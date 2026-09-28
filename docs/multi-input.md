# 统一拉丁系 DGUS 输入法底座

`source/multi_input.c/h` 负责输入会话、光标编辑、大小写、候选词和 DGUS
事件消费。语言差异全部由 `MultiInputLanguagePack` 描述，底座本身不包含
特定语言字符或词典。

## DGUS 接口

| VP | Words | 用途 |
| --- | ---: | --- |
| `0x0700` | 1 | 按键事件，OS 消费后清零 |
| `0x0710` | 1 | 启动按键返回值，即目标文本首 VP；OS 消费后清零 |
| `0x0711` | 1 | 本次输入最大字符数，默认 64，有效范围 1–64 |
| `0x0720` | 16 | 斯洛伐克候选页码；克罗地亚保持空白 |
| `0x0730`–`0x0760` | 16 each | 四个候选词 |
| `0x0770` | 16 | Caps、容量和光标状态 |
| `0x0780` | 65 | 最多 64 个 UTF-16 字符和可视光标 |
| `0x0800`–`0x087F` | 128 | 外部 NOR 读取专用暂存区，不用于用户字段 |
| `0x00AA`–`0x00AF` | 6 | 外部资源读取命令，词库读取期间独占 |

斯洛伐克键盘使用页面 11，克罗地亚键盘使用页面 13。正文、候选和目标 VP 均使用 UTF-16BE；预览中的 `|`
只表示光标，不会写入目标文本。

每个文本输入启动键都应把变量地址设为 `0x0710`，把按键返回值设为目标文本
的首 VP。固定 64 字符时无需额外操作；需要其他长度时，必须先向 `0x0711`
写入 1–64，再触发启动键。底座在启动时采样该值，随后立即把 `0x0711`
恢复为 64，因此非默认长度需要在每次启动前写入。零或大于 64 的值按 64
处理。

确认时只会从目标首 VP 开始写入 `0x0711` 指定数量的 words，未使用部分清零，
不会按固定 64 words 覆盖较短字段之后的 VP。

控制键保持 `F0/F1/F2/F3/F4/F7/F8`，其中 `F7/F8` 始终移动光标。候选键为 `F101–F104`；
斯洛伐克新增 `F105/F106` 上一页、下一页，每页 4 词，首末页不循环。克罗地亚忽略这两个翻页事件。语言扩展键
从 `F200` 开始按 `extended_characters` 的顺序查表。`F300` 在活动会话中
交换主/次语言并进入新语言的 `keyboard_page`，正文、光标和 Caps 均保持。
没有注册第二语言时会忽略该事件。ASCII QWERTY 键继续
使用高字节大写、低字节小写的组合键值。

## 公共接口

```c
uint8_t MultiInputInit(MultiInputLanguagePack code *language);
uint8_t MultiInputSetLanguage(MultiInputLanguagePack code *language);
uint8_t MultiInputSetSecondaryLanguage(MultiInputLanguagePack code *language);
void MultiInputTask(void);
void MultiInputDictionaryTask(void);
```

`MultiInputInit()` 校验默认语言包并清空输入法 VP。`MultiInputSetLanguage()`
和 `MultiInputSetSecondaryLanguage()` 仅允许在没有活动输入会话时调用。
运行时的 `F300` 会交换两个语言包，所以当前选择会延续到本次上电期间的下一次
输入会话；重启后仍由 `MultiInputInit()` 指定的斯洛伐克语开始。

按键任务 `MultiInputTask()` 每 20 ms 执行；另外注册任务 ID 3，以 1 ms 周期调用
`MultiInputDictionaryTask()`，用于分批校验资源及推进非阻塞读取。`user/main.c` 已注册两项任务。
启动校验未完成时仍可编辑正文，有效前缀的页码区显示 `...`，完成后自动显示候选。

语言包包含字符大小写表、扩展键表、键盘页号和词库后端。`dictionary_word_count` 为 32 位；
新增 `dictionary_backend` 与 `dictionary_min_prefix` 两项配置。CODE 后端使用以 U+0000 分隔的
UTF-16 词池和按候选优先级排列的偏移表；FLASH 后端使用 SKD1 外部词库，code 词池指针为零。
所有自定义语言包初始化表均需补上后端和最小前缀长度；无词典时将词数设为零。

## 斯洛伐克语言包

`modules/slovak_dictionary.c/h` 导出 `SlovakLanguagePack`，保留 17 个扩展字母及键盘配置，
**10,000 个词和前缀索引全部存放在外部 NOR 资源**，不再把 256 词池编译到 code 区。
源为 prim-11.0-public-all 的完整词频文件，经 NFC、大小写折叠、合法字母、1～15 字符过滤、
合并同词词频后，取频率最高的 10,000 词。同频按规范化词字符串排序。

输入一个字母即开始前缀匹配；匹配不区分大小写，但区分变音符号，完整单词本身也可作为候选。
所有入库匹配词均可通过翻页访问，没有额外的前四项或频率阈值过滤。
编辑正文、移动光标、切换语言后回到第一页；新查询和翻页期间清空旧候选，避免提交过期结果。
页码显示 `当前页/总页数`，无匹配为 `0/0`，资源不可用为 `--/--`。

候选继承前缀的小写、首字母大写或全大写形式；单个大写前缀按全大写处理，与现有 Caps 行为一致。
选择候选替换光标所在完整单词，仅当单词在正文末尾且有空间时补空格。替换超过字段容量时
保留原文并显示 `FULL`。选择操作使用已完整载入的当前页缓存，不等待 Flash。

比较用词表为 [`slovak-ime-words.tsv`](slovak-ime-words.tsv)，逐词对应实际镜像。
[`slovak-dictionary-format.md`](slovak-dictionary-format.md) 记录来源、格式和可重现生成方式；
资源部署、界面和硬件核验见 [`slovak-ime-upgrade.md`](slovak-ime-upgrade.md)。

## 克罗地亚语言包

`modules/croatian_dictionary.c/h` 导出 `CroatianLanguagePack`。它使用页面 13，
包含 `č/Č、ć/Ć、đ/Đ、š/Š、ž/Ž` 的大小写映射；`F200–F204` 按键盘物理位置
依次输入 `š、đ、ž、č、ć`。256 个候选词来自 CLARIN.SI hrWaC 2.1 高频
词形，NFC 归一化、按大小写折叠去重后生成；查询、响应 SHA-256、原始排名
和频次记录在 `croatian-ime-dictionary.tsv`。

## 新增语言

1. 在 `code` 区定义 `MultiInputCasePair`、扩展字符；CODE 后端可另外定义词池和偏移表。
2. 组装 `MultiInputLanguagePack`，指定 `keyboard_page`、后端和最小前缀长度。当前 FLASH 编码对应斯洛伐克 43 字母表，其他语言须生成匹配的资源格式及映射。
3. 启动时传给 `MultiInputInit()`，或在输入法空闲时传给
   `MultiInputSetLanguage()`；需要会话内切换时再用
   `MultiInputSetSecondaryLanguage()` 注册第二语言。

当前底座处理 UTF-16 BMP 内的预组合拉丁字符，不实现组合附加符或死键序列。
