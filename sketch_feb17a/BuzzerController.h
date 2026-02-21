#ifndef BUZZER_CONTROLLER_H
#define BUZZER_CONTROLLER_H

#include <Arduino.h>

/**
 * @brief Reusable component for Buzzer feedback
 * Handles non-blocking audio patterns.
 */
class BuzzerController {
private:
    uint8_t _pin;
    bool _muted;
    unsigned long _patternStartTime;
    int _currentPattern;
    int _step;
    bool _isRunning;

public:
    BuzzerController(uint8_t pin)
        : _pin(pin), _muted(false), _patternStartTime(0), _currentPattern(0), _step(0), _isRunning(false) {}

    void begin() {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
    }

    void setMuted(bool muted) {
        _muted = muted;
        if (_muted) stop();
    }

    void playPattern(int pattern) {
        if (_muted) return;
        _currentPattern = pattern;
        _step = 0;
        _patternStartTime = millis();
        _isRunning = true;
    }

    void stop() {
        digitalWrite(_pin, LOW);
        _isRunning = false;
    }

    /**
     * @brief Update loop for buzzer patterns. Non-blocking.
     */
    void update() {
        if (!_isRunning || _muted) return;

        unsigned long elapsed = millis() - _patternStartTime;

        switch (_currentPattern) {
            case 0: // Single beep (Move Start)
                if (elapsed < 100) digitalWrite(_pin, HIGH);
                else { stop(); }
                break;

            case 1: // Double beep (Target Reached)
                if (elapsed < 100) digitalWrite(_pin, HIGH);
                else if (elapsed < 200) digitalWrite(_pin, LOW);
                else if (elapsed < 300) digitalWrite(_pin, HIGH);
                else { stop(); }
                break;

            case 2: // Triple beep (Error)
                if (elapsed < 100) digitalWrite(_pin, HIGH);
                else if (elapsed < 200) digitalWrite(_pin, LOW);
                else if (elapsed < 300) digitalWrite(_pin, HIGH);
                else if (elapsed < 400) digitalWrite(_pin, LOW);
                else if (elapsed < 500) digitalWrite(_pin, HIGH);
                else { stop(); }
                break;

            case 3: // Low-volume Blip (Cloud Update)
                if (elapsed < 50) analogWrite(_pin, 40); // Soft blip
                else { stop(); }
                break;
        }
    }
};

#endif
