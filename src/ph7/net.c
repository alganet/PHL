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
#include <netinet/tcp.h>
#endif

/*
 * The `socket` context options that are a setsockopt() on a fresh socket. php
 * applies them to both halves — the one it binds and the one it connects — and
 * ignores what the platform has no name for (SO_REUSEPORT is absent on Windows,
 * where php's own code is #ifdef'd out the same way).
 */
/*
 * Stamp a port into a resolved address. php resolves the HOST and then writes
 * the port into each candidate itself, which is why the port is not a service
 * NAME here: a negative or out-of-range one simply wraps in the 16 bits the
 * wire has (php's own truncation, `-1` becoming 65535), where handing
 * getaddrinfo() the digits would make it an unresolvable address instead.
 */
static void NetStampPort(struct sockaddr *pAddr,int iPort)
{
	if( pAddr == 0 ){
		return;
	}
	if( pAddr->sa_family == AF_INET ){
		((struct sockaddr_in *)pAddr)->sin_port = htons((unsigned short)iPort);
	}else if( pAddr->sa_family == AF_INET6 ){
		((struct sockaddr_in6 *)pAddr)->sin6_port = htons((unsigned short)iPort);
	}
}
/* Defined with the address helpers further down; used by the three callers that
 * report an address to a script. */
static int NetFormatAddr(const struct sockaddr *pAddr,char *zBuf,int nBuf);
/*
 * inet_ntop() with a buffer size the platform's own prototype accepts: Winsock
 * declares the last parameter `size_t` and POSIX `socklen_t`, and /W4 /WX will
 * not take a plain int for either.
 */
static void NetInetNtop(int iFamily,const void *pSrc,char *zBuf,int nBuf)
{
#ifdef __WINNT__
	inet_ntop(iFamily,pSrc,zBuf,(size_t)nBuf);
#else
	inet_ntop(iFamily,pSrc,zBuf,(socklen_t)nBuf);
#endif
}
static void NetApplySockOpts(ph7_socket sock,const ph7_sockopts *pOpt)
{
	int on = 1;
	if( pOpt == 0 ){
		return;
	}
	if( pOpt->bReusePort ){
#ifdef SO_REUSEPORT
		setsockopt(sock,SOL_SOCKET,SO_REUSEPORT,(const char *)&on,sizeof(on));
#endif
	}
	if( pOpt->bNoDelay ){
#ifdef TCP_NODELAY
		setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(const char *)&on,sizeof(on));
#endif
	}
	if( pOpt->bBroadcast ){
		/* The one `socket` option that is a PERMISSION rather than a tuning
		 * knob: without it the OS refuses a datagram addressed to a broadcast
		 * address outright (EACCES at the connect, or at the sendto for a bound
		 * socket), so a script that asks for it and does not get it cannot tell
		 * the difference between a wrong address and an unset flag. php reads
		 * it on both halves, the bound one included. */
#ifdef SO_BROADCAST
		setsockopt(sock,SOL_SOCKET,SO_BROADCAST,(const char *)&on,sizeof(on));
#endif
	}
}
/*
 * `bindto`: the LOCAL address a client socket takes before it connects, which
 * is how a program picks the interface (or the source port) its connection goes
 * out on. It is a NUMERIC literal and never a name (see the body), and when it
 * cannot be used php warns and connects from wherever the routing table would
 * have sent it — so a failure here is reported and never fatal.
 */
static int NetBindLocal(ph7_socket sock,int iFamily,const char *zHost,int iPort,int *pErrno)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr *pAddr;
	ph7_socklen nAddr;
	int rc;
	*pErrno = 0;
	/* php reads a local address as a NUMERIC literal and nothing else — it is
	 * inet_pton(), not the resolver — so `bindto => 'localhost:0'` is an
	 * Invalid IP Address there and binds nothing. It is parsed in the family of
	 * the socket that will carry it, so the answer describes THIS candidate and
	 * not the address family net.c prefers. (php's bracketed spelling, and what
	 * it does with a local address the candidate's family cannot take, is the
	 * recorded gap §7.4 slice-1 (b)(iii) names.) */
	if( iFamily == AF_INET6 ){
		memset(&sin6,0,sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port = htons((unsigned short)iPort);
		if( inet_pton(AF_INET6,zHost,&sin6.sin6_addr) != 1 ){
			return PH7_SOCKOPT_BIND_RESOLVE;
		}
		pAddr = (struct sockaddr *)&sin6;
		nAddr = (ph7_socklen)sizeof(sin6);
	}else{
		memset(&sin,0,sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons((unsigned short)iPort);
		if( inet_pton(AF_INET,zHost,&sin.sin_addr) != 1 ){
			return PH7_SOCKOPT_BIND_RESOLVE;
		}
		pAddr = (struct sockaddr *)&sin;
		nAddr = (ph7_socklen)sizeof(sin);
	}
	rc = bind(sock,pAddr,nAddr);
	if( rc != 0 ){
		*pErrno = PH7_NetLastError();
		return PH7_SOCKOPT_BIND_REFUSED;
	}
	return 0;
}
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
	int iBacklog, const ph7_sockopts *pOpt, int *pErrno, const char **pzErr)
{
	struct addrinfo hints, *res = 0, *rp;
	ph7_socket sock = PH7_NET_INVALID_SOCKET;
	int on = 1, iLastErr = 0;
	int iType = bDgram ? SOCK_DGRAM : SOCK_STREAM;
	if( pErrno ){ *pErrno = 0; }
	if( pzErr ){ *pzErr = ""; }
	if( PH7_NetEnsureInit() != PH7_OK ){
		if( pzErr ){ *pzErr = "Unable to start the networking subsystem"; }
		return PH7_NET_INVALID_SOCKET;
	}
	/* php RESOLVES the address it is asked to bind, family and all, and binds
	 * the first candidate that takes -- which is what makes `[::1]:0` an
	 * AF_INET6 listener and `0.0.0.0:0` an AF_INET one. This used to build a
	 * sockaddr_in by hand with three special cases in front of it, so every
	 * IPv6 address a script could write was refused by the resolver. */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = iType;
	hints.ai_flags = AI_PASSIVE;
	if( getaddrinfo((zHost && zHost[0]) ? zHost : 0, 0, &hints, &res) != 0 || res == 0 ){
		/* Nothing is open yet: the socket is created below. */
		if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }
		if( pzErr ){ *pzErr = 0; }
		return PH7_NET_INVALID_SOCKET;
	}
	if( pOpt && pOpt->iBacklog > 0 ){
		/* php's `backlog` context option: how many completed connections the OS
		 * may queue before the accept loop gets to them. */
		iBacklog = pOpt->iBacklog;
	}
	for( rp = res ; rp != 0 ; rp = rp->ai_next ){
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if( sock == PH7_NET_INVALID_SOCKET ){
			iLastErr = PH7_NetLastError();
			continue;
		}
		NetStampPort(rp->ai_addr, iPort);
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
		NetApplySockOpts(sock, pOpt);
		if( rp->ai_family == AF_INET6 && pOpt && pOpt->bV6Only ){
#ifdef IPV6_V6ONLY
			/* php's `ipv6_v6only` context option, which had no consumer at all
			 * while nothing here could open an AF_INET6 socket. */
			setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&on, sizeof(on));
#endif
		}
		if( bind(sock, rp->ai_addr, (ph7_socklen)rp->ai_addrlen) == 0
		 && (!bListen || bDgram || listen(sock, iBacklog) == 0) ){
			freeaddrinfo(res);
			return sock;
		}
		/* Read the error BEFORE closing the socket: close() is a call of its
		 * own and overwrites what the failure left behind. */
		iLastErr = PH7_NetLastError();
		PH7_NetClose(sock);
		sock = PH7_NET_INVALID_SOCKET;
	}
	freeaddrinfo(res);
	if( pErrno ){ *pErrno = iLastErr; }
	if( pzErr ){ *pzErr = PH7_NetStrError(iLastErr); }
	return PH7_NET_INVALID_SOCKET;
}
/*
 * Create a TCP listening socket bound to the given host and port.
 * Returns the socket descriptor, or PH7_NET_INVALID_SOCKET on error.
 */
PH7_PRIVATE ph7_socket PH7_NetListen(const char *zHost, int iPort, int iBacklog)
{
	return PH7_NetBind(zHost, iPort, 0, 1, iBacklog, 0, 0, 0);
}
/*
 * The host's own name. Winsock's gethostname() wants the library started,
 * which PH7_NetInit() has already done for any build that reaches here; on
 * POSIX it is the plain unistd call. The buffer is left EMPTY and the OS code
 * reported when the call fails, which is what php's warning names.
 */
PH7_PRIVATE int PH7_NetHostName(char *zBuf, int nBuf, int *pErrno)
{
	if( zBuf == 0 || nBuf < 2 ){
		return -1;
	}
	zBuf[0] = 0;
	if( gethostname(zBuf, (size_t)(nBuf - 1)) != 0 ){
		if( pErrno ){ *pErrno = PH7_NetLastError(); }
		zBuf[0] = 0;
		return -1;
	}
	/* A name too long for the buffer is truncated rather than terminated on
	 * some platforms; make the answer a C string either way. */
	zBuf[nBuf-1] = 0;
	return PH7_OK;
}
/*
 * The local (bPeer == 0) or the peer address of a socket, formatted the way
 * php's stream_socket_get_name() answers it: "ip:port". Returns PH7_OK, or -1
 * for a socket that has no such name (an unbound one, or the peer of a socket
 * that is not connected) — which is php's `false`.
 */
PH7_PRIVATE int PH7_NetSockName(ph7_socket sock, int bPeer, char *zBuf, int nBuf)
{
	struct sockaddr_storage addr;
	ph7_socklen nLen = (ph7_socklen)sizeof(addr);
	if( zBuf == 0 || nBuf < 2 ){
		return -1;
	}
	zBuf[0] = 0;
	memset(&addr, 0, sizeof(addr));
	if( (bPeer ? getpeername(sock, (struct sockaddr *)&addr, &nLen)
	           : getsockname(sock, (struct sockaddr *)&addr, &nLen)) != 0 ){
		return -1;
	}
	/* A sockaddr_in is too small to hold the answer for an IPv6 socket -- the
	 * OS truncates into whatever it is given -- so the buffer is the storage
	 * union. A socketpair has no address of any kind, and php answers false for
	 * one rather than inventing a name for it. */
	return NetFormatAddr((struct sockaddr *)&addr, zBuf, nBuf) ? PH7_OK : -1;
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
	struct sockaddr_storage addr;
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
	if( zPeer && nPeer > 0 ){
		NetFormatAddr((struct sockaddr *)&addr, zPeer, nPeer);
	}
	return sock;
}
/*
 * Did the connect() merely START? php's asynchronous dial puts the socket in
 * non-blocking mode first, and the "not finished yet" code IS the success it
 * reports back to the script.
 */
static int NetConnectInProgress(void)
{
#ifdef __WINNT__
	int iErr = WSAGetLastError();
	return iErr == WSAEWOULDBLOCK || iErr == WSAEINPROGRESS || iErr == WSAEALREADY;
#else
	return errno == EINPROGRESS || errno == EINTR || errno == EALREADY;
#endif
}
/*
 * Connect a socket to the given host and port (blocking unless bAsync; the
 * caller sets a timeout with PH7_NetSetTimeout afterwards). *pErrno receives
 * the OS error code and *pzErr a static description on failure, matching what
 * fsockopen() reports through its by-ref out-params.
 * Returns the connected socket, or PH7_NET_INVALID_SOCKET on error.
 */
PH7_PRIVATE ph7_socket PH7_NetConnect(const char *zHost, int iPort, int iTimeoutMs,
	int bDgram, int bAsync, ph7_sockopts *pOpt, int *pErrno, const char **pzErr)
{
	struct addrinfo hints, *res = 0, *rp;
	ph7_socket sock = PH7_NET_INVALID_SOCKET;
	int iLastErr = 0;
	if( pErrno ){ *pErrno = 0; }
	if( pzErr ){ *pzErr = ""; }
	if( PH7_NetEnsureInit() != PH7_OK ){
		if( pErrno ){ *pErrno = -1; }
		if( pzErr ){ *pzErr = "Unable to start the networking subsystem"; }
		return PH7_NET_INVALID_SOCKET;
	}
	if( zHost == 0 || zHost[0] == 0 ){
		/* An address whose host half is empty (`tcp://:9`, `[]:9`, an
		 * fsockopen() with no hostname) is a NAME php hands to the resolver
		 * like any other, and the resolver is what refuses it -- twice, in the
		 * two sentences the caller composes. This used to answer a message of
		 * PHL's own, "Empty host" with an errno of -1, that no php prints; the
		 * BIND half has always reported the resolver's. */
		if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }
		if( pzErr ){ *pzErr = 0; }
		return PH7_NET_INVALID_SOCKET;
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = bDgram ? SOCK_DGRAM : SOCK_STREAM;
	if( getaddrinfo(zHost, 0, &hints, &res) != 0 || res == 0 ){
		/* php words the HOST into this one and reports no OS code for it; the
		 * caller composes it, since only it has the name to interpolate. */
		if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }
		if( pzErr ){ *pzErr = 0; }
		return PH7_NET_INVALID_SOCKET;
	}
	for( rp = res ; rp != 0 ; rp = rp->ai_next ){
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if( sock == PH7_NET_INVALID_SOCKET ){
			iLastErr = PH7_NetLastError();
			continue;
		}
		NetStampPort(rp->ai_addr, iPort);
		NetApplySockOpts(sock, pOpt);
		if( pOpt ){
			/* Per CANDIDATE: the failure that gets reported belongs to the socket
			 * that ends up carrying the connection, not to one abandoned earlier.
			 * php connects anyway when the local bind cannot be made; the caller
			 * words the warning, since only it knows the function name. */
			pOpt->iBindErr = 0;
			pOpt->iBindErrno = 0;
			if( pOpt->zBindHost ){
				pOpt->iBindErr = NetBindLocal(sock, rp->ai_family, pOpt->zBindHost,
					pOpt->iBindPort, &pOpt->iBindErrno);
			}
		}
		if( iTimeoutMs > 0 ){
			/* the connect() itself stays blocking; the timeout bounds the
			 * subsequent recv/send (php applies it to both — recorded) */
			PH7_NetSetTimeout(sock, iTimeoutMs);
		}
		if( bAsync ){
			/* php's STREAM_CLIENT_ASYNC_CONNECT: the socket is put in
			 * non-blocking mode, connect() is ISSUED, and "in progress" is the
			 * answer the caller gets -- so a dial to a port nothing is
			 * listening on hands the script a working RESOURCE and reports the
			 * refusal later, at the first write. php then puts the socket back
			 * in blocking mode, which is why the handle reports `blocked`. */
			int rcA;
			PH7_NetSetBlocking(sock, 0);
			rcA = connect(sock, rp->ai_addr, (ph7_socklen)rp->ai_addrlen);
			if( rcA == 0 || NetConnectInProgress() ){
				PH7_NetSetBlocking(sock, 1);
				freeaddrinfo(res);
				return sock;
			}
			iLastErr = PH7_NetLastError();
			PH7_NetClose(sock);
			sock = PH7_NET_INVALID_SOCKET;
			continue;
		}
		if( connect(sock, rp->ai_addr, (ph7_socklen)rp->ai_addrlen) == 0 ){
			freeaddrinfo(res);
			return sock;
		}
		/* Read the code BEFORE closing: close() is a call of its own. php keeps
		 * the LAST candidate's, which is what a script reads back from
		 * $errno/$errstr -- this used to answer a hardcoded ECONNREFUSED for
		 * every failure, so a broadcast address refused for want of
		 * SO_BROADCAST, an unreachable network and a refused port were one
		 * answer. */
		iLastErr = PH7_NetLastError();
		PH7_NetClose(sock);
		sock = PH7_NET_INVALID_SOCKET;
	}
	freeaddrinfo(res);
	if( iLastErr == 0 ){
		iLastErr = 111; /* nothing was even tried: php's own default reason */
	}
	if( pErrno ){ *pErrno = iLastErr; }
	if( pzErr ){ *pzErr = PH7_NetStrError(iLastErr); }
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
	if( nBufLen > 0 ){
		zBuf[0] = 0;
	}
	if( pAddr == 0 || nBufLen < 2 ){
		return;
	}
	if( pAddr->sa_family == AF_INET ){
		const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;
		NetInetNtop(AF_INET, (const void *)&pIn->sin_addr, zBuf, nBufLen);
		return;
	}
	if( pAddr->sa_family == AF_INET6 ){
		const struct sockaddr_in6 *pIn6 = (const struct sockaddr_in6 *)pAddr;
		NetInetNtop(AF_INET6, (const void *)&pIn6->sin6_addr, zBuf, nBufLen);
		return;
	}
	/* A socketpair, or anything else with no address of its own. */
}
/*
 * Extract the port number from a sockaddr (in host byte order).
 */
PH7_PRIVATE int PH7_NetAddrPort(const struct sockaddr *pAddr)
{
	if( pAddr == 0 ){
		return 0;
	}
	if( pAddr->sa_family == AF_INET ){
		return (int)ntohs(((const struct sockaddr_in *)pAddr)->sin_port);
	}
	if( pAddr->sa_family == AF_INET6 ){
		return (int)ntohs(((const struct sockaddr_in6 *)pAddr)->sin6_port);
	}
	return 0;
}
/*
 * One sockaddr as php's stream_socket_get_name() spells it: `ip:port` for IPv4
 * and `[ip]:port` for IPv6 -- the BRACKETED form, which is the same spelling
 * every address argument in the family reads back. Answers 0 when the address
 * has no name at all (a socketpair), which is php's `false`.
 */
static int NetFormatAddr(const struct sockaddr *pAddr, char *zBuf, int nBuf)
{
	char zIp[80];
	int n;
	if( zBuf == 0 || nBuf < 2 ){
		return 0;
	}
	zBuf[0] = 0;
	PH7_NetAddrToString(pAddr, zIp, (int)sizeof(zIp));
	if( zIp[0] == 0 ){
		return 0;
	}
	n = pAddr->sa_family == AF_INET6
		? snprintf(zBuf, (size_t)nBuf, "[%s]:%d", zIp, PH7_NetAddrPort(pAddr))
		: snprintf(zBuf, (size_t)nBuf, "%s:%d", zIp, PH7_NetAddrPort(pAddr));
	if( n <= 0 || n >= nBuf ){
		zBuf[0] = 0;
		return 0;
	}
	return 1;
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
/*
 * php's feof() for a SOCKET is not a latch on a read that already happened: it
 * is a liveness probe run at the moment the question is asked (zend's
 * PHP_STREAM_OPTION_CHECK_LIVENESS). Poll the descriptor for readability with a
 * zero timeout and, only if something IS there, peek one byte: a peek that
 * comes back with data means the stream is alive, and one that comes back with
 * nothing (or with an error that is not "would have waited") means the far end
 * is gone. Everything else -- a listening socket with no connection queued, a
 * bound datagram socket with no datagram -- is not readable and therefore not
 * an end of file.
 *
 * The three answers this changes are all sockets nothing has read from yet: a
 * bound-but-not-listening one, a connected one whose peer has departed, and a
 * socketpair whose other end was closed. php reports EOF for all three before a
 * single read; PHL reported false until a read came back empty, so a
 * `while (!feof($sock))` loop written php's way ran one turn too many.
 */
PH7_PRIVATE int PH7_NetIsAlive(ph7_socket sock)
{
	char c;
	if( sock == PH7_NET_INVALID_SOCKET ){
		return 0;
	}
	if( PH7_NetWait(sock,0,0) <= 0 ){
		/* Not readable, or the wait itself failed: neither is evidence of an
		 * end, and php only looks further when the poll says there is
		 * something to look at. */
		return 1;
	}
#ifndef __WINNT__
	errno = 0; /* so a STALE EAGAIN cannot answer for this call's recv() */
#endif
	if( recv(sock,&c,1,MSG_PEEK) > 0 ){
		return 1;
	}
	return PH7_NetWouldBlock() ? 1 : 0;
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
	struct sockaddr_storage sFrom;
	ph7_socklen nFrom = (ph7_socklen)sizeof(sFrom);
	int n;
	if( zAddr && nAddr > 0 ){
		zAddr[0] = 0;
	}
	memset(&sFrom,0,sizeof(sFrom));
	n = (int)recvfrom(sock,(char *)pBuf,nLen,NetMsgFlags(iFlags),(struct sockaddr *)&sFrom,&nFrom);
#ifdef __WINNT__
	if( n < 0 && WSAGetLastError() == WSAEMSGSIZE ){
		/* A datagram LONGER than the buffer is a truncated read on POSIX and a
		 * failed call on Winsock -- which fills the buffer anyway and reports
		 * WSAEMSGSIZE for the part it dropped. php hands the truncated bytes
		 * back on both, so the two answer the same `wor` for a three-byte read
		 * of `world`; without this the whole datagram was lost to a false. */
		n = nLen;
	}
#endif
	if( n >= 0 && zAddr && nAddr > 0 ){
		NetFormatAddr((struct sockaddr *)&sFrom,zAddr,nAddr);
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
	struct sockaddr_storage sTo;
	ph7_socklen nTo;
	if( pErrno ){ *pErrno = 0; }
	if( zHost == 0 ){
		/* No $address at all: php's plain send() to whatever the socket is
		 * connected to. An EMPTY host is not this case -- it is a name the
		 * resolver refuses, which is what php answers for `stream_socket_sendto
		 * ($s, $d, 0, ':53')`. */
		return (int)send(sock,(const char *)pBuf,nLen,NetMsgFlags(iFlags));
	}
	memset(&sTo,0,sizeof(sTo));
	{
		/* php's own ladder: a NUMERIC literal is used as written -- in either
		 * family -- and only a name goes to the resolver. The family that comes
		 * back is the one sendto() is handed, so an IPv6 target on an IPv4
		 * socket is the OS's own refusal rather than a silent send elsewhere. */
		struct sockaddr_in *pIn = (struct sockaddr_in *)&sTo;
		struct sockaddr_in6 *pIn6 = (struct sockaddr_in6 *)&sTo;
		if( inet_pton(AF_INET,zHost,(void *)&pIn->sin_addr) == 1 ){
			pIn->sin_family = AF_INET;
			pIn->sin_port = htons((unsigned short)iPort);
			nTo = (ph7_socklen)sizeof(*pIn);
		}else if( inet_pton(AF_INET6,zHost,(void *)&pIn6->sin6_addr) == 1 ){
			pIn6->sin6_family = AF_INET6;
			pIn6->sin6_port = htons((unsigned short)iPort);
			nTo = (ph7_socklen)sizeof(*pIn6);
		}else{
			struct addrinfo hints,*res;
			memset(&hints,0,sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_DGRAM;
			if( zHost[0] == 0 || getaddrinfo(zHost,0,&hints,&res) != 0 || res == 0 ){
				/* Nothing was sent, and the caller says so in php's own three
				 * voices — which is a different answer from a send that failed. */
				if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }
				return -1;
			}
			memcpy(&sTo,res->ai_addr,(size_t)res->ai_addrlen);
			nTo = (ph7_socklen)res->ai_addrlen;
			NetStampPort((struct sockaddr *)&sTo,iPort);
			freeaddrinfo(res);
		}
	}
	return (int)sendto(sock,(const char *)pBuf,nLen,NetMsgFlags(iFlags),
		(struct sockaddr *)&sTo,nTo);
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
		listener = PH7_NetBind("127.0.0.1",0,0,1,1,0,pErrno,0);
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
