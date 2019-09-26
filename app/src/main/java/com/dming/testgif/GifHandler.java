package com.dming.testgif;

import android.graphics.Bitmap;

public class GifHandler {
    static {
        System.loadLibrary("native-lib");
    }

    private long gifAddr;

    public GifHandler(String path){
        gifAddr = loadGif(path);
    }

    public int getWidth(){
        return getWidth(gifAddr);
    }

    public int getHeight(){
        return getHeight(gifAddr);
    }

    public int updateFrame(Bitmap bitmap){
        return updateFrame(bitmap, gifAddr);
    }

    private native long loadGif(String path);

    private native int getWidth(long gifAddr);

    private native int getHeight(long gifAddr);

    private native int updateFrame(Bitmap bitmap, long gifAddr);

}
