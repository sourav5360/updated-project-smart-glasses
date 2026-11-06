package com.smartglasses.companion;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.IOException;
import java.io.OutputStream;
import java.lang.reflect.Method;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Singleton Connection Manager for communicating with Raspberry Pi Zero W Smart Glasses.
 * Supports both Bluetooth Classic RFCOMM (Channel 1 / SPP UUID) and Wi-Fi TCP (Port 8888).
 */
public class GlassesConnectionManager {
    private static final String TAG = "GlassesConnMgr";
    private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

    private static GlassesConnectionManager instance;
    private final ExecutorService ioExecutor = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    private BluetoothSocket btSocket;
    private Socket tcpSocket;
    private OutputStream outputStream;
    private volatile boolean connected = false;
    private String connectionLabel = "Disconnected";
    private ConnectionListener listener;

    public interface ConnectionListener {
        void onConnectionStateChanged(boolean isConnected, String statusText);
    }

    private GlassesConnectionManager() {}

    public static synchronized GlassesConnectionManager getInstance() {
        if (instance == null) {
            instance = new GlassesConnectionManager();
        }
        return instance;
    }

    public void setListener(ConnectionListener listener) {
        this.listener = listener;
    }

    public boolean isConnected() {
        return connected;
    }

    public String getConnectionLabel() {
        return connectionLabel;
    }

    private void notifyState(boolean state, String text) {
        this.connected = state;
        this.connectionLabel = text;
        if (listener != null) {
            mainHandler.post(() -> listener.onConnectionStateChanged(state, text));
        }
    }

    /**
     * Connect to Raspberry Pi Zero W over Bluetooth RFCOMM Channel 1.
     */
    @SuppressLint("MissingPermission")
    public void connectBluetooth(BluetoothDevice device) {
        ioExecutor.execute(() -> {
            disconnectInternal();
            notifyState(false, "Connecting Bluetooth to " + device.getName() + "...");
            try {
                try {
                    // Direct RFCOMM Channel 1 socket (matches loc_addr.rc_channel = 1 in raspberry.c)
                    Method m = device.getClass().getMethod("createRfcommSocket", int.class);
                    btSocket = (BluetoothSocket) m.invoke(device, 1);
                } catch (Exception e) {
                    btSocket = device.createRfcommSocketToServiceRecord(SPP_UUID);
                }
                btSocket.connect();
                outputStream = btSocket.getOutputStream();
                notifyState(true, "Connected (Bluetooth: " + device.getName() + ")");
            } catch (Exception e) {
                Log.e(TAG, "Bluetooth connection failed", e);
                disconnectInternal();
                notifyState(false, "BT Error: " + e.getMessage());
            }
        });
    }

    /**
     * Connect to Raspberry Pi Zero W over Wi-Fi TCP (default port 8888).
     */
    public void connectWifi(String host, int port) {
        ioExecutor.execute(() -> {
            disconnectInternal();
            notifyState(false, "Connecting Wi-Fi to " + host + ":" + port + "...");
            try {
                tcpSocket = new Socket();
                tcpSocket.connect(new InetSocketAddress(host, port), 4000);
                outputStream = tcpSocket.getOutputStream();
                notifyState(true, "Connected (Wi-Fi: " + host + ":" + port + ")");
            } catch (Exception e) {
                Log.e(TAG, "Wi-Fi connection failed", e);
                disconnectInternal();
                notifyState(false, "Wi-Fi Error: " + e.getMessage());
            }
        });
    }

    public void disconnect() {
        ioExecutor.execute(() -> {
            disconnectInternal();
            notifyState(false, "Disconnected");
        });
    }

    private void disconnectInternal() {
        connected = false;
        try {
            if (outputStream != null) outputStream.close();
        } catch (IOException ignored) {}
        try {
            if (btSocket != null) btSocket.close();
        } catch (IOException ignored) {}
        try {
            if (tcpSocket != null) tcpSocket.close();
        } catch (IOException ignored) {}
        outputStream = null;
        btSocket = null;
        tcpSocket = null;
    }

    /**
     * Send a structured newline-delimited command packet to the Smart Glasses.
     */
    public void sendPacket(String packet) {
        if (packet == null) return;
        ioExecutor.execute(() -> {
            if (!connected || outputStream == null) return;
            try {
                String clean = packet.replace("\n", " ").replace("\r", "") + "\n";
                outputStream.write(clean.getBytes(StandardCharsets.UTF_8));
                outputStream.flush();
            } catch (IOException e) {
                Log.e(TAG, "Send failed", e);
                disconnectInternal();
                notifyState(false, "Connection lost");
            }
        });
    }

    private static String sanitizeField(String s) {
        if (s == null) return "";
        return s.replace("|", "/").replace("\n", " ").replace("\r", "").trim();
    }

    // High-level Protocol Helpers
    public void sendSystemSync(String timeHHMM, String dateStr, int batteryPct, String weather) {
        sendPacket("SYS|" + sanitizeField(timeHHMM) + "|" + sanitizeField(dateStr) + "|"
                + batteryPct + "|" + sanitizeField(weather));
    }

    public void sendNotification(String appCategory, String title, String body) {
        sendPacket("NOTIF|" + sanitizeField(appCategory) + "|"
                + sanitizeField(title) + "|" + sanitizeField(body));
    }

    public void sendNavigation(String direction, String distance, String street, String eta) {
        sendPacket("NAV|" + sanitizeField(direction) + "|" + sanitizeField(distance) + "|"
                + sanitizeField(street) + "|" + sanitizeField(eta));
    }

    public void sendMusic(boolean playing, String title, String artist, int progressPct) {
        sendPacket("MUSIC|" + (playing ? "PLAY" : "PAUSE") + "|"
                + sanitizeField(title) + "|" + sanitizeField(artist) + "|" + progressPct);
    }

    public void sendCallState(String state, String callerName, String number) {
        sendPacket("CALL|" + sanitizeField(state) + "|"
                + sanitizeField(callerName) + "|" + sanitizeField(number));
    }

    public void sendTts(String speechText) {
        sendPacket("TTS|" + sanitizeField(speechText));
    }

    public void sendMirrorConfig(boolean flipX, boolean flipY) {
        sendPacket("MIRROR|" + (flipX ? "1" : "0") + "|" + (flipY ? "1" : "0"));
    }

    public void sendBrightness(int contrast0to255) {
        sendPacket("BRIGHTNESS|" + Math.max(0, Math.min(255, contrast0to255)));
    }

    public void sendBitmapHex(String hex2048) {
        if (hex2048 != null && hex2048.length() >= 2048) {
            sendPacket("BMP|" + hex2048);
        }
    }
}
