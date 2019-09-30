#include <jni.h>
#include <string>
#include <malloc.h>
#include <string.h>
#include "log.h"
#include "giflib/gif_lib.h"
#include <unistd.h>
#include <android/bitmap.h>
#include "PthreadSleep.h"
#include "SyncTime.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <fcntl.h>

#define UNSIGNED_LITTLE_ENDIAN(lo, hi)    ((lo) | ((hi) << 8))
#define  MAKE_COLOR_ABGR(r, g, b) ((0xff) << 24 ) | ((b) << 16 ) | ((g) << 8 ) | ((r) & 0xff)

static GifFileType *gifFile = NULL;
static PthreadSleep threadSleep;
static SyncTime syncTime;
static pthread_mutex_t play_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t play_cond = PTHREAD_COND_INITIALIZER;
static bool is_pause = false;
static bool is_play_quit = false;
static int gif_width = 0;
static int gif_height = 0;
enum PlayState {
    IDLE, PREPARE, PLAYING
};
static enum PlayState play_state = IDLE; // 0 idle 1 playing -1 stop

static void PrintGifError(int Error) {
    LOGI("PrintGifError: %s", GifErrorString(Error));
}

static void setPlayState(PlayState state) {
    pthread_mutex_lock(&play_mutex);
    play_state = state;
    pthread_mutex_unlock(&play_mutex);
}

static void getPlayState(PlayState *playState) {
    pthread_mutex_lock(&play_mutex);
    *playState = play_state;
    pthread_mutex_unlock(&play_mutex);
}

static int fileRead(GifFileType *gif, GifByteType *buf, int size) {
    AAsset *asset = (AAsset *) gif->UserData;
    return AAsset_read(asset, buf, (size_t) size);
}

static int loadGifInfo(JNIEnv *env, jobject assetManager, const char *filename) {
    int Error;
    gif_width = 0;
    gif_height = 0;
    is_play_quit = false;
    threadSleep.reset();
    if (gifFile != NULL) {
        DGifCloseFile(gifFile, &Error);
        gifFile = NULL;
    }
    setPlayState(PREPARE);
    if (assetManager) {
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        AAsset *asset = AAssetManager_open(mgr, filename, AASSET_MODE_STREAMING);
        if ((gifFile = DGifOpen(asset, fileRead, &Error)) == NULL) {
            setPlayState(IDLE);
            PrintGifError(Error);
            return -1;
        }
    } else {
        if ((gifFile = DGifOpenFileName(filename, &Error)) == NULL) { // 用外部路径用这里
            PrintGifError(Error);
            return -1;
        }
    }
    gif_width = gifFile->SWidth;
    gif_height = gifFile->SHeight;
    LOGI("gif SWidth: %d SHeight: %d", gifFile->SWidth, gifFile->SHeight);
    return 0;
}

static uint32_t gifColorToColorARGB(const GifColorType &color) {
    return (uint32_t) (MAKE_COLOR_ABGR(color.Red, color.Green, color.Blue));
}

static void setColorARGB(uint32_t *sPixels, ColorMapObject *colorMap, int transparentColorIndex,
                         GifByteType colorIndex) {
    if (colorIndex != transparentColorIndex || transparentColorIndex == NO_TRANSPARENT_COLOR) {
        *sPixels = gifColorToColorARGB(colorMap->Colors[colorIndex]);
    }
}

// RGBA_8888
static void drawBitmap(JNIEnv *env, jobject bitmap,
                       SavedImage *SavedImages, ColorMapObject *ColorMap,
                       GifRowType *ScreenBuffer,
                       int left,
                       int top, int width, int height, int transparentColorIndex) {
    //
    AndroidBitmapInfo bitmapInfo;
    void *pixels;
    AndroidBitmap_getInfo(env, bitmap, &bitmapInfo);
    AndroidBitmap_lockPixels(env, bitmap, &pixels);
    uint32_t *sPixels = (uint32_t *) pixels;
    //
    int imageIndex = sizeof(int32_t) * 2;
    //
    for (int h = top; h < height; h++) {
        for (int w = left; w < width; w++) {
            setColorARGB(&sPixels[(bitmapInfo.stride / 4) * h + w],
                         ColorMap,
                         transparentColorIndex,
                         ScreenBuffer[h][w]);
            if (SavedImages != NULL) {
                SavedImages->RasterBits[imageIndex++] = (GifByteType) ScreenBuffer[h][w];
            }
        }
    }
    AndroidBitmap_unlockPixels(env, bitmap);
}


static void playGif(JNIEnv *env, jobject bitmap, jobject runnable) {
    int i, j, Row, Col, Width, Height, ExtCode;
    GifRecordType RecordType;
    GifByteType *Extension;
    GifRowType *ScreenBuffer;
    int InterlacedOffset[] = {0, 4, 2, 1}; /* The way Interlaced image should. */
    int InterlacedJumps[] = {8, 8, 4, 2};    /* be read - offsets and jumps... */
    int ImageNum = 0;
    int Error;
    GifByteType *GifExtension;
    ColorMapObject *ColorMap;
    size_t size;
    SavedImage *sp = NULL;
    int32_t delayTime = 0;
    int32_t transparentColorIndex = NO_TRANSPARENT_COLOR;

    setPlayState(PLAYING);
    syncTime.reset_clock();

    jclass runClass = env->GetObjectClass(runnable);
    jmethodID runMethod = env->GetMethodID(runClass, "run", "()V");

    if ((ScreenBuffer = (GifRowType *)
            malloc(gifFile->SHeight * sizeof(GifRowType))) == NULL) {
        LOGI("Failed to allocate memory required, aborted.");
        goto end;
    }
    size = gifFile->SWidth * sizeof(GifPixelType);
    if ((ScreenBuffer[0] = (GifRowType) malloc(size)) == NULL) {
        LOGE("Failed to allocate memory required, aborted.");
        goto end;
    }
    GifPixelType *buffer;
    buffer = (GifPixelType *) (ScreenBuffer[0]);
    for (i = 0; i < gifFile->SWidth; i++)
        buffer[i] = (GifPixelType) gifFile->SBackGroundColor;
    for (i = 1; i < gifFile->SHeight; i++) {
        if ((ScreenBuffer[i] = (GifRowType) malloc(size)) == NULL) {
            LOGI("Failed to allocate memory required, aborted.");
            goto end;
        }
        memcpy(ScreenBuffer[i], ScreenBuffer[0], size);
    }
    do {
        if (DGifGetRecordType(gifFile, &RecordType) == GIF_ERROR) {
            PrintGifError(gifFile->Error);
            goto end;
        }
        switch (RecordType) {
            case IMAGE_DESC_RECORD_TYPE:
                if (DGifGetImageDesc(gifFile) == GIF_ERROR) {
                    PrintGifError(gifFile->Error);
                    goto end;
                }
                sp = &gifFile->SavedImages[gifFile->ImageCount - 1];
                sp->RasterBits = (unsigned char *) malloc(
                        sizeof(GifPixelType) * gif_width * gif_height +
                        sizeof(int32_t) * 2);
                int32_t *p2;
                p2 = (int32_t *) sp->RasterBits;
                p2[0] = delayTime;
                p2[1] = transparentColorIndex;
                delayTime = 0;
//                LOGI(">>>time: %d tColorIndex: %d", p2[0], transparentColorIndex);
                //
                Row = gifFile->Image.Top;
                Col = gifFile->Image.Left;
                Width = gifFile->Image.Width;
                Height = gifFile->Image.Height;

//                LOGI("gifFile Image %d at (%d, %d) [%dx%d]", ++ImageNum, Col, Row, Width, Height);

                if (gifFile->Image.Left + gifFile->Image.Width > gifFile->SWidth ||
                    gifFile->Image.Top + gifFile->Image.Height > gifFile->SHeight) {
                    LOGI("Image %d is not confined to screen dimension, aborted", ImageNum);
                    goto end;
                }
                if (gifFile->Image.Interlace) {
                    for (i = 0; i < 4; i++)
                        for (j = Row + InterlacedOffset[i]; j < Row + Height;
                             j += InterlacedJumps[i]) {
                            if (DGifGetLine(gifFile, &ScreenBuffer[j][Col],
                                            Width) == GIF_ERROR) {
                                PrintGifError(gifFile->Error);
                                goto end;
                            }
                        }
                } else {
                    for (i = 0; i < Height; i++) {
                        if (DGifGetLine(gifFile, &ScreenBuffer[Row++][Col],
                                        Width) == GIF_ERROR) {
                            PrintGifError(gifFile->Error);
                            goto end;
                        }
                    }
                }
                ColorMap = (gifFile->Image.ColorMap
                            ? gifFile->Image.ColorMap
                            : gifFile->SColorMap);
                drawBitmap(env, bitmap, sp, ColorMap, ScreenBuffer,
                           gifFile->Image.Left, gifFile->Image.Top,
                           gifFile->Image.Left + Width, gifFile->Image.Top + Height,
                           transparentColorIndex);
                env->CallVoidMethod(runnable, runMethod);
                break;
            case EXTENSION_RECORD_TYPE:
                if (DGifGetExtension(gifFile, &ExtCode, &Extension) == GIF_ERROR) {
                    PrintGifError(gifFile->Error);
                    goto end;
                }
                if (ExtCode == GRAPHICS_EXT_FUNC_CODE) {
                    if (Extension[0] != 4) {
                        PrintGifError(GIF_ERROR);
                        goto end;
                    }
                    GifExtension = Extension + 1;
                    delayTime = UNSIGNED_LITTLE_ENDIAN(GifExtension[1], GifExtension[2]);
                    if (GifExtension[0] & 0x01)
                        transparentColorIndex = (int) GifExtension[3];
                    else
                        transparentColorIndex = NO_TRANSPARENT_COLOR;

                    pthread_mutex_lock(&play_mutex);
                    if (is_pause) {
                        is_pause = false;
                        pthread_cond_wait(&play_cond, &play_mutex);
                        syncTime.reset_clock();
                    }
                    pthread_mutex_unlock(&play_mutex);

                    unsigned int dt = 0;
                    dt = syncTime.synchronize_time(delayTime * 10);
                    threadSleep.msleep(dt);
                    syncTime.set_clock();
                }
                while (Extension != NULL) {
                    if (DGifGetExtensionNext(gifFile, &Extension) == GIF_ERROR) {
                        PrintGifError(gifFile->Error);
                        goto end;
                    }
                }
                break;
            case TERMINATE_RECORD_TYPE:
                break;
            default:
                break;
        }
    } while (RecordType != TERMINATE_RECORD_TYPE && !is_play_quit);

    // 释放不再使用变量
    free(ScreenBuffer);
    ScreenBuffer = NULL;
    if (gifFile->UserData) {
        AAsset *asset = (AAsset *) gifFile->UserData;
        AAsset_close(asset);
        gifFile->UserData = NULL;
    }
    //释放不再使用变量

    syncTime.reset_clock();
    while (!is_play_quit) {
        for (int t = 0; t < gifFile->ImageCount; t++) {
            if(is_play_quit){
                break;
            }
            SavedImage frame = gifFile->SavedImages[t];
            GifImageDesc frameInfo = frame.ImageDesc;
//            LOGI("gifFile Image %d at (%d, %d) [%dx%d]", t, frameInfo.Left, frameInfo.Top,
//                 frameInfo.Width, frameInfo.Height);
            ColorMap = (frameInfo.ColorMap
                        ? frameInfo.ColorMap
                        : gifFile->SColorMap);
            //
            AndroidBitmapInfo bitmapInfo;
            void *pixels;
            AndroidBitmap_getInfo(env, bitmap, &bitmapInfo);
            AndroidBitmap_lockPixels(env, bitmap, &pixels);
            uint32_t *sPixels = (uint32_t *) pixels;
            //
            int32_t d_time = 0;
            int32_t tColorIndex = 0;
            int32_t *p1;
            p1 = (int32_t *) frame.RasterBits;
            d_time = p1[0];
            tColorIndex = p1[1];
//            LOGI("d_time: %d tColorIndex: %d", d_time, tColorIndex);
            //
            pthread_mutex_lock(&play_mutex);
            if (is_pause) {
                is_pause = false;
                pthread_cond_wait(&play_cond, &play_mutex);
                syncTime.reset_clock();
            }
            pthread_mutex_unlock(&play_mutex);
            //
            unsigned int dt = 0;
            dt = syncTime.synchronize_time(d_time * 10);
            threadSleep.msleep(dt);
            syncTime.set_clock();
            //
            int pointPixelIdx = sizeof(int32_t) * 2;
            for (int h = frameInfo.Top; h < frameInfo.Top + frameInfo.Height; h++) {
                for (int w = frameInfo.Left; w < frameInfo.Left + frameInfo.Width; w++) {
                    setColorARGB(&sPixels[(bitmapInfo.stride / 4) * h + w],
                                 ColorMap,
                                 tColorIndex,
                                 frame.RasterBits[pointPixelIdx++]);
                }
            }
            //
            AndroidBitmap_unlockPixels(env, bitmap);
            env->CallVoidMethod(runnable, runMethod);
        }
    }
    // free
    end:
    if (ScreenBuffer) {
        free(ScreenBuffer);
    }
    if (gifFile->UserData) {
        AAsset *asset = (AAsset *) gifFile->UserData;
        AAsset_close(asset);
        gifFile->UserData = NULL;
    }
    if (DGifCloseFile(gifFile, &Error) == GIF_ERROR) {
        PrintGifError(Error);
    }
    gifFile = NULL;
    setPlayState(IDLE);
}

/////////////////////////////////jni/////////////////////////////////////////////

JNIEXPORT jboolean JNICALL
load_gif_jni(JNIEnv *env, jclass type, jobject assetManager, jstring gifPath_) {
    const char *gifPath = env->GetStringUTFChars(gifPath_, 0);
    PlayState playState;
    int ret;
    getPlayState(&playState);
    if (playState == IDLE) {
        ret = loadGifInfo(env, assetManager, gifPath);
    } else {
        ret = -1;
    }
    env->ReleaseStringUTFChars(gifPath_, gifPath);
    return (jboolean) (ret == 0 ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT void JNICALL start_jni(JNIEnv *env, jclass type, jobject bitmap,
                                 jobject runnable) {
    PlayState playState;
    getPlayState(&playState);
    if (playState == PREPARE) {
        playGif(env, bitmap, runnable);
    }
}

JNIEXPORT void JNICALL pause_jni(JNIEnv *env, jclass type) {
    pthread_mutex_lock(&play_mutex);
    if (play_state == 1) {
        is_pause = true;
    }
    pthread_mutex_unlock(&play_mutex);
}

JNIEXPORT void JNICALL resume_jni(JNIEnv *env, jclass type) {
    pthread_mutex_lock(&play_mutex);
    is_pause = false;
    pthread_cond_signal(&play_cond);
    pthread_mutex_unlock(&play_mutex);
}

JNIEXPORT jint JNICALL get_width_jni(JNIEnv *env, jclass type) {
    return gif_width;
}

JNIEXPORT jint JNICALL get_height_jni(JNIEnv *env, jclass type) {
    return gif_height;
}

JNIEXPORT void JNICALL release_jni(JNIEnv *env, jclass type) {
    pthread_mutex_lock(&play_mutex);
    is_play_quit = true;
    is_pause = false;
    pthread_cond_signal(&play_cond);
    pthread_mutex_unlock(&play_mutex);
    threadSleep.interrupt();
}

JNINativeMethod method[] = {{"native_load_gif",   "(Landroid/content/res/AssetManager;Ljava/lang/String;)Z", (void *) load_gif_jni},
                            {"native_start",      "(Landroid/graphics/Bitmap;Ljava/lang/Runnable;)V",        (void *) start_jni},
                            {"native_pause",      "()V",                                                     (void *) pause_jni},
                            {"native_resume",     "()V",                                                     (void *) resume_jni},
                            {"native_get_width",  "()I",                                                     (void *) get_width_jni},
                            {"native_get_height", "()I",                                                     (void *) get_height_jni},
                            {"native_release",    "()V",                                                     (void *) release_jni},
};

jint registerNativeMethod(JNIEnv *env) {
    jclass cl = env->FindClass("com/dming/testgif/GifPlayer");
    if ((env->RegisterNatives(cl, method, sizeof(method) / sizeof(method[0]))) < 0) {
        return -1;
    }
    return 0;
}

jint unRegisterNativeMethod(JNIEnv *env) {
    jclass cl = env->FindClass("com/dming/testgif/GifPlayer");
    env->UnregisterNatives(cl);
    return 0;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) == JNI_OK) {
        registerNativeMethod(env);
        return JNI_VERSION_1_6;
    } else if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_OK) {
        registerNativeMethod(env);
        return JNI_VERSION_1_4;
    }
    return JNI_ERR;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) == JNI_OK) {
        unRegisterNativeMethod(env);
    } else if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_OK) {
        unRegisterNativeMethod(env);
    }
}
