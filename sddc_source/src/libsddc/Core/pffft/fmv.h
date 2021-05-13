#ifndef FMV_H

#if HAVE_FUNC_ATTRIBUTE_IFUNC
#if defined(__has_attribute)
#if __has_attribute(target_clones)
#if defined(__x86_64)

// see https://gcc.gnu.org/wiki/FunctionMultiVersioning
#define PF_TARGET_CLONES __attribute__((target_clones("avx","sse4.2","sse3","sse2","sse","default")))
#define HAVE_PF_TARGET_CLONES  1
#endif
#endif
#endif
#endif

#ifndef PF_TARGET_CLONES
#define PF_TARGET_CLONES
#endif

#endif
