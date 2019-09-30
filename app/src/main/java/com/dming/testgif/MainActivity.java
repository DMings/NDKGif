package com.dming.testgif;

import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.ImageView;

public class MainActivity extends AppCompatActivity {

    private ImageView mIvShow;
    private GifPlayer mGifPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
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
                mGifPlayer.play(MainActivity.this,"test.gif");
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
