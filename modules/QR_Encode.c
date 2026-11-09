
/* 移植适配：源工程把 "string.h" 放最前，其 NULL 会与 T5LOSConfig.h 的 NULL 冲突
   产生 C317 告警，故把本文件自己的头文件提到最前（sys.h/T5LOSConfig.h 先定义 NULL） */
#include "QR_Encode.h"
#include "string.h"
#include "stdio.h"
#include <stdlib.h>
#include <math.h>
#include "uart.h"
/* 移植适配：源工程为 "nor_flash.h"（Nor_Flash_read），本工程复用 t5l_stc.c 里
   已有的 nor_flash_read()，声明见 t5l_stc.h，故改包含本工程的 t5l_stc.h */
#include "t5l_stc.h"

//unsigned int countTime = 0;
//= {"https://d.autel.com/c/AE0022A1GN4C03023B01"};
// char xdata test[MAX_TEST_TAB_SIZE] = {"SSID:MCQX-5611;CODE:12345678;MAC:56110A13667E;IP:10.0.0.1"};
char xdata test[MAX_TEST_TAB_SIZE];

//P28 表9 QR码符号各版本的纠错特效
QR_VERSIONINFO code QR_VersonInfo[] = {{0}, // Ver.0
										 { 1, // Ver.1
										    26,   19,   16,   13,    9,
										   0,   0,   0,   0,   0,   0,   0,
										   1,  26,  19,
										   1,  26,  16,
										   1,  26,  13,
										   1,  26,   9,
										   0,   0,   0,
										   0,   0,   0,
										   0,   0,   0,
										   0,   0,   0},
										 { 2, // Ver.2
										    44,   34,   28,   22,   16,
										   1,  18,   0,   0,   0,   0,   0,
										   1,  44,  34,
										   1,  44,  28,
										   1,  44,  22,
										   1,  44,  16,
										   0,   0,   0,
										   0,   0,   0,
										   0,   0,   0,
										   0,   0,   0},
										 { 3, // Ver.3
										    70,   55,   44,   34,   26,
										   1,  22,   0,   0,   0,   0,   0,
										   1,  70,  55,
										   1,  70,  44,
										   2,  35,  17,
										   2,  35,  13,
										   0,   0,   0,
										   0,   0,   0,
										   0,   0,   0,
										   0,   0,   0},
										 { 4, // Ver.4
										   100,   80,   64,   48,   36,
										   1,  26,   0,   0,   0,   0,   0,
										   1, 100,  80,
										   2,  50,  32,
										   2,  50,  24,
										   4,  25,   9,
										   0,   0,   0,
										   0,   0,   0,
										   0,   0,   0,
										   0,   0,   0},
										 { 5, // Ver.5
										   134,  108,   86,   62,   46,
										   1,  30,   0,   0,   0,   0,   0,
										   1, 134, 108,
										   2,  67,  43,
										   2,  33,  15,
										   2,  33,  11,
										   0,   0,   0,
										   0,   0,   0,
										   2,  34,  16,
										   2,  34,  12},
										 /* 移植适配：本工程固定版本5，版本表只保留 Ver.0~Ver.5；
										    Ver.6~Ver.14 已删除（省 666 字节代码空间），源工程见
										    "二维码替换 - flash读取版本\MODULES\QR_Encode.c" 第18~172行 */
//  {15, // Ver.15
										//    655,  523,  415,  295,  223,
										//    3,  26,  48,  70,   0,   0,   0,
										//    5, 109,  87,
										//    5,  65,  41,
										//    5,  54,  24,
										//   11,  36,  12,
										//    1, 110,  88,
										//    5,  66,  42,
										//    7,  55,  25,
										//    7,  37,  13},
										//  {16, // Ver.16
										//    733,  589,  453,  325,  253,
										//    3,  26,  50,  74,   0,   0,   0,
										//    5, 122,  98,
										//    7,  73,  45,
										//   15,  43,  19,
										//    3,  45,  15,
										//    1, 123,  99,
										//    3,  74,  46,
										//    2,  44,  20,
										//   13,  46,  16},
										//  {17, // Ver.17
										//    815,  647,  507,  367,  283,
										//    3,  30,  54,  78,   0,   0,   0,
										//    1, 135, 107,
										//   10,  74,  46,
										//    1,  50,  22,
										//    2,  42,  14,
										//    5, 136, 108,
										//    1,  75,  47,
										//   15,  51,  23,
										//   17,  43,  15},
										//  {18, // Ver.18
										//    901,  721,  563,  397,  313,
										//    3,  30,  56,  82,   0,   0,   0,
										//    5, 150, 120,
										//    9,  69,  43,
										//   17,  50,  22,
										//    2,  42,  14,
										//    1, 151, 121,
										//    4,  70,  44,
										//    1,  51,  23,
										//   19,  43,  15},
										//  {19, // Ver.19
										//    991,  795,  627,  445,  341,
										//    3,  30,  58,  86,   0,   0,   0,
										//    3, 141, 113,
										//    3,  70,  44,
										//   17,  47,  21,
										//    9,  39,  13,
										//    4, 142, 114,
										//   11,  71,  45,
										//    4,  48,  22,
										//   16,  40,  14},
										//  {20, // Ver.20
										//   1085,  861,  669,  485,  385,
										//    3,  34,  62,  90,   0,   0,   0,
										//    3, 135, 107,
										//    3,  67,  41,
										//   15,  54,  24,
										//   15,  43,  15,
										//    5, 136, 108,
										//   13,  68,  42,
										//    5,  55,  25,
										//   10,  44,  16},
										//  {21, // Ver.21
										//   1156,  932,  714,  512,  406,
										//    4,  28,  50,  72,  94,   0,   0,
										//    4, 144, 116,
										//   17,  68,  42,
										//   17,  50,  22,
										//   19,  46,  16,
										//    4, 145, 117,
										//    0,   0,   0,
										//    6,  51,  23,
										//    6,  47,  17},
										//  {22, // Ver.22
										//   1258, 1006,  782,  568,  442,
										//    4,  26,  50,  74,  98,   0,   0,
										//    2, 139, 111,
										//   17,  74,  46,
										//    7,  54,  24,
										//   34,  37,  13,
										//    7, 140, 112,
										//    0,   0,   0,
										//   16,  55,  25,
										//    0,   0,   0},
										//  {23, // Ver.23
										//   1364, 1094,  860,  614,  464,
										//    4,  30,  54,  78, 102,   0,   0,
										//    4, 151, 121,
										//    4,  75,  47,
										//   11,  54,  24,
										//   16,  45,  15,
										//    5, 152, 122,
										//   14,  76,  48,
										//   14,  55,  25,
										//   14,  46,  16},
										//  {24, // Ver.24
										//   1474, 1174,  914,  664,  514,
										//    4,  28,  54,  80, 106,   0,   0,
										//    6, 147, 117,
										//    6,  73,  45,
										//   11,  54,  24,
										//   30,  46,  16,
										//    4, 148, 118,
										//   14,  74,  46,
										//   16,  55,  25,
										//    2,  47,  17},
										//  {25, // Ver.25
										//   1588, 1276, 1000,  718,  538,
										//    4,  32,  58,  84, 110,   0,   0,
										//    8, 132, 106,
										//    8,  75,  47,
										//    7,  54,  24,
										//   22,  45,  15,
										//    4, 133, 107,
										//   13,  76,  48,
										//   22,  55,  25,
										//   13,  46,  16},
										//  {26, // Ver.26
										//   1706, 1370, 1062,  754,  596,
										//    4,  30,  58,  86, 114,   0,   0,
										//   10, 142, 114,
										//   19,  74,  46,
										//   28,  50,  22,
										//   33,  46,  16,
										//    2, 143, 115,
										//    4,  75,  47,
										//    6,  51,  23,
										//    4,  47,  17},
										//  {27, // Ver.27
										//   1828, 1468, 1128,  808,  628,
										//    4,  34,  62,  90, 118,   0,   0,
										//    8, 152, 122,
										//   22,  73,  45,
										//    8,  53,  23,
										//   12,  45,  15,
										//    4, 153, 123,
										//    3,  74,  46,
										//   26,  54,  24,
										//   28,  46,  16},
										//  {28, // Ver.28
										//   1921, 1531, 1193,  871,  661,
										//    5,  26,  50,  74,  98, 122,   0,
										//    3, 147, 117,
										//    3,  73,  45,
										//    4,  54,  24,
										//   11,  45,  15,
										//   10, 148, 118,
										//   23,  74,  46,
										//   31,  55,  25,
										//   31,  46,  16},
										//  {29, // Ver.29
										//   2051, 1631, 1267,  911,  701,
										//    5,  30,  54,  78, 102, 126,   0,
										//    7, 146, 116,
										//   21,  73,  45,
										//    1,  53,  23,
										//   19,  45,  15,
										//    7, 147, 117,
										//    7,  74,  46,
										//   37,  54,  24,
										//   26,  46,  16},
										//  {30, // Ver.30
										//   2185, 1735, 1373,  985,  745,
										//    5,  26,  52,  78, 104, 130,   0,
										//    5, 145, 115,
										//   19,  75,  47,
										//   15,  54,  24,
										//   23,  45,  15,
										//   10, 146, 116,
										//   10,  76,  48,
										//   25,  55,  25,
										//   25,  46,  16},
										//  {31, // Ver.31
										//   2323, 1843, 1455, 1033,  793,
										//    5,  30,  56,  82, 108, 134,   0,
										//   13, 145, 115,
										//    2,  74,  46,
										//   42,  54,  24,
										//   23,  45,  15,
										//    3, 146, 116,
										//   29,  75,  47,
										//    1,  55,  25,
										//   28,  46,  16},
										//  {32, // Ver.32
										//   2465, 1955, 1541, 1115,  845,
										//    5,  34,  60,  86, 112, 138,   0,
										//   17, 145, 115,
										//   10,  74,  46,
										//   10,  54,  24,
										//   19,  45,  15,
										//    0,   0,   0,
										//   23,  75,  47,
										//   35,  55,  25,
										//   35,  46,  16},
										//  {33, // Ver.33
										//   2611, 2071, 1631, 1171,  901,
										//    5,  30,  58,  86, 114, 142,   0,
										//   17, 145, 115,
										//   14,  74,  46,
										//   29,  54,  24,
										//   11,  45,  15,
										//    1, 146, 116,
										//   21,  75,  47,
										//   19,  55,  25,
										//   46,  46,  16},
										//  {34, // Ver.34
										//   2761, 2191, 1725, 1231,  961,
										//    5,  34,  62,  90, 118, 146,   0,
										//   13, 145, 115,
										//   14,  74,  46,
										//   44,  54,  24,
										//   59,  46,  16,
										//    6, 146, 116,
										//   23,  75,  47,
										//    7,  55,  25,
										//    1,  47,  17},
										//  {35, // Ver.35
										//   2876, 2306, 1812, 1286,  986,
										//    6,  30,  54,  78, 102, 126, 150,
										//   12, 151, 121,
										//   12,  75,  47,
										//   39,  54,  24,
										//   22,  45,  15,
										//    7, 152, 122,
										//   26,  76,  48,
										//   14,  55,  25,
										//   41,  46,  16},
										//  {36, // Ver.36
										//   3034, 2434, 1914, 1354, 1054,
										//    6,  24,  50,  76, 102, 128, 154,
										//    6, 151, 121,
										//    6,  75,  47,
										//   46,  54,  24,
										//    2,  45,  15,
										//   14, 152, 122,
										//   34,  76,  48,
										//   10,  55,  25,
										//   64,  46,  16},
										//  {37, // Ver.37
										//   3196, 2566, 1992, 1426, 1096,
										//    6,  28,  54,  80, 106, 132, 158,
										//   17, 152, 122,
										//   29,  74,  46,
										//   49,  54,  24,
										//   24,  45,  15,
										//    4, 153, 123,
										//   14,  75,  47,
										//   10,  55,  25,
										//   46,  46,  16},
										//  {38, // Ver.38
										//   3362, 2702, 2102, 1502, 1142,
										//    6,  32,  58,  84, 110, 136, 162,
										//    4, 152, 122,
										//   13,  74,  46,
										//   48,  54,  24,
										//   42,  45,  15,
										//   18, 153, 123,
										//   32,  75,  47,
										//   14,  55,  25,
										//   32,  46,  16},
										//  {39, // Ver.39
										//   3532, 2812, 2216, 1582, 1222,
										//    6,  26,  54,  82, 110, 138, 166,
										//   20, 147, 117,
										//   40,  75,  47,
										//   43,  54,  24,
										//   10,  45,  15,
										//    4, 148, 118,
										//    7,  76,  48,
										//   22,  55,  25,
										//   67,  46,  16},
										//  {40, // Ver.40
										//   3706, 2956, 2334, 1666, 1276,
										//    6,  30,  58,  86, 114, 142, 170,
										//   19, 148, 118,
										//   18,  75,  47,
										//   34,  54,  24,
										//   20,  45,  15,
										//    6, 149, 119,
										//   31,  76,  48,
										//   34,  55,  25,
										//   61,  46,  16}
										 
										  
};

// GF(2^8) 指数2系数——本源多项式
/////////////////////////////////////////////////////////////////////////////
unsigned char code byExpToInt[] = {  1,   2,   4,   8,  16,  32,  64, 128,  29,  58, 116, 232, 205, 135,  19,  38,
							 76, 152,  45,  90, 180, 117, 234, 201, 143,   3,   6,  12,  24,  48,  96, 192,
							157,  39,  78, 156,  37,  74, 148,  53, 106, 212, 181, 119, 238, 193, 159,  35,
							 70, 140,   5,  10,  20,  40,  80, 160,  93, 186, 105, 210, 185, 111, 222, 161,
							 95, 190,  97, 194, 153,  47,  94, 188, 101, 202, 137,  15,  30,  60, 120, 240,
							253, 231, 211, 187, 107, 214, 177, 127, 254, 225, 223, 163,  91, 182, 113, 226,
							217, 175,  67, 134,  17,  34,  68, 136,  13,  26,  52, 104, 208, 189, 103, 206,
							129,  31,  62, 124, 248, 237, 199, 147,  59, 118, 236, 197, 151,  51, 102, 204,
							133,  23,  46,  92, 184, 109, 218, 169,  79, 158,  33,  66, 132,  21,  42,  84,
							168,  77, 154,  41,  82, 164,  85, 170,  73, 146,  57, 114, 228, 213, 183, 115,
							230, 209, 191,  99, 198, 145,  63, 126, 252, 229, 215, 179, 123, 246, 241, 255,
							227, 219, 171,  75, 150,  49,  98, 196, 149,  55, 110, 220, 165,  87, 174,  65,
							130,  25,  50, 100, 200, 141,   7,  14,  28,  56, 112, 224, 221, 167,  83, 166,
							 81, 162,  89, 178, 121, 242, 249, 239, 195, 155,  43,  86, 172,  69, 138,   9,
							 18,  36,  72, 144,  61, 122, 244, 245, 247, 243, 251, 235, 203, 139,  11,  22,
							 44,  88, 176, 125, 250, 233, 207, 131,  27,  54, 108, 216, 173,  71, 142,   1};
// GF(2^8) 系数2指数——本源多项式 
/////////////////////////////////////////////////////////////////////////////
unsigned char code byIntToExp[] = {  0,   0,   1,  25,   2,  50,  26, 198,   3, 223,  51, 238,  27, 104, 199,  75,
							  4, 100, 224,  14,  52, 141, 239, 129,  28, 193, 105, 248, 200,   8,  76, 113,
							  5, 138, 101,  47, 225,  36,  15,  33,  53, 147, 142, 218, 240,  18, 130,  69,
							 29, 181, 194, 125, 106,  39, 249, 185, 201, 154,   9, 120,  77, 228, 114, 166,
							  6, 191, 139,  98, 102, 221,  48, 253, 226, 152,  37, 179,  16, 145,  34, 136,
							 54, 208, 148, 206, 143, 150, 219, 189, 241, 210,  19,  92, 131,  56,  70,  64,
							 30,  66, 182, 163, 195,  72, 126, 110, 107,  58,  40,  84, 250, 133, 186,  61,
							202,  94, 155, 159,  10,  21, 121,  43,  78, 212, 229, 172, 115, 243, 167,  87,
							  7, 112, 192, 247, 140, 128,  99,  13, 103,  74, 222, 237,  49, 197, 254,  24,
							227, 165, 153, 119,  38, 184, 180, 124,  17,  68, 146, 217,  35,  32, 137,  46,
							 55,  63, 209,  91, 149, 188, 207, 205, 144, 135, 151, 178, 220, 252, 190,  97,
							242,  86, 211, 171,  20,  42,  93, 158, 132,  60,  57,  83,  71, 109,  65, 162,
							 31,  45,  67, 216, 183, 123, 164, 118, 196,  23,  73, 236, 127,  12, 111, 246,
							108, 161,  59,  82,  41, 157,  85, 170, 251,  96, 134, 177, 187, 204,  62,  90,
							203,  89,  95, 176, 156, 169, 160,  81,  11, 245,  22, 235, 122, 117,  44, 215,
							 79, 174, 213, 233, 230, 231, 173, 232, 116, 214, 244, 234, 168,  80,  88, 175};

//纠错码字的生成多项式 //P53 附录A
//这是加罗瓦域2^8以    100011101         表示主模块多项式：
//X^8+X^4+X^3+X^2+1
/////////////////////////////////////////////////////////////////////////////
unsigned char code byRSExp7[]  = {87, 229, 146, 149, 238, 102,  21};
unsigned char code byRSExp10[] = {251,  67,  46,  61, 118,  70,  64,  94,  32,  45};
unsigned char code byRSExp13[] = { 74, 152, 176, 100,  86, 100, 106, 104, 130, 218, 206, 140,  78};
unsigned char code byRSExp15[] = {  8, 183,  61,  91, 202,  37,  51,  58,  58, 237, 140, 124,   5,  99, 105};
unsigned char code byRSExp16[] = {120, 104, 107, 109, 102, 161,  76,   3,  91, 191, 147, 169, 182, 194, 225, 120};
unsigned char code byRSExp17[] = { 43, 139, 206,  78,  43, 239, 123, 206, 214, 147,  24,  99, 150,  39, 243, 163, 136};
unsigned char code byRSExp18[] = {215, 234, 158,  94, 184,  97, 118, 170,  79, 187, 152, 148, 252, 179,   5,  98,  96, 153};
unsigned char code byRSExp20[] = { 17,  60,  79,  50,  61, 163,  26, 187, 202, 180, 221, 225,  83, 239, 156, 164, 212, 212, 188, 190};
unsigned char code byRSExp22[] = {210, 171, 247, 242,  93, 230,  14, 109, 221,  53, 200,  74,   8, 172,  98,  80, 219, 134, 160, 105,
						   165, 231};
unsigned char code byRSExp24[] = {229, 121, 135,  48, 211, 117, 251, 126, 159, 180, 169, 152, 192, 226, 228, 218, 111,   0, 117, 232,
						    87,  96, 227,  21};
unsigned char code byRSExp26[] = {173, 125, 158,   2, 103, 182, 118,  17, 145, 201, 111,  28, 165,  53, 161,  21, 245, 142,  13, 102,
						    48, 227, 153, 145, 218,  70};
unsigned char code byRSExp28[] = {168, 223, 200, 104, 224, 234, 108, 180, 110, 190, 195, 147, 205,  27, 232, 201,  21,  43, 245,  87,
						    42, 195, 212, 119, 242,  37,   9, 123};
unsigned char code byRSExp30[] = { 41, 173, 145, 152, 216,  31, 179, 182,  50,  48, 110,  86, 239,  96, 222, 125,  42, 173, 226, 193,
						   224, 130, 156,  37, 251, 216, 238,  40, 192, 180};
/* 移植适配：版本表只保留到 Ver.14（源工程已如此），Ver.1~14 各纠错等级下
   "每块纠错码字数 EC/block"最大为 30（v9-L / v11-M / v14-L），故此处的
   byRSExp32~68 这 18 张生成多项式表（共 900 字节）不会被索引，删除以省代码空间。
   源工程原文见 "二维码替换 - flash读取版本\MODULES\QR_Encode.c" 第506~579行，需要时拷回 */

unsigned char	*byRSExp[] = {NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      byRSExp7,  NULL,      NULL,
							byRSExp10, NULL,      NULL,      byRSExp13, NULL,      byRSExp15, byRSExp16, byRSExp17, byRSExp18, NULL,
							byRSExp20, NULL,      byRSExp22, NULL,      byRSExp24, NULL,      byRSExp26, NULL,      byRSExp28, NULL,
							byRSExp30, NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,
							NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,
							NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,
							NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL,      NULL};

unsigned char code nIndicatorLenNumeral[]  = {10, 12, 14};
unsigned char code nIndicatorLenAlphabet[] = { 9, 11, 13};
unsigned char code nIndicatorLen8Bit[]	   = { 8, 16, 16};
unsigned char code nIndicatorLenKanji[]	   = { 8, 10, 12};

int xdata m_nSymbleSize;
unsigned char xdata m_byModuleData[MAX_MODULESIZE][MAX_MODULESIZE];

int xdata m_ncDataCodeWordBit;
unsigned char xdata	 m_byDataCodeWord[MAX_DATACODEWORD];

int m_ncDataBlock;
unsigned char xdata m_byBlockMode[MAX_DATACODEWORD];
unsigned int xdata m_nBlockLength[MAX_DATACODEWORD];

int m_ncAllCodeWord; 
unsigned char xdata	 m_byAllCodeWord[MAX_ALLCODEWORD]; 
unsigned char xdata	 m_byRSWork[MAX_CODEBLOCK];

int xdata m_nLevel;	
int xdata m_nVersion;	
bool xdata m_bAutoExtent;	
int xdata m_nMaskingNo;

//写DGUS变量数据 addr=DGUS变量地址  *buf=写入数据指针  len=写入数据字长度
void qr_writeDgusiiVp(unsigned long addr, unsigned char *buf, unsigned int len)
{   
	unsigned long OS_addr = 0;
	unsigned int OS_addr_offset = 0;
	unsigned int OS_len = 0, OS_len_offset = 0;
	OS_addr = addr >> 1;
	OS_addr_offset = addr & 0x01;
	ADR_H = (unsigned char)(OS_addr >> 16);
	ADR_M = (unsigned char)(OS_addr >> 8);
	ADR_L = (unsigned char)OS_addr;
	ADR_INC = 0x01; //DGUS  OS存储器地址在读写后自动加1
	RAMMODE = 0x83;
	while (!APP_ACK); //等待
	if (OS_addr_offset)
	{
		DATA1 = *buf++;
		DATA0 = *buf++;
		APP_EN = 1;
		while(APP_EN);
		len--;
	}
	OS_len = len >> 1;
	OS_len_offset = len & 0x01;
	RAMMODE = 0x8F; //启动写模式
	while (OS_len--)
	{
		DATA3 = *buf++;
		DATA2 = *buf++;
		DATA1 = *buf++;
		DATA0 = *buf++;
		APP_EN = 1;
		while (APP_EN);
	}
	if (OS_len_offset)
	{
		RAMMODE = 0x8C;
		DATA3 = *buf++;
		DATA2 = *buf++;
		APP_EN = 1;
		while (APP_EN);
	}
	RAMMODE = 0x00; //读写完成后RAMMODE必须置零
}




/* 配置块读取缓冲：块头8字（Nor_Flash_read长度必须为偶数） */
static u8 xdata qrCfgHdr[16];

/* 上电缓存：QR_ConfigInit一次性扫描配置区填充，页面切换只查内存不再读flash */
typedef struct
{
	u16 page;	//页面号（块头字节2~3，大端解码）
	u8  cnt;	//该页二维码数量（1~2）
	u8  rec[QR_CFG_MAX_RECORD][QR_CFG_RECORD_WORDS * 2];	//原始记录字节，与flash大端格式一致
} QR_PAGE_CFG;
static QR_PAGE_CFG xdata qrPageCfg[QR_CFG_MAX_PAGES];
static u8 xdata qrPageCnt;	//有效条目数（查找只能以此界定，XDATALEN=0其余条目是垃圾值）

/// @brief 在内存缓存中查找page对应的配置块并解析进inst[]数组
///        （配置由QR_ConfigInit上电一次性从NOR flash读入，此处不再读flash）
/// @param page  当前显示的页面号（配置块按页数匹配）
/// @param inst  接收解析结果的二维码实例数组
/// @param maxInst 实例数组最大容量（单页最多支持的数量）
/// @return 解析出的二维码数量；0=未找到该页，或配置区空白/损坏
/* 配置记录字段序(0..10: data_addr,icon_data_vp,shoe_vp_base,display_vp_base,pos_x,pos_y,
   white_x,white_y,black_x,black_y,mag) -> QR_INSTANCE 的 u16 成员下标 */
static u8 code qrCfgFieldMap[11] = {6, 7, 3, 4, 0, 1, 8, 9, 10, 11, 2};

/* 编译期自检：QR_INSTANCE 的 13 个成员全是 unsigned int（C51 大模式 2 字节）且无填充，
   故 sizeof 必须为 26。若以后有人增删/重排成员，这里会直接编译报错（数组长度为负），
   提醒同步修改上面的映射表与下面的 u16 下标访问。 */
static u8 code qrInstSizeCheck[(sizeof(QR_INSTANCE) == 26) ? 1 : -1];

u8 QR_CfgFindPage(u16 page, QR_INSTANCE *inst, u8 maxInst)
{
	u8 p, i, k, found;
	u16 *pDst;

	for (p = 0; p < qrPageCnt; p++)	//只能按qrPageCnt界定（XDATALEN=0其余条目是垃圾值）
	{
		if (qrPageCfg[p].page != page)
			continue;
		found = (qrPageCfg[p].cnt > maxInst) ? maxInst : qrPageCfg[p].cnt;
		for (i = 0; i < found; i++)
		{
			/* 移植适配：记录11个字段与 QR_INSTANCE 成员一一对应但顺序不同，
			   用映射表+循环代替源工程11次手写赋值（省代码空间）。
			   QR_INSTANCE 前13个成员全是 unsigned int（C51 大模式2字节、无填充），
			   可用 u16 下标访问：0 pos_x, 1 pos_y, 2 mag_factor, 3 shoe_vp_base,
			   4 display_vp_base, 5 version_set(不来自flash), 6 data_addr, 7 icon_data_vp,
			   8 white_x, 9 white_y, 10 black_x, 11 black_y, 12 last_black */
			pDst = (u16 *)&inst[i];
			for (k = 0; k < 11; k++)
			{
				pDst[qrCfgFieldMap[k]] = COMBINE_U16(qrPageCfg[p].rec[i][k * 2], qrPageCfg[p].rec[i][k * 2 + 1]);
			}
			inst[i].last_black      = 0;	//运行字段：上次黑点数归零（非flash配置字段，进页首次绘制按全量处理）
		}
		return found;
	}
	return 0;
}

/* 整控件清空缓冲：250条目×2字×2字节=1000B，全0=隐藏全部图标。
   移植适配：源工程写 static ... = {0}，1000字节初值会在代码空间占1000字节初始化镜像
   （本工程CODE已满），改为无初值 + QR_ConfigInit()上电清零，效果相同、省1KB代码空间 */
static u8 xdata qrClearBuf[ONE_ROW_DIS_NUM * 4];

/// @brief 上电初始化：一次性扫描NOR flash配置区(0x3F000起)填充qrPageCfg缓存，
///        之后QR_CfgFindPage只查缓存不再读flash。
///        须在 EA=1 后（Nor_Flash_read忙等依赖中断）、首次QR_CfgFindPage前调用。
///        终止条件与原扫描一致（读到非magic/非法数量/越界即配置区结束），
///        超过QR_CFG_MAX_PAGES页时打告警并截断。
void QR_ConfigInit(void)
{
	u32 addr = QR_CFG_BASE_ADDR;
	u16 cnt, i;
	u8 p = 0;
	u16 block;	//上限512，必须u16（u8会溢出成死循环）

	qrPageCnt = 0;
	memset(qrClearBuf, 0, sizeof(qrClearBuf));	//移植适配：清屏缓冲改无初值，上电在此清零（见qrClearBuf定义）

	for (block = 0; block < QR_CFG_SCAN_MAX_BLOCKS; block++)
	{
		if (addr + QR_CFG_HEADER_WORDS > QR_CFG_SCAN_END)
			break;
		nor_flash_read(addr, qrCfgHdr, QR_CFG_HEADER_WORDS);	//移植适配：源为 Nor_Flash_read()
		if (COMBINE_U16(qrCfgHdr[0], qrCfgHdr[1]) != QR_CFG_MAGIC)
			break;							//读到非magic（空白区0xFF/损坏）即配置区结束
		cnt = COMBINE_U16(qrCfgHdr[4], qrCfgHdr[5]);	//数量（块头字节4~5，样例: 00 02 = 2）
		if (cnt > QR_CFG_MAX_RECORD || cnt == 0)
			break;							//非法配置，安全终止
		addr += QR_CFG_HEADER_WORDS;
		if (addr + (u32)cnt * QR_CFG_RECORD_WORDS > QR_CFG_SCAN_END)
			break;							//记录区越出配置区，安全终止
		if (p < QR_CFG_MAX_PAGES)
		{
			qrPageCfg[p].page = COMBINE_U16(qrCfgHdr[2], qrCfgHdr[3]);	//页数（块头字节2~3，样例: 00 0B = 11）
			qrPageCfg[p].cnt = cnt;
			for (i = 0; i < cnt; i++)
			{
				nor_flash_read(addr + (u32)i * QR_CFG_RECORD_WORDS, qrPageCfg[p].rec[i], QR_CFG_RECORD_WORDS);
			}
			p++;
		}
		else
		{
			// printf("QR cfg pages over %d, truncated\r\n", QR_CFG_MAX_PAGES);
			break;
		}
		addr += (u32)cnt * QR_CFG_RECORD_WORDS;	//跳到下一块头（无块尾）
	}
	qrPageCnt = p;
}

/// @brief 清空实例的二维码图标显示（变量地址VP区整块置0=隐藏全部图标）
/// @param inst 二维码实例（显示参数由调用者填充）
void QR_ClearQR(QR_INSTANCE *inst)
{
	u16 nSize, control_end, z;

	if (inst == NULL) return;
	nSize = 21 + (inst->version_set - 1) * 4;	//版本号→模块数：Ver:40 = 21+（Ver-1）*4
	control_end = QR_AREA_GROUP_NUM(nSize);		//控件组数（黑白全画/只画黑块两方式统一公式，见QR_Encode.h），与QR_printf划分一致
	for (z = 0; z < control_end; z++)
	{
		writeDgusiiVp(inst->shoe_vp_base + z * INTERVAL_ADDR, qrClearBuf, ONE_ROW_DIS_NUM * 2);
	}
	inst->last_black = 0;	//整区已清零，上次黑点数归零（只画黑块方式缩量残留依据）
}

/*
 * 功能：两张图片剪切显示，需要在UI放置好批量数据图标。
 * 参数：
 */
void QR_Batch_ICON(u16 ADDR_VP, u16 DISPLAY_VP, u16 show_x, u16 show_y, u16 TB_num, u8 posNum)
{
    u8 tmp[4];
    u16 display_vp[2];

    display_vp[0] = show_x;
    display_vp[1] = show_y;
    writeDgusiiVp(DISPLAY_VP + TB_num * 2, (u8 *)display_vp, 2); //图标显示位置指针

    tmp[0] = 0x5a;
    tmp[1] = posNum; //图标编号：0=黑块 1=白块（与裁剪入口对应）
    tmp[2] = 0x80;
    tmp[3] = 0xff;
    writeDgusiiVp(ADDR_VP + TB_num * 2, (u8 *)tmp, 2); //变量地址VP

    return;
}


int calculate_floor_x_squared_over(int nSize, int x)
{
    float calculation_result;
    int floor_result;

    calculation_result = (float)nSize / (float)x;
    // 使用ceilr实现向上取整
    floor_result = (int)ceil(calculation_result);

    return floor_result;
}

/// @brief QR二维码的编码与显示，最大支持版本14
///        显示方式由QR_DRAW_BLACK_ONLY宏预编译切换：0=黑白全画（每个码元1图标项）；
///        1=只画黑块（背景白色，白码元不占图标项、黑码元紧凑编号，地址占用约减半，
///        白块裁剪入口不再写入，组划分见QR_AREA_GROUP_NUM）
/// @param inst 二维码实例（显示参数由调用者填充，编码结果写入实例）
/// @param pstr 内容/链接数组
/// @return 1=已显示，0=编码失败（如内容为空）
u8 QR_printf(QR_INSTANCE *inst,char *pstr)
{
	int nSize, mag;
#if QR_DRAW_BLACK_ONLY
	int i, j;
#else
	int z, i, j;
	int control_num, line, row;
	int control_end, line_end;
	u8 posNum;
#endif
	// batch_icon_sp(1089, 32, 0);//显存模式
	if (inst == NULL) return 0;
	if (EncodeData(pstr,inst->version_set)) // QR码编码
	{
		u16 icon_vp[4];	//裁剪区写入缓冲（黑/白两入口各写一次，全码共用）
		nSize = m_nSymbleSize;   //编码刚完成，工作区即本实例的编码结果
		mag = inst->mag_factor;
		icon_vp[0] = inst->black_x;
		icon_vp[1] = inst->black_y;
		icon_vp[2] = mag;
		icon_vp[3] = mag;
		writeDgusiiVp(inst->icon_data_vp, (u8 *)icon_vp, 4);		//黑块裁剪入口（posNum=0）
#if QR_DRAW_BLACK_ONLY
		// 只画黑块（背景为白色）：白色码元不写任何图标项，黑色码元按出现顺序
		// 紧凑编号（每图标项自带显示位置，无须按码元格位对齐），每控件组最多250项
		{
			u16 k = 0;				//黑色码元累计序号（跨控件组连续编号）
			u16 group, tb;			//组号（0起）与组内项号
			u8 zero[2] = {0, 0};	//槽位清零缓冲（写0=隐藏该图标项）
			for ( i = 0; i < nSize; i++)
			{
				for ( j = 0; j < nSize; j++)
				{
					if (!m_byModuleData[i][j])
						continue;	//白色码元：不描绘（背景即白色）
					if (k >= (u16)QR_AREA_GROUP_NUM(nSize) * ONE_ROW_DIS_NUM)
						return 0;	//超出预留容量（>65%黑密度极端内容）：按编码失败处理（调用方清区）
					group = k / ONE_ROW_DIS_NUM;
					tb = k % ONE_ROW_DIS_NUM;
					QR_Batch_ICON( inst->shoe_vp_base + group*INTERVAL_ADDR,
								inst->display_vp_base + group*INTERVAL_ADDR,
								inst->pos_x + mag*j,
								inst->pos_y + mag*i,
								tb, 0 );	//posNum恒为0=黑块入口
					k++;
				}
			}
			// 清缩量残留：上次绘制到 inst->last_black-1 槽，本次只重写到 k-1 槽，
			// 多出的旧槽位若不清零，其残留图标项会按旧坐标继续显示
			// （显存模式图层重绘只清图层，不清DGUS数据表）
			while (k < inst->last_black)
			{
				group = k / ONE_ROW_DIS_NUM;
				tb = k % ONE_ROW_DIS_NUM;
				writeDgusiiVp(inst->shoe_vp_base + group*INTERVAL_ADDR + tb*2, zero, 1);	//槽位清零=隐藏
				k++;
			}
			inst->last_black = k;
		}
#else
		icon_vp[0] = inst->white_x;
		icon_vp[1] = inst->white_y;
		writeDgusiiVp(inst->icon_data_vp + 4, (u8 *)icon_vp, 4);	//白块裁剪入口（posNum=1）
		line = ONE_ROW_DIS_NUM / nSize;
		control_num = nSize / line;
		row = nSize;
		for ( z = 0; z < control_num; z++)//控件数
		{
			for ( i = 0; i < line; i++)//一个最大存几行
			{
				for ( j = 0; j < row; j++)
				{
					if (m_byModuleData[i+z*line][j])//黑色
					{
						posNum = 0;
					}else{//白色
						posNum = 1;
					}
					QR_Batch_ICON( inst->shoe_vp_base + z*INTERVAL_ADDR,
								inst->display_vp_base + z*INTERVAL_ADDR,
								inst->pos_x + mag*j,
								inst->pos_y + mag*i+z*line*mag,
								i*row+j, posNum );
				}
			}
		}
		control_end = calculate_floor_x_squared_over(nSize,line);
		line_end = nSize - (control_num * line);
		for ( z = control_num; z < control_end; z++)
		{
			for ( i = 0; i < line_end; i++)
			{
				for ( j = 0; j < row; j++)
				{
					if (m_byModuleData[i+z*line][j])//黑色
					{
						posNum = 0;
					}else{//白色
						posNum = 1;
					}
					QR_Batch_ICON( inst->shoe_vp_base + z*INTERVAL_ADDR,
								inst->display_vp_base + z*INTERVAL_ADDR,
								inst->pos_x + mag*j,
								inst->pos_y + mag*i+z*line*mag,
								i*row+j, posNum );
				}
			}
		}
#endif
		return 1;
	}
	else
	{
		// printf("EncodeData failed\n");
		return 0;
	}
}


//自动生成字符型数据 "0123456789012345..."
void generateData(int dataLen)
{
	int i;
	memset(test, 0, sizeof(test));
	for (i = 0; i < dataLen; i++)
		test[i] = '1' + i % 10;
}

//编码数据
//lpsSource 源数据
bool EncodeData(char *lpsSource,unsigned int nVersion_set)
{
	int i, j, ncSource;			//移植适配：nVersion/bAutoExtent/nEncodeVersion 随版本自适应一并删除
	int ncLength, ncDataCodeWord, ncTerminater;
	unsigned char	 byPaddingCode = 0xec;
	int nDataCwIndex = 0, ncBlock1, ncBlock2, ncBlockSum; 
	int nBlockNo = 0, ncDataCw1, ncDataCw2;
	int ncRSCw1, ncRSCw2;

	int testEncodeData;

	ncSource = 0;
    
	m_nLevel = QR_ECC_LEVEL;	//纠错等级：QR_LEVEL_L/M/Q/H（宏定义于QR_Encode.h）
	m_nMaskingNo = -1;	//-1=启用8种掩码罚分搜索，选观感最佳掩码

		//计算要编码的数据的长度：0xFF（DGUS变量区未写满部分）与'\0'同为结束符，避免尾部填充被当作内容编码
	if (ncSource > 0)
		ncLength = ncSource;
	else
		for (ncLength = 0; lpsSource[ncLength] != '\0' && (unsigned char)lpsSource[ncLength] != 0xFF; ++ncLength)
			;

	if (ncLength == 0)
		return FALSE;
	/* 移植适配：本工程固定使用版本 5（QR_VERSION_SET_DEFAULT，屏端也只支持 5 版），
	   删除版本自适应搜索（原 getSuitableVersion + GetEncodeVersion，省约 700 字节代码空间）：
	   只做一次 1~9 版组(QR_VRESION_S)的模式分析，再按版本5容量校验，装不下即失败。
	   同时消除原代码自适应放大版本时的越界隐患——m_byModuleData 只有
	   MAX_MODULESIZE(40)x40，版本6 需要 41x41。 */
	if (nVersion_set > 5)		//版本参数超出本工程支持范围（固定5版）
		return FALSE;
	m_nVersion = nVersion_set;
	if (!EncodeSourceData(lpsSource, ncLength, QR_VRESION_S))	//模式分析+码字统计
		return FALSE;
	if ((m_ncDataCodeWordBit + 7) / 8 > QR_VersonInfo[m_nVersion].ncDataCodeWord[m_nLevel])
		return FALSE;		//内容超过版本5在当前纠错等级下的容量

	ncDataCodeWord = QR_VersonInfo[m_nVersion].ncDataCodeWord[m_nLevel];

	ncTerminater = min(4, (ncDataCodeWord * 8) - m_ncDataCodeWordBit);

	if (ncTerminater > 0)
		m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, 0, ncTerminater);
		testEncodeData = (m_ncDataCodeWordBit + 7) / 8;

	for (i = (m_ncDataCodeWordBit + 7) / 8; i < ncDataCodeWord; ++i)
	{
		m_byDataCodeWord[i] = byPaddingCode;

		byPaddingCode = (unsigned char	)(byPaddingCode == 0xec ? 0x11 : 0xec);

	}

	m_ncAllCodeWord = QR_VersonInfo[m_nVersion].ncAllCodeWord;
	memset(m_byAllCodeWord, 0, m_ncAllCodeWord);

	ncBlock1 = QR_VersonInfo[m_nVersion].RS_BlockInfo1[m_nLevel].ncRSBlock;
	ncBlock2 = QR_VersonInfo[m_nVersion].RS_BlockInfo2[m_nLevel].ncRSBlock;
	ncBlockSum = ncBlock1 + ncBlock2;

	ncDataCw1 = QR_VersonInfo[m_nVersion].RS_BlockInfo1[m_nLevel].ncDataCodeWord;
	ncDataCw2 = QR_VersonInfo[m_nVersion].RS_BlockInfo2[m_nLevel].ncDataCodeWord;

	for (i = 0; i < ncBlock1; ++i)
	{
		for (j = 0; j < ncDataCw1; ++j)
		{
			m_byAllCodeWord[(ncBlockSum * j) + nBlockNo] = m_byDataCodeWord[nDataCwIndex++];
		}

		++nBlockNo;
	}

	for (i = 0; i < ncBlock2; ++i)
	{
		for (j = 0; j < ncDataCw2; ++j)
		{
			if (j < ncDataCw1)
			{
				m_byAllCodeWord[(ncBlockSum * j) + nBlockNo] = m_byDataCodeWord[nDataCwIndex++];
			}
			else
			{
				m_byAllCodeWord[(ncBlockSum * ncDataCw1) + i]  = m_byDataCodeWord[nDataCwIndex++];
			}	
		}

		++nBlockNo;
	}

	ncRSCw1 = QR_VersonInfo[m_nVersion].RS_BlockInfo1[m_nLevel].ncAllCodeWord - ncDataCw1;
	ncRSCw2 = QR_VersonInfo[m_nVersion].RS_BlockInfo2[m_nLevel].ncAllCodeWord - ncDataCw2;
	nDataCwIndex = 0;
	nBlockNo = 0;

	for (i = 0; i < ncBlock1; ++i)
	{
		memset(m_byRSWork, 0, sizeof(m_byRSWork));
		memmove(m_byRSWork, m_byDataCodeWord + nDataCwIndex, ncDataCw1);
		GetRSCodeWord(m_byRSWork, ncDataCw1, ncRSCw1);
		for (j = 0; j < ncRSCw1; ++j)
		{
			m_byAllCodeWord[ncDataCodeWord + (ncBlockSum * j) + nBlockNo] = m_byRSWork[j];
		}
		nDataCwIndex += ncDataCw1;
		++nBlockNo;
	}

	for (i = 0; i < ncBlock2; ++i)
	{
		memset(m_byRSWork, 0, sizeof(m_byRSWork));

		memmove(m_byRSWork, m_byDataCodeWord + nDataCwIndex, ncDataCw2);

		GetRSCodeWord(m_byRSWork, ncDataCw2, ncRSCw2);

		for (j = 0; j < ncRSCw2; ++j)
		{
			m_byAllCodeWord[ncDataCodeWord + (ncBlockSum * j) + nBlockNo] = m_byRSWork[j];
		}

		nDataCwIndex += ncDataCw2;
		++nBlockNo;
	}

	m_nSymbleSize = m_nVersion * 4 + 17;

	FormatModule();

	return TRUE;
}
//每个版本长度字段的位数
//nVersion	版本
//lpsSource	源数据
//ncLength	长度
/*
Number of bits per length field
Encoding		Ver.1–9		10–26		27–40
Numeric			10			12			14
Alphanumeric	9			11			13
Byte			8			16			16
Kanji			8			10			12 
*/
int GetEncodeVersion(int nVersion, char *lpsSource, int ncLength)
{
	int nVerGroup = nVersion >= 27 ? QR_VRESION_L : (nVersion >= 10 ? QR_VRESION_M : QR_VRESION_S);
	int i, j;

	for (i = nVerGroup; i <= QR_VRESION_L; ++i)
	{
		if (EncodeSourceData(lpsSource, ncLength, i))
		{
			if (i == QR_VRESION_S)
			{
				for (j = 1; j <= 9; ++j)
				{
					if ((m_ncDataCodeWordBit + 7) / 8 <= QR_VersonInfo[j].ncDataCodeWord[m_nLevel])
						return j;
				}
			}
			else if (i == QR_VRESION_M)
			{
				for (j = 10; j <= 26; ++j)
				{
					if ((m_ncDataCodeWordBit + 7) / 8 <= QR_VersonInfo[j].ncDataCodeWord[m_nLevel])
						return j;
				}
			}
			else if (i == QR_VRESION_L)
			{
				for (j = 27; j <= 40; ++j)
				{
					if ((m_ncDataCodeWordBit + 7) / 8 <= QR_VersonInfo[j].ncDataCodeWord[m_nLevel])
						return j;
				}
			}
		}
	}
	return 0;
}
//模式指标，编码模式
//lpsSource 源数据
//ncLength	数据长度
//nVerGroup	版本组
//bool EncodeSourceData(char *lpsSource, int ncLength, int nVerGroup)
int EncodeSourceData(char *lpsSource, int ncLength, int nVerGroup)
{
	int i, j;
	int ncSrcBits, ncDstBits;
	int nBlock = 0;
	int ncComplete = 0; 
	unsigned short wBinCode;
	unsigned char byMode;

	memset(m_nBlockLength, 0, sizeof(m_nBlockLength));
	
	for (m_ncDataBlock = i = 0; i < ncLength; ++i)
	{
		//默认字符 加速
		if (IsAlphabetData(lpsSource[i]))
			byMode = QR_MODE_ALPHABET;			//字符
		else if (i < ncLength - 1 && IsKanjiData(lpsSource[i], lpsSource[i + 1]))
			byMode = QR_MODE_KANJI;				//日文
		else if(i < ncLength - 1 && IsChineseData(lpsSource[i], lpsSource[i + 1]))
			byMode = QR_MODE_CHINESE;			//中文
		else if (IsNumeralData(lpsSource[i]))	
			byMode = QR_MODE_NUMERAL;			//数字
		else
			byMode = QR_MODE_8BIT;				//字节
		// byMode = QR_MODE_ALPHABET;
		if (i == 0)
			m_byBlockMode[0] = byMode;			//

		if (m_byBlockMode[m_ncDataBlock] != byMode)//如果前后两个数据的模式不一样
			m_byBlockMode[++m_ncDataBlock] = byMode;

		++m_nBlockLength[m_ncDataBlock];

		if (byMode == QR_MODE_KANJI)//如果是日文模式，那么前后2个字节为一个汉字
		{
			++m_nBlockLength[m_ncDataBlock];
			++i;
		}
	}

	++m_ncDataBlock;

	while (nBlock < m_ncDataBlock - 1)
	{
		int ncJoinFront, ncJoinBehind; 
		int nJoinPosition = 0; 

		if ((m_byBlockMode[nBlock] == QR_MODE_NUMERAL  && m_byBlockMode[nBlock + 1] == QR_MODE_ALPHABET) ||
			(m_byBlockMode[nBlock] == QR_MODE_ALPHABET && m_byBlockMode[nBlock + 1] == QR_MODE_NUMERAL))
		{//如果前后2个数据块为字母和数字，可以合并成字母数字模式(字母模式包含了数字)  计算数据位流
			ncSrcBits = GetBitLength(m_byBlockMode[nBlock], m_nBlockLength[nBlock], nVerGroup) +
						GetBitLength(m_byBlockMode[nBlock + 1], m_nBlockLength[nBlock + 1], nVerGroup);

			ncDstBits = GetBitLength(QR_MODE_ALPHABET, m_nBlockLength[nBlock] + m_nBlockLength[nBlock + 1], nVerGroup);

			if (ncSrcBits > ncDstBits)
			{//如果合并前大于合并后长度
				if (nBlock >= 1 && m_byBlockMode[nBlock - 1] == QR_MODE_8BIT)
				{//判断之前的数据块是不是8bit模式，如果是的话合并数据(必须要有3个数据块以上判断)
					ncJoinFront = GetBitLength(QR_MODE_8BIT, m_nBlockLength[nBlock - 1] + m_nBlockLength[nBlock], nVerGroup) +
								  GetBitLength(m_byBlockMode[nBlock + 1], m_nBlockLength[nBlock + 1], nVerGroup);

					if (ncJoinFront > ncDstBits + GetBitLength(QR_MODE_8BIT, m_nBlockLength[nBlock - 1], nVerGroup))
						ncJoinFront = 0; //如果合并后的数据超过长度 非法
				}
				else
					ncJoinFront = 0;//不满足合并的条件

				if (nBlock < m_ncDataBlock - 2 && m_byBlockMode[nBlock + 2] == QR_MODE_8BIT)
				{//如果后面的数据块为8bit模式，合并数据(必须要有3个数据块以上判断)
					ncJoinBehind = GetBitLength(m_byBlockMode[nBlock], m_nBlockLength[nBlock], nVerGroup) +
								   GetBitLength(QR_MODE_8BIT, m_nBlockLength[nBlock + 1] + m_nBlockLength[nBlock + 2], nVerGroup);

					if (ncJoinBehind > ncDstBits + GetBitLength(QR_MODE_8BIT, m_nBlockLength[nBlock + 2], nVerGroup))
						ncJoinBehind = 0; //如果合并后的数据超过长度 非法
				}
				else
					ncJoinBehind = 0;//不满足合并的条件

				if (ncJoinFront != 0 && ncJoinBehind != 0)//前后的数据块合并后都不为0
				{
					nJoinPosition = (ncJoinFront < ncJoinBehind) ? -1 : 1;//位置的标记，如果前面的小于后面的为-1；大于等于为1
				}
				else
				{//如果有一个或者都为0		如果前面不为0为-1；后面不为0为1  否则为0
					nJoinPosition = (ncJoinFront != 0) ? -1 : ((ncJoinBehind != 0) ? 1 : 0);
				}

				if (nJoinPosition != 0)
				{//如果合并后的数据不为0
					if (nJoinPosition == -1)//前面有数据
					{
						m_nBlockLength[nBlock - 1] += m_nBlockLength[nBlock];

						for (i = nBlock; i < m_ncDataBlock - 1; ++i)
						{
							m_byBlockMode[i]  = m_byBlockMode[i + 1];
							m_nBlockLength[i] = m_nBlockLength[i + 1];
						}
					}
					else
					{//后面有数据
						m_byBlockMode[nBlock + 1] = QR_MODE_8BIT;//后面数据块为8BIT模式，将新的数据块模式定义成8BIT
						m_nBlockLength[nBlock + 1] += m_nBlockLength[nBlock + 2];//长度

						for (i = nBlock + 2; i < m_ncDataBlock - 1; ++i)
						{
							m_byBlockMode[i]  = m_byBlockMode[i + 1];
							m_nBlockLength[i] = m_nBlockLength[i + 1];
						}
					}

					--m_ncDataBlock;
				}
				else
				{//合并后数据异常，或者不满足合并条件
					if (nBlock < m_ncDataBlock - 2 && m_byBlockMode[nBlock + 2] == QR_MODE_ALPHABET)
					{//如果后面一块数据也是字母数字，合并数据
						m_nBlockLength[nBlock + 1] += m_nBlockLength[nBlock + 2];

						for (i = nBlock + 2; i < m_ncDataBlock - 1; ++i)
						{
							m_byBlockMode[i]  = m_byBlockMode[i + 1];
							m_nBlockLength[i] = m_nBlockLength[i + 1];
						}

						--m_ncDataBlock;
					}
					//设置新数据块为字母数字
					m_byBlockMode[nBlock] = QR_MODE_ALPHABET;
					m_nBlockLength[nBlock] += m_nBlockLength[nBlock + 1];

					for (i = nBlock + 1; i < m_ncDataBlock - 1; ++i)
					{
						m_byBlockMode[i]  = m_byBlockMode[i + 1];
						m_nBlockLength[i] = m_nBlockLength[i + 1];
					}

					--m_ncDataBlock;
					//如果前面一块数据也是字母数字，合并数据
					if (nBlock >= 1 && m_byBlockMode[nBlock - 1] == QR_MODE_ALPHABET)
					{
						m_nBlockLength[nBlock - 1] += m_nBlockLength[nBlock];

						for (i = nBlock; i < m_ncDataBlock - 1; ++i)
						{
							m_byBlockMode[i]  = m_byBlockMode[i + 1];
							m_nBlockLength[i] = m_nBlockLength[i + 1];
						}

						--m_ncDataBlock;
					}
				}

				continue;
			}
		}

		++nBlock; 
	}

	nBlock = 0;

	while (nBlock < m_ncDataBlock - 1)
	{//合并2个部分数据
		ncSrcBits = GetBitLength(m_byBlockMode[nBlock], m_nBlockLength[nBlock], nVerGroup)
					+ GetBitLength(m_byBlockMode[nBlock + 1], m_nBlockLength[nBlock + 1], nVerGroup);

		ncDstBits = GetBitLength(QR_MODE_8BIT, m_nBlockLength[nBlock] + m_nBlockLength[nBlock + 1], nVerGroup);

		if (nBlock >= 1 && m_byBlockMode[nBlock - 1] == QR_MODE_8BIT)//前一数据块为BIT模式
			ncDstBits -= (4 + nIndicatorLen8Bit[nVerGroup]);

		if (nBlock < m_ncDataBlock - 2 && m_byBlockMode[nBlock + 2] == QR_MODE_8BIT)
			ncDstBits -= (4 + nIndicatorLen8Bit[nVerGroup]);
		
		if (ncSrcBits > ncDstBits)
		{//如果合并后数据增加，前一块数据是8bit数据
			if (nBlock >= 1 && m_byBlockMode[nBlock - 1] == QR_MODE_8BIT)
			{
				m_nBlockLength[nBlock - 1] += m_nBlockLength[nBlock];

				for (i = nBlock; i < m_ncDataBlock - 1; ++i)
				{
					m_byBlockMode[i]  = m_byBlockMode[i + 1];
					m_nBlockLength[i] = m_nBlockLength[i + 1];
				}

				--m_ncDataBlock;
				--nBlock;
			}

			if (nBlock < m_ncDataBlock - 2 && m_byBlockMode[nBlock + 2] == QR_MODE_8BIT)
			{//数据块+2也是8bit
				m_nBlockLength[nBlock + 1] += m_nBlockLength[nBlock + 2];

				for (i = nBlock + 2; i < m_ncDataBlock - 1; ++i)
				{
					m_byBlockMode[i]  = m_byBlockMode[i + 1];
					m_nBlockLength[i] = m_nBlockLength[i + 1];
				}

				--m_ncDataBlock;
			}
			//设置新的数据块为8BIT模式
			m_byBlockMode[nBlock] = QR_MODE_8BIT;
			m_nBlockLength[nBlock] += m_nBlockLength[nBlock + 1];

			for (i = nBlock + 1; i < m_ncDataBlock - 1; ++i)
			{
				m_byBlockMode[i]  = m_byBlockMode[i + 1];
				m_nBlockLength[i] = m_nBlockLength[i + 1];
			}

			--m_ncDataBlock;

			if (nBlock >= 1)
				--nBlock;

			continue;
		}

		++nBlock; 
	}

	m_ncDataCodeWordBit = 0; 

	memset(m_byDataCodeWord, 0, MAX_DATACODEWORD);

	for (i = 0; i < m_ncDataBlock && m_ncDataCodeWordBit != -1; ++i)
	{
		if (m_byBlockMode[i] == QR_MODE_NUMERAL)
		{//如果数据块是数字模式
			m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, 1, 4); 

			m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, (unsigned short)m_nBlockLength[i], nIndicatorLenNumeral[nVerGroup]);

			for (j = 0; j < m_nBlockLength[i]; j += 3)
			{
				if (j < m_nBlockLength[i] - 2)
				{
					wBinCode = (unsigned short)(((lpsSource[ncComplete + j]	  - '0') * 100) +
									  ((lpsSource[ncComplete + j + 1] - '0') * 10) +
									   (lpsSource[ncComplete + j + 2] - '0'));

					m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, wBinCode, 10);
				}
				else if (j == m_nBlockLength[i] - 2)
				{
					wBinCode = (unsigned short)(((lpsSource[ncComplete + j] - '0') * 10) +
									   (lpsSource[ncComplete + j + 1] - '0'));

					m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, wBinCode, 7);
				}
				else if (j == m_nBlockLength[i] - 1)
				{
					wBinCode = (unsigned short)(lpsSource[ncComplete + j] - '0');

					m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, wBinCode, 4);
				}
			}

			ncComplete += m_nBlockLength[i];
		}

		else if (m_byBlockMode[i] == QR_MODE_ALPHABET)
		{
			m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, 2, 4);

			m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, (unsigned short)m_nBlockLength[i], nIndicatorLenAlphabet[nVerGroup]);

			for (j = 0; j < m_nBlockLength[i]; j += 2)
			{
				if (j < m_nBlockLength[i] - 1)
				{
					wBinCode = (unsigned short)((AlphabetToBinaly(lpsSource[ncComplete + j]) * 45) +
									   AlphabetToBinaly(lpsSource[ncComplete + j + 1]));

					m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, wBinCode, 11);
				}
				else
				{
					wBinCode = (unsigned short)AlphabetToBinaly(lpsSource[ncComplete + j]);

					m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, wBinCode, 6);
				}
			}

			ncComplete += m_nBlockLength[i];
		}

		else if (m_byBlockMode[i] == QR_MODE_8BIT)
		{
			m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, 4, 4);

			m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, (unsigned short)m_nBlockLength[i], nIndicatorLen8Bit[nVerGroup]);

			for (j = 0; j < m_nBlockLength[i]; ++j)
			{
				m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, (unsigned short)lpsSource[ncComplete + j], 8);
			}

			ncComplete += m_nBlockLength[i];
		}
		else// if(m_byBlockMode[i] == QR_MODE_KANJI)
		{//日本模式
			m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, 8, 4);

			m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, (unsigned short)(m_nBlockLength[i] / 2), nIndicatorLenKanji[nVerGroup]);

			for (j = 0; j < m_nBlockLength[i] / 2; ++j)
			{
				unsigned short wBinCode = KanjiToBinaly((unsigned short)(((unsigned char)lpsSource[ncComplete + (j * 2)] << 8) + (unsigned char)lpsSource[ncComplete + (j * 2) + 1]));

				m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, wBinCode, 13);
			}

			ncComplete += m_nBlockLength[i];
		}
//		else
//		{//中文模式
//			m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, 8, 4);

//			m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, (WORD)(m_nBlockLength[i] / 2), nIndicatorLenKanji[nVerGroup]);

//			for (j = 0; j < m_nBlockLength[i] / 2; ++j)
//			{
//				WORD wBinCode = ChineseToBinaly((WORD)(((BYTE)lpsSource[ncComplete + (j * 2)] << 8) + (BYTE)lpsSource[ncComplete + (j * 2) + 1]));

//				m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, wBinCode, 13);
//			}

//			ncComplete += m_nBlockLength[i];
//		}
	}

	return (m_ncDataCodeWordBit != -1);
	//return TRUE;
}
//获取bit长度
int GetBitLength(unsigned char nMode, int ncData, int nVerGroup)
{
	int ncBits = 0;

	switch (nMode)
	{
	case QR_MODE_NUMERAL:
		ncBits = 4 + nIndicatorLenNumeral[nVerGroup] + (10 * (ncData / 3));
		switch (ncData % 3)
		{
			case 1:
				ncBits += 4;
				break;
			case 2:
				ncBits += 7;
				break;
			default:
				break;
		}

		break;

	case QR_MODE_ALPHABET:
		ncBits = 4 + nIndicatorLenAlphabet[nVerGroup] + (11 * (ncData / 2)) + (6 * (ncData % 2));
		break;

	case QR_MODE_8BIT:
		ncBits = 4 + nIndicatorLen8Bit[nVerGroup] + (8 * ncData);
		break;

	default:
		//ncBits = 4 + nIndicatorLenKanji[nVerGroup] + (13 * (ncData / 2));//本程序写法？
		ncBits = 4 + nIndicatorLenKanji[nVerGroup] + (13 * ncData);//文档算法
		break;
	}

	return ncBits;
}
//设置bit流
int SetBitStream(int nIndex, unsigned short wData, int ncData)
{
	int i;

	if (nIndex == -1 || nIndex + ncData > MAX_DATACODEWORD * 8)
		return -1;

	for (i = 0; i < ncData; ++i)
	{
		if (wData & (1 << (ncData - i - 1)))
		{
			m_byDataCodeWord[(nIndex + i) / 8] |= 1 << (7 - ((nIndex + i) % 8));
		}
	}

	return nIndex + ncData;
}
//是不是数字编码信息
//是返回TRUE，否返回FALSE
bool IsNumeralData(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return TRUE;

	return FALSE;
}
//是否是字符编码信息
//是返回TRUE，否返回FALSE
bool IsAlphabetData(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return TRUE;

	if (c >= 'A' && c <= 'Z')
		return TRUE;

	if (c == ' ' || c == '$' || c == '%' || c == '*' || c == '+' || c == '-' || c == '.' || c == '/' || c == ':')
		return TRUE;

	return FALSE;
}
//是否中文汉字信息
//是返回TRUE，否返回FALSE
bool IsKanjiData(unsigned char c1, unsigned char c2)
{

	if (((c1 >= 0x81 && c1 <= 0x9f) || (c1 >= 0xe0 && c1 <= 0xeb)) && (c2 >= 0x40))
	{
		if ((c1 == 0x9f && c2 > 0xfc) || (c1 == 0xeb && c2 > 0xbf))
			return FALSE;

		return TRUE;
	}
	return FALSE;
}
bool IsChineseData(unsigned char c1, unsigned char c2)
{
	//中文模式
	if((c1 >= 0xa1 && c1 < 0xaa) || (c1 >= 0xb0 && c1 <= 0xfa))
	{
		if(c2 >= 0xa1 && c2 <= 0xfe)	return TRUE;
	}
	return FALSE;
}


//字母表信息转化
unsigned char AlphabetToBinaly(unsigned char c)
{
	if (c >= '0' && c <= '9') return (unsigned char)(c - '0');

	if (c >= 'A' && c <= 'Z') return (unsigned char)(c - 'A' + 10);

	if (c == ' ') return 36;

	if (c == '$') return 37;

	if (c == '%') return 38;

	if (c == '*') return 39;

	if (c == '+') return 40;

	if (c == '-') return 41;

	if (c == '.') return 42;

	if (c == '/') return 43;

	return 44;
}
//日文中文信息转化
//日文和汉字的编码会减去一个值。
//如：在0X8140 to 0X9FFC中的字符会减去8140，
//在0XE040到0XEBBF中的字符要减去0XC140，
//然后把结果前两个16进制位拿出来乘以0XC0，
//然后再加上后两个16进制位，最后转成13bit的编码。
unsigned short KanjiToBinaly(unsigned short wc)
{
	if (wc >= 0x8140 && wc <= 0x9ffc)
		wc -= 0x8140;
	else
		wc -= 0xc140;

	return (unsigned short)(((wc >> 8) * 0xc0) + (wc & 0x00ff));
}
/*
二、中文汉字的与日文汉字转换步骤相似：
1、对于第一字节为0xA1~0xAA之间,第二字节在0xA1~0xFE之间字符：
a)第一字节减去0xA1；
b)上一步结果乘以0x60;
c）第二字节减去0xA1;
d)将b)步骤的结果加上c步骤的结果;
e)将结果转换为13位二进制串。
1、对于第一字节为0xB0~0xFA之间,第二字节在0xA1~0xFE之间字符：
a)第一字节减去0xA6；
b)上一步结果乘以0x60;
c）第二字节减去0xA1;
d)将b)步骤的结果加上c步骤的结果;
e)将结果转换为13位二进制串
*/
unsigned short ChineseToBinaly(unsigned short wc)
{
	if(wc >= 0xa1a1 && wc <= 0xa9fe)
	{
		return (unsigned short)((((wc >> 8)-0xa1) * 0x60) + ((wc & 0x00ff)-0xa1));
	}
	if(wc >= 0xb0a1 && wc <= 0xf9fe)
	{
		return (unsigned short)((((wc >> 8)-0xa6) * 0x60) + ((wc & 0x00ff)-0xa1));
	}
	return (unsigned short)((((wc >> 8)-0xa6) * 0x60) + ((wc & 0x00ff)-0xa1));
}
//纠错码
void GetRSCodeWord(unsigned char *lpbyRSWork, int ncDataCodeWord, int ncRSCodeWord)
{
	int i, j;

	for (i = 0; i < ncDataCodeWord ; ++i)
	{
		if (lpbyRSWork[0] != 0)
		{
			unsigned char nExpFirst = byIntToExp[lpbyRSWork[0]]; 

			for (j = 0; j < ncRSCodeWord; ++j)
			{
				unsigned char nExpElement = (unsigned char)(((int)(byRSExp[ncRSCodeWord][j] + nExpFirst)) % 255);

				lpbyRSWork[j] = (unsigned char)(lpbyRSWork[j + 1] ^ byExpToInt[nExpElement]);
			}

			for (j = ncRSCodeWord; j < ncDataCodeWord + ncRSCodeWord - 1; ++j)
				lpbyRSWork[j] = lpbyRSWork[j + 1];
		}
		else
		{
			for (j = 0; j < ncDataCodeWord + ncRSCodeWord - 1; ++j)
				lpbyRSWork[j] = lpbyRSWork[j + 1];
		}
	}
}
//格式化数据存放，存在于所有的尺寸中，
//详见http://coolshell.cn/articles/10590.html#jtss-tsina
void FormatModule(void)
{
	int i, j;
	int nMinPenalty, nPenalty;

	memset(m_byModuleData, 0, sizeof(m_byModuleData));

	SetFunctionModule();	//首先把回字图案花在三个角上

	SetCodeWordPattern();	//

	if (m_nMaskingNo == -1)
	{
		m_nMaskingNo = 0;

		SetCodeWordPattern();	//复位数据位，清除上一掩码的异或结果
		SetMaskingPattern(m_nMaskingNo);
		SetFormatInfoPattern(m_nMaskingNo);

		nMinPenalty = CountPenalty();

		for (i = 1; i <= 7; ++i)
		{
			SetCodeWordPattern();	//每个候选掩码前复位数据位，避免掩码叠加
			SetMaskingPattern(i);
			SetFormatInfoPattern(i);

			nPenalty = CountPenalty();

			if (nPenalty < nMinPenalty)
			{
				nMinPenalty = nPenalty;
				m_nMaskingNo = i;
			}
		}
	}

	SetCodeWordPattern();	//最终确认前复位数据位，只保留选中的掩码
	SetMaskingPattern(m_nMaskingNo);
	SetFormatInfoPattern(m_nMaskingNo);

	for (i = 0; i < m_nSymbleSize; ++i)
	{
		for (j = 0; j < m_nSymbleSize; ++j)
		{
			m_byModuleData[i][j] = (unsigned char)((m_byModuleData[i][j] & 0x11) != 0);
		}
	}
}
//设置功能性模块信息
//Position Detection Pattern	用于标记二维码举行大小
//Separators for Postion Detection Patterns	三个就可以标记一个矩形
//Timing Patterns也是用于定位的。原因是二维码有40种尺寸，尺寸过大了后需要有根标准线，不然扫描的时候可能会扫歪了
//Alignment Patterns 只有Version 2以上（包括Version2）的二维码需要这个东东，同样是为了定位用的。
void SetFunctionModule(void)
{
	int i, j;

	SetFinderPattern(0, 0);					//三个回字坐标
	SetFinderPattern(m_nSymbleSize - 7, 0);	
	SetFinderPattern(0, m_nSymbleSize - 7);	

	for (i = 0; i < 8; ++i)
	{
		m_byModuleData[i][7] = m_byModuleData[7][i] = '\x20';
		m_byModuleData[m_nSymbleSize - 8][i] = m_byModuleData[m_nSymbleSize - 8 + i][7] = '\x20';
		m_byModuleData[i][m_nSymbleSize - 8] = m_byModuleData[7][m_nSymbleSize - 8 + i] = '\x20';
	}

	for (i = 0; i < 9; ++i)
	{
		m_byModuleData[i][8] = m_byModuleData[8][i] = '\x20';
	}

	for (i = 0; i < 8; ++i)
	{
		m_byModuleData[m_nSymbleSize - 8 + i][8] = m_byModuleData[8][m_nSymbleSize - 8 + i] = '\x20';
	}

#if (QR_VERSION_SET_DEFAULT > 6)	//移植适配：版本信息区 v7 以上才有，固定5版时用不到，省约420字节
	SetVersionPattern();	//版本信息
#endif

	for (i = 0; i < QR_VersonInfo[m_nVersion].ncAlignPoint; ++i)
	{
		SetAlignmentPattern(QR_VersonInfo[m_nVersion].nAlignPoint[i], 6);//小回字
		SetAlignmentPattern(6, QR_VersonInfo[m_nVersion].nAlignPoint[i]);

		for (j = 0; j < QR_VersonInfo[m_nVersion].ncAlignPoint; ++j)
		{
			SetAlignmentPattern(QR_VersonInfo[m_nVersion].nAlignPoint[i], QR_VersonInfo[m_nVersion].nAlignPoint[j]);
		}
	}

	for (i = 8; i <= m_nSymbleSize - 9; ++i)
	{
		m_byModuleData[i][6] = (i % 2) == 0 ? '\x30' : '\x20';
		m_byModuleData[6][i] = (i % 2) == 0 ? '\x30' : '\x20';
	}
}
//设置大回字信息
// O X X X X X X X
// O X O O O O O X
// O X O X X X O X
// O X O X X X O X
// O X O X X X O X
// O X O O O O O X
// O X X X X X X X
void SetFinderPattern(int x, int y)
{
	static unsigned char byPattern[] = {0x7f,  
							   0x41,  
							   0x5d,  
							   0x5d, 
							   0x5d, 
							   0x41,  
							   0x7f}; 
	int i, j;

	for (i = 0; i < 7; ++i)
	{
		for (j = 0; j < 7; ++j)
		{
			m_byModuleData[x + j][y + i] = (byPattern[i] & (1 << (6 - j))) ? '\x30' : '\x20'; 
		}
	}
}
//设置小回字信息
//Alignment Patterns 只有Version 2以上（包括Version2）的二维码需要这个东东，同样是为了定位用的。
//Alignment Patterns是除了3个大的回字，较小的回字
// O O O X X X X X
// O O O X O O O X
// O O O X O X O X
// O O O X O O O X
// O O O X X X X X
void SetAlignmentPattern(int x, int y)
{
	static unsigned char byPattern[] = {0x1f,  
							   0x11,  
							   0x15, 
							   0x11,  
							   0x1f}; 
	int i, j;

	if (m_byModuleData[x][y] & 0x20)
		return; 

	x -= 2; y -= 2; 

	for (i = 0; i < 5; ++i)
	{
		for (j = 0; j < 5; ++j)
		{
			m_byModuleData[x + j][y + i] = (byPattern[i] & (1 << (4 - j))) ? '\x30' : '\x20'; 
		}
	}
}
//设置版本，在 >= Version 7以上，需要预留两块3 x 6的区域存放一些版本信息。
void SetVersionPattern(void)
{
	int i, j;
	int nVerData;

	if (m_nVersion <= 6)//在 >= Version 7以上，需要预留两块3 x 6的区域存放一些版本信息。
		return;

	nVerData = m_nVersion << 12;

	for (i = 0; i < 6; ++i)
	{
		if (nVerData & (1 << (17 - i)))
		{
			nVerData ^= (0x1f25 << (5 - i));
		}
	}

	nVerData += m_nVersion << 12;

	for (i = 0; i < 6; ++i)
	{
		for (j = 0; j < 3; ++j)
		{
			m_byModuleData[m_nSymbleSize - 11 + j][i] = m_byModuleData[i][m_nSymbleSize - 11 + j] =
			(nVerData & (1 << (i * 3 + j))) ? '\x30' : '\x20';
		}
	}
}
//如果你以为我们可以开始画图，你就错了。
//二维码的混乱技术还没有玩完，它还要把数据码和纠错码的各个codewords交替放在一起。
//如何交替呢，规则如下：
//对于数据码：把每个块的第一个codewords先拿出来按顺度排列好，
//然后再取第一块的第二个，如此类推。如：上述示例中的Data Codewords如下
void SetCodeWordPattern(void)
{
	int x = m_nSymbleSize;
	int y = m_nSymbleSize - 1;

	int nCoef_x = 1; 
	int nCoef_y = 1; 

	int i, j;

	for (i = 0; i < m_ncAllCodeWord; ++i)
	{
		for (j = 0; j < 8; ++j)
		{
			do
			{
				x += nCoef_x;
				nCoef_x *= -1;

				if (nCoef_x < 0)
				{
					y += nCoef_y;

					if (y < 0 || y == m_nSymbleSize)
					{
						y = (y < 0) ? 0 : m_nSymbleSize - 1;
						nCoef_y *= -1;

						x -= 2;

						if (x == 6) 
							--x;
					}
				}
			}
			while (m_byModuleData[x][y] & 0x20); 

			m_byModuleData[x][y] = (m_byAllCodeWord[i] & (1 << (7 - j))) ? '\x02' : '\x00';
		}
	}
}
//设置遮蔽信息
void SetMaskingPattern(int nPatternNo)
{
	int i, j;
	bool bMask;
	for (i = 0; i < m_nSymbleSize; ++i)
	{
		for (j = 0; j < m_nSymbleSize; ++j)
		{
			if (! (m_byModuleData[j][i] & 0x20))
			{
				switch (nPatternNo)
				{
				case 0:
					bMask = ((i + j) % 2 == 0)?TRUE:FALSE;
					break;

				case 1:
					bMask = (i % 2 == 0)?TRUE:FALSE;
					break;

				case 2:
					bMask = (j % 3 == 0)?TRUE:FALSE;
					break;

				case 3:
					bMask = ((i + j) % 3 == 0)?TRUE:FALSE;
					break;

				case 4:
					bMask = (((i / 2) + (j / 3)) % 2 == 0)?TRUE:FALSE;
					break;

				case 5:
					bMask = (((i * j) % 2) + ((i * j) % 3) == 0)?TRUE:FALSE;
					break;

				case 6:
					bMask = ((((i * j) % 2) + ((i * j) % 3)) % 2 == 0)?TRUE:FALSE;
					break;

				default: 
					bMask = ((((i * j) % 3) + ((i + j) % 2)) % 2 == 0)?TRUE:FALSE;
					break;
				}

				m_byModuleData[j][i] = (unsigned char)((m_byModuleData[j][i] & 0xfe) | (((m_byModuleData[j][i] & 0x02) > 1) ^ bMask));
			}
		}
	}
}
//设置格式化信息 
void SetFormatInfoPattern(int nPatternNo)
{
	int nFormatInfo;
	int i;
	int nFormatData;

	switch (m_nLevel)
	{
	case QR_LEVEL_M:
		nFormatInfo = 0x00; 
		break;

	case QR_LEVEL_L:
		nFormatInfo = 0x08; 
		break;

	case QR_LEVEL_Q:
		nFormatInfo = 0x18; 
		break;

	default: 
		nFormatInfo = 0x10; 
		break;
	}

	nFormatInfo += nPatternNo;

	nFormatData = nFormatInfo << 10;

	for (i = 0; i < 5; ++i)
	{
		if (nFormatData & (1 << (14 - i)))
		{
			nFormatData ^= (0x0537 << (4 - i)); 
		}
	}

	nFormatData += nFormatInfo << 10;

	nFormatData ^= 0x5412;

	for (i = 0; i <= 5; ++i)
		m_byModuleData[8][i] = (nFormatData & (1 << i)) ? '\x30' : '\x20';

	m_byModuleData[8][7] = (nFormatData & (1 << 6)) ? '\x30' : '\x20';
	m_byModuleData[8][8] = (nFormatData & (1 << 7)) ? '\x30' : '\x20';
	m_byModuleData[7][8] = (nFormatData & (1 << 8)) ? '\x30' : '\x20';

	for (i = 9; i <= 14; ++i)
		m_byModuleData[14 - i][8] = (nFormatData & (1 << i)) ? '\x30' : '\x20';

	for (i = 0; i <= 7; ++i)
		m_byModuleData[m_nSymbleSize - 1 - i][8] = (nFormatData & (1 << i)) ? '\x30' : '\x20';

	m_byModuleData[8][m_nSymbleSize - 8] = '\x30';

	for (i = 8; i <= 14; ++i)
		m_byModuleData[8][m_nSymbleSize - 15 + i] = (nFormatData & (1 << i)) ? '\x30' : '\x20';
}

int CountPenalty(void)
{
	int nPenalty = 0;
	int i, j, k;
	int nCount = 0, s_nCount;

	for (i = 0; i < m_nSymbleSize; ++i)
	{
		for (j = 0; j < m_nSymbleSize - 4; ++j)
		{
			int nCount = 1;

			for (k = j + 1; k < m_nSymbleSize; k++)
			{
				if (((m_byModuleData[i][j] & 0x11) == 0) == ((m_byModuleData[i][k] & 0x11) == 0))
					++nCount;
				else
					break;
			}

			if (nCount >= 5)
			{
				nPenalty += 3 + (nCount - 5);
			}

			j = k - 1;
		}
	}

	for (i = 0; i < m_nSymbleSize; ++i)
	{
		for (j = 0; j < m_nSymbleSize - 4; ++j)
		{
			int nCount = 1;

			for (k = j + 1; k < m_nSymbleSize; k++)
			{
				if (((m_byModuleData[j][i] & 0x11) == 0) == ((m_byModuleData[k][i] & 0x11) == 0))
					++nCount;
				else
					break;
			}

			if (nCount >= 5)
			{
				nPenalty += 3 + (nCount - 5);
			}

			j = k - 1;
		}
	}

	for (i = 0; i < m_nSymbleSize - 1; ++i)
	{
		for (j = 0; j < m_nSymbleSize - 1; ++j)
		{
			if ((((m_byModuleData[i][j] & 0x11) == 0) == ((m_byModuleData[i + 1][j]		& 0x11) == 0)) &&
				(((m_byModuleData[i][j] & 0x11) == 0) == ((m_byModuleData[i]	[j + 1] & 0x11) == 0)) &&
				(((m_byModuleData[i][j] & 0x11) == 0) == ((m_byModuleData[i + 1][j + 1] & 0x11) == 0)))
			{
				nPenalty += 3;
			}
		}
	}

	for (i = 0; i < m_nSymbleSize; ++i)
	{
		for (j = 0; j < m_nSymbleSize - 6; ++j)
		{
			if (((j == 0) ||				 (! (m_byModuleData[i][j - 1] & 0x11))) && // 柧 傑偨偼 僔儞儃儖奜
											 (   m_byModuleData[i][j]     & 0x11)   && // 埫 - 1
											 (! (m_byModuleData[i][j + 1] & 0x11))  && // 柧 - 1
											 (   m_byModuleData[i][j + 2] & 0x11)   && // 埫 劉
											 (   m_byModuleData[i][j + 3] & 0x11)   && // 埫 劆3
											 (   m_byModuleData[i][j + 4] & 0x11)   && // 埫 劊
											 (! (m_byModuleData[i][j + 5] & 0x11))  && // 柧 - 1
											 (   m_byModuleData[i][j + 6] & 0x11)   && // 埫 - 1
				((j == m_nSymbleSize - 7) || (! (m_byModuleData[i][j + 7] & 0x11))))   // 柧 傑偨偼 僔儞儃儖奜
			{
				if (((j < 2 || ! (m_byModuleData[i][j - 2] & 0x11)) && 
					 (j < 3 || ! (m_byModuleData[i][j - 3] & 0x11)) &&
					 (j < 4 || ! (m_byModuleData[i][j - 4] & 0x11))) ||
					((j >= m_nSymbleSize - 8  || ! (m_byModuleData[i][j + 8]  & 0x11)) &&
					 (j >= m_nSymbleSize - 9  || ! (m_byModuleData[i][j + 9]  & 0x11)) &&
					 (j >= m_nSymbleSize - 10 || ! (m_byModuleData[i][j + 10] & 0x11))))
				{
					nPenalty += 40;
				}
			}
		}
	}

	for (i = 0; i < m_nSymbleSize; ++i)
	{
		for (j = 0; j < m_nSymbleSize - 6; ++j)
		{
			if (((j == 0) ||				 (! (m_byModuleData[j - 1][i] & 0x11))) && // 柧 傑偨偼 僔儞儃儖奜
											 (   m_byModuleData[j]    [i] & 0x11)   && // 埫 - 1
											 (! (m_byModuleData[j + 1][i] & 0x11))  && // 柧 - 1
											 (   m_byModuleData[j + 2][i] & 0x11)   && // 埫 劉
											 (   m_byModuleData[j + 3][i] & 0x11)   && // 埫 劆3
											 (   m_byModuleData[j + 4][i] & 0x11)   && // 埫 劊
											 (! (m_byModuleData[j + 5][i] & 0x11))  && // 柧 - 1
											 (   m_byModuleData[j + 6][i] & 0x11)   && // 埫 - 1
				((j == m_nSymbleSize - 7) || (! (m_byModuleData[j + 7][i] & 0x11))))   // 柧 傑偨偼 僔儞儃儖奜
			{
				if (((j < 2 || ! (m_byModuleData[j - 2][i] & 0x11)) && 
					 (j < 3 || ! (m_byModuleData[j - 3][i] & 0x11)) &&
					 (j < 4 || ! (m_byModuleData[j - 4][i] & 0x11))) ||
					((j >= m_nSymbleSize - 8  || ! (m_byModuleData[j + 8][i]  & 0x11)) &&
					 (j >= m_nSymbleSize - 9  || ! (m_byModuleData[j + 9][i]  & 0x11)) &&
					 (j >= m_nSymbleSize - 10 || ! (m_byModuleData[j + 10][i] & 0x11))))
				{
					nPenalty += 40;
				}
			}
		}
	}

	for (i = 0; i < m_nSymbleSize; ++i)
	{
		for (j = 0; j < m_nSymbleSize; ++j)
		{
			if (! (m_byModuleData[i][j] & 0x11))
			{
				++nCount;
			}
		}
	}
	
	if( (50 - ((nCount * 100) / (m_nSymbleSize * m_nSymbleSize))) > 0)
		s_nCount = 50 - ((nCount * 100) / (m_nSymbleSize * m_nSymbleSize));
	else
		s_nCount = 0 - (50 - ((nCount * 100) / (m_nSymbleSize * m_nSymbleSize)));
	nPenalty += (s_nCount / 5) * 10;

	return nPenalty;
}

//按当前纠错等级(m_nLevel，由QR_ECC_LEVEL宏决定)动态计算各版本(1~14)的字节容量
/*
最大字节数 = floor( ( ncDataCodeWord[m_nLevel] × 8 − 4 − 长度指示位数 ) / 8 )
            （4 = 模式指示；v1~9 长度指示 8 位，v10+ 是 16 位）
各版本在各纠错等级下的最大字节数（8-bit模式，库支持版本1~14）：
版本      1    2    3    4    5    6    7    8    9   10   11   12   13   14
L(7%)    17   32   53   78  106  134  154  192  230  271  321  367  425  458
M(15%)   14   26   42   62   84  106  122  152  180  213  251  287  331  362
Q(25%)   11   20   32   46   60   74   86  108  130  151  177  203  241  258
H(30%)    7   14   24   34   44   58   64   84   98  119  137  155  177  194
*/
unsigned int getSuitableVersion(unsigned int dataLen)
{
	unsigned int i, lenbits, cap;
	for (i = 1; i <= 14; ++i)
	{
		lenbits = (i <= 9) ? 8 : 16;
		cap = (QR_VersonInfo[i].ncDataCodeWord[m_nLevel] * 8 - 4 - lenbits) / 8;
		if (dataLen <= cap)
			return i;
	}
	return 0;	//超出当前纠错等级下版本14的最大容量
}
//void T0_ISR_PC(void) interrupt 1 // 1ms定时器
//{
//	EA = 0;
//	TH0 = T0MS >> 8;
//	TL0 = T0MS;
//	countTime++;
//	EA = 1;
//}

