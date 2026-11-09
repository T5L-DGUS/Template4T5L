/**
 * @file qr_page.c
 * @brief 页面二维码刷新
 * @details 代码搬运自源工程 MODULES/ui.c 的二维码部分（第72~153行），仅改动：
 *          read_pic() -> ReadPageId()（本工程 sys.h 的页面号读取），
 *          PageFirstRun/PagePointer -> QR_PageFirstRun/QR_PageRefresh（避免与本工程
 *          t5l_stc.c 的 PageFunction 等页面函数重名），其余逻辑逐行保留。
 */
#include "qr_page.h"
#include "QR_Encode.h"

/* 当前页二维码：实例配置从NOR flash配置区按页面号动态扫描解析，最多2个 */
static QR_INSTANCE xdata qrInst[QR_CFG_MAX_RECORD];	//当前页二维码实例
static u8 xdata qrLastData[QR_CFG_MAX_RECORD][QR_DATA_MAX_LEN];	//各实例上一次显示的数据快照
static u8 xdata qrForce[QR_CFG_MAX_RECORD];			//强制刷新标志：上电首次/进入页面时置1
static u16 xdata qrCfgPage = 0xFFFF;				//已加载配置的页面号缓存
static u8 xdata qrCfgCnt = 0;						//当前页解析出的二维码数量（必须显式清零：XDATALEN=0，上电XDATA不初始化）

//刷新当前页全部二维码：页面变化时重新扫描配置，内容变化时重绘
void QR_RefreshPage(u16 page)
{
	u8 i, k;
	u8 diff;

	if (page != qrCfgPage)		//页面切换：离开旧页面才清空其图标，再扫描新页面配置
	{
		if (qrCfgCnt > 0)		//旧页面有二维码配置=离开的是二维码界面，才清空（进入任意界面不清空）
		{
			for (i = 0; i < qrCfgCnt; i++)	//旧页面的实例（配置尚未被新页覆盖）
			{
				QR_ClearQR(&qrInst[i]);
			}
		}
		qrCfgCnt = QR_CfgFindPage(page, qrInst, QR_CFG_MAX_RECORD);
		qrCfgPage = page;
		for (i = 0; i < qrCfgCnt; i++)
		{
			qrInst[i].version_set = QR_VERSION_SET_DEFAULT;	//版本号固定5
			qrForce[i] = 1;
		}
	}

	for (i = 0; i < qrCfgCnt; i++)
	{
		readDgusiiVp(qrInst[i].data_addr, (u8*)test, QR_DATA_MAX_LEN/2);	//42字=84字节，读取到test缓冲区

		diff = 0;
		if (!qrForce[i])
		{
			for (k = 0; k < QR_DATA_MAX_LEN; k++)
			{
				if (test[k] != qrLastData[i][k])
				{
					diff = 1;
					break;
				}
			}
		}

		if (qrForce[i] || diff)
		{
			for (k = 0; k < QR_DATA_MAX_LEN; k++)
			{
				qrLastData[i][k] = test[k];	//保存本次快照
			}
			qrForce[i] = 0;
			test[QR_DATA_MAX_LEN] = 0;		//补字符串结束符，防止EncodeData越界读取
			if (!QR_printf(&qrInst[i], test))	//编码失败（如空内容）：清区域防残留，快照已存防反复重试
			{
				QR_ClearQR(&qrInst[i]);
			}
		}
	}
}

//进入界面首次运行（源工程 PageFirstRun）
void QR_PageFirstRun(void)
{
	static u16 curPic = 0xFFFF;

	u16 nowPic = ReadPageId();
	if (curPic != nowPic)
	{
		curPic = nowPic;
		QR_RefreshPage(nowPic);
	}
}

//页面周期刷新（源工程 PagePointer）
void QR_PageRefresh(void)
{
	QR_RefreshPage(ReadPageId());
}
