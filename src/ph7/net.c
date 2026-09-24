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
#include <poll.h>
#endif

/*
 * The OS error the last socket call reported. A Windows socket does not touch
 * errno at all, so a caller reading errno there reads whatever the last
 * unrelated CRT call left behind.
 */
PH7_PRIVATE int PH7_NetLastError(void)
{
#ifdef __WINNT__
	return WSAGetLastError();
#else
	return errno;
#endif
}
/*
 * The message php words a socket failure with. php's own POSIX build answers
 * strerror(), which is what a script comparing an $errstr against a documented
 * text expects; a Winsock code is not an errno at all, so the codes a stream
 * builtin can actually surface are worded here rather than handed to
 * strerror() (which would answer for a completely different errno) — the
 * FormatMessage() prose php's Windows build answers is its own (§7.4).
 */
PH7_PRIVATE const char * PH7_NetStrError(int iErr)
{
#ifdef __WINNT__
	static const struct { int iCode; const char *zMsg; } aWsa[] = {
		{ WSAEACCES,          "Permission denied" },
		{ WSAEADDRINUSE,      "Address already in use" },
		{ WSAEADDRNOTAVAIL,   "Cannot assign requested address" },
		{ WSAEAFNOSUPPORT,    "Address family not supported by protocol" },
		{ WSAECONNABORTED,    "Software caused connection abort" },
		{ WSAECONNREFUSED,    "Connection refused" },
		{ WSAECONNRESET,      "Connection reset by peer" },
		{ WSAEHOSTUNREACH,    "No route to host" },
		{ WSAEINVAL,          "Invalid argument" },
		{ WSAEMFILE,          "Too many open files" },
		{ WSAENETDOWN,        "Network is down" },
		{ WSAENETUNREACH,     "Network is unreachable" },
		{ WSAENOTCONN,        "Transport endpoint is not connected" },
		{ WSAENOTSOCK,        "Socket operation on non-socket" },
		{ WSAEOPNOTSUPP,      "Operation not supported" },
		{ WSAEPROTONOSUPPORT, "Protocol not supported" },
		{ WSAETIMEDOUT,       "Connection timed out" },
		{ WSAEWOULDBLOCK,     "Resource temporarily unavailable" },
		{ WSAEDESTADDRREQ,    "Destination address required" },
		{ WSAEISCONN,         "Transport endpoint is already connected" },
		{ WSAEMSGSIZE,        "Message too long" },
		{ WSAENOBUFS,         "No buffer space available" },
		{ WSAENOPROTOOPT,     "Protocol not available" },
		/* A send on a socket whose write side is shut is POSIX's EPIPE, and it
		 * is the same condition — the wording follows the errno rather than the
		 * Winsock name so a script reading it reads one answer. */
		{ WSAESHUTDOWN,       "Broken pipe" }
	};
	int i;
	for( i = 0 ; i < (int)(sizeof(aWsa)/sizeof(aWsa[0])) ; i++ ){
		if( aWsa[i].iCode == iErr ){
			return aWsa[i].zMsg;
		}
	}
	return "Unknown error";
#else
	return strerror(iErr);
#endif
}
/*
 * Did the last socket call fail only because it would have WAITED? That is the
 * one failure php does not report as an error: a read answers "" or false by
 * the handle's own rules, and a write answers what it managed to send.
 */
PH7_PRIVATE int PH7_NetWouldBlock(void)
{
#ifdef __WINNT__
	int iErr = WSAGetLastError();
	return iErr == WSAEWOULDBLOCK || iErr == WSAETIMEDOUT;
#else
	return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}
/*
 * Wait until a socket is readable (bWrite == 0) or writable, for at most
 * iTimeoutMs milliseconds (a negative timeout blocks). Answers 1 (ready),
 * 0 (the wait expired) or -1 (the wait itself failed).
 *
 * POSIX takes poll() rather than select(): a descriptor at or above
 * FD_SETSIZE cannot be put in an fd_set at all, and a long-running server is
 * exactly the program that reaches those numbers.
 */
PH7_PRIVATE int PH7_NetWait(ph7_socket sock,int bWrite,int iTimeoutMs)
{
#ifdef __WINNT__
	fd_set sSet;
	struct timeval tv,*pTv = 0;
	int rc;
	if( sock == PH7_NET_INVALID_SOCKET ){
		/* Nothing to wait on: php's own select() over a stream whose socket was
		 * never created reports it not-ready, which is an expiry and not an
		 * error. poll() would answer POLLNVAL and look like readiness. */
		return 0;
	}
	FD_ZERO(&sSet);
	FD_SET(sock,&sSet);
	if( iTimeoutMs >= 0 ){
		tv.tv_sec = iTimeoutMs / 1000;
		tv.tv_usec = (iTimeoutMs % 1000) * 1000;
		pTv = &tv;
	}
	rc = select(0,bWrite ? 0 : &sSet,bWrite ? &sSet : 0,0,pTv);
	return rc < 0 ? -1 : (rc > 0 ? 1 : 0);
#else
	struct pollfd sPoll;
	int rc;
	if( sock == PH7_NET_INVALID_SOCKET ){
		/* See above: nothing to wait on is an expiry, and poll() would answer
		 * POLLNVAL for it, which looks like readiness. */
		return 0;
	}
	sPoll.fd = sock;
	sPoll.events = (short)(bWrite ? POLLOUT : POLLIN);
	sPoll.revents = 0;
	for(;;){
		rc = poll(&sPoll,1,iTimeoutMs);
		if( rc < 0 && errno == EINTR ){
			/* A signal is not an answer to the question that was asked. */
			continue;
		}
		break;
	}
	return rc < 0 ? -1 : (rc > 0 ? 1 : 0);
#endif
}

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
 * Start the networking subsystem the first time a socket is opened.
 *
 * PH7_NetInit() used to be called by the `-S` server and by nothing else, so
 * every socket a SCRIPT opened ran with the subsystem unstarted: on Windows
 * that is `WSANOTINITIALISED` from the very first call — fsockopen() could
 * never connect at all — and on POSIX it left SIGPIPE at its default, so
 * writing to a socket whose peer had closed KILLED the process (exit 141)
 * where php answers false. Both are invisible from a POSIX-only reading of a
 * program that never loses a peer.
 */
PH7_PRIVATE int PH7_NetEnsureInit(void)
{
	static int bReady = 0;
	if( bReady ){
		return PH7_OK;
	}
	if( PH7_NetInit() != PH7_OK ){
		return PH7_IO_ERR;
	}
	bReady = 1;
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
 * Bind a socket to the given host and port, and — for a stream socket that is
 * going to serve — listen on it. `stream_socket_server()` is the caller that
 * needs the two apart: php's STREAM_SERVER_LISTEN is a separate flag, and a
 * datagram socket is bound WITHOUT ever listening. (A server asked for neither
 * BIND nor LISTEN never reaches here at all — php creates no socket for it.)
 *
 * *pErrno receives the OS error and *pzErr its message, which is what the
 * builtin reports through its by-ref out-params and its warning.
 * Returns the socket, or PH7_NET_INVALID_SOCKET.
 */
PH7_PRIVATE ph7_socket PH7_NetBind(const char *zHost, int iPort, int bDgram, int bListen,
	int iBacklog, int *pErrno, const char **pzErr)
{
	struct sockaddr_in addr;
	ph7_socket sock;
	int on = 1;
	int iType = bDgram ? SOCK_DGRAM : SOCK_STREAM;
	if( pErrno ){ *pErrno = 0; }
	if( pzErr ){ *pzErr = ""; }
	if( PH7_NetEnsureInit() != PH7_OK ){
		if( pzErr ){ *pzErr = "Unable to start the networking subsystem"; }
		return PH7_NET_INVALID_SOCKET;
	}
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
		hints.ai_socktype = iType;
		if( getaddrinfo(zHost, 0, &hints, &res) != 0 || res == 0 ){
			/* Nothing is open yet: the socket is created below. */
			if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }
			if( pzErr ){ *pzErr = 0; }
			return PH7_NET_INVALID_SOCKET;
		}
		addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
		freeaddrinfo(res);
	}
	sock = socket(AF_INET, iType, 0);
	if( sock == PH7_NET_INVALID_SOCKET ){
		if( pErrno ){ *pErrno = PH7_NetLastError(); }
		if( pzErr ){ *pzErr = PH7_NetStrError(PH7_NetLastError()); }
		return PH7_NET_INVALID_SOCKET;
	}
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
	if( bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0
	 || (bListen && !bDgram && listen(sock, iBacklog) != 0) ){
		/* Read the error BEFORE closing the socket: close() is a call of its
		 * own and overwrites what the failure left behind. */
		int iErr = PH7_NetLastError();
		if( pErrno ){ *pErrno = iErr; }
		if( pzErr ){ *pzErr = PH7_NetStrError(iErr); }
		PH7_NetClose(sock);
		return PH7_NET_INVALID_SOCKET;
	}
	return sock;
}
/*
 * Create a TCP listening socket bound to the given host and port.
 * Returns the socket descriptor, or PH7_NET_INVALID_SOCKET on error.
 */
PH7_PRIVATE ph7_socket PH7_NetListen(const char *zHost, int iPort, int iBacklog)
{
	return PH7_NetBind(zHost, iPort, 0, 1, iBacklog, 0, 0);
}
/*
 * The local (bPeer == 0) or the peer address of a socket, formatted the way
 * php's stream_socket_get_name() answers it: "ip:port". Returns PH7_OK, or -1
 * for a socket that has no such name (an unbound one, or the peer of a socket
 * that is not connected) — which is php's `false`.
 */
PH7_PRIVATE int PH7_NetSockName(ph7_socket sock, int bPeer, char *zBuf, int nBuf)
{
	struct sockaddr_in addr;
	ph7_socklen nLen = (ph7_socklen)sizeof(addr);
	char zAddr[64];
	int n;
	if( zBuf == 0 || nBuf < 2 ){
		return -1;
	}
	zBuf[0] = 0;
	memset(&addr, 0, sizeof(addr));
	if( (bPeer ? getpeername(sock, (struct sockaddr *)&addr, &nLen)
	           : getsockname(sock, (struct sockaddr *)&addr, &nLen)) != 0 ){
		return -1;
	}
	if( addr.sin_family != AF_INET ){
		/* A socketpair has no address of any kind, and php answers false for
		 * one rather than inventing a name for it. */
		return -1;
	}
	PH7_NetAddrToString((struct sockaddr *)&addr, zAddr, (int)sizeof(zAddr));
	if( zAddr[0] == 0 ){
		return -1;
	}
	n = snprintf(zBuf, (size_t)nBuf, "%s:%d", zAddr, PH7_NetAddrPort((struct sockaddr *)&addr));
	return (n > 0 && n < nBuf) ? PH7_OK : -1;
}
/*
 * accept() bounded by a timeout in milliseconds (a negative one blocks). The
 * peer's address is written to zPeer when it fits, which is php's by-ref
 * $peer_name out-param. *pbTimedOut tells a wait that EXPIRED — php's own
 * "Accept failed: Connection timed out" — from a call that failed.
 */
PH7_PRIVATE ph7_socket PH7_NetAcceptTimed(ph7_socket listenSock, int iTimeoutMs, int *pbTimedOut,
	char *zPeer, int nPeer)
{
	struct sockaddr_in addr;
	ph7_socklen nLen = (ph7_socklen)sizeof(addr);
	ph7_socket sock;
	if( pbTimedOut ){ *pbTimedOut = 0; }
	if( zPeer && nPeer > 0 ){ zPeer[0] = 0; }
	if( iTimeoutMs >= 0 ){
		int rc = PH7_NetWait(listenSock, 0, iTimeoutMs);
		if( rc == 0 ){
			if( pbTimedOut ){ *pbTimedOut = 1; }
			return PH7_NET_INVALID_SOCKET;
		}
		if( rc < 0 ){
			return PH7_NET_INVALID_SOCKET;
		}
	}
	memset(&addr, 0, sizeof(addr));
	sock = accept(listenSock, (struct sockaddr *)&addr, &nLen);
	if( sock == PH7_NET_INVALID_SOCKET ){
		return PH7_NET_INVALID_SOCKET;
	}
	if( zPeer && nPeer > 0 && addr.sin_family == AF_INET ){
		char zAddr[64];
		PH7_NetAddrToString((struct sockaddr *)&addr, zAddr, (int)sizeof(zAddr));
		if( zAddr[0] ){
			snprintf(zPeer, (size_t)nPeer, "%s:%d", zAddr, PH7_NetAddrPort((struct sockaddr *)&addr));
		}
	}
	return sock;
}
/*
 * Connect a TCP socket to the given host and port (blocking; the caller sets
 * a timeout with PH7_NetSetTimeout afterwards). *pErrno receives the OS error
 * code and *pzErr a static description on failure, matching what fsockopen()
 * reports through its by-ref out-params.
 * Returns the connected socket, or PH7_NET_INVALID_SOCKET on error.
 */
PH7_PRIVATE ph7_socket PH7_NetConnect(const char *zHost, int iPort, int iTimeoutMs,
	int *pErrno, const char **pzErr)
{
	struct addrinfo hints, *res = 0, *rp;
	char zPort[16];
	ph7_socket sock = PH7_NET_INVALID_SOCKET;
	if( pErrno ){ *pErrno = 0; }
	if( pzErr ){ *pzErr = ""; }
	if( PH7_NetEnsureInit() != PH7_OK ){
		if( pErrno ){ *pErrno = -1; }
		if( pzErr ){ *pzErr = "Unable to start the networking subsystem"; }
		return PH7_NET_INVALID_SOCKET;
	}
	if( zHost == 0 || zHost[0] == 0 ){
		if( pErrno ){ *pErrno = -1; }
		if( pzErr ){ *pzErr = "Empty host"; }
		return PH7_NET_INVALID_SOCKET;
	}
	snprintf(zPort, sizeof(zPort), "%d", iPort);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if( getaddrinfo(zHost, zPort, &hints, &res) != 0 || res == 0 ){
		/* php words the HOST into this one and reports no OS code for it; the
		 * caller composes it, since only it has the name to interpolate. */
		if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }
		if( pzErr ){ *pzErr = 0; }
		return PH7_NET_INVALID_SOCKET;
	}
	for( rp = res ; rp != 0 ; rp = rp->ai_next ){
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if( sock == PH7_NET_INVALID_SOCKET ){
			continue;
		}
		if( iTimeoutMs > 0 ){
			/* the connect() itself stays blocking; the timeout bounds the
			 * subsequent recv/send (php applies it to both — recorded) */
			PH7_NetSetTimeout(sock, iTimeoutMs);
		}
		if( connect(sock, rp->ai_addr, (ph7_socklen)rp->ai_addrlen) == 0 ){
			freeaddrinfo(res);
			return sock;
		}
		PH7_NetClose(sock);
		sock = PH7_NET_INVALID_SOCKET;
	}
	freeaddrinfo(res);
	if( pErrno ){ *pErrno = 111; }
	if( pzErr ){ *pzErr = "Connection refused"; }
	return PH7_NET_INVALID_SOCKET;
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
 * Set BOTH the receive and the send timeout, which is what php's
 * stream_set_timeout() means by "the timeout of this stream". A zero pair
 * means "no timeout" to the OS, and that is php's answer for it too.
 */
PH7_PRIVATE void PH7_NetSetRwTimeout(ph7_socket sock, ph7_int64 iSeconds, ph7_int64 iMicroseconds)
{
	/* Carry the microseconds over: the OS rejects a tv_usec of 1000000 or more
	 * outright (EINVAL), so `stream_set_timeout($s, 0, 1500000)` used to set
	 * NOTHING and leave the read unbounded while answering true. */
	if( iMicroseconds >= 1000000 ){
		iSeconds += iMicroseconds / 1000000;
		iMicroseconds %= 1000000;
	}
	if( iSeconds == 0 && iMicroseconds == 0 ){
		/* php's zero timeout means "do not wait", where a zero timeval means
		 * "wait forever" to the OS: the smallest one it can express is what
		 * carries that intent. */
		iMicroseconds = 1;
	}
	if( iSeconds > 4000000 ){
		/* Beyond any real deadline, and past what a millisecond DWORD holds. */
		iSeconds = 4000000;
	}
#ifdef __WINNT__
	{
		DWORD tv = (DWORD)(iSeconds * 1000 + iMicroseconds / 1000);
		if( tv == 0 ){
			tv = 1; /* a Windows zero is "block forever" too */
		}
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
	}
#else
	{
		struct timeval tv;
		tv.tv_sec = (time_t)iSeconds;
		tv.tv_usec = (suseconds_t)iMicroseconds;
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(tv));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const void *)&tv, sizeof(tv));
	}
#endif
}
/*
 * Turn a socket's blocking mode on or off. On Windows a socket is not an fd,
 * so the fcntl() route the rest of the stream family takes cannot reach it.
 */
PH7_PRIVATE void PH7_NetSetBlocking(ph7_socket sock, int bBlocking)
{
#ifdef __WINNT__
	u_long iMode = bBlocking ? 0 : 1;
	ioctlsocket(sock, FIONBIO, &iMode);
#else
	int iFlags = fcntl(sock, F_GETFL, 0);
	if( iFlags < 0 ){
		return;
	}
	if( bBlocking ){
		iFlags &= ~O_NONBLOCK;
	}else{
		iFlags |= O_NONBLOCK;
	}
	fcntl(sock, F_SETFL, iFlags);
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

/*
 * The PLATFORM numbers php's socket constants carry, which is why they cannot
 * be written down in a header: `STREAM_PF_INET6` is 10 on Linux, 23 on Windows
 * and 30 on the BSDs, and a program that hands one of them to
 * stream_socket_pair() is handing the OS its own value.
 */
PH7_PRIVATE ph7_int64 PH7_NetSocketConst(int iWhich)
{
	switch( iWhich ){
	case PH7_NETC_PF_INET:      return AF_INET;
	case PH7_NETC_PF_INET6:     return AF_INET6;
	case PH7_NETC_PF_UNIX:      return AF_UNIX;
	case PH7_NETC_SOCK_STREAM:  return SOCK_STREAM;
	case PH7_NETC_SOCK_DGRAM:   return SOCK_DGRAM;
	case PH7_NETC_SOCK_RAW:     return SOCK_RAW;
	case PH7_NETC_SOCK_SEQPACKET: return SOCK_SEQPACKET;
	case PH7_NETC_SOCK_RDM:     return SOCK_RDM;
	case PH7_NETC_IPPROTO_IP:   return IPPROTO_IP;
	case PH7_NETC_IPPROTO_TCP:  return IPPROTO_TCP;
	case PH7_NETC_IPPROTO_UDP:  return IPPROTO_UDP;
	case PH7_NETC_IPPROTO_ICMP: return IPPROTO_ICMP;
	case PH7_NETC_IPPROTO_RAW:  return IPPROTO_RAW;
	default: break;
	}
	return 0;
}
/*
 * shutdown(), which is how a program says "I am done SENDING" without closing
 * the handle it still wants to read — the half-close every line protocol ends
 * with. php's three modes are 0/1/2 in its own numbering.
 */
PH7_PRIVATE int PH7_NetShutdown(ph7_socket sock,int iHow)
{
	int iSys;
	/* Winsock spells the three SD_RECEIVE/SD_SEND/SD_BOTH, with the same
	 * numbers; POSIX spells them SHUT_*. */
#ifdef __WINNT__
	switch( iHow ){
	case 0:  iSys = SD_RECEIVE; break;
	case 1:  iSys = SD_SEND; break;
	default: iSys = SD_BOTH; break;
	}
#else
	switch( iHow ){
	case 0:  iSys = SHUT_RD; break;
	case 1:  iSys = SHUT_WR; break;
	default: iSys = SHUT_RDWR; break;
	}
#endif
	return shutdown(sock,iSys) == 0 ? PH7_OK : -1;
}
/*
 * Is there anything left on this socket to hand over? Asked of a socket whose
 * READ side has just been shut down, where a recv() cannot block: php answers
 * feof() for a socket by probing it, so a half-close with bytes still queued is
 * NOT an end of file and one with nothing queued is.
 */
PH7_PRIVATE int PH7_NetAtEnd(ph7_socket sock)
{
	char c;
	return recv(sock,&c,1,MSG_PEEK) <= 0 ? 1 : 0;
}
/* php's STREAM_OOB/STREAM_PEEK are php's own bits, not the OS's. */
static int NetMsgFlags(int iFlags)
{
	int iOut = 0;
	if( iFlags & PH7_STREAM_OOB ){
		iOut |= MSG_OOB;
	}
	if( iFlags & PH7_STREAM_PEEK ){
		iOut |= MSG_PEEK;
	}
	return iOut;
}
/*
 * Receive straight from the socket, with the sender's address when the datagram
 * carries one. This deliberately does NOT go through the handle's read buffer:
 * php's own recvfrom() asks the SOCKET, which is why it blocks on a handle whose
 * buffer still holds bytes.
 */
PH7_PRIVATE int PH7_NetRecvFrom(ph7_socket sock,void *pBuf,int nLen,int iFlags,char *zAddr,int nAddr)
{
	struct sockaddr_in sFrom;
	ph7_socklen nFrom = (ph7_socklen)sizeof(sFrom);
	int n;
	if( zAddr && nAddr > 0 ){
		zAddr[0] = 0;
	}
	memset(&sFrom,0,sizeof(sFrom));
	n = (int)recvfrom(sock,(char *)pBuf,nLen,NetMsgFlags(iFlags),(struct sockaddr *)&sFrom,&nFrom);
	if( n >= 0 && zAddr && nAddr > 0 && sFrom.sin_family == AF_INET ){
		char zIp[64];
		PH7_NetAddrToString((struct sockaddr *)&sFrom,zIp,(int)sizeof(zIp));
		if( zIp[0] ){
			snprintf(zAddr,(size_t)nAddr,"%s:%d",zIp,PH7_NetAddrPort((struct sockaddr *)&sFrom));
		}
	}
	return n;
}
/*
 * Send straight to the socket, to a named address when one is given. A
 * connected socket takes the address too — php passes it to sendto() and lets
 * the OS decide, which for a connected TCP socket means it is simply sent.
 */
PH7_PRIVATE int PH7_NetSendTo(ph7_socket sock,const void *pBuf,int nLen,int iFlags,
	const char *zHost,int iPort,int *pErrno)
{
	struct sockaddr_in sTo;
	if( pErrno ){ *pErrno = 0; }
	if( zHost == 0 || zHost[0] == 0 ){
		return (int)send(sock,(const char *)pBuf,nLen,NetMsgFlags(iFlags));
	}
	memset(&sTo,0,sizeof(sTo));
	sTo.sin_family = AF_INET;
	sTo.sin_port = htons((unsigned short)iPort);
	if( strcmp(zHost,"localhost") == 0 || strcmp(zHost,"127.0.0.1") == 0 ){
		sTo.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	}else{
		struct addrinfo hints,*res;
		memset(&hints,0,sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		if( getaddrinfo(zHost,0,&hints,&res) != 0 || res == 0 ){
			/* Nothing was sent, and the caller says so in php's own three
			 * voices — which is a different answer from a send that failed. */
			if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }
			return -1;
		}
		sTo.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
		freeaddrinfo(res);
	}
	return (int)sendto(sock,(const char *)pBuf,nLen,NetMsgFlags(iFlags),
		(struct sockaddr *)&sTo,(ph7_socklen)sizeof(sTo));
}
/*
 * A connected PAIR of sockets, which is what a program hands a child process (or
 * a test double) as a two-way pipe. POSIX has the call; Windows does not, and
 * php builds the pair over the loopback there — a listener nobody else can
 * reach, one connect, one accept, and a check that the socket that arrived is
 * the one that was dialled.
 */
PH7_PRIVATE int PH7_NetSocketPair(int iDomain,int iType,int iProtocol,ph7_socket *aOut,int *pErrno)
{
	if( pErrno ){ *pErrno = 0; }
	aOut[0] = aOut[1] = PH7_NET_INVALID_SOCKET;
	if( PH7_NetEnsureInit() != PH7_OK ){
		return -1;
	}
#ifndef __WINNT__
	{
		int aFd[2];
		if( socketpair(iDomain,iType,iProtocol,aFd) != 0 ){
			if( pErrno ){ *pErrno = PH7_NetLastError(); }
			return -1;
		}
		aOut[0] = aFd[0];
		aOut[1] = aFd[1];
		return PH7_OK;
	}
#else
	{
		ph7_socket listener,client,server;
		struct sockaddr_in addr,peer,self;
		ph7_socklen nAddr;
		SXUNUSED(iProtocol);
		if( iDomain != AF_INET || iType != SOCK_STREAM ){
			/* php's own Windows emulation covers exactly this pair, and answers
			 * "protocol not available" for anything else — the mirror image of
			 * POSIX, where AF_UNIX is the one that works. */
			if( pErrno ){ *pErrno = WSAENOPROTOOPT; }
			return -1;
		}
		listener = PH7_NetBind("127.0.0.1",0,0,1,1,pErrno,0);
		if( listener == PH7_NET_INVALID_SOCKET ){
			return -1;
		}
		nAddr = (ph7_socklen)sizeof(addr);
		memset(&addr,0,sizeof(addr));
		if( getsockname(listener,(struct sockaddr *)&addr,&nAddr) != 0 ){
			if( pErrno ){ *pErrno = PH7_NetLastError(); }
			PH7_NetClose(listener);
			return -1;
		}
		client = socket(AF_INET,SOCK_STREAM,0);
		if( client == PH7_NET_INVALID_SOCKET
		 || connect(client,(struct sockaddr *)&addr,(ph7_socklen)sizeof(addr)) != 0 ){
			if( pErrno ){ *pErrno = PH7_NetLastError(); }
			PH7_NetClose(client);
			PH7_NetClose(listener);
			return -1;
		}
		nAddr = (ph7_socklen)sizeof(peer);
		memset(&peer,0,sizeof(peer));
		server = accept(listener,(struct sockaddr *)&peer,&nAddr);
		PH7_NetClose(listener);
		if( server == PH7_NET_INVALID_SOCKET ){
			if( pErrno ){ *pErrno = PH7_NetLastError(); }
			PH7_NetClose(client);
			return -1;
		}
		/* The connection that arrived must be the one that was made: another
		 * process could have reached the same listener first. */
		nAddr = (ph7_socklen)sizeof(self);
		memset(&self,0,sizeof(self));
		if( getsockname(client,(struct sockaddr *)&self,&nAddr) != 0
		 || self.sin_port != peer.sin_port
		 || self.sin_addr.s_addr != peer.sin_addr.s_addr ){
			if( pErrno ){ *pErrno = WSAECONNABORTED; }
			PH7_NetClose(client);
			PH7_NetClose(server);
			return -1;
		}
		aOut[0] = client;
		aOut[1] = server;
		return PH7_OK;
	}
#endif
}

#endif /* PH7_ENABLE_NET */
