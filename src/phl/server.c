/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7.h"
#include "ph7int.h"
#include "server.h"
#if defined(PHL_ENABLE_SERVER) && !defined(PH7_ENABLE_NET)
#error "PHL_ENABLE_SERVER requires PH7_ENABLE_NET"
#endif
#if defined(PHL_ENABLE_SERVER) && defined(PH7_ENABLE_NET)
/*
 * PHL Built-in Development Server.
 * A single-threaded HTTP server for testing, similar to PHP's `php -S`.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef __WINNT__
#include <strings.h>
#endif

#ifdef __WINNT__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#endif

/* Maximum size of a single HTTP request (headers + body) */
#define PHL_MAX_REQUEST  (1024 * 1024)
/* Read buffer size for static files */
#define PHL_FILE_BUF     (64 * 1024)
/* Maximum path length */
#define PHL_MAX_PATH     1024

/* Shutdown flag set by signal handler */
static volatile int g_shutdown = 0;
#ifndef __WINNT__
/* Pre-forked worker pids (parent only) — signalled on shutdown so killing
 * the parent doesn't orphan workers holding the port */
static pid_t g_aWorkerPid[64];
static int g_nWorkerPid = 0;
#endif

/* Resolved interpreter path, exposed to scripts as the PHP_BINARY constant.
 * Set once in phl_serve(); the matching expand callback mirrors the CLI's
 * (PHL_PhpBinaryConst is static to phl.c, so the server keeps its own). */
static const char *g_phpBinaryPath = 0;
static void PhlServerPhpBinaryConst(ph7_value *pVal, void *pUserData)
{
	ph7_value_string(pVal, (const char *)pUserData, -1);
}
/* Define PHP_BINARY on a freshly compiled VM. Constants persist across
 * ph7_vm_reset(), so a cache-owned VM needs this only once, at compile time. */
static void DefinePhpBinary(ph7_vm *pVm)
{
	if( g_phpBinaryPath ){
		ph7_create_constant(pVm, "PHP_BINARY", PhlServerPhpBinaryConst, (void *)g_phpBinaryPath);
	}
}

#ifdef __WINNT__
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
	(void)dwCtrlType;
	g_shutdown = 1;
	return TRUE;
}
#else
static void SignalHandler(int sig)
{
	(void)sig;
	g_shutdown = 1;
}
#endif
/*
 * Install signal handlers for graceful shutdown.
 */
static void InstallSignalHandlers(void)
{
#ifdef __WINNT__
	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#else
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SignalHandler;
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGTERM, &sa, 0);
#endif
}
/*
 * MIME type lookup table.
 */
typedef struct MimeEntry MimeEntry;
struct MimeEntry {
	const char *zExt;
	const char *zType;
};
static const MimeEntry aMime[] = {
	{ ".html", "text/html" },
	{ ".htm",  "text/html" },
	{ ".css",  "text/css" },
	{ ".js",   "application/javascript" },
	{ ".json", "application/json" },
	{ ".xml",  "application/xml" },
	{ ".txt",  "text/plain" },
	{ ".png",  "image/png" },
	{ ".jpg",  "image/jpeg" },
	{ ".jpeg", "image/jpeg" },
	{ ".gif",  "image/gif" },
	{ ".svg",  "image/svg+xml" },
	{ ".ico",  "image/x-icon" },
	{ ".pdf",  "application/pdf" },
	{ ".woff", "font/woff" },
	{ ".woff2","font/woff2" },
	{ ".ttf",  "font/ttf" },
	{ 0, 0 }
};
static const char *GetMimeType(const char *zPath)
{
	const char *zDot;
	int i;
	zDot = strrchr(zPath, '.');
	if( zDot == 0 ){
		return "application/octet-stream";
	}
	for( i = 0; aMime[i].zExt != 0; i++ ){
		if(
#ifdef __WINNT__
			_stricmp(zDot, aMime[i].zExt)
#else
			strcasecmp(zDot, aMime[i].zExt)
#endif
			== 0 ){
			return aMime[i].zType;
		}
	}
	return "application/octet-stream";
}
/*
 * Check if a path contains directory traversal sequences.
 * Returns 1 if the path is safe, 0 if it contains "..".
 */
static int IsPathSafe(const char *zPath)
{
	const char *z = zPath;
	while( *z ){
		if( z[0] == '.' && z[1] == '.' && (z[2] == '/' || z[2] == '\\' || z[2] == 0) ){
			return 0;
		}
		z++;
	}
	return 1;
}
/*
 * Check if a file exists and is a regular file.
 * Returns 1 if it exists, 0 otherwise. Sets *pSize to the file size.
 */
static int FileExists(const char *zPath, long *pSize)
{
#ifdef __WINNT__
	WIN32_FILE_ATTRIBUTE_DATA info;
	if( !GetFileAttributesExA(zPath, GetFileExInfoStandard, &info) ){
		return 0;
	}
	if( info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
		return 0;
	}
	if( pSize ){
		*pSize = (long)info.nFileSizeLow;
	}
	return 1;
#else
	struct stat st;
	if( stat(zPath, &st) != 0 ){
		return 0;
	}
	if( !S_ISREG(st.st_mode) ){
		return 0;
	}
	if( pSize ){
		*pSize = (long)st.st_size;
	}
	return 1;
#endif
}
/*
 * Check if a path is a directory.
 */
static int IsDirectory(const char *zPath)
{
#ifdef __WINNT__
	DWORD attr = GetFileAttributesA(zPath);
	return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
	struct stat st;
	if( stat(zPath, &st) != 0 ){
		return 0;
	}
	return S_ISDIR(st.st_mode);
#endif
}
/*
 * Read the full HTTP request from a socket into a buffer.
 * Reads headers first (until \r\n\r\n), then reads the body
 * based on Content-Length if present.
 * Returns total bytes read, or -1 on error.
 */
static int ReadRequest(ph7_socket sock, char *zBuf, int nBufSize)
{
	int nTotal = 0;
	int nRead;
	char *zHeaderEnd;
	/* Read data until we find the end of headers */
	while( nTotal < nBufSize - 1 ){
		nRead = PH7_NetRecv(sock, zBuf + nTotal, nBufSize - 1 - nTotal, 0);
		if( nRead <= 0 ){
			if( nTotal == 0 ){
				return -1;
			}
			break;
		}
		nTotal += nRead;
		zBuf[nTotal] = 0;
		/* Check if we have the complete headers */
		zHeaderEnd = strstr(zBuf, "\r\n\r\n");
		if( zHeaderEnd ){
			int nHeaderLen = (int)(zHeaderEnd - zBuf) + 4;
			/* Look for Content-Length */
			const char *zCL = strstr(zBuf, "Content-Length:");
			if( zCL == 0 ){
				zCL = strstr(zBuf, "content-length:");
			}
			if( zCL ){
				int nContentLen = atoi(zCL + 15);
				int nExpected = nHeaderLen + nContentLen;
				if( nExpected > nBufSize - 1 ){
					nExpected = nBufSize - 1;
				}
				/* Read remaining body bytes */
				while( nTotal < nExpected ){
					nRead = PH7_NetRecv(sock, zBuf + nTotal, nExpected - nTotal, 0);
					if( nRead <= 0 ){
						break;
					}
					nTotal += nRead;
				}
			}
			break;
		}
	}
	zBuf[nTotal] = 0;
	return nTotal;
}
/*
 * Send an HTTP response with headers.
 */
static void SendResponse(ph7_socket sock, int iStatus, const char *zStatus,
						 const char *zContentType, const void *pBody, int nBodyLen)
{
	char zHeader[512];
	int nHeader;
	nHeader = snprintf(zHeader, sizeof(zHeader),
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"Connection: close\r\n"
		"Server: PHL/" PH7_VERSION "\r\n"
		"\r\n",
		iStatus, zStatus, zContentType, nBodyLen);
	PH7_NetSendAll(sock, zHeader, nHeader);
	if( pBody && nBodyLen > 0 ){
		PH7_NetSendAll(sock, pBody, nBodyLen);
	}
}
/*
 * Send an HTTP error response.
 */
static void SendError(ph7_socket sock, int iStatus, const char *zStatus)
{
	char zBody[256];
	int nBody;
	nBody = snprintf(zBody, sizeof(zBody),
		"<html><head><title>%d %s</title></head>"
		"<body><h1>%d %s</h1></body></html>",
		iStatus, zStatus, iStatus, zStatus);
	SendResponse(sock, iStatus, zStatus, "text/html", zBody, nBody);
}
/*
 * Map a status code to a reason phrase.
 */
static const char *StatusReason(int iStatus)
{
	switch( iStatus ){
	case 200: return "OK";
	case 201: return "Created";
	case 204: return "No Content";
	case 301: return "Moved Permanently";
	case 302: return "Found";
	case 304: return "Not Modified";
	case 400: return "Bad Request";
	case 401: return "Unauthorized";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 500: return "Internal Server Error";
	case 502: return "Bad Gateway";
	case 503: return "Service Unavailable";
	default:  return "";
	}
}
/*
 * Send an HTTP response using the VM's response headers and status code.
 * Falls back to Content-Type: text/html if the script didn't set one.
 */
/*
 * State passed to the response header callback.
 */
typedef struct VmResponseCtx VmResponseCtx;
struct VmResponseCtx {
	ph7_socket sock;
	int bHasContentType;
};
static int VmResponseHeaderCB(const char *zName, unsigned int nName,
							   const char *zValue, unsigned int nValue,
							   void *pUserData)
{
	VmResponseCtx *pCtx = (VmResponseCtx *)pUserData;
	char zLine[512];
	int nLine;
	nLine = snprintf(zLine, sizeof(zLine), "%.*s: %.*s\r\n",
		(int)nName, zName, (int)nValue, zValue);
	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);
	PH7_NetSendAll(pCtx->sock, zLine, nLine);
	if( nName == 12 && SyStrnicmp(zName, "Content-Type", 12) == 0 ){
		pCtx->bHasContentType = 1;
	}
	return PH7_OK;
}
static void SendVmResponse(ph7_socket sock, ph7_vm *pVm,
							const void *pBody, int nBodyLen)
{
	int iStatus;
	VmResponseCtx sCtx;
	char zLine[512];
	int nLine;
	iStatus = 200;
	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_STATUS, &iStatus);
	/* Status line */
	nLine = snprintf(zLine, sizeof(zLine), "HTTP/1.1 %d %s\r\n", iStatus, StatusReason(iStatus));
	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);
	PH7_NetSendAll(sock, zLine, nLine);
	/* Script-set headers via callback */
	sCtx.sock = sock;
	sCtx.bHasContentType = 0;
	ph7_vm_config(pVm, PH7_VM_CONFIG_RESPONSE_HEADERS, VmResponseHeaderCB, &sCtx);
	/* Default Content-Type if not set by the script */
	if( !sCtx.bHasContentType ){
		PH7_NetSendAll(sock, "Content-Type: text/html\r\n", 25);
	}
	/* Standard headers */
	nLine = snprintf(zLine, sizeof(zLine),
		"Content-Length: %d\r\n"
		"Connection: close\r\n"
		"Server: PHL/" PH7_VERSION "\r\n"
		"\r\n",
		nBodyLen);
	if( nLine > (int)sizeof(zLine) ) nLine = (int)sizeof(zLine);
	PH7_NetSendAll(sock, zLine, nLine);
	/* Body */
	if( pBody && nBodyLen > 0 ){
		PH7_NetSendAll(sock, pBody, nBodyLen);
	}
}
/*
 * Serve a static file.
 */
static void ServeStaticFile(ph7_socket sock, const char *zPath, long nFileSize)
{
	const char *zMime;
	char zHeader[512];
	char zFileBuf[PHL_FILE_BUF];
	FILE *pFile;
	int nHeader;
	size_t nRead;
	zMime = GetMimeType(zPath);
#ifdef __WINNT__
	if( fopen_s(&pFile, zPath, "rb") != 0 ) pFile = 0;
#else
	pFile = fopen(zPath, "rb");
#endif
	if( pFile == 0 ){
		SendError(sock, 500, "Internal Server Error");
		return;
	}
	nHeader = snprintf(zHeader, sizeof(zHeader),
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %ld\r\n"
		"Connection: close\r\n"
		"Server: PHL/" PH7_VERSION "\r\n"
		"\r\n",
		zMime, nFileSize);
	PH7_NetSendAll(sock, zHeader, nHeader);
	while( (nRead = fread(zFileBuf, 1, sizeof(zFileBuf), pFile)) > 0 ){
		if( PH7_NetSendAll(sock, zFileBuf, (int)nRead) != PH7_OK ){
			break;
		}
	}
	fclose(pFile);
}
/*
 * Extract the request method and URI path from the first line of the HTTP request.
 * E.g., "GET /foo/bar?q=1 HTTP/1.1\r\n..."
 * Writes the method to zMethod (up to nMethodSize) and path to zPath (up to nPathSize).
 * The path is the URI without the query string.
 */
static void ExtractRequestLine(const char *zRequest, char *zMethod, int nMethodSize,
							   char *zPath, int nPathSize)
{
	const char *z = zRequest;
	int i;
	/* Extract method */
	for( i = 0; *z && *z != ' ' && i < nMethodSize - 1; z++, i++ ){
		zMethod[i] = *z;
	}
	zMethod[i] = 0;
	/* Skip space */
	while( *z == ' ' ) z++;
	/* Extract path (stop at '?', ' ', or end) */
	for( i = 0; *z && *z != '?' && *z != ' ' && *z != '\r' && i < nPathSize - 1; z++, i++ ){
		zPath[i] = *z;
	}
	zPath[i] = 0;
}
/*
 * Resolve a request URI path to a filesystem path under the document root.
 * Returns 1 on success, 0 on failure (bad path).
 * zOut must be at least PHL_MAX_PATH bytes.
 */
static int ResolvePath(const char *zDocRoot, const char *zUri, char *zOut)
{
	if( !IsPathSafe(zUri) ){
		return 0;
	}
	snprintf(zOut, PHL_MAX_PATH, "%s%s", zDocRoot, zUri);
	/* If path points to a directory, try index.php */
	if( IsDirectory(zOut) ){
		int n = (int)strlen(zOut);
		/* Ensure trailing slash */
		if( n > 0 && zOut[n-1] != '/' && zOut[n-1] != '\\' ){
			if( n < PHL_MAX_PATH - 1 ){
				zOut[n] = '/';
				zOut[n+1] = 0;
				n++;
			}
		}
		snprintf(zOut + n, PHL_MAX_PATH - n, "index.php");
	}
	return 1;
}
/*
 * Check if a filename ends with ".php" (case-insensitive).
 */
static int IsPhpFile(const char *zPath)
{
	int n = (int)strlen(zPath);
	if( n < 4 ){
		return 0;
	}
	return (
#ifdef __WINNT__
		_stricmp(zPath + n - 4, ".php")
#else
		strcasecmp(zPath + n - 4, ".php")
#endif
		== 0);
}
/*
 * Return a file's last-modification time as a comparable integer (0 on error).
 * Used to invalidate the compiled-VM cache when a script is edited, preserving
 * the dev-server expectation that reloading picks up source changes.
 */
static long long GetFileMtime(const char *zPath)
{
#ifdef __WINNT__
	WIN32_FILE_ATTRIBUTE_DATA info;
	if( !GetFileAttributesExA(zPath, GetFileExInfoStandard, &info) ){
		return 0;
	}
	return ((long long)info.ftLastWriteTime.dwHighDateTime << 32)
		| (long long)info.ftLastWriteTime.dwLowDateTime;
#else
	struct stat st;
	if( stat(zPath, &st) != 0 ){
		return 0;
	}
	return (long long)st.st_mtime;
#endif
}
/* ---- Compiled-VM reuse cache ------------------------------------------------
 * Compilation dominates the per-request cost (the on-hardware profile measured
 * 167 of 197 ms/request in compile). We therefore compile each script once and
 * re-execute it per request, calling ph7_vm_reset() in between to clear all
 * per-execution state (globals, superglobals, statics, output, ...). The cache
 * is keyed by the resolved filesystem path and is safe without locking because
 * the dev server is single-threaded. Set PHL_NO_REUSE=1 to fall back to the old
 * compile-per-request behaviour (useful for diffing). */
#define PHL_VM_CACHE_SIZE 16
typedef struct PhlVmCacheEntry {
	char zPath[PHL_MAX_PATH]; /* Resolved script path ("" = empty slot) */
	ph7_vm *pVm;              /* Compiled, reusable VM */
	unsigned long nUse;       /* Last-use clock for LRU eviction */
	long long nMtime;         /* Source mtime when compiled (cache invalidation) */
} PhlVmCacheEntry;
static PhlVmCacheEntry g_vmCache[PHL_VM_CACHE_SIZE];
static unsigned long g_vmCacheClock = 0;
static int g_vmReuse = 1;

/*
 * Acquire a ready-to-execute VM for zPath. On a cache hit the VM is reset and
 * reused; on a miss it is compiled and cached (evicting the least-recently-used
 * entry when full). Returns NULL on compile error. On success *pbCached tells
 * the caller whether the VM is cache-owned (do NOT release it) or a throwaway
 * (release after use, e.g. when reuse is disabled).
 */
static ph7_vm *AcquireScriptVm(ph7 *pEngine, const char *zPath, int *pbCached)
{
	int i, iFree = -1, iLru = -1;
	ph7_vm *pVm = 0;
	long long nMtime;
	if( !g_vmReuse ){
		*pbCached = 0;
		if( ph7_compile_file(pEngine, zPath, &pVm, 0) != PH7_OK ){
			return 0;
		}
		DefinePhpBinary(pVm);
		return pVm;
	}
	nMtime = GetFileMtime(zPath);
	for( i = 0 ; i < PHL_VM_CACHE_SIZE ; i++ ){
		if( g_vmCache[i].pVm == 0 ){
			if( iFree < 0 ){ iFree = i; }
			continue;
		}
		if( strcmp(g_vmCache[i].zPath, zPath) == 0 ){
			/* Hit: reuse only if the source is unchanged on disk (dev-server
			 * live-edit) and the reset succeeds; otherwise drop and recompile. */
			if( g_vmCache[i].nMtime == nMtime
			 && ph7_vm_reset(g_vmCache[i].pVm) == PH7_OK ){
				g_vmCache[i].nUse = ++g_vmCacheClock;
				*pbCached = 1;
				return g_vmCache[i].pVm;
			}
			ph7_vm_release(g_vmCache[i].pVm);
			g_vmCache[i].pVm = 0;
			g_vmCache[i].zPath[0] = '\0';
			iFree = i;
			break;
		}
		if( iLru < 0 || g_vmCache[i].nUse < g_vmCache[iLru].nUse ){
			iLru = i;
		}
	}
	/* Miss: compile a fresh VM. */
	if( ph7_compile_file(pEngine, zPath, &pVm, 0) != PH7_OK ){
		*pbCached = 0;
		return 0;
	}
	DefinePhpBinary(pVm);
	if( iFree < 0 ){
		/* Cache full: evict the least-recently-used entry. */
		ph7_vm_release(g_vmCache[iLru].pVm);
		iFree = iLru;
	}
	snprintf(g_vmCache[iFree].zPath, sizeof(g_vmCache[iFree].zPath), "%s", zPath);
	g_vmCache[iFree].pVm = pVm;
	g_vmCache[iFree].nUse = ++g_vmCacheClock;
	g_vmCache[iFree].nMtime = nMtime;
	*pbCached = 1;
	return pVm;
}
/*
 * Release every cached VM. Called at server shutdown.
 */
static void ReleaseVmCache(void)
{
	int i;
	for( i = 0 ; i < PHL_VM_CACHE_SIZE ; i++ ){
		if( g_vmCache[i].pVm ){
			ph7_vm_release(g_vmCache[i].pVm);
			g_vmCache[i].pVm = 0;
			g_vmCache[i].zPath[0] = '\0';
		}
	}
}
/*
 * Execute a PHP script and send its output as an HTTP response.
 * pEngine is the shared engine instance.
 * zFilePath is the resolved filesystem path to the PHP file.
 * zRawRequest/nRequestLen is the raw HTTP request for superglobal population.
 * The additional server attributes are set from the request context.
 */
static void ExecutePhpScript(ph7 *pEngine, ph7_socket client,
							 const char *zFilePath, const char *zScriptName,
							 const char *zRawRequest, int nRequestLen,
							 const char *zHost, int iPort, const char *zDocRoot,
							 const char *zRemoteAddr, int iRemotePort)
{
	ph7_vm *pVm = 0;
	const void *pOutput;
	unsigned int nOutputLen;
	char zPortBuf[16];
	char zRemotePortBuf[16];
	int bCached = 0;
	pVm = AcquireScriptVm(pEngine, zFilePath, &bCached);
	if( pVm == 0 ){
		SendError(client, 500, "Internal Server Error");
		return;
	}
	/* Feed the raw HTTP request to populate $_SERVER, $_GET, $_POST, etc. */
	ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);
	/* Set additional $_SERVER attributes */
	snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);
	snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);
	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);
	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);
	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);
	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);
	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zFilePath, -1);
	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zScriptName, -1);
	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);
	ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);
	ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);
	/* Execute the script (output accumulates in the VM's internal buffer) */
	ph7_vm_exec(pVm, 0);
	/* Extract accumulated output */
	pOutput = 0;
	nOutputLen = 0;
	ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);
	/* Send the response using VM-set headers and status code */
	SendVmResponse(client, pVm, pOutput, (int)nOutputLen);
	/* Cache-owned VMs are kept for reuse; throwaways are released. */
	if( !bCached ){
		ph7_vm_release(pVm);
	}
}
/*
 * Handle a single HTTP request.
 */
static int HandleRequest(ph7 *pEngine, ph7_socket client,
						  const char *zDocRoot, const char *zRouter,
						  const char *zHost, int iPort,
						  const char *zRawRequest, int nRequestLen,
						  const char *zRemoteAddr, int iRemotePort)
{
	char zMethod[16];
	char zUri[PHL_MAX_PATH];
	char zFilePath[PHL_MAX_PATH];
	long nFileSize;
	/* Extract request line */
	ExtractRequestLine(zRawRequest, zMethod, sizeof(zMethod), zUri, sizeof(zUri));
	(void)zMethod;
	/* Try router script first (if configured) */
	if( zRouter && zRouter[0] ){
		ph7_vm *pVm = 0;
		ph7_value *pRetVal;
		int bCached = 0;
		char zPortBuf[16];
		char zRemotePortBuf[16];
		pVm = AcquireScriptVm(pEngine, zRouter, &bCached);
		if( pVm != 0 ){
			ph7_vm_config(pVm, PH7_VM_CONFIG_HTTP_REQUEST, zRawRequest, nRequestLen);
			snprintf(zPortBuf, sizeof(zPortBuf), "%d", iPort);
			snprintf(zRemotePortBuf, sizeof(zRemotePortBuf), "%d", iRemotePort);
			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_SOFTWARE", "PHL/" PH7_VERSION " Development Server", -1);
			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_NAME", zHost, -1);
			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SERVER_PORT", zPortBuf, -1);
			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "DOCUMENT_ROOT", zDocRoot, -1);
			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_FILENAME", zRouter, -1);
			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "SCRIPT_NAME", zUri, -1);
			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_ADDR", zRemoteAddr, -1);
			ph7_vm_config(pVm, PH7_VM_CONFIG_SERVER_ATTR, "REMOTE_PORT", zRemotePortBuf, -1);
			ph7_vm_config(pVm, PH7_VM_CONFIG_ERR_REPORT);
			ph7_vm_exec(pVm, 0);
			/* Check if the router returned false (meaning: fall through to default serving) */
			pRetVal = 0;
			ph7_vm_config(pVm, PH7_VM_CONFIG_EXEC_VALUE, &pRetVal);
			if( pRetVal && ph7_value_is_bool(pRetVal) && !ph7_value_to_bool(pRetVal) ){
				/* Router returned false: fall through to default file serving */
				if( !bCached ){
					ph7_vm_release(pVm);
				}
			}else{
				/* Router handled the request: send its output */
				const void *pOutput;
				unsigned int nOutputLen;
				pOutput = 0;
				nOutputLen = 0;
				ph7_vm_config(pVm, PH7_VM_CONFIG_EXTRACT_OUTPUT, &pOutput, &nOutputLen);
				SendVmResponse(client, pVm, pOutput, (int)nOutputLen);
				if( !bCached ){
					ph7_vm_release(pVm);
				}
				return 200;
			}
		}
		/* Router compile failed or returned false: fall through */
	}
	/* Resolve URI to filesystem path */
	if( !ResolvePath(zDocRoot, zUri, zFilePath) ){
		SendError(client, 403, "Forbidden");
		return 403;
	}
	/* Check if file exists */
	if( FileExists(zFilePath, &nFileSize) ){
		if( IsPhpFile(zFilePath) ){
			ExecutePhpScript(pEngine, client, zFilePath, zUri,
							 zRawRequest, nRequestLen,
							 zHost, iPort, zDocRoot,
							 zRemoteAddr, iRemotePort);
		}else{
			ServeStaticFile(client, zFilePath, nFileSize);
		}
		return 200;
	}
	SendError(client, 404, "Not Found");
	return 404;
}
/*
 * Log a request line to stderr.
 */
static void LogRequest(const char *zRemoteAddr, int iRemotePort,
					   const char *zMethod, const char *zUri, int iStatus)
{
	time_t now;
	struct tm tm_buf;
	char zTime[64];
	time(&now);
#ifdef __WINNT__
	localtime_s(&tm_buf, &now);
#else
	localtime_r(&now, &tm_buf);
#endif
	strftime(zTime, sizeof(zTime), "%a %b %d %H:%M:%S %Y", &tm_buf);
	fprintf(stderr, "[%s] %s:%d [%d]: %s %s\n",
		zTime, zRemoteAddr, iRemotePort, iStatus, zMethod, zUri);
}
/*
 * Main server entry point.
 */
int phl_serve(const char *zHost, int iPort, const char *zDocRoot, const char *zRouter, const char *zBinaryPath)
{
	ph7 *pEngine;
	ph7_socket listenSock;
	ph7_socket clientSock;
	struct sockaddr_in clientAddr;
	ph7_socklen addrLen;
	char *zRequestBuf;
	int nRequestLen;
	char zRemoteAddr[64];
	int iRemotePort;
	int rc;
	/* Resolved interpreter path → PHP_BINARY for every served VM. */
	g_phpBinaryPath = zBinaryPath;
	/* Compile-once / reuse is on by default; PHL_NO_REUSE=1 forces the legacy
	 * compile-per-request path for behaviour diffing. */
	{
		const char *zNoReuse = getenv("PHL_NO_REUSE");
		if( zNoReuse && zNoReuse[0] && zNoReuse[0] != '0' ){
			g_vmReuse = 0;
		}
	}
	/* Initialize networking */
	rc = PH7_NetInit();
	if( rc != PH7_OK ){
		fprintf(stderr, "Error: Failed to initialize networking\n");
		return 1;
	}
	/* Create the PH7 engine (shared across requests) */
	rc = ph7_init(&pEngine);
	if( rc != PH7_OK ){
		fprintf(stderr, "Error: Failed to initialize PH7 engine\n");
		PH7_NetCleanup();
		return 1;
	}
	/* Create listening socket */
	listenSock = PH7_NetListen(zHost, iPort, 10);
	if( listenSock == PH7_NET_INVALID_SOCKET ){
		fprintf(stderr, "Error: Failed to listen on %s:%d\n", zHost, iPort);
		ph7_release(pEngine);
		PH7_NetCleanup();
		return 1;
	}
	/* Install signal handlers */
	InstallSignalHandlers();
	/* Print banner */
	fprintf(stderr, "PHL %s Development Server started at http://%s:%d/\n", PH7_VERSION, zHost, iPort);
	fprintf(stderr, "Document root: %s\n", zDocRoot);
	if( zRouter ){
		fprintf(stderr, "Router script: %s\n", zRouter);
	}
	fprintf(stderr, "Press Ctrl+C to stop.\n");
#ifndef __WINNT__
	/* php CLI-server parity: PHP_CLI_SERVER_WORKERS=N pre-forks N workers
	 * that all accept() on the shared listen socket (the kernel load-balances
	 * connections). Each worker keeps its own warm VM cache and runs the
	 * unchanged sequential loop, so no locking is needed anywhere. Like php,
	 * the default (unset/1) stays single-process; Windows keeps the
	 * single-process model (no fork — recorded). */
	{
		const char *zWorkers = getenv("PHP_CLI_SERVER_WORKERS");
		int nWorkers = zWorkers ? atoi(zWorkers) : 0;
		if( nWorkers > 1 ){
			int iWorker;
			int bChild = 0;
			if( nWorkers > 64 ){
				nWorkers = 64;
			}
			/* No zombies if a worker crashes mid-run */
			signal(SIGCHLD, SIG_IGN);
			for( iWorker = 1 ; iWorker < nWorkers ; iWorker++ ){
				pid_t pid = fork();
				if( pid == 0 ){
					/* worker child: fall through to the accept loop */
					bChild = 1;
					g_nWorkerPid = 0; /* children propagate nothing */
					break;
				}
				if( pid < 0 ){
					fprintf(stderr, "Warning: fork failed; continuing with %d worker(s)\n", iWorker);
					break;
				}
				g_aWorkerPid[g_nWorkerPid++] = pid;
			}
			if( !bChild ){
				fprintf(stderr, "Using %d worker processes\n", nWorkers);
			}
		}
	}
#endif
	/* Allocate request buffer */
	zRequestBuf = (char *)malloc(PHL_MAX_REQUEST);
	if( zRequestBuf == 0 ){
		fprintf(stderr, "Error: Out of memory\n");
		PH7_NetClose(listenSock);
		ph7_release(pEngine);
		PH7_NetCleanup();
		return 1;
	}
	/* Accept loop */
	while( !g_shutdown ){
		addrLen = sizeof(clientAddr);
		clientSock = PH7_NetAccept(listenSock, (struct sockaddr *)&clientAddr, &addrLen);
		if( clientSock == PH7_NET_INVALID_SOCKET ){
			if( g_shutdown ){
				break;
			}
			continue;
		}
		/* Set a receive timeout so we don't block forever on a slow client */
		PH7_NetSetTimeout(clientSock, 5000);
		/* Read the request */
		nRequestLen = ReadRequest(clientSock, zRequestBuf, PHL_MAX_REQUEST);
		if( nRequestLen > 0 ){
			char zMethod[16];
			char zUri[PHL_MAX_PATH];
			/* Extract method and URI for logging */
			ExtractRequestLine(zRequestBuf, zMethod, sizeof(zMethod), zUri, sizeof(zUri));
			/* Get client address info */
			PH7_NetAddrToString((struct sockaddr *)&clientAddr, zRemoteAddr, sizeof(zRemoteAddr));
			iRemotePort = PH7_NetAddrPort((struct sockaddr *)&clientAddr);
			/* Handle the request */
			rc = HandleRequest(pEngine, clientSock, zDocRoot, zRouter,
						  zHost, iPort, zRequestBuf, nRequestLen,
						  zRemoteAddr, iRemotePort);
			/* Log it */
			LogRequest(zRemoteAddr, iRemotePort, zMethod, zUri, rc);
		}
		PH7_NetClose(clientSock);
	}
	/* Cleanup */
#ifndef __WINNT__
	/* Parent: take the workers down with us */
	{
		int k;
		for( k = 0 ; k < g_nWorkerPid ; k++ ){
			kill(g_aWorkerPid[k], SIGTERM);
		}
	}
#endif
	fprintf(stderr, "\nShutting down...\n");
	ReleaseVmCache();
	free(zRequestBuf);
	PH7_NetClose(listenSock);
	ph7_release(pEngine);
	PH7_NetCleanup();
	return 0;
}

#endif /* PHL_ENABLE_SERVER */
