/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "nsWidgetSupport.h"
#include "nsRect.h"
#include "nsITextAreaWidget.h"
#include "nsIFileWidget.h"
#include "nsIAppShell.h"
#include "nsIButton.h"
#include "nsIEventListener.h"
#include "nsILabel.h"
#include "nsIListBox.h"
#include "nsIListWidget.h"
#include "nsILookAndFeel.h"
#include "nsIMouseListener.h"
#include "nsIToolkit.h"
#include "nsIWidget.h"
#include "nsICheckButton.h"
#include "nsIScrollbar.h"
#include "nsIRadioButton.h"
#include "nsITextWidget.h"




NS_WIDGET nsresult 
NS_CreateButton(nsISupports* aParent, 
								nsIButton* aButton, 
								const nsRect& aRect, 
								EVENT_CALLBACK aHandleEventFunction,
								const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(NS_GET_IID(nsIWidget),(void**)&parent);

  nsIWidget* 	widget;
	if (NS_OK == aButton->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
		NS_IF_RELEASE(widget);
	}
  
  if (aParent != nsnull)
    NS_IF_RELEASE(parent);
  return NS_OK;
}

NS_WIDGET nsresult 
NS_CreateCheckButton(nsISupports* aParent, 
											  nsICheckButton* aCheckButton, 
											  const nsRect& aRect, 
											  EVENT_CALLBACK aHandleEventFunction,
											  const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(NS_GET_IID(nsIWidget),(void**)&parent);

 	nsIWidget* 	widget;
	if (NS_OK == aCheckButton->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
		NS_IF_RELEASE(widget);
	}
  if (aParent != nsnull)
    NS_IF_RELEASE(parent);
 return NS_OK;
}




NS_WIDGET nsresult 
NS_CreateRadioButton( nsISupports* aParent, 
												nsIRadioButton* aRadioButton, 
												const nsRect& aRect, 
												EVENT_CALLBACK aHandleEventFunction,
												const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(NS_GET_IID(nsIWidget),(void**)&parent);

 	nsIWidget* 	widget;
	if (NS_OK == aRadioButton->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
		NS_IF_RELEASE(widget);
	}
  if (aParent != nsnull)
    NS_IF_RELEASE(parent);
 return NS_OK;
}


NS_WIDGET nsresult 
NS_CreateLabel( nsISupports* aParent, 
									nsILabel* aLabel, 
									const nsRect& aRect, 
									EVENT_CALLBACK aHandleEventFunction,
									const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (NS_OK == aParent->QueryInterface(NS_GET_IID(nsIWidget),(void**)&parent))
	{
		nsIWidget* 	widget;
		if (NS_OK == aLabel->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget)) {
	  	widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  	widget->Show(PR_TRUE);
      if (aFont != nsnull)
	  	  widget->SetFont(*aFont);
			NS_IF_RELEASE(widget);
		}
		NS_IF_RELEASE(parent);
	}
  return NS_OK;
}



NS_WIDGET nsresult 
NS_CreateTextAreaWidget(nsISupports* aParent, 
									nsITextAreaWidget* aWidget, 
									const nsRect& aRect, 
									EVENT_CALLBACK aHandleEventFunction,
									const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(NS_GET_IID(nsIWidget),(void**)&parent);

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
    NS_IF_RELEASE(widget);
	}
  else
  {
    NS_ERROR("Called QueryInterface on a non nsIWidget supported object");
  }

	if (aParent)
	  NS_IF_RELEASE(parent);

  return NS_OK;
}

NS_WIDGET nsresult 
NS_CreateTextWidget(nsISupports* aParent, 
									nsITextWidget* aWidget, 
									const nsRect& aRect, 
									EVENT_CALLBACK aHandleEventFunction,
									const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(NS_GET_IID(nsIWidget),(void**)&parent);

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
    NS_IF_RELEASE(widget);
	}
  else
  {
    NS_ERROR("Called QueryInterface on a non nsIWidget supported object");
  }

	if (aParent)
	  NS_IF_RELEASE(parent);

  return NS_OK;
}



NS_WIDGET nsresult 
NS_CreateScrollBar(nsISupports* aParent, 
									nsIScrollbar* aWidget, 
									const nsRect& aRect, 
									EVENT_CALLBACK aHandleEventFunction)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(NS_GET_IID(nsIWidget),(void**)&parent);

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    NS_IF_RELEASE(widget);
	}
  else
  {
    NS_ERROR("Called QueryInterface on a non nsIWidget supported object");
  }

	if (aParent)
	  NS_IF_RELEASE(parent);

  return NS_OK;
}

NS_WIDGET nsresult 
NS_CreateListBox(nsISupports* aParent, 
									nsIListBox* aWidget, 
									const nsRect& aRect, 
                  EVENT_CALLBACK aHandleEventFunction,
                  const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(NS_GET_IID(nsIWidget),(void**)&parent);

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
    NS_IF_RELEASE(widget);
	}
  else
  {
    NS_ERROR("Called QueryInterface on a non nsIWidget supported object");
  }

	if (aParent)
	  NS_IF_RELEASE(parent);

  return NS_OK;
}

extern NS_WIDGET nsresult 
NS_ShowWidget(nsISupports* aWidget, PRBool aShow)
{

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget)) {
	  widget->Show(aShow);
	  NS_IF_RELEASE(widget);
	}

  return NS_OK;
}

extern NS_WIDGET nsresult 
NS_MoveWidget(nsISupports* aWidget, PRUint32 aX, PRUint32 aY)
{

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget)) {
	  widget->Move(aX,aY);
	  NS_IF_RELEASE(widget);
	}
  return NS_OK;
}

extern NS_WIDGET nsresult 
NS_EnableWidget(nsISupports* aWidget, PRBool aEnable)
{
	void* 			result = nsnull;
	nsIWidget* 	widget;
	if (NS_OK == aWidget->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget))
	{
		widget->Enable(aEnable);
		NS_RELEASE(widget);
	}
  return NS_OK;
}


extern NS_WIDGET nsresult 
NS_SetFocusToWidget(nsISupports* aWidget)
{

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget)) {
	  widget->SetFocus();
	  NS_IF_RELEASE(widget);
	}
  return NS_OK;
}


extern NS_WIDGET nsresult 
NS_GetWidgetNativeData(nsISupports* aWidget, void** aNativeData)
{
	void* 			result = nsnull;
	nsIWidget* 	widget;
	if (NS_OK == aWidget->QueryInterface(NS_GET_IID(nsIWidget),(void**)&widget))
	{
		result = widget->GetNativeData(NS_NATIVE_WIDGET);
		NS_RELEASE(widget);
	}
	*aNativeData = result;
  return NS_OK;

}
