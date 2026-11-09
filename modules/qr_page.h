/**
 * @file qr_page.h
 * @brief 页面二维码刷新（对应源工程 MODULES/ui.c 里的二维码部分）
 * @details 二维码显示参数由 NOR flash 配置区（QR_CFG_BASE_ADDR）按页面号决定，
 *          本模块只负责“页面切换→清旧页/加载新页配置→内容变化时重绘”，
 *          编码与 DGUS 批量图标绘制在 QR_Encode.c。
 *
 *          源工程对应关系：
 *            QR_RefreshPage  <- ui.c: QR_RefreshPage()
 *            QR_PageFirstRun <- ui.c: PageFirstRun()   （10ms 任务，检测页面切换）
 *            QR_PageRefresh  <- ui.c: PagePointer()    （500ms 任务，周期刷新）
 *            ReadPageId()    <- sys.h: read_pic()
 */
#ifndef __QR_PAGE_H__
#define __QR_PAGE_H__

#include "sys.h"

/**
 * @brief 刷新当前页全部二维码
 * @details 页面变化时重新查配置缓存，内容变化（或强制刷新）时重绘；
 *          离开二维码页时清空该页图标
 * @param[in] page 当前显示页面号
 */
void QR_RefreshPage(uint16_t page);

/**
 * @brief 页面切换检测任务函数（源工程 PageFirstRun）
 * @details 建议 10ms 周期；页面号变化才调用 QR_RefreshPage
 */
void QR_PageFirstRun(void);

/**
 * @brief 页面周期刷新任务函数（源工程 PagePointer）
 * @details 建议 500ms 周期；用于检测二维码内容（数据 VP）变化后重绘
 */
void QR_PageRefresh(void);

#endif /* __QR_PAGE_H__ */
