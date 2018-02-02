/* -----------------------------------------------------------------------------
 * math.i
 *
 * SWIG library file for floating point operations.
 * ----------------------------------------------------------------------------- */

%module math
%{
#include <math.h>
%}

extern double	cos(double x);
/* Cosine of x */

extern double	sin(double x);
/* Sine of x */

extern double	tan(double x);
/* Tangent of x */

extern double	acos(double x);
/* Inverse cosine in range [-PI/2,PI/2], x in [-1,1]. */

extern double	asin(double x);
/* Inverse sine in range [0,PI], x in [-1,1]. */

extern double	atan(double x);
/* Inverse tangent in range [-PI/2,PI/2]. */

extern double	atan2(double y, double x);
/* Inverse tangent of y/x in range [-PI,PI]. */

extern double	cosh(double x);
/* Hyperbolic cosine of x */

extern double	sinh(double x);
/* Hyperbolic sine of x */

extern double	tanh(double x);
/* Hyperbolic tangent of x */

extern double	exp(double x);
/* Natural exponential function e^x */

extern double	log(double x);
/* Natural logarithm ln(x), x > 0 */

extern double	log10(double x);
/* Base 10 logarithm, x > 0 */

extern double	pow(double x, double y);
/* Power function x^y. */

extern double	sqrt(double x);
/* Square root. x >= 0 */

extern double	fabs(double x);
/* Absolute value of x */

extern double	ceil(double x);
/* Smallest integer not less than x, as a double */

extern double	floor(double x);
/* Largest integer not greater than x, as a double */

extern double	fmod(double x, double y);
/* Floating-point remainder of x/y, with the same sign as x. */

#define M_E		2.7182818284590452354
#define M_LOG2E		1.4426950408889634074
#define M_LOG10E	0.43429448190325182765
#define M_LN2		0.69314718055994530942
#define M_LN10		2.30258509299404568402
#define M_PI		3.14159265358979323846
#define M_PI_2		1.57079632679489661923
#define M_PI_4		0.78539816339744830962
#define M_1_PI		0.31830988618379067154
#define M_2_PI		0.63661977236758134308
#define M_2_SQRTPI	1.12837916709551257390
#define M_SQRT2		1.41421356237309504880
#define M_SQRT1_2	0.70710678118654752440

