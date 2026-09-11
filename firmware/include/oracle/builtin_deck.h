#pragma once
#include "oracle/locale.h"
#if ORACLE_LANG_ZH
#include "oracle/builtin_deck_zh.h"
#elif ORACLE_LANG_RU
#include "oracle/builtin_deck_ru.h"
#else
#include "oracle/builtin_deck_en.h"
#endif
