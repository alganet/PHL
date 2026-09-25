# src/ph7/net.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 381/474 lines (80.38%)

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
|  138 |   39 | `static void NetApplySockOpts(ph7_socket sock,const ph7_sockopts *pOpt)` |
|    3 |   40 | `{` |
|  141 |   41 | `	int on = 1;` |
|  141 |   42 | `	if( pOpt == 0 ){` |
|   33 |   43 | `		return;` |
|    - |   44 | `	}` |
|  109 |   45 | `	if( pOpt->bReusePort ){` |
|    - |   46 | `#ifdef SO_REUSEPORT` |
|    4 |   47 | `		setsockopt(sock,SOL_SOCKET,SO_REUSEPORT,(const char *)&on,sizeof(on));` |
|    - |   48 | `#endif` |
|    2 |   49 | `	}` |
|  109 |   50 | `	if( pOpt->bNoDelay ){` |
|    - |   51 | `#ifdef TCP_NODELAY` |
|    8 |   52 | `		setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(const char *)&on,sizeof(on));` |
|    - |   53 | `#endif` |
|    3 |   54 | `	}` |
|   72 |   55 | `}` |
|    - |   56 | `/*` |
|    - |   57 | `` * `bindto`: the LOCAL address a client socket takes before it connects, which`` |
|    - |   58 | ` * is how a program picks the interface (or the source port) its connection goes` |
|    - |   59 | ` * out on. It is a NUMERIC literal and never a name (see the body), and when it` |
|    - |   60 | ` * cannot be used php warns and connects from wherever the routing table would` |
|    - |   61 | ` * have sent it — so a failure here is reported and never fatal.` |
|    - |   62 | ` */` |
|   10 |   63 | `static int NetBindLocal(ph7_socket sock,int iFamily,const char *zHost,int iPort,int *pErrno)` |
|    1 |   64 | `{` |
|    - |   65 | `	struct sockaddr_in sin;` |
|    - |   66 | `	struct sockaddr_in6 sin6;` |
|    - |   67 | `	struct sockaddr *pAddr;` |
|    - |   68 | `	ph7_socklen nAddr;` |
|    - |   69 | `	int rc;` |
|   11 |   70 | `	*pErrno = 0;` |
|    - |   71 | `	/* php reads a local address as a NUMERIC literal and nothing else — it is` |
|    - |   72 | ``	 * inet_pton(), not the resolver — so `bindto => 'localhost:0'` is an`` |
|    - |   73 | `	 * Invalid IP Address there and binds nothing. It is parsed in the family of` |
|    - |   74 | `	 * the socket that will carry it, so the answer describes THIS candidate and` |
|    - |   75 | `	 * not the address family net.c prefers. (php's bracketed IPv6 spelling is` |
|    - |   76 | `	 * the recorded gap §7.4 slice-2 (a) names; it does not parse here either.) */` |
|   11 |   77 | `	if( iFamily == AF_INET6 ){` |
|  ! 0 |   78 | `		memset(&sin6,0,sizeof(sin6));` |
|  ! 0 |   79 | `		sin6.sin6_family = AF_INET6;` |
|  ! 0 |   80 | `		sin6.sin6_port = htons((unsigned short)iPort);` |
|  ! 0 |   81 | `		if( inet_pton(AF_INET6,zHost,&sin6.sin6_addr) != 1 ){` |
|  ! 0 |   82 | `			return PH7_SOCKOPT_BIND_RESOLVE;` |
|    - |   83 | `		}` |
|  ! 0 |   84 | `		pAddr = (struct sockaddr *)&sin6;` |
|  ! 0 |   85 | `		nAddr = (ph7_socklen)sizeof(sin6);` |
|  ! 0 |   86 | `	}else{` |
|   11 |   87 | `		memset(&sin,0,sizeof(sin));` |
|   11 |   88 | `		sin.sin_family = AF_INET;` |
|   11 |   89 | `		sin.sin_port = htons((unsigned short)iPort);` |
|   11 |   90 | `		if( inet_pton(AF_INET,zHost,&sin.sin_addr) != 1 ){` |
|    5 |   91 | `			return PH7_SOCKOPT_BIND_RESOLVE;` |
|    - |   92 | `		}` |
|    7 |   93 | `		pAddr = (struct sockaddr *)&sin;` |
|    7 |   94 | `		nAddr = (ph7_socklen)sizeof(sin);` |
|    - |   95 | `	}` |
|    7 |   96 | `	rc = bind(sock,pAddr,nAddr);` |
|    7 |   97 | `	if( rc != 0 ){` |
|    5 |   98 | `		*pErrno = PH7_NetLastError();` |
|    5 |   99 | `		return PH7_SOCKOPT_BIND_REFUSED;` |
|    - |  100 | `	}` |
|    3 |  101 | `	return 0;` |
|    6 |  102 | `}` |
|    - |  103 | `/*` |
|    - |  104 | ` * The OS error the last socket call reported. A Windows socket does not touch` |
|    - |  105 | ` * errno at all, so a caller reading errno there reads whatever the last` |
|    - |  106 | ` * unrelated CRT call left behind.` |
|    - |  107 | ` */` |
|   12 |  108 | `PH7_PRIVATE int PH7_NetLastError(void)` |
|    2 |  109 | `{` |
|    - |  110 | `#ifdef __WINNT__` |
|    2 |  111 | `	return WSAGetLastError();` |
|    - |  112 | `#else` |
|   12 |  113 | `	return errno;` |
|    - |  114 | `#endif` |
|    2 |  115 | `}` |
|    - |  116 | `/*` |
|    - |  117 | ` * The message php words a socket failure with. php's own POSIX build answers` |
|    - |  118 | ` * strerror(), which is what a script comparing an $errstr against a documented` |
|    - |  119 | ` * text expects; a Winsock code is not an errno at all, so the codes a stream` |
|    - |  120 | ` * builtin can actually surface are worded here rather than handed to` |
|    - |  121 | ` * strerror() (which would answer for a completely different errno) — the` |
|    - |  122 | ` * FormatMessage() prose php's Windows build answers is its own (§7.4).` |
|    - |  123 | ` */` |
|   12 |  124 | `PH7_PRIVATE const char * PH7_NetStrError(int iErr)` |
|    2 |  125 | `{` |
|    - |  126 | `#ifdef __WINNT__` |
|    - |  127 | `	static const struct { int iCode; const char *zMsg; } aWsa[] = {` |
|    - |  128 | `		{ WSAEACCES,          "Permission denied" },` |
|    - |  129 | `		{ WSAEADDRINUSE,      "Address already in use" },` |
|    - |  130 | `		{ WSAEADDRNOTAVAIL,   "Cannot assign requested address" },` |
|    - |  131 | `		{ WSAEAFNOSUPPORT,    "Address family not supported by protocol" },` |
|    - |  132 | `		{ WSAECONNABORTED,    "Software caused connection abort" },` |
|    - |  133 | `		{ WSAECONNREFUSED,    "Connection refused" },` |
|    - |  134 | `		{ WSAECONNRESET,      "Connection reset by peer" },` |
|    - |  135 | `		{ WSAEHOSTUNREACH,    "No route to host" },` |
|    - |  136 | `		{ WSAEINVAL,          "Invalid argument" },` |
|    - |  137 | `		{ WSAEMFILE,          "Too many open files" },` |
|    - |  138 | `		{ WSAENETDOWN,        "Network is down" },` |
|    - |  139 | `		{ WSAENETUNREACH,     "Network is unreachable" },` |
|    - |  140 | `		{ WSAENOTCONN,        "Transport endpoint is not connected" },` |
|    - |  141 | `		{ WSAENOTSOCK,        "Socket operation on non-socket" },` |
|    - |  142 | `		{ WSAEOPNOTSUPP,      "Operation not supported" },` |
|    - |  143 | `		{ WSAEPROTONOSUPPORT, "Protocol not supported" },` |
|    - |  144 | `		{ WSAETIMEDOUT,       "Connection timed out" },` |
|    - |  145 | `		{ WSAEWOULDBLOCK,     "Resource temporarily unavailable" },` |
|    - |  146 | `		{ WSAEDESTADDRREQ,    "Destination address required" },` |
|    - |  147 | `		{ WSAEISCONN,         "Transport endpoint is already connected" },` |
|    - |  148 | `		{ WSAEMSGSIZE,        "Message too long" },` |
|    - |  149 | `		{ WSAENOBUFS,         "No buffer space available" },` |
|    - |  150 | `		{ WSAENOPROTOOPT,     "Protocol not available" },` |
|    - |  151 | `		/* A send on a socket whose write side is shut is POSIX's EPIPE, and it` |
|    - |  152 | `		 * is the same condition — the wording follows the errno rather than the` |
|    - |  153 | `		 * Winsock name so a script reading it reads one answer. */` |
|    - |  154 | `		{ WSAESHUTDOWN,       "Broken pipe" }` |
|    - |  155 | `	};` |
|    - |  156 | `	int i;` |
|    2 |  157 | `	for( i = 0 ; i < (int)(sizeof(aWsa)/sizeof(aWsa[0])) ; i++ ){` |
|    2 |  158 | `		if( aWsa[i].iCode == iErr ){` |
|    2 |  159 | `			return aWsa[i].zMsg;` |
|    - |  160 | `		}` |
|    2 |  161 | `	}` |
|  ! 0 |  162 | `	return "Unknown error";` |
|    - |  163 | `#else` |
|   12 |  164 | `	return strerror(iErr);` |
|    - |  165 | `#endif` |
|    2 |  166 | `}` |
|    - |  167 | `/*` |
|    - |  168 | ` * Did the last socket call fail only because it would have WAITED? That is the` |
|    - |  169 | ` * one failure php does not report as an error: a read answers "" or false by` |
|    - |  170 | ` * the handle's own rules, and a write answers what it managed to send.` |
|    - |  171 | ` */` |
|   11 |  172 | `PH7_PRIVATE int PH7_NetWouldBlock(void)` |
|    2 |  173 | `{` |
|    - |  174 | `#ifdef __WINNT__` |
|    2 |  175 | `	int iErr = WSAGetLastError();` |
|    2 |  176 | `	return iErr == WSAEWOULDBLOCK \|\| iErr == WSAETIMEDOUT;` |
|    - |  177 | `#else` |
|   11 |  178 | `	return errno == EAGAIN \|\| errno == EWOULDBLOCK \|\| errno == EINTR;` |
|    - |  179 | `#endif` |
|    2 |  180 | `}` |
|    - |  181 | `/*` |
|    - |  182 | ` * Wait until a socket is readable (bWrite == 0) or writable, for at most` |
|    - |  183 | ` * iTimeoutMs milliseconds (a negative timeout blocks). Answers 1 (ready),` |
|    - |  184 | ` * 0 (the wait expired) or -1 (the wait itself failed).` |
|    - |  185 | ` *` |
|    - |  186 | ` * POSIX takes poll() rather than select(): a descriptor at or above` |
|    - |  187 | ` * FD_SETSIZE cannot be put in an fd_set at all, and a long-running server is` |
|    - |  188 | ` * exactly the program that reaches those numbers.` |
|    - |  189 | ` */` |
|   28 |  190 | `PH7_PRIVATE int PH7_NetWait(ph7_socket sock,int bWrite,int iTimeoutMs)` |
|    3 |  191 | `{` |
|    - |  192 | `#ifdef __WINNT__` |
|    - |  193 | `	fd_set sSet;` |
|    3 |  194 | `	struct timeval tv,*pTv = 0;` |
|    - |  195 | `	int rc;` |
|    3 |  196 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    - |  197 | `		/* Nothing to wait on: php's own select() over a stream whose socket was` |
|    - |  198 | `		 * never created reports it not-ready, which is an expiry and not an` |
|    - |  199 | `		 * error. poll() would answer POLLNVAL and look like readiness. */` |
|    1 |  200 | `		return 0;` |
|    - |  201 | `	}` |
|    3 |  202 | `	FD_ZERO(&sSet);` |
|    3 |  203 | `	FD_SET(sock,&sSet);` |
|    3 |  204 | `	if( iTimeoutMs >= 0 ){` |
|    3 |  205 | `		tv.tv_sec = iTimeoutMs / 1000;` |
|    3 |  206 | `		tv.tv_usec = (iTimeoutMs % 1000) * 1000;` |
|    3 |  207 | `		pTv = &tv;` |
|    - |  208 | `	}` |
|    3 |  209 | `	rc = select(0,bWrite ? 0 : &sSet,bWrite ? &sSet : 0,0,pTv);` |
|    3 |  210 | `	return rc < 0 ? -1 : (rc > 0 ? 1 : 0);` |
|    - |  211 | `#else` |
|    - |  212 | `	struct pollfd sPoll;` |
|    - |  213 | `	int rc;` |
|   28 |  214 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    - |  215 | `		/* See above: nothing to wait on is an expiry, and poll() would answer` |
|    - |  216 | `		 * POLLNVAL for it, which looks like readiness. */` |
|    2 |  217 | `		return 0;` |
|    - |  218 | `	}` |
|   26 |  219 | `	sPoll.fd = sock;` |
|   26 |  220 | `	sPoll.events = (short)(bWrite ? POLLOUT : POLLIN);` |
|   26 |  221 | `	sPoll.revents = 0;` |
|   13 |  222 | `	for(;;){` |
|   13 |  223 | `		rc = poll(&sPoll,1,iTimeoutMs);` |
|   26 |  224 | `		if( rc < 0 && errno == EINTR ){` |
|    - |  225 | `			/* A signal is not an answer to the question that was asked. */` |
|  ! 0 |  226 | `			continue;` |
|    - |  227 | `		}` |
|   26 |  228 | `		break;` |
|    - |  229 | `	}` |
|   26 |  230 | `	return rc < 0 ? -1 : (rc > 0 ? 1 : 0);` |
|    - |  231 | `#endif` |
|   17 |  232 | `}` |
|    - |  233 |  |
|    - |  234 | `/*` |
|    - |  235 | ` * Initialize the networking subsystem.` |
|    - |  236 | ` * On Windows, calls WSAStartup(). On Unix, ignores SIGPIPE.` |
|    - |  237 | ` * Returns PH7_OK on success.` |
|    - |  238 | ` */` |
|  114 |  239 | `PH7_PRIVATE int PH7_NetInit(void)` |
|    4 |  240 | `{` |
|    - |  241 | `#ifdef __WINNT__` |
|    - |  242 | `	WSADATA wsaData;` |
|    4 |  243 | `	if( WSAStartup(MAKEWORD(2,2), &wsaData) != 0 ){` |
|  ! 0 |  244 | `		return PH7_IO_ERR;` |
|    - |  245 | `	}` |
|    - |  246 | `#else` |
|  114 |  247 | `	signal(SIGPIPE, SIG_IGN);` |
|    - |  248 | `#endif` |
|  118 |  249 | `	return PH7_OK;` |
|    4 |  250 | `}` |
|    - |  251 | `/*` |
|    - |  252 | ` * Start the networking subsystem the first time a socket is opened.` |
|    - |  253 | ` *` |
|    - |  254 | `` * PH7_NetInit() used to be called by the `-S` server and by nothing else, so`` |
|    - |  255 | ` * every socket a SCRIPT opened ran with the subsystem unstarted: on Windows` |
|    - |  256 | `` * that is `WSANOTINITIALISED` from the very first call — fsockopen() could`` |
|    - |  257 | ` * never connect at all — and on POSIX it left SIGPIPE at its default, so` |
|    - |  258 | ` * writing to a socket whose peer had closed KILLED the process (exit 141)` |
|    - |  259 | ` * where php answers false. Both are invisible from a POSIX-only reading of a` |
|    - |  260 | ` * program that never loses a peer.` |
|    - |  261 | ` */` |
|  144 |  262 | `PH7_PRIVATE int PH7_NetEnsureInit(void)` |
|    4 |  263 | `{` |
|    - |  264 | `	static int bReady = 0;` |
|  148 |  265 | `	if( bReady ){` |
|   66 |  266 | `		return PH7_OK;` |
|    - |  267 | `	}` |
|   86 |  268 | `	if( PH7_NetInit() != PH7_OK ){` |
|  ! 0 |  269 | `		return PH7_IO_ERR;` |
|    - |  270 | `	}` |
|   86 |  271 | `	bReady = 1;` |
|   86 |  272 | `	return PH7_OK;` |
|   76 |  273 | `}` |
|    - |  274 | `/*` |
|    - |  275 | ` * Cleanup the networking subsystem.` |
|    - |  276 | ` */` |
|   38 |  277 | `PH7_PRIVATE void PH7_NetCleanup(void)` |
|  ! 0 |  278 | `{` |
|    - |  279 | `#ifdef __WINNT__` |
|  ! 0 |  280 | `	WSACleanup();` |
|    - |  281 | `#endif` |
|   38 |  282 | `}` |
|    - |  283 | `/*` |
|    - |  284 | ` * Bind a socket to the given host and port, and — for a stream socket that is` |
|    - |  285 | `` * going to serve — listen on it. `stream_socket_server()` is the caller that`` |
|    - |  286 | ` * needs the two apart: php's STREAM_SERVER_LISTEN is a separate flag, and a` |
|    - |  287 | ` * datagram socket is bound WITHOUT ever listening. (A server asked for neither` |
|    - |  288 | ` * BIND nor LISTEN never reaches here at all — php creates no socket for it.)` |
|    - |  289 | ` *` |
|    - |  290 | ` * *pErrno receives the OS error and *pzErr its message, which is what the` |
|    - |  291 | ` * builtin reports through its by-ref out-params and its warning.` |
|    - |  292 | ` * Returns the socket, or PH7_NET_INVALID_SOCKET.` |
|    - |  293 | ` */` |
|   60 |  294 | `PH7_PRIVATE ph7_socket PH7_NetBind(const char *zHost, int iPort, int bDgram, int bListen,` |
|    - |  295 | `	int iBacklog, const ph7_sockopts *pOpt, int *pErrno, const char **pzErr)` |
|    4 |  296 | `{` |
|    - |  297 | `	struct sockaddr_in addr;` |
|    - |  298 | `	ph7_socket sock;` |
|   64 |  299 | `	int on = 1;` |
|   64 |  300 | `	int iType = bDgram ? SOCK_DGRAM : SOCK_STREAM;` |
|   64 |  301 | `	if( pErrno ){ *pErrno = 0; }` |
|   64 |  302 | `	if( pzErr ){ *pzErr = ""; }` |
|   64 |  303 | `	if( PH7_NetEnsureInit() != PH7_OK ){` |
|  ! 0 |  304 | `		if( pzErr ){ *pzErr = "Unable to start the networking subsystem"; }` |
|  ! 0 |  305 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  306 | `	}` |
|   64 |  307 | `	memset(&addr, 0, sizeof(addr));` |
|   64 |  308 | `	addr.sin_family = AF_INET;` |
|   64 |  309 | `	addr.sin_port = htons((unsigned short)iPort);` |
|   64 |  310 | `	if( zHost == 0 \|\| zHost[0] == 0 \|\| strcmp(zHost, "0.0.0.0") == 0 ){` |
|  ! 0 |  311 | `		addr.sin_addr.s_addr = htonl(INADDR_ANY);` |
|   64 |  312 | `	}else if( strcmp(zHost, "localhost") == 0 \|\| strcmp(zHost, "127.0.0.1") == 0 ){` |
|   61 |  313 | `		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);` |
|   32 |  314 | `	}else{` |
|    - |  315 | `		struct addrinfo hints, *res;` |
|    3 |  316 | `		memset(&hints, 0, sizeof(hints));` |
|    3 |  317 | `		hints.ai_family = AF_INET;` |
|    3 |  318 | `		hints.ai_socktype = iType;` |
|    3 |  319 | `		if( getaddrinfo(zHost, 0, &hints, &res) != 0 \|\| res == 0 ){` |
|    - |  320 | `			/* Nothing is open yet: the socket is created below. */` |
|    3 |  321 | `			if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }` |
|    3 |  322 | `			if( pzErr ){ *pzErr = 0; }` |
|    3 |  323 | `			return PH7_NET_INVALID_SOCKET;` |
|    - |  324 | `		}` |
|  ! 0 |  325 | `		addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;` |
|  ! 0 |  326 | `		freeaddrinfo(res);` |
|    - |  327 | `	}` |
|   61 |  328 | `	sock = socket(AF_INET, iType, 0);` |
|   61 |  329 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  330 | `		if( pErrno ){ *pErrno = PH7_NetLastError(); }` |
|  ! 0 |  331 | `		if( pzErr ){ *pzErr = PH7_NetStrError(PH7_NetLastError()); }` |
|  ! 0 |  332 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  333 | `	}` |
|   61 |  334 | `	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));` |
|   61 |  335 | `	NetApplySockOpts(sock, pOpt);` |
|   61 |  336 | `	if( pOpt && pOpt->iBacklog > 0 ){` |
|    - |  337 | ``		/* php's `backlog` context option: how many completed connections the OS`` |
|    - |  338 | `		 * may queue before the accept loop gets to them. */` |
|    5 |  339 | `		iBacklog = pOpt->iBacklog;` |
|    2 |  340 | `	}` |
|   58 |  341 | `	if( bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0` |
|   60 |  342 | `	 \|\| (bListen && !bDgram && listen(sock, iBacklog) != 0) ){` |
|    - |  343 | `		/* Read the error BEFORE closing the socket: close() is a call of its` |
|    - |  344 | `		 * own and overwrites what the failure left behind. */` |
|    2 |  345 | `		int iErr = PH7_NetLastError();` |
|    2 |  346 | `		if( pErrno ){ *pErrno = iErr; }` |
|    2 |  347 | `		if( pzErr ){ *pzErr = PH7_NetStrError(iErr); }` |
|    2 |  348 | `		PH7_NetClose(sock);` |
|    2 |  349 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  350 | `	}` |
|   59 |  351 | `	return sock;` |
|   34 |  352 | `}` |
|    - |  353 | `/*` |
|    - |  354 | ` * Create a TCP listening socket bound to the given host and port.` |
|    - |  355 | ` * Returns the socket descriptor, or PH7_NET_INVALID_SOCKET on error.` |
|    - |  356 | ` */` |
|   32 |  357 | `PH7_PRIVATE ph7_socket PH7_NetListen(const char *zHost, int iPort, int iBacklog)` |
|  ! 0 |  358 | `{` |
|   32 |  359 | `	return PH7_NetBind(zHost, iPort, 0, 1, iBacklog, 0, 0, 0);` |
|  ! 0 |  360 | `}` |
|    - |  361 | `/*` |
|    - |  362 | ` * The local (bPeer == 0) or the peer address of a socket, formatted the way` |
|    - |  363 | ` * php's stream_socket_get_name() answers it: "ip:port". Returns PH7_OK, or -1` |
|    - |  364 | ` * for a socket that has no such name (an unbound one, or the peer of a socket` |
|    - |  365 | `` * that is not connected) — which is php's `false`.`` |
|    - |  366 | ` */` |
|   42 |  367 | `PH7_PRIVATE int PH7_NetSockName(ph7_socket sock, int bPeer, char *zBuf, int nBuf)` |
|    3 |  368 | `{` |
|    - |  369 | `	struct sockaddr_in addr;` |
|   45 |  370 | `	ph7_socklen nLen = (ph7_socklen)sizeof(addr);` |
|    - |  371 | `	char zAddr[64];` |
|    - |  372 | `	int n;` |
|   45 |  373 | `	if( zBuf == 0 \|\| nBuf < 2 ){` |
|  ! 0 |  374 | `		return -1;` |
|    - |  375 | `	}` |
|   45 |  376 | `	zBuf[0] = 0;` |
|   45 |  377 | `	memset(&addr, 0, sizeof(addr));` |
|   45 |  378 | `	if( (bPeer ? getpeername(sock, (struct sockaddr *)&addr, &nLen)` |
|   48 |  379 | `	           : getsockname(sock, (struct sockaddr *)&addr, &nLen)) != 0 ){` |
|   10 |  380 | `		return -1;` |
|    - |  381 | `	}` |
|   37 |  382 | `	if( addr.sin_family != AF_INET ){` |
|    - |  383 | `		/* A socketpair has no address of any kind, and php answers false for` |
|    - |  384 | `		 * one rather than inventing a name for it. */` |
|  ! 0 |  385 | `		return -1;` |
|    - |  386 | `	}` |
|   37 |  387 | `	PH7_NetAddrToString((struct sockaddr *)&addr, zAddr, (int)sizeof(zAddr));` |
|   37 |  388 | `	if( zAddr[0] == 0 ){` |
|  ! 0 |  389 | `		return -1;` |
|    - |  390 | `	}` |
|   37 |  391 | `	n = snprintf(zBuf, (size_t)nBuf, "%s:%d", zAddr, PH7_NetAddrPort((struct sockaddr *)&addr));` |
|   37 |  392 | `	return (n > 0 && n < nBuf) ? PH7_OK : -1;` |
|   24 |  393 | `}` |
|    - |  394 | `/*` |
|    - |  395 | ` * accept() bounded by a timeout in milliseconds (a negative one blocks). The` |
|    - |  396 | ` * peer's address is written to zPeer when it fits, which is php's by-ref` |
|    - |  397 | ` * $peer_name out-param. *pbTimedOut tells a wait that EXPIRED — php's own` |
|    - |  398 | ` * "Accept failed: Connection timed out" — from a call that failed.` |
|    - |  399 | ` */` |
|   28 |  400 | `PH7_PRIVATE ph7_socket PH7_NetAcceptTimed(ph7_socket listenSock, int iTimeoutMs, int *pbTimedOut,` |
|    - |  401 | `	char *zPeer, int nPeer)` |
|    3 |  402 | `{` |
|    - |  403 | `	struct sockaddr_in addr;` |
|   31 |  404 | `	ph7_socklen nLen = (ph7_socklen)sizeof(addr);` |
|    - |  405 | `	ph7_socket sock;` |
|   31 |  406 | `	if( pbTimedOut ){ *pbTimedOut = 0; }` |
|   31 |  407 | `	if( zPeer && nPeer > 0 ){ zPeer[0] = 0; }` |
|   31 |  408 | `	if( iTimeoutMs >= 0 ){` |
|   31 |  409 | `		int rc = PH7_NetWait(listenSock, 0, iTimeoutMs);` |
|   31 |  410 | `		if( rc == 0 ){` |
|    6 |  411 | `			if( pbTimedOut ){ *pbTimedOut = 1; }` |
|    6 |  412 | `			return PH7_NET_INVALID_SOCKET;` |
|    - |  413 | `		}` |
|   27 |  414 | `		if( rc < 0 ){` |
|  ! 0 |  415 | `			return PH7_NET_INVALID_SOCKET;` |
|    - |  416 | `		}` |
|   12 |  417 | `	}` |
|   27 |  418 | `	memset(&addr, 0, sizeof(addr));` |
|   27 |  419 | `	sock = accept(listenSock, (struct sockaddr *)&addr, &nLen);` |
|   27 |  420 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  421 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  422 | `	}` |
|   27 |  423 | `	if( zPeer && nPeer > 0 && addr.sin_family == AF_INET ){` |
|    - |  424 | `		char zAddr[64];` |
|   27 |  425 | `		PH7_NetAddrToString((struct sockaddr *)&addr, zAddr, (int)sizeof(zAddr));` |
|   27 |  426 | `		if( zAddr[0] ){` |
|   27 |  427 | `			snprintf(zPeer, (size_t)nPeer, "%s:%d", zAddr, PH7_NetAddrPort((struct sockaddr *)&addr));` |
|   12 |  428 | `		}` |
|   12 |  429 | `	}` |
|   27 |  430 | `	return sock;` |
|   17 |  431 | `}` |
|    - |  432 | `/*` |
|    - |  433 | ` * Connect a TCP socket to the given host and port (blocking; the caller sets` |
|    - |  434 | ` * a timeout with PH7_NetSetTimeout afterwards). *pErrno receives the OS error` |
|    - |  435 | ` * code and *pzErr a static description on failure, matching what fsockopen()` |
|    - |  436 | ` * reports through its by-ref out-params.` |
|    - |  437 | ` * Returns the connected socket, or PH7_NET_INVALID_SOCKET on error.` |
|    - |  438 | ` */` |
|   80 |  439 | `PH7_PRIVATE ph7_socket PH7_NetConnect(const char *zHost, int iPort, int iTimeoutMs,` |
|    - |  440 | `	ph7_sockopts *pOpt, int *pErrno, const char **pzErr)` |
|    4 |  441 | `{` |
|   84 |  442 | `	struct addrinfo hints, *res = 0, *rp;` |
|    - |  443 | `	char zPort[16];` |
|   84 |  444 | `	ph7_socket sock = PH7_NET_INVALID_SOCKET;` |
|   84 |  445 | `	if( pErrno ){ *pErrno = 0; }` |
|   84 |  446 | `	if( pzErr ){ *pzErr = ""; }` |
|   84 |  447 | `	if( PH7_NetEnsureInit() != PH7_OK ){` |
|  ! 0 |  448 | `		if( pErrno ){ *pErrno = -1; }` |
|  ! 0 |  449 | `		if( pzErr ){ *pzErr = "Unable to start the networking subsystem"; }` |
|  ! 0 |  450 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  451 | `	}` |
|   84 |  452 | `	if( zHost == 0 \|\| zHost[0] == 0 ){` |
|  ! 0 |  453 | `		if( pErrno ){ *pErrno = -1; }` |
|  ! 0 |  454 | `		if( pzErr ){ *pzErr = "Empty host"; }` |
|  ! 0 |  455 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  456 | `	}` |
|   84 |  457 | `	snprintf(zPort, sizeof(zPort), "%d", iPort);` |
|   84 |  458 | `	memset(&hints, 0, sizeof(hints));` |
|   84 |  459 | `	hints.ai_family = AF_UNSPEC;` |
|   84 |  460 | `	hints.ai_socktype = SOCK_STREAM;` |
|   84 |  461 | `	if( getaddrinfo(zHost, zPort, &hints, &res) != 0 \|\| res == 0 ){` |
|    - |  462 | `		/* php words the HOST into this one and reports no OS code for it; the` |
|    - |  463 | `		 * caller composes it, since only it has the name to interpolate. */` |
|    3 |  464 | `		if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }` |
|    3 |  465 | `		if( pzErr ){ *pzErr = 0; }` |
|    3 |  466 | `		return PH7_NET_INVALID_SOCKET;` |
|    - |  467 | `	}` |
|  117 |  468 | `	for( rp = res ; rp != 0 ; rp = rp->ai_next ){` |
|   83 |  469 | `		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);` |
|   83 |  470 | `		if( sock == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  471 | `			continue;` |
|    - |  472 | `		}` |
|   83 |  473 | `		NetApplySockOpts(sock, pOpt);` |
|   83 |  474 | `		if( pOpt ){` |
|    - |  475 | `			/* Per CANDIDATE: the failure that gets reported belongs to the socket` |
|    - |  476 | `			 * that ends up carrying the connection, not to one abandoned earlier.` |
|    - |  477 | `			 * php connects anyway when the local bind cannot be made; the caller` |
|    - |  478 | `			 * words the warning, since only it knows the function name. */` |
|   83 |  479 | `			pOpt->iBindErr = 0;` |
|   83 |  480 | `			pOpt->iBindErrno = 0;` |
|   83 |  481 | `			if( pOpt->zBindHost ){` |
|   16 |  482 | `				pOpt->iBindErr = NetBindLocal(sock, rp->ai_family, pOpt->zBindHost,` |
|    5 |  483 | `					pOpt->iBindPort, &pOpt->iBindErrno);` |
|    5 |  484 | `			}` |
|   40 |  485 | `		}` |
|   83 |  486 | `		if( iTimeoutMs > 0 ){` |
|    - |  487 | `			/* the connect() itself stays blocking; the timeout bounds the` |
|    - |  488 | `			 * subsequent recv/send (php applies it to both — recorded) */` |
|   73 |  489 | `			PH7_NetSetTimeout(sock, iTimeoutMs);` |
|   35 |  490 | `		}` |
|   83 |  491 | `		if( connect(sock, rp->ai_addr, (ph7_socklen)rp->ai_addrlen) == 0 ){` |
|   47 |  492 | `			freeaddrinfo(res);` |
|   47 |  493 | `			return sock;` |
|    - |  494 | `		}` |
|   37 |  495 | `		PH7_NetClose(sock);` |
|   37 |  496 | `		sock = PH7_NET_INVALID_SOCKET;` |
|   19 |  497 | `	}` |
|   34 |  498 | `	freeaddrinfo(res);` |
|   34 |  499 | `	if( pErrno ){ *pErrno = 111; }` |
|   34 |  500 | `	if( pzErr ){ *pzErr = "Connection refused"; }` |
|   34 |  501 | `	return PH7_NET_INVALID_SOCKET;` |
|   44 |  502 | `}` |
|    - |  503 | `/*` |
|    - |  504 | ` * Accept an incoming connection on a listening socket.` |
|    - |  505 | ` * If pAddr and pAddrLen are non-NULL, the client address is stored there.` |
|    - |  506 | ` * Returns the client socket, or PH7_NET_INVALID_SOCKET on error.` |
|    - |  507 | ` */` |
|   90 |  508 | `PH7_PRIVATE ph7_socket PH7_NetAccept(ph7_socket listenSock, struct sockaddr *pAddr, ph7_socklen *pAddrLen)` |
|  ! 0 |  509 | `{` |
|   90 |  510 | `	return accept(listenSock, pAddr, pAddrLen);` |
|  ! 0 |  511 | `}` |
|    - |  512 | `/*` |
|    - |  513 | ` * Receive data from a socket.` |
|    - |  514 | ` * Returns the number of bytes received, or -1 on error.` |
|    - |  515 | ` */` |
|   94 |  516 | `PH7_PRIVATE int PH7_NetRecv(ph7_socket sock, void *pBuf, int nLen, int flags)` |
|    3 |  517 | `{` |
|   97 |  518 | `	return (int)recv(sock, (char *)pBuf, nLen, flags);` |
|    3 |  519 | `}` |
|    - |  520 | `/*` |
|    - |  521 | ` * Send data on a socket.` |
|    - |  522 | ` * Returns the number of bytes sent, or -1 on error.` |
|    - |  523 | ` */` |
|   55 |  524 | `PH7_PRIVATE int PH7_NetSend(ph7_socket sock, const void *pBuf, int nLen, int flags)` |
|    3 |  525 | `{` |
|   58 |  526 | `	return (int)send(sock, (const char *)pBuf, nLen, flags);` |
|    3 |  527 | `}` |
|    - |  528 | `/*` |
|    - |  529 | ` * Send all data on a socket, retrying on partial writes.` |
|    - |  530 | ` * Returns PH7_OK on success, PH7_IO_ERR on error.` |
|    - |  531 | ` */` |
|  248 |  532 | `PH7_PRIVATE int PH7_NetSendAll(ph7_socket sock, const void *pBuf, int nLen)` |
|  ! 0 |  533 | `{` |
|  248 |  534 | `	const char *zBuf = (const char *)pBuf;` |
|    - |  535 | `	int nSent;` |
|  496 |  536 | `	while( nLen > 0 ){` |
|  248 |  537 | `		nSent = (int)send(sock, zBuf, nLen, 0);` |
|  248 |  538 | `		if( nSent <= 0 ){` |
|  ! 0 |  539 | `			return PH7_IO_ERR;` |
|    - |  540 | `		}` |
|  248 |  541 | `		zBuf += nSent;` |
|  248 |  542 | `		nLen -= nSent;` |
|  ! 0 |  543 | `	}` |
|  248 |  544 | `	return PH7_OK;` |
|  124 |  545 | `}` |
|    - |  546 | `/*` |
|    - |  547 | ` * Close a socket.` |
|    - |  548 | ` */` |
|  206 |  549 | `PH7_PRIVATE void PH7_NetClose(ph7_socket sock)` |
|    3 |  550 | `{` |
|  209 |  551 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    8 |  552 | `		return;` |
|    - |  553 | `	}` |
|    - |  554 | `#ifdef __WINNT__` |
|    3 |  555 | `	closesocket(sock);` |
|    - |  556 | `#else` |
|  200 |  557 | `	close(sock);` |
|    - |  558 | `#endif` |
|  106 |  559 | `}` |
|    - |  560 | `/*` |
|    - |  561 | ` * Set a receive timeout on a socket (in milliseconds).` |
|    - |  562 | ` */` |
|  122 |  563 | `PH7_PRIVATE void PH7_NetSetTimeout(ph7_socket sock, int iMilliseconds)` |
|    3 |  564 | `{` |
|    - |  565 | `#ifdef __WINNT__` |
|    3 |  566 | `	DWORD tv = (DWORD)iMilliseconds;` |
|    3 |  567 | `	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));` |
|    - |  568 | `#else` |
|    - |  569 | `	struct timeval tv;` |
|  122 |  570 | `	tv.tv_sec = iMilliseconds / 1000;` |
|  122 |  571 | `	tv.tv_usec = (iMilliseconds % 1000) * 1000;` |
|  122 |  572 | `	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(tv));` |
|    - |  573 | `#endif` |
|  125 |  574 | `}` |
|    - |  575 | `/*` |
|    - |  576 | ` * Set BOTH the receive and the send timeout, which is what php's` |
|    - |  577 | ` * stream_set_timeout() means by "the timeout of this stream". A zero pair` |
|    - |  578 | ` * means "no timeout" to the OS, and that is php's answer for it too.` |
|    - |  579 | ` */` |
|   72 |  580 | `PH7_PRIVATE void PH7_NetSetRwTimeout(ph7_socket sock, ph7_int64 iSeconds, ph7_int64 iMicroseconds)` |
|    3 |  581 | `{` |
|    - |  582 | `	/* Carry the microseconds over: the OS rejects a tv_usec of 1000000 or more` |
|    - |  583 | ``	 * outright (EINVAL), so `stream_set_timeout($s, 0, 1500000)` used to set`` |
|    - |  584 | `	 * NOTHING and leave the read unbounded while answering true. */` |
|   75 |  585 | `	if( iMicroseconds >= 1000000 ){` |
|  ! 0 |  586 | `		iSeconds += iMicroseconds / 1000000;` |
|  ! 0 |  587 | `		iMicroseconds %= 1000000;` |
|  ! 0 |  588 | `	}` |
|   75 |  589 | `	if( iSeconds == 0 && iMicroseconds == 0 ){` |
|    - |  590 | `		/* php's zero timeout means "do not wait", where a zero timeval means` |
|    - |  591 | `		 * "wait forever" to the OS: the smallest one it can express is what` |
|    - |  592 | `		 * carries that intent. */` |
|  ! 0 |  593 | `		iMicroseconds = 1;` |
|  ! 0 |  594 | `	}` |
|   75 |  595 | `	if( iSeconds > 4000000 ){` |
|    - |  596 | `		/* Beyond any real deadline, and past what a millisecond DWORD holds. */` |
|  ! 0 |  597 | `		iSeconds = 4000000;` |
|  ! 0 |  598 | `	}` |
|    - |  599 | `#ifdef __WINNT__` |
|    - |  600 | `	{` |
|    3 |  601 | `		DWORD tv = (DWORD)(iSeconds * 1000 + iMicroseconds / 1000);` |
|    3 |  602 | `		if( tv == 0 ){` |
|  ! 0 |  603 | `			tv = 1; /* a Windows zero is "block forever" too */` |
|    - |  604 | `		}` |
|    3 |  605 | `		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));` |
|    3 |  606 | `		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));` |
|    - |  607 | `	}` |
|    - |  608 | `#else` |
|    - |  609 | `	{` |
|    - |  610 | `		struct timeval tv;` |
|   72 |  611 | `		tv.tv_sec = (time_t)iSeconds;` |
|   72 |  612 | `		tv.tv_usec = (suseconds_t)iMicroseconds;` |
|   72 |  613 | `		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(tv));` |
|   72 |  614 | `		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const void *)&tv, sizeof(tv));` |
|    - |  615 | `	}` |
|    - |  616 | `#endif` |
|   75 |  617 | `}` |
|    - |  618 | `/*` |
|    - |  619 | ` * Turn a socket's blocking mode on or off. On Windows a socket is not an fd,` |
|    - |  620 | ` * so the fcntl() route the rest of the stream family takes cannot reach it.` |
|    - |  621 | ` */` |
|    4 |  622 | `PH7_PRIVATE void PH7_NetSetBlocking(ph7_socket sock, int bBlocking)` |
|    2 |  623 | `{` |
|    - |  624 | `#ifdef __WINNT__` |
|    2 |  625 | `	u_long iMode = bBlocking ? 0 : 1;` |
|    2 |  626 | `	ioctlsocket(sock, FIONBIO, &iMode);` |
|    - |  627 | `#else` |
|    4 |  628 | `	int iFlags = fcntl(sock, F_GETFL, 0);` |
|    4 |  629 | `	if( iFlags < 0 ){` |
|  ! 0 |  630 | `		return;` |
|    - |  631 | `	}` |
|    4 |  632 | `	if( bBlocking ){` |
|  ! 0 |  633 | `		iFlags &= ~O_NONBLOCK;` |
|  ! 0 |  634 | `	}else{` |
|    4 |  635 | `		iFlags \|= O_NONBLOCK;` |
|    - |  636 | `	}` |
|    4 |  637 | `	fcntl(sock, F_SETFL, iFlags);` |
|    - |  638 | `#endif` |
|    4 |  639 | `}` |
|    - |  640 | `/*` |
|    - |  641 | ` * Extract a human-readable IP address string from a sockaddr.` |
|    - |  642 | ` * Writes at most nBufLen bytes (including NUL) to zBuf.` |
|    - |  643 | ` */` |
|  110 |  644 | `PH7_PRIVATE void PH7_NetAddrToString(const struct sockaddr *pAddr, char *zBuf, int nBufLen)` |
|    3 |  645 | `{` |
|  113 |  646 | `	const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;` |
|  113 |  647 | `	if( pAddr == 0 \|\| pAddr->sa_family != AF_INET ){` |
|  ! 0 |  648 | `		if( nBufLen > 0 ){` |
|  ! 0 |  649 | `			zBuf[0] = 0;` |
|  ! 0 |  650 | `		}` |
|  ! 0 |  651 | `		return;` |
|    - |  652 | `	}` |
|    - |  653 | `#ifdef __WINNT__` |
|    - |  654 | `	{` |
|    3 |  655 | `		char *zAddr = inet_ntoa(pIn->sin_addr);` |
|    3 |  656 | `		if( zAddr ){` |
|    3 |  657 | `			int n = (int)strlen(zAddr);` |
|    3 |  658 | `			if( n >= nBufLen ) n = nBufLen - 1;` |
|    3 |  659 | `			memcpy(zBuf, zAddr, n);` |
|    3 |  660 | `			zBuf[n] = 0;` |
|    3 |  661 | `		}else{` |
|  ! 0 |  662 | `			zBuf[0] = 0;` |
|    - |  663 | `		}` |
|    - |  664 | `	}` |
|    - |  665 | `#else` |
|  110 |  666 | `	inet_ntop(AF_INET, &pIn->sin_addr, zBuf, (ph7_socklen)nBufLen);` |
|    - |  667 | `#endif` |
|   58 |  668 | `}` |
|    - |  669 | `/*` |
|    - |  670 | ` * Extract the port number from a sockaddr (in host byte order).` |
|    - |  671 | ` */` |
|  110 |  672 | `PH7_PRIVATE int PH7_NetAddrPort(const struct sockaddr *pAddr)` |
|    3 |  673 | `{` |
|  113 |  674 | `	const struct sockaddr_in *pIn = (const struct sockaddr_in *)pAddr;` |
|  113 |  675 | `	if( pAddr == 0 \|\| pAddr->sa_family != AF_INET ){` |
|  ! 0 |  676 | `		return 0;` |
|    - |  677 | `	}` |
|  113 |  678 | `	return (int)ntohs(pIn->sin_port);` |
|   58 |  679 | `}` |
|    - |  680 |  |
|    - |  681 | `/*` |
|    - |  682 | ` * The PLATFORM numbers php's socket constants carry, which is why they cannot` |
|    - |  683 | `` * be written down in a header: `STREAM_PF_INET6` is 10 on Linux, 23 on Windows`` |
|    - |  684 | ` * and 30 on the BSDs, and a program that hands one of them to` |
|    - |  685 | ` * stream_socket_pair() is handing the OS its own value.` |
|    - |  686 | ` */` |
|  812 |  687 | `PH7_PRIVATE ph7_int64 PH7_NetSocketConst(int iWhich)` |
|    4 |  688 | `{` |
|  816 |  689 | `	switch( iWhich ){` |
|   66 |  690 | `	case PH7_NETC_PF_INET:      return AF_INET;` |
|   65 |  691 | `	case PH7_NETC_PF_INET6:     return AF_INET6;` |
|   70 |  692 | `	case PH7_NETC_PF_UNIX:      return AF_UNIX;` |
|   68 |  693 | `	case PH7_NETC_SOCK_STREAM:  return SOCK_STREAM;` |
|   65 |  694 | `	case PH7_NETC_SOCK_DGRAM:   return SOCK_DGRAM;` |
|   65 |  695 | `	case PH7_NETC_SOCK_RAW:     return SOCK_RAW;` |
|   65 |  696 | `	case PH7_NETC_SOCK_SEQPACKET: return SOCK_SEQPACKET;` |
|   65 |  697 | `	case PH7_NETC_SOCK_RDM:     return SOCK_RDM;` |
|   66 |  698 | `	case PH7_NETC_IPPROTO_IP:   return IPPROTO_IP;` |
|   65 |  699 | `	case PH7_NETC_IPPROTO_TCP:  return IPPROTO_TCP;` |
|   65 |  700 | `	case PH7_NETC_IPPROTO_UDP:  return IPPROTO_UDP;` |
|   65 |  701 | `	case PH7_NETC_IPPROTO_ICMP: return IPPROTO_ICMP;` |
|   65 |  702 | `	case PH7_NETC_IPPROTO_RAW:  return IPPROTO_RAW;` |
|  ! 0 |  703 | `	default: break;` |
|    - |  704 | `	}` |
|  ! 0 |  705 | `	return 0;` |
|  410 |  706 | `}` |
|    - |  707 | `/*` |
|    - |  708 | ` * shutdown(), which is how a program says "I am done SENDING" without closing` |
|    - |  709 | ` * the handle it still wants to read — the half-close every line protocol ends` |
|    - |  710 | ` * with. php's three modes are 0/1/2 in its own numbering.` |
|    - |  711 | ` */` |
|    4 |  712 | `PH7_PRIVATE int PH7_NetShutdown(ph7_socket sock,int iHow)` |
|    1 |  713 | `{` |
|    - |  714 | `	int iSys;` |
|    - |  715 | `	/* Winsock spells the three SD_RECEIVE/SD_SEND/SD_BOTH, with the same` |
|    - |  716 | `	 * numbers; POSIX spells them SHUT_*. */` |
|    - |  717 | `#ifdef __WINNT__` |
|    1 |  718 | `	switch( iHow ){` |
|    1 |  719 | `	case 0:  iSys = SD_RECEIVE; break;` |
|    1 |  720 | `	case 1:  iSys = SD_SEND; break;` |
|  ! 0 |  721 | `	default: iSys = SD_BOTH; break;` |
|    - |  722 | `	}` |
|    - |  723 | `#else` |
|    4 |  724 | `	switch( iHow ){` |
|    2 |  725 | `	case 0:  iSys = SHUT_RD; break;` |
|    2 |  726 | `	case 1:  iSys = SHUT_WR; break;` |
|  ! 0 |  727 | `	default: iSys = SHUT_RDWR; break;` |
|    - |  728 | `	}` |
|    - |  729 | `#endif` |
|    5 |  730 | `	return shutdown(sock,iSys) == 0 ? PH7_OK : -1;` |
|    1 |  731 | `}` |
|    - |  732 | `/*` |
|    - |  733 | ` * Is there anything left on this socket to hand over? Asked of a socket whose` |
|    - |  734 | ` * READ side has just been shut down, where a recv() cannot block: php answers` |
|    - |  735 | ` * feof() for a socket by probing it, so a half-close with bytes still queued is` |
|    - |  736 | ` * NOT an end of file and one with nothing queued is.` |
|    - |  737 | ` */` |
|    2 |  738 | `PH7_PRIVATE int PH7_NetAtEnd(ph7_socket sock)` |
|    1 |  739 | `{` |
|    - |  740 | `	char c;` |
|    3 |  741 | `	return recv(sock,&c,1,MSG_PEEK) <= 0 ? 1 : 0;` |
|    1 |  742 | `}` |
|    - |  743 | `/* php's STREAM_OOB/STREAM_PEEK are php's own bits, not the OS's. */` |
|   14 |  744 | `static int NetMsgFlags(int iFlags)` |
|    1 |  745 | `{` |
|   15 |  746 | `	int iOut = 0;` |
|   15 |  747 | `	if( iFlags & PH7_STREAM_OOB ){` |
|  ! 0 |  748 | `		iOut \|= MSG_OOB;` |
|  ! 0 |  749 | `	}` |
|   15 |  750 | `	if( iFlags & PH7_STREAM_PEEK ){` |
|    3 |  751 | `		iOut \|= MSG_PEEK;` |
|    1 |  752 | `	}` |
|   15 |  753 | `	return iOut;` |
|    1 |  754 | `}` |
|    - |  755 | `/*` |
|    - |  756 | ` * Receive straight from the socket, with the sender's address when the datagram` |
|    - |  757 | ` * carries one. This deliberately does NOT go through the handle's read buffer:` |
|    - |  758 | ` * php's own recvfrom() asks the SOCKET, which is why it blocks on a handle whose` |
|    - |  759 | ` * buffer still holds bytes.` |
|    - |  760 | ` */` |
|    8 |  761 | `PH7_PRIVATE int PH7_NetRecvFrom(ph7_socket sock,void *pBuf,int nLen,int iFlags,char *zAddr,int nAddr)` |
|    1 |  762 | `{` |
|    - |  763 | `	struct sockaddr_in sFrom;` |
|    9 |  764 | `	ph7_socklen nFrom = (ph7_socklen)sizeof(sFrom);` |
|    - |  765 | `	int n;` |
|    9 |  766 | `	if( zAddr && nAddr > 0 ){` |
|    9 |  767 | `		zAddr[0] = 0;` |
|    4 |  768 | `	}` |
|    9 |  769 | `	memset(&sFrom,0,sizeof(sFrom));` |
|    9 |  770 | `	n = (int)recvfrom(sock,(char *)pBuf,nLen,NetMsgFlags(iFlags),(struct sockaddr *)&sFrom,&nFrom);` |
|    9 |  771 | `	if( n >= 0 && zAddr && nAddr > 0 && sFrom.sin_family == AF_INET ){` |
|    - |  772 | `		char zIp[64];` |
|  ! 0 |  773 | `		PH7_NetAddrToString((struct sockaddr *)&sFrom,zIp,(int)sizeof(zIp));` |
|  ! 0 |  774 | `		if( zIp[0] ){` |
|  ! 0 |  775 | `			snprintf(zAddr,(size_t)nAddr,"%s:%d",zIp,PH7_NetAddrPort((struct sockaddr *)&sFrom));` |
|  ! 0 |  776 | `		}` |
|  ! 0 |  777 | `	}` |
|    9 |  778 | `	return n;` |
|    1 |  779 | `}` |
|    - |  780 | `/*` |
|    - |  781 | ` * Send straight to the socket, to a named address when one is given. A` |
|    - |  782 | ` * connected socket takes the address too — php passes it to sendto() and lets` |
|    - |  783 | ` * the OS decide, which for a connected TCP socket means it is simply sent.` |
|    - |  784 | ` */` |
|    6 |  785 | `PH7_PRIVATE int PH7_NetSendTo(ph7_socket sock,const void *pBuf,int nLen,int iFlags,` |
|    - |  786 | `	const char *zHost,int iPort,int *pErrno)` |
|    1 |  787 | `{` |
|    - |  788 | `	struct sockaddr_in sTo;` |
|    7 |  789 | `	if( pErrno ){ *pErrno = 0; }` |
|    7 |  790 | `	if( zHost == 0 \|\| zHost[0] == 0 ){` |
|    5 |  791 | `		return (int)send(sock,(const char *)pBuf,nLen,NetMsgFlags(iFlags));` |
|    - |  792 | `	}` |
|    3 |  793 | `	memset(&sTo,0,sizeof(sTo));` |
|    3 |  794 | `	sTo.sin_family = AF_INET;` |
|    3 |  795 | `	sTo.sin_port = htons((unsigned short)iPort);` |
|    3 |  796 | `	if( strcmp(zHost,"localhost") == 0 \|\| strcmp(zHost,"127.0.0.1") == 0 ){` |
|    3 |  797 | `		sTo.sin_addr.s_addr = htonl(INADDR_LOOPBACK);` |
|    2 |  798 | `	}else{` |
|    - |  799 | `		struct addrinfo hints,*res;` |
|  ! 0 |  800 | `		memset(&hints,0,sizeof(hints));` |
|  ! 0 |  801 | `		hints.ai_family = AF_INET;` |
|  ! 0 |  802 | `		hints.ai_socktype = SOCK_DGRAM;` |
|  ! 0 |  803 | `		if( getaddrinfo(zHost,0,&hints,&res) != 0 \|\| res == 0 ){` |
|    - |  804 | `			/* Nothing was sent, and the caller says so in php's own three` |
|    - |  805 | `			 * voices — which is a different answer from a send that failed. */` |
|  ! 0 |  806 | `			if( pErrno ){ *pErrno = PH7_NET_ERR_RESOLVE; }` |
|  ! 0 |  807 | `			return -1;` |
|    - |  808 | `		}` |
|  ! 0 |  809 | `		sTo.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;` |
|  ! 0 |  810 | `		freeaddrinfo(res);` |
|    - |  811 | `	}` |
|    3 |  812 | `	return (int)sendto(sock,(const char *)pBuf,nLen,NetMsgFlags(iFlags),` |
|    - |  813 | `		(struct sockaddr *)&sTo,(ph7_socklen)sizeof(sTo));` |
|    4 |  814 | `}` |
|    - |  815 | `/*` |
|    - |  816 | ` * A connected PAIR of sockets, which is what a program hands a child process (or` |
|    - |  817 | ` * a test double) as a two-way pipe. POSIX has the call; Windows does not, and` |
|    - |  818 | ` * php builds the pair over the loopback there — a listener nobody else can` |
|    - |  819 | ` * reach, one connect, one accept, and a check that the socket that arrived is` |
|    - |  820 | ` * the one that was dialled.` |
|    - |  821 | ` */` |
|    4 |  822 | `PH7_PRIVATE int PH7_NetSocketPair(int iDomain,int iType,int iProtocol,ph7_socket *aOut,int *pErrno)` |
|    1 |  823 | `{` |
|    5 |  824 | `	if( pErrno ){ *pErrno = 0; }` |
|    5 |  825 | `	aOut[0] = aOut[1] = PH7_NET_INVALID_SOCKET;` |
|    5 |  826 | `	if( PH7_NetEnsureInit() != PH7_OK ){` |
|  ! 0 |  827 | `		return -1;` |
|    - |  828 | `	}` |
|    - |  829 | `#ifndef __WINNT__` |
|    - |  830 | `	{` |
|    - |  831 | `		int aFd[2];` |
|    4 |  832 | `		if( socketpair(iDomain,iType,iProtocol,aFd) != 0 ){` |
|    2 |  833 | `			if( pErrno ){ *pErrno = PH7_NetLastError(); }` |
|    2 |  834 | `			return -1;` |
|    - |  835 | `		}` |
|    2 |  836 | `		aOut[0] = aFd[0];` |
|    2 |  837 | `		aOut[1] = aFd[1];` |
|    2 |  838 | `		return PH7_OK;` |
|    - |  839 | `	}` |
|    - |  840 | `#else` |
|    - |  841 | `	{` |
|    - |  842 | `		ph7_socket listener,client,server;` |
|    - |  843 | `		struct sockaddr_in addr,peer,self;` |
|    - |  844 | `		ph7_socklen nAddr;` |
|    - |  845 | `		SXUNUSED(iProtocol);` |
|    1 |  846 | `		if( iDomain != AF_INET \|\| iType != SOCK_STREAM ){` |
|    - |  847 | `			/* php's own Windows emulation covers exactly this pair, and answers` |
|    - |  848 | `			 * "protocol not available" for anything else — the mirror image of` |
|    - |  849 | `			 * POSIX, where AF_UNIX is the one that works. */` |
|    1 |  850 | `			if( pErrno ){ *pErrno = WSAENOPROTOOPT; }` |
|    1 |  851 | `			return -1;` |
|    - |  852 | `		}` |
|    1 |  853 | `		listener = PH7_NetBind("127.0.0.1",0,0,1,1,0,pErrno,0);` |
|    1 |  854 | `		if( listener == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  855 | `			return -1;` |
|    - |  856 | `		}` |
|    1 |  857 | `		nAddr = (ph7_socklen)sizeof(addr);` |
|    1 |  858 | `		memset(&addr,0,sizeof(addr));` |
|    1 |  859 | `		if( getsockname(listener,(struct sockaddr *)&addr,&nAddr) != 0 ){` |
|  ! 0 |  860 | `			if( pErrno ){ *pErrno = PH7_NetLastError(); }` |
|  ! 0 |  861 | `			PH7_NetClose(listener);` |
|  ! 0 |  862 | `			return -1;` |
|    - |  863 | `		}` |
|    1 |  864 | `		client = socket(AF_INET,SOCK_STREAM,0);` |
|    - |  865 | `		if( client == PH7_NET_INVALID_SOCKET` |
|    1 |  866 | `		 \|\| connect(client,(struct sockaddr *)&addr,(ph7_socklen)sizeof(addr)) != 0 ){` |
|  ! 0 |  867 | `			if( pErrno ){ *pErrno = PH7_NetLastError(); }` |
|  ! 0 |  868 | `			PH7_NetClose(client);` |
|  ! 0 |  869 | `			PH7_NetClose(listener);` |
|  ! 0 |  870 | `			return -1;` |
|    - |  871 | `		}` |
|    1 |  872 | `		nAddr = (ph7_socklen)sizeof(peer);` |
|    1 |  873 | `		memset(&peer,0,sizeof(peer));` |
|    1 |  874 | `		server = accept(listener,(struct sockaddr *)&peer,&nAddr);` |
|    1 |  875 | `		PH7_NetClose(listener);` |
|    1 |  876 | `		if( server == PH7_NET_INVALID_SOCKET ){` |
|  ! 0 |  877 | `			if( pErrno ){ *pErrno = PH7_NetLastError(); }` |
|  ! 0 |  878 | `			PH7_NetClose(client);` |
|  ! 0 |  879 | `			return -1;` |
|    - |  880 | `		}` |
|    - |  881 | `		/* The connection that arrived must be the one that was made: another` |
|    - |  882 | `		 * process could have reached the same listener first. */` |
|    1 |  883 | `		nAddr = (ph7_socklen)sizeof(self);` |
|    1 |  884 | `		memset(&self,0,sizeof(self));` |
|    - |  885 | `		if( getsockname(client,(struct sockaddr *)&self,&nAddr) != 0` |
|    - |  886 | `		 \|\| self.sin_port != peer.sin_port` |
|    1 |  887 | `		 \|\| self.sin_addr.s_addr != peer.sin_addr.s_addr ){` |
|  ! 0 |  888 | `			if( pErrno ){ *pErrno = WSAECONNABORTED; }` |
|  ! 0 |  889 | `			PH7_NetClose(client);` |
|  ! 0 |  890 | `			PH7_NetClose(server);` |
|  ! 0 |  891 | `			return -1;` |
|    - |  892 | `		}` |
|    1 |  893 | `		aOut[0] = client;` |
|    1 |  894 | `		aOut[1] = server;` |
|    1 |  895 | `		return PH7_OK;` |
|    - |  896 | `	}` |
|    - |  897 | `#endif` |
|    3 |  898 | `}` |
|    - |  899 |  |
|    - |  900 | `#endif /* PH7_ENABLE_NET */` |
|    - |  901 |  |
