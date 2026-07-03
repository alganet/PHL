# src/phl/server.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 277/450 lines (61.56%)

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
|    - |   48 | `/* Resolved interpreter path, exposed to scripts as the PHP_BINARY constant.` |
|    - |   49 | ` * Set once in phl_serve(); the matching expand callback mirrors the CLI's` |
|    - |   50 | ` * (PHL_PhpBinaryConst is static to phl.c, so the server keeps its own). */` |
|    - |   51 | `static const char *g_phpBinaryPath = 0;` |
|    4 |   52 | `static void PhlServerPhpBinaryConst(ph7_value *pVal, void *pUserData)` |
|  ! 0 |   53 | `{` |
|    4 |   54 | `	ph7_value_string(pVal, (const char *)pUserData, -1);` |
|    4 |   55 | `}` |
|    - |   56 | `/* Define PHP_BINARY on a freshly compiled VM. Constants persist across` |
|    - |   57 | ` * ph7_vm_reset(), so a cache-owned VM needs this only once, at compile time. */` |
|   18 |   58 | `static void DefinePhpBinary(ph7_vm *pVm)` |
|  ! 0 |   59 | `{` |
|   18 |   60 | `	if( g_phpBinaryPath ){` |
|   18 |   61 | `		ph7_create_constant(pVm, "PHP_BINARY", PhlServerPhpBinaryConst, (void *)g_phpBinaryPath);` |
|    9 |   62 | `	}` |
|   18 |   63 | `}` |
|    - |   64 |  |
|    - |   65 | `#ifdef __WINNT__` |
|    - |   66 | `static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)` |
|  ! 0 |   67 | `{` |
|    - |   68 | `	(void)dwCtrlType;` |
|  ! 0 |   69 | `	g_shutdown = 1;` |
|  ! 0 |   70 | `	return TRUE;` |
|  ! 0 |   71 | `}` |
|    - |   72 | `#else` |
|   22 |   73 | `static void SignalHandler(int sig)` |
|    - |   74 | `{` |
|   11 |   75 | `	(void)sig;` |
|   22 |   76 | `	g_shutdown = 1;` |
|   22 |   77 | `}` |
|    - |   78 | `#endif` |
|    - |   79 | `/*` |
|    - |   80 | ` * Install signal handlers for graceful shutdown.` |
|    - |   81 | ` */` |
|   22 |   82 | `static void InstallSignalHandlers(void)` |
|  ! 0 |   83 | `{` |
|    - |   84 | `#ifdef __WINNT__` |
|  ! 0 |   85 | `	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);` |
|    - |   86 | `#else` |
|    - |   87 | `	struct sigaction sa;` |
|   22 |   88 | `	memset(&sa, 0, sizeof(sa));` |
|   22 |   89 | `	sa.sa_handler = SignalHandler;` |
|   22 |   90 | `	sigaction(SIGINT, &sa, 0);` |
|   22 |   91 | `	sigaction(SIGTERM, &sa, 0);` |
|    - |   92 | `#endif` |
|   22 |   93 | `}` |
|    - |   94 | `/*` |
|    - |   95 | ` * MIME type lookup table.` |
|    - |   96 | ` */` |
|    - |   97 | `typedef struct MimeEntry MimeEntry;` |
|    - |   98 | `struct MimeEntry {` |
|    - |   99 | `	const char *zExt;` |
|    - |  100 | `	const char *zType;` |
|    - |  101 | `};` |
|    - |  102 | `static const MimeEntry aMime[] = {` |
|    - |  103 | `	{ ".html", "text/html" },` |
|    - |  104 | `	{ ".htm",  "text/html" },` |
|    - |  105 | `	{ ".css",  "text/css" },` |
|    - |  106 | `	{ ".js",   "application/javascript" },` |
|    - |  107 | `	{ ".json", "application/json" },` |
|    - |  108 | `	{ ".xml",  "application/xml" },` |
|    - |  109 | `	{ ".txt",  "text/plain" },` |
|    - |  110 | `	{ ".png",  "image/png" },` |
|    - |  111 | `	{ ".jpg",  "image/jpeg" },` |
|    - |  112 | `	{ ".jpeg", "image/jpeg" },` |
|    - |  113 | `	{ ".gif",  "image/gif" },` |
|    - |  114 | `	{ ".svg",  "image/svg+xml" },` |
|    - |  115 | `	{ ".ico",  "image/x-icon" },` |
|    - |  116 | `	{ ".pdf",  "application/pdf" },` |
|    - |  117 | `	{ ".woff", "font/woff" },` |
|    - |  118 | `	{ ".woff2","font/woff2" },` |
|    - |  119 | `	{ ".ttf",  "font/ttf" },` |
|    - |  120 | `	{ 0, 0 }` |
|    - |  121 | `};` |
|    4 |  122 | `static const char *GetMimeType(const char *zPath)` |
|  ! 0 |  123 | `{` |
|    - |  124 | `	const char *zDot;` |
|    - |  125 | `	int i;` |
|    4 |  126 | `	zDot = strrchr(zPath, '.');` |
|    4 |  127 | `	if( zDot == 0 ){` |
|  ! 0 |  128 | `		return "application/octet-stream";` |
|    - |  129 | `	}` |
|   28 |  130 | `	for( i = 0; aMime[i].zExt != 0; i++ ){` |
|   14 |  131 | `		if(` |
|    - |  132 | `#ifdef __WINNT__` |
|    - |  133 | `			_stricmp(zDot, aMime[i].zExt)` |
|    - |  134 | `#else` |
|   42 |  135 | `			strcasecmp(zDot, aMime[i].zExt)` |
|    - |  136 | `#endif` |
|   14 |  137 | `			== 0 ){` |
|    4 |  138 | `			return aMime[i].zType;` |
|    - |  139 | `		}` |
|   12 |  140 | `	}` |
|  ! 0 |  141 | `	return "application/octet-stream";` |
|    2 |  142 | `}` |
|    - |  143 | `/*` |
|    - |  144 | ` * Check if a path contains directory traversal sequences.` |
|    - |  145 | ` * Returns 1 if the path is safe, 0 if it contains "..".` |
|    - |  146 | ` */` |
|   30 |  147 | `static int IsPathSafe(const char *zPath)` |
|  ! 0 |  148 | `{` |
|   30 |  149 | `	const char *z = zPath;` |
|  300 |  150 | `	while( *z ){` |
|  270 |  151 | `		if( z[0] == '.' && z[1] == '.' && (z[2] == '/' \|\| z[2] == '\\' \|\| z[2] == 0) ){` |
|  ! 0 |  152 | `			return 0;` |
|    - |  153 | `		}` |
|  270 |  154 | `		z++;` |
|  ! 0 |  155 | `	}` |
|   30 |  156 | `	return 1;` |
|   15 |  157 | `}` |
|    - |  158 | `/*` |
|    - |  159 | ` * Check if a file exists and is a regular file.` |
|    - |  160 | ` * Returns 1 if it exists, 0 otherwise. Sets *pSize to the file size.` |
|    - |  161 | ` */` |
|   30 |  162 | `static int FileExists(const char *zPath, long *pSize)` |
|  ! 0 |  163 | `{` |
|    - |  164 | `#ifdef __WINNT__` |
|    - |  165 | `	WIN32_FILE_ATTRIBUTE_DATA info;` |
|  ! 0 |  166 | `	if( !GetFileAttributesExA(zPath, GetFileExInfoStandard, &info) ){` |
|  ! 0 |  167 | `		return 0;` |
|    - |  168 | `	}` |
|  ! 0 |  169 | `	if( info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){` |
|  ! 0 |  170 | `		return 0;` |
|    - |  171 | `	}` |
|  ! 0 |  172 | `	if( pSize ){` |
|  ! 0 |  173 | `		*pSize = (long)info.nFileSizeLow;` |
|    - |  174 | `	}` |
|  ! 0 |  175 | `	return 1;` |
|    - |  176 | `#else` |
|    - |  177 | `	struct stat st;` |
|   30 |  178 | `	if( stat(zPath, &st) != 0 ){` |
|    2 |  179 | `		return 0;` |
|    - |  180 | `	}` |
|   28 |  181 | `	if( !S_ISREG(st.st_mode) ){` |
|  ! 0 |  182 | `		return 0;` |
|    - |  183 | `	}` |
|   28 |  184 | `	if( pSize ){` |
|   28 |  185 | `		*pSize = (long)st.st_size;` |
|   14 |  186 | `	}` |
|   28 |  187 | `	return 1;` |
|    - |  188 | `#endif` |
|   15 |  189 | `}` |
|    - |  190 | `/*` |
|    - |  191 | ` * Check if a path is a directory.` |
|    - |  192 | ` */` |
|   30 |  193 | `static int IsDirectory(const char *zPath)` |
|  ! 0 |  194 | `{` |
|    - |  195 | `#ifdef __WINNT__` |
|  ! 0 |  196 | `	DWORD attr = GetFileAttributesA(zPath);` |
|  ! 0 |  197 | `	return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));` |
|    - |  198 | `#else` |
|    - |  199 | `	struct stat st;` |
|   30 |  200 | `	if( stat(zPath, &st) != 0 ){` |
|    2 |  201 | `		return 0;` |
|    - |  202 | `	}` |
|   28 |  203 | `	return S_ISDIR(st.st_mode);` |
|    - |  204 | `#endif` |
|   15 |  205 | `}` |
|    - |  206 | `/*` |
|    - |  207 | ` * Read the full HTTP request from a socket into a buffer.` |
|    - |  208 | ` * Reads headers first (until \r\n\r\n), then reads the body` |
|    - |  209 | ` * based on Content-Length if present.` |
|    - |  210 | ` * Returns total bytes read, or -1 on error.` |
|    - |  211 | ` */` |
|   30 |  212 | `static int ReadRequest(ph7_socket sock, char *zBuf, int nBufSize)` |
|  ! 0 |  213 | `{` |
|   30 |  214 | `	int nTotal = 0;` |
|    - |  215 | `	int nRead;` |
|    - |  216 | `	char *zHeaderEnd;` |
|    - |  217 | `	/* Read data until we find the end of headers */` |
|   30 |  218 | `	while( nTotal < nBufSize - 1 ){` |
|   30 |  219 | `		nRead = PH7_NetRecv(sock, zBuf + nTotal, nBufSize - 1 - nTotal, 0);` |
|   30 |  220 | `		if( nRead <= 0 ){` |
|  ! 0 |  221 | `			if( nTotal == 0 ){` |
|  ! 0 |  222 | `				return -1;` |
|    - |  223 | `			}` |
|  ! 0 |  224 | `			break;` |
|    - |  225 | `		}` |
|   30 |  226 | `		nTotal += nRead;` |
|   30 |  227 | `		zBuf[nTotal] = 0;` |
|    - |  228 | `		/* Check if we have the complete headers */` |
|   30 |  229 | `		zHeaderEnd = strstr(zBuf, "\r\n\r\n");` |
|   30 |  230 | `		if( zHeaderEnd ){` |
|   30 |  231 | `			int nHeaderLen = (int)(zHeaderEnd - zBuf) + 4;` |
|    - |  232 | `			/* Look for Content-Length */` |
|   30 |  233 | `			const char *zCL = strstr(zBuf, "Content-Length:");` |
|   30 |  234 | `			if( zCL == 0 ){` |
|   30 |  235 | `				zCL = strstr(zBuf, "content-length:");` |
|   15 |  236 | `			}` |
|   30 |  237 | `			if( zCL ){` |
|  ! 0 |  238 | `				int nContentLen = atoi(zCL + 15);` |
|  ! 0 |  239 | `				int nExpected = nHeaderLen + nContentLen;` |
|  ! 0 |  240 | `				if( nExpected > nBufSize - 1 ){` |
|  ! 0 |  241 | `					nExpected = nBufSize - 1;` |
|  ! 0 |  242 | `				}` |
|    - |  243 | `				/* Read remaining body bytes */` |
|  ! 0 |  244 | `				while( nTotal < nExpected ){` |
|  ! 0 |  245 | `					nRead = PH7_NetRecv(sock, zBuf + nTotal, nExpected - nTotal, 0);` |
|  ! 0 |  246 | `					if( nRead <= 0 ){` |
|  ! 0 |  247 | `						break;` |
|    - |  248 | `					}` |
|  ! 0 |  249 | `					nTotal += nRead;` |
|  ! 0 |  250 | `				}` |
|  ! 0 |  251 | `			}` |
|   30 |  252 | `			break;` |
|    - |  253 | `		}` |
|  ! 0 |  254 | `	}` |
|   30 |  255 | `	zBuf[nTotal] = 0;` |
|   30 |  256 | `	return nTotal;` |
|   15 |  257 | `}` |
|    - |  258 | `/*` |
|    - |  259 | ` * Send an HTTP response with headers.` |
|    - |  260 | ` */` |
|    2 |  261 | `static void SendResponse(ph7_socket sock, int iStatus, const char *zStatus,` |
|    - |  262 | `						 const char *zContentType, const void *pBody, int nBodyLen)` |
|  ! 0 |  263 | `{` |
|    - |  264 | `	char zHeader[512];` |
|    - |  265 | `	int nHeader;` |
|    2 |  266 | `	nHeader = snprintf(zHeader, sizeof(zHeader),` |
|    - |  267 | `		"HTTP/1.1 %d %s\r\n"` |
|    - |  268 | `		"Content-Type: %s\r\n"` |
|    - |  269 | `		"Content-Length: %d\r\n"` |
|    - |  270 | `		"Connection: close\r\n"` |
|    - |  271 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  272 | `		"\r\n",` |
|    - |  273 | `		iStatus, zStatus, zContentType, nBodyLen);` |
|    2 |  274 | `	PH7_NetSendAll(sock, zHeader, nHeader);` |
|    2 |  275 | `	if( pBody && nBodyLen > 0 ){` |
|    2 |  276 | `		PH7_NetSendAll(sock, pBody, nBodyLen);` |
|    1 |  277 | `	}` |
|    2 |  278 | `}` |
|    - |  279 | `/*` |
|    - |  280 | ` * Send an HTTP error response.` |
|    - |  281 | ` */` |
|    2 |  282 | `static void SendError(ph7_socket sock, int iStatus, const char *zStatus)` |
|  ! 0 |  283 | `{` |
|    - |  284 | `	char zBody[256];` |
|    - |  285 | `	int nBody;` |
|    2 |  286 | `	nBody = snprintf(zBody, sizeof(zBody),` |
|    - |  287 | `		"<html><head><title>%d %s</title></head>"` |
|    - |  288 | `		"<body><h1>%d %s</h1></body></html>",` |
|    - |  289 | `		iStatus, zStatus, iStatus, zStatus);` |
|    2 |  290 | `	SendResponse(sock, iStatus, zStatus, "text/html", zBody, nBody);` |
|    2 |  291 | `}` |
|    - |  292 | `/*` |
|    - |  293 | ` * Map a status code to a reason phrase.` |
|    - |  294 | ` */` |
|   24 |  295 | `static const char *StatusReason(int iStatus)` |
|  ! 0 |  296 | `{` |
|   24 |  297 | `	switch( iStatus ){` |
|   18 |  298 | `	case 200: return "OK";` |
|    4 |  299 | `	case 201: return "Created";` |
|  ! 0 |  300 | `	case 204: return "No Content";` |
|  ! 0 |  301 | `	case 301: return "Moved Permanently";` |
|  ! 0 |  302 | `	case 302: return "Found";` |
|  ! 0 |  303 | `	case 304: return "Not Modified";` |
|  ! 0 |  304 | `	case 400: return "Bad Request";` |
|  ! 0 |  305 | `	case 401: return "Unauthorized";` |
|    2 |  306 | `	case 403: return "Forbidden";` |
|  ! 0 |  307 | `	case 404: return "Not Found";` |
|  ! 0 |  308 | `	case 405: return "Method Not Allowed";` |
|  ! 0 |  309 | `	case 500: return "Internal Server Error";` |
|  ! 0 |  310 | `	case 502: return "Bad Gateway";` |
|  ! 0 |  311 | `	case 503: return "Service Unavailable";` |
|  ! 0 |  312 | `	default:  return "";` |
|    - |  313 | `	}` |
|   12 |  314 | `}` |
|    - |  315 | `/*` |
|    - |  316 | ` * Send an HTTP response using the VM's response headers and status code.` |
|    - |  317 | ` * Falls back to Content-Type: text/html if the script didn't set one.` |
|    - |  318 | ` */` |
|    - |  319 | `/*` |
|    - |  320 | ` * State passed to the response header callback.` |
|    - |  321 | ` */` |
|    - |  322 | `typedef struct VmResponseCtx VmResponseCtx;` |
|    - |  323 | `struct VmResponseCtx {` |
|    - |  324 | `	ph7_socket sock;` |
|    - |  325 | `	int bHasContentType;` |
|    - |  326 | `};` |
|   12 |  327 | `static int VmResponseHeaderCB(const char *zName, unsigned int nName,` |
|    - |  328 | `							   const char *zValue, unsigned int nValue,` |
|    - |  329 | `							   void *pUserData)` |
|  ! 0 |  330 | `{` |
|   12 |  331 | `	VmResponseCtx *pCtx = (VmResponseCtx *)pUserData;` |
|    - |  332 | `	char zLine[512];` |
|    - |  333 | `	int nLine;` |
|   12 |  334 | `	nLine = snprintf(zLine, sizeof(zLine), "%.*s: %.*s\r\n",` |
|    - |  335 | `		(int)nName, zName, (int)nValue, zValue);` |
|   12 |  336 | `	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);` |
|   12 |  337 | `	PH7_NetSendAll(pCtx->sock, zLine, nLine);` |
|   12 |  338 | `	if( nName == 12 && SyStrnicmp(zName, "Content-Type", 12) == 0 ){` |
|    2 |  339 | `		pCtx->bHasContentType = 1;` |
|    1 |  340 | `	}` |
|   12 |  341 | `	return PH7_OK;` |
|  ! 0 |  342 | `}` |
|   24 |  343 | `static void SendVmResponse(ph7_socket sock, ph7_vm *pVm,` |
|    - |  344 | `							const void *pBody, int nBodyLen)` |
|  ! 0 |  345 | `{` |
|    - |  346 | `	int iStatus;` |
|    - |  347 | `	VmResponseCtx sCtx;` |
|    - |  348 | `	char zLine[512];` |
|    - |  349 | `	int nLine;` |
|   24 |  350 | `	iStatus = 200;` |
|   24 |  351 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_STATUS, &iStatus);` |
|    - |  352 | `	/* Status line */` |
|   24 |  353 | `	nLine = snprintf(zLine, sizeof(zLine), "HTTP/1.1 %d %s\r\n", iStatus, StatusReason(iStatus));` |
|   24 |  354 | `	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);` |
|   24 |  355 | `	PH7_NetSendAll(sock, zLine, nLine);` |
|    - |  356 | `	/* Script-set headers via callback */` |
|   24 |  357 | `	sCtx.sock = sock;` |
|   24 |  358 | `	sCtx.bHasContentType = 0;` |
|   24 |  359 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_HEADERS, VmResponseHeaderCB, &sCtx);` |
|    - |  360 | `	/* Default Content-Type if not set by the script */` |
|   24 |  361 | `	if( !sCtx.bHasContentType ){` |
|   22 |  362 | `		PH7_NetSendAll(sock, "Content-Type: text/html\r\n", 25);` |
|   11 |  363 | `	}` |
|    - |  364 | `	/* Standard headers */` |
|   24 |  365 | `	nLine = snprintf(zLine, sizeof(zLine),` |
|    - |  366 | `		"Content-Length: %d\r\n"` |
|    - |  367 | `		"Connection: close\r\n"` |
|    - |  368 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  369 | `		"\r\n",` |
|    - |  370 | `		nBodyLen);` |
|   24 |  371 | `	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);` |
|   24 |  372 | `	PH7_NetSendAll(sock, zLine, nLine);` |
|    - |  373 | `	/* Body */` |
|   24 |  374 | `	if( pBody && nBodyLen > 0 ){` |
|   24 |  375 | `		PH7_NetSendAll(sock, pBody, nBodyLen);` |
|   12 |  376 | `	}` |
|   24 |  377 | `}` |
|    - |  378 | `/*` |
|    - |  379 | ` * Serve a static file.` |
|    - |  380 | ` */` |
|    4 |  381 | `static void ServeStaticFile(ph7_socket sock, const char *zPath, long nFileSize)` |
|  ! 0 |  382 | `{` |
|    - |  383 | `	const char *zMime;` |
|    - |  384 | `	char zHeader[512];` |
|    - |  385 | `	char zFileBuf[PHL_FILE_BUF];` |
|    - |  386 | `	FILE *pFile;` |
|    - |  387 | `	int nHeader;` |
|    - |  388 | `	size_t nRead;` |
|    4 |  389 | `	zMime = GetMimeType(zPath);` |
|    - |  390 | `#ifdef __WINNT__` |
|  ! 0 |  391 | `	if( fopen_s(&pFile, zPath, "rb") != 0 ) pFile = 0;` |
|    - |  392 | `#else` |
|    4 |  393 | `	pFile = fopen(zPath, "rb");` |
|    - |  394 | `#endif` |
|    4 |  395 | `	if( pFile == 0 ){` |
|  ! 0 |  396 | `		SendError(sock, 500, "Internal Server Error");` |
|  ! 0 |  397 | `		return;` |
|    - |  398 | `	}` |
|    4 |  399 | `	nHeader = snprintf(zHeader, sizeof(zHeader),` |
|    - |  400 | `		"HTTP/1.1 200 OK\r\n"` |
|    - |  401 | `		"Content-Type: %s\r\n"` |
|    - |  402 | `		"Content-Length: %ld\r\n"` |
|    - |  403 | `		"Connection: close\r\n"` |
|    - |  404 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  405 | `		"\r\n",` |
|    - |  406 | `		zMime, nFileSize);` |
|    4 |  407 | `	PH7_NetSendAll(sock, zHeader, nHeader);` |
|    8 |  408 | `	while( (nRead = fread(zFileBuf, 1, sizeof(zFileBuf), pFile)) > 0 ){` |
|    4 |  409 | `		if( PH7_NetSendAll(sock, zFileBuf, (int)nRead) != PH7_OK ){` |
|  ! 0 |  410 | `			break;` |
|    - |  411 | `		}` |
|  ! 0 |  412 | `	}` |
|    4 |  413 | `	fclose(pFile);` |
|    2 |  414 | `}` |
|    - |  415 | `/*` |
|    - |  416 | ` * Extract the request method and URI path from the first line of the HTTP request.` |
|    - |  417 | ` * E.g., "GET /foo/bar?q=1 HTTP/1.1\r\n..."` |
|    - |  418 | ` * Writes the method to zMethod (up to nMethodSize) and path to zPath (up to nPathSize).` |
|    - |  419 | ` * The path is the URI without the query string.` |
|    - |  420 | ` */` |
|   60 |  421 | `static void ExtractRequestLine(const char *zRequest, char *zMethod, int nMethodSize,` |
|    - |  422 | `							   char *zPath, int nPathSize)` |
|  ! 0 |  423 | `{` |
|   60 |  424 | `	const char *z = zRequest;` |
|    - |  425 | `	int i;` |
|    - |  426 | `	/* Extract method */` |
|  240 |  427 | `	for( i = 0; *z && *z != ' ' && i < nMethodSize - 1; z++, i++ ){` |
|  180 |  428 | `		zMethod[i] = *z;` |
|   90 |  429 | `	}` |
|   60 |  430 | `	zMethod[i] = 0;` |
|    - |  431 | `	/* Skip space */` |
|  120 |  432 | `	while( *z == ' ' ) z++;` |
|    - |  433 | `	/* Extract path (stop at '?', ' ', or end) */` |
|  600 |  434 | `	for( i = 0; *z && *z != '?' && *z != ' ' && *z != '\r' && i < nPathSize - 1; z++, i++ ){` |
|  540 |  435 | `		zPath[i] = *z;` |
|  270 |  436 | `	}` |
|   60 |  437 | `	zPath[i] = 0;` |
|   60 |  438 | `}` |
|    - |  439 | `/*` |
|    - |  440 | ` * Resolve a request URI path to a filesystem path under the document root.` |
|    - |  441 | ` * Returns 1 on success, 0 on failure (bad path).` |
|    - |  442 | ` * zOut must be at least PHL_MAX_PATH bytes.` |
|    - |  443 | ` */` |
|   30 |  444 | `static int ResolvePath(const char *zDocRoot, const char *zUri, char *zOut)` |
|  ! 0 |  445 | `{` |
|   30 |  446 | `	if( !IsPathSafe(zUri) ){` |
|  ! 0 |  447 | `		return 0;` |
|    - |  448 | `	}` |
|   30 |  449 | `	snprintf(zOut, PHL_MAX_PATH, "%s%s", zDocRoot, zUri);` |
|    - |  450 | `	/* If path points to a directory, try index.php */` |
|   30 |  451 | `	if( IsDirectory(zOut) ){` |
|  ! 0 |  452 | `		int n = (int)strlen(zOut);` |
|    - |  453 | `		/* Ensure trailing slash */` |
|  ! 0 |  454 | `		if( n > 0 && zOut[n-1] != '/' && zOut[n-1] != '\\' ){` |
|  ! 0 |  455 | `			if( n < PHL_MAX_PATH - 1 ){` |
|  ! 0 |  456 | `				zOut[n] = '/';` |
|  ! 0 |  457 | `				zOut[n+1] = 0;` |
|  ! 0 |  458 | `				n++;` |
|  ! 0 |  459 | `			}` |
|  ! 0 |  460 | `		}` |
|  ! 0 |  461 | `		snprintf(zOut + n, PHL_MAX_PATH - n, "index.php");` |
|  ! 0 |  462 | `	}` |
|   30 |  463 | `	return 1;` |
|   15 |  464 | `}` |
|    - |  465 | `/*` |
|    - |  466 | ` * Check if a filename ends with ".php" (case-insensitive).` |
|    - |  467 | ` */` |
|   28 |  468 | `static int IsPhpFile(const char *zPath)` |
|  ! 0 |  469 | `{` |
|   28 |  470 | `	int n = (int)strlen(zPath);` |
|   28 |  471 | `	if( n < 4 ){` |
|  ! 0 |  472 | `		return 0;` |
|    - |  473 | `	}` |
|   14 |  474 | `	return (` |
|    - |  475 | `#ifdef __WINNT__` |
|    - |  476 | `		_stricmp(zPath + n - 4, ".php")` |
|    - |  477 | `#else` |
|   28 |  478 | `		strcasecmp(zPath + n - 4, ".php")` |
|    - |  479 | `#endif` |
|   28 |  480 | `		== 0);` |
|   14 |  481 | `}` |
|    - |  482 | `/*` |
|    - |  483 | ` * Return a file's last-modification time as a comparable integer (0 on error).` |
|    - |  484 | ` * Used to invalidate the compiled-VM cache when a script is edited, preserving` |
|    - |  485 | ` * the dev-server expectation that reloading picks up source changes.` |
|    - |  486 | ` */` |
|   24 |  487 | `static long long GetFileMtime(const char *zPath)` |
|  ! 0 |  488 | `{` |
|    - |  489 | `#ifdef __WINNT__` |
|    - |  490 | `	WIN32_FILE_ATTRIBUTE_DATA info;` |
|  ! 0 |  491 | `	if( !GetFileAttributesExA(zPath, GetFileExInfoStandard, &info) ){` |
|  ! 0 |  492 | `		return 0;` |
|    - |  493 | `	}` |
|  ! 0 |  494 | `	return ((long long)info.ftLastWriteTime.dwHighDateTime << 32)` |
|    - |  495 | `		\| (long long)info.ftLastWriteTime.dwLowDateTime;` |
|    - |  496 | `#else` |
|    - |  497 | `	struct stat st;` |
|   24 |  498 | `	if( stat(zPath, &st) != 0 ){` |
|  ! 0 |  499 | `		return 0;` |
|    - |  500 | `	}` |
|   24 |  501 | `	return (long long)st.st_mtime;` |
|    - |  502 | `#endif` |
|   12 |  503 | `}` |
|    - |  504 | `/* ---- Compiled-VM reuse cache ------------------------------------------------` |
|    - |  505 | ` * Compilation dominates the per-request cost (the on-hardware profile measured` |
|    - |  506 | ` * 167 of 197 ms/request in compile). We therefore compile each script once and` |
|    - |  507 | ` * re-execute it per request, calling ph7_vm_reset() in between to clear all` |
|    - |  508 | ` * per-execution state (globals, superglobals, statics, output, ...). The cache` |
|    - |  509 | ` * is keyed by the resolved filesystem path and is safe without locking because` |
|    - |  510 | ` * the dev server is single-threaded. Set PHL_NO_REUSE=1 to fall back to the old` |
|    - |  511 | ` * compile-per-request behaviour (useful for diffing). */` |
|    - |  512 | `#define PHL_VM_CACHE_SIZE 16` |
|    - |  513 | `typedef struct PhlVmCacheEntry {` |
|    - |  514 | `	char zPath[PHL_MAX_PATH]; /* Resolved script path ("" = empty slot) */` |
|    - |  515 | `	ph7_vm *pVm;              /* Compiled, reusable VM */` |
|    - |  516 | `	unsigned long nUse;       /* Last-use clock for LRU eviction */` |
|    - |  517 | `	long long nMtime;         /* Source mtime when compiled (cache invalidation) */` |
|    - |  518 | `} PhlVmCacheEntry;` |
|    - |  519 | `static PhlVmCacheEntry g_vmCache[PHL_VM_CACHE_SIZE];` |
|    - |  520 | `static unsigned long g_vmCacheClock = 0;` |
|    - |  521 | `static int g_vmReuse = 1;` |
|    - |  522 |  |
|    - |  523 | `/*` |
|    - |  524 | ` * Acquire a ready-to-execute VM for zPath. On a cache hit the VM is reset and` |
|    - |  525 | ` * reused; on a miss it is compiled and cached (evicting the least-recently-used` |
|    - |  526 | ` * entry when full). Returns NULL on compile error. On success *pbCached tells` |
|    - |  527 | ` * the caller whether the VM is cache-owned (do NOT release it) or a throwaway` |
|    - |  528 | ` * (release after use, e.g. when reuse is disabled).` |
|    - |  529 | ` */` |
|   24 |  530 | `static ph7_vm *AcquireScriptVm(ph7 *pEngine, const char *zPath, int *pbCached)` |
|  ! 0 |  531 | `{` |
|   24 |  532 | `	int i, iFree = -1, iLru = -1;` |
|   24 |  533 | `	ph7_vm *pVm = 0;` |
|    - |  534 | `	long long nMtime;` |
|   24 |  535 | `	if( !g_vmReuse ){` |
|  ! 0 |  536 | `		*pbCached = 0;` |
|  ! 0 |  537 | `		if( ph7_compile_file(pEngine, zPath, &pVm, 0) != PH7_OK ){` |
|  ! 0 |  538 | `			return 0;` |
|    - |  539 | `		}` |
|  ! 0 |  540 | `		DefinePhpBinary(pVm);` |
|  ! 0 |  541 | `		return pVm;` |
|    - |  542 | `	}` |
|   24 |  543 | `	nMtime = GetFileMtime(zPath);` |
|  312 |  544 | `	for( i = 0 ; i < PHL_VM_CACHE_SIZE ; i++ ){` |
|  294 |  545 | `		if( g_vmCache[i].pVm == 0 ){` |
|  288 |  546 | `			if( iFree < 0 ){ iFree = i; }` |
|  288 |  547 | `			continue;` |
|    - |  548 | `		}` |
|    6 |  549 | `		if( strcmp(g_vmCache[i].zPath, zPath) == 0 ){` |
|    - |  550 | `			/* Hit: reuse only if the source is unchanged on disk (dev-server` |
|    - |  551 | `			 * live-edit) and the reset succeeds; otherwise drop and recompile. */` |
|    6 |  552 | `			if( g_vmCache[i].nMtime == nMtime` |
|    6 |  553 | `			 && ph7_vm_reset(g_vmCache[i].pVm) == PH7_OK ){` |
|    6 |  554 | `				g_vmCache[i].nUse = ++g_vmCacheClock;` |
|    6 |  555 | `				*pbCached = 1;` |
|    6 |  556 | `				return g_vmCache[i].pVm;` |
|    - |  557 | `			}` |
|  ! 0 |  558 | `			ph7_vm_release(g_vmCache[i].pVm);` |
|  ! 0 |  559 | `			g_vmCache[i].pVm = 0;` |
|  ! 0 |  560 | `			g_vmCache[i].zPath[0] = '\0';` |
|  ! 0 |  561 | `			iFree = i;` |
|  ! 0 |  562 | `			break;` |
|    - |  563 | `		}` |
|  ! 0 |  564 | `		if( iLru < 0 \|\| g_vmCache[i].nUse < g_vmCache[iLru].nUse ){` |
|  ! 0 |  565 | `			iLru = i;` |
|  ! 0 |  566 | `		}` |
|  ! 0 |  567 | `	}` |
|    - |  568 | `	/* Miss: compile a fresh VM. */` |
|   18 |  569 | `	if( ph7_compile_file(pEngine, zPath, &pVm, 0) != PH7_OK ){` |
|  ! 0 |  570 | `		*pbCached = 0;` |
|  ! 0 |  571 | `		return 0;` |
|    - |  572 | `	}` |
|   18 |  573 | `	DefinePhpBinary(pVm);` |
|   18 |  574 | `	if( iFree < 0 ){` |
|    - |  575 | `		/* Cache full: evict the least-recently-used entry. */` |
|  ! 0 |  576 | `		ph7_vm_release(g_vmCache[iLru].pVm);` |
|  ! 0 |  577 | `		iFree = iLru;` |
|  ! 0 |  578 | `	}` |
|   18 |  579 | `	snprintf(g_vmCache[iFree].zPath, sizeof(g_vmCache[iFree].zPath), "%s", zPath);` |
|   18 |  580 | `	g_vmCache[iFree].pVm = pVm;` |
|   18 |  581 | `	g_vmCache[iFree].nUse = ++g_vmCacheClock;` |
|   18 |  582 | `	g_vmCache[iFree].nMtime = nMtime;` |
|   18 |  583 | `	*pbCached = 1;` |
|   18 |  584 | `	return pVm;` |
|   12 |  585 | `}` |
|    - |  586 | `/*` |
|    - |  587 | ` * Release every cached VM. Called at server shutdown.` |
|    - |  588 | ` */` |
|   22 |  589 | `static void ReleaseVmCache(void)` |
|  ! 0 |  590 | `{` |
|    - |  591 | `	int i;` |
|  374 |  592 | `	for( i = 0 ; i < PHL_VM_CACHE_SIZE ; i++ ){` |
|  352 |  593 | `		if( g_vmCache[i].pVm ){` |
|   18 |  594 | `			ph7_vm_release(g_vmCache[i].pVm);` |
|   18 |  595 | `			g_vmCache[i].pVm = 0;` |
|   18 |  596 | `			g_vmCache[i].zPath[0] = '\0';` |
|    9 |  597 | `		}` |
|  176 |  598 | `	}` |
|   22 |  599 | `}` |
|    - |  600 | `/*` |
|    - |  601 | ` * Execute a PHP script and send its output as an HTTP response.` |
|    - |  602 | ` * pEngine is the shared engine instance.` |
|    - |  603 | ` * zFilePath is the resolved filesystem path to the PHP file.` |
|    - |  604 | ` * zRawRequest/nRequestLen is the raw HTTP request for superglobal population.` |
|    - |  605 | ` * The additional server attributes are set from the request context.` |
|    - |  606 | ` */` |
|   24 |  607 | `static void ExecutePhpScript(ph7 *pEngine, ph7_socket client,` |
|    - |  608 | `							 const char *zFilePath, const char *zScriptName,` |
|    - |  609 | `							 const char *zRawRequest, int nRequestLen,` |
|    - |  610 | `							 const char *zHost, int iPort, const char *zDocRoot,` |
|    - |  611 | `							 const char *zRemoteAddr, int iRemotePort)` |
|  ! 0 |  612 | `{` |
|   24 |  613 | `	ph7_vm *pVm = 0;` |
|    - |  614 | `	const void *pOutput;` |
|    - |  615 | `	unsigned int nOutputLen;` |
|    - |  616 | `	char zPortBuf[16];` |
|    - |  617 | `	char zRemotePortBuf[16];` |
|   24 |  618 | `	int bCached = 0;` |
|   24 |  619 | `	pVm = AcquireScriptVm(pEngine, zFilePath, &bCached);` |
|   24 |  620 | `	if( pVm == 0 ){` |
|  ! 0 |  621 | `		SendError(client, 500, "Internal Server Error");` |
|  ! 0 |  622 | `		return;` |
|    - |  623 | `	}` |
|    - |  624 | `	/* Feed the raw HTTP request to populate $_SERVER, $_GET, $_POST, etc. */` |
|   24 |  625 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);` |
|    - |  626 | `	/* Set additional $_SERVER attributes */` |
|   24 |  627 | `	snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);` |
|   24 |  628 | `	snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);` |
|   24 |  629 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);` |
|   24 |  630 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);` |
|   24 |  631 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);` |
|   24 |  632 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);` |
|   24 |  633 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zFilePath, -1);` |
|   24 |  634 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zScriptName, -1);` |
|   24 |  635 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);` |
|   24 |  636 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);` |
|   24 |  637 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);` |
|    - |  638 | `	/* Execute the script (output accumulates in the VM's internal buffer) */` |
|   24 |  639 | `	ph7_vm_exec(pVm, 0);` |
|    - |  640 | `	/* Extract accumulated output */` |
|   24 |  641 | `	pOutput = 0;` |
|   24 |  642 | `	nOutputLen = 0;` |
|   24 |  643 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);` |
|    - |  644 | `	/* Send the response using VM-set headers and status code */` |
|   24 |  645 | `	SendVmResponse(client, pVm, pOutput, (int)nOutputLen);` |
|    - |  646 | `	/* Cache-owned VMs are kept for reuse; throwaways are released. */` |
|   24 |  647 | `	if( !bCached ){` |
|  ! 0 |  648 | `		ph7_vm_release(pVm);` |
|  ! 0 |  649 | `	}` |
|   12 |  650 | `}` |
|    - |  651 | `/*` |
|    - |  652 | ` * Handle a single HTTP request.` |
|    - |  653 | ` */` |
|   30 |  654 | `static int HandleRequest(ph7 *pEngine, ph7_socket client,` |
|    - |  655 | `						  const char *zDocRoot, const char *zRouter,` |
|    - |  656 | `						  const char *zHost, int iPort,` |
|    - |  657 | `						  const char *zRawRequest, int nRequestLen,` |
|    - |  658 | `						  const char *zRemoteAddr, int iRemotePort)` |
|  ! 0 |  659 | `{` |
|    - |  660 | `	char zMethod[16];` |
|    - |  661 | `	char zUri[PHL_MAX_PATH];` |
|    - |  662 | `	char zFilePath[PHL_MAX_PATH];` |
|    - |  663 | `	long nFileSize;` |
|    - |  664 | `	/* Extract request line */` |
|   30 |  665 | `	ExtractRequestLine(zRawRequest, zMethod, sizeof(zMethod), zUri, sizeof(zUri));` |
|   15 |  666 | `	(void)zMethod;` |
|    - |  667 | `	/* Try router script first (if configured) */` |
|   30 |  668 | `	if( zRouter && zRouter[0] ){` |
|  ! 0 |  669 | `		ph7_vm *pVm = 0;` |
|    - |  670 | `		ph7_value *pRetVal;` |
|  ! 0 |  671 | `		int bCached = 0;` |
|    - |  672 | `		char zPortBuf[16];` |
|    - |  673 | `		char zRemotePortBuf[16];` |
|  ! 0 |  674 | `		pVm = AcquireScriptVm(pEngine, zRouter, &bCached);` |
|  ! 0 |  675 | `		if( pVm != 0 ){` |
|  ! 0 |  676 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);` |
|  ! 0 |  677 | `			snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);` |
|  ! 0 |  678 | `			snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);` |
|  ! 0 |  679 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);` |
|  ! 0 |  680 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);` |
|  ! 0 |  681 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);` |
|  ! 0 |  682 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);` |
|  ! 0 |  683 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zRouter, -1);` |
|  ! 0 |  684 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zUri, -1);` |
|  ! 0 |  685 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);` |
|  ! 0 |  686 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);` |
|  ! 0 |  687 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);` |
|  ! 0 |  688 | `			ph7_vm_exec(pVm, 0);` |
|    - |  689 | `			/* Check if the router returned false (meaning: fall through to default serving) */` |
|  ! 0 |  690 | `			pRetVal = 0;` |
|  ! 0 |  691 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_EXEC_VALUE, &pRetVal);` |
|  ! 0 |  692 | `			if( pRetVal && ph7_value_is_bool(pRetVal) && !ph7_value_to_bool(pRetVal) ){` |
|    - |  693 | `				/* Router returned false: fall through to default file serving */` |
|  ! 0 |  694 | `				if( !bCached ){` |
|  ! 0 |  695 | `					ph7_vm_release(pVm);` |
|  ! 0 |  696 | `				}` |
|  ! 0 |  697 | `			}else{` |
|    - |  698 | `				/* Router handled the request: send its output */` |
|    - |  699 | `				const void *pOutput;` |
|    - |  700 | `				unsigned int nOutputLen;` |
|  ! 0 |  701 | `				pOutput = 0;` |
|  ! 0 |  702 | `				nOutputLen = 0;` |
|  ! 0 |  703 | `				ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);` |
|  ! 0 |  704 | `				SendVmResponse(client, pVm, pOutput, (int)nOutputLen);` |
|  ! 0 |  705 | `				if( !bCached ){` |
|  ! 0 |  706 | `					ph7_vm_release(pVm);` |
|  ! 0 |  707 | `				}` |
|  ! 0 |  708 | `				return 200;` |
|    - |  709 | `			}` |
|  ! 0 |  710 | `		}` |
|    - |  711 | `		/* Router compile failed or returned false: fall through */` |
|  ! 0 |  712 | `	}` |
|    - |  713 | `	/* Resolve URI to filesystem path */` |
|   30 |  714 | `	if( !ResolvePath(zDocRoot, zUri, zFilePath) ){` |
|  ! 0 |  715 | `		SendError(client, 403, "Forbidden");` |
|  ! 0 |  716 | `		return 403;` |
|    - |  717 | `	}` |
|    - |  718 | `	/* Check if file exists */` |
|   30 |  719 | `	if( FileExists(zFilePath, &nFileSize) ){` |
|   28 |  720 | `		if( IsPhpFile(zFilePath) ){` |
|   36 |  721 | `			ExecutePhpScript(pEngine, client, zFilePath, zUri,` |
|   12 |  722 | `							 zRawRequest, nRequestLen,` |
|   12 |  723 | `							 zHost, iPort, zDocRoot,` |
|   12 |  724 | `							 zRemoteAddr, iRemotePort);` |
|   12 |  725 | `		}else{` |
|    4 |  726 | `			ServeStaticFile(client, zFilePath, nFileSize);` |
|    - |  727 | `		}` |
|   28 |  728 | `		return 200;` |
|    - |  729 | `	}` |
|    2 |  730 | `	SendError(client, 404, "Not Found");` |
|    2 |  731 | `	return 404;` |
|   15 |  732 | `}` |
|    - |  733 | `/*` |
|    - |  734 | ` * Log a request line to stderr.` |
|    - |  735 | ` */` |
|   30 |  736 | `static void LogRequest(const char *zRemoteAddr, int iRemotePort,` |
|    - |  737 | `					   const char *zMethod, const char *zUri, int iStatus)` |
|  ! 0 |  738 | `{` |
|    - |  739 | `	time_t now;` |
|    - |  740 | `	struct tm tm_buf;` |
|    - |  741 | `	char zTime[64];` |
|   30 |  742 | `	time(&now);` |
|    - |  743 | `#ifdef __WINNT__` |
|  ! 0 |  744 | `	localtime_s(&tm_buf, &now);` |
|    - |  745 | `#else` |
|   30 |  746 | `	localtime_r(&now, &tm_buf);` |
|    - |  747 | `#endif` |
|   30 |  748 | `	strftime(zTime, sizeof(zTime), "%a %b %d %H:%M:%S %Y", &tm_buf);` |
|   45 |  749 | `	fprintf(stderr, "[%s] %s:%d [%d]: %s %s\n",` |
|   15 |  750 | `		zTime, zRemoteAddr, iRemotePort, iStatus, zMethod, zUri);` |
|   30 |  751 | `}` |
|    - |  752 | `/*` |
|    - |  753 | ` * Main server entry point.` |
|    - |  754 | ` */` |
|   22 |  755 | `int phl_serve(const char *zHost, int iPort, const char *zDocRoot, const char *zRouter, const char *zBinaryPath)` |
|  ! 0 |  756 | `{` |
|    - |  757 | `	ph7 *pEngine;` |
|    - |  758 | `	ph7_socket listenSock;` |
|    - |  759 | `	ph7_socket clientSock;` |
|    - |  760 | `	struct sockaddr_in clientAddr;` |
|    - |  761 | `	ph7_socklen addrLen;` |
|    - |  762 | `	char *zRequestBuf;` |
|    - |  763 | `	int nRequestLen;` |
|    - |  764 | `	char zRemoteAddr[64];` |
|    - |  765 | `	int iRemotePort;` |
|    - |  766 | `	int rc;` |
|    - |  767 | `	/* Resolved interpreter path → PHP_BINARY for every served VM. */` |
|   22 |  768 | `	g_phpBinaryPath = zBinaryPath;` |
|    - |  769 | `	/* Compile-once / reuse is on by default; PHL_NO_REUSE=1 forces the legacy` |
|    - |  770 | `	 * compile-per-request path for behaviour diffing. */` |
|    - |  771 | `	{` |
|   22 |  772 | `		const char *zNoReuse = getenv("PHL_NO_REUSE");` |
|   22 |  773 | `		if( zNoReuse && zNoReuse[0] && zNoReuse[0] != '0' ){` |
|  ! 0 |  774 | `			g_vmReuse = 0;` |
|  ! 0 |  775 | `		}` |
|    - |  776 | `	}` |
|    - |  777 | `	/* Initialize networking */` |
|   22 |  778 | `	rc = PH7_NetInit();` |
|   22 |  779 | `	if( rc != PH7_OK ){` |
|  ! 0 |  780 | `		fprintf(stderr, "Error: Failed to initialize networking\n");` |
|  ! 0 |  781 | `		return 1;` |
|    - |  782 | `	}` |
|    - |  783 | `	/* Create the PH7 engine (shared across requests) */` |
|   22 |  784 | `	rc = ph7_init(&pEngine);` |
|   22 |  785 | `	if( rc != PH7_OK ){` |
|  ! 0 |  786 | `		fprintf(stderr, "Error: Failed to initialize PH7 engine\n");` |
|  ! 0 |  787 | `		PH7_NetCleanup();` |
|  ! 0 |  788 | `		return 1;` |
|    - |  789 | `	}` |
|    - |  790 | `	/* Create listening socket */` |
|   22 |  791 | `	listenSock = PH7_NetListen(zHost, iPort, 10);` |
|   22 |  792 | `	if( listenSock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  793 | `		fprintf(stderr, "Error: Failed to listen on %s:%d\n", zHost, iPort);` |
|  ! 0 |  794 | `		ph7_release(pEngine);` |
|  ! 0 |  795 | `		PH7_NetCleanup();` |
|  ! 0 |  796 | `		return 1;` |
|    - |  797 | `	}` |
|    - |  798 | `	/* Install signal handlers */` |
|   22 |  799 | `	InstallSignalHandlers();` |
|    - |  800 | `	/* Print banner */` |
|   22 |  801 | `	fprintf(stderr, "PHL %s Development Server started at http://%s:%d/\n", PH7_VERSION, zHost, iPort);` |
|   22 |  802 | `	fprintf(stderr, "Document root: %s\n", zDocRoot);` |
|   22 |  803 | `	if( zRouter ){` |
|  ! 0 |  804 | `		fprintf(stderr, "Router script: %s\n", zRouter);` |
|  ! 0 |  805 | `	}` |
|   22 |  806 | `	fprintf(stderr, "Press Ctrl+C to stop.\n");` |
|    - |  807 | `	/* Allocate request buffer */` |
|   22 |  808 | `	zRequestBuf = (char *)malloc(PHL_MAX_REQUEST);` |
|   22 |  809 | `	if( zRequestBuf == 0 ){` |
|  ! 0 |  810 | `		fprintf(stderr, "Error: Out of memory\n");` |
|  ! 0 |  811 | `		PH7_NetClose(listenSock);` |
|  ! 0 |  812 | `		ph7_release(pEngine);` |
|  ! 0 |  813 | `		PH7_NetCleanup();` |
|  ! 0 |  814 | `		return 1;` |
|    - |  815 | `	}` |
|    - |  816 | `	/* Accept loop */` |
|   52 |  817 | `	while( !g_shutdown ){` |
|   52 |  818 | `		addrLen = sizeof(clientAddr);` |
|   52 |  819 | `		clientSock = PH7_NetAccept(listenSock, (struct sockaddr *)&clientAddr, &addrLen);` |
|   52 |  820 | `		if( clientSock == PH7_NET_INVALID_SOCKET ){` |
|   22 |  821 | `			if( g_shutdown ){` |
|   22 |  822 | `				break;` |
|    - |  823 | `			}` |
|  ! 0 |  824 | `			continue;` |
|    - |  825 | `		}` |
|    - |  826 | `		/* Set a receive timeout so we don't block forever on a slow client */` |
|   30 |  827 | `		PH7_NetSetTimeout(clientSock, 5000);` |
|    - |  828 | `		/* Read the request */` |
|   30 |  829 | `		nRequestLen = ReadRequest(clientSock, zRequestBuf, PHL_MAX_REQUEST);` |
|   30 |  830 | `		if( nRequestLen > 0 ){` |
|    - |  831 | `			char zMethod[16];` |
|    - |  832 | `			char zUri[PHL_MAX_PATH];` |
|    - |  833 | `			/* Extract method and URI for logging */` |
|   30 |  834 | `			ExtractRequestLine(zRequestBuf, zMethod, sizeof(zMethod), zUri, sizeof(zUri));` |
|    - |  835 | `			/* Get client address info */` |
|   30 |  836 | `			PH7_NetAddrToString((struct sockaddr *)&clientAddr, zRemoteAddr, sizeof(zRemoteAddr));` |
|   30 |  837 | `			iRemotePort = PH7_NetAddrPort((struct sockaddr *)&clientAddr);` |
|    - |  838 | `			/* Handle the request */` |
|   45 |  839 | `			rc = HandleRequest(pEngine, clientSock, zDocRoot, zRouter,` |
|   15 |  840 | `						  zHost, iPort, zRequestBuf, nRequestLen,` |
|   15 |  841 | `						  zRemoteAddr, iRemotePort);` |
|    - |  842 | `			/* Log it */` |
|   30 |  843 | `			LogRequest(zRemoteAddr, iRemotePort, zMethod, zUri, rc);` |
|   15 |  844 | `		}` |
|   30 |  845 | `		PH7_NetClose(clientSock);` |
|  ! 0 |  846 | `	}` |
|    - |  847 | `	/* Cleanup */` |
|   22 |  848 | `	fprintf(stderr, "\nShutting down...\n");` |
|   22 |  849 | `	ReleaseVmCache();` |
|   22 |  850 | `	free(zRequestBuf);` |
|   22 |  851 | `	PH7_NetClose(listenSock);` |
|   22 |  852 | `	ph7_release(pEngine);` |
|   22 |  853 | `	PH7_NetCleanup();` |
|   22 |  854 | `	return 0;` |
|   11 |  855 | `}` |
|    - |  856 |  |
|    - |  857 | `#endif /* PHL_ENABLE_SERVER */` |
|    - |  858 |  |
