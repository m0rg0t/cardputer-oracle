#pragma once
#include "oracle/core.h"
#include <memory>
namespace oracle {
bool buildTarotReceipt(const Prediction& result, std::unique_ptr<std::uint8_t[]>& bytes,
                       std::size_t& size, unsigned& height);
}
