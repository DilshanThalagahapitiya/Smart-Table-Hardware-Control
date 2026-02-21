#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>

/**
 * @brief Reusable component for Button Handling
 * Optimized for ZERO LAG responsiveness.
 */
class ButtonHandler {
private:
    uint8_t _pin;
    bool _lastState;
    bool _isPressed;
    unsigned long _lastDebounceTime;
    unsigned long _debounceDelay;
    unsigned long _lastPressTime;
    uint8_t _clickCount;
    bool _clickReported;

public:
    ButtonHandler(uint8_t pin, unsigned long debounceDelay = 50)
        : _pin(pin), _debounceDelay(debounceDelay), _lastState(HIGH), _isPressed(false), 
          _lastPressTime(0), _clickCount(0), _clickReported(true) {}

    void begin() {
        pinMode(_pin, INPUT_PULLUP);
    }

    /**
     * @brief Polls for click events.
     * Note: Single clicks are reported after a short window to check for double-clicks,
     * BUT for instant feedback, use isPressed() in the loop.
     */
    int getClickType() {
        bool reading = digitalRead(_pin);
        int result = 0;

        if (reading != _lastState) {
            _lastDebounceTime = millis();
        }

        if ((millis() - _lastDebounceTime) > _debounceDelay) {
            if (reading == LOW && !_isPressed) {
                _isPressed = true;
                _clickCount++;
                _lastPressTime = millis();
                _clickReported = false;
            } else if (reading == HIGH) {
                _isPressed = false;
            }
        }

        // Return count after window or if max clicks reached
        if (!_clickReported && (millis() - _lastPressTime > 250 || _clickCount >= 2)) {
            result = _clickCount;
            _clickCount = 0;
            _clickReported = true;
        }

        _lastState = reading;
        return result;
    }

    /**
     * @brief INSTANT state check (No lag). 
     * Use this for starting movement or high-priority responsiveness.
     */
    bool isPressed() {
        return digitalRead(_pin) == LOW;
    }

    /**
     * @brief Check for hold state.
     */
    bool isHeld() {
        return isPressed(); 
    }
};

#endif
