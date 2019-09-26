#include <jni.h>
#include <string>
#include <malloc.h>
#include <string.h>
#include "log.h"
#include "giflib/gif_lib.h"
#include <unistd.h>
#include <android/bitmap.h>

#define UNSIGNED_LITTLE_ENDIAN(lo, hi)    ((lo) | ((hi) << 8))

static int width = 0;
static int height = 0;
static GifFileType *GifFile = NULL;

static void PrintGifError(int Error) {
    LOGI("PrintGifError: %s", GifErrorString(Error));
}

//static int DumpScreen2RGB(ColorMapObject *ColorMap,
//                          GifRowType *ScreenBuffer,
//                          int ScreenWidth, int ScreenHeight) {
//    int i, j;
//    GifRowType GifRow;
//    GifColorType *ColorMapEntry;
//    unsigned char *Buffer, *BufferP;
//
//    if ((Buffer = (unsigned char *) malloc(ScreenWidth * 3)) == NULL) {
//        LOGE("Failed to allocate memory required, aborted.");
//        return -1;
//    }
//    for (i = 0; i < ScreenHeight; i++) {
//        GifRow = ScreenBuffer[i];
//        LOGI("=->%-4d", ScreenHeight - i);
//        for (j = 0, BufferP = Buffer; j < ScreenWidth; j++) {
//            ColorMapEntry = &ColorMap->Colors[GifRow[j]];
//            *BufferP++ = ColorMapEntry->Red;
//            *BufferP++ = ColorMapEntry->Green;
//            *BufferP++ = ColorMapEntry->Blue;
//        }
////        if (fwrite(Buffer, ScreenWidth * 3, 1, rgbfp[0]) != 1) {
////            LOGE("Write to file(s) failed.");
////            return -2;
////        }
//    }
//    free((char *) Buffer);
//}

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

static void drawBitmap(JNIEnv *env, jobject bitmap, GifRowType *ScreenBuffer) {
    //
    AndroidBitmapInfo bitmapInfo;
    void *pixels;
    AndroidBitmap_getInfo(env, bitmap, &bitmapInfo);
    AndroidBitmap_lockPixels(env, bitmap, &pixels);

    AndroidBitmap_unlockPixels(env, bitmap);
}

static int GIF2RGB(JNIEnv *env, jobject bitmap, jobject runnable) {
    int i, j, Size, Row, Col, Width, Height, ExtCode, Count;
    GifRecordType RecordType;
    GifByteType *Extension;
    GifRowType *ScreenBuffer;
    int InterlacedOffset[] = {0, 4, 2, 1}; /* The way Interlaced image should. */
    int InterlacedJumps[] = {8, 8, 4, 2};    /* be read - offsets and jumps... */
    int ImageNum = 0;
    ColorMapObject *ColorMap;
    int Error;
    int delayTime;
    GifByteType *GifExtension;

    jclass runClass = env->GetObjectClass(runnable);
    jmethodID runMethod = env->GetMethodID(runClass, "run", "()V");

    /*
     * Allocate the screen as vector of column of rows. Note this
     * screen is device independent - it's the screen defined by the
     * GIF file parameters.
     */
    if ((ScreenBuffer = (GifRowType *)
            malloc(GifFile->SHeight * sizeof(GifRowType))) == NULL)
        LOGI("Failed to allocate memory required, aborted.");

    Size = GifFile->SWidth * sizeof(GifPixelType);/* Size in bytes one row.*/
    if ((ScreenBuffer[0] = (GifRowType) malloc(Size)) == NULL) /* First row. */
    {
        LOGE("Failed to allocate memory required, aborted.");
        return -3;
    }

    for (i = 0; i < GifFile->SWidth; i++)  /* Set its color to BackGround. */
        ScreenBuffer[0][i] = GifFile->SBackGroundColor;
    for (i = 1; i < GifFile->SHeight; i++) {
        /* Allocate the other rows, and set their color to background too: */
        if ((ScreenBuffer[i] = (GifRowType) malloc(Size)) == NULL) {
            LOGI("Failed to allocate memory required, aborted.");
            return -4;
        }

        memcpy(ScreenBuffer[i], ScreenBuffer[0], Size);
    }
    drawBitmap(env, bitmap, ScreenBuffer);
    env->CallVoidMethod(runnable, runMethod);
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
                Row = GifFile->Image.Top; /* Image Position relative to Screen. */
                Col = GifFile->Image.Left;
                Width = GifFile->Image.Width;
                Height = GifFile->Image.Height;
                LOGI("GifFile Image %d at (%d, %d) [%dx%d]", ++ImageNum, GifFile->Image.Left,
                     GifFile->Image.Width, Width, Height);
                if (GifFile->Image.Left + GifFile->Image.Width > GifFile->SWidth ||
                    GifFile->Image.Top + GifFile->Image.Height > GifFile->SHeight) {
                    LOGI("Image %d is not confined to screen dimension, aborted.\n",
                         ImageNum);
                    return -7;
                }
                if (GifFile->Image.Interlace) {
                    /* Need to perform 4 passes on the images: */
                    for (Count = i = 0; i < 4; i++)
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
                drawBitmap(env, bitmap, ScreenBuffer);
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

    /* Lets dump it - set the global variables required and do it: */
    ColorMap = (GifFile->Image.ColorMap
                ? GifFile->Image.ColorMap
                : GifFile->SColorMap);
    if (ColorMap == NULL) {
        fprintf(stderr, "Gif Image does not have a colormap\n");
        return -12;
    }

    free(ScreenBuffer);

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