package com.example.baseapp;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "BaseApp";

    // 声明 native 方法，由 native-lib.c 中实现
    public native String stringFromJNI();

    static {
        // 应用启动时加载 so 文件
        System.loadLibrary("native-lib");
        Log.i(TAG, "native-lib.so loaded successfully");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Log.i(TAG, "onCreate: calling native stringFromJNI()");

        // 调用 native 方法并显示返回值
        String result = stringFromJNI();
        Log.i(TAG, "onCreate: native stringFromJNI() returned: " + result);

        TextView tv = findViewById(R.id.tv_result);
        tv.setText(result);
    }
}
