#ifndef SLOVAK_DICTIONARY_H
#define SLOVAK_DICTIONARY_H

#include "multi_input.h"

/* 10000 词 SKD1 外部资源；字母表及键盘配置仍保存在 code 区。 */
#define SLOVAK_DICTIONARY_WORD_COUNT 10000UL
#define SLOVAK_DICTIONARY_MAX_WORD_LENGTH 15U
#define SLOVAK_CASE_PAIR_COUNT 17U
#define SLOVAK_EXTENDED_CHARACTER_COUNT 17U
#define SLOVAK_KEYBOARD_PAGE 11U

/** 可直接传给 MultiInputInit/MultiInputSetLanguage 的斯洛伐克语言包。 */
extern code MultiInputLanguagePack SlovakLanguagePack;

#endif /* SLOVAK_DICTIONARY_H */
