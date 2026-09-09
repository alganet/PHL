# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1547/1952 lines (79.25%)

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
|   312 |  416 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec)` |
|     1 |  417 | `{` |
|   313 |  418 | `	const char *zEnd = &zIn[nLen];` |
|     - |  419 | `	const char *zCur;` |
|     - |  420 | `	/* Start the format process */` |
|  1158 |  421 | `	for(;;){` |
|  2317 |  422 | `		if( zIn >= zEnd ){` |
|     - |  423 | `			/* No more input to process */` |
|   313 |  424 | `			break;` |
|     - |  425 | `		}` |
|  2005 |  426 | `		switch(zIn[0]){` |
|   105 |  427 | `		case 'd':` |
|     - |  428 | `			/* Day of the month, 2 digits with leading zeros */` |
|   211 |  429 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|   211 |  430 | `			break;` |
|    31 |  431 | `		case 'D':` |
|     - |  432 | `			/*A textual representation of a day, three letters*/` |
|    63 |  433 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    63 |  434 | `			ph7_result_string(pCtx,zCur,3);` |
|    63 |  435 | `			break;` |
|     3 |  436 | `		case 'j':` |
|     - |  437 | `			/*	Day of the month without leading zeros */` |
|     7 |  438 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|     7 |  439 | `			break;` |
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
|   105 |  463 | `		case 'm':` |
|     - |  464 | `			/*Numeric representation of a month, with leading zeros*/` |
|   211 |  465 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|   211 |  466 | `			break;` |
|   ! 0 |  467 | `		case 'M':` |
|     - |  468 | `			/*A short textual representation of a month, three letters*/` |
|   ! 0 |  469 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|   ! 0 |  470 | `			ph7_result_string(pCtx,zCur,3);` |
|   ! 0 |  471 | `			break;` |
|     3 |  472 | `		case 'n':` |
|     - |  473 | `			/*Numeric representation of a month, without leading zeros*/` |
|     7 |  474 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|     7 |  475 | `			break;` |
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
|    94 |  510 | `		case 'Y':` |
|     - |  511 | `			/*	A full numeric representation of a year, 4 digits */` |
|   189 |  512 | `			ph7_result_string_format(pCtx,"%04d",pTm->tm_year);` |
|   189 |  513 | `			break;` |
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
|    65 |  566 | `		case 'H':` |
|     - |  567 | `			/*	24-hour format of an hour with leading zeros */` |
|   131 |  568 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|   131 |  569 | `			break;` |
|    66 |  570 | `		case 'i':` |
|     - |  571 | `			/* 	Minutes with leading zeros */` |
|   133 |  572 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|   133 |  573 | `			break;` |
|    68 |  574 | `		case 's':` |
|     - |  575 | `			/* 	second with leading zeros */` |
|   137 |  576 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|   137 |  577 | `			break;` |
|    11 |  578 | `		case 'u':` |
|     - |  579 | `			/* 	Microseconds. date()/gmdate() have no sub-second part (uSec == 0);` |
|     - |  580 | `			 * 	DateTime::format passes its stored microseconds. */` |
|    23 |  581 | `			ph7_result_string_format(pCtx,"%06d",uSec);` |
|    23 |  582 | `			break;` |
|     3 |  583 | `		case 'v':` |
|     - |  584 | `			/* 	Milliseconds */` |
|     7 |  585 | `			ph7_result_string_format(pCtx,"%03d",uSec/1000);` |
|     7 |  586 | `			break;` |
|   ! 0 |  587 | `		case 'S':{` |
|     - |  588 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|     - |  589 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|   ! 0 |  590 | `			int v = pTm->tm_mday;` |
|   ! 0 |  591 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|   ! 0 |  592 | `			break;` |
|     - |  593 | `				 }` |
|     7 |  594 | `		case 'e':` |
|     - |  595 | `			/* 	Timezone identifier */` |
|    15 |  596 | `			zCur = pTm->tm_zone;` |
|    15 |  597 | `			if( zCur == 0 ){` |
|     - |  598 | `				/* date()-family fills: the script default timezone */` |
|     5 |  599 | `				zCur = pCtx->pVm->zDefTz;` |
|     2 |  600 | `			}` |
|    15 |  601 | `			ph7_result_string(pCtx,zCur,-1);` |
|    15 |  602 | `			break;` |
|     2 |  603 | `		case 'T':{` |
|     - |  604 | `			/* Timezone abbreviation: "UTC" for offset 0, "GMT+0530" for a` |
|     - |  605 | `			 * fixed offset (php's shape). PHL has no tz database, so the` |
|     - |  606 | `			 * zone-name path only ever sees UTC/GMT, uppercased. */` |
|     - |  607 | `			const char *z;` |
|     5 |  608 | `			if( pTm->tm_gmtoff != 0 ){` |
|   ! 0 |  609 | `				long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   ! 0 |  610 | `				ph7_result_string_format(pCtx,"GMT%c%02d%02d",` |
|   ! 0 |  611 | `					pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|   ! 0 |  612 | `				break;` |
|     - |  613 | `			}` |
|     5 |  614 | `			z = pTm->tm_zone ? pTm->tm_zone : pCtx->pVm->zDefTz;` |
|    17 |  615 | `			while( *z ){` |
|    13 |  616 | `				int c = (unsigned char)*z;` |
|    13 |  617 | `				if( c >= 'a' && c <= 'z' ){` |
|   ! 0 |  618 | `					c -= 'a' - 'A';` |
|   ! 0 |  619 | `				}` |
|    13 |  620 | `				ph7_result_string_format(pCtx,"%c",c);` |
|    13 |  621 | `				z++;` |
|     1 |  622 | `			}` |
|     5 |  623 | `			break;` |
|     - |  624 | `				 }` |
|   ! 0 |  625 | `		case 'I':` |
|     - |  626 | `			/* Whether or not the date is in daylight saving time */` |
|     - |  627 | `#ifdef __WINNT__` |
|     - |  628 | `#ifdef _MSC_VER` |
|     - |  629 | `#ifndef _WIN32_WCE` |
|   ! 0 |  630 | `			_get_daylight(&pTm->tm_isdst);` |
|     - |  631 | `#endif` |
|     - |  632 | `#endif` |
|     - |  633 | `#endif` |
|   ! 0 |  634 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|   ! 0 |  635 | `			break;` |
|   ! 0 |  636 | `		case 'r':{` |
|     - |  637 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200 */` |
|   ! 0 |  638 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   ! 0 |  639 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d %c%02d%02d",` |
|   ! 0 |  640 | `				SyTimeGetDay(pTm->tm_wday),` |
|   ! 0 |  641 | `				pTm->tm_mday,` |
|   ! 0 |  642 | `				SyTimeGetMonth(pTm->tm_mon),` |
|   ! 0 |  643 | `				pTm->tm_year,` |
|   ! 0 |  644 | `				pTm->tm_hour,` |
|   ! 0 |  645 | `				pTm->tm_min,` |
|   ! 0 |  646 | `				pTm->tm_sec,` |
|   ! 0 |  647 | `				pTm->tm_gmtoff < 0 ? '-' : '+',` |
|   ! 0 |  648 | `				(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  649 | `				);` |
|   ! 0 |  650 | `			break;` |
|     - |  651 | `				 }` |
|     2 |  652 | `		case 'U':` |
|     - |  653 | `			/* Seconds since the Unix Epoch FOR THIS Sytm (php: the timestamp` |
|     - |  654 | `			 * being formatted — pre-fix this printed time(0) regardless of the` |
|     - |  655 | `			 * date under format). */` |
|     7 |  656 | `			ph7_result_string_format(pCtx,"%qd",` |
|     4 |  657 | `				DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400` |
|     4 |  658 | `				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec` |
|     4 |  659 | `				- (sxi64)pTm->tm_gmtoff);` |
|     5 |  660 | `			break;` |
|     1 |  661 | `		case 'O':{` |
|     - |  662 | `			/* Difference to GMT without colon: +0530 (php) */` |
|     3 |  663 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     3 |  664 | `			ph7_result_string_format(pCtx,"%c%02d%02d",` |
|     2 |  665 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|     3 |  666 | `			break;` |
|     - |  667 | `				 }` |
|     2 |  668 | `		case 'P':{` |
|     - |  669 | `			/* Difference to GMT with colon: +05:30 (php) */` |
|     5 |  670 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     5 |  671 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|     4 |  672 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|     5 |  673 | `			break;` |
|     - |  674 | `				 }` |
|   ! 0 |  675 | `		case 'p':{` |
|     - |  676 | `			/* Like P, but "Z" for UTC (php 8.0+) */` |
|     - |  677 | `			long a;` |
|   ! 0 |  678 | `			if( pTm->tm_gmtoff == 0 ){` |
|   ! 0 |  679 | `				ph7_result_string(pCtx,"Z",1);` |
|   ! 0 |  680 | `				break;` |
|     - |  681 | `			}` |
|   ! 0 |  682 | `			a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   ! 0 |  683 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|   ! 0 |  684 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|   ! 0 |  685 | `			break;` |
|     - |  686 | `				 }` |
|   ! 0 |  687 | `		case 'Z':` |
|     - |  688 | `			/* Timezone offset in seconds, plain integer (php) */` |
|   ! 0 |  689 | `			ph7_result_string_format(pCtx,"%d",(int)pTm->tm_gmtoff);` |
|   ! 0 |  690 | `			break;` |
|     4 |  691 | `		case 'c':{` |
|     - |  692 | `			/* 	ISO 8601 date: 2004-02-12T15:19:21+00:00 (php) */` |
|     9 |  693 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    13 |  694 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",` |
|     4 |  695 | `				pTm->tm_year,` |
|     8 |  696 | `				pTm->tm_mon+1,` |
|     4 |  697 | `				pTm->tm_mday,` |
|     4 |  698 | `				pTm->tm_hour,` |
|     4 |  699 | `				pTm->tm_min,` |
|     4 |  700 | `				pTm->tm_sec,` |
|     8 |  701 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  702 | `				);` |
|     9 |  703 | `			break;` |
|     - |  704 | `				 }` |
|     1 |  705 | `		case '\\':` |
|     3 |  706 | `			zIn++;` |
|     - |  707 | `			/* Expand verbatim */` |
|     3 |  708 | `			if( zIn < zEnd ){` |
|     3 |  709 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     1 |  710 | `			}` |
|     3 |  711 | `			break;` |
|   424 |  712 | `		default:` |
|     - |  713 | `			/* Unknown format specifer,expand verbatim */` |
|   849 |  714 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|   848 |  715 | `			break;` |
|     - |  716 | `		}` |
|     - |  717 | `		/* Point to the next character */` |
|  2005 |  718 | `		zIn++;` |
|     1 |  719 | `	}` |
|   313 |  720 | `	return SXRET_OK;` |
|     1 |  721 | `}` |
|     - |  722 | `/*` |
|     - |  723 | ` * PH7 implementation of the strftime() function.` |
|     - |  724 | ` * The following formats are supported:` |
|     - |  725 | ` * %a 	An abbreviated textual representation of the day` |
|     - |  726 | ` * %A 	A full textual representation of the day` |
|     - |  727 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|     - |  728 | ` * %e 	Day of the month, with a space preceding single digits.` |
|     - |  729 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|     - |  730 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|     - |  731 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|     - |  732 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|     - |  733 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|     - |  734 | ` *   4 weekdays, with Monday being the start of the week.` |
|     - |  735 | ` * %W 	A numeric representation of the week of the year` |
|     - |  736 | ` * %b 	Abbreviated month name, based on the locale` |
|     - |  737 | ` * %B 	Full month name, based on the locale` |
|     - |  738 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|     - |  739 | ` * %m 	Two digit representation of the month` |
|     - |  740 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|     - |  741 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|     - |  742 | ` * %G 	The full four-digit version of %g` |
|     - |  743 | ` * %y 	Two digit representation of the year` |
|     - |  744 | ` * %Y 	Four digit representation for the year` |
|     - |  745 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|     - |  746 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|     - |  747 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|     - |  748 | ` * %M 	Two digit representation of the minute` |
|     - |  749 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|     - |  750 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|     - |  751 | ` * %r 	Same as "%I:%M:%S %p"` |
|     - |  752 | ` * %R 	Same as "%H:%M"` |
|     - |  753 | ` * %S 	Two digit representation of the second` |
|     - |  754 | ` * %T 	Same as "%H:%M:%S"` |
|     - |  755 | ` * %X 	Preferred time representation based on locale, without the date` |
|     - |  756 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|     - |  757 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|     - |  758 | ` * %c 	Preferred date and time stamp based on local` |
|     - |  759 | ` * %D 	Same as "%m/%d/%y"` |
|     - |  760 | ` * %F 	Same as "%Y-%m-%d"` |
|     - |  761 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|     - |  762 | ` * %x 	Preferred date representation based on locale, without the time` |
|     - |  763 | ` * %n 	A newline character ("\n")` |
|     - |  764 | ` * %t 	A Tab character ("\t")` |
|     - |  765 | ` * %% 	A literal percentage character ("%")` |
|     - |  766 | ` */` |
|    18 |  767 | `static int PH7_Strftime(` |
|     - |  768 | `	ph7_context *pCtx,  /* Call context */` |
|     - |  769 | `	const char *zIn,    /* Input string */` |
|     - |  770 | `	int nLen,           /* Input length */` |
|     - |  771 | `	Sytm *pTm           /* Parse of the given time */` |
|     - |  772 | `	)` |
|     1 |  773 | `{` |
|    19 |  774 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|     - |  775 | `	int c;` |
|     - |  776 | `	/* Start the format process */` |
|    20 |  777 | `	for(;;){` |
|    41 |  778 | `		zCur = zIn;` |
|    45 |  779 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|     5 |  780 | `			zIn++;` |
|     1 |  781 | `		}` |
|    41 |  782 | `		if( zIn > zCur ){` |
|     - |  783 | `			/* Consume input verbatim */` |
|     5 |  784 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     2 |  785 | `		}` |
|    41 |  786 | `		zIn++; /* Jump the percent sign */` |
|    41 |  787 | `		if( zIn >= zEnd ){` |
|     - |  788 | `			/* No more input to process */` |
|    19 |  789 | `			break;` |
|     - |  790 | `		}` |
|    23 |  791 | `		c = zIn[0];` |
|     - |  792 | `		/* Act according to the current specifer */` |
|    23 |  793 | `		switch(c){` |
|   ! 0 |  794 | `		case '%':` |
|     - |  795 | `			/* A literal percentage character ("%") */` |
|   ! 0 |  796 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|   ! 0 |  797 | `			break;` |
|   ! 0 |  798 | `		case 't':` |
|     - |  799 | `			/* A Tab character */` |
|   ! 0 |  800 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|   ! 0 |  801 | `			break;` |
|   ! 0 |  802 | `		case 'n':` |
|     - |  803 | `			/* A newline character */` |
|   ! 0 |  804 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|   ! 0 |  805 | `			break;` |
|     1 |  806 | `		case 'a':` |
|     - |  807 | `			/* An abbreviated textual representation of the day */` |
|     3 |  808 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|     3 |  809 | `			break;` |
|   ! 0 |  810 | `		case 'A':` |
|     - |  811 | `			/* A full textual representation of the day */` |
|   ! 0 |  812 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|   ! 0 |  813 | `			break;` |
|   ! 0 |  814 | `		case 'e':` |
|     - |  815 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|   ! 0 |  816 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|   ! 0 |  817 | `			break;` |
|     2 |  818 | `		case 'd':` |
|     - |  819 | `			/* Two-digit day of the month (with leading zeros) */` |
|     5 |  820 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|     5 |  821 | `			break;` |
|   ! 0 |  822 | `		case 'j':` |
|     - |  823 | `			/*The day of the year,3 digits with leading zeros*/` |
|   ! 0 |  824 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|   ! 0 |  825 | `			break;` |
|   ! 0 |  826 | `		case 'u':` |
|     - |  827 | `			/* ISO-8601 numeric representation of the day of the week */` |
|   ! 0 |  828 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|   ! 0 |  829 | `			break;` |
|   ! 0 |  830 | `		case 'w':` |
|     - |  831 | `			/* Numeric representation of the day of the week */` |
|   ! 0 |  832 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|   ! 0 |  833 | `			break;` |
|   ! 0 |  834 | `		case 'b':` |
|     - |  835 | `		case 'h':` |
|     - |  836 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|   ! 0 |  837 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|   ! 0 |  838 | `			break;` |
|   ! 0 |  839 | `		case 'B':` |
|     - |  840 | `			/* Full month name (Not based on locale) */` |
|   ! 0 |  841 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|   ! 0 |  842 | `			break;` |
|     2 |  843 | `		case 'm':` |
|     - |  844 | `			/*Numeric representation of a month, with leading zeros*/` |
|     5 |  845 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     5 |  846 | `			break;` |
|   ! 0 |  847 | `		case 'C':` |
|     - |  848 | `			/* Two digit representation of the century */` |
|   ! 0 |  849 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|   ! 0 |  850 | `			break;` |
|   ! 0 |  851 | `		case 'y':` |
|     - |  852 | `		case 'g':` |
|     - |  853 | `			/* Two digit representation of the year */` |
|   ! 0 |  854 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|   ! 0 |  855 | `			break;` |
|     3 |  856 | `		case 'Y':` |
|     - |  857 | `		case 'G':` |
|     - |  858 | `			/* Four digit representation of the year */` |
|     7 |  859 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     7 |  860 | `			break;` |
|   ! 0 |  861 | `		case 'I':` |
|     - |  862 | `			/* 12-hour format of an hour with leading zeros */` |
|   ! 0 |  863 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|   ! 0 |  864 | `			break;` |
|   ! 0 |  865 | `		case 'l':` |
|     - |  866 | `			/* 12-hour format of an hour with leading space */` |
|   ! 0 |  867 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|   ! 0 |  868 | `			break;` |
|     1 |  869 | `		case 'H':` |
|     - |  870 | `			/* 24-hour format of an hour with leading zeros */` |
|     3 |  871 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|     3 |  872 | `			break;` |
|     1 |  873 | `		case 'M':` |
|     - |  874 | `			/* Minutes with leading zeros */` |
|     3 |  875 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|     3 |  876 | `			break;` |
|   ! 0 |  877 | `		case 'S':` |
|     - |  878 | `			/* Seconds with leading zeros */` |
|   ! 0 |  879 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|   ! 0 |  880 | `			break;` |
|   ! 0 |  881 | `		case 'z':` |
|     - |  882 | `		case 'Z':` |
|     - |  883 | `			/* 	Timezone identifier */` |
|   ! 0 |  884 | `			zCur = pTm->tm_zone;` |
|   ! 0 |  885 | `			if( zCur == 0 ){` |
|     - |  886 | `				/* date()-family fills: the script default timezone */` |
|   ! 0 |  887 | `				zCur = pCtx->pVm->zDefTz;` |
|   ! 0 |  888 | `			}` |
|   ! 0 |  889 | `			ph7_result_string(pCtx,zCur,-1);` |
|   ! 0 |  890 | `			break;` |
|   ! 0 |  891 | `		case 'T':` |
|     - |  892 | `		case 'X':` |
|     - |  893 | `			/* Same as "%H:%M:%S" */` |
|   ! 0 |  894 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|   ! 0 |  895 | `			break;` |
|   ! 0 |  896 | `		case 'R':` |
|     - |  897 | `			/* Same as "%H:%M" */` |
|   ! 0 |  898 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|   ! 0 |  899 | `			break;` |
|   ! 0 |  900 | `		case 'P':` |
|     - |  901 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|   ! 0 |  902 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|   ! 0 |  903 | `			break;` |
|   ! 0 |  904 | `		case 'p':` |
|     - |  905 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|   ! 0 |  906 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|   ! 0 |  907 | `			break;` |
|   ! 0 |  908 | `		case 'r':` |
|     - |  909 | `			/* Same as "%I:%M:%S %p" */` |
|   ! 0 |  910 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|   ! 0 |  911 | `				1+(pTm->tm_hour%12),` |
|   ! 0 |  912 | `				pTm->tm_min,` |
|   ! 0 |  913 | `				pTm->tm_sec,` |
|   ! 0 |  914 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|     - |  915 | `				);` |
|   ! 0 |  916 | `			break;` |
|     1 |  917 | `		case 'D':` |
|     - |  918 | `		case 'x':` |
|     - |  919 | `			/* Same as "%m/%d/%y" */` |
|     4 |  920 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|     2 |  921 | `				pTm->tm_mon+1,` |
|     1 |  922 | `				pTm->tm_mday,` |
|     2 |  923 | `				pTm->tm_year%100` |
|     - |  924 | `				);` |
|     3 |  925 | `			break;` |
|   ! 0 |  926 | `		case 'F':` |
|     - |  927 | `			/* Same as "%Y-%m-%d" */` |
|   ! 0 |  928 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|   ! 0 |  929 | `				pTm->tm_year,` |
|   ! 0 |  930 | `				pTm->tm_mon+1,` |
|   ! 0 |  931 | `				pTm->tm_mday` |
|     - |  932 | `				);` |
|   ! 0 |  933 | `			break;` |
|   ! 0 |  934 | `		case 'c':` |
|   ! 0 |  935 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|   ! 0 |  936 | `				pTm->tm_year,` |
|   ! 0 |  937 | `				pTm->tm_mon+1,` |
|   ! 0 |  938 | `				pTm->tm_mday,` |
|   ! 0 |  939 | `				pTm->tm_hour,` |
|   ! 0 |  940 | `				pTm->tm_min,` |
|   ! 0 |  941 | `				pTm->tm_sec` |
|     - |  942 | `				);` |
|   ! 0 |  943 | `			break;` |
|   ! 0 |  944 | `		case 's':{` |
|     - |  945 | `			time_t tt;` |
|     - |  946 | `			/* Seconds since the Unix Epoch */` |
|   ! 0 |  947 | `			time(&tt);` |
|   ! 0 |  948 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|   ! 0 |  949 | `			break;` |
|     - |  950 | `				 }` |
|   ! 0 |  951 | `		default:` |
|     - |  952 | `			/* unknown specifer,simply ignore*/` |
|   ! 0 |  953 | `			break;` |
|     - |  954 | `		}` |
|     - |  955 | `		/* Advance the cursor */` |
|    23 |  956 | `		zIn++;` |
|     1 |  957 | `	}` |
|    19 |  958 | `	return SXRET_OK;` |
|     1 |  959 | `}` |
|     - |  960 | `/*` |
|     - |  961 | ` * Resolve a date()/gmdate() $timestamp argument under php 8's ?int weak ZPP:` |
|     - |  962 | ` *   - null            -> *pbUseNow = 1 (caller uses the current time)` |
|     - |  963 | ` *   - int/bool/float  -> coerce to a Unix timestamp (float truncates; php's` |
|     - |  964 | ` *                        float->int precision E_DEPRECATED is not emitted, §3.7)` |
|     - |  965 | ` *   - numeric string  -> coerce via php's is_numeric_string grammar` |
|     - |  966 | ` *                        (RangeStrToNumber: " 100 "/"1e3"/".5"/"+5" ok)` |
|     - |  967 | ` *   - anything else (non-numeric string, array, object, resource)` |
|     - |  968 | ` *                     -> catchable TypeError, byte-exact with php.` |
|     - |  969 | ` * Returns PH7_OK with *pbUseNow / *pT set, or the PH7_VmThrowException status.` |
|     - |  970 | ` */` |
|   104 |  971 | `static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)` |
|     1 |  972 | `{` |
|     - |  973 | `	char zBuf[64];` |
|   105 |  974 | `	*pbUseNow = 0;` |
|   105 |  975 | `	if( ph7_value_is_null(pArg) ){` |
|     3 |  976 | `		*pbUseNow = 1;` |
|     3 |  977 | `		return PH7_OK;` |
|     - |  978 | `	}` |
|   103 |  979 | `	if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_float(pArg) ){` |
|    85 |  980 | `		*pT = (time_t)ph7_value_to_int64(pArg);` |
|    85 |  981 | `		return PH7_OK;` |
|     - |  982 | `	}` |
|    19 |  983 | `	if( ph7_value_is_string(pArg) ){` |
|     - |  984 | `		int nStr;` |
|    19 |  985 | `		const char *zStr = ph7_value_to_string(pArg,&nStr);` |
|     - |  986 | `		sxi64 iLong; double dReal;` |
|    19 |  987 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|    19 |  988 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|     3 |  989 | `			*pT = (time_t)dReal;` |
|     6 |  990 | `			return PH7_OK;` |
|     - |  991 | `		}` |
|    17 |  992 | `		if( iKind == RANGE_IN_LONG ){` |
|     7 |  993 | `			*pT = (time_t)iLong;` |
|     7 |  994 | `			return PH7_OK;` |
|     - |  995 | `		}` |
|     - |  996 | `		/* Not a numeric string: fall through to the TypeError. */` |
|     5 |  997 | `	}` |
|    16 |  998 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  999 | `		"%s(): Argument #2 ($timestamp) must be of type ?int, %s given",` |
|     5 | 1000 | `		ph7_function_name(pCtx),VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|    53 | 1001 | `}` |
|     - | 1002 | `/*` |
|     - | 1003 | ` * string date(string $format [, int $timestamp = time() ] )` |
|     - | 1004 | ` *  Returns a string formatted according to the given format string using` |
|     - | 1005 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|     - | 1006 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|     - | 1007 | ` * Parameters` |
|     - | 1008 | ` *  $format` |
|     - | 1009 | ` *   The format of the outputted date string (See code above)` |
|     - | 1010 | ` * $timestamp` |
|     - | 1011 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1012 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1013 | ` *   In other words, it defaults to the value of time().` |
|     - | 1014 | ` * Return` |
|     - | 1015 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - | 1016 | ` */` |
|    46 | 1017 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1018 | `{` |
|     - | 1019 | `	const char *zFormat;` |
|     - | 1020 | `	int nLen;` |
|     - | 1021 | `	Sytm sTm;` |
|    47 | 1022 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1023 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1024 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1025 | `		return PH7_OK;` |
|     - | 1026 | `	}` |
|    47 | 1027 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    47 | 1028 | `	if( nLen < 1 ){` |
|     - | 1029 | `		/* Don't bother processing return the empty string */` |
|   ! 0 | 1030 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1031 | `	}` |
|    47 | 1032 | `	if( nArg < 2 ){` |
|     - | 1033 | `#ifdef __WINNT__` |
|     - | 1034 | `		SYSTEMTIME sOS;` |
|     1 | 1035 | `		GetSystemTime(&sOS);` |
|     1 | 1036 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1037 | `#else` |
|     - | 1038 | `		struct tm *pTm;` |
|     - | 1039 | `		time_t t;` |
|    30 | 1040 | `		time(&t);` |
|    30 | 1041 | `		pTm = gmtime(&t);` |
|    30 | 1042 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    30 | 1043 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1044 | `#endif` |
|    16 | 1045 | `	}else{` |
|     - | 1046 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|    17 | 1047 | `		time_t t = 0;` |
|     - | 1048 | `		struct tm *pTm;` |
|     - | 1049 | `		int bUseNow;` |
|    17 | 1050 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|    17 | 1051 | `		if( rc != PH7_OK ){` |
|     9 | 1052 | `			return rc;` |
|     - | 1053 | `		}` |
|     9 | 1054 | `		if( bUseNow ){` |
|   ! 0 | 1055 | `			time(&t);` |
|   ! 0 | 1056 | `		}` |
|     9 | 1057 | `		pTm = gmtime(&t);` |
|     9 | 1058 | `		if( pTm == 0 ){` |
|   ! 0 | 1059 | `			time(&t);` |
|   ! 0 | 1060 | `			pTm = gmtime(&t);` |
|   ! 0 | 1061 | `		}` |
|     9 | 1062 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     9 | 1063 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1064 | `	}` |
|     - | 1065 | `	/* Format the given string */` |
|    39 | 1066 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|    39 | 1067 | `	return PH7_OK;` |
|    24 | 1068 | `}` |
|     - | 1069 | `/*` |
|     - | 1070 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|     - | 1071 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|     - | 1072 | ` * Parameters` |
|     - | 1073 | ` *  $format` |
|     - | 1074 | ` *   The format of the outputted date string (See code above)` |
|     - | 1075 | ` * $timestamp` |
|     - | 1076 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1077 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1078 | ` *   In other words, it defaults to the value of time().` |
|     - | 1079 | ` * Return` |
|     - | 1080 | ` * Returns a string formatted according format using the given timestamp` |
|     - | 1081 | ` * or the current local time if no timestamp is given.` |
|     - | 1082 | ` */` |
|    18 | 1083 | `PH7_PRIVATE int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1084 | `{` |
|     - | 1085 | `	const char *zFormat;` |
|     - | 1086 | `	int nLen;` |
|     - | 1087 | `	Sytm sTm;` |
|     - | 1088 | `	/* The php 8.1 whole-function deprecation is declared in aBuiltinDeprecated[] and` |
|     - | 1089 | `	 * emitted at the OP_CALL choke point, which is what puts it BEFORE the` |
|     - | 1090 | `	 * ArgumentCountError for a no-arg call — php's order. */` |
|    19 | 1091 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1092 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1093 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1094 | `		return PH7_OK;` |
|     - | 1095 | `	}` |
|    19 | 1096 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 1097 | `	if( nLen < 1 ){` |
|     - | 1098 | `		/* Don't bother processing return FALSE */` |
|   ! 0 | 1099 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1100 | `	}` |
|    19 | 1101 | `	if( nArg < 2 ){` |
|     - | 1102 | `#ifdef __WINNT__` |
|     - | 1103 | `		SYSTEMTIME sOS;` |
|     1 | 1104 | `		GetSystemTime(&sOS);` |
|     1 | 1105 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1106 | `#else` |
|     - | 1107 | `		struct tm *pTm;` |
|     - | 1108 | `		time_t t;` |
|    16 | 1109 | `		time(&t);` |
|    16 | 1110 | `		pTm = gmtime(&t);` |
|    16 | 1111 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    16 | 1112 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1113 | `#endif` |
|     9 | 1114 | `	}else{` |
|     - | 1115 | `		/* Use the given timestamp */` |
|     - | 1116 | `		time_t t;` |
|     - | 1117 | `		struct tm *pTm;` |
|     3 | 1118 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     3 | 1119 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     3 | 1120 | `			pTm = gmtime(&t);` |
|     3 | 1121 | `			if( pTm == 0 ){` |
|   ! 0 | 1122 | `				time(&t);` |
|   ! 0 | 1123 | `			}` |
|     2 | 1124 | `		}else{` |
|   ! 0 | 1125 | `			time(&t);` |
|     - | 1126 | `		}` |
|     3 | 1127 | `		pTm = gmtime(&t);` |
|     3 | 1128 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     3 | 1129 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1130 | `	}` |
|     - | 1131 | `	/* Format the given string */` |
|    19 | 1132 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|    19 | 1133 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     - | 1134 | `		/* Nothing was formatted,return FALSE */` |
|   ! 0 | 1135 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1136 | `	}` |
|    19 | 1137 | `	return PH7_OK;` |
|    10 | 1138 | `}` |
|     - | 1139 | `/*` |
|     - | 1140 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|     - | 1141 | ` *  Identical to the date() function except that the time returned` |
|     - | 1142 | ` *  is Greenwich Mean Time (GMT).` |
|     - | 1143 | ` * Parameters` |
|     - | 1144 | ` *  $format` |
|     - | 1145 | ` *  The format of the outputted date string (See code above)` |
|     - | 1146 | ` *  $timestamp` |
|     - | 1147 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1148 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1149 | ` *   In other words, it defaults to the value of time().` |
|     - | 1150 | ` * Return` |
|     - | 1151 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - | 1152 | ` */` |
|   102 | 1153 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1154 | `{` |
|     - | 1155 | `	const char *zFormat;` |
|     - | 1156 | `	int nLen;` |
|     - | 1157 | `	Sytm sTm;` |
|   103 | 1158 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1159 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1160 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1161 | `		return PH7_OK;` |
|     - | 1162 | `	}` |
|   103 | 1163 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   103 | 1164 | `	if( nLen < 1 ){` |
|     - | 1165 | `		/* Don't bother processing return the empty string */` |
|   ! 0 | 1166 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1167 | `	}` |
|   103 | 1168 | `	if( nArg < 2 ){` |
|     - | 1169 | `#ifdef __WINNT__` |
|     - | 1170 | `		SYSTEMTIME sOS;` |
|     1 | 1171 | `		GetSystemTime(&sOS);` |
|     1 | 1172 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1173 | `#else` |
|     - | 1174 | `		struct tm *pTm;` |
|     - | 1175 | `		time_t t;` |
|    14 | 1176 | `		time(&t);` |
|    14 | 1177 | `		pTm = gmtime(&t);` |
|    14 | 1178 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    14 | 1179 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1180 | `#endif` |
|     8 | 1181 | `	}else{` |
|     - | 1182 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|    89 | 1183 | `		time_t t = 0;` |
|     - | 1184 | `		struct tm *pTm;` |
|     - | 1185 | `		int bUseNow;` |
|    89 | 1186 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|    89 | 1187 | `		if( rc != PH7_OK ){` |
|     3 | 1188 | `			return rc;` |
|     - | 1189 | `		}` |
|    87 | 1190 | `		if( bUseNow ){` |
|     3 | 1191 | `			time(&t);` |
|     1 | 1192 | `		}` |
|    87 | 1193 | `		pTm = gmtime(&t);` |
|    87 | 1194 | `		if( pTm == 0 ){` |
|   ! 0 | 1195 | `			time(&t);` |
|   ! 0 | 1196 | `			pTm = gmtime(&t);` |
|   ! 0 | 1197 | `		}` |
|    87 | 1198 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    87 | 1199 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1200 | `	}` |
|     - | 1201 | `	/* Format the given string */` |
|   101 | 1202 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|   101 | 1203 | `	return PH7_OK;` |
|    52 | 1204 | `}` |
|     - | 1205 | `/*` |
|     - | 1206 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|     - | 1207 | ` *  Return the local time.` |
|     - | 1208 | ` * Parameter` |
|     - | 1209 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1210 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - | 1211 | ` *     In other words, it defaults to the value of time().` |
|     - | 1212 | ` * $is_associative` |
|     - | 1213 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|     - | 1214 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|     - | 1215 | ` *   array containing all the different elements of the structure returned by the C function` |
|     - | 1216 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|     - | 1217 | ` *      "tm_sec" - seconds, 0 to 59` |
|     - | 1218 | ` *      "tm_min" - minutes, 0 to 59` |
|     - | 1219 | ` *      "tm_hour" - hours, 0 to 23` |
|     - | 1220 | ` *      "tm_mday" - day of the month, 1 to 31` |
|     - | 1221 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|     - | 1222 | ` *      "tm_year" - years since 1900` |
|     - | 1223 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|     - | 1224 | ` *      "tm_yday" - day of the year, 0 to 365` |
|     - | 1225 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|     - | 1226 | ` * Returns` |
|     - | 1227 | ` *  An associative array of information related to the timestamp.` |
|     - | 1228 | ` */` |
|     8 | 1229 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1230 | `{` |
|     - | 1231 | `	ph7_value *pValue,*pArray;` |
|     9 | 1232 | `	int isAssoc = 0;` |
|     - | 1233 | `	Sytm sTm;` |
|     9 | 1234 | `	if( nArg < 1 ){` |
|     - | 1235 | `#ifdef __WINNT__` |
|     - | 1236 | `		SYSTEMTIME sOS;` |
|     1 | 1237 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|     1 | 1238 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1239 | `#else` |
|     - | 1240 | `		struct tm *pTm;` |
|     - | 1241 | `		time_t t;` |
|     4 | 1242 | `		time(&t);` |
|     4 | 1243 | `		pTm = gmtime(&t);` |
|     4 | 1244 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     4 | 1245 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1246 | `#endif` |
|     3 | 1247 | `	}else{` |
|     - | 1248 | `		/* Use the given timestamp */` |
|     - | 1249 | `		time_t t;` |
|     - | 1250 | `		struct tm *pTm;` |
|     5 | 1251 | `		if( ph7_value_is_int(apArg[0]) ){` |
|     5 | 1252 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|     5 | 1253 | `			pTm = gmtime(&t);` |
|     5 | 1254 | `			if( pTm == 0 ){` |
|   ! 0 | 1255 | `				time(&t);` |
|   ! 0 | 1256 | `			}` |
|     3 | 1257 | `		}else{` |
|   ! 0 | 1258 | `			time(&t);` |
|     - | 1259 | `		}` |
|     5 | 1260 | `		pTm = gmtime(&t);` |
|     5 | 1261 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     5 | 1262 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1263 | `	}` |
|     - | 1264 | `	/* Element value */` |
|     9 | 1265 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     9 | 1266 | `	if( pValue == 0 ){` |
|     - | 1267 | `		/* Return NULL */` |
|   ! 0 | 1268 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1269 | `		return PH7_OK;` |
|     - | 1270 | `	}` |
|     - | 1271 | `	/* Create a new array */` |
|     9 | 1272 | `	pArray = ph7_context_new_array(pCtx);` |
|     9 | 1273 | `	if( pArray == 0 ){` |
|     - | 1274 | `		/* Return NULL */` |
|   ! 0 | 1275 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1276 | `		return PH7_OK;` |
|     - | 1277 | `	}` |
|     9 | 1278 | `	if( nArg > 1 ){` |
|     3 | 1279 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|     1 | 1280 | `	}` |
|     - | 1281 | `	/* Fill the array */` |
|     - | 1282 | `	/* Seconds */` |
|     9 | 1283 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|     9 | 1284 | `	if( isAssoc ){` |
|     3 | 1285 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|     2 | 1286 | `	}else{` |
|     7 | 1287 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1288 | `	}` |
|     - | 1289 | `	/* Minutes */` |
|     9 | 1290 | `	ph7_value_int(pValue,sTm.tm_min);` |
|     9 | 1291 | `	if( isAssoc ){` |
|     3 | 1292 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|     2 | 1293 | `	}else{` |
|     7 | 1294 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1295 | `	}` |
|     - | 1296 | `	/* Hours */` |
|     9 | 1297 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|     9 | 1298 | `	if( isAssoc ){` |
|     3 | 1299 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|     2 | 1300 | `	}else{` |
|     7 | 1301 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1302 | `	}` |
|     - | 1303 | `	/* mday */` |
|     9 | 1304 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|     9 | 1305 | `	if( isAssoc ){` |
|     3 | 1306 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|     2 | 1307 | `	}else{` |
|     7 | 1308 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1309 | `	}` |
|     - | 1310 | `	/* mon */` |
|     9 | 1311 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|     9 | 1312 | `	if( isAssoc ){` |
|     3 | 1313 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|     2 | 1314 | `	}else{` |
|     7 | 1315 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1316 | `	}` |
|     - | 1317 | `	/* year since 1900 */` |
|     9 | 1318 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|     9 | 1319 | `	if( isAssoc ){` |
|     3 | 1320 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|     2 | 1321 | `	}else{` |
|     7 | 1322 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1323 | `	}` |
|     - | 1324 | `	/* wday */` |
|     9 | 1325 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|     9 | 1326 | `	if( isAssoc ){` |
|     3 | 1327 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|     2 | 1328 | `	}else{` |
|     7 | 1329 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1330 | `	}` |
|     - | 1331 | `	/* yday */` |
|     9 | 1332 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|     9 | 1333 | `	if( isAssoc ){` |
|     3 | 1334 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|     2 | 1335 | `	}else{` |
|     7 | 1336 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1337 | `	}` |
|     - | 1338 | `	/* isdst */` |
|     - | 1339 | `#ifdef __WINNT__` |
|     - | 1340 | `#ifdef _MSC_VER` |
|     - | 1341 | `#ifndef _WIN32_WCE` |
|     1 | 1342 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1343 | `#endif` |
|     - | 1344 | `#endif` |
|     - | 1345 | `#endif` |
|     9 | 1346 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|     9 | 1347 | `	if( isAssoc ){` |
|     3 | 1348 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|     2 | 1349 | `	}else{` |
|     7 | 1350 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1351 | `	}` |
|     - | 1352 | `	/* Return the array */` |
|     9 | 1353 | `	ph7_result_value(pCtx,pArray);` |
|     9 | 1354 | `	return PH7_OK;` |
|     5 | 1355 | `}` |
|     - | 1356 | `/*` |
|     - | 1357 | ` * int idate(string $format [, int $timestamp = time() ])` |
|     - | 1358 | ` *  Returns a number formatted according to the given format string` |
|     - | 1359 | ` *  using the given integer timestamp or the current local time if` |
|     - | 1360 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|     - | 1361 | ` *  to the value of time().` |
|     - | 1362 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|     - | 1363 | ` *  parameter.` |
|     - | 1364 | ` * $Parameters` |
|     - | 1365 | ` *  Supported format` |
|     - | 1366 | ` *   d 	Day of the month` |
|     - | 1367 | ` *   h 	Hour (12 hour format)` |
|     - | 1368 | ` *   H 	Hour (24 hour format)` |
|     - | 1369 | ` *   i 	Minutes` |
|     - | 1370 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|     - | 1371 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|     - | 1372 | ` *   m 	Month number` |
|     - | 1373 | ` *   s 	Seconds` |
|     - | 1374 | ` *   t 	Days in current month` |
|     - | 1375 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|     - | 1376 | ` *   w 	Day of the week (0 on Sunday)` |
|     - | 1377 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|     - | 1378 | ` *   y 	Year (1 or 2 digits - check note below)` |
|     - | 1379 | ` *   Y 	Year (4 digits)` |
|     - | 1380 | ` *   z 	Day of the year` |
|     - | 1381 | ` *   Z 	Timezone offset in seconds` |
|     - | 1382 | ` * $timestamp` |
|     - | 1383 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|     - | 1384 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|     - | 1385 | ` *  to the value of time().` |
|     - | 1386 | ` * Return` |
|     - | 1387 | ` *  An integer.` |
|     - | 1388 | ` */` |
|    38 | 1389 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1390 | `{` |
|     - | 1391 | `	const char *zFormat;` |
|    40 | 1392 | `	ph7_int64 iVal = 0;` |
|     - | 1393 | `	int nLen;` |
|     - | 1394 | `	Sytm sTm;` |
|    40 | 1395 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1396 | `		/* Missing/Invalid argument,return -1 */` |
|   ! 0 | 1397 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1398 | `		return PH7_OK;` |
|     - | 1399 | `	}` |
|    40 | 1400 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    40 | 1401 | `	if( nLen < 1 ){` |
|     - | 1402 | `		/* Don't bother processing return -1*/` |
|   ! 0 | 1403 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1404 | `	}` |
|    40 | 1405 | `	if( nArg < 2 ){` |
|     - | 1406 | `#ifdef __WINNT__` |
|     - | 1407 | `		SYSTEMTIME sOS;` |
|     2 | 1408 | `		GetSystemTime(&sOS);` |
|     2 | 1409 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1410 | `#else` |
|     - | 1411 | `		struct tm *pTm;` |
|     - | 1412 | `		time_t t;` |
|    28 | 1413 | `		time(&t);` |
|    28 | 1414 | `		pTm = gmtime(&t);` |
|    28 | 1415 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    28 | 1416 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1417 | `#endif` |
|    16 | 1418 | `	}else{` |
|     - | 1419 | `		/* Use the given timestamp */` |
|     - | 1420 | `		time_t t;` |
|     - | 1421 | `		struct tm *pTm;` |
|    11 | 1422 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    11 | 1423 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    11 | 1424 | `			pTm = gmtime(&t);` |
|    11 | 1425 | `			if( pTm == 0 ){` |
|   ! 0 | 1426 | `				time(&t);` |
|   ! 0 | 1427 | `			}` |
|     6 | 1428 | `		}else{` |
|   ! 0 | 1429 | `			time(&t);` |
|     - | 1430 | `		}` |
|    11 | 1431 | `		pTm = gmtime(&t);` |
|    11 | 1432 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    11 | 1433 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1434 | `	}` |
|     - | 1435 | `	/* Perform the requested operation */` |
|    40 | 1436 | `	switch(zFormat[0]){` |
|     2 | 1437 | `	case 'd':` |
|     - | 1438 | `		/* Day of the month */` |
|     5 | 1439 | `		iVal = sTm.tm_mday;` |
|     5 | 1440 | `		break;` |
|   ! 0 | 1441 | `	case 'h':` |
|     - | 1442 | `		/*	Hour (12 hour format)*/` |
|   ! 0 | 1443 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|   ! 0 | 1444 | `		break;` |
|     1 | 1445 | `	case 'H':` |
|     - | 1446 | `		/* Hour (24 hour format)*/` |
|     3 | 1447 | `		iVal = sTm.tm_hour;` |
|     3 | 1448 | `		break;` |
|     1 | 1449 | `	case 'i':` |
|     - | 1450 | `		/*Minutes*/` |
|     3 | 1451 | `		iVal = sTm.tm_min;` |
|     3 | 1452 | `		break;` |
|     1 | 1453 | `	case 'I':` |
|     - | 1454 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|     - | 1455 | `#ifdef __WINNT__` |
|     - | 1456 | `#ifdef _MSC_VER` |
|     - | 1457 | `#ifndef _WIN32_WCE` |
|     1 | 1458 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1459 | `#endif` |
|     - | 1460 | `#endif` |
|     - | 1461 | `#endif` |
|     3 | 1462 | `		iVal = sTm.tm_isdst;` |
|     3 | 1463 | `		break;` |
|     1 | 1464 | `	case 'L':` |
|     - | 1465 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|     3 | 1466 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|     3 | 1467 | `		break;` |
|     2 | 1468 | `	case 'm':` |
|     - | 1469 | `		/* Month number*/` |
|     5 | 1470 | `		iVal = sTm.tm_mon;` |
|     5 | 1471 | `		break;` |
|     1 | 1472 | `	case 's':` |
|     - | 1473 | `		/*Seconds*/` |
|     3 | 1474 | `		iVal = sTm.tm_sec;` |
|     3 | 1475 | `		break;` |
|     1 | 1476 | `	case 't':{` |
|     - | 1477 | `		/*Days in current month*/` |
|     - | 1478 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|     3 | 1479 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|     3 | 1480 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|   ! 0 | 1481 | `			nDays = 28;` |
|   ! 0 | 1482 | `		}` |
|     3 | 1483 | `		iVal = nDays;` |
|     3 | 1484 | `		break;` |
|     - | 1485 | `			 }` |
|     1 | 1486 | `	case 'U':` |
|     - | 1487 | `		/*Seconds since the Unix Epoch*/` |
|     3 | 1488 | `		iVal = (ph7_int64)time(0);` |
|     3 | 1489 | `		break;` |
|     1 | 1490 | `	case 'w':` |
|     - | 1491 | `		/*	Day of the week (0 on Sunday) */` |
|     3 | 1492 | `		iVal = sTm.tm_wday;` |
|     3 | 1493 | `		break;` |
|     1 | 1494 | `	case 'W': {` |
|     - | 1495 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|     - | 1496 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|     3 | 1497 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|     3 | 1498 | `		break;` |
|     - | 1499 | `			  }` |
|   ! 0 | 1500 | `	case 'y':` |
|     - | 1501 | `		/* Year (2 digits) */` |
|   ! 0 | 1502 | `		iVal = sTm.tm_year % 100;` |
|   ! 0 | 1503 | `		break;` |
|     3 | 1504 | `	case 'Y':` |
|     - | 1505 | `		/* Year (4 digits) */` |
|     7 | 1506 | `		iVal = sTm.tm_year;` |
|     7 | 1507 | `		break;` |
|     1 | 1508 | `	case 'z':` |
|     - | 1509 | `		/* Day of the year */` |
|     3 | 1510 | `		iVal = sTm.tm_yday;` |
|     3 | 1511 | `		break;` |
|     1 | 1512 | `	case 'Z':` |
|     - | 1513 | `		/*Timezone offset in seconds*/` |
|     3 | 1514 | `		iVal = sTm.tm_gmtoff;` |
|     3 | 1515 | `		break;` |
|     1 | 1516 | `	default:` |
|     - | 1517 | `		/* unknown format,throw a warning */` |
|     3 | 1518 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|     2 | 1519 | `		break;` |
|     - | 1520 | `	}` |
|     - | 1521 | `	/* Return the time value */` |
|    40 | 1522 | `	ph7_result_int64(pCtx,iVal);` |
|    40 | 1523 | `	return PH7_OK;` |
|    21 | 1524 | `}` |
|     - | 1525 | `/*` |
|     - | 1526 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|     - | 1527 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|     - | 1528 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|     - | 1529 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|     - | 1530 | ` *  specified.` |
|     - | 1531 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|     - | 1532 | ` *  the current value according to the local date and time.` |
|     - | 1533 | ` * Parameters` |
|     - | 1534 | ` * $hour` |
|     - | 1535 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|     - | 1536 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|     - | 1537 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|     - | 1538 | ` * $minute` |
|     - | 1539 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|     - | 1540 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|     - | 1541 | ` *  in the following hour(s).` |
|     - | 1542 | ` * $second` |
|     - | 1543 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|     - | 1544 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|     - | 1545 | ` * second in the following minute(s).` |
|     - | 1546 | ` * $month` |
|     - | 1547 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|     - | 1548 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|     - | 1549 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|     - | 1550 | ` * $day` |
|     - | 1551 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|     - | 1552 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|     - | 1553 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|     - | 1554 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|     - | 1555 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|     - | 1556 | ` * $year` |
|     - | 1557 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|     - | 1558 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|     - | 1559 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|     - | 1560 | ` * $is_dst` |
|     - | 1561 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|     - | 1562 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|     - | 1563 | ` * Return` |
|     - | 1564 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|     - | 1565 | ` *   If the arguments are invalid, the function returns FALSE` |
|     - | 1566 | ` */` |
|    36 | 1567 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1568 | `{` |
|     - | 1569 | `	const char *zFunction;` |
|     - | 1570 | `	ph7_int64 iVal;` |
|     - | 1571 | `	sxi64 h,mi,s,mo,d,y,yAdj;` |
|     - | 1572 | `	int moN;` |
|     - | 1573 | `	struct tm *pTm;` |
|     - | 1574 | `	time_t t;` |
|     - | 1575 | `	/* Extract function name */` |
|    37 | 1576 | `	zFunction = ph7_function_name(pCtx);` |
|     - | 1577 | `	/* PHP 8 dropped the legacy $is_dst 7th parameter: mktime()/gmmktime() now` |
|     - | 1578 | `	 * accept at most 6 arguments and throw a catchable ArgumentCountError` |
|     - | 1579 | `	 * otherwise (the central aBuiltinArity table only enforces the minimum, so` |
|     - | 1580 | `	 * this maximum is checked here). */` |
|    37 | 1581 | `	if( nArg > 6 ){` |
|    10 | 1582 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     3 | 1583 | `			"%s() expects at most 6 arguments, %d given",zFunction,nArg);` |
|     - | 1584 | `	}` |
|    31 | 1585 | `	if( nArg < 1 ){` |
|   ! 0 | 1586 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1587 | `			"%s() expects at least 1 argument, 0 given",zFunction);` |
|     - | 1588 | `	}` |
|     - | 1589 | `	/* Missing components default from the current time in php's default` |
|     - | 1590 | `	 * timezone. PHL's date_default_timezone_set() only accepts UTC/GMT (no tz` |
|     - | 1591 | `	 * database), so mktime() and gmmktime() agree and both read gmtime(). */` |
|    31 | 1592 | `	time(&t);` |
|    31 | 1593 | `	pTm = gmtime(&t);` |
|    15 | 1594 | `	SXUNUSED(zFunction);` |
|    31 | 1595 | `	h  = pTm->tm_hour;` |
|    31 | 1596 | `	mi = pTm->tm_min;` |
|    31 | 1597 | `	s  = pTm->tm_sec;` |
|    31 | 1598 | `	mo = pTm->tm_mon + 1;` |
|    31 | 1599 | `	d  = pTm->tm_mday;` |
|    31 | 1600 | `	y  = pTm->tm_year + 1900;` |
|    31 | 1601 | `	h = ph7_value_to_int64(apArg[0]);` |
|    31 | 1602 | `	if( nArg > 1 ){` |
|    31 | 1603 | `		mi = ph7_value_to_int64(apArg[1]);` |
|    31 | 1604 | `		if( nArg > 2 ){` |
|    31 | 1605 | `			s = ph7_value_to_int64(apArg[2]);` |
|    31 | 1606 | `			if( nArg > 3 ){` |
|    31 | 1607 | `				mo = ph7_value_to_int64(apArg[3]);` |
|    31 | 1608 | `				if( nArg > 4 ){` |
|    31 | 1609 | `					d = ph7_value_to_int64(apArg[4]);` |
|    31 | 1610 | `					if( nArg > 5 ){` |
|     - | 1611 | `						/* php's legacy two-digit mapping: 0-69 -> 2000-2069,` |
|     - | 1612 | `						 * 70-100 -> 1970-2000; anything else is verbatim */` |
|    31 | 1613 | `						y = ph7_value_to_int64(apArg[5]);` |
|    31 | 1614 | `						if( y >= 0 && y <= 69 ){` |
|     7 | 1615 | `							y += 2000;` |
|    28 | 1616 | `						}else if( y >= 70 && y <= 100 ){` |
|     5 | 1617 | `							y += 1900;` |
|     2 | 1618 | `						}` |
|    15 | 1619 | `					}` |
|    15 | 1620 | `				}` |
|    15 | 1621 | `			}` |
|    15 | 1622 | `		}` |
|    15 | 1623 | `	}` |
|     - | 1624 | `	/* Normalize the month with floor semantics, then let day/time components` |
|     - | 1625 | `	 * overflow linearly (php: mktime(25,-30,0,1,1,2024) == Jan 2 00:30). */` |
|    31 | 1626 | `	yAdj = y + DtFloorDiv(mo - 1,12);` |
|    31 | 1627 | `	moN  = (int)(mo - 1 - DtFloorDiv(mo - 1,12) * 12) + 1;` |
|    31 | 1628 | `	iVal = (DtDaysFromCivil(yAdj,moN,1) + (d - 1)) * 86400 + h*3600 + mi*60 + s;` |
|     - | 1629 | `	/* Return the timestamp as a 64bit integer */` |
|    31 | 1630 | `	ph7_result_int64(pCtx,iVal);` |
|    31 | 1631 | `	return PH7_OK;` |
|    19 | 1632 | `}` |
|     - | 1633 | `/*` |
|     - | 1634 | ` * string date_default_timezone_get(void)` |
|     - | 1635 | ` *  Gets the default timezone used by all date/time functions in a script.` |
|     - | 1636 | ` */` |
|     4 | 1637 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1638 | `{` |
|     5 | 1639 | `	ph7_vm *pVm = pCtx->pVm;` |
|     2 | 1640 | `	SXUNUSED(nArg);` |
|     2 | 1641 | `	SXUNUSED(apArg);` |
|     5 | 1642 | `	ph7_result_string(pCtx,pVm->zDefTz,(int)pVm->nDefTz);` |
|     5 | 1643 | `	return PH7_OK;` |
|     1 | 1644 | `}` |
|     - | 1645 | `/*` |
|     - | 1646 | ` * bool date_default_timezone_set(string $timezoneId)` |
|     - | 1647 | ` *  Sets the default timezone used by all date/time functions in a script.` |
|     - | 1648 | ` *  php validates against the tz database and stores the id verbatim (get()` |
|     - | 1649 | ` *  echoes back "utc" if that's what was set). PHL ships no tz database, so` |
|     - | 1650 | ` *  only UTC and GMT are accepted; every other id — including region names php` |
|     - | 1651 | ` *  would accept — is rejected with php's invalid-id notice (recorded scope cut).` |
|     - | 1652 | ` */` |
|    22 | 1653 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1654 | `{` |
|    23 | 1655 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1656 | `	const char *zId;` |
|     - | 1657 | `	int nId;` |
|    23 | 1658 | `	if( nArg < 1 ){` |
|   ! 0 | 1659 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1660 | `		return PH7_OK;` |
|     - | 1661 | `	}` |
|    23 | 1662 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|    23 | 1663 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|    23 | 1664 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|    23 | 1665 | `		pVm->zDefTz[3] = 0;` |
|    23 | 1666 | `		pVm->nDefTz = 3;` |
|    23 | 1667 | `		ph7_result_bool(pCtx,1);` |
|    23 | 1668 | `		return PH7_OK;` |
|     - | 1669 | `	}` |
|     - | 1670 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|     - | 1671 | `	 * — exactly php's notice shape here */` |
|   ! 0 | 1672 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|   ! 0 | 1673 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 1674 | `	return PH7_OK;` |
|    12 | 1675 | `}` |
|     - | 1676 |  |
|     - | 1677 | `/* ===========================================================================` |
|     - | 1678 | ` * DateTime family (NEWPLAN band D slice 1): DateTimeInterface, DateTime,` |
|     - | 1679 | ` * DateTimeImmutable, DateTimeZone (UTC + fixed offsets), date_create(),` |
|     - | 1680 | ` * date_create_immutable(). Embedded-PHP chunk + C thunks, following the` |
|     - | 1681 | ` * Reflection architecture (installed inside the bCompilingBuiltin window).` |
|     - | 1682 | ` * Timezone SCOPE: UTC and fixed "+HH:MM" offsets only — no tz database` |
|     - | 1683 | ` * (recorded §10 scope cut; named region zones throw like unknown zones).` |
|     - | 1684 | ` * ======================================================================== */` |
|     - | 1685 |  |
|     - | 1686 | `/*` |
|     - | 1687 | ` * Proleptic-Gregorian civil <-> day-count conversions (Howard Hinnant's` |
|     - | 1688 | ` * algorithms): no time_t / libc dependence, correct far past 2038 and` |
|     - | 1689 | ` * before 1970 on every platform. Day 0 == 1970-01-01.` |
|     - | 1690 | ` */` |
|   824 | 1691 | `static sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|     1 | 1692 | `{` |
|     - | 1693 | `	sxi64 era;` |
|     - | 1694 | `	unsigned yoe,doy,doe;` |
|   825 | 1695 | `	y -= (m <= 2);` |
|   825 | 1696 | `	era = (y >= 0 ? y : y - 399) / 400;` |
|   825 | 1697 | `	yoe = (unsigned)(y - era * 400);` |
|   825 | 1698 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|   825 | 1699 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|   825 | 1700 | `	return era * 146097 + (sxi64)doe - 719468;` |
|     1 | 1701 | `}` |
|   408 | 1702 | `static void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|     1 | 1703 | `{` |
|     - | 1704 | `	sxi64 era;` |
|     - | 1705 | `	unsigned doe,yoe,doy,mp;` |
|   409 | 1706 | `	z += 719468;` |
|   409 | 1707 | `	era = (z >= 0 ? z : z - 146096) / 146097;` |
|   409 | 1708 | `	doe = (unsigned)(z - era * 146097);` |
|   409 | 1709 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|   409 | 1710 | `	*py = (sxi64)yoe + era * 400;` |
|   409 | 1711 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|   409 | 1712 | `	mp = (5 * doy + 2) / 153;` |
|   409 | 1713 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|   409 | 1714 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|   409 | 1715 | `	if( *pm <= 2 ){` |
|   179 | 1716 | `		*py += 1;` |
|    89 | 1717 | `	}` |
|   409 | 1718 | `}` |
|   698 | 1719 | `static sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|     1 | 1720 | `{` |
|   699 | 1721 | `	sxi64 q = a / b;` |
|   699 | 1722 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|     3 | 1723 | `		q--;` |
|     1 | 1724 | `	}` |
|   699 | 1725 | `	return q;` |
|     1 | 1726 | `}` |
|     - | 1727 | `/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z) */` |
|   174 | 1728 | `static void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)` |
|     1 | 1729 | `{` |
|   175 | 1730 | `	sxi64 t = iTs + iOff;` |
|   175 | 1731 | `	sxi64 days = DtFloorDiv(t,86400);` |
|   175 | 1732 | `	sxi64 secs = t - days * 86400;` |
|     - | 1733 | `	sxi64 y;` |
|     - | 1734 | `	int mo,d;` |
|   175 | 1735 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|   175 | 1736 | `	pTm->tm_sec  = (int)(secs % 60);` |
|   175 | 1737 | `	pTm->tm_min  = (int)((secs / 60) % 60);` |
|   175 | 1738 | `	pTm->tm_hour = (int)(secs / 3600);` |
|   175 | 1739 | `	pTm->tm_mday = d;` |
|   175 | 1740 | `	pTm->tm_mon  = mo - 1;` |
|   175 | 1741 | `	pTm->tm_year = (int)y;` |
|   175 | 1742 | `	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */` |
|   175 | 1743 | `	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));` |
|   175 | 1744 | `	pTm->tm_isdst = 0;` |
|   175 | 1745 | `	pTm->tm_zone = zZone;` |
|   175 | 1746 | `	pTm->tm_gmtoff = (long)iOff;` |
|   175 | 1747 | `}` |
|   232 | 1748 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|     1 | 1749 | `{` |
|   233 | 1750 | `	return DtDaysFromCivil(y,mo,d) * 86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     1 | 1751 | `}` |
|     - | 1752 | `/* Month-arithmetic with php's overflow semantics (Jan 31 +1 month -> Mar 2/3):` |
|     - | 1753 | ` * normalize the month, keep the day — the civil day-count formula is linear in` |
|     - | 1754 | ` * d, so an out-of-range day simply lands in the following month. */` |
|    22 | 1755 | `static sxi64 DtAddMonths(sxi64 iTs,sxi32 iOff,sxi64 nMonths)` |
|     1 | 1756 | `{` |
|    23 | 1757 | `	sxi64 t = iTs + iOff;` |
|    23 | 1758 | `	sxi64 days = DtFloorDiv(t,86400);` |
|    23 | 1759 | `	sxi64 secs = t - days * 86400;` |
|     - | 1760 | `	sxi64 y;` |
|     - | 1761 | `	int mo,d;` |
|     - | 1762 | `	sxi64 m0;` |
|    23 | 1763 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|    23 | 1764 | `	m0 = (y * 12 + (mo - 1)) + nMonths;` |
|    23 | 1765 | `	y  = DtFloorDiv(m0,12);` |
|    23 | 1766 | `	mo = (int)(m0 - y * 12) + 1;` |
|    23 | 1767 | `	return DtDaysFromCivil(y,mo,d) * 86400 + secs - iOff;` |
|     1 | 1768 | `}` |
|     - | 1769 | `/*` |
|     - | 1770 | ` * Read a fractional-seconds part at z (which points at the '.'): up to 6 digits` |
|     - | 1771 | ` * become microseconds (right-padded to 6, extra digits ignored). Advances *pz.` |
|     - | 1772 | ` */` |
|    16 | 1773 | `static int DtReadFraction(const char **pz,const char *zEnd)` |
|     1 | 1774 | `{` |
|    17 | 1775 | `	const char *z = *pz;` |
|    17 | 1776 | `	int us = 0,n = 0;` |
|    17 | 1777 | `	z++; /* skip '.' */` |
|    71 | 1778 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|    55 | 1779 | `		if( n < 6 ){ us = us*10 + (z[0]-'0'); n++; }` |
|    55 | 1780 | `		z++;` |
|     1 | 1781 | `	}` |
|    59 | 1782 | `	while( n < 6 ){ us *= 10; n++; }` |
|    17 | 1783 | `	*pz = z;` |
|    17 | 1784 | `	return us;` |
|     1 | 1785 | `}` |
|     - | 1786 | `/*` |
|     - | 1787 | ` * Parse an OPTIONAL time-of-day suffix after a date component:` |
|     - | 1788 | ` * "[( \|T)]HH:MM[:SS][.frac][Z\|±hh[:mm]]". On entry *pz points just past the date;` |
|     - | 1789 | ` * the h/mi/s outs must be pre-zeroed and the offset outs pre-seeded with the current` |
|     - | 1790 | ` * offset; *pUs receives the microseconds from a fractional part (unchanged when` |
|     - | 1791 | ` * absent). Advances *pz over whatever it consumes. Returns 0 on success (whether or` |
|     - | 1792 | ` * not a time was present), or a 1-based error position into zIn (negative encodes` |
|     - | 1793 | ` * php's "Double time specification"). Shared by every absolute-date branch.` |
|     - | 1794 | ` */` |
|   184 | 1795 | `static int DtTimeSuffix(const char **pz,const char *zEnd,const char *zIn,` |
|     - | 1796 | `	int *ph,int *pmi,int *ps,sxi32 *piOff,int *pbOffSet,int *pUs)` |
|     1 | 1797 | `{` |
|   185 | 1798 | `	const char *z = *pz;` |
|   184 | 1799 | `	if( z < zEnd && (z[0]=='T' \|\| z[0]==' ') && zEnd-z >= 6` |
|    72 | 1800 | `	 && SyisDigit(z[1]) && SyisDigit(z[2]) && z[3]==':' ){` |
|    69 | 1801 | `		z++;` |
|    69 | 1802 | `		*ph  = (z[0]-'0')*10 + (z[1]-'0');` |
|    69 | 1803 | `		*pmi = (z[3]-'0')*10 + (z[4]-'0');` |
|     - | 1804 | `		/* a 25+ hour kills php's whole time token: error at its start */` |
|    69 | 1805 | `		if( *ph > 24 ){ return (int)(z - zIn) + 1; }` |
|     - | 1806 | `		/* php lexes HH:M, then the minute's second digit starts a SECOND time` |
|     - | 1807 | `		 * token: "Double time specification" (negative encoding) */` |
|    67 | 1808 | `		if( *pmi > 59 ){ return -((int)(&z[4] - zIn) + 1); }` |
|    65 | 1809 | `		z += 5;` |
|    65 | 1810 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|    63 | 1811 | `			*ps = (z[1]-'0')*10 + (z[2]-'0');` |
|    63 | 1812 | `			if( *ps > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|    61 | 1813 | `			z += 3;` |
|    30 | 1814 | `		}` |
|    63 | 1815 | `		if( z < zEnd && z[0]=='.' && zEnd-z >= 2 && SyisDigit(z[1]) ){ /* fractional seconds */` |
|    15 | 1816 | `			*pUs = DtReadFraction(&z,zEnd);` |
|     7 | 1817 | `		}` |
|    63 | 1818 | `		if( z < zEnd && (z[0]=='Z' \|\| z[0]=='z') ){` |
|     7 | 1819 | `			*piOff = 0; *pbOffSet = 2; z++;` |
|    60 | 1820 | `		}else if( z < zEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|     7 | 1821 | `			int sign = (z[0]=='-') ? -1 : 1;` |
|     7 | 1822 | `			int oh,om = 0;` |
|     7 | 1823 | `			z++;` |
|     7 | 1824 | `			if( zEnd-z < 2 \|\| !SyisDigit(z[0]) \|\| !SyisDigit(z[1]) ){ return (int)(z - zIn) + 1; }` |
|     7 | 1825 | `			oh = (z[0]-'0')*10 + (z[1]-'0');` |
|     7 | 1826 | `			z += 2;` |
|     7 | 1827 | `			if( z < zEnd && z[0]==':' ){ z++; }` |
|     7 | 1828 | `			if( zEnd-z >= 2 && SyisDigit(z[0]) && SyisDigit(z[1]) ){` |
|     7 | 1829 | `				om = (z[0]-'0')*10 + (z[1]-'0');` |
|     7 | 1830 | `				z += 2;` |
|     3 | 1831 | `			}` |
|     7 | 1832 | `			*piOff = sign * (oh*3600 + om*60);` |
|     7 | 1833 | `			*pbOffSet = 1;` |
|     3 | 1834 | `		}` |
|    31 | 1835 | `	}` |
|   179 | 1836 | `	*pz = z;` |
|   179 | 1837 | `	return 0;` |
|    93 | 1838 | `}` |
|     - | 1839 | `/*` |
|     - | 1840 | ` * Read one or two decimal digits at z (z<zEnd guaranteed by caller for the first).` |
|     - | 1841 | ` * Returns the value; *pn = digits consumed (1 or 2).` |
|     - | 1842 | ` */` |
|   118 | 1843 | `static int DtRead1or2(const char *z,const char *zEnd,int *pn)` |
|     1 | 1844 | `{` |
|   119 | 1845 | `	int v = z[0]-'0';` |
|   119 | 1846 | `	if( z+1 < zEnd && SyisDigit(z[1]) ){ v = v*10 + (z[1]-'0'); *pn = 2; }` |
|    13 | 1847 | `	else { *pn = 1; }` |
|   119 | 1848 | `	return v;` |
|     1 | 1849 | `}` |
|     - | 1850 | `/*` |
|     - | 1851 | ` * Try to read a non-ISO numeric date at z: three integer components joined by ONE` |
|     - | 1852 | ` * consistent separator, plus an optional time suffix. php's field order depends on` |
|     - | 1853 | ` * the separator:` |
|     - | 1854 | ` *   '/'      -> YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY` |
|     - | 1855 | ` *   '-','.'  -> DD-MM-YYYY (day first); a 4-digit-first '.' date (YYYY.MM.DD) is` |
|     - | 1856 | ` *               NOT a php format and is rejected. (ISO YYYY-MM-DD is matched by the` |
|     - | 1857 | ` *               dedicated branch BEFORE this one, so a 4-digit-first '-' never` |
|     - | 1858 | ` *               reaches here.)` |
|     - | 1859 | ` * A 1-2 digit year maps php-style (00-69 -> 2000s, 70-99 -> 1900s). Returns 0 when` |
|     - | 1860 | ` * the text is not such a date (caller falls through), 1 on success (the ts/off outs` |
|     - | 1861 | ` * set and *pzOut advanced past the whole token), or an error code in DtParse's own` |
|     - | 1862 | ` * convention (positive 1-based position into zIn, negative = "double time") when the` |
|     - | 1863 | ` * shape matched but a component is out of range.` |
|     - | 1864 | ` */` |
|    90 | 1865 | `static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,` |
|     - | 1866 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,int *pUs)` |
|     1 | 1867 | `{` |
|     - | 1868 | `	int a,b,c,na,nb,nc;` |
|     - | 1869 | `	char sep;` |
|    91 | 1870 | `	int y,mo,d,h = 0,mi = 0,s = 0,us = 0;` |
|    91 | 1871 | `	sxi32 iOff = *pOff;` |
|     - | 1872 | `	int rcT;` |
|     - | 1873 | `	/* first field: 1-4 digits */` |
|    91 | 1874 | `	if( !SyisDigit(z[0]) ){ return 0; }` |
|    91 | 1875 | `	a = 0; na = 0;` |
|   289 | 1876 | `	while( z < zEnd && SyisDigit(z[0]) && na < 4 ){ a = a*10 + (z[0]-'0'); z++; na++; }` |
|    91 | 1877 | `	if( z >= zEnd \|\| (z[0] != '-' && z[0] != '/' && z[0] != '.') ){ return 0; }` |
|    57 | 1878 | `	sep = z[0];` |
|    57 | 1879 | `	z++;` |
|     - | 1880 | `	/* second field: 1-2 digits */` |
|    57 | 1881 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    57 | 1882 | `	b = DtRead1or2(z,zEnd,&nb); z += nb;` |
|    57 | 1883 | `	if( z >= zEnd \|\| z[0] != sep ){ return 0; }` |
|    57 | 1884 | `	z++;` |
|     - | 1885 | `	/* third field: 1-4 digits */` |
|    57 | 1886 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    57 | 1887 | `	c = 0; nc = 0;` |
|   239 | 1888 | `	while( z < zEnd && SyisDigit(z[0]) && nc < 4 ){ c = c*10 + (z[0]-'0'); z++; nc++; }` |
|     - | 1889 | `	/* map fields to Y/M/D; nyear tracks the year field's width for 2-digit mapping.` |
|     - | 1890 | `	 * '/'  : YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY.` |
|     - | 1891 | `	 * '-'/'.': a 4-digit LAST field is DD-MM-YYYY (day first); otherwise YY-MM-DD` |
|     - | 1892 | `	 *          (year first) — php's width heuristic. (A 4-digit FIRST '-' field is` |
|     - | 1893 | `	 *          ISO and never reaches here; a 4-digit-first '.' is not a php format.) */` |
|     - | 1894 | `	{` |
|     - | 1895 | `		int nyear;` |
|    57 | 1896 | `		if( sep == '/' ){` |
|    27 | 1897 | `			if( na == 4 ){ y = a; mo = b; d = c; nyear = na; }` |
|    19 | 1898 | `			else{ mo = a; d = b; y = c; nyear = nc; }` |
|    44 | 1899 | `		}else if( sep == '.' ){` |
|     - | 1900 | `			/* php's dot date is DD.MM.YYYY only (a 4-digit year, day first). Other` |
|     - | 1901 | `			 * widths are not a clean php format (php itself yields garbage there),` |
|     - | 1902 | `			 * so don't claim the match — let the caller fail the parse. */` |
|     5 | 1903 | `			if( na == 4 \|\| nc != 4 ){ return 0; }` |
|     3 | 1904 | `			d = a; mo = b; y = c; nyear = nc;` |
|     2 | 1905 | `		}else{ /* '-' : a 4-digit LAST field is DD-MM-YYYY, else YY-MM-DD */` |
|    27 | 1906 | `			if( nc == 4 ){ d = a; mo = b; y = c; nyear = nc; }` |
|    11 | 1907 | `			else{ y = a; mo = b; d = c; nyear = na; }` |
|     - | 1908 | `		}` |
|    55 | 1909 | `		if( nyear <= 2 ){` |
|    11 | 1910 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|     3 | 1911 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|     5 | 1912 | `		}` |
|     - | 1913 | `	}` |
|     - | 1914 | `	/* php normalizes month 0 to December of the previous year (like the ISO branch)` |
|     - | 1915 | `	 * but fails a month past 12; a day past 31 fails, while day 0 normalizes in` |
|     - | 1916 | `	 * DtMakeTs. Errors point at the field end. */` |
|    55 | 1917 | `	if( mo > 12 ){ return (int)(z - zIn) + 1; }` |
|    51 | 1918 | `	if( mo == 0 ){ mo = 12; y--; }` |
|    51 | 1919 | `	if( d > 31 ){ return (int)(z - zIn) + 1; }` |
|     - | 1920 | `	/* optional time-of-day suffix, then commit */` |
|    47 | 1921 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff,&us);` |
|    47 | 1922 | `	if( rcT != 0 ){ return rcT; }` |
|    47 | 1923 | `	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    47 | 1924 | `	*pOff = iOff;` |
|    47 | 1925 | `	*pUs = us;` |
|    47 | 1926 | `	*pzOut = z;` |
|    47 | 1927 | `	return 1;` |
|    46 | 1928 | `}` |
|     - | 1929 | `/*` |
|     - | 1930 | ` * Match a month name at z (full name or its distinct 3-letter abbreviation, plus` |
|     - | 1931 | ` * "sept"), case-insensitively and only at a word boundary. Returns the month 1-12` |
|     - | 1932 | ` * and sets *pAdv to the bytes consumed, or 0 when no month name is present.` |
|     - | 1933 | ` */` |
|   236 | 1934 | `static int DtMatchMonth(const char *z,const char *zEnd,int *pAdv)` |
|     1 | 1935 | `{` |
|     - | 1936 | `	static const struct { const char *z; int n; int mo; } aM[] = {` |
|     - | 1937 | `		{ "january",7,1 },{ "february",8,2 },{ "march",5,3 },{ "april",5,4 },` |
|     - | 1938 | `		{ "june",4,6 },{ "july",4,7 },{ "august",6,8 },{ "september",9,9 },` |
|     - | 1939 | `		{ "sept",4,9 },{ "october",7,10 },{ "november",8,11 },{ "december",8,12 },` |
|     - | 1940 | `		{ "may",3,5 },` |
|     - | 1941 | `		{ "jan",3,1 },{ "feb",3,2 },{ "mar",3,3 },{ "apr",3,4 },{ "jun",3,6 },` |
|     - | 1942 | `		{ "jul",3,7 },{ "aug",3,8 },{ "sep",3,9 },{ "oct",3,10 },{ "nov",3,11 },` |
|     - | 1943 | `		{ "dec",3,12 }` |
|     - | 1944 | `	};` |
|     - | 1945 | `	sxu32 i;` |
|  4805 | 1946 | `	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){` |
|  4631 | 1947 | `		int n = aM[i].n;` |
|  4630 | 1948 | `		if( zEnd - z >= n && SyStrnicmp(z,aM[i].z,(sxu32)n) == 0` |
|  2186 | 1949 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|    63 | 1950 | `			*pAdv = n;` |
|    63 | 1951 | `			return aM[i].mo;` |
|     - | 1952 | `		}` |
|  2285 | 1953 | `	}` |
|   175 | 1954 | `	return 0;` |
|   119 | 1955 | `}` |
|     - | 1956 | `/* Match a weekday name at z (full or 3-letter, case-insensitive, word boundary).` |
|     - | 1957 | ` * Returns the day-of-week 0=Sunday..6=Saturday and sets *pAdv, or -1. */` |
|   154 | 1958 | `static int DtMatchWeekday(const char *z,const char *zEnd,int *pAdv)` |
|     1 | 1959 | `{` |
|     - | 1960 | `	static const struct { const char *z; int n; int dow; } aW[] = {` |
|     - | 1961 | `		{ "sunday",6,0 },{ "monday",6,1 },{ "tuesday",7,2 },{ "wednesday",9,3 },` |
|     - | 1962 | `		{ "thursday",8,4 },{ "friday",6,5 },{ "saturday",8,6 },` |
|     - | 1963 | `		{ "sun",3,0 },{ "mon",3,1 },{ "tue",3,2 },{ "wed",3,3 },{ "thu",3,4 },` |
|     - | 1964 | `		{ "fri",3,5 },{ "sat",3,6 }` |
|     - | 1965 | `	};` |
|     - | 1966 | `	sxu32 i;` |
|  1865 | 1967 | `	for( i = 0 ; i < SX_ARRAYSIZE(aW) ; ++i ){` |
|  1755 | 1968 | `		int n = aW[i].n;` |
|  1754 | 1969 | `		if( zEnd - z >= n && SyStrnicmp(z,aW[i].z,(sxu32)n) == 0` |
|   737 | 1970 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|    45 | 1971 | `			*pAdv = n;` |
|    45 | 1972 | `			return aW[i].dow;` |
|     - | 1973 | `		}` |
|   856 | 1974 | `	}` |
|   111 | 1975 | `	return -1;` |
|    78 | 1976 | `}` |
|     - | 1977 | `/* True if z points at a two-letter English ordinal suffix (st/nd/rd/th). */` |
|    62 | 1978 | `static int DtIsOrdinal(const char *z,const char *zEnd)` |
|     1 | 1979 | `{` |
|    63 | 1980 | `	if( zEnd - z < 2 ){ return 0; }` |
|   119 | 1981 | `	return SyStrnicmp(z,"st",2) == 0 \|\| SyStrnicmp(z,"nd",2) == 0` |
|    89 | 1982 | `		\|\| SyStrnicmp(z,"rd",2) == 0 \|\| SyStrnicmp(z,"th",2) == 0;` |
|    32 | 1983 | `}` |
|     - | 1984 | `/*` |
|     - | 1985 | ` * Try to read a textual-month date at z, in either order:` |
|     - | 1986 | ` *   MonthName [Day] [Year]   ("Jan 15 2020", "January", "January 2020")` |
|     - | 1987 | ` *   Day MonthName [Year]     ("15 January 2020", "15th Jan")` |
|     - | 1988 | ` * A missing day defaults to 1, a missing year to the base timestamp's year (php).` |
|     - | 1989 | ` * Day may carry an ordinal suffix, fields may be comma-separated, month names are` |
|     - | 1990 | ` * case-insensitive, and an optional time-of-day suffix + trailing UTC/GMT is` |
|     - | 1991 | ` * consumed. Returns 0 (not a month date — caller falls through, *pzOut untouched),` |
|     - | 1992 | ` * 1 on success, or a DtParse error code (out-of-range day).` |
|     - | 1993 | ` */` |
|   188 | 1994 | `static int DtTryMonthDate(const char *z,const char *zEnd,const char **pzOut,` |
|     - | 1995 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,sxi64 iBaseTs,int *pUs)` |
|     1 | 1996 | `{` |
|   189 | 1997 | `	int mo,d = 1,adv,haveDay = 0,haveYear = 0;` |
|   189 | 1998 | `	sxi64 y = 0;` |
|   189 | 1999 | `	int h = 0,mi = 0,s = 0,us = 0;` |
|   189 | 2000 | `	sxi32 iOff = *pOff;` |
|     - | 2001 | `	int rcT;` |
|     - | 2002 | `#define MDSKIPWS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|   189 | 2003 | `	if( (mo = DtMatchMonth(z,zEnd,&adv)) != 0 ){` |
|     - | 2004 | `		/* MonthName [Day] [Year]. A 4-digit number here is the YEAR, not the day` |
|     - | 2005 | `		 * ("January 2020" is month+year, day defaults); a 1-2 digit number is the day. */` |
|    31 | 2006 | `		z += adv;` |
|    76 | 2007 | `		MDSKIPWS();` |
|    31 | 2008 | `		if( z < zEnd && SyisDigit(z[0]) ){` |
|    31 | 2009 | `			int nrun = 0;` |
|    31 | 2010 | `			const char *zp = z;` |
|    95 | 2011 | `			while( zp < zEnd && SyisDigit(zp[0]) && nrun < 4 ){ zp++; nrun++; }` |
|    31 | 2012 | `			if( nrun < 4 ){` |
|    27 | 2013 | `				d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|    27 | 2014 | `				if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|    27 | 2015 | `				haveDay = 1;` |
|    68 | 2016 | `				MDSKIPWS();` |
|    13 | 2017 | `			}` |
|    16 | 2018 | `		}` |
|   174 | 2019 | `	}else if( SyisDigit(z[0]) ){` |
|     - | 2020 | `		/* Day MonthName [Year] */` |
|    37 | 2021 | `		d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|    37 | 2022 | `		if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|    37 | 2023 | `		haveDay = 1;` |
|    77 | 2024 | `		MDSKIPWS();` |
|    37 | 2025 | `		if( (mo = DtMatchMonth(z,zEnd,&adv)) == 0 ){ return 0; }` |
|    21 | 2026 | `		z += adv;` |
|    49 | 2027 | `		MDSKIPWS();` |
|    11 | 2028 | `	}else{` |
|   123 | 2029 | `		return 0;` |
|     - | 2030 | `	}` |
|     - | 2031 | `	/* optional year */` |
|    51 | 2032 | `	if( z < zEnd && SyisDigit(z[0]) ){` |
|    47 | 2033 | `		int ny = 0;` |
|    47 | 2034 | `		y = 0;` |
|   231 | 2035 | `		while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ y = y*10 + (z[0]-'0'); z++; ny++; }` |
|    47 | 2036 | `		if( ny <= 2 ){` |
|   ! 0 | 2037 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|   ! 0 | 2038 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|   ! 0 | 2039 | `		}` |
|    47 | 2040 | `		haveYear = 1;` |
|    23 | 2041 | `	}` |
|     - | 2042 | `	/* Default the unspecified fields from the base timestamp. php overlays: a` |
|     - | 2043 | `	 * missing year takes the base year; a missing day is 1 when a year WAS given` |
|     - | 2044 | `	 * ("January 2020" -> the 1st) but the base day when only the month was named` |
|     - | 2045 | `	 * ("January" -> the base day). */` |
|     - | 2046 | `	{` |
|     - | 2047 | `		sxi64 by; int bm,bd;` |
|    51 | 2048 | `		DtCivilFromDays(DtFloorDiv(iBaseTs + *pOff,86400),&by,&bm,&bd);` |
|    51 | 2049 | `		if( !haveYear ){ y = by; }` |
|    51 | 2050 | `		if( !haveDay ){ d = haveYear ? 1 : bd; }` |
|     - | 2051 | `	}` |
|    51 | 2052 | `	if( d > 31 ){ return (int)(z - zIn) + 1; }` |
|     - | 2053 | `	/* optional time-of-day suffix */` |
|    51 | 2054 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff,&us);` |
|    51 | 2055 | `	if( rcT != 0 ){ return rcT; }` |
|     - | 2056 | `	/* optional trailing UTC/GMT zone name (PHL's default zone is already UTC) */` |
|    55 | 2057 | `	MDSKIPWS();` |
|    50 | 2058 | `	if( (zEnd-z >= 3 && SyStrnicmp(z,"utc",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3])))` |
|    49 | 2059 | `	 \|\| (zEnd-z >= 3 && SyStrnicmp(z,"gmt",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3]))) ){` |
|     3 | 2060 | `		iOff = 0; z += 3;` |
|     1 | 2061 | `	}` |
|    51 | 2062 | `	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    51 | 2063 | `	*pOff = iOff;` |
|    51 | 2064 | `	*pUs = us;` |
|    51 | 2065 | `	*pzOut = z;` |
|    51 | 2066 | `	return 1;` |
|     - | 2067 | `#undef MDSKIPWS` |
|    95 | 2068 | `}` |
|     - | 2069 | `/*` |
|     - | 2070 | ` * Minimal php-datetime-string parser (slice 1): absolute forms` |
|     - | 2071 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]",` |
|     - | 2072 | ` * keywords today/midnight/noon/tomorrow/yesterday, and relative sequences` |
|     - | 2073 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". Returns 0 on` |
|     - | 2074 | ` * success (ts/off/bOffSet out), or the byte position of the first` |
|     - | 2075 | ` * unparseable character +1 (for php's "at position N" message).` |
|     - | 2076 | ` */` |
|   442 | 2077 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,` |
|     - | 2078 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs)` |
|     1 | 2079 | `{` |
|   443 | 2080 | `	const char *z = zIn, *zEnd = &zIn[nLen];` |
|   443 | 2081 | `	sxi64 iTs = iBaseTs;` |
|   443 | 2082 | `	sxi32 iOff = iBaseOff;` |
|   443 | 2083 | `	int bOffSet = 0;` |
|   443 | 2084 | `	int bAny = 0;` |
|     - | 2085 | `	int iNumRc,iMonRc;` |
|   443 | 2086 | `	int uSec = 0;` |
|   443 | 2087 | `	*pUs = 0;` |
|     - | 2088 | `#define DT_SKIP_WS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|     - | 2089 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|     - | 2090 | `	&& (zEnd-z == (nKw) \|\| !SyisAlpha(z[(nKw)])))` |
|   670 | 2091 | `	DT_SKIP_WS();` |
|   443 | 2092 | `	if( z >= zEnd ){` |
|     - | 2093 | `		/* php: the empty string is "now" */` |
|     3 | 2094 | `		*pTs = iTs;` |
|     3 | 2095 | `		*pOff = iOff;` |
|     3 | 2096 | `		*pbOffSet = bOffSet;` |
|     3 | 2097 | `		return 0;` |
|     - | 2098 | `	}` |
|     - | 2099 | `	/* "@<seconds>" absolute epoch */` |
|   441 | 2100 | `	if( z[0] == '@' ){` |
|    77 | 2101 | `		int neg = 0;` |
|    77 | 2102 | `		sxi64 v = 0;` |
|    77 | 2103 | `		const char *zAt = z;` |
|    77 | 2104 | `		z++;` |
|    77 | 2105 | `		if( z < zEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|     - | 2106 | `		/* php's lexer rejects the whole token: the error points at the '@' */` |
|    77 | 2107 | `		if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zAt - zIn) + 1; }` |
|   217 | 2108 | `		while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|     - | 2109 | `		/* php accepts a fractional epoch ("@1600000000.5" -> .5s = 500000us) */` |
|    75 | 2110 | `		if( z < zEnd && z[0]=='.' && zEnd-z >= 2 && SyisDigit(z[1]) ){` |
|     3 | 2111 | `			*pUs = DtReadFraction(&z,zEnd);` |
|     1 | 2112 | `		}` |
|    75 | 2113 | `		*pTs = neg ? -v : v;` |
|    75 | 2114 | `		*pOff = 0;` |
|    75 | 2115 | `		*pbOffSet = 1;` |
|    75 | 2116 | `		DT_SKIP_WS();` |
|    75 | 2117 | `		return (z < zEnd) ? (int)(z - zIn) + 1 : 0;` |
|     - | 2118 | `	}` |
|     - | 2119 | `	/* Absolute date: YYYY-MM-DD[...] */` |
|   364 | 2120 | `	if( zEnd-z >= 10 && SyisDigit(z[0]) && SyisDigit(z[1]) && SyisDigit(z[2])` |
|   135 | 2121 | `	 && SyisDigit(z[3]) && z[4]=='-' ){` |
|    99 | 2122 | `		sxi64 y = (z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0');` |
|    99 | 2123 | `		int mo,d,h=0,mi=0,s=0;` |
|    99 | 2124 | `		if( !SyisDigit(z[5])\|\|!SyisDigit(z[6])\|\|z[7] != '-'\|\|!SyisDigit(z[8])\|\|!SyisDigit(z[9]) ){` |
|   ! 0 | 2125 | `			return (int)(z - zIn) + 1;` |
|     - | 2126 | `		}` |
|    99 | 2127 | `		mo = (z[5]-'0')*10 + (z[6]-'0');` |
|    99 | 2128 | `		d  = (z[8]-'0')*10 + (z[9]-'0');` |
|     - | 2129 | `		/* php's lexer dies on the SECOND digit of an out-of-range month/day` |
|     - | 2130 | `		 * (either the two-digit pattern fails there, or a one-digit component` |
|     - | 2131 | `		 * matched and the separator check fails there); "00" lexes fine and` |
|     - | 2132 | `		 * normalizes (month 0 == December of the previous year). */` |
|    99 | 2133 | `		if( mo > 12 ){ return (int)(&z[6] - zIn) + 1; }` |
|    93 | 2134 | `		if( d > 31 ){ return (int)(&z[9] - zIn) + 1; }` |
|    89 | 2135 | `		if( mo == 0 ){ mo = 12; y--; }` |
|    89 | 2136 | `		z += 10;` |
|     - | 2137 | `		{` |
|    89 | 2138 | `			int rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,&bOffSet,&uSec);` |
|    89 | 2139 | `			if( rcT != 0 ){ return rcT; }` |
|     - | 2140 | `		}` |
|    83 | 2141 | `		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    83 | 2142 | `		bAny = 1;` |
|   308 | 2143 | `	}else if( SyisDigit(z[0])` |
|   179 | 2144 | `	 && (iNumRc = DtTryNumericDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,&uSec)) != 0 ){` |
|     - | 2145 | `		/* DD-MM-YYYY / DD.MM.YYYY (day first), MM/DD/YYYY (slash, American), and` |
|     - | 2146 | `		 * YYYY/MM/DD (slash, year first) — see DtTryNumericDate. Anything other than` |
|     - | 2147 | `		 * 1 is an error code in DtParse's own convention (positive position / negative` |
|     - | 2148 | `		 * "double time"); propagate it verbatim. */` |
|    55 | 2149 | `		if( iNumRc != 1 ){ return iNumRc; }` |
|    47 | 2150 | `		bAny = 1;` |
|   236 | 2151 | `	}else if( (SyisAlpha(z[0]) \|\| SyisDigit(z[0]))` |
|   201 | 2152 | `	 && (iMonRc = DtTryMonthDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,iBaseTs,&uSec)) != 0 ){` |
|     - | 2153 | `		/* MonthName Day Year / Day MonthName Year, in any of php's spellings. As with` |
|     - | 2154 | `		 * DtTryNumericDate, anything other than 1 is an error code to propagate. */` |
|    51 | 2155 | `		if( iMonRc != 1 ){ return iMonRc; }` |
|    51 | 2156 | `		bAny = 1;` |
|   188 | 2157 | `	}else if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|    14 | 2158 | `	 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|     - | 2159 | `		/* Time-only: HH:MM[:SS] on the base date */` |
|    11 | 2160 | `		sxi64 t = iTs + iOff;` |
|    11 | 2161 | `		sxi64 days = DtFloorDiv(t,86400);` |
|    11 | 2162 | `		int h  = (z[0]-'0')*10 + (z[1]-'0');` |
|    11 | 2163 | `		int mi = (z[3]-'0')*10 + (z[4]-'0');` |
|    11 | 2164 | `		int s = 0;` |
|     - | 2165 | `		/* php: bad hour kills the token (error at its start); bad minute /` |
|     - | 2166 | `		 * second dies on the component's second digit */` |
|    11 | 2167 | `		if( h > 24 ){ return (int)(z - zIn) + 1; }` |
|     9 | 2168 | `		if( mi > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|     7 | 2169 | `		z += 5;` |
|     7 | 2170 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     5 | 2171 | `			s = (z[1]-'0')*10 + (z[2]-'0');` |
|     5 | 2172 | `			if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|     3 | 2173 | `			z += 3;` |
|     1 | 2174 | `		}` |
|     5 | 2175 | `		iTs = days*86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     5 | 2176 | `		bAny = 1;` |
|   155 | 2177 | `	}else if( DT_LOWEQ("now",3) ){` |
|     3 | 2178 | `		z += 3;` |
|     3 | 2179 | `		bAny = 1;` |
|     1 | 2180 | `	}` |
|     - | 2181 | `	/* Relative / keyword sequence */` |
|   167 | 2182 | `	for(;;){` |
|   596 | 2183 | `		DT_SKIP_WS();` |
|   485 | 2184 | `		if( z >= zEnd ){` |
|   317 | 2185 | `			break;` |
|     - | 2186 | `		}` |
|   169 | 2187 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|     9 | 2188 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     9 | 2189 | `			iTs = days*86400 - iOff;` |
|     9 | 2190 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|     9 | 2191 | `			bAny = 1;` |
|     9 | 2192 | `			continue;` |
|     - | 2193 | `		}` |
|   161 | 2194 | `		if( DT_LOWEQ("noon",4) ){` |
|     3 | 2195 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     3 | 2196 | `			iTs = days*86400 + 12*3600 - iOff;` |
|     3 | 2197 | `			z += 4;` |
|     3 | 2198 | `			bAny = 1;` |
|     3 | 2199 | `			continue;` |
|     - | 2200 | `		}` |
|   159 | 2201 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|     3 | 2202 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) + 1;` |
|     3 | 2203 | `			iTs = days*86400 - iOff;` |
|     3 | 2204 | `			z += 8;` |
|     3 | 2205 | `			bAny = 1;` |
|     3 | 2206 | `			continue;` |
|     - | 2207 | `		}` |
|   157 | 2208 | `		if( DT_LOWEQ("yesterday",9) ){` |
|     3 | 2209 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) - 1;` |
|     3 | 2210 | `			iTs = days*86400 - iOff;` |
|     3 | 2211 | `			z += 9;` |
|     3 | 2212 | `			bAny = 1;` |
|     3 | 2213 | `			continue;` |
|     - | 2214 | `		}` |
|     - | 2215 | `		/* Weekday navigation: "[next\|last\|previous\|this] <weekday>" moves to the` |
|     - | 2216 | `		 * midnight of the target weekday. Bare/"this" = the this-week occurrence on` |
|     - | 2217 | `		 * or after the base day; "next"/"last"/"previous" skip a matching base day. */` |
|     - | 2218 | `		{` |
|   155 | 2219 | `			const char *zSave = z;` |
|   155 | 2220 | `			int dir = 0;         /* 0 = this-week occurrence, 1 = next, -1 = last */` |
|     - | 2221 | `			int adv,dow;` |
|   188 | 2222 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; DT_SKIP_WS(); }` |
|   136 | 2223 | `			else if( DT_LOWEQ("previous",8) ){ dir = -1; z += 8; DT_SKIP_WS(); }` |
|   179 | 2224 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; DT_SKIP_WS(); }` |
|   114 | 2225 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; DT_SKIP_WS(); }` |
|   155 | 2226 | `			dow = DtMatchWeekday(z,zEnd,&adv);` |
|   155 | 2227 | `			if( dow >= 0 ){` |
|    45 | 2228 | `				sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|    45 | 2229 | `				int bdow = (int)(((days + 4) % 7 + 7) % 7); /* 1970-01-01 was Thursday */` |
|     - | 2230 | `				sxi64 delta;` |
|    45 | 2231 | `				if( dir == 1 ){` |
|    13 | 2232 | `					delta = ((dow - bdow) % 7 + 7) % 7;` |
|    13 | 2233 | `					if( delta == 0 ){ delta = 7; }` |
|    39 | 2234 | `				}else if( dir == -1 ){` |
|    13 | 2235 | `					delta = -(((bdow - dow) % 7 + 7) % 7);` |
|    13 | 2236 | `					if( delta == 0 ){ delta = -7; }` |
|     7 | 2237 | `				}else{` |
|    21 | 2238 | `					delta = ((dow - bdow) % 7 + 7) % 7;` |
|     - | 2239 | `				}` |
|    45 | 2240 | `				iTs = (days + delta)*86400 - iOff; /* midnight of the target day */` |
|    45 | 2241 | `				z += adv;` |
|    45 | 2242 | `				bAny = 1;` |
|    45 | 2243 | `				continue;` |
|     - | 2244 | `			}` |
|   111 | 2245 | `			z = zSave; /* prefix did not introduce a weekday: rewind and try the rest */` |
|     - | 2246 | `		}` |
|     - | 2247 | `		/* "first\|last day of (this\|next\|last month \| MonthName [Year])": jump to the` |
|     - | 2248 | `		 * first or last day of a target month. A this/next/last-month target keeps the` |
|     - | 2249 | `		 * base time-of-day; an absolute MonthName [Year] target resets it to midnight` |
|     - | 2250 | `		 * (php). */` |
|   111 | 2251 | `		if( DT_LOWEQ("first",5) \|\| DT_LOWEQ("last",4) ){` |
|    39 | 2252 | `			const char *zSave = z;` |
|    39 | 2253 | `			int bFirst = (SyToLower((unsigned char)z[0]) == 'f');` |
|    39 | 2254 | `			z += bFirst ? 5 : 4;` |
|    96 | 2255 | `			DT_SKIP_WS();` |
|    39 | 2256 | `			if( DT_LOWEQ("day",3) ){` |
|    31 | 2257 | `				z += 3;` |
|    76 | 2258 | `				DT_SKIP_WS();` |
|    31 | 2259 | `				if( DT_LOWEQ("of",2) ){` |
|    31 | 2260 | `					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);` |
|     - | 2261 | `					sxi64 yy,tod;` |
|    31 | 2262 | `					int mm,dd0,keepTime = 1,ok = 1;` |
|    31 | 2263 | `					z += 2;` |
|    74 | 2264 | `					DT_SKIP_WS();` |
|    31 | 2265 | `					DtCivilFromDays(days0,&yy,&mm,&dd0);` |
|    31 | 2266 | `					tod = (iTs + iOff) - days0*86400;` |
|    40 | 2267 | `					if( DT_LOWEQ("this",4) ){ z += 4; DT_SKIP_WS();` |
|     7 | 2268 | `						if( DT_LOWEQ("month",5) ){ z += 5; }else{ ok = 0; } }` |
|    34 | 2269 | `					else if( DT_LOWEQ("next",4) ){ z += 4; DT_SKIP_WS();` |
|     7 | 2270 | `						if( DT_LOWEQ("month",5) ){ z += 5; mm++; if(mm>12){ mm=1; yy++; } }else{ ok = 0; } }` |
|    25 | 2271 | `					else if( DT_LOWEQ("last",4) ){ z += 4; DT_SKIP_WS();` |
|     5 | 2272 | `						if( DT_LOWEQ("month",5) ){ z += 5; mm--; if(mm<1){ mm=12; yy--; } }else{ ok = 0; } }` |
|    15 | 2273 | `					else if( z < zEnd ){` |
|     - | 2274 | `						int mo,adv;` |
|    13 | 2275 | `						mo = DtMatchMonth(z,zEnd,&adv);` |
|    13 | 2276 | `						if( mo == 0 ){ return (int)(z - zIn) + 1; }` |
|    27 | 2277 | `						z += adv; DT_SKIP_WS();` |
|    13 | 2278 | `						mm = mo; keepTime = 0; tod = 0;` |
|    13 | 2279 | `						if( z < zEnd && SyisDigit(z[0]) ){` |
|     9 | 2280 | `							int ny = 0; sxi64 yv = 0;` |
|    41 | 2281 | `							while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ yv = yv*10 + (z[0]-'0'); z++; ny++; }` |
|     9 | 2282 | `							if( ny <= 2 ){ if( yv <= 69 ){ yv += 2000; } else if( yv <= 99 ){ yv += 1900; } }` |
|     9 | 2283 | `							yy = yv;` |
|     4 | 2284 | `						}` |
|     6 | 2285 | `					}` |
|     - | 2286 | `					/* else: "... day of" with nothing after — php defaults to this` |
|     - | 2287 | `					 * month (mm/yy/tod stay the base, keepTime stays 1). */` |
|    31 | 2288 | `					if( ok ){` |
|    31 | 2289 | `						int dim = (int)(DtDaysFromCivil(yy,mm+1,1) - DtDaysFromCivil(yy,mm,1));` |
|    31 | 2290 | `						int day = bFirst ? 1 : dim;` |
|    31 | 2291 | `						iTs = DtDaysFromCivil(yy,mm,day)*86400 + (keepTime ? tod : 0) - iOff;` |
|    31 | 2292 | `						bAny = 1;` |
|    31 | 2293 | `						continue;` |
|     - | 2294 | `					}` |
|   ! 0 | 2295 | `				}` |
|   ! 0 | 2296 | `			}` |
|     9 | 2297 | `			z = zSave; /* not the "first\|last day of ..." shape: rewind */` |
|     4 | 2298 | `		}` |
|     - | 2299 | `		/* Standalone "this\|next\|last (month\|week)": month shifts by ±1 keeping the` |
|     - | 2300 | `		 * day/time; week moves to the Monday of this/next/last ISO week keeping the` |
|     - | 2301 | `		 * time-of-day (php: weeks start on Monday). */` |
|     - | 2302 | `		{` |
|   103 | 2303 | `			const char *zSave = z;` |
|   103 | 2304 | `			int dir = 2; /* 2 = no prefix */` |
|   103 | 2305 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; }` |
|    71 | 2306 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; }` |
|    63 | 2307 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; }` |
|    81 | 2308 | `			if( dir != 2 ){` |
|    66 | 2309 | `				DT_SKIP_WS();` |
|    27 | 2310 | `				if( DT_LOWEQ("month",5) ){` |
|    13 | 2311 | `					z += 5;` |
|    13 | 2312 | `					iTs = DtAddMonths(iTs,iOff,dir);` |
|    13 | 2313 | `					bAny = 1;` |
|    13 | 2314 | `					continue;` |
|     - | 2315 | `				}` |
|    15 | 2316 | `				if( DT_LOWEQ("week",4) ){` |
|    13 | 2317 | `					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);` |
|    13 | 2318 | `					sxi64 tod = (iTs + iOff) - days0*86400;` |
|    13 | 2319 | `					int bdow = (int)(((days0 + 4) % 7 + 7) % 7);` |
|    13 | 2320 | `					sxi64 monday = days0 - ((bdow + 6) % 7); /* Monday of the base week */` |
|    13 | 2321 | `					z += 4;` |
|    13 | 2322 | `					monday += (sxi64)dir * 7;` |
|    13 | 2323 | `					iTs = monday*86400 + tod - iOff;` |
|    13 | 2324 | `					bAny = 1;` |
|    13 | 2325 | `					continue;` |
|     - | 2326 | `				}` |
|     1 | 2327 | `			}` |
|    57 | 2328 | `			z = zSave;` |
|     - | 2329 | `		}` |
|     - | 2330 | `		/* Trailing time-of-day in a relative sequence ("next thursday 15:00"): set` |
|     - | 2331 | `		 * the clock on the current day. The leading absolute HH:MM branch handles a` |
|     - | 2332 | `		 * time at the START; this handles one AFTER a date/relative token. */` |
|    56 | 2333 | `		if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|    16 | 2334 | `		 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|    13 | 2335 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|    13 | 2336 | `			int hh = (z[0]-'0')*10 + (z[1]-'0');` |
|    13 | 2337 | `			int mm = (z[3]-'0')*10 + (z[4]-'0');` |
|    13 | 2338 | `			int ss = 0;` |
|    13 | 2339 | `			if( hh > 24 ){ return (int)(z - zIn) + 1; }` |
|    13 | 2340 | `			if( mm > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|    13 | 2341 | `			z += 5;` |
|    13 | 2342 | `			if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     5 | 2343 | `				ss = (z[1]-'0')*10 + (z[2]-'0');` |
|     5 | 2344 | `				if( ss > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|     5 | 2345 | `				z += 3;` |
|     2 | 2346 | `			}` |
|    13 | 2347 | `			iTs = days*86400 + (sxi64)hh*3600 + (sxi64)mm*60 + ss - iOff;` |
|    13 | 2348 | `			bAny = 1;` |
|    13 | 2349 | `			continue;` |
|     - | 2350 | `		}` |
|    45 | 2351 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|    33 | 2352 | `			int neg = 0;` |
|    33 | 2353 | `			sxi64 v = 0;` |
|    33 | 2354 | `			const char *zNumStart = z;` |
|    33 | 2355 | `			if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; }` |
|    33 | 2356 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zNumStart - zIn) + 1; }` |
|    81 | 2357 | `			while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|    33 | 2358 | `			if( neg ){ v = -v; }` |
|    79 | 2359 | `			DT_SKIP_WS();` |
|    33 | 2360 | `			if( DT_LOWEQ("seconds",7) )     { iTs += v;            z += 7; }` |
|    33 | 2361 | `			else if( DT_LOWEQ("second",6) ) { iTs += v;            z += 6; }` |
|    33 | 2362 | `			else if( DT_LOWEQ("secs",4) )   { iTs += v;            z += 4; }` |
|    33 | 2363 | `			else if( DT_LOWEQ("sec",3) )    { iTs += v;            z += 3; }` |
|    33 | 2364 | `			else if( DT_LOWEQ("minutes",7) ){ iTs += v*60;         z += 7; }` |
|    31 | 2365 | `			else if( DT_LOWEQ("minute",6) ) { iTs += v*60;         z += 6; }` |
|    31 | 2366 | `			else if( DT_LOWEQ("mins",4) )   { iTs += v*60;         z += 4; }` |
|    31 | 2367 | `			else if( DT_LOWEQ("min",3) )    { iTs += v*60;         z += 3; }` |
|    31 | 2368 | `			else if( DT_LOWEQ("hours",5) )  { iTs += v*3600;       z += 5; }` |
|    29 | 2369 | `			else if( DT_LOWEQ("hour",4) )   { iTs += v*3600;       z += 4; }` |
|    29 | 2370 | `			else if( DT_LOWEQ("days",4) )   { iTs += v*86400;      z += 4; }` |
|    29 | 2371 | `			else if( DT_LOWEQ("day",3) )    { iTs += v*86400;      z += 3; }` |
|    23 | 2372 | `			else if( DT_LOWEQ("weeks",5) )  { iTs += v*7*86400;    z += 5; }` |
|    21 | 2373 | `			else if( DT_LOWEQ("week",4) )   { iTs += v*7*86400;    z += 4; }` |
|    19 | 2374 | `			else if( DT_LOWEQ("fortnights",10) ){ iTs += v*14*86400; z += 10; }` |
|    17 | 2375 | `			else if( DT_LOWEQ("fortnight",9) )  { iTs += v*14*86400; z += 9; }` |
|    17 | 2376 | `			else if( DT_LOWEQ("months",6) ) { iTs = DtAddMonths(iTs,iOff,v); z += 6; }` |
|    15 | 2377 | `			else if( DT_LOWEQ("month",5) )  { iTs = DtAddMonths(iTs,iOff,v); z += 5; }` |
|     9 | 2378 | `			else if( DT_LOWEQ("years",5) )  { iTs = DtAddMonths(iTs,iOff,v*12); z += 5; }` |
|     9 | 2379 | `			else if( DT_LOWEQ("year",4) )   { iTs = DtAddMonths(iTs,iOff,v*12); z += 4; }` |
|     - | 2380 | `			else{` |
|     7 | 2381 | `				return (int)(z - zIn) + 1;` |
|     - | 2382 | `			}` |
|    27 | 2383 | `			bAny = 1;` |
|    27 | 2384 | `			continue;` |
|     - | 2385 | `		}` |
|    13 | 2386 | `		return (int)(z - zIn) + 1;` |
|   ! 0 | 2387 | `	}` |
|   317 | 2388 | `	if( !bAny ){` |
|   ! 0 | 2389 | `		return 1;` |
|     - | 2390 | `	}` |
|   317 | 2391 | `	*pTs = iTs;` |
|   317 | 2392 | `	*pOff = iOff;` |
|   317 | 2393 | `	*pbOffSet = bOffSet;` |
|   317 | 2394 | `	*pUs = uSec;` |
|   317 | 2395 | `	return 0;` |
|     - | 2396 | `#undef DT_SKIP_WS` |
|     - | 2397 | `#undef DT_LOWEQ` |
|   222 | 2398 | `}` |
|     - | 2399 | `/* int __dt_now() */` |
|   224 | 2400 | `static int vm_builtin_dt_now(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2401 | `{` |
|   112 | 2402 | `	SXUNUSED(nArg);` |
|   112 | 2403 | `	SXUNUSED(apArg);` |
|   225 | 2404 | `	ph7_result_int64(pCtx,(ph7_int64)time(0));` |
|   225 | 2405 | `	return PH7_OK;` |
|     1 | 2406 | `}` |
|     - | 2407 | `/* mixed __dt_parse(string $s, int $baseTs, int $baseOff)` |
|     - | 2408 | ` *   -> [ts, off, offWasExplicit] on success; php's error MESSAGE string on` |
|     - | 2409 | ` *      failure (the chunk wraps it in DateMalformedStringException). */` |
|   442 | 2410 | `static int vm_builtin_dt_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2411 | `{` |
|     - | 2412 | `	const char *zIn;` |
|     - | 2413 | `	int nLen;` |
|     - | 2414 | `	sxi64 iBaseTs;` |
|     - | 2415 | `	sxi32 iBaseOff;` |
|   443 | 2416 | `	sxi64 iTs = 0;` |
|   443 | 2417 | `	sxi32 iOff = 0;` |
|   443 | 2418 | `	int bOffSet = 0;` |
|   443 | 2419 | `	int uSec = 0;` |
|     - | 2420 | `	int iErrPos;` |
|   443 | 2421 | `	if( nArg < 3 ){` |
|   ! 0 | 2422 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2423 | `		return PH7_OK;` |
|     - | 2424 | `	}` |
|   443 | 2425 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   443 | 2426 | `	iBaseTs  = ph7_value_to_int64(apArg[1]);` |
|   443 | 2427 | `	iBaseOff = (sxi32)ph7_value_to_int64(apArg[2]);` |
|   443 | 2428 | `	iErrPos = DtParse(zIn,nLen,iBaseTs,iBaseOff,&iTs,&iOff,&bOffSet,&uSec);` |
|   443 | 2429 | `	if( iErrPos != 0 ){` |
|     - | 2430 | `		/* Negative encoding: php's "Double time specification" reason */` |
|    51 | 2431 | `		int bDouble = iErrPos < 0;` |
|    51 | 2432 | `		int iPos = (bDouble ? -iErrPos : iErrPos) - 1;` |
|    51 | 2433 | `		char cAt = (iPos < nLen) ? zIn[iPos] : ' ';` |
|     - | 2434 | `		/* php appends a reason: an alphabetic token is assumed to be a timezone` |
|     - | 2435 | `		 * lookup miss, anything else an unexpected character. */` |
|   100 | 2436 | `		ph7_result_string_format(pCtx,` |
|     - | 2437 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    25 | 2438 | `			nLen,zIn,iPos,cAt,` |
|    49 | 2439 | `			bDouble ? "Double time specification"` |
|    48 | 2440 | `			: ((cAt >= 'a' && cAt <= 'z') \|\| (cAt >= 'A' && cAt <= 'Z'))` |
|     - | 2441 | `				? "The timezone could not be found in the database"` |
|    48 | 2442 | `				: "Unexpected character");` |
|    51 | 2443 | `		return PH7_OK;` |
|     - | 2444 | `	}` |
|     - | 2445 | `	{` |
|   393 | 2446 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|   393 | 2447 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|   393 | 2448 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2449 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2450 | `		}` |
|   393 | 2451 | `		ph7_value_int64(pV,iTs);` |
|   393 | 2452 | `		ph7_array_add_elem(pArr,0,pV);` |
|   393 | 2453 | `		ph7_value_int64(pV,iOff);` |
|   393 | 2454 | `		ph7_array_add_elem(pArr,0,pV);` |
|     - | 2455 | `		/* int, not bool: 0 = no explicit offset, 1 = numeric offset/@epoch,` |
|     - | 2456 | `		 * 2 = literal "Z" (php keeps the distinction in the zone name) */` |
|   393 | 2457 | `		ph7_value_int64(pV,bOffSet);` |
|   393 | 2458 | `		ph7_array_add_elem(pArr,0,pV);` |
|     - | 2459 | `		/* [3] = microseconds parsed from a fractional-seconds part (0 when absent) */` |
|   393 | 2460 | `		ph7_value_int64(pV,uSec);` |
|   393 | 2461 | `		ph7_array_add_elem(pArr,0,pV);` |
|   393 | 2462 | `		ph7_result_value(pCtx,pArr);` |
|     - | 2463 | `	}` |
|   393 | 2464 | `	return PH7_OK;` |
|   222 | 2465 | `}` |
|     - | 2466 | `/* string __dt_default_tz(void) — the date_default_timezone_set() identifier */` |
|   224 | 2467 | `static int vm_builtin_dt_default_tz(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2468 | `{` |
|   112 | 2469 | `	SXUNUSED(nArg);` |
|   112 | 2470 | `	SXUNUSED(apArg);` |
|   225 | 2471 | `	ph7_result_string(pCtx,pCtx->pVm->zDefTz,(int)pCtx->pVm->nDefTz);` |
|   225 | 2472 | `	return PH7_OK;` |
|     1 | 2473 | `}` |
|     - | 2474 | `/* string __dt_format(int $ts, int $off, string $tzname, string $format, int $us = 0) */` |
|   174 | 2475 | `static int vm_builtin_dt_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2476 | `{` |
|     - | 2477 | `	Sytm sTm;` |
|     - | 2478 | `	sxi64 iTs;` |
|     - | 2479 | `	sxi32 iOff;` |
|     - | 2480 | `	const char *zName,*zFmt;` |
|   175 | 2481 | `	int nName,nFmt,uSec = 0;` |
|     - | 2482 | `	char zZone[64];` |
|   175 | 2483 | `	if( nArg < 4 ){` |
|   ! 0 | 2484 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2485 | `		return PH7_OK;` |
|     - | 2486 | `	}` |
|   175 | 2487 | `	iTs  = ph7_value_to_int64(apArg[0]);` |
|   175 | 2488 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|   175 | 2489 | `	zName = ph7_value_to_string(apArg[2],&nName);` |
|   175 | 2490 | `	zFmt  = ph7_value_to_string(apArg[3],&nFmt);` |
|   175 | 2491 | `	if( nArg > 4 ){ uSec = ph7_value_to_int(apArg[4]); }` |
|   175 | 2492 | `	if( nName >= (int)sizeof(zZone) ){ nName = (int)sizeof(zZone) - 1; }` |
|   175 | 2493 | `	SyMemcpy(zName,zZone,(sxu32)nName);` |
|   175 | 2494 | `	zZone[nName] = 0;` |
|   175 | 2495 | `	DtFillSytm(iTs,iOff,zZone,&sTm);` |
|   175 | 2496 | `	DateFormat(pCtx,zFmt,nFmt,&sTm,uSec);` |
|   175 | 2497 | `	return PH7_OK;` |
|    88 | 2498 | `}` |
|     - | 2499 | `/* int __dt_make(int y, int mo, int d, int h, int i, int s, int off) */` |
|     8 | 2500 | `static int vm_builtin_dt_make(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2501 | `{` |
|     - | 2502 | `	sxi64 y;` |
|     - | 2503 | `	int mo,d,h,mi,s;` |
|     - | 2504 | `	sxi32 iOff;` |
|     9 | 2505 | `	if( nArg < 7 ){` |
|   ! 0 | 2506 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2507 | `		return PH7_OK;` |
|     - | 2508 | `	}` |
|     9 | 2509 | `	y   = ph7_value_to_int64(apArg[0]);` |
|     9 | 2510 | `	mo  = ph7_value_to_int(apArg[1]);` |
|     9 | 2511 | `	d   = ph7_value_to_int(apArg[2]);` |
|     9 | 2512 | `	h   = ph7_value_to_int(apArg[3]);` |
|     9 | 2513 | `	mi  = ph7_value_to_int(apArg[4]);` |
|     9 | 2514 | `	s   = ph7_value_to_int(apArg[5]);` |
|     9 | 2515 | `	iOff = (sxi32)ph7_value_to_int64(apArg[6]);` |
|     9 | 2516 | `	ph7_result_int64(pCtx,DtMakeTs(y,mo,d,h,mi,s,iOff));` |
|     9 | 2517 | `	return PH7_OK;` |
|     5 | 2518 | `}` |
|     - | 2519 | `/* Days in a civil month (php's overflow rules use it during diff borrows) */` |
|    62 | 2520 | `static int DtDaysInMonth(sxi64 y,int m)` |
|     1 | 2521 | `{` |
|     - | 2522 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|    63 | 2523 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|     9 | 2524 | `		return 29;` |
|     - | 2525 | `	}` |
|    55 | 2526 | `	return aMonDays[(m - 1) % 12];` |
|    32 | 2527 | `}` |
|     - | 2528 | `/* int __dt_civil_add(int ts, int off, int y, int m, int d, int h, int i,` |
|     - | 2529 | ` *                    int s, int sign)` |
|     - | 2530 | ` *   php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|     - | 2531 | ` *   (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset. */` |
|    54 | 2532 | `static int vm_builtin_dt_civil_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2533 | `{` |
|     - | 2534 | `	sxi64 iTs,iLocal,iDays,iSecs,y0,moT,dayCount;` |
|     - | 2535 | `	sxi32 iOff;` |
|     - | 2536 | `	int mo0,d0,iSign;` |
|     - | 2537 | `	sxi64 y,m,d,h,i,s;` |
|    55 | 2538 | `	if( nArg < 9 ){` |
|   ! 0 | 2539 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2540 | `		return PH7_OK;` |
|     - | 2541 | `	}` |
|    55 | 2542 | `	iTs   = ph7_value_to_int64(apArg[0]);` |
|    55 | 2543 | `	iOff  = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    55 | 2544 | `	y     = ph7_value_to_int64(apArg[2]);` |
|    55 | 2545 | `	m     = ph7_value_to_int64(apArg[3]);` |
|    55 | 2546 | `	d     = ph7_value_to_int64(apArg[4]);` |
|    55 | 2547 | `	h     = ph7_value_to_int64(apArg[5]);` |
|    55 | 2548 | `	i     = ph7_value_to_int64(apArg[6]);` |
|    55 | 2549 | `	s     = ph7_value_to_int64(apArg[7]);` |
|    55 | 2550 | `	iSign = ph7_value_to_int(apArg[8]) < 0 ? -1 : 1;` |
|    55 | 2551 | `	iLocal = iTs + iOff;` |
|    55 | 2552 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    55 | 2553 | `	iSecs  = iLocal - iDays*86400;` |
|    55 | 2554 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    55 | 2555 | `	y0 += iSign * y;` |
|    55 | 2556 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    55 | 2557 | `	y0 += DtFloorDiv(moT,12);` |
|    55 | 2558 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    55 | 2559 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    55 | 2560 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    55 | 2561 | `	ph7_result_int64(pCtx,iLocal - iOff);` |
|    55 | 2562 | `	return PH7_OK;` |
|    28 | 2563 | `}` |
|     - | 2564 | `/* array __dt_civil_diff(int ts1, int off1, int ts2)` |
|     - | 2565 | ` *   -> [y,m,d,h,i,s,days,invert]: timelib's breakdown — field-wise deltas in` |
|     - | 2566 | ` *   the FIRST operand's offset, then borrow seconds→minutes→hours→days, then` |
|     - | 2567 | ` *   the day borrow walks whole months backward from the later date (that walk` |
|     - | 2568 | ` *   is why Jan 31 → Mar 02 reports m=0 d=30, not "1 month"). */` |
|    14 | 2569 | `static int vm_builtin_dt_civil_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2570 | `{` |
|     - | 2571 | `	sxi64 iTs1,iTs2,iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|     - | 2572 | `	sxi32 iOff;` |
|     - | 2573 | `	int moA,dA,moB,dB,bInvert;` |
|     - | 2574 | `	sxi64 sA,sB,y,m,d,h,i,s;` |
|     - | 2575 | `	ph7_value *pArr,*pV;` |
|    15 | 2576 | `	if( nArg < 3 ){` |
|   ! 0 | 2577 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2578 | `		return PH7_OK;` |
|     - | 2579 | `	}` |
|    15 | 2580 | `	iTs1 = ph7_value_to_int64(apArg[0]);` |
|    15 | 2581 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    15 | 2582 | `	iTs2 = ph7_value_to_int64(apArg[2]);` |
|    15 | 2583 | `	bInvert = iTs1 > iTs2;` |
|    15 | 2584 | `	iA = bInvert ? iTs2 : iTs1;` |
|    15 | 2585 | `	iB = bInvert ? iTs1 : iTs2;` |
|    15 | 2586 | `	iLa = iA + iOff;` |
|    15 | 2587 | `	iLb = iB + iOff;` |
|    15 | 2588 | `	daysA = DtFloorDiv(iLa,86400);` |
|    15 | 2589 | `	daysB = DtFloorDiv(iLb,86400);` |
|    15 | 2590 | `	sA = iLa - daysA*86400;` |
|    15 | 2591 | `	sB = iLb - daysB*86400;` |
|    15 | 2592 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|    15 | 2593 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|    15 | 2594 | `	s = (sB % 60) - (sA % 60);` |
|    15 | 2595 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|    15 | 2596 | `	h = (sB / 3600) - (sA / 3600);` |
|    15 | 2597 | `	d = dB - dA;` |
|    15 | 2598 | `	m = moB - moA;` |
|    15 | 2599 | `	y = yB - yA;` |
|    15 | 2600 | `	if( s < 0 ){ s += 60; i--; }` |
|    15 | 2601 | `	if( i < 0 ){ i += 60; h--; }` |
|    15 | 2602 | `	if( h < 0 ){ h += 24; d--; }` |
|    27 | 2603 | `	while( d < 0 ){` |
|    13 | 2604 | `		moB--;` |
|    13 | 2605 | `		if( moB < 1 ){ moB = 12; yB--; }` |
|    13 | 2606 | `		d += DtDaysInMonth(yB,moB);` |
|    13 | 2607 | `		m--;` |
|     1 | 2608 | `	}` |
|    15 | 2609 | `	if( m < 0 ){ m += 12; y--; }` |
|    15 | 2610 | `	pArr = ph7_context_new_array(pCtx);` |
|    15 | 2611 | `	pV = ph7_context_new_scalar(pCtx);` |
|    15 | 2612 | `	if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2613 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2614 | `	}` |
|    15 | 2615 | `	ph7_value_int64(pV,y);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2616 | `	ph7_value_int64(pV,m);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2617 | `	ph7_value_int64(pV,d);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2618 | `	ph7_value_int64(pV,h);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2619 | `	ph7_value_int64(pV,i);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2620 | `	ph7_value_int64(pV,s);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2621 | `	ph7_value_int64(pV,(iB - iA) / 86400); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2622 | `	ph7_value_int64(pV,bInvert); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2623 | `	ph7_result_value(pCtx,pArr);` |
|    15 | 2624 | `	return PH7_OK;` |
|     8 | 2625 | `}` |
|     - | 2626 | `/* int __dt_isodate(int ts, int off, int y, int w, int dow)` |
|     - | 2627 | ` *   setISODate: jump to ISO year/week/weekday, preserving the time of day. */` |
|     8 | 2628 | `static int vm_builtin_dt_isodate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2629 | `{` |
|     - | 2630 | `	sxi64 iTs,iLocal,iTod,jan4,monday1,target,y;` |
|     - | 2631 | `	sxi32 iOff;` |
|     - | 2632 | `	sxi64 w,dow;` |
|     - | 2633 | `	int isoDow;` |
|     9 | 2634 | `	if( nArg < 5 ){` |
|   ! 0 | 2635 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2636 | `		return PH7_OK;` |
|     - | 2637 | `	}` |
|     9 | 2638 | `	iTs = ph7_value_to_int64(apArg[0]);` |
|     9 | 2639 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|     9 | 2640 | `	y   = ph7_value_to_int64(apArg[2]);` |
|     9 | 2641 | `	w   = ph7_value_to_int64(apArg[3]);` |
|     9 | 2642 | `	dow = ph7_value_to_int64(apArg[4]);` |
|     9 | 2643 | `	iLocal = iTs + iOff;` |
|     9 | 2644 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|     9 | 2645 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|     9 | 2646 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|     9 | 2647 | `	monday1 = jan4 - (isoDow - 1);` |
|     9 | 2648 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|     9 | 2649 | `	ph7_result_int64(pCtx,target*86400 + iTod - iOff);` |
|     9 | 2650 | `	return PH7_OK;` |
|     5 | 2651 | `}` |
|     - | 2652 | `/* Consume nMin..nMax digits from *pz; returns count consumed (0 = failure) */` |
|   184 | 2653 | `static int DtEatDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2654 | `{` |
|   185 | 2655 | `	const char *z = *pz;` |
|   185 | 2656 | `	sxi64 v = 0;` |
|   185 | 2657 | `	int n = 0;` |
|   663 | 2658 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|   479 | 2659 | `		v = v*10 + (z[0] - '0');` |
|   479 | 2660 | `		z++;` |
|   479 | 2661 | `		n++;` |
|     1 | 2662 | `	}` |
|   185 | 2663 | `	if( n < nMin ){` |
|     5 | 2664 | `		return 0;` |
|     - | 2665 | `	}` |
|   181 | 2666 | `	*pz = z;` |
|   181 | 2667 | `	*pVal = v;` |
|   181 | 2668 | `	return n;` |
|    93 | 2669 | `}` |
|     - | 2670 | `/* timelib_get_nr's recovery: skip non-digits hunting for the field.` |
|     - | 2671 | ` * Returns 1 = found+read, 0 = digits present but short, -1 = exhausted. */` |
|     4 | 2672 | `static int DtHuntDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2673 | `{` |
|     5 | 2674 | `	const char *z = *pz;` |
|    29 | 2675 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|     5 | 2676 | `	*pz = z;` |
|     5 | 2677 | `	if( z >= zEnd ){` |
|     5 | 2678 | `		return -1;` |
|     - | 2679 | `	}` |
|   ! 0 | 2680 | `	return DtEatDigits(pz,zEnd,nMin,nMax,pVal) ? 1 : 0;` |
|     3 | 2681 | `}` |
|     - | 2682 | `/* Case-insensitive name-table lookup; returns 1-based index or 0 */` |
|    14 | 2683 | `static int DtEatName(const char **pz,const char *zEnd,const char **azNames,int nNames)` |
|     1 | 2684 | `{` |
|     - | 2685 | `	int k;` |
|    23 | 2686 | `	for( k = 0 ; k < nNames ; k++ ){` |
|    23 | 2687 | `		int n = (int)SyStrlen(azNames[k]);` |
|    23 | 2688 | `		if( zEnd - *pz >= n && SyStrnicmp(*pz,azNames[k],(sxu32)n) == 0 ){` |
|    15 | 2689 | `			*pz += n;` |
|    15 | 2690 | `			return k + 1;` |
|     - | 2691 | `		}` |
|     5 | 2692 | `	}` |
|   ! 0 | 2693 | `	return 0;` |
|     8 | 2694 | `}` |
|     - | 2695 | `/* mixed __dt_from_format(string fmt, string input, int nowTs, int defOff)` |
|     - | 2696 | ` *   php's DateTime::createFromFormat engine. Success: [ts, off, offKind, name]` |
|     - | 2697 | ` *   where offKind 0=none-parsed, 1=numeric offset, 2=literal Z, 3=named id.` |
|     - | 2698 | ` *   Failure: "POS\tMESSAGE" (timelib's message strings; PHL reports the FIRST` |
|     - | 2699 | ` *   error where php may accumulate several — recorded). A trailing-data` |
|     - | 2700 | ` *   warning rides as [4]=pos, [5]=msg on the success array. */` |
|    58 | 2701 | `static int vm_builtin_dt_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2702 | `{` |
|     - | 2703 | `	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};` |
|     - | 2704 | `	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",` |
|     - | 2705 | `		"thursday","friday","saturday"};` |
|     - | 2706 | `	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",` |
|     - | 2707 | `		"aug","sep","oct","nov","dec"};` |
|     - | 2708 | `	static const char *azMonFull[] = {"january","february","march","april",` |
|     - | 2709 | `		"may","june","july","august","september","october","november","december"};` |
|     - | 2710 | `	const char *zFmt,*zIn,*zEnd,*zInEnd,*z;` |
|     - | 2711 | `	int nFmt,nIn;` |
|     - | 2712 | `	sxi64 iNow,v;` |
|     - | 2713 | `	sxi32 iDefOff;` |
|     - | 2714 | `	/* -1 == unset */` |
|    59 | 2715 | `	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;` |
|    59 | 2716 | `	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;` |
|    59 | 2717 | `	int uSecFF = 0,bHasUs = 0;` |
|    59 | 2718 | `	int iOffKind = 0;` |
|    59 | 2719 | `	sxi32 iOffVal = 0;` |
|     - | 2720 | `	char zName[16];` |
|    59 | 2721 | `	const char *zErr = 0;` |
|     - | 2722 | `	const char *aWarnMsg[3];` |
|     - | 2723 | `	int aWarnPos[3];` |
|    59 | 2724 | `	int nWarn = 0,bAborted = 0;` |
|     - | 2725 | `	const char *aErrMsg[8];` |
|     - | 2726 | `	int aErrPos[8];` |
|    59 | 2727 | `	int nErr = 0,nErrKept = 0;` |
|    59 | 2728 | `	if( nArg < 4 ){` |
|   ! 0 | 2729 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2730 | `		return PH7_OK;` |
|     - | 2731 | `	}` |
|    59 | 2732 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    59 | 2733 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    59 | 2734 | `	iNow = ph7_value_to_int64(apArg[2]);` |
|    59 | 2735 | `	iDefOff = (sxi32)ph7_value_to_int64(apArg[3]);` |
|    59 | 2736 | `	zEnd = &zFmt[nFmt];` |
|    59 | 2737 | `	zInEnd = &zIn[nIn];` |
|    59 | 2738 | `	z = zIn;` |
|    59 | 2739 | `	zName[0] = 0;` |
|     - | 2740 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|     - | 2741 | `	{ int _p = (iPos),_k,_f = -1; \` |
|     - | 2742 | `	  nErr++; \` |
|     - | 2743 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|     - | 2744 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|     - | 2745 | `	  else if( nErrKept < 8 ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|   411 | 2746 | `	while( zFmt < zEnd ){` |
|   357 | 2747 | `		char c = zFmt[0];` |
|   357 | 2748 | `		zFmt++;` |
|   357 | 2749 | `		zErr = 0;` |
|   357 | 2750 | `		if( c == '!' ){` |
|    11 | 2751 | `			y = 1970; mo = 1; d = 1; h = 0; mi = 0; s = 0;` |
|    11 | 2752 | `			h12 = -1; iMeridiem = -1;` |
|    11 | 2753 | `			continue;` |
|     - | 2754 | `		}` |
|   347 | 2755 | `		if( c == '\|' ){ bPipe = 1; continue; }` |
|   343 | 2756 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|   341 | 2757 | `		if( z >= zInEnd ){` |
|     - | 2758 | `			/* timelib aborts the scan once input is exhausted */` |
|     9 | 2759 | `			DT_FF_LOGERR(nIn,"Not enough data available to satisfy format");` |
|     5 | 2760 | `			break;` |
|     - | 2761 | `		}` |
|   337 | 2762 | `		switch( c ){` |
|    21 | 2763 | `		case 'd': case 'j':` |
|    43 | 2764 | `			if( !DtEatDigits(&z,zInEnd,1,2,&d) ){` |
|   ! 0 | 2765 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit day could not be found");` |
|   ! 0 | 2766 | `				if( DtHuntDigits(&z,zInEnd,1,2,&d) < 0 ){` |
|   ! 0 | 2767 | `					DT_FF_LOGERR(nIn,"A two digit day could not be found");` |
|   ! 0 | 2768 | `				}` |
|   ! 0 | 2769 | `			}` |
|    43 | 2770 | `			break;` |
|     1 | 2771 | `		case 'D':` |
|     3 | 2772 | `			if( !DtEatName(&z,zInEnd,azDay3,7) ){` |
|   ! 0 | 2773 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2774 | `			}` |
|     3 | 2775 | `			break;` |
|     1 | 2776 | `		case 'l':` |
|     3 | 2777 | `			if( !DtEatName(&z,zInEnd,azDayFull,7) ){` |
|   ! 0 | 2778 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2779 | `			}` |
|     3 | 2780 | `			break;` |
|     1 | 2781 | `		case 'S':` |
|     - | 2782 | `			/* ordinal suffix: st nd rd th */` |
|     4 | 2783 | `			if( zInEnd-z >= 2 && ((z[0]=='s'&&z[1]=='t')\|\|(z[0]=='n'&&z[1]=='d')` |
|     2 | 2784 | `			 \|\|(z[0]=='r'&&z[1]=='d')\|\|(z[0]=='t'&&z[1]=='h')) ){` |
|     3 | 2785 | `				z += 2;` |
|     1 | 2786 | `			}` |
|     3 | 2787 | `			break;` |
|    19 | 2788 | `		case 'm': case 'n':` |
|    39 | 2789 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mo) ){` |
|   ! 0 | 2790 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit month could not be found");` |
|   ! 0 | 2791 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mo) < 0 ){` |
|   ! 0 | 2792 | `					DT_FF_LOGERR(nIn,"A two digit month could not be found");` |
|   ! 0 | 2793 | `				}` |
|   ! 0 | 2794 | `			}` |
|    39 | 2795 | `			break;` |
|     1 | 2796 | `		case 'M':{` |
|     3 | 2797 | `			int k = DtEatName(&z,zInEnd,azMon3,12);` |
|     3 | 2798 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2799 | `			break;` |
|     - | 2800 | `				 }` |
|     1 | 2801 | `		case 'F':{` |
|     3 | 2802 | `			int k = DtEatName(&z,zInEnd,azMonFull,12);` |
|     3 | 2803 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2804 | `			break;` |
|     - | 2805 | `				 }` |
|   ! 0 | 2806 | `		case 'y':` |
|   ! 0 | 2807 | `			if( DtEatDigits(&z,zInEnd,2,2,&y) ){` |
|   ! 0 | 2808 | `				y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2809 | `			}else{` |
|   ! 0 | 2810 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit year could not be found");` |
|   ! 0 | 2811 | `				if( DtHuntDigits(&z,zInEnd,2,2,&y) < 0 ){` |
|   ! 0 | 2812 | `					DT_FF_LOGERR(nIn,"A two digit year could not be found");` |
|   ! 0 | 2813 | `				}else if( y >= 0 ){` |
|   ! 0 | 2814 | `					y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2815 | `				}` |
|     - | 2816 | `			}` |
|   ! 0 | 2817 | `			break;` |
|    24 | 2818 | `		case 'Y':{` |
|    49 | 2819 | `			int neg = 0;` |
|    49 | 2820 | `			if( z < zInEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|    49 | 2821 | `			if( DtEatDigits(&z,zInEnd,1,4,&y) ){` |
|    45 | 2822 | `				if( neg ){ y = -y; }` |
|    23 | 2823 | `			}else{` |
|     5 | 2824 | `				DT_FF_LOGERR((int)(z - zIn),"A four digit year could not be found");` |
|     5 | 2825 | `				if( DtHuntDigits(&z,zInEnd,1,4,&y) < 0 ){` |
|     9 | 2826 | `					DT_FF_LOGERR(nIn,"A four digit year could not be found");` |
|     2 | 2827 | `				}` |
|     - | 2828 | `			}` |
|    49 | 2829 | `			break;` |
|     - | 2830 | `				 }` |
|     7 | 2831 | `		case 'H': case 'G':` |
|    15 | 2832 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h) ){` |
|   ! 0 | 2833 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2834 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h) < 0 ){` |
|   ! 0 | 2835 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2836 | `				}` |
|   ! 0 | 2837 | `			}` |
|    15 | 2838 | `			break;` |
|     2 | 2839 | `		case 'h': case 'g':` |
|     5 | 2840 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h12) ){` |
|   ! 0 | 2841 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2842 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h12) < 0 ){` |
|   ! 0 | 2843 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2844 | `				}` |
|   ! 0 | 2845 | `			}` |
|     5 | 2846 | `			break;` |
|     9 | 2847 | `		case 'i':` |
|    19 | 2848 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mi) ){` |
|   ! 0 | 2849 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit minute could not be found");` |
|   ! 0 | 2850 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mi) < 0 ){` |
|   ! 0 | 2851 | `					DT_FF_LOGERR(nIn,"A two digit minute could not be found");` |
|   ! 0 | 2852 | `				}` |
|   ! 0 | 2853 | `			}` |
|    19 | 2854 | `			break;` |
|     3 | 2855 | `		case 's':` |
|     7 | 2856 | `			if( !DtEatDigits(&z,zInEnd,1,2,&s) ){` |
|   ! 0 | 2857 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit second could not be found");` |
|   ! 0 | 2858 | `				if( DtHuntDigits(&z,zInEnd,1,2,&s) < 0 ){` |
|   ! 0 | 2859 | `					DT_FF_LOGERR(nIn,"A two digit second could not be found");` |
|   ! 0 | 2860 | `				}` |
|   ! 0 | 2861 | `			}` |
|     7 | 2862 | `			break;` |
|     1 | 2863 | `		case 'u':{` |
|     - | 2864 | `			/* Microseconds: the digits parsed are right-padded to 6 (".5" -> 500000). */` |
|     3 | 2865 | `			const char *zStart = z;` |
|     3 | 2866 | `			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){` |
|   ! 0 | 2867 | `				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");` |
|   ! 0 | 2868 | `				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){` |
|   ! 0 | 2869 | `					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");` |
|   ! 0 | 2870 | `				}else{` |
|   ! 0 | 2871 | `					zStart = z; /* HuntDigits repositioned; treat as freshly read */` |
|     - | 2872 | `				}` |
|   ! 0 | 2873 | `			}` |
|     - | 2874 | `			{` |
|     3 | 2875 | `				int nd = (int)(z - zStart);` |
|     3 | 2876 | `				while( nd > 0 && nd < 6 ){ v *= 10; nd++; }` |
|     3 | 2877 | `				uSecFF = (int)v; bHasUs = 1;` |
|     - | 2878 | `			}` |
|     3 | 2879 | `			break;` |
|     - | 2880 | `				 }` |
|   ! 0 | 2881 | `		case 'v':{` |
|   ! 0 | 2882 | `			const char *zStart = z;` |
|   ! 0 | 2883 | `			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){` |
|   ! 0 | 2884 | `				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");` |
|   ! 0 | 2885 | `				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){` |
|   ! 0 | 2886 | `					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");` |
|   ! 0 | 2887 | `				}else{` |
|   ! 0 | 2888 | `					zStart = z;` |
|     - | 2889 | `				}` |
|   ! 0 | 2890 | `			}` |
|     - | 2891 | `			{` |
|   ! 0 | 2892 | `				int nd = (int)(z - zStart);` |
|   ! 0 | 2893 | `				while( nd > 0 && nd < 3 ){ v *= 10; nd++; }` |
|   ! 0 | 2894 | `				uSecFF = (int)v * 1000; bHasUs = 1; /* ms -> us */` |
|     - | 2895 | `			}` |
|   ! 0 | 2896 | `			break;` |
|     - | 2897 | `				 }` |
|     2 | 2898 | `		case 'a': case 'A':{` |
|     - | 2899 | `			static const char *azMer[] = {"am","pm","a.m.","p.m."};` |
|     5 | 2900 | `			int k = DtEatName(&z,zInEnd,azMer,4);` |
|     5 | 2901 | `			if( k ){` |
|     5 | 2902 | `				iMeridiem = ((k - 1) & 1);` |
|     3 | 2903 | `			}else{` |
|   ! 0 | 2904 | `				zErr = "A meridian could not be found";` |
|     - | 2905 | `			}` |
|     5 | 2906 | `			break;` |
|     - | 2907 | `				 }` |
|     2 | 2908 | `		case 'U':{` |
|     5 | 2909 | `			int neg = 0;` |
|     5 | 2910 | `			if( z < zInEnd && z[0]=='-' ){ neg = 1; z++; }` |
|     5 | 2911 | `			if( DtEatDigits(&z,zInEnd,1,19,&uVal) ){` |
|     5 | 2912 | `				if( neg ){ uVal = -uVal; }` |
|     5 | 2913 | `				bHasU = 1;` |
|     3 | 2914 | `			}else{` |
|   ! 0 | 2915 | `				DT_FF_LOGERR((int)(z - zIn),"A unix timestamp could not be found");` |
|   ! 0 | 2916 | `				if( DtHuntDigits(&z,zInEnd,1,19,&uVal) < 0 ){` |
|   ! 0 | 2917 | `					DT_FF_LOGERR(nIn,"A unix timestamp could not be found");` |
|   ! 0 | 2918 | `				}else{` |
|   ! 0 | 2919 | `					if( neg ){ uVal = -uVal; }` |
|   ! 0 | 2920 | `					bHasU = 1;` |
|     - | 2921 | `				}` |
|     - | 2922 | `			}` |
|     5 | 2923 | `			break;` |
|     - | 2924 | `				 }` |
|     1 | 2925 | `		case 'e': case 'T':{` |
|     - | 2926 | `			static const char *azZone[] = {"UTC","GMT","Z"};` |
|     3 | 2927 | `			int k = DtEatName(&z,zInEnd,azZone,3);` |
|     3 | 2928 | `			if( k == 3 ){` |
|   ! 0 | 2929 | `				iOffKind = 2; iOffVal = 0;` |
|     3 | 2930 | `			}else if( k ){` |
|     3 | 2931 | `				iOffKind = 3; iOffVal = 0;` |
|     3 | 2932 | `				SyMemcpy(azZone[k-1],zName,4);` |
|     1 | 2933 | `			}else if( z < zInEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|   ! 0 | 2934 | `				goto parse_num_off;` |
|   ! 0 | 2935 | `			}else{` |
|   ! 0 | 2936 | `				zErr = "The timezone could not be found in the database";` |
|     - | 2937 | `			}` |
|     3 | 2938 | `			break;` |
|     2 | 2939 | `				 }` |
|     - | 2940 | `		case 'O': case 'P':` |
|     2 | 2941 | `parse_num_off:	{` |
|     5 | 2942 | `			int sign,oh,om = 0;` |
|     - | 2943 | `			sxi64 t;` |
|     5 | 2944 | `			if( z >= zInEnd \|\| (z[0] != '+' && z[0] != '-') ){` |
|   ! 0 | 2945 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2946 | `				break;` |
|     - | 2947 | `			}` |
|     5 | 2948 | `			sign = (z[0]=='-') ? -1 : 1;` |
|     5 | 2949 | `			z++;` |
|     5 | 2950 | `			if( !DtEatDigits(&z,zInEnd,2,2,&t) ){` |
|   ! 0 | 2951 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2952 | `				break;` |
|     - | 2953 | `			}` |
|     5 | 2954 | `			oh = (int)t;` |
|     5 | 2955 | `			if( z < zInEnd && z[0]==':' ){ z++; }` |
|     5 | 2956 | `			if( DtEatDigits(&z,zInEnd,2,2,&t) ){ om = (int)t; }` |
|     5 | 2957 | `			iOffKind = 1;` |
|     5 | 2958 | `			iOffVal = sign * (oh*3600 + om*60);` |
|     5 | 2959 | `			break;` |
|     - | 2960 | `				 }` |
|   ! 0 | 2961 | `		case '?':` |
|   ! 0 | 2962 | `			if( z < zInEnd ){ z++; }` |
|   ! 0 | 2963 | `			break;` |
|   ! 0 | 2964 | `		case '*':` |
|     - | 2965 | `			/* skip input until the next separator byte */` |
|   ! 0 | 2966 | `			while( z < zInEnd && !SyisDigit(z[0]) && z[0] != ';' && z[0] != ':'` |
|   ! 0 | 2967 | `			 && z[0] != '/' && z[0] != '.' && z[0] != ',' && z[0] != '-'` |
|   ! 0 | 2968 | `			 && z[0] != '(' && z[0] != ')' && z[0] != ' ' ){` |
|   ! 0 | 2969 | `				z++;` |
|   ! 0 | 2970 | `			}` |
|   ! 0 | 2971 | `			break;` |
|     1 | 2972 | `		case '#':` |
|     3 | 2973 | `			if( z < zInEnd && (z[0]==';'\|\|z[0]==':'\|\|z[0]=='/'\|\|z[0]=='.'` |
|   ! 0 | 2974 | `			 \|\|z[0]==','\|\|z[0]=='-'\|\|z[0]=='('\|\|z[0]==')') ){` |
|     3 | 2975 | `				z++;` |
|     2 | 2976 | `			}else{` |
|   ! 0 | 2977 | `				zErr = "The separation symbol could not be found";` |
|     - | 2978 | `			}` |
|     3 | 2979 | `			break;` |
|     1 | 2980 | `		case '\\':` |
|     3 | 2981 | `			if( zFmt < zEnd ){` |
|     3 | 2982 | `				if( z < zInEnd && z[0] == zFmt[0] ){` |
|     3 | 2983 | `					z++;` |
|     3 | 2984 | `					zFmt++;` |
|     2 | 2985 | `				}else{` |
|     - | 2986 | `					/* a literal mismatch aborts timelib's scan */` |
|   ! 0 | 2987 | `					DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|   ! 0 | 2988 | `					zFmt = zEnd;` |
|   ! 0 | 2989 | `					bAborted = 1;` |
|     - | 2990 | `				}` |
|     1 | 2991 | `			}` |
|     3 | 2992 | `			break;` |
|    51 | 2993 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|     - | 2994 | `		case '(' : case ')':` |
|   103 | 2995 | `			if( z < zInEnd && z[0] == c ){` |
|   103 | 2996 | `				z++;` |
|    52 | 2997 | `			}else{` |
|     - | 2998 | `				/* timelib logs BOTH messages (count +2, last-wins on the` |
|     - | 2999 | `				 * position), consumes the offending byte, and keeps going */` |
|   ! 0 | 3000 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 3001 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 3002 | `				z++;` |
|     - | 3003 | `			}` |
|   103 | 3004 | `			break;` |
|    16 | 3005 | `		case ' ':` |
|    33 | 3006 | `			if( z < zInEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|    33 | 3007 | `				z++;` |
|    17 | 3008 | `			}else{` |
|   ! 0 | 3009 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 3010 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 3011 | `				z++;` |
|     - | 3012 | `			}` |
|    33 | 3013 | `			break;` |
|     1 | 3014 | `		default:` |
|     - | 3015 | `			/* any other format byte must match the input verbatim; a mismatch` |
|     - | 3016 | `			 * aborts timelib's scan */` |
|     3 | 3017 | `			if( z < zInEnd && z[0] == c ){` |
|   ! 0 | 3018 | `				z++;` |
|   ! 0 | 3019 | `			}else{` |
|     3 | 3020 | `				DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|     3 | 3021 | `				zFmt = zEnd;` |
|     3 | 3022 | `				bAborted = 1;` |
|     - | 3023 | `			}` |
|     2 | 3024 | `			break;` |
|     - | 3025 | `		}` |
|   337 | 3026 | `		if( zErr ){` |
|     - | 3027 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|   ! 0 | 3028 | `			DT_FF_LOGERR((int)(z - zIn),zErr);` |
|   ! 0 | 3029 | `		}` |
|     1 | 3030 | `	}` |
|    59 | 3031 | `	if( z < zInEnd && !bAborted ){` |
|     5 | 3032 | `		if( bPlus ){` |
|     - | 3033 | `			/* '+' downgrades trailing data to a warning */` |
|     3 | 3034 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     3 | 3035 | `			aWarnMsg[nWarn] = "Trailing data";` |
|     3 | 3036 | `			nWarn++;` |
|     2 | 3037 | `		}else{` |
|     3 | 3038 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|     - | 3039 | `		}` |
|     2 | 3040 | `	}` |
|    59 | 3041 | `	if( nErr > 0 ){` |
|     - | 3042 | `		SyBlob sOut;` |
|     - | 3043 | `		int k;` |
|     9 | 3044 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     9 | 3045 | `		SyBlobFormat(&sOut,"%d",nErr);` |
|    21 | 3046 | `		for( k = 0 ; k < nErrKept ; k++ ){` |
|    13 | 3047 | `			SyBlobFormat(&sOut,"\n%d\t%s",aErrPos[k],aErrMsg[k]);` |
|     7 | 3048 | `		}` |
|     9 | 3049 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     9 | 3050 | `		SyBlobRelease(&sOut);` |
|     9 | 3051 | `		return PH7_OK;` |
|     - | 3052 | `	}` |
|    51 | 3053 | `	if( bPipe ){` |
|     5 | 3054 | `		if( y < 0 ){ y = 1970; }` |
|     5 | 3055 | `		if( mo < 0 ){ mo = 1; }` |
|     5 | 3056 | `		if( d < 0 ){ d = 1; }` |
|     5 | 3057 | `		if( h < 0 && h12 < 0 ){ h = 0; }` |
|     5 | 3058 | `		if( mi < 0 ){ mi = 0; }` |
|     5 | 3059 | `		if( s < 0 ){ s = 0; }` |
|     2 | 3060 | `	}` |
|     - | 3061 | `	{` |
|     - | 3062 | `		/* remaining unset fields come from "now" in the default offset */` |
|    51 | 3063 | `		sxi64 iLocal = iNow + iDefOff;` |
|    51 | 3064 | `		sxi64 days = DtFloorDiv(iLocal,86400);` |
|    51 | 3065 | `		sxi64 secs = iLocal - days*86400;` |
|     - | 3066 | `		sxi64 ny;` |
|     - | 3067 | `		int nmo,nd;` |
|    51 | 3068 | `		DtCivilFromDays(days,&ny,&nmo,&nd);` |
|    51 | 3069 | `		if( y < 0 ){ y = ny; }` |
|    51 | 3070 | `		if( mo < 0 ){ mo = nmo; }` |
|    51 | 3071 | `		if( d < 0 ){ d = nd; }` |
|    51 | 3072 | `		if( h12 >= 0 ){` |
|     5 | 3073 | `			h = (h12 % 12) + ((iMeridiem == 1) ? 12 : 0);` |
|     2 | 3074 | `		}` |
|     - | 3075 | `		/* php: parsing a time component zeroes the finer unset units */` |
|    51 | 3076 | `		if( h >= 0 ){` |
|    27 | 3077 | `			if( mi < 0 ){ mi = 0; }` |
|    27 | 3078 | `			if( s < 0 ){ s = 0; }` |
|    38 | 3079 | `		}else if( mi >= 0 ){` |
|     3 | 3080 | `			if( s < 0 ){ s = 0; }` |
|     1 | 3081 | `		}` |
|    51 | 3082 | `		if( h < 0 ){ h = secs / 3600; }` |
|    51 | 3083 | `		if( mi < 0 ){ mi = (secs / 60) % 60; }` |
|    51 | 3084 | `		if( s < 0 ){ s = secs % 60; }` |
|     - | 3085 | `	}` |
|     - | 3086 | `	/* php validates the RESOLVED fields and warns (parse still succeeds,` |
|     - | 3087 | `	 * values roll over via civil arithmetic) */` |
|    51 | 3088 | `	if( mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo) ){` |
|     3 | 3089 | `		if( nWarn < 3 ){` |
|     3 | 3090 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 3091 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|     3 | 3092 | `			nWarn++;` |
|     1 | 3093 | `		}` |
|     1 | 3094 | `	}` |
|    51 | 3095 | `	if( h > 24 \|\| mi > 59 \|\| s > 59 ){` |
|     3 | 3096 | `		if( nWarn < 3 ){` |
|     3 | 3097 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 3098 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|     3 | 3099 | `			nWarn++;` |
|     1 | 3100 | `		}` |
|     1 | 3101 | `	}` |
|     - | 3102 | `	{` |
|    51 | 3103 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    51 | 3104 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|     - | 3105 | `		sxi64 iTs;` |
|    51 | 3106 | `		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;` |
|    51 | 3107 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 3108 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 3109 | `		}` |
|    51 | 3110 | `		if( bHasU ){` |
|     5 | 3111 | `			iTs = uVal;` |
|     5 | 3112 | `			iUseOff = 0;` |
|     5 | 3113 | `			iOffKind = 1;` |
|     3 | 3114 | `		}else{` |
|    47 | 3115 | `			iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);` |
|     - | 3116 | `		}` |
|    51 | 3117 | `		ph7_value_int64(pV,iTs);           ph7_array_add_elem(pArr,0,pV);` |
|    51 | 3118 | `		ph7_value_int64(pV,iUseOff);       ph7_array_add_elem(pArr,0,pV);` |
|    51 | 3119 | `		ph7_value_int64(pV,iOffKind);      ph7_array_add_elem(pArr,0,pV);` |
|    51 | 3120 | `		ph7_value_string(pV,zName,-1);     ph7_array_add_elem(pArr,0,pV);` |
|     - | 3121 | `		/* microseconds from a u/v token ride an associative key so they never` |
|     - | 3122 | `		 * collide with the numeric [4+] trailing-warning pairs */` |
|    51 | 3123 | `		if( bHasUs ){ ph7_value_int64(pV,uSecFF); ph7_array_add_strkey_elem(pArr,"us",pV); }` |
|     - | 3124 | `		{` |
|     - | 3125 | `			int k;` |
|    57 | 3126 | `			for( k = 0 ; k < nWarn ; k++ ){` |
|     7 | 3127 | `				ph7_value_int64(pV,aWarnPos[k]);` |
|     7 | 3128 | `				ph7_array_add_elem(pArr,0,pV);` |
|     7 | 3129 | `				ph7_value_string(pV,aWarnMsg[k],-1);` |
|     7 | 3130 | `				ph7_array_add_elem(pArr,0,pV);` |
|     4 | 3131 | `			}` |
|     - | 3132 | `		}` |
|    51 | 3133 | `		ph7_result_value(pCtx,pArr);` |
|     - | 3134 | `	}` |
|    51 | 3135 | `	return PH7_OK;` |
|    30 | 3136 | `}` |
|     - | 3137 | `/*` |
|     - | 3138 | ` * The embedded DateTime library. Timezone scope: UTC + fixed offsets.` |
|     - | 3139 | ` */` |
|     - | 3140 | `static const char zDateTimeLib[] =` |
|     - | 3141 | `"class DateException extends Exception {}"` |
|     - | 3142 | `"class DateMalformedStringException extends DateException {}"` |
|     - | 3143 | `"class DateInvalidTimeZoneException extends DateException {}"` |
|     - | 3144 | `"class DateMalformedIntervalStringException extends DateException {}"` |
|     - | 3145 | `"class DateMalformedPeriodStringException extends DateException {}"` |
|     - | 3146 | `"interface DateTimeInterface {"` |
|     - | 3147 | `" const ATOM = 'Y-m-d\\TH:i:sP';"` |
|     - | 3148 | `" const COOKIE = 'l, d-M-Y H:i:s T';"` |
|     - | 3149 | `" const ISO8601 = 'Y-m-d\\TH:i:sO';"` |
|     - | 3150 | `" const ISO8601_EXPANDED = 'X-m-d\\TH:i:sP';"` |
|     - | 3151 | `" const RFC822 = 'D, d M y H:i:s O';"` |
|     - | 3152 | `" const RFC850 = 'l, d-M-y H:i:s T';"` |
|     - | 3153 | `" const RFC1036 = 'D, d M y H:i:s O';"` |
|     - | 3154 | `" const RFC1123 = 'D, d M Y H:i:s O';"` |
|     - | 3155 | `" const RFC7231 = 'D, d M Y H:i:s \\G\\M\\T';"` |
|     - | 3156 | `" const RFC2822 = 'D, d M Y H:i:s O';"` |
|     - | 3157 | `" const RFC3339 = 'Y-m-d\\TH:i:sP';"` |
|     - | 3158 | `" const RFC3339_EXTENDED = 'Y-m-d\\TH:i:s.vP';"` |
|     - | 3159 | `" const RSS = 'D, d M Y H:i:s O';"` |
|     - | 3160 | `" const W3C = 'Y-m-d\\TH:i:sP';"` |
|     - | 3161 | `"}"` |
|     - | 3162 | `"class DateTimeZone {"` |
|     - | 3163 | `" private $__dtzOff = 0;"` |
|     - | 3164 | `" private $__dtzName = 'UTC';"` |
|     - | 3165 | `" public function __construct($timezone = 'UTC'){"` |
|     - | 3166 | `"  $tz = (string)$timezone;"` |
|     - | 3167 | `"  if( strcasecmp($tz, 'UTC') === 0 ){"` |
|     - | 3168 | `"   $this->__dtzOff = 0; $this->__dtzName = 'UTC';"` |
|     - | 3169 | `"   return;"` |
|     - | 3170 | `"  }"` |
|     - | 3171 | `"  if( $tz === 'Z' ){"` |
|     - | 3172 | `"   $this->__dtzOff = 0; $this->__dtzName = 'Z';"` |
|     - | 3173 | `"   return;"` |
|     - | 3174 | `"  }"` |
|     - | 3175 | `"  if( strcasecmp($tz, 'GMT') === 0 ){"` |
|     - | 3176 | `"   $this->__dtzOff = 0; $this->__dtzName = 'GMT';"` |
|     - | 3177 | `"   return;"` |
|     - | 3178 | `"  }"` |
|     - | 3179 | `"  $m = null;"` |
|     - | 3180 | `"  if( preg_match('/^([+-])(\\d{2}):?(\\d{2})$/', $tz, $m) ){"` |
|     - | 3181 | `"   $off = ((int)$m[2]) * 3600 + ((int)$m[3]) * 60;"` |
|     - | 3182 | `"   if( $m[1] === '-' ){ $off = -$off; }"` |
|     - | 3183 | `"   $this->__dtzOff = $off;"` |
|     - | 3184 | `"   $this->__dtzName = $m[1] . $m[2] . ':' . $m[3];"` |
|     - | 3185 | `"   return;"` |
|     - | 3186 | `"  }"` |
|     - | 3187 | `"  throw new DateInvalidTimeZoneException("` |
|     - | 3188 | `"   'DateTimeZone::__construct(): Unknown or bad timezone (' . $tz . ')');"` |
|     - | 3189 | `" }"` |
|     - | 3190 | `" public function getName(){ return $this->__dtzName; }"` |
|     - | 3191 | `" public function getOffset($datetime = null){ return $this->__dtzOff; }"` |
|     - | 3192 | `"}"` |
|     - | 3193 | `"trait __DtCoreT {"` |
|     - | 3194 | `" private $__dtTs = 0;"` |
|     - | 3195 | `" private $__dtOff = 0;"` |
|     - | 3196 | `" private $__dtName = 'UTC';"` |
|     - | 3197 | `" private $__dtUs = 0;"` |
|     - | 3198 | `" private function __dtInit($datetime, $timezone){"` |
|     - | 3199 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 3200 | `"  if( $timezone !== null ){"` |
|     - | 3201 | `"   $off = $timezone->getOffset($this);"` |
|     - | 3202 | `"   $name = $timezone->getName();"` |
|     - | 3203 | `"  }"` |
|     - | 3204 | `"  $r = __dt_parse((string)$datetime, __dt_now(), $off);"` |
|     - | 3205 | `"  if( is_string($r) ){ throw new DateMalformedStringException($r); }"` |
|     - | 3206 | `"  $this->__dtTs = $r[0];"` |
|     - | 3207 | `"  $this->__dtUs = $r[3];"` |
|     - | 3208 | `"  if( $r[2] ){"` |
|     - | 3209 | `"   $this->__dtOff = $r[1];"` |
|     - | 3210 | `"   $this->__dtName = $r[2] === 2 ? 'Z' : $this->__dtOffName($r[1]);"` |
|     - | 3211 | `"  }else{"` |
|     - | 3212 | `"   $this->__dtOff = $off;"` |
|     - | 3213 | `"   $this->__dtName = $name;"` |
|     - | 3214 | `"  }"` |
|     - | 3215 | `" }"` |
|     - | 3216 | `" private function __dtOffName($off){"` |
|     - | 3217 | `"  $s = $off < 0 ? '-' : '+';"` |
|     - | 3218 | `"  $a = $off < 0 ? -$off : $off;"` |
|     - | 3219 | `"  return $s . sprintf('%02d:%02d', intdiv($a, 3600), intdiv($a % 3600, 60));"` |
|     - | 3220 | `" }"` |
|     - | 3221 | `" public function format($format){ return __dt_format($this->__dtTs, $this->__dtOff, $this->__dtName, (string)$format, $this->__dtUs); }"` |
|     - | 3222 | `" public function getTimestamp(){ return $this->__dtTs; }"` |
|     - | 3223 | `" public function getOffset(){ return $this->__dtOff; }"` |
|     - | 3224 | `" public function getTimezone(){ return new DateTimeZone($this->__dtName); }"` |
|     - | 3225 | `" public function diff($targetObject, $absolute = false){"` |
|     - | 3226 | `"  $r = __dt_civil_diff($this->__dtTs, $this->__dtOff, $targetObject->getTimestamp());"` |
|     - | 3227 | `"  $iv = new DateInterval('P0D');"` |
|     - | 3228 | `"  $iv->y = $r[0]; $iv->m = $r[1]; $iv->d = $r[2];"` |
|     - | 3229 | `"  $iv->h = $r[3]; $iv->i = $r[4]; $iv->s = $r[5];"` |
|     - | 3230 | `"  $iv->days = $r[6];"` |
|     - | 3231 | `"  $iv->invert = $absolute ? 0 : $r[7];"` |
|     - | 3232 | `"  return $iv;"` |
|     - | 3233 | `" }"` |
|     - | 3234 | `" private function __dtAddTs($interval, $sign){"` |
|     - | 3235 | `"  if( $interval->invert ){ $sign = -$sign; }"` |
|     - | 3236 | `"  return __dt_civil_add($this->__dtTs, $this->__dtOff, $interval->y, $interval->m,"` |
|     - | 3237 | `"   $interval->d, $interval->h, $interval->i, $interval->s, $sign);"` |
|     - | 3238 | `" }"` |
|     - | 3239 | `" private static function __dtFromFormat($format, $datetime, $timezone, $class){"` |
|     - | 3240 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 3241 | `"  if( $timezone !== null ){"` |
|     - | 3242 | `"   $off = $timezone->getOffset(null);"` |
|     - | 3243 | `"   $name = $timezone->getName();"` |
|     - | 3244 | `"  }"` |
|     - | 3245 | `"  $r = __dt_from_format((string)$format, (string)$datetime, __dt_now(), $off);"` |
|     - | 3246 | `"  if( is_string($r) ){"` |
|     - | 3247 | `"   $lines = explode(\"\\n\", $r);"` |
|     - | 3248 | `"   $errs = [];"` |
|     - | 3249 | `"   $nl = count($lines);"` |
|     - | 3250 | `"   for( $k = 1; $k < $nl; $k++ ){"` |
|     - | 3251 | `"    $p = strpos($lines[$k], \"\\t\");"` |
|     - | 3252 | `"    $errs[(int)substr($lines[$k], 0, $p)] = substr($lines[$k], $p + 1);"` |
|     - | 3253 | `"   }"` |
|     - | 3254 | `"   DateTime::$__dtLastErr = ['warning_count' => 0, 'warnings' => [],"` |
|     - | 3255 | `"    'error_count' => (int)$lines[0], 'errors' => $errs];"` |
|     - | 3256 | `"   return false;"` |
|     - | 3257 | `"  }"` |
|     - | 3258 | `"  if( isset($r[4]) ){"` |
|     - | 3259 | `"   $warns = [];"` |
|     - | 3260 | `"   $wc = 0;"` |
|     - | 3261 | `"   for( $k = 4; isset($r[$k]); $k += 2 ){"` |
|     - | 3262 | `"    $warns[$r[$k]] = $r[$k + 1];"` |
|     - | 3263 | `"    $wc++;"` |
|     - | 3264 | `"   }"` |
|     - | 3265 | `"   DateTime::$__dtLastErr = ['warning_count' => $wc, 'warnings' => $warns,"` |
|     - | 3266 | `"    'error_count' => 0, 'errors' => []];"` |
|     - | 3267 | `"  }else{"` |
|     - | 3268 | `"   DateTime::$__dtLastErr = false;"` |
|     - | 3269 | `"  }"` |
|     - | 3270 | `"  $obj = new $class('@0');"` |
|     - | 3271 | `"  $obj->__dtTs = $r[0];"` |
|     - | 3272 | `"  $obj->__dtUs = $r['us'] ?? 0;"` |
|     - | 3273 | `"  if( $r[2] === 0 ){ $obj->__dtOff = $off; $obj->__dtName = $name; }"` |
|     - | 3274 | `"  elseif( $r[2] === 2 ){ $obj->__dtOff = 0; $obj->__dtName = 'Z'; }"` |
|     - | 3275 | `"  elseif( $r[2] === 3 ){ $obj->__dtOff = $r[1]; $obj->__dtName = $r[3]; }"` |
|     - | 3276 | `"  else { $obj->__dtOff = $r[1]; $obj->__dtName = $obj->__dtOffName($r[1]); }"` |
|     - | 3277 | `"  return $obj;"` |
|     - | 3278 | `" }"` |
|     - | 3279 | `" private static function __dtCopyOf($object, $class){"` |
|     - | 3280 | `"  $d = new $class('@0');"` |
|     - | 3281 | `"  $d->__dtTs = $object->getTimestamp();"` |
|     - | 3282 | `"  $d->__dtOff = $object->getOffset();"` |
|     - | 3283 | `"  $d->__dtName = $object->getTimezone()->getName();"` |
|     - | 3284 | `"  return $d;"` |
|     - | 3285 | `" }"` |
|     - | 3286 | `"}"` |
|     - | 3287 | `"class DateTime implements DateTimeInterface {"` |
|     - | 3288 | `" use __DtCoreT;"` |
|     - | 3289 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 3290 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 3291 | `" }"` |
|     - | 3292 | `" public function modify($modifier){"` |
|     - | 3293 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 3294 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTime::modify(): ' . $r); }"` |
|     - | 3295 | `"  $this->__dtTs = $r[0];"` |
|     - | 3296 | `"  return $this;"` |
|     - | 3297 | `" }"` |
|     - | 3298 | `" public function setTimestamp($timestamp){ $this->__dtTs = (int)$timestamp; $this->__dtUs = 0; return $this; }"` |
|     - | 3299 | `" public function setTimezone($timezone){"` |
|     - | 3300 | `"  $this->__dtOff = $timezone->getOffset($this);"` |
|     - | 3301 | `"  $this->__dtName = $timezone->getName();"` |
|     - | 3302 | `"  return $this;"` |
|     - | 3303 | `" }"` |
|     - | 3304 | `" public function setDate($year, $month, $day){"` |
|     - | 3305 | `"  $this->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 3306 | `"  return $this;"` |
|     - | 3307 | `" }"` |
|     - | 3308 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3309 | `"  $this->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 3310 | `"  $this->__dtUs = (int)$microsecond;"` |
|     - | 3311 | `"  return $this;"` |
|     - | 3312 | `" }"` |
|     - | 3313 | `" public function add($interval){ $this->__dtTs = $this->__dtAddTs($interval, 1); return $this; }"` |
|     - | 3314 | `" public function sub($interval){ $this->__dtTs = $this->__dtAddTs($interval, -1); return $this; }"` |
|     - | 3315 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 3316 | `"  $this->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 3317 | `"  return $this;"` |
|     - | 3318 | `" }"` |
|     - | 3319 | `" public static $__dtLastErr = false;"` |
|     - | 3320 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 3321 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 3322 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTime');"` |
|     - | 3323 | `" }"` |
|     - | 3324 | `" public static function createFromImmutable($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 3325 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 3326 | `"}"` |
|     - | 3327 | `"class DateTimeImmutable implements DateTimeInterface {"` |
|     - | 3328 | `" use __DtCoreT;"` |
|     - | 3329 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 3330 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 3331 | `" }"` |
|     - | 3332 | `" public function modify($modifier){"` |
|     - | 3333 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 3334 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTimeImmutable::modify(): ' . $r); }"` |
|     - | 3335 | `"  $c = clone $this;"` |
|     - | 3336 | `"  $c->__dtTs = $r[0];"` |
|     - | 3337 | `"  return $c;"` |
|     - | 3338 | `" }"` |
|     - | 3339 | `" public function setTimestamp($timestamp){ $c = clone $this; $c->__dtTs = (int)$timestamp; $c->__dtUs = 0; return $c; }"` |
|     - | 3340 | `" public function setTimezone($timezone){"` |
|     - | 3341 | `"  $c = clone $this;"` |
|     - | 3342 | `"  $c->__dtOff = $timezone->getOffset($this);"` |
|     - | 3343 | `"  $c->__dtName = $timezone->getName();"` |
|     - | 3344 | `"  return $c;"` |
|     - | 3345 | `" }"` |
|     - | 3346 | `" public function setDate($year, $month, $day){"` |
|     - | 3347 | `"  $c = clone $this;"` |
|     - | 3348 | `"  $c->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 3349 | `"  return $c;"` |
|     - | 3350 | `" }"` |
|     - | 3351 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3352 | `"  $c = clone $this;"` |
|     - | 3353 | `"  $c->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 3354 | `"  $c->__dtUs = (int)$microsecond;"` |
|     - | 3355 | `"  return $c;"` |
|     - | 3356 | `" }"` |
|     - | 3357 | `" public function add($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, 1); return $c; }"` |
|     - | 3358 | `" public function sub($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, -1); return $c; }"` |
|     - | 3359 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 3360 | `"  $c = clone $this;"` |
|     - | 3361 | `"  $c->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 3362 | `"  return $c;"` |
|     - | 3363 | `" }"` |
|     - | 3364 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 3365 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 3366 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTimeImmutable');"` |
|     - | 3367 | `" }"` |
|     - | 3368 | `" public static function createFromMutable($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 3369 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 3370 | `"}"` |
|     - | 3371 | `"function date_create($datetime = 'now', $timezone = null){"` |
|     - | 3372 | `" try { return new DateTime($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 3373 | `"}"` |
|     - | 3374 | `"function date_create_immutable($datetime = 'now', $timezone = null){"` |
|     - | 3375 | `" try { return new DateTimeImmutable($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 3376 | `"}"` |
|     - | 3377 | `/* Procedural aliases of the createFromFormat statics: same (format, datetime,` |
|     - | 3378 | ` * ?timezone) order, returning false on failure like php. */` |
|     - | 3379 | `"function date_create_from_format($format, $datetime, $timezone = null){"` |
|     - | 3380 | `" return DateTime::createFromFormat($format, $datetime, $timezone);"` |
|     - | 3381 | `"}"` |
|     - | 3382 | `"function date_create_immutable_from_format($format, $datetime, $timezone = null){"` |
|     - | 3383 | `" return DateTimeImmutable::createFromFormat($format, $datetime, $timezone);"` |
|     - | 3384 | `"}"` |
|     - | 3385 | `"class DateInterval {"` |
|     - | 3386 | `" public $y = 0;"` |
|     - | 3387 | `" public $m = 0;"` |
|     - | 3388 | `" public $d = 0;"` |
|     - | 3389 | `" public $h = 0;"` |
|     - | 3390 | `" public $i = 0;"` |
|     - | 3391 | `" public $s = 0;"` |
|     - | 3392 | `" public $f = 0;"` |
|     - | 3393 | `" public $invert = 0;"` |
|     - | 3394 | `" public $days = false;"` |
|     - | 3395 | `" public $from_string = false;"` |
|     - | 3396 | `" public function __construct($duration = 'P0D'){"` |
|     - | 3397 | `"  $dur = (string)$duration;"` |
|     - | 3398 | `"  $mm = null;"` |
|     - | 3399 | `"  if( strlen($dur) < 2 \|\| substr($dur, -1) === 'T'"` |
|     - | 3400 | `"   \|\| !preg_match('/^P(?:(\\d+)Y)?(?:(\\d+)M)?(?:(\\d+)W)?(?:(\\d+)D)?(?:T(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?)?$/', $dur, $mm) ){"` |
|     - | 3401 | `"   throw new DateMalformedIntervalStringException('Unknown or bad format (' . $dur . ')');"` |
|     - | 3402 | `"  }"` |
|     - | 3403 | `"  $this->y = (int)($mm[1] ?? 0);"` |
|     - | 3404 | `"  $this->m = (int)($mm[2] ?? 0);"` |
|     - | 3405 | `"  $this->d = (int)($mm[4] ?? 0) + 7 * (int)($mm[3] ?? 0);"` |
|     - | 3406 | `"  $this->h = (int)($mm[5] ?? 0);"` |
|     - | 3407 | `"  $this->i = (int)($mm[6] ?? 0);"` |
|     - | 3408 | `"  $this->s = (int)($mm[7] ?? 0);"` |
|     - | 3409 | `" }"` |
|     - | 3410 | `" public static function createFromDateString($datetime){"` |
|     - | 3411 | `"  $s = trim((string)$datetime);"` |
|     - | 3412 | `"  $iv = new DateInterval('P0D');"` |
|     - | 3413 | `"  $rest = $s;"` |
|     - | 3414 | `"  $any = false;"` |
|     - | 3415 | `"  while( $rest !== '' ){"` |
|     - | 3416 | `"   $mm = null;"` |
|     - | 3417 | `"   if( !preg_match('/^[\\s,+]*([+-]?\\d+)\\s*(sec\|secs\|second\|seconds\|min\|mins\|minute\|minutes\|hour\|hours\|day\|days\|week\|weeks\|fortnight\|fortnights\|month\|months\|year\|years)\\b/i', $rest, $mm) ){"` |
|     - | 3418 | `"    throw new DateMalformedIntervalStringException("` |
|     - | 3419 | `"     'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 3420 | `"   }"` |
|     - | 3421 | `"   $n = (int)$mm[1];"` |
|     - | 3422 | `"   $u = strtolower($mm[2]);"` |
|     - | 3423 | `"   if( $u === 'sec' \|\| $u === 'secs' \|\| $u === 'second' \|\| $u === 'seconds' ){ $iv->s += $n; }"` |
|     - | 3424 | `"   elseif( $u === 'min' \|\| $u === 'mins' \|\| $u === 'minute' \|\| $u === 'minutes' ){ $iv->i += $n; }"` |
|     - | 3425 | `"   elseif( $u === 'hour' \|\| $u === 'hours' ){ $iv->h += $n; }"` |
|     - | 3426 | `"   elseif( $u === 'day' \|\| $u === 'days' ){ $iv->d += $n; }"` |
|     - | 3427 | `"   elseif( $u === 'week' \|\| $u === 'weeks' ){ $iv->d += 7 * $n; }"` |
|     - | 3428 | `"   elseif( $u === 'fortnight' \|\| $u === 'fortnights' ){ $iv->d += 14 * $n; }"` |
|     - | 3429 | `"   elseif( $u === 'month' \|\| $u === 'months' ){ $iv->m += $n; }"` |
|     - | 3430 | `"   else { $iv->y += $n; }"` |
|     - | 3431 | `"   $any = true;"` |
|     - | 3432 | `"   $rest = ltrim(substr($rest, strlen($mm[0])));"` |
|     - | 3433 | `"  }"` |
|     - | 3434 | `"  if( !$any ){"` |
|     - | 3435 | `"   throw new DateMalformedIntervalStringException("` |
|     - | 3436 | `"    'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 3437 | `"  }"` |
|     - | 3438 | `"  return $iv;"` |
|     - | 3439 | `" }"` |
|     - | 3440 | `" public function format($format){"` |
|     - | 3441 | `"  $f = (string)$format;"` |
|     - | 3442 | `"  $out = '';"` |
|     - | 3443 | `"  $n = strlen($f);"` |
|     - | 3444 | `"  for( $k = 0; $k < $n; $k++ ){"` |
|     - | 3445 | `"   $c = $f[$k];"` |
|     - | 3446 | `"   if( $c !== '%' ){ $out .= $c; continue; }"` |
|     - | 3447 | `"   $k++;"` |
|     - | 3448 | `"   if( $k >= $n ){ $out .= '%'; break; }"` |
|     - | 3449 | `"   $t = $f[$k];"` |
|     - | 3450 | `"   if( $t === 'Y' ){ $out .= sprintf('%02d', $this->y); }"` |
|     - | 3451 | `"   elseif( $t === 'y' ){ $out .= $this->y; }"` |
|     - | 3452 | `"   elseif( $t === 'M' ){ $out .= sprintf('%02d', $this->m); }"` |
|     - | 3453 | `"   elseif( $t === 'm' ){ $out .= $this->m; }"` |
|     - | 3454 | `"   elseif( $t === 'D' ){ $out .= sprintf('%02d', $this->d); }"` |
|     - | 3455 | `"   elseif( $t === 'd' ){ $out .= $this->d; }"` |
|     - | 3456 | `"   elseif( $t === 'H' ){ $out .= sprintf('%02d', $this->h); }"` |
|     - | 3457 | `"   elseif( $t === 'h' ){ $out .= $this->h; }"` |
|     - | 3458 | `"   elseif( $t === 'I' ){ $out .= sprintf('%02d', $this->i); }"` |
|     - | 3459 | `"   elseif( $t === 'i' ){ $out .= $this->i; }"` |
|     - | 3460 | `"   elseif( $t === 'S' ){ $out .= sprintf('%02d', $this->s); }"` |
|     - | 3461 | `"   elseif( $t === 's' ){ $out .= $this->s; }"` |
|     - | 3462 | `"   elseif( $t === 'F' ){ $out .= sprintf('%06d', (int)round($this->f * 1000000)); }"` |
|     - | 3463 | `"   elseif( $t === 'f' ){ $out .= (int)round($this->f * 1000000); }"` |
|     - | 3464 | `"   elseif( $t === 'R' ){ $out .= $this->invert ? '-' : '+'; }"` |
|     - | 3465 | `"   elseif( $t === 'r' ){ $out .= $this->invert ? '-' : ''; }"` |
|     - | 3466 | `"   elseif( $t === 'a' ){ $out .= $this->days === false ? '(unknown)' : $this->days; }"` |
|     - | 3467 | `"   elseif( $t === '%' ){ $out .= '%'; }"` |
|     - | 3468 | `"   else { $out .= $t; }"` |
|     - | 3469 | `"  }"` |
|     - | 3470 | `"  return $out;"` |
|     - | 3471 | `" }"` |
|     - | 3472 | `"}"` |
|     - | 3473 | `"class DatePeriod implements IteratorAggregate {"` |
|     - | 3474 | `" const EXCLUDE_START_DATE = 1;"` |
|     - | 3475 | `" const INCLUDE_END_DATE = 2;"` |
|     - | 3476 | `" public $start = null;"` |
|     - | 3477 | `" public $current = null;"` |
|     - | 3478 | `" public $end = null;"` |
|     - | 3479 | `" public $interval = null;"` |
|     - | 3480 | `" public $recurrences = 1;"` |
|     - | 3481 | `" public $include_start_date = true;"` |
|     - | 3482 | `" public $include_end_date = false;"` |
|     - | 3483 | `" private $__dpN = null;"` |
|     - | 3484 | `" public function __construct($start, $interval = null, $end = null, $options = 0){"` |
|     - | 3485 | `"  if( is_string($start) ){"` |
|     - | 3486 | `"   $mm = null;"` |
|     - | 3487 | `"   if( !preg_match('/^R(\\d+)\\/(.+)\\/(P.+)$/', $start, $mm) ){"` |
|     - | 3488 | `"    throw new DateMalformedPeriodStringException("` |
|     - | 3489 | `"     'DatePeriod::__construct(): Unknown or bad format (' . $start . ')');"` |
|     - | 3490 | `"   }"` |
|     - | 3491 | `"   $options = is_int($interval) ? $interval : 0;"` |
|     - | 3492 | `"   $this->start = new DateTimeImmutable($mm[2]);"` |
|     - | 3493 | `"   $this->interval = new DateInterval($mm[3]);"` |
|     - | 3494 | `"   $this->__dpN = (int)$mm[1];"` |
|     - | 3495 | `"   $this->recurrences = $this->__dpN + 1;"` |
|     - | 3496 | `"  }else{"` |
|     - | 3497 | `"   $this->start = clone $start;"` |
|     - | 3498 | `"   $this->interval = $interval;"` |
|     - | 3499 | `"   if( is_int($end) ){"` |
|     - | 3500 | `"    $this->__dpN = $end;"` |
|     - | 3501 | `"    $this->recurrences = $end + 1;"` |
|     - | 3502 | `"   }else{"` |
|     - | 3503 | `"    $this->end = $end === null ? null : (clone $end);"` |
|     - | 3504 | `"   }"` |
|     - | 3505 | `"  }"` |
|     - | 3506 | `"  $this->include_start_date = !((int)$options & 1);"` |
|     - | 3507 | `"  $this->include_end_date = ((int)$options & 2) !== 0;"` |
|     - | 3508 | `" }"` |
|     - | 3509 | `" public static function createFromISO8601String($specification, $options = 0){"` |
|     - | 3510 | `"  return new DatePeriod((string)$specification, (int)$options);"` |
|     - | 3511 | `" }"` |
|     - | 3512 | `" public function getStartDate(){ return $this->start; }"` |
|     - | 3513 | `" public function getEndDate(){ return $this->end; }"` |
|     - | 3514 | `" public function getDateInterval(){ return $this->interval; }"` |
|     - | 3515 | `" public function getRecurrences(){ return $this->__dpN; }"` |
|     - | 3516 | `" public function getIterator(): Generator {"` |
|     - | 3517 | `"  $cur = $this->start;"` |
|     - | 3518 | `"  $iv = $this->interval;"` |
|     - | 3519 | `"  $k = 0;"` |
|     - | 3520 | `"  if( $this->end !== null ){"` |
|     - | 3521 | `"   $endTs = $this->end->getTimestamp();"` |
|     - | 3522 | `"   $first = true;"` |
|     - | 3523 | `"   while( true ){"` |
|     - | 3524 | `"    $ts = $cur->getTimestamp();"` |
|     - | 3525 | `"    if( $this->include_end_date ? ($ts > $endTs) : ($ts >= $endTs) ){ break; }"` |
|     - | 3526 | `"    if( !$first \|\| $this->include_start_date ){"` |
|     - | 3527 | `"     yield $k => (clone $cur);"` |
|     - | 3528 | `"     $k++;"` |
|     - | 3529 | `"    }"` |
|     - | 3530 | `"    $first = false;"` |
|     - | 3531 | `"    $next = clone $cur;"` |
|     - | 3532 | `"    $cur = $next->add($iv);"` |
|     - | 3533 | `"   }"` |
|     - | 3534 | `"   return;"` |
|     - | 3535 | `"  }"` |
|     - | 3536 | `"  $total = $this->__dpN + 1 + ($this->include_end_date ? 1 : 0);"` |
|     - | 3537 | `"  for( $j = 0; $j < $total; $j++ ){"` |
|     - | 3538 | `"   if( $j > 0 \|\| $this->include_start_date ){"` |
|     - | 3539 | `"    yield $k => (clone $cur);"` |
|     - | 3540 | `"    $k++;"` |
|     - | 3541 | `"   }"` |
|     - | 3542 | `"   $next = clone $cur;"` |
|     - | 3543 | `"   $cur = $next->add($iv);"` |
|     - | 3544 | `"  }"` |
|     - | 3545 | `" }"` |
|     - | 3546 | `"}"` |
|     - | 3547 | `"function date_format($object, $format){ return $object->format($format); }"` |
|     - | 3548 | `"function date_modify($object, $modifier){"` |
|     - | 3549 | `" try { return $object->modify($modifier); } catch (Exception $e) { return false; }"` |
|     - | 3550 | `"}"` |
|     - | 3551 | `"function date_add($object, $interval){ return $object->add($interval); }"` |
|     - | 3552 | `"function date_sub($object, $interval){ return $object->sub($interval); }"` |
|     - | 3553 | `"function date_diff($baseObject, $targetObject, $absolute = false){"` |
|     - | 3554 | `" return $baseObject->diff($targetObject, $absolute);"` |
|     - | 3555 | `"}"` |
|     - | 3556 | `"function date_timestamp_get($object){ return $object->getTimestamp(); }"` |
|     - | 3557 | `"function date_timestamp_set($object, $timestamp){ return $object->setTimestamp($timestamp); }"` |
|     - | 3558 | `"function date_timezone_get($object){ return $object->getTimezone(); }"` |
|     - | 3559 | `"function date_timezone_set($object, $timezone){ return $object->setTimezone($timezone); }"` |
|     - | 3560 | `"function date_offset_get($object){ return $object->getOffset(); }"` |
|     - | 3561 | `"function date_date_set($object, $year, $month, $day){ return $object->setDate($year, $month, $day); }"` |
|     - | 3562 | `"function date_time_set($object, $hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3563 | `" return $object->setTime($hour, $minute, $second, $microsecond);"` |
|     - | 3564 | `"}"` |
|     - | 3565 | `"function date_isodate_set($object, $year, $week, $dayOfWeek = 1){"` |
|     - | 3566 | `" return $object->setISODate($year, $week, $dayOfWeek);"` |
|     - | 3567 | `"}"` |
|     - | 3568 | `"function date_interval_create_from_date_string($datetime){"` |
|     - | 3569 | `" return DateInterval::createFromDateString($datetime);"` |
|     - | 3570 | `"}"` |
|     - | 3571 | `"function date_interval_format($object, $format){ return $object->format($format); }"` |
|     - | 3572 | `"function date_get_last_errors(){ return DateTime::getLastErrors(); }"` |
|     - | 3573 | `"function timezone_open($timezone){"` |
|     - | 3574 | `" try { return new DateTimeZone($timezone); } catch (Exception $e) { return false; }"` |
|     - | 3575 | `"}"` |
|     - | 3576 | `"function timezone_name_get($object){ return $object->getName(); }"` |
|     - | 3577 | `"function timezone_offset_get($object, $datetime){ return $object->getOffset($datetime); }"` |
|     - | 3578 | `/* int\|false strtotime(string $datetime, ?int $baseTimestamp = null). Rides the` |
|     - | 3579 | ` * same DtParse the DateTime constructor uses, so its format coverage is identical.` |
|     - | 3580 | ` * php: the EMPTY string is false, but whitespace-only is 'now'; a parse failure is` |
|     - | 3581 | ` * false (never an exception). The default timezone is treated as offset 0, exactly` |
|     - | 3582 | ` * as the DateTime constructor does for a null $timezone. */` |
|     - | 3583 | `"function strtotime($datetime, $baseTimestamp = null){"` |
|     - | 3584 | `" $s = (string)$datetime;"` |
|     - | 3585 | `" if( $s === '' ){ return false; }"` |
|     - | 3586 | `" $base = $baseTimestamp === null ? __dt_now() : (int)$baseTimestamp;"` |
|     - | 3587 | `" $r = __dt_parse($s, $base, 0);"` |
|     - | 3588 | `" return is_string($r) ? false : $r[0];"` |
|     - | 3589 | `"}"` |
|     - | 3590 | `;` |
|     - | 3591 | `/*` |
|     - | 3592 | ` * Install the DateTime family: thunks first, then the chunk. Called from` |
|     - | 3593 | ` * PH7_VmInit inside the bCompilingBuiltin window, after the Reflection` |
|     - | 3594 | ` * install (Exception must exist).` |
|     - | 3595 | ` */` |
|  3812 | 3596 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
|     5 | 3597 | `{` |
|     - | 3598 | `	static const struct {` |
|     - | 3599 | `		const char *zName;` |
|     - | 3600 | `		ProchHostFunction xFunc;` |
|     - | 3601 | `	} aFunc[] = {` |
|     - | 3602 | `		{ "__dt_now",    vm_builtin_dt_now },` |
|     - | 3603 | `		{ "__dt_default_tz", vm_builtin_dt_default_tz },` |
|     - | 3604 | `		{ "__dt_civil_add",  vm_builtin_dt_civil_add },` |
|     - | 3605 | `		{ "__dt_civil_diff", vm_builtin_dt_civil_diff },` |
|     - | 3606 | `		{ "__dt_isodate",    vm_builtin_dt_isodate },` |
|     - | 3607 | `		{ "__dt_from_format", vm_builtin_dt_from_format },` |
|     - | 3608 | `		{ "__dt_parse",  vm_builtin_dt_parse },` |
|     - | 3609 | `		{ "__dt_format", vm_builtin_dt_format },` |
|     - | 3610 | `		{ "__dt_make",   vm_builtin_dt_make },` |
|     - | 3611 | `	};` |
|     - | 3612 | `	sxu32 n;` |
|     - | 3613 | `	/* php's date.timezone default */` |
|  3817 | 3614 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|  3817 | 3615 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
| 38125 | 3616 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 34313 | 3617 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 17159 | 3618 | `	}` |
|  3817 | 3619 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zDateTimeLib,sizeof(zDateTimeLib)-1);` |
|     5 | 3620 | `}` |
|     - | 3621 |  |
|     - | 3622 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 3623 |  |
|     - | 3624 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 3625 | `/* Tiny build: no DateTime family (builtin layer disabled) */` |
|     - | 3626 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){` |
|     - | 3627 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|     - | 3628 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|     - | 3629 | `	return SXRET_OK;` |
|     - | 3630 | `}` |
|     - | 3631 | `#endif` |
|     - | 3632 |  |
