# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 694/763 lines (90.96%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |    8 | `/*` |
|     - |    9 | ` * Date/Time functions` |
|     - |   10 | ` * Status:` |
|     - |   11 | ` *    Devel.` |
|     - |   12 | ` */` |
|     - |   13 | `#include <time.h>` |
|     - |   14 | `/* Civil-date helpers (defined with the DateTime layer below) */` |
|     - |   15 | `#ifdef __WINNT__` |
|     - |   16 | `#ifdef _MSC_VER` |
|     - |   17 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|     - |   18 | `#pragma warning(disable:4996) /* _CRT_SECURE_NO_WARNINGS */` |
|     - |   19 | `#endif` |
|     - |   20 | `#endif` |
|     - |   21 | `#endif` |
|     - |   22 | `#ifdef __WINNT__` |
|     - |   23 | `/* GetSystemTime() */` |
|     - |   24 | `#include <Windows.h>` |
|     - |   25 | `/* GetProcessMemoryInfo(), for getrusage() */` |
|     - |   26 | `#include <psapi.h>` |
|     - |   27 | `#ifdef _WIN32_WCE` |
|     - |   28 | `/* SPDX-SnippetBegin */` |
|     - |   29 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|     - |   30 | `/* SPDX-License-Identifier: blessing */` |
|     - |   31 | `/*` |
|     - |   32 | `** WindowsCE does not have a localtime() function.  So create a` |
|     - |   33 | `** substitute.` |
|     - |   34 | `** Taken from the SQLite3 source tree.` |
|     - |   35 | `** Status: Public domain` |
|     - |   36 | `*/` |
|     - |   37 | `struct tm *__cdecl localtime(const time_t *t)` |
|     - |   38 | `{` |
|     - |   39 | `  static struct tm y;` |
|     - |   40 | `  FILETIME uTm, lTm;` |
|     - |   41 | `  SYSTEMTIME pTm;` |
|     - |   42 | `  ph7_int64 t64;` |
|     - |   43 | `  t64 = *t;` |
|     - |   44 | `  t64 = (t64 + 11644473600)*10000000;` |
|     - |   45 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|     - |   46 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|     - |   47 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|     - |   48 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|     - |   49 | `  y.tm_year = pTm.wYear - 1900;` |
|     - |   50 | `  y.tm_mon = pTm.wMonth - 1;` |
|     - |   51 | `  y.tm_wday = pTm.wDayOfWeek;` |
|     - |   52 | `  y.tm_mday = pTm.wDay;` |
|     - |   53 | `  y.tm_hour = pTm.wHour;` |
|     - |   54 | `  y.tm_min = pTm.wMinute;` |
|     - |   55 | `  y.tm_sec = pTm.wSecond;` |
|     - |   56 | `  return &y;` |
|     - |   57 | `}` |
|     - |   58 | `/* SPDX-SnippetEnd */` |
|     - |   59 | `#endif /*_WIN32_WCE */` |
|     - |   60 | `#elif defined(__UNIXES__)` |
|     - |   61 | `#include <sys/time.h>` |
|     - |   62 | `#include <sys/resource.h>` |
|     - |   63 | `#endif /* __WINNT__*/` |
|     - |   64 | `/*` |
|     - |   65 | ` * Resolve the current wall-clock time (epoch seconds + sub-second microseconds).` |
|     - |   66 | ` *` |
|     - |   67 | ` * An embedder may override the platform clock via PH7_CONFIG_CLOCK (e.g. the` |
|     - |   68 | ` * ESP32 port routes this through esp_timer); when no hook is registered we use` |
|     - |   69 | ` * gettimeofday() on Unix and fall back to a second-resolution time() elsewhere.` |
|     - |   70 | ` * Centralising this here gives microtime()/gettimeofday() a single sub-second` |
|     - |   71 | `` * source instead of the old nonsensical `tt % SX_USEC_PER_SEC` off-Unix path.`` |
|     - |   72 | ` */` |
|  4728 |   73 | `static void DateNow(ph7_vm *pVm,sytime *pOut)` |
|     4 |   74 | `{` |
|  4732 |   75 | `	if( pVm && pVm->pEngine->xConf.xClock ){` |
|   ! 0 |   76 | `		ph7_int64 sec = 0,usec = 0;` |
|   ! 0 |   77 | `		if( pVm->pEngine->xConf.xClock(pVm->pEngine->xConf.pClockData,&sec,&usec) == PH7_OK ){` |
|   ! 0 |   78 | `			pOut->tm_sec  = (long)sec;` |
|   ! 0 |   79 | `			pOut->tm_usec = (long)usec;` |
|   ! 0 |   80 | `			return;` |
|     - |   81 | `		}` |
|   ! 0 |   82 | `	}` |
|     - |   83 | `#if defined(__UNIXES__)` |
|     - |   84 | `	{` |
|     - |   85 | `		struct timeval tv;` |
|  4728 |   86 | `		gettimeofday(&tv,0);` |
|  4728 |   87 | `		pOut->tm_sec  = (long)tv.tv_sec;` |
|  4728 |   88 | `		pOut->tm_usec = (long)tv.tv_usec;` |
|     - |   89 | `	}` |
|     - |   90 | `#elif defined(__WINNT__)` |
|     - |   91 | `	{` |
|     - |   92 | `		/* FILETIME is 100-ns ticks since 1601-01-01 UTC; convert to the Unix` |
|     - |   93 | `		 * epoch with microsecond resolution (GetSystemTime() only carries` |
|     - |   94 | `		 * milliseconds, and time() has no sub-second part at all).` |
|     - |   95 | `		 *` |
|     - |   96 | `		 * GetSystemTimeAsFileTime is a FILETIME with the timer-interrupt's` |
|     - |   97 | `		 * granularity behind it -- about 15.6ms by default -- so it does not` |
|     - |   98 | `		 * carry microseconds at all, whatever its unit says. php reads the` |
|     - |   99 | `		 * PRECISE call where the system has one (Windows 8 and up) for exactly` |
|     - |  100 | `		 * this reason, and resolves it at run time so an older system still` |
|     - |  101 | ``		 * links; without it `uniqid()` answers the same id for every call inside`` |
|     - |  102 | `		 * one timer tick, and microtime() has three useful digits. */` |
|     - |  103 | `		static void (WINAPI *xPrecise)(LPFILETIME) = 0;` |
|     - |  104 | `		static int bPreciseResolved = 0;` |
|     - |  105 | `		FILETIME ft;` |
|     - |  106 | `		ph7_int64 t;` |
|     4 |  107 | `		if( !bPreciseResolved ){` |
|     4 |  108 | `			HMODULE hKernel = GetModuleHandleA("kernel32.dll");` |
|     4 |  109 | `			if( hKernel ){` |
|     4 |  110 | `				xPrecise = (void (WINAPI *)(LPFILETIME))(void *)` |
|     - |  111 | `					GetProcAddress(hKernel,"GetSystemTimePreciseAsFileTime");` |
|     - |  112 | `			}` |
|     4 |  113 | `			bPreciseResolved = 1;` |
|     - |  114 | `		}` |
|     4 |  115 | `		if( xPrecise ){` |
|     4 |  116 | `			xPrecise(&ft);` |
|     4 |  117 | `		}else{` |
|   ! 0 |  118 | `			GetSystemTimeAsFileTime(&ft);` |
|     - |  119 | `		}` |
|     4 |  120 | `		t  = (ph7_int64)ft.dwHighDateTime << 32;` |
|     4 |  121 | `		t += ft.dwLowDateTime;` |
|     4 |  122 | `		t -= 116444736000000000LL; /* 100-ns ticks between 1601 and 1970 */` |
|     4 |  123 | `		pOut->tm_sec  = (long)(t / 10000000);` |
|     4 |  124 | `		pOut->tm_usec = (long)((t % 10000000) / 10);` |
|     - |  125 | `	}` |
|     - |  126 | `#else` |
|     - |  127 | `	{` |
|     - |  128 | `		time_t tt;` |
|     - |  129 | `		time(&tt);` |
|     - |  130 | `		pOut->tm_sec  = (long)tt;` |
|     - |  131 | `		pOut->tm_usec = 0; /* no sub-second source; embedders supply one via PH7_CONFIG_CLOCK */` |
|     - |  132 | `	}` |
|     - |  133 | `#endif /* __UNIXES__ */` |
|  2368 |  134 | `}` |
|     - |  135 | `/*` |
|     - |  136 | ` * ...and the same clock for a caller outside this file. php reads it in exactly` |
|     - |  137 | ` * two places that matter to a script: the date surface, and uniqid(), whose` |
|     - |  138 | `` * whole value is `%08x%05x` of these two numbers.`` |
|     - |  139 | ` */` |
|  1250 |  140 | `PH7_PRIVATE void PH7_VmClockNow(ph7_vm *pVm,ph7_int64 *pSec,ph7_int64 *pUsec)` |
|     2 |  141 | `{` |
|     - |  142 | `	sytime sNow;` |
|  1252 |  143 | `	sNow.tm_sec = 0;` |
|  1252 |  144 | `	sNow.tm_usec = 0;` |
|  1252 |  145 | `	DateNow(pVm,&sNow);` |
|  1252 |  146 | `	if( pSec ){` |
|  1252 |  147 | `		*pSec = (ph7_int64)sNow.tm_sec;` |
|   625 |  148 | `	}` |
|  1252 |  149 | `	if( pUsec ){` |
|  1252 |  150 | `		*pUsec = (ph7_int64)sNow.tm_usec;` |
|   625 |  151 | `	}` |
|  1252 |  152 | `}` |
|     - |  153 |  |
|     - |  154 | `/*` |
|     - |  155 | ` * The current moment as the DateTime layer wants it: epoch seconds plus the` |
|     - |  156 | ` * MICROSECONDS beside them.` |
|     - |  157 | ` *` |
|     - |  158 | ` * php's date classes take their base moment from the same clock microtime()` |
|     - |  159 | `` * reads, sub-second part included -- `new DateTime()` carries the microseconds`` |
|     - |  160 | `` * of the instant it was built, which is what makes `$a->diff($b)->f` mean`` |
|     - |  161 | ` * anything for two moments a program measured. The parse layer used to read` |
|     - |  162 | `` * `time(0)` directly, so every one of them was born on a whole second and every`` |
|     - |  163 | ` * such diff answered 0.0. Routing them through DateNow() also hands the date` |
|     - |  164 | ` * classes the PH7_CONFIG_CLOCK hook the procedural half already had.` |
|     - |  165 | ` */` |
|  3400 |  166 | `PH7_PRIVATE void DtNowUs(ph7_vm *pVm,sxi64 *piSec,int *puSec)` |
|     3 |  167 | `{` |
|     - |  168 | `	sytime sNow;` |
|  3403 |  169 | `	DateNow(pVm,&sNow);` |
|  3403 |  170 | `	if( piSec ){` |
|  3403 |  171 | `		*piSec = (sxi64)sNow.tm_sec;` |
|  1700 |  172 | `	}` |
|  3403 |  173 | `	if( puSec ){` |
|  3323 |  174 | `		*puSec = (int)sNow.tm_usec;` |
|  1660 |  175 | `	}` |
|  3403 |  176 | `}` |
|     - |  177 | `/*` |
|     - |  178 | ` * Break a Unix timestamp (or the current time) down into a Sytm the way the` |
|     - |  179 | ` * DateTime layer does: PHL's own civil arithmetic, not the platform's gmtime().` |
|     - |  180 | ` *` |
|     - |  181 | `` * gmtime() keeps its year in an `int` and simply FAILS past it, and the five`` |
|     - |  182 | ` * procedural doors below then formatted the CURRENT time instead -- so` |
|     - |  183 | `` * `date('Y', PHP_INT_MAX)` read today's year here where php reads`` |
|     - |  184 | ` * 292277026596. The engine has no tz database (date_default_timezone_set()` |
|     - |  185 | ` * takes UTC/GMT only), so date() and gmdate() share this UTC breakdown exactly` |
|     - |  186 | ` * as they already did through gmtime().` |
|     - |  187 | ` */` |
|   750 |  188 | `static void DtSytmOfTimestamp(sxi64 iTs,Sytm *pOut)` |
|     2 |  189 | `{` |
|     - |  190 | `	/* A NULL tm_zone is what the date()-family fills mean by "the script's` |
|     - |  191 | `	 * default timezone" -- DateFormat's 'e'/'T' read pVm->zDefTz through it,` |
|     - |  192 | `	 * so naming the zone here would pin every one of them to UTC. */` |
|   752 |  193 | `	DtFillSytm(iTs,0,0,pOut);` |
|   752 |  194 | `}` |
|     - |  195 | ` /*` |
|     - |  196 | `  * int64 time(void)` |
|     - |  197 | `  *  Current Unix timestamp` |
|     - |  198 | `  * Parameters` |
|     - |  199 | `  *  None.` |
|     - |  200 | `  * Return` |
|     - |  201 | `  *  Returns the current time measured in the number of seconds` |
|     - |  202 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|     - |  203 | `  */` |
|    20 |  204 | `PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  205 | `{` |
|     - |  206 | `	time_t tt;` |
|    10 |  207 | `	SXUNUSED(nArg); /* cc warning */` |
|    10 |  208 | `	SXUNUSED(apArg);` |
|     - |  209 | `	/* Extract the current time */` |
|    23 |  210 | `	time(&tt);` |
|     - |  211 | `	/* Return as 64-bit integer */` |
|    23 |  212 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|    23 |  213 | `	return  PH7_OK;` |
|     3 |  214 | `}` |
|     - |  215 | `/*` |
|     - |  216 | `  * string/float microtime([ bool $get_as_float = false ])` |
|     - |  217 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|     - |  218 | `  * Parameters` |
|     - |  219 | `  *  $get_as_float` |
|     - |  220 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|     - |  221 | `  *   as described in the return values section below.` |
|     - |  222 | `  * Return` |
|     - |  223 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|     - |  224 | `  *  is the current time measured in the number of seconds since the Unix` |
|     - |  225 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|     - |  226 | `  *  that have elapsed since sec expressed in seconds.` |
|     - |  227 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|     - |  228 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|     - |  229 | `  */` |
|    70 |  230 | `PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  231 | `{` |
|    73 |  232 | `	int bFloat = 0;` |
|     - |  233 | `	sytime sTime;` |
|    73 |  234 | `	DateNow(pCtx->pVm,&sTime);` |
|    73 |  235 | `	if( nArg > 0 ){` |
|    67 |  236 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|    32 |  237 | `	}` |
|    73 |  238 | `	if( bFloat ){` |
|     - |  239 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|    67 |  240 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|    35 |  241 | `	}else{` |
|     - |  242 | `		/* Return PHP's "msec sec" form: the sub-second part as fractional` |
|     - |  243 | `		 * seconds to 8 decimals, e.g. "0.50667100 1700000000". tm_usec is in` |
|     - |  244 | `		 * microseconds (0..999999), so scaling by 100 yields the 8-digit` |
|     - |  245 | `		 * fraction — matching PHP's "%.8F" output exactly. */` |
|     7 |  246 | `		ph7_result_string_format(pCtx,"0.%08ld %ld",sTime.tm_usec*100,sTime.tm_sec);` |
|     - |  247 | `	}` |
|    73 |  248 | `	return PH7_OK;` |
|     3 |  249 | `}` |
|     - |  250 | `/*` |
|     - |  251 | ` * array\|false getrusage(int $mode = 0)` |
|     - |  252 | ` *` |
|     - |  253 | `` * php's seventeen `getrusage(2)` fields, in php's own key ORDER (which is the`` |
|     - |  254 | ` * struct's reverse -- the two time pairs last, each with its microseconds before` |
|     - |  255 | `` * its seconds). $mode 1 asks for RUSAGE_CHILDREN; php answers `false` for any`` |
|     - |  256 | ` * other non-zero value.` |
|     - |  257 | ` *` |
|     - |  258 | ` * PHPUnit's telemetry calls it for every event it emits, so without it PHPUnit 13` |
|     - |  259 | `` * does not start at all -- `An error occurred inside PHPUnit. Call to undefined`` |
|     - |  260 | `` * function getrusage()`, before a single test runs.`` |
|     - |  261 | ` *` |
|     - |  262 | ` * php's Windows build has its own getrusage() and keys for only the SIX fields` |
|     - |  263 | ` * it fills: the page-fault count, the peak working set in KiB and the two` |
|     - |  264 | ` * CPU-time pairs. RUSAGE_CHILDREN is the same number as RUSAGE_THREAD there, so` |
|     - |  265 | ` * $mode 1 answers the calling thread's times (and 0 for the other two).` |
|     - |  266 | ` */` |
|    12 |  267 | `PH7_PRIVATE int PH7_builtin_getrusage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  268 | `{` |
|     - |  269 | `	ph7_value *pArray,*pVal;` |
|    13 |  270 | `	int iMode = 0;` |
|     - |  271 | `	/* php's key order, and the field each one carries. */` |
|     - |  272 | `	ph7_int64 aVal[17];` |
|     - |  273 | `	static const char *const azKey[17] = {` |
|     - |  274 | `		"ru_oublock","ru_inblock","ru_msgsnd","ru_msgrcv","ru_maxrss","ru_ixrss",` |
|     - |  275 | `		"ru_idrss","ru_minflt","ru_majflt","ru_nsignals","ru_nvcsw","ru_nivcsw",` |
|     - |  276 | `		"ru_nswap","ru_utime.tv_usec","ru_utime.tv_sec","ru_stime.tv_usec",` |
|     - |  277 | `		"ru_stime.tv_sec"` |
|     - |  278 | `	};` |
|     - |  279 | `	int i;` |
|    13 |  280 | `	if( nArg > 0 ){` |
|     7 |  281 | `		iMode = (int)ph7_value_to_int(apArg[0]);` |
|     3 |  282 | `	}` |
|     - |  283 | `	/* php asks the OS for RUSAGE_CHILDREN on 1 and RUSAGE_SELF on everything else` |
|     - |  284 | `	 * -- there is no refusal for an out-of-range mode, it simply is not 1. */` |
|   217 |  285 | `	for( i = 0 ; i < 17 ; i++ ){` |
|   205 |  286 | `		aVal[i] = 0;` |
|   103 |  287 | `	}` |
|     - |  288 | `#if defined(__UNIXES__)` |
|     - |  289 | `	{` |
|     - |  290 | `		struct rusage sUsage;` |
|    12 |  291 | `		if( getrusage(iMode == 1 ? RUSAGE_CHILDREN : RUSAGE_SELF,&sUsage) == 0 ){` |
|    12 |  292 | `			aVal[0]  = (ph7_int64)sUsage.ru_oublock;` |
|    12 |  293 | `			aVal[1]  = (ph7_int64)sUsage.ru_inblock;` |
|    12 |  294 | `			aVal[2]  = (ph7_int64)sUsage.ru_msgsnd;` |
|    12 |  295 | `			aVal[3]  = (ph7_int64)sUsage.ru_msgrcv;` |
|    12 |  296 | `			aVal[4]  = (ph7_int64)sUsage.ru_maxrss;` |
|    12 |  297 | `			aVal[5]  = (ph7_int64)sUsage.ru_ixrss;` |
|    12 |  298 | `			aVal[6]  = (ph7_int64)sUsage.ru_idrss;` |
|    12 |  299 | `			aVal[7]  = (ph7_int64)sUsage.ru_minflt;` |
|    12 |  300 | `			aVal[8]  = (ph7_int64)sUsage.ru_majflt;` |
|    12 |  301 | `			aVal[9]  = (ph7_int64)sUsage.ru_nsignals;` |
|    12 |  302 | `			aVal[10] = (ph7_int64)sUsage.ru_nvcsw;` |
|    12 |  303 | `			aVal[11] = (ph7_int64)sUsage.ru_nivcsw;` |
|    12 |  304 | `			aVal[12] = (ph7_int64)sUsage.ru_nswap;` |
|    12 |  305 | `			aVal[13] = (ph7_int64)sUsage.ru_utime.tv_usec;` |
|    12 |  306 | `			aVal[14] = (ph7_int64)sUsage.ru_utime.tv_sec;` |
|    12 |  307 | `			aVal[15] = (ph7_int64)sUsage.ru_stime.tv_usec;` |
|    12 |  308 | `			aVal[16] = (ph7_int64)sUsage.ru_stime.tv_sec;` |
|     6 |  309 | `		}` |
|     - |  310 | `	}` |
|     - |  311 | `#elif defined(__WINNT__)` |
|     - |  312 | `	{` |
|     - |  313 | `		FILETIME sCreate,sExit,sKernel,sUser;` |
|     - |  314 | `		BOOL bOk;` |
|     1 |  315 | `		if( iMode == 1 ){` |
|     1 |  316 | `			bOk = GetThreadTimes(GetCurrentThread(),&sCreate,&sExit,&sKernel,&sUser);` |
|     1 |  317 | `		}else{` |
|     1 |  318 | `			PROCESS_MEMORY_COUNTERS sMem = {0};` |
|     1 |  319 | `			bOk = GetProcessTimes(GetCurrentProcess(),&sCreate,&sExit,&sKernel,&sUser)` |
|     - |  320 | `			   && GetProcessMemoryInfo(GetCurrentProcess(),&sMem,sizeof(sMem));` |
|     1 |  321 | `			if( bOk ){` |
|     1 |  322 | `				aVal[4] = (ph7_int64)(sMem.PeakWorkingSetSize / 1024);` |
|     1 |  323 | `				aVal[8] = (ph7_int64)sMem.PageFaultCount;` |
|     - |  324 | `			}` |
|     - |  325 | `		}` |
|     1 |  326 | `		if( !bOk ){` |
|     - |  327 | `			/* php's own getrusage() failed, and php answers false. */` |
|   ! 0 |  328 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  329 | `			return PH7_OK;` |
|     - |  330 | `		}` |
|     - |  331 | `		{` |
|     - |  332 | `			/* Both are 100-nanosecond counts since the process started. */` |
|     1 |  333 | `			ULONGLONG uUser = ((ULONGLONG)sUser.dwHighDateTime << 32) \| sUser.dwLowDateTime;` |
|     1 |  334 | `			ULONGLONG uKern = ((ULONGLONG)sKernel.dwHighDateTime << 32) \| sKernel.dwLowDateTime;` |
|     1 |  335 | `			aVal[13] = (ph7_int64)((uUser / 10) % 1000000);` |
|     1 |  336 | `			aVal[14] = (ph7_int64)(uUser / 10000000);` |
|     1 |  337 | `			aVal[15] = (ph7_int64)((uKern / 10) % 1000000);` |
|     1 |  338 | `			aVal[16] = (ph7_int64)(uKern / 10000000);` |
|     - |  339 | `		}` |
|     - |  340 | `	}` |
|     - |  341 | `#endif` |
|    13 |  342 | `	pArray = ph7_context_new_array(pCtx);` |
|    13 |  343 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    13 |  344 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 |  345 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  346 | `	}` |
|     - |  347 | `#if defined(__WINNT__)` |
|     - |  348 | `	{` |
|     - |  349 | `		/* Only the six keys php's Windows build has, in its order. */` |
|     - |  350 | `		static const int aWin[6] = { 8, 4, 13, 14, 15, 16 };` |
|     1 |  351 | `		for( i = 0 ; i < 6 ; i++ ){` |
|     1 |  352 | `			ph7_value_int64(pVal,aVal[aWin[i]]);` |
|     1 |  353 | `			ph7_array_add_strkey_elem(pArray,azKey[aWin[i]],pVal);` |
|     1 |  354 | `		}` |
|     - |  355 | `	}` |
|     - |  356 | `#else` |
|   216 |  357 | `	for( i = 0 ; i < 17 ; i++ ){` |
|   204 |  358 | `		ph7_value_int64(pVal,aVal[i]);` |
|   204 |  359 | `		ph7_array_add_strkey_elem(pArray,azKey[i],pVal);` |
|   102 |  360 | `	}` |
|     - |  361 | `#endif` |
|    13 |  362 | `	ph7_result_value(pCtx,pArray);` |
|    13 |  363 | `	return PH7_OK;` |
|     7 |  364 | `}` |
|     - |  365 | `/*` |
|     - |  366 | ` * array\|int hrtime(bool $as_number = false)` |
|     - |  367 | ` *  The system's high-resolution time, counted from an arbitrary monotonic` |
|     - |  368 | ` *  point in nanoseconds. Returns [seconds, nanoseconds] by default, or the` |
|     - |  369 | ` *  total nanoseconds as an int when $as_number is true.` |
|     - |  370 | ` */` |
|     8 |  371 | `PH7_PRIVATE int PH7_builtin_hrtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  372 | `{` |
|     9 |  373 | `	ph7_int64 sec = 0,nsec = 0;` |
|     9 |  374 | `	int bAsNumber = 0;` |
|     9 |  375 | `	if( nArg > 0 ){` |
|     7 |  376 | `		bAsNumber = ph7_value_to_bool(apArg[0]);` |
|     3 |  377 | `	}` |
|     - |  378 | `#if defined(CLOCK_MONOTONIC)` |
|     - |  379 | `	{` |
|     - |  380 | `		struct timespec ts;` |
|     8 |  381 | `		if( clock_gettime(CLOCK_MONOTONIC,&ts) == 0 ){` |
|     8 |  382 | `			sec  = (ph7_int64)ts.tv_sec;` |
|     8 |  383 | `			nsec = (ph7_int64)ts.tv_nsec;` |
|     4 |  384 | `		}` |
|     - |  385 | `	}` |
|     - |  386 | `#else` |
|     - |  387 | `	{` |
|     - |  388 | `		/* No monotonic clock available: fall back to the wall-clock microsecond` |
|     - |  389 | `		 * source (embedder clock / gettimeofday). Coarser and not strictly` |
|     - |  390 | `		 * monotonic, but keeps hrtime() usable off-Unix. */` |
|     - |  391 | `		sytime sTime;` |
|     1 |  392 | `		DateNow(pCtx->pVm,&sTime);` |
|     1 |  393 | `		sec  = (ph7_int64)sTime.tm_sec;` |
|     1 |  394 | `		nsec = (ph7_int64)sTime.tm_usec * 1000;` |
|     - |  395 | `	}` |
|     - |  396 | `#endif` |
|     9 |  397 | `	if( bAsNumber ){` |
|     7 |  398 | `		ph7_result_int64(pCtx,sec * 1000000000LL + nsec);` |
|     4 |  399 | `	}else{` |
|     - |  400 | `		ph7_value *pValue,*pArray;` |
|     3 |  401 | `		pArray = ph7_context_new_array(pCtx);` |
|     3 |  402 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     3 |  403 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 |  404 | `			ph7_result_null(pCtx);` |
|   ! 0 |  405 | `			return PH7_OK;` |
|     - |  406 | `		}` |
|     3 |  407 | `		ph7_value_int64(pValue,sec);` |
|     3 |  408 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     3 |  409 | `		ph7_value_int64(pValue,nsec);` |
|     3 |  410 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     3 |  411 | `		ph7_result_value(pCtx,pArray);` |
|     3 |  412 | `		ph7_context_release_value(pCtx,pValue);` |
|     3 |  413 | `		ph7_context_release_value(pCtx,pArray);` |
|     - |  414 | `	}` |
|     9 |  415 | `	return PH7_OK;` |
|     5 |  416 | `}` |
|     - |  417 | `/*` |
|     - |  418 | ` * array getdate ([ int $timestamp = time() ])` |
|     - |  419 | ` *  Returns an associative array containing the date information` |
|     - |  420 | ` *  of the timestamp, or the current local time if no timestamp is given.` |
|     - |  421 | ` * Parameter` |
|     - |  422 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - |  423 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - |  424 | ` *     In other words, it defaults to the value of time().` |
|     - |  425 | ` * Returns` |
|     - |  426 | ` *  Returns an associative array of information related to the timestamp.` |
|     - |  427 | ` */` |
|    16 |  428 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  429 | `{` |
|     - |  430 | `	ph7_value *pValue,*pArray;` |
|     - |  431 | `	Sytm sTm;` |
|     - |  432 | `	time_t t;` |
|    17 |  433 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|     5 |  434 | `		time(&t);` |
|     3 |  435 | `	}else{` |
|     - |  436 | `		/* Use the given timestamp */` |
|    13 |  437 | `		t = (time_t)ph7_value_to_int64(apArg[0]);` |
|     - |  438 | `	}` |
|    17 |  439 | `	DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     - |  440 | `	/* Element value */` |
|    17 |  441 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    17 |  442 | `	if( pValue == 0 ){` |
|     - |  443 | `		/* Return NULL */` |
|   ! 0 |  444 | `		ph7_result_null(pCtx);` |
|   ! 0 |  445 | `		return PH7_OK;` |
|     - |  446 | `	}` |
|     - |  447 | `	/* Create a new array */` |
|    17 |  448 | `	pArray = ph7_context_new_array(pCtx);` |
|    17 |  449 | `	if( pArray == 0 ){` |
|     - |  450 | `		/* Return NULL */` |
|   ! 0 |  451 | `		ph7_result_null(pCtx);` |
|   ! 0 |  452 | `		return PH7_OK;` |
|     - |  453 | `	}` |
|     - |  454 | `	/* Fill the array */` |
|     - |  455 | `	/* Seconds */` |
|    17 |  456 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    17 |  457 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|     - |  458 | `	/* Minutes */` |
|    17 |  459 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    17 |  460 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|     - |  461 | `	/* Hours */` |
|    17 |  462 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    17 |  463 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|     - |  464 | `	/* mday */` |
|    17 |  465 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    17 |  466 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|     - |  467 | `	/* wday */` |
|    17 |  468 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    17 |  469 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|     - |  470 | `	/* mon */` |
|    17 |  471 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|    17 |  472 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|     - |  473 | `	/* year */` |
|    17 |  474 | `	ph7_value_int64(pValue,sTm.tm_year);` |
|    17 |  475 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|     - |  476 | `	/* yday */` |
|    17 |  477 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    17 |  478 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|     - |  479 | `	/* Weekday [i.e: Monday,Tuesday,...] */` |
|    17 |  480 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|    17 |  481 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|     - |  482 | `	/* Reset the string cursor */` |
|    17 |  483 | `	ph7_value_reset_string_cursor(pValue);` |
|     - |  484 | `	/* Month [i.e: January,February,...] */` |
|    17 |  485 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|    17 |  486 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|     - |  487 | `	/* php's eleventh entry, keyed by the INTEGER 0 and written last: the` |
|     - |  488 | `	 * timestamp the other ten were derived from. It was missing, so the` |
|     - |  489 | ``	 * documented `$g[0]` read as an Undefined array key. */`` |
|    17 |  490 | `	ph7_value_int64(pValue,(ph7_int64)t);` |
|    17 |  491 | `	ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - |  492 | `	/* Return the freshly created array */` |
|    17 |  493 | `	ph7_result_value(pCtx,pArray);` |
|    17 |  494 | `	return PH7_OK;` |
|     9 |  495 | `}` |
|     - |  496 | `/*` |
|     - |  497 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|     - |  498 | ` *  Returns an associative array containing the data returned from the system call.` |
|     - |  499 | ` * Parameters` |
|     - |  500 | ` *  $return_float` |
|     - |  501 | ` *   When set to TRUE, a float instead of an array is returned.` |
|     - |  502 | ` * Return` |
|     - |  503 | ` *  By default an array is returned. If return_float is set, then` |
|     - |  504 | ` *  a float is returned.` |
|     - |  505 | ` */` |
|     8 |  506 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  507 | `{` |
|     9 |  508 | `	int bFloat = 0;` |
|     - |  509 | `	sytime sTime;` |
|     9 |  510 | `	DateNow(pCtx->pVm,&sTime);` |
|     9 |  511 | `	if( nArg > 0 ){` |
|     7 |  512 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|     3 |  513 | `	}` |
|     9 |  514 | `	if( bFloat ){` |
|     - |  515 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|     5 |  516 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|     3 |  517 | `	}else{` |
|     - |  518 | `		/* Return an associative array */` |
|     - |  519 | `		ph7_value *pValue,*pArray;` |
|     - |  520 | `		/* Create a new array */` |
|     5 |  521 | `		pArray = ph7_context_new_array(pCtx);` |
|     - |  522 | `		/* Element value */` |
|     5 |  523 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     5 |  524 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|     - |  525 | `			/* Return NULL */` |
|   ! 0 |  526 | `			ph7_result_null(pCtx);` |
|   ! 0 |  527 | `			return PH7_OK;` |
|     - |  528 | `		}` |
|     - |  529 | `		/* Fill the array */` |
|     - |  530 | `		/* sec */` |
|     5 |  531 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|     5 |  532 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|     - |  533 | `		/* usec */` |
|     5 |  534 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|     5 |  535 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|     - |  536 | `		/* Return the array */` |
|     5 |  537 | `		ph7_result_value(pCtx,pArray);` |
|     - |  538 | `	}` |
|     9 |  539 | `	return PH7_OK;` |
|     5 |  540 | `}` |
|     - |  541 | `/* Check if the given year is leap or not */` |
|     - |  542 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|     - |  543 | `/* A year's magnitude, for the tokens php pads BEHIND the sign. Negating through` |
|     - |  544 | ` * sxu64 keeps the arithmetic defined even at the 64-bit floor. */` |
|     - |  545 | `#define DT_ABSYEAR(Y) ((sxi64)((Y) < 0 ? (sxu64)0 - (sxu64)(Y) : (sxu64)(Y)))` |
|     - |  546 | `/* ISO-8601 numeric representation of the day of the week */` |
|     - |  547 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|     - |  548 | `/*` |
|     - |  549 | ` * Format a given date string.` |
|     - |  550 | ` * Supported format: (Taken from PHP online docs)` |
|     - |  551 | ` * character 	Description` |
|     - |  552 | ` * d          Day of the month, 2 digits with leading zeros` |
|     - |  553 | ` * D          A textual representation of a day, three letters` |
|     - |  554 | ` * j          Day of the month without leading zeros` |
|     - |  555 | ` * l          A full textual representation of the day of the week` |
|     - |  556 | ` * N          ISO-8601 numeric representation of the day of the week` |
|     - |  557 | ` * w          Numeric representation of the day of the week` |
|     - |  558 | ` * z          The day of the year (starting from 0)` |
|     - |  559 | ` * F          A full textual representation of a month, such as January or March` |
|     - |  560 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|     - |  561 | ` * M          A short textual representation of a month, three letters` |
|     - |  562 | ` * n          Numeric representation of a month, without leading zeros` |
|     - |  563 | ` * t          Number of days in the given month` |
|     - |  564 | ` * L          Whether it's a leap year` |
|     - |  565 | ` * o          ISO-8601 year number. This has the same value as Y` |
|     - |  566 | ` * Y          A full numeric representation of a year, 4 digits` |
|     - |  567 | ` * y          A two digit representation of a year` |
|     - |  568 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|     - |  569 | ` * A          Uppercase Ante meridiem and Post meridiem` |
|     - |  570 | ` * g          12-hour format of an hour without leading zeros` |
|     - |  571 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|     - |  572 | ` * h          12-hour format of an hour with leading zeros` |
|     - |  573 | ` * H          24-hour format of an hour with leading zeros` |
|     - |  574 | ` * i          Minutes with leading zeros` |
|     - |  575 | ` * s          Seconds, with leading zeros` |
|     - |  576 | ` * u          Microseconds` |
|     - |  577 | ` * e          Timezone identifier` |
|     - |  578 | ` * I          Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|     - |  579 | ` * r          RFC 2822 formatted date` |
|     - |  580 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|     - |  581 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|     - |  582 | ` * O          Difference to Greenwich time (GMT) in hours` |
|     - |  583 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|     - |  584 | ` *            east of UTC is always positive.` |
|     - |  585 | ` * c         ISO 8601 date` |
|     - |  586 | ` */` |
|  3608 |  587 | `PH7_PRIVATE sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec)` |
|     3 |  588 | `{` |
|  3611 |  589 | `	const char *zEnd = &zIn[nLen];` |
|     - |  590 | `	const char *zCur;` |
|     - |  591 | `	/* Start the format process */` |
| 15783 |  592 | `	for(;;){` |
| 31569 |  593 | `		if( zIn >= zEnd ){` |
|     - |  594 | `			/* No more input to process */` |
|  3611 |  595 | `			break;` |
|     - |  596 | `		}` |
| 27961 |  597 | `		switch(zIn[0]){` |
|  1187 |  598 | `		case 'd':` |
|     - |  599 | `			/* Day of the month, 2 digits with leading zeros */` |
|  2376 |  600 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|  2376 |  601 | `			break;` |
|    84 |  602 | `		case 'D':` |
|     - |  603 | `			/*A textual representation of a day, three letters*/` |
|   169 |  604 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|   169 |  605 | `			ph7_result_string(pCtx,zCur,3);` |
|   169 |  606 | `			break;` |
|     1 |  607 | `		case 'j':` |
|     - |  608 | `			/*	Day of the month without leading zeros */` |
|     3 |  609 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|     3 |  610 | `			break;` |
|     3 |  611 | `		case 'l':` |
|     - |  612 | `			/* A full textual representation of the day of the week */` |
|     7 |  613 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|     7 |  614 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     7 |  615 | `			break;` |
|     1 |  616 | `		case 'N':{` |
|     - |  617 | `			/* ISO-8601 numeric representation of the day of the week */` |
|     3 |  618 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|     3 |  619 | `			break;` |
|     - |  620 | `				 }` |
|     1 |  621 | `		case 'w':` |
|     - |  622 | `			/*Numeric representation of the day of the week*/` |
|     3 |  623 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|     3 |  624 | `			break;` |
|     1 |  625 | `		case 'z':` |
|     - |  626 | `			/*The day of the year*/` |
|     3 |  627 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|     3 |  628 | `			break;` |
|     3 |  629 | `		case 'F':` |
|     - |  630 | `			/*A full textual representation of a month, such as January or March*/` |
|     7 |  631 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|     7 |  632 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     7 |  633 | `			break;` |
|  1187 |  634 | `		case 'm':` |
|     - |  635 | `			/*Numeric representation of a month, with leading zeros*/` |
|  2376 |  636 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|  2376 |  637 | `			break;` |
|     1 |  638 | `		case 'M':` |
|     - |  639 | `			/*A short textual representation of a month, three letters*/` |
|     3 |  640 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|     3 |  641 | `			ph7_result_string(pCtx,zCur,3);` |
|     3 |  642 | `			break;` |
|     1 |  643 | `		case 'n':` |
|     - |  644 | `			/*Numeric representation of a month, without leading zeros*/` |
|     3 |  645 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|     3 |  646 | `			break;` |
|     1 |  647 | `		case 't':{` |
|     - |  648 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|     3 |  649 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|     3 |  650 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|   ! 0 |  651 | `				nDays = 28;` |
|   ! 0 |  652 | `			}` |
|     - |  653 | `			/*Number of days in the given month*/` |
|     3 |  654 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|     3 |  655 | `			break;` |
|     - |  656 | `				 }` |
|     1 |  657 | `		case 'L':{` |
|     3 |  658 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|     - |  659 | `			/* Whether it's a leap year */` |
|     3 |  660 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|     3 |  661 | `			break;` |
|     - |  662 | `				 }` |
|    27 |  663 | `		case 'o': case 'W': {` |
|     - |  664 | `			/* ISO-8601 week-numbering year / week number: both belong to the` |
|     - |  665 | `			 * year owning the Thursday of the civil week (php: 2024-12-31 is` |
|     - |  666 | `			 * 2025-W01, 2027-01-01 is 2026-W53). php pads W but not o. */` |
|    55 |  667 | `			sxi64 days = DtDaysFromCivil(pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday);` |
|    55 |  668 | `			int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|    55 |  669 | `			sxi64 thu = days + (4 - isoDow);` |
|     - |  670 | `			sxi64 wy;` |
|     - |  671 | `			int wm,wd;` |
|    55 |  672 | `			DtCivilFromDays(thu,&wy,&wm,&wd);` |
|    55 |  673 | `			if( zIn[0] == 'o' ){` |
|    49 |  674 | `				ph7_result_string_format(pCtx,"%qd",wy);` |
|    25 |  675 | `			}else{` |
|    10 |  676 | `				ph7_result_string_format(pCtx,"%02d",` |
|     6 |  677 | `					(int)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1);` |
|     - |  678 | `			}` |
|    55 |  679 | `			break;` |
|     - |  680 | `				 }` |
|  1098 |  681 | `		case 'Y':` |
|     - |  682 | `			/* A full numeric representation of a year, at least 4 digits. php pads` |
|     - |  683 | `			 * the ABSOLUTE value behind the sign (-495 prints "-0495"); a plain` |
|     - |  684 | `			 * "%04qd" spends one of the four columns on the '-' and printed "-495". */` |
|  4360 |  685 | `			ph7_result_string_format(pCtx,"%s%04qd",` |
|  3260 |  686 | `				pTm->tm_year < 0 ? "-" : "",DT_ABSYEAR(pTm->tm_year));` |
|  2198 |  687 | `			break;` |
|    22 |  688 | `		case 'X':` |
|     - |  689 | `			/* Expanded full year, always signed (php 8.2+): +2024 */` |
|    80 |  690 | `			ph7_result_string_format(pCtx,"%c%04qd",` |
|    57 |  691 | `				pTm->tm_year < 0 ? '-' : '+',DT_ABSYEAR(pTm->tm_year));` |
|    45 |  692 | `			break;` |
|    22 |  693 | `		case 'x':` |
|     - |  694 | `			/* Expanded year, signed only past 4 digits (php 8.2+) — otherwise 'Y'. */` |
|    45 |  695 | `			if( pTm->tm_year > 9999 ){` |
|     5 |  696 | `				ph7_result_string_format(pCtx,"+%qd",pTm->tm_year);` |
|     3 |  697 | `			}else{` |
|    72 |  698 | `				ph7_result_string_format(pCtx,"%s%04qd",` |
|    51 |  699 | `					pTm->tm_year < 0 ? "-" : "",DT_ABSYEAR(pTm->tm_year));` |
|     - |  700 | `			}` |
|    45 |  701 | `			break;` |
|    22 |  702 | `		case 'y':` |
|     - |  703 | `			/*A two digit representation of a year*/` |
|    45 |  704 | `			ph7_result_string_format(pCtx,"%02qd",pTm->tm_year%100);` |
|    45 |  705 | `			break;` |
|     3 |  706 | `		case 'a':` |
|     - |  707 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|     7 |  708 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "pm" : "am",2);` |
|     7 |  709 | `			break;` |
|     3 |  710 | `		case 'A':` |
|     - |  711 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|     7 |  712 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "PM" : "AM",2);` |
|     7 |  713 | `			break;` |
|     1 |  714 | `		case 'B':{` |
|     - |  715 | `			/* Swatch Internet time: thousandths of the UTC+1 day. Only the time` |
|     - |  716 | `			 * OF DAY decides it, so take it from the clock fields rather than` |
|     - |  717 | `			 * rebuilding the absolute timestamp -- that product overflows an` |
|     - |  718 | `			 * int64 near the extremes of php's own clock. */` |
|     4 |  719 | `			sxi64 iBie = ((sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + pTm->tm_sec` |
|     2 |  720 | `				- (sxi64)pTm->tm_gmtoff + 3600) % 86400;` |
|     3 |  721 | `			if( iBie < 0 ){` |
|   ! 0 |  722 | `				iBie += 86400;` |
|   ! 0 |  723 | `			}` |
|     3 |  724 | `			ph7_result_string_format(pCtx,"%03d",(int)(iBie * 1000 / 86400));` |
|     3 |  725 | `			break;` |
|     - |  726 | `				 }` |
|     3 |  727 | `		case 'g':` |
|     - |  728 | `			/*	12-hour format of an hour without leading zeros*/` |
|    10 |  729 | `			ph7_result_string_format(pCtx,"%d",` |
|     6 |  730 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|     7 |  731 | `			break;` |
|     1 |  732 | `		case 'G':` |
|     - |  733 | `			/* 24-hour format of an hour without leading zeros */` |
|     3 |  734 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|     3 |  735 | `			break;` |
|     3 |  736 | `		case 'h':` |
|     - |  737 | `			/* 12-hour format of an hour with leading zeros */` |
|    10 |  738 | `			ph7_result_string_format(pCtx,"%02d",` |
|     6 |  739 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|     7 |  740 | `			break;` |
|  1064 |  741 | `		case 'H':` |
|     - |  742 | `			/*	24-hour format of an hour with leading zeros */` |
|  2129 |  743 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|  2129 |  744 | `			break;` |
|  1064 |  745 | `		case 'i':` |
|     - |  746 | `			/* 	Minutes with leading zeros */` |
|  2129 |  747 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|  2129 |  748 | `			break;` |
|  1030 |  749 | `		case 's':` |
|     - |  750 | `			/* 	second with leading zeros */` |
|  2061 |  751 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|  2061 |  752 | `			break;` |
|   359 |  753 | `		case 'u':` |
|     - |  754 | `			/* 	Microseconds. date()/gmdate() have no sub-second part (uSec == 0);` |
|     - |  755 | `			 * 	DateTime::format passes its stored microseconds. */` |
|   719 |  756 | `			ph7_result_string_format(pCtx,"%06d",uSec);` |
|   719 |  757 | `			break;` |
|     4 |  758 | `		case 'v':` |
|     - |  759 | `			/* 	Milliseconds */` |
|     9 |  760 | `			ph7_result_string_format(pCtx,"%03d",uSec/1000);` |
|     9 |  761 | `			break;` |
|     1 |  762 | `		case 'S':{` |
|     - |  763 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|     - |  764 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|     3 |  765 | `			int v = pTm->tm_mday;` |
|     3 |  766 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|     3 |  767 | `			break;` |
|     - |  768 | `				 }` |
|    82 |  769 | `		case 'e':` |
|     - |  770 | `			/* 	Timezone identifier */` |
|   165 |  771 | `			zCur = pTm->tm_zone;` |
|   165 |  772 | `			if( zCur == 0 ){` |
|     - |  773 | `				/* date()-family fills: the script default timezone */` |
|     7 |  774 | `				zCur = pCtx->pVm->zDefTz;` |
|     3 |  775 | `			}` |
|   165 |  776 | `			ph7_result_string(pCtx,zCur,-1);` |
|   165 |  777 | `			break;` |
|    53 |  778 | `		case 'T':{` |
|     - |  779 | `			/* Timezone abbreviation: "GMT+0530" for a fixed offset, the name` |
|     - |  780 | `			 * itself (uppercased) for a named zone. php decides on the zone's` |
|     - |  781 | `			 * TYPE, not on the offset's value, so a zone whose name is an offset` |
|     - |  782 | ``			 * spelling — `new DateTime('@0')`, `new DateTimeZone('+00:00')` —`` |
|     - |  783 | `			 * prints "GMT+0000" where PHL printed the name "+00:00". PHL has no` |
|     - |  784 | `			 * tz database, so the name path only ever sees UTC/GMT/Z. */` |
|     - |  785 | `			const char *z;` |
|   106 |  786 | `			if( pTm->tm_gmtoff != 0` |
|    73 |  787 | `			 \|\| (pTm->tm_zone && (pTm->tm_zone[0] == '+' \|\| pTm->tm_zone[0] == '-')) ){` |
|    87 |  788 | `				long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   130 |  789 | `				ph7_result_string_format(pCtx,"GMT%c%02d%02d",` |
|    86 |  790 | `					pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    87 |  791 | `				break;` |
|     - |  792 | `			}` |
|    21 |  793 | `			z = pTm->tm_zone ? pTm->tm_zone : pCtx->pVm->zDefTz;` |
|    73 |  794 | `			while( *z ){` |
|    53 |  795 | `				int c = (unsigned char)*z;` |
|    53 |  796 | `				if( c >= 'a' && c <= 'z' ){` |
|   ! 0 |  797 | `					c -= 'a' - 'A';` |
|   ! 0 |  798 | `				}` |
|    53 |  799 | `				ph7_result_string_format(pCtx,"%c",c);` |
|    53 |  800 | `				z++;` |
|     1 |  801 | `			}` |
|    21 |  802 | `			break;` |
|     - |  803 | `				 }` |
|     1 |  804 | `		case 'I':` |
|     - |  805 | `			/* Whether or not the date is in daylight saving time. Use the` |
|     - |  806 | `			 * broken-down time's own tm_isdst (as every other platform does):` |
|     - |  807 | `			 * the old Windows _get_daylight() override reported whether the` |
|     - |  808 | `			 * timezone observes DST at all, not whether THIS date is in it. */` |
|     3 |  809 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|     3 |  810 | `			break;` |
|    22 |  811 | `		case 'r':{` |
|     - |  812 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200 */` |
|    45 |  813 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     - |  814 | `			/* php zero-pads this year to four columns INCLUDING the sign` |
|     - |  815 | `			 * ("0050", "-001"), where a plain "%4d" space-padded every year` |
|     - |  816 | `			 * below 1000 — an ordinary date, not just a BCE one. */` |
|    45 |  817 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %04qd %02d:%02d:%02d %c%02d%02d",` |
|    22 |  818 | `				SyTimeGetDay(pTm->tm_wday),` |
|    22 |  819 | `				pTm->tm_mday,` |
|    22 |  820 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    22 |  821 | `				pTm->tm_year,` |
|    22 |  822 | `				pTm->tm_hour,` |
|    22 |  823 | `				pTm->tm_min,` |
|    22 |  824 | `				pTm->tm_sec,` |
|    44 |  825 | `				pTm->tm_gmtoff < 0 ? '-' : '+',` |
|    44 |  826 | `				(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  827 | `				);` |
|    45 |  828 | `			break;` |
|     - |  829 | `				 }` |
|    19 |  830 | `		case 'U':` |
|     - |  831 | `			/* Seconds since the Unix Epoch FOR THIS Sytm (php: the timestamp` |
|     - |  832 | `			 * being formatted — pre-fix this printed time(0) regardless of the` |
|     - |  833 | `			 * date under format). */` |
|    59 |  834 | `			ph7_result_string_format(pCtx,"%qd",(sxi64)(` |
|    38 |  835 | `				(sxu64)DtDaysFromCivil(pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400u` |
|    57 |  836 | `				+ (sxu64)((sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60` |
|    38 |  837 | `				          + (sxi64)pTm->tm_sec - (sxi64)pTm->tm_gmtoff)));` |
|    40 |  838 | `			break;` |
|    48 |  839 | `		case 'O':{` |
|     - |  840 | `			/* Difference to GMT without colon: +0530 (php) */` |
|    97 |  841 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   145 |  842 | `			ph7_result_string_format(pCtx,"%c%02d%02d",` |
|    96 |  843 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    97 |  844 | `			break;` |
|     - |  845 | `				 }` |
|   378 |  846 | `		case 'P':{` |
|     - |  847 | `			/* Difference to GMT with colon: +05:30 (php) */` |
|   757 |  848 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|  1135 |  849 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|   756 |  850 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|   757 |  851 | `			break;` |
|     - |  852 | `				 }` |
|    47 |  853 | `		case 'p':{` |
|     - |  854 | `			/* Like P, but "Z" for UTC (php 8.0+). Two rules PHL had wrong, both` |
|     - |  855 | `			 * visible only once a zone can carry SECONDS or be an abbreviation:` |
|     - |  856 | `			 * php decides on what P would have PRINTED rather than on the raw` |
|     - |  857 | `			 * offset, so "+00:00:59" prints Z and "-00:00:59" prints "-00:00";` |
|     - |  858 | `			 * and the GMT abbreviation is php's one exception -- it prints` |
|     - |  859 | `			 * "+00:00" where the UTC and Z spellings of the same instant print Z. */` |
|     - |  860 | `			long a;` |
|    94 |  861 | `			if( pTm->tm_gmtoff >= 0 && pTm->tm_gmtoff < 60` |
|    59 |  862 | `			 && !(pTm->tm_zone && SyStrncmp(pTm->tm_zone,"GMT",3) == 0` |
|    14 |  863 | `			      && pTm->tm_zone[3] == 0) ){` |
|    23 |  864 | `				ph7_result_string(pCtx,"Z",1);` |
|    23 |  865 | `				break;` |
|     - |  866 | `			}` |
|    73 |  867 | `			a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   109 |  868 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|    72 |  869 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    73 |  870 | `			break;` |
|     - |  871 | `				 }` |
|     2 |  872 | `		case 'Z':` |
|     - |  873 | `			/* Timezone offset in seconds, plain integer (php) */` |
|     5 |  874 | `			ph7_result_string_format(pCtx,"%d",(int)pTm->tm_gmtoff);` |
|     5 |  875 | `			break;` |
|    38 |  876 | `		case 'c':{` |
|     - |  877 | `			/* 	ISO 8601 date: 2004-02-12T15:19:21+00:00 (php) */` |
|    77 |  878 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     - |  879 | `			/* Same four-column zero pad as 'r' (php: "0050-…", "-001-…"). */` |
|   115 |  880 | `			ph7_result_string_format(pCtx,"%04qd-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",` |
|    38 |  881 | `				pTm->tm_year,` |
|    76 |  882 | `				pTm->tm_mon+1,` |
|    38 |  883 | `				pTm->tm_mday,` |
|    38 |  884 | `				pTm->tm_hour,` |
|    38 |  885 | `				pTm->tm_min,` |
|    38 |  886 | `				pTm->tm_sec,` |
|    76 |  887 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  888 | `				);` |
|    77 |  889 | `			break;` |
|     - |  890 | `				 }` |
|     4 |  891 | `		case '\\':` |
|     9 |  892 | `			zIn++;` |
|     - |  893 | `			/* Expand verbatim */` |
|     9 |  894 | `			if( zIn < zEnd ){` |
|     9 |  895 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     4 |  896 | `			}` |
|     9 |  897 | `			break;` |
|  6086 |  898 | `		default:` |
|     - |  899 | `			/* Unknown format specifer,expand verbatim */` |
| 12174 |  900 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
| 12172 |  901 | `			break;` |
|     - |  902 | `		}` |
|     - |  903 | `		/* Point to the next character */` |
| 27961 |  904 | `		zIn++;` |
|     3 |  905 | `	}` |
|  3611 |  906 | `	return SXRET_OK;` |
|     3 |  907 | `}` |
|     - |  908 | `/*` |
|     - |  909 | ` * Resolve a date()/gmdate() $timestamp argument under php 8's ?int weak ZPP:` |
|     - |  910 | ` *   - null            -> *pbUseNow = 1 (caller uses the current time)` |
|     - |  911 | ` *   - int/bool/float  -> coerce to a Unix timestamp (float truncates; php's` |
|     - |  912 | ` *                        float->int precision E_DEPRECATED is not emitted, §3.7)` |
|     - |  913 | ` *   - numeric string  -> coerce via php's is_numeric_string grammar` |
|     - |  914 | ` *                        (RangeStrToNumber: " 100 "/"1e3"/".5"/"+5" ok)` |
|     - |  915 | ` *   - anything else (non-numeric string, array, object, resource)` |
|     - |  916 | ` *                     -> catchable TypeError, byte-exact with php.` |
|     - |  917 | ` * Returns PH7_OK with *pbUseNow / *pT set, or the PH7_VmThrowException status.` |
|     - |  918 | ` */` |
|   474 |  919 | `static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)` |
|     1 |  920 | `{` |
|     - |  921 | `	char zBuf[64];` |
|   475 |  922 | `	*pbUseNow = 0;` |
|   475 |  923 | `	if( ph7_value_is_null(pArg) ){` |
|     3 |  924 | `		*pbUseNow = 1;` |
|     3 |  925 | `		return PH7_OK;` |
|     - |  926 | `	}` |
|   473 |  927 | `	if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_float(pArg) ){` |
|   465 |  928 | `		*pT = (time_t)ph7_value_to_int64(pArg);` |
|   465 |  929 | `		return PH7_OK;` |
|     - |  930 | `	}` |
|     9 |  931 | `	if( ph7_value_is_string(pArg) ){` |
|     - |  932 | `		int nStr;` |
|     9 |  933 | `		const char *zStr = ph7_value_to_string(pArg,&nStr);` |
|     - |  934 | `		sxi64 iLong; double dReal;` |
|     9 |  935 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|     9 |  936 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|     3 |  937 | `			*pT = (time_t)dReal;` |
|     6 |  938 | `			return PH7_OK;` |
|     - |  939 | `		}` |
|     7 |  940 | `		if( iKind == RANGE_IN_LONG ){` |
|     7 |  941 | `			*pT = (time_t)iLong;` |
|     7 |  942 | `			return PH7_OK;` |
|     - |  943 | `		}` |
|     - |  944 | `		/* Not a numeric string: fall through to the TypeError. */` |
|   ! 0 |  945 | `	}` |
|   ! 0 |  946 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  947 | `		"%s(): Argument #2 ($timestamp) must be of type ?int, %s given",` |
|   ! 0 |  948 | `		ph7_function_name(pCtx),VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|   238 |  949 | `}` |
|     - |  950 | `/*` |
|     - |  951 | ` * string date(string $format [, int $timestamp = time() ] )` |
|     - |  952 | ` *  Returns a string formatted according to the given format string using` |
|     - |  953 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|     - |  954 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|     - |  955 | ` * Parameters` |
|     - |  956 | ` *  $format` |
|     - |  957 | ` *   The format of the outputted date string (See code above)` |
|     - |  958 | ` * $timestamp` |
|     - |  959 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - |  960 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - |  961 | ` *   In other words, it defaults to the value of time().` |
|     - |  962 | ` * Return` |
|     - |  963 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - |  964 | ` */` |
|   414 |  965 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  966 | `{` |
|     - |  967 | `	const char *zFormat;` |
|     - |  968 | `	int nLen;` |
|     - |  969 | `	Sytm sTm;` |
|   415 |  970 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - |  971 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 |  972 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  973 | `		return PH7_OK;` |
|     - |  974 | `	}` |
|   415 |  975 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   415 |  976 | `	if( nLen < 1 ){` |
|     - |  977 | `		/* Don't bother processing return the empty string */` |
|   ! 0 |  978 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  979 | `	}` |
|   415 |  980 | `	if( nArg < 2 ){` |
|     - |  981 | `		time_t t;` |
|    35 |  982 | `		time(&t);` |
|    35 |  983 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|    18 |  984 | `	}else{` |
|     - |  985 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|   381 |  986 | `		time_t t = 0;` |
|     - |  987 | `		int bUseNow;` |
|   381 |  988 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|   381 |  989 | `		if( rc != PH7_OK ){` |
|   ! 0 |  990 | `			return rc;` |
|     - |  991 | `		}` |
|   381 |  992 | `		if( bUseNow ){` |
|   ! 0 |  993 | `			time(&t);` |
|   ! 0 |  994 | `		}` |
|   381 |  995 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     - |  996 | `	}` |
|     - |  997 | `	/* Format the given string */` |
|   415 |  998 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|   415 |  999 | `	return PH7_OK;` |
|   208 | 1000 | `}` |
|     - | 1001 | `/*` |
|     - | 1002 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|     - | 1003 | ` *  Identical to the date() function except that the time returned` |
|     - | 1004 | ` *  is Greenwich Mean Time (GMT).` |
|     - | 1005 | ` * Parameters` |
|     - | 1006 | ` *  $format` |
|     - | 1007 | ` *  The format of the outputted date string (See code above)` |
|     - | 1008 | ` *  $timestamp` |
|     - | 1009 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1010 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1011 | ` *   In other words, it defaults to the value of time().` |
|     - | 1012 | ` * Return` |
|     - | 1013 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - | 1014 | ` */` |
|   108 | 1015 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1016 | `{` |
|     - | 1017 | `	const char *zFormat;` |
|     - | 1018 | `	int nLen;` |
|     - | 1019 | `	Sytm sTm;` |
|   109 | 1020 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1021 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1022 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1023 | `		return PH7_OK;` |
|     - | 1024 | `	}` |
|   109 | 1025 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   109 | 1026 | `	if( nLen < 1 ){` |
|     - | 1027 | `		/* Don't bother processing return the empty string */` |
|   ! 0 | 1028 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1029 | `	}` |
|   109 | 1030 | `	if( nArg < 2 ){` |
|     - | 1031 | `		time_t t;` |
|    15 | 1032 | `		time(&t);` |
|    15 | 1033 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     8 | 1034 | `	}else{` |
|     - | 1035 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|    95 | 1036 | `		time_t t = 0;` |
|     - | 1037 | `		int bUseNow;` |
|    95 | 1038 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|    95 | 1039 | `		if( rc != PH7_OK ){` |
|   ! 0 | 1040 | `			return rc;` |
|     - | 1041 | `		}` |
|    95 | 1042 | `		if( bUseNow ){` |
|     3 | 1043 | `			time(&t);` |
|     1 | 1044 | `		}` |
|    95 | 1045 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     - | 1046 | `	}` |
|     - | 1047 | `	/* Format the given string */` |
|   109 | 1048 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|   109 | 1049 | `	return PH7_OK;` |
|    55 | 1050 | `}` |
|     - | 1051 | `/*` |
|     - | 1052 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|     - | 1053 | ` *  Return the local time.` |
|     - | 1054 | ` * Parameter` |
|     - | 1055 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1056 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - | 1057 | ` *     In other words, it defaults to the value of time().` |
|     - | 1058 | ` * $is_associative` |
|     - | 1059 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|     - | 1060 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|     - | 1061 | ` *   array containing all the different elements of the structure returned by the C function` |
|     - | 1062 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|     - | 1063 | ` *      "tm_sec" - seconds, 0 to 59` |
|     - | 1064 | ` *      "tm_min" - minutes, 0 to 59` |
|     - | 1065 | ` *      "tm_hour" - hours, 0 to 23` |
|     - | 1066 | ` *      "tm_mday" - day of the month, 1 to 31` |
|     - | 1067 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|     - | 1068 | ` *      "tm_year" - years since 1900` |
|     - | 1069 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|     - | 1070 | ` *      "tm_yday" - day of the year, 0 to 365` |
|     - | 1071 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|     - | 1072 | ` * Returns` |
|     - | 1073 | ` *  An associative array of information related to the timestamp.` |
|     - | 1074 | ` */` |
|    16 | 1075 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1076 | `{` |
|     - | 1077 | `	ph7_value *pValue,*pArray;` |
|    17 | 1078 | `	int isAssoc = 0;` |
|     - | 1079 | `	Sytm sTm;` |
|    17 | 1080 | `	if( nArg < 1 ){` |
|     - | 1081 | `		time_t t;` |
|     5 | 1082 | `		time(&t);` |
|     5 | 1083 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     3 | 1084 | `	}else{` |
|     - | 1085 | `		/* Use the given timestamp */` |
|     - | 1086 | `		time_t t;` |
|    13 | 1087 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    13 | 1088 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|     7 | 1089 | `		}else{` |
|   ! 0 | 1090 | `			time(&t);` |
|     - | 1091 | `		}` |
|    13 | 1092 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     - | 1093 | `	}` |
|     - | 1094 | `	/* Element value */` |
|    17 | 1095 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    17 | 1096 | `	if( pValue == 0 ){` |
|     - | 1097 | `		/* Return NULL */` |
|   ! 0 | 1098 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1099 | `		return PH7_OK;` |
|     - | 1100 | `	}` |
|     - | 1101 | `	/* Create a new array */` |
|    17 | 1102 | `	pArray = ph7_context_new_array(pCtx);` |
|    17 | 1103 | `	if( pArray == 0 ){` |
|     - | 1104 | `		/* Return NULL */` |
|   ! 0 | 1105 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1106 | `		return PH7_OK;` |
|     - | 1107 | `	}` |
|    17 | 1108 | `	if( nArg > 1 ){` |
|    11 | 1109 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|     5 | 1110 | `	}` |
|     - | 1111 | `	/* Fill the array */` |
|     - | 1112 | `	/* Seconds */` |
|    17 | 1113 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    17 | 1114 | `	if( isAssoc ){` |
|    11 | 1115 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|     6 | 1116 | `	}else{` |
|     7 | 1117 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1118 | `	}` |
|     - | 1119 | `	/* Minutes */` |
|    17 | 1120 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    17 | 1121 | `	if( isAssoc ){` |
|    11 | 1122 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|     6 | 1123 | `	}else{` |
|     7 | 1124 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1125 | `	}` |
|     - | 1126 | `	/* Hours */` |
|    17 | 1127 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    17 | 1128 | `	if( isAssoc ){` |
|    11 | 1129 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|     6 | 1130 | `	}else{` |
|     7 | 1131 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1132 | `	}` |
|     - | 1133 | `	/* mday */` |
|    17 | 1134 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    17 | 1135 | `	if( isAssoc ){` |
|    11 | 1136 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|     6 | 1137 | `	}else{` |
|     7 | 1138 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1139 | `	}` |
|     - | 1140 | `	/* mon */` |
|    17 | 1141 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|    17 | 1142 | `	if( isAssoc ){` |
|    11 | 1143 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|     6 | 1144 | `	}else{` |
|     7 | 1145 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1146 | `	}` |
|     - | 1147 | `	/* year since 1900 */` |
|    17 | 1148 | `	ph7_value_int64(pValue,sTm.tm_year-1900);` |
|    17 | 1149 | `	if( isAssoc ){` |
|    11 | 1150 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|     6 | 1151 | `	}else{` |
|     7 | 1152 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1153 | `	}` |
|     - | 1154 | `	/* wday */` |
|    17 | 1155 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    17 | 1156 | `	if( isAssoc ){` |
|    11 | 1157 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|     6 | 1158 | `	}else{` |
|     7 | 1159 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1160 | `	}` |
|     - | 1161 | `	/* yday */` |
|    17 | 1162 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    17 | 1163 | `	if( isAssoc ){` |
|    11 | 1164 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|     6 | 1165 | `	}else{` |
|     7 | 1166 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1167 | `	}` |
|     - | 1168 | `	/* isdst */` |
|     - | 1169 | `#ifdef __WINNT__` |
|     - | 1170 | `#ifdef _MSC_VER` |
|     - | 1171 | `#ifndef _WIN32_WCE` |
|     1 | 1172 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1173 | `#endif` |
|     - | 1174 | `#endif` |
|     - | 1175 | `#endif` |
|    17 | 1176 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|    17 | 1177 | `	if( isAssoc ){` |
|    11 | 1178 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|     6 | 1179 | `	}else{` |
|     7 | 1180 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1181 | `	}` |
|     - | 1182 | `	/* Return the array */` |
|    17 | 1183 | `	ph7_result_value(pCtx,pArray);` |
|    17 | 1184 | `	return PH7_OK;` |
|     9 | 1185 | `}` |
|     - | 1186 | `/*` |
|     - | 1187 | ` * int idate(string $format [, int $timestamp = time() ])` |
|     - | 1188 | ` *  Returns a number formatted according to the given format string` |
|     - | 1189 | ` *  using the given integer timestamp or the current local time if` |
|     - | 1190 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|     - | 1191 | ` *  to the value of time().` |
|     - | 1192 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|     - | 1193 | ` *  parameter.` |
|     - | 1194 | ` * $Parameters` |
|     - | 1195 | ` *  Supported format` |
|     - | 1196 | ` *   d 	Day of the month` |
|     - | 1197 | ` *   h 	Hour (12 hour format)` |
|     - | 1198 | ` *   H 	Hour (24 hour format)` |
|     - | 1199 | ` *   i 	Minutes` |
|     - | 1200 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|     - | 1201 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|     - | 1202 | ` *   m 	Month number` |
|     - | 1203 | ` *   s 	Seconds` |
|     - | 1204 | ` *   t 	Days in current month` |
|     - | 1205 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|     - | 1206 | ` *   w 	Day of the week (0 on Sunday)` |
|     - | 1207 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|     - | 1208 | ` *   y 	Year (1 or 2 digits - check note below)` |
|     - | 1209 | ` *   Y 	Year (4 digits)` |
|     - | 1210 | ` *   z 	Day of the year` |
|     - | 1211 | ` *   Z 	Timezone offset in seconds` |
|     - | 1212 | ` * $timestamp` |
|     - | 1213 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|     - | 1214 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|     - | 1215 | ` *  to the value of time().` |
|     - | 1216 | ` * Return` |
|     - | 1217 | ` *  An integer.` |
|     - | 1218 | ` */` |
|   196 | 1219 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1220 | `{` |
|     - | 1221 | `	const char *zFormat;` |
|   198 | 1222 | `	ph7_int64 iVal = 0;` |
|     - | 1223 | `	int nLen;` |
|     - | 1224 | `	Sytm sTm;` |
|   198 | 1225 | `	time_t t = 0; /* The resolved timestamp; 'U' must report THIS, not time(0) */` |
|   198 | 1226 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1227 | `		/* Missing/Invalid argument,return -1 */` |
|   ! 0 | 1228 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1229 | `		return PH7_OK;` |
|     - | 1230 | `	}` |
|   198 | 1231 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   198 | 1232 | `	if( nLen < 1 ){` |
|     - | 1233 | `		/* Don't bother processing return -1*/` |
|   ! 0 | 1234 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1235 | `	}` |
|   198 | 1236 | `	if( nArg < 2 ){` |
|    16 | 1237 | `		time(&t);` |
|    16 | 1238 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     9 | 1239 | `	}else{` |
|     - | 1240 | `		/* Use the given timestamp */` |
|   183 | 1241 | `		if( ph7_value_is_int(apArg[1]) ){` |
|   183 | 1242 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    92 | 1243 | `		}else{` |
|   ! 0 | 1244 | `			time(&t);` |
|     - | 1245 | `		}` |
|   183 | 1246 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     - | 1247 | `	}` |
|     - | 1248 | `	/* Perform the requested operation */` |
|   198 | 1249 | `	switch(zFormat[0]){` |
|     9 | 1250 | `	case 'd':` |
|     - | 1251 | `	case 'j':` |
|     - | 1252 | `		/* Day of the month ('j' differs from 'd' only in zero padding, which an` |
|     - | 1253 | `		 * integer result cannot carry) */` |
|    19 | 1254 | `		iVal = sTm.tm_mday;` |
|    19 | 1255 | `		break;` |
|     8 | 1256 | `	case 'h':` |
|     - | 1257 | `	case 'g':` |
|     - | 1258 | `		/* Hour (12 hour format): php reports midnight and noon as 12, not 0 —` |
|     - | 1259 | ``		 * `1 + hour % 12` answered 1 for both. */`` |
|    17 | 1260 | `		iVal = sTm.tm_hour % 12;` |
|    17 | 1261 | `		if( iVal == 0 ){` |
|    17 | 1262 | `			iVal = 12;` |
|     8 | 1263 | `		}` |
|    17 | 1264 | `		break;` |
|     9 | 1265 | `	case 'H':` |
|     - | 1266 | `	case 'G':` |
|     - | 1267 | `		/* Hour (24 hour format) */` |
|    19 | 1268 | `		iVal = sTm.tm_hour;` |
|    19 | 1269 | `		break;` |
|     5 | 1270 | `	case 'B': {` |
|     - | 1271 | `		/* Swatch Internet time: 1000 "beats" per day in UTC+1, no fractions.` |
|     - | 1272 | `		 * Integer math throughout so the tiny build (no floating point) agrees.` |
|     - | 1273 | ``		 * Read the time OF DAY off the broken-down clock: `t + 3600` overflows`` |
|     - | 1274 | `		 * at the top of php's timestamp range, which is undefined and answered` |
|     - | 1275 | `		 * the wrong beat. */` |
|    16 | 1276 | `		ph7_int64 iSec = ((ph7_int64)sTm.tm_hour*3600 + sTm.tm_min*60 + sTm.tm_sec` |
|    10 | 1277 | `			- sTm.tm_gmtoff + 3600) % 86400;` |
|    11 | 1278 | `		if( iSec < 0 ){` |
|   ! 0 | 1279 | `			iSec += 86400;` |
|   ! 0 | 1280 | `		}` |
|    11 | 1281 | `		iVal = iSec * 1000 / 86400;` |
|    11 | 1282 | `		break;` |
|     - | 1283 | `			  }` |
|     5 | 1284 | `	case 'i':` |
|     - | 1285 | `		/*Minutes*/` |
|    11 | 1286 | `		iVal = sTm.tm_min;` |
|    11 | 1287 | `		break;` |
|   ! 0 | 1288 | `	case 'I':` |
|     - | 1289 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|     - | 1290 | `#ifdef __WINNT__` |
|     - | 1291 | `#ifdef _MSC_VER` |
|     - | 1292 | `#ifndef _WIN32_WCE` |
|   ! 0 | 1293 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1294 | `#endif` |
|     - | 1295 | `#endif` |
|     - | 1296 | `#endif` |
|   ! 0 | 1297 | `		iVal = sTm.tm_isdst;` |
|   ! 0 | 1298 | `		break;` |
|     4 | 1299 | `	case 'L':` |
|     - | 1300 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|     9 | 1301 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|     9 | 1302 | `		break;` |
|     9 | 1303 | `	case 'm':` |
|     - | 1304 | `	case 'n':` |
|     - | 1305 | `		/* Month number. Sytm keeps tm_mon 0-based (see 't' below, which tests` |
|     - | 1306 | ``		 * `tm_mon == 1` for February), so July used to answer 6. */`` |
|    19 | 1307 | `		iVal = sTm.tm_mon + 1;` |
|    19 | 1308 | `		break;` |
|     5 | 1309 | `	case 's':` |
|     - | 1310 | `		/*Seconds*/` |
|    11 | 1311 | `		iVal = sTm.tm_sec;` |
|    11 | 1312 | `		break;` |
|     4 | 1313 | `	case 't':{` |
|     - | 1314 | `		/*Days in current month*/` |
|     - | 1315 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|     9 | 1316 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|     9 | 1317 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|   ! 0 | 1318 | `			nDays = 28;` |
|   ! 0 | 1319 | `		}` |
|     9 | 1320 | `		iVal = nDays;` |
|     9 | 1321 | `		break;` |
|     - | 1322 | `			 }` |
|     8 | 1323 | `	case 'U':` |
|     - | 1324 | `		/* Seconds since the Unix Epoch. This used to call time(0), ignoring the` |
|     - | 1325 | `		 * $timestamp argument entirely and always answering "now". */` |
|    17 | 1326 | `		iVal = (ph7_int64)t;` |
|    17 | 1327 | `		break;` |
|     4 | 1328 | `	case 'w':` |
|     - | 1329 | `		/*	Day of the week (0 on Sunday) */` |
|     9 | 1330 | `		iVal = sTm.tm_wday;` |
|     9 | 1331 | `		break;` |
|     8 | 1332 | `	case 'W':` |
|     - | 1333 | `	case 'o': {` |
|     - | 1334 | `		/* ISO-8601 week number / week-numbering year: both belong to the year` |
|     - | 1335 | `		 * owning the Thursday of the civil week, so 2021-01-01 is 2020-W53.` |
|     - | 1336 | `		 * The old code indexed a weekday table and returned a DAY number` |
|     - | 1337 | `		 * (1..7) as if it were a week number — idate("W") answered 4 in the` |
|     - | 1338 | `		 * middle of July. Same derivation as date()'s 'o'/'W' above. */` |
|    17 | 1339 | `		sxi64 days = DtDaysFromCivil(sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|    17 | 1340 | `		int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|    17 | 1341 | `		sxi64 thu = days + (4 - isoDow);` |
|     - | 1342 | `		sxi64 wy;` |
|     - | 1343 | `		int wm,wd;` |
|    17 | 1344 | `		DtCivilFromDays(thu,&wy,&wm,&wd);` |
|    17 | 1345 | `		if( zFormat[0] == 'o' ){` |
|     9 | 1346 | `			iVal = (ph7_int64)wy;` |
|     5 | 1347 | `		}else{` |
|     9 | 1348 | `			iVal = (ph7_int64)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1;` |
|     - | 1349 | `		}` |
|    17 | 1350 | `		break;` |
|     - | 1351 | `			  }` |
|     4 | 1352 | `	case 'y':` |
|     - | 1353 | `		/* Year (2 digits) */` |
|     9 | 1354 | `		iVal = sTm.tm_year % 100;` |
|     9 | 1355 | `		break;` |
|    10 | 1356 | `	case 'Y':` |
|     - | 1357 | `		/* Year (4 digits) */` |
|    21 | 1358 | `		iVal = sTm.tm_year;` |
|    21 | 1359 | `		break;` |
|     4 | 1360 | `	case 'z':` |
|     - | 1361 | `		/* Day of the year */` |
|     9 | 1362 | `		iVal = sTm.tm_yday;` |
|     9 | 1363 | `		break;` |
|   ! 0 | 1364 | `	case 'Z':` |
|     - | 1365 | `		/*Timezone offset in seconds*/` |
|   ! 0 | 1366 | `		iVal = sTm.tm_gmtoff;` |
|   ! 0 | 1367 | `		break;` |
|     2 | 1368 | `	default:` |
|     - | 1369 | `		/* Unknown token: php's own -1, which the shared tail below reports. */` |
|     6 | 1370 | `		iVal = -1;` |
|     4 | 1371 | `		break;` |
|     - | 1372 | `	}` |
|     - | 1373 | ``	/* php's idate answers a C `int`, so every token is narrowed to one -- and`` |
|     - | 1374 | `	 * -1 is the single value it uses for "no answer": it warns about the TOKEN` |
|     - | 1375 | `	 * and answers FALSE, whatever produced the -1. That is why` |
|     - | 1376 | ``	 * `idate('U', PHP_INT_MAX)` is a false with an "Unrecognized date format`` |
|     - | 1377 | `` 	 * token" warning in front of it rather than the timestamp. `idate() === false` `` |
|     - | 1378 | `	 * is the documented check, and 0 is a legitimate answer for several tokens. */` |
|   198 | 1379 | `	if( (int)iVal == -1 ){` |
|     8 | 1380 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unrecognized date format token");` |
|     8 | 1381 | `		ph7_result_bool(pCtx,0);` |
|     5 | 1382 | `	}else{` |
|   191 | 1383 | `		ph7_result_int64(pCtx,(int)iVal);` |
|     - | 1384 | `	}` |
|   198 | 1385 | `	return PH7_OK;` |
|   100 | 1386 | `}` |
|     - | 1387 | `/*` |
|     - | 1388 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|     - | 1389 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|     - | 1390 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|     - | 1391 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|     - | 1392 | ` *  specified.` |
|     - | 1393 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|     - | 1394 | ` *  the current value according to the local date and time.` |
|     - | 1395 | ` * Parameters` |
|     - | 1396 | ` * $hour` |
|     - | 1397 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|     - | 1398 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|     - | 1399 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|     - | 1400 | ` * $minute` |
|     - | 1401 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|     - | 1402 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|     - | 1403 | ` *  in the following hour(s).` |
|     - | 1404 | ` * $second` |
|     - | 1405 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|     - | 1406 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|     - | 1407 | ` * second in the following minute(s).` |
|     - | 1408 | ` * $month` |
|     - | 1409 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|     - | 1410 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|     - | 1411 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|     - | 1412 | ` * $day` |
|     - | 1413 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|     - | 1414 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|     - | 1415 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|     - | 1416 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|     - | 1417 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|     - | 1418 | ` * $year` |
|     - | 1419 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|     - | 1420 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|     - | 1421 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|     - | 1422 | ` * $is_dst` |
|     - | 1423 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|     - | 1424 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|     - | 1425 | ` * Return` |
|     - | 1426 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|     - | 1427 | ` *   If the arguments are invalid, the function returns FALSE` |
|     - | 1428 | ` */` |
|  4564 | 1429 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1430 | `{` |
|     - | 1431 | `	const char *zFunction;` |
|     - | 1432 | `	ph7_int64 iVal;` |
|     - | 1433 | `	sxi64 h,mi,s,mo,d,y,yAdj;` |
|     - | 1434 | `	int moN;` |
|     - | 1435 | `	struct tm *pTm;` |
|     - | 1436 | `	time_t t;` |
|     - | 1437 | `	/* Extract function name */` |
|  4565 | 1438 | `	zFunction = ph7_function_name(pCtx);` |
|     - | 1439 | `	/* PHP 8 dropped the legacy $is_dst 7th parameter: mktime()/gmmktime() now` |
|     - | 1440 | `	 * accept at most 6 arguments and throw a catchable ArgumentCountError` |
|     - | 1441 | `	 * otherwise (the central aBuiltinArity table only enforces the minimum, so` |
|     - | 1442 | `	 * this maximum is checked here). */` |
|  4565 | 1443 | `	if( nArg > 6 ){` |
|   ! 0 | 1444 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1445 | `			"%s() expects at most 6 arguments, %d given",zFunction,nArg);` |
|     - | 1446 | `	}` |
|  4565 | 1447 | `	if( nArg < 1 ){` |
|   ! 0 | 1448 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1449 | `			"%s() expects at least 1 argument, 0 given",zFunction);` |
|     - | 1450 | `	}` |
|     - | 1451 | `	/* Missing components default from the current time in php's default` |
|     - | 1452 | `	 * timezone. PHL's date_default_timezone_set() only accepts UTC/GMT (no tz` |
|     - | 1453 | `	 * database), so mktime() and gmmktime() agree and both read gmtime(). */` |
|  4565 | 1454 | `	time(&t);` |
|  4565 | 1455 | `	pTm = gmtime(&t);` |
|  2282 | 1456 | `	SXUNUSED(zFunction);` |
|  4565 | 1457 | `	h  = pTm->tm_hour;` |
|  4565 | 1458 | `	mi = pTm->tm_min;` |
|  4565 | 1459 | `	s  = pTm->tm_sec;` |
|  4565 | 1460 | `	mo = pTm->tm_mon + 1;` |
|  4565 | 1461 | `	d  = pTm->tm_mday;` |
|  4565 | 1462 | `	y  = pTm->tm_year + 1900;` |
|  4565 | 1463 | `	h = ph7_value_to_int64(apArg[0]);` |
|  4565 | 1464 | `	if( nArg > 1 ){` |
|  4565 | 1465 | `		mi = ph7_value_to_int64(apArg[1]);` |
|  4565 | 1466 | `		if( nArg > 2 ){` |
|  4565 | 1467 | `			s = ph7_value_to_int64(apArg[2]);` |
|  4565 | 1468 | `			if( nArg > 3 ){` |
|  4565 | 1469 | `				mo = ph7_value_to_int64(apArg[3]);` |
|  4565 | 1470 | `				if( nArg > 4 ){` |
|  4565 | 1471 | `					d = ph7_value_to_int64(apArg[4]);` |
|  4565 | 1472 | `					if( nArg > 5 ){` |
|     - | 1473 | `						/* php's legacy two-digit mapping: 0-69 -> 2000-2069,` |
|     - | 1474 | `						 * 70-100 -> 1970-2000; anything else is verbatim */` |
|  4565 | 1475 | `						y = ph7_value_to_int64(apArg[5]);` |
|  4565 | 1476 | `						if( y >= 0 && y <= 69 ){` |
|     7 | 1477 | `							y += 2000;` |
|  4562 | 1478 | `						}else if( y >= 70 && y <= 100 ){` |
|     5 | 1479 | `							y += 1900;` |
|     2 | 1480 | `						}` |
|  2282 | 1481 | `					}` |
|  2282 | 1482 | `				}` |
|  2282 | 1483 | `			}` |
|  2282 | 1484 | `		}` |
|  2282 | 1485 | `	}` |
|     - | 1486 | `	/* Normalize the month with floor semantics, then let day/time components` |
|     - | 1487 | `	 * overflow linearly (php: mktime(25,-30,0,1,1,2024) == Jan 2 00:30). */` |
|  4565 | 1488 | `	yAdj = y + DtFloorDiv(mo - 1,12);` |
|  4565 | 1489 | `	moN  = (int)(mo - 1 - DtFloorDiv(mo - 1,12) * 12) + 1;` |
|  4565 | 1490 | `	iVal = (DtDaysFromCivil(yAdj,moN,1) + (d - 1)) * 86400 + h*3600 + mi*60 + s;` |
|     - | 1491 | `	/* Return the timestamp as a 64bit integer */` |
|  4565 | 1492 | `	ph7_result_int64(pCtx,iVal);` |
|  4565 | 1493 | `	return PH7_OK;` |
|  2283 | 1494 | `}` |
|     - | 1495 | `/*` |
|     - | 1496 | ` * string date_default_timezone_get(void)` |
|     - | 1497 | ` *  Gets the default timezone used by all date/time functions in a script.` |
|     - | 1498 | ` */` |
|     6 | 1499 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1500 | `{` |
|     7 | 1501 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 | 1502 | `	SXUNUSED(nArg);` |
|     3 | 1503 | `	SXUNUSED(apArg);` |
|     7 | 1504 | `	ph7_result_string(pCtx,pVm->zDefTz,(int)pVm->nDefTz);` |
|     7 | 1505 | `	return PH7_OK;` |
|     1 | 1506 | `}` |
|     - | 1507 | `/*` |
|     - | 1508 | ` * bool date_default_timezone_set(string $timezoneId)` |
|     - | 1509 | ` *  Sets the default timezone used by all date/time functions in a script.` |
|     - | 1510 | ` *  php validates against the tz database and stores the id verbatim (get()` |
|     - | 1511 | ` *  echoes back "utc" if that's what was set). PHL ships no tz database, so` |
|     - | 1512 | ` *  only UTC and GMT are accepted; every other id — including region names php` |
|     - | 1513 | ` *  would accept — is rejected with php's invalid-id notice (recorded scope cut).` |
|     - | 1514 | ` */` |
|   146 | 1515 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1516 | `{` |
|   149 | 1517 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1518 | `	const char *zId;` |
|     - | 1519 | `	int nId;` |
|   149 | 1520 | `	if( nArg < 1 ){` |
|   ! 0 | 1521 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1522 | `		return PH7_OK;` |
|     - | 1523 | `	}` |
|   149 | 1524 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|   149 | 1525 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|   149 | 1526 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|   149 | 1527 | `		pVm->zDefTz[3] = 0;` |
|   149 | 1528 | `		pVm->nDefTz = 3;` |
|   149 | 1529 | `		ph7_result_bool(pCtx,1);` |
|   149 | 1530 | `		return PH7_OK;` |
|     - | 1531 | `	}` |
|     - | 1532 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|     - | 1533 | `	 * — exactly php's notice shape here */` |
|   ! 0 | 1534 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|   ! 0 | 1535 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 1536 | `	return PH7_OK;` |
|    76 | 1537 | `}` |
|     - | 1538 |  |
|     - | 1539 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 1540 |  |
