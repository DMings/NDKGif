package com.dming.testgif;

import android.Manifest;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.ImageView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    private ImageView mIvShow;
    private GifPlayer mGifPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE}, 666);
        }
        mIvShow = findViewById(R.id.iv_show);
        mGifPlayer = new GifPlayer();
        mGifPlayer.setUpdateBitmap(new UpdateBitmap() {
            @Override
            public void draw(final Bitmap bitmap) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mIvShow.setImageBitmap(bitmap);
                    }
                });
            }
        });
        findViewById(R.id.btn_one).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                File file = new File(Environment.getExternalStorageDirectory(), "1/demo.gif");
                if (file.exists()) {
                    mGifPlayer.play(file.getPath());
                } else {
                    Toast.makeText(MainActivity.this, "file not exists!!!", Toast.LENGTH_SHORT).show();
                }
            }
        });
        findViewById(R.id.btn_two).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mGifPlayer.onPause();
            }
        });
        findViewById(R.id.btn_three).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mGifPlayer.onResume();
            }
        });
        findViewById(R.id.btn_four).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mGifPlayer.onDestroy();
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        mGifPlayer.onResume();
    }

    @Override
    protected void onPause() {
        mGifPlayer.onPause();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        mGifPlayer.onDestroy();
        super.onDestroy();
    }
}
