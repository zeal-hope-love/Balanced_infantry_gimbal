#ifndef STRUCT_TYPEDEF_H
#define STRUCT_TYPEDEF_H

/* Exact-width integer types come from the C standard library.
 * Do not redefine them here: with GCC/newlib int32_t is "long int",
 * so a local "signed int int32_t" typedef would conflict. */
#include <stdint.h>

/* Supplies the Keil/ARMCC compatible __packed macro for GCC. */
#include <sys/cdefs.h>
#ifndef __packed
#define __packed __attribute__((packed))
#endif

typedef unsigned char bool_t;
typedef float fp32;
typedef double fp64;

#endif
