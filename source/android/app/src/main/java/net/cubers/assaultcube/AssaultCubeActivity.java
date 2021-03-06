package net.cubers.assaultcube;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.hardware.SensorEvent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.KeyEvent;

import com.google.android.play.core.review.ReviewInfo;
import com.google.android.play.core.review.ReviewManager;
import com.google.android.play.core.review.ReviewManagerFactory;
import com.google.android.play.core.tasks.Task;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.util.Calendar;

public class AssaultCubeActivity extends SDLActivity {

    int ASK_LIKE_APP_AFTER_INSTALLED_HOURS = 48;
    int ASK_LIKE_APP_AGAIN_AFTER_HOURS = 7*24;
    String LAST_APP_RATING = "LAST_APP_RATING";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // enable full brightness to improve visual experience
        this.getWindow().getAttributes().screenBrightness = 1.0f;

        // Unfortunately AssaultCube uses two different API's for file access: SDL_RW from libsdl2 and fopen from cstdio:
        // - libsdl2 targets the app's internal storage meaning the *readonly* APK data itself and this behavior is not easy to change.
        //   AssaultCube reads only few things using SDL_RW such as audio files and certain images. No write access is performed (!)
        // - cstdio can target any accessible path on the device the app has permissions to
        //   AssaultCube reads most of the things using fopen and it writes *ALL* things using fopen.
        //   Since we cannot easily get rid of the libsdl2 behavior our approach is to extract all data from the APK to the app's external
        //   directory which is writeable and then have all calls to stdio target this directory. We do this once on startup.
        AssetExporter assetExporter = new AssetExporter();
        assetExporter.copyAssets(this);
        super.onCreate(savedInstanceState);

        if(shouldTriggerReview()) {
            triggerReviewDelayed();
        }
    }

    private boolean shouldTriggerReview() {
        // game allows it
        boolean conditionGameAllows = AssaultCubeLib.allowaskrating();

        // after installed hours
        long installDate = getInstallDate();
        long now = Calendar.getInstance().getTimeInMillis();
        long installedSinceHours = (now - installDate) / (1000*60*60);
        boolean conditionAfterInstalledHoursSatisfied = (installedSinceHours >= ASK_LIKE_APP_AFTER_INSTALLED_HOURS);

        // again after hours
        SharedPreferences sharedPref = this.getPreferences(Context.MODE_PRIVATE);
        String lastAppRating = sharedPref.getString(LAST_APP_RATING, "");
        boolean askFirstTime = (lastAppRating == null || lastAppRating.equals(""));
        boolean askRepeatedTime =
                (lastAppRating != null
                        && !"".equals(lastAppRating)
                        && (now - Long.parseLong(lastAppRating)) / (1000*60*60) >= ASK_LIKE_APP_AGAIN_AFTER_HOURS);
        boolean conditionAgainAfterHoursSatisfied = (askFirstTime || askRepeatedTime);

        return conditionGameAllows && conditionAfterInstalledHoursSatisfied
                && conditionAgainAfterHoursSatisfied;
    }

    private static long getInstallDate() {
        long installDate;
        try {
            Context context = AssaultCubeApplication.getAppContext();
            installDate = context.getPackageManager().getPackageInfo(context.getPackageName(), 0).firstInstallTime;
        } catch (PackageManager.NameNotFoundException e) {
            installDate = Calendar.getInstance().getTimeInMillis();
        }
        return installDate;
    }

    private void triggerReviewDelayed() {
        final Handler handler = new Handler(Looper.getMainLooper());
        handler.postDelayed(this::triggerReview, 10000);
    }

    private void triggerReview() {
        ReviewManager manager = ReviewManagerFactory.create(this);
        Task<ReviewInfo> request = manager.requestReviewFlow();
        request.addOnCompleteListener(task -> {
            if (task.isSuccessful()) {
                ReviewInfo reviewInfo = task.getResult();
                Task<Void> flow = manager.launchReviewFlow(this, reviewInfo);
                flow.addOnCompleteListener(task2 -> {
                    // The flow has finished. The API does not indicate whether the user
                    // reviewed or not, or even whether the review dialog was shown. Thus, no
                    // matter the result, we continue our app flow.


                    SharedPreferences sharedPref = this.getPreferences(Context.MODE_PRIVATE);
                    SharedPreferences.Editor editor = sharedPref.edit();
                    editor.putString(LAST_APP_RATING, Long.toString(Calendar.getInstance().getTimeInMillis()));
                    editor.apply();
                });
            } else {
                // There was some problem, continue regardless of the result.
            }
        });
    }

    @Override
    protected void onPause() {
        superOnPause();
    }

    @Override
    protected void onStop() {
        superOnStop();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {

        if (SDLActivity.mBrokenLibraries) {
            return false;
        }

        int keyCode = event.getKeyCode();

        if(!AssaultCubeLib.hijackvolumekeys() && (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN ||
                keyCode == KeyEvent.KEYCODE_VOLUME_UP))
        {
            return false;
        }

        // Ignore certain special keys so they're handled by Android
        if (keyCode == KeyEvent.KEYCODE_CAMERA ||
                        keyCode == KeyEvent.KEYCODE_ZOOM_IN || /* API 11 */
                        keyCode == KeyEvent.KEYCODE_ZOOM_OUT /* API 11 */
        ) {
            return false;
        }

        return super.superDispatchKeyEvent(event);
    }


}

