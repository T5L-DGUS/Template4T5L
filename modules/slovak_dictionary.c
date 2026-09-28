#include "slovak_dictionary.h"

/* 预组合扩展字符的小写/大写对应关系。 */
static code MultiInputCasePair SlovakCasePairs[SLOVAK_CASE_PAIR_COUNT] = {
    {0x00E1U, 0x00C1U}, {0x00E4U, 0x00C4U}, {0x010DU, 0x010CU},
    {0x010FU, 0x010EU}, {0x00E9U, 0x00C9U}, {0x00EDU, 0x00CDU},
    {0x013AU, 0x0139U}, {0x013EU, 0x013DU}, {0x0148U, 0x0147U},
    {0x00F3U, 0x00D3U}, {0x00F4U, 0x00D4U}, {0x0155U, 0x0154U},
    {0x0161U, 0x0160U}, {0x0165U, 0x0164U}, {0x00FAU, 0x00DAU},
    {0x00FDU, 0x00DDU}, {0x017EU, 0x017DU}
};

/* F200..F210 依次输入 á ä č ď é í ĺ ľ ň ó ô ŕ š ť ú ý ž。 */
static code uint16_t SlovakExtendedCharacters[SLOVAK_EXTENDED_CHARACTER_COUNT] = {
    0x00E1U, 0x00E4U, 0x010DU, 0x010FU, 0x00E9U, 0x00EDU,
    0x013AU, 0x013EU, 0x0148U, 0x00F3U, 0x00F4U, 0x0155U,
    0x0161U, 0x0165U, 0x00FAU, 0x00FDU, 0x017EU
};

/* 单词及前缀索引由 scripts/build_slovak_dictionary.py 生成到外部 NOR。 */
code MultiInputLanguagePack SlovakLanguagePack = {
    SlovakCasePairs,
    SlovakExtendedCharacters,
    0,
    0,
    SLOVAK_DICTIONARY_WORD_COUNT,
    SLOVAK_CASE_PAIR_COUNT,
    SLOVAK_EXTENDED_CHARACTER_COUNT,
    SLOVAK_DICTIONARY_MAX_WORD_LENGTH,
    SLOVAK_KEYBOARD_PAGE,
    MULTI_INPUT_DICTIONARY_FLASH,
    1U
};
