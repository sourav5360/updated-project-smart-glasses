package com.smartglasses.companion;

import android.app.Notification;
import android.os.Bundle;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import java.util.Locale;

/**
 * Background NotificationListenerService that automatically captures notifications
 * from any app on the user's phone (WhatsApp, SMS, Gmail, Google Maps, Music players,
 * Phone Dialer) and projects them onto the Smart Glasses HUD.
 */
public class GlassesNotificationService extends NotificationListenerService {
    private static final String TAG = "GlassesNotifSvc";

    @Override
    public void onNotificationPosted(StatusBarNotification sbn) {
        if (sbn == null) return;
        GlassesConnectionManager conn = GlassesConnectionManager.getInstance();
        if (!conn.isConnected()) return;

        String pkg = sbn.getPackageName() != null ? sbn.getPackageName().toLowerCase(Locale.ROOT) : "";
        if (pkg.equals(getPackageName())) return; // Ignore our own companion app

        Notification notif = sbn.getNotification();
        if (notif == null || notif.extras == null) return;
        Bundle extras = notif.extras;

        CharSequence titleCs = extras.getCharSequence(Notification.EXTRA_TITLE);
        CharSequence textCs = extras.getCharSequence(Notification.EXTRA_TEXT);
        CharSequence subTextCs = extras.getCharSequence(Notification.EXTRA_SUB_TEXT);

        String title = titleCs != null ? titleCs.toString().trim() : "";
        String text = textCs != null ? textCs.toString().trim() : "";
        String subText = subTextCs != null ? subTextCs.toString().trim() : "";

        if (title.isEmpty() && text.isEmpty()) return;

        Log.d(TAG, "Notification from " + pkg + ": " + title + " | " + text);

        // 1. Google Maps Turn-by-Turn Navigation Detection
        if (pkg.contains("apps.maps") || pkg.contains("waze")) {
            handleMapsNavigation(conn, title, text, subText);
            return;
        }

        // 2. Music / Media Player Detection (Spotify, YouTube Music, Apple Music, System Media)
        if (pkg.contains("spotify") || pkg.contains("music") ||
             Notification.CATEGORY_TRANSPORT.equals(notif.category)) {
            conn.sendMusic(true, title.isEmpty() ? "Playing Track" : title,
                    text.isEmpty() ? "Media Player" : text, 50);
            return;
        }

        // 3. Incoming Phone Call / VoIP Call Detection
        if (Notification.CATEGORY_CALL.equals(notif.category) || pkg.contains("dialer") || pkg.contains("incallui")) {
            conn.sendCallState("RINGING", title.isEmpty() ? "Incoming Call" : title, text);
            return;
        }

        // 4. General App Notification Categorization
        String appCategory = classifyPackage(pkg);
        conn.sendNotification(appCategory, title, text);
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn) {
        if (sbn == null || sbn.getNotification() == null) return;
        if (Notification.CATEGORY_CALL.equals(sbn.getNotification().category)) {
            GlassesConnectionManager.getInstance().sendCallState("ENDED", "", "");
        }
    }

    private void handleMapsNavigation(GlassesConnectionManager conn, String title, String text, String subText) {
        String combined = (title + " " + text).toLowerCase(Locale.ROOT);
        String dir = "STRAIGHT";
        if (combined.contains("u-turn") || combined.contains("uturn")) {
            dir = "UTURN";
        } else if (combined.contains("slight left") || combined.contains("keep left")) {
            dir = "SLIGHT_LEFT";
        } else if (combined.contains("slight right") || combined.contains("keep right")) {
            dir = "SLIGHT_RIGHT";
        } else if (combined.contains("left")) {
            dir = "LEFT";
        } else if (combined.contains("right")) {
            dir = "RIGHT";
        } else if (combined.contains("arrive") || combined.contains("destination")) {
            dir = "ARRIVED";
        }

        // In Google Maps notifications, title is typically distance ("250 m") or street,
        // and text is street name ("Turn right onto Main St")
        String dist = title.length() <= 12 ? title : "200 m";
        String street = text.isEmpty() ? title : text;
        String eta = subText.isEmpty() ? "Active Route" : subText;
        conn.sendNavigation(dir, dist, street, eta);
    }

    private String classifyPackage(String pkg) {
        if (pkg.contains("whatsapp")) return "WHATSAPP";
        if (pkg.contains("mms") || pkg.contains("messaging") || pkg.contains("sms")) return "SMS";
        if (pkg.contains("gm") || pkg.contains("mail") || pkg.contains("outlook")) return "EMAIL";
        if (pkg.contains("telegram") || pkg.contains("discord") || pkg.contains("slack")) return "CHAT";
        if (pkg.contains("instagram") || pkg.contains("twitter") || pkg.contains("facebook")) return "SOCIAL";
        return "APP";
    }
}
