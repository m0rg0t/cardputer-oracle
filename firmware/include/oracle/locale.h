#pragma once
#include <cstring>

// Select at compile time so the unused UI strings never occupy firmware flash.
#ifndef ORACLE_LANG_RU
#define ORACLE_LANG_RU 0
#endif
#ifndef ORACLE_LANG_ZH
#define ORACLE_LANG_ZH 0
#endif
#if ORACLE_LANG_ZH != 0 && ORACLE_LANG_ZH != 1
#error ORACLE_LANG_ZH must be 0 or 1
#endif
#if ORACLE_LANG_ZH && ORACLE_LANG_RU
#error Select exactly one language
#endif
#if ORACLE_LANG_RU != 0 && ORACLE_LANG_RU != 1
#error ORACLE_LANG_RU must be 0 (English) or 1 (Russian)
#endif
#if ORACLE_LANG_RU
#define ORACLE_TEXT(en, ru) ru
#else
#define ORACLE_TEXT(en, ru) en
#endif
#if ORACLE_LANG_ZH
#define ORACLE_I18N(en, ru, zh) zh
#else
#define ORACLE_I18N(en, ru, zh) ORACLE_TEXT(en, ru)
#endif
namespace oracle {
constexpr const char* kLanguage = ORACLE_I18N("en", "ru", "zh");
constexpr bool kRussian = ORACLE_LANG_RU == 1;
constexpr bool kChinese = ORACLE_LANG_ZH == 1;
constexpr unsigned kQuestionMaxBytes = kChinese ? 240 : 160;
constexpr const char* kBuildLanguageMarker = ORACLE_I18N("ORACLE_BUILD_LANGUAGE=en", "ORACLE_BUILD_LANGUAGE=ru", "ORACLE_BUILD_LANGUAGE=zh");
inline bool matchesPackLanguage(const char* language) {
    // Legacy untagged SD packs were Russian; never load them in an English build.
    return language ? std::strcmp(language,kLanguage)==0 : kRussian;
}
}
