#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace oracle {
enum class Theme { HumorIT, Beer, Tea, Tarot };
constexpr unsigned kThemeCount = 4;
const char* themeKey(Theme theme);
const char* themeLabel(Theme theme);
struct Request { std::string topic, question; }; // Free-form questions remain reserved.
struct Prediction {
    std::string id, title, text, topic;
    std::string image, audio; // Optional absolute SD paths, empty in the starter pack.
    unsigned motif = 0;
    std::array<unsigned,3> tarotCards{};
    std::string question{};
};
bool validPrediction(const Prediction& p);
std::string receiptContent(const Prediction& p);
bool validId(const std::string& id);
bool validAssetPath(const std::string& path, const char* extension);

class Catalog {
public:
    virtual ~Catalog() = default;
    virtual std::size_t size() const = 0;
    virtual bool load(std::size_t index, Prediction& out) = 0;
    virtual bool matches(std::size_t index, Theme theme);
};
class BuiltinCatalog final : public Catalog {
public:
    std::size_t size() const override;
    bool load(std::size_t index, Prediction& out) override;
};
class PredictionSource {
public:
    virtual ~PredictionSource() = default;
    virtual bool next(const Request& request, Prediction& out) = 0;
};
// An engine returns a frozen result. Screen, audio and future printer all consume it.
class DeckSource final : public PredictionSource {
public:
    DeckSource(Catalog& catalog, std::uint32_t seed) : catalog_(catalog), state_(seed ? seed : 1) {}
    bool next(const Request& request, Prediction& out) override;
private:
    std::uint32_t random();
    Catalog& catalog_;
    std::uint32_t state_;
    struct Bag {
        std::vector<std::size_t> cards;
        std::size_t last = static_cast<std::size_t>(-1);
    };
    std::array<Bag, kThemeCount> bags_;
};
enum class PrintStatus { Unavailable, Ready, Searching, Connecting, Sending, Sent, Uncertain, Failed };
class Printer {
public:
    virtual ~Printer() = default;
    virtual PrintStatus status() const = 0;
    virtual bool submit(const Prediction& result) = 0;
};
class NoPrinter final : public Printer {
public:
    PrintStatus status() const override { return PrintStatus::Unavailable; }
    bool submit(const Prediction&) override { return false; }
};
enum class Screen { Idle, Question, Ritual, Generating, Result, Error };
struct View {
    Screen screen = Screen::Idle;
    Theme theme = Theme::HumorIT;
    Prediction result;
    std::vector<std::string> lines;
    std::string notice;
    std::string question;
    bool russianInput=true;
    std::string printNotice;
    PrintStatus printStatus = PrintStatus::Unavailable;
    bool sd = false, muted = false, playing = false, showArt = false;
    int batteryPercent = -1; // Unknown until the board reports a valid reading.
    unsigned scroll = 0;
    std::uint32_t started = 0, frame = 0;
};
class Controller {
public:
    explicit Controller(PredictionSource& source) : source_(source) {}
    View view;
    bool changeTheme(int direction); // Only on the home screen; preserves each deck's progress.
    bool trigger(std::uint32_t now);
    bool submitQuestion(std::uint32_t now);
    bool acceptRemote(const Prediction& result, std::uint32_t now);
    void failRemote(const std::string& message);
    bool tick(std::uint32_t now); // true exactly when the result is revealed
    void back();
    void scroll(int delta);
private:
    PredictionSource& source_;
};
constexpr unsigned kVisibleLines = 4;
constexpr unsigned kColumns = 18;
std::vector<std::string> wrap(const std::string& text, unsigned columns);
}
