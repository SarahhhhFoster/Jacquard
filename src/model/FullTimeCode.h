#pragma once
#include <cmath>

// Time is represented as PPQ (quarter notes, floating-point).
// This gives sub-tick precision for arbitrary frequency placement.
using FullTimeCode = double;

inline bool ftcApproxEqual(FullTimeCode a, FullTimeCode b, double eps = 1e-9)
{
    return std::abs(a - b) < eps;
}
