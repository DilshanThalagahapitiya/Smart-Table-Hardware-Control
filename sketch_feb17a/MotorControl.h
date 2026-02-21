#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>

/**
 * @brief Reusable component for Motor Control (L298N or similar H-Bridge)
 * Handles non-blocking movement and height simulation with smooth PWM ramping.
 */
class MotorControl {
private:
    uint8_t _in1, _in2;
    uint8_t _ledUp, _ledDown;
    int& _currentHeight;
    int _targetHeight;
    int _minHeight, _maxHeight;
    unsigned long _lastStepTime;
    int _stepInterval;
    
    // Smooth Ramping Variables
    int _currentSpeed;
    const int MAX_SPEED = 255;
    const int RAMP_STEP = 15;

public:
    MotorControl(uint8_t in1, uint8_t in2, uint8_t ledUp, uint8_t ledDown, int& height, int minH, int maxH)
        : _in1(in1), _in2(in2), _ledUp(ledUp), _ledDown(ledDown), _currentHeight(height), 
          _minHeight(minH), _maxHeight(maxH), _lastStepTime(0), _stepInterval(100), _currentSpeed(0) {
        _targetHeight = height;
    }

    void begin() {
        pinMode(_in1, OUTPUT);
        pinMode(_in2, OUTPUT);
        pinMode(_ledUp, OUTPUT);
        pinMode(_ledDown, OUTPUT);
        analogWrite(_in1, 0); // Safety init
        analogWrite(_in2, 0); // Safety init
        stop();
    }

    void setTarget(int target) {
        if (target != _targetHeight) {
            _targetHeight = constrain(target, _minHeight, _maxHeight);
            Serial.printf("Motor: New Target -> %d (Current: %d)\n", _targetHeight, _currentHeight);
        }
    }

    void stop() {
        _currentSpeed = 0;
        analogWrite(_in1, 0);
        analogWrite(_in2, 0);
        digitalWrite(_ledUp, LOW);
        digitalWrite(_ledDown, LOW);
        _targetHeight = _currentHeight; 
    }

    /**
     * @brief Update loop for motor logic. 
     * Now includes PWM ramping for smooth start/stop.
     */
    void update() {
        if (_currentHeight == _targetHeight && _currentSpeed == 0) {
            stop();
            return;
        }

        unsigned long now = millis();
        if (now - _lastStepTime >= _stepInterval) {
            _lastStepTime = now;
            
            if (_currentHeight < _targetHeight) {
                // Ramping Up
                if (_currentSpeed < MAX_SPEED) _currentSpeed = min(MAX_SPEED, _currentSpeed + RAMP_STEP);
                analogWrite(_in1, _currentSpeed);
                digitalWrite(_in2, LOW);
                digitalWrite(_ledUp, HIGH);
                digitalWrite(_ledDown, LOW);
                _currentHeight++;
            } 
            else if (_currentHeight > _targetHeight) {
                // Ramping Down
                if (_currentSpeed < MAX_SPEED) _currentSpeed = min(MAX_SPEED, _currentSpeed + RAMP_STEP);
                analogWrite(_in2, _currentSpeed);
                digitalWrite(_in1, LOW);
                digitalWrite(_ledDown, HIGH);
                digitalWrite(_ledUp, LOW);
                _currentHeight--;
            }
            else {
                // Decelerate when reaching target
                _currentSpeed = 0; 
                stop();
                Serial.printf("Motor: Target %d Reached.\n", _currentHeight);
            }
        }
    }

    bool isMoving() const {
        return _currentHeight != _targetHeight;
    }

    void setStepInterval(int interval) {
        _stepInterval = interval;
    }
};

#endif
