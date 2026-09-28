#ifndef MULTI_INPUT_H
#define MULTI_INPUT_H

#include "sys.h"

/* 输入法任务每 20 ms 轮询一次 DGUS 启动与按键事件。 */
#define MULTI_INPUT_TASK_ID                 1U
#define MULTI_INPUT_TASK_INTERVAL           20U
#define MULTI_INPUT_DICTIONARY_TASK_ID       3U
#define MULTI_INPUT_DICTIONARY_INTERVAL      1U

/* 兼容现有屏端工程的 VP 地址。 */
#define MULTI_INPUT_KEY_VP                  0x0700U
#define MULTI_INPUT_LAUNCH_VP               0x0710U
#define MULTI_INPUT_LENGTH_VP               0x0711U
#define MULTI_INPUT_COMPOSITION_VP          0x0720U
#define MULTI_INPUT_PAGE_VP                 MULTI_INPUT_COMPOSITION_VP
#define MULTI_INPUT_CANDIDATE1_VP           0x0730U
#define MULTI_INPUT_CANDIDATE2_VP           0x0740U
#define MULTI_INPUT_CANDIDATE3_VP           0x0750U
#define MULTI_INPUT_CANDIDATE4_VP           0x0760U
#define MULTI_INPUT_STATUS_VP               0x0770U
#define MULTI_INPUT_BUFFER_VP               0x0780U

/* 正文最多 64 字符；预览区额外保留一个可视光标字位。 */
#define MULTI_INPUT_DEFAULT_LENGTH          64U
#define MULTI_INPUT_MAX_LENGTH              64U
#define MULTI_INPUT_PREVIEW_WORD_COUNT      (MULTI_INPUT_MAX_LENGTH + 1U)
#define MULTI_INPUT_CANDIDATE_COUNT         4U
#define MULTI_INPUT_CANDIDATE_MAX_LENGTH    15U
#define MULTI_INPUT_DICTIONARY_CODE         0U
#define MULTI_INPUT_DICTIONARY_FLASH        1U
#define MULTI_INPUT_KEY_PREVIOUS_PAGE       0xF105U
#define MULTI_INPUT_KEY_NEXT_PAGE           0xF106U

/** 一个预组合拉丁字符的大小写映射。 */
typedef struct
{
    uint16_t lower;
    uint16_t upper;
} MultiInputCasePair;

/**
 * 拉丁语言包描述。
 *
 * 描述及字符表保存在 code 区。CODE 后端使用 UTF-16 词池及偏移表；
 * FLASH 后端使用独立 SKD1 资源，两个 code 词典指针可为零。
 */
typedef struct
{
    MultiInputCasePair code *case_pairs;
    uint16_t code *extended_characters;
    uint16_t code *dictionary_pool;
    uint16_t code *dictionary_offsets;
    uint32_t dictionary_word_count;
    uint8_t case_pair_count;
    uint8_t extended_character_count;
    uint8_t dictionary_max_word_length;
    uint16_t keyboard_page;
    uint8_t dictionary_backend;
    uint8_t dictionary_min_prefix;
} MultiInputLanguagePack;

/** 初始化底座、选择默认语言，并清空输入法占用的 DGUS VP。 */
uint8_t MultiInputInit(MultiInputLanguagePack code *language);

/** 在无活动输入会话时切换语言；成功返回 1，否则返回 0。 */
uint8_t MultiInputSetLanguage(MultiInputLanguagePack code *language);

/** 在无活动输入会话时注册可由 0xF300 切换的第二语言。 */
uint8_t MultiInputSetSecondaryLanguage(MultiInputLanguagePack code *language);

/** 消费启动和按键事件；由系统调度器每 20 ms 调用一次。 */
void MultiInputTask(void);

/** 推进外部词库校验及查询；独立以 1 ms 周期调度，不阻塞正文输入。 */
void MultiInputDictionaryTask(void);

#endif /* MULTI_INPUT_H */
