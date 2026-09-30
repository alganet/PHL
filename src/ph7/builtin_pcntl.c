/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
/* waitid(), unshare(), sched_getaffinity() and sched_getcpu() are outside the
 * strict POSIX subset a default compile exposes, and php's own build asks for
 * them the same way. This has to come BEFORE any system header, which is why it
 * is above ph7int.h rather than beside the includes below. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include "ph7int.h"
/*
 * Section:
 *    php's pcntl extension: fork/exec, the wait family, and the SIGNAL half a
 *    command-line program handles Ctrl-C with.
 * Status:
 *    Stable.
 *
 * php's ext/pcntl is a shell over the C library's process and signal calls, so
 * every answer here is the platform's; what had to be reproduced is the shape
 * php gives them, and one piece of machinery that is not a system call at all.
 *
 *   THE EXTENSION IS NOT THERE ON WINDOWS. php builds no ext/pcntl for it, so
 *   `extension_loaded('pcntl')` and `function_exists('pcntl_signal')` are both
 *   false on a Windows php -- which is exactly what monolog's SignalHandler and
 *   symfony/console's SignalRegistry test before calling anything. This whole
 *   translation unit is empty there, and the constants are not defined either.
 *   The `posix` precedent (124th session) is the same rule.
 *
 *   A SIGNAL IS NOT DELIVERED TO PHP CODE WHERE THE KERNEL DELIVERS IT. The C
 *   handler may not call an interpreter, so it records the delivery in a
 *   preallocated ring (nothing it does allocates or takes a lock) and the PHP
 *   handler runs later, at a point the engine chooses. There are two such
 *   points, and which one a program gets is `pcntl_async_signals()`:
 *
 *     ASYNC OFF (php's default): nothing runs until the program itself asks,
 *     with `pcntl_signal_dispatch()`. A signal that arrives between two such
 *     calls is remembered, not lost.
 *
 *     ASYNC ON: the C handler raises PH7_PcntlAsyncPending, and the executor's
 *     fetch point -- the same guard that routes a C-boundary throw -- drains
 *     the ring before the next instruction. That is php's `EG(vm_interrupt)`
 *     with a different name, and it is why `posix_kill(getmypid(), SIGURG)`
 *     followed by an `echo` prints the handler's line FIRST.
 *
 *   The two share one ring, so a signal that arrived while async was off is
 *   still there afterwards -- but turning async ON does not sweep it up. php
 *   raises its interrupt from the SIGNAL HANDLER and nowhere else, so what is
 *   already queued waits for the next dispatch or the next signal, and under an
 *   async program `pcntl_signal_dispatch()` usually just finds the ring empty
 *   and answers true.
 *
 *   A DISPATCH DETACHES THE QUEUE before it runs anything, which is what stops
 *   a handler that re-raises its own signal from looping for ever: the new
 *   delivery lands on what is, by then, a fresh queue. A dispatch reached from
 *   INSIDE a handler is a no-op for the same reason. And a handler that THROWS
 *   takes the rest of the detached queue with it -- php's own loop keeps
 *   walking but zend refuses to call anything while an exception is pending, so
 *   whatever was queued behind the thrower is dropped rather than deferred.
 *
 *   WHAT A HANDLER IS TOLD is `($signo, $siginfo)`, and the second argument is
 *   php's own selection out of `siginfo_t` rather than the whole of it: signo,
 *   errno and code always, plus status/utime/stime/pid/uid for SIGCHLD, addr
 *   (as a FLOAT, which is php's own choice) for the four fault signals, and
 *   band/fd for SIGPOLL. `pcntl_sigwaitinfo()`, `pcntl_sigtimedwait()` and
 *   `pcntl_waitid()` fill the same array.
 *
 *   FAILURE IS false (or a negative number) AND AN ERRNO. `pcntl_get_last_error()`
 *   (and its alias `pcntl_errno()`) answers what the last call that stored one
 *   saw, and nothing clears it -- exactly the ext/posix rule, and a separate
 *   number from posix's. Most failures ALSO raise an E_WARNING whose text is a
 *   per-function errno table php wrote by hand ("Error 3: No process was located
 *   using the given parameters"); `pcntl_sigtimedwait()` is the one that stores
 *   nothing on its own timeout, because EAGAIN is that call's success.
 *
 *   RESTARTING IS PER SIGNAL. `pcntl_signal($signo, $h, $restart)` picks
 *   SA_RESTART or not, so a `pcntl_waitpid()` interrupted by that signal either
 *   resumes or answers -1/EINTR -- which is a behaviour monolog's suite asserts
 *   both ways. SIGALRM is never restarted, whatever the argument says: that is
 *   php's own exception and it is reproduced.
 */
#include <signal.h>
/*
 * The async-dispatch flag, raised by the C signal handler and read by the
 * executor's fetch point (VmLoopFetch). It lives OUTSIDE every guard below
 * because vm_exec.c reads it in every build -- on Windows and in the tiny
 * build it is simply always zero, and the drain beside it is a no-op.
 */
PH7_PRIVATE volatile sig_atomic_t PH7_PcntlAsyncPending = 0;

#if defined(PH7_DISABLE_BUILTIN_FUNC) || defined(__WINNT__)
/* No ext/pcntl in this build: the executor still links against the drain. */
PH7_PRIVATE void PH7_PcntlDrainAsync(ph7_vm *pVm)
{
	SXUNUSED(pVm);
	PH7_PcntlAsyncPending = 0;
}
PH7_PRIVATE void PH7_PcntlVmRelease(ph7_vm *pVm)
{
	SXUNUSED(pVm);
}
PH7_PRIVATE void PH7_RegisterPcntlConstants(ph7_vm *pVm)
{
	SXUNUSED(pVm);
}
PH7_PRIVATE sxi32 PH7_VmInstallPcntl(ph7_vm *pVm)
{
	SXUNUSED(pVm);
	return SXRET_OK;
}
#else /* the real thing */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#ifdef __linux__
#include <sched.h>
#endif
#include <sys/param.h>    /* HZ, which php divides si_utime/si_stime by */

/* php's own bound on a signal number, and the one its diagnostics quote. */
#ifndef NSIG
#define NSIG 32
#endif
#define PCNTL_NSIG NSIG

/* The delivery ring. Sized well past anything a program can leave pending: the
 * kernel coalesces a non-realtime signal that is already queued, so the depth
 * that matters is "distinct signals delivered between two drains". */
#define PCNTL_QUEUE 64

/* php divides si_utime/si_stime by HZ. */
#ifndef HZ
#define HZ 100
#endif

typedef struct pcntl_event pcntl_event;
struct pcntl_event {
	int signo;
	siginfo_t sInfo;
};
/*
 * The extension's per-VM state: the handler a signal was last given, and the
 * two numbers php keeps beside it. php holds one set per MODULE; per VM is the
 * same lifetime for a program and keeps two embedded VMs apart, which is the
 * shape ext/posix and ext/gettext already use.
 */
typedef struct pcntl_state pcntl_state;
struct pcntl_state {
	ph7_value aHandler[PCNTL_NSIG]; /* the value pcntl_signal() was given, per signal */
	sxu8 aSet[PCNTL_NSIG];          /* 1 once that slot has been written */
	int iLastError;                 /* pcntl_get_last_error() */
	int bAsync;                     /* pcntl_async_signals() */
	int bDraining;                  /* re-entry guard around the drain */
};
/*
 * The C signal handler reaches the ring through file statics, because that is
 * all a handler may touch: no allocation, no lock, no interpreter. The cursors
 * live in [0, 2*PCNTL_QUEUE) rather than counting up for ever, so a program
 * that handles signals for weeks cannot overflow them into a negative index.
 * The VM pointer beside them is not the handler's: it only records WHOSE
 * handler table the process-wide dispositions belong to, so teardown of a
 * different VM leaves them alone.
 */
static ph7_vm *pPcntlVm = 0;
static volatile sig_atomic_t bPcntlAsyncFlag = 0;
static volatile sig_atomic_t nPcntlHead = 0;
static volatile sig_atomic_t nPcntlTail = 0;
static pcntl_event aPcntlQueue[PCNTL_QUEUE];

/* --- The state ---------------------------------------------------------- */

static pcntl_state * PcntlState(ph7_vm *pVm,int bCreate)
{
	pcntl_state *pS = (pcntl_state *)pVm->pPcntl;
	if( pS == 0 && bCreate ){
		int i;
		pS = (pcntl_state *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(pcntl_state));
		if( pS == 0 ){
			return 0;
		}
		SyZero(pS,sizeof(pcntl_state));
		for( i = 0 ; i < PCNTL_NSIG ; ++i ){
			PH7_MemObjInit(pVm,&pS->aHandler[i]);
		}
		pVm->pPcntl = pS;
		pPcntlVm = pVm;
	}
	return pS;
}
/*
 * Remember WHY the last call failed. Which calls store one is per-function and
 * measured: most failures do, `pcntl_sigtimedwait()`'s own timeout does not
 * (EAGAIN is that call's "nothing arrived"), and two REFUSALS do even though
 * they throw rather than answer -- a handler that is not callable and a
 * priority mode php does not name both leave EINVAL behind.
 */
static void PcntlStoreErr(ph7_vm *pVm,int iErr)
{
	pcntl_state *pS = PcntlState(pVm,1);
	if( pS ){
		pS->iLastError = iErr;
	}
}

/* --- The delivery ring -------------------------------------------------- */

/*
 * The C handler. Everything it touches is preallocated and everything it does
 * is a store, so it is async-signal-safe in the strict sense -- php's own
 * allocates a spare queue entry here, which is a known wart rather than a model.
 * A full ring DROPS the delivery: the alternative is allocating inside a signal
 * handler, and the depth is far past what a program can hold pending.
 */
static void PcntlSigHandler(int signo,siginfo_t *pInfo,void *pUnused)
{
	int nT = (int)nPcntlTail;
	int nUsed = (nT - (int)nPcntlHead + 2 * PCNTL_QUEUE) % (2 * PCNTL_QUEUE);
	int nSaved = errno;
	SXUNUSED(pUnused);
	if( nUsed < PCNTL_QUEUE ){
		pcntl_event *pE = &aPcntlQueue[nT % PCNTL_QUEUE];
		pE->signo = signo;
		if( pInfo ){
			pE->sInfo = *pInfo;
		}else{
			SyZero(&pE->sInfo,sizeof(pE->sInfo));
			pE->sInfo.si_signo = signo;
		}
		/* The store has to be complete before the cursor says it is there. */
		nPcntlTail = (sig_atomic_t)((nT + 1) % (2 * PCNTL_QUEUE));
	}
	if( bPcntlAsyncFlag ){
		PH7_PcntlAsyncPending = 1;
	}
	errno = nSaved;
}
/*
 * php's php_signal4(): SA_SIGINFO always, every other signal blocked for the
 * duration when the handler is ours, and SA_RESTART unless the caller asked for
 * an interruptible one -- or the signal is SIGALRM, which php never restarts.
 * Answers 0 on success and -1 with errno set.
 */
static int PcntlInstall(int signo,void *pFunc,int bRestart,int bMaskAll)
{
	struct sigaction sAct;
	SyZero(&sAct,sizeof(sAct));
	if( pFunc == (void *)PcntlSigHandler ){
		sAct.sa_sigaction = PcntlSigHandler;
		sAct.sa_flags = SA_SIGINFO;
	}else{
		sAct.sa_handler = (void (*)(int))pFunc;
		sAct.sa_flags = 0;
	}
	if( bMaskAll ){
		sigfillset(&sAct.sa_mask);
	}else{
		sigemptyset(&sAct.sa_mask);
	}
#ifdef SIGALRM
	if( signo != SIGALRM && bRestart ){
#else
	if( bRestart ){
#endif
#ifdef SA_RESTART
		sAct.sa_flags |= SA_RESTART;
#endif
	}else{
#ifdef SA_INTERRUPT
		sAct.sa_flags |= SA_INTERRUPT;   /* SunOS */
#endif
	}
	return sigaction(signo,&sAct,0);
}

/* --- What a handler is told --------------------------------------------- */

/*
 * php's siginfo array: three keys always, and a per-signal tail. `addr` is a
 * FLOAT because php adds it with add_assoc_double -- reproduced rather than
 * corrected, since a program can print it.
 */
static void PcntlSigInfoArray(ph7_vm *pVm,ph7_value *pArray,int signo,const siginfo_t *pInfo)
{
	ph7_value *pVal = ph7_new_scalar(pVm);
	if( pVal == 0 ){
		return;
	}
	ph7_value_int64(pVal,(ph7_int64)pInfo->si_signo);
	ph7_array_add_strkey_elem(pArray,"signo",pVal);
	ph7_value_int64(pVal,(ph7_int64)pInfo->si_errno);
	ph7_array_add_strkey_elem(pArray,"errno",pVal);
	ph7_value_int64(pVal,(ph7_int64)pInfo->si_code);
	ph7_array_add_strkey_elem(pArray,"code",pVal);
	switch( signo ){
#ifdef SIGCHLD
	case SIGCHLD:
		ph7_value_int64(pVal,(ph7_int64)pInfo->si_status);
		ph7_array_add_strkey_elem(pArray,"status",pVal);
#ifdef si_utime
		ph7_value_double(pVal,(double)pInfo->si_utime / (double)HZ);
		ph7_array_add_strkey_elem(pArray,"utime",pVal);
#endif
#ifdef si_stime
		ph7_value_double(pVal,(double)pInfo->si_stime / (double)HZ);
		ph7_array_add_strkey_elem(pArray,"stime",pVal);
#endif
		ph7_value_int64(pVal,(ph7_int64)pInfo->si_pid);
		ph7_array_add_strkey_elem(pArray,"pid",pVal);
		ph7_value_int64(pVal,(ph7_int64)pInfo->si_uid);
		ph7_array_add_strkey_elem(pArray,"uid",pVal);
		break;
#endif
	case SIGILL:
	case SIGFPE:
	case SIGSEGV:
	case SIGBUS:
		ph7_value_double(pVal,(double)(sxi64)(sxptr)pInfo->si_addr);
		ph7_array_add_strkey_elem(pArray,"addr",pVal);
		break;
#ifdef SIGPOLL
	case SIGPOLL:
		ph7_value_int64(pVal,(ph7_int64)pInfo->si_band);
		ph7_array_add_strkey_elem(pArray,"band",pVal);
#ifdef si_fd
		ph7_value_int64(pVal,(ph7_int64)pInfo->si_fd);
		ph7_array_add_strkey_elem(pArray,"fd",pVal);
#endif
		break;
#endif
	default:
		break;
	}
	ph7_release_value(pVm,pVal);
}
/*
 * Run the PHP handlers for everything the ring held WHEN THIS STARTED. Answers
 * the unwind a handler produced (PH7_EXCEPTION / PH7_ABORT) so the sync door
 * can propagate it; the async door needs nothing back, because
 * PH7_VmCallUserFunction has already parked the same status for the fetch
 * point that called it.
 *
 * Three things keep this from running away, and each of them is php's:
 * the async flag is cleared before any handler runs, a re-entrant call is a
 * no-op, and the queue is SNAPSHOT rather than followed.
 */
static sxi32 PcntlDrain(ph7_vm *pVm)
{
	pcntl_state *pS = PcntlState(pVm,0);
	sxi32 rc = SXRET_OK, rcUnwind = SXRET_OK;
	int nEnd, bUnwound = 0;
	if( pS == 0 ){
		/* Nothing can consume the flag: drop it rather than spin on it. */
		PH7_PcntlAsyncPending = 0;
		return SXRET_OK;
	}
	if( pS->bDraining ){
		/* Re-entered from inside a handler -- php keeps the same guard, and
		 * monolog's SignalHandler calls pcntl_signal_dispatch() from within
		 * one. Everything queued since the outer drain started is past its
		 * snapshot and belongs to the NEXT dispatch, so there is nothing to do
		 * here and the flag is left as the outer call left it. */
		return SXRET_OK;
	}
	pS->bDraining = 1;
	PH7_PcntlAsyncPending = 0;
	/* SNAPSHOT the tail. php detaches the whole queue before it runs anything
	 * (`PCNTL_G(head) = NULL`) precisely so that a handler which raises the
	 * signal it is handling lands on a FRESH queue, dispatched by the NEXT
	 * call -- reading the live tail each round instead makes such a handler
	 * loop forever, which is what the first cut of this did. */
	nEnd = (int)nPcntlTail;
	while( (int)nPcntlHead != nEnd ){
		pcntl_event sEv = aPcntlQueue[((int)nPcntlHead) % PCNTL_QUEUE];
		ph7_value *pArgs[2];
		ph7_value sRes, sCb;
		nPcntlHead = (sig_atomic_t)(((int)nPcntlHead + 1) % (2 * PCNTL_QUEUE));
		if( sEv.signo < 1 || sEv.signo >= PCNTL_NSIG ){
			continue;
		}
		if( bUnwound || !pS->aSet[sEv.signo] ){
			continue;
		}
		/* SIG_DFL / SIG_IGN are stored as INTEGERS, and php skips those rather
		 * than calling them -- a disposition that changed between the delivery
		 * and the drain is therefore honoured at the drain. */
		if( (pS->aHandler[sEv.signo].iFlags & (MEMOBJ_INT|MEMOBJ_NULL)) != 0 ){
			continue;
		}
		pArgs[0] = ph7_new_scalar(pVm);
		pArgs[1] = ph7_new_array(pVm);
		if( pArgs[0] == 0 || pArgs[1] == 0 ){
			if( pArgs[0] ){ ph7_release_value(pVm,pArgs[0]); }
			if( pArgs[1] ){ ph7_release_value(pVm,pArgs[1]); }
			break;
		}
		ph7_value_int64(pArgs[0],(ph7_int64)sEv.signo);
		PcntlSigInfoArray(pVm,pArgs[1],sEv.signo,&sEv.sInfo);
		PH7_MemObjInit(pVm,&sRes);
		/* Call a COPY of the handler, not the slot. A handler is allowed to
		 * re-install the signal it is handling -- monolog's SignalHandler puts
		 * SIG_DFL back, re-raises, and then restores itself, all from inside
		 * this call -- and that releases the very value being dispatched. */
		PH7_MemObjInit(pVm,&sCb);
		PH7_MemObjStore(&pS->aHandler[sEv.signo],&sCb);
		rc = PH7_VmCallUserFunction(pVm,&sCb,2,pArgs,&sRes);
		PH7_MemObjRelease(&sCb);
		PH7_MemObjRelease(&sRes);
		ph7_release_value(pVm,pArgs[0]);
		ph7_release_value(pVm,pArgs[1]);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
			/* php CONSUMES the rest of the detached queue without calling
			 * anything: zend_call_function refuses to run while an exception is
			 * pending, and the nodes are recycled anyway. So a signal that was
			 * queued behind a handler that threw is DROPPED, not deferred --
			 * measured against the oracle, because keeping it looked like the
			 * kinder answer and is not php's. */
			rcUnwind = rc;
			bUnwound = 1;
		}
		rc = SXRET_OK;
	}
	pS->bDraining = 0;
	/* A delivery that landed DURING the drain is past the snapshot and raised
	 * the flag from the C handler itself, so it needs nothing from here. */
	return rcUnwind;
}
/*
 * The executor's door (VmLoopFetch). The status a handler raised is already
 * parked on the VM by PH7_VmCallUserFunction, and the fetch-point router that
 * called this runs immediately after -- so nothing has to be returned here.
 */
PH7_PRIVATE void PH7_PcntlDrainAsync(ph7_vm *pVm)
{
	PcntlDrain(pVm);
}
/*
 * VM teardown. php's request shutdown puts every signal it took over back to
 * SIG_DFL; without that a still-installed handler would fire into a VM whose
 * allocator has been released.
 */
PH7_PRIVATE void PH7_PcntlVmRelease(ph7_vm *pVm)
{
	pcntl_state *pS = (pcntl_state *)pVm->pPcntl;
	int i;
	if( pS == 0 ){
		return;
	}
	/* php's request shutdown puts EVERY signal its table names back to SIG_DFL,
	 * not just the ones it installed a C handler for -- a SIG_IGN a script left
	 * behind is undone too. */
	for( i = 1 ; i < PCNTL_NSIG ; ++i ){
		if( pS->aSet[i] ){
			PcntlInstall(i,(void *)SIG_DFL,1,0);
			PH7_MemObjRelease(&pS->aHandler[i]);
		}
	}
	pVm->pPcntl = 0;
	if( pPcntlVm == pVm ){
		pPcntlVm = 0;
		bPcntlAsyncFlag = 0;
		PH7_PcntlAsyncPending = 0;
		nPcntlHead = nPcntlTail = 0;
	}
	SyMemBackendFree(&pVm->sAllocator,pS);
}

/* --- Screens ------------------------------------------------------------ */

/*
 * php's signal-number screen, and it is not one screen: pcntl_signal() words
 * its two bounds separately ("must be greater than or equal to 1" and "must be
 * less than 65") where pcntl_signal_get_handler() words them as one ("must be
 * between 1 and 64"). Reproduced as php has it -- the difference is visible.
 */
static int PcntlSigArg(ph7_context *pCtx,ph7_value *pArg,int bRange,int *pSigno)
{
	sxi64 iSig = ph7_value_to_int64(pArg);
	if( bRange ){
		if( iSig < 1 || iSig >= PCNTL_NSIG ){
			PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #1 ($signal) must be between 1 and %d",
				ph7_function_name(pCtx),PCNTL_NSIG - 1);
			return 0;
		}
	}else{
		if( iSig < 1 ){
			PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #1 ($signal) must be greater than or equal to 1",
				ph7_function_name(pCtx));
			return 0;
		}
		if( iSig >= PCNTL_NSIG ){
			PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #1 ($signal) must be less than %d",
				ph7_function_name(pCtx),PCNTL_NSIG);
			return 0;
		}
	}
	*pSigno = (int)iSig;
	return 1;
}
/*
 * The errno tables php wrote by hand, one per call. Every one of them is an
 * E_WARNING that answers false, and every one stores the errno first.
 */
static void PcntlWarnErrno(ph7_context *pCtx,int iErr,const char *zWhy)
{
	PcntlStoreErr(pCtx->pVm,iErr);
	if( zWhy ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Error %d: %s",iErr,zWhy);
	}else{
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Error %d",iErr);
	}
}

/* --- fork, exec and the wait family ------------------------------------- */

/* int pcntl_fork() */
PH7_PRIVATE int PH7_builtin_pcntl_fork(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	pid_t id;
	SXUNUSED(nArg); SXUNUSED(apArg);
	id = fork();
	if( id == 0 ){
#if defined(PH7_ENABLE_THREADS)
		/* Only this thread survived, and it owns none of the recursive mutexes
		 * the parent was holding -- see PH7_LibForkChild, without which a child
		 * that merely calls exit() blocks on a futex for good. */
		PH7_LibForkChild();
#endif
	}
	if( id == -1 ){
		switch( errno ){
		case EAGAIN:
			PcntlWarnErrno(pCtx,errno,"Reached the maximum limit of number of processes");
			break;
		case ENOMEM:
			PcntlWarnErrno(pCtx,errno,"Insufficient memory");
			break;
#ifdef ENOSYS
		case ENOSYS:
			PcntlWarnErrno(pCtx,errno,"Unimplemented");
			break;
#endif
		default:
			PcntlWarnErrno(pCtx,errno,0);
			break;
		}
	}
	ph7_result_int64(pCtx,(ph7_int64)id);
	return PH7_OK;
}
/*
 * php's seventeen resource-usage keys, in php's own order -- which is neither
 * the struct's nor alphabetical, and which a program reading the array by
 * position would see.
 */
static void PcntlRusage(ph7_vm *pVm,ph7_value *pArray,struct rusage *pRu)
{
	ph7_value *pVal = ph7_new_scalar(pVm);
	if( pVal == 0 ){
		return;
	}
#define PCNTL_RU(field) \
	ph7_value_int64(pVal,(ph7_int64)pRu->field); \
	ph7_array_add_strkey_elem(pArray,#field,pVal)
	PCNTL_RU(ru_oublock);
	PCNTL_RU(ru_inblock);
	PCNTL_RU(ru_msgsnd);
	PCNTL_RU(ru_msgrcv);
	PCNTL_RU(ru_maxrss);
	PCNTL_RU(ru_ixrss);
	PCNTL_RU(ru_idrss);
	PCNTL_RU(ru_minflt);
	PCNTL_RU(ru_majflt);
	PCNTL_RU(ru_nsignals);
	PCNTL_RU(ru_nvcsw);
	PCNTL_RU(ru_nivcsw);
	PCNTL_RU(ru_nswap);
#undef PCNTL_RU
	ph7_value_int64(pVal,(ph7_int64)pRu->ru_utime.tv_usec);
	ph7_array_add_strkey_elem(pArray,"ru_utime.tv_usec",pVal);
	ph7_value_int64(pVal,(ph7_int64)pRu->ru_utime.tv_sec);
	ph7_array_add_strkey_elem(pArray,"ru_utime.tv_sec",pVal);
	ph7_value_int64(pVal,(ph7_int64)pRu->ru_stime.tv_usec);
	ph7_array_add_strkey_elem(pArray,"ru_stime.tv_usec",pVal);
	ph7_value_int64(pVal,(ph7_int64)pRu->ru_stime.tv_sec);
	ph7_array_add_strkey_elem(pArray,"ru_stime.tv_sec",pVal);
	ph7_release_value(pVm,pVal);
}
/* Write a rusage array back through a by-reference argument. */
static void PcntlStoreRusage(ph7_context *pCtx,ph7_value *pArg,struct rusage *pRu)
{
	ph7_value *pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		return;
	}
	PcntlRusage(pCtx->pVm,pArray,pRu);
	PH7_VmStoreArgByRef(pCtx->pVm,pArg,pArray);
}
/*
 * The shared body of pcntl_waitpid() and pcntl_wait(): the same call with the
 * pid either given or -1, and the same by-reference status and (optional)
 * resource usage. php assigns the status EVEN when the wait failed -- it is
 * zero there, which is what a caller testing `$status !== -1` reads.
 */
static int PcntlWaitBody(ph7_context *pCtx,int nArg,ph7_value **apArg,int iStatusArg,pid_t pid)
{
	int iFlagsArg = iStatusArg + 1;
	int iRuArg = iStatusArg + 2;
	int status = 0;
	int iFlags = 0;
	pid_t id;
	ph7_value *pVal;
	if( nArg > iFlagsArg ){
		iFlags = (int)ph7_value_to_int64(apArg[iFlagsArg]);
	}
	if( nArg > iRuArg ){
		struct rusage sRu;
		SyZero(&sRu,sizeof(sRu));
		id = wait4(pid,&status,iFlags,&sRu);
		if( id < 0 ){
			PcntlStoreErr(pCtx->pVm,errno);
		}
		PcntlStoreRusage(pCtx,apArg[iRuArg],&sRu);
	}else{
		id = waitpid(pid,&status,iFlags);
		if( id < 0 ){
			PcntlStoreErr(pCtx->pVm,errno);
		}
	}
	pVal = ph7_context_new_scalar(pCtx);
	if( pVal ){
		ph7_value_int64(pVal,(ph7_int64)status);
		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iStatusArg],pVal);
	}
	ph7_result_int64(pCtx,(ph7_int64)id);
	return PH7_OK;
}
/* int pcntl_waitpid(int $process_id, &$status, int $flags = 0, &$resource_usage = []) */
PH7_PRIVATE int PH7_builtin_pcntl_waitpid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return PcntlWaitBody(pCtx,nArg,apArg,1,(pid_t)ph7_value_to_int64(apArg[0]));
}
/* int pcntl_wait(&$status, int $flags = 0, &$resource_usage = []) */
PH7_PRIVATE int PH7_builtin_pcntl_wait(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return PcntlWaitBody(pCtx,nArg,apArg,0,(pid_t)-1);
}
/*
 * bool pcntl_waitid(int $idtype = P_ALL, ?int $id = null, &$info = [],
 *                   int $flags = WEXITED, &$resource_usage = [])
 *  waitid() carries no usage of its own, so php answers the fifth argument
 *  with getrusage(RUSAGE_CHILDREN) -- measured against the oracle rather than
 *  assumed, because the two are not the same number for a single child.
 */
PH7_PRIVATE int PH7_builtin_pcntl_waitid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	siginfo_t sInfo;
	int iType = 0, iFlags = WEXITED;
	id_t id = 0;
	SyZero(&sInfo,sizeof(sInfo));
	if( nArg > 0 ){
		iType = (int)ph7_value_to_int64(apArg[0]);
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		id = (id_t)ph7_value_to_int64(apArg[1]);
	}
	if( nArg > 3 ){
		iFlags = (int)ph7_value_to_int64(apArg[3]);
	}
	if( waitid((idtype_t)iType,id,&sInfo,iFlags) != 0 ){
		PcntlStoreErr(pCtx->pVm,errno);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		ph7_value *pArray = ph7_context_new_array(pCtx);
		if( pArray ){
			PcntlSigInfoArray(pCtx->pVm,pArray,sInfo.si_signo,&sInfo);
			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pArray);
		}
	}
	if( nArg > 4 ){
		struct rusage sRu;
		SyZero(&sRu,sizeof(sRu));
		getrusage(RUSAGE_CHILDREN,&sRu);
		PcntlStoreRusage(pCtx,apArg[4],&sRu);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * The seven status questions. php applies the C macro and hands back whatever
 * it answers -- there is no screen for "this status is not that kind", so
 * pcntl_wstopsig() of an EXITED status answers the exit code.
 */
PH7_PRIVATE int PH7_builtin_pcntl_wifexited(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int status = (int)ph7_value_to_int64(apArg[0]);
	SXUNUSED(nArg);
	ph7_result_bool(pCtx,WIFEXITED(status) ? 1 : 0);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_pcntl_wifstopped(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int status = (int)ph7_value_to_int64(apArg[0]);
	SXUNUSED(nArg);
	ph7_result_bool(pCtx,WIFSTOPPED(status) ? 1 : 0);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_pcntl_wifsignaled(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int status = (int)ph7_value_to_int64(apArg[0]);
	SXUNUSED(nArg);
	ph7_result_bool(pCtx,WIFSIGNALED(status) ? 1 : 0);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_pcntl_wifcontinued(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int status = (int)ph7_value_to_int64(apArg[0]);
	SXUNUSED(nArg);
#ifdef WIFCONTINUED
	ph7_result_bool(pCtx,WIFCONTINUED(status) ? 1 : 0);
#else
	ph7_result_bool(pCtx,0);
#endif
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_pcntl_wexitstatus(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int status = (int)ph7_value_to_int64(apArg[0]);
	SXUNUSED(nArg);
	ph7_result_int64(pCtx,(ph7_int64)WEXITSTATUS(status));
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_pcntl_wtermsig(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int status = (int)ph7_value_to_int64(apArg[0]);
	SXUNUSED(nArg);
	ph7_result_int64(pCtx,(ph7_int64)WTERMSIG(status));
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_pcntl_wstopsig(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int status = (int)ph7_value_to_int64(apArg[0]);
	SXUNUSED(nArg);
	ph7_result_int64(pCtx,(ph7_int64)WSTOPSIG(status));
	return PH7_OK;
}

/* --- pcntl_exec --------------------------------------------------------- */

/*
 * Append one NUL-terminated string to the argument block and remember where it
 * starts. The block is built first and the pointers taken afterwards, because
 * every append can move it.
 */
static int PcntlPushArg(SyBlob *pBuf,SySet *pOff,const char *zStr,int nStr)
{
	sxu32 nAt = SyBlobLength(pBuf);
	if( SySetPut(pOff,(const void *)&nAt) != SXRET_OK ){
		return 0;
	}
	if( nStr > 0 && SyBlobAppend(pBuf,zStr,(sxu32)nStr) != SXRET_OK ){
		return 0;
	}
	return SyBlobAppend(pBuf,"\0",1) == SXRET_OK;
}
/*
 * false pcntl_exec(string $path, array $args = [], array $env_vars = [])
 *  php puts the PATH itself in argv[0] and the caller's $args after it, so a
 *  program spelling its own argv[0] cannot. A null byte anywhere is a
 *  ValueError with php's three different sentences -- one for the path, one for
 *  an argument, and two for an environment entry's halves.
 */
PH7_PRIVATE int PH7_builtin_pcntl_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyBlob sBuf, sEnvBuf;
	SySet aOff, aEnvOff;
	const char *zPath;
	int nPath = 0, rc = PH7_OK;
	char **apArgv = 0, **apEnvp = 0;
	sxu32 n, nArgv, nEnvp = 0;
	ph7_vm *pVm = pCtx->pVm;
	zPath = ph7_value_to_string(apArg[0],&nPath);
	if( nPath > 0 && SyByteFind(zPath,(sxu32)nPath,'\0',0) == SXRET_OK ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($path) must not contain any null bytes",
			ph7_function_name(pCtx));
	}
	SyBlobInit(&sBuf,&pVm->sAllocator);
	SyBlobInit(&sEnvBuf,&pVm->sAllocator);
	SySetInit(&aOff,&pVm->sAllocator,sizeof(sxu32));
	SySetInit(&aEnvOff,&pVm->sAllocator,sizeof(sxu32));
	if( !PcntlPushArg(&sBuf,&aOff,zPath,nPath) ){
		goto Oom;
	}
	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){
		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;
		ph7_hashmap_node *pNode;
		pMap->pCur = pMap->pFirst;
		while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){
			ph7_value sVal;
			const char *zVal;
			int nVal = 0, bBad;
			sxi32 rcSv;
			PH7_MemObjInit(pVm,&sVal);
			PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);
			/* php's `(string)` cast, not the silent embedder one: an array
			 * argument is `Array` behind an `Array to string conversion`
			 * warning (once per element), and an object with no __toString()
			 * stops the call with php's Error. */
			rcSv = PH7_ValueToStringUV(pCtx,&sVal,&zVal,&nVal);
			if( rcSv != SXRET_OK ){
				PH7_MemObjRelease(&sVal);
				rc = rcSv;
				goto Done;
			}
			bBad = (nVal > 0 && SyByteFind(zVal,(sxu32)nVal,'\0',0) == SXRET_OK);
			if( !bBad && !PcntlPushArg(&sBuf,&aOff,zVal,nVal) ){
				PH7_MemObjRelease(&sVal);
				goto Oom;
			}
			PH7_MemObjRelease(&sVal);
			if( bBad ){
				rc = PH7_VmThrowException(pCtx,"ValueError",
					"%s(): Argument #2 ($args) individual argument must not contain null bytes",
					ph7_function_name(pCtx));
				goto Done;
			}
		}
	}
	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){
		ph7_hashmap *pMap = (ph7_hashmap *)apArg[2]->x.pOther;
		ph7_hashmap_node *pNode;
		pMap->pCur = pMap->pFirst;
		while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){
			ph7_value sKey, sVal;
			const char *zKey, *zVal;
			int nKey = 0, nVal = 0;
			sxu32 nAt;
			sxi32 rcSv;
			PH7_MemObjInit(pVm,&sKey);
			PH7_MemObjInit(pVm,&sVal);
			PH7_HashmapExtractNodeKey(pNode,&sKey);
			PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);
			/* A hash KEY is already an int or a string, so nothing about it can
			 * warn -- only the VALUE takes the user-visible cast. */
			zKey = ph7_value_to_string(&sKey,&nKey);
			if( nKey > 0 && SyByteFind(zKey,(sxu32)nKey,'\0',0) == SXRET_OK ){
				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);
				rc = PH7_VmThrowException(pCtx,"ValueError",
					"%s(): Argument #3 ($env_vars) name for environment variable must not contain null bytes",
					ph7_function_name(pCtx));
				goto Done;
			}
			rcSv = PH7_ValueToStringUV(pCtx,&sVal,&zVal,&nVal);
			if( rcSv != SXRET_OK ){
				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);
				rc = rcSv;
				goto Done;
			}
			if( nVal > 0 && SyByteFind(zVal,(sxu32)nVal,'\0',0) == SXRET_OK ){
				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);
				rc = PH7_VmThrowException(pCtx,"ValueError",
					"%s(): Argument #3 ($env_vars) value for environment variable must not contain null bytes",
					ph7_function_name(pCtx));
				goto Done;
			}
			nAt = SyBlobLength(&sEnvBuf);
			if( SySetPut(&aEnvOff,(const void *)&nAt) != SXRET_OK
			 || (nKey > 0 && SyBlobAppend(&sEnvBuf,zKey,(sxu32)nKey) != SXRET_OK)
			 || SyBlobAppend(&sEnvBuf,"=",1) != SXRET_OK
			 || (nVal > 0 && SyBlobAppend(&sEnvBuf,zVal,(sxu32)nVal) != SXRET_OK)
			 || SyBlobAppend(&sEnvBuf,"\0",1) != SXRET_OK ){
				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);
				goto Oom;
			}
			PH7_MemObjRelease(&sKey);
			PH7_MemObjRelease(&sVal);
		}
		nEnvp = SySetUsed(&aEnvOff);
	}
	nArgv = SySetUsed(&aOff);
	apArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,(nArgv + 1) * sizeof(char *));
	if( apArgv == 0 ){
		goto Oom;
	}
	for( n = 0 ; n < nArgv ; ++n ){
		apArgv[n] = (char *)SyBlobData(&sBuf) + ((sxu32 *)SySetBasePtr(&aOff))[n];
	}
	apArgv[nArgv] = 0;
	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){
		apEnvp = (char **)SyMemBackendAlloc(&pVm->sAllocator,(nEnvp + 1) * sizeof(char *));
		if( apEnvp == 0 ){
			goto Oom;
		}
		for( n = 0 ; n < nEnvp ; ++n ){
			apEnvp[n] = (char *)SyBlobData(&sEnvBuf) + ((sxu32 *)SySetBasePtr(&aEnvOff))[n];
		}
		apEnvp[nEnvp] = 0;
	}
	/* Anything this program still owns is about to stop existing, so nothing
	 * below the exec has to be tidy -- but the exec can FAIL, and then it does. */
	if( apEnvp ){
		execve(apArgv[0],apArgv,apEnvp);
	}else{
		execv(apArgv[0],apArgv);
	}
	PcntlStoreErr(pVm,errno);
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
		"Error has occurred: (errno %d) %s",errno,strerror(errno));
	ph7_result_bool(pCtx,0);
	goto Done;
Oom:
	ph7_result_bool(pCtx,0);
Done:
	if( apArgv ){ SyMemBackendFree(&pVm->sAllocator,apArgv); }
	if( apEnvp ){ SyMemBackendFree(&pVm->sAllocator,apEnvp); }
	SyBlobRelease(&sBuf);
	SyBlobRelease(&sEnvBuf);
	SySetRelease(&aOff);
	SySetRelease(&aEnvOff);
	return rc;
}

/* --- The signal surface -------------------------------------------------- */

/*
 * bool pcntl_signal(int $signal, callable|int $handler, bool $restart_syscalls = true)
 *  An INTEGER handler may only be SIG_DFL or SIG_IGN, and anything that is
 *  neither an integer nor a callable is a TypeError naming the union. A
 *  disposition sigaction() refuses -- SIGKILL, SIGSTOP -- is php's E_ERROR
 *  rather than a warning, and it is not catchable there either.
 */
PH7_PRIVATE int PH7_builtin_pcntl_signal(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	pcntl_state *pS;
	int signo = 0, bRestart = 1;
	char zGiven[64];
	if( !PcntlSigArg(pCtx,apArg[0],0,&signo) ){
		return PH7_OK;
	}
	if( nArg > 2 ){
		bRestart = ph7_value_to_bool(apArg[2]);
	}
	pS = PcntlState(pCtx->pVm,1);
	if( pS == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( ph7_value_is_int(apArg[1]) ){
		sxi64 iDisp = ph7_value_to_int64(apArg[1]);
		if( iDisp != (sxi64)(sxptr)SIG_DFL && iDisp != (sxi64)(sxptr)SIG_IGN ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #2 ($handler) must be either SIG_DFL or SIG_IGN "
				"when an integer value is given",ph7_function_name(pCtx));
		}
		if( PcntlInstall(signo,(void *)(sxptr)iDisp,bRestart,0) != 0 ){
			PcntlStoreErr(pCtx->pVm,errno);
			PH7_VmSignalInstallFatal(pCtx->pVm,signo);
			return PH7_ABORT;
		}
		PH7_MemObjRelease(&pS->aHandler[signo]);
		PH7_MemObjStore(apArg[1],&pS->aHandler[signo]);
		pS->aSet[signo] = 1;
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( !ph7_value_is_callable(apArg[1]) ){
		/* php stores EINVAL BEFORE it throws here -- a refusal that leaves a
		 * remembered errno behind, which most of the other screens do not. */
		PcntlStoreErr(pCtx->pVm,EINVAL);
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #2 ($handler) must be of type callable|int, %s given",
			ph7_function_name(pCtx),VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));
	}
	PH7_MemObjRelease(&pS->aHandler[signo]);
	PH7_MemObjStore(apArg[1],&pS->aHandler[signo]);
	pS->aSet[signo] = 1;
	/* Every other signal is blocked for the duration of the C handler (php's
	 * mask_all), so the ring can never be re-entered mid-store. */
	if( PcntlInstall(signo,(void *)PcntlSigHandler,bRestart,1) != 0 ){
		PcntlStoreErr(pCtx->pVm,errno);
		PH7_VmSignalInstallFatal(pCtx->pVm,signo);
		return PH7_ABORT;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * callable|int pcntl_signal_get_handler(int $signal)
 *  A signal nobody has spoken for answers SIG_DFL -- the same 0 a signal
 *  explicitly set to SIG_DFL answers, which is why monolog can save and restore
 *  a disposition without knowing which of the two it had.
 */
PH7_PRIVATE int PH7_builtin_pcntl_signal_get_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	pcntl_state *pS;
	int signo = 0;
	SXUNUSED(nArg);
	if( !PcntlSigArg(pCtx,apArg[0],1,&signo) ){
		return PH7_OK;
	}
	pS = PcntlState(pCtx->pVm,0);
	if( pS == 0 || !pS->aSet[signo] ){
		ph7_result_int64(pCtx,(ph7_int64)(sxptr)SIG_DFL);
		return PH7_OK;
	}
	ph7_result_value(pCtx,&pS->aHandler[signo]);
	return PH7_OK;
}
/* true pcntl_signal_dispatch() */
PH7_PRIVATE int PH7_builtin_pcntl_signal_dispatch(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	SXUNUSED(nArg); SXUNUSED(apArg);
	rc = PcntlDrain(pCtx->pVm);
	if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
		return rc;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool pcntl_async_signals(?bool $enable = null)
 *  Answers the OLD setting when it changes one and the CURRENT one when asked
 *  with null -- which is how a test saves what it found and puts it back.
 */
PH7_PRIVATE int PH7_builtin_pcntl_async_signals(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	pcntl_state *pS = PcntlState(pCtx->pVm,1);
	int bOld;
	if( pS == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	bOld = pS->bAsync;
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		pS->bAsync = ph7_value_to_bool(apArg[0]) ? 1 : 0;
		bPcntlAsyncFlag = (sig_atomic_t)pS->bAsync;
		/* Turning async ON does NOT flush what is already queued: php raises
		 * its interrupt from the SIGNAL HANDLER and nowhere else, so a delivery
		 * that arrived while async was off waits for the next
		 * pcntl_signal_dispatch() or the next signal. Measured, not assumed --
		 * this is the one place the first cut of this extension invented a
		 * behaviour php does not have. */
	}
	ph7_result_bool(pCtx,bOld);
	return PH7_OK;
}
/*
 * Read an array of signal numbers into a sigset_t. php screens each element
 * itself -- an int outside 1..NSIG-1 is a ValueError and a non-int is a
 * TypeError, both naming the ARRAY's parameter rather than the element.
 */
static int PcntlSigSet(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zParam,sigset_t *pSet)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode;
	char zGiven[64];
	sigemptyset(pSet);
	if( !ph7_value_is_array(pArg) ){
		return 1;
	}
	pMap = (ph7_hashmap *)pArg->x.pOther;
	pMap->pCur = pMap->pFirst;
	while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){
		ph7_value sVal;
		sxi64 iSig;
		PH7_MemObjInit(pCtx->pVm,&sVal);
		PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);
		if( !ph7_value_is_int(&sVal) ){
			PH7_VmThrowException(pCtx,"TypeError",
				"%s(): Argument #%d ($%s) signals must be of type int, %s given",
				ph7_function_name(pCtx),iPos,zParam,
				VmValueGivenName(&sVal,zGiven,sizeof(zGiven)));
			PH7_MemObjRelease(&sVal);
			return 0;
		}
		iSig = ph7_value_to_int64(&sVal);
		PH7_MemObjRelease(&sVal);
		if( iSig < 1 || iSig >= PCNTL_NSIG ){
			PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #%d ($%s) signals must be between 1 and %d",
				ph7_function_name(pCtx),iPos,zParam,PCNTL_NSIG - 1);
			return 0;
		}
		sigaddset(pSet,(int)iSig);
	}
	return 1;
}
/* The signals a set holds, as php's list of numbers in ascending order. */
static void PcntlSetToArray(ph7_context *pCtx,ph7_value *pArg,const sigset_t *pSet)
{
	ph7_value *pArray = ph7_context_new_array(pCtx);
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	int i;
	if( pArray == 0 || pVal == 0 ){
		return;
	}
	for( i = 1 ; i < PCNTL_NSIG ; ++i ){
		if( sigismember(pSet,i) != 1 ){
			continue;
		}
		ph7_value_int64(pVal,(ph7_int64)i);
		ph7_array_add_elem(pArray,0,pVal);
	}
	PH7_VmStoreArgByRef(pCtx->pVm,pArg,pArray);
}
/* bool pcntl_sigprocmask(int $mode, array $signals, &$old_signals = null) */
PH7_PRIVATE int PH7_builtin_pcntl_sigprocmask(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sigset_t sSet, sOld;
	int iMode = (int)ph7_value_to_int64(apArg[0]);
	SXUNUSED(nArg);
	if( iMode != SIG_BLOCK && iMode != SIG_UNBLOCK && iMode != SIG_SETMASK ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($mode) must be one of SIG_BLOCK, SIG_UNBLOCK, or SIG_SETMASK",
			ph7_function_name(pCtx));
	}
	if( !PcntlSigSet(pCtx,apArg[1],2,"signals",&sSet) ){
		return PH7_OK;
	}
	sigemptyset(&sOld);
	if( sigprocmask(iMode,&sSet,&sOld) != 0 ){
		PcntlStoreErr(pCtx->pVm,errno);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Error %d",errno);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		PcntlSetToArray(pCtx,apArg[2],&sOld);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
#ifndef __APPLE__
/*
 * The shared body of pcntl_sigwaitinfo() and pcntl_sigtimedwait(), which php
 * builds only where the system has them: macOS has neither. php stores
 * an errno for every failure EXCEPT the timed wait's own EAGAIN, which is that
 * call saying "nothing arrived" rather than failing.
 */
static int PcntlSigWait(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTimed)
{
	sigset_t sSet;
	siginfo_t sInfo;
	int iSig;
	if( !PcntlSigSet(pCtx,apArg[0],1,"signals",&sSet) ){
		return PH7_OK;
	}
	SyZero(&sInfo,sizeof(sInfo));
	if( bTimed ){
		struct timespec sTs;
		sxi64 iSec = 0, iNsec = 0;
		if( nArg > 2 ){
			iSec = ph7_value_to_int64(apArg[2]);
		}
		if( nArg > 3 ){
			iNsec = ph7_value_to_int64(apArg[3]);
		}
		if( iSec < 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #3 ($seconds) must be greater than or equal to 0",
				ph7_function_name(pCtx));
		}
		if( iNsec < 0 || iNsec > 1000000000 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #4 ($nanoseconds) must be between 0 and 1e9",
				ph7_function_name(pCtx));
		}
		if( iSec == 0 && iNsec == 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): At least one of argument #3 ($seconds) or argument #4 "
				"($nanoseconds) must be greater than 0",ph7_function_name(pCtx));
		}
		sTs.tv_sec = (time_t)iSec;
		sTs.tv_nsec = (long)iNsec;
		iSig = sigtimedwait(&sSet,&sInfo,&sTs);
	}else{
		iSig = sigwaitinfo(&sSet,&sInfo);
	}
	if( iSig < 0 ){
		if( errno != EAGAIN ){
			PcntlStoreErr(pCtx->pVm,errno);
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		ph7_value *pArray = ph7_context_new_array(pCtx);
		if( pArray ){
			PcntlSigInfoArray(pCtx->pVm,pArray,iSig,&sInfo);
			PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pArray);
		}
	}
	ph7_result_int64(pCtx,(ph7_int64)iSig);
	return PH7_OK;
}
/* int|false pcntl_sigwaitinfo(array $signals, &$info = []) */
PH7_PRIVATE int PH7_builtin_pcntl_sigwaitinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return PcntlSigWait(pCtx,nArg,apArg,0);
}
/* int|false pcntl_sigtimedwait(array $signals, &$info = [], int $seconds = 0, int $nanoseconds = 0) */
PH7_PRIVATE int PH7_builtin_pcntl_sigtimedwait(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return PcntlSigWait(pCtx,nArg,apArg,1);
}
#endif /* __APPLE__ */
/*
 * int pcntl_alarm(int $seconds)
 *  Answers how many seconds were left on the PREVIOUS alarm, which is what a
 *  program cancelling one reads back.
 */
PH7_PRIVATE int PH7_builtin_pcntl_alarm(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	ph7_result_int64(pCtx,(ph7_int64)alarm((unsigned int)ph7_value_to_int64(apArg[0])));
	return PH7_OK;
}

/* --- Priority ------------------------------------------------------------ */

/* php's three-way $mode screen, shared by the getter and the setter. */
static int PcntlPrioMode(ph7_context *pCtx,ph7_value *pArg,int iPos,int *pMode)
{
	sxi64 iMode = ph7_value_to_int64(pArg);
	if( iMode != PRIO_PGRP && iMode != PRIO_USER && iMode != PRIO_PROCESS ){
		/* php remembers EINVAL for this one too, before the ValueError. */
		PcntlStoreErr(pCtx->pVm,EINVAL);
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($mode) must be one of PRIO_PGRP, PRIO_USER, or PRIO_PROCESS",
			ph7_function_name(pCtx),iPos);
		return 0;
	}
	*pMode = (int)iMode;
	return 1;
}
/* int|false pcntl_getpriority(?int $process_id = null, int $mode = PRIO_PROCESS) */
PH7_PRIVATE int PH7_builtin_pcntl_getpriority(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iMode = PRIO_PROCESS, iPri;
	id_t id = 0;
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		id = (id_t)ph7_value_to_int64(apArg[0]);
	}
	if( nArg > 1 && !PcntlPrioMode(pCtx,apArg[1],2,&iMode) ){
		return PH7_OK;
	}
	/* getpriority() answers -1 for a real priority too, so errno is the only
	 * way to tell a failure from a nice value of -1. */
	errno = 0;
	iPri = getpriority(iMode,id);
	if( errno != 0 ){
		if( errno == ESRCH ){
			PcntlWarnErrno(pCtx,errno,"No process was located using the given parameters");
		}else{
			PcntlStoreErr(pCtx->pVm,errno);
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Unknown error %d has occurred",errno);
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,(ph7_int64)iPri);
	return PH7_OK;
}
/* bool pcntl_setpriority(int $priority, ?int $process_id = null, int $mode = PRIO_PROCESS) */
PH7_PRIVATE int PH7_builtin_pcntl_setpriority(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iMode = PRIO_PROCESS;
	int iPri = (int)ph7_value_to_int64(apArg[0]);
	id_t id = 0;
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		id = (id_t)ph7_value_to_int64(apArg[1]);
	}
	if( nArg > 2 && !PcntlPrioMode(pCtx,apArg[2],3,&iMode) ){
		return PH7_OK;
	}
	if( setpriority(iMode,id,iPri) != 0 ){
		switch( errno ){
		case ESRCH:
			PcntlWarnErrno(pCtx,errno,"No process was located using the given parameters");
			break;
		case EPERM:
			PcntlWarnErrno(pCtx,errno,
				"A process was located, but neither its effective nor real user ID "
				"matched the effective user ID of the caller");
			break;
		case EACCES:
			PcntlWarnErrno(pCtx,errno,
				"Only a super user may attempt to increase the process priority");
			break;
		default:
			PcntlStoreErr(pCtx->pVm,errno);
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Unknown error %d has occurred",errno);
			break;
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* --- The remembered errno, read back ------------------------------------- */

/* int pcntl_get_last_error() / int pcntl_errno() */
PH7_PRIVATE int PH7_builtin_pcntl_get_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	pcntl_state *pS = PcntlState(pCtx->pVm,0);
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,(ph7_int64)(pS ? pS->iLastError : 0));
	return PH7_OK;
}
/* string pcntl_strerror(int $error_code) */
PH7_PRIVATE int PH7_builtin_pcntl_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zMsg = strerror((int)ph7_value_to_int64(apArg[0]));
	SXUNUSED(nArg);
	ph7_result_string(pCtx,zMsg ? zMsg : "",-1);
	return PH7_OK;
}

/* --- The Linux-only half -------------------------------------------------- */

#ifdef __linux__
/* bool pcntl_unshare(int $flags) */
PH7_PRIVATE int PH7_builtin_pcntl_unshare(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iFlags = (int)ph7_value_to_int64(apArg[0]);
	SXUNUSED(nArg);
	if( unshare(iFlags) != 0 ){
		PcntlStoreErr(pCtx->pVm,errno);
		switch( errno ){
		case EINVAL:
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #1 ($flags) must be a combination of CLONE_* flags, "
				"or at least one flag is unsupported by the kernel",
				ph7_function_name(pCtx));
		case ENOMEM:
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Error %d: Insufficient memory for unshare",errno);
			break;
		case EPERM:
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Error %d: No privilege to use these flags",errno);
			break;
#ifdef EUSERS
		case EUSERS:
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Error %d: Reached the maximum nesting limit for the user namespace",errno);
			break;
#endif
		case ENOSPC:
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Error %d: Reached the maximum nesting limit for one of the specified namespaces",errno);
			break;
		default:
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Unknown error %d has occurred",errno);
			break;
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* How many CPUs the affinity questions count against. */
static long PcntlNCpus(void)
{
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	return n > 0 ? n : 1;
}
/* The three affinity failures php words, shared by the getter and the setter. */
static int PcntlAffinityFail(ph7_context *pCtx,sxi64 iPid,const char *zSizeMsg)
{
	PcntlStoreErr(pCtx->pVm,errno);
	switch( errno ){
	case EINVAL:
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zSizeMsg);
		break;
	case EPERM:
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Calling process not having the proper privileges");
		break;
	case ESRCH:
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($process_id) invalid process (%qd)",
			ph7_function_name(pCtx),iPid);
	default:
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unknown error %d has occurred",errno);
		break;
	}
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/* array|false pcntl_getcpuaffinity(?int $process_id = null) */
PH7_PRIVATE int PH7_builtin_pcntl_getcpuaffinity(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	cpu_set_t sMask;
	ph7_value *pArray, *pVal;
	sxi64 iPid = 0;
	long i, nCpu = PcntlNCpus();
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		iPid = ph7_value_to_int64(apArg[0]);
	}
	CPU_ZERO(&sMask);
	if( sched_getaffinity((pid_t)iPid,sizeof(sMask),&sMask) != 0 ){
		return PcntlAffinityFail(pCtx,iPid,"invalid cpu affinity mask size");
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	for( i = 0 ; i < nCpu ; ++i ){
		if( !CPU_ISSET((int)i,&sMask) ){
			continue;
		}
		ph7_value_int64(pVal,(ph7_int64)i);
		ph7_array_add_elem(pArray,0,pVal);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool pcntl_setcpuaffinity(?int $process_id = null, array $cpu_ids = [])
 *  A cpu id may be an int or a NUMERIC STRING; anything else is a TypeError,
 *  and a string that is not a whole number is a ValueError naming it. php's
 *  range sentence prints the CPU COUNT where the last valid id is one less --
 *  reproduced, because it is what a program's error output would show.
 */
PH7_PRIVATE int PH7_builtin_pcntl_setcpuaffinity(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	cpu_set_t sMask;
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode;
	sxi64 iPid = 0;
	long nCpu = PcntlNCpus();
	char zGiven[64];
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		iPid = ph7_value_to_int64(apArg[0]);
	}
	CPU_ZERO(&sMask);
	if( nArg < 2 || !ph7_value_is_array(apArg[1]) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	pMap->pCur = pMap->pFirst;
	while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){
		ph7_value sVal;
		sxi64 iCpu;
		PH7_MemObjInit(pCtx->pVm,&sVal);
		PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);
		if( ph7_value_is_string(&sVal) ){
			if( !PH7_MemObjStringIsNumeric(&sVal) ){
				int nStr = 0;
				const char *zStr = ph7_value_to_string(&sVal,&nStr);
				PH7_VmThrowException(pCtx,"ValueError",
					"%s(): Argument #2 ($cpu_ids) cpu id invalid value (%.*s)",
					ph7_function_name(pCtx),nStr,zStr);
				PH7_MemObjRelease(&sVal);
				return PH7_OK;
			}
		}else if( !ph7_value_is_int(&sVal) ){
			PH7_VmThrowException(pCtx,"TypeError",
				"%s(): Argument #2 ($cpu_ids) value must be of type int|string, %s given",
				ph7_function_name(pCtx),VmValueGivenName(&sVal,zGiven,sizeof(zGiven)));
			PH7_MemObjRelease(&sVal);
			return PH7_OK;
		}
		iCpu = ph7_value_to_int64(&sVal);
		PH7_MemObjRelease(&sVal);
		if( iCpu < 0 || iCpu >= (sxi64)nCpu ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #2 ($cpu_ids) cpu id must be between 0 and %qd (%qd)",
				ph7_function_name(pCtx),(sxi64)nCpu,iCpu);
		}
		CPU_SET((int)iCpu,&sMask);
	}
	if( sched_setaffinity((pid_t)iPid,sizeof(sMask),&sMask) != 0 ){
		return PcntlAffinityFail(pCtx,iPid,"invalid cpu affinity mask size or unmapped cpu id(s)");
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* int pcntl_getcpu() */
PH7_PRIVATE int PH7_builtin_pcntl_getcpu(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,(ph7_int64)sched_getcpu());
	return PH7_OK;
}
#endif /* __linux__ */

/* --- The constants -------------------------------------------------------- */

/*
 * Every one of these is the PLATFORM's macro rather than a number copied out of
 * one build: SIGUSR1 is 10 on Linux and 30 on macOS, and a script that stores
 * one and hands it back to posix_kill() has to get its own system's answer.
 * php defines none of them on Windows, so neither does this.
 *
 * Two are missing from the table on purpose: SIGRTMIN and SIGRTMAX are function
 * CALLS on glibc, not constants, so the registration below adds them by hand.
 */
static const struct {
	const char *zName;
	int iValue;
} aPcntlConst[] = {
	/* The wait flags */
	{ "WNOHANG",    WNOHANG    },
	{ "WUNTRACED",  WUNTRACED  },
#ifdef WCONTINUED
	{ "WCONTINUED", WCONTINUED },
#endif
#ifdef WEXITED
	{ "WEXITED",    WEXITED    },
#endif
#ifdef WSTOPPED
	{ "WSTOPPED",   WSTOPPED   },
#endif
#ifdef WNOWAIT
	{ "WNOWAIT",    WNOWAIT    },
#endif
#if defined(__linux__) || defined(__APPLE__)
	/* waitid()'s id types are an ENUM rather than macros, so they cannot be
	 * #ifdef'd one by one the way everything else here can. */
	{ "P_ALL",  P_ALL  },
	{ "P_PID",  P_PID  },
	{ "P_PGID", P_PGID },
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 31))
	{ "P_PIDFD", P_PIDFD },
#endif
#endif
	/* The three dispositions. SIG_DFL and friends are function POINTERS. */
	{ "SIG_IGN", (int)(sxptr)SIG_IGN },
	{ "SIG_DFL", (int)(sxptr)SIG_DFL },
	{ "SIG_ERR", (int)(sxptr)SIG_ERR },
	/* The signals */
	{ "SIGHUP",    SIGHUP    },
	{ "SIGINT",    SIGINT    },
	{ "SIGQUIT",   SIGQUIT   },
	{ "SIGILL",    SIGILL    },
	{ "SIGTRAP",   SIGTRAP   },
	{ "SIGABRT",   SIGABRT   },
#ifdef SIGIOT
	{ "SIGIOT",    SIGIOT    },
#endif
	{ "SIGBUS",    SIGBUS    },
	{ "SIGFPE",    SIGFPE    },
	{ "SIGKILL",   SIGKILL   },
	{ "SIGUSR1",   SIGUSR1   },
	{ "SIGSEGV",   SIGSEGV   },
	{ "SIGUSR2",   SIGUSR2   },
	{ "SIGPIPE",   SIGPIPE   },
	{ "SIGALRM",   SIGALRM   },
	{ "SIGTERM",   SIGTERM   },
#ifdef SIGSTKFLT
	{ "SIGSTKFLT", SIGSTKFLT },
#endif
#ifdef SIGCLD
	{ "SIGCLD",    SIGCLD    },
#endif
#ifdef SIGCHLD
	{ "SIGCHLD",   SIGCHLD   },
#endif
	{ "SIGCONT",   SIGCONT   },
	{ "SIGSTOP",   SIGSTOP   },
	{ "SIGTSTP",   SIGTSTP   },
	{ "SIGTTIN",   SIGTTIN   },
	{ "SIGTTOU",   SIGTTOU   },
	{ "SIGURG",    SIGURG    },
	{ "SIGXCPU",   SIGXCPU   },
	{ "SIGXFSZ",   SIGXFSZ   },
	{ "SIGVTALRM", SIGVTALRM },
	{ "SIGPROF",   SIGPROF   },
	{ "SIGWINCH",  SIGWINCH  },
#ifdef SIGPOLL
	{ "SIGPOLL",   SIGPOLL   },
#endif
#ifdef SIGIO
	{ "SIGIO",     SIGIO     },
#endif
#ifdef SIGPWR
	{ "SIGPWR",    SIGPWR    },
#endif
#ifdef SIGSYS
	{ "SIGSYS",    SIGSYS    },
	/* php ships SIGBABY beside SIGSYS -- the SunOS spelling of the same
	 * signal, which glibc does not define and php registers anyway. */
	{ "SIGBABY",   SIGSYS    },
#endif
	/* Priority */
	{ "PRIO_PGRP",    PRIO_PGRP    },
	{ "PRIO_USER",    PRIO_USER    },
	{ "PRIO_PROCESS", PRIO_PROCESS },
	/* Mask modes */
	{ "SIG_BLOCK",   SIG_BLOCK   },
	{ "SIG_UNBLOCK", SIG_UNBLOCK },
	{ "SIG_SETMASK", SIG_SETMASK },
	/* siginfo's `code` field, per origin and per signal */
#ifdef SI_USER
	{ "SI_USER",    SI_USER    },
#endif
#ifdef SI_KERNEL
	{ "SI_KERNEL",  SI_KERNEL  },
#endif
#ifdef SI_QUEUE
	{ "SI_QUEUE",   SI_QUEUE   },
#endif
#ifdef SI_TIMER
	{ "SI_TIMER",   SI_TIMER   },
#endif
#ifdef SI_MESGQ
	{ "SI_MESGQ",   SI_MESGQ   },
#endif
#ifdef SI_ASYNCIO
	{ "SI_ASYNCIO", SI_ASYNCIO },
#endif
#ifdef SI_SIGIO
	{ "SI_SIGIO",   SI_SIGIO   },
#endif
#ifdef SI_TKILL
	{ "SI_TKILL",   SI_TKILL   },
#endif
#ifdef CLD_EXITED
	{ "CLD_EXITED",    CLD_EXITED    },
	{ "CLD_KILLED",    CLD_KILLED    },
	{ "CLD_DUMPED",    CLD_DUMPED    },
	{ "CLD_TRAPPED",   CLD_TRAPPED   },
	{ "CLD_STOPPED",   CLD_STOPPED   },
	{ "CLD_CONTINUED", CLD_CONTINUED },
#endif
#ifdef TRAP_BRKPT
	{ "TRAP_BRKPT", TRAP_BRKPT },
	{ "TRAP_TRACE", TRAP_TRACE },
#endif
#ifdef POLL_IN
	{ "POLL_IN",  POLL_IN  },
	{ "POLL_OUT", POLL_OUT },
	{ "POLL_MSG", POLL_MSG },
	{ "POLL_ERR", POLL_ERR },
	{ "POLL_PRI", POLL_PRI },
	{ "POLL_HUP", POLL_HUP },
#endif
#ifdef ILL_ILLOPC
	{ "ILL_ILLOPC", ILL_ILLOPC },
	{ "ILL_ILLOPN", ILL_ILLOPN },
	{ "ILL_ILLADR", ILL_ILLADR },
	{ "ILL_ILLTRP", ILL_ILLTRP },
	{ "ILL_PRVOPC", ILL_PRVOPC },
	{ "ILL_PRVREG", ILL_PRVREG },
	{ "ILL_COPROC", ILL_COPROC },
	{ "ILL_BADSTK", ILL_BADSTK },
#endif
#ifdef FPE_INTDIV
	{ "FPE_INTDIV", FPE_INTDIV },
	{ "FPE_INTOVF", FPE_INTOVF },
	{ "FPE_FLTDIV", FPE_FLTDIV },
	{ "FPE_FLTOVF", FPE_FLTOVF },
	{ "FPE_FLTUND", FPE_FLTUND },
	{ "FPE_FLTRES", FPE_FLTRES },
	{ "FPE_FLTINV", FPE_FLTINV },
	{ "FPE_FLTSUB", FPE_FLTSUB },
#endif
#ifdef SEGV_MAPERR
	{ "SEGV_MAPERR", SEGV_MAPERR },
	{ "SEGV_ACCERR", SEGV_ACCERR },
#endif
#ifdef BUS_ADRALN
	{ "BUS_ADRALN", BUS_ADRALN },
	{ "BUS_ADRERR", BUS_ADRERR },
	{ "BUS_OBJERR", BUS_OBJERR },
#endif
	/* The namespaces pcntl_unshare() can leave */
#ifdef CLONE_NEWNS
	{ "CLONE_NEWNS",     CLONE_NEWNS     },
#endif
#ifdef CLONE_NEWIPC
	{ "CLONE_NEWIPC",    CLONE_NEWIPC    },
#endif
#ifdef CLONE_NEWUTS
	{ "CLONE_NEWUTS",    CLONE_NEWUTS    },
#endif
#ifdef CLONE_NEWNET
	{ "CLONE_NEWNET",    CLONE_NEWNET    },
#endif
#ifdef CLONE_NEWPID
	{ "CLONE_NEWPID",    CLONE_NEWPID    },
#endif
#ifdef CLONE_NEWUSER
	{ "CLONE_NEWUSER",   CLONE_NEWUSER   },
#endif
#ifdef CLONE_NEWCGROUP
	{ "CLONE_NEWCGROUP", CLONE_NEWCGROUP },
#endif
	/* php's own names for the errnos pcntl_get_last_error() can answer with */
	{ "PCNTL_EINTR",        EINTR        },
	{ "PCNTL_ECHILD",       ECHILD       },
	{ "PCNTL_EINVAL",       EINVAL       },
	{ "PCNTL_EAGAIN",       EAGAIN       },
	{ "PCNTL_ESRCH",        ESRCH        },
	{ "PCNTL_EACCES",       EACCES       },
	{ "PCNTL_EPERM",        EPERM        },
	{ "PCNTL_ENOMEM",       ENOMEM       },
	{ "PCNTL_E2BIG",        E2BIG        },
	{ "PCNTL_EFAULT",       EFAULT       },
	{ "PCNTL_EIO",          EIO          },
	{ "PCNTL_EISDIR",       EISDIR       },
#ifdef ELIBBAD
	{ "PCNTL_ELIBBAD",      ELIBBAD      },
#endif
	{ "PCNTL_ELOOP",        ELOOP        },
	{ "PCNTL_EMFILE",       EMFILE       },
	{ "PCNTL_ENAMETOOLONG", ENAMETOOLONG },
	{ "PCNTL_ENFILE",       ENFILE       },
	{ "PCNTL_ENOENT",       ENOENT       },
	{ "PCNTL_ENOEXEC",      ENOEXEC      },
	{ "PCNTL_ENOTDIR",      ENOTDIR      },
	{ "PCNTL_ETXTBSY",      ETXTBSY      },
	{ "PCNTL_ENOSPC",       ENOSPC       },
#ifdef EUSERS
	{ "PCNTL_EUSERS",       EUSERS       },
#endif
};
/* The shared expander: pUserData carries the value (SX_INT_TO_PTR). */
static void PcntlConstExpand(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,SX_PTR_TO_INT(pUserData));
}
PH7_PRIVATE void PH7_RegisterPcntlConstants(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aPcntlConst) ; ++n ){
		ph7_create_constant(&(*pVm),aPcntlConst[n].zName,PcntlConstExpand,
			SX_INT_TO_PTR(aPcntlConst[n].iValue));
	}
#ifdef SIGRTMIN
	ph7_create_constant(&(*pVm),"SIGRTMIN",PcntlConstExpand,SX_INT_TO_PTR(SIGRTMIN));
#endif
#ifdef SIGRTMAX
	ph7_create_constant(&(*pVm),"SIGRTMAX",PcntlConstExpand,SX_INT_TO_PTR(SIGRTMAX));
#endif
}
/*
 * Pcntl\QosClass: php registers this pure enum on EVERY platform, even though
 * the two functions that read it (pcntl_getqos_class/pcntl_setqos_class) exist
 * only on macOS -- so `enum_exists('Pcntl\QosClass')` is true under a Linux php
 * and has to be true here.
 */
static const char * const azPcntlQos[] = {
	"UserInteractive", "UserInitiated", "Default", "Utility", "Background",
};
PH7_PRIVATE sxi32 PH7_VmInstallPcntl(ph7_vm *pVm)
{
	PH7_NativeEnumCase aCase[SX_ARRAYSIZE(azPcntlQos)];
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(azPcntlQos) ; ++n ){
		aCase[n].zName = azPcntlQos[n];
		aCase[n].sValue.zName = 0;
		aCase[n].sValue.iMods = 0;
		aCase[n].sValue.iType = PH7_NATIVE_VAL_NULL;
		aCase[n].sValue.iValue = 0;
		aCase[n].sValue.zValue = 0;
		aCase[n].sValue.rValue = 0.0;
	}
	return PH7_InstallNativeEnum(&(*pVm),"Pcntl\\QosClass",0,
		aCase,SX_ARRAYSIZE(aCase),0,0);
}
#endif /* the real thing */
