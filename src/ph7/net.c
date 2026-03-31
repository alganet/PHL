/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifdef PH7_ENABLE_NET
/*
 * Cross-platform socket abstraction layer.
 * Provides a thin wrapper over POSIX sockets (Unix) and Winsock2 (Windows).
 * Guarded by PH7_ENABLE_NET so it compiles to nothing in tiny builds.
 */
#include <string.h>
#include <stdio.h>

#ifdef __WINNT__
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#endif

/*
 * Initialize the networking subsystem.
 * On Windows, calls WSAStartup(). On Unix, ignores SIGPIPE.
 * Returns PH7_OK on success.
 */
PH7_PRIVATE int PH7_NetInit(void)
{
#ifdef __WINNT__
	WSADATA wsaData;
	if( WSAStartup(MAKEWORD(2,2), &wsaData) != 0 ){
		return PH7_IO_ERR;
	}
#else
	signal(SIGPIPE, SIG_IGN);
#endif
	return PH7_OK;
}
/*
 * Cleanup the networking subsystem.
 */
PH7_PRIVATE void PH7_NetCleanup(void)
{
#ifdef __WINNT__
	WSACleanup();
#endif
}
/*
 * Create a TCP listening socket bound to the given host and port.
 * Returns the socket descriptor, or PH7_NET_INVALID_SOCKET on error.
 */
PH7_PRIVATE ph7_socket PH7_NetListen(const char *zHost, int iPort, int iBacklog)
{
	struct sockaddr_in addr;
	ph7_socket sock;
	int on = 1;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)iPort);
	if( zHost == 0 || zHost[0] == 0 || strcmp(zHost, "0.0.0.0") == 0 ){
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}else if( strcmp(zHost, "localhost") == 0 || strcmp(zHost, "127.0.0.1") == 0 ){
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	}else{
		struct addrinfo hints, *res;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		if( getaddrinfo(zHost, 0, &hints, &res) != 0 || res == 0 ){
			return PH7_NET_INVALID_SOCKET;
		}
		addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
		freeaddrinfo(res);
	}
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if( sock == PH7_NET_INVALID_SOCKET ){
		return PH7_NET_INVALID_SOCKET;
	}
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
	if( bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0 ){
		PH7_NetClose(sock);
		return PH7_NET_INVALID_SOCKET;
	}
	if( listen(sock, iBacklog) != 0 ){
		PH7_NetClose(sock);
		return PH7_NET_INVALID_SOCKET;
	}
	return sock;
}
/*
 * Accept an incoming connection on a listening socket.
 * If pAddr and pAddrLen are non-NULL, the client address is stored there.
 * Returns the client socket, or PH7_NET_INVALID_SOCKET on error.
 */
PH7_PRIVATE ph7_socket PH7_NetAccept(ph7_socket listenSock, struct sockaddr *pAddr, ph7_socklen *pAddrLen)
{
	return accept(listenSock, pAddr, pAddrLen);
}
/*
 * Receive data from a socket.
 * Returns the number of bytes received, or -1 on error.
 */
PH7_PRIVATE int PH7_NetRecv(ph7_socket sock, void *pBuf, int nLen, int flags)
{
	return (int)recv(sock, (char *)pBuf, nLen, flags);
}
/*
 * Send data on a socket.
 * Returns the number of bytes sent, or -1 on error.
 */
PH7_PRIVATE int PH7_NetSend(ph7_socket sock, const void *pBuf, int nLen, int flags)
{
	return (int)send(sock, (const char *)pBuf, nLen, flags);
}
/*
 * Send all data on a socket, retrying on partial writes.
 * Returns PH7_OK on success, PH7_IO_ERR on error.
 */
PH7_PRIVATE int PH7_NetSendAll(ph7_socket sock, const void *pBuf, int nLen)
{
	const char *zBuf = (const char *)pBuf;
	int nSent;
	while( nLen > 0 ){
		nSent = (int)send(sock, zBuf, nLen, 0);
		if( nSent <= 0 ){
			return PH7_IO_ERR;
		}
		zBuf += nSent;
		nLen -= nSent;
	}
	return PH7_OK;
}
/*
 * Close a socket.
 */
PH7_PRIVATE void PH7_NetClose(ph7_socket sock)
{
	if( sock == PH7_NET_INVALID_SOCKET ){
		return;
	}
#ifdef __WINNT__
	closesocket(sock);
#else
	close(sock);
#endif
}
/*
 * Set a receive timeout on a socket (in milliseconds).
 */
PH7_PRIVATE void PH7_NetSetTimeout(ph7_socket sock, int iMilliseconds)
{
#ifdef __WINNT__
	DWORD tv = (DWORD)iMilliseconds;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#else
	struct timeval tv;
	tv.tv_sec = iMilliseconds / 1000;
	tv.tv_usec = (iMilliseconds % 1000) * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(tv));
#endif
}
/*
 * Extract a human-readable IP address string from a sockaddr.
 * Writes at most nBufLen bytes (including NUL) to zBuf.
 */
PH7_PRIVATE void PH7_NetAddrToString(const struct sockaddr *pAddr, char *zBuf, int nBufLen)
{
	const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;
	if( pAddr == 0 || pAddr->sa_family != AF_INET ){
		if( nBufLen > 0 ){
			zBuf[0] = 0;
		}
		return;
	}
#ifdef __WINNT__
	{
		char *zAddr = inet_ntoa(pIn->sin_addr);
		if( zAddr ){
			int n = (int)strlen(zAddr);
			if( n >= nBufLen ) n = nBufLen - 1;
			memcpy(zBuf, zAddr, n);
			zBuf[n] = 0;
		}else{
			zBuf[0] = 0;
		}
	}
#else
	inet_ntop(AF_INET, &pIn->sin_addr, zBuf, (ph7_socklen)nBufLen);
#endif
}
/*
 * Extract the port number from a sockaddr (in host byte order).
 */
PH7_PRIVATE int PH7_NetAddrPort(const struct sockaddr *pAddr)
{
	const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;
	if( pAddr == 0 || pAddr->sa_family != AF_INET ){
		return 0;
	}
	return (int)ntohs(pIn->sin_port);
}

#endif /* PH7_ENABLE_NET */
