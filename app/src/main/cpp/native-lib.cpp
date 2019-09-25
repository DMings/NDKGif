#include <jni.h>
#include <string>
#include <malloc.h>
#include <string.h>
#include "log.h"
#include "giflib/gif_lib.h"

static void PrintGifError(int Error) {
    LOGI("PrintGifError: %s", GifErrorString(Error));
}

static int DumpScreen2RGB(ColorMapObject *ColorMap,
                          GifRowType *ScreenBuffer,
                          int ScreenWidth, int ScreenHeight) {
    int i, j;
    GifRowType GifRow;
    GifColorType *ColorMapEntry;
    unsigned char *Buffer, *BufferP;

    if ((Buffer = (unsigned char *) malloc(ScreenWidth * 3)) == NULL) {
        LOGE("Failed to allocate memory required, aborted.");
        return -1;
    }
    for (i = 0; i < ScreenHeight; i++) {
        GifRow = ScreenBuffer[i];
        LOGI("=->%-4d", ScreenHeight - i);
        for (j = 0, BufferP = Buffer; j < ScreenWidth; j++) {
            ColorMapEntry = &ColorMap->Colors[GifRow[j]];
            *BufferP++ = ColorMapEntry->Red;
            *BufferP++ = ColorMapEntry->Green;
            *BufferP++ = ColorMapEntry->Blue;
        }
//        if (fwrite(Buffer, ScreenWidth * 3, 1, rgbfp[0]) != 1) {
//            LOGE("Write to file(s) failed.");
//            return -2;
//        }
    }
    free((char *) Buffer);
}

static int GIF2RGB(const char *FileName) {
    int i, j, Size, Row, Col, Width, Height, ExtCode, Count;
    GifRecordType RecordType;
    GifByteType *Extension;
    GifRowType *ScreenBuffer;
    GifFileType *GifFile;
    int
            InterlacedOffset[] = {0, 4, 2, 1}, /* The way Interlaced image should. */
            InterlacedJumps[] = {8, 8, 4, 2};    /* be read - offsets and jumps... */
    int ImageNum = 0;
    ColorMapObject *ColorMap;
    int Error;

    if ((GifFile = DGifOpenFileName(FileName, &Error)) == NULL) {
        PrintGifError(Error);
        return -1;
    }

//    if (DGifSlurp(GifFile) == GIF_OK) {
    LOGI("gif SWidth: %d SHeight: %d", GifFile->SWidth, GifFile->SHeight);
    LOGI("gif ImageCount: %d", GifFile->ImageCount);
    LOGI("gif SColorResolution: %d", GifFile->SColorResolution);
    LOGI("gif SBackGroundColor: %d", GifFile->SBackGroundColor);
//    }

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
                LOGI("\n%s: Image %d at (%d, %d) [%dx%d]:     ",
                     "gif2rgb", ++ImageNum, Col, Row, Width, Height);
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
//                            LOGI("8=->%-4d", Count++);
                            if (DGifGetLine(GifFile, &ScreenBuffer[j][Col],
                                            Width) == GIF_ERROR) {
                                PrintGifError(GifFile->Error);
                                return -8;
                            }
                        }
                } else {
                    for (i = 0; i < Height; i++) {
//                        LOGI("9=->%-4d", i);
                        if (DGifGetLine(GifFile, &ScreenBuffer[Row++][Col],
                                        Width) == GIF_ERROR) {
                            PrintGifError(GifFile->Error);
                            return -9;
                        }
                    }
                }
                break;
            case EXTENSION_RECORD_TYPE:
                /* Skip any extension blocks in file: */
                if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR) {
                    PrintGifError(GifFile->Error);
                    return -10;
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

//    DumpScreen2RGB(ColorMap,
//                   ScreenBuffer,
//                   GifFile->SWidth, GifFile->SHeight);

    free(ScreenBuffer);

    if (DGifCloseFile(GifFile, &Error) == GIF_ERROR) {
        PrintGifError(Error);
        return -12;
    }
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dming_testgif_MainActivity_testGif(JNIEnv *env, jobject instance, jstring gifPath_) {
    const char *gifPath = env->GetStringUTFChars(gifPath_, 0);

    GIF2RGB(gifPath);
//    int err;
//    GifFileType *gifFileType = DGifOpenFileName(gifPath, &err);
//    LOGI("start GifErrorString: %s", GifErrorString(err));
//    if (DGifSlurp(gifFileType) == GIF_OK) {
//        LOGI("gif SWidth: %d SHeight: %d", gifFileType->SWidth, gifFileType->SHeight);
//        LOGI("gif ImageCount: %d", gifFileType->ImageCount);
//        LOGI("gif SColorResolution: %d", gifFileType->SColorResolution);
//        LOGI("gif SBackGroundColor: %d", gifFileType->SBackGroundColor);
//    }
//    DGifCloseFile(gifFileType, &err);
//    LOGI("end GifErrorString: %s", GifErrorString(err));
    env->ReleaseStringUTFChars(gifPath_, gifPath);
}