#include "oracle/core.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
    using namespace oracle;
    assert(View{}.russianInput==kRussian);
    assert(matchesPackLanguage(kLanguage));
    assert(!matchesPackLanguage(kRussian?"en":"ru"));
    assert(!matchesPackLanguage("de"));
    assert(!matchesPackLanguage(""));
    assert(matchesPackLanguage(nullptr)==kRussian);
    assert(std::strcmp(themeLabel(Theme::Tea),ORACLE_I18N("TEA","ЧАЙНАЯ","茶"))==0);
    BuiltinCatalog catalog;
    assert(catalog.size()==18);
    for(unsigned i=0;i<catalog.size();++i) {
        Prediction p; assert(catalog.load(i,p)); assert(validPrediction(p));
        bool nonAscii=false;
        for(unsigned char c:p.text) nonAscii|=c>=128;
        assert(nonAscii==(kRussian || kChinese));
    }
    std::cout<<"PASS: "<<kLanguage<<" fallback, keyboard default and wrong-language SD rejection\n";
}
