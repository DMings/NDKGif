#include <jni.h>
#include <string>
#include <malloc.h>
#include <string.h>
#include "giflib/gif_lib.h"
#include "log.h"
#include <android/log.h>
#include <android/bitmap.h>

#define  argb(a, r, g, b) ( ((a) & 0xff) << 24 ) | ( ((b) & 0xff) << 16 ) | ( ((g) & 0xff) << 8 ) | ((r) & 0xff)

typedef struct GifBean {
    int current_frame;
    int total_frame;
    int *delays;
} GifBean;

extern "C" {

void
drawFrame(GifFileType *gifFileType, GifBean *gifBean, AndroidBitmapInfo bitmapInfo, void *pixels) {
//    LOGI("drawFrame>>>111");
    SavedImage savedImage = gifFileType->SavedImages[gifBean->current_frame];

    GifImageDesc frameInfo = savedImage.ImageDesc;
    int *px = (int *) pixels;
    //
    int *line;
//    LOGI("drawFrame>>>222");
    //Index of pixel in bitmap
    int pointPixelIdx;
    GifByteType gifByteType;
    GifColorType gifColorType;
    /* The local color map */
    ColorMapObject *colorMapObject = frameInfo.ColorMap;
//    LOGI("drawFrame>>>colorMapObject: %p",colorMapObject);
    //Offset to the real color pixel; stride:The number of byte per row;
    px = (int *) ((char *) px + bitmapInfo.stride * frameInfo.Top);
//    LOGI("drawFrame>>>px: %p %d %d", px, bitmapInfo.stride, frameInfo.Top);
    for (int y = frameInfo.Top; y < frameInfo.Top + frameInfo.Height; y++) {
//        LOGI("drawFrame>>>y: %d", y);
        line = px;
        for (int x = frameInfo.Left; x < frameInfo.Left + frameInfo.Width; x++) {
//            LOGI("drawFrame>>>x: %d", x);
            pointPixelIdx = (y - frameInfo.Top) * frameInfo.Width + (x - frameInfo.Left);
//            LOGI("drawFrame>>>pointPixelIdx: %d", pointPixelIdx);
            //Can't use it directly, as it has been compressed.
            gifByteType = savedImage.RasterBits[pointPixelIdx];
//            LOGI("drawFrame>>>gifByteType: %d", gifByteType);
            gifColorType = colorMapObject->Colors[gifByteType];
//            LOGI("drawFrame>>>gifColorType: %d", gifColorType);
            //Now, it is true pixel color.
            line[x] = argb(255, gifColorType.Red, gifColorType.Green, gifColorType.Blue);
        }
        px = (int *) ((char *) px + bitmapInfo.stride);
    }
//    LOGI("drawFrame>>>555");
}

JNIEXPORT jlong JNICALL
Java_com_dming_testgif_GifHandler_loadGif(JNIEnv *env, jobject instance, jstring path_) {
    const char *path = env->GetStringUTFChars(path_, 0);
    int error;
    /** Open a new GIF file for read, given by its name.Returns dynamically allocated GifFileType pointer which serves as the GIF
info record.*/
    GifFileType *gifFileType = DGifOpenFileName(path, &error);
    //Init gifFileType;
    int ret = DGifSlurp(gifFileType);
    LOGI("ret: %d",ret);
    GifBean *gifBean = (GifBean *) malloc(sizeof(GifBean));

    memset(gifBean, 0, sizeof(GifBean));
    //Like view.setTag(); used to store/cache data.
    gifFileType->UserData = gifBean;

    //ImageCount is total frames of gif, to alloc a size of ImageCount for array 'delays'.
    gifBean->delays = (int *) malloc(sizeof(int) * gifFileType->ImageCount);
    //Init delays
    memset(gifBean->delays, 0, sizeof(int) * gifFileType->ImageCount);
    gifBean->total_frame = gifFileType->ImageCount;

    ExtensionBlock *ext;
    for (int i = 0; i < gifFileType->ImageCount; ++i) {
        SavedImage frame = gifFileType->SavedImages[i];
        for (int j = 0; j < frame.ExtensionBlockCount; ++j) {
            /*Find the graphics control extension block (GIF89) */
            if (frame.ExtensionBlocks[j].Function == GRAPHICS_EXT_FUNC_CODE) {
                ext = &frame.ExtensionBlocks[j];
                break;
            }
        }
        if (ext) {
            //Bytes[2]存放第5个字节（高八位），Bytes[1]存放第6个字节（低8位）。
            int frame_delay = 10 * (ext->Bytes[2] << 8 | ext->Bytes[1]);
            gifBean->delays[i] = frame_delay;
        }
    }

    env->ReleaseStringUTFChars(path_, path);
    return (jlong) gifFileType;
}

JNIEXPORT jint JNICALL
Java_com_dming_testgif_GifHandler_getWidth(JNIEnv *env, jobject instance, jlong gifAddr) {
    GifFileType *gifFileType = (GifFileType *) gifAddr;
    LOGI("gifFileType->SWidth>>>%d", gifFileType->SWidth);
    return gifFileType->SWidth;
}

JNIEXPORT jint JNICALL
Java_com_dming_testgif_GifHandler_getHeight(JNIEnv *env, jobject instance, jlong gifAddr) {
    GifFileType *gifFileType = (GifFileType *) gifAddr;
    LOGI("gifFileType->SHeight>>>%d", gifFileType->SHeight);
    return gifFileType->SHeight;
}


JNIEXPORT jint JNICALL
Java_com_dming_testgif_GifHandler_updateFrame(JNIEnv *env, jobject instance, jobject bitmap,
                                              jlong gifAddr) {
    GifFileType *gifFileType = (GifFileType *) gifAddr;
    GifBean *gifBean = (GifBean *) gifFileType->UserData;

    //
    AndroidBitmapInfo bitmapInfo;
    void *pixels;
    AndroidBitmap_getInfo(env, bitmap, &bitmapInfo);

    LOGI("Java_com_dming_testgif_GifHandler_updateFrame111");

    //Given a java bitmap object, attempt to lock the pixel address.
    AndroidBitmap_lockPixels(env, bitmap, &pixels);


    drawFrame(gifFileType, gifBean, bitmapInfo, pixels);

    //Update current frame
    gifBean->current_frame += 1;

    if (gifBean->current_frame >= gifBean->total_frame - 1) {
        gifBean->current_frame = 0;
    }
    AndroidBitmap_unlockPixels(env, bitmap);

    LOGI("Java_com_dming_testgif_GifHandler_updateFrame222");

    return gifBean->delays[gifBean->current_frame];
}

}