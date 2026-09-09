# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1504/1915 lines (78.54%)

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
|   210 |   24 | `static void DtSytmFillOffset(Sytm *pSTm,time_t t)` |
|     1 |   25 | `{` |
|   316 |   26 | `	sxi64 iCivil = DtDaysFromCivil((sxi64)pSTm->tm_year,pSTm->tm_mon+1,pSTm->tm_mday) * 86400` |
|   210 |   27 | `		+ (sxi64)pSTm->tm_hour*3600 + (sxi64)pSTm->tm_min*60 + (sxi64)pSTm->tm_sec;` |
|   211 |   28 | `	pSTm->tm_gmtoff = (long)(iCivil - (sxi64)t);` |
|   211 |   29 | `}` |
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
|   278 |  416 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|     1 |  417 | `{` |
|   279 |  418 | `	const char *zEnd = &zIn[nLen];` |
|     - |  419 | `	const char *zCur;` |
|     - |  420 | `	/* Start the format process */` |
|  1090 |  421 | `	for(;;){` |
|  2181 |  422 | `		if( zIn >= zEnd ){` |
|     - |  423 | `			/* No more input to process */` |
|   279 |  424 | `			break;` |
|     - |  425 | `		}` |
|  1903 |  426 | `		switch(zIn[0]){` |
|   104 |  427 | `		case 'd':` |
|     - |  428 | `			/* Day of the month, 2 digits with leading zeros */` |
|   209 |  429 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|   209 |  430 | `			break;` |
|    31 |  431 | `		case 'D':` |
|     - |  432 | `			/*A textual representation of a day, three letters*/` |
|    63 |  433 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    63 |  434 | `			ph7_result_string(pCtx,zCur,3);` |
|    63 |  435 | `			break;` |
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
|   104 |  463 | `		case 'm':` |
|     - |  464 | `			/*Numeric representation of a month, with leading zeros*/` |
|   209 |  465 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|   209 |  466 | `			break;` |
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
|    91 |  510 | `		case 'Y':` |
|     - |  511 | `			/*	A full numeric representation of a year, 4 digits */` |
|   183 |  512 | `			ph7_result_string_format(pCtx,"%04d",pTm->tm_year);` |
|   183 |  513 | `			break;` |
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
|    62 |  566 | `		case 'H':` |
|     - |  567 | `			/*	24-hour format of an hour with leading zeros */` |
|   125 |  568 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|   125 |  569 | `			break;` |
|    63 |  570 | `		case 'i':` |
|     - |  571 | `			/* 	Minutes with leading zeros */` |
|   127 |  572 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|   127 |  573 | `			break;` |
|    63 |  574 | `		case 's':` |
|     - |  575 | `			/* 	second with leading zeros */` |
|   127 |  576 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|   127 |  577 | `			break;` |
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
|   407 |  711 | `		default:` |
|     - |  712 | `			/* Unknown format specifer,expand verbatim */` |
|   815 |  713 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|   814 |  714 | `			break;` |
|     - |  715 | `		}` |
|     - |  716 | `		/* Point to the next character */` |
|  1903 |  717 | `		zIn++;` |
|     1 |  718 | `	}` |
|   279 |  719 | `	return SXRET_OK;` |
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
|   104 |  970 | `static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)` |
|     1 |  971 | `{` |
|     - |  972 | `	char zBuf[64];` |
|   105 |  973 | `	*pbUseNow = 0;` |
|   105 |  974 | `	if( ph7_value_is_null(pArg) ){` |
|     3 |  975 | `		*pbUseNow = 1;` |
|     3 |  976 | `		return PH7_OK;` |
|     - |  977 | `	}` |
|   103 |  978 | `	if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_float(pArg) ){` |
|    85 |  979 | `		*pT = (time_t)ph7_value_to_int64(pArg);` |
|    85 |  980 | `		return PH7_OK;` |
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
|    53 | 1000 | `}` |
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
|   102 | 1152 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1153 | `{` |
|     - | 1154 | `	const char *zFormat;` |
|     - | 1155 | `	int nLen;` |
|     - | 1156 | `	Sytm sTm;` |
|   103 | 1157 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1158 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1159 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1160 | `		return PH7_OK;` |
|     - | 1161 | `	}` |
|   103 | 1162 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   103 | 1163 | `	if( nLen < 1 ){` |
|     - | 1164 | `		/* Don't bother processing return the empty string */` |
|   ! 0 | 1165 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1166 | `	}` |
|   103 | 1167 | `	if( nArg < 2 ){` |
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
|    89 | 1182 | `		time_t t = 0;` |
|     - | 1183 | `		struct tm *pTm;` |
|     - | 1184 | `		int bUseNow;` |
|    89 | 1185 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|    89 | 1186 | `		if( rc != PH7_OK ){` |
|     3 | 1187 | `			return rc;` |
|     - | 1188 | `		}` |
|    87 | 1189 | `		if( bUseNow ){` |
|     3 | 1190 | `			time(&t);` |
|     1 | 1191 | `		}` |
|    87 | 1192 | `		pTm = gmtime(&t);` |
|    87 | 1193 | `		if( pTm == 0 ){` |
|   ! 0 | 1194 | `			time(&t);` |
|   ! 0 | 1195 | `			pTm = gmtime(&t);` |
|   ! 0 | 1196 | `		}` |
|    87 | 1197 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    87 | 1198 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1199 | `	}` |
|     - | 1200 | `	/* Format the given string */` |
|   101 | 1201 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|   101 | 1202 | `	return PH7_OK;` |
|    52 | 1203 | `}` |
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
|    20 | 1652 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1653 | `{` |
|    21 | 1654 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1655 | `	const char *zId;` |
|     - | 1656 | `	int nId;` |
|    21 | 1657 | `	if( nArg < 1 ){` |
|   ! 0 | 1658 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1659 | `		return PH7_OK;` |
|     - | 1660 | `	}` |
|    21 | 1661 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|    21 | 1662 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|    21 | 1663 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|    21 | 1664 | `		pVm->zDefTz[3] = 0;` |
|    21 | 1665 | `		pVm->nDefTz = 3;` |
|    21 | 1666 | `		ph7_result_bool(pCtx,1);` |
|    21 | 1667 | `		return PH7_OK;` |
|     - | 1668 | `	}` |
|     - | 1669 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|     - | 1670 | `	 * — exactly php's notice shape here */` |
|   ! 0 | 1671 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|   ! 0 | 1672 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 1673 | `	return PH7_OK;` |
|    11 | 1674 | `}` |
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
|   768 | 1690 | `static sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|     1 | 1691 | `{` |
|     - | 1692 | `	sxi64 era;` |
|     - | 1693 | `	unsigned yoe,doy,doe;` |
|   769 | 1694 | `	y -= (m <= 2);` |
|   769 | 1695 | `	era = (y >= 0 ? y : y - 399) / 400;` |
|   769 | 1696 | `	yoe = (unsigned)(y - era * 400);` |
|   769 | 1697 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|   769 | 1698 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|   769 | 1699 | `	return era * 146097 + (sxi64)doe - 719468;` |
|     1 | 1700 | `}` |
|   372 | 1701 | `static void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|     1 | 1702 | `{` |
|     - | 1703 | `	sxi64 era;` |
|     - | 1704 | `	unsigned doe,yoe,doy,mp;` |
|   373 | 1705 | `	z += 719468;` |
|   373 | 1706 | `	era = (z >= 0 ? z : z - 146096) / 146097;` |
|   373 | 1707 | `	doe = (unsigned)(z - era * 146097);` |
|   373 | 1708 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|   373 | 1709 | `	*py = (sxi64)yoe + era * 400;` |
|   373 | 1710 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|   373 | 1711 | `	mp = (5 * doy + 2) / 153;` |
|   373 | 1712 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|   373 | 1713 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|   373 | 1714 | `	if( *pm <= 2 ){` |
|   159 | 1715 | `		*py += 1;` |
|    79 | 1716 | `	}` |
|   373 | 1717 | `}` |
|   662 | 1718 | `static sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|     1 | 1719 | `{` |
|   663 | 1720 | `	sxi64 q = a / b;` |
|   663 | 1721 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|     3 | 1722 | `		q--;` |
|     1 | 1723 | `	}` |
|   663 | 1724 | `	return q;` |
|     1 | 1725 | `}` |
|     - | 1726 | `/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z) */` |
|   140 | 1727 | `static void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)` |
|     1 | 1728 | `{` |
|   141 | 1729 | `	sxi64 t = iTs + iOff;` |
|   141 | 1730 | `	sxi64 days = DtFloorDiv(t,86400);` |
|   141 | 1731 | `	sxi64 secs = t - days * 86400;` |
|     - | 1732 | `	sxi64 y;` |
|     - | 1733 | `	int mo,d;` |
|   141 | 1734 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|   141 | 1735 | `	pTm->tm_sec  = (int)(secs % 60);` |
|   141 | 1736 | `	pTm->tm_min  = (int)((secs / 60) % 60);` |
|   141 | 1737 | `	pTm->tm_hour = (int)(secs / 3600);` |
|   141 | 1738 | `	pTm->tm_mday = d;` |
|   141 | 1739 | `	pTm->tm_mon  = mo - 1;` |
|   141 | 1740 | `	pTm->tm_year = (int)y;` |
|   141 | 1741 | `	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */` |
|   141 | 1742 | `	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));` |
|   141 | 1743 | `	pTm->tm_isdst = 0;` |
|   141 | 1744 | `	pTm->tm_zone = zZone;` |
|   141 | 1745 | `	pTm->tm_gmtoff = (long)iOff;` |
|   141 | 1746 | `}` |
|   210 | 1747 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|     1 | 1748 | `{` |
|   211 | 1749 | `	return DtDaysFromCivil(y,mo,d) * 86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     1 | 1750 | `}` |
|     - | 1751 | `/* Month-arithmetic with php's overflow semantics (Jan 31 +1 month -> Mar 2/3):` |
|     - | 1752 | ` * normalize the month, keep the day — the civil day-count formula is linear in` |
|     - | 1753 | ` * d, so an out-of-range day simply lands in the following month. */` |
|    22 | 1754 | `static sxi64 DtAddMonths(sxi64 iTs,sxi32 iOff,sxi64 nMonths)` |
|     1 | 1755 | `{` |
|    23 | 1756 | `	sxi64 t = iTs + iOff;` |
|    23 | 1757 | `	sxi64 days = DtFloorDiv(t,86400);` |
|    23 | 1758 | `	sxi64 secs = t - days * 86400;` |
|     - | 1759 | `	sxi64 y;` |
|     - | 1760 | `	int mo,d;` |
|     - | 1761 | `	sxi64 m0;` |
|    23 | 1762 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|    23 | 1763 | `	m0 = (y * 12 + (mo - 1)) + nMonths;` |
|    23 | 1764 | `	y  = DtFloorDiv(m0,12);` |
|    23 | 1765 | `	mo = (int)(m0 - y * 12) + 1;` |
|    23 | 1766 | `	return DtDaysFromCivil(y,mo,d) * 86400 + secs - iOff;` |
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
|   236 | 1915 | `static int DtMatchMonth(const char *z,const char *zEnd,int *pAdv)` |
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
|  4805 | 1927 | `	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){` |
|  4631 | 1928 | `		int n = aM[i].n;` |
|  4630 | 1929 | `		if( zEnd - z >= n && SyStrnicmp(z,aM[i].z,(sxu32)n) == 0` |
|  2186 | 1930 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|    63 | 1931 | `			*pAdv = n;` |
|    63 | 1932 | `			return aM[i].mo;` |
|     - | 1933 | `		}` |
|  2285 | 1934 | `	}` |
|   175 | 1935 | `	return 0;` |
|   119 | 1936 | `}` |
|     - | 1937 | `/* Match a weekday name at z (full or 3-letter, case-insensitive, word boundary).` |
|     - | 1938 | ` * Returns the day-of-week 0=Sunday..6=Saturday and sets *pAdv, or -1. */` |
|   152 | 1939 | `static int DtMatchWeekday(const char *z,const char *zEnd,int *pAdv)` |
|     1 | 1940 | `{` |
|     - | 1941 | `	static const struct { const char *z; int n; int dow; } aW[] = {` |
|     - | 1942 | `		{ "sunday",6,0 },{ "monday",6,1 },{ "tuesday",7,2 },{ "wednesday",9,3 },` |
|     - | 1943 | `		{ "thursday",8,4 },{ "friday",6,5 },{ "saturday",8,6 },` |
|     - | 1944 | `		{ "sun",3,0 },{ "mon",3,1 },{ "tue",3,2 },{ "wed",3,3 },{ "thu",3,4 },` |
|     - | 1945 | `		{ "fri",3,5 },{ "sat",3,6 }` |
|     - | 1946 | `	};` |
|     - | 1947 | `	sxu32 i;` |
|  1835 | 1948 | `	for( i = 0 ; i < SX_ARRAYSIZE(aW) ; ++i ){` |
|  1727 | 1949 | `		int n = aW[i].n;` |
|  1726 | 1950 | `		if( zEnd - z >= n && SyStrnicmp(z,aW[i].z,(sxu32)n) == 0` |
|   727 | 1951 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|    45 | 1952 | `			*pAdv = n;` |
|    45 | 1953 | `			return aW[i].dow;` |
|     - | 1954 | `		}` |
|   842 | 1955 | `	}` |
|   109 | 1956 | `	return -1;` |
|    77 | 1957 | `}` |
|     - | 1958 | `/* True if z points at a two-letter English ordinal suffix (st/nd/rd/th). */` |
|    62 | 1959 | `static int DtIsOrdinal(const char *z,const char *zEnd)` |
|     1 | 1960 | `{` |
|    63 | 1961 | `	if( zEnd - z < 2 ){ return 0; }` |
|   119 | 1962 | `	return SyStrnicmp(z,"st",2) == 0 \|\| SyStrnicmp(z,"nd",2) == 0` |
|    89 | 1963 | `		\|\| SyStrnicmp(z,"rd",2) == 0 \|\| SyStrnicmp(z,"th",2) == 0;` |
|    32 | 1964 | `}` |
|     - | 1965 | `/*` |
|     - | 1966 | ` * Try to read a textual-month date at z, in either order:` |
|     - | 1967 | ` *   MonthName [Day] [Year]   ("Jan 15 2020", "January", "January 2020")` |
|     - | 1968 | ` *   Day MonthName [Year]     ("15 January 2020", "15th Jan")` |
|     - | 1969 | ` * A missing day defaults to 1, a missing year to the base timestamp's year (php).` |
|     - | 1970 | ` * Day may carry an ordinal suffix, fields may be comma-separated, month names are` |
|     - | 1971 | ` * case-insensitive, and an optional time-of-day suffix + trailing UTC/GMT is` |
|     - | 1972 | ` * consumed. Returns 0 (not a month date — caller falls through, *pzOut untouched),` |
|     - | 1973 | ` * 1 on success, or a DtParse error code (out-of-range day).` |
|     - | 1974 | ` */` |
|   188 | 1975 | `static int DtTryMonthDate(const char *z,const char *zEnd,const char **pzOut,` |
|     - | 1976 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,sxi64 iBaseTs)` |
|     1 | 1977 | `{` |
|   189 | 1978 | `	int mo,d = 1,adv,haveDay = 0,haveYear = 0;` |
|   189 | 1979 | `	sxi64 y = 0;` |
|   189 | 1980 | `	int h = 0,mi = 0,s = 0;` |
|   189 | 1981 | `	sxi32 iOff = *pOff;` |
|     - | 1982 | `	int rcT;` |
|     - | 1983 | `#define MDSKIPWS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|   189 | 1984 | `	if( (mo = DtMatchMonth(z,zEnd,&adv)) != 0 ){` |
|     - | 1985 | `		/* MonthName [Day] [Year]. A 4-digit number here is the YEAR, not the day` |
|     - | 1986 | `		 * ("January 2020" is month+year, day defaults); a 1-2 digit number is the day. */` |
|    31 | 1987 | `		z += adv;` |
|    76 | 1988 | `		MDSKIPWS();` |
|    31 | 1989 | `		if( z < zEnd && SyisDigit(z[0]) ){` |
|    31 | 1990 | `			int nrun = 0;` |
|    31 | 1991 | `			const char *zp = z;` |
|    95 | 1992 | `			while( zp < zEnd && SyisDigit(zp[0]) && nrun < 4 ){ zp++; nrun++; }` |
|    31 | 1993 | `			if( nrun < 4 ){` |
|    27 | 1994 | `				d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|    27 | 1995 | `				if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|    27 | 1996 | `				haveDay = 1;` |
|    68 | 1997 | `				MDSKIPWS();` |
|    13 | 1998 | `			}` |
|    16 | 1999 | `		}` |
|   174 | 2000 | `	}else if( SyisDigit(z[0]) ){` |
|     - | 2001 | `		/* Day MonthName [Year] */` |
|    37 | 2002 | `		d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|    37 | 2003 | `		if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|    37 | 2004 | `		haveDay = 1;` |
|    77 | 2005 | `		MDSKIPWS();` |
|    37 | 2006 | `		if( (mo = DtMatchMonth(z,zEnd,&adv)) == 0 ){ return 0; }` |
|    21 | 2007 | `		z += adv;` |
|    49 | 2008 | `		MDSKIPWS();` |
|    11 | 2009 | `	}else{` |
|   123 | 2010 | `		return 0;` |
|     - | 2011 | `	}` |
|     - | 2012 | `	/* optional year */` |
|    51 | 2013 | `	if( z < zEnd && SyisDigit(z[0]) ){` |
|    47 | 2014 | `		int ny = 0;` |
|    47 | 2015 | `		y = 0;` |
|   231 | 2016 | `		while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ y = y*10 + (z[0]-'0'); z++; ny++; }` |
|    47 | 2017 | `		if( ny <= 2 ){` |
|   ! 0 | 2018 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|   ! 0 | 2019 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|   ! 0 | 2020 | `		}` |
|    47 | 2021 | `		haveYear = 1;` |
|    23 | 2022 | `	}` |
|     - | 2023 | `	/* Default the unspecified fields from the base timestamp. php overlays: a` |
|     - | 2024 | `	 * missing year takes the base year; a missing day is 1 when a year WAS given` |
|     - | 2025 | `	 * ("January 2020" -> the 1st) but the base day when only the month was named` |
|     - | 2026 | `	 * ("January" -> the base day). */` |
|     - | 2027 | `	{` |
|     - | 2028 | `		sxi64 by; int bm,bd;` |
|    51 | 2029 | `		DtCivilFromDays(DtFloorDiv(iBaseTs + *pOff,86400),&by,&bm,&bd);` |
|    51 | 2030 | `		if( !haveYear ){ y = by; }` |
|    51 | 2031 | `		if( !haveDay ){ d = haveYear ? 1 : bd; }` |
|     - | 2032 | `	}` |
|    51 | 2033 | `	if( d > 31 ){ return (int)(z - zIn) + 1; }` |
|     - | 2034 | `	/* optional time-of-day suffix */` |
|    51 | 2035 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff);` |
|    51 | 2036 | `	if( rcT != 0 ){ return rcT; }` |
|     - | 2037 | `	/* optional trailing UTC/GMT zone name (PHL's default zone is already UTC) */` |
|    55 | 2038 | `	MDSKIPWS();` |
|    50 | 2039 | `	if( (zEnd-z >= 3 && SyStrnicmp(z,"utc",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3])))` |
|    49 | 2040 | `	 \|\| (zEnd-z >= 3 && SyStrnicmp(z,"gmt",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3]))) ){` |
|     3 | 2041 | `		iOff = 0; z += 3;` |
|     1 | 2042 | `	}` |
|    51 | 2043 | `	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    51 | 2044 | `	*pOff = iOff;` |
|    51 | 2045 | `	*pzOut = z;` |
|    51 | 2046 | `	return 1;` |
|     - | 2047 | `#undef MDSKIPWS` |
|    95 | 2048 | `}` |
|     - | 2049 | `/*` |
|     - | 2050 | ` * Minimal php-datetime-string parser (slice 1): absolute forms` |
|     - | 2051 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]",` |
|     - | 2052 | ` * keywords today/midnight/noon/tomorrow/yesterday, and relative sequences` |
|     - | 2053 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". Returns 0 on` |
|     - | 2054 | ` * success (ts/off/bOffSet out), or the byte position of the first` |
|     - | 2055 | ` * unparseable character +1 (for php's "at position N" message).` |
|     - | 2056 | ` */` |
|   420 | 2057 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,` |
|     - | 2058 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet)` |
|     1 | 2059 | `{` |
|   421 | 2060 | `	const char *z = zIn, *zEnd = &zIn[nLen];` |
|   421 | 2061 | `	sxi64 iTs = iBaseTs;` |
|   421 | 2062 | `	sxi32 iOff = iBaseOff;` |
|   421 | 2063 | `	int bOffSet = 0;` |
|   421 | 2064 | `	int bAny = 0;` |
|     - | 2065 | `	int iNumRc,iMonRc;` |
|     - | 2066 | `#define DT_SKIP_WS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|     - | 2067 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|     - | 2068 | `	&& (zEnd-z == (nKw) \|\| !SyisAlpha(z[(nKw)])))` |
|   637 | 2069 | `	DT_SKIP_WS();` |
|   421 | 2070 | `	if( z >= zEnd ){` |
|     - | 2071 | `		/* php: the empty string is "now" */` |
|     3 | 2072 | `		*pTs = iTs;` |
|     3 | 2073 | `		*pOff = iOff;` |
|     3 | 2074 | `		*pbOffSet = bOffSet;` |
|     3 | 2075 | `		return 0;` |
|     - | 2076 | `	}` |
|     - | 2077 | `	/* "@<seconds>" absolute epoch */` |
|   419 | 2078 | `	if( z[0] == '@' ){` |
|    73 | 2079 | `		int neg = 0;` |
|    73 | 2080 | `		sxi64 v = 0;` |
|    73 | 2081 | `		const char *zAt = z;` |
|    73 | 2082 | `		z++;` |
|    73 | 2083 | `		if( z < zEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|     - | 2084 | `		/* php's lexer rejects the whole token: the error points at the '@' */` |
|    73 | 2085 | `		if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zAt - zIn) + 1; }` |
|   191 | 2086 | `		while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|    71 | 2087 | `		*pTs = neg ? -v : v;` |
|    71 | 2088 | `		*pOff = 0;` |
|    71 | 2089 | `		*pbOffSet = 1;` |
|    71 | 2090 | `		DT_SKIP_WS();` |
|    71 | 2091 | `		return (z < zEnd) ? (int)(z - zIn) + 1 : 0;` |
|     - | 2092 | `	}` |
|     - | 2093 | `	/* Absolute date: YYYY-MM-DD[...] */` |
|   346 | 2094 | `	if( zEnd-z >= 10 && SyisDigit(z[0]) && SyisDigit(z[1]) && SyisDigit(z[2])` |
|   119 | 2095 | `	 && SyisDigit(z[3]) && z[4]=='-' ){` |
|    83 | 2096 | `		sxi64 y = (z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0');` |
|    83 | 2097 | `		int mo,d,h=0,mi=0,s=0;` |
|    83 | 2098 | `		if( !SyisDigit(z[5])\|\|!SyisDigit(z[6])\|\|z[7] != '-'\|\|!SyisDigit(z[8])\|\|!SyisDigit(z[9]) ){` |
|   ! 0 | 2099 | `			return (int)(z - zIn) + 1;` |
|     - | 2100 | `		}` |
|    83 | 2101 | `		mo = (z[5]-'0')*10 + (z[6]-'0');` |
|    83 | 2102 | `		d  = (z[8]-'0')*10 + (z[9]-'0');` |
|     - | 2103 | `		/* php's lexer dies on the SECOND digit of an out-of-range month/day` |
|     - | 2104 | `		 * (either the two-digit pattern fails there, or a one-digit component` |
|     - | 2105 | `		 * matched and the separator check fails there); "00" lexes fine and` |
|     - | 2106 | `		 * normalizes (month 0 == December of the previous year). */` |
|    83 | 2107 | `		if( mo > 12 ){ return (int)(&z[6] - zIn) + 1; }` |
|    77 | 2108 | `		if( d > 31 ){ return (int)(&z[9] - zIn) + 1; }` |
|    73 | 2109 | `		if( mo == 0 ){ mo = 12; y--; }` |
|    73 | 2110 | `		z += 10;` |
|     - | 2111 | `		{` |
|    73 | 2112 | `			int rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,&bOffSet);` |
|    73 | 2113 | `			if( rcT != 0 ){ return rcT; }` |
|     - | 2114 | `		}` |
|    67 | 2115 | `		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    67 | 2116 | `		bAny = 1;` |
|   298 | 2117 | `	}else if( SyisDigit(z[0])` |
|   178 | 2118 | `	 && (iNumRc = DtTryNumericDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn)) != 0 ){` |
|     - | 2119 | `		/* DD-MM-YYYY / DD.MM.YYYY (day first), MM/DD/YYYY (slash, American), and` |
|     - | 2120 | `		 * YYYY/MM/DD (slash, year first) — see DtTryNumericDate. Anything other than` |
|     - | 2121 | `		 * 1 is an error code in DtParse's own convention (positive position / negative` |
|     - | 2122 | `		 * "double time"); propagate it verbatim. */` |
|    55 | 2123 | `		if( iNumRc != 1 ){ return iNumRc; }` |
|    47 | 2124 | `		bAny = 1;` |
|   234 | 2125 | `	}else if( (SyisAlpha(z[0]) \|\| SyisDigit(z[0]))` |
|   200 | 2126 | `	 && (iMonRc = DtTryMonthDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,iBaseTs)) != 0 ){` |
|     - | 2127 | `		/* MonthName Day Year / Day MonthName Year, in any of php's spellings. As with` |
|     - | 2128 | `		 * DtTryNumericDate, anything other than 1 is an error code to propagate. */` |
|    51 | 2129 | `		if( iMonRc != 1 ){ return iMonRc; }` |
|    51 | 2130 | `		bAny = 1;` |
|   186 | 2131 | `	}else if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|    14 | 2132 | `	 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|     - | 2133 | `		/* Time-only: HH:MM[:SS] on the base date */` |
|    11 | 2134 | `		sxi64 t = iTs + iOff;` |
|    11 | 2135 | `		sxi64 days = DtFloorDiv(t,86400);` |
|    11 | 2136 | `		int h  = (z[0]-'0')*10 + (z[1]-'0');` |
|    11 | 2137 | `		int mi = (z[3]-'0')*10 + (z[4]-'0');` |
|    11 | 2138 | `		int s = 0;` |
|     - | 2139 | `		/* php: bad hour kills the token (error at its start); bad minute /` |
|     - | 2140 | `		 * second dies on the component's second digit */` |
|    11 | 2141 | `		if( h > 24 ){ return (int)(z - zIn) + 1; }` |
|     9 | 2142 | `		if( mi > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|     7 | 2143 | `		z += 5;` |
|     7 | 2144 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     5 | 2145 | `			s = (z[1]-'0')*10 + (z[2]-'0');` |
|     5 | 2146 | `			if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|     3 | 2147 | `			z += 3;` |
|     1 | 2148 | `		}` |
|     5 | 2149 | `		iTs = days*86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     5 | 2150 | `		bAny = 1;` |
|   153 | 2151 | `	}else if( DT_LOWEQ("now",3) ){` |
|     3 | 2152 | `		z += 3;` |
|     3 | 2153 | `		bAny = 1;` |
|     1 | 2154 | `	}` |
|     - | 2155 | `	/* Relative / keyword sequence */` |
|   158 | 2156 | `	for(;;){` |
|   575 | 2157 | `		DT_SKIP_WS();` |
|   465 | 2158 | `		if( z >= zEnd ){` |
|   299 | 2159 | `			break;` |
|     - | 2160 | `		}` |
|   167 | 2161 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|     9 | 2162 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     9 | 2163 | `			iTs = days*86400 - iOff;` |
|     9 | 2164 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|     9 | 2165 | `			bAny = 1;` |
|     9 | 2166 | `			continue;` |
|     - | 2167 | `		}` |
|   159 | 2168 | `		if( DT_LOWEQ("noon",4) ){` |
|     3 | 2169 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     3 | 2170 | `			iTs = days*86400 + 12*3600 - iOff;` |
|     3 | 2171 | `			z += 4;` |
|     3 | 2172 | `			bAny = 1;` |
|     3 | 2173 | `			continue;` |
|     - | 2174 | `		}` |
|   157 | 2175 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|     3 | 2176 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) + 1;` |
|     3 | 2177 | `			iTs = days*86400 - iOff;` |
|     3 | 2178 | `			z += 8;` |
|     3 | 2179 | `			bAny = 1;` |
|     3 | 2180 | `			continue;` |
|     - | 2181 | `		}` |
|   155 | 2182 | `		if( DT_LOWEQ("yesterday",9) ){` |
|     3 | 2183 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) - 1;` |
|     3 | 2184 | `			iTs = days*86400 - iOff;` |
|     3 | 2185 | `			z += 9;` |
|     3 | 2186 | `			bAny = 1;` |
|     3 | 2187 | `			continue;` |
|     - | 2188 | `		}` |
|     - | 2189 | `		/* Weekday navigation: "[next\|last\|previous\|this] <weekday>" moves to the` |
|     - | 2190 | `		 * midnight of the target weekday. Bare/"this" = the this-week occurrence on` |
|     - | 2191 | `		 * or after the base day; "next"/"last"/"previous" skip a matching base day. */` |
|     - | 2192 | `		{` |
|   153 | 2193 | `			const char *zSave = z;` |
|   153 | 2194 | `			int dir = 0;         /* 0 = this-week occurrence, 1 = next, -1 = last */` |
|     - | 2195 | `			int adv,dow;` |
|   186 | 2196 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; DT_SKIP_WS(); }` |
|   134 | 2197 | `			else if( DT_LOWEQ("previous",8) ){ dir = -1; z += 8; DT_SKIP_WS(); }` |
|   177 | 2198 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; DT_SKIP_WS(); }` |
|   112 | 2199 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; DT_SKIP_WS(); }` |
|   153 | 2200 | `			dow = DtMatchWeekday(z,zEnd,&adv);` |
|   153 | 2201 | `			if( dow >= 0 ){` |
|    45 | 2202 | `				sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|    45 | 2203 | `				int bdow = (int)(((days + 4) % 7 + 7) % 7); /* 1970-01-01 was Thursday */` |
|     - | 2204 | `				sxi64 delta;` |
|    45 | 2205 | `				if( dir == 1 ){` |
|    13 | 2206 | `					delta = ((dow - bdow) % 7 + 7) % 7;` |
|    13 | 2207 | `					if( delta == 0 ){ delta = 7; }` |
|    39 | 2208 | `				}else if( dir == -1 ){` |
|    13 | 2209 | `					delta = -(((bdow - dow) % 7 + 7) % 7);` |
|    13 | 2210 | `					if( delta == 0 ){ delta = -7; }` |
|     7 | 2211 | `				}else{` |
|    21 | 2212 | `					delta = ((dow - bdow) % 7 + 7) % 7;` |
|     - | 2213 | `				}` |
|    45 | 2214 | `				iTs = (days + delta)*86400 - iOff; /* midnight of the target day */` |
|    45 | 2215 | `				z += adv;` |
|    45 | 2216 | `				bAny = 1;` |
|    45 | 2217 | `				continue;` |
|     - | 2218 | `			}` |
|   109 | 2219 | `			z = zSave; /* prefix did not introduce a weekday: rewind and try the rest */` |
|     - | 2220 | `		}` |
|     - | 2221 | `		/* "first\|last day of (this\|next\|last month \| MonthName [Year])": jump to the` |
|     - | 2222 | `		 * first or last day of a target month. A this/next/last-month target keeps the` |
|     - | 2223 | `		 * base time-of-day; an absolute MonthName [Year] target resets it to midnight` |
|     - | 2224 | `		 * (php). */` |
|   109 | 2225 | `		if( DT_LOWEQ("first",5) \|\| DT_LOWEQ("last",4) ){` |
|    39 | 2226 | `			const char *zSave = z;` |
|    39 | 2227 | `			int bFirst = (SyToLower((unsigned char)z[0]) == 'f');` |
|    39 | 2228 | `			z += bFirst ? 5 : 4;` |
|    96 | 2229 | `			DT_SKIP_WS();` |
|    39 | 2230 | `			if( DT_LOWEQ("day",3) ){` |
|    31 | 2231 | `				z += 3;` |
|    76 | 2232 | `				DT_SKIP_WS();` |
|    31 | 2233 | `				if( DT_LOWEQ("of",2) ){` |
|    31 | 2234 | `					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);` |
|     - | 2235 | `					sxi64 yy,tod;` |
|    31 | 2236 | `					int mm,dd0,keepTime = 1,ok = 1;` |
|    31 | 2237 | `					z += 2;` |
|    74 | 2238 | `					DT_SKIP_WS();` |
|    31 | 2239 | `					DtCivilFromDays(days0,&yy,&mm,&dd0);` |
|    31 | 2240 | `					tod = (iTs + iOff) - days0*86400;` |
|    40 | 2241 | `					if( DT_LOWEQ("this",4) ){ z += 4; DT_SKIP_WS();` |
|     7 | 2242 | `						if( DT_LOWEQ("month",5) ){ z += 5; }else{ ok = 0; } }` |
|    34 | 2243 | `					else if( DT_LOWEQ("next",4) ){ z += 4; DT_SKIP_WS();` |
|     7 | 2244 | `						if( DT_LOWEQ("month",5) ){ z += 5; mm++; if(mm>12){ mm=1; yy++; } }else{ ok = 0; } }` |
|    25 | 2245 | `					else if( DT_LOWEQ("last",4) ){ z += 4; DT_SKIP_WS();` |
|     5 | 2246 | `						if( DT_LOWEQ("month",5) ){ z += 5; mm--; if(mm<1){ mm=12; yy--; } }else{ ok = 0; } }` |
|    15 | 2247 | `					else if( z < zEnd ){` |
|     - | 2248 | `						int mo,adv;` |
|    13 | 2249 | `						mo = DtMatchMonth(z,zEnd,&adv);` |
|    13 | 2250 | `						if( mo == 0 ){ return (int)(z - zIn) + 1; }` |
|    27 | 2251 | `						z += adv; DT_SKIP_WS();` |
|    13 | 2252 | `						mm = mo; keepTime = 0; tod = 0;` |
|    13 | 2253 | `						if( z < zEnd && SyisDigit(z[0]) ){` |
|     9 | 2254 | `							int ny = 0; sxi64 yv = 0;` |
|    41 | 2255 | `							while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ yv = yv*10 + (z[0]-'0'); z++; ny++; }` |
|     9 | 2256 | `							if( ny <= 2 ){ if( yv <= 69 ){ yv += 2000; } else if( yv <= 99 ){ yv += 1900; } }` |
|     9 | 2257 | `							yy = yv;` |
|     4 | 2258 | `						}` |
|     6 | 2259 | `					}` |
|     - | 2260 | `					/* else: "... day of" with nothing after — php defaults to this` |
|     - | 2261 | `					 * month (mm/yy/tod stay the base, keepTime stays 1). */` |
|    31 | 2262 | `					if( ok ){` |
|    31 | 2263 | `						int dim = (int)(DtDaysFromCivil(yy,mm+1,1) - DtDaysFromCivil(yy,mm,1));` |
|    31 | 2264 | `						int day = bFirst ? 1 : dim;` |
|    31 | 2265 | `						iTs = DtDaysFromCivil(yy,mm,day)*86400 + (keepTime ? tod : 0) - iOff;` |
|    31 | 2266 | `						bAny = 1;` |
|    31 | 2267 | `						continue;` |
|     - | 2268 | `					}` |
|   ! 0 | 2269 | `				}` |
|   ! 0 | 2270 | `			}` |
|     9 | 2271 | `			z = zSave; /* not the "first\|last day of ..." shape: rewind */` |
|     4 | 2272 | `		}` |
|     - | 2273 | `		/* Standalone "this\|next\|last (month\|week)": month shifts by ±1 keeping the` |
|     - | 2274 | `		 * day/time; week moves to the Monday of this/next/last ISO week keeping the` |
|     - | 2275 | `		 * time-of-day (php: weeks start on Monday). */` |
|     - | 2276 | `		{` |
|   101 | 2277 | `			const char *zSave = z;` |
|   101 | 2278 | `			int dir = 2; /* 2 = no prefix */` |
|   101 | 2279 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; }` |
|    69 | 2280 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; }` |
|    61 | 2281 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; }` |
|    79 | 2282 | `			if( dir != 2 ){` |
|    66 | 2283 | `				DT_SKIP_WS();` |
|    27 | 2284 | `				if( DT_LOWEQ("month",5) ){` |
|    13 | 2285 | `					z += 5;` |
|    13 | 2286 | `					iTs = DtAddMonths(iTs,iOff,dir);` |
|    13 | 2287 | `					bAny = 1;` |
|    13 | 2288 | `					continue;` |
|     - | 2289 | `				}` |
|    15 | 2290 | `				if( DT_LOWEQ("week",4) ){` |
|    13 | 2291 | `					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);` |
|    13 | 2292 | `					sxi64 tod = (iTs + iOff) - days0*86400;` |
|    13 | 2293 | `					int bdow = (int)(((days0 + 4) % 7 + 7) % 7);` |
|    13 | 2294 | `					sxi64 monday = days0 - ((bdow + 6) % 7); /* Monday of the base week */` |
|    13 | 2295 | `					z += 4;` |
|    13 | 2296 | `					monday += (sxi64)dir * 7;` |
|    13 | 2297 | `					iTs = monday*86400 + tod - iOff;` |
|    13 | 2298 | `					bAny = 1;` |
|    13 | 2299 | `					continue;` |
|     - | 2300 | `				}` |
|     1 | 2301 | `			}` |
|    55 | 2302 | `			z = zSave;` |
|     - | 2303 | `		}` |
|     - | 2304 | `		/* Trailing time-of-day in a relative sequence ("next thursday 15:00"): set` |
|     - | 2305 | `		 * the clock on the current day. The leading absolute HH:MM branch handles a` |
|     - | 2306 | `		 * time at the START; this handles one AFTER a date/relative token. */` |
|    54 | 2307 | `		if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|    16 | 2308 | `		 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|    13 | 2309 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|    13 | 2310 | `			int hh = (z[0]-'0')*10 + (z[1]-'0');` |
|    13 | 2311 | `			int mm = (z[3]-'0')*10 + (z[4]-'0');` |
|    13 | 2312 | `			int ss = 0;` |
|    13 | 2313 | `			if( hh > 24 ){ return (int)(z - zIn) + 1; }` |
|    13 | 2314 | `			if( mm > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|    13 | 2315 | `			z += 5;` |
|    13 | 2316 | `			if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     5 | 2317 | `				ss = (z[1]-'0')*10 + (z[2]-'0');` |
|     5 | 2318 | `				if( ss > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|     5 | 2319 | `				z += 3;` |
|     2 | 2320 | `			}` |
|    13 | 2321 | `			iTs = days*86400 + (sxi64)hh*3600 + (sxi64)mm*60 + ss - iOff;` |
|    13 | 2322 | `			bAny = 1;` |
|    13 | 2323 | `			continue;` |
|     - | 2324 | `		}` |
|    43 | 2325 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|    31 | 2326 | `			int neg = 0;` |
|    31 | 2327 | `			sxi64 v = 0;` |
|    31 | 2328 | `			const char *zNumStart = z;` |
|    31 | 2329 | `			if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; }` |
|    31 | 2330 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zNumStart - zIn) + 1; }` |
|    77 | 2331 | `			while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|    31 | 2332 | `			if( neg ){ v = -v; }` |
|    74 | 2333 | `			DT_SKIP_WS();` |
|    31 | 2334 | `			if( DT_LOWEQ("seconds",7) )     { iTs += v;            z += 7; }` |
|    31 | 2335 | `			else if( DT_LOWEQ("second",6) ) { iTs += v;            z += 6; }` |
|    31 | 2336 | `			else if( DT_LOWEQ("secs",4) )   { iTs += v;            z += 4; }` |
|    31 | 2337 | `			else if( DT_LOWEQ("sec",3) )    { iTs += v;            z += 3; }` |
|    31 | 2338 | `			else if( DT_LOWEQ("minutes",7) ){ iTs += v*60;         z += 7; }` |
|    29 | 2339 | `			else if( DT_LOWEQ("minute",6) ) { iTs += v*60;         z += 6; }` |
|    29 | 2340 | `			else if( DT_LOWEQ("mins",4) )   { iTs += v*60;         z += 4; }` |
|    29 | 2341 | `			else if( DT_LOWEQ("min",3) )    { iTs += v*60;         z += 3; }` |
|    29 | 2342 | `			else if( DT_LOWEQ("hours",5) )  { iTs += v*3600;       z += 5; }` |
|    27 | 2343 | `			else if( DT_LOWEQ("hour",4) )   { iTs += v*3600;       z += 4; }` |
|    27 | 2344 | `			else if( DT_LOWEQ("days",4) )   { iTs += v*86400;      z += 4; }` |
|    27 | 2345 | `			else if( DT_LOWEQ("day",3) )    { iTs += v*86400;      z += 3; }` |
|    23 | 2346 | `			else if( DT_LOWEQ("weeks",5) )  { iTs += v*7*86400;    z += 5; }` |
|    21 | 2347 | `			else if( DT_LOWEQ("week",4) )   { iTs += v*7*86400;    z += 4; }` |
|    19 | 2348 | `			else if( DT_LOWEQ("fortnights",10) ){ iTs += v*14*86400; z += 10; }` |
|    17 | 2349 | `			else if( DT_LOWEQ("fortnight",9) )  { iTs += v*14*86400; z += 9; }` |
|    17 | 2350 | `			else if( DT_LOWEQ("months",6) ) { iTs = DtAddMonths(iTs,iOff,v); z += 6; }` |
|    15 | 2351 | `			else if( DT_LOWEQ("month",5) )  { iTs = DtAddMonths(iTs,iOff,v); z += 5; }` |
|     9 | 2352 | `			else if( DT_LOWEQ("years",5) )  { iTs = DtAddMonths(iTs,iOff,v*12); z += 5; }` |
|     9 | 2353 | `			else if( DT_LOWEQ("year",4) )   { iTs = DtAddMonths(iTs,iOff,v*12); z += 4; }` |
|     - | 2354 | `			else{` |
|     7 | 2355 | `				return (int)(z - zIn) + 1;` |
|     - | 2356 | `			}` |
|    25 | 2357 | `			bAny = 1;` |
|    25 | 2358 | `			continue;` |
|     - | 2359 | `		}` |
|    13 | 2360 | `		return (int)(z - zIn) + 1;` |
|   ! 0 | 2361 | `	}` |
|   299 | 2362 | `	if( !bAny ){` |
|   ! 0 | 2363 | `		return 1;` |
|     - | 2364 | `	}` |
|   299 | 2365 | `	*pTs = iTs;` |
|   299 | 2366 | `	*pOff = iOff;` |
|   299 | 2367 | `	*pbOffSet = bOffSet;` |
|   299 | 2368 | `	return 0;` |
|     - | 2369 | `#undef DT_SKIP_WS` |
|     - | 2370 | `#undef DT_LOWEQ` |
|   211 | 2371 | `}` |
|     - | 2372 | `/* int __dt_now() */` |
|   202 | 2373 | `static int vm_builtin_dt_now(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2374 | `{` |
|   101 | 2375 | `	SXUNUSED(nArg);` |
|   101 | 2376 | `	SXUNUSED(apArg);` |
|   203 | 2377 | `	ph7_result_int64(pCtx,(ph7_int64)time(0));` |
|   203 | 2378 | `	return PH7_OK;` |
|     1 | 2379 | `}` |
|     - | 2380 | `/* mixed __dt_parse(string $s, int $baseTs, int $baseOff)` |
|     - | 2381 | ` *   -> [ts, off, offWasExplicit] on success; php's error MESSAGE string on` |
|     - | 2382 | ` *      failure (the chunk wraps it in DateMalformedStringException). */` |
|   420 | 2383 | `static int vm_builtin_dt_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2384 | `{` |
|     - | 2385 | `	const char *zIn;` |
|     - | 2386 | `	int nLen;` |
|     - | 2387 | `	sxi64 iBaseTs;` |
|     - | 2388 | `	sxi32 iBaseOff;` |
|   421 | 2389 | `	sxi64 iTs = 0;` |
|   421 | 2390 | `	sxi32 iOff = 0;` |
|   421 | 2391 | `	int bOffSet = 0;` |
|     - | 2392 | `	int iErrPos;` |
|   421 | 2393 | `	if( nArg < 3 ){` |
|   ! 0 | 2394 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2395 | `		return PH7_OK;` |
|     - | 2396 | `	}` |
|   421 | 2397 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   421 | 2398 | `	iBaseTs  = ph7_value_to_int64(apArg[1]);` |
|   421 | 2399 | `	iBaseOff = (sxi32)ph7_value_to_int64(apArg[2]);` |
|   421 | 2400 | `	iErrPos = DtParse(zIn,nLen,iBaseTs,iBaseOff,&iTs,&iOff,&bOffSet);` |
|   421 | 2401 | `	if( iErrPos != 0 ){` |
|     - | 2402 | `		/* Negative encoding: php's "Double time specification" reason */` |
|    51 | 2403 | `		int bDouble = iErrPos < 0;` |
|    51 | 2404 | `		int iPos = (bDouble ? -iErrPos : iErrPos) - 1;` |
|    51 | 2405 | `		char cAt = (iPos < nLen) ? zIn[iPos] : ' ';` |
|     - | 2406 | `		/* php appends a reason: an alphabetic token is assumed to be a timezone` |
|     - | 2407 | `		 * lookup miss, anything else an unexpected character. */` |
|   100 | 2408 | `		ph7_result_string_format(pCtx,` |
|     - | 2409 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    25 | 2410 | `			nLen,zIn,iPos,cAt,` |
|    49 | 2411 | `			bDouble ? "Double time specification"` |
|    48 | 2412 | `			: ((cAt >= 'a' && cAt <= 'z') \|\| (cAt >= 'A' && cAt <= 'Z'))` |
|     - | 2413 | `				? "The timezone could not be found in the database"` |
|    48 | 2414 | `				: "Unexpected character");` |
|    51 | 2415 | `		return PH7_OK;` |
|     - | 2416 | `	}` |
|     - | 2417 | `	{` |
|   371 | 2418 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|   371 | 2419 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|   371 | 2420 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2421 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2422 | `		}` |
|   371 | 2423 | `		ph7_value_int64(pV,iTs);` |
|   371 | 2424 | `		ph7_array_add_elem(pArr,0,pV);` |
|   371 | 2425 | `		ph7_value_int64(pV,iOff);` |
|   371 | 2426 | `		ph7_array_add_elem(pArr,0,pV);` |
|     - | 2427 | `		/* int, not bool: 0 = no explicit offset, 1 = numeric offset/@epoch,` |
|     - | 2428 | `		 * 2 = literal "Z" (php keeps the distinction in the zone name) */` |
|   371 | 2429 | `		ph7_value_int64(pV,bOffSet);` |
|   371 | 2430 | `		ph7_array_add_elem(pArr,0,pV);` |
|   371 | 2431 | `		ph7_result_value(pCtx,pArr);` |
|     - | 2432 | `	}` |
|   371 | 2433 | `	return PH7_OK;` |
|   211 | 2434 | `}` |
|     - | 2435 | `/* string __dt_default_tz(void) — the date_default_timezone_set() identifier */` |
|   202 | 2436 | `static int vm_builtin_dt_default_tz(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2437 | `{` |
|   101 | 2438 | `	SXUNUSED(nArg);` |
|   101 | 2439 | `	SXUNUSED(apArg);` |
|   203 | 2440 | `	ph7_result_string(pCtx,pCtx->pVm->zDefTz,(int)pCtx->pVm->nDefTz);` |
|   203 | 2441 | `	return PH7_OK;` |
|     1 | 2442 | `}` |
|     - | 2443 | `/* string __dt_format(int $ts, int $off, string $tzname, string $format) */` |
|   140 | 2444 | `static int vm_builtin_dt_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2445 | `{` |
|     - | 2446 | `	Sytm sTm;` |
|     - | 2447 | `	sxi64 iTs;` |
|     - | 2448 | `	sxi32 iOff;` |
|     - | 2449 | `	const char *zName,*zFmt;` |
|     - | 2450 | `	int nName,nFmt;` |
|     - | 2451 | `	char zZone[64];` |
|   141 | 2452 | `	if( nArg < 4 ){` |
|   ! 0 | 2453 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2454 | `		return PH7_OK;` |
|     - | 2455 | `	}` |
|   141 | 2456 | `	iTs  = ph7_value_to_int64(apArg[0]);` |
|   141 | 2457 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|   141 | 2458 | `	zName = ph7_value_to_string(apArg[2],&nName);` |
|   141 | 2459 | `	zFmt  = ph7_value_to_string(apArg[3],&nFmt);` |
|   141 | 2460 | `	if( nName >= (int)sizeof(zZone) ){ nName = (int)sizeof(zZone) - 1; }` |
|   141 | 2461 | `	SyMemcpy(zName,zZone,(sxu32)nName);` |
|   141 | 2462 | `	zZone[nName] = 0;` |
|   141 | 2463 | `	DtFillSytm(iTs,iOff,zZone,&sTm);` |
|   141 | 2464 | `	DateFormat(pCtx,zFmt,nFmt,&sTm);` |
|   141 | 2465 | `	return PH7_OK;` |
|    71 | 2466 | `}` |
|     - | 2467 | `/* int __dt_make(int y, int mo, int d, int h, int i, int s, int off) */` |
|     4 | 2468 | `static int vm_builtin_dt_make(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2469 | `{` |
|     - | 2470 | `	sxi64 y;` |
|     - | 2471 | `	int mo,d,h,mi,s;` |
|     - | 2472 | `	sxi32 iOff;` |
|     5 | 2473 | `	if( nArg < 7 ){` |
|   ! 0 | 2474 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2475 | `		return PH7_OK;` |
|     - | 2476 | `	}` |
|     5 | 2477 | `	y   = ph7_value_to_int64(apArg[0]);` |
|     5 | 2478 | `	mo  = ph7_value_to_int(apArg[1]);` |
|     5 | 2479 | `	d   = ph7_value_to_int(apArg[2]);` |
|     5 | 2480 | `	h   = ph7_value_to_int(apArg[3]);` |
|     5 | 2481 | `	mi  = ph7_value_to_int(apArg[4]);` |
|     5 | 2482 | `	s   = ph7_value_to_int(apArg[5]);` |
|     5 | 2483 | `	iOff = (sxi32)ph7_value_to_int64(apArg[6]);` |
|     5 | 2484 | `	ph7_result_int64(pCtx,DtMakeTs(y,mo,d,h,mi,s,iOff));` |
|     5 | 2485 | `	return PH7_OK;` |
|     3 | 2486 | `}` |
|     - | 2487 | `/* Days in a civil month (php's overflow rules use it during diff borrows) */` |
|    60 | 2488 | `static int DtDaysInMonth(sxi64 y,int m)` |
|     1 | 2489 | `{` |
|     - | 2490 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|    61 | 2491 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|     9 | 2492 | `		return 29;` |
|     - | 2493 | `	}` |
|    53 | 2494 | `	return aMonDays[(m - 1) % 12];` |
|    31 | 2495 | `}` |
|     - | 2496 | `/* int __dt_civil_add(int ts, int off, int y, int m, int d, int h, int i,` |
|     - | 2497 | ` *                    int s, int sign)` |
|     - | 2498 | ` *   php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|     - | 2499 | ` *   (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset. */` |
|    54 | 2500 | `static int vm_builtin_dt_civil_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2501 | `{` |
|     - | 2502 | `	sxi64 iTs,iLocal,iDays,iSecs,y0,moT,dayCount;` |
|     - | 2503 | `	sxi32 iOff;` |
|     - | 2504 | `	int mo0,d0,iSign;` |
|     - | 2505 | `	sxi64 y,m,d,h,i,s;` |
|    55 | 2506 | `	if( nArg < 9 ){` |
|   ! 0 | 2507 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2508 | `		return PH7_OK;` |
|     - | 2509 | `	}` |
|    55 | 2510 | `	iTs   = ph7_value_to_int64(apArg[0]);` |
|    55 | 2511 | `	iOff  = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    55 | 2512 | `	y     = ph7_value_to_int64(apArg[2]);` |
|    55 | 2513 | `	m     = ph7_value_to_int64(apArg[3]);` |
|    55 | 2514 | `	d     = ph7_value_to_int64(apArg[4]);` |
|    55 | 2515 | `	h     = ph7_value_to_int64(apArg[5]);` |
|    55 | 2516 | `	i     = ph7_value_to_int64(apArg[6]);` |
|    55 | 2517 | `	s     = ph7_value_to_int64(apArg[7]);` |
|    55 | 2518 | `	iSign = ph7_value_to_int(apArg[8]) < 0 ? -1 : 1;` |
|    55 | 2519 | `	iLocal = iTs + iOff;` |
|    55 | 2520 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    55 | 2521 | `	iSecs  = iLocal - iDays*86400;` |
|    55 | 2522 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    55 | 2523 | `	y0 += iSign * y;` |
|    55 | 2524 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    55 | 2525 | `	y0 += DtFloorDiv(moT,12);` |
|    55 | 2526 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    55 | 2527 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    55 | 2528 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    55 | 2529 | `	ph7_result_int64(pCtx,iLocal - iOff);` |
|    55 | 2530 | `	return PH7_OK;` |
|    28 | 2531 | `}` |
|     - | 2532 | `/* array __dt_civil_diff(int ts1, int off1, int ts2)` |
|     - | 2533 | ` *   -> [y,m,d,h,i,s,days,invert]: timelib's breakdown — field-wise deltas in` |
|     - | 2534 | ` *   the FIRST operand's offset, then borrow seconds→minutes→hours→days, then` |
|     - | 2535 | ` *   the day borrow walks whole months backward from the later date (that walk` |
|     - | 2536 | ` *   is why Jan 31 → Mar 02 reports m=0 d=30, not "1 month"). */` |
|    14 | 2537 | `static int vm_builtin_dt_civil_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2538 | `{` |
|     - | 2539 | `	sxi64 iTs1,iTs2,iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|     - | 2540 | `	sxi32 iOff;` |
|     - | 2541 | `	int moA,dA,moB,dB,bInvert;` |
|     - | 2542 | `	sxi64 sA,sB,y,m,d,h,i,s;` |
|     - | 2543 | `	ph7_value *pArr,*pV;` |
|    15 | 2544 | `	if( nArg < 3 ){` |
|   ! 0 | 2545 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2546 | `		return PH7_OK;` |
|     - | 2547 | `	}` |
|    15 | 2548 | `	iTs1 = ph7_value_to_int64(apArg[0]);` |
|    15 | 2549 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    15 | 2550 | `	iTs2 = ph7_value_to_int64(apArg[2]);` |
|    15 | 2551 | `	bInvert = iTs1 > iTs2;` |
|    15 | 2552 | `	iA = bInvert ? iTs2 : iTs1;` |
|    15 | 2553 | `	iB = bInvert ? iTs1 : iTs2;` |
|    15 | 2554 | `	iLa = iA + iOff;` |
|    15 | 2555 | `	iLb = iB + iOff;` |
|    15 | 2556 | `	daysA = DtFloorDiv(iLa,86400);` |
|    15 | 2557 | `	daysB = DtFloorDiv(iLb,86400);` |
|    15 | 2558 | `	sA = iLa - daysA*86400;` |
|    15 | 2559 | `	sB = iLb - daysB*86400;` |
|    15 | 2560 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|    15 | 2561 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|    15 | 2562 | `	s = (sB % 60) - (sA % 60);` |
|    15 | 2563 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|    15 | 2564 | `	h = (sB / 3600) - (sA / 3600);` |
|    15 | 2565 | `	d = dB - dA;` |
|    15 | 2566 | `	m = moB - moA;` |
|    15 | 2567 | `	y = yB - yA;` |
|    15 | 2568 | `	if( s < 0 ){ s += 60; i--; }` |
|    15 | 2569 | `	if( i < 0 ){ i += 60; h--; }` |
|    15 | 2570 | `	if( h < 0 ){ h += 24; d--; }` |
|    27 | 2571 | `	while( d < 0 ){` |
|    13 | 2572 | `		moB--;` |
|    13 | 2573 | `		if( moB < 1 ){ moB = 12; yB--; }` |
|    13 | 2574 | `		d += DtDaysInMonth(yB,moB);` |
|    13 | 2575 | `		m--;` |
|     1 | 2576 | `	}` |
|    15 | 2577 | `	if( m < 0 ){ m += 12; y--; }` |
|    15 | 2578 | `	pArr = ph7_context_new_array(pCtx);` |
|    15 | 2579 | `	pV = ph7_context_new_scalar(pCtx);` |
|    15 | 2580 | `	if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2581 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2582 | `	}` |
|    15 | 2583 | `	ph7_value_int64(pV,y);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2584 | `	ph7_value_int64(pV,m);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2585 | `	ph7_value_int64(pV,d);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2586 | `	ph7_value_int64(pV,h);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2587 | `	ph7_value_int64(pV,i);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2588 | `	ph7_value_int64(pV,s);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2589 | `	ph7_value_int64(pV,(iB - iA) / 86400); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2590 | `	ph7_value_int64(pV,bInvert); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2591 | `	ph7_result_value(pCtx,pArr);` |
|    15 | 2592 | `	return PH7_OK;` |
|     8 | 2593 | `}` |
|     - | 2594 | `/* int __dt_isodate(int ts, int off, int y, int w, int dow)` |
|     - | 2595 | ` *   setISODate: jump to ISO year/week/weekday, preserving the time of day. */` |
|     8 | 2596 | `static int vm_builtin_dt_isodate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2597 | `{` |
|     - | 2598 | `	sxi64 iTs,iLocal,iTod,jan4,monday1,target,y;` |
|     - | 2599 | `	sxi32 iOff;` |
|     - | 2600 | `	sxi64 w,dow;` |
|     - | 2601 | `	int isoDow;` |
|     9 | 2602 | `	if( nArg < 5 ){` |
|   ! 0 | 2603 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2604 | `		return PH7_OK;` |
|     - | 2605 | `	}` |
|     9 | 2606 | `	iTs = ph7_value_to_int64(apArg[0]);` |
|     9 | 2607 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|     9 | 2608 | `	y   = ph7_value_to_int64(apArg[2]);` |
|     9 | 2609 | `	w   = ph7_value_to_int64(apArg[3]);` |
|     9 | 2610 | `	dow = ph7_value_to_int64(apArg[4]);` |
|     9 | 2611 | `	iLocal = iTs + iOff;` |
|     9 | 2612 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|     9 | 2613 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|     9 | 2614 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|     9 | 2615 | `	monday1 = jan4 - (isoDow - 1);` |
|     9 | 2616 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|     9 | 2617 | `	ph7_result_int64(pCtx,target*86400 + iTod - iOff);` |
|     9 | 2618 | `	return PH7_OK;` |
|     5 | 2619 | `}` |
|     - | 2620 | `/* Consume nMin..nMax digits from *pz; returns count consumed (0 = failure) */` |
|   170 | 2621 | `static int DtEatDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2622 | `{` |
|   171 | 2623 | `	const char *z = *pz;` |
|   171 | 2624 | `	sxi64 v = 0;` |
|   171 | 2625 | `	int n = 0;` |
|   609 | 2626 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|   439 | 2627 | `		v = v*10 + (z[0] - '0');` |
|   439 | 2628 | `		z++;` |
|   439 | 2629 | `		n++;` |
|     1 | 2630 | `	}` |
|   171 | 2631 | `	if( n < nMin ){` |
|     5 | 2632 | `		return 0;` |
|     - | 2633 | `	}` |
|   167 | 2634 | `	*pz = z;` |
|   167 | 2635 | `	*pVal = v;` |
|   167 | 2636 | `	return n;` |
|    86 | 2637 | `}` |
|     - | 2638 | `/* timelib_get_nr's recovery: skip non-digits hunting for the field.` |
|     - | 2639 | ` * Returns 1 = found+read, 0 = digits present but short, -1 = exhausted. */` |
|     4 | 2640 | `static int DtHuntDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2641 | `{` |
|     5 | 2642 | `	const char *z = *pz;` |
|    29 | 2643 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|     5 | 2644 | `	*pz = z;` |
|     5 | 2645 | `	if( z >= zEnd ){` |
|     5 | 2646 | `		return -1;` |
|     - | 2647 | `	}` |
|   ! 0 | 2648 | `	return DtEatDigits(pz,zEnd,nMin,nMax,pVal) ? 1 : 0;` |
|     3 | 2649 | `}` |
|     - | 2650 | `/* Case-insensitive name-table lookup; returns 1-based index or 0 */` |
|    14 | 2651 | `static int DtEatName(const char **pz,const char *zEnd,const char **azNames,int nNames)` |
|     1 | 2652 | `{` |
|     - | 2653 | `	int k;` |
|    23 | 2654 | `	for( k = 0 ; k < nNames ; k++ ){` |
|    23 | 2655 | `		int n = (int)SyStrlen(azNames[k]);` |
|    23 | 2656 | `		if( zEnd - *pz >= n && SyStrnicmp(*pz,azNames[k],(sxu32)n) == 0 ){` |
|    15 | 2657 | `			*pz += n;` |
|    15 | 2658 | `			return k + 1;` |
|     - | 2659 | `		}` |
|     5 | 2660 | `	}` |
|   ! 0 | 2661 | `	return 0;` |
|     8 | 2662 | `}` |
|     - | 2663 | `/* mixed __dt_from_format(string fmt, string input, int nowTs, int defOff)` |
|     - | 2664 | ` *   php's DateTime::createFromFormat engine. Success: [ts, off, offKind, name]` |
|     - | 2665 | ` *   where offKind 0=none-parsed, 1=numeric offset, 2=literal Z, 3=named id.` |
|     - | 2666 | ` *   Failure: "POS\tMESSAGE" (timelib's message strings; PHL reports the FIRST` |
|     - | 2667 | ` *   error where php may accumulate several — recorded). A trailing-data` |
|     - | 2668 | ` *   warning rides as [4]=pos, [5]=msg on the success array. */` |
|    56 | 2669 | `static int vm_builtin_dt_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2670 | `{` |
|     - | 2671 | `	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};` |
|     - | 2672 | `	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",` |
|     - | 2673 | `		"thursday","friday","saturday"};` |
|     - | 2674 | `	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",` |
|     - | 2675 | `		"aug","sep","oct","nov","dec"};` |
|     - | 2676 | `	static const char *azMonFull[] = {"january","february","march","april",` |
|     - | 2677 | `		"may","june","july","august","september","october","november","december"};` |
|     - | 2678 | `	const char *zFmt,*zIn,*zEnd,*zInEnd,*z;` |
|     - | 2679 | `	int nFmt,nIn;` |
|     - | 2680 | `	sxi64 iNow,v;` |
|     - | 2681 | `	sxi32 iDefOff;` |
|     - | 2682 | `	/* -1 == unset */` |
|    57 | 2683 | `	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;` |
|    57 | 2684 | `	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;` |
|    57 | 2685 | `	int iOffKind = 0;` |
|    57 | 2686 | `	sxi32 iOffVal = 0;` |
|     - | 2687 | `	char zName[16];` |
|    57 | 2688 | `	const char *zErr = 0;` |
|     - | 2689 | `	const char *aWarnMsg[3];` |
|     - | 2690 | `	int aWarnPos[3];` |
|    57 | 2691 | `	int nWarn = 0,bAborted = 0;` |
|     - | 2692 | `	const char *aErrMsg[8];` |
|     - | 2693 | `	int aErrPos[8];` |
|    57 | 2694 | `	int nErr = 0,nErrKept = 0;` |
|    57 | 2695 | `	if( nArg < 4 ){` |
|   ! 0 | 2696 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2697 | `		return PH7_OK;` |
|     - | 2698 | `	}` |
|    57 | 2699 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    57 | 2700 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    57 | 2701 | `	iNow = ph7_value_to_int64(apArg[2]);` |
|    57 | 2702 | `	iDefOff = (sxi32)ph7_value_to_int64(apArg[3]);` |
|    57 | 2703 | `	zEnd = &zFmt[nFmt];` |
|    57 | 2704 | `	zInEnd = &zIn[nIn];` |
|    57 | 2705 | `	z = zIn;` |
|    57 | 2706 | `	zName[0] = 0;` |
|     - | 2707 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|     - | 2708 | `	{ int _p = (iPos),_k,_f = -1; \` |
|     - | 2709 | `	  nErr++; \` |
|     - | 2710 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|     - | 2711 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|     - | 2712 | `	  else if( nErrKept < 8 ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|   383 | 2713 | `	while( zFmt < zEnd ){` |
|   331 | 2714 | `		char c = zFmt[0];` |
|   331 | 2715 | `		zFmt++;` |
|   331 | 2716 | `		zErr = 0;` |
|   331 | 2717 | `		if( c == '!' ){` |
|    11 | 2718 | `			y = 1970; mo = 1; d = 1; h = 0; mi = 0; s = 0;` |
|    11 | 2719 | `			h12 = -1; iMeridiem = -1;` |
|    11 | 2720 | `			continue;` |
|     - | 2721 | `		}` |
|   321 | 2722 | `		if( c == '\|' ){ bPipe = 1; continue; }` |
|   317 | 2723 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|   315 | 2724 | `		if( z >= zInEnd ){` |
|     - | 2725 | `			/* timelib aborts the scan once input is exhausted */` |
|     9 | 2726 | `			DT_FF_LOGERR(nIn,"Not enough data available to satisfy format");` |
|     5 | 2727 | `			break;` |
|     - | 2728 | `		}` |
|   311 | 2729 | `		switch( c ){` |
|    20 | 2730 | `		case 'd': case 'j':` |
|    41 | 2731 | `			if( !DtEatDigits(&z,zInEnd,1,2,&d) ){` |
|   ! 0 | 2732 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit day could not be found");` |
|   ! 0 | 2733 | `				if( DtHuntDigits(&z,zInEnd,1,2,&d) < 0 ){` |
|   ! 0 | 2734 | `					DT_FF_LOGERR(nIn,"A two digit day could not be found");` |
|   ! 0 | 2735 | `				}` |
|   ! 0 | 2736 | `			}` |
|    41 | 2737 | `			break;` |
|     1 | 2738 | `		case 'D':` |
|     3 | 2739 | `			if( !DtEatName(&z,zInEnd,azDay3,7) ){` |
|   ! 0 | 2740 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2741 | `			}` |
|     3 | 2742 | `			break;` |
|     1 | 2743 | `		case 'l':` |
|     3 | 2744 | `			if( !DtEatName(&z,zInEnd,azDayFull,7) ){` |
|   ! 0 | 2745 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2746 | `			}` |
|     3 | 2747 | `			break;` |
|     1 | 2748 | `		case 'S':` |
|     - | 2749 | `			/* ordinal suffix: st nd rd th */` |
|     4 | 2750 | `			if( zInEnd-z >= 2 && ((z[0]=='s'&&z[1]=='t')\|\|(z[0]=='n'&&z[1]=='d')` |
|     2 | 2751 | `			 \|\|(z[0]=='r'&&z[1]=='d')\|\|(z[0]=='t'&&z[1]=='h')) ){` |
|     3 | 2752 | `				z += 2;` |
|     1 | 2753 | `			}` |
|     3 | 2754 | `			break;` |
|    18 | 2755 | `		case 'm': case 'n':` |
|    37 | 2756 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mo) ){` |
|   ! 0 | 2757 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit month could not be found");` |
|   ! 0 | 2758 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mo) < 0 ){` |
|   ! 0 | 2759 | `					DT_FF_LOGERR(nIn,"A two digit month could not be found");` |
|   ! 0 | 2760 | `				}` |
|   ! 0 | 2761 | `			}` |
|    37 | 2762 | `			break;` |
|     1 | 2763 | `		case 'M':{` |
|     3 | 2764 | `			int k = DtEatName(&z,zInEnd,azMon3,12);` |
|     3 | 2765 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2766 | `			break;` |
|     - | 2767 | `				 }` |
|     1 | 2768 | `		case 'F':{` |
|     3 | 2769 | `			int k = DtEatName(&z,zInEnd,azMonFull,12);` |
|     3 | 2770 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2771 | `			break;` |
|     - | 2772 | `				 }` |
|   ! 0 | 2773 | `		case 'y':` |
|   ! 0 | 2774 | `			if( DtEatDigits(&z,zInEnd,2,2,&y) ){` |
|   ! 0 | 2775 | `				y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2776 | `			}else{` |
|   ! 0 | 2777 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit year could not be found");` |
|   ! 0 | 2778 | `				if( DtHuntDigits(&z,zInEnd,2,2,&y) < 0 ){` |
|   ! 0 | 2779 | `					DT_FF_LOGERR(nIn,"A two digit year could not be found");` |
|   ! 0 | 2780 | `				}else if( y >= 0 ){` |
|   ! 0 | 2781 | `					y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2782 | `				}` |
|     - | 2783 | `			}` |
|   ! 0 | 2784 | `			break;` |
|    23 | 2785 | `		case 'Y':{` |
|    47 | 2786 | `			int neg = 0;` |
|    47 | 2787 | `			if( z < zInEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|    47 | 2788 | `			if( DtEatDigits(&z,zInEnd,1,4,&y) ){` |
|    43 | 2789 | `				if( neg ){ y = -y; }` |
|    22 | 2790 | `			}else{` |
|     5 | 2791 | `				DT_FF_LOGERR((int)(z - zIn),"A four digit year could not be found");` |
|     5 | 2792 | `				if( DtHuntDigits(&z,zInEnd,1,4,&y) < 0 ){` |
|     9 | 2793 | `					DT_FF_LOGERR(nIn,"A four digit year could not be found");` |
|     2 | 2794 | `				}` |
|     - | 2795 | `			}` |
|    47 | 2796 | `			break;` |
|     - | 2797 | `				 }` |
|     6 | 2798 | `		case 'H': case 'G':` |
|    13 | 2799 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h) ){` |
|   ! 0 | 2800 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2801 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h) < 0 ){` |
|   ! 0 | 2802 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2803 | `				}` |
|   ! 0 | 2804 | `			}` |
|    13 | 2805 | `			break;` |
|     2 | 2806 | `		case 'h': case 'g':` |
|     5 | 2807 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h12) ){` |
|   ! 0 | 2808 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2809 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h12) < 0 ){` |
|   ! 0 | 2810 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2811 | `				}` |
|   ! 0 | 2812 | `			}` |
|     5 | 2813 | `			break;` |
|     8 | 2814 | `		case 'i':` |
|    17 | 2815 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mi) ){` |
|   ! 0 | 2816 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit minute could not be found");` |
|   ! 0 | 2817 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mi) < 0 ){` |
|   ! 0 | 2818 | `					DT_FF_LOGERR(nIn,"A two digit minute could not be found");` |
|   ! 0 | 2819 | `				}` |
|   ! 0 | 2820 | `			}` |
|    17 | 2821 | `			break;` |
|     2 | 2822 | `		case 's':` |
|     5 | 2823 | `			if( !DtEatDigits(&z,zInEnd,1,2,&s) ){` |
|   ! 0 | 2824 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit second could not be found");` |
|   ! 0 | 2825 | `				if( DtHuntDigits(&z,zInEnd,1,2,&s) < 0 ){` |
|   ! 0 | 2826 | `					DT_FF_LOGERR(nIn,"A two digit second could not be found");` |
|   ! 0 | 2827 | `				}` |
|   ! 0 | 2828 | `			}` |
|     5 | 2829 | `			break;` |
|   ! 0 | 2830 | `		case 'u':` |
|     - | 2831 | `			/* micro parsed then dropped: PHL keeps whole seconds (recorded) */` |
|   ! 0 | 2832 | `			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){` |
|   ! 0 | 2833 | `				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");` |
|   ! 0 | 2834 | `				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){` |
|   ! 0 | 2835 | `					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");` |
|   ! 0 | 2836 | `				}` |
|   ! 0 | 2837 | `			}` |
|   ! 0 | 2838 | `			break;` |
|   ! 0 | 2839 | `		case 'v':` |
|   ! 0 | 2840 | `			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){` |
|   ! 0 | 2841 | `				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");` |
|   ! 0 | 2842 | `				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){` |
|   ! 0 | 2843 | `					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");` |
|   ! 0 | 2844 | `				}` |
|   ! 0 | 2845 | `			}` |
|   ! 0 | 2846 | `			break;` |
|     2 | 2847 | `		case 'a': case 'A':{` |
|     - | 2848 | `			static const char *azMer[] = {"am","pm","a.m.","p.m."};` |
|     5 | 2849 | `			int k = DtEatName(&z,zInEnd,azMer,4);` |
|     5 | 2850 | `			if( k ){` |
|     5 | 2851 | `				iMeridiem = ((k - 1) & 1);` |
|     3 | 2852 | `			}else{` |
|   ! 0 | 2853 | `				zErr = "A meridian could not be found";` |
|     - | 2854 | `			}` |
|     5 | 2855 | `			break;` |
|     - | 2856 | `				 }` |
|     2 | 2857 | `		case 'U':{` |
|     5 | 2858 | `			int neg = 0;` |
|     5 | 2859 | `			if( z < zInEnd && z[0]=='-' ){ neg = 1; z++; }` |
|     5 | 2860 | `			if( DtEatDigits(&z,zInEnd,1,19,&uVal) ){` |
|     5 | 2861 | `				if( neg ){ uVal = -uVal; }` |
|     5 | 2862 | `				bHasU = 1;` |
|     3 | 2863 | `			}else{` |
|   ! 0 | 2864 | `				DT_FF_LOGERR((int)(z - zIn),"A unix timestamp could not be found");` |
|   ! 0 | 2865 | `				if( DtHuntDigits(&z,zInEnd,1,19,&uVal) < 0 ){` |
|   ! 0 | 2866 | `					DT_FF_LOGERR(nIn,"A unix timestamp could not be found");` |
|   ! 0 | 2867 | `				}else{` |
|   ! 0 | 2868 | `					if( neg ){ uVal = -uVal; }` |
|   ! 0 | 2869 | `					bHasU = 1;` |
|     - | 2870 | `				}` |
|     - | 2871 | `			}` |
|     5 | 2872 | `			break;` |
|     - | 2873 | `				 }` |
|     1 | 2874 | `		case 'e': case 'T':{` |
|     - | 2875 | `			static const char *azZone[] = {"UTC","GMT","Z"};` |
|     3 | 2876 | `			int k = DtEatName(&z,zInEnd,azZone,3);` |
|     3 | 2877 | `			if( k == 3 ){` |
|   ! 0 | 2878 | `				iOffKind = 2; iOffVal = 0;` |
|     3 | 2879 | `			}else if( k ){` |
|     3 | 2880 | `				iOffKind = 3; iOffVal = 0;` |
|     3 | 2881 | `				SyMemcpy(azZone[k-1],zName,4);` |
|     1 | 2882 | `			}else if( z < zInEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|   ! 0 | 2883 | `				goto parse_num_off;` |
|   ! 0 | 2884 | `			}else{` |
|   ! 0 | 2885 | `				zErr = "The timezone could not be found in the database";` |
|     - | 2886 | `			}` |
|     3 | 2887 | `			break;` |
|     2 | 2888 | `				 }` |
|     - | 2889 | `		case 'O': case 'P':` |
|     2 | 2890 | `parse_num_off:	{` |
|     5 | 2891 | `			int sign,oh,om = 0;` |
|     - | 2892 | `			sxi64 t;` |
|     5 | 2893 | `			if( z >= zInEnd \|\| (z[0] != '+' && z[0] != '-') ){` |
|   ! 0 | 2894 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2895 | `				break;` |
|     - | 2896 | `			}` |
|     5 | 2897 | `			sign = (z[0]=='-') ? -1 : 1;` |
|     5 | 2898 | `			z++;` |
|     5 | 2899 | `			if( !DtEatDigits(&z,zInEnd,2,2,&t) ){` |
|   ! 0 | 2900 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2901 | `				break;` |
|     - | 2902 | `			}` |
|     5 | 2903 | `			oh = (int)t;` |
|     5 | 2904 | `			if( z < zInEnd && z[0]==':' ){ z++; }` |
|     5 | 2905 | `			if( DtEatDigits(&z,zInEnd,2,2,&t) ){ om = (int)t; }` |
|     5 | 2906 | `			iOffKind = 1;` |
|     5 | 2907 | `			iOffVal = sign * (oh*3600 + om*60);` |
|     5 | 2908 | `			break;` |
|     - | 2909 | `				 }` |
|   ! 0 | 2910 | `		case '?':` |
|   ! 0 | 2911 | `			if( z < zInEnd ){ z++; }` |
|   ! 0 | 2912 | `			break;` |
|   ! 0 | 2913 | `		case '*':` |
|     - | 2914 | `			/* skip input until the next separator byte */` |
|   ! 0 | 2915 | `			while( z < zInEnd && !SyisDigit(z[0]) && z[0] != ';' && z[0] != ':'` |
|   ! 0 | 2916 | `			 && z[0] != '/' && z[0] != '.' && z[0] != ',' && z[0] != '-'` |
|   ! 0 | 2917 | `			 && z[0] != '(' && z[0] != ')' && z[0] != ' ' ){` |
|   ! 0 | 2918 | `				z++;` |
|   ! 0 | 2919 | `			}` |
|   ! 0 | 2920 | `			break;` |
|     1 | 2921 | `		case '#':` |
|     3 | 2922 | `			if( z < zInEnd && (z[0]==';'\|\|z[0]==':'\|\|z[0]=='/'\|\|z[0]=='.'` |
|   ! 0 | 2923 | `			 \|\|z[0]==','\|\|z[0]=='-'\|\|z[0]=='('\|\|z[0]==')') ){` |
|     3 | 2924 | `				z++;` |
|     2 | 2925 | `			}else{` |
|   ! 0 | 2926 | `				zErr = "The separation symbol could not be found";` |
|     - | 2927 | `			}` |
|     3 | 2928 | `			break;` |
|     1 | 2929 | `		case '\\':` |
|     3 | 2930 | `			if( zFmt < zEnd ){` |
|     3 | 2931 | `				if( z < zInEnd && z[0] == zFmt[0] ){` |
|     3 | 2932 | `					z++;` |
|     3 | 2933 | `					zFmt++;` |
|     2 | 2934 | `				}else{` |
|     - | 2935 | `					/* a literal mismatch aborts timelib's scan */` |
|   ! 0 | 2936 | `					DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|   ! 0 | 2937 | `					zFmt = zEnd;` |
|   ! 0 | 2938 | `					bAborted = 1;` |
|     - | 2939 | `				}` |
|     1 | 2940 | `			}` |
|     3 | 2941 | `			break;` |
|    46 | 2942 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|     - | 2943 | `		case '(' : case ')':` |
|    93 | 2944 | `			if( z < zInEnd && z[0] == c ){` |
|    93 | 2945 | `				z++;` |
|    47 | 2946 | `			}else{` |
|     - | 2947 | `				/* timelib logs BOTH messages (count +2, last-wins on the` |
|     - | 2948 | `				 * position), consumes the offending byte, and keeps going */` |
|   ! 0 | 2949 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 2950 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 2951 | `				z++;` |
|     - | 2952 | `			}` |
|    93 | 2953 | `			break;` |
|    15 | 2954 | `		case ' ':` |
|    31 | 2955 | `			if( z < zInEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|    31 | 2956 | `				z++;` |
|    16 | 2957 | `			}else{` |
|   ! 0 | 2958 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 2959 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 2960 | `				z++;` |
|     - | 2961 | `			}` |
|    31 | 2962 | `			break;` |
|     1 | 2963 | `		default:` |
|     - | 2964 | `			/* any other format byte must match the input verbatim; a mismatch` |
|     - | 2965 | `			 * aborts timelib's scan */` |
|     3 | 2966 | `			if( z < zInEnd && z[0] == c ){` |
|   ! 0 | 2967 | `				z++;` |
|   ! 0 | 2968 | `			}else{` |
|     3 | 2969 | `				DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|     3 | 2970 | `				zFmt = zEnd;` |
|     3 | 2971 | `				bAborted = 1;` |
|     - | 2972 | `			}` |
|     2 | 2973 | `			break;` |
|     - | 2974 | `		}` |
|   311 | 2975 | `		if( zErr ){` |
|     - | 2976 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|   ! 0 | 2977 | `			DT_FF_LOGERR((int)(z - zIn),zErr);` |
|   ! 0 | 2978 | `		}` |
|     1 | 2979 | `	}` |
|    57 | 2980 | `	if( z < zInEnd && !bAborted ){` |
|     5 | 2981 | `		if( bPlus ){` |
|     - | 2982 | `			/* '+' downgrades trailing data to a warning */` |
|     3 | 2983 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     3 | 2984 | `			aWarnMsg[nWarn] = "Trailing data";` |
|     3 | 2985 | `			nWarn++;` |
|     2 | 2986 | `		}else{` |
|     3 | 2987 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|     - | 2988 | `		}` |
|     2 | 2989 | `	}` |
|    57 | 2990 | `	if( nErr > 0 ){` |
|     - | 2991 | `		SyBlob sOut;` |
|     - | 2992 | `		int k;` |
|     9 | 2993 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     9 | 2994 | `		SyBlobFormat(&sOut,"%d",nErr);` |
|    21 | 2995 | `		for( k = 0 ; k < nErrKept ; k++ ){` |
|    13 | 2996 | `			SyBlobFormat(&sOut,"\n%d\t%s",aErrPos[k],aErrMsg[k]);` |
|     7 | 2997 | `		}` |
|     9 | 2998 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     9 | 2999 | `		SyBlobRelease(&sOut);` |
|     9 | 3000 | `		return PH7_OK;` |
|     - | 3001 | `	}` |
|    49 | 3002 | `	if( bPipe ){` |
|     5 | 3003 | `		if( y < 0 ){ y = 1970; }` |
|     5 | 3004 | `		if( mo < 0 ){ mo = 1; }` |
|     5 | 3005 | `		if( d < 0 ){ d = 1; }` |
|     5 | 3006 | `		if( h < 0 && h12 < 0 ){ h = 0; }` |
|     5 | 3007 | `		if( mi < 0 ){ mi = 0; }` |
|     5 | 3008 | `		if( s < 0 ){ s = 0; }` |
|     2 | 3009 | `	}` |
|     - | 3010 | `	{` |
|     - | 3011 | `		/* remaining unset fields come from "now" in the default offset */` |
|    49 | 3012 | `		sxi64 iLocal = iNow + iDefOff;` |
|    49 | 3013 | `		sxi64 days = DtFloorDiv(iLocal,86400);` |
|    49 | 3014 | `		sxi64 secs = iLocal - days*86400;` |
|     - | 3015 | `		sxi64 ny;` |
|     - | 3016 | `		int nmo,nd;` |
|    49 | 3017 | `		DtCivilFromDays(days,&ny,&nmo,&nd);` |
|    49 | 3018 | `		if( y < 0 ){ y = ny; }` |
|    49 | 3019 | `		if( mo < 0 ){ mo = nmo; }` |
|    49 | 3020 | `		if( d < 0 ){ d = nd; }` |
|    49 | 3021 | `		if( h12 >= 0 ){` |
|     5 | 3022 | `			h = (h12 % 12) + ((iMeridiem == 1) ? 12 : 0);` |
|     2 | 3023 | `		}` |
|     - | 3024 | `		/* php: parsing a time component zeroes the finer unset units */` |
|    49 | 3025 | `		if( h >= 0 ){` |
|    25 | 3026 | `			if( mi < 0 ){ mi = 0; }` |
|    25 | 3027 | `			if( s < 0 ){ s = 0; }` |
|    37 | 3028 | `		}else if( mi >= 0 ){` |
|     3 | 3029 | `			if( s < 0 ){ s = 0; }` |
|     1 | 3030 | `		}` |
|    49 | 3031 | `		if( h < 0 ){ h = secs / 3600; }` |
|    49 | 3032 | `		if( mi < 0 ){ mi = (secs / 60) % 60; }` |
|    49 | 3033 | `		if( s < 0 ){ s = secs % 60; }` |
|     - | 3034 | `	}` |
|     - | 3035 | `	/* php validates the RESOLVED fields and warns (parse still succeeds,` |
|     - | 3036 | `	 * values roll over via civil arithmetic) */` |
|    49 | 3037 | `	if( mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo) ){` |
|     3 | 3038 | `		if( nWarn < 3 ){` |
|     3 | 3039 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 3040 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|     3 | 3041 | `			nWarn++;` |
|     1 | 3042 | `		}` |
|     1 | 3043 | `	}` |
|    49 | 3044 | `	if( h > 24 \|\| mi > 59 \|\| s > 59 ){` |
|     3 | 3045 | `		if( nWarn < 3 ){` |
|     3 | 3046 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 3047 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|     3 | 3048 | `			nWarn++;` |
|     1 | 3049 | `		}` |
|     1 | 3050 | `	}` |
|     - | 3051 | `	{` |
|    49 | 3052 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    49 | 3053 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|     - | 3054 | `		sxi64 iTs;` |
|    49 | 3055 | `		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;` |
|    49 | 3056 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 3057 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 3058 | `		}` |
|    49 | 3059 | `		if( bHasU ){` |
|     5 | 3060 | `			iTs = uVal;` |
|     5 | 3061 | `			iUseOff = 0;` |
|     5 | 3062 | `			iOffKind = 1;` |
|     3 | 3063 | `		}else{` |
|    45 | 3064 | `			iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);` |
|     - | 3065 | `		}` |
|    49 | 3066 | `		ph7_value_int64(pV,iTs);           ph7_array_add_elem(pArr,0,pV);` |
|    49 | 3067 | `		ph7_value_int64(pV,iUseOff);       ph7_array_add_elem(pArr,0,pV);` |
|    49 | 3068 | `		ph7_value_int64(pV,iOffKind);      ph7_array_add_elem(pArr,0,pV);` |
|    49 | 3069 | `		ph7_value_string(pV,zName,-1);     ph7_array_add_elem(pArr,0,pV);` |
|     - | 3070 | `		{` |
|     - | 3071 | `			int k;` |
|    55 | 3072 | `			for( k = 0 ; k < nWarn ; k++ ){` |
|     7 | 3073 | `				ph7_value_int64(pV,aWarnPos[k]);` |
|     7 | 3074 | `				ph7_array_add_elem(pArr,0,pV);` |
|     7 | 3075 | `				ph7_value_string(pV,aWarnMsg[k],-1);` |
|     7 | 3076 | `				ph7_array_add_elem(pArr,0,pV);` |
|     4 | 3077 | `			}` |
|     - | 3078 | `		}` |
|    49 | 3079 | `		ph7_result_value(pCtx,pArr);` |
|     - | 3080 | `	}` |
|    49 | 3081 | `	return PH7_OK;` |
|    29 | 3082 | `}` |
|     - | 3083 | `/*` |
|     - | 3084 | ` * The embedded DateTime library. Timezone scope: UTC + fixed offsets.` |
|     - | 3085 | ` */` |
|     - | 3086 | `static const char zDateTimeLib[] =` |
|     - | 3087 | `"class DateException extends Exception {}"` |
|     - | 3088 | `"class DateMalformedStringException extends DateException {}"` |
|     - | 3089 | `"class DateInvalidTimeZoneException extends DateException {}"` |
|     - | 3090 | `"class DateMalformedIntervalStringException extends DateException {}"` |
|     - | 3091 | `"class DateMalformedPeriodStringException extends DateException {}"` |
|     - | 3092 | `"interface DateTimeInterface {"` |
|     - | 3093 | `" const ATOM = 'Y-m-d\\TH:i:sP';"` |
|     - | 3094 | `" const COOKIE = 'l, d-M-Y H:i:s T';"` |
|     - | 3095 | `" const ISO8601 = 'Y-m-d\\TH:i:sO';"` |
|     - | 3096 | `" const ISO8601_EXPANDED = 'X-m-d\\TH:i:sP';"` |
|     - | 3097 | `" const RFC822 = 'D, d M y H:i:s O';"` |
|     - | 3098 | `" const RFC850 = 'l, d-M-y H:i:s T';"` |
|     - | 3099 | `" const RFC1036 = 'D, d M y H:i:s O';"` |
|     - | 3100 | `" const RFC1123 = 'D, d M Y H:i:s O';"` |
|     - | 3101 | `" const RFC7231 = 'D, d M Y H:i:s \\G\\M\\T';"` |
|     - | 3102 | `" const RFC2822 = 'D, d M Y H:i:s O';"` |
|     - | 3103 | `" const RFC3339 = 'Y-m-d\\TH:i:sP';"` |
|     - | 3104 | `" const RFC3339_EXTENDED = 'Y-m-d\\TH:i:s.vP';"` |
|     - | 3105 | `" const RSS = 'D, d M Y H:i:s O';"` |
|     - | 3106 | `" const W3C = 'Y-m-d\\TH:i:sP';"` |
|     - | 3107 | `"}"` |
|     - | 3108 | `"class DateTimeZone {"` |
|     - | 3109 | `" private $__dtzOff = 0;"` |
|     - | 3110 | `" private $__dtzName = 'UTC';"` |
|     - | 3111 | `" public function __construct($timezone = 'UTC'){"` |
|     - | 3112 | `"  $tz = (string)$timezone;"` |
|     - | 3113 | `"  if( strcasecmp($tz, 'UTC') === 0 ){"` |
|     - | 3114 | `"   $this->__dtzOff = 0; $this->__dtzName = 'UTC';"` |
|     - | 3115 | `"   return;"` |
|     - | 3116 | `"  }"` |
|     - | 3117 | `"  if( $tz === 'Z' ){"` |
|     - | 3118 | `"   $this->__dtzOff = 0; $this->__dtzName = 'Z';"` |
|     - | 3119 | `"   return;"` |
|     - | 3120 | `"  }"` |
|     - | 3121 | `"  if( strcasecmp($tz, 'GMT') === 0 ){"` |
|     - | 3122 | `"   $this->__dtzOff = 0; $this->__dtzName = 'GMT';"` |
|     - | 3123 | `"   return;"` |
|     - | 3124 | `"  }"` |
|     - | 3125 | `"  $m = null;"` |
|     - | 3126 | `"  if( preg_match('/^([+-])(\\d{2}):?(\\d{2})$/', $tz, $m) ){"` |
|     - | 3127 | `"   $off = ((int)$m[2]) * 3600 + ((int)$m[3]) * 60;"` |
|     - | 3128 | `"   if( $m[1] === '-' ){ $off = -$off; }"` |
|     - | 3129 | `"   $this->__dtzOff = $off;"` |
|     - | 3130 | `"   $this->__dtzName = $m[1] . $m[2] . ':' . $m[3];"` |
|     - | 3131 | `"   return;"` |
|     - | 3132 | `"  }"` |
|     - | 3133 | `"  throw new DateInvalidTimeZoneException("` |
|     - | 3134 | `"   'DateTimeZone::__construct(): Unknown or bad timezone (' . $tz . ')');"` |
|     - | 3135 | `" }"` |
|     - | 3136 | `" public function getName(){ return $this->__dtzName; }"` |
|     - | 3137 | `" public function getOffset($datetime = null){ return $this->__dtzOff; }"` |
|     - | 3138 | `"}"` |
|     - | 3139 | `"trait __DtCoreT {"` |
|     - | 3140 | `" private $__dtTs = 0;"` |
|     - | 3141 | `" private $__dtOff = 0;"` |
|     - | 3142 | `" private $__dtName = 'UTC';"` |
|     - | 3143 | `" private function __dtInit($datetime, $timezone){"` |
|     - | 3144 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 3145 | `"  if( $timezone !== null ){"` |
|     - | 3146 | `"   $off = $timezone->getOffset($this);"` |
|     - | 3147 | `"   $name = $timezone->getName();"` |
|     - | 3148 | `"  }"` |
|     - | 3149 | `"  $r = __dt_parse((string)$datetime, __dt_now(), $off);"` |
|     - | 3150 | `"  if( is_string($r) ){ throw new DateMalformedStringException($r); }"` |
|     - | 3151 | `"  $this->__dtTs = $r[0];"` |
|     - | 3152 | `"  if( $r[2] ){"` |
|     - | 3153 | `"   $this->__dtOff = $r[1];"` |
|     - | 3154 | `"   $this->__dtName = $r[2] === 2 ? 'Z' : $this->__dtOffName($r[1]);"` |
|     - | 3155 | `"  }else{"` |
|     - | 3156 | `"   $this->__dtOff = $off;"` |
|     - | 3157 | `"   $this->__dtName = $name;"` |
|     - | 3158 | `"  }"` |
|     - | 3159 | `" }"` |
|     - | 3160 | `" private function __dtOffName($off){"` |
|     - | 3161 | `"  $s = $off < 0 ? '-' : '+';"` |
|     - | 3162 | `"  $a = $off < 0 ? -$off : $off;"` |
|     - | 3163 | `"  return $s . sprintf('%02d:%02d', intdiv($a, 3600), intdiv($a % 3600, 60));"` |
|     - | 3164 | `" }"` |
|     - | 3165 | `" public function format($format){ return __dt_format($this->__dtTs, $this->__dtOff, $this->__dtName, (string)$format); }"` |
|     - | 3166 | `" public function getTimestamp(){ return $this->__dtTs; }"` |
|     - | 3167 | `" public function getOffset(){ return $this->__dtOff; }"` |
|     - | 3168 | `" public function getTimezone(){ return new DateTimeZone($this->__dtName); }"` |
|     - | 3169 | `" public function diff($targetObject, $absolute = false){"` |
|     - | 3170 | `"  $r = __dt_civil_diff($this->__dtTs, $this->__dtOff, $targetObject->getTimestamp());"` |
|     - | 3171 | `"  $iv = new DateInterval('P0D');"` |
|     - | 3172 | `"  $iv->y = $r[0]; $iv->m = $r[1]; $iv->d = $r[2];"` |
|     - | 3173 | `"  $iv->h = $r[3]; $iv->i = $r[4]; $iv->s = $r[5];"` |
|     - | 3174 | `"  $iv->days = $r[6];"` |
|     - | 3175 | `"  $iv->invert = $absolute ? 0 : $r[7];"` |
|     - | 3176 | `"  return $iv;"` |
|     - | 3177 | `" }"` |
|     - | 3178 | `" private function __dtAddTs($interval, $sign){"` |
|     - | 3179 | `"  if( $interval->invert ){ $sign = -$sign; }"` |
|     - | 3180 | `"  return __dt_civil_add($this->__dtTs, $this->__dtOff, $interval->y, $interval->m,"` |
|     - | 3181 | `"   $interval->d, $interval->h, $interval->i, $interval->s, $sign);"` |
|     - | 3182 | `" }"` |
|     - | 3183 | `" private static function __dtFromFormat($format, $datetime, $timezone, $class){"` |
|     - | 3184 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 3185 | `"  if( $timezone !== null ){"` |
|     - | 3186 | `"   $off = $timezone->getOffset(null);"` |
|     - | 3187 | `"   $name = $timezone->getName();"` |
|     - | 3188 | `"  }"` |
|     - | 3189 | `"  $r = __dt_from_format((string)$format, (string)$datetime, __dt_now(), $off);"` |
|     - | 3190 | `"  if( is_string($r) ){"` |
|     - | 3191 | `"   $lines = explode(\"\\n\", $r);"` |
|     - | 3192 | `"   $errs = [];"` |
|     - | 3193 | `"   $nl = count($lines);"` |
|     - | 3194 | `"   for( $k = 1; $k < $nl; $k++ ){"` |
|     - | 3195 | `"    $p = strpos($lines[$k], \"\\t\");"` |
|     - | 3196 | `"    $errs[(int)substr($lines[$k], 0, $p)] = substr($lines[$k], $p + 1);"` |
|     - | 3197 | `"   }"` |
|     - | 3198 | `"   DateTime::$__dtLastErr = ['warning_count' => 0, 'warnings' => [],"` |
|     - | 3199 | `"    'error_count' => (int)$lines[0], 'errors' => $errs];"` |
|     - | 3200 | `"   return false;"` |
|     - | 3201 | `"  }"` |
|     - | 3202 | `"  if( isset($r[4]) ){"` |
|     - | 3203 | `"   $warns = [];"` |
|     - | 3204 | `"   $wc = 0;"` |
|     - | 3205 | `"   for( $k = 4; isset($r[$k]); $k += 2 ){"` |
|     - | 3206 | `"    $warns[$r[$k]] = $r[$k + 1];"` |
|     - | 3207 | `"    $wc++;"` |
|     - | 3208 | `"   }"` |
|     - | 3209 | `"   DateTime::$__dtLastErr = ['warning_count' => $wc, 'warnings' => $warns,"` |
|     - | 3210 | `"    'error_count' => 0, 'errors' => []];"` |
|     - | 3211 | `"  }else{"` |
|     - | 3212 | `"   DateTime::$__dtLastErr = false;"` |
|     - | 3213 | `"  }"` |
|     - | 3214 | `"  $obj = new $class('@0');"` |
|     - | 3215 | `"  $obj->__dtTs = $r[0];"` |
|     - | 3216 | `"  if( $r[2] === 0 ){ $obj->__dtOff = $off; $obj->__dtName = $name; }"` |
|     - | 3217 | `"  elseif( $r[2] === 2 ){ $obj->__dtOff = 0; $obj->__dtName = 'Z'; }"` |
|     - | 3218 | `"  elseif( $r[2] === 3 ){ $obj->__dtOff = $r[1]; $obj->__dtName = $r[3]; }"` |
|     - | 3219 | `"  else { $obj->__dtOff = $r[1]; $obj->__dtName = $obj->__dtOffName($r[1]); }"` |
|     - | 3220 | `"  return $obj;"` |
|     - | 3221 | `" }"` |
|     - | 3222 | `" private static function __dtCopyOf($object, $class){"` |
|     - | 3223 | `"  $d = new $class('@0');"` |
|     - | 3224 | `"  $d->__dtTs = $object->getTimestamp();"` |
|     - | 3225 | `"  $d->__dtOff = $object->getOffset();"` |
|     - | 3226 | `"  $d->__dtName = $object->getTimezone()->getName();"` |
|     - | 3227 | `"  return $d;"` |
|     - | 3228 | `" }"` |
|     - | 3229 | `"}"` |
|     - | 3230 | `"class DateTime implements DateTimeInterface {"` |
|     - | 3231 | `" use __DtCoreT;"` |
|     - | 3232 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 3233 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 3234 | `" }"` |
|     - | 3235 | `" public function modify($modifier){"` |
|     - | 3236 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 3237 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTime::modify(): ' . $r); }"` |
|     - | 3238 | `"  $this->__dtTs = $r[0];"` |
|     - | 3239 | `"  return $this;"` |
|     - | 3240 | `" }"` |
|     - | 3241 | `" public function setTimestamp($timestamp){ $this->__dtTs = (int)$timestamp; return $this; }"` |
|     - | 3242 | `" public function setTimezone($timezone){"` |
|     - | 3243 | `"  $this->__dtOff = $timezone->getOffset($this);"` |
|     - | 3244 | `"  $this->__dtName = $timezone->getName();"` |
|     - | 3245 | `"  return $this;"` |
|     - | 3246 | `" }"` |
|     - | 3247 | `" public function setDate($year, $month, $day){"` |
|     - | 3248 | `"  $this->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 3249 | `"  return $this;"` |
|     - | 3250 | `" }"` |
|     - | 3251 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3252 | `"  $this->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 3253 | `"  return $this;"` |
|     - | 3254 | `" }"` |
|     - | 3255 | `" public function add($interval){ $this->__dtTs = $this->__dtAddTs($interval, 1); return $this; }"` |
|     - | 3256 | `" public function sub($interval){ $this->__dtTs = $this->__dtAddTs($interval, -1); return $this; }"` |
|     - | 3257 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 3258 | `"  $this->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 3259 | `"  return $this;"` |
|     - | 3260 | `" }"` |
|     - | 3261 | `" public static $__dtLastErr = false;"` |
|     - | 3262 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 3263 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 3264 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTime');"` |
|     - | 3265 | `" }"` |
|     - | 3266 | `" public static function createFromImmutable($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 3267 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 3268 | `"}"` |
|     - | 3269 | `"class DateTimeImmutable implements DateTimeInterface {"` |
|     - | 3270 | `" use __DtCoreT;"` |
|     - | 3271 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 3272 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 3273 | `" }"` |
|     - | 3274 | `" public function modify($modifier){"` |
|     - | 3275 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 3276 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTimeImmutable::modify(): ' . $r); }"` |
|     - | 3277 | `"  $c = clone $this;"` |
|     - | 3278 | `"  $c->__dtTs = $r[0];"` |
|     - | 3279 | `"  return $c;"` |
|     - | 3280 | `" }"` |
|     - | 3281 | `" public function setTimestamp($timestamp){ $c = clone $this; $c->__dtTs = (int)$timestamp; return $c; }"` |
|     - | 3282 | `" public function setTimezone($timezone){"` |
|     - | 3283 | `"  $c = clone $this;"` |
|     - | 3284 | `"  $c->__dtOff = $timezone->getOffset($this);"` |
|     - | 3285 | `"  $c->__dtName = $timezone->getName();"` |
|     - | 3286 | `"  return $c;"` |
|     - | 3287 | `" }"` |
|     - | 3288 | `" public function setDate($year, $month, $day){"` |
|     - | 3289 | `"  $c = clone $this;"` |
|     - | 3290 | `"  $c->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 3291 | `"  return $c;"` |
|     - | 3292 | `" }"` |
|     - | 3293 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3294 | `"  $c = clone $this;"` |
|     - | 3295 | `"  $c->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 3296 | `"  return $c;"` |
|     - | 3297 | `" }"` |
|     - | 3298 | `" public function add($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, 1); return $c; }"` |
|     - | 3299 | `" public function sub($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, -1); return $c; }"` |
|     - | 3300 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 3301 | `"  $c = clone $this;"` |
|     - | 3302 | `"  $c->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 3303 | `"  return $c;"` |
|     - | 3304 | `" }"` |
|     - | 3305 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 3306 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 3307 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTimeImmutable');"` |
|     - | 3308 | `" }"` |
|     - | 3309 | `" public static function createFromMutable($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 3310 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 3311 | `"}"` |
|     - | 3312 | `"function date_create($datetime = 'now', $timezone = null){"` |
|     - | 3313 | `" try { return new DateTime($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 3314 | `"}"` |
|     - | 3315 | `"function date_create_immutable($datetime = 'now', $timezone = null){"` |
|     - | 3316 | `" try { return new DateTimeImmutable($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 3317 | `"}"` |
|     - | 3318 | `/* Procedural aliases of the createFromFormat statics: same (format, datetime,` |
|     - | 3319 | ` * ?timezone) order, returning false on failure like php. */` |
|     - | 3320 | `"function date_create_from_format($format, $datetime, $timezone = null){"` |
|     - | 3321 | `" return DateTime::createFromFormat($format, $datetime, $timezone);"` |
|     - | 3322 | `"}"` |
|     - | 3323 | `"function date_create_immutable_from_format($format, $datetime, $timezone = null){"` |
|     - | 3324 | `" return DateTimeImmutable::createFromFormat($format, $datetime, $timezone);"` |
|     - | 3325 | `"}"` |
|     - | 3326 | `"class DateInterval {"` |
|     - | 3327 | `" public $y = 0;"` |
|     - | 3328 | `" public $m = 0;"` |
|     - | 3329 | `" public $d = 0;"` |
|     - | 3330 | `" public $h = 0;"` |
|     - | 3331 | `" public $i = 0;"` |
|     - | 3332 | `" public $s = 0;"` |
|     - | 3333 | `" public $f = 0;"` |
|     - | 3334 | `" public $invert = 0;"` |
|     - | 3335 | `" public $days = false;"` |
|     - | 3336 | `" public $from_string = false;"` |
|     - | 3337 | `" public function __construct($duration = 'P0D'){"` |
|     - | 3338 | `"  $dur = (string)$duration;"` |
|     - | 3339 | `"  $mm = null;"` |
|     - | 3340 | `"  if( strlen($dur) < 2 \|\| substr($dur, -1) === 'T'"` |
|     - | 3341 | `"   \|\| !preg_match('/^P(?:(\\d+)Y)?(?:(\\d+)M)?(?:(\\d+)W)?(?:(\\d+)D)?(?:T(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?)?$/', $dur, $mm) ){"` |
|     - | 3342 | `"   throw new DateMalformedIntervalStringException('Unknown or bad format (' . $dur . ')');"` |
|     - | 3343 | `"  }"` |
|     - | 3344 | `"  $this->y = (int)($mm[1] ?? 0);"` |
|     - | 3345 | `"  $this->m = (int)($mm[2] ?? 0);"` |
|     - | 3346 | `"  $this->d = (int)($mm[4] ?? 0) + 7 * (int)($mm[3] ?? 0);"` |
|     - | 3347 | `"  $this->h = (int)($mm[5] ?? 0);"` |
|     - | 3348 | `"  $this->i = (int)($mm[6] ?? 0);"` |
|     - | 3349 | `"  $this->s = (int)($mm[7] ?? 0);"` |
|     - | 3350 | `" }"` |
|     - | 3351 | `" public static function createFromDateString($datetime){"` |
|     - | 3352 | `"  $s = trim((string)$datetime);"` |
|     - | 3353 | `"  $iv = new DateInterval('P0D');"` |
|     - | 3354 | `"  $rest = $s;"` |
|     - | 3355 | `"  $any = false;"` |
|     - | 3356 | `"  while( $rest !== '' ){"` |
|     - | 3357 | `"   $mm = null;"` |
|     - | 3358 | `"   if( !preg_match('/^[\\s,+]*([+-]?\\d+)\\s*(sec\|secs\|second\|seconds\|min\|mins\|minute\|minutes\|hour\|hours\|day\|days\|week\|weeks\|fortnight\|fortnights\|month\|months\|year\|years)\\b/i', $rest, $mm) ){"` |
|     - | 3359 | `"    throw new DateMalformedIntervalStringException("` |
|     - | 3360 | `"     'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 3361 | `"   }"` |
|     - | 3362 | `"   $n = (int)$mm[1];"` |
|     - | 3363 | `"   $u = strtolower($mm[2]);"` |
|     - | 3364 | `"   if( $u === 'sec' \|\| $u === 'secs' \|\| $u === 'second' \|\| $u === 'seconds' ){ $iv->s += $n; }"` |
|     - | 3365 | `"   elseif( $u === 'min' \|\| $u === 'mins' \|\| $u === 'minute' \|\| $u === 'minutes' ){ $iv->i += $n; }"` |
|     - | 3366 | `"   elseif( $u === 'hour' \|\| $u === 'hours' ){ $iv->h += $n; }"` |
|     - | 3367 | `"   elseif( $u === 'day' \|\| $u === 'days' ){ $iv->d += $n; }"` |
|     - | 3368 | `"   elseif( $u === 'week' \|\| $u === 'weeks' ){ $iv->d += 7 * $n; }"` |
|     - | 3369 | `"   elseif( $u === 'fortnight' \|\| $u === 'fortnights' ){ $iv->d += 14 * $n; }"` |
|     - | 3370 | `"   elseif( $u === 'month' \|\| $u === 'months' ){ $iv->m += $n; }"` |
|     - | 3371 | `"   else { $iv->y += $n; }"` |
|     - | 3372 | `"   $any = true;"` |
|     - | 3373 | `"   $rest = ltrim(substr($rest, strlen($mm[0])));"` |
|     - | 3374 | `"  }"` |
|     - | 3375 | `"  if( !$any ){"` |
|     - | 3376 | `"   throw new DateMalformedIntervalStringException("` |
|     - | 3377 | `"    'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 3378 | `"  }"` |
|     - | 3379 | `"  return $iv;"` |
|     - | 3380 | `" }"` |
|     - | 3381 | `" public function format($format){"` |
|     - | 3382 | `"  $f = (string)$format;"` |
|     - | 3383 | `"  $out = '';"` |
|     - | 3384 | `"  $n = strlen($f);"` |
|     - | 3385 | `"  for( $k = 0; $k < $n; $k++ ){"` |
|     - | 3386 | `"   $c = $f[$k];"` |
|     - | 3387 | `"   if( $c !== '%' ){ $out .= $c; continue; }"` |
|     - | 3388 | `"   $k++;"` |
|     - | 3389 | `"   if( $k >= $n ){ $out .= '%'; break; }"` |
|     - | 3390 | `"   $t = $f[$k];"` |
|     - | 3391 | `"   if( $t === 'Y' ){ $out .= sprintf('%02d', $this->y); }"` |
|     - | 3392 | `"   elseif( $t === 'y' ){ $out .= $this->y; }"` |
|     - | 3393 | `"   elseif( $t === 'M' ){ $out .= sprintf('%02d', $this->m); }"` |
|     - | 3394 | `"   elseif( $t === 'm' ){ $out .= $this->m; }"` |
|     - | 3395 | `"   elseif( $t === 'D' ){ $out .= sprintf('%02d', $this->d); }"` |
|     - | 3396 | `"   elseif( $t === 'd' ){ $out .= $this->d; }"` |
|     - | 3397 | `"   elseif( $t === 'H' ){ $out .= sprintf('%02d', $this->h); }"` |
|     - | 3398 | `"   elseif( $t === 'h' ){ $out .= $this->h; }"` |
|     - | 3399 | `"   elseif( $t === 'I' ){ $out .= sprintf('%02d', $this->i); }"` |
|     - | 3400 | `"   elseif( $t === 'i' ){ $out .= $this->i; }"` |
|     - | 3401 | `"   elseif( $t === 'S' ){ $out .= sprintf('%02d', $this->s); }"` |
|     - | 3402 | `"   elseif( $t === 's' ){ $out .= $this->s; }"` |
|     - | 3403 | `"   elseif( $t === 'F' ){ $out .= sprintf('%06d', (int)round($this->f * 1000000)); }"` |
|     - | 3404 | `"   elseif( $t === 'f' ){ $out .= (int)round($this->f * 1000000); }"` |
|     - | 3405 | `"   elseif( $t === 'R' ){ $out .= $this->invert ? '-' : '+'; }"` |
|     - | 3406 | `"   elseif( $t === 'r' ){ $out .= $this->invert ? '-' : ''; }"` |
|     - | 3407 | `"   elseif( $t === 'a' ){ $out .= $this->days === false ? '(unknown)' : $this->days; }"` |
|     - | 3408 | `"   elseif( $t === '%' ){ $out .= '%'; }"` |
|     - | 3409 | `"   else { $out .= $t; }"` |
|     - | 3410 | `"  }"` |
|     - | 3411 | `"  return $out;"` |
|     - | 3412 | `" }"` |
|     - | 3413 | `"}"` |
|     - | 3414 | `"class DatePeriod implements IteratorAggregate {"` |
|     - | 3415 | `" const EXCLUDE_START_DATE = 1;"` |
|     - | 3416 | `" const INCLUDE_END_DATE = 2;"` |
|     - | 3417 | `" public $start = null;"` |
|     - | 3418 | `" public $current = null;"` |
|     - | 3419 | `" public $end = null;"` |
|     - | 3420 | `" public $interval = null;"` |
|     - | 3421 | `" public $recurrences = 1;"` |
|     - | 3422 | `" public $include_start_date = true;"` |
|     - | 3423 | `" public $include_end_date = false;"` |
|     - | 3424 | `" private $__dpN = null;"` |
|     - | 3425 | `" public function __construct($start, $interval = null, $end = null, $options = 0){"` |
|     - | 3426 | `"  if( is_string($start) ){"` |
|     - | 3427 | `"   $mm = null;"` |
|     - | 3428 | `"   if( !preg_match('/^R(\\d+)\\/(.+)\\/(P.+)$/', $start, $mm) ){"` |
|     - | 3429 | `"    throw new DateMalformedPeriodStringException("` |
|     - | 3430 | `"     'DatePeriod::__construct(): Unknown or bad format (' . $start . ')');"` |
|     - | 3431 | `"   }"` |
|     - | 3432 | `"   $options = is_int($interval) ? $interval : 0;"` |
|     - | 3433 | `"   $this->start = new DateTimeImmutable($mm[2]);"` |
|     - | 3434 | `"   $this->interval = new DateInterval($mm[3]);"` |
|     - | 3435 | `"   $this->__dpN = (int)$mm[1];"` |
|     - | 3436 | `"   $this->recurrences = $this->__dpN + 1;"` |
|     - | 3437 | `"  }else{"` |
|     - | 3438 | `"   $this->start = clone $start;"` |
|     - | 3439 | `"   $this->interval = $interval;"` |
|     - | 3440 | `"   if( is_int($end) ){"` |
|     - | 3441 | `"    $this->__dpN = $end;"` |
|     - | 3442 | `"    $this->recurrences = $end + 1;"` |
|     - | 3443 | `"   }else{"` |
|     - | 3444 | `"    $this->end = $end === null ? null : (clone $end);"` |
|     - | 3445 | `"   }"` |
|     - | 3446 | `"  }"` |
|     - | 3447 | `"  $this->include_start_date = !((int)$options & 1);"` |
|     - | 3448 | `"  $this->include_end_date = ((int)$options & 2) !== 0;"` |
|     - | 3449 | `" }"` |
|     - | 3450 | `" public static function createFromISO8601String($specification, $options = 0){"` |
|     - | 3451 | `"  return new DatePeriod((string)$specification, (int)$options);"` |
|     - | 3452 | `" }"` |
|     - | 3453 | `" public function getStartDate(){ return $this->start; }"` |
|     - | 3454 | `" public function getEndDate(){ return $this->end; }"` |
|     - | 3455 | `" public function getDateInterval(){ return $this->interval; }"` |
|     - | 3456 | `" public function getRecurrences(){ return $this->__dpN; }"` |
|     - | 3457 | `" public function getIterator(): Generator {"` |
|     - | 3458 | `"  $cur = $this->start;"` |
|     - | 3459 | `"  $iv = $this->interval;"` |
|     - | 3460 | `"  $k = 0;"` |
|     - | 3461 | `"  if( $this->end !== null ){"` |
|     - | 3462 | `"   $endTs = $this->end->getTimestamp();"` |
|     - | 3463 | `"   $first = true;"` |
|     - | 3464 | `"   while( true ){"` |
|     - | 3465 | `"    $ts = $cur->getTimestamp();"` |
|     - | 3466 | `"    if( $this->include_end_date ? ($ts > $endTs) : ($ts >= $endTs) ){ break; }"` |
|     - | 3467 | `"    if( !$first \|\| $this->include_start_date ){"` |
|     - | 3468 | `"     yield $k => (clone $cur);"` |
|     - | 3469 | `"     $k++;"` |
|     - | 3470 | `"    }"` |
|     - | 3471 | `"    $first = false;"` |
|     - | 3472 | `"    $next = clone $cur;"` |
|     - | 3473 | `"    $cur = $next->add($iv);"` |
|     - | 3474 | `"   }"` |
|     - | 3475 | `"   return;"` |
|     - | 3476 | `"  }"` |
|     - | 3477 | `"  $total = $this->__dpN + 1 + ($this->include_end_date ? 1 : 0);"` |
|     - | 3478 | `"  for( $j = 0; $j < $total; $j++ ){"` |
|     - | 3479 | `"   if( $j > 0 \|\| $this->include_start_date ){"` |
|     - | 3480 | `"    yield $k => (clone $cur);"` |
|     - | 3481 | `"    $k++;"` |
|     - | 3482 | `"   }"` |
|     - | 3483 | `"   $next = clone $cur;"` |
|     - | 3484 | `"   $cur = $next->add($iv);"` |
|     - | 3485 | `"  }"` |
|     - | 3486 | `" }"` |
|     - | 3487 | `"}"` |
|     - | 3488 | `"function date_format($object, $format){ return $object->format($format); }"` |
|     - | 3489 | `"function date_modify($object, $modifier){"` |
|     - | 3490 | `" try { return $object->modify($modifier); } catch (Exception $e) { return false; }"` |
|     - | 3491 | `"}"` |
|     - | 3492 | `"function date_add($object, $interval){ return $object->add($interval); }"` |
|     - | 3493 | `"function date_sub($object, $interval){ return $object->sub($interval); }"` |
|     - | 3494 | `"function date_diff($baseObject, $targetObject, $absolute = false){"` |
|     - | 3495 | `" return $baseObject->diff($targetObject, $absolute);"` |
|     - | 3496 | `"}"` |
|     - | 3497 | `"function date_timestamp_get($object){ return $object->getTimestamp(); }"` |
|     - | 3498 | `"function date_timestamp_set($object, $timestamp){ return $object->setTimestamp($timestamp); }"` |
|     - | 3499 | `"function date_timezone_get($object){ return $object->getTimezone(); }"` |
|     - | 3500 | `"function date_timezone_set($object, $timezone){ return $object->setTimezone($timezone); }"` |
|     - | 3501 | `"function date_offset_get($object){ return $object->getOffset(); }"` |
|     - | 3502 | `"function date_date_set($object, $year, $month, $day){ return $object->setDate($year, $month, $day); }"` |
|     - | 3503 | `"function date_time_set($object, $hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3504 | `" return $object->setTime($hour, $minute, $second, $microsecond);"` |
|     - | 3505 | `"}"` |
|     - | 3506 | `"function date_isodate_set($object, $year, $week, $dayOfWeek = 1){"` |
|     - | 3507 | `" return $object->setISODate($year, $week, $dayOfWeek);"` |
|     - | 3508 | `"}"` |
|     - | 3509 | `"function date_interval_create_from_date_string($datetime){"` |
|     - | 3510 | `" return DateInterval::createFromDateString($datetime);"` |
|     - | 3511 | `"}"` |
|     - | 3512 | `"function date_interval_format($object, $format){ return $object->format($format); }"` |
|     - | 3513 | `"function date_get_last_errors(){ return DateTime::getLastErrors(); }"` |
|     - | 3514 | `"function timezone_open($timezone){"` |
|     - | 3515 | `" try { return new DateTimeZone($timezone); } catch (Exception $e) { return false; }"` |
|     - | 3516 | `"}"` |
|     - | 3517 | `"function timezone_name_get($object){ return $object->getName(); }"` |
|     - | 3518 | `"function timezone_offset_get($object, $datetime){ return $object->getOffset($datetime); }"` |
|     - | 3519 | `/* int\|false strtotime(string $datetime, ?int $baseTimestamp = null). Rides the` |
|     - | 3520 | ` * same DtParse the DateTime constructor uses, so its format coverage is identical.` |
|     - | 3521 | ` * php: the EMPTY string is false, but whitespace-only is 'now'; a parse failure is` |
|     - | 3522 | ` * false (never an exception). The default timezone is treated as offset 0, exactly` |
|     - | 3523 | ` * as the DateTime constructor does for a null $timezone. */` |
|     - | 3524 | `"function strtotime($datetime, $baseTimestamp = null){"` |
|     - | 3525 | `" $s = (string)$datetime;"` |
|     - | 3526 | `" if( $s === '' ){ return false; }"` |
|     - | 3527 | `" $base = $baseTimestamp === null ? __dt_now() : (int)$baseTimestamp;"` |
|     - | 3528 | `" $r = __dt_parse($s, $base, 0);"` |
|     - | 3529 | `" return is_string($r) ? false : $r[0];"` |
|     - | 3530 | `"}"` |
|     - | 3531 | `;` |
|     - | 3532 | `/*` |
|     - | 3533 | ` * Install the DateTime family: thunks first, then the chunk. Called from` |
|     - | 3534 | ` * PH7_VmInit inside the bCompilingBuiltin window, after the Reflection` |
|     - | 3535 | ` * install (Exception must exist).` |
|     - | 3536 | ` */` |
|  3812 | 3537 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
|     5 | 3538 | `{` |
|     - | 3539 | `	static const struct {` |
|     - | 3540 | `		const char *zName;` |
|     - | 3541 | `		ProchHostFunction xFunc;` |
|     - | 3542 | `	} aFunc[] = {` |
|     - | 3543 | `		{ "__dt_now",    vm_builtin_dt_now },` |
|     - | 3544 | `		{ "__dt_default_tz", vm_builtin_dt_default_tz },` |
|     - | 3545 | `		{ "__dt_civil_add",  vm_builtin_dt_civil_add },` |
|     - | 3546 | `		{ "__dt_civil_diff", vm_builtin_dt_civil_diff },` |
|     - | 3547 | `		{ "__dt_isodate",    vm_builtin_dt_isodate },` |
|     - | 3548 | `		{ "__dt_from_format", vm_builtin_dt_from_format },` |
|     - | 3549 | `		{ "__dt_parse",  vm_builtin_dt_parse },` |
|     - | 3550 | `		{ "__dt_format", vm_builtin_dt_format },` |
|     - | 3551 | `		{ "__dt_make",   vm_builtin_dt_make },` |
|     - | 3552 | `	};` |
|     - | 3553 | `	sxu32 n;` |
|     - | 3554 | `	/* php's date.timezone default */` |
|  3817 | 3555 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|  3817 | 3556 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
| 38125 | 3557 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 34313 | 3558 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 17159 | 3559 | `	}` |
|  3817 | 3560 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zDateTimeLib,sizeof(zDateTimeLib)-1);` |
|     5 | 3561 | `}` |
|     - | 3562 |  |
|     - | 3563 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 3564 |  |
|     - | 3565 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 3566 | `/* Tiny build: no DateTime family (builtin layer disabled) */` |
|     - | 3567 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){` |
|     - | 3568 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|     - | 3569 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|     - | 3570 | `	return SXRET_OK;` |
|     - | 3571 | `}` |
|     - | 3572 | `#endif` |
|     - | 3573 |  |
