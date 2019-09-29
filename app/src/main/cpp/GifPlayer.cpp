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

static int width = 0;
static int height = 0;
static GifFileType *GifFile = NULL;
static PthreadSleep pthreadSleep;
static SyncTime syncTime;
static pthread_mutex_t play_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t play_cond = PTHREAD_COND_INITIALIZER;
static bool is_pause = false;
static bool play_finish = false;

static void PrintGifError(int Error) {
    LOGI("PrintGifError: %s", GifErrorString(Error));
}

static int fileRead(GifFileType *gif, GifByteType *bytes, int size) {
    FILE *file = (FILE *) gif->UserData;
    LOGI("fileRead file: %p",file);
    return (int) fread(bytes, 1, size, file);
}

static int loadGifInfo(const char *FileName) {
    int Error;
    GifFile = NULL;
    width = 0;
    height = 0;

    int FileHandle;
    GifFileType *GifFile;
    if ((FileHandle = open(FileName, O_RDONLY)) == -1) {
        return -12;
    }
    LOGI("FileHandle: %d",FileHandle);
    FILE *f = fdopen(FileHandle, "rb");
    LOGI("fdopen f: %p",f);
    if ((GifFile = DGifOpen(f, fileRead, &Error)) == NULL) {
        PrintGifError(Error);
        return -11;
    }
//    if ((GifFile = DGifOpenFileName(FileName, &Error)) == NULL) {
//        PrintGifError(Error);
//        return -1;
//    }
    width = GifFile->SWidth;
    height = GifFile->SHeight;
    LOGI("gif SWidth: %d SHeight: %d", GifFile->SWidth, GifFile->SHeight);
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
static void
drawBitmap(JNIEnv *env, jobject bitmap, SavedImage *SavedImages, ColorMapObject *ColorMap,
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


static int GIF2RGB(JNIEnv *env, jobject bitmap, jobject runnable) {
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

    jclass runClass = env->GetObjectClass(runnable);
    jmethodID runMethod = env->GetMethodID(runClass, "run", "()V");

    /*
     * Allocate the screen as vector of column of rows. Note this
     * screen is device independent - it's the screen defined by the
     * GIF file parameters.
     */
    if ((ScreenBuffer = (GifRowType *)
            malloc(GifFile->SHeight * sizeof(GifRowType))) == NULL) {
        LOGI("Failed to allocate memory required, aborted.");
        return -11;
    }
    size = GifFile->SWidth * sizeof(GifPixelType);/* Size in bytes one row.*/
    if ((ScreenBuffer[0] = (GifRowType) malloc(size)) == NULL) /* First row. */
    {
        LOGE("Failed to allocate memory required, aborted.");
        return -3;
    }
    GifPixelType *buffer;
    buffer = (GifPixelType *) (ScreenBuffer[0]);
    for (i = 0; i < GifFile->SWidth; i++)  /* Set its color to BackGround. */
        buffer[i] = (GifPixelType) GifFile->SBackGroundColor;
    for (i = 1; i < GifFile->SHeight; i++) {
        /* Allocate the other rows, and set their color to background too: */
        if ((ScreenBuffer[i] = (GifRowType) malloc(size)) == NULL) {
            LOGI("Failed to allocate memory required, aborted.");
            return -4;
        }
        memcpy(ScreenBuffer[i], ScreenBuffer[0], size);
    }
    do {
        if (DGifGetRecordType(GifFile, &RecordType) == GIF_ERROR) {
            PrintGifError(GifFile->Error);
            return -5;
        }
        switch (RecordType) {
            case IMAGE_DESC_RECORD_TYPE:
                if (DGifGetImageDesc(GifFile) == GIF_ERROR) {
                    PrintGifError(GifFile->Error);
                    return -6;
                }
                sp = &GifFile->SavedImages[GifFile->ImageCount - 1];
                sp->RasterBits = (unsigned char *) reallocarray(NULL, width * height +
                                                                      sizeof(int32_t) * 2,
                                                                sizeof(GifPixelType));
                int32_t *p2;
                p2 = (int32_t *) sp->RasterBits;
                p2[0] = delayTime;
                p2[1] = transparentColorIndex;
                delayTime = 0;
//                transparentColorIndex = NO_TRANSPARENT_COLOR;
                LOGI(">>>time: %d tColorIndex: %d", p2[0], transparentColorIndex);
                //
                Row = GifFile->Image.Top; /* Image Position relative to Screen. */
                Col = GifFile->Image.Left;
                Width = GifFile->Image.Width;
                Height = GifFile->Image.Height;

                LOGI("GifFile Image %d at (%d, %d) [%dx%d]", ++ImageNum, Col, Row, Width, Height);

                if (GifFile->Image.Left + GifFile->Image.Width > GifFile->SWidth ||
                    GifFile->Image.Top + GifFile->Image.Height > GifFile->SHeight) {
                    LOGI("Image %d is not confined to screen dimension, aborted", ImageNum);
                    return -7;
                }
                if (GifFile->Image.Interlace) {
                    /* Need to perform 4 passes on the images: */
                    for (i = 0; i < 4; i++)
                        for (j = Row + InterlacedOffset[i]; j < Row + Height;
                             j += InterlacedJumps[i]) {
                            if (DGifGetLine(GifFile, &ScreenBuffer[j][Col],
                                            Width) == GIF_ERROR) {
                                PrintGifError(GifFile->Error);
                                return -8;
                            }
                        }
                } else {
                    for (i = 0; i < Height; i++) {
                        if (DGifGetLine(GifFile, &ScreenBuffer[Row++][Col],
                                        Width) == GIF_ERROR) {
                            PrintGifError(GifFile->Error);
                            return -9;
                        }
                    }
                }
                ColorMap = (GifFile->Image.ColorMap
                            ? GifFile->Image.ColorMap
                            : GifFile->SColorMap);
                drawBitmap(env, bitmap, sp, ColorMap, ScreenBuffer,
                           GifFile->Image.Left, GifFile->Image.Top,
                           GifFile->Image.Left + Width, GifFile->Image.Top + Height,
                           transparentColorIndex);
                env->CallVoidMethod(runnable, runMethod);
                break;
            case EXTENSION_RECORD_TYPE:
                /* Skip any extension blocks in file: */
                if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR) {
                    PrintGifError(GifFile->Error);
                    return -10;
                }
                if (ExtCode == GRAPHICS_EXT_FUNC_CODE) {
                    if (Extension[0] != 4) {
                        PrintGifError(GIF_ERROR);
                        return -20;
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
                    pthreadSleep.msleep(dt);
                    syncTime.set_clock();
                }
                while (Extension != NULL) {
                    if (DGifGetExtensionNext(GifFile, &Extension) == GIF_ERROR) {
                        PrintGifError(GifFile->Error);
                        return -11;
                    }
                }
                break;
            case TERMINATE_RECORD_TYPE:
                break;
            default:            /* Should be trapped by DGifGetRecordType. */
                break;
        }
    } while (RecordType != TERMINATE_RECORD_TYPE && !play_finish);

    free(ScreenBuffer);

    syncTime.reset_clock();
//    ColorMapObject *ColorMap;
    while (!play_finish) {
        for (int t = 0; t < GifFile->ImageCount; t++) {
            SavedImage frame = GifFile->SavedImages[t];
            GifImageDesc frameInfo = frame.ImageDesc;
            LOGI("GifFile Image %d at (%d, %d) [%dx%d]", t, frameInfo.Left, frameInfo.Top,
                 frameInfo.Width, frameInfo.Height);
            ColorMap = (frameInfo.ColorMap
                        ? frameInfo.ColorMap
                        : GifFile->SColorMap);
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
            LOGI("d_time: %d tColorIndex: %d", d_time, tColorIndex);
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
            pthreadSleep.msleep(dt);
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
    if (DGifCloseFile(GifFile, &Error) == GIF_ERROR) {
        PrintGifError(Error);
        return -12;
    }
    if (GifFile->UserData) {
        FILE *file = (FILE *) GifFile->UserData;
        fclose(file);
    }
    return 0;
}


JNIEXPORT jboolean JNICALL load_gif_jni(JNIEnv *env, jclass type, jstring gifPath_) {
    const char *gifPath = env->GetStringUTFChars(gifPath_, 0);

    int ret = loadGifInfo(gifPath);

    env->ReleaseStringUTFChars(gifPath_, gifPath);

    return (jboolean) (ret == 0 ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT void JNICALL start_jni(JNIEnv *env, jclass type, jobject bitmap,
                                 jobject runnable) {
    pthread_mutex_lock(&play_mutex);
    play_finish = false;
    pthread_mutex_unlock(&play_mutex);
    GIF2RGB(env, bitmap, runnable);
}

JNIEXPORT void JNICALL pause_jni(JNIEnv *env, jclass type) {
    pthread_mutex_lock(&play_mutex);
    is_pause = true;
    pthread_mutex_unlock(&play_mutex);
}

JNIEXPORT void JNICALL resume_jni(JNIEnv *env, jclass type) {
    pthread_mutex_lock(&play_mutex);
    is_pause = false;
    pthread_cond_signal(&play_cond);
    pthread_mutex_unlock(&play_mutex);
}

JNIEXPORT jint JNICALL get_width_jni(JNIEnv *env, jclass typee) {
    return width;
}

JNIEXPORT jint JNICALL get_height_jni(JNIEnv *env, jclass type) {
    return height;
}

JNIEXPORT void JNICALL release_jni(JNIEnv *env, jclass type) {
    pthread_mutex_lock(&play_mutex);
    play_finish = true;
    is_pause = false;
    pthread_cond_signal(&play_cond);
    pthread_mutex_unlock(&play_mutex);
    pthreadSleep.interrupt();
}

JNINativeMethod method[] = {{"load_gif",   "(Ljava/lang/String;)Z",                            (void *) load_gif_jni},
                            {"start",      "(Landroid/graphics/Bitmap;Ljava/lang/Runnable;)V", (void *) start_jni},
                            {"pause",      "()V",                                              (void *) pause_jni},
                            {"resume",     "()V",                                              (void *) resume_jni},
                            {"get_width",  "()I",                                              (void *) get_width_jni},
                            {"get_height", "()I",                                              (void *) get_height_jni},
                            {"release",    "()V",                                              (void *) release_jni},
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
