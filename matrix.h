#pragma once

struct Mat4 {
    float data[16];

    static Mat4 identity();
    static Mat4 ortho(float left, float right, float bottom, float top, float nearVal, float farVal);
    static Mat4 translation(float x, float y, float z);
    static Mat4 scale(float x, float y, float z);

    Mat4 operator*(const Mat4& r) const;
};
