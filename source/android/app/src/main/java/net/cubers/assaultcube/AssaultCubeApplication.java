package net.cubers.assaultcube;

import android.app.Application;
import android.content.Context;

public class AssaultCubeApplication extends Application {
    private static Context context;

    public void onCreate() {
        super.onCreate();
        AssaultCubeApplication.context = getApplicationContext();
    }

    public static Context getAppContext() {
        return AssaultCubeApplication.context;
    }
}
