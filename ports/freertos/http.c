/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * HTTP server for PHL on FreeRTOS.
 * Listens on port 80, compiles and executes an embedded PHP script
 * for each request using the full PH7 HTTP VM infrastructure:
 * superglobals ($_SERVER, $_GET, $_POST), header(), http_response_code(), etc.
 */
#include "phl_port.h"
#include "ph7.h"

#include "FreeRTOS.h"
#include "task.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#include <stdio.h>
#include <string.h>

#define HTTP_PORT          80
#define HTTP_BACKLOG       2
#define HTTP_RECV_BUF      1024
#define HTTP_LINE_BUF      512

/* The PHP script served for every request */
static const char zPhpScript[] =
	"<?php\n"
	"echo '<html><body>';\n"
	"echo '<h1>PHL on FreeRTOS</h1>';\n"
	"echo '<p>Method: ' . $_SERVER['REQUEST_METHOD'] . '</p>';\n"
	"echo '<p>URI: ' . $_SERVER['REQUEST_URI'] . '</p>';\n"
	"echo '<p>Server: ' . $_SERVER['SERVER_SOFTWARE'] . '</p>';\n"
	"$items = ['Arrays', 'Loops', 'Classes', 'Functions'];\n"
	"echo '<ul>';\n"
	"foreach ($items as $item) {\n"
	"    echo '<li>' . $item . ' work!</li>';\n"
	"}\n"
	"echo '</ul>';\n"
	"echo '</body></html>';\n"
	"?>";

static const char *StatusReason(int iStatus)
{
	switch( iStatus ){
	case 200: return "OK";
	case 204: return "No Content";
	case 301: return "Moved Permanently";
	case 302: return "Found";
	case 304: return "Not Modified";
	case 400: return "Bad Request";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 500: return "Internal Server Error";
	default:  return "";
	}
}

/* State for the response header callback */
typedef struct {
	Socket_t xClient;
	int bHasContentType;
} ResponseCtx;

static int ResponseHeaderCB(const char *zName, unsigned int nName,
                            const char *zValue, unsigned int nValue,
                            void *pUserData)
{
	ResponseCtx *pCtx = (ResponseCtx *)pUserData;
	char zLine[HTTP_LINE_BUF];
	int nLine;
	nLine = snprintf(zLine, sizeof(zLine), "%.*s: %.*s\r\n",
		(int)nName, zName, (int)nValue, zValue);
	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);
	FreeRTOS_send(pCtx->xClient, zLine, nLine, 0);
	if( nName == 12 && strncasecmp(zName, "Content-Type", 12) == 0 ){
		pCtx->bHasContentType = 1;
	}
	return PH7_OK;
}

static void SendVmResponse(Socket_t xClient, ph7_vm *pVm,
                           const void *pBody, int nBodyLen)
{
	int iStatus = 200;
	ResponseCtx sCtx;
	char zLine[HTTP_LINE_BUF];
	int nLine;

	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_STATUS, &iStatus);

	/* Status line */
	nLine = snprintf(zLine, sizeof(zLine), "HTTP/1.0 %d %s\r\n",
		iStatus, StatusReason(iStatus));
	FreeRTOS_send(xClient, zLine, nLine, 0);

	/* Script-set headers via callback */
	sCtx.xClient = xClient;
	sCtx.bHasContentType = 0;
	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_HEADERS, ResponseHeaderCB, &sCtx);

	/* Default Content-Type if not set by the script */
	if( !sCtx.bHasContentType ){
		FreeRTOS_send(xClient, "Content-Type: text/html\r\n", 25, 0);
	}

	/* Standard headers + end of headers */
	nLine = snprintf(zLine, sizeof(zLine),
		"Content-Length: %d\r\n"
		"Connection: close\r\n"
		"Server: PHL/" PH7_VERSION " FreeRTOS\r\n"
		"\r\n",
		nBodyLen);
	FreeRTOS_send(xClient, zLine, nLine, 0);

	/* Body */
	if( pBody && nBodyLen > 0 ){
		FreeRTOS_send(xClient, pBody, nBodyLen, 0);
	}
}

static void HandleRequest(Socket_t xClient, const char *zRawRequest, int nRequestLen)
{
	ph7 *pEngine = 0;
	ph7_vm *pVm = 0;
	const void *pOutput;
	unsigned int nOutputLen;
	char zPortBuf[8];
	int rc;

	rc = ph7_init(&pEngine);
	if( rc != PH7_OK ){
		return;
	}
	rc = ph7_compile_v2(pEngine, zPhpScript, -1, &pVm, 0);
	if( rc != PH7_OK ){
		ph7_release(pEngine);
		return;
	}

	/* Feed raw HTTP request — populates $_SERVER, $_GET, $_POST, $_COOKIE */
	ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);

	/* Set additional $_SERVER attributes */
	snprintf(zPortBuf, sizeof(zPortBuf), "%d", HTTP_PORT);
	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " FreeRTOS", -1);
	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", "0.0.0.0", -1);
	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);
	ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);

	/* Execute — output accumulates in the VM's internal buffer */
	ph7_vm_exec(pVm, 0);

	/* Extract accumulated output */
	pOutput = 0;
	nOutputLen = 0;
	ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);

	/* Send HTTP response using VM headers and status code */
	SendVmResponse(xClient, pVm, pOutput, (int)nOutputLen);

	ph7_vm_release(pVm);
	ph7_release(pEngine);
}

void vHTTPServerTask(void *pvParameters)
{
	Socket_t xListenSocket, xClientSocket;
	struct freertos_sockaddr xBindAddr, xClientAddr;
	socklen_t xClientAddrLen;
	char zRecvBuf[HTTP_RECV_BUF];
	int nRecvLen;

	(void)pvParameters;

	printf("[HTTP] Starting server on port %d\n", HTTP_PORT);

	xListenSocket = FreeRTOS_socket(
		FREERTOS_AF_INET4, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
	configASSERT(xListenSocket != FREERTOS_INVALID_SOCKET);

	/* Socket options */
	{
		TickType_t xTimeout = pdMS_TO_TICKS(30000);
		WinProperties_t xWinProps;
		FreeRTOS_setsockopt(xListenSocket, 0, FREERTOS_SO_RCVTIMEO, &xTimeout, sizeof(xTimeout));

		xWinProps.lTxBufSize = ipconfigTCP_TX_BUFFER_LENGTH;
		xWinProps.lTxWinSize = 2;
		xWinProps.lRxBufSize = ipconfigTCP_RX_BUFFER_LENGTH;
		xWinProps.lRxWinSize = 2;
		FreeRTOS_setsockopt(xListenSocket, 0, FREERTOS_SO_WIN_PROPERTIES, &xWinProps, sizeof(xWinProps));
	}

	memset(&xBindAddr, 0, sizeof(xBindAddr));
	xBindAddr.sin_port = FreeRTOS_htons(HTTP_PORT);
	xBindAddr.sin_family = FREERTOS_AF_INET;

	FreeRTOS_bind(xListenSocket, &xBindAddr, sizeof(xBindAddr));
	FreeRTOS_listen(xListenSocket, HTTP_BACKLOG);

	printf("[HTTP] Listening on port %d\n", HTTP_PORT);

	for( ;; ){
		xClientAddrLen = sizeof(xClientAddr);
		xClientSocket = FreeRTOS_accept(xListenSocket, &xClientAddr, &xClientAddrLen);
		if( xClientSocket == NULL || xClientSocket == FREERTOS_INVALID_SOCKET ){
			continue;
		}

		/* Read raw HTTP request */
		nRecvLen = FreeRTOS_recv(xClientSocket, zRecvBuf, sizeof(zRecvBuf) - 1, 0);
		if( nRecvLen > 0 ){
			zRecvBuf[nRecvLen] = '\0';
			HandleRequest(xClientSocket, zRecvBuf, nRecvLen);
		}

		/* Graceful shutdown */
		FreeRTOS_shutdown(xClientSocket, FREERTOS_SHUT_RDWR);
		{
			TickType_t xTimeOnShutdown = xTaskGetTickCount();
			while( FreeRTOS_recv(xClientSocket, zRecvBuf, sizeof(zRecvBuf), 0) >= 0 ){
				if( ( xTaskGetTickCount() - xTimeOnShutdown ) > pdMS_TO_TICKS(2000) ){
					break;
				}
			}
		}
		FreeRTOS_closesocket(xClientSocket);
	}
}
