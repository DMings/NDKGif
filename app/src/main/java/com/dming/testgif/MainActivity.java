package com.dming.testgif;

import android.Manifest;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.ImageView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    //    private GifHandler mGifHandler;
    private ImageView mIvShow;
    private Bitmap mBitmap;
    private TestGif mTestGif;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE}, 666);
        }
        mIvShow = findViewById(R.id.iv_show);
        mTestGif = new TestGif();
        findViewById(R.id.btn_one).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                File file = new File(Environment.getExternalStorageDirectory(), "1/test.gif");
                if (file.exists()) {
                    if (mTestGif.loadGif(file.getPath())) {
                        mBitmap = Bitmap.createBitmap(mTestGif.getWidth(), mTestGif.getHeight(), Bitmap.Config.ARGB_8888);
                        testGif();
                    }
                } else {
                    Toast.makeText(MainActivity.this, "file not exists!!!", Toast.LENGTH_SHORT).show();
                }
            }
        });
//        findViewById(R.id.btn_two).setOnClickListener(new View.OnClickListener() {
//            @Override
//            public void onClick(View v) {
//                File file = new File(Environment.getExternalStorageDirectory(), "1/demo.gif");
//                if (file.exists()) {
//                    testGif();
//                } else {
//                    Toast.makeText(MainActivity.this, "file not exists!!!", Toast.LENGTH_SHORT).show();
//                }
//            }
//        });
    }

    public void testGif() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                mTestGif.testGif(mBitmap, new Runnable() {
                    @Override
                    public void run() {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                Log.i("DMUI", "testGif>>> run");
                                mIvShow.setImageBitmap(mBitmap);
                            }
                        });
                    }
                });
            }
        }).start();
    }

}
