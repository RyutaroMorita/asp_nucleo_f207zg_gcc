/*
 *  TINET (TCP/IP Protocol Stack)
 *
 *  Copyright (C) 2001-2017 by Dep. of Computer Science and Engineering
 *                   Tomakomai National College of Technology, JAPAN
 *
 *  上記著作権者は，以下の (1)～(4) の条件か，Free Software Foundation
 *  によって公表されている GNU General Public License の Version 2 に記
 *  述されている条件を満たす場合に限り，本ソフトウェア（本ソフトウェア
 *  を改変したものを含む．以下同じ）を使用・複製・改変・再配布（以下，
 *  利用と呼ぶ）することを無償で許諾する．
 *  (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
 *      権表示，この利用条件および下記の無保証規定が，そのままの形でソー
 *      スコード中に含まれていること．
 *  (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
 *      用できる形で再配布する場合には，再配布に伴うドキュメント（利用
 *      者マニュアルなど）に，上記の著作権表示，この利用条件および下記
 *      の無保証規定を掲載すること．
 *  (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
 *      用できない形で再配布する場合には，次の条件を満たすこと．
 *    (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
 *        作権表示，この利用条件および下記の無保証規定を掲載すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，その適用可能性も
 *  含めて，いかなる保証も行わない．また，本ソフトウェアの利用により直
 *  接的または間接的に生じたいかなる損害に関しても，その責任を負わない．
 *
 *  @(#) $Id: if_ed.c 1.7 2017/6/1 8:49:51 abe $
 */

/*
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/isa/if_ed.c,v 1.148.2.4 1999/09/25 13:08:18 nyan Exp $
 */

/*
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 *   adapters. By David Greenman, 29-April-1993
 *
 * Currently supports the Western Digital/SMC 8003 and 8013 series,
 *   the SMC Elite Ultra (8216), the 3Com 3c503, the NE1000 and NE2000,
 *   and a variety of similar clones.
 *
 */

#ifdef TARGET_KERNEL_ASP

#define CAST(type, val)		((type)(val))

#include <kernel.h>
#include <sil.h>
#include <t_syslog.h>
#include "kernel_cfg.h"
#include "target_config.h"

#endif	/* of #ifdef TARGET_KERNEL_ASP */

#ifdef TARGET_KERNEL_JSP

#include <s_services.h>
#include <t_services.h>
#include "kernel_id.h"

#endif	/* of #ifdef TARGET_KERNEL_JSP */

#include <tinet_defs.h>
#include <tinet_config.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <net/net.h>
#include <net/net_timer.h>
#include <net/net_count.h>
#include <net/net_buf.h>

#include "if_stm32freg.h"
#include <string.h>

/*
 *  内部 RAM のページ指定
 */
ETH_DMADescTypeDef DMARxDscrTab[ETH_RXBUFNB] __attribute__((section(".RxDecripSection")));/* Ethernet Rx MA Descriptor */
ETH_DMADescTypeDef DMATxDscrTab[ETH_TXBUFNB] __attribute__((section(".TxDescripSection")));/* Ethernet Tx DMA Descriptor */
__IO uint32_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE/4] __attribute__((section(".RxBUF")));/* Ethernet Receive Buffer */
__IO uint32_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE/4] __attribute__((section(".TxBUF")));/* Ethernet Transmit Buffer */

/*
 *  ネットワークインタフェースに依存するソフトウェア情報
 */
ETH_HandleTypeDef	EthHandle;
typedef struct t_stm32f_softc {
	ETH_HandleTypeDef	*handle;
	bool_t link_pre;
	bool_t link_now;
	} T_STM32F_SOFTC;

/*
 *  ネットワークインタフェースのソフトウェア情報
 */

/* ネットワークインタフェースに依存するソフトウェア情報 */
static T_STM32F_SOFTC stm32f_softc;

/* ネットワークインタフェースに依存しないソフトウェア情報 */

T_IF_SOFTC if_softc = {
	{},							/* ネットワークインタフェースのアドレス	*/
	0,							/* 送信タイムアウト			*/
	&stm32f_softc,				/* ディバイス依存のソフトウェア情報	*/
	SEM_IF_STM32F_SBUF_READY,	/* 送信セマフォ			*/
	SEM_IF_STM32F_RBUF_READY,	/* 受信セマフォ			*/
#ifdef _IP6_CFG

	IF_MADDR_INIT,				/* マルチキャストアドレスリスト	*/

#endif	/* of #ifdef _IP6_CFG */
	};

/*
 *  局所変数
 */
static void stm32f_stop (T_STM32F_SOFTC *sc);
static void stm32f_init_sub (T_IF_SOFTC *ic);

/*
 *  stm32f_stop -- ed ネットワークインタフェースを停止する。
 *
 *    注意: NIC 割り込み禁止状態で呼び出すこと。
 */

static void
stm32f_stop (T_STM32F_SOFTC *sc)
{
	HAL_ETH_Stop(sc->handle);
	}

/*
 *  stm32f_init_sub -- ed ネットワークインタフェースの初期化
 *
 *    注意: NIC 割り込み禁止状態で呼び出すこと。
 */

static void
stm32f_init_sub (T_IF_SOFTC *ic)
{
	T_STM32F_SOFTC	*sc = ic->sc;
	int_t		ix;
	//ID			tid;

	sc->handle->Instance = ETH;
	sc->handle->Init.MACAddr = ic->ifaddr.lladdr;
	sc->handle->Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
	sc->handle->Init.Speed = ETH_SPEED_100M;
	sc->handle->Init.DuplexMode = ETH_MODE_FULLDUPLEX;
#if defined(LAN8742A_PHY_ADDRESS)
	sc->handle->Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
#elif defined(LAN8710_PHY_ADDRESS)
	sc->handle->Init.MediaInterface = ETH_MEDIA_INTERFACE_MII;
#else
#error "Invalid PHY"
#endif

	sc->handle->Init.RxMode = ETH_RXINTERRUPT_MODE;
	//sc->handle->Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
	sc->handle->Init.ChecksumMode = ETH_CHECKSUM_BY_SOFTWARE;	// 互換性維持のため
#if defined(LAN8742A_PHY_ADDRESS)
	sc->handle->Init.PhyAddress = LAN8742A_PHY_ADDRESS;
#elif defined(LAN8710_PHY_ADDRESS)
	sc->handle->Init.PhyAddress = LAN8710_PHY_ADDRESS;
#else
#error "Invalid PHY"
#endif

	/*
	 *  不要な割り込みを無効にする（重要！）
	 */
	__HAL_ETH_MAC_ENABLE_IT(sc->handle, ETH_MACIMR_TSTIM | ETH_MACIMR_PMTIM);
	__HAL_ETH_MMC_RX_IT_DISABLE(sc->handle, ETH_MMCRIMR_RGUFM | ETH_MMCRIMR_RFAEM | ETH_MMCRIMR_RFCEM);
	__HAL_ETH_MMC_TX_IT_DISABLE(sc->handle, ETH_MMCTIMR_TGFM | ETH_MMCTIMR_TGFMSCM | ETH_MMCTIMR_TGFSCM);

	sc->link_pre = false;
	sc->link_now = false;

	/*
	 *  ETH_TIMEOUT_LINKED_STATE待ち回避のため一時的に優先度を下げる
	 */
	//get_tid(&tid);
	//chg_pri(tid, TMAX_TPRI);

	/* configure ethernet peripheral (GPIOs, clocks, MAC, DMA) */
	if (HAL_ETH_Init(sc->handle) == HAL_OK) {;
		sc->link_pre = true;
		sc->link_now = true;
		/*
		 *  リンクイベント処理挿入場所
		 */
	}

	/*
	 *  優先度を元に戻す
	 */
	//chg_pri(tid, TPRI_INI);

	/*
	 * 送信割り込みの設定
	 */
	__HAL_ETH_DMA_ENABLE_IT(sc->handle, ETH_DMA_IT_T);

	/* Initialize Tx Descriptors list: Chain Mode */
	HAL_ETH_DMATxDescListInit(sc->handle, DMATxDscrTab, (uint8_t *)&Tx_Buff[0][0], ETH_TXBUFNB);

	/* Initialize Rx Descriptors list: Chain Mode  */
	HAL_ETH_DMARxDescListInit(sc->handle, DMARxDscrTab, (uint8_t *)&Rx_Buff[0][0], ETH_RXBUFNB);

	if (sc->link_now)
		/* Enable MAC and DMA transmission and reception */
		HAL_ETH_Start(sc->handle);

	/* 送信可能セマフォを初期化する。*/
	for (ix = NUM_IF_STM32F_TXBUF; ix --; )
		sig_sem(ic->semid_txb_ready);

#if defined(TARGET_KERNEL_ASP)

	/* ターゲット依存部の割込み初期化 */
	stm32f_inter_init();

#endif	/* of #if defined(TARGET_KERNEL_ASP) */

#if defined(TARGET_KERNEL_JSP) && TKERNEL_PRVER >= 0x1042u	/* JSP-1.4.2 以降 */

	/* ターゲット依存部の割込み初期化 */
	stm32f_inter_init();

#endif	/* of #if defined(TARGET_KERNEL_JSP) && TKERNEL_PRVER >= 0x1042u */
	}

/*
 * stm32f_reset -- ed ネットワークインタフェースをリセットする。
 */

void
stm32f_reset (T_IF_SOFTC *ic)
{
#ifdef TARGET_KERNEL_JSP
	IPM	ipm;
#endif

	/* NIC からの割り込みを禁止する。*/
#ifdef TARGET_KERNEL_JSP
	ipm = ed_dis_inter();
#endif
#ifdef TARGET_KERNEL_ASP
	syscall(dis_int(INTNO_IF_STM32F));
#endif

	stm32f_stop(ic->sc);
	stm32f_init_sub(ic);

	/* NIC からの割り込みを許可する。*/
#ifdef TARGET_KERNEL_JSP
	ed_ena_inter(ipm);
#endif
#ifdef TARGET_KERNEL_ASP
	syscall(ena_int(INTNO_IF_STM32F));
#endif
	}

/*
 *  stm32f_get_softc -- ネットワークインタフェースのソフトウェア情報を返す。
 */

T_IF_SOFTC *
stm32f_get_softc (void)
{
	return &if_softc;
	}

/*
 * stm32f_watchdog -- ed ネットワークインタフェースのワッチドッグタイムアウト
 */

void
stm32f_watchdog (T_IF_SOFTC *ic)
{
	stm32f_reset(ic);
	}

/*
 * stm32f_probe -- ed ネットワークインタフェースの検出
 */

void
stm32f_probe (T_IF_SOFTC *ic)
{
	T_STM32F_SOFTC	*sc = ic->sc;
	int i;
	uint8_t macaddress[6]= { 0x00, 0x12, 0x34, 0x56, 0x78, 0x90 };

	sc->handle = &EthHandle;

#if defined(TARGET_KERNEL_ASP)

	/* ターゲット依存部のバスの初期化 */
	stm32f_bus_init();

#endif	/* of #if defined(TARGET_KERNEL_ASP) */

#if defined(TARGET_KERNEL_JSP) && TKERNEL_PRVER >= 0x1042u	/* JSP-1.4.2 以降 */

	/* ターゲット依存部のバスの初期化 */
	stm32f_bus_init();

#endif	/* of #if defined(TARGET_KERNEL_JSP) && TKERNEL_PRVER >= 0x1042u */

	for(i = 0; i < ETHER_ADDR_LEN; i++){
		ic->ifaddr.lladdr[i] = macaddress[i];
	}
	}

/*
 * stm32f_init -- ed ネットワークインタフェースの初期化
 */

void
stm32f_init (T_IF_SOFTC *ic)
{
#ifdef TARGET_KERNEL_JSP
	IPM	ipm;
#endif

	/* NIC からの割り込みを禁止する。*/
#ifdef TARGET_KERNEL_JSP
	ipm = ed_dis_inter();
#endif
#ifdef TARGET_KERNEL_ASP
	syscall(dis_int(INTNO_IF_STM32F));
#endif

	/* stm32f_init 本体を呼び出す。*/
	stm32f_init_sub(ic);

	act_tsk(TSK_LNK);

	/* NIC からの割り込みを許可する。*/
#ifdef TARGET_KERNEL_JSP
	ed_ena_inter(ipm);
#endif
#ifdef TARGET_KERNEL_ASP
	syscall(ena_int(INTNO_IF_STM32F));
#endif
	}

/*
 * stm32f_read -- フレームの読み込み
 */

T_NET_BUF *
stm32f_read (T_IF_SOFTC *ic)
{
	T_STM32F_SOFTC	*sc = ic->sc;
	T_NET_BUF	*input = NULL;
	uint32_t byteslefttocopy = 0;
	uint8_t *buffer;
	__IO ETH_DMADescTypeDef *dmarxdesc;
	uint16_t align;
	ER error;
	uint8_t *dst;
	//uint32_t payloadoffset = 0;
	uint32_t i = 0;

	/* get received frame */
	if (HAL_ETH_GetReceivedFrame_IT(sc->handle) != HAL_OK)
		return NULL;

	/* Obtain the size of the packet and put it into the "len" variable. */
	byteslefttocopy = sc->handle->RxFrameInfos.length;
	//syslog(LOG_NOTICE, "receive length : %d", byteslefttocopy);
	buffer = (uint8_t *)sc->handle->RxFrameInfos.buffer;

	dmarxdesc = sc->handle->RxFrameInfos.FSRxDesc;

	/*
	 *  +-----------+--------+---------+---------+
	 *  | Ehter HDR | IP HDR | TCP HDR | TCP SDU |
	 *  +-----------+--------+---------+---------+
	 *        14        20        20        n
	 *   <----------------- len ---------------->
	 *              ^
	 *              t_net_buf で 4 オクテット境界にアラインされている。
	 *
	 *  tcp_input と udp_input では、擬似ヘッダと SDU でチェックサムを
	 *  計算するが、n が 4 オクテット境界になるように SDU の後ろに 0 を
	 *  パッディングする。その分を考慮して net_buf を獲得しなければならない。
	 */
	align = ((((byteslefttocopy - sizeof(T_IF_HDR)) + 3) >> 2) << 2) + sizeof(T_IF_HDR);
	if ((error = tget_net_buf(&input, align, TMO_IF_STM32F_GET_NET_BUF)) == E_OK && input != NULL) {
		dst = input->buf + IF_ETHER_NIC_HDR_ALIGN;
#if 0
		/* Check if the length of bytes to copy in current pbuf is bigger than Rx buffer size */
		while (byteslefttocopy > ETH_RX_BUF_SIZE)
		{
			memcpy((uint8_t*)(dst + payloadoffset), (uint8_t*)buffer, ETH_RX_BUF_SIZE);
			/* Point to next descriptor */
			dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
			buffer = (uint8_t *)(dmarxdesc->Buffer1Addr);
			byteslefttocopy -= ETH_RX_BUF_SIZE;
			payloadoffset += ETH_RX_BUF_SIZE;
		}
		/* Copy remaining data in pbuf */
		memcpy((uint8_t*)(dst + payloadoffset), (uint8_t*)buffer, byteslefttocopy);
#endif
		memcpy((uint8_t*)dst, (uint8_t*)buffer, byteslefttocopy);
	}

	/* Release descriptors to DMA */
	/* Point to first descriptor */
	dmarxdesc = sc->handle->RxFrameInfos.FSRxDesc;
	/* Set Own bit in Rx descriptors: gives the buffers back to DMA */
	for (i = 0; i < sc->handle->RxFrameInfos.SegCount; i++)
	{
		dmarxdesc->Status |= ETH_DMARXDESC_OWN;
		dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
	}

	/* Clear Segment_Count */
	sc->handle->RxFrameInfos.SegCount = 0;

	/* When Rx Buffer unavailable flag is set: clear it and resume reception */
	if ((sc->handle->Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET)
	{
		/* Clear RBUS ETHERNET DMA flag */
		sc->handle->Instance->DMASR = ETH_DMASR_RBUS;
		/* Resume DMA reception */
		sc->handle->Instance->DMARPDR = 0;
	}

	/* 受信バッファにデータが残っていれば、受信処理を継続する。*/
	if ((sc->handle->RxDesc->Status & ETH_DMARXDESC_OWN) == (uint32_t)RESET)
		sig_sem(ic->semid_rxb_ready);

	return input;
	}

/*
 * stm32f_start -- 送信フレームをバッファリングする。
 */

void
stm32f_start (T_IF_SOFTC *ic, T_NET_BUF *output)
{
	T_STM32F_SOFTC	*sc = ic->sc;
	__IO ETH_DMADescTypeDef *DmaTxDesc;
	uint32_t byteslefttocopy = 0;
	uint8_t *buffer = (uint8_t *)(sc->handle->TxDesc->Buffer1Addr);
	//uint32_t payloadoffset = 0;
	uint32_t framelength = 0;

	DmaTxDesc = sc->handle->TxDesc;

	/* copy frame from pbufs to driver buffers */
	/* Is this buffer available? If not, goto error */
	if ((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET)
	{
		goto error;
	}

	/* Get bytes in current lwIP buffer */
	byteslefttocopy = output->len;

	/* Check if the length of data to copy is bigger than Tx buffer size*/
#if 0
	while (byteslefttocopy > ETH_TX_BUF_SIZE)
	{
		/* Copy data to Tx buffer*/
		memcpy((uint8_t*)buffer, (uint8_t*)(output->buf + payloadoffset), ETH_TX_BUF_SIZE);
		/* Point to next descriptor */
		DmaTxDesc = (ETH_DMADescTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);
		/* Check if the buffer is available */
		if ((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET)
		{
			goto error;
		}
		buffer = (uint8_t *)(DmaTxDesc->Buffer1Addr);
		byteslefttocopy -= ETH_TX_BUF_SIZE;
		payloadoffset += ETH_TX_BUF_SIZE;
		framelength += ETH_TX_BUF_SIZE;
	}

	/* Copy the remaining bytes */
	memcpy((uint8_t*)buffer, (uint8_t*)(output->buf + payloadoffset), byteslefttocopy);
#endif
	memcpy((uint8_t*)buffer, (uint8_t*)output->buf, byteslefttocopy);
	framelength += byteslefttocopy;

	//syslog(LOG_NOTICE, "send length : %d", framelength);

	__HAL_ETH_DMATXDESC_ENABLE_IT(sc->handle);

	/* Prepare transmit descriptors to give to DMA */
	HAL_ETH_TransmitFrame(sc->handle, framelength);

error:

	/* When Transmit Underflow flag is set, clear it and issue a Transmit Poll Demand to resume transmission */
	if ((sc->handle->Instance->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET)
	{
		/* Clear TUS ETHERNET DMA flag */
		sc->handle->Instance->DMASR = ETH_DMASR_TUS;

		/* Resume DMA transmission*/
		sc->handle->Instance->DMATPDR = 0;
	}

	//sig_sem(ic->semid_txb_ready);
	}

/**
  * @brief  Tx Transfer completed callbacks.
  * @param  heth pointer to a ETH_HandleTypeDef structure that contains
  *         the configuration information for ETHERNET module
  * @retval None
  */
void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth)
{
	T_IF_SOFTC	*ic;

	ic = &if_softc;
	isig_sem(ic->semid_txb_ready);
	}

/**
  * @brief  Ethernet Rx Transfer completed callback
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
	T_IF_SOFTC	*ic;

	ic = &if_softc;
	isig_sem(ic->semid_rxb_ready);
	}

/*
 *  NIC 割り込みハンドラ
 */

void
if_stm32f_handler (void)
{
	T_STM32F_SOFTC	*sc;
	T_IF_SOFTC	*ic;

	ic = &if_softc;
	sc = ic->sc;

	HAL_ETH_IRQHandler(sc->handle);
	}

void stm32f_tick(intptr_t exinf)
{
	T_IF_SOFTC	*ic;
	T_STM32F_SOFTC	*sc;
	uint32_t phyreg = 0;
#ifdef TARGET_KERNEL_JSP
	IPM		ipm;
#endif

	ic = stm32f_get_softc();
	sc = ic->sc;

	while (1) {
		dly_tsk(100);
		HAL_ETH_ReadPHYRegister(sc->handle, PHY_BSR, &phyreg);
		if (((phyreg & PHY_AUTONEGO_COMPLETE) != PHY_AUTONEGO_COMPLETE))
			sc->link_now = false;
		else
			sc->link_now = true;
		if (sc->link_pre != sc->link_now) {		// リンク状態に変化あり
			sc->link_pre = sc->link_now;
			//syslog(LOG_NOTICE, "link state chenged : %d", sc->link_pre);
			if ( sc->link_pre )
				stm32f_reset(ic);
			/*
			 *  リンクイベント処理挿入場所
			 */
		}
	}
	}
