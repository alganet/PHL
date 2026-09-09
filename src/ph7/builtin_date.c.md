# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1310/1720 lines (76.16%)

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
|    10 | 1652 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1653 | `{` |
|    11 | 1654 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1655 | `	const char *zId;` |
|     - | 1656 | `	int nId;` |
|    11 | 1657 | `	if( nArg < 1 ){` |
|   ! 0 | 1658 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1659 | `		return PH7_OK;` |
|     - | 1660 | `	}` |
|    11 | 1661 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|    11 | 1662 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|    11 | 1663 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|    11 | 1664 | `		pVm->zDefTz[3] = 0;` |
|    11 | 1665 | `		pVm->nDefTz = 3;` |
|    11 | 1666 | `		ph7_result_bool(pCtx,1);` |
|    11 | 1667 | `		return PH7_OK;` |
|     - | 1668 | `	}` |
|     - | 1669 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|     - | 1670 | `	 * — exactly php's notice shape here */` |
|   ! 0 | 1671 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|   ! 0 | 1672 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 1673 | `	return PH7_OK;` |
|     6 | 1674 | `}` |
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
|   534 | 1690 | `static sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|     1 | 1691 | `{` |
|     - | 1692 | `	sxi64 era;` |
|     - | 1693 | `	unsigned yoe,doy,doe;` |
|   535 | 1694 | `	y -= (m <= 2);` |
|   535 | 1695 | `	era = (y >= 0 ? y : y - 399) / 400;` |
|   535 | 1696 | `	yoe = (unsigned)(y - era * 400);` |
|   535 | 1697 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|   535 | 1698 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|   535 | 1699 | `	return era * 146097 + (sxi64)doe - 719468;` |
|     1 | 1700 | `}` |
|   260 | 1701 | `static void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|     1 | 1702 | `{` |
|     - | 1703 | `	sxi64 era;` |
|     - | 1704 | `	unsigned doe,yoe,doy,mp;` |
|   261 | 1705 | `	z += 719468;` |
|   261 | 1706 | `	era = (z >= 0 ? z : z - 146096) / 146097;` |
|   261 | 1707 | `	doe = (unsigned)(z - era * 146097);` |
|   261 | 1708 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|   261 | 1709 | `	*py = (sxi64)yoe + era * 400;` |
|   261 | 1710 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|   261 | 1711 | `	mp = (5 * doy + 2) / 153;` |
|   261 | 1712 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|   261 | 1713 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|   261 | 1714 | `	if( *pm <= 2 ){` |
|   157 | 1715 | `		*py += 1;` |
|    78 | 1716 | `	}` |
|   261 | 1717 | `}` |
|   466 | 1718 | `static sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|     1 | 1719 | `{` |
|   467 | 1720 | `	sxi64 q = a / b;` |
|   467 | 1721 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|     3 | 1722 | `		q--;` |
|     1 | 1723 | `	}` |
|   467 | 1724 | `	return q;` |
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
|   150 | 1747 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|     1 | 1748 | `{` |
|   151 | 1749 | `	return DtDaysFromCivil(y,mo,d) * 86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
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
|   118 | 1776 | `static int DtTimeSuffix(const char **pz,const char *zEnd,const char *zIn,` |
|     - | 1777 | `	int *ph,int *pmi,int *ps,sxi32 *piOff,int *pbOffSet)` |
|     1 | 1778 | `{` |
|   119 | 1779 | `	const char *z = *pz;` |
|   118 | 1780 | `	if( z < zEnd && (z[0]=='T' \|\| z[0]==' ') && zEnd-z >= 6` |
|    51 | 1781 | `	 && SyisDigit(z[1]) && SyisDigit(z[2]) && z[3]==':' ){` |
|    49 | 1782 | `		z++;` |
|    49 | 1783 | `		*ph  = (z[0]-'0')*10 + (z[1]-'0');` |
|    49 | 1784 | `		*pmi = (z[3]-'0')*10 + (z[4]-'0');` |
|     - | 1785 | `		/* a 25+ hour kills php's whole time token: error at its start */` |
|    49 | 1786 | `		if( *ph > 24 ){ return (int)(z - zIn) + 1; }` |
|     - | 1787 | `		/* php lexes HH:M, then the minute's second digit starts a SECOND time` |
|     - | 1788 | `		 * token: "Double time specification" (negative encoding) */` |
|    47 | 1789 | `		if( *pmi > 59 ){ return -((int)(&z[4] - zIn) + 1); }` |
|    45 | 1790 | `		z += 5;` |
|    45 | 1791 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|    45 | 1792 | `			*ps = (z[1]-'0')*10 + (z[2]-'0');` |
|    45 | 1793 | `			if( *ps > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|    43 | 1794 | `			z += 3;` |
|    21 | 1795 | `		}` |
|    43 | 1796 | `		if( z < zEnd && z[0]=='.' ){ /* fractional seconds: consume */` |
|   ! 0 | 1797 | `			z++;` |
|   ! 0 | 1798 | `			while( z < zEnd && SyisDigit(z[0]) ){ z++; }` |
|   ! 0 | 1799 | `		}` |
|    43 | 1800 | `		if( z < zEnd && (z[0]=='Z' \|\| z[0]=='z') ){` |
|     7 | 1801 | `			*piOff = 0; *pbOffSet = 2; z++;` |
|    40 | 1802 | `		}else if( z < zEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
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
|    21 | 1817 | `	}` |
|   113 | 1818 | `	*pz = z;` |
|   113 | 1819 | `	return 0;` |
|    60 | 1820 | `}` |
|     - | 1821 | `/*` |
|     - | 1822 | ` * Read one or two decimal digits at z (z<zEnd guaranteed by caller for the first).` |
|     - | 1823 | ` * Returns the value; *pn = digits consumed (1 or 2).` |
|     - | 1824 | ` */` |
|    56 | 1825 | `static int DtRead1or2(const char *z,const char *zEnd,int *pn)` |
|     1 | 1826 | `{` |
|    57 | 1827 | `	int v = z[0]-'0';` |
|    57 | 1828 | `	if( z+1 < zEnd && SyisDigit(z[1]) ){ v = v*10 + (z[1]-'0'); *pn = 2; }` |
|     7 | 1829 | `	else { *pn = 1; }` |
|    57 | 1830 | `	return v;` |
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
|    66 | 1847 | `static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,` |
|     - | 1848 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn)` |
|     1 | 1849 | `{` |
|     - | 1850 | `	int a,b,c,na,nb,nc;` |
|     - | 1851 | `	char sep;` |
|    67 | 1852 | `	int y,mo,d,h = 0,mi = 0,s = 0;` |
|    67 | 1853 | `	sxi32 iOff = *pOff;` |
|     - | 1854 | `	int rcT;` |
|     - | 1855 | `	/* first field: 1-4 digits */` |
|    67 | 1856 | `	if( !SyisDigit(z[0]) ){ return 0; }` |
|    67 | 1857 | `	a = 0; na = 0;` |
|   215 | 1858 | `	while( z < zEnd && SyisDigit(z[0]) && na < 4 ){ a = a*10 + (z[0]-'0'); z++; na++; }` |
|    67 | 1859 | `	if( z >= zEnd \|\| (z[0] != '-' && z[0] != '/' && z[0] != '.') ){ return 0; }` |
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
|    34 | 1909 | `}` |
|     - | 1910 | `/*` |
|     - | 1911 | ` * Minimal php-datetime-string parser (slice 1): absolute forms` |
|     - | 1912 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]",` |
|     - | 1913 | ` * keywords today/midnight/noon/tomorrow/yesterday, and relative sequences` |
|     - | 1914 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". Returns 0 on` |
|     - | 1915 | ` * success (ts/off/bOffSet out), or the byte position of the first` |
|     - | 1916 | ` * unparseable character +1 (for php's "at position N" message).` |
|     - | 1917 | ` */` |
|   256 | 1918 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,` |
|     - | 1919 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet)` |
|     1 | 1920 | `{` |
|   257 | 1921 | `	const char *z = zIn, *zEnd = &zIn[nLen];` |
|   257 | 1922 | `	sxi64 iTs = iBaseTs;` |
|   257 | 1923 | `	sxi32 iOff = iBaseOff;` |
|   257 | 1924 | `	int bOffSet = 0;` |
|   257 | 1925 | `	int bAny = 0;` |
|     - | 1926 | `	int iNumRc;` |
|     - | 1927 | `#define DT_SKIP_WS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|     - | 1928 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|     - | 1929 | `	&& (zEnd-z == (nKw) \|\| !SyisAlpha(z[(nKw)])))` |
|   391 | 1930 | `	DT_SKIP_WS();` |
|   257 | 1931 | `	if( z >= zEnd ){` |
|     - | 1932 | `		/* php: the empty string is "now" */` |
|     3 | 1933 | `		*pTs = iTs;` |
|     3 | 1934 | `		*pOff = iOff;` |
|     3 | 1935 | `		*pbOffSet = bOffSet;` |
|     3 | 1936 | `		return 0;` |
|     - | 1937 | `	}` |
|     - | 1938 | `	/* "@<seconds>" absolute epoch */` |
|   255 | 1939 | `	if( z[0] == '@' ){` |
|    63 | 1940 | `		int neg = 0;` |
|    63 | 1941 | `		sxi64 v = 0;` |
|    63 | 1942 | `		const char *zAt = z;` |
|    63 | 1943 | `		z++;` |
|    63 | 1944 | `		if( z < zEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|     - | 1945 | `		/* php's lexer rejects the whole token: the error points at the '@' */` |
|    63 | 1946 | `		if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zAt - zIn) + 1; }` |
|   171 | 1947 | `		while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|    61 | 1948 | `		*pTs = neg ? -v : v;` |
|    61 | 1949 | `		*pOff = 0;` |
|    61 | 1950 | `		*pbOffSet = 1;` |
|    61 | 1951 | `		DT_SKIP_WS();` |
|    61 | 1952 | `		return (z < zEnd) ? (int)(z - zIn) + 1 : 0;` |
|     - | 1953 | `	}` |
|     - | 1954 | `	/* Absolute date: YYYY-MM-DD[...] */` |
|   192 | 1955 | `	if( zEnd-z >= 10 && SyisDigit(z[0]) && SyisDigit(z[1]) && SyisDigit(z[2])` |
|   107 | 1956 | `	 && SyisDigit(z[3]) && z[4]=='-' ){` |
|    83 | 1957 | `		sxi64 y = (z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0');` |
|    83 | 1958 | `		int mo,d,h=0,mi=0,s=0;` |
|    83 | 1959 | `		if( !SyisDigit(z[5])\|\|!SyisDigit(z[6])\|\|z[7] != '-'\|\|!SyisDigit(z[8])\|\|!SyisDigit(z[9]) ){` |
|   ! 0 | 1960 | `			return (int)(z - zIn) + 1;` |
|     - | 1961 | `		}` |
|    83 | 1962 | `		mo = (z[5]-'0')*10 + (z[6]-'0');` |
|    83 | 1963 | `		d  = (z[8]-'0')*10 + (z[9]-'0');` |
|     - | 1964 | `		/* php's lexer dies on the SECOND digit of an out-of-range month/day` |
|     - | 1965 | `		 * (either the two-digit pattern fails there, or a one-digit component` |
|     - | 1966 | `		 * matched and the separator check fails there); "00" lexes fine and` |
|     - | 1967 | `		 * normalizes (month 0 == December of the previous year). */` |
|    83 | 1968 | `		if( mo > 12 ){ return (int)(&z[6] - zIn) + 1; }` |
|    77 | 1969 | `		if( d > 31 ){ return (int)(&z[9] - zIn) + 1; }` |
|    73 | 1970 | `		if( mo == 0 ){ mo = 12; y--; }` |
|    73 | 1971 | `		z += 10;` |
|     - | 1972 | `		{` |
|    73 | 1973 | `			int rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,&bOffSet);` |
|    73 | 1974 | `			if( rcT != 0 ){ return rcT; }` |
|     - | 1975 | `		}` |
|    67 | 1976 | `		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    67 | 1977 | `		bAny = 1;` |
|   144 | 1978 | `	}else if( SyisDigit(z[0])` |
|    89 | 1979 | `	 && (iNumRc = DtTryNumericDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn)) != 0 ){` |
|     - | 1980 | `		/* DD-MM-YYYY / DD.MM.YYYY (day first), MM/DD/YYYY (slash, American), and` |
|     - | 1981 | `		 * YYYY/MM/DD (slash, year first) — see DtTryNumericDate. Anything other than` |
|     - | 1982 | `		 * 1 is an error code in DtParse's own convention (positive position / negative` |
|     - | 1983 | `		 * "double time"); propagate it verbatim. */` |
|    55 | 1984 | `		if( iNumRc != 1 ){ return iNumRc; }` |
|    47 | 1985 | `		bAny = 1;` |
|    80 | 1986 | `	}else if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|    12 | 1987 | `	 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|     - | 1988 | `		/* Time-only: HH:MM[:SS] on the base date */` |
|    11 | 1989 | `		sxi64 t = iTs + iOff;` |
|    11 | 1990 | `		sxi64 days = DtFloorDiv(t,86400);` |
|    11 | 1991 | `		int h  = (z[0]-'0')*10 + (z[1]-'0');` |
|    11 | 1992 | `		int mi = (z[3]-'0')*10 + (z[4]-'0');` |
|    11 | 1993 | `		int s = 0;` |
|     - | 1994 | `		/* php: bad hour kills the token (error at its start); bad minute /` |
|     - | 1995 | `		 * second dies on the component's second digit */` |
|    11 | 1996 | `		if( h > 24 ){ return (int)(z - zIn) + 1; }` |
|     9 | 1997 | `		if( mi > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|     7 | 1998 | `		z += 5;` |
|     7 | 1999 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     5 | 2000 | `			s = (z[1]-'0')*10 + (z[2]-'0');` |
|     5 | 2001 | `			if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|     3 | 2002 | `			z += 3;` |
|     1 | 2003 | `		}` |
|     5 | 2004 | `		iTs = days*86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     5 | 2005 | `		bAny = 1;` |
|    49 | 2006 | `	}else if( DT_LOWEQ("now",3) ){` |
|     3 | 2007 | `		z += 3;` |
|     3 | 2008 | `		bAny = 1;` |
|     1 | 2009 | `	}` |
|     - | 2010 | `	/* Relative / keyword sequence */` |
|    81 | 2011 | `	for(;;){` |
|   223 | 2012 | `		DT_SKIP_WS();` |
|   197 | 2013 | `		if( z >= zEnd ){` |
|   151 | 2014 | `			break;` |
|     - | 2015 | `		}` |
|    47 | 2016 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|     5 | 2017 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     5 | 2018 | `			iTs = days*86400 - iOff;` |
|     5 | 2019 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|     5 | 2020 | `			bAny = 1;` |
|     5 | 2021 | `			continue;` |
|     - | 2022 | `		}` |
|    43 | 2023 | `		if( DT_LOWEQ("noon",4) ){` |
|     3 | 2024 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     3 | 2025 | `			iTs = days*86400 + 12*3600 - iOff;` |
|     3 | 2026 | `			z += 4;` |
|     3 | 2027 | `			bAny = 1;` |
|     3 | 2028 | `			continue;` |
|     - | 2029 | `		}` |
|    41 | 2030 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|     3 | 2031 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) + 1;` |
|     3 | 2032 | `			iTs = days*86400 - iOff;` |
|     3 | 2033 | `			z += 8;` |
|     3 | 2034 | `			bAny = 1;` |
|     3 | 2035 | `			continue;` |
|     - | 2036 | `		}` |
|    39 | 2037 | `		if( DT_LOWEQ("yesterday",9) ){` |
|     3 | 2038 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) - 1;` |
|     3 | 2039 | `			iTs = days*86400 - iOff;` |
|     3 | 2040 | `			z += 9;` |
|     3 | 2041 | `			bAny = 1;` |
|     3 | 2042 | `			continue;` |
|     - | 2043 | `		}` |
|    37 | 2044 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|    27 | 2045 | `			int neg = 0;` |
|    27 | 2046 | `			sxi64 v = 0;` |
|    27 | 2047 | `			const char *zNumStart = z;` |
|    27 | 2048 | `			if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; }` |
|    27 | 2049 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zNumStart - zIn) + 1; }` |
|    61 | 2050 | `			while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|    27 | 2051 | `			if( neg ){ v = -v; }` |
|    64 | 2052 | `			DT_SKIP_WS();` |
|    27 | 2053 | `			if( DT_LOWEQ("seconds",7) )     { iTs += v;            z += 7; }` |
|    27 | 2054 | `			else if( DT_LOWEQ("second",6) ) { iTs += v;            z += 6; }` |
|    27 | 2055 | `			else if( DT_LOWEQ("secs",4) )   { iTs += v;            z += 4; }` |
|    27 | 2056 | `			else if( DT_LOWEQ("sec",3) )    { iTs += v;            z += 3; }` |
|    27 | 2057 | `			else if( DT_LOWEQ("minutes",7) ){ iTs += v*60;         z += 7; }` |
|    25 | 2058 | `			else if( DT_LOWEQ("minute",6) ) { iTs += v*60;         z += 6; }` |
|    25 | 2059 | `			else if( DT_LOWEQ("mins",4) )   { iTs += v*60;         z += 4; }` |
|    25 | 2060 | `			else if( DT_LOWEQ("min",3) )    { iTs += v*60;         z += 3; }` |
|    25 | 2061 | `			else if( DT_LOWEQ("hours",5) )  { iTs += v*3600;       z += 5; }` |
|    23 | 2062 | `			else if( DT_LOWEQ("hour",4) )   { iTs += v*3600;       z += 4; }` |
|    23 | 2063 | `			else if( DT_LOWEQ("days",4) )   { iTs += v*86400;      z += 4; }` |
|    23 | 2064 | `			else if( DT_LOWEQ("day",3) )    { iTs += v*86400;      z += 3; }` |
|    19 | 2065 | `			else if( DT_LOWEQ("weeks",5) )  { iTs += v*7*86400;    z += 5; }` |
|    17 | 2066 | `			else if( DT_LOWEQ("week",4) )   { iTs += v*7*86400;    z += 4; }` |
|    15 | 2067 | `			else if( DT_LOWEQ("fortnights",10) ){ iTs += v*14*86400; z += 10; }` |
|    13 | 2068 | `			else if( DT_LOWEQ("fortnight",9) )  { iTs += v*14*86400; z += 9; }` |
|    13 | 2069 | `			else if( DT_LOWEQ("months",6) ) { iTs = DtAddMonths(iTs,iOff,v); z += 6; }` |
|    11 | 2070 | `			else if( DT_LOWEQ("month",5) )  { iTs = DtAddMonths(iTs,iOff,v); z += 5; }` |
|     5 | 2071 | `			else if( DT_LOWEQ("years",5) )  { iTs = DtAddMonths(iTs,iOff,v*12); z += 5; }` |
|     5 | 2072 | `			else if( DT_LOWEQ("year",4) )   { iTs = DtAddMonths(iTs,iOff,v*12); z += 4; }` |
|     - | 2073 | `			else{` |
|     3 | 2074 | `				return (int)(z - zIn) + 1;` |
|     - | 2075 | `			}` |
|    25 | 2076 | `			bAny = 1;` |
|    25 | 2077 | `			continue;` |
|     - | 2078 | `		}` |
|    11 | 2079 | `		return (int)(z - zIn) + 1;` |
|   ! 0 | 2080 | `	}` |
|   151 | 2081 | `	if( !bAny ){` |
|   ! 0 | 2082 | `		return 1;` |
|     - | 2083 | `	}` |
|   151 | 2084 | `	*pTs = iTs;` |
|   151 | 2085 | `	*pOff = iOff;` |
|   151 | 2086 | `	*pbOffSet = bOffSet;` |
|   151 | 2087 | `	return 0;` |
|     - | 2088 | `#undef DT_SKIP_WS` |
|     - | 2089 | `#undef DT_LOWEQ` |
|   129 | 2090 | `}` |
|     - | 2091 | `/* int __dt_now() */` |
|   180 | 2092 | `static int vm_builtin_dt_now(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2093 | `{` |
|    90 | 2094 | `	SXUNUSED(nArg);` |
|    90 | 2095 | `	SXUNUSED(apArg);` |
|   181 | 2096 | `	ph7_result_int64(pCtx,(ph7_int64)time(0));` |
|   181 | 2097 | `	return PH7_OK;` |
|     1 | 2098 | `}` |
|     - | 2099 | `/* mixed __dt_parse(string $s, int $baseTs, int $baseOff)` |
|     - | 2100 | ` *   -> [ts, off, offWasExplicit] on success; php's error MESSAGE string on` |
|     - | 2101 | ` *      failure (the chunk wraps it in DateMalformedStringException). */` |
|   256 | 2102 | `static int vm_builtin_dt_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2103 | `{` |
|     - | 2104 | `	const char *zIn;` |
|     - | 2105 | `	int nLen;` |
|     - | 2106 | `	sxi64 iBaseTs;` |
|     - | 2107 | `	sxi32 iBaseOff;` |
|   257 | 2108 | `	sxi64 iTs = 0;` |
|   257 | 2109 | `	sxi32 iOff = 0;` |
|   257 | 2110 | `	int bOffSet = 0;` |
|     - | 2111 | `	int iErrPos;` |
|   257 | 2112 | `	if( nArg < 3 ){` |
|   ! 0 | 2113 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2114 | `		return PH7_OK;` |
|     - | 2115 | `	}` |
|   257 | 2116 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   257 | 2117 | `	iBaseTs  = ph7_value_to_int64(apArg[1]);` |
|   257 | 2118 | `	iBaseOff = (sxi32)ph7_value_to_int64(apArg[2]);` |
|   257 | 2119 | `	iErrPos = DtParse(zIn,nLen,iBaseTs,iBaseOff,&iTs,&iOff,&bOffSet);` |
|   257 | 2120 | `	if( iErrPos != 0 ){` |
|     - | 2121 | `		/* Negative encoding: php's "Double time specification" reason */` |
|    45 | 2122 | `		int bDouble = iErrPos < 0;` |
|    45 | 2123 | `		int iPos = (bDouble ? -iErrPos : iErrPos) - 1;` |
|    45 | 2124 | `		char cAt = (iPos < nLen) ? zIn[iPos] : ' ';` |
|     - | 2125 | `		/* php appends a reason: an alphabetic token is assumed to be a timezone` |
|     - | 2126 | `		 * lookup miss, anything else an unexpected character. */` |
|    88 | 2127 | `		ph7_result_string_format(pCtx,` |
|     - | 2128 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    22 | 2129 | `			nLen,zIn,iPos,cAt,` |
|    43 | 2130 | `			bDouble ? "Double time specification"` |
|    42 | 2131 | `			: ((cAt >= 'a' && cAt <= 'z') \|\| (cAt >= 'A' && cAt <= 'Z'))` |
|     - | 2132 | `				? "The timezone could not be found in the database"` |
|    42 | 2133 | `				: "Unexpected character");` |
|    45 | 2134 | `		return PH7_OK;` |
|     - | 2135 | `	}` |
|     - | 2136 | `	{` |
|   213 | 2137 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|   213 | 2138 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|   213 | 2139 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2140 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2141 | `		}` |
|   213 | 2142 | `		ph7_value_int64(pV,iTs);` |
|   213 | 2143 | `		ph7_array_add_elem(pArr,0,pV);` |
|   213 | 2144 | `		ph7_value_int64(pV,iOff);` |
|   213 | 2145 | `		ph7_array_add_elem(pArr,0,pV);` |
|     - | 2146 | `		/* int, not bool: 0 = no explicit offset, 1 = numeric offset/@epoch,` |
|     - | 2147 | `		 * 2 = literal "Z" (php keeps the distinction in the zone name) */` |
|   213 | 2148 | `		ph7_value_int64(pV,bOffSet);` |
|   213 | 2149 | `		ph7_array_add_elem(pArr,0,pV);` |
|   213 | 2150 | `		ph7_result_value(pCtx,pArr);` |
|     - | 2151 | `	}` |
|   213 | 2152 | `	return PH7_OK;` |
|   129 | 2153 | `}` |
|     - | 2154 | `/* string __dt_default_tz(void) — the date_default_timezone_set() identifier */` |
|   180 | 2155 | `static int vm_builtin_dt_default_tz(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2156 | `{` |
|    90 | 2157 | `	SXUNUSED(nArg);` |
|    90 | 2158 | `	SXUNUSED(apArg);` |
|   181 | 2159 | `	ph7_result_string(pCtx,pCtx->pVm->zDefTz,(int)pCtx->pVm->nDefTz);` |
|   181 | 2160 | `	return PH7_OK;` |
|     1 | 2161 | `}` |
|     - | 2162 | `/* string __dt_format(int $ts, int $off, string $tzname, string $format) */` |
|   130 | 2163 | `static int vm_builtin_dt_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2164 | `{` |
|     - | 2165 | `	Sytm sTm;` |
|     - | 2166 | `	sxi64 iTs;` |
|     - | 2167 | `	sxi32 iOff;` |
|     - | 2168 | `	const char *zName,*zFmt;` |
|     - | 2169 | `	int nName,nFmt;` |
|     - | 2170 | `	char zZone[64];` |
|   131 | 2171 | `	if( nArg < 4 ){` |
|   ! 0 | 2172 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2173 | `		return PH7_OK;` |
|     - | 2174 | `	}` |
|   131 | 2175 | `	iTs  = ph7_value_to_int64(apArg[0]);` |
|   131 | 2176 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|   131 | 2177 | `	zName = ph7_value_to_string(apArg[2],&nName);` |
|   131 | 2178 | `	zFmt  = ph7_value_to_string(apArg[3],&nFmt);` |
|   131 | 2179 | `	if( nName >= (int)sizeof(zZone) ){ nName = (int)sizeof(zZone) - 1; }` |
|   131 | 2180 | `	SyMemcpy(zName,zZone,(sxu32)nName);` |
|   131 | 2181 | `	zZone[nName] = 0;` |
|   131 | 2182 | `	DtFillSytm(iTs,iOff,zZone,&sTm);` |
|   131 | 2183 | `	DateFormat(pCtx,zFmt,nFmt,&sTm);` |
|   131 | 2184 | `	return PH7_OK;` |
|    66 | 2185 | `}` |
|     - | 2186 | `/* int __dt_make(int y, int mo, int d, int h, int i, int s, int off) */` |
|     4 | 2187 | `static int vm_builtin_dt_make(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2188 | `{` |
|     - | 2189 | `	sxi64 y;` |
|     - | 2190 | `	int mo,d,h,mi,s;` |
|     - | 2191 | `	sxi32 iOff;` |
|     5 | 2192 | `	if( nArg < 7 ){` |
|   ! 0 | 2193 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2194 | `		return PH7_OK;` |
|     - | 2195 | `	}` |
|     5 | 2196 | `	y   = ph7_value_to_int64(apArg[0]);` |
|     5 | 2197 | `	mo  = ph7_value_to_int(apArg[1]);` |
|     5 | 2198 | `	d   = ph7_value_to_int(apArg[2]);` |
|     5 | 2199 | `	h   = ph7_value_to_int(apArg[3]);` |
|     5 | 2200 | `	mi  = ph7_value_to_int(apArg[4]);` |
|     5 | 2201 | `	s   = ph7_value_to_int(apArg[5]);` |
|     5 | 2202 | `	iOff = (sxi32)ph7_value_to_int64(apArg[6]);` |
|     5 | 2203 | `	ph7_result_int64(pCtx,DtMakeTs(y,mo,d,h,mi,s,iOff));` |
|     5 | 2204 | `	return PH7_OK;` |
|     3 | 2205 | `}` |
|     - | 2206 | `/* Days in a civil month (php's overflow rules use it during diff borrows) */` |
|    50 | 2207 | `static int DtDaysInMonth(sxi64 y,int m)` |
|     1 | 2208 | `{` |
|     - | 2209 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|    51 | 2210 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|     9 | 2211 | `		return 29;` |
|     - | 2212 | `	}` |
|    43 | 2213 | `	return aMonDays[(m - 1) % 12];` |
|    26 | 2214 | `}` |
|     - | 2215 | `/* int __dt_civil_add(int ts, int off, int y, int m, int d, int h, int i,` |
|     - | 2216 | ` *                    int s, int sign)` |
|     - | 2217 | ` *   php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|     - | 2218 | ` *   (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset. */` |
|    54 | 2219 | `static int vm_builtin_dt_civil_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2220 | `{` |
|     - | 2221 | `	sxi64 iTs,iLocal,iDays,iSecs,y0,moT,dayCount;` |
|     - | 2222 | `	sxi32 iOff;` |
|     - | 2223 | `	int mo0,d0,iSign;` |
|     - | 2224 | `	sxi64 y,m,d,h,i,s;` |
|    55 | 2225 | `	if( nArg < 9 ){` |
|   ! 0 | 2226 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2227 | `		return PH7_OK;` |
|     - | 2228 | `	}` |
|    55 | 2229 | `	iTs   = ph7_value_to_int64(apArg[0]);` |
|    55 | 2230 | `	iOff  = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    55 | 2231 | `	y     = ph7_value_to_int64(apArg[2]);` |
|    55 | 2232 | `	m     = ph7_value_to_int64(apArg[3]);` |
|    55 | 2233 | `	d     = ph7_value_to_int64(apArg[4]);` |
|    55 | 2234 | `	h     = ph7_value_to_int64(apArg[5]);` |
|    55 | 2235 | `	i     = ph7_value_to_int64(apArg[6]);` |
|    55 | 2236 | `	s     = ph7_value_to_int64(apArg[7]);` |
|    55 | 2237 | `	iSign = ph7_value_to_int(apArg[8]) < 0 ? -1 : 1;` |
|    55 | 2238 | `	iLocal = iTs + iOff;` |
|    55 | 2239 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    55 | 2240 | `	iSecs  = iLocal - iDays*86400;` |
|    55 | 2241 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    55 | 2242 | `	y0 += iSign * y;` |
|    55 | 2243 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    55 | 2244 | `	y0 += DtFloorDiv(moT,12);` |
|    55 | 2245 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    55 | 2246 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    55 | 2247 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    55 | 2248 | `	ph7_result_int64(pCtx,iLocal - iOff);` |
|    55 | 2249 | `	return PH7_OK;` |
|    28 | 2250 | `}` |
|     - | 2251 | `/* array __dt_civil_diff(int ts1, int off1, int ts2)` |
|     - | 2252 | ` *   -> [y,m,d,h,i,s,days,invert]: timelib's breakdown — field-wise deltas in` |
|     - | 2253 | ` *   the FIRST operand's offset, then borrow seconds→minutes→hours→days, then` |
|     - | 2254 | ` *   the day borrow walks whole months backward from the later date (that walk` |
|     - | 2255 | ` *   is why Jan 31 → Mar 02 reports m=0 d=30, not "1 month"). */` |
|    14 | 2256 | `static int vm_builtin_dt_civil_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2257 | `{` |
|     - | 2258 | `	sxi64 iTs1,iTs2,iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|     - | 2259 | `	sxi32 iOff;` |
|     - | 2260 | `	int moA,dA,moB,dB,bInvert;` |
|     - | 2261 | `	sxi64 sA,sB,y,m,d,h,i,s;` |
|     - | 2262 | `	ph7_value *pArr,*pV;` |
|    15 | 2263 | `	if( nArg < 3 ){` |
|   ! 0 | 2264 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2265 | `		return PH7_OK;` |
|     - | 2266 | `	}` |
|    15 | 2267 | `	iTs1 = ph7_value_to_int64(apArg[0]);` |
|    15 | 2268 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    15 | 2269 | `	iTs2 = ph7_value_to_int64(apArg[2]);` |
|    15 | 2270 | `	bInvert = iTs1 > iTs2;` |
|    15 | 2271 | `	iA = bInvert ? iTs2 : iTs1;` |
|    15 | 2272 | `	iB = bInvert ? iTs1 : iTs2;` |
|    15 | 2273 | `	iLa = iA + iOff;` |
|    15 | 2274 | `	iLb = iB + iOff;` |
|    15 | 2275 | `	daysA = DtFloorDiv(iLa,86400);` |
|    15 | 2276 | `	daysB = DtFloorDiv(iLb,86400);` |
|    15 | 2277 | `	sA = iLa - daysA*86400;` |
|    15 | 2278 | `	sB = iLb - daysB*86400;` |
|    15 | 2279 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|    15 | 2280 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|    15 | 2281 | `	s = (sB % 60) - (sA % 60);` |
|    15 | 2282 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|    15 | 2283 | `	h = (sB / 3600) - (sA / 3600);` |
|    15 | 2284 | `	d = dB - dA;` |
|    15 | 2285 | `	m = moB - moA;` |
|    15 | 2286 | `	y = yB - yA;` |
|    15 | 2287 | `	if( s < 0 ){ s += 60; i--; }` |
|    15 | 2288 | `	if( i < 0 ){ i += 60; h--; }` |
|    15 | 2289 | `	if( h < 0 ){ h += 24; d--; }` |
|    27 | 2290 | `	while( d < 0 ){` |
|    13 | 2291 | `		moB--;` |
|    13 | 2292 | `		if( moB < 1 ){ moB = 12; yB--; }` |
|    13 | 2293 | `		d += DtDaysInMonth(yB,moB);` |
|    13 | 2294 | `		m--;` |
|     1 | 2295 | `	}` |
|    15 | 2296 | `	if( m < 0 ){ m += 12; y--; }` |
|    15 | 2297 | `	pArr = ph7_context_new_array(pCtx);` |
|    15 | 2298 | `	pV = ph7_context_new_scalar(pCtx);` |
|    15 | 2299 | `	if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2300 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2301 | `	}` |
|    15 | 2302 | `	ph7_value_int64(pV,y);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2303 | `	ph7_value_int64(pV,m);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2304 | `	ph7_value_int64(pV,d);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2305 | `	ph7_value_int64(pV,h);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2306 | `	ph7_value_int64(pV,i);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2307 | `	ph7_value_int64(pV,s);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2308 | `	ph7_value_int64(pV,(iB - iA) / 86400); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2309 | `	ph7_value_int64(pV,bInvert); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2310 | `	ph7_result_value(pCtx,pArr);` |
|    15 | 2311 | `	return PH7_OK;` |
|     8 | 2312 | `}` |
|     - | 2313 | `/* int __dt_isodate(int ts, int off, int y, int w, int dow)` |
|     - | 2314 | ` *   setISODate: jump to ISO year/week/weekday, preserving the time of day. */` |
|     8 | 2315 | `static int vm_builtin_dt_isodate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2316 | `{` |
|     - | 2317 | `	sxi64 iTs,iLocal,iTod,jan4,monday1,target,y;` |
|     - | 2318 | `	sxi32 iOff;` |
|     - | 2319 | `	sxi64 w,dow;` |
|     - | 2320 | `	int isoDow;` |
|     9 | 2321 | `	if( nArg < 5 ){` |
|   ! 0 | 2322 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2323 | `		return PH7_OK;` |
|     - | 2324 | `	}` |
|     9 | 2325 | `	iTs = ph7_value_to_int64(apArg[0]);` |
|     9 | 2326 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|     9 | 2327 | `	y   = ph7_value_to_int64(apArg[2]);` |
|     9 | 2328 | `	w   = ph7_value_to_int64(apArg[3]);` |
|     9 | 2329 | `	dow = ph7_value_to_int64(apArg[4]);` |
|     9 | 2330 | `	iLocal = iTs + iOff;` |
|     9 | 2331 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|     9 | 2332 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|     9 | 2333 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|     9 | 2334 | `	monday1 = jan4 - (isoDow - 1);` |
|     9 | 2335 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|     9 | 2336 | `	ph7_result_int64(pCtx,target*86400 + iTod - iOff);` |
|     9 | 2337 | `	return PH7_OK;` |
|     5 | 2338 | `}` |
|     - | 2339 | `/* Consume nMin..nMax digits from *pz; returns count consumed (0 = failure) */` |
|   130 | 2340 | `static int DtEatDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2341 | `{` |
|   131 | 2342 | `	const char *z = *pz;` |
|   131 | 2343 | `	sxi64 v = 0;` |
|   131 | 2344 | `	int n = 0;` |
|   473 | 2345 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|   343 | 2346 | `		v = v*10 + (z[0] - '0');` |
|   343 | 2347 | `		z++;` |
|   343 | 2348 | `		n++;` |
|     1 | 2349 | `	}` |
|   131 | 2350 | `	if( n < nMin ){` |
|     3 | 2351 | `		return 0;` |
|     - | 2352 | `	}` |
|   129 | 2353 | `	*pz = z;` |
|   129 | 2354 | `	*pVal = v;` |
|   129 | 2355 | `	return n;` |
|    66 | 2356 | `}` |
|     - | 2357 | `/* timelib_get_nr's recovery: skip non-digits hunting for the field.` |
|     - | 2358 | ` * Returns 1 = found+read, 0 = digits present but short, -1 = exhausted. */` |
|     2 | 2359 | `static int DtHuntDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2360 | `{` |
|     3 | 2361 | `	const char *z = *pz;` |
|    13 | 2362 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|     3 | 2363 | `	*pz = z;` |
|     3 | 2364 | `	if( z >= zEnd ){` |
|     3 | 2365 | `		return -1;` |
|     - | 2366 | `	}` |
|   ! 0 | 2367 | `	return DtEatDigits(pz,zEnd,nMin,nMax,pVal) ? 1 : 0;` |
|     2 | 2368 | `}` |
|     - | 2369 | `/* Case-insensitive name-table lookup; returns 1-based index or 0 */` |
|    14 | 2370 | `static int DtEatName(const char **pz,const char *zEnd,const char **azNames,int nNames)` |
|     1 | 2371 | `{` |
|     - | 2372 | `	int k;` |
|    23 | 2373 | `	for( k = 0 ; k < nNames ; k++ ){` |
|    23 | 2374 | `		int n = (int)SyStrlen(azNames[k]);` |
|    23 | 2375 | `		if( zEnd - *pz >= n && SyStrnicmp(*pz,azNames[k],(sxu32)n) == 0 ){` |
|    15 | 2376 | `			*pz += n;` |
|    15 | 2377 | `			return k + 1;` |
|     - | 2378 | `		}` |
|     5 | 2379 | `	}` |
|   ! 0 | 2380 | `	return 0;` |
|     8 | 2381 | `}` |
|     - | 2382 | `/* mixed __dt_from_format(string fmt, string input, int nowTs, int defOff)` |
|     - | 2383 | ` *   php's DateTime::createFromFormat engine. Success: [ts, off, offKind, name]` |
|     - | 2384 | ` *   where offKind 0=none-parsed, 1=numeric offset, 2=literal Z, 3=named id.` |
|     - | 2385 | ` *   Failure: "POS\tMESSAGE" (timelib's message strings; PHL reports the FIRST` |
|     - | 2386 | ` *   error where php may accumulate several — recorded). A trailing-data` |
|     - | 2387 | ` *   warning rides as [4]=pos, [5]=msg on the success array. */` |
|    44 | 2388 | `static int vm_builtin_dt_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2389 | `{` |
|     - | 2390 | `	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};` |
|     - | 2391 | `	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",` |
|     - | 2392 | `		"thursday","friday","saturday"};` |
|     - | 2393 | `	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",` |
|     - | 2394 | `		"aug","sep","oct","nov","dec"};` |
|     - | 2395 | `	static const char *azMonFull[] = {"january","february","march","april",` |
|     - | 2396 | `		"may","june","july","august","september","october","november","december"};` |
|     - | 2397 | `	const char *zFmt,*zIn,*zEnd,*zInEnd,*z;` |
|     - | 2398 | `	int nFmt,nIn;` |
|     - | 2399 | `	sxi64 iNow,v;` |
|     - | 2400 | `	sxi32 iDefOff;` |
|     - | 2401 | `	/* -1 == unset */` |
|    45 | 2402 | `	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;` |
|    45 | 2403 | `	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;` |
|    45 | 2404 | `	int iOffKind = 0;` |
|    45 | 2405 | `	sxi32 iOffVal = 0;` |
|     - | 2406 | `	char zName[16];` |
|    45 | 2407 | `	const char *zErr = 0;` |
|     - | 2408 | `	const char *aWarnMsg[3];` |
|     - | 2409 | `	int aWarnPos[3];` |
|    45 | 2410 | `	int nWarn = 0,bAborted = 0;` |
|     - | 2411 | `	const char *aErrMsg[8];` |
|     - | 2412 | `	int aErrPos[8];` |
|    45 | 2413 | `	int nErr = 0,nErrKept = 0;` |
|    45 | 2414 | `	if( nArg < 4 ){` |
|   ! 0 | 2415 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2416 | `		return PH7_OK;` |
|     - | 2417 | `	}` |
|    45 | 2418 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    45 | 2419 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    45 | 2420 | `	iNow = ph7_value_to_int64(apArg[2]);` |
|    45 | 2421 | `	iDefOff = (sxi32)ph7_value_to_int64(apArg[3]);` |
|    45 | 2422 | `	zEnd = &zFmt[nFmt];` |
|    45 | 2423 | `	zInEnd = &zIn[nIn];` |
|    45 | 2424 | `	z = zIn;` |
|    45 | 2425 | `	zName[0] = 0;` |
|     - | 2426 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|     - | 2427 | `	{ int _p = (iPos),_k,_f = -1; \` |
|     - | 2428 | `	  nErr++; \` |
|     - | 2429 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|     - | 2430 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|     - | 2431 | `	  else if( nErrKept < 8 ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|   299 | 2432 | `	while( zFmt < zEnd ){` |
|   257 | 2433 | `		char c = zFmt[0];` |
|   257 | 2434 | `		zFmt++;` |
|   257 | 2435 | `		zErr = 0;` |
|   257 | 2436 | `		if( c == '!' ){` |
|     7 | 2437 | `			y = 1970; mo = 1; d = 1; h = 0; mi = 0; s = 0;` |
|     7 | 2438 | `			h12 = -1; iMeridiem = -1;` |
|     7 | 2439 | `			continue;` |
|     - | 2440 | `		}` |
|   251 | 2441 | `		if( c == '\|' ){ bPipe = 1; continue; }` |
|   247 | 2442 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|   245 | 2443 | `		if( z >= zInEnd ){` |
|     - | 2444 | `			/* timelib aborts the scan once input is exhausted */` |
|     5 | 2445 | `			DT_FF_LOGERR(nIn,"Not enough data available to satisfy format");` |
|     3 | 2446 | `			break;` |
|     - | 2447 | `		}` |
|   243 | 2448 | `		switch( c ){` |
|    15 | 2449 | `		case 'd': case 'j':` |
|    31 | 2450 | `			if( !DtEatDigits(&z,zInEnd,1,2,&d) ){` |
|   ! 0 | 2451 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit day could not be found");` |
|   ! 0 | 2452 | `				if( DtHuntDigits(&z,zInEnd,1,2,&d) < 0 ){` |
|   ! 0 | 2453 | `					DT_FF_LOGERR(nIn,"A two digit day could not be found");` |
|   ! 0 | 2454 | `				}` |
|   ! 0 | 2455 | `			}` |
|    31 | 2456 | `			break;` |
|     1 | 2457 | `		case 'D':` |
|     3 | 2458 | `			if( !DtEatName(&z,zInEnd,azDay3,7) ){` |
|   ! 0 | 2459 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2460 | `			}` |
|     3 | 2461 | `			break;` |
|     1 | 2462 | `		case 'l':` |
|     3 | 2463 | `			if( !DtEatName(&z,zInEnd,azDayFull,7) ){` |
|   ! 0 | 2464 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2465 | `			}` |
|     3 | 2466 | `			break;` |
|     1 | 2467 | `		case 'S':` |
|     - | 2468 | `			/* ordinal suffix: st nd rd th */` |
|     4 | 2469 | `			if( zInEnd-z >= 2 && ((z[0]=='s'&&z[1]=='t')\|\|(z[0]=='n'&&z[1]=='d')` |
|     2 | 2470 | `			 \|\|(z[0]=='r'&&z[1]=='d')\|\|(z[0]=='t'&&z[1]=='h')) ){` |
|     3 | 2471 | `				z += 2;` |
|     1 | 2472 | `			}` |
|     3 | 2473 | `			break;` |
|    13 | 2474 | `		case 'm': case 'n':` |
|    27 | 2475 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mo) ){` |
|   ! 0 | 2476 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit month could not be found");` |
|   ! 0 | 2477 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mo) < 0 ){` |
|   ! 0 | 2478 | `					DT_FF_LOGERR(nIn,"A two digit month could not be found");` |
|   ! 0 | 2479 | `				}` |
|   ! 0 | 2480 | `			}` |
|    27 | 2481 | `			break;` |
|     1 | 2482 | `		case 'M':{` |
|     3 | 2483 | `			int k = DtEatName(&z,zInEnd,azMon3,12);` |
|     3 | 2484 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2485 | `			break;` |
|     - | 2486 | `				 }` |
|     1 | 2487 | `		case 'F':{` |
|     3 | 2488 | `			int k = DtEatName(&z,zInEnd,azMonFull,12);` |
|     3 | 2489 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2490 | `			break;` |
|     - | 2491 | `				 }` |
|   ! 0 | 2492 | `		case 'y':` |
|   ! 0 | 2493 | `			if( DtEatDigits(&z,zInEnd,2,2,&y) ){` |
|   ! 0 | 2494 | `				y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2495 | `			}else{` |
|   ! 0 | 2496 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit year could not be found");` |
|   ! 0 | 2497 | `				if( DtHuntDigits(&z,zInEnd,2,2,&y) < 0 ){` |
|   ! 0 | 2498 | `					DT_FF_LOGERR(nIn,"A two digit year could not be found");` |
|   ! 0 | 2499 | `				}else if( y >= 0 ){` |
|   ! 0 | 2500 | `					y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2501 | `				}` |
|     - | 2502 | `			}` |
|   ! 0 | 2503 | `			break;` |
|    17 | 2504 | `		case 'Y':{` |
|    35 | 2505 | `			int neg = 0;` |
|    35 | 2506 | `			if( z < zInEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|    35 | 2507 | `			if( DtEatDigits(&z,zInEnd,1,4,&y) ){` |
|    33 | 2508 | `				if( neg ){ y = -y; }` |
|    17 | 2509 | `			}else{` |
|     3 | 2510 | `				DT_FF_LOGERR((int)(z - zIn),"A four digit year could not be found");` |
|     3 | 2511 | `				if( DtHuntDigits(&z,zInEnd,1,4,&y) < 0 ){` |
|     5 | 2512 | `					DT_FF_LOGERR(nIn,"A four digit year could not be found");` |
|     1 | 2513 | `				}` |
|     - | 2514 | `			}` |
|    35 | 2515 | `			break;` |
|     - | 2516 | `				 }` |
|     4 | 2517 | `		case 'H': case 'G':` |
|     9 | 2518 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h) ){` |
|   ! 0 | 2519 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2520 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h) < 0 ){` |
|   ! 0 | 2521 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2522 | `				}` |
|   ! 0 | 2523 | `			}` |
|     9 | 2524 | `			break;` |
|     2 | 2525 | `		case 'h': case 'g':` |
|     5 | 2526 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h12) ){` |
|   ! 0 | 2527 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2528 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h12) < 0 ){` |
|   ! 0 | 2529 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2530 | `				}` |
|   ! 0 | 2531 | `			}` |
|     5 | 2532 | `			break;` |
|     6 | 2533 | `		case 'i':` |
|    13 | 2534 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mi) ){` |
|   ! 0 | 2535 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit minute could not be found");` |
|   ! 0 | 2536 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mi) < 0 ){` |
|   ! 0 | 2537 | `					DT_FF_LOGERR(nIn,"A two digit minute could not be found");` |
|   ! 0 | 2538 | `				}` |
|   ! 0 | 2539 | `			}` |
|    13 | 2540 | `			break;` |
|     2 | 2541 | `		case 's':` |
|     5 | 2542 | `			if( !DtEatDigits(&z,zInEnd,1,2,&s) ){` |
|   ! 0 | 2543 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit second could not be found");` |
|   ! 0 | 2544 | `				if( DtHuntDigits(&z,zInEnd,1,2,&s) < 0 ){` |
|   ! 0 | 2545 | `					DT_FF_LOGERR(nIn,"A two digit second could not be found");` |
|   ! 0 | 2546 | `				}` |
|   ! 0 | 2547 | `			}` |
|     5 | 2548 | `			break;` |
|   ! 0 | 2549 | `		case 'u':` |
|     - | 2550 | `			/* micro parsed then dropped: PHL keeps whole seconds (recorded) */` |
|   ! 0 | 2551 | `			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){` |
|   ! 0 | 2552 | `				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");` |
|   ! 0 | 2553 | `				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){` |
|   ! 0 | 2554 | `					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");` |
|   ! 0 | 2555 | `				}` |
|   ! 0 | 2556 | `			}` |
|   ! 0 | 2557 | `			break;` |
|   ! 0 | 2558 | `		case 'v':` |
|   ! 0 | 2559 | `			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){` |
|   ! 0 | 2560 | `				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");` |
|   ! 0 | 2561 | `				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){` |
|   ! 0 | 2562 | `					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");` |
|   ! 0 | 2563 | `				}` |
|   ! 0 | 2564 | `			}` |
|   ! 0 | 2565 | `			break;` |
|     2 | 2566 | `		case 'a': case 'A':{` |
|     - | 2567 | `			static const char *azMer[] = {"am","pm","a.m.","p.m."};` |
|     5 | 2568 | `			int k = DtEatName(&z,zInEnd,azMer,4);` |
|     5 | 2569 | `			if( k ){` |
|     5 | 2570 | `				iMeridiem = ((k - 1) & 1);` |
|     3 | 2571 | `			}else{` |
|   ! 0 | 2572 | `				zErr = "A meridian could not be found";` |
|     - | 2573 | `			}` |
|     5 | 2574 | `			break;` |
|     - | 2575 | `				 }` |
|     2 | 2576 | `		case 'U':{` |
|     5 | 2577 | `			int neg = 0;` |
|     5 | 2578 | `			if( z < zInEnd && z[0]=='-' ){ neg = 1; z++; }` |
|     5 | 2579 | `			if( DtEatDigits(&z,zInEnd,1,19,&uVal) ){` |
|     5 | 2580 | `				if( neg ){ uVal = -uVal; }` |
|     5 | 2581 | `				bHasU = 1;` |
|     3 | 2582 | `			}else{` |
|   ! 0 | 2583 | `				DT_FF_LOGERR((int)(z - zIn),"A unix timestamp could not be found");` |
|   ! 0 | 2584 | `				if( DtHuntDigits(&z,zInEnd,1,19,&uVal) < 0 ){` |
|   ! 0 | 2585 | `					DT_FF_LOGERR(nIn,"A unix timestamp could not be found");` |
|   ! 0 | 2586 | `				}else{` |
|   ! 0 | 2587 | `					if( neg ){ uVal = -uVal; }` |
|   ! 0 | 2588 | `					bHasU = 1;` |
|     - | 2589 | `				}` |
|     - | 2590 | `			}` |
|     5 | 2591 | `			break;` |
|     - | 2592 | `				 }` |
|     1 | 2593 | `		case 'e': case 'T':{` |
|     - | 2594 | `			static const char *azZone[] = {"UTC","GMT","Z"};` |
|     3 | 2595 | `			int k = DtEatName(&z,zInEnd,azZone,3);` |
|     3 | 2596 | `			if( k == 3 ){` |
|   ! 0 | 2597 | `				iOffKind = 2; iOffVal = 0;` |
|     3 | 2598 | `			}else if( k ){` |
|     3 | 2599 | `				iOffKind = 3; iOffVal = 0;` |
|     3 | 2600 | `				SyMemcpy(azZone[k-1],zName,4);` |
|     1 | 2601 | `			}else if( z < zInEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|   ! 0 | 2602 | `				goto parse_num_off;` |
|   ! 0 | 2603 | `			}else{` |
|   ! 0 | 2604 | `				zErr = "The timezone could not be found in the database";` |
|     - | 2605 | `			}` |
|     3 | 2606 | `			break;` |
|     2 | 2607 | `				 }` |
|     - | 2608 | `		case 'O': case 'P':` |
|     2 | 2609 | `parse_num_off:	{` |
|     5 | 2610 | `			int sign,oh,om = 0;` |
|     - | 2611 | `			sxi64 t;` |
|     5 | 2612 | `			if( z >= zInEnd \|\| (z[0] != '+' && z[0] != '-') ){` |
|   ! 0 | 2613 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2614 | `				break;` |
|     - | 2615 | `			}` |
|     5 | 2616 | `			sign = (z[0]=='-') ? -1 : 1;` |
|     5 | 2617 | `			z++;` |
|     5 | 2618 | `			if( !DtEatDigits(&z,zInEnd,2,2,&t) ){` |
|   ! 0 | 2619 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2620 | `				break;` |
|     - | 2621 | `			}` |
|     5 | 2622 | `			oh = (int)t;` |
|     5 | 2623 | `			if( z < zInEnd && z[0]==':' ){ z++; }` |
|     5 | 2624 | `			if( DtEatDigits(&z,zInEnd,2,2,&t) ){ om = (int)t; }` |
|     5 | 2625 | `			iOffKind = 1;` |
|     5 | 2626 | `			iOffVal = sign * (oh*3600 + om*60);` |
|     5 | 2627 | `			break;` |
|     - | 2628 | `				 }` |
|   ! 0 | 2629 | `		case '?':` |
|   ! 0 | 2630 | `			if( z < zInEnd ){ z++; }` |
|   ! 0 | 2631 | `			break;` |
|   ! 0 | 2632 | `		case '*':` |
|     - | 2633 | `			/* skip input until the next separator byte */` |
|   ! 0 | 2634 | `			while( z < zInEnd && !SyisDigit(z[0]) && z[0] != ';' && z[0] != ':'` |
|   ! 0 | 2635 | `			 && z[0] != '/' && z[0] != '.' && z[0] != ',' && z[0] != '-'` |
|   ! 0 | 2636 | `			 && z[0] != '(' && z[0] != ')' && z[0] != ' ' ){` |
|   ! 0 | 2637 | `				z++;` |
|   ! 0 | 2638 | `			}` |
|   ! 0 | 2639 | `			break;` |
|     1 | 2640 | `		case '#':` |
|     3 | 2641 | `			if( z < zInEnd && (z[0]==';'\|\|z[0]==':'\|\|z[0]=='/'\|\|z[0]=='.'` |
|   ! 0 | 2642 | `			 \|\|z[0]==','\|\|z[0]=='-'\|\|z[0]=='('\|\|z[0]==')') ){` |
|     3 | 2643 | `				z++;` |
|     2 | 2644 | `			}else{` |
|   ! 0 | 2645 | `				zErr = "The separation symbol could not be found";` |
|     - | 2646 | `			}` |
|     3 | 2647 | `			break;` |
|     1 | 2648 | `		case '\\':` |
|     3 | 2649 | `			if( zFmt < zEnd ){` |
|     3 | 2650 | `				if( z < zInEnd && z[0] == zFmt[0] ){` |
|     3 | 2651 | `					z++;` |
|     3 | 2652 | `					zFmt++;` |
|     2 | 2653 | `				}else{` |
|     - | 2654 | `					/* a literal mismatch aborts timelib's scan */` |
|   ! 0 | 2655 | `					DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|   ! 0 | 2656 | `					zFmt = zEnd;` |
|   ! 0 | 2657 | `					bAborted = 1;` |
|     - | 2658 | `				}` |
|     1 | 2659 | `			}` |
|     3 | 2660 | `			break;` |
|    34 | 2661 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|     - | 2662 | `		case '(' : case ')':` |
|    69 | 2663 | `			if( z < zInEnd && z[0] == c ){` |
|    69 | 2664 | `				z++;` |
|    35 | 2665 | `			}else{` |
|     - | 2666 | `				/* timelib logs BOTH messages (count +2, last-wins on the` |
|     - | 2667 | `				 * position), consumes the offending byte, and keeps going */` |
|   ! 0 | 2668 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 2669 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 2670 | `				z++;` |
|     - | 2671 | `			}` |
|    69 | 2672 | `			break;` |
|    13 | 2673 | `		case ' ':` |
|    27 | 2674 | `			if( z < zInEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|    27 | 2675 | `				z++;` |
|    14 | 2676 | `			}else{` |
|   ! 0 | 2677 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 2678 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 2679 | `				z++;` |
|     - | 2680 | `			}` |
|    27 | 2681 | `			break;` |
|     1 | 2682 | `		default:` |
|     - | 2683 | `			/* any other format byte must match the input verbatim; a mismatch` |
|     - | 2684 | `			 * aborts timelib's scan */` |
|     3 | 2685 | `			if( z < zInEnd && z[0] == c ){` |
|   ! 0 | 2686 | `				z++;` |
|   ! 0 | 2687 | `			}else{` |
|     3 | 2688 | `				DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|     3 | 2689 | `				zFmt = zEnd;` |
|     3 | 2690 | `				bAborted = 1;` |
|     - | 2691 | `			}` |
|     2 | 2692 | `			break;` |
|     - | 2693 | `		}` |
|   243 | 2694 | `		if( zErr ){` |
|     - | 2695 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|   ! 0 | 2696 | `			DT_FF_LOGERR((int)(z - zIn),zErr);` |
|   ! 0 | 2697 | `		}` |
|     1 | 2698 | `	}` |
|    45 | 2699 | `	if( z < zInEnd && !bAborted ){` |
|     5 | 2700 | `		if( bPlus ){` |
|     - | 2701 | `			/* '+' downgrades trailing data to a warning */` |
|     3 | 2702 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     3 | 2703 | `			aWarnMsg[nWarn] = "Trailing data";` |
|     3 | 2704 | `			nWarn++;` |
|     2 | 2705 | `		}else{` |
|     3 | 2706 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|     - | 2707 | `		}` |
|     2 | 2708 | `	}` |
|    45 | 2709 | `	if( nErr > 0 ){` |
|     - | 2710 | `		SyBlob sOut;` |
|     - | 2711 | `		int k;` |
|     7 | 2712 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     7 | 2713 | `		SyBlobFormat(&sOut,"%d",nErr);` |
|    15 | 2714 | `		for( k = 0 ; k < nErrKept ; k++ ){` |
|     9 | 2715 | `			SyBlobFormat(&sOut,"\n%d\t%s",aErrPos[k],aErrMsg[k]);` |
|     5 | 2716 | `		}` |
|     7 | 2717 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     7 | 2718 | `		SyBlobRelease(&sOut);` |
|     7 | 2719 | `		return PH7_OK;` |
|     - | 2720 | `	}` |
|    39 | 2721 | `	if( bPipe ){` |
|     5 | 2722 | `		if( y < 0 ){ y = 1970; }` |
|     5 | 2723 | `		if( mo < 0 ){ mo = 1; }` |
|     5 | 2724 | `		if( d < 0 ){ d = 1; }` |
|     5 | 2725 | `		if( h < 0 && h12 < 0 ){ h = 0; }` |
|     5 | 2726 | `		if( mi < 0 ){ mi = 0; }` |
|     5 | 2727 | `		if( s < 0 ){ s = 0; }` |
|     2 | 2728 | `	}` |
|     - | 2729 | `	{` |
|     - | 2730 | `		/* remaining unset fields come from "now" in the default offset */` |
|    39 | 2731 | `		sxi64 iLocal = iNow + iDefOff;` |
|    39 | 2732 | `		sxi64 days = DtFloorDiv(iLocal,86400);` |
|    39 | 2733 | `		sxi64 secs = iLocal - days*86400;` |
|     - | 2734 | `		sxi64 ny;` |
|     - | 2735 | `		int nmo,nd;` |
|    39 | 2736 | `		DtCivilFromDays(days,&ny,&nmo,&nd);` |
|    39 | 2737 | `		if( y < 0 ){ y = ny; }` |
|    39 | 2738 | `		if( mo < 0 ){ mo = nmo; }` |
|    39 | 2739 | `		if( d < 0 ){ d = nd; }` |
|    39 | 2740 | `		if( h12 >= 0 ){` |
|     5 | 2741 | `			h = (h12 % 12) + ((iMeridiem == 1) ? 12 : 0);` |
|     2 | 2742 | `		}` |
|     - | 2743 | `		/* php: parsing a time component zeroes the finer unset units */` |
|    39 | 2744 | `		if( h >= 0 ){` |
|    19 | 2745 | `			if( mi < 0 ){ mi = 0; }` |
|    19 | 2746 | `			if( s < 0 ){ s = 0; }` |
|    30 | 2747 | `		}else if( mi >= 0 ){` |
|     3 | 2748 | `			if( s < 0 ){ s = 0; }` |
|     1 | 2749 | `		}` |
|    39 | 2750 | `		if( h < 0 ){ h = secs / 3600; }` |
|    39 | 2751 | `		if( mi < 0 ){ mi = (secs / 60) % 60; }` |
|    39 | 2752 | `		if( s < 0 ){ s = secs % 60; }` |
|     - | 2753 | `	}` |
|     - | 2754 | `	/* php validates the RESOLVED fields and warns (parse still succeeds,` |
|     - | 2755 | `	 * values roll over via civil arithmetic) */` |
|    39 | 2756 | `	if( mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo) ){` |
|     3 | 2757 | `		if( nWarn < 3 ){` |
|     3 | 2758 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 2759 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|     3 | 2760 | `			nWarn++;` |
|     1 | 2761 | `		}` |
|     1 | 2762 | `	}` |
|    39 | 2763 | `	if( h > 24 \|\| mi > 59 \|\| s > 59 ){` |
|     3 | 2764 | `		if( nWarn < 3 ){` |
|     3 | 2765 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 2766 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|     3 | 2767 | `			nWarn++;` |
|     1 | 2768 | `		}` |
|     1 | 2769 | `	}` |
|     - | 2770 | `	{` |
|    39 | 2771 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    39 | 2772 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|     - | 2773 | `		sxi64 iTs;` |
|    39 | 2774 | `		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;` |
|    39 | 2775 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2776 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2777 | `		}` |
|    39 | 2778 | `		if( bHasU ){` |
|     5 | 2779 | `			iTs = uVal;` |
|     5 | 2780 | `			iUseOff = 0;` |
|     5 | 2781 | `			iOffKind = 1;` |
|     3 | 2782 | `		}else{` |
|    35 | 2783 | `			iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);` |
|     - | 2784 | `		}` |
|    39 | 2785 | `		ph7_value_int64(pV,iTs);           ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2786 | `		ph7_value_int64(pV,iUseOff);       ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2787 | `		ph7_value_int64(pV,iOffKind);      ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2788 | `		ph7_value_string(pV,zName,-1);     ph7_array_add_elem(pArr,0,pV);` |
|     - | 2789 | `		{` |
|     - | 2790 | `			int k;` |
|    45 | 2791 | `			for( k = 0 ; k < nWarn ; k++ ){` |
|     7 | 2792 | `				ph7_value_int64(pV,aWarnPos[k]);` |
|     7 | 2793 | `				ph7_array_add_elem(pArr,0,pV);` |
|     7 | 2794 | `				ph7_value_string(pV,aWarnMsg[k],-1);` |
|     7 | 2795 | `				ph7_array_add_elem(pArr,0,pV);` |
|     4 | 2796 | `			}` |
|     - | 2797 | `		}` |
|    39 | 2798 | `		ph7_result_value(pCtx,pArr);` |
|     - | 2799 | `	}` |
|    39 | 2800 | `	return PH7_OK;` |
|    23 | 2801 | `}` |
|     - | 2802 | `/*` |
|     - | 2803 | ` * The embedded DateTime library. Timezone scope: UTC + fixed offsets.` |
|     - | 2804 | ` */` |
|     - | 2805 | `static const char zDateTimeLib[] =` |
|     - | 2806 | `"class DateException extends Exception {}"` |
|     - | 2807 | `"class DateMalformedStringException extends DateException {}"` |
|     - | 2808 | `"class DateInvalidTimeZoneException extends DateException {}"` |
|     - | 2809 | `"class DateMalformedIntervalStringException extends DateException {}"` |
|     - | 2810 | `"class DateMalformedPeriodStringException extends DateException {}"` |
|     - | 2811 | `"interface DateTimeInterface {"` |
|     - | 2812 | `" const ATOM = 'Y-m-d\\TH:i:sP';"` |
|     - | 2813 | `" const COOKIE = 'l, d-M-Y H:i:s T';"` |
|     - | 2814 | `" const ISO8601 = 'Y-m-d\\TH:i:sO';"` |
|     - | 2815 | `" const ISO8601_EXPANDED = 'X-m-d\\TH:i:sP';"` |
|     - | 2816 | `" const RFC822 = 'D, d M y H:i:s O';"` |
|     - | 2817 | `" const RFC850 = 'l, d-M-y H:i:s T';"` |
|     - | 2818 | `" const RFC1036 = 'D, d M y H:i:s O';"` |
|     - | 2819 | `" const RFC1123 = 'D, d M Y H:i:s O';"` |
|     - | 2820 | `" const RFC7231 = 'D, d M Y H:i:s \\G\\M\\T';"` |
|     - | 2821 | `" const RFC2822 = 'D, d M Y H:i:s O';"` |
|     - | 2822 | `" const RFC3339 = 'Y-m-d\\TH:i:sP';"` |
|     - | 2823 | `" const RFC3339_EXTENDED = 'Y-m-d\\TH:i:s.vP';"` |
|     - | 2824 | `" const RSS = 'D, d M Y H:i:s O';"` |
|     - | 2825 | `" const W3C = 'Y-m-d\\TH:i:sP';"` |
|     - | 2826 | `"}"` |
|     - | 2827 | `"class DateTimeZone {"` |
|     - | 2828 | `" private $__dtzOff = 0;"` |
|     - | 2829 | `" private $__dtzName = 'UTC';"` |
|     - | 2830 | `" public function __construct($timezone = 'UTC'){"` |
|     - | 2831 | `"  $tz = (string)$timezone;"` |
|     - | 2832 | `"  if( strcasecmp($tz, 'UTC') === 0 ){"` |
|     - | 2833 | `"   $this->__dtzOff = 0; $this->__dtzName = 'UTC';"` |
|     - | 2834 | `"   return;"` |
|     - | 2835 | `"  }"` |
|     - | 2836 | `"  if( $tz === 'Z' ){"` |
|     - | 2837 | `"   $this->__dtzOff = 0; $this->__dtzName = 'Z';"` |
|     - | 2838 | `"   return;"` |
|     - | 2839 | `"  }"` |
|     - | 2840 | `"  if( strcasecmp($tz, 'GMT') === 0 ){"` |
|     - | 2841 | `"   $this->__dtzOff = 0; $this->__dtzName = 'GMT';"` |
|     - | 2842 | `"   return;"` |
|     - | 2843 | `"  }"` |
|     - | 2844 | `"  $m = null;"` |
|     - | 2845 | `"  if( preg_match('/^([+-])(\\d{2}):?(\\d{2})$/', $tz, $m) ){"` |
|     - | 2846 | `"   $off = ((int)$m[2]) * 3600 + ((int)$m[3]) * 60;"` |
|     - | 2847 | `"   if( $m[1] === '-' ){ $off = -$off; }"` |
|     - | 2848 | `"   $this->__dtzOff = $off;"` |
|     - | 2849 | `"   $this->__dtzName = $m[1] . $m[2] . ':' . $m[3];"` |
|     - | 2850 | `"   return;"` |
|     - | 2851 | `"  }"` |
|     - | 2852 | `"  throw new DateInvalidTimeZoneException("` |
|     - | 2853 | `"   'DateTimeZone::__construct(): Unknown or bad timezone (' . $tz . ')');"` |
|     - | 2854 | `" }"` |
|     - | 2855 | `" public function getName(){ return $this->__dtzName; }"` |
|     - | 2856 | `" public function getOffset($datetime = null){ return $this->__dtzOff; }"` |
|     - | 2857 | `"}"` |
|     - | 2858 | `"trait __DtCoreT {"` |
|     - | 2859 | `" private $__dtTs = 0;"` |
|     - | 2860 | `" private $__dtOff = 0;"` |
|     - | 2861 | `" private $__dtName = 'UTC';"` |
|     - | 2862 | `" private function __dtInit($datetime, $timezone){"` |
|     - | 2863 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 2864 | `"  if( $timezone !== null ){"` |
|     - | 2865 | `"   $off = $timezone->getOffset($this);"` |
|     - | 2866 | `"   $name = $timezone->getName();"` |
|     - | 2867 | `"  }"` |
|     - | 2868 | `"  $r = __dt_parse((string)$datetime, __dt_now(), $off);"` |
|     - | 2869 | `"  if( is_string($r) ){ throw new DateMalformedStringException($r); }"` |
|     - | 2870 | `"  $this->__dtTs = $r[0];"` |
|     - | 2871 | `"  if( $r[2] ){"` |
|     - | 2872 | `"   $this->__dtOff = $r[1];"` |
|     - | 2873 | `"   $this->__dtName = $r[2] === 2 ? 'Z' : $this->__dtOffName($r[1]);"` |
|     - | 2874 | `"  }else{"` |
|     - | 2875 | `"   $this->__dtOff = $off;"` |
|     - | 2876 | `"   $this->__dtName = $name;"` |
|     - | 2877 | `"  }"` |
|     - | 2878 | `" }"` |
|     - | 2879 | `" private function __dtOffName($off){"` |
|     - | 2880 | `"  $s = $off < 0 ? '-' : '+';"` |
|     - | 2881 | `"  $a = $off < 0 ? -$off : $off;"` |
|     - | 2882 | `"  return $s . sprintf('%02d:%02d', intdiv($a, 3600), intdiv($a % 3600, 60));"` |
|     - | 2883 | `" }"` |
|     - | 2884 | `" public function format($format){ return __dt_format($this->__dtTs, $this->__dtOff, $this->__dtName, (string)$format); }"` |
|     - | 2885 | `" public function getTimestamp(){ return $this->__dtTs; }"` |
|     - | 2886 | `" public function getOffset(){ return $this->__dtOff; }"` |
|     - | 2887 | `" public function getTimezone(){ return new DateTimeZone($this->__dtName); }"` |
|     - | 2888 | `" public function diff($targetObject, $absolute = false){"` |
|     - | 2889 | `"  $r = __dt_civil_diff($this->__dtTs, $this->__dtOff, $targetObject->getTimestamp());"` |
|     - | 2890 | `"  $iv = new DateInterval('P0D');"` |
|     - | 2891 | `"  $iv->y = $r[0]; $iv->m = $r[1]; $iv->d = $r[2];"` |
|     - | 2892 | `"  $iv->h = $r[3]; $iv->i = $r[4]; $iv->s = $r[5];"` |
|     - | 2893 | `"  $iv->days = $r[6];"` |
|     - | 2894 | `"  $iv->invert = $absolute ? 0 : $r[7];"` |
|     - | 2895 | `"  return $iv;"` |
|     - | 2896 | `" }"` |
|     - | 2897 | `" private function __dtAddTs($interval, $sign){"` |
|     - | 2898 | `"  if( $interval->invert ){ $sign = -$sign; }"` |
|     - | 2899 | `"  return __dt_civil_add($this->__dtTs, $this->__dtOff, $interval->y, $interval->m,"` |
|     - | 2900 | `"   $interval->d, $interval->h, $interval->i, $interval->s, $sign);"` |
|     - | 2901 | `" }"` |
|     - | 2902 | `" private static function __dtFromFormat($format, $datetime, $timezone, $class){"` |
|     - | 2903 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 2904 | `"  if( $timezone !== null ){"` |
|     - | 2905 | `"   $off = $timezone->getOffset(null);"` |
|     - | 2906 | `"   $name = $timezone->getName();"` |
|     - | 2907 | `"  }"` |
|     - | 2908 | `"  $r = __dt_from_format((string)$format, (string)$datetime, __dt_now(), $off);"` |
|     - | 2909 | `"  if( is_string($r) ){"` |
|     - | 2910 | `"   $lines = explode(\"\\n\", $r);"` |
|     - | 2911 | `"   $errs = [];"` |
|     - | 2912 | `"   $nl = count($lines);"` |
|     - | 2913 | `"   for( $k = 1; $k < $nl; $k++ ){"` |
|     - | 2914 | `"    $p = strpos($lines[$k], \"\\t\");"` |
|     - | 2915 | `"    $errs[(int)substr($lines[$k], 0, $p)] = substr($lines[$k], $p + 1);"` |
|     - | 2916 | `"   }"` |
|     - | 2917 | `"   DateTime::$__dtLastErr = ['warning_count' => 0, 'warnings' => [],"` |
|     - | 2918 | `"    'error_count' => (int)$lines[0], 'errors' => $errs];"` |
|     - | 2919 | `"   return false;"` |
|     - | 2920 | `"  }"` |
|     - | 2921 | `"  if( isset($r[4]) ){"` |
|     - | 2922 | `"   $warns = [];"` |
|     - | 2923 | `"   $wc = 0;"` |
|     - | 2924 | `"   for( $k = 4; isset($r[$k]); $k += 2 ){"` |
|     - | 2925 | `"    $warns[$r[$k]] = $r[$k + 1];"` |
|     - | 2926 | `"    $wc++;"` |
|     - | 2927 | `"   }"` |
|     - | 2928 | `"   DateTime::$__dtLastErr = ['warning_count' => $wc, 'warnings' => $warns,"` |
|     - | 2929 | `"    'error_count' => 0, 'errors' => []];"` |
|     - | 2930 | `"  }else{"` |
|     - | 2931 | `"   DateTime::$__dtLastErr = false;"` |
|     - | 2932 | `"  }"` |
|     - | 2933 | `"  $obj = new $class('@0');"` |
|     - | 2934 | `"  $obj->__dtTs = $r[0];"` |
|     - | 2935 | `"  if( $r[2] === 0 ){ $obj->__dtOff = $off; $obj->__dtName = $name; }"` |
|     - | 2936 | `"  elseif( $r[2] === 2 ){ $obj->__dtOff = 0; $obj->__dtName = 'Z'; }"` |
|     - | 2937 | `"  elseif( $r[2] === 3 ){ $obj->__dtOff = $r[1]; $obj->__dtName = $r[3]; }"` |
|     - | 2938 | `"  else { $obj->__dtOff = $r[1]; $obj->__dtName = $obj->__dtOffName($r[1]); }"` |
|     - | 2939 | `"  return $obj;"` |
|     - | 2940 | `" }"` |
|     - | 2941 | `" private static function __dtCopyOf($object, $class){"` |
|     - | 2942 | `"  $d = new $class('@0');"` |
|     - | 2943 | `"  $d->__dtTs = $object->getTimestamp();"` |
|     - | 2944 | `"  $d->__dtOff = $object->getOffset();"` |
|     - | 2945 | `"  $d->__dtName = $object->getTimezone()->getName();"` |
|     - | 2946 | `"  return $d;"` |
|     - | 2947 | `" }"` |
|     - | 2948 | `"}"` |
|     - | 2949 | `"class DateTime implements DateTimeInterface {"` |
|     - | 2950 | `" use __DtCoreT;"` |
|     - | 2951 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 2952 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 2953 | `" }"` |
|     - | 2954 | `" public function modify($modifier){"` |
|     - | 2955 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 2956 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTime::modify(): ' . $r); }"` |
|     - | 2957 | `"  $this->__dtTs = $r[0];"` |
|     - | 2958 | `"  return $this;"` |
|     - | 2959 | `" }"` |
|     - | 2960 | `" public function setTimestamp($timestamp){ $this->__dtTs = (int)$timestamp; return $this; }"` |
|     - | 2961 | `" public function setTimezone($timezone){"` |
|     - | 2962 | `"  $this->__dtOff = $timezone->getOffset($this);"` |
|     - | 2963 | `"  $this->__dtName = $timezone->getName();"` |
|     - | 2964 | `"  return $this;"` |
|     - | 2965 | `" }"` |
|     - | 2966 | `" public function setDate($year, $month, $day){"` |
|     - | 2967 | `"  $this->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 2968 | `"  return $this;"` |
|     - | 2969 | `" }"` |
|     - | 2970 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 2971 | `"  $this->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 2972 | `"  return $this;"` |
|     - | 2973 | `" }"` |
|     - | 2974 | `" public function add($interval){ $this->__dtTs = $this->__dtAddTs($interval, 1); return $this; }"` |
|     - | 2975 | `" public function sub($interval){ $this->__dtTs = $this->__dtAddTs($interval, -1); return $this; }"` |
|     - | 2976 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 2977 | `"  $this->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 2978 | `"  return $this;"` |
|     - | 2979 | `" }"` |
|     - | 2980 | `" public static $__dtLastErr = false;"` |
|     - | 2981 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 2982 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 2983 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTime');"` |
|     - | 2984 | `" }"` |
|     - | 2985 | `" public static function createFromImmutable($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 2986 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 2987 | `"}"` |
|     - | 2988 | `"class DateTimeImmutable implements DateTimeInterface {"` |
|     - | 2989 | `" use __DtCoreT;"` |
|     - | 2990 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 2991 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 2992 | `" }"` |
|     - | 2993 | `" public function modify($modifier){"` |
|     - | 2994 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 2995 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTimeImmutable::modify(): ' . $r); }"` |
|     - | 2996 | `"  $c = clone $this;"` |
|     - | 2997 | `"  $c->__dtTs = $r[0];"` |
|     - | 2998 | `"  return $c;"` |
|     - | 2999 | `" }"` |
|     - | 3000 | `" public function setTimestamp($timestamp){ $c = clone $this; $c->__dtTs = (int)$timestamp; return $c; }"` |
|     - | 3001 | `" public function setTimezone($timezone){"` |
|     - | 3002 | `"  $c = clone $this;"` |
|     - | 3003 | `"  $c->__dtOff = $timezone->getOffset($this);"` |
|     - | 3004 | `"  $c->__dtName = $timezone->getName();"` |
|     - | 3005 | `"  return $c;"` |
|     - | 3006 | `" }"` |
|     - | 3007 | `" public function setDate($year, $month, $day){"` |
|     - | 3008 | `"  $c = clone $this;"` |
|     - | 3009 | `"  $c->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 3010 | `"  return $c;"` |
|     - | 3011 | `" }"` |
|     - | 3012 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3013 | `"  $c = clone $this;"` |
|     - | 3014 | `"  $c->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 3015 | `"  return $c;"` |
|     - | 3016 | `" }"` |
|     - | 3017 | `" public function add($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, 1); return $c; }"` |
|     - | 3018 | `" public function sub($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, -1); return $c; }"` |
|     - | 3019 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 3020 | `"  $c = clone $this;"` |
|     - | 3021 | `"  $c->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 3022 | `"  return $c;"` |
|     - | 3023 | `" }"` |
|     - | 3024 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 3025 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 3026 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTimeImmutable');"` |
|     - | 3027 | `" }"` |
|     - | 3028 | `" public static function createFromMutable($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 3029 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 3030 | `"}"` |
|     - | 3031 | `"function date_create($datetime = 'now', $timezone = null){"` |
|     - | 3032 | `" try { return new DateTime($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 3033 | `"}"` |
|     - | 3034 | `"function date_create_immutable($datetime = 'now', $timezone = null){"` |
|     - | 3035 | `" try { return new DateTimeImmutable($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 3036 | `"}"` |
|     - | 3037 | `"class DateInterval {"` |
|     - | 3038 | `" public $y = 0;"` |
|     - | 3039 | `" public $m = 0;"` |
|     - | 3040 | `" public $d = 0;"` |
|     - | 3041 | `" public $h = 0;"` |
|     - | 3042 | `" public $i = 0;"` |
|     - | 3043 | `" public $s = 0;"` |
|     - | 3044 | `" public $f = 0;"` |
|     - | 3045 | `" public $invert = 0;"` |
|     - | 3046 | `" public $days = false;"` |
|     - | 3047 | `" public $from_string = false;"` |
|     - | 3048 | `" public function __construct($duration = 'P0D'){"` |
|     - | 3049 | `"  $dur = (string)$duration;"` |
|     - | 3050 | `"  $mm = null;"` |
|     - | 3051 | `"  if( strlen($dur) < 2 \|\| substr($dur, -1) === 'T'"` |
|     - | 3052 | `"   \|\| !preg_match('/^P(?:(\\d+)Y)?(?:(\\d+)M)?(?:(\\d+)W)?(?:(\\d+)D)?(?:T(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?)?$/', $dur, $mm) ){"` |
|     - | 3053 | `"   throw new DateMalformedIntervalStringException('Unknown or bad format (' . $dur . ')');"` |
|     - | 3054 | `"  }"` |
|     - | 3055 | `"  $this->y = (int)($mm[1] ?? 0);"` |
|     - | 3056 | `"  $this->m = (int)($mm[2] ?? 0);"` |
|     - | 3057 | `"  $this->d = (int)($mm[4] ?? 0) + 7 * (int)($mm[3] ?? 0);"` |
|     - | 3058 | `"  $this->h = (int)($mm[5] ?? 0);"` |
|     - | 3059 | `"  $this->i = (int)($mm[6] ?? 0);"` |
|     - | 3060 | `"  $this->s = (int)($mm[7] ?? 0);"` |
|     - | 3061 | `" }"` |
|     - | 3062 | `" public static function createFromDateString($datetime){"` |
|     - | 3063 | `"  $s = trim((string)$datetime);"` |
|     - | 3064 | `"  $iv = new DateInterval('P0D');"` |
|     - | 3065 | `"  $rest = $s;"` |
|     - | 3066 | `"  $any = false;"` |
|     - | 3067 | `"  while( $rest !== '' ){"` |
|     - | 3068 | `"   $mm = null;"` |
|     - | 3069 | `"   if( !preg_match('/^[\\s,+]*([+-]?\\d+)\\s*(sec\|secs\|second\|seconds\|min\|mins\|minute\|minutes\|hour\|hours\|day\|days\|week\|weeks\|fortnight\|fortnights\|month\|months\|year\|years)\\b/i', $rest, $mm) ){"` |
|     - | 3070 | `"    throw new DateMalformedIntervalStringException("` |
|     - | 3071 | `"     'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 3072 | `"   }"` |
|     - | 3073 | `"   $n = (int)$mm[1];"` |
|     - | 3074 | `"   $u = strtolower($mm[2]);"` |
|     - | 3075 | `"   if( $u === 'sec' \|\| $u === 'secs' \|\| $u === 'second' \|\| $u === 'seconds' ){ $iv->s += $n; }"` |
|     - | 3076 | `"   elseif( $u === 'min' \|\| $u === 'mins' \|\| $u === 'minute' \|\| $u === 'minutes' ){ $iv->i += $n; }"` |
|     - | 3077 | `"   elseif( $u === 'hour' \|\| $u === 'hours' ){ $iv->h += $n; }"` |
|     - | 3078 | `"   elseif( $u === 'day' \|\| $u === 'days' ){ $iv->d += $n; }"` |
|     - | 3079 | `"   elseif( $u === 'week' \|\| $u === 'weeks' ){ $iv->d += 7 * $n; }"` |
|     - | 3080 | `"   elseif( $u === 'fortnight' \|\| $u === 'fortnights' ){ $iv->d += 14 * $n; }"` |
|     - | 3081 | `"   elseif( $u === 'month' \|\| $u === 'months' ){ $iv->m += $n; }"` |
|     - | 3082 | `"   else { $iv->y += $n; }"` |
|     - | 3083 | `"   $any = true;"` |
|     - | 3084 | `"   $rest = ltrim(substr($rest, strlen($mm[0])));"` |
|     - | 3085 | `"  }"` |
|     - | 3086 | `"  if( !$any ){"` |
|     - | 3087 | `"   throw new DateMalformedIntervalStringException("` |
|     - | 3088 | `"    'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 3089 | `"  }"` |
|     - | 3090 | `"  return $iv;"` |
|     - | 3091 | `" }"` |
|     - | 3092 | `" public function format($format){"` |
|     - | 3093 | `"  $f = (string)$format;"` |
|     - | 3094 | `"  $out = '';"` |
|     - | 3095 | `"  $n = strlen($f);"` |
|     - | 3096 | `"  for( $k = 0; $k < $n; $k++ ){"` |
|     - | 3097 | `"   $c = $f[$k];"` |
|     - | 3098 | `"   if( $c !== '%' ){ $out .= $c; continue; }"` |
|     - | 3099 | `"   $k++;"` |
|     - | 3100 | `"   if( $k >= $n ){ $out .= '%'; break; }"` |
|     - | 3101 | `"   $t = $f[$k];"` |
|     - | 3102 | `"   if( $t === 'Y' ){ $out .= sprintf('%02d', $this->y); }"` |
|     - | 3103 | `"   elseif( $t === 'y' ){ $out .= $this->y; }"` |
|     - | 3104 | `"   elseif( $t === 'M' ){ $out .= sprintf('%02d', $this->m); }"` |
|     - | 3105 | `"   elseif( $t === 'm' ){ $out .= $this->m; }"` |
|     - | 3106 | `"   elseif( $t === 'D' ){ $out .= sprintf('%02d', $this->d); }"` |
|     - | 3107 | `"   elseif( $t === 'd' ){ $out .= $this->d; }"` |
|     - | 3108 | `"   elseif( $t === 'H' ){ $out .= sprintf('%02d', $this->h); }"` |
|     - | 3109 | `"   elseif( $t === 'h' ){ $out .= $this->h; }"` |
|     - | 3110 | `"   elseif( $t === 'I' ){ $out .= sprintf('%02d', $this->i); }"` |
|     - | 3111 | `"   elseif( $t === 'i' ){ $out .= $this->i; }"` |
|     - | 3112 | `"   elseif( $t === 'S' ){ $out .= sprintf('%02d', $this->s); }"` |
|     - | 3113 | `"   elseif( $t === 's' ){ $out .= $this->s; }"` |
|     - | 3114 | `"   elseif( $t === 'F' ){ $out .= sprintf('%06d', (int)round($this->f * 1000000)); }"` |
|     - | 3115 | `"   elseif( $t === 'f' ){ $out .= (int)round($this->f * 1000000); }"` |
|     - | 3116 | `"   elseif( $t === 'R' ){ $out .= $this->invert ? '-' : '+'; }"` |
|     - | 3117 | `"   elseif( $t === 'r' ){ $out .= $this->invert ? '-' : ''; }"` |
|     - | 3118 | `"   elseif( $t === 'a' ){ $out .= $this->days === false ? '(unknown)' : $this->days; }"` |
|     - | 3119 | `"   elseif( $t === '%' ){ $out .= '%'; }"` |
|     - | 3120 | `"   else { $out .= $t; }"` |
|     - | 3121 | `"  }"` |
|     - | 3122 | `"  return $out;"` |
|     - | 3123 | `" }"` |
|     - | 3124 | `"}"` |
|     - | 3125 | `"class DatePeriod implements IteratorAggregate {"` |
|     - | 3126 | `" const EXCLUDE_START_DATE = 1;"` |
|     - | 3127 | `" const INCLUDE_END_DATE = 2;"` |
|     - | 3128 | `" public $start = null;"` |
|     - | 3129 | `" public $current = null;"` |
|     - | 3130 | `" public $end = null;"` |
|     - | 3131 | `" public $interval = null;"` |
|     - | 3132 | `" public $recurrences = 1;"` |
|     - | 3133 | `" public $include_start_date = true;"` |
|     - | 3134 | `" public $include_end_date = false;"` |
|     - | 3135 | `" private $__dpN = null;"` |
|     - | 3136 | `" public function __construct($start, $interval = null, $end = null, $options = 0){"` |
|     - | 3137 | `"  if( is_string($start) ){"` |
|     - | 3138 | `"   $mm = null;"` |
|     - | 3139 | `"   if( !preg_match('/^R(\\d+)\\/(.+)\\/(P.+)$/', $start, $mm) ){"` |
|     - | 3140 | `"    throw new DateMalformedPeriodStringException("` |
|     - | 3141 | `"     'DatePeriod::__construct(): Unknown or bad format (' . $start . ')');"` |
|     - | 3142 | `"   }"` |
|     - | 3143 | `"   $options = is_int($interval) ? $interval : 0;"` |
|     - | 3144 | `"   $this->start = new DateTimeImmutable($mm[2]);"` |
|     - | 3145 | `"   $this->interval = new DateInterval($mm[3]);"` |
|     - | 3146 | `"   $this->__dpN = (int)$mm[1];"` |
|     - | 3147 | `"   $this->recurrences = $this->__dpN + 1;"` |
|     - | 3148 | `"  }else{"` |
|     - | 3149 | `"   $this->start = clone $start;"` |
|     - | 3150 | `"   $this->interval = $interval;"` |
|     - | 3151 | `"   if( is_int($end) ){"` |
|     - | 3152 | `"    $this->__dpN = $end;"` |
|     - | 3153 | `"    $this->recurrences = $end + 1;"` |
|     - | 3154 | `"   }else{"` |
|     - | 3155 | `"    $this->end = $end === null ? null : (clone $end);"` |
|     - | 3156 | `"   }"` |
|     - | 3157 | `"  }"` |
|     - | 3158 | `"  $this->include_start_date = !((int)$options & 1);"` |
|     - | 3159 | `"  $this->include_end_date = ((int)$options & 2) !== 0;"` |
|     - | 3160 | `" }"` |
|     - | 3161 | `" public static function createFromISO8601String($specification, $options = 0){"` |
|     - | 3162 | `"  return new DatePeriod((string)$specification, (int)$options);"` |
|     - | 3163 | `" }"` |
|     - | 3164 | `" public function getStartDate(){ return $this->start; }"` |
|     - | 3165 | `" public function getEndDate(){ return $this->end; }"` |
|     - | 3166 | `" public function getDateInterval(){ return $this->interval; }"` |
|     - | 3167 | `" public function getRecurrences(){ return $this->__dpN; }"` |
|     - | 3168 | `" public function getIterator(): Generator {"` |
|     - | 3169 | `"  $cur = $this->start;"` |
|     - | 3170 | `"  $iv = $this->interval;"` |
|     - | 3171 | `"  $k = 0;"` |
|     - | 3172 | `"  if( $this->end !== null ){"` |
|     - | 3173 | `"   $endTs = $this->end->getTimestamp();"` |
|     - | 3174 | `"   $first = true;"` |
|     - | 3175 | `"   while( true ){"` |
|     - | 3176 | `"    $ts = $cur->getTimestamp();"` |
|     - | 3177 | `"    if( $this->include_end_date ? ($ts > $endTs) : ($ts >= $endTs) ){ break; }"` |
|     - | 3178 | `"    if( !$first \|\| $this->include_start_date ){"` |
|     - | 3179 | `"     yield $k => (clone $cur);"` |
|     - | 3180 | `"     $k++;"` |
|     - | 3181 | `"    }"` |
|     - | 3182 | `"    $first = false;"` |
|     - | 3183 | `"    $next = clone $cur;"` |
|     - | 3184 | `"    $cur = $next->add($iv);"` |
|     - | 3185 | `"   }"` |
|     - | 3186 | `"   return;"` |
|     - | 3187 | `"  }"` |
|     - | 3188 | `"  $total = $this->__dpN + 1 + ($this->include_end_date ? 1 : 0);"` |
|     - | 3189 | `"  for( $j = 0; $j < $total; $j++ ){"` |
|     - | 3190 | `"   if( $j > 0 \|\| $this->include_start_date ){"` |
|     - | 3191 | `"    yield $k => (clone $cur);"` |
|     - | 3192 | `"    $k++;"` |
|     - | 3193 | `"   }"` |
|     - | 3194 | `"   $next = clone $cur;"` |
|     - | 3195 | `"   $cur = $next->add($iv);"` |
|     - | 3196 | `"  }"` |
|     - | 3197 | `" }"` |
|     - | 3198 | `"}"` |
|     - | 3199 | `"function date_format($object, $format){ return $object->format($format); }"` |
|     - | 3200 | `"function date_modify($object, $modifier){"` |
|     - | 3201 | `" try { return $object->modify($modifier); } catch (Exception $e) { return false; }"` |
|     - | 3202 | `"}"` |
|     - | 3203 | `"function date_add($object, $interval){ return $object->add($interval); }"` |
|     - | 3204 | `"function date_sub($object, $interval){ return $object->sub($interval); }"` |
|     - | 3205 | `"function date_diff($baseObject, $targetObject, $absolute = false){"` |
|     - | 3206 | `" return $baseObject->diff($targetObject, $absolute);"` |
|     - | 3207 | `"}"` |
|     - | 3208 | `"function date_timestamp_get($object){ return $object->getTimestamp(); }"` |
|     - | 3209 | `"function date_timestamp_set($object, $timestamp){ return $object->setTimestamp($timestamp); }"` |
|     - | 3210 | `"function date_timezone_get($object){ return $object->getTimezone(); }"` |
|     - | 3211 | `"function date_timezone_set($object, $timezone){ return $object->setTimezone($timezone); }"` |
|     - | 3212 | `"function date_offset_get($object){ return $object->getOffset(); }"` |
|     - | 3213 | `"function date_date_set($object, $year, $month, $day){ return $object->setDate($year, $month, $day); }"` |
|     - | 3214 | `"function date_time_set($object, $hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3215 | `" return $object->setTime($hour, $minute, $second, $microsecond);"` |
|     - | 3216 | `"}"` |
|     - | 3217 | `"function date_isodate_set($object, $year, $week, $dayOfWeek = 1){"` |
|     - | 3218 | `" return $object->setISODate($year, $week, $dayOfWeek);"` |
|     - | 3219 | `"}"` |
|     - | 3220 | `"function date_interval_create_from_date_string($datetime){"` |
|     - | 3221 | `" return DateInterval::createFromDateString($datetime);"` |
|     - | 3222 | `"}"` |
|     - | 3223 | `"function date_interval_format($object, $format){ return $object->format($format); }"` |
|     - | 3224 | `"function date_get_last_errors(){ return DateTime::getLastErrors(); }"` |
|     - | 3225 | `"function timezone_open($timezone){"` |
|     - | 3226 | `" try { return new DateTimeZone($timezone); } catch (Exception $e) { return false; }"` |
|     - | 3227 | `"}"` |
|     - | 3228 | `"function timezone_name_get($object){ return $object->getName(); }"` |
|     - | 3229 | `"function timezone_offset_get($object, $datetime){ return $object->getOffset($datetime); }"` |
|     - | 3230 | `/* int\|false strtotime(string $datetime, ?int $baseTimestamp = null). Rides the` |
|     - | 3231 | ` * same DtParse the DateTime constructor uses, so its format coverage is identical.` |
|     - | 3232 | ` * php: the EMPTY string is false, but whitespace-only is 'now'; a parse failure is` |
|     - | 3233 | ` * false (never an exception). The default timezone is treated as offset 0, exactly` |
|     - | 3234 | ` * as the DateTime constructor does for a null $timezone. */` |
|     - | 3235 | `"function strtotime($datetime, $baseTimestamp = null){"` |
|     - | 3236 | `" $s = (string)$datetime;"` |
|     - | 3237 | `" if( $s === '' ){ return false; }"` |
|     - | 3238 | `" $base = $baseTimestamp === null ? __dt_now() : (int)$baseTimestamp;"` |
|     - | 3239 | `" $r = __dt_parse($s, $base, 0);"` |
|     - | 3240 | `" return is_string($r) ? false : $r[0];"` |
|     - | 3241 | `"}"` |
|     - | 3242 | `;` |
|     - | 3243 | `/*` |
|     - | 3244 | ` * Install the DateTime family: thunks first, then the chunk. Called from` |
|     - | 3245 | ` * PH7_VmInit inside the bCompilingBuiltin window, after the Reflection` |
|     - | 3246 | ` * install (Exception must exist).` |
|     - | 3247 | ` */` |
|  3812 | 3248 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
|     5 | 3249 | `{` |
|     - | 3250 | `	static const struct {` |
|     - | 3251 | `		const char *zName;` |
|     - | 3252 | `		ProchHostFunction xFunc;` |
|     - | 3253 | `	} aFunc[] = {` |
|     - | 3254 | `		{ "__dt_now",    vm_builtin_dt_now },` |
|     - | 3255 | `		{ "__dt_default_tz", vm_builtin_dt_default_tz },` |
|     - | 3256 | `		{ "__dt_civil_add",  vm_builtin_dt_civil_add },` |
|     - | 3257 | `		{ "__dt_civil_diff", vm_builtin_dt_civil_diff },` |
|     - | 3258 | `		{ "__dt_isodate",    vm_builtin_dt_isodate },` |
|     - | 3259 | `		{ "__dt_from_format", vm_builtin_dt_from_format },` |
|     - | 3260 | `		{ "__dt_parse",  vm_builtin_dt_parse },` |
|     - | 3261 | `		{ "__dt_format", vm_builtin_dt_format },` |
|     - | 3262 | `		{ "__dt_make",   vm_builtin_dt_make },` |
|     - | 3263 | `	};` |
|     - | 3264 | `	sxu32 n;` |
|     - | 3265 | `	/* php's date.timezone default */` |
|  3817 | 3266 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|  3817 | 3267 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
| 38125 | 3268 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 34313 | 3269 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 17159 | 3270 | `	}` |
|  3817 | 3271 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zDateTimeLib,sizeof(zDateTimeLib)-1);` |
|     5 | 3272 | `}` |
|     - | 3273 |  |
|     - | 3274 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 3275 |  |
|     - | 3276 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 3277 | `/* Tiny build: no DateTime family (builtin layer disabled) */` |
|     - | 3278 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){` |
|     - | 3279 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|     - | 3280 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|     - | 3281 | `	return SXRET_OK;` |
|     - | 3282 | `}` |
|     - | 3283 | `#endif` |
|     - | 3284 |  |
