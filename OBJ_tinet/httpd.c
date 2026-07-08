/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 * 
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2005-2008 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 * 
 *  上記著作権者は，以下の(1)～(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．
 *  (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
 *      権表示，この利用条件および下記の無保証規定が，そのままの形でソー
 *      スコード中に含まれていること．
 *  (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
 *      用できる形で再配布する場合には，再配布に伴うドキュメント（利用
 *      者マニュアルなど）に，上記の著作権表示，この利用条件および下記
 *      の無保証規定を掲載すること．
 *  (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
 *      用できない形で再配布する場合には，次のいずれかの条件を満たすこ
 *      と．
 *    (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
 *        作権表示，この利用条件および下記の無保証規定を掲載すること．
 *    (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
 *        報告すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *      また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
 *      由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを
 *      免責すること．
 * 
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的
 *  に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェ
 *  アの利用により直接的または間接的に生じたいかなる損害に関しても，そ
 *  の責任を負わない．
 * 
 */

#include <string.h>
#include <kernel.h>
#include <t_syslog.h>
#include <t_stdlib.h>
#include "syssvc/serial.h"
#include "syssvc/syslog.h"
#include "kernel_cfg.h"

#include "tinet_cfg.h"
#include <tinet_config.h>
#include <tinet_defs.h>

#include <netinet/in.h>
#include <netinet/in_itron.h>
#include <netinet/tcp.h>

#include "httpd.h"

T_TCP_CREP tcp_crep;	// TCP受付口 : Reception endpoint
T_TCP_CCEP tcp_ccep;	// TCP通信端点 : Communication endpoint

#define SIZE_CEP_BUF	2048
uint8_t cep_sbuf[SIZE_CEP_BUF];
uint8_t cep_rbuf[SIZE_CEP_BUF];

static const uint8_t HTTP_REQUEST_GET[] = "GET ";

#define SIZE_BUF		1024
static uint8_t buf[SIZE_BUF];

#define SIZE_PATH		256
static uint8_t path[SIZE_PATH];

#define SIZE_VERSION	16
static uint8_t version[SIZE_VERSION];

static const uint8_t HTTP_CONTENTS[] = 
	"<!DOCTYPE HTML>\n"
	"<html>\n"
	"<head>\n"
	"<title>It works! TINET httpd</title>\n"
	"</head>\n"
	"<body>\n"
	"<p>It works!</p>\n"
	"</body>\n"
	"</html>\n"
	;

static const uint8_t HTTP_HEADER200[] = 
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: "
//	"Content-Length: %d\r\n"
//	"\r\n"
	;

static const uint8_t HTTP_HEADER404[] = 
	"HTTP/1.1 404 Not Found\r\n"
	"Content-Type: text/html\r\n"
	"\r\n"
	;

static uint8_t HTTP_HEADER200_[256];

/*
 *  メインタスク : Main Tesk
 */
void main_task(intptr_t exinf)
{
	T_IPV4EP dst;
	ER er;
	uint8_t *p;
	int i;
	size_t len;
	uint8_t d;

	/*
	 *  HTTP サーバの初期化 : Initializing for HTTP server
	 */
	tcp_crep.repatr = 0;
	tcp_crep.myaddr.ipaddr = IPV4_ADDRANY;
	tcp_crep.myaddr.portno = 80;
	tcp_cre_rep(ID_REP, &tcp_crep);	// TCP受付口の生成 : Create a reception endpoint

	tcp_ccep.sbufsz = SIZE_CEP_BUF;
	tcp_ccep.rbufsz = SIZE_CEP_BUF;
	tcp_ccep.sbuf = cep_sbuf;
	tcp_ccep.rbuf = cep_rbuf;
	tcp_ccep.callback = NULL;
	tcp_cre_cep(ID_CEP, &tcp_ccep);	// TCP通信端点の生成 : Create a communication endpoint

	while (1) {
		/*
		 *  メインループ : Main loop
		 */
		if (E_OK != tcp_acp_cep(ID_CEP, ID_REP, &dst, TMO_FEVR))	// 待ち受け : Accept
			continue;

		er = tcp_rcv_dat(ID_CEP, (void *)&buf[0], SIZE_BUF, 30000);	// 受信 : Receive
		if (0 >= er)
			goto discconect;

		if (memcmp(&buf[0], HTTP_REQUEST_GET, (sizeof(HTTP_REQUEST_GET)-1)) != 0)
			goto discconect;

		p = &buf[0];
		p += (sizeof(HTTP_REQUEST_GET)-1);
		for (i = 0; i < SIZE_PATH; i++) {
			if (*p == ' ') {
				p++;
				break;
			}
			path[i] = *p++;
		}
		path[i] = 0;	// 終端 NULL : NULL terminator
		for (i = 0; i < SIZE_VERSION; i++) {
			if (*p == '\r')
				break;
			version[i] = *p++;
		}
		version[i] = 0;		// 終端 NULL : NULL terminator

		if (strcmp((char *)path, "/") == 0)
			strcpy((char *)path, "/index.html");

		/*
		 * ログ出力 : Log output
		 */
		syslog(LOG_NOTICE, "");
		syslog(LOG_NOTICE, "Method: GET");
		syslog(LOG_NOTICE, "Path: %s", path);
		syslog(LOG_NOTICE, "HTTP Version: %s", version);

		if (strcmp((char *)path, "/index.html") == 0) {
			/*
			 *  HTTP/1.1 200 OK
			 */
			memcpy(HTTP_HEADER200_, HTTP_HEADER200, sizeof(HTTP_HEADER200));
			p = &HTTP_HEADER200_[strlen((const char *)HTTP_HEADER200)];
			len = strlen((const char *)HTTP_CONTENTS);
			d = (len / 1000) % 10;
			if (d)	*p++ = '0' + d;
			d = (len / 100) % 10;
			if (d)	*p++ = '0' + d;
			d = (len / 10) % 10;
			if (d)	*p++ = '0' + d;
			*p++ = '0' + (len % 10);
			*p++ = '\r';
			*p++ = '\n';
			*p++ = '\r';
			*p++ = '\n';
			*p = 0;	// 終端 NULL : NULL terminator
			tcp_snd_dat(
				ID_CEP,
				(void *)&HTTP_HEADER200_[0],
				strlen((const char *)HTTP_HEADER200_),
				TMO_FEVR
			);	// 200ヘッダ送信 : Send 200 header
			tcp_snd_dat(
				ID_CEP,
				(void *)&HTTP_CONTENTS[0],
				sizeof(HTTP_CONTENTS)-1,
				TMO_FEVR
			);	// コンテンツ送信 : Send content
		} else {
			/*
			 *  HTTP/1.1 404 Not Found
			 */
			tcp_snd_dat(
				ID_CEP,
				(void *)&HTTP_HEADER404[0],
				sizeof(HTTP_HEADER404)-1,
				TMO_FEVR
			);	// 404ヘッダ送信 : Send 404 header
		}

discconect:
		tcp_sht_cep(ID_CEP);			// 送信終了 : Sending complete
		tcp_cls_cep(ID_CEP, TMO_FEVR);	// クローズ : Close
	}
}
