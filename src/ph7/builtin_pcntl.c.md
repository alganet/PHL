# src/ph7/builtin_pcntl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 581/844 lines (68.84%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    4 | ` */` |
|       - |    5 | `/* waitid(), unshare(), sched_getaffinity() and sched_getcpu() are outside the` |
|       - |    6 | ` * strict POSIX subset a default compile exposes, and php's own build asks for` |
|       - |    7 | ` * them the same way. This has to come BEFORE any system header, which is why it` |
|       - |    8 | ` * is above ph7int.h rather than beside the includes below. */` |
|       - |    9 | `#ifndef _GNU_SOURCE` |
|       - |   10 | `#define _GNU_SOURCE 1` |
|       - |   11 | `#endif` |
|       - |   12 | `#ifndef _DEFAULT_SOURCE` |
|       - |   13 | `#define _DEFAULT_SOURCE 1` |
|       - |   14 | `#endif` |
|       - |   15 | `#include "ph7int.h"` |
|       - |   16 | `/*` |
|       - |   17 | ` * Section:` |
|       - |   18 | ` *    php's pcntl extension: fork/exec, the wait family, and the SIGNAL half a` |
|       - |   19 | ` *    command-line program handles Ctrl-C with.` |
|       - |   20 | ` * Status:` |
|       - |   21 | ` *    Stable.` |
|       - |   22 | ` *` |
|       - |   23 | ` * php's ext/pcntl is a shell over the C library's process and signal calls, so` |
|       - |   24 | ` * every answer here is the platform's; what had to be reproduced is the shape` |
|       - |   25 | ` * php gives them, and one piece of machinery that is not a system call at all.` |
|       - |   26 | ` *` |
|       - |   27 | ` *   THE EXTENSION IS NOT THERE ON WINDOWS. php builds no ext/pcntl for it, so` |
|       - |   28 | `` *   `extension_loaded('pcntl')` and `function_exists('pcntl_signal')` are both`` |
|       - |   29 | ` *   false on a Windows php -- which is exactly what monolog's SignalHandler and` |
|       - |   30 | ` *   symfony/console's SignalRegistry test before calling anything. This whole` |
|       - |   31 | ` *   translation unit is empty there, and the constants are not defined either.` |
|       - |   32 | `` *   The `posix` precedent (124th session) is the same rule.`` |
|       - |   33 | ` *` |
|       - |   34 | ` *   A SIGNAL IS NOT DELIVERED TO PHP CODE WHERE THE KERNEL DELIVERS IT. The C` |
|       - |   35 | ` *   handler may not call an interpreter, so it records the delivery in a` |
|       - |   36 | ` *   preallocated ring (nothing it does allocates or takes a lock) and the PHP` |
|       - |   37 | ` *   handler runs later, at a point the engine chooses. There are two such` |
|       - |   38 | `` *   points, and which one a program gets is `pcntl_async_signals()`:`` |
|       - |   39 | ` *` |
|       - |   40 | ` *     ASYNC OFF (php's default): nothing runs until the program itself asks,` |
|       - |   41 | `` *     with `pcntl_signal_dispatch()`. A signal that arrives between two such`` |
|       - |   42 | ` *     calls is remembered, not lost.` |
|       - |   43 | ` *` |
|       - |   44 | ` *     ASYNC ON: the C handler raises PH7_PcntlAsyncPending, and the executor's` |
|       - |   45 | ` *     fetch point -- the same guard that routes a C-boundary throw -- drains` |
|       - |   46 | ``  *     the ring before the next instruction. That is php's `EG(vm_interrupt)` `` |
|       - |   47 | ``  *     with a different name, and it is why `posix_kill(getmypid(), SIGURG)` `` |
|       - |   48 | `` *     followed by an `echo` prints the handler's line FIRST.`` |
|       - |   49 | ` *` |
|       - |   50 | ` *   The two share one ring, so a signal that arrived while async was off is` |
|       - |   51 | ` *   still there afterwards -- but turning async ON does not sweep it up. php` |
|       - |   52 | ` *   raises its interrupt from the SIGNAL HANDLER and nowhere else, so what is` |
|       - |   53 | ` *   already queued waits for the next dispatch or the next signal, and under an` |
|       - |   54 | `` *   async program `pcntl_signal_dispatch()` usually just finds the ring empty`` |
|       - |   55 | ` *   and answers true.` |
|       - |   56 | ` *` |
|       - |   57 | ` *   A DISPATCH DETACHES THE QUEUE before it runs anything, which is what stops` |
|       - |   58 | ` *   a handler that re-raises its own signal from looping for ever: the new` |
|       - |   59 | ` *   delivery lands on what is, by then, a fresh queue. A dispatch reached from` |
|       - |   60 | ` *   INSIDE a handler is a no-op for the same reason. And a handler that THROWS` |
|       - |   61 | ` *   takes the rest of the detached queue with it -- php's own loop keeps` |
|       - |   62 | ` *   walking but zend refuses to call anything while an exception is pending, so` |
|       - |   63 | ` *   whatever was queued behind the thrower is dropped rather than deferred.` |
|       - |   64 | ` *` |
|       - |   65 | `` *   WHAT A HANDLER IS TOLD is `($signo, $siginfo)`, and the second argument is`` |
|       - |   66 | `` *   php's own selection out of `siginfo_t` rather than the whole of it: signo,`` |
|       - |   67 | ` *   errno and code always, plus status/utime/stime/pid/uid for SIGCHLD, addr` |
|       - |   68 | ` *   (as a FLOAT, which is php's own choice) for the four fault signals, and` |
|       - |   69 | `` *   band/fd for SIGPOLL. `pcntl_sigwaitinfo()`, `pcntl_sigtimedwait()` and`` |
|       - |   70 | `` *   `pcntl_waitid()` fill the same array.`` |
|       - |   71 | ` *` |
|       - |   72 | ``  *   FAILURE IS false (or a negative number) AND AN ERRNO. `pcntl_get_last_error()` `` |
|       - |   73 | `` *   (and its alias `pcntl_errno()`) answers what the last call that stored one`` |
|       - |   74 | ` *   saw, and nothing clears it -- exactly the ext/posix rule, and a separate` |
|       - |   75 | ` *   number from posix's. Most failures ALSO raise an E_WARNING whose text is a` |
|       - |   76 | ` *   per-function errno table php wrote by hand ("Error 3: No process was located` |
|       - |   77 | `` *   using the given parameters"); `pcntl_sigtimedwait()` is the one that stores`` |
|       - |   78 | ` *   nothing on its own timeout, because EAGAIN is that call's success.` |
|       - |   79 | ` *` |
|       - |   80 | `` *   RESTARTING IS PER SIGNAL. `pcntl_signal($signo, $h, $restart)` picks`` |
|       - |   81 | `` *   SA_RESTART or not, so a `pcntl_waitpid()` interrupted by that signal either`` |
|       - |   82 | ` *   resumes or answers -1/EINTR -- which is a behaviour monolog's suite asserts` |
|       - |   83 | ` *   both ways. SIGALRM is never restarted, whatever the argument says: that is` |
|       - |   84 | ` *   php's own exception and it is reproduced.` |
|       - |   85 | ` */` |
|       - |   86 | `#include <signal.h>` |
|       - |   87 | `/*` |
|       - |   88 | ` * The async-dispatch flag, raised by the C signal handler and read by the` |
|       - |   89 | ` * executor's fetch point (VmLoopFetch). It lives OUTSIDE every guard below` |
|       - |   90 | ` * because vm_exec.c reads it in every build -- on Windows and in the tiny` |
|       - |   91 | ` * build it is simply always zero, and the drain beside it is a no-op.` |
|       - |   92 | ` */` |
|       - |   93 | `PH7_PRIVATE volatile sig_atomic_t PH7_PcntlAsyncPending = 0;` |
|       - |   94 |  |
|       - |   95 | `#if defined(PH7_DISABLE_BUILTIN_FUNC) \|\| defined(__WINNT__)` |
|       - |   96 | `/* No ext/pcntl in this build: the executor still links against the drain. */` |
|       - |   97 | `PH7_PRIVATE void PH7_PcntlDrainAsync(ph7_vm *pVm)` |
|     ! 0 |   98 | `{` |
|       - |   99 | `	SXUNUSED(pVm);` |
|     ! 0 |  100 | `	PH7_PcntlAsyncPending = 0;` |
|     ! 0 |  101 | `}` |
|       - |  102 | `PH7_PRIVATE void PH7_PcntlVmRelease(ph7_vm *pVm)` |
|       5 |  103 | `{` |
|       - |  104 | `	SXUNUSED(pVm);` |
|       5 |  105 | `}` |
|       - |  106 | `PH7_PRIVATE void PH7_RegisterPcntlConstants(ph7_vm *pVm)` |
|       5 |  107 | `{` |
|       - |  108 | `	SXUNUSED(pVm);` |
|       5 |  109 | `}` |
|       - |  110 | `PH7_PRIVATE sxi32 PH7_VmInstallPcntl(ph7_vm *pVm)` |
|       5 |  111 | `{` |
|       - |  112 | `	SXUNUSED(pVm);` |
|       5 |  113 | `	return SXRET_OK;` |
|       5 |  114 | `}` |
|       - |  115 | `#else /* the real thing */` |
|       - |  116 |  |
|       - |  117 | `#include <errno.h>` |
|       - |  118 | `#include <string.h>` |
|       - |  119 | `#include <stdlib.h>` |
|       - |  120 | `#include <unistd.h>` |
|       - |  121 | `#include <sys/types.h>` |
|       - |  122 | `#include <sys/wait.h>` |
|       - |  123 | `#include <sys/time.h>` |
|       - |  124 | `#include <sys/resource.h>` |
|       - |  125 | `#ifdef __linux__` |
|       - |  126 | `#include <sched.h>` |
|       - |  127 | `#endif` |
|       - |  128 | `#include <sys/param.h>    /* HZ, which php divides si_utime/si_stime by */` |
|       - |  129 |  |
|       - |  130 | `/* php's own bound on a signal number, and the one its diagnostics quote. */` |
|       - |  131 | `#ifndef NSIG` |
|       - |  132 | `#define NSIG 32` |
|       - |  133 | `#endif` |
|       - |  134 | `#define PCNTL_NSIG NSIG` |
|       - |  135 |  |
|       - |  136 | `/* The delivery ring. Sized well past anything a program can leave pending: the` |
|       - |  137 | ` * kernel coalesces a non-realtime signal that is already queued, so the depth` |
|       - |  138 | ` * that matters is "distinct signals delivered between two drains". */` |
|       - |  139 | `#define PCNTL_QUEUE 64` |
|       - |  140 |  |
|       - |  141 | `/* php divides si_utime/si_stime by HZ. */` |
|       - |  142 | `#ifndef HZ` |
|       - |  143 | `#define HZ 100` |
|       - |  144 | `#endif` |
|       - |  145 |  |
|       - |  146 | `typedef struct pcntl_event pcntl_event;` |
|       - |  147 | `struct pcntl_event {` |
|       - |  148 | `	int signo;` |
|       - |  149 | `	siginfo_t sInfo;` |
|       - |  150 | `};` |
|       - |  151 | `/*` |
|       - |  152 | ` * The extension's per-VM state: the handler a signal was last given, and the` |
|       - |  153 | ` * two numbers php keeps beside it. php holds one set per MODULE; per VM is the` |
|       - |  154 | ` * same lifetime for a program and keeps two embedded VMs apart, which is the` |
|       - |  155 | ` * shape ext/posix and ext/gettext already use.` |
|       - |  156 | ` */` |
|       - |  157 | `typedef struct pcntl_state pcntl_state;` |
|       - |  158 | `struct pcntl_state {` |
|       - |  159 | `	ph7_value aHandler[PCNTL_NSIG]; /* the value pcntl_signal() was given, per signal */` |
|       - |  160 | `	sxu8 aSet[PCNTL_NSIG];          /* 1 once that slot has been written */` |
|       - |  161 | `	int iLastError;                 /* pcntl_get_last_error() */` |
|       - |  162 | `	int bAsync;                     /* pcntl_async_signals() */` |
|       - |  163 | `	int bDraining;                  /* re-entry guard around the drain */` |
|       - |  164 | `};` |
|       - |  165 | `/*` |
|       - |  166 | ` * The C signal handler reaches the ring through file statics, because that is` |
|       - |  167 | ` * all a handler may touch: no allocation, no lock, no interpreter. The cursors` |
|       - |  168 | ` * live in [0, 2*PCNTL_QUEUE) rather than counting up for ever, so a program` |
|       - |  169 | ` * that handles signals for weeks cannot overflow them into a negative index.` |
|       - |  170 | ` * The VM pointer beside them is not the handler's: it only records WHOSE` |
|       - |  171 | ` * handler table the process-wide dispositions belong to, so teardown of a` |
|       - |  172 | ` * different VM leaves them alone.` |
|       - |  173 | ` */` |
|       - |  174 | `static ph7_vm *pPcntlVm = 0;` |
|       - |  175 | `static volatile sig_atomic_t bPcntlAsyncFlag = 0;` |
|       - |  176 | `static volatile sig_atomic_t nPcntlHead = 0;` |
|       - |  177 | `static volatile sig_atomic_t nPcntlTail = 0;` |
|       - |  178 | `static pcntl_event aPcntlQueue[PCNTL_QUEUE];` |
|       - |  179 |  |
|       - |  180 | `/* --- The state ---------------------------------------------------------- */` |
|       - |  181 |  |
|     100 |  182 | `static pcntl_state * PcntlState(ph7_vm *pVm,int bCreate)` |
|       - |  183 | `{` |
|     100 |  184 | `	pcntl_state *pS = (pcntl_state *)pVm->pPcntl;` |
|     100 |  185 | `	if( pS == 0 && bCreate ){` |
|       - |  186 | `		int i;` |
|       4 |  187 | `		pS = (pcntl_state *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(pcntl_state));` |
|       4 |  188 | `		if( pS == 0 ){` |
|     ! 0 |  189 | `			return 0;` |
|       - |  190 | `		}` |
|       4 |  191 | `		SyZero(pS,sizeof(pcntl_state));` |
|     231 |  192 | `		for( i = 0 ; i < PCNTL_NSIG ; ++i ){` |
|     227 |  193 | `			PH7_MemObjInit(pVm,&pS->aHandler[i]);` |
|      32 |  194 | `		}` |
|       4 |  195 | `		pVm->pPcntl = pS;` |
|       4 |  196 | `		pPcntlVm = pVm;` |
|       1 |  197 | `	}` |
|     100 |  198 | `	return pS;` |
|      13 |  199 | `}` |
|       - |  200 | `/*` |
|       - |  201 | ` * Remember WHY the last call failed. Which calls store one is per-function and` |
|       - |  202 | `` * measured: most failures do, `pcntl_sigtimedwait()`'s own timeout does not`` |
|       - |  203 | ` * (EAGAIN is that call's "nothing arrived"), and two REFUSALS do even though` |
|       - |  204 | ` * they throw rather than answer -- a handler that is not callable and a` |
|       - |  205 | ` * priority mode php does not name both leave EINVAL behind.` |
|       - |  206 | ` */` |
|      18 |  207 | `static void PcntlStoreErr(ph7_vm *pVm,int iErr)` |
|       - |  208 | `{` |
|      18 |  209 | `	pcntl_state *pS = PcntlState(pVm,1);` |
|      18 |  210 | `	if( pS ){` |
|      18 |  211 | `		pS->iLastError = iErr;` |
|       3 |  212 | `	}` |
|      18 |  213 | `}` |
|       - |  214 |  |
|       - |  215 | `/* --- The delivery ring -------------------------------------------------- */` |
|       - |  216 |  |
|       - |  217 | `/*` |
|       - |  218 | ` * The C handler. Everything it touches is preallocated and everything it does` |
|       - |  219 | ` * is a store, so it is async-signal-safe in the strict sense -- php's own` |
|       - |  220 | ` * allocates a spare queue entry here, which is a known wart rather than a model.` |
|       - |  221 | ` * A full ring DROPS the delivery: the alternative is allocating inside a signal` |
|       - |  222 | ` * handler, and the depth is far past what a program can hold pending.` |
|       - |  223 | ` */` |
|      19 |  224 | `static void PcntlSigHandler(int signo,siginfo_t *pInfo,void *pUnused)` |
|       - |  225 | `{` |
|      19 |  226 | `	int nT = (int)nPcntlTail;` |
|      19 |  227 | `	int nUsed = (nT - (int)nPcntlHead + 2 * PCNTL_QUEUE) % (2 * PCNTL_QUEUE);` |
|      19 |  228 | `	int nSaved = errno;` |
|       3 |  229 | `	SXUNUSED(pUnused);` |
|      19 |  230 | `	if( nUsed < PCNTL_QUEUE ){` |
|      19 |  231 | `		pcntl_event *pE = &aPcntlQueue[nT % PCNTL_QUEUE];` |
|      19 |  232 | `		pE->signo = signo;` |
|      19 |  233 | `		if( pInfo ){` |
|      19 |  234 | `			pE->sInfo = *pInfo;` |
|       3 |  235 | `		}else{` |
|     ! 0 |  236 | `			SyZero(&pE->sInfo,sizeof(pE->sInfo));` |
|     ! 0 |  237 | `			pE->sInfo.si_signo = signo;` |
|       - |  238 | `		}` |
|       - |  239 | `		/* The store has to be complete before the cursor says it is there. */` |
|      19 |  240 | `		nPcntlTail = (sig_atomic_t)((nT + 1) % (2 * PCNTL_QUEUE));` |
|       3 |  241 | `	}` |
|      22 |  242 | `	if( bPcntlAsyncFlag ){` |
|       2 |  243 | `		PH7_PcntlAsyncPending = 1;` |
|     ! 0 |  244 | `	}` |
|      19 |  245 | `	errno = nSaved;` |
|      19 |  246 | `}` |
|       - |  247 | `/*` |
|       - |  248 | ` * php's php_signal4(): SA_SIGINFO always, every other signal blocked for the` |
|       - |  249 | ` * duration when the handler is ours, and SA_RESTART unless the caller asked for` |
|       - |  250 | ` * an interruptible one -- or the signal is SIGALRM, which php never restarts.` |
|       - |  251 | ` * Answers 0 on success and -1 with errno set.` |
|       - |  252 | ` */` |
|      36 |  253 | `static int PcntlInstall(int signo,void *pFunc,int bRestart,int bMaskAll)` |
|       - |  254 | `{` |
|       - |  255 | `	struct sigaction sAct;` |
|      36 |  256 | `	SyZero(&sAct,sizeof(sAct));` |
|      36 |  257 | `	if( pFunc == (void *)PcntlSigHandler ){` |
|      14 |  258 | `		sAct.sa_sigaction = PcntlSigHandler;` |
|      14 |  259 | `		sAct.sa_flags = SA_SIGINFO;` |
|       3 |  260 | `	}else{` |
|      22 |  261 | `		sAct.sa_handler = (void (*)(int))pFunc;` |
|      22 |  262 | `		sAct.sa_flags = 0;` |
|       - |  263 | `	}` |
|      36 |  264 | `	if( bMaskAll ){` |
|      14 |  265 | `		sigfillset(&sAct.sa_mask);` |
|       3 |  266 | `	}else{` |
|      22 |  267 | `		sigemptyset(&sAct.sa_mask);` |
|       - |  268 | `	}` |
|       - |  269 | `#ifdef SIGALRM` |
|      36 |  270 | `	if( signo != SIGALRM && bRestart ){` |
|       - |  271 | `#else` |
|       - |  272 | `	if( bRestart ){` |
|       - |  273 | `#endif` |
|       - |  274 | `#ifdef SA_RESTART` |
|      34 |  275 | `		sAct.sa_flags \|= SA_RESTART;` |
|       - |  276 | `#endif` |
|       8 |  277 | `	}else{` |
|       - |  278 | `#ifdef SA_INTERRUPT` |
|       1 |  279 | `		sAct.sa_flags \|= SA_INTERRUPT;   /* SunOS */` |
|       - |  280 | `#endif` |
|       - |  281 | `	}` |
|      36 |  282 | `	return sigaction(signo,&sAct,0);` |
|       - |  283 | `}` |
|       - |  284 |  |
|       - |  285 | `/* --- What a handler is told --------------------------------------------- */` |
|       - |  286 |  |
|       - |  287 | `/*` |
|       - |  288 | `` * php's siginfo array: three keys always, and a per-signal tail. `addr` is a`` |
|       - |  289 | ` * FLOAT because php adds it with add_assoc_double -- reproduced rather than` |
|       - |  290 | ` * corrected, since a program can print it.` |
|       - |  291 | ` */` |
|      20 |  292 | `static void PcntlSigInfoArray(ph7_vm *pVm,ph7_value *pArray,int signo,const siginfo_t *pInfo)` |
|       - |  293 | `{` |
|      20 |  294 | `	ph7_value *pVal = ph7_new_scalar(pVm);` |
|      20 |  295 | `	if( pVal == 0 ){` |
|     ! 0 |  296 | `		return;` |
|       - |  297 | `	}` |
|      20 |  298 | `	ph7_value_int64(pVal,(ph7_int64)pInfo->si_signo);` |
|      20 |  299 | `	ph7_array_add_strkey_elem(pArray,"signo",pVal);` |
|      20 |  300 | `	ph7_value_int64(pVal,(ph7_int64)pInfo->si_errno);` |
|      20 |  301 | `	ph7_array_add_strkey_elem(pArray,"errno",pVal);` |
|      20 |  302 | `	ph7_value_int64(pVal,(ph7_int64)pInfo->si_code);` |
|      20 |  303 | `	ph7_array_add_strkey_elem(pArray,"code",pVal);` |
|      20 |  304 | `	switch( signo ){` |
|       - |  305 | `#ifdef SIGCHLD` |
|       1 |  306 | `	case SIGCHLD:` |
|       2 |  307 | `		ph7_value_int64(pVal,(ph7_int64)pInfo->si_status);` |
|       2 |  308 | `		ph7_array_add_strkey_elem(pArray,"status",pVal);` |
|       - |  309 | `#ifdef si_utime` |
|       1 |  310 | `		ph7_value_double(pVal,(double)pInfo->si_utime / (double)HZ);` |
|       1 |  311 | `		ph7_array_add_strkey_elem(pArray,"utime",pVal);` |
|       - |  312 | `#endif` |
|       - |  313 | `#ifdef si_stime` |
|       1 |  314 | `		ph7_value_double(pVal,(double)pInfo->si_stime / (double)HZ);` |
|       1 |  315 | `		ph7_array_add_strkey_elem(pArray,"stime",pVal);` |
|       - |  316 | `#endif` |
|       2 |  317 | `		ph7_value_int64(pVal,(ph7_int64)pInfo->si_pid);` |
|       2 |  318 | `		ph7_array_add_strkey_elem(pArray,"pid",pVal);` |
|       2 |  319 | `		ph7_value_int64(pVal,(ph7_int64)pInfo->si_uid);` |
|       2 |  320 | `		ph7_array_add_strkey_elem(pArray,"uid",pVal);` |
|       2 |  321 | `		break;` |
|       - |  322 | `#endif` |
|     ! 0 |  323 | `	case SIGILL:` |
|       - |  324 | `	case SIGFPE:` |
|       - |  325 | `	case SIGSEGV:` |
|       - |  326 | `	case SIGBUS:` |
|     ! 0 |  327 | `		ph7_value_double(pVal,(double)(sxi64)(sxptr)pInfo->si_addr);` |
|     ! 0 |  328 | `		ph7_array_add_strkey_elem(pArray,"addr",pVal);` |
|     ! 0 |  329 | `		break;` |
|       - |  330 | `#ifdef SIGPOLL` |
|     ! 0 |  331 | `	case SIGPOLL:` |
|     ! 0 |  332 | `		ph7_value_int64(pVal,(ph7_int64)pInfo->si_band);` |
|     ! 0 |  333 | `		ph7_array_add_strkey_elem(pArray,"band",pVal);` |
|       - |  334 | `#ifdef si_fd` |
|     ! 0 |  335 | `		ph7_value_int64(pVal,(ph7_int64)pInfo->si_fd);` |
|     ! 0 |  336 | `		ph7_array_add_strkey_elem(pArray,"fd",pVal);` |
|       - |  337 | `#endif` |
|     ! 0 |  338 | `		break;` |
|       - |  339 | `#endif` |
|      15 |  340 | `	default:` |
|      18 |  341 | `		break;` |
|       - |  342 | `	}` |
|      20 |  343 | `	ph7_release_value(pVm,pVal);` |
|       4 |  344 | `}` |
|       - |  345 | `/*` |
|       - |  346 | ` * Run the PHP handlers for everything the ring held WHEN THIS STARTED. Answers` |
|       - |  347 | ` * the unwind a handler produced (PH7_EXCEPTION / PH7_ABORT) so the sync door` |
|       - |  348 | ` * can propagate it; the async door needs nothing back, because` |
|       - |  349 | ` * PH7_VmCallUserFunction has already parked the same status for the fetch` |
|       - |  350 | ` * point that called it.` |
|       - |  351 | ` *` |
|       - |  352 | ` * Three things keep this from running away, and each of them is php's:` |
|       - |  353 | ` * the async flag is cleared before any handler runs, a re-entrant call is a` |
|       - |  354 | ` * no-op, and the queue is SNAPSHOT rather than followed.` |
|       - |  355 | ` */` |
|      22 |  356 | `static sxi32 PcntlDrain(ph7_vm *pVm)` |
|       - |  357 | `{` |
|      22 |  358 | `	pcntl_state *pS = PcntlState(pVm,0);` |
|      22 |  359 | `	sxi32 rc = SXRET_OK, rcUnwind = SXRET_OK;` |
|      22 |  360 | `	int nEnd, bUnwound = 0;` |
|      22 |  361 | `	if( pS == 0 ){` |
|       - |  362 | `		/* Nothing can consume the flag: drop it rather than spin on it. */` |
|     ! 0 |  363 | `		PH7_PcntlAsyncPending = 0;` |
|     ! 0 |  364 | `		return SXRET_OK;` |
|       - |  365 | `	}` |
|      22 |  366 | `	if( pS->bDraining ){` |
|       - |  367 | `		/* Re-entered from inside a handler -- php keeps the same guard, and` |
|       - |  368 | `		 * monolog's SignalHandler calls pcntl_signal_dispatch() from within` |
|       - |  369 | `		 * one. Everything queued since the outer drain started is past its` |
|       - |  370 | `		 * snapshot and belongs to the NEXT dispatch, so there is nothing to do` |
|       - |  371 | `		 * here and the flag is left as the outer call left it. */` |
|       2 |  372 | `		return SXRET_OK;` |
|       - |  373 | `	}` |
|      20 |  374 | `	pS->bDraining = 1;` |
|      20 |  375 | `	PH7_PcntlAsyncPending = 0;` |
|       - |  376 | `	/* SNAPSHOT the tail. php detaches the whole queue before it runs anything` |
|       - |  377 | ``	 * (`PCNTL_G(head) = NULL`) precisely so that a handler which raises the`` |
|       - |  378 | `	 * signal it is handling lands on a FRESH queue, dispatched by the NEXT` |
|       - |  379 | `	 * call -- reading the live tail each round instead makes such a handler` |
|       - |  380 | `	 * loop forever, which is what the first cut of this did. */` |
|      20 |  381 | `	nEnd = (int)nPcntlTail;` |
|      39 |  382 | `	while( (int)nPcntlHead != nEnd ){` |
|      19 |  383 | `		pcntl_event sEv = aPcntlQueue[((int)nPcntlHead) % PCNTL_QUEUE];` |
|       - |  384 | `		ph7_value *pArgs[2];` |
|       - |  385 | `		ph7_value sRes, sCb;` |
|      19 |  386 | `		nPcntlHead = (sig_atomic_t)(((int)nPcntlHead + 1) % (2 * PCNTL_QUEUE));` |
|      19 |  387 | `		if( sEv.signo < 1 \|\| sEv.signo >= PCNTL_NSIG ){` |
|     ! 0 |  388 | `			continue;` |
|       - |  389 | `		}` |
|      19 |  390 | `		if( bUnwound \|\| !pS->aSet[sEv.signo] ){` |
|       1 |  391 | `			continue;` |
|       - |  392 | `		}` |
|       - |  393 | `		/* SIG_DFL / SIG_IGN are stored as INTEGERS, and php skips those rather` |
|       - |  394 | `		 * than calling them -- a disposition that changed between the delivery` |
|       - |  395 | `		 * and the drain is therefore honoured at the drain. */` |
|      18 |  396 | `		if( (pS->aHandler[sEv.signo].iFlags & (MEMOBJ_INT\|MEMOBJ_NULL)) != 0 ){` |
|       2 |  397 | `			continue;` |
|       - |  398 | `		}` |
|      16 |  399 | `		pArgs[0] = ph7_new_scalar(pVm);` |
|      16 |  400 | `		pArgs[1] = ph7_new_array(pVm);` |
|      16 |  401 | `		if( pArgs[0] == 0 \|\| pArgs[1] == 0 ){` |
|     ! 0 |  402 | `			if( pArgs[0] ){ ph7_release_value(pVm,pArgs[0]); }` |
|     ! 0 |  403 | `			if( pArgs[1] ){ ph7_release_value(pVm,pArgs[1]); }` |
|     ! 0 |  404 | `			break;` |
|       - |  405 | `		}` |
|      16 |  406 | `		ph7_value_int64(pArgs[0],(ph7_int64)sEv.signo);` |
|      16 |  407 | `		PcntlSigInfoArray(pVm,pArgs[1],sEv.signo,&sEv.sInfo);` |
|      16 |  408 | `		PH7_MemObjInit(pVm,&sRes);` |
|       - |  409 | `		/* Call a COPY of the handler, not the slot. A handler is allowed to` |
|       - |  410 | `		 * re-install the signal it is handling -- monolog's SignalHandler puts` |
|       - |  411 | `		 * SIG_DFL back, re-raises, and then restores itself, all from inside` |
|       - |  412 | `		 * this call -- and that releases the very value being dispatched. */` |
|      16 |  413 | `		PH7_MemObjInit(pVm,&sCb);` |
|      16 |  414 | `		PH7_MemObjStore(&pS->aHandler[sEv.signo],&sCb);` |
|      16 |  415 | `		rc = PH7_VmCallUserFunction(pVm,&sCb,2,pArgs,&sRes);` |
|      16 |  416 | `		PH7_MemObjRelease(&sCb);` |
|      16 |  417 | `		PH7_MemObjRelease(&sRes);` |
|      16 |  418 | `		ph7_release_value(pVm,pArgs[0]);` |
|      16 |  419 | `		ph7_release_value(pVm,pArgs[1]);` |
|      16 |  420 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       - |  421 | `			/* php CONSUMES the rest of the detached queue without calling` |
|       - |  422 | `			 * anything: zend_call_function refuses to run while an exception is` |
|       - |  423 | `			 * pending, and the nodes are recycled anyway. So a signal that was` |
|       - |  424 | `			 * queued behind a handler that threw is DROPPED, not deferred --` |
|       - |  425 | `			 * measured against the oracle, because keeping it looked like the` |
|       - |  426 | `			 * kinder answer and is not php's. */` |
|       3 |  427 | `			rcUnwind = rc;` |
|       3 |  428 | `			bUnwound = 1;` |
|     ! 0 |  429 | `		}` |
|      16 |  430 | `		rc = SXRET_OK;` |
|       - |  431 | `	}` |
|      20 |  432 | `	pS->bDraining = 0;` |
|       - |  433 | `	/* A delivery that landed DURING the drain is past the snapshot and raised` |
|       - |  434 | `	 * the flag from the C handler itself, so it needs nothing from here. */` |
|      20 |  435 | `	return rcUnwind;` |
|       3 |  436 | `}` |
|       - |  437 | `/*` |
|       - |  438 | ` * The executor's door (VmLoopFetch). The status a handler raised is already` |
|       - |  439 | ` * parked on the VM by PH7_VmCallUserFunction, and the fetch-point router that` |
|       - |  440 | ` * called this runs immediately after -- so nothing has to be returned here.` |
|       - |  441 | ` */` |
|       2 |  442 | `PH7_PRIVATE void PH7_PcntlDrainAsync(ph7_vm *pVm)` |
|       - |  443 | `{` |
|       2 |  444 | `	PcntlDrain(pVm);` |
|       2 |  445 | `}` |
|       - |  446 | `/*` |
|       - |  447 | ` * VM teardown. php's request shutdown puts every signal it took over back to` |
|       - |  448 | ` * SIG_DFL; without that a still-installed handler would fire into a VM whose` |
|       - |  449 | ` * allocator has been released.` |
|       - |  450 | ` */` |
|    6701 |  451 | `PH7_PRIVATE void PH7_PcntlVmRelease(ph7_vm *pVm)` |
|       - |  452 | `{` |
|    6701 |  453 | `	pcntl_state *pS = (pcntl_state *)pVm->pPcntl;` |
|       - |  454 | `	int i;` |
|    6701 |  455 | `	if( pS == 0 ){` |
|    6691 |  456 | `		return;` |
|       - |  457 | `	}` |
|       - |  458 | `	/* php's request shutdown puts EVERY signal its table names back to SIG_DFL,` |
|       - |  459 | `	 * not just the ones it installed a C handler for -- a SIG_IGN a script left` |
|       - |  460 | `	 * behind is undone too. */` |
|     518 |  461 | `	for( i = 1 ; i < PCNTL_NSIG ; ++i ){` |
|     508 |  462 | `		if( pS->aSet[i] ){` |
|      11 |  463 | `			PcntlInstall(i,(void *)SIG_DFL,1,0);` |
|      11 |  464 | `			PH7_MemObjRelease(&pS->aHandler[i]);` |
|       4 |  465 | `		}` |
|     124 |  466 | `	}` |
|      10 |  467 | `	pVm->pPcntl = 0;` |
|      10 |  468 | `	if( pPcntlVm == pVm ){` |
|      10 |  469 | `		pPcntlVm = 0;` |
|      10 |  470 | `		bPcntlAsyncFlag = 0;` |
|      10 |  471 | `		PH7_PcntlAsyncPending = 0;` |
|      10 |  472 | `		nPcntlHead = nPcntlTail = 0;` |
|       4 |  473 | `	}` |
|      10 |  474 | `	SyMemBackendFree(&pVm->sAllocator,pS);` |
|    3345 |  475 | `}` |
|       - |  476 |  |
|       - |  477 | `/* --- Screens ------------------------------------------------------------ */` |
|       - |  478 |  |
|       - |  479 | `/*` |
|       - |  480 | ` * php's signal-number screen, and it is not one screen: pcntl_signal() words` |
|       - |  481 | ` * its two bounds separately ("must be greater than or equal to 1" and "must be` |
|       - |  482 | ` * less than 65") where pcntl_signal_get_handler() words them as one ("must be` |
|       - |  483 | ` * between 1 and 64"). Reproduced as php has it -- the difference is visible.` |
|       - |  484 | ` */` |
|      40 |  485 | `static int PcntlSigArg(ph7_context *pCtx,ph7_value *pArg,int bRange,int *pSigno)` |
|       - |  486 | `{` |
|      40 |  487 | `	sxi64 iSig = ph7_value_to_int64(pArg);` |
|      40 |  488 | `	if( bRange ){` |
|       6 |  489 | `		if( iSig < 1 \|\| iSig >= PCNTL_NSIG ){` |
|       2 |  490 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  491 | `				"%s(): Argument #1 ($signal) must be between 1 and %d",` |
|     ! 0 |  492 | `				ph7_function_name(pCtx),PCNTL_NSIG - 1);` |
|       2 |  493 | `			return 0;` |
|       - |  494 | `		}` |
|     ! 0 |  495 | `	}else{` |
|      34 |  496 | `		if( iSig < 1 ){` |
|       2 |  497 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  498 | `				"%s(): Argument #1 ($signal) must be greater than or equal to 1",` |
|     ! 0 |  499 | `				ph7_function_name(pCtx));` |
|       2 |  500 | `			return 0;` |
|       - |  501 | `		}` |
|      32 |  502 | `		if( iSig >= PCNTL_NSIG ){` |
|       1 |  503 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  504 | `				"%s(): Argument #1 ($signal) must be less than %d",` |
|     ! 0 |  505 | `				ph7_function_name(pCtx),PCNTL_NSIG);` |
|       1 |  506 | `			return 0;` |
|       - |  507 | `		}` |
|       - |  508 | `	}` |
|      35 |  509 | `	*pSigno = (int)iSig;` |
|      35 |  510 | `	return 1;` |
|       5 |  511 | `}` |
|       - |  512 | `/*` |
|       - |  513 | ` * The errno tables php wrote by hand, one per call. Every one of them is an` |
|       - |  514 | ` * E_WARNING that answers false, and every one stores the errno first.` |
|       - |  515 | ` */` |
|       1 |  516 | `static void PcntlWarnErrno(ph7_context *pCtx,int iErr,const char *zWhy)` |
|       - |  517 | `{` |
|       1 |  518 | `	PcntlStoreErr(pCtx->pVm,iErr);` |
|       1 |  519 | `	if( zWhy ){` |
|       1 |  520 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Error %d: %s",iErr,zWhy);` |
|     ! 0 |  521 | `	}else{` |
|     ! 0 |  522 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Error %d",iErr);` |
|       - |  523 | `	}` |
|       1 |  524 | `}` |
|       - |  525 |  |
|       - |  526 | `/* --- fork, exec and the wait family ------------------------------------- */` |
|       - |  527 |  |
|       - |  528 | `/* int pcntl_fork() */` |
|      18 |  529 | `PH7_PRIVATE int PH7_builtin_pcntl_fork(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  530 | `{` |
|       - |  531 | `	pid_t id;` |
|       9 |  532 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      18 |  533 | `	id = fork();` |
|      26 |  534 | `	if( id == 0 ){` |
|       - |  535 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  536 | `		/* Only this thread survived, and it owns none of the recursive mutexes` |
|       - |  537 | `		 * the parent was holding -- see PH7_LibForkChild, without which a child` |
|       - |  538 | `		 * that merely calls exit() blocks on a futex for good. */` |
|      16 |  539 | `		PH7_LibForkChild();` |
|       - |  540 | `#endif` |
|       8 |  541 | `	}` |
|      33 |  542 | `	if( id == -1 ){` |
|     ! 0 |  543 | `		switch( errno ){` |
|     ! 0 |  544 | `		case EAGAIN:` |
|     ! 0 |  545 | `			PcntlWarnErrno(pCtx,errno,"Reached the maximum limit of number of processes");` |
|     ! 0 |  546 | `			break;` |
|     ! 0 |  547 | `		case ENOMEM:` |
|     ! 0 |  548 | `			PcntlWarnErrno(pCtx,errno,"Insufficient memory");` |
|     ! 0 |  549 | `			break;` |
|       - |  550 | `#ifdef ENOSYS` |
|     ! 0 |  551 | `		case ENOSYS:` |
|     ! 0 |  552 | `			PcntlWarnErrno(pCtx,errno,"Unimplemented");` |
|     ! 0 |  553 | `			break;` |
|       - |  554 | `#endif` |
|     ! 0 |  555 | `		default:` |
|     ! 0 |  556 | `			PcntlWarnErrno(pCtx,errno,0);` |
|     ! 0 |  557 | `			break;` |
|       - |  558 | `		}` |
|     ! 0 |  559 | `	}` |
|      34 |  560 | `	ph7_result_int64(pCtx,(ph7_int64)id);` |
|      34 |  561 | `	return PH7_OK;` |
|       - |  562 | `}` |
|       - |  563 | `/*` |
|       - |  564 | ` * php's seventeen resource-usage keys, in php's own order -- which is neither` |
|       - |  565 | ` * the struct's nor alphabetical, and which a program reading the array by` |
|       - |  566 | ` * position would see.` |
|       - |  567 | ` */` |
|       4 |  568 | `static void PcntlRusage(ph7_vm *pVm,ph7_value *pArray,struct rusage *pRu)` |
|       - |  569 | `{` |
|       4 |  570 | `	ph7_value *pVal = ph7_new_scalar(pVm);` |
|       4 |  571 | `	if( pVal == 0 ){` |
|     ! 0 |  572 | `		return;` |
|       - |  573 | `	}` |
|       - |  574 | `#define PCNTL_RU(field) \` |
|       - |  575 | `	ph7_value_int64(pVal,(ph7_int64)pRu->field); \` |
|       - |  576 | `	ph7_array_add_strkey_elem(pArray,#field,pVal)` |
|       4 |  577 | `	PCNTL_RU(ru_oublock);` |
|       4 |  578 | `	PCNTL_RU(ru_inblock);` |
|       4 |  579 | `	PCNTL_RU(ru_msgsnd);` |
|       4 |  580 | `	PCNTL_RU(ru_msgrcv);` |
|       4 |  581 | `	PCNTL_RU(ru_maxrss);` |
|       4 |  582 | `	PCNTL_RU(ru_ixrss);` |
|       4 |  583 | `	PCNTL_RU(ru_idrss);` |
|       4 |  584 | `	PCNTL_RU(ru_minflt);` |
|       4 |  585 | `	PCNTL_RU(ru_majflt);` |
|       4 |  586 | `	PCNTL_RU(ru_nsignals);` |
|       4 |  587 | `	PCNTL_RU(ru_nvcsw);` |
|       4 |  588 | `	PCNTL_RU(ru_nivcsw);` |
|       4 |  589 | `	PCNTL_RU(ru_nswap);` |
|       - |  590 | `#undef PCNTL_RU` |
|       4 |  591 | `	ph7_value_int64(pVal,(ph7_int64)pRu->ru_utime.tv_usec);` |
|       4 |  592 | `	ph7_array_add_strkey_elem(pArray,"ru_utime.tv_usec",pVal);` |
|       4 |  593 | `	ph7_value_int64(pVal,(ph7_int64)pRu->ru_utime.tv_sec);` |
|       4 |  594 | `	ph7_array_add_strkey_elem(pArray,"ru_utime.tv_sec",pVal);` |
|       4 |  595 | `	ph7_value_int64(pVal,(ph7_int64)pRu->ru_stime.tv_usec);` |
|       4 |  596 | `	ph7_array_add_strkey_elem(pArray,"ru_stime.tv_usec",pVal);` |
|       4 |  597 | `	ph7_value_int64(pVal,(ph7_int64)pRu->ru_stime.tv_sec);` |
|       4 |  598 | `	ph7_array_add_strkey_elem(pArray,"ru_stime.tv_sec",pVal);` |
|       4 |  599 | `	ph7_release_value(pVm,pVal);` |
|       2 |  600 | `}` |
|       - |  601 | `/* Write a rusage array back through a by-reference argument. */` |
|       4 |  602 | `static void PcntlStoreRusage(ph7_context *pCtx,ph7_value *pArg,struct rusage *pRu)` |
|       - |  603 | `{` |
|       4 |  604 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|       4 |  605 | `	if( pArray == 0 ){` |
|     ! 0 |  606 | `		return;` |
|       - |  607 | `	}` |
|       4 |  608 | `	PcntlRusage(pCtx->pVm,pArray,pRu);` |
|       4 |  609 | `	PH7_VmStoreArgByRef(pCtx->pVm,pArg,pArray);` |
|       2 |  610 | `}` |
|       - |  611 | `/*` |
|       - |  612 | ` * The shared body of pcntl_waitpid() and pcntl_wait(): the same call with the` |
|       - |  613 | ` * pid either given or -1, and the same by-reference status and (optional)` |
|       - |  614 | ` * resource usage. php assigns the status EVEN when the wait failed -- it is` |
|       - |  615 | `` * zero there, which is what a caller testing `$status !== -1` reads.`` |
|       - |  616 | ` */` |
|      23 |  617 | `static int PcntlWaitBody(ph7_context *pCtx,int nArg,ph7_value **apArg,int iStatusArg,pid_t pid)` |
|       - |  618 | `{` |
|      23 |  619 | `	int iFlagsArg = iStatusArg + 1;` |
|      23 |  620 | `	int iRuArg = iStatusArg + 2;` |
|      23 |  621 | `	int status = 0;` |
|      23 |  622 | `	int iFlags = 0;` |
|       - |  623 | `	pid_t id;` |
|       - |  624 | `	ph7_value *pVal;` |
|      23 |  625 | `	if( nArg > iFlagsArg ){` |
|       5 |  626 | `		iFlags = (int)ph7_value_to_int64(apArg[iFlagsArg]);` |
|       2 |  627 | `	}` |
|      23 |  628 | `	if( nArg > iRuArg ){` |
|       - |  629 | `		struct rusage sRu;` |
|       2 |  630 | `		SyZero(&sRu,sizeof(sRu));` |
|       2 |  631 | `		id = wait4(pid,&status,iFlags,&sRu);` |
|       2 |  632 | `		if( id < 0 ){` |
|     ! 0 |  633 | `			PcntlStoreErr(pCtx->pVm,errno);` |
|     ! 0 |  634 | `		}` |
|       2 |  635 | `		PcntlStoreRusage(pCtx,apArg[iRuArg],&sRu);` |
|       1 |  636 | `	}else{` |
|      21 |  637 | `		id = waitpid(pid,&status,iFlags);` |
|      21 |  638 | `		if( id < 0 ){` |
|       5 |  639 | `			PcntlStoreErr(pCtx->pVm,errno);` |
|       2 |  640 | `		}` |
|       - |  641 | `	}` |
|      23 |  642 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      23 |  643 | `	if( pVal ){` |
|      23 |  644 | `		ph7_value_int64(pVal,(ph7_int64)status);` |
|      23 |  645 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iStatusArg],pVal);` |
|      11 |  646 | `	}` |
|      23 |  647 | `	ph7_result_int64(pCtx,(ph7_int64)id);` |
|      23 |  648 | `	return PH7_OK;` |
|       - |  649 | `}` |
|       - |  650 | `/* int pcntl_waitpid(int $process_id, &$status, int $flags = 0, &$resource_usage = []) */` |
|      20 |  651 | `PH7_PRIVATE int PH7_builtin_pcntl_waitpid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  652 | `{` |
|      20 |  653 | `	return PcntlWaitBody(pCtx,nArg,apArg,1,(pid_t)ph7_value_to_int64(apArg[0]));` |
|       - |  654 | `}` |
|       - |  655 | `/* int pcntl_wait(&$status, int $flags = 0, &$resource_usage = []) */` |
|       3 |  656 | `PH7_PRIVATE int PH7_builtin_pcntl_wait(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  657 | `{` |
|       3 |  658 | `	return PcntlWaitBody(pCtx,nArg,apArg,0,(pid_t)-1);` |
|       - |  659 | `}` |
|       - |  660 | `/*` |
|       - |  661 | ` * bool pcntl_waitid(int $idtype = P_ALL, ?int $id = null, &$info = [],` |
|       - |  662 | ` *                   int $flags = WEXITED, &$resource_usage = [])` |
|       - |  663 | ` *  waitid() carries no usage of its own, so php answers the fifth argument` |
|       - |  664 | ` *  with getrusage(RUSAGE_CHILDREN) -- measured against the oracle rather than` |
|       - |  665 | ` *  assumed, because the two are not the same number for a single child.` |
|       - |  666 | ` */` |
|       4 |  667 | `PH7_PRIVATE int PH7_builtin_pcntl_waitid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  668 | `{` |
|       - |  669 | `	siginfo_t sInfo;` |
|       4 |  670 | `	int iType = 0, iFlags = WEXITED;` |
|       4 |  671 | `	id_t id = 0;` |
|       4 |  672 | `	SyZero(&sInfo,sizeof(sInfo));` |
|       4 |  673 | `	if( nArg > 0 ){` |
|       4 |  674 | `		iType = (int)ph7_value_to_int64(apArg[0]);` |
|       2 |  675 | `	}` |
|       4 |  676 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       4 |  677 | `		id = (id_t)ph7_value_to_int64(apArg[1]);` |
|       2 |  678 | `	}` |
|       4 |  679 | `	if( nArg > 3 ){` |
|       4 |  680 | `		iFlags = (int)ph7_value_to_int64(apArg[3]);` |
|       2 |  681 | `	}` |
|       4 |  682 | `	if( waitid((idtype_t)iType,id,&sInfo,iFlags) != 0 ){` |
|       2 |  683 | `		PcntlStoreErr(pCtx->pVm,errno);` |
|       2 |  684 | `		ph7_result_bool(pCtx,0);` |
|       2 |  685 | `		return PH7_OK;` |
|       - |  686 | `	}` |
|       2 |  687 | `	if( nArg > 2 ){` |
|       2 |  688 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|       2 |  689 | `		if( pArray ){` |
|       2 |  690 | `			PcntlSigInfoArray(pCtx->pVm,pArray,sInfo.si_signo,&sInfo);` |
|       2 |  691 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pArray);` |
|       1 |  692 | `		}` |
|       1 |  693 | `	}` |
|       2 |  694 | `	if( nArg > 4 ){` |
|       - |  695 | `		struct rusage sRu;` |
|       2 |  696 | `		SyZero(&sRu,sizeof(sRu));` |
|       2 |  697 | `		getrusage(RUSAGE_CHILDREN,&sRu);` |
|       2 |  698 | `		PcntlStoreRusage(pCtx,apArg[4],&sRu);` |
|       1 |  699 | `	}` |
|       2 |  700 | `	ph7_result_bool(pCtx,1);` |
|       2 |  701 | `	return PH7_OK;` |
|       2 |  702 | `}` |
|       - |  703 | `/*` |
|       - |  704 | ` * The seven status questions. php applies the C macro and hands back whatever` |
|       - |  705 | ` * it answers -- there is no screen for "this status is not that kind", so` |
|       - |  706 | ` * pcntl_wstopsig() of an EXITED status answers the exit code.` |
|       - |  707 | ` */` |
|       8 |  708 | `PH7_PRIVATE int PH7_builtin_pcntl_wifexited(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  709 | `{` |
|       8 |  710 | `	int status = (int)ph7_value_to_int64(apArg[0]);` |
|       3 |  711 | `	SXUNUSED(nArg);` |
|       8 |  712 | `	ph7_result_bool(pCtx,WIFEXITED(status) ? 1 : 0);` |
|       8 |  713 | `	return PH7_OK;` |
|       - |  714 | `}` |
|       1 |  715 | `PH7_PRIVATE int PH7_builtin_pcntl_wifstopped(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  716 | `{` |
|       1 |  717 | `	int status = (int)ph7_value_to_int64(apArg[0]);` |
|     ! 0 |  718 | `	SXUNUSED(nArg);` |
|       1 |  719 | `	ph7_result_bool(pCtx,WIFSTOPPED(status) ? 1 : 0);` |
|       1 |  720 | `	return PH7_OK;` |
|       - |  721 | `}` |
|       7 |  722 | `PH7_PRIVATE int PH7_builtin_pcntl_wifsignaled(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  723 | `{` |
|       7 |  724 | `	int status = (int)ph7_value_to_int64(apArg[0]);` |
|       2 |  725 | `	SXUNUSED(nArg);` |
|       7 |  726 | `	ph7_result_bool(pCtx,WIFSIGNALED(status) ? 1 : 0);` |
|       7 |  727 | `	return PH7_OK;` |
|       - |  728 | `}` |
|       1 |  729 | `PH7_PRIVATE int PH7_builtin_pcntl_wifcontinued(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  730 | `{` |
|       1 |  731 | `	int status = (int)ph7_value_to_int64(apArg[0]);` |
|     ! 0 |  732 | `	SXUNUSED(nArg);` |
|       - |  733 | `#ifdef WIFCONTINUED` |
|       1 |  734 | `	ph7_result_bool(pCtx,WIFCONTINUED(status) ? 1 : 0);` |
|       - |  735 | `#else` |
|       - |  736 | `	ph7_result_bool(pCtx,0);` |
|       - |  737 | `#endif` |
|       1 |  738 | `	return PH7_OK;` |
|       - |  739 | `}` |
|       8 |  740 | `PH7_PRIVATE int PH7_builtin_pcntl_wexitstatus(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  741 | `{` |
|       8 |  742 | `	int status = (int)ph7_value_to_int64(apArg[0]);` |
|       3 |  743 | `	SXUNUSED(nArg);` |
|       8 |  744 | `	ph7_result_int64(pCtx,(ph7_int64)WEXITSTATUS(status));` |
|       8 |  745 | `	return PH7_OK;` |
|       - |  746 | `}` |
|       4 |  747 | `PH7_PRIVATE int PH7_builtin_pcntl_wtermsig(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  748 | `{` |
|       4 |  749 | `	int status = (int)ph7_value_to_int64(apArg[0]);` |
|       1 |  750 | `	SXUNUSED(nArg);` |
|       4 |  751 | `	ph7_result_int64(pCtx,(ph7_int64)WTERMSIG(status));` |
|       4 |  752 | `	return PH7_OK;` |
|       - |  753 | `}` |
|       2 |  754 | `PH7_PRIVATE int PH7_builtin_pcntl_wstopsig(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  755 | `{` |
|       2 |  756 | `	int status = (int)ph7_value_to_int64(apArg[0]);` |
|     ! 0 |  757 | `	SXUNUSED(nArg);` |
|       2 |  758 | `	ph7_result_int64(pCtx,(ph7_int64)WSTOPSIG(status));` |
|       2 |  759 | `	return PH7_OK;` |
|       - |  760 | `}` |
|       - |  761 |  |
|       - |  762 | `/* --- pcntl_exec --------------------------------------------------------- */` |
|       - |  763 |  |
|       - |  764 | `/*` |
|       - |  765 | ` * Append one NUL-terminated string to the argument block and remember where it` |
|       - |  766 | ` * starts. The block is built first and the pointers taken afterwards, because` |
|       - |  767 | ` * every append can move it.` |
|       - |  768 | ` */` |
|      11 |  769 | `static int PcntlPushArg(SyBlob *pBuf,SySet *pOff,const char *zStr,int nStr)` |
|       - |  770 | `{` |
|      11 |  771 | `	sxu32 nAt = SyBlobLength(pBuf);` |
|      11 |  772 | `	if( SySetPut(pOff,(const void *)&nAt) != SXRET_OK ){` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|      11 |  775 | `	if( nStr > 0 && SyBlobAppend(pBuf,zStr,(sxu32)nStr) != SXRET_OK ){` |
|     ! 0 |  776 | `		return 0;` |
|       - |  777 | `	}` |
|      11 |  778 | `	return SyBlobAppend(pBuf,"\0",1) == SXRET_OK;` |
|     ! 0 |  779 | `}` |
|       - |  780 | `/*` |
|       - |  781 | ` * false pcntl_exec(string $path, array $args = [], array $env_vars = [])` |
|       - |  782 | ` *  php puts the PATH itself in argv[0] and the caller's $args after it, so a` |
|       - |  783 | ` *  program spelling its own argv[0] cannot. A null byte anywhere is a` |
|       - |  784 | ` *  ValueError with php's three different sentences -- one for the path, one for` |
|       - |  785 | ` *  an argument, and two for an environment entry's halves.` |
|       - |  786 | ` */` |
|       9 |  787 | `PH7_PRIVATE int PH7_builtin_pcntl_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  788 | `{` |
|       - |  789 | `	SyBlob sBuf, sEnvBuf;` |
|       - |  790 | `	SySet aOff, aEnvOff;` |
|       - |  791 | `	const char *zPath;` |
|       9 |  792 | `	int nPath = 0, rc = PH7_OK;` |
|       9 |  793 | `	char **apArgv = 0, **apEnvp = 0;` |
|       9 |  794 | `	sxu32 n, nArgv, nEnvp = 0;` |
|       9 |  795 | `	ph7_vm *pVm = pCtx->pVm;` |
|       9 |  796 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|       9 |  797 | `	if( nPath > 0 && SyByteFind(zPath,(sxu32)nPath,'\0',0) == SXRET_OK ){` |
|       1 |  798 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  799 | `			"%s(): Argument #1 ($path) must not contain any null bytes",` |
|     ! 0 |  800 | `			ph7_function_name(pCtx));` |
|       - |  801 | `	}` |
|       8 |  802 | `	SyBlobInit(&sBuf,&pVm->sAllocator);` |
|       8 |  803 | `	SyBlobInit(&sEnvBuf,&pVm->sAllocator);` |
|       8 |  804 | `	SySetInit(&aOff,&pVm->sAllocator,sizeof(sxu32));` |
|       8 |  805 | `	SySetInit(&aEnvOff,&pVm->sAllocator,sizeof(sxu32));` |
|       8 |  806 | `	if( !PcntlPushArg(&sBuf,&aOff,zPath,nPath) ){` |
|     ! 0 |  807 | `		goto Oom;` |
|       - |  808 | `	}` |
|       8 |  809 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|       7 |  810 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - |  811 | `		ph7_hashmap_node *pNode;` |
|       7 |  812 | `		pMap->pCur = pMap->pFirst;` |
|      10 |  813 | `		while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|       - |  814 | `			ph7_value sVal;` |
|       - |  815 | `			const char *zVal;` |
|       5 |  816 | `			int nVal = 0, bBad;` |
|       - |  817 | `			sxi32 rcSv;` |
|       5 |  818 | `			PH7_MemObjInit(pVm,&sVal);` |
|       5 |  819 | `			PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|       - |  820 | ``			/* php's `(string)` cast, not the silent embedder one: an array`` |
|       - |  821 | `` 			 * argument is `Array` behind an `Array to string conversion` `` |
|       - |  822 | `			 * warning (once per element), and an object with no __toString()` |
|       - |  823 | `			 * stops the call with php's Error. */` |
|       5 |  824 | `			rcSv = PH7_ValueToStringUV(pCtx,&sVal,&zVal,&nVal);` |
|       5 |  825 | `			if( rcSv != SXRET_OK ){` |
|       1 |  826 | `				PH7_MemObjRelease(&sVal);` |
|       1 |  827 | `				rc = rcSv;` |
|       2 |  828 | `				goto Done;` |
|       - |  829 | `			}` |
|       4 |  830 | `			bBad = (nVal > 0 && SyByteFind(zVal,(sxu32)nVal,'\0',0) == SXRET_OK);` |
|       4 |  831 | `			if( !bBad && !PcntlPushArg(&sBuf,&aOff,zVal,nVal) ){` |
|     ! 0 |  832 | `				PH7_MemObjRelease(&sVal);` |
|     ! 0 |  833 | `				goto Oom;` |
|       - |  834 | `			}` |
|       4 |  835 | `			PH7_MemObjRelease(&sVal);` |
|       4 |  836 | `			if( bBad ){` |
|       1 |  837 | `				rc = PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  838 | `					"%s(): Argument #2 ($args) individual argument must not contain null bytes",` |
|     ! 0 |  839 | `					ph7_function_name(pCtx));` |
|       1 |  840 | `				goto Done;` |
|       - |  841 | `			}` |
|       - |  842 | `		}` |
|     ! 0 |  843 | `	}` |
|       6 |  844 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|       3 |  845 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|       - |  846 | `		ph7_hashmap_node *pNode;` |
|       3 |  847 | `		pMap->pCur = pMap->pFirst;` |
|       3 |  848 | `		while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|       - |  849 | `			ph7_value sKey, sVal;` |
|       - |  850 | `			const char *zKey, *zVal;` |
|       3 |  851 | `			int nKey = 0, nVal = 0;` |
|       - |  852 | `			sxu32 nAt;` |
|       - |  853 | `			sxi32 rcSv;` |
|       3 |  854 | `			PH7_MemObjInit(pVm,&sKey);` |
|       3 |  855 | `			PH7_MemObjInit(pVm,&sVal);` |
|       3 |  856 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       3 |  857 | `			PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|       - |  858 | `			/* A hash KEY is already an int or a string, so nothing about it can` |
|       - |  859 | `			 * warn -- only the VALUE takes the user-visible cast. */` |
|       3 |  860 | `			zKey = ph7_value_to_string(&sKey,&nKey);` |
|       3 |  861 | `			if( nKey > 0 && SyByteFind(zKey,(sxu32)nKey,'\0',0) == SXRET_OK ){` |
|       1 |  862 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|       1 |  863 | `				rc = PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  864 | `					"%s(): Argument #3 ($env_vars) name for environment variable must not contain null bytes",` |
|     ! 0 |  865 | `					ph7_function_name(pCtx));` |
|       3 |  866 | `				goto Done;` |
|       - |  867 | `			}` |
|       2 |  868 | `			rcSv = PH7_ValueToStringUV(pCtx,&sVal,&zVal,&nVal);` |
|       2 |  869 | `			if( rcSv != SXRET_OK ){` |
|       1 |  870 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|       1 |  871 | `				rc = rcSv;` |
|       1 |  872 | `				goto Done;` |
|       - |  873 | `			}` |
|       1 |  874 | `			if( nVal > 0 && SyByteFind(zVal,(sxu32)nVal,'\0',0) == SXRET_OK ){` |
|       1 |  875 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|       1 |  876 | `				rc = PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  877 | `					"%s(): Argument #3 ($env_vars) value for environment variable must not contain null bytes",` |
|     ! 0 |  878 | `					ph7_function_name(pCtx));` |
|       1 |  879 | `				goto Done;` |
|       - |  880 | `			}` |
|     ! 0 |  881 | `			nAt = SyBlobLength(&sEnvBuf);` |
|     ! 0 |  882 | `			if( SySetPut(&aEnvOff,(const void *)&nAt) != SXRET_OK` |
|     ! 0 |  883 | `			 \|\| (nKey > 0 && SyBlobAppend(&sEnvBuf,zKey,(sxu32)nKey) != SXRET_OK)` |
|     ! 0 |  884 | `			 \|\| SyBlobAppend(&sEnvBuf,"=",1) != SXRET_OK` |
|     ! 0 |  885 | `			 \|\| (nVal > 0 && SyBlobAppend(&sEnvBuf,zVal,(sxu32)nVal) != SXRET_OK)` |
|     ! 0 |  886 | `			 \|\| SyBlobAppend(&sEnvBuf,"\0",1) != SXRET_OK ){` |
|     ! 0 |  887 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|     ! 0 |  888 | `				goto Oom;` |
|       - |  889 | `			}` |
|     ! 0 |  890 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 |  891 | `			PH7_MemObjRelease(&sVal);` |
|       - |  892 | `		}` |
|     ! 0 |  893 | `		nEnvp = SySetUsed(&aEnvOff);` |
|     ! 0 |  894 | `	}` |
|       3 |  895 | `	nArgv = SySetUsed(&aOff);` |
|       3 |  896 | `	apArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,(nArgv + 1) * sizeof(char *));` |
|       3 |  897 | `	if( apArgv == 0 ){` |
|     ! 0 |  898 | `		goto Oom;` |
|       - |  899 | `	}` |
|       9 |  900 | `	for( n = 0 ; n < nArgv ; ++n ){` |
|       6 |  901 | `		apArgv[n] = (char *)SyBlobData(&sBuf) + ((sxu32 *)SySetBasePtr(&aOff))[n];` |
|     ! 0 |  902 | `	}` |
|       3 |  903 | `	apArgv[nArgv] = 0;` |
|       3 |  904 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     ! 0 |  905 | `		apEnvp = (char **)SyMemBackendAlloc(&pVm->sAllocator,(nEnvp + 1) * sizeof(char *));` |
|     ! 0 |  906 | `		if( apEnvp == 0 ){` |
|     ! 0 |  907 | `			goto Oom;` |
|       - |  908 | `		}` |
|     ! 0 |  909 | `		for( n = 0 ; n < nEnvp ; ++n ){` |
|     ! 0 |  910 | `			apEnvp[n] = (char *)SyBlobData(&sEnvBuf) + ((sxu32 *)SySetBasePtr(&aEnvOff))[n];` |
|     ! 0 |  911 | `		}` |
|     ! 0 |  912 | `		apEnvp[nEnvp] = 0;` |
|     ! 0 |  913 | `	}` |
|       - |  914 | `	/* Anything this program still owns is about to stop existing, so nothing` |
|       - |  915 | `	 * below the exec has to be tidy -- but the exec can FAIL, and then it does. */` |
|       3 |  916 | `	if( apEnvp ){` |
|     ! 0 |  917 | `		execve(apArgv[0],apArgv,apEnvp);` |
|     ! 0 |  918 | `	}else{` |
|       3 |  919 | `		execv(apArgv[0],apArgv);` |
|       - |  920 | `	}` |
|       3 |  921 | `	PcntlStoreErr(pVm,errno);` |
|       6 |  922 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       3 |  923 | `		"Error has occurred: (errno %d) %s",errno,strerror(errno));` |
|       3 |  924 | `	ph7_result_bool(pCtx,0);` |
|       3 |  925 | `	goto Done;` |
|     ! 0 |  926 | `Oom:` |
|     ! 0 |  927 | `	ph7_result_bool(pCtx,0);` |
|       8 |  928 | `Done:` |
|       8 |  929 | `	if( apArgv ){ SyMemBackendFree(&pVm->sAllocator,apArgv); }` |
|       8 |  930 | `	if( apEnvp ){ SyMemBackendFree(&pVm->sAllocator,apEnvp); }` |
|       8 |  931 | `	SyBlobRelease(&sBuf);` |
|       8 |  932 | `	SyBlobRelease(&sEnvBuf);` |
|       8 |  933 | `	SySetRelease(&aOff);` |
|       8 |  934 | `	SySetRelease(&aEnvOff);` |
|       8 |  935 | `	return rc;` |
|     ! 0 |  936 | `}` |
|       - |  937 |  |
|       - |  938 | `/* --- The signal surface -------------------------------------------------- */` |
|       - |  939 |  |
|       - |  940 | `/*` |
|       - |  941 | ` * bool pcntl_signal(int $signal, callable\|int $handler, bool $restart_syscalls = true)` |
|       - |  942 | ` *  An INTEGER handler may only be SIG_DFL or SIG_IGN, and anything that is` |
|       - |  943 | ` *  neither an integer nor a callable is a TypeError naming the union. A` |
|       - |  944 | ` *  disposition sigaction() refuses -- SIGKILL, SIGSTOP -- is php's E_ERROR` |
|       - |  945 | ` *  rather than a warning, and it is not catchable there either.` |
|       - |  946 | ` */` |
|      34 |  947 | `PH7_PRIVATE int PH7_builtin_pcntl_signal(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  948 | `{` |
|       - |  949 | `	pcntl_state *pS;` |
|      34 |  950 | `	int signo = 0, bRestart = 1;` |
|       - |  951 | `	char zGiven[64];` |
|      34 |  952 | `	if( !PcntlSigArg(pCtx,apArg[0],0,&signo) ){` |
|       3 |  953 | `		return PH7_OK;` |
|       - |  954 | `	}` |
|      31 |  955 | `	if( nArg > 2 ){` |
|       4 |  956 | `		bRestart = ph7_value_to_bool(apArg[2]);` |
|       2 |  957 | `	}` |
|      31 |  958 | `	pS = PcntlState(pCtx->pVm,1);` |
|      31 |  959 | `	if( pS == 0 ){` |
|     ! 0 |  960 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  961 | `		return PH7_OK;` |
|       - |  962 | `	}` |
|      31 |  963 | `	if( ph7_value_is_int(apArg[1]) ){` |
|      12 |  964 | `		sxi64 iDisp = ph7_value_to_int64(apArg[1]);` |
|      12 |  965 | `		if( iDisp != (sxi64)(sxptr)SIG_DFL && iDisp != (sxi64)(sxptr)SIG_IGN ){` |
|       1 |  966 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  967 | `				"%s(): Argument #2 ($handler) must be either SIG_DFL or SIG_IGN "` |
|     ! 0 |  968 | `				"when an integer value is given",ph7_function_name(pCtx));` |
|       - |  969 | `		}` |
|      11 |  970 | `		if( PcntlInstall(signo,(void *)(sxptr)iDisp,bRestart,0) != 0 ){` |
|     ! 0 |  971 | `			PcntlStoreErr(pCtx->pVm,errno);` |
|     ! 0 |  972 | `			PH7_VmSignalInstallFatal(pCtx->pVm,signo);` |
|     ! 0 |  973 | `			return PH7_ABORT;` |
|       - |  974 | `		}` |
|      11 |  975 | `		PH7_MemObjRelease(&pS->aHandler[signo]);` |
|      11 |  976 | `		PH7_MemObjStore(apArg[1],&pS->aHandler[signo]);` |
|      11 |  977 | `		pS->aSet[signo] = 1;` |
|      11 |  978 | `		ph7_result_bool(pCtx,1);` |
|      11 |  979 | `		return PH7_OK;` |
|       - |  980 | `	}` |
|      19 |  981 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       - |  982 | `		/* php stores EINVAL BEFORE it throws here -- a refusal that leaves a` |
|       - |  983 | `		 * remembered errno behind, which most of the other screens do not. */` |
|       5 |  984 | `		PcntlStoreErr(pCtx->pVm,EINVAL);` |
|       5 |  985 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  986 | `			"%s(): Argument #2 ($handler) must be of type callable\|int, %s given",` |
|       5 |  987 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|       - |  988 | `	}` |
|      14 |  989 | `	PH7_MemObjRelease(&pS->aHandler[signo]);` |
|      14 |  990 | `	PH7_MemObjStore(apArg[1],&pS->aHandler[signo]);` |
|      14 |  991 | `	pS->aSet[signo] = 1;` |
|       - |  992 | `	/* Every other signal is blocked for the duration of the C handler (php's` |
|       - |  993 | `	 * mask_all), so the ring can never be re-entered mid-store. */` |
|      14 |  994 | `	if( PcntlInstall(signo,(void *)PcntlSigHandler,bRestart,1) != 0 ){` |
|     ! 0 |  995 | `		PcntlStoreErr(pCtx->pVm,errno);` |
|     ! 0 |  996 | `		PH7_VmSignalInstallFatal(pCtx->pVm,signo);` |
|     ! 0 |  997 | `		return PH7_ABORT;` |
|       - |  998 | `	}` |
|      14 |  999 | `	ph7_result_bool(pCtx,1);` |
|      14 | 1000 | `	return PH7_OK;` |
|       5 | 1001 | `}` |
|       - | 1002 | `/*` |
|       - | 1003 | ` * callable\|int pcntl_signal_get_handler(int $signal)` |
|       - | 1004 | ` *  A signal nobody has spoken for answers SIG_DFL -- the same 0 a signal` |
|       - | 1005 | ` *  explicitly set to SIG_DFL answers, which is why monolog can save and restore` |
|       - | 1006 | ` *  a disposition without knowing which of the two it had.` |
|       - | 1007 | ` */` |
|       6 | 1008 | `PH7_PRIVATE int PH7_builtin_pcntl_signal_get_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1009 | `{` |
|       - | 1010 | `	pcntl_state *pS;` |
|       6 | 1011 | `	int signo = 0;` |
|     ! 0 | 1012 | `	SXUNUSED(nArg);` |
|       6 | 1013 | `	if( !PcntlSigArg(pCtx,apArg[0],1,&signo) ){` |
|       2 | 1014 | `		return PH7_OK;` |
|       - | 1015 | `	}` |
|       4 | 1016 | `	pS = PcntlState(pCtx->pVm,0);` |
|       4 | 1017 | `	if( pS == 0 \|\| !pS->aSet[signo] ){` |
|       1 | 1018 | `		ph7_result_int64(pCtx,(ph7_int64)(sxptr)SIG_DFL);` |
|       1 | 1019 | `		return PH7_OK;` |
|       - | 1020 | `	}` |
|       3 | 1021 | `	ph7_result_value(pCtx,&pS->aHandler[signo]);` |
|       3 | 1022 | `	return PH7_OK;` |
|     ! 0 | 1023 | `}` |
|       - | 1024 | `/* true pcntl_signal_dispatch() */` |
|      20 | 1025 | `PH7_PRIVATE int PH7_builtin_pcntl_signal_dispatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1026 | `{` |
|       - | 1027 | `	sxi32 rc;` |
|       3 | 1028 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      20 | 1029 | `	rc = PcntlDrain(pCtx->pVm);` |
|      20 | 1030 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       2 | 1031 | `		return rc;` |
|       - | 1032 | `	}` |
|      18 | 1033 | `	ph7_result_bool(pCtx,1);` |
|      18 | 1034 | `	return PH7_OK;` |
|       3 | 1035 | `}` |
|       - | 1036 | `/*` |
|       - | 1037 | ` * bool pcntl_async_signals(?bool $enable = null)` |
|       - | 1038 | ` *  Answers the OLD setting when it changes one and the CURRENT one when asked` |
|       - | 1039 | ` *  with null -- which is how a test saves what it found and puts it back.` |
|       - | 1040 | ` */` |
|      12 | 1041 | `PH7_PRIVATE int PH7_builtin_pcntl_async_signals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1042 | `{` |
|      12 | 1043 | `	pcntl_state *pS = PcntlState(pCtx->pVm,1);` |
|       - | 1044 | `	int bOld;` |
|      12 | 1045 | `	if( pS == 0 ){` |
|     ! 0 | 1046 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1047 | `		return PH7_OK;` |
|       - | 1048 | `	}` |
|      12 | 1049 | `	bOld = pS->bAsync;` |
|      12 | 1050 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|       9 | 1051 | `		pS->bAsync = ph7_value_to_bool(apArg[0]) ? 1 : 0;` |
|       9 | 1052 | `		bPcntlAsyncFlag = (sig_atomic_t)pS->bAsync;` |
|       - | 1053 | `		/* Turning async ON does NOT flush what is already queued: php raises` |
|       - | 1054 | `		 * its interrupt from the SIGNAL HANDLER and nowhere else, so a delivery` |
|       - | 1055 | `		 * that arrived while async was off waits for the next` |
|       - | 1056 | `		 * pcntl_signal_dispatch() or the next signal. Measured, not assumed --` |
|       - | 1057 | `		 * this is the one place the first cut of this extension invented a` |
|       - | 1058 | `		 * behaviour php does not have. */` |
|     ! 0 | 1059 | `	}` |
|      12 | 1060 | `	ph7_result_bool(pCtx,bOld);` |
|      12 | 1061 | `	return PH7_OK;` |
|     ! 0 | 1062 | `}` |
|       - | 1063 | `/*` |
|       - | 1064 | ` * Read an array of signal numbers into a sigset_t. php screens each element` |
|       - | 1065 | ` * itself -- an int outside 1..NSIG-1 is a ValueError and a non-int is a` |
|       - | 1066 | ` * TypeError, both naming the ARRAY's parameter rather than the element.` |
|       - | 1067 | ` */` |
|      23 | 1068 | `static int PcntlSigSet(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zParam,sigset_t *pSet)` |
|       - | 1069 | `{` |
|       - | 1070 | `	ph7_hashmap *pMap;` |
|       - | 1071 | `	ph7_hashmap_node *pNode;` |
|       - | 1072 | `	char zGiven[64];` |
|      23 | 1073 | `	sigemptyset(pSet);` |
|      23 | 1074 | `	if( !ph7_value_is_array(pArg) ){` |
|     ! 0 | 1075 | `		return 1;` |
|       - | 1076 | `	}` |
|      23 | 1077 | `	pMap = (ph7_hashmap *)pArg->x.pOther;` |
|      23 | 1078 | `	pMap->pCur = pMap->pFirst;` |
|      42 | 1079 | `	while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|       - | 1080 | `		ph7_value sVal;` |
|       - | 1081 | `		sxi64 iSig;` |
|      21 | 1082 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|      21 | 1083 | `		PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|      21 | 1084 | `		if( !ph7_value_is_int(&sVal) ){` |
|       1 | 1085 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1086 | `				"%s(): Argument #%d ($%s) signals must be of type int, %s given",` |
|     ! 0 | 1087 | `				ph7_function_name(pCtx),iPos,zParam,` |
|     ! 0 | 1088 | `				VmValueGivenName(&sVal,zGiven,sizeof(zGiven)));` |
|       1 | 1089 | `			PH7_MemObjRelease(&sVal);` |
|       2 | 1090 | `			return 0;` |
|       - | 1091 | `		}` |
|      20 | 1092 | `		iSig = ph7_value_to_int64(&sVal);` |
|      20 | 1093 | `		PH7_MemObjRelease(&sVal);` |
|      20 | 1094 | `		if( iSig < 1 \|\| iSig >= PCNTL_NSIG ){` |
|       1 | 1095 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1096 | `				"%s(): Argument #%d ($%s) signals must be between 1 and %d",` |
|     ! 0 | 1097 | `				ph7_function_name(pCtx),iPos,zParam,PCNTL_NSIG - 1);` |
|       1 | 1098 | `			return 0;` |
|       - | 1099 | `		}` |
|      19 | 1100 | `		sigaddset(pSet,(int)iSig);` |
|       - | 1101 | `	}` |
|      21 | 1102 | `	return 1;` |
|     ! 0 | 1103 | `}` |
|       - | 1104 | `/* The signals a set holds, as php's list of numbers in ascending order. */` |
|       5 | 1105 | `static void PcntlSetToArray(ph7_context *pCtx,ph7_value *pArg,const sigset_t *pSet)` |
|       - | 1106 | `{` |
|       5 | 1107 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|       5 | 1108 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|       - | 1109 | `	int i;` |
|       5 | 1110 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|     ! 0 | 1111 | `		return;` |
|       - | 1112 | `	}` |
|     325 | 1113 | `	for( i = 1 ; i < PCNTL_NSIG ; ++i ){` |
|     320 | 1114 | `		if( sigismember(pSet,i) != 1 ){` |
|     318 | 1115 | `			continue;` |
|       - | 1116 | `		}` |
|       2 | 1117 | `		ph7_value_int64(pVal,(ph7_int64)i);` |
|       2 | 1118 | `		ph7_array_add_elem(pArray,0,pVal);` |
|     ! 0 | 1119 | `	}` |
|       5 | 1120 | `	PH7_VmStoreArgByRef(pCtx->pVm,pArg,pArray);` |
|     ! 0 | 1121 | `}` |
|       - | 1122 | `/* bool pcntl_sigprocmask(int $mode, array $signals, &$old_signals = null) */` |
|      17 | 1123 | `PH7_PRIVATE int PH7_builtin_pcntl_sigprocmask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1124 | `{` |
|       - | 1125 | `	sigset_t sSet, sOld;` |
|      17 | 1126 | `	int iMode = (int)ph7_value_to_int64(apArg[0]);` |
|     ! 0 | 1127 | `	SXUNUSED(nArg);` |
|      17 | 1128 | `	if( iMode != SIG_BLOCK && iMode != SIG_UNBLOCK && iMode != SIG_SETMASK ){` |
|       1 | 1129 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1130 | `			"%s(): Argument #1 ($mode) must be one of SIG_BLOCK, SIG_UNBLOCK, or SIG_SETMASK",` |
|     ! 0 | 1131 | `			ph7_function_name(pCtx));` |
|       - | 1132 | `	}` |
|      16 | 1133 | `	if( !PcntlSigSet(pCtx,apArg[1],2,"signals",&sSet) ){` |
|       2 | 1134 | `		return PH7_OK;` |
|       - | 1135 | `	}` |
|      14 | 1136 | `	sigemptyset(&sOld);` |
|      14 | 1137 | `	if( sigprocmask(iMode,&sSet,&sOld) != 0 ){` |
|     ! 0 | 1138 | `		PcntlStoreErr(pCtx->pVm,errno);` |
|     ! 0 | 1139 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Error %d",errno);` |
|     ! 0 | 1140 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1141 | `		return PH7_OK;` |
|       - | 1142 | `	}` |
|      14 | 1143 | `	if( nArg > 2 ){` |
|       5 | 1144 | `		PcntlSetToArray(pCtx,apArg[2],&sOld);` |
|     ! 0 | 1145 | `	}` |
|      14 | 1146 | `	ph7_result_bool(pCtx,1);` |
|      14 | 1147 | `	return PH7_OK;` |
|     ! 0 | 1148 | `}` |
|       - | 1149 | `#ifndef __APPLE__` |
|       - | 1150 | `/*` |
|       - | 1151 | ` * The shared body of pcntl_sigwaitinfo() and pcntl_sigtimedwait(), which php` |
|       - | 1152 | ` * builds only where the system has them: macOS has neither. php stores` |
|       - | 1153 | ` * an errno for every failure EXCEPT the timed wait's own EAGAIN, which is that` |
|       - | 1154 | ` * call saying "nothing arrived" rather than failing.` |
|       - | 1155 | ` */` |
|       7 | 1156 | `static int PcntlSigWait(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTimed)` |
|       - | 1157 | `{` |
|       - | 1158 | `	sigset_t sSet;` |
|       - | 1159 | `	siginfo_t sInfo;` |
|       - | 1160 | `	int iSig;` |
|       7 | 1161 | `	if( !PcntlSigSet(pCtx,apArg[0],1,"signals",&sSet) ){` |
|     ! 0 | 1162 | `		return PH7_OK;` |
|       - | 1163 | `	}` |
|       7 | 1164 | `	SyZero(&sInfo,sizeof(sInfo));` |
|       7 | 1165 | `	if( bTimed ){` |
|       - | 1166 | `		struct timespec sTs;` |
|       6 | 1167 | `		sxi64 iSec = 0, iNsec = 0;` |
|       6 | 1168 | `		if( nArg > 2 ){` |
|       6 | 1169 | `			iSec = ph7_value_to_int64(apArg[2]);` |
|       - | 1170 | `		}` |
|       6 | 1171 | `		if( nArg > 3 ){` |
|       5 | 1172 | `			iNsec = ph7_value_to_int64(apArg[3]);` |
|       - | 1173 | `		}` |
|       6 | 1174 | `		if( iSec < 0 ){` |
|       4 | 1175 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1176 | `				"%s(): Argument #3 ($seconds) must be greater than or equal to 0",` |
|       - | 1177 | `				ph7_function_name(pCtx));` |
|       - | 1178 | `		}` |
|       5 | 1179 | `		if( iNsec < 0 \|\| iNsec > 1000000000 ){` |
|       2 | 1180 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1181 | `				"%s(): Argument #4 ($nanoseconds) must be between 0 and 1e9",` |
|       - | 1182 | `				ph7_function_name(pCtx));` |
|       - | 1183 | `		}` |
|       3 | 1184 | `		if( iSec == 0 && iNsec == 0 ){` |
|       1 | 1185 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1186 | `				"%s(): At least one of argument #3 ($seconds) or argument #4 "` |
|       - | 1187 | `				"($nanoseconds) must be greater than 0",ph7_function_name(pCtx));` |
|       - | 1188 | `		}` |
|       2 | 1189 | `		sTs.tv_sec = (time_t)iSec;` |
|       2 | 1190 | `		sTs.tv_nsec = (long)iNsec;` |
|       2 | 1191 | `		iSig = sigtimedwait(&sSet,&sInfo,&sTs);` |
|       - | 1192 | `	}else{` |
|       1 | 1193 | `		iSig = sigwaitinfo(&sSet,&sInfo);` |
|       - | 1194 | `	}` |
|       3 | 1195 | `	if( iSig < 0 ){` |
|       1 | 1196 | `		if( errno != EAGAIN ){` |
|     ! 0 | 1197 | `			PcntlStoreErr(pCtx->pVm,errno);` |
|       - | 1198 | `		}` |
|       1 | 1199 | `		ph7_result_bool(pCtx,0);` |
|       1 | 1200 | `		return PH7_OK;` |
|       - | 1201 | `	}` |
|       2 | 1202 | `	if( nArg > 1 ){` |
|       2 | 1203 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|       2 | 1204 | `		if( pArray ){` |
|       2 | 1205 | `			PcntlSigInfoArray(pCtx->pVm,pArray,iSig,&sInfo);` |
|       2 | 1206 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pArray);` |
|       - | 1207 | `		}` |
|       - | 1208 | `	}` |
|       2 | 1209 | `	ph7_result_int64(pCtx,(ph7_int64)iSig);` |
|       2 | 1210 | `	return PH7_OK;` |
|       - | 1211 | `}` |
|       - | 1212 | `/* int\|false pcntl_sigwaitinfo(array $signals, &$info = []) */` |
|       1 | 1213 | `PH7_PRIVATE int PH7_builtin_pcntl_sigwaitinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1214 | `{` |
|       1 | 1215 | `	return PcntlSigWait(pCtx,nArg,apArg,0);` |
|       - | 1216 | `}` |
|       - | 1217 | `/* int\|false pcntl_sigtimedwait(array $signals, &$info = [], int $seconds = 0, int $nanoseconds = 0) */` |
|       6 | 1218 | `PH7_PRIVATE int PH7_builtin_pcntl_sigtimedwait(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1219 | `{` |
|       6 | 1220 | `	return PcntlSigWait(pCtx,nArg,apArg,1);` |
|       - | 1221 | `}` |
|       - | 1222 | `#endif /* __APPLE__ */` |
|       - | 1223 | `/*` |
|       - | 1224 | ` * int pcntl_alarm(int $seconds)` |
|       - | 1225 | ` *  Answers how many seconds were left on the PREVIOUS alarm, which is what a` |
|       - | 1226 | ` *  program cancelling one reads back.` |
|       - | 1227 | ` */` |
|       4 | 1228 | `PH7_PRIVATE int PH7_builtin_pcntl_alarm(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1229 | `{` |
|     ! 0 | 1230 | `	SXUNUSED(nArg);` |
|       4 | 1231 | `	ph7_result_int64(pCtx,(ph7_int64)alarm((unsigned int)ph7_value_to_int64(apArg[0])));` |
|       4 | 1232 | `	return PH7_OK;` |
|       - | 1233 | `}` |
|       - | 1234 |  |
|       - | 1235 | `/* --- Priority ------------------------------------------------------------ */` |
|       - | 1236 |  |
|       - | 1237 | `/* php's three-way $mode screen, shared by the getter and the setter. */` |
|       2 | 1238 | `static int PcntlPrioMode(ph7_context *pCtx,ph7_value *pArg,int iPos,int *pMode)` |
|       - | 1239 | `{` |
|       2 | 1240 | `	sxi64 iMode = ph7_value_to_int64(pArg);` |
|       2 | 1241 | `	if( iMode != PRIO_PGRP && iMode != PRIO_USER && iMode != PRIO_PROCESS ){` |
|       - | 1242 | `		/* php remembers EINVAL for this one too, before the ValueError. */` |
|       2 | 1243 | `		PcntlStoreErr(pCtx->pVm,EINVAL);` |
|       2 | 1244 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1245 | `			"%s(): Argument #%d ($mode) must be one of PRIO_PGRP, PRIO_USER, or PRIO_PROCESS",` |
|     ! 0 | 1246 | `			ph7_function_name(pCtx),iPos);` |
|       2 | 1247 | `		return 0;` |
|       - | 1248 | `	}` |
|     ! 0 | 1249 | `	*pMode = (int)iMode;` |
|     ! 0 | 1250 | `	return 1;` |
|     ! 0 | 1251 | `}` |
|       - | 1252 | `/* int\|false pcntl_getpriority(?int $process_id = null, int $mode = PRIO_PROCESS) */` |
|       6 | 1253 | `PH7_PRIVATE int PH7_builtin_pcntl_getpriority(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1254 | `{` |
|       6 | 1255 | `	int iMode = PRIO_PROCESS, iPri;` |
|       6 | 1256 | `	id_t id = 0;` |
|       6 | 1257 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|       2 | 1258 | `		id = (id_t)ph7_value_to_int64(apArg[0]);` |
|     ! 0 | 1259 | `	}` |
|       6 | 1260 | `	if( nArg > 1 && !PcntlPrioMode(pCtx,apArg[1],2,&iMode) ){` |
|       1 | 1261 | `		return PH7_OK;` |
|       - | 1262 | `	}` |
|       - | 1263 | `	/* getpriority() answers -1 for a real priority too, so errno is the only` |
|       - | 1264 | `	 * way to tell a failure from a nice value of -1. */` |
|       5 | 1265 | `	errno = 0;` |
|       5 | 1266 | `	iPri = getpriority(iMode,id);` |
|       5 | 1267 | `	if( errno != 0 ){` |
|       1 | 1268 | `		if( errno == ESRCH ){` |
|       1 | 1269 | `			PcntlWarnErrno(pCtx,errno,"No process was located using the given parameters");` |
|     ! 0 | 1270 | `		}else{` |
|     ! 0 | 1271 | `			PcntlStoreErr(pCtx->pVm,errno);` |
|     ! 0 | 1272 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 1273 | `				"Unknown error %d has occurred",errno);` |
|       - | 1274 | `		}` |
|       1 | 1275 | `		ph7_result_bool(pCtx,0);` |
|       1 | 1276 | `		return PH7_OK;` |
|       - | 1277 | `	}` |
|       4 | 1278 | `	ph7_result_int64(pCtx,(ph7_int64)iPri);` |
|       4 | 1279 | `	return PH7_OK;` |
|     ! 0 | 1280 | `}` |
|       - | 1281 | `/* bool pcntl_setpriority(int $priority, ?int $process_id = null, int $mode = PRIO_PROCESS) */` |
|       2 | 1282 | `PH7_PRIVATE int PH7_builtin_pcntl_setpriority(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1283 | `{` |
|       2 | 1284 | `	int iMode = PRIO_PROCESS;` |
|       2 | 1285 | `	int iPri = (int)ph7_value_to_int64(apArg[0]);` |
|       2 | 1286 | `	id_t id = 0;` |
|       2 | 1287 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 1288 | `		id = (id_t)ph7_value_to_int64(apArg[1]);` |
|     ! 0 | 1289 | `	}` |
|       2 | 1290 | `	if( nArg > 2 && !PcntlPrioMode(pCtx,apArg[2],3,&iMode) ){` |
|       1 | 1291 | `		return PH7_OK;` |
|       - | 1292 | `	}` |
|       1 | 1293 | `	if( setpriority(iMode,id,iPri) != 0 ){` |
|     ! 0 | 1294 | `		switch( errno ){` |
|     ! 0 | 1295 | `		case ESRCH:` |
|     ! 0 | 1296 | `			PcntlWarnErrno(pCtx,errno,"No process was located using the given parameters");` |
|     ! 0 | 1297 | `			break;` |
|     ! 0 | 1298 | `		case EPERM:` |
|     ! 0 | 1299 | `			PcntlWarnErrno(pCtx,errno,` |
|       - | 1300 | `				"A process was located, but neither its effective nor real user ID "` |
|       - | 1301 | `				"matched the effective user ID of the caller");` |
|     ! 0 | 1302 | `			break;` |
|     ! 0 | 1303 | `		case EACCES:` |
|     ! 0 | 1304 | `			PcntlWarnErrno(pCtx,errno,` |
|       - | 1305 | `				"Only a super user may attempt to increase the process priority");` |
|     ! 0 | 1306 | `			break;` |
|     ! 0 | 1307 | `		default:` |
|     ! 0 | 1308 | `			PcntlStoreErr(pCtx->pVm,errno);` |
|     ! 0 | 1309 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 1310 | `				"Unknown error %d has occurred",errno);` |
|     ! 0 | 1311 | `			break;` |
|       - | 1312 | `		}` |
|     ! 0 | 1313 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1314 | `		return PH7_OK;` |
|       - | 1315 | `	}` |
|       1 | 1316 | `	ph7_result_bool(pCtx,1);` |
|       1 | 1317 | `	return PH7_OK;` |
|     ! 0 | 1318 | `}` |
|       - | 1319 |  |
|       - | 1320 | `/* --- The remembered errno, read back ------------------------------------- */` |
|       - | 1321 |  |
|       - | 1322 | `/* int pcntl_get_last_error() / int pcntl_errno() */` |
|      13 | 1323 | `PH7_PRIVATE int PH7_builtin_pcntl_get_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1324 | `{` |
|      13 | 1325 | `	pcntl_state *pS = PcntlState(pCtx->pVm,0);` |
|       2 | 1326 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      13 | 1327 | `	ph7_result_int64(pCtx,(ph7_int64)(pS ? pS->iLastError : 0));` |
|      13 | 1328 | `	return PH7_OK;` |
|       - | 1329 | `}` |
|       - | 1330 | `/* string pcntl_strerror(int $error_code) */` |
|       2 | 1331 | `PH7_PRIVATE int PH7_builtin_pcntl_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1332 | `{` |
|       2 | 1333 | `	const char *zMsg = strerror((int)ph7_value_to_int64(apArg[0]));` |
|     ! 0 | 1334 | `	SXUNUSED(nArg);` |
|       2 | 1335 | `	ph7_result_string(pCtx,zMsg ? zMsg : "",-1);` |
|       2 | 1336 | `	return PH7_OK;` |
|       - | 1337 | `}` |
|       - | 1338 |  |
|       - | 1339 | `/* --- The Linux-only half -------------------------------------------------- */` |
|       - | 1340 |  |
|       - | 1341 | `#ifdef __linux__` |
|       - | 1342 | `/* bool pcntl_unshare(int $flags) */` |
|     ! 0 | 1343 | `PH7_PRIVATE int PH7_builtin_pcntl_unshare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1344 | `{` |
|     ! 0 | 1345 | `	int iFlags = (int)ph7_value_to_int64(apArg[0]);` |
|       - | 1346 | `	SXUNUSED(nArg);` |
|     ! 0 | 1347 | `	if( unshare(iFlags) != 0 ){` |
|     ! 0 | 1348 | `		PcntlStoreErr(pCtx->pVm,errno);` |
|     ! 0 | 1349 | `		switch( errno ){` |
|     ! 0 | 1350 | `		case EINVAL:` |
|     ! 0 | 1351 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1352 | `				"%s(): Argument #1 ($flags) must be a combination of CLONE_* flags, "` |
|       - | 1353 | `				"or at least one flag is unsupported by the kernel",` |
|       - | 1354 | `				ph7_function_name(pCtx));` |
|     ! 0 | 1355 | `		case ENOMEM:` |
|     ! 0 | 1356 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 1357 | `				"Error %d: Insufficient memory for unshare",errno);` |
|     ! 0 | 1358 | `			break;` |
|     ! 0 | 1359 | `		case EPERM:` |
|     ! 0 | 1360 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 1361 | `				"Error %d: No privilege to use these flags",errno);` |
|     ! 0 | 1362 | `			break;` |
|       - | 1363 | `#ifdef EUSERS` |
|     ! 0 | 1364 | `		case EUSERS:` |
|     ! 0 | 1365 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 1366 | `				"Error %d: Reached the maximum nesting limit for the user namespace",errno);` |
|     ! 0 | 1367 | `			break;` |
|       - | 1368 | `#endif` |
|     ! 0 | 1369 | `		case ENOSPC:` |
|     ! 0 | 1370 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 1371 | `				"Error %d: Reached the maximum nesting limit for one of the specified namespaces",errno);` |
|     ! 0 | 1372 | `			break;` |
|     ! 0 | 1373 | `		default:` |
|     ! 0 | 1374 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 1375 | `				"Unknown error %d has occurred",errno);` |
|     ! 0 | 1376 | `			break;` |
|       - | 1377 | `		}` |
|     ! 0 | 1378 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1379 | `		return PH7_OK;` |
|       - | 1380 | `	}` |
|     ! 0 | 1381 | `	ph7_result_bool(pCtx,1);` |
|     ! 0 | 1382 | `	return PH7_OK;` |
|       - | 1383 | `}` |
|       - | 1384 | `/* How many CPUs the affinity questions count against. */` |
|     ! 0 | 1385 | `static long PcntlNCpus(void)` |
|       - | 1386 | `{` |
|     ! 0 | 1387 | `	long n = sysconf(_SC_NPROCESSORS_ONLN);` |
|     ! 0 | 1388 | `	return n > 0 ? n : 1;` |
|       - | 1389 | `}` |
|       - | 1390 | `/* The three affinity failures php words, shared by the getter and the setter. */` |
|     ! 0 | 1391 | `static int PcntlAffinityFail(ph7_context *pCtx,sxi64 iPid,const char *zSizeMsg)` |
|       - | 1392 | `{` |
|     ! 0 | 1393 | `	PcntlStoreErr(pCtx->pVm,errno);` |
|     ! 0 | 1394 | `	switch( errno ){` |
|     ! 0 | 1395 | `	case EINVAL:` |
|     ! 0 | 1396 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zSizeMsg);` |
|     ! 0 | 1397 | `		break;` |
|     ! 0 | 1398 | `	case EPERM:` |
|     ! 0 | 1399 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 1400 | `			"Calling process not having the proper privileges");` |
|     ! 0 | 1401 | `		break;` |
|     ! 0 | 1402 | `	case ESRCH:` |
|     ! 0 | 1403 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1404 | `			"%s(): Argument #1 ($process_id) invalid process (%qd)",` |
|       - | 1405 | `			ph7_function_name(pCtx),iPid);` |
|     ! 0 | 1406 | `	default:` |
|     ! 0 | 1407 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 1408 | `			"Unknown error %d has occurred",errno);` |
|     ! 0 | 1409 | `		break;` |
|       - | 1410 | `	}` |
|     ! 0 | 1411 | `	ph7_result_bool(pCtx,0);` |
|     ! 0 | 1412 | `	return PH7_OK;` |
|       - | 1413 | `}` |
|       - | 1414 | `/* array\|false pcntl_getcpuaffinity(?int $process_id = null) */` |
|     ! 0 | 1415 | `PH7_PRIVATE int PH7_builtin_pcntl_getcpuaffinity(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1416 | `{` |
|       - | 1417 | `	cpu_set_t sMask;` |
|       - | 1418 | `	ph7_value *pArray, *pVal;` |
|     ! 0 | 1419 | `	sxi64 iPid = 0;` |
|     ! 0 | 1420 | `	long i, nCpu = PcntlNCpus();` |
|     ! 0 | 1421 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 1422 | `		iPid = ph7_value_to_int64(apArg[0]);` |
|       - | 1423 | `	}` |
|     ! 0 | 1424 | `	CPU_ZERO(&sMask);` |
|     ! 0 | 1425 | `	if( sched_getaffinity((pid_t)iPid,sizeof(sMask),&sMask) != 0 ){` |
|     ! 0 | 1426 | `		return PcntlAffinityFail(pCtx,iPid,"invalid cpu affinity mask size");` |
|       - | 1427 | `	}` |
|     ! 0 | 1428 | `	pArray = ph7_context_new_array(pCtx);` |
|     ! 0 | 1429 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     ! 0 | 1430 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|     ! 0 | 1431 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1432 | `		return PH7_OK;` |
|       - | 1433 | `	}` |
|     ! 0 | 1434 | `	for( i = 0 ; i < nCpu ; ++i ){` |
|     ! 0 | 1435 | `		if( !CPU_ISSET((int)i,&sMask) ){` |
|     ! 0 | 1436 | `			continue;` |
|       - | 1437 | `		}` |
|     ! 0 | 1438 | `		ph7_value_int64(pVal,(ph7_int64)i);` |
|     ! 0 | 1439 | `		ph7_array_add_elem(pArray,0,pVal);` |
|       - | 1440 | `	}` |
|     ! 0 | 1441 | `	ph7_result_value(pCtx,pArray);` |
|     ! 0 | 1442 | `	return PH7_OK;` |
|       - | 1443 | `}` |
|       - | 1444 | `/*` |
|       - | 1445 | ` * bool pcntl_setcpuaffinity(?int $process_id = null, array $cpu_ids = [])` |
|       - | 1446 | ` *  A cpu id may be an int or a NUMERIC STRING; anything else is a TypeError,` |
|       - | 1447 | ` *  and a string that is not a whole number is a ValueError naming it. php's` |
|       - | 1448 | ` *  range sentence prints the CPU COUNT where the last valid id is one less --` |
|       - | 1449 | ` *  reproduced, because it is what a program's error output would show.` |
|       - | 1450 | ` */` |
|     ! 0 | 1451 | `PH7_PRIVATE int PH7_builtin_pcntl_setcpuaffinity(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1452 | `{` |
|       - | 1453 | `	cpu_set_t sMask;` |
|       - | 1454 | `	ph7_hashmap *pMap;` |
|       - | 1455 | `	ph7_hashmap_node *pNode;` |
|     ! 0 | 1456 | `	sxi64 iPid = 0;` |
|     ! 0 | 1457 | `	long nCpu = PcntlNCpus();` |
|       - | 1458 | `	char zGiven[64];` |
|     ! 0 | 1459 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 1460 | `		iPid = ph7_value_to_int64(apArg[0]);` |
|       - | 1461 | `	}` |
|     ! 0 | 1462 | `	CPU_ZERO(&sMask);` |
|     ! 0 | 1463 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     ! 0 | 1464 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1465 | `		return PH7_OK;` |
|       - | 1466 | `	}` |
|     ! 0 | 1467 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     ! 0 | 1468 | `	pMap->pCur = pMap->pFirst;` |
|     ! 0 | 1469 | `	while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|       - | 1470 | `		ph7_value sVal;` |
|       - | 1471 | `		sxi64 iCpu;` |
|     ! 0 | 1472 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|     ! 0 | 1473 | `		PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|     ! 0 | 1474 | `		if( ph7_value_is_string(&sVal) ){` |
|     ! 0 | 1475 | `			if( !PH7_MemObjStringIsNumeric(&sVal) ){` |
|     ! 0 | 1476 | `				int nStr = 0;` |
|     ! 0 | 1477 | `				const char *zStr = ph7_value_to_string(&sVal,&nStr);` |
|     ! 0 | 1478 | `				PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1479 | `					"%s(): Argument #2 ($cpu_ids) cpu id invalid value (%.*s)",` |
|       - | 1480 | `					ph7_function_name(pCtx),nStr,zStr);` |
|     ! 0 | 1481 | `				PH7_MemObjRelease(&sVal);` |
|     ! 0 | 1482 | `				return PH7_OK;` |
|       - | 1483 | `			}` |
|     ! 0 | 1484 | `		}else if( !ph7_value_is_int(&sVal) ){` |
|     ! 0 | 1485 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1486 | `				"%s(): Argument #2 ($cpu_ids) value must be of type int\|string, %s given",` |
|       - | 1487 | `				ph7_function_name(pCtx),VmValueGivenName(&sVal,zGiven,sizeof(zGiven)));` |
|     ! 0 | 1488 | `			PH7_MemObjRelease(&sVal);` |
|     ! 0 | 1489 | `			return PH7_OK;` |
|       - | 1490 | `		}` |
|     ! 0 | 1491 | `		iCpu = ph7_value_to_int64(&sVal);` |
|     ! 0 | 1492 | `		PH7_MemObjRelease(&sVal);` |
|     ! 0 | 1493 | `		if( iCpu < 0 \|\| iCpu >= (sxi64)nCpu ){` |
|     ! 0 | 1494 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1495 | `				"%s(): Argument #2 ($cpu_ids) cpu id must be between 0 and %qd (%qd)",` |
|       - | 1496 | `				ph7_function_name(pCtx),(sxi64)nCpu,iCpu);` |
|       - | 1497 | `		}` |
|     ! 0 | 1498 | `		CPU_SET((int)iCpu,&sMask);` |
|       - | 1499 | `	}` |
|     ! 0 | 1500 | `	if( sched_setaffinity((pid_t)iPid,sizeof(sMask),&sMask) != 0 ){` |
|     ! 0 | 1501 | `		return PcntlAffinityFail(pCtx,iPid,"invalid cpu affinity mask size or unmapped cpu id(s)");` |
|       - | 1502 | `	}` |
|     ! 0 | 1503 | `	ph7_result_bool(pCtx,1);` |
|     ! 0 | 1504 | `	return PH7_OK;` |
|       - | 1505 | `}` |
|       - | 1506 | `/* int pcntl_getcpu() */` |
|     ! 0 | 1507 | `PH7_PRIVATE int PH7_builtin_pcntl_getcpu(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - | 1508 | `{` |
|       - | 1509 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 | 1510 | `	ph7_result_int64(pCtx,(ph7_int64)sched_getcpu());` |
|     ! 0 | 1511 | `	return PH7_OK;` |
|       - | 1512 | `}` |
|       - | 1513 | `#endif /* __linux__ */` |
|       - | 1514 |  |
|       - | 1515 | `/* --- The constants -------------------------------------------------------- */` |
|       - | 1516 |  |
|       - | 1517 | `/*` |
|       - | 1518 | ` * Every one of these is the PLATFORM's macro rather than a number copied out of` |
|       - | 1519 | ` * one build: SIGUSR1 is 10 on Linux and 30 on macOS, and a script that stores` |
|       - | 1520 | ` * one and hands it back to posix_kill() has to get its own system's answer.` |
|       - | 1521 | ` * php defines none of them on Windows, so neither does this.` |
|       - | 1522 | ` *` |
|       - | 1523 | ` * Two are missing from the table on purpose: SIGRTMIN and SIGRTMAX are function` |
|       - | 1524 | ` * CALLS on glibc, not constants, so the registration below adds them by hand.` |
|       - | 1525 | ` */` |
|       - | 1526 | `static const struct {` |
|       - | 1527 | `	const char *zName;` |
|       - | 1528 | `	int iValue;` |
|       - | 1529 | `} aPcntlConst[] = {` |
|       - | 1530 | `	/* The wait flags */` |
|       - | 1531 | `	{ "WNOHANG",    WNOHANG    },` |
|       - | 1532 | `	{ "WUNTRACED",  WUNTRACED  },` |
|       - | 1533 | `#ifdef WCONTINUED` |
|       - | 1534 | `	{ "WCONTINUED", WCONTINUED },` |
|       - | 1535 | `#endif` |
|       - | 1536 | `#ifdef WEXITED` |
|       - | 1537 | `	{ "WEXITED",    WEXITED    },` |
|       - | 1538 | `#endif` |
|       - | 1539 | `#ifdef WSTOPPED` |
|       - | 1540 | `	{ "WSTOPPED",   WSTOPPED   },` |
|       - | 1541 | `#endif` |
|       - | 1542 | `#ifdef WNOWAIT` |
|       - | 1543 | `	{ "WNOWAIT",    WNOWAIT    },` |
|       - | 1544 | `#endif` |
|       - | 1545 | `#if defined(__linux__) \|\| defined(__APPLE__)` |
|       - | 1546 | `	/* waitid()'s id types are an ENUM rather than macros, so they cannot be` |
|       - | 1547 | `	 * #ifdef'd one by one the way everything else here can. */` |
|       - | 1548 | `	{ "P_ALL",  P_ALL  },` |
|       - | 1549 | `	{ "P_PID",  P_PID  },` |
|       - | 1550 | `	{ "P_PGID", P_PGID },` |
|       - | 1551 | `#if defined(__GLIBC__) && (__GLIBC__ > 2 \|\| (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 31))` |
|       - | 1552 | `	{ "P_PIDFD", P_PIDFD },` |
|       - | 1553 | `#endif` |
|       - | 1554 | `#endif` |
|       - | 1555 | `	/* The three dispositions. SIG_DFL and friends are function POINTERS. */` |
|       - | 1556 | `	{ "SIG_IGN", (int)(sxptr)SIG_IGN },` |
|       - | 1557 | `	{ "SIG_DFL", (int)(sxptr)SIG_DFL },` |
|       - | 1558 | `	{ "SIG_ERR", (int)(sxptr)SIG_ERR },` |
|       - | 1559 | `	/* The signals */` |
|       - | 1560 | `	{ "SIGHUP",    SIGHUP    },` |
|       - | 1561 | `	{ "SIGINT",    SIGINT    },` |
|       - | 1562 | `	{ "SIGQUIT",   SIGQUIT   },` |
|       - | 1563 | `	{ "SIGILL",    SIGILL    },` |
|       - | 1564 | `	{ "SIGTRAP",   SIGTRAP   },` |
|       - | 1565 | `	{ "SIGABRT",   SIGABRT   },` |
|       - | 1566 | `#ifdef SIGIOT` |
|       - | 1567 | `	{ "SIGIOT",    SIGIOT    },` |
|       - | 1568 | `#endif` |
|       - | 1569 | `	{ "SIGBUS",    SIGBUS    },` |
|       - | 1570 | `	{ "SIGFPE",    SIGFPE    },` |
|       - | 1571 | `	{ "SIGKILL",   SIGKILL   },` |
|       - | 1572 | `	{ "SIGUSR1",   SIGUSR1   },` |
|       - | 1573 | `	{ "SIGSEGV",   SIGSEGV   },` |
|       - | 1574 | `	{ "SIGUSR2",   SIGUSR2   },` |
|       - | 1575 | `	{ "SIGPIPE",   SIGPIPE   },` |
|       - | 1576 | `	{ "SIGALRM",   SIGALRM   },` |
|       - | 1577 | `	{ "SIGTERM",   SIGTERM   },` |
|       - | 1578 | `#ifdef SIGSTKFLT` |
|       - | 1579 | `	{ "SIGSTKFLT", SIGSTKFLT },` |
|       - | 1580 | `#endif` |
|       - | 1581 | `#ifdef SIGCLD` |
|       - | 1582 | `	{ "SIGCLD",    SIGCLD    },` |
|       - | 1583 | `#endif` |
|       - | 1584 | `#ifdef SIGCHLD` |
|       - | 1585 | `	{ "SIGCHLD",   SIGCHLD   },` |
|       - | 1586 | `#endif` |
|       - | 1587 | `	{ "SIGCONT",   SIGCONT   },` |
|       - | 1588 | `	{ "SIGSTOP",   SIGSTOP   },` |
|       - | 1589 | `	{ "SIGTSTP",   SIGTSTP   },` |
|       - | 1590 | `	{ "SIGTTIN",   SIGTTIN   },` |
|       - | 1591 | `	{ "SIGTTOU",   SIGTTOU   },` |
|       - | 1592 | `	{ "SIGURG",    SIGURG    },` |
|       - | 1593 | `	{ "SIGXCPU",   SIGXCPU   },` |
|       - | 1594 | `	{ "SIGXFSZ",   SIGXFSZ   },` |
|       - | 1595 | `	{ "SIGVTALRM", SIGVTALRM },` |
|       - | 1596 | `	{ "SIGPROF",   SIGPROF   },` |
|       - | 1597 | `	{ "SIGWINCH",  SIGWINCH  },` |
|       - | 1598 | `#ifdef SIGPOLL` |
|       - | 1599 | `	{ "SIGPOLL",   SIGPOLL   },` |
|       - | 1600 | `#endif` |
|       - | 1601 | `#ifdef SIGIO` |
|       - | 1602 | `	{ "SIGIO",     SIGIO     },` |
|       - | 1603 | `#endif` |
|       - | 1604 | `#ifdef SIGPWR` |
|       - | 1605 | `	{ "SIGPWR",    SIGPWR    },` |
|       - | 1606 | `#endif` |
|       - | 1607 | `#ifdef SIGSYS` |
|       - | 1608 | `	{ "SIGSYS",    SIGSYS    },` |
|       - | 1609 | `	/* php ships SIGBABY beside SIGSYS -- the SunOS spelling of the same` |
|       - | 1610 | `	 * signal, which glibc does not define and php registers anyway. */` |
|       - | 1611 | `	{ "SIGBABY",   SIGSYS    },` |
|       - | 1612 | `#endif` |
|       - | 1613 | `	/* Priority */` |
|       - | 1614 | `	{ "PRIO_PGRP",    PRIO_PGRP    },` |
|       - | 1615 | `	{ "PRIO_USER",    PRIO_USER    },` |
|       - | 1616 | `	{ "PRIO_PROCESS", PRIO_PROCESS },` |
|       - | 1617 | `	/* Mask modes */` |
|       - | 1618 | `	{ "SIG_BLOCK",   SIG_BLOCK   },` |
|       - | 1619 | `	{ "SIG_UNBLOCK", SIG_UNBLOCK },` |
|       - | 1620 | `	{ "SIG_SETMASK", SIG_SETMASK },` |
|       - | 1621 | ``	/* siginfo's `code` field, per origin and per signal */`` |
|       - | 1622 | `#ifdef SI_USER` |
|       - | 1623 | `	{ "SI_USER",    SI_USER    },` |
|       - | 1624 | `#endif` |
|       - | 1625 | `#ifdef SI_KERNEL` |
|       - | 1626 | `	{ "SI_KERNEL",  SI_KERNEL  },` |
|       - | 1627 | `#endif` |
|       - | 1628 | `#ifdef SI_QUEUE` |
|       - | 1629 | `	{ "SI_QUEUE",   SI_QUEUE   },` |
|       - | 1630 | `#endif` |
|       - | 1631 | `#ifdef SI_TIMER` |
|       - | 1632 | `	{ "SI_TIMER",   SI_TIMER   },` |
|       - | 1633 | `#endif` |
|       - | 1634 | `#ifdef SI_MESGQ` |
|       - | 1635 | `	{ "SI_MESGQ",   SI_MESGQ   },` |
|       - | 1636 | `#endif` |
|       - | 1637 | `#ifdef SI_ASYNCIO` |
|       - | 1638 | `	{ "SI_ASYNCIO", SI_ASYNCIO },` |
|       - | 1639 | `#endif` |
|       - | 1640 | `#ifdef SI_SIGIO` |
|       - | 1641 | `	{ "SI_SIGIO",   SI_SIGIO   },` |
|       - | 1642 | `#endif` |
|       - | 1643 | `#ifdef SI_TKILL` |
|       - | 1644 | `	{ "SI_TKILL",   SI_TKILL   },` |
|       - | 1645 | `#endif` |
|       - | 1646 | `#ifdef CLD_EXITED` |
|       - | 1647 | `	{ "CLD_EXITED",    CLD_EXITED    },` |
|       - | 1648 | `	{ "CLD_KILLED",    CLD_KILLED    },` |
|       - | 1649 | `	{ "CLD_DUMPED",    CLD_DUMPED    },` |
|       - | 1650 | `	{ "CLD_TRAPPED",   CLD_TRAPPED   },` |
|       - | 1651 | `	{ "CLD_STOPPED",   CLD_STOPPED   },` |
|       - | 1652 | `	{ "CLD_CONTINUED", CLD_CONTINUED },` |
|       - | 1653 | `#endif` |
|       - | 1654 | `#ifdef TRAP_BRKPT` |
|       - | 1655 | `	{ "TRAP_BRKPT", TRAP_BRKPT },` |
|       - | 1656 | `	{ "TRAP_TRACE", TRAP_TRACE },` |
|       - | 1657 | `#endif` |
|       - | 1658 | `#ifdef POLL_IN` |
|       - | 1659 | `	{ "POLL_IN",  POLL_IN  },` |
|       - | 1660 | `	{ "POLL_OUT", POLL_OUT },` |
|       - | 1661 | `	{ "POLL_MSG", POLL_MSG },` |
|       - | 1662 | `	{ "POLL_ERR", POLL_ERR },` |
|       - | 1663 | `	{ "POLL_PRI", POLL_PRI },` |
|       - | 1664 | `	{ "POLL_HUP", POLL_HUP },` |
|       - | 1665 | `#endif` |
|       - | 1666 | `#ifdef ILL_ILLOPC` |
|       - | 1667 | `	{ "ILL_ILLOPC", ILL_ILLOPC },` |
|       - | 1668 | `	{ "ILL_ILLOPN", ILL_ILLOPN },` |
|       - | 1669 | `	{ "ILL_ILLADR", ILL_ILLADR },` |
|       - | 1670 | `	{ "ILL_ILLTRP", ILL_ILLTRP },` |
|       - | 1671 | `	{ "ILL_PRVOPC", ILL_PRVOPC },` |
|       - | 1672 | `	{ "ILL_PRVREG", ILL_PRVREG },` |
|       - | 1673 | `	{ "ILL_COPROC", ILL_COPROC },` |
|       - | 1674 | `	{ "ILL_BADSTK", ILL_BADSTK },` |
|       - | 1675 | `#endif` |
|       - | 1676 | `#ifdef FPE_INTDIV` |
|       - | 1677 | `	{ "FPE_INTDIV", FPE_INTDIV },` |
|       - | 1678 | `	{ "FPE_INTOVF", FPE_INTOVF },` |
|       - | 1679 | `	{ "FPE_FLTDIV", FPE_FLTDIV },` |
|       - | 1680 | `	{ "FPE_FLTOVF", FPE_FLTOVF },` |
|       - | 1681 | `	{ "FPE_FLTUND", FPE_FLTUND },` |
|       - | 1682 | `	{ "FPE_FLTRES", FPE_FLTRES },` |
|       - | 1683 | `	{ "FPE_FLTINV", FPE_FLTINV },` |
|       - | 1684 | `	{ "FPE_FLTSUB", FPE_FLTSUB },` |
|       - | 1685 | `#endif` |
|       - | 1686 | `#ifdef SEGV_MAPERR` |
|       - | 1687 | `	{ "SEGV_MAPERR", SEGV_MAPERR },` |
|       - | 1688 | `	{ "SEGV_ACCERR", SEGV_ACCERR },` |
|       - | 1689 | `#endif` |
|       - | 1690 | `#ifdef BUS_ADRALN` |
|       - | 1691 | `	{ "BUS_ADRALN", BUS_ADRALN },` |
|       - | 1692 | `	{ "BUS_ADRERR", BUS_ADRERR },` |
|       - | 1693 | `	{ "BUS_OBJERR", BUS_OBJERR },` |
|       - | 1694 | `#endif` |
|       - | 1695 | `	/* The namespaces pcntl_unshare() can leave */` |
|       - | 1696 | `#ifdef CLONE_NEWNS` |
|       - | 1697 | `	{ "CLONE_NEWNS",     CLONE_NEWNS     },` |
|       - | 1698 | `#endif` |
|       - | 1699 | `#ifdef CLONE_NEWIPC` |
|       - | 1700 | `	{ "CLONE_NEWIPC",    CLONE_NEWIPC    },` |
|       - | 1701 | `#endif` |
|       - | 1702 | `#ifdef CLONE_NEWUTS` |
|       - | 1703 | `	{ "CLONE_NEWUTS",    CLONE_NEWUTS    },` |
|       - | 1704 | `#endif` |
|       - | 1705 | `#ifdef CLONE_NEWNET` |
|       - | 1706 | `	{ "CLONE_NEWNET",    CLONE_NEWNET    },` |
|       - | 1707 | `#endif` |
|       - | 1708 | `#ifdef CLONE_NEWPID` |
|       - | 1709 | `	{ "CLONE_NEWPID",    CLONE_NEWPID    },` |
|       - | 1710 | `#endif` |
|       - | 1711 | `#ifdef CLONE_NEWUSER` |
|       - | 1712 | `	{ "CLONE_NEWUSER",   CLONE_NEWUSER   },` |
|       - | 1713 | `#endif` |
|       - | 1714 | `#ifdef CLONE_NEWCGROUP` |
|       - | 1715 | `	{ "CLONE_NEWCGROUP", CLONE_NEWCGROUP },` |
|       - | 1716 | `#endif` |
|       - | 1717 | `	/* php's own names for the errnos pcntl_get_last_error() can answer with */` |
|       - | 1718 | `	{ "PCNTL_EINTR",        EINTR        },` |
|       - | 1719 | `	{ "PCNTL_ECHILD",       ECHILD       },` |
|       - | 1720 | `	{ "PCNTL_EINVAL",       EINVAL       },` |
|       - | 1721 | `	{ "PCNTL_EAGAIN",       EAGAIN       },` |
|       - | 1722 | `	{ "PCNTL_ESRCH",        ESRCH        },` |
|       - | 1723 | `	{ "PCNTL_EACCES",       EACCES       },` |
|       - | 1724 | `	{ "PCNTL_EPERM",        EPERM        },` |
|       - | 1725 | `	{ "PCNTL_ENOMEM",       ENOMEM       },` |
|       - | 1726 | `	{ "PCNTL_E2BIG",        E2BIG        },` |
|       - | 1727 | `	{ "PCNTL_EFAULT",       EFAULT       },` |
|       - | 1728 | `	{ "PCNTL_EIO",          EIO          },` |
|       - | 1729 | `	{ "PCNTL_EISDIR",       EISDIR       },` |
|       - | 1730 | `#ifdef ELIBBAD` |
|       - | 1731 | `	{ "PCNTL_ELIBBAD",      ELIBBAD      },` |
|       - | 1732 | `#endif` |
|       - | 1733 | `	{ "PCNTL_ELOOP",        ELOOP        },` |
|       - | 1734 | `	{ "PCNTL_EMFILE",       EMFILE       },` |
|       - | 1735 | `	{ "PCNTL_ENAMETOOLONG", ENAMETOOLONG },` |
|       - | 1736 | `	{ "PCNTL_ENFILE",       ENFILE       },` |
|       - | 1737 | `	{ "PCNTL_ENOENT",       ENOENT       },` |
|       - | 1738 | `	{ "PCNTL_ENOEXEC",      ENOEXEC      },` |
|       - | 1739 | `	{ "PCNTL_ENOTDIR",      ENOTDIR      },` |
|       - | 1740 | `	{ "PCNTL_ETXTBSY",      ETXTBSY      },` |
|       - | 1741 | `	{ "PCNTL_ENOSPC",       ENOSPC       },` |
|       - | 1742 | `#ifdef EUSERS` |
|       - | 1743 | `	{ "PCNTL_EUSERS",       EUSERS       },` |
|       - | 1744 | `#endif` |
|       - | 1745 | `};` |
|       - | 1746 | `/* The shared expander: pUserData carries the value (SX_INT_TO_PTR). */` |
|    8543 | 1747 | `static void PcntlConstExpand(ph7_value *pVal,void *pUserData)` |
|       - | 1748 | `{` |
|    8543 | 1749 | `	ph7_value_int(pVal,SX_PTR_TO_INT(pUserData));` |
|    8543 | 1750 | `}` |
|    6691 | 1751 | `PH7_PRIVATE void PH7_RegisterPcntlConstants(ph7_vm *pVm)` |
|       - | 1752 | `{` |
|       - | 1753 | `	sxu32 n;` |
|  803008 | 1754 | `	for( n = 0 ; n < SX_ARRAYSIZE(aPcntlConst) ; ++n ){` |
| 1167057 | 1755 | `		ph7_create_constant(&(*pVm),aPcntlConst[n].zName,PcntlConstExpand,` |
|  796317 | 1756 | `			SX_INT_TO_PTR(aPcntlConst[n].iValue));` |
|  370740 | 1757 | `	}` |
|       - | 1758 | `#ifdef SIGRTMIN` |
|    3351 | 1759 | `	ph7_create_constant(&(*pVm),"SIGRTMIN",PcntlConstExpand,SX_INT_TO_PTR(SIGRTMIN));` |
|       - | 1760 | `#endif` |
|       - | 1761 | `#ifdef SIGRTMAX` |
|    3351 | 1762 | `	ph7_create_constant(&(*pVm),"SIGRTMAX",PcntlConstExpand,SX_INT_TO_PTR(SIGRTMAX));` |
|       - | 1763 | `#endif` |
|    6691 | 1764 | `}` |
|       - | 1765 | `/*` |
|       - | 1766 | ` * Pcntl\QosClass: php registers this pure enum on EVERY platform, even though` |
|       - | 1767 | ` * the two functions that read it (pcntl_getqos_class/pcntl_setqos_class) exist` |
|       - | 1768 | `` * only on macOS -- so `enum_exists('Pcntl\QosClass')` is true under a Linux php`` |
|       - | 1769 | ` * and has to be true here.` |
|       - | 1770 | ` */` |
|       - | 1771 | `static const char * const azPcntlQos[] = {` |
|       - | 1772 | `	"UserInteractive", "UserInitiated", "Default", "Utility", "Background",` |
|       - | 1773 | `};` |
|    7925 | 1774 | `PH7_PRIVATE sxi32 PH7_VmInstallPcntl(ph7_vm *pVm)` |
|       - | 1775 | `{` |
|       - | 1776 | `	PH7_NativeEnumCase aCase[SX_ARRAYSIZE(azPcntlQos)];` |
|       - | 1777 | `	sxu32 n;` |
|   47550 | 1778 | `	for( n = 0 ; n < SX_ARRAYSIZE(azPcntlQos) ; ++n ){` |
|   39625 | 1779 | `		aCase[n].zName = azPcntlQos[n];` |
|   39625 | 1780 | `		aCase[n].sValue.zName = 0;` |
|   39625 | 1781 | `		aCase[n].sValue.iMods = 0;` |
|   39625 | 1782 | `		aCase[n].sValue.iType = PH7_NATIVE_VAL_NULL;` |
|   39625 | 1783 | `		aCase[n].sValue.iValue = 0;` |
|   39625 | 1784 | `		aCase[n].sValue.zValue = 0;` |
|   39625 | 1785 | `		aCase[n].sValue.rValue = 0.0;` |
|   19785 | 1786 | `	}` |
|   11882 | 1787 | `	return PH7_InstallNativeEnum(&(*pVm),"Pcntl\\QosClass",0,` |
|    3957 | 1788 | `		aCase,SX_ARRAYSIZE(aCase),0,0);` |
|       - | 1789 | `}` |
|       - | 1790 | `#endif /* the real thing */` |
|       - | 1791 |  |
