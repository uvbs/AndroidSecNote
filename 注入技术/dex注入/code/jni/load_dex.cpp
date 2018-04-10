#include "jni.h"
#include <android/log.h>
#include <string>
#include <android_runtime/AndroidRuntime.h>
#include <pthread.h>


#define FLOG_TAG "TTT"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, FLOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, FLOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, FLOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, FLOG_TAG, __VA_ARGS__)
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, FLOG_TAG, __VA_ARGS__)

static JavaVM *jvm;
static bool g_bAttatedT;
static const char JSTRING[] = "Ljava/lang/String;";
static const char JCLASS_LOADER[] = "Ljava/lang/ClassLoader;";
static const char JCLASS[] = "Ljava/lang/Class;";
static char sig_buffer[512];



void init()
{
    g_bAttatedT = false;
    jvm = android::AndroidRuntime::getJavaVM();
}

JNIEnv *getEnv()
{
    int status;
    JNIEnv *jni_env;
    status = jvm->GetEnv((void **)&jni_env, JNI_VERSION_1_4);
    if(status < 0)
    {
        status = jvm->AttachCurrentThread(&jni_env, NULL);
        if(status < 0)
        {
            return NULL;
        }
        g_bAttatedT = true;
    }
    return jni_env;
}

void detachCurrent()
{
    if(g_bAttatedT)
    {
        jvm->DetachCurrentThread();
    }
}

std::string jstring2str(JNIEnv* env, jstring jstr)
{
    char* rtn = NULL;
    jclass clsstring = env->FindClass("java/lang/String");
    jstring strencode = env->NewStringUTF("GB2312");
    jmethodID mid = env->GetMethodID(clsstring,   "getBytes",   "(Ljava/lang/String;)[B");
    jbyteArray barr = (jbyteArray)env->CallObjectMethod(jstr,mid,strencode);
    jsize alen = env->GetArrayLength(barr);
    jbyte* ba = env->GetByteArrayElements(barr,JNI_FALSE);
    if(alen > 0)
    {
        rtn = (char*)malloc(alen+1);
        memcpy(rtn,ba,alen);
        rtn[alen]=0;
    }
    env->ReleaseByteArrayElements(barr,ba,0);
    std::string stemp(rtn);
    free(rtn);
    return stemp;
}

int handleException(JNIEnv *jni_env);
std::string toString(JNIEnv *jni_env, jclass claxx, jobject obj)
{
    snprintf(sig_buffer, 512, "()%s", JSTRING);
    jmethodID toString_method = jni_env->GetMethodID(claxx, "toString", sig_buffer);
    jstring javaString = (jstring)(jni_env->CallObjectMethod(obj, toString_method, NULL));
    jthrowable exception = jni_env->ExceptionOccurred();
    handleException(jni_env);
    std::string result = jstring2str(jni_env, javaString);
    return result;
}

int handleException(JNIEnv *jni_env)
{
    jthrowable exception = jni_env->ExceptionOccurred();
    if (exception) {
        jni_env->ExceptionClear();
        LOGI("exception occured");
        jclass clazz = jni_env->GetObjectClass(exception);
        jmethodID getMessage = jni_env->GetMethodID(clazz,
                                                    "getMessage",
                                                    "()Ljava/lang/String;");
        jstring message = (jstring)jni_env->CallObjectMethod(exception, getMessage);
        LOGI("error message is: %s", jstring2str(jni_env, message).c_str());
        jni_env->ExceptionClear();
        return 1;
    }
    return 0;
}

jobject getSystemClassLoader(JNIEnv *jni_env){
    jclass class_loader_claxx = jni_env->FindClass("java/lang/ClassLoader");
    memset(sig_buffer, 0, 512);
    snprintf(sig_buffer, 512, "()%s", JCLASS_LOADER);
    jmethodID getSystemClassLoader_method = jni_env->GetStaticMethodID(class_loader_claxx, "getSystemClassLoader", sig_buffer);
    jobject result = jni_env->CallStaticObjectMethod(class_loader_claxx, getSystemClassLoader_method);
    handleException(jni_env);
    return result;
}

void *load_dex(void *)
{
    init();
    JNIEnv *jni_env = getEnv();
    jstring dex_path = jni_env->NewStringUTF("/data/data/com.glider.todo/lib/inject.dex");
    jstring dex_out_path = jni_env->NewStringUTF("/data/data/com.glider.todo/cache");
    //find dexloader_claxx
    jclass dexloader_claxx = jni_env->FindClass("dalvik/system/DexClassLoader");
    handleException(jni_env);
    memset(sig_buffer, 0, 512);
    snprintf(sig_buffer, 512, "(%s%s%s%s)V", JSTRING, JSTRING, JSTRING, JCLASS_LOADER);
    jmethodID dexloader_init_method = jni_env->GetMethodID(dexloader_claxx, "<init>", sig_buffer);
    handleException(jni_env);
    memset(sig_buffer, 0, 512);
    snprintf(sig_buffer, 512, "(%s)%s", JSTRING, JCLASS);
    jmethodID loadClass_method = jni_env->GetMethodID(dexloader_claxx, "loadClass", sig_buffer);
    handleException(jni_env);
    //create DexClassLoader
    jobject class_loader = getSystemClassLoader(jni_env);
    handleException(jni_env);
    jobject dex_loader_obj = jni_env->NewObject(dexloader_claxx, dexloader_init_method, dex_path, dex_out_path, NULL, class_loader);
    handleException(jni_env);
    //load dex
    jstring class_name = jni_env->NewStringUTF("com.expample.tools.EntryClass");
    jclass entry_class = static_cast<jclass>(jni_env->CallObjectMethod(dex_loader_obj, loadClass_method, class_name));
    handleException(jni_env);
    jmethodID invoke_method = jni_env->GetStaticMethodID(entry_class, "invoke", "(I)Ljava/lang/Object;");
    handleException(jni_env);
    jclass clazzTarget = (jclass) jni_env->CallStaticObjectMethod(entry_class, invoke_method, 0);
    
    detachCurrent();

}


extern "C" void InjectInterface(char* arg){
    LOGI("*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*");
    LOGI("*-*-*-*-*-* Injected so *-*-*-*-*-*-*-*");
    pthread_t pth_id;
    void *status;
    pthread_create(&pth_id, NULL, load_dex, NULL);
    pthread_join(pth_id, &status);
    LOGI("*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*");
}