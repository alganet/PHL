# src/ph7/net.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 53/105 lines (50.48%)

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
|   20 |   36 | `PH7_PRIVATE int PH7_NetInit(void)` |
|  ! 0 |   37 |  |
|    - |   38 | `#ifdef __WINNT__` |
|    - |   39 | `	WSADATA wsaData;` |
|  ! 0 |   40 | `	if( WSAStartup(MAKEWORD(2,2), &wsaData) != 0 ){` |
|  ! 0 |   41 | `		return PH7_IO_ERR;` |
|    - |   42 | `	}` |
|    - |   43 | `#else` |
|   20 |   44 | `	signal(SIGPIPE, SIG_IGN);` |
|    - |   45 | `#endif` |
|   20 |   46 | `	return PH7_OK;` |
|  ! 0 |   47 |  |
|    - |   48 | `/*` |
|    - |   49 | ` * Cleanup the networking subsystem.` |
|    - |   50 | ` */` |
|   20 |   51 | `PH7_PRIVATE void PH7_NetCleanup(void)` |
|  ! 0 |   52 |  |
|    - |   53 | `#ifdef __WINNT__` |
|  ! 0 |   54 | `	WSACleanup();` |
|    - |   55 | `#endif` |
|   20 |   56 |  |
|    - |   57 | `/*` |
|    - |   58 | ` * Create a TCP listening socket bound to the given host and port.` |
|    - |   59 | ` * Returns the socket descriptor, or PH7_NET_INVALID_SOCKET on error.` |
|    - |   60 | ` */` |
|   20 |   61 | `PH7_PRIVATE ph7_socket PH7_NetListen(const char *zHost, int iPort, int iBacklog)` |
|  ! 0 |   62 |  |
|    - |   63 | `	struct sockaddr_in addr;` |
|    - |   64 | `	ph7_socket sock;` |
|   20 |   65 | `	int on = 1;` |
|   20 |   66 | `	memset(&addr, 0, sizeof(addr));` |
|   20 |   67 | `	addr.sin_family = AF_INET;` |
|   20 |   68 | `	addr.sin_port = htons((unsigned short)iPort);` |
|   20 |   69 | `	if( zHost == 0 \|\| zHost[0] == 0 \|\| strcmp(zHost, "0.0.0.0") == 0 ){` |
|  ! 0 |   70 | `		addr.sin_addr.s_addr = htonl(INADDR_ANY);` |
|   20 |   71 | `	}else if( strcmp(zHost, "localhost") == 0 \|\| strcmp(zHost, "127.0.0.1") == 0 ){` |
|   20 |   72 | `		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);` |
|   10 |   73 | `	}else{` |
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
|   20 |   84 | `	sock = socket(AF_INET, SOCK_STREAM, 0);` |
|   20 |   85 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |   86 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |   87 | `	}` |
|   20 |   88 | `	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));` |
|   20 |   89 | `	if( bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0 ){` |
|  ! 0 |   90 | `		PH7_NetClose(sock);` |
|  ! 0 |   91 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |   92 | `	}` |
|   20 |   93 | `	if( listen(sock, iBacklog) != 0 ){` |
|  ! 0 |   94 | `		PH7_NetClose(sock);` |
|  ! 0 |   95 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |   96 | `	}` |
|   20 |   97 | `	return sock;` |
|   10 |   98 |  |
|    - |   99 | `/*` |
|    - |  100 | ` * Accept an incoming connection on a listening socket.` |
|    - |  101 | ` * If pAddr and pAddrLen are non-NULL, the client address is stored there.` |
|    - |  102 | ` * Returns the client socket, or PH7_NET_INVALID_SOCKET on error.` |
|    - |  103 | ` */` |
|   48 |  104 | `PH7_PRIVATE ph7_socket PH7_NetAccept(ph7_socket listenSock, struct sockaddr *pAddr, ph7_socklen *pAddrLen)` |
|  ! 0 |  105 |  |
|   48 |  106 | `	return accept(listenSock, pAddr, pAddrLen);` |
|  ! 0 |  107 |  |
|    - |  108 | `/*` |
|    - |  109 | ` * Receive data from a socket.` |
|    - |  110 | ` * Returns the number of bytes received, or -1 on error.` |
|    - |  111 | ` */` |
|   28 |  112 | `PH7_PRIVATE int PH7_NetRecv(ph7_socket sock, void *pBuf, int nLen, int flags)` |
|  ! 0 |  113 |  |
|   28 |  114 | `	return (int)recv(sock, (char *)pBuf, nLen, flags);` |
|  ! 0 |  115 |  |
|    - |  116 | `/*` |
|    - |  117 | ` * Send data on a socket.` |
|    - |  118 | ` * Returns the number of bytes sent, or -1 on error.` |
|    - |  119 | ` */` |
|  ! 0 |  120 | `PH7_PRIVATE int PH7_NetSend(ph7_socket sock, const void *pBuf, int nLen, int flags)` |
|  ! 0 |  121 |  |
|  ! 0 |  122 | `	return (int)send(sock, (const char *)pBuf, nLen, flags);` |
|  ! 0 |  123 |  |
|    - |  124 | `/*` |
|    - |  125 | ` * Send all data on a socket, retrying on partial writes.` |
|    - |  126 | ` * Returns PH7_OK on success, PH7_IO_ERR on error.` |
|    - |  127 | ` */` |
|  110 |  128 | `PH7_PRIVATE int PH7_NetSendAll(ph7_socket sock, const void *pBuf, int nLen)` |
|  ! 0 |  129 |  |
|  110 |  130 | `	const char *zBuf = (const char *)pBuf;` |
|    - |  131 | `	int nSent;` |
|  220 |  132 | `	while( nLen > 0 ){` |
|  110 |  133 | `		nSent = (int)send(sock, zBuf, nLen, 0);` |
|  110 |  134 | `		if( nSent <= 0 ){` |
|  ! 0 |  135 | `			return PH7_IO_ERR;` |
|    - |  136 | `		}` |
|  110 |  137 | `		zBuf += nSent;` |
|  110 |  138 | `		nLen -= nSent;` |
|  ! 0 |  139 | `	}` |
|  110 |  140 | `	return PH7_OK;` |
|   55 |  141 |  |
|    - |  142 | `/*` |
|    - |  143 | ` * Close a socket.` |
|    - |  144 | ` */` |
|   48 |  145 | `PH7_PRIVATE void PH7_NetClose(ph7_socket sock)` |
|  ! 0 |  146 |  |
|   48 |  147 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  148 | `		return;` |
|    - |  149 | `	}` |
|    - |  150 | `#ifdef __WINNT__` |
|  ! 0 |  151 | `	closesocket(sock);` |
|    - |  152 | `#else` |
|   48 |  153 | `	close(sock);` |
|    - |  154 | `#endif` |
|   24 |  155 |  |
|    - |  156 | `/*` |
|    - |  157 | ` * Set a receive timeout on a socket (in milliseconds).` |
|    - |  158 | ` */` |
|   28 |  159 | `PH7_PRIVATE void PH7_NetSetTimeout(ph7_socket sock, int iMilliseconds)` |
|  ! 0 |  160 |  |
|    - |  161 | `#ifdef __WINNT__` |
|  ! 0 |  162 | `	DWORD tv = (DWORD)iMilliseconds;` |
|  ! 0 |  163 | `	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));` |
|    - |  164 | `#else` |
|    - |  165 | `	struct timeval tv;` |
|   28 |  166 | `	tv.tv_sec = iMilliseconds / 1000;` |
|   28 |  167 | `	tv.tv_usec = (iMilliseconds % 1000) * 1000;` |
|   28 |  168 | `	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(tv));` |
|    - |  169 | `#endif` |
|   28 |  170 |  |
|    - |  171 | `/*` |
|    - |  172 | ` * Extract a human-readable IP address string from a sockaddr.` |
|    - |  173 | ` * Writes at most nBufLen bytes (including NUL) to zBuf.` |
|    - |  174 | ` */` |
|   28 |  175 | `PH7_PRIVATE void PH7_NetAddrToString(const struct sockaddr *pAddr, char *zBuf, int nBufLen)` |
|  ! 0 |  176 |  |
|   28 |  177 | `	const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;` |
|   28 |  178 | `	if( pAddr == 0 \|\| pAddr->sa_family != AF_INET ){` |
|  ! 0 |  179 | `		if( nBufLen > 0 ){` |
|  ! 0 |  180 | `			zBuf[0] = 0;` |
|  ! 0 |  181 | `		}` |
|  ! 0 |  182 | `		return;` |
|    - |  183 | `	}` |
|    - |  184 | `#ifdef __WINNT__` |
|    - |  185 | `	{` |
|  ! 0 |  186 | `		char *zAddr = inet_ntoa(pIn->sin_addr);` |
|  ! 0 |  187 | `		if( zAddr ){` |
|  ! 0 |  188 | `			int n = (int)strlen(zAddr);` |
|  ! 0 |  189 | `			if( n >= nBufLen ) n = nBufLen - 1;` |
|  ! 0 |  190 | `			memcpy(zBuf, zAddr, n);` |
|  ! 0 |  191 | `			zBuf[n] = 0;` |
|  ! 0 |  192 | `		}else{` |
|  ! 0 |  193 | `			zBuf[0] = 0;` |
|    - |  194 | `		}` |
|    - |  195 | `	}` |
|    - |  196 | `#else` |
|   28 |  197 | `	inet_ntop(AF_INET, &pIn->sin_addr, zBuf, (ph7_socklen)nBufLen);` |
|    - |  198 | `#endif` |
|   14 |  199 |  |
|    - |  200 | `/*` |
|    - |  201 | ` * Extract the port number from a sockaddr (in host byte order).` |
|    - |  202 | ` */` |
|   28 |  203 | `PH7_PRIVATE int PH7_NetAddrPort(const struct sockaddr *pAddr)` |
|  ! 0 |  204 |  |
|   28 |  205 | `	const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;` |
|   28 |  206 | `	if( pAddr == 0 \|\| pAddr->sa_family != AF_INET ){` |
|  ! 0 |  207 | `		return 0;` |
|    - |  208 | `	}` |
|   28 |  209 | `	return (int)ntohs(pIn->sin_port);` |
|   14 |  210 |  |
|    - |  211 |  |
|    - |  212 | `#endif /* PH7_ENABLE_NET */` |
|    - |  213 |  |
