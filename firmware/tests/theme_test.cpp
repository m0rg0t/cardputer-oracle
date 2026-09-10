#include "oracle/core.h"
#include "oracle/input.h"
#include <cassert>
#include <iostream>
#include <set>

struct SampleCatalog : oracle::Catalog {
    std::vector<oracle::Prediction> cards;
    std::size_t size() const override { return cards.size(); }
    bool load(std::size_t i,oracle::Prediction& out) override {
        if(i>=cards.size()) return false;
        out=cards[i]; return true;
    }
};
struct IndexedCatalog : SampleCatalog {
    unsigned reads=0;
    bool matches(std::size_t i,oracle::Theme theme) override {
        return i<cards.size() && unsigned(theme)==(i<60?0u:i<90?1u:2u);
    }
    bool load(std::size_t i,oracle::Prediction& out) override {
        ++reads; return SampleCatalog::load(i,out);
    }
};
oracle::Prediction card(const char* id,const char* topic) {
    return {id,"Знак","Сегодня всё получится.",topic,"","",0};
}
int main() {
    using namespace oracle;
    IndexedCatalog indexed;
    for(unsigned i=0;i<120;++i) {
        const auto id="card_"+std::to_string(i);
        indexed.cards.push_back(card(id.c_str(),i<60?"it":i<90?"beer":"tea"));
    }
    DeckSource fast(indexed,7);
    Prediction selected;
    for(const auto theme:{"humor_it","beer","tea"}) {
        const auto before=indexed.reads;
        assert(fast.next({theme,""},selected));
        assert(indexed.reads==before+1); // 120-card index must cause one card read.
    }
    IndexedCatalog stale;
    stale.cards={card("wrong","beer")}; // Index says humor; actual file wins.
    DeckSource staleSource(stale,1);
    assert(!staleSource.next({"humor_it",""},selected));
    SampleCatalog catalog;
    catalog.cards={card("a","life"),card("b","it"),card("c","it"),card("d","beer"),card("e","beer"),
                   card("f","tea"),card("g","tea"),card("h","tea"),card("i","tea")};
    for(unsigned seed=1;seed<=40;++seed) {
        DeckSource source(catalog,seed);
        std::set<std::string> humor,beer,tea;
        std::string lastHumor,lastBeer,lastTea;
        Prediction p;
        // Three differently sized bags must keep their own cycles while interleaved.
        for(unsigned i=0;i<120;++i) {
            assert(source.next({"humor_it",""},p)); assert(p.topic=="life" || p.topic=="it");
            assert(p.id!=lastHumor); lastHumor=p.id; assert(humor.insert(p.id).second);
            if((i+1)%3==0) { assert(humor.size()==3); humor.clear(); }
            assert(source.next({"beer",""},p)); assert(p.topic=="beer");
            assert(p.id!=lastBeer); lastBeer=p.id; assert(beer.insert(p.id).second);
            if((i+1)%2==0) { assert(beer.size()==2); beer.clear(); }
            assert(source.next({"tea",""},p)); assert(p.topic=="tea");
            assert(p.id!=lastTea); lastTea=p.id; assert(tea.insert(p.id).second);
            if((i+1)%4==0) { assert(tea.size()==4); tea.clear(); }
        }
    }
    DeckSource defaults(catalog,5),explicitDefaults(catalog,5);
    Prediction a,b;
    for(unsigned i=0;i<12;++i) {
        assert(!defaults.next({"unknown",""},a));
        assert(!defaults.next({"beer","Will I win?"},a));
        assert(!defaults.next({"tea","Will I win?"},a));
        assert(defaults.next({},a)); assert(explicitDefaults.next({"humor_it",""},b));
        assert(a.id==b.id && (a.topic=="life" || a.topic=="it"));
    }
    SampleCatalog single;
    single.cards={card("one","beer")};
    DeckSource one(single,1);
    assert(!one.next({},a));
    for(int i=0;i<5;++i) { assert(one.next({"beer",""},a)); assert(a.id=="one"); }
    single.cards[0].topic="life";
    assert(!one.next({"beer",""},a)); // Stale catalog entries cannot cross themes.

    SampleCatalog teaOnly;
    teaOnly.cards={card("tea_only","tea")};
    DeckSource oneTea(teaOnly,1);
    assert(!oneTea.next({},a)); assert(!oneTea.next({"beer",""},a));
    for(int i=0;i<5;++i) { assert(oneTea.next({"tea",""},a)); assert(a.id=="tea_only"); }
    teaOnly.cards[0].topic="beer";
    assert(!oneTea.next({"tea",""},a));

    // Actual source deck: every theme has the expected cards, without cross-contamination.
    BuiltinCatalog builtin;
    DeckSource real(builtin,17);
    for(const auto& expected:std::vector<std::pair<std::string,unsigned>>{{"humor_it",6},{"beer",6},{"tea",6}}) {
        std::set<std::string> seen;
        for(unsigned i=0;i<expected.second;++i) {
            assert(real.next({expected.first,""},a));
            assert(seen.insert(a.id).second);
            assert(expected.first=="humor_it" ? (a.topic=="life" || a.topic=="it") : a.topic==expected.first);
        }
    }

    DeckSource source(catalog,99); Controller app(source);
    assert(app.view.theme==Theme::HumorIT);
    assert(!app.changeTheme(0));
    assert(app.changeTheme(1)); assert(app.view.theme==Theme::Beer);
    assert(app.changeTheme(1)); assert(app.view.theme==Theme::Tea);
    assert(app.changeTheme(1)); assert(app.view.theme==Theme::Tarot);
    assert(app.changeTheme(1)); assert(app.view.theme==Theme::HumorIT);
    assert(app.changeTheme(-1)); assert(app.view.theme==Theme::Tarot);
    assert(app.changeTheme(-1)); assert(app.view.theme==Theme::Tea);
    assert(app.changeTheme(-1)); assert(app.view.theme==Theme::Beer);
    assert(app.changeTheme(-1)); assert(app.view.theme==Theme::HumorIT);
    assert(app.changeTheme(-1)); assert(app.view.theme==Theme::Tarot);
    assert(app.changeTheme(-1)); assert(app.view.theme==Theme::Tea);
    assert(app.view.screen==Screen::Idle);
    assert(app.trigger(100)); assert(app.view.result.topic=="tea");
    assert(!app.changeTheme(1)); assert(app.view.theme==Theme::Tea);
    assert(app.tick(2700)); assert(!app.changeTheme(-1));
    app.back(); assert(app.view.theme==Theme::Tea);
    assert(app.changeTheme(1)); assert(app.view.theme==Theme::Tarot);
    assert(app.changeTheme(1)); assert(app.view.theme==Theme::HumorIT);
    assert(app.trigger(2800)); assert(app.view.result.topic=="life" || app.view.result.topic=="it");

    KeyboardEdges keys;
    assert(homeAction(keys.update(kRightKey))==HomeAction::NextTheme);
    assert(homeAction(keys.update(kRightKey))==HomeAction::None);
    assert(homeAction(keys.update(0))==HomeAction::None);
    const auto shift=keyBit(2,1),enter=keyBit(2,13);
    assert(homeAction(keys.update(kLeftKey|shift))==HomeAction::PreviousTheme);
    assert(homeAction(keys.update(kLeftKey))==HomeAction::None); // Release is not a press.
    assert(homeAction(keys.update(kLeftKey|enter))==HomeAction::Start);
    assert(homeAction(keys.update(kLeftKey))==HomeAction::None);
    keys.update(0);
    assert(homeAction(keys.update(kLeftKey|kRightKey))==HomeAction::None);
    keys.update(0);
    assert(homeAction(keys.update(shift))==HomeAction::Start);
    assert(keyBit(4,0)==0 && keyBit(0,14)==0);
    std::cout<<"PASS: one card read per indexed draw, stale-index protection, 14,400 interleaved draws, switching guards and keyboard edges\n";
}
