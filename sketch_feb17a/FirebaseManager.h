#ifndef FIREBASE_MANAGER_H
#define FIREBASE_MANAGER_H

#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

/**
 * @brief Reusable component for Firebase Realtime Database management.
 * Now supports non-blocking Streams for ultra-responsive updates.
 */
class FirebaseManager {
private:
    FirebaseData _fbdo;
    FirebaseData _stream; // Dedicated data object for streaming
    FirebaseAuth _auth;
    FirebaseConfig _config;
    String _dbUrl;
    String _dbSecret;
    bool _isInitialized;

public:
    FirebaseManager(const char* host, const char* auth)
        : _dbUrl(host), _dbSecret(auth), _isInitialized(false) {}

    void begin() {
        // Robust URL cleaning: remove https:// and trailing slashes
        String host = _dbUrl;
        if (host.startsWith("https://")) host.remove(0, 8);
        if (host.endsWith("/")) host.remove(host.length() - 1);
        
        _config.host = host.c_str();
        _config.signer.tokens.legacy_token = _dbSecret.c_str();
        
        // Check for placeholder secret
        if (_dbSecret == "YOUR_FIREBASE_DATABASE_SECRET" || _dbSecret.length() < 10) {
            Serial.println("\n[!] CRITICAL: You are using a placeholder Database Secret!");
            Serial.println("[!] Please replace 'YOUR_FIREBASE_DATABASE_SECRET' in sketch_feb17a.ino");
        }

        // Increase timeout for unstable networks
        _config.timeout.serverResponse = 10 * 1000; 

        Firebase.begin(&_config, &_auth);
        Firebase.reconnectWiFi(true);
        _isInitialized = true;
    }

    /**
     * @brief Setup a stream to listen for real-time changes.
     * This is non-blocking and fires when data on the path changes.
     */
    bool setStream(const char* path, FirebaseData::StreamEventCallback callback, FirebaseData::StreamTimeoutCallback timeout) {
        if (!Firebase.RTDB.beginStream(&_stream, path)) {
            return false;
        }
        Firebase.RTDB.setStreamCallback(&_stream, callback, timeout);
        return true;
    }

    void setInt(const char* path, int value) {
        Firebase.RTDB.setIntAsync(&_fbdo, path, value);
    }

    void setString(const char* path, String value) {
        Firebase.RTDB.setStringAsync(&_fbdo, path, value);
    }

    bool ready() {
        return Firebase.ready();
    }
};

#endif
