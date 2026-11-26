#pragma once

class ComplementaryFilter {
public:
    ComplementaryFilter(float alpha = 0.98f);

    void update(float accel[3], float gyro[3], float dt);
    void reset();

    float getPitch() const { return pitch_; }
    float getRoll() const { return roll_; }

    void setAlpha(float alpha) { alpha_ = alpha; }

private:
    float alpha_;
    float pitch_;
    float roll_;
    bool initialized_;
};
