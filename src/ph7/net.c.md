# src/ph7/net.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 81/141 lines (57.45%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#ifdef PH7_ENABLE_NET` |
|    - |    7 | `/*` |
|    - |    8 | ` * Cross-platform socket abstraction layer.` |
|    - |    9 | ` * Provides a thin wrapper over POSIX sockets (Unix) and Winsock2 (Windows).` |
|    - |   10 | ` * Guarded by PH7_ENABLE_NET so it compiles to nothing in tiny builds.` |
|    - |   11 | ` */` |
|    - |   12 | `#include <string.h>` |
|    - |   13 | `#include <stdio.h>` |
|    - |   14 |  |
|    - |   15 | `#ifdef __WINNT__` |
|    - |   16 | `#include <winsock2.h>` |
|    - |   17 | `#include <ws2tcpip.h>` |
|    - |   18 | `#pragma comment(lib, "ws2_32.lib")` |
|    - |   19 | `#else` |
|    - |   20 | `#include <sys/types.h>` |
|    - |   21 | `#include <sys/socket.h>` |
|    - |   22 | `#include <netinet/in.h>` |
|    - |   23 | `#include <arpa/inet.h>` |
|    - |   24 | `#include <netdb.h>` |
|    - |   25 | `#include <unistd.h>` |
|    - |   26 | `#include <signal.h>` |
|    - |   27 | `#include <errno.h>` |
|    - |   28 | `#include <fcntl.h>` |
|    - |   29 | `#endif` |
|    - |   30 |  |
|    - |   31 | `/*` |
|    - |   32 | ` * Initialize the networking subsystem.` |
|    - |   33 | ` * On Windows, calls WSAStartup(). On Unix, ignores SIGPIPE.` |
|    - |   34 | ` * Returns PH7_OK on success.` |
|    - |   35 | ` */` |
|   26 |   36 | `PH7_PRIVATE int PH7_NetInit(void)` |
|  ! 0 |   37 | `{` |
|    - |   38 | `#ifdef __WINNT__` |
|    - |   39 | `	WSADATA wsaData;` |
|  ! 0 |   40 | `	if( WSAStartup(MAKEWORD(2,2), &wsaData) != 0 ){` |
|  ! 0 |   41 | `		return PH7_IO_ERR;` |
|    - |   42 | `	}` |
|    - |   43 | `#else` |
|   26 |   44 | `	signal(SIGPIPE, SIG_IGN);` |
|    - |   45 | `#endif` |
|   26 |   46 | `	return PH7_OK;` |
|  ! 0 |   47 | `}` |
|    - |   48 | `/*` |
|    - |   49 | ` * Cleanup the networking subsystem.` |
|    - |   50 | ` */` |
|   32 |   51 | `PH7_PRIVATE void PH7_NetCleanup(void)` |
|  ! 0 |   52 | `{` |
|    - |   53 | `#ifdef __WINNT__` |
|  ! 0 |   54 | `	WSACleanup();` |
|    - |   55 | `#endif` |
|   32 |   56 | `}` |
|    - |   57 | `/*` |
|    - |   58 | ` * Create a TCP listening socket bound to the given host and port.` |
|    - |   59 | ` * Returns the socket descriptor, or PH7_NET_INVALID_SOCKET on error.` |
|    - |   60 | ` */` |
|   26 |   61 | `PH7_PRIVATE ph7_socket PH7_NetListen(const char *zHost, int iPort, int iBacklog)` |
|  ! 0 |   62 | `{` |
|    - |   63 | `	struct sockaddr_in addr;` |
|    - |   64 | `	ph7_socket sock;` |
|   26 |   65 | `	int on = 1;` |
|   26 |   66 | `	memset(&addr, 0, sizeof(addr));` |
|   26 |   67 | `	addr.sin_family = AF_INET;` |
|   26 |   68 | `	addr.sin_port = htons((unsigned short)iPort);` |
|   26 |   69 | `	if( zHost == 0 \|\| zHost[0] == 0 \|\| strcmp(zHost, "0.0.0.0") == 0 ){` |
|  ! 0 |   70 | `		addr.sin_addr.s_addr = htonl(INADDR_ANY);` |
|   26 |   71 | `	}else if( strcmp(zHost, "localhost") == 0 \|\| strcmp(zHost, "127.0.0.1") == 0 ){` |
|   26 |   72 | `		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);` |
|   13 |   73 | `	}else{` |
|    - |   74 | `		struct addrinfo hints, *res;` |
|  ! 0 |   75 | `		memset(&hints, 0, sizeof(hints));` |
|  ! 0 |   76 | `		hints.ai_family = AF_INET;` |
|  ! 0 |   77 | `		hints.ai_socktype = SOCK_STREAM;` |
|  ! 0 |   78 | `		if( getaddrinfo(zHost, 0, &hints, &res) != 0 \|\| res == 0 ){` |
|  ! 0 |   79 | `			return PH7_NET_INVALID_SOCKET;` |
|    - |   80 | `		}` |
|  ! 0 |   81 | `		addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;` |
|  ! 0 |   82 | `		freeaddrinfo(res);` |
|    - |   83 | `	}` |
|   26 |   84 | `	sock = socket(AF_INET, SOCK_STREAM, 0);` |
|   26 |   85 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |   86 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |   87 | `	}` |
|   26 |   88 | `	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));` |
|   26 |   89 | `	if( bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0 ){` |
|  ! 0 |   90 | `		PH7_NetClose(sock);` |
|  ! 0 |   91 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |   92 | `	}` |
|   26 |   93 | `	if( listen(sock, iBacklog) != 0 ){` |
|  ! 0 |   94 | `		PH7_NetClose(sock);` |
|  ! 0 |   95 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |   96 | `	}` |
|   26 |   97 | `	return sock;` |
|   13 |   98 | `}` |
|    - |   99 | `/*` |
|    - |  100 | ` * Connect a TCP socket to the given host and port (blocking; the caller sets` |
|    - |  101 | ` * a timeout with PH7_NetSetTimeout afterwards). *pErrno receives the OS error` |
|    - |  102 | ` * code and *pzErr a static description on failure, matching what fsockopen()` |
|    - |  103 | ` * reports through its by-ref out-params.` |
|    - |  104 | ` * Returns the connected socket, or PH7_NET_INVALID_SOCKET on error.` |
|    - |  105 | ` */` |
|    6 |  106 | `PH7_PRIVATE ph7_socket PH7_NetConnect(const char *zHost, int iPort, int iTimeoutMs,` |
|    - |  107 | `	int *pErrno, const char **pzErr)` |
|  ! 0 |  108 | `{` |
|    6 |  109 | `	struct addrinfo hints, *res = 0, *rp;` |
|    - |  110 | `	char zPort[16];` |
|    6 |  111 | `	ph7_socket sock = PH7_NET_INVALID_SOCKET;` |
|    6 |  112 | `	if( pErrno ){ *pErrno = 0; }` |
|    6 |  113 | `	if( pzErr ){ *pzErr = ""; }` |
|    6 |  114 | `	if( zHost == 0 \|\| zHost[0] == 0 ){` |
|  ! 0 |  115 | `		if( pErrno ){ *pErrno = -1; }` |
|  ! 0 |  116 | `		if( pzErr ){ *pzErr = "Empty host"; }` |
|  ! 0 |  117 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  118 | `	}` |
|    6 |  119 | `	snprintf(zPort, sizeof(zPort), "%d", iPort);` |
|    6 |  120 | `	memset(&hints, 0, sizeof(hints));` |
|    6 |  121 | `	hints.ai_family = AF_UNSPEC;` |
|    6 |  122 | `	hints.ai_socktype = SOCK_STREAM;` |
|    6 |  123 | `	if( getaddrinfo(zHost, zPort, &hints, &res) != 0 \|\| res == 0 ){` |
|  ! 0 |  124 | `		if( pErrno ){ *pErrno = -3; }` |
|  ! 0 |  125 | `		if( pzErr ){ *pzErr = "php_network_getaddresses: getaddrinfo failed: Name or service not known"; }` |
|  ! 0 |  126 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  127 | `	}` |
|    8 |  128 | `	for( rp = res ; rp != 0 ; rp = rp->ai_next ){` |
|    6 |  129 | `		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);` |
|    6 |  130 | `		if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  131 | `			continue;` |
|    - |  132 | `		}` |
|    6 |  133 | `		if( iTimeoutMs > 0 ){` |
|    - |  134 | `			/* the connect() itself stays blocking; the timeout bounds the` |
|    - |  135 | `			 * subsequent recv/send (php applies it to both — recorded) */` |
|    6 |  136 | `			PH7_NetSetTimeout(sock, iTimeoutMs);` |
|    3 |  137 | `		}` |
|    6 |  138 | `		if( connect(sock, rp->ai_addr, (ph7_socklen)rp->ai_addrlen) == 0 ){` |
|    4 |  139 | `			freeaddrinfo(res);` |
|    4 |  140 | `			return sock;` |
|    - |  141 | `		}` |
|    2 |  142 | `		PH7_NetClose(sock);` |
|    2 |  143 | `		sock = PH7_NET_INVALID_SOCKET;` |
|    1 |  144 | `	}` |
|    2 |  145 | `	freeaddrinfo(res);` |
|    2 |  146 | `	if( pErrno ){ *pErrno = 111; }` |
|    2 |  147 | `	if( pzErr ){ *pzErr = "Connection refused"; }` |
|    2 |  148 | `	return PH7_NET_INVALID_SOCKET;` |
|    3 |  149 | `}` |
|    - |  150 | `/*` |
|    - |  151 | ` * Accept an incoming connection on a listening socket.` |
|    - |  152 | ` * If pAddr and pAddrLen are non-NULL, the client address is stored there.` |
|    - |  153 | ` * Returns the client socket, or PH7_NET_INVALID_SOCKET on error.` |
|    - |  154 | ` */` |
|   70 |  155 | `PH7_PRIVATE ph7_socket PH7_NetAccept(ph7_socket listenSock, struct sockaddr *pAddr, ph7_socklen *pAddrLen)` |
|  ! 0 |  156 | `{` |
|   70 |  157 | `	return accept(listenSock, pAddr, pAddrLen);` |
|  ! 0 |  158 | `}` |
|    - |  159 | `/*` |
|    - |  160 | ` * Receive data from a socket.` |
|    - |  161 | ` * Returns the number of bytes received, or -1 on error.` |
|    - |  162 | ` */` |
|   50 |  163 | `PH7_PRIVATE int PH7_NetRecv(ph7_socket sock, void *pBuf, int nLen, int flags)` |
|  ! 0 |  164 | `{` |
|   50 |  165 | `	return (int)recv(sock, (char *)pBuf, nLen, flags);` |
|  ! 0 |  166 | `}` |
|    - |  167 | `/*` |
|    - |  168 | ` * Send data on a socket.` |
|    - |  169 | ` * Returns the number of bytes sent, or -1 on error.` |
|    - |  170 | ` */` |
|  ! 0 |  171 | `PH7_PRIVATE int PH7_NetSend(ph7_socket sock, const void *pBuf, int nLen, int flags)` |
|  ! 0 |  172 | `{` |
|  ! 0 |  173 | `	return (int)send(sock, (const char *)pBuf, nLen, flags);` |
|  ! 0 |  174 | `}` |
|    - |  175 | `/*` |
|    - |  176 | ` * Send all data on a socket, retrying on partial writes.` |
|    - |  177 | ` * Returns PH7_OK on success, PH7_IO_ERR on error.` |
|    - |  178 | ` */` |
|  154 |  179 | `PH7_PRIVATE int PH7_NetSendAll(ph7_socket sock, const void *pBuf, int nLen)` |
|  ! 0 |  180 | `{` |
|  154 |  181 | `	const char *zBuf = (const char *)pBuf;` |
|    - |  182 | `	int nSent;` |
|  308 |  183 | `	while( nLen > 0 ){` |
|  154 |  184 | `		nSent = (int)send(sock, zBuf, nLen, 0);` |
|  154 |  185 | `		if( nSent <= 0 ){` |
|  ! 0 |  186 | `			return PH7_IO_ERR;` |
|    - |  187 | `		}` |
|  154 |  188 | `		zBuf += nSent;` |
|  154 |  189 | `		nLen -= nSent;` |
|  ! 0 |  190 | `	}` |
|  154 |  191 | `	return PH7_OK;` |
|   77 |  192 | `}` |
|    - |  193 | `/*` |
|    - |  194 | ` * Close a socket.` |
|    - |  195 | ` */` |
|   76 |  196 | `PH7_PRIVATE void PH7_NetClose(ph7_socket sock)` |
|  ! 0 |  197 | `{` |
|   76 |  198 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  199 | `		return;` |
|    - |  200 | `	}` |
|    - |  201 | `#ifdef __WINNT__` |
|  ! 0 |  202 | `	closesocket(sock);` |
|    - |  203 | `#else` |
|   76 |  204 | `	close(sock);` |
|    - |  205 | `#endif` |
|   38 |  206 | `}` |
|    - |  207 | `/*` |
|    - |  208 | ` * Set a receive timeout on a socket (in milliseconds).` |
|    - |  209 | ` */` |
|   44 |  210 | `PH7_PRIVATE void PH7_NetSetTimeout(ph7_socket sock, int iMilliseconds)` |
|  ! 0 |  211 | `{` |
|    - |  212 | `#ifdef __WINNT__` |
|  ! 0 |  213 | `	DWORD tv = (DWORD)iMilliseconds;` |
|  ! 0 |  214 | `	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));` |
|    - |  215 | `#else` |
|    - |  216 | `	struct timeval tv;` |
|   44 |  217 | `	tv.tv_sec = iMilliseconds / 1000;` |
|   44 |  218 | `	tv.tv_usec = (iMilliseconds % 1000) * 1000;` |
|   44 |  219 | `	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(tv));` |
|    - |  220 | `#endif` |
|   44 |  221 | `}` |
|    - |  222 | `/*` |
|    - |  223 | ` * Extract a human-readable IP address string from a sockaddr.` |
|    - |  224 | ` * Writes at most nBufLen bytes (including NUL) to zBuf.` |
|    - |  225 | ` */` |
|   38 |  226 | `PH7_PRIVATE void PH7_NetAddrToString(const struct sockaddr *pAddr, char *zBuf, int nBufLen)` |
|  ! 0 |  227 | `{` |
|   38 |  228 | `	const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;` |
|   38 |  229 | `	if( pAddr == 0 \|\| pAddr->sa_family != AF_INET ){` |
|  ! 0 |  230 | `		if( nBufLen > 0 ){` |
|  ! 0 |  231 | `			zBuf[0] = 0;` |
|  ! 0 |  232 | `		}` |
|  ! 0 |  233 | `		return;` |
|    - |  234 | `	}` |
|    - |  235 | `#ifdef __WINNT__` |
|    - |  236 | `	{` |
|  ! 0 |  237 | `		char *zAddr = inet_ntoa(pIn->sin_addr);` |
|  ! 0 |  238 | `		if( zAddr ){` |
|  ! 0 |  239 | `			int n = (int)strlen(zAddr);` |
|  ! 0 |  240 | `			if( n >= nBufLen ) n = nBufLen - 1;` |
|  ! 0 |  241 | `			memcpy(zBuf, zAddr, n);` |
|  ! 0 |  242 | `			zBuf[n] = 0;` |
|  ! 0 |  243 | `		}else{` |
|  ! 0 |  244 | `			zBuf[0] = 0;` |
|    - |  245 | `		}` |
|    - |  246 | `	}` |
|    - |  247 | `#else` |
|   38 |  248 | `	inet_ntop(AF_INET, &pIn->sin_addr, zBuf, (ph7_socklen)nBufLen);` |
|    - |  249 | `#endif` |
|   19 |  250 | `}` |
|    - |  251 | `/*` |
|    - |  252 | ` * Extract the port number from a sockaddr (in host byte order).` |
|    - |  253 | ` */` |
|   38 |  254 | `PH7_PRIVATE int PH7_NetAddrPort(const struct sockaddr *pAddr)` |
|  ! 0 |  255 | `{` |
|   38 |  256 | `	const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;` |
|   38 |  257 | `	if( pAddr == 0 \|\| pAddr->sa_family != AF_INET ){` |
|  ! 0 |  258 | `		return 0;` |
|    - |  259 | `	}` |
|   38 |  260 | `	return (int)ntohs(pIn->sin_port);` |
|   19 |  261 | `}` |
|    - |  262 |  |
|    - |  263 | `#endif /* PH7_ENABLE_NET */` |
|    - |  264 |  |
