# src/ph7/builtin_sockets.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1285/2107 lines (60.99%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    4 | ` */` |
|       - |    5 | `/* AF_PACKET, SOCK_DCCP, the SO_* names newer than POSIX and the linux/filter.h` |
|       - |    6 | ` * SKF_AD_* block are outside the strict subset a default compile exposes, and` |
|       - |    7 | ` * php's own build asks for them the same way. This has to come BEFORE any` |
|       - |    8 | ` * system header, which is why it sits above ph7int.h. */` |
|       - |    9 | `#ifndef _GNU_SOURCE` |
|       - |   10 | `#define _GNU_SOURCE 1` |
|       - |   11 | `#endif` |
|       - |   12 | `#ifndef _DEFAULT_SOURCE` |
|       - |   13 | `#define _DEFAULT_SOURCE 1` |
|       - |   14 | `#endif` |
|       - |   15 | `/* macOS hides IPV6_HOPLIMIT and the rest of RFC 3542's names unless asked;` |
|       - |   16 | ` * php's build asks on darwin, so php there has them. */` |
|       - |   17 | `#if defined(__APPLE__) && !defined(__APPLE_USE_RFC_3542)` |
|       - |   18 | `#define __APPLE_USE_RFC_3542 1` |
|       - |   19 | `#endif` |
|       - |   20 | `/*` |
|       - |   21 | ` * inet_addr() and the ANSI WSA* entry points are php's own choices here, and` |
|       - |   22 | ` * Winsock marks all three deprecated in favour of ones that behave differently` |
|       - |   23 | ` * (inet_pton() refuses the shorthand forms php's inet_aton() takes). This has` |
|       - |   24 | ` * to come before ph7int.h, which pulls winsock2.h in for the transport types.` |
|       - |   25 | ` */` |
|       - |   26 | `#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS` |
|       - |   27 | `#define _WINSOCK_DEPRECATED_NO_WARNINGS 1` |
|       - |   28 | `#endif` |
|       - |   29 | `#include "ph7int.h"` |
|       - |   30 | `/*` |
|       - |   31 | ` * Section:` |
|       - |   32 | ` *    php's sockets extension: the BSD socket API as php shapes it -- an opaque` |
|       - |   33 | `` *    `Socket` object, a per-socket errno beside a per-request one, and a`` |
|       - |   34 | ` *    warning-plus-false failure model rather than an exception.` |
|       - |   35 | ` * Status:` |
|       - |   36 | ` *    Stable.` |
|       - |   37 | ` *` |
|       - |   38 | ` * WHY THIS IS DERIVED AND NOT BOUND. There is no library under ext/sockets:` |
|       - |   39 | `` * php's own is a thin shell over the platform's `socket()`/`bind()`/`recv()`,`` |
|       - |   40 | ` * so what had to be reproduced is the SHAPE php gives them and nothing else.` |
|       - |   41 | ` * Every answer below was taken from php 8.5.9 on this box (and, for the` |
|       - |   42 | `` * Windows halves, from a real `php.exe`), never from reading php's source.`` |
|       - |   43 | ` *` |
|       - |   44 | ` *   THE ENGINE ALREADY HAD SOCKETS, and this is not them. net.c carries the` |
|       - |   45 | `` *   transport under `tcp://`/`udp://` and the stream builtins: a socket there`` |
|       - |   46 | ` *   is a private detail of a stream, opened by URI, read through the io_private` |
|       - |   47 | ` *   device stack and closed with fclose(). ext/sockets is the OTHER face -- the` |
|       - |   48 | ` *   descriptor itself, with the address, the flags and the options exposed --` |
|       - |   49 | ` *   so it keeps its own record and calls the platform directly. The two meet at` |
|       - |   50 | `` *   exactly two doors, `socket_import_stream()` and `socket_export_stream()`,`` |
|       - |   51 | ` *   which are the reason vfs_stream.c grew a pair of accessors for it.` |
|       - |   52 | ` *` |
|       - |   53 | ` *   A FAILURE IS A WARNING, A false, AND TWO STORED ERRNOS. php's own` |
|       - |   54 | `` *   PHP_SOCKET_ERROR records the code on the SOCKET (`socket_last_error($s)`)`` |
|       - |   55 | `` *   and in a per-request global (`socket_last_error()`), and prints`` |
|       - |   56 | ` *   "<what> [<code>]: <text>" as an E_WARNING -- except for EAGAIN,` |
|       - |   57 | ` *   EWOULDBLOCK and EINPROGRESS, which are stored silently because a` |
|       - |   58 | ` *   non-blocking program hits them as a matter of course. Nothing ever clears` |
|       - |   59 | `` *   either one but `socket_clear_error()`.`` |
|       - |   60 | ` *` |
|       - |   61 | ` *   A REFUSAL THE CALLER COULD HAVE AVOIDED IS A THROW. A domain php has no` |
|       - |   62 | `` *   name for, a port outside 0..65535, a `$value` array missing the key its`` |
|       - |   63 | `` *   option needs, an element of `socket_select()`'s array that is not a Socket:`` |
|       - |   64 | ` *   those are ValueError/TypeError, raised before any system call. And a` |
|       - |   65 | `` *   Socket whose descriptor was already closed is an `Error` -- php gives the`` |
|       - |   66 | ` *   whole extension one sentence for it, "Argument #N ($name) has already been` |
|       - |   67 | ` *   closed", because a closed handle is a program bug rather than a network` |
|       - |   68 | ` *   condition.` |
|       - |   69 | ` *` |
|       - |   70 | ``  *   A HOST LOOKUP FAILURE IS NOT AN ERRNO. php reports it as `-10000 - h_errno` `` |
|       - |   71 | `` *   so it cannot collide with one, and `socket_strerror()` reads a code below`` |
|       - |   72 | `` *   -10000 back through hstrerror(). That is why `socket_bind($s,`` |
|       - |   73 | `` *   '256.256.256.256', 0)` answers -10000 and a name that does not resolve`` |
|       - |   74 | ` *   answers -10001.` |
|       - |   75 | ` *` |
|       - |   76 | ` *   php BUILDS THIS ON WINDOWS, unlike ext/posix and ext/pcntl -- so this unit` |
|       - |   77 | ` *   is not #ifdef'd away there. What differs is the constant SET (no AF_PACKET,` |
|       - |   78 | ` *   no SCM_RIGHTS, no BPF names) and the error NUMBERS: a Winsock code is not` |
|       - |   79 | `` *   an errno, so `SOCKET_EWOULDBLOCK` is 10035 there and 11 here. Both faces`` |
|       - |   80 | ` *   come from the platform's own macros, which is what keeps them right.` |
|       - |   81 | ` */` |
|       - |   82 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|       - |   83 | `#include <string.h>` |
|       - |   84 | `#include <stdio.h>` |
|       - |   85 | `#ifdef __WINNT__` |
|       - |   86 | `#include <winsock2.h>` |
|       - |   87 | `#include <ws2tcpip.h>` |
|       - |   88 | `#include <mswsock.h>` |
|       - |   89 | ``/* `struct sockaddr_un`: Windows 10 has AF_UNIX and php's ext/sockets uses it. */`` |
|       - |   90 | `#include <afunix.h>` |
|       - |   91 | `#else` |
|       - |   92 | `#include <sys/types.h>` |
|       - |   93 | `#include <sys/socket.h>` |
|       - |   94 | `#include <sys/select.h>` |
|       - |   95 | `#include <sys/un.h>` |
|       - |   96 | `#include <netinet/in.h>` |
|       - |   97 | `#include <netinet/tcp.h>` |
|       - |   98 | `#include <arpa/inet.h>` |
|       - |   99 | `#include <netdb.h>` |
|       - |  100 | `#include <unistd.h>` |
|       - |  101 | `#include <errno.h>` |
|       - |  102 | `#include <fcntl.h>` |
|       - |  103 | `#include <net/if.h>` |
|       - |  104 | `#include <ifaddrs.h>` |
|       - |  105 | `#include <sys/ioctl.h>` |
|       - |  106 | `#ifdef __linux__` |
|       - |  107 | `#include <netinet/udp.h>` |
|       - |  108 | `#include <linux/filter.h>` |
|       - |  109 | `#include <linux/if_ether.h>` |
|       - |  110 | `#endif` |
|       - |  111 | `#endif /* __WINNT__ */` |
|       - |  112 |  |
|       - |  113 | `/* php reports a host-lookup failure in a range no errno occupies. */` |
|       - |  114 | `#define PHL_SOCK_HERR(h) (-10000 - (int)(h))` |
|       - |  115 | `/*` |
|       - |  116 | ` * The length a socket call takes. POSIX says size_t, Winsock says int, and` |
|       - |  117 | ` * MSVC's /WX makes the difference an error rather than a warning.` |
|       - |  118 | ` */` |
|       - |  119 | `#ifdef __WINNT__` |
|       - |  120 | `typedef int phl_sock_size;` |
|       - |  121 | `#else` |
|       - |  122 | `typedef size_t phl_sock_size;` |
|       - |  123 | `#endif` |
|       - |  124 | `/*` |
|       - |  125 | ` * The ancillary-data layer. Windows spells every one of these WSA_*, and its` |
|       - |  126 | `` * header is a WSACMSGHDR rather than a `struct cmsghdr` -- but the LAYOUT rule`` |
|       - |  127 | ` * is the same on both, which is what lets the control block be built and walked` |
|       - |  128 | ` * by hand here rather than through the FIRSTHDR/NXTHDR macros (those take a` |
|       - |  129 | ` * msghdr, and the two platforms have no such struct in common).` |
|       - |  130 | ` */` |
|       - |  131 | `#ifdef __WINNT__` |
|       - |  132 | `typedef WSACMSGHDR phl_cmsghdr;` |
|       - |  133 | `#define PHL_CMSG_SPACE(n) WSA_CMSG_SPACE(n)` |
|       - |  134 | `#define PHL_CMSG_LEN(n)   WSA_CMSG_LEN(n)` |
|       - |  135 | `#define PHL_CMSG_DATA(c)  WSA_CMSG_DATA(c)` |
|       - |  136 | `#else` |
|       - |  137 | `typedef struct cmsghdr phl_cmsghdr;` |
|       - |  138 | `#define PHL_CMSG_SPACE(n) CMSG_SPACE(n)` |
|       - |  139 | `#define PHL_CMSG_LEN(n)   CMSG_LEN(n)` |
|       - |  140 | `#define PHL_CMSG_DATA(c)  CMSG_DATA(c)` |
|       - |  141 | `#endif` |
|       - |  142 | `/*` |
|       - |  143 | `` * The two creation flags php masks out of `$type` before it screens the range.`` |
|       - |  144 | ` * A platform without them screens the same numbers -- there is simply nothing` |
|       - |  145 | ` * to mask -- so 0 is the right stand-in rather than an #ifdef at each use.` |
|       - |  146 | ` */` |
|       - |  147 | `#ifdef SOCK_CLOEXEC` |
|       - |  148 | `#define PHL_SOCK_CLOEXEC SOCK_CLOEXEC` |
|       - |  149 | `#else` |
|       - |  150 | `#define PHL_SOCK_CLOEXEC 0` |
|       - |  151 | `#endif` |
|       - |  152 | `#ifdef SOCK_NONBLOCK` |
|       - |  153 | `#define PHL_SOCK_NONBLOCK SOCK_NONBLOCK` |
|       - |  154 | `#else` |
|       - |  155 | `#define PHL_SOCK_NONBLOCK 0` |
|       - |  156 | `#endif` |
|       - |  157 | `/*` |
|       - |  158 | ` * The two options whose VALUE is a string. A platform without one still has to` |
|       - |  159 | ` * compile the branch that reads it, so a stand-in number no option occupies is` |
|       - |  160 | ` * what stops that branch matching anything.` |
|       - |  161 | ` */` |
|       - |  162 | `#ifdef SO_BINDTODEVICE` |
|       - |  163 | `#define PHL_SO_BINDTODEVICE SO_BINDTODEVICE` |
|       - |  164 | `#else` |
|       - |  165 | `#define PHL_SO_BINDTODEVICE (-1)` |
|       - |  166 | `#endif` |
|       - |  167 | `#ifdef TCP_CONGESTION` |
|       - |  168 | `#define PHL_TCP_CONGESTION TCP_CONGESTION` |
|       - |  169 | `#else` |
|       - |  170 | `#define PHL_TCP_CONGESTION (-1)` |
|       - |  171 | `#endif` |
|       - |  172 | `/*` |
|       - |  173 | ` * The most a single read may ask for. php hands its allocator the whole length` |
|       - |  174 | ` * and dies if it cannot have it; the chunk allocator here takes an unsigned` |
|       - |  175 | ` * int, so anything past this is refused with the same out-of-memory fatal` |
|       - |  176 | ` * rather than allocated short and then read into at full length -- which is a` |
|       - |  177 | ` * heap overflow rather than a wrong answer.` |
|       - |  178 | ` */` |
|       - |  179 | `#define PHL_SOCK_MAXBUF ((ph7_int64)0x7FFFFFFE)` |
|       - |  180 | `/* ...and php's own cap on a recvmsg() buffer, which is smaller and worded. */` |
|       - |  181 | `#define PHL_SOCK_MSGBUF ((ph7_int64)104857600)` |
|       - |  182 |  |
|       - |  183 | `typedef struct phl_socket phl_socket;` |
|       - |  184 | `typedef struct phl_addrinfo phl_addrinfo;` |
|       - |  185 | `/*` |
|       - |  186 | `` * The record behind a `Socket`. It is reached from the object through the`` |
|       - |  187 | `` * hidden `__res` slot and is ALSO chained on the per-VM registry, because a PH7`` |
|       - |  188 | ` * resource carries no destructor: the sweep at VM reset/release is what closes` |
|       - |  189 | ` * a descriptor a script left open.` |
|       - |  190 | ` */` |
|       - |  191 | `struct phl_socket` |
|       - |  192 | `{` |
|       - |  193 | `	ph7_vm *pVm;` |
|       - |  194 | `	ph7_socket sock;      /* PH7_NET_INVALID_SOCKET once socket_close() ran */` |
|       - |  195 | `	int iDomain;          /* AF_* -- remembered because the ADDRESS shape follows it */` |
|       - |  196 | `	int iType;            /* SOCK_*, with the CLOEXEC/NONBLOCK bits stripped */` |
|       - |  197 | `	int iProtocol;` |
|       - |  198 | `	int iError;           /* php's per-socket errno (socket_last_error($s)) */` |
|       - |  199 | `	int bBlocking;        /* php's own flag; the descriptor carries the truth */` |
|       - |  200 | `	int bExported;        /* socket_export_stream() handed the descriptor to a stream,` |
|       - |  201 | `	                       * which now owns the close */` |
|       - |  202 | `	void *pStream;        /* the io_private that stream is, when bExported. php answers` |
|       - |  203 | `	                       * the SAME handle to a second socket_export_stream() and` |
|       - |  204 | `	                       * closes it from socket_close(), so the record keeps it */` |
|       - |  205 | `	ph7_class_instance *pOwner;` |
|       - |  206 | `	phl_socket *pNext;` |
|       - |  207 | `};` |
|       - |  208 | `/*` |
|       - |  209 | `` * The record behind an `AddressInfo`. php hands back one object per candidate`` |
|       - |  210 | ` * getaddrinfo() answered and keeps the whole list alive behind them; this keeps` |
|       - |  211 | ` * a per-object COPY instead, so freeing one never disturbs another and there is` |
|       - |  212 | `` * no shared `freeaddrinfo` to sequence.`` |
|       - |  213 | ` */` |
|       - |  214 | `struct phl_addrinfo` |
|       - |  215 | `{` |
|       - |  216 | `	ph7_vm *pVm;` |
|       - |  217 | `	int iFlags;` |
|       - |  218 | `	int iFamily;` |
|       - |  219 | `	int iSockType;` |
|       - |  220 | `	int iProtocol;` |
|       - |  221 | `	struct sockaddr_storage sAddr;` |
|       - |  222 | `	ph7_socklen nAddr;` |
|       - |  223 | `	char *zCanon;         /* ai_canonname, or 0 */` |
|       - |  224 | `	ph7_class_instance *pOwner;` |
|       - |  225 | `	phl_addrinfo *pNext;` |
|       - |  226 | `};` |
|       - |  227 |  |
|       - |  228 | `/* ------------------------------------------------------------------------` |
|       - |  229 | ` * Handle lifetime` |
|       - |  230 | ` * ------------------------------------------------------------------------ */` |
|       - |  231 | ``/* The record behind a `__res` slot value; shared with the AddressInfo half. */`` |
|     432 |  232 | `static void * SockSlotOf(ph7_class_instance *pThis)` |
|       3 |  233 | `{` |
|       - |  234 | `	SyString sAttr;` |
|       - |  235 | `	ph7_value *pVal;` |
|     435 |  236 | `	if( pThis == 0 ){` |
|     ! 0 |  237 | `		return 0;` |
|       - |  238 | `	}` |
|     435 |  239 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|     435 |  240 | `	pVal = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|     435 |  241 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|     ! 0 |  242 | `		return 0;` |
|       - |  243 | `	}` |
|     435 |  244 | `	return pVal->x.pOther;` |
|     219 |  245 | `}` |
|      94 |  246 | `static int SockSlotAttach(ph7_class_instance *pThis,void *pRec)` |
|       3 |  247 | `{` |
|       - |  248 | `	SyString sAttr;` |
|       - |  249 | `	ph7_value *pRes;` |
|      97 |  250 | `	if( pThis == 0 ){` |
|     ! 0 |  251 | `		return -1;` |
|       - |  252 | `	}` |
|      97 |  253 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|      97 |  254 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|      97 |  255 | `	if( pRes == 0 ){` |
|     ! 0 |  256 | `		return -1;` |
|       - |  257 | `	}` |
|      97 |  258 | `	PH7_MemObjRelease(pRes);` |
|      97 |  259 | `	pRes->x.pOther = pRec;` |
|      97 |  260 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|      97 |  261 | `	return 0;` |
|      50 |  262 | `}` |
|      86 |  263 | `static phl_socket * SockNew(ph7_vm *pVm,ph7_socket sock,int iDomain,int iType,int iProtocol)` |
|       3 |  264 | `{` |
|      89 |  265 | `	phl_socket *pSock = (phl_socket *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_socket));` |
|      89 |  266 | `	if( pSock == 0 ){` |
|     ! 0 |  267 | `		return 0;` |
|       - |  268 | `	}` |
|      89 |  269 | `	SyZero(pSock,sizeof(phl_socket));` |
|      89 |  270 | `	pSock->pVm = pVm;` |
|      89 |  271 | `	pSock->sock = sock;` |
|      89 |  272 | `	pSock->iDomain = iDomain;` |
|      89 |  273 | `	pSock->iType = iType;` |
|      89 |  274 | `	pSock->iProtocol = iProtocol;` |
|      89 |  275 | `	pSock->bBlocking = 1;` |
|      89 |  276 | `	pSock->pNext = (phl_socket *)pVm->pSockets;` |
|      89 |  277 | `	pVm->pSockets = pSock;` |
|      89 |  278 | `	return pSock;` |
|      46 |  279 | `}` |
|       - |  280 | `/*` |
|       - |  281 | ` * Close the descriptor a record holds, leaving the record itself on the chain:` |
|       - |  282 | `` * php's `socket_close()` does exactly this, which is why every later verb can`` |
|       - |  283 | ` * still tell a CLOSED Socket from one the engine never made and raise its own` |
|       - |  284 | ` * "has already been closed" rather than a type error.` |
|       - |  285 | ` */` |
|     190 |  286 | `static void SockShut(phl_socket *pSock)` |
|       3 |  287 | `{` |
|     193 |  288 | `	if( pSock->sock != PH7_NET_INVALID_SOCKET ){` |
|      89 |  289 | `		if( !pSock->bExported ){` |
|      79 |  290 | `			PH7_NetClose(pSock->sock);` |
|      38 |  291 | `		}` |
|      89 |  292 | `		pSock->sock = PH7_NET_INVALID_SOCKET;` |
|      43 |  293 | `	}` |
|     193 |  294 | `}` |
|      14 |  295 | `static void SockFreeAddrInfo(phl_addrinfo *pAi)` |
|       1 |  296 | `{` |
|      15 |  297 | `	if( pAi->zCanon ){` |
|       2 |  298 | `		SyMemBackendFree(&pAi->pVm->sAllocator,pAi->zCanon);` |
|       2 |  299 | `		pAi->zCanon = 0;` |
|     ! 0 |  300 | `	}` |
|      15 |  301 | `}` |
|       - |  302 | `/* The two per-object release hooks: an object dropped before its record was. */` |
|      84 |  303 | `static void SockInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       3 |  304 | `{` |
|      87 |  305 | `	phl_socket *pSock = (phl_socket *)SockSlotOf(pThis);` |
|      42 |  306 | `	SXUNUSED(pVm);` |
|      87 |  307 | `	if( pSock == 0 \|\| pSock->pOwner != pThis ){` |
|     ! 0 |  308 | `		return;` |
|       - |  309 | `	}` |
|      87 |  310 | `	SockShut(pSock);` |
|      87 |  311 | `	pSock->pOwner = 0;` |
|      45 |  312 | `}` |
|       6 |  313 | `static void SockAddrInfoRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       1 |  314 | `{` |
|       7 |  315 | `	phl_addrinfo *pAi = (phl_addrinfo *)SockSlotOf(pThis);` |
|       3 |  316 | `	SXUNUSED(pVm);` |
|       7 |  317 | `	if( pAi == 0 \|\| pAi->pOwner != pThis ){` |
|     ! 0 |  318 | `		return;` |
|       - |  319 | `	}` |
|       7 |  320 | `	SockFreeAddrInfo(pAi);` |
|       7 |  321 | `	pAi->pOwner = 0;` |
|       4 |  322 | `}` |
|       - |  323 | `/*` |
|       - |  324 | ` * Free every registered record. Called from PH7_SocketsVmReset (a reused VM --` |
|       - |  325 | ` * the -S server's -- must not answer the next request through a descriptor the` |
|       - |  326 | ` * previous one opened) and from PH7_SocketsVmRelease before the allocator` |
|       - |  327 | ` * holding the shells is torn down.` |
|       - |  328 | ` */` |
|    5645 |  329 | `static void SockVmSweep(ph7_vm *pVm)` |
|       5 |  330 | `{` |
|    5650 |  331 | `	phl_socket *pSock = (phl_socket *)pVm->pSockets;` |
|    5650 |  332 | `	phl_addrinfo *pAi = (phl_addrinfo *)pVm->pAddrInfos;` |
|    5736 |  333 | `	while( pSock ){` |
|      89 |  334 | `		phl_socket *pNext = pSock->pNext;` |
|      89 |  335 | `		SockShut(pSock);` |
|      89 |  336 | `		SyMemBackendFree(&pVm->sAllocator,pSock);` |
|      89 |  337 | `		pSock = pNext;` |
|       3 |  338 | `	}` |
|    5650 |  339 | `	pVm->pSockets = 0;` |
|    5658 |  340 | `	while( pAi ){` |
|       9 |  341 | `		phl_addrinfo *pNext = pAi->pNext;` |
|       9 |  342 | `		SockFreeAddrInfo(pAi);` |
|       9 |  343 | `		SyMemBackendFree(&pVm->sAllocator,pAi);` |
|       9 |  344 | `		pAi = pNext;` |
|       1 |  345 | `	}` |
|    5650 |  346 | `	pVm->pAddrInfos = 0;` |
|    5650 |  347 | `}` |
|      16 |  348 | `PH7_PRIVATE void PH7_SocketsVmReset(ph7_vm *pVm)` |
|     ! 0 |  349 | `{` |
|      16 |  350 | `	SockVmSweep(pVm);` |
|      16 |  351 | `	pVm->iSocketLastErr = 0;` |
|      16 |  352 | `}` |
|    5629 |  353 | `PH7_PRIVATE void PH7_SocketsVmRelease(ph7_vm *pVm)` |
|       5 |  354 | `{` |
|    5634 |  355 | `	SockVmSweep(pVm);` |
|    5634 |  356 | `}` |
|       - |  357 |  |
|       - |  358 | `/* ------------------------------------------------------------------------` |
|       - |  359 | ` * php's failure model` |
|       - |  360 | ` * ------------------------------------------------------------------------ */` |
|       - |  361 | `/*` |
|       - |  362 | ` * php's PHP_SOCKET_ERROR: the code lands on the socket AND in the per-request` |
|       - |  363 | ` * global, and the warning is printed for everything but the three codes a` |
|       - |  364 | `` * non-blocking program meets in normal operation. `pSock` may be 0 -- the`` |
|       - |  365 | ` * lookup failures in socket_addrinfo_lookup() have no socket to blame.` |
|       - |  366 | ` */` |
|      16 |  367 | `static void SockFail(ph7_context *pCtx,phl_socket *pSock,const char *zWhat,int iErr)` |
|       3 |  368 | `{` |
|      19 |  369 | `	if( pSock ){` |
|      17 |  370 | `		pSock->iError = iErr;` |
|       7 |  371 | `	}` |
|      19 |  372 | `	pCtx->pVm->iSocketLastErr = iErr;` |
|       - |  373 | `#ifdef __WINNT__` |
|       3 |  374 | `	if( iErr != WSAEWOULDBLOCK && iErr != WSAEINPROGRESS ){` |
|       - |  375 | `#else` |
|      16 |  376 | `	if( iErr != EAGAIN && iErr != EWOULDBLOCK && iErr != EINPROGRESS ){` |
|       - |  377 | `#endif` |
|      24 |  378 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s [%d]: %s",` |
|       7 |  379 | `			zWhat,iErr,PH7_SocketStrError(iErr));` |
|       7 |  380 | `	}` |
|      19 |  381 | `}` |
|       - |  382 | `/* The same, for the errno the platform just set. */` |
|      14 |  383 | `static void SockFailLast(ph7_context *pCtx,phl_socket *pSock,const char *zWhat)` |
|       3 |  384 | `{` |
|      17 |  385 | `	SockFail(pCtx,pSock,zWhat,PH7_NetLastError());` |
|      17 |  386 | `}` |
|       - |  387 | `/*` |
|       - |  388 | ` * php's socket_strerror(): a code below -10000 is a host-lookup failure carried` |
|       - |  389 | ` * in a range no errno occupies, and is read back through hstrerror(); anything` |
|       - |  390 | ` * else is the platform's own. Shared with the builtin, hence the exported name.` |
|       - |  391 | ` */` |
|      18 |  392 | `PH7_PRIVATE const char * PH7_SocketStrError(int iErr)` |
|       3 |  393 | `{` |
|       - |  394 | `#ifdef __WINNT__` |
|       - |  395 | `	/*` |
|       - |  396 | `	 * php answers FormatMessage()'s prose here -- "A non-blocking socket` |
|       - |  397 | `	 * operation could not be completed immediately" for 10035 -- rather than` |
|       - |  398 | `	 * the short errno texts net.c words for the stream layer, and the EMPTY` |
|       - |  399 | `	 * string for a code the system has no message for. The trailing CRLF that` |
|       - |  400 | `	 * FormatMessage appends is not part of php's answer.` |
|       - |  401 | `	 */` |
|       - |  402 | `	static char zBuf[512];` |
|       3 |  403 | `	DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM\|FORMAT_MESSAGE_IGNORE_INSERTS,` |
|       - |  404 | `		0,(DWORD)iErr,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),` |
|       - |  405 | `		zBuf,(DWORD)sizeof(zBuf) - 1,0);` |
|       - |  406 | `	/* php trims the trailing CRLF **and the full stop** FormatMessage puts on` |
|       - |  407 | `	 * every one of these: its answer for 10022 is "An invalid argument was` |
|       - |  408 | `	 * supplied", not "...supplied.". */` |
|       3 |  409 | `	while( n > 0 && (zBuf[n-1] == '\r' \|\| zBuf[n-1] == '\n' \|\| zBuf[n-1] == ' '` |
|       - |  410 | `	              \|\| zBuf[n-1] == '.') ){` |
|       3 |  411 | `		n--;` |
|       3 |  412 | `	}` |
|       3 |  413 | `	zBuf[n] = 0;` |
|       3 |  414 | `	return zBuf;` |
|       - |  415 | `#else` |
|      18 |  416 | `	if( iErr < -10000 ){` |
|       2 |  417 | `		return hstrerror(-iErr - 10000);` |
|       - |  418 | `	}` |
|      16 |  419 | `	return PH7_NetStrError(iErr);` |
|       - |  420 | `#endif` |
|      12 |  421 | `}` |
|       - |  422 |  |
|       - |  423 | `/* ------------------------------------------------------------------------` |
|       - |  424 | ` * Argument screens` |
|       - |  425 | ` * ------------------------------------------------------------------------ */` |
|       - |  426 | `/*` |
|       - |  427 | ` * The Socket (or AddressInfo) argument of every verb. The signature table has` |
|       - |  428 | ` * already screened the TYPE, so a missing record means the engine tore the` |
|       - |  429 | `` * object down; a record whose descriptor is gone is php's own `Error`, one`` |
|       - |  430 | ` * sentence for the whole extension.` |
|       - |  431 | ` */` |
|     298 |  432 | `static phl_socket * SockArg(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zName,int *pRc)` |
|       3 |  433 | `{` |
|       - |  434 | `	ph7_class_instance *pThis;` |
|       - |  435 | `	phl_socket *pSock;` |
|     301 |  436 | `	*pRc = PH7_OK;` |
|     301 |  437 | `	pThis = (pArg && ph7_value_is_object(pArg)) ? (ph7_class_instance *)pArg->x.pOther : 0;` |
|     301 |  438 | `	pSock = (phl_socket *)SockSlotOf(pThis);` |
|     301 |  439 | `	if( pSock == 0 ){` |
|     ! 0 |  440 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  441 | `		return 0;` |
|       - |  442 | `	}` |
|     301 |  443 | `	if( pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|      49 |  444 | `		*pRc = PH7_VmThrowException(pCtx,"Error",` |
|       - |  445 | `			"%z(): Argument #%d ($%s) has already been closed",` |
|      24 |  446 | `			&pCtx->pFunc->sName,iPos,zName);` |
|      25 |  447 | `		return 0;` |
|       - |  448 | `	}` |
|     277 |  449 | `	return pSock;` |
|     152 |  450 | `}` |
|      10 |  451 | `static phl_addrinfo * SockAddrInfoArg(ph7_context *pCtx,ph7_value *pArg)` |
|       1 |  452 | `{` |
|      11 |  453 | `	ph7_class_instance *pThis = (pArg && ph7_value_is_object(pArg)) ?` |
|      15 |  454 | `		(ph7_class_instance *)pArg->x.pOther : 0;` |
|       5 |  455 | `	SXUNUSED(pCtx);` |
|      11 |  456 | `	return (phl_addrinfo *)SockSlotOf(pThis);` |
|       1 |  457 | `}` |
|       - |  458 | `/* php's port screen: every door that takes one refuses the same range. */` |
|      56 |  459 | `static int SockPortArg(ph7_context *pCtx,ph7_int64 iPort,int iPos,const char *zName,int *pRc)` |
|       3 |  460 | `{` |
|      59 |  461 | `	if( iPort < 0 \|\| iPort > 65535 ){` |
|      13 |  462 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  463 | `			"%z(): Argument #%d ($%s) must be between 0 and 65535",` |
|       6 |  464 | `			&pCtx->pFunc->sName,iPos,zName);` |
|       7 |  465 | `		return -1;` |
|       - |  466 | `	}` |
|      52 |  467 | `	*pRc = PH7_OK;` |
|      52 |  468 | `	return (int)iPort;` |
|      31 |  469 | `}` |
|       - |  470 |  |
|       - |  471 | `/* ------------------------------------------------------------------------` |
|       - |  472 | ` * Addresses` |
|       - |  473 | ` * ------------------------------------------------------------------------ */` |
|       - |  474 | `/*` |
|       - |  475 | ` * php's IPv4 name resolution, and it has to be the REENTRANT call rather than` |
|       - |  476 | ` * plain gethostbyname() -- not for thread safety but because the code php` |
|       - |  477 | ` * reports depends on it. gethostbyname_r() writes its error into the out` |
|       - |  478 | `` * parameter and leaves the process-wide `h_errno` to whatever glibc's resolver`` |
|       - |  479 | `` * happened to set on the way, and `-10000 - h_errno` is what php prints: that`` |
|       - |  480 | `` * is why `256.256.256.256` answers -10000 (h_errno untouched, 0) where`` |
|       - |  481 | `` * `no.such.host.invalid` answers -10001 and `notanaddr` answers -10002, all`` |
|       - |  482 | ` * three from the same failed lookup. gethostbyname() sets h_errno itself and` |
|       - |  483 | ` * would answer -10001 for all three.` |
|       - |  484 | ` */` |
|       2 |  485 | `static int SockResolve4(const char *zHost,struct in_addr *pOut)` |
|       1 |  486 | `{` |
|       - |  487 | `#if defined(__GLIBC__) && !defined(__WINNT__)` |
|       1 |  488 | `	struct hostent sHe,*pRes = 0;` |
|       - |  489 | `	char zBuf[4096];` |
|       1 |  490 | `	int iErr = 0;` |
|       - |  491 | `	/* h_errno is NOT cleared first, and that is deliberate: php never clears it` |
|       - |  492 | `	 * either, so a lookup glibc leaves it alone for reports whatever the LAST` |
|       - |  493 | ``	 * one set. `socket_bind($s,'256.256.256.256',0)` is -10000 in a fresh`` |
|       - |  494 | `	 * process and -10001 right after a failed name lookup, under both engines. */` |
|       1 |  495 | `	if( gethostbyname_r(zHost,&sHe,zBuf,sizeof(zBuf),&pRes,&iErr) < 0 \|\| pRes == 0 ){` |
|       1 |  496 | `		return -1;` |
|       - |  497 | `	}` |
|       - |  498 | `#else` |
|       2 |  499 | `	struct hostent *pRes = gethostbyname(zHost);` |
|       2 |  500 | `	if( pRes == 0 ){` |
|       2 |  501 | `		return -1;` |
|       - |  502 | `	}` |
|       - |  503 | `#endif` |
|     ! 0 |  504 | `	if( pRes->h_addrtype != AF_INET \|\| pRes->h_addr_list[0] == 0 ){` |
|     ! 0 |  505 | `		return -1;` |
|       - |  506 | `	}` |
|     ! 0 |  507 | `	SyMemcpy((const void *)pRes->h_addr_list[0],(void *)pOut,sizeof(struct in_addr));` |
|     ! 0 |  508 | `	return 0;` |
|       2 |  509 | `}` |
|       - |  510 | `/*` |
|       - |  511 | ` * Build the sockaddr a verb was asked for out of php's (string $address,` |
|       - |  512 | ` * ?int $port) pair. php resolves a name that is not already numeric -- with` |
|       - |  513 | ` * gethostbyname() for AF_INET and getaddrinfo() for AF_INET6 -- and reports a` |
|       - |  514 | ` * miss as "Host lookup failed" in the -10000 range.` |
|       - |  515 | ` *` |
|       - |  516 | ` * Answers the length to hand the system call, or 0 after reporting the failure` |
|       - |  517 | ` * itself (the caller has only to answer false), or -1 after THROWING, which is` |
|       - |  518 | ` * what a null port for an internet socket and an over-long AF_UNIX path are.` |
|       - |  519 | ` */` |
|      38 |  520 | `static int SockBuildAddr(ph7_context *pCtx,phl_socket *pSock,const char *zAddr,int nAddr,` |
|       - |  521 | `	int bHasPort,ph7_int64 iPort,int iPortPos,const char *zPortName,` |
|       - |  522 | `	struct sockaddr_storage *pOut,int *pRc)` |
|       3 |  523 | `{` |
|      41 |  524 | `	*pRc = PH7_OK;` |
|      41 |  525 | `	SyZero(pOut,sizeof(*pOut));` |
|      41 |  526 | `	if( pSock->iDomain == AF_UNIX ){` |
|     ! 0 |  527 | `		struct sockaddr_un *pUn = (struct sockaddr_un *)pOut;` |
|     ! 0 |  528 | `		if( nAddr >= (int)sizeof(pUn->sun_path) ){` |
|     ! 0 |  529 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  530 | `				"%z(): Argument #2 ($address) must be less than %d bytes",` |
|     ! 0 |  531 | `				&pCtx->pFunc->sName,(int)sizeof(pUn->sun_path));` |
|     ! 0 |  532 | `			return -1;` |
|       - |  533 | `		}` |
|     ! 0 |  534 | `		pUn->sun_family = AF_UNIX;` |
|     ! 0 |  535 | `		if( nAddr > 0 ){` |
|     ! 0 |  536 | `			SyMemcpy(zAddr,pUn->sun_path,(sxu32)nAddr);` |
|     ! 0 |  537 | `		}` |
|     ! 0 |  538 | `		return (int)(sizeof(pUn->sun_family) + (sxu32)nAddr);` |
|       - |  539 | `	}` |
|      41 |  540 | `	if( !bHasPort ){` |
|       - |  541 | `		/* php names the FAMILY it screened for, and calls both internet` |
|       - |  542 | `		 * families AF_INET in this one sentence. */` |
|       9 |  543 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  544 | `			"%z(): Argument #%d ($%s) cannot be null when the socket type is AF_INET",` |
|       4 |  545 | `			&pCtx->pFunc->sName,iPortPos,zPortName);` |
|       5 |  546 | `		return -1;` |
|       - |  547 | `	}` |
|      37 |  548 | `	if( SockPortArg(pCtx,iPort,iPortPos,zPortName,pRc) < 0 ){` |
|       5 |  549 | `		return -1;` |
|       - |  550 | `	}` |
|      32 |  551 | `	if( pSock->iDomain == AF_INET6 ){` |
|     ! 0 |  552 | `		struct sockaddr_in6 *pIn6 = (struct sockaddr_in6 *)pOut;` |
|     ! 0 |  553 | `		pIn6->sin6_family = AF_INET6;` |
|     ! 0 |  554 | `		pIn6->sin6_port = htons((unsigned short)iPort);` |
|     ! 0 |  555 | `		if( inet_pton(AF_INET6,zAddr,&pIn6->sin6_addr) != 1 ){` |
|     ! 0 |  556 | `			struct addrinfo sHint,*pRes = 0;` |
|     ! 0 |  557 | `			SyZero(&sHint,sizeof(sHint));` |
|     ! 0 |  558 | `			sHint.ai_family = AF_INET6;` |
|     ! 0 |  559 | `			sHint.ai_socktype = SOCK_DGRAM;` |
|     ! 0 |  560 | `			if( getaddrinfo(zAddr,0,&sHint,&pRes) != 0 \|\| pRes == 0 ){` |
|     ! 0 |  561 | `				if( pRes ){` |
|     ! 0 |  562 | `					freeaddrinfo(pRes);` |
|     ! 0 |  563 | `				}` |
|     ! 0 |  564 | `				SockFail(pCtx,pSock,"Host lookup failed",PHL_SOCK_HERR(h_errno));` |
|     ! 0 |  565 | `				return 0;` |
|       - |  566 | `			}` |
|     ! 0 |  567 | `			SyMemcpy((const void *)&((struct sockaddr_in6 *)pRes->ai_addr)->sin6_addr,` |
|     ! 0 |  568 | `				(void *)&pIn6->sin6_addr,sizeof(struct in6_addr));` |
|     ! 0 |  569 | `			freeaddrinfo(pRes);` |
|     ! 0 |  570 | `		}` |
|     ! 0 |  571 | `		return (int)sizeof(struct sockaddr_in6);` |
|       - |  572 | `	}` |
|       - |  573 | `	{` |
|      32 |  574 | `		struct sockaddr_in *pIn = (struct sockaddr_in *)pOut;` |
|      32 |  575 | `		pIn->sin_family = AF_INET;` |
|      32 |  576 | `		pIn->sin_port = htons((unsigned short)iPort);` |
|       - |  577 | `#ifdef __WINNT__` |
|       2 |  578 | `		pIn->sin_addr.s_addr = inet_addr(zAddr);` |
|       2 |  579 | `		if( pIn->sin_addr.s_addr == INADDR_NONE && SyStrncmp(zAddr,"255.255.255.255",16) != 0 ){` |
|       - |  580 | `#else` |
|      30 |  581 | `		if( inet_aton(zAddr,&pIn->sin_addr) == 0 ){` |
|       - |  582 | `#endif` |
|       3 |  583 | `			if( SockResolve4(zAddr,&pIn->sin_addr) != 0 ){` |
|       3 |  584 | `				SockFail(pCtx,pSock,"Host lookup failed",PHL_SOCK_HERR(h_errno));` |
|       3 |  585 | `				return 0;` |
|       - |  586 | `			}` |
|     ! 0 |  587 | `		}` |
|      30 |  588 | `		return (int)sizeof(struct sockaddr_in);` |
|       - |  589 | `	}` |
|      22 |  590 | `}` |
|       - |  591 | `/*` |
|       - |  592 | ` * Report a sockaddr back through php's (&$address, &$port) pair. An AF_UNIX` |
|       - |  593 | ` * socket writes the PATH and leaves the port argument exactly as the caller` |
|       - |  594 | ` * left it, which is php's own asymmetry rather than a null it stores.` |
|       - |  595 | ` */` |
|      34 |  596 | `static void SockReportAddr(ph7_context *pCtx,const struct sockaddr *pAddr,` |
|       - |  597 | `	ph7_value *pAddrArg,ph7_value *pPortArg)` |
|       2 |  598 | `{` |
|       - |  599 | `	char zBuf[INET6_ADDRSTRLEN+1];` |
|      36 |  600 | `	ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      36 |  601 | `	int iPort = -1;` |
|      36 |  602 | `	if( pTmp == 0 ){` |
|     ! 0 |  603 | `		return;` |
|       - |  604 | `	}` |
|      36 |  605 | `	zBuf[0] = 0;` |
|      36 |  606 | `	if( pAddr->sa_family == AF_INET6 ){` |
|     ! 0 |  607 | `		const struct sockaddr_in6 *pIn6 = (const struct sockaddr_in6 *)pAddr;` |
|     ! 0 |  608 | `		inet_ntop(AF_INET6,(const void *)&pIn6->sin6_addr,zBuf,sizeof(zBuf));` |
|     ! 0 |  609 | `		iPort = (int)ntohs(pIn6->sin6_port);` |
|     ! 0 |  610 | `		ph7_value_string(pTmp,zBuf,-1);` |
|      36 |  611 | `	}else if( pAddr->sa_family == AF_UNIX ){` |
|     ! 0 |  612 | `		const struct sockaddr_un *pUn = (const struct sockaddr_un *)pAddr;` |
|     ! 0 |  613 | `		ph7_value_string(pTmp,pUn->sun_path,(int)SyStrlen(pUn->sun_path));` |
|     ! 0 |  614 | `	}else{` |
|      36 |  615 | `		const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;` |
|      36 |  616 | `		inet_ntop(AF_INET,(const void *)&pIn->sin_addr,zBuf,sizeof(zBuf));` |
|      36 |  617 | `		iPort = (int)ntohs(pIn->sin_port);` |
|      36 |  618 | `		ph7_value_string(pTmp,zBuf,-1);` |
|       - |  619 | `	}` |
|      36 |  620 | `	PH7_VmStoreArgByRef(pCtx->pVm,pAddrArg,pTmp);` |
|      36 |  621 | `	if( pPortArg && iPort >= 0 ){` |
|      36 |  622 | `		ph7_value_int(pTmp,iPort);` |
|      36 |  623 | `		PH7_VmStoreArgByRef(pCtx->pVm,pPortArg,pTmp);` |
|      17 |  624 | `	}` |
|      36 |  625 | `	ph7_context_release_value(pCtx,pTmp);` |
|      19 |  626 | `}` |
|       - |  627 | ``/* Hand a fresh record back as the `Socket` object php answers with. */`` |
|      84 |  628 | `static int SockResultObject(ph7_context *pCtx,ph7_socket sock,int iDomain,int iType,int iProtocol)` |
|       3 |  629 | `{` |
|      87 |  630 | `	ph7_vm *pVm = pCtx->pVm;` |
|      87 |  631 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,"Socket",sizeof("Socket")-1,0,0);` |
|      87 |  632 | `	ph7_class_instance *pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|       - |  633 | `	phl_socket *pSock;` |
|      87 |  634 | `	if( pThis == 0 ){` |
|     ! 0 |  635 | `		PH7_NetClose(sock);` |
|     ! 0 |  636 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  637 | `	}` |
|      87 |  638 | `	pSock = SockNew(pVm,sock,iDomain,iType,iProtocol);` |
|      87 |  639 | `	if( pSock == 0 \|\| SockSlotAttach(pThis,(void *)pSock) != 0 ){` |
|     ! 0 |  640 | `		PH7_ClassInstanceUnref(pThis);` |
|     ! 0 |  641 | `		PH7_NetClose(sock);` |
|     ! 0 |  642 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  643 | `	}` |
|      87 |  644 | `	pSock->pOwner = pThis;` |
|      87 |  645 | `	PH7_NativeResultObject(pCtx,pThis);` |
|      87 |  646 | `	return PH7_OK;` |
|      45 |  647 | `}` |
|       - |  648 |  |
|       - |  649 | `/* ------------------------------------------------------------------------` |
|       - |  650 | ` * Creating and destroying a socket` |
|       - |  651 | ` * ------------------------------------------------------------------------ */` |
|       - |  652 | `/* Socket\|false socket_create(int $domain, int $type, int $protocol) */` |
|      46 |  653 | `static int vm_builtin_socket_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  654 | `{` |
|       - |  655 | `	ph7_int64 iDomain,iType,iProto;` |
|       - |  656 | `	ph7_socket sock;` |
|      23 |  657 | `	SXUNUSED(nArg);` |
|      49 |  658 | `	iDomain = ph7_value_to_int64(apArg[0]);` |
|      49 |  659 | `	iType   = ph7_value_to_int64(apArg[1]);` |
|      49 |  660 | `	iProto  = ph7_value_to_int64(apArg[2]);` |
|      49 |  661 | `	if( iDomain != AF_UNIX && iDomain != AF_INET && iDomain != AF_INET6` |
|       - |  662 | `#ifdef AF_PACKET` |
|       1 |  663 | `	 && iDomain != AF_PACKET` |
|       - |  664 | `#endif` |
|       - |  665 | `	){` |
|       - |  666 | `		/* php words this the same everywhere, AF_PACKET included, even on a` |
|       - |  667 | `		 * Windows build that has no such family -- verified against a real` |
|       - |  668 | `		 * php.exe rather than assumed. */` |
|       4 |  669 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  670 | `			"%z(): Argument #1 ($domain) must be one of "` |
|       2 |  671 | `			"AF_UNIX, AF_PACKET, AF_INET6, or AF_INET",&pCtx->pFunc->sName);` |
|       - |  672 | `	}` |
|       - |  673 | `	/*` |
|       - |  674 | `	 * php screens the TYPE by RANGE rather than by name: everything up to` |
|       - |  675 | `	 * SOCK_DCCP (10) passes, the two creation flags are masked out first, and a` |
|       - |  676 | `	 * negative number passes too and is refused by socket() itself. So` |
|       - |  677 | ``	 * `socket_create(AF_INET, 7, 0)` is a warning-and-false, not a throw.`` |
|       - |  678 | `	 */` |
|      47 |  679 | `	if( (iType & ~(ph7_int64)(PHL_SOCK_CLOEXEC\|PHL_SOCK_NONBLOCK)) > 10 ){` |
|       4 |  680 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  681 | `			"%z(): Argument #2 ($type) must be one of SOCK_STREAM, SOCK_DGRAM, "` |
|       - |  682 | `			"SOCK_SEQPACKET, SOCK_RAW, or SOCK_RDM optionally OR'ed with "` |
|       2 |  683 | `			"SOCK_CLOEXEC, SOCK_NONBLOCK",&pCtx->pFunc->sName);` |
|       - |  684 | `	}` |
|      45 |  685 | `	PH7_NetEnsureInit();` |
|      45 |  686 | `	sock = socket((int)iDomain,(int)iType,(int)iProto);` |
|      45 |  687 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|       3 |  688 | `		SockFailLast(pCtx,0,"Unable to create socket");` |
|       3 |  689 | `		ph7_result_bool(pCtx,0);` |
|       3 |  690 | `		return PH7_OK;` |
|       - |  691 | `	}` |
|      63 |  692 | `	return SockResultObject(pCtx,sock,(int)iDomain,` |
|      20 |  693 | `		(int)(iType & ~(ph7_int64)(PHL_SOCK_CLOEXEC\|PHL_SOCK_NONBLOCK)),(int)iProto);` |
|      26 |  694 | `}` |
|       - |  695 | `/*` |
|       - |  696 | ` * Socket\|false socket_create_listen(int $port, int $backlog = 4096)` |
|       - |  697 | ` *` |
|       - |  698 | ` * php's one-call server: an AF_INET stream socket bound to INADDR_ANY and` |
|       - |  699 | ` * listening. SO_REUSEADDR is set BEFORE the bind, which is why a program can` |
|       - |  700 | ` * restart on the same port.` |
|       - |  701 | ` */` |
|      22 |  702 | `static int vm_builtin_socket_create_listen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  703 | `{` |
|       - |  704 | `	struct sockaddr_in sIn;` |
|       - |  705 | `	ph7_socket sock;` |
|      25 |  706 | `	int iPort,iBacklog = SOMAXCONN,iOn = 1,rc;` |
|      25 |  707 | `	iPort = SockPortArg(pCtx,ph7_value_to_int64(apArg[0]),1,"port",&rc);` |
|      25 |  708 | `	if( iPort < 0 ){` |
|       3 |  709 | `		return rc;` |
|       - |  710 | `	}` |
|      22 |  711 | `	if( nArg > 1 ){` |
|     ! 0 |  712 | `		iBacklog = (int)ph7_value_to_int64(apArg[1]);` |
|     ! 0 |  713 | `	}` |
|      22 |  714 | `	PH7_NetEnsureInit();` |
|      22 |  715 | `	sock = socket(AF_INET,SOCK_STREAM,0);` |
|      22 |  716 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     ! 0 |  717 | `		SockFailLast(pCtx,0,"Unable to create socket");` |
|     ! 0 |  718 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  719 | `		return PH7_OK;` |
|       - |  720 | `	}` |
|      22 |  721 | `	setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char *)&iOn,sizeof(iOn));` |
|      22 |  722 | `	SyZero(&sIn,sizeof(sIn));` |
|      22 |  723 | `	sIn.sin_family = AF_INET;` |
|      22 |  724 | `	sIn.sin_addr.s_addr = htonl(INADDR_ANY);` |
|      22 |  725 | `	sIn.sin_port = htons((unsigned short)iPort);` |
|      22 |  726 | `	if( bind(sock,(struct sockaddr *)&sIn,sizeof(sIn)) != 0 ){` |
|     ! 0 |  727 | `		SockFailLast(pCtx,0,"unable to bind to given address");` |
|     ! 0 |  728 | `		PH7_NetClose(sock);` |
|     ! 0 |  729 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  730 | `		return PH7_OK;` |
|       - |  731 | `	}` |
|      22 |  732 | `	if( listen(sock,iBacklog) != 0 ){` |
|     ! 0 |  733 | `		SockFailLast(pCtx,0,"unable to listen on socket");` |
|     ! 0 |  734 | `		PH7_NetClose(sock);` |
|     ! 0 |  735 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  736 | `		return PH7_OK;` |
|       - |  737 | `	}` |
|      22 |  738 | `	return SockResultObject(pCtx,sock,AF_INET,SOCK_STREAM,0);` |
|      14 |  739 | `}` |
|       - |  740 | `/* bool socket_create_pair(int $domain, int $type, int $protocol, &$pair) */` |
|     ! 0 |  741 | `static int vm_builtin_socket_create_pair(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  742 | `{` |
|       - |  743 | `	ph7_socket aSock[2];` |
|       - |  744 | `	ph7_value *pArr,*pVal;` |
|     ! 0 |  745 | `	int iDomain,iType,iProto,iErrno = 0,i;` |
|     ! 0 |  746 | `	SXUNUSED(nArg);` |
|     ! 0 |  747 | `	iDomain = (int)ph7_value_to_int64(apArg[0]);` |
|     ! 0 |  748 | `	iType   = (int)ph7_value_to_int64(apArg[1]);` |
|     ! 0 |  749 | `	iProto  = (int)ph7_value_to_int64(apArg[2]);` |
|     ! 0 |  750 | `	if( iDomain != AF_UNIX && iDomain != AF_INET && iDomain != AF_INET6 ){` |
|     ! 0 |  751 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  752 | `			"%z(): Argument #1 ($domain) must be one of AF_UNIX, AF_INET6, or AF_INET",` |
|     ! 0 |  753 | `			&pCtx->pFunc->sName);` |
|       - |  754 | `	}` |
|     ! 0 |  755 | `	if( (iType & ~(PHL_SOCK_CLOEXEC\|PHL_SOCK_NONBLOCK)) > 10 ){` |
|     ! 0 |  756 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  757 | `			"%z(): Argument #2 ($type) must be one of SOCK_STREAM, SOCK_DGRAM, "` |
|       - |  758 | `			"SOCK_SEQPACKET, SOCK_RAW, or SOCK_RDM optionally OR'ed with "` |
|     ! 0 |  759 | `			"SOCK_CLOEXEC, SOCK_NONBLOCK",&pCtx->pFunc->sName);` |
|       - |  760 | `	}` |
|     ! 0 |  761 | `	PH7_NetEnsureInit();` |
|     ! 0 |  762 | `	if( PH7_NetSocketPair(iDomain,iType,iProto,aSock,&iErrno) != PH7_OK ){` |
|     ! 0 |  763 | `		SockFail(pCtx,0,"Unable to create socket pair",iErrno);` |
|     ! 0 |  764 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  765 | `		return PH7_OK;` |
|       - |  766 | `	}` |
|     ! 0 |  767 | `	pArr = ph7_context_new_array(pCtx);` |
|     ! 0 |  768 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     ! 0 |  769 | `	if( pArr == 0 \|\| pVal == 0 ){` |
|     ! 0 |  770 | `		PH7_NetClose(aSock[0]);` |
|     ! 0 |  771 | `		PH7_NetClose(aSock[1]);` |
|     ! 0 |  772 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  773 | `	}` |
|     ! 0 |  774 | `	for( i = 0 ; i < 2 ; ++i ){` |
|     ! 0 |  775 | `		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,"Socket",sizeof("Socket")-1,0,0);` |
|     ! 0 |  776 | `		ph7_class_instance *pThis = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|     ! 0 |  777 | `		phl_socket *pRec = pThis ? SockNew(pCtx->pVm,aSock[i],iDomain,iType,iProto) : 0;` |
|     ! 0 |  778 | `		if( pRec == 0 \|\| SockSlotAttach(pThis,(void *)pRec) != 0 ){` |
|     ! 0 |  779 | `			if( pThis ){` |
|     ! 0 |  780 | `				PH7_ClassInstanceUnref(pThis);` |
|     ! 0 |  781 | `			}` |
|     ! 0 |  782 | `			PH7_NetClose(aSock[i]);` |
|     ! 0 |  783 | `			if( i == 0 ){` |
|     ! 0 |  784 | `				PH7_NetClose(aSock[1]);` |
|     ! 0 |  785 | `			}` |
|     ! 0 |  786 | `			return PH7_ContextMemoryError(pCtx);` |
|       - |  787 | `		}` |
|     ! 0 |  788 | `		pRec->pOwner = pThis;` |
|     ! 0 |  789 | `		PH7_MemObjRelease(pVal);` |
|     ! 0 |  790 | `		pVal->x.pOther = pThis;` |
|     ! 0 |  791 | `		pVal->iFlags = MEMOBJ_OBJ;` |
|     ! 0 |  792 | `		ph7_array_add_elem(pArr,0,pVal);` |
|       - |  793 | `		/* The array took its own reference; drop the creation one. */` |
|     ! 0 |  794 | `		PH7_ClassInstanceUnref(pThis);` |
|     ! 0 |  795 | `		pVal->iFlags = MEMOBJ_NULL;` |
|     ! 0 |  796 | `		pVal->x.pOther = 0;` |
|     ! 0 |  797 | `	}` |
|     ! 0 |  798 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[3],pArr);` |
|     ! 0 |  799 | `	ph7_result_bool(pCtx,1);` |
|     ! 0 |  800 | `	return PH7_OK;` |
|     ! 0 |  801 | `}` |
|       - |  802 | `/*` |
|       - |  803 | ` * void socket_close(Socket $socket)` |
|       - |  804 | ` *` |
|       - |  805 | ` * A SECOND close is php's "has already been closed" Error, not a no-op -- the` |
|       - |  806 | ` * same screen every other verb runs, which is why this goes through SockArg().` |
|       - |  807 | ` */` |
|      22 |  808 | `static int vm_builtin_socket_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  809 | `{` |
|       - |  810 | `	phl_socket *pSock;` |
|       - |  811 | `	int rc;` |
|      11 |  812 | `	SXUNUSED(nArg);` |
|      24 |  813 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      24 |  814 | `	if( pSock == 0 ){` |
|       3 |  815 | `		return rc;` |
|       - |  816 | `	}` |
|       - |  817 | `	/*` |
|       - |  818 | `	 * php closes an EXPORTED socket through its stream: the two share one` |
|       - |  819 | `	 * descriptor, and closing the raw number would leave a live stream handle` |
|       - |  820 | ``	 * pointing at a recycled one. This is also why `fwrite()` on that handle`` |
|       - |  821 | `	 * afterwards is php's "must be an open stream resource" rather than a` |
|       - |  822 | `	 * write to whatever opened next.` |
|       - |  823 | `	 */` |
|      22 |  824 | `	if( pSock->bExported && pSock->pStream ){` |
|     ! 0 |  825 | `		PH7_StreamCloseExported((io_private *)pSock->pStream);` |
|     ! 0 |  826 | `		pSock->pStream = 0;` |
|     ! 0 |  827 | `		pSock->bExported = 0;` |
|     ! 0 |  828 | `		pSock->sock = PH7_NET_INVALID_SOCKET;` |
|     ! 0 |  829 | `	}` |
|      22 |  830 | `	SockShut(pSock);` |
|      22 |  831 | `	return PH7_OK;` |
|      13 |  832 | `}` |
|       - |  833 |  |
|       - |  834 | `/* ------------------------------------------------------------------------` |
|       - |  835 | ` * Naming, listening and accepting` |
|       - |  836 | ` * ------------------------------------------------------------------------ */` |
|       - |  837 | `/* bool socket_bind(Socket $socket, string $address, int $port = 0) */` |
|      10 |  838 | `static int vm_builtin_socket_bind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  839 | `{` |
|       - |  840 | `	struct sockaddr_storage sAddr;` |
|       - |  841 | `	phl_socket *pSock;` |
|       - |  842 | `	const char *zAddr;` |
|      12 |  843 | `	int nAddr = 0,nLen,rc;` |
|      12 |  844 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      12 |  845 | `	if( pSock == 0 ){` |
|       3 |  846 | `		return rc;` |
|       - |  847 | `	}` |
|      10 |  848 | `	zAddr = ph7_value_to_string(apArg[1],&nAddr);` |
|      18 |  849 | `	nLen = SockBuildAddr(pCtx,pSock,zAddr,nAddr,1,` |
|       8 |  850 | `		nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0,3,"port",&sAddr,&rc);` |
|      10 |  851 | `	if( nLen <= 0 ){` |
|       5 |  852 | `		if( nLen < 0 ){` |
|       5 |  853 | `			return rc;` |
|       - |  854 | `		}` |
|     ! 0 |  855 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  856 | `		return PH7_OK;` |
|       - |  857 | `	}` |
|       5 |  858 | `	if( bind(pSock->sock,(struct sockaddr *)&sAddr,(ph7_socklen)nLen) != 0 ){` |
|     ! 0 |  859 | `		SockFailLast(pCtx,pSock,"Unable to bind address");` |
|     ! 0 |  860 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  861 | `		return PH7_OK;` |
|       - |  862 | `	}` |
|       5 |  863 | `	ph7_result_bool(pCtx,1);` |
|       5 |  864 | `	return PH7_OK;` |
|       7 |  865 | `}` |
|       - |  866 | `/* bool socket_connect(Socket $socket, string $address, ?int $port = null) */` |
|      24 |  867 | `static int vm_builtin_socket_connect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  868 | `{` |
|       - |  869 | `	struct sockaddr_storage sAddr;` |
|       - |  870 | `	phl_socket *pSock;` |
|       - |  871 | `	const char *zAddr;` |
|      27 |  872 | `	int nAddr = 0,nLen,bHasPort,rc;` |
|      27 |  873 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      27 |  874 | `	if( pSock == 0 ){` |
|     ! 0 |  875 | `		return rc;` |
|       - |  876 | `	}` |
|      27 |  877 | `	zAddr = ph7_value_to_string(apArg[1],&nAddr);` |
|      27 |  878 | `	bHasPort = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      50 |  879 | `	nLen = SockBuildAddr(pCtx,pSock,zAddr,nAddr,bHasPort,` |
|      23 |  880 | `		bHasPort ? ph7_value_to_int64(apArg[2]) : 0,3,"port",&sAddr,&rc);` |
|      27 |  881 | `	if( nLen <= 0 ){` |
|       3 |  882 | `		if( nLen < 0 ){` |
|       3 |  883 | `			return rc;` |
|       - |  884 | `		}` |
|     ! 0 |  885 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  886 | `		return PH7_OK;` |
|       - |  887 | `	}` |
|      24 |  888 | `	if( connect(pSock->sock,(struct sockaddr *)&sAddr,(ph7_socklen)nLen) != 0 ){` |
|       3 |  889 | `		SockFailLast(pCtx,pSock,"unable to connect");` |
|       3 |  890 | `		ph7_result_bool(pCtx,0);` |
|       3 |  891 | `		return PH7_OK;` |
|       - |  892 | `	}` |
|      22 |  893 | `	ph7_result_bool(pCtx,1);` |
|      22 |  894 | `	return PH7_OK;` |
|      15 |  895 | `}` |
|       - |  896 | `/* bool socket_listen(Socket $socket, int $backlog = 0) */` |
|       2 |  897 | `static int vm_builtin_socket_listen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  898 | `{` |
|       - |  899 | `	phl_socket *pSock;` |
|       - |  900 | `	int rc;` |
|       3 |  901 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|       3 |  902 | `	if( pSock == 0 ){` |
|       3 |  903 | `		return rc;` |
|       - |  904 | `	}` |
|     ! 0 |  905 | `	if( listen(pSock->sock,nArg > 1 ? (int)ph7_value_to_int64(apArg[1]) : 0) != 0 ){` |
|     ! 0 |  906 | `		SockFailLast(pCtx,pSock,"Unable to listen on socket");` |
|     ! 0 |  907 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  908 | `		return PH7_OK;` |
|       - |  909 | `	}` |
|     ! 0 |  910 | `	ph7_result_bool(pCtx,1);` |
|     ! 0 |  911 | `	return PH7_OK;` |
|       2 |  912 | `}` |
|       - |  913 | `/*` |
|       - |  914 | ` * Socket\|false socket_accept(Socket $socket)` |
|       - |  915 | ` *` |
|       - |  916 | ` * php stores the failure on the socket it was ABOUT to hand back rather than on` |
|       - |  917 | `` * the listener -- so `socket_last_error($listener)` after a would-block accept`` |
|       - |  918 | `` * is 0 while `socket_last_error()` is EAGAIN. That asymmetry is visible and is`` |
|       - |  919 | ` * reproduced by passing no socket to the reporter.` |
|       - |  920 | ` */` |
|      22 |  921 | `static int vm_builtin_socket_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  922 | `{` |
|       - |  923 | `	struct sockaddr_storage sAddr;` |
|      25 |  924 | `	ph7_socklen nAddr = (ph7_socklen)sizeof(sAddr);` |
|       - |  925 | `	phl_socket *pSock;` |
|       - |  926 | `	ph7_socket sock;` |
|       - |  927 | `	int rc;` |
|      11 |  928 | `	SXUNUSED(nArg);` |
|      25 |  929 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      25 |  930 | `	if( pSock == 0 ){` |
|       3 |  931 | `		return rc;` |
|       - |  932 | `	}` |
|      22 |  933 | `	sock = accept(pSock->sock,(struct sockaddr *)&sAddr,&nAddr);` |
|      22 |  934 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     ! 0 |  935 | `		SockFailLast(pCtx,0,"unable to accept incoming connection");` |
|     ! 0 |  936 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  937 | `		return PH7_OK;` |
|       - |  938 | `	}` |
|      22 |  939 | `	return SockResultObject(pCtx,sock,pSock->iDomain,pSock->iType,pSock->iProtocol);` |
|      14 |  940 | `}` |
|       - |  941 | `/* bool socket_getsockname / socket_getpeername (Socket, &$address, &$port = null) */` |
|      36 |  942 | `static int SockName(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPeer)` |
|       3 |  943 | `{` |
|       - |  944 | `	struct sockaddr_storage sAddr;` |
|      39 |  945 | `	ph7_socklen nAddr = (ph7_socklen)sizeof(sAddr);` |
|       - |  946 | `	phl_socket *pSock;` |
|       - |  947 | `	int rc,iRet;` |
|      39 |  948 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      39 |  949 | `	if( pSock == 0 ){` |
|     ! 0 |  950 | `		return rc;` |
|       - |  951 | `	}` |
|      39 |  952 | `	SyZero(&sAddr,sizeof(sAddr));` |
|      25 |  953 | `	iRet = bPeer ? getpeername(pSock->sock,(struct sockaddr *)&sAddr,&nAddr)` |
|      32 |  954 | `	             : getsockname(pSock->sock,(struct sockaddr *)&sAddr,&nAddr);` |
|      39 |  955 | `	if( iRet != 0 ){` |
|       6 |  956 | `		SockFailLast(pCtx,pSock,bPeer ? "unable to retrieve peer name"` |
|       - |  957 | `		                              : "unable to retrieve socket name");` |
|       6 |  958 | `		ph7_result_bool(pCtx,0);` |
|       6 |  959 | `		return PH7_OK;` |
|       - |  960 | `	}` |
|      34 |  961 | `	SockReportAddr(pCtx,(const struct sockaddr *)&sAddr,apArg[1],nArg > 2 ? apArg[2] : 0);` |
|      34 |  962 | `	ph7_result_bool(pCtx,1);` |
|      34 |  963 | `	return PH7_OK;` |
|      21 |  964 | `}` |
|      28 |  965 | `static int vm_builtin_socket_getsockname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  966 | `{` |
|      30 |  967 | `	return SockName(pCtx,nArg,apArg,0);` |
|       2 |  968 | `}` |
|       8 |  969 | `static int vm_builtin_socket_getpeername(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  970 | `{` |
|      10 |  971 | `	return SockName(pCtx,nArg,apArg,1);` |
|       2 |  972 | `}` |
|       - |  973 | `/* bool socket_set_nonblock(Socket) / socket_set_block(Socket) */` |
|       4 |  974 | `static int SockSetBlocking(ph7_context *pCtx,ph7_value **apArg,int bBlocking)` |
|       1 |  975 | `{` |
|       - |  976 | `	phl_socket *pSock;` |
|       - |  977 | `	int rc;` |
|       5 |  978 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|       5 |  979 | `	if( pSock == 0 ){` |
|     ! 0 |  980 | `		return rc;` |
|       - |  981 | `	}` |
|       - |  982 | `#ifdef __WINNT__` |
|       - |  983 | `	{` |
|       1 |  984 | `		u_long iMode = bBlocking ? 0 : 1;` |
|       1 |  985 | `		if( ioctlsocket(pSock->sock,FIONBIO,&iMode) != 0 ){` |
|     ! 0 |  986 | `			SockFailLast(pCtx,pSock,bBlocking ? "unable to set blocking mode"` |
|       - |  987 | `			                                  : "unable to set nonblocking mode");` |
|     ! 0 |  988 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 |  989 | `			return PH7_OK;` |
|       - |  990 | `		}` |
|       - |  991 | `	}` |
|       - |  992 | `#else` |
|       - |  993 | `	{` |
|       4 |  994 | `		int iFlags = fcntl(pSock->sock,F_GETFL);` |
|       4 |  995 | `		if( iFlags < 0 ){` |
|     ! 0 |  996 | `			SockFailLast(pCtx,pSock,bBlocking ? "unable to set blocking mode"` |
|       - |  997 | `			                                  : "unable to set nonblocking mode");` |
|     ! 0 |  998 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 |  999 | `			return PH7_OK;` |
|       - | 1000 | `		}` |
|       4 | 1001 | `		if( bBlocking ){` |
|       2 | 1002 | `			iFlags &= ~O_NONBLOCK;` |
|       1 | 1003 | `		}else{` |
|       2 | 1004 | `			iFlags \|= O_NONBLOCK;` |
|       - | 1005 | `		}` |
|       4 | 1006 | `		if( fcntl(pSock->sock,F_SETFL,iFlags) < 0 ){` |
|     ! 0 | 1007 | `			SockFailLast(pCtx,pSock,bBlocking ? "unable to set blocking mode"` |
|       - | 1008 | `			                                  : "unable to set nonblocking mode");` |
|     ! 0 | 1009 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 1010 | `			return PH7_OK;` |
|       - | 1011 | `		}` |
|       - | 1012 | `	}` |
|       - | 1013 | `#endif` |
|       5 | 1014 | `	pSock->bBlocking = bBlocking;` |
|       5 | 1015 | `	ph7_result_bool(pCtx,1);` |
|       5 | 1016 | `	return PH7_OK;` |
|       3 | 1017 | `}` |
|       2 | 1018 | `static int vm_builtin_socket_set_nonblock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1019 | `{` |
|       1 | 1020 | `	SXUNUSED(nArg);` |
|       3 | 1021 | `	return SockSetBlocking(pCtx,apArg,0);` |
|       1 | 1022 | `}` |
|       2 | 1023 | `static int vm_builtin_socket_set_block(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1024 | `{` |
|       1 | 1025 | `	SXUNUSED(nArg);` |
|       3 | 1026 | `	return SockSetBlocking(pCtx,apArg,1);` |
|       1 | 1027 | `}` |
|       - | 1028 | `/* bool socket_shutdown(Socket $socket, int $mode = 2) */` |
|       8 | 1029 | `static int vm_builtin_socket_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1030 | `{` |
|       - | 1031 | `	phl_socket *pSock;` |
|      10 | 1032 | `	ph7_int64 iMode = 2;` |
|       - | 1033 | `	int rc;` |
|      10 | 1034 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      10 | 1035 | `	if( pSock == 0 ){` |
|       3 | 1036 | `		return rc;` |
|       - | 1037 | `	}` |
|       8 | 1038 | `	if( nArg > 1 ){` |
|       8 | 1039 | `		iMode = ph7_value_to_int64(apArg[1]);` |
|       3 | 1040 | `	}` |
|       8 | 1041 | `	if( iMode < 0 \|\| iMode > 2 ){` |
|       - | 1042 | `		/* php's own wording, with no serial comma before the last name. */` |
|       7 | 1043 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1044 | `			"%z(): Argument #2 ($mode) must be one of SHUT_RD, SHUT_WR or SHUT_RDWR",` |
|       4 | 1045 | `			&pCtx->pFunc->sName);` |
|       - | 1046 | `	}` |
|       3 | 1047 | `	if( shutdown(pSock->sock,(int)iMode) != 0 ){` |
|     ! 0 | 1048 | `		SockFailLast(pCtx,pSock,"Unable to shutdown socket");` |
|     ! 0 | 1049 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1050 | `		return PH7_OK;` |
|       - | 1051 | `	}` |
|       3 | 1052 | `	ph7_result_bool(pCtx,1);` |
|       3 | 1053 | `	return PH7_OK;` |
|       6 | 1054 | `}` |
|       - | 1055 | `#ifndef __WINNT__` |
|       - | 1056 | `/*` |
|       - | 1057 | ` * bool socket_atmark(Socket $socket)` |
|       - | 1058 | ` *` |
|       - | 1059 | ` * The one function of this extension php does NOT build for Windows -- its` |
|       - | 1060 | `` * `function_exists('socket_atmark')` is false there -- so this one is absent`` |
|       - | 1061 | ` * with it rather than emulated over ioctlsocket(SIOCATMARK).` |
|       - | 1062 | ` */` |
|       4 | 1063 | `static int vm_builtin_socket_atmark(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1064 | `{` |
|       - | 1065 | `	phl_socket *pSock;` |
|       - | 1066 | `	int rc,iMark;` |
|       2 | 1067 | `	SXUNUSED(nArg);` |
|       4 | 1068 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|       4 | 1069 | `	if( pSock == 0 ){` |
|       2 | 1070 | `		return rc;` |
|       - | 1071 | `	}` |
|       2 | 1072 | `	iMark = sockatmark(pSock->sock);` |
|       2 | 1073 | `	if( iMark < 0 ){` |
|     ! 0 | 1074 | `		SockFailLast(pCtx,pSock,"Unable to apply sockmark");` |
|     ! 0 | 1075 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1076 | `		return PH7_OK;` |
|       - | 1077 | `	}` |
|       2 | 1078 | `	ph7_result_bool(pCtx,iMark != 0);` |
|       2 | 1079 | `	return PH7_OK;` |
|       2 | 1080 | `}` |
|       - | 1081 | `#endif /* !__WINNT__ */` |
|       - | 1082 |  |
|       - | 1083 | `/* ------------------------------------------------------------------------` |
|       - | 1084 | ` * Reading and writing` |
|       - | 1085 | ` * ------------------------------------------------------------------------ */` |
|       - | 1086 | `/*` |
|       - | 1087 | ` * php's line reader (PHP_NORMAL_READ). It takes ONE BYTE at a time and stops` |
|       - | 1088 | `` * after the byte it just stored is `\n` or `\r`, so the terminator is part of`` |
|       - | 1089 | `` * the answer -- a `fgets()` with no buffering under it. A peer that has closed`` |
|       - | 1090 | ` * makes recv() answer 0 for ever, and php spins 200 times before calling that` |
|       - | 1091 | ` * ECONNRESET; a NON-blocking socket with nothing to read gives up after two` |
|       - | 1092 | ` * empty passes and answers what it has, which for the first pass is "".` |
|       - | 1093 | ` */` |
|      10 | 1094 | `static int SockReadLine(phl_socket *pSock,char *zBuf,int nMax,int *pErr)` |
|       1 | 1095 | `{` |
|      11 | 1096 | `	int n = 0,nEmpty = 0,bBlocking = pSock->bBlocking;` |
|       - | 1097 | `#if !defined(__WINNT__)` |
|       - | 1098 | `	/* php asks the DESCRIPTOR rather than its own flag, so a socket some other` |
|       - | 1099 | `	 * door made non-blocking (an exported stream's stream_set_blocking()) is` |
|       - | 1100 | `	 * seen as such here too. Winsock cannot be asked, and php keeps the flag` |
|       - | 1101 | `	 * there for exactly that reason. */` |
|       - | 1102 | `	{` |
|      10 | 1103 | `		int iFlags = fcntl(pSock->sock,F_GETFL);` |
|      10 | 1104 | `		if( iFlags >= 0 ){` |
|      10 | 1105 | `			bBlocking = (iFlags & O_NONBLOCK) == 0;` |
|       5 | 1106 | `		}` |
|       - | 1107 | `	}` |
|       - | 1108 | `#endif` |
|      11 | 1109 | `	*pErr = 0;` |
|      41 | 1110 | `	while( n < nMax ){` |
|      37 | 1111 | `		int iRead = (int)recv(pSock->sock,zBuf + n,1,0);` |
|      37 | 1112 | `		if( iRead == 1 ){` |
|      37 | 1113 | `			char c = zBuf[n];` |
|      37 | 1114 | `			n++;` |
|      37 | 1115 | `			if( c == '\n' \|\| c == '\r' ){` |
|       4 | 1116 | `				break;` |
|       - | 1117 | `			}` |
|      31 | 1118 | `			nEmpty = 0;` |
|      31 | 1119 | `			continue;` |
|       - | 1120 | `		}` |
|     ! 0 | 1121 | `		if( iRead == 0 ){` |
|     ! 0 | 1122 | `			nEmpty++;` |
|     ! 0 | 1123 | `			if( !bBlocking && nEmpty >= 2 ){` |
|     ! 0 | 1124 | `				break;` |
|       - | 1125 | `			}` |
|     ! 0 | 1126 | `			if( nEmpty > 200 ){` |
|       - | 1127 | `#ifdef __WINNT__` |
|     ! 0 | 1128 | `				*pErr = WSAECONNRESET;` |
|       - | 1129 | `#else` |
|     ! 0 | 1130 | `				*pErr = ECONNRESET;` |
|       - | 1131 | `#endif` |
|     ! 0 | 1132 | `				return -1;` |
|       - | 1133 | `			}` |
|     ! 0 | 1134 | `			continue;` |
|       - | 1135 | `		}` |
|     ! 0 | 1136 | `		if( PH7_NetWouldBlock() ){` |
|       - | 1137 | `			/* Where php SPINS: its counter only advances on a 0-length read and` |
|       - | 1138 | `			 * a would-block answers -1, so its loop never leaves. Answering` |
|       - | 1139 | `			 * what has been read is what that dead guard was written to do --` |
|       - | 1140 | `			 * a recorded, deliberate divergence (PLAN.md 2.1). */` |
|     ! 0 | 1141 | `			break;` |
|       - | 1142 | `		}` |
|     ! 0 | 1143 | `		*pErr = PH7_NetLastError();` |
|     ! 0 | 1144 | `		return -1;` |
|     ! 0 | 1145 | `	}` |
|      11 | 1146 | `	return n;` |
|       6 | 1147 | `}` |
|       - | 1148 | `/* string\|false socket_read(Socket $socket, int $length, int $mode = PHP_BINARY_READ) */` |
|      30 | 1149 | `static int vm_builtin_socket_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1150 | `{` |
|       - | 1151 | `	phl_socket *pSock;` |
|       - | 1152 | `	ph7_int64 iLen;` |
|       - | 1153 | `	char *zBuf;` |
|      33 | 1154 | `	int rc,iRead,iMode = 2;` |
|      33 | 1155 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      33 | 1156 | `	if( pSock == 0 ){` |
|       3 | 1157 | `		return rc;` |
|       - | 1158 | `	}` |
|      31 | 1159 | `	iLen = ph7_value_to_int64(apArg[1]);` |
|      31 | 1160 | `	if( nArg > 2 ){` |
|      11 | 1161 | `		iMode = (int)ph7_value_to_int64(apArg[2]);` |
|       5 | 1162 | `	}` |
|       - | 1163 | `	/* php's own overflow screen, which is also what refuses 0 and -1. */` |
|      31 | 1164 | `	if( iLen + 1 < 2 ){` |
|       5 | 1165 | `		ph7_result_bool(pCtx,0);` |
|       5 | 1166 | `		return PH7_OK;` |
|       - | 1167 | `	}` |
|      26 | 1168 | `	if( iLen > PHL_SOCK_MAXBUF ){` |
|     ! 0 | 1169 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1170 | `	}` |
|      26 | 1171 | `	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(iLen + 1),TRUE,FALSE);` |
|      26 | 1172 | `	if( zBuf == 0 ){` |
|     ! 0 | 1173 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1174 | `	}` |
|      26 | 1175 | `	if( iMode == 1 ){` |
|      11 | 1176 | `		int iErr = 0;` |
|      11 | 1177 | `		iRead = SockReadLine(pSock,zBuf,(int)iLen,&iErr);` |
|      11 | 1178 | `		if( iRead < 0 ){` |
|       - | 1179 | `			/* Lower case, unlike socket_recv()'s: php words the two apart. */` |
|     ! 0 | 1180 | `			SockFail(pCtx,pSock,"unable to read from socket",iErr);` |
|     ! 0 | 1181 | `		}` |
|       6 | 1182 | `	}else{` |
|      16 | 1183 | `		iRead = (int)recv(pSock->sock,zBuf,(phl_sock_size)iLen,0);` |
|      16 | 1184 | `		if( iRead < 0 ){` |
|     ! 0 | 1185 | `			SockFailLast(pCtx,pSock,"unable to read from socket");` |
|     ! 0 | 1186 | `		}` |
|       - | 1187 | `	}` |
|      26 | 1188 | `	if( iRead < 0 ){` |
|     ! 0 | 1189 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1190 | `	}else{` |
|      26 | 1191 | `		ph7_result_string(pCtx,zBuf,iRead);` |
|       - | 1192 | `	}` |
|      26 | 1193 | `	ph7_context_free_chunk(pCtx,zBuf);` |
|      26 | 1194 | `	return PH7_OK;` |
|      18 | 1195 | `}` |
|       - | 1196 | `/* int\|false socket_write(Socket $socket, string $data, ?int $length = null) */` |
|      22 | 1197 | `static int vm_builtin_socket_write(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1198 | `{` |
|       - | 1199 | `	phl_socket *pSock;` |
|       - | 1200 | `	const char *zData;` |
|      25 | 1201 | `	int nData = 0,rc,iSent;` |
|       - | 1202 | `	ph7_int64 iLen;` |
|      25 | 1203 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      25 | 1204 | `	if( pSock == 0 ){` |
|       3 | 1205 | `		return rc;` |
|       - | 1206 | `	}` |
|      23 | 1207 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|      23 | 1208 | `	iLen = nData;` |
|      23 | 1209 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|       8 | 1210 | `		iLen = ph7_value_to_int64(apArg[2]);` |
|       8 | 1211 | `		if( iLen < 0 ){` |
|       4 | 1212 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1213 | `				"%z(): Argument #3 ($length) must be greater than or equal to 0",` |
|       2 | 1214 | `				&pCtx->pFunc->sName);` |
|       - | 1215 | `		}` |
|       5 | 1216 | `		if( iLen > nData ){` |
|     ! 0 | 1217 | `			iLen = nData;` |
|     ! 0 | 1218 | `		}` |
|       2 | 1219 | `	}` |
|      20 | 1220 | `	iSent = (int)send(pSock->sock,zData,(phl_sock_size)iLen,0);` |
|      20 | 1221 | `	if( iSent < 0 ){` |
|       3 | 1222 | `		SockFailLast(pCtx,pSock,"unable to write to socket");` |
|       3 | 1223 | `		ph7_result_bool(pCtx,0);` |
|       3 | 1224 | `		return PH7_OK;` |
|       - | 1225 | `	}` |
|      18 | 1226 | `	ph7_result_int64(pCtx,iSent);` |
|      18 | 1227 | `	return PH7_OK;` |
|      14 | 1228 | `}` |
|       - | 1229 | `/* int\|false socket_send(Socket $socket, string $data, int $length, int $flags) */` |
|       6 | 1230 | `static int vm_builtin_socket_send(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1231 | `{` |
|       - | 1232 | `	phl_socket *pSock;` |
|       - | 1233 | `	const char *zData;` |
|       8 | 1234 | `	int nData = 0,rc,iSent;` |
|       - | 1235 | `	ph7_int64 iLen;` |
|       3 | 1236 | `	SXUNUSED(nArg);` |
|       8 | 1237 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|       8 | 1238 | `	if( pSock == 0 ){` |
|     ! 0 | 1239 | `		return rc;` |
|       - | 1240 | `	}` |
|       8 | 1241 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|       8 | 1242 | `	iLen = ph7_value_to_int64(apArg[2]);` |
|       8 | 1243 | `	if( iLen < 0 ){` |
|       4 | 1244 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1245 | `			"%z(): Argument #3 ($length) must be greater than or equal to 0",` |
|       2 | 1246 | `			&pCtx->pFunc->sName);` |
|       - | 1247 | `	}` |
|       5 | 1248 | `	if( iLen > nData ){` |
|       3 | 1249 | `		iLen = nData;` |
|       1 | 1250 | `	}` |
|       5 | 1251 | `	iSent = (int)send(pSock->sock,zData,(phl_sock_size)iLen,(int)ph7_value_to_int64(apArg[3]));` |
|       5 | 1252 | `	if( iSent < 0 ){` |
|     ! 0 | 1253 | `		SockFailLast(pCtx,pSock,"Unable to write to socket");` |
|     ! 0 | 1254 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1255 | `		return PH7_OK;` |
|       - | 1256 | `	}` |
|       5 | 1257 | `	ph7_result_int64(pCtx,iSent);` |
|       5 | 1258 | `	return PH7_OK;` |
|       5 | 1259 | `}` |
|       - | 1260 | `/* int\|false socket_recv(Socket $socket, &$data, int $length, int $flags) */` |
|      12 | 1261 | `static int vm_builtin_socket_recv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1262 | `{` |
|       - | 1263 | `	phl_socket *pSock;` |
|       - | 1264 | `	ph7_value *pOut;` |
|       - | 1265 | `	ph7_int64 iLen;` |
|       - | 1266 | `	char *zBuf;` |
|       - | 1267 | `	int rc,iRead;` |
|       6 | 1268 | `	SXUNUSED(nArg);` |
|      14 | 1269 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      14 | 1270 | `	if( pSock == 0 ){` |
|     ! 0 | 1271 | `		return rc;` |
|       - | 1272 | `	}` |
|      14 | 1273 | `	iLen = ph7_value_to_int64(apArg[2]);` |
|      14 | 1274 | `	if( iLen + 1 < 2 ){` |
|       3 | 1275 | `		ph7_result_bool(pCtx,0);` |
|       3 | 1276 | `		return PH7_OK;` |
|       - | 1277 | `	}` |
|      11 | 1278 | `	if( iLen > PHL_SOCK_MAXBUF ){` |
|     ! 0 | 1279 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1280 | `	}` |
|      11 | 1281 | `	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(iLen + 1),TRUE,FALSE);` |
|      11 | 1282 | `	pOut = ph7_context_new_scalar(pCtx);` |
|      11 | 1283 | `	if( zBuf == 0 \|\| pOut == 0 ){` |
|     ! 0 | 1284 | `		if( zBuf ){` |
|     ! 0 | 1285 | `			ph7_context_free_chunk(pCtx,zBuf);` |
|     ! 0 | 1286 | `		}` |
|     ! 0 | 1287 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1288 | `	}` |
|      11 | 1289 | `	iRead = (int)recv(pSock->sock,zBuf,(phl_sock_size)iLen,(int)ph7_value_to_int64(apArg[3]));` |
|      11 | 1290 | `	if( iRead < 0 ){` |
|       3 | 1291 | `		SockFailLast(pCtx,pSock,"Unable to read from socket");` |
|       1 | 1292 | `	}` |
|       - | 1293 | `	/* php answers NULL rather than "" for both the failure and the orderly` |
|       - | 1294 | `	 * end of stream; only a non-empty read reaches the buffer. */` |
|      11 | 1295 | `	if( iRead > 0 ){` |
|       7 | 1296 | `		ph7_value_string(pOut,zBuf,iRead);` |
|       4 | 1297 | `	}else{` |
|       5 | 1298 | `		ph7_value_null(pOut);` |
|       - | 1299 | `	}` |
|      11 | 1300 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOut);` |
|      11 | 1301 | `	ph7_context_free_chunk(pCtx,zBuf);` |
|      11 | 1302 | `	if( iRead < 0 ){` |
|       3 | 1303 | `		ph7_result_bool(pCtx,0);` |
|       2 | 1304 | `	}else{` |
|       9 | 1305 | `		ph7_result_int64(pCtx,iRead);` |
|       - | 1306 | `	}` |
|      11 | 1307 | `	return PH7_OK;` |
|       8 | 1308 | `}` |
|       - | 1309 | `/*` |
|       - | 1310 | ` * int\|false socket_sendto(Socket $socket, string $data, int $length, int $flags,` |
|       - | 1311 | ` *                         string $address, ?int $port = null)` |
|       - | 1312 | ` */` |
|       8 | 1313 | `static int vm_builtin_socket_sendto(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1314 | `{` |
|       - | 1315 | `	struct sockaddr_storage sAddr;` |
|       - | 1316 | `	phl_socket *pSock;` |
|       - | 1317 | `	const char *zData,*zAddr;` |
|      10 | 1318 | `	int nData = 0,nAddr = 0,rc,iSent,nLen,bHasPort;` |
|       - | 1319 | `	ph7_int64 iLen;` |
|      10 | 1320 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      10 | 1321 | `	if( pSock == 0 ){` |
|     ! 0 | 1322 | `		return rc;` |
|       - | 1323 | `	}` |
|      10 | 1324 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|      10 | 1325 | `	iLen = ph7_value_to_int64(apArg[2]);` |
|      10 | 1326 | `	if( iLen < 0 ){` |
|       4 | 1327 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1328 | `			"%z(): Argument #3 ($length) must be greater than or equal to 0",` |
|       2 | 1329 | `			&pCtx->pFunc->sName);` |
|       - | 1330 | `	}` |
|       8 | 1331 | `	if( iLen > nData ){` |
|     ! 0 | 1332 | `		iLen = nData;` |
|     ! 0 | 1333 | `	}` |
|       8 | 1334 | `	zAddr = ph7_value_to_string(apArg[4],&nAddr);` |
|       8 | 1335 | `	bHasPort = (nArg > 5 && !ph7_value_is_null(apArg[5]));` |
|      13 | 1336 | `	nLen = SockBuildAddr(pCtx,pSock,zAddr,nAddr,bHasPort,` |
|       5 | 1337 | `		bHasPort ? ph7_value_to_int64(apArg[5]) : 0,6,"port",&sAddr,&rc);` |
|       8 | 1338 | `	if( nLen <= 0 ){` |
|       6 | 1339 | `		if( nLen < 0 ){` |
|       3 | 1340 | `			return rc;` |
|       - | 1341 | `		}` |
|       3 | 1342 | `		ph7_result_bool(pCtx,0);` |
|       3 | 1343 | `		return PH7_OK;` |
|       - | 1344 | `	}` |
|       4 | 1345 | `	iSent = (int)sendto(pSock->sock,zData,(phl_sock_size)iLen,(int)ph7_value_to_int64(apArg[3]),` |
|       1 | 1346 | `		(struct sockaddr *)&sAddr,(ph7_socklen)nLen);` |
|       3 | 1347 | `	if( iSent < 0 ){` |
|     ! 0 | 1348 | `		SockFailLast(pCtx,pSock,"Unable to write to socket");` |
|     ! 0 | 1349 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1350 | `		return PH7_OK;` |
|       - | 1351 | `	}` |
|       3 | 1352 | `	ph7_result_int64(pCtx,iSent);` |
|       3 | 1353 | `	return PH7_OK;` |
|       6 | 1354 | `}` |
|       - | 1355 | `/*` |
|       - | 1356 | ` * int\|false socket_recvfrom(Socket $socket, &$data, int $length, int $flags,` |
|       - | 1357 | ` *                          &$address, &$port = null)` |
|       - | 1358 | ` */` |
|       2 | 1359 | `static int vm_builtin_socket_recvfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1360 | `{` |
|       - | 1361 | `	struct sockaddr_storage sAddr;` |
|       3 | 1362 | `	ph7_socklen nAddr = (ph7_socklen)sizeof(sAddr);` |
|       - | 1363 | `	phl_socket *pSock;` |
|       - | 1364 | `	ph7_value *pOut;` |
|       - | 1365 | `	ph7_int64 iLen;` |
|       - | 1366 | `	char *zBuf;` |
|       - | 1367 | `	int rc,iRead;` |
|       3 | 1368 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|       3 | 1369 | `	if( pSock == 0 ){` |
|     ! 0 | 1370 | `		return rc;` |
|       - | 1371 | `	}` |
|       3 | 1372 | `	iLen = ph7_value_to_int64(apArg[2]);` |
|       3 | 1373 | `	if( iLen + 1 < 2 ){` |
|     ! 0 | 1374 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1375 | `		return PH7_OK;` |
|       - | 1376 | `	}` |
|       3 | 1377 | `	if( iLen > PHL_SOCK_MAXBUF ){` |
|     ! 0 | 1378 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1379 | `	}` |
|       3 | 1380 | `	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(iLen + 1),TRUE,FALSE);` |
|       3 | 1381 | `	pOut = ph7_context_new_scalar(pCtx);` |
|       3 | 1382 | `	if( zBuf == 0 \|\| pOut == 0 ){` |
|     ! 0 | 1383 | `		if( zBuf ){` |
|     ! 0 | 1384 | `			ph7_context_free_chunk(pCtx,zBuf);` |
|     ! 0 | 1385 | `		}` |
|     ! 0 | 1386 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1387 | `	}` |
|       3 | 1388 | `	SyZero(&sAddr,sizeof(sAddr));` |
|       3 | 1389 | `	iRead = (int)recvfrom(pSock->sock,zBuf,(phl_sock_size)iLen,(int)ph7_value_to_int64(apArg[3]),` |
|       - | 1390 | `		(struct sockaddr *)&sAddr,&nAddr);` |
|       3 | 1391 | `	if( iRead < 0 ){` |
|     ! 0 | 1392 | `		SockFailLast(pCtx,pSock,"Unable to recvfrom");` |
|     ! 0 | 1393 | `		ph7_context_free_chunk(pCtx,zBuf);` |
|     ! 0 | 1394 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1395 | `		return PH7_OK;` |
|       - | 1396 | `	}` |
|       3 | 1397 | `	ph7_value_string(pOut,zBuf,iRead);` |
|       3 | 1398 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOut);` |
|       3 | 1399 | `	ph7_context_free_chunk(pCtx,zBuf);` |
|       - | 1400 | `	/* An AF_UNIX datagram from an unbound sender carries no path at all, which` |
|       - | 1401 | `	 * is the empty string php reports rather than a missing argument. */` |
|       3 | 1402 | `	if( sAddr.ss_family == 0 ){` |
|     ! 0 | 1403 | `		sAddr.ss_family = (unsigned short)pSock->iDomain;` |
|     ! 0 | 1404 | `	}` |
|       3 | 1405 | `	SockReportAddr(pCtx,(const struct sockaddr *)&sAddr,apArg[4],nArg > 5 ? apArg[5] : 0);` |
|       3 | 1406 | `	ph7_result_int64(pCtx,iRead);` |
|       3 | 1407 | `	return PH7_OK;` |
|       2 | 1408 | `}` |
|       - | 1409 |  |
|       - | 1410 | `/* ------------------------------------------------------------------------` |
|       - | 1411 | ` * Socket options` |
|       - | 1412 | ` * ------------------------------------------------------------------------ */` |
|       - | 1413 | `/*` |
|       - | 1414 | ` * The missing-key refusal, in php's TWO wordings: the option php reads with its` |
|       - | 1415 | ` * own struct converter names the ARGUMENT, and the multicast group reader --` |
|       - | 1416 | ` * which is shared with nothing -- names only the key.` |
|       - | 1417 | ` */` |
|      16 | 1418 | `static ph7_value * SockOptKey(ph7_context *pCtx,ph7_value *pArr,const char *zKey,int *pRc)` |
|       2 | 1419 | `{` |
|      18 | 1420 | `	ph7_value *pVal = ph7_array_fetch(pArr,zKey,(int)SyStrlen(zKey));` |
|      18 | 1421 | `	if( pVal == 0 ){` |
|       9 | 1422 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1423 | `			"%z(): Argument #4 ($value) must have key \"%s\"",` |
|       4 | 1424 | `			&pCtx->pFunc->sName,zKey);` |
|       5 | 1425 | `		return 0;` |
|       - | 1426 | `	}` |
|      14 | 1427 | `	*pRc = PH7_OK;` |
|      14 | 1428 | `	return pVal;` |
|      10 | 1429 | `}` |
|     ! 0 | 1430 | `static ph7_value * SockOptvalKey(ph7_context *pCtx,ph7_value *pArr,const char *zKey,int *pRc)` |
|       - | 1431 | `{` |
|     ! 0 | 1432 | `	ph7_value *pVal = ph7_array_fetch(pArr,zKey,(int)SyStrlen(zKey));` |
|     ! 0 | 1433 | `	if( pVal == 0 ){` |
|     ! 0 | 1434 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|     ! 0 | 1435 | `			"No key \"%s\" passed in optval",zKey);` |
|     ! 0 | 1436 | `		return 0;` |
|       - | 1437 | `	}` |
|     ! 0 | 1438 | `	*pRc = PH7_OK;` |
|     ! 0 | 1439 | `	return pVal;` |
|     ! 0 | 1440 | `}` |
|       - | 1441 | `#if defined(IP_MULTICAST_IF) && !defined(__WINNT__)` |
|       - | 1442 | `/*` |
|       - | 1443 | `` * php's interface naming, both ways. A `$value` that is a NUMBER is already an`` |
|       - | 1444 | ` * index; a string is an interface NAME. IPv4's IP_MULTICAST_IF is stated as an` |
|       - | 1445 | ` * ADDRESS on the wire rather than an index, so the two have to be translated` |
|       - | 1446 | ` * into each other -- index 0 is INADDR_ANY, "the default route", and anything` |
|       - | 1447 | ` * else is that interface's own address.` |
|       - | 1448 | ` */` |
|       - | 1449 | `/*` |
|       - | 1450 | ` * php's diagnostic for an interface NAME nothing answers to. A numeric index is` |
|       - | 1451 | ` * never refused here -- it goes to the kernel, which reports ENODEV through the` |
|       - | 1452 | ` * ordinary "Unable to set socket option".` |
|       - | 1453 | ` */` |
|     ! 0 | 1454 | `static void SockNoSuchInterface(ph7_context *pCtx,ph7_value *pVal)` |
|       - | 1455 | `{` |
|     ! 0 | 1456 | `	int n = 0;` |
|     ! 0 | 1457 | `	const char *z = ph7_value_to_string(pVal,&n);` |
|     ! 0 | 1458 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 1459 | `		"No interface with name \"%.*s\" could be found",n,z ? z : "");` |
|     ! 0 | 1460 | `}` |
|     ! 0 | 1461 | `static int SockIfIndexOf(ph7_context *pCtx,ph7_value *pVal,unsigned int *pOut)` |
|       - | 1462 | `{` |
|     ! 0 | 1463 | `	if( ph7_value_is_int(pVal) ){` |
|     ! 0 | 1464 | `		ph7_int64 i = ph7_value_to_int64(pVal);` |
|     ! 0 | 1465 | `		if( i < 0 \|\| i > 0xFFFFFFFF ){` |
|     ! 0 | 1466 | `			return -1;` |
|       - | 1467 | `		}` |
|     ! 0 | 1468 | `		*pOut = (unsigned int)i;` |
|     ! 0 | 1469 | `		return 0;` |
|       - | 1470 | `	}` |
|       - | 1471 | `	{` |
|     ! 0 | 1472 | `		int n = 0;` |
|     ! 0 | 1473 | `		const char *z = ph7_value_to_string(pVal,&n);` |
|       - | 1474 | `		char zName[IF_NAMESIZE+1];` |
|     ! 0 | 1475 | `		if( n < 1 \|\| n >= (int)sizeof(zName) ){` |
|     ! 0 | 1476 | `			return -1;` |
|       - | 1477 | `		}` |
|     ! 0 | 1478 | `		SyMemcpy(z,zName,(sxu32)n);` |
|     ! 0 | 1479 | `		zName[n] = 0;` |
|     ! 0 | 1480 | `		*pOut = if_nametoindex(zName);` |
|     ! 0 | 1481 | `		if( *pOut == 0 ){` |
|     ! 0 | 1482 | `			return -1;` |
|       - | 1483 | `		}` |
|       - | 1484 | `	}` |
|     ! 0 | 1485 | `	SXUNUSED(pCtx);` |
|     ! 0 | 1486 | `	return 0;` |
|     ! 0 | 1487 | `}` |
|     ! 0 | 1488 | `static int SockIfIndexToAddr4(unsigned int iIndex,struct in_addr *pOut)` |
|       - | 1489 | `{` |
|       - | 1490 | `	struct ifreq sReq;` |
|       - | 1491 | `	int fd;` |
|     ! 0 | 1492 | `	if( iIndex == 0 ){` |
|     ! 0 | 1493 | `		pOut->s_addr = htonl(INADDR_ANY);` |
|     ! 0 | 1494 | `		return 0;` |
|       - | 1495 | `	}` |
|     ! 0 | 1496 | `	SyZero(&sReq,sizeof(sReq));` |
|     ! 0 | 1497 | `	if( if_indextoname(iIndex,sReq.ifr_name) == 0 ){` |
|     ! 0 | 1498 | `		return -1;` |
|       - | 1499 | `	}` |
|     ! 0 | 1500 | `	fd = socket(AF_INET,SOCK_DGRAM,0);` |
|     ! 0 | 1501 | `	if( fd < 0 ){` |
|     ! 0 | 1502 | `		return -1;` |
|       - | 1503 | `	}` |
|     ! 0 | 1504 | `	if( ioctl(fd,SIOCGIFADDR,&sReq) != 0 ){` |
|     ! 0 | 1505 | `		close(fd);` |
|     ! 0 | 1506 | `		return -1;` |
|       - | 1507 | `	}` |
|     ! 0 | 1508 | `	close(fd);` |
|     ! 0 | 1509 | `	SyMemcpy((const void *)&((struct sockaddr_in *)&sReq.ifr_addr)->sin_addr,` |
|     ! 0 | 1510 | `		(void *)pOut,sizeof(struct in_addr));` |
|     ! 0 | 1511 | `	return 0;` |
|     ! 0 | 1512 | `}` |
|     ! 0 | 1513 | `static int SockAddr4ToIfIndex(const struct in_addr *pAddr,unsigned int *pOut)` |
|       - | 1514 | `{` |
|     ! 0 | 1515 | `	struct ifaddrs *pList = 0,*p;` |
|     ! 0 | 1516 | `	if( pAddr->s_addr == htonl(INADDR_ANY) ){` |
|     ! 0 | 1517 | `		*pOut = 0;` |
|     ! 0 | 1518 | `		return 0;` |
|       - | 1519 | `	}` |
|     ! 0 | 1520 | `	if( getifaddrs(&pList) != 0 ){` |
|     ! 0 | 1521 | `		return -1;` |
|       - | 1522 | `	}` |
|     ! 0 | 1523 | `	for( p = pList ; p ; p = p->ifa_next ){` |
|     ! 0 | 1524 | `		if( p->ifa_addr && p->ifa_addr->sa_family == AF_INET` |
|     ! 0 | 1525 | `		 && ((struct sockaddr_in *)p->ifa_addr)->sin_addr.s_addr == pAddr->s_addr ){` |
|     ! 0 | 1526 | `			*pOut = if_nametoindex(p->ifa_name);` |
|     ! 0 | 1527 | `			freeifaddrs(pList);` |
|     ! 0 | 1528 | `			return *pOut ? 0 : -1;` |
|       - | 1529 | `		}` |
|     ! 0 | 1530 | `	}` |
|     ! 0 | 1531 | `	freeifaddrs(pList);` |
|     ! 0 | 1532 | `	return -1;` |
|     ! 0 | 1533 | `}` |
|       - | 1534 | `/*` |
|       - | 1535 | ``  * The group-membership options. php takes `['group' => …, 'interface' => …]` `` |
|       - | 1536 | `` * (plus `'source'` for the four source-filtered ones) and hands the kernel a`` |
|       - | 1537 | `` * `group_req`/`group_source_req`. The ADDRESS is parsed the way every other`` |
|       - | 1538 | ` * address in this extension is -- by the SOCKET's family, name resolution and` |
|       - | 1539 | ` * all -- which is why a group that is not an address reports the -10000 host` |
|       - | 1540 | ` * lookup failure rather than a plain refusal.` |
|       - | 1541 | ` */` |
|     ! 0 | 1542 | `static int SockOptGroupAddr(ph7_context *pCtx,phl_socket *pSock,ph7_value *pArr,` |
|       - | 1543 | `	const char *zKey,struct sockaddr_storage *pOut,int *pRc)` |
|       - | 1544 | `{` |
|     ! 0 | 1545 | `	ph7_value *pVal = SockOptvalKey(pCtx,pArr,zKey,pRc);` |
|       - | 1546 | `	const char *z;` |
|     ! 0 | 1547 | `	int n = 0,nLen;` |
|     ! 0 | 1548 | `	if( pVal == 0 ){` |
|     ! 0 | 1549 | `		return -1;` |
|       - | 1550 | `	}` |
|     ! 0 | 1551 | `	z = ph7_value_to_string(pVal,&n);` |
|     ! 0 | 1552 | `	nLen = SockBuildAddr(pCtx,pSock,z,n,1,0,4,"value",pOut,pRc);` |
|     ! 0 | 1553 | `	if( nLen <= 0 ){` |
|     ! 0 | 1554 | `		if( nLen == 0 ){` |
|     ! 0 | 1555 | `			*pRc = PH7_OK;` |
|     ! 0 | 1556 | `		}` |
|     ! 0 | 1557 | `		return -1;` |
|       - | 1558 | `	}` |
|     ! 0 | 1559 | `	return 0;` |
|     ! 0 | 1560 | `}` |
|     ! 0 | 1561 | `static int SockSetMcastGroup(ph7_context *pCtx,phl_socket *pSock,int iLevel,int iOpt,` |
|       - | 1562 | `	ph7_value *pVal,int bSource,int *pRc)` |
|       - | 1563 | `{` |
|       - | 1564 | `	struct group_source_req sReq;` |
|       - | 1565 | `	ph7_value *pTmp;` |
|     ! 0 | 1566 | `	unsigned int iIf = 0;` |
|     ! 0 | 1567 | `	SyZero(&sReq,sizeof(sReq));` |
|     ! 0 | 1568 | `	if( SockOptGroupAddr(pCtx,pSock,pVal,"group",` |
|     ! 0 | 1569 | `		(struct sockaddr_storage *)&sReq.gsr_group,pRc) != 0 ){` |
|     ! 0 | 1570 | `		return -1;` |
|       - | 1571 | `	}` |
|     ! 0 | 1572 | `	pTmp = ph7_array_fetch(pVal,"interface",sizeof("interface")-1);` |
|     ! 0 | 1573 | `	if( pTmp && SockIfIndexOf(pCtx,pTmp,&iIf) != 0 ){` |
|     ! 0 | 1574 | `		*pRc = PH7_OK;` |
|     ! 0 | 1575 | `		SockNoSuchInterface(pCtx,pTmp);` |
|     ! 0 | 1576 | `		return -1;` |
|       - | 1577 | `	}` |
|     ! 0 | 1578 | `	sReq.gsr_interface = iIf;` |
|     ! 0 | 1579 | `	if( bSource ){` |
|     ! 0 | 1580 | `		if( SockOptGroupAddr(pCtx,pSock,pVal,"source",` |
|     ! 0 | 1581 | `			(struct sockaddr_storage *)&sReq.gsr_source,pRc) != 0 ){` |
|     ! 0 | 1582 | `			return -1;` |
|       - | 1583 | `		}` |
|     ! 0 | 1584 | `	}` |
|     ! 0 | 1585 | `	*pRc = PH7_OK;` |
|     ! 0 | 1586 | `	if( setsockopt(pSock->sock,iLevel,iOpt,(const char *)&sReq,` |
|     ! 0 | 1587 | `		bSource ? (ph7_socklen)sizeof(struct group_source_req)` |
|     ! 0 | 1588 | `		        : (ph7_socklen)sizeof(struct group_req)) != 0 ){` |
|     ! 0 | 1589 | `		SockFailLast(pCtx,pSock,"Unable to set socket option");` |
|     ! 0 | 1590 | `		return -1;` |
|       - | 1591 | `	}` |
|     ! 0 | 1592 | `	return 0;` |
|     ! 0 | 1593 | `}` |
|       - | 1594 | `#endif /* IP_MULTICAST_IF && !__WINNT__ */` |
|       - | 1595 |  |
|       - | 1596 | `/* bool socket_set_option(Socket $socket, int $level, int $option, $value) */` |
|      16 | 1597 | `static int vm_builtin_socket_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1598 | `{` |
|       - | 1599 | `	phl_socket *pSock;` |
|       - | 1600 | `	ph7_value *pVal;` |
|       - | 1601 | `	ph7_int64 iLevel,iOpt;` |
|       - | 1602 | `	int rc,iRet;` |
|       8 | 1603 | `	SXUNUSED(nArg);` |
|      18 | 1604 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      18 | 1605 | `	if( pSock == 0 ){` |
|     ! 0 | 1606 | `		return rc;` |
|       - | 1607 | `	}` |
|      18 | 1608 | `	iLevel = ph7_value_to_int64(apArg[1]);` |
|      18 | 1609 | `	iOpt   = ph7_value_to_int64(apArg[2]);` |
|      18 | 1610 | `	pVal   = apArg[3];` |
|       - | 1611 | `	{` |
|       - | 1612 | `		/*` |
|       - | 1613 | `		 * Every struct-valued option is screened for an ARRAY before anything` |
|       - | 1614 | `		 * else -- and php names the option with the LAST label of the switch` |
|       - | 1615 | ``		 * arm it shares, so `MCAST_JOIN_GROUP` reports itself as`` |
|       - | 1616 | ``		 * `MCAST_LEAVE_GROUP` and the four source-filtered ones all report`` |
|       - | 1617 | ``		 * `MCAST_LEAVE_SOURCE_GROUP`. That is php's own text, reproduced.`` |
|       - | 1618 | `		 */` |
|      18 | 1619 | `		const char *zOptName = 0;` |
|      18 | 1620 | `		if( iLevel == SOL_SOCKET ){` |
|      18 | 1621 | `			if( (int)iOpt == SO_LINGER ){ zOptName = "SO_LINGER"; }` |
|      12 | 1622 | `			else if( (int)iOpt == SO_RCVTIMEO ){ zOptName = "SO_RCVTIMEO"; }` |
|       5 | 1623 | `			else if( (int)iOpt == SO_SNDTIMEO ){ zOptName = "SO_SNDTIMEO"; }` |
|       8 | 1624 | `		}` |
|       - | 1625 | `#if defined(MCAST_JOIN_GROUP) && !defined(__WINNT__)` |
|     ! 0 | 1626 | `		else if( iLevel == IPPROTO_IP \|\| iLevel == IPPROTO_IPV6 ){` |
|     ! 0 | 1627 | `			switch( (int)iOpt ){` |
|     ! 0 | 1628 | `				case MCAST_JOIN_GROUP: case MCAST_LEAVE_GROUP:` |
|     ! 0 | 1629 | `					zOptName = "MCAST_LEAVE_GROUP"; break;` |
|     ! 0 | 1630 | `				case MCAST_BLOCK_SOURCE: case MCAST_UNBLOCK_SOURCE:` |
|       - | 1631 | `				case MCAST_JOIN_SOURCE_GROUP: case MCAST_LEAVE_SOURCE_GROUP:` |
|     ! 0 | 1632 | `					zOptName = "MCAST_LEAVE_SOURCE_GROUP"; break;` |
|     ! 0 | 1633 | `				default: break;` |
|       - | 1634 | `			}` |
|     ! 0 | 1635 | `		}` |
|       - | 1636 | `#endif` |
|      18 | 1637 | `		if( zOptName && !ph7_value_is_array(pVal) ){` |
|       - | 1638 | `			char zGiven[64];` |
|       9 | 1639 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1640 | `				"%z(): Argument #4 ($value) must be of type array when "` |
|       - | 1641 | `				"argument #3 ($option) is %s, %s given",` |
|       4 | 1642 | `				&pCtx->pFunc->sName,zOptName,` |
|       2 | 1643 | `				VmValueGivenName(pVal,zGiven,sizeof(zGiven)));` |
|       - | 1644 | `		}` |
|       - | 1645 | `	}` |
|       - | 1646 | `#if defined(IP_MULTICAST_IF) && !defined(__WINNT__)` |
|      12 | 1647 | `	if( iLevel == IPPROTO_IP \|\| iLevel == IPPROTO_IPV6 ){` |
|     ! 0 | 1648 | `		int bHandled = 1,bSource = 0,bGroup = 0;` |
|     ! 0 | 1649 | `		switch( (int)iOpt ){` |
|     ! 0 | 1650 | `			case MCAST_JOIN_GROUP: case MCAST_LEAVE_GROUP:` |
|     ! 0 | 1651 | `				bGroup = 1; break;` |
|     ! 0 | 1652 | `			case MCAST_BLOCK_SOURCE: case MCAST_UNBLOCK_SOURCE:` |
|       - | 1653 | `			case MCAST_JOIN_SOURCE_GROUP: case MCAST_LEAVE_SOURCE_GROUP:` |
|     ! 0 | 1654 | `				bGroup = bSource = 1; break;` |
|     ! 0 | 1655 | `			default: bHandled = 0; break;` |
|       - | 1656 | `		}` |
|     ! 0 | 1657 | `		if( bGroup ){` |
|     ! 0 | 1658 | `			if( SockSetMcastGroup(pCtx,pSock,(int)iLevel,(int)iOpt,pVal,bSource,&rc) != 0 ){` |
|     ! 0 | 1659 | `				if( rc != PH7_OK ){` |
|     ! 0 | 1660 | `					return rc;` |
|       - | 1661 | `				}` |
|     ! 0 | 1662 | `				ph7_result_bool(pCtx,0);` |
|     ! 0 | 1663 | `				return PH7_OK;` |
|       - | 1664 | `			}` |
|     ! 0 | 1665 | `			ph7_result_bool(pCtx,1);` |
|     ! 0 | 1666 | `			return PH7_OK;` |
|       - | 1667 | `		}` |
|     ! 0 | 1668 | `		if( iLevel == IPPROTO_IP && (int)iOpt == IP_MULTICAST_IF ){` |
|       - | 1669 | `			struct in_addr sIf;` |
|     ! 0 | 1670 | `			unsigned int iIdx = 0;` |
|     ! 0 | 1671 | `			if( SockIfIndexOf(pCtx,pVal,&iIdx) != 0` |
|     ! 0 | 1672 | `			 \|\| SockIfIndexToAddr4(iIdx,&sIf) != 0 ){` |
|     ! 0 | 1673 | `				SockNoSuchInterface(pCtx,pVal);` |
|     ! 0 | 1674 | `				ph7_result_bool(pCtx,0);` |
|     ! 0 | 1675 | `				return PH7_OK;` |
|       - | 1676 | `			}` |
|     ! 0 | 1677 | `			iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,` |
|       - | 1678 | `				(const char *)&sIf,(ph7_socklen)sizeof(sIf));` |
|     ! 0 | 1679 | `			goto done;` |
|       - | 1680 | `		}` |
|     ! 0 | 1681 | `		if( iLevel == IPPROTO_IPV6 && (int)iOpt == IPV6_MULTICAST_IF ){` |
|     ! 0 | 1682 | `			unsigned int iIdx = 0;` |
|     ! 0 | 1683 | `			if( SockIfIndexOf(pCtx,pVal,&iIdx) != 0 ){` |
|     ! 0 | 1684 | `				SockNoSuchInterface(pCtx,pVal);` |
|     ! 0 | 1685 | `				ph7_result_bool(pCtx,0);` |
|     ! 0 | 1686 | `				return PH7_OK;` |
|       - | 1687 | `			}` |
|     ! 0 | 1688 | `			iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,` |
|       - | 1689 | `				(const char *)&iIdx,(ph7_socklen)sizeof(iIdx));` |
|     ! 0 | 1690 | `			goto done;` |
|       - | 1691 | `		}` |
|     ! 0 | 1692 | `		if( iLevel == IPPROTO_IP` |
|     ! 0 | 1693 | `		 && ((int)iOpt == IP_MULTICAST_LOOP \|\| (int)iOpt == IP_MULTICAST_TTL) ){` |
|       - | 1694 | `			/* IPv4 states both of these in ONE BYTE; the v6 pair below are ints. */` |
|     ! 0 | 1695 | `			unsigned char c = (unsigned char)ph7_value_to_int64(pVal);` |
|     ! 0 | 1696 | `			iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,` |
|       - | 1697 | `				(const char *)&c,(ph7_socklen)sizeof(c));` |
|     ! 0 | 1698 | `			goto done;` |
|       - | 1699 | `		}` |
|     ! 0 | 1700 | `		SXUNUSED(bHandled);` |
|     ! 0 | 1701 | `	}` |
|       - | 1702 | `#endif` |
|      14 | 1703 | `	if( iLevel == SOL_SOCKET && (int)iOpt == SO_LINGER ){` |
|       - | 1704 | `		struct linger sLin;` |
|       - | 1705 | `		ph7_value *pOn,*pFor;` |
|       6 | 1706 | `		SyZero(&sLin,sizeof(sLin));` |
|       6 | 1707 | `		pOn = SockOptKey(pCtx,pVal,"l_onoff",&rc);` |
|       6 | 1708 | `		if( pOn == 0 ){` |
|     ! 0 | 1709 | `			return rc;` |
|       - | 1710 | `		}` |
|       6 | 1711 | `		pFor = SockOptKey(pCtx,pVal,"l_linger",&rc);` |
|       6 | 1712 | `		if( pFor == 0 ){` |
|       3 | 1713 | `			return rc;` |
|       - | 1714 | `		}` |
|       - | 1715 | `#ifdef __WINNT__` |
|       - | 1716 | `		/* Winsock states both halves in sixteen bits; POSIX states them in an int. */` |
|       1 | 1717 | `		sLin.l_onoff = (u_short)ph7_value_to_int64(pOn);` |
|       1 | 1718 | `		sLin.l_linger = (u_short)ph7_value_to_int64(pFor);` |
|       - | 1719 | `#else` |
|       2 | 1720 | `		sLin.l_onoff = (int)ph7_value_to_int64(pOn);` |
|       2 | 1721 | `		sLin.l_linger = (int)ph7_value_to_int64(pFor);` |
|       - | 1722 | `#endif` |
|       3 | 1723 | `		iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,` |
|       - | 1724 | `			(const char *)&sLin,(ph7_socklen)sizeof(sLin));` |
|       3 | 1725 | `		goto done;` |
|       - | 1726 | `	}` |
|      10 | 1727 | `	if( iLevel == SOL_SOCKET && ((int)iOpt == SO_RCVTIMEO \|\| (int)iOpt == SO_SNDTIMEO) ){` |
|       - | 1728 | `		struct timeval tv;` |
|       - | 1729 | `		ph7_value *pSec,*pUsec;` |
|       6 | 1730 | `		pSec = SockOptKey(pCtx,pVal,"sec",&rc);` |
|       6 | 1731 | `		if( pSec == 0 ){` |
|     ! 0 | 1732 | `			return rc;` |
|       - | 1733 | `		}` |
|       6 | 1734 | `		pUsec = SockOptKey(pCtx,pVal,"usec",&rc);` |
|       6 | 1735 | `		if( pUsec == 0 ){` |
|       3 | 1736 | `			return rc;` |
|       - | 1737 | `		}` |
|       3 | 1738 | `		tv.tv_sec = (long)ph7_value_to_int64(pSec);` |
|       3 | 1739 | `		tv.tv_usec = (long)ph7_value_to_int64(pUsec);` |
|       3 | 1740 | `		iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,` |
|       - | 1741 | `			(const char *)&tv,(ph7_socklen)sizeof(tv));` |
|       3 | 1742 | `		goto done;` |
|       - | 1743 | `	}` |
|       4 | 1744 | `	if( (iLevel == SOL_SOCKET && (int)iOpt == PHL_SO_BINDTODEVICE)` |
|       5 | 1745 | `	 \|\| (iLevel == IPPROTO_TCP && (int)iOpt == PHL_TCP_CONGESTION) ){` |
|       - | 1746 | `		/* The two options whose value is a STRING rather than a number. */` |
|     ! 0 | 1747 | `		int n = 0;` |
|     ! 0 | 1748 | `		const char *z = ph7_value_to_string(pVal,&n);` |
|     ! 0 | 1749 | `		iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,z,(ph7_socklen)n);` |
|     ! 0 | 1750 | `		goto done;` |
|       - | 1751 | `	}` |
|       - | 1752 | `	{` |
|       5 | 1753 | `		int iNum = (int)ph7_value_to_int64(pVal);` |
|       5 | 1754 | `		iRet = setsockopt(pSock->sock,(int)iLevel,(int)iOpt,` |
|       - | 1755 | `			(const char *)&iNum,(ph7_socklen)sizeof(iNum));` |
|       2 | 1756 | `	}` |
|       4 | 1757 | `done:` |
|       9 | 1758 | `	if( iRet != 0 ){` |
|     ! 0 | 1759 | `		SockFailLast(pCtx,pSock,"Unable to set socket option");` |
|     ! 0 | 1760 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1761 | `		return PH7_OK;` |
|       - | 1762 | `	}` |
|       9 | 1763 | `	ph7_result_bool(pCtx,1);` |
|       9 | 1764 | `	return PH7_OK;` |
|      10 | 1765 | `}` |
|       - | 1766 | `/* array\|int\|false socket_get_option(Socket $socket, int $level, int $option) */` |
|      18 | 1767 | `static int vm_builtin_socket_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1768 | `{` |
|       - | 1769 | `	phl_socket *pSock;` |
|       - | 1770 | `	ph7_int64 iLevel,iOpt;` |
|       - | 1771 | `	ph7_socklen nOpt;` |
|       - | 1772 | `	int rc;` |
|       9 | 1773 | `	SXUNUSED(nArg);` |
|      21 | 1774 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      21 | 1775 | `	if( pSock == 0 ){` |
|       3 | 1776 | `		return rc;` |
|       - | 1777 | `	}` |
|      18 | 1778 | `	iLevel = ph7_value_to_int64(apArg[1]);` |
|      18 | 1779 | `	iOpt   = ph7_value_to_int64(apArg[2]);` |
|       - | 1780 | `#if defined(IP_MULTICAST_IF) && !defined(__WINNT__)` |
|      16 | 1781 | `	if( iLevel == IPPROTO_IP && (int)iOpt == IP_MULTICAST_IF ){` |
|       - | 1782 | `		struct in_addr sIf;` |
|     ! 0 | 1783 | `		unsigned int iIdx = 0;` |
|     ! 0 | 1784 | `		nOpt = (ph7_socklen)sizeof(sIf);` |
|     ! 0 | 1785 | `		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,(char *)&sIf,&nOpt) != 0 ){` |
|     ! 0 | 1786 | `			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");` |
|     ! 0 | 1787 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 1788 | `			return PH7_OK;` |
|       - | 1789 | `		}` |
|     ! 0 | 1790 | `		if( SockAddr4ToIfIndex(&sIf,&iIdx) != 0 ){` |
|     ! 0 | 1791 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 1792 | `			return PH7_OK;` |
|       - | 1793 | `		}` |
|     ! 0 | 1794 | `		ph7_result_int64(pCtx,(ph7_int64)iIdx);` |
|     ! 0 | 1795 | `		return PH7_OK;` |
|       - | 1796 | `	}` |
|      16 | 1797 | `	if( iLevel == IPPROTO_IP` |
|       8 | 1798 | `	 && ((int)iOpt == IP_MULTICAST_LOOP \|\| (int)iOpt == IP_MULTICAST_TTL) ){` |
|     ! 0 | 1799 | `		unsigned char c = 0;` |
|     ! 0 | 1800 | `		nOpt = (ph7_socklen)sizeof(c);` |
|     ! 0 | 1801 | `		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,(char *)&c,&nOpt) != 0 ){` |
|     ! 0 | 1802 | `			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");` |
|     ! 0 | 1803 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 1804 | `			return PH7_OK;` |
|       - | 1805 | `		}` |
|     ! 0 | 1806 | `		ph7_result_int64(pCtx,(ph7_int64)c);` |
|     ! 0 | 1807 | `		return PH7_OK;` |
|       - | 1808 | `	}` |
|       - | 1809 | `#endif` |
|      18 | 1810 | `	if( iLevel == SOL_SOCKET && (int)iOpt == SO_LINGER ){` |
|       - | 1811 | `		struct linger sLin;` |
|       - | 1812 | `		ph7_value *pArr,*pTmp;` |
|       3 | 1813 | `		SyZero(&sLin,sizeof(sLin));` |
|       3 | 1814 | `		nOpt = (ph7_socklen)sizeof(sLin);` |
|       3 | 1815 | `		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,(char *)&sLin,&nOpt) != 0 ){` |
|     ! 0 | 1816 | `			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");` |
|     ! 0 | 1817 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 1818 | `			return PH7_OK;` |
|       - | 1819 | `		}` |
|       3 | 1820 | `		pArr = ph7_context_new_array(pCtx);` |
|       3 | 1821 | `		pTmp = ph7_context_new_scalar(pCtx);` |
|       3 | 1822 | `		if( pArr == 0 \|\| pTmp == 0 ){` |
|     ! 0 | 1823 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 1824 | `		}` |
|       3 | 1825 | `		ph7_value_int64(pTmp,sLin.l_onoff);` |
|       3 | 1826 | `		ph7_array_add_strkey_elem(pArr,"l_onoff",pTmp);` |
|       3 | 1827 | `		ph7_value_int64(pTmp,sLin.l_linger);` |
|       3 | 1828 | `		ph7_array_add_strkey_elem(pArr,"l_linger",pTmp);` |
|       3 | 1829 | `		ph7_result_value(pCtx,pArr);` |
|       3 | 1830 | `		return PH7_OK;` |
|       - | 1831 | `	}` |
|      16 | 1832 | `	if( iLevel == SOL_SOCKET && ((int)iOpt == SO_RCVTIMEO \|\| (int)iOpt == SO_SNDTIMEO) ){` |
|       - | 1833 | `		struct timeval tv;` |
|       - | 1834 | `		ph7_value *pArr,*pTmp;` |
|       3 | 1835 | `		SyZero(&tv,sizeof(tv));` |
|       3 | 1836 | `		nOpt = (ph7_socklen)sizeof(tv);` |
|       3 | 1837 | `		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,(char *)&tv,&nOpt) != 0 ){` |
|     ! 0 | 1838 | `			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");` |
|     ! 0 | 1839 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 1840 | `			return PH7_OK;` |
|       - | 1841 | `		}` |
|       3 | 1842 | `		pArr = ph7_context_new_array(pCtx);` |
|       3 | 1843 | `		pTmp = ph7_context_new_scalar(pCtx);` |
|       3 | 1844 | `		if( pArr == 0 \|\| pTmp == 0 ){` |
|     ! 0 | 1845 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 1846 | `		}` |
|       3 | 1847 | `		ph7_value_int64(pTmp,(ph7_int64)tv.tv_sec);` |
|       3 | 1848 | `		ph7_array_add_strkey_elem(pArr,"sec",pTmp);` |
|       3 | 1849 | `		ph7_value_int64(pTmp,(ph7_int64)tv.tv_usec);` |
|       3 | 1850 | `		ph7_array_add_strkey_elem(pArr,"usec",pTmp);` |
|       3 | 1851 | `		ph7_result_value(pCtx,pArr);` |
|       3 | 1852 | `		return PH7_OK;` |
|       - | 1853 | `	}` |
|      14 | 1854 | `	if( iLevel == IPPROTO_TCP && (int)iOpt == PHL_TCP_CONGESTION ){` |
|       - | 1855 | ``		/* The one option php reports as a NAMED string: `['name' => 'cubic']`. */`` |
|       - | 1856 | `		char zName[64];` |
|       - | 1857 | `		ph7_value *pArr,*pTmp;` |
|     ! 0 | 1858 | `		nOpt = (ph7_socklen)sizeof(zName);` |
|     ! 0 | 1859 | `		SyZero(zName,sizeof(zName));` |
|     ! 0 | 1860 | `		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,zName,&nOpt) != 0 ){` |
|     ! 0 | 1861 | `			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");` |
|     ! 0 | 1862 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 1863 | `			return PH7_OK;` |
|       - | 1864 | `		}` |
|     ! 0 | 1865 | `		pArr = ph7_context_new_array(pCtx);` |
|     ! 0 | 1866 | `		pTmp = ph7_context_new_scalar(pCtx);` |
|     ! 0 | 1867 | `		if( pArr == 0 \|\| pTmp == 0 ){` |
|     ! 0 | 1868 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 1869 | `		}` |
|     ! 0 | 1870 | `		ph7_value_string(pTmp,zName,(int)SyStrlen(zName));` |
|     ! 0 | 1871 | `		ph7_array_add_strkey_elem(pArr,"name",pTmp);` |
|     ! 0 | 1872 | `		ph7_result_value(pCtx,pArr);` |
|     ! 0 | 1873 | `		return PH7_OK;` |
|       - | 1874 | `	}` |
|       - | 1875 | `	{` |
|      14 | 1876 | `		int iNum = 0;` |
|      14 | 1877 | `		nOpt = (ph7_socklen)sizeof(iNum);` |
|      14 | 1878 | `		if( getsockopt(pSock->sock,(int)iLevel,(int)iOpt,(char *)&iNum,&nOpt) != 0 ){` |
|       3 | 1879 | `			SockFailLast(pCtx,pSock,"Unable to retrieve socket option");` |
|       3 | 1880 | `			ph7_result_bool(pCtx,0);` |
|       3 | 1881 | `			return PH7_OK;` |
|       - | 1882 | `		}` |
|      12 | 1883 | `		ph7_result_int64(pCtx,iNum);` |
|       - | 1884 | `	}` |
|      12 | 1885 | `	return PH7_OK;` |
|      12 | 1886 | `}` |
|       - | 1887 |  |
|       - | 1888 | `/* ------------------------------------------------------------------------` |
|       - | 1889 | ` * socket_select()` |
|       - | 1890 | ` * ------------------------------------------------------------------------ */` |
|       - | 1891 | `typedef struct SockSelectCtx SockSelectCtx;` |
|       - | 1892 | `struct SockSelectCtx` |
|       - | 1893 | `{` |
|       - | 1894 | `	ph7_context *pCtx;` |
|       - | 1895 | ``	ph7_class *pClass;  /* the mounted `Socket`, compared by POINTER: php's own`` |
|       - | 1896 | ``	                     * screen is `Z_OBJCE_P(element) != socket_ce`, and a`` |
|       - | 1897 | `	                     * name compare would have to reach into a SyString that` |
|       - | 1898 | `	                     * is not NUL-terminated */` |
|       - | 1899 | `	fd_set *pSet;` |
|       - | 1900 | `	ph7_value *pOut;    /* the rebuilt array, on the FILTER pass */` |
|       - | 1901 | `	int iArgPos;        /* which of the three arrays, for the type error */` |
|       - | 1902 | `	const char *zArgName;` |
|       - | 1903 | `	int iMax;` |
|       - | 1904 | `	int nSeen;` |
|       - | 1905 | `	int rc;             /* PH7_OK, or the throw a bad element raised */` |
|       - | 1906 | `	int bBad;` |
|       - | 1907 | `};` |
|       - | 1908 | `/*` |
|       - | 1909 | ` * Pass one: every element must BE a Socket, and one that was closed is php's` |
|       - | 1910 | ` * own Error rather than a skipped entry.` |
|       - | 1911 | ` */` |
|      24 | 1912 | `static int SockSelectAdd(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|       2 | 1913 | `{` |
|      26 | 1914 | `	SockSelectCtx *p = (SockSelectCtx *)pUserData;` |
|       - | 1915 | `	ph7_class_instance *pThis;` |
|       - | 1916 | `	phl_socket *pSock;` |
|      12 | 1917 | `	SXUNUSED(pKey);` |
|      26 | 1918 | `	pThis = ph7_value_is_object(pVal) ? (ph7_class_instance *)pVal->x.pOther : 0;` |
|      26 | 1919 | `	if( pThis == 0 \|\| pThis->pClass != p->pClass ){` |
|       - | 1920 | `		char zGiven[64];` |
|       7 | 1921 | `		p->bBad = 1;` |
|      10 | 1922 | `		p->rc = PH7_VmThrowException(p->pCtx,"TypeError",` |
|       - | 1923 | `			"%z(): Argument #%d ($%s) must only have elements of type Socket, %s given",` |
|       6 | 1924 | `			&p->pCtx->pFunc->sName,p->iArgPos,p->zArgName,` |
|       3 | 1925 | `			VmValueGivenName(pVal,zGiven,sizeof(zGiven)));` |
|       7 | 1926 | `		return PH7_ABORT;` |
|       - | 1927 | `	}` |
|      20 | 1928 | `	pSock = (phl_socket *)SockSlotOf(pThis);` |
|      20 | 1929 | `	if( pSock == 0 \|\| pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|       - | 1930 | `		/* A TypeError rather than the Error every other verb raises: php screens` |
|       - | 1931 | `		 * the ARRAY here, so a closed element is a bad argument value. */` |
|       3 | 1932 | `		p->bBad = 1;` |
|       5 | 1933 | `		p->rc = PH7_VmThrowException(p->pCtx,"TypeError",` |
|       - | 1934 | `			"%z(): Argument #%d ($%s) contains a closed socket",` |
|       2 | 1935 | `			&p->pCtx->pFunc->sName,p->iArgPos,p->zArgName);` |
|       3 | 1936 | `		return PH7_ABORT;` |
|       - | 1937 | `	}` |
|      17 | 1938 | `	FD_SET(pSock->sock,p->pSet);` |
|      17 | 1939 | `	if( (int)pSock->sock > p->iMax ){` |
|      15 | 1940 | `		p->iMax = (int)pSock->sock;` |
|       7 | 1941 | `	}` |
|      17 | 1942 | `	p->nSeen++;` |
|      17 | 1943 | `	return PH7_OK;` |
|      14 | 1944 | `}` |
|       - | 1945 | `/* Pass two: rebuild the array out of the entries select() marked, KEYS AND ALL. */` |
|      16 | 1946 | `static int SockSelectKeep(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|       1 | 1947 | `{` |
|      17 | 1948 | `	SockSelectCtx *p = (SockSelectCtx *)pUserData;` |
|      17 | 1949 | `	ph7_class_instance *pThis = ph7_value_is_object(pVal) ?` |
|      16 | 1950 | `		(ph7_class_instance *)pVal->x.pOther : 0;` |
|      17 | 1951 | `	phl_socket *pSock = (phl_socket *)SockSlotOf(pThis);` |
|      16 | 1952 | `	if( pSock && pSock->sock != PH7_NET_INVALID_SOCKET` |
|      17 | 1953 | `	 && FD_ISSET(pSock->sock,p->pSet) ){` |
|      13 | 1954 | `		ph7_array_add_elem(p->pOut,pKey,pVal);` |
|       6 | 1955 | `	}` |
|      17 | 1956 | `	return PH7_OK;` |
|       1 | 1957 | `}` |
|       - | 1958 | `/*` |
|       - | 1959 | ` * int\|false socket_select(?array &$read, ?array &$write, ?array &$except,` |
|       - | 1960 | ` *                         ?int $seconds, int $microseconds = 0)` |
|       - | 1961 | ` */` |
|      22 | 1962 | `static int vm_builtin_socket_select(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1963 | `{` |
|       - | 1964 | `	static const char * const azName[3] = { "read","write","except" };` |
|       - | 1965 | `	fd_set aSet[3];` |
|       - | 1966 | `	SockSelectCtx sWalk;` |
|       - | 1967 | `	ph7_value *apOut[3];` |
|       - | 1968 | `	ph7_class *pClass;` |
|      24 | 1969 | `	struct timeval tv,*pTv = 0;` |
|      24 | 1970 | `	int i,iMax = -1,nSets = 0,iRet;` |
|      11 | 1971 | `	SXUNUSED(nArg);` |
|      24 | 1972 | `	pClass = PH7_VmExtractClass(pCtx->pVm,"Socket",sizeof("Socket")-1,0,0);` |
|      90 | 1973 | `	for( i = 0 ; i < 3 ; ++i ){` |
|     596 | 1974 | `		FD_ZERO(&aSet[i]);` |
|      68 | 1975 | `		apOut[i] = 0;` |
|      35 | 1976 | `	}` |
|      72 | 1977 | `	for( i = 0 ; i < 3 ; ++i ){` |
|      58 | 1978 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      32 | 1979 | `			continue;` |
|       - | 1980 | `		}` |
|      28 | 1981 | `		SyZero(&sWalk,sizeof(sWalk));` |
|      28 | 1982 | `		sWalk.pCtx = pCtx;` |
|      28 | 1983 | `		sWalk.pClass = pClass;` |
|      28 | 1984 | `		sWalk.pSet = &aSet[i];` |
|      28 | 1985 | `		sWalk.iArgPos = i + 1;` |
|      28 | 1986 | `		sWalk.zArgName = azName[i];` |
|      28 | 1987 | `		sWalk.iMax = iMax;` |
|      28 | 1988 | `		sWalk.rc = PH7_OK;` |
|      28 | 1989 | `		ph7_array_walk(apArg[i],SockSelectAdd,(void *)&sWalk);` |
|      28 | 1990 | `		if( sWalk.bBad ){` |
|       9 | 1991 | `			return sWalk.rc;` |
|       - | 1992 | `		}` |
|      20 | 1993 | `		iMax = sWalk.iMax;` |
|      20 | 1994 | `		if( sWalk.nSeen > 0 ){` |
|      11 | 1995 | `			nSets++;` |
|       5 | 1996 | `		}` |
|      11 | 1997 | `	}` |
|      16 | 1998 | `	if( nSets < 1 ){` |
|       7 | 1999 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       4 | 2000 | `			"%z(): At least one array argument must be passed",&pCtx->pFunc->sName);` |
|       - | 2001 | `	}` |
|      11 | 2002 | `	if( !ph7_value_is_null(apArg[3]) ){` |
|      11 | 2003 | `		ph7_int64 iSec = ph7_value_to_int64(apArg[3]);` |
|      11 | 2004 | `		ph7_int64 iUsec = nArg > 4 ? ph7_value_to_int64(apArg[4]) : 0;` |
|      11 | 2005 | `		tv.tv_sec = (long)(iSec + iUsec / 1000000);` |
|      11 | 2006 | `		tv.tv_usec = (long)(iUsec % 1000000);` |
|      11 | 2007 | `		pTv = &tv;` |
|       5 | 2008 | `	}` |
|      11 | 2009 | `	iRet = select(iMax + 1,&aSet[0],&aSet[1],&aSet[2],pTv);` |
|      11 | 2010 | `	if( iRet < 0 ){` |
|     ! 0 | 2011 | `		SockFailLast(pCtx,0,"Unable to select");` |
|     ! 0 | 2012 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2013 | `		return PH7_OK;` |
|       - | 2014 | `	}` |
|      41 | 2015 | `	for( i = 0 ; i < 3 ; ++i ){` |
|      31 | 2016 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      19 | 2017 | `			continue;` |
|       - | 2018 | `		}` |
|      13 | 2019 | `		apOut[i] = ph7_context_new_array(pCtx);` |
|      13 | 2020 | `		if( apOut[i] == 0 ){` |
|     ! 0 | 2021 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 2022 | `		}` |
|      13 | 2023 | `		SyZero(&sWalk,sizeof(sWalk));` |
|      13 | 2024 | `		sWalk.pCtx = pCtx;` |
|      13 | 2025 | `		sWalk.pSet = &aSet[i];` |
|      13 | 2026 | `		sWalk.pOut = apOut[i];` |
|      13 | 2027 | `		ph7_array_walk(apArg[i],SockSelectKeep,(void *)&sWalk);` |
|       7 | 2028 | `	}` |
|       - | 2029 | `	/* The stores happen only after every array has been rebuilt: writing one` |
|       - | 2030 | `	 * back can move the memobj pool, and the walk above holds pointers into it. */` |
|      41 | 2031 | `	for( i = 0 ; i < 3 ; ++i ){` |
|      31 | 2032 | `		if( apOut[i] ){` |
|      13 | 2033 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[i],apOut[i]);` |
|       6 | 2034 | `		}` |
|      16 | 2035 | `	}` |
|      11 | 2036 | `	ph7_result_int64(pCtx,iRet);` |
|      11 | 2037 | `	return PH7_OK;` |
|      13 | 2038 | `}` |
|       - | 2039 |  |
|       - | 2040 | `/* ------------------------------------------------------------------------` |
|       - | 2041 | ` * The error verbs` |
|       - | 2042 | ` * ------------------------------------------------------------------------ */` |
|       - | 2043 | `/*` |
|       - | 2044 | ` * int socket_last_error(?Socket $socket = null)` |
|       - | 2045 | ` *` |
|       - | 2046 | ` * With a socket it is that socket's own code; with none it is the per-request` |
|       - | 2047 | ` * one. A CLOSED socket is refused here too -- php runs the same screen on both` |
|       - | 2048 | ` * of these as on a verb that would touch the descriptor.` |
|       - | 2049 | ` */` |
|      32 | 2050 | `static int vm_builtin_socket_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2051 | `{` |
|      35 | 2052 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|       - | 2053 | `		int rc;` |
|      25 | 2054 | `		phl_socket *pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      25 | 2055 | `		if( pSock == 0 ){` |
|       3 | 2056 | `			return rc;` |
|       - | 2057 | `		}` |
|      23 | 2058 | `		ph7_result_int64(pCtx,pSock->iError);` |
|      23 | 2059 | `		return PH7_OK;` |
|       - | 2060 | `	}` |
|      11 | 2061 | `	ph7_result_int64(pCtx,pCtx->pVm->iSocketLastErr);` |
|      11 | 2062 | `	return PH7_OK;` |
|      19 | 2063 | `}` |
|       - | 2064 | `/* void socket_clear_error(?Socket $socket = null) */` |
|      26 | 2065 | `static int vm_builtin_socket_clear_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2066 | `{` |
|      29 | 2067 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|       - | 2068 | `		int rc;` |
|       5 | 2069 | `		phl_socket *pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|       5 | 2070 | `		if( pSock == 0 ){` |
|       3 | 2071 | `			return rc;` |
|       - | 2072 | `		}` |
|       3 | 2073 | `		pSock->iError = 0;` |
|       3 | 2074 | `		return PH7_OK;` |
|       - | 2075 | `	}` |
|      25 | 2076 | `	pCtx->pVm->iSocketLastErr = 0;` |
|      25 | 2077 | `	return PH7_OK;` |
|      16 | 2078 | `}` |
|       - | 2079 | `/* string socket_strerror(int $error_code) */` |
|       4 | 2080 | `static int vm_builtin_socket_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2081 | `{` |
|       - | 2082 | `	const char *z;` |
|       2 | 2083 | `	SXUNUSED(nArg);` |
|       5 | 2084 | `	z = PH7_SocketStrError((int)ph7_value_to_int64(apArg[0]));` |
|       5 | 2085 | `	ph7_result_string(pCtx,z ? z : "",-1);` |
|       5 | 2086 | `	return PH7_OK;` |
|       1 | 2087 | `}` |
|       - | 2088 |  |
|       - | 2089 | `/* ------------------------------------------------------------------------` |
|       - | 2090 | ` * The two doors onto the stream layer` |
|       - | 2091 | ` * ------------------------------------------------------------------------ */` |
|       - | 2092 | `/*` |
|       - | 2093 | ` * resource\|false socket_export_stream(Socket $socket)` |
|       - | 2094 | ` *` |
|       - | 2095 | ` * php builds the stream ONCE and hands the same handle back for ever after, and` |
|       - | 2096 | ` * the two then share a descriptor: closing either closes both. The URI and the` |
|       - | 2097 | `` * `stream_type` label are picked from the DOMAIN as well as the type, which is`` |
|       - | 2098 | `` * why an AF_UNIX datagram socket reports `udg_socket`.`` |
|       - | 2099 | ` */` |
|      12 | 2100 | `static int vm_builtin_socket_export_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2101 | `{` |
|       - | 2102 | `	phl_socket *pSock;` |
|       - | 2103 | `	io_private *pDev;` |
|       - | 2104 | `	const char *zUri,*zLabel;` |
|       - | 2105 | `	int rc,bDgram;` |
|       6 | 2106 | `	SXUNUSED(nArg);` |
|      14 | 2107 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|      14 | 2108 | `	if( pSock == 0 ){` |
|       3 | 2109 | `		return rc;` |
|       - | 2110 | `	}` |
|      11 | 2111 | `	if( pSock->pStream ){` |
|       3 | 2112 | `		ph7_result_resource(pCtx,pSock->pStream);` |
|       3 | 2113 | `		return PH7_OK;` |
|       - | 2114 | `	}` |
|       9 | 2115 | `	bDgram = (pSock->iType != SOCK_STREAM);` |
|       9 | 2116 | `	if( pSock->iDomain == AF_UNIX ){` |
|     ! 0 | 2117 | `		zUri   = bDgram ? "udg://" : "unix://";` |
|     ! 0 | 2118 | `		zLabel = bDgram ? "udg_socket" : "unix_socket";` |
|     ! 0 | 2119 | `	}else{` |
|       9 | 2120 | `		zUri   = bDgram ? "udp://" : "tcp://";` |
|       9 | 2121 | `		zLabel = bDgram ? "udp_socket" : "tcp_socket/ssl";` |
|       - | 2122 | `	}` |
|       9 | 2123 | `	pDev = PH7_StreamWrapSocket(pCtx,pSock->sock,bDgram,zLabel,zUri);` |
|       9 | 2124 | `	if( pDev == 0 ){` |
|     ! 0 | 2125 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 2126 | `	}` |
|       9 | 2127 | `	pSock->pStream = (void *)pDev;` |
|       9 | 2128 | `	pSock->bExported = 1;` |
|       9 | 2129 | `	ph7_result_resource(pCtx,pDev);` |
|       9 | 2130 | `	return PH7_OK;` |
|       8 | 2131 | `}` |
|       - | 2132 | `/* Socket\|false socket_import_stream($stream) */` |
|       4 | 2133 | `static int vm_builtin_socket_import_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2134 | `{` |
|       - | 2135 | `	struct sockaddr_storage sAddr;` |
|       5 | 2136 | `	ph7_socklen nAddr = (ph7_socklen)sizeof(sAddr);` |
|       - | 2137 | `	ph7_class *pClass;` |
|       - | 2138 | `	ph7_class_instance *pThis;` |
|       - | 2139 | `	io_private *pDev;` |
|       - | 2140 | `	phl_socket *pRec;` |
|       - | 2141 | `	ph7_socket sock;` |
|       5 | 2142 | `	int rc,iType = SOCK_STREAM,iDomain = AF_INET;` |
|       5 | 2143 | `	ph7_socklen nOpt = (ph7_socklen)sizeof(iType);` |
|       2 | 2144 | `	SXUNUSED(nArg);` |
|       5 | 2145 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|       5 | 2146 | `	if( pDev == 0 ){` |
|     ! 0 | 2147 | `		return rc;` |
|       - | 2148 | `	}` |
|       5 | 2149 | `	if( !PH7_StreamSocketHandle(pDev,&sock) ){` |
|       4 | 2150 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 2151 | `			"Cannot represent a stream of type %s as a Socket Descriptor",` |
|       1 | 2152 | `			PH7_StreamTypeLabel(pDev));` |
|       3 | 2153 | `		ph7_result_bool(pCtx,0);` |
|       3 | 2154 | `		return PH7_OK;` |
|       - | 2155 | `	}` |
|       3 | 2156 | `	SyZero(&sAddr,sizeof(sAddr));` |
|       3 | 2157 | `	if( getsockname(sock,(struct sockaddr *)&sAddr,&nAddr) == 0 && sAddr.ss_family != 0 ){` |
|       2 | 2158 | `		iDomain = (int)sAddr.ss_family;` |
|       1 | 2159 | `	}` |
|       3 | 2160 | `	getsockopt(sock,SOL_SOCKET,SO_TYPE,(char *)&iType,&nOpt);` |
|       3 | 2161 | `	pClass = PH7_VmExtractClass(pCtx->pVm,"Socket",sizeof("Socket")-1,0,0);` |
|       3 | 2162 | `	pThis = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|       3 | 2163 | `	pRec = pThis ? SockNew(pCtx->pVm,sock,iDomain,iType,0) : 0;` |
|       3 | 2164 | `	if( pRec == 0 \|\| SockSlotAttach(pThis,(void *)pRec) != 0 ){` |
|     ! 0 | 2165 | `		if( pThis ){` |
|     ! 0 | 2166 | `			PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2167 | `		}` |
|     ! 0 | 2168 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 2169 | `	}` |
|       - | 2170 | `	/* The STREAM owns the descriptor: an imported socket must not close it` |
|       - | 2171 | `	 * behind the handle a script is still holding. */` |
|       3 | 2172 | `	pRec->pOwner = pThis;` |
|       3 | 2173 | `	pRec->pStream = (void *)pDev;` |
|       3 | 2174 | `	pRec->bExported = 1;` |
|       3 | 2175 | `	PH7_NativeResultObject(pCtx,pThis);` |
|       3 | 2176 | `	return PH7_OK;` |
|       3 | 2177 | `}` |
|       - | 2178 |  |
|       - | 2179 | `/* ------------------------------------------------------------------------` |
|       - | 2180 | ` * sendmsg / recvmsg / cmsg_space` |
|       - | 2181 | ` * ------------------------------------------------------------------------ */` |
|       - | 2182 | `/*` |
|       - | 2183 | ` * php's ancillary registry: the (level, type) pairs it knows how to convert,` |
|       - | 2184 | ` * with the fixed part of the payload and the size of one repeat. The pair that` |
|       - | 2185 | ` * is not in it is refused by NUMBER -- "Pair level 1 and/or type 9999 is not` |
|       - | 2186 | ` * supported" -- which is why this is a table rather than a switch.` |
|       - | 2187 | ` */` |
|       8 | 2188 | `static int SockCmsgEntry(int iLevel,int iType,unsigned int *pFixed,unsigned int *pElem)` |
|       1 | 2189 | `{` |
|       4 | 2190 | `	SXUNUSED(iType); SXUNUSED(pFixed); SXUNUSED(pElem); /* a platform with no pair at all */` |
|       9 | 2191 | `	if( iLevel == SOL_SOCKET ){` |
|       - | 2192 | `#ifdef SCM_RIGHTS` |
|     ! 0 | 2193 | `		if( iType == SCM_RIGHTS ){` |
|     ! 0 | 2194 | `			*pFixed = 0;` |
|     ! 0 | 2195 | `			*pElem = (unsigned int)sizeof(int);` |
|     ! 0 | 2196 | `			return 0;` |
|       - | 2197 | `		}` |
|       - | 2198 | `#endif` |
|       - | 2199 | `#ifdef SCM_CREDENTIALS` |
|     ! 0 | 2200 | `		if( iType == SCM_CREDENTIALS ){` |
|     ! 0 | 2201 | `			*pFixed = (unsigned int)sizeof(struct ucred);` |
|     ! 0 | 2202 | `			*pElem = 0;` |
|     ! 0 | 2203 | `			return 0;` |
|       - | 2204 | `		}` |
|       - | 2205 | `#endif` |
|     ! 0 | 2206 | `		return -1;` |
|       - | 2207 | `	}` |
|       - | 2208 | `#if defined(IPPROTO_IPV6) \|\| defined(__WINNT__)` |
|       9 | 2209 | `	if( iLevel == IPPROTO_IPV6 ){` |
|       - | 2210 | `#ifdef IPV6_PKTINFO` |
|       7 | 2211 | `		if( iType == IPV6_PKTINFO ){` |
|     ! 0 | 2212 | `			*pFixed = (unsigned int)sizeof(struct in6_pktinfo);` |
|     ! 0 | 2213 | `			*pElem = 0;` |
|     ! 0 | 2214 | `			return 0;` |
|       - | 2215 | `		}` |
|       - | 2216 | `#endif` |
|       - | 2217 | `#ifdef IPV6_HOPLIMIT` |
|       7 | 2218 | `		if( iType == IPV6_HOPLIMIT ){` |
|       7 | 2219 | `			*pFixed = (unsigned int)sizeof(int);` |
|       7 | 2220 | `			*pElem = 0;` |
|       7 | 2221 | `			return 0;` |
|       - | 2222 | `		}` |
|       - | 2223 | `#endif` |
|       - | 2224 | `#ifdef IPV6_TCLASS` |
|     ! 0 | 2225 | `		if( iType == IPV6_TCLASS ){` |
|     ! 0 | 2226 | `			*pFixed = (unsigned int)sizeof(int);` |
|     ! 0 | 2227 | `			*pElem = 0;` |
|     ! 0 | 2228 | `			return 0;` |
|       - | 2229 | `		}` |
|       - | 2230 | `#endif` |
|     ! 0 | 2231 | `	}` |
|       - | 2232 | `#endif` |
|       3 | 2233 | `	return -1;` |
|       5 | 2234 | `}` |
|       - | 2235 | `/* ?int socket_cmsg_space(int $level, int $type, int $num = 0) */` |
|       8 | 2236 | `static int vm_builtin_socket_cmsg_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2237 | `{` |
|       9 | 2238 | `	unsigned int nFixed = 0,nElem = 0;` |
|       9 | 2239 | `	ph7_int64 iLevel,iType,iNum = 0;` |
|       9 | 2240 | `	iLevel = ph7_value_to_int64(apArg[0]);` |
|       9 | 2241 | `	iType  = ph7_value_to_int64(apArg[1]);` |
|       9 | 2242 | `	if( nArg > 2 ){` |
|       5 | 2243 | `		iNum = ph7_value_to_int64(apArg[2]);` |
|       2 | 2244 | `	}` |
|       9 | 2245 | `	if( SockCmsgEntry((int)iLevel,(int)iType,&nFixed,&nElem) != 0 ){` |
|       - | 2246 | `		/* php's one refusal here carries no function prefix. */` |
|       4 | 2247 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       1 | 2248 | `			"Pair level %d and/or type %d is not supported",(int)iLevel,(int)iType);` |
|       - | 2249 | `	}` |
|       7 | 2250 | `	if( iNum < 0 ){` |
|       4 | 2251 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2252 | `			"%z(): Argument #3 ($num) must be greater than or equal to 0",` |
|       2 | 2253 | `			&pCtx->pFunc->sName);` |
|       - | 2254 | `	}` |
|       5 | 2255 | `	if( iNum > 0 && nElem > 0 && iNum > (ph7_int64)((0x7FFFFFFF - nFixed) / nElem) ){` |
|     ! 0 | 2256 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     ! 0 | 2257 | `			"%z(): Argument #3 ($num) is too large",&pCtx->pFunc->sName);` |
|       - | 2258 | `	}` |
|       - | 2259 | `	/* An entry with no repeating part IGNORES $num rather than refusing it:` |
|       - | 2260 | ``	 * `socket_cmsg_space(SOL_SOCKET, SCM_CREDENTIALS, 2)` is the same 32 as`` |
|       - | 2261 | `	 * with no third argument at all. */` |
|       5 | 2262 | `	ph7_result_int64(pCtx,(ph7_int64)PHL_CMSG_SPACE(nFixed + (size_t)iNum * nElem));` |
|       5 | 2263 | `	return PH7_OK;` |
|       5 | 2264 | `}` |
|       - | 2265 |  |
|       - | 2266 | `/*` |
|       - | 2267 | ` * The two message primitives, and the ONLY part of sendmsg()/recvmsg() that is` |
|       - | 2268 | `` * written twice. POSIX takes a `struct msghdr` and Winsock a `WSAMSG` whose`` |
|       - | 2269 | ` * fields have different names, different types and (for the control block) a` |
|       - | 2270 | ` * different shape -- but both take exactly one address, one gathered data` |
|       - | 2271 | ` * buffer and one contiguous control block, which is what the shared half above` |
|       - | 2272 | `` * and below builds. `WSARecvMsg` is not exported by ws2_32 at all and has to be`` |
|       - | 2273 | ` * fetched per socket through WSAIoctl, which is what php does too.` |
|       - | 2274 | ` */` |
|       4 | 2275 | `static int SockMsgSend(ph7_socket sock,struct sockaddr *pName,ph7_socklen nName,` |
|       - | 2276 | `	const char *zData,unsigned int nData,const char *zCtl,unsigned int nCtl,int iFlags)` |
|       1 | 2277 | `{` |
|       - | 2278 | `#ifdef __WINNT__` |
|       - | 2279 | `	WSAMSG sMsg;` |
|       - | 2280 | `	WSABUF sBuf;` |
|       1 | 2281 | `	DWORD nSent = 0;` |
|       1 | 2282 | `	SyZero(&sMsg,sizeof(sMsg));` |
|       1 | 2283 | `	sBuf.len = (ULONG)nData;` |
|       1 | 2284 | `	sBuf.buf = (CHAR *)zData;` |
|       1 | 2285 | `	sMsg.name = pName;` |
|       1 | 2286 | `	sMsg.namelen = (INT)nName;` |
|       1 | 2287 | `	if( nData > 0 \|\| zData ){` |
|       1 | 2288 | `		sMsg.lpBuffers = &sBuf;` |
|       1 | 2289 | `		sMsg.dwBufferCount = 1;` |
|       - | 2290 | `	}` |
|       1 | 2291 | `	sMsg.Control.len = (ULONG)nCtl;` |
|       1 | 2292 | `	sMsg.Control.buf = (CHAR *)zCtl;` |
|       1 | 2293 | `	if( WSASendMsg(sock,&sMsg,(DWORD)iFlags,&nSent,0,0) != 0 ){` |
|     ! 0 | 2294 | `		return -1;` |
|       - | 2295 | `	}` |
|       1 | 2296 | `	return (int)nSent;` |
|       - | 2297 | `#else` |
|       - | 2298 | `	struct msghdr sMsg;` |
|       - | 2299 | `	struct iovec sVec;` |
|       4 | 2300 | `	SyZero(&sMsg,sizeof(sMsg));` |
|       4 | 2301 | `	sVec.iov_base = (void *)zData;` |
|       4 | 2302 | `	sVec.iov_len = (size_t)nData;` |
|       4 | 2303 | `	sMsg.msg_name = (void *)pName;` |
|       4 | 2304 | `	sMsg.msg_namelen = nName;` |
|       4 | 2305 | `	if( nData > 0 \|\| zData ){` |
|       4 | 2306 | `		sMsg.msg_iov = &sVec;` |
|       4 | 2307 | `		sMsg.msg_iovlen = 1;` |
|       2 | 2308 | `	}` |
|       4 | 2309 | `	sMsg.msg_control = (void *)zCtl;` |
|       4 | 2310 | `	sMsg.msg_controllen = nCtl;` |
|       4 | 2311 | `	return (int)sendmsg(sock,&sMsg,iFlags);` |
|       - | 2312 | `#endif` |
|       1 | 2313 | `}` |
|       4 | 2314 | `static int SockMsgRecv(ph7_socket sock,struct sockaddr *pName,ph7_socklen *pnName,` |
|       - | 2315 | `	char *zData,unsigned int nData,char *zCtl,unsigned int nCtl,` |
|       - | 2316 | `	unsigned int *pnCtlOut,int iFlags,int *piFlagsOut)` |
|       1 | 2317 | `{` |
|       - | 2318 | `#ifdef __WINNT__` |
|       - | 2319 | `	static LPFN_WSARECVMSG xRecvMsg = 0;` |
|       - | 2320 | `	WSAMSG sMsg;` |
|       - | 2321 | `	WSABUF sBuf;` |
|       1 | 2322 | `	DWORD nGot = 0;` |
|       1 | 2323 | `	if( xRecvMsg == 0 ){` |
|       1 | 2324 | `		GUID sId = WSAID_WSARECVMSG;` |
|       1 | 2325 | `		DWORD nOut = 0;` |
|       - | 2326 | `		if( WSAIoctl(sock,SIO_GET_EXTENSION_FUNCTION_POINTER,&sId,(DWORD)sizeof(sId),` |
|       1 | 2327 | `			&xRecvMsg,(DWORD)sizeof(xRecvMsg),&nOut,0,0) != 0 ){` |
|     ! 0 | 2328 | `			xRecvMsg = 0;` |
|     ! 0 | 2329 | `			return -1;` |
|       - | 2330 | `		}` |
|       - | 2331 | `	}` |
|       1 | 2332 | `	SyZero(&sMsg,sizeof(sMsg));` |
|       1 | 2333 | `	sBuf.len = (ULONG)nData;` |
|       1 | 2334 | `	sBuf.buf = zData;` |
|       1 | 2335 | `	sMsg.name = pName;` |
|       1 | 2336 | `	sMsg.namelen = pName ? (INT)*pnName : 0;` |
|       1 | 2337 | `	sMsg.lpBuffers = &sBuf;` |
|       1 | 2338 | `	sMsg.dwBufferCount = 1;` |
|       1 | 2339 | `	sMsg.Control.len = (ULONG)nCtl;` |
|       1 | 2340 | `	sMsg.Control.buf = zCtl;` |
|       1 | 2341 | `	sMsg.dwFlags = (DWORD)iFlags;` |
|       1 | 2342 | `	if( xRecvMsg(sock,&sMsg,&nGot,0,0) != 0 ){` |
|     ! 0 | 2343 | `		return -1;` |
|       - | 2344 | `	}` |
|       1 | 2345 | `	if( pName ){` |
|       1 | 2346 | `		*pnName = (ph7_socklen)sMsg.namelen;` |
|       - | 2347 | `	}` |
|       1 | 2348 | `	*pnCtlOut = (unsigned int)sMsg.Control.len;` |
|       1 | 2349 | `	*piFlagsOut = (int)sMsg.dwFlags;` |
|       1 | 2350 | `	return (int)nGot;` |
|       - | 2351 | `#else` |
|       - | 2352 | `	struct msghdr sMsg;` |
|       - | 2353 | `	struct iovec sVec;` |
|       - | 2354 | `	int iGot;` |
|       4 | 2355 | `	SyZero(&sMsg,sizeof(sMsg));` |
|       4 | 2356 | `	sVec.iov_base = zData;` |
|       4 | 2357 | `	sVec.iov_len = (size_t)nData;` |
|       4 | 2358 | `	sMsg.msg_name = (void *)pName;` |
|       4 | 2359 | `	sMsg.msg_namelen = pName ? *pnName : 0;` |
|       4 | 2360 | `	sMsg.msg_iov = &sVec;` |
|       4 | 2361 | `	sMsg.msg_iovlen = 1;` |
|       4 | 2362 | `	sMsg.msg_control = zCtl;` |
|       4 | 2363 | `	sMsg.msg_controllen = nCtl;` |
|       4 | 2364 | `	iGot = (int)recvmsg(sock,&sMsg,iFlags);` |
|       4 | 2365 | `	if( iGot < 0 ){` |
|     ! 0 | 2366 | `		return -1;` |
|       - | 2367 | `	}` |
|       4 | 2368 | `	if( pName ){` |
|       2 | 2369 | `		*pnName = (ph7_socklen)sMsg.msg_namelen;` |
|       1 | 2370 | `	}` |
|       4 | 2371 | `	*pnCtlOut = (unsigned int)sMsg.msg_controllen;` |
|       4 | 2372 | `	*piFlagsOut = sMsg.msg_flags;` |
|       4 | 2373 | `	return iGot;` |
|       - | 2374 | `#endif` |
|       3 | 2375 | `}` |
|       - | 2376 | `/*` |
|       - | 2377 | ` * Walk a control block by hand. The layout rule is the same on both platforms` |
|       - | 2378 | ` * -- each header occupies PHL_CMSG_SPACE(payload) from its own start -- so this` |
|       - | 2379 | ` * replaces the FIRSTHDR/NXTHDR macros, which take a msghdr the two do not share.` |
|       - | 2380 | ` */` |
|       4 | 2381 | `static phl_cmsghdr * SockCmsgWalk(char *zBuf,unsigned int nBuf,unsigned int *pOfft)` |
|       1 | 2382 | `{` |
|       - | 2383 | `	phl_cmsghdr *pHdr;` |
|       5 | 2384 | `	unsigned int nHdr = (unsigned int)PHL_CMSG_LEN(0);` |
|       5 | 2385 | `	if( zBuf == 0 \|\| *pOfft + nHdr > nBuf ){` |
|       5 | 2386 | `		return 0;` |
|       - | 2387 | `	}` |
|     ! 0 | 2388 | `	pHdr = (phl_cmsghdr *)(zBuf + *pOfft);` |
|     ! 0 | 2389 | `	if( (unsigned int)pHdr->cmsg_len < nHdr \|\| *pOfft + (unsigned int)pHdr->cmsg_len > nBuf ){` |
|     ! 0 | 2390 | `		return 0;` |
|       - | 2391 | `	}` |
|     ! 0 | 2392 | `	*pOfft += (unsigned int)PHL_CMSG_SPACE((unsigned int)pHdr->cmsg_len - nHdr);` |
|     ! 0 | 2393 | `	return pHdr;` |
|       3 | 2394 | `}` |
|       - | 2395 | `/* php's one wording for every msghdr conversion failure, with the PATH in it. */` |
|       6 | 2396 | `static void SockMsgErr(ph7_context *pCtx,const char *zPath,const char *zWhat)` |
|       1 | 2397 | `{` |
|      10 | 2398 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       3 | 2399 | `		"error converting user data (path: %s): %s",zPath,zWhat);` |
|       7 | 2400 | `}` |
|       - | 2401 | `/*` |
|       - | 2402 | `` * php's `name` entry, written. The FAMILY is the socket's unless the array`` |
|       - | 2403 | `` * states one, and every field is optional -- an address with no `port` is`` |
|       - | 2404 | ` * simply port 0, which is why php's own answer for it is the kernel's EINVAL` |
|       - | 2405 | ` * rather than a refusal of its own.` |
|       - | 2406 | ` */` |
|       4 | 2407 | `static int SockMsgWriteName(ph7_context *pCtx,phl_socket *pSock,ph7_value *pVal,` |
|       - | 2408 | `	struct sockaddr_storage *pOut,ph7_socklen *pLen)` |
|       1 | 2409 | `{` |
|       - | 2410 | `	ph7_value *pElem;` |
|       5 | 2411 | `	int iFamily = pSock->iDomain;` |
|       5 | 2412 | `	if( !ph7_value_is_array(pVal) ){` |
|     ! 0 | 2413 | `		SockMsgErr(pCtx,"msghdr > name","expected an array here");` |
|     ! 0 | 2414 | `		return -1;` |
|       - | 2415 | `	}` |
|       5 | 2416 | `	SyZero(pOut,sizeof(*pOut));` |
|       5 | 2417 | `	pElem = ph7_array_fetch(pVal,"family",sizeof("family")-1);` |
|       5 | 2418 | `	if( pElem && !ph7_value_is_null(pElem) ){` |
|     ! 0 | 2419 | `		iFamily = (int)ph7_value_to_int64(pElem);` |
|     ! 0 | 2420 | `	}` |
|       5 | 2421 | `	if( iFamily == AF_UNIX ){` |
|     ! 0 | 2422 | `		struct sockaddr_un *pUn = (struct sockaddr_un *)pOut;` |
|     ! 0 | 2423 | `		pUn->sun_family = AF_UNIX;` |
|     ! 0 | 2424 | `		pElem = ph7_array_fetch(pVal,"path",sizeof("path")-1);` |
|     ! 0 | 2425 | `		if( pElem ){` |
|     ! 0 | 2426 | `			int n = 0;` |
|     ! 0 | 2427 | `			const char *z = ph7_value_to_string(pElem,&n);` |
|     ! 0 | 2428 | `			if( n >= (int)sizeof(pUn->sun_path) ){` |
|     ! 0 | 2429 | `				SockMsgErr(pCtx,"msghdr > name > path","path too long");` |
|     ! 0 | 2430 | `				return -1;` |
|       - | 2431 | `			}` |
|     ! 0 | 2432 | `			SyMemcpy(z,pUn->sun_path,(sxu32)n);` |
|     ! 0 | 2433 | `		}` |
|     ! 0 | 2434 | `		*pLen = (ph7_socklen)sizeof(struct sockaddr_un);` |
|     ! 0 | 2435 | `		return 0;` |
|       - | 2436 | `	}` |
|       5 | 2437 | `	if( iFamily == AF_INET6 ){` |
|     ! 0 | 2438 | `		struct sockaddr_in6 *pIn6 = (struct sockaddr_in6 *)pOut;` |
|     ! 0 | 2439 | `		pIn6->sin6_family = AF_INET6;` |
|     ! 0 | 2440 | `		pElem = ph7_array_fetch(pVal,"addr",sizeof("addr")-1);` |
|     ! 0 | 2441 | `		if( pElem ){` |
|       - | 2442 | `			char zHost[NI_MAXHOST+1];` |
|     ! 0 | 2443 | `			int n = 0;` |
|     ! 0 | 2444 | `			const char *z = ph7_value_to_string(pElem,&n);` |
|     ! 0 | 2445 | `			if( n < 0 \|\| n >= (int)sizeof(zHost) ){` |
|     ! 0 | 2446 | `				SockMsgErr(pCtx,"msghdr > name > addr","expected a string");` |
|     ! 0 | 2447 | `				return -1;` |
|       - | 2448 | `			}` |
|     ! 0 | 2449 | `			SyMemcpy(z,zHost,(sxu32)n);` |
|     ! 0 | 2450 | `			zHost[n] = 0;` |
|     ! 0 | 2451 | `			if( inet_pton(AF_INET6,zHost,&pIn6->sin6_addr) != 1 ){` |
|     ! 0 | 2452 | `				SockMsgErr(pCtx,"msghdr > name > addr","expected a valid IPv6 address");` |
|     ! 0 | 2453 | `				return -1;` |
|       - | 2454 | `			}` |
|     ! 0 | 2455 | `		}` |
|     ! 0 | 2456 | `		pElem = ph7_array_fetch(pVal,"port",sizeof("port")-1);` |
|     ! 0 | 2457 | `		if( pElem ){` |
|     ! 0 | 2458 | `			pIn6->sin6_port = htons((unsigned short)ph7_value_to_int64(pElem));` |
|     ! 0 | 2459 | `		}` |
|     ! 0 | 2460 | `		*pLen = (ph7_socklen)sizeof(struct sockaddr_in6);` |
|     ! 0 | 2461 | `		return 0;` |
|       - | 2462 | `	}` |
|       - | 2463 | `	{` |
|       5 | 2464 | `		struct sockaddr_in *pIn = (struct sockaddr_in *)pOut;` |
|       5 | 2465 | `		pIn->sin_family = AF_INET;` |
|       5 | 2466 | `		pElem = ph7_array_fetch(pVal,"addr",sizeof("addr")-1);` |
|       5 | 2467 | `		if( pElem ){` |
|       - | 2468 | `			char zHost[NI_MAXHOST+1];` |
|       5 | 2469 | `			int n = 0;` |
|       5 | 2470 | `			const char *z = ph7_value_to_string(pElem,&n);` |
|       5 | 2471 | `			if( n < 0 \|\| n >= (int)sizeof(zHost) ){` |
|     ! 0 | 2472 | `				SockMsgErr(pCtx,"msghdr > name > addr","expected a string");` |
|     ! 0 | 2473 | `				return -1;` |
|       - | 2474 | `			}` |
|       5 | 2475 | `			SyMemcpy(z,zHost,(sxu32)n);` |
|       5 | 2476 | `			zHost[n] = 0;` |
|       5 | 2477 | `			if( inet_pton(AF_INET,zHost,&pIn->sin_addr) != 1 ){` |
|     ! 0 | 2478 | `				SockMsgErr(pCtx,"msghdr > name > addr","expected a valid IPv4 address");` |
|     ! 0 | 2479 | `				return -1;` |
|       - | 2480 | `			}` |
|       2 | 2481 | `		}` |
|       5 | 2482 | `		pElem = ph7_array_fetch(pVal,"port",sizeof("port")-1);` |
|       5 | 2483 | `		if( pElem ){` |
|       5 | 2484 | `			pIn->sin_port = htons((unsigned short)ph7_value_to_int64(pElem));` |
|       2 | 2485 | `		}` |
|       5 | 2486 | `		*pLen = (ph7_socklen)sizeof(struct sockaddr_in);` |
|       - | 2487 | `	}` |
|       5 | 2488 | `	return 0;` |
|       3 | 2489 | `}` |
|       - | 2490 | `/* ...and read back, which is the shape php reports rather than the one it takes. */` |
|       4 | 2491 | `static void SockMsgReadName(ph7_context *pCtx,const struct sockaddr *pAddr,ph7_socklen nAddr,` |
|       - | 2492 | `	ph7_value *pOut)` |
|       1 | 2493 | `{` |
|       - | 2494 | `	ph7_value *pArr,*pTmp;` |
|       - | 2495 | `	char zBuf[INET6_ADDRSTRLEN+1];` |
|       5 | 2496 | `	if( nAddr == 0 \|\| pAddr->sa_family == 0 ){` |
|       3 | 2497 | `		ph7_value_null(pOut);` |
|       3 | 2498 | `		return;` |
|       - | 2499 | `	}` |
|       3 | 2500 | `	pArr = ph7_context_new_array(pCtx);` |
|       3 | 2501 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|       3 | 2502 | `	if( pArr == 0 \|\| pTmp == 0 ){` |
|     ! 0 | 2503 | `		ph7_value_null(pOut);` |
|     ! 0 | 2504 | `		return;` |
|       - | 2505 | `	}` |
|       3 | 2506 | `	ph7_value_int64(pTmp,pAddr->sa_family);` |
|       3 | 2507 | `	ph7_array_add_strkey_elem(pArr,"family",pTmp);` |
|       3 | 2508 | `	zBuf[0] = 0;` |
|       3 | 2509 | `	if( pAddr->sa_family == AF_UNIX ){` |
|     ! 0 | 2510 | `		const struct sockaddr_un *pUn = (const struct sockaddr_un *)pAddr;` |
|     ! 0 | 2511 | `		ph7_value_string(pTmp,pUn->sun_path,(int)SyStrlen(pUn->sun_path));` |
|     ! 0 | 2512 | `		ph7_array_add_strkey_elem(pArr,"path",pTmp);` |
|       3 | 2513 | `	}else if( pAddr->sa_family == AF_INET6 ){` |
|     ! 0 | 2514 | `		const struct sockaddr_in6 *pIn6 = (const struct sockaddr_in6 *)pAddr;` |
|     ! 0 | 2515 | `		inet_ntop(AF_INET6,(const void *)&pIn6->sin6_addr,zBuf,sizeof(zBuf));` |
|     ! 0 | 2516 | `		ph7_value_string(pTmp,zBuf,-1);` |
|     ! 0 | 2517 | `		ph7_array_add_strkey_elem(pArr,"addr",pTmp);` |
|     ! 0 | 2518 | `		ph7_value_int64(pTmp,(ph7_int64)ntohs(pIn6->sin6_port));` |
|     ! 0 | 2519 | `		ph7_array_add_strkey_elem(pArr,"port",pTmp);` |
|     ! 0 | 2520 | `	}else{` |
|       3 | 2521 | `		const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;` |
|       3 | 2522 | `		inet_ntop(AF_INET,(const void *)&pIn->sin_addr,zBuf,sizeof(zBuf));` |
|       3 | 2523 | `		ph7_value_string(pTmp,zBuf,-1);` |
|       3 | 2524 | `		ph7_array_add_strkey_elem(pArr,"addr",pTmp);` |
|       3 | 2525 | `		ph7_value_int64(pTmp,(ph7_int64)ntohs(pIn->sin_port));` |
|       3 | 2526 | `		ph7_array_add_strkey_elem(pArr,"port",pTmp);` |
|       - | 2527 | `	}` |
|       3 | 2528 | `	PH7_MemObjStore(pArr,pOut);` |
|       3 | 2529 | `	ph7_context_release_value(pCtx,pTmp);` |
|       3 | 2530 | `}` |
|       - | 2531 | `/*` |
|       - | 2532 | `` * The `iov` array, gathered into ONE buffer. The wire cannot tell the two`` |
|       - | 2533 | ` * apart -- a sendmsg() is one datagram (or one contiguous run of stream bytes)` |
|       - | 2534 | ` * whatever its iovlen -- and the copy is not an optimization but a necessity:` |
|       - | 2535 | ` * the ph7_value a walker is handed is a TEMPORARY, so a pointer into its string` |
|       - | 2536 | ` * is dangling by the time the syscall runs. Gathering by hand made a two-piece` |
|       - | 2537 | `` * message arrive as `abccon` instead of `abcdef`.`` |
|       - | 2538 | ` */` |
|       - | 2539 | `typedef struct SockIovCtx SockIovCtx;` |
|       - | 2540 | `struct SockIovCtx` |
|       - | 2541 | `{` |
|       - | 2542 | `	SyBlob sBuf;` |
|       - | 2543 | `	int bInit;` |
|       - | 2544 | `};` |
|       8 | 2545 | `static int SockIovAdd(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|       1 | 2546 | `{` |
|       9 | 2547 | `	SockIovCtx *p = (SockIovCtx *)pUserData;` |
|       9 | 2548 | `	int n = 0;` |
|       - | 2549 | `	const char *z;` |
|       4 | 2550 | `	SXUNUSED(pKey);` |
|       9 | 2551 | `	z = ph7_value_to_string(pVal,&n);` |
|       9 | 2552 | `	if( n > 0 ){` |
|       9 | 2553 | `		SyBlobAppend(&p->sBuf,z,(sxu32)n);` |
|       4 | 2554 | `	}` |
|       9 | 2555 | `	return PH7_OK;` |
|       1 | 2556 | `}` |
|       - | 2557 | `/*` |
|       - | 2558 | `` * The `control` array, in two passes over the same elements: one that adds up`` |
|       - | 2559 | ` * how much room the whole block needs (and refuses a pair php has no converter` |
|       - | 2560 | ` * for) and one that writes the headers into it.` |
|       - | 2561 | ` */` |
|       - | 2562 | `typedef struct SockCtlCtx SockCtlCtx;` |
|       - | 2563 | `struct SockCtlCtx` |
|       - | 2564 | `{` |
|       - | 2565 | `	ph7_context *pCtx;` |
|       - | 2566 | `	char *zBuf;` |
|       - | 2567 | `	unsigned int nTotal;` |
|       - | 2568 | `	unsigned int nOfft;` |
|       - | 2569 | `	unsigned int iIndex;` |
|       - | 2570 | `	int bBad;` |
|       - | 2571 | `};` |
|       - | 2572 | `/* How many bytes ONE entry's payload takes, or -1 for a pair php cannot convert. */` |
|     ! 0 | 2573 | `static int SockCtlEntrySize(ph7_value *pEnt,int *piLvl,int *piTyp,unsigned int *pnData)` |
|     ! 0 | 2574 | `{` |
|       - | 2575 | `	ph7_value *pLvl,*pTyp,*pDat;` |
|     ! 0 | 2576 | `	unsigned int nFixed = 0,nUnit = 0;` |
|     ! 0 | 2577 | `	if( pEnt == 0 \|\| !ph7_value_is_array(pEnt) ){` |
|     ! 0 | 2578 | `		return -1;` |
|       - | 2579 | `	}` |
|     ! 0 | 2580 | `	pLvl = ph7_array_fetch(pEnt,"level",sizeof("level")-1);` |
|     ! 0 | 2581 | `	pTyp = ph7_array_fetch(pEnt,"type",sizeof("type")-1);` |
|     ! 0 | 2582 | `	pDat = ph7_array_fetch(pEnt,"data",sizeof("data")-1);` |
|     ! 0 | 2583 | `	*piLvl = pLvl ? (int)ph7_value_to_int64(pLvl) : 0;` |
|     ! 0 | 2584 | `	*piTyp = pTyp ? (int)ph7_value_to_int64(pTyp) : 0;` |
|     ! 0 | 2585 | `	if( pLvl == 0 \|\| pTyp == 0 \|\| SockCmsgEntry(*piLvl,*piTyp,&nFixed,&nUnit) != 0 ){` |
|     ! 0 | 2586 | `		return -1;` |
|       - | 2587 | `	}` |
|     ! 0 | 2588 | `	*pnData = nFixed;` |
|     ! 0 | 2589 | `	if( nUnit > 0 && pDat && ph7_value_is_array(pDat) ){` |
|     ! 0 | 2590 | `		*pnData += nUnit * ph7_array_count(pDat);` |
|     ! 0 | 2591 | `	}` |
|     ! 0 | 2592 | `	return 0;` |
|     ! 0 | 2593 | `}` |
|     ! 0 | 2594 | `static int SockCtlSize(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     ! 0 | 2595 | `{` |
|     ! 0 | 2596 | `	SockCtlCtx *p = (SockCtlCtx *)pUserData;` |
|     ! 0 | 2597 | `	unsigned int nData = 0;` |
|     ! 0 | 2598 | `	int iLvl = 0,iTyp = 0;` |
|     ! 0 | 2599 | `	SXUNUSED(pKey);` |
|     ! 0 | 2600 | `	if( SockCtlEntrySize(pVal,&iLvl,&iTyp,&nData) != 0 ){` |
|     ! 0 | 2601 | `		p->bBad = 1;` |
|     ! 0 | 2602 | `		ph7_context_throw_error_format(p->pCtx,PH7_CTX_WARNING,` |
|       - | 2603 | `			"error converting user data (path: msghdr > control > element #%u): "` |
|     ! 0 | 2604 | `			"cmsghdr with level %d and type %d not supported",p->iIndex,iLvl,iTyp);` |
|     ! 0 | 2605 | `		return PH7_ABORT;` |
|       - | 2606 | `	}` |
|     ! 0 | 2607 | `	p->nTotal += (unsigned int)PHL_CMSG_SPACE(nData);` |
|     ! 0 | 2608 | `	p->iIndex++;` |
|     ! 0 | 2609 | `	return PH7_OK;` |
|     ! 0 | 2610 | `}` |
|       - | 2611 | ``/* One descriptor per element of an SCM_RIGHTS `data` array. */`` |
|       - | 2612 | `typedef struct SockFdCtx SockFdCtx;` |
|       - | 2613 | `struct SockFdCtx { int *aFd; unsigned int nUsed; unsigned int nMax; };` |
|     ! 0 | 2614 | `static int SockCtlFd(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|       - | 2615 | `{` |
|     ! 0 | 2616 | `	SockFdCtx *p = (SockFdCtx *)pUserData;` |
|       - | 2617 | `	ph7_class_instance *pObj;` |
|       - | 2618 | `	phl_socket *pRec;` |
|     ! 0 | 2619 | `	SXUNUSED(pKey);` |
|     ! 0 | 2620 | `	if( p->nUsed >= p->nMax ){` |
|     ! 0 | 2621 | `		return PH7_ABORT;` |
|       - | 2622 | `	}` |
|     ! 0 | 2623 | `	pObj = ph7_value_is_object(pVal) ? (ph7_class_instance *)pVal->x.pOther : 0;` |
|     ! 0 | 2624 | `	pRec = (phl_socket *)SockSlotOf(pObj);` |
|     ! 0 | 2625 | `	p->aFd[p->nUsed++] = pRec ? (int)pRec->sock : -1;` |
|     ! 0 | 2626 | `	return PH7_OK;` |
|     ! 0 | 2627 | `}` |
|     ! 0 | 2628 | `static int SockCtlFill(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     ! 0 | 2629 | `{` |
|     ! 0 | 2630 | `	SockCtlCtx *p = (SockCtlCtx *)pUserData;` |
|       - | 2631 | `	struct cmsghdr *pHdr;` |
|       - | 2632 | `	ph7_value *pDat;` |
|     ! 0 | 2633 | `	unsigned int nData = 0;` |
|     ! 0 | 2634 | `	int iLvl = 0,iTyp = 0;` |
|     ! 0 | 2635 | `	SXUNUSED(pKey);` |
|     ! 0 | 2636 | `	if( SockCtlEntrySize(pVal,&iLvl,&iTyp,&nData) != 0 ){` |
|     ! 0 | 2637 | `		return PH7_ABORT;` |
|       - | 2638 | `	}` |
|     ! 0 | 2639 | `	pHdr = (struct cmsghdr *)(p->zBuf + p->nOfft);` |
|     ! 0 | 2640 | `	pHdr->cmsg_level = iLvl;` |
|     ! 0 | 2641 | `	pHdr->cmsg_type = iTyp;` |
|     ! 0 | 2642 | `	pHdr->cmsg_len = PHL_CMSG_LEN(nData);` |
|     ! 0 | 2643 | `	pDat = ph7_array_fetch(pVal,"data",sizeof("data")-1);` |
|       - | 2644 | `#ifdef SCM_RIGHTS` |
|     ! 0 | 2645 | `	if( iLvl == SOL_SOCKET && iTyp == SCM_RIGHTS && pDat && ph7_value_is_array(pDat) ){` |
|       - | 2646 | `		SockFdCtx sFd;` |
|     ! 0 | 2647 | `		sFd.aFd = (int *)CMSG_DATA(pHdr);` |
|     ! 0 | 2648 | `		sFd.nUsed = 0;` |
|     ! 0 | 2649 | `		sFd.nMax = ph7_array_count(pDat);` |
|     ! 0 | 2650 | `		ph7_array_walk(pDat,SockCtlFd,(void *)&sFd);` |
|     ! 0 | 2651 | `	}` |
|       - | 2652 | `#endif` |
|       - | 2653 | `#ifdef SCM_CREDENTIALS` |
|     ! 0 | 2654 | `	if( iLvl == SOL_SOCKET && iTyp == SCM_CREDENTIALS && pDat && ph7_value_is_array(pDat) ){` |
|       - | 2655 | `		struct ucred sCred;` |
|       - | 2656 | `		ph7_value *pF;` |
|     ! 0 | 2657 | `		SyZero(&sCred,sizeof(sCred));` |
|     ! 0 | 2658 | `		pF = ph7_array_fetch(pDat,"pid",sizeof("pid")-1);` |
|     ! 0 | 2659 | `		sCred.pid = pF ? (pid_t)ph7_value_to_int64(pF) : 0;` |
|     ! 0 | 2660 | `		pF = ph7_array_fetch(pDat,"uid",sizeof("uid")-1);` |
|     ! 0 | 2661 | `		sCred.uid = pF ? (uid_t)ph7_value_to_int64(pF) : 0;` |
|     ! 0 | 2662 | `		pF = ph7_array_fetch(pDat,"gid",sizeof("gid")-1);` |
|     ! 0 | 2663 | `		sCred.gid = pF ? (gid_t)ph7_value_to_int64(pF) : 0;` |
|     ! 0 | 2664 | `		SyMemcpy((const void *)&sCred,(void *)PHL_CMSG_DATA(pHdr),sizeof(sCred));` |
|       - | 2665 | `	}` |
|       - | 2666 | `#endif` |
|     ! 0 | 2667 | `	p->nOfft += (unsigned int)PHL_CMSG_SPACE(nData);` |
|     ! 0 | 2668 | `	return PH7_OK;` |
|     ! 0 | 2669 | `}` |
|       - | 2670 | `/* int\|false socket_sendmsg(Socket $socket, array $message, int $flags = 0) */` |
|       6 | 2671 | `static int vm_builtin_socket_sendmsg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2672 | `{` |
|       - | 2673 | `	struct sockaddr_storage sAddr;` |
|       7 | 2674 | `	struct sockaddr *pName = 0;` |
|       7 | 2675 | `	ph7_socklen nName = 0;` |
|       - | 2676 | `	SockIovCtx sIov;` |
|       - | 2677 | `	SockCtlCtx sCtl;` |
|       - | 2678 | `	phl_socket *pSock;` |
|       - | 2679 | `	ph7_value *pElem;` |
|       7 | 2680 | `	char *zCtl = 0;` |
|       7 | 2681 | `	unsigned int nCtl = 0;` |
|       - | 2682 | `	int rc,iSent;` |
|       7 | 2683 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|       7 | 2684 | `	if( pSock == 0 ){` |
|     ! 0 | 2685 | `		return rc;` |
|       - | 2686 | `	}` |
|       7 | 2687 | `	SyZero(&sIov,sizeof(sIov));` |
|       7 | 2688 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     ! 0 | 2689 | `		SockMsgErr(pCtx,"msghdr","expected an array here");` |
|     ! 0 | 2690 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2691 | `		return PH7_OK;` |
|       - | 2692 | `	}` |
|       7 | 2693 | `	pElem = ph7_array_fetch(apArg[1],"name",sizeof("name")-1);` |
|       7 | 2694 | `	if( pElem ){` |
|       5 | 2695 | `		if( SockMsgWriteName(pCtx,pSock,pElem,&sAddr,&nName) != 0 ){` |
|     ! 0 | 2696 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2697 | `			return PH7_OK;` |
|       - | 2698 | `		}` |
|       5 | 2699 | `		pName = (struct sockaddr *)&sAddr;` |
|       2 | 2700 | `	}` |
|       7 | 2701 | `	pElem = ph7_array_fetch(apArg[1],"iov",sizeof("iov")-1);` |
|       7 | 2702 | `	if( pElem ){` |
|       7 | 2703 | `		if( !ph7_value_is_array(pElem) ){` |
|       3 | 2704 | `			SockMsgErr(pCtx,"msghdr > iov","expected an array here");` |
|       3 | 2705 | `			ph7_result_bool(pCtx,0);` |
|       3 | 2706 | `			return PH7_OK;` |
|       - | 2707 | `		}` |
|       5 | 2708 | `		SyBlobInit(&sIov.sBuf,&pCtx->pVm->sAllocator);` |
|       5 | 2709 | `		sIov.bInit = 1;` |
|       5 | 2710 | `		ph7_array_walk(pElem,SockIovAdd,(void *)&sIov);` |
|       2 | 2711 | `	}` |
|       5 | 2712 | `	pElem = ph7_array_fetch(apArg[1],"control",sizeof("control")-1);` |
|       5 | 2713 | `	if( pElem ){` |
|       - | 2714 | `		/* One cmsghdr per element, laid out end to end in one buffer: the SIZE` |
|       - | 2715 | `		 * pass runs first because the whole block has to be allocated before` |
|       - | 2716 | `		 * any header can be written into it. */` |
|     ! 0 | 2717 | `		if( !ph7_value_is_array(pElem) ){` |
|     ! 0 | 2718 | `			SockMsgErr(pCtx,"msghdr > control","expected an array here");` |
|     ! 0 | 2719 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2720 | `			goto cleanup;` |
|       - | 2721 | `		}` |
|     ! 0 | 2722 | `		SyZero(&sCtl,sizeof(sCtl));` |
|     ! 0 | 2723 | `		sCtl.pCtx = pCtx;` |
|     ! 0 | 2724 | `		ph7_array_walk(pElem,SockCtlSize,(void *)&sCtl);` |
|     ! 0 | 2725 | `		if( sCtl.bBad ){` |
|     ! 0 | 2726 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2727 | `			goto cleanup;` |
|       - | 2728 | `		}` |
|     ! 0 | 2729 | `		if( sCtl.nTotal > 0 ){` |
|     ! 0 | 2730 | `			zCtl = (char *)ph7_context_alloc_chunk(pCtx,sCtl.nTotal,TRUE,FALSE);` |
|     ! 0 | 2731 | `			if( zCtl == 0 ){` |
|     ! 0 | 2732 | `				rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 | 2733 | `				goto cleanup;` |
|       - | 2734 | `			}` |
|     ! 0 | 2735 | `			nCtl = sCtl.nTotal;` |
|     ! 0 | 2736 | `			sCtl.zBuf = zCtl;` |
|     ! 0 | 2737 | `			sCtl.nOfft = 0;` |
|     ! 0 | 2738 | `			ph7_array_walk(pElem,SockCtlFill,(void *)&sCtl);` |
|     ! 0 | 2739 | `		}` |
|     ! 0 | 2740 | `	}` |
|      13 | 2741 | `	iSent = SockMsgSend(pSock->sock,pName,nName,` |
|       4 | 2742 | `		sIov.bInit ? (const char *)SyBlobData(&sIov.sBuf) : 0,` |
|       4 | 2743 | `		sIov.bInit ? SyBlobLength(&sIov.sBuf) : 0,` |
|       4 | 2744 | `		zCtl,nCtl,nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : 0);` |
|       5 | 2745 | `	if( iSent < 0 ){` |
|     ! 0 | 2746 | `		SockFailLast(pCtx,pSock,"Error in sendmsg");` |
|     ! 0 | 2747 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2748 | `	}else{` |
|       5 | 2749 | `		ph7_result_int64(pCtx,iSent);` |
|       - | 2750 | `	}` |
|       5 | 2751 | `	rc = PH7_OK;` |
|       2 | 2752 | `cleanup:` |
|       5 | 2753 | `	if( sIov.bInit ){` |
|       5 | 2754 | `		SyBlobRelease(&sIov.sBuf);` |
|       2 | 2755 | `	}` |
|       5 | 2756 | `	if( zCtl ){` |
|     ! 0 | 2757 | `		ph7_context_free_chunk(pCtx,zCtl);` |
|     ! 0 | 2758 | `	}` |
|       5 | 2759 | `	return rc;` |
|       4 | 2760 | `}` |
|       - | 2761 | `/* int\|false socket_recvmsg(Socket $socket, array &$message, int $flags = 0) */` |
|       8 | 2762 | `static int vm_builtin_socket_recvmsg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2763 | `{` |
|       - | 2764 | `	struct sockaddr_storage sAddr;` |
|       9 | 2765 | `	struct sockaddr *pName = 0;` |
|       9 | 2766 | `	ph7_socklen nName = (ph7_socklen)sizeof(sAddr);` |
|       - | 2767 | `	phl_cmsghdr *pHdr;` |
|       - | 2768 | `	phl_socket *pSock;` |
|       - | 2769 | `	ph7_value *pElem,*pOut,*pTmp,*pCtl;` |
|       9 | 2770 | `	char *zBuf = 0,*zCtl = 0;` |
|       9 | 2771 | `	unsigned int nCtlOut = 0,nWalk = 0;` |
|       9 | 2772 | `	ph7_int64 iBufSize = 8192,iCtlLen;` |
|       9 | 2773 | `	int rc,iRead,iFlagsOut = 0;` |
|       9 | 2774 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|       9 | 2775 | `	if( pSock == 0 ){` |
|     ! 0 | 2776 | `		return rc;` |
|       - | 2777 | `	}` |
|       9 | 2778 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     ! 0 | 2779 | `		SockMsgErr(pCtx,"msghdr","expected an array here");` |
|     ! 0 | 2780 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2781 | `		return PH7_OK;` |
|       - | 2782 | `	}` |
|       9 | 2783 | `	pElem = ph7_array_fetch(apArg[1],"controllen",sizeof("controllen")-1);` |
|       9 | 2784 | `	if( pElem == 0 ){` |
|       3 | 2785 | `		SockMsgErr(pCtx,"msghdr","The key 'controllen' is required");` |
|       3 | 2786 | `		ph7_result_bool(pCtx,0);` |
|       3 | 2787 | `		return PH7_OK;` |
|       - | 2788 | `	}` |
|       7 | 2789 | `	iCtlLen = ph7_value_to_int64(pElem);` |
|       7 | 2790 | `	if( iCtlLen < 0 \|\| iCtlLen > 0xFFFFFFFF ){` |
|       - | 2791 | `		/* php reads this one as an unsigned 32-bit field and says so. */` |
|     ! 0 | 2792 | `		SockMsgErr(pCtx,"msghdr > controllen",` |
|       - | 2793 | `			"given PHP integer is out of bounds for an unsigned 32-bit integer");` |
|     ! 0 | 2794 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2795 | `		return PH7_OK;` |
|       - | 2796 | `	}` |
|       7 | 2797 | `	if( iCtlLen == 0 ){` |
|       3 | 2798 | `		SockMsgErr(pCtx,"msghdr > controllen","controllen cannot be 0");` |
|       3 | 2799 | `		ph7_result_bool(pCtx,0);` |
|       3 | 2800 | `		return PH7_OK;` |
|       - | 2801 | `	}` |
|       5 | 2802 | `	if( iCtlLen > PHL_SOCK_MAXBUF ){` |
|     ! 0 | 2803 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 2804 | `	}` |
|       5 | 2805 | `	pElem = ph7_array_fetch(apArg[1],"buffer_size",sizeof("buffer_size")-1);` |
|       5 | 2806 | `	if( pElem ){` |
|       5 | 2807 | `		iBufSize = ph7_value_to_int64(pElem);` |
|       - | 2808 | `		/* php's own range, and its own off-by-one wording: the message says` |
|       - | 2809 | `		 * "between 1 and", and 0 is taken. */` |
|       5 | 2810 | `		if( iBufSize < 0 \|\| iBufSize > PHL_SOCK_MSGBUF ){` |
|     ! 0 | 2811 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 2812 | `				"error converting user data (path: msghdr > buffer_size): "` |
|       - | 2813 | `				"the buffer size must be between 1 and %d; given %qd",` |
|     ! 0 | 2814 | `				(int)PHL_SOCK_MSGBUF,iBufSize);` |
|     ! 0 | 2815 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2816 | `			return PH7_OK;` |
|       - | 2817 | `		}` |
|       2 | 2818 | `	}` |
|       5 | 2819 | `	SyZero(&sAddr,sizeof(sAddr));` |
|       - | 2820 | ``	/* A `buffer_size` of 0 is TAKEN by php (its refusal starts at -1, whatever`` |
|       - | 2821 | `	 * the message says), and a zero-byte request is not something the backend` |
|       - | 2822 | `	 * has to answer -- so the ask is one byte and the READ is still zero. */` |
|       7 | 2823 | `	zBuf = (char *)ph7_context_alloc_chunk(pCtx,` |
|       2 | 2824 | `		(unsigned int)(iBufSize > 0 ? iBufSize : 1),TRUE,FALSE);` |
|       5 | 2825 | `	zCtl = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)iCtlLen,TRUE,FALSE);` |
|       5 | 2826 | `	if( zBuf == 0 \|\| zCtl == 0 ){` |
|     ! 0 | 2827 | `		if( zBuf ){ ph7_context_free_chunk(pCtx,zBuf); }` |
|     ! 0 | 2828 | `		if( zCtl ){ ph7_context_free_chunk(pCtx,zCtl); }` |
|     ! 0 | 2829 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 2830 | `	}` |
|       - | 2831 | `	/*` |
|       - | 2832 | `	 * php allocates room for the peer's address only when the caller's array` |
|       - | 2833 | ``	 * ALREADY carries a `name` entry -- so a recvmsg() on a datagram socket`` |
|       - | 2834 | ``	 * reports `name => null` unless it was asked for one. That looks like an`` |
|       - | 2835 | `	 * oversight and is the answer both engines have to give.` |
|       - | 2836 | `	 */` |
|       5 | 2837 | `	if( ph7_array_fetch(apArg[1],"name",sizeof("name")-1) != 0 ){` |
|       3 | 2838 | `		pName = (struct sockaddr *)&sAddr;` |
|       1 | 2839 | `	}` |
|       9 | 2840 | `	iRead = SockMsgRecv(pSock->sock,pName,&nName,zBuf,(unsigned int)iBufSize,` |
|       2 | 2841 | `		zCtl,(unsigned int)iCtlLen,&nCtlOut,` |
|       4 | 2842 | `		nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : 0,&iFlagsOut);` |
|       5 | 2843 | `	if( iRead < 0 ){` |
|       - | 2844 | `		/*` |
|       - | 2845 | `		 * The ONE verb in this extension that does not report through php's` |
|       - | 2846 | `		 * PHP_SOCKET_ERROR: it stores the code only in the per-request slot,` |
|       - | 2847 | `		 * leaves the socket's own alone, and warns even for EAGAIN -- so a` |
|       - | 2848 | `		 * non-blocking recvmsg() with nothing waiting PRINTS where a` |
|       - | 2849 | `		 * non-blocking recv() is silent.` |
|       - | 2850 | `		 */` |
|     ! 0 | 2851 | `		pCtx->pVm->iSocketLastErr = PH7_NetLastError();` |
|     ! 0 | 2852 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Error in recvmsg [%d]: %s",` |
|     ! 0 | 2853 | `			pCtx->pVm->iSocketLastErr,PH7_SocketStrError(pCtx->pVm->iSocketLastErr));` |
|     ! 0 | 2854 | `		ph7_context_free_chunk(pCtx,zBuf);` |
|     ! 0 | 2855 | `		ph7_context_free_chunk(pCtx,zCtl);` |
|     ! 0 | 2856 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2857 | `		return PH7_OK;` |
|       - | 2858 | `	}` |
|       5 | 2859 | `	pOut = ph7_context_new_array(pCtx);` |
|       5 | 2860 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|       5 | 2861 | `	pCtl = ph7_context_new_array(pCtx);` |
|       5 | 2862 | `	if( pOut == 0 \|\| pTmp == 0 \|\| pCtl == 0 ){` |
|     ! 0 | 2863 | `		ph7_context_free_chunk(pCtx,zBuf);` |
|     ! 0 | 2864 | `		ph7_context_free_chunk(pCtx,zCtl);` |
|     ! 0 | 2865 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 2866 | `	}` |
|       5 | 2867 | `	SockMsgReadName(pCtx,(const struct sockaddr *)&sAddr,pName ? nName : 0,pTmp);` |
|       5 | 2868 | `	ph7_array_add_strkey_elem(pOut,"name",pTmp);` |
|       5 | 2869 | `	while( (pHdr = SockCmsgWalk(zCtl,nCtlOut,&nWalk)) != 0 ){` |
|     ! 0 | 2870 | `		ph7_value *pEnt = ph7_context_new_array(pCtx);` |
|     ! 0 | 2871 | `		if( pEnt == 0 ){` |
|     ! 0 | 2872 | `			break;` |
|       - | 2873 | `		}` |
|     ! 0 | 2874 | `		ph7_value_int64(pTmp,pHdr->cmsg_level);` |
|     ! 0 | 2875 | `		ph7_array_add_strkey_elem(pEnt,"level",pTmp);` |
|     ! 0 | 2876 | `		ph7_value_int64(pTmp,pHdr->cmsg_type);` |
|     ! 0 | 2877 | `		ph7_array_add_strkey_elem(pEnt,"type",pTmp);` |
|       - | 2878 | `#ifdef SCM_RIGHTS` |
|     ! 0 | 2879 | `		if( pHdr->cmsg_level == SOL_SOCKET && pHdr->cmsg_type == SCM_RIGHTS ){` |
|       - | 2880 | `			/* Every descriptor that arrived becomes a Socket of its own. */` |
|     ! 0 | 2881 | `			unsigned int nFd = (unsigned int)(((unsigned int)pHdr->cmsg_len` |
|     ! 0 | 2882 | `				- (unsigned int)PHL_CMSG_LEN(0)) / sizeof(int));` |
|     ! 0 | 2883 | `			ph7_value *pFds = ph7_context_new_array(pCtx);` |
|       - | 2884 | `			unsigned int j;` |
|     ! 0 | 2885 | `			for( j = 0 ; pFds && j < nFd ; ++j ){` |
|       - | 2886 | `				int fd;` |
|       - | 2887 | `				ph7_class *pClass;` |
|       - | 2888 | `				ph7_class_instance *pThis;` |
|       - | 2889 | `				phl_socket *pRec;` |
|     ! 0 | 2890 | `				SyMemcpy((const void *)(PHL_CMSG_DATA(pHdr) + j * sizeof(int)),` |
|       - | 2891 | `					(void *)&fd,sizeof(int));` |
|     ! 0 | 2892 | `				pClass = PH7_VmExtractClass(pCtx->pVm,"Socket",sizeof("Socket")-1,0,0);` |
|     ! 0 | 2893 | `				pThis = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|     ! 0 | 2894 | `				pRec = pThis ? SockNew(pCtx->pVm,(ph7_socket)fd,AF_UNIX,SOCK_STREAM,0) : 0;` |
|     ! 0 | 2895 | `				if( pRec == 0 ){` |
|     ! 0 | 2896 | `					if( pThis ){` |
|     ! 0 | 2897 | `						PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2898 | `					}` |
|     ! 0 | 2899 | `					break;` |
|       - | 2900 | `				}` |
|     ! 0 | 2901 | `				pRec->pOwner = pThis;` |
|     ! 0 | 2902 | `				PH7_MemObjRelease(pTmp);` |
|     ! 0 | 2903 | `				pTmp->x.pOther = pThis;` |
|     ! 0 | 2904 | `				pTmp->iFlags = MEMOBJ_OBJ;` |
|     ! 0 | 2905 | `				ph7_array_add_elem(pFds,0,pTmp);` |
|     ! 0 | 2906 | `				PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2907 | `				pTmp->iFlags = MEMOBJ_NULL;` |
|     ! 0 | 2908 | `				pTmp->x.pOther = 0;` |
|     ! 0 | 2909 | `			}` |
|     ! 0 | 2910 | `			if( pFds ){` |
|     ! 0 | 2911 | `				ph7_array_add_strkey_elem(pEnt,"data",pFds);` |
|     ! 0 | 2912 | `			}` |
|     ! 0 | 2913 | `		}else` |
|       - | 2914 | `#endif` |
|       - | 2915 | `#ifdef SCM_CREDENTIALS` |
|     ! 0 | 2916 | `		if( pHdr->cmsg_level == SOL_SOCKET && pHdr->cmsg_type == SCM_CREDENTIALS ){` |
|       - | 2917 | `			struct ucred sCred;` |
|     ! 0 | 2918 | `			ph7_value *pCr = ph7_context_new_array(pCtx);` |
|     ! 0 | 2919 | `			SyMemcpy((const void *)PHL_CMSG_DATA(pHdr),(void *)&sCred,sizeof(sCred));` |
|     ! 0 | 2920 | `			if( pCr ){` |
|     ! 0 | 2921 | `				ph7_value_int64(pTmp,(ph7_int64)sCred.pid);` |
|     ! 0 | 2922 | `				ph7_array_add_strkey_elem(pCr,"pid",pTmp);` |
|     ! 0 | 2923 | `				ph7_value_int64(pTmp,(ph7_int64)sCred.uid);` |
|     ! 0 | 2924 | `				ph7_array_add_strkey_elem(pCr,"uid",pTmp);` |
|     ! 0 | 2925 | `				ph7_value_int64(pTmp,(ph7_int64)sCred.gid);` |
|     ! 0 | 2926 | `				ph7_array_add_strkey_elem(pCr,"gid",pTmp);` |
|     ! 0 | 2927 | `				ph7_array_add_strkey_elem(pEnt,"data",pCr);` |
|       - | 2928 | `			}` |
|       - | 2929 | `		}else` |
|       - | 2930 | `#endif` |
|       - | 2931 | `		{` |
|     ! 0 | 2932 | `			ph7_int64 iVal = 0;` |
|     ! 0 | 2933 | `			if( (unsigned int)pHdr->cmsg_len >= (unsigned int)PHL_CMSG_LEN(sizeof(int)) ){` |
|     ! 0 | 2934 | `				int i32 = 0;` |
|     ! 0 | 2935 | `				SyMemcpy((const void *)PHL_CMSG_DATA(pHdr),(void *)&i32,sizeof(i32));` |
|     ! 0 | 2936 | `				iVal = i32;` |
|     ! 0 | 2937 | `			}` |
|     ! 0 | 2938 | `			ph7_value_int64(pTmp,iVal);` |
|     ! 0 | 2939 | `			ph7_array_add_strkey_elem(pEnt,"data",pTmp);` |
|       - | 2940 | `		}` |
|     ! 0 | 2941 | `		ph7_array_add_elem(pCtl,0,pEnt);` |
|     ! 0 | 2942 | `	}` |
|       5 | 2943 | `	ph7_array_add_strkey_elem(pOut,"control",pCtl);` |
|       - | 2944 | `	{` |
|       5 | 2945 | `		ph7_value *pIov = ph7_context_new_array(pCtx);` |
|       5 | 2946 | `		if( pIov ){` |
|       5 | 2947 | `			ph7_value_string(pTmp,zBuf,iRead);` |
|       5 | 2948 | `			ph7_array_add_elem(pIov,0,pTmp);` |
|       5 | 2949 | `			ph7_array_add_strkey_elem(pOut,"iov",pIov);` |
|       2 | 2950 | `		}` |
|       - | 2951 | `	}` |
|       5 | 2952 | `	ph7_value_int64(pTmp,(ph7_int64)iFlagsOut);` |
|       5 | 2953 | `	ph7_array_add_strkey_elem(pOut,"flags",pTmp);` |
|       5 | 2954 | `	ph7_context_free_chunk(pCtx,zBuf);` |
|       5 | 2955 | `	ph7_context_free_chunk(pCtx,zCtl);` |
|       5 | 2956 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOut);` |
|       5 | 2957 | `	ph7_result_int64(pCtx,iRead);` |
|       5 | 2958 | `	return PH7_OK;` |
|       5 | 2959 | `}` |
|       - | 2960 |  |
|       - | 2961 | `/* ------------------------------------------------------------------------` |
|       - | 2962 | ` * getaddrinfo()` |
|       - | 2963 | ` * ------------------------------------------------------------------------ */` |
|       - | 2964 | ``/* Hand one candidate back as the opaque `AddressInfo` php answers with. */`` |
|       8 | 2965 | `static ph7_class_instance * SockNewAddrInfo(ph7_context *pCtx,const struct addrinfo *pAi)` |
|       1 | 2966 | `{` |
|       9 | 2967 | `	ph7_vm *pVm = pCtx->pVm;` |
|       9 | 2968 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,"AddressInfo",sizeof("AddressInfo")-1,0,0);` |
|       9 | 2969 | `	ph7_class_instance *pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|       - | 2970 | `	phl_addrinfo *pRec;` |
|       9 | 2971 | `	if( pThis == 0 ){` |
|     ! 0 | 2972 | `		return 0;` |
|       - | 2973 | `	}` |
|       9 | 2974 | `	pRec = (phl_addrinfo *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_addrinfo));` |
|       9 | 2975 | `	if( pRec == 0 ){` |
|     ! 0 | 2976 | `		PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2977 | `		return 0;` |
|       - | 2978 | `	}` |
|       9 | 2979 | `	SyZero(pRec,sizeof(phl_addrinfo));` |
|       9 | 2980 | `	pRec->pVm = pVm;` |
|       9 | 2981 | `	pRec->iFlags = pAi->ai_flags;` |
|       9 | 2982 | `	pRec->iFamily = pAi->ai_family;` |
|       9 | 2983 | `	pRec->iSockType = pAi->ai_socktype;` |
|       9 | 2984 | `	pRec->iProtocol = pAi->ai_protocol;` |
|       9 | 2985 | `	pRec->nAddr = (ph7_socklen)pAi->ai_addrlen;` |
|       9 | 2986 | `	if( pAi->ai_addrlen > 0 && pAi->ai_addrlen <= sizeof(pRec->sAddr) ){` |
|       9 | 2987 | `		SyMemcpy((const void *)pAi->ai_addr,(void *)&pRec->sAddr,(sxu32)pAi->ai_addrlen);` |
|       4 | 2988 | `	}` |
|       9 | 2989 | `	if( pAi->ai_canonname ){` |
|       2 | 2990 | `		sxu32 n = SyStrlen(pAi->ai_canonname);` |
|       2 | 2991 | `		pRec->zCanon = (char *)SyMemBackendAlloc(&pVm->sAllocator,n + 1);` |
|       2 | 2992 | `		if( pRec->zCanon ){` |
|       2 | 2993 | `			SyMemcpy(pAi->ai_canonname,pRec->zCanon,n);` |
|       2 | 2994 | `			pRec->zCanon[n] = 0;` |
|     ! 0 | 2995 | `		}` |
|     ! 0 | 2996 | `	}` |
|       9 | 2997 | `	pRec->pNext = (phl_addrinfo *)pVm->pAddrInfos;` |
|       9 | 2998 | `	pVm->pAddrInfos = pRec;` |
|       9 | 2999 | `	if( SockSlotAttach(pThis,(void *)pRec) != 0 ){` |
|     ! 0 | 3000 | `		PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3001 | `		return 0;` |
|       - | 3002 | `	}` |
|       9 | 3003 | `	pRec->pOwner = pThis;` |
|       9 | 3004 | `	return pThis;` |
|       5 | 3005 | `}` |
|       - | 3006 | `/* array\|false socket_addrinfo_lookup(string $host, ?string $service = null, array $hints = []) */` |
|      14 | 3007 | `static int vm_builtin_socket_addrinfo_lookup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3008 | `{` |
|      16 | 3009 | `	struct addrinfo sHint,*pRes = 0,*p;` |
|       - | 3010 | `	char zHost[NI_MAXHOST+1],zServ[NI_MAXSERV+1];` |
|       - | 3011 | `	const char *z;` |
|       - | 3012 | `	ph7_value *pArr,*pVal;` |
|      16 | 3013 | `	int n = 0,bServ = 0;` |
|      16 | 3014 | `	SyZero(&sHint,sizeof(sHint));` |
|      16 | 3015 | `	z = ph7_value_to_string(apArg[0],&n);` |
|      16 | 3016 | `	if( n < 0 \|\| n >= (int)sizeof(zHost) ){` |
|     ! 0 | 3017 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3018 | `		return PH7_OK;` |
|       - | 3019 | `	}` |
|      16 | 3020 | `	SyMemcpy(z,zHost,(sxu32)n);` |
|      16 | 3021 | `	zHost[n] = 0;` |
|      16 | 3022 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      11 | 3023 | `		z = ph7_value_to_string(apArg[1],&n);` |
|      11 | 3024 | `		if( n < 0 \|\| n >= (int)sizeof(zServ) ){` |
|     ! 0 | 3025 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3026 | `			return PH7_OK;` |
|       - | 3027 | `		}` |
|      11 | 3028 | `		SyMemcpy(z,zServ,(sxu32)n);` |
|      11 | 3029 | `		zServ[n] = 0;` |
|      11 | 3030 | `		bServ = 1;` |
|       5 | 3031 | `	}` |
|      16 | 3032 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|       - | 3033 | `		static const struct { const char *zKey; int iOfft; } aKey[] = {` |
|       - | 3034 | `			{ "ai_flags",    0 }, { "ai_socktype", 1 },` |
|       - | 3035 | `			{ "ai_protocol", 2 }, { "ai_family",   3 }` |
|       - | 3036 | `		};` |
|       - | 3037 | `		ph7_value *pKey;` |
|      12 | 3038 | `		unsigned int i,nSeen = 0;` |
|      52 | 3039 | `		for( i = 0 ; i < SX_ARRAYSIZE(aKey) ; ++i ){` |
|      42 | 3040 | `			pKey = ph7_array_fetch(apArg[2],aKey[i].zKey,(int)SyStrlen(aKey[i].zKey));` |
|      42 | 3041 | `			if( pKey == 0 ){` |
|      22 | 3042 | `				continue;` |
|       - | 3043 | `			}` |
|      21 | 3044 | `			nSeen++;` |
|      21 | 3045 | `			switch( aKey[i].iOfft ){` |
|       5 | 3046 | `				case 0: sHint.ai_flags    = (int)ph7_value_to_int64(pKey); break;` |
|       9 | 3047 | `				case 1: sHint.ai_socktype = (int)ph7_value_to_int64(pKey); break;` |
|     ! 0 | 3048 | `				case 2: sHint.ai_protocol = (int)ph7_value_to_int64(pKey); break;` |
|       9 | 3049 | `				default: sHint.ai_family  = (int)ph7_value_to_int64(pKey); break;` |
|       - | 3050 | `			}` |
|      11 | 3051 | `		}` |
|       - | 3052 | `		/* php refuses a hint array carrying anything it has no field for, and` |
|       - | 3053 | `		 * lists the four it knows in its own order. */` |
|      12 | 3054 | `		if( nSeen != ph7_array_count(apArg[2]) ){` |
|       4 | 3055 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3056 | `				"%z(): Argument #3 ($hints) must only contain array keys "` |
|       - | 3057 | `				"\"ai_flags\", \"ai_socktype\", \"ai_protocol\", or \"ai_family\"",` |
|       2 | 3058 | `				&pCtx->pFunc->sName);` |
|       - | 3059 | `		}` |
|       4 | 3060 | `	}` |
|      14 | 3061 | `	if( getaddrinfo(zHost,bServ ? zServ : 0,&sHint,&pRes) != 0 \|\| pRes == 0 ){` |
|       6 | 3062 | `		if( pRes ){` |
|     ! 0 | 3063 | `			freeaddrinfo(pRes);` |
|     ! 0 | 3064 | `		}` |
|       - | 3065 | `		/* A miss is a silent false: php reports nothing at all here. */` |
|       6 | 3066 | `		ph7_result_bool(pCtx,0);` |
|       6 | 3067 | `		return PH7_OK;` |
|       - | 3068 | `	}` |
|       9 | 3069 | `	pArr = ph7_context_new_array(pCtx);` |
|       9 | 3070 | `	pVal = ph7_context_new_scalar(pCtx);` |
|       9 | 3071 | `	if( pArr == 0 \|\| pVal == 0 ){` |
|     ! 0 | 3072 | `		freeaddrinfo(pRes);` |
|     ! 0 | 3073 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3074 | `	}` |
|      17 | 3075 | `	for( p = pRes ; p ; p = p->ai_next ){` |
|       9 | 3076 | `		ph7_class_instance *pThis = SockNewAddrInfo(pCtx,p);` |
|       9 | 3077 | `		if( pThis == 0 ){` |
|     ! 0 | 3078 | `			freeaddrinfo(pRes);` |
|     ! 0 | 3079 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3080 | `		}` |
|       9 | 3081 | `		PH7_MemObjRelease(pVal);` |
|       9 | 3082 | `		pVal->x.pOther = pThis;` |
|       9 | 3083 | `		pVal->iFlags = MEMOBJ_OBJ;` |
|       9 | 3084 | `		ph7_array_add_elem(pArr,0,pVal);` |
|       9 | 3085 | `		PH7_ClassInstanceUnref(pThis);` |
|       9 | 3086 | `		pVal->iFlags = MEMOBJ_NULL;` |
|       9 | 3087 | `		pVal->x.pOther = 0;` |
|       5 | 3088 | `	}` |
|       9 | 3089 | `	freeaddrinfo(pRes);` |
|       9 | 3090 | `	ph7_result_value(pCtx,pArr);` |
|       9 | 3091 | `	return PH7_OK;` |
|       9 | 3092 | `}` |
|       - | 3093 | `/* array socket_addrinfo_explain(AddressInfo $address) */` |
|       6 | 3094 | `static int vm_builtin_socket_addrinfo_explain(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3095 | `{` |
|       - | 3096 | `	phl_addrinfo *pAi;` |
|       - | 3097 | `	ph7_value *pArr,*pAddr,*pTmp;` |
|       - | 3098 | `	char zBuf[INET6_ADDRSTRLEN+1];` |
|       3 | 3099 | `	SXUNUSED(nArg);` |
|       7 | 3100 | `	pAi = SockAddrInfoArg(pCtx,apArg[0]);` |
|       7 | 3101 | `	if( pAi == 0 ){` |
|     ! 0 | 3102 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3103 | `		return PH7_OK;` |
|       - | 3104 | `	}` |
|       7 | 3105 | `	pArr = ph7_context_new_array(pCtx);` |
|       7 | 3106 | `	pAddr = ph7_context_new_array(pCtx);` |
|       7 | 3107 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|       7 | 3108 | `	if( pArr == 0 \|\| pAddr == 0 \|\| pTmp == 0 ){` |
|     ! 0 | 3109 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3110 | `	}` |
|       7 | 3111 | `	ph7_value_int64(pTmp,pAi->iFlags);` |
|       7 | 3112 | `	ph7_array_add_strkey_elem(pArr,"ai_flags",pTmp);` |
|       7 | 3113 | `	ph7_value_int64(pTmp,pAi->iFamily);` |
|       7 | 3114 | `	ph7_array_add_strkey_elem(pArr,"ai_family",pTmp);` |
|       7 | 3115 | `	ph7_value_int64(pTmp,pAi->iSockType);` |
|       7 | 3116 | `	ph7_array_add_strkey_elem(pArr,"ai_socktype",pTmp);` |
|       7 | 3117 | `	ph7_value_int64(pTmp,pAi->iProtocol);` |
|       7 | 3118 | `	ph7_array_add_strkey_elem(pArr,"ai_protocol",pTmp);` |
|       7 | 3119 | `	if( pAi->zCanon ){` |
|       - | 3120 | `		/* Only a lookup that ASKED for AI_CANONNAME carries this key at all. */` |
|       2 | 3121 | `		ph7_value_string(pTmp,pAi->zCanon,-1);` |
|       2 | 3122 | `		ph7_array_add_strkey_elem(pArr,"ai_canonname",pTmp);` |
|     ! 0 | 3123 | `	}` |
|       7 | 3124 | `	zBuf[0] = 0;` |
|       7 | 3125 | `	if( pAi->iFamily == AF_INET6 ){` |
|     ! 0 | 3126 | `		const struct sockaddr_in6 *pIn6 = (const struct sockaddr_in6 *)&pAi->sAddr;` |
|     ! 0 | 3127 | `		ph7_value_int64(pTmp,(ph7_int64)ntohs(pIn6->sin6_port));` |
|     ! 0 | 3128 | `		ph7_array_add_strkey_elem(pAddr,"sin6_port",pTmp);` |
|     ! 0 | 3129 | `		inet_ntop(AF_INET6,(const void *)&pIn6->sin6_addr,zBuf,sizeof(zBuf));` |
|     ! 0 | 3130 | `		ph7_value_string(pTmp,zBuf,-1);` |
|     ! 0 | 3131 | `		ph7_array_add_strkey_elem(pAddr,"sin6_addr",pTmp);` |
|     ! 0 | 3132 | `	}else{` |
|       7 | 3133 | `		const struct sockaddr_in *pIn = (const struct sockaddr_in *)&pAi->sAddr;` |
|       7 | 3134 | `		ph7_value_int64(pTmp,(ph7_int64)ntohs(pIn->sin_port));` |
|       7 | 3135 | `		ph7_array_add_strkey_elem(pAddr,"sin_port",pTmp);` |
|       7 | 3136 | `		inet_ntop(AF_INET,(const void *)&pIn->sin_addr,zBuf,sizeof(zBuf));` |
|       7 | 3137 | `		ph7_value_string(pTmp,zBuf,-1);` |
|       7 | 3138 | `		ph7_array_add_strkey_elem(pAddr,"sin_addr",pTmp);` |
|       - | 3139 | `	}` |
|       7 | 3140 | `	ph7_array_add_strkey_elem(pArr,"ai_addr",pAddr);` |
|       7 | 3141 | `	ph7_result_value(pCtx,pArr);` |
|       7 | 3142 | `	return PH7_OK;` |
|       4 | 3143 | `}` |
|       - | 3144 | `/* Socket\|false socket_addrinfo_bind / socket_addrinfo_connect (AddressInfo) */` |
|       4 | 3145 | `static int SockAddrInfoOpen(ph7_context *pCtx,ph7_value **apArg,int bConnect)` |
|       1 | 3146 | `{` |
|       - | 3147 | `	phl_addrinfo *pAi;` |
|       - | 3148 | `	ph7_socket sock;` |
|       5 | 3149 | `	pAi = SockAddrInfoArg(pCtx,apArg[0]);` |
|       5 | 3150 | `	if( pAi == 0 ){` |
|     ! 0 | 3151 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3152 | `		return PH7_OK;` |
|       - | 3153 | `	}` |
|       5 | 3154 | `	PH7_NetEnsureInit();` |
|       5 | 3155 | `	sock = socket(pAi->iFamily,pAi->iSockType,pAi->iProtocol);` |
|       5 | 3156 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     ! 0 | 3157 | `		SockFailLast(pCtx,0,"Unable to create socket");` |
|     ! 0 | 3158 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3159 | `		return PH7_OK;` |
|       - | 3160 | `	}` |
|       5 | 3161 | `	if( (bConnect ? connect(sock,(struct sockaddr *)&pAi->sAddr,pAi->nAddr)` |
|       6 | 3162 | `	              : bind(sock,(struct sockaddr *)&pAi->sAddr,pAi->nAddr)) != 0 ){` |
|     ! 0 | 3163 | `		SockFailLast(pCtx,0,bConnect ? "Unable to connect address" : "Unable to bind address");` |
|     ! 0 | 3164 | `		PH7_NetClose(sock);` |
|     ! 0 | 3165 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3166 | `		return PH7_OK;` |
|       - | 3167 | `	}` |
|       5 | 3168 | `	return SockResultObject(pCtx,sock,pAi->iFamily,pAi->iSockType,pAi->iProtocol);` |
|       3 | 3169 | `}` |
|       2 | 3170 | `static int vm_builtin_socket_addrinfo_bind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3171 | `{` |
|       1 | 3172 | `	SXUNUSED(nArg);` |
|       3 | 3173 | `	return SockAddrInfoOpen(pCtx,apArg,0);` |
|       1 | 3174 | `}` |
|       2 | 3175 | `static int vm_builtin_socket_addrinfo_connect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3176 | `{` |
|       1 | 3177 | `	SXUNUSED(nArg);` |
|       3 | 3178 | `	return SockAddrInfoOpen(pCtx,apArg,1);` |
|       1 | 3179 | `}` |
|       - | 3180 |  |
|       - | 3181 | `#ifdef __WINNT__` |
|       - | 3182 | `/* ------------------------------------------------------------------------` |
|       - | 3183 | ` * The three Windows-only verbs` |
|       - | 3184 | ` * ------------------------------------------------------------------------ */` |
|       - | 3185 | `/*` |
|       - | 3186 | ` * Windows has no descriptor to pass down a socket, so a socket reaches another` |
|       - | 3187 | `` * PROCESS by being DUPLICATED into it: `WSADuplicateSocket()` writes a`` |
|       - | 3188 | ` * WSAPROTOCOL_INFO describing it, the exporter parks that in a named shared` |
|       - | 3189 | ` * mapping, and the importer opens the mapping by name and calls WSASocket()` |
|       - | 3190 | `` * with what it finds. php names the mapping `php_wsa_for_<n>` with a`` |
|       - | 3191 | ` * per-process counter -- not the pid, which is what the name reads like -- and` |
|       - | 3192 | `` * `socket_wsaprotocol_info_release()` closes the mapping so a second release`` |
|       - | 3193 | ` * answers false. The three exist on no other platform.` |
|       - | 3194 | ` */` |
|       - | 3195 | `typedef struct phl_wsa_map phl_wsa_map;` |
|       - | 3196 | `struct phl_wsa_map` |
|       - | 3197 | `{` |
|       - | 3198 | `	char zName[32];` |
|       - | 3199 | `	HANDLE hMap;` |
|       - | 3200 | `	phl_wsa_map *pNext;` |
|       - | 3201 | `};` |
|       - | 3202 | `static phl_wsa_map *pWsaMaps = 0;` |
|       - | 3203 | `static unsigned int nWsaNext = 0;` |
|       - | 3204 | `/* string\|false socket_wsaprotocol_info_export(Socket $socket, int $process_id) */` |
|       - | 3205 | `static int vm_builtin_socket_wsaprotocol_info_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 3206 | `{` |
|       - | 3207 | `	WSAPROTOCOL_INFO sInfo;` |
|       - | 3208 | `	phl_socket *pSock;` |
|       - | 3209 | `	phl_wsa_map *pMap;` |
|       - | 3210 | `	HANDLE hMap;` |
|       - | 3211 | `	void *pView;` |
|       - | 3212 | `	int rc;` |
|       - | 3213 | `	SXUNUSED(nArg);` |
|     ! 0 | 3214 | `	pSock = SockArg(pCtx,apArg[0],1,"socket",&rc);` |
|     ! 0 | 3215 | `	if( pSock == 0 ){` |
|     ! 0 | 3216 | `		return rc;` |
|       - | 3217 | `	}` |
|     ! 0 | 3218 | `	SyZero(&sInfo,sizeof(sInfo));` |
|     ! 0 | 3219 | `	if( WSADuplicateSocket(pSock->sock,(DWORD)ph7_value_to_int64(apArg[1]),&sInfo) != 0 ){` |
|     ! 0 | 3220 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3221 | `			"Unable to export WSA protocol info [0x%08lx]: %s",` |
|       - | 3222 | `			(unsigned long)WSAGetLastError(),PH7_SocketStrError(WSAGetLastError()));` |
|     ! 0 | 3223 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3224 | `		return PH7_OK;` |
|       - | 3225 | `	}` |
|     ! 0 | 3226 | `	pMap = (phl_wsa_map *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_wsa_map));` |
|     ! 0 | 3227 | `	if( pMap == 0 ){` |
|     ! 0 | 3228 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3229 | `	}` |
|     ! 0 | 3230 | `	SyZero(pMap,sizeof(phl_wsa_map));` |
|     ! 0 | 3231 | `	SyBufferFormat(pMap->zName,sizeof(pMap->zName),"php_wsa_for_%u",nWsaNext++);` |
|     ! 0 | 3232 | `	hMap = CreateFileMappingA(INVALID_HANDLE_VALUE,0,PAGE_READWRITE,0,` |
|       - | 3233 | `		(DWORD)sizeof(sInfo),pMap->zName);` |
|     ! 0 | 3234 | `	if( hMap == 0 ){` |
|     ! 0 | 3235 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pMap);` |
|     ! 0 | 3236 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3237 | `			"Unable to create file mapping [0x%08lx]",(unsigned long)GetLastError());` |
|     ! 0 | 3238 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3239 | `		return PH7_OK;` |
|       - | 3240 | `	}` |
|     ! 0 | 3241 | `	pView = MapViewOfFile(hMap,FILE_MAP_WRITE,0,0,0);` |
|     ! 0 | 3242 | `	if( pView == 0 ){` |
|     ! 0 | 3243 | `		CloseHandle(hMap);` |
|     ! 0 | 3244 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pMap);` |
|     ! 0 | 3245 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3246 | `			"Unable to map file view [0x%08lx]",(unsigned long)GetLastError());` |
|     ! 0 | 3247 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3248 | `		return PH7_OK;` |
|       - | 3249 | `	}` |
|     ! 0 | 3250 | `	SyMemcpy((const void *)&sInfo,pView,(sxu32)sizeof(sInfo));` |
|     ! 0 | 3251 | `	UnmapViewOfFile(pView);` |
|       - | 3252 | `	/* The mapping stays OPEN: it only exists while a handle holds it, and the` |
|       - | 3253 | `	 * importing process needs it to still be there. */` |
|     ! 0 | 3254 | `	pMap->hMap = hMap;` |
|     ! 0 | 3255 | `	pMap->pNext = pWsaMaps;` |
|     ! 0 | 3256 | `	pWsaMaps = pMap;` |
|     ! 0 | 3257 | `	ph7_result_string(pCtx,pMap->zName,-1);` |
|     ! 0 | 3258 | `	return PH7_OK;` |
|     ! 0 | 3259 | `}` |
|       - | 3260 | `/* Socket\|false socket_wsaprotocol_info_import(string $info_id) */` |
|       - | 3261 | `static int vm_builtin_socket_wsaprotocol_info_import(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 3262 | `{` |
|       - | 3263 | `	WSAPROTOCOL_INFO sInfo;` |
|       - | 3264 | `	char zName[32];` |
|       - | 3265 | `	const char *z;` |
|       - | 3266 | `	HANDLE hMap;` |
|       - | 3267 | `	void *pView;` |
|       - | 3268 | `	ph7_socket sock;` |
|     ! 0 | 3269 | `	int n = 0;` |
|       - | 3270 | `	SXUNUSED(nArg);` |
|     ! 0 | 3271 | `	z = ph7_value_to_string(apArg[0],&n);` |
|     ! 0 | 3272 | `	if( n < 1 \|\| n >= (int)sizeof(zName) ){` |
|     ! 0 | 3273 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3274 | `			"Unable to open file mapping [0x%08lx]",2UL);` |
|     ! 0 | 3275 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3276 | `		return PH7_OK;` |
|       - | 3277 | `	}` |
|     ! 0 | 3278 | `	SyMemcpy(z,zName,(sxu32)n);` |
|     ! 0 | 3279 | `	zName[n] = 0;` |
|     ! 0 | 3280 | `	hMap = OpenFileMappingA(FILE_MAP_READ,FALSE,zName);` |
|     ! 0 | 3281 | `	if( hMap == 0 ){` |
|     ! 0 | 3282 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3283 | `			"Unable to open file mapping [0x%08lx]",(unsigned long)GetLastError());` |
|     ! 0 | 3284 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3285 | `		return PH7_OK;` |
|       - | 3286 | `	}` |
|     ! 0 | 3287 | `	pView = MapViewOfFile(hMap,FILE_MAP_READ,0,0,0);` |
|     ! 0 | 3288 | `	if( pView == 0 ){` |
|     ! 0 | 3289 | `		CloseHandle(hMap);` |
|     ! 0 | 3290 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3291 | `			"Unable to map file view [0x%08lx]",(unsigned long)GetLastError());` |
|     ! 0 | 3292 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3293 | `		return PH7_OK;` |
|       - | 3294 | `	}` |
|     ! 0 | 3295 | `	SyMemcpy((const void *)pView,(void *)&sInfo,(sxu32)sizeof(sInfo));` |
|     ! 0 | 3296 | `	UnmapViewOfFile(pView);` |
|     ! 0 | 3297 | `	CloseHandle(hMap);` |
|     ! 0 | 3298 | `	sock = WSASocket(FROM_PROTOCOL_INFO,FROM_PROTOCOL_INFO,FROM_PROTOCOL_INFO,` |
|       - | 3299 | `		&sInfo,0,WSA_FLAG_OVERLAPPED);` |
|     ! 0 | 3300 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     ! 0 | 3301 | `		SockFailLast(pCtx,0,"Unable to create socket");` |
|     ! 0 | 3302 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3303 | `		return PH7_OK;` |
|       - | 3304 | `	}` |
|     ! 0 | 3305 | `	return SockResultObject(pCtx,sock,sInfo.iAddressFamily,sInfo.iSocketType,sInfo.iProtocol);` |
|     ! 0 | 3306 | `}` |
|       - | 3307 | `/* bool socket_wsaprotocol_info_release(string $info_id) */` |
|       - | 3308 | `static int vm_builtin_socket_wsaprotocol_info_release(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 3309 | `{` |
|     ! 0 | 3310 | `	phl_wsa_map **ppSlot = &pWsaMaps;` |
|       - | 3311 | `	const char *z;` |
|     ! 0 | 3312 | `	int n = 0;` |
|       - | 3313 | `	SXUNUSED(nArg);` |
|     ! 0 | 3314 | `	z = ph7_value_to_string(apArg[0],&n);` |
|     ! 0 | 3315 | `	while( *ppSlot ){` |
|     ! 0 | 3316 | `		phl_wsa_map *pMap = *ppSlot;` |
|     ! 0 | 3317 | `		if( (int)SyStrlen(pMap->zName) == n && SyMemcmp(pMap->zName,z,(sxu32)n) == 0 ){` |
|     ! 0 | 3318 | `			*ppSlot = pMap->pNext;` |
|     ! 0 | 3319 | `			CloseHandle(pMap->hMap);` |
|     ! 0 | 3320 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pMap);` |
|     ! 0 | 3321 | `			ph7_result_bool(pCtx,1);` |
|     ! 0 | 3322 | `			return PH7_OK;` |
|       - | 3323 | `		}` |
|     ! 0 | 3324 | `		ppSlot = &pMap->pNext;` |
|     ! 0 | 3325 | `	}` |
|     ! 0 | 3326 | `	ph7_result_bool(pCtx,0);` |
|     ! 0 | 3327 | `	return PH7_OK;` |
|     ! 0 | 3328 | `}` |
|       - | 3329 | `#endif /* __WINNT__ */` |
|       - | 3330 |  |
|       - | 3331 | `/* ------------------------------------------------------------------------` |
|       - | 3332 | ` * The constants, the classes and the registration` |
|       - | 3333 | ` * ------------------------------------------------------------------------ */` |
|       - | 3334 | `/*` |
|       - | 3335 | ` * php's whole ext/sockets constant surface, in php's own registration order` |
|       - | 3336 | ` * (which is what ReflectionExtension::getConstants() answers in). Every one is` |
|       - | 3337 | ` * the PLATFORM's macro rather than a number copied out of one build: AF_INET6` |
|       - | 3338 | ` * is 10 here and 23 on Windows, and every SOCKET_E* name is a Winsock code` |
|       - | 3339 | ` * there (10035 for EWOULDBLOCK) rather than an errno. Two are php's own` |
|       - | 3340 | ` * inventions -- PHP_NORMAL_READ and PHP_BINARY_READ, socket_read()'s $mode --` |
|       - | 3341 | ` * and two more are php's aliases for a name the platform spells differently.` |
|       - | 3342 | ` *` |
|       - | 3343 | ` * A name this platform has no macro for is simply not registered, which is what` |
|       - | 3344 | ` * php's own #ifdef'd registration block does: the Linux-only BPF, packet-socket` |
|       - | 3345 | ` * and MCAST_* families are all absent from a Windows php too. Twelve rows go the` |
|       - | 3346 | ` * other way and are gated on the PLATFORM rather than on the macro, because the` |
|       - | 3347 | ``  * macro exists on both and php registers the name on only one: `SOCKET_ESTALE` `` |
|       - | 3348 | ` * is ESTALE (116) on Linux and php does not define it there.` |
|       - | 3349 | ` */` |
|       - | 3350 | `static const struct {` |
|       - | 3351 | `	const char *zName;` |
|       - | 3352 | `	int iValue;` |
|       - | 3353 | `} aSockConst[] = {` |
|       - | 3354 | `#ifdef AF_UNIX` |
|       - | 3355 | `	{ "AF_UNIX", AF_UNIX },` |
|       - | 3356 | `#endif` |
|       - | 3357 | `#ifdef AF_INET` |
|       - | 3358 | `	{ "AF_INET", AF_INET },` |
|       - | 3359 | `#endif` |
|       - | 3360 | `#ifdef AF_INET6` |
|       - | 3361 | `	{ "AF_INET6", AF_INET6 },` |
|       - | 3362 | `#endif` |
|       - | 3363 | `#ifdef AF_PACKET` |
|       - | 3364 | `	{ "AF_PACKET", AF_PACKET },` |
|       - | 3365 | `#endif` |
|       - | 3366 | `#ifdef SOCK_STREAM` |
|       - | 3367 | `	{ "SOCK_STREAM", SOCK_STREAM },` |
|       - | 3368 | `#endif` |
|       - | 3369 | `#ifdef SOCK_DGRAM` |
|       - | 3370 | `	{ "SOCK_DGRAM", SOCK_DGRAM },` |
|       - | 3371 | `#endif` |
|       - | 3372 | `#ifdef SOCK_RAW` |
|       - | 3373 | `	{ "SOCK_RAW", SOCK_RAW },` |
|       - | 3374 | `#endif` |
|       - | 3375 | `#ifdef SOCK_SEQPACKET` |
|       - | 3376 | `	{ "SOCK_SEQPACKET", SOCK_SEQPACKET },` |
|       - | 3377 | `#endif` |
|       - | 3378 | `#ifdef SOCK_RDM` |
|       - | 3379 | `	{ "SOCK_RDM", SOCK_RDM },` |
|       - | 3380 | `#endif` |
|       - | 3381 | `#ifdef SOCK_DCCP` |
|       - | 3382 | `	{ "SOCK_DCCP", SOCK_DCCP },` |
|       - | 3383 | `#endif` |
|       - | 3384 | `#ifdef SOCK_CLOEXEC` |
|       - | 3385 | `	{ "SOCK_CLOEXEC", SOCK_CLOEXEC },` |
|       - | 3386 | `#endif` |
|       - | 3387 | `#ifdef SOCK_NONBLOCK` |
|       - | 3388 | `	{ "SOCK_NONBLOCK", SOCK_NONBLOCK },` |
|       - | 3389 | `#endif` |
|       - | 3390 | `#ifdef MSG_OOB` |
|       - | 3391 | `	{ "MSG_OOB", MSG_OOB },` |
|       - | 3392 | `#endif` |
|       - | 3393 | `#ifdef MSG_WAITALL` |
|       - | 3394 | `	{ "MSG_WAITALL", MSG_WAITALL },` |
|       - | 3395 | `#endif` |
|       - | 3396 | `#ifdef MSG_CTRUNC` |
|       - | 3397 | `	{ "MSG_CTRUNC", MSG_CTRUNC },` |
|       - | 3398 | `#endif` |
|       - | 3399 | `#ifdef MSG_TRUNC` |
|       - | 3400 | `	{ "MSG_TRUNC", MSG_TRUNC },` |
|       - | 3401 | `#endif` |
|       - | 3402 | `#ifdef MSG_PEEK` |
|       - | 3403 | `	{ "MSG_PEEK", MSG_PEEK },` |
|       - | 3404 | `#endif` |
|       - | 3405 | `#ifdef MSG_DONTROUTE` |
|       - | 3406 | `	{ "MSG_DONTROUTE", MSG_DONTROUTE },` |
|       - | 3407 | `#endif` |
|       - | 3408 | `#ifdef MSG_EOR` |
|       - | 3409 | `	{ "MSG_EOR", MSG_EOR },` |
|       - | 3410 | `#endif` |
|       - | 3411 | `#if defined(MSG_EOF)` |
|       - | 3412 | `	{ "MSG_EOF", MSG_EOF },` |
|       - | 3413 | `#elif defined(MSG_FIN)` |
|       - | 3414 | `	/* php takes the Linux spelling where the BSD one is absent. */` |
|       - | 3415 | `	{ "MSG_EOF", MSG_FIN },` |
|       - | 3416 | `#endif` |
|       - | 3417 | `#ifdef MSG_CONFIRM` |
|       - | 3418 | `	{ "MSG_CONFIRM", MSG_CONFIRM },` |
|       - | 3419 | `#endif` |
|       - | 3420 | `#ifdef MSG_ERRQUEUE` |
|       - | 3421 | `	{ "MSG_ERRQUEUE", MSG_ERRQUEUE },` |
|       - | 3422 | `#endif` |
|       - | 3423 | `#ifdef MSG_NOSIGNAL` |
|       - | 3424 | `	{ "MSG_NOSIGNAL", MSG_NOSIGNAL },` |
|       - | 3425 | `#endif` |
|       - | 3426 | `#ifdef MSG_DONTWAIT` |
|       - | 3427 | `	{ "MSG_DONTWAIT", MSG_DONTWAIT },` |
|       - | 3428 | `#endif` |
|       - | 3429 | `#ifdef MSG_MORE` |
|       - | 3430 | `	{ "MSG_MORE", MSG_MORE },` |
|       - | 3431 | `#endif` |
|       - | 3432 | `#ifdef MSG_WAITFORONE` |
|       - | 3433 | `	{ "MSG_WAITFORONE", MSG_WAITFORONE },` |
|       - | 3434 | `#endif` |
|       - | 3435 | `#ifdef MSG_CMSG_CLOEXEC` |
|       - | 3436 | `	{ "MSG_CMSG_CLOEXEC", MSG_CMSG_CLOEXEC },` |
|       - | 3437 | `#endif` |
|       - | 3438 | `#ifdef MSG_ZEROCOPY` |
|       - | 3439 | `	{ "MSG_ZEROCOPY", MSG_ZEROCOPY },` |
|       - | 3440 | `#endif` |
|       - | 3441 | `#ifdef SO_DEBUG` |
|       - | 3442 | `	{ "SO_DEBUG", SO_DEBUG },` |
|       - | 3443 | `#endif` |
|       - | 3444 | `#ifdef SO_REUSEADDR` |
|       - | 3445 | `	{ "SO_REUSEADDR", SO_REUSEADDR },` |
|       - | 3446 | `#endif` |
|       - | 3447 | `#ifdef SO_REUSEPORT` |
|       - | 3448 | `	{ "SO_REUSEPORT", SO_REUSEPORT },` |
|       - | 3449 | `#endif` |
|       - | 3450 | `#ifdef SO_KEEPALIVE` |
|       - | 3451 | `	{ "SO_KEEPALIVE", SO_KEEPALIVE },` |
|       - | 3452 | `#endif` |
|       - | 3453 | `#ifdef SO_DONTROUTE` |
|       - | 3454 | `	{ "SO_DONTROUTE", SO_DONTROUTE },` |
|       - | 3455 | `#endif` |
|       - | 3456 | `#ifdef SO_LINGER` |
|       - | 3457 | `	{ "SO_LINGER", SO_LINGER },` |
|       - | 3458 | `#endif` |
|       - | 3459 | `#ifdef SO_BROADCAST` |
|       - | 3460 | `	{ "SO_BROADCAST", SO_BROADCAST },` |
|       - | 3461 | `#endif` |
|       - | 3462 | `#ifdef SO_OOBINLINE` |
|       - | 3463 | `	{ "SO_OOBINLINE", SO_OOBINLINE },` |
|       - | 3464 | `#endif` |
|       - | 3465 | `#ifdef SO_SNDBUF` |
|       - | 3466 | `	{ "SO_SNDBUF", SO_SNDBUF },` |
|       - | 3467 | `#endif` |
|       - | 3468 | `#ifdef SO_RCVBUF` |
|       - | 3469 | `	{ "SO_RCVBUF", SO_RCVBUF },` |
|       - | 3470 | `#endif` |
|       - | 3471 | `#ifdef SO_SNDLOWAT` |
|       - | 3472 | `	{ "SO_SNDLOWAT", SO_SNDLOWAT },` |
|       - | 3473 | `#endif` |
|       - | 3474 | `#ifdef SO_RCVLOWAT` |
|       - | 3475 | `	{ "SO_RCVLOWAT", SO_RCVLOWAT },` |
|       - | 3476 | `#endif` |
|       - | 3477 | `#ifdef SO_SNDTIMEO` |
|       - | 3478 | `	{ "SO_SNDTIMEO", SO_SNDTIMEO },` |
|       - | 3479 | `#endif` |
|       - | 3480 | `#ifdef SO_RCVTIMEO` |
|       - | 3481 | `	{ "SO_RCVTIMEO", SO_RCVTIMEO },` |
|       - | 3482 | `#endif` |
|       - | 3483 | `#ifdef SO_TYPE` |
|       - | 3484 | `	{ "SO_TYPE", SO_TYPE },` |
|       - | 3485 | `#endif` |
|       - | 3486 | `#ifdef SO_ERROR` |
|       - | 3487 | `	{ "SO_ERROR", SO_ERROR },` |
|       - | 3488 | `#endif` |
|       - | 3489 | `#ifdef SO_BINDTODEVICE` |
|       - | 3490 | `	{ "SO_BINDTODEVICE", SO_BINDTODEVICE },` |
|       - | 3491 | `#endif` |
|       - | 3492 | `#ifdef SO_BINDTOIFINDEX` |
|       - | 3493 | `	{ "SO_BINDTOIFINDEX", SO_BINDTOIFINDEX },` |
|       - | 3494 | `#endif` |
|       - | 3495 | `#ifdef SOL_SOCKET` |
|       - | 3496 | `	{ "SOL_SOCKET", SOL_SOCKET },` |
|       - | 3497 | `#endif` |
|       - | 3498 | `#ifdef SOMAXCONN` |
|       - | 3499 | `	{ "SOMAXCONN", SOMAXCONN },` |
|       - | 3500 | `#endif` |
|       - | 3501 | `#ifdef SO_MARK` |
|       - | 3502 | `	{ "SO_MARK", SO_MARK },` |
|       - | 3503 | `#endif` |
|       - | 3504 | `#ifdef SO_INCOMING_CPU` |
|       - | 3505 | `	{ "SO_INCOMING_CPU", SO_INCOMING_CPU },` |
|       - | 3506 | `#endif` |
|       - | 3507 | `#ifdef SO_MEMINFO` |
|       - | 3508 | `	{ "SO_MEMINFO", SO_MEMINFO },` |
|       - | 3509 | `#endif` |
|       - | 3510 | `#ifdef SO_BPF_EXTENSIONS` |
|       - | 3511 | `	{ "SO_BPF_EXTENSIONS", SO_BPF_EXTENSIONS },` |
|       - | 3512 | `#endif` |
|       - | 3513 | `#ifdef SO_BUSY_POLL` |
|       - | 3514 | `	{ "SO_BUSY_POLL", SO_BUSY_POLL },` |
|       - | 3515 | `#endif` |
|       - | 3516 | `#ifdef SKF_AD_OFF` |
|       - | 3517 | `	{ "SKF_AD_OFF", SKF_AD_OFF },` |
|       - | 3518 | `#endif` |
|       - | 3519 | `#ifdef SKF_AD_PROTOCOL` |
|       - | 3520 | `	{ "SKF_AD_PROTOCOL", SKF_AD_PROTOCOL },` |
|       - | 3521 | `#endif` |
|       - | 3522 | `#ifdef SKF_AD_PKTTYPE` |
|       - | 3523 | `	{ "SKF_AD_PKTTYPE", SKF_AD_PKTTYPE },` |
|       - | 3524 | `#endif` |
|       - | 3525 | `#ifdef SKF_AD_IFINDEX` |
|       - | 3526 | `	{ "SKF_AD_IFINDEX", SKF_AD_IFINDEX },` |
|       - | 3527 | `#endif` |
|       - | 3528 | `#ifdef SKF_AD_NLATTR` |
|       - | 3529 | `	{ "SKF_AD_NLATTR", SKF_AD_NLATTR },` |
|       - | 3530 | `#endif` |
|       - | 3531 | `#ifdef SKF_AD_NLATTR_NEST` |
|       - | 3532 | `	{ "SKF_AD_NLATTR_NEST", SKF_AD_NLATTR_NEST },` |
|       - | 3533 | `#endif` |
|       - | 3534 | `#ifdef SKF_AD_MARK` |
|       - | 3535 | `	{ "SKF_AD_MARK", SKF_AD_MARK },` |
|       - | 3536 | `#endif` |
|       - | 3537 | `#ifdef SKF_AD_QUEUE` |
|       - | 3538 | `	{ "SKF_AD_QUEUE", SKF_AD_QUEUE },` |
|       - | 3539 | `#endif` |
|       - | 3540 | `#ifdef SKF_AD_HATYPE` |
|       - | 3541 | `	{ "SKF_AD_HATYPE", SKF_AD_HATYPE },` |
|       - | 3542 | `#endif` |
|       - | 3543 | `#ifdef SKF_AD_RXHASH` |
|       - | 3544 | `	{ "SKF_AD_RXHASH", SKF_AD_RXHASH },` |
|       - | 3545 | `#endif` |
|       - | 3546 | `#ifdef SKF_AD_CPU` |
|       - | 3547 | `	{ "SKF_AD_CPU", SKF_AD_CPU },` |
|       - | 3548 | `#endif` |
|       - | 3549 | `#ifdef SKF_AD_ALU_XOR_X` |
|       - | 3550 | `	{ "SKF_AD_ALU_XOR_X", SKF_AD_ALU_XOR_X },` |
|       - | 3551 | `#endif` |
|       - | 3552 | `#ifdef SKF_AD_VLAN_TAG` |
|       - | 3553 | `	{ "SKF_AD_VLAN_TAG", SKF_AD_VLAN_TAG },` |
|       - | 3554 | `#endif` |
|       - | 3555 | `#ifdef SKF_AD_VLAN_TAG_PRESENT` |
|       - | 3556 | `	{ "SKF_AD_VLAN_TAG_PRESENT", SKF_AD_VLAN_TAG_PRESENT },` |
|       - | 3557 | `#endif` |
|       - | 3558 | `#ifdef SKF_AD_PAY_OFFSET` |
|       - | 3559 | `	{ "SKF_AD_PAY_OFFSET", SKF_AD_PAY_OFFSET },` |
|       - | 3560 | `#endif` |
|       - | 3561 | `#ifdef SKF_AD_RANDOM` |
|       - | 3562 | `	{ "SKF_AD_RANDOM", SKF_AD_RANDOM },` |
|       - | 3563 | `#endif` |
|       - | 3564 | `#ifdef SKF_AD_VLAN_TPID` |
|       - | 3565 | `	{ "SKF_AD_VLAN_TPID", SKF_AD_VLAN_TPID },` |
|       - | 3566 | `#endif` |
|       - | 3567 | `#ifdef SKF_AD_MAX` |
|       - | 3568 | `	{ "SKF_AD_MAX", SKF_AD_MAX },` |
|       - | 3569 | `#endif` |
|       - | 3570 | `#ifdef TCP_CONGESTION` |
|       - | 3571 | `	{ "TCP_CONGESTION", TCP_CONGESTION },` |
|       - | 3572 | `#endif` |
|       - | 3573 | `#ifdef TCP_SYNCNT` |
|       - | 3574 | `	{ "TCP_SYNCNT", TCP_SYNCNT },` |
|       - | 3575 | `#endif` |
|       - | 3576 | `#ifdef SO_ZEROCOPY` |
|       - | 3577 | `	{ "SO_ZEROCOPY", SO_ZEROCOPY },` |
|       - | 3578 | `#endif` |
|       - | 3579 | `#ifdef TCP_NODELAY` |
|       - | 3580 | `	{ "TCP_NODELAY", TCP_NODELAY },` |
|       - | 3581 | `#endif` |
|       - | 3582 | `#ifdef __WINNT__` |
|       - | 3583 | `	{ "TCP_KEEPALIVE", TCP_KEEPALIVE },` |
|       - | 3584 | `#endif` |
|       - | 3585 | `#ifdef TCP_NOTSENT_LOWAT` |
|       - | 3586 | `	{ "TCP_NOTSENT_LOWAT", TCP_NOTSENT_LOWAT },` |
|       - | 3587 | `#endif` |
|       - | 3588 | `#ifdef TCP_DEFER_ACCEPT` |
|       - | 3589 | `	{ "TCP_DEFER_ACCEPT", TCP_DEFER_ACCEPT },` |
|       - | 3590 | `#endif` |
|       - | 3591 | `#ifdef TCP_KEEPIDLE` |
|       - | 3592 | `	{ "TCP_KEEPIDLE", TCP_KEEPIDLE },` |
|       - | 3593 | `#endif` |
|       - | 3594 | `#ifdef TCP_KEEPINTVL` |
|       - | 3595 | `	{ "TCP_KEEPINTVL", TCP_KEEPINTVL },` |
|       - | 3596 | `#endif` |
|       - | 3597 | `#ifdef TCP_KEEPCNT` |
|       - | 3598 | `	{ "TCP_KEEPCNT", TCP_KEEPCNT },` |
|       - | 3599 | `#endif` |
|       - | 3600 | `	{ "PHP_NORMAL_READ", 1 },` |
|       - | 3601 | `	{ "PHP_BINARY_READ", 2 },` |
|       - | 3602 | `#ifdef MCAST_JOIN_GROUP` |
|       - | 3603 | `	{ "MCAST_JOIN_GROUP", MCAST_JOIN_GROUP },` |
|       - | 3604 | `#endif` |
|       - | 3605 | `#ifdef MCAST_LEAVE_GROUP` |
|       - | 3606 | `	{ "MCAST_LEAVE_GROUP", MCAST_LEAVE_GROUP },` |
|       - | 3607 | `#endif` |
|       - | 3608 | `#ifdef MCAST_BLOCK_SOURCE` |
|       - | 3609 | `	{ "MCAST_BLOCK_SOURCE", MCAST_BLOCK_SOURCE },` |
|       - | 3610 | `#endif` |
|       - | 3611 | `#ifdef MCAST_UNBLOCK_SOURCE` |
|       - | 3612 | `	{ "MCAST_UNBLOCK_SOURCE", MCAST_UNBLOCK_SOURCE },` |
|       - | 3613 | `#endif` |
|       - | 3614 | `#ifdef MCAST_JOIN_SOURCE_GROUP` |
|       - | 3615 | `	{ "MCAST_JOIN_SOURCE_GROUP", MCAST_JOIN_SOURCE_GROUP },` |
|       - | 3616 | `#endif` |
|       - | 3617 | `#ifdef MCAST_LEAVE_SOURCE_GROUP` |
|       - | 3618 | `	{ "MCAST_LEAVE_SOURCE_GROUP", MCAST_LEAVE_SOURCE_GROUP },` |
|       - | 3619 | `#endif` |
|       - | 3620 | `#ifdef IP_MULTICAST_IF` |
|       - | 3621 | `	{ "IP_MULTICAST_IF", IP_MULTICAST_IF },` |
|       - | 3622 | `#endif` |
|       - | 3623 | `#ifdef IP_MULTICAST_TTL` |
|       - | 3624 | `	{ "IP_MULTICAST_TTL", IP_MULTICAST_TTL },` |
|       - | 3625 | `#endif` |
|       - | 3626 | `#ifdef IP_MULTICAST_LOOP` |
|       - | 3627 | `	{ "IP_MULTICAST_LOOP", IP_MULTICAST_LOOP },` |
|       - | 3628 | `#endif` |
|       - | 3629 | `#ifdef IP_BIND_ADDRESS_NO_PORT` |
|       - | 3630 | `	{ "IP_BIND_ADDRESS_NO_PORT", IP_BIND_ADDRESS_NO_PORT },` |
|       - | 3631 | `#endif` |
|       - | 3632 | `#ifdef IPV6_MULTICAST_IF` |
|       - | 3633 | `	{ "IPV6_MULTICAST_IF", IPV6_MULTICAST_IF },` |
|       - | 3634 | `#endif` |
|       - | 3635 | `#ifdef IPV6_MULTICAST_HOPS` |
|       - | 3636 | `	{ "IPV6_MULTICAST_HOPS", IPV6_MULTICAST_HOPS },` |
|       - | 3637 | `#endif` |
|       - | 3638 | `#ifdef IPV6_MULTICAST_LOOP` |
|       - | 3639 | `	{ "IPV6_MULTICAST_LOOP", IPV6_MULTICAST_LOOP },` |
|       - | 3640 | `#endif` |
|       - | 3641 | `#ifdef IPV6_V6ONLY` |
|       - | 3642 | `	{ "IPV6_V6ONLY", IPV6_V6ONLY },` |
|       - | 3643 | `#endif` |
|       - | 3644 | `#ifdef EPERM` |
|       - | 3645 | `	{ "SOCKET_EPERM", EPERM },` |
|       - | 3646 | `#endif` |
|       - | 3647 | `#ifdef ENOENT` |
|       - | 3648 | `	{ "SOCKET_ENOENT", ENOENT },` |
|       - | 3649 | `#endif` |
|       - | 3650 | `#ifdef __WINNT__` |
|       - | 3651 | `	{ "SOCKET_EINTR", WSAEINTR },` |
|       - | 3652 | `#elif defined(EINTR)` |
|       - | 3653 | `	{ "SOCKET_EINTR", EINTR },` |
|       - | 3654 | `#endif` |
|       - | 3655 | `#ifdef EIO` |
|       - | 3656 | `	{ "SOCKET_EIO", EIO },` |
|       - | 3657 | `#endif` |
|       - | 3658 | `#ifdef ENXIO` |
|       - | 3659 | `	{ "SOCKET_ENXIO", ENXIO },` |
|       - | 3660 | `#endif` |
|       - | 3661 | `#ifdef E2BIG` |
|       - | 3662 | `	{ "SOCKET_E2BIG", E2BIG },` |
|       - | 3663 | `#endif` |
|       - | 3664 | `#ifdef __WINNT__` |
|       - | 3665 | `	{ "SOCKET_EBADF", WSAEBADF },` |
|       - | 3666 | `#elif defined(EBADF)` |
|       - | 3667 | `	{ "SOCKET_EBADF", EBADF },` |
|       - | 3668 | `#endif` |
|       - | 3669 | `#ifdef __WINNT__` |
|       - | 3670 | `	{ "SOCKET_EAGAIN", WSAEWOULDBLOCK },` |
|       - | 3671 | `#elif defined(EAGAIN)` |
|       - | 3672 | `	{ "SOCKET_EAGAIN", EAGAIN },` |
|       - | 3673 | `#endif` |
|       - | 3674 | `#ifdef ENOMEM` |
|       - | 3675 | `	{ "SOCKET_ENOMEM", ENOMEM },` |
|       - | 3676 | `#endif` |
|       - | 3677 | `#ifdef __WINNT__` |
|       - | 3678 | `	{ "SOCKET_EACCES", WSAEACCES },` |
|       - | 3679 | `#elif defined(EACCES)` |
|       - | 3680 | `	{ "SOCKET_EACCES", EACCES },` |
|       - | 3681 | `#endif` |
|       - | 3682 | `#ifdef __WINNT__` |
|       - | 3683 | `	{ "SOCKET_EFAULT", WSAEFAULT },` |
|       - | 3684 | `#elif defined(EFAULT)` |
|       - | 3685 | `	{ "SOCKET_EFAULT", EFAULT },` |
|       - | 3686 | `#endif` |
|       - | 3687 | `#ifdef ENOTBLK` |
|       - | 3688 | `	{ "SOCKET_ENOTBLK", ENOTBLK },` |
|       - | 3689 | `#endif` |
|       - | 3690 | `#ifdef EBUSY` |
|       - | 3691 | `	{ "SOCKET_EBUSY", EBUSY },` |
|       - | 3692 | `#endif` |
|       - | 3693 | `#ifdef EEXIST` |
|       - | 3694 | `	{ "SOCKET_EEXIST", EEXIST },` |
|       - | 3695 | `#endif` |
|       - | 3696 | `#ifdef EXDEV` |
|       - | 3697 | `	{ "SOCKET_EXDEV", EXDEV },` |
|       - | 3698 | `#endif` |
|       - | 3699 | `#ifdef ENODEV` |
|       - | 3700 | `	{ "SOCKET_ENODEV", ENODEV },` |
|       - | 3701 | `#endif` |
|       - | 3702 | `#ifdef ENOTDIR` |
|       - | 3703 | `	{ "SOCKET_ENOTDIR", ENOTDIR },` |
|       - | 3704 | `#endif` |
|       - | 3705 | `#ifdef EISDIR` |
|       - | 3706 | `	{ "SOCKET_EISDIR", EISDIR },` |
|       - | 3707 | `#endif` |
|       - | 3708 | `#ifdef __WINNT__` |
|       - | 3709 | `	{ "SOCKET_EINVAL", WSAEINVAL },` |
|       - | 3710 | `#elif defined(EINVAL)` |
|       - | 3711 | `	{ "SOCKET_EINVAL", EINVAL },` |
|       - | 3712 | `#endif` |
|       - | 3713 | `#ifdef ENFILE` |
|       - | 3714 | `	{ "SOCKET_ENFILE", ENFILE },` |
|       - | 3715 | `#endif` |
|       - | 3716 | `#ifdef __WINNT__` |
|       - | 3717 | `	{ "SOCKET_EMFILE", WSAEMFILE },` |
|       - | 3718 | `#elif defined(EMFILE)` |
|       - | 3719 | `	{ "SOCKET_EMFILE", EMFILE },` |
|       - | 3720 | `#endif` |
|       - | 3721 | `#ifdef ENOTTY` |
|       - | 3722 | `	{ "SOCKET_ENOTTY", ENOTTY },` |
|       - | 3723 | `#endif` |
|       - | 3724 | `#ifdef ENOSPC` |
|       - | 3725 | `	{ "SOCKET_ENOSPC", ENOSPC },` |
|       - | 3726 | `#endif` |
|       - | 3727 | `#ifdef ESPIPE` |
|       - | 3728 | `	{ "SOCKET_ESPIPE", ESPIPE },` |
|       - | 3729 | `#endif` |
|       - | 3730 | `#ifdef EROFS` |
|       - | 3731 | `	{ "SOCKET_EROFS", EROFS },` |
|       - | 3732 | `#endif` |
|       - | 3733 | `#ifdef EMLINK` |
|       - | 3734 | `	{ "SOCKET_EMLINK", EMLINK },` |
|       - | 3735 | `#endif` |
|       - | 3736 | `#ifdef EPIPE` |
|       - | 3737 | `	{ "SOCKET_EPIPE", EPIPE },` |
|       - | 3738 | `#endif` |
|       - | 3739 | `#ifdef __WINNT__` |
|       - | 3740 | `	{ "SOCKET_ENAMETOOLONG", WSAENAMETOOLONG },` |
|       - | 3741 | `#elif defined(ENAMETOOLONG)` |
|       - | 3742 | `	{ "SOCKET_ENAMETOOLONG", ENAMETOOLONG },` |
|       - | 3743 | `#endif` |
|       - | 3744 | `#ifdef ENOLCK` |
|       - | 3745 | `	{ "SOCKET_ENOLCK", ENOLCK },` |
|       - | 3746 | `#endif` |
|       - | 3747 | `#ifdef ENOSYS` |
|       - | 3748 | `	{ "SOCKET_ENOSYS", ENOSYS },` |
|       - | 3749 | `#endif` |
|       - | 3750 | `#ifdef __WINNT__` |
|       - | 3751 | `	{ "SOCKET_ENOTEMPTY", WSAENOTEMPTY },` |
|       - | 3752 | `#elif defined(ENOTEMPTY)` |
|       - | 3753 | `	{ "SOCKET_ENOTEMPTY", ENOTEMPTY },` |
|       - | 3754 | `#endif` |
|       - | 3755 | `#ifdef __WINNT__` |
|       - | 3756 | `	{ "SOCKET_ELOOP", WSAELOOP },` |
|       - | 3757 | `#elif defined(ELOOP)` |
|       - | 3758 | `	{ "SOCKET_ELOOP", ELOOP },` |
|       - | 3759 | `#endif` |
|       - | 3760 | `#ifdef __WINNT__` |
|       - | 3761 | `	{ "SOCKET_EWOULDBLOCK", WSAEWOULDBLOCK },` |
|       - | 3762 | `#elif defined(EWOULDBLOCK)` |
|       - | 3763 | `	{ "SOCKET_EWOULDBLOCK", EWOULDBLOCK },` |
|       - | 3764 | `#endif` |
|       - | 3765 | `#ifdef ENOMSG` |
|       - | 3766 | `	{ "SOCKET_ENOMSG", ENOMSG },` |
|       - | 3767 | `#endif` |
|       - | 3768 | `#ifdef EIDRM` |
|       - | 3769 | `	{ "SOCKET_EIDRM", EIDRM },` |
|       - | 3770 | `#endif` |
|       - | 3771 | `#ifdef ECHRNG` |
|       - | 3772 | `	{ "SOCKET_ECHRNG", ECHRNG },` |
|       - | 3773 | `#endif` |
|       - | 3774 | `#ifdef EL2NSYNC` |
|       - | 3775 | `	{ "SOCKET_EL2NSYNC", EL2NSYNC },` |
|       - | 3776 | `#endif` |
|       - | 3777 | `#ifdef EL3HLT` |
|       - | 3778 | `	{ "SOCKET_EL3HLT", EL3HLT },` |
|       - | 3779 | `#endif` |
|       - | 3780 | `#ifdef EL3RST` |
|       - | 3781 | `	{ "SOCKET_EL3RST", EL3RST },` |
|       - | 3782 | `#endif` |
|       - | 3783 | `#ifdef ELNRNG` |
|       - | 3784 | `	{ "SOCKET_ELNRNG", ELNRNG },` |
|       - | 3785 | `#endif` |
|       - | 3786 | `#ifdef EUNATCH` |
|       - | 3787 | `	{ "SOCKET_EUNATCH", EUNATCH },` |
|       - | 3788 | `#endif` |
|       - | 3789 | `#ifdef ENOCSI` |
|       - | 3790 | `	{ "SOCKET_ENOCSI", ENOCSI },` |
|       - | 3791 | `#endif` |
|       - | 3792 | `#ifdef EL2HLT` |
|       - | 3793 | `	{ "SOCKET_EL2HLT", EL2HLT },` |
|       - | 3794 | `#endif` |
|       - | 3795 | `#ifdef EBADE` |
|       - | 3796 | `	{ "SOCKET_EBADE", EBADE },` |
|       - | 3797 | `#endif` |
|       - | 3798 | `#ifdef EBADR` |
|       - | 3799 | `	{ "SOCKET_EBADR", EBADR },` |
|       - | 3800 | `#endif` |
|       - | 3801 | `#ifdef EXFULL` |
|       - | 3802 | `	{ "SOCKET_EXFULL", EXFULL },` |
|       - | 3803 | `#endif` |
|       - | 3804 | `#ifdef ENOANO` |
|       - | 3805 | `	{ "SOCKET_ENOANO", ENOANO },` |
|       - | 3806 | `#endif` |
|       - | 3807 | `#ifdef EBADRQC` |
|       - | 3808 | `	{ "SOCKET_EBADRQC", EBADRQC },` |
|       - | 3809 | `#endif` |
|       - | 3810 | `#ifdef EBADSLT` |
|       - | 3811 | `	{ "SOCKET_EBADSLT", EBADSLT },` |
|       - | 3812 | `#endif` |
|       - | 3813 | `#ifdef ENOSTR` |
|       - | 3814 | `	{ "SOCKET_ENOSTR", ENOSTR },` |
|       - | 3815 | `#endif` |
|       - | 3816 | `#ifdef ENODATA` |
|       - | 3817 | `	{ "SOCKET_ENODATA", ENODATA },` |
|       - | 3818 | `#endif` |
|       - | 3819 | `#ifdef ETIME` |
|       - | 3820 | `	{ "SOCKET_ETIME", ETIME },` |
|       - | 3821 | `#endif` |
|       - | 3822 | `#ifdef ENOSR` |
|       - | 3823 | `	{ "SOCKET_ENOSR", ENOSR },` |
|       - | 3824 | `#endif` |
|       - | 3825 | `#ifdef ENONET` |
|       - | 3826 | `	{ "SOCKET_ENONET", ENONET },` |
|       - | 3827 | `#endif` |
|       - | 3828 | `#ifdef __WINNT__` |
|       - | 3829 | `	{ "SOCKET_EREMOTE", WSAEREMOTE },` |
|       - | 3830 | `#elif defined(EREMOTE)` |
|       - | 3831 | `	{ "SOCKET_EREMOTE", EREMOTE },` |
|       - | 3832 | `#endif` |
|       - | 3833 | `#ifdef ENOLINK` |
|       - | 3834 | `	{ "SOCKET_ENOLINK", ENOLINK },` |
|       - | 3835 | `#endif` |
|       - | 3836 | `#ifdef EADV` |
|       - | 3837 | `	{ "SOCKET_EADV", EADV },` |
|       - | 3838 | `#endif` |
|       - | 3839 | `#ifdef ESRMNT` |
|       - | 3840 | `	{ "SOCKET_ESRMNT", ESRMNT },` |
|       - | 3841 | `#endif` |
|       - | 3842 | `#ifdef ECOMM` |
|       - | 3843 | `	{ "SOCKET_ECOMM", ECOMM },` |
|       - | 3844 | `#endif` |
|       - | 3845 | `#ifdef EPROTO` |
|       - | 3846 | `	{ "SOCKET_EPROTO", EPROTO },` |
|       - | 3847 | `#endif` |
|       - | 3848 | `#ifdef EMULTIHOP` |
|       - | 3849 | `	{ "SOCKET_EMULTIHOP", EMULTIHOP },` |
|       - | 3850 | `#endif` |
|       - | 3851 | `#ifdef EBADMSG` |
|       - | 3852 | `	{ "SOCKET_EBADMSG", EBADMSG },` |
|       - | 3853 | `#endif` |
|       - | 3854 | `#ifdef ENOTUNIQ` |
|       - | 3855 | `	{ "SOCKET_ENOTUNIQ", ENOTUNIQ },` |
|       - | 3856 | `#endif` |
|       - | 3857 | `#ifdef EBADFD` |
|       - | 3858 | `	{ "SOCKET_EBADFD", EBADFD },` |
|       - | 3859 | `#endif` |
|       - | 3860 | `#ifdef EREMCHG` |
|       - | 3861 | `	{ "SOCKET_EREMCHG", EREMCHG },` |
|       - | 3862 | `#endif` |
|       - | 3863 | `#ifdef ERESTART` |
|       - | 3864 | `	{ "SOCKET_ERESTART", ERESTART },` |
|       - | 3865 | `#endif` |
|       - | 3866 | `#ifdef ESTRPIPE` |
|       - | 3867 | `	{ "SOCKET_ESTRPIPE", ESTRPIPE },` |
|       - | 3868 | `#endif` |
|       - | 3869 | `#ifdef __WINNT__` |
|       - | 3870 | `	{ "SOCKET_EUSERS", WSAEUSERS },` |
|       - | 3871 | `#elif defined(EUSERS)` |
|       - | 3872 | `	{ "SOCKET_EUSERS", EUSERS },` |
|       - | 3873 | `#endif` |
|       - | 3874 | `#ifdef __WINNT__` |
|       - | 3875 | `	{ "SOCKET_ENOTSOCK", WSAENOTSOCK },` |
|       - | 3876 | `#elif defined(ENOTSOCK)` |
|       - | 3877 | `	{ "SOCKET_ENOTSOCK", ENOTSOCK },` |
|       - | 3878 | `#endif` |
|       - | 3879 | `#ifdef __WINNT__` |
|       - | 3880 | `	{ "SOCKET_EDESTADDRREQ", WSAEDESTADDRREQ },` |
|       - | 3881 | `#elif defined(EDESTADDRREQ)` |
|       - | 3882 | `	{ "SOCKET_EDESTADDRREQ", EDESTADDRREQ },` |
|       - | 3883 | `#endif` |
|       - | 3884 | `#ifdef __WINNT__` |
|       - | 3885 | `	{ "SOCKET_EMSGSIZE", WSAEMSGSIZE },` |
|       - | 3886 | `#elif defined(EMSGSIZE)` |
|       - | 3887 | `	{ "SOCKET_EMSGSIZE", EMSGSIZE },` |
|       - | 3888 | `#endif` |
|       - | 3889 | `#ifdef __WINNT__` |
|       - | 3890 | `	{ "SOCKET_EPROTOTYPE", WSAEPROTOTYPE },` |
|       - | 3891 | `#elif defined(EPROTOTYPE)` |
|       - | 3892 | `	{ "SOCKET_EPROTOTYPE", EPROTOTYPE },` |
|       - | 3893 | `#endif` |
|       - | 3894 | `#ifdef __WINNT__` |
|       - | 3895 | `	{ "SOCKET_ENOPROTOOPT", WSAENOPROTOOPT },` |
|       - | 3896 | `#elif defined(ENOPROTOOPT)` |
|       - | 3897 | `	{ "SOCKET_ENOPROTOOPT", ENOPROTOOPT },` |
|       - | 3898 | `#endif` |
|       - | 3899 | `#ifdef __WINNT__` |
|       - | 3900 | `	{ "SOCKET_EPROTONOSUPPORT", WSAEPROTONOSUPPORT },` |
|       - | 3901 | `#elif defined(EPROTONOSUPPORT)` |
|       - | 3902 | `	{ "SOCKET_EPROTONOSUPPORT", EPROTONOSUPPORT },` |
|       - | 3903 | `#endif` |
|       - | 3904 | `#ifdef __WINNT__` |
|       - | 3905 | `	{ "SOCKET_ESOCKTNOSUPPORT", WSAESOCKTNOSUPPORT },` |
|       - | 3906 | `#elif defined(ESOCKTNOSUPPORT)` |
|       - | 3907 | `	{ "SOCKET_ESOCKTNOSUPPORT", ESOCKTNOSUPPORT },` |
|       - | 3908 | `#endif` |
|       - | 3909 | `#ifdef __WINNT__` |
|       - | 3910 | `	{ "SOCKET_EOPNOTSUPP", WSAEOPNOTSUPP },` |
|       - | 3911 | `#elif defined(EOPNOTSUPP)` |
|       - | 3912 | `	{ "SOCKET_EOPNOTSUPP", EOPNOTSUPP },` |
|       - | 3913 | `#endif` |
|       - | 3914 | `#ifdef __WINNT__` |
|       - | 3915 | `	{ "SOCKET_EPFNOSUPPORT", WSAEPFNOSUPPORT },` |
|       - | 3916 | `#elif defined(EPFNOSUPPORT)` |
|       - | 3917 | `	{ "SOCKET_EPFNOSUPPORT", EPFNOSUPPORT },` |
|       - | 3918 | `#endif` |
|       - | 3919 | `#ifdef __WINNT__` |
|       - | 3920 | `	{ "SOCKET_EAFNOSUPPORT", WSAEAFNOSUPPORT },` |
|       - | 3921 | `#elif defined(EAFNOSUPPORT)` |
|       - | 3922 | `	{ "SOCKET_EAFNOSUPPORT", EAFNOSUPPORT },` |
|       - | 3923 | `#endif` |
|       - | 3924 | `#ifdef __WINNT__` |
|       - | 3925 | `	{ "SOCKET_EADDRINUSE", WSAEADDRINUSE },` |
|       - | 3926 | `#elif defined(EADDRINUSE)` |
|       - | 3927 | `	{ "SOCKET_EADDRINUSE", EADDRINUSE },` |
|       - | 3928 | `#endif` |
|       - | 3929 | `#ifdef __WINNT__` |
|       - | 3930 | `	{ "SOCKET_EADDRNOTAVAIL", WSAEADDRNOTAVAIL },` |
|       - | 3931 | `#elif defined(EADDRNOTAVAIL)` |
|       - | 3932 | `	{ "SOCKET_EADDRNOTAVAIL", EADDRNOTAVAIL },` |
|       - | 3933 | `#endif` |
|       - | 3934 | `#ifdef __WINNT__` |
|       - | 3935 | `	{ "SOCKET_ENETDOWN", WSAENETDOWN },` |
|       - | 3936 | `#elif defined(ENETDOWN)` |
|       - | 3937 | `	{ "SOCKET_ENETDOWN", ENETDOWN },` |
|       - | 3938 | `#endif` |
|       - | 3939 | `#ifdef __WINNT__` |
|       - | 3940 | `	{ "SOCKET_ENETUNREACH", WSAENETUNREACH },` |
|       - | 3941 | `#elif defined(ENETUNREACH)` |
|       - | 3942 | `	{ "SOCKET_ENETUNREACH", ENETUNREACH },` |
|       - | 3943 | `#endif` |
|       - | 3944 | `#ifdef __WINNT__` |
|       - | 3945 | `	{ "SOCKET_ENETRESET", WSAENETRESET },` |
|       - | 3946 | `#elif defined(ENETRESET)` |
|       - | 3947 | `	{ "SOCKET_ENETRESET", ENETRESET },` |
|       - | 3948 | `#endif` |
|       - | 3949 | `#ifdef __WINNT__` |
|       - | 3950 | `	{ "SOCKET_ECONNABORTED", WSAECONNABORTED },` |
|       - | 3951 | `#elif defined(ECONNABORTED)` |
|       - | 3952 | `	{ "SOCKET_ECONNABORTED", ECONNABORTED },` |
|       - | 3953 | `#endif` |
|       - | 3954 | `#ifdef __WINNT__` |
|       - | 3955 | `	{ "SOCKET_ECONNRESET", WSAECONNRESET },` |
|       - | 3956 | `#elif defined(ECONNRESET)` |
|       - | 3957 | `	{ "SOCKET_ECONNRESET", ECONNRESET },` |
|       - | 3958 | `#endif` |
|       - | 3959 | `#ifdef __WINNT__` |
|       - | 3960 | `	{ "SOCKET_ENOBUFS", WSAENOBUFS },` |
|       - | 3961 | `#elif defined(ENOBUFS)` |
|       - | 3962 | `	{ "SOCKET_ENOBUFS", ENOBUFS },` |
|       - | 3963 | `#endif` |
|       - | 3964 | `#ifdef __WINNT__` |
|       - | 3965 | `	{ "SOCKET_EISCONN", WSAEISCONN },` |
|       - | 3966 | `#elif defined(EISCONN)` |
|       - | 3967 | `	{ "SOCKET_EISCONN", EISCONN },` |
|       - | 3968 | `#endif` |
|       - | 3969 | `#ifdef __WINNT__` |
|       - | 3970 | `	{ "SOCKET_ENOTCONN", WSAENOTCONN },` |
|       - | 3971 | `#elif defined(ENOTCONN)` |
|       - | 3972 | `	{ "SOCKET_ENOTCONN", ENOTCONN },` |
|       - | 3973 | `#endif` |
|       - | 3974 | `#ifdef __WINNT__` |
|       - | 3975 | `	{ "SOCKET_ESHUTDOWN", WSAESHUTDOWN },` |
|       - | 3976 | `#elif defined(ESHUTDOWN)` |
|       - | 3977 | `	{ "SOCKET_ESHUTDOWN", ESHUTDOWN },` |
|       - | 3978 | `#endif` |
|       - | 3979 | `#ifdef __WINNT__` |
|       - | 3980 | `	{ "SOCKET_ETOOMANYREFS", WSAETOOMANYREFS },` |
|       - | 3981 | `#elif defined(ETOOMANYREFS)` |
|       - | 3982 | `	{ "SOCKET_ETOOMANYREFS", ETOOMANYREFS },` |
|       - | 3983 | `#endif` |
|       - | 3984 | `#ifdef __WINNT__` |
|       - | 3985 | `	{ "SOCKET_ETIMEDOUT", WSAETIMEDOUT },` |
|       - | 3986 | `#elif defined(ETIMEDOUT)` |
|       - | 3987 | `	{ "SOCKET_ETIMEDOUT", ETIMEDOUT },` |
|       - | 3988 | `#endif` |
|       - | 3989 | `#ifdef __WINNT__` |
|       - | 3990 | `	{ "SOCKET_ECONNREFUSED", WSAECONNREFUSED },` |
|       - | 3991 | `#elif defined(ECONNREFUSED)` |
|       - | 3992 | `	{ "SOCKET_ECONNREFUSED", ECONNREFUSED },` |
|       - | 3993 | `#endif` |
|       - | 3994 | `#ifdef __WINNT__` |
|       - | 3995 | `	{ "SOCKET_EHOSTDOWN", WSAEHOSTDOWN },` |
|       - | 3996 | `#elif defined(EHOSTDOWN)` |
|       - | 3997 | `	{ "SOCKET_EHOSTDOWN", EHOSTDOWN },` |
|       - | 3998 | `#endif` |
|       - | 3999 | `#ifdef __WINNT__` |
|       - | 4000 | `	{ "SOCKET_EHOSTUNREACH", WSAEHOSTUNREACH },` |
|       - | 4001 | `#elif defined(EHOSTUNREACH)` |
|       - | 4002 | `	{ "SOCKET_EHOSTUNREACH", EHOSTUNREACH },` |
|       - | 4003 | `#endif` |
|       - | 4004 | `#ifdef __WINNT__` |
|       - | 4005 | `	{ "SOCKET_EALREADY", WSAEALREADY },` |
|       - | 4006 | `#elif defined(EALREADY)` |
|       - | 4007 | `	{ "SOCKET_EALREADY", EALREADY },` |
|       - | 4008 | `#endif` |
|       - | 4009 | `#ifdef __WINNT__` |
|       - | 4010 | `	{ "SOCKET_EINPROGRESS", WSAEINPROGRESS },` |
|       - | 4011 | `#elif defined(EINPROGRESS)` |
|       - | 4012 | `	{ "SOCKET_EINPROGRESS", EINPROGRESS },` |
|       - | 4013 | `#endif` |
|       - | 4014 | `#ifdef EISNAM` |
|       - | 4015 | `	{ "SOCKET_EISNAM", EISNAM },` |
|       - | 4016 | `#endif` |
|       - | 4017 | `#ifdef EREMOTEIO` |
|       - | 4018 | `	{ "SOCKET_EREMOTEIO", EREMOTEIO },` |
|       - | 4019 | `#endif` |
|       - | 4020 | `#ifdef __WINNT__` |
|       - | 4021 | `	{ "SOCKET_EDQUOT", WSAEDQUOT },` |
|       - | 4022 | `#elif defined(EDQUOT)` |
|       - | 4023 | `	{ "SOCKET_EDQUOT", EDQUOT },` |
|       - | 4024 | `#endif` |
|       - | 4025 | `#ifdef __WINNT__` |
|       - | 4026 | `	{ "SOCKET_ESTALE", WSAESTALE },` |
|       - | 4027 | `#endif` |
|       - | 4028 | `#ifdef __WINNT__` |
|       - | 4029 | `	{ "SOCKET_EDISCON", WSAEDISCON },` |
|       - | 4030 | `#endif` |
|       - | 4031 | `#ifdef __WINNT__` |
|       - | 4032 | `	{ "SOCKET_SYSNOTREADY", WSASYSNOTREADY },` |
|       - | 4033 | `#endif` |
|       - | 4034 | `#ifdef __WINNT__` |
|       - | 4035 | `	{ "SOCKET_VERNOTSUPPORTED", WSAVERNOTSUPPORTED },` |
|       - | 4036 | `#endif` |
|       - | 4037 | `#ifdef __WINNT__` |
|       - | 4038 | `	{ "SOCKET_NOTINITIALISED", WSANOTINITIALISED },` |
|       - | 4039 | `#endif` |
|       - | 4040 | `#ifdef __WINNT__` |
|       - | 4041 | `	{ "SOCKET_HOST_NOT_FOUND", WSAHOST_NOT_FOUND },` |
|       - | 4042 | `#endif` |
|       - | 4043 | `#ifdef __WINNT__` |
|       - | 4044 | `	{ "SOCKET_TRY_AGAIN", WSATRY_AGAIN },` |
|       - | 4045 | `#endif` |
|       - | 4046 | `#ifdef __WINNT__` |
|       - | 4047 | `	{ "SOCKET_NO_RECOVERY", WSANO_RECOVERY },` |
|       - | 4048 | `#endif` |
|       - | 4049 | `#ifdef __WINNT__` |
|       - | 4050 | `	{ "SOCKET_NO_DATA", WSANO_DATA },` |
|       - | 4051 | `#endif` |
|       - | 4052 | `#ifdef __WINNT__` |
|       - | 4053 | `	{ "SOCKET_NO_ADDRESS", WSANO_ADDRESS },` |
|       - | 4054 | `#endif` |
|       - | 4055 | `#ifdef ENOMEDIUM` |
|       - | 4056 | `	{ "SOCKET_ENOMEDIUM", ENOMEDIUM },` |
|       - | 4057 | `#endif` |
|       - | 4058 | `#ifdef EMEDIUMTYPE` |
|       - | 4059 | `	{ "SOCKET_EMEDIUMTYPE", EMEDIUMTYPE },` |
|       - | 4060 | `#endif` |
|       - | 4061 | `#ifdef IPPROTO_IP` |
|       - | 4062 | `	{ "IPPROTO_IP", IPPROTO_IP },` |
|       - | 4063 | `#endif` |
|       - | 4064 | `#if defined(__WINNT__) \|\| defined(IPPROTO_IPV6)` |
|       - | 4065 | `	{ "IPPROTO_IPV6", IPPROTO_IPV6 },` |
|       - | 4066 | `#endif` |
|       - | 4067 | `	/* php's are the protocol numbers on every platform; macOS and Windows have` |
|       - | 4068 | `	 * no SOL_TCP/SOL_UDP of their own. */` |
|       - | 4069 | `	{ "SOL_TCP", IPPROTO_TCP },` |
|       - | 4070 | `	{ "SOL_UDP", IPPROTO_UDP },` |
|       - | 4071 | `#if defined(SOL_UDPLITE)` |
|       - | 4072 | `	{ "SOL_UDPLITE", SOL_UDPLITE },` |
|       - | 4073 | `#elif defined(IPPROTO_UDPLITE)` |
|       - | 4074 | `	/* php names the LEVEL after the protocol where the platform has only` |
|       - | 4075 | `	 * the protocol, which is every Linux. */` |
|       - | 4076 | `	{ "SOL_UDPLITE", IPPROTO_UDPLITE },` |
|       - | 4077 | `#endif` |
|       - | 4078 | `#if defined(__WINNT__) \|\| defined(IPPROTO_ICMP)` |
|       - | 4079 | `	{ "IPPROTO_ICMP", IPPROTO_ICMP },` |
|       - | 4080 | `#endif` |
|       - | 4081 | `#if defined(__WINNT__) \|\| defined(IPPROTO_ICMPV6)` |
|       - | 4082 | `	{ "IPPROTO_ICMPV6", IPPROTO_ICMPV6 },` |
|       - | 4083 | `#endif` |
|       - | 4084 | `#ifdef IPV6_UNICAST_HOPS` |
|       - | 4085 | `	{ "IPV6_UNICAST_HOPS", IPV6_UNICAST_HOPS },` |
|       - | 4086 | `#endif` |
|       - | 4087 | `#ifdef AI_PASSIVE` |
|       - | 4088 | `	{ "AI_PASSIVE", AI_PASSIVE },` |
|       - | 4089 | `#endif` |
|       - | 4090 | `#ifdef AI_CANONNAME` |
|       - | 4091 | `	{ "AI_CANONNAME", AI_CANONNAME },` |
|       - | 4092 | `#endif` |
|       - | 4093 | `#ifdef AI_NUMERICHOST` |
|       - | 4094 | `	{ "AI_NUMERICHOST", AI_NUMERICHOST },` |
|       - | 4095 | `#endif` |
|       - | 4096 | `#ifdef AI_V4MAPPED` |
|       - | 4097 | `	{ "AI_V4MAPPED", AI_V4MAPPED },` |
|       - | 4098 | `#endif` |
|       - | 4099 | `#ifdef AI_ALL` |
|       - | 4100 | `	{ "AI_ALL", AI_ALL },` |
|       - | 4101 | `#endif` |
|       - | 4102 | `#ifdef AI_ADDRCONFIG` |
|       - | 4103 | `	{ "AI_ADDRCONFIG", AI_ADDRCONFIG },` |
|       - | 4104 | `#endif` |
|       - | 4105 | `#ifdef AI_IDN` |
|       - | 4106 | `	{ "AI_IDN", AI_IDN },` |
|       - | 4107 | `#endif` |
|       - | 4108 | `#ifdef AI_CANONIDN` |
|       - | 4109 | `	{ "AI_CANONIDN", AI_CANONIDN },` |
|       - | 4110 | `#endif` |
|       - | 4111 | `#ifdef AI_NUMERICSERV` |
|       - | 4112 | `	{ "AI_NUMERICSERV", AI_NUMERICSERV },` |
|       - | 4113 | `#endif` |
|       - | 4114 | `#ifdef __WINNT__` |
|       - | 4115 | `	{ "IPV6_RECVPKTINFO", IPV6_PKTINFO },` |
|       - | 4116 | `#elif defined(IPV6_RECVPKTINFO)` |
|       - | 4117 | `	{ "IPV6_RECVPKTINFO", IPV6_RECVPKTINFO },` |
|       - | 4118 | `#endif` |
|       - | 4119 | `#ifdef IPV6_PKTINFO` |
|       - | 4120 | `	{ "IPV6_PKTINFO", IPV6_PKTINFO },` |
|       - | 4121 | `#endif` |
|       - | 4122 | `#ifdef __WINNT__` |
|       - | 4123 | `	{ "IPV6_RECVHOPLIMIT", IPV6_HOPLIMIT },` |
|       - | 4124 | `#elif defined(IPV6_RECVHOPLIMIT)` |
|       - | 4125 | `	{ "IPV6_RECVHOPLIMIT", IPV6_RECVHOPLIMIT },` |
|       - | 4126 | `#endif` |
|       - | 4127 | `#ifdef IPV6_HOPLIMIT` |
|       - | 4128 | `	{ "IPV6_HOPLIMIT", IPV6_HOPLIMIT },` |
|       - | 4129 | `#endif` |
|       - | 4130 | `#ifdef IPV6_RECVTCLASS` |
|       - | 4131 | `	{ "IPV6_RECVTCLASS", IPV6_RECVTCLASS },` |
|       - | 4132 | `#endif` |
|       - | 4133 | `#ifdef IPV6_TCLASS` |
|       - | 4134 | `	{ "IPV6_TCLASS", IPV6_TCLASS },` |
|       - | 4135 | `#endif` |
|       - | 4136 | `#ifdef __WINNT__` |
|       - | 4137 | `	{ "SO_EXCLUSIVEADDRUSE", SO_EXCLUSIVEADDRUSE },` |
|       - | 4138 | `#endif` |
|       - | 4139 | `#ifdef SCM_RIGHTS` |
|       - | 4140 | `	{ "SCM_RIGHTS", SCM_RIGHTS },` |
|       - | 4141 | `#endif` |
|       - | 4142 | `#ifdef SCM_CREDENTIALS` |
|       - | 4143 | `	{ "SCM_CREDENTIALS", SCM_CREDENTIALS },` |
|       - | 4144 | `#endif` |
|       - | 4145 | `#ifdef SO_PASSCRED` |
|       - | 4146 | `	{ "SO_PASSCRED", SO_PASSCRED },` |
|       - | 4147 | `#endif` |
|       - | 4148 | `#ifdef SO_ATTACH_REUSEPORT_CBPF` |
|       - | 4149 | `	{ "SO_ATTACH_REUSEPORT_CBPF", SO_ATTACH_REUSEPORT_CBPF },` |
|       - | 4150 | `#endif` |
|       - | 4151 | `#ifdef SO_DETACH_FILTER` |
|       - | 4152 | `	{ "SO_DETACH_FILTER", SO_DETACH_FILTER },` |
|       - | 4153 | `#endif` |
|       - | 4154 | `#ifdef SO_DETACH_BPF` |
|       - | 4155 | `	{ "SO_DETACH_BPF", SO_DETACH_BPF },` |
|       - | 4156 | `#endif` |
|       - | 4157 | `#ifdef TCP_QUICKACK` |
|       - | 4158 | `	{ "TCP_QUICKACK", TCP_QUICKACK },` |
|       - | 4159 | `#endif` |
|       - | 4160 | `#ifdef TCP_REPAIR` |
|       - | 4161 | `	{ "TCP_REPAIR", TCP_REPAIR },` |
|       - | 4162 | `#endif` |
|       - | 4163 | `#ifdef IP_MTU_DISCOVER` |
|       - | 4164 | `	{ "IP_MTU_DISCOVER", IP_MTU_DISCOVER },` |
|       - | 4165 | `#endif` |
|       - | 4166 | `#ifdef IP_PMTUDISC_DO` |
|       - | 4167 | `	{ "IP_PMTUDISC_DO", IP_PMTUDISC_DO },` |
|       - | 4168 | `#endif` |
|       - | 4169 | `#ifdef IP_PMTUDISC_DONT` |
|       - | 4170 | `	{ "IP_PMTUDISC_DONT", IP_PMTUDISC_DONT },` |
|       - | 4171 | `#endif` |
|       - | 4172 | `#ifdef IP_PMTUDISC_WANT` |
|       - | 4173 | `	{ "IP_PMTUDISC_WANT", IP_PMTUDISC_WANT },` |
|       - | 4174 | `#endif` |
|       - | 4175 | `#ifdef IP_PMTUDISC_PROBE` |
|       - | 4176 | `	{ "IP_PMTUDISC_PROBE", IP_PMTUDISC_PROBE },` |
|       - | 4177 | `#endif` |
|       - | 4178 | `#ifdef IP_PMTUDISC_INTERFACE` |
|       - | 4179 | `	{ "IP_PMTUDISC_INTERFACE", IP_PMTUDISC_INTERFACE },` |
|       - | 4180 | `#endif` |
|       - | 4181 | `#ifdef IP_PMTUDISC_OMIT` |
|       - | 4182 | `	{ "IP_PMTUDISC_OMIT", IP_PMTUDISC_OMIT },` |
|       - | 4183 | `#endif` |
|       - | 4184 | `#ifdef ETH_P_IP` |
|       - | 4185 | `	{ "ETH_P_IP", ETH_P_IP },` |
|       - | 4186 | `#endif` |
|       - | 4187 | `#ifdef ETH_P_IPV6` |
|       - | 4188 | `	{ "ETH_P_IPV6", ETH_P_IPV6 },` |
|       - | 4189 | `#endif` |
|       - | 4190 | `#ifdef ETH_P_LOOP` |
|       - | 4191 | `	{ "ETH_P_LOOP", ETH_P_LOOP },` |
|       - | 4192 | `#endif` |
|       - | 4193 | `#ifdef ETH_P_ALL` |
|       - | 4194 | `	{ "ETH_P_ALL", ETH_P_ALL },` |
|       - | 4195 | `#endif` |
|       - | 4196 | `#ifdef UDP_SEGMENT` |
|       - | 4197 | `	{ "UDP_SEGMENT", UDP_SEGMENT },` |
|       - | 4198 | `#endif` |
|       - | 4199 | `#ifdef __WINNT__` |
|       - | 4200 | `	{ "SHUT_RD", SD_RECEIVE },` |
|       - | 4201 | `#elif defined(SHUT_RD)` |
|       - | 4202 | `	{ "SHUT_RD", SHUT_RD },` |
|       - | 4203 | `#endif` |
|       - | 4204 | `#ifdef __WINNT__` |
|       - | 4205 | `	{ "SHUT_WR", SD_SEND },` |
|       - | 4206 | `#elif defined(SHUT_WR)` |
|       - | 4207 | `	{ "SHUT_WR", SHUT_WR },` |
|       - | 4208 | `#endif` |
|       - | 4209 | `#ifdef __WINNT__` |
|       - | 4210 | `	{ "SHUT_RDWR", SD_BOTH },` |
|       - | 4211 | `#elif defined(SHUT_RDWR)` |
|       - | 4212 | `	{ "SHUT_RDWR", SHUT_RDWR },` |
|       - | 4213 | `#endif` |
|       - | 4214 | `};` |
|   13740 | 4215 | `static void SockConstExpand(ph7_value *pVal,void *pUserData)` |
|       4 | 4216 | `{` |
|   13744 | 4217 | `	ph7_value_int(pVal,SX_PTR_TO_INT(pUserData));` |
|   13744 | 4218 | `}` |
|    5619 | 4219 | `PH7_PRIVATE void PH7_RegisterSocketsConstants(ph7_vm *pVm)` |
|       5 | 4220 | `{` |
|       - | 4221 | `	sxu32 n;` |
| 1121396 | 4222 | `	for( n = 0 ; n < SX_ARRAYSIZE(aSockConst) ; ++n ){` |
| 1547747 | 4223 | `		ph7_create_constant(&(*pVm),aSockConst[n].zName,SockConstExpand,` |
| 1115772 | 4224 | `			SX_INT_TO_PTR(aSockConst[n].iValue));` |
|  431975 | 4225 | `	}` |
|    5624 | 4226 | `}` |
|       - | 4227 | `/* The extension's functions, in php's own order. */` |
|       - | 4228 | `static const ph7_builtin_func aSockFunc[] = {` |
|       - | 4229 | `	{ "socket_select",            vm_builtin_socket_select            },` |
|       - | 4230 | `	{ "socket_create_listen",     vm_builtin_socket_create_listen     },` |
|       - | 4231 | `	{ "socket_accept",            vm_builtin_socket_accept            },` |
|       - | 4232 | `	{ "socket_set_nonblock",      vm_builtin_socket_set_nonblock      },` |
|       - | 4233 | `	{ "socket_set_block",         vm_builtin_socket_set_block         },` |
|       - | 4234 | `	{ "socket_listen",            vm_builtin_socket_listen            },` |
|       - | 4235 | `	{ "socket_close",             vm_builtin_socket_close             },` |
|       - | 4236 | `	{ "socket_write",             vm_builtin_socket_write             },` |
|       - | 4237 | `	{ "socket_read",              vm_builtin_socket_read              },` |
|       - | 4238 | `	{ "socket_getsockname",       vm_builtin_socket_getsockname       },` |
|       - | 4239 | `	{ "socket_getpeername",       vm_builtin_socket_getpeername       },` |
|       - | 4240 | `	{ "socket_create",            vm_builtin_socket_create            },` |
|       - | 4241 | `	{ "socket_connect",           vm_builtin_socket_connect           },` |
|       - | 4242 | `	{ "socket_strerror",          vm_builtin_socket_strerror          },` |
|       - | 4243 | `	{ "socket_bind",              vm_builtin_socket_bind              },` |
|       - | 4244 | `	{ "socket_recv",              vm_builtin_socket_recv              },` |
|       - | 4245 | `	{ "socket_send",              vm_builtin_socket_send              },` |
|       - | 4246 | `	{ "socket_recvfrom",          vm_builtin_socket_recvfrom          },` |
|       - | 4247 | `	{ "socket_sendto",            vm_builtin_socket_sendto            },` |
|       - | 4248 | `	{ "socket_get_option",        vm_builtin_socket_get_option        },` |
|       - | 4249 | `	{ "socket_getopt",            vm_builtin_socket_get_option        },` |
|       - | 4250 | `	{ "socket_set_option",        vm_builtin_socket_set_option        },` |
|       - | 4251 | `	{ "socket_setopt",            vm_builtin_socket_set_option        },` |
|       - | 4252 | `	{ "socket_create_pair",       vm_builtin_socket_create_pair       },` |
|       - | 4253 | `	{ "socket_shutdown",          vm_builtin_socket_shutdown          },` |
|       - | 4254 | `#ifndef __WINNT__` |
|       - | 4255 | `	{ "socket_atmark",            vm_builtin_socket_atmark            },` |
|       - | 4256 | `#endif` |
|       - | 4257 | `	{ "socket_last_error",        vm_builtin_socket_last_error        },` |
|       - | 4258 | `	{ "socket_clear_error",       vm_builtin_socket_clear_error       },` |
|       - | 4259 | `	{ "socket_import_stream",     vm_builtin_socket_import_stream     },` |
|       - | 4260 | `	{ "socket_export_stream",     vm_builtin_socket_export_stream     },` |
|       - | 4261 | `	{ "socket_sendmsg",           vm_builtin_socket_sendmsg           },` |
|       - | 4262 | `	{ "socket_recvmsg",           vm_builtin_socket_recvmsg           },` |
|       - | 4263 | `	{ "socket_cmsg_space",        vm_builtin_socket_cmsg_space        },` |
|       - | 4264 | `	{ "socket_addrinfo_lookup",   vm_builtin_socket_addrinfo_lookup   },` |
|       - | 4265 | `	{ "socket_addrinfo_connect",  vm_builtin_socket_addrinfo_connect  },` |
|       - | 4266 | `	{ "socket_addrinfo_bind",     vm_builtin_socket_addrinfo_bind     },` |
|       - | 4267 | `	{ "socket_addrinfo_explain",  vm_builtin_socket_addrinfo_explain  }` |
|       - | 4268 | `#ifdef __WINNT__` |
|       - | 4269 | `	,` |
|       - | 4270 | `	/* Windows only, and php's own order puts them last. */` |
|       - | 4271 | `	{ "socket_wsaprotocol_info_export",  vm_builtin_socket_wsaprotocol_info_export  },` |
|       - | 4272 | `	{ "socket_wsaprotocol_info_import",  vm_builtin_socket_wsaprotocol_info_import  },` |
|       - | 4273 | `	{ "socket_wsaprotocol_info_release", vm_builtin_socket_wsaprotocol_info_release }` |
|       - | 4274 | `#endif` |
|       - | 4275 | `};` |
|    6721 | 4276 | `PH7_PRIVATE const ph7_builtin_func * PH7_SocketsFuncTable(sxu32 *pnEntry)` |
|       5 | 4277 | `{` |
|    6726 | 4278 | `	*pnEntry = SX_ARRAYSIZE(aSockFunc);` |
|    6726 | 4279 | `	return aSockFunc;` |
|       5 | 4280 | `}` |
|       - | 4281 | `/*` |
|       - | 4282 | `` * `Socket` and `AddressInfo`: two FINAL classes with no method, no constant and`` |
|       - | 4283 | `` * no property of their own, each refusing `new` in its own sentence and each`` |
|       - | 4284 | ``  * uncloneable and unserializable. They are NOT PH7_CLASS_HANDLE_ID -- `(int)$s` `` |
|       - | 4285 | ` * is php's ordinary "Object of class Socket could not be converted to int"` |
|       - | 4286 | ` * warning and 1, not the object handle -- and their comparison handler` |
|       - | 4287 | ` * recognizes nothing, so two distinct sockets are unequal even though both` |
|       - | 4288 | ` * present as an empty object.` |
|       - | 4289 | ` */` |
|    6721 | 4290 | `PH7_PRIVATE sxi32 PH7_VmInstallSockets(ph7_vm *pVm)` |
|       5 | 4291 | `{` |
|       - | 4292 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 4293 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }` |
|       - | 4294 | `	};` |
|       - | 4295 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 4296 | `		{ "Socket", 0, 0,` |
|       - | 4297 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|       - | 4298 | `		  0, 0, 0, 0,` |
|       - | 4299 | `		  aProp, SX_ARRAYSIZE(aProp),` |
|       - | 4300 | `		  SockInstanceRelease, 0, 0 },` |
|       - | 4301 | `		{ "AddressInfo", 0, 0,` |
|       - | 4302 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|       - | 4303 | `		  0, 0, 0, 0,` |
|       - | 4304 | `		  aProp, SX_ARRAYSIZE(aProp),` |
|       - | 4305 | `		  SockAddrInfoRelease, 0, 0 }` |
|       - | 4306 | `	};` |
|       - | 4307 | `	sxi32 rc;` |
|    6726 | 4308 | `	pVm->pSockets = 0;` |
|    6726 | 4309 | `	pVm->pAddrInfos = 0;` |
|    6726 | 4310 | `	pVm->iSocketLastErr = 0;` |
|    6726 | 4311 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    6726 | 4312 | `	if( rc == SXRET_OK ){` |
|       - | 4313 | `		static const struct { const char *zClass; const char *zMsg; } aRefusal[] = {` |
|       - | 4314 | `			{ "Socket",` |
|       - | 4315 | `			  "Cannot directly construct Socket, use socket_create() instead" },` |
|       - | 4316 | `			{ "AddressInfo",` |
|       - | 4317 | `			  "Cannot directly construct AddressInfo, use socket_addrinfo_lookup() instead" }` |
|       - | 4318 | `		};` |
|       - | 4319 | `		sxu32 n;` |
|   20168 | 4320 | `		for( n = 0 ; n < SX_ARRAYSIZE(aRefusal) ; ++n ){` |
|   20159 | 4321 | `			ph7_class *pClass = PH7_VmExtractClass(&(*pVm),aRefusal[n].zClass,` |
|   13442 | 4322 | `				(sxu32)SyStrlen(aRefusal[n].zClass),FALSE,0);` |
|   13447 | 4323 | `			if( pClass ){` |
|   13447 | 4324 | `				pClass->zNewRefusal = aRefusal[n].zMsg;` |
|   13447 | 4325 | `				pClass->xCmp = PH7_NativeCmpOpaqueHandle;` |
|    6712 | 4326 | `			}` |
|    6717 | 4327 | `		}` |
|    3356 | 4328 | `	}` |
|    6726 | 4329 | `	return rc;` |
|       5 | 4330 | `}` |
|       - | 4331 | `#else` |
|       - | 4332 | `/* Ensure a non-empty translation unit when the extension is out (MSVC C4206) */` |
|       - | 4333 | `typedef int builtin_sockets_unused;` |
|       - | 4334 | `#endif /* PH7_ENABLE_NET && !PH7_DISABLE_BUILTIN_FUNC */` |
|       - | 4335 |  |
