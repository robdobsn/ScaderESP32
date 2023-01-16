// RBotFirmware
// Rob Dobson 2016-18

#include "AxisValues.h"

double AxisUtils::cosineRule(double a, double b, double c)
{
    // Calculate angle C of a triangle using the cosine rule
    // LOG_V(MODULE_PREFIX, "cosineRule a %f b %f c %f acos %f = %f",
    //         a, b, c, (a*a + b*b - c*c) / (2 * a * b), acos((a*a + b*b - c*c) / (2 * a * b)));
    double val = (a*a + b*b - c*c) / (2 * a * b);
    if (val > 1) val = 1;
    if (val < -1) val = -1;
    return acos(val);
}

double AxisUtils::wrapRadians( double angle )
{
    static const double twoPi = 2.0 * M_PI;
    // LOG_V(MODULE_PREFIX, "wrapRadians %f %f", angle, angle - twoPi * floor( angle / twoPi ));
    return angle - twoPi * floor( angle / twoPi );
}

double AxisUtils::wrapDegrees( double angle )
{
    // LOG_V(MODULE_PREFIX, "wrapDegrees %f %f", angle, angle - 360 * floor( angle / 360 ));
    return angle - 360.0 * floor( angle / 360.0 );
}

double AxisUtils::r2d(double angleRadians)
{
    // LOG_V(MODULE_PREFIX, "r2d %f %f", angleRadians, angleRadians * 180.0 / M_PI);
    return angleRadians * 180.0 / M_PI;
}

double AxisUtils::d2r(double angleDegrees)
{
    return angleDegrees * M_PI / 180.0;
}

bool AxisUtils::isApprox(double v1, double v2, double withinRng)
{
    // LOG_V(MODULE_PREFIX, "isApprox %f %f = %d", v1, v2, fabs(v1 - v2) < withinRng);
    return fabs(v1 - v2) < withinRng;
}

bool AxisUtils::isApproxWrap(double v1, double v2, double wrapSize, double withinRng)
{
    // LOG_V(MODULE_PREFIX, "isApprox %f %f = %d", v1, v2, fabs(v1 - v2) < withinRng);
    double t1 = v1 - wrapSize * floor(v1 / wrapSize);
    double t2 = v2 - wrapSize * floor(v2 / wrapSize);
    return (fabs(t1 - t2) < withinRng) || (fabs(t1 - wrapSize - t2) < withinRng) || (fabs(t1 + wrapSize - t2) < withinRng);
}
