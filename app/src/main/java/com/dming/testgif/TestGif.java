package com.dming.testgif;

import android.graphics.Bitmap;

public class TestGif {

    static {
        System.loadLibrary("native-lib");
    }

    public native boolean loadGif(String gifPath);

    public native int getWidth();

    public native int getHeight();

    public native void testGif(Bitmap bitmap, Runnable runnable);
}
