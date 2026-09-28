# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 618/683 lines (90.48%)

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
|     - |   25 | `#ifdef _WIN32_WCE` |
|     - |   26 | `/* SPDX-SnippetBegin */` |
|     - |   27 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|     - |   28 | `/* SPDX-License-Identifier: blessing */` |
|     - |   29 | `/*` |
|     - |   30 | `** WindowsCE does not have a localtime() function.  So create a` |
|     - |   31 | `** substitute.` |
|     - |   32 | `** Taken from the SQLite3 source tree.` |
|     - |   33 | `** Status: Public domain` |
|     - |   34 | `*/` |
|     - |   35 | `struct tm *__cdecl localtime(const time_t *t)` |
|     - |   36 | `{` |
|     - |   37 | `  static struct tm y;` |
|     - |   38 | `  FILETIME uTm, lTm;` |
|     - |   39 | `  SYSTEMTIME pTm;` |
|     - |   40 | `  ph7_int64 t64;` |
|     - |   41 | `  t64 = *t;` |
|     - |   42 | `  t64 = (t64 + 11644473600)*10000000;` |
|     - |   43 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|     - |   44 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|     - |   45 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|     - |   46 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|     - |   47 | `  y.tm_year = pTm.wYear - 1900;` |
|     - |   48 | `  y.tm_mon = pTm.wMonth - 1;` |
|     - |   49 | `  y.tm_wday = pTm.wDayOfWeek;` |
|     - |   50 | `  y.tm_mday = pTm.wDay;` |
|     - |   51 | `  y.tm_hour = pTm.wHour;` |
|     - |   52 | `  y.tm_min = pTm.wMinute;` |
|     - |   53 | `  y.tm_sec = pTm.wSecond;` |
|     - |   54 | `  return &y;` |
|     - |   55 | `}` |
|     - |   56 | `/* SPDX-SnippetEnd */` |
|     - |   57 | `#endif /*_WIN32_WCE */` |
|     - |   58 | `#elif defined(__UNIXES__)` |
|     - |   59 | `#include <sys/time.h>` |
|     - |   60 | `#endif /* __WINNT__*/` |
|     - |   61 | `/*` |
|     - |   62 | ` * Resolve the current wall-clock time (epoch seconds + sub-second microseconds).` |
|     - |   63 | ` *` |
|     - |   64 | ` * An embedder may override the platform clock via PH7_CONFIG_CLOCK (e.g. the` |
|     - |   65 | ` * ESP32 port routes this through esp_timer); when no hook is registered we use` |
|     - |   66 | ` * gettimeofday() on Unix and fall back to a second-resolution time() elsewhere.` |
|     - |   67 | ` * Centralising this here gives microtime()/gettimeofday() a single sub-second` |
|     - |   68 | `` * source instead of the old nonsensical `tt % SX_USEC_PER_SEC` off-Unix path.`` |
|     - |   69 | ` */` |
|  3476 |   70 | `static void DateNow(ph7_vm *pVm,sytime *pOut)` |
|     4 |   71 | `{` |
|  3480 |   72 | `	if( pVm && pVm->pEngine->xConf.xClock ){` |
|   ! 0 |   73 | `		ph7_int64 sec = 0,usec = 0;` |
|   ! 0 |   74 | `		if( pVm->pEngine->xConf.xClock(pVm->pEngine->xConf.pClockData,&sec,&usec) == PH7_OK ){` |
|   ! 0 |   75 | `			pOut->tm_sec  = (long)sec;` |
|   ! 0 |   76 | `			pOut->tm_usec = (long)usec;` |
|   ! 0 |   77 | `			return;` |
|     - |   78 | `		}` |
|   ! 0 |   79 | `	}` |
|     - |   80 | `#if defined(__UNIXES__)` |
|     - |   81 | `	{` |
|     - |   82 | `		struct timeval tv;` |
|  3476 |   83 | `		gettimeofday(&tv,0);` |
|  3476 |   84 | `		pOut->tm_sec  = (long)tv.tv_sec;` |
|  3476 |   85 | `		pOut->tm_usec = (long)tv.tv_usec;` |
|     - |   86 | `	}` |
|     - |   87 | `#elif defined(__WINNT__)` |
|     - |   88 | `	{` |
|     - |   89 | `		/* FILETIME is 100-ns ticks since 1601-01-01 UTC; convert to the Unix` |
|     - |   90 | `		 * epoch with microsecond resolution (GetSystemTime() only carries` |
|     - |   91 | `		 * milliseconds, and time() has no sub-second part at all). */` |
|     - |   92 | `		FILETIME ft;` |
|     - |   93 | `		ph7_int64 t;` |
|     4 |   94 | `		GetSystemTimeAsFileTime(&ft);` |
|     4 |   95 | `		t  = (ph7_int64)ft.dwHighDateTime << 32;` |
|     4 |   96 | `		t += ft.dwLowDateTime;` |
|     4 |   97 | `		t -= 116444736000000000LL; /* 100-ns ticks between 1601 and 1970 */` |
|     4 |   98 | `		pOut->tm_sec  = (long)(t / 10000000);` |
|     4 |   99 | `		pOut->tm_usec = (long)((t % 10000000) / 10);` |
|     - |  100 | `	}` |
|     - |  101 | `#else` |
|     - |  102 | `	{` |
|     - |  103 | `		time_t tt;` |
|     - |  104 | `		time(&tt);` |
|     - |  105 | `		pOut->tm_sec  = (long)tt;` |
|     - |  106 | `		pOut->tm_usec = 0; /* no sub-second source; embedders supply one via PH7_CONFIG_CLOCK */` |
|     - |  107 | `	}` |
|     - |  108 | `#endif /* __UNIXES__ */` |
|  1742 |  109 | `}` |
|     - |  110 | `/*` |
|     - |  111 | ` * The current moment as the DateTime layer wants it: epoch seconds plus the` |
|     - |  112 | ` * MICROSECONDS beside them.` |
|     - |  113 | ` *` |
|     - |  114 | ` * php's date classes take their base moment from the same clock microtime()` |
|     - |  115 | `` * reads, sub-second part included -- `new DateTime()` carries the microseconds`` |
|     - |  116 | `` * of the instant it was built, which is what makes `$a->diff($b)->f` mean`` |
|     - |  117 | ` * anything for two moments a program measured. The parse layer used to read` |
|     - |  118 | `` * `time(0)` directly, so every one of them was born on a whole second and every`` |
|     - |  119 | ` * such diff answered 0.0. Routing them through DateNow() also hands the date` |
|     - |  120 | ` * classes the PH7_CONFIG_CLOCK hook the procedural half already had.` |
|     - |  121 | ` */` |
|  3398 |  122 | `PH7_PRIVATE void DtNowUs(ph7_vm *pVm,sxi64 *piSec,int *puSec)` |
|     4 |  123 | `{` |
|     - |  124 | `	sytime sNow;` |
|  3402 |  125 | `	DateNow(pVm,&sNow);` |
|  3402 |  126 | `	if( piSec ){` |
|  3402 |  127 | `		*piSec = (sxi64)sNow.tm_sec;` |
|  1699 |  128 | `	}` |
|  3402 |  129 | `	if( puSec ){` |
|  3322 |  130 | `		*puSec = (int)sNow.tm_usec;` |
|  1659 |  131 | `	}` |
|  3402 |  132 | `}` |
|     - |  133 | `/*` |
|     - |  134 | ` * Break a Unix timestamp (or the current time) down into a Sytm the way the` |
|     - |  135 | ` * DateTime layer does: PHL's own civil arithmetic, not the platform's gmtime().` |
|     - |  136 | ` *` |
|     - |  137 | `` * gmtime() keeps its year in an `int` and simply FAILS past it, and the five`` |
|     - |  138 | ` * procedural doors below then formatted the CURRENT time instead -- so` |
|     - |  139 | `` * `date('Y', PHP_INT_MAX)` read today's year here where php reads`` |
|     - |  140 | ` * 292277026596. The engine has no tz database (date_default_timezone_set()` |
|     - |  141 | ` * takes UTC/GMT only), so date() and gmdate() share this UTC breakdown exactly` |
|     - |  142 | ` * as they already did through gmtime().` |
|     - |  143 | ` */` |
|   750 |  144 | `static void DtSytmOfTimestamp(sxi64 iTs,Sytm *pOut)` |
|     2 |  145 | `{` |
|     - |  146 | `	/* A NULL tm_zone is what the date()-family fills mean by "the script's` |
|     - |  147 | `	 * default timezone" -- DateFormat's 'e'/'T' read pVm->zDefTz through it,` |
|     - |  148 | `	 * so naming the zone here would pin every one of them to UTC. */` |
|   752 |  149 | `	DtFillSytm(iTs,0,0,pOut);` |
|   752 |  150 | `}` |
|     - |  151 | ` /*` |
|     - |  152 | `  * int64 time(void)` |
|     - |  153 | `  *  Current Unix timestamp` |
|     - |  154 | `  * Parameters` |
|     - |  155 | `  *  None.` |
|     - |  156 | `  * Return` |
|     - |  157 | `  *  Returns the current time measured in the number of seconds` |
|     - |  158 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|     - |  159 | `  */` |
|    18 |  160 | `PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  161 | `{` |
|     - |  162 | `	time_t tt;` |
|     9 |  163 | `	SXUNUSED(nArg); /* cc warning */` |
|     9 |  164 | `	SXUNUSED(apArg);` |
|     - |  165 | `	/* Extract the current time */` |
|    21 |  166 | `	time(&tt);` |
|     - |  167 | `	/* Return as 64-bit integer */` |
|    21 |  168 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|    21 |  169 | `	return  PH7_OK;` |
|     3 |  170 | `}` |
|     - |  171 | `/*` |
|     - |  172 | `  * string/float microtime([ bool $get_as_float = false ])` |
|     - |  173 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|     - |  174 | `  * Parameters` |
|     - |  175 | `  *  $get_as_float` |
|     - |  176 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|     - |  177 | `  *   as described in the return values section below.` |
|     - |  178 | `  * Return` |
|     - |  179 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|     - |  180 | `  *  is the current time measured in the number of seconds since the Unix` |
|     - |  181 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|     - |  182 | `  *  that have elapsed since sec expressed in seconds.` |
|     - |  183 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|     - |  184 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|     - |  185 | `  */` |
|    70 |  186 | `PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  187 | `{` |
|    72 |  188 | `	int bFloat = 0;` |
|     - |  189 | `	sytime sTime;` |
|    72 |  190 | `	DateNow(pCtx->pVm,&sTime);` |
|    72 |  191 | `	if( nArg > 0 ){` |
|    66 |  192 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|    32 |  193 | `	}` |
|    72 |  194 | `	if( bFloat ){` |
|     - |  195 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|    66 |  196 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|    34 |  197 | `	}else{` |
|     - |  198 | `		/* Return PHP's "msec sec" form: the sub-second part as fractional` |
|     - |  199 | `		 * seconds to 8 decimals, e.g. "0.50667100 1700000000". tm_usec is in` |
|     - |  200 | `		 * microseconds (0..999999), so scaling by 100 yields the 8-digit` |
|     - |  201 | `		 * fraction — matching PHP's "%.8F" output exactly. */` |
|     7 |  202 | `		ph7_result_string_format(pCtx,"0.%08ld %ld",sTime.tm_usec*100,sTime.tm_sec);` |
|     - |  203 | `	}` |
|    72 |  204 | `	return PH7_OK;` |
|     2 |  205 | `}` |
|     - |  206 | `/*` |
|     - |  207 | ` * array\|int hrtime(bool $as_number = false)` |
|     - |  208 | ` *  The system's high-resolution time, counted from an arbitrary monotonic` |
|     - |  209 | ` *  point in nanoseconds. Returns [seconds, nanoseconds] by default, or the` |
|     - |  210 | ` *  total nanoseconds as an int when $as_number is true.` |
|     - |  211 | ` */` |
|     8 |  212 | `PH7_PRIVATE int PH7_builtin_hrtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  213 | `{` |
|     9 |  214 | `	ph7_int64 sec = 0,nsec = 0;` |
|     9 |  215 | `	int bAsNumber = 0;` |
|     9 |  216 | `	if( nArg > 0 ){` |
|     7 |  217 | `		bAsNumber = ph7_value_to_bool(apArg[0]);` |
|     3 |  218 | `	}` |
|     - |  219 | `#if defined(CLOCK_MONOTONIC)` |
|     - |  220 | `	{` |
|     - |  221 | `		struct timespec ts;` |
|     8 |  222 | `		if( clock_gettime(CLOCK_MONOTONIC,&ts) == 0 ){` |
|     8 |  223 | `			sec  = (ph7_int64)ts.tv_sec;` |
|     8 |  224 | `			nsec = (ph7_int64)ts.tv_nsec;` |
|     4 |  225 | `		}` |
|     - |  226 | `	}` |
|     - |  227 | `#else` |
|     - |  228 | `	{` |
|     - |  229 | `		/* No monotonic clock available: fall back to the wall-clock microsecond` |
|     - |  230 | `		 * source (embedder clock / gettimeofday). Coarser and not strictly` |
|     - |  231 | `		 * monotonic, but keeps hrtime() usable off-Unix. */` |
|     - |  232 | `		sytime sTime;` |
|     1 |  233 | `		DateNow(pCtx->pVm,&sTime);` |
|     1 |  234 | `		sec  = (ph7_int64)sTime.tm_sec;` |
|     1 |  235 | `		nsec = (ph7_int64)sTime.tm_usec * 1000;` |
|     - |  236 | `	}` |
|     - |  237 | `#endif` |
|     9 |  238 | `	if( bAsNumber ){` |
|     7 |  239 | `		ph7_result_int64(pCtx,sec * 1000000000LL + nsec);` |
|     4 |  240 | `	}else{` |
|     - |  241 | `		ph7_value *pValue,*pArray;` |
|     3 |  242 | `		pArray = ph7_context_new_array(pCtx);` |
|     3 |  243 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     3 |  244 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 |  245 | `			ph7_result_null(pCtx);` |
|   ! 0 |  246 | `			return PH7_OK;` |
|     - |  247 | `		}` |
|     3 |  248 | `		ph7_value_int64(pValue,sec);` |
|     3 |  249 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     3 |  250 | `		ph7_value_int64(pValue,nsec);` |
|     3 |  251 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     3 |  252 | `		ph7_result_value(pCtx,pArray);` |
|     3 |  253 | `		ph7_context_release_value(pCtx,pValue);` |
|     3 |  254 | `		ph7_context_release_value(pCtx,pArray);` |
|     - |  255 | `	}` |
|     9 |  256 | `	return PH7_OK;` |
|     5 |  257 | `}` |
|     - |  258 | `/*` |
|     - |  259 | ` * array getdate ([ int $timestamp = time() ])` |
|     - |  260 | ` *  Returns an associative array containing the date information` |
|     - |  261 | ` *  of the timestamp, or the current local time if no timestamp is given.` |
|     - |  262 | ` * Parameter` |
|     - |  263 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - |  264 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - |  265 | ` *     In other words, it defaults to the value of time().` |
|     - |  266 | ` * Returns` |
|     - |  267 | ` *  Returns an associative array of information related to the timestamp.` |
|     - |  268 | ` */` |
|    16 |  269 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  270 | `{` |
|     - |  271 | `	ph7_value *pValue,*pArray;` |
|     - |  272 | `	Sytm sTm;` |
|     - |  273 | `	time_t t;` |
|    17 |  274 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|     5 |  275 | `		time(&t);` |
|     3 |  276 | `	}else{` |
|     - |  277 | `		/* Use the given timestamp */` |
|    13 |  278 | `		t = (time_t)ph7_value_to_int64(apArg[0]);` |
|     - |  279 | `	}` |
|    17 |  280 | `	DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     - |  281 | `	/* Element value */` |
|    17 |  282 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    17 |  283 | `	if( pValue == 0 ){` |
|     - |  284 | `		/* Return NULL */` |
|   ! 0 |  285 | `		ph7_result_null(pCtx);` |
|   ! 0 |  286 | `		return PH7_OK;` |
|     - |  287 | `	}` |
|     - |  288 | `	/* Create a new array */` |
|    17 |  289 | `	pArray = ph7_context_new_array(pCtx);` |
|    17 |  290 | `	if( pArray == 0 ){` |
|     - |  291 | `		/* Return NULL */` |
|   ! 0 |  292 | `		ph7_result_null(pCtx);` |
|   ! 0 |  293 | `		return PH7_OK;` |
|     - |  294 | `	}` |
|     - |  295 | `	/* Fill the array */` |
|     - |  296 | `	/* Seconds */` |
|    17 |  297 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    17 |  298 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|     - |  299 | `	/* Minutes */` |
|    17 |  300 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    17 |  301 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|     - |  302 | `	/* Hours */` |
|    17 |  303 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    17 |  304 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|     - |  305 | `	/* mday */` |
|    17 |  306 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    17 |  307 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|     - |  308 | `	/* wday */` |
|    17 |  309 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    17 |  310 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|     - |  311 | `	/* mon */` |
|    17 |  312 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|    17 |  313 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|     - |  314 | `	/* year */` |
|    17 |  315 | `	ph7_value_int64(pValue,sTm.tm_year);` |
|    17 |  316 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|     - |  317 | `	/* yday */` |
|    17 |  318 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    17 |  319 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|     - |  320 | `	/* Weekday [i.e: Monday,Tuesday,...] */` |
|    17 |  321 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|    17 |  322 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|     - |  323 | `	/* Reset the string cursor */` |
|    17 |  324 | `	ph7_value_reset_string_cursor(pValue);` |
|     - |  325 | `	/* Month [i.e: January,February,...] */` |
|    17 |  326 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|    17 |  327 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|     - |  328 | `	/* php's eleventh entry, keyed by the INTEGER 0 and written last: the` |
|     - |  329 | `	 * timestamp the other ten were derived from. It was missing, so the` |
|     - |  330 | ``	 * documented `$g[0]` read as an Undefined array key. */`` |
|    17 |  331 | `	ph7_value_int64(pValue,(ph7_int64)t);` |
|    17 |  332 | `	ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - |  333 | `	/* Return the freshly created array */` |
|    17 |  334 | `	ph7_result_value(pCtx,pArray);` |
|    17 |  335 | `	return PH7_OK;` |
|     9 |  336 | `}` |
|     - |  337 | `/*` |
|     - |  338 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|     - |  339 | ` *  Returns an associative array containing the data returned from the system call.` |
|     - |  340 | ` * Parameters` |
|     - |  341 | ` *  $return_float` |
|     - |  342 | ` *   When set to TRUE, a float instead of an array is returned.` |
|     - |  343 | ` * Return` |
|     - |  344 | ` *  By default an array is returned. If return_float is set, then` |
|     - |  345 | ` *  a float is returned.` |
|     - |  346 | ` */` |
|     8 |  347 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  348 | `{` |
|     9 |  349 | `	int bFloat = 0;` |
|     - |  350 | `	sytime sTime;` |
|     9 |  351 | `	DateNow(pCtx->pVm,&sTime);` |
|     9 |  352 | `	if( nArg > 0 ){` |
|     7 |  353 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|     3 |  354 | `	}` |
|     9 |  355 | `	if( bFloat ){` |
|     - |  356 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|     5 |  357 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|     3 |  358 | `	}else{` |
|     - |  359 | `		/* Return an associative array */` |
|     - |  360 | `		ph7_value *pValue,*pArray;` |
|     - |  361 | `		/* Create a new array */` |
|     5 |  362 | `		pArray = ph7_context_new_array(pCtx);` |
|     - |  363 | `		/* Element value */` |
|     5 |  364 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     5 |  365 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|     - |  366 | `			/* Return NULL */` |
|   ! 0 |  367 | `			ph7_result_null(pCtx);` |
|   ! 0 |  368 | `			return PH7_OK;` |
|     - |  369 | `		}` |
|     - |  370 | `		/* Fill the array */` |
|     - |  371 | `		/* sec */` |
|     5 |  372 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|     5 |  373 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|     - |  374 | `		/* usec */` |
|     5 |  375 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|     5 |  376 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|     - |  377 | `		/* Return the array */` |
|     5 |  378 | `		ph7_result_value(pCtx,pArray);` |
|     - |  379 | `	}` |
|     9 |  380 | `	return PH7_OK;` |
|     5 |  381 | `}` |
|     - |  382 | `/* Check if the given year is leap or not */` |
|     - |  383 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|     - |  384 | `/* A year's magnitude, for the tokens php pads BEHIND the sign. Negating through` |
|     - |  385 | ` * sxu64 keeps the arithmetic defined even at the 64-bit floor. */` |
|     - |  386 | `#define DT_ABSYEAR(Y) ((sxi64)((Y) < 0 ? (sxu64)0 - (sxu64)(Y) : (sxu64)(Y)))` |
|     - |  387 | `/* ISO-8601 numeric representation of the day of the week */` |
|     - |  388 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|     - |  389 | `/*` |
|     - |  390 | ` * Format a given date string.` |
|     - |  391 | ` * Supported format: (Taken from PHP online docs)` |
|     - |  392 | ` * character 	Description` |
|     - |  393 | ` * d          Day of the month, 2 digits with leading zeros` |
|     - |  394 | ` * D          A textual representation of a day, three letters` |
|     - |  395 | ` * j          Day of the month without leading zeros` |
|     - |  396 | ` * l          A full textual representation of the day of the week` |
|     - |  397 | ` * N          ISO-8601 numeric representation of the day of the week` |
|     - |  398 | ` * w          Numeric representation of the day of the week` |
|     - |  399 | ` * z          The day of the year (starting from 0)` |
|     - |  400 | ` * F          A full textual representation of a month, such as January or March` |
|     - |  401 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|     - |  402 | ` * M          A short textual representation of a month, three letters` |
|     - |  403 | ` * n          Numeric representation of a month, without leading zeros` |
|     - |  404 | ` * t          Number of days in the given month` |
|     - |  405 | ` * L          Whether it's a leap year` |
|     - |  406 | ` * o          ISO-8601 year number. This has the same value as Y` |
|     - |  407 | ` * Y          A full numeric representation of a year, 4 digits` |
|     - |  408 | ` * y          A two digit representation of a year` |
|     - |  409 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|     - |  410 | ` * A          Uppercase Ante meridiem and Post meridiem` |
|     - |  411 | ` * g          12-hour format of an hour without leading zeros` |
|     - |  412 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|     - |  413 | ` * h          12-hour format of an hour with leading zeros` |
|     - |  414 | ` * H          24-hour format of an hour with leading zeros` |
|     - |  415 | ` * i          Minutes with leading zeros` |
|     - |  416 | ` * s          Seconds, with leading zeros` |
|     - |  417 | ` * u          Microseconds` |
|     - |  418 | ` * e          Timezone identifier` |
|     - |  419 | ` * I          Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|     - |  420 | ` * r          RFC 2822 formatted date` |
|     - |  421 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|     - |  422 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|     - |  423 | ` * O          Difference to Greenwich time (GMT) in hours` |
|     - |  424 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|     - |  425 | ` *            east of UTC is always positive.` |
|     - |  426 | ` * c         ISO 8601 date` |
|     - |  427 | ` */` |
|  3608 |  428 | `PH7_PRIVATE sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec)` |
|     3 |  429 | `{` |
|  3611 |  430 | `	const char *zEnd = &zIn[nLen];` |
|     - |  431 | `	const char *zCur;` |
|     - |  432 | `	/* Start the format process */` |
| 15783 |  433 | `	for(;;){` |
| 31569 |  434 | `		if( zIn >= zEnd ){` |
|     - |  435 | `			/* No more input to process */` |
|  3611 |  436 | `			break;` |
|     - |  437 | `		}` |
| 27961 |  438 | `		switch(zIn[0]){` |
|  1187 |  439 | `		case 'd':` |
|     - |  440 | `			/* Day of the month, 2 digits with leading zeros */` |
|  2376 |  441 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|  2376 |  442 | `			break;` |
|    84 |  443 | `		case 'D':` |
|     - |  444 | `			/*A textual representation of a day, three letters*/` |
|   169 |  445 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|   169 |  446 | `			ph7_result_string(pCtx,zCur,3);` |
|   169 |  447 | `			break;` |
|     1 |  448 | `		case 'j':` |
|     - |  449 | `			/*	Day of the month without leading zeros */` |
|     3 |  450 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|     3 |  451 | `			break;` |
|     3 |  452 | `		case 'l':` |
|     - |  453 | `			/* A full textual representation of the day of the week */` |
|     7 |  454 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|     7 |  455 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     7 |  456 | `			break;` |
|     1 |  457 | `		case 'N':{` |
|     - |  458 | `			/* ISO-8601 numeric representation of the day of the week */` |
|     3 |  459 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|     3 |  460 | `			break;` |
|     - |  461 | `				 }` |
|     1 |  462 | `		case 'w':` |
|     - |  463 | `			/*Numeric representation of the day of the week*/` |
|     3 |  464 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|     3 |  465 | `			break;` |
|     1 |  466 | `		case 'z':` |
|     - |  467 | `			/*The day of the year*/` |
|     3 |  468 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|     3 |  469 | `			break;` |
|     3 |  470 | `		case 'F':` |
|     - |  471 | `			/*A full textual representation of a month, such as January or March*/` |
|     7 |  472 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|     7 |  473 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     7 |  474 | `			break;` |
|  1187 |  475 | `		case 'm':` |
|     - |  476 | `			/*Numeric representation of a month, with leading zeros*/` |
|  2376 |  477 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|  2376 |  478 | `			break;` |
|     1 |  479 | `		case 'M':` |
|     - |  480 | `			/*A short textual representation of a month, three letters*/` |
|     3 |  481 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|     3 |  482 | `			ph7_result_string(pCtx,zCur,3);` |
|     3 |  483 | `			break;` |
|     1 |  484 | `		case 'n':` |
|     - |  485 | `			/*Numeric representation of a month, without leading zeros*/` |
|     3 |  486 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|     3 |  487 | `			break;` |
|     1 |  488 | `		case 't':{` |
|     - |  489 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|     3 |  490 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|     3 |  491 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|   ! 0 |  492 | `				nDays = 28;` |
|   ! 0 |  493 | `			}` |
|     - |  494 | `			/*Number of days in the given month*/` |
|     3 |  495 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|     3 |  496 | `			break;` |
|     - |  497 | `				 }` |
|     1 |  498 | `		case 'L':{` |
|     3 |  499 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|     - |  500 | `			/* Whether it's a leap year */` |
|     3 |  501 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|     3 |  502 | `			break;` |
|     - |  503 | `				 }` |
|    27 |  504 | `		case 'o': case 'W': {` |
|     - |  505 | `			/* ISO-8601 week-numbering year / week number: both belong to the` |
|     - |  506 | `			 * year owning the Thursday of the civil week (php: 2024-12-31 is` |
|     - |  507 | `			 * 2025-W01, 2027-01-01 is 2026-W53). php pads W but not o. */` |
|    55 |  508 | `			sxi64 days = DtDaysFromCivil(pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday);` |
|    55 |  509 | `			int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|    55 |  510 | `			sxi64 thu = days + (4 - isoDow);` |
|     - |  511 | `			sxi64 wy;` |
|     - |  512 | `			int wm,wd;` |
|    55 |  513 | `			DtCivilFromDays(thu,&wy,&wm,&wd);` |
|    55 |  514 | `			if( zIn[0] == 'o' ){` |
|    49 |  515 | `				ph7_result_string_format(pCtx,"%qd",wy);` |
|    25 |  516 | `			}else{` |
|    10 |  517 | `				ph7_result_string_format(pCtx,"%02d",` |
|     6 |  518 | `					(int)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1);` |
|     - |  519 | `			}` |
|    55 |  520 | `			break;` |
|     - |  521 | `				 }` |
|  1098 |  522 | `		case 'Y':` |
|     - |  523 | `			/* A full numeric representation of a year, at least 4 digits. php pads` |
|     - |  524 | `			 * the ABSOLUTE value behind the sign (-495 prints "-0495"); a plain` |
|     - |  525 | `			 * "%04qd" spends one of the four columns on the '-' and printed "-495". */` |
|  4360 |  526 | `			ph7_result_string_format(pCtx,"%s%04qd",` |
|  3260 |  527 | `				pTm->tm_year < 0 ? "-" : "",DT_ABSYEAR(pTm->tm_year));` |
|  2198 |  528 | `			break;` |
|    22 |  529 | `		case 'X':` |
|     - |  530 | `			/* Expanded full year, always signed (php 8.2+): +2024 */` |
|    80 |  531 | `			ph7_result_string_format(pCtx,"%c%04qd",` |
|    57 |  532 | `				pTm->tm_year < 0 ? '-' : '+',DT_ABSYEAR(pTm->tm_year));` |
|    45 |  533 | `			break;` |
|    22 |  534 | `		case 'x':` |
|     - |  535 | `			/* Expanded year, signed only past 4 digits (php 8.2+) — otherwise 'Y'. */` |
|    45 |  536 | `			if( pTm->tm_year > 9999 ){` |
|     5 |  537 | `				ph7_result_string_format(pCtx,"+%qd",pTm->tm_year);` |
|     3 |  538 | `			}else{` |
|    72 |  539 | `				ph7_result_string_format(pCtx,"%s%04qd",` |
|    51 |  540 | `					pTm->tm_year < 0 ? "-" : "",DT_ABSYEAR(pTm->tm_year));` |
|     - |  541 | `			}` |
|    45 |  542 | `			break;` |
|    22 |  543 | `		case 'y':` |
|     - |  544 | `			/*A two digit representation of a year*/` |
|    45 |  545 | `			ph7_result_string_format(pCtx,"%02qd",pTm->tm_year%100);` |
|    45 |  546 | `			break;` |
|     3 |  547 | `		case 'a':` |
|     - |  548 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|     7 |  549 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "pm" : "am",2);` |
|     7 |  550 | `			break;` |
|     3 |  551 | `		case 'A':` |
|     - |  552 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|     7 |  553 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "PM" : "AM",2);` |
|     7 |  554 | `			break;` |
|     1 |  555 | `		case 'B':{` |
|     - |  556 | `			/* Swatch Internet time: thousandths of the UTC+1 day. Only the time` |
|     - |  557 | `			 * OF DAY decides it, so take it from the clock fields rather than` |
|     - |  558 | `			 * rebuilding the absolute timestamp -- that product overflows an` |
|     - |  559 | `			 * int64 near the extremes of php's own clock. */` |
|     4 |  560 | `			sxi64 iBie = ((sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + pTm->tm_sec` |
|     2 |  561 | `				- (sxi64)pTm->tm_gmtoff + 3600) % 86400;` |
|     3 |  562 | `			if( iBie < 0 ){` |
|   ! 0 |  563 | `				iBie += 86400;` |
|   ! 0 |  564 | `			}` |
|     3 |  565 | `			ph7_result_string_format(pCtx,"%03d",(int)(iBie * 1000 / 86400));` |
|     3 |  566 | `			break;` |
|     - |  567 | `				 }` |
|     3 |  568 | `		case 'g':` |
|     - |  569 | `			/*	12-hour format of an hour without leading zeros*/` |
|    10 |  570 | `			ph7_result_string_format(pCtx,"%d",` |
|     6 |  571 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|     7 |  572 | `			break;` |
|     1 |  573 | `		case 'G':` |
|     - |  574 | `			/* 24-hour format of an hour without leading zeros */` |
|     3 |  575 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|     3 |  576 | `			break;` |
|     3 |  577 | `		case 'h':` |
|     - |  578 | `			/* 12-hour format of an hour with leading zeros */` |
|    10 |  579 | `			ph7_result_string_format(pCtx,"%02d",` |
|     6 |  580 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|     7 |  581 | `			break;` |
|  1064 |  582 | `		case 'H':` |
|     - |  583 | `			/*	24-hour format of an hour with leading zeros */` |
|  2129 |  584 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|  2129 |  585 | `			break;` |
|  1064 |  586 | `		case 'i':` |
|     - |  587 | `			/* 	Minutes with leading zeros */` |
|  2129 |  588 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|  2129 |  589 | `			break;` |
|  1030 |  590 | `		case 's':` |
|     - |  591 | `			/* 	second with leading zeros */` |
|  2061 |  592 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|  2061 |  593 | `			break;` |
|   359 |  594 | `		case 'u':` |
|     - |  595 | `			/* 	Microseconds. date()/gmdate() have no sub-second part (uSec == 0);` |
|     - |  596 | `			 * 	DateTime::format passes its stored microseconds. */` |
|   719 |  597 | `			ph7_result_string_format(pCtx,"%06d",uSec);` |
|   719 |  598 | `			break;` |
|     4 |  599 | `		case 'v':` |
|     - |  600 | `			/* 	Milliseconds */` |
|     9 |  601 | `			ph7_result_string_format(pCtx,"%03d",uSec/1000);` |
|     9 |  602 | `			break;` |
|     1 |  603 | `		case 'S':{` |
|     - |  604 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|     - |  605 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|     3 |  606 | `			int v = pTm->tm_mday;` |
|     3 |  607 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|     3 |  608 | `			break;` |
|     - |  609 | `				 }` |
|    82 |  610 | `		case 'e':` |
|     - |  611 | `			/* 	Timezone identifier */` |
|   165 |  612 | `			zCur = pTm->tm_zone;` |
|   165 |  613 | `			if( zCur == 0 ){` |
|     - |  614 | `				/* date()-family fills: the script default timezone */` |
|     7 |  615 | `				zCur = pCtx->pVm->zDefTz;` |
|     3 |  616 | `			}` |
|   165 |  617 | `			ph7_result_string(pCtx,zCur,-1);` |
|   165 |  618 | `			break;` |
|    53 |  619 | `		case 'T':{` |
|     - |  620 | `			/* Timezone abbreviation: "GMT+0530" for a fixed offset, the name` |
|     - |  621 | `			 * itself (uppercased) for a named zone. php decides on the zone's` |
|     - |  622 | `			 * TYPE, not on the offset's value, so a zone whose name is an offset` |
|     - |  623 | ``			 * spelling — `new DateTime('@0')`, `new DateTimeZone('+00:00')` —`` |
|     - |  624 | `			 * prints "GMT+0000" where PHL printed the name "+00:00". PHL has no` |
|     - |  625 | `			 * tz database, so the name path only ever sees UTC/GMT/Z. */` |
|     - |  626 | `			const char *z;` |
|   106 |  627 | `			if( pTm->tm_gmtoff != 0` |
|    73 |  628 | `			 \|\| (pTm->tm_zone && (pTm->tm_zone[0] == '+' \|\| pTm->tm_zone[0] == '-')) ){` |
|    87 |  629 | `				long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   130 |  630 | `				ph7_result_string_format(pCtx,"GMT%c%02d%02d",` |
|    86 |  631 | `					pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    87 |  632 | `				break;` |
|     - |  633 | `			}` |
|    21 |  634 | `			z = pTm->tm_zone ? pTm->tm_zone : pCtx->pVm->zDefTz;` |
|    73 |  635 | `			while( *z ){` |
|    53 |  636 | `				int c = (unsigned char)*z;` |
|    53 |  637 | `				if( c >= 'a' && c <= 'z' ){` |
|   ! 0 |  638 | `					c -= 'a' - 'A';` |
|   ! 0 |  639 | `				}` |
|    53 |  640 | `				ph7_result_string_format(pCtx,"%c",c);` |
|    53 |  641 | `				z++;` |
|     1 |  642 | `			}` |
|    21 |  643 | `			break;` |
|     - |  644 | `				 }` |
|     1 |  645 | `		case 'I':` |
|     - |  646 | `			/* Whether or not the date is in daylight saving time. Use the` |
|     - |  647 | `			 * broken-down time's own tm_isdst (as every other platform does):` |
|     - |  648 | `			 * the old Windows _get_daylight() override reported whether the` |
|     - |  649 | `			 * timezone observes DST at all, not whether THIS date is in it. */` |
|     3 |  650 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|     3 |  651 | `			break;` |
|    22 |  652 | `		case 'r':{` |
|     - |  653 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200 */` |
|    45 |  654 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     - |  655 | `			/* php zero-pads this year to four columns INCLUDING the sign` |
|     - |  656 | `			 * ("0050", "-001"), where a plain "%4d" space-padded every year` |
|     - |  657 | `			 * below 1000 — an ordinary date, not just a BCE one. */` |
|    45 |  658 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %04qd %02d:%02d:%02d %c%02d%02d",` |
|    22 |  659 | `				SyTimeGetDay(pTm->tm_wday),` |
|    22 |  660 | `				pTm->tm_mday,` |
|    22 |  661 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    22 |  662 | `				pTm->tm_year,` |
|    22 |  663 | `				pTm->tm_hour,` |
|    22 |  664 | `				pTm->tm_min,` |
|    22 |  665 | `				pTm->tm_sec,` |
|    44 |  666 | `				pTm->tm_gmtoff < 0 ? '-' : '+',` |
|    44 |  667 | `				(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  668 | `				);` |
|    45 |  669 | `			break;` |
|     - |  670 | `				 }` |
|    19 |  671 | `		case 'U':` |
|     - |  672 | `			/* Seconds since the Unix Epoch FOR THIS Sytm (php: the timestamp` |
|     - |  673 | `			 * being formatted — pre-fix this printed time(0) regardless of the` |
|     - |  674 | `			 * date under format). */` |
|    59 |  675 | `			ph7_result_string_format(pCtx,"%qd",(sxi64)(` |
|    38 |  676 | `				(sxu64)DtDaysFromCivil(pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400u` |
|    57 |  677 | `				+ (sxu64)((sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60` |
|    38 |  678 | `				          + (sxi64)pTm->tm_sec - (sxi64)pTm->tm_gmtoff)));` |
|    40 |  679 | `			break;` |
|    48 |  680 | `		case 'O':{` |
|     - |  681 | `			/* Difference to GMT without colon: +0530 (php) */` |
|    97 |  682 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   145 |  683 | `			ph7_result_string_format(pCtx,"%c%02d%02d",` |
|    96 |  684 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    97 |  685 | `			break;` |
|     - |  686 | `				 }` |
|   378 |  687 | `		case 'P':{` |
|     - |  688 | `			/* Difference to GMT with colon: +05:30 (php) */` |
|   757 |  689 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|  1135 |  690 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|   756 |  691 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|   757 |  692 | `			break;` |
|     - |  693 | `				 }` |
|    47 |  694 | `		case 'p':{` |
|     - |  695 | `			/* Like P, but "Z" for UTC (php 8.0+). Two rules PHL had wrong, both` |
|     - |  696 | `			 * visible only once a zone can carry SECONDS or be an abbreviation:` |
|     - |  697 | `			 * php decides on what P would have PRINTED rather than on the raw` |
|     - |  698 | `			 * offset, so "+00:00:59" prints Z and "-00:00:59" prints "-00:00";` |
|     - |  699 | `			 * and the GMT abbreviation is php's one exception -- it prints` |
|     - |  700 | `			 * "+00:00" where the UTC and Z spellings of the same instant print Z. */` |
|     - |  701 | `			long a;` |
|    94 |  702 | `			if( pTm->tm_gmtoff >= 0 && pTm->tm_gmtoff < 60` |
|    59 |  703 | `			 && !(pTm->tm_zone && SyStrncmp(pTm->tm_zone,"GMT",3) == 0` |
|    14 |  704 | `			      && pTm->tm_zone[3] == 0) ){` |
|    23 |  705 | `				ph7_result_string(pCtx,"Z",1);` |
|    23 |  706 | `				break;` |
|     - |  707 | `			}` |
|    73 |  708 | `			a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   109 |  709 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|    72 |  710 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    73 |  711 | `			break;` |
|     - |  712 | `				 }` |
|     2 |  713 | `		case 'Z':` |
|     - |  714 | `			/* Timezone offset in seconds, plain integer (php) */` |
|     5 |  715 | `			ph7_result_string_format(pCtx,"%d",(int)pTm->tm_gmtoff);` |
|     5 |  716 | `			break;` |
|    38 |  717 | `		case 'c':{` |
|     - |  718 | `			/* 	ISO 8601 date: 2004-02-12T15:19:21+00:00 (php) */` |
|    77 |  719 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     - |  720 | `			/* Same four-column zero pad as 'r' (php: "0050-…", "-001-…"). */` |
|   115 |  721 | `			ph7_result_string_format(pCtx,"%04qd-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",` |
|    38 |  722 | `				pTm->tm_year,` |
|    76 |  723 | `				pTm->tm_mon+1,` |
|    38 |  724 | `				pTm->tm_mday,` |
|    38 |  725 | `				pTm->tm_hour,` |
|    38 |  726 | `				pTm->tm_min,` |
|    38 |  727 | `				pTm->tm_sec,` |
|    76 |  728 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  729 | `				);` |
|    77 |  730 | `			break;` |
|     - |  731 | `				 }` |
|     4 |  732 | `		case '\\':` |
|     9 |  733 | `			zIn++;` |
|     - |  734 | `			/* Expand verbatim */` |
|     9 |  735 | `			if( zIn < zEnd ){` |
|     9 |  736 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     4 |  737 | `			}` |
|     9 |  738 | `			break;` |
|  6086 |  739 | `		default:` |
|     - |  740 | `			/* Unknown format specifer,expand verbatim */` |
| 12174 |  741 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
| 12172 |  742 | `			break;` |
|     - |  743 | `		}` |
|     - |  744 | `		/* Point to the next character */` |
| 27961 |  745 | `		zIn++;` |
|     3 |  746 | `	}` |
|  3611 |  747 | `	return SXRET_OK;` |
|     3 |  748 | `}` |
|     - |  749 | `/*` |
|     - |  750 | ` * Resolve a date()/gmdate() $timestamp argument under php 8's ?int weak ZPP:` |
|     - |  751 | ` *   - null            -> *pbUseNow = 1 (caller uses the current time)` |
|     - |  752 | ` *   - int/bool/float  -> coerce to a Unix timestamp (float truncates; php's` |
|     - |  753 | ` *                        float->int precision E_DEPRECATED is not emitted, §3.7)` |
|     - |  754 | ` *   - numeric string  -> coerce via php's is_numeric_string grammar` |
|     - |  755 | ` *                        (RangeStrToNumber: " 100 "/"1e3"/".5"/"+5" ok)` |
|     - |  756 | ` *   - anything else (non-numeric string, array, object, resource)` |
|     - |  757 | ` *                     -> catchable TypeError, byte-exact with php.` |
|     - |  758 | ` * Returns PH7_OK with *pbUseNow / *pT set, or the PH7_VmThrowException status.` |
|     - |  759 | ` */` |
|   474 |  760 | `static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)` |
|     1 |  761 | `{` |
|     - |  762 | `	char zBuf[64];` |
|   475 |  763 | `	*pbUseNow = 0;` |
|   475 |  764 | `	if( ph7_value_is_null(pArg) ){` |
|     3 |  765 | `		*pbUseNow = 1;` |
|     3 |  766 | `		return PH7_OK;` |
|     - |  767 | `	}` |
|   473 |  768 | `	if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_float(pArg) ){` |
|   465 |  769 | `		*pT = (time_t)ph7_value_to_int64(pArg);` |
|   465 |  770 | `		return PH7_OK;` |
|     - |  771 | `	}` |
|     9 |  772 | `	if( ph7_value_is_string(pArg) ){` |
|     - |  773 | `		int nStr;` |
|     9 |  774 | `		const char *zStr = ph7_value_to_string(pArg,&nStr);` |
|     - |  775 | `		sxi64 iLong; double dReal;` |
|     9 |  776 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|     9 |  777 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|     3 |  778 | `			*pT = (time_t)dReal;` |
|     6 |  779 | `			return PH7_OK;` |
|     - |  780 | `		}` |
|     7 |  781 | `		if( iKind == RANGE_IN_LONG ){` |
|     7 |  782 | `			*pT = (time_t)iLong;` |
|     7 |  783 | `			return PH7_OK;` |
|     - |  784 | `		}` |
|     - |  785 | `		/* Not a numeric string: fall through to the TypeError. */` |
|   ! 0 |  786 | `	}` |
|   ! 0 |  787 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  788 | `		"%s(): Argument #2 ($timestamp) must be of type ?int, %s given",` |
|   ! 0 |  789 | `		ph7_function_name(pCtx),VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|   238 |  790 | `}` |
|     - |  791 | `/*` |
|     - |  792 | ` * string date(string $format [, int $timestamp = time() ] )` |
|     - |  793 | ` *  Returns a string formatted according to the given format string using` |
|     - |  794 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|     - |  795 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|     - |  796 | ` * Parameters` |
|     - |  797 | ` *  $format` |
|     - |  798 | ` *   The format of the outputted date string (See code above)` |
|     - |  799 | ` * $timestamp` |
|     - |  800 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - |  801 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - |  802 | ` *   In other words, it defaults to the value of time().` |
|     - |  803 | ` * Return` |
|     - |  804 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - |  805 | ` */` |
|   414 |  806 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  807 | `{` |
|     - |  808 | `	const char *zFormat;` |
|     - |  809 | `	int nLen;` |
|     - |  810 | `	Sytm sTm;` |
|   415 |  811 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - |  812 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 |  813 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  814 | `		return PH7_OK;` |
|     - |  815 | `	}` |
|   415 |  816 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   415 |  817 | `	if( nLen < 1 ){` |
|     - |  818 | `		/* Don't bother processing return the empty string */` |
|   ! 0 |  819 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  820 | `	}` |
|   415 |  821 | `	if( nArg < 2 ){` |
|     - |  822 | `		time_t t;` |
|    35 |  823 | `		time(&t);` |
|    35 |  824 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|    18 |  825 | `	}else{` |
|     - |  826 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|   381 |  827 | `		time_t t = 0;` |
|     - |  828 | `		int bUseNow;` |
|   381 |  829 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|   381 |  830 | `		if( rc != PH7_OK ){` |
|   ! 0 |  831 | `			return rc;` |
|     - |  832 | `		}` |
|   381 |  833 | `		if( bUseNow ){` |
|   ! 0 |  834 | `			time(&t);` |
|   ! 0 |  835 | `		}` |
|   381 |  836 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     - |  837 | `	}` |
|     - |  838 | `	/* Format the given string */` |
|   415 |  839 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|   415 |  840 | `	return PH7_OK;` |
|   208 |  841 | `}` |
|     - |  842 | `/*` |
|     - |  843 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|     - |  844 | ` *  Identical to the date() function except that the time returned` |
|     - |  845 | ` *  is Greenwich Mean Time (GMT).` |
|     - |  846 | ` * Parameters` |
|     - |  847 | ` *  $format` |
|     - |  848 | ` *  The format of the outputted date string (See code above)` |
|     - |  849 | ` *  $timestamp` |
|     - |  850 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - |  851 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - |  852 | ` *   In other words, it defaults to the value of time().` |
|     - |  853 | ` * Return` |
|     - |  854 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - |  855 | ` */` |
|   108 |  856 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  857 | `{` |
|     - |  858 | `	const char *zFormat;` |
|     - |  859 | `	int nLen;` |
|     - |  860 | `	Sytm sTm;` |
|   109 |  861 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - |  862 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 |  863 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  864 | `		return PH7_OK;` |
|     - |  865 | `	}` |
|   109 |  866 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   109 |  867 | `	if( nLen < 1 ){` |
|     - |  868 | `		/* Don't bother processing return the empty string */` |
|   ! 0 |  869 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  870 | `	}` |
|   109 |  871 | `	if( nArg < 2 ){` |
|     - |  872 | `		time_t t;` |
|    15 |  873 | `		time(&t);` |
|    15 |  874 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     8 |  875 | `	}else{` |
|     - |  876 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|    95 |  877 | `		time_t t = 0;` |
|     - |  878 | `		int bUseNow;` |
|    95 |  879 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|    95 |  880 | `		if( rc != PH7_OK ){` |
|   ! 0 |  881 | `			return rc;` |
|     - |  882 | `		}` |
|    95 |  883 | `		if( bUseNow ){` |
|     3 |  884 | `			time(&t);` |
|     1 |  885 | `		}` |
|    95 |  886 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     - |  887 | `	}` |
|     - |  888 | `	/* Format the given string */` |
|   109 |  889 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|   109 |  890 | `	return PH7_OK;` |
|    55 |  891 | `}` |
|     - |  892 | `/*` |
|     - |  893 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|     - |  894 | ` *  Return the local time.` |
|     - |  895 | ` * Parameter` |
|     - |  896 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - |  897 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - |  898 | ` *     In other words, it defaults to the value of time().` |
|     - |  899 | ` * $is_associative` |
|     - |  900 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|     - |  901 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|     - |  902 | ` *   array containing all the different elements of the structure returned by the C function` |
|     - |  903 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|     - |  904 | ` *      "tm_sec" - seconds, 0 to 59` |
|     - |  905 | ` *      "tm_min" - minutes, 0 to 59` |
|     - |  906 | ` *      "tm_hour" - hours, 0 to 23` |
|     - |  907 | ` *      "tm_mday" - day of the month, 1 to 31` |
|     - |  908 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|     - |  909 | ` *      "tm_year" - years since 1900` |
|     - |  910 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|     - |  911 | ` *      "tm_yday" - day of the year, 0 to 365` |
|     - |  912 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|     - |  913 | ` * Returns` |
|     - |  914 | ` *  An associative array of information related to the timestamp.` |
|     - |  915 | ` */` |
|    16 |  916 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  917 | `{` |
|     - |  918 | `	ph7_value *pValue,*pArray;` |
|    17 |  919 | `	int isAssoc = 0;` |
|     - |  920 | `	Sytm sTm;` |
|    17 |  921 | `	if( nArg < 1 ){` |
|     - |  922 | `		time_t t;` |
|     5 |  923 | `		time(&t);` |
|     5 |  924 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     3 |  925 | `	}else{` |
|     - |  926 | `		/* Use the given timestamp */` |
|     - |  927 | `		time_t t;` |
|    13 |  928 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    13 |  929 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|     7 |  930 | `		}else{` |
|   ! 0 |  931 | `			time(&t);` |
|     - |  932 | `		}` |
|    13 |  933 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     - |  934 | `	}` |
|     - |  935 | `	/* Element value */` |
|    17 |  936 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    17 |  937 | `	if( pValue == 0 ){` |
|     - |  938 | `		/* Return NULL */` |
|   ! 0 |  939 | `		ph7_result_null(pCtx);` |
|   ! 0 |  940 | `		return PH7_OK;` |
|     - |  941 | `	}` |
|     - |  942 | `	/* Create a new array */` |
|    17 |  943 | `	pArray = ph7_context_new_array(pCtx);` |
|    17 |  944 | `	if( pArray == 0 ){` |
|     - |  945 | `		/* Return NULL */` |
|   ! 0 |  946 | `		ph7_result_null(pCtx);` |
|   ! 0 |  947 | `		return PH7_OK;` |
|     - |  948 | `	}` |
|    17 |  949 | `	if( nArg > 1 ){` |
|    11 |  950 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|     5 |  951 | `	}` |
|     - |  952 | `	/* Fill the array */` |
|     - |  953 | `	/* Seconds */` |
|    17 |  954 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    17 |  955 | `	if( isAssoc ){` |
|    11 |  956 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|     6 |  957 | `	}else{` |
|     7 |  958 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - |  959 | `	}` |
|     - |  960 | `	/* Minutes */` |
|    17 |  961 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    17 |  962 | `	if( isAssoc ){` |
|    11 |  963 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|     6 |  964 | `	}else{` |
|     7 |  965 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - |  966 | `	}` |
|     - |  967 | `	/* Hours */` |
|    17 |  968 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    17 |  969 | `	if( isAssoc ){` |
|    11 |  970 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|     6 |  971 | `	}else{` |
|     7 |  972 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - |  973 | `	}` |
|     - |  974 | `	/* mday */` |
|    17 |  975 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    17 |  976 | `	if( isAssoc ){` |
|    11 |  977 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|     6 |  978 | `	}else{` |
|     7 |  979 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - |  980 | `	}` |
|     - |  981 | `	/* mon */` |
|    17 |  982 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|    17 |  983 | `	if( isAssoc ){` |
|    11 |  984 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|     6 |  985 | `	}else{` |
|     7 |  986 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - |  987 | `	}` |
|     - |  988 | `	/* year since 1900 */` |
|    17 |  989 | `	ph7_value_int64(pValue,sTm.tm_year-1900);` |
|    17 |  990 | `	if( isAssoc ){` |
|    11 |  991 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|     6 |  992 | `	}else{` |
|     7 |  993 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - |  994 | `	}` |
|     - |  995 | `	/* wday */` |
|    17 |  996 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    17 |  997 | `	if( isAssoc ){` |
|    11 |  998 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|     6 |  999 | `	}else{` |
|     7 | 1000 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1001 | `	}` |
|     - | 1002 | `	/* yday */` |
|    17 | 1003 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    17 | 1004 | `	if( isAssoc ){` |
|    11 | 1005 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|     6 | 1006 | `	}else{` |
|     7 | 1007 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1008 | `	}` |
|     - | 1009 | `	/* isdst */` |
|     - | 1010 | `#ifdef __WINNT__` |
|     - | 1011 | `#ifdef _MSC_VER` |
|     - | 1012 | `#ifndef _WIN32_WCE` |
|     1 | 1013 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1014 | `#endif` |
|     - | 1015 | `#endif` |
|     - | 1016 | `#endif` |
|    17 | 1017 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|    17 | 1018 | `	if( isAssoc ){` |
|    11 | 1019 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|     6 | 1020 | `	}else{` |
|     7 | 1021 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1022 | `	}` |
|     - | 1023 | `	/* Return the array */` |
|    17 | 1024 | `	ph7_result_value(pCtx,pArray);` |
|    17 | 1025 | `	return PH7_OK;` |
|     9 | 1026 | `}` |
|     - | 1027 | `/*` |
|     - | 1028 | ` * int idate(string $format [, int $timestamp = time() ])` |
|     - | 1029 | ` *  Returns a number formatted according to the given format string` |
|     - | 1030 | ` *  using the given integer timestamp or the current local time if` |
|     - | 1031 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|     - | 1032 | ` *  to the value of time().` |
|     - | 1033 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|     - | 1034 | ` *  parameter.` |
|     - | 1035 | ` * $Parameters` |
|     - | 1036 | ` *  Supported format` |
|     - | 1037 | ` *   d 	Day of the month` |
|     - | 1038 | ` *   h 	Hour (12 hour format)` |
|     - | 1039 | ` *   H 	Hour (24 hour format)` |
|     - | 1040 | ` *   i 	Minutes` |
|     - | 1041 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|     - | 1042 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|     - | 1043 | ` *   m 	Month number` |
|     - | 1044 | ` *   s 	Seconds` |
|     - | 1045 | ` *   t 	Days in current month` |
|     - | 1046 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|     - | 1047 | ` *   w 	Day of the week (0 on Sunday)` |
|     - | 1048 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|     - | 1049 | ` *   y 	Year (1 or 2 digits - check note below)` |
|     - | 1050 | ` *   Y 	Year (4 digits)` |
|     - | 1051 | ` *   z 	Day of the year` |
|     - | 1052 | ` *   Z 	Timezone offset in seconds` |
|     - | 1053 | ` * $timestamp` |
|     - | 1054 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|     - | 1055 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|     - | 1056 | ` *  to the value of time().` |
|     - | 1057 | ` * Return` |
|     - | 1058 | ` *  An integer.` |
|     - | 1059 | ` */` |
|   196 | 1060 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1061 | `{` |
|     - | 1062 | `	const char *zFormat;` |
|   198 | 1063 | `	ph7_int64 iVal = 0;` |
|     - | 1064 | `	int nLen;` |
|     - | 1065 | `	Sytm sTm;` |
|   198 | 1066 | `	time_t t = 0; /* The resolved timestamp; 'U' must report THIS, not time(0) */` |
|   198 | 1067 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1068 | `		/* Missing/Invalid argument,return -1 */` |
|   ! 0 | 1069 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1070 | `		return PH7_OK;` |
|     - | 1071 | `	}` |
|   198 | 1072 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   198 | 1073 | `	if( nLen < 1 ){` |
|     - | 1074 | `		/* Don't bother processing return -1*/` |
|   ! 0 | 1075 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1076 | `	}` |
|   198 | 1077 | `	if( nArg < 2 ){` |
|    16 | 1078 | `		time(&t);` |
|    16 | 1079 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     9 | 1080 | `	}else{` |
|     - | 1081 | `		/* Use the given timestamp */` |
|   183 | 1082 | `		if( ph7_value_is_int(apArg[1]) ){` |
|   183 | 1083 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    92 | 1084 | `		}else{` |
|   ! 0 | 1085 | `			time(&t);` |
|     - | 1086 | `		}` |
|   183 | 1087 | `		DtSytmOfTimestamp((sxi64)t,&sTm);` |
|     - | 1088 | `	}` |
|     - | 1089 | `	/* Perform the requested operation */` |
|   198 | 1090 | `	switch(zFormat[0]){` |
|     9 | 1091 | `	case 'd':` |
|     - | 1092 | `	case 'j':` |
|     - | 1093 | `		/* Day of the month ('j' differs from 'd' only in zero padding, which an` |
|     - | 1094 | `		 * integer result cannot carry) */` |
|    19 | 1095 | `		iVal = sTm.tm_mday;` |
|    19 | 1096 | `		break;` |
|     8 | 1097 | `	case 'h':` |
|     - | 1098 | `	case 'g':` |
|     - | 1099 | `		/* Hour (12 hour format): php reports midnight and noon as 12, not 0 —` |
|     - | 1100 | ``		 * `1 + hour % 12` answered 1 for both. */`` |
|    17 | 1101 | `		iVal = sTm.tm_hour % 12;` |
|    17 | 1102 | `		if( iVal == 0 ){` |
|    17 | 1103 | `			iVal = 12;` |
|     8 | 1104 | `		}` |
|    17 | 1105 | `		break;` |
|     9 | 1106 | `	case 'H':` |
|     - | 1107 | `	case 'G':` |
|     - | 1108 | `		/* Hour (24 hour format) */` |
|    19 | 1109 | `		iVal = sTm.tm_hour;` |
|    19 | 1110 | `		break;` |
|     5 | 1111 | `	case 'B': {` |
|     - | 1112 | `		/* Swatch Internet time: 1000 "beats" per day in UTC+1, no fractions.` |
|     - | 1113 | `		 * Integer math throughout so the tiny build (no floating point) agrees.` |
|     - | 1114 | ``		 * Read the time OF DAY off the broken-down clock: `t + 3600` overflows`` |
|     - | 1115 | `		 * at the top of php's timestamp range, which is undefined and answered` |
|     - | 1116 | `		 * the wrong beat. */` |
|    16 | 1117 | `		ph7_int64 iSec = ((ph7_int64)sTm.tm_hour*3600 + sTm.tm_min*60 + sTm.tm_sec` |
|    10 | 1118 | `			- sTm.tm_gmtoff + 3600) % 86400;` |
|    11 | 1119 | `		if( iSec < 0 ){` |
|   ! 0 | 1120 | `			iSec += 86400;` |
|   ! 0 | 1121 | `		}` |
|    11 | 1122 | `		iVal = iSec * 1000 / 86400;` |
|    11 | 1123 | `		break;` |
|     - | 1124 | `			  }` |
|     5 | 1125 | `	case 'i':` |
|     - | 1126 | `		/*Minutes*/` |
|    11 | 1127 | `		iVal = sTm.tm_min;` |
|    11 | 1128 | `		break;` |
|   ! 0 | 1129 | `	case 'I':` |
|     - | 1130 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|     - | 1131 | `#ifdef __WINNT__` |
|     - | 1132 | `#ifdef _MSC_VER` |
|     - | 1133 | `#ifndef _WIN32_WCE` |
|   ! 0 | 1134 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1135 | `#endif` |
|     - | 1136 | `#endif` |
|     - | 1137 | `#endif` |
|   ! 0 | 1138 | `		iVal = sTm.tm_isdst;` |
|   ! 0 | 1139 | `		break;` |
|     4 | 1140 | `	case 'L':` |
|     - | 1141 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|     9 | 1142 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|     9 | 1143 | `		break;` |
|     9 | 1144 | `	case 'm':` |
|     - | 1145 | `	case 'n':` |
|     - | 1146 | `		/* Month number. Sytm keeps tm_mon 0-based (see 't' below, which tests` |
|     - | 1147 | ``		 * `tm_mon == 1` for February), so July used to answer 6. */`` |
|    19 | 1148 | `		iVal = sTm.tm_mon + 1;` |
|    19 | 1149 | `		break;` |
|     5 | 1150 | `	case 's':` |
|     - | 1151 | `		/*Seconds*/` |
|    11 | 1152 | `		iVal = sTm.tm_sec;` |
|    11 | 1153 | `		break;` |
|     4 | 1154 | `	case 't':{` |
|     - | 1155 | `		/*Days in current month*/` |
|     - | 1156 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|     9 | 1157 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|     9 | 1158 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|   ! 0 | 1159 | `			nDays = 28;` |
|   ! 0 | 1160 | `		}` |
|     9 | 1161 | `		iVal = nDays;` |
|     9 | 1162 | `		break;` |
|     - | 1163 | `			 }` |
|     8 | 1164 | `	case 'U':` |
|     - | 1165 | `		/* Seconds since the Unix Epoch. This used to call time(0), ignoring the` |
|     - | 1166 | `		 * $timestamp argument entirely and always answering "now". */` |
|    17 | 1167 | `		iVal = (ph7_int64)t;` |
|    17 | 1168 | `		break;` |
|     4 | 1169 | `	case 'w':` |
|     - | 1170 | `		/*	Day of the week (0 on Sunday) */` |
|     9 | 1171 | `		iVal = sTm.tm_wday;` |
|     9 | 1172 | `		break;` |
|     8 | 1173 | `	case 'W':` |
|     - | 1174 | `	case 'o': {` |
|     - | 1175 | `		/* ISO-8601 week number / week-numbering year: both belong to the year` |
|     - | 1176 | `		 * owning the Thursday of the civil week, so 2021-01-01 is 2020-W53.` |
|     - | 1177 | `		 * The old code indexed a weekday table and returned a DAY number` |
|     - | 1178 | `		 * (1..7) as if it were a week number — idate("W") answered 4 in the` |
|     - | 1179 | `		 * middle of July. Same derivation as date()'s 'o'/'W' above. */` |
|    17 | 1180 | `		sxi64 days = DtDaysFromCivil(sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|    17 | 1181 | `		int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|    17 | 1182 | `		sxi64 thu = days + (4 - isoDow);` |
|     - | 1183 | `		sxi64 wy;` |
|     - | 1184 | `		int wm,wd;` |
|    17 | 1185 | `		DtCivilFromDays(thu,&wy,&wm,&wd);` |
|    17 | 1186 | `		if( zFormat[0] == 'o' ){` |
|     9 | 1187 | `			iVal = (ph7_int64)wy;` |
|     5 | 1188 | `		}else{` |
|     9 | 1189 | `			iVal = (ph7_int64)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1;` |
|     - | 1190 | `		}` |
|    17 | 1191 | `		break;` |
|     - | 1192 | `			  }` |
|     4 | 1193 | `	case 'y':` |
|     - | 1194 | `		/* Year (2 digits) */` |
|     9 | 1195 | `		iVal = sTm.tm_year % 100;` |
|     9 | 1196 | `		break;` |
|    10 | 1197 | `	case 'Y':` |
|     - | 1198 | `		/* Year (4 digits) */` |
|    21 | 1199 | `		iVal = sTm.tm_year;` |
|    21 | 1200 | `		break;` |
|     4 | 1201 | `	case 'z':` |
|     - | 1202 | `		/* Day of the year */` |
|     9 | 1203 | `		iVal = sTm.tm_yday;` |
|     9 | 1204 | `		break;` |
|   ! 0 | 1205 | `	case 'Z':` |
|     - | 1206 | `		/*Timezone offset in seconds*/` |
|   ! 0 | 1207 | `		iVal = sTm.tm_gmtoff;` |
|   ! 0 | 1208 | `		break;` |
|     2 | 1209 | `	default:` |
|     - | 1210 | `		/* Unknown token: php's own -1, which the shared tail below reports. */` |
|     6 | 1211 | `		iVal = -1;` |
|     4 | 1212 | `		break;` |
|     - | 1213 | `	}` |
|     - | 1214 | ``	/* php's idate answers a C `int`, so every token is narrowed to one -- and`` |
|     - | 1215 | `	 * -1 is the single value it uses for "no answer": it warns about the TOKEN` |
|     - | 1216 | `	 * and answers FALSE, whatever produced the -1. That is why` |
|     - | 1217 | ``	 * `idate('U', PHP_INT_MAX)` is a false with an "Unrecognized date format`` |
|     - | 1218 | `` 	 * token" warning in front of it rather than the timestamp. `idate() === false` `` |
|     - | 1219 | `	 * is the documented check, and 0 is a legitimate answer for several tokens. */` |
|   198 | 1220 | `	if( (int)iVal == -1 ){` |
|     8 | 1221 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unrecognized date format token");` |
|     8 | 1222 | `		ph7_result_bool(pCtx,0);` |
|     5 | 1223 | `	}else{` |
|   191 | 1224 | `		ph7_result_int64(pCtx,(int)iVal);` |
|     - | 1225 | `	}` |
|   198 | 1226 | `	return PH7_OK;` |
|   100 | 1227 | `}` |
|     - | 1228 | `/*` |
|     - | 1229 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|     - | 1230 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|     - | 1231 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|     - | 1232 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|     - | 1233 | ` *  specified.` |
|     - | 1234 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|     - | 1235 | ` *  the current value according to the local date and time.` |
|     - | 1236 | ` * Parameters` |
|     - | 1237 | ` * $hour` |
|     - | 1238 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|     - | 1239 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|     - | 1240 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|     - | 1241 | ` * $minute` |
|     - | 1242 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|     - | 1243 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|     - | 1244 | ` *  in the following hour(s).` |
|     - | 1245 | ` * $second` |
|     - | 1246 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|     - | 1247 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|     - | 1248 | ` * second in the following minute(s).` |
|     - | 1249 | ` * $month` |
|     - | 1250 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|     - | 1251 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|     - | 1252 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|     - | 1253 | ` * $day` |
|     - | 1254 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|     - | 1255 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|     - | 1256 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|     - | 1257 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|     - | 1258 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|     - | 1259 | ` * $year` |
|     - | 1260 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|     - | 1261 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|     - | 1262 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|     - | 1263 | ` * $is_dst` |
|     - | 1264 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|     - | 1265 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|     - | 1266 | ` * Return` |
|     - | 1267 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|     - | 1268 | ` *   If the arguments are invalid, the function returns FALSE` |
|     - | 1269 | ` */` |
|  4564 | 1270 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1271 | `{` |
|     - | 1272 | `	const char *zFunction;` |
|     - | 1273 | `	ph7_int64 iVal;` |
|     - | 1274 | `	sxi64 h,mi,s,mo,d,y,yAdj;` |
|     - | 1275 | `	int moN;` |
|     - | 1276 | `	struct tm *pTm;` |
|     - | 1277 | `	time_t t;` |
|     - | 1278 | `	/* Extract function name */` |
|  4565 | 1279 | `	zFunction = ph7_function_name(pCtx);` |
|     - | 1280 | `	/* PHP 8 dropped the legacy $is_dst 7th parameter: mktime()/gmmktime() now` |
|     - | 1281 | `	 * accept at most 6 arguments and throw a catchable ArgumentCountError` |
|     - | 1282 | `	 * otherwise (the central aBuiltinArity table only enforces the minimum, so` |
|     - | 1283 | `	 * this maximum is checked here). */` |
|  4565 | 1284 | `	if( nArg > 6 ){` |
|   ! 0 | 1285 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1286 | `			"%s() expects at most 6 arguments, %d given",zFunction,nArg);` |
|     - | 1287 | `	}` |
|  4565 | 1288 | `	if( nArg < 1 ){` |
|   ! 0 | 1289 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1290 | `			"%s() expects at least 1 argument, 0 given",zFunction);` |
|     - | 1291 | `	}` |
|     - | 1292 | `	/* Missing components default from the current time in php's default` |
|     - | 1293 | `	 * timezone. PHL's date_default_timezone_set() only accepts UTC/GMT (no tz` |
|     - | 1294 | `	 * database), so mktime() and gmmktime() agree and both read gmtime(). */` |
|  4565 | 1295 | `	time(&t);` |
|  4565 | 1296 | `	pTm = gmtime(&t);` |
|  2282 | 1297 | `	SXUNUSED(zFunction);` |
|  4565 | 1298 | `	h  = pTm->tm_hour;` |
|  4565 | 1299 | `	mi = pTm->tm_min;` |
|  4565 | 1300 | `	s  = pTm->tm_sec;` |
|  4565 | 1301 | `	mo = pTm->tm_mon + 1;` |
|  4565 | 1302 | `	d  = pTm->tm_mday;` |
|  4565 | 1303 | `	y  = pTm->tm_year + 1900;` |
|  4565 | 1304 | `	h = ph7_value_to_int64(apArg[0]);` |
|  4565 | 1305 | `	if( nArg > 1 ){` |
|  4565 | 1306 | `		mi = ph7_value_to_int64(apArg[1]);` |
|  4565 | 1307 | `		if( nArg > 2 ){` |
|  4565 | 1308 | `			s = ph7_value_to_int64(apArg[2]);` |
|  4565 | 1309 | `			if( nArg > 3 ){` |
|  4565 | 1310 | `				mo = ph7_value_to_int64(apArg[3]);` |
|  4565 | 1311 | `				if( nArg > 4 ){` |
|  4565 | 1312 | `					d = ph7_value_to_int64(apArg[4]);` |
|  4565 | 1313 | `					if( nArg > 5 ){` |
|     - | 1314 | `						/* php's legacy two-digit mapping: 0-69 -> 2000-2069,` |
|     - | 1315 | `						 * 70-100 -> 1970-2000; anything else is verbatim */` |
|  4565 | 1316 | `						y = ph7_value_to_int64(apArg[5]);` |
|  4565 | 1317 | `						if( y >= 0 && y <= 69 ){` |
|     7 | 1318 | `							y += 2000;` |
|  4562 | 1319 | `						}else if( y >= 70 && y <= 100 ){` |
|     5 | 1320 | `							y += 1900;` |
|     2 | 1321 | `						}` |
|  2282 | 1322 | `					}` |
|  2282 | 1323 | `				}` |
|  2282 | 1324 | `			}` |
|  2282 | 1325 | `		}` |
|  2282 | 1326 | `	}` |
|     - | 1327 | `	/* Normalize the month with floor semantics, then let day/time components` |
|     - | 1328 | `	 * overflow linearly (php: mktime(25,-30,0,1,1,2024) == Jan 2 00:30). */` |
|  4565 | 1329 | `	yAdj = y + DtFloorDiv(mo - 1,12);` |
|  4565 | 1330 | `	moN  = (int)(mo - 1 - DtFloorDiv(mo - 1,12) * 12) + 1;` |
|  4565 | 1331 | `	iVal = (DtDaysFromCivil(yAdj,moN,1) + (d - 1)) * 86400 + h*3600 + mi*60 + s;` |
|     - | 1332 | `	/* Return the timestamp as a 64bit integer */` |
|  4565 | 1333 | `	ph7_result_int64(pCtx,iVal);` |
|  4565 | 1334 | `	return PH7_OK;` |
|  2283 | 1335 | `}` |
|     - | 1336 | `/*` |
|     - | 1337 | ` * string date_default_timezone_get(void)` |
|     - | 1338 | ` *  Gets the default timezone used by all date/time functions in a script.` |
|     - | 1339 | ` */` |
|     6 | 1340 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1341 | `{` |
|     7 | 1342 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 | 1343 | `	SXUNUSED(nArg);` |
|     3 | 1344 | `	SXUNUSED(apArg);` |
|     7 | 1345 | `	ph7_result_string(pCtx,pVm->zDefTz,(int)pVm->nDefTz);` |
|     7 | 1346 | `	return PH7_OK;` |
|     1 | 1347 | `}` |
|     - | 1348 | `/*` |
|     - | 1349 | ` * bool date_default_timezone_set(string $timezoneId)` |
|     - | 1350 | ` *  Sets the default timezone used by all date/time functions in a script.` |
|     - | 1351 | ` *  php validates against the tz database and stores the id verbatim (get()` |
|     - | 1352 | ` *  echoes back "utc" if that's what was set). PHL ships no tz database, so` |
|     - | 1353 | ` *  only UTC and GMT are accepted; every other id — including region names php` |
|     - | 1354 | ` *  would accept — is rejected with php's invalid-id notice (recorded scope cut).` |
|     - | 1355 | ` */` |
|   146 | 1356 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1357 | `{` |
|   149 | 1358 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1359 | `	const char *zId;` |
|     - | 1360 | `	int nId;` |
|   149 | 1361 | `	if( nArg < 1 ){` |
|   ! 0 | 1362 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1363 | `		return PH7_OK;` |
|     - | 1364 | `	}` |
|   149 | 1365 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|   149 | 1366 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|   149 | 1367 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|   149 | 1368 | `		pVm->zDefTz[3] = 0;` |
|   149 | 1369 | `		pVm->nDefTz = 3;` |
|   149 | 1370 | `		ph7_result_bool(pCtx,1);` |
|   149 | 1371 | `		return PH7_OK;` |
|     - | 1372 | `	}` |
|     - | 1373 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|     - | 1374 | `	 * — exactly php's notice shape here */` |
|   ! 0 | 1375 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|   ! 0 | 1376 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 1377 | `	return PH7_OK;` |
|    76 | 1378 | `}` |
|     - | 1379 |  |
|     - | 1380 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 1381 |  |
