#include "oracle/tarot.h"
#include "oracle/tarot_api.h"
#include <ArduinoJson.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>

std::string response(const std::string& reading) {
    JsonDocument text,doc;
    text["reading"]=reading; std::string encoded; serializeJson(text,encoded);
    doc["status"]="completed";
    doc["output"][0]["type"]="reasoning";
    doc["output"][1]["type"]="message"; doc["output"][1]["role"]="assistant"; doc["output"][1]["status"]="completed";
    doc["output"][1]["content"][0]["type"]="output_text"; doc["output"][1]["content"][0]["text"]=encoded;
    std::string result; serializeJson(doc,result); return result;
}
int main(int argc,char** argv) {
    using namespace oracle;
    const std::array<unsigned,3> cards{1,17,19};
    const std::string question="Как пройдёт мой доклад?";
    const std::string reading="Вчера ты приручил хаос, сегодня ловишь звёзды. Завтра даже самый строгий слушатель улыбнётся: твоя магия сработает, особенно если не забудешь включить микрофон.";
    if(argc==3 && std::string(argv[1])=="--request") {
        std::ofstream(argv[2])<<tarotApiRequest("openai/gpt-5.6-luna",cards,question); return 0;
    }
    if(argc==3 && std::string(argv[1])=="--response") {
        std::ifstream file(argv[2]); std::string raw((std::istreambuf_iterator<char>(file)),{});
        Prediction p;
        if(!parseTarotResponse(raw,cards,question,42,p)) return 1;
        std::cout<<p.title<<"\n"<<p.question<<"\n"<<p.text<<"\n"; return 0;
    }
    std::set<unsigned> seen;
    for(unsigned seed=0;seed<10000;++seed) {
        auto spread=drawArcana(seed); assert(validSpread(spread));
        for(auto c:spread) seen.insert(c);
    }
    assert(seen.size()==22); assert(!validSpread({0,0,1})); assert(!validSpread({22,0,1}));
    assert(validQuestion(question)); assert(!validQuestion("bad\ninput")); assert(!validQuestion("🃏"));
    assert(!validQuestion(std::string(81,'a'))); assert(!validQuestion(std::string("\xc0\x80",2)));
    std::string typed;
    for(char c:std::string("Ghbdtn")) assert(appendQuestionKey(typed,c,true));
    assert(typed=="Привет"); eraseQuestionKey(typed); assert(typed=="Приве");
    typed.clear(); for(unsigned i=0;i<80;++i) assert(appendQuestionKey(typed,'z',true));
    assert(!appendQuestionKey(typed,'z',true)); assert(validQuestion(typed));
    for(unsigned i=0;i<81;++i) eraseQuestionKey(typed); assert(typed.empty());
    Prediction p;
    assert(makeTarotPrediction(cards,reading,42,p,question));
    assert(p.tarotCards==cards && p.question==question && p.audio.empty());
    assert(parseTarotResponse(response(reading),cards,question,42,p));
    assert(!parseTarotResponse("{}",cards,question,42,p));
    auto incomplete=response(reading); incomplete.replace(incomplete.find("completed"),9,"incomplete");
    assert(!parseTarotResponse(incomplete,cards,question,42,p));
    assert(!parseTarotResponse(response(std::string(241,'x')),cards,question,42,p));
    assert(!parseTarotResponse(response("🃏 Привет от будущего, всё будет отлично!"),cards,question,42,p));
    assert(!makeTarotPrediction(cards,"       \t                  ",42,p));
    auto refusal=response(reading); const auto pos=refusal.find("output_text"); refusal.replace(pos,11,"refusal");
    assert(!parseTarotResponse(refusal,cards,question,42,p));
    JsonDocument request; assert(!deserializeJson(request,tarotApiRequest("openai/gpt-5.6-luna",cards,question)));
    assert(request["store"]==false && request["stream"]==false);
    assert(request["model"]=="openai/gpt-5.6-luna");
    assert(request["reasoning"]["effort"]=="none" && request["max_output_tokens"]==512);
    assert(request["text"]["format"]["strict"]==true);
    JsonDocument user; assert(!deserializeJson(user,request["input"][1]["content"].as<std::string>()));
    assert(user["question"].as<std::string>()==question && user["present"]==arcanaName(17));
    BuiltinCatalog catalog; DeckSource source(catalog,17); Controller app(source);
    app.changeTheme(-1); assert(app.view.theme==Theme::Tarot);
    assert(app.trigger(0) && app.view.screen==Screen::Question);
    assert(!app.acceptRemote(p,0)); assert(!app.changeTheme(1));
    app.view.question=question; assert(app.submitQuestion(10)); assert(!app.trigger(20));
    assert(!app.tick(999999)); assert(app.acceptRemote(p,100)); assert(!app.tick(2699)); assert(app.tick(2700));
    assert(app.view.result.question==question && app.view.lines.front()=="Вопрос: Как");
    app.back(); app.trigger(3000); app.submitQuestion(3001); app.back();
    assert(!app.acceptRemote(p,3010)); app.failRemote("late"); assert(app.view.screen==Screen::Idle);
    assert(!source.next({"tarot",""},p));
    std::cout<<"PASS: 10,000 unique-card spreads, RU/EN input, question limits, frozen result, API format/refusal/limits, cancellation and offline isolation\n";
}
