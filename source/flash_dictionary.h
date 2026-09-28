#ifndef FLASH_DICTIONARY_H
#define FLASH_DICTIONARY_H

#include "sys.h"

#define FLASH_DICTIONARY_EMPTY 0U
#define FLASH_DICTIONARY_BUSY  1U
#define FLASH_DICTIONARY_READY 2U
#define FLASH_DICTIONARY_ERROR 3U
#define FLASH_DICTIONARY_PAGE_SIZE 4U
#define FLASH_DICTIONARY_WORD_SIZE 16U

/* 编码 1..26 对应 a..z，27..43 对应语言包扩展字符顺序，0 为结束符。 */
typedef struct
{
    uint8_t words[FLASH_DICTIONARY_PAGE_SIZE][FLASH_DICTIONARY_WORD_SIZE];
    uint32_t total;
    uint32_t page;
    uint8_t count;
    uint8_t state;
} FlashDictionaryResult;

void FlashDictionaryInit(void);
/* 独立的 1 ms 任务；校验、检索和 Flash 等待均可跨任务周期进行。 */
void FlashDictionaryTask(void);
void FlashDictionaryQuery(uint8_t *prefix, uint8_t length, uint32_t page);
FlashDictionaryResult xdata *FlashDictionaryGetResult(void);

#endif
