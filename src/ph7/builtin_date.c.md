# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1384/1797 lines (77.02%)

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
|     - |   15 | `static sxi64 DtDaysFromCivil(sxi64 y,int m,int d);` |
|     - |   16 | `static void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd);` |
|     - |   17 | `static sxi64 DtFloorDiv(sxi64 a,sxi64 b);` |
|     - |   18 | `/*` |
|     - |   19 | ` * STRUCT_TM_TO_SYTM zeroes tm_gmtoff (struct tm carries it only as a BSD/glibc` |
|     - |   20 | ` * extension, absent on newlib/ESP32). Derive the zone offset portably from the` |
|     - |   21 | ` * broken-down civil fields and the timestamp they came from: for localtime()` |
|     - |   22 | ` * fills this yields the local UTC offset, for gmtime() fills it yields 0.` |
|     - |   23 | ` */` |
|   148 |   24 | `static void DtSytmFillOffset(Sytm *pSTm,time_t t)` |
|     1 |   25 | `{` |
|   223 |   26 | `	sxi64 iCivil = DtDaysFromCivil((sxi64)pSTm->tm_year,pSTm->tm_mon+1,pSTm->tm_mday) * 86400` |
|   148 |   27 | `		+ (sxi64)pSTm->tm_hour*3600 + (sxi64)pSTm->tm_min*60 + (sxi64)pSTm->tm_sec;` |
|   149 |   28 | `	pSTm->tm_gmtoff = (long)(iCivil - (sxi64)t);` |
|   149 |   29 | `}` |
|     - |   30 | `#ifdef __WINNT__` |
|     - |   31 | `#ifdef _MSC_VER` |
|     - |   32 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|     - |   33 | `#pragma warning(disable:4996) /* _CRT_SECURE_NO_WARNINGS */` |
|     - |   34 | `#endif` |
|     - |   35 | `#endif` |
|     - |   36 | `#endif` |
|     - |   37 | `#ifdef __WINNT__` |
|     - |   38 | `/* GetSystemTime() */` |
|     - |   39 | `#include <Windows.h>` |
|     - |   40 | `#ifdef _WIN32_WCE` |
|     - |   41 | `/* SPDX-SnippetBegin */` |
|     - |   42 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|     - |   43 | `/* SPDX-License-Identifier: blessing */` |
|     - |   44 | `/*` |
|     - |   45 | `** WindowsCE does not have a localtime() function.  So create a` |
|     - |   46 | `** substitute.` |
|     - |   47 | `** Taken from the SQLite3 source tree.` |
|     - |   48 | `** Status: Public domain` |
|     - |   49 | `*/` |
|     - |   50 | `struct tm *__cdecl localtime(const time_t *t)` |
|     - |   51 | `{` |
|     - |   52 | `  static struct tm y;` |
|     - |   53 | `  FILETIME uTm, lTm;` |
|     - |   54 | `  SYSTEMTIME pTm;` |
|     - |   55 | `  ph7_int64 t64;` |
|     - |   56 | `  t64 = *t;` |
|     - |   57 | `  t64 = (t64 + 11644473600)*10000000;` |
|     - |   58 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|     - |   59 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|     - |   60 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|     - |   61 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|     - |   62 | `  y.tm_year = pTm.wYear - 1900;` |
|     - |   63 | `  y.tm_mon = pTm.wMonth - 1;` |
|     - |   64 | `  y.tm_wday = pTm.wDayOfWeek;` |
|     - |   65 | `  y.tm_mday = pTm.wDay;` |
|     - |   66 | `  y.tm_hour = pTm.wHour;` |
|     - |   67 | `  y.tm_min = pTm.wMinute;` |
|     - |   68 | `  y.tm_sec = pTm.wSecond;` |
|     - |   69 | `  return &y;` |
|     - |   70 | `}` |
|     - |   71 | `/* SPDX-SnippetEnd */` |
|     - |   72 | `#endif /*_WIN32_WCE */` |
|     - |   73 | `#elif defined(__UNIXES__)` |
|     - |   74 | `#include <sys/time.h>` |
|     - |   75 | `#endif /* __WINNT__*/` |
|     - |   76 | `/*` |
|     - |   77 | ` * Resolve the current wall-clock time (epoch seconds + sub-second microseconds).` |
|     - |   78 | ` *` |
|     - |   79 | ` * An embedder may override the platform clock via PH7_CONFIG_CLOCK (e.g. the` |
|     - |   80 | ` * ESP32 port routes this through esp_timer); when no hook is registered we use` |
|     - |   81 | ` * gettimeofday() on Unix and fall back to a second-resolution time() elsewhere.` |
|     - |   82 | ` * Centralising this here gives microtime()/gettimeofday() a single sub-second` |
|     - |   83 | `` * source instead of the old nonsensical `tt % SX_USEC_PER_SEC` off-Unix path.`` |
|     - |   84 | ` */` |
|    38 |   85 | `static void DateNow(ph7_vm *pVm,sytime *pOut)` |
|     1 |   86 | `{` |
|    39 |   87 | `	if( pVm && pVm->pEngine->xConf.xClock ){` |
|   ! 0 |   88 | `		ph7_int64 sec = 0,usec = 0;` |
|   ! 0 |   89 | `		if( pVm->pEngine->xConf.xClock(pVm->pEngine->xConf.pClockData,&sec,&usec) == PH7_OK ){` |
|   ! 0 |   90 | `			pOut->tm_sec  = (long)sec;` |
|   ! 0 |   91 | `			pOut->tm_usec = (long)usec;` |
|   ! 0 |   92 | `			return;` |
|     - |   93 | `		}` |
|   ! 0 |   94 | `	}` |
|     - |   95 | `#if defined(__UNIXES__)` |
|     - |   96 | `	{` |
|     - |   97 | `		struct timeval tv;` |
|    38 |   98 | `		gettimeofday(&tv,0);` |
|    38 |   99 | `		pOut->tm_sec  = (long)tv.tv_sec;` |
|    38 |  100 | `		pOut->tm_usec = (long)tv.tv_usec;` |
|     - |  101 | `	}` |
|     - |  102 | `#elif defined(__WINNT__)` |
|     - |  103 | `	{` |
|     - |  104 | `		/* FILETIME is 100-ns ticks since 1601-01-01 UTC; convert to the Unix` |
|     - |  105 | `		 * epoch with microsecond resolution (GetSystemTime() only carries` |
|     - |  106 | `		 * milliseconds, and time() has no sub-second part at all). */` |
|     - |  107 | `		FILETIME ft;` |
|     - |  108 | `		ph7_int64 t;` |
|     1 |  109 | `		GetSystemTimeAsFileTime(&ft);` |
|     1 |  110 | `		t  = (ph7_int64)ft.dwHighDateTime << 32;` |
|     1 |  111 | `		t += ft.dwLowDateTime;` |
|     1 |  112 | `		t -= 116444736000000000LL; /* 100-ns ticks between 1601 and 1970 */` |
|     1 |  113 | `		pOut->tm_sec  = (long)(t / 10000000);` |
|     1 |  114 | `		pOut->tm_usec = (long)((t % 10000000) / 10);` |
|     - |  115 | `	}` |
|     - |  116 | `#else` |
|     - |  117 | `	{` |
|     - |  118 | `		time_t tt;` |
|     - |  119 | `		time(&tt);` |
|     - |  120 | `		pOut->tm_sec  = (long)tt;` |
|     - |  121 | `		pOut->tm_usec = 0; /* no sub-second source; embedders supply one via PH7_CONFIG_CLOCK */` |
|     - |  122 | `	}` |
|     - |  123 | `#endif /* __UNIXES__ */` |
|    20 |  124 | `}` |
|     - |  125 | ` /*` |
|     - |  126 | `  * int64 time(void)` |
|     - |  127 | `  *  Current Unix timestamp` |
|     - |  128 | `  * Parameters` |
|     - |  129 | `  *  None.` |
|     - |  130 | `  * Return` |
|     - |  131 | `  *  Returns the current time measured in the number of seconds` |
|     - |  132 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|     - |  133 | `  */` |
|     8 |  134 | `PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  135 | `{` |
|     - |  136 | `	time_t tt;` |
|     4 |  137 | `	SXUNUSED(nArg); /* cc warning */` |
|     4 |  138 | `	SXUNUSED(apArg);` |
|     - |  139 | `	/* Extract the current time */` |
|     9 |  140 | `	time(&tt);` |
|     - |  141 | `	/* Return as 64-bit integer */` |
|     9 |  142 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|     9 |  143 | `	return  PH7_OK;` |
|     1 |  144 | `}` |
|     - |  145 | `/*` |
|     - |  146 | `  * string/float microtime([ bool $get_as_float = false ])` |
|     - |  147 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|     - |  148 | `  * Parameters` |
|     - |  149 | `  *  $get_as_float` |
|     - |  150 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|     - |  151 | `  *   as described in the return values section below.` |
|     - |  152 | `  * Return` |
|     - |  153 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|     - |  154 | `  *  is the current time measured in the number of seconds since the Unix` |
|     - |  155 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|     - |  156 | `  *  that have elapsed since sec expressed in seconds.` |
|     - |  157 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|     - |  158 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|     - |  159 | `  */` |
|    30 |  160 | `PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  161 | `{` |
|    31 |  162 | `	int bFloat = 0;` |
|     - |  163 | `	sytime sTime;` |
|    31 |  164 | `	DateNow(pCtx->pVm,&sTime);` |
|    31 |  165 | `	if( nArg > 0 ){` |
|    25 |  166 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|    12 |  167 | `	}` |
|    31 |  168 | `	if( bFloat ){` |
|     - |  169 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|    25 |  170 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|    13 |  171 | `	}else{` |
|     - |  172 | `		/* Return PHP's "msec sec" form: the sub-second part as fractional` |
|     - |  173 | `		 * seconds to 8 decimals, e.g. "0.50667100 1700000000". tm_usec is in` |
|     - |  174 | `		 * microseconds (0..999999), so scaling by 100 yields the 8-digit` |
|     - |  175 | `		 * fraction — matching PHP's "%.8F" output exactly. */` |
|     7 |  176 | `		ph7_result_string_format(pCtx,"0.%08ld %ld",sTime.tm_usec*100,sTime.tm_sec);` |
|     - |  177 | `	}` |
|    31 |  178 | `	return PH7_OK;` |
|     1 |  179 | `}` |
|     - |  180 | `/*` |
|     - |  181 | ` * array\|int hrtime(bool $as_number = false)` |
|     - |  182 | ` *  The system's high-resolution time, counted from an arbitrary monotonic` |
|     - |  183 | ` *  point in nanoseconds. Returns [seconds, nanoseconds] by default, or the` |
|     - |  184 | ` *  total nanoseconds as an int when $as_number is true.` |
|     - |  185 | ` */` |
|     8 |  186 | `PH7_PRIVATE int PH7_builtin_hrtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  187 | `{` |
|     9 |  188 | `	ph7_int64 sec = 0,nsec = 0;` |
|     9 |  189 | `	int bAsNumber = 0;` |
|     9 |  190 | `	if( nArg > 0 ){` |
|     7 |  191 | `		bAsNumber = ph7_value_to_bool(apArg[0]);` |
|     3 |  192 | `	}` |
|     - |  193 | `#if defined(CLOCK_MONOTONIC)` |
|     - |  194 | `	{` |
|     - |  195 | `		struct timespec ts;` |
|     8 |  196 | `		if( clock_gettime(CLOCK_MONOTONIC,&ts) == 0 ){` |
|     8 |  197 | `			sec  = (ph7_int64)ts.tv_sec;` |
|     8 |  198 | `			nsec = (ph7_int64)ts.tv_nsec;` |
|     4 |  199 | `		}` |
|     - |  200 | `	}` |
|     - |  201 | `#else` |
|     - |  202 | `	{` |
|     - |  203 | `		/* No monotonic clock available: fall back to the wall-clock microsecond` |
|     - |  204 | `		 * source (embedder clock / gettimeofday). Coarser and not strictly` |
|     - |  205 | `		 * monotonic, but keeps hrtime() usable off-Unix. */` |
|     - |  206 | `		sytime sTime;` |
|     1 |  207 | `		DateNow(pCtx->pVm,&sTime);` |
|     1 |  208 | `		sec  = (ph7_int64)sTime.tm_sec;` |
|     1 |  209 | `		nsec = (ph7_int64)sTime.tm_usec * 1000;` |
|     - |  210 | `	}` |
|     - |  211 | `#endif` |
|     9 |  212 | `	if( bAsNumber ){` |
|     7 |  213 | `		ph7_result_int64(pCtx,sec * 1000000000LL + nsec);` |
|     4 |  214 | `	}else{` |
|     - |  215 | `		ph7_value *pValue,*pArray;` |
|     3 |  216 | `		pArray = ph7_context_new_array(pCtx);` |
|     3 |  217 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     3 |  218 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 |  219 | `			ph7_result_null(pCtx);` |
|   ! 0 |  220 | `			return PH7_OK;` |
|     - |  221 | `		}` |
|     3 |  222 | `		ph7_value_int64(pValue,sec);` |
|     3 |  223 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     3 |  224 | `		ph7_value_int64(pValue,nsec);` |
|     3 |  225 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     3 |  226 | `		ph7_result_value(pCtx,pArray);` |
|     3 |  227 | `		ph7_context_release_value(pCtx,pValue);` |
|     3 |  228 | `		ph7_context_release_value(pCtx,pArray);` |
|     - |  229 | `	}` |
|     9 |  230 | `	return PH7_OK;` |
|     5 |  231 | `}` |
|     - |  232 | `/*` |
|     - |  233 | ` * array getdate ([ int $timestamp = time() ])` |
|     - |  234 | ` *  Returns an associative array containing the date information` |
|     - |  235 | ` *  of the timestamp, or the current local time if no timestamp is given.` |
|     - |  236 | ` * Parameter` |
|     - |  237 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - |  238 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - |  239 | ` *     In other words, it defaults to the value of time().` |
|     - |  240 | ` * Returns` |
|     - |  241 | ` *  Returns an associative array of information related to the timestamp.` |
|     - |  242 | ` */` |
|     8 |  243 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  244 | `{` |
|     - |  245 | `	ph7_value *pValue,*pArray;` |
|     - |  246 | `	Sytm sTm;` |
|     9 |  247 | `	if( nArg < 1 ){` |
|     - |  248 | `#ifdef __WINNT__` |
|     - |  249 | `		SYSTEMTIME sOS;` |
|     1 |  250 | `		GetSystemTime(&sOS);` |
|     1 |  251 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - |  252 | `#else` |
|     - |  253 | `		struct tm *pTm;` |
|     - |  254 | `		time_t t;` |
|     4 |  255 | `		time(&t);` |
|     4 |  256 | `		pTm = gmtime(&t);` |
|     4 |  257 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     4 |  258 | `		DtSytmFillOffset(&sTm,t);` |
|     - |  259 | `#endif` |
|     3 |  260 | `	}else{` |
|     - |  261 | `		/* Use the given timestamp */` |
|     - |  262 | `		time_t t;` |
|     - |  263 | `		struct tm *pTm;` |
|     5 |  264 | `		if( ph7_value_is_int(apArg[0]) ){` |
|     5 |  265 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|     5 |  266 | `			pTm = gmtime(&t);` |
|     5 |  267 | `			if( pTm == 0 ){` |
|   ! 0 |  268 | `				time(&t);` |
|   ! 0 |  269 | `			}` |
|     3 |  270 | `		}else{` |
|   ! 0 |  271 | `			time(&t);` |
|     - |  272 | `		}` |
|     5 |  273 | `		pTm = gmtime(&t);` |
|     5 |  274 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     5 |  275 | `		DtSytmFillOffset(&sTm,t);` |
|     - |  276 | `	}` |
|     - |  277 | `	/* Element value */` |
|     9 |  278 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     9 |  279 | `	if( pValue == 0 ){` |
|     - |  280 | `		/* Return NULL */` |
|   ! 0 |  281 | `		ph7_result_null(pCtx);` |
|   ! 0 |  282 | `		return PH7_OK;` |
|     - |  283 | `	}` |
|     - |  284 | `	/* Create a new array */` |
|     9 |  285 | `	pArray = ph7_context_new_array(pCtx);` |
|     9 |  286 | `	if( pArray == 0 ){` |
|     - |  287 | `		/* Return NULL */` |
|   ! 0 |  288 | `		ph7_result_null(pCtx);` |
|   ! 0 |  289 | `		return PH7_OK;` |
|     - |  290 | `	}` |
|     - |  291 | `	/* Fill the array */` |
|     - |  292 | `	/* Seconds */` |
|     9 |  293 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|     9 |  294 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|     - |  295 | `	/* Minutes */` |
|     9 |  296 | `	ph7_value_int(pValue,sTm.tm_min);` |
|     9 |  297 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|     - |  298 | `	/* Hours */` |
|     9 |  299 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|     9 |  300 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|     - |  301 | `	/* mday */` |
|     9 |  302 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|     9 |  303 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|     - |  304 | `	/* wday */` |
|     9 |  305 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|     9 |  306 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|     - |  307 | `	/* mon */` |
|     9 |  308 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|     9 |  309 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|     - |  310 | `	/* year */` |
|     9 |  311 | `	ph7_value_int(pValue,sTm.tm_year);` |
|     9 |  312 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|     - |  313 | `	/* yday */` |
|     9 |  314 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|     9 |  315 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|     - |  316 | `	/* Weekday [i.e: Monday,Tuesday,...] */` |
|     9 |  317 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|     9 |  318 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|     - |  319 | `	/* Reset the string cursor */` |
|     9 |  320 | `	ph7_value_reset_string_cursor(pValue);` |
|     - |  321 | `	/* Month [i.e: January,February,...] */` |
|     9 |  322 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|     9 |  323 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|     - |  324 | `	/* Return the freshly created array */` |
|     9 |  325 | `	ph7_result_value(pCtx,pArray);` |
|     9 |  326 | `	return PH7_OK;` |
|     5 |  327 | `}` |
|     - |  328 | `/*` |
|     - |  329 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|     - |  330 | ` *  Returns an associative array containing the data returned from the system call.` |
|     - |  331 | ` * Parameters` |
|     - |  332 | ` *  $return_float` |
|     - |  333 | ` *   When set to TRUE, a float instead of an array is returned.` |
|     - |  334 | ` * Return` |
|     - |  335 | ` *  By default an array is returned. If return_float is set, then` |
|     - |  336 | ` *  a float is returned.` |
|     - |  337 | ` */` |
|     8 |  338 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  339 | `{` |
|     9 |  340 | `	int bFloat = 0;` |
|     - |  341 | `	sytime sTime;` |
|     9 |  342 | `	DateNow(pCtx->pVm,&sTime);` |
|     9 |  343 | `	if( nArg > 0 ){` |
|     7 |  344 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|     3 |  345 | `	}` |
|     9 |  346 | `	if( bFloat ){` |
|     - |  347 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|     5 |  348 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|     3 |  349 | `	}else{` |
|     - |  350 | `		/* Return an associative array */` |
|     - |  351 | `		ph7_value *pValue,*pArray;` |
|     - |  352 | `		/* Create a new array */` |
|     5 |  353 | `		pArray = ph7_context_new_array(pCtx);` |
|     - |  354 | `		/* Element value */` |
|     5 |  355 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     5 |  356 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|     - |  357 | `			/* Return NULL */` |
|   ! 0 |  358 | `			ph7_result_null(pCtx);` |
|   ! 0 |  359 | `			return PH7_OK;` |
|     - |  360 | `		}` |
|     - |  361 | `		/* Fill the array */` |
|     - |  362 | `		/* sec */` |
|     5 |  363 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|     5 |  364 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|     - |  365 | `		/* usec */` |
|     5 |  366 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|     5 |  367 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|     - |  368 | `		/* Return the array */` |
|     5 |  369 | `		ph7_result_value(pCtx,pArray);` |
|     - |  370 | `	}` |
|     9 |  371 | `	return PH7_OK;` |
|     5 |  372 | `}` |
|     - |  373 | `/* Check if the given year is leap or not */` |
|     - |  374 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|     - |  375 | `/* ISO-8601 numeric representation of the day of the week */` |
|     - |  376 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|     - |  377 | `/*` |
|     - |  378 | ` * Format a given date string.` |
|     - |  379 | ` * Supported format: (Taken from PHP online docs)` |
|     - |  380 | ` * character 	Description` |
|     - |  381 | ` * d          Day of the month, 2 digits with leading zeros` |
|     - |  382 | ` * D          A textual representation of a day, three letters` |
|     - |  383 | ` * j          Day of the month without leading zeros` |
|     - |  384 | ` * l          A full textual representation of the day of the week` |
|     - |  385 | ` * N          ISO-8601 numeric representation of the day of the week` |
|     - |  386 | ` * w          Numeric representation of the day of the week` |
|     - |  387 | ` * z          The day of the year (starting from 0)` |
|     - |  388 | ` * F          A full textual representation of a month, such as January or March` |
|     - |  389 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|     - |  390 | ` * M          A short textual representation of a month, three letters` |
|     - |  391 | ` * n          Numeric representation of a month, without leading zeros` |
|     - |  392 | ` * t          Number of days in the given month` |
|     - |  393 | ` * L          Whether it's a leap year` |
|     - |  394 | ` * o          ISO-8601 year number. This has the same value as Y` |
|     - |  395 | ` * Y          A full numeric representation of a year, 4 digits` |
|     - |  396 | ` * y          A two digit representation of a year` |
|     - |  397 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|     - |  398 | ` * A          Uppercase Ante meridiem and Post meridiem` |
|     - |  399 | ` * g          12-hour format of an hour without leading zeros` |
|     - |  400 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|     - |  401 | ` * h          12-hour format of an hour with leading zeros` |
|     - |  402 | ` * H          24-hour format of an hour with leading zeros` |
|     - |  403 | ` * i          Minutes with leading zeros` |
|     - |  404 | ` * s          Seconds, with leading zeros` |
|     - |  405 | ` * u          Microseconds` |
|     - |  406 | ` * e          Timezone identifier` |
|     - |  407 | ` * I          Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|     - |  408 | ` * r          RFC 2822 formatted date` |
|     - |  409 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|     - |  410 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|     - |  411 | ` * O          Difference to Greenwich time (GMT) in hours` |
|     - |  412 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|     - |  413 | ` *            east of UTC is always positive.` |
|     - |  414 | ` * c         ISO 8601 date` |
|     - |  415 | ` */` |
|   206 |  416 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|     1 |  417 | `{` |
|   207 |  418 | `	const char *zEnd = &zIn[nLen];` |
|     - |  419 | `	const char *zCur;` |
|     - |  420 | `	/* Start the format process */` |
|   608 |  421 | `	for(;;){` |
|  1217 |  422 | `		if( zIn >= zEnd ){` |
|     - |  423 | `			/* No more input to process */` |
|   207 |  424 | `			break;` |
|     - |  425 | `		}` |
|  1011 |  426 | `		switch(zIn[0]){` |
|    68 |  427 | `		case 'd':` |
|     - |  428 | `			/* Day of the month, 2 digits with leading zeros */` |
|   137 |  429 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|   137 |  430 | `			break;` |
|   ! 0 |  431 | `		case 'D':` |
|     - |  432 | `			/*A textual representation of a day, three letters*/` |
|   ! 0 |  433 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|   ! 0 |  434 | `			ph7_result_string(pCtx,zCur,3);` |
|   ! 0 |  435 | `			break;` |
|     1 |  436 | `		case 'j':` |
|     - |  437 | `			/*	Day of the month without leading zeros */` |
|     3 |  438 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|     3 |  439 | `			break;` |
|     2 |  440 | `		case 'l':` |
|     - |  441 | `			/* A full textual representation of the day of the week */` |
|     5 |  442 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|     5 |  443 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     5 |  444 | `			break;` |
|   ! 0 |  445 | `		case 'N':{` |
|     - |  446 | `			/* ISO-8601 numeric representation of the day of the week */` |
|   ! 0 |  447 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|   ! 0 |  448 | `			break;` |
|     - |  449 | `				 }` |
|   ! 0 |  450 | `		case 'w':` |
|     - |  451 | `			/*Numeric representation of the day of the week*/` |
|   ! 0 |  452 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|   ! 0 |  453 | `			break;` |
|   ! 0 |  454 | `		case 'z':` |
|     - |  455 | `			/*The day of the year*/` |
|   ! 0 |  456 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|   ! 0 |  457 | `			break;` |
|     2 |  458 | `		case 'F':` |
|     - |  459 | `			/*A full textual representation of a month, such as January or March*/` |
|     5 |  460 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|     5 |  461 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     5 |  462 | `			break;` |
|    68 |  463 | `		case 'm':` |
|     - |  464 | `			/*Numeric representation of a month, with leading zeros*/` |
|   137 |  465 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|   137 |  466 | `			break;` |
|   ! 0 |  467 | `		case 'M':` |
|     - |  468 | `			/*A short textual representation of a month, three letters*/` |
|   ! 0 |  469 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|   ! 0 |  470 | `			ph7_result_string(pCtx,zCur,3);` |
|   ! 0 |  471 | `			break;` |
|     1 |  472 | `		case 'n':` |
|     - |  473 | `			/*Numeric representation of a month, without leading zeros*/` |
|     3 |  474 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|     3 |  475 | `			break;` |
|   ! 0 |  476 | `		case 't':{` |
|     - |  477 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|   ! 0 |  478 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|   ! 0 |  479 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|   ! 0 |  480 | `				nDays = 28;` |
|   ! 0 |  481 | `			}` |
|     - |  482 | `			/*Number of days in the given month*/` |
|   ! 0 |  483 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|   ! 0 |  484 | `			break;` |
|     - |  485 | `				 }` |
|   ! 0 |  486 | `		case 'L':{` |
|   ! 0 |  487 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|     - |  488 | `			/* Whether it's a leap year */` |
|   ! 0 |  489 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|   ! 0 |  490 | `			break;` |
|     - |  491 | `				 }` |
|   ! 0 |  492 | `		case 'o': case 'W': {` |
|     - |  493 | `			/* ISO-8601 week-numbering year / week number: both belong to the` |
|     - |  494 | `			 * year owning the Thursday of the civil week (php: 2024-12-31 is` |
|     - |  495 | `			 * 2025-W01, 2027-01-01 is 2026-W53). php pads W but not o. */` |
|   ! 0 |  496 | `			sxi64 days = DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday);` |
|   ! 0 |  497 | `			int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|   ! 0 |  498 | `			sxi64 thu = days + (4 - isoDow);` |
|     - |  499 | `			sxi64 wy;` |
|     - |  500 | `			int wm,wd;` |
|   ! 0 |  501 | `			DtCivilFromDays(thu,&wy,&wm,&wd);` |
|   ! 0 |  502 | `			if( zIn[0] == 'o' ){` |
|   ! 0 |  503 | `				ph7_result_string_format(pCtx,"%d",(int)wy);` |
|   ! 0 |  504 | `			}else{` |
|   ! 0 |  505 | `				ph7_result_string_format(pCtx,"%02d",` |
|   ! 0 |  506 | `					(int)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1);` |
|     - |  507 | `			}` |
|   ! 0 |  508 | `			break;` |
|     - |  509 | `				 }` |
|    55 |  510 | `		case 'Y':` |
|     - |  511 | `			/*	A full numeric representation of a year, 4 digits */` |
|   111 |  512 | `			ph7_result_string_format(pCtx,"%04d",pTm->tm_year);` |
|   111 |  513 | `			break;` |
|   ! 0 |  514 | `		case 'X':` |
|     - |  515 | `			/* Expanded full year, always signed (php 8.2+): +2024 */` |
|   ! 0 |  516 | `			ph7_result_string_format(pCtx,"%c%04d",` |
|   ! 0 |  517 | `				pTm->tm_year < 0 ? '-' : '+',` |
|   ! 0 |  518 | `				pTm->tm_year < 0 ? -pTm->tm_year : pTm->tm_year);` |
|   ! 0 |  519 | `			break;` |
|   ! 0 |  520 | `		case 'x':` |
|     - |  521 | `			/* Expanded year, signed only past 4 digits (php 8.2+) */` |
|   ! 0 |  522 | `			if( pTm->tm_year > 9999 ){` |
|   ! 0 |  523 | `				ph7_result_string_format(pCtx,"+%d",pTm->tm_year);` |
|   ! 0 |  524 | `			}else{` |
|   ! 0 |  525 | `				ph7_result_string_format(pCtx,"%04d",pTm->tm_year);` |
|     - |  526 | `			}` |
|   ! 0 |  527 | `			break;` |
|   ! 0 |  528 | `		case 'y':` |
|     - |  529 | `			/*A two digit representation of a year*/` |
|   ! 0 |  530 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|   ! 0 |  531 | `			break;` |
|   ! 0 |  532 | `		case 'a':` |
|     - |  533 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|   ! 0 |  534 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "pm" : "am",2);` |
|   ! 0 |  535 | `			break;` |
|   ! 0 |  536 | `		case 'A':` |
|     - |  537 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|   ! 0 |  538 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "PM" : "AM",2);` |
|   ! 0 |  539 | `			break;` |
|   ! 0 |  540 | `		case 'B':{` |
|     - |  541 | `			/* Swatch Internet time: thousandths of the UTC+1 day */` |
|   ! 0 |  542 | `			sxi64 iUtc = DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400` |
|   ! 0 |  543 | `				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec` |
|   ! 0 |  544 | `				- (sxi64)pTm->tm_gmtoff;` |
|   ! 0 |  545 | `			sxi64 iBie = (iUtc + 3600) % 86400;` |
|   ! 0 |  546 | `			if( iBie < 0 ){` |
|   ! 0 |  547 | `				iBie += 86400;` |
|   ! 0 |  548 | `			}` |
|   ! 0 |  549 | `			ph7_result_string_format(pCtx,"%03d",(int)(iBie * 1000 / 86400));` |
|   ! 0 |  550 | `			break;` |
|     - |  551 | `				 }` |
|   ! 0 |  552 | `		case 'g':` |
|     - |  553 | `			/*	12-hour format of an hour without leading zeros*/` |
|   ! 0 |  554 | `			ph7_result_string_format(pCtx,"%d",` |
|   ! 0 |  555 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|   ! 0 |  556 | `			break;` |
|     1 |  557 | `		case 'G':` |
|     - |  558 | `			/* 24-hour format of an hour without leading zeros */` |
|     3 |  559 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|     3 |  560 | `			break;` |
|   ! 0 |  561 | `		case 'h':` |
|     - |  562 | `			/* 12-hour format of an hour with leading zeros */` |
|   ! 0 |  563 | `			ph7_result_string_format(pCtx,"%02d",` |
|   ! 0 |  564 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|   ! 0 |  565 | `			break;` |
|    28 |  566 | `		case 'H':` |
|     - |  567 | `			/*	24-hour format of an hour with leading zeros */` |
|    57 |  568 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|    57 |  569 | `			break;` |
|    29 |  570 | `		case 'i':` |
|     - |  571 | `			/* 	Minutes with leading zeros */` |
|    59 |  572 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|    59 |  573 | `			break;` |
|    29 |  574 | `		case 's':` |
|     - |  575 | `			/* 	second with leading zeros */` |
|    59 |  576 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    59 |  577 | `			break;` |
|   ! 0 |  578 | `		case 'u':` |
|     - |  579 | `			/* 	Microseconds (whole-second timestamps only: php pads zeros) */` |
|   ! 0 |  580 | `			ph7_result_string(pCtx,"000000",6);` |
|   ! 0 |  581 | `			break;` |
|   ! 0 |  582 | `		case 'v':` |
|     - |  583 | `			/* 	Milliseconds (same) */` |
|   ! 0 |  584 | `			ph7_result_string(pCtx,"000",3);` |
|   ! 0 |  585 | `			break;` |
|   ! 0 |  586 | `		case 'S':{` |
|     - |  587 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|     - |  588 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|   ! 0 |  589 | `			int v = pTm->tm_mday;` |
|   ! 0 |  590 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|   ! 0 |  591 | `			break;` |
|     - |  592 | `				 }` |
|     7 |  593 | `		case 'e':` |
|     - |  594 | `			/* 	Timezone identifier */` |
|    15 |  595 | `			zCur = pTm->tm_zone;` |
|    15 |  596 | `			if( zCur == 0 ){` |
|     - |  597 | `				/* date()-family fills: the script default timezone */` |
|     5 |  598 | `				zCur = pCtx->pVm->zDefTz;` |
|     2 |  599 | `			}` |
|    15 |  600 | `			ph7_result_string(pCtx,zCur,-1);` |
|    15 |  601 | `			break;` |
|     2 |  602 | `		case 'T':{` |
|     - |  603 | `			/* Timezone abbreviation: "UTC" for offset 0, "GMT+0530" for a` |
|     - |  604 | `			 * fixed offset (php's shape). PHL has no tz database, so the` |
|     - |  605 | `			 * zone-name path only ever sees UTC/GMT, uppercased. */` |
|     - |  606 | `			const char *z;` |
|     5 |  607 | `			if( pTm->tm_gmtoff != 0 ){` |
|   ! 0 |  608 | `				long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   ! 0 |  609 | `				ph7_result_string_format(pCtx,"GMT%c%02d%02d",` |
|   ! 0 |  610 | `					pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|   ! 0 |  611 | `				break;` |
|     - |  612 | `			}` |
|     5 |  613 | `			z = pTm->tm_zone ? pTm->tm_zone : pCtx->pVm->zDefTz;` |
|    17 |  614 | `			while( *z ){` |
|    13 |  615 | `				int c = (unsigned char)*z;` |
|    13 |  616 | `				if( c >= 'a' && c <= 'z' ){` |
|   ! 0 |  617 | `					c -= 'a' - 'A';` |
|   ! 0 |  618 | `				}` |
|    13 |  619 | `				ph7_result_string_format(pCtx,"%c",c);` |
|    13 |  620 | `				z++;` |
|     1 |  621 | `			}` |
|     5 |  622 | `			break;` |
|     - |  623 | `				 }` |
|   ! 0 |  624 | `		case 'I':` |
|     - |  625 | `			/* Whether or not the date is in daylight saving time */` |
|     - |  626 | `#ifdef __WINNT__` |
|     - |  627 | `#ifdef _MSC_VER` |
|     - |  628 | `#ifndef _WIN32_WCE` |
|   ! 0 |  629 | `			_get_daylight(&pTm->tm_isdst);` |
|     - |  630 | `#endif` |
|     - |  631 | `#endif` |
|     - |  632 | `#endif` |
|   ! 0 |  633 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|   ! 0 |  634 | `			break;` |
|   ! 0 |  635 | `		case 'r':{` |
|     - |  636 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200 */` |
|   ! 0 |  637 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   ! 0 |  638 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d %c%02d%02d",` |
|   ! 0 |  639 | `				SyTimeGetDay(pTm->tm_wday),` |
|   ! 0 |  640 | `				pTm->tm_mday,` |
|   ! 0 |  641 | `				SyTimeGetMonth(pTm->tm_mon),` |
|   ! 0 |  642 | `				pTm->tm_year,` |
|   ! 0 |  643 | `				pTm->tm_hour,` |
|   ! 0 |  644 | `				pTm->tm_min,` |
|   ! 0 |  645 | `				pTm->tm_sec,` |
|   ! 0 |  646 | `				pTm->tm_gmtoff < 0 ? '-' : '+',` |
|   ! 0 |  647 | `				(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  648 | `				);` |
|   ! 0 |  649 | `			break;` |
|     - |  650 | `				 }` |
|     2 |  651 | `		case 'U':` |
|     - |  652 | `			/* Seconds since the Unix Epoch FOR THIS Sytm (php: the timestamp` |
|     - |  653 | `			 * being formatted — pre-fix this printed time(0) regardless of the` |
|     - |  654 | `			 * date under format). */` |
|     7 |  655 | `			ph7_result_string_format(pCtx,"%qd",` |
|     4 |  656 | `				DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400` |
|     4 |  657 | `				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec` |
|     4 |  658 | `				- (sxi64)pTm->tm_gmtoff);` |
|     5 |  659 | `			break;` |
|     1 |  660 | `		case 'O':{` |
|     - |  661 | `			/* Difference to GMT without colon: +0530 (php) */` |
|     3 |  662 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     3 |  663 | `			ph7_result_string_format(pCtx,"%c%02d%02d",` |
|     2 |  664 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|     3 |  665 | `			break;` |
|     - |  666 | `				 }` |
|     2 |  667 | `		case 'P':{` |
|     - |  668 | `			/* Difference to GMT with colon: +05:30 (php) */` |
|     5 |  669 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     5 |  670 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|     4 |  671 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|     5 |  672 | `			break;` |
|     - |  673 | `				 }` |
|   ! 0 |  674 | `		case 'p':{` |
|     - |  675 | `			/* Like P, but "Z" for UTC (php 8.0+) */` |
|     - |  676 | `			long a;` |
|   ! 0 |  677 | `			if( pTm->tm_gmtoff == 0 ){` |
|   ! 0 |  678 | `				ph7_result_string(pCtx,"Z",1);` |
|   ! 0 |  679 | `				break;` |
|     - |  680 | `			}` |
|   ! 0 |  681 | `			a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   ! 0 |  682 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|   ! 0 |  683 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|   ! 0 |  684 | `			break;` |
|     - |  685 | `				 }` |
|   ! 0 |  686 | `		case 'Z':` |
|     - |  687 | `			/* Timezone offset in seconds, plain integer (php) */` |
|   ! 0 |  688 | `			ph7_result_string_format(pCtx,"%d",(int)pTm->tm_gmtoff);` |
|   ! 0 |  689 | `			break;` |
|     4 |  690 | `		case 'c':{` |
|     - |  691 | `			/* 	ISO 8601 date: 2004-02-12T15:19:21+00:00 (php) */` |
|     9 |  692 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    13 |  693 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",` |
|     4 |  694 | `				pTm->tm_year,` |
|     8 |  695 | `				pTm->tm_mon+1,` |
|     4 |  696 | `				pTm->tm_mday,` |
|     4 |  697 | `				pTm->tm_hour,` |
|     4 |  698 | `				pTm->tm_min,` |
|     4 |  699 | `				pTm->tm_sec,` |
|     8 |  700 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  701 | `				);` |
|     9 |  702 | `			break;` |
|     - |  703 | `				 }` |
|     1 |  704 | `		case '\\':` |
|     3 |  705 | `			zIn++;` |
|     - |  706 | `			/* Expand verbatim */` |
|     3 |  707 | `			if( zIn < zEnd ){` |
|     3 |  708 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     1 |  709 | `			}` |
|     3 |  710 | `			break;` |
|   202 |  711 | `		default:` |
|     - |  712 | `			/* Unknown format specifer,expand verbatim */` |
|   405 |  713 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|   404 |  714 | `			break;` |
|     - |  715 | `		}` |
|     - |  716 | `		/* Point to the next character */` |
|  1011 |  717 | `		zIn++;` |
|     1 |  718 | `	}` |
|   207 |  719 | `	return SXRET_OK;` |
|     1 |  720 | `}` |
|     - |  721 | `/*` |
|     - |  722 | ` * PH7 implementation of the strftime() function.` |
|     - |  723 | ` * The following formats are supported:` |
|     - |  724 | ` * %a 	An abbreviated textual representation of the day` |
|     - |  725 | ` * %A 	A full textual representation of the day` |
|     - |  726 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|     - |  727 | ` * %e 	Day of the month, with a space preceding single digits.` |
|     - |  728 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|     - |  729 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|     - |  730 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|     - |  731 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|     - |  732 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|     - |  733 | ` *   4 weekdays, with Monday being the start of the week.` |
|     - |  734 | ` * %W 	A numeric representation of the week of the year` |
|     - |  735 | ` * %b 	Abbreviated month name, based on the locale` |
|     - |  736 | ` * %B 	Full month name, based on the locale` |
|     - |  737 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|     - |  738 | ` * %m 	Two digit representation of the month` |
|     - |  739 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|     - |  740 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|     - |  741 | ` * %G 	The full four-digit version of %g` |
|     - |  742 | ` * %y 	Two digit representation of the year` |
|     - |  743 | ` * %Y 	Four digit representation for the year` |
|     - |  744 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|     - |  745 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|     - |  746 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|     - |  747 | ` * %M 	Two digit representation of the minute` |
|     - |  748 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|     - |  749 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|     - |  750 | ` * %r 	Same as "%I:%M:%S %p"` |
|     - |  751 | ` * %R 	Same as "%H:%M"` |
|     - |  752 | ` * %S 	Two digit representation of the second` |
|     - |  753 | ` * %T 	Same as "%H:%M:%S"` |
|     - |  754 | ` * %X 	Preferred time representation based on locale, without the date` |
|     - |  755 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|     - |  756 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|     - |  757 | ` * %c 	Preferred date and time stamp based on local` |
|     - |  758 | ` * %D 	Same as "%m/%d/%y"` |
|     - |  759 | ` * %F 	Same as "%Y-%m-%d"` |
|     - |  760 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|     - |  761 | ` * %x 	Preferred date representation based on locale, without the time` |
|     - |  762 | ` * %n 	A newline character ("\n")` |
|     - |  763 | ` * %t 	A Tab character ("\t")` |
|     - |  764 | ` * %% 	A literal percentage character ("%")` |
|     - |  765 | ` */` |
|    18 |  766 | `static int PH7_Strftime(` |
|     - |  767 | `	ph7_context *pCtx,  /* Call context */` |
|     - |  768 | `	const char *zIn,    /* Input string */` |
|     - |  769 | `	int nLen,           /* Input length */` |
|     - |  770 | `	Sytm *pTm           /* Parse of the given time */` |
|     - |  771 | `	)` |
|     1 |  772 | `{` |
|    19 |  773 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|     - |  774 | `	int c;` |
|     - |  775 | `	/* Start the format process */` |
|    20 |  776 | `	for(;;){` |
|    41 |  777 | `		zCur = zIn;` |
|    45 |  778 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|     5 |  779 | `			zIn++;` |
|     1 |  780 | `		}` |
|    41 |  781 | `		if( zIn > zCur ){` |
|     - |  782 | `			/* Consume input verbatim */` |
|     5 |  783 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     2 |  784 | `		}` |
|    41 |  785 | `		zIn++; /* Jump the percent sign */` |
|    41 |  786 | `		if( zIn >= zEnd ){` |
|     - |  787 | `			/* No more input to process */` |
|    19 |  788 | `			break;` |
|     - |  789 | `		}` |
|    23 |  790 | `		c = zIn[0];` |
|     - |  791 | `		/* Act according to the current specifer */` |
|    23 |  792 | `		switch(c){` |
|   ! 0 |  793 | `		case '%':` |
|     - |  794 | `			/* A literal percentage character ("%") */` |
|   ! 0 |  795 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|   ! 0 |  796 | `			break;` |
|   ! 0 |  797 | `		case 't':` |
|     - |  798 | `			/* A Tab character */` |
|   ! 0 |  799 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|   ! 0 |  800 | `			break;` |
|   ! 0 |  801 | `		case 'n':` |
|     - |  802 | `			/* A newline character */` |
|   ! 0 |  803 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|   ! 0 |  804 | `			break;` |
|     1 |  805 | `		case 'a':` |
|     - |  806 | `			/* An abbreviated textual representation of the day */` |
|     3 |  807 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|     3 |  808 | `			break;` |
|   ! 0 |  809 | `		case 'A':` |
|     - |  810 | `			/* A full textual representation of the day */` |
|   ! 0 |  811 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|   ! 0 |  812 | `			break;` |
|   ! 0 |  813 | `		case 'e':` |
|     - |  814 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|   ! 0 |  815 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|   ! 0 |  816 | `			break;` |
|     2 |  817 | `		case 'd':` |
|     - |  818 | `			/* Two-digit day of the month (with leading zeros) */` |
|     5 |  819 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|     5 |  820 | `			break;` |
|   ! 0 |  821 | `		case 'j':` |
|     - |  822 | `			/*The day of the year,3 digits with leading zeros*/` |
|   ! 0 |  823 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|   ! 0 |  824 | `			break;` |
|   ! 0 |  825 | `		case 'u':` |
|     - |  826 | `			/* ISO-8601 numeric representation of the day of the week */` |
|   ! 0 |  827 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|   ! 0 |  828 | `			break;` |
|   ! 0 |  829 | `		case 'w':` |
|     - |  830 | `			/* Numeric representation of the day of the week */` |
|   ! 0 |  831 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|   ! 0 |  832 | `			break;` |
|   ! 0 |  833 | `		case 'b':` |
|     - |  834 | `		case 'h':` |
|     - |  835 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|   ! 0 |  836 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|   ! 0 |  837 | `			break;` |
|   ! 0 |  838 | `		case 'B':` |
|     - |  839 | `			/* Full month name (Not based on locale) */` |
|   ! 0 |  840 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|   ! 0 |  841 | `			break;` |
|     2 |  842 | `		case 'm':` |
|     - |  843 | `			/*Numeric representation of a month, with leading zeros*/` |
|     5 |  844 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     5 |  845 | `			break;` |
|   ! 0 |  846 | `		case 'C':` |
|     - |  847 | `			/* Two digit representation of the century */` |
|   ! 0 |  848 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|   ! 0 |  849 | `			break;` |
|   ! 0 |  850 | `		case 'y':` |
|     - |  851 | `		case 'g':` |
|     - |  852 | `			/* Two digit representation of the year */` |
|   ! 0 |  853 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|   ! 0 |  854 | `			break;` |
|     3 |  855 | `		case 'Y':` |
|     - |  856 | `		case 'G':` |
|     - |  857 | `			/* Four digit representation of the year */` |
|     7 |  858 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     7 |  859 | `			break;` |
|   ! 0 |  860 | `		case 'I':` |
|     - |  861 | `			/* 12-hour format of an hour with leading zeros */` |
|   ! 0 |  862 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|   ! 0 |  863 | `			break;` |
|   ! 0 |  864 | `		case 'l':` |
|     - |  865 | `			/* 12-hour format of an hour with leading space */` |
|   ! 0 |  866 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|   ! 0 |  867 | `			break;` |
|     1 |  868 | `		case 'H':` |
|     - |  869 | `			/* 24-hour format of an hour with leading zeros */` |
|     3 |  870 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|     3 |  871 | `			break;` |
|     1 |  872 | `		case 'M':` |
|     - |  873 | `			/* Minutes with leading zeros */` |
|     3 |  874 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|     3 |  875 | `			break;` |
|   ! 0 |  876 | `		case 'S':` |
|     - |  877 | `			/* Seconds with leading zeros */` |
|   ! 0 |  878 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|   ! 0 |  879 | `			break;` |
|   ! 0 |  880 | `		case 'z':` |
|     - |  881 | `		case 'Z':` |
|     - |  882 | `			/* 	Timezone identifier */` |
|   ! 0 |  883 | `			zCur = pTm->tm_zone;` |
|   ! 0 |  884 | `			if( zCur == 0 ){` |
|     - |  885 | `				/* date()-family fills: the script default timezone */` |
|   ! 0 |  886 | `				zCur = pCtx->pVm->zDefTz;` |
|   ! 0 |  887 | `			}` |
|   ! 0 |  888 | `			ph7_result_string(pCtx,zCur,-1);` |
|   ! 0 |  889 | `			break;` |
|   ! 0 |  890 | `		case 'T':` |
|     - |  891 | `		case 'X':` |
|     - |  892 | `			/* Same as "%H:%M:%S" */` |
|   ! 0 |  893 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|   ! 0 |  894 | `			break;` |
|   ! 0 |  895 | `		case 'R':` |
|     - |  896 | `			/* Same as "%H:%M" */` |
|   ! 0 |  897 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|   ! 0 |  898 | `			break;` |
|   ! 0 |  899 | `		case 'P':` |
|     - |  900 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|   ! 0 |  901 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|   ! 0 |  902 | `			break;` |
|   ! 0 |  903 | `		case 'p':` |
|     - |  904 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|   ! 0 |  905 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|   ! 0 |  906 | `			break;` |
|   ! 0 |  907 | `		case 'r':` |
|     - |  908 | `			/* Same as "%I:%M:%S %p" */` |
|   ! 0 |  909 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|   ! 0 |  910 | `				1+(pTm->tm_hour%12),` |
|   ! 0 |  911 | `				pTm->tm_min,` |
|   ! 0 |  912 | `				pTm->tm_sec,` |
|   ! 0 |  913 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|     - |  914 | `				);` |
|   ! 0 |  915 | `			break;` |
|     1 |  916 | `		case 'D':` |
|     - |  917 | `		case 'x':` |
|     - |  918 | `			/* Same as "%m/%d/%y" */` |
|     4 |  919 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|     2 |  920 | `				pTm->tm_mon+1,` |
|     1 |  921 | `				pTm->tm_mday,` |
|     2 |  922 | `				pTm->tm_year%100` |
|     - |  923 | `				);` |
|     3 |  924 | `			break;` |
|   ! 0 |  925 | `		case 'F':` |
|     - |  926 | `			/* Same as "%Y-%m-%d" */` |
|   ! 0 |  927 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|   ! 0 |  928 | `				pTm->tm_year,` |
|   ! 0 |  929 | `				pTm->tm_mon+1,` |
|   ! 0 |  930 | `				pTm->tm_mday` |
|     - |  931 | `				);` |
|   ! 0 |  932 | `			break;` |
|   ! 0 |  933 | `		case 'c':` |
|   ! 0 |  934 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|   ! 0 |  935 | `				pTm->tm_year,` |
|   ! 0 |  936 | `				pTm->tm_mon+1,` |
|   ! 0 |  937 | `				pTm->tm_mday,` |
|   ! 0 |  938 | `				pTm->tm_hour,` |
|   ! 0 |  939 | `				pTm->tm_min,` |
|   ! 0 |  940 | `				pTm->tm_sec` |
|     - |  941 | `				);` |
|   ! 0 |  942 | `			break;` |
|   ! 0 |  943 | `		case 's':{` |
|     - |  944 | `			time_t tt;` |
|     - |  945 | `			/* Seconds since the Unix Epoch */` |
|   ! 0 |  946 | `			time(&tt);` |
|   ! 0 |  947 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|   ! 0 |  948 | `			break;` |
|     - |  949 | `				 }` |
|   ! 0 |  950 | `		default:` |
|     - |  951 | `			/* unknown specifer,simply ignore*/` |
|   ! 0 |  952 | `			break;` |
|     - |  953 | `		}` |
|     - |  954 | `		/* Advance the cursor */` |
|    23 |  955 | `		zIn++;` |
|     1 |  956 | `	}` |
|    19 |  957 | `	return SXRET_OK;` |
|     1 |  958 | `}` |
|     - |  959 | `/*` |
|     - |  960 | ` * Resolve a date()/gmdate() $timestamp argument under php 8's ?int weak ZPP:` |
|     - |  961 | ` *   - null            -> *pbUseNow = 1 (caller uses the current time)` |
|     - |  962 | ` *   - int/bool/float  -> coerce to a Unix timestamp (float truncates; php's` |
|     - |  963 | ` *                        float->int precision E_DEPRECATED is not emitted, §3.7)` |
|     - |  964 | ` *   - numeric string  -> coerce via php's is_numeric_string grammar` |
|     - |  965 | ` *                        (RangeStrToNumber: " 100 "/"1e3"/".5"/"+5" ok)` |
|     - |  966 | ` *   - anything else (non-numeric string, array, object, resource)` |
|     - |  967 | ` *                     -> catchable TypeError, byte-exact with php.` |
|     - |  968 | ` * Returns PH7_OK with *pbUseNow / *pT set, or the PH7_VmThrowException status.` |
|     - |  969 | ` */` |
|    42 |  970 | `static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)` |
|     1 |  971 | `{` |
|     - |  972 | `	char zBuf[64];` |
|    43 |  973 | `	*pbUseNow = 0;` |
|    43 |  974 | `	if( ph7_value_is_null(pArg) ){` |
|     3 |  975 | `		*pbUseNow = 1;` |
|     3 |  976 | `		return PH7_OK;` |
|     - |  977 | `	}` |
|    41 |  978 | `	if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_float(pArg) ){` |
|    23 |  979 | `		*pT = (time_t)ph7_value_to_int64(pArg);` |
|    23 |  980 | `		return PH7_OK;` |
|     - |  981 | `	}` |
|    19 |  982 | `	if( ph7_value_is_string(pArg) ){` |
|     - |  983 | `		int nStr;` |
|    19 |  984 | `		const char *zStr = ph7_value_to_string(pArg,&nStr);` |
|     - |  985 | `		sxi64 iLong; double dReal;` |
|    19 |  986 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|    19 |  987 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|     3 |  988 | `			*pT = (time_t)dReal;` |
|     6 |  989 | `			return PH7_OK;` |
|     - |  990 | `		}` |
|    17 |  991 | `		if( iKind == RANGE_IN_LONG ){` |
|     7 |  992 | `			*pT = (time_t)iLong;` |
|     7 |  993 | `			return PH7_OK;` |
|     - |  994 | `		}` |
|     - |  995 | `		/* Not a numeric string: fall through to the TypeError. */` |
|     5 |  996 | `	}` |
|    16 |  997 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  998 | `		"%s(): Argument #2 ($timestamp) must be of type ?int, %s given",` |
|     5 |  999 | `		ph7_function_name(pCtx),VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|    22 | 1000 | `}` |
|     - | 1001 | `/*` |
|     - | 1002 | ` * string date(string $format [, int $timestamp = time() ] )` |
|     - | 1003 | ` *  Returns a string formatted according to the given format string using` |
|     - | 1004 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|     - | 1005 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|     - | 1006 | ` * Parameters` |
|     - | 1007 | ` *  $format` |
|     - | 1008 | ` *   The format of the outputted date string (See code above)` |
|     - | 1009 | ` * $timestamp` |
|     - | 1010 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1011 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1012 | ` *   In other words, it defaults to the value of time().` |
|     - | 1013 | ` * Return` |
|     - | 1014 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - | 1015 | ` */` |
|    46 | 1016 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1017 | `{` |
|     - | 1018 | `	const char *zFormat;` |
|     - | 1019 | `	int nLen;` |
|     - | 1020 | `	Sytm sTm;` |
|    47 | 1021 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1022 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1023 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1024 | `		return PH7_OK;` |
|     - | 1025 | `	}` |
|    47 | 1026 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    47 | 1027 | `	if( nLen < 1 ){` |
|     - | 1028 | `		/* Don't bother processing return the empty string */` |
|   ! 0 | 1029 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1030 | `	}` |
|    47 | 1031 | `	if( nArg < 2 ){` |
|     - | 1032 | `#ifdef __WINNT__` |
|     - | 1033 | `		SYSTEMTIME sOS;` |
|     1 | 1034 | `		GetSystemTime(&sOS);` |
|     1 | 1035 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1036 | `#else` |
|     - | 1037 | `		struct tm *pTm;` |
|     - | 1038 | `		time_t t;` |
|    30 | 1039 | `		time(&t);` |
|    30 | 1040 | `		pTm = gmtime(&t);` |
|    30 | 1041 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    30 | 1042 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1043 | `#endif` |
|    16 | 1044 | `	}else{` |
|     - | 1045 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|    17 | 1046 | `		time_t t = 0;` |
|     - | 1047 | `		struct tm *pTm;` |
|     - | 1048 | `		int bUseNow;` |
|    17 | 1049 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|    17 | 1050 | `		if( rc != PH7_OK ){` |
|     9 | 1051 | `			return rc;` |
|     - | 1052 | `		}` |
|     9 | 1053 | `		if( bUseNow ){` |
|   ! 0 | 1054 | `			time(&t);` |
|   ! 0 | 1055 | `		}` |
|     9 | 1056 | `		pTm = gmtime(&t);` |
|     9 | 1057 | `		if( pTm == 0 ){` |
|   ! 0 | 1058 | `			time(&t);` |
|   ! 0 | 1059 | `			pTm = gmtime(&t);` |
|   ! 0 | 1060 | `		}` |
|     9 | 1061 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     9 | 1062 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1063 | `	}` |
|     - | 1064 | `	/* Format the given string */` |
|    39 | 1065 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|    39 | 1066 | `	return PH7_OK;` |
|    24 | 1067 | `}` |
|     - | 1068 | `/*` |
|     - | 1069 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|     - | 1070 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|     - | 1071 | ` * Parameters` |
|     - | 1072 | ` *  $format` |
|     - | 1073 | ` *   The format of the outputted date string (See code above)` |
|     - | 1074 | ` * $timestamp` |
|     - | 1075 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1076 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1077 | ` *   In other words, it defaults to the value of time().` |
|     - | 1078 | ` * Return` |
|     - | 1079 | ` * Returns a string formatted according format using the given timestamp` |
|     - | 1080 | ` * or the current local time if no timestamp is given.` |
|     - | 1081 | ` */` |
|    18 | 1082 | `PH7_PRIVATE int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1083 | `{` |
|     - | 1084 | `	const char *zFormat;` |
|     - | 1085 | `	int nLen;` |
|     - | 1086 | `	Sytm sTm;` |
|     - | 1087 | `	/* The php 8.1 whole-function deprecation is declared in aBuiltinDeprecated[] and` |
|     - | 1088 | `	 * emitted at the OP_CALL choke point, which is what puts it BEFORE the` |
|     - | 1089 | `	 * ArgumentCountError for a no-arg call — php's order. */` |
|    19 | 1090 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1091 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1092 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1093 | `		return PH7_OK;` |
|     - | 1094 | `	}` |
|    19 | 1095 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 1096 | `	if( nLen < 1 ){` |
|     - | 1097 | `		/* Don't bother processing return FALSE */` |
|   ! 0 | 1098 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1099 | `	}` |
|    19 | 1100 | `	if( nArg < 2 ){` |
|     - | 1101 | `#ifdef __WINNT__` |
|     - | 1102 | `		SYSTEMTIME sOS;` |
|     1 | 1103 | `		GetSystemTime(&sOS);` |
|     1 | 1104 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1105 | `#else` |
|     - | 1106 | `		struct tm *pTm;` |
|     - | 1107 | `		time_t t;` |
|    16 | 1108 | `		time(&t);` |
|    16 | 1109 | `		pTm = gmtime(&t);` |
|    16 | 1110 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    16 | 1111 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1112 | `#endif` |
|     9 | 1113 | `	}else{` |
|     - | 1114 | `		/* Use the given timestamp */` |
|     - | 1115 | `		time_t t;` |
|     - | 1116 | `		struct tm *pTm;` |
|     3 | 1117 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     3 | 1118 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     3 | 1119 | `			pTm = gmtime(&t);` |
|     3 | 1120 | `			if( pTm == 0 ){` |
|   ! 0 | 1121 | `				time(&t);` |
|   ! 0 | 1122 | `			}` |
|     2 | 1123 | `		}else{` |
|   ! 0 | 1124 | `			time(&t);` |
|     - | 1125 | `		}` |
|     3 | 1126 | `		pTm = gmtime(&t);` |
|     3 | 1127 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     3 | 1128 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1129 | `	}` |
|     - | 1130 | `	/* Format the given string */` |
|    19 | 1131 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|    19 | 1132 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     - | 1133 | `		/* Nothing was formatted,return FALSE */` |
|   ! 0 | 1134 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1135 | `	}` |
|    19 | 1136 | `	return PH7_OK;` |
|    10 | 1137 | `}` |
|     - | 1138 | `/*` |
|     - | 1139 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|     - | 1140 | ` *  Identical to the date() function except that the time returned` |
|     - | 1141 | ` *  is Greenwich Mean Time (GMT).` |
|     - | 1142 | ` * Parameters` |
|     - | 1143 | ` *  $format` |
|     - | 1144 | ` *  The format of the outputted date string (See code above)` |
|     - | 1145 | ` *  $timestamp` |
|     - | 1146 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1147 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1148 | ` *   In other words, it defaults to the value of time().` |
|     - | 1149 | ` * Return` |
|     - | 1150 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - | 1151 | ` */` |
|    40 | 1152 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1153 | `{` |
|     - | 1154 | `	const char *zFormat;` |
|     - | 1155 | `	int nLen;` |
|     - | 1156 | `	Sytm sTm;` |
|    41 | 1157 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1158 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1159 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1160 | `		return PH7_OK;` |
|     - | 1161 | `	}` |
|    41 | 1162 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    41 | 1163 | `	if( nLen < 1 ){` |
|     - | 1164 | `		/* Don't bother processing return the empty string */` |
|   ! 0 | 1165 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1166 | `	}` |
|    41 | 1167 | `	if( nArg < 2 ){` |
|     - | 1168 | `#ifdef __WINNT__` |
|     - | 1169 | `		SYSTEMTIME sOS;` |
|     1 | 1170 | `		GetSystemTime(&sOS);` |
|     1 | 1171 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1172 | `#else` |
|     - | 1173 | `		struct tm *pTm;` |
|     - | 1174 | `		time_t t;` |
|    14 | 1175 | `		time(&t);` |
|    14 | 1176 | `		pTm = gmtime(&t);` |
|    14 | 1177 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    14 | 1178 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1179 | `#endif` |
|     8 | 1180 | `	}else{` |
|     - | 1181 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|    27 | 1182 | `		time_t t = 0;` |
|     - | 1183 | `		struct tm *pTm;` |
|     - | 1184 | `		int bUseNow;` |
|    27 | 1185 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|    27 | 1186 | `		if( rc != PH7_OK ){` |
|     3 | 1187 | `			return rc;` |
|     - | 1188 | `		}` |
|    25 | 1189 | `		if( bUseNow ){` |
|     3 | 1190 | `			time(&t);` |
|     1 | 1191 | `		}` |
|    25 | 1192 | `		pTm = gmtime(&t);` |
|    25 | 1193 | `		if( pTm == 0 ){` |
|   ! 0 | 1194 | `			time(&t);` |
|   ! 0 | 1195 | `			pTm = gmtime(&t);` |
|   ! 0 | 1196 | `		}` |
|    25 | 1197 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    25 | 1198 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1199 | `	}` |
|     - | 1200 | `	/* Format the given string */` |
|    39 | 1201 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|    39 | 1202 | `	return PH7_OK;` |
|    21 | 1203 | `}` |
|     - | 1204 | `/*` |
|     - | 1205 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|     - | 1206 | ` *  Return the local time.` |
|     - | 1207 | ` * Parameter` |
|     - | 1208 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1209 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - | 1210 | ` *     In other words, it defaults to the value of time().` |
|     - | 1211 | ` * $is_associative` |
|     - | 1212 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|     - | 1213 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|     - | 1214 | ` *   array containing all the different elements of the structure returned by the C function` |
|     - | 1215 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|     - | 1216 | ` *      "tm_sec" - seconds, 0 to 59` |
|     - | 1217 | ` *      "tm_min" - minutes, 0 to 59` |
|     - | 1218 | ` *      "tm_hour" - hours, 0 to 23` |
|     - | 1219 | ` *      "tm_mday" - day of the month, 1 to 31` |
|     - | 1220 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|     - | 1221 | ` *      "tm_year" - years since 1900` |
|     - | 1222 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|     - | 1223 | ` *      "tm_yday" - day of the year, 0 to 365` |
|     - | 1224 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|     - | 1225 | ` * Returns` |
|     - | 1226 | ` *  An associative array of information related to the timestamp.` |
|     - | 1227 | ` */` |
|     8 | 1228 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1229 | `{` |
|     - | 1230 | `	ph7_value *pValue,*pArray;` |
|     9 | 1231 | `	int isAssoc = 0;` |
|     - | 1232 | `	Sytm sTm;` |
|     9 | 1233 | `	if( nArg < 1 ){` |
|     - | 1234 | `#ifdef __WINNT__` |
|     - | 1235 | `		SYSTEMTIME sOS;` |
|     1 | 1236 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|     1 | 1237 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1238 | `#else` |
|     - | 1239 | `		struct tm *pTm;` |
|     - | 1240 | `		time_t t;` |
|     4 | 1241 | `		time(&t);` |
|     4 | 1242 | `		pTm = gmtime(&t);` |
|     4 | 1243 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     4 | 1244 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1245 | `#endif` |
|     3 | 1246 | `	}else{` |
|     - | 1247 | `		/* Use the given timestamp */` |
|     - | 1248 | `		time_t t;` |
|     - | 1249 | `		struct tm *pTm;` |
|     5 | 1250 | `		if( ph7_value_is_int(apArg[0]) ){` |
|     5 | 1251 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|     5 | 1252 | `			pTm = gmtime(&t);` |
|     5 | 1253 | `			if( pTm == 0 ){` |
|   ! 0 | 1254 | `				time(&t);` |
|   ! 0 | 1255 | `			}` |
|     3 | 1256 | `		}else{` |
|   ! 0 | 1257 | `			time(&t);` |
|     - | 1258 | `		}` |
|     5 | 1259 | `		pTm = gmtime(&t);` |
|     5 | 1260 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     5 | 1261 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1262 | `	}` |
|     - | 1263 | `	/* Element value */` |
|     9 | 1264 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     9 | 1265 | `	if( pValue == 0 ){` |
|     - | 1266 | `		/* Return NULL */` |
|   ! 0 | 1267 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1268 | `		return PH7_OK;` |
|     - | 1269 | `	}` |
|     - | 1270 | `	/* Create a new array */` |
|     9 | 1271 | `	pArray = ph7_context_new_array(pCtx);` |
|     9 | 1272 | `	if( pArray == 0 ){` |
|     - | 1273 | `		/* Return NULL */` |
|   ! 0 | 1274 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1275 | `		return PH7_OK;` |
|     - | 1276 | `	}` |
|     9 | 1277 | `	if( nArg > 1 ){` |
|     3 | 1278 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|     1 | 1279 | `	}` |
|     - | 1280 | `	/* Fill the array */` |
|     - | 1281 | `	/* Seconds */` |
|     9 | 1282 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|     9 | 1283 | `	if( isAssoc ){` |
|     3 | 1284 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|     2 | 1285 | `	}else{` |
|     7 | 1286 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1287 | `	}` |
|     - | 1288 | `	/* Minutes */` |
|     9 | 1289 | `	ph7_value_int(pValue,sTm.tm_min);` |
|     9 | 1290 | `	if( isAssoc ){` |
|     3 | 1291 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|     2 | 1292 | `	}else{` |
|     7 | 1293 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1294 | `	}` |
|     - | 1295 | `	/* Hours */` |
|     9 | 1296 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|     9 | 1297 | `	if( isAssoc ){` |
|     3 | 1298 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|     2 | 1299 | `	}else{` |
|     7 | 1300 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1301 | `	}` |
|     - | 1302 | `	/* mday */` |
|     9 | 1303 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|     9 | 1304 | `	if( isAssoc ){` |
|     3 | 1305 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|     2 | 1306 | `	}else{` |
|     7 | 1307 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1308 | `	}` |
|     - | 1309 | `	/* mon */` |
|     9 | 1310 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|     9 | 1311 | `	if( isAssoc ){` |
|     3 | 1312 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|     2 | 1313 | `	}else{` |
|     7 | 1314 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1315 | `	}` |
|     - | 1316 | `	/* year since 1900 */` |
|     9 | 1317 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|     9 | 1318 | `	if( isAssoc ){` |
|     3 | 1319 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|     2 | 1320 | `	}else{` |
|     7 | 1321 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1322 | `	}` |
|     - | 1323 | `	/* wday */` |
|     9 | 1324 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|     9 | 1325 | `	if( isAssoc ){` |
|     3 | 1326 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|     2 | 1327 | `	}else{` |
|     7 | 1328 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1329 | `	}` |
|     - | 1330 | `	/* yday */` |
|     9 | 1331 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|     9 | 1332 | `	if( isAssoc ){` |
|     3 | 1333 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|     2 | 1334 | `	}else{` |
|     7 | 1335 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1336 | `	}` |
|     - | 1337 | `	/* isdst */` |
|     - | 1338 | `#ifdef __WINNT__` |
|     - | 1339 | `#ifdef _MSC_VER` |
|     - | 1340 | `#ifndef _WIN32_WCE` |
|     1 | 1341 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1342 | `#endif` |
|     - | 1343 | `#endif` |
|     - | 1344 | `#endif` |
|     9 | 1345 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|     9 | 1346 | `	if( isAssoc ){` |
|     3 | 1347 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|     2 | 1348 | `	}else{` |
|     7 | 1349 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1350 | `	}` |
|     - | 1351 | `	/* Return the array */` |
|     9 | 1352 | `	ph7_result_value(pCtx,pArray);` |
|     9 | 1353 | `	return PH7_OK;` |
|     5 | 1354 | `}` |
|     - | 1355 | `/*` |
|     - | 1356 | ` * int idate(string $format [, int $timestamp = time() ])` |
|     - | 1357 | ` *  Returns a number formatted according to the given format string` |
|     - | 1358 | ` *  using the given integer timestamp or the current local time if` |
|     - | 1359 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|     - | 1360 | ` *  to the value of time().` |
|     - | 1361 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|     - | 1362 | ` *  parameter.` |
|     - | 1363 | ` * $Parameters` |
|     - | 1364 | ` *  Supported format` |
|     - | 1365 | ` *   d 	Day of the month` |
|     - | 1366 | ` *   h 	Hour (12 hour format)` |
|     - | 1367 | ` *   H 	Hour (24 hour format)` |
|     - | 1368 | ` *   i 	Minutes` |
|     - | 1369 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|     - | 1370 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|     - | 1371 | ` *   m 	Month number` |
|     - | 1372 | ` *   s 	Seconds` |
|     - | 1373 | ` *   t 	Days in current month` |
|     - | 1374 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|     - | 1375 | ` *   w 	Day of the week (0 on Sunday)` |
|     - | 1376 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|     - | 1377 | ` *   y 	Year (1 or 2 digits - check note below)` |
|     - | 1378 | ` *   Y 	Year (4 digits)` |
|     - | 1379 | ` *   z 	Day of the year` |
|     - | 1380 | ` *   Z 	Timezone offset in seconds` |
|     - | 1381 | ` * $timestamp` |
|     - | 1382 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|     - | 1383 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|     - | 1384 | ` *  to the value of time().` |
|     - | 1385 | ` * Return` |
|     - | 1386 | ` *  An integer.` |
|     - | 1387 | ` */` |
|    38 | 1388 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1389 | `{` |
|     - | 1390 | `	const char *zFormat;` |
|    40 | 1391 | `	ph7_int64 iVal = 0;` |
|     - | 1392 | `	int nLen;` |
|     - | 1393 | `	Sytm sTm;` |
|    40 | 1394 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1395 | `		/* Missing/Invalid argument,return -1 */` |
|   ! 0 | 1396 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1397 | `		return PH7_OK;` |
|     - | 1398 | `	}` |
|    40 | 1399 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    40 | 1400 | `	if( nLen < 1 ){` |
|     - | 1401 | `		/* Don't bother processing return -1*/` |
|   ! 0 | 1402 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1403 | `	}` |
|    40 | 1404 | `	if( nArg < 2 ){` |
|     - | 1405 | `#ifdef __WINNT__` |
|     - | 1406 | `		SYSTEMTIME sOS;` |
|     2 | 1407 | `		GetSystemTime(&sOS);` |
|     2 | 1408 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1409 | `#else` |
|     - | 1410 | `		struct tm *pTm;` |
|     - | 1411 | `		time_t t;` |
|    28 | 1412 | `		time(&t);` |
|    28 | 1413 | `		pTm = gmtime(&t);` |
|    28 | 1414 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    28 | 1415 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1416 | `#endif` |
|    16 | 1417 | `	}else{` |
|     - | 1418 | `		/* Use the given timestamp */` |
|     - | 1419 | `		time_t t;` |
|     - | 1420 | `		struct tm *pTm;` |
|    11 | 1421 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    11 | 1422 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    11 | 1423 | `			pTm = gmtime(&t);` |
|    11 | 1424 | `			if( pTm == 0 ){` |
|   ! 0 | 1425 | `				time(&t);` |
|   ! 0 | 1426 | `			}` |
|     6 | 1427 | `		}else{` |
|   ! 0 | 1428 | `			time(&t);` |
|     - | 1429 | `		}` |
|    11 | 1430 | `		pTm = gmtime(&t);` |
|    11 | 1431 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    11 | 1432 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1433 | `	}` |
|     - | 1434 | `	/* Perform the requested operation */` |
|    40 | 1435 | `	switch(zFormat[0]){` |
|     2 | 1436 | `	case 'd':` |
|     - | 1437 | `		/* Day of the month */` |
|     5 | 1438 | `		iVal = sTm.tm_mday;` |
|     5 | 1439 | `		break;` |
|   ! 0 | 1440 | `	case 'h':` |
|     - | 1441 | `		/*	Hour (12 hour format)*/` |
|   ! 0 | 1442 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|   ! 0 | 1443 | `		break;` |
|     1 | 1444 | `	case 'H':` |
|     - | 1445 | `		/* Hour (24 hour format)*/` |
|     3 | 1446 | `		iVal = sTm.tm_hour;` |
|     3 | 1447 | `		break;` |
|     1 | 1448 | `	case 'i':` |
|     - | 1449 | `		/*Minutes*/` |
|     3 | 1450 | `		iVal = sTm.tm_min;` |
|     3 | 1451 | `		break;` |
|     1 | 1452 | `	case 'I':` |
|     - | 1453 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|     - | 1454 | `#ifdef __WINNT__` |
|     - | 1455 | `#ifdef _MSC_VER` |
|     - | 1456 | `#ifndef _WIN32_WCE` |
|     1 | 1457 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1458 | `#endif` |
|     - | 1459 | `#endif` |
|     - | 1460 | `#endif` |
|     3 | 1461 | `		iVal = sTm.tm_isdst;` |
|     3 | 1462 | `		break;` |
|     1 | 1463 | `	case 'L':` |
|     - | 1464 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|     3 | 1465 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|     3 | 1466 | `		break;` |
|     2 | 1467 | `	case 'm':` |
|     - | 1468 | `		/* Month number*/` |
|     5 | 1469 | `		iVal = sTm.tm_mon;` |
|     5 | 1470 | `		break;` |
|     1 | 1471 | `	case 's':` |
|     - | 1472 | `		/*Seconds*/` |
|     3 | 1473 | `		iVal = sTm.tm_sec;` |
|     3 | 1474 | `		break;` |
|     1 | 1475 | `	case 't':{` |
|     - | 1476 | `		/*Days in current month*/` |
|     - | 1477 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|     3 | 1478 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|     3 | 1479 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|   ! 0 | 1480 | `			nDays = 28;` |
|   ! 0 | 1481 | `		}` |
|     3 | 1482 | `		iVal = nDays;` |
|     3 | 1483 | `		break;` |
|     - | 1484 | `			 }` |
|     1 | 1485 | `	case 'U':` |
|     - | 1486 | `		/*Seconds since the Unix Epoch*/` |
|     3 | 1487 | `		iVal = (ph7_int64)time(0);` |
|     3 | 1488 | `		break;` |
|     1 | 1489 | `	case 'w':` |
|     - | 1490 | `		/*	Day of the week (0 on Sunday) */` |
|     3 | 1491 | `		iVal = sTm.tm_wday;` |
|     3 | 1492 | `		break;` |
|     1 | 1493 | `	case 'W': {` |
|     - | 1494 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|     - | 1495 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|     3 | 1496 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|     3 | 1497 | `		break;` |
|     - | 1498 | `			  }` |
|   ! 0 | 1499 | `	case 'y':` |
|     - | 1500 | `		/* Year (2 digits) */` |
|   ! 0 | 1501 | `		iVal = sTm.tm_year % 100;` |
|   ! 0 | 1502 | `		break;` |
|     3 | 1503 | `	case 'Y':` |
|     - | 1504 | `		/* Year (4 digits) */` |
|     7 | 1505 | `		iVal = sTm.tm_year;` |
|     7 | 1506 | `		break;` |
|     1 | 1507 | `	case 'z':` |
|     - | 1508 | `		/* Day of the year */` |
|     3 | 1509 | `		iVal = sTm.tm_yday;` |
|     3 | 1510 | `		break;` |
|     1 | 1511 | `	case 'Z':` |
|     - | 1512 | `		/*Timezone offset in seconds*/` |
|     3 | 1513 | `		iVal = sTm.tm_gmtoff;` |
|     3 | 1514 | `		break;` |
|     1 | 1515 | `	default:` |
|     - | 1516 | `		/* unknown format,throw a warning */` |
|     3 | 1517 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|     2 | 1518 | `		break;` |
|     - | 1519 | `	}` |
|     - | 1520 | `	/* Return the time value */` |
|    40 | 1521 | `	ph7_result_int64(pCtx,iVal);` |
|    40 | 1522 | `	return PH7_OK;` |
|    21 | 1523 | `}` |
|     - | 1524 | `/*` |
|     - | 1525 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|     - | 1526 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|     - | 1527 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|     - | 1528 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|     - | 1529 | ` *  specified.` |
|     - | 1530 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|     - | 1531 | ` *  the current value according to the local date and time.` |
|     - | 1532 | ` * Parameters` |
|     - | 1533 | ` * $hour` |
|     - | 1534 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|     - | 1535 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|     - | 1536 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|     - | 1537 | ` * $minute` |
|     - | 1538 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|     - | 1539 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|     - | 1540 | ` *  in the following hour(s).` |
|     - | 1541 | ` * $second` |
|     - | 1542 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|     - | 1543 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|     - | 1544 | ` * second in the following minute(s).` |
|     - | 1545 | ` * $month` |
|     - | 1546 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|     - | 1547 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|     - | 1548 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|     - | 1549 | ` * $day` |
|     - | 1550 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|     - | 1551 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|     - | 1552 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|     - | 1553 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|     - | 1554 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|     - | 1555 | ` * $year` |
|     - | 1556 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|     - | 1557 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|     - | 1558 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|     - | 1559 | ` * $is_dst` |
|     - | 1560 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|     - | 1561 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|     - | 1562 | ` * Return` |
|     - | 1563 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|     - | 1564 | ` *   If the arguments are invalid, the function returns FALSE` |
|     - | 1565 | ` */` |
|    36 | 1566 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1567 | `{` |
|     - | 1568 | `	const char *zFunction;` |
|     - | 1569 | `	ph7_int64 iVal;` |
|     - | 1570 | `	sxi64 h,mi,s,mo,d,y,yAdj;` |
|     - | 1571 | `	int moN;` |
|     - | 1572 | `	struct tm *pTm;` |
|     - | 1573 | `	time_t t;` |
|     - | 1574 | `	/* Extract function name */` |
|    37 | 1575 | `	zFunction = ph7_function_name(pCtx);` |
|     - | 1576 | `	/* PHP 8 dropped the legacy $is_dst 7th parameter: mktime()/gmmktime() now` |
|     - | 1577 | `	 * accept at most 6 arguments and throw a catchable ArgumentCountError` |
|     - | 1578 | `	 * otherwise (the central aBuiltinArity table only enforces the minimum, so` |
|     - | 1579 | `	 * this maximum is checked here). */` |
|    37 | 1580 | `	if( nArg > 6 ){` |
|    10 | 1581 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     3 | 1582 | `			"%s() expects at most 6 arguments, %d given",zFunction,nArg);` |
|     - | 1583 | `	}` |
|    31 | 1584 | `	if( nArg < 1 ){` |
|   ! 0 | 1585 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1586 | `			"%s() expects at least 1 argument, 0 given",zFunction);` |
|     - | 1587 | `	}` |
|     - | 1588 | `	/* Missing components default from the current time in php's default` |
|     - | 1589 | `	 * timezone. PHL's date_default_timezone_set() only accepts UTC/GMT (no tz` |
|     - | 1590 | `	 * database), so mktime() and gmmktime() agree and both read gmtime(). */` |
|    31 | 1591 | `	time(&t);` |
|    31 | 1592 | `	pTm = gmtime(&t);` |
|    15 | 1593 | `	SXUNUSED(zFunction);` |
|    31 | 1594 | `	h  = pTm->tm_hour;` |
|    31 | 1595 | `	mi = pTm->tm_min;` |
|    31 | 1596 | `	s  = pTm->tm_sec;` |
|    31 | 1597 | `	mo = pTm->tm_mon + 1;` |
|    31 | 1598 | `	d  = pTm->tm_mday;` |
|    31 | 1599 | `	y  = pTm->tm_year + 1900;` |
|    31 | 1600 | `	h = ph7_value_to_int64(apArg[0]);` |
|    31 | 1601 | `	if( nArg > 1 ){` |
|    31 | 1602 | `		mi = ph7_value_to_int64(apArg[1]);` |
|    31 | 1603 | `		if( nArg > 2 ){` |
|    31 | 1604 | `			s = ph7_value_to_int64(apArg[2]);` |
|    31 | 1605 | `			if( nArg > 3 ){` |
|    31 | 1606 | `				mo = ph7_value_to_int64(apArg[3]);` |
|    31 | 1607 | `				if( nArg > 4 ){` |
|    31 | 1608 | `					d = ph7_value_to_int64(apArg[4]);` |
|    31 | 1609 | `					if( nArg > 5 ){` |
|     - | 1610 | `						/* php's legacy two-digit mapping: 0-69 -> 2000-2069,` |
|     - | 1611 | `						 * 70-100 -> 1970-2000; anything else is verbatim */` |
|    31 | 1612 | `						y = ph7_value_to_int64(apArg[5]);` |
|    31 | 1613 | `						if( y >= 0 && y <= 69 ){` |
|     7 | 1614 | `							y += 2000;` |
|    28 | 1615 | `						}else if( y >= 70 && y <= 100 ){` |
|     5 | 1616 | `							y += 1900;` |
|     2 | 1617 | `						}` |
|    15 | 1618 | `					}` |
|    15 | 1619 | `				}` |
|    15 | 1620 | `			}` |
|    15 | 1621 | `		}` |
|    15 | 1622 | `	}` |
|     - | 1623 | `	/* Normalize the month with floor semantics, then let day/time components` |
|     - | 1624 | `	 * overflow linearly (php: mktime(25,-30,0,1,1,2024) == Jan 2 00:30). */` |
|    31 | 1625 | `	yAdj = y + DtFloorDiv(mo - 1,12);` |
|    31 | 1626 | `	moN  = (int)(mo - 1 - DtFloorDiv(mo - 1,12) * 12) + 1;` |
|    31 | 1627 | `	iVal = (DtDaysFromCivil(yAdj,moN,1) + (d - 1)) * 86400 + h*3600 + mi*60 + s;` |
|     - | 1628 | `	/* Return the timestamp as a 64bit integer */` |
|    31 | 1629 | `	ph7_result_int64(pCtx,iVal);` |
|    31 | 1630 | `	return PH7_OK;` |
|    19 | 1631 | `}` |
|     - | 1632 | `/*` |
|     - | 1633 | ` * string date_default_timezone_get(void)` |
|     - | 1634 | ` *  Gets the default timezone used by all date/time functions in a script.` |
|     - | 1635 | ` */` |
|     4 | 1636 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1637 | `{` |
|     5 | 1638 | `	ph7_vm *pVm = pCtx->pVm;` |
|     2 | 1639 | `	SXUNUSED(nArg);` |
|     2 | 1640 | `	SXUNUSED(apArg);` |
|     5 | 1641 | `	ph7_result_string(pCtx,pVm->zDefTz,(int)pVm->nDefTz);` |
|     5 | 1642 | `	return PH7_OK;` |
|     1 | 1643 | `}` |
|     - | 1644 | `/*` |
|     - | 1645 | ` * bool date_default_timezone_set(string $timezoneId)` |
|     - | 1646 | ` *  Sets the default timezone used by all date/time functions in a script.` |
|     - | 1647 | ` *  php validates against the tz database and stores the id verbatim (get()` |
|     - | 1648 | ` *  echoes back "utc" if that's what was set). PHL ships no tz database, so` |
|     - | 1649 | ` *  only UTC and GMT are accepted; every other id — including region names php` |
|     - | 1650 | ` *  would accept — is rejected with php's invalid-id notice (recorded scope cut).` |
|     - | 1651 | ` */` |
|    12 | 1652 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1653 | `{` |
|    13 | 1654 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1655 | `	const char *zId;` |
|     - | 1656 | `	int nId;` |
|    13 | 1657 | `	if( nArg < 1 ){` |
|   ! 0 | 1658 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1659 | `		return PH7_OK;` |
|     - | 1660 | `	}` |
|    13 | 1661 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|    13 | 1662 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|    13 | 1663 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|    13 | 1664 | `		pVm->zDefTz[3] = 0;` |
|    13 | 1665 | `		pVm->nDefTz = 3;` |
|    13 | 1666 | `		ph7_result_bool(pCtx,1);` |
|    13 | 1667 | `		return PH7_OK;` |
|     - | 1668 | `	}` |
|     - | 1669 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|     - | 1670 | `	 * — exactly php's notice shape here */` |
|   ! 0 | 1671 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|   ! 0 | 1672 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 1673 | `	return PH7_OK;` |
|     7 | 1674 | `}` |
|     - | 1675 |  |
|     - | 1676 | `/* ===========================================================================` |
|     - | 1677 | ` * DateTime family (NEWPLAN band D slice 1): DateTimeInterface, DateTime,` |
|     - | 1678 | ` * DateTimeImmutable, DateTimeZone (UTC + fixed offsets), date_create(),` |
|     - | 1679 | ` * date_create_immutable(). Embedded-PHP chunk + C thunks, following the` |
|     - | 1680 | ` * Reflection architecture (installed inside the bCompilingBuiltin window).` |
|     - | 1681 | ` * Timezone SCOPE: UTC and fixed "+HH:MM" offsets only — no tz database` |
|     - | 1682 | ` * (recorded §10 scope cut; named region zones throw like unknown zones).` |
|     - | 1683 | ` * ======================================================================== */` |
|     - | 1684 |  |
|     - | 1685 | `/*` |
|     - | 1686 | ` * Proleptic-Gregorian civil <-> day-count conversions (Howard Hinnant's` |
|     - | 1687 | ` * algorithms): no time_t / libc dependence, correct far past 2038 and` |
|     - | 1688 | ` * before 1970 on every platform. Day 0 == 1970-01-01.` |
|     - | 1689 | ` */` |
|   584 | 1690 | `static sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|     1 | 1691 | `{` |
|     - | 1692 | `	sxi64 era;` |
|     - | 1693 | `	unsigned yoe,doy,doe;` |
|   585 | 1694 | `	y -= (m <= 2);` |
|   585 | 1695 | `	era = (y >= 0 ? y : y - 399) / 400;` |
|   585 | 1696 | `	yoe = (unsigned)(y - era * 400);` |
|   585 | 1697 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|   585 | 1698 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|   585 | 1699 | `	return era * 146097 + (sxi64)doe - 719468;` |
|     1 | 1700 | `}` |
|   310 | 1701 | `static void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|     1 | 1702 | `{` |
|     - | 1703 | `	sxi64 era;` |
|     - | 1704 | `	unsigned doe,yoe,doy,mp;` |
|   311 | 1705 | `	z += 719468;` |
|   311 | 1706 | `	era = (z >= 0 ? z : z - 146096) / 146097;` |
|   311 | 1707 | `	doe = (unsigned)(z - era * 146097);` |
|   311 | 1708 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|   311 | 1709 | `	*py = (sxi64)yoe + era * 400;` |
|   311 | 1710 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|   311 | 1711 | `	mp = (5 * doy + 2) / 153;` |
|   311 | 1712 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|   311 | 1713 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|   311 | 1714 | `	if( *pm <= 2 ){` |
|   157 | 1715 | `		*py += 1;` |
|    78 | 1716 | `	}` |
|   311 | 1717 | `}` |
|   516 | 1718 | `static sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|     1 | 1719 | `{` |
|   517 | 1720 | `	sxi64 q = a / b;` |
|   517 | 1721 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|     3 | 1722 | `		q--;` |
|     1 | 1723 | `	}` |
|   517 | 1724 | `	return q;` |
|     1 | 1725 | `}` |
|     - | 1726 | `/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z) */` |
|   130 | 1727 | `static void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)` |
|     1 | 1728 | `{` |
|   131 | 1729 | `	sxi64 t = iTs + iOff;` |
|   131 | 1730 | `	sxi64 days = DtFloorDiv(t,86400);` |
|   131 | 1731 | `	sxi64 secs = t - days * 86400;` |
|     - | 1732 | `	sxi64 y;` |
|     - | 1733 | `	int mo,d;` |
|   131 | 1734 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|   131 | 1735 | `	pTm->tm_sec  = (int)(secs % 60);` |
|   131 | 1736 | `	pTm->tm_min  = (int)((secs / 60) % 60);` |
|   131 | 1737 | `	pTm->tm_hour = (int)(secs / 3600);` |
|   131 | 1738 | `	pTm->tm_mday = d;` |
|   131 | 1739 | `	pTm->tm_mon  = mo - 1;` |
|   131 | 1740 | `	pTm->tm_year = (int)y;` |
|   131 | 1741 | `	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */` |
|   131 | 1742 | `	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));` |
|   131 | 1743 | `	pTm->tm_isdst = 0;` |
|   131 | 1744 | `	pTm->tm_zone = zZone;` |
|   131 | 1745 | `	pTm->tm_gmtoff = (long)iOff;` |
|   131 | 1746 | `}` |
|   200 | 1747 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|     1 | 1748 | `{` |
|   201 | 1749 | `	return DtDaysFromCivil(y,mo,d) * 86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     1 | 1750 | `}` |
|     - | 1751 | `/* Month-arithmetic with php's overflow semantics (Jan 31 +1 month -> Mar 2/3):` |
|     - | 1752 | ` * normalize the month, keep the day — the civil day-count formula is linear in` |
|     - | 1753 | ` * d, so an out-of-range day simply lands in the following month. */` |
|    10 | 1754 | `static sxi64 DtAddMonths(sxi64 iTs,sxi32 iOff,sxi64 nMonths)` |
|     1 | 1755 | `{` |
|    11 | 1756 | `	sxi64 t = iTs + iOff;` |
|    11 | 1757 | `	sxi64 days = DtFloorDiv(t,86400);` |
|    11 | 1758 | `	sxi64 secs = t - days * 86400;` |
|     - | 1759 | `	sxi64 y;` |
|     - | 1760 | `	int mo,d;` |
|     - | 1761 | `	sxi64 m0;` |
|    11 | 1762 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|    11 | 1763 | `	m0 = (y * 12 + (mo - 1)) + nMonths;` |
|    11 | 1764 | `	y  = DtFloorDiv(m0,12);` |
|    11 | 1765 | `	mo = (int)(m0 - y * 12) + 1;` |
|    11 | 1766 | `	return DtDaysFromCivil(y,mo,d) * 86400 + secs - iOff;` |
|     1 | 1767 | `}` |
|     - | 1768 | `/*` |
|     - | 1769 | ` * Parse an OPTIONAL time-of-day suffix after a date component:` |
|     - | 1770 | ` * "[( \|T)]HH:MM[:SS][.frac][Z\|±hh[:mm]]". On entry *pz points just past the date;` |
|     - | 1771 | ` * the h/mi/s outs must be pre-zeroed and the offset outs pre-seeded with the current` |
|     - | 1772 | ` * offset. Advances *pz over whatever it consumes. Returns 0 on success (whether or` |
|     - | 1773 | ` * not a time was present), or a 1-based error position into zIn (negative encodes` |
|     - | 1774 | ` * php's "Double time specification"). Shared by every absolute-date branch.` |
|     - | 1775 | ` */` |
|   168 | 1776 | `static int DtTimeSuffix(const char **pz,const char *zEnd,const char *zIn,` |
|     - | 1777 | `	int *ph,int *pmi,int *ps,sxi32 *piOff,int *pbOffSet)` |
|     1 | 1778 | `{` |
|   169 | 1779 | `	const char *z = *pz;` |
|   168 | 1780 | `	if( z < zEnd && (z[0]=='T' \|\| z[0]==' ') && zEnd-z >= 6` |
|    56 | 1781 | `	 && SyisDigit(z[1]) && SyisDigit(z[2]) && z[3]==':' ){` |
|    53 | 1782 | `		z++;` |
|    53 | 1783 | `		*ph  = (z[0]-'0')*10 + (z[1]-'0');` |
|    53 | 1784 | `		*pmi = (z[3]-'0')*10 + (z[4]-'0');` |
|     - | 1785 | `		/* a 25+ hour kills php's whole time token: error at its start */` |
|    53 | 1786 | `		if( *ph > 24 ){ return (int)(z - zIn) + 1; }` |
|     - | 1787 | `		/* php lexes HH:M, then the minute's second digit starts a SECOND time` |
|     - | 1788 | `		 * token: "Double time specification" (negative encoding) */` |
|    51 | 1789 | `		if( *pmi > 59 ){ return -((int)(&z[4] - zIn) + 1); }` |
|    49 | 1790 | `		z += 5;` |
|    49 | 1791 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|    47 | 1792 | `			*ps = (z[1]-'0')*10 + (z[2]-'0');` |
|    47 | 1793 | `			if( *ps > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|    45 | 1794 | `			z += 3;` |
|    22 | 1795 | `		}` |
|    47 | 1796 | `		if( z < zEnd && z[0]=='.' ){ /* fractional seconds: consume */` |
|   ! 0 | 1797 | `			z++;` |
|   ! 0 | 1798 | `			while( z < zEnd && SyisDigit(z[0]) ){ z++; }` |
|   ! 0 | 1799 | `		}` |
|    47 | 1800 | `		if( z < zEnd && (z[0]=='Z' \|\| z[0]=='z') ){` |
|     7 | 1801 | `			*piOff = 0; *pbOffSet = 2; z++;` |
|    44 | 1802 | `		}else if( z < zEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|     7 | 1803 | `			int sign = (z[0]=='-') ? -1 : 1;` |
|     7 | 1804 | `			int oh,om = 0;` |
|     7 | 1805 | `			z++;` |
|     7 | 1806 | `			if( zEnd-z < 2 \|\| !SyisDigit(z[0]) \|\| !SyisDigit(z[1]) ){ return (int)(z - zIn) + 1; }` |
|     7 | 1807 | `			oh = (z[0]-'0')*10 + (z[1]-'0');` |
|     7 | 1808 | `			z += 2;` |
|     7 | 1809 | `			if( z < zEnd && z[0]==':' ){ z++; }` |
|     7 | 1810 | `			if( zEnd-z >= 2 && SyisDigit(z[0]) && SyisDigit(z[1]) ){` |
|     7 | 1811 | `				om = (z[0]-'0')*10 + (z[1]-'0');` |
|     7 | 1812 | `				z += 2;` |
|     3 | 1813 | `			}` |
|     7 | 1814 | `			*piOff = sign * (oh*3600 + om*60);` |
|     7 | 1815 | `			*pbOffSet = 1;` |
|     3 | 1816 | `		}` |
|    23 | 1817 | `	}` |
|   163 | 1818 | `	*pz = z;` |
|   163 | 1819 | `	return 0;` |
|    85 | 1820 | `}` |
|     - | 1821 | `/*` |
|     - | 1822 | ` * Read one or two decimal digits at z (z<zEnd guaranteed by caller for the first).` |
|     - | 1823 | ` * Returns the value; *pn = digits consumed (1 or 2).` |
|     - | 1824 | ` */` |
|   118 | 1825 | `static int DtRead1or2(const char *z,const char *zEnd,int *pn)` |
|     1 | 1826 | `{` |
|   119 | 1827 | `	int v = z[0]-'0';` |
|   119 | 1828 | `	if( z+1 < zEnd && SyisDigit(z[1]) ){ v = v*10 + (z[1]-'0'); *pn = 2; }` |
|    13 | 1829 | `	else { *pn = 1; }` |
|   119 | 1830 | `	return v;` |
|     1 | 1831 | `}` |
|     - | 1832 | `/*` |
|     - | 1833 | ` * Try to read a non-ISO numeric date at z: three integer components joined by ONE` |
|     - | 1834 | ` * consistent separator, plus an optional time suffix. php's field order depends on` |
|     - | 1835 | ` * the separator:` |
|     - | 1836 | ` *   '/'      -> YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY` |
|     - | 1837 | ` *   '-','.'  -> DD-MM-YYYY (day first); a 4-digit-first '.' date (YYYY.MM.DD) is` |
|     - | 1838 | ` *               NOT a php format and is rejected. (ISO YYYY-MM-DD is matched by the` |
|     - | 1839 | ` *               dedicated branch BEFORE this one, so a 4-digit-first '-' never` |
|     - | 1840 | ` *               reaches here.)` |
|     - | 1841 | ` * A 1-2 digit year maps php-style (00-69 -> 2000s, 70-99 -> 1900s). Returns 0 when` |
|     - | 1842 | ` * the text is not such a date (caller falls through), 1 on success (the ts/off outs` |
|     - | 1843 | ` * set and *pzOut advanced past the whole token), or an error code in DtParse's own` |
|     - | 1844 | ` * convention (positive 1-based position into zIn, negative = "double time") when the` |
|     - | 1845 | ` * shape matched but a component is out of range.` |
|     - | 1846 | ` */` |
|    90 | 1847 | `static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,` |
|     - | 1848 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn)` |
|     1 | 1849 | `{` |
|     - | 1850 | `	int a,b,c,na,nb,nc;` |
|     - | 1851 | `	char sep;` |
|    91 | 1852 | `	int y,mo,d,h = 0,mi = 0,s = 0;` |
|    91 | 1853 | `	sxi32 iOff = *pOff;` |
|     - | 1854 | `	int rcT;` |
|     - | 1855 | `	/* first field: 1-4 digits */` |
|    91 | 1856 | `	if( !SyisDigit(z[0]) ){ return 0; }` |
|    91 | 1857 | `	a = 0; na = 0;` |
|   289 | 1858 | `	while( z < zEnd && SyisDigit(z[0]) && na < 4 ){ a = a*10 + (z[0]-'0'); z++; na++; }` |
|    91 | 1859 | `	if( z >= zEnd \|\| (z[0] != '-' && z[0] != '/' && z[0] != '.') ){ return 0; }` |
|    57 | 1860 | `	sep = z[0];` |
|    57 | 1861 | `	z++;` |
|     - | 1862 | `	/* second field: 1-2 digits */` |
|    57 | 1863 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    57 | 1864 | `	b = DtRead1or2(z,zEnd,&nb); z += nb;` |
|    57 | 1865 | `	if( z >= zEnd \|\| z[0] != sep ){ return 0; }` |
|    57 | 1866 | `	z++;` |
|     - | 1867 | `	/* third field: 1-4 digits */` |
|    57 | 1868 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    57 | 1869 | `	c = 0; nc = 0;` |
|   239 | 1870 | `	while( z < zEnd && SyisDigit(z[0]) && nc < 4 ){ c = c*10 + (z[0]-'0'); z++; nc++; }` |
|     - | 1871 | `	/* map fields to Y/M/D; nyear tracks the year field's width for 2-digit mapping.` |
|     - | 1872 | `	 * '/'  : YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY.` |
|     - | 1873 | `	 * '-'/'.': a 4-digit LAST field is DD-MM-YYYY (day first); otherwise YY-MM-DD` |
|     - | 1874 | `	 *          (year first) — php's width heuristic. (A 4-digit FIRST '-' field is` |
|     - | 1875 | `	 *          ISO and never reaches here; a 4-digit-first '.' is not a php format.) */` |
|     - | 1876 | `	{` |
|     - | 1877 | `		int nyear;` |
|    57 | 1878 | `		if( sep == '/' ){` |
|    27 | 1879 | `			if( na == 4 ){ y = a; mo = b; d = c; nyear = na; }` |
|    19 | 1880 | `			else{ mo = a; d = b; y = c; nyear = nc; }` |
|    44 | 1881 | `		}else if( sep == '.' ){` |
|     - | 1882 | `			/* php's dot date is DD.MM.YYYY only (a 4-digit year, day first). Other` |
|     - | 1883 | `			 * widths are not a clean php format (php itself yields garbage there),` |
|     - | 1884 | `			 * so don't claim the match — let the caller fail the parse. */` |
|     5 | 1885 | `			if( na == 4 \|\| nc != 4 ){ return 0; }` |
|     3 | 1886 | `			d = a; mo = b; y = c; nyear = nc;` |
|     2 | 1887 | `		}else{ /* '-' : a 4-digit LAST field is DD-MM-YYYY, else YY-MM-DD */` |
|    27 | 1888 | `			if( nc == 4 ){ d = a; mo = b; y = c; nyear = nc; }` |
|    11 | 1889 | `			else{ y = a; mo = b; d = c; nyear = na; }` |
|     - | 1890 | `		}` |
|    55 | 1891 | `		if( nyear <= 2 ){` |
|    11 | 1892 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|     3 | 1893 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|     5 | 1894 | `		}` |
|     - | 1895 | `	}` |
|     - | 1896 | `	/* php normalizes month 0 to December of the previous year (like the ISO branch)` |
|     - | 1897 | `	 * but fails a month past 12; a day past 31 fails, while day 0 normalizes in` |
|     - | 1898 | `	 * DtMakeTs. Errors point at the field end. */` |
|    55 | 1899 | `	if( mo > 12 ){ return (int)(z - zIn) + 1; }` |
|    51 | 1900 | `	if( mo == 0 ){ mo = 12; y--; }` |
|    51 | 1901 | `	if( d > 31 ){ return (int)(z - zIn) + 1; }` |
|     - | 1902 | `	/* optional time-of-day suffix, then commit */` |
|    47 | 1903 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff);` |
|    47 | 1904 | `	if( rcT != 0 ){ return rcT; }` |
|    47 | 1905 | `	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    47 | 1906 | `	*pOff = iOff;` |
|    47 | 1907 | `	*pzOut = z;` |
|    47 | 1908 | `	return 1;` |
|    46 | 1909 | `}` |
|     - | 1910 | `/*` |
|     - | 1911 | ` * Match a month name at z (full name or its distinct 3-letter abbreviation, plus` |
|     - | 1912 | ` * "sept"), case-insensitively and only at a word boundary. Returns the month 1-12` |
|     - | 1913 | ` * and sets *pAdv to the bytes consumed, or 0 when no month name is present.` |
|     - | 1914 | ` */` |
|   126 | 1915 | `static int DtMatchMonth(const char *z,const char *zEnd,int *pAdv)` |
|     1 | 1916 | `{` |
|     - | 1917 | `	static const struct { const char *z; int n; int mo; } aM[] = {` |
|     - | 1918 | `		{ "january",7,1 },{ "february",8,2 },{ "march",5,3 },{ "april",5,4 },` |
|     - | 1919 | `		{ "june",4,6 },{ "july",4,7 },{ "august",6,8 },{ "september",9,9 },` |
|     - | 1920 | `		{ "sept",4,9 },{ "october",7,10 },{ "november",8,11 },{ "december",8,12 },` |
|     - | 1921 | `		{ "may",3,5 },` |
|     - | 1922 | `		{ "jan",3,1 },{ "feb",3,2 },{ "mar",3,3 },{ "apr",3,4 },{ "jun",3,6 },` |
|     - | 1923 | `		{ "jul",3,7 },{ "aug",3,8 },{ "sep",3,9 },{ "oct",3,10 },{ "nov",3,11 },` |
|     - | 1924 | `		{ "dec",3,12 }` |
|     - | 1925 | `	};` |
|     - | 1926 | `	sxu32 i;` |
|  2281 | 1927 | `	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){` |
|  2205 | 1928 | `		int n = aM[i].n;` |
|  2204 | 1929 | `		if( zEnd - z >= n && SyStrnicmp(z,aM[i].z,(sxu32)n) == 0` |
|  1010 | 1930 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|    51 | 1931 | `			*pAdv = n;` |
|    51 | 1932 | `			return aM[i].mo;` |
|     - | 1933 | `		}` |
|  1078 | 1934 | `	}` |
|    77 | 1935 | `	return 0;` |
|    64 | 1936 | `}` |
|     - | 1937 | `/* True if z points at a two-letter English ordinal suffix (st/nd/rd/th). */` |
|    62 | 1938 | `static int DtIsOrdinal(const char *z,const char *zEnd)` |
|     1 | 1939 | `{` |
|    63 | 1940 | `	if( zEnd - z < 2 ){ return 0; }` |
|   119 | 1941 | `	return SyStrnicmp(z,"st",2) == 0 \|\| SyStrnicmp(z,"nd",2) == 0` |
|    89 | 1942 | `		\|\| SyStrnicmp(z,"rd",2) == 0 \|\| SyStrnicmp(z,"th",2) == 0;` |
|    32 | 1943 | `}` |
|     - | 1944 | `/*` |
|     - | 1945 | ` * Try to read a textual-month date at z, in either order:` |
|     - | 1946 | ` *   MonthName [Day] [Year]   ("Jan 15 2020", "January", "January 2020")` |
|     - | 1947 | ` *   Day MonthName [Year]     ("15 January 2020", "15th Jan")` |
|     - | 1948 | ` * A missing day defaults to 1, a missing year to the base timestamp's year (php).` |
|     - | 1949 | ` * Day may carry an ordinal suffix, fields may be comma-separated, month names are` |
|     - | 1950 | ` * case-insensitive, and an optional time-of-day suffix + trailing UTC/GMT is` |
|     - | 1951 | ` * consumed. Returns 0 (not a month date — caller falls through, *pzOut untouched),` |
|     - | 1952 | ` * 1 on success, or a DtParse error code (out-of-range day).` |
|     - | 1953 | ` */` |
|    90 | 1954 | `static int DtTryMonthDate(const char *z,const char *zEnd,const char **pzOut,` |
|     - | 1955 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,sxi64 iBaseTs)` |
|     1 | 1956 | `{` |
|    91 | 1957 | `	int mo,d = 1,adv,haveDay = 0,haveYear = 0;` |
|    91 | 1958 | `	sxi64 y = 0;` |
|    91 | 1959 | `	int h = 0,mi = 0,s = 0;` |
|    91 | 1960 | `	sxi32 iOff = *pOff;` |
|     - | 1961 | `	int rcT;` |
|     - | 1962 | `#define MDSKIPWS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|    91 | 1963 | `	if( (mo = DtMatchMonth(z,zEnd,&adv)) != 0 ){` |
|     - | 1964 | `		/* MonthName [Day] [Year]. A 4-digit number here is the YEAR, not the day` |
|     - | 1965 | `		 * ("January 2020" is month+year, day defaults); a 1-2 digit number is the day. */` |
|    31 | 1966 | `		z += adv;` |
|    76 | 1967 | `		MDSKIPWS();` |
|    31 | 1968 | `		if( z < zEnd && SyisDigit(z[0]) ){` |
|    31 | 1969 | `			int nrun = 0;` |
|    31 | 1970 | `			const char *zp = z;` |
|    95 | 1971 | `			while( zp < zEnd && SyisDigit(zp[0]) && nrun < 4 ){ zp++; nrun++; }` |
|    31 | 1972 | `			if( nrun < 4 ){` |
|    27 | 1973 | `				d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|    27 | 1974 | `				if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|    27 | 1975 | `				haveDay = 1;` |
|    68 | 1976 | `				MDSKIPWS();` |
|    13 | 1977 | `			}` |
|    16 | 1978 | `		}` |
|    76 | 1979 | `	}else if( SyisDigit(z[0]) ){` |
|     - | 1980 | `		/* Day MonthName [Year] */` |
|    37 | 1981 | `		d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|    37 | 1982 | `		if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|    37 | 1983 | `		haveDay = 1;` |
|    77 | 1984 | `		MDSKIPWS();` |
|    37 | 1985 | `		if( (mo = DtMatchMonth(z,zEnd,&adv)) == 0 ){ return 0; }` |
|    21 | 1986 | `		z += adv;` |
|    49 | 1987 | `		MDSKIPWS();` |
|    11 | 1988 | `	}else{` |
|    25 | 1989 | `		return 0;` |
|     - | 1990 | `	}` |
|     - | 1991 | `	/* optional year */` |
|    51 | 1992 | `	if( z < zEnd && SyisDigit(z[0]) ){` |
|    47 | 1993 | `		int ny = 0;` |
|    47 | 1994 | `		y = 0;` |
|   231 | 1995 | `		while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ y = y*10 + (z[0]-'0'); z++; ny++; }` |
|    47 | 1996 | `		if( ny <= 2 ){` |
|   ! 0 | 1997 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|   ! 0 | 1998 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|   ! 0 | 1999 | `		}` |
|    47 | 2000 | `		haveYear = 1;` |
|    23 | 2001 | `	}` |
|     - | 2002 | `	/* Default the unspecified fields from the base timestamp. php overlays: a` |
|     - | 2003 | `	 * missing year takes the base year; a missing day is 1 when a year WAS given` |
|     - | 2004 | `	 * ("January 2020" -> the 1st) but the base day when only the month was named` |
|     - | 2005 | `	 * ("January" -> the base day). */` |
|     - | 2006 | `	{` |
|     - | 2007 | `		sxi64 by; int bm,bd;` |
|    51 | 2008 | `		DtCivilFromDays(DtFloorDiv(iBaseTs + *pOff,86400),&by,&bm,&bd);` |
|    51 | 2009 | `		if( !haveYear ){ y = by; }` |
|    51 | 2010 | `		if( !haveDay ){ d = haveYear ? 1 : bd; }` |
|     - | 2011 | `	}` |
|    51 | 2012 | `	if( d > 31 ){ return (int)(z - zIn) + 1; }` |
|     - | 2013 | `	/* optional time-of-day suffix */` |
|    51 | 2014 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff);` |
|    51 | 2015 | `	if( rcT != 0 ){ return rcT; }` |
|     - | 2016 | `	/* optional trailing UTC/GMT zone name (PHL's default zone is already UTC) */` |
|    55 | 2017 | `	MDSKIPWS();` |
|    50 | 2018 | `	if( (zEnd-z >= 3 && SyStrnicmp(z,"utc",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3])))` |
|    49 | 2019 | `	 \|\| (zEnd-z >= 3 && SyStrnicmp(z,"gmt",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3]))) ){` |
|     3 | 2020 | `		iOff = 0; z += 3;` |
|     1 | 2021 | `	}` |
|    51 | 2022 | `	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    51 | 2023 | `	*pOff = iOff;` |
|    51 | 2024 | `	*pzOut = z;` |
|    51 | 2025 | `	return 1;` |
|     - | 2026 | `#undef MDSKIPWS` |
|    46 | 2027 | `}` |
|     - | 2028 | `/*` |
|     - | 2029 | ` * Minimal php-datetime-string parser (slice 1): absolute forms` |
|     - | 2030 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]",` |
|     - | 2031 | ` * keywords today/midnight/noon/tomorrow/yesterday, and relative sequences` |
|     - | 2032 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". Returns 0 on` |
|     - | 2033 | ` * success (ts/off/bOffSet out), or the byte position of the first` |
|     - | 2034 | ` * unparseable character +1 (for php's "at position N" message).` |
|     - | 2035 | ` */` |
|   312 | 2036 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,` |
|     - | 2037 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet)` |
|     1 | 2038 | `{` |
|   313 | 2039 | `	const char *z = zIn, *zEnd = &zIn[nLen];` |
|   313 | 2040 | `	sxi64 iTs = iBaseTs;` |
|   313 | 2041 | `	sxi32 iOff = iBaseOff;` |
|   313 | 2042 | `	int bOffSet = 0;` |
|   313 | 2043 | `	int bAny = 0;` |
|     - | 2044 | `	int iNumRc,iMonRc;` |
|     - | 2045 | `#define DT_SKIP_WS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|     - | 2046 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|     - | 2047 | `	&& (zEnd-z == (nKw) \|\| !SyisAlpha(z[(nKw)])))` |
|   475 | 2048 | `	DT_SKIP_WS();` |
|   313 | 2049 | `	if( z >= zEnd ){` |
|     - | 2050 | `		/* php: the empty string is "now" */` |
|     3 | 2051 | `		*pTs = iTs;` |
|     3 | 2052 | `		*pOff = iOff;` |
|     3 | 2053 | `		*pbOffSet = bOffSet;` |
|     3 | 2054 | `		return 0;` |
|     - | 2055 | `	}` |
|     - | 2056 | `	/* "@<seconds>" absolute epoch */` |
|   311 | 2057 | `	if( z[0] == '@' ){` |
|    63 | 2058 | `		int neg = 0;` |
|    63 | 2059 | `		sxi64 v = 0;` |
|    63 | 2060 | `		const char *zAt = z;` |
|    63 | 2061 | `		z++;` |
|    63 | 2062 | `		if( z < zEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|     - | 2063 | `		/* php's lexer rejects the whole token: the error points at the '@' */` |
|    63 | 2064 | `		if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zAt - zIn) + 1; }` |
|   171 | 2065 | `		while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|    61 | 2066 | `		*pTs = neg ? -v : v;` |
|    61 | 2067 | `		*pOff = 0;` |
|    61 | 2068 | `		*pbOffSet = 1;` |
|    61 | 2069 | `		DT_SKIP_WS();` |
|    61 | 2070 | `		return (z < zEnd) ? (int)(z - zIn) + 1 : 0;` |
|     - | 2071 | `	}` |
|     - | 2072 | `	/* Absolute date: YYYY-MM-DD[...] */` |
|   248 | 2073 | `	if( zEnd-z >= 10 && SyisDigit(z[0]) && SyisDigit(z[1]) && SyisDigit(z[2])` |
|   119 | 2074 | `	 && SyisDigit(z[3]) && z[4]=='-' ){` |
|    83 | 2075 | `		sxi64 y = (z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0');` |
|    83 | 2076 | `		int mo,d,h=0,mi=0,s=0;` |
|    83 | 2077 | `		if( !SyisDigit(z[5])\|\|!SyisDigit(z[6])\|\|z[7] != '-'\|\|!SyisDigit(z[8])\|\|!SyisDigit(z[9]) ){` |
|   ! 0 | 2078 | `			return (int)(z - zIn) + 1;` |
|     - | 2079 | `		}` |
|    83 | 2080 | `		mo = (z[5]-'0')*10 + (z[6]-'0');` |
|    83 | 2081 | `		d  = (z[8]-'0')*10 + (z[9]-'0');` |
|     - | 2082 | `		/* php's lexer dies on the SECOND digit of an out-of-range month/day` |
|     - | 2083 | `		 * (either the two-digit pattern fails there, or a one-digit component` |
|     - | 2084 | `		 * matched and the separator check fails there); "00" lexes fine and` |
|     - | 2085 | `		 * normalizes (month 0 == December of the previous year). */` |
|    83 | 2086 | `		if( mo > 12 ){ return (int)(&z[6] - zIn) + 1; }` |
|    77 | 2087 | `		if( d > 31 ){ return (int)(&z[9] - zIn) + 1; }` |
|    73 | 2088 | `		if( mo == 0 ){ mo = 12; y--; }` |
|    73 | 2089 | `		z += 10;` |
|     - | 2090 | `		{` |
|    73 | 2091 | `			int rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,&bOffSet);` |
|    73 | 2092 | `			if( rcT != 0 ){ return rcT; }` |
|     - | 2093 | `		}` |
|    67 | 2094 | `		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    67 | 2095 | `		bAny = 1;` |
|   200 | 2096 | `	}else if( SyisDigit(z[0])` |
|   129 | 2097 | `	 && (iNumRc = DtTryNumericDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn)) != 0 ){` |
|     - | 2098 | `		/* DD-MM-YYYY / DD.MM.YYYY (day first), MM/DD/YYYY (slash, American), and` |
|     - | 2099 | `		 * YYYY/MM/DD (slash, year first) — see DtTryNumericDate. Anything other than` |
|     - | 2100 | `		 * 1 is an error code in DtParse's own convention (positive position / negative` |
|     - | 2101 | `		 * "double time"); propagate it verbatim. */` |
|    55 | 2102 | `		if( iNumRc != 1 ){ return iNumRc; }` |
|    47 | 2103 | `		bAny = 1;` |
|   136 | 2104 | `	}else if( (SyisAlpha(z[0]) \|\| SyisDigit(z[0]))` |
|   102 | 2105 | `	 && (iMonRc = DtTryMonthDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,iBaseTs)) != 0 ){` |
|     - | 2106 | `		/* MonthName Day Year / Day MonthName Year, in any of php's spellings. As with` |
|     - | 2107 | `		 * DtTryNumericDate, anything other than 1 is an error code to propagate. */` |
|    51 | 2108 | `		if( iMonRc != 1 ){ return iMonRc; }` |
|    51 | 2109 | `		bAny = 1;` |
|    88 | 2110 | `	}else if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|    14 | 2111 | `	 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|     - | 2112 | `		/* Time-only: HH:MM[:SS] on the base date */` |
|    11 | 2113 | `		sxi64 t = iTs + iOff;` |
|    11 | 2114 | `		sxi64 days = DtFloorDiv(t,86400);` |
|    11 | 2115 | `		int h  = (z[0]-'0')*10 + (z[1]-'0');` |
|    11 | 2116 | `		int mi = (z[3]-'0')*10 + (z[4]-'0');` |
|    11 | 2117 | `		int s = 0;` |
|     - | 2118 | `		/* php: bad hour kills the token (error at its start); bad minute /` |
|     - | 2119 | `		 * second dies on the component's second digit */` |
|    11 | 2120 | `		if( h > 24 ){ return (int)(z - zIn) + 1; }` |
|     9 | 2121 | `		if( mi > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|     7 | 2122 | `		z += 5;` |
|     7 | 2123 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     5 | 2124 | `			s = (z[1]-'0')*10 + (z[2]-'0');` |
|     5 | 2125 | `			if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|     3 | 2126 | `			z += 3;` |
|     1 | 2127 | `		}` |
|     5 | 2128 | `		iTs = days*86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     5 | 2129 | `		bAny = 1;` |
|    55 | 2130 | `	}else if( DT_LOWEQ("now",3) ){` |
|     3 | 2131 | `		z += 3;` |
|     3 | 2132 | `		bAny = 1;` |
|     1 | 2133 | `	}` |
|     - | 2134 | `	/* Relative / keyword sequence */` |
|   109 | 2135 | `	for(;;){` |
|   282 | 2136 | `		DT_SKIP_WS();` |
|   253 | 2137 | `		if( z >= zEnd ){` |
|   201 | 2138 | `			break;` |
|     - | 2139 | `		}` |
|    53 | 2140 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|     5 | 2141 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     5 | 2142 | `			iTs = days*86400 - iOff;` |
|     5 | 2143 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|     5 | 2144 | `			bAny = 1;` |
|     5 | 2145 | `			continue;` |
|     - | 2146 | `		}` |
|    49 | 2147 | `		if( DT_LOWEQ("noon",4) ){` |
|     3 | 2148 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     3 | 2149 | `			iTs = days*86400 + 12*3600 - iOff;` |
|     3 | 2150 | `			z += 4;` |
|     3 | 2151 | `			bAny = 1;` |
|     3 | 2152 | `			continue;` |
|     - | 2153 | `		}` |
|    47 | 2154 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|     3 | 2155 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) + 1;` |
|     3 | 2156 | `			iTs = days*86400 - iOff;` |
|     3 | 2157 | `			z += 8;` |
|     3 | 2158 | `			bAny = 1;` |
|     3 | 2159 | `			continue;` |
|     - | 2160 | `		}` |
|    45 | 2161 | `		if( DT_LOWEQ("yesterday",9) ){` |
|     3 | 2162 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) - 1;` |
|     3 | 2163 | `			iTs = days*86400 - iOff;` |
|     3 | 2164 | `			z += 9;` |
|     3 | 2165 | `			bAny = 1;` |
|     3 | 2166 | `			continue;` |
|     - | 2167 | `		}` |
|    43 | 2168 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|    31 | 2169 | `			int neg = 0;` |
|    31 | 2170 | `			sxi64 v = 0;` |
|    31 | 2171 | `			const char *zNumStart = z;` |
|    31 | 2172 | `			if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; }` |
|    31 | 2173 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zNumStart - zIn) + 1; }` |
|    77 | 2174 | `			while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|    31 | 2175 | `			if( neg ){ v = -v; }` |
|    74 | 2176 | `			DT_SKIP_WS();` |
|    31 | 2177 | `			if( DT_LOWEQ("seconds",7) )     { iTs += v;            z += 7; }` |
|    31 | 2178 | `			else if( DT_LOWEQ("second",6) ) { iTs += v;            z += 6; }` |
|    31 | 2179 | `			else if( DT_LOWEQ("secs",4) )   { iTs += v;            z += 4; }` |
|    31 | 2180 | `			else if( DT_LOWEQ("sec",3) )    { iTs += v;            z += 3; }` |
|    31 | 2181 | `			else if( DT_LOWEQ("minutes",7) ){ iTs += v*60;         z += 7; }` |
|    29 | 2182 | `			else if( DT_LOWEQ("minute",6) ) { iTs += v*60;         z += 6; }` |
|    29 | 2183 | `			else if( DT_LOWEQ("mins",4) )   { iTs += v*60;         z += 4; }` |
|    29 | 2184 | `			else if( DT_LOWEQ("min",3) )    { iTs += v*60;         z += 3; }` |
|    29 | 2185 | `			else if( DT_LOWEQ("hours",5) )  { iTs += v*3600;       z += 5; }` |
|    27 | 2186 | `			else if( DT_LOWEQ("hour",4) )   { iTs += v*3600;       z += 4; }` |
|    27 | 2187 | `			else if( DT_LOWEQ("days",4) )   { iTs += v*86400;      z += 4; }` |
|    27 | 2188 | `			else if( DT_LOWEQ("day",3) )    { iTs += v*86400;      z += 3; }` |
|    23 | 2189 | `			else if( DT_LOWEQ("weeks",5) )  { iTs += v*7*86400;    z += 5; }` |
|    21 | 2190 | `			else if( DT_LOWEQ("week",4) )   { iTs += v*7*86400;    z += 4; }` |
|    19 | 2191 | `			else if( DT_LOWEQ("fortnights",10) ){ iTs += v*14*86400; z += 10; }` |
|    17 | 2192 | `			else if( DT_LOWEQ("fortnight",9) )  { iTs += v*14*86400; z += 9; }` |
|    17 | 2193 | `			else if( DT_LOWEQ("months",6) ) { iTs = DtAddMonths(iTs,iOff,v); z += 6; }` |
|    15 | 2194 | `			else if( DT_LOWEQ("month",5) )  { iTs = DtAddMonths(iTs,iOff,v); z += 5; }` |
|     9 | 2195 | `			else if( DT_LOWEQ("years",5) )  { iTs = DtAddMonths(iTs,iOff,v*12); z += 5; }` |
|     9 | 2196 | `			else if( DT_LOWEQ("year",4) )   { iTs = DtAddMonths(iTs,iOff,v*12); z += 4; }` |
|     - | 2197 | `			else{` |
|     7 | 2198 | `				return (int)(z - zIn) + 1;` |
|     - | 2199 | `			}` |
|    25 | 2200 | `			bAny = 1;` |
|    25 | 2201 | `			continue;` |
|     - | 2202 | `		}` |
|    13 | 2203 | `		return (int)(z - zIn) + 1;` |
|   ! 0 | 2204 | `	}` |
|   201 | 2205 | `	if( !bAny ){` |
|   ! 0 | 2206 | `		return 1;` |
|     - | 2207 | `	}` |
|   201 | 2208 | `	*pTs = iTs;` |
|   201 | 2209 | `	*pOff = iOff;` |
|   201 | 2210 | `	*pbOffSet = bOffSet;` |
|   201 | 2211 | `	return 0;` |
|     - | 2212 | `#undef DT_SKIP_WS` |
|     - | 2213 | `#undef DT_LOWEQ` |
|   157 | 2214 | `}` |
|     - | 2215 | `/* int __dt_now() */` |
|   180 | 2216 | `static int vm_builtin_dt_now(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2217 | `{` |
|    90 | 2218 | `	SXUNUSED(nArg);` |
|    90 | 2219 | `	SXUNUSED(apArg);` |
|   181 | 2220 | `	ph7_result_int64(pCtx,(ph7_int64)time(0));` |
|   181 | 2221 | `	return PH7_OK;` |
|     1 | 2222 | `}` |
|     - | 2223 | `/* mixed __dt_parse(string $s, int $baseTs, int $baseOff)` |
|     - | 2224 | ` *   -> [ts, off, offWasExplicit] on success; php's error MESSAGE string on` |
|     - | 2225 | ` *      failure (the chunk wraps it in DateMalformedStringException). */` |
|   312 | 2226 | `static int vm_builtin_dt_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2227 | `{` |
|     - | 2228 | `	const char *zIn;` |
|     - | 2229 | `	int nLen;` |
|     - | 2230 | `	sxi64 iBaseTs;` |
|     - | 2231 | `	sxi32 iBaseOff;` |
|   313 | 2232 | `	sxi64 iTs = 0;` |
|   313 | 2233 | `	sxi32 iOff = 0;` |
|   313 | 2234 | `	int bOffSet = 0;` |
|     - | 2235 | `	int iErrPos;` |
|   313 | 2236 | `	if( nArg < 3 ){` |
|   ! 0 | 2237 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2238 | `		return PH7_OK;` |
|     - | 2239 | `	}` |
|   313 | 2240 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   313 | 2241 | `	iBaseTs  = ph7_value_to_int64(apArg[1]);` |
|   313 | 2242 | `	iBaseOff = (sxi32)ph7_value_to_int64(apArg[2]);` |
|   313 | 2243 | `	iErrPos = DtParse(zIn,nLen,iBaseTs,iBaseOff,&iTs,&iOff,&bOffSet);` |
|   313 | 2244 | `	if( iErrPos != 0 ){` |
|     - | 2245 | `		/* Negative encoding: php's "Double time specification" reason */` |
|    51 | 2246 | `		int bDouble = iErrPos < 0;` |
|    51 | 2247 | `		int iPos = (bDouble ? -iErrPos : iErrPos) - 1;` |
|    51 | 2248 | `		char cAt = (iPos < nLen) ? zIn[iPos] : ' ';` |
|     - | 2249 | `		/* php appends a reason: an alphabetic token is assumed to be a timezone` |
|     - | 2250 | `		 * lookup miss, anything else an unexpected character. */` |
|   100 | 2251 | `		ph7_result_string_format(pCtx,` |
|     - | 2252 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    25 | 2253 | `			nLen,zIn,iPos,cAt,` |
|    49 | 2254 | `			bDouble ? "Double time specification"` |
|    48 | 2255 | `			: ((cAt >= 'a' && cAt <= 'z') \|\| (cAt >= 'A' && cAt <= 'Z'))` |
|     - | 2256 | `				? "The timezone could not be found in the database"` |
|    48 | 2257 | `				: "Unexpected character");` |
|    51 | 2258 | `		return PH7_OK;` |
|     - | 2259 | `	}` |
|     - | 2260 | `	{` |
|   263 | 2261 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|   263 | 2262 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|   263 | 2263 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2264 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2265 | `		}` |
|   263 | 2266 | `		ph7_value_int64(pV,iTs);` |
|   263 | 2267 | `		ph7_array_add_elem(pArr,0,pV);` |
|   263 | 2268 | `		ph7_value_int64(pV,iOff);` |
|   263 | 2269 | `		ph7_array_add_elem(pArr,0,pV);` |
|     - | 2270 | `		/* int, not bool: 0 = no explicit offset, 1 = numeric offset/@epoch,` |
|     - | 2271 | `		 * 2 = literal "Z" (php keeps the distinction in the zone name) */` |
|   263 | 2272 | `		ph7_value_int64(pV,bOffSet);` |
|   263 | 2273 | `		ph7_array_add_elem(pArr,0,pV);` |
|   263 | 2274 | `		ph7_result_value(pCtx,pArr);` |
|     - | 2275 | `	}` |
|   263 | 2276 | `	return PH7_OK;` |
|   157 | 2277 | `}` |
|     - | 2278 | `/* string __dt_default_tz(void) — the date_default_timezone_set() identifier */` |
|   180 | 2279 | `static int vm_builtin_dt_default_tz(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2280 | `{` |
|    90 | 2281 | `	SXUNUSED(nArg);` |
|    90 | 2282 | `	SXUNUSED(apArg);` |
|   181 | 2283 | `	ph7_result_string(pCtx,pCtx->pVm->zDefTz,(int)pCtx->pVm->nDefTz);` |
|   181 | 2284 | `	return PH7_OK;` |
|     1 | 2285 | `}` |
|     - | 2286 | `/* string __dt_format(int $ts, int $off, string $tzname, string $format) */` |
|   130 | 2287 | `static int vm_builtin_dt_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2288 | `{` |
|     - | 2289 | `	Sytm sTm;` |
|     - | 2290 | `	sxi64 iTs;` |
|     - | 2291 | `	sxi32 iOff;` |
|     - | 2292 | `	const char *zName,*zFmt;` |
|     - | 2293 | `	int nName,nFmt;` |
|     - | 2294 | `	char zZone[64];` |
|   131 | 2295 | `	if( nArg < 4 ){` |
|   ! 0 | 2296 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2297 | `		return PH7_OK;` |
|     - | 2298 | `	}` |
|   131 | 2299 | `	iTs  = ph7_value_to_int64(apArg[0]);` |
|   131 | 2300 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|   131 | 2301 | `	zName = ph7_value_to_string(apArg[2],&nName);` |
|   131 | 2302 | `	zFmt  = ph7_value_to_string(apArg[3],&nFmt);` |
|   131 | 2303 | `	if( nName >= (int)sizeof(zZone) ){ nName = (int)sizeof(zZone) - 1; }` |
|   131 | 2304 | `	SyMemcpy(zName,zZone,(sxu32)nName);` |
|   131 | 2305 | `	zZone[nName] = 0;` |
|   131 | 2306 | `	DtFillSytm(iTs,iOff,zZone,&sTm);` |
|   131 | 2307 | `	DateFormat(pCtx,zFmt,nFmt,&sTm);` |
|   131 | 2308 | `	return PH7_OK;` |
|    66 | 2309 | `}` |
|     - | 2310 | `/* int __dt_make(int y, int mo, int d, int h, int i, int s, int off) */` |
|     4 | 2311 | `static int vm_builtin_dt_make(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2312 | `{` |
|     - | 2313 | `	sxi64 y;` |
|     - | 2314 | `	int mo,d,h,mi,s;` |
|     - | 2315 | `	sxi32 iOff;` |
|     5 | 2316 | `	if( nArg < 7 ){` |
|   ! 0 | 2317 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2318 | `		return PH7_OK;` |
|     - | 2319 | `	}` |
|     5 | 2320 | `	y   = ph7_value_to_int64(apArg[0]);` |
|     5 | 2321 | `	mo  = ph7_value_to_int(apArg[1]);` |
|     5 | 2322 | `	d   = ph7_value_to_int(apArg[2]);` |
|     5 | 2323 | `	h   = ph7_value_to_int(apArg[3]);` |
|     5 | 2324 | `	mi  = ph7_value_to_int(apArg[4]);` |
|     5 | 2325 | `	s   = ph7_value_to_int(apArg[5]);` |
|     5 | 2326 | `	iOff = (sxi32)ph7_value_to_int64(apArg[6]);` |
|     5 | 2327 | `	ph7_result_int64(pCtx,DtMakeTs(y,mo,d,h,mi,s,iOff));` |
|     5 | 2328 | `	return PH7_OK;` |
|     3 | 2329 | `}` |
|     - | 2330 | `/* Days in a civil month (php's overflow rules use it during diff borrows) */` |
|    50 | 2331 | `static int DtDaysInMonth(sxi64 y,int m)` |
|     1 | 2332 | `{` |
|     - | 2333 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|    51 | 2334 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|     9 | 2335 | `		return 29;` |
|     - | 2336 | `	}` |
|    43 | 2337 | `	return aMonDays[(m - 1) % 12];` |
|    26 | 2338 | `}` |
|     - | 2339 | `/* int __dt_civil_add(int ts, int off, int y, int m, int d, int h, int i,` |
|     - | 2340 | ` *                    int s, int sign)` |
|     - | 2341 | ` *   php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|     - | 2342 | ` *   (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset. */` |
|    54 | 2343 | `static int vm_builtin_dt_civil_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2344 | `{` |
|     - | 2345 | `	sxi64 iTs,iLocal,iDays,iSecs,y0,moT,dayCount;` |
|     - | 2346 | `	sxi32 iOff;` |
|     - | 2347 | `	int mo0,d0,iSign;` |
|     - | 2348 | `	sxi64 y,m,d,h,i,s;` |
|    55 | 2349 | `	if( nArg < 9 ){` |
|   ! 0 | 2350 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2351 | `		return PH7_OK;` |
|     - | 2352 | `	}` |
|    55 | 2353 | `	iTs   = ph7_value_to_int64(apArg[0]);` |
|    55 | 2354 | `	iOff  = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    55 | 2355 | `	y     = ph7_value_to_int64(apArg[2]);` |
|    55 | 2356 | `	m     = ph7_value_to_int64(apArg[3]);` |
|    55 | 2357 | `	d     = ph7_value_to_int64(apArg[4]);` |
|    55 | 2358 | `	h     = ph7_value_to_int64(apArg[5]);` |
|    55 | 2359 | `	i     = ph7_value_to_int64(apArg[6]);` |
|    55 | 2360 | `	s     = ph7_value_to_int64(apArg[7]);` |
|    55 | 2361 | `	iSign = ph7_value_to_int(apArg[8]) < 0 ? -1 : 1;` |
|    55 | 2362 | `	iLocal = iTs + iOff;` |
|    55 | 2363 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    55 | 2364 | `	iSecs  = iLocal - iDays*86400;` |
|    55 | 2365 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    55 | 2366 | `	y0 += iSign * y;` |
|    55 | 2367 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    55 | 2368 | `	y0 += DtFloorDiv(moT,12);` |
|    55 | 2369 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    55 | 2370 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    55 | 2371 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    55 | 2372 | `	ph7_result_int64(pCtx,iLocal - iOff);` |
|    55 | 2373 | `	return PH7_OK;` |
|    28 | 2374 | `}` |
|     - | 2375 | `/* array __dt_civil_diff(int ts1, int off1, int ts2)` |
|     - | 2376 | ` *   -> [y,m,d,h,i,s,days,invert]: timelib's breakdown — field-wise deltas in` |
|     - | 2377 | ` *   the FIRST operand's offset, then borrow seconds→minutes→hours→days, then` |
|     - | 2378 | ` *   the day borrow walks whole months backward from the later date (that walk` |
|     - | 2379 | ` *   is why Jan 31 → Mar 02 reports m=0 d=30, not "1 month"). */` |
|    14 | 2380 | `static int vm_builtin_dt_civil_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2381 | `{` |
|     - | 2382 | `	sxi64 iTs1,iTs2,iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|     - | 2383 | `	sxi32 iOff;` |
|     - | 2384 | `	int moA,dA,moB,dB,bInvert;` |
|     - | 2385 | `	sxi64 sA,sB,y,m,d,h,i,s;` |
|     - | 2386 | `	ph7_value *pArr,*pV;` |
|    15 | 2387 | `	if( nArg < 3 ){` |
|   ! 0 | 2388 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2389 | `		return PH7_OK;` |
|     - | 2390 | `	}` |
|    15 | 2391 | `	iTs1 = ph7_value_to_int64(apArg[0]);` |
|    15 | 2392 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    15 | 2393 | `	iTs2 = ph7_value_to_int64(apArg[2]);` |
|    15 | 2394 | `	bInvert = iTs1 > iTs2;` |
|    15 | 2395 | `	iA = bInvert ? iTs2 : iTs1;` |
|    15 | 2396 | `	iB = bInvert ? iTs1 : iTs2;` |
|    15 | 2397 | `	iLa = iA + iOff;` |
|    15 | 2398 | `	iLb = iB + iOff;` |
|    15 | 2399 | `	daysA = DtFloorDiv(iLa,86400);` |
|    15 | 2400 | `	daysB = DtFloorDiv(iLb,86400);` |
|    15 | 2401 | `	sA = iLa - daysA*86400;` |
|    15 | 2402 | `	sB = iLb - daysB*86400;` |
|    15 | 2403 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|    15 | 2404 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|    15 | 2405 | `	s = (sB % 60) - (sA % 60);` |
|    15 | 2406 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|    15 | 2407 | `	h = (sB / 3600) - (sA / 3600);` |
|    15 | 2408 | `	d = dB - dA;` |
|    15 | 2409 | `	m = moB - moA;` |
|    15 | 2410 | `	y = yB - yA;` |
|    15 | 2411 | `	if( s < 0 ){ s += 60; i--; }` |
|    15 | 2412 | `	if( i < 0 ){ i += 60; h--; }` |
|    15 | 2413 | `	if( h < 0 ){ h += 24; d--; }` |
|    27 | 2414 | `	while( d < 0 ){` |
|    13 | 2415 | `		moB--;` |
|    13 | 2416 | `		if( moB < 1 ){ moB = 12; yB--; }` |
|    13 | 2417 | `		d += DtDaysInMonth(yB,moB);` |
|    13 | 2418 | `		m--;` |
|     1 | 2419 | `	}` |
|    15 | 2420 | `	if( m < 0 ){ m += 12; y--; }` |
|    15 | 2421 | `	pArr = ph7_context_new_array(pCtx);` |
|    15 | 2422 | `	pV = ph7_context_new_scalar(pCtx);` |
|    15 | 2423 | `	if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2424 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2425 | `	}` |
|    15 | 2426 | `	ph7_value_int64(pV,y);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2427 | `	ph7_value_int64(pV,m);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2428 | `	ph7_value_int64(pV,d);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2429 | `	ph7_value_int64(pV,h);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2430 | `	ph7_value_int64(pV,i);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2431 | `	ph7_value_int64(pV,s);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2432 | `	ph7_value_int64(pV,(iB - iA) / 86400); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2433 | `	ph7_value_int64(pV,bInvert); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2434 | `	ph7_result_value(pCtx,pArr);` |
|    15 | 2435 | `	return PH7_OK;` |
|     8 | 2436 | `}` |
|     - | 2437 | `/* int __dt_isodate(int ts, int off, int y, int w, int dow)` |
|     - | 2438 | ` *   setISODate: jump to ISO year/week/weekday, preserving the time of day. */` |
|     8 | 2439 | `static int vm_builtin_dt_isodate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2440 | `{` |
|     - | 2441 | `	sxi64 iTs,iLocal,iTod,jan4,monday1,target,y;` |
|     - | 2442 | `	sxi32 iOff;` |
|     - | 2443 | `	sxi64 w,dow;` |
|     - | 2444 | `	int isoDow;` |
|     9 | 2445 | `	if( nArg < 5 ){` |
|   ! 0 | 2446 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2447 | `		return PH7_OK;` |
|     - | 2448 | `	}` |
|     9 | 2449 | `	iTs = ph7_value_to_int64(apArg[0]);` |
|     9 | 2450 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|     9 | 2451 | `	y   = ph7_value_to_int64(apArg[2]);` |
|     9 | 2452 | `	w   = ph7_value_to_int64(apArg[3]);` |
|     9 | 2453 | `	dow = ph7_value_to_int64(apArg[4]);` |
|     9 | 2454 | `	iLocal = iTs + iOff;` |
|     9 | 2455 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|     9 | 2456 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|     9 | 2457 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|     9 | 2458 | `	monday1 = jan4 - (isoDow - 1);` |
|     9 | 2459 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|     9 | 2460 | `	ph7_result_int64(pCtx,target*86400 + iTod - iOff);` |
|     9 | 2461 | `	return PH7_OK;` |
|     5 | 2462 | `}` |
|     - | 2463 | `/* Consume nMin..nMax digits from *pz; returns count consumed (0 = failure) */` |
|   130 | 2464 | `static int DtEatDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2465 | `{` |
|   131 | 2466 | `	const char *z = *pz;` |
|   131 | 2467 | `	sxi64 v = 0;` |
|   131 | 2468 | `	int n = 0;` |
|   473 | 2469 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|   343 | 2470 | `		v = v*10 + (z[0] - '0');` |
|   343 | 2471 | `		z++;` |
|   343 | 2472 | `		n++;` |
|     1 | 2473 | `	}` |
|   131 | 2474 | `	if( n < nMin ){` |
|     3 | 2475 | `		return 0;` |
|     - | 2476 | `	}` |
|   129 | 2477 | `	*pz = z;` |
|   129 | 2478 | `	*pVal = v;` |
|   129 | 2479 | `	return n;` |
|    66 | 2480 | `}` |
|     - | 2481 | `/* timelib_get_nr's recovery: skip non-digits hunting for the field.` |
|     - | 2482 | ` * Returns 1 = found+read, 0 = digits present but short, -1 = exhausted. */` |
|     2 | 2483 | `static int DtHuntDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2484 | `{` |
|     3 | 2485 | `	const char *z = *pz;` |
|    13 | 2486 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|     3 | 2487 | `	*pz = z;` |
|     3 | 2488 | `	if( z >= zEnd ){` |
|     3 | 2489 | `		return -1;` |
|     - | 2490 | `	}` |
|   ! 0 | 2491 | `	return DtEatDigits(pz,zEnd,nMin,nMax,pVal) ? 1 : 0;` |
|     2 | 2492 | `}` |
|     - | 2493 | `/* Case-insensitive name-table lookup; returns 1-based index or 0 */` |
|    14 | 2494 | `static int DtEatName(const char **pz,const char *zEnd,const char **azNames,int nNames)` |
|     1 | 2495 | `{` |
|     - | 2496 | `	int k;` |
|    23 | 2497 | `	for( k = 0 ; k < nNames ; k++ ){` |
|    23 | 2498 | `		int n = (int)SyStrlen(azNames[k]);` |
|    23 | 2499 | `		if( zEnd - *pz >= n && SyStrnicmp(*pz,azNames[k],(sxu32)n) == 0 ){` |
|    15 | 2500 | `			*pz += n;` |
|    15 | 2501 | `			return k + 1;` |
|     - | 2502 | `		}` |
|     5 | 2503 | `	}` |
|   ! 0 | 2504 | `	return 0;` |
|     8 | 2505 | `}` |
|     - | 2506 | `/* mixed __dt_from_format(string fmt, string input, int nowTs, int defOff)` |
|     - | 2507 | ` *   php's DateTime::createFromFormat engine. Success: [ts, off, offKind, name]` |
|     - | 2508 | ` *   where offKind 0=none-parsed, 1=numeric offset, 2=literal Z, 3=named id.` |
|     - | 2509 | ` *   Failure: "POS\tMESSAGE" (timelib's message strings; PHL reports the FIRST` |
|     - | 2510 | ` *   error where php may accumulate several — recorded). A trailing-data` |
|     - | 2511 | ` *   warning rides as [4]=pos, [5]=msg on the success array. */` |
|    44 | 2512 | `static int vm_builtin_dt_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2513 | `{` |
|     - | 2514 | `	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};` |
|     - | 2515 | `	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",` |
|     - | 2516 | `		"thursday","friday","saturday"};` |
|     - | 2517 | `	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",` |
|     - | 2518 | `		"aug","sep","oct","nov","dec"};` |
|     - | 2519 | `	static const char *azMonFull[] = {"january","february","march","april",` |
|     - | 2520 | `		"may","june","july","august","september","october","november","december"};` |
|     - | 2521 | `	const char *zFmt,*zIn,*zEnd,*zInEnd,*z;` |
|     - | 2522 | `	int nFmt,nIn;` |
|     - | 2523 | `	sxi64 iNow,v;` |
|     - | 2524 | `	sxi32 iDefOff;` |
|     - | 2525 | `	/* -1 == unset */` |
|    45 | 2526 | `	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;` |
|    45 | 2527 | `	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;` |
|    45 | 2528 | `	int iOffKind = 0;` |
|    45 | 2529 | `	sxi32 iOffVal = 0;` |
|     - | 2530 | `	char zName[16];` |
|    45 | 2531 | `	const char *zErr = 0;` |
|     - | 2532 | `	const char *aWarnMsg[3];` |
|     - | 2533 | `	int aWarnPos[3];` |
|    45 | 2534 | `	int nWarn = 0,bAborted = 0;` |
|     - | 2535 | `	const char *aErrMsg[8];` |
|     - | 2536 | `	int aErrPos[8];` |
|    45 | 2537 | `	int nErr = 0,nErrKept = 0;` |
|    45 | 2538 | `	if( nArg < 4 ){` |
|   ! 0 | 2539 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2540 | `		return PH7_OK;` |
|     - | 2541 | `	}` |
|    45 | 2542 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    45 | 2543 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    45 | 2544 | `	iNow = ph7_value_to_int64(apArg[2]);` |
|    45 | 2545 | `	iDefOff = (sxi32)ph7_value_to_int64(apArg[3]);` |
|    45 | 2546 | `	zEnd = &zFmt[nFmt];` |
|    45 | 2547 | `	zInEnd = &zIn[nIn];` |
|    45 | 2548 | `	z = zIn;` |
|    45 | 2549 | `	zName[0] = 0;` |
|     - | 2550 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|     - | 2551 | `	{ int _p = (iPos),_k,_f = -1; \` |
|     - | 2552 | `	  nErr++; \` |
|     - | 2553 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|     - | 2554 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|     - | 2555 | `	  else if( nErrKept < 8 ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|   299 | 2556 | `	while( zFmt < zEnd ){` |
|   257 | 2557 | `		char c = zFmt[0];` |
|   257 | 2558 | `		zFmt++;` |
|   257 | 2559 | `		zErr = 0;` |
|   257 | 2560 | `		if( c == '!' ){` |
|     7 | 2561 | `			y = 1970; mo = 1; d = 1; h = 0; mi = 0; s = 0;` |
|     7 | 2562 | `			h12 = -1; iMeridiem = -1;` |
|     7 | 2563 | `			continue;` |
|     - | 2564 | `		}` |
|   251 | 2565 | `		if( c == '\|' ){ bPipe = 1; continue; }` |
|   247 | 2566 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|   245 | 2567 | `		if( z >= zInEnd ){` |
|     - | 2568 | `			/* timelib aborts the scan once input is exhausted */` |
|     5 | 2569 | `			DT_FF_LOGERR(nIn,"Not enough data available to satisfy format");` |
|     3 | 2570 | `			break;` |
|     - | 2571 | `		}` |
|   243 | 2572 | `		switch( c ){` |
|    15 | 2573 | `		case 'd': case 'j':` |
|    31 | 2574 | `			if( !DtEatDigits(&z,zInEnd,1,2,&d) ){` |
|   ! 0 | 2575 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit day could not be found");` |
|   ! 0 | 2576 | `				if( DtHuntDigits(&z,zInEnd,1,2,&d) < 0 ){` |
|   ! 0 | 2577 | `					DT_FF_LOGERR(nIn,"A two digit day could not be found");` |
|   ! 0 | 2578 | `				}` |
|   ! 0 | 2579 | `			}` |
|    31 | 2580 | `			break;` |
|     1 | 2581 | `		case 'D':` |
|     3 | 2582 | `			if( !DtEatName(&z,zInEnd,azDay3,7) ){` |
|   ! 0 | 2583 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2584 | `			}` |
|     3 | 2585 | `			break;` |
|     1 | 2586 | `		case 'l':` |
|     3 | 2587 | `			if( !DtEatName(&z,zInEnd,azDayFull,7) ){` |
|   ! 0 | 2588 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2589 | `			}` |
|     3 | 2590 | `			break;` |
|     1 | 2591 | `		case 'S':` |
|     - | 2592 | `			/* ordinal suffix: st nd rd th */` |
|     4 | 2593 | `			if( zInEnd-z >= 2 && ((z[0]=='s'&&z[1]=='t')\|\|(z[0]=='n'&&z[1]=='d')` |
|     2 | 2594 | `			 \|\|(z[0]=='r'&&z[1]=='d')\|\|(z[0]=='t'&&z[1]=='h')) ){` |
|     3 | 2595 | `				z += 2;` |
|     1 | 2596 | `			}` |
|     3 | 2597 | `			break;` |
|    13 | 2598 | `		case 'm': case 'n':` |
|    27 | 2599 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mo) ){` |
|   ! 0 | 2600 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit month could not be found");` |
|   ! 0 | 2601 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mo) < 0 ){` |
|   ! 0 | 2602 | `					DT_FF_LOGERR(nIn,"A two digit month could not be found");` |
|   ! 0 | 2603 | `				}` |
|   ! 0 | 2604 | `			}` |
|    27 | 2605 | `			break;` |
|     1 | 2606 | `		case 'M':{` |
|     3 | 2607 | `			int k = DtEatName(&z,zInEnd,azMon3,12);` |
|     3 | 2608 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2609 | `			break;` |
|     - | 2610 | `				 }` |
|     1 | 2611 | `		case 'F':{` |
|     3 | 2612 | `			int k = DtEatName(&z,zInEnd,azMonFull,12);` |
|     3 | 2613 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2614 | `			break;` |
|     - | 2615 | `				 }` |
|   ! 0 | 2616 | `		case 'y':` |
|   ! 0 | 2617 | `			if( DtEatDigits(&z,zInEnd,2,2,&y) ){` |
|   ! 0 | 2618 | `				y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2619 | `			}else{` |
|   ! 0 | 2620 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit year could not be found");` |
|   ! 0 | 2621 | `				if( DtHuntDigits(&z,zInEnd,2,2,&y) < 0 ){` |
|   ! 0 | 2622 | `					DT_FF_LOGERR(nIn,"A two digit year could not be found");` |
|   ! 0 | 2623 | `				}else if( y >= 0 ){` |
|   ! 0 | 2624 | `					y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2625 | `				}` |
|     - | 2626 | `			}` |
|   ! 0 | 2627 | `			break;` |
|    17 | 2628 | `		case 'Y':{` |
|    35 | 2629 | `			int neg = 0;` |
|    35 | 2630 | `			if( z < zInEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|    35 | 2631 | `			if( DtEatDigits(&z,zInEnd,1,4,&y) ){` |
|    33 | 2632 | `				if( neg ){ y = -y; }` |
|    17 | 2633 | `			}else{` |
|     3 | 2634 | `				DT_FF_LOGERR((int)(z - zIn),"A four digit year could not be found");` |
|     3 | 2635 | `				if( DtHuntDigits(&z,zInEnd,1,4,&y) < 0 ){` |
|     5 | 2636 | `					DT_FF_LOGERR(nIn,"A four digit year could not be found");` |
|     1 | 2637 | `				}` |
|     - | 2638 | `			}` |
|    35 | 2639 | `			break;` |
|     - | 2640 | `				 }` |
|     4 | 2641 | `		case 'H': case 'G':` |
|     9 | 2642 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h) ){` |
|   ! 0 | 2643 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2644 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h) < 0 ){` |
|   ! 0 | 2645 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2646 | `				}` |
|   ! 0 | 2647 | `			}` |
|     9 | 2648 | `			break;` |
|     2 | 2649 | `		case 'h': case 'g':` |
|     5 | 2650 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h12) ){` |
|   ! 0 | 2651 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2652 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h12) < 0 ){` |
|   ! 0 | 2653 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2654 | `				}` |
|   ! 0 | 2655 | `			}` |
|     5 | 2656 | `			break;` |
|     6 | 2657 | `		case 'i':` |
|    13 | 2658 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mi) ){` |
|   ! 0 | 2659 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit minute could not be found");` |
|   ! 0 | 2660 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mi) < 0 ){` |
|   ! 0 | 2661 | `					DT_FF_LOGERR(nIn,"A two digit minute could not be found");` |
|   ! 0 | 2662 | `				}` |
|   ! 0 | 2663 | `			}` |
|    13 | 2664 | `			break;` |
|     2 | 2665 | `		case 's':` |
|     5 | 2666 | `			if( !DtEatDigits(&z,zInEnd,1,2,&s) ){` |
|   ! 0 | 2667 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit second could not be found");` |
|   ! 0 | 2668 | `				if( DtHuntDigits(&z,zInEnd,1,2,&s) < 0 ){` |
|   ! 0 | 2669 | `					DT_FF_LOGERR(nIn,"A two digit second could not be found");` |
|   ! 0 | 2670 | `				}` |
|   ! 0 | 2671 | `			}` |
|     5 | 2672 | `			break;` |
|   ! 0 | 2673 | `		case 'u':` |
|     - | 2674 | `			/* micro parsed then dropped: PHL keeps whole seconds (recorded) */` |
|   ! 0 | 2675 | `			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){` |
|   ! 0 | 2676 | `				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");` |
|   ! 0 | 2677 | `				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){` |
|   ! 0 | 2678 | `					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");` |
|   ! 0 | 2679 | `				}` |
|   ! 0 | 2680 | `			}` |
|   ! 0 | 2681 | `			break;` |
|   ! 0 | 2682 | `		case 'v':` |
|   ! 0 | 2683 | `			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){` |
|   ! 0 | 2684 | `				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");` |
|   ! 0 | 2685 | `				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){` |
|   ! 0 | 2686 | `					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");` |
|   ! 0 | 2687 | `				}` |
|   ! 0 | 2688 | `			}` |
|   ! 0 | 2689 | `			break;` |
|     2 | 2690 | `		case 'a': case 'A':{` |
|     - | 2691 | `			static const char *azMer[] = {"am","pm","a.m.","p.m."};` |
|     5 | 2692 | `			int k = DtEatName(&z,zInEnd,azMer,4);` |
|     5 | 2693 | `			if( k ){` |
|     5 | 2694 | `				iMeridiem = ((k - 1) & 1);` |
|     3 | 2695 | `			}else{` |
|   ! 0 | 2696 | `				zErr = "A meridian could not be found";` |
|     - | 2697 | `			}` |
|     5 | 2698 | `			break;` |
|     - | 2699 | `				 }` |
|     2 | 2700 | `		case 'U':{` |
|     5 | 2701 | `			int neg = 0;` |
|     5 | 2702 | `			if( z < zInEnd && z[0]=='-' ){ neg = 1; z++; }` |
|     5 | 2703 | `			if( DtEatDigits(&z,zInEnd,1,19,&uVal) ){` |
|     5 | 2704 | `				if( neg ){ uVal = -uVal; }` |
|     5 | 2705 | `				bHasU = 1;` |
|     3 | 2706 | `			}else{` |
|   ! 0 | 2707 | `				DT_FF_LOGERR((int)(z - zIn),"A unix timestamp could not be found");` |
|   ! 0 | 2708 | `				if( DtHuntDigits(&z,zInEnd,1,19,&uVal) < 0 ){` |
|   ! 0 | 2709 | `					DT_FF_LOGERR(nIn,"A unix timestamp could not be found");` |
|   ! 0 | 2710 | `				}else{` |
|   ! 0 | 2711 | `					if( neg ){ uVal = -uVal; }` |
|   ! 0 | 2712 | `					bHasU = 1;` |
|     - | 2713 | `				}` |
|     - | 2714 | `			}` |
|     5 | 2715 | `			break;` |
|     - | 2716 | `				 }` |
|     1 | 2717 | `		case 'e': case 'T':{` |
|     - | 2718 | `			static const char *azZone[] = {"UTC","GMT","Z"};` |
|     3 | 2719 | `			int k = DtEatName(&z,zInEnd,azZone,3);` |
|     3 | 2720 | `			if( k == 3 ){` |
|   ! 0 | 2721 | `				iOffKind = 2; iOffVal = 0;` |
|     3 | 2722 | `			}else if( k ){` |
|     3 | 2723 | `				iOffKind = 3; iOffVal = 0;` |
|     3 | 2724 | `				SyMemcpy(azZone[k-1],zName,4);` |
|     1 | 2725 | `			}else if( z < zInEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|   ! 0 | 2726 | `				goto parse_num_off;` |
|   ! 0 | 2727 | `			}else{` |
|   ! 0 | 2728 | `				zErr = "The timezone could not be found in the database";` |
|     - | 2729 | `			}` |
|     3 | 2730 | `			break;` |
|     2 | 2731 | `				 }` |
|     - | 2732 | `		case 'O': case 'P':` |
|     2 | 2733 | `parse_num_off:	{` |
|     5 | 2734 | `			int sign,oh,om = 0;` |
|     - | 2735 | `			sxi64 t;` |
|     5 | 2736 | `			if( z >= zInEnd \|\| (z[0] != '+' && z[0] != '-') ){` |
|   ! 0 | 2737 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2738 | `				break;` |
|     - | 2739 | `			}` |
|     5 | 2740 | `			sign = (z[0]=='-') ? -1 : 1;` |
|     5 | 2741 | `			z++;` |
|     5 | 2742 | `			if( !DtEatDigits(&z,zInEnd,2,2,&t) ){` |
|   ! 0 | 2743 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2744 | `				break;` |
|     - | 2745 | `			}` |
|     5 | 2746 | `			oh = (int)t;` |
|     5 | 2747 | `			if( z < zInEnd && z[0]==':' ){ z++; }` |
|     5 | 2748 | `			if( DtEatDigits(&z,zInEnd,2,2,&t) ){ om = (int)t; }` |
|     5 | 2749 | `			iOffKind = 1;` |
|     5 | 2750 | `			iOffVal = sign * (oh*3600 + om*60);` |
|     5 | 2751 | `			break;` |
|     - | 2752 | `				 }` |
|   ! 0 | 2753 | `		case '?':` |
|   ! 0 | 2754 | `			if( z < zInEnd ){ z++; }` |
|   ! 0 | 2755 | `			break;` |
|   ! 0 | 2756 | `		case '*':` |
|     - | 2757 | `			/* skip input until the next separator byte */` |
|   ! 0 | 2758 | `			while( z < zInEnd && !SyisDigit(z[0]) && z[0] != ';' && z[0] != ':'` |
|   ! 0 | 2759 | `			 && z[0] != '/' && z[0] != '.' && z[0] != ',' && z[0] != '-'` |
|   ! 0 | 2760 | `			 && z[0] != '(' && z[0] != ')' && z[0] != ' ' ){` |
|   ! 0 | 2761 | `				z++;` |
|   ! 0 | 2762 | `			}` |
|   ! 0 | 2763 | `			break;` |
|     1 | 2764 | `		case '#':` |
|     3 | 2765 | `			if( z < zInEnd && (z[0]==';'\|\|z[0]==':'\|\|z[0]=='/'\|\|z[0]=='.'` |
|   ! 0 | 2766 | `			 \|\|z[0]==','\|\|z[0]=='-'\|\|z[0]=='('\|\|z[0]==')') ){` |
|     3 | 2767 | `				z++;` |
|     2 | 2768 | `			}else{` |
|   ! 0 | 2769 | `				zErr = "The separation symbol could not be found";` |
|     - | 2770 | `			}` |
|     3 | 2771 | `			break;` |
|     1 | 2772 | `		case '\\':` |
|     3 | 2773 | `			if( zFmt < zEnd ){` |
|     3 | 2774 | `				if( z < zInEnd && z[0] == zFmt[0] ){` |
|     3 | 2775 | `					z++;` |
|     3 | 2776 | `					zFmt++;` |
|     2 | 2777 | `				}else{` |
|     - | 2778 | `					/* a literal mismatch aborts timelib's scan */` |
|   ! 0 | 2779 | `					DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|   ! 0 | 2780 | `					zFmt = zEnd;` |
|   ! 0 | 2781 | `					bAborted = 1;` |
|     - | 2782 | `				}` |
|     1 | 2783 | `			}` |
|     3 | 2784 | `			break;` |
|    34 | 2785 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|     - | 2786 | `		case '(' : case ')':` |
|    69 | 2787 | `			if( z < zInEnd && z[0] == c ){` |
|    69 | 2788 | `				z++;` |
|    35 | 2789 | `			}else{` |
|     - | 2790 | `				/* timelib logs BOTH messages (count +2, last-wins on the` |
|     - | 2791 | `				 * position), consumes the offending byte, and keeps going */` |
|   ! 0 | 2792 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 2793 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 2794 | `				z++;` |
|     - | 2795 | `			}` |
|    69 | 2796 | `			break;` |
|    13 | 2797 | `		case ' ':` |
|    27 | 2798 | `			if( z < zInEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|    27 | 2799 | `				z++;` |
|    14 | 2800 | `			}else{` |
|   ! 0 | 2801 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 2802 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 2803 | `				z++;` |
|     - | 2804 | `			}` |
|    27 | 2805 | `			break;` |
|     1 | 2806 | `		default:` |
|     - | 2807 | `			/* any other format byte must match the input verbatim; a mismatch` |
|     - | 2808 | `			 * aborts timelib's scan */` |
|     3 | 2809 | `			if( z < zInEnd && z[0] == c ){` |
|   ! 0 | 2810 | `				z++;` |
|   ! 0 | 2811 | `			}else{` |
|     3 | 2812 | `				DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|     3 | 2813 | `				zFmt = zEnd;` |
|     3 | 2814 | `				bAborted = 1;` |
|     - | 2815 | `			}` |
|     2 | 2816 | `			break;` |
|     - | 2817 | `		}` |
|   243 | 2818 | `		if( zErr ){` |
|     - | 2819 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|   ! 0 | 2820 | `			DT_FF_LOGERR((int)(z - zIn),zErr);` |
|   ! 0 | 2821 | `		}` |
|     1 | 2822 | `	}` |
|    45 | 2823 | `	if( z < zInEnd && !bAborted ){` |
|     5 | 2824 | `		if( bPlus ){` |
|     - | 2825 | `			/* '+' downgrades trailing data to a warning */` |
|     3 | 2826 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     3 | 2827 | `			aWarnMsg[nWarn] = "Trailing data";` |
|     3 | 2828 | `			nWarn++;` |
|     2 | 2829 | `		}else{` |
|     3 | 2830 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|     - | 2831 | `		}` |
|     2 | 2832 | `	}` |
|    45 | 2833 | `	if( nErr > 0 ){` |
|     - | 2834 | `		SyBlob sOut;` |
|     - | 2835 | `		int k;` |
|     7 | 2836 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     7 | 2837 | `		SyBlobFormat(&sOut,"%d",nErr);` |
|    15 | 2838 | `		for( k = 0 ; k < nErrKept ; k++ ){` |
|     9 | 2839 | `			SyBlobFormat(&sOut,"\n%d\t%s",aErrPos[k],aErrMsg[k]);` |
|     5 | 2840 | `		}` |
|     7 | 2841 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     7 | 2842 | `		SyBlobRelease(&sOut);` |
|     7 | 2843 | `		return PH7_OK;` |
|     - | 2844 | `	}` |
|    39 | 2845 | `	if( bPipe ){` |
|     5 | 2846 | `		if( y < 0 ){ y = 1970; }` |
|     5 | 2847 | `		if( mo < 0 ){ mo = 1; }` |
|     5 | 2848 | `		if( d < 0 ){ d = 1; }` |
|     5 | 2849 | `		if( h < 0 && h12 < 0 ){ h = 0; }` |
|     5 | 2850 | `		if( mi < 0 ){ mi = 0; }` |
|     5 | 2851 | `		if( s < 0 ){ s = 0; }` |
|     2 | 2852 | `	}` |
|     - | 2853 | `	{` |
|     - | 2854 | `		/* remaining unset fields come from "now" in the default offset */` |
|    39 | 2855 | `		sxi64 iLocal = iNow + iDefOff;` |
|    39 | 2856 | `		sxi64 days = DtFloorDiv(iLocal,86400);` |
|    39 | 2857 | `		sxi64 secs = iLocal - days*86400;` |
|     - | 2858 | `		sxi64 ny;` |
|     - | 2859 | `		int nmo,nd;` |
|    39 | 2860 | `		DtCivilFromDays(days,&ny,&nmo,&nd);` |
|    39 | 2861 | `		if( y < 0 ){ y = ny; }` |
|    39 | 2862 | `		if( mo < 0 ){ mo = nmo; }` |
|    39 | 2863 | `		if( d < 0 ){ d = nd; }` |
|    39 | 2864 | `		if( h12 >= 0 ){` |
|     5 | 2865 | `			h = (h12 % 12) + ((iMeridiem == 1) ? 12 : 0);` |
|     2 | 2866 | `		}` |
|     - | 2867 | `		/* php: parsing a time component zeroes the finer unset units */` |
|    39 | 2868 | `		if( h >= 0 ){` |
|    19 | 2869 | `			if( mi < 0 ){ mi = 0; }` |
|    19 | 2870 | `			if( s < 0 ){ s = 0; }` |
|    30 | 2871 | `		}else if( mi >= 0 ){` |
|     3 | 2872 | `			if( s < 0 ){ s = 0; }` |
|     1 | 2873 | `		}` |
|    39 | 2874 | `		if( h < 0 ){ h = secs / 3600; }` |
|    39 | 2875 | `		if( mi < 0 ){ mi = (secs / 60) % 60; }` |
|    39 | 2876 | `		if( s < 0 ){ s = secs % 60; }` |
|     - | 2877 | `	}` |
|     - | 2878 | `	/* php validates the RESOLVED fields and warns (parse still succeeds,` |
|     - | 2879 | `	 * values roll over via civil arithmetic) */` |
|    39 | 2880 | `	if( mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo) ){` |
|     3 | 2881 | `		if( nWarn < 3 ){` |
|     3 | 2882 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 2883 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|     3 | 2884 | `			nWarn++;` |
|     1 | 2885 | `		}` |
|     1 | 2886 | `	}` |
|    39 | 2887 | `	if( h > 24 \|\| mi > 59 \|\| s > 59 ){` |
|     3 | 2888 | `		if( nWarn < 3 ){` |
|     3 | 2889 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 2890 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|     3 | 2891 | `			nWarn++;` |
|     1 | 2892 | `		}` |
|     1 | 2893 | `	}` |
|     - | 2894 | `	{` |
|    39 | 2895 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    39 | 2896 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|     - | 2897 | `		sxi64 iTs;` |
|    39 | 2898 | `		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;` |
|    39 | 2899 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2900 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2901 | `		}` |
|    39 | 2902 | `		if( bHasU ){` |
|     5 | 2903 | `			iTs = uVal;` |
|     5 | 2904 | `			iUseOff = 0;` |
|     5 | 2905 | `			iOffKind = 1;` |
|     3 | 2906 | `		}else{` |
|    35 | 2907 | `			iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);` |
|     - | 2908 | `		}` |
|    39 | 2909 | `		ph7_value_int64(pV,iTs);           ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2910 | `		ph7_value_int64(pV,iUseOff);       ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2911 | `		ph7_value_int64(pV,iOffKind);      ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2912 | `		ph7_value_string(pV,zName,-1);     ph7_array_add_elem(pArr,0,pV);` |
|     - | 2913 | `		{` |
|     - | 2914 | `			int k;` |
|    45 | 2915 | `			for( k = 0 ; k < nWarn ; k++ ){` |
|     7 | 2916 | `				ph7_value_int64(pV,aWarnPos[k]);` |
|     7 | 2917 | `				ph7_array_add_elem(pArr,0,pV);` |
|     7 | 2918 | `				ph7_value_string(pV,aWarnMsg[k],-1);` |
|     7 | 2919 | `				ph7_array_add_elem(pArr,0,pV);` |
|     4 | 2920 | `			}` |
|     - | 2921 | `		}` |
|    39 | 2922 | `		ph7_result_value(pCtx,pArr);` |
|     - | 2923 | `	}` |
|    39 | 2924 | `	return PH7_OK;` |
|    23 | 2925 | `}` |
|     - | 2926 | `/*` |
|     - | 2927 | ` * The embedded DateTime library. Timezone scope: UTC + fixed offsets.` |
|     - | 2928 | ` */` |
|     - | 2929 | `static const char zDateTimeLib[] =` |
|     - | 2930 | `"class DateException extends Exception {}"` |
|     - | 2931 | `"class DateMalformedStringException extends DateException {}"` |
|     - | 2932 | `"class DateInvalidTimeZoneException extends DateException {}"` |
|     - | 2933 | `"class DateMalformedIntervalStringException extends DateException {}"` |
|     - | 2934 | `"class DateMalformedPeriodStringException extends DateException {}"` |
|     - | 2935 | `"interface DateTimeInterface {"` |
|     - | 2936 | `" const ATOM = 'Y-m-d\\TH:i:sP';"` |
|     - | 2937 | `" const COOKIE = 'l, d-M-Y H:i:s T';"` |
|     - | 2938 | `" const ISO8601 = 'Y-m-d\\TH:i:sO';"` |
|     - | 2939 | `" const ISO8601_EXPANDED = 'X-m-d\\TH:i:sP';"` |
|     - | 2940 | `" const RFC822 = 'D, d M y H:i:s O';"` |
|     - | 2941 | `" const RFC850 = 'l, d-M-y H:i:s T';"` |
|     - | 2942 | `" const RFC1036 = 'D, d M y H:i:s O';"` |
|     - | 2943 | `" const RFC1123 = 'D, d M Y H:i:s O';"` |
|     - | 2944 | `" const RFC7231 = 'D, d M Y H:i:s \\G\\M\\T';"` |
|     - | 2945 | `" const RFC2822 = 'D, d M Y H:i:s O';"` |
|     - | 2946 | `" const RFC3339 = 'Y-m-d\\TH:i:sP';"` |
|     - | 2947 | `" const RFC3339_EXTENDED = 'Y-m-d\\TH:i:s.vP';"` |
|     - | 2948 | `" const RSS = 'D, d M Y H:i:s O';"` |
|     - | 2949 | `" const W3C = 'Y-m-d\\TH:i:sP';"` |
|     - | 2950 | `"}"` |
|     - | 2951 | `"class DateTimeZone {"` |
|     - | 2952 | `" private $__dtzOff = 0;"` |
|     - | 2953 | `" private $__dtzName = 'UTC';"` |
|     - | 2954 | `" public function __construct($timezone = 'UTC'){"` |
|     - | 2955 | `"  $tz = (string)$timezone;"` |
|     - | 2956 | `"  if( strcasecmp($tz, 'UTC') === 0 ){"` |
|     - | 2957 | `"   $this->__dtzOff = 0; $this->__dtzName = 'UTC';"` |
|     - | 2958 | `"   return;"` |
|     - | 2959 | `"  }"` |
|     - | 2960 | `"  if( $tz === 'Z' ){"` |
|     - | 2961 | `"   $this->__dtzOff = 0; $this->__dtzName = 'Z';"` |
|     - | 2962 | `"   return;"` |
|     - | 2963 | `"  }"` |
|     - | 2964 | `"  if( strcasecmp($tz, 'GMT') === 0 ){"` |
|     - | 2965 | `"   $this->__dtzOff = 0; $this->__dtzName = 'GMT';"` |
|     - | 2966 | `"   return;"` |
|     - | 2967 | `"  }"` |
|     - | 2968 | `"  $m = null;"` |
|     - | 2969 | `"  if( preg_match('/^([+-])(\\d{2}):?(\\d{2})$/', $tz, $m) ){"` |
|     - | 2970 | `"   $off = ((int)$m[2]) * 3600 + ((int)$m[3]) * 60;"` |
|     - | 2971 | `"   if( $m[1] === '-' ){ $off = -$off; }"` |
|     - | 2972 | `"   $this->__dtzOff = $off;"` |
|     - | 2973 | `"   $this->__dtzName = $m[1] . $m[2] . ':' . $m[3];"` |
|     - | 2974 | `"   return;"` |
|     - | 2975 | `"  }"` |
|     - | 2976 | `"  throw new DateInvalidTimeZoneException("` |
|     - | 2977 | `"   'DateTimeZone::__construct(): Unknown or bad timezone (' . $tz . ')');"` |
|     - | 2978 | `" }"` |
|     - | 2979 | `" public function getName(){ return $this->__dtzName; }"` |
|     - | 2980 | `" public function getOffset($datetime = null){ return $this->__dtzOff; }"` |
|     - | 2981 | `"}"` |
|     - | 2982 | `"trait __DtCoreT {"` |
|     - | 2983 | `" private $__dtTs = 0;"` |
|     - | 2984 | `" private $__dtOff = 0;"` |
|     - | 2985 | `" private $__dtName = 'UTC';"` |
|     - | 2986 | `" private function __dtInit($datetime, $timezone){"` |
|     - | 2987 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 2988 | `"  if( $timezone !== null ){"` |
|     - | 2989 | `"   $off = $timezone->getOffset($this);"` |
|     - | 2990 | `"   $name = $timezone->getName();"` |
|     - | 2991 | `"  }"` |
|     - | 2992 | `"  $r = __dt_parse((string)$datetime, __dt_now(), $off);"` |
|     - | 2993 | `"  if( is_string($r) ){ throw new DateMalformedStringException($r); }"` |
|     - | 2994 | `"  $this->__dtTs = $r[0];"` |
|     - | 2995 | `"  if( $r[2] ){"` |
|     - | 2996 | `"   $this->__dtOff = $r[1];"` |
|     - | 2997 | `"   $this->__dtName = $r[2] === 2 ? 'Z' : $this->__dtOffName($r[1]);"` |
|     - | 2998 | `"  }else{"` |
|     - | 2999 | `"   $this->__dtOff = $off;"` |
|     - | 3000 | `"   $this->__dtName = $name;"` |
|     - | 3001 | `"  }"` |
|     - | 3002 | `" }"` |
|     - | 3003 | `" private function __dtOffName($off){"` |
|     - | 3004 | `"  $s = $off < 0 ? '-' : '+';"` |
|     - | 3005 | `"  $a = $off < 0 ? -$off : $off;"` |
|     - | 3006 | `"  return $s . sprintf('%02d:%02d', intdiv($a, 3600), intdiv($a % 3600, 60));"` |
|     - | 3007 | `" }"` |
|     - | 3008 | `" public function format($format){ return __dt_format($this->__dtTs, $this->__dtOff, $this->__dtName, (string)$format); }"` |
|     - | 3009 | `" public function getTimestamp(){ return $this->__dtTs; }"` |
|     - | 3010 | `" public function getOffset(){ return $this->__dtOff; }"` |
|     - | 3011 | `" public function getTimezone(){ return new DateTimeZone($this->__dtName); }"` |
|     - | 3012 | `" public function diff($targetObject, $absolute = false){"` |
|     - | 3013 | `"  $r = __dt_civil_diff($this->__dtTs, $this->__dtOff, $targetObject->getTimestamp());"` |
|     - | 3014 | `"  $iv = new DateInterval('P0D');"` |
|     - | 3015 | `"  $iv->y = $r[0]; $iv->m = $r[1]; $iv->d = $r[2];"` |
|     - | 3016 | `"  $iv->h = $r[3]; $iv->i = $r[4]; $iv->s = $r[5];"` |
|     - | 3017 | `"  $iv->days = $r[6];"` |
|     - | 3018 | `"  $iv->invert = $absolute ? 0 : $r[7];"` |
|     - | 3019 | `"  return $iv;"` |
|     - | 3020 | `" }"` |
|     - | 3021 | `" private function __dtAddTs($interval, $sign){"` |
|     - | 3022 | `"  if( $interval->invert ){ $sign = -$sign; }"` |
|     - | 3023 | `"  return __dt_civil_add($this->__dtTs, $this->__dtOff, $interval->y, $interval->m,"` |
|     - | 3024 | `"   $interval->d, $interval->h, $interval->i, $interval->s, $sign);"` |
|     - | 3025 | `" }"` |
|     - | 3026 | `" private static function __dtFromFormat($format, $datetime, $timezone, $class){"` |
|     - | 3027 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 3028 | `"  if( $timezone !== null ){"` |
|     - | 3029 | `"   $off = $timezone->getOffset(null);"` |
|     - | 3030 | `"   $name = $timezone->getName();"` |
|     - | 3031 | `"  }"` |
|     - | 3032 | `"  $r = __dt_from_format((string)$format, (string)$datetime, __dt_now(), $off);"` |
|     - | 3033 | `"  if( is_string($r) ){"` |
|     - | 3034 | `"   $lines = explode(\"\\n\", $r);"` |
|     - | 3035 | `"   $errs = [];"` |
|     - | 3036 | `"   $nl = count($lines);"` |
|     - | 3037 | `"   for( $k = 1; $k < $nl; $k++ ){"` |
|     - | 3038 | `"    $p = strpos($lines[$k], \"\\t\");"` |
|     - | 3039 | `"    $errs[(int)substr($lines[$k], 0, $p)] = substr($lines[$k], $p + 1);"` |
|     - | 3040 | `"   }"` |
|     - | 3041 | `"   DateTime::$__dtLastErr = ['warning_count' => 0, 'warnings' => [],"` |
|     - | 3042 | `"    'error_count' => (int)$lines[0], 'errors' => $errs];"` |
|     - | 3043 | `"   return false;"` |
|     - | 3044 | `"  }"` |
|     - | 3045 | `"  if( isset($r[4]) ){"` |
|     - | 3046 | `"   $warns = [];"` |
|     - | 3047 | `"   $wc = 0;"` |
|     - | 3048 | `"   for( $k = 4; isset($r[$k]); $k += 2 ){"` |
|     - | 3049 | `"    $warns[$r[$k]] = $r[$k + 1];"` |
|     - | 3050 | `"    $wc++;"` |
|     - | 3051 | `"   }"` |
|     - | 3052 | `"   DateTime::$__dtLastErr = ['warning_count' => $wc, 'warnings' => $warns,"` |
|     - | 3053 | `"    'error_count' => 0, 'errors' => []];"` |
|     - | 3054 | `"  }else{"` |
|     - | 3055 | `"   DateTime::$__dtLastErr = false;"` |
|     - | 3056 | `"  }"` |
|     - | 3057 | `"  $obj = new $class('@0');"` |
|     - | 3058 | `"  $obj->__dtTs = $r[0];"` |
|     - | 3059 | `"  if( $r[2] === 0 ){ $obj->__dtOff = $off; $obj->__dtName = $name; }"` |
|     - | 3060 | `"  elseif( $r[2] === 2 ){ $obj->__dtOff = 0; $obj->__dtName = 'Z'; }"` |
|     - | 3061 | `"  elseif( $r[2] === 3 ){ $obj->__dtOff = $r[1]; $obj->__dtName = $r[3]; }"` |
|     - | 3062 | `"  else { $obj->__dtOff = $r[1]; $obj->__dtName = $obj->__dtOffName($r[1]); }"` |
|     - | 3063 | `"  return $obj;"` |
|     - | 3064 | `" }"` |
|     - | 3065 | `" private static function __dtCopyOf($object, $class){"` |
|     - | 3066 | `"  $d = new $class('@0');"` |
|     - | 3067 | `"  $d->__dtTs = $object->getTimestamp();"` |
|     - | 3068 | `"  $d->__dtOff = $object->getOffset();"` |
|     - | 3069 | `"  $d->__dtName = $object->getTimezone()->getName();"` |
|     - | 3070 | `"  return $d;"` |
|     - | 3071 | `" }"` |
|     - | 3072 | `"}"` |
|     - | 3073 | `"class DateTime implements DateTimeInterface {"` |
|     - | 3074 | `" use __DtCoreT;"` |
|     - | 3075 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 3076 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 3077 | `" }"` |
|     - | 3078 | `" public function modify($modifier){"` |
|     - | 3079 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 3080 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTime::modify(): ' . $r); }"` |
|     - | 3081 | `"  $this->__dtTs = $r[0];"` |
|     - | 3082 | `"  return $this;"` |
|     - | 3083 | `" }"` |
|     - | 3084 | `" public function setTimestamp($timestamp){ $this->__dtTs = (int)$timestamp; return $this; }"` |
|     - | 3085 | `" public function setTimezone($timezone){"` |
|     - | 3086 | `"  $this->__dtOff = $timezone->getOffset($this);"` |
|     - | 3087 | `"  $this->__dtName = $timezone->getName();"` |
|     - | 3088 | `"  return $this;"` |
|     - | 3089 | `" }"` |
|     - | 3090 | `" public function setDate($year, $month, $day){"` |
|     - | 3091 | `"  $this->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 3092 | `"  return $this;"` |
|     - | 3093 | `" }"` |
|     - | 3094 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3095 | `"  $this->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 3096 | `"  return $this;"` |
|     - | 3097 | `" }"` |
|     - | 3098 | `" public function add($interval){ $this->__dtTs = $this->__dtAddTs($interval, 1); return $this; }"` |
|     - | 3099 | `" public function sub($interval){ $this->__dtTs = $this->__dtAddTs($interval, -1); return $this; }"` |
|     - | 3100 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 3101 | `"  $this->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 3102 | `"  return $this;"` |
|     - | 3103 | `" }"` |
|     - | 3104 | `" public static $__dtLastErr = false;"` |
|     - | 3105 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 3106 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 3107 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTime');"` |
|     - | 3108 | `" }"` |
|     - | 3109 | `" public static function createFromImmutable($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 3110 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 3111 | `"}"` |
|     - | 3112 | `"class DateTimeImmutable implements DateTimeInterface {"` |
|     - | 3113 | `" use __DtCoreT;"` |
|     - | 3114 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 3115 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 3116 | `" }"` |
|     - | 3117 | `" public function modify($modifier){"` |
|     - | 3118 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 3119 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTimeImmutable::modify(): ' . $r); }"` |
|     - | 3120 | `"  $c = clone $this;"` |
|     - | 3121 | `"  $c->__dtTs = $r[0];"` |
|     - | 3122 | `"  return $c;"` |
|     - | 3123 | `" }"` |
|     - | 3124 | `" public function setTimestamp($timestamp){ $c = clone $this; $c->__dtTs = (int)$timestamp; return $c; }"` |
|     - | 3125 | `" public function setTimezone($timezone){"` |
|     - | 3126 | `"  $c = clone $this;"` |
|     - | 3127 | `"  $c->__dtOff = $timezone->getOffset($this);"` |
|     - | 3128 | `"  $c->__dtName = $timezone->getName();"` |
|     - | 3129 | `"  return $c;"` |
|     - | 3130 | `" }"` |
|     - | 3131 | `" public function setDate($year, $month, $day){"` |
|     - | 3132 | `"  $c = clone $this;"` |
|     - | 3133 | `"  $c->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 3134 | `"  return $c;"` |
|     - | 3135 | `" }"` |
|     - | 3136 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3137 | `"  $c = clone $this;"` |
|     - | 3138 | `"  $c->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 3139 | `"  return $c;"` |
|     - | 3140 | `" }"` |
|     - | 3141 | `" public function add($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, 1); return $c; }"` |
|     - | 3142 | `" public function sub($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, -1); return $c; }"` |
|     - | 3143 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 3144 | `"  $c = clone $this;"` |
|     - | 3145 | `"  $c->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 3146 | `"  return $c;"` |
|     - | 3147 | `" }"` |
|     - | 3148 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 3149 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 3150 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTimeImmutable');"` |
|     - | 3151 | `" }"` |
|     - | 3152 | `" public static function createFromMutable($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 3153 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 3154 | `"}"` |
|     - | 3155 | `"function date_create($datetime = 'now', $timezone = null){"` |
|     - | 3156 | `" try { return new DateTime($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 3157 | `"}"` |
|     - | 3158 | `"function date_create_immutable($datetime = 'now', $timezone = null){"` |
|     - | 3159 | `" try { return new DateTimeImmutable($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 3160 | `"}"` |
|     - | 3161 | `"class DateInterval {"` |
|     - | 3162 | `" public $y = 0;"` |
|     - | 3163 | `" public $m = 0;"` |
|     - | 3164 | `" public $d = 0;"` |
|     - | 3165 | `" public $h = 0;"` |
|     - | 3166 | `" public $i = 0;"` |
|     - | 3167 | `" public $s = 0;"` |
|     - | 3168 | `" public $f = 0;"` |
|     - | 3169 | `" public $invert = 0;"` |
|     - | 3170 | `" public $days = false;"` |
|     - | 3171 | `" public $from_string = false;"` |
|     - | 3172 | `" public function __construct($duration = 'P0D'){"` |
|     - | 3173 | `"  $dur = (string)$duration;"` |
|     - | 3174 | `"  $mm = null;"` |
|     - | 3175 | `"  if( strlen($dur) < 2 \|\| substr($dur, -1) === 'T'"` |
|     - | 3176 | `"   \|\| !preg_match('/^P(?:(\\d+)Y)?(?:(\\d+)M)?(?:(\\d+)W)?(?:(\\d+)D)?(?:T(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?)?$/', $dur, $mm) ){"` |
|     - | 3177 | `"   throw new DateMalformedIntervalStringException('Unknown or bad format (' . $dur . ')');"` |
|     - | 3178 | `"  }"` |
|     - | 3179 | `"  $this->y = (int)($mm[1] ?? 0);"` |
|     - | 3180 | `"  $this->m = (int)($mm[2] ?? 0);"` |
|     - | 3181 | `"  $this->d = (int)($mm[4] ?? 0) + 7 * (int)($mm[3] ?? 0);"` |
|     - | 3182 | `"  $this->h = (int)($mm[5] ?? 0);"` |
|     - | 3183 | `"  $this->i = (int)($mm[6] ?? 0);"` |
|     - | 3184 | `"  $this->s = (int)($mm[7] ?? 0);"` |
|     - | 3185 | `" }"` |
|     - | 3186 | `" public static function createFromDateString($datetime){"` |
|     - | 3187 | `"  $s = trim((string)$datetime);"` |
|     - | 3188 | `"  $iv = new DateInterval('P0D');"` |
|     - | 3189 | `"  $rest = $s;"` |
|     - | 3190 | `"  $any = false;"` |
|     - | 3191 | `"  while( $rest !== '' ){"` |
|     - | 3192 | `"   $mm = null;"` |
|     - | 3193 | `"   if( !preg_match('/^[\\s,+]*([+-]?\\d+)\\s*(sec\|secs\|second\|seconds\|min\|mins\|minute\|minutes\|hour\|hours\|day\|days\|week\|weeks\|fortnight\|fortnights\|month\|months\|year\|years)\\b/i', $rest, $mm) ){"` |
|     - | 3194 | `"    throw new DateMalformedIntervalStringException("` |
|     - | 3195 | `"     'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 3196 | `"   }"` |
|     - | 3197 | `"   $n = (int)$mm[1];"` |
|     - | 3198 | `"   $u = strtolower($mm[2]);"` |
|     - | 3199 | `"   if( $u === 'sec' \|\| $u === 'secs' \|\| $u === 'second' \|\| $u === 'seconds' ){ $iv->s += $n; }"` |
|     - | 3200 | `"   elseif( $u === 'min' \|\| $u === 'mins' \|\| $u === 'minute' \|\| $u === 'minutes' ){ $iv->i += $n; }"` |
|     - | 3201 | `"   elseif( $u === 'hour' \|\| $u === 'hours' ){ $iv->h += $n; }"` |
|     - | 3202 | `"   elseif( $u === 'day' \|\| $u === 'days' ){ $iv->d += $n; }"` |
|     - | 3203 | `"   elseif( $u === 'week' \|\| $u === 'weeks' ){ $iv->d += 7 * $n; }"` |
|     - | 3204 | `"   elseif( $u === 'fortnight' \|\| $u === 'fortnights' ){ $iv->d += 14 * $n; }"` |
|     - | 3205 | `"   elseif( $u === 'month' \|\| $u === 'months' ){ $iv->m += $n; }"` |
|     - | 3206 | `"   else { $iv->y += $n; }"` |
|     - | 3207 | `"   $any = true;"` |
|     - | 3208 | `"   $rest = ltrim(substr($rest, strlen($mm[0])));"` |
|     - | 3209 | `"  }"` |
|     - | 3210 | `"  if( !$any ){"` |
|     - | 3211 | `"   throw new DateMalformedIntervalStringException("` |
|     - | 3212 | `"    'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 3213 | `"  }"` |
|     - | 3214 | `"  return $iv;"` |
|     - | 3215 | `" }"` |
|     - | 3216 | `" public function format($format){"` |
|     - | 3217 | `"  $f = (string)$format;"` |
|     - | 3218 | `"  $out = '';"` |
|     - | 3219 | `"  $n = strlen($f);"` |
|     - | 3220 | `"  for( $k = 0; $k < $n; $k++ ){"` |
|     - | 3221 | `"   $c = $f[$k];"` |
|     - | 3222 | `"   if( $c !== '%' ){ $out .= $c; continue; }"` |
|     - | 3223 | `"   $k++;"` |
|     - | 3224 | `"   if( $k >= $n ){ $out .= '%'; break; }"` |
|     - | 3225 | `"   $t = $f[$k];"` |
|     - | 3226 | `"   if( $t === 'Y' ){ $out .= sprintf('%02d', $this->y); }"` |
|     - | 3227 | `"   elseif( $t === 'y' ){ $out .= $this->y; }"` |
|     - | 3228 | `"   elseif( $t === 'M' ){ $out .= sprintf('%02d', $this->m); }"` |
|     - | 3229 | `"   elseif( $t === 'm' ){ $out .= $this->m; }"` |
|     - | 3230 | `"   elseif( $t === 'D' ){ $out .= sprintf('%02d', $this->d); }"` |
|     - | 3231 | `"   elseif( $t === 'd' ){ $out .= $this->d; }"` |
|     - | 3232 | `"   elseif( $t === 'H' ){ $out .= sprintf('%02d', $this->h); }"` |
|     - | 3233 | `"   elseif( $t === 'h' ){ $out .= $this->h; }"` |
|     - | 3234 | `"   elseif( $t === 'I' ){ $out .= sprintf('%02d', $this->i); }"` |
|     - | 3235 | `"   elseif( $t === 'i' ){ $out .= $this->i; }"` |
|     - | 3236 | `"   elseif( $t === 'S' ){ $out .= sprintf('%02d', $this->s); }"` |
|     - | 3237 | `"   elseif( $t === 's' ){ $out .= $this->s; }"` |
|     - | 3238 | `"   elseif( $t === 'F' ){ $out .= sprintf('%06d', (int)round($this->f * 1000000)); }"` |
|     - | 3239 | `"   elseif( $t === 'f' ){ $out .= (int)round($this->f * 1000000); }"` |
|     - | 3240 | `"   elseif( $t === 'R' ){ $out .= $this->invert ? '-' : '+'; }"` |
|     - | 3241 | `"   elseif( $t === 'r' ){ $out .= $this->invert ? '-' : ''; }"` |
|     - | 3242 | `"   elseif( $t === 'a' ){ $out .= $this->days === false ? '(unknown)' : $this->days; }"` |
|     - | 3243 | `"   elseif( $t === '%' ){ $out .= '%'; }"` |
|     - | 3244 | `"   else { $out .= $t; }"` |
|     - | 3245 | `"  }"` |
|     - | 3246 | `"  return $out;"` |
|     - | 3247 | `" }"` |
|     - | 3248 | `"}"` |
|     - | 3249 | `"class DatePeriod implements IteratorAggregate {"` |
|     - | 3250 | `" const EXCLUDE_START_DATE = 1;"` |
|     - | 3251 | `" const INCLUDE_END_DATE = 2;"` |
|     - | 3252 | `" public $start = null;"` |
|     - | 3253 | `" public $current = null;"` |
|     - | 3254 | `" public $end = null;"` |
|     - | 3255 | `" public $interval = null;"` |
|     - | 3256 | `" public $recurrences = 1;"` |
|     - | 3257 | `" public $include_start_date = true;"` |
|     - | 3258 | `" public $include_end_date = false;"` |
|     - | 3259 | `" private $__dpN = null;"` |
|     - | 3260 | `" public function __construct($start, $interval = null, $end = null, $options = 0){"` |
|     - | 3261 | `"  if( is_string($start) ){"` |
|     - | 3262 | `"   $mm = null;"` |
|     - | 3263 | `"   if( !preg_match('/^R(\\d+)\\/(.+)\\/(P.+)$/', $start, $mm) ){"` |
|     - | 3264 | `"    throw new DateMalformedPeriodStringException("` |
|     - | 3265 | `"     'DatePeriod::__construct(): Unknown or bad format (' . $start . ')');"` |
|     - | 3266 | `"   }"` |
|     - | 3267 | `"   $options = is_int($interval) ? $interval : 0;"` |
|     - | 3268 | `"   $this->start = new DateTimeImmutable($mm[2]);"` |
|     - | 3269 | `"   $this->interval = new DateInterval($mm[3]);"` |
|     - | 3270 | `"   $this->__dpN = (int)$mm[1];"` |
|     - | 3271 | `"   $this->recurrences = $this->__dpN + 1;"` |
|     - | 3272 | `"  }else{"` |
|     - | 3273 | `"   $this->start = clone $start;"` |
|     - | 3274 | `"   $this->interval = $interval;"` |
|     - | 3275 | `"   if( is_int($end) ){"` |
|     - | 3276 | `"    $this->__dpN = $end;"` |
|     - | 3277 | `"    $this->recurrences = $end + 1;"` |
|     - | 3278 | `"   }else{"` |
|     - | 3279 | `"    $this->end = $end === null ? null : (clone $end);"` |
|     - | 3280 | `"   }"` |
|     - | 3281 | `"  }"` |
|     - | 3282 | `"  $this->include_start_date = !((int)$options & 1);"` |
|     - | 3283 | `"  $this->include_end_date = ((int)$options & 2) !== 0;"` |
|     - | 3284 | `" }"` |
|     - | 3285 | `" public static function createFromISO8601String($specification, $options = 0){"` |
|     - | 3286 | `"  return new DatePeriod((string)$specification, (int)$options);"` |
|     - | 3287 | `" }"` |
|     - | 3288 | `" public function getStartDate(){ return $this->start; }"` |
|     - | 3289 | `" public function getEndDate(){ return $this->end; }"` |
|     - | 3290 | `" public function getDateInterval(){ return $this->interval; }"` |
|     - | 3291 | `" public function getRecurrences(){ return $this->__dpN; }"` |
|     - | 3292 | `" public function getIterator(): Generator {"` |
|     - | 3293 | `"  $cur = $this->start;"` |
|     - | 3294 | `"  $iv = $this->interval;"` |
|     - | 3295 | `"  $k = 0;"` |
|     - | 3296 | `"  if( $this->end !== null ){"` |
|     - | 3297 | `"   $endTs = $this->end->getTimestamp();"` |
|     - | 3298 | `"   $first = true;"` |
|     - | 3299 | `"   while( true ){"` |
|     - | 3300 | `"    $ts = $cur->getTimestamp();"` |
|     - | 3301 | `"    if( $this->include_end_date ? ($ts > $endTs) : ($ts >= $endTs) ){ break; }"` |
|     - | 3302 | `"    if( !$first \|\| $this->include_start_date ){"` |
|     - | 3303 | `"     yield $k => (clone $cur);"` |
|     - | 3304 | `"     $k++;"` |
|     - | 3305 | `"    }"` |
|     - | 3306 | `"    $first = false;"` |
|     - | 3307 | `"    $next = clone $cur;"` |
|     - | 3308 | `"    $cur = $next->add($iv);"` |
|     - | 3309 | `"   }"` |
|     - | 3310 | `"   return;"` |
|     - | 3311 | `"  }"` |
|     - | 3312 | `"  $total = $this->__dpN + 1 + ($this->include_end_date ? 1 : 0);"` |
|     - | 3313 | `"  for( $j = 0; $j < $total; $j++ ){"` |
|     - | 3314 | `"   if( $j > 0 \|\| $this->include_start_date ){"` |
|     - | 3315 | `"    yield $k => (clone $cur);"` |
|     - | 3316 | `"    $k++;"` |
|     - | 3317 | `"   }"` |
|     - | 3318 | `"   $next = clone $cur;"` |
|     - | 3319 | `"   $cur = $next->add($iv);"` |
|     - | 3320 | `"  }"` |
|     - | 3321 | `" }"` |
|     - | 3322 | `"}"` |
|     - | 3323 | `"function date_format($object, $format){ return $object->format($format); }"` |
|     - | 3324 | `"function date_modify($object, $modifier){"` |
|     - | 3325 | `" try { return $object->modify($modifier); } catch (Exception $e) { return false; }"` |
|     - | 3326 | `"}"` |
|     - | 3327 | `"function date_add($object, $interval){ return $object->add($interval); }"` |
|     - | 3328 | `"function date_sub($object, $interval){ return $object->sub($interval); }"` |
|     - | 3329 | `"function date_diff($baseObject, $targetObject, $absolute = false){"` |
|     - | 3330 | `" return $baseObject->diff($targetObject, $absolute);"` |
|     - | 3331 | `"}"` |
|     - | 3332 | `"function date_timestamp_get($object){ return $object->getTimestamp(); }"` |
|     - | 3333 | `"function date_timestamp_set($object, $timestamp){ return $object->setTimestamp($timestamp); }"` |
|     - | 3334 | `"function date_timezone_get($object){ return $object->getTimezone(); }"` |
|     - | 3335 | `"function date_timezone_set($object, $timezone){ return $object->setTimezone($timezone); }"` |
|     - | 3336 | `"function date_offset_get($object){ return $object->getOffset(); }"` |
|     - | 3337 | `"function date_date_set($object, $year, $month, $day){ return $object->setDate($year, $month, $day); }"` |
|     - | 3338 | `"function date_time_set($object, $hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3339 | `" return $object->setTime($hour, $minute, $second, $microsecond);"` |
|     - | 3340 | `"}"` |
|     - | 3341 | `"function date_isodate_set($object, $year, $week, $dayOfWeek = 1){"` |
|     - | 3342 | `" return $object->setISODate($year, $week, $dayOfWeek);"` |
|     - | 3343 | `"}"` |
|     - | 3344 | `"function date_interval_create_from_date_string($datetime){"` |
|     - | 3345 | `" return DateInterval::createFromDateString($datetime);"` |
|     - | 3346 | `"}"` |
|     - | 3347 | `"function date_interval_format($object, $format){ return $object->format($format); }"` |
|     - | 3348 | `"function date_get_last_errors(){ return DateTime::getLastErrors(); }"` |
|     - | 3349 | `"function timezone_open($timezone){"` |
|     - | 3350 | `" try { return new DateTimeZone($timezone); } catch (Exception $e) { return false; }"` |
|     - | 3351 | `"}"` |
|     - | 3352 | `"function timezone_name_get($object){ return $object->getName(); }"` |
|     - | 3353 | `"function timezone_offset_get($object, $datetime){ return $object->getOffset($datetime); }"` |
|     - | 3354 | `/* int\|false strtotime(string $datetime, ?int $baseTimestamp = null). Rides the` |
|     - | 3355 | ` * same DtParse the DateTime constructor uses, so its format coverage is identical.` |
|     - | 3356 | ` * php: the EMPTY string is false, but whitespace-only is 'now'; a parse failure is` |
|     - | 3357 | ` * false (never an exception). The default timezone is treated as offset 0, exactly` |
|     - | 3358 | ` * as the DateTime constructor does for a null $timezone. */` |
|     - | 3359 | `"function strtotime($datetime, $baseTimestamp = null){"` |
|     - | 3360 | `" $s = (string)$datetime;"` |
|     - | 3361 | `" if( $s === '' ){ return false; }"` |
|     - | 3362 | `" $base = $baseTimestamp === null ? __dt_now() : (int)$baseTimestamp;"` |
|     - | 3363 | `" $r = __dt_parse($s, $base, 0);"` |
|     - | 3364 | `" return is_string($r) ? false : $r[0];"` |
|     - | 3365 | `"}"` |
|     - | 3366 | `;` |
|     - | 3367 | `/*` |
|     - | 3368 | ` * Install the DateTime family: thunks first, then the chunk. Called from` |
|     - | 3369 | ` * PH7_VmInit inside the bCompilingBuiltin window, after the Reflection` |
|     - | 3370 | ` * install (Exception must exist).` |
|     - | 3371 | ` */` |
|  3812 | 3372 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
|     5 | 3373 | `{` |
|     - | 3374 | `	static const struct {` |
|     - | 3375 | `		const char *zName;` |
|     - | 3376 | `		ProchHostFunction xFunc;` |
|     - | 3377 | `	} aFunc[] = {` |
|     - | 3378 | `		{ "__dt_now",    vm_builtin_dt_now },` |
|     - | 3379 | `		{ "__dt_default_tz", vm_builtin_dt_default_tz },` |
|     - | 3380 | `		{ "__dt_civil_add",  vm_builtin_dt_civil_add },` |
|     - | 3381 | `		{ "__dt_civil_diff", vm_builtin_dt_civil_diff },` |
|     - | 3382 | `		{ "__dt_isodate",    vm_builtin_dt_isodate },` |
|     - | 3383 | `		{ "__dt_from_format", vm_builtin_dt_from_format },` |
|     - | 3384 | `		{ "__dt_parse",  vm_builtin_dt_parse },` |
|     - | 3385 | `		{ "__dt_format", vm_builtin_dt_format },` |
|     - | 3386 | `		{ "__dt_make",   vm_builtin_dt_make },` |
|     - | 3387 | `	};` |
|     - | 3388 | `	sxu32 n;` |
|     - | 3389 | `	/* php's date.timezone default */` |
|  3817 | 3390 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|  3817 | 3391 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
| 38125 | 3392 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 34313 | 3393 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 17159 | 3394 | `	}` |
|  3817 | 3395 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zDateTimeLib,sizeof(zDateTimeLib)-1);` |
|     5 | 3396 | `}` |
|     - | 3397 |  |
|     - | 3398 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 3399 |  |
|     - | 3400 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 3401 | `/* Tiny build: no DateTime family (builtin layer disabled) */` |
|     - | 3402 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){` |
|     - | 3403 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|     - | 3404 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|     - | 3405 | `	return SXRET_OK;` |
|     - | 3406 | `}` |
|     - | 3407 | `#endif` |
|     - | 3408 |  |
