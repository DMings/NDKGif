#include <jni.h>
#include <string>
#include <malloc.h>
#include <string.h>
#include "log.h"
#include "giflib/gif_lib.h"
#include <unistd.h>
#include <android/bitmap.h>

#define UNSIGNED_LITTLE_ENDIAN(lo, hi)    ((lo) | ((hi) << 8))
#define  MAKE_COLOR_ABGR(r, g, b) ((0xff) << 24 ) | ((b) << 16 ) | ((g) << 8 ) | ((r) & 0xff)
//typedef uint32_t ColorARGB;
#define  argb(a, r, g, b) ( ((a) & 0xff) << 24 ) | ( ((b) & 0xff) << 16 ) | ( ((g) & 0xff) << 8 ) | ((r) & 0xff)

static int width = 0;
static int height = 0;
static GifFileType *GifFile = NULL;

static void PrintGifError(int Error) {
    LOGI("PrintGifError: %s", GifErrorString(Error));
}

static int loadGifInfo(const char *FileName) {
    int Error;
    GifFile = NULL;
    width = 0;
    height = 0;
    if ((GifFile = DGifOpenFileName(FileName, &Error)) == NULL) {
        PrintGifError(Error);
        return -1;
    }
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
    if (colorIndex != transparentColorIndex) {
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
    SavedImages->RasterBits = (unsigned char *) reallocarray(NULL, width * height,
                                                             sizeof(GifPixelType));
    int imageIndex = 0;
    //
    for (int h = top; h < height; h++) {
        for (int w = left; w < width; w++) {
            setColorARGB(&sPixels[(bitmapInfo.stride / 4) * h + w],
                         ColorMap,
                         transparentColorIndex,
                         ScreenBuffer[h][w]);
            SavedImages->RasterBits[imageIndex++] = (GifByteType) ScreenBuffer[h][w];
        }
    }
    AndroidBitmap_unlockPixels(env, bitmap);
}

static void printMsg(JNIEnv *env, jobject bitmap, int transparentColorIndex) {

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
    int delayTime;
    GifByteType *GifExtension;
    int transparentColorIndex = NO_TRANSPARENT_COLOR;
    ColorMapObject *ColorMap;
    size_t size;
    SavedImage *sp = NULL;

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
//    drawBitmap(env, bitmap, ScreenBuffer);
//    env->CallVoidMethod(runnable, runMethod);
    /* Scan the content of the GIF file and load the image(s) in: */
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
                LOGI("EXTENSION_RECORD_TYPE");
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
                    transparentColorIndex = (int) GifExtension[3];
                    usleep(delayTime * 10000);
                    LOGI("DelayTime: %d", delayTime);
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
    } while (RecordType != TERMINATE_RECORD_TYPE);

//    /* Lets dump it - set the global variables required and do it: */
//    ColorMap = (GifFile->Image.ColorMap
//                ? GifFile->Image.ColorMap
//                : GifFile->SColorMap);
//    if (ColorMap == NULL) {
//        fprintf(stderr, "Gif Image does not have a colormap\n");
//        return -12;
//    }

    free(ScreenBuffer);

    printMsg(env, bitmap, transparentColorIndex);

    LOGI("printMsg>>>>>>>>>>>>>>>>>>>>");
//    ColorMapObject *ColorMap;
    for (int c = 0; c < 10; ++c) {
        for (int t = 0; t < GifFile->ImageCount; t++) {
            SavedImage frame = GifFile->SavedImages[t];
            GifImageDesc frameInfo = frame.ImageDesc;
            LOGI("GifFile Image %d at (%d, %d) [%dx%d]", t, frameInfo.Left, frameInfo.Top,
                 frameInfo.Width, frameInfo.Height);
//        frameInfo.ColorMap[];
            ColorMap = (GifFile->Image.ColorMap
                        ? GifFile->Image.ColorMap
                        : GifFile->SColorMap);
            //
            AndroidBitmapInfo bitmapInfo;
            void *pixels;
            AndroidBitmap_getInfo(env, bitmap, &bitmapInfo);
            AndroidBitmap_lockPixels(env, bitmap, &pixels);
//        uint32_t *sPixels = (uint32_t *) pixels;
            //
            uint32_t *sPixels = (uint32_t *) pixels;
            int pointPixelIdx = 0;
            for (int h = frameInfo.Top; h < frameInfo.Top + frameInfo.Height; h++) {
                for (int w = frameInfo.Left; w < frameInfo.Left + frameInfo.Width; w++) {
                    setColorARGB(&sPixels[(bitmapInfo.stride / 4) * h + w],
                                 ColorMap,
                                 transparentColorIndex,
                                 frame.RasterBits[pointPixelIdx++]);
                }
            }
            //
            AndroidBitmap_unlockPixels(env, bitmap);
            env->CallVoidMethod(runnable, runMethod);
            usleep(40000);
        }
    }

    if (DGifCloseFile(GifFile, &Error) == GIF_ERROR) {
        PrintGifError(Error);
        return -12;
    }
    return 0;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_dming_testgif_TestGif_loadGif(JNIEnv *env, jobject instance, jstring gifPath_) {
    const char *gifPath = env->GetStringUTFChars(gifPath_, 0);

    int ret = loadGifInfo(gifPath);

    env->ReleaseStringUTFChars(gifPath_, gifPath);

    return (jboolean) (ret == 0 ? JNI_TRUE : JNI_FALSE);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_dming_testgif_TestGif_getWidth(JNIEnv *env, jobject instance) {
    return width;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_dming_testgif_TestGif_getHeight(JNIEnv *env, jobject instance) {
    return height;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dming_testgif_TestGif_testGif(JNIEnv *env, jobject instance, jobject bitmap,
                                       jobject runnable) {
    GIF2RGB(env, bitmap, runnable);
}