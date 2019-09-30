package com.dming.testgif;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.os.Handler;
import android.os.HandlerThread;

public class GifPlayer {

    public enum PlayState {
        IDLE, PLAY, PLAYING, STOP
    }

    private Bitmap mBitmap;
    private UpdateBitmap mUpdateBitmap;
    private HandlerThread mHandlerThread;
    private Handler mHandler;
    private PlayState mPlayState;

    public GifPlayer() {
        mPlayState = PlayState.IDLE;
        mHandlerThread = new HandlerThread("GifPlayer");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    public void setUpdateBitmap(UpdateBitmap updateBitmap) {
        this.mUpdateBitmap = updateBitmap;

    }

    public boolean assetPlay(Context context, String gifPath) {
        return play(context, gifPath);
    }

    public boolean storagePlay(String gifPath) {
        return play(null, gifPath);
    }

    private boolean play(final Context context, final String gifPath) {
        if (mPlayState == PlayState.IDLE) {
            mPlayState = PlayState.PLAY;
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    if (native_load_gif(context != null ? context.getResources().getAssets() : null, gifPath)) {
                        mPlayState = PlayState.PLAYING;
                        mBitmap = Bitmap.createBitmap(native_get_width(), native_get_height(), Bitmap.Config.ARGB_8888);
                        native_start(mBitmap, new Runnable() {
                            @Override
                            public void run() {
                                if (mUpdateBitmap != null) {
                                    mUpdateBitmap.draw(mBitmap);
                                }
                            }
                        });
                    }
                }
            });
        } else {
            return false;
        }
        return true;
    }

    public boolean pause() {
        if (mPlayState == PlayState.PLAYING) {
            native_pause();
            return true;
        }
        return false;
    }

    public boolean resume() {
        if (mPlayState == PlayState.PLAYING) {
            native_resume();
            return true;
        }
        return false;
    }

    public boolean stop() {
        if (mPlayState != PlayState.IDLE &&
                mPlayState != PlayState.STOP) {
            mPlayState = PlayState.STOP;
            native_release();
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    mPlayState = PlayState.IDLE;
                }
            });
            return true;
        }
        return false;
    }

    public void destroy() {
        native_release();
        mHandlerThread.quit();
        try {
            mHandlerThread.join();
        } catch (InterruptedException e) {
//            e.printStackTrace();
        }
//        if(mBitmap != null){
//            mBitmap.recycle();
//        }
    }

    static {
        System.loadLibrary("gifplayer");
    }

    private static native boolean native_load_gif(AssetManager assetManager, String gifPath);

    private static native void native_start(Bitmap bitmap, Runnable updateBitmap);

    private static native int native_get_width();

    private static native int native_get_height();

    private static native void native_pause();

    private static native void native_resume();

    private static native void native_release();

}
