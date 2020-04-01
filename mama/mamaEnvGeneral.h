#ifndef MAMAENVGENERAL_H
#define MAMAENVGENERAL_H

#include <mama/mama.h>

//
// "Boilerplate" definitions
/* Windows. */
#if WIN32

#ifdef MAMAENV_EXPORTS
#define MAMAENV_API __declspec(dllexport)
#else
#define MAMAENV_API __declspec(dllimport)
#endif

/* GCC */
#else

#ifdef MAMAENV_EXPORTS
#define MAMAENV_API extern
#else
#define MAMAENV_API
#endif

#endif

/* Inline function declarations are different on each platform, this define
 * provides the correct keyword.
 */
#ifndef MAMAENVINLINE

/* Windows */
#ifdef WIN32

#define MAMAENVINLINE __inline
#define MAMAENVFORCEINLINE __forceinline

/* Solaris */
#elif __SUNPRO_C

#define MAMAENVINLINE inline
#define MAMAENVFORCEINLINE inline

/* All other OS, assuming that GCC is supported. */
#else

#define MAMAENVINLINE inline static
#define MAMAENVFORCEINLINE inline static

#endif
#endif

#endif
