package com.smartglasses.companion;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Bundle;
import android.provider.MediaStore;
import android.provider.Settings;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * Main Control Dashboard for the Upgraded Smart Glasses Android App.
 */
public class MainActivity extends AppCompatActivity {

    private TextView tvConnectionStatus;
    private EditText etPiIpAddress;
    private EditText etCustomMessage;
    private CheckBox cbMirrorX;
    private CheckBox cbMirrorY;
    private CheckBox cbTtsEnabled;

    private final GlassesConnectionManager conn = GlassesConnectionManager.getInstance();

    private final ActivityResultLauncher<Intent> imagePickerLauncher =
            registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == RESULT_OK && result.getData() != null) {
                    Uri uri = result.getData().getData();
                    if (uri != null) {
                        projectUriImage(uri);
                    }
                }
            });

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvConnectionStatus = findViewById(R.id.tvConnectionStatus);
        etPiIpAddress = findViewById(R.id.etPiIpAddress);
        etCustomMessage = findViewById(R.id.etCustomMessage);
        cbMirrorX = findViewById(R.id.cbMirrorX);
        cbMirrorY = findViewById(R.id.cbMirrorY);
        cbTtsEnabled = findViewById(R.id.cbTtsEnabled);

        conn.setListener((isConnected, statusText) -> {
            tvConnectionStatus.setText("Status: " + statusText);
            tvConnectionStatus.setBackgroundColor(isConnected ? 0xFF1B5E20 : 0xFF263238);
            if (isConnected) {
                syncSystemStatusToGlasses();
            }
        });

        setupButtons();
        requestRuntimePermissions();
    }

    private void setupButtons() {
        Button btnConnectBt = findViewById(R.id.btnConnectBluetooth);
        btnConnectBt.setOnClickListener(v -> showPairedBluetoothDialog());

        Button btnEnableNotif = findViewById(R.id.btnEnableNotifAccess);
        btnEnableNotif.setOnClickListener(v -> {
            Intent intent = new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS);
            startActivity(intent);
        });

        Button btnConnectWifi = findViewById(R.id.btnConnectWifi);
        btnConnectWifi.setOnClickListener(v -> {
            String ip = etPiIpAddress.getText().toString().trim();
            if (ip.isEmpty()) {
                Toast.makeText(this, "Enter Raspberry Pi IP address", Toast.LENGTH_SHORT).show();
                return;
            }
            conn.connectWifi(ip, 8888);
        });

        // HUD Screen Mode Buttons
        findViewById(R.id.btnModeDashboard).setOnClickListener(v -> conn.sendPacket("MODE|DASHBOARD"));
        findViewById(R.id.btnModeNotif).setOnClickListener(v -> conn.sendPacket("MODE|NOTIF"));
        findViewById(R.id.btnModeNav).setOnClickListener(v -> conn.sendPacket("MODE|NAV"));
        findViewById(R.id.btnModeMusic).setOnClickListener(v -> conn.sendPacket("MODE|MUSIC"));

        // Optics Combiner Mirror & TTS Toggles
        cbMirrorX.setOnCheckedChangeListener((b, checked) ->
                conn.sendMirrorConfig(cbMirrorX.isChecked(), cbMirrorY.isChecked()));
        cbMirrorY.setOnCheckedChangeListener((b, checked) ->
                conn.sendMirrorConfig(cbMirrorX.isChecked(), cbMirrorY.isChecked()));
        cbTtsEnabled.setOnCheckedChangeListener((b, checked) ->
                conn.sendPacket("TTS|" + (checked ? "ON" : "OFF")));

        // Turn-by-Turn Navigation Demo / Quick Buttons
        findViewById(R.id.btnNavLeft).setOnClickListener(v ->
                conn.sendNavigation("LEFT", "150 m", "Turn left on Oak Ave", "4 min"));
        findViewById(R.id.btnNavStraight).setOnClickListener(v ->
                conn.sendNavigation("STRAIGHT", "800 m", "Continue on Main St", "3 min"));
        findViewById(R.id.btnNavRight).setOnClickListener(v ->
                conn.sendNavigation("RIGHT", "60 m", "Turn right on Park Rd", "1 min"));

        // Pro Peripherals: POV Camera Snapshot & Hand Gesture Controls
        findViewById(R.id.btnSnapPovCamera).setOnClickListener(v ->
                conn.sendPacket("CAM|SNAP|/tmp/glasses_pov.jpg"));
        findViewById(R.id.btnGestureLeft).setOnClickListener(v ->
                conn.sendPacket("GESTURE|LEFT"));
        findViewById(R.id.btnGestureRight).setOnClickListener(v ->
                conn.sendPacket("GESTURE|RIGHT"));

        // Image / Bitmap Projection
        findViewById(R.id.btnProjectPhoto).setOnClickListener(v -> {
            Intent pick = new Intent(Intent.ACTION_PICK, MediaStore.Images.Media.EXTERNAL_CONTENT_URI);
            imagePickerLauncher.launch(pick);
        });

        // System Clock & Battery Sync
        findViewById(R.id.btnSyncSystem).setOnClickListener(v -> syncSystemStatusToGlasses());

        // Custom Text & WM8960 TTS Voice Readout
        findViewById(R.id.btnSendText).setOnClickListener(v -> {
            String msg = etCustomMessage.getText().toString().trim();
            if (!msg.isEmpty()) {
                conn.sendNotification("MESSAGE", "Phone Note", msg);
            }
        });

        findViewById(R.id.btnSpeakTts).setOnClickListener(v -> {
            String msg = etCustomMessage.getText().toString().trim();
            if (!msg.isEmpty()) {
                conn.sendTts(msg);
            }
        });

        findViewById(R.id.btnClearHud).setOnClickListener(v -> conn.sendPacket("clear"));
    }

    @SuppressLint("MissingPermission")
    private void showPairedBluetoothDialog() {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter == null || !adapter.isEnabled()) {
            Toast.makeText(this, "Please enable Bluetooth first", Toast.LENGTH_SHORT).show();
            return;
        }

        Set<BluetoothDevice> paired = adapter.getBondedDevices();
        if (paired == null || paired.isEmpty()) {
            Toast.makeText(this, "No paired Bluetooth devices found. Pair with Raspberry Pi first.", Toast.LENGTH_LONG).show();
            return;
        }

        List<BluetoothDevice> deviceList = new ArrayList<>(paired);
        String[] names = new String[deviceList.size()];
        for (int i = 0; i < deviceList.size(); i++) {
            names[i] = deviceList.get(i).getName() + " (" + deviceList.get(i).getAddress() + ")";
        }

        new AlertDialog.Builder(this)
                .setTitle("Select Raspberry Pi Zero W")
                .setItems(names, (dialog, which) -> conn.connectBluetooth(deviceList.get(which)))
                .show();
    }

    private void syncSystemStatusToGlasses() {
        String timeStr = new SimpleDateFormat("HH:mm", Locale.getDefault()).format(new Date());
        String dateStr = new SimpleDateFormat("EEE MMM d", Locale.getDefault()).format(new Date());
        int batteryPct = getPhoneBatteryPercentage();
        conn.sendSystemSync(timeStr, dateStr, batteryPct, "24C Clear");
    }

    private int getPhoneBatteryPercentage() {
        IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent batteryStatus = registerReceiver(null, ifilter);
        if (batteryStatus == null) return 100;
        int level = batteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int scale = batteryStatus.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (level < 0 || scale <= 0) return 100;
        return (int) ((level * 100f) / scale);
    }

    private void projectUriImage(Uri uri) {
        try {
            Bitmap bmp = MediaStore.Images.Media.getBitmap(getContentResolver(), uri);
            String hex2048 = BitmapProjector.bitmapToMonochromeHex(bmp);
            if (hex2048 != null) {
                conn.sendBitmapHex(hex2048);
                Toast.makeText(this, "Projected 128x64 dithered image to Smart Glasses!", Toast.LENGTH_SHORT).show();
            }
        } catch (IOException e) {
            Toast.makeText(this, "Failed to load image: " + e.getMessage(), Toast.LENGTH_SHORT).show();
        }
    }

    private void requestRuntimePermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                    != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.BLUETOOTH_SCAN}, 101);
            }
        }
    }
}
