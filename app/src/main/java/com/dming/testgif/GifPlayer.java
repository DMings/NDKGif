package com.dming.testgif;

import android.graphics.Bitmap;
import android.os.Handler;
import android.os.HandlerThread;

public class GifPlayer {

    private Bitmap mBitmap;
    private UpdateBitmap mUpdateBitmap;
    private HandlerThread mHandlerThread;
    private Handler mHandler;

    public GifPlayer() {
        mHandlerThread = new HandlerThread("GifPlayer");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    public void setUpdateBitmap(UpdateBitmap updateBitmap) {
        this.mUpdateBitmap = updateBitmap;
    }

    public void play(final String gifPath) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (load_gif(gifPath)) {
                    mBitmap = Bitmap.createBitmap(get_width(), get_height(), Bitmap.Config.ARGB_8888);
                    start(mBitmap, new Runnable() {
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
    }

    public void onPause() {
        pause();
    }

    public void onResume() {
        resume();
    }

    public void stop() {
        release();
    }

    public void onDestroy() {
        release();
        try {
            mHandlerThread.join();
        } catch (InterruptedException e) {
//            e.printStackTrace();
        }
    }

    static {
        System.loadLibrary("gifplayer");
    }

    private static native boolean load_gif(String gifPath);

    private static native void start(Bitmap bitmap, Runnable updateBitmap);

    private static native int get_width();

    private static native int get_height();

    private static native void pause();

    private static native void resume();

    private static native void release();

}
