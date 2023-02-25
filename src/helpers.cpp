#include "helpers.h"
#include <math.h>

// rounds a number to anything but zero
int RoundNotZero(double v)
{
    if (v > 0.0 && v < 1.0)
        return 1;
    if (v < 0.0 && v > -1.0)
        return -1;
    else
        return (int)round(v);
}
