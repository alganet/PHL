# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 950/1025 lines (92.68%)

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
|     - |   14 | `#include <math.h>` |
|     - |   15 | `/* Civil-date helpers (defined with the DateTime layer below) */` |
|     - |   16 | `#ifdef __WINNT__` |
|     - |   17 | `#ifdef _MSC_VER` |
|     - |   18 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|     - |   19 | `#pragma warning(disable:4996) /* _CRT_SECURE_NO_WARNINGS */` |
|     - |   20 | `#endif` |
|     - |   21 | `#endif` |
|     - |   22 | `#endif` |
|     - |   23 | `#ifdef __WINNT__` |
|     - |   24 | `/* GetSystemTime() */` |
|     - |   25 | `#include <Windows.h>` |
|     - |   26 | `/* GetProcessMemoryInfo(), for getrusage() */` |
|     - |   27 | `#include <psapi.h>` |
|     - |   28 | `#ifdef _WIN32_WCE` |
|     - |   29 | `/* SPDX-SnippetBegin */` |
|     - |   30 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|     - |   31 | `/* SPDX-License-Identifier: blessing */` |
|     - |   32 | `/*` |
|     - |   33 | `** WindowsCE does not have a localtime() function.  So create a` |
|     - |   34 | `** substitute.` |
|     - |   35 | `** Taken from the SQLite3 source tree.` |
|     - |   36 | `** Status: Public domain` |
|     - |   37 | `*/` |
|     - |   38 | `struct tm *__cdecl localtime(const time_t *t)` |
|     - |   39 | `{` |
|     - |   40 | `  static struct tm y;` |
|     - |   41 | `  FILETIME uTm, lTm;` |
|     - |   42 | `  SYSTEMTIME pTm;` |
|     - |   43 | `  ph7_int64 t64;` |
|     - |   44 | `  t64 = *t;` |
|     - |   45 | `  t64 = (t64 + 11644473600)*10000000;` |
|     - |   46 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|     - |   47 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|     - |   48 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|     - |   49 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|     - |   50 | `  y.tm_year = pTm.wYear - 1900;` |
|     - |   51 | `  y.tm_mon = pTm.wMonth - 1;` |
|     - |   52 | `  y.tm_wday = pTm.wDayOfWeek;` |
|     - |   53 | `  y.tm_mday = pTm.wDay;` |
|     - |   54 | `  y.tm_hour = pTm.wHour;` |
|     - |   55 | `  y.tm_min = pTm.wMinute;` |
|     - |   56 | `  y.tm_sec = pTm.wSecond;` |
|     - |   57 | `  return &y;` |
|     - |   58 | `}` |
|     - |   59 | `/* SPDX-SnippetEnd */` |
|     - |   60 | `#endif /*_WIN32_WCE */` |
|     - |   61 | `#elif defined(__UNIXES__)` |
|     - |   62 | `#include <sys/time.h>` |
|     - |   63 | `#include <sys/resource.h>` |
|     - |   64 | `#endif /* __WINNT__*/` |
|     - |   65 | `/*` |
|     - |   66 | ` * Resolve the current wall-clock time (epoch seconds + sub-second microseconds).` |
|     - |   67 | ` *` |
|     - |   68 | ` * An embedder may override the platform clock via PH7_CONFIG_CLOCK (e.g. the` |
|     - |   69 | ` * ESP32 port routes this through esp_timer); when no hook is registered we use` |
|     - |   70 | ` * gettimeofday() on Unix and fall back to a second-resolution time() elsewhere.` |
|     - |   71 | ` * Centralising this here gives microtime()/gettimeofday() a single sub-second` |
|     - |   72 | `` * source instead of the old nonsensical `tt % SX_USEC_PER_SEC` off-Unix path.`` |
|     - |   73 | ` */` |
|  5648 |   74 | `static void DateNow(ph7_vm *pVm,sytime *pOut)` |
|     5 |   75 | `{` |
|  5653 |   76 | `	if( pVm && pVm->pEngine->xConf.xClock ){` |
|   ! 0 |   77 | `		ph7_int64 sec = 0,usec = 0;` |
|   ! 0 |   78 | `		if( pVm->pEngine->xConf.xClock(pVm->pEngine->xConf.pClockData,&sec,&usec) == PH7_OK ){` |
|   ! 0 |   79 | `			pOut->tm_sec  = (long)sec;` |
|   ! 0 |   80 | `			pOut->tm_usec = (long)usec;` |
|   ! 0 |   81 | `			return;` |
|     - |   82 | `		}` |
|   ! 0 |   83 | `	}` |
|     - |   84 | `#if defined(__UNIXES__)` |
|     - |   85 | `	{` |
|     - |   86 | `		struct timeval tv;` |
|  5648 |   87 | `		gettimeofday(&tv,0);` |
|  5648 |   88 | `		pOut->tm_sec  = (long)tv.tv_sec;` |
|  5648 |   89 | `		pOut->tm_usec = (long)tv.tv_usec;` |
|     - |   90 | `	}` |
|     - |   91 | `#elif defined(__WINNT__)` |
|     - |   92 | `	{` |
|     - |   93 | `		/* FILETIME is 100-ns ticks since 1601-01-01 UTC; convert to the Unix` |
|     - |   94 | `		 * epoch with microsecond resolution (GetSystemTime() only carries` |
|     - |   95 | `		 * milliseconds, and time() has no sub-second part at all).` |
|     - |   96 | `		 *` |
|     - |   97 | `		 * GetSystemTimeAsFileTime is a FILETIME with the timer-interrupt's` |
|     - |   98 | `		 * granularity behind it -- about 15.6ms by default -- so it does not` |
|     - |   99 | `		 * carry microseconds at all, whatever its unit says. php reads the` |
|     - |  100 | `		 * PRECISE call where the system has one (Windows 8 and up) for exactly` |
|     - |  101 | `		 * this reason, and resolves it at run time so an older system still` |
|     - |  102 | ``		 * links; without it `uniqid()` answers the same id for every call inside`` |
|     - |  103 | `		 * one timer tick, and microtime() has three useful digits. */` |
|     - |  104 | `		static void (WINAPI *xPrecise)(LPFILETIME) = 0;` |
|     - |  105 | `		static int bPreciseResolved = 0;` |
|     - |  106 | `		FILETIME ft;` |
|     - |  107 | `		ph7_int64 t;` |
|     5 |  108 | `		if( !bPreciseResolved ){` |
|     5 |  109 | `			HMODULE hKernel = GetModuleHandleA("kernel32.dll");` |
|     5 |  110 | `			if( hKernel ){` |
|     5 |  111 | `				xPrecise = (void (WINAPI *)(LPFILETIME))(void *)` |
|     - |  112 | `					GetProcAddress(hKernel,"GetSystemTimePreciseAsFileTime");` |
|     - |  113 | `			}` |
|     5 |  114 | `			bPreciseResolved = 1;` |
|     - |  115 | `		}` |
|     5 |  116 | `		if( xPrecise ){` |
|     5 |  117 | `			xPrecise(&ft);` |
|     5 |  118 | `		}else{` |
|   ! 0 |  119 | `			GetSystemTimeAsFileTime(&ft);` |
|     - |  120 | `		}` |
|     5 |  121 | `		t  = (ph7_int64)ft.dwHighDateTime << 32;` |
|     5 |  122 | `		t += ft.dwLowDateTime;` |
|     5 |  123 | `		t -= 116444736000000000LL; /* 100-ns ticks between 1601 and 1970 */` |
|     5 |  124 | `		pOut->tm_sec  = (long)(t / 10000000);` |
|     5 |  125 | `		pOut->tm_usec = (long)((t % 10000000) / 10);` |
|     - |  126 | `	}` |
|     - |  127 | `#else` |
|     - |  128 | `	{` |
|     - |  129 | `		time_t tt;` |
|     - |  130 | `		time(&tt);` |
|     - |  131 | `		pOut->tm_sec  = (long)tt;` |
|     - |  132 | `		pOut->tm_usec = 0; /* no sub-second source; embedders supply one via PH7_CONFIG_CLOCK */` |
|     - |  133 | `	}` |
|     - |  134 | `#endif /* __UNIXES__ */` |
|  2829 |  135 | `}` |
|     - |  136 | `/*` |
|     - |  137 | ` * ...and the same clock for a caller outside this file. php reads it in exactly` |
|     - |  138 | ` * two places that matter to a script: the date surface, and uniqid(), whose` |
|     - |  139 | `` * whole value is `%08x%05x` of these two numbers.`` |
|     - |  140 | ` */` |
|  1250 |  141 | `PH7_PRIVATE void PH7_VmClockNow(ph7_vm *pVm,ph7_int64 *pSec,ph7_int64 *pUsec)` |
|     2 |  142 | `{` |
|     - |  143 | `	sytime sNow;` |
|  1252 |  144 | `	sNow.tm_sec = 0;` |
|  1252 |  145 | `	sNow.tm_usec = 0;` |
|  1252 |  146 | `	DateNow(pVm,&sNow);` |
|  1252 |  147 | `	if( pSec ){` |
|  1252 |  148 | `		*pSec = (ph7_int64)sNow.tm_sec;` |
|   625 |  149 | `	}` |
|  1252 |  150 | `	if( pUsec ){` |
|  1252 |  151 | `		*pUsec = (ph7_int64)sNow.tm_usec;` |
|   625 |  152 | `	}` |
|  1252 |  153 | `}` |
|     - |  154 |  |
|     - |  155 | `/*` |
|     - |  156 | ` * The current moment as the DateTime layer wants it: epoch seconds plus the` |
|     - |  157 | ` * MICROSECONDS beside them.` |
|     - |  158 | ` *` |
|     - |  159 | ` * php's date classes take their base moment from the same clock microtime()` |
|     - |  160 | `` * reads, sub-second part included -- `new DateTime()` carries the microseconds`` |
|     - |  161 | `` * of the instant it was built, which is what makes `$a->diff($b)->f` mean`` |
|     - |  162 | ` * anything for two moments a program measured. The parse layer used to read` |
|     - |  163 | `` * `time(0)` directly, so every one of them was born on a whole second and every`` |
|     - |  164 | ` * such diff answered 0.0. Routing them through DateNow() also hands the date` |
|     - |  165 | ` * classes the PH7_CONFIG_CLOCK hook the procedural half already had.` |
|     - |  166 | ` */` |
|  4320 |  167 | `PH7_PRIVATE void DtNowUs(ph7_vm *pVm,sxi64 *piSec,int *puSec)` |
|     4 |  168 | `{` |
|     - |  169 | `	sytime sNow;` |
|  4324 |  170 | `	DateNow(pVm,&sNow);` |
|  4324 |  171 | `	if( piSec ){` |
|  4324 |  172 | `		*piSec = (sxi64)sNow.tm_sec;` |
|  2160 |  173 | `	}` |
|  4324 |  174 | `	if( puSec ){` |
|  4202 |  175 | `		*puSec = (int)sNow.tm_usec;` |
|  2099 |  176 | `	}` |
|  4324 |  177 | `}` |
|     - |  178 | `/*` |
|     - |  179 | ` * Break a Unix timestamp (or the current time) down into a Sytm the way the` |
|     - |  180 | ` * DateTime layer does: PHL's own civil arithmetic, not the platform's gmtime().` |
|     - |  181 | ` *` |
|     - |  182 | `` * gmtime() keeps its year in an `int` and simply FAILS past it, and the five`` |
|     - |  183 | ` * procedural doors below then formatted the CURRENT time instead -- so` |
|     - |  184 | `` * `date('Y', PHP_INT_MAX)` read today's year here where php reads`` |
|     - |  185 | ` * 292277026596. The engine has no tz database (date_default_timezone_set()` |
|     - |  186 | ` * takes UTC/GMT only), so date() and gmdate() share this UTC breakdown exactly` |
|     - |  187 | ` * as they already did through gmtime().` |
|     - |  188 | ` */` |
|   848 |  189 | `static void DtSytmOfTimestamp(ph7_vm *pVm,sxi64 iTs,Sytm *pOut)` |
|     4 |  190 | `{` |
|     - |  191 | `	/* A NULL tm_zone is what the date()-family fills mean by "the script's` |
|     - |  192 | `	 * default timezone" -- DateFormat's 'e'/'T' read pVm->zDefTz through it,` |
|     - |  193 | `	 * so naming the zone here would pin every one of them to UTC. */` |
|   852 |  194 | `	int iTz = DtDefaultTzIndex(pVm);` |
|   852 |  195 | `	int bDst = 0,nAbbr = 0;` |
|   852 |  196 | `	const char *zAbbr = 0;` |
|   852 |  197 | `	sxi32 iOff = DtTzOffsetOf(iTz,0,iTs,&bDst,&zAbbr,&nAbbr);` |
|   852 |  198 | `	DtFillSytm(iTs,iOff,0,pOut);` |
|     - |  199 | `	/* A DATABASE default carries what a fixed one cannot: an offset that` |
|     - |  200 | ``	 * depends on the instant, the ABBREVIATION `T` prints there ("CEST", not`` |
|     - |  201 | ``	 * the identifier), and the flag `I` prints. With a fixed default all three`` |
|     - |  202 | `	 * are the zero this always filled and every door answers as it did. */` |
|   852 |  203 | `	pOut->tm_isdst = bDst;` |
|   852 |  204 | `	pOut->tm_abbr = zAbbr;` |
|   852 |  205 | `	pOut->tm_nabbr = nAbbr;` |
|   852 |  206 | `}` |
|     - |  207 | `/*` |
|     - |  208 | `` * php's error-log timestamp, the `[d-M-Y H:i:s e] ` prefix php's own logger`` |
|     - |  209 | `` * writes in front of every line it appends to the `error_log` destination.`` |
|     - |  210 | ` *` |
|     - |  211 | `` * It lives here because the zone token is `e`, the timezone IDENTIFIER, and`` |
|     - |  212 | `` * that is the script's current default -- `date.timezone`, or whatever`` |
|     - |  213 | ` * date_default_timezone_set() moved it to since. The breakdown is the one the` |
|     - |  214 | `` * date() family takes, so a log line and a `date()` call in the same script`` |
|     - |  215 | ` * never disagree about what time it is. Nothing is appended after the prefix:` |
|     - |  216 | ` * the caller owns the message and the newline.` |
|     - |  217 | ` */` |
|     6 |  218 | `PH7_PRIVATE void PH7_VmLogTimestamp(ph7_vm *pVm,SyBlob *pOut)` |
|     1 |  219 | `{` |
|     - |  220 | `	const char *zMon;` |
|     - |  221 | `	Sytm sTm;` |
|     - |  222 | `	time_t t;` |
|     7 |  223 | `	time(&t);` |
|     7 |  224 | `	DtSytmOfTimestamp(pVm,(sxi64)t,&sTm);` |
|     7 |  225 | `	zMon = SyTimeGetMonth(sTm.tm_mon);` |
|    10 |  226 | `	SyBlobFormat(pOut,"[%02d-%.3s-%04d %02d:%02d:%02d %.*s] ",` |
|     3 |  227 | `		sTm.tm_mday,zMon,sTm.tm_year,sTm.tm_hour,sTm.tm_min,sTm.tm_sec,` |
|     6 |  228 | `		(int)pVm->nDefTz,pVm->zDefTz);` |
|     7 |  229 | `}` |
|     - |  230 | `/*` |
|     - |  231 | ` * The same breakdown for the gm* doors, which are UTC whatever the script's` |
|     - |  232 | ` * default zone is -- and whose zone FIELDS are not the default's either. php` |
|     - |  233 | `` * names the zone `UTC` there and abbreviates it `GMT`, so `gmdate('e T')` is`` |
|     - |  234 | ` * "UTC GMT" under every default; PHL answered "UTC UTC" because the abbreviation` |
|     - |  235 | ` * came from uppercasing the name, and would now have answered the default zone` |
|     - |  236 | ` * outright if these shared the door above.` |
|     - |  237 | ` */` |
|   148 |  238 | `static void DtSytmOfTimestampUtc(sxi64 iTs,Sytm *pOut)` |
|     2 |  239 | `{` |
|     - |  240 | `	static char zUtc[] = "UTC";` |
|   150 |  241 | `	DtFillSytm(iTs,0,zUtc,pOut);` |
|   150 |  242 | `	pOut->tm_abbr = "GMT";` |
|   150 |  243 | `	pOut->tm_nabbr = 3;` |
|   150 |  244 | `}` |
|     - |  245 | ` /*` |
|     - |  246 | `  * int64 time(void)` |
|     - |  247 | `  *  Current Unix timestamp` |
|     - |  248 | `  * Parameters` |
|     - |  249 | `  *  None.` |
|     - |  250 | `  * Return` |
|     - |  251 | `  *  Returns the current time measured in the number of seconds` |
|     - |  252 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|     - |  253 | `  */` |
|    20 |  254 | `PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  255 | `{` |
|     - |  256 | `	time_t tt;` |
|    10 |  257 | `	SXUNUSED(nArg); /* cc warning */` |
|    10 |  258 | `	SXUNUSED(apArg);` |
|     - |  259 | `	/* Extract the current time */` |
|    24 |  260 | `	time(&tt);` |
|     - |  261 | `	/* Return as 64-bit integer */` |
|    24 |  262 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|    24 |  263 | `	return  PH7_OK;` |
|     4 |  264 | `}` |
|     - |  265 | `/*` |
|     - |  266 | `  * string/float microtime([ bool $get_as_float = false ])` |
|     - |  267 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|     - |  268 | `  * Parameters` |
|     - |  269 | `  *  $get_as_float` |
|     - |  270 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|     - |  271 | `  *   as described in the return values section below.` |
|     - |  272 | `  * Return` |
|     - |  273 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|     - |  274 | `  *  is the current time measured in the number of seconds since the Unix` |
|     - |  275 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|     - |  276 | `  *  that have elapsed since sec expressed in seconds.` |
|     - |  277 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|     - |  278 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|     - |  279 | `  */` |
|    70 |  280 | `PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  281 | `{` |
|    73 |  282 | `	int bFloat = 0;` |
|     - |  283 | `	sytime sTime;` |
|    73 |  284 | `	DateNow(pCtx->pVm,&sTime);` |
|    73 |  285 | `	if( nArg > 0 ){` |
|    67 |  286 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|    32 |  287 | `	}` |
|    73 |  288 | `	if( bFloat ){` |
|     - |  289 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|    67 |  290 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|    35 |  291 | `	}else{` |
|     - |  292 | `		/* Return PHP's "msec sec" form: the sub-second part as fractional` |
|     - |  293 | `		 * seconds to 8 decimals, e.g. "0.50667100 1700000000". tm_usec is in` |
|     - |  294 | `		 * microseconds (0..999999), so scaling by 100 yields the 8-digit` |
|     - |  295 | `		 * fraction — matching PHP's "%.8F" output exactly. */` |
|     7 |  296 | `		ph7_result_string_format(pCtx,"0.%08ld %ld",sTime.tm_usec*100,sTime.tm_sec);` |
|     - |  297 | `	}` |
|    73 |  298 | `	return PH7_OK;` |
|     3 |  299 | `}` |
|     - |  300 | `/*` |
|     - |  301 | ` * array\|false getrusage(int $mode = 0)` |
|     - |  302 | ` *` |
|     - |  303 | `` * php's seventeen `getrusage(2)` fields, in php's own key ORDER (which is the`` |
|     - |  304 | ` * struct's reverse -- the two time pairs last, each with its microseconds before` |
|     - |  305 | `` * its seconds). $mode 1 asks for RUSAGE_CHILDREN; php answers `false` for any`` |
|     - |  306 | ` * other non-zero value.` |
|     - |  307 | ` *` |
|     - |  308 | ` * PHPUnit's telemetry calls it for every event it emits, so without it PHPUnit 13` |
|     - |  309 | `` * does not start at all -- `An error occurred inside PHPUnit. Call to undefined`` |
|     - |  310 | `` * function getrusage()`, before a single test runs.`` |
|     - |  311 | ` *` |
|     - |  312 | ` * php's Windows build has its own getrusage() and keys for only the SIX fields` |
|     - |  313 | ` * it fills: the page-fault count, the peak working set in KiB and the two` |
|     - |  314 | ` * CPU-time pairs. RUSAGE_CHILDREN is the same number as RUSAGE_THREAD there, so` |
|     - |  315 | ` * $mode 1 answers the calling thread's times (and 0 for the other two).` |
|     - |  316 | ` */` |
|    12 |  317 | `PH7_PRIVATE int PH7_builtin_getrusage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  318 | `{` |
|     - |  319 | `	ph7_value *pArray,*pVal;` |
|    13 |  320 | `	int iMode = 0;` |
|     - |  321 | `	/* php's key order, and the field each one carries. */` |
|     - |  322 | `	ph7_int64 aVal[17];` |
|     - |  323 | `	static const char *const azKey[17] = {` |
|     - |  324 | `		"ru_oublock","ru_inblock","ru_msgsnd","ru_msgrcv","ru_maxrss","ru_ixrss",` |
|     - |  325 | `		"ru_idrss","ru_minflt","ru_majflt","ru_nsignals","ru_nvcsw","ru_nivcsw",` |
|     - |  326 | `		"ru_nswap","ru_utime.tv_usec","ru_utime.tv_sec","ru_stime.tv_usec",` |
|     - |  327 | `		"ru_stime.tv_sec"` |
|     - |  328 | `	};` |
|     - |  329 | `	int i;` |
|    13 |  330 | `	if( nArg > 0 ){` |
|     7 |  331 | `		iMode = (int)ph7_value_to_int(apArg[0]);` |
|     3 |  332 | `	}` |
|     - |  333 | `	/* php asks the OS for RUSAGE_CHILDREN on 1 and RUSAGE_SELF on everything else` |
|     - |  334 | `	 * -- there is no refusal for an out-of-range mode, it simply is not 1. */` |
|   217 |  335 | `	for( i = 0 ; i < 17 ; i++ ){` |
|   205 |  336 | `		aVal[i] = 0;` |
|   103 |  337 | `	}` |
|     - |  338 | `#if defined(__UNIXES__)` |
|     - |  339 | `	{` |
|     - |  340 | `		struct rusage sUsage;` |
|    12 |  341 | `		if( getrusage(iMode == 1 ? RUSAGE_CHILDREN : RUSAGE_SELF,&sUsage) == 0 ){` |
|    12 |  342 | `			aVal[0]  = (ph7_int64)sUsage.ru_oublock;` |
|    12 |  343 | `			aVal[1]  = (ph7_int64)sUsage.ru_inblock;` |
|    12 |  344 | `			aVal[2]  = (ph7_int64)sUsage.ru_msgsnd;` |
|    12 |  345 | `			aVal[3]  = (ph7_int64)sUsage.ru_msgrcv;` |
|    12 |  346 | `			aVal[4]  = (ph7_int64)sUsage.ru_maxrss;` |
|    12 |  347 | `			aVal[5]  = (ph7_int64)sUsage.ru_ixrss;` |
|    12 |  348 | `			aVal[6]  = (ph7_int64)sUsage.ru_idrss;` |
|    12 |  349 | `			aVal[7]  = (ph7_int64)sUsage.ru_minflt;` |
|    12 |  350 | `			aVal[8]  = (ph7_int64)sUsage.ru_majflt;` |
|    12 |  351 | `			aVal[9]  = (ph7_int64)sUsage.ru_nsignals;` |
|    12 |  352 | `			aVal[10] = (ph7_int64)sUsage.ru_nvcsw;` |
|    12 |  353 | `			aVal[11] = (ph7_int64)sUsage.ru_nivcsw;` |
|    12 |  354 | `			aVal[12] = (ph7_int64)sUsage.ru_nswap;` |
|    12 |  355 | `			aVal[13] = (ph7_int64)sUsage.ru_utime.tv_usec;` |
|    12 |  356 | `			aVal[14] = (ph7_int64)sUsage.ru_utime.tv_sec;` |
|    12 |  357 | `			aVal[15] = (ph7_int64)sUsage.ru_stime.tv_usec;` |
|    12 |  358 | `			aVal[16] = (ph7_int64)sUsage.ru_stime.tv_sec;` |
|     6 |  359 | `		}` |
|     - |  360 | `	}` |
|     - |  361 | `#elif defined(__WINNT__)` |
|     - |  362 | `	{` |
|     - |  363 | `		FILETIME sCreate,sExit,sKernel,sUser;` |
|     - |  364 | `		BOOL bOk;` |
|     1 |  365 | `		if( iMode == 1 ){` |
|     1 |  366 | `			bOk = GetThreadTimes(GetCurrentThread(),&sCreate,&sExit,&sKernel,&sUser);` |
|     1 |  367 | `		}else{` |
|     1 |  368 | `			PROCESS_MEMORY_COUNTERS sMem = {0};` |
|     1 |  369 | `			bOk = GetProcessTimes(GetCurrentProcess(),&sCreate,&sExit,&sKernel,&sUser)` |
|     - |  370 | `			   && GetProcessMemoryInfo(GetCurrentProcess(),&sMem,sizeof(sMem));` |
|     1 |  371 | `			if( bOk ){` |
|     1 |  372 | `				aVal[4] = (ph7_int64)(sMem.PeakWorkingSetSize / 1024);` |
|     1 |  373 | `				aVal[8] = (ph7_int64)sMem.PageFaultCount;` |
|     - |  374 | `			}` |
|     - |  375 | `		}` |
|     1 |  376 | `		if( !bOk ){` |
|     - |  377 | `			/* php's own getrusage() failed, and php answers false. */` |
|   ! 0 |  378 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  379 | `			return PH7_OK;` |
|     - |  380 | `		}` |
|     - |  381 | `		{` |
|     - |  382 | `			/* Both are 100-nanosecond counts since the process started. */` |
|     1 |  383 | `			ULONGLONG uUser = ((ULONGLONG)sUser.dwHighDateTime << 32) \| sUser.dwLowDateTime;` |
|     1 |  384 | `			ULONGLONG uKern = ((ULONGLONG)sKernel.dwHighDateTime << 32) \| sKernel.dwLowDateTime;` |
|     1 |  385 | `			aVal[13] = (ph7_int64)((uUser / 10) % 1000000);` |
|     1 |  386 | `			aVal[14] = (ph7_int64)(uUser / 10000000);` |
|     1 |  387 | `			aVal[15] = (ph7_int64)((uKern / 10) % 1000000);` |
|     1 |  388 | `			aVal[16] = (ph7_int64)(uKern / 10000000);` |
|     - |  389 | `		}` |
|     - |  390 | `	}` |
|     - |  391 | `#endif` |
|    13 |  392 | `	pArray = ph7_context_new_array(pCtx);` |
|    13 |  393 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    13 |  394 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 |  395 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  396 | `	}` |
|     - |  397 | `#if defined(__WINNT__)` |
|     - |  398 | `	{` |
|     - |  399 | `		/* Only the six keys php's Windows build has, in its order. */` |
|     - |  400 | `		static const int aWin[6] = { 8, 4, 13, 14, 15, 16 };` |
|     1 |  401 | `		for( i = 0 ; i < 6 ; i++ ){` |
|     1 |  402 | `			ph7_value_int64(pVal,aVal[aWin[i]]);` |
|     1 |  403 | `			ph7_array_add_strkey_elem(pArray,azKey[aWin[i]],pVal);` |
|     1 |  404 | `		}` |
|     - |  405 | `	}` |
|     - |  406 | `#else` |
|   216 |  407 | `	for( i = 0 ; i < 17 ; i++ ){` |
|   204 |  408 | `		ph7_value_int64(pVal,aVal[i]);` |
|   204 |  409 | `		ph7_array_add_strkey_elem(pArray,azKey[i],pVal);` |
|   102 |  410 | `	}` |
|     - |  411 | `#endif` |
|    13 |  412 | `	ph7_result_value(pCtx,pArray);` |
|    13 |  413 | `	return PH7_OK;` |
|     7 |  414 | `}` |
|     - |  415 | `/*` |
|     - |  416 | ` * array\|int hrtime(bool $as_number = false)` |
|     - |  417 | ` *  The system's high-resolution time, counted from an arbitrary monotonic` |
|     - |  418 | ` *  point in nanoseconds. Returns [seconds, nanoseconds] by default, or the` |
|     - |  419 | ` *  total nanoseconds as an int when $as_number is true.` |
|     - |  420 | ` */` |
|     8 |  421 | `PH7_PRIVATE int PH7_builtin_hrtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  422 | `{` |
|     9 |  423 | `	ph7_int64 sec = 0,nsec = 0;` |
|     9 |  424 | `	int bAsNumber = 0;` |
|     9 |  425 | `	if( nArg > 0 ){` |
|     7 |  426 | `		bAsNumber = ph7_value_to_bool(apArg[0]);` |
|     3 |  427 | `	}` |
|     - |  428 | `#if defined(CLOCK_MONOTONIC)` |
|     - |  429 | `	{` |
|     - |  430 | `		struct timespec ts;` |
|     8 |  431 | `		if( clock_gettime(CLOCK_MONOTONIC,&ts) == 0 ){` |
|     8 |  432 | `			sec  = (ph7_int64)ts.tv_sec;` |
|     8 |  433 | `			nsec = (ph7_int64)ts.tv_nsec;` |
|     4 |  434 | `		}` |
|     - |  435 | `	}` |
|     - |  436 | `#else` |
|     - |  437 | `	{` |
|     - |  438 | `		/* No monotonic clock available: fall back to the wall-clock microsecond` |
|     - |  439 | `		 * source (embedder clock / gettimeofday). Coarser and not strictly` |
|     - |  440 | `		 * monotonic, but keeps hrtime() usable off-Unix. */` |
|     - |  441 | `		sytime sTime;` |
|     1 |  442 | `		DateNow(pCtx->pVm,&sTime);` |
|     1 |  443 | `		sec  = (ph7_int64)sTime.tm_sec;` |
|     1 |  444 | `		nsec = (ph7_int64)sTime.tm_usec * 1000;` |
|     - |  445 | `	}` |
|     - |  446 | `#endif` |
|     9 |  447 | `	if( bAsNumber ){` |
|     7 |  448 | `		ph7_result_int64(pCtx,sec * 1000000000LL + nsec);` |
|     4 |  449 | `	}else{` |
|     - |  450 | `		ph7_value *pValue,*pArray;` |
|     3 |  451 | `		pArray = ph7_context_new_array(pCtx);` |
|     3 |  452 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     3 |  453 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 |  454 | `			ph7_result_null(pCtx);` |
|   ! 0 |  455 | `			return PH7_OK;` |
|     - |  456 | `		}` |
|     3 |  457 | `		ph7_value_int64(pValue,sec);` |
|     3 |  458 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     3 |  459 | `		ph7_value_int64(pValue,nsec);` |
|     3 |  460 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     3 |  461 | `		ph7_result_value(pCtx,pArray);` |
|     3 |  462 | `		ph7_context_release_value(pCtx,pValue);` |
|     3 |  463 | `		ph7_context_release_value(pCtx,pArray);` |
|     - |  464 | `	}` |
|     9 |  465 | `	return PH7_OK;` |
|     5 |  466 | `}` |
|     - |  467 | `/*` |
|     - |  468 | ` * array getdate ([ int $timestamp = time() ])` |
|     - |  469 | ` *  Returns an associative array containing the date information` |
|     - |  470 | ` *  of the timestamp, or the current local time if no timestamp is given.` |
|     - |  471 | ` * Parameter` |
|     - |  472 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - |  473 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - |  474 | ` *     In other words, it defaults to the value of time().` |
|     - |  475 | ` * Returns` |
|     - |  476 | ` *  Returns an associative array of information related to the timestamp.` |
|     - |  477 | ` */` |
|    56 |  478 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  479 | `{` |
|     - |  480 | `	ph7_value *pValue,*pArray;` |
|     - |  481 | `	Sytm sTm;` |
|     - |  482 | `	time_t t;` |
|    58 |  483 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|     5 |  484 | `		time(&t);` |
|     3 |  485 | `	}else{` |
|     - |  486 | `		/* Use the given timestamp */` |
|    54 |  487 | `		t = (time_t)ph7_value_to_int64(apArg[0]);` |
|     - |  488 | `	}` |
|    58 |  489 | `	DtSytmOfTimestamp(pCtx->pVm,(sxi64)t,&sTm);` |
|     - |  490 | `	/* Element value */` |
|    58 |  491 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    58 |  492 | `	if( pValue == 0 ){` |
|     - |  493 | `		/* Return NULL */` |
|   ! 0 |  494 | `		ph7_result_null(pCtx);` |
|   ! 0 |  495 | `		return PH7_OK;` |
|     - |  496 | `	}` |
|     - |  497 | `	/* Create a new array */` |
|    58 |  498 | `	pArray = ph7_context_new_array(pCtx);` |
|    58 |  499 | `	if( pArray == 0 ){` |
|     - |  500 | `		/* Return NULL */` |
|   ! 0 |  501 | `		ph7_result_null(pCtx);` |
|   ! 0 |  502 | `		return PH7_OK;` |
|     - |  503 | `	}` |
|     - |  504 | `	/* Fill the array */` |
|     - |  505 | `	/* Seconds */` |
|    58 |  506 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    58 |  507 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|     - |  508 | `	/* Minutes */` |
|    58 |  509 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    58 |  510 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|     - |  511 | `	/* Hours */` |
|    58 |  512 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    58 |  513 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|     - |  514 | `	/* mday */` |
|    58 |  515 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    58 |  516 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|     - |  517 | `	/* wday */` |
|    58 |  518 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    58 |  519 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|     - |  520 | `	/* mon */` |
|    58 |  521 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|    58 |  522 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|     - |  523 | `	/* year */` |
|    58 |  524 | `	ph7_value_int64(pValue,sTm.tm_year);` |
|    58 |  525 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|     - |  526 | `	/* yday */` |
|    58 |  527 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    58 |  528 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|     - |  529 | `	/* Weekday [i.e: Monday,Tuesday,...] */` |
|    58 |  530 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|    58 |  531 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|     - |  532 | `	/* Reset the string cursor */` |
|    58 |  533 | `	ph7_value_reset_string_cursor(pValue);` |
|     - |  534 | `	/* Month [i.e: January,February,...] */` |
|    58 |  535 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|    58 |  536 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|     - |  537 | `	/* php's eleventh entry, keyed by the INTEGER 0 and written last: the` |
|     - |  538 | `	 * timestamp the other ten were derived from. It was missing, so the` |
|     - |  539 | ``	 * documented `$g[0]` read as an Undefined array key. */`` |
|    58 |  540 | `	ph7_value_int64(pValue,(ph7_int64)t);` |
|    58 |  541 | `	ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - |  542 | `	/* Return the freshly created array */` |
|    58 |  543 | `	ph7_result_value(pCtx,pArray);` |
|    58 |  544 | `	return PH7_OK;` |
|    30 |  545 | `}` |
|     - |  546 | `/*` |
|     - |  547 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|     - |  548 | ` *  Returns an associative array containing the data returned from the system call.` |
|     - |  549 | ` * Parameters` |
|     - |  550 | ` *  $return_float` |
|     - |  551 | ` *   When set to TRUE, a float instead of an array is returned.` |
|     - |  552 | ` * Return` |
|     - |  553 | ` *  By default an array is returned. If return_float is set, then` |
|     - |  554 | ` *  a float is returned.` |
|     - |  555 | ` */` |
|     8 |  556 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  557 | `{` |
|     9 |  558 | `	int bFloat = 0;` |
|     - |  559 | `	sytime sTime;` |
|     9 |  560 | `	DateNow(pCtx->pVm,&sTime);` |
|     9 |  561 | `	if( nArg > 0 ){` |
|     7 |  562 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|     3 |  563 | `	}` |
|     9 |  564 | `	if( bFloat ){` |
|     - |  565 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|     5 |  566 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|     3 |  567 | `	}else{` |
|     - |  568 | `		/* Return an associative array */` |
|     - |  569 | `		ph7_value *pValue,*pArray;` |
|     - |  570 | `		/* Create a new array */` |
|     5 |  571 | `		pArray = ph7_context_new_array(pCtx);` |
|     - |  572 | `		/* Element value */` |
|     5 |  573 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     5 |  574 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|     - |  575 | `			/* Return NULL */` |
|   ! 0 |  576 | `			ph7_result_null(pCtx);` |
|   ! 0 |  577 | `			return PH7_OK;` |
|     - |  578 | `		}` |
|     - |  579 | `		/* Fill the array */` |
|     - |  580 | `		/* sec */` |
|     5 |  581 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|     5 |  582 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|     - |  583 | `		/* usec */` |
|     5 |  584 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|     5 |  585 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|     - |  586 | `		/* Return the array */` |
|     5 |  587 | `		ph7_result_value(pCtx,pArray);` |
|     - |  588 | `	}` |
|     9 |  589 | `	return PH7_OK;` |
|     5 |  590 | `}` |
|     - |  591 | `/* Check if the given year is leap or not */` |
|     - |  592 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|     - |  593 | `/* A year's magnitude, for the tokens php pads BEHIND the sign. Negating through` |
|     - |  594 | ` * sxu64 keeps the arithmetic defined even at the 64-bit floor. */` |
|     - |  595 | `#define DT_ABSYEAR(Y) ((sxi64)((Y) < 0 ? (sxu64)0 - (sxu64)(Y) : (sxu64)(Y)))` |
|     - |  596 | `/* ISO-8601 numeric representation of the day of the week */` |
|     - |  597 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|     - |  598 | `/*` |
|     - |  599 | ` * Format a given date string.` |
|     - |  600 | ` * Supported format: (Taken from PHP online docs)` |
|     - |  601 | ` * character 	Description` |
|     - |  602 | ` * d          Day of the month, 2 digits with leading zeros` |
|     - |  603 | ` * D          A textual representation of a day, three letters` |
|     - |  604 | ` * j          Day of the month without leading zeros` |
|     - |  605 | ` * l          A full textual representation of the day of the week` |
|     - |  606 | ` * N          ISO-8601 numeric representation of the day of the week` |
|     - |  607 | ` * w          Numeric representation of the day of the week` |
|     - |  608 | ` * z          The day of the year (starting from 0)` |
|     - |  609 | ` * F          A full textual representation of a month, such as January or March` |
|     - |  610 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|     - |  611 | ` * M          A short textual representation of a month, three letters` |
|     - |  612 | ` * n          Numeric representation of a month, without leading zeros` |
|     - |  613 | ` * t          Number of days in the given month` |
|     - |  614 | ` * L          Whether it's a leap year` |
|     - |  615 | ` * o          ISO-8601 year number. This has the same value as Y` |
|     - |  616 | ` * Y          A full numeric representation of a year, 4 digits` |
|     - |  617 | ` * y          A two digit representation of a year` |
|     - |  618 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|     - |  619 | ` * A          Uppercase Ante meridiem and Post meridiem` |
|     - |  620 | ` * g          12-hour format of an hour without leading zeros` |
|     - |  621 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|     - |  622 | ` * h          12-hour format of an hour with leading zeros` |
|     - |  623 | ` * H          24-hour format of an hour with leading zeros` |
|     - |  624 | ` * i          Minutes with leading zeros` |
|     - |  625 | ` * s          Seconds, with leading zeros` |
|     - |  626 | ` * u          Microseconds` |
|     - |  627 | ` * e          Timezone identifier` |
|     - |  628 | ` * I          Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|     - |  629 | ` * r          RFC 2822 formatted date` |
|     - |  630 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|     - |  631 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|     - |  632 | ` * O          Difference to Greenwich time (GMT) in hours` |
|     - |  633 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|     - |  634 | ` *            east of UTC is always positive.` |
|     - |  635 | ` * c         ISO 8601 date` |
|     - |  636 | ` */` |
|  4580 |  637 | `PH7_PRIVATE sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec)` |
|     4 |  638 | `{` |
|  4584 |  639 | `	const char *zEnd = &zIn[nLen];` |
|     - |  640 | `	const char *zCur;` |
|     - |  641 | `	/* Start the format process */` |
| 20381 |  642 | `	for(;;){` |
| 40766 |  643 | `		if( zIn >= zEnd ){` |
|     - |  644 | `			/* No more input to process */` |
|  4584 |  645 | `			break;` |
|     - |  646 | `		}` |
| 36186 |  647 | `		switch(zIn[0]){` |
|  1410 |  648 | `		case 'd':` |
|     - |  649 | `			/* Day of the month, 2 digits with leading zeros */` |
|  2824 |  650 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|  2824 |  651 | `			break;` |
|    84 |  652 | `		case 'D':` |
|     - |  653 | `			/*A textual representation of a day, three letters*/` |
|   169 |  654 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|   169 |  655 | `			ph7_result_string(pCtx,zCur,3);` |
|   169 |  656 | `			break;` |
|     1 |  657 | `		case 'j':` |
|     - |  658 | `			/*	Day of the month without leading zeros */` |
|     3 |  659 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|     3 |  660 | `			break;` |
|     3 |  661 | `		case 'l':` |
|     - |  662 | `			/* A full textual representation of the day of the week */` |
|     7 |  663 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|     7 |  664 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     7 |  665 | `			break;` |
|     1 |  666 | `		case 'N':{` |
|     - |  667 | `			/* ISO-8601 numeric representation of the day of the week */` |
|     3 |  668 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|     3 |  669 | `			break;` |
|     - |  670 | `				 }` |
|     1 |  671 | `		case 'w':` |
|     - |  672 | `			/*Numeric representation of the day of the week*/` |
|     3 |  673 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|     3 |  674 | `			break;` |
|     1 |  675 | `		case 'z':` |
|     - |  676 | `			/*The day of the year*/` |
|     3 |  677 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|     3 |  678 | `			break;` |
|     3 |  679 | `		case 'F':` |
|     - |  680 | `			/*A full textual representation of a month, such as January or March*/` |
|     7 |  681 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|     7 |  682 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     7 |  683 | `			break;` |
|  1410 |  684 | `		case 'm':` |
|     - |  685 | `			/*Numeric representation of a month, with leading zeros*/` |
|  2824 |  686 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|  2824 |  687 | `			break;` |
|     1 |  688 | `		case 'M':` |
|     - |  689 | `			/*A short textual representation of a month, three letters*/` |
|     3 |  690 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|     3 |  691 | `			ph7_result_string(pCtx,zCur,3);` |
|     3 |  692 | `			break;` |
|     1 |  693 | `		case 'n':` |
|     - |  694 | `			/*Numeric representation of a month, without leading zeros*/` |
|     3 |  695 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|     3 |  696 | `			break;` |
|     1 |  697 | `		case 't':{` |
|     - |  698 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|     3 |  699 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|     3 |  700 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|   ! 0 |  701 | `				nDays = 28;` |
|   ! 0 |  702 | `			}` |
|     - |  703 | `			/*Number of days in the given month*/` |
|     3 |  704 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|     3 |  705 | `			break;` |
|     - |  706 | `				 }` |
|     1 |  707 | `		case 'L':{` |
|     3 |  708 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|     - |  709 | `			/* Whether it's a leap year */` |
|     3 |  710 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|     3 |  711 | `			break;` |
|     - |  712 | `				 }` |
|    27 |  713 | `		case 'o': case 'W': {` |
|     - |  714 | `			/* ISO-8601 week-numbering year / week number: both belong to the` |
|     - |  715 | `			 * year owning the Thursday of the civil week (php: 2024-12-31 is` |
|     - |  716 | `			 * 2025-W01, 2027-01-01 is 2026-W53). php pads W but not o. */` |
|    55 |  717 | `			sxi64 days = DtDaysFromCivil(pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday);` |
|    55 |  718 | `			int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|    55 |  719 | `			sxi64 thu = days + (4 - isoDow);` |
|     - |  720 | `			sxi64 wy;` |
|     - |  721 | `			int wm,wd;` |
|    55 |  722 | `			DtCivilFromDays(thu,&wy,&wm,&wd);` |
|    55 |  723 | `			if( zIn[0] == 'o' ){` |
|    49 |  724 | `				ph7_result_string_format(pCtx,"%qd",wy);` |
|    25 |  725 | `			}else{` |
|    10 |  726 | `				ph7_result_string_format(pCtx,"%02d",` |
|     6 |  727 | `					(int)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1);` |
|     - |  728 | `			}` |
|    55 |  729 | `			break;` |
|     - |  730 | `				 }` |
|  1321 |  731 | `		case 'Y':` |
|     - |  732 | `			/* A full numeric representation of a year, at least 4 digits. php pads` |
|     - |  733 | `			 * the ABSOLUTE value behind the sign (-495 prints "-0495"); a plain` |
|     - |  734 | `			 * "%04qd" spends one of the four columns on the '-' and printed "-495". */` |
|  5254 |  735 | `			ph7_result_string_format(pCtx,"%s%04qd",` |
|  3929 |  736 | `				pTm->tm_year < 0 ? "-" : "",DT_ABSYEAR(pTm->tm_year));` |
|  2646 |  737 | `			break;` |
|    22 |  738 | `		case 'X':` |
|     - |  739 | `			/* Expanded full year, always signed (php 8.2+): +2024 */` |
|    80 |  740 | `			ph7_result_string_format(pCtx,"%c%04qd",` |
|    57 |  741 | `				pTm->tm_year < 0 ? '-' : '+',DT_ABSYEAR(pTm->tm_year));` |
|    45 |  742 | `			break;` |
|    22 |  743 | `		case 'x':` |
|     - |  744 | `			/* Expanded year, signed only past 4 digits (php 8.2+) — otherwise 'Y'. */` |
|    45 |  745 | `			if( pTm->tm_year > 9999 ){` |
|     5 |  746 | `				ph7_result_string_format(pCtx,"+%qd",pTm->tm_year);` |
|     3 |  747 | `			}else{` |
|    72 |  748 | `				ph7_result_string_format(pCtx,"%s%04qd",` |
|    51 |  749 | `					pTm->tm_year < 0 ? "-" : "",DT_ABSYEAR(pTm->tm_year));` |
|     - |  750 | `			}` |
|    45 |  751 | `			break;` |
|    22 |  752 | `		case 'y':` |
|     - |  753 | `			/*A two digit representation of a year*/` |
|    45 |  754 | `			ph7_result_string_format(pCtx,"%02qd",pTm->tm_year%100);` |
|    45 |  755 | `			break;` |
|     3 |  756 | `		case 'a':` |
|     - |  757 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|     7 |  758 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "pm" : "am",2);` |
|     7 |  759 | `			break;` |
|     3 |  760 | `		case 'A':` |
|     - |  761 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|     7 |  762 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "PM" : "AM",2);` |
|     7 |  763 | `			break;` |
|     1 |  764 | `		case 'B':{` |
|     - |  765 | `			/* Swatch Internet time: thousandths of the UTC+1 day. Only the time` |
|     - |  766 | `			 * OF DAY decides it, so take it from the clock fields rather than` |
|     - |  767 | `			 * rebuilding the absolute timestamp -- that product overflows an` |
|     - |  768 | `			 * int64 near the extremes of php's own clock. */` |
|     4 |  769 | `			sxi64 iBie = ((sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + pTm->tm_sec` |
|     2 |  770 | `				- (sxi64)pTm->tm_gmtoff + 3600) % 86400;` |
|     3 |  771 | `			if( iBie < 0 ){` |
|   ! 0 |  772 | `				iBie += 86400;` |
|   ! 0 |  773 | `			}` |
|     3 |  774 | `			ph7_result_string_format(pCtx,"%03d",(int)(iBie * 1000 / 86400));` |
|     3 |  775 | `			break;` |
|     - |  776 | `				 }` |
|     3 |  777 | `		case 'g':` |
|     - |  778 | `			/*	12-hour format of an hour without leading zeros*/` |
|    10 |  779 | `			ph7_result_string_format(pCtx,"%d",` |
|     6 |  780 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|     7 |  781 | `			break;` |
|     1 |  782 | `		case 'G':` |
|     - |  783 | `			/* 24-hour format of an hour without leading zeros */` |
|     3 |  784 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|     3 |  785 | `			break;` |
|     3 |  786 | `		case 'h':` |
|     - |  787 | `			/* 12-hour format of an hour with leading zeros */` |
|    10 |  788 | `			ph7_result_string_format(pCtx,"%02d",` |
|     6 |  789 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|     7 |  790 | `			break;` |
|  1287 |  791 | `		case 'H':` |
|     - |  792 | `			/*	24-hour format of an hour with leading zeros */` |
|  2577 |  793 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|  2577 |  794 | `			break;` |
|  1287 |  795 | `		case 'i':` |
|     - |  796 | `			/* 	Minutes with leading zeros */` |
|  2577 |  797 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|  2577 |  798 | `			break;` |
|  1253 |  799 | `		case 's':` |
|     - |  800 | `			/* 	second with leading zeros */` |
|  2509 |  801 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|  2509 |  802 | `			break;` |
|   359 |  803 | `		case 'u':` |
|     - |  804 | `			/* 	Microseconds. date()/gmdate() have no sub-second part (uSec == 0);` |
|     - |  805 | `			 * 	DateTime::format passes its stored microseconds. */` |
|   719 |  806 | `			ph7_result_string_format(pCtx,"%06d",uSec);` |
|   719 |  807 | `			break;` |
|     4 |  808 | `		case 'v':` |
|     - |  809 | `			/* 	Milliseconds */` |
|     9 |  810 | `			ph7_result_string_format(pCtx,"%03d",uSec/1000);` |
|     9 |  811 | `			break;` |
|     1 |  812 | `		case 'S':{` |
|     - |  813 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|     - |  814 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|     3 |  815 | `			int v = pTm->tm_mday;` |
|     3 |  816 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|     3 |  817 | `			break;` |
|     - |  818 | `				 }` |
|   137 |  819 | `		case 'e':` |
|     - |  820 | `			/* 	Timezone identifier */` |
|   277 |  821 | `			zCur = pTm->tm_zone;` |
|   277 |  822 | `			if( zCur == 0 ){` |
|     - |  823 | `				/* date()-family fills: the script default timezone */` |
|    48 |  824 | `				zCur = pCtx->pVm->zDefTz;` |
|    23 |  825 | `			}` |
|   277 |  826 | `			ph7_result_string(pCtx,zCur,-1);` |
|   277 |  827 | `			break;` |
|   295 |  828 | `		case 'T':{` |
|     - |  829 | `			/* Timezone abbreviation: "GMT+0530" for a fixed offset, the name` |
|     - |  830 | `			 * itself (uppercased) for a named zone. php decides on the zone's` |
|     - |  831 | `			 * TYPE, not on the offset's value, so a zone whose name is an offset` |
|     - |  832 | ``			 * spelling — `new DateTime('@0')`, `new DateTimeZone('+00:00')` —`` |
|     - |  833 | `			 * prints "GMT+0000" where PHL printed the name "+00:00". PHL has no` |
|     - |  834 | `			 * tz database, so the name path only ever sees UTC/GMT/Z.` |
|     - |  835 | `			 *` |
|     - |  836 | `			 * A DATABASE zone brings its own answer and neither branch below` |
|     - |  837 | `			 * applies: the abbreviation is whatever tzdata wrote for that` |
|     - |  838 | `			 * instant, which is a word for some zones ("EDT", "MSK") and an` |
|     - |  839 | `			 * offset spelling for others ("+0545", "-03"). */` |
|     - |  840 | `			const char *z;` |
|   593 |  841 | `			if( pTm->tm_abbr ){` |
|   477 |  842 | `				ph7_result_string(pCtx,pTm->tm_abbr,pTm->tm_nabbr);` |
|   477 |  843 | `				break;` |
|     - |  844 | `			}` |
|   116 |  845 | `			if( pTm->tm_gmtoff != 0` |
|    82 |  846 | `			 \|\| (pTm->tm_zone && (pTm->tm_zone[0] == '+' \|\| pTm->tm_zone[0] == '-')) ){` |
|    96 |  847 | `				long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   143 |  848 | `				ph7_result_string_format(pCtx,"GMT%c%02d%02d",` |
|    94 |  849 | `					pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    96 |  850 | `				break;` |
|     - |  851 | `			}` |
|    24 |  852 | `			z = pTm->tm_zone ? pTm->tm_zone : pCtx->pVm->zDefTz;` |
|    90 |  853 | `			while( *z ){` |
|    68 |  854 | `				int c = (unsigned char)*z;` |
|    68 |  855 | `				if( c >= 'a' && c <= 'z' ){` |
|   ! 0 |  856 | `					c -= 'a' - 'A';` |
|   ! 0 |  857 | `				}` |
|    68 |  858 | `				ph7_result_string_format(pCtx,"%c",c);` |
|    68 |  859 | `				z++;` |
|     2 |  860 | `			}` |
|    24 |  861 | `			break;` |
|     - |  862 | `				 }` |
|   364 |  863 | `		case 'I':` |
|     - |  864 | `			/* Whether or not the date is in daylight saving time. Use the` |
|     - |  865 | `			 * broken-down time's own tm_isdst (as every other platform does):` |
|     - |  866 | `			 * the old Windows _get_daylight() override reported whether the` |
|     - |  867 | `			 * timezone observes DST at all, not whether THIS date is in it. */` |
|   731 |  868 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|   731 |  869 | `			break;` |
|    22 |  870 | `		case 'r':{` |
|     - |  871 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200 */` |
|    45 |  872 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     - |  873 | `			/* php zero-pads this year to four columns INCLUDING the sign` |
|     - |  874 | `			 * ("0050", "-001"), where a plain "%4d" space-padded every year` |
|     - |  875 | `			 * below 1000 — an ordinary date, not just a BCE one. */` |
|    45 |  876 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %04qd %02d:%02d:%02d %c%02d%02d",` |
|    22 |  877 | `				SyTimeGetDay(pTm->tm_wday),` |
|    22 |  878 | `				pTm->tm_mday,` |
|    22 |  879 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    22 |  880 | `				pTm->tm_year,` |
|    22 |  881 | `				pTm->tm_hour,` |
|    22 |  882 | `				pTm->tm_min,` |
|    22 |  883 | `				pTm->tm_sec,` |
|    44 |  884 | `				pTm->tm_gmtoff < 0 ? '-' : '+',` |
|    44 |  885 | `				(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  886 | `				);` |
|    45 |  887 | `			break;` |
|     - |  888 | `				 }` |
|    23 |  889 | `		case 'U':` |
|     - |  890 | `			/* Seconds since the Unix Epoch FOR THIS Sytm (php: the timestamp` |
|     - |  891 | `			 * being formatted — pre-fix this printed time(0) regardless of the` |
|     - |  892 | `			 * date under format). */` |
|    71 |  893 | `			ph7_result_string_format(pCtx,"%qd",(sxi64)(` |
|    46 |  894 | `				(sxu64)DtDaysFromCivil(pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400u` |
|    69 |  895 | `				+ (sxu64)((sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60` |
|    46 |  896 | `				          + (sxi64)pTm->tm_sec - (sxi64)pTm->tm_gmtoff)));` |
|    48 |  897 | `			break;` |
|    48 |  898 | `		case 'O':{` |
|     - |  899 | `			/* Difference to GMT without colon: +0530 (php) */` |
|    97 |  900 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   145 |  901 | `			ph7_result_string_format(pCtx,"%c%02d%02d",` |
|    96 |  902 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    97 |  903 | `			break;` |
|     - |  904 | `				 }` |
|   625 |  905 | `		case 'P':{` |
|     - |  906 | `			/* Difference to GMT with colon: +05:30 (php) */` |
|  1253 |  907 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|  1878 |  908 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|  1250 |  909 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|  1253 |  910 | `			break;` |
|     - |  911 | `				 }` |
|    47 |  912 | `		case 'p':{` |
|     - |  913 | `			/* Like P, but "Z" for UTC (php 8.0+). Two rules PHL had wrong, both` |
|     - |  914 | `			 * visible only once a zone can carry SECONDS or be an abbreviation:` |
|     - |  915 | `			 * php decides on what P would have PRINTED rather than on the raw` |
|     - |  916 | `			 * offset, so "+00:00:59" prints Z and "-00:00:59" prints "-00:00";` |
|     - |  917 | `			 * and the GMT abbreviation is php's one exception -- it prints` |
|     - |  918 | `			 * "+00:00" where the UTC and Z spellings of the same instant print Z. */` |
|     - |  919 | `			long a;` |
|    94 |  920 | `			if( pTm->tm_gmtoff >= 0 && pTm->tm_gmtoff < 60` |
|    59 |  921 | `			 && !(pTm->tm_zone && SyStrncmp(pTm->tm_zone,"GMT",3) == 0` |
|    14 |  922 | `			      && pTm->tm_zone[3] == 0) ){` |
|    23 |  923 | `				ph7_result_string(pCtx,"Z",1);` |
|    23 |  924 | `				break;` |
|     - |  925 | `			}` |
|    73 |  926 | `			a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   109 |  927 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|    72 |  928 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    73 |  929 | `			break;` |
|     - |  930 | `				 }` |
|    52 |  931 | `		case 'Z':` |
|     - |  932 | `			/* Timezone offset in seconds, plain integer (php) */` |
|   106 |  933 | `			ph7_result_string_format(pCtx,"%d",(int)pTm->tm_gmtoff);` |
|   106 |  934 | `			break;` |
|    38 |  935 | `		case 'c':{` |
|     - |  936 | `			/* 	ISO 8601 date: 2004-02-12T15:19:21+00:00 (php) */` |
|    77 |  937 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     - |  938 | `			/* Same four-column zero pad as 'r' (php: "0050-…", "-001-…"). */` |
|   115 |  939 | `			ph7_result_string_format(pCtx,"%04qd-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",` |
|    38 |  940 | `				pTm->tm_year,` |
|    76 |  941 | `				pTm->tm_mon+1,` |
|    38 |  942 | `				pTm->tm_mday,` |
|    38 |  943 | `				pTm->tm_hour,` |
|    38 |  944 | `				pTm->tm_min,` |
|    38 |  945 | `				pTm->tm_sec,` |
|    76 |  946 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  947 | `				);` |
|    77 |  948 | `			break;` |
|     - |  949 | `				 }` |
|     4 |  950 | `		case '\\':` |
|     9 |  951 | `			zIn++;` |
|     - |  952 | `			/* Expand verbatim */` |
|     9 |  953 | `			if( zIn < zEnd ){` |
|     9 |  954 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     4 |  955 | `			}` |
|     9 |  956 | `			break;` |
|  7899 |  957 | `		default:` |
|     - |  958 | `			/* Unknown format specifer,expand verbatim */` |
| 15802 |  959 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
| 15798 |  960 | `			break;` |
|     - |  961 | `		}` |
|     - |  962 | `		/* Point to the next character */` |
| 36186 |  963 | `		zIn++;` |
|     4 |  964 | `	}` |
|  4584 |  965 | `	return SXRET_OK;` |
|     4 |  966 | `}` |
|     - |  967 | `/*` |
|     - |  968 | ` * Resolve a date()/gmdate() $timestamp argument under php 8's ?int weak ZPP:` |
|     - |  969 | ` *   - null            -> *pbUseNow = 1 (caller uses the current time)` |
|     - |  970 | ` *   - int/bool/float  -> coerce to a Unix timestamp (float truncates; php's` |
|     - |  971 | ` *                        float->int precision E_DEPRECATED is not emitted)` |
|     - |  972 | ` *   - numeric string  -> coerce via php's is_numeric_string grammar` |
|     - |  973 | ` *                        (RangeStrToNumber: " 100 "/"1e3"/".5"/"+5" ok)` |
|     - |  974 | ` *   - anything else (non-numeric string, array, object, resource)` |
|     - |  975 | ` *                     -> catchable TypeError, byte-exact with php.` |
|     - |  976 | ` * Returns PH7_OK with *pbUseNow / *pT set, or the PH7_VmThrowException status.` |
|     - |  977 | ` */` |
|   554 |  978 | `static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)` |
|     2 |  979 | `{` |
|     - |  980 | `	char zBuf[64];` |
|   556 |  981 | `	*pbUseNow = 0;` |
|   556 |  982 | `	if( ph7_value_is_null(pArg) ){` |
|     3 |  983 | `		*pbUseNow = 1;` |
|     3 |  984 | `		return PH7_OK;` |
|     - |  985 | `	}` |
|   554 |  986 | `	if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_float(pArg) ){` |
|   546 |  987 | `		*pT = (time_t)ph7_value_to_int64(pArg);` |
|   546 |  988 | `		return PH7_OK;` |
|     - |  989 | `	}` |
|     9 |  990 | `	if( ph7_value_is_string(pArg) ){` |
|     - |  991 | `		int nStr;` |
|     9 |  992 | `		const char *zStr = ph7_value_to_string(pArg,&nStr);` |
|     - |  993 | `		sxi64 iLong; double dReal;` |
|     9 |  994 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|     9 |  995 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|     3 |  996 | `			*pT = (time_t)dReal;` |
|     6 |  997 | `			return PH7_OK;` |
|     - |  998 | `		}` |
|     7 |  999 | `		if( iKind == RANGE_IN_LONG ){` |
|     7 | 1000 | `			*pT = (time_t)iLong;` |
|     7 | 1001 | `			return PH7_OK;` |
|     - | 1002 | `		}` |
|     - | 1003 | `		/* Not a numeric string: fall through to the TypeError. */` |
|   ! 0 | 1004 | `	}` |
|   ! 0 | 1005 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1006 | `		"%s(): Argument #2 ($timestamp) must be of type ?int, %s given",` |
|   ! 0 | 1007 | `		ph7_function_name(pCtx),VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|   279 | 1008 | `}` |
|     - | 1009 | `/*` |
|     - | 1010 | ` * string date(string $format [, int $timestamp = time() ] )` |
|     - | 1011 | ` *  Returns a string formatted according to the given format string using` |
|     - | 1012 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|     - | 1013 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|     - | 1014 | ` * Parameters` |
|     - | 1015 | ` *  $format` |
|     - | 1016 | ` *   The format of the outputted date string (See code above)` |
|     - | 1017 | ` * $timestamp` |
|     - | 1018 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1019 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1020 | ` *   In other words, it defaults to the value of time().` |
|     - | 1021 | ` * Return` |
|     - | 1022 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - | 1023 | ` */` |
|   454 | 1024 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1025 | `{` |
|     - | 1026 | `	const char *zFormat;` |
|     - | 1027 | `	int nLen;` |
|     - | 1028 | `	Sytm sTm;` |
|   456 | 1029 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1030 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1031 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1032 | `		return PH7_OK;` |
|     - | 1033 | `	}` |
|   456 | 1034 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   456 | 1035 | `	if( nLen < 1 ){` |
|     - | 1036 | `		/* Don't bother processing return the empty string */` |
|   ! 0 | 1037 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1038 | `	}` |
|   456 | 1039 | `	if( nArg < 2 ){` |
|     - | 1040 | `		time_t t;` |
|    35 | 1041 | `		time(&t);` |
|    35 | 1042 | `		DtSytmOfTimestamp(pCtx->pVm,(sxi64)t,&sTm);` |
|    18 | 1043 | `	}else{` |
|     - | 1044 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|   422 | 1045 | `		time_t t = 0;` |
|     - | 1046 | `		int bUseNow;` |
|   422 | 1047 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|   422 | 1048 | `		if( rc != PH7_OK ){` |
|   ! 0 | 1049 | `			return rc;` |
|     - | 1050 | `		}` |
|   422 | 1051 | `		if( bUseNow ){` |
|   ! 0 | 1052 | `			time(&t);` |
|   ! 0 | 1053 | `		}` |
|   422 | 1054 | `		DtSytmOfTimestamp(pCtx->pVm,(sxi64)t,&sTm);` |
|     - | 1055 | `	}` |
|     - | 1056 | `	/* Format the given string */` |
|   456 | 1057 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|   456 | 1058 | `	return PH7_OK;` |
|   229 | 1059 | `}` |
|     - | 1060 | `/*` |
|     - | 1061 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|     - | 1062 | ` *  Identical to the date() function except that the time returned` |
|     - | 1063 | ` *  is Greenwich Mean Time (GMT).` |
|     - | 1064 | ` * Parameters` |
|     - | 1065 | ` *  $format` |
|     - | 1066 | ` *  The format of the outputted date string (See code above)` |
|     - | 1067 | ` *  $timestamp` |
|     - | 1068 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1069 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1070 | ` *   In other words, it defaults to the value of time().` |
|     - | 1071 | ` * Return` |
|     - | 1072 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - | 1073 | ` */` |
|   148 | 1074 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1075 | `{` |
|     - | 1076 | `	const char *zFormat;` |
|     - | 1077 | `	int nLen;` |
|     - | 1078 | `	Sytm sTm;` |
|   150 | 1079 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1080 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1081 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1082 | `		return PH7_OK;` |
|     - | 1083 | `	}` |
|   150 | 1084 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   150 | 1085 | `	if( nLen < 1 ){` |
|     - | 1086 | `		/* Don't bother processing return the empty string */` |
|   ! 0 | 1087 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1088 | `	}` |
|   150 | 1089 | `	if( nArg < 2 ){` |
|     - | 1090 | `		time_t t;` |
|    15 | 1091 | `		time(&t);` |
|    15 | 1092 | `		DtSytmOfTimestampUtc((sxi64)t,&sTm);` |
|     8 | 1093 | `	}else{` |
|     - | 1094 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|   136 | 1095 | `		time_t t = 0;` |
|     - | 1096 | `		int bUseNow;` |
|   136 | 1097 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|   136 | 1098 | `		if( rc != PH7_OK ){` |
|   ! 0 | 1099 | `			return rc;` |
|     - | 1100 | `		}` |
|   136 | 1101 | `		if( bUseNow ){` |
|     3 | 1102 | `			time(&t);` |
|     1 | 1103 | `		}` |
|   136 | 1104 | `		DtSytmOfTimestampUtc((sxi64)t,&sTm);` |
|     - | 1105 | `	}` |
|     - | 1106 | `	/* Format the given string */` |
|   150 | 1107 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|   150 | 1108 | `	return PH7_OK;` |
|    76 | 1109 | `}` |
|     - | 1110 | `/*` |
|     - | 1111 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|     - | 1112 | ` *  Return the local time.` |
|     - | 1113 | ` * Parameter` |
|     - | 1114 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1115 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - | 1116 | ` *     In other words, it defaults to the value of time().` |
|     - | 1117 | ` * $is_associative` |
|     - | 1118 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|     - | 1119 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|     - | 1120 | ` *   array containing all the different elements of the structure returned by the C function` |
|     - | 1121 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|     - | 1122 | ` *      "tm_sec" - seconds, 0 to 59` |
|     - | 1123 | ` *      "tm_min" - minutes, 0 to 59` |
|     - | 1124 | ` *      "tm_hour" - hours, 0 to 23` |
|     - | 1125 | ` *      "tm_mday" - day of the month, 1 to 31` |
|     - | 1126 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|     - | 1127 | ` *      "tm_year" - years since 1900` |
|     - | 1128 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|     - | 1129 | ` *      "tm_yday" - day of the year, 0 to 365` |
|     - | 1130 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|     - | 1131 | ` * Returns` |
|     - | 1132 | ` *  An associative array of information related to the timestamp.` |
|     - | 1133 | ` */` |
|    56 | 1134 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1135 | `{` |
|     - | 1136 | `	ph7_value *pValue,*pArray;` |
|    58 | 1137 | `	int isAssoc = 0;` |
|     - | 1138 | `	Sytm sTm;` |
|    58 | 1139 | `	if( nArg < 1 ){` |
|     - | 1140 | `		time_t t;` |
|     5 | 1141 | `		time(&t);` |
|     5 | 1142 | `		DtSytmOfTimestamp(pCtx->pVm,(sxi64)t,&sTm);` |
|     3 | 1143 | `	}else{` |
|     - | 1144 | `		/* Use the given timestamp */` |
|     - | 1145 | `		time_t t;` |
|    54 | 1146 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    54 | 1147 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|    28 | 1148 | `		}else{` |
|   ! 0 | 1149 | `			time(&t);` |
|     - | 1150 | `		}` |
|    54 | 1151 | `		DtSytmOfTimestamp(pCtx->pVm,(sxi64)t,&sTm);` |
|     - | 1152 | `	}` |
|     - | 1153 | `	/* Element value */` |
|    58 | 1154 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    58 | 1155 | `	if( pValue == 0 ){` |
|     - | 1156 | `		/* Return NULL */` |
|   ! 0 | 1157 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1158 | `		return PH7_OK;` |
|     - | 1159 | `	}` |
|     - | 1160 | `	/* Create a new array */` |
|    58 | 1161 | `	pArray = ph7_context_new_array(pCtx);` |
|    58 | 1162 | `	if( pArray == 0 ){` |
|     - | 1163 | `		/* Return NULL */` |
|   ! 0 | 1164 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1165 | `		return PH7_OK;` |
|     - | 1166 | `	}` |
|    58 | 1167 | `	if( nArg > 1 ){` |
|    52 | 1168 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|    25 | 1169 | `	}` |
|     - | 1170 | `	/* Fill the array */` |
|     - | 1171 | `	/* Seconds */` |
|    58 | 1172 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    58 | 1173 | `	if( isAssoc ){` |
|    52 | 1174 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|    27 | 1175 | `	}else{` |
|     7 | 1176 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1177 | `	}` |
|     - | 1178 | `	/* Minutes */` |
|    58 | 1179 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    58 | 1180 | `	if( isAssoc ){` |
|    52 | 1181 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|    27 | 1182 | `	}else{` |
|     7 | 1183 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1184 | `	}` |
|     - | 1185 | `	/* Hours */` |
|    58 | 1186 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    58 | 1187 | `	if( isAssoc ){` |
|    52 | 1188 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|    27 | 1189 | `	}else{` |
|     7 | 1190 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1191 | `	}` |
|     - | 1192 | `	/* mday */` |
|    58 | 1193 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    58 | 1194 | `	if( isAssoc ){` |
|    52 | 1195 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|    27 | 1196 | `	}else{` |
|     7 | 1197 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1198 | `	}` |
|     - | 1199 | `	/* mon */` |
|    58 | 1200 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|    58 | 1201 | `	if( isAssoc ){` |
|    52 | 1202 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|    27 | 1203 | `	}else{` |
|     7 | 1204 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1205 | `	}` |
|     - | 1206 | `	/* year since 1900 */` |
|    58 | 1207 | `	ph7_value_int64(pValue,sTm.tm_year-1900);` |
|    58 | 1208 | `	if( isAssoc ){` |
|    52 | 1209 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|    27 | 1210 | `	}else{` |
|     7 | 1211 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1212 | `	}` |
|     - | 1213 | `	/* wday */` |
|    58 | 1214 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    58 | 1215 | `	if( isAssoc ){` |
|    52 | 1216 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|    27 | 1217 | `	}else{` |
|     7 | 1218 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1219 | `	}` |
|     - | 1220 | `	/* yday */` |
|    58 | 1221 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    58 | 1222 | `	if( isAssoc ){` |
|    52 | 1223 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|    27 | 1224 | `	}else{` |
|     7 | 1225 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1226 | `	}` |
|     - | 1227 | `	/* isdst -- the broken-down time's own, which DtSytmOfTimestamp() answered` |
|     - | 1228 | ``	 * from the zone. A Windows-only `_get_daylight()` override used to stand`` |
|     - | 1229 | `	 * here and it asked the wrong question: whether the HOST's timezone` |
|     - | 1230 | `	 * observes daylight saving AT ALL, not whether this date is in it. Its twin` |
|     - | 1231 | ``	 * in DateFormat's `I` was removed when that was found; this one and idate's`` |
|     - | 1232 | `	 * survived, unseen because with no database no date was ever on daylight` |
|     - | 1233 | `	 * time and the two answers only differ when one is. */` |
|    58 | 1234 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|    58 | 1235 | `	if( isAssoc ){` |
|    52 | 1236 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|    27 | 1237 | `	}else{` |
|     7 | 1238 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1239 | `	}` |
|     - | 1240 | `	/* Return the array */` |
|    58 | 1241 | `	ph7_result_value(pCtx,pArray);` |
|    58 | 1242 | `	return PH7_OK;` |
|    30 | 1243 | `}` |
|     - | 1244 | `/*` |
|     - | 1245 | ` * int idate(string $format [, int $timestamp = time() ])` |
|     - | 1246 | ` *  Returns a number formatted according to the given format string` |
|     - | 1247 | ` *  using the given integer timestamp or the current local time if` |
|     - | 1248 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|     - | 1249 | ` *  to the value of time().` |
|     - | 1250 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|     - | 1251 | ` *  parameter.` |
|     - | 1252 | ` * $Parameters` |
|     - | 1253 | ` *  Supported format` |
|     - | 1254 | ` *   d 	Day of the month` |
|     - | 1255 | ` *   h 	Hour (12 hour format)` |
|     - | 1256 | ` *   H 	Hour (24 hour format)` |
|     - | 1257 | ` *   i 	Minutes` |
|     - | 1258 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|     - | 1259 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|     - | 1260 | ` *   m 	Month number` |
|     - | 1261 | ` *   s 	Seconds` |
|     - | 1262 | ` *   t 	Days in current month` |
|     - | 1263 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|     - | 1264 | ` *   w 	Day of the week (0 on Sunday)` |
|     - | 1265 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|     - | 1266 | ` *   y 	Year (1 or 2 digits - check note below)` |
|     - | 1267 | ` *   Y 	Year (4 digits)` |
|     - | 1268 | ` *   z 	Day of the year` |
|     - | 1269 | ` *   Z 	Timezone offset in seconds` |
|     - | 1270 | ` * $timestamp` |
|     - | 1271 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|     - | 1272 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|     - | 1273 | ` *  to the value of time().` |
|     - | 1274 | ` * Return` |
|     - | 1275 | ` *  An integer.` |
|     - | 1276 | ` */` |
|   276 | 1277 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1278 | `{` |
|     - | 1279 | `	const char *zFormat;` |
|   279 | 1280 | `	ph7_int64 iVal = 0;` |
|     - | 1281 | `	int nLen;` |
|     - | 1282 | `	Sytm sTm;` |
|   279 | 1283 | `	time_t t = 0; /* The resolved timestamp; 'U' must report THIS, not time(0) */` |
|   279 | 1284 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1285 | `		/* Missing/Invalid argument,return -1 */` |
|   ! 0 | 1286 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1287 | `		return PH7_OK;` |
|     - | 1288 | `	}` |
|   279 | 1289 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   279 | 1290 | `	if( nLen < 1 ){` |
|     - | 1291 | `		/* Don't bother processing return -1*/` |
|   ! 0 | 1292 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1293 | `	}` |
|   279 | 1294 | `	if( nArg < 2 ){` |
|    16 | 1295 | `		time(&t);` |
|    16 | 1296 | `		DtSytmOfTimestamp(pCtx->pVm,(sxi64)t,&sTm);` |
|     9 | 1297 | `	}else{` |
|     - | 1298 | `		/* Use the given timestamp */` |
|   264 | 1299 | `		if( ph7_value_is_int(apArg[1]) ){` |
|   264 | 1300 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|   133 | 1301 | `		}else{` |
|   ! 0 | 1302 | `			time(&t);` |
|     - | 1303 | `		}` |
|   264 | 1304 | `		DtSytmOfTimestamp(pCtx->pVm,(sxi64)t,&sTm);` |
|     - | 1305 | `	}` |
|     - | 1306 | `	/* Perform the requested operation */` |
|   279 | 1307 | `	switch(zFormat[0]){` |
|     9 | 1308 | `	case 'd':` |
|     - | 1309 | `	case 'j':` |
|     - | 1310 | `		/* Day of the month ('j' differs from 'd' only in zero padding, which an` |
|     - | 1311 | `		 * integer result cannot carry) */` |
|    19 | 1312 | `		iVal = sTm.tm_mday;` |
|    19 | 1313 | `		break;` |
|     8 | 1314 | `	case 'h':` |
|     - | 1315 | `	case 'g':` |
|     - | 1316 | `		/* Hour (12 hour format): php reports midnight and noon as 12, not 0 —` |
|     - | 1317 | ``		 * `1 + hour % 12` answered 1 for both. */`` |
|    17 | 1318 | `		iVal = sTm.tm_hour % 12;` |
|    17 | 1319 | `		if( iVal == 0 ){` |
|    17 | 1320 | `			iVal = 12;` |
|     8 | 1321 | `		}` |
|    17 | 1322 | `		break;` |
|     9 | 1323 | `	case 'H':` |
|     - | 1324 | `	case 'G':` |
|     - | 1325 | `		/* Hour (24 hour format) */` |
|    19 | 1326 | `		iVal = sTm.tm_hour;` |
|    19 | 1327 | `		break;` |
|     5 | 1328 | `	case 'B': {` |
|     - | 1329 | `		/* Swatch Internet time: 1000 "beats" per day in UTC+1, no fractions.` |
|     - | 1330 | `		 * Integer math throughout so the tiny build (no floating point) agrees.` |
|     - | 1331 | ``		 * Read the time OF DAY off the broken-down clock: `t + 3600` overflows`` |
|     - | 1332 | `		 * at the top of php's timestamp range, which is undefined and answered` |
|     - | 1333 | `		 * the wrong beat. */` |
|    16 | 1334 | `		ph7_int64 iSec = ((ph7_int64)sTm.tm_hour*3600 + sTm.tm_min*60 + sTm.tm_sec` |
|    10 | 1335 | `			- sTm.tm_gmtoff + 3600) % 86400;` |
|    11 | 1336 | `		if( iSec < 0 ){` |
|   ! 0 | 1337 | `			iSec += 86400;` |
|   ! 0 | 1338 | `		}` |
|    11 | 1339 | `		iVal = iSec * 1000 / 86400;` |
|    11 | 1340 | `		break;` |
|     - | 1341 | `			  }` |
|     5 | 1342 | `	case 'i':` |
|     - | 1343 | `		/*Minutes*/` |
|    11 | 1344 | `		iVal = sTm.tm_min;` |
|    11 | 1345 | `		break;` |
|    20 | 1346 | `	case 'I':` |
|     - | 1347 | `		/* returns 1 if DST is activated, 0 otherwise -- for THIS date, off the` |
|     - | 1348 | `		 * broken-down time. See the note in localtime() above for the Windows` |
|     - | 1349 | `		 * override that used to stand here. */` |
|    41 | 1350 | `		iVal = sTm.tm_isdst;` |
|    41 | 1351 | `		break;` |
|     4 | 1352 | `	case 'L':` |
|     - | 1353 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|     9 | 1354 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|     9 | 1355 | `		break;` |
|     9 | 1356 | `	case 'm':` |
|     - | 1357 | `	case 'n':` |
|     - | 1358 | `		/* Month number. Sytm keeps tm_mon 0-based (see 't' below, which tests` |
|     - | 1359 | ``		 * `tm_mon == 1` for February), so July used to answer 6. */`` |
|    19 | 1360 | `		iVal = sTm.tm_mon + 1;` |
|    19 | 1361 | `		break;` |
|     5 | 1362 | `	case 's':` |
|     - | 1363 | `		/*Seconds*/` |
|    11 | 1364 | `		iVal = sTm.tm_sec;` |
|    11 | 1365 | `		break;` |
|     4 | 1366 | `	case 't':{` |
|     - | 1367 | `		/*Days in current month*/` |
|     - | 1368 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|     9 | 1369 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|     9 | 1370 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|   ! 0 | 1371 | `			nDays = 28;` |
|   ! 0 | 1372 | `		}` |
|     9 | 1373 | `		iVal = nDays;` |
|     9 | 1374 | `		break;` |
|     - | 1375 | `			 }` |
|     8 | 1376 | `	case 'U':` |
|     - | 1377 | `		/* Seconds since the Unix Epoch. This used to call time(0), ignoring the` |
|     - | 1378 | `		 * $timestamp argument entirely and always answering "now". */` |
|    17 | 1379 | `		iVal = (ph7_int64)t;` |
|    17 | 1380 | `		break;` |
|     4 | 1381 | `	case 'w':` |
|     - | 1382 | `		/*	Day of the week (0 on Sunday) */` |
|     9 | 1383 | `		iVal = sTm.tm_wday;` |
|     9 | 1384 | `		break;` |
|     8 | 1385 | `	case 'W':` |
|     - | 1386 | `	case 'o': {` |
|     - | 1387 | `		/* ISO-8601 week number / week-numbering year: both belong to the year` |
|     - | 1388 | `		 * owning the Thursday of the civil week, so 2021-01-01 is 2020-W53.` |
|     - | 1389 | `		 * The old code indexed a weekday table and returned a DAY number` |
|     - | 1390 | `		 * (1..7) as if it were a week number — idate("W") answered 4 in the` |
|     - | 1391 | `		 * middle of July. Same derivation as date()'s 'o'/'W' above. */` |
|    17 | 1392 | `		sxi64 days = DtDaysFromCivil(sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|    17 | 1393 | `		int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|    17 | 1394 | `		sxi64 thu = days + (4 - isoDow);` |
|     - | 1395 | `		sxi64 wy;` |
|     - | 1396 | `		int wm,wd;` |
|    17 | 1397 | `		DtCivilFromDays(thu,&wy,&wm,&wd);` |
|    17 | 1398 | `		if( zFormat[0] == 'o' ){` |
|     9 | 1399 | `			iVal = (ph7_int64)wy;` |
|     5 | 1400 | `		}else{` |
|     9 | 1401 | `			iVal = (ph7_int64)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1;` |
|     - | 1402 | `		}` |
|    17 | 1403 | `		break;` |
|     - | 1404 | `			  }` |
|     4 | 1405 | `	case 'y':` |
|     - | 1406 | `		/* Year (2 digits) */` |
|     9 | 1407 | `		iVal = sTm.tm_year % 100;` |
|     9 | 1408 | `		break;` |
|    10 | 1409 | `	case 'Y':` |
|     - | 1410 | `		/* Year (4 digits) */` |
|    21 | 1411 | `		iVal = sTm.tm_year;` |
|    21 | 1412 | `		break;` |
|     4 | 1413 | `	case 'z':` |
|     - | 1414 | `		/* Day of the year */` |
|     9 | 1415 | `		iVal = sTm.tm_yday;` |
|     9 | 1416 | `		break;` |
|    20 | 1417 | `	case 'Z':` |
|     - | 1418 | `		/*Timezone offset in seconds*/` |
|    41 | 1419 | `		iVal = sTm.tm_gmtoff;` |
|    41 | 1420 | `		break;` |
|     2 | 1421 | `	default:` |
|     - | 1422 | `		/* Unknown token: php's own -1, which the shared tail below reports. */` |
|     6 | 1423 | `		iVal = -1;` |
|     4 | 1424 | `		break;` |
|     - | 1425 | `	}` |
|     - | 1426 | ``	/* php's idate answers a C `int`, so every token is narrowed to one -- and`` |
|     - | 1427 | `	 * -1 is the single value it uses for "no answer": it warns about the TOKEN` |
|     - | 1428 | `	 * and answers FALSE, whatever produced the -1. That is why` |
|     - | 1429 | ``	 * `idate('U', PHP_INT_MAX)` is a false with an "Unrecognized date format`` |
|     - | 1430 | `` 	 * token" warning in front of it rather than the timestamp. `idate() === false` `` |
|     - | 1431 | `	 * is the documented check, and 0 is a legitimate answer for several tokens. */` |
|   279 | 1432 | `	if( (int)iVal == -1 ){` |
|     8 | 1433 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unrecognized date format token");` |
|     8 | 1434 | `		ph7_result_bool(pCtx,0);` |
|     5 | 1435 | `	}else{` |
|   272 | 1436 | `		ph7_result_int64(pCtx,(int)iVal);` |
|     - | 1437 | `	}` |
|   279 | 1438 | `	return PH7_OK;` |
|   141 | 1439 | `}` |
|     - | 1440 | `/*` |
|     - | 1441 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|     - | 1442 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|     - | 1443 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|     - | 1444 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|     - | 1445 | ` *  specified.` |
|     - | 1446 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|     - | 1447 | ` *  the current value according to the local date and time.` |
|     - | 1448 | ` * Parameters` |
|     - | 1449 | ` * $hour` |
|     - | 1450 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|     - | 1451 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|     - | 1452 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|     - | 1453 | ` * $minute` |
|     - | 1454 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|     - | 1455 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|     - | 1456 | ` *  in the following hour(s).` |
|     - | 1457 | ` * $second` |
|     - | 1458 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|     - | 1459 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|     - | 1460 | ` * second in the following minute(s).` |
|     - | 1461 | ` * $month` |
|     - | 1462 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|     - | 1463 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|     - | 1464 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|     - | 1465 | ` * $day` |
|     - | 1466 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|     - | 1467 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|     - | 1468 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|     - | 1469 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|     - | 1470 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|     - | 1471 | ` * $year` |
|     - | 1472 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|     - | 1473 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|     - | 1474 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|     - | 1475 | ` * $is_dst` |
|     - | 1476 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|     - | 1477 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|     - | 1478 | ` * Return` |
|     - | 1479 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|     - | 1480 | ` *   If the arguments are invalid, the function returns FALSE` |
|     - | 1481 | ` */` |
|  4606 | 1482 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1483 | `{` |
|     - | 1484 | `	const char *zFunction;` |
|     - | 1485 | `	ph7_int64 iVal;` |
|     - | 1486 | `	sxi64 h,mi,s,mo,d,y,yAdj;` |
|  4610 | 1487 | `	int moN,bLocal,iTzDef = -1,bDstNow = 0;` |
|  4610 | 1488 | `	sxi32 iOffDef = 0;` |
|     - | 1489 | `	struct tm *pTm;` |
|     - | 1490 | `	time_t t;` |
|     - | 1491 | `	/* Extract function name */` |
|  4610 | 1492 | `	zFunction = ph7_function_name(pCtx);` |
|     - | 1493 | `	/* PHP 8 dropped the legacy $is_dst 7th parameter: mktime()/gmmktime() now` |
|     - | 1494 | `	 * accept at most 6 arguments and throw a catchable ArgumentCountError` |
|     - | 1495 | `	 * otherwise (the central aBuiltinArity table only enforces the minimum, so` |
|     - | 1496 | `	 * this maximum is checked here). */` |
|  4610 | 1497 | `	if( nArg > 6 ){` |
|   ! 0 | 1498 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1499 | `			"%s() expects at most 6 arguments, %d given",zFunction,nArg);` |
|     - | 1500 | `	}` |
|  4610 | 1501 | `	if( nArg < 1 ){` |
|   ! 0 | 1502 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1503 | `			"%s() expects at least 1 argument, 0 given",zFunction);` |
|     - | 1504 | `	}` |
|     - | 1505 | `	/* Missing components default from the current time IN THE ZONE THIS DOOR` |
|     - | 1506 | `	 * SPEAKS: php's default zone for mktime(), UTC for gmmktime(). With a fixed` |
|     - | 1507 | `	 * default the two agree and both read the clock straight, which is why this` |
|     - | 1508 | `	 * read gmtime() for both while there was no database. */` |
|  4610 | 1509 | `	bLocal = SyStrncmp(zFunction,"gm",2) != 0;` |
|  4610 | 1510 | `	time(&t);` |
|  4610 | 1511 | `	if( bLocal ){` |
|    68 | 1512 | `		iTzDef = DtDefaultTzIndex(pCtx->pVm);` |
|    68 | 1513 | `		if( iTzDef >= 0 ){` |
|     - | 1514 | `			int nAbbr;` |
|     - | 1515 | `			const char *zAbbr;` |
|    17 | 1516 | `			iOffDef = DtTzOffsetOf(iTzDef,0,(sxi64)t,&bDstNow,&zAbbr,&nAbbr);` |
|     8 | 1517 | `		}` |
|    32 | 1518 | `	}` |
|  4610 | 1519 | `	t = (time_t)((sxi64)t + iOffDef);` |
|  4610 | 1520 | `	pTm = gmtime(&t);` |
|  4610 | 1521 | `	h  = pTm->tm_hour;` |
|  4610 | 1522 | `	mi = pTm->tm_min;` |
|  4610 | 1523 | `	s  = pTm->tm_sec;` |
|  4610 | 1524 | `	mo = pTm->tm_mon + 1;` |
|  4610 | 1525 | `	d  = pTm->tm_mday;` |
|  4610 | 1526 | `	y  = pTm->tm_year + 1900;` |
|  4610 | 1527 | `	h = ph7_value_to_int64(apArg[0]);` |
|  4610 | 1528 | `	if( nArg > 1 ){` |
|  4610 | 1529 | `		mi = ph7_value_to_int64(apArg[1]);` |
|  4610 | 1530 | `		if( nArg > 2 ){` |
|  4610 | 1531 | `			s = ph7_value_to_int64(apArg[2]);` |
|  4610 | 1532 | `			if( nArg > 3 ){` |
|  4610 | 1533 | `				mo = ph7_value_to_int64(apArg[3]);` |
|  4610 | 1534 | `				if( nArg > 4 ){` |
|  4610 | 1535 | `					d = ph7_value_to_int64(apArg[4]);` |
|  4610 | 1536 | `					if( nArg > 5 ){` |
|     - | 1537 | `						/* php's legacy two-digit mapping: 0-69 -> 2000-2069,` |
|     - | 1538 | `						 * 70-100 -> 1970-2000; anything else is verbatim */` |
|  4610 | 1539 | `						y = ph7_value_to_int64(apArg[5]);` |
|  4610 | 1540 | `						if( y >= 0 && y <= 69 ){` |
|     7 | 1541 | `							y += 2000;` |
|  4607 | 1542 | `						}else if( y >= 70 && y <= 100 ){` |
|     5 | 1543 | `							y += 1900;` |
|     2 | 1544 | `						}` |
|  2303 | 1545 | `					}` |
|  2303 | 1546 | `				}` |
|  2303 | 1547 | `			}` |
|  2303 | 1548 | `		}` |
|  2303 | 1549 | `	}` |
|     - | 1550 | `	/* Normalize the month with floor semantics, then let day/time components` |
|     - | 1551 | `	 * overflow linearly (php: mktime(25,-30,0,1,1,2024) == Jan 2 00:30). */` |
|  4610 | 1552 | `	yAdj = y + DtFloorDiv(mo - 1,12);` |
|  4610 | 1553 | `	moN  = (int)(mo - 1 - DtFloorDiv(mo - 1,12) * 12) + 1;` |
|  4610 | 1554 | `	iVal = (DtDaysFromCivil(yAdj,moN,1) + (d - 1)) * 86400 + h*3600 + mi*60 + s;` |
|     - | 1555 | `	/* What the fields spell is a WALL CLOCK, and mktime() is the door that turns` |
|     - | 1556 | `	 * one into an instant -- so it owes the zone the same search a constructor` |
|     - | 1557 | `	 * does, ambiguous and skipped hours included. gmmktime() spells UTC and owes` |
|     - | 1558 | `	 * nothing. */` |
|     - | 1559 | `#ifdef PH7_ENABLE_TZDB` |
|  4610 | 1560 | `	if( bLocal && iTzDef >= 0 ){` |
|    17 | 1561 | `		sxi64 iFixed = iVal;` |
|    17 | 1562 | `		sxi32 iOffAt = iOffDef;` |
|     - | 1563 | `		/* bDstNow is php's seed: mktime() builds its struct from the CURRENT` |
|     - | 1564 | `		 * moment and that moment's daylight flag decides an ambiguous hour.` |
|     - | 1565 | `		 * See PH7_TzLocalToUtcDst(). */` |
|    17 | 1566 | `		if( PH7_TzLocalToUtcSeed(iTzDef,iVal,iOffDef,bDstNow,&iFixed,&iOffAt) ){` |
|    17 | 1567 | `			iVal = iFixed;` |
|     8 | 1568 | `		}` |
|     8 | 1569 | `	}` |
|     - | 1570 | `#endif` |
|     - | 1571 | `	/* Return the timestamp as a 64bit integer */` |
|  4610 | 1572 | `	ph7_result_int64(pCtx,iVal);` |
|  4610 | 1573 | `	return PH7_OK;` |
|  2307 | 1574 | `}` |
|     - | 1575 | `/*` |
|     - | 1576 | ` * string date_default_timezone_get(void)` |
|     - | 1577 | ` *  Gets the default timezone used by all date/time functions in a script.` |
|     - | 1578 | ` */` |
|    54 | 1579 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1580 | `{` |
|    57 | 1581 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 | 1582 | `	SXUNUSED(nArg);` |
|    27 | 1583 | `	SXUNUSED(apArg);` |
|    57 | 1584 | `	ph7_result_string(pCtx,pVm->zDefTz,(int)pVm->nDefTz);` |
|    57 | 1585 | `	return PH7_OK;` |
|     3 | 1586 | `}` |
|     - | 1587 | `/*` |
|     - | 1588 | ` * bool date_default_timezone_set(string $timezoneId)` |
|     - | 1589 | ` *  Sets the default timezone used by all date/time functions in a script.` |
|     - | 1590 | ` *  php validates against the tz database and stores the id verbatim (get()` |
|     - | 1591 | ` *  echoes back "utc" if that's what was set). With PH7_ENABLE_TZDB off there is` |
|     - | 1592 | ` *  no database to validate against and only UTC and GMT are accepted; every` |
|     - | 1593 | ` *  other id is then rejected with php's invalid-id notice.` |
|     - | 1594 | ` */` |
|   214 | 1595 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1596 | `{` |
|   218 | 1597 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1598 | `	const char *zId;` |
|     - | 1599 | `	int nId;` |
|   218 | 1600 | `	if( nArg < 1 ){` |
|   ! 0 | 1601 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1602 | `		return PH7_OK;` |
|     - | 1603 | `	}` |
|   218 | 1604 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|     - | 1605 | `#ifdef PH7_ENABLE_TZDB` |
|     - | 1606 | `	/* An IDENTIFIER and nothing else. This door does not share the zone grammar` |
|     - | 1607 | `	 * DateTimeZone's constructor has: it skips no leading blank, reads no` |
|     - | 1608 | `` 	 * `±HH:MM` and consults no abbreviation table, so `+05:00`, ` Europe/Paris` `` |
|     - | 1609 | ``	 * and `CEST` are all "invalid" here while `new DateTimeZone` takes all`` |
|     - | 1610 | `	 * three. The ten names that are BOTH an abbreviation and a zone file are` |
|     - | 1611 | ``	 * where that shows: a `CET` default is the FILE, and switches to CEST every`` |
|     - | 1612 | ``	 * summer, where `new DateTimeZone('CET')` is the fixed +01:00 abbreviation.`` |
|     - | 1613 | `	 *` |
|     - | 1614 | `	 * The lookup folds case and the STORED name is the caller's own bytes, the` |
|     - | 1615 | `	 * same rule an identifier has everywhere else -- get() echoes back` |
|     - | 1616 | ``	 * `europe/paris`. */`` |
|   218 | 1617 | `	if( nId > 0 && (sxu32)nId < sizeof(pVm->zDefTz) && PH7_TzFind(zId,nId) >= 0 ){` |
|   202 | 1618 | `		SyMemcpy(zId,pVm->zDefTz,(sxu32)nId);` |
|   202 | 1619 | `		pVm->zDefTz[nId] = 0;` |
|   202 | 1620 | `		pVm->nDefTz = (sxu32)nId;` |
|   202 | 1621 | `		pVm->bDefTzExplicit = 1;` |
|   202 | 1622 | `		ph7_result_bool(pCtx,1);` |
|   202 | 1623 | `		return PH7_OK;` |
|     - | 1624 | `	}` |
|     - | 1625 | `#endif` |
|    18 | 1626 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|   ! 0 | 1627 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|   ! 0 | 1628 | `		pVm->zDefTz[3] = 0;` |
|   ! 0 | 1629 | `		pVm->nDefTz = 3;` |
|   ! 0 | 1630 | `		pVm->bDefTzExplicit = 1;` |
|   ! 0 | 1631 | `		ph7_result_bool(pCtx,1);` |
|   ! 0 | 1632 | `		return PH7_OK;` |
|     - | 1633 | `	}` |
|     - | 1634 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|     - | 1635 | `	 * — exactly php's notice shape here */` |
|    18 | 1636 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|    18 | 1637 | `	ph7_result_bool(pCtx,0);` |
|    18 | 1638 | `	return PH7_OK;` |
|   111 | 1639 | `}` |
|     - | 1640 |  |
|     - | 1641 | `/*` |
|     - | 1642 | ` * The sun trio -- date_sun_info(), date_sunrise() and date_sunset().` |
|     - | 1643 | ` *` |
|     - | 1644 | `` * One solar model answers all three. php's is Paul Schlyter's `sunriset.c`,`` |
|     - | 1645 | `` * carried into timelib and reached through `timelib_astro_rise_set_altitude()`;`` |
|     - | 1646 | ` * the arithmetic below is that model, and every constant in it was FITTED` |
|     - | 1647 | ` * against /usr/bin/php rather than recalled -- 1386 oracle cases over eleven` |
|     - | 1648 | ` * points, six default zones and three eras agree to the second.` |
|     - | 1649 | ` *` |
|     - | 1650 | ` * Two things about it are not obvious and were both measured:` |
|     - | 1651 | ` *` |
|     - | 1652 | ` *   - The day is the LOCAL calendar date, but the clock the answers are` |
|     - | 1653 | ` *     expressed on is UTC. The base instant is midnight UTC of the day the` |
|     - | 1654 | ` *     script's default zone is on at $timestamp, so the same timestamp asked in` |
|     - | 1655 | ` *     Amsterdam and in Los Angeles is answered for DIFFERENT days -- that is` |
|     - | 1656 | ` *     php, not a rounding artefact.` |
|     - | 1657 | ` *   - The epoch is "2000 Jan 0.0" (JD 2451543.5) and the moment evaluated is` |
|     - | 1658 | `` *     NOON, hence the `+ 0.5`. Using J2000 proper puts every answer ~30 s out,`` |
|     - | 1659 | ` *     which is exactly how far the first four fits missed.` |
|     - | 1660 | ` *` |
|     - | 1661 | ` * date_sun_info() and date_sunrise() do NOT agree, and are not supposed to:` |
|     - | 1662 | ` * both ask for the sun's upper limb, but sun_info asks at -35/60 degrees while` |
|     - | 1663 | ` * the pair asks at 90 - $zenith, which the stock directive puts at -50/60. At` |
|     - | 1664 | ` * Amsterdam in June those 15 arcminutes are 133 seconds.` |
|     - | 1665 | ` */` |
|     - | 1666 | `/* Finite in the IEEE-754 sense, without pulling in a C99 isfinite() the tiny` |
|     - | 1667 | ` * builds would have to answer for: an exponent field of all ones is NaN or an` |
|     - | 1668 | ` * infinity, and nothing else is. */` |
|   496 | 1669 | `static int DtSunIsFinite(double d)` |
|     2 | 1670 | `{` |
|     - | 1671 | `	union { double d; sxu64 u; } v;` |
|   498 | 1672 | `	v.d = d;` |
|   498 | 1673 | `	return ((v.u >> 52) & 0x7FF) != 0x7FF;` |
|     2 | 1674 | `}` |
|     - | 1675 | `#define DT_SUN_DEGRAD  (PH7_PI / 180.0)` |
|     - | 1676 | `#define DT_SUN_RADEG   (180.0 / PH7_PI)` |
|     - | 1677 | `/* The two altitudes the trio asks at, in degrees above the horizon. The` |
|     - | 1678 | ` * twilights are php's own -6/-12/-18 and are asked of the sun's CENTRE. */` |
|     - | 1679 | `#define DT_SUN_RISESET (-35.0 / 60.0)` |
|     - | 1680 |  |
|   558 | 1681 | `static double DtSunRev360(double r)` |
|     2 | 1682 | `{` |
|   560 | 1683 | `	return r - 360.0 * floor(r / 360.0);` |
|     2 | 1684 | `}` |
|     - | 1685 | `/* Fold into [-180,180), which is what the hour angle south of the meridian` |
|     - | 1686 | `` * wants; `rev360` would put a pre-noon transit a whole day out. */`` |
|   186 | 1687 | `static double DtSunRev180(double r)` |
|     2 | 1688 | `{` |
|   188 | 1689 | `	return r - 360.0 * floor(r / 360.0 + 0.5);` |
|     2 | 1690 | `}` |
|     - | 1691 | `/*` |
|     - | 1692 | ``  * The sun's ecliptic longitude and its distance in astronomical units, at `d` `` |
|     - | 1693 | ` * days from 2000 Jan 0.0. The mean anomaly is folded before the eccentric` |
|     - | 1694 | ` * anomaly is taken from it: left unfolded it grows without bound and the` |
|     - | 1695 | ` * one-term Kepler correction loses its meaning for dates far from the epoch.` |
|     - | 1696 | ` */` |
|   186 | 1697 | `static void DtSunPos(double d,double *prLon,double *prDist)` |
|     2 | 1698 | `{` |
|     - | 1699 | `	double M,w,e,E,x,y,v;` |
|   188 | 1700 | `	M = DtSunRev360(356.0470 + 0.9856002585 * d);` |
|   188 | 1701 | `	w = 282.9404 + 4.70935e-5 * d;` |
|   188 | 1702 | `	e = 0.016709 - 1.151e-9 * d;` |
|   188 | 1703 | `	E = M + e * DT_SUN_RADEG * sin(M * DT_SUN_DEGRAD) * (1.0 + e * cos(M * DT_SUN_DEGRAD));` |
|   188 | 1704 | `	x = cos(E * DT_SUN_DEGRAD) - e;` |
|   188 | 1705 | `	y = sqrt(1.0 - e * e) * sin(E * DT_SUN_DEGRAD);` |
|   188 | 1706 | `	*prDist = sqrt(x * x + y * y);` |
|   188 | 1707 | `	v = atan2(y,x) * DT_SUN_RADEG;` |
|   188 | 1708 | `	*prLon = v + w;` |
|   188 | 1709 | `	if( *prLon > 360.0 ){` |
|   170 | 1710 | `		*prLon -= 360.0;` |
|    84 | 1711 | `	}` |
|   188 | 1712 | `}` |
|     - | 1713 | `/* The same position as a right ascension and a declination. */` |
|   186 | 1714 | `static void DtSunRaDec(double d,double *prRA,double *prDec,double *prDist)` |
|     2 | 1715 | `{` |
|     - | 1716 | `	double rLon,rObl,x,y,z;` |
|   188 | 1717 | `	DtSunPos(d,&rLon,prDist);` |
|   188 | 1718 | `	x = *prDist * cos(rLon * DT_SUN_DEGRAD);` |
|   188 | 1719 | `	y = *prDist * sin(rLon * DT_SUN_DEGRAD);` |
|   188 | 1720 | `	rObl = 23.4393 - 3.563e-7 * d;` |
|   188 | 1721 | `	z = y * sin(rObl * DT_SUN_DEGRAD);` |
|   188 | 1722 | `	y = y * cos(rObl * DT_SUN_DEGRAD);` |
|   188 | 1723 | `	*prRA  = atan2(y,x) * DT_SUN_RADEG;` |
|   188 | 1724 | `	*prDec = atan2(z,sqrt(x * x + y * y)) * DT_SUN_RADEG;` |
|   188 | 1725 | `}` |
|     - | 1726 | `/* Greenwich mean sidereal time at 0h, degrees. */` |
|   186 | 1727 | `static double DtSunGmst0(double d)` |
|     2 | 1728 | `{` |
|   281 | 1729 | `	return DtSunRev360((180.0 + 356.0470 + 282.9404)` |
|   186 | 1730 | `		+ (0.9856002585 + 4.70935e-5) * d);` |
|     2 | 1731 | `}` |
|     - | 1732 | `/*` |
|     - | 1733 | `` * The diurnal arc at one altitude. `iBase` is midnight UTC of the local day;`` |
|     - | 1734 | ` * the three results come back as HOURS on that day's UTC clock and may fall` |
|     - | 1735 | ` * outside [0,24) -- a rise east of the date line legitimately lands before its` |
|     - | 1736 | ` * own base, and php keeps the timestamp that says so.` |
|     - | 1737 | ` *` |
|     - | 1738 | ` * Returns -1 when the sun never reaches the altitude that day, +1 when it never` |
|     - | 1739 | ` * drops below it, and 0 when it does both. Callers must branch on that rather` |
|     - | 1740 | ` * than on the hours: at rc != 0 the hours are the placeholders 0 and 12, which` |
|     - | 1741 | ` * are perfectly ordinary times.` |
|     - | 1742 | ` */` |
|   186 | 1743 | `static int DtSunRiseSet(sxi64 iBase,double rLon,double rLat,double rAltit,` |
|     - | 1744 | `	int bUpperLimb,double *prRise,double *prSet,double *prTransit)` |
|     2 | 1745 | `{` |
|     - | 1746 | `	double d,rSid,rRA,rDec,rDist,rSouth,rRadius,rCos,t;` |
|   188 | 1747 | `	int rc = 0;` |
|     - | 1748 | `	/* Days from 2000 Jan 0.0 to NOON of the local day, corrected to the` |
|     - | 1749 | `	 * meridian: 2440587.5 is the Julian day of the Unix epoch and 2451543.5` |
|     - | 1750 | `	 * the epoch this model counts from. */` |
|   188 | 1751 | `	d = (double)iBase / 86400.0 + (2440587.5 - 2451543.5) + 0.5 - rLon / 360.0;` |
|   188 | 1752 | `	rSid = DtSunRev360(DtSunGmst0(d) + 180.0 + rLon);` |
|   188 | 1753 | `	DtSunRaDec(d,&rRA,&rDec,&rDist);` |
|   188 | 1754 | `	rSouth = 12.0 - DtSunRev180(rSid - rRA) / 15.0;` |
|     - | 1755 | `	/* The sun's apparent radius shrinks as the earth moves away from it, so it` |
|     - | 1756 | `	 * is taken per day and not as a constant. */` |
|   188 | 1757 | `	rRadius = 0.2666 / rDist;` |
|   188 | 1758 | `	if( bUpperLimb ){` |
|   128 | 1759 | `		rAltit -= rRadius;` |
|    63 | 1760 | `	}` |
|   281 | 1761 | `	rCos = (sin(DT_SUN_DEGRAD * rAltit)` |
|   186 | 1762 | `	      - sin(DT_SUN_DEGRAD * rLat) * sin(DT_SUN_DEGRAD * rDec))` |
|   186 | 1763 | `	     / (cos(DT_SUN_DEGRAD * rLat) * cos(DT_SUN_DEGRAD * rDec));` |
|   188 | 1764 | `	if( rCos >= 1.0 ){` |
|    16 | 1765 | `		rc = -1;` |
|    16 | 1766 | `		t = 0.0;` |
|   181 | 1767 | `	}else if( rCos <= -1.0 ){` |
|    30 | 1768 | `		rc = 1;` |
|    30 | 1769 | `		t = 12.0;` |
|    16 | 1770 | `	}else{` |
|   146 | 1771 | `		t = (acos(rCos) * DT_SUN_RADEG) / 15.0;` |
|     - | 1772 | `	}` |
|   188 | 1773 | `	*prRise = rSouth - t;` |
|   188 | 1774 | `	*prSet  = rSouth + t;` |
|   188 | 1775 | `	*prTransit = rSouth;` |
|   188 | 1776 | `	return rc;` |
|     2 | 1777 | `}` |
|     - | 1778 | `/*` |
|     - | 1779 | `` * Midnight UTC of the day the script's default zone is on at `iTs`. Every`` |
|     - | 1780 | ` * answer the trio gives is measured from here, so a zone that moves the local` |
|     - | 1781 | ` * date moves all nine fields by a day -- see the note at the top.` |
|     - | 1782 | ` */` |
|   126 | 1783 | `static sxi64 DtSunLocalMidnight(ph7_vm *pVm,sxi64 iTs,sxi32 *piOff)` |
|     2 | 1784 | `{` |
|   128 | 1785 | `	int iTz = DtDefaultTzIndex(pVm);` |
|   128 | 1786 | `	int bDst = 0,nAbbr = 0;` |
|   128 | 1787 | `	const char *zAbbr = 0;` |
|     - | 1788 | `	sxi64 iLocal;` |
|   128 | 1789 | `	sxi32 iOff = DtTzOffsetOf(iTz,0,iTs,&bDst,&zAbbr,&nAbbr);` |
|   128 | 1790 | `	if( piOff ){` |
|   ! 0 | 1791 | `		*piOff = iOff;` |
|   ! 0 | 1792 | `	}` |
|   128 | 1793 | `	iLocal = iTs + iOff;` |
|     - | 1794 | `	/* A floored division: C truncates toward zero, which would put every` |
|     - | 1795 | `	 * pre-1970 instant on the FOLLOWING day. */` |
|   128 | 1796 | `	if( iLocal < 0 && (iLocal % 86400) != 0 ){` |
|   ! 0 | 1797 | `		return ((iLocal / 86400) - 1) * 86400;` |
|     - | 1798 | `	}` |
|   128 | 1799 | `	return (iLocal / 86400) * 86400;` |
|    65 | 1800 | `}` |
|     - | 1801 | `/*` |
|     - | 1802 | ` * The hour on the base day as an absolute timestamp. php adds in DOUBLE and` |
|     - | 1803 | ` * truncates the sum, which is not the same as truncating the offset and adding` |
|     - | 1804 | ` * it: a rise east of the meridian has a NEGATIVE offset, and truncating that` |
|     - | 1805 | ` * toward zero rounds it up, putting every such answer one second late. Four of` |
|     - | 1806 | ` * date_sun_info()'s nine fields are that kind of offset, and Sydney read` |
|     - | 1807 | ` * one second late in all four before the addition moved inside the cast.` |
|     - | 1808 | ` */` |
|   144 | 1809 | `static sxi64 DtSunStamp(sxi64 iBase,double rHours)` |
|     2 | 1810 | `{` |
|   146 | 1811 | `	return (sxi64)((double)iBase + rHours * 3600.0);` |
|     2 | 1812 | `}` |
|     - | 1813 | `/*` |
|     - | 1814 | ` * A rise/set pair into the result array under php's two names, or the bool that` |
|     - | 1815 | ` * says the sun spent the whole day on one side of the altitude. php answers` |
|     - | 1816 | ` * TRUE for "never set" and FALSE for "never rose", and gives BOTH keys the same` |
|     - | 1817 | ` * bool -- so a caller that tests only one of them still learns which it was.` |
|     - | 1818 | ` */` |
|    80 | 1819 | `static void DtSunInfoPair(ph7_context *pCtx,ph7_value *pArray,ph7_value *pWork,` |
|     - | 1820 | `	sxi64 iBase,double rLon,double rLat,double rAltit,int bUpperLimb,` |
|     - | 1821 | `	const char *zBegin,const char *zEnd,double *prTransit)` |
|     2 | 1822 | `{` |
|     - | 1823 | `	double rRise,rSet,rTransit;` |
|    82 | 1824 | `	int rc = DtSunRiseSet(iBase,rLon,rLat,rAltit,bUpperLimb,&rRise,&rSet,&rTransit);` |
|    82 | 1825 | `	if( prTransit ){` |
|    22 | 1826 | `		*prTransit = rTransit;` |
|    10 | 1827 | `	}` |
|    82 | 1828 | `	if( rc != 0 ){` |
|    34 | 1829 | `		ph7_value_bool(pWork,rc > 0);` |
|    34 | 1830 | `		ph7_array_add_strkey_elem(pArray,zBegin,pWork);` |
|    34 | 1831 | `		ph7_value_bool(pWork,rc > 0);` |
|    34 | 1832 | `		ph7_array_add_strkey_elem(pArray,zEnd,pWork);` |
|    34 | 1833 | `		return;` |
|     - | 1834 | `	}` |
|    50 | 1835 | `	ph7_value_int64(pWork,DtSunStamp(iBase,rRise));` |
|    50 | 1836 | `	ph7_array_add_strkey_elem(pArray,zBegin,pWork);` |
|    50 | 1837 | `	ph7_value_int64(pWork,DtSunStamp(iBase,rSet));` |
|    50 | 1838 | `	ph7_array_add_strkey_elem(pArray,zEnd,pWork);` |
|    24 | 1839 | `	SXUNUSED(pCtx);` |
|    42 | 1840 | `}` |
|     - | 1841 | `/*` |
|     - | 1842 | ` * array date_sun_info(int $timestamp, float $latitude, float $longitude)` |
|     - | 1843 | ` *` |
|     - | 1844 | ` * The nine keys in php's order: the rise/set pair, the transit between them,` |
|     - | 1845 | ` * then the three twilights outward. Only the rise/set pair is asked of the` |
|     - | 1846 | ` * sun's upper limb.` |
|     - | 1847 | ` */` |
|    28 | 1848 | `PH7_PRIVATE int PH7_builtin_date_sun_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1849 | `{` |
|    30 | 1850 | `	double rLat,rLon,rTransit = 0.0;` |
|     - | 1851 | `	ph7_value *pArray,*pWork;` |
|     - | 1852 | `	sxi64 iTs,iBase;` |
|    30 | 1853 | `	if( nArg < 3 ){` |
|     - | 1854 | `		/* The arity screen has already spoken for the ordinary call; this is` |
|     - | 1855 | `		 * the belt for a direct dispatch. */` |
|   ! 0 | 1856 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1857 | `		return PH7_OK;` |
|     - | 1858 | `	}` |
|    30 | 1859 | `	iTs  = ph7_value_to_int64(apArg[0]);` |
|    30 | 1860 | `	rLat = ph7_value_to_double(apArg[1]);` |
|    30 | 1861 | `	rLon = ph7_value_to_double(apArg[2]);` |
|     - | 1862 | `	/* php screens these two and NOT the sunrise/sunset pair's, which quietly` |
|     - | 1863 | `	 * answer false instead -- both faces were measured. */` |
|    30 | 1864 | `	if( !DtSunIsFinite(rLat) ){` |
|     5 | 1865 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1866 | `			"date_sun_info(): Argument #2 ($latitude) must be finite");` |
|     - | 1867 | `	}` |
|    26 | 1868 | `	if( !DtSunIsFinite(rLon) ){` |
|     5 | 1869 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1870 | `			"date_sun_info(): Argument #3 ($longitude) must be finite");` |
|     - | 1871 | `	}` |
|    22 | 1872 | `	pArray = ph7_context_new_array(pCtx);` |
|    22 | 1873 | `	pWork  = ph7_context_new_scalar(pCtx);` |
|    22 | 1874 | `	if( pArray == 0 \|\| pWork == 0 ){` |
|   ! 0 | 1875 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 | 1876 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1877 | `		return PH7_OK;` |
|     - | 1878 | `	}` |
|    22 | 1879 | `	iBase = DtSunLocalMidnight(pCtx->pVm,iTs,0);` |
|    22 | 1880 | `	DtSunInfoPair(pCtx,pArray,pWork,iBase,rLon,rLat,DT_SUN_RISESET,1,` |
|     - | 1881 | `		"sunrise","sunset",&rTransit);` |
|     - | 1882 | `	/* The transit is the sun's meridian crossing and happens whether or not it` |
|     - | 1883 | `	 * rose, so it is a timestamp even on a polar day. */` |
|    22 | 1884 | `	ph7_value_int64(pWork,DtSunStamp(iBase,rTransit));` |
|    22 | 1885 | `	ph7_array_add_strkey_elem(pArray,"transit",pWork);` |
|    22 | 1886 | `	DtSunInfoPair(pCtx,pArray,pWork,iBase,rLon,rLat,-6.0,0,` |
|     - | 1887 | `		"civil_twilight_begin","civil_twilight_end",0);` |
|    22 | 1888 | `	DtSunInfoPair(pCtx,pArray,pWork,iBase,rLon,rLat,-12.0,0,` |
|     - | 1889 | `		"nautical_twilight_begin","nautical_twilight_end",0);` |
|    22 | 1890 | `	DtSunInfoPair(pCtx,pArray,pWork,iBase,rLon,rLat,-18.0,0,` |
|     - | 1891 | `		"astronomical_twilight_begin","astronomical_twilight_end",0);` |
|    22 | 1892 | `	ph7_result_value(pCtx,pArray);` |
|    22 | 1893 | `	return PH7_OK;` |
|    16 | 1894 | `}` |
|     - | 1895 | `/*` |
|     - | 1896 | ``  * The `date.default_latitude` / `date.default_longitude` / `date.sunrise_zenith` `` |
|     - | 1897 | `` * / `date.sunset_zenith` directives, which is where the sunrise/sunset pair`` |
|     - | 1898 | ` * takes every argument the caller left null. Read as text and converted here` |
|     - | 1899 | ` * because the ini layer has no float door.` |
|     - | 1900 | ` */` |
|    46 | 1901 | `static double DtSunIniFloat(ph7_vm *pVm,const char *zName,double rDefault)` |
|     2 | 1902 | `{` |
|     - | 1903 | `	const char *zVal;` |
|     - | 1904 | `	SyBlob sVal;` |
|    48 | 1905 | `	double r = rDefault;` |
|     - | 1906 | `	int nVal;` |
|    48 | 1907 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|    48 | 1908 | `	PH7_VmIniGetStr(pVm,zName,&sVal);` |
|    48 | 1909 | `	nVal = (int)SyBlobLength(&sVal);` |
|    48 | 1910 | `	zVal = (const char *)SyBlobData(&sVal);` |
|    48 | 1911 | `	if( nVal > 0 ){` |
|    48 | 1912 | `		SyStrToReal(zVal,(sxu32)nVal,(void *)&r,0);` |
|    23 | 1913 | `	}` |
|    48 | 1914 | `	SyBlobRelease(&sVal);` |
|    48 | 1915 | `	return r;` |
|     2 | 1916 | `}` |
|     - | 1917 | `/*` |
|     - | 1918 | ` * The body behind date_sunrise() and date_sunset(), which differ only in which` |
|     - | 1919 | ` * end of the arc they return and which zenith directive they default from.` |
|     - | 1920 | ` *` |
|     - | 1921 | ` * Everything here is php's, including the parts that look like oversights and` |
|     - | 1922 | ` * were confirmed against it: a non-finite argument returns FALSE rather than` |
|     - | 1923 | ` * throwing (date_sun_info throws for the same value), the hours are folded into` |
|     - | 1924 | ` * [0,24) for the STRING and DOUBLE shapes but the TIMESTAMP shape ignores` |
|     - | 1925 | ` * $utcOffset entirely, and "the sun never rose" and "the sun never set" are the` |
|     - | 1926 | ` * same answer -- false.` |
|     - | 1927 | ` */` |
|   126 | 1928 | `static int DtSunRiseSetDoor(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|     - | 1929 | `	int bSunset)` |
|     2 | 1930 | `{` |
|     - | 1931 | `	double rLat,rLon,rZenith,rUtcOff,rRise,rSet,rTransit,rHours;` |
|   128 | 1932 | `	const char *zZenithIni = bSunset ? "date.sunset_zenith" : "date.sunrise_zenith";` |
|   128 | 1933 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1934 | `	sxi64 iTs,iBase;` |
|   128 | 1935 | `	sxi32 iZoneOff = 0;` |
|   128 | 1936 | `	int iFormat = 1 /* SUNFUNCS_RET_STRING */;` |
|     - | 1937 | `	int bUtcOff,rc;` |
|   128 | 1938 | `	if( nArg < 1 ){` |
|   ! 0 | 1939 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1940 | `		return PH7_OK;` |
|     - | 1941 | `	}` |
|   128 | 1942 | `	iTs = ph7_value_to_int64(apArg[0]);` |
|   128 | 1943 | `	if( nArg > 1 ){` |
|   128 | 1944 | `		iFormat = ph7_value_to_int(apArg[1]);` |
|    63 | 1945 | `	}` |
|   128 | 1946 | `	if( iFormat < 0 \|\| iFormat > 2 ){` |
|    19 | 1947 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1948 | `			"%s(): Argument #2 ($returnFormat) must be one of "` |
|     - | 1949 | `			"SUNFUNCS_RET_TIMESTAMP, SUNFUNCS_RET_STRING, or SUNFUNCS_RET_DOUBLE",` |
|     6 | 1950 | `			bSunset ? "date_sunset" : "date_sunrise");` |
|     - | 1951 | `	}` |
|   115 | 1952 | `	rLat = (nArg > 2 && !ph7_value_is_null(apArg[2]))` |
|   112 | 1953 | `		? ph7_value_to_double(apArg[2])` |
|    58 | 1954 | `		: DtSunIniFloat(pVm,"date.default_latitude",31.7667);` |
|   115 | 1955 | `	rLon = (nArg > 3 && !ph7_value_is_null(apArg[3]))` |
|   112 | 1956 | `		? ph7_value_to_double(apArg[3])` |
|    58 | 1957 | `		: DtSunIniFloat(pVm,"date.default_longitude",35.2333);` |
|    95 | 1958 | `	rZenith = (nArg > 4 && !ph7_value_is_null(apArg[4]))` |
|    72 | 1959 | `		? ph7_value_to_double(apArg[4])` |
|    78 | 1960 | `		: DtSunIniFloat(pVm,zZenithIni,90.833333);` |
|     - | 1961 | `	/* $utcOffset defaults to the SCRIPT ZONE's offset, not to zero: left null,` |
|     - | 1962 | `	 * the clock face these two answer on is the local one. Defaulting it to` |
|     - | 1963 | `	 * zero instead put every Los Angeles answer eight hours out while UTC --` |
|     - | 1964 | `	 * where the two agree -- read perfectly green. */` |
|   116 | 1965 | `	bUtcOff = (nArg > 5 && !ph7_value_is_null(apArg[5]));` |
|   116 | 1966 | `	rUtcOff = bUtcOff ? ph7_value_to_double(apArg[5]) : 0.0;` |
|   114 | 1967 | `	if( !DtSunIsFinite(rLat) \|\| !DtSunIsFinite(rLon)` |
|   113 | 1968 | `	 \|\| !DtSunIsFinite(rZenith) \|\| !DtSunIsFinite(rUtcOff) ){` |
|     9 | 1969 | `		ph7_result_bool(pCtx,0);` |
|     9 | 1970 | `		return PH7_OK;` |
|     - | 1971 | `	}` |
|   108 | 1972 | `	iBase = DtSunLocalMidnight(pVm,iTs,0);` |
|   108 | 1973 | `	if( !bUtcOff ){` |
|     - | 1974 | `		/* php reads the zone's offset at the EPOCH, not at $timestamp, so the` |
|     - | 1975 | `		 * clock face these answer on ignores both DST and every rule change` |
|     - | 1976 | `		 * since 1970. It shows: Pacific/Kiritimati has been UTC+14 since 1995` |
|     - | 1977 | `		 * and still defaults to the -10:40 it kept in 1970, and Lord Howe's` |
|     - | 1978 | `		 * +10:30 reads as the +10:00 it was then. Neither is a rounding` |
|     - | 1979 | `		 * artefact and both were measured -- taking the offset at $timestamp` |
|     - | 1980 | `		 * instead left every DST day an hour out. */` |
|    70 | 1981 | `		int bDst = 0,nAbbr = 0;` |
|    70 | 1982 | `		const char *zAbbr = 0;` |
|    70 | 1983 | `		iZoneOff = DtTzOffsetOf(DtDefaultTzIndex(pVm),0,(sxi64)0,&bDst,&zAbbr,&nAbbr);` |
|    70 | 1984 | `		rUtcOff = (double)iZoneOff / 3600.0;` |
|    34 | 1985 | `	}` |
|     - | 1986 | `	/* Asked at the complement of the zenith and, like date_sun_info(), of the` |
|     - | 1987 | `	 * sun's UPPER LIMB. The two still disagree by ~133 seconds at one place on` |
|     - | 1988 | `	 * one day, because the stock zenith puts the centre 50 arcminutes down` |
|     - | 1989 | `	 * where sun_info asks for 35 -- so the gap is the 15 arcminutes, not a` |
|     - | 1990 | `	 * missing correction. Dropping the correction here instead moved every` |
|     - | 1991 | `	 * answer ~135 seconds the wrong way. */` |
|   108 | 1992 | `	rc = DtSunRiseSet(iBase,rLon,rLat,90.0 - rZenith,1,&rRise,&rSet,&rTransit);` |
|   108 | 1993 | `	if( rc != 0 ){` |
|    11 | 1994 | `		ph7_result_bool(pCtx,0);` |
|    11 | 1995 | `		return PH7_OK;` |
|     - | 1996 | `	}` |
|    98 | 1997 | `	rHours = bSunset ? rSet : rRise;` |
|    98 | 1998 | `	if( iFormat == 0 /* SUNFUNCS_RET_TIMESTAMP */ ){` |
|     - | 1999 | `		/* $utcOffset is deliberately not applied: a timestamp is already an` |
|     - | 2000 | `		 * absolute instant, and php leaves it alone. */` |
|    30 | 2001 | `		ph7_result_int64(pCtx,DtSunStamp(iBase,rHours));` |
|    30 | 2002 | `		return PH7_OK;` |
|     - | 2003 | `	}` |
|    69 | 2004 | `	rHours += rUtcOff;` |
|     - | 2005 | `	/* Fold into a clock face. php does this for both remaining shapes, so an` |
|     - | 2006 | `	 * offset of -8 reads 19:15 and not -4:44. */` |
|    69 | 2007 | `	rHours -= 24.0 * floor(rHours / 24.0);` |
|    69 | 2008 | `	if( iFormat == 2 /* SUNFUNCS_RET_DOUBLE */ ){` |
|    47 | 2009 | `		ph7_result_double(pCtx,rHours);` |
|    47 | 2010 | `		return PH7_OK;` |
|     - | 2011 | `	}` |
|     - | 2012 | `	/* SUNFUNCS_RET_STRING is "H:i" of the folded hour, TRUNCATED -- 3.2628 h` |
|     - | 2013 | `	 * is 03:15 and never 03:16. */` |
|     - | 2014 | `	{` |
|    23 | 2015 | `		int iHour = (int)rHours;` |
|    23 | 2016 | `		int iMin  = (int)((rHours - (double)iHour) * 60.0);` |
|    23 | 2017 | `		if( iHour > 23 ){ iHour = 23; }` |
|    23 | 2018 | `		if( iMin > 59 ){ iMin = 59; }` |
|    23 | 2019 | `		ph7_result_string_format(pCtx,"%02d:%02d",iHour,iMin);` |
|     - | 2020 | `	}` |
|    23 | 2021 | `	return PH7_OK;` |
|    65 | 2022 | `}` |
|     - | 2023 | `/*` |
|     - | 2024 | ` * string\|int\|float\|false date_sunrise(int $timestamp, int $returnFormat = SUNFUNCS_RET_STRING,` |
|     - | 2025 | ` *   ?float $latitude = null, ?float $longitude = null, ?float $zenith = null, ?float $utcOffset = null)` |
|     - | 2026 | ` */` |
|    54 | 2027 | `PH7_PRIVATE int PH7_builtin_date_sunrise(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2028 | `{` |
|    56 | 2029 | `	return DtSunRiseSetDoor(pCtx,nArg,apArg,0);` |
|     2 | 2030 | `}` |
|     - | 2031 | `/* The same door, the other end of the arc. */` |
|    72 | 2032 | `PH7_PRIVATE int PH7_builtin_date_sunset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2033 | `{` |
|    74 | 2034 | `	return DtSunRiseSetDoor(pCtx,nArg,apArg,1);` |
|     2 | 2035 | `}` |
|     - | 2036 |  |
|     - | 2037 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2038 |  |
