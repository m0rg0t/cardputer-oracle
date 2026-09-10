#pragma once
#include "oracle/tarot.h"
namespace oracle {
std::string tarotApiRequest(const std::string& model,const std::array<unsigned,3>& cards,const std::string& question);
bool parseTarotResponse(const std::string& json,const std::array<unsigned,3>& cards,
                        const std::string& question,std::uint32_t id,Prediction& result);
}
