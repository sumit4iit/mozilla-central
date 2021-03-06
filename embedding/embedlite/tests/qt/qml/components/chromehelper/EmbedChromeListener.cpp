/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedChromeListener.h"

#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "mozilla/embedlite/EmbedLog.h"

#include "nsStringGlue.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"
#include "nsPIDOMWindow.h"

#pragma GCC visibility push(default)
#include <json/json.h>
#pragma GCC visibility pop

EmbedChromeListener::EmbedChromeListener()
  : mWindowCounter(0)
{
    LOGT();
}

EmbedChromeListener::~EmbedChromeListener()
{
    LOGT();
}

NS_IMPL_ISUPPORTS3(EmbedChromeListener, nsIObserver, nsIDOMEventListener, nsSupportsWeakReference)

nsresult
EmbedChromeListener::Init()
{
    nsresult rv;

    nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

    if (observerService) {
        rv = observerService->AddObserver(this,
                                          "domwindowopened",
                                          true);
        rv = observerService->AddObserver(this,
                                          "domwindowclosed",
                                          true);
        rv = observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                          false);
    }

    return NS_OK;
}

NS_IMETHODIMP
EmbedChromeListener::Observe(nsISupports *aSubject,
                             const char *aTopic,
                             const PRUnichar *aData)
{
    nsresult rv;
    if (!strcmp(aTopic, "domwindowopened")) {
        nsCOMPtr<nsIDOMWindow> win = do_QueryInterface(aSubject, &rv);
        NS_ENSURE_SUCCESS(rv, NS_OK);
        WindowCreated(win);
    } else if (!strcmp(aTopic, "domwindclosed")) {
        nsCOMPtr<nsIDOMWindow> win = do_QueryInterface(aSubject, &rv);
        NS_ENSURE_SUCCESS(rv, NS_OK);
        WindowDestroyed(win);
    } else {
        LOGT("obj:%p, top:%s", aSubject, aTopic);
    }

    return NS_OK;
}

void
EmbedChromeListener::WindowCreated(nsIDOMWindow* aWin)
{
    LOGT("WindowOpened: %p", aWin);
    nsCOMPtr<nsPIDOMWindow> pidomWindow = do_GetInterface(aWin);
    NS_ENSURE_TRUE(pidomWindow, );
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(pidomWindow->GetChromeEventHandler());
    NS_ENSURE_TRUE(target, );
    target->AddEventListener(NS_LITERAL_STRING("DOMTitleChanged"), this,  PR_FALSE);
    mWindowCounter++;
    if (!mService) {
        mService = do_GetService("@mozilla.org/embedlite-app-service;1");
    }
}

void
EmbedChromeListener::WindowDestroyed(nsIDOMWindow* aWin)
{
    LOGT("WindowClosed: %p", aWin);
    nsCOMPtr<nsPIDOMWindow> pidomWindow = do_GetInterface(aWin);
    NS_ENSURE_TRUE(pidomWindow, );
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(pidomWindow->GetChromeEventHandler());
    NS_ENSURE_TRUE(target, );
    target->RemoveEventListener(NS_LITERAL_STRING("DOMTitleChanged"), this,  PR_FALSE);
    mWindowCounter--;
    if (!mWindowCounter) {
        mService = nullptr;
    }
}

nsresult
GetDOMWindowByNode(nsIDOMNode *aNode, nsIDOMWindow **aDOMWindow)
{
    nsresult rv;
    nsCOMPtr<nsIDOMDocument> ctDoc = do_QueryInterface(aNode, &rv);
    NS_ENSURE_SUCCESS(rv , rv);
    nsCOMPtr<nsIDOMWindow> targetWin;
    rv = ctDoc->GetDefaultView(getter_AddRefs(targetWin));
    NS_ENSURE_SUCCESS(rv , rv);
    NS_ADDREF(*aDOMWindow = targetWin);
    return rv;
}

NS_IMETHODIMP
EmbedChromeListener::HandleEvent(nsIDOMEvent* aEvent)
{
    nsresult rv;
    nsString type;
    if (aEvent) {
        aEvent->GetType(type);
    }
    LOGT("Event:'%s'", NS_ConvertUTF16toUTF8(type).get());
    if (type.EqualsLiteral("DOMTitleChanged")) {
        nsCOMPtr<nsIDOMEventTarget> eventTarget;
        rv = aEvent->GetTarget(getter_AddRefs(eventTarget));
        nsCOMPtr<nsIDOMNode> eventNode = do_QueryInterface(eventTarget, &rv);
        nsCOMPtr<nsIDOMWindow> window;
        rv = GetDOMWindowByNode(eventNode, getter_AddRefs(window));

        nsCOMPtr<nsIDOMDocument> ctDoc;
        window->GetDocument(getter_AddRefs(ctDoc));
        nsString title;
        ctDoc->GetTitle(title);
        uint32_t winid;
        mService->GetIDByWindow(window, &winid);
        NS_ENSURE_TRUE(winid , NS_ERROR_FAILURE);
        json_object* my_object = json_object_new_object();
        LOGT("title:'%s'", NS_ConvertUTF16toUTF8(title).get());
        json_object_object_add(my_object, "title", json_object_new_string(NS_ConvertUTF16toUTF8(title).get()));
        mService->SendAsyncMessage(winid, NS_LITERAL_STRING("chrome:title"), NS_ConvertUTF8toUTF16(json_object_to_json_string(my_object)));
        free(my_object);
    }

    return NS_OK;
}
