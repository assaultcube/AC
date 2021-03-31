package net.cubers.assaultcube;

import android.app.Activity;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;

/**
 * This activity is being launched on startup.
 * We need this activity because we want to show a progress text while we perform long running operations prior to launching the game.
 */
public class LaunchActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_launch);
    }

    @Override
    protected void onResume() {
        super.onResume();

        // Unfortunately AssaultCube uses two different API's for file access: SDL_RW from libsdl2 and fopen from cstdio:
        // - libsdl2 targets the app's internal storage meaning the *readonly* APK data itself and this behavior is not easy to change.
        //   AssaultCube reads only few things using SDL_RW such as audio files and certain images. No write access is performed (!)
        // - cstdio can target any accessible path on the device the app has permissions to
        //   AssaultCube reads most of the things using fopen and it writes *ALL* things using fopen.
        //   Since we cannot easily get rid of the libsdl2 behavior our approach is to extract all data from the APK to the app's external
        //   directory which is writeable and then have all calls to stdio target this directory. We do this once on startup.
        AssetExporter assetExporter = new AssetExporter();
        boolean copyAssetsRequired = assetExporter.isAssetExportRequired(this);
        long showProgressMinimumDurationMilliseconds = copyAssetsRequired ? 1000 : 0;

        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            // exports assets if needed
            if(copyAssetsRequired)
                assetExporter.copyAssets(this);

            // launch game
            Intent intent = new Intent(this, AssaultCubeActivity.class);
            startActivity(intent);
            finish();
        }, showProgressMinimumDurationMilliseconds);
    }
}