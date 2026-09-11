#include "oracle/locale.h"
#include "oracle/tarot.h"
#include "oracle/text_rules.h"
#include "oracle/cyrillic_font.h"
#include "oracle/chinese_font.h"
#include <cstdio>
#include <cstring>

namespace oracle {
const char* arcanaName(unsigned index) {
    static const char* const names[] = {
        ORACLE_I18N("Fool", "Шут", "愚者"), ORACLE_I18N("Magician", "Маг", "魔术师"), ORACLE_I18N("High Priestess", "Жрица", "女祭司"), ORACLE_I18N("Empress", "Императрица", "女皇"), ORACLE_I18N("Emperor", "Император", "皇帝"), ORACLE_I18N("Hierophant", "Иерофант", "教皇"),
        ORACLE_I18N("Lovers", "Влюблённые", "恋人"), ORACLE_I18N("Chariot", "Колесница", "战车"), ORACLE_I18N("Strength", "Сила", "力量"), ORACLE_I18N("Hermit", "Отшельник", "隐者"), ORACLE_I18N("Wheel of Fortune", "Колесо Фортуны", "命运之轮"),
        ORACLE_I18N("Justice", "Правосудие", "正义"), ORACLE_I18N("Hanged Man", "Повешенный", "倒吊人"), ORACLE_I18N("Death", "Смерть", "死神"), ORACLE_I18N("Temperance", "Умеренность", "节制"), ORACLE_I18N("Devil", "Дьявол", "恶魔"),
        ORACLE_I18N("Tower", "Башня", "高塔"), ORACLE_I18N("Star", "Звезда", "星星"), ORACLE_I18N("Moon", "Луна", "月亮"), ORACLE_I18N("Sun", "Солнце", "太阳"), ORACLE_I18N("Judgement", "Суд", "审判"), ORACLE_I18N("World", "Мир", "世界")
    };
    return index < kArcanaCount ? names[index] : "?";
}
bool validSpread(const std::array<unsigned,3>& cards) {
    return cards[0]<kArcanaCount && cards[1]<kArcanaCount && cards[2]<kArcanaCount &&
        cards[0]!=cards[1] && cards[0]!=cards[2] && cards[1]!=cards[2];
}
std::array<unsigned,3> drawArcana(std::uint32_t seed) {
    if(!seed) seed=1;
    std::array<unsigned,kArcanaCount> deck{};
    for(unsigned i=0;i<deck.size();++i) deck[i]=i;
    for(unsigned i=0;i<3;++i) {
        seed^=seed<<13; seed^=seed>>17; seed^=seed<<5;
        std::swap(deck[i],deck[i+seed%(kArcanaCount-i)]);
    }
    return {deck[0],deck[1],deck[2]};
}
bool validQuestion(const std::string& question) {
    if(question.size()>kQuestionMaxBytes || hermes_terminal::utf8Columns(question.data(),question.size())>(kChinese?160:80)) return false;
    unsigned characters=0;
    for(std::size_t i=0;i<question.size();) {
        if(++characters>80) return false;
        std::uint32_t cp; i+=hermes_terminal::decodeUtf8(question.data()+i,question.size()-i,cp);
        std::uint16_t rows[12];
        if(!(cp>=32 && cp<127) && hermes_terminal::cyrillicGlyphIndex(cp)<0 && !(hermes_terminal::isChinese(cp) && loadChineseGlyph(cp,rows))) return false;
    }
    return true;
}
bool appendQuestionKey(std::string& question,char key,bool russian) {
    if(key<32 || key>=127 || hermes_terminal::utf8Columns(question.data(),question.size())>=80) return false;
    std::uint32_t cp=static_cast<unsigned char>(key);
    if(russian) {
        const char* latin="qwertyuiop[]asdfghjkl;'zxcvbnm,.QWERTYUIOP{}ASDFGHJKL:\"ZXCVBNM<>";
        const char* letters="йцукенгшщзхъфывапролджэячсмитьбюЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ";
        const auto match=std::strchr(latin,key);
        if(match) {
            std::size_t offset=0;
            for(std::ptrdiff_t n=0;n<=match-latin;++n) offset+=hermes_terminal::decodeUtf8(letters+offset,std::strlen(letters)-offset,cp);
        }
    }
    if(cp<128) question+=char(cp);
    else { question+=char(0xC0|(cp>>6)); question+=char(0x80|(cp&0x3F)); }
    return true;
}
void eraseQuestionKey(std::string& question) {
    if(question.empty()) return;
    auto last=question.size()-1;
    while(last && (static_cast<unsigned char>(question[last])&0xC0)==0x80) --last;
    question.resize(last);
}
bool makeTarotPrediction(const std::array<unsigned,3>& cards, const std::string& reading,
                         std::uint32_t id, Prediction& out,const std::string& question) {
    if(!validSpread(cards) || !validQuestion(question) || reading.empty() || reading.size()>1600) return false;
    std::string clean;
    unsigned count=0;
    for(std::size_t i=0;i<reading.size();) {
        std::uint32_t cp;
        const auto bytes=hermes_terminal::decodeUtf8(reading.data()+i,reading.size()-i,cp);
        if(++count>(kChinese?100:240)) return false;
        if(cp=='\n' || cp=='\r' || cp=='\t') clean+=' ';
        else if(cp>=32 && cp<127) clean+=char(cp);
        else if(hermes_terminal::cyrillicGlyphIndex(cp)>=0) clean.append(reading,i,bytes);
        else if(hermes_terminal::isChinese(cp)) {
            std::uint16_t rows[12]; if(!loadChineseGlyph(cp,rows)) return false;
            clean.append(reading,i,bytes);
        }
        else if(const auto replacement=hermes_terminal::asciiFallback(cp)) clean+=replacement;
        else return false;
        i+=bytes;
    }
    const auto first=clean.find_first_not_of(' '), last=clean.find_last_not_of(' ');
    if(first==std::string::npos || count<20) return false;
    clean=clean.substr(first,last-first+1);
    char name[25]; std::snprintf(name,sizeof(name),"tarot_%08lx",static_cast<unsigned long>(id));
    Prediction result{name,ORACLE_I18N("Tarot: three cards", "Таро: три карты", "塔罗：三张牌"),"","tarot","","",cards[2]%4};
    result.tarotCards=cards;
    result.question=question;
    const char* positions[]={ORACLE_I18N("Past: ", "Было: ", "过去： "),ORACLE_I18N("Present: ", "Сейчас: ", "现在： "),ORACLE_I18N("Future: ", "Впереди: ", "未来： ")};
    for(unsigned i=0;i<3;++i) result.text+=std::string(positions[i])+arcanaName(cards[i])+".\n";
    result.text+='\n'; result.text+=clean;
    // 29 readable 21-pixel lines plus a full-width drawing fit a BQ95 receipt.
    const auto questionHeight=question.empty()?0:wrap(ORACLE_I18N("Question: ", "Вопрос: ", "问题： ")+question,28).size()*(kChinese?26:18)+12;
    if(!validPrediction(result) || 260+wrap(result.text,19).size()*(kChinese?38:26)+questionHeight>1024) return false;
    out=std::move(result); return true;
}
}
