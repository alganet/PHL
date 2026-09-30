/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
/* AF_PACKET, SOCK_DCCP, the SO_* names newer than POSIX and the linux/filter.h
 * SKF_AD_* block are outside the strict subset a default compile exposes, and
 * php's own build asks for them the same way. This has to come BEFORE any
 * system header, which is why it sits above ph7int.h. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
/* macOS hides IPV6_HOPLIMIT and the rest of RFC 3542's names unless asked;
 * php's build asks on darwin, so php there has them. */
#if defined(__APPLE__) && !defined(__APPLE_USE_RFC_3542)
#define __APPLE_USE_RFC_3542 1
#endif
/*
 * inet_addr() and the ANSI WSA* entry points are php's own choices here, and
 * Winsock marks all three deprecated in favour of ones that behave differently
 * (inet_pton() refuses the shorthand forms php's inet_aton() takes). This has
 * to come before ph7int.h, which pulls winsock2.h in for the transport types.
 */
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#endif
#include "ph7int.h"
/*
 * Section:
 *    php's sockets extension: the BSD socket API as php shapes it -- an opaque
 *    `Socket` object, a per-socket errno beside a per-request one, and a
 *    warning-plus-false failure model rather than an exception.
 * Status:
 *    Stable.
 *
 * WHY THIS IS DERIVED AND NOT BOUND. There is no library under ext/sockets:
 * php's own is a thin shell over the platform's `socket()`/`bind()`/`recv()`,
 * so what had to be reproduced is the SHAPE php gives them and nothing else.
 * Every answer below was taken from php 8.5.9 on this box (and, for the
 * Windows halves, from a real `php.exe`), never from reading php's source.
 *
 *   THE ENGINE ALREADY HAD SOCKETS, and this is not them. net.c carries the
 *   transport under `tcp://`/`udp://` and the stream builtins: a socket there
 *   is a private detail of a stream, opened by URI, read through the io_private
 *   device stack and closed with fclose(). ext/sockets is the OTHER face -- the
 *   descriptor itself, with the address, the flags and the options exposed --
 *   so it keeps its own record and calls the platform directly. The two meet at
 *   exactly two doors, `socket_import_stream()` and `socket_export_stream()`,
 *   which are the reason vfs_stream.c grew a pair of accessors for it.
 *
 *   A FAILURE IS A WARNING, A false, AND TWO STORED ERRNOS. php's own
 *   PHP_SOCKET_ERROR records the code on the SOCKET (`socket_last_error($s)`)
 *   and in a per-request global (`socket_last_error()`), and prints
 *   "<what> [<code>]: <text>" as an E_WARNING -- except for EAGAIN,
 *   EWOULDBLOCK and EINPROGRESS, which are stored silently because a
 *   non-blocking program hits them as a matter of course. Nothing ever clears
 *   either one but `socket_clear_error()`.
 *
 *   A REFUSAL THE CALLER COULD HAVE AVOIDED IS A THROW. A domain php has no
 *   name for, a port outside 0..65535, a `$value` array missing the key its
 *   option needs, an element of `socket_select()`'s array that is not a Socket:
 *   those are ValueError/TypeError, raised before any system call. And a
 *   Socket whose descriptor was already closed is an `Error` -- php gives the
 *   whole extension one sentence for it, "Argument #N ($name) has already been
 *   closed", because a closed handle is a program bug rather than a network
 *   condition.
 *
 *   A HOST LOOKUP FAILURE IS NOT AN ERRNO. php reports it as `-10000 - h_errno`
 *   so it cannot collide with one, and `socket_strerror()` reads a code below
 *   -10000 back through hstrerror(). That is why `socket_bind($s,
 *   '256.256.256.256', 0)` answers -10000 and a name that does not resolve
 *   answers -10001.
 *
 *   php BUILDS THIS ON WINDOWS, unlike ext/posix and ext/pcntl -- so this unit
 *   is not #ifdef'd away there. What differs is the constant SET (no AF_PACKET,
 *   no SCM_RIGHTS, no BPF names) and the error NUMBERS: a Winsock code is not
 *   an errno, so `SOCKET_EWOULDBLOCK` is 10035 there and 11 here. Both faces
 *   come from the platform's own macros, which is what keeps them right.
 */
#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)
#include <string.h>
#include <stdio.h>
#ifdef __WINNT__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
/* `struct sockaddr_un`: Windows 10 has AF_UNIX and php's ext/sockets uses it. */
#include <afunix.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <netinet/udp.h>
#include <linux/filter.h>
#include <linux/if_ether.h>
#endif
#endif /* __WINNT__ */

/* php reports a host-lookup failure in a range no errno occupies. */
#define PHL_SOCK_HERR(h) (-10000 - (int)(h))
/*
 * The length a socket call takes. POSIX says size_t, Winsock says int, and
 * MSVC's /WX makes the difference an error rather than a warning.
 */
#ifdef __WINNT__
typedef int phl_sock_size;
#else
typedef size_t phl_sock_size;
#endif
/*
 * The ancillary-data layer. Windows spells every one of these WSA_*, and its
 * header is a WSACMSGHDR rather than a `struct cmsghdr` -- but the LAYOUT rule
 * is the same on both, which is what lets the control block be built and walked
 * by hand here rather than through the FIRSTHDR/NXTHDR macros (those take a
 * msghdr, and the two platforms have no such struct in common).
 */
#ifdef __WINNT__
typedef WSACMSGHDR phl_cmsghdr;
#define PHL_CMSG_SPACE(n) WSA_CMSG_SPACE(n)
#define PHL_CMSG_LEN(n)   WSA_CMSG_LEN(n)
#define PHL_CMSG_DATA(c)  WSA_CMSG_DATA(c)
#else
typedef struct cmsghdr phl_cmsghdr;
#define PHL_CMSG_SPACE(n) CMSG_SPACE(n)
#define PHL_CMSG_LEN(n)   CMSG_LEN(n)
#define PHL_CMSG_DATA(c)  CMSG_DATA(c)
#endif
/*
 * The two creation flags php masks out of `$type` before it screens the range.
 * A platform without them screens the same numbers -- there is simply nothing
 * to mask -- so 0 is the right stand-in rather than an #ifdef at each use.
 */
#ifdef SOCK_CLOEXEC
#define PHL_SOCK_CLOEXEC SOCK_CLOEXEC
#else
#define PHL_SOCK_CLOEXEC 0
#endif
#ifdef SOCK_NONBLOCK
#define PHL_SOCK_NONBLOCK SOCK_NONBLOCK
#else
#define PHL_SOCK_NONBLOCK 0
#endif
/*
 * The two options whose VALUE is a string. A platform without one still has to
 * compile the branch that reads it, so a stand-in number no option occupies is
 * what stops that branch matching anything.
 */
#ifdef SO_BINDTODEVICE
#define PHL_SO_BINDTODEVICE SO_BINDTODEVICE
#else
#define PHL_SO_BINDTODEVICE (-1)
#endif
#ifdef TCP_CONGESTION
#define PHL_TCP_CONGESTION TCP_CONGESTION
#else
#define PHL_TCP_CONGESTION (-1)
#endif
/*
 * The most a single read may ask for. php hands its allocator the whole length
 * and dies if it cannot have it; the chunk allocator here takes an unsigned
 * int, so anything past this is refused with the same out-of-memory fatal
 * rather than allocated short and then read into at full length -- which is a
 * heap overflow rather than a wrong answer.
 */
#define PHL_SOCK_MAXBUF ((ph7_int64)0x7FFFFFFE)
/* ...and php's own cap on a recvmsg() buffer, which is smaller and worded. */
#define PHL_SOCK_MSGBUF ((ph7_int64)104857600)

typedef struct phl_socket phl_socket;
typedef struct phl_addrinfo phl_addrinfo;
/*
 * The record behind a `Socket`. It is reached from the object through the
 * hidden `__res` slot and is ALSO chained on the per-VM registry, because a PH7
 * resource carries no destructor: the sweep at VM reset/release is what closes
 * a descriptor a script left open.
 */
struct phl_socket
{
	ph7_vm *pVm;
	ph7_socket sock;      /* PH7_NET_INVALID_SOCKET once socket_close() ran */
	int iDomain;          /* AF_* -- remembered because the ADDRESS shape follows it */
	int iType;            /* SOCK_*, with the CLOEXEC/NONBLOCK bits stripped */
	int iProtocol;
	int iError;           /* php's per-socket errno (socket_last_error($s)) */
	int bBlocking;        /* php's own flag; the descriptor carries the truth */
	int bExported;        /* socket_export_stream() handed the descriptor to a stream,
	                       * which now owns the close */
	void *pStream;        /* the io_private that stream is, when bExported. php answers
	                       * the SAME handle to a second socket_export_stream() and
	                       * closes it from socket_close(), so the record keeps it */
	ph7_class_instance *pOwner;
	phl_socket *pNext;
};
/*
 * The record behind an `AddressInfo`. php hands back one object per candidate
 * getaddrinfo() answered and keeps the whole list alive behind them; this keeps
 * a per-object COPY instead, so freeing one never disturbs another and there is
 * no shared `freeaddrinfo` to sequence.
 */
struct phl_addrinfo
{
	ph7_vm *pVm;
	int iFlags;
	int iFamily;
	int iSockType;
	int iProtocol;
	struct sockaddr_storage sAddr;
	ph7_socklen nAddr;
	char *zCanon;         /* ai_canonname, or 0 */
	ph7_class_instance *pOwner;
	phl_addrinfo *pNext;
};

/* ------------------------------------------------------------------------
 * Handle lifetime
 * ------------------------------------------------------------------------ */
/* The record behind a `__res` slot value; shared with the AddressInfo half. */
static void * SockSlotOf(ph7_class_instance *pThis)
{
	SyString sAttr;
	ph7_value *pVal;
	if( pThis == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pVal = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pVal == 0 || !ph7_value_is_resource(pVal) ){
		return 0;
	}
	return pVal->x.pOther;
}
static int SockSlotAttach(ph7_class_instance *pThis,void *pRec)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return -1;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 ){
		return -1;
	}
	PH7_MemObjRelease(pRes);
	pRes->x.pOther = pRec;
	MemObjSetType(pRes,MEMOBJ_RES);
	return 0;
}
static phl_socket * SockNew(ph7_vm *pVm,ph7_socket sock,int iDomain,int iType,int iProtocol)
{
	phl_socket *pSock = (phl_socket *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_socket));
	if( pSock == 0 ){
		return 0;
	}
	SyZero(pSock,sizeof(phl_socket));
	pSock->pVm = pVm;
	pSock->sock = sock;
	pSock->iDomain = iDomain;
	pSock->iType = iType;
	pSock->iProtocol = iProtocol;
	pSock->bBlocking = 1;
	pSock->pNext = (phl_socket *)pVm->pSockets;
	pVm->pSockets = pSock;
	return pSock;
}
/*
 * Close the descriptor a record holds, leaving the record itself on the chain:
 * php's `socket_close()` does exactly this, which is why every later verb can
 * still tell a CLOSED Socket from one the engine never made and raise its own
 * "has already been closed" rather than a type error.
 */
static void SockShut(phl_socket *pSock)
{
	if( pSock->sock != PH7_NET_INVALID_SOCKET ){
		if( !pSock->bExported ){
			PH7_NetClose(pSock->sock);
		}
		pSock->sock = PH7_NET_INVALID_SOCKET;
	}
}
static void SockFreeAddrInfo(phl_addrinfo *pAi)
{
	if( pAi->zCanon ){
		SyMemBackendFree(&pAi->pVm->sAllocator,pAi->zCanon);
		pAi->zCanon = 0;
	}
}
/* The two per-object release hooks: an object dropped before its record was. */
static void SockInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_socket *pSock = (phl_socket *)SockSlotOf(pThis);
	SXUNUSED(pVm);
	if( pSock == 0 || pSock->pOwner != pThis ){
		return;
	}
	SockShut(pSock);
	pSock->pOwner = 0;
}
static void SockAddrInfoRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_addrinfo *pAi = (phl_addrinfo *)SockSlotOf(pThis);
	SXUNUSED(pVm);
	if( pAi == 0 || pAi->pOwner != pThis ){
		return;
	}
	SockFreeAddrInfo(pAi);
	pAi->pOwner = 0;
}
/*
 * Free every registered record. Called from PH7_SocketsVmReset (a reused VM --
 * the -S server's -- must not answer the next request through a descriptor the
 * previous one opened) and from PH7_SocketsVmRelease before the allocator
 * holding the shells is torn down.
 */
static void SockVmSweep(ph7_vm *pVm)
{
	phl_socket *pSock = (phl_socket *)pVm->pSockets;
	phl_addrinfo *pAi = (phl_addrinfo *)pVm->pAddrInfos;
	while( pSock ){
		phl_socket *pNext = pSock->pNext;
		SockShut(pSock);
		SyMemBackendFree(&pVm->sAllocator,pSock);
		pSock = pNext;
	}
	pVm->pSockets = 0;
	while( pAi ){
		phl_addrinfo *pNext = pAi->pNext;
		SockFreeAddrInfo(pAi);
		SyMemBackendFree(&pVm->sAllocator,pAi);
		pAi = pNext;
	}
	pVm->pAddrInfos = 0;
}
PH7_PRIVATE void PH7_SocketsVmReset(ph7_vm *pVm)
{
	SockVmSweep(pVm);
	pVm->iSocketLastErr = 0;
}
PH7_PRIVATE void PH7_SocketsVmRelease(ph7_vm *pVm)
{
	SockVmSweep(pVm);
}

/* ------------------------------------------------------------------------
 * php's failure model
 * ------------------------------------------------------------------------ */
/*
 * php's PHP_SOCKET_ERROR: the code lands on the socket AND in the per-request
 * global, and the warning is printed for everything but the three codes a
 * non-blocking program meets in normal operation. `pSock` may be 0 -- the
 * lookup failures in socket_addrinfo_lookup() have no socket to blame.
 */
static void SockFail(ph7_context *pCtx,phl_socket *pSock,const char *zWhat,int iErr)
{
	if( pSock ){
		pSock->iError = iErr;
	}
	pCtx->pVm->iSocketLastErr = iErr;
#ifdef __WINNT__
	if( iErr != WSAEWOULDBLOCK && iErr != WSAEINPROGRESS ){
#else
	if( iErr != EAGAIN && iErr != EWOULDBLOCK && iErr != EINPROGRESS ){
#endif
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s [%d]: %s",
			zWhat,iErr,PH7_SocketStrError(iErr));
	}
}
/* The same, for the errno the platform just set. */
static void SockFailLast(ph7_context *pCtx,phl_socket *pSock,const char *zWhat)
{
	SockFail(pCtx,pSock,zWhat,PH7_NetLastError());
}
/*
 * php's socket_strerror(): a code below -10000 is a host-lookup failure carried
 * in a range no errno occupies, and is read back through hstrerror(); anything
 * else is the platform's own. Shared with the builtin, hence the exported name.
 */
PH7_PRIVATE const char * PH7_SocketStrError(int iErr)
{
#ifdef __WINNT__
	/*
	 * php answers FormatMessage()'s prose here -- "A non-blocking socket
	 * operation could not be completed immediately" for 10035 -- rather than
	 * the short errno texts net.c words for the stream layer, and the EMPTY
	 * string for a code the system has no message for. The trailing CRLF that
	 * FormatMessage appends is not part of php's answer.
	 */
	static char zBuf[512];
	DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
		0,(DWORD)iErr,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
		zBuf,(DWORD)sizeof(zBuf) - 1,0);
	/* php trims the trailing CRLF **and the full stop** FormatMessage puts on
	 * every one of these: its answer for 10022 is "An invalid argument was
	 * supplied", not "...supplied.". */
	while( n > 0 && (zBuf[n-1] == '\r' || zBuf[n-1] == '\n' || zBuf[n-1] == ' '
	              || zBuf[n-1] == '.') ){
		n--;
	}
	zBuf[n] = 0;
	return zBuf;
#else
	if( iErr < -10000 ){
		return hstrerror(-iErr - 10000);
	}
	return PH7_NetStrError(iErr);
#endif
}

/* ------------------------------------------------------------------------
 * Argument screens
 * ------------------------------------------------------------------------ */
/*
 * The Socket (or AddressInfo) argument of every verb. The signature table has
 * already screened the TYPE, so a missing record means the engine tore the
 * object down; a record whose descriptor is gone is php's own `Error`, one
 * sentence for the whole extension.
 */
static phl_socket * SockArg(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zName,int *pRc)
{
	ph7_class_instance *pThis;
	phl_socket *pSock;
	*pRc = PH7_OK;
	pThis = (pArg && ph7_value_is_object(pArg)) ? (ph7_class_instance *)pArg->x.pOther : 0;
	pSock = (phl_socket *)SockSlotOf(pThis);
	if( pSock == 0 ){
		ph7_result_bool(pCtx,0);
		return 0;
	}
	if( pSock->sock == PH7_NET_INVALID_SOCKET ){
		*pRc = PH7_VmThrowException(pCtx,"Error",
			"%z(): Argument #%d ($%s) has already been closed",
			&pCtx->pFunc->sName,iPos,zName);
		return 0;
	}
	return pSock;
}
static phl_addrinfo * SockAddrInfoArg(ph7_context *pCtx,ph7_value *pArg)
{
	ph7_class_instance *pThis = (pArg && ph7_value_is_object(pArg)) ?
		(ph7_class_instance *)pArg->x.pOther : 0;
	SXUNUSED(pCtx);
	return (phl_addrinfo *)SockSlotOf(pThis);
}
/* php's port screen: every door that takes one refuses the same range. */
static int SockPortArg(ph7_context *pCtx,ph7_int64 iPort,int iPos,const char *zName,int *pRc)
{
	if( iPort < 0 || iPort > 65535 ){
		*pRc = PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #%d ($%s) must be between 0 and 65535",
			&pCtx->pFunc->sName,iPos,zName);
		return -1;
	}
	*pRc = PH7_OK;
	return (int)iPort;
}

/* ------------------------------------------------------------------------
 * Addresses
 * ------------------------------------------------------------------------ */
/*
 * php's IPv4 name resolution, and it has to be the REENTRANT call rather than
 * plain gethostbyname() -- not for thread safety but because the code php
 * reports depends on it. gethostbyname_r() writes its error into the out
 * parameter and leaves the process-wide `h_errno` to whatever glibc's resolver
 * happened to set on the way, and `-10000 - h_errno` is what php prints: that
 * is why `256.256.256.256` answers -10000 (h_errno untouched, 0) where
 * `no.such.host.invalid` answers -10001 and `notanaddr` answers -10002, all
 * three from the same failed lookup. gethostbyname() sets h_errno itself and
 * would answer -10001 for all three.
 */
static int SockResolve4(const char *zHost,struct in_addr *pOut)
{
#if defined(__GLIBC__) && !defined(__WINNT__)
	struct hostent sHe,*pRes = 0;
	char zBuf[4096];
	int iErr = 0;
	/* h_errno is NOT cleared first, and that is deliberate: php never clears it
	 * either, so a lookup glibc leaves it alone for reports whatever the LAST
	 * one set. `socket_bind($s,'256.256.256.256',0)` is -10000 in a fresh
	 * process and -10001 right after a failed name lookup, under both engines. */
	if( gethostbyname_r(zHost,&sHe,zBuf,sizeof(zBuf),&pRes,&iErr) < 0 || pRes == 0 ){
		return -1;
	}
#else
	struct hostent *pRes = gethostbyname(zHost);
	if( pRes == 0 ){
		return -1;
	}
#endif
	if( pRes->h_addrtype != AF_INET || pRes->h_addr_list[0] == 0 ){
		return -1;
	}
	SyMemcpy((const void *)pRes->h_addr_list[0],(void *)pOut,sizeof(struct in_addr));
	return 0;
}
/*
 * Build the sockaddr a verb was asked for out of php's (string $address,
 * ?int $port) pair. php resolves a name that is not already numeric -- with
 * gethostbyname() for AF_INET and getaddrinfo() for AF_INET6 -- and reports a
 * miss as "Host lookup failed" in the -10000 range.
 *
 * Answers the length to hand the system call, or 0 after reporting the failure
 * itself (the caller has only to answer false), or -1 after THROWING, which is
 * what a null port for an internet socket and an over-long AF_UNIX path are.
 */
static int SockBuildAddr(ph7_context *pCtx,phl_socket *pSock,const char *zAddr,int nAddr,
	int bHasPort,ph7_int64 iPort,int iPortPos,const char *zPortName,
	struct sockaddr_storage *pOut,int *pRc)
{
	*pRc = PH7_OK;
	SyZero(pOut,sizeof(*pOut));
	if( pSock->iDomain == AF_UNIX ){
		struct sockaddr_un *pUn = (struct sockaddr_un *)pOut;
		if( nAddr >= (int)sizeof(pUn->sun_path) ){
			*pRc = PH7_VmThrowException(pCtx,"ValueError",
				"%z(): Argument #2 ($address) must be less than %d bytes",
				&pCtx->pFunc->sName,(int)sizeof(pUn->sun_path));
			return -1;
		}
		pUn->sun_family = AF_UNIX;
		if( nAddr > 0 ){
			SyMemcpy(zAddr,pUn->sun_path,(sxu32)nAddr);
		}
		return (int)(sizeof(pUn->sun_family) + (sxu32)nAddr);
	}
	if( !bHasPort ){
		/* php names the FAMILY it screened for, and calls both internet
		 * families AF_INET in this one sentence. */
		*pRc = PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #%d ($%s) cannot be null when the socket type is AF_INET",
			&pCtx->pFunc->sName,iPortPos,zPortName);
		return -1;
	}
	if( SockPortArg(pCtx,iPort,iPortPos,zPortName,pRc) < 0 ){
		return -1;
	}
	if( pSock->iDomain == AF_INET6 ){
		struct sockaddr_in6 *pIn6 = (struct sockaddr_in6 *)pOut;
		pIn6->sin6_family = AF_INET6;
		pIn6->sin6_port = htons((unsigned short)iPort);
		if( inet_pton(AF_INET6,zAddr,&pIn6->sin6_addr) != 1 ){
			struct addrinfo sHint,*pRes = 0;
			SyZero(&sHint,sizeof(sHint));
			sHint.ai_family = AF_INET6;
			sHint.ai_socktype = SOCK_DGRAM;
			if( getaddrinfo(zAddr,0,&sHint,&pRes) != 0 || pRes == 0 ){
				if( pRes ){
					freeaddrinfo(pRes);
				}
				SockFail(pCtx,pSock,"Host lookup failed",PHL_SOCK_HERR(h_errno));
				return 0;
			}
			SyMemcpy((const void *)&((struct sockaddr_in6 *)pRes->ai_addr)->sin6_addr,
				(void *)&pIn6->sin6_addr,sizeof(struct in6_addr));
			freeaddrinfo(pRes);
		}
		return (int)sizeof(struct sockaddr_in6);
	}
	{
		struct sockaddr_in *pIn = (struct sockaddr_in *)pOut;
		pIn->sin_family = AF_INET;
		pIn->sin_port = htons((unsigned short)iPort);
#ifdef __WINNT__
		pIn->sin_addr.s_addr = inet_addr(zAddr);
		if( pIn->sin_addr.s_addr == INADDR_NONE && SyStrncmp(zAddr,"255.255.255.255",16) != 0 ){
#else
		if( inet_aton(zAddr,&pIn->sin_addr) == 0 ){
#endif
			if( SockResolve4(zAddr,&pIn->sin_addr) != 0 ){
				SockFail(pCtx,pSock,"Host lookup failed",PHL_SOCK_HERR(h_errno));
				return 0;
			}
		}
		return (int)sizeof(struct sockaddr_in);
	}
}
/*
 * Report a sockaddr back through php's (&$address, &$port) pair. An AF_UNIX
 * socket writes the PATH and leaves the port argument exactly as the caller
 * left it, which is php's own asymmetry rather than a null it stores.
 */
static void SockReportAddr(ph7_context *pCtx,const struct sockaddr *pAddr,
	ph7_value *pAddrArg,ph7_value *pPortArg)
{
	char zBuf[INET6_ADDRSTRLEN+1];
	ph7_value *pTmp = ph7_context_new_scalar(pCtx);
	int iPort = -1;
	if( pTmp == 0 ){
		return;
	}
	zBuf[0] = 0;
	if( pAddr->sa_family == AF_INET6 ){
		const struct sockaddr_in6 *pIn6 = (const struct sockaddr_in6 *)pAddr;
		inet_ntop(AF_INET6,(const void *)&pIn6->sin6_addr,zBuf,sizeof(zBuf));
		iPort = (int)ntohs(pIn6->sin6_port);
		ph7_value_string(pTmp,zBuf,-1);
	}else if( pAddr->sa_family == AF_UNIX ){
		const struct sockaddr_un *pUn = (const struct sockaddr_un *)pAddr;
		ph7_value_string(pTmp,pUn->sun_path,(int)SyStrlen(pUn->sun_path));
	}else{
		const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;
		inet_ntop(AF_INET,(const void *)&pIn->sin_addr,zBuf,sizeof(zBuf));
		iPort = (int)ntohs(pIn->sin_port);
		ph7_value_string(pTmp,zBuf,-1);
	}
	PH7_VmStoreArgByRef(pCtx->pVm,pAddrArg,pTmp);
	if( pPortArg && iPort >= 0 ){
		ph7_value_int(pTmp,iPort);
		PH7_VmStoreArgByRef(pCtx->pVm,pPortArg,pTmp);
	}
	ph7_context_release_value(pCtx,pTmp);
}
/* Hand a fresh record back as the `Socket` object php answers with. */
static int SockResultObject(ph7_context *pCtx,ph7_socket sock,int iDomain,int iType,int iProtocol)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = PH7_VmExtractClass(pVm,"Socket",sizeof("Socket")-1,0,0);
	ph7_class_instance *pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
	phl_socket *pSock;
	if( pThis == 0 ){
		PH7_NetClose(sock);
		return PH7_ContextMemoryError(pCtx);
	}
	pSock = SockNew(pVm,sock,iDomain,iType,iProtocol);
	if( pSock == 0 || SockSlotAttach(pThis,(void *)pSock) != 0 ){
		PH7_ClassInstanceUnref(pThis);
		PH7_NetClose(sock);
		return PH7_ContextMemoryError(pCtx);
	}
	pSock->pOwner = pThis;
	PH7_NativeResultObject(pCtx,pThis);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Creating and destroying a socket
 * ------------------------------------------------------------------------ */
/* Socket|false socket_create(int $domain, int $type, int $protocol) */
static int vm_builtin_socket_create(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 iDomain,iType,iProto;
	ph7_socket sock;
	SXUNUSED(nArg);
	iDomain = ph7_value_to_int64(apArg[0]);
	iType   = ph7_value_to_int64(apArg[1]);
	iProto  = ph7_value_to_int64(apArg[2]);
	if( iDomain != AF_UNIX && iDomain != AF_INET && iDomain != AF_INET6
#ifdef AF_PACKET
	 && iDomain != AF_PACKET
#endif
	){
		/* php words this the same everywhere, AF_PACKET included, even on a
		 * Windows build that has no such family -- verified against a real
		 * php.exe rather than assumed. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #1 ($domain) must be one of "
			"AF_UNIX, AF_PACKET, AF_INET6, or AF_INET",&pCtx->pFunc->sName);
	}
	/*
	 * php screens the TYPE by RANGE rather than by name: everything up to
	 * SOCK_DCCP (10) passes, the two creation flags are masked out first, and a
	 * negative number passes too and is refused by socket() itself. So
	 * `socket_create(AF_INET, 7, 0)` is a warning-and-false, not a throw.
	 */
	if( (iType & ~(ph7_int64)(PHL_SOCK_CLOEXEC|PHL_SOCK_NONBLOCK)) > 10 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #2 ($type) must be one of SOCK_STREAM, SOCK_DGRAM, "
			"SOCK_SEQPACKET, SOCK_RAW, or SOCK_RDM optionally OR'ed with "
			"SOCK_CLOEXEC, SOCK_NONBLOCK",&pCtx->pFunc->sName);
	}
	PH7_NetEnsureInit();
	sock = socket((int)iDomain,(int)iType,(int)iProto);
	if( sock == PH7_NET_INVALID_SOCKET ){
		SockFailLast(pCtx,0,"Unable to create socket");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return SockResultObject(pCtx,sock,(int)iDomain,
		(int)(iType & ~(ph7_int64)(PHL_SOCK_CLOEXEC|PHL_SOCK_NONBLOCK)),(int)iProto);
}
/*
 * Socket|false socket_create_listen(int $port, int $backlog = 4096)
 *
 * php's one-call server: an AF_INET stream socket bound to INADDR_ANY and
 * listening. SO_REUSEADDR is set BEFORE the bind, which is why a program can
 * restart on the same port.
 */
static int vm_builtin_socket_create_listen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct sockaddr_in sIn;
	ph7_socket sock;
	int iPort,iBacklog = SOMAXCONN,iOn = 1,rc;
	iPort = SockPortArg(pCtx,ph7_value_to_int64(apArg[0]),1,"port",&rc);
	if( iPort < 0 ){
		return rc;
	}
	if( nArg > 1 ){
		iBacklog = (int)ph7_value_to_int64(apArg[1]);
	}
	PH7_NetEnsureInit();
	sock = socket(AF_INET,SOCK_STREAM,0);
	if( sock == PH7_NET_INVALID_SOCKET ){
		SockFailLast(pCtx,0,"Unable to create socket");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char *)&iOn,sizeof(iOn));
	SyZero(&sIn,sizeof(sIn));
	sIn.sin_family = AF_INET;
	sIn.sin_addr.s_addr = htonl(INADDR_ANY);
	sIn.sin_port = htons((unsigned short)iPort);
	if( bind(sock,(struct sockaddr *)&sIn,sizeof(sIn)) != 0 ){
		SockFailLast(pCtx,0,"unable to bind to given address");
		PH7_NetClose(sock);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( listen(sock,iBacklog) != 0 ){
		SockFailLast(pCtx,0,"unable to listen on socket");
		PH7_NetClose(sock);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return SockResultObject(pCtx,sock,AF_INET,SOCK_STREAM,0);
}
/* bool socket_create_pair(int $domain, int $type, int $protocol, &$pair) */
static int vm_builtin_socket_create_pair(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_socket aSock[2];
	ph7_value *pArr,*pVal;
	int iDomain,iType,iProto,iErrno = 0,i;
	SXUNUSED(nArg);
	iDomain = (int)ph7_value_to_int64(apArg[0]);
	iType   = (int)ph7_value_to_int64(apArg[1]);
	iProto  = (int)ph7_value_to_int64(apArg[2]);
	if( iDomain != AF_UNIX && iDomain != AF_INET && iDomain != AF_INET6 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #1 ($domain) must be one of AF_UNIX, AF_INET6, or AF_INET",
			&pCtx->pFunc->sName);
	}
	if( (iType & ~(PHL_SOCK_CLOEXEC|PHL_SOCK_NONBLOCK)) > 10 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #2 ($type) must be one of SOCK_STREAM, SOCK_DGRAM, "
			"SOCK_SEQPACKET, SOCK_RAW, or SOCK_RDM optionally OR'ed with "
			"SOCK_CLOEXEC, SOCK_NONBLOCK",&pCtx->pFunc->sName);
	}
	PH7_NetEnsureInit();
	if( PH7_NetSocketPair(iDomain,iType,iProto,aSock,&iErrno) != PH7_OK ){
		SockFail(pCtx,0,"Unable to create socket pair",iErrno);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArr = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pVal == 0 ){
		PH7_NetClose(aSock[0]);
		PH7_NetClose(aSock[1]);
		return PH7_ContextMemoryError(pCtx);
	}
	for( i = 0 ; i < 2 ; ++i ){
		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,"Socket",sizeof("Socket")-1,0,0);
		ph7_class_instance *pThis = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;
		phl_socket *pRec = pThis ? SockNew(pCtx->pVm,aSock[i],iDomain,iType,iProto) : 0;
		if( pRec == 0 || SockSlotAttach(pThis,(void *)pRec) != 0 ){
			if( pThis ){
				PH7_ClassInstanceUnref(pThis);
			}
			PH7_NetClose(aSock[i]);
			if( i == 0 ){
				PH7_NetClose(aSock[1]);
			}
			return PH7_ContextMemoryError(pCtx);
		}
		pRec->pOwner = pThis;
		PH7_MemObjRelease(pVal);
		pVal->x.pOther = pThis;
		pVal->iFlags = MEMOBJ_OBJ;
		ph7_array_add_elem(pArr,0,pVal);
		/* The array took its own reference; drop the creation one. */
		PH7_ClassInstanceUnref(pThis);
		pVal->iFlags = MEMOBJ_NULL;
		pVal->x.pOther = 0;
	}
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[3],pArr);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * void socket_close(Socket $socket)
 *
 * A SECOND close is php's "has already been closed" Error, not a no-op -- the
 * same screen every other verb runs, which is why this goes through SockArg().
 */
static int vm_builtin_socket_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_socket *pSock;
	int rc;
	SXUNUSED(nArg);
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	/*
	 * php closes an EXPORTED socket through its stream: the two share one
	 * descriptor, and closing the raw number would leave a live stream handle
	 * pointing at a recycled one. This is also why `fwrite()` on that handle
	 * afterwards is php's "must be an open stream resource" rather than a
	 * write to whatever opened next.
	 */
	if( pSock->bExported && pSock->pStream ){
		PH7_StreamCloseExported((io_private *)pSock->pStream);
		pSock->pStream = 0;
		pSock->bExported = 0;
		pSock->sock = PH7_NET_INVALID_SOCKET;
	}
	SockShut(pSock);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Naming, listening and accepting
 * ------------------------------------------------------------------------ */
/* bool socket_bind(Socket $socket, string $address, int $port = 0) */
static int vm_builtin_socket_bind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct sockaddr_storage sAddr;
	phl_socket *pSock;
	const char *zAddr;
	int nAddr = 0,nLen,rc;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	zAddr = ph7_value_to_string(apArg[1],&nAddr);
	nLen = SockBuildAddr(pCtx,pSock,zAddr,nAddr,1,
		nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0,3,"port",&sAddr,&rc);
	if( nLen <= 0 ){
		if( nLen < 0 ){
			return rc;
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( bind(pSock->sock,(struct sockaddr *)&sAddr,(ph7_socklen)nLen) != 0 ){
		SockFailLast(pCtx,pSock,"Unable to bind address");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool socket_connect(Socket $socket, string $address, ?int $port = null) */
static int vm_builtin_socket_connect(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct sockaddr_storage sAddr;
	phl_socket *pSock;
	const char *zAddr;
	int nAddr = 0,nLen,bHasPort,rc;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	zAddr = ph7_value_to_string(apArg[1],&nAddr);
	bHasPort = (nArg > 2 && !ph7_value_is_null(apArg[2]));
	nLen = SockBuildAddr(pCtx,pSock,zAddr,nAddr,bHasPort,
		bHasPort ? ph7_value_to_int64(apArg[2]) : 0,3,"port",&sAddr,&rc);
	if( nLen <= 0 ){
		if( nLen < 0 ){
			return rc;
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( connect(pSock->sock,(struct sockaddr *)&sAddr,(ph7_socklen)nLen) != 0 ){
		SockFailLast(pCtx,pSock,"unable to connect");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool socket_listen(Socket $socket, int $backlog = 0) */
static int vm_builtin_socket_listen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_socket *pSock;
	int rc;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	if( listen(pSock->sock,nArg > 1 ? (int)ph7_value_to_int64(apArg[1]) : 0) != 0 ){
		SockFailLast(pCtx,pSock,"Unable to listen on socket");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Socket|false socket_accept(Socket $socket)
 *
 * php stores the failure on the socket it was ABOUT to hand back rather than on
 * the listener -- so `socket_last_error($listener)` after a would-block accept
 * is 0 while `socket_last_error()` is EAGAIN. That asymmetry is visible and is
 * reproduced by passing no socket to the reporter.
 */
static int vm_builtin_socket_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct sockaddr_storage sAddr;
	ph7_socklen nAddr = (ph7_socklen)sizeof(sAddr);
	phl_socket *pSock;
	ph7_socket sock;
	int rc;
	SXUNUSED(nArg);
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	sock = accept(pSock->sock,(struct sockaddr *)&sAddr,&nAddr);
	if( sock == PH7_NET_INVALID_SOCKET ){
		SockFailLast(pCtx,0,"unable to accept incoming connection");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return SockResultObject(pCtx,sock,pSock->iDomain,pSock->iType,pSock->iProtocol);
}
/* bool socket_getsockname / socket_getpeername (Socket, &$address, &$port = null) */
static int SockName(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPeer)
{
	struct sockaddr_storage sAddr;
	ph7_socklen nAddr = (ph7_socklen)sizeof(sAddr);
	phl_socket *pSock;
	int rc,iRet;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	SyZero(&sAddr,sizeof(sAddr));
	iRet = bPeer ? getpeername(pSock->sock,(struct sockaddr *)&sAddr,&nAddr)
	             : getsockname(pSock->sock,(struct sockaddr *)&sAddr,&nAddr);
	if( iRet != 0 ){
		SockFailLast(pCtx,pSock,bPeer ? "unable to retrieve peer name"
		                              : "unable to retrieve socket name");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SockReportAddr(pCtx,(const struct sockaddr *)&sAddr,apArg[1],nArg > 2 ? apArg[2] : 0);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_socket_getsockname(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SockName(pCtx,nArg,apArg,0);
}
static int vm_builtin_socket_getpeername(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SockName(pCtx,nArg,apArg,1);
}
/* bool socket_set_nonblock(Socket) / socket_set_block(Socket) */
static int SockSetBlocking(ph7_context *pCtx,ph7_value **apArg,int bBlocking)
{
	phl_socket *pSock;
	int rc;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
#ifdef __WINNT__
	{
		u_long iMode = bBlocking ? 0 : 1;
		if( ioctlsocket(pSock->sock,FIONBIO,&iMode) != 0 ){
			SockFailLast(pCtx,pSock,bBlocking ? "unable to set blocking mode"
			                                  : "unable to set nonblocking mode");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
#else
	{
		int iFlags = fcntl(pSock->sock,F_GETFL);
		if( iFlags < 0 ){
			SockFailLast(pCtx,pSock,bBlocking ? "unable to set blocking mode"
			                                  : "unable to set nonblocking mode");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		if( bBlocking ){
			iFlags &= ~O_NONBLOCK;
		}else{
			iFlags |= O_NONBLOCK;
		}
		if( fcntl(pSock->sock,F_SETFL,iFlags) < 0 ){
			SockFailLast(pCtx,pSock,bBlocking ? "unable to set blocking mode"
			                                  : "unable to set nonblocking mode");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
#endif
	pSock->bBlocking = bBlocking;
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_socket_set_nonblock(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	return SockSetBlocking(pCtx,apArg,0);
}
static int vm_builtin_socket_set_block(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	return SockSetBlocking(pCtx,apArg,1);
}
/* bool socket_shutdown(Socket $socket, int $mode = 2) */
static int vm_builtin_socket_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_socket *pSock;
	ph7_int64 iMode = 2;
	int rc;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	if( nArg > 1 ){
		iMode = ph7_value_to_int64(apArg[1]);
	}
	if( iMode < 0 || iMode > 2 ){
		/* php's own wording, with no serial comma before the last name. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #2 ($mode) must be one of SHUT_RD, SHUT_WR or SHUT_RDWR",
			&pCtx->pFunc->sName);
	}
	if( shutdown(pSock->sock,(int)iMode) != 0 ){
		SockFailLast(pCtx,pSock,"Unable to shutdown socket");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
#ifndef __WINNT__
/*
 * bool socket_atmark(Socket $socket)
 *
 * The one function of this extension php does NOT build for Windows -- its
 * `function_exists('socket_atmark')` is false there -- so this one is absent
 * with it rather than emulated over ioctlsocket(SIOCATMARK).
 */
static int vm_builtin_socket_atmark(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_socket *pSock;
	int rc,iMark;
	SXUNUSED(nArg);
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	iMark = sockatmark(pSock->sock);
	if( iMark < 0 ){
		SockFailLast(pCtx,pSock,"Unable to apply sockmark");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,iMark != 0);
	return PH7_OK;
}
#endif /* !__WINNT__ */

/* ------------------------------------------------------------------------
 * Reading and writing
 * ------------------------------------------------------------------------ */
/*
 * php's line reader (PHP_NORMAL_READ). It takes ONE BYTE at a time and stops
 * after the byte it just stored is `\n` or `\r`, so the terminator is part of
 * the answer -- a `fgets()` with no buffering under it. A peer that has closed
 * makes recv() answer 0 for ever, and php spins 200 times before calling that
 * ECONNRESET; a NON-blocking socket with nothing to read gives up after two
 * empty passes and answers what it has, which for the first pass is "".
 */
static int SockReadLine(phl_socket *pSock,char *zBuf,int nMax,int *pErr)
{
	int n = 0,nEmpty = 0,bBlocking = pSock->bBlocking;
#if !defined(__WINNT__)
	/* php asks the DESCRIPTOR rather than its own flag, so a socket some other
	 * door made non-blocking (an exported stream's stream_set_blocking()) is
	 * seen as such here too. Winsock cannot be asked, and php keeps the flag
	 * there for exactly that reason. */
	{
		int iFlags = fcntl(pSock->sock,F_GETFL);
		if( iFlags >= 0 ){
			bBlocking = (iFlags & O_NONBLOCK) == 0;
		}
	}
#endif
	*pErr = 0;
	while( n < nMax ){
		int iRead = (int)recv(pSock->sock,zBuf + n,1,0);
		if( iRead == 1 ){
			char c = zBuf[n];
			n++;
			if( c == '\n' || c == '\r' ){
				break;
			}
			nEmpty = 0;
			continue;
		}
		if( iRead == 0 ){
			nEmpty++;
			if( !bBlocking && nEmpty >= 2 ){
				break;
			}
			if( nEmpty > 200 ){
#ifdef __WINNT__
				*pErr = WSAECONNRESET;
#else
				*pErr = ECONNRESET;
#endif
				return -1;
			}
			continue;
		}
		if( PH7_NetWouldBlock() ){
			/* Where php SPINS: its counter only advances on a 0-length read and
			 * a would-block answers -1, so its loop never leaves. Answering
			 * what has been read is what that dead guard was written to do --
			 * a recorded, deliberate divergence (PLAN.md 2.1). */
			break;
		}
		*pErr = PH7_NetLastError();
		return -1;
	}
	return n;
}
/* string|false socket_read(Socket $socket, int $length, int $mode = PHP_BINARY_READ) */
static int vm_builtin_socket_read(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_socket *pSock;
	ph7_int64 iLen;
	char *zBuf;
	int rc,iRead,iMode = 2;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	iLen = ph7_value_to_int64(apArg[1]);
	if( nArg > 2 ){
		iMode = (int)ph7_value_to_int64(apArg[2]);
	}
	/* php's own overflow screen, which is also what refuses 0 and -1. */
	if( iLen + 1 < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( iLen > PHL_SOCK_MAXBUF ){
		return PH7_ContextMemoryError(pCtx);
	}
	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(iLen + 1),TRUE,FALSE);
	if( zBuf == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( iMode == 1 ){
		int iErr = 0;
		iRead = SockReadLine(pSock,zBuf,(int)iLen,&iErr);
		if( iRead < 0 ){
			/* Lower case, unlike socket_recv()'s: php words the two apart. */
			SockFail(pCtx,pSock,"unable to read from socket",iErr);
		}
	}else{
		iRead = (int)recv(pSock->sock,zBuf,(phl_sock_size)iLen,0);
		if( iRead < 0 ){
			SockFailLast(pCtx,pSock,"unable to read from socket");
		}
	}
	if( iRead < 0 ){
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_string(pCtx,zBuf,iRead);
	}
	ph7_context_free_chunk(pCtx,zBuf);
	return PH7_OK;
}
/* int|false socket_write(Socket $socket, string $data, ?int $length = null) */
static int vm_builtin_socket_write(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_socket *pSock;
	const char *zData;
	int nData = 0,rc,iSent;
	ph7_int64 iLen;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	zData = ph7_value_to_string(apArg[1],&nData);
	iLen = nData;
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		iLen = ph7_value_to_int64(apArg[2]);
		if( iLen < 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%z(): Argument #3 ($length) must be greater than or equal to 0",
				&pCtx->pFunc->sName);
		}
		if( iLen > nData ){
			iLen = nData;
		}
	}
	iSent = (int)send(pSock->sock,zData,(phl_sock_size)iLen,0);
	if( iSent < 0 ){
		SockFailLast(pCtx,pSock,"unable to write to socket");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,iSent);
	return PH7_OK;
}
/* int|false socket_send(Socket $socket, string $data, int $length, int $flags) */
static int vm_builtin_socket_send(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_socket *pSock;
	const char *zData;
	int nData = 0,rc,iSent;
	ph7_int64 iLen;
	SXUNUSED(nArg);
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	zData = ph7_value_to_string(apArg[1],&nData);
	iLen = ph7_value_to_int64(apArg[2]);
	if( iLen < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #3 ($length) must be greater than or equal to 0",
			&pCtx->pFunc->sName);
	}
	if( iLen > nData ){
		iLen = nData;
	}
	iSent = (int)send(pSock->sock,zData,(phl_sock_size)iLen,(int)ph7_value_to_int64(apArg[3]));
	if( iSent < 0 ){
		SockFailLast(pCtx,pSock,"Unable to write to socket");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,iSent);
	return PH7_OK;
}
/* int|false socket_recv(Socket $socket, &$data, int $length, int $flags) */
static int vm_builtin_socket_recv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_socket *pSock;
	ph7_value *pOut;
	ph7_int64 iLen;
	char *zBuf;
	int rc,iRead;
	SXUNUSED(nArg);
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	iLen = ph7_value_to_int64(apArg[2]);
	if( iLen + 1 < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( iLen > PHL_SOCK_MAXBUF ){
		return PH7_ContextMemoryError(pCtx);
	}
	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(iLen + 1),TRUE,FALSE);
	pOut = ph7_context_new_scalar(pCtx);
	if( zBuf == 0 || pOut == 0 ){
		if( zBuf ){
			ph7_context_free_chunk(pCtx,zBuf);
		}
		return PH7_ContextMemoryError(pCtx);
	}
	iRead = (int)recv(pSock->sock,zBuf,(phl_sock_size)iLen,(int)ph7_value_to_int64(apArg[3]));
	if( iRead < 0 ){
		SockFailLast(pCtx,pSock,"Unable to read from socket");
	}
	/* php answers NULL rather than "" for both the failure and the orderly
	 * end of stream; only a non-empty read reaches the buffer. */
	if( iRead > 0 ){
		ph7_value_string(pOut,zBuf,iRead);
	}else{
		ph7_value_null(pOut);
	}
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOut);
	ph7_context_free_chunk(pCtx,zBuf);
	if( iRead < 0 ){
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_int64(pCtx,iRead);
	}
	return PH7_OK;
}
/*
 * int|false socket_sendto(Socket $socket, string $data, int $length, int $flags,
 *                         string $address, ?int $port = null)
 */
static int vm_builtin_socket_sendto(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct sockaddr_storage sAddr;
	phl_socket *pSock;
	const char *zData,*zAddr;
	int nData = 0,nAddr = 0,rc,iSent,nLen,bHasPort;
	ph7_int64 iLen;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	zData = ph7_value_to_string(apArg[1],&nData);
	iLen = ph7_value_to_int64(apArg[2]);
	if( iLen < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #3 ($length) must be greater than or equal to 0",
			&pCtx->pFunc->sName);
	}
	if( iLen > nData ){
		iLen = nData;
	}
	zAddr = ph7_value_to_string(apArg[4],&nAddr);
	bHasPort = (nArg > 5 && !ph7_value_is_null(apArg[5]));
	nLen = SockBuildAddr(pCtx,pSock,zAddr,nAddr,bHasPort,
		bHasPort ? ph7_value_to_int64(apArg[5]) : 0,6,"port",&sAddr,&rc);
	if( nLen <= 0 ){
		if( nLen < 0 ){
			return rc;
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iSent = (int)sendto(pSock->sock,zData,(phl_sock_size)iLen,(int)ph7_value_to_int64(apArg[3]),
		(struct sockaddr *)&sAddr,(ph7_socklen)nLen);
	if( iSent < 0 ){
		SockFailLast(pCtx,pSock,"Unable to write to socket");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,iSent);
	return PH7_OK;
}
/*
 * int|false socket_recvfrom(Socket $socket, &$data, int $length, int $flags,
 *                          &$address, &$port = null)
 */
static int vm_builtin_socket_recvfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct sockaddr_storage sAddr;
	ph7_socklen nAddr = (ph7_socklen)sizeof(sAddr);
	phl_socket *pSock;
	ph7_value *pOut;
	ph7_int64 iLen;
	char *zBuf;
	int rc,iRead;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	iLen = ph7_value_to_int64(apArg[2]);
	if( iLen + 1 < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( iLen > PHL_SOCK_MAXBUF ){
		return PH7_ContextMemoryError(pCtx);
	}
	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(iLen + 1),TRUE,FALSE);
	pOut = ph7_context_new_scalar(pCtx);
	if( zBuf == 0 || pOut == 0 ){
		if( zBuf ){
			ph7_context_free_chunk(pCtx,zBuf);
		}
		return PH7_ContextMemoryError(pCtx);
	}
	SyZero(&sAddr,sizeof(sAddr));
	iRead = (int)recvfrom(pSock->sock,zBuf,(phl_sock_size)iLen,(int)ph7_value_to_int64(apArg[3]),
		(struct sockaddr *)&sAddr,&nAddr);
	if( iRead < 0 ){
		SockFailLast(pCtx,pSock,"Unable to recvfrom");
		ph7_context_free_chunk(pCtx,zBuf);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_value_string(pOut,zBuf,iRead);
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOut);
	ph7_context_free_chunk(pCtx,zBuf);
	/* An AF_UNIX datagram from an unbound sender carries no path at all, which
	 * is the empty string php reports rather than a missing argument. */
	if( sAddr.ss_family == 0 ){
		sAddr.ss_family = (unsigned short)pSock->iDomain;
	}
	SockReportAddr(pCtx,(const struct sockaddr *)&sAddr,apArg[4],nArg > 5 ? apArg[5] : 0);
	ph7_result_int64(pCtx,iRead);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Socket options
 * ------------------------------------------------------------------------ */
/*
 * The missing-key refusal, in php's TWO wordings: the option php reads with its
 * own struct converter names the ARGUMENT, and the multicast group reader --
 * which is shared with nothing -- names only the key.
 */
static ph7_value * SockOptKey(ph7_context *pCtx,ph7_value *pArr,const char *zKey,int *pRc)
{
	ph7_value *pVal = ph7_array_fetch(pArr,zKey,(int)SyStrlen(zKey));
	if( pVal == 0 ){
		*pRc = PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #4 ($value) must have key \"%s\"",
			&pCtx->pFunc->sName,zKey);
		return 0;
	}
	*pRc = PH7_OK;
	return pVal;
}
static ph7_value * SockOptvalKey(ph7_context *pCtx,ph7_value *pArr,const char *zKey,int *pRc)
{
	ph7_value *pVal = ph7_array_fetch(pArr,zKey,(int)SyStrlen(zKey));
	if( pVal == 0 ){
		*pRc = PH7_VmThrowException(pCtx,"ValueError",
			"No key \"%s\" passed in optval",zKey);
		return 0;
	}
	*pRc = PH7_OK;
	return pVal;
}
#if defined(IP_MULTICAST_IF) && !defined(__WINNT__)
/*
 * php's interface naming, both ways. A `$value` that is a NUMBER is already an
 * index; a string is an interface NAME. IPv4's IP_MULTICAST_IF is stated as an
 * ADDRESS on the wire rather than an index, so the two have to be translated
 * into each other -- index 0 is INADDR_ANY, "the default route", and anything
 * else is that interface's own address.
 */
/*
 * php's diagnostic for an interface NAME nothing answers to. A numeric index is
 * never refused here -- it goes to the kernel, which reports ENODEV through the
 * ordinary "Unable to set socket option".
 */
static void SockNoSuchInterface(ph7_context *pCtx,ph7_value *pVal)
{
	int n = 0;
	const char *z = ph7_value_to_string(pVal,&n);
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
		"No interface with name \"%.*s\" could be found",n,z ? z : "");
}
static int SockIfIndexOf(ph7_context *pCtx,ph7_value *pVal,unsigned int *pOut)
{
	if( ph7_value_is_int(pVal) ){
		ph7_int64 i = ph7_value_to_int64(pVal);
		if( i < 0 || i > 0xFFFFFFFF ){
			return -1;
		}
		*pOut = (unsigned int)i;
		return 0;
	}
	{
		int n = 0;
		const char *z = ph7_value_to_string(pVal,&n);
		char zName[IF_NAMESIZE+1];
		if( n < 1 || n >= (int)sizeof(zName) ){
			return -1;
		}
		SyMemcpy(z,zName,(sxu32)n);
		zName[n] = 0;
		*pOut = if_nametoindex(zName);
		if( *pOut == 0 ){
			return -1;
		}
	}
	SXUNUSED(pCtx);
	return 0;
}
static int SockIfIndexToAddr4(unsigned int iIndex,struct in_addr *pOut)
{
	struct ifreq sReq;
	int fd;
	if( iIndex == 0 ){
		pOut->s_addr = htonl(INADDR_ANY);
		return 0;
	}
	SyZero(&sReq,sizeof(sReq));
	if( if_indextoname(iIndex,sReq.ifr_name) == 0 ){
		return -1;
	}
	fd = socket(AF_INET,SOCK_DGRAM,0);
	if( fd < 0 ){
		return -1;
	}
	if( ioctl(fd,SIOCGIFADDR,&sReq) != 0 ){
		close(fd);
		return -1;
	}
	close(fd);
	SyMemcpy((const void *)&((struct sockaddr_in *)&sReq.ifr_addr)->sin_addr,
		(void *)pOut,sizeof(struct in_addr));
	return 0;
}
static int SockAddr4ToIfIndex(const struct in_addr *pAddr,unsigned int *pOut)
{
	struct ifaddrs *pList = 0,*p;
	if( pAddr->s_addr == htonl(INADDR_ANY) ){
		*pOut = 0;
		return 0;
	}
	if( getifaddrs(&pList) != 0 ){
		return -1;
	}
	for( p = pList ; p ; p = p->ifa_next ){
		if( p->ifa_addr && p->ifa_addr->sa_family == AF_INET
		 && ((struct sockaddr_in *)p->ifa_addr)->sin_addr.s_addr == pAddr->s_addr ){
			*pOut = if_nametoindex(p->ifa_name);
			freeifaddrs(pList);
			return *pOut ? 0 : -1;
		}
	}
	freeifaddrs(pList);
	return -1;
}
/*
 * The group-membership options. php takes `['group' => …, 'interface' => …]`
 * (plus `'source'` for the four source-filtered ones) and hands the kernel a
 * `group_req`/`group_source_req`. The ADDRESS is parsed the way every other
 * address in this extension is -- by the SOCKET's family, name resolution and
 * all -- which is why a group that is not an address reports the -10000 host
 * lookup failure rather than a plain refusal.
 */
static int SockOptGroupAddr(ph7_context *pCtx,phl_socket *pSock,ph7_value *pArr,
	const char *zKey,struct sockaddr_storage *pOut,int *pRc)
{
	ph7_value *pVal = SockOptvalKey(pCtx,pArr,zKey,pRc);
	const char *z;
	int n = 0,nLen;
	if( pVal == 0 ){
		return -1;
	}
	z = ph7_value_to_string(pVal,&n);
	nLen = SockBuildAddr(pCtx,pSock,z,n,1,0,4,"value",pOut,pRc);
	if( nLen <= 0 ){
		if( nLen == 0 ){
			*pRc = PH7_OK;
		}
		return -1;
	}
	return 0;
}
static int SockSetMcastGroup(ph7_context *pCtx,phl_socket *pSock,int iLevel,int iOpt,
	ph7_value *pVal,int bSource,int *pRc)
{
	struct group_source_req sReq;
	ph7_value *pTmp;
	unsigned int iIf = 0;
	SyZero(&sReq,sizeof(sReq));
	if( SockOptGroupAddr(pCtx,pSock,pVal,"group",
		(struct sockaddr_storage *)&sReq.gsr_group,pRc) != 0 ){
		return -1;
	}
	pTmp = ph7_array_fetch(pVal,"interface",sizeof("interface")-1);
	if( pTmp && SockIfIndexOf(pCtx,pTmp,&iIf) != 0 ){
		*pRc = PH7_OK;
		SockNoSuchInterface(pCtx,pTmp);
		return -1;
	}
	sReq.gsr_interface = iIf;
	if( bSource ){
		if( SockOptGroupAddr(pCtx,pSock,pVal,"source",
			(struct sockaddr_storage *)&sReq.gsr_source,pRc) != 0 ){
			return -1;
		}
	}
	*pRc = PH7_OK;
	if( setsockopt(pSock->sock,iLevel,iOpt,(const char *)&sReq,
		bSource ? (ph7_socklen)sizeof(struct group_source_req)
		        : (ph7_socklen)sizeof(struct group_req)) != 0 ){
		SockFailLast(pCtx,pSock,"Unable to set socket option");
		return -1;
	}
	return 0;
}
#endif /* IP_MULTICAST_IF && !__WINNT__ */

/* bool socket_set_option(Socket $socket, int $level, int $option, $value) */
static int vm_builtin_socket_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_socket *pSock;
	ph7_value *pVal;
	ph7_int64 iLevel,iOpt;
	int rc,iRet;
	SXUNUSED(nArg);
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	iLevel = ph7_value_to_int64(apArg[1]);
	iOpt   = ph7_value_to_int64(apArg[2]);
	pVal   = apArg[3];
	{
		/*
		 * Every struct-valued option is screened for an ARRAY before anything
		 * else -- and php names the option with the LAST label of the switch
		 * arm it shares, so `MCAST_JOIN_GROUP` reports itself as
		 * `MCAST_LEAVE_GROUP` and the four source-filtered ones all report
		 * `MCAST_LEAVE_SOURCE_GROUP`. That is php's own text, reproduced.
		 */
		const char *zOptName = 0;
		if( iLevel == SOL_SOCKET ){
			if( (int)iOpt == SO_LINGER ){ zOptName = "SO_LINGER"; }
			else if( (int)iOpt == SO_RCVTIMEO ){ zOptName = "SO_RCVTIMEO"; }
			else if( (int)iOpt == SO_SNDTIMEO ){ zOptName = "SO_SNDTIMEO"; }
		}
#if defined(MCAST_JOIN_GROUP) && !defined(__WINNT__)
		else if( iLevel == IPPROTO_IP || iLevel == IPPROTO_IPV6 ){
			switch( (int)iOpt ){
				case MCAST_JOIN_GROUP: case MCAST_LEAVE_GROUP:
					zOptName = "MCAST_LEAVE_GROUP"; break;
				case MCAST_BLOCK_SOURCE: case MCAST_UNBLOCK_SOURCE:
				case MCAST_JOIN_SOURCE_GROUP: case MCAST_LEAVE_SOURCE_GROUP:
					zOptName = "MCAST_LEAVE_SOURCE_GROUP"; break;
				default: break;
			}
		}
#endif
		if( zOptName && !ph7_value_is_array(pVal) ){
			char zGiven[64];
			return PH7_VmThrowException(pCtx,"TypeError",
				"%z(): Argument #4 ($value) must be of type array when "
				"argument #3 ($option) is %s, %s given",
				&pCtx->pFunc->sName,zOptName,
				VmValueGivenName(pVal,zGiven,sizeof(zGiven)));
		}
	}
#if defined(IP_MULTICAST_IF) && !defined(__WINNT__)
	if( iLevel == IPPROTO_IP || iLevel == IPPROTO_IPV6 ){
		int bHandled = 1,bSource = 0,bGroup = 0;
		switch( (int)iOpt ){
			case MCAST_JOIN_GROUP: case MCAST_LEAVE_GROUP:
				bGroup = 1; break;
			case MCAST_BLOCK_SOURCE: case MCAST_UNBLOCK_SOURCE:
			case MCAST_JOIN_SOURCE_GROUP: case MCAST_LEAVE_SOURCE_GROUP:
				bGroup = bSource = 1; break;
			default: bHandled = 0; break;
		}
		if( bGroup ){
			if( SockSetMcastGroup(pCtx,pSock,(int)iLevel,(int)iOpt,pVal,bSource,&rc) != 0 ){
				if( rc != PH7_OK ){
					return rc;
				}
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( iLevel == IPPROTO_IP && (int)iOpt == IP_MULTICAST_IF ){
			struct in_addr sIf;
			unsigned int iIdx = 0;
			if( SockIfIndexOf(pCtx,pVal,&iIdx) != 0
			 || SockIfIndexToAddr4(iIdx,&sIf) != 0 ){
				SockNoSuchInterface(pCtx,pVal);
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,
				(const char *)&sIf,(ph7_socklen)sizeof(sIf));
			goto done;
		}
		if( iLevel == IPPROTO_IPV6 && (int)iOpt == IPV6_MULTICAST_IF ){
			unsigned int iIdx = 0;
			if( SockIfIndexOf(pCtx,pVal,&iIdx) != 0 ){
				SockNoSuchInterface(pCtx,pVal);
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,
				(const char *)&iIdx,(ph7_socklen)sizeof(iIdx));
			goto done;
		}
		if( iLevel == IPPROTO_IP
		 && ((int)iOpt == IP_MULTICAST_LOOP || (int)iOpt == IP_MULTICAST_TTL) ){
			/* IPv4 states both of these in ONE BYTE; the v6 pair below are ints. */
			unsigned char c = (unsigned char)ph7_value_to_int64(pVal);
			iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,
				(const char *)&c,(ph7_socklen)sizeof(c));
			goto done;
		}
		SXUNUSED(bHandled);
	}
#endif
	if( iLevel == SOL_SOCKET && (int)iOpt == SO_LINGER ){
		struct linger sLin;
		ph7_value *pOn,*pFor;
		SyZero(&sLin,sizeof(sLin));
		pOn = SockOptKey(pCtx,pVal,"l_onoff",&rc);
		if( pOn == 0 ){
			return rc;
		}
		pFor = SockOptKey(pCtx,pVal,"l_linger",&rc);
		if( pFor == 0 ){
			return rc;
		}
#ifdef __WINNT__
		/* Winsock states both halves in sixteen bits; POSIX states them in an int. */
		sLin.l_onoff = (u_short)ph7_value_to_int64(pOn);
		sLin.l_linger = (u_short)ph7_value_to_int64(pFor);
#else
		sLin.l_onoff = (int)ph7_value_to_int64(pOn);
		sLin.l_linger = (int)ph7_value_to_int64(pFor);
#endif
		iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,
			(const char *)&sLin,(ph7_socklen)sizeof(sLin));
		goto done;
	}
	if( iLevel == SOL_SOCKET && ((int)iOpt == SO_RCVTIMEO || (int)iOpt == SO_SNDTIMEO) ){
		struct timeval tv;
		ph7_value *pSec,*pUsec;
		pSec = SockOptKey(pCtx,pVal,"sec",&rc);
		if( pSec == 0 ){
			return rc;
		}
		pUsec = SockOptKey(pCtx,pVal,"usec",&rc);
		if( pUsec == 0 ){
			return rc;
		}
		tv.tv_sec = (long)ph7_value_to_int64(pSec);
		tv.tv_usec = (long)ph7_value_to_int64(pUsec);
		iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,
			(const char *)&tv,(ph7_socklen)sizeof(tv));
		goto done;
	}
	if( (iLevel == SOL_SOCKET && (int)iOpt == PHL_SO_BINDTODEVICE)
	 || (iLevel == IPPROTO_TCP && (int)iOpt == PHL_TCP_CONGESTION) ){
		/* The two options whose value is a STRING rather than a number. */
		int n = 0;
		const char *z = ph7_value_to_string(pVal,&n);
		iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,z,(ph7_socklen)n);
		goto done;
	}
	{
		int iNum = (int)ph7_value_to_int64(pVal);
		iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,
			(const char *)&iNum,(ph7_socklen)sizeof(iNum));
	}
done:
	if( iRet != 0 ){
		SockFailLast(pCtx,pSock,"Unable to set socket option");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* array|int|false socket_get_option(Socket $socket, int $level, int $option) */
static int vm_builtin_socket_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_socket *pSock;
	ph7_int64 iLevel,iOpt;
	ph7_socklen nOpt;
	int rc;
	SXUNUSED(nArg);
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	iLevel = ph7_value_to_int64(apArg[1]);
	iOpt   = ph7_value_to_int64(apArg[2]);
#if defined(IP_MULTICAST_IF) && !defined(__WINNT__)
	if( iLevel == IPPROTO_IP && (int)iOpt == IP_MULTICAST_IF ){
		struct in_addr sIf;
		unsigned int iIdx = 0;
		nOpt = (ph7_socklen)sizeof(sIf);
		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,(char *)&sIf,&nOpt) != 0 ){
			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		if( SockAddr4ToIfIndex(&sIf,&iIdx) != 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ph7_result_int64(pCtx,(ph7_int64)iIdx);
		return PH7_OK;
	}
	if( iLevel == IPPROTO_IP
	 && ((int)iOpt == IP_MULTICAST_LOOP || (int)iOpt == IP_MULTICAST_TTL) ){
		unsigned char c = 0;
		nOpt = (ph7_socklen)sizeof(c);
		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,(char *)&c,&nOpt) != 0 ){
			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ph7_result_int64(pCtx,(ph7_int64)c);
		return PH7_OK;
	}
#endif
	if( iLevel == SOL_SOCKET && (int)iOpt == SO_LINGER ){
		struct linger sLin;
		ph7_value *pArr,*pTmp;
		SyZero(&sLin,sizeof(sLin));
		nOpt = (ph7_socklen)sizeof(sLin);
		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,(char *)&sLin,&nOpt) != 0 ){
			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pArr = ph7_context_new_array(pCtx);
		pTmp = ph7_context_new_scalar(pCtx);
		if( pArr == 0 || pTmp == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		ph7_value_int64(pTmp,sLin.l_onoff);
		ph7_array_add_strkey_elem(pArr,"l_onoff",pTmp);
		ph7_value_int64(pTmp,sLin.l_linger);
		ph7_array_add_strkey_elem(pArr,"l_linger",pTmp);
		ph7_result_value(pCtx,pArr);
		return PH7_OK;
	}
	if( iLevel == SOL_SOCKET && ((int)iOpt == SO_RCVTIMEO || (int)iOpt == SO_SNDTIMEO) ){
		struct timeval tv;
		ph7_value *pArr,*pTmp;
		SyZero(&tv,sizeof(tv));
		nOpt = (ph7_socklen)sizeof(tv);
		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,(char *)&tv,&nOpt) != 0 ){
			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pArr = ph7_context_new_array(pCtx);
		pTmp = ph7_context_new_scalar(pCtx);
		if( pArr == 0 || pTmp == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		ph7_value_int64(pTmp,(ph7_int64)tv.tv_sec);
		ph7_array_add_strkey_elem(pArr,"sec",pTmp);
		ph7_value_int64(pTmp,(ph7_int64)tv.tv_usec);
		ph7_array_add_strkey_elem(pArr,"usec",pTmp);
		ph7_result_value(pCtx,pArr);
		return PH7_OK;
	}
	if( iLevel == IPPROTO_TCP && (int)iOpt == PHL_TCP_CONGESTION ){
		/* The one option php reports as a NAMED string: `['name' => 'cubic']`. */
		char zName[64];
		ph7_value *pArr,*pTmp;
		nOpt = (ph7_socklen)sizeof(zName);
		SyZero(zName,sizeof(zName));
		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,zName,&nOpt) != 0 ){
			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pArr = ph7_context_new_array(pCtx);
		pTmp = ph7_context_new_scalar(pCtx);
		if( pArr == 0 || pTmp == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		ph7_value_string(pTmp,zName,(int)SyStrlen(zName));
		ph7_array_add_strkey_elem(pArr,"name",pTmp);
		ph7_result_value(pCtx,pArr);
		return PH7_OK;
	}
	{
		int iNum = 0;
		nOpt = (ph7_socklen)sizeof(iNum);
		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,(char *)&iNum,&nOpt) != 0 ){
			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ph7_result_int64(pCtx,iNum);
	}
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * socket_select()
 * ------------------------------------------------------------------------ */
typedef struct SockSelectCtx SockSelectCtx;
struct SockSelectCtx
{
	ph7_context *pCtx;
	ph7_class *pClass;  /* the mounted `Socket`, compared by POINTER: php's own
	                     * screen is `Z_OBJCE_P(element) != socket_ce`, and a
	                     * name compare would have to reach into a SyString that
	                     * is not NUL-terminated */
	fd_set *pSet;
	ph7_value *pOut;    /* the rebuilt array, on the FILTER pass */
	int iArgPos;        /* which of the three arrays, for the type error */
	const char *zArgName;
	int iMax;
	int nSeen;
	int rc;             /* PH7_OK, or the throw a bad element raised */
	int bBad;
};
/*
 * Pass one: every element must BE a Socket, and one that was closed is php's
 * own Error rather than a skipped entry.
 */
static int SockSelectAdd(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	SockSelectCtx *p = (SockSelectCtx *)pUserData;
	ph7_class_instance *pThis;
	phl_socket *pSock;
	SXUNUSED(pKey);
	pThis = ph7_value_is_object(pVal) ? (ph7_class_instance *)pVal->x.pOther : 0;
	if( pThis == 0 || pThis->pClass != p->pClass ){
		char zGiven[64];
		p->bBad = 1;
		p->rc = PH7_VmThrowException(p->pCtx,"TypeError",
			"%z(): Argument #%d ($%s) must only have elements of type Socket, %s given",
			&p->pCtx->pFunc->sName,p->iArgPos,p->zArgName,
			VmValueGivenName(pVal,zGiven,sizeof(zGiven)));
		return PH7_ABORT;
	}
	pSock = (phl_socket *)SockSlotOf(pThis);
	if( pSock == 0 || pSock->sock == PH7_NET_INVALID_SOCKET ){
		/* A TypeError rather than the Error every other verb raises: php screens
		 * the ARRAY here, so a closed element is a bad argument value. */
		p->bBad = 1;
		p->rc = PH7_VmThrowException(p->pCtx,"TypeError",
			"%z(): Argument #%d ($%s) contains a closed socket",
			&p->pCtx->pFunc->sName,p->iArgPos,p->zArgName);
		return PH7_ABORT;
	}
	FD_SET(pSock->sock,p->pSet);
	if( (int)pSock->sock > p->iMax ){
		p->iMax = (int)pSock->sock;
	}
	p->nSeen++;
	return PH7_OK;
}
/* Pass two: rebuild the array out of the entries select() marked, KEYS AND ALL. */
static int SockSelectKeep(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	SockSelectCtx *p = (SockSelectCtx *)pUserData;
	ph7_class_instance *pThis = ph7_value_is_object(pVal) ?
		(ph7_class_instance *)pVal->x.pOther : 0;
	phl_socket *pSock = (phl_socket *)SockSlotOf(pThis);
	if( pSock && pSock->sock != PH7_NET_INVALID_SOCKET
	 && FD_ISSET(pSock->sock,p->pSet) ){
		ph7_array_add_elem(p->pOut,pKey,pVal);
	}
	return PH7_OK;
}
/*
 * int|false socket_select(?array &$read, ?array &$write, ?array &$except,
 *                         ?int $seconds, int $microseconds = 0)
 */
static int vm_builtin_socket_select(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	static const char * const azName[3] = { "read","write","except" };
	fd_set aSet[3];
	SockSelectCtx sWalk;
	ph7_value *apOut[3];
	ph7_class *pClass;
	struct timeval tv,*pTv = 0;
	int i,iMax = -1,nSets = 0,iRet;
	SXUNUSED(nArg);
	pClass = PH7_VmExtractClass(pCtx->pVm,"Socket",sizeof("Socket")-1,0,0);
	for( i = 0 ; i < 3 ; ++i ){
		FD_ZERO(&aSet[i]);
		apOut[i] = 0;
	}
	for( i = 0 ; i < 3 ; ++i ){
		if( !ph7_value_is_array(apArg[i]) ){
			continue;
		}
		SyZero(&sWalk,sizeof(sWalk));
		sWalk.pCtx = pCtx;
		sWalk.pClass = pClass;
		sWalk.pSet = &aSet[i];
		sWalk.iArgPos = i + 1;
		sWalk.zArgName = azName[i];
		sWalk.iMax = iMax;
		sWalk.rc = PH7_OK;
		ph7_array_walk(apArg[i],SockSelectAdd,(void *)&sWalk);
		if( sWalk.bBad ){
			return sWalk.rc;
		}
		iMax = sWalk.iMax;
		if( sWalk.nSeen > 0 ){
			nSets++;
		}
	}
	if( nSets < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): At least one array argument must be passed",&pCtx->pFunc->sName);
	}
	if( !ph7_value_is_null(apArg[3]) ){
		ph7_int64 iSec = ph7_value_to_int64(apArg[3]);
		ph7_int64 iUsec = nArg > 4 ? ph7_value_to_int64(apArg[4]) : 0;
		tv.tv_sec = (long)(iSec + iUsec / 1000000);
		tv.tv_usec = (long)(iUsec % 1000000);
		pTv = &tv;
	}
	iRet = select(iMax + 1,&aSet[0],&aSet[1],&aSet[2],pTv);
	if( iRet < 0 ){
		SockFailLast(pCtx,0,"Unable to select");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	for( i = 0 ; i < 3 ; ++i ){
		if( !ph7_value_is_array(apArg[i]) ){
			continue;
		}
		apOut[i] = ph7_context_new_array(pCtx);
		if( apOut[i] == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		SyZero(&sWalk,sizeof(sWalk));
		sWalk.pCtx = pCtx;
		sWalk.pSet = &aSet[i];
		sWalk.pOut = apOut[i];
		ph7_array_walk(apArg[i],SockSelectKeep,(void *)&sWalk);
	}
	/* The stores happen only after every array has been rebuilt: writing one
	 * back can move the memobj pool, and the walk above holds pointers into it. */
	for( i = 0 ; i < 3 ; ++i ){
		if( apOut[i] ){
			PH7_VmStoreArgByRef(pCtx->pVm,apArg[i],apOut[i]);
		}
	}
	ph7_result_int64(pCtx,iRet);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * The error verbs
 * ------------------------------------------------------------------------ */
/*
 * int socket_last_error(?Socket $socket = null)
 *
 * With a socket it is that socket's own code; with none it is the per-request
 * one. A CLOSED socket is refused here too -- php runs the same screen on both
 * of these as on a verb that would touch the descriptor.
 */
static int vm_builtin_socket_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		int rc;
		phl_socket *pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
		if( pSock == 0 ){
			return rc;
		}
		ph7_result_int64(pCtx,pSock->iError);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,pCtx->pVm->iSocketLastErr);
	return PH7_OK;
}
/* void socket_clear_error(?Socket $socket = null) */
static int vm_builtin_socket_clear_error(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		int rc;
		phl_socket *pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
		if( pSock == 0 ){
			return rc;
		}
		pSock->iError = 0;
		return PH7_OK;
	}
	pCtx->pVm->iSocketLastErr = 0;
	return PH7_OK;
}
/* string socket_strerror(int $error_code) */
static int vm_builtin_socket_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *z;
	SXUNUSED(nArg);
	z = PH7_SocketStrError((int)ph7_value_to_int64(apArg[0]));
	ph7_result_string(pCtx,z ? z : "",-1);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * The two doors onto the stream layer
 * ------------------------------------------------------------------------ */
/*
 * resource|false socket_export_stream(Socket $socket)
 *
 * php builds the stream ONCE and hands the same handle back for ever after, and
 * the two then share a descriptor: closing either closes both. The URI and the
 * `stream_type` label are picked from the DOMAIN as well as the type, which is
 * why an AF_UNIX datagram socket reports `udg_socket`.
 */
static int vm_builtin_socket_export_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_socket *pSock;
	io_private *pDev;
	const char *zUri,*zLabel;
	int rc,bDgram;
	SXUNUSED(nArg);
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	if( pSock->pStream ){
		ph7_result_resource(pCtx,pSock->pStream);
		return PH7_OK;
	}
	bDgram = (pSock->iType != SOCK_STREAM);
	if( pSock->iDomain == AF_UNIX ){
		zUri   = bDgram ? "udg://" : "unix://";
		zLabel = bDgram ? "udg_socket" : "unix_socket";
	}else{
		zUri   = bDgram ? "udp://" : "tcp://";
		zLabel = bDgram ? "udp_socket" : "tcp_socket/ssl";
	}
	pDev = PH7_StreamWrapSocket(pCtx,pSock->sock,bDgram,zLabel,zUri);
	if( pDev == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pSock->pStream = (void *)pDev;
	pSock->bExported = 1;
	ph7_result_resource(pCtx,pDev);
	return PH7_OK;
}
/* Socket|false socket_import_stream($stream) */
static int vm_builtin_socket_import_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct sockaddr_storage sAddr;
	ph7_socklen nAddr = (ph7_socklen)sizeof(sAddr);
	ph7_class *pClass;
	ph7_class_instance *pThis;
	io_private *pDev;
	phl_socket *pRec;
	ph7_socket sock;
	int rc,iType = SOCK_STREAM,iDomain = AF_INET;
	ph7_socklen nOpt = (ph7_socklen)sizeof(iType);
	SXUNUSED(nArg);
	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);
	if( pDev == 0 ){
		return rc;
	}
	if( !PH7_StreamSocketHandle(pDev,&sock) ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Cannot represent a stream of type %s as a Socket Descriptor",
			PH7_StreamTypeLabel(pDev));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyZero(&sAddr,sizeof(sAddr));
	if( getsockname(sock,(struct sockaddr *)&sAddr,&nAddr) == 0 && sAddr.ss_family != 0 ){
		iDomain = (int)sAddr.ss_family;
	}
	getsockopt(sock,SOL_SOCKET,SO_TYPE,(char *)&iType,&nOpt);
	pClass = PH7_VmExtractClass(pCtx->pVm,"Socket",sizeof("Socket")-1,0,0);
	pThis = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;
	pRec = pThis ? SockNew(pCtx->pVm,sock,iDomain,iType,0) : 0;
	if( pRec == 0 || SockSlotAttach(pThis,(void *)pRec) != 0 ){
		if( pThis ){
			PH7_ClassInstanceUnref(pThis);
		}
		return PH7_ContextMemoryError(pCtx);
	}
	/* The STREAM owns the descriptor: an imported socket must not close it
	 * behind the handle a script is still holding. */
	pRec->pOwner = pThis;
	pRec->pStream = (void *)pDev;
	pRec->bExported = 1;
	PH7_NativeResultObject(pCtx,pThis);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * sendmsg / recvmsg / cmsg_space
 * ------------------------------------------------------------------------ */
/*
 * php's ancillary registry: the (level, type) pairs it knows how to convert,
 * with the fixed part of the payload and the size of one repeat. The pair that
 * is not in it is refused by NUMBER -- "Pair level 1 and/or type 9999 is not
 * supported" -- which is why this is a table rather than a switch.
 */
static int SockCmsgEntry(int iLevel,int iType,unsigned int *pFixed,unsigned int *pElem)
{
	SXUNUSED(iType); SXUNUSED(pFixed); SXUNUSED(pElem); /* a platform with no pair at all */
	if( iLevel == SOL_SOCKET ){
#ifdef SCM_RIGHTS
		if( iType == SCM_RIGHTS ){
			*pFixed = 0;
			*pElem = (unsigned int)sizeof(int);
			return 0;
		}
#endif
#ifdef SCM_CREDENTIALS
		if( iType == SCM_CREDENTIALS ){
			*pFixed = (unsigned int)sizeof(struct ucred);
			*pElem = 0;
			return 0;
		}
#endif
		return -1;
	}
#if defined(IPPROTO_IPV6) || defined(__WINNT__)
	if( iLevel == IPPROTO_IPV6 ){
#ifdef IPV6_PKTINFO
		if( iType == IPV6_PKTINFO ){
			*pFixed = (unsigned int)sizeof(struct in6_pktinfo);
			*pElem = 0;
			return 0;
		}
#endif
#ifdef IPV6_HOPLIMIT
		if( iType == IPV6_HOPLIMIT ){
			*pFixed = (unsigned int)sizeof(int);
			*pElem = 0;
			return 0;
		}
#endif
#ifdef IPV6_TCLASS
		if( iType == IPV6_TCLASS ){
			*pFixed = (unsigned int)sizeof(int);
			*pElem = 0;
			return 0;
		}
#endif
	}
#endif
	return -1;
}
/* ?int socket_cmsg_space(int $level, int $type, int $num = 0) */
static int vm_builtin_socket_cmsg_space(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	unsigned int nFixed = 0,nElem = 0;
	ph7_int64 iLevel,iType,iNum = 0;
	iLevel = ph7_value_to_int64(apArg[0]);
	iType  = ph7_value_to_int64(apArg[1]);
	if( nArg > 2 ){
		iNum = ph7_value_to_int64(apArg[2]);
	}
	if( SockCmsgEntry((int)iLevel,(int)iType,&nFixed,&nElem) != 0 ){
		/* php's one refusal here carries no function prefix. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"Pair level %d and/or type %d is not supported",(int)iLevel,(int)iType);
	}
	if( iNum < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #3 ($num) must be greater than or equal to 0",
			&pCtx->pFunc->sName);
	}
	if( iNum > 0 && nElem > 0 && iNum > (ph7_int64)((0x7FFFFFFF - nFixed) / nElem) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #3 ($num) is too large",&pCtx->pFunc->sName);
	}
	/* An entry with no repeating part IGNORES $num rather than refusing it:
	 * `socket_cmsg_space(SOL_SOCKET, SCM_CREDENTIALS, 2)` is the same 32 as
	 * with no third argument at all. */
	ph7_result_int64(pCtx,(ph7_int64)PHL_CMSG_SPACE(nFixed + (size_t)iNum * nElem));
	return PH7_OK;
}

/*
 * The two message primitives, and the ONLY part of sendmsg()/recvmsg() that is
 * written twice. POSIX takes a `struct msghdr` and Winsock a `WSAMSG` whose
 * fields have different names, different types and (for the control block) a
 * different shape -- but both take exactly one address, one gathered data
 * buffer and one contiguous control block, which is what the shared half above
 * and below builds. `WSARecvMsg` is not exported by ws2_32 at all and has to be
 * fetched per socket through WSAIoctl, which is what php does too.
 */
static int SockMsgSend(ph7_socket sock,struct sockaddr *pName,ph7_socklen nName,
	const char *zData,unsigned int nData,const char *zCtl,unsigned int nCtl,int iFlags)
{
#ifdef __WINNT__
	WSAMSG sMsg;
	WSABUF sBuf;
	DWORD nSent = 0;
	SyZero(&sMsg,sizeof(sMsg));
	sBuf.len = (ULONG)nData;
	sBuf.buf = (CHAR *)zData;
	sMsg.name = pName;
	sMsg.namelen = (INT)nName;
	if( nData > 0 || zData ){
		sMsg.lpBuffers = &sBuf;
		sMsg.dwBufferCount = 1;
	}
	sMsg.Control.len = (ULONG)nCtl;
	sMsg.Control.buf = (CHAR *)zCtl;
	if( WSASendMsg(sock,&sMsg,(DWORD)iFlags,&nSent,0,0) != 0 ){
		return -1;
	}
	return (int)nSent;
#else
	struct msghdr sMsg;
	struct iovec sVec;
	SyZero(&sMsg,sizeof(sMsg));
	sVec.iov_base = (void *)zData;
	sVec.iov_len = (size_t)nData;
	sMsg.msg_name = (void *)pName;
	sMsg.msg_namelen = nName;
	if( nData > 0 || zData ){
		sMsg.msg_iov = &sVec;
		sMsg.msg_iovlen = 1;
	}
	sMsg.msg_control = (void *)zCtl;
	sMsg.msg_controllen = nCtl;
	return (int)sendmsg(sock,&sMsg,iFlags);
#endif
}
static int SockMsgRecv(ph7_socket sock,struct sockaddr *pName,ph7_socklen *pnName,
	char *zData,unsigned int nData,char *zCtl,unsigned int nCtl,
	unsigned int *pnCtlOut,int iFlags,int *piFlagsOut)
{
#ifdef __WINNT__
	static LPFN_WSARECVMSG xRecvMsg = 0;
	WSAMSG sMsg;
	WSABUF sBuf;
	DWORD nGot = 0;
	if( xRecvMsg == 0 ){
		GUID sId = WSAID_WSARECVMSG;
		DWORD nOut = 0;
		if( WSAIoctl(sock,SIO_GET_EXTENSION_FUNCTION_POINTER,&sId,(DWORD)sizeof(sId),
			&xRecvMsg,(DWORD)sizeof(xRecvMsg),&nOut,0,0) != 0 ){
			xRecvMsg = 0;
			return -1;
		}
	}
	SyZero(&sMsg,sizeof(sMsg));
	sBuf.len = (ULONG)nData;
	sBuf.buf = zData;
	sMsg.name = pName;
	sMsg.namelen = pName ? (INT)*pnName : 0;
	sMsg.lpBuffers = &sBuf;
	sMsg.dwBufferCount = 1;
	sMsg.Control.len = (ULONG)nCtl;
	sMsg.Control.buf = zCtl;
	sMsg.dwFlags = (DWORD)iFlags;
	if( xRecvMsg(sock,&sMsg,&nGot,0,0) != 0 ){
		return -1;
	}
	if( pName ){
		*pnName = (ph7_socklen)sMsg.namelen;
	}
	*pnCtlOut = (unsigned int)sMsg.Control.len;
	*piFlagsOut = (int)sMsg.dwFlags;
	return (int)nGot;
#else
	struct msghdr sMsg;
	struct iovec sVec;
	int iGot;
	SyZero(&sMsg,sizeof(sMsg));
	sVec.iov_base = zData;
	sVec.iov_len = (size_t)nData;
	sMsg.msg_name = (void *)pName;
	sMsg.msg_namelen = pName ? *pnName : 0;
	sMsg.msg_iov = &sVec;
	sMsg.msg_iovlen = 1;
	sMsg.msg_control = zCtl;
	sMsg.msg_controllen = nCtl;
	iGot = (int)recvmsg(sock,&sMsg,iFlags);
	if( iGot < 0 ){
		return -1;
	}
	if( pName ){
		*pnName = (ph7_socklen)sMsg.msg_namelen;
	}
	*pnCtlOut = (unsigned int)sMsg.msg_controllen;
	*piFlagsOut = sMsg.msg_flags;
	return iGot;
#endif
}
/*
 * Walk a control block by hand. The layout rule is the same on both platforms
 * -- each header occupies PHL_CMSG_SPACE(payload) from its own start -- so this
 * replaces the FIRSTHDR/NXTHDR macros, which take a msghdr the two do not share.
 */
static phl_cmsghdr * SockCmsgWalk(char *zBuf,unsigned int nBuf,unsigned int *pOfft)
{
	phl_cmsghdr *pHdr;
	unsigned int nHdr = (unsigned int)PHL_CMSG_LEN(0);
	if( zBuf == 0 || *pOfft + nHdr > nBuf ){
		return 0;
	}
	pHdr = (phl_cmsghdr *)(zBuf + *pOfft);
	if( (unsigned int)pHdr->cmsg_len < nHdr || *pOfft + (unsigned int)pHdr->cmsg_len > nBuf ){
		return 0;
	}
	*pOfft += (unsigned int)PHL_CMSG_SPACE((unsigned int)pHdr->cmsg_len - nHdr);
	return pHdr;
}
/* php's one wording for every msghdr conversion failure, with the PATH in it. */
static void SockMsgErr(ph7_context *pCtx,const char *zPath,const char *zWhat)
{
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
		"error converting user data (path: %s): %s",zPath,zWhat);
}
/*
 * php's `name` entry, written. The FAMILY is the socket's unless the array
 * states one, and every field is optional -- an address with no `port` is
 * simply port 0, which is why php's own answer for it is the kernel's EINVAL
 * rather than a refusal of its own.
 */
static int SockMsgWriteName(ph7_context *pCtx,phl_socket *pSock,ph7_value *pVal,
	struct sockaddr_storage *pOut,ph7_socklen *pLen)
{
	ph7_value *pElem;
	int iFamily = pSock->iDomain;
	if( !ph7_value_is_array(pVal) ){
		SockMsgErr(pCtx,"msghdr > name","expected an array here");
		return -1;
	}
	SyZero(pOut,sizeof(*pOut));
	pElem = ph7_array_fetch(pVal,"family",sizeof("family")-1);
	if( pElem && !ph7_value_is_null(pElem) ){
		iFamily = (int)ph7_value_to_int64(pElem);
	}
	if( iFamily == AF_UNIX ){
		struct sockaddr_un *pUn = (struct sockaddr_un *)pOut;
		pUn->sun_family = AF_UNIX;
		pElem = ph7_array_fetch(pVal,"path",sizeof("path")-1);
		if( pElem ){
			int n = 0;
			const char *z = ph7_value_to_string(pElem,&n);
			if( n >= (int)sizeof(pUn->sun_path) ){
				SockMsgErr(pCtx,"msghdr > name > path","path too long");
				return -1;
			}
			SyMemcpy(z,pUn->sun_path,(sxu32)n);
		}
		*pLen = (ph7_socklen)sizeof(struct sockaddr_un);
		return 0;
	}
	if( iFamily == AF_INET6 ){
		struct sockaddr_in6 *pIn6 = (struct sockaddr_in6 *)pOut;
		pIn6->sin6_family = AF_INET6;
		pElem = ph7_array_fetch(pVal,"addr",sizeof("addr")-1);
		if( pElem ){
			char zHost[NI_MAXHOST+1];
			int n = 0;
			const char *z = ph7_value_to_string(pElem,&n);
			if( n < 0 || n >= (int)sizeof(zHost) ){
				SockMsgErr(pCtx,"msghdr > name > addr","expected a string");
				return -1;
			}
			SyMemcpy(z,zHost,(sxu32)n);
			zHost[n] = 0;
			if( inet_pton(AF_INET6,zHost,&pIn6->sin6_addr) != 1 ){
				SockMsgErr(pCtx,"msghdr > name > addr","expected a valid IPv6 address");
				return -1;
			}
		}
		pElem = ph7_array_fetch(pVal,"port",sizeof("port")-1);
		if( pElem ){
			pIn6->sin6_port = htons((unsigned short)ph7_value_to_int64(pElem));
		}
		*pLen = (ph7_socklen)sizeof(struct sockaddr_in6);
		return 0;
	}
	{
		struct sockaddr_in *pIn = (struct sockaddr_in *)pOut;
		pIn->sin_family = AF_INET;
		pElem = ph7_array_fetch(pVal,"addr",sizeof("addr")-1);
		if( pElem ){
			char zHost[NI_MAXHOST+1];
			int n = 0;
			const char *z = ph7_value_to_string(pElem,&n);
			if( n < 0 || n >= (int)sizeof(zHost) ){
				SockMsgErr(pCtx,"msghdr > name > addr","expected a string");
				return -1;
			}
			SyMemcpy(z,zHost,(sxu32)n);
			zHost[n] = 0;
			if( inet_pton(AF_INET,zHost,&pIn->sin_addr) != 1 ){
				SockMsgErr(pCtx,"msghdr > name > addr","expected a valid IPv4 address");
				return -1;
			}
		}
		pElem = ph7_array_fetch(pVal,"port",sizeof("port")-1);
		if( pElem ){
			pIn->sin_port = htons((unsigned short)ph7_value_to_int64(pElem));
		}
		*pLen = (ph7_socklen)sizeof(struct sockaddr_in);
	}
	return 0;
}
/* ...and read back, which is the shape php reports rather than the one it takes. */
static void SockMsgReadName(ph7_context *pCtx,const struct sockaddr *pAddr,ph7_socklen nAddr,
	ph7_value *pOut)
{
	ph7_value *pArr,*pTmp;
	char zBuf[INET6_ADDRSTRLEN+1];
	if( nAddr == 0 || pAddr->sa_family == 0 ){
		ph7_value_null(pOut);
		return;
	}
	pArr = ph7_context_new_array(pCtx);
	pTmp = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pTmp == 0 ){
		ph7_value_null(pOut);
		return;
	}
	ph7_value_int64(pTmp,pAddr->sa_family);
	ph7_array_add_strkey_elem(pArr,"family",pTmp);
	zBuf[0] = 0;
	if( pAddr->sa_family == AF_UNIX ){
		const struct sockaddr_un *pUn = (const struct sockaddr_un *)pAddr;
		ph7_value_string(pTmp,pUn->sun_path,(int)SyStrlen(pUn->sun_path));
		ph7_array_add_strkey_elem(pArr,"path",pTmp);
	}else if( pAddr->sa_family == AF_INET6 ){
		const struct sockaddr_in6 *pIn6 = (const struct sockaddr_in6 *)pAddr;
		inet_ntop(AF_INET6,(const void *)&pIn6->sin6_addr,zBuf,sizeof(zBuf));
		ph7_value_string(pTmp,zBuf,-1);
		ph7_array_add_strkey_elem(pArr,"addr",pTmp);
		ph7_value_int64(pTmp,(ph7_int64)ntohs(pIn6->sin6_port));
		ph7_array_add_strkey_elem(pArr,"port",pTmp);
	}else{
		const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;
		inet_ntop(AF_INET,(const void *)&pIn->sin_addr,zBuf,sizeof(zBuf));
		ph7_value_string(pTmp,zBuf,-1);
		ph7_array_add_strkey_elem(pArr,"addr",pTmp);
		ph7_value_int64(pTmp,(ph7_int64)ntohs(pIn->sin_port));
		ph7_array_add_strkey_elem(pArr,"port",pTmp);
	}
	PH7_MemObjStore(pArr,pOut);
	ph7_context_release_value(pCtx,pTmp);
}
/*
 * The `iov` array, gathered into ONE buffer. The wire cannot tell the two
 * apart -- a sendmsg() is one datagram (or one contiguous run of stream bytes)
 * whatever its iovlen -- and the copy is not an optimization but a necessity:
 * the ph7_value a walker is handed is a TEMPORARY, so a pointer into its string
 * is dangling by the time the syscall runs. Gathering by hand made a two-piece
 * message arrive as `abccon` instead of `abcdef`.
 */
typedef struct SockIovCtx SockIovCtx;
struct SockIovCtx
{
	SyBlob sBuf;
	int bInit;
};
static int SockIovAdd(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	SockIovCtx *p = (SockIovCtx *)pUserData;
	int n = 0;
	const char *z;
	SXUNUSED(pKey);
	z = ph7_value_to_string(pVal,&n);
	if( n > 0 ){
		SyBlobAppend(&p->sBuf,z,(sxu32)n);
	}
	return PH7_OK;
}
/*
 * The `control` array, in two passes over the same elements: one that adds up
 * how much room the whole block needs (and refuses a pair php has no converter
 * for) and one that writes the headers into it.
 */
typedef struct SockCtlCtx SockCtlCtx;
struct SockCtlCtx
{
	ph7_context *pCtx;
	char *zBuf;
	unsigned int nTotal;
	unsigned int nOfft;
	unsigned int iIndex;
	int bBad;
};
/* How many bytes ONE entry's payload takes, or -1 for a pair php cannot convert. */
static int SockCtlEntrySize(ph7_value *pEnt,int *piLvl,int *piTyp,unsigned int *pnData)
{
	ph7_value *pLvl,*pTyp,*pDat;
	unsigned int nFixed = 0,nUnit = 0;
	if( pEnt == 0 || !ph7_value_is_array(pEnt) ){
		return -1;
	}
	pLvl = ph7_array_fetch(pEnt,"level",sizeof("level")-1);
	pTyp = ph7_array_fetch(pEnt,"type",sizeof("type")-1);
	pDat = ph7_array_fetch(pEnt,"data",sizeof("data")-1);
	*piLvl = pLvl ? (int)ph7_value_to_int64(pLvl) : 0;
	*piTyp = pTyp ? (int)ph7_value_to_int64(pTyp) : 0;
	if( pLvl == 0 || pTyp == 0 || SockCmsgEntry(*piLvl,*piTyp,&nFixed,&nUnit) != 0 ){
		return -1;
	}
	*pnData = nFixed;
	if( nUnit > 0 && pDat && ph7_value_is_array(pDat) ){
		*pnData += nUnit * ph7_array_count(pDat);
	}
	return 0;
}
static int SockCtlSize(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	SockCtlCtx *p = (SockCtlCtx *)pUserData;
	unsigned int nData = 0;
	int iLvl = 0,iTyp = 0;
	SXUNUSED(pKey);
	if( SockCtlEntrySize(pVal,&iLvl,&iTyp,&nData) != 0 ){
		p->bBad = 1;
		ph7_context_throw_error_format(p->pCtx,PH7_CTX_WARNING,
			"error converting user data (path: msghdr > control > element #%u): "
			"cmsghdr with level %d and type %d not supported",p->iIndex,iLvl,iTyp);
		return PH7_ABORT;
	}
	p->nTotal += (unsigned int)PHL_CMSG_SPACE(nData);
	p->iIndex++;
	return PH7_OK;
}
/* One descriptor per element of an SCM_RIGHTS `data` array. */
typedef struct SockFdCtx SockFdCtx;
struct SockFdCtx { int *aFd; unsigned int nUsed; unsigned int nMax; };
static int SockCtlFd(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	SockFdCtx *p = (SockFdCtx *)pUserData;
	ph7_class_instance *pObj;
	phl_socket *pRec;
	SXUNUSED(pKey);
	if( p->nUsed >= p->nMax ){
		return PH7_ABORT;
	}
	pObj = ph7_value_is_object(pVal) ? (ph7_class_instance *)pVal->x.pOther : 0;
	pRec = (phl_socket *)SockSlotOf(pObj);
	p->aFd[p->nUsed++] = pRec ? (int)pRec->sock : -1;
	return PH7_OK;
}
static int SockCtlFill(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	SockCtlCtx *p = (SockCtlCtx *)pUserData;
	struct cmsghdr *pHdr;
	ph7_value *pDat;
	unsigned int nData = 0;
	int iLvl = 0,iTyp = 0;
	SXUNUSED(pKey);
	if( SockCtlEntrySize(pVal,&iLvl,&iTyp,&nData) != 0 ){
		return PH7_ABORT;
	}
	pHdr = (struct cmsghdr *)(p->zBuf + p->nOfft);
	pHdr->cmsg_level = iLvl;
	pHdr->cmsg_type = iTyp;
	pHdr->cmsg_len = PHL_CMSG_LEN(nData);
	pDat = ph7_array_fetch(pVal,"data",sizeof("data")-1);
#ifdef SCM_RIGHTS
	if( iLvl == SOL_SOCKET && iTyp == SCM_RIGHTS && pDat && ph7_value_is_array(pDat) ){
		SockFdCtx sFd;
		sFd.aFd = (int *)CMSG_DATA(pHdr);
		sFd.nUsed = 0;
		sFd.nMax = ph7_array_count(pDat);
		ph7_array_walk(pDat,SockCtlFd,(void *)&sFd);
	}
#endif
#ifdef SCM_CREDENTIALS
	if( iLvl == SOL_SOCKET && iTyp == SCM_CREDENTIALS && pDat && ph7_value_is_array(pDat) ){
		struct ucred sCred;
		ph7_value *pF;
		SyZero(&sCred,sizeof(sCred));
		pF = ph7_array_fetch(pDat,"pid",sizeof("pid")-1);
		sCred.pid = pF ? (pid_t)ph7_value_to_int64(pF) : 0;
		pF = ph7_array_fetch(pDat,"uid",sizeof("uid")-1);
		sCred.uid = pF ? (uid_t)ph7_value_to_int64(pF) : 0;
		pF = ph7_array_fetch(pDat,"gid",sizeof("gid")-1);
		sCred.gid = pF ? (gid_t)ph7_value_to_int64(pF) : 0;
		SyMemcpy((const void *)&sCred,(void *)PHL_CMSG_DATA(pHdr),sizeof(sCred));
	}
#endif
	p->nOfft += (unsigned int)PHL_CMSG_SPACE(nData);
	return PH7_OK;
}
/* int|false socket_sendmsg(Socket $socket, array $message, int $flags = 0) */
static int vm_builtin_socket_sendmsg(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct sockaddr_storage sAddr;
	struct sockaddr *pName = 0;
	ph7_socklen nName = 0;
	SockIovCtx sIov;
	SockCtlCtx sCtl;
	phl_socket *pSock;
	ph7_value *pElem;
	char *zCtl = 0;
	unsigned int nCtl = 0;
	int rc,iSent;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	SyZero(&sIov,sizeof(sIov));
	if( !ph7_value_is_array(apArg[1]) ){
		SockMsgErr(pCtx,"msghdr","expected an array here");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pElem = ph7_array_fetch(apArg[1],"name",sizeof("name")-1);
	if( pElem ){
		if( SockMsgWriteName(pCtx,pSock,pElem,&sAddr,&nName) != 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pName = (struct sockaddr *)&sAddr;
	}
	pElem = ph7_array_fetch(apArg[1],"iov",sizeof("iov")-1);
	if( pElem ){
		if( !ph7_value_is_array(pElem) ){
			SockMsgErr(pCtx,"msghdr > iov","expected an array here");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		SyBlobInit(&sIov.sBuf,&pCtx->pVm->sAllocator);
		sIov.bInit = 1;
		ph7_array_walk(pElem,SockIovAdd,(void *)&sIov);
	}
	pElem = ph7_array_fetch(apArg[1],"control",sizeof("control")-1);
	if( pElem ){
		/* One cmsghdr per element, laid out end to end in one buffer: the SIZE
		 * pass runs first because the whole block has to be allocated before
		 * any header can be written into it. */
		if( !ph7_value_is_array(pElem) ){
			SockMsgErr(pCtx,"msghdr > control","expected an array here");
			ph7_result_bool(pCtx,0);
			goto cleanup;
		}
		SyZero(&sCtl,sizeof(sCtl));
		sCtl.pCtx = pCtx;
		ph7_array_walk(pElem,SockCtlSize,(void *)&sCtl);
		if( sCtl.bBad ){
			ph7_result_bool(pCtx,0);
			goto cleanup;
		}
		if( sCtl.nTotal > 0 ){
			zCtl = (char *)ph7_context_alloc_chunk(pCtx,sCtl.nTotal,TRUE,FALSE);
			if( zCtl == 0 ){
				rc = PH7_ContextMemoryError(pCtx);
				goto cleanup;
			}
			nCtl = sCtl.nTotal;
			sCtl.zBuf = zCtl;
			sCtl.nOfft = 0;
			ph7_array_walk(pElem,SockCtlFill,(void *)&sCtl);
		}
	}
	iSent = SockMsgSend(pSock->sock,pName,nName,
		sIov.bInit ? (const char *)SyBlobData(&sIov.sBuf) : 0,
		sIov.bInit ? SyBlobLength(&sIov.sBuf) : 0,
		zCtl,nCtl,nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : 0);
	if( iSent < 0 ){
		SockFailLast(pCtx,pSock,"Error in sendmsg");
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_int64(pCtx,iSent);
	}
	rc = PH7_OK;
cleanup:
	if( sIov.bInit ){
		SyBlobRelease(&sIov.sBuf);
	}
	if( zCtl ){
		ph7_context_free_chunk(pCtx,zCtl);
	}
	return rc;
}
/* int|false socket_recvmsg(Socket $socket, array &$message, int $flags = 0) */
static int vm_builtin_socket_recvmsg(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct sockaddr_storage sAddr;
	struct sockaddr *pName = 0;
	ph7_socklen nName = (ph7_socklen)sizeof(sAddr);
	phl_cmsghdr *pHdr;
	phl_socket *pSock;
	ph7_value *pElem,*pOut,*pTmp,*pCtl;
	char *zBuf = 0,*zCtl = 0;
	unsigned int nCtlOut = 0,nWalk = 0;
	ph7_int64 iBufSize = 8192,iCtlLen;
	int rc,iRead,iFlagsOut = 0;
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	if( !ph7_value_is_array(apArg[1]) ){
		SockMsgErr(pCtx,"msghdr","expected an array here");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pElem = ph7_array_fetch(apArg[1],"controllen",sizeof("controllen")-1);
	if( pElem == 0 ){
		SockMsgErr(pCtx,"msghdr","The key 'controllen' is required");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iCtlLen = ph7_value_to_int64(pElem);
	if( iCtlLen < 0 || iCtlLen > 0xFFFFFFFF ){
		/* php reads this one as an unsigned 32-bit field and says so. */
		SockMsgErr(pCtx,"msghdr > controllen",
			"given PHP integer is out of bounds for an unsigned 32-bit integer");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( iCtlLen == 0 ){
		SockMsgErr(pCtx,"msghdr > controllen","controllen cannot be 0");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( iCtlLen > PHL_SOCK_MAXBUF ){
		return PH7_ContextMemoryError(pCtx);
	}
	pElem = ph7_array_fetch(apArg[1],"buffer_size",sizeof("buffer_size")-1);
	if( pElem ){
		iBufSize = ph7_value_to_int64(pElem);
		/* php's own range, and its own off-by-one wording: the message says
		 * "between 1 and", and 0 is taken. */
		if( iBufSize < 0 || iBufSize > PHL_SOCK_MSGBUF ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"error converting user data (path: msghdr > buffer_size): "
				"the buffer size must be between 1 and %d; given %qd",
				(int)PHL_SOCK_MSGBUF,iBufSize);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	SyZero(&sAddr,sizeof(sAddr));
	/* A `buffer_size` of 0 is TAKEN by php (its refusal starts at -1, whatever
	 * the message says), and a zero-byte request is not something the backend
	 * has to answer -- so the ask is one byte and the READ is still zero. */
	zBuf = (char *)ph7_context_alloc_chunk(pCtx,
		(unsigned int)(iBufSize > 0 ? iBufSize : 1),TRUE,FALSE);
	zCtl = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)iCtlLen,TRUE,FALSE);
	if( zBuf == 0 || zCtl == 0 ){
		if( zBuf ){ ph7_context_free_chunk(pCtx,zBuf); }
		if( zCtl ){ ph7_context_free_chunk(pCtx,zCtl); }
		return PH7_ContextMemoryError(pCtx);
	}
	/*
	 * php allocates room for the peer's address only when the caller's array
	 * ALREADY carries a `name` entry -- so a recvmsg() on a datagram socket
	 * reports `name => null` unless it was asked for one. That looks like an
	 * oversight and is the answer both engines have to give.
	 */
	if( ph7_array_fetch(apArg[1],"name",sizeof("name")-1) != 0 ){
		pName = (struct sockaddr *)&sAddr;
	}
	iRead = SockMsgRecv(pSock->sock,pName,&nName,zBuf,(unsigned int)iBufSize,
		zCtl,(unsigned int)iCtlLen,&nCtlOut,
		nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : 0,&iFlagsOut);
	if( iRead < 0 ){
		/*
		 * The ONE verb in this extension that does not report through php's
		 * PHP_SOCKET_ERROR: it stores the code only in the per-request slot,
		 * leaves the socket's own alone, and warns even for EAGAIN -- so a
		 * non-blocking recvmsg() with nothing waiting PRINTS where a
		 * non-blocking recv() is silent.
		 */
		pCtx->pVm->iSocketLastErr = PH7_NetLastError();
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Error in recvmsg [%d]: %s",
			pCtx->pVm->iSocketLastErr,PH7_SocketStrError(pCtx->pVm->iSocketLastErr));
		ph7_context_free_chunk(pCtx,zBuf);
		ph7_context_free_chunk(pCtx,zCtl);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pOut = ph7_context_new_array(pCtx);
	pTmp = ph7_context_new_scalar(pCtx);
	pCtl = ph7_context_new_array(pCtx);
	if( pOut == 0 || pTmp == 0 || pCtl == 0 ){
		ph7_context_free_chunk(pCtx,zBuf);
		ph7_context_free_chunk(pCtx,zCtl);
		return PH7_ContextMemoryError(pCtx);
	}
	SockMsgReadName(pCtx,(const struct sockaddr *)&sAddr,pName ? nName : 0,pTmp);
	ph7_array_add_strkey_elem(pOut,"name",pTmp);
	while( (pHdr = SockCmsgWalk(zCtl,nCtlOut,&nWalk)) != 0 ){
		ph7_value *pEnt = ph7_context_new_array(pCtx);
		if( pEnt == 0 ){
			break;
		}
		ph7_value_int64(pTmp,pHdr->cmsg_level);
		ph7_array_add_strkey_elem(pEnt,"level",pTmp);
		ph7_value_int64(pTmp,pHdr->cmsg_type);
		ph7_array_add_strkey_elem(pEnt,"type",pTmp);
#ifdef SCM_RIGHTS
		if( pHdr->cmsg_level == SOL_SOCKET && pHdr->cmsg_type == SCM_RIGHTS ){
			/* Every descriptor that arrived becomes a Socket of its own. */
			unsigned int nFd = (unsigned int)(((unsigned int)pHdr->cmsg_len
				- (unsigned int)PHL_CMSG_LEN(0)) / sizeof(int));
			ph7_value *pFds = ph7_context_new_array(pCtx);
			unsigned int j;
			for( j = 0 ; pFds && j < nFd ; ++j ){
				int fd;
				ph7_class *pClass;
				ph7_class_instance *pThis;
				phl_socket *pRec;
				SyMemcpy((const void *)(PHL_CMSG_DATA(pHdr) + j * sizeof(int)),
					(void *)&fd,sizeof(int));
				pClass = PH7_VmExtractClass(pCtx->pVm,"Socket",sizeof("Socket")-1,0,0);
				pThis = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;
				pRec = pThis ? SockNew(pCtx->pVm,(ph7_socket)fd,AF_UNIX,SOCK_STREAM,0) : 0;
				if( pRec == 0 ){
					if( pThis ){
						PH7_ClassInstanceUnref(pThis);
					}
					break;
				}
				pRec->pOwner = pThis;
				PH7_MemObjRelease(pTmp);
				pTmp->x.pOther = pThis;
				pTmp->iFlags = MEMOBJ_OBJ;
				ph7_array_add_elem(pFds,0,pTmp);
				PH7_ClassInstanceUnref(pThis);
				pTmp->iFlags = MEMOBJ_NULL;
				pTmp->x.pOther = 0;
			}
			if( pFds ){
				ph7_array_add_strkey_elem(pEnt,"data",pFds);
			}
		}else
#endif
#ifdef SCM_CREDENTIALS
		if( pHdr->cmsg_level == SOL_SOCKET && pHdr->cmsg_type == SCM_CREDENTIALS ){
			struct ucred sCred;
			ph7_value *pCr = ph7_context_new_array(pCtx);
			SyMemcpy((const void *)PHL_CMSG_DATA(pHdr),(void *)&sCred,sizeof(sCred));
			if( pCr ){
				ph7_value_int64(pTmp,(ph7_int64)sCred.pid);
				ph7_array_add_strkey_elem(pCr,"pid",pTmp);
				ph7_value_int64(pTmp,(ph7_int64)sCred.uid);
				ph7_array_add_strkey_elem(pCr,"uid",pTmp);
				ph7_value_int64(pTmp,(ph7_int64)sCred.gid);
				ph7_array_add_strkey_elem(pCr,"gid",pTmp);
				ph7_array_add_strkey_elem(pEnt,"data",pCr);
			}
		}else
#endif
		{
			ph7_int64 iVal = 0;
			if( (unsigned int)pHdr->cmsg_len >= (unsigned int)PHL_CMSG_LEN(sizeof(int)) ){
				int i32 = 0;
				SyMemcpy((const void *)PHL_CMSG_DATA(pHdr),(void *)&i32,sizeof(i32));
				iVal = i32;
			}
			ph7_value_int64(pTmp,iVal);
			ph7_array_add_strkey_elem(pEnt,"data",pTmp);
		}
		ph7_array_add_elem(pCtl,0,pEnt);
	}
	ph7_array_add_strkey_elem(pOut,"control",pCtl);
	{
		ph7_value *pIov = ph7_context_new_array(pCtx);
		if( pIov ){
			ph7_value_string(pTmp,zBuf,iRead);
			ph7_array_add_elem(pIov,0,pTmp);
			ph7_array_add_strkey_elem(pOut,"iov",pIov);
		}
	}
	ph7_value_int64(pTmp,(ph7_int64)iFlagsOut);
	ph7_array_add_strkey_elem(pOut,"flags",pTmp);
	ph7_context_free_chunk(pCtx,zBuf);
	ph7_context_free_chunk(pCtx,zCtl);
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOut);
	ph7_result_int64(pCtx,iRead);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * getaddrinfo()
 * ------------------------------------------------------------------------ */
/* Hand one candidate back as the opaque `AddressInfo` php answers with. */
static ph7_class_instance * SockNewAddrInfo(ph7_context *pCtx,const struct addrinfo *pAi)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = PH7_VmExtractClass(pVm,"AddressInfo",sizeof("AddressInfo")-1,0,0);
	ph7_class_instance *pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
	phl_addrinfo *pRec;
	if( pThis == 0 ){
		return 0;
	}
	pRec = (phl_addrinfo *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_addrinfo));
	if( pRec == 0 ){
		PH7_ClassInstanceUnref(pThis);
		return 0;
	}
	SyZero(pRec,sizeof(phl_addrinfo));
	pRec->pVm = pVm;
	pRec->iFlags = pAi->ai_flags;
	pRec->iFamily = pAi->ai_family;
	pRec->iSockType = pAi->ai_socktype;
	pRec->iProtocol = pAi->ai_protocol;
	pRec->nAddr = (ph7_socklen)pAi->ai_addrlen;
	if( pAi->ai_addrlen > 0 && pAi->ai_addrlen <= sizeof(pRec->sAddr) ){
		SyMemcpy((const void *)pAi->ai_addr,(void *)&pRec->sAddr,(sxu32)pAi->ai_addrlen);
	}
	if( pAi->ai_canonname ){
		sxu32 n = SyStrlen(pAi->ai_canonname);
		pRec->zCanon = (char *)SyMemBackendAlloc(&pVm->sAllocator,n + 1);
		if( pRec->zCanon ){
			SyMemcpy(pAi->ai_canonname,pRec->zCanon,n);
			pRec->zCanon[n] = 0;
		}
	}
	pRec->pNext = (phl_addrinfo *)pVm->pAddrInfos;
	pVm->pAddrInfos = pRec;
	if( SockSlotAttach(pThis,(void *)pRec) != 0 ){
		PH7_ClassInstanceUnref(pThis);
		return 0;
	}
	pRec->pOwner = pThis;
	return pThis;
}
/* array|false socket_addrinfo_lookup(string $host, ?string $service = null, array $hints = []) */
static int vm_builtin_socket_addrinfo_lookup(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct addrinfo sHint,*pRes = 0,*p;
	char zHost[NI_MAXHOST+1],zServ[NI_MAXSERV+1];
	const char *z;
	ph7_value *pArr,*pVal;
	int n = 0,bServ = 0;
	SyZero(&sHint,sizeof(sHint));
	z = ph7_value_to_string(apArg[0],&n);
	if( n < 0 || n >= (int)sizeof(zHost) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyMemcpy(z,zHost,(sxu32)n);
	zHost[n] = 0;
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		z = ph7_value_to_string(apArg[1],&n);
		if( n < 0 || n >= (int)sizeof(zServ) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		SyMemcpy(z,zServ,(sxu32)n);
		zServ[n] = 0;
		bServ = 1;
	}
	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){
		static const struct { const char *zKey; int iOfft; } aKey[] = {
			{ "ai_flags",    0 }, { "ai_socktype", 1 },
			{ "ai_protocol", 2 }, { "ai_family",   3 }
		};
		ph7_value *pKey;
		unsigned int i,nSeen = 0;
		for( i = 0 ; i < SX_ARRAYSIZE(aKey) ; ++i ){
			pKey = ph7_array_fetch(apArg[2],aKey[i].zKey,(int)SyStrlen(aKey[i].zKey));
			if( pKey == 0 ){
				continue;
			}
			nSeen++;
			switch( aKey[i].iOfft ){
				case 0: sHint.ai_flags    = (int)ph7_value_to_int64(pKey); break;
				case 1: sHint.ai_socktype = (int)ph7_value_to_int64(pKey); break;
				case 2: sHint.ai_protocol = (int)ph7_value_to_int64(pKey); break;
				default: sHint.ai_family  = (int)ph7_value_to_int64(pKey); break;
			}
		}
		/* php refuses a hint array carrying anything it has no field for, and
		 * lists the four it knows in its own order. */
		if( nSeen != ph7_array_count(apArg[2]) ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%z(): Argument #3 ($hints) must only contain array keys "
				"\"ai_flags\", \"ai_socktype\", \"ai_protocol\", or \"ai_family\"",
				&pCtx->pFunc->sName);
		}
	}
	if( getaddrinfo(zHost,bServ ? zServ : 0,&sHint,&pRes) != 0 || pRes == 0 ){
		if( pRes ){
			freeaddrinfo(pRes);
		}
		/* A miss is a silent false: php reports nothing at all here. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArr = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pVal == 0 ){
		freeaddrinfo(pRes);
		return PH7_ContextMemoryError(pCtx);
	}
	for( p = pRes ; p ; p = p->ai_next ){
		ph7_class_instance *pThis = SockNewAddrInfo(pCtx,p);
		if( pThis == 0 ){
			freeaddrinfo(pRes);
			return PH7_ContextMemoryError(pCtx);
		}
		PH7_MemObjRelease(pVal);
		pVal->x.pOther = pThis;
		pVal->iFlags = MEMOBJ_OBJ;
		ph7_array_add_elem(pArr,0,pVal);
		PH7_ClassInstanceUnref(pThis);
		pVal->iFlags = MEMOBJ_NULL;
		pVal->x.pOther = 0;
	}
	freeaddrinfo(pRes);
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/* array socket_addrinfo_explain(AddressInfo $address) */
static int vm_builtin_socket_addrinfo_explain(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_addrinfo *pAi;
	ph7_value *pArr,*pAddr,*pTmp;
	char zBuf[INET6_ADDRSTRLEN+1];
	SXUNUSED(nArg);
	pAi = SockAddrInfoArg(pCtx,apArg[0]);
	if( pAi == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArr = ph7_context_new_array(pCtx);
	pAddr = ph7_context_new_array(pCtx);
	pTmp = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pAddr == 0 || pTmp == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_value_int64(pTmp,pAi->iFlags);
	ph7_array_add_strkey_elem(pArr,"ai_flags",pTmp);
	ph7_value_int64(pTmp,pAi->iFamily);
	ph7_array_add_strkey_elem(pArr,"ai_family",pTmp);
	ph7_value_int64(pTmp,pAi->iSockType);
	ph7_array_add_strkey_elem(pArr,"ai_socktype",pTmp);
	ph7_value_int64(pTmp,pAi->iProtocol);
	ph7_array_add_strkey_elem(pArr,"ai_protocol",pTmp);
	if( pAi->zCanon ){
		/* Only a lookup that ASKED for AI_CANONNAME carries this key at all. */
		ph7_value_string(pTmp,pAi->zCanon,-1);
		ph7_array_add_strkey_elem(pArr,"ai_canonname",pTmp);
	}
	zBuf[0] = 0;
	if( pAi->iFamily == AF_INET6 ){
		const struct sockaddr_in6 *pIn6 = (const struct sockaddr_in6 *)&pAi->sAddr;
		ph7_value_int64(pTmp,(ph7_int64)ntohs(pIn6->sin6_port));
		ph7_array_add_strkey_elem(pAddr,"sin6_port",pTmp);
		inet_ntop(AF_INET6,(const void *)&pIn6->sin6_addr,zBuf,sizeof(zBuf));
		ph7_value_string(pTmp,zBuf,-1);
		ph7_array_add_strkey_elem(pAddr,"sin6_addr",pTmp);
	}else{
		const struct sockaddr_in *pIn = (const struct sockaddr_in *)&pAi->sAddr;
		ph7_value_int64(pTmp,(ph7_int64)ntohs(pIn->sin_port));
		ph7_array_add_strkey_elem(pAddr,"sin_port",pTmp);
		inet_ntop(AF_INET,(const void *)&pIn->sin_addr,zBuf,sizeof(zBuf));
		ph7_value_string(pTmp,zBuf,-1);
		ph7_array_add_strkey_elem(pAddr,"sin_addr",pTmp);
	}
	ph7_array_add_strkey_elem(pArr,"ai_addr",pAddr);
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/* Socket|false socket_addrinfo_bind / socket_addrinfo_connect (AddressInfo) */
static int SockAddrInfoOpen(ph7_context *pCtx,ph7_value **apArg,int bConnect)
{
	phl_addrinfo *pAi;
	ph7_socket sock;
	pAi = SockAddrInfoArg(pCtx,apArg[0]);
	if( pAi == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_NetEnsureInit();
	sock = socket(pAi->iFamily,pAi->iSockType,pAi->iProtocol);
	if( sock == PH7_NET_INVALID_SOCKET ){
		SockFailLast(pCtx,0,"Unable to create socket");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( (bConnect ? connect(sock,(struct sockaddr *)&pAi->sAddr,pAi->nAddr)
	              : bind(sock,(struct sockaddr *)&pAi->sAddr,pAi->nAddr)) != 0 ){
		SockFailLast(pCtx,0,bConnect ? "Unable to connect address" : "Unable to bind address");
		PH7_NetClose(sock);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return SockResultObject(pCtx,sock,pAi->iFamily,pAi->iSockType,pAi->iProtocol);
}
static int vm_builtin_socket_addrinfo_bind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	return SockAddrInfoOpen(pCtx,apArg,0);
}
static int vm_builtin_socket_addrinfo_connect(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	return SockAddrInfoOpen(pCtx,apArg,1);
}

#ifdef __WINNT__
/* ------------------------------------------------------------------------
 * The three Windows-only verbs
 * ------------------------------------------------------------------------ */
/*
 * Windows has no descriptor to pass down a socket, so a socket reaches another
 * PROCESS by being DUPLICATED into it: `WSADuplicateSocket()` writes a
 * WSAPROTOCOL_INFO describing it, the exporter parks that in a named shared
 * mapping, and the importer opens the mapping by name and calls WSASocket()
 * with what it finds. php names the mapping `php_wsa_for_<n>` with a
 * per-process counter -- not the pid, which is what the name reads like -- and
 * `socket_wsaprotocol_info_release()` closes the mapping so a second release
 * answers false. The three exist on no other platform.
 */
typedef struct phl_wsa_map phl_wsa_map;
struct phl_wsa_map
{
	char zName[32];
	HANDLE hMap;
	phl_wsa_map *pNext;
};
static phl_wsa_map *pWsaMaps = 0;
static unsigned int nWsaNext = 0;
/* string|false socket_wsaprotocol_info_export(Socket $socket, int $process_id) */
static int vm_builtin_socket_wsaprotocol_info_export(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	WSAPROTOCOL_INFO sInfo;
	phl_socket *pSock;
	phl_wsa_map *pMap;
	HANDLE hMap;
	void *pView;
	int rc;
	SXUNUSED(nArg);
	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);
	if( pSock == 0 ){
		return rc;
	}
	SyZero(&sInfo,sizeof(sInfo));
	if( WSADuplicateSocket(pSock->sock,(DWORD)ph7_value_to_int64(apArg[1]),&sInfo) != 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unable to export WSA protocol info [0x%08lx]: %s",
			(unsigned long)WSAGetLastError(),PH7_SocketStrError(WSAGetLastError()));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pMap = (phl_wsa_map *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_wsa_map));
	if( pMap == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	SyZero(pMap,sizeof(phl_wsa_map));
	SyBufferFormat(pMap->zName,sizeof(pMap->zName),"php_wsa_for_%u",nWsaNext++);
	hMap = CreateFileMappingA(INVALID_HANDLE_VALUE,0,PAGE_READWRITE,0,
		(DWORD)sizeof(sInfo),pMap->zName);
	if( hMap == 0 ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,pMap);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unable to create file mapping [0x%08lx]",(unsigned long)GetLastError());
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pView = MapViewOfFile(hMap,FILE_MAP_WRITE,0,0,0);
	if( pView == 0 ){
		CloseHandle(hMap);
		SyMemBackendFree(&pCtx->pVm->sAllocator,pMap);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unable to map file view [0x%08lx]",(unsigned long)GetLastError());
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyMemcpy((const void *)&sInfo,pView,(sxu32)sizeof(sInfo));
	UnmapViewOfFile(pView);
	/* The mapping stays OPEN: it only exists while a handle holds it, and the
	 * importing process needs it to still be there. */
	pMap->hMap = hMap;
	pMap->pNext = pWsaMaps;
	pWsaMaps = pMap;
	ph7_result_string(pCtx,pMap->zName,-1);
	return PH7_OK;
}
/* Socket|false socket_wsaprotocol_info_import(string $info_id) */
static int vm_builtin_socket_wsaprotocol_info_import(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	WSAPROTOCOL_INFO sInfo;
	char zName[32];
	const char *z;
	HANDLE hMap;
	void *pView;
	ph7_socket sock;
	int n = 0;
	SXUNUSED(nArg);
	z = ph7_value_to_string(apArg[0],&n);
	if( n < 1 || n >= (int)sizeof(zName) ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unable to open file mapping [0x%08lx]",2UL);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyMemcpy(z,zName,(sxu32)n);
	zName[n] = 0;
	hMap = OpenFileMappingA(FILE_MAP_READ,FALSE,zName);
	if( hMap == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unable to open file mapping [0x%08lx]",(unsigned long)GetLastError());
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pView = MapViewOfFile(hMap,FILE_MAP_READ,0,0,0);
	if( pView == 0 ){
		CloseHandle(hMap);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unable to map file view [0x%08lx]",(unsigned long)GetLastError());
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyMemcpy((const void *)pView,(void *)&sInfo,(sxu32)sizeof(sInfo));
	UnmapViewOfFile(pView);
	CloseHandle(hMap);
	sock = WSASocket(FROM_PROTOCOL_INFO,FROM_PROTOCOL_INFO,FROM_PROTOCOL_INFO,
		&sInfo,0,WSA_FLAG_OVERLAPPED);
	if( sock == PH7_NET_INVALID_SOCKET ){
		SockFailLast(pCtx,0,"Unable to create socket");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return SockResultObject(pCtx,sock,sInfo.iAddressFamily,sInfo.iSocketType,sInfo.iProtocol);
}
/* bool socket_wsaprotocol_info_release(string $info_id) */
static int vm_builtin_socket_wsaprotocol_info_release(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_wsa_map **ppSlot = &pWsaMaps;
	const char *z;
	int n = 0;
	SXUNUSED(nArg);
	z = ph7_value_to_string(apArg[0],&n);
	while( *ppSlot ){
		phl_wsa_map *pMap = *ppSlot;
		if( (int)SyStrlen(pMap->zName) == n && SyMemcmp(pMap->zName,z,(sxu32)n) == 0 ){
			*ppSlot = pMap->pNext;
			CloseHandle(pMap->hMap);
			SyMemBackendFree(&pCtx->pVm->sAllocator,pMap);
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		ppSlot = &pMap->pNext;
	}
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
#endif /* __WINNT__ */

/* ------------------------------------------------------------------------
 * The constants, the classes and the registration
 * ------------------------------------------------------------------------ */
/*
 * php's whole ext/sockets constant surface, in php's own registration order
 * (which is what ReflectionExtension::getConstants() answers in). Every one is
 * the PLATFORM's macro rather than a number copied out of one build: AF_INET6
 * is 10 here and 23 on Windows, and every SOCKET_E* name is a Winsock code
 * there (10035 for EWOULDBLOCK) rather than an errno. Two are php's own
 * inventions -- PHP_NORMAL_READ and PHP_BINARY_READ, socket_read()'s $mode --
 * and two more are php's aliases for a name the platform spells differently.
 *
 * A name this platform has no macro for is simply not registered, which is what
 * php's own #ifdef'd registration block does: the Linux-only BPF, packet-socket
 * and MCAST_* families are all absent from a Windows php too. Twelve rows go the
 * other way and are gated on the PLATFORM rather than on the macro, because the
 * macro exists on both and php registers the name on only one: `SOCKET_ESTALE`
 * is ESTALE (116) on Linux and php does not define it there.
 */
static const struct {
	const char *zName;
	int iValue;
} aSockConst[] = {
#ifdef AF_UNIX
	{ "AF_UNIX", AF_UNIX },
#endif
#ifdef AF_INET
	{ "AF_INET", AF_INET },
#endif
#ifdef AF_INET6
	{ "AF_INET6", AF_INET6 },
#endif
#ifdef AF_PACKET
	{ "AF_PACKET", AF_PACKET },
#endif
#ifdef SOCK_STREAM
	{ "SOCK_STREAM", SOCK_STREAM },
#endif
#ifdef SOCK_DGRAM
	{ "SOCK_DGRAM", SOCK_DGRAM },
#endif
#ifdef SOCK_RAW
	{ "SOCK_RAW", SOCK_RAW },
#endif
#ifdef SOCK_SEQPACKET
	{ "SOCK_SEQPACKET", SOCK_SEQPACKET },
#endif
#ifdef SOCK_RDM
	{ "SOCK_RDM", SOCK_RDM },
#endif
#ifdef SOCK_DCCP
	{ "SOCK_DCCP", SOCK_DCCP },
#endif
#ifdef SOCK_CLOEXEC
	{ "SOCK_CLOEXEC", SOCK_CLOEXEC },
#endif
#ifdef SOCK_NONBLOCK
	{ "SOCK_NONBLOCK", SOCK_NONBLOCK },
#endif
#ifdef MSG_OOB
	{ "MSG_OOB", MSG_OOB },
#endif
#ifdef MSG_WAITALL
	{ "MSG_WAITALL", MSG_WAITALL },
#endif
#ifdef MSG_CTRUNC
	{ "MSG_CTRUNC", MSG_CTRUNC },
#endif
#ifdef MSG_TRUNC
	{ "MSG_TRUNC", MSG_TRUNC },
#endif
#ifdef MSG_PEEK
	{ "MSG_PEEK", MSG_PEEK },
#endif
#ifdef MSG_DONTROUTE
	{ "MSG_DONTROUTE", MSG_DONTROUTE },
#endif
#ifdef MSG_EOR
	{ "MSG_EOR", MSG_EOR },
#endif
#if defined(MSG_EOF)
	{ "MSG_EOF", MSG_EOF },
#elif defined(MSG_FIN)
	/* php takes the Linux spelling where the BSD one is absent. */
	{ "MSG_EOF", MSG_FIN },
#endif
#ifdef MSG_CONFIRM
	{ "MSG_CONFIRM", MSG_CONFIRM },
#endif
#ifdef MSG_ERRQUEUE
	{ "MSG_ERRQUEUE", MSG_ERRQUEUE },
#endif
#ifdef MSG_NOSIGNAL
	{ "MSG_NOSIGNAL", MSG_NOSIGNAL },
#endif
#ifdef MSG_DONTWAIT
	{ "MSG_DONTWAIT", MSG_DONTWAIT },
#endif
#ifdef MSG_MORE
	{ "MSG_MORE", MSG_MORE },
#endif
#ifdef MSG_WAITFORONE
	{ "MSG_WAITFORONE", MSG_WAITFORONE },
#endif
#ifdef MSG_CMSG_CLOEXEC
	{ "MSG_CMSG_CLOEXEC", MSG_CMSG_CLOEXEC },
#endif
#ifdef MSG_ZEROCOPY
	{ "MSG_ZEROCOPY", MSG_ZEROCOPY },
#endif
#ifdef SO_DEBUG
	{ "SO_DEBUG", SO_DEBUG },
#endif
#ifdef SO_REUSEADDR
	{ "SO_REUSEADDR", SO_REUSEADDR },
#endif
#ifdef SO_REUSEPORT
	{ "SO_REUSEPORT", SO_REUSEPORT },
#endif
#ifdef SO_KEEPALIVE
	{ "SO_KEEPALIVE", SO_KEEPALIVE },
#endif
#ifdef SO_DONTROUTE
	{ "SO_DONTROUTE", SO_DONTROUTE },
#endif
#ifdef SO_LINGER
	{ "SO_LINGER", SO_LINGER },
#endif
#ifdef SO_BROADCAST
	{ "SO_BROADCAST", SO_BROADCAST },
#endif
#ifdef SO_OOBINLINE
	{ "SO_OOBINLINE", SO_OOBINLINE },
#endif
#ifdef SO_SNDBUF
	{ "SO_SNDBUF", SO_SNDBUF },
#endif
#ifdef SO_RCVBUF
	{ "SO_RCVBUF", SO_RCVBUF },
#endif
#ifdef SO_SNDLOWAT
	{ "SO_SNDLOWAT", SO_SNDLOWAT },
#endif
#ifdef SO_RCVLOWAT
	{ "SO_RCVLOWAT", SO_RCVLOWAT },
#endif
#ifdef SO_SNDTIMEO
	{ "SO_SNDTIMEO", SO_SNDTIMEO },
#endif
#ifdef SO_RCVTIMEO
	{ "SO_RCVTIMEO", SO_RCVTIMEO },
#endif
#ifdef SO_TYPE
	{ "SO_TYPE", SO_TYPE },
#endif
#ifdef SO_ERROR
	{ "SO_ERROR", SO_ERROR },
#endif
#ifdef SO_BINDTODEVICE
	{ "SO_BINDTODEVICE", SO_BINDTODEVICE },
#endif
#ifdef SO_BINDTOIFINDEX
	{ "SO_BINDTOIFINDEX", SO_BINDTOIFINDEX },
#endif
#ifdef SOL_SOCKET
	{ "SOL_SOCKET", SOL_SOCKET },
#endif
#ifdef SOMAXCONN
	{ "SOMAXCONN", SOMAXCONN },
#endif
#ifdef SO_MARK
	{ "SO_MARK", SO_MARK },
#endif
#ifdef SO_INCOMING_CPU
	{ "SO_INCOMING_CPU", SO_INCOMING_CPU },
#endif
#ifdef SO_MEMINFO
	{ "SO_MEMINFO", SO_MEMINFO },
#endif
#ifdef SO_BPF_EXTENSIONS
	{ "SO_BPF_EXTENSIONS", SO_BPF_EXTENSIONS },
#endif
#ifdef SO_BUSY_POLL
	{ "SO_BUSY_POLL", SO_BUSY_POLL },
#endif
#ifdef SKF_AD_OFF
	{ "SKF_AD_OFF", SKF_AD_OFF },
#endif
#ifdef SKF_AD_PROTOCOL
	{ "SKF_AD_PROTOCOL", SKF_AD_PROTOCOL },
#endif
#ifdef SKF_AD_PKTTYPE
	{ "SKF_AD_PKTTYPE", SKF_AD_PKTTYPE },
#endif
#ifdef SKF_AD_IFINDEX
	{ "SKF_AD_IFINDEX", SKF_AD_IFINDEX },
#endif
#ifdef SKF_AD_NLATTR
	{ "SKF_AD_NLATTR", SKF_AD_NLATTR },
#endif
#ifdef SKF_AD_NLATTR_NEST
	{ "SKF_AD_NLATTR_NEST", SKF_AD_NLATTR_NEST },
#endif
#ifdef SKF_AD_MARK
	{ "SKF_AD_MARK", SKF_AD_MARK },
#endif
#ifdef SKF_AD_QUEUE
	{ "SKF_AD_QUEUE", SKF_AD_QUEUE },
#endif
#ifdef SKF_AD_HATYPE
	{ "SKF_AD_HATYPE", SKF_AD_HATYPE },
#endif
#ifdef SKF_AD_RXHASH
	{ "SKF_AD_RXHASH", SKF_AD_RXHASH },
#endif
#ifdef SKF_AD_CPU
	{ "SKF_AD_CPU", SKF_AD_CPU },
#endif
#ifdef SKF_AD_ALU_XOR_X
	{ "SKF_AD_ALU_XOR_X", SKF_AD_ALU_XOR_X },
#endif
#ifdef SKF_AD_VLAN_TAG
	{ "SKF_AD_VLAN_TAG", SKF_AD_VLAN_TAG },
#endif
#ifdef SKF_AD_VLAN_TAG_PRESENT
	{ "SKF_AD_VLAN_TAG_PRESENT", SKF_AD_VLAN_TAG_PRESENT },
#endif
#ifdef SKF_AD_PAY_OFFSET
	{ "SKF_AD_PAY_OFFSET", SKF_AD_PAY_OFFSET },
#endif
#ifdef SKF_AD_RANDOM
	{ "SKF_AD_RANDOM", SKF_AD_RANDOM },
#endif
#ifdef SKF_AD_VLAN_TPID
	{ "SKF_AD_VLAN_TPID", SKF_AD_VLAN_TPID },
#endif
#ifdef SKF_AD_MAX
	{ "SKF_AD_MAX", SKF_AD_MAX },
#endif
#ifdef TCP_CONGESTION
	{ "TCP_CONGESTION", TCP_CONGESTION },
#endif
#ifdef TCP_SYNCNT
	{ "TCP_SYNCNT", TCP_SYNCNT },
#endif
#ifdef SO_ZEROCOPY
	{ "SO_ZEROCOPY", SO_ZEROCOPY },
#endif
#ifdef TCP_NODELAY
	{ "TCP_NODELAY", TCP_NODELAY },
#endif
#ifdef __WINNT__
	{ "TCP_KEEPALIVE", TCP_KEEPALIVE },
#endif
#ifdef TCP_NOTSENT_LOWAT
	{ "TCP_NOTSENT_LOWAT", TCP_NOTSENT_LOWAT },
#endif
#ifdef TCP_DEFER_ACCEPT
	{ "TCP_DEFER_ACCEPT", TCP_DEFER_ACCEPT },
#endif
#ifdef TCP_KEEPIDLE
	{ "TCP_KEEPIDLE", TCP_KEEPIDLE },
#endif
#ifdef TCP_KEEPINTVL
	{ "TCP_KEEPINTVL", TCP_KEEPINTVL },
#endif
#ifdef TCP_KEEPCNT
	{ "TCP_KEEPCNT", TCP_KEEPCNT },
#endif
	{ "PHP_NORMAL_READ", 1 },
	{ "PHP_BINARY_READ", 2 },
#ifdef MCAST_JOIN_GROUP
	{ "MCAST_JOIN_GROUP", MCAST_JOIN_GROUP },
#endif
#ifdef MCAST_LEAVE_GROUP
	{ "MCAST_LEAVE_GROUP", MCAST_LEAVE_GROUP },
#endif
#ifdef MCAST_BLOCK_SOURCE
	{ "MCAST_BLOCK_SOURCE", MCAST_BLOCK_SOURCE },
#endif
#ifdef MCAST_UNBLOCK_SOURCE
	{ "MCAST_UNBLOCK_SOURCE", MCAST_UNBLOCK_SOURCE },
#endif
#ifdef MCAST_JOIN_SOURCE_GROUP
	{ "MCAST_JOIN_SOURCE_GROUP", MCAST_JOIN_SOURCE_GROUP },
#endif
#ifdef MCAST_LEAVE_SOURCE_GROUP
	{ "MCAST_LEAVE_SOURCE_GROUP", MCAST_LEAVE_SOURCE_GROUP },
#endif
#ifdef IP_MULTICAST_IF
	{ "IP_MULTICAST_IF", IP_MULTICAST_IF },
#endif
#ifdef IP_MULTICAST_TTL
	{ "IP_MULTICAST_TTL", IP_MULTICAST_TTL },
#endif
#ifdef IP_MULTICAST_LOOP
	{ "IP_MULTICAST_LOOP", IP_MULTICAST_LOOP },
#endif
#ifdef IP_BIND_ADDRESS_NO_PORT
	{ "IP_BIND_ADDRESS_NO_PORT", IP_BIND_ADDRESS_NO_PORT },
#endif
#ifdef IPV6_MULTICAST_IF
	{ "IPV6_MULTICAST_IF", IPV6_MULTICAST_IF },
#endif
#ifdef IPV6_MULTICAST_HOPS
	{ "IPV6_MULTICAST_HOPS", IPV6_MULTICAST_HOPS },
#endif
#ifdef IPV6_MULTICAST_LOOP
	{ "IPV6_MULTICAST_LOOP", IPV6_MULTICAST_LOOP },
#endif
#ifdef IPV6_V6ONLY
	{ "IPV6_V6ONLY", IPV6_V6ONLY },
#endif
#ifdef EPERM
	{ "SOCKET_EPERM", EPERM },
#endif
#ifdef ENOENT
	{ "SOCKET_ENOENT", ENOENT },
#endif
#ifdef __WINNT__
	{ "SOCKET_EINTR", WSAEINTR },
#elif defined(EINTR)
	{ "SOCKET_EINTR", EINTR },
#endif
#ifdef EIO
	{ "SOCKET_EIO", EIO },
#endif
#ifdef ENXIO
	{ "SOCKET_ENXIO", ENXIO },
#endif
#ifdef E2BIG
	{ "SOCKET_E2BIG", E2BIG },
#endif
#ifdef __WINNT__
	{ "SOCKET_EBADF", WSAEBADF },
#elif defined(EBADF)
	{ "SOCKET_EBADF", EBADF },
#endif
#ifdef __WINNT__
	{ "SOCKET_EAGAIN", WSAEWOULDBLOCK },
#elif defined(EAGAIN)
	{ "SOCKET_EAGAIN", EAGAIN },
#endif
#ifdef ENOMEM
	{ "SOCKET_ENOMEM", ENOMEM },
#endif
#ifdef __WINNT__
	{ "SOCKET_EACCES", WSAEACCES },
#elif defined(EACCES)
	{ "SOCKET_EACCES", EACCES },
#endif
#ifdef __WINNT__
	{ "SOCKET_EFAULT", WSAEFAULT },
#elif defined(EFAULT)
	{ "SOCKET_EFAULT", EFAULT },
#endif
#ifdef ENOTBLK
	{ "SOCKET_ENOTBLK", ENOTBLK },
#endif
#ifdef EBUSY
	{ "SOCKET_EBUSY", EBUSY },
#endif
#ifdef EEXIST
	{ "SOCKET_EEXIST", EEXIST },
#endif
#ifdef EXDEV
	{ "SOCKET_EXDEV", EXDEV },
#endif
#ifdef ENODEV
	{ "SOCKET_ENODEV", ENODEV },
#endif
#ifdef ENOTDIR
	{ "SOCKET_ENOTDIR", ENOTDIR },
#endif
#ifdef EISDIR
	{ "SOCKET_EISDIR", EISDIR },
#endif
#ifdef __WINNT__
	{ "SOCKET_EINVAL", WSAEINVAL },
#elif defined(EINVAL)
	{ "SOCKET_EINVAL", EINVAL },
#endif
#ifdef ENFILE
	{ "SOCKET_ENFILE", ENFILE },
#endif
#ifdef __WINNT__
	{ "SOCKET_EMFILE", WSAEMFILE },
#elif defined(EMFILE)
	{ "SOCKET_EMFILE", EMFILE },
#endif
#ifdef ENOTTY
	{ "SOCKET_ENOTTY", ENOTTY },
#endif
#ifdef ENOSPC
	{ "SOCKET_ENOSPC", ENOSPC },
#endif
#ifdef ESPIPE
	{ "SOCKET_ESPIPE", ESPIPE },
#endif
#ifdef EROFS
	{ "SOCKET_EROFS", EROFS },
#endif
#ifdef EMLINK
	{ "SOCKET_EMLINK", EMLINK },
#endif
#ifdef EPIPE
	{ "SOCKET_EPIPE", EPIPE },
#endif
#ifdef __WINNT__
	{ "SOCKET_ENAMETOOLONG", WSAENAMETOOLONG },
#elif defined(ENAMETOOLONG)
	{ "SOCKET_ENAMETOOLONG", ENAMETOOLONG },
#endif
#ifdef ENOLCK
	{ "SOCKET_ENOLCK", ENOLCK },
#endif
#ifdef ENOSYS
	{ "SOCKET_ENOSYS", ENOSYS },
#endif
#ifdef __WINNT__
	{ "SOCKET_ENOTEMPTY", WSAENOTEMPTY },
#elif defined(ENOTEMPTY)
	{ "SOCKET_ENOTEMPTY", ENOTEMPTY },
#endif
#ifdef __WINNT__
	{ "SOCKET_ELOOP", WSAELOOP },
#elif defined(ELOOP)
	{ "SOCKET_ELOOP", ELOOP },
#endif
#ifdef __WINNT__
	{ "SOCKET_EWOULDBLOCK", WSAEWOULDBLOCK },
#elif defined(EWOULDBLOCK)
	{ "SOCKET_EWOULDBLOCK", EWOULDBLOCK },
#endif
#ifdef ENOMSG
	{ "SOCKET_ENOMSG", ENOMSG },
#endif
#ifdef EIDRM
	{ "SOCKET_EIDRM", EIDRM },
#endif
#ifdef ECHRNG
	{ "SOCKET_ECHRNG", ECHRNG },
#endif
#ifdef EL2NSYNC
	{ "SOCKET_EL2NSYNC", EL2NSYNC },
#endif
#ifdef EL3HLT
	{ "SOCKET_EL3HLT", EL3HLT },
#endif
#ifdef EL3RST
	{ "SOCKET_EL3RST", EL3RST },
#endif
#ifdef ELNRNG
	{ "SOCKET_ELNRNG", ELNRNG },
#endif
#ifdef EUNATCH
	{ "SOCKET_EUNATCH", EUNATCH },
#endif
#ifdef ENOCSI
	{ "SOCKET_ENOCSI", ENOCSI },
#endif
#ifdef EL2HLT
	{ "SOCKET_EL2HLT", EL2HLT },
#endif
#ifdef EBADE
	{ "SOCKET_EBADE", EBADE },
#endif
#ifdef EBADR
	{ "SOCKET_EBADR", EBADR },
#endif
#ifdef EXFULL
	{ "SOCKET_EXFULL", EXFULL },
#endif
#ifdef ENOANO
	{ "SOCKET_ENOANO", ENOANO },
#endif
#ifdef EBADRQC
	{ "SOCKET_EBADRQC", EBADRQC },
#endif
#ifdef EBADSLT
	{ "SOCKET_EBADSLT", EBADSLT },
#endif
#ifdef ENOSTR
	{ "SOCKET_ENOSTR", ENOSTR },
#endif
#ifdef ENODATA
	{ "SOCKET_ENODATA", ENODATA },
#endif
#ifdef ETIME
	{ "SOCKET_ETIME", ETIME },
#endif
#ifdef ENOSR
	{ "SOCKET_ENOSR", ENOSR },
#endif
#ifdef ENONET
	{ "SOCKET_ENONET", ENONET },
#endif
#ifdef __WINNT__
	{ "SOCKET_EREMOTE", WSAEREMOTE },
#elif defined(EREMOTE)
	{ "SOCKET_EREMOTE", EREMOTE },
#endif
#ifdef ENOLINK
	{ "SOCKET_ENOLINK", ENOLINK },
#endif
#ifdef EADV
	{ "SOCKET_EADV", EADV },
#endif
#ifdef ESRMNT
	{ "SOCKET_ESRMNT", ESRMNT },
#endif
#ifdef ECOMM
	{ "SOCKET_ECOMM", ECOMM },
#endif
#ifdef EPROTO
	{ "SOCKET_EPROTO", EPROTO },
#endif
#ifdef EMULTIHOP
	{ "SOCKET_EMULTIHOP", EMULTIHOP },
#endif
#ifdef EBADMSG
	{ "SOCKET_EBADMSG", EBADMSG },
#endif
#ifdef ENOTUNIQ
	{ "SOCKET_ENOTUNIQ", ENOTUNIQ },
#endif
#ifdef EBADFD
	{ "SOCKET_EBADFD", EBADFD },
#endif
#ifdef EREMCHG
	{ "SOCKET_EREMCHG", EREMCHG },
#endif
#ifdef ERESTART
	{ "SOCKET_ERESTART", ERESTART },
#endif
#ifdef ESTRPIPE
	{ "SOCKET_ESTRPIPE", ESTRPIPE },
#endif
#ifdef __WINNT__
	{ "SOCKET_EUSERS", WSAEUSERS },
#elif defined(EUSERS)
	{ "SOCKET_EUSERS", EUSERS },
#endif
#ifdef __WINNT__
	{ "SOCKET_ENOTSOCK", WSAENOTSOCK },
#elif defined(ENOTSOCK)
	{ "SOCKET_ENOTSOCK", ENOTSOCK },
#endif
#ifdef __WINNT__
	{ "SOCKET_EDESTADDRREQ", WSAEDESTADDRREQ },
#elif defined(EDESTADDRREQ)
	{ "SOCKET_EDESTADDRREQ", EDESTADDRREQ },
#endif
#ifdef __WINNT__
	{ "SOCKET_EMSGSIZE", WSAEMSGSIZE },
#elif defined(EMSGSIZE)
	{ "SOCKET_EMSGSIZE", EMSGSIZE },
#endif
#ifdef __WINNT__
	{ "SOCKET_EPROTOTYPE", WSAEPROTOTYPE },
#elif defined(EPROTOTYPE)
	{ "SOCKET_EPROTOTYPE", EPROTOTYPE },
#endif
#ifdef __WINNT__
	{ "SOCKET_ENOPROTOOPT", WSAENOPROTOOPT },
#elif defined(ENOPROTOOPT)
	{ "SOCKET_ENOPROTOOPT", ENOPROTOOPT },
#endif
#ifdef __WINNT__
	{ "SOCKET_EPROTONOSUPPORT", WSAEPROTONOSUPPORT },
#elif defined(EPROTONOSUPPORT)
	{ "SOCKET_EPROTONOSUPPORT", EPROTONOSUPPORT },
#endif
#ifdef __WINNT__
	{ "SOCKET_ESOCKTNOSUPPORT", WSAESOCKTNOSUPPORT },
#elif defined(ESOCKTNOSUPPORT)
	{ "SOCKET_ESOCKTNOSUPPORT", ESOCKTNOSUPPORT },
#endif
#ifdef __WINNT__
	{ "SOCKET_EOPNOTSUPP", WSAEOPNOTSUPP },
#elif defined(EOPNOTSUPP)
	{ "SOCKET_EOPNOTSUPP", EOPNOTSUPP },
#endif
#ifdef __WINNT__
	{ "SOCKET_EPFNOSUPPORT", WSAEPFNOSUPPORT },
#elif defined(EPFNOSUPPORT)
	{ "SOCKET_EPFNOSUPPORT", EPFNOSUPPORT },
#endif
#ifdef __WINNT__
	{ "SOCKET_EAFNOSUPPORT", WSAEAFNOSUPPORT },
#elif defined(EAFNOSUPPORT)
	{ "SOCKET_EAFNOSUPPORT", EAFNOSUPPORT },
#endif
#ifdef __WINNT__
	{ "SOCKET_EADDRINUSE", WSAEADDRINUSE },
#elif defined(EADDRINUSE)
	{ "SOCKET_EADDRINUSE", EADDRINUSE },
#endif
#ifdef __WINNT__
	{ "SOCKET_EADDRNOTAVAIL", WSAEADDRNOTAVAIL },
#elif defined(EADDRNOTAVAIL)
	{ "SOCKET_EADDRNOTAVAIL", EADDRNOTAVAIL },
#endif
#ifdef __WINNT__
	{ "SOCKET_ENETDOWN", WSAENETDOWN },
#elif defined(ENETDOWN)
	{ "SOCKET_ENETDOWN", ENETDOWN },
#endif
#ifdef __WINNT__
	{ "SOCKET_ENETUNREACH", WSAENETUNREACH },
#elif defined(ENETUNREACH)
	{ "SOCKET_ENETUNREACH", ENETUNREACH },
#endif
#ifdef __WINNT__
	{ "SOCKET_ENETRESET", WSAENETRESET },
#elif defined(ENETRESET)
	{ "SOCKET_ENETRESET", ENETRESET },
#endif
#ifdef __WINNT__
	{ "SOCKET_ECONNABORTED", WSAECONNABORTED },
#elif defined(ECONNABORTED)
	{ "SOCKET_ECONNABORTED", ECONNABORTED },
#endif
#ifdef __WINNT__
	{ "SOCKET_ECONNRESET", WSAECONNRESET },
#elif defined(ECONNRESET)
	{ "SOCKET_ECONNRESET", ECONNRESET },
#endif
#ifdef __WINNT__
	{ "SOCKET_ENOBUFS", WSAENOBUFS },
#elif defined(ENOBUFS)
	{ "SOCKET_ENOBUFS", ENOBUFS },
#endif
#ifdef __WINNT__
	{ "SOCKET_EISCONN", WSAEISCONN },
#elif defined(EISCONN)
	{ "SOCKET_EISCONN", EISCONN },
#endif
#ifdef __WINNT__
	{ "SOCKET_ENOTCONN", WSAENOTCONN },
#elif defined(ENOTCONN)
	{ "SOCKET_ENOTCONN", ENOTCONN },
#endif
#ifdef __WINNT__
	{ "SOCKET_ESHUTDOWN", WSAESHUTDOWN },
#elif defined(ESHUTDOWN)
	{ "SOCKET_ESHUTDOWN", ESHUTDOWN },
#endif
#ifdef __WINNT__
	{ "SOCKET_ETOOMANYREFS", WSAETOOMANYREFS },
#elif defined(ETOOMANYREFS)
	{ "SOCKET_ETOOMANYREFS", ETOOMANYREFS },
#endif
#ifdef __WINNT__
	{ "SOCKET_ETIMEDOUT", WSAETIMEDOUT },
#elif defined(ETIMEDOUT)
	{ "SOCKET_ETIMEDOUT", ETIMEDOUT },
#endif
#ifdef __WINNT__
	{ "SOCKET_ECONNREFUSED", WSAECONNREFUSED },
#elif defined(ECONNREFUSED)
	{ "SOCKET_ECONNREFUSED", ECONNREFUSED },
#endif
#ifdef __WINNT__
	{ "SOCKET_EHOSTDOWN", WSAEHOSTDOWN },
#elif defined(EHOSTDOWN)
	{ "SOCKET_EHOSTDOWN", EHOSTDOWN },
#endif
#ifdef __WINNT__
	{ "SOCKET_EHOSTUNREACH", WSAEHOSTUNREACH },
#elif defined(EHOSTUNREACH)
	{ "SOCKET_EHOSTUNREACH", EHOSTUNREACH },
#endif
#ifdef __WINNT__
	{ "SOCKET_EALREADY", WSAEALREADY },
#elif defined(EALREADY)
	{ "SOCKET_EALREADY", EALREADY },
#endif
#ifdef __WINNT__
	{ "SOCKET_EINPROGRESS", WSAEINPROGRESS },
#elif defined(EINPROGRESS)
	{ "SOCKET_EINPROGRESS", EINPROGRESS },
#endif
#ifdef EISNAM
	{ "SOCKET_EISNAM", EISNAM },
#endif
#ifdef EREMOTEIO
	{ "SOCKET_EREMOTEIO", EREMOTEIO },
#endif
#ifdef __WINNT__
	{ "SOCKET_EDQUOT", WSAEDQUOT },
#elif defined(EDQUOT)
	{ "SOCKET_EDQUOT", EDQUOT },
#endif
#ifdef __WINNT__
	{ "SOCKET_ESTALE", WSAESTALE },
#endif
#ifdef __WINNT__
	{ "SOCKET_EDISCON", WSAEDISCON },
#endif
#ifdef __WINNT__
	{ "SOCKET_SYSNOTREADY", WSASYSNOTREADY },
#endif
#ifdef __WINNT__
	{ "SOCKET_VERNOTSUPPORTED", WSAVERNOTSUPPORTED },
#endif
#ifdef __WINNT__
	{ "SOCKET_NOTINITIALISED", WSANOTINITIALISED },
#endif
#ifdef __WINNT__
	{ "SOCKET_HOST_NOT_FOUND", WSAHOST_NOT_FOUND },
#endif
#ifdef __WINNT__
	{ "SOCKET_TRY_AGAIN", WSATRY_AGAIN },
#endif
#ifdef __WINNT__
	{ "SOCKET_NO_RECOVERY", WSANO_RECOVERY },
#endif
#ifdef __WINNT__
	{ "SOCKET_NO_DATA", WSANO_DATA },
#endif
#ifdef __WINNT__
	{ "SOCKET_NO_ADDRESS", WSANO_ADDRESS },
#endif
#ifdef ENOMEDIUM
	{ "SOCKET_ENOMEDIUM", ENOMEDIUM },
#endif
#ifdef EMEDIUMTYPE
	{ "SOCKET_EMEDIUMTYPE", EMEDIUMTYPE },
#endif
#ifdef IPPROTO_IP
	{ "IPPROTO_IP", IPPROTO_IP },
#endif
#if defined(__WINNT__) || defined(IPPROTO_IPV6)
	{ "IPPROTO_IPV6", IPPROTO_IPV6 },
#endif
	/* php's are the protocol numbers on every platform; macOS and Windows have
	 * no SOL_TCP/SOL_UDP of their own. */
	{ "SOL_TCP", IPPROTO_TCP },
	{ "SOL_UDP", IPPROTO_UDP },
#if defined(SOL_UDPLITE)
	{ "SOL_UDPLITE", SOL_UDPLITE },
#elif defined(IPPROTO_UDPLITE)
	/* php names the LEVEL after the protocol where the platform has only
	 * the protocol, which is every Linux. */
	{ "SOL_UDPLITE", IPPROTO_UDPLITE },
#endif
#if defined(__WINNT__) || defined(IPPROTO_ICMP)
	{ "IPPROTO_ICMP", IPPROTO_ICMP },
#endif
#if defined(__WINNT__) || defined(IPPROTO_ICMPV6)
	{ "IPPROTO_ICMPV6", IPPROTO_ICMPV6 },
#endif
#ifdef IPV6_UNICAST_HOPS
	{ "IPV6_UNICAST_HOPS", IPV6_UNICAST_HOPS },
#endif
#ifdef AI_PASSIVE
	{ "AI_PASSIVE", AI_PASSIVE },
#endif
#ifdef AI_CANONNAME
	{ "AI_CANONNAME", AI_CANONNAME },
#endif
#ifdef AI_NUMERICHOST
	{ "AI_NUMERICHOST", AI_NUMERICHOST },
#endif
#ifdef AI_V4MAPPED
	{ "AI_V4MAPPED", AI_V4MAPPED },
#endif
#ifdef AI_ALL
	{ "AI_ALL", AI_ALL },
#endif
#ifdef AI_ADDRCONFIG
	{ "AI_ADDRCONFIG", AI_ADDRCONFIG },
#endif
#ifdef AI_IDN
	{ "AI_IDN", AI_IDN },
#endif
#ifdef AI_CANONIDN
	{ "AI_CANONIDN", AI_CANONIDN },
#endif
#ifdef AI_NUMERICSERV
	{ "AI_NUMERICSERV", AI_NUMERICSERV },
#endif
#ifdef __WINNT__
	{ "IPV6_RECVPKTINFO", IPV6_PKTINFO },
#elif defined(IPV6_RECVPKTINFO)
	{ "IPV6_RECVPKTINFO", IPV6_RECVPKTINFO },
#endif
#ifdef IPV6_PKTINFO
	{ "IPV6_PKTINFO", IPV6_PKTINFO },
#endif
#ifdef __WINNT__
	{ "IPV6_RECVHOPLIMIT", IPV6_HOPLIMIT },
#elif defined(IPV6_RECVHOPLIMIT)
	{ "IPV6_RECVHOPLIMIT", IPV6_RECVHOPLIMIT },
#endif
#ifdef IPV6_HOPLIMIT
	{ "IPV6_HOPLIMIT", IPV6_HOPLIMIT },
#endif
#ifdef IPV6_RECVTCLASS
	{ "IPV6_RECVTCLASS", IPV6_RECVTCLASS },
#endif
#ifdef IPV6_TCLASS
	{ "IPV6_TCLASS", IPV6_TCLASS },
#endif
#ifdef __WINNT__
	{ "SO_EXCLUSIVEADDRUSE", SO_EXCLUSIVEADDRUSE },
#endif
#ifdef SCM_RIGHTS
	{ "SCM_RIGHTS", SCM_RIGHTS },
#endif
#ifdef SCM_CREDENTIALS
	{ "SCM_CREDENTIALS", SCM_CREDENTIALS },
#endif
#ifdef SO_PASSCRED
	{ "SO_PASSCRED", SO_PASSCRED },
#endif
#ifdef SO_ATTACH_REUSEPORT_CBPF
	{ "SO_ATTACH_REUSEPORT_CBPF", SO_ATTACH_REUSEPORT_CBPF },
#endif
#ifdef SO_DETACH_FILTER
	{ "SO_DETACH_FILTER", SO_DETACH_FILTER },
#endif
#ifdef SO_DETACH_BPF
	{ "SO_DETACH_BPF", SO_DETACH_BPF },
#endif
#ifdef TCP_QUICKACK
	{ "TCP_QUICKACK", TCP_QUICKACK },
#endif
#ifdef TCP_REPAIR
	{ "TCP_REPAIR", TCP_REPAIR },
#endif
#ifdef IP_MTU_DISCOVER
	{ "IP_MTU_DISCOVER", IP_MTU_DISCOVER },
#endif
#ifdef IP_PMTUDISC_DO
	{ "IP_PMTUDISC_DO", IP_PMTUDISC_DO },
#endif
#ifdef IP_PMTUDISC_DONT
	{ "IP_PMTUDISC_DONT", IP_PMTUDISC_DONT },
#endif
#ifdef IP_PMTUDISC_WANT
	{ "IP_PMTUDISC_WANT", IP_PMTUDISC_WANT },
#endif
#ifdef IP_PMTUDISC_PROBE
	{ "IP_PMTUDISC_PROBE", IP_PMTUDISC_PROBE },
#endif
#ifdef IP_PMTUDISC_INTERFACE
	{ "IP_PMTUDISC_INTERFACE", IP_PMTUDISC_INTERFACE },
#endif
#ifdef IP_PMTUDISC_OMIT
	{ "IP_PMTUDISC_OMIT", IP_PMTUDISC_OMIT },
#endif
#ifdef ETH_P_IP
	{ "ETH_P_IP", ETH_P_IP },
#endif
#ifdef ETH_P_IPV6
	{ "ETH_P_IPV6", ETH_P_IPV6 },
#endif
#ifdef ETH_P_LOOP
	{ "ETH_P_LOOP", ETH_P_LOOP },
#endif
#ifdef ETH_P_ALL
	{ "ETH_P_ALL", ETH_P_ALL },
#endif
#ifdef UDP_SEGMENT
	{ "UDP_SEGMENT", UDP_SEGMENT },
#endif
#ifdef __WINNT__
	{ "SHUT_RD", SD_RECEIVE },
#elif defined(SHUT_RD)
	{ "SHUT_RD", SHUT_RD },
#endif
#ifdef __WINNT__
	{ "SHUT_WR", SD_SEND },
#elif defined(SHUT_WR)
	{ "SHUT_WR", SHUT_WR },
#endif
#ifdef __WINNT__
	{ "SHUT_RDWR", SD_BOTH },
#elif defined(SHUT_RDWR)
	{ "SHUT_RDWR", SHUT_RDWR },
#endif
};
static void SockConstExpand(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,SX_PTR_TO_INT(pUserData));
}
PH7_PRIVATE void PH7_RegisterSocketsConstants(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aSockConst) ; ++n ){
		ph7_create_constant(&(*pVm),aSockConst[n].zName,SockConstExpand,
			SX_INT_TO_PTR(aSockConst[n].iValue));
	}
}
/* The extension's functions, in php's own order. */
static const ph7_builtin_func aSockFunc[] = {
	{ "socket_select",            vm_builtin_socket_select            },
	{ "socket_create_listen",     vm_builtin_socket_create_listen     },
	{ "socket_accept",            vm_builtin_socket_accept            },
	{ "socket_set_nonblock",      vm_builtin_socket_set_nonblock      },
	{ "socket_set_block",         vm_builtin_socket_set_block         },
	{ "socket_listen",            vm_builtin_socket_listen            },
	{ "socket_close",             vm_builtin_socket_close             },
	{ "socket_write",             vm_builtin_socket_write             },
	{ "socket_read",              vm_builtin_socket_read              },
	{ "socket_getsockname",       vm_builtin_socket_getsockname       },
	{ "socket_getpeername",       vm_builtin_socket_getpeername       },
	{ "socket_create",            vm_builtin_socket_create            },
	{ "socket_connect",           vm_builtin_socket_connect           },
	{ "socket_strerror",          vm_builtin_socket_strerror          },
	{ "socket_bind",              vm_builtin_socket_bind              },
	{ "socket_recv",              vm_builtin_socket_recv              },
	{ "socket_send",              vm_builtin_socket_send              },
	{ "socket_recvfrom",          vm_builtin_socket_recvfrom          },
	{ "socket_sendto",            vm_builtin_socket_sendto            },
	{ "socket_get_option",        vm_builtin_socket_get_option        },
	{ "socket_getopt",            vm_builtin_socket_get_option        },
	{ "socket_set_option",        vm_builtin_socket_set_option        },
	{ "socket_setopt",            vm_builtin_socket_set_option        },
	{ "socket_create_pair",       vm_builtin_socket_create_pair       },
	{ "socket_shutdown",          vm_builtin_socket_shutdown          },
#ifndef __WINNT__
	{ "socket_atmark",            vm_builtin_socket_atmark            },
#endif
	{ "socket_last_error",        vm_builtin_socket_last_error        },
	{ "socket_clear_error",       vm_builtin_socket_clear_error       },
	{ "socket_import_stream",     vm_builtin_socket_import_stream     },
	{ "socket_export_stream",     vm_builtin_socket_export_stream     },
	{ "socket_sendmsg",           vm_builtin_socket_sendmsg           },
	{ "socket_recvmsg",           vm_builtin_socket_recvmsg           },
	{ "socket_cmsg_space",        vm_builtin_socket_cmsg_space        },
	{ "socket_addrinfo_lookup",   vm_builtin_socket_addrinfo_lookup   },
	{ "socket_addrinfo_connect",  vm_builtin_socket_addrinfo_connect  },
	{ "socket_addrinfo_bind",     vm_builtin_socket_addrinfo_bind     },
	{ "socket_addrinfo_explain",  vm_builtin_socket_addrinfo_explain  }
#ifdef __WINNT__
	,
	/* Windows only, and php's own order puts them last. */
	{ "socket_wsaprotocol_info_export",  vm_builtin_socket_wsaprotocol_info_export  },
	{ "socket_wsaprotocol_info_import",  vm_builtin_socket_wsaprotocol_info_import  },
	{ "socket_wsaprotocol_info_release", vm_builtin_socket_wsaprotocol_info_release }
#endif
};
PH7_PRIVATE const ph7_builtin_func * PH7_SocketsFuncTable(sxu32 *pnEntry)
{
	*pnEntry = SX_ARRAYSIZE(aSockFunc);
	return aSockFunc;
}
/*
 * `Socket` and `AddressInfo`: two FINAL classes with no method, no constant and
 * no property of their own, each refusing `new` in its own sentence and each
 * uncloneable and unserializable. They are NOT PH7_CLASS_HANDLE_ID -- `(int)$s`
 * is php's ordinary "Object of class Socket could not be converted to int"
 * warning and 1, not the object handle -- and their comparison handler
 * recognizes nothing, so two distinct sockets are unequal even though both
 * present as an empty object.
 */
PH7_PRIVATE sxi32 PH7_VmInstallSockets(ph7_vm *pVm)
{
	static const PH7_NativePropDef aProp[] = {
		{ "__res", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "Socket", 0, 0,
		  PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOSERIALIZE|PH7_CLASS_NOCLONE,
		  0, 0, 0, 0,
		  aProp, SX_ARRAYSIZE(aProp),
		  SockInstanceRelease, 0, 0 },
		{ "AddressInfo", 0, 0,
		  PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOSERIALIZE|PH7_CLASS_NOCLONE,
		  0, 0, 0, 0,
		  aProp, SX_ARRAYSIZE(aProp),
		  SockAddrInfoRelease, 0, 0 }
	};
	sxi32 rc;
	pVm->pSockets = 0;
	pVm->pAddrInfos = 0;
	pVm->iSocketLastErr = 0;
	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc == SXRET_OK ){
		static const struct { const char *zClass; const char *zMsg; } aRefusal[] = {
			{ "Socket",
			  "Cannot directly construct Socket, use socket_create() instead" },
			{ "AddressInfo",
			  "Cannot directly construct AddressInfo, use socket_addrinfo_lookup() instead" }
		};
		sxu32 n;
		for( n = 0 ; n < SX_ARRAYSIZE(aRefusal) ; ++n ){
			ph7_class *pClass = PH7_VmExtractClass(&(*pVm),aRefusal[n].zClass,
				(sxu32)SyStrlen(aRefusal[n].zClass),FALSE,0);
			if( pClass ){
				pClass->zNewRefusal = aRefusal[n].zMsg;
				pClass->xCmp = PH7_NativeCmpOpaqueHandle;
			}
		}
	}
	return rc;
}
#else
/* Ensure a non-empty translation unit when the extension is out (MSVC C4206) */
typedef int builtin_sockets_unused;
#endif /* PH7_ENABLE_NET && !PH7_DISABLE_BUILTIN_FUNC */
