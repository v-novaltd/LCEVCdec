

extern "C"
{
#include "surface/surface.h"
}

#include <functional>
#include <vector>

using PelFunction = std::function<int16_t(int16_t)>;

std::vector<uint8_t> entropyEncode(const Surface_t& surface, bool bRLEOnly, const PelFunction& pelOp);