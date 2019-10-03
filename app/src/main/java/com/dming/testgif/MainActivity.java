package com.dming.testgif;

import android.Manifest;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;

public class MainActivity extends AppCompatActivity {

    private SurfaceView mSvShow;
    private GifPlayer mGifPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE}, 666);
        }
        mSvShow = findViewById(R.id.sv_show);
        mGifPlayer = new GifPlayer(mSvShow);
        mGifPlayer.setOnGifListener(new GifPlayer.OnGifListener() {
            @Override
            public void start() {
                Log.i("DMUI", "gif start");
            }

            @Override
            public void update() {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        DLog.i("update!!!");
                    }
                });
            }

            @Override
            public void end() {
                Log.i("DMUI", "gif end");
            }
        });
        findViewById(R.id.btn_one).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mGifPlayer.assetPlay(false, MainActivity.this, "demo.gif");
//                // 外部目录play
//                File file = new File(Environment.getExternalStorageDirectory(), "1/test.gif");
//                if (file.exists()) {
//                    mGifPlayer.storagePlay(false,file.getPath());
//                } else {
//                    Toast.makeText(MainActivity.this, "file not exists!!!", Toast.LENGTH_SHORT).show();
//                }
            }
        });
        findViewById(R.id.btn_two).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mGifPlayer.pause();
            }
        });
        findViewById(R.id.btn_three).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mGifPlayer.resume();
            }
        });
        findViewById(R.id.btn_four).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mGifPlayer.stop();
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        mGifPlayer.resume();
    }

    @Override
    protected void onPause() {
        mGifPlayer.pause();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        mGifPlayer.destroy();
        super.onDestroy();
    }
}
