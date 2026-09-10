#pragma once
#include <M5GFX.h>
#include "oracle/core.h"
namespace oracle {
void draw(lgfx::LGFX_Sprite& canvas, const View& view, lgfx::LGFX_Sprite* image = nullptr);
void drawText(lgfx::LGFX_Sprite& canvas, int x, int y, const std::string& str,
              int scale=1, std::uint32_t color=0xF6EDDD);
void drawTarotCards(lgfx::LGFX_Sprite& canvas, const Prediction& result,
                    int x, int y, int width, int height, std::uint32_t color);
}
