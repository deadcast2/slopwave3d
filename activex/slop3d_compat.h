/*
 * slop3d_compat.h — VC6 compatibility shim
 *
 * Provides missing C99/C++ standard library features for Visual C++ 6.0,
 * then includes the shared engine header. ActiveX builds include this
 * instead of slop3d.h directly.
 */

#ifndef SLOP3D_COMPAT_H
#define SLOP3D_COMPAT_H

/* VC6 lacks sqrtf — map to (float)sqrt() */
#if defined(_MSC_VER) && (_MSC_VER < 1300)
#include <math.h>
#define sqrtf(x) ((float)sqrt(x))
#endif

#include "../src/slop3d.h"

#endif /* SLOP3D_COMPAT_H */
