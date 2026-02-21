#ifndef LED_EFFECTS_H
#define LED_EFFECTS_H

#include <Arduino.h>

/**
 * @brief Reusable component for LED Effects (Breathing/Fading)
 * Provides a "next gen" premium feel.
 */
class LedEffects {
private:
    uint8_t _pin;
    int _currentPWM;
    int _targetPWM;
    unsigned long _triggerTime;
    const int BRIGHT = 255;
    const int DIM = 25;

public:
    LedEffects(uint8_t pin)
        : _pin(pin), _currentPWM(DIM), _targetPWM(DIM), _triggerTime(0) {}

    void begin() {
        pinMode(_pin, OUTPUT);
        analogWrite(_pin, _currentPWM);
    }

    /**
     * @brief Flash the LED to full brightness and hold for 1s.
     */
    void trigger() {
        _targetPWM = BRIGHT;
        _currentPWM = BRIGHT;
        _triggerTime = millis();
        analogWrite(_pin, _currentPWM);
    }

    /**
     * @brief Update loop for smooth transitions and breathing.
     */
    void update() {
        unsigned long now = millis();
        
        // After 1 second of being bright, set target to DIM
        if (_triggerTime > 0 && now - _triggerTime > 1000) {
            _targetPWM = DIM;
            _triggerTime = 0;
        }

        // 1. Smoothly transition current PWM to target PWM (Fading)
        static unsigned long lastUpdate = 0;
        if (now - lastUpdate > 30) {
            lastUpdate = now;
            
            if (_currentPWM != _targetPWM) {
                if (_currentPWM > _targetPWM) _currentPWM = max(_targetPWM, _currentPWM - 5);
                else _currentPWM = min(_targetPWM, _currentPWM + 15);
                analogWrite(_pin, _currentPWM);
            } 
            // 2. If we are at the DIM target, start the "Breathing" pulse
            else if (_targetPWM == DIM) {
                static float angle = 0;
                angle += 0.05; // Adjust for pulse speed
                if (angle > TWO_PI) angle = 0;
                
                // Oscillate between 10 and 50 PWM for a subtle breath
                int breatheVal = DIM + (sin(angle) * 20); 
                analogWrite(_pin, breatheVal);
            }
        }
    }

    bool isDimming() const {
        return _targetPWM == DIM && _currentPWM == DIM;
    }
};

#endif
