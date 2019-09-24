#include <jni.h>
#include <string>
#include "log.h"
#include "giflib/gif_lib.h"

extern "C"
JNIEXPORT void JNICALL
Java_com_dming_testgif_MainActivity_testGif(JNIEnv *env, jobject instance, jstring gifPath_) {
    const char *gifPath = env->GetStringUTFChars(gifPath_, 0);

    int err;
    GifFileType *gifFileType = DGifOpenFileName(gifPath, &err);//调用源码api里方法，打开gif，返回GifFileType实体
    LOGI("start GifErrorString: %s", GifErrorString(err));
    if (DGifSlurp(gifFileType) == GIF_OK) {
        LOGI("gif SWidth: %d SHeight: %d", gifFileType->SWidth, gifFileType->SHeight);
        LOGI("gif ImageCount: %d", gifFileType->ImageCount);
        LOGI("gif SColorResolution: %d", gifFileType->SColorResolution);
    }
    DGifCloseFile(gifFileType, &err);
    LOGI("end GifErrorString: %s", GifErrorString(err));
    env->ReleaseStringUTFChars(gifPath_, gifPath);
}