# src/phl/server.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 299/476 lines (62.82%)

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
|    - |   47 | `#ifndef __WINNT__` |
|    - |   48 | `/* Pre-forked worker pids (parent only) — signalled on shutdown so killing` |
|    - |   49 | ` * the parent doesn't orphan workers holding the port */` |
|    - |   50 | `static pid_t g_aWorkerPid[64];` |
|    - |   51 | `static int g_nWorkerPid = 0;` |
|    - |   52 | `#endif` |
|    - |   53 |  |
|    - |   54 | `/* Resolved interpreter path, exposed to scripts as the PHP_BINARY constant.` |
|    - |   55 | ` * Set once in phl_serve(); the matching expand callback mirrors the CLI's` |
|    - |   56 | ` * (PHL_PhpBinaryConst is static to phl.c, so the server keeps its own). */` |
|    - |   57 | `static const char *g_phpBinaryPath = 0;` |
|    4 |   58 | `static void PhlServerPhpBinaryConst(ph7_value *pVal, void *pUserData)` |
|  ! 0 |   59 | `{` |
|    4 |   60 | `	ph7_value_string(pVal, (const char *)pUserData, -1);` |
|    4 |   61 | `}` |
|    - |   62 | `/* Define PHP_BINARY on a freshly compiled VM. Constants persist across` |
|    - |   63 | ` * ph7_vm_reset(), so a cache-owned VM needs this only once, at compile time. */` |
|   24 |   64 | `static void DefinePhpBinary(ph7_vm *pVm)` |
|  ! 0 |   65 | `{` |
|   24 |   66 | `	if( g_phpBinaryPath ){` |
|   24 |   67 | `		ph7_create_constant(pVm, "PHP_BINARY", PhlServerPhpBinaryConst, (void *)g_phpBinaryPath);` |
|   12 |   68 | `	}` |
|   24 |   69 | `}` |
|    - |   70 |  |
|    - |   71 | `#ifdef __WINNT__` |
|    - |   72 | `static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)` |
|  ! 0 |   73 | `{` |
|    - |   74 | `	(void)dwCtrlType;` |
|  ! 0 |   75 | `	g_shutdown = 1;` |
|  ! 0 |   76 | `	return TRUE;` |
|  ! 0 |   77 | `}` |
|    - |   78 | `#else` |
|   32 |   79 | `static void SignalHandler(int sig)` |
|    - |   80 | `{` |
|   16 |   81 | `	(void)sig;` |
|   32 |   82 | `	g_shutdown = 1;` |
|   32 |   83 | `}` |
|    - |   84 | `#endif` |
|    - |   85 | `/*` |
|    - |   86 | ` * Install signal handlers for graceful shutdown.` |
|    - |   87 | ` */` |
|   26 |   88 | `static void InstallSignalHandlers(void)` |
|  ! 0 |   89 | `{` |
|    - |   90 | `#ifdef __WINNT__` |
|  ! 0 |   91 | `	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);` |
|    - |   92 | `#else` |
|    - |   93 | `	struct sigaction sa;` |
|   26 |   94 | `	memset(&sa, 0, sizeof(sa));` |
|   26 |   95 | `	sa.sa_handler = SignalHandler;` |
|   26 |   96 | `	sigaction(SIGINT, &sa, 0);` |
|   26 |   97 | `	sigaction(SIGTERM, &sa, 0);` |
|    - |   98 | `#endif` |
|   26 |   99 | `}` |
|    - |  100 | `/*` |
|    - |  101 | ` * MIME type lookup table.` |
|    - |  102 | ` */` |
|    - |  103 | `typedef struct MimeEntry MimeEntry;` |
|    - |  104 | `struct MimeEntry {` |
|    - |  105 | `	const char *zExt;` |
|    - |  106 | `	const char *zType;` |
|    - |  107 | `};` |
|    - |  108 | `static const MimeEntry aMime[] = {` |
|    - |  109 | `	{ ".html", "text/html" },` |
|    - |  110 | `	{ ".htm",  "text/html" },` |
|    - |  111 | `	{ ".css",  "text/css" },` |
|    - |  112 | `	{ ".js",   "application/javascript" },` |
|    - |  113 | `	{ ".json", "application/json" },` |
|    - |  114 | `	{ ".xml",  "application/xml" },` |
|    - |  115 | `	{ ".txt",  "text/plain" },` |
|    - |  116 | `	{ ".png",  "image/png" },` |
|    - |  117 | `	{ ".jpg",  "image/jpeg" },` |
|    - |  118 | `	{ ".jpeg", "image/jpeg" },` |
|    - |  119 | `	{ ".gif",  "image/gif" },` |
|    - |  120 | `	{ ".svg",  "image/svg+xml" },` |
|    - |  121 | `	{ ".ico",  "image/x-icon" },` |
|    - |  122 | `	{ ".pdf",  "application/pdf" },` |
|    - |  123 | `	{ ".woff", "font/woff" },` |
|    - |  124 | `	{ ".woff2","font/woff2" },` |
|    - |  125 | `	{ ".ttf",  "font/ttf" },` |
|    - |  126 | `	{ 0, 0 }` |
|    - |  127 | `};` |
|    4 |  128 | `static const char *GetMimeType(const char *zPath)` |
|  ! 0 |  129 | `{` |
|    - |  130 | `	const char *zDot;` |
|    - |  131 | `	int i;` |
|    4 |  132 | `	zDot = strrchr(zPath, '.');` |
|    4 |  133 | `	if( zDot == 0 ){` |
|  ! 0 |  134 | `		return "application/octet-stream";` |
|    - |  135 | `	}` |
|   28 |  136 | `	for( i = 0; aMime[i].zExt != 0; i++ ){` |
|   14 |  137 | `		if(` |
|    - |  138 | `#ifdef __WINNT__` |
|    - |  139 | `			_stricmp(zDot, aMime[i].zExt)` |
|    - |  140 | `#else` |
|   28 |  141 | `			strcasecmp(zDot, aMime[i].zExt)` |
|    - |  142 | `#endif` |
|   14 |  143 | `			== 0 ){` |
|    4 |  144 | `			return aMime[i].zType;` |
|    - |  145 | `		}` |
|   12 |  146 | `	}` |
|  ! 0 |  147 | `	return "application/octet-stream";` |
|    2 |  148 | `}` |
|    - |  149 | `/*` |
|    - |  150 | ` * Check if a path contains directory traversal sequences.` |
|    - |  151 | ` * Returns 1 if the path is safe, 0 if it contains "..".` |
|    - |  152 | ` */` |
|   38 |  153 | `static int IsPathSafe(const char *zPath)` |
|  ! 0 |  154 | `{` |
|   38 |  155 | `	const char *z = zPath;` |
|  368 |  156 | `	while( *z ){` |
|  330 |  157 | `		if( z[0] == '.' && z[1] == '.' && (z[2] == '/' \|\| z[2] == '\\' \|\| z[2] == 0) ){` |
|  ! 0 |  158 | `			return 0;` |
|    - |  159 | `		}` |
|  330 |  160 | `		z++;` |
|  ! 0 |  161 | `	}` |
|   38 |  162 | `	return 1;` |
|   19 |  163 | `}` |
|    - |  164 | `/*` |
|    - |  165 | ` * Check if a file exists and is a regular file.` |
|    - |  166 | ` * Returns 1 if it exists, 0 otherwise. Sets *pSize to the file size.` |
|    - |  167 | ` */` |
|   38 |  168 | `static int FileExists(const char *zPath, long *pSize)` |
|  ! 0 |  169 | `{` |
|    - |  170 | `#ifdef __WINNT__` |
|    - |  171 | `	WIN32_FILE_ATTRIBUTE_DATA info;` |
|  ! 0 |  172 | `	if( !GetFileAttributesExA(zPath, GetFileExInfoStandard, &info) ){` |
|  ! 0 |  173 | `		return 0;` |
|    - |  174 | `	}` |
|  ! 0 |  175 | `	if( info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){` |
|  ! 0 |  176 | `		return 0;` |
|    - |  177 | `	}` |
|  ! 0 |  178 | `	if( pSize ){` |
|  ! 0 |  179 | `		*pSize = (long)info.nFileSizeLow;` |
|    - |  180 | `	}` |
|  ! 0 |  181 | `	return 1;` |
|    - |  182 | `#else` |
|    - |  183 | `	struct stat st;` |
|   38 |  184 | `	if( stat(zPath, &st) != 0 ){` |
|    2 |  185 | `		return 0;` |
|    - |  186 | `	}` |
|   36 |  187 | `	if( !S_ISREG(st.st_mode) ){` |
|  ! 0 |  188 | `		return 0;` |
|    - |  189 | `	}` |
|   36 |  190 | `	if( pSize ){` |
|   36 |  191 | `		*pSize = (long)st.st_size;` |
|   18 |  192 | `	}` |
|   36 |  193 | `	return 1;` |
|    - |  194 | `#endif` |
|   19 |  195 | `}` |
|    - |  196 | `/*` |
|    - |  197 | ` * Check if a path is a directory.` |
|    - |  198 | ` */` |
|   38 |  199 | `static int IsDirectory(const char *zPath)` |
|  ! 0 |  200 | `{` |
|    - |  201 | `#ifdef __WINNT__` |
|  ! 0 |  202 | `	DWORD attr = GetFileAttributesA(zPath);` |
|  ! 0 |  203 | `	return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));` |
|    - |  204 | `#else` |
|    - |  205 | `	struct stat st;` |
|   38 |  206 | `	if( stat(zPath, &st) != 0 ){` |
|    2 |  207 | `		return 0;` |
|    - |  208 | `	}` |
|   36 |  209 | `	return S_ISDIR(st.st_mode);` |
|    - |  210 | `#endif` |
|   19 |  211 | `}` |
|    - |  212 | `/*` |
|    - |  213 | ` * Read the full HTTP request from a socket into a buffer.` |
|    - |  214 | ` * Reads headers first (until \r\n\r\n), then reads the body` |
|    - |  215 | ` * based on Content-Length if present.` |
|    - |  216 | ` * Returns total bytes read, or -1 on error.` |
|    - |  217 | ` */` |
|   38 |  218 | `static int ReadRequest(ph7_socket sock, char *zBuf, int nBufSize)` |
|  ! 0 |  219 | `{` |
|   38 |  220 | `	int nTotal = 0;` |
|    - |  221 | `	int nRead;` |
|    - |  222 | `	char *zHeaderEnd;` |
|    - |  223 | `	/* Read data until we find the end of headers */` |
|   38 |  224 | `	while( nTotal < nBufSize - 1 ){` |
|   38 |  225 | `		nRead = PH7_NetRecv(sock, zBuf + nTotal, nBufSize - 1 - nTotal, 0);` |
|   38 |  226 | `		if( nRead <= 0 ){` |
|  ! 0 |  227 | `			if( nTotal == 0 ){` |
|  ! 0 |  228 | `				return -1;` |
|    - |  229 | `			}` |
|  ! 0 |  230 | `			break;` |
|    - |  231 | `		}` |
|   38 |  232 | `		nTotal += nRead;` |
|   38 |  233 | `		zBuf[nTotal] = 0;` |
|    - |  234 | `		/* Check if we have the complete headers */` |
|   38 |  235 | `		zHeaderEnd = strstr(zBuf, "\r\n\r\n");` |
|   38 |  236 | `		if( zHeaderEnd ){` |
|   38 |  237 | `			int nHeaderLen = (int)(zHeaderEnd - zBuf) + 4;` |
|    - |  238 | `			/* Look for Content-Length */` |
|   38 |  239 | `			const char *zCL = strstr(zBuf, "Content-Length:");` |
|   38 |  240 | `			if( zCL == 0 ){` |
|   38 |  241 | `				zCL = strstr(zBuf, "content-length:");` |
|   19 |  242 | `			}` |
|   57 |  243 | `			if( zCL ){` |
|  ! 0 |  244 | `				int nContentLen = atoi(zCL + 15);` |
|  ! 0 |  245 | `				int nExpected = nHeaderLen + nContentLen;` |
|  ! 0 |  246 | `				if( nExpected > nBufSize - 1 ){` |
|  ! 0 |  247 | `					nExpected = nBufSize - 1;` |
|  ! 0 |  248 | `				}` |
|    - |  249 | `				/* Read remaining body bytes */` |
|  ! 0 |  250 | `				while( nTotal < nExpected ){` |
|  ! 0 |  251 | `					nRead = PH7_NetRecv(sock, zBuf + nTotal, nExpected - nTotal, 0);` |
|  ! 0 |  252 | `					if( nRead <= 0 ){` |
|  ! 0 |  253 | `						break;` |
|    - |  254 | `					}` |
|  ! 0 |  255 | `					nTotal += nRead;` |
|  ! 0 |  256 | `				}` |
|  ! 0 |  257 | `			}` |
|   38 |  258 | `			break;` |
|    - |  259 | `		}` |
|  ! 0 |  260 | `	}` |
|   38 |  261 | `	zBuf[nTotal] = 0;` |
|   38 |  262 | `	return nTotal;` |
|   19 |  263 | `}` |
|    - |  264 | `/*` |
|    - |  265 | ` * Send an HTTP response with headers.` |
|    - |  266 | ` */` |
|    2 |  267 | `static void SendResponse(ph7_socket sock, int iStatus, const char *zStatus,` |
|    - |  268 | `						 const char *zContentType, const void *pBody, int nBodyLen)` |
|  ! 0 |  269 | `{` |
|    - |  270 | `	char zHeader[512];` |
|    - |  271 | `	int nHeader;` |
|    2 |  272 | `	nHeader = snprintf(zHeader, sizeof(zHeader),` |
|    - |  273 | `		"HTTP/1.1 %d %s\r\n"` |
|    - |  274 | `		"Content-Type: %s\r\n"` |
|    - |  275 | `		"Content-Length: %d\r\n"` |
|    - |  276 | `		"Connection: close\r\n"` |
|    - |  277 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  278 | `		"\r\n",` |
|    - |  279 | `		iStatus, zStatus, zContentType, nBodyLen);` |
|    2 |  280 | `	PH7_NetSendAll(sock, zHeader, nHeader);` |
|    2 |  281 | `	if( pBody && nBodyLen > 0 ){` |
|    2 |  282 | `		PH7_NetSendAll(sock, pBody, nBodyLen);` |
|    1 |  283 | `	}` |
|    2 |  284 | `}` |
|    - |  285 | `/*` |
|    - |  286 | ` * Send an HTTP error response.` |
|    - |  287 | ` */` |
|    2 |  288 | `static void SendError(ph7_socket sock, int iStatus, const char *zStatus)` |
|  ! 0 |  289 | `{` |
|    - |  290 | `	char zBody[256];` |
|    - |  291 | `	int nBody;` |
|    2 |  292 | `	nBody = snprintf(zBody, sizeof(zBody),` |
|    - |  293 | `		"<html><head><title>%d %s</title></head>"` |
|    - |  294 | `		"<body><h1>%d %s</h1></body></html>",` |
|    - |  295 | `		iStatus, zStatus, iStatus, zStatus);` |
|    2 |  296 | `	SendResponse(sock, iStatus, zStatus, "text/html", zBody, nBody);` |
|    2 |  297 | `}` |
|    - |  298 | `/*` |
|    - |  299 | ` * Map a status code to a reason phrase.` |
|    - |  300 | ` */` |
|   32 |  301 | `static const char *StatusReason(int iStatus)` |
|  ! 0 |  302 | `{` |
|   32 |  303 | `	switch( iStatus ){` |
|   26 |  304 | `	case 200: return "OK";` |
|    4 |  305 | `	case 201: return "Created";` |
|  ! 0 |  306 | `	case 204: return "No Content";` |
|  ! 0 |  307 | `	case 301: return "Moved Permanently";` |
|  ! 0 |  308 | `	case 302: return "Found";` |
|  ! 0 |  309 | `	case 304: return "Not Modified";` |
|  ! 0 |  310 | `	case 400: return "Bad Request";` |
|  ! 0 |  311 | `	case 401: return "Unauthorized";` |
|    2 |  312 | `	case 403: return "Forbidden";` |
|  ! 0 |  313 | `	case 404: return "Not Found";` |
|  ! 0 |  314 | `	case 405: return "Method Not Allowed";` |
|  ! 0 |  315 | `	case 500: return "Internal Server Error";` |
|  ! 0 |  316 | `	case 502: return "Bad Gateway";` |
|  ! 0 |  317 | `	case 503: return "Service Unavailable";` |
|  ! 0 |  318 | `	default:  return "";` |
|    - |  319 | `	}` |
|   16 |  320 | `}` |
|    - |  321 | `/*` |
|    - |  322 | ` * Send an HTTP response using the VM's response headers and status code.` |
|    - |  323 | ` * Falls back to Content-Type: text/html if the script didn't set one.` |
|    - |  324 | ` */` |
|    - |  325 | `/*` |
|    - |  326 | ` * State passed to the response header callback.` |
|    - |  327 | ` */` |
|    - |  328 | `typedef struct VmResponseCtx VmResponseCtx;` |
|    - |  329 | `struct VmResponseCtx {` |
|    - |  330 | `	ph7_socket sock;` |
|    - |  331 | `	int bHasContentType;` |
|    - |  332 | `};` |
|   12 |  333 | `static int VmResponseHeaderCB(const char *zName, unsigned int nName,` |
|    - |  334 | `							   const char *zValue, unsigned int nValue,` |
|    - |  335 | `							   void *pUserData)` |
|  ! 0 |  336 | `{` |
|   12 |  337 | `	VmResponseCtx *pCtx = (VmResponseCtx *)pUserData;` |
|    - |  338 | `	char zLine[512];` |
|    - |  339 | `	int nLine;` |
|   12 |  340 | `	nLine = snprintf(zLine, sizeof(zLine), "%.*s: %.*s\r\n",` |
|    - |  341 | `		(int)nName, zName, (int)nValue, zValue);` |
|   12 |  342 | `	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);` |
|    7 |  343 | `	PH7_NetSendAll(pCtx->sock, zLine, nLine);` |
|    7 |  344 | `	if( nName == 12 && SyStrnicmp(zName, "Content-Type", 12) == 0 ){` |
|    2 |  345 | `		pCtx->bHasContentType = 1;` |
|    1 |  346 | `	}` |
|   12 |  347 | `	return PH7_OK;` |
|  ! 0 |  348 | `}` |
|   32 |  349 | `static void SendVmResponse(ph7_socket sock, ph7_vm *pVm,` |
|    - |  350 | `							const void *pBody, int nBodyLen)` |
|  ! 0 |  351 | `{` |
|    - |  352 | `	int iStatus;` |
|    - |  353 | `	VmResponseCtx sCtx;` |
|    - |  354 | `	char zLine[512];` |
|    - |  355 | `	int nLine;` |
|   32 |  356 | `	iStatus = 200;` |
|   32 |  357 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_STATUS, &iStatus);` |
|    - |  358 | `	/* Status line */` |
|   32 |  359 | `	nLine = snprintf(zLine, sizeof(zLine), "HTTP/1.1 %d %s\r\n", iStatus, StatusReason(iStatus));` |
|   32 |  360 | `	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);` |
|   31 |  361 | `	PH7_NetSendAll(sock, zLine, nLine);` |
|    - |  362 | `	/* Script-set headers via callback */` |
|   31 |  363 | `	sCtx.sock = sock;` |
|   31 |  364 | `	sCtx.bHasContentType = 0;` |
|   31 |  365 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_HEADERS, VmResponseHeaderCB, &sCtx);` |
|    - |  366 | `	/* Default Content-Type if not set by the script */` |
|   31 |  367 | `	if( !sCtx.bHasContentType ){` |
|   30 |  368 | `		PH7_NetSendAll(sock, "Content-Type: text/html\r\n", 25);` |
|   15 |  369 | `	}` |
|    - |  370 | `	/* Standard headers */` |
|   46 |  371 | `	nLine = snprintf(zLine, sizeof(zLine),` |
|    - |  372 | `		"Content-Length: %d\r\n"` |
|    - |  373 | `		"Connection: close\r\n"` |
|    - |  374 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  375 | `		"\r\n",` |
|    - |  376 | `		nBodyLen);` |
|   46 |  377 | `	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);` |
|   32 |  378 | `	PH7_NetSendAll(sock, zLine, nLine);` |
|    - |  379 | `	/* Body */` |
|   32 |  380 | `	if( pBody && nBodyLen > 0 ){` |
|   32 |  381 | `		PH7_NetSendAll(sock, pBody, nBodyLen);` |
|   16 |  382 | `	}` |
|   32 |  383 | `}` |
|    - |  384 | `/*` |
|    - |  385 | ` * Serve a static file.` |
|    - |  386 | ` */` |
|    4 |  387 | `static void ServeStaticFile(ph7_socket sock, const char *zPath, long nFileSize)` |
|  ! 0 |  388 | `{` |
|    - |  389 | `	const char *zMime;` |
|    - |  390 | `	char zHeader[512];` |
|    - |  391 | `	char zFileBuf[PHL_FILE_BUF];` |
|    - |  392 | `	FILE *pFile;` |
|    - |  393 | `	int nHeader;` |
|    - |  394 | `	size_t nRead;` |
|    4 |  395 | `	zMime = GetMimeType(zPath);` |
|    - |  396 | `#ifdef __WINNT__` |
|  ! 0 |  397 | `	if( fopen_s(&pFile, zPath, "rb") != 0 ) pFile = 0;` |
|    - |  398 | `#else` |
|    4 |  399 | `	pFile = fopen(zPath, "rb");` |
|    - |  400 | `#endif` |
|    4 |  401 | `	if( pFile == 0 ){` |
|  ! 0 |  402 | `		SendError(sock, 500, "Internal Server Error");` |
|  ! 0 |  403 | `		return;` |
|    - |  404 | `	}` |
|    4 |  405 | `	nHeader = snprintf(zHeader, sizeof(zHeader),` |
|    - |  406 | `		"HTTP/1.1 200 OK\r\n"` |
|    - |  407 | `		"Content-Type: %s\r\n"` |
|    - |  408 | `		"Content-Length: %ld\r\n"` |
|    - |  409 | `		"Connection: close\r\n"` |
|    - |  410 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  411 | `		"\r\n",` |
|    - |  412 | `		zMime, nFileSize);` |
|    4 |  413 | `	PH7_NetSendAll(sock, zHeader, nHeader);` |
|    8 |  414 | `	while( (nRead = fread(zFileBuf, 1, sizeof(zFileBuf), pFile)) > 0 ){` |
|    4 |  415 | `		if( PH7_NetSendAll(sock, zFileBuf, (int)nRead) != PH7_OK ){` |
|  ! 0 |  416 | `			break;` |
|    - |  417 | `		}` |
|  ! 0 |  418 | `	}` |
|    4 |  419 | `	fclose(pFile);` |
|    2 |  420 | `}` |
|    - |  421 | `/*` |
|    - |  422 | ` * Extract the request method and URI path from the first line of the HTTP request.` |
|    - |  423 | ` * E.g., "GET /foo/bar?q=1 HTTP/1.1\r\n..."` |
|    - |  424 | ` * Writes the method to zMethod (up to nMethodSize) and path to zPath (up to nPathSize).` |
|    - |  425 | ` * The path is the URI without the query string.` |
|    - |  426 | ` */` |
|   76 |  427 | `static void ExtractRequestLine(const char *zRequest, char *zMethod, int nMethodSize,` |
|    - |  428 | `							   char *zPath, int nPathSize)` |
|  ! 0 |  429 | `{` |
|   76 |  430 | `	const char *z = zRequest;` |
|    - |  431 | `	int i;` |
|    - |  432 | `	/* Extract method */` |
|  304 |  433 | `	for( i = 0; *z && *z != ' ' && i < nMethodSize - 1; z++, i++ ){` |
|  228 |  434 | `		zMethod[i] = *z;` |
|  114 |  435 | `	}` |
|   76 |  436 | `	zMethod[i] = 0;` |
|    - |  437 | `	/* Skip space */` |
|  152 |  438 | `	while( *z == ' ' ) z++;` |
|    - |  439 | `	/* Extract path (stop at '?', ' ', or end) */` |
|  736 |  440 | `	for( i = 0; *z && *z != '?' && *z != ' ' && *z != '\r' && i < nPathSize - 1; z++, i++ ){` |
|  660 |  441 | `		zPath[i] = *z;` |
|  330 |  442 | `	}` |
|   76 |  443 | `	zPath[i] = 0;` |
|   76 |  444 | `}` |
|    - |  445 | `/*` |
|    - |  446 | ` * Resolve a request URI path to a filesystem path under the document root.` |
|    - |  447 | ` * Returns 1 on success, 0 on failure (bad path).` |
|    - |  448 | ` * zOut must be at least PHL_MAX_PATH bytes.` |
|    - |  449 | ` */` |
|   38 |  450 | `static int ResolvePath(const char *zDocRoot, const char *zUri, char *zOut)` |
|  ! 0 |  451 | `{` |
|   38 |  452 | `	if( !IsPathSafe(zUri) ){` |
|  ! 0 |  453 | `		return 0;` |
|    - |  454 | `	}` |
|   38 |  455 | `	snprintf(zOut, PHL_MAX_PATH, "%s%s", zDocRoot, zUri);` |
|    - |  456 | `	/* If path points to a directory, try index.php */` |
|   38 |  457 | `	if( IsDirectory(zOut) ){` |
|  ! 0 |  458 | `		int n = (int)strlen(zOut);` |
|    - |  459 | `		/* Ensure trailing slash */` |
|  ! 0 |  460 | `		if( n > 0 && zOut[n-1] != '/' && zOut[n-1] != '\\' ){` |
|  ! 0 |  461 | `			if( n < PHL_MAX_PATH - 1 ){` |
|  ! 0 |  462 | `				zOut[n] = '/';` |
|  ! 0 |  463 | `				zOut[n+1] = 0;` |
|  ! 0 |  464 | `				n++;` |
|  ! 0 |  465 | `			}` |
|  ! 0 |  466 | `		}` |
|  ! 0 |  467 | `		snprintf(zOut + n, PHL_MAX_PATH - n, "index.php");` |
|  ! 0 |  468 | `	}` |
|   38 |  469 | `	return 1;` |
|   19 |  470 | `}` |
|    - |  471 | `/*` |
|    - |  472 | ` * Check if a filename ends with ".php" (case-insensitive).` |
|    - |  473 | ` */` |
|   36 |  474 | `static int IsPhpFile(const char *zPath)` |
|  ! 0 |  475 | `{` |
|   36 |  476 | `	int n = (int)strlen(zPath);` |
|   36 |  477 | `	if( n < 4 ){` |
|  ! 0 |  478 | `		return 0;` |
|    - |  479 | `	}` |
|   18 |  480 | `	return (` |
|    - |  481 | `#ifdef __WINNT__` |
|    - |  482 | `		_stricmp(zPath + n - 4, ".php")` |
|    - |  483 | `#else` |
|   36 |  484 | `		strcasecmp(zPath + n - 4, ".php")` |
|    - |  485 | `#endif` |
|   36 |  486 | `		== 0);` |
|   18 |  487 | `}` |
|    - |  488 | `/*` |
|    - |  489 | ` * Return a file's last-modification time as a comparable integer (0 on error).` |
|    - |  490 | ` * Used to invalidate the compiled-VM cache when a script is edited, preserving` |
|    - |  491 | ` * the dev-server expectation that reloading picks up source changes.` |
|    - |  492 | ` */` |
|   32 |  493 | `static long long GetFileMtime(const char *zPath)` |
|  ! 0 |  494 | `{` |
|    - |  495 | `#ifdef __WINNT__` |
|    - |  496 | `	WIN32_FILE_ATTRIBUTE_DATA info;` |
|  ! 0 |  497 | `	if( !GetFileAttributesExA(zPath, GetFileExInfoStandard, &info) ){` |
|  ! 0 |  498 | `		return 0;` |
|    - |  499 | `	}` |
|  ! 0 |  500 | `	return ((long long)info.ftLastWriteTime.dwHighDateTime << 32)` |
|    - |  501 | `		\| (long long)info.ftLastWriteTime.dwLowDateTime;` |
|    - |  502 | `#else` |
|    - |  503 | `	struct stat st;` |
|   32 |  504 | `	if( stat(zPath, &st) != 0 ){` |
|  ! 0 |  505 | `		return 0;` |
|    - |  506 | `	}` |
|   32 |  507 | `	return (long long)st.st_mtime;` |
|    - |  508 | `#endif` |
|   16 |  509 | `}` |
|    - |  510 | `/* ---- Compiled-VM reuse cache ------------------------------------------------` |
|    - |  511 | ` * Compilation dominates the per-request cost (the on-hardware profile measured` |
|    - |  512 | ` * 167 of 197 ms/request in compile). We therefore compile each script once and` |
|    - |  513 | ` * re-execute it per request, calling ph7_vm_reset() in between to clear all` |
|    - |  514 | ` * per-execution state (globals, superglobals, statics, output, ...). The cache` |
|    - |  515 | ` * is keyed by the resolved filesystem path and is safe without locking because` |
|    - |  516 | ` * the dev server is single-threaded. Set PHL_NO_REUSE=1 to fall back to the old` |
|    - |  517 | ` * compile-per-request behaviour (useful for diffing). */` |
|    - |  518 | `#define PHL_VM_CACHE_SIZE 16` |
|    - |  519 | `typedef struct PhlVmCacheEntry {` |
|    - |  520 | `	char zPath[PHL_MAX_PATH]; /* Resolved script path ("" = empty slot) */` |
|    - |  521 | `	ph7_vm *pVm;              /* Compiled, reusable VM */` |
|    - |  522 | `	unsigned long nUse;       /* Last-use clock for LRU eviction */` |
|    - |  523 | `	long long nMtime;         /* Source mtime when compiled (cache invalidation) */` |
|    - |  524 | `} PhlVmCacheEntry;` |
|    - |  525 | `static PhlVmCacheEntry g_vmCache[PHL_VM_CACHE_SIZE];` |
|    - |  526 | `static unsigned long g_vmCacheClock = 0;` |
|    - |  527 | `static int g_vmReuse = 1;` |
|    - |  528 |  |
|    - |  529 | `/*` |
|    - |  530 | ` * Acquire a ready-to-execute VM for zPath. On a cache hit the VM is reset and` |
|    - |  531 | ` * reused; on a miss it is compiled and cached (evicting the least-recently-used` |
|    - |  532 | ` * entry when full). Returns NULL on compile error. On success *pbCached tells` |
|    - |  533 | ` * the caller whether the VM is cache-owned (do NOT release it) or a throwaway` |
|    - |  534 | ` * (release after use, e.g. when reuse is disabled).` |
|    - |  535 | ` */` |
|   32 |  536 | `static ph7_vm *AcquireScriptVm(ph7 *pEngine, const char *zPath, int *pbCached)` |
|  ! 0 |  537 | `{` |
|   32 |  538 | `	int i, iFree = -1, iLru = -1;` |
|   32 |  539 | `	ph7_vm *pVm = 0;` |
|    - |  540 | `	long long nMtime;` |
|   32 |  541 | `	if( !g_vmReuse ){` |
|  ! 0 |  542 | `		*pbCached = 0;` |
|  ! 0 |  543 | `		if( ph7_compile_file(pEngine, zPath, &pVm, 0) != PH7_OK ){` |
|  ! 0 |  544 | `			return 0;` |
|    - |  545 | `		}` |
|  ! 0 |  546 | `		DefinePhpBinary(pVm);` |
|  ! 0 |  547 | `		return pVm;` |
|    - |  548 | `	}` |
|   32 |  549 | `	nMtime = GetFileMtime(zPath);` |
|  416 |  550 | `	for( i = 0 ; i < PHL_VM_CACHE_SIZE ; i++ ){` |
|  392 |  551 | `		if( g_vmCache[i].pVm == 0 ){` |
|  384 |  552 | `			if( iFree < 0 ){ iFree = i; }` |
|  384 |  553 | `			continue;` |
|    - |  554 | `		}` |
|    8 |  555 | `		if( strcmp(g_vmCache[i].zPath, zPath) == 0 ){` |
|    - |  556 | `			/* Hit: reuse only if the source is unchanged on disk (dev-server` |
|    - |  557 | `			 * live-edit) and the reset succeeds; otherwise drop and recompile. */` |
|    8 |  558 | `			if( g_vmCache[i].nMtime == nMtime` |
|    8 |  559 | `			 && ph7_vm_reset(g_vmCache[i].pVm) == PH7_OK ){` |
|    8 |  560 | `				g_vmCache[i].nUse = ++g_vmCacheClock;` |
|    8 |  561 | `				*pbCached = 1;` |
|    8 |  562 | `				return g_vmCache[i].pVm;` |
|    - |  563 | `			}` |
|  ! 0 |  564 | `			ph7_vm_release(g_vmCache[i].pVm);` |
|  ! 0 |  565 | `			g_vmCache[i].pVm = 0;` |
|  ! 0 |  566 | `			g_vmCache[i].zPath[0] = '\0';` |
|  ! 0 |  567 | `			iFree = i;` |
|  ! 0 |  568 | `			break;` |
|    - |  569 | `		}` |
|  ! 0 |  570 | `		if( iLru < 0 \|\| g_vmCache[i].nUse < g_vmCache[iLru].nUse ){` |
|  ! 0 |  571 | `			iLru = i;` |
|  ! 0 |  572 | `		}` |
|  ! 0 |  573 | `	}` |
|    - |  574 | `	/* Miss: compile a fresh VM. */` |
|   24 |  575 | `	if( ph7_compile_file(pEngine, zPath, &pVm, 0) != PH7_OK ){` |
|  ! 0 |  576 | `		*pbCached = 0;` |
|  ! 0 |  577 | `		return 0;` |
|    - |  578 | `	}` |
|   24 |  579 | `	DefinePhpBinary(pVm);` |
|   24 |  580 | `	if( iFree < 0 ){` |
|    - |  581 | `		/* Cache full: evict the least-recently-used entry. */` |
|  ! 0 |  582 | `		ph7_vm_release(g_vmCache[iLru].pVm);` |
|  ! 0 |  583 | `		iFree = iLru;` |
|  ! 0 |  584 | `	}` |
|   24 |  585 | `	snprintf(g_vmCache[iFree].zPath, sizeof(g_vmCache[iFree].zPath), "%s", zPath);` |
|   24 |  586 | `	g_vmCache[iFree].pVm = pVm;` |
|   24 |  587 | `	g_vmCache[iFree].nUse = ++g_vmCacheClock;` |
|   24 |  588 | `	g_vmCache[iFree].nMtime = nMtime;` |
|   24 |  589 | `	*pbCached = 1;` |
|   24 |  590 | `	return pVm;` |
|   16 |  591 | `}` |
|    - |  592 | `/*` |
|    - |  593 | ` * Release every cached VM. Called at server shutdown.` |
|    - |  594 | ` */` |
|   32 |  595 | `static void ReleaseVmCache(void)` |
|  ! 0 |  596 | `{` |
|    - |  597 | `	int i;` |
|  544 |  598 | `	for( i = 0 ; i < PHL_VM_CACHE_SIZE ; i++ ){` |
|  512 |  599 | `		if( g_vmCache[i].pVm ){` |
|   24 |  600 | `			ph7_vm_release(g_vmCache[i].pVm);` |
|   24 |  601 | `			g_vmCache[i].pVm = 0;` |
|   24 |  602 | `			g_vmCache[i].zPath[0] = '\0';` |
|   12 |  603 | `		}` |
|  256 |  604 | `	}` |
|   32 |  605 | `}` |
|    - |  606 | `/*` |
|    - |  607 | ` * Execute a PHP script and send its output as an HTTP response.` |
|    - |  608 | ` * pEngine is the shared engine instance.` |
|    - |  609 | ` * zFilePath is the resolved filesystem path to the PHP file.` |
|    - |  610 | ` * zRawRequest/nRequestLen is the raw HTTP request for superglobal population.` |
|    - |  611 | ` * The additional server attributes are set from the request context.` |
|    - |  612 | ` */` |
|   32 |  613 | `static void ExecutePhpScript(ph7 *pEngine, ph7_socket client,` |
|    - |  614 | `							 const char *zFilePath, const char *zScriptName,` |
|    - |  615 | `							 const char *zRawRequest, int nRequestLen,` |
|    - |  616 | `							 const char *zHost, int iPort, const char *zDocRoot,` |
|    - |  617 | `							 const char *zRemoteAddr, int iRemotePort)` |
|  ! 0 |  618 | `{` |
|   32 |  619 | `	ph7_vm *pVm = 0;` |
|    - |  620 | `	const void *pOutput;` |
|    - |  621 | `	unsigned int nOutputLen;` |
|    - |  622 | `	char zPortBuf[16];` |
|    - |  623 | `	char zRemotePortBuf[16];` |
|   32 |  624 | `	int bCached = 0;` |
|   32 |  625 | `	pVm = AcquireScriptVm(pEngine, zFilePath, &bCached);` |
|   32 |  626 | `	if( pVm == 0 ){` |
|  ! 0 |  627 | `		SendError(client, 500, "Internal Server Error");` |
|  ! 0 |  628 | `		return;` |
|    - |  629 | `	}` |
|    - |  630 | `	/* Feed the raw HTTP request to populate $_SERVER, $_GET, $_POST, etc. */` |
|   32 |  631 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);` |
|    - |  632 | `	/* Set additional $_SERVER attributes */` |
|   32 |  633 | `	snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);` |
|   32 |  634 | `	snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);` |
|   32 |  635 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);` |
|   32 |  636 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);` |
|   32 |  637 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);` |
|   32 |  638 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);` |
|   32 |  639 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zFilePath, -1);` |
|   32 |  640 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zScriptName, -1);` |
|   32 |  641 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);` |
|   32 |  642 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);` |
|   32 |  643 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);` |
|    - |  644 | `	/* Execute the script (output accumulates in the VM's internal buffer) */` |
|   32 |  645 | `	ph7_vm_exec(pVm, 0);` |
|    - |  646 | `	/* Extract accumulated output */` |
|   32 |  647 | `	pOutput = 0;` |
|   32 |  648 | `	nOutputLen = 0;` |
|   32 |  649 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);` |
|    - |  650 | `	/* Send the response using VM-set headers and status code */` |
|   32 |  651 | `	SendVmResponse(client, pVm, pOutput, (int)nOutputLen);` |
|    - |  652 | `	/* Cache-owned VMs are kept for reuse; throwaways are released. */` |
|   32 |  653 | `	if( !bCached ){` |
|  ! 0 |  654 | `		ph7_vm_release(pVm);` |
|  ! 0 |  655 | `	}` |
|   16 |  656 | `}` |
|    - |  657 | `/*` |
|    - |  658 | ` * Handle a single HTTP request.` |
|    - |  659 | ` */` |
|   38 |  660 | `static int HandleRequest(ph7 *pEngine, ph7_socket client,` |
|    - |  661 | `						  const char *zDocRoot, const char *zRouter,` |
|    - |  662 | `						  const char *zHost, int iPort,` |
|    - |  663 | `						  const char *zRawRequest, int nRequestLen,` |
|    - |  664 | `						  const char *zRemoteAddr, int iRemotePort)` |
|  ! 0 |  665 | `{` |
|    - |  666 | `	char zMethod[16];` |
|    - |  667 | `	char zUri[PHL_MAX_PATH];` |
|    - |  668 | `	char zFilePath[PHL_MAX_PATH];` |
|    - |  669 | `	long nFileSize;` |
|    - |  670 | `	/* Extract request line */` |
|   38 |  671 | `	ExtractRequestLine(zRawRequest, zMethod, sizeof(zMethod), zUri, sizeof(zUri));` |
|   19 |  672 | `	(void)zMethod;` |
|    - |  673 | `	/* Try router script first (if configured) */` |
|   38 |  674 | `	if( zRouter && zRouter[0] ){` |
|  ! 0 |  675 | `		ph7_vm *pVm = 0;` |
|    - |  676 | `		ph7_value *pRetVal;` |
|  ! 0 |  677 | `		int bCached = 0;` |
|    - |  678 | `		char zPortBuf[16];` |
|    - |  679 | `		char zRemotePortBuf[16];` |
|  ! 0 |  680 | `		pVm = AcquireScriptVm(pEngine, zRouter, &bCached);` |
|  ! 0 |  681 | `		if( pVm != 0 ){` |
|  ! 0 |  682 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);` |
|  ! 0 |  683 | `			snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);` |
|  ! 0 |  684 | `			snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);` |
|  ! 0 |  685 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);` |
|  ! 0 |  686 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);` |
|  ! 0 |  687 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);` |
|  ! 0 |  688 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);` |
|  ! 0 |  689 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zRouter, -1);` |
|  ! 0 |  690 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zUri, -1);` |
|  ! 0 |  691 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);` |
|  ! 0 |  692 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);` |
|  ! 0 |  693 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);` |
|  ! 0 |  694 | `			ph7_vm_exec(pVm, 0);` |
|    - |  695 | `			/* Check if the router returned false (meaning: fall through to default serving) */` |
|  ! 0 |  696 | `			pRetVal = 0;` |
|  ! 0 |  697 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_EXEC_VALUE, &pRetVal);` |
|  ! 0 |  698 | `			if( pRetVal && ph7_value_is_bool(pRetVal) && !ph7_value_to_bool(pRetVal) ){` |
|    - |  699 | `				/* Router returned false: fall through to default file serving */` |
|  ! 0 |  700 | `				if( !bCached ){` |
|  ! 0 |  701 | `					ph7_vm_release(pVm);` |
|  ! 0 |  702 | `				}` |
|  ! 0 |  703 | `			}else{` |
|    - |  704 | `				/* Router handled the request: send its output */` |
|    - |  705 | `				const void *pOutput;` |
|    - |  706 | `				unsigned int nOutputLen;` |
|  ! 0 |  707 | `				pOutput = 0;` |
|  ! 0 |  708 | `				nOutputLen = 0;` |
|  ! 0 |  709 | `				ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);` |
|  ! 0 |  710 | `				SendVmResponse(client, pVm, pOutput, (int)nOutputLen);` |
|  ! 0 |  711 | `				if( !bCached ){` |
|  ! 0 |  712 | `					ph7_vm_release(pVm);` |
|  ! 0 |  713 | `				}` |
|  ! 0 |  714 | `				return 200;` |
|    - |  715 | `			}` |
|  ! 0 |  716 | `		}` |
|    - |  717 | `		/* Router compile failed or returned false: fall through */` |
|  ! 0 |  718 | `	}` |
|    - |  719 | `	/* Resolve URI to filesystem path */` |
|   38 |  720 | `	if( !ResolvePath(zDocRoot, zUri, zFilePath) ){` |
|  ! 0 |  721 | `		SendError(client, 403, "Forbidden");` |
|  ! 0 |  722 | `		return 403;` |
|    - |  723 | `	}` |
|    - |  724 | `	/* Check if file exists */` |
|   38 |  725 | `	if( FileExists(zFilePath, &nFileSize) ){` |
|   36 |  726 | `		if( IsPhpFile(zFilePath) ){` |
|   48 |  727 | `			ExecutePhpScript(pEngine, client, zFilePath, zUri,` |
|   16 |  728 | `							 zRawRequest, nRequestLen,` |
|   16 |  729 | `							 zHost, iPort, zDocRoot,` |
|   16 |  730 | `							 zRemoteAddr, iRemotePort);` |
|   16 |  731 | `		}else{` |
|    4 |  732 | `			ServeStaticFile(client, zFilePath, nFileSize);` |
|    - |  733 | `		}` |
|   36 |  734 | `		return 200;` |
|    - |  735 | `	}` |
|    2 |  736 | `	SendError(client, 404, "Not Found");` |
|    2 |  737 | `	return 404;` |
|   19 |  738 | `}` |
|    - |  739 | `/*` |
|    - |  740 | ` * Log a request line to stderr.` |
|    - |  741 | ` */` |
|   38 |  742 | `static void LogRequest(const char *zRemoteAddr, int iRemotePort,` |
|    - |  743 | `					   const char *zMethod, const char *zUri, int iStatus)` |
|  ! 0 |  744 | `{` |
|    - |  745 | `	time_t now;` |
|    - |  746 | `	struct tm tm_buf;` |
|    - |  747 | `	char zTime[64];` |
|   38 |  748 | `	time(&now);` |
|    - |  749 | `#ifdef __WINNT__` |
|  ! 0 |  750 | `	localtime_s(&tm_buf, &now);` |
|    - |  751 | `#else` |
|   38 |  752 | `	localtime_r(&now, &tm_buf);` |
|    - |  753 | `#endif` |
|   38 |  754 | `	strftime(zTime, sizeof(zTime), "%a %b %d %H:%M:%S %Y", &tm_buf);` |
|   57 |  755 | `	fprintf(stderr, "[%s] %s:%d [%d]: %s %s\n",` |
|   19 |  756 | `		zTime, zRemoteAddr, iRemotePort, iStatus, zMethod, zUri);` |
|   38 |  757 | `}` |
|    - |  758 | `/*` |
|    - |  759 | ` * Main server entry point.` |
|    - |  760 | ` */` |
|   26 |  761 | `int phl_serve(const char *zHost, int iPort, const char *zDocRoot, const char *zRouter, const char *zBinaryPath)` |
|  ! 0 |  762 | `{` |
|    - |  763 | `	ph7 *pEngine;` |
|    - |  764 | `	ph7_socket listenSock;` |
|    - |  765 | `	ph7_socket clientSock;` |
|    - |  766 | `	struct sockaddr_in clientAddr;` |
|    - |  767 | `	ph7_socklen addrLen;` |
|    - |  768 | `	char *zRequestBuf;` |
|    - |  769 | `	int nRequestLen;` |
|    - |  770 | `	char zRemoteAddr[64];` |
|    - |  771 | `	int iRemotePort;` |
|    - |  772 | `	int rc;` |
|    - |  773 | `	/* Resolved interpreter path → PHP_BINARY for every served VM. */` |
|   26 |  774 | `	g_phpBinaryPath = zBinaryPath;` |
|    - |  775 | `	/* Compile-once / reuse is on by default; PHL_NO_REUSE=1 forces the legacy` |
|    - |  776 | `	 * compile-per-request path for behaviour diffing. */` |
|    - |  777 | `	{` |
|   26 |  778 | `		const char *zNoReuse = getenv("PHL_NO_REUSE");` |
|   26 |  779 | `		if( zNoReuse && zNoReuse[0] && zNoReuse[0] != '0' ){` |
|  ! 0 |  780 | `			g_vmReuse = 0;` |
|  ! 0 |  781 | `		}` |
|    - |  782 | `	}` |
|    - |  783 | `	/* Initialize networking */` |
|   26 |  784 | `	rc = PH7_NetInit();` |
|   26 |  785 | `	if( rc != PH7_OK ){` |
|  ! 0 |  786 | `		fprintf(stderr, "Error: Failed to initialize networking\n");` |
|  ! 0 |  787 | `		return 1;` |
|    - |  788 | `	}` |
|    - |  789 | `	/* Create the PH7 engine (shared across requests) */` |
|   26 |  790 | `	rc = ph7_init(&pEngine);` |
|   26 |  791 | `	if( rc != PH7_OK ){` |
|  ! 0 |  792 | `		fprintf(stderr, "Error: Failed to initialize PH7 engine\n");` |
|  ! 0 |  793 | `		PH7_NetCleanup();` |
|  ! 0 |  794 | `		return 1;` |
|    - |  795 | `	}` |
|    - |  796 | `	/* Create listening socket */` |
|   26 |  797 | `	listenSock = PH7_NetListen(zHost, iPort, 10);` |
|   26 |  798 | `	if( listenSock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  799 | `		fprintf(stderr, "Error: Failed to listen on %s:%d\n", zHost, iPort);` |
|  ! 0 |  800 | `		ph7_release(pEngine);` |
|  ! 0 |  801 | `		PH7_NetCleanup();` |
|  ! 0 |  802 | `		return 1;` |
|    - |  803 | `	}` |
|    - |  804 | `	/* Install signal handlers */` |
|   26 |  805 | `	InstallSignalHandlers();` |
|    - |  806 | `	/* Print banner */` |
|   26 |  807 | `	fprintf(stderr, "PHL %s Development Server started at http://%s:%d/\n", PH7_VERSION, zHost, iPort);` |
|   26 |  808 | `	fprintf(stderr, "Document root: %s\n", zDocRoot);` |
|   26 |  809 | `	if( zRouter ){` |
|  ! 0 |  810 | `		fprintf(stderr, "Router script: %s\n", zRouter);` |
|  ! 0 |  811 | `	}` |
|   26 |  812 | `	fprintf(stderr, "Press Ctrl+C to stop.\n");` |
|    - |  813 | `#ifndef __WINNT__` |
|    - |  814 | `	/* php CLI-server parity: PHP_CLI_SERVER_WORKERS=N pre-forks N workers` |
|    - |  815 | `	 * that all accept() on the shared listen socket (the kernel load-balances` |
|    - |  816 | `	 * connections). Each worker keeps its own warm VM cache and runs the` |
|    - |  817 | `	 * unchanged sequential loop, so no locking is needed anywhere. Like php,` |
|    - |  818 | `	 * the default (unset/1) stays single-process; Windows keeps the` |
|    - |  819 | `	 * single-process model (no fork — recorded). */` |
|    - |  820 | `	{` |
|   26 |  821 | `		const char *zWorkers = getenv("PHP_CLI_SERVER_WORKERS");` |
|   26 |  822 | `		int nWorkers = zWorkers ? atoi(zWorkers) : 0;` |
|   26 |  823 | `		if( nWorkers > 1 ){` |
|    - |  824 | `			int iWorker;` |
|    2 |  825 | `			int bChild = 0;` |
|    2 |  826 | `			if( nWorkers > 64 ){` |
|  ! 0 |  827 | `				nWorkers = 64;` |
|  ! 0 |  828 | `			}` |
|    - |  829 | `			/* No zombies if a worker crashes mid-run */` |
|    2 |  830 | `			signal(SIGCHLD, SIG_IGN);` |
|    8 |  831 | `			for( iWorker = 1 ; iWorker < nWorkers ; iWorker++ ){` |
|    6 |  832 | `				pid_t pid = fork();` |
|    9 |  833 | `				if( pid == 0 ){` |
|    - |  834 | `					/* worker child: fall through to the accept loop */` |
|    6 |  835 | `					bChild = 1;` |
|    6 |  836 | `					g_nWorkerPid = 0; /* children propagate nothing */` |
|    6 |  837 | `					break;` |
|    - |  838 | `				}` |
|    6 |  839 | `				if( pid < 0 ){` |
|  ! 0 |  840 | `					fprintf(stderr, "Warning: fork failed; continuing with %d worker(s)\n", iWorker);` |
|  ! 0 |  841 | `					break;` |
|    - |  842 | `				}` |
|    6 |  843 | `				g_aWorkerPid[g_nWorkerPid++] = pid;` |
|    3 |  844 | `			}` |
|    9 |  845 | `			if( !bChild ){` |
|    2 |  846 | `				fprintf(stderr, "Using %d worker processes\n", nWorkers);` |
|    1 |  847 | `			}` |
|    4 |  848 | `		}` |
|    - |  849 | `	}` |
|    - |  850 | `#endif` |
|    - |  851 | `	/* Allocate request buffer */` |
|   32 |  852 | `	zRequestBuf = (char *)malloc(PHL_MAX_REQUEST);` |
|   32 |  853 | `	if( zRequestBuf == 0 ){` |
|  ! 0 |  854 | `		fprintf(stderr, "Error: Out of memory\n");` |
|  ! 0 |  855 | `		PH7_NetClose(listenSock);` |
|  ! 0 |  856 | `		ph7_release(pEngine);` |
|  ! 0 |  857 | `		PH7_NetCleanup();` |
|  ! 0 |  858 | `		return 1;` |
|    - |  859 | `	}` |
|    - |  860 | `	/* Accept loop */` |
|   70 |  861 | `	while( !g_shutdown ){` |
|   70 |  862 | `		addrLen = sizeof(clientAddr);` |
|   70 |  863 | `		clientSock = PH7_NetAccept(listenSock, (struct sockaddr *)&clientAddr, &addrLen);` |
|   70 |  864 | `		if( clientSock == PH7_NET_INVALID_SOCKET ){` |
|   32 |  865 | `			if( g_shutdown ){` |
|   32 |  866 | `				break;` |
|    - |  867 | `			}` |
|  ! 0 |  868 | `			continue;` |
|    - |  869 | `		}` |
|    - |  870 | `		/* Set a receive timeout so we don't block forever on a slow client */` |
|   38 |  871 | `		PH7_NetSetTimeout(clientSock, 5000);` |
|    - |  872 | `		/* Read the request */` |
|   38 |  873 | `		nRequestLen = ReadRequest(clientSock, zRequestBuf, PHL_MAX_REQUEST);` |
|   38 |  874 | `		if( nRequestLen > 0 ){` |
|    - |  875 | `			char zMethod[16];` |
|    - |  876 | `			char zUri[PHL_MAX_PATH];` |
|    - |  877 | `			/* Extract method and URI for logging */` |
|   38 |  878 | `			ExtractRequestLine(zRequestBuf, zMethod, sizeof(zMethod), zUri, sizeof(zUri));` |
|    - |  879 | `			/* Get client address info */` |
|   38 |  880 | `			PH7_NetAddrToString((struct sockaddr *)&clientAddr, zRemoteAddr, sizeof(zRemoteAddr));` |
|   38 |  881 | `			iRemotePort = PH7_NetAddrPort((struct sockaddr *)&clientAddr);` |
|    - |  882 | `			/* Handle the request */` |
|   57 |  883 | `			rc = HandleRequest(pEngine, clientSock, zDocRoot, zRouter,` |
|   19 |  884 | `						  zHost, iPort, zRequestBuf, nRequestLen,` |
|   19 |  885 | `						  zRemoteAddr, iRemotePort);` |
|    - |  886 | `			/* Log it */` |
|   38 |  887 | `			LogRequest(zRemoteAddr, iRemotePort, zMethod, zUri, rc);` |
|   19 |  888 | `		}` |
|   38 |  889 | `		PH7_NetClose(clientSock);` |
|  ! 0 |  890 | `	}` |
|    - |  891 | `	/* Cleanup */` |
|    - |  892 | `#ifndef __WINNT__` |
|    - |  893 | `	/* Parent: take the workers down with us */` |
|    - |  894 | `	{` |
|    - |  895 | `		int k;` |
|   38 |  896 | `		for( k = 0 ; k < g_nWorkerPid ; k++ ){` |
|    6 |  897 | `			kill(g_aWorkerPid[k], SIGTERM);` |
|    3 |  898 | `		}` |
|    - |  899 | `	}` |
|    - |  900 | `#endif` |
|   32 |  901 | `	fprintf(stderr, "\nShutting down...\n");` |
|   32 |  902 | `	ReleaseVmCache();` |
|   32 |  903 | `	free(zRequestBuf);` |
|   32 |  904 | `	PH7_NetClose(listenSock);` |
|   32 |  905 | `	ph7_release(pEngine);` |
|   32 |  906 | `	PH7_NetCleanup();` |
|   32 |  907 | `	return 0;` |
|   16 |  908 | `}` |
|    - |  909 |  |
|    - |  910 | `#endif /* PHL_ENABLE_SERVER */` |
|    - |  911 |  |
