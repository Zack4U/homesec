#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

// ============================================================================
// Simplified Kalman Filter for 1D angle tracking
// State: [angle, angular_velocity]
// ============================================================================
struct KalmanFilter1D {
    float angle = 0;
    float velocity = 0;
    float P[2][2] = {{100, 0}, {0, 100}};  // covariance
    float Q_angle = 0.1f;    // process noise
    float Q_velocity = 0.5f;
    float R = 5.0f;          // measurement noise

    void predict(float dt) {
        // State prediction
        angle += velocity * dt;
        // Covariance prediction
        float P00 = P[0][0] + dt * (P[1][0] + P[0][1]) + dt * dt * P[1][1] + Q_angle;
        float P01 = P[0][1] + dt * P[1][1];
        float P10 = P[1][0] + dt * P[1][1];
        float P11 = P[1][1] + Q_velocity;
        P[0][0] = P00; P[0][1] = P01; P[1][0] = P10; P[1][1] = P11;
    }

    void update(float measurement) {
        // Innovation
        float y = measurement - angle;
        float S = P[0][0] + R;
        // Kalman gain
        float K0 = P[0][0] / S;
        float K1 = P[1][0] / S;
        // State update
        angle += K0 * y;
        velocity += K1 * y;
        // Covariance update
        float P00 = (1 - K0) * P[0][0];
        float P01 = (1 - K0) * P[0][1];
        float P10 = P[1][0] - K1 * P[0][0];
        float P11 = P[1][1] - K1 * P[0][1];
        P[0][0] = P00; P[0][1] = P01; P[1][0] = P10; P[1][1] = P11;
    }

    int getPredictedAngle(float dt_ahead) const {
        return constrain((int)(angle + velocity * dt_ahead), 0, 180);
    }

    void reset() {
        angle = 0; velocity = 0;
        P[0][0] = 100; P[0][1] = 0; P[1][0] = 0; P[1][1] = 100;
    }
};

// ============================================================================
// Radar System: HC-SR04 ultrasonic on servo with patrol/detect/track modes
// ============================================================================
class RadarSystem {
public:
    void begin() {
        pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
        pinMode(PIN_ULTRASONIC_ECHO, INPUT);
        servo_.attach(PIN_SERVO_RADAR);
        servo_.write(currentAngle_);
        memset(scanMap_, 0, sizeof(scanMap_));
        memset(prevScanMap_, 0, sizeof(prevScanMap_));
        Serial.println(F("[RADAR] Initialized OK"));
    }

    void update() {
        if (mode_ == RadarMode::OFF) return;
        if (mode_ == RadarMode::MANUAL) return;

        unsigned long now = millis();
        if (now - lastStep_ < stepDelayMs_) return;
        lastStep_ = now;

        // Measure distance at current angle
        float dist = measureDistance();
        if (currentAngle_ >= 0 && currentAngle_ < RADAR_SCAN_MAP_SIZE) {
            scanMap_[currentAngle_] = dist;
        }
        lastDistance_ = dist;

        switch (mode_) {
            case RadarMode::PATROL:    updatePatrol(dist, now); break;
            case RadarMode::DETECTION: updateDetection(dist, now); break;
            case RadarMode::TRACKING:  updateTracking(dist, now); break;
            default: break;
        }
    }

    // ---- Configuration ----
    void setAngleRange(int minA, int maxA) {
        minAngle_ = constrain(minA, 0, 180);
        maxAngle_ = constrain(maxA, 0, 180);
        if (minAngle_ > maxAngle_) { int t = minAngle_; minAngle_ = maxAngle_; maxAngle_ = t; }
    }

    void setDetectionThreshold(int cm) { detectionThreshold_ = cm; }
    void setAlertDistance(int cm) { alertDistance_ = cm; }
    void setStepDelay(unsigned long ms) { stepDelayMs_ = ms; }

    void setMode(RadarMode m) {
        if (m == mode_) return;
        RadarMode prev = mode_;
        mode_ = m;
        modeChanged_ = true;
        if (m == RadarMode::PATROL) {
            sweepDirection_ = 1;
            trackingLostCount_ = 0;
        }
        Serial.print(F("[RADAR] Mode: "));
        Serial.println((int)m);
    }

    void setManualAngle(int angle) {
        mode_ = RadarMode::MANUAL;
        currentAngle_ = constrain(angle, 0, 180);
        if (!servoAttached_) { servo_.attach(PIN_SERVO_RADAR); servoAttached_ = true; }
        servo_.write(currentAngle_);
        lastStep_ = millis();
    }

    void returnToPatrol() {
        kalman_.reset();
        trackingLostCount_ = 0;
        alertTriggered_ = false;
        setMode(RadarMode::PATROL);
    }

    // ---- State getters ----
    RadarMode getMode() const { return mode_; }
    int getCurrentAngle() const { return currentAngle_; }
    float getLastDistance() const { return lastDistance_; }
    int getTrackingAngle() const { return trackingAngle_; }
    int getPredictedAngle() const { return kalman_.getPredictedAngle(0.5f); }
    const float* getScanMap() const { return scanMap_; }
    int getMinAngle() const { return minAngle_; }
    int getMaxAngle() const { return maxAngle_; }

    bool hasModeChanged() { bool c = modeChanged_; modeChanged_ = false; return c; }
    bool hasAlertTriggered() { bool a = alertTriggered_; alertTriggered_ = false; return a; }
    bool isApproaching() const { return approaching_; }
    bool isTrackingActive() const { return mode_ == RadarMode::TRACKING; }

    // Mark known target (BLE correlated) - stop tracking this object
    void markTargetAsKnown() {
        knownTarget_ = true;
        returnToPatrol();
        Serial.println(F("[RADAR] Target marked as KNOWN"));
    }

private:
    float measureDistance() {
        digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
        delayMicroseconds(2);
        digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
        delayMicroseconds(10);
        digitalWrite(PIN_ULTRASONIC_TRIG, LOW);

        long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 25000); // 25ms timeout
        if (duration == 0) return RADAR_MAX_DISTANCE;
        float dist = (duration * 0.034f) / 2.0f;
        return constrain(dist, 0, (float)RADAR_MAX_DISTANCE);
    }

    void moveServo(int angle) {
        currentAngle_ = constrain(angle, 0, 180);
        if (!servoAttached_) { servo_.attach(PIN_SERVO_RADAR); servoAttached_ = true; }
        servo_.write(currentAngle_);
    }

    // ---- PATROL MODE ----
    void updatePatrol(float dist, unsigned long now) {
        // Check for obstacle in detection range
        if (dist < detectionThreshold_ && dist > 2) {
            // Store detection point
            detectedAngle_ = currentAngle_;
            detectedDistance_ = dist;
            setMode(RadarMode::DETECTION);
            Serial.print(F("[RADAR] Obstacle at "));
            Serial.print(currentAngle_);
            Serial.print(F("deg, "));
            Serial.print(dist);
            Serial.println(F("cm"));
            return;
        }

        // Normal sweep
        currentAngle_ += sweepDirection_;
        if (currentAngle_ >= maxAngle_) {
            currentAngle_ = maxAngle_;
            sweepDirection_ = -1;
            // Store prev map at end of sweep
            memcpy(prevScanMap_, scanMap_, sizeof(scanMap_));
        } else if (currentAngle_ <= minAngle_) {
            currentAngle_ = minAngle_;
            sweepDirection_ = 1;
        }
        moveServo(currentAngle_);
    }

    // ---- DETECTION MODE ----
    void updateDetection(float dist, unsigned long now) {
        // Measure at detected angle to confirm and analyze
        moveServo(detectedAngle_);

        if (dist < detectionThreshold_ && dist > 2) {
            // Compare with previous reading
            if (dist < detectedDistance_ - 2) {
                // Object is APPROACHING
                approaching_ = true;
                alertTriggered_ = true;
                knownTarget_ = false;
                
                // Initialize Kalman with current angle
                kalman_.reset();
                kalman_.angle = (float)detectedAngle_;
                kalman_.velocity = 0;
                trackingAngle_ = detectedAngle_;

                setMode(RadarMode::TRACKING);
                Serial.print(F("[RADAR] APPROACHING! Entering TRACKING at "));
                Serial.print(detectedAngle_);
                Serial.println(F("deg"));
            } else {
                // Object is static or moving away
                detectedDistance_ = dist; // update for next comparison
                detectionHoldCount_++;
                if (detectionHoldCount_ > 20) { // ~1 second of static
                    detectionHoldCount_ = 0;
                    approaching_ = false;
                    returnToPatrol();
                }
            }
        } else {
            // Object left detection range
            approaching_ = false;
            detectionHoldCount_ = 0;
            returnToPatrol();
        }
    }

    // ---- TRACKING MODE ----
    void updateTracking(float dist, unsigned long now) {
        if (knownTarget_) return; // Don't track known BLE devices

        float dt = (now - lastTrackTime_) / 1000.0f;
        if (dt <= 0) dt = 0.05f;
        lastTrackTime_ = now;

        if (dist < detectionThreshold_ && dist > 2) {
            trackingLostCount_ = 0;

            // Scan slightly left and right to find exact angle of closest approach
            int bestAngle = currentAngle_;
            float bestDist = dist;

            // Quick 5-degree scan around current position
            for (int offset = -3; offset <= 3; offset++) {
                int testAngle = constrain(currentAngle_ + offset, minAngle_, maxAngle_);
                moveServo(testAngle);
                delayMicroseconds(500); // minimal settling
                float d = measureDistance();
                if (d < bestDist && d > 2) {
                    bestDist = d;
                    bestAngle = testAngle;
                }
            }

            // Update Kalman filter with best angle
            kalman_.predict(dt);
            kalman_.update((float)bestAngle);

            trackingAngle_ = (int)kalman_.angle;
            int predictedAngle = kalman_.getPredictedAngle(0.3f);
            
            // Move servo to predicted position
            moveServo(constrain(predictedAngle, minAngle_, maxAngle_));
            lastDistance_ = bestDist;

            // Check alert distance
            if (bestDist < alertDistance_) {
                alertTriggered_ = true;
            }
        } else {
            trackingLostCount_++;
            if (trackingLostCount_ >= DEFAULT_TRACKING_LOST_COUNT) {
                Serial.println(F("[RADAR] Target LOST - returning to patrol"));
                approaching_ = false;
                returnToPatrol();
            } else {
                // Use prediction to keep following
                kalman_.predict(dt);
                int predicted = kalman_.getPredictedAngle(0.0f);
                moveServo(constrain(predicted, minAngle_, maxAngle_));
            }
        }
    }

    // Hardware
    Servo servo_;
    bool servoAttached_ = true;

    // State
    RadarMode mode_ = RadarMode::PATROL;
    int currentAngle_ = 0;
    float lastDistance_ = 0;
    int sweepDirection_ = 1;
    unsigned long lastStep_ = 0;

    // Configuration
    int minAngle_ = DEFAULT_RADAR_MIN_ANGLE;
    int maxAngle_ = DEFAULT_RADAR_MAX_ANGLE;
    int detectionThreshold_ = DEFAULT_DETECTION_THRESHOLD;
    int alertDistance_ = DEFAULT_ALERT_DISTANCE;
    unsigned long stepDelayMs_ = DEFAULT_RADAR_STEP_DELAY_MS;

    // Detection
    int detectedAngle_ = 0;
    float detectedDistance_ = 0;
    int detectionHoldCount_ = 0;
    bool approaching_ = false;
    bool alertTriggered_ = false;
    bool modeChanged_ = false;
    bool knownTarget_ = false;

    // Tracking
    KalmanFilter1D kalman_;
    int trackingAngle_ = 0;
    int trackingLostCount_ = 0;
    unsigned long lastTrackTime_ = 0;

    // Scan maps
    float scanMap_[RADAR_SCAN_MAP_SIZE];
    float prevScanMap_[RADAR_SCAN_MAP_SIZE];
};
