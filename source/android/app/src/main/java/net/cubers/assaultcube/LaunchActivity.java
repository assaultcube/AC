package net.cubers.assaultcube;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Handler;
import android.os.Looper;
import android.preference.PreferenceManager;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.text.Html;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.TabStopSpan;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.concurrent.atomic.AtomicBoolean;

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
        showTerms();
    }

    @SuppressLint("ApplySharedPref")
    private void showTerms() {
        final SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
        boolean agreed = sharedPreferences.getBoolean(Constants.AGREEEMENTVERSION,false);
        if(agreed) {
            termsAccepted();
        } else {
            AlertDialog.Builder builder = new AlertDialog.Builder(this, R.style.AcAlertDialogTheme);
            builder.setTitle("Agreement");
            Spanned msg = Html.fromHtml("Do you agree to the <a href=\"" + Constants.TERMSLINK
                    + "\">Terms and Conditions</a> and to the <a href=\"" + Constants.PRIVACYLINK + "\">Privacy Policy?</a>");
            builder.setMessage(msg);
            builder.setCancelable(true);
            builder.setPositiveButton("Yes", (dialog, id) -> {
                dialog.cancel();
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putBoolean(Constants.AGREEEMENTVERSION, true);
                editor.commit();
                termsAccepted();
            });
            builder.setNegativeButton("No", (dialog, id) -> {
                dialog.cancel();
                finish();
            });
            AlertDialog dialog = builder.create();
            dialog.show();

            // make links clickable
            ((TextView)dialog.findViewById(android.R.id.message)).setMovementMethod(LinkMovementMethod.getInstance());
        }
    }

    private void termsAccepted() {
        AsyncTask.execute(() -> {
            exportAssets();
            updateFromMasterserver();
            new Handler(Looper.getMainLooper()).post(this::startGame);
        });
    }

    /**
     * Exports assets from APK into our writeable directory.
     * Unfortunately AssaultCube uses two different API's for file access: SDL_RW from libsdl2 and fopen from cstdio.
     * - libsdl2 targets the app's internal storage meaning the *readonly* APK data itself and this behavior is not easy to change.
     *   AssaultCube reads only few things using SDL_RW such as audio files and certain images. No write access is performed (!)
     * - cstdio can target any accessible path on the device the app has permissions to
     *   AssaultCube reads most of the things using fopen and it writes *ALL* things using fopen.
     *   Since we cannot easily get rid of the libsdl2 behavior our approach is to extract all data from the APK to the app's external
     *   directory which is writeable and then have all calls to stdio target this directory. We do this once on startup.
     */
    private void exportAssets() {
        AssetExporter assetExporter = new AssetExporter();
        boolean copyAssetsRequired = assetExporter.isAssetExportRequired(LaunchActivity.this);
        if(copyAssetsRequired) assetExporter.copyAssets(LaunchActivity.this);
    }

    /**
     * Cheap way to get the official serverlist once per app startup.
     * AC mobile currently supports official severs only and therefore a simple serverlist file on the web suffices - no server registration capabilities currently needed.
     * We do this in Java world because currently the AC masterserver client only supports ENET or HTTP but not HTTPS -
     * and we want HTTPS so that we can host the rather static serverlist on an arbitrary webserver.
     * Unlike AC "desktop" there is no way to register custom servers and so a static
     * This should be upgraded to a more robust solution.
     */
    private void updateFromMasterserver() {
        HttpURLConnection urlConnection = null;
        InputStream inputStream = null;
        FileOutputStream outputStream = null;
        try {
            File file = new File(LaunchActivity.this.getExternalFilesDir(null), Constants.SERVERLISTFILE);
            outputStream = new FileOutputStream(file);

            // write variables to the script
            // these are useful so that the serverlist can provide conditional actions depending on app version
            outputStream.write(("acos = \"android\"\n" ).getBytes(StandardCharsets.UTF_8));
            outputStream.write(("acbuild = " + BuildConfig.VERSION_CODE + "\n").getBytes(StandardCharsets.UTF_8));

            // write serverlist to the script
            try {
                URL url = new URL(Constants.SERVERLIST);
                urlConnection = (HttpURLConnection) url.openConnection();
                inputStream = new BufferedInputStream(urlConnection.getInputStream());
            } catch(Exception ex) {
                new Handler(Looper.getMainLooper()).post(this::showRetryUpdateFromMasterserver);
                return;
            }

            int read = 0;
            byte[] bytes = new byte[1024];
            while ((read = inputStream.read(bytes)) != -1) {
                outputStream.write(bytes, 0, read);
            }

            // clear vars
            outputStream.write(("\ndelalias acos\n").getBytes(StandardCharsets.UTF_8));
            outputStream.write(("delalias acbuild\n").getBytes(StandardCharsets.UTF_8));
        }
        catch (Exception e) {
            // ignore other errors and let the user run the game with old/stale serverlist
            e.printStackTrace();
        }
        finally {
            if(urlConnection != null) urlConnection.disconnect();
            if(inputStream != null) {
                try {
                    inputStream.close();
                } catch (IOException ignored) { }
            }
            if(outputStream != null) {
                try {
                    outputStream.close();
                } catch (IOException ignored) { }
            }
        }
    }

    /**
     * Let the user decide whether or not to retry the masterserver update
     */
    private void showRetryUpdateFromMasterserver() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this, R.style.AcAlertDialogTheme);
        builder.setTitle("Something went wrong");
        builder.setMessage("Could not update the list of servers from the internet.");
        builder.setCancelable(true);
        builder.setPositiveButton("Retry", (dialog, id) -> {
            AsyncTask.execute(this::updateFromMasterserver);
        });
        builder.setNegativeButton("Ignore", (dialog, id) -> {
            dialog.cancel();
            startGame();
        });
        AlertDialog dialog = builder.create();
        dialog.show();
    }

    private void startGame() {
        Intent intent = new Intent(LaunchActivity.this, AssaultCubeActivity.class);
        startActivity(intent);
        finish();
    }
}