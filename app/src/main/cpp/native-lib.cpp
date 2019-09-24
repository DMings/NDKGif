#include <jni.h>
#include <string>
#include "log.h"
#include "giflib/gif_lib.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_dming_testgif_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++--x-";


    LOGI("test: GifErrorString=> %s", GifErrorString(D_GIF_ERR_OPEN_FAILED));


    return env->NewStringUTF(hello.c_str());
}
