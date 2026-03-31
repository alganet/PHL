# src/phl/server.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 192/312 lines (61.54%)

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
|    6 |   56 | `static void SignalHandler(int sig)` |
|    - |   57 |  |
|    3 |   58 | `	(void)sig;` |
|    6 |   59 | `	g_shutdown = 1;` |
|    6 |   60 |  |
|    - |   61 | `#endif` |
|    - |   62 | `/*` |
|    - |   63 | ` * Install signal handlers for graceful shutdown.` |
|    - |   64 | ` */` |
|    6 |   65 | `static void InstallSignalHandlers(void)` |
|  ! 0 |   66 |  |
|    - |   67 | `#ifdef __WINNT__` |
|  ! 0 |   68 | `	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);` |
|    - |   69 | `#else` |
|    - |   70 | `	struct sigaction sa;` |
|    6 |   71 | `	memset(&sa, 0, sizeof(sa));` |
|    6 |   72 | `	sa.sa_handler = SignalHandler;` |
|    6 |   73 | `	sigaction(SIGINT, &sa, 0);` |
|    6 |   74 | `	sigaction(SIGTERM, &sa, 0);` |
|    - |   75 | `#endif` |
|    6 |   76 |  |
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
|    8 |  130 | `static int IsPathSafe(const char *zPath)` |
|  ! 0 |  131 |  |
|    8 |  132 | `	const char *z = zPath;` |
|  104 |  133 | `	while( *z ){` |
|   96 |  134 | `		if( z[0] == '.' && z[1] == '.' && (z[2] == '/' \|\| z[2] == '\\' \|\| z[2] == 0) ){` |
|  ! 0 |  135 | `			return 0;` |
|    - |  136 | `		}` |
|   96 |  137 | `		z++;` |
|  ! 0 |  138 | `	}` |
|    8 |  139 | `	return 1;` |
|    4 |  140 |  |
|    - |  141 | `/*` |
|    - |  142 | ` * Check if a file exists and is a regular file.` |
|    - |  143 | ` * Returns 1 if it exists, 0 otherwise. Sets *pSize to the file size.` |
|    - |  144 | ` */` |
|    8 |  145 | `static int FileExists(const char *zPath, long *pSize)` |
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
|    8 |  161 | `	if( stat(zPath, &st) != 0 ){` |
|    2 |  162 | `		return 0;` |
|    - |  163 | `	}` |
|    6 |  164 | `	if( !S_ISREG(st.st_mode) ){` |
|  ! 0 |  165 | `		return 0;` |
|    - |  166 | `	}` |
|    6 |  167 | `	if( pSize ){` |
|    6 |  168 | `		*pSize = (long)st.st_size;` |
|    3 |  169 | `	}` |
|    6 |  170 | `	return 1;` |
|    - |  171 | `#endif` |
|    4 |  172 |  |
|    - |  173 | `/*` |
|    - |  174 | ` * Check if a path is a directory.` |
|    - |  175 | ` */` |
|    8 |  176 | `static int IsDirectory(const char *zPath)` |
|  ! 0 |  177 |  |
|    - |  178 | `#ifdef __WINNT__` |
|  ! 0 |  179 | `	DWORD attr = GetFileAttributesA(zPath);` |
|  ! 0 |  180 | `	return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));` |
|    - |  181 | `#else` |
|    - |  182 | `	struct stat st;` |
|    8 |  183 | `	if( stat(zPath, &st) != 0 ){` |
|    2 |  184 | `		return 0;` |
|    - |  185 | `	}` |
|    6 |  186 | `	return S_ISDIR(st.st_mode);` |
|    - |  187 | `#endif` |
|    4 |  188 |  |
|    - |  189 | `/*` |
|    - |  190 | ` * Read the full HTTP request from a socket into a buffer.` |
|    - |  191 | ` * Reads headers first (until \r\n\r\n), then reads the body` |
|    - |  192 | ` * based on Content-Length if present.` |
|    - |  193 | ` * Returns total bytes read, or -1 on error.` |
|    - |  194 | ` */` |
|    8 |  195 | `static int ReadRequest(ph7_socket sock, char *zBuf, int nBufSize)` |
|  ! 0 |  196 |  |
|    8 |  197 | `	int nTotal = 0;` |
|    - |  198 | `	int nRead;` |
|    - |  199 | `	char *zHeaderEnd;` |
|    - |  200 | `	/* Read data until we find the end of headers */` |
|    8 |  201 | `	while( nTotal < nBufSize - 1 ){` |
|    8 |  202 | `		nRead = PH7_NetRecv(sock, zBuf + nTotal, nBufSize - 1 - nTotal, 0);` |
|    8 |  203 | `		if( nRead <= 0 ){` |
|  ! 0 |  204 | `			if( nTotal == 0 ){` |
|  ! 0 |  205 | `				return -1;` |
|    - |  206 | `			}` |
|  ! 0 |  207 | `			break;` |
|    - |  208 | `		}` |
|    8 |  209 | `		nTotal += nRead;` |
|    8 |  210 | `		zBuf[nTotal] = 0;` |
|    - |  211 | `		/* Check if we have the complete headers */` |
|    8 |  212 | `		zHeaderEnd = strstr(zBuf, "\r\n\r\n");` |
|    8 |  213 | `		if( zHeaderEnd ){` |
|    8 |  214 | `			int nHeaderLen = (int)(zHeaderEnd - zBuf) + 4;` |
|    - |  215 | `			/* Look for Content-Length */` |
|    8 |  216 | `			const char *zCL = strstr(zBuf, "Content-Length:");` |
|    8 |  217 | `			if( zCL == 0 ){` |
|    8 |  218 | `				zCL = strstr(zBuf, "content-length:");` |
|    4 |  219 | `			}` |
|    8 |  220 | `			if( zCL ){` |
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
|    8 |  235 | `			break;` |
|    - |  236 | `		}` |
|  ! 0 |  237 | `	}` |
|    8 |  238 | `	zBuf[nTotal] = 0;` |
|    8 |  239 | `	return nTotal;` |
|    4 |  240 |  |
|    - |  241 | `/*` |
|    - |  242 | ` * Send an HTTP response with headers.` |
|    - |  243 | ` */` |
|    4 |  244 | `static void SendResponse(ph7_socket sock, int iStatus, const char *zStatus,` |
|    - |  245 | `						 const char *zContentType, const void *pBody, int nBodyLen)` |
|  ! 0 |  246 |  |
|    - |  247 | `	char zHeader[512];` |
|    - |  248 | `	int nHeader;` |
|    4 |  249 | `	nHeader = snprintf(zHeader, sizeof(zHeader),` |
|    - |  250 | `		"HTTP/1.1 %d %s\r\n"` |
|    - |  251 | `		"Content-Type: %s\r\n"` |
|    - |  252 | `		"Content-Length: %d\r\n"` |
|    - |  253 | `		"Connection: close\r\n"` |
|    - |  254 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  255 | `		"\r\n",` |
|    - |  256 | `		iStatus, zStatus, zContentType, nBodyLen);` |
|    4 |  257 | `	PH7_NetSendAll(sock, zHeader, nHeader);` |
|    4 |  258 | `	if( pBody && nBodyLen > 0 ){` |
|    4 |  259 | `		PH7_NetSendAll(sock, pBody, nBodyLen);` |
|    2 |  260 | `	}` |
|    4 |  261 |  |
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
|    - |  276 | ` * Serve a static file.` |
|    - |  277 | ` */` |
|    4 |  278 | `static void ServeStaticFile(ph7_socket sock, const char *zPath, long nFileSize)` |
|  ! 0 |  279 |  |
|    - |  280 | `	const char *zMime;` |
|    - |  281 | `	char zHeader[512];` |
|    - |  282 | `	char zFileBuf[PHL_FILE_BUF];` |
|    - |  283 | `	FILE *pFile;` |
|    - |  284 | `	int nHeader;` |
|    - |  285 | `	size_t nRead;` |
|    4 |  286 | `	zMime = GetMimeType(zPath);` |
|    - |  287 | `#ifdef __WINNT__` |
|  ! 0 |  288 | `	if( fopen_s(&pFile, zPath, "rb") != 0 ) pFile = 0;` |
|    - |  289 | `#else` |
|    4 |  290 | `	pFile = fopen(zPath, "rb");` |
|    - |  291 | `#endif` |
|    4 |  292 | `	if( pFile == 0 ){` |
|  ! 0 |  293 | `		SendError(sock, 500, "Internal Server Error");` |
|  ! 0 |  294 | `		return;` |
|    - |  295 | `	}` |
|    4 |  296 | `	nHeader = snprintf(zHeader, sizeof(zHeader),` |
|    - |  297 | `		"HTTP/1.1 200 OK\r\n"` |
|    - |  298 | `		"Content-Type: %s\r\n"` |
|    - |  299 | `		"Content-Length: %ld\r\n"` |
|    - |  300 | `		"Connection: close\r\n"` |
|    - |  301 | `		"Server: PHL/" PH7_VERSION "\r\n"` |
|    - |  302 | `		"\r\n",` |
|    - |  303 | `		zMime, nFileSize);` |
|    4 |  304 | `	PH7_NetSendAll(sock, zHeader, nHeader);` |
|    8 |  305 | `	while( (nRead = fread(zFileBuf, 1, sizeof(zFileBuf), pFile)) > 0 ){` |
|    4 |  306 | `		if( PH7_NetSendAll(sock, zFileBuf, (int)nRead) != PH7_OK ){` |
|  ! 0 |  307 | `			break;` |
|    - |  308 | `		}` |
|  ! 0 |  309 | `	}` |
|    4 |  310 | `	fclose(pFile);` |
|    2 |  311 |  |
|    - |  312 | `/*` |
|    - |  313 | ` * Extract the request method and URI path from the first line of the HTTP request.` |
|    - |  314 | ` * E.g., "GET /foo/bar?q=1 HTTP/1.1\r\n..."` |
|    - |  315 | ` * Writes the method to zMethod (up to nMethodSize) and path to zPath (up to nPathSize).` |
|    - |  316 | ` * The path is the URI without the query string.` |
|    - |  317 | ` */` |
|   16 |  318 | `static void ExtractRequestLine(const char *zRequest, char *zMethod, int nMethodSize,` |
|    - |  319 | `							   char *zPath, int nPathSize)` |
|  ! 0 |  320 |  |
|   16 |  321 | `	const char *z = zRequest;` |
|    - |  322 | `	int i;` |
|    - |  323 | `	/* Extract method */` |
|   64 |  324 | `	for( i = 0; *z && *z != ' ' && i < nMethodSize - 1; z++, i++ ){` |
|   48 |  325 | `		zMethod[i] = *z;` |
|   24 |  326 | `	}` |
|   16 |  327 | `	zMethod[i] = 0;` |
|    - |  328 | `	/* Skip space */` |
|   32 |  329 | `	while( *z == ' ' ) z++;` |
|    - |  330 | `	/* Extract path (stop at '?', ' ', or end) */` |
|  208 |  331 | `	for( i = 0; *z && *z != '?' && *z != ' ' && *z != '\r' && i < nPathSize - 1; z++, i++ ){` |
|  192 |  332 | `		zPath[i] = *z;` |
|   96 |  333 | `	}` |
|   16 |  334 | `	zPath[i] = 0;` |
|   16 |  335 |  |
|    - |  336 | `/*` |
|    - |  337 | ` * Resolve a request URI path to a filesystem path under the document root.` |
|    - |  338 | ` * Returns 1 on success, 0 on failure (bad path).` |
|    - |  339 | ` * zOut must be at least PHL_MAX_PATH bytes.` |
|    - |  340 | ` */` |
|    8 |  341 | `static int ResolvePath(const char *zDocRoot, const char *zUri, char *zOut)` |
|  ! 0 |  342 |  |
|    8 |  343 | `	if( !IsPathSafe(zUri) ){` |
|  ! 0 |  344 | `		return 0;` |
|    - |  345 | `	}` |
|    8 |  346 | `	snprintf(zOut, PHL_MAX_PATH, "%s%s", zDocRoot, zUri);` |
|    - |  347 | `	/* If path points to a directory, try index.php */` |
|    8 |  348 | `	if( IsDirectory(zOut) ){` |
|  ! 0 |  349 | `		int n = (int)strlen(zOut);` |
|    - |  350 | `		/* Ensure trailing slash */` |
|  ! 0 |  351 | `		if( n > 0 && zOut[n-1] != '/' && zOut[n-1] != '\\' ){` |
|  ! 0 |  352 | `			if( n < PHL_MAX_PATH - 1 ){` |
|  ! 0 |  353 | `				zOut[n] = '/';` |
|  ! 0 |  354 | `				zOut[n+1] = 0;` |
|  ! 0 |  355 | `				n++;` |
|  ! 0 |  356 | `			}` |
|  ! 0 |  357 | `		}` |
|  ! 0 |  358 | `		snprintf(zOut + n, PHL_MAX_PATH - n, "index.php");` |
|  ! 0 |  359 | `	}` |
|    8 |  360 | `	return 1;` |
|    4 |  361 |  |
|    - |  362 | `/*` |
|    - |  363 | ` * Check if a filename ends with ".php" (case-insensitive).` |
|    - |  364 | ` */` |
|    6 |  365 | `static int IsPhpFile(const char *zPath)` |
|  ! 0 |  366 |  |
|    6 |  367 | `	int n = (int)strlen(zPath);` |
|    6 |  368 | `	if( n < 4 ){` |
|  ! 0 |  369 | `		return 0;` |
|    - |  370 | `	}` |
|    3 |  371 | `	return (` |
|    - |  372 | `#ifdef __WINNT__` |
|    - |  373 | `		_stricmp(zPath + n - 4, ".php")` |
|    - |  374 | `#else` |
|    6 |  375 | `		strcasecmp(zPath + n - 4, ".php")` |
|    - |  376 | `#endif` |
|    6 |  377 | `		== 0);` |
|    3 |  378 |  |
|    - |  379 | `/*` |
|    - |  380 | ` * Execute a PHP script and send its output as an HTTP response.` |
|    - |  381 | ` * pEngine is the shared engine instance.` |
|    - |  382 | ` * zFilePath is the resolved filesystem path to the PHP file.` |
|    - |  383 | ` * zRawRequest/nRequestLen is the raw HTTP request for superglobal population.` |
|    - |  384 | ` * The additional server attributes are set from the request context.` |
|    - |  385 | ` */` |
|    2 |  386 | `static void ExecutePhpScript(ph7 *pEngine, ph7_socket client,` |
|    - |  387 | `							 const char *zFilePath, const char *zScriptName,` |
|    - |  388 | `							 const char *zRawRequest, int nRequestLen,` |
|    - |  389 | `							 const char *zHost, int iPort, const char *zDocRoot,` |
|    - |  390 | `							 const char *zRemoteAddr, int iRemotePort)` |
|  ! 0 |  391 |  |
|    2 |  392 | `	ph7_vm *pVm = 0;` |
|    - |  393 | `	const void *pOutput;` |
|    - |  394 | `	unsigned int nOutputLen;` |
|    - |  395 | `	char zPortBuf[16];` |
|    - |  396 | `	char zRemotePortBuf[16];` |
|    - |  397 | `	int rc;` |
|    2 |  398 | `	rc = ph7_compile_file(pEngine, zFilePath, &pVm, 0);` |
|    2 |  399 | `	if( rc != PH7_OK ){` |
|  ! 0 |  400 | `		SendError(client, 500, "Internal Server Error");` |
|  ! 0 |  401 | `		return;` |
|    - |  402 | `	}` |
|    - |  403 | `	/* Feed the raw HTTP request to populate $_SERVER, $_GET, $_POST, etc. */` |
|    2 |  404 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);` |
|    - |  405 | `	/* Set additional $_SERVER attributes */` |
|    2 |  406 | `	snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);` |
|    2 |  407 | `	snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);` |
|    2 |  408 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);` |
|    2 |  409 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);` |
|    2 |  410 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);` |
|    2 |  411 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);` |
|    2 |  412 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zFilePath, -1);` |
|    2 |  413 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zScriptName, -1);` |
|    2 |  414 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);` |
|    2 |  415 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);` |
|    2 |  416 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);` |
|    - |  417 | `	/* Execute the script (output accumulates in the VM's internal buffer) */` |
|    2 |  418 | `	ph7_vm_exec(pVm, 0);` |
|    - |  419 | `	/* Extract accumulated output */` |
|    2 |  420 | `	pOutput = 0;` |
|    2 |  421 | `	nOutputLen = 0;` |
|    2 |  422 | `	ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);` |
|    - |  423 | `	/* Send the response */` |
|    2 |  424 | `	SendResponse(client, 200, "OK", "text/html", pOutput, (int)nOutputLen);` |
|    2 |  425 | `	ph7_vm_release(pVm);` |
|    1 |  426 |  |
|    - |  427 | `/*` |
|    - |  428 | ` * Handle a single HTTP request.` |
|    - |  429 | ` */` |
|    8 |  430 | `static int HandleRequest(ph7 *pEngine, ph7_socket client,` |
|    - |  431 | `						  const char *zDocRoot, const char *zRouter,` |
|    - |  432 | `						  const char *zHost, int iPort,` |
|    - |  433 | `						  const char *zRawRequest, int nRequestLen,` |
|    - |  434 | `						  const char *zRemoteAddr, int iRemotePort)` |
|  ! 0 |  435 |  |
|    - |  436 | `	char zMethod[16];` |
|    - |  437 | `	char zUri[PHL_MAX_PATH];` |
|    - |  438 | `	char zFilePath[PHL_MAX_PATH];` |
|    - |  439 | `	long nFileSize;` |
|    - |  440 | `	/* Extract request line */` |
|    8 |  441 | `	ExtractRequestLine(zRawRequest, zMethod, sizeof(zMethod), zUri, sizeof(zUri));` |
|    4 |  442 | `	(void)zMethod;` |
|    - |  443 | `	/* Try router script first (if configured) */` |
|    8 |  444 | `	if( zRouter && zRouter[0] ){` |
|  ! 0 |  445 | `		ph7_vm *pVm = 0;` |
|    - |  446 | `		ph7_value *pRetVal;` |
|    - |  447 | `		int rc;` |
|    - |  448 | `		char zPortBuf[16];` |
|    - |  449 | `		char zRemotePortBuf[16];` |
|  ! 0 |  450 | `		rc = ph7_compile_file(pEngine, zRouter, &pVm, 0);` |
|  ! 0 |  451 | `		if( rc == PH7_OK ){` |
|  ! 0 |  452 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);` |
|  ! 0 |  453 | `			snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);` |
|  ! 0 |  454 | `			snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);` |
|  ! 0 |  455 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);` |
|  ! 0 |  456 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);` |
|  ! 0 |  457 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);` |
|  ! 0 |  458 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);` |
|  ! 0 |  459 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zRouter, -1);` |
|  ! 0 |  460 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zUri, -1);` |
|  ! 0 |  461 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);` |
|  ! 0 |  462 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);` |
|  ! 0 |  463 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);` |
|  ! 0 |  464 | `			ph7_vm_exec(pVm, 0);` |
|    - |  465 | `			/* Check if the router returned false (meaning: fall through to default serving) */` |
|  ! 0 |  466 | `			pRetVal = 0;` |
|  ! 0 |  467 | `			ph7_vm_config(pVm, PH7_VM_CONFIG_EXEC_VALUE, &pRetVal);` |
|  ! 0 |  468 | `			if( pRetVal && ph7_value_is_bool(pRetVal) && !ph7_value_to_bool(pRetVal) ){` |
|    - |  469 | `				/* Router returned false: fall through to default file serving */` |
|  ! 0 |  470 | `				ph7_vm_release(pVm);` |
|  ! 0 |  471 | `			}else{` |
|    - |  472 | `				/* Router handled the request: send its output */` |
|    - |  473 | `				const void *pOutput;` |
|    - |  474 | `				unsigned int nOutputLen;` |
|  ! 0 |  475 | `				pOutput = 0;` |
|  ! 0 |  476 | `				nOutputLen = 0;` |
|  ! 0 |  477 | `				ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);` |
|  ! 0 |  478 | `				SendResponse(client, 200, "OK", "text/html", pOutput, (int)nOutputLen);` |
|  ! 0 |  479 | `				ph7_vm_release(pVm);` |
|  ! 0 |  480 | `				return 200;` |
|    - |  481 | `			}` |
|  ! 0 |  482 | `		}` |
|    - |  483 | `		/* Router compile failed or returned false: fall through */` |
|  ! 0 |  484 | `	}` |
|    - |  485 | `	/* Resolve URI to filesystem path */` |
|    8 |  486 | `	if( !ResolvePath(zDocRoot, zUri, zFilePath) ){` |
|  ! 0 |  487 | `		SendError(client, 403, "Forbidden");` |
|  ! 0 |  488 | `		return 403;` |
|    - |  489 | `	}` |
|    - |  490 | `	/* Check if file exists */` |
|    8 |  491 | `	if( FileExists(zFilePath, &nFileSize) ){` |
|    6 |  492 | `		if( IsPhpFile(zFilePath) ){` |
|    3 |  493 | `			ExecutePhpScript(pEngine, client, zFilePath, zUri,` |
|    1 |  494 | `							 zRawRequest, nRequestLen,` |
|    1 |  495 | `							 zHost, iPort, zDocRoot,` |
|    1 |  496 | `							 zRemoteAddr, iRemotePort);` |
|    1 |  497 | `		}else{` |
|    4 |  498 | `			ServeStaticFile(client, zFilePath, nFileSize);` |
|    - |  499 | `		}` |
|    6 |  500 | `		return 200;` |
|    - |  501 | `	}` |
|    2 |  502 | `	SendError(client, 404, "Not Found");` |
|    2 |  503 | `	return 404;` |
|    4 |  504 |  |
|    - |  505 | `/*` |
|    - |  506 | ` * Log a request line to stderr.` |
|    - |  507 | ` */` |
|    8 |  508 | `static void LogRequest(const char *zRemoteAddr, int iRemotePort,` |
|    - |  509 | `					   const char *zMethod, const char *zUri, int iStatus)` |
|  ! 0 |  510 |  |
|    - |  511 | `	time_t now;` |
|    - |  512 | `	struct tm tm_buf;` |
|    - |  513 | `	char zTime[64];` |
|    8 |  514 | `	time(&now);` |
|    - |  515 | `#ifdef __WINNT__` |
|  ! 0 |  516 | `	localtime_s(&tm_buf, &now);` |
|    - |  517 | `#else` |
|    8 |  518 | `	localtime_r(&now, &tm_buf);` |
|    - |  519 | `#endif` |
|    8 |  520 | `	strftime(zTime, sizeof(zTime), "%a %b %d %H:%M:%S %Y", &tm_buf);` |
|   12 |  521 | `	fprintf(stderr, "[%s] %s:%d [%d]: %s %s\n",` |
|    4 |  522 | `		zTime, zRemoteAddr, iRemotePort, iStatus, zMethod, zUri);` |
|    8 |  523 |  |
|    - |  524 | `/*` |
|    - |  525 | ` * Main server entry point.` |
|    - |  526 | ` */` |
|    6 |  527 | `int phl_serve(const char *zHost, int iPort, const char *zDocRoot, const char *zRouter)` |
|  ! 0 |  528 |  |
|    - |  529 | `	ph7 *pEngine;` |
|    - |  530 | `	ph7_socket listenSock;` |
|    - |  531 | `	ph7_socket clientSock;` |
|    - |  532 | `	struct sockaddr_in clientAddr;` |
|    - |  533 | `	ph7_socklen addrLen;` |
|    - |  534 | `	char *zRequestBuf;` |
|    - |  535 | `	int nRequestLen;` |
|    - |  536 | `	char zRemoteAddr[64];` |
|    - |  537 | `	int iRemotePort;` |
|    - |  538 | `	int rc;` |
|    - |  539 | `	/* Initialize networking */` |
|    6 |  540 | `	rc = PH7_NetInit();` |
|    6 |  541 | `	if( rc != PH7_OK ){` |
|  ! 0 |  542 | `		fprintf(stderr, "Error: Failed to initialize networking\n");` |
|  ! 0 |  543 | `		return 1;` |
|    - |  544 | `	}` |
|    - |  545 | `	/* Create the PH7 engine (shared across requests) */` |
|    6 |  546 | `	rc = ph7_init(&pEngine);` |
|    6 |  547 | `	if( rc != PH7_OK ){` |
|  ! 0 |  548 | `		fprintf(stderr, "Error: Failed to initialize PH7 engine\n");` |
|  ! 0 |  549 | `		PH7_NetCleanup();` |
|  ! 0 |  550 | `		return 1;` |
|    - |  551 | `	}` |
|    - |  552 | `	/* Create listening socket */` |
|    6 |  553 | `	listenSock = PH7_NetListen(zHost, iPort, 10);` |
|    6 |  554 | `	if( listenSock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  555 | `		fprintf(stderr, "Error: Failed to listen on %s:%d\n", zHost, iPort);` |
|  ! 0 |  556 | `		ph7_release(pEngine);` |
|  ! 0 |  557 | `		PH7_NetCleanup();` |
|  ! 0 |  558 | `		return 1;` |
|    - |  559 | `	}` |
|    - |  560 | `	/* Install signal handlers */` |
|    6 |  561 | `	InstallSignalHandlers();` |
|    - |  562 | `	/* Print banner */` |
|    6 |  563 | `	fprintf(stderr, "PHL %s Development Server started at http://%s:%d/\n", PH7_VERSION, zHost, iPort);` |
|    6 |  564 | `	fprintf(stderr, "Document root: %s\n", zDocRoot);` |
|    6 |  565 | `	if( zRouter ){` |
|  ! 0 |  566 | `		fprintf(stderr, "Router script: %s\n", zRouter);` |
|  ! 0 |  567 | `	}` |
|    6 |  568 | `	fprintf(stderr, "Press Ctrl+C to stop.\n");` |
|    - |  569 | `	/* Allocate request buffer */` |
|    6 |  570 | `	zRequestBuf = (char *)malloc(PHL_MAX_REQUEST);` |
|    6 |  571 | `	if( zRequestBuf == 0 ){` |
|  ! 0 |  572 | `		fprintf(stderr, "Error: Out of memory\n");` |
|  ! 0 |  573 | `		PH7_NetClose(listenSock);` |
|  ! 0 |  574 | `		ph7_release(pEngine);` |
|  ! 0 |  575 | `		PH7_NetCleanup();` |
|  ! 0 |  576 | `		return 1;` |
|    - |  577 | `	}` |
|    - |  578 | `	/* Accept loop */` |
|   14 |  579 | `	while( !g_shutdown ){` |
|   14 |  580 | `		addrLen = sizeof(clientAddr);` |
|   14 |  581 | `		clientSock = PH7_NetAccept(listenSock, (struct sockaddr *)&clientAddr, &addrLen);` |
|   14 |  582 | `		if( clientSock == PH7_NET_INVALID_SOCKET ){` |
|    6 |  583 | `			if( g_shutdown ){` |
|    6 |  584 | `				break;` |
|    - |  585 | `			}` |
|  ! 0 |  586 | `			continue;` |
|    - |  587 | `		}` |
|    - |  588 | `		/* Set a receive timeout so we don't block forever on a slow client */` |
|    8 |  589 | `		PH7_NetSetTimeout(clientSock, 5000);` |
|    - |  590 | `		/* Read the request */` |
|    8 |  591 | `		nRequestLen = ReadRequest(clientSock, zRequestBuf, PHL_MAX_REQUEST);` |
|    8 |  592 | `		if( nRequestLen > 0 ){` |
|    - |  593 | `			char zMethod[16];` |
|    - |  594 | `			char zUri[PHL_MAX_PATH];` |
|    - |  595 | `			/* Extract method and URI for logging */` |
|    8 |  596 | `			ExtractRequestLine(zRequestBuf, zMethod, sizeof(zMethod), zUri, sizeof(zUri));` |
|    - |  597 | `			/* Get client address info */` |
|    8 |  598 | `			PH7_NetAddrToString((struct sockaddr *)&clientAddr, zRemoteAddr, sizeof(zRemoteAddr));` |
|    8 |  599 | `			iRemotePort = PH7_NetAddrPort((struct sockaddr *)&clientAddr);` |
|    - |  600 | `			/* Handle the request */` |
|   12 |  601 | `			rc = HandleRequest(pEngine, clientSock, zDocRoot, zRouter,` |
|    4 |  602 | `						  zHost, iPort, zRequestBuf, nRequestLen,` |
|    4 |  603 | `						  zRemoteAddr, iRemotePort);` |
|    - |  604 | `			/* Log it */` |
|    8 |  605 | `			LogRequest(zRemoteAddr, iRemotePort, zMethod, zUri, rc);` |
|    4 |  606 | `		}` |
|    8 |  607 | `		PH7_NetClose(clientSock);` |
|  ! 0 |  608 | `	}` |
|    - |  609 | `	/* Cleanup */` |
|    6 |  610 | `	fprintf(stderr, "\nShutting down...\n");` |
|    6 |  611 | `	free(zRequestBuf);` |
|    6 |  612 | `	PH7_NetClose(listenSock);` |
|    6 |  613 | `	ph7_release(pEngine);` |
|    6 |  614 | `	PH7_NetCleanup();` |
|    6 |  615 | `	return 0;` |
|    3 |  616 |  |
|    - |  617 |  |
|    - |  618 | `#endif /* PHL_ENABLE_SERVER */` |
|    - |  619 |  |
