#pragma once
#include "oracle/core.h"

namespace oracle {
constexpr unsigned kArcanaCount = 22;
const char* arcanaName(unsigned index);
std::array<unsigned,3> drawArcana(std::uint32_t seed);
bool validSpread(const std::array<unsigned,3>& cards);
bool validQuestion(const std::string& question);
bool appendQuestionKey(std::string& question, char key, bool russian);
void eraseQuestionKey(std::string& question);
// Reject unsupported glyphs and excessive output before freezing a result.
bool makeTarotPrediction(const std::array<unsigned,3>& cards, const std::string& reading,
                         std::uint32_t id, Prediction& out, const std::string& question = "");
}
