#pragma once
#include <cstdint>

namespace oracle {
// Physical Cardputer matrix: the printed left/right arrows share ',' and '/'.
constexpr std::uint64_t keyBit(unsigned row,unsigned column) {
    return row<4 && column<14 ? std::uint64_t(1)<<(row*14+column) : 0;
}
constexpr auto kLeftKey=keyBit(3,10);
constexpr auto kRightKey=keyBit(3,12);

class KeyboardEdges {
public:
    std::uint64_t update(std::uint64_t held) {
        auto pressed=held & ~previous_;
        previous_=held;
        return pressed;
    }
private:
    std::uint64_t previous_=0;
};

enum class HomeAction { None, PreviousTheme, NextTheme, Start };
inline HomeAction homeAction(std::uint64_t pressed) {
    if(!pressed) return HomeAction::None;
    const bool left=pressed & kLeftKey, right=pressed & kRightKey;
    if(left && right) return HomeAction::None;
    if(left) return HomeAction::PreviousTheme;
    if(right) return HomeAction::NextTheme;
    return HomeAction::Start;
}
}
