#include "matrix.h"

Mat4 Mat4::identity() {
    Mat4 res;
    memset(&res, 0, sizeof(Mat4));
    res.data[0 + 0 * 4] = 1.0f;
    res.data[1 + 1 * 4] = 1.0f;
    res.data[2 + 2 * 4] = 1.0f;
    res.data[3 + 3 * 4] = 1.0f;
    return res;
}

Mat4 Mat4::ortho(float left, float right, float bottom, float top, float nearVal, float farVal) {
    Mat4 res = identity();

    res.data[0 + 0 * 4] = 2.0f / (right - left);
    res.data[0 + 3 * 4] = -(right + left) / (right - left);

    res.data[1 + 1 * 4] = 2.0f / (top - bottom);
    res.data[1 + 3 * 4] = -(top + bottom) / (top - bottom);

    res.data[2 + 2 * 4] = -2.0f / (farVal - nearVal);
    res.data[2 + 3 * 4] = -(farVal + nearVal) / (farVal - nearVal);

    return res;
}

Mat4 Mat4::translation(float x, float y, float z) {
    Mat4 res = identity();

    res.data[0 + 3 * 4] = x;
    res.data[1 + 3 * 4] = y;
    res.data[2 + 3 * 4] = z;

    return res;
}

Mat4 Mat4::scale(float x, float y, float z) {
    Mat4 res = identity();

    res.data[0 + 0 * 4] = x;
    res.data[1 + 1 * 4] = y;
    res.data[2 + 2 * 4] = z;

    return res;
}

Mat4 Mat4::operator*(const Mat4& r) const {
    Mat4 res;

    for(int row = 0; row < 4; row++) {
        for(int col = 0; col < 4; col++) {
            res.data[row + col * 4] =
                this->data[row + 0 * 4] * r.data[0 + col * 4] +
                this->data[row + 1 * 4] * r.data[1 + col * 4] +
                this->data[row + 2 * 4] * r.data[2 + col * 4] +
                this->data[row + 3 * 4] * r.data[3 + col * 4];
        }
    }

    return res;
}
