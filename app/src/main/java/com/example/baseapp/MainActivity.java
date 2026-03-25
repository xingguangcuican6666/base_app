package com.example.baseapp;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import java.util.List;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        TextView tvResult = findViewById(R.id.tv_result);
        Handler mainHandler = new Handler(Looper.getMainLooper());

        Executors.newSingleThreadExecutor().execute(() -> {
            PackageManager pm = getPackageManager();
            List<ApplicationInfo> allApps = pm.getInstalledApplications(PackageManager.GET_META_DATA);

            int systemAppCount = 0;
            int userAppCount = 0;

            for (ApplicationInfo appInfo : allApps) {
                if ((appInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0) {
                    systemAppCount++;
                } else {
                    userAppCount++;
                }
            }

            final String result = "应用列表检测结果\n\n"
                    + "系统应用数量：" + systemAppCount + "\n"
                    + "用户应用数量：" + userAppCount + "\n"
                    + "总计：" + allApps.size() + " 个应用";

            mainHandler.post(() -> tvResult.setText(result));
        });
    }
}
