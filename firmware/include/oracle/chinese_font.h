#pragma once
#include "oracle/locale.h"
#include <cstdint>
namespace oracle {
using ChineseGlyphReader = bool (*)(std::uint16_t, std::uint16_t*);
void setChineseGlyphReader(ChineseGlyphReader reader);
bool loadChineseGlyph(std::uint32_t code, std::uint16_t* rows);
}
