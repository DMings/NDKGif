package com.dming.testgif;

import android.content.Context;
import android.content.res.AssetManager;
import android.opengl.GLES20;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class GifPlayer {

    public enum PlayState {
        IDLE, PREPARE, PLAYING, STOP
    }

    public interface OnGifListener {
        void start();

        void update();

        void end();
    }

    private int mTexture;
    private OnGifListener mOnGifListener;
    private HandlerThread mHandlerThread;
    private Handler mHandler;
    private PlayState mPlayState;
    private long mGifPlayerPtr = 0;
    private EglHelper mEglHelper;
    private Surface mSurface;
    private NoFilter mNoFilter;
    private int mWidth, mHeight;

    public GifPlayer(final SurfaceView surfaceView) {
        mEglHelper = new EglHelper();
        mGifPlayerPtr = native_create();
        mPlayState = PlayState.IDLE;
        mHandlerThread = new HandlerThread("GifPlayer");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(final SurfaceHolder holder) {
                mSurface = holder.getSurface();
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        if (!mEglHelper.isGLCreate()) {
                            mEglHelper.initEgl(null, mSurface);
                            mEglHelper.glBindThread();
                            mNoFilter = new NoFilter(surfaceView.getContext());
                            GLES20.glClearColor(1, 1, 1, 1);
                            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
                        }
                    }
                });
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                mWidth = width;
                mHeight = height;
                mSurface = holder.getSurface();
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        mNoFilter.onDestroy();
                        mEglHelper.destroyEgl();
                    }
                });
            }
        });
    }

    public void setOnGifListener(OnGifListener onGifListener) {
        this.mOnGifListener = onGifListener;
    }

    public boolean assetPlay(Context context, String gifPath) {
        return play(false, context, gifPath);
    }

    public boolean storagePlay(String gifPath) {
        return play(false, null, gifPath);
    }

    public boolean assetPlay(boolean once, Context context, String gifPath) {
        return play(once, context, gifPath);
    }

    public boolean storagePlay(boolean once, String gifPath) {
        return play(once, null, gifPath);
    }

    private boolean play(final boolean once, final Context context, final String gifPath) {
        if (mPlayState == PlayState.IDLE && mSurface != null) {
            mPlayState = PlayState.PREPARE;
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    if (mOnGifListener != null) {
                        mOnGifListener.start();
                    }
                    if (native_load(mGifPlayerPtr, context != null ? context.getResources().getAssets() : null, gifPath)) {
                        mPlayState = PlayState.PLAYING;
                        mTexture = FGLUtils.createTexture();
                        native_start(mGifPlayerPtr, once, mTexture, new Runnable() {
                            @Override
                            public void run() {
                                FGLUtils.glCheckErr("test");
                                mNoFilter.onDraw(mTexture, 0, 0, mWidth, mHeight);
                                FGLUtils.glCheckErr("test down");
                                mEglHelper.swapBuffers();
                                if (mOnGifListener != null) {
                                    mOnGifListener.update();
                                }
                            }
                        });
                    }
                    mPlayState = PlayState.IDLE;
                    if (mOnGifListener != null) {
                        mOnGifListener.end();
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
            native_pause(mGifPlayerPtr);
            return true;
        }
        return false;
    }

    public boolean resume() {
        if (mPlayState == PlayState.PLAYING) {
            native_resume(mGifPlayerPtr);
            return true;
        }
        return false;
    }

    public boolean stop() {
        if (mPlayState != PlayState.IDLE) {
            mPlayState = PlayState.STOP;
            native_stop(mGifPlayerPtr);
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
        native_stop(mGifPlayerPtr);
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                native_release(mGifPlayerPtr);
                mGifPlayerPtr = 0;
                mHandler = null;
                mHandlerThread.quit();
                mHandlerThread = null;
            }
        });
    }

    static {
        System.loadLibrary("gifplayer");
    }

    private native long native_create();

    private native boolean native_load(long ptr, AssetManager assetManager, String gifPath);

    private native void native_start(long ptr, boolean once, int texture, Runnable updateBitmap);

    private native int native_get_width(long ptr);

    private native int native_get_height(long ptr);

    private native void native_pause(long ptr);

    private native void native_resume(long ptr);

    private native void native_stop(long ptr);

    private native void native_release(long ptr);

}
