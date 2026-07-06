package com.sudoevolve.euineo;

import android.os.Bundle;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;

public class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {
                "SDL2",
                "main"
        };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        copyBundledAssets();
        super.onCreate(savedInstanceState);
    }

    private void copyBundledAssets() {
        try {
            copyAssetTree("", getFilesDir());
        } catch (IOException ignored) {
        }
    }

    public float displayDensity() {
        return getResources().getDisplayMetrics().density;
    }

    public static byte[] renderEmojiBitmap(String text, float textSize) {
        if (text == null || text.isEmpty() || textSize <= 0.0f) {
            return null;
        }

        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
        paint.setColor(Color.WHITE);
        paint.setTextSize(textSize);
        paint.setTypeface(Typeface.DEFAULT);
        if (!paint.hasGlyph(text)) {
            return null;
        }

        Paint.FontMetricsInt metrics = paint.getFontMetricsInt();
        float advance = Math.max(1.0f, paint.measureText(text));
        int padding = Math.max(2, (int)Math.ceil(textSize * 0.10f));
        int width = Math.max(1, (int)Math.ceil(advance) + padding * 2);
        int height = Math.max(1, metrics.descent - metrics.ascent + padding * 2);
        int baseline = padding - metrics.ascent;

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.TRANSPARENT);
        canvas.drawText(text, padding, baseline, paint);

        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        bitmap.recycle();

        ByteBuffer output = ByteBuffer.allocate(20 + width * height * 4).order(ByteOrder.LITTLE_ENDIAN);
        output.putInt(width);
        output.putInt(height);
        output.putInt(padding);
        output.putInt(baseline);
        output.putFloat(advance);
        for (int pixel : pixels) {
            output.put((byte)((pixel >> 16) & 0xFF));
            output.put((byte)((pixel >> 8) & 0xFF));
            output.put((byte)(pixel & 0xFF));
            output.put((byte)((pixel >> 24) & 0xFF));
        }
        return output.array();
    }

    private void copyAssetTree(String assetPath, File targetDir) throws IOException {
        String[] entries = getAssets().list(assetPath);
        if (entries == null || entries.length == 0) {
            copyAssetFile(assetPath, new File(targetDir, assetPath));
            return;
        }

        File currentTarget = assetPath.isEmpty() ? targetDir : new File(targetDir, assetPath);
        if (!currentTarget.exists() && !currentTarget.mkdirs()) {
            return;
        }

        for (String entry : entries) {
            String childPath = assetPath.isEmpty() ? entry : assetPath + "/" + entry;
            copyAssetTree(childPath, targetDir);
        }
    }

    private void copyAssetFile(String assetPath, File targetFile) throws IOException {
        if (assetPath.isEmpty()) {
            return;
        }
        File parent = targetFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            return;
        }
        try (InputStream input = getAssets().open(assetPath)) {
            if (targetFile.exists() && targetFile.length() == input.available()) {
                return;
            }
        }
        try (InputStream input = getAssets().open(assetPath);
             OutputStream output = new FileOutputStream(targetFile, false)) {
            byte[] buffer = new byte[16 * 1024];
            int read;
            while ((read = input.read(buffer)) >= 0) {
                output.write(buffer, 0, read);
            }
        }
    }

    public static String downloadUrlToString(String url) {
        HttpURLConnection connection = null;
        try {
            connection = (HttpURLConnection) new URL(url).openConnection();
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(15000);
            connection.setInstanceFollowRedirects(true);
            connection.setRequestProperty("User-Agent", "EUI-NEO Android Gallery");
            int status = connection.getResponseCode();
            if (status < 200 || status >= 300) {
                return null;
            }
            try (InputStream input = connection.getInputStream()) {
                byte[] buffer = readAllBytes(input);
                return new String(buffer, StandardCharsets.UTF_8);
            }
        } catch (IOException ignored) {
            return null;
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    public static boolean downloadUrlToFile(String url, String targetPath) {
        HttpURLConnection connection = null;
        File tempFile = new File(targetPath + ".download");
        try {
            File targetFile = new File(targetPath);
            File parent = targetFile.getParentFile();
            if (parent != null && !parent.exists() && !parent.mkdirs()) {
                return false;
            }
            connection = (HttpURLConnection) new URL(url).openConnection();
            connection.setConnectTimeout(20000);
            connection.setReadTimeout(20000);
            connection.setInstanceFollowRedirects(true);
            connection.setRequestProperty("User-Agent", "EUI-NEO Android Gallery");
            int status = connection.getResponseCode();
            if (status < 200 || status >= 300) {
                return false;
            }
            try (InputStream input = connection.getInputStream();
                 OutputStream output = new FileOutputStream(tempFile, false)) {
                byte[] buffer = new byte[32 * 1024];
                int read;
                while ((read = input.read(buffer)) >= 0) {
                    output.write(buffer, 0, read);
                }
            }
            if (targetFile.exists() && !targetFile.delete()) {
                return false;
            }
            return tempFile.renameTo(targetFile);
        } catch (IOException ignored) {
            return false;
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
            if (tempFile.exists()) {
                //noinspection ResultOfMethodCallIgnored
                tempFile.delete();
            }
        }
    }

    private static byte[] readAllBytes(InputStream input) throws IOException {
        java.io.ByteArrayOutputStream output = new java.io.ByteArrayOutputStream();
        byte[] buffer = new byte[16 * 1024];
        int read;
        while ((read = input.read(buffer)) >= 0) {
            output.write(buffer, 0, read);
        }
        return output.toByteArray();
    }
}
