# src/phl/server.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 226/362 lines (62.43%)

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
|   18 |   56 | `static void SignalHandler(int sig)` |
|    - |   57 |  |
|    9 |   58 | `	(void)sig;` |
|   18 |   59 | `	g_shutdown = 1;` |
|   18 |   60 |  |
|    - |   61 | `#endif` |
|    - |   62 | `/*` |
|    - |   63 | ` * Install signal handlers for graceful shutdown.` |
|    - |   64 | ` */` |
|   18 |   65 | `static void InstallSignalHandlers(void)` |
|  ! 0 |   66 |  |
|    - |   67 | `#ifdef __WINNT__` |
|  ! 0 |   68 | `	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);` |
|    - |   69 | `#else` |
|    - |   70 | `	struct sigaction sa;` |
|   18 |   71 | `	memset(&sa, 0, sizeof(sa));` |
|   18 |   72 | `	sa.sa_handler = SignalHandler;` |
|   18 |   73 | `	sigaction(SIGINT, &sa, 0);` |
|   18 |   74 | `	sigaction(SIGTERM, &sa, 0);` |
|    - |   75 | `#endif` |
|   18 |   76 |  |
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
|   22 |  130 | `static int IsPathSafe(const char *zPath)` |
|  ! 0 |  131 |  |
|   22 |  132 | `	const char *z = zPath;` |
|  216 |  133 | `	while( *z ){` |
|  194 |  134 | `		if( z[0] == '.' && z[1] == '.' && (z[2] == '/' \|\| z[2] == '\\' \|\| z[2] == 0) ){` |
|  ! 0 |  135 | `			return 0;` |
|    - |  136 | `		}` |
|  194 |  137 | `		z++;` |
|  ! 0 |  138 | `	}` |
|   22 |  139 | `	return 1;` |
|   11 |  140 |  |
|    - |  141 | `/*` |
|    - |  142 | ` * Check if a file exists and is a regular file.` |
|    - |  143 | ` * Returns 1 if it exists, 0 otherwise. Sets *pSize to the file size.` |
|    - |  144 | ` */` |
|   22 |  145 | `static int FileExists(const char *zPath, long *pSize)` |
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
|   22 |  161 | `	if( stat(zPath, &st) != 0 ){` |
|    2 |  162 | `		return 0;` |
|    - |  163 | `	}` |
|   20 |  164 | `	if( !S_ISREG(st.st_mode) ){` |
|  ! 0 |  165 | `		return 0;` |
|    - |  166 | `	}` |
|   20 |  167 | `	if( pSize ){` |
|   20 |  168 | `		*pSize = (long)st.st_size;` |
|   10 |  169 | `	}` |
|   20 |  170 | `	return 1;` |
|    - |  171 | `#endif` |
|   11 |  172 |  |
|    - |  173 | `/*` |
|    - |  174 | ` * Check if a path is a directory.` |
|    - |  175 | ` */` |
|   22 |  176 | `static int IsDirectory(const char *zPath)` |
|  ! 0 |  177 |  |
|    - |  178 | `#ifdef __WINNT__` |
|  ! 0 |  179 | `	DWORD attr = GetFileAttributesA(zPath);` |
|  ! 0 |  180 | `	return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));` |
|    - |  181 | `#else` |
|    - |  182 | `	struct stat st;` |
|   22 |  183 | `	if( stat(zPath, &st) != 0 ){` |
|    2 |  184 | `		return 0;` |
|    - |  185 | `	}` |
|   20 |  186 | `	return S_ISDIR(st.st_mode);` |
|    - |  187 | `#endif` |
|   11 |  188 |  |
|    - |  189 | `/*` |
|    - |  190 | ` * Read the full HTTP request from a socket into a buffer.` |
|    - |  191 | ` * Reads headers first (until \r\n\r\n), then reads the body` |
|    - |  192 | ` * based on Content-Length if present.` |
|    - |  193 | ` * Returns total bytes read, or -1 on error.` |
|    - |  194 | ` */` |
|   22 |  195 | `static int ReadRequest(ph7_socket sock, char *zBuf, int nBufSize)` |
|  ! 0 |  196 |  |
|   22 |  197 | `	int nTotal = 0;` |
|    - |  198 | `	int nRead;` |
|    - |  199 | `	char *zHeaderEnd;` |
|    - |  200 | `	/* Read data until we find the end of headers */` |
|   22 |  201 | `	while( nTotal < nBufSize - 1 ){` |
|   22 |  202 | `		nRead = PH7_NetRecv(sock, zBuf + nTotal, nBufSize - 1 - nTotal, 0);` |
|   22 |  203 | `		if( nRead <= 0 ){` |
|  ! 0 |  204 | `			if( nTotal == 0 ){` |
|  ! 0 |  205 | `				return -1;` |
|    - |  206 | `			}` |
|  ! 0 |  207 | `			break;` |
|    - |  208 | `		}` |
|   22 |  209 | `		nTotal += nRead;` |
|   22 |  210 | `		zBuf[nTotal] = 0;` |
|    - |  211 | `		/* Check if we have the complete headers */` |
|   22 |  212 | `		zHeaderEnd = strstr(zBuf, "\r\n\r\n");` |
|   22 |  213 | `		if( zHeaderEnd ){` |
|   22 |  214 | `			int nHeaderLen = (int)(zHeaderEnd - zBuf) + 4;` |
|    - |  215 | `			/* Look for Content-Length */` |
|   22 |  216 | `			const char *zCL = strstr(zBuf, "Content-Length:");` |
|   22 |  217 | `			if( zCL == 0 ){` |
|   22 |  218 | `				zCL = strstr(zBuf, "content-length:");` |
|   11 |  219 | `			}` |
|   22 |  220 | `			if( zCL ){` |
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
|   22 |  235 | `			break;` |
|    - |  236 | `		}` |
|  ! 0 |  237 | `	}` |
|   22 |  238 | `	zBuf[nTotal] = 0;` |
|   22 |  239 | `	return nTotal;` |
|   11 |  240 |  |
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
|   16 |  278 | `static const char *StatusReason(int iStatus)` |
|  ! 0 |  279 |  |
|   16 |  280 | `	switch( iStatus ){` |
|   10 |  281 | `	case 200: return "OK";` |
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
|    8 |  297 |  |
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
|   16 |  326 | `static void SendVmResponse(ph7_socket sock, ph7_vm *pVm,` |
|    - |  327 | `							const void *pBody, int nBodyLen)` |
|  ! 0 |  328 |  |
|    - |  329 | `	int iStatus;` |
|    - |  330 | `	VmResponseCtx sCtx;` |
|    - |  331 | `	char zLine[512];` |
|    - |  332 | `	int nLine;` |
|   16 |  333 | `	iStatus = 200;` |
|   16 |  334 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_STATUS, &iStatus);` |
|    - |  335 | `	/* Status line */` |
|   16 |  336 | `	nLine = snprintf(zLine, sizeof(zLine), "HTTP/1.1 %d %s\r\n", iStatus, StatusReason(iStatus));` |
|   16 |  337 | `	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);` |
|   16 |  338 | `	PH7_NetSendAll(sock, zLine, nLine);` |
|    - |  339 | `	/* Script-set headers via callback */` |
|   16 |  340 | `	sCtx.sock = sock;` |
|   16 |  341 | `	sCtx.bHasContentType = 0;` |
|   16 |  342 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_HEADERS, VmResponseHeaderCB, &sCtx);` |
|    - |  343 | `	/* Default Content-Type if not set by the script */` |
|   16 |  344 | `	if( !sCtx.bHasContentType ){` |
|   14 |  345 | `		PH7_NetSendAll(sock, "Content-Type: text/html\r\n", 25);` |
|    7 |  346 | `	}` |
|    - |  347 | `	/* Standard headers */` |
|   16 |  348 | `	nLine = snprintf(zLine, sizeof(zLine),` |
|    - |  349 | `		"Content-Length: %d\r\n"` |
|    - |  350 | `		"Connection: close\r\n"` |
|    - |  351 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  352 | `		"\r\n",` |
|    - |  353 | `		nBodyLen);` |
|   16 |  354 | `	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);` |
|   16 |  355 | `	PH7_NetSendAll(sock, zLine, nLine);` |
|    - |  356 | `	/* Body */` |
|   16 |  357 | `	if( pBody && nBodyLen > 0 ){` |
|   16 |  358 | `		PH7_NetSendAll(sock, pBody, nBodyLen);` |
|    8 |  359 | `	}` |
|   16 |  360 |  |
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
|   44 |  404 | `static void ExtractRequestLine(const char *zRequest, char *zMethod, int nMethodSize,` |
|    - |  405 | `							   char *zPath, int nPathSize)` |
|  ! 0 |  406 |  |
|   44 |  407 | `	const char *z = zRequest;` |
|    - |  408 | `	int i;` |
|    - |  409 | `	/* Extract method */` |
|  176 |  410 | `	for( i = 0; *z && *z != ' ' && i < nMethodSize - 1; z++, i++ ){` |
|  132 |  411 | `		zMethod[i] = *z;` |
|   66 |  412 | `	}` |
|   44 |  413 | `	zMethod[i] = 0;` |
|    - |  414 | `	/* Skip space */` |
|   88 |  415 | `	while( *z == ' ' ) z++;` |
|    - |  416 | `	/* Extract path (stop at '?', ' ', or end) */` |
|  432 |  417 | `	for( i = 0; *z && *z != '?' && *z != ' ' && *z != '\r' && i < nPathSize - 1; z++, i++ ){` |
|  388 |  418 | `		zPath[i] = *z;` |
|  194 |  419 | `	}` |
|   44 |  420 | `	zPath[i] = 0;` |
|   44 |  421 |  |
|    - |  422 | `/*` |
|    - |  423 | ` * Resolve a request URI path to a filesystem path under the document root.` |
|    - |  424 | ` * Returns 1 on success, 0 on failure (bad path).` |
|    - |  425 | ` * zOut must be at least PHL_MAX_PATH bytes.` |
|    - |  426 | ` */` |
|   22 |  427 | `static int ResolvePath(const char *zDocRoot, const char *zUri, char *zOut)` |
|  ! 0 |  428 |  |
|   22 |  429 | `	if( !IsPathSafe(zUri) ){` |
|  ! 0 |  430 | `		return 0;` |
|    - |  431 | `	}` |
|   22 |  432 | `	snprintf(zOut, PHL_MAX_PATH, "%s%s", zDocRoot, zUri);` |
|    - |  433 | `	/* If path points to a directory, try index.php */` |
|   22 |  434 | `	if( IsDirectory(zOut) ){` |
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
|   22 |  446 | `	return 1;` |
|   11 |  447 |  |
|    - |  448 | `/*` |
|    - |  449 | ` * Check if a filename ends with ".php" (case-insensitive).` |
|    - |  450 | ` */` |
|   20 |  451 | `static int IsPhpFile(const char *zPath)` |
|  ! 0 |  452 |  |
|   20 |  453 | `	int n = (int)strlen(zPath);` |
|   20 |  454 | `	if( n < 4 ){` |
|  ! 0 |  455 | `		return 0;` |
|    - |  456 | `	}` |
|   10 |  457 | `	return (` |
|    - |  458 | `#ifdef __WINNT__` |
|    - |  459 | `		_stricmp(zPath + n - 4, ".php")` |
|    - |  460 | `#else` |
|   20 |  461 | `		strcasecmp(zPath + n - 4, ".php")` |
|    - |  462 | `#endif` |
|   20 |  463 | `		== 0);` |
|   10 |  464 |  |
|    - |  465 | `/*` |
|    - |  466 | ` * Execute a PHP script and send its output as an HTTP response.` |
|    - |  467 | ` * pEngine is the shared engine instance.` |
|    - |  468 | ` * zFilePath is the resolved filesystem path to the PHP file.` |
|    - |  469 | ` * zRawRequest/nRequestLen is the raw HTTP request for superglobal population.` |
|    - |  470 | ` * The additional server attributes are set from the request context.` |
|    - |  471 | ` */` |
|   16 |  472 | `static void ExecutePhpScript(ph7 *pEngine, ph7_socket client,` |
|    - |  473 | `							 const char *zFilePath, const char *zScriptName,` |
|    - |  474 | `							 const char *zRawRequest, int nRequestLen,` |
|    - |  475 | `							 const char *zHost, int iPort, const char *zDocRoot,` |
|    - |  476 | `							 const char *zRemoteAddr, int iRemotePort)` |
|  ! 0 |  477 |  |
|   16 |  478 | `	ph7_vm *pVm = 0;` |
|    - |  479 | `	const void *pOutput;` |
|    - |  480 | `	unsigned int nOutputLen;` |
|    - |  481 | `	char zPortBuf[16];` |
|    - |  482 | `	char zRemotePortBuf[16];` |
|    - |  483 | `	int rc;` |
|   16 |  484 | `	rc = ph7_compile_file(pEngine, zFilePath, &pVm, 0);` |
|   16 |  485 | `	if( rc != PH7_OK ){` |
|  ! 0 |  486 | `		SendError(client, 500, "Internal Server Error");` |
|  ! 0 |  487 | `		return;` |
|    - |  488 | `	}` |
|    - |  489 | `	/* Feed the raw HTTP request to populate $_SERVER, $_GET, $_POST, etc. */` |
|   16 |  490 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);` |
|    - |  491 | `	/* Set additional $_SERVER attributes */` |
|   16 |  492 | `	snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);` |
|   16 |  493 | `	snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);` |
|   16 |  494 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);` |
|   16 |  495 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);` |
|   16 |  496 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);` |
|   16 |  497 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);` |
|   16 |  498 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zFilePath, -1);` |
|   16 |  499 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zScriptName, -1);` |
|   16 |  500 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);` |
|   16 |  501 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);` |
|   16 |  502 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);` |
|    - |  503 | `	/* Execute the script (output accumulates in the VM's internal buffer) */` |
|   16 |  504 | `	ph7_vm_exec(pVm, 0);` |
|    - |  505 | `	/* Extract accumulated output */` |
|   16 |  506 | `	pOutput = 0;` |
|   16 |  507 | `	nOutputLen = 0;` |
|   16 |  508 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);` |
|    - |  509 | `	/* Send the response using VM-set headers and status code */` |
|   16 |  510 | `	SendVmResponse(client, pVm, pOutput, (int)nOutputLen);` |
|   16 |  511 | `	ph7_vm_release(pVm);` |
|    8 |  512 |  |
|    - |  513 | `/*` |
|    - |  514 | ` * Handle a single HTTP request.` |
|    - |  515 | ` */` |
|   22 |  516 | `static int HandleRequest(ph7 *pEngine, ph7_socket client,` |
|    - |  517 | `						  const char *zDocRoot, const char *zRouter,` |
|    - |  518 | `						  const char *zHost, int iPort,` |
|    - |  519 | `						  const char *zRawRequest, int nRequestLen,` |
|    - |  520 | `						  const char *zRemoteAddr, int iRemotePort)` |
|  ! 0 |  521 |  |
|    - |  522 | `	char zMethod[16];` |
|    - |  523 | `	char zUri[PHL_MAX_PATH];` |
|    - |  524 | `	char zFilePath[PHL_MAX_PATH];` |
|    - |  525 | `	long nFileSize;` |
|    - |  526 | `	/* Extract request line */` |
|   22 |  527 | `	ExtractRequestLine(zRawRequest, zMethod, sizeof(zMethod), zUri, sizeof(zUri));` |
|   11 |  528 | `	(void)zMethod;` |
|    - |  529 | `	/* Try router script first (if configured) */` |
|   22 |  530 | `	if( zRouter && zRouter[0] ){` |
|  ! 0 |  531 | `		ph7_vm *pVm = 0;` |
|    - |  532 | `		ph7_value *pRetVal;` |
|    - |  533 | `		int rc;` |
|    - |  534 | `		char zPortBuf[16];` |
|    - |  535 | `		char zRemotePortBuf[16];` |
|  ! 0 |  536 | `		rc = ph7_compile_file(pEngine, zRouter, &pVm, 0);` |
|  ! 0 |  537 | `		if( rc == PH7_OK ){` |
|  ! 0 |  538 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);` |
|  ! 0 |  539 | `			snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);` |
|  ! 0 |  540 | `			snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);` |
|  ! 0 |  541 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);` |
|  ! 0 |  542 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);` |
|  ! 0 |  543 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);` |
|  ! 0 |  544 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);` |
|  ! 0 |  545 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zRouter, -1);` |
|  ! 0 |  546 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zUri, -1);` |
|  ! 0 |  547 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);` |
|  ! 0 |  548 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);` |
|  ! 0 |  549 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);` |
|  ! 0 |  550 | `			ph7_vm_exec(pVm, 0);` |
|    - |  551 | `			/* Check if the router returned false (meaning: fall through to default serving) */` |
|  ! 0 |  552 | `			pRetVal = 0;` |
|  ! 0 |  553 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_EXEC_VALUE, &pRetVal);` |
|  ! 0 |  554 | `			if( pRetVal && ph7_value_is_bool(pRetVal) && !ph7_value_to_bool(pRetVal) ){` |
|    - |  555 | `				/* Router returned false: fall through to default file serving */` |
|  ! 0 |  556 | `				ph7_vm_release(pVm);` |
|  ! 0 |  557 | `			}else{` |
|    - |  558 | `				/* Router handled the request: send its output */` |
|    - |  559 | `				const void *pOutput;` |
|    - |  560 | `				unsigned int nOutputLen;` |
|  ! 0 |  561 | `				pOutput = 0;` |
|  ! 0 |  562 | `				nOutputLen = 0;` |
|  ! 0 |  563 | `				ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);` |
|  ! 0 |  564 | `				SendVmResponse(client, pVm, pOutput, (int)nOutputLen);` |
|  ! 0 |  565 | `				ph7_vm_release(pVm);` |
|  ! 0 |  566 | `				return 200;` |
|    - |  567 | `			}` |
|  ! 0 |  568 | `		}` |
|    - |  569 | `		/* Router compile failed or returned false: fall through */` |
|  ! 0 |  570 | `	}` |
|    - |  571 | `	/* Resolve URI to filesystem path */` |
|   22 |  572 | `	if( !ResolvePath(zDocRoot, zUri, zFilePath) ){` |
|  ! 0 |  573 | `		SendError(client, 403, "Forbidden");` |
|  ! 0 |  574 | `		return 403;` |
|    - |  575 | `	}` |
|    - |  576 | `	/* Check if file exists */` |
|   22 |  577 | `	if( FileExists(zFilePath, &nFileSize) ){` |
|   20 |  578 | `		if( IsPhpFile(zFilePath) ){` |
|   24 |  579 | `			ExecutePhpScript(pEngine, client, zFilePath, zUri,` |
|    8 |  580 | `							 zRawRequest, nRequestLen,` |
|    8 |  581 | `							 zHost, iPort, zDocRoot,` |
|    8 |  582 | `							 zRemoteAddr, iRemotePort);` |
|    8 |  583 | `		}else{` |
|    4 |  584 | `			ServeStaticFile(client, zFilePath, nFileSize);` |
|    - |  585 | `		}` |
|   20 |  586 | `		return 200;` |
|    - |  587 | `	}` |
|    2 |  588 | `	SendError(client, 404, "Not Found");` |
|    2 |  589 | `	return 404;` |
|   11 |  590 |  |
|    - |  591 | `/*` |
|    - |  592 | ` * Log a request line to stderr.` |
|    - |  593 | ` */` |
|   22 |  594 | `static void LogRequest(const char *zRemoteAddr, int iRemotePort,` |
|    - |  595 | `					   const char *zMethod, const char *zUri, int iStatus)` |
|  ! 0 |  596 |  |
|    - |  597 | `	time_t now;` |
|    - |  598 | `	struct tm tm_buf;` |
|    - |  599 | `	char zTime[64];` |
|   22 |  600 | `	time(&now);` |
|    - |  601 | `#ifdef __WINNT__` |
|  ! 0 |  602 | `	localtime_s(&tm_buf, &now);` |
|    - |  603 | `#else` |
|   22 |  604 | `	localtime_r(&now, &tm_buf);` |
|    - |  605 | `#endif` |
|   22 |  606 | `	strftime(zTime, sizeof(zTime), "%a %b %d %H:%M:%S %Y", &tm_buf);` |
|   33 |  607 | `	fprintf(stderr, "[%s] %s:%d [%d]: %s %s\n",` |
|   11 |  608 | `		zTime, zRemoteAddr, iRemotePort, iStatus, zMethod, zUri);` |
|   22 |  609 |  |
|    - |  610 | `/*` |
|    - |  611 | ` * Main server entry point.` |
|    - |  612 | ` */` |
|   18 |  613 | `int phl_serve(const char *zHost, int iPort, const char *zDocRoot, const char *zRouter)` |
|  ! 0 |  614 |  |
|    - |  615 | `	ph7 *pEngine;` |
|    - |  616 | `	ph7_socket listenSock;` |
|    - |  617 | `	ph7_socket clientSock;` |
|    - |  618 | `	struct sockaddr_in clientAddr;` |
|    - |  619 | `	ph7_socklen addrLen;` |
|    - |  620 | `	char *zRequestBuf;` |
|    - |  621 | `	int nRequestLen;` |
|    - |  622 | `	char zRemoteAddr[64];` |
|    - |  623 | `	int iRemotePort;` |
|    - |  624 | `	int rc;` |
|    - |  625 | `	/* Initialize networking */` |
|   18 |  626 | `	rc = PH7_NetInit();` |
|   18 |  627 | `	if( rc != PH7_OK ){` |
|  ! 0 |  628 | `		fprintf(stderr, "Error: Failed to initialize networking\n");` |
|  ! 0 |  629 | `		return 1;` |
|    - |  630 | `	}` |
|    - |  631 | `	/* Create the PH7 engine (shared across requests) */` |
|   18 |  632 | `	rc = ph7_init(&pEngine);` |
|   18 |  633 | `	if( rc != PH7_OK ){` |
|  ! 0 |  634 | `		fprintf(stderr, "Error: Failed to initialize PH7 engine\n");` |
|  ! 0 |  635 | `		PH7_NetCleanup();` |
|  ! 0 |  636 | `		return 1;` |
|    - |  637 | `	}` |
|    - |  638 | `	/* Create listening socket */` |
|   18 |  639 | `	listenSock = PH7_NetListen(zHost, iPort, 10);` |
|   18 |  640 | `	if( listenSock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  641 | `		fprintf(stderr, "Error: Failed to listen on %s:%d\n", zHost, iPort);` |
|  ! 0 |  642 | `		ph7_release(pEngine);` |
|  ! 0 |  643 | `		PH7_NetCleanup();` |
|  ! 0 |  644 | `		return 1;` |
|    - |  645 | `	}` |
|    - |  646 | `	/* Install signal handlers */` |
|   18 |  647 | `	InstallSignalHandlers();` |
|    - |  648 | `	/* Print banner */` |
|   18 |  649 | `	fprintf(stderr, "PHL %s Development Server started at http://%s:%d/\n", PH7_VERSION, zHost, iPort);` |
|   18 |  650 | `	fprintf(stderr, "Document root: %s\n", zDocRoot);` |
|   18 |  651 | `	if( zRouter ){` |
|  ! 0 |  652 | `		fprintf(stderr, "Router script: %s\n", zRouter);` |
|  ! 0 |  653 | `	}` |
|   18 |  654 | `	fprintf(stderr, "Press Ctrl+C to stop.\n");` |
|    - |  655 | `	/* Allocate request buffer */` |
|   18 |  656 | `	zRequestBuf = (char *)malloc(PHL_MAX_REQUEST);` |
|   18 |  657 | `	if( zRequestBuf == 0 ){` |
|  ! 0 |  658 | `		fprintf(stderr, "Error: Out of memory\n");` |
|  ! 0 |  659 | `		PH7_NetClose(listenSock);` |
|  ! 0 |  660 | `		ph7_release(pEngine);` |
|  ! 0 |  661 | `		PH7_NetCleanup();` |
|  ! 0 |  662 | `		return 1;` |
|    - |  663 | `	}` |
|    - |  664 | `	/* Accept loop */` |
|   40 |  665 | `	while( !g_shutdown ){` |
|   40 |  666 | `		addrLen = sizeof(clientAddr);` |
|   40 |  667 | `		clientSock = PH7_NetAccept(listenSock, (struct sockaddr *)&clientAddr, &addrLen);` |
|   40 |  668 | `		if( clientSock == PH7_NET_INVALID_SOCKET ){` |
|   18 |  669 | `			if( g_shutdown ){` |
|   18 |  670 | `				break;` |
|    - |  671 | `			}` |
|  ! 0 |  672 | `			continue;` |
|    - |  673 | `		}` |
|    - |  674 | `		/* Set a receive timeout so we don't block forever on a slow client */` |
|   22 |  675 | `		PH7_NetSetTimeout(clientSock, 5000);` |
|    - |  676 | `		/* Read the request */` |
|   22 |  677 | `		nRequestLen = ReadRequest(clientSock, zRequestBuf, PHL_MAX_REQUEST);` |
|   22 |  678 | `		if( nRequestLen > 0 ){` |
|    - |  679 | `			char zMethod[16];` |
|    - |  680 | `			char zUri[PHL_MAX_PATH];` |
|    - |  681 | `			/* Extract method and URI for logging */` |
|   22 |  682 | `			ExtractRequestLine(zRequestBuf, zMethod, sizeof(zMethod), zUri, sizeof(zUri));` |
|    - |  683 | `			/* Get client address info */` |
|   22 |  684 | `			PH7_NetAddrToString((struct sockaddr *)&clientAddr, zRemoteAddr, sizeof(zRemoteAddr));` |
|   22 |  685 | `			iRemotePort = PH7_NetAddrPort((struct sockaddr *)&clientAddr);` |
|    - |  686 | `			/* Handle the request */` |
|   33 |  687 | `			rc = HandleRequest(pEngine, clientSock, zDocRoot, zRouter,` |
|   11 |  688 | `						  zHost, iPort, zRequestBuf, nRequestLen,` |
|   11 |  689 | `						  zRemoteAddr, iRemotePort);` |
|    - |  690 | `			/* Log it */` |
|   22 |  691 | `			LogRequest(zRemoteAddr, iRemotePort, zMethod, zUri, rc);` |
|   11 |  692 | `		}` |
|   22 |  693 | `		PH7_NetClose(clientSock);` |
|  ! 0 |  694 | `	}` |
|    - |  695 | `	/* Cleanup */` |
|   18 |  696 | `	fprintf(stderr, "\nShutting down...\n");` |
|   18 |  697 | `	free(zRequestBuf);` |
|   18 |  698 | `	PH7_NetClose(listenSock);` |
|   18 |  699 | `	ph7_release(pEngine);` |
|   18 |  700 | `	PH7_NetCleanup();` |
|   18 |  701 | `	return 0;` |
|    9 |  702 |  |
|    - |  703 |  |
|    - |  704 | `#endif /* PHL_ENABLE_SERVER */` |
|    - |  705 |  |
