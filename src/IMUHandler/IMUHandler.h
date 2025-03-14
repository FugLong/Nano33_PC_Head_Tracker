#pragma once

#include <LSM9DS1/Arduino_LSM9DS1.h>

#include <BMI270_BMM150/Arduino_BMI270_BMM150_Modded.h>

class IMUHandler {
public:
    enum IMURevision {
        Rev1,
        Rev2,
        Unknown
    };

private:
    IMURevision revision;
    float gx_temp;
    float gy_temp;
    float gz_temp;
    float ax_temp;
    float ay_temp;
    float az_temp;
    float mx_temp;
    float my_temp;
    float mz_temp;


public:
    IMUHandler() : revision(Unknown) {}

    // Detect and initialize the appropriate IMU
    bool begin() {
        if (IMU_LSM9DS1.begin()) {
            revision = Rev1;
            IMU_LSM9DS1.setGyroODR(4);  // 238Hz
            IMU_LSM9DS1.setAccelODR(4); // 238Hz
            IMU_LSM9DS1.setMagnetODR(7); // 80Hz
            IMU_LSM9DS1.setContinuousMode();
        } else if (IMU_BMI270_BMM150.Rev2_begin()) {
            revision = Rev2;
            IMU_BMI270_BMM150.Rev2_setGyroODR(3);  // Max possible
            IMU_BMI270_BMM150.Rev2_setAccelODR(3); // Max possible
            IMU_BMI270_BMM150.Rev2_setMagnetODR(7); // Max possible
            IMU_BMI270_BMM150.Rev2_setContinuousMode();
        } else {
            revision = Unknown;
            return false;
        }
        return true;
    }

    IMURevision getRevision() const {
        return revision;
    }

    // Read IMU data
    bool readIMU(float &gX, float &gY, float &gZ, float &aX, float &aY, float &aZ, float &mX, float &mY, float &mZ) {
        switch (revision) {
        case Rev1:
            if (IMU_LSM9DS1.gyroAvailable()) {
                IMU_LSM9DS1.readRawGyro(gX, gY, gZ);
                gX *= -1.0; // flip X axis
            }
            if (IMU_LSM9DS1.accelAvailable()) {
                IMU_LSM9DS1.readRawAccel(aX, aY, aZ);
                aX *= -1.0;
            }
            if (IMU_LSM9DS1.magnetAvailable()) {
                IMU_LSM9DS1.readRawMagnet(mX, mY, mZ);
            }
            break;

        case Rev2:
            if (IMU_BMI270_BMM150.Rev2_gyroscopeAvailable()) {
                IMU_BMI270_BMM150.Rev2_readGyroscope(gX, gY, gZ);
                gx_temp = gY;
                gy_temp = gX;
                gz_temp = -gZ;
                gX = gx_temp;
                gY = gy_temp;
                gZ = gz_temp;
            }
            if (IMU_BMI270_BMM150.Rev2_accelerationAvailable()) {
                IMU_BMI270_BMM150.Rev2_readAcceleration(aX, aY, aZ);
                ax_temp = aY;
                ay_temp = aX;
                az_temp = -aZ;
                aX = ax_temp;
                aY = ay_temp;
                aZ = az_temp;
            }
            if (IMU_BMI270_BMM150.Rev2_magneticFieldAvailable()) {
                IMU_BMI270_BMM150.Rev2_readMagneticField(mX, mY, mZ);
                mx_temp = mY;      // X gets Y (matching gyro/accel pattern)
                my_temp = mX;      // Y gets X (matching gyro/accel pattern)
                mz_temp = -mZ;     // Z gets negated Z (matching gyro/accel pattern)
                mX = mx_temp;
                mY = my_temp;
                mZ = mz_temp;
            }
            break;

        default:
            return false;
        }
        return true;
    }

    bool gyroAvailable() {
        switch (revision) {
        case Rev1:
            return IMU_LSM9DS1.gyroAvailable();
            break;

        case Rev2:
            return IMU_BMI270_BMM150.Rev2_gyroscopeAvailable();
            break;

        default:
            return false;
        }
        return true;
    }

    bool readRawGyro(float &gX, float &gY, float &gZ) {
        switch (revision) {
        case Rev1:
            IMU_LSM9DS1.readGyro(gX, gY, gZ);
            gX *= -1.0; // flip X axis
            break;

        case Rev2:
            IMU_BMI270_BMM150.Rev2_readGyroscope(gX, gY, gZ);
            gx_temp = gY;
            gy_temp = gX;
            gz_temp = -gZ;
            gX = gx_temp;
            gY = gy_temp;
            gZ = gz_temp;
            break;

        default:
            return false;
        }
        return true;
    }

    bool accelAvailable() {
        switch (revision) {
        case Rev1:
            return IMU_LSM9DS1.accelAvailable();
            break;

        case Rev2:
            return IMU_BMI270_BMM150.Rev2_accelerationAvailable();
            break;

        default:
            return false;
        }
        return true;
    }

    bool readRawAccel(float &aX, float &aY, float &aZ) {
        switch (revision) {
        case Rev1:
            IMU_LSM9DS1.readAccel(aX, aY, aZ);
            aX *= -1.0;
            break;

        case Rev2:
            IMU_BMI270_BMM150.Rev2_readAcceleration(aX, aY, aZ);
            ax_temp = aY;
            ay_temp = aX;
            az_temp = -aZ;
            aX = ax_temp;
            aY = ay_temp;
            aZ = az_temp;
            break;

        default:
            return false;
        }
        return true;
    }

    bool magnetAvailable() {
        switch (revision) {
        case Rev1:
            return IMU_LSM9DS1.magnetAvailable();
            break;

        case Rev2:
            return IMU_BMI270_BMM150.Rev2_magneticFieldAvailable();
            break;

        default:
            return false;
        }
        return true;
    }

    bool readRawMagnet(float &mX, float &mY, float &mZ) {
        switch (revision) {
        case Rev1:
            IMU_LSM9DS1.readMagnet(mX, mY, mZ);
            break;

        case Rev2:
            IMU_BMI270_BMM150.Rev2_readMagneticField(mX, mY, mZ);
            mx_temp = mY;      // X gets Y (matching gyro/accel pattern)
            my_temp = mX;      // Y gets X (matching gyro/accel pattern)
            mz_temp = -mZ;     // Z gets negated Z (matching gyro/accel pattern)
            mX = mx_temp;
            mY = my_temp;
            mZ = mz_temp;
            break;

        default:
            return false;
        }
        return true;
    }
};
