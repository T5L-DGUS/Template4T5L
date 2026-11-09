/* 这里的代码和压缩包里原版的QR_Encode.h不一样,
是修改过的,更改了纠错等级 好纠错容量
不清楚啥意思 百度一下 学习一下二维码基本知识
*/

#ifndef __QRENCODE_H
#define __QRENCODE_H
//#include "data_type.h"
/* 移植适配：源工程先包含 stdio.h，本工程 T5LOSConfig.h 已定义 NULL，
   stdio.h 中的 NULL 会造成宏重定义告警(C317)，故把 sys.h 提到前面 */
#include "sys.h"
#include "stdio.h"
#include "t5los8051.h"

/* ================== 移植适配（本项目 Template4T5L 补充）==================
 * 来源：D:\Work\0.项目\14二维码封装\C51\二维码替换 - flash读取版本\MODULES\QR_Encode.c/.h
 * 本工程无 USER/types.h，也不带 writeDgusiiVp/readDgusiiVp/COMBINE_U16，故在此补齐，
 * 使 QR_Encode.c 与源工程保持一致（便于日后两工程同步）。
 * 另：源工程 sys.h 的 read_pic()/Nor_Flash_read() 在本工程分别对应
 *     ReadPageId()（sys.h）/ nor_flash_read()（t5l_stc.c），见 QR_Encode.c 与 qr_page.c。
 * ====================================================================== */
typedef unsigned char   u8;
typedef unsigned int    u16;
typedef unsigned long   u32;
typedef unsigned char   bool;

#define writeDgusiiVp   write_dgus_vp
#define readDgusiiVp    read_dgus_vp

/* 源工程 sys.h：由高/低字节拼成 u16（flash 配置为大端存放） */
#define COMBINE_U16(high, low) ((uint16_t)(((uint16_t)((uint8_t)(high)) << 8) | (uint8_t)(low)))
//#ifndef BYTE
//typedef unsigned char  BYTE;
//#endif
//#ifndef WORD
//	typedef unsigned int  WORD;
//#endif

#ifndef NULL
	#define NULL 0
#endif

#ifndef TRUE
	#define TRUE 1
#endif

#ifndef FALSE
	#define FALSE 0
#endif

//typedef enum {FALSE = 0, TRUE = !FALSE} bool;
/* 移植适配：源工程在此重定义 min()，本工程 T5LOSConfig.h 已定义同名同义宏，
   保留会触发宏重定义告警，故删除，直接沿用 T5LOSConfig.h 的 min() */
//#define min(a,b)	(((a) < (b)) ? (a) : (b))
//4种纠错等级，可恢复的码字比例为：
#define QR_LEVEL_L	0	// 7%的字码可被修正
#define QR_LEVEL_M	1	//15%的字码可被修正
#define QR_LEVEL_Q	2	//25%的字码可被修正
#define QR_LEVEL_H	3	//30%的字码可被修正

//纠错等级（编译期宏）：QR_LEVEL_L/M/Q/H，数据容量表与版本选择按此动态计算
#define QR_ECC_LEVEL	QR_LEVEL_M

//编码形式
#define QR_MODE_NUMERAL		0
#define QR_MODE_ALPHABET	1
#define QR_MODE_8BIT		2
#define QR_MODE_KANJI		3
#define QR_MODE_CHINESE		4

//Number of bits per length field
//Encoding		Ver.1–9		10–26		27–40
//Numeric		10			12			14
//Alphanumeric	9			11			13
//Byte			8			16			16
//Kanji			8			10			12
//Chinese
//P17 字符计数指示的位数
#define QR_VRESION_S	0
#define QR_VRESION_M	1
#define QR_VRESION_L	2

// #define QR_MARGIN	4
// #define	QR_VER1_SIZE	21// 版本的行列数



/*************************************需要修改的地方************************************************/
/*
【移植适配·本工程 Template4T5L】只固定使用版本 5（屏端也只支持 5 版，37x37）：
  - 版本表 QR_VersonInfo 只保留 Ver.0~Ver.5；
  - EncodeData 已删除版本自适应搜索，内容超过版本5容量时直接编码失败（不再放大版本）；
  - 因此链接内容上限固定为 84 字节（M 级 15% 纠错，对应 QR_DATA_MAX_LEN）。
    超过 84 字节不会显示任何二维码（qr_page.c 会清空该区域）。
  若以后要用更高版本，需从源工程拷回版本表/RS表并恢复 EncodeData 的自适应段落。

以下为源工程原说明（版本1-14）：
仅适配版本1-14的QR二维码(显示端),链接长度上限随纠错等级QR_ECC_LEVEL变化(L级为458字节). 各版本容量区间由getSuitableVersion按当前纠错等级动态计算.
各个版本号对应的数据量和控件数不同,版本越高数据越多,占用的DGUS地址越多,可根据使用情况适配.
每个控件最多显示250个数据(实测单个批量图标控件上限256项,覆盖文档4096),根据版本号来确定需要用多少个控件/地址,
未用到的版本可不放置控件,省DGUS地址空间.
裁剪区(黑白两入口,共8字)全区共用,所有控件统一指向ICL_DATA_VP,不再随控件号递减.
两种显示方式由QR_DRAW_BLACK_ONLY宏预编译切换(见下): 黑白全画时每个码元都占1个图标项;
只画黑块(背景为白色)时白色码元不占图标项,黑色码元紧凑编号,区域控件组数与地址占用约减半.
两方式的控件组划分统一用QR_AREA_GROUP_NUM(nSize),地址占用公式与Ver5示例见main.c顶部"二维码(QR)逻辑实现说明".
*/

// #define MAX_ALLCODEWORD		3706//3706//400// //P14,P35 数据容量[码字]* (E) (VER:40), 所有码字为8位
// #define MAX_DATACODEWORD	2956//2956//400// //P27     最大信息码子(Ver：40-L)，所有码字为8位

#define MAX_ALLCODEWORD     140// 移植适配：源工程1089(v14)。固定v5总码字134，+余量6
#define MAX_DATACODEWORD    96// 移植适配：源工程855(v14-L)。固定v5-M数据码字86，+余量10
#define MAX_CODEBLOCK		80	// 移植适配：源工程149(v40)。固定v5-M单块43数据+24纠错=67，GetRSCodeWord最大读下标68
#define MAX_MODULESIZE		37// 移植适配：源工程40(v40)。固定v5码元数 = 21+(5-1)*4 = 37
#define MAX_TEST_TAB_SIZE 	(96) // 移植适配：源工程458(仅调试用)。链接内容上限84字节(+结束符)即可

//版本5、容错15%（M级）下二维码最大内容长度（字节），与getSuitableVersion容量表M行版本5对应
#define QR_DATA_MAX_LEN		84


/********************************************************************/

#define ONE_ROW_DIS_NUM			250    //一个控件可显示的数量(实测单控件上限256项)
#define INTERVAL_ADDR			0x400  //每个控件间隔地址(裁剪区全区共用,每控件只需VP+VP1=0x400字)

#define ICL_DATA_VP			0x5F00	//图标数据描述指针(全区共用裁剪区:黑白两入口各4字,共8字)

/* QR二维码显示方式（预编译切换，一次编译只保留一种，0=黑白全画 1=只画黑块）：
 * 黑白全画：每个码元都写1个图标项（黑块/白块裁剪入口各一），地址占用大；
 * 只画黑块：背景为白色，白色码元不写任何图标项，黑色码元按出现顺序紧凑编号，
 *           区域控件组数按黑密度上限65%预留，地址占用约减半（显存模式图层自动重绘）。 */
#define QR_DRAW_BLACK_ONLY		1

/* 每区域批量图标控件组数（QR_printf绘制与QR_ClearQR清空共用，保证划分一致）：
 * 每组最多ONE_ROW_DIS_NUM(250)项，组间地址间隔INTERVAL_ADDR(0x400)字，
 * 每区域占用 ≈ 组数×0x400字（变量地址VP与位置VP各半，公式/示例见main.c顶部说明）。 */
#if QR_DRAW_BLACK_ONLY
// 只画黑块：组数=⌈13×nSize²/5000⌉（=⌈0.65×nSize²/250⌉，黑密度上限65%预留），如Ver5(37²)：4组
#define QR_AREA_GROUP_NUM(nSize)	((unsigned int)(((unsigned long)(nSize)*(nSize)*13UL + 4999UL) / 5000UL))
#else
// 黑白全画：每控件装⌊250/nSize⌋行（每行nSize项≤250），组数=⌈nSize/⌊250/nSize⌋⌉，如Ver5(37²)：7组
#define QR_AREA_GROUP_NUM(nSize)	(((nSize) + (ONE_ROW_DIS_NUM/(nSize)) - 1) / (ONE_ROW_DIS_NUM/(nSize)))
#endif

/* 二维码显示参数动态配置：NOR flash 0x3F000（字地址，大端字节序）
 * 多页块连续存放，块间无分隔符；扫描读到非 0x5AA5 即配置区结束。
 * 块头8字：magic(0x5AA5) | 页数u16 | 数量u16 | 预留10B
 * 每条16字：data_addr, icon_data_vp, shoe_vp, display_vp, pos_x, pos_y,
 *           white_x, white_y, black_x, black_y, mag（11个u16大端）+ 预留10B
 * 以上宏仅作格式参考，代码不再引用（全部经QR_CfgFindPage动态解析） */
#define QR_CFG_BASE_ADDR			0x30000UL	//配置区起始地址（字地址）
#define QR_CFG_MAGIC				0x5AA5		//块头标志
#define QR_CFG_HEADER_WORDS			8			//块头长度（字）
#define QR_CFG_RECORD_WORDS			16			//单条记录长度（字）
#define QR_CFG_MAX_RECORD			2			//单页最多二维码数（数量字段上限，超出视为非法配置）
#define QR_CFG_SCAN_MAX_BLOCKS		((QR_CFG_SCAN_END - QR_CFG_BASE_ADDR) / QR_CFG_HEADER_WORDS)	//配置区可容纳的最大块数（每块最小8字=512，安全阀）
#define QR_CFG_SCAN_END				0x31000UL	//扫描越界防护（字地址）
#define QR_CFG_MAX_PAGES			20			//配置缓存最大页数（20页×2码=40个二维码；超限时初始化打告警并截断）
#define QR_VERSION_SET_DEFAULT		5			//版本号固定值




extern int xdata m_nSymbleSize;
extern unsigned char xdata	 m_byModuleData[MAX_MODULESIZE][MAX_MODULESIZE];
extern unsigned int countTime;
extern char xdata test[MAX_TEST_TAB_SIZE];



//typedef unsigned char   		bool;      /* 8-bit */
typedef struct
{
	unsigned short  int	 ncRSBlock;		//纠错的块数
	unsigned short  int	 ncAllCodeWord;	//码字总数
	unsigned short  int	 ncDataCodeWord;	//指定纠错等级下的数据码字数
}RS_BLOCKINFO;
typedef struct
{
	unsigned short  int	 nVersionNo;			//ver 1~40
	unsigned short  int	 ncAllCodeWord; 		//码字总数=数据码字+纠错码字

	unsigned short  int	 ncDataCodeWord[4];	//指定纠错等级下的数据码字(0=L,1=M,2=Q,3=H)
	unsigned short  int	 ncAlignPoint;		//P61 表E1 校正图形 个数
	unsigned short  int	 nAlignPoint[6];		//P61 表E1 校正图形	行列坐标
	//(0=L,1=M,2=Q,3=H)
	RS_BLOCKINFO RS_BlockInfo1[4];	//纠错块1
	RS_BLOCKINFO RS_BlockInfo2[4];	//纠错块2
}QR_VERSIONINFO;

void QRShow(void);
void binaryBmpShow(unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize);
// void QR_printf(char *pstr,unsigned int nVersion_set);// unsigned int vpEncodeTime, unsigned int vpShowTime);
void qrEnlarge_multiple(int qr_multiple);
void qrEnlargeX2(void);
void qrEnlargeX3(void);
void generateData(int dataLen);

bool EncodeData(char *lpsSource,unsigned int nVersion_set);
int GetEncodeVersion(int nVersion, char *lpsSource, int ncLength);
int EncodeSourceData(char *lpsSource, int ncLength, int nVerGroup);
int GetBitLength(unsigned char	 nMode, int ncData, int nVerGroup);
int SetBitStream(int nIndex, unsigned short wData, int ncData);

bool IsNumeralData(unsigned char c);
bool IsAlphabetData(unsigned char c);
bool IsKanjiData(unsigned char c1, unsigned char c2);
bool IsChineseData(unsigned char c1, unsigned char c2);

unsigned char	 AlphabetToBinaly(unsigned char c);
unsigned short KanjiToBinaly(unsigned short wc);
unsigned short ChineseToBinaly(unsigned short wc);
void GetRSCodeWord(unsigned char	 *lpbyRSWork, int ncDataCodeWord, int ncRSCodeWord);
void FormatModule(void);
void SetFunctionModule(void);
void SetFinderPattern(int x, int y);
void SetAlignmentPattern(int x, int y);
void SetVersionPattern(void);
void SetCodeWordPattern(void);
void SetMaskingPattern(int nPatternNo);
void SetFormatInfoPattern(int nPatternNo);
int CountPenalty(void);
unsigned int getSuitableVersion(unsigned int dataLen);

/* 单个二维码实例：仅显示参数。
 * 结构体数组 = 多个独立二维码。编码结果复用全局工作区
 * （编码与显示同步串行执行，显示时工作区即本实例的编码结果）。 */
typedef struct
{
    unsigned int  pos_x;            //显示坐标X（屏幕像素）
    unsigned int  pos_y;            //显示坐标Y
    unsigned int  mag_factor;       //放大倍数（=裁剪块边长）
    unsigned int  shoe_vp_base;     //批量图标"变量地址VP"组基址（原ICL_SHOE_VP_1）
    unsigned int  display_vp_base;  //"显示位置指针VP"组基址（原ICL_DISPLAY_VP_1）
    unsigned int  version_set;      //版本号，0=自动适配（1~14）
    unsigned int  data_addr;        //二维码内容读取VP（原固定0x200B/0x210B）
    unsigned int  icon_data_vp;     //批量图标"图标数据描述指针"（原ICL_DATA_VP）
    unsigned int  white_x;          //切白色块位置X（原QR_White_Fase_X）
    unsigned int  white_y;          //切白色块位置Y
    unsigned int  black_x;          //切黑色块位置X（原QR_Black_Fase_X）
    unsigned int  black_y;          //切黑色块位置Y
    unsigned int  last_black;       //上次绘制的黑色码元数（仅运行时用，非flash配置字段；只画黑块方式清缩量残留用）
} QR_INSTANCE;

u8 QR_printf(QR_INSTANCE *inst,char *pstr);	//返回1=已显示，0=编码失败（如内容为空）

/* 扫描NOR flash配置区，查找page对应的配置块并解析进inst[]数组
 * 返回解析出的二维码数量（0=未找到该页或配置区损坏）
 * 上电初始化后仅查内存缓存，不再读flash（见QR_ConfigInit） */
u8 QR_CfgFindPage(u16 page, QR_INSTANCE *inst, u8 maxInst);

/* 上电一次性扫描NOR flash配置区到内存缓存，此后QR_CfgFindPage只查缓存。
 * 须在 EA=1 之后（Nor_Flash_read忙等依赖中断）、首次QR_CfgFindPage之前调用一次；
 * XDATALEN=0上电XDATA不初始化，缓存由本函数运行时填充 */
void QR_ConfigInit(void);

/* 清空实例的二维码图标显示（变量地址VP区整块置0=隐藏全部图标，退出页面时调用） */
void QR_ClearQR(QR_INSTANCE *inst);

#endif
