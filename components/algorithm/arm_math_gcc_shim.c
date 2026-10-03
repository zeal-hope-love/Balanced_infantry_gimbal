/**
  * @file       arm_math_gcc_shim.c
  * @brief      GCC replacements for the few CMSIS-DSP symbols this project uses.
  * @note       components/algorithm/arm_cortexM4lf_math.lib is an ARM Compiler
  *             (Keil) archive, so GNU ld cannot merge it into this firmware
  *             ("conflicting CPU architectures"). Only arm_sin_f32() and
  *             arm_cos_f32() are referenced by the application code, so they are
  *             provided here with the same contract (radians in, radians out).
  */

#include "arm_math.h"
#include <math.h>

float32_t arm_sin_f32(float32_t x)
{
    return sinf(x);
}

float32_t arm_cos_f32(float32_t x)
{
    return cosf(x);
}
