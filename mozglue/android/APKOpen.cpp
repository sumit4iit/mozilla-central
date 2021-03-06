/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This custom library loading code is only meant to be called
 * during initialization. As a result, it takes no special 
 * precautions to be threadsafe. Any of the library loading functions
 * like mozload should not be available to other code.
 */

#include <jni.h>
#include <android/log.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/limits.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include "dlfcn.h"
#include "APKOpen.h"
#include <sys/time.h>
#include <sys/resource.h>
#include "Zip.h"
#include "sqlite3.h"
#include "SQLiteBridge.h"
#include "NSSBridge.h"
#include "ElfLoader.h"
#include "application.ini.h"

/* Android headers don't define RUSAGE_THREAD */
#ifndef RUSAGE_THREAD
#define RUSAGE_THREAD 1
#endif

extern "C" {
/*
 * To work around http://code.google.com/p/android/issues/detail?id=23203
 * we don't link with the crt objects. In some configurations, this means
 * a lack of the __dso_handle symbol because it is defined there, and
 * depending on the android platform and ndk versions used, it may or may
 * not be defined in libc.so. In the latter case, we fail to link. Defining
 * it here as weak makes us provide the symbol when it's not provided by
 * the crt objects, making the change transparent for future NDKs that
 * would fix the original problem. On older NDKs, it is not a problem
 * either because the way __dso_handle was used was already broken (and
 * the custom linker works around it).
 */
  NS_EXPORT __attribute__((weak)) void *__dso_handle;
}

typedef int mozglueresult;
typedef int64_t MOZTime;

enum StartupEvent {
#define mozilla_StartupTimeline_Event(ev, z) ev,
#include "StartupTimeline.h"
#undef mozilla_StartupTimeline_Event
  MAX_STARTUP_EVENT_ID
};

using namespace mozilla;

static MOZTime MOZ_Now()
{
  struct timeval tm;
  gettimeofday(&tm, 0);
  return (((MOZTime)tm.tv_sec * 1000000LL) + (MOZTime)tm.tv_usec);
}

static struct mapping_info * lib_mapping = NULL;

NS_EXPORT const struct mapping_info *
getLibraryMapping()
{
  return lib_mapping;
}

void
JNI_Throw(JNIEnv* jenv, const char* classname, const char* msg)
{
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Throw\n");
    jclass cls = jenv->FindClass(classname);
    if (cls == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't find exception class (or exception pending) %s\n", classname);
        exit(FAILURE);
    }
    int rc = jenv->ThrowNew(cls, msg);
    if (rc < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Error throwing exception %s\n", msg);
        exit(FAILURE);
    }
    jenv->DeleteLocalRef(cls);
}

#define SHELL_WRAPPER0(name) \
typedef void (*name ## _t)(JNIEnv *, jclass); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc) \
{ \
  f_ ## name(jenv, jc); \
}

#define SHELL_WRAPPER0_WITH_RETURN(name, return_type) \
typedef return_type (*name ## _t)(JNIEnv *, jclass); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT return_type JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc) \
{ \
  return f_ ## name(jenv, jc); \
}

#define SHELL_WRAPPER1(name,type1) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one) \
{ \
  f_ ## name(jenv, jc, one); \
}

#define SHELL_WRAPPER1_WITH_RETURN(name, return_type, type1) \
typedef return_type (*name ## _t)(JNIEnv *, jclass, type1 one); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT return_type JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one) \
{ \
  return f_ ## name(jenv, jc, one); \
}

#define SHELL_WRAPPER2(name,type1,type2) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two) \
{ \
  f_ ## name(jenv, jc, one, two); \
}

#define SHELL_WRAPPER2_WITH_RETURN(name, return_type, type1, type2) \
typedef return_type (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT return_type JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two) \
{ \
  return f_ ## name(jenv, jc, one, two); \
}

#define SHELL_WRAPPER3(name,type1,type2,type3) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three) \
{ \
  f_ ## name(jenv, jc, one, two, three); \
}

#define SHELL_WRAPPER3_WITH_RETURN(name, return_type, type1, type2, type3) \
typedef return_type (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT return_type JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three) \
{ \
  return f_ ## name(jenv, jc, one, two, three); \
}

#define SHELL_WRAPPER4(name,type1,type2,type3,type4) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four) \
{ \
  f_ ## name(jenv, jc, one, two, three, four); \
}

#define SHELL_WRAPPER4_WITH_RETURN(name, return_type, type1, type2, type3, type4) \
typedef return_type (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT return_type JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four) \
{ \
  return f_ ## name(jenv, jc, one, two, three, four); \
}

#define SHELL_WRAPPER5(name,type1,type2,type3,type4,type5) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four, type5 five); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four, type5 five) \
{ \
  f_ ## name(jenv, jc, one, two, three, four, five); \
}

#define SHELL_WRAPPER5_WITH_RETURN(name, return_type, type1, type2, type3, type4, type5) \
typedef return_type (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four, type5 five); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT return_type JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four, type5 five) \
{ \
  return f_ ## name(jenv, jc, one, two, three, four, five); \
}

#define SHELL_WRAPPER6(name,type1,type2,type3,type4,type5,type6) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six) \
{ \
  f_ ## name(jenv, jc, one, two, three, four, five, six); \
}

#define SHELL_WRAPPER6_WITH_RETURN(name, return_type, type1, type2, type3, type4, type5, type6) \
typedef return_type (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT return_type JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six) \
{ \
  return f_ ## name(jenv, jc, one, two, three, four, five, six); \
}

#define SHELL_WRAPPER7(name,type1,type2,type3,type4,type5,type6,type7) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven) \
{ \
  f_ ## name(jenv, jc, one, two, three, four, five, six, seven); \
}

#define SHELL_WRAPPER7_WITH_RETURN(name, return_type, type1, type2, type3, type4, type5, type6, type7) \
typedef return_type (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT return_type JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven) \
{ \
  return f_ ## name(jenv, jc, one, two, three, four, five, six, seven); \
}

#define SHELL_WRAPPER8(name,type1,type2,type3,type4,type5,type6,type7,type8) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven, type8 eight); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven, type8 eight) \
{ \
  f_ ## name(jenv, jc, one, two, three, four, five, six, seven, eight); \
}

#define SHELL_WRAPPER8_WITH_RETURN(name, return_type, type1, type2, type3, type4, type5, type6, type7, type8) \
typedef return_type (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven, type8 eight); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT return_type JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven, type8 eight) \
{ \
  return f_ ## name(jenv, jc, one, two, three, four, five, six, seven, eight); \
}

#define SHELL_WRAPPER9_WITH_RETURN(name, return_type, type1, type2, type3, type4, type5, type6, type7, type8, type9) \
typedef return_type (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven, type8 eight, type9 nine); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT return_type JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven, type8 eight, type9 nine) \
{ \
  return f_ ## name(jenv, jc, one, two, three, four, five, six, seven, eight, nine); \
}

#define SHELL_WRAPPER9(name,type1,type2,type3,type4,type5,type6,type7,type8, type9) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven, type8 eight, type9 nine); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three, type4 four, type5 five, type6 six, type7 seven, type8 eight, type9 nine) \
{ \
  f_ ## name(jenv, jc, one, two, three, four, five, six, seven, eight, nine); \
}

SHELL_WRAPPER0(nativeInit)
SHELL_WRAPPER1(notifyGeckoOfEvent, jobject)
SHELL_WRAPPER0(processNextNativeEvent)
SHELL_WRAPPER1(setSurfaceView, jobject)
SHELL_WRAPPER2(setLayerClient, jobject, jint)
SHELL_WRAPPER0(onResume)
SHELL_WRAPPER0(onLowMemory)
SHELL_WRAPPER3(callObserver, jstring, jstring, jstring)
SHELL_WRAPPER1(removeObserver, jstring)
SHELL_WRAPPER1(onChangeNetworkLinkStatus, jstring)
SHELL_WRAPPER1(reportJavaCrash, jstring)
SHELL_WRAPPER1(cameraCallbackBridge, jbyteArray)
SHELL_WRAPPER3(notifyBatteryChange, jdouble, jboolean, jdouble)
SHELL_WRAPPER3(notifySmsReceived, jstring, jstring, jlong)
SHELL_WRAPPER0(bindWidgetTexture)
SHELL_WRAPPER0(scheduleComposite)
SHELL_WRAPPER0(schedulePauseComposition)
SHELL_WRAPPER2(scheduleResumeComposition, jint, jint)
SHELL_WRAPPER0_WITH_RETURN(computeRenderIntegrity, jfloat)
SHELL_WRAPPER3_WITH_RETURN(saveMessageInSentbox, jint, jstring, jstring, jlong)
SHELL_WRAPPER5(notifySmsSent, jint, jstring, jstring, jlong, jint)
SHELL_WRAPPER5(notifySmsDelivery, jint, jint, jstring, jstring, jlong)
SHELL_WRAPPER2(notifySmsSendFailed, jint, jint)
SHELL_WRAPPER7(notifyGetSms, jint, jint, jstring, jstring, jstring, jlong, jint)
SHELL_WRAPPER2(notifyGetSmsFailed, jint, jint)
SHELL_WRAPPER2(notifySmsDeleted, jboolean, jint)
SHELL_WRAPPER2(notifySmsDeleteFailed, jint, jint)
SHELL_WRAPPER1(notifyNoMessageInList, jint)
SHELL_WRAPPER8(notifyListCreated, jint, jint, jint, jstring, jstring, jstring, jlong, jint)
SHELL_WRAPPER7(notifyGotNextMessage, jint, jint, jstring, jstring, jstring, jlong, jint)
SHELL_WRAPPER2(notifyReadingMessageListFailed, jint, jint)
SHELL_WRAPPER2(notifyFilePickerResult, jstring, jlong)
SHELL_WRAPPER1_WITH_RETURN(getSurfaceBits, jobject, jobject)
SHELL_WRAPPER1(onFullScreenPluginHidden, jobject)
SHELL_WRAPPER1_WITH_RETURN(getNextMessageFromQueue, jobject, jobject)
SHELL_WRAPPER2(onSurfaceTextureFrameAvailable, jobject, jint)

static void * xul_handle = NULL;
static void * sqlite_handle = NULL;
static void * nss_handle = NULL;
static void * nspr_handle = NULL;
static void * plc_handle = NULL;

template <typename T> inline void
xul_dlsym(const char *symbolName, T *value)
{
  *value = (T) (uintptr_t) __wrap_dlsym(xul_handle, symbolName);
}

#if defined(MOZ_CRASHREPORTER)
static void
extractLib(Zip::Stream &s, void * dest)
{
  z_stream strm = {
    next_in: (Bytef *)s.GetBuffer(),
    avail_in: s.GetSize(),
    total_in: 0,

    next_out: (Bytef *)dest,
    avail_out: s.GetUncompressedSize(),
    total_out: 0
  };

  int ret;
  ret = inflateInit2(&strm, -MAX_WBITS);
  if (ret != Z_OK)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "inflateInit failed: %s", strm.msg);

  ret = inflate(&strm, Z_SYNC_FLUSH);
  if (ret != Z_STREAM_END)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "inflate failed: %s", strm.msg);

  ret = inflateEnd(&strm);
  if (ret != Z_OK)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "inflateEnd failed: %s", strm.msg);

  if (strm.total_out != s.GetUncompressedSize())
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "File not fully uncompressed! %lu / %d", strm.total_out, s.GetUncompressedSize());
}
#endif

static struct lib_cache_info *cache_mapping = NULL;

NS_EXPORT const struct lib_cache_info *
getLibraryCache()
{
  return cache_mapping;
}

#ifdef MOZ_CRASHREPORTER
static void *
extractBuf(const char * path, Zip *zip)
{
  Zip::Stream s;
  if (!zip->GetStream(path, &s))
    return NULL;

  // allocate space for a trailing null byte
  void * buf = malloc(s.GetUncompressedSize() + 1);
  if (buf == (void *)-1) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't alloc decompression buffer for %s", path);
    return NULL;
  }
  if (s.GetType() == Zip::Stream::DEFLATE)
    extractLib(s, buf);
  else
    memcpy(buf, s.GetBuffer(), s.GetUncompressedSize());

  // null terminate it
  ((unsigned char*) buf)[s.GetUncompressedSize()] = 0;

  return buf;
}
#endif

static int mapping_count = 0;
static char *file_ids = NULL;

#define MAX_MAPPING_INFO 32

extern "C" void
report_mapping(char *name, void *base, uint32_t len, uint32_t offset)
{
  if (!file_ids || mapping_count >= MAX_MAPPING_INFO)
    return;

  struct mapping_info *info = &lib_mapping[mapping_count++];
  info->name = strdup(name);
  info->base = (uintptr_t)base;
  info->len = len;
  info->offset = offset;

  char * entry = strstr(file_ids, name);
  if (entry)
    info->file_id = strndup(entry + strlen(name) + 1, 32);
}

static mozglueresult
loadGeckoLibs(const char *apkName)
{
  chdir(getenv("GRE_HOME"));

  MOZTime t0 = MOZ_Now();
  struct rusage usage1;
  getrusage(RUSAGE_THREAD, &usage1);
  
  RefPtr<Zip> zip = new Zip(apkName);

#ifdef MOZ_CRASHREPORTER
  file_ids = (char *)extractBuf("lib.id", zip);
#endif

  char *file = new char[strlen(apkName) + sizeof("!/libxpcom.so")];
  sprintf(file, "%s!/libxpcom.so", apkName);
  __wrap_dlopen(file, RTLD_GLOBAL | RTLD_LAZY);
  // libxul.so is pulled from libxpcom.so, so we don't need to give the full path
  xul_handle = __wrap_dlopen("libxul.so", RTLD_GLOBAL | RTLD_LAZY);
  delete[] file;

#ifdef MOZ_CRASHREPORTER
  free(file_ids);
  file_ids = NULL;
#endif

  if (!xul_handle) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libxul!");
    return FAILURE;
  }

#define GETFUNC(name) xul_dlsym("Java_org_mozilla_gecko_GeckoAppShell_" #name, &f_ ## name)
  GETFUNC(nativeInit);
  GETFUNC(notifyGeckoOfEvent);
  GETFUNC(processNextNativeEvent);
  GETFUNC(setSurfaceView);
  GETFUNC(setLayerClient);
  GETFUNC(onResume);
  GETFUNC(onLowMemory);
  GETFUNC(callObserver);
  GETFUNC(removeObserver);
  GETFUNC(onChangeNetworkLinkStatus);
  GETFUNC(reportJavaCrash);
  GETFUNC(cameraCallbackBridge);
  GETFUNC(notifyBatteryChange);
  GETFUNC(notifySmsReceived);
  GETFUNC(bindWidgetTexture);
  GETFUNC(scheduleComposite);
  GETFUNC(schedulePauseComposition);
  GETFUNC(scheduleResumeComposition);
  GETFUNC(computeRenderIntegrity);
  GETFUNC(saveMessageInSentbox);
  GETFUNC(notifySmsSent);
  GETFUNC(notifySmsDelivery);
  GETFUNC(notifySmsSendFailed);
  GETFUNC(notifyGetSms);
  GETFUNC(notifyGetSmsFailed);
  GETFUNC(notifySmsDeleted);
  GETFUNC(notifySmsDeleteFailed);
  GETFUNC(notifyNoMessageInList);
  GETFUNC(notifyListCreated);
  GETFUNC(notifyGotNextMessage);
  GETFUNC(notifyReadingMessageListFailed);
  GETFUNC(notifyFilePickerResult);
  GETFUNC(getSurfaceBits);
  GETFUNC(onFullScreenPluginHidden);
  GETFUNC(getNextMessageFromQueue);
  GETFUNC(onSurfaceTextureFrameAvailable);
#undef GETFUNC

  void (*XRE_StartupTimelineRecord)(int, MOZTime);
  xul_dlsym("XRE_StartupTimelineRecord", &XRE_StartupTimelineRecord);

  MOZTime t1 = MOZ_Now();
  struct rusage usage2;
  getrusage(RUSAGE_THREAD, &usage2);

  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Loaded libs in %lldms total, %ldms user, %ldms system, %ld faults",
                      (t1 - t0) / 1000,
                      (usage2.ru_utime.tv_sec - usage1.ru_utime.tv_sec)*1000 + (usage2.ru_utime.tv_usec - usage1.ru_utime.tv_usec)/1000,
                      (usage2.ru_stime.tv_sec - usage1.ru_stime.tv_sec)*1000 + (usage2.ru_stime.tv_usec - usage1.ru_stime.tv_usec)/1000,
                      usage2.ru_majflt-usage1.ru_majflt);

  XRE_StartupTimelineRecord(LINKER_INITIALIZED, t0);
  XRE_StartupTimelineRecord(LIBRARIES_LOADED, t1);
  return SUCCESS;
}

static int loadSQLiteLibs(const char *apkName)
{
  chdir(getenv("GRE_HOME"));

  RefPtr<Zip> zip = new Zip(apkName);
  if (!lib_mapping) {
    lib_mapping = (struct mapping_info *)calloc(MAX_MAPPING_INFO, sizeof(*lib_mapping));
  }

#ifdef MOZ_CRASHREPORTER
  file_ids = (char *)extractBuf("lib.id", zip);
#endif

  char *file = new char[strlen(apkName) + sizeof("!/libmozsqlite3.so")];
  sprintf(file, "%s!/libmozsqlite3.so", apkName);
  sqlite_handle = __wrap_dlopen(file, RTLD_GLOBAL | RTLD_LAZY);
  delete [] file;

#ifdef MOZ_CRASHREPORTER
  free(file_ids);
  file_ids = NULL;
#endif

  if (!sqlite_handle) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libmozsqlite3!");
    return FAILURE;
  }

  setup_sqlite_functions(sqlite_handle);
  return SUCCESS;
}

static mozglueresult
loadNSSLibs(const char *apkName)
{
  chdir(getenv("GRE_HOME"));

  RefPtr<Zip> zip = new Zip(apkName);
  if (!lib_mapping) {
    lib_mapping = (struct mapping_info *)calloc(MAX_MAPPING_INFO, sizeof(*lib_mapping));
  }

#ifdef MOZ_CRASHREPORTER
  file_ids = (char *)extractBuf("lib.id", zip);
#endif

  char *file = new char[strlen(apkName) + sizeof("!/libnss3.so")];
  sprintf(file, "%s!/libnss3.so", apkName);
  nss_handle = __wrap_dlopen(file, RTLD_GLOBAL | RTLD_LAZY);
  delete [] file;

  file = new char[strlen(apkName) + sizeof("!/libnspr4.so")];
  sprintf(file, "%s!/libnspr4.so", apkName);
  nspr_handle = __wrap_dlopen(file, RTLD_GLOBAL | RTLD_LAZY);
  delete [] file;

  file = new char[strlen(apkName) + sizeof("!/libplc4.so")];
  sprintf(file, "%s!/libplc4.so", apkName);
  plc_handle = __wrap_dlopen(file, RTLD_GLOBAL | RTLD_LAZY);
  delete [] file;

#ifdef MOZ_CRASHREPORTER
  free(file_ids);
  file_ids = NULL;
#endif

  if (!nss_handle) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libnss3!");
    return FAILURE;
  }

  if (!nspr_handle) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libnspr4!");
    return FAILURE;
  }

  if (!plc_handle) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libplc4!");
    return FAILURE;
  }

  return setup_nss_functions(nss_handle, nspr_handle, plc_handle);
}

extern "C" NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_loadGeckoLibsNative(JNIEnv *jenv, jclass jGeckoAppShellClass, jstring jApkName)
{
  const char* str;
  // XXX: java doesn't give us true UTF8, we should figure out something
  // better to do here
  str = jenv->GetStringUTFChars(jApkName, NULL);
  if (str == NULL)
    return;

  int res = loadGeckoLibs(str);
  if (res != SUCCESS) {
    JNI_Throw(jenv, "java/lang/Exception", "Error loading gecko libraries");
  }
  jenv->ReleaseStringUTFChars(jApkName, str);
}

extern "C" NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_loadSQLiteLibsNative(JNIEnv *jenv, jclass jGeckoAppShellClass, jstring jApkName, jboolean jShouldExtract) {
  if (jShouldExtract) {
    putenv("MOZ_LINKER_EXTRACT=1");
  }

  const char* str;
  // XXX: java doesn't give us true UTF8, we should figure out something
  // better to do here
  str = jenv->GetStringUTFChars(jApkName, NULL);
  if (str == NULL)
    return;

  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Load sqlite start\n");
  mozglueresult rv = loadSQLiteLibs(str);
  if (rv != SUCCESS) {
      JNI_Throw(jenv, "java/lang/Exception", "Error loading sqlite libraries");
  }
  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Load sqlite done\n");
  jenv->ReleaseStringUTFChars(jApkName, str);
}

extern "C" NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_loadNSSLibsNative(JNIEnv *jenv, jclass jGeckoAppShellClass, jstring jApkName, jboolean jShouldExtract) {
  if (jShouldExtract) {
    putenv("MOZ_LINKER_EXTRACT=1");
  }

  const char* str;
  // XXX: java doesn't give us true UTF8, we should figure out something
  // better to do here
  str = jenv->GetStringUTFChars(jApkName, NULL);
  if (str == NULL)
    return;

  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Load nss start\n");
  mozglueresult rv = loadNSSLibs(str);
  if (rv != SUCCESS) {
    JNI_Throw(jenv, "java/lang/Exception", "Error loading nss libraries");
  }
  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Load nss done\n");
  jenv->ReleaseStringUTFChars(jApkName, str);
}

typedef void (*GeckoStart_t)(void *, const nsXREAppData *);

extern "C" NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_nativeRun(JNIEnv *jenv, jclass jc, jstring jargs)
{
  GeckoStart_t GeckoStart;
  xul_dlsym("GeckoStart", &GeckoStart);
  if (GeckoStart == NULL)
    return;
  // XXX: java doesn't give us true UTF8, we should figure out something
  // better to do here
  int len = jenv->GetStringUTFLength(jargs);
  // GeckoStart needs to write in the args buffer, so we need a copy.
  char *args = (char *) malloc(len + 1);
  jenv->GetStringUTFRegion(jargs, 0, len, args);
  args[len] = '\0';
  GeckoStart(args, &sAppData);
  free(args);
}

typedef int GeckoProcessType;

extern "C" NS_EXPORT mozglueresult
ChildProcessInit(int argc, char* argv[])
{
  int i;
  for (i = 0; i < (argc - 1); i++) {
    if (strcmp(argv[i], "-greomni"))
      continue;

    i = i + 1;
    break;
  }

  if (loadNSSLibs(argv[i]) != SUCCESS) {
    return FAILURE;
  }
  if (loadSQLiteLibs(argv[i]) != SUCCESS) {
    return FAILURE;
  }
  if (loadGeckoLibs(argv[i]) != SUCCESS) {
    return FAILURE;
  }

  // don't pass the last arg - it's only recognized by the lib cache
  argc--;

  GeckoProcessType (*fXRE_StringToChildProcessType)(char*);
  xul_dlsym("XRE_StringToChildProcessType", &fXRE_StringToChildProcessType);

  mozglueresult (*fXRE_InitChildProcess)(int, char**, GeckoProcessType);
  xul_dlsym("XRE_InitChildProcess", &fXRE_InitChildProcess);

  GeckoProcessType proctype = fXRE_StringToChildProcessType(argv[--argc]);

  return fXRE_InitChildProcess(argc, argv, proctype);
}

