#include "Vector2.h"
#include <math.h>


float Vector2::Length() const {
    return sqrtf(x * x + y * y);
}


Vector2 Vector2::Normalize() const {
    float len = Length();

    if (len == 0.0f) {
        return { 0.0f, 0.0f };
    };

    return { x / len, y / len };
}