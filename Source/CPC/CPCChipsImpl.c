
#define CHIPS_IMPL
#include "CPCChipsImpl.h"

#define CHIPS_UTIL_IMPL
#include "util/z80dasm.h"

uint32_t GetCPCColour(unsigned int index)
{
    return _am40010_cpc_colors[index];
}