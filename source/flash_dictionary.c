#include "flash_dictionary.h"
#include "ext_font_flash.h"

#define DICT_HEADER_SIZE 64U
#define DICT_NODE_SIZE 16U
#define DICT_BUCKET_SIZE 64U
#define DICT_ALPHABET_SIZE 43U
#define DICT_HEALTH_HEADER 0U
#define DICT_HEALTH_CRC 1U
#define DICT_HEALTH_READY 2U
#define DICT_HEALTH_ERROR 3U
#define DICT_QUERY_NODE 1U
#define DICT_QUERY_CHILD 2U
#define DICT_QUERY_LEAF_POINTER 3U
#define DICT_QUERY_LEAF_WORD 4U
#define DICT_QUERY_PREPARE 5U
#define DICT_QUERY_PAGE_POINTER 6U
#define DICT_QUERY_PAGE_WORD 7U
#define DICT_QUERY_DONE 8U

typedef struct
{
    uint8_t cache[2][256];
    uint32_t cache_page[2];
    uint8_t cache_valid[2];
    uint8_t cache_recent;
    uint8_t cache_pending;
    uint8_t cache_slot;
    uint32_t pending_page;
    uint8_t scratch[64];
    uint8_t prefix[15];
    uint8_t word[16];
    uint32_t leaf_offsets[DICT_BUCKET_SIZE];
    uint32_t image_size;
    uint32_t word_count;
    uint32_t node_count;
    uint32_t postings_offset;
    uint32_t pool_offset;
    uint32_t pool_size;
    uint32_t expected_crc;
    uint32_t crc;
    uint32_t crc_position;
    uint32_t node_offset;
    uint32_t children_offset;
    uint32_t posting_offset;
    uint32_t node_matches;
    uint32_t word_offset;
    uint32_t requested_page;
    uint32_t match_total;
    uint8_t health;
    uint8_t phase;
    uint8_t prefix_length;
    uint8_t consumed;
    uint8_t child_low;
    uint8_t child_high;
    uint8_t leaf_index;
    uint8_t leaf_count;
    uint8_t use_leaf;
    uint8_t page_slot;
    FlashDictionaryResult result;
} FlashDictionaryContext;

static FlashDictionaryContext xdata Dictionary;
/* STARTUP_M5 不清空 XDATA；必须由 C 初始化表设置首次启动标志。 */
static uint8_t xdata DictionaryInitialized = 0U;
static code uint32_t DictionaryCrcTable[16] = {
    0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
    0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
    0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
    0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL
};

static uint32_t DictionaryRead32(uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static uint32_t DictionaryRead24(uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 16) | ((uint32_t)bytes[1] << 8) |
           (uint32_t)bytes[2];
}

static void DictionaryClearResult(void)
{
    uint8_t i;
    uint8_t j;
    Dictionary.result.total = 0UL;
    Dictionary.result.page = 0UL;
    Dictionary.result.count = 0U;
    for(i = 0U; i < FLASH_DICTIONARY_PAGE_SIZE; i++)
    {
        for(j = 0U; j < FLASH_DICTIONARY_WORD_SIZE; j++)
        {
            Dictionary.result.words[i][j] = 0U;
        }
    }
}

static void DictionaryFail(void)
{
    Dictionary.health = DICT_HEALTH_ERROR;
    Dictionary.phase = DICT_QUERY_DONE;
    DictionaryClearResult();
    Dictionary.result.state = FLASH_DICTIONARY_ERROR;
}

/* 两个缓存页确保跨 256-byte 边界的短记录不会反复换页。 */
static uint8_t DictionaryRead(uint32_t offset, uint8_t *destination, uint16_t length)
{
    uint16_t i;
    uint8_t slot;
    uint8_t found;
    uint32_t position;
    uint32_t page;
    uint32_t limit;

    limit = Dictionary.health == DICT_HEALTH_HEADER ?
            EXT_FONT_FLASH_CAPACITY_BYTES : Dictionary.image_size;
    if((offset > limit) || ((uint32_t)length > limit - offset))
    {
        DictionaryFail();
        return 0U;
    }
    for(i = 0U; i < length; i++)
    {
        position = offset + (uint32_t)i;
        page = position & 0xFFFFFF00UL;
        found = 0U;
        for(slot = 0U; slot < 2U; slot++)
        {
            if(Dictionary.cache_valid[slot] && Dictionary.cache_page[slot] == page)
            {
                destination[i] = Dictionary.cache[slot][(uint8_t)position];
                Dictionary.cache_recent = slot;
                found = 1U;
                break;
            }
        }
        if(!found)
        {
            if(!Dictionary.cache_pending)
            {
                slot = Dictionary.cache_recent ^ 1U;
                if(ExtFontFlashStart(page, 256U))
                {
                    Dictionary.cache_slot = slot;
                    Dictionary.pending_page = page;
                    Dictionary.cache_pending = 1U;
                    Dictionary.cache_valid[slot] = 0U;
                }
            }
            return 0U;
        }
    }
    return 1U;
}

static uint8_t DictionaryValidateHeader(void)
{
    uint8_t i;
    uint8_t *bytes;
    uint32_t nodes_end;

    bytes = Dictionary.scratch;
    if((bytes[0] != 'S') || (bytes[1] != 'K') || (bytes[2] != 'D') ||
       (bytes[3] != '1') || (bytes[4] != 0U) || (bytes[5] != 1U) ||
       (bytes[6] != 0U) || (bytes[7] != DICT_HEADER_SIZE) ||
       (DictionaryRead32(bytes + 20) != DICT_HEADER_SIZE) ||
       (bytes[40] != 0U) || (bytes[41] != 15U) ||
       (bytes[42] != 0U) || (bytes[43] != DICT_BUCKET_SIZE))
    {
        return 0U;
    }
    for(i = 44U; i < DICT_HEADER_SIZE; i++)
    {
        if(bytes[i] != 0U)
        {
            return 0U;
        }
    }
    Dictionary.image_size = DictionaryRead32(bytes + 8);
    Dictionary.word_count = DictionaryRead32(bytes + 12);
    Dictionary.node_count = DictionaryRead32(bytes + 16);
    Dictionary.postings_offset = DictionaryRead32(bytes + 24);
    Dictionary.pool_offset = DictionaryRead32(bytes + 28);
    Dictionary.pool_size = DictionaryRead32(bytes + 32);
    Dictionary.expected_crc = DictionaryRead32(bytes + 36);
    if((Dictionary.image_size < DICT_HEADER_SIZE) ||
       (Dictionary.image_size > EXT_FONT_FLASH_CAPACITY_BYTES) ||
       (Dictionary.word_count != 10000UL) || (Dictionary.node_count == 0UL) ||
       (Dictionary.node_count > (EXT_FONT_FLASH_CAPACITY_BYTES - 64UL) / 16UL))
    {
        return 0U;
    }
    nodes_end = 64UL + Dictionary.node_count * 16UL;
    if((Dictionary.postings_offset != nodes_end) ||
       (Dictionary.pool_offset < nodes_end) ||
       (Dictionary.pool_offset > Dictionary.image_size) ||
       ((Dictionary.pool_offset & 3UL) != 0UL) ||
       (Dictionary.pool_size == 0UL) ||
       (Dictionary.pool_size > Dictionary.image_size - Dictionary.pool_offset) ||
       (Dictionary.image_size - Dictionary.pool_offset - Dictionary.pool_size > 3UL))
    {
        return 0U;
    }
    return 1U;
}

static uint8_t DictionaryValidateNode(uint32_t offset)
{
    uint32_t children;
    uint32_t posting;
    uint32_t matches;
    uint8_t *bytes;
    uint8_t child_count;

    bytes = Dictionary.scratch;
    child_count = bytes[1];
    children = DictionaryRead32(bytes + 4);
    posting = DictionaryRead32(bytes + 8);
    matches = DictionaryRead32(bytes + 12);
    if((bytes[0] > DICT_ALPHABET_SIZE) || (child_count > DICT_ALPHABET_SIZE) ||
       (bytes[2] != 0U) || (bytes[3] != 0U) ||
       (matches == 0UL) || (matches > Dictionary.word_count))
    {
        return 0U;
    }
    if(child_count)
    {
        if((children < DICT_HEADER_SIZE) || ((children & 15UL) != 0UL) ||
           (children >= Dictionary.postings_offset) ||
           ((uint32_t)child_count * 16UL > Dictionary.postings_offset - children))
        {
            return 0U;
        }
    }
    else if(children != 0UL)
    {
        return 0U;
    }
    if(offset == DICT_HEADER_SIZE)
    {
        return (uint8_t)((bytes[0] == 0U) && (posting == 0UL) &&
                         (matches == Dictionary.word_count) && child_count);
    }
    if((bytes[0] == 0U) || (posting < Dictionary.postings_offset) ||
       (posting >= Dictionary.pool_offset) || ((posting & 3UL) != 0UL) ||
       (matches * 3UL > Dictionary.pool_offset - posting))
    {
        return 0U;
    }
    return 1U;
}

static uint8_t DictionaryReadWord(void)
{
    uint8_t i;
    uint8_t length;
    uint32_t remaining;

    if(Dictionary.word_offset >= Dictionary.pool_size)
    {
        DictionaryFail();
        return 0U;
    }
    remaining = Dictionary.pool_size - Dictionary.word_offset;
    length = remaining < 16UL ? (uint8_t)remaining : 16U;
    if(!DictionaryRead(Dictionary.pool_offset + Dictionary.word_offset,
                       Dictionary.word, length))
    {
        return 0U;
    }
    for(i = 0U; i < length; i++)
    {
        if(Dictionary.word[i] == 0U)
        {
            if(i == 0U)
            {
                DictionaryFail();
                return 0U;
            }
            while(++i < 16U)
            {
                Dictionary.word[i] = 0U;
            }
            return 1U;
        }
        if(Dictionary.word[i] > DICT_ALPHABET_SIZE)
        {
            break;
        }
    }
    DictionaryFail();
    return 0U;
}

static uint8_t DictionaryWordMatches(void)
{
    uint8_t i;
    for(i = 0U; i < Dictionary.prefix_length; i++)
    {
        if(Dictionary.word[i] != Dictionary.prefix[i])
        {
            return 0U;
        }
    }
    return 1U;
}

static void DictionaryNoMatches(void)
{
    DictionaryClearResult();
    Dictionary.phase = DICT_QUERY_DONE;
    Dictionary.result.state = FLASH_DICTIONARY_READY;
}

/* 返回 0 表示需要等待 Flash；每个调用最多推进一个有界状态。 */
static uint8_t DictionaryQueryStep(void)
{
    uint8_t middle;
    uint8_t i;
    uint8_t child_count;
    uint32_t offset;
    uint32_t page_count;
    uint32_t ordinal;

    if(Dictionary.phase == DICT_QUERY_NODE)
    {
        if(!DictionaryRead(Dictionary.node_offset, Dictionary.scratch, 16U))
        {
            return 0U;
        }
        if(!DictionaryValidateNode(Dictionary.node_offset))
        {
            DictionaryFail();
            return 0U;
        }
        child_count = Dictionary.scratch[1];
        Dictionary.children_offset = DictionaryRead32(Dictionary.scratch + 4);
        Dictionary.posting_offset = DictionaryRead32(Dictionary.scratch + 8);
        Dictionary.node_matches = DictionaryRead32(Dictionary.scratch + 12);
        if(Dictionary.consumed == Dictionary.prefix_length)
        {
            Dictionary.match_total = Dictionary.node_matches;
            Dictionary.phase = DICT_QUERY_PREPARE;
        }
        else if(child_count)
        {
            Dictionary.child_low = 0U;
            Dictionary.child_high = child_count;
            Dictionary.phase = DICT_QUERY_CHILD;
        }
        else
        {
            if(Dictionary.node_matches > DICT_BUCKET_SIZE)
            {
                DictionaryFail();
                return 0U;
            }
            Dictionary.use_leaf = 1U;
            Dictionary.leaf_index = 0U;
            Dictionary.leaf_count = 0U;
            Dictionary.phase = DICT_QUERY_LEAF_POINTER;
        }
    }
    else if(Dictionary.phase == DICT_QUERY_CHILD)
    {
        if(Dictionary.child_low >= Dictionary.child_high)
        {
            DictionaryNoMatches();
            return 0U;
        }
        middle = Dictionary.child_low + (Dictionary.child_high - Dictionary.child_low) / 2U;
        offset = Dictionary.children_offset + (uint32_t)middle * 16UL;
        if(!DictionaryRead(offset, Dictionary.scratch, 16U))
        {
            return 0U;
        }
        if(Dictionary.scratch[0] == Dictionary.prefix[Dictionary.consumed])
        {
            Dictionary.node_offset = offset;
            Dictionary.consumed++;
            Dictionary.phase = DICT_QUERY_NODE;
        }
        else if(Dictionary.scratch[0] < Dictionary.prefix[Dictionary.consumed])
        {
            Dictionary.child_low = middle + 1U;
        }
        else
        {
            Dictionary.child_high = middle;
        }
    }
    else if(Dictionary.phase == DICT_QUERY_LEAF_POINTER)
    {
        if(!DictionaryRead(Dictionary.posting_offset + (uint32_t)Dictionary.leaf_index * 3UL,
                           Dictionary.scratch, 3U))
        {
            return 0U;
        }
        Dictionary.word_offset = DictionaryRead24(Dictionary.scratch);
        Dictionary.phase = DICT_QUERY_LEAF_WORD;
    }
    else if(Dictionary.phase == DICT_QUERY_LEAF_WORD)
    {
        if(!DictionaryReadWord())
        {
            return 0U;
        }
        if(DictionaryWordMatches())
        {
            Dictionary.leaf_offsets[Dictionary.leaf_count++] = Dictionary.word_offset;
        }
        Dictionary.leaf_index++;
        if((uint32_t)Dictionary.leaf_index == Dictionary.node_matches)
        {
            Dictionary.match_total = (uint32_t)Dictionary.leaf_count;
            Dictionary.phase = DICT_QUERY_PREPARE;
        }
        else
        {
            Dictionary.phase = DICT_QUERY_LEAF_POINTER;
        }
    }
    else if(Dictionary.phase == DICT_QUERY_PREPARE)
    {
        if(Dictionary.match_total == 0UL)
        {
            DictionaryNoMatches();
            return 0U;
        }
        page_count = (Dictionary.match_total + 3UL) / 4UL;
        if(Dictionary.requested_page >= page_count)
        {
            Dictionary.requested_page = page_count - 1UL;
        }
        Dictionary.result.total = Dictionary.match_total;
        Dictionary.result.page = Dictionary.requested_page;
        Dictionary.page_slot = 0U;
        Dictionary.phase = DICT_QUERY_PAGE_POINTER;
    }
    else if(Dictionary.phase == DICT_QUERY_PAGE_POINTER)
    {
        ordinal = Dictionary.requested_page * 4UL + (uint32_t)Dictionary.page_slot;
        if((Dictionary.page_slot == 4U) || (ordinal >= Dictionary.match_total))
        {
            Dictionary.result.count = Dictionary.page_slot;
            Dictionary.result.state = FLASH_DICTIONARY_READY;
            Dictionary.phase = DICT_QUERY_DONE;
            return 0U;
        }
        if(Dictionary.use_leaf)
        {
            Dictionary.word_offset = Dictionary.leaf_offsets[(uint8_t)ordinal];
        }
        else
        {
            if(!DictionaryRead(Dictionary.posting_offset + ordinal * 3UL,
                               Dictionary.scratch, 3U))
            {
                return 0U;
            }
            Dictionary.word_offset = DictionaryRead24(Dictionary.scratch);
        }
        Dictionary.phase = DICT_QUERY_PAGE_WORD;
    }
    else if(Dictionary.phase == DICT_QUERY_PAGE_WORD)
    {
        if(!DictionaryReadWord())
        {
            return 0U;
        }
        if(!DictionaryWordMatches())
        {
            DictionaryFail();
            return 0U;
        }
        for(i = 0U; i < 16U; i++)
        {
            Dictionary.result.words[Dictionary.page_slot][i] = Dictionary.word[i];
        }
        Dictionary.page_slot++;
        Dictionary.phase = DICT_QUERY_PAGE_POINTER;
    }
    else
    {
        return 0U;
    }
    return 1U;
}

void FlashDictionaryInit(void)
{
    if(DictionaryInitialized)
    {
        FlashDictionaryQuery(Dictionary.prefix, 0U, 0UL);
        return;
    }
    DictionaryInitialized = 1U;
    ExtFontFlashInit();
    Dictionary.cache_valid[0] = 0U;
    Dictionary.cache_valid[1] = 0U;
    Dictionary.cache_recent = 1U;
    Dictionary.cache_pending = 0U;
    Dictionary.health = DICT_HEALTH_HEADER;
    Dictionary.prefix_length = 0U;
    Dictionary.phase = DICT_QUERY_DONE;
    DictionaryClearResult();
    Dictionary.result.state = FLASH_DICTIONARY_EMPTY;
}

void FlashDictionaryQuery(uint8_t *prefix, uint8_t length, uint32_t page)
{
    uint8_t i;
    DictionaryClearResult();
    Dictionary.prefix_length = 0U;
    Dictionary.phase = DICT_QUERY_DONE;
    if(Dictionary.health == DICT_HEALTH_ERROR)
    {
        Dictionary.result.state = FLASH_DICTIONARY_ERROR;
        return;
    }
    if((length == 0U) || (length > 15U))
    {
        Dictionary.result.state = FLASH_DICTIONARY_EMPTY;
        return;
    }
    for(i = 0U; i < length; i++)
    {
        if((prefix[i] == 0U) || (prefix[i] > DICT_ALPHABET_SIZE))
        {
            Dictionary.result.state = FLASH_DICTIONARY_EMPTY;
            return;
        }
        Dictionary.prefix[i] = prefix[i];
    }
    Dictionary.prefix_length = length;
    Dictionary.requested_page = page;
    Dictionary.consumed = 0U;
    Dictionary.use_leaf = 0U;
    Dictionary.node_offset = DICT_HEADER_SIZE;
    Dictionary.phase = DICT_QUERY_NODE;
    Dictionary.result.state = FLASH_DICTIONARY_BUSY;
}

FlashDictionaryResult xdata *FlashDictionaryGetResult(void)
{
    return &Dictionary.result;
}

void FlashDictionaryTask(void)
{
    uint8_t status;
    uint16_t i;
    uint8_t *bytes;
    uint8_t value;
    uint32_t crc;

    ExtFontFlashTask();
    status = ExtFontFlashStatus();
    if(status == EXT_FONT_FLASH_ERROR)
    {
        DictionaryFail();
        return;
    }
    if(Dictionary.cache_pending && status == EXT_FONT_FLASH_READY)
    {
        bytes = ExtFontFlashData();
        for(i = 0U; i < 256U; i++)
        {
            Dictionary.cache[Dictionary.cache_slot][i] = bytes[i];
        }
        Dictionary.cache_page[Dictionary.cache_slot] = Dictionary.pending_page;
        Dictionary.cache_valid[Dictionary.cache_slot] = 1U;
        Dictionary.cache_recent = Dictionary.cache_slot;
        Dictionary.cache_pending = 0U;
        ExtFontFlashRelease();
    }
    if(Dictionary.health == DICT_HEALTH_HEADER)
    {
        if(!DictionaryRead(0UL, Dictionary.scratch, DICT_HEADER_SIZE))
        {
            return;
        }
        if(!DictionaryValidateHeader())
        {
            DictionaryFail();
            return;
        }
        Dictionary.crc = 0xFFFFFFFFUL;
        Dictionary.crc_position = DICT_HEADER_SIZE;
        Dictionary.health = DICT_HEALTH_CRC;
    }
    if(Dictionary.health == DICT_HEALTH_CRC)
    {
        /* 一次最多校验 256 bytes，期间继续接收正文编辑及新前缀。 */
        for(i = 0U; i < 256U && Dictionary.crc_position < Dictionary.image_size; i++)
        {
            if(!DictionaryRead(Dictionary.crc_position, &value, 1U))
            {
                return;
            }
            crc = Dictionary.crc ^ (uint32_t)value;
            crc = (crc >> 4) ^ DictionaryCrcTable[(uint8_t)crc & 15U];
            Dictionary.crc = (crc >> 4) ^ DictionaryCrcTable[(uint8_t)crc & 15U];
            Dictionary.crc_position++;
        }
        if(Dictionary.crc_position != Dictionary.image_size)
        {
            return;
        }
        if((Dictionary.crc ^ 0xFFFFFFFFUL) != Dictionary.expected_crc)
        {
            DictionaryFail();
            return;
        }
        Dictionary.health = DICT_HEALTH_READY;
    }
    if(Dictionary.health == DICT_HEALTH_READY && Dictionary.result.state == FLASH_DICTIONARY_BUSY)
    {
        for(i = 0U; i < 16U; i++)
        {
            if(!DictionaryQueryStep())
            {
                break;
            }
        }
    }
}
