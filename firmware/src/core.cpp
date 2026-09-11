#include "oracle/locale.h"
#include "oracle/core.h"
#include "oracle/text_rules.h"
#include "oracle/builtin_deck.h"
#include <algorithm>
#include <numeric>

namespace oracle {
const char* themeKey(Theme theme) {
    switch(theme) {
        case Theme::Beer: return "beer";
        case Theme::Tea: return "tea";
        case Theme::Tarot: return "tarot";
        default: return "humor_it";
    }
}
const char* themeLabel(Theme theme) {
    switch(theme) {
        case Theme::Beer: return ORACLE_I18N("BEER", "ПИВНАЯ", "啤酒");
        case Theme::Tea: return ORACLE_I18N("TEA", "ЧАЙНАЯ", "茶");
        case Theme::Tarot: return ORACLE_I18N("TAROT / AI", "ТАРО / AI", "塔罗 / AI");
        default: return ORACLE_I18N("TECH & EVERYDAY", "АЙТИ И ЮМОР", "科技与日常");
    }
}
namespace {
bool matchesTheme(const Prediction& p, Theme theme) {
    if(theme==Theme::Beer) return p.topic=="beer";
    if(theme==Theme::Tea) return p.topic=="tea";
    return p.topic.empty() || p.topic=="life" || p.topic=="it";
}
}
bool validId(const std::string& id) {
    if (id.empty() || id.size() > 24) return false;
    for (unsigned char c : id) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}
bool validAssetPath(const std::string& path, const char* extension) {
    if (path.empty()) return true;
    const std::string ext(extension);
    if (path.size() > 96 || path.find("/ORACLE/") != 0 || path.find("..") != std::string::npos || path.size() < ext.size() || path.substr(path.size()-ext.size()) != ext) return false;
    for (unsigned char c : path) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '_' || c == '-' || c == '.')) return false;
    return true;
}
bool validPrediction(const Prediction& p) {
    return validId(p.id) && !p.title.empty() && p.title.size() <= 48 &&
        !p.text.empty() && p.text.size() <= 1400 && p.topic.size() <= 48 && p.question.size()<=kQuestionMaxBytes && p.motif < 4 &&
        validAssetPath(p.image, ".jpg") && validAssetPath(p.audio, ".wav");
}
std::string receiptContent(const Prediction& p) {
    auto content=p.title+"\n"+p.text+"\n"+p.image;
    if(p.topic=="tarot") {
        content+="\n"+p.question;
        for(auto card:p.tarotCards) content+="\n"+std::to_string(card);
    }
    return content;
}
std::size_t BuiltinCatalog::size() const { return sizeof(kBuiltinDeck) / sizeof(kBuiltinDeck[0]); }
bool Catalog::matches(std::size_t index,Theme theme) {
    Prediction candidate;
    return load(index,candidate) && validPrediction(candidate) && matchesTheme(candidate,theme);
}
bool BuiltinCatalog::load(std::size_t i, Prediction& out) {
    if (i >= size()) return false;
    const auto& p = kBuiltinDeck[i];
    out = {p.id, p.title, p.text, p.topic, "", "", p.motif};
    return true;
}
std::uint32_t DeckSource::random() {
    state_ ^= state_ << 13; state_ ^= state_ >> 17; state_ ^= state_ << 5;
    return state_;
}
bool DeckSource::next(const Request& request, Prediction& out) {
    // A random card can match a theme, but must not pretend to answer a free-form question.
    Theme theme=Theme::HumorIT;
    if(request.topic=="beer") theme=Theme::Beer;
    else if(request.topic=="tea") theme=Theme::Tea;
    else if(!request.topic.empty() && request.topic!="humor_it") return false;
    if (!request.question.empty() || !catalog_.size() || catalog_.size() > 256) return false;
    auto& state=bags_[static_cast<unsigned>(theme)];
    auto& bag=state.cards;
    if (bag.empty()) {
        for(std::size_t i=0;i<catalog_.size();++i) {
            if(catalog_.matches(i,theme)) bag.push_back(i);
        }
        for (std::size_t i = bag.size(); i > 1; --i) std::swap(bag[i-1], bag[random() % i]);
        if (bag.size() > 1 && bag.back() == state.last) std::swap(bag.front(), bag.back());
    }
    while (!bag.empty()) {
        const auto i = bag.back(); bag.pop_back();
        Prediction candidate;
        if (catalog_.load(i, candidate) && validPrediction(candidate) && matchesTheme(candidate,theme)) {
            out = candidate; state.last = i; return true;
        }
    }
    return false;
}
std::vector<std::string> wrap(const std::string& text, unsigned columns) {
    std::vector<std::string> rows;
    hermes_terminal::wrapMonospace(text.data(), text.size(), columns,
        [&](const char* row, std::size_t size) { rows.emplace_back(row, size); });
    return rows;
}
bool Controller::changeTheme(int direction) {
    if(view.screen!=Screen::Idle || direction==0) return false;
    const auto step=direction>0 ? 1u : kThemeCount-1;
    view.theme=static_cast<Theme>((static_cast<unsigned>(view.theme)+step)%kThemeCount);
    view.notice.clear();
    return true;
}
bool Controller::trigger(std::uint32_t now) {
    if (view.screen == Screen::Ritual || view.screen == Screen::Generating) return false;
    if (view.theme == Theme::Tarot) {
        view.result = {}; view.lines.clear(); view.scroll = 0; view.showArt = false;
        view.started = now; view.screen = Screen::Question; view.notice.clear();
        return true;
    }
    if (!source_.next({themeKey(view.theme),""}, view.result)) {
        view.screen = Screen::Error; view.notice = ORACLE_I18N("No cards in this theme", "В этой теме нет карточек", "此主题无卡牌"); return false;
    }
    view.lines = wrap(view.result.text, kColumns);
    view.showArt = false; view.scroll = 0; view.started = now; view.screen = Screen::Ritual; view.notice.clear();
    return true;
}
bool Controller::submitQuestion(std::uint32_t now) {
    if(view.screen!=Screen::Question || view.theme!=Theme::Tarot) return false;
    view.started=now; view.screen=Screen::Generating; view.notice.clear(); return true;
}
bool Controller::acceptRemote(const Prediction& result, std::uint32_t now) {
    if(view.screen != Screen::Generating || view.theme != Theme::Tarot ||
       result.topic != "tarot" || !validPrediction(result)) return false;
    view.result = result;
    view.lines = wrap((result.question.empty()?"":ORACLE_I18N("Question: ", "Вопрос: ", "问题： ")+result.question+"\n\n")+result.text, kColumns);
    view.notice.clear(); view.started = now; view.screen = Screen::Ritual;
    return true;
}
void Controller::failRemote(const std::string& message) {
    if(view.screen != Screen::Generating) return;
    view.screen = Screen::Error; view.notice = message;
}
bool Controller::tick(std::uint32_t now) {
    view.frame = now;
    if (view.screen == Screen::Ritual && std::uint32_t(now-view.started) >= 2600) {
        view.screen = Screen::Result;
        return true;
    }
    return false;
}
void Controller::back() { view.screen = Screen::Idle; view.notice.clear(); }
void Controller::scroll(int delta) {
    if (view.screen != Screen::Result) return;
    int max = std::max(0, int(view.lines.size()) - int(kVisibleLines));
    view.scroll = unsigned(std::max(0, std::min(max, int(view.scroll) + delta)));
}
}
