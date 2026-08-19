
#if defined _WIN32 || defined _WIN64

#include <intrin.h>

bool use_mmx() {
  int cpuInfo[4];
  __cpuid(cpuInfo, 1);
  return (cpuInfo[3] & 0x800000) != 0;
}

#else

bool use_mmx() {
  return false;
}

#endif
