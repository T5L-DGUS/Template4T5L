/**
 * @file multi_input.c
 * @brief 拉丁系单行输入法、编辑器与候选词底座。
 *
 * DGUS 通过 0x0710 发起输入会话，并把目标文本 VP 作为按键返回值传入；
 * 0x0711 保存本次会话允许的最大字符数。键盘页随后把所有触摸事件写到
 * 0x0700。本模块以 20 ms 周期消费并清零事件，在 xdata 中维护不含光标
 * 标记的 UTF-16 正文，确认时才写回目标 VP。
 *
 * DGUS VP 的一个 word 使用高字节在前的 UTF-16BE 格式。为避免依赖 8051
 * 本机整数端序，本文件的所有 VP 读写均显式拆分或合并高、低字节。
 */
#include "multi_input.h"
#include "flash_dictionary.h"

/* 原键盘保留的控制键事件。 */
#define MULTI_INPUT_KEY_ESCAPE           0x00F0U
#define MULTI_INPUT_KEY_OK               0x00F1U
#define MULTI_INPUT_KEY_BACKSPACE        0x00F2U
#define MULTI_INPUT_KEY_DELETE           0x00F3U
#define MULTI_INPUT_KEY_CAPS             0x00F4U
#define MULTI_INPUT_KEY_LEFT             0x00F7U
#define MULTI_INPUT_KEY_RIGHT            0x00F8U
#define MULTI_INPUT_KEY_ENTER_PAIR       0x0D0DU
#define MULTI_INPUT_KEY_ENTER            0x000DU

/* 候选键与语言扩展键使用独立事件区，避免与控制键值冲突。 */
#define MULTI_INPUT_CANDIDATE_KEY_BASE   0xF101U
#define MULTI_INPUT_EXTENDED_KEY_BASE    0xF200U
#define MULTI_INPUT_KEY_LANGUAGE_SWITCH  0xF300U

/* 候选词输出的三种大小写形式。 */
#define MULTI_INPUT_CASE_LOWER           0U
#define MULTI_INPUT_CASE_TITLE           1U
#define MULTI_INPUT_CASE_UPPER           2U

/** 输入法的完整运行状态，正文始终不包含用于预览的可视光标。 */
typedef struct
{
    MultiInputLanguagePack code *language;
    MultiInputLanguagePack code *secondary_language;
    uint16_t buffer[MULTI_INPUT_MAX_LENGTH + 1U];
    uint16_t candidate_words[MULTI_INPUT_CANDIDATE_COUNT][MULTI_INPUT_CANDIDATE_MAX_LENGTH + 1U];
    uint8_t dictionary_prefix[MULTI_INPUT_CANDIDATE_MAX_LENGTH];
    uint32_t candidate_page;
    uint32_t candidate_total;
    uint8_t candidate_count;
    uint8_t dictionary_prefix_length;
    uint8_t dictionary_result_state;
    uint8_t vp_bytes[MULTI_INPUT_PREVIEW_WORD_COUNT * 2U];
    uint16_t target_vp;
    uint16_t source_page;
    uint8_t length;
    uint8_t max_length;
    uint8_t cursor;
    uint8_t active;
    uint8_t caps;
    uint8_t full;
    uint8_t candidate_case;
} MultiInputContextType;

static MultiInputContextType xdata MultiInputContext;

/* 状态栏仅使用 ASCII，字库缺失时仍能显示容量与大小写状态。 */
code uint8_t MultiInputFullText[] = "FULL ";
code uint8_t MultiInputCapsText[] = "CAPS ";
code uint8_t MultiInputLowerText[] = "abc ";

/** 从指定 VP 读取一个 DGUS 大端 word。 */
static uint16_t MultiInputReadWord(uint32_t vp)
{
    uint8_t bytes[2];
    read_dgus_vp(vp, bytes, 1U);
    return ((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1];
}

/** 把一个 16 位值显式拆为高低字节后写入指定 VP。 */
static void MultiInputWriteWord(uint32_t vp, uint16_t value)
{
    uint8_t bytes[2];
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
    write_dgus_vp(vp, bytes, 1U);
}

/** 清零共用的 VP 字节暂存区前 byte_count 个字节。 */
static void MultiInputZeroVpBytes(uint16_t byte_count)
{
    uint16_t i;
    for(i = 0U; i < byte_count; i++)
    {
        MultiInputContext.vp_bytes[i] = 0U;
    }
}

/** 将指定数量的 DGUS words 写零，用于清除文本或事件区域。 */
static void MultiInputClearVp(uint32_t vp, uint16_t word_count)
{
    MultiInputZeroVpBytes(word_count * 2U);
    write_dgus_vp(vp, MultiInputContext.vp_bytes, word_count);
}

/** 读取并约束 0x0711；零或超过底座容量时使用默认的 64 字符。 */
static uint8_t MultiInputResolveMaximumLength(void)
{
    uint16_t requested_length;

    requested_length = MultiInputReadWord(MULTI_INPUT_LENGTH_VP);
    if((requested_length == 0U) ||
       (requested_length > MULTI_INPUT_MAX_LENGTH))
    {
        requested_length = MULTI_INPUT_DEFAULT_LENGTH;
    }
    return (uint8_t)requested_length;
}

/** 校验语言描述表，避免后续任务解引用无效的 code 区指针。 */
static uint8_t MultiInputLanguageIsValid(MultiInputLanguagePack code *language)
{
    if(language == 0)
    {
        return 0U;
    }
    if(language->keyboard_page == 0U)
    {
        return 0U;
    }
    if((language->case_pair_count > 0U) && (language->case_pairs == 0))
    {
        return 0U;
    }
    if((language->extended_character_count > 0U) &&
       (language->extended_characters == 0))
    {
        return 0U;
    }
    if(language->dictionary_word_count == 0U)
    {
        return 1U;
    }
    if((language->dictionary_min_prefix == 0U) ||
       (language->dictionary_min_prefix > MULTI_INPUT_CANDIDATE_MAX_LENGTH))
    {
        return 0U;
    }
    if(language->dictionary_backend == MULTI_INPUT_DICTIONARY_FLASH)
    {
        return (uint8_t)((language->dictionary_max_word_length == 15U) &&
                         (language->extended_character_count == 17U) &&
                         (language->dictionary_word_count == 10000UL));
    }
    if((language->dictionary_backend != MULTI_INPUT_DICTIONARY_CODE) ||
       (language->dictionary_word_count > 65535UL))
    {
        return 0U;
    }
    if((language->dictionary_pool == 0) ||
       (language->dictionary_offsets == 0))
    {
        return 0U;
    }
    if((language->dictionary_max_word_length == 0U) ||
       (language->dictionary_max_word_length > MULTI_INPUT_CANDIDATE_MAX_LENGTH))
    {
        return 0U;
    }
    return 1U;
}

/** 将 ASCII 与当前语言包中的预组合大写字母映射为小写。 */
static uint16_t MultiInputToLower(uint16_t value)
{
    uint8_t i;

    if((value >= 0x0041U) && (value <= 0x005AU))
    {
        return value + 0x0020U;
    }

    for(i = 0U; i < MultiInputContext.language->case_pair_count; i++)
    {
        if(MultiInputContext.language->case_pairs[i].upper == value)
        {
            return MultiInputContext.language->case_pairs[i].lower;
        }
    }
    return value;
}

/** 将 ASCII 与当前语言包中的预组合小写字母映射为大写。 */
static uint16_t MultiInputToUpper(uint16_t value)
{
    uint8_t i;

    if((value >= 0x0061U) && (value <= 0x007AU))
    {
        return value - 0x0020U;
    }

    for(i = 0U; i < MultiInputContext.language->case_pair_count; i++)
    {
        if(MultiInputContext.language->case_pairs[i].lower == value)
        {
            return MultiInputContext.language->case_pairs[i].upper;
        }
    }
    return value;
}

/** 判断字符是否为 ASCII 字母或当前语言包声明的扩展字母。 */
static uint8_t MultiInputIsLetter(uint16_t value)
{
    uint16_t lower;
    uint8_t i;

    lower = MultiInputToLower(value);

    if((lower >= 0x0061U) && (lower <= 0x007AU))
    {
        return 1U;
    }

    for(i = 0U; i < MultiInputContext.language->case_pair_count; i++)
    {
        if(MultiInputContext.language->case_pairs[i].lower == lower)
        {
            return 1U;
        }
    }
    return 0U;
}

/** 按小写、首字母大写或全大写规则转换候选词中的一个字符。 */
static uint16_t MultiInputApplyCase(uint16_t value, uint8_t style, uint8_t position)
{
    if(style == MULTI_INPUT_CASE_UPPER)
    {
        return MultiInputToUpper(value);
    }
    if((style == MULTI_INPUT_CASE_TITLE) && (position == 0U))
    {
        return MultiInputToUpper(value);
    }
    return MultiInputToLower(value);
}

/**
 * 将带可视光标的预览写到 0x0780。
 *
 * 竖线只存在于显示副本中，不进入正文缓冲区，也不会在确认时写入目标 VP。
 * 正文最多 64 字符，因此预览控件使用 65 words 容纳正文和光标。
 */
static void MultiInputWritePreview(void)
{
    uint8_t source;
    uint8_t display;
    uint16_t value;

    MultiInputZeroVpBytes(MULTI_INPUT_PREVIEW_WORD_COUNT * 2U);
    display = 0U;
    for(source = 0U; source < MultiInputContext.length; source++)
    {
        if(source == MultiInputContext.cursor)
        {
            /* 在光标所指正文字符之前插入可见的 U+007C。 */
            MultiInputContext.vp_bytes[(uint16_t)display * 2U] = 0U;
            MultiInputContext.vp_bytes[(uint16_t)display * 2U + 1U] = '|';
            display++;
        }
        value = MultiInputContext.buffer[source];
        MultiInputContext.vp_bytes[(uint16_t)display * 2U] = (uint8_t)(value >> 8);
        MultiInputContext.vp_bytes[(uint16_t)display * 2U + 1U] = (uint8_t)value;
        display++;
    }
    if(MultiInputContext.cursor == MultiInputContext.length)
    {
        /* 光标位于末尾时，循环不会插入竖线，需要在这里补上。 */
        MultiInputContext.vp_bytes[(uint16_t)display * 2U] = 0U;
        MultiInputContext.vp_bytes[(uint16_t)display * 2U + 1U] = '|';
    }
    write_dgus_vp(MULTI_INPUT_BUFFER_VP, MultiInputContext.vp_bytes,
                  MULTI_INPUT_PREVIEW_WORD_COUNT);
}

/** 按 0x0711 指定的容量，将规范正文写回本次会话的动态目标 VP。 */
static void MultiInputWriteTarget(void)
{
    uint8_t i;
    MultiInputZeroVpBytes((uint16_t)MultiInputContext.max_length * 2U);
    for(i = 0U; i < MultiInputContext.length; i++)
    {
        MultiInputContext.vp_bytes[(uint16_t)i * 2U] = (uint8_t)(MultiInputContext.buffer[i] >> 8);
        MultiInputContext.vp_bytes[(uint16_t)i * 2U + 1U] = (uint8_t)MultiInputContext.buffer[i];
    }
    write_dgus_vp(MultiInputContext.target_vp, MultiInputContext.vp_bytes,
                  MultiInputContext.max_length);
}

/** 向 16-word 状态暂存区追加一个 ASCII 字符，并返回下一写入位置。 */
static uint8_t MultiInputAppendStatusCharacter(uint8_t position, uint8_t value)
{
    if(position < 15U)
    {
        MultiInputContext.vp_bytes[(uint16_t)position * 2U] = 0U;
        MultiInputContext.vp_bytes[(uint16_t)position * 2U + 1U] = value;
        position++;
    }
    return position;
}

/** 从 8051 code 区向状态暂存区追加一个零结尾 ASCII 字符串。 */
static uint8_t MultiInputAppendStatusText(uint8_t position, uint8_t code *text)
{
    while((*text != 0U) && (position < 15U))
    {
        position = MultiInputAppendStatusCharacter(position, *text);
        text++;
    }
    return position;
}

/** 向状态暂存区追加 0..99 的无前导零十进制表示。 */
static uint8_t MultiInputAppendStatusNumber(uint8_t position, uint8_t value)
{
    if(value >= 10U)
    {
        position = MultiInputAppendStatusCharacter(position, (uint8_t)('0' + value / 10U));
    }
    return MultiInputAppendStatusCharacter(position, (uint8_t)('0' + value % 10U));
}

/** 输出“FULL/CAPS/abc 光标/长度”状态；最多写入 15 个可见字符。 */
static void MultiInputWriteStatus(void)
{
    uint8_t position;
    MultiInputZeroVpBytes(32U);
    position = 0U;
    if(MultiInputContext.full)
    {
        position = MultiInputAppendStatusText(position, MultiInputFullText);
    }
    if(MultiInputContext.caps)
    {
        position = MultiInputAppendStatusText(position, MultiInputCapsText);
    }
    else
    {
        position = MultiInputAppendStatusText(position, MultiInputLowerText);
    }
    position = MultiInputAppendStatusNumber(position, MultiInputContext.cursor);
    position = MultiInputAppendStatusCharacter(position, '/');
    MultiInputAppendStatusNumber(position, MultiInputContext.length);
    write_dgus_vp(MULTI_INPUT_STATUS_VP, MultiInputContext.vp_bytes, 16U);
}

/** 两种后端都先将当前页复制到候选缓存，提交不再依赖异步 Flash。 */
static uint8_t MultiInputDictionaryWordLength(uint8_t slot)
{
    uint8_t length;
    length = 0U;
    while((length < MultiInputContext.language->dictionary_max_word_length) &&
          (MultiInputContext.candidate_words[slot][length] != 0U))
    {
        length++;
    }
    return length;
}

/** 将指定候选槽按当前前缀的大小写形式输出到相应候选 VP。 */
static void MultiInputWriteCandidate(uint8_t slot)
{
    uint32_t vp;
    uint16_t value;
    uint8_t i;

    /* 四个候选 VP 的固定步长为 0x10 words。 */
    vp = (uint32_t)MULTI_INPUT_CANDIDATE1_VP + ((uint32_t)slot * 0x10UL);
    MultiInputZeroVpBytes(32U);

    if(slot < MultiInputContext.candidate_count)
    {
        for(i = 0U; i < MultiInputContext.language->dictionary_max_word_length; i++)
        {
            value = MultiInputContext.candidate_words[slot][i];
            if(value == 0U)
            {
                break;
            }
            value = MultiInputApplyCase(value, MultiInputContext.candidate_case, i);
            MultiInputContext.vp_bytes[(uint16_t)i * 2U] = (uint8_t)(value >> 8);
            MultiInputContext.vp_bytes[(uint16_t)i * 2U + 1U] = (uint8_t)value;
        }
    }
    write_dgus_vp(vp, MultiInputContext.vp_bytes, 16U);
}

/** 从光标向两侧扫描字母，得到光标所在完整单词的半开区间 [start, end)。 */
static void MultiInputFindWordBounds(uint8_t *start, uint8_t *end)
{
    *start = MultiInputContext.cursor;
    while((*start > 0U) && MultiInputIsLetter(MultiInputContext.buffer[*start - 1U]))
    {
        (*start)--;
    }

    *end = MultiInputContext.cursor;
    while((*end < MultiInputContext.length) && MultiInputIsLetter(MultiInputContext.buffer[*end]))
    {
        (*end)++;
    }
}

/** 对候选和光标左侧前缀执行不区分大小写、但区分变音字母的精确匹配。 */
static uint8_t MultiInputPrefixMatches(uint16_t offset, uint8_t start, uint8_t prefix_length)
{
    uint8_t i;
    uint16_t dictionary_value;
    for(i = 0U; i < prefix_length; i++)
    {
        dictionary_value = MultiInputContext.language->dictionary_pool[offset + i];
        if((dictionary_value == 0U) ||
           (MultiInputToLower(dictionary_value) !=
            MultiInputToLower(MultiInputContext.buffer[start + i])))
        {
            return 0U;
        }
    }
    return 1U;
}

/** 根据已输入前缀识别小写、首字母大写或全大写候选输出形式。 */
static uint8_t MultiInputDeterminePrefixCase(uint8_t start, uint8_t prefix_length)
{
    uint8_t i;
    uint8_t upper_count;
    upper_count = 0U;

    for(i = 0U; i < prefix_length; i++)
    {
        if(MultiInputToLower(MultiInputContext.buffer[start + i]) !=
           MultiInputContext.buffer[start + i])
        {
            upper_count++;
        }
    }

    if(upper_count == prefix_length)
    {
        return MULTI_INPUT_CASE_UPPER;
    }
    if((upper_count == 1U) &&
       (MultiInputToLower(MultiInputContext.buffer[start]) !=
        MultiInputContext.buffer[start]))
    {
        return MULTI_INPUT_CASE_TITLE;
    }
    return MULTI_INPUT_CASE_LOWER;
}

/** 清除当前页缓存，异步检索期间旧候选不再允许提交。 */
static void MultiInputClearCandidates(void)
{
    uint8_t i;
    uint8_t j;
    MultiInputContext.candidate_count = 0U;
    MultiInputContext.candidate_total = 0UL;
    MultiInputContext.candidate_page = 0UL;
    for(i = 0U; i < MULTI_INPUT_CANDIDATE_COUNT; i++)
    {
        for(j = 0U; j <= MULTI_INPUT_CANDIDATE_MAX_LENGTH; j++)
        {
            MultiInputContext.candidate_words[i][j] = 0U;
        }
    }
}

static uint8_t MultiInputAppendPageNumber(uint8_t position, uint32_t number)
{
    uint8_t digits[10];
    uint8_t count;
    count = 0U;
    do
    {
        digits[count++] = (uint8_t)('0' + number % 10UL);
        number /= 10UL;
    } while(number && count < 10U);
    while(count)
    {
        position = MultiInputAppendStatusCharacter(position, digits[--count]);
    }
    return position;
}

static void MultiInputWritePage(void)
{
    uint8_t position;
    uint32_t pages;
    MultiInputZeroVpBytes(32U);
    position = 0U;
    if(MultiInputContext.language->dictionary_backend == MULTI_INPUT_DICTIONARY_FLASH)
    {
        if(MultiInputContext.dictionary_result_state == FLASH_DICTIONARY_ERROR)
        {
            position = MultiInputAppendStatusCharacter(position, '-');
            position = MultiInputAppendStatusCharacter(position, '-');
            position = MultiInputAppendStatusCharacter(position, '/');
            position = MultiInputAppendStatusCharacter(position, '-');
            MultiInputAppendStatusCharacter(position, '-');
        }
        else if(MultiInputContext.dictionary_result_state == FLASH_DICTIONARY_BUSY)
        {
            position = MultiInputAppendStatusCharacter(position, '.');
            position = MultiInputAppendStatusCharacter(position, '.');
            MultiInputAppendStatusCharacter(position, '.');
        }
        else
        {
            pages = (MultiInputContext.candidate_total + 3UL) / 4UL;
            position = MultiInputAppendPageNumber(position,
                pages ? MultiInputContext.candidate_page + 1UL : 0UL);
            position = MultiInputAppendStatusCharacter(position, '/');
            MultiInputAppendPageNumber(position, pages);
        }
    }
    write_dgus_vp(MULTI_INPUT_PAGE_VP, MultiInputContext.vp_bytes, 16U);
}

/** 仅在异步状态变化时刷新 VP；页面内容在 READY 后一次性发布。 */
static void MultiInputPollFlashCandidates(void)
{
    FlashDictionaryResult xdata *result;
    uint8_t slot;
    uint8_t i;
    uint8_t value;
    if(!MultiInputContext.active || MultiInputContext.language->dictionary_word_count == 0UL ||
       MultiInputContext.language->dictionary_backend != MULTI_INPUT_DICTIONARY_FLASH)
    {
        return;
    }
    result = FlashDictionaryGetResult();
    if(result->state == MultiInputContext.dictionary_result_state)
    {
        return;
    }
    MultiInputContext.dictionary_result_state = result->state;
    MultiInputClearCandidates();
    if(result->state == FLASH_DICTIONARY_READY)
    {
        MultiInputContext.candidate_count = result->count;
        MultiInputContext.candidate_total = result->total;
        MultiInputContext.candidate_page = result->page;
        for(slot = 0U; slot < result->count; slot++)
        {
            for(i = 0U; i < MULTI_INPUT_CANDIDATE_MAX_LENGTH; i++)
            {
                value = result->words[slot][i];
                if(value == 0U)
                {
                    break;
                }
                if(value <= 26U)
                {
                    MultiInputContext.candidate_words[slot][i] = (uint16_t)('a' + value - 1U);
                }
                else
                {
                    MultiInputContext.candidate_words[slot][i] =
                        MultiInputContext.language->extended_characters[value - 27U];
                }
            }
        }
    }
    for(slot = 0U; slot < MULTI_INPUT_CANDIDATE_COUNT; slot++)
    {
        MultiInputWriteCandidate(slot);
    }
    MultiInputWritePage();
}

static void MultiInputRequestFlashPage(uint32_t page)
{
    FlashDictionaryQuery(MultiInputContext.dictionary_prefix,
                         MultiInputContext.dictionary_prefix_length, page);
    MultiInputContext.dictionary_result_state = 0xFFU;
    MultiInputPollFlashCandidates();
}

/** 根据语言后端重建候选；CODE 保留原有前四项，FLASH 可访问全部匹配。 */
static void MultiInputUpdateCandidates(void)
{
    uint32_t dictionary_index;
    uint16_t offset;
    uint16_t value;
    uint8_t start;
    uint8_t end;
    uint8_t prefix_length;
    uint8_t found;
    uint8_t i;
    uint8_t j;
    uint8_t encoded;

    MultiInputClearCandidates();
    if(MultiInputContext.language->dictionary_word_count == 0UL)
    {
        MultiInputContext.dictionary_prefix_length = 0U;
        MultiInputContext.dictionary_result_state = FLASH_DICTIONARY_EMPTY;
        FlashDictionaryQuery(MultiInputContext.dictionary_prefix, 0U, 0UL);
        for(i = 0U; i < MULTI_INPUT_CANDIDATE_COUNT; i++)
        {
            MultiInputWriteCandidate(i);
        }
        MultiInputWritePage();
        return;
    }
    MultiInputFindWordBounds(&start, &end);
    prefix_length = MultiInputContext.cursor - start;
    if(prefix_length > MULTI_INPUT_CANDIDATE_MAX_LENGTH ||
       prefix_length < MultiInputContext.language->dictionary_min_prefix)
    {
        prefix_length = 0U;
    }
    if(prefix_length)
    {
        MultiInputContext.candidate_case = MultiInputDeterminePrefixCase(start, prefix_length);
    }
    if(MultiInputContext.language->dictionary_backend == MULTI_INPUT_DICTIONARY_FLASH)
    {
        for(i = 0U; i < prefix_length; i++)
        {
            value = MultiInputToLower(MultiInputContext.buffer[start + i]);
            encoded = 0U;
            if(value >= 'a' && value <= 'z')
            {
                encoded = (uint8_t)(value - 'a' + 1U);
            }
            else
            {
                for(j = 0U; j < MultiInputContext.language->extended_character_count; j++)
                {
                    if(value == MultiInputContext.language->extended_characters[j])
                    {
                        encoded = 27U + j;
                        break;
                    }
                }
            }
            if(encoded == 0U)
            {
                prefix_length = 0U;
                break;
            }
            MultiInputContext.dictionary_prefix[i] = encoded;
        }
        MultiInputContext.dictionary_prefix_length = prefix_length;
        MultiInputRequestFlashPage(0UL);
        return;
    }

    FlashDictionaryQuery(MultiInputContext.dictionary_prefix, 0U, 0UL);
    found = 0U;
    if(prefix_length)
    {
        for(dictionary_index = 0UL;
            dictionary_index < MultiInputContext.language->dictionary_word_count &&
            found < MULTI_INPUT_CANDIDATE_COUNT; dictionary_index++)
        {
            offset = MultiInputContext.language->dictionary_offsets[(uint16_t)dictionary_index];
            if(MultiInputPrefixMatches(offset, start, prefix_length))
            {
                for(i = 0U; i < MultiInputContext.language->dictionary_max_word_length; i++)
                {
                    value = MultiInputContext.language->dictionary_pool[offset + i];
                    MultiInputContext.candidate_words[found][i] = value;
                    if(value == 0U)
                    {
                        break;
                    }
                }
                found++;
            }
        }
    }
    MultiInputContext.candidate_count = found;
    MultiInputContext.candidate_total = found;
    for(i = 0U; i < MULTI_INPUT_CANDIDATE_COUNT; i++)
    {
        MultiInputWriteCandidate(i);
    }
    MultiInputWritePage();
}

/** 在正文或光标状态变化后同步预览、状态栏和全部候选词。 */
static void MultiInputRefresh(void)
{
    MultiInputWritePreview();
    MultiInputWriteStatus();
    MultiInputUpdateCandidates();
}

/** 在光标处插入一个 UTF-16 字符；达到本次字段容量时置 FULL 并拒绝插入。 */
static uint8_t MultiInputInsertCharacter(uint16_t value)
{
    uint8_t i;
    if(MultiInputContext.length >= MultiInputContext.max_length)
    {
        MultiInputContext.full = 1U;
        return 0U;
    }

    /* 从尾部向右搬移，避免覆盖光标后的原有正文。 */
    i = MultiInputContext.length;
    while(i > MultiInputContext.cursor)
    {
        MultiInputContext.buffer[i] = MultiInputContext.buffer[i - 1U];
        i--;
    }
    MultiInputContext.buffer[MultiInputContext.cursor] = value;
    MultiInputContext.length++;
    MultiInputContext.cursor++;
    MultiInputContext.buffer[MultiInputContext.length] = 0U;
    MultiInputContext.full = 0U;
    return 1U;
}

/** 删除光标左侧字符；光标在开头时不执行任何操作。 */
static void MultiInputBackspace(void)
{
    uint8_t i;
    if(MultiInputContext.cursor == 0U)
    {
        return;
    }
    for(i = MultiInputContext.cursor - 1U; i < MultiInputContext.length - 1U; i++)
    {
        MultiInputContext.buffer[i] = MultiInputContext.buffer[i + 1U];
    }
    MultiInputContext.length--;
    MultiInputContext.cursor--;
    MultiInputContext.buffer[MultiInputContext.length] = 0U;
    MultiInputContext.full = 0U;
}

/** 删除光标当前位置字符；光标在正文末尾时不执行任何操作。 */
static void MultiInputDelete(void)
{
    uint8_t i;
    if(MultiInputContext.cursor >= MultiInputContext.length)
    {
        return;
    }
    for(i = MultiInputContext.cursor; i < MultiInputContext.length - 1U; i++)
    {
        MultiInputContext.buffer[i] = MultiInputContext.buffer[i + 1U];
    }
    MultiInputContext.length--;
    MultiInputContext.buffer[MultiInputContext.length] = 0U;
    MultiInputContext.full = 0U;
}

/**
 * 用所选候选替换光标所在的完整单词，并把光标移动到候选词尾。
 * 候选变长或变短时先调整尾部正文；只有原单词位于文本末尾时才自动加空格。
 */
static void MultiInputSelectCandidate(uint8_t slot)
{
    uint16_t value;
    uint8_t start;
    uint8_t end;
    uint8_t old_word_length;
    uint8_t candidate_length;
    uint8_t difference;
    uint8_t i;
    uint8_t old_length;

    if(slot >= MultiInputContext.candidate_count)
    {
        return;
    }

    MultiInputFindWordBounds(&start, &end);
    old_word_length = end - start;
    candidate_length = MultiInputDictionaryWordLength(slot);
    old_length = MultiInputContext.length;

    if(candidate_length > old_word_length)
    {
        /* 候选更长：先校验容量，再从后向前为差值腾出空间。 */
        difference = candidate_length - old_word_length;
        if((uint16_t)MultiInputContext.length + difference >
           MultiInputContext.max_length)
        {
            MultiInputContext.full = 1U;
            return;
        }
        i = MultiInputContext.length;
        while(i > end)
        {
            MultiInputContext.buffer[i + difference - 1U] = MultiInputContext.buffer[i - 1U];
            i--;
        }
        MultiInputContext.length += difference;
    }
    else if(candidate_length < old_word_length)
    {
        /* 候选更短：将原单词后的正文整体左移以收缩缓冲区。 */
        difference = old_word_length - candidate_length;
        for(i = end; i < MultiInputContext.length; i++)
        {
            MultiInputContext.buffer[i - difference] = MultiInputContext.buffer[i];
        }
        MultiInputContext.length -= difference;
    }

    /* 候选显示形式沿用用户前缀的大小写风格。 */
    for(i = 0U; i < candidate_length; i++)
    {
        value = MultiInputContext.candidate_words[slot][i];
        MultiInputContext.buffer[start + i] =
            MultiInputApplyCase(value, MultiInputContext.candidate_case, i);
    }
    MultiInputContext.cursor = start + candidate_length;

    /* 只在词尾原本没有后续空白或标点且仍有容量时补一个空格。 */
    if((end == old_length) &&
       (MultiInputContext.length < MultiInputContext.max_length))
    {
        MultiInputContext.buffer[MultiInputContext.cursor] = 0x0020U;
        MultiInputContext.cursor++;
        MultiInputContext.length++;
    }
    MultiInputContext.buffer[MultiInputContext.length] = 0U;
    MultiInputContext.full = 0U;
}

/** 结束输入会话；commit 非零时写回正文，取消时保持原目标内容不变。 */
static void MultiInputFinish(uint8_t commit)
{
    if(commit)
    {
        MultiInputWriteTarget();
    }

    MultiInputContext.active = 0U;
    FlashDictionaryQuery(MultiInputContext.dictionary_prefix, 0U, 0UL);
    MultiInputClearVp(MULTI_INPUT_PAGE_VP, 16U);
    MultiInputClearVp(MULTI_INPUT_CANDIDATE1_VP, 16U);
    MultiInputClearVp(MULTI_INPUT_CANDIDATE2_VP, 16U);
    MultiInputClearVp(MULTI_INPUT_CANDIDATE3_VP, 16U);
    MultiInputClearVp(MULTI_INPUT_CANDIDATE4_VP, 16U);
    SwitchPageById(MultiInputContext.source_page);
}

/**
 * 从动态目标 VP 载入本次允许数量的 UTF-16BE 字符，并进入键盘页。
 * 当前页面会在切换前保存，以便确认或取消后准确返回调用页面。
 */
static void MultiInputLoadTarget(uint16_t target_vp, uint8_t max_length)
{
    uint8_t i;
    uint16_t value;

    MultiInputContext.target_vp = target_vp;
    MultiInputContext.max_length = max_length;
    MultiInputContext.source_page = MultiInputReadWord(sysDGUS_PIC_NOW);
    MultiInputZeroVpBytes((uint16_t)MultiInputContext.max_length * 2U);
    read_dgus_vp(MultiInputContext.target_vp, MultiInputContext.vp_bytes,
                 MultiInputContext.max_length);

    MultiInputContext.length = 0U;
    for(i = 0U; i < MultiInputContext.max_length; i++)
    {
        /* 显式合并高低字节，避免 8051 整数端序影响 Unicode。 */
        value = ((uint16_t)MultiInputContext.vp_bytes[(uint16_t)i * 2U] << 8) |
                (uint16_t)MultiInputContext.vp_bytes[(uint16_t)i * 2U + 1U];
        if(value == 0U)
        {
            break;
        }
        MultiInputContext.buffer[MultiInputContext.length] = value;
        MultiInputContext.length++;
    }
    MultiInputContext.buffer[MultiInputContext.length] = 0U;
    MultiInputContext.cursor = MultiInputContext.length;
    MultiInputContext.caps = 0U;
    MultiInputContext.full = 0U;
    MultiInputContext.active = 1U;
    MultiInputRefresh();
    SwitchPageById(MultiInputContext.language->keyboard_page);
}

/** 分派控制键、候选键、扩展字母键以及原 QWERTY/标点返回键。 */
static void MultiInputHandleKey(uint16_t key)
{
    uint16_t value;
    uint8_t slot;

    if(key == MULTI_INPUT_KEY_ESCAPE)
    {
        MultiInputFinish(0U);
        return;
    }
    if((key == MULTI_INPUT_KEY_OK) ||
       (key == MULTI_INPUT_KEY_ENTER) ||
       (key == MULTI_INPUT_KEY_ENTER_PAIR))
    {
        MultiInputFinish(1U);
        return;
    }
    if(key == MULTI_INPUT_KEY_BACKSPACE)
    {
        MultiInputBackspace();
        MultiInputRefresh();
        return;
    }
    if(key == MULTI_INPUT_KEY_DELETE)
    {
        MultiInputDelete();
        MultiInputRefresh();
        return;
    }
    if(key == MULTI_INPUT_KEY_CAPS)
    {
        MultiInputContext.caps = !MultiInputContext.caps;
        MultiInputContext.full = 0U;
        MultiInputRefresh();
        return;
    }
    if(key == MULTI_INPUT_KEY_LEFT)
    {
        if(MultiInputContext.cursor > 0U)
        {
            MultiInputContext.cursor--;
        }
        MultiInputContext.full = 0U;
        MultiInputRefresh();
        return;
    }
    if(key == MULTI_INPUT_KEY_RIGHT)
    {
        if(MultiInputContext.cursor < MultiInputContext.length)
        {
            MultiInputContext.cursor++;
        }
        MultiInputContext.full = 0U;
        MultiInputRefresh();
        return;
    }

    if((key == MULTI_INPUT_KEY_PREVIOUS_PAGE) || (key == MULTI_INPUT_KEY_NEXT_PAGE))
    {
        if(MultiInputContext.language->dictionary_backend == MULTI_INPUT_DICTIONARY_FLASH &&
           MultiInputContext.dictionary_result_state == FLASH_DICTIONARY_READY)
        {
            if(key == MULTI_INPUT_KEY_PREVIOUS_PAGE && MultiInputContext.candidate_page > 0UL)
            {
                MultiInputRequestFlashPage(MultiInputContext.candidate_page - 1UL);
            }
            else if(key == MULTI_INPUT_KEY_NEXT_PAGE &&
                    (MultiInputContext.candidate_page + 1UL) * 4UL < MultiInputContext.candidate_total)
            {
                MultiInputRequestFlashPage(MultiInputContext.candidate_page + 1UL);
            }
        }
        return;
    }

    if((key >= MULTI_INPUT_CANDIDATE_KEY_BASE) &&
       (key < MULTI_INPUT_CANDIDATE_KEY_BASE + MULTI_INPUT_CANDIDATE_COUNT))
    {
        /* 0xF101..0xF104 映射到候选槽 0..3。 */
        slot = (uint8_t)(key - MULTI_INPUT_CANDIDATE_KEY_BASE);
        if(slot < MultiInputContext.candidate_count)
        {
            MultiInputSelectCandidate(slot);
            MultiInputRefresh();
        }
        return;
    }

    if(key == MULTI_INPUT_KEY_LANGUAGE_SWITCH)
    {
        if(MultiInputContext.secondary_language != 0)
        {
            MultiInputLanguagePack code *previous_language;

            /* 交换主/次语言，使选择在本次上电期间及后续会话中继续生效。 */
            previous_language = MultiInputContext.language;
            MultiInputContext.language = MultiInputContext.secondary_language;
            MultiInputContext.secondary_language = previous_language;
            MultiInputContext.full = 0U;
            MultiInputRefresh();
            SwitchPageById(MultiInputContext.language->keyboard_page);
        }
        return;
    }

    if((key >= MULTI_INPUT_EXTENDED_KEY_BASE) &&
       (key < MULTI_INPUT_EXTENDED_KEY_BASE +
              MultiInputContext.language->extended_character_count))
    {
        /* 扩展事件查表得到预组合小写字符，Caps 打开时再转为大写。 */
        value = MultiInputContext.language->extended_characters[
            key - MULTI_INPUT_EXTENDED_KEY_BASE];
        if(MultiInputContext.caps)
        {
            value = MultiInputToUpper(value);
        }
        MultiInputInsertCharacter(value);
        MultiInputRefresh();
        return;
    }

    if((key & 0xFF00U) != 0U)
    {
        /*
         * 原 return_key 同时携带小写/大写两个 ASCII 字节：Caps 决定取高
         * 字节还是低字节；单字节标点事件则直接使用整个 key。
         */
        if(MultiInputContext.caps)
        {
            value = (key >> 8) & 0x00FFU;
        }
        else
        {
            value = key & 0x00FFU;
        }
    }
    else
    {
        value = key;
    }

    if(value >= 0x0020U)
    {
        MultiInputInsertCharacter(value);
        MultiInputRefresh();
    }
}

/**
 * 初始化模块及其 DGUS VP 协议区。
 *
 * 除清空内部状态外，也会清除页码区 0x0720、候选、状态和预览，避免
 * 设备复位后显示 RAM 中残留上一次输入会话的数据。
 */
uint8_t MultiInputInit(MultiInputLanguagePack code *language)
{
    if(!MultiInputLanguageIsValid(language))
    {
        return 0U;
    }
    MultiInputContext.language = language;
    MultiInputContext.secondary_language = 0;
    MultiInputContext.target_vp = 0U;
    MultiInputContext.source_page = 0U;
    MultiInputContext.length = 0U;
    MultiInputContext.max_length = MULTI_INPUT_DEFAULT_LENGTH;
    MultiInputContext.cursor = 0U;
    MultiInputContext.active = 0U;
    MultiInputContext.caps = 0U;
    MultiInputContext.full = 0U;
    MultiInputContext.candidate_case = MULTI_INPUT_CASE_LOWER;
    MultiInputContext.buffer[0] = 0U;
    MultiInputClearCandidates();
    MultiInputContext.dictionary_prefix_length = 0U;
    MultiInputContext.dictionary_result_state = FLASH_DICTIONARY_EMPTY;
    FlashDictionaryInit();

    /* 事件 VP 由 OS 消费后清零；初始化时先建立相同的空闲状态。 */
    MultiInputWriteWord(MULTI_INPUT_KEY_VP, 0U);
    MultiInputWriteWord(MULTI_INPUT_LAUNCH_VP, 0U);
    MultiInputWriteWord(MULTI_INPUT_LENGTH_VP, MULTI_INPUT_DEFAULT_LENGTH);
    MultiInputClearVp(MULTI_INPUT_COMPOSITION_VP, 16U);
    MultiInputClearVp(MULTI_INPUT_CANDIDATE1_VP, 16U);
    MultiInputClearVp(MULTI_INPUT_CANDIDATE2_VP, 16U);
    MultiInputClearVp(MULTI_INPUT_CANDIDATE3_VP, 16U);
    MultiInputClearVp(MULTI_INPUT_CANDIDATE4_VP, 16U);
    MultiInputClearVp(MULTI_INPUT_STATUS_VP, 16U);
    MultiInputClearVp(MULTI_INPUT_BUFFER_VP, MULTI_INPUT_PREVIEW_WORD_COUNT);
    return 1U;
}

/** 切换后续输入会话使用的语言包；当前会话的语言配置保持稳定。 */
uint8_t MultiInputSetLanguage(MultiInputLanguagePack code *language)
{
    if(MultiInputContext.active || !MultiInputLanguageIsValid(language))
    {
        return 0U;
    }
    if(language == MultiInputContext.secondary_language)
    {
        MultiInputContext.secondary_language = MultiInputContext.language;
    }
    MultiInputContext.language = language;
    return 1U;
}

/** 注册活动会话中可由 0xF300 交换的第二语言。 */
uint8_t MultiInputSetSecondaryLanguage(MultiInputLanguagePack code *language)
{
    if(MultiInputContext.active || !MultiInputLanguageIsValid(language) ||
       (language == MultiInputContext.language))
    {
        return 0U;
    }
    MultiInputContext.secondary_language = language;
    return 1U;
}

/**
 * 输入法周期任务入口。
 *
 * 启动请求优先于按键处理；每个非零事件在分派前立即清零，保证一次触摸只
 * 消费一次。会话进行中收到新的启动请求会被消费但不会覆盖当前目标 VP。
 */
void MultiInputTask(void)
{
    uint16_t launch_target;
    uint16_t key;
    uint8_t max_length;

    launch_target = MultiInputReadWord(MULTI_INPUT_LAUNCH_VP);
    if(launch_target != 0U)
    {
        /* 启动事件携带动态目标 VP，0x0000 被定义为无请求。 */
        MultiInputWriteWord(MULTI_INPUT_LAUNCH_VP, 0U);
        if(!MultiInputContext.active)
        {
            MultiInputWriteWord(MULTI_INPUT_KEY_VP, 0U);
            max_length = MultiInputResolveMaximumLength();
            /* 最大长度是单次启动参数；消费后恢复下一次会话的默认值。 */
            MultiInputWriteWord(MULTI_INPUT_LENGTH_VP,
                                MULTI_INPUT_DEFAULT_LENGTH);
            MultiInputLoadTarget(launch_target, max_length);
        }
    }

    MultiInputPollFlashCandidates();
    key = MultiInputReadWord(MULTI_INPUT_KEY_VP);
    if(key != 0U)
    {
        /* 先清事件再处理，后续页面切换不会造成旧按键重放。 */
        MultiInputWriteWord(MULTI_INPUT_KEY_VP, 0U);
        if(MultiInputContext.active)
        {
            MultiInputHandleKey(key);
        }
    }
}



/** 词库 I/O 与 CRC 独立推进，20 ms 按键任务只发布完整结果。 */
void MultiInputDictionaryTask(void)
{
    FlashDictionaryTask();
}
