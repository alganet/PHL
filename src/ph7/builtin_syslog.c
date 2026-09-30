/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    php's syslog trio -- openlog(), syslog(), closelog() -- and the thirty-three
 *    LOG_* constants ext/standard registers beside them.
 * Status:
 *    Stable.
 *
 * These three are not an extension: php puts them in ext/standard, so there is no
 * `extension_loaded('syslog')` to answer and no way for a program to ask whether
 * they are there other than `function_exists()`. They are here on EVERY platform,
 * because they are there under php on every platform -- which is the whole reason
 * this file has a Windows half at all.
 *
 *   THE CONSTANTS ARE NOT THE SAME NUMBERS ON WINDOWS. On a POSIX build every one
 *   of them is the platform's own macro. On Windows there is no <syslog.h>, so php
 *   ships its own header -- and the eight PRIORITIES it defines there are php's own
 *   values, chosen so its event-type switch has distinct cases: LOG_EMERG, LOG_ALERT
 *   and LOG_CRIT are all 1, LOG_ERR is 4, LOG_WARNING is 5, and LOG_NOTICE, LOG_INFO
 *   and LOG_DEBUG are all 6. The facilities and options keep their POSIX numbers, and
 *   LOG_LOCAL0..LOG_LOCAL7 DO NOT EXIST -- twenty-five constants there against
 *   thirty-three here. All of that was read off a real php.exe rather than guessed;
 *   monolog's `AbstractSyslogHandler` hard-codes the eight local facilities for
 *   Windows precisely because php does not define them.
 *
 *   THE MESSAGE IS FILTERED, and `syslog.filter` says how. php never hands the bytes
 *   to the C library untouched unless asked to: it splits the message on `\n` and
 *   emits each piece as its own record, and escapes some bytes as `\xNN`. Which ones
 *   is the directive, and the table below was derived by sweeping all 256 byte values
 *   through the oracle in each of the four modes rather than read out of a manual:
 *
 *     no-ctrl (php's default)  escapes 0x00-0x1f and 0x7f
 *     ascii                    escapes 0x00-0x1f, 0x7f-0xff
 *     all                      escapes 0x7f, and nothing else
 *     raw                      escapes nothing, and does NOT split on `\n`
 *
 *   A NUL that is not escaped truncates the record, because the last thing every one
 *   of these does is hand a C string to the platform -- so `"a\0b"` is `a\x00b` under
 *   the two escaping modes and `a` under the two that let it through. That is php's
 *   answer too, and it is an artefact of the same C string.
 *
 *   THE PREFIX BELONGS TO THE CALLER AND THE POINTER BELONGS TO libc. POSIX openlog()
 *   keeps the pointer it is given rather than copying it, so the string has to outlive
 *   the call -- php keeps it in a module global and this keeps it per VM. A second
 *   openlog() replaces it; closelog() drops it, and a syslog() after that is labelled
 *   with the PROGRAM's name, which is the platform's answer rather than php's.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC

#ifdef __WINNT__
/* After ph7int.h, which pulls in <winsock2.h> for the net layer -- that order is
 * required, and is the one every other Windows unit here uses. */
#include <Windows.h>
#else
#include <syslog.h>
#endif

/* php's four `syslog.filter` modes. */
#define PH7_SYSLOG_ALL      0
#define PH7_SYSLOG_NO_CTRL  1
#define PH7_SYSLOG_ASCII    2
#define PH7_SYSLOG_RAW      3

/*
 * Windows has no <syslog.h>, so php ships its own numbers -- see the header note.
 * They are spelled here rather than taken from a system macro because on that
 * platform there IS no system macro to take them from.
 */
#ifdef __WINNT__
#define PH7_LOG_EMERG    1
#define PH7_LOG_ALERT    1
#define PH7_LOG_CRIT     1
#define PH7_LOG_ERR      4
#define PH7_LOG_WARNING  5
#define PH7_LOG_NOTICE   6
#define PH7_LOG_INFO     6
#define PH7_LOG_DEBUG    6
#define PH7_LOG_KERN     0
#define PH7_LOG_USER     (1<<3)
#define PH7_LOG_MAIL     (2<<3)
#define PH7_LOG_DAEMON   (3<<3)
#define PH7_LOG_AUTH     (4<<3)
#define PH7_LOG_SYSLOG   (5<<3)
#define PH7_LOG_LPR      (6<<3)
#define PH7_LOG_NEWS     (7<<3)
#define PH7_LOG_UUCP     (8<<3)
#define PH7_LOG_CRON     (9<<3)
#define PH7_LOG_AUTHPRIV (10<<3)
#define PH7_LOG_PID      0x01
#define PH7_LOG_CONS     0x02
#define PH7_LOG_ODELAY   0x04
#define PH7_LOG_NDELAY   0x08
#define PH7_LOG_NOWAIT   0x10
#define PH7_LOG_PERROR   0x20
#else
#define PH7_LOG_EMERG    LOG_EMERG
#define PH7_LOG_ALERT    LOG_ALERT
#define PH7_LOG_CRIT     LOG_CRIT
#define PH7_LOG_ERR      LOG_ERR
#define PH7_LOG_WARNING  LOG_WARNING
#define PH7_LOG_NOTICE   LOG_NOTICE
#define PH7_LOG_INFO     LOG_INFO
#define PH7_LOG_DEBUG    LOG_DEBUG
#define PH7_LOG_KERN     LOG_KERN
#define PH7_LOG_USER     LOG_USER
#define PH7_LOG_MAIL     LOG_MAIL
#define PH7_LOG_DAEMON   LOG_DAEMON
#define PH7_LOG_AUTH     LOG_AUTH
#define PH7_LOG_SYSLOG   LOG_SYSLOG
#define PH7_LOG_LPR      LOG_LPR
#define PH7_LOG_NEWS     LOG_NEWS
#define PH7_LOG_UUCP     LOG_UUCP
#define PH7_LOG_CRON     LOG_CRON
#define PH7_LOG_AUTHPRIV LOG_AUTHPRIV
#define PH7_LOG_PID      LOG_PID
#define PH7_LOG_CONS     LOG_CONS
#define PH7_LOG_ODELAY   LOG_ODELAY
#define PH7_LOG_NDELAY   LOG_NDELAY
#define PH7_LOG_NOWAIT   LOG_NOWAIT
#define PH7_LOG_PERROR   LOG_PERROR
#endif

/*
 * The per-VM state. The IDENT has to live here rather than on the stack because
 * POSIX openlog() keeps the pointer; the Windows handle has to live here because
 * that is what a record is reported through.
 */
typedef struct syslog_state syslog_state;
struct syslog_state {
	SyBlob sIdent;   /* the prefix, NUL-terminated, alive until closelog() */
	int bOpen;
#ifdef __WINNT__
	void *pSource;   /* HANDLE from RegisterEventSource */
	int iFacility;   /* what openlog() was told; ReportEvent's category */
#endif
};

static syslog_state * SyslogState(ph7_vm *pVm,int bCreate)
{
	syslog_state *pS = (syslog_state *)pVm->pSyslog;
	if( pS == 0 && bCreate ){
		pS = (syslog_state *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(syslog_state));
		if( pS == 0 ){
			return 0;
		}
		SyZero(pS,sizeof(syslog_state));
		SyBlobInit(&pS->sIdent,&pVm->sAllocator);
		pVm->pSyslog = pS;
	}
	return pS;
}
/* Which of the four `syslog.filter` modes is in force. */
static int SyslogFilter(ph7_vm *pVm)
{
	SyBlob sVal;
	int eMode = PH7_SYSLOG_NO_CTRL;
	const char *z;
	sxu32 n;
	SyBlobInit(&sVal,&pVm->sAllocator);
	PH7_VmIniGetStr(pVm,"syslog.filter",&sVal);
	z = (const char *)SyBlobData(&sVal);
	n = SyBlobLength(&sVal);
	/* Case-SENSITIVE, the way php's own OnUpdate handler matches -- the ini
	 * screen refuses anything else, so nothing but these four can be stored. */
	if( n == 3 && z && SyMemcmp(z,"raw",3) == 0 ){
		eMode = PH7_SYSLOG_RAW;
	}else if( n == 3 && z && SyMemcmp(z,"all",3) == 0 ){
		eMode = PH7_SYSLOG_ALL;
	}else if( n == 5 && z && SyMemcmp(z,"ascii",5) == 0 ){
		eMode = PH7_SYSLOG_ASCII;
	}
	SyBlobRelease(&sVal);
	return eMode;
}
/* Does this mode escape this byte? Derived by sweeping all 256 through the oracle. */
static int SyslogEscapes(int eMode,unsigned char c)
{
	switch( eMode ){
	case PH7_SYSLOG_ALL:     return c == 0x7f;
	case PH7_SYSLOG_ASCII:   return c < 0x20 || c >= 0x7f;
	case PH7_SYSLOG_NO_CTRL: return c < 0x20 || c == 0x7f;
	default:                 return 0;
	}
}
/*
 * Hand ONE record to the platform. Everything above this point has already split
 * and escaped; what arrives here is a C string, which is why an unescaped NUL
 * truncates the record under php too.
 */
static void SyslogRecord(ph7_vm *pVm,int iPriority,const char *zMsg)
{
#ifdef __WINNT__
	syslog_state *pS = SyslogState(pVm,0);
	LPCSTR azStr[1];
	WORD wType;
	/* php's event-type switch. Its Windows LOG_* priorities are the numbers that
	 * make these cases distinct, which is why they are not 0..7 there. */
	switch( iPriority ){
	case PH7_LOG_EMERG:   /* == LOG_ALERT == LOG_CRIT */
		wType = EVENTLOG_ERROR_TYPE; break;
	case PH7_LOG_ERR:
		wType = EVENTLOG_ERROR_TYPE; break;
	case PH7_LOG_WARNING:
		wType = EVENTLOG_WARNING_TYPE; break;
	case PH7_LOG_NOTICE:  /* == LOG_INFO == LOG_DEBUG */
		wType = EVENTLOG_INFORMATION_TYPE; break;
	default:
		wType = EVENTLOG_ERROR_TYPE; break;
	}
	if( pS == 0 || pS->pSource == 0 ){
		return;   /* nothing registered: php reports nothing and still answers true */
	}
	azStr[0] = (LPCSTR)zMsg;
	ReportEventA((HANDLE)pS->pSource,wType,(WORD)pS->iFacility,2000,
		NULL,1,0,azStr,NULL);
#else
	SXUNUSED(pVm);
	syslog(iPriority,"%s",zMsg);
#endif
}
/*
 * php's whole message transform: split on `\n` into records, escape what the
 * filter says to escape, and hand each piece over on its own. `raw` skips both.
 */
static void SyslogEmit(ph7_vm *pVm,int iPriority,const char *zMsg,int nMsg)
{
	int eMode = SyslogFilter(pVm);
	SyBlob sLine;
	int i;
	if( nMsg < 0 ){
		nMsg = 0;
	}
	if( eMode == PH7_SYSLOG_RAW ){
		SyBlobInit(&sLine,&pVm->sAllocator);
		if( nMsg > 0 ){
			SyBlobAppend(&sLine,zMsg,(sxu32)nMsg);
		}
		SyBlobAppend(&sLine,"\0",1);
		SyslogRecord(pVm,iPriority,(const char *)SyBlobData(&sLine));
		SyBlobRelease(&sLine);
		return;
	}
	SyBlobInit(&sLine,&pVm->sAllocator);
	for( i = 0 ; i < nMsg ; ++i ){
		unsigned char c = (unsigned char)zMsg[i];
		if( c == '\n' ){
			SyBlobAppend(&sLine,"\0",1);
			SyslogRecord(pVm,iPriority,(const char *)SyBlobData(&sLine));
			SyBlobReset(&sLine);
			continue;
		}
		if( SyslogEscapes(eMode,c) ){
			char zEsc[8];
			sxu32 nEsc = SyBufferFormat(zEsc,(sxu32)sizeof(zEsc),"\\x%02x",(int)c);
			SyBlobAppend(&sLine,zEsc,nEsc);
		}else{
			SyBlobAppend(&sLine,(const char *)&c,1);
		}
	}
	SyBlobAppend(&sLine,"\0",1);
	SyslogRecord(pVm,iPriority,(const char *)SyBlobData(&sLine));
	SyBlobRelease(&sLine);
}
/* Close whatever is open, on either platform. Idempotent, as php's is. */
static void SyslogClose(ph7_vm *pVm)
{
	syslog_state *pS = SyslogState(pVm,0);
#ifdef __WINNT__
	if( pS && pS->pSource ){
		DeregisterEventSource((HANDLE)pS->pSource);
		pS->pSource = 0;
	}
#else
	closelog();
#endif
	if( pS ){
		SyBlobReset(&pS->sIdent);
		pS->bOpen = 0;
	}
}
/*
 * VM teardown. A POSIX openlog() left standing holds a pointer into the ident
 * blob, which is about to be freed with the allocator; a Windows one holds an
 * event-source handle the process would otherwise leak.
 */
PH7_PRIVATE void PH7_SyslogVmRelease(ph7_vm *pVm)
{
	syslog_state *pS = (syslog_state *)pVm->pSyslog;
	if( pS == 0 ){
		return;
	}
	if( pS->bOpen ){
		SyslogClose(pVm);
	}
	SyBlobRelease(&pS->sIdent);
	pVm->pSyslog = 0;
	SyMemBackendFree(&pVm->sAllocator,pS);
}

/* --- The three functions -------------------------------------------------- */

/*
 * true openlog(string $prefix, int $flags, int $facility)
 *  Answers true unconditionally -- php has no failure to report here, and does
 *  not screen the prefix for a NUL either: the C call takes a C string and stops
 *  at one, which is what php does too.
 */
PH7_PRIVATE int PH7_builtin_openlog(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	syslog_state *pS = SyslogState(pCtx->pVm,1);
	const char *zIdent;
	int nIdent = 0, iFlags, iFacility;
	SXUNUSED(nArg);
	if( pS == 0 ){
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	zIdent = ph7_value_to_string(apArg[0],&nIdent);
	iFlags = (int)ph7_value_to_int64(apArg[1]);
	iFacility = (int)ph7_value_to_int64(apArg[2]);
	/* A second openlog() replaces the first, and the CLOSE has to come before the
	 * blob is rewritten: POSIX openlog() kept a pointer INTO it, and closelog()
	 * is what makes libc let that pointer go. */
	SyslogClose(pCtx->pVm);
	SyBlobReset(&pS->sIdent);
	if( nIdent > 0 ){
		SyBlobAppend(&pS->sIdent,zIdent,(sxu32)nIdent);
	}
	SyBlobAppend(&pS->sIdent,"\0",1);
	pS->bOpen = 1;
#ifdef __WINNT__
	/* php's Windows half has nowhere to put the OPTIONS: an event source has no
	 * LOG_PID or LOG_PERROR, so the flags are taken and dropped, as php takes and
	 * drops them. */
	SXUNUSED(iFlags);
	pS->iFacility = iFacility;
	pS->pSource = (void *)RegisterEventSourceA(NULL,
		(LPCSTR)SyBlobData(&pS->sIdent));
#else
	/* The POINTER is kept by the C library, which is why the blob above outlives
	 * this call and is only reset by closelog() or VM teardown. */
	openlog((const char *)SyBlobData(&pS->sIdent),iFlags,iFacility);
#endif
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * true syslog(int $priority, string $message)
 *  php answers true whatever happens, including when nothing was ever opened --
 *  the platform then labels the record with the PROGRAM's name.
 */
PH7_PRIVATE int PH7_builtin_syslog(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zMsg;
	int nMsg = 0;
	SXUNUSED(nArg);
	zMsg = ph7_value_to_string(apArg[1],&nMsg);
	SyslogEmit(pCtx->pVm,(int)ph7_value_to_int64(apArg[0]),zMsg,nMsg);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* true closelog() */
PH7_PRIVATE int PH7_builtin_closelog(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	SyslogClose(pCtx->pVm);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* --- The constants -------------------------------------------------------- */

static const struct {
	const char *zName;
	int iValue;
} aSyslogConst[] = {
	/* php's own registration order, which is what ReflectionExtension answers in. */
	{ "LOG_EMERG",    PH7_LOG_EMERG    },
	{ "LOG_ALERT",    PH7_LOG_ALERT    },
	{ "LOG_CRIT",     PH7_LOG_CRIT     },
	{ "LOG_ERR",      PH7_LOG_ERR      },
	{ "LOG_WARNING",  PH7_LOG_WARNING  },
	{ "LOG_NOTICE",   PH7_LOG_NOTICE   },
	{ "LOG_INFO",     PH7_LOG_INFO     },
	{ "LOG_DEBUG",    PH7_LOG_DEBUG    },
	{ "LOG_KERN",     PH7_LOG_KERN     },
	{ "LOG_USER",     PH7_LOG_USER     },
	{ "LOG_MAIL",     PH7_LOG_MAIL     },
	{ "LOG_DAEMON",   PH7_LOG_DAEMON   },
	{ "LOG_AUTH",     PH7_LOG_AUTH     },
	{ "LOG_SYSLOG",   PH7_LOG_SYSLOG   },
	{ "LOG_LPR",      PH7_LOG_LPR      },
	{ "LOG_NEWS",     PH7_LOG_NEWS     },
	{ "LOG_UUCP",     PH7_LOG_UUCP     },
	{ "LOG_CRON",     PH7_LOG_CRON     },
	{ "LOG_AUTHPRIV", PH7_LOG_AUTHPRIV },
#ifdef LOG_LOCAL0
	/* php defines none of these eight on Windows, and neither does this --
	 * monolog's AbstractSyslogHandler hard-codes their numbers there for exactly
	 * that reason. */
	{ "LOG_LOCAL0",   LOG_LOCAL0 },
	{ "LOG_LOCAL1",   LOG_LOCAL1 },
	{ "LOG_LOCAL2",   LOG_LOCAL2 },
	{ "LOG_LOCAL3",   LOG_LOCAL3 },
	{ "LOG_LOCAL4",   LOG_LOCAL4 },
	{ "LOG_LOCAL5",   LOG_LOCAL5 },
	{ "LOG_LOCAL6",   LOG_LOCAL6 },
	{ "LOG_LOCAL7",   LOG_LOCAL7 },
#endif
	{ "LOG_PID",      PH7_LOG_PID    },
	{ "LOG_CONS",     PH7_LOG_CONS   },
	{ "LOG_ODELAY",   PH7_LOG_ODELAY },
	{ "LOG_NDELAY",   PH7_LOG_NDELAY },
	{ "LOG_NOWAIT",   PH7_LOG_NOWAIT },
	{ "LOG_PERROR",   PH7_LOG_PERROR },
};
static void SyslogConstExpand(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,SX_PTR_TO_INT(pUserData));
}
PH7_PRIVATE void PH7_RegisterSyslogConstants(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aSyslogConst) ; ++n ){
		ph7_create_constant(&(*pVm),aSyslogConst[n].zName,SyslogConstExpand,
			SX_INT_TO_PTR(aSyslogConst[n].iValue));
	}
}
#else /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE void PH7_SyslogVmRelease(ph7_vm *pVm)
{
	SXUNUSED(pVm);
}
PH7_PRIVATE void PH7_RegisterSyslogConstants(ph7_vm *pVm)
{
	SXUNUSED(pVm);
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
