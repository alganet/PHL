# src/phl/server.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 267/435 lines (61.38%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7.h"` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `#include "server.h"` |
|    - |    8 | `#if defined(PHL_ENABLE_SERVER) && !defined(PH7_ENABLE_NET)` |
|    - |    9 | `#error "PHL_ENABLE_SERVER requires PH7_ENABLE_NET"` |
|    - |   10 | `#endif` |
|    - |   11 | `#if defined(PHL_ENABLE_SERVER) && defined(PH7_ENABLE_NET)` |
|    - |   12 | `/*` |
|    - |   13 | ` * PHL Built-in Development Server.` |
|    - |   14 | `` * A single-threaded HTTP server for testing, similar to PHP's `php -S`.`` |
|    - |   15 | ` */` |
|    - |   16 | `#include <stdio.h>` |
|    - |   17 | `#include <stdlib.h>` |
|    - |   18 | `#include <string.h>` |
|    - |   19 | `#include <time.h>` |
|    - |   20 |  |
|    - |   21 | `#ifndef __WINNT__` |
|    - |   22 | `#include <strings.h>` |
|    - |   23 | `#endif` |
|    - |   24 |  |
|    - |   25 | `#ifdef __WINNT__` |
|    - |   26 | `#include <winsock2.h>` |
|    - |   27 | `#include <ws2tcpip.h>` |
|    - |   28 | `#include <windows.h>` |
|    - |   29 | `#include <io.h>` |
|    - |   30 | `#else` |
|    - |   31 | `#include <sys/socket.h>` |
|    - |   32 | `#include <netinet/in.h>` |
|    - |   33 | `#include <sys/stat.h>` |
|    - |   34 | `#include <unistd.h>` |
|    - |   35 | `#include <signal.h>` |
|    - |   36 | `#endif` |
|    - |   37 |  |
|    - |   38 | `/* Maximum size of a single HTTP request (headers + body) */` |
|    - |   39 | `#define PHL_MAX_REQUEST  (1024 * 1024)` |
|    - |   40 | `/* Read buffer size for static files */` |
|    - |   41 | `#define PHL_FILE_BUF     (64 * 1024)` |
|    - |   42 | `/* Maximum path length */` |
|    - |   43 | `#define PHL_MAX_PATH     1024` |
|    - |   44 |  |
|    - |   45 | `/* Shutdown flag set by signal handler */` |
|    - |   46 | `static volatile int g_shutdown = 0;` |
|    - |   47 |  |
|    - |   48 | `#ifdef __WINNT__` |
|    - |   49 | `static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)` |
|  ! 0 |   50 |  |
|    - |   51 | `	(void)dwCtrlType;` |
|  ! 0 |   52 | `	g_shutdown = 1;` |
|  ! 0 |   53 | `	return TRUE;` |
|  ! 0 |   54 |  |
|    - |   55 | `#else` |
|   20 |   56 | `static void SignalHandler(int sig)` |
|    - |   57 |  |
|   10 |   58 | `	(void)sig;` |
|   20 |   59 | `	g_shutdown = 1;` |
|   20 |   60 |  |
|    - |   61 | `#endif` |
|    - |   62 | `/*` |
|    - |   63 | ` * Install signal handlers for graceful shutdown.` |
|    - |   64 | ` */` |
|   20 |   65 | `static void InstallSignalHandlers(void)` |
|  ! 0 |   66 |  |
|    - |   67 | `#ifdef __WINNT__` |
|  ! 0 |   68 | `	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);` |
|    - |   69 | `#else` |
|    - |   70 | `	struct sigaction sa;` |
|   20 |   71 | `	memset(&sa, 0, sizeof(sa));` |
|   20 |   72 | `	sa.sa_handler = SignalHandler;` |
|   20 |   73 | `	sigaction(SIGINT, &sa, 0);` |
|   20 |   74 | `	sigaction(SIGTERM, &sa, 0);` |
|    - |   75 | `#endif` |
|   20 |   76 |  |
|    - |   77 | `/*` |
|    - |   78 | ` * MIME type lookup table.` |
|    - |   79 | ` */` |
|    - |   80 | `typedef struct MimeEntry MimeEntry;` |
|    - |   81 | `struct MimeEntry {` |
|    - |   82 | `	const char *zExt;` |
|    - |   83 | `	const char *zType;` |
|    - |   84 | `};` |
|    - |   85 | `static const MimeEntry aMime[] = {` |
|    - |   86 | `	{ ".html", "text/html" },` |
|    - |   87 | `	{ ".htm",  "text/html" },` |
|    - |   88 | `	{ ".css",  "text/css" },` |
|    - |   89 | `	{ ".js",   "application/javascript" },` |
|    - |   90 | `	{ ".json", "application/json" },` |
|    - |   91 | `	{ ".xml",  "application/xml" },` |
|    - |   92 | `	{ ".txt",  "text/plain" },` |
|    - |   93 | `	{ ".png",  "image/png" },` |
|    - |   94 | `	{ ".jpg",  "image/jpeg" },` |
|    - |   95 | `	{ ".jpeg", "image/jpeg" },` |
|    - |   96 | `	{ ".gif",  "image/gif" },` |
|    - |   97 | `	{ ".svg",  "image/svg+xml" },` |
|    - |   98 | `	{ ".ico",  "image/x-icon" },` |
|    - |   99 | `	{ ".pdf",  "application/pdf" },` |
|    - |  100 | `	{ ".woff", "font/woff" },` |
|    - |  101 | `	{ ".woff2","font/woff2" },` |
|    - |  102 | `	{ ".ttf",  "font/ttf" },` |
|    - |  103 | `	{ 0, 0 }` |
|    - |  104 | `};` |
|    4 |  105 | `static const char *GetMimeType(const char *zPath)` |
|  ! 0 |  106 |  |
|    - |  107 | `	const char *zDot;` |
|    - |  108 | `	int i;` |
|    4 |  109 | `	zDot = strrchr(zPath, '.');` |
|    4 |  110 | `	if( zDot == 0 ){` |
|  ! 0 |  111 | `		return "application/octet-stream";` |
|    - |  112 | `	}` |
|   28 |  113 | `	for( i = 0; aMime[i].zExt != 0; i++ ){` |
|   14 |  114 | `		if(` |
|    - |  115 | `#ifdef __WINNT__` |
|    - |  116 | `			_stricmp(zDot, aMime[i].zExt)` |
|    - |  117 | `#else` |
|   42 |  118 | `			strcasecmp(zDot, aMime[i].zExt)` |
|    - |  119 | `#endif` |
|   14 |  120 | `			== 0 ){` |
|    4 |  121 | `			return aMime[i].zType;` |
|    - |  122 | `		}` |
|   12 |  123 | `	}` |
|  ! 0 |  124 | `	return "application/octet-stream";` |
|    2 |  125 |  |
|    - |  126 | `/*` |
|    - |  127 | ` * Check if a path contains directory traversal sequences.` |
|    - |  128 | ` * Returns 1 if the path is safe, 0 if it contains "..".` |
|    - |  129 | ` */` |
|   28 |  130 | `static int IsPathSafe(const char *zPath)` |
|  ! 0 |  131 |  |
|   28 |  132 | `	const char *z = zPath;` |
|  282 |  133 | `	while( *z ){` |
|  254 |  134 | `		if( z[0] == '.' && z[1] == '.' && (z[2] == '/' \|\| z[2] == '\\' \|\| z[2] == 0) ){` |
|  ! 0 |  135 | `			return 0;` |
|    - |  136 | `		}` |
|  254 |  137 | `		z++;` |
|  ! 0 |  138 | `	}` |
|   28 |  139 | `	return 1;` |
|   14 |  140 |  |
|    - |  141 | `/*` |
|    - |  142 | ` * Check if a file exists and is a regular file.` |
|    - |  143 | ` * Returns 1 if it exists, 0 otherwise. Sets *pSize to the file size.` |
|    - |  144 | ` */` |
|   28 |  145 | `static int FileExists(const char *zPath, long *pSize)` |
|  ! 0 |  146 |  |
|    - |  147 | `#ifdef __WINNT__` |
|    - |  148 | `	WIN32_FILE_ATTRIBUTE_DATA info;` |
|  ! 0 |  149 | `	if( !GetFileAttributesExA(zPath, GetFileExInfoStandard, &info) ){` |
|  ! 0 |  150 | `		return 0;` |
|    - |  151 | `	}` |
|  ! 0 |  152 | `	if( info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){` |
|  ! 0 |  153 | `		return 0;` |
|    - |  154 | `	}` |
|  ! 0 |  155 | `	if( pSize ){` |
|  ! 0 |  156 | `		*pSize = (long)info.nFileSizeLow;` |
|    - |  157 | `	}` |
|  ! 0 |  158 | `	return 1;` |
|    - |  159 | `#else` |
|    - |  160 | `	struct stat st;` |
|   28 |  161 | `	if( stat(zPath, &st) != 0 ){` |
|    2 |  162 | `		return 0;` |
|    - |  163 | `	}` |
|   26 |  164 | `	if( !S_ISREG(st.st_mode) ){` |
|  ! 0 |  165 | `		return 0;` |
|    - |  166 | `	}` |
|   26 |  167 | `	if( pSize ){` |
|   26 |  168 | `		*pSize = (long)st.st_size;` |
|   13 |  169 | `	}` |
|   26 |  170 | `	return 1;` |
|    - |  171 | `#endif` |
|   14 |  172 |  |
|    - |  173 | `/*` |
|    - |  174 | ` * Check if a path is a directory.` |
|    - |  175 | ` */` |
|   28 |  176 | `static int IsDirectory(const char *zPath)` |
|  ! 0 |  177 |  |
|    - |  178 | `#ifdef __WINNT__` |
|  ! 0 |  179 | `	DWORD attr = GetFileAttributesA(zPath);` |
|  ! 0 |  180 | `	return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));` |
|    - |  181 | `#else` |
|    - |  182 | `	struct stat st;` |
|   28 |  183 | `	if( stat(zPath, &st) != 0 ){` |
|    2 |  184 | `		return 0;` |
|    - |  185 | `	}` |
|   26 |  186 | `	return S_ISDIR(st.st_mode);` |
|    - |  187 | `#endif` |
|   14 |  188 |  |
|    - |  189 | `/*` |
|    - |  190 | ` * Read the full HTTP request from a socket into a buffer.` |
|    - |  191 | ` * Reads headers first (until \r\n\r\n), then reads the body` |
|    - |  192 | ` * based on Content-Length if present.` |
|    - |  193 | ` * Returns total bytes read, or -1 on error.` |
|    - |  194 | ` */` |
|   28 |  195 | `static int ReadRequest(ph7_socket sock, char *zBuf, int nBufSize)` |
|  ! 0 |  196 |  |
|   28 |  197 | `	int nTotal = 0;` |
|    - |  198 | `	int nRead;` |
|    - |  199 | `	char *zHeaderEnd;` |
|    - |  200 | `	/* Read data until we find the end of headers */` |
|   28 |  201 | `	while( nTotal < nBufSize - 1 ){` |
|   28 |  202 | `		nRead = PH7_NetRecv(sock, zBuf + nTotal, nBufSize - 1 - nTotal, 0);` |
|   28 |  203 | `		if( nRead <= 0 ){` |
|  ! 0 |  204 | `			if( nTotal == 0 ){` |
|  ! 0 |  205 | `				return -1;` |
|    - |  206 | `			}` |
|  ! 0 |  207 | `			break;` |
|    - |  208 | `		}` |
|   28 |  209 | `		nTotal += nRead;` |
|   28 |  210 | `		zBuf[nTotal] = 0;` |
|    - |  211 | `		/* Check if we have the complete headers */` |
|   28 |  212 | `		zHeaderEnd = strstr(zBuf, "\r\n\r\n");` |
|   28 |  213 | `		if( zHeaderEnd ){` |
|   28 |  214 | `			int nHeaderLen = (int)(zHeaderEnd - zBuf) + 4;` |
|    - |  215 | `			/* Look for Content-Length */` |
|   28 |  216 | `			const char *zCL = strstr(zBuf, "Content-Length:");` |
|   28 |  217 | `			if( zCL == 0 ){` |
|   28 |  218 | `				zCL = strstr(zBuf, "content-length:");` |
|   14 |  219 | `			}` |
|   28 |  220 | `			if( zCL ){` |
|  ! 0 |  221 | `				int nContentLen = atoi(zCL + 15);` |
|  ! 0 |  222 | `				int nExpected = nHeaderLen + nContentLen;` |
|  ! 0 |  223 | `				if( nExpected > nBufSize - 1 ){` |
|  ! 0 |  224 | `					nExpected = nBufSize - 1;` |
|  ! 0 |  225 | `				}` |
|    - |  226 | `				/* Read remaining body bytes */` |
|  ! 0 |  227 | `				while( nTotal < nExpected ){` |
|  ! 0 |  228 | `					nRead = PH7_NetRecv(sock, zBuf + nTotal, nExpected - nTotal, 0);` |
|  ! 0 |  229 | `					if( nRead <= 0 ){` |
|  ! 0 |  230 | `						break;` |
|    - |  231 | `					}` |
|  ! 0 |  232 | `					nTotal += nRead;` |
|  ! 0 |  233 | `				}` |
|  ! 0 |  234 | `			}` |
|   28 |  235 | `			break;` |
|    - |  236 | `		}` |
|  ! 0 |  237 | `	}` |
|   28 |  238 | `	zBuf[nTotal] = 0;` |
|   28 |  239 | `	return nTotal;` |
|   14 |  240 |  |
|    - |  241 | `/*` |
|    - |  242 | ` * Send an HTTP response with headers.` |
|    - |  243 | ` */` |
|    2 |  244 | `static void SendResponse(ph7_socket sock, int iStatus, const char *zStatus,` |
|    - |  245 | `						 const char *zContentType, const void *pBody, int nBodyLen)` |
|  ! 0 |  246 |  |
|    - |  247 | `	char zHeader[512];` |
|    - |  248 | `	int nHeader;` |
|    2 |  249 | `	nHeader = snprintf(zHeader, sizeof(zHeader),` |
|    - |  250 | `		"HTTP/1.1 %d %s\r\n"` |
|    - |  251 | `		"Content-Type: %s\r\n"` |
|    - |  252 | `		"Content-Length: %d\r\n"` |
|    - |  253 | `		"Connection: close\r\n"` |
|    - |  254 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  255 | `		"\r\n",` |
|    - |  256 | `		iStatus, zStatus, zContentType, nBodyLen);` |
|    2 |  257 | `	PH7_NetSendAll(sock, zHeader, nHeader);` |
|    2 |  258 | `	if( pBody && nBodyLen > 0 ){` |
|    2 |  259 | `		PH7_NetSendAll(sock, pBody, nBodyLen);` |
|    1 |  260 | `	}` |
|    2 |  261 |  |
|    - |  262 | `/*` |
|    - |  263 | ` * Send an HTTP error response.` |
|    - |  264 | ` */` |
|    2 |  265 | `static void SendError(ph7_socket sock, int iStatus, const char *zStatus)` |
|  ! 0 |  266 |  |
|    - |  267 | `	char zBody[256];` |
|    - |  268 | `	int nBody;` |
|    2 |  269 | `	nBody = snprintf(zBody, sizeof(zBody),` |
|    - |  270 | `		"<html><head><title>%d %s</title></head>"` |
|    - |  271 | `		"<body><h1>%d %s</h1></body></html>",` |
|    - |  272 | `		iStatus, zStatus, iStatus, zStatus);` |
|    2 |  273 | `	SendResponse(sock, iStatus, zStatus, "text/html", zBody, nBody);` |
|    2 |  274 |  |
|    - |  275 | `/*` |
|    - |  276 | ` * Map a status code to a reason phrase.` |
|    - |  277 | ` */` |
|   22 |  278 | `static const char *StatusReason(int iStatus)` |
|  ! 0 |  279 |  |
|   22 |  280 | `	switch( iStatus ){` |
|   16 |  281 | `	case 200: return "OK";` |
|    4 |  282 | `	case 201: return "Created";` |
|  ! 0 |  283 | `	case 204: return "No Content";` |
|  ! 0 |  284 | `	case 301: return "Moved Permanently";` |
|  ! 0 |  285 | `	case 302: return "Found";` |
|  ! 0 |  286 | `	case 304: return "Not Modified";` |
|  ! 0 |  287 | `	case 400: return "Bad Request";` |
|  ! 0 |  288 | `	case 401: return "Unauthorized";` |
|    2 |  289 | `	case 403: return "Forbidden";` |
|  ! 0 |  290 | `	case 404: return "Not Found";` |
|  ! 0 |  291 | `	case 405: return "Method Not Allowed";` |
|  ! 0 |  292 | `	case 500: return "Internal Server Error";` |
|  ! 0 |  293 | `	case 502: return "Bad Gateway";` |
|  ! 0 |  294 | `	case 503: return "Service Unavailable";` |
|  ! 0 |  295 | `	default:  return "";` |
|    - |  296 | `	}` |
|   11 |  297 |  |
|    - |  298 | `/*` |
|    - |  299 | ` * Send an HTTP response using the VM's response headers and status code.` |
|    - |  300 | ` * Falls back to Content-Type: text/html if the script didn't set one.` |
|    - |  301 | ` */` |
|    - |  302 | `/*` |
|    - |  303 | ` * State passed to the response header callback.` |
|    - |  304 | ` */` |
|    - |  305 | `typedef struct VmResponseCtx VmResponseCtx;` |
|    - |  306 | `struct VmResponseCtx {` |
|    - |  307 | `	ph7_socket sock;` |
|    - |  308 | `	int bHasContentType;` |
|    - |  309 | `};` |
|   12 |  310 | `static int VmResponseHeaderCB(const char *zName, unsigned int nName,` |
|    - |  311 | `							   const char *zValue, unsigned int nValue,` |
|    - |  312 | `							   void *pUserData)` |
|  ! 0 |  313 |  |
|   12 |  314 | `	VmResponseCtx *pCtx = (VmResponseCtx *)pUserData;` |
|    - |  315 | `	char zLine[512];` |
|    - |  316 | `	int nLine;` |
|   12 |  317 | `	nLine = snprintf(zLine, sizeof(zLine), "%.*s: %.*s\r\n",` |
|    - |  318 | `		(int)nName, zName, (int)nValue, zValue);` |
|   12 |  319 | `	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);` |
|   12 |  320 | `	PH7_NetSendAll(pCtx->sock, zLine, nLine);` |
|   12 |  321 | `	if( nName == 12 && SyStrnicmp(zName, "Content-Type", 12) == 0 ){` |
|    2 |  322 | `		pCtx->bHasContentType = 1;` |
|    1 |  323 | `	}` |
|   12 |  324 | `	return PH7_OK;` |
|  ! 0 |  325 |  |
|   22 |  326 | `static void SendVmResponse(ph7_socket sock, ph7_vm *pVm,` |
|    - |  327 | `							const void *pBody, int nBodyLen)` |
|  ! 0 |  328 |  |
|    - |  329 | `	int iStatus;` |
|    - |  330 | `	VmResponseCtx sCtx;` |
|    - |  331 | `	char zLine[512];` |
|    - |  332 | `	int nLine;` |
|   22 |  333 | `	iStatus = 200;` |
|   22 |  334 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_STATUS, &iStatus);` |
|    - |  335 | `	/* Status line */` |
|   22 |  336 | `	nLine = snprintf(zLine, sizeof(zLine), "HTTP/1.1 %d %s\r\n", iStatus, StatusReason(iStatus));` |
|   22 |  337 | `	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);` |
|   22 |  338 | `	PH7_NetSendAll(sock, zLine, nLine);` |
|    - |  339 | `	/* Script-set headers via callback */` |
|   22 |  340 | `	sCtx.sock = sock;` |
|   22 |  341 | `	sCtx.bHasContentType = 0;` |
|   22 |  342 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_HEADERS, VmResponseHeaderCB, &sCtx);` |
|    - |  343 | `	/* Default Content-Type if not set by the script */` |
|   22 |  344 | `	if( !sCtx.bHasContentType ){` |
|   20 |  345 | `		PH7_NetSendAll(sock, "Content-Type: text/html\r\n", 25);` |
|   10 |  346 | `	}` |
|    - |  347 | `	/* Standard headers */` |
|   22 |  348 | `	nLine = snprintf(zLine, sizeof(zLine),` |
|    - |  349 | `		"Content-Length: %d\r\n"` |
|    - |  350 | `		"Connection: close\r\n"` |
|    - |  351 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  352 | `		"\r\n",` |
|    - |  353 | `		nBodyLen);` |
|   22 |  354 | `	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);` |
|   22 |  355 | `	PH7_NetSendAll(sock, zLine, nLine);` |
|    - |  356 | `	/* Body */` |
|   22 |  357 | `	if( pBody && nBodyLen > 0 ){` |
|   22 |  358 | `		PH7_NetSendAll(sock, pBody, nBodyLen);` |
|   11 |  359 | `	}` |
|   22 |  360 |  |
|    - |  361 | `/*` |
|    - |  362 | ` * Serve a static file.` |
|    - |  363 | ` */` |
|    4 |  364 | `static void ServeStaticFile(ph7_socket sock, const char *zPath, long nFileSize)` |
|  ! 0 |  365 |  |
|    - |  366 | `	const char *zMime;` |
|    - |  367 | `	char zHeader[512];` |
|    - |  368 | `	char zFileBuf[PHL_FILE_BUF];` |
|    - |  369 | `	FILE *pFile;` |
|    - |  370 | `	int nHeader;` |
|    - |  371 | `	size_t nRead;` |
|    4 |  372 | `	zMime = GetMimeType(zPath);` |
|    - |  373 | `#ifdef __WINNT__` |
|  ! 0 |  374 | `	if( fopen_s(&pFile, zPath, "rb") != 0 ) pFile = 0;` |
|    - |  375 | `#else` |
|    4 |  376 | `	pFile = fopen(zPath, "rb");` |
|    - |  377 | `#endif` |
|    4 |  378 | `	if( pFile == 0 ){` |
|  ! 0 |  379 | `		SendError(sock, 500, "Internal Server Error");` |
|  ! 0 |  380 | `		return;` |
|    - |  381 | `	}` |
|    4 |  382 | `	nHeader = snprintf(zHeader, sizeof(zHeader),` |
|    - |  383 | `		"HTTP/1.1 200 OK\r\n"` |
|    - |  384 | `		"Content-Type: %s\r\n"` |
|    - |  385 | `		"Content-Length: %ld\r\n"` |
|    - |  386 | `		"Connection: close\r\n"` |
|    - |  387 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  388 | `		"\r\n",` |
|    - |  389 | `		zMime, nFileSize);` |
|    4 |  390 | `	PH7_NetSendAll(sock, zHeader, nHeader);` |
|    8 |  391 | `	while( (nRead = fread(zFileBuf, 1, sizeof(zFileBuf), pFile)) > 0 ){` |
|    4 |  392 | `		if( PH7_NetSendAll(sock, zFileBuf, (int)nRead) != PH7_OK ){` |
|  ! 0 |  393 | `			break;` |
|    - |  394 | `		}` |
|  ! 0 |  395 | `	}` |
|    4 |  396 | `	fclose(pFile);` |
|    2 |  397 |  |
|    - |  398 | `/*` |
|    - |  399 | ` * Extract the request method and URI path from the first line of the HTTP request.` |
|    - |  400 | ` * E.g., "GET /foo/bar?q=1 HTTP/1.1\r\n..."` |
|    - |  401 | ` * Writes the method to zMethod (up to nMethodSize) and path to zPath (up to nPathSize).` |
|    - |  402 | ` * The path is the URI without the query string.` |
|    - |  403 | ` */` |
|   56 |  404 | `static void ExtractRequestLine(const char *zRequest, char *zMethod, int nMethodSize,` |
|    - |  405 | `							   char *zPath, int nPathSize)` |
|  ! 0 |  406 |  |
|   56 |  407 | `	const char *z = zRequest;` |
|    - |  408 | `	int i;` |
|    - |  409 | `	/* Extract method */` |
|  224 |  410 | `	for( i = 0; *z && *z != ' ' && i < nMethodSize - 1; z++, i++ ){` |
|  168 |  411 | `		zMethod[i] = *z;` |
|   84 |  412 | `	}` |
|   56 |  413 | `	zMethod[i] = 0;` |
|    - |  414 | `	/* Skip space */` |
|  112 |  415 | `	while( *z == ' ' ) z++;` |
|    - |  416 | `	/* Extract path (stop at '?', ' ', or end) */` |
|  564 |  417 | `	for( i = 0; *z && *z != '?' && *z != ' ' && *z != '\r' && i < nPathSize - 1; z++, i++ ){` |
|  508 |  418 | `		zPath[i] = *z;` |
|  254 |  419 | `	}` |
|   56 |  420 | `	zPath[i] = 0;` |
|   56 |  421 |  |
|    - |  422 | `/*` |
|    - |  423 | ` * Resolve a request URI path to a filesystem path under the document root.` |
|    - |  424 | ` * Returns 1 on success, 0 on failure (bad path).` |
|    - |  425 | ` * zOut must be at least PHL_MAX_PATH bytes.` |
|    - |  426 | ` */` |
|   28 |  427 | `static int ResolvePath(const char *zDocRoot, const char *zUri, char *zOut)` |
|  ! 0 |  428 |  |
|   28 |  429 | `	if( !IsPathSafe(zUri) ){` |
|  ! 0 |  430 | `		return 0;` |
|    - |  431 | `	}` |
|   28 |  432 | `	snprintf(zOut, PHL_MAX_PATH, "%s%s", zDocRoot, zUri);` |
|    - |  433 | `	/* If path points to a directory, try index.php */` |
|   28 |  434 | `	if( IsDirectory(zOut) ){` |
|  ! 0 |  435 | `		int n = (int)strlen(zOut);` |
|    - |  436 | `		/* Ensure trailing slash */` |
|  ! 0 |  437 | `		if( n > 0 && zOut[n-1] != '/' && zOut[n-1] != '\\' ){` |
|  ! 0 |  438 | `			if( n < PHL_MAX_PATH - 1 ){` |
|  ! 0 |  439 | `				zOut[n] = '/';` |
|  ! 0 |  440 | `				zOut[n+1] = 0;` |
|  ! 0 |  441 | `				n++;` |
|  ! 0 |  442 | `			}` |
|  ! 0 |  443 | `		}` |
|  ! 0 |  444 | `		snprintf(zOut + n, PHL_MAX_PATH - n, "index.php");` |
|  ! 0 |  445 | `	}` |
|   28 |  446 | `	return 1;` |
|   14 |  447 |  |
|    - |  448 | `/*` |
|    - |  449 | ` * Check if a filename ends with ".php" (case-insensitive).` |
|    - |  450 | ` */` |
|   26 |  451 | `static int IsPhpFile(const char *zPath)` |
|  ! 0 |  452 |  |
|   26 |  453 | `	int n = (int)strlen(zPath);` |
|   26 |  454 | `	if( n < 4 ){` |
|  ! 0 |  455 | `		return 0;` |
|    - |  456 | `	}` |
|   13 |  457 | `	return (` |
|    - |  458 | `#ifdef __WINNT__` |
|    - |  459 | `		_stricmp(zPath + n - 4, ".php")` |
|    - |  460 | `#else` |
|   26 |  461 | `		strcasecmp(zPath + n - 4, ".php")` |
|    - |  462 | `#endif` |
|   26 |  463 | `		== 0);` |
|   13 |  464 |  |
|    - |  465 | `/*` |
|    - |  466 | ` * Return a file's last-modification time as a comparable integer (0 on error).` |
|    - |  467 | ` * Used to invalidate the compiled-VM cache when a script is edited, preserving` |
|    - |  468 | ` * the dev-server expectation that reloading picks up source changes.` |
|    - |  469 | ` */` |
|   22 |  470 | `static long long GetFileMtime(const char *zPath)` |
|  ! 0 |  471 |  |
|    - |  472 | `#ifdef __WINNT__` |
|    - |  473 | `	WIN32_FILE_ATTRIBUTE_DATA info;` |
|  ! 0 |  474 | `	if( !GetFileAttributesExA(zPath, GetFileExInfoStandard, &info) ){` |
|  ! 0 |  475 | `		return 0;` |
|    - |  476 | `	}` |
|  ! 0 |  477 | `	return ((long long)info.ftLastWriteTime.dwHighDateTime << 32)` |
|    - |  478 | `		\| (long long)info.ftLastWriteTime.dwLowDateTime;` |
|    - |  479 | `#else` |
|    - |  480 | `	struct stat st;` |
|   22 |  481 | `	if( stat(zPath, &st) != 0 ){` |
|  ! 0 |  482 | `		return 0;` |
|    - |  483 | `	}` |
|   22 |  484 | `	return (long long)st.st_mtime;` |
|    - |  485 | `#endif` |
|   11 |  486 |  |
|    - |  487 | `/* ---- Compiled-VM reuse cache ------------------------------------------------` |
|    - |  488 | ` * Compilation dominates the per-request cost (the on-hardware profile measured` |
|    - |  489 | ` * 167 of 197 ms/request in compile). We therefore compile each script once and` |
|    - |  490 | ` * re-execute it per request, calling ph7_vm_reset() in between to clear all` |
|    - |  491 | ` * per-execution state (globals, superglobals, statics, output, ...). The cache` |
|    - |  492 | ` * is keyed by the resolved filesystem path and is safe without locking because` |
|    - |  493 | ` * the dev server is single-threaded. Set PHL_NO_REUSE=1 to fall back to the old` |
|    - |  494 | ` * compile-per-request behaviour (useful for diffing). */` |
|    - |  495 | `#define PHL_VM_CACHE_SIZE 16` |
|    - |  496 | `typedef struct PhlVmCacheEntry {` |
|    - |  497 | `	char zPath[PHL_MAX_PATH]; /* Resolved script path ("" = empty slot) */` |
|    - |  498 | `	ph7_vm *pVm;              /* Compiled, reusable VM */` |
|    - |  499 | `	unsigned long nUse;       /* Last-use clock for LRU eviction */` |
|    - |  500 | `	long long nMtime;         /* Source mtime when compiled (cache invalidation) */` |
|    - |  501 | `} PhlVmCacheEntry;` |
|    - |  502 | `static PhlVmCacheEntry g_vmCache[PHL_VM_CACHE_SIZE];` |
|    - |  503 | `static unsigned long g_vmCacheClock = 0;` |
|    - |  504 | `static int g_vmReuse = 1;` |
|    - |  505 |  |
|    - |  506 | `/*` |
|    - |  507 | ` * Acquire a ready-to-execute VM for zPath. On a cache hit the VM is reset and` |
|    - |  508 | ` * reused; on a miss it is compiled and cached (evicting the least-recently-used` |
|    - |  509 | ` * entry when full). Returns NULL on compile error. On success *pbCached tells` |
|    - |  510 | ` * the caller whether the VM is cache-owned (do NOT release it) or a throwaway` |
|    - |  511 | ` * (release after use, e.g. when reuse is disabled).` |
|    - |  512 | ` */` |
|   22 |  513 | `static ph7_vm *AcquireScriptVm(ph7 *pEngine, const char *zPath, int *pbCached)` |
|  ! 0 |  514 |  |
|   22 |  515 | `	int i, iFree = -1, iLru = -1;` |
|   22 |  516 | `	ph7_vm *pVm = 0;` |
|    - |  517 | `	long long nMtime;` |
|   22 |  518 | `	if( !g_vmReuse ){` |
|  ! 0 |  519 | `		*pbCached = 0;` |
|  ! 0 |  520 | `		return ph7_compile_file(pEngine, zPath, &pVm, 0) == PH7_OK ? pVm : 0;` |
|    - |  521 | `	}` |
|   22 |  522 | `	nMtime = GetFileMtime(zPath);` |
|  278 |  523 | `	for( i = 0 ; i < PHL_VM_CACHE_SIZE ; i++ ){` |
|  262 |  524 | `		if( g_vmCache[i].pVm == 0 ){` |
|  256 |  525 | `			if( iFree < 0 ){ iFree = i; }` |
|  256 |  526 | `			continue;` |
|    - |  527 | `		}` |
|    6 |  528 | `		if( strcmp(g_vmCache[i].zPath, zPath) == 0 ){` |
|    - |  529 | `			/* Hit: reuse only if the source is unchanged on disk (dev-server` |
|    - |  530 | `			 * live-edit) and the reset succeeds; otherwise drop and recompile. */` |
|    6 |  531 | `			if( g_vmCache[i].nMtime == nMtime` |
|    6 |  532 | `			 && ph7_vm_reset(g_vmCache[i].pVm) == PH7_OK ){` |
|    6 |  533 | `				g_vmCache[i].nUse = ++g_vmCacheClock;` |
|    6 |  534 | `				*pbCached = 1;` |
|    6 |  535 | `				return g_vmCache[i].pVm;` |
|    - |  536 | `			}` |
|  ! 0 |  537 | `			ph7_vm_release(g_vmCache[i].pVm);` |
|  ! 0 |  538 | `			g_vmCache[i].pVm = 0;` |
|  ! 0 |  539 | `			g_vmCache[i].zPath[0] = '\0';` |
|  ! 0 |  540 | `			iFree = i;` |
|  ! 0 |  541 | `			break;` |
|    - |  542 | `		}` |
|  ! 0 |  543 | `		if( iLru < 0 \|\| g_vmCache[i].nUse < g_vmCache[iLru].nUse ){` |
|  ! 0 |  544 | `			iLru = i;` |
|  ! 0 |  545 | `		}` |
|  ! 0 |  546 | `	}` |
|    - |  547 | `	/* Miss: compile a fresh VM. */` |
|   16 |  548 | `	if( ph7_compile_file(pEngine, zPath, &pVm, 0) != PH7_OK ){` |
|  ! 0 |  549 | `		*pbCached = 0;` |
|  ! 0 |  550 | `		return 0;` |
|    - |  551 | `	}` |
|   16 |  552 | `	if( iFree < 0 ){` |
|    - |  553 | `		/* Cache full: evict the least-recently-used entry. */` |
|  ! 0 |  554 | `		ph7_vm_release(g_vmCache[iLru].pVm);` |
|  ! 0 |  555 | `		iFree = iLru;` |
|  ! 0 |  556 | `	}` |
|   16 |  557 | `	snprintf(g_vmCache[iFree].zPath, sizeof(g_vmCache[iFree].zPath), "%s", zPath);` |
|   16 |  558 | `	g_vmCache[iFree].pVm = pVm;` |
|   16 |  559 | `	g_vmCache[iFree].nUse = ++g_vmCacheClock;` |
|   16 |  560 | `	g_vmCache[iFree].nMtime = nMtime;` |
|   16 |  561 | `	*pbCached = 1;` |
|   16 |  562 | `	return pVm;` |
|   11 |  563 |  |
|    - |  564 | `/*` |
|    - |  565 | ` * Release every cached VM. Called at server shutdown.` |
|    - |  566 | ` */` |
|   20 |  567 | `static void ReleaseVmCache(void)` |
|  ! 0 |  568 |  |
|    - |  569 | `	int i;` |
|  340 |  570 | `	for( i = 0 ; i < PHL_VM_CACHE_SIZE ; i++ ){` |
|  320 |  571 | `		if( g_vmCache[i].pVm ){` |
|   16 |  572 | `			ph7_vm_release(g_vmCache[i].pVm);` |
|   16 |  573 | `			g_vmCache[i].pVm = 0;` |
|   16 |  574 | `			g_vmCache[i].zPath[0] = '\0';` |
|    8 |  575 | `		}` |
|  160 |  576 | `	}` |
|   20 |  577 |  |
|    - |  578 | `/*` |
|    - |  579 | ` * Execute a PHP script and send its output as an HTTP response.` |
|    - |  580 | ` * pEngine is the shared engine instance.` |
|    - |  581 | ` * zFilePath is the resolved filesystem path to the PHP file.` |
|    - |  582 | ` * zRawRequest/nRequestLen is the raw HTTP request for superglobal population.` |
|    - |  583 | ` * The additional server attributes are set from the request context.` |
|    - |  584 | ` */` |
|   22 |  585 | `static void ExecutePhpScript(ph7 *pEngine, ph7_socket client,` |
|    - |  586 | `							 const char *zFilePath, const char *zScriptName,` |
|    - |  587 | `							 const char *zRawRequest, int nRequestLen,` |
|    - |  588 | `							 const char *zHost, int iPort, const char *zDocRoot,` |
|    - |  589 | `							 const char *zRemoteAddr, int iRemotePort)` |
|  ! 0 |  590 |  |
|   22 |  591 | `	ph7_vm *pVm = 0;` |
|    - |  592 | `	const void *pOutput;` |
|    - |  593 | `	unsigned int nOutputLen;` |
|    - |  594 | `	char zPortBuf[16];` |
|    - |  595 | `	char zRemotePortBuf[16];` |
|   22 |  596 | `	int bCached = 0;` |
|   22 |  597 | `	pVm = AcquireScriptVm(pEngine, zFilePath, &bCached);` |
|   22 |  598 | `	if( pVm == 0 ){` |
|  ! 0 |  599 | `		SendError(client, 500, "Internal Server Error");` |
|  ! 0 |  600 | `		return;` |
|    - |  601 | `	}` |
|    - |  602 | `	/* Feed the raw HTTP request to populate $_SERVER, $_GET, $_POST, etc. */` |
|   22 |  603 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);` |
|    - |  604 | `	/* Set additional $_SERVER attributes */` |
|   22 |  605 | `	snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);` |
|   22 |  606 | `	snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);` |
|   22 |  607 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);` |
|   22 |  608 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);` |
|   22 |  609 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);` |
|   22 |  610 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);` |
|   22 |  611 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zFilePath, -1);` |
|   22 |  612 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zScriptName, -1);` |
|   22 |  613 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);` |
|   22 |  614 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);` |
|   22 |  615 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);` |
|    - |  616 | `	/* Execute the script (output accumulates in the VM's internal buffer) */` |
|   22 |  617 | `	ph7_vm_exec(pVm, 0);` |
|    - |  618 | `	/* Extract accumulated output */` |
|   22 |  619 | `	pOutput = 0;` |
|   22 |  620 | `	nOutputLen = 0;` |
|   22 |  621 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);` |
|    - |  622 | `	/* Send the response using VM-set headers and status code */` |
|   22 |  623 | `	SendVmResponse(client, pVm, pOutput, (int)nOutputLen);` |
|    - |  624 | `	/* Cache-owned VMs are kept for reuse; throwaways are released. */` |
|   22 |  625 | `	if( !bCached ){` |
|  ! 0 |  626 | `		ph7_vm_release(pVm);` |
|  ! 0 |  627 | `	}` |
|   11 |  628 |  |
|    - |  629 | `/*` |
|    - |  630 | ` * Handle a single HTTP request.` |
|    - |  631 | ` */` |
|   28 |  632 | `static int HandleRequest(ph7 *pEngine, ph7_socket client,` |
|    - |  633 | `						  const char *zDocRoot, const char *zRouter,` |
|    - |  634 | `						  const char *zHost, int iPort,` |
|    - |  635 | `						  const char *zRawRequest, int nRequestLen,` |
|    - |  636 | `						  const char *zRemoteAddr, int iRemotePort)` |
|  ! 0 |  637 |  |
|    - |  638 | `	char zMethod[16];` |
|    - |  639 | `	char zUri[PHL_MAX_PATH];` |
|    - |  640 | `	char zFilePath[PHL_MAX_PATH];` |
|    - |  641 | `	long nFileSize;` |
|    - |  642 | `	/* Extract request line */` |
|   28 |  643 | `	ExtractRequestLine(zRawRequest, zMethod, sizeof(zMethod), zUri, sizeof(zUri));` |
|   14 |  644 | `	(void)zMethod;` |
|    - |  645 | `	/* Try router script first (if configured) */` |
|   28 |  646 | `	if( zRouter && zRouter[0] ){` |
|  ! 0 |  647 | `		ph7_vm *pVm = 0;` |
|    - |  648 | `		ph7_value *pRetVal;` |
|  ! 0 |  649 | `		int bCached = 0;` |
|    - |  650 | `		char zPortBuf[16];` |
|    - |  651 | `		char zRemotePortBuf[16];` |
|  ! 0 |  652 | `		pVm = AcquireScriptVm(pEngine, zRouter, &bCached);` |
|  ! 0 |  653 | `		if( pVm != 0 ){` |
|  ! 0 |  654 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);` |
|  ! 0 |  655 | `			snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);` |
|  ! 0 |  656 | `			snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);` |
|  ! 0 |  657 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);` |
|  ! 0 |  658 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);` |
|  ! 0 |  659 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);` |
|  ! 0 |  660 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);` |
|  ! 0 |  661 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zRouter, -1);` |
|  ! 0 |  662 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zUri, -1);` |
|  ! 0 |  663 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);` |
|  ! 0 |  664 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);` |
|  ! 0 |  665 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);` |
|  ! 0 |  666 | `			ph7_vm_exec(pVm, 0);` |
|    - |  667 | `			/* Check if the router returned false (meaning: fall through to default serving) */` |
|  ! 0 |  668 | `			pRetVal = 0;` |
|  ! 0 |  669 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_EXEC_VALUE, &pRetVal);` |
|  ! 0 |  670 | `			if( pRetVal && ph7_value_is_bool(pRetVal) && !ph7_value_to_bool(pRetVal) ){` |
|    - |  671 | `				/* Router returned false: fall through to default file serving */` |
|  ! 0 |  672 | `				if( !bCached ){` |
|  ! 0 |  673 | `					ph7_vm_release(pVm);` |
|  ! 0 |  674 | `				}` |
|  ! 0 |  675 | `			}else{` |
|    - |  676 | `				/* Router handled the request: send its output */` |
|    - |  677 | `				const void *pOutput;` |
|    - |  678 | `				unsigned int nOutputLen;` |
|  ! 0 |  679 | `				pOutput = 0;` |
|  ! 0 |  680 | `				nOutputLen = 0;` |
|  ! 0 |  681 | `				ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);` |
|  ! 0 |  682 | `				SendVmResponse(client, pVm, pOutput, (int)nOutputLen);` |
|  ! 0 |  683 | `				if( !bCached ){` |
|  ! 0 |  684 | `					ph7_vm_release(pVm);` |
|  ! 0 |  685 | `				}` |
|  ! 0 |  686 | `				return 200;` |
|    - |  687 | `			}` |
|  ! 0 |  688 | `		}` |
|    - |  689 | `		/* Router compile failed or returned false: fall through */` |
|  ! 0 |  690 | `	}` |
|    - |  691 | `	/* Resolve URI to filesystem path */` |
|   28 |  692 | `	if( !ResolvePath(zDocRoot, zUri, zFilePath) ){` |
|  ! 0 |  693 | `		SendError(client, 403, "Forbidden");` |
|  ! 0 |  694 | `		return 403;` |
|    - |  695 | `	}` |
|    - |  696 | `	/* Check if file exists */` |
|   28 |  697 | `	if( FileExists(zFilePath, &nFileSize) ){` |
|   26 |  698 | `		if( IsPhpFile(zFilePath) ){` |
|   33 |  699 | `			ExecutePhpScript(pEngine, client, zFilePath, zUri,` |
|   11 |  700 | `							 zRawRequest, nRequestLen,` |
|   11 |  701 | `							 zHost, iPort, zDocRoot,` |
|   11 |  702 | `							 zRemoteAddr, iRemotePort);` |
|   11 |  703 | `		}else{` |
|    4 |  704 | `			ServeStaticFile(client, zFilePath, nFileSize);` |
|    - |  705 | `		}` |
|   26 |  706 | `		return 200;` |
|    - |  707 | `	}` |
|    2 |  708 | `	SendError(client, 404, "Not Found");` |
|    2 |  709 | `	return 404;` |
|   14 |  710 |  |
|    - |  711 | `/*` |
|    - |  712 | ` * Log a request line to stderr.` |
|    - |  713 | ` */` |
|   28 |  714 | `static void LogRequest(const char *zRemoteAddr, int iRemotePort,` |
|    - |  715 | `					   const char *zMethod, const char *zUri, int iStatus)` |
|  ! 0 |  716 |  |
|    - |  717 | `	time_t now;` |
|    - |  718 | `	struct tm tm_buf;` |
|    - |  719 | `	char zTime[64];` |
|   28 |  720 | `	time(&now);` |
|    - |  721 | `#ifdef __WINNT__` |
|  ! 0 |  722 | `	localtime_s(&tm_buf, &now);` |
|    - |  723 | `#else` |
|   28 |  724 | `	localtime_r(&now, &tm_buf);` |
|    - |  725 | `#endif` |
|   28 |  726 | `	strftime(zTime, sizeof(zTime), "%a %b %d %H:%M:%S %Y", &tm_buf);` |
|   42 |  727 | `	fprintf(stderr, "[%s] %s:%d [%d]: %s %s\n",` |
|   14 |  728 | `		zTime, zRemoteAddr, iRemotePort, iStatus, zMethod, zUri);` |
|   28 |  729 |  |
|    - |  730 | `/*` |
|    - |  731 | ` * Main server entry point.` |
|    - |  732 | ` */` |
|   20 |  733 | `int phl_serve(const char *zHost, int iPort, const char *zDocRoot, const char *zRouter)` |
|  ! 0 |  734 |  |
|    - |  735 | `	ph7 *pEngine;` |
|    - |  736 | `	ph7_socket listenSock;` |
|    - |  737 | `	ph7_socket clientSock;` |
|    - |  738 | `	struct sockaddr_in clientAddr;` |
|    - |  739 | `	ph7_socklen addrLen;` |
|    - |  740 | `	char *zRequestBuf;` |
|    - |  741 | `	int nRequestLen;` |
|    - |  742 | `	char zRemoteAddr[64];` |
|    - |  743 | `	int iRemotePort;` |
|    - |  744 | `	int rc;` |
|    - |  745 | `	/* Compile-once / reuse is on by default; PHL_NO_REUSE=1 forces the legacy` |
|    - |  746 | `	 * compile-per-request path for behaviour diffing. */` |
|    - |  747 | `	{` |
|   20 |  748 | `		const char *zNoReuse = getenv("PHL_NO_REUSE");` |
|   20 |  749 | `		if( zNoReuse && zNoReuse[0] && zNoReuse[0] != '0' ){` |
|  ! 0 |  750 | `			g_vmReuse = 0;` |
|  ! 0 |  751 | `		}` |
|    - |  752 | `	}` |
|    - |  753 | `	/* Initialize networking */` |
|   20 |  754 | `	rc = PH7_NetInit();` |
|   20 |  755 | `	if( rc != PH7_OK ){` |
|  ! 0 |  756 | `		fprintf(stderr, "Error: Failed to initialize networking\n");` |
|  ! 0 |  757 | `		return 1;` |
|    - |  758 | `	}` |
|    - |  759 | `	/* Create the PH7 engine (shared across requests) */` |
|   20 |  760 | `	rc = ph7_init(&pEngine);` |
|   20 |  761 | `	if( rc != PH7_OK ){` |
|  ! 0 |  762 | `		fprintf(stderr, "Error: Failed to initialize PH7 engine\n");` |
|  ! 0 |  763 | `		PH7_NetCleanup();` |
|  ! 0 |  764 | `		return 1;` |
|    - |  765 | `	}` |
|    - |  766 | `	/* Create listening socket */` |
|   20 |  767 | `	listenSock = PH7_NetListen(zHost, iPort, 10);` |
|   20 |  768 | `	if( listenSock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  769 | `		fprintf(stderr, "Error: Failed to listen on %s:%d\n", zHost, iPort);` |
|  ! 0 |  770 | `		ph7_release(pEngine);` |
|  ! 0 |  771 | `		PH7_NetCleanup();` |
|  ! 0 |  772 | `		return 1;` |
|    - |  773 | `	}` |
|    - |  774 | `	/* Install signal handlers */` |
|   20 |  775 | `	InstallSignalHandlers();` |
|    - |  776 | `	/* Print banner */` |
|   20 |  777 | `	fprintf(stderr, "PHL %s Development Server started at http://%s:%d/\n", PH7_VERSION, zHost, iPort);` |
|   20 |  778 | `	fprintf(stderr, "Document root: %s\n", zDocRoot);` |
|   20 |  779 | `	if( zRouter ){` |
|  ! 0 |  780 | `		fprintf(stderr, "Router script: %s\n", zRouter);` |
|  ! 0 |  781 | `	}` |
|   20 |  782 | `	fprintf(stderr, "Press Ctrl+C to stop.\n");` |
|    - |  783 | `	/* Allocate request buffer */` |
|   20 |  784 | `	zRequestBuf = (char *)malloc(PHL_MAX_REQUEST);` |
|   20 |  785 | `	if( zRequestBuf == 0 ){` |
|  ! 0 |  786 | `		fprintf(stderr, "Error: Out of memory\n");` |
|  ! 0 |  787 | `		PH7_NetClose(listenSock);` |
|  ! 0 |  788 | `		ph7_release(pEngine);` |
|  ! 0 |  789 | `		PH7_NetCleanup();` |
|  ! 0 |  790 | `		return 1;` |
|    - |  791 | `	}` |
|    - |  792 | `	/* Accept loop */` |
|   48 |  793 | `	while( !g_shutdown ){` |
|   48 |  794 | `		addrLen = sizeof(clientAddr);` |
|   48 |  795 | `		clientSock = PH7_NetAccept(listenSock, (struct sockaddr *)&clientAddr, &addrLen);` |
|   48 |  796 | `		if( clientSock == PH7_NET_INVALID_SOCKET ){` |
|   20 |  797 | `			if( g_shutdown ){` |
|   20 |  798 | `				break;` |
|    - |  799 | `			}` |
|  ! 0 |  800 | `			continue;` |
|    - |  801 | `		}` |
|    - |  802 | `		/* Set a receive timeout so we don't block forever on a slow client */` |
|   28 |  803 | `		PH7_NetSetTimeout(clientSock, 5000);` |
|    - |  804 | `		/* Read the request */` |
|   28 |  805 | `		nRequestLen = ReadRequest(clientSock, zRequestBuf, PHL_MAX_REQUEST);` |
|   28 |  806 | `		if( nRequestLen > 0 ){` |
|    - |  807 | `			char zMethod[16];` |
|    - |  808 | `			char zUri[PHL_MAX_PATH];` |
|    - |  809 | `			/* Extract method and URI for logging */` |
|   28 |  810 | `			ExtractRequestLine(zRequestBuf, zMethod, sizeof(zMethod), zUri, sizeof(zUri));` |
|    - |  811 | `			/* Get client address info */` |
|   28 |  812 | `			PH7_NetAddrToString((struct sockaddr *)&clientAddr, zRemoteAddr, sizeof(zRemoteAddr));` |
|   28 |  813 | `			iRemotePort = PH7_NetAddrPort((struct sockaddr *)&clientAddr);` |
|    - |  814 | `			/* Handle the request */` |
|   42 |  815 | `			rc = HandleRequest(pEngine, clientSock, zDocRoot, zRouter,` |
|   14 |  816 | `						  zHost, iPort, zRequestBuf, nRequestLen,` |
|   14 |  817 | `						  zRemoteAddr, iRemotePort);` |
|    - |  818 | `			/* Log it */` |
|   28 |  819 | `			LogRequest(zRemoteAddr, iRemotePort, zMethod, zUri, rc);` |
|   14 |  820 | `		}` |
|   28 |  821 | `		PH7_NetClose(clientSock);` |
|  ! 0 |  822 | `	}` |
|    - |  823 | `	/* Cleanup */` |
|   20 |  824 | `	fprintf(stderr, "\nShutting down...\n");` |
|   20 |  825 | `	ReleaseVmCache();` |
|   20 |  826 | `	free(zRequestBuf);` |
|   20 |  827 | `	PH7_NetClose(listenSock);` |
|   20 |  828 | `	ph7_release(pEngine);` |
|   20 |  829 | `	PH7_NetCleanup();` |
|   20 |  830 | `	return 0;` |
|   10 |  831 |  |
|    - |  832 |  |
|    - |  833 | `#endif /* PHL_ENABLE_SERVER */` |
|    - |  834 |  |
