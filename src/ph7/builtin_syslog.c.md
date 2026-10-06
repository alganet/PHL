# src/ph7/builtin_syslog.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 111/162 lines (68.52%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#include "ph7int.h"` |
|      - |    6 | `/*` |
|      - |    7 | ` * Section:` |
|      - |    8 | ` *    php's syslog trio -- openlog(), syslog(), closelog() -- and the thirty-three` |
|      - |    9 | ` *    LOG_* constants ext/standard registers beside them.` |
|      - |   10 | ` * Status:` |
|      - |   11 | ` *    Stable.` |
|      - |   12 | ` *` |
|      - |   13 | ` * These three are not an extension: php puts them in ext/standard, so there is no` |
|      - |   14 | `` * `extension_loaded('syslog')` to answer and no way for a program to ask whether`` |
|      - |   15 | `` * they are there other than `function_exists()`. They are here on EVERY platform,`` |
|      - |   16 | ` * because they are there under php on every platform -- which is the whole reason` |
|      - |   17 | ` * this file has a Windows half at all.` |
|      - |   18 | ` *` |
|      - |   19 | ` *   THE CONSTANTS ARE NOT THE SAME NUMBERS ON WINDOWS. On a POSIX build every one` |
|      - |   20 | ` *   of them is the platform's own macro. On Windows there is no <syslog.h>, so php` |
|      - |   21 | ` *   ships its own header -- and the eight PRIORITIES it defines there are php's own` |
|      - |   22 | ` *   values, chosen so its event-type switch has distinct cases: LOG_EMERG, LOG_ALERT` |
|      - |   23 | ` *   and LOG_CRIT are all 1, LOG_ERR is 4, LOG_WARNING is 5, and LOG_NOTICE, LOG_INFO` |
|      - |   24 | ` *   and LOG_DEBUG are all 6. The facilities and options keep their POSIX numbers, and` |
|      - |   25 | ` *   LOG_LOCAL0..LOG_LOCAL7 DO NOT EXIST -- twenty-five constants there against` |
|      - |   26 | ` *   thirty-three here. All of that was read off a real php.exe rather than guessed;` |
|      - |   27 | `` *   monolog's `AbstractSyslogHandler` hard-codes the eight local facilities for`` |
|      - |   28 | ` *   Windows precisely because php does not define them.` |
|      - |   29 | ` *` |
|      - |   30 | `` *   THE MESSAGE IS FILTERED, and `syslog.filter` says how. php never hands the bytes`` |
|      - |   31 | `` *   to the C library untouched unless asked to: it splits the message on `\n` and`` |
|      - |   32 | `` *   emits each piece as its own record, and escapes some bytes as `\xNN`. Which ones`` |
|      - |   33 | ` *   is the directive, and the table below was derived by sweeping all 256 byte values` |
|      - |   34 | ` *   through the oracle in each of the four modes rather than read out of a manual:` |
|      - |   35 | ` *` |
|      - |   36 | ` *     no-ctrl (php's default)  escapes 0x00-0x1f and 0x7f` |
|      - |   37 | ` *     ascii                    escapes 0x00-0x1f, 0x7f-0xff` |
|      - |   38 | ` *     all                      escapes 0x7f, and nothing else` |
|      - |   39 | ``  *     raw                      escapes nothing, and does NOT split on `\n` `` |
|      - |   40 | ` *` |
|      - |   41 | ` *   A NUL that is not escaped truncates the record, because the last thing every one` |
|      - |   42 | `` *   of these does is hand a C string to the platform -- so `"a\0b"` is `a\x00b` under`` |
|      - |   43 | `` *   the two escaping modes and `a` under the two that let it through. That is php's`` |
|      - |   44 | ` *   answer too, and it is an artefact of the same C string.` |
|      - |   45 | ` *` |
|      - |   46 | ` *   THE PREFIX BELONGS TO THE CALLER AND THE POINTER BELONGS TO libc. POSIX openlog()` |
|      - |   47 | ` *   keeps the pointer it is given rather than copying it, so the string has to outlive` |
|      - |   48 | ` *   the call -- php keeps it in a module global and this keeps it per VM. A second` |
|      - |   49 | ` *   openlog() replaces it; closelog() drops it, and a syslog() after that is labelled` |
|      - |   50 | ` *   with the PROGRAM's name, which is the platform's answer rather than php's.` |
|      - |   51 | ` */` |
|      - |   52 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |   53 |  |
|      - |   54 | `#ifdef __WINNT__` |
|      - |   55 | `/* After ph7int.h, which pulls in <winsock2.h> for the net layer -- that order is` |
|      - |   56 | ` * required, and is the one every other Windows unit here uses. */` |
|      - |   57 | `#include <Windows.h>` |
|      - |   58 | `#else` |
|      - |   59 | `#include <syslog.h>` |
|      - |   60 | `#endif` |
|      - |   61 |  |
|      - |   62 | ``/* php's four `syslog.filter` modes. */`` |
|      - |   63 | `#define PH7_SYSLOG_ALL      0` |
|      - |   64 | `#define PH7_SYSLOG_NO_CTRL  1` |
|      - |   65 | `#define PH7_SYSLOG_ASCII    2` |
|      - |   66 | `#define PH7_SYSLOG_RAW      3` |
|      - |   67 |  |
|      - |   68 | `/*` |
|      - |   69 | ` * Windows has no <syslog.h>, so php ships its own numbers -- see the header note.` |
|      - |   70 | ` * They are spelled here rather than taken from a system macro because on that` |
|      - |   71 | ` * platform there IS no system macro to take them from.` |
|      - |   72 | ` */` |
|      - |   73 | `#ifdef __WINNT__` |
|      - |   74 | `#define PH7_LOG_EMERG    1` |
|      - |   75 | `#define PH7_LOG_ALERT    1` |
|      - |   76 | `#define PH7_LOG_CRIT     1` |
|      - |   77 | `#define PH7_LOG_ERR      4` |
|      - |   78 | `#define PH7_LOG_WARNING  5` |
|      - |   79 | `#define PH7_LOG_NOTICE   6` |
|      - |   80 | `#define PH7_LOG_INFO     6` |
|      - |   81 | `#define PH7_LOG_DEBUG    6` |
|      - |   82 | `#define PH7_LOG_KERN     0` |
|      - |   83 | `#define PH7_LOG_USER     (1<<3)` |
|      - |   84 | `#define PH7_LOG_MAIL     (2<<3)` |
|      - |   85 | `#define PH7_LOG_DAEMON   (3<<3)` |
|      - |   86 | `#define PH7_LOG_AUTH     (4<<3)` |
|      - |   87 | `#define PH7_LOG_SYSLOG   (5<<3)` |
|      - |   88 | `#define PH7_LOG_LPR      (6<<3)` |
|      - |   89 | `#define PH7_LOG_NEWS     (7<<3)` |
|      - |   90 | `#define PH7_LOG_UUCP     (8<<3)` |
|      - |   91 | `#define PH7_LOG_CRON     (9<<3)` |
|      - |   92 | `#define PH7_LOG_AUTHPRIV (10<<3)` |
|      - |   93 | `#define PH7_LOG_PID      0x01` |
|      - |   94 | `#define PH7_LOG_CONS     0x02` |
|      - |   95 | `#define PH7_LOG_ODELAY   0x04` |
|      - |   96 | `#define PH7_LOG_NDELAY   0x08` |
|      - |   97 | `#define PH7_LOG_NOWAIT   0x10` |
|      - |   98 | `#define PH7_LOG_PERROR   0x20` |
|      - |   99 | `#else` |
|      - |  100 | `#define PH7_LOG_EMERG    LOG_EMERG` |
|      - |  101 | `#define PH7_LOG_ALERT    LOG_ALERT` |
|      - |  102 | `#define PH7_LOG_CRIT     LOG_CRIT` |
|      - |  103 | `#define PH7_LOG_ERR      LOG_ERR` |
|      - |  104 | `#define PH7_LOG_WARNING  LOG_WARNING` |
|      - |  105 | `#define PH7_LOG_NOTICE   LOG_NOTICE` |
|      - |  106 | `#define PH7_LOG_INFO     LOG_INFO` |
|      - |  107 | `#define PH7_LOG_DEBUG    LOG_DEBUG` |
|      - |  108 | `#define PH7_LOG_KERN     LOG_KERN` |
|      - |  109 | `#define PH7_LOG_USER     LOG_USER` |
|      - |  110 | `#define PH7_LOG_MAIL     LOG_MAIL` |
|      - |  111 | `#define PH7_LOG_DAEMON   LOG_DAEMON` |
|      - |  112 | `#define PH7_LOG_AUTH     LOG_AUTH` |
|      - |  113 | `#define PH7_LOG_SYSLOG   LOG_SYSLOG` |
|      - |  114 | `#define PH7_LOG_LPR      LOG_LPR` |
|      - |  115 | `#define PH7_LOG_NEWS     LOG_NEWS` |
|      - |  116 | `#define PH7_LOG_UUCP     LOG_UUCP` |
|      - |  117 | `#define PH7_LOG_CRON     LOG_CRON` |
|      - |  118 | `#define PH7_LOG_AUTHPRIV LOG_AUTHPRIV` |
|      - |  119 | `#define PH7_LOG_PID      LOG_PID` |
|      - |  120 | `#define PH7_LOG_CONS     LOG_CONS` |
|      - |  121 | `#define PH7_LOG_ODELAY   LOG_ODELAY` |
|      - |  122 | `#define PH7_LOG_NDELAY   LOG_NDELAY` |
|      - |  123 | `#define PH7_LOG_NOWAIT   LOG_NOWAIT` |
|      - |  124 | `#define PH7_LOG_PERROR   LOG_PERROR` |
|      - |  125 | `#endif` |
|      - |  126 |  |
|      - |  127 | `/*` |
|      - |  128 | ` * The per-VM state. The IDENT has to live here rather than on the stack because` |
|      - |  129 | ` * POSIX openlog() keeps the pointer; the Windows handle has to live here because` |
|      - |  130 | ` * that is what a record is reported through.` |
|      - |  131 | ` */` |
|      - |  132 | `typedef struct syslog_state syslog_state;` |
|      - |  133 | `struct syslog_state {` |
|      - |  134 | `	SyBlob sIdent;   /* the prefix, NUL-terminated, alive until closelog() */` |
|      - |  135 | `	int bOpen;` |
|      - |  136 | `#ifdef __WINNT__` |
|      - |  137 | `	void *pSource;   /* HANDLE from RegisterEventSource */` |
|      - |  138 | `	int iFacility;   /* what openlog() was told; ReportEvent's category */` |
|      - |  139 | `#endif` |
|      - |  140 | `};` |
|      - |  141 |  |
|     23 |  142 | `static syslog_state * SyslogState(ph7_vm *pVm,int bCreate)` |
|    ! 0 |  143 | `{` |
|     23 |  144 | `	syslog_state *pS = (syslog_state *)pVm->pSyslog;` |
|     23 |  145 | `	if( pS == 0 && bCreate ){` |
|      1 |  146 | `		pS = (syslog_state *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(syslog_state));` |
|      1 |  147 | `		if( pS == 0 ){` |
|    ! 0 |  148 | `			return 0;` |
|      - |  149 | `		}` |
|      1 |  150 | `		SyZero(pS,sizeof(syslog_state));` |
|      1 |  151 | `		SyBlobInit(&pS->sIdent,&pVm->sAllocator);` |
|      1 |  152 | `		pVm->pSyslog = pS;` |
|    ! 0 |  153 | `	}` |
|     23 |  154 | `	return pS;` |
|    ! 0 |  155 | `}` |
|      - |  156 | ``/* Which of the four `syslog.filter` modes is in force. */`` |
|     45 |  157 | `static int SyslogFilter(ph7_vm *pVm)` |
|    ! 0 |  158 | `{` |
|      - |  159 | `	SyBlob sVal;` |
|     45 |  160 | `	int eMode = PH7_SYSLOG_NO_CTRL;` |
|      - |  161 | `	const char *z;` |
|      - |  162 | `	sxu32 n;` |
|     45 |  163 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|     45 |  164 | `	PH7_VmIniGetStr(pVm,"syslog.filter",&sVal);` |
|     45 |  165 | `	z = (const char *)SyBlobData(&sVal);` |
|     45 |  166 | `	n = SyBlobLength(&sVal);` |
|      - |  167 | `	/* Case-SENSITIVE, the way php's own OnUpdate handler matches -- the ini` |
|      - |  168 | `	 * screen refuses anything else, so nothing but these four can be stored. */` |
|     45 |  169 | `	if( n == 3 && z && SyMemcmp(z,"raw",3) == 0 ){` |
|     14 |  170 | `		eMode = PH7_SYSLOG_RAW;` |
|     31 |  171 | `	}else if( n == 3 && z && SyMemcmp(z,"all",3) == 0 ){` |
|      7 |  172 | `		eMode = PH7_SYSLOG_ALL;` |
|     24 |  173 | `	}else if( n == 5 && z && SyMemcmp(z,"ascii",5) == 0 ){` |
|      7 |  174 | `		eMode = PH7_SYSLOG_ASCII;` |
|    ! 0 |  175 | `	}` |
|     45 |  176 | `	SyBlobRelease(&sVal);` |
|     45 |  177 | `	return eMode;` |
|    ! 0 |  178 | `}` |
|      - |  179 | `/* Does this mode escape this byte? Derived by sweeping all 256 through the oracle. */` |
|     96 |  180 | `static int SyslogEscapes(int eMode,unsigned char c)` |
|    ! 0 |  181 | `{` |
|     96 |  182 | `	switch( eMode ){` |
|     22 |  183 | `	case PH7_SYSLOG_ALL:     return c == 0x7f;` |
|     22 |  184 | `	case PH7_SYSLOG_ASCII:   return c < 0x20 \|\| c >= 0x7f;` |
|     52 |  185 | `	case PH7_SYSLOG_NO_CTRL: return c < 0x20 \|\| c == 0x7f;` |
|    ! 0 |  186 | `	default:                 return 0;` |
|      - |  187 | `	}` |
|    ! 0 |  188 | `}` |
|      - |  189 | `/*` |
|      - |  190 | ` * Hand ONE record to the platform. Everything above this point has already split` |
|      - |  191 | ` * and escaped; what arrives here is a C string, which is why an unescaped NUL` |
|      - |  192 | ` * truncates the record under php too.` |
|      - |  193 | ` */` |
|     52 |  194 | `static void SyslogRecord(ph7_vm *pVm,int iPriority,const char *zMsg)` |
|    ! 0 |  195 | `{` |
|      - |  196 | `#ifdef __WINNT__` |
|    ! 0 |  197 | `	syslog_state *pS = SyslogState(pVm,0);` |
|      - |  198 | `	LPCSTR azStr[1];` |
|      - |  199 | `	WORD wType;` |
|      - |  200 | `	/* php's event-type switch. Its Windows LOG_* priorities are the numbers that` |
|      - |  201 | `	 * make these cases distinct, which is why they are not 0..7 there. */` |
|    ! 0 |  202 | `	switch( iPriority ){` |
|      - |  203 | `	case PH7_LOG_EMERG:   /* == LOG_ALERT == LOG_CRIT */` |
|    ! 0 |  204 | `		wType = EVENTLOG_ERROR_TYPE; break;` |
|      - |  205 | `	case PH7_LOG_ERR:` |
|    ! 0 |  206 | `		wType = EVENTLOG_ERROR_TYPE; break;` |
|      - |  207 | `	case PH7_LOG_WARNING:` |
|    ! 0 |  208 | `		wType = EVENTLOG_WARNING_TYPE; break;` |
|      - |  209 | `	case PH7_LOG_NOTICE:  /* == LOG_INFO == LOG_DEBUG */` |
|    ! 0 |  210 | `		wType = EVENTLOG_INFORMATION_TYPE; break;` |
|      - |  211 | `	default:` |
|    ! 0 |  212 | `		wType = EVENTLOG_ERROR_TYPE; break;` |
|      - |  213 | `	}` |
|    ! 0 |  214 | `	if( pS == 0 \|\| pS->pSource == 0 ){` |
|    ! 0 |  215 | `		return;   /* nothing registered: php reports nothing and still answers true */` |
|      - |  216 | `	}` |
|    ! 0 |  217 | `	azStr[0] = (LPCSTR)zMsg;` |
|    ! 0 |  218 | `	ReportEventA((HANDLE)pS->pSource,wType,(WORD)pS->iFacility,2000,` |
|      - |  219 | `		NULL,1,0,azStr,NULL);` |
|      - |  220 | `#else` |
|    ! 0 |  221 | `	SXUNUSED(pVm);` |
|     52 |  222 | `	syslog(iPriority,"%s",zMsg);` |
|      - |  223 | `#endif` |
|     52 |  224 | `}` |
|      - |  225 | `/*` |
|      - |  226 | `` * php's whole message transform: split on `\n` into records, escape what the`` |
|      - |  227 | `` * filter says to escape, and hand each piece over on its own. `raw` skips both.`` |
|      - |  228 | ` */` |
|     45 |  229 | `static void SyslogEmit(ph7_vm *pVm,int iPriority,const char *zMsg,int nMsg)` |
|    ! 0 |  230 | `{` |
|     45 |  231 | `	int eMode = SyslogFilter(pVm);` |
|      - |  232 | `	SyBlob sLine;` |
|      - |  233 | `	int i;` |
|     45 |  234 | `	if( nMsg < 0 ){` |
|    ! 0 |  235 | `		nMsg = 0;` |
|    ! 0 |  236 | `	}` |
|     45 |  237 | `	if( eMode == PH7_SYSLOG_RAW ){` |
|     14 |  238 | `		SyBlobInit(&sLine,&pVm->sAllocator);` |
|     14 |  239 | `		if( nMsg > 0 ){` |
|     13 |  240 | `			SyBlobAppend(&sLine,zMsg,(sxu32)nMsg);` |
|    ! 0 |  241 | `		}` |
|     14 |  242 | `		SyBlobAppend(&sLine,"\0",1);` |
|     14 |  243 | `		SyslogRecord(pVm,iPriority,(const char *)SyBlobData(&sLine));` |
|     14 |  244 | `		SyBlobRelease(&sLine);` |
|     14 |  245 | `		return;` |
|      - |  246 | `	}` |
|     31 |  247 | `	SyBlobInit(&sLine,&pVm->sAllocator);` |
|    134 |  248 | `	for( i = 0 ; i < nMsg ; ++i ){` |
|    103 |  249 | `		unsigned char c = (unsigned char)zMsg[i];` |
|    103 |  250 | `		if( c == '\n' ){` |
|      7 |  251 | `			SyBlobAppend(&sLine,"\0",1);` |
|      7 |  252 | `			SyslogRecord(pVm,iPriority,(const char *)SyBlobData(&sLine));` |
|      7 |  253 | `			SyBlobReset(&sLine);` |
|      7 |  254 | `			continue;` |
|      - |  255 | `		}` |
|     96 |  256 | `		if( SyslogEscapes(eMode,c) ){` |
|      - |  257 | `			char zEsc[8];` |
|     12 |  258 | `			sxu32 nEsc = SyBufferFormat(zEsc,(sxu32)sizeof(zEsc),"\\x%02x",(int)c);` |
|     12 |  259 | `			SyBlobAppend(&sLine,zEsc,nEsc);` |
|    ! 0 |  260 | `		}else{` |
|     84 |  261 | `			SyBlobAppend(&sLine,(const char *)&c,1);` |
|      - |  262 | `		}` |
|    ! 0 |  263 | `	}` |
|     31 |  264 | `	SyBlobAppend(&sLine,"\0",1);` |
|     31 |  265 | `	SyslogRecord(pVm,iPriority,(const char *)SyBlobData(&sLine));` |
|     31 |  266 | `	SyBlobRelease(&sLine);` |
|    ! 0 |  267 | `}` |
|      - |  268 | `/* Close whatever is open, on either platform. Idempotent, as php's is. */` |
|     16 |  269 | `static void SyslogClose(ph7_vm *pVm)` |
|    ! 0 |  270 | `{` |
|     16 |  271 | `	syslog_state *pS = SyslogState(pVm,0);` |
|      - |  272 | `#ifdef __WINNT__` |
|    ! 0 |  273 | `	if( pS && pS->pSource ){` |
|    ! 0 |  274 | `		DeregisterEventSource((HANDLE)pS->pSource);` |
|    ! 0 |  275 | `		pS->pSource = 0;` |
|      - |  276 | `	}` |
|      - |  277 | `#else` |
|     16 |  278 | `	closelog();` |
|      - |  279 | `#endif` |
|     16 |  280 | `	if( pS ){` |
|     15 |  281 | `		SyBlobReset(&pS->sIdent);` |
|     15 |  282 | `		pS->bOpen = 0;` |
|    ! 0 |  283 | `	}` |
|     16 |  284 | `}` |
|      - |  285 | `/*` |
|      - |  286 | ` * VM teardown. A POSIX openlog() left standing holds a pointer into the ident` |
|      - |  287 | ` * blob, which is about to be freed with the allocator; a Windows one holds an` |
|      - |  288 | ` * event-source handle the process would otherwise leak.` |
|      - |  289 | ` */` |
|   6995 |  290 | `PH7_PRIVATE void PH7_SyslogVmRelease(ph7_vm *pVm)` |
|      5 |  291 | `{` |
|   7000 |  292 | `	syslog_state *pS = (syslog_state *)pVm->pSyslog;` |
|   7000 |  293 | `	if( pS == 0 ){` |
|   6999 |  294 | `		return;` |
|      - |  295 | `	}` |
|      1 |  296 | `	if( pS->bOpen ){` |
|    ! 0 |  297 | `		SyslogClose(pVm);` |
|    ! 0 |  298 | `	}` |
|      1 |  299 | `	SyBlobRelease(&pS->sIdent);` |
|      1 |  300 | `	pVm->pSyslog = 0;` |
|      1 |  301 | `	SyMemBackendFree(&pVm->sAllocator,pS);` |
|   3497 |  302 | `}` |
|      - |  303 |  |
|      - |  304 | `/* --- The three functions -------------------------------------------------- */` |
|      - |  305 |  |
|      - |  306 | `/*` |
|      - |  307 | ` * true openlog(string $prefix, int $flags, int $facility)` |
|      - |  308 | ` *  Answers true unconditionally -- php has no failure to report here, and does` |
|      - |  309 | ` *  not screen the prefix for a NUL either: the C call takes a C string and stops` |
|      - |  310 | ` *  at one, which is what php does too.` |
|      - |  311 | ` */` |
|      7 |  312 | `PH7_PRIVATE int PH7_builtin_openlog(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  313 | `{` |
|      7 |  314 | `	syslog_state *pS = SyslogState(pCtx->pVm,1);` |
|      - |  315 | `	const char *zIdent;` |
|      7 |  316 | `	int nIdent = 0, iFlags, iFacility;` |
|    ! 0 |  317 | `	SXUNUSED(nArg);` |
|      7 |  318 | `	if( pS == 0 ){` |
|    ! 0 |  319 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  320 | `		return PH7_OK;` |
|      - |  321 | `	}` |
|      7 |  322 | `	zIdent = ph7_value_to_string(apArg[0],&nIdent);` |
|      7 |  323 | `	iFlags = (int)ph7_value_to_int64(apArg[1]);` |
|      7 |  324 | `	iFacility = (int)ph7_value_to_int64(apArg[2]);` |
|      - |  325 | `	/* A second openlog() replaces the first, and the CLOSE has to come before the` |
|      - |  326 | `	 * blob is rewritten: POSIX openlog() kept a pointer INTO it, and closelog()` |
|      - |  327 | `	 * is what makes libc let that pointer go. */` |
|      7 |  328 | `	SyslogClose(pCtx->pVm);` |
|      7 |  329 | `	SyBlobReset(&pS->sIdent);` |
|      7 |  330 | `	if( nIdent > 0 ){` |
|      7 |  331 | `		SyBlobAppend(&pS->sIdent,zIdent,(sxu32)nIdent);` |
|    ! 0 |  332 | `	}` |
|      7 |  333 | `	SyBlobAppend(&pS->sIdent,"\0",1);` |
|      7 |  334 | `	pS->bOpen = 1;` |
|      - |  335 | `#ifdef __WINNT__` |
|      - |  336 | `	/* php's Windows half has nowhere to put the OPTIONS: an event source has no` |
|      - |  337 | `	 * LOG_PID or LOG_PERROR, so the flags are taken and dropped, as php takes and` |
|      - |  338 | `	 * drops them. */` |
|      - |  339 | `	SXUNUSED(iFlags);` |
|    ! 0 |  340 | `	pS->iFacility = iFacility;` |
|    ! 0 |  341 | `	pS->pSource = (void *)RegisterEventSourceA(NULL,` |
|      - |  342 | `		(LPCSTR)SyBlobData(&pS->sIdent));` |
|      - |  343 | `#else` |
|      - |  344 | `	/* The POINTER is kept by the C library, which is why the blob above outlives` |
|      - |  345 | `	 * this call and is only reset by closelog() or VM teardown. */` |
|      7 |  346 | `	openlog((const char *)SyBlobData(&pS->sIdent),iFlags,iFacility);` |
|      - |  347 | `#endif` |
|      7 |  348 | `	ph7_result_bool(pCtx,1);` |
|      7 |  349 | `	return PH7_OK;` |
|    ! 0 |  350 | `}` |
|      - |  351 | `/*` |
|      - |  352 | ` * true syslog(int $priority, string $message)` |
|      - |  353 | ` *  php answers true whatever happens, including when nothing was ever opened --` |
|      - |  354 | ` *  the platform then labels the record with the PROGRAM's name.` |
|      - |  355 | ` */` |
|     45 |  356 | `PH7_PRIVATE int PH7_builtin_syslog(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  357 | `{` |
|      - |  358 | `	const char *zMsg;` |
|     45 |  359 | `	int nMsg = 0;` |
|    ! 0 |  360 | `	SXUNUSED(nArg);` |
|     45 |  361 | `	zMsg = ph7_value_to_string(apArg[1],&nMsg);` |
|     45 |  362 | `	SyslogEmit(pCtx->pVm,(int)ph7_value_to_int64(apArg[0]),zMsg,nMsg);` |
|     45 |  363 | `	ph7_result_bool(pCtx,1);` |
|     45 |  364 | `	return PH7_OK;` |
|    ! 0 |  365 | `}` |
|      - |  366 | `/* true closelog() */` |
|      9 |  367 | `PH7_PRIVATE int PH7_builtin_closelog(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  368 | `{` |
|    ! 0 |  369 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      9 |  370 | `	SyslogClose(pCtx->pVm);` |
|      9 |  371 | `	ph7_result_bool(pCtx,1);` |
|      9 |  372 | `	return PH7_OK;` |
|    ! 0 |  373 | `}` |
|      - |  374 |  |
|      - |  375 | `/* --- The constants -------------------------------------------------------- */` |
|      - |  376 |  |
|      - |  377 | `static const struct {` |
|      - |  378 | `	const char *zName;` |
|      - |  379 | `	int iValue;` |
|      - |  380 | `} aSyslogConst[] = {` |
|      - |  381 | `	/* php's own registration order, which is what ReflectionExtension answers in. */` |
|      - |  382 | `	{ "LOG_EMERG",    PH7_LOG_EMERG    },` |
|      - |  383 | `	{ "LOG_ALERT",    PH7_LOG_ALERT    },` |
|      - |  384 | `	{ "LOG_CRIT",     PH7_LOG_CRIT     },` |
|      - |  385 | `	{ "LOG_ERR",      PH7_LOG_ERR      },` |
|      - |  386 | `	{ "LOG_WARNING",  PH7_LOG_WARNING  },` |
|      - |  387 | `	{ "LOG_NOTICE",   PH7_LOG_NOTICE   },` |
|      - |  388 | `	{ "LOG_INFO",     PH7_LOG_INFO     },` |
|      - |  389 | `	{ "LOG_DEBUG",    PH7_LOG_DEBUG    },` |
|      - |  390 | `	{ "LOG_KERN",     PH7_LOG_KERN     },` |
|      - |  391 | `	{ "LOG_USER",     PH7_LOG_USER     },` |
|      - |  392 | `	{ "LOG_MAIL",     PH7_LOG_MAIL     },` |
|      - |  393 | `	{ "LOG_DAEMON",   PH7_LOG_DAEMON   },` |
|      - |  394 | `	{ "LOG_AUTH",     PH7_LOG_AUTH     },` |
|      - |  395 | `	{ "LOG_SYSLOG",   PH7_LOG_SYSLOG   },` |
|      - |  396 | `	{ "LOG_LPR",      PH7_LOG_LPR      },` |
|      - |  397 | `	{ "LOG_NEWS",     PH7_LOG_NEWS     },` |
|      - |  398 | `	{ "LOG_UUCP",     PH7_LOG_UUCP     },` |
|      - |  399 | `	{ "LOG_CRON",     PH7_LOG_CRON     },` |
|      - |  400 | `	{ "LOG_AUTHPRIV", PH7_LOG_AUTHPRIV },` |
|      - |  401 | `#ifdef LOG_LOCAL0` |
|      - |  402 | `	/* php defines none of these eight on Windows, and neither does this --` |
|      - |  403 | `	 * monolog's AbstractSyslogHandler hard-codes their numbers there for exactly` |
|      - |  404 | `	 * that reason. */` |
|      - |  405 | `	{ "LOG_LOCAL0",   LOG_LOCAL0 },` |
|      - |  406 | `	{ "LOG_LOCAL1",   LOG_LOCAL1 },` |
|      - |  407 | `	{ "LOG_LOCAL2",   LOG_LOCAL2 },` |
|      - |  408 | `	{ "LOG_LOCAL3",   LOG_LOCAL3 },` |
|      - |  409 | `	{ "LOG_LOCAL4",   LOG_LOCAL4 },` |
|      - |  410 | `	{ "LOG_LOCAL5",   LOG_LOCAL5 },` |
|      - |  411 | `	{ "LOG_LOCAL6",   LOG_LOCAL6 },` |
|      - |  412 | `	{ "LOG_LOCAL7",   LOG_LOCAL7 },` |
|      - |  413 | `#endif` |
|      - |  414 | `	{ "LOG_PID",      PH7_LOG_PID    },` |
|      - |  415 | `	{ "LOG_CONS",     PH7_LOG_CONS   },` |
|      - |  416 | `	{ "LOG_ODELAY",   PH7_LOG_ODELAY },` |
|      - |  417 | `	{ "LOG_NDELAY",   PH7_LOG_NDELAY },` |
|      - |  418 | `	{ "LOG_NOWAIT",   PH7_LOG_NOWAIT },` |
|      - |  419 | `	{ "LOG_PERROR",   PH7_LOG_PERROR },` |
|      - |  420 | `};` |
|   2735 |  421 | `static void SyslogConstExpand(ph7_value *pVal,void *pUserData)` |
|      5 |  422 | `{` |
|   2740 |  423 | `	ph7_value_int(pVal,SX_PTR_TO_INT(pUserData));` |
|   2740 |  424 | `}` |
|   6985 |  425 | `PH7_PRIVATE void PH7_RegisterSyslogConstants(ph7_vm *pVm)` |
|      5 |  426 | `{` |
|      - |  427 | `	sxu32 n;` |
| 237495 |  428 | `	for( n = 0 ; n < SX_ARRAYSIZE(aSyslogConst) ; ++n ){` |
| 345581 |  429 | `		ph7_create_constant(&(*pVm),aSyslogConst[n].zName,SyslogConstExpand,` |
| 230505 |  430 | `			SX_INT_TO_PTR(aSyslogConst[n].iValue));` |
| 115076 |  431 | `	}` |
|   6990 |  432 | `}` |
|      - |  433 | `#else /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - |  434 | `PH7_PRIVATE void PH7_SyslogVmRelease(ph7_vm *pVm)` |
|      - |  435 | `{` |
|      - |  436 | `	SXUNUSED(pVm);` |
|      - |  437 | `}` |
|      - |  438 | `PH7_PRIVATE void PH7_RegisterSyslogConstants(ph7_vm *pVm)` |
|      - |  439 | `{` |
|      - |  440 | `	SXUNUSED(pVm);` |
|      - |  441 | `}` |
|      - |  442 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - |  443 |  |
