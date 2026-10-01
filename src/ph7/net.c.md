# src/ph7/net.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 455/555 lines (81.98%)

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
|    - |   29 | `#include <poll.h>` |
|    - |   30 | `#include <netinet/tcp.h>` |
|    - |   31 | `#endif` |
|    - |   32 |  |
|    - |   33 | `/*` |
|    - |   34 | `` * The `socket` context options that are a setsockopt() on a fresh socket. php`` |
|    - |   35 | ` * applies them to both halves — the one it binds and the one it connects — and` |
|    - |   36 | ` * ignores what the platform has no name for (SO_REUSEPORT is absent on Windows,` |
|    - |   37 | ` * where php's own code is #ifdef'd out the same way).` |
|    - |   38 | ` */` |
|    - |   39 | `/*` |
|    - |   40 | ` * Stamp a port into a resolved address. php resolves the HOST and then writes` |
|    - |   41 | ` * the port into each candidate itself, which is why the port is not a service` |
|    - |   42 | ` * NAME here: a negative or out-of-range one simply wraps in the 16 bits the` |
|    - |   43 | `` * wire has (php's own truncation, `-1` becoming 65535), where handing`` |
|    - |   44 | ` * getaddrinfo() the digits would make it an unresolvable address instead.` |
|    - |   45 | ` */` |
|  598 |   46 | `static void NetStampPort(struct sockaddr *pAddr,int iPort)` |
|    5 |   47 | `{` |
|  603 |   48 | `	if( pAddr == 0 ){` |
|  ! 0 |   49 | `		return;` |
|    - |   50 | `	}` |
|  603 |   51 | `	if( pAddr->sa_family == AF_INET ){` |
|  561 |   52 | `		((struct sockaddr_in *)pAddr)->sin_port = htons((unsigned short)iPort);` |
|  322 |   53 | `	}else if( pAddr->sa_family == AF_INET6 ){` |
|   44 |   54 | `		((struct sockaddr_in6 *)pAddr)->sin6_port = htons((unsigned short)iPort);` |
|   21 |   55 | `	}` |
|  304 |   56 | `}` |
|    - |   57 | `/* Defined with the address helpers further down; used by the three callers that` |
|    - |   58 | ` * report an address to a script. */` |
|    - |   59 | `static int NetFormatAddr(const struct sockaddr *pAddr,char *zBuf,int nBuf);` |
|    - |   60 | `/*` |
|    - |   61 | ` * inet_ntop() with a buffer size the platform's own prototype accepts: Winsock` |
|    - |   62 | `` * declares the last parameter `size_t` and POSIX `socklen_t`, and /W4 /WX will`` |
|    - |   63 | ` * not take a plain int for either.` |
|    - |   64 | ` */` |
|  160 |   65 | `static void NetInetNtop(int iFamily,const void *pSrc,char *zBuf,int nBuf)` |
|    4 |   66 | `{` |
|    - |   67 | `#ifdef __WINNT__` |
|    4 |   68 | `	inet_ntop(iFamily,pSrc,zBuf,(size_t)nBuf);` |
|    - |   69 | `#else` |
|  160 |   70 | `	inet_ntop(iFamily,pSrc,zBuf,(socklen_t)nBuf);` |
|    - |   71 | `#endif` |
|  164 |   72 | `}` |
|  598 |   73 | `static void NetApplySockOpts(ph7_socket sock,const ph7_sockopts *pOpt)` |
|    5 |   74 | `{` |
|  603 |   75 | `	int on = 1;` |
|  603 |   76 | `	if( pOpt == 0 ){` |
|  319 |   77 | `		return;` |
|    - |   78 | `	}` |
|  286 |   79 | `	if( pOpt->bReusePort ){` |
|    - |   80 | `#ifdef SO_REUSEPORT` |
|    4 |   81 | `		setsockopt(sock,SOL_SOCKET,SO_REUSEPORT,(const char *)&on,sizeof(on));` |
|    - |   82 | `#endif` |
|    2 |   83 | `	}` |
|  286 |   84 | `	if( pOpt->bNoDelay ){` |
|    - |   85 | `#ifdef TCP_NODELAY` |
|    8 |   86 | `		setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(const char *)&on,sizeof(on));` |
|    - |   87 | `#endif` |
|    3 |   88 | `	}` |
|  286 |   89 | `	if( pOpt->bBroadcast ){` |
|    - |   90 | ``		/* The one `socket` option that is a PERMISSION rather than a tuning`` |
|    - |   91 | `		 * knob: without it the OS refuses a datagram addressed to a broadcast` |
|    - |   92 | `		 * address outright (EACCES at the connect, or at the sendto for a bound` |
|    - |   93 | `		 * socket), so a script that asks for it and does not get it cannot tell` |
|    - |   94 | `		 * the difference between a wrong address and an unset flag. php reads` |
|    - |   95 | `		 * it on both halves, the bound one included. */` |
|    - |   96 | `#ifdef SO_BROADCAST` |
|  ! 0 |   97 | `		setsockopt(sock,SOL_SOCKET,SO_BROADCAST,(const char *)&on,sizeof(on));` |
|    - |   98 | `#endif` |
|  ! 0 |   99 | `	}` |
|  304 |  100 | `}` |
|    - |  101 | `/*` |
|    - |  102 | `` * `bindto`: the LOCAL address a client socket takes before it connects, which`` |
|    - |  103 | ` * is how a program picks the interface (or the source port) its connection goes` |
|    - |  104 | ` * out on. It is a NUMERIC literal and never a name (see the body), and when it` |
|    - |  105 | ` * cannot be used php warns and connects from wherever the routing table would` |
|    - |  106 | ` * have sent it — so a failure here is reported and never fatal.` |
|    - |  107 | ` */` |
|   10 |  108 | `static int NetBindLocal(ph7_socket sock,int iFamily,const char *zHost,int iPort,int *pErrno)` |
|    1 |  109 | `{` |
|    - |  110 | `	struct sockaddr_in sin;` |
|    - |  111 | `	struct sockaddr_in6 sin6;` |
|    - |  112 | `	struct sockaddr *pAddr;` |
|    - |  113 | `	ph7_socklen nAddr;` |
|    - |  114 | `	int rc;` |
|   11 |  115 | `	*pErrno = 0;` |
|    - |  116 | `	/* php reads a local address as a NUMERIC literal and nothing else — it is` |
|    - |  117 | ``	 * inet_pton(), not the resolver — so `bindto => 'localhost:0'` is an`` |
|    - |  118 | `	 * Invalid IP Address there and binds nothing. It is parsed in the family of` |
|    - |  119 | `	 * the socket that will carry it, so the answer describes THIS candidate and` |
|    - |  120 | `	 * not the address family net.c prefers. (php's bracketed spelling, and what` |
|    - |  121 | `	 * it does with a local address the candidate's family cannot take, is the` |
|    - |  122 | `	 * recorded gap §7.4 slice-1 (b)(iii) names.) */` |
|   11 |  123 | `	if( iFamily == AF_INET6 ){` |
|  ! 0 |  124 | `		memset(&sin6,0,sizeof(sin6));` |
|  ! 0 |  125 | `		sin6.sin6_family = AF_INET6;` |
|  ! 0 |  126 | `		sin6.sin6_port = htons((unsigned short)iPort);` |
|  ! 0 |  127 | `		if( inet_pton(AF_INET6,zHost,&sin6.sin6_addr) != 1 ){` |
|  ! 0 |  128 | `			return PH7_SOCKOPT_BIND_RESOLVE;` |
|    - |  129 | `		}` |
|  ! 0 |  130 | `		pAddr = (struct sockaddr *)&sin6;` |
|  ! 0 |  131 | `		nAddr = (ph7_socklen)sizeof(sin6);` |
|  ! 0 |  132 | `	}else{` |
|   11 |  133 | `		memset(&sin,0,sizeof(sin));` |
|   11 |  134 | `		sin.sin_family = AF_INET;` |
|   11 |  135 | `		sin.sin_port = htons((unsigned short)iPort);` |
|   11 |  136 | `		if( inet_pton(AF_INET,zHost,&sin.sin_addr) != 1 ){` |
|    5 |  137 | `			return PH7_SOCKOPT_BIND_RESOLVE;` |
|    - |  138 | `		}` |
|    7 |  139 | `		pAddr = (struct sockaddr *)&sin;` |
|    7 |  140 | `		nAddr = (ph7_socklen)sizeof(sin);` |
|    - |  141 | `	}` |
|    7 |  142 | `	rc = bind(sock,pAddr,nAddr);` |
|    7 |  143 | `	if( rc != 0 ){` |
|    5 |  144 | `		*pErrno = PH7_NetLastError();` |
|    5 |  145 | `		return PH7_SOCKOPT_BIND_REFUSED;` |
|    - |  146 | `	}` |
|    3 |  147 | `	return 0;` |
|    6 |  148 | `}` |
|    - |  149 | `/*` |
|    - |  150 | ` * The OS error the last socket call reported. A Windows socket does not touch` |
|    - |  151 | ` * errno at all, so a caller reading errno there reads whatever the last` |
|    - |  152 | ` * unrelated CRT call left behind.` |
|    - |  153 | ` */` |
|  151 |  154 | `PH7_PRIVATE int PH7_NetLastError(void)` |
|    5 |  155 | `{` |
|    - |  156 | `#ifdef __WINNT__` |
|    5 |  157 | `	return WSAGetLastError();` |
|    - |  158 | `#else` |
|  151 |  159 | `	return errno;` |
|    - |  160 | `#endif` |
|    5 |  161 | `}` |
|    - |  162 | `/*` |
|    - |  163 | ` * The message php words a socket failure with. php's own POSIX build answers` |
|    - |  164 | ` * strerror(), which is what a script comparing an $errstr against a documented` |
|    - |  165 | ` * text expects; a Winsock code is not an errno at all, so the codes a stream` |
|    - |  166 | ` * builtin can actually surface are worded here rather than handed to` |
|    - |  167 | ` * strerror() (which would answer for a completely different errno) — the` |
|    - |  168 | ` * FormatMessage() prose php's Windows build answers is its own (§7.4).` |
|    - |  169 | ` */` |
|  151 |  170 | `PH7_PRIVATE const char * PH7_NetStrError(int iErr)` |
|    4 |  171 | `{` |
|    - |  172 | `#ifdef __WINNT__` |
|    - |  173 | `	static const struct { int iCode; const char *zMsg; } aWsa[] = {` |
|    - |  174 | `		{ WSAEACCES,          "Permission denied" },` |
|    - |  175 | `		{ WSAEADDRINUSE,      "Address already in use" },` |
|    - |  176 | `		{ WSAEADDRNOTAVAIL,   "Cannot assign requested address" },` |
|    - |  177 | `		{ WSAEAFNOSUPPORT,    "Address family not supported by protocol" },` |
|    - |  178 | `		{ WSAECONNABORTED,    "Software caused connection abort" },` |
|    - |  179 | `		{ WSAECONNREFUSED,    "Connection refused" },` |
|    - |  180 | `		{ WSAECONNRESET,      "Connection reset by peer" },` |
|    - |  181 | `		{ WSAEHOSTUNREACH,    "No route to host" },` |
|    - |  182 | `		{ WSAEINVAL,          "Invalid argument" },` |
|    - |  183 | `		{ WSAEMFILE,          "Too many open files" },` |
|    - |  184 | `		{ WSAENETDOWN,        "Network is down" },` |
|    - |  185 | `		{ WSAENETUNREACH,     "Network is unreachable" },` |
|    - |  186 | `		{ WSAENOTCONN,        "Transport endpoint is not connected" },` |
|    - |  187 | `		{ WSAENOTSOCK,        "Socket operation on non-socket" },` |
|    - |  188 | `		{ WSAEOPNOTSUPP,      "Operation not supported" },` |
|    - |  189 | `		{ WSAEPROTONOSUPPORT, "Protocol not supported" },` |
|    - |  190 | `		{ WSAETIMEDOUT,       "Connection timed out" },` |
|    - |  191 | `		{ WSAEWOULDBLOCK,     "Resource temporarily unavailable" },` |
|    - |  192 | `		{ WSAEDESTADDRREQ,    "Destination address required" },` |
|    - |  193 | `		{ WSAEISCONN,         "Transport endpoint is already connected" },` |
|    - |  194 | `		{ WSAEMSGSIZE,        "Message too long" },` |
|    - |  195 | `		{ WSAENOBUFS,         "No buffer space available" },` |
|    - |  196 | `		{ WSAENOPROTOOPT,     "Protocol not available" },` |
|    - |  197 | `		/* A send on a socket whose write side is shut is POSIX's EPIPE, and it` |
|    - |  198 | `		 * is the same condition — the wording follows the errno rather than the` |
|    - |  199 | `		 * Winsock name so a script reading it reads one answer. */` |
|    - |  200 | `		{ WSAESHUTDOWN,       "Broken pipe" }` |
|    - |  201 | `	};` |
|    - |  202 | `	int i;` |
|    4 |  203 | `	for( i = 0 ; i < (int)(sizeof(aWsa)/sizeof(aWsa[0])) ; i++ ){` |
|    4 |  204 | `		if( aWsa[i].iCode == iErr ){` |
|    4 |  205 | `			return aWsa[i].zMsg;` |
|    - |  206 | `		}` |
|    4 |  207 | `	}` |
|  ! 0 |  208 | `	return "Unknown error";` |
|    - |  209 | `#else` |
|  151 |  210 | `	return strerror(iErr);` |
|    - |  211 | `#endif` |
|    4 |  212 | `}` |
|    - |  213 | `/*` |
|    - |  214 | ` * Did the last socket call fail only because it would have WAITED? That is the` |
|    - |  215 | ` * one failure php does not report as an error: a read answers "" or false by` |
|    - |  216 | ` * the handle's own rules, and a write answers what it managed to send.` |
|    - |  217 | ` */` |
|   40 |  218 | `PH7_PRIVATE int PH7_NetWouldBlock(void)` |
|    4 |  219 | `{` |
|    - |  220 | `#ifdef __WINNT__` |
|    4 |  221 | `	int iErr = WSAGetLastError();` |
|    4 |  222 | `	return iErr == WSAEWOULDBLOCK \|\| iErr == WSAETIMEDOUT;` |
|    - |  223 | `#else` |
|   40 |  224 | `	return errno == EAGAIN \|\| errno == EWOULDBLOCK \|\| errno == EINTR;` |
|    - |  225 | `#endif` |
|    4 |  226 | `}` |
|    - |  227 | `/*` |
|    - |  228 | ` * Wait until a socket is readable (bWrite == 0) or writable, for at most` |
|    - |  229 | ` * iTimeoutMs milliseconds (a negative timeout blocks). Answers 1 (ready),` |
|    - |  230 | ` * 0 (the wait expired) or -1 (the wait itself failed).` |
|    - |  231 | ` *` |
|    - |  232 | ` * POSIX takes poll() rather than select(): a descriptor at or above` |
|    - |  233 | ` * FD_SETSIZE cannot be put in an fd_set at all, and a long-running server is` |
|    - |  234 | ` * exactly the program that reaches those numbers.` |
|    - |  235 | ` */` |
|  136 |  236 | `PH7_PRIVATE int PH7_NetWait(ph7_socket sock,int bWrite,int iTimeoutMs)` |
|    4 |  237 | `{` |
|    - |  238 | `#ifdef __WINNT__` |
|    - |  239 | `	fd_set sSet;` |
|    4 |  240 | `	struct timeval tv,*pTv = 0;` |
|    - |  241 | `	int rc;` |
|    4 |  242 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    - |  243 | `		/* Nothing to wait on: php's own select() over a stream whose socket was` |
|    - |  244 | `		 * never created reports it not-ready, which is an expiry and not an` |
|    - |  245 | `		 * error. poll() would answer POLLNVAL and look like readiness. */` |
|    1 |  246 | `		return 0;` |
|    - |  247 | `	}` |
|    4 |  248 | `	FD_ZERO(&sSet);` |
|    4 |  249 | `	FD_SET(sock,&sSet);` |
|    4 |  250 | `	if( iTimeoutMs >= 0 ){` |
|    4 |  251 | `		tv.tv_sec = iTimeoutMs / 1000;` |
|    4 |  252 | `		tv.tv_usec = (iTimeoutMs % 1000) * 1000;` |
|    4 |  253 | `		pTv = &tv;` |
|    - |  254 | `	}` |
|    4 |  255 | `	rc = select(0,bWrite ? 0 : &sSet,bWrite ? &sSet : 0,0,pTv);` |
|    4 |  256 | `	return rc < 0 ? -1 : (rc > 0 ? 1 : 0);` |
|    - |  257 | `#else` |
|    - |  258 | `	struct pollfd sPoll;` |
|    - |  259 | `	int rc;` |
|  136 |  260 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    - |  261 | `		/* See above: nothing to wait on is an expiry, and poll() would answer` |
|    - |  262 | `		 * POLLNVAL for it, which looks like readiness. */` |
|    2 |  263 | `		return 0;` |
|    - |  264 | `	}` |
|  134 |  265 | `	sPoll.fd = sock;` |
|  134 |  266 | `	sPoll.events = (short)(bWrite ? POLLOUT : POLLIN);` |
|  134 |  267 | `	sPoll.revents = 0;` |
|   67 |  268 | `	for(;;){` |
|   67 |  269 | `		rc = poll(&sPoll,1,iTimeoutMs);` |
|  134 |  270 | `		if( rc < 0 && errno == EINTR ){` |
|    - |  271 | `			/* A signal is not an answer to the question that was asked. */` |
|  ! 0 |  272 | `			continue;` |
|    - |  273 | `		}` |
|  134 |  274 | `		break;` |
|    - |  275 | `	}` |
|  134 |  276 | `	return rc < 0 ? -1 : (rc > 0 ? 1 : 0);` |
|    - |  277 | `#endif` |
|   72 |  278 | `}` |
|    - |  279 |  |
|    - |  280 | `/*` |
|    - |  281 | ` * Initialize the networking subsystem.` |
|    - |  282 | ` * On Windows, calls WSAStartup(). On Unix, ignores SIGPIPE.` |
|    - |  283 | ` * Returns PH7_OK on success.` |
|    - |  284 | ` */` |
|  160 |  285 | `PH7_PRIVATE int PH7_NetInit(void)` |
|    5 |  286 | `{` |
|    - |  287 | `#ifdef __WINNT__` |
|    - |  288 | `	WSADATA wsaData;` |
|    5 |  289 | `	if( WSAStartup(MAKEWORD(2,2), &wsaData) != 0 ){` |
|  ! 0 |  290 | `		return PH7_IO_ERR;` |
|    - |  291 | `	}` |
|    - |  292 | `#else` |
|  160 |  293 | `	signal(SIGPIPE, SIG_IGN);` |
|    - |  294 | `#endif` |
|  165 |  295 | `	return PH7_OK;` |
|    5 |  296 | `}` |
|    - |  297 | `/*` |
|    - |  298 | ` * Start the networking subsystem the first time a socket is opened.` |
|    - |  299 | ` *` |
|    - |  300 | `` * PH7_NetInit() used to be called by the `-S` server and by nothing else, so`` |
|    - |  301 | ` * every socket a SCRIPT opened ran with the subsystem unstarted: on Windows` |
|    - |  302 | `` * that is `WSANOTINITIALISED` from the very first call — fsockopen() could`` |
|    - |  303 | ` * never connect at all — and on POSIX it left SIGPIPE at its default, so` |
|    - |  304 | ` * writing to a socket whose peer had closed KILLED the process (exit 141)` |
|    - |  305 | ` * where php answers false. Both are invisible from a POSIX-only reading of a` |
|    - |  306 | ` * program that never loses a peer.` |
|    - |  307 | ` */` |
|  670 |  308 | `PH7_PRIVATE int PH7_NetEnsureInit(void)` |
|    5 |  309 | `{` |
|    - |  310 | `	static int bReady = 0;` |
|  675 |  311 | `	if( bReady ){` |
|  546 |  312 | `		return PH7_OK;` |
|    - |  313 | `	}` |
|  133 |  314 | `	if( PH7_NetInit() != PH7_OK ){` |
|  ! 0 |  315 | `		return PH7_IO_ERR;` |
|    - |  316 | `	}` |
|  133 |  317 | `	bReady = 1;` |
|  133 |  318 | `	return PH7_OK;` |
|  340 |  319 | `}` |
|    - |  320 | `/*` |
|    - |  321 | ` * Cleanup the networking subsystem.` |
|    - |  322 | ` */` |
|   38 |  323 | `PH7_PRIVATE void PH7_NetCleanup(void)` |
|  ! 0 |  324 | `{` |
|    - |  325 | `#ifdef __WINNT__` |
|  ! 0 |  326 | `	WSACleanup();` |
|    - |  327 | `#endif` |
|   38 |  328 | `}` |
|    - |  329 | `/*` |
|    - |  330 | ` * Bind a socket to the given host and port, and — for a stream socket that is` |
|    - |  331 | `` * going to serve — listen on it. `stream_socket_server()` is the caller that`` |
|    - |  332 | ` * needs the two apart: php's STREAM_SERVER_LISTEN is a separate flag, and a` |
|    - |  333 | ` * datagram socket is bound WITHOUT ever listening. (A server asked for neither` |
|    - |  334 | ` * BIND nor LISTEN never reaches here at all — php creates no socket for it.)` |
|    - |  335 | ` *` |
|    - |  336 | ` * *pErrno receives the OS error and *pzErr its message, which is what the` |
|    - |  337 | ` * builtin reports through its by-ref out-params and its warning.` |
|    - |  338 | ` * Returns the socket, or PH7_NET_INVALID_SOCKET.` |
|    - |  339 | ` */` |
|   78 |  340 | `PH7_PRIVATE ph7_socket PH7_NetBind(const char *zHost, int iPort, int bDgram, int bListen,` |
|    - |  341 | `	int iBacklog, const ph7_sockopts *pOpt, int *pErrno, const char **pzErr)` |
|    4 |  342 | `{` |
|   82 |  343 | `	struct addrinfo hints, *res = 0, *rp;` |
|   82 |  344 | `	ph7_socket sock = PH7_NET_INVALID_SOCKET;` |
|   82 |  345 | `	int on = 1, iLastErr = 0;` |
|   82 |  346 | `	int iType = bDgram ? SOCK_DGRAM : SOCK_STREAM;` |
|   82 |  347 | `	if( pErrno ){ *pErrno = 0; }` |
|   82 |  348 | `	if( pzErr ){ *pzErr = ""; }` |
|   82 |  349 | `	if( PH7_NetEnsureInit() != PH7_OK ){` |
|  ! 0 |  350 | `		if( pzErr ){ *pzErr = "Unable to start the networking subsystem"; }` |
|  ! 0 |  351 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  352 | `	}` |
|    - |  353 | `	/* php RESOLVES the address it is asked to bind, family and all, and binds` |
|    - |  354 | ``	 * the first candidate that takes -- which is what makes `[::1]:0` an`` |
|    - |  355 | ``	 * AF_INET6 listener and `0.0.0.0:0` an AF_INET one. This used to build a`` |
|    - |  356 | `	 * sockaddr_in by hand with three special cases in front of it, so every` |
|    - |  357 | `	 * IPv6 address a script could write was refused by the resolver. */` |
|   82 |  358 | `	memset(&hints, 0, sizeof(hints));` |
|   82 |  359 | `	hints.ai_family = AF_UNSPEC;` |
|   82 |  360 | `	hints.ai_socktype = iType;` |
|   82 |  361 | `	hints.ai_flags = AI_PASSIVE;` |
|   82 |  362 | `	if( getaddrinfo((zHost && zHost[0]) ? zHost : 0, 0, &hints, &res) != 0 \|\| res == 0 ){` |
|    - |  363 | `		/* Nothing is open yet: the socket is created below. */` |
|  ! 0 |  364 | `		if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }` |
|  ! 0 |  365 | `		if( pzErr ){ *pzErr = 0; }` |
|  ! 0 |  366 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  367 | `	}` |
|   82 |  368 | `	if( pOpt && pOpt->iBacklog > 0 ){` |
|    - |  369 | ``		/* php's `backlog` context option: how many completed connections the OS`` |
|    - |  370 | `		 * may queue before the accept loop gets to them. */` |
|    5 |  371 | `		iBacklog = pOpt->iBacklog;` |
|    2 |  372 | `	}` |
|   84 |  373 | `	for( rp = res ; rp != 0 ; rp = rp->ai_next ){` |
|   82 |  374 | `		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);` |
|   82 |  375 | `		if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  376 | `			iLastErr = PH7_NetLastError();` |
|  ! 0 |  377 | `			continue;` |
|    - |  378 | `		}` |
|   82 |  379 | `		NetStampPort(rp->ai_addr, iPort);` |
|   82 |  380 | `		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));` |
|   82 |  381 | `		NetApplySockOpts(sock, pOpt);` |
|   82 |  382 | `		if( rp->ai_family == AF_INET6 && pOpt && pOpt->bV6Only ){` |
|    - |  383 | `#ifdef IPV6_V6ONLY` |
|    - |  384 | ``			/* php's `ipv6_v6only` context option, which had no consumer at all`` |
|    - |  385 | `			 * while nothing here could open an AF_INET6 socket. */` |
|  ! 0 |  386 | `			setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&on, sizeof(on));` |
|    - |  387 | `#endif` |
|  ! 0 |  388 | `		}` |
|   78 |  389 | `		if( bind(sock, rp->ai_addr, (ph7_socklen)rp->ai_addrlen) == 0` |
|   81 |  390 | `		 && (!bListen \|\| bDgram \|\| listen(sock, iBacklog) == 0) ){` |
|   80 |  391 | `			freeaddrinfo(res);` |
|   80 |  392 | `			return sock;` |
|    - |  393 | `		}` |
|    - |  394 | `		/* Read the error BEFORE closing the socket: close() is a call of its` |
|    - |  395 | `		 * own and overwrites what the failure left behind. */` |
|    2 |  396 | `		iLastErr = PH7_NetLastError();` |
|    2 |  397 | `		PH7_NetClose(sock);` |
|    2 |  398 | `		sock = PH7_NET_INVALID_SOCKET;` |
|    1 |  399 | `	}` |
|    2 |  400 | `	freeaddrinfo(res);` |
|    2 |  401 | `	if( pErrno ){ *pErrno = iLastErr; }` |
|    2 |  402 | `	if( pzErr ){ *pzErr = PH7_NetStrError(iLastErr); }` |
|    2 |  403 | `	return PH7_NET_INVALID_SOCKET;` |
|   43 |  404 | `}` |
|    - |  405 | `/*` |
|    - |  406 | ` * Create a TCP listening socket bound to the given host and port.` |
|    - |  407 | ` * Returns the socket descriptor, or PH7_NET_INVALID_SOCKET on error.` |
|    - |  408 | ` */` |
|   32 |  409 | `PH7_PRIVATE ph7_socket PH7_NetListen(const char *zHost, int iPort, int iBacklog)` |
|  ! 0 |  410 | `{` |
|   32 |  411 | `	return PH7_NetBind(zHost, iPort, 0, 1, iBacklog, 0, 0, 0);` |
|  ! 0 |  412 | `}` |
|    - |  413 | `/*` |
|    - |  414 | ` * The host's own name. Winsock's gethostname() wants the library started,` |
|    - |  415 | ` * which PH7_NetInit() has already done for any build that reaches here; on` |
|    - |  416 | ` * POSIX it is the plain unistd call. The buffer is left EMPTY and the OS code` |
|    - |  417 | ` * reported when the call fails, which is what php's warning names.` |
|    - |  418 | ` */` |
|    6 |  419 | `PH7_PRIVATE int PH7_NetHostName(char *zBuf, int nBuf, int *pErrno)` |
|    1 |  420 | `{` |
|    7 |  421 | `	if( zBuf == 0 \|\| nBuf < 2 ){` |
|  ! 0 |  422 | `		return -1;` |
|    - |  423 | `	}` |
|    7 |  424 | `	zBuf[0] = 0;` |
|    7 |  425 | `	if( gethostname(zBuf, (size_t)(nBuf - 1)) != 0 ){` |
|  ! 0 |  426 | `		if( pErrno ){ *pErrno = PH7_NetLastError(); }` |
|  ! 0 |  427 | `		zBuf[0] = 0;` |
|  ! 0 |  428 | `		return -1;` |
|    - |  429 | `	}` |
|    - |  430 | `	/* A name too long for the buffer is truncated rather than terminated on` |
|    - |  431 | `	 * some platforms; make the answer a C string either way. */` |
|    7 |  432 | `	zBuf[nBuf-1] = 0;` |
|    7 |  433 | `	return PH7_OK;` |
|    4 |  434 | `}` |
|    - |  435 | `/*` |
|    - |  436 | ` * The local (bPeer == 0) or the peer address of a socket, formatted the way` |
|    - |  437 | ` * php's stream_socket_get_name() answers it: "ip:port". Returns PH7_OK, or -1` |
|    - |  438 | ` * for a socket that has no such name (an unbound one, or the peer of a socket` |
|    - |  439 | `` * that is not connected) — which is php's `false`.`` |
|    - |  440 | ` */` |
|   72 |  441 | `PH7_PRIVATE int PH7_NetSockName(ph7_socket sock, int bPeer, char *zBuf, int nBuf)` |
|    4 |  442 | `{` |
|    - |  443 | `	struct sockaddr_storage addr;` |
|   76 |  444 | `	ph7_socklen nLen = (ph7_socklen)sizeof(addr);` |
|   76 |  445 | `	if( zBuf == 0 \|\| nBuf < 2 ){` |
|  ! 0 |  446 | `		return -1;` |
|    - |  447 | `	}` |
|   76 |  448 | `	zBuf[0] = 0;` |
|   76 |  449 | `	memset(&addr, 0, sizeof(addr));` |
|   80 |  450 | `	if( (bPeer ? getpeername(sock, (struct sockaddr *)&addr, &nLen)` |
|   84 |  451 | `	           : getsockname(sock, (struct sockaddr *)&addr, &nLen)) != 0 ){` |
|   16 |  452 | `		return -1;` |
|    - |  453 | `	}` |
|    - |  454 | `	/* A sockaddr_in is too small to hold the answer for an IPv6 socket -- the` |
|    - |  455 | `	 * OS truncates into whatever it is given -- so the buffer is the storage` |
|    - |  456 | `	 * union. A socketpair has no address of any kind, and php answers false for` |
|    - |  457 | `	 * one rather than inventing a name for it. */` |
|   64 |  458 | `	return NetFormatAddr((struct sockaddr *)&addr, zBuf, nBuf) ? PH7_OK : -1;` |
|   40 |  459 | `}` |
|    - |  460 | `/*` |
|    - |  461 | ` * accept() bounded by a timeout in milliseconds (a negative one blocks). The` |
|    - |  462 | ` * peer's address is written to zPeer when it fits, which is php's by-ref` |
|    - |  463 | ` * $peer_name out-param. *pbTimedOut tells a wait that EXPIRED — php's own` |
|    - |  464 | ` * "Accept failed: Connection timed out" — from a call that failed.` |
|    - |  465 | ` */` |
|   40 |  466 | `PH7_PRIVATE ph7_socket PH7_NetAcceptTimed(ph7_socket listenSock, int iTimeoutMs, int *pbTimedOut,` |
|    - |  467 | `	char *zPeer, int nPeer)` |
|    4 |  468 | `{` |
|    - |  469 | `	struct sockaddr_storage addr;` |
|   44 |  470 | `	ph7_socklen nLen = (ph7_socklen)sizeof(addr);` |
|    - |  471 | `	ph7_socket sock;` |
|   44 |  472 | `	if( pbTimedOut ){ *pbTimedOut = 0; }` |
|   44 |  473 | `	if( zPeer && nPeer > 0 ){ zPeer[0] = 0; }` |
|   44 |  474 | `	if( iTimeoutMs >= 0 ){` |
|   44 |  475 | `		int rc = PH7_NetWait(listenSock, 0, iTimeoutMs);` |
|   44 |  476 | `		if( rc == 0 ){` |
|    9 |  477 | `			if( pbTimedOut ){ *pbTimedOut = 1; }` |
|    9 |  478 | `			return PH7_NET_INVALID_SOCKET;` |
|    - |  479 | `		}` |
|   38 |  480 | `		if( rc < 0 ){` |
|  ! 0 |  481 | `			return PH7_NET_INVALID_SOCKET;` |
|    - |  482 | `		}` |
|   17 |  483 | `	}` |
|   38 |  484 | `	memset(&addr, 0, sizeof(addr));` |
|   38 |  485 | `	sock = accept(listenSock, (struct sockaddr *)&addr, &nLen);` |
|   38 |  486 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  487 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  488 | `	}` |
|   38 |  489 | `	if( zPeer && nPeer > 0 ){` |
|   38 |  490 | `		NetFormatAddr((struct sockaddr *)&addr, zPeer, nPeer);` |
|   17 |  491 | `	}` |
|   38 |  492 | `	return sock;` |
|   24 |  493 | `}` |
|    - |  494 | `/*` |
|    - |  495 | ` * Did the connect() merely START? php's asynchronous dial puts the socket in` |
|    - |  496 | ` * non-blocking mode first, and the "not finished yet" code IS the success it` |
|    - |  497 | ` * reports back to the script.` |
|    - |  498 | ` */` |
|    4 |  499 | `static int NetConnectInProgress(void)` |
|    1 |  500 | `{` |
|    - |  501 | `#ifdef __WINNT__` |
|    1 |  502 | `	int iErr = WSAGetLastError();` |
|    1 |  503 | `	return iErr == WSAEWOULDBLOCK \|\| iErr == WSAEINPROGRESS \|\| iErr == WSAEALREADY;` |
|    - |  504 | `#else` |
|    4 |  505 | `	return errno == EINPROGRESS \|\| errno == EINTR \|\| errno == EALREADY;` |
|    - |  506 | `#endif` |
|    1 |  507 | `}` |
|    - |  508 | `/*` |
|    - |  509 | ` * Connect a socket to the given host and port (blocking unless bAsync; the` |
|    - |  510 | ` * caller sets a timeout with PH7_NetSetTimeout afterwards). *pErrno receives` |
|    - |  511 | ` * the OS error code and *pzErr a static description on failure, matching what` |
|    - |  512 | ` * fsockopen() reports through its by-ref out-params.` |
|    - |  513 | ` * Returns the connected socket, or PH7_NET_INVALID_SOCKET on error.` |
|    - |  514 | ` */` |
|  520 |  515 | `PH7_PRIVATE ph7_socket PH7_NetConnect(const char *zHost, int iPort, int iTimeoutMs,` |
|    - |  516 | `	int bDgram, int bAsync, ph7_sockopts *pOpt, int *pErrno, const char **pzErr)` |
|    5 |  517 | `{` |
|  525 |  518 | `	struct addrinfo hints, *res = 0, *rp;` |
|  525 |  519 | `	ph7_socket sock = PH7_NET_INVALID_SOCKET;` |
|  525 |  520 | `	int iLastErr = 0;` |
|  525 |  521 | `	if( pErrno ){ *pErrno = 0; }` |
|  525 |  522 | `	if( pzErr ){ *pzErr = ""; }` |
|  525 |  523 | `	if( PH7_NetEnsureInit() != PH7_OK ){` |
|  ! 0 |  524 | `		if( pErrno ){ *pErrno = -1; }` |
|  ! 0 |  525 | `		if( pzErr ){ *pzErr = "Unable to start the networking subsystem"; }` |
|  ! 0 |  526 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  527 | `	}` |
|  525 |  528 | `	if( zHost == 0 \|\| zHost[0] == 0 ){` |
|    - |  529 | ``		/* An address whose host half is empty (`tcp://:9`, `[]:9`, an`` |
|    - |  530 | `		 * fsockopen() with no hostname) is a NAME php hands to the resolver` |
|    - |  531 | `		 * like any other, and the resolver is what refuses it -- twice, in the` |
|    - |  532 | `		 * two sentences the caller composes. This used to answer a message of` |
|    - |  533 | `		 * PHL's own, "Empty host" with an errno of -1, that no php prints; the` |
|    - |  534 | `		 * BIND half has always reported the resolver's. */` |
|    3 |  535 | `		if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }` |
|    3 |  536 | `		if( pzErr ){ *pzErr = 0; }` |
|    3 |  537 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  538 | `	}` |
|  523 |  539 | `	memset(&hints, 0, sizeof(hints));` |
|  523 |  540 | `	hints.ai_family = AF_UNSPEC;` |
|  523 |  541 | `	hints.ai_socktype = bDgram ? SOCK_DGRAM : SOCK_STREAM;` |
|  523 |  542 | `	if( getaddrinfo(zHost, 0, &hints, &res) != 0 \|\| res == 0 ){` |
|    - |  543 | `		/* php words the HOST into this one and reports no OS code for it; the` |
|    - |  544 | `		 * caller composes it, since only it has the name to interpolate. */` |
|  ! 0 |  545 | `		if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }` |
|  ! 0 |  546 | `		if( pzErr ){ *pzErr = 0; }` |
|  ! 0 |  547 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  548 | `	}` |
|  640 |  549 | `	for( rp = res ; rp != 0 ; rp = rp->ai_next ){` |
|  525 |  550 | `		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);` |
|  525 |  551 | `		if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  552 | `			iLastErr = PH7_NetLastError();` |
|  ! 0 |  553 | `			continue;` |
|    - |  554 | `		}` |
|  525 |  555 | `		NetStampPort(rp->ai_addr, iPort);` |
|  525 |  556 | `		NetApplySockOpts(sock, pOpt);` |
|  525 |  557 | `		if( pOpt ){` |
|    - |  558 | `			/* Per CANDIDATE: the failure that gets reported belongs to the socket` |
|    - |  559 | `			 * that ends up carrying the connection, not to one abandoned earlier.` |
|    - |  560 | `			 * php connects anyway when the local bind cannot be made; the caller` |
|    - |  561 | `			 * words the warning, since only it knows the function name. */` |
|  240 |  562 | `			pOpt->iBindErr = 0;` |
|  240 |  563 | `			pOpt->iBindErrno = 0;` |
|  240 |  564 | `			if( pOpt->zBindHost ){` |
|   16 |  565 | `				pOpt->iBindErr = NetBindLocal(sock, rp->ai_family, pOpt->zBindHost,` |
|    5 |  566 | `					pOpt->iBindPort, &pOpt->iBindErrno);` |
|    5 |  567 | `			}` |
|  118 |  568 | `		}` |
|  525 |  569 | `		if( iTimeoutMs > 0 ){` |
|    - |  570 | `			/* the connect() itself stays blocking; the timeout bounds the` |
|    - |  571 | `			 * subsequent recv/send (php applies it to both — recorded) */` |
|  490 |  572 | `			PH7_NetSetTimeout(sock, iTimeoutMs);` |
|  243 |  573 | `		}` |
|  525 |  574 | `		if( bAsync ){` |
|    - |  575 | `			/* php's STREAM_CLIENT_ASYNC_CONNECT: the socket is put in` |
|    - |  576 | `			 * non-blocking mode, connect() is ISSUED, and "in progress" is the` |
|    - |  577 | `			 * answer the caller gets -- so a dial to a port nothing is` |
|    - |  578 | `			 * listening on hands the script a working RESOURCE and reports the` |
|    - |  579 | `			 * refusal later, at the first write. php then puts the socket back` |
|    - |  580 | ``			 * in blocking mode, which is why the handle reports `blocked`. */`` |
|    - |  581 | `			int rcA;` |
|    5 |  582 | `			PH7_NetSetBlocking(sock, 0);` |
|    5 |  583 | `			rcA = connect(sock, rp->ai_addr, (ph7_socklen)rp->ai_addrlen);` |
|    5 |  584 | `			if( rcA == 0 \|\| NetConnectInProgress() ){` |
|    5 |  585 | `				PH7_NetSetBlocking(sock, 1);` |
|    5 |  586 | `				freeaddrinfo(res);` |
|    5 |  587 | `				return sock;` |
|    - |  588 | `			}` |
|  ! 0 |  589 | `			iLastErr = PH7_NetLastError();` |
|  ! 0 |  590 | `			PH7_NetClose(sock);` |
|  ! 0 |  591 | `			sock = PH7_NET_INVALID_SOCKET;` |
|  ! 0 |  592 | `			continue;` |
|    - |  593 | `		}` |
|  521 |  594 | `		if( connect(sock, rp->ai_addr, (ph7_socklen)rp->ai_addrlen) == 0 ){` |
|  403 |  595 | `			freeaddrinfo(res);` |
|  403 |  596 | `			return sock;` |
|    - |  597 | `		}` |
|    - |  598 | `		/* Read the code BEFORE closing: close() is a call of its own. php keeps` |
|    - |  599 | `		 * the LAST candidate's, which is what a script reads back from` |
|    - |  600 | `		 * $errno/$errstr -- this used to answer a hardcoded ECONNREFUSED for` |
|    - |  601 | `		 * every failure, so a broadcast address refused for want of` |
|    - |  602 | `		 * SO_BROADCAST, an unreachable network and a refused port were one` |
|    - |  603 | `		 * answer. */` |
|  120 |  604 | `		iLastErr = PH7_NetLastError();` |
|  120 |  605 | `		PH7_NetClose(sock);` |
|  120 |  606 | `		sock = PH7_NET_INVALID_SOCKET;` |
|   61 |  607 | `	}` |
|  117 |  608 | `	freeaddrinfo(res);` |
|  117 |  609 | `	if( iLastErr == 0 ){` |
|  ! 0 |  610 | `		iLastErr = 111; /* nothing was even tried: php's own default reason */` |
|  ! 0 |  611 | `	}` |
|  117 |  612 | `	if( pErrno ){ *pErrno = iLastErr; }` |
|  117 |  613 | `	if( pzErr ){ *pzErr = PH7_NetStrError(iLastErr); }` |
|  117 |  614 | `	return PH7_NET_INVALID_SOCKET;` |
|  265 |  615 | `}` |
|    - |  616 | `/*` |
|    - |  617 | ` * Accept an incoming connection on a listening socket.` |
|    - |  618 | ` * If pAddr and pAddrLen are non-NULL, the client address is stored there.` |
|    - |  619 | ` * Returns the client socket, or PH7_NET_INVALID_SOCKET on error.` |
|    - |  620 | ` */` |
|   90 |  621 | `PH7_PRIVATE ph7_socket PH7_NetAccept(ph7_socket listenSock, struct sockaddr *pAddr, ph7_socklen *pAddrLen)` |
|  ! 0 |  622 | `{` |
|   90 |  623 | `	return accept(listenSock, pAddr, pAddrLen);` |
|  ! 0 |  624 | `}` |
|    - |  625 | `/*` |
|    - |  626 | ` * Receive data from a socket.` |
|    - |  627 | ` * Returns the number of bytes received, or -1 on error.` |
|    - |  628 | ` */` |
|  594 |  629 | `PH7_PRIVATE int PH7_NetRecv(ph7_socket sock, void *pBuf, int nLen, int flags)` |
|    4 |  630 | `{` |
|  598 |  631 | `	return (int)recv(sock, (char *)pBuf, nLen, flags);` |
|    4 |  632 | `}` |
|    - |  633 | `/*` |
|    - |  634 | ` * Send data on a socket.` |
|    - |  635 | ` * Returns the number of bytes sent, or -1 on error.` |
|    - |  636 | ` */` |
|  109 |  637 | `PH7_PRIVATE int PH7_NetSend(ph7_socket sock, const void *pBuf, int nLen, int flags)` |
|    4 |  638 | `{` |
|  113 |  639 | `	return (int)send(sock, (const char *)pBuf, nLen, flags);` |
|    4 |  640 | `}` |
|    - |  641 | `/*` |
|    - |  642 | ` * Send all data on a socket, retrying on partial writes.` |
|    - |  643 | ` * Returns PH7_OK on success, PH7_IO_ERR on error.` |
|    - |  644 | ` */` |
|  522 |  645 | `PH7_PRIVATE int PH7_NetSendAll(ph7_socket sock, const void *pBuf, int nLen)` |
|  ! 0 |  646 | `{` |
|  522 |  647 | `	const char *zBuf = (const char *)pBuf;` |
|    - |  648 | `	int nSent;` |
| 1044 |  649 | `	while( nLen > 0 ){` |
|  522 |  650 | `		nSent = (int)send(sock, zBuf, nLen, 0);` |
|  522 |  651 | `		if( nSent <= 0 ){` |
|  ! 0 |  652 | `			return PH7_IO_ERR;` |
|    - |  653 | `		}` |
|  522 |  654 | `		zBuf += nSent;` |
|  522 |  655 | `		nLen -= nSent;` |
|  ! 0 |  656 | `	}` |
|  522 |  657 | `	return PH7_OK;` |
|  261 |  658 | `}` |
|    - |  659 | `/*` |
|    - |  660 | ` * Close a socket.` |
|    - |  661 | ` */` |
|  758 |  662 | `PH7_PRIVATE void PH7_NetClose(ph7_socket sock)` |
|    5 |  663 | `{` |
|  763 |  664 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|   13 |  665 | `		return;` |
|    - |  666 | `	}` |
|    - |  667 | `#ifdef __WINNT__` |
|    5 |  668 | `	closesocket(sock);` |
|    - |  669 | `#else` |
|  748 |  670 | `	close(sock);` |
|    - |  671 | `#endif` |
|  384 |  672 | `}` |
|    - |  673 | `/*` |
|    - |  674 | ` * Set a receive timeout on a socket (in milliseconds).` |
|    - |  675 | ` */` |
|  538 |  676 | `PH7_PRIVATE void PH7_NetSetTimeout(ph7_socket sock, int iMilliseconds)` |
|    4 |  677 | `{` |
|    - |  678 | `#ifdef __WINNT__` |
|    4 |  679 | `	DWORD tv = (DWORD)iMilliseconds;` |
|    4 |  680 | `	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));` |
|    - |  681 | `#else` |
|    - |  682 | `	struct timeval tv;` |
|  538 |  683 | `	tv.tv_sec = iMilliseconds / 1000;` |
|  538 |  684 | `	tv.tv_usec = (iMilliseconds % 1000) * 1000;` |
|  538 |  685 | `	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(tv));` |
|    - |  686 | `#endif` |
|  542 |  687 | `}` |
|    - |  688 | `/*` |
|    - |  689 | ` * Set BOTH the receive and the send timeout, which is what php's` |
|    - |  690 | ` * stream_set_timeout() means by "the timeout of this stream". A zero pair` |
|    - |  691 | ` * means "no timeout" to the OS, and that is php's answer for it too.` |
|    - |  692 | ` */` |
|  203 |  693 | `PH7_PRIVATE void PH7_NetSetRwTimeout(ph7_socket sock, ph7_int64 iSeconds, ph7_int64 iMicroseconds)` |
|    4 |  694 | `{` |
|    - |  695 | `	/* Carry the microseconds over: the OS rejects a tv_usec of 1000000 or more` |
|    - |  696 | ``	 * outright (EINVAL), so `stream_set_timeout($s, 0, 1500000)` used to set`` |
|    - |  697 | `	 * NOTHING and leave the read unbounded while answering true. */` |
|  207 |  698 | `	if( iMicroseconds >= 1000000 ){` |
|  ! 0 |  699 | `		iSeconds += iMicroseconds / 1000000;` |
|  ! 0 |  700 | `		iMicroseconds %= 1000000;` |
|  ! 0 |  701 | `	}` |
|  207 |  702 | `	if( iSeconds == 0 && iMicroseconds == 0 ){` |
|    - |  703 | `		/* php's zero timeout means "do not wait", where a zero timeval means` |
|    - |  704 | `		 * "wait forever" to the OS: the smallest one it can express is what` |
|    - |  705 | `		 * carries that intent. */` |
|  ! 0 |  706 | `		iMicroseconds = 1;` |
|  ! 0 |  707 | `	}` |
|  207 |  708 | `	if( iSeconds > 4000000 ){` |
|    - |  709 | `		/* Beyond any real deadline, and past what a millisecond DWORD holds. */` |
|  ! 0 |  710 | `		iSeconds = 4000000;` |
|  ! 0 |  711 | `	}` |
|    - |  712 | `#ifdef __WINNT__` |
|    - |  713 | `	{` |
|    4 |  714 | `		DWORD tv = (DWORD)(iSeconds * 1000 + iMicroseconds / 1000);` |
|    4 |  715 | `		if( tv == 0 ){` |
|  ! 0 |  716 | `			tv = 1; /* a Windows zero is "block forever" too */` |
|    - |  717 | `		}` |
|    4 |  718 | `		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));` |
|    4 |  719 | `		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));` |
|    - |  720 | `	}` |
|    - |  721 | `#else` |
|    - |  722 | `	{` |
|    - |  723 | `		struct timeval tv;` |
|  203 |  724 | `		tv.tv_sec = (time_t)iSeconds;` |
|  203 |  725 | `		tv.tv_usec = (suseconds_t)iMicroseconds;` |
|  203 |  726 | `		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(tv));` |
|  203 |  727 | `		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const void *)&tv, sizeof(tv));` |
|    - |  728 | `	}` |
|    - |  729 | `#endif` |
|  207 |  730 | `}` |
|    - |  731 | `/*` |
|    - |  732 | ` * Turn a socket's blocking mode on or off. On Windows a socket is not an fd,` |
|    - |  733 | ` * so the fcntl() route the rest of the stream family takes cannot reach it.` |
|    - |  734 | ` */` |
|   16 |  735 | `PH7_PRIVATE void PH7_NetSetBlocking(ph7_socket sock, int bBlocking)` |
|    4 |  736 | `{` |
|    - |  737 | `#ifdef __WINNT__` |
|    4 |  738 | `	u_long iMode = bBlocking ? 0 : 1;` |
|    4 |  739 | `	ioctlsocket(sock, FIONBIO, &iMode);` |
|    - |  740 | `#else` |
|   16 |  741 | `	int iFlags = fcntl(sock, F_GETFL, 0);` |
|   16 |  742 | `	if( iFlags < 0 ){` |
|  ! 0 |  743 | `		return;` |
|    - |  744 | `	}` |
|   16 |  745 | `	if( bBlocking ){` |
|    6 |  746 | `		iFlags &= ~O_NONBLOCK;` |
|    3 |  747 | `	}else{` |
|   10 |  748 | `		iFlags \|= O_NONBLOCK;` |
|    - |  749 | `	}` |
|   16 |  750 | `	fcntl(sock, F_SETFL, iFlags);` |
|    - |  751 | `#endif` |
|   12 |  752 | `}` |
|    - |  753 | `/*` |
|    - |  754 | ` * Extract a human-readable IP address string from a sockaddr.` |
|    - |  755 | ` * Writes at most nBufLen bytes (including NUL) to zBuf.` |
|    - |  756 | ` */` |
|  168 |  757 | `PH7_PRIVATE void PH7_NetAddrToString(const struct sockaddr *pAddr, char *zBuf, int nBufLen)` |
|    4 |  758 | `{` |
|  172 |  759 | `	if( nBufLen > 0 ){` |
|  172 |  760 | `		zBuf[0] = 0;` |
|   84 |  761 | `	}` |
|  172 |  762 | `	if( pAddr == 0 \|\| nBufLen < 2 ){` |
|  ! 0 |  763 | `		return;` |
|    - |  764 | `	}` |
|  172 |  765 | `	if( pAddr->sa_family == AF_INET ){` |
|  100 |  766 | `		const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;` |
|  100 |  767 | `		NetInetNtop(AF_INET, (const void *)&pIn->sin_addr, zBuf, nBufLen);` |
|  100 |  768 | `		return;` |
|    - |  769 | `	}` |
|   74 |  770 | `	if( pAddr->sa_family == AF_INET6 ){` |
|   65 |  771 | `		const struct sockaddr_in6 *pIn6 = (const struct sockaddr_in6 *)pAddr;` |
|   65 |  772 | `		NetInetNtop(AF_INET6, (const void *)&pIn6->sin6_addr, zBuf, nBufLen);` |
|   64 |  773 | `		return;` |
|    - |  774 | `	}` |
|    - |  775 | `	/* A socketpair, or anything else with no address of its own. */` |
|   88 |  776 | `}` |
|    - |  777 | `/*` |
|    - |  778 | ` * Extract the port number from a sockaddr (in host byte order).` |
|    - |  779 | ` */` |
|  160 |  780 | `PH7_PRIVATE int PH7_NetAddrPort(const struct sockaddr *pAddr)` |
|    4 |  781 | `{` |
|  164 |  782 | `	if( pAddr == 0 ){` |
|  ! 0 |  783 | `		return 0;` |
|    - |  784 | `	}` |
|  164 |  785 | `	if( pAddr->sa_family == AF_INET ){` |
|  100 |  786 | `		return (int)ntohs(((const struct sockaddr_in *)pAddr)->sin_port);` |
|    - |  787 | `	}` |
|   65 |  788 | `	if( pAddr->sa_family == AF_INET6 ){` |
|   65 |  789 | `		return (int)ntohs(((const struct sockaddr_in6 *)pAddr)->sin6_port);` |
|    - |  790 | `	}` |
|  ! 0 |  791 | `	return 0;` |
|   84 |  792 | `}` |
|    - |  793 | `/*` |
|    - |  794 | `` * One sockaddr as php's stream_socket_get_name() spells it: `ip:port` for IPv4`` |
|    - |  795 | `` * and `[ip]:port` for IPv6 -- the BRACKETED form, which is the same spelling`` |
|    - |  796 | ` * every address argument in the family reads back. Answers 0 when the address` |
|    - |  797 | `` * has no name at all (a socketpair), which is php's `false`.`` |
|    - |  798 | ` */` |
|  116 |  799 | `static int NetFormatAddr(const struct sockaddr *pAddr, char *zBuf, int nBuf)` |
|    4 |  800 | `{` |
|    - |  801 | `	char zIp[80];` |
|    - |  802 | `	int n;` |
|  120 |  803 | `	if( zBuf == 0 \|\| nBuf < 2 ){` |
|  ! 0 |  804 | `		return 0;` |
|    - |  805 | `	}` |
|  120 |  806 | `	zBuf[0] = 0;` |
|  120 |  807 | `	PH7_NetAddrToString(pAddr, zIp, (int)sizeof(zIp));` |
|  120 |  808 | `	if( zIp[0] == 0 ){` |
|    9 |  809 | `		return 0;` |
|    - |  810 | `	}` |
|  166 |  811 | `	n = pAddr->sa_family == AF_INET6` |
|   16 |  812 | `		? snprintf(zBuf, (size_t)nBuf, "[%s]:%d", zIp, PH7_NetAddrPort(pAddr))` |
|  100 |  813 | `		: snprintf(zBuf, (size_t)nBuf, "%s:%d", zIp, PH7_NetAddrPort(pAddr));` |
|  112 |  814 | `	if( n <= 0 \|\| n >= nBuf ){` |
|  ! 0 |  815 | `		zBuf[0] = 0;` |
|  ! 0 |  816 | `		return 0;` |
|    - |  817 | `	}` |
|  112 |  818 | `	return 1;` |
|   62 |  819 | `}` |
|    - |  820 |  |
|    - |  821 | `/*` |
|    - |  822 | ` * The PLATFORM numbers php's socket constants carry, which is why they cannot` |
|    - |  823 | `` * be written down in a header: `STREAM_PF_INET6` is 10 on Linux, 23 on Windows`` |
|    - |  824 | ` * and 30 on the BSDs, and a program that hands one of them to` |
|    - |  825 | ` * stream_socket_pair() is handing the OS its own value.` |
|    - |  826 | ` */` |
|  907 |  827 | `PH7_PRIVATE ph7_int64 PH7_NetSocketConst(int iWhich)` |
|    3 |  828 | `{` |
|  910 |  829 | `	switch( iWhich ){` |
|   72 |  830 | `	case PH7_NETC_PF_INET:      return AF_INET;` |
|   72 |  831 | `	case PH7_NETC_PF_INET6:     return AF_INET6;` |
|   78 |  832 | `	case PH7_NETC_PF_UNIX:      return AF_UNIX;` |
|   76 |  833 | `	case PH7_NETC_SOCK_STREAM:  return SOCK_STREAM;` |
|   72 |  834 | `	case PH7_NETC_SOCK_DGRAM:   return SOCK_DGRAM;` |
|   72 |  835 | `	case PH7_NETC_SOCK_RAW:     return SOCK_RAW;` |
|   72 |  836 | `	case PH7_NETC_SOCK_SEQPACKET: return SOCK_SEQPACKET;` |
|   72 |  837 | `	case PH7_NETC_SOCK_RDM:     return SOCK_RDM;` |
|   72 |  838 | `	case PH7_NETC_IPPROTO_IP:   return IPPROTO_IP;` |
|   72 |  839 | `	case PH7_NETC_IPPROTO_TCP:  return IPPROTO_TCP;` |
|   72 |  840 | `	case PH7_NETC_IPPROTO_UDP:  return IPPROTO_UDP;` |
|   72 |  841 | `	case PH7_NETC_IPPROTO_ICMP: return IPPROTO_ICMP;` |
|   72 |  842 | `	case PH7_NETC_IPPROTO_RAW:  return IPPROTO_RAW;` |
|  ! 0 |  843 | `	default: break;` |
|    - |  844 | `	}` |
|  ! 0 |  845 | `	return 0;` |
|  450 |  846 | `}` |
|    - |  847 | `/*` |
|    - |  848 | ` * shutdown(), which is how a program says "I am done SENDING" without closing` |
|    - |  849 | ` * the handle it still wants to read — the half-close every line protocol ends` |
|    - |  850 | ` * with. php's three modes are 0/1/2 in its own numbering.` |
|    - |  851 | ` */` |
|    6 |  852 | `PH7_PRIVATE int PH7_NetShutdown(ph7_socket sock,int iHow)` |
|    2 |  853 | `{` |
|    - |  854 | `	int iSys;` |
|    - |  855 | `	/* Winsock spells the three SD_RECEIVE/SD_SEND/SD_BOTH, with the same` |
|    - |  856 | `	 * numbers; POSIX spells them SHUT_*. */` |
|    - |  857 | `#ifdef __WINNT__` |
|    2 |  858 | `	switch( iHow ){` |
|    1 |  859 | `	case 0:  iSys = SD_RECEIVE; break;` |
|    1 |  860 | `	case 1:  iSys = SD_SEND; break;` |
|    1 |  861 | `	default: iSys = SD_BOTH; break;` |
|    - |  862 | `	}` |
|    - |  863 | `#else` |
|    6 |  864 | `	switch( iHow ){` |
|    2 |  865 | `	case 0:  iSys = SHUT_RD; break;` |
|    2 |  866 | `	case 1:  iSys = SHUT_WR; break;` |
|    2 |  867 | `	default: iSys = SHUT_RDWR; break;` |
|    - |  868 | `	}` |
|    - |  869 | `#endif` |
|    8 |  870 | `	return shutdown(sock,iSys) == 0 ? PH7_OK : -1;` |
|    2 |  871 | `}` |
|    - |  872 | `/*` |
|    - |  873 | ` * Is there anything left on this socket to hand over? Asked of a socket whose` |
|    - |  874 | ` * READ side has just been shut down, where a recv() cannot block: php answers` |
|    - |  875 | ` * feof() for a socket by probing it, so a half-close with bytes still queued is` |
|    - |  876 | ` * NOT an end of file and one with nothing queued is.` |
|    - |  877 | ` */` |
|    2 |  878 | `PH7_PRIVATE int PH7_NetAtEnd(ph7_socket sock)` |
|    2 |  879 | `{` |
|    - |  880 | `	char c;` |
|    4 |  881 | `	return recv(sock,&c,1,MSG_PEEK) <= 0 ? 1 : 0;` |
|    2 |  882 | `}` |
|    - |  883 | `/*` |
|    - |  884 | ` * php's feof() for a SOCKET is not a latch on a read that already happened: it` |
|    - |  885 | ` * is a liveness probe run at the moment the question is asked (zend's` |
|    - |  886 | ` * PHP_STREAM_OPTION_CHECK_LIVENESS). Poll the descriptor for readability with a` |
|    - |  887 | ` * zero timeout and, only if something IS there, peek one byte: a peek that` |
|    - |  888 | ` * comes back with data means the stream is alive, and one that comes back with` |
|    - |  889 | ` * nothing (or with an error that is not "would have waited") means the far end` |
|    - |  890 | ` * is gone. Everything else -- a listening socket with no connection queued, a` |
|    - |  891 | ` * bound datagram socket with no datagram -- is not readable and therefore not` |
|    - |  892 | ` * an end of file.` |
|    - |  893 | ` *` |
|    - |  894 | ` * The three answers this changes are all sockets nothing has read from yet: a` |
|    - |  895 | ` * bound-but-not-listening one, a connected one whose peer has departed, and a` |
|    - |  896 | ` * socketpair whose other end was closed. php reports EOF for all three before a` |
|    - |  897 | ` * single read; PHL reported false until a read came back empty, so a` |
|    - |  898 | `` * `while (!feof($sock))` loop written php's way ran one turn too many.`` |
|    - |  899 | ` */` |
|   96 |  900 | `PH7_PRIVATE int PH7_NetIsAlive(ph7_socket sock)` |
|    3 |  901 | `{` |
|    - |  902 | `	char c;` |
|   99 |  903 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  904 | `		return 0;` |
|    - |  905 | `	}` |
|   99 |  906 | `	if( PH7_NetWait(sock,0,0) <= 0 ){` |
|    - |  907 | `		/* Not readable, or the wait itself failed: neither is evidence of an` |
|    - |  908 | `		 * end, and php only looks further when the poll says there is` |
|    - |  909 | `		 * something to look at. */` |
|   68 |  910 | `		return 1;` |
|    - |  911 | `	}` |
|    - |  912 | `#ifndef __WINNT__` |
|   30 |  913 | `	errno = 0; /* so a STALE EAGAIN cannot answer for this call's recv() */` |
|    - |  914 | `#endif` |
|   32 |  915 | `	if( recv(sock,&c,1,MSG_PEEK) > 0 ){` |
|    6 |  916 | `		return 1;` |
|    - |  917 | `	}` |
|   26 |  918 | `	return PH7_NetWouldBlock() ? 1 : 0;` |
|   51 |  919 | `}` |
|    - |  920 | `/* php's STREAM_OOB/STREAM_PEEK are php's own bits, not the OS's. */` |
|   40 |  921 | `static int NetMsgFlags(int iFlags)` |
|    3 |  922 | `{` |
|   43 |  923 | `	int iOut = 0;` |
|   43 |  924 | `	if( iFlags & PH7_STREAM_OOB ){` |
|  ! 0 |  925 | `		iOut \|= MSG_OOB;` |
|  ! 0 |  926 | `	}` |
|   43 |  927 | `	if( iFlags & PH7_STREAM_PEEK ){` |
|    6 |  928 | `		iOut \|= MSG_PEEK;` |
|    2 |  929 | `	}` |
|   43 |  930 | `	return iOut;` |
|    3 |  931 | `}` |
|    - |  932 | `/*` |
|    - |  933 | ` * Receive straight from the socket, with the sender's address when the datagram` |
|    - |  934 | ` * carries one. This deliberately does NOT go through the handle's read buffer:` |
|    - |  935 | ` * php's own recvfrom() asks the SOCKET, which is why it blocks on a handle whose` |
|    - |  936 | ` * buffer still holds bytes.` |
|    - |  937 | ` */` |
|   24 |  938 | `PH7_PRIVATE int PH7_NetRecvFrom(ph7_socket sock,void *pBuf,int nLen,int iFlags,char *zAddr,int nAddr)` |
|    2 |  939 | `{` |
|    - |  940 | `	struct sockaddr_storage sFrom;` |
|   26 |  941 | `	ph7_socklen nFrom = (ph7_socklen)sizeof(sFrom);` |
|    - |  942 | `	int n;` |
|   26 |  943 | `	if( zAddr && nAddr > 0 ){` |
|   26 |  944 | `		zAddr[0] = 0;` |
|   12 |  945 | `	}` |
|   26 |  946 | `	memset(&sFrom,0,sizeof(sFrom));` |
|   26 |  947 | `	n = (int)recvfrom(sock,(char *)pBuf,nLen,NetMsgFlags(iFlags),(struct sockaddr *)&sFrom,&nFrom);` |
|    - |  948 | `#ifdef __WINNT__` |
|    2 |  949 | `	if( n < 0 && WSAGetLastError() == WSAEMSGSIZE ){` |
|    - |  950 | `		/* A datagram LONGER than the buffer is a truncated read on POSIX and a` |
|    - |  951 | `		 * failed call on Winsock -- which fills the buffer anyway and reports` |
|    - |  952 | `		 * WSAEMSGSIZE for the part it dropped. php hands the truncated bytes` |
|    - |  953 | ``		 * back on both, so the two answer the same `wor` for a three-byte read`` |
|    - |  954 | ``		 * of `world`; without this the whole datagram was lost to a false. */`` |
|    1 |  955 | `		n = nLen;` |
|    - |  956 | `	}` |
|    - |  957 | `#endif` |
|   26 |  958 | `	if( n >= 0 && zAddr && nAddr > 0 ){` |
|   24 |  959 | `		NetFormatAddr((struct sockaddr *)&sFrom,zAddr,nAddr);` |
|   11 |  960 | `	}` |
|   26 |  961 | `	return n;` |
|    2 |  962 | `}` |
|    - |  963 | `/*` |
|    - |  964 | ` * Send straight to the socket, to a named address when one is given. A` |
|    - |  965 | ` * connected socket takes the address too — php passes it to sendto() and lets` |
|    - |  966 | ` * the OS decide, which for a connected TCP socket means it is simply sent.` |
|    - |  967 | ` */` |
|   16 |  968 | `PH7_PRIVATE int PH7_NetSendTo(ph7_socket sock,const void *pBuf,int nLen,int iFlags,` |
|    - |  969 | `	const char *zHost,int iPort,int *pErrno)` |
|    3 |  970 | `{` |
|    - |  971 | `	struct sockaddr_storage sTo;` |
|    - |  972 | `	ph7_socklen nTo;` |
|   19 |  973 | `	if( pErrno ){ *pErrno = 0; }` |
|   19 |  974 | `	if( zHost == 0 ){` |
|    - |  975 | `		/* No $address at all: php's plain send() to whatever the socket is` |
|    - |  976 | `		 * connected to. An EMPTY host is not this case -- it is a name the` |
|    - |  977 | ``		 * resolver refuses, which is what php answers for `stream_socket_sendto`` |
|    - |  978 | ``		 * ($s, $d, 0, ':53')`. */`` |
|   10 |  979 | `		return (int)send(sock,(const char *)pBuf,nLen,NetMsgFlags(iFlags));` |
|    - |  980 | `	}` |
|   11 |  981 | `	memset(&sTo,0,sizeof(sTo));` |
|    - |  982 | `	{` |
|    - |  983 | `		/* php's own ladder: a NUMERIC literal is used as written -- in either` |
|    - |  984 | `		 * family -- and only a name goes to the resolver. The family that comes` |
|    - |  985 | `		 * back is the one sendto() is handed, so an IPv6 target on an IPv4` |
|    - |  986 | `		 * socket is the OS's own refusal rather than a silent send elsewhere. */` |
|   11 |  987 | `		struct sockaddr_in *pIn = (struct sockaddr_in *)&sTo;` |
|   11 |  988 | `		struct sockaddr_in6 *pIn6 = (struct sockaddr_in6 *)&sTo;` |
|   11 |  989 | `		if( inet_pton(AF_INET,zHost,(void *)&pIn->sin_addr) == 1 ){` |
|    9 |  990 | `			pIn->sin_family = AF_INET;` |
|    9 |  991 | `			pIn->sin_port = htons((unsigned short)iPort);` |
|    9 |  992 | `			nTo = (ph7_socklen)sizeof(*pIn);` |
|    6 |  993 | `		}else if( inet_pton(AF_INET6,zHost,(void *)&pIn6->sin6_addr) == 1 ){` |
|    3 |  994 | `			pIn6->sin6_family = AF_INET6;` |
|    3 |  995 | `			pIn6->sin6_port = htons((unsigned short)iPort);` |
|    3 |  996 | `			nTo = (ph7_socklen)sizeof(*pIn6);` |
|    2 |  997 | `		}else{` |
|    - |  998 | `			struct addrinfo hints,*res;` |
|  ! 0 |  999 | `			memset(&hints,0,sizeof(hints));` |
|  ! 0 | 1000 | `			hints.ai_family = AF_UNSPEC;` |
|  ! 0 | 1001 | `			hints.ai_socktype = SOCK_DGRAM;` |
|  ! 0 | 1002 | `			if( zHost[0] == 0 \|\| getaddrinfo(zHost,0,&hints,&res) != 0 \|\| res == 0 ){` |
|    - | 1003 | `				/* Nothing was sent, and the caller says so in php's own three` |
|    - | 1004 | `				 * voices — which is a different answer from a send that failed. */` |
|  ! 0 | 1005 | `				if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }` |
|  ! 0 | 1006 | `				return -1;` |
|    - | 1007 | `			}` |
|  ! 0 | 1008 | `			memcpy(&sTo,res->ai_addr,(size_t)res->ai_addrlen);` |
|  ! 0 | 1009 | `			nTo = (ph7_socklen)res->ai_addrlen;` |
|  ! 0 | 1010 | `			NetStampPort((struct sockaddr *)&sTo,iPort);` |
|  ! 0 | 1011 | `			freeaddrinfo(res);` |
|    - | 1012 | `		}` |
|    - | 1013 | `	}` |
|   15 | 1014 | `	return (int)sendto(sock,(const char *)pBuf,nLen,NetMsgFlags(iFlags),` |
|    4 | 1015 | `		(struct sockaddr *)&sTo,nTo);` |
|   11 | 1016 | `}` |
|    - | 1017 | `/*` |
|    - | 1018 | ` * A connected PAIR of sockets, which is what a program hands a child process (or` |
|    - | 1019 | ` * a test double) as a two-way pipe. POSIX has the call; Windows does not, and` |
|    - | 1020 | ` * php builds the pair over the loopback there — a listener nobody else can` |
|    - | 1021 | ` * reach, one connect, one accept, and a check that the socket that arrived is` |
|    - | 1022 | ` * the one that was dialled.` |
|    - | 1023 | ` */` |
|    6 | 1024 | `PH7_PRIVATE int PH7_NetSocketPair(int iDomain,int iType,int iProtocol,ph7_socket *aOut,int *pErrno)` |
|    2 | 1025 | `{` |
|    8 | 1026 | `	if( pErrno ){ *pErrno = 0; }` |
|    8 | 1027 | `	aOut[0] = aOut[1] = PH7_NET_INVALID_SOCKET;` |
|    8 | 1028 | `	if( PH7_NetEnsureInit() != PH7_OK ){` |
|  ! 0 | 1029 | `		return -1;` |
|    - | 1030 | `	}` |
|    - | 1031 | `#ifndef __WINNT__` |
|    - | 1032 | `	{` |
|    - | 1033 | `		int aFd[2];` |
|    6 | 1034 | `		if( socketpair(iDomain,iType,iProtocol,aFd) != 0 ){` |
|    2 | 1035 | `			if( pErrno ){ *pErrno = PH7_NetLastError(); }` |
|    2 | 1036 | `			return -1;` |
|    - | 1037 | `		}` |
|    4 | 1038 | `		aOut[0] = aFd[0];` |
|    4 | 1039 | `		aOut[1] = aFd[1];` |
|    4 | 1040 | `		return PH7_OK;` |
|    - | 1041 | `	}` |
|    - | 1042 | `#else` |
|    - | 1043 | `	{` |
|    - | 1044 | `		ph7_socket listener,client,server;` |
|    - | 1045 | `		struct sockaddr_in addr,peer,self;` |
|    - | 1046 | `		ph7_socklen nAddr;` |
|    - | 1047 | `		SXUNUSED(iProtocol);` |
|    2 | 1048 | `		if( iDomain != AF_INET \|\| iType != SOCK_STREAM ){` |
|    - | 1049 | `			/* php's own Windows emulation covers exactly this pair, and answers` |
|    - | 1050 | `			 * "protocol not available" for anything else — the mirror image of` |
|    - | 1051 | `			 * POSIX, where AF_UNIX is the one that works. */` |
|    2 | 1052 | `			if( pErrno ){ *pErrno = WSAENOPROTOOPT; }` |
|    2 | 1053 | `			return -1;` |
|    - | 1054 | `		}` |
|    2 | 1055 | `		listener = PH7_NetBind("127.0.0.1",0,0,1,1,0,pErrno,0);` |
|    2 | 1056 | `		if( listener == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 | 1057 | `			return -1;` |
|    - | 1058 | `		}` |
|    2 | 1059 | `		nAddr = (ph7_socklen)sizeof(addr);` |
|    2 | 1060 | `		memset(&addr,0,sizeof(addr));` |
|    2 | 1061 | `		if( getsockname(listener,(struct sockaddr *)&addr,&nAddr) != 0 ){` |
|  ! 0 | 1062 | `			if( pErrno ){ *pErrno = PH7_NetLastError(); }` |
|  ! 0 | 1063 | `			PH7_NetClose(listener);` |
|  ! 0 | 1064 | `			return -1;` |
|    - | 1065 | `		}` |
|    2 | 1066 | `		client = socket(AF_INET,SOCK_STREAM,0);` |
|    - | 1067 | `		if( client == PH7_NET_INVALID_SOCKET` |
|    2 | 1068 | `		 \|\| connect(client,(struct sockaddr *)&addr,(ph7_socklen)sizeof(addr)) != 0 ){` |
|  ! 0 | 1069 | `			if( pErrno ){ *pErrno = PH7_NetLastError(); }` |
|  ! 0 | 1070 | `			PH7_NetClose(client);` |
|  ! 0 | 1071 | `			PH7_NetClose(listener);` |
|  ! 0 | 1072 | `			return -1;` |
|    - | 1073 | `		}` |
|    2 | 1074 | `		nAddr = (ph7_socklen)sizeof(peer);` |
|    2 | 1075 | `		memset(&peer,0,sizeof(peer));` |
|    2 | 1076 | `		server = accept(listener,(struct sockaddr *)&peer,&nAddr);` |
|    2 | 1077 | `		PH7_NetClose(listener);` |
|    2 | 1078 | `		if( server == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 | 1079 | `			if( pErrno ){ *pErrno = PH7_NetLastError(); }` |
|  ! 0 | 1080 | `			PH7_NetClose(client);` |
|  ! 0 | 1081 | `			return -1;` |
|    - | 1082 | `		}` |
|    - | 1083 | `		/* The connection that arrived must be the one that was made: another` |
|    - | 1084 | `		 * process could have reached the same listener first. */` |
|    2 | 1085 | `		nAddr = (ph7_socklen)sizeof(self);` |
|    2 | 1086 | `		memset(&self,0,sizeof(self));` |
|    - | 1087 | `		if( getsockname(client,(struct sockaddr *)&self,&nAddr) != 0` |
|    - | 1088 | `		 \|\| self.sin_port != peer.sin_port` |
|    2 | 1089 | `		 \|\| self.sin_addr.s_addr != peer.sin_addr.s_addr ){` |
|  ! 0 | 1090 | `			if( pErrno ){ *pErrno = WSAECONNABORTED; }` |
|  ! 0 | 1091 | `			PH7_NetClose(client);` |
|  ! 0 | 1092 | `			PH7_NetClose(server);` |
|  ! 0 | 1093 | `			return -1;` |
|    - | 1094 | `		}` |
|    2 | 1095 | `		aOut[0] = client;` |
|    2 | 1096 | `		aOut[1] = server;` |
|    2 | 1097 | `		return PH7_OK;` |
|    - | 1098 | `	}` |
|    - | 1099 | `#endif` |
|    5 | 1100 | `}` |
|    - | 1101 |  |
|    - | 1102 | `#endif /* PH7_ENABLE_NET */` |
|    - | 1103 |  |
