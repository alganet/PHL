# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1649/1951 lines (84.52%)

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
|   300 |   24 | `static void DtSytmFillOffset(Sytm *pSTm,time_t t)` |
|     1 |   25 | `{` |
|   451 |   26 | `	sxi64 iCivil = DtDaysFromCivil((sxi64)pSTm->tm_year,pSTm->tm_mon+1,pSTm->tm_mday) * 86400` |
|   300 |   27 | `		+ (sxi64)pSTm->tm_hour*3600 + (sxi64)pSTm->tm_min*60 + (sxi64)pSTm->tm_sec;` |
|   301 |   28 | `	pSTm->tm_gmtoff = (long)(iCivil - (sxi64)t);` |
|   301 |   29 | `}` |
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
|   410 |  416 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec)` |
|     1 |  417 | `{` |
|   411 |  418 | `	const char *zEnd = &zIn[nLen];` |
|     - |  419 | `	const char *zCur;` |
|     - |  420 | `	/* Start the format process */` |
|  1309 |  421 | `	for(;;){` |
|  2619 |  422 | `		if( zIn >= zEnd ){` |
|     - |  423 | `			/* No more input to process */` |
|   411 |  424 | `			break;` |
|     - |  425 | `		}` |
|  2209 |  426 | `		switch(zIn[0]){` |
|   107 |  427 | `		case 'd':` |
|     - |  428 | `			/* Day of the month, 2 digits with leading zeros */` |
|   215 |  429 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|   215 |  430 | `			break;` |
|    32 |  431 | `		case 'D':` |
|     - |  432 | `			/*A textual representation of a day, three letters*/` |
|    65 |  433 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    65 |  434 | `			ph7_result_string(pCtx,zCur,3);` |
|    65 |  435 | `			break;` |
|     4 |  436 | `		case 'j':` |
|     - |  437 | `			/*	Day of the month without leading zeros */` |
|     9 |  438 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|     9 |  439 | `			break;` |
|     3 |  440 | `		case 'l':` |
|     - |  441 | `			/* A full textual representation of the day of the week */` |
|     7 |  442 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|     7 |  443 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     7 |  444 | `			break;` |
|     1 |  445 | `		case 'N':{` |
|     - |  446 | `			/* ISO-8601 numeric representation of the day of the week */` |
|     3 |  447 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|     3 |  448 | `			break;` |
|     - |  449 | `				 }` |
|     1 |  450 | `		case 'w':` |
|     - |  451 | `			/*Numeric representation of the day of the week*/` |
|     3 |  452 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|     3 |  453 | `			break;` |
|     1 |  454 | `		case 'z':` |
|     - |  455 | `			/*The day of the year*/` |
|     3 |  456 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|     3 |  457 | `			break;` |
|     3 |  458 | `		case 'F':` |
|     - |  459 | `			/*A full textual representation of a month, such as January or March*/` |
|     7 |  460 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|     7 |  461 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     7 |  462 | `			break;` |
|   107 |  463 | `		case 'm':` |
|     - |  464 | `			/*Numeric representation of a month, with leading zeros*/` |
|   215 |  465 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|   215 |  466 | `			break;` |
|     1 |  467 | `		case 'M':` |
|     - |  468 | `			/*A short textual representation of a month, three letters*/` |
|     3 |  469 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|     3 |  470 | `			ph7_result_string(pCtx,zCur,3);` |
|     3 |  471 | `			break;` |
|     4 |  472 | `		case 'n':` |
|     - |  473 | `			/*Numeric representation of a month, without leading zeros*/` |
|     9 |  474 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|     9 |  475 | `			break;` |
|     1 |  476 | `		case 't':{` |
|     - |  477 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|     3 |  478 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|     3 |  479 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|   ! 0 |  480 | `				nDays = 28;` |
|   ! 0 |  481 | `			}` |
|     - |  482 | `			/*Number of days in the given month*/` |
|     3 |  483 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|     3 |  484 | `			break;` |
|     - |  485 | `				 }` |
|     1 |  486 | `		case 'L':{` |
|     3 |  487 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|     - |  488 | `			/* Whether it's a leap year */` |
|     3 |  489 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|     3 |  490 | `			break;` |
|     - |  491 | `				 }` |
|     7 |  492 | `		case 'o': case 'W': {` |
|     - |  493 | `			/* ISO-8601 week-numbering year / week number: both belong to the` |
|     - |  494 | `			 * year owning the Thursday of the civil week (php: 2024-12-31 is` |
|     - |  495 | `			 * 2025-W01, 2027-01-01 is 2026-W53). php pads W but not o. */` |
|    15 |  496 | `			sxi64 days = DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday);` |
|    15 |  497 | `			int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|    15 |  498 | `			sxi64 thu = days + (4 - isoDow);` |
|     - |  499 | `			sxi64 wy;` |
|     - |  500 | `			int wm,wd;` |
|    15 |  501 | `			DtCivilFromDays(thu,&wy,&wm,&wd);` |
|    15 |  502 | `			if( zIn[0] == 'o' ){` |
|     9 |  503 | `				ph7_result_string_format(pCtx,"%d",(int)wy);` |
|     5 |  504 | `			}else{` |
|    10 |  505 | `				ph7_result_string_format(pCtx,"%02d",` |
|     6 |  506 | `					(int)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1);` |
|     - |  507 | `			}` |
|    15 |  508 | `			break;` |
|     - |  509 | `				 }` |
|    97 |  510 | `		case 'Y':` |
|     - |  511 | `			/*	A full numeric representation of a year, 4 digits */` |
|   195 |  512 | `			ph7_result_string_format(pCtx,"%04d",pTm->tm_year);` |
|   195 |  513 | `			break;` |
|     2 |  514 | `		case 'X':` |
|     - |  515 | `			/* Expanded full year, always signed (php 8.2+): +2024 */` |
|     5 |  516 | `			ph7_result_string_format(pCtx,"%c%04d",` |
|     4 |  517 | `				pTm->tm_year < 0 ? '-' : '+',` |
|     4 |  518 | `				pTm->tm_year < 0 ? -pTm->tm_year : pTm->tm_year);` |
|     5 |  519 | `			break;` |
|     2 |  520 | `		case 'x':` |
|     - |  521 | `			/* Expanded year, signed only past 4 digits (php 8.2+) */` |
|     5 |  522 | `			if( pTm->tm_year > 9999 ){` |
|   ! 0 |  523 | `				ph7_result_string_format(pCtx,"+%d",pTm->tm_year);` |
|   ! 0 |  524 | `			}else{` |
|     5 |  525 | `				ph7_result_string_format(pCtx,"%04d",pTm->tm_year);` |
|     - |  526 | `			}` |
|     5 |  527 | `			break;` |
|     2 |  528 | `		case 'y':` |
|     - |  529 | `			/*A two digit representation of a year*/` |
|     5 |  530 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|     5 |  531 | `			break;` |
|     3 |  532 | `		case 'a':` |
|     - |  533 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|     7 |  534 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "pm" : "am",2);` |
|     7 |  535 | `			break;` |
|     3 |  536 | `		case 'A':` |
|     - |  537 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|     7 |  538 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "PM" : "AM",2);` |
|     7 |  539 | `			break;` |
|     1 |  540 | `		case 'B':{` |
|     - |  541 | `			/* Swatch Internet time: thousandths of the UTC+1 day */` |
|     4 |  542 | `			sxi64 iUtc = DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400` |
|     2 |  543 | `				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec` |
|     2 |  544 | `				- (sxi64)pTm->tm_gmtoff;` |
|     3 |  545 | `			sxi64 iBie = (iUtc + 3600) % 86400;` |
|     3 |  546 | `			if( iBie < 0 ){` |
|   ! 0 |  547 | `				iBie += 86400;` |
|   ! 0 |  548 | `			}` |
|     3 |  549 | `			ph7_result_string_format(pCtx,"%03d",(int)(iBie * 1000 / 86400));` |
|     3 |  550 | `			break;` |
|     - |  551 | `				 }` |
|     3 |  552 | `		case 'g':` |
|     - |  553 | `			/*	12-hour format of an hour without leading zeros*/` |
|    10 |  554 | `			ph7_result_string_format(pCtx,"%d",` |
|     6 |  555 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|     7 |  556 | `			break;` |
|     2 |  557 | `		case 'G':` |
|     - |  558 | `			/* 24-hour format of an hour without leading zeros */` |
|     5 |  559 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|     5 |  560 | `			break;` |
|     3 |  561 | `		case 'h':` |
|     - |  562 | `			/* 12-hour format of an hour with leading zeros */` |
|    10 |  563 | `			ph7_result_string_format(pCtx,"%02d",` |
|     6 |  564 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|     7 |  565 | `			break;` |
|    67 |  566 | `		case 'H':` |
|     - |  567 | `			/*	24-hour format of an hour with leading zeros */` |
|   135 |  568 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|   135 |  569 | `			break;` |
|    68 |  570 | `		case 'i':` |
|     - |  571 | `			/* 	Minutes with leading zeros */` |
|   137 |  572 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|   137 |  573 | `			break;` |
|    70 |  574 | `		case 's':` |
|     - |  575 | `			/* 	second with leading zeros */` |
|   141 |  576 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|   141 |  577 | `			break;` |
|    13 |  578 | `		case 'u':` |
|     - |  579 | `			/* 	Microseconds. date()/gmdate() have no sub-second part (uSec == 0);` |
|     - |  580 | `			 * 	DateTime::format passes its stored microseconds. */` |
|    27 |  581 | `			ph7_result_string_format(pCtx,"%06d",uSec);` |
|    27 |  582 | `			break;` |
|     4 |  583 | `		case 'v':` |
|     - |  584 | `			/* 	Milliseconds */` |
|     9 |  585 | `			ph7_result_string_format(pCtx,"%03d",uSec/1000);` |
|     9 |  586 | `			break;` |
|     1 |  587 | `		case 'S':{` |
|     - |  588 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|     - |  589 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|     3 |  590 | `			int v = pTm->tm_mday;` |
|     3 |  591 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|     3 |  592 | `			break;` |
|     - |  593 | `				 }` |
|     9 |  594 | `		case 'e':` |
|     - |  595 | `			/* 	Timezone identifier */` |
|    19 |  596 | `			zCur = pTm->tm_zone;` |
|    19 |  597 | `			if( zCur == 0 ){` |
|     - |  598 | `				/* date()-family fills: the script default timezone */` |
|     7 |  599 | `				zCur = pCtx->pVm->zDefTz;` |
|     3 |  600 | `			}` |
|    19 |  601 | `			ph7_result_string(pCtx,zCur,-1);` |
|    19 |  602 | `			break;` |
|     4 |  603 | `		case 'T':{` |
|     - |  604 | `			/* Timezone abbreviation: "UTC" for offset 0, "GMT+0530" for a` |
|     - |  605 | `			 * fixed offset (php's shape). PHL has no tz database, so the` |
|     - |  606 | `			 * zone-name path only ever sees UTC/GMT, uppercased. */` |
|     - |  607 | `			const char *z;` |
|     9 |  608 | `			if( pTm->tm_gmtoff != 0 ){` |
|     3 |  609 | `				long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     3 |  610 | `				ph7_result_string_format(pCtx,"GMT%c%02d%02d",` |
|     2 |  611 | `					pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|     3 |  612 | `				break;` |
|     - |  613 | `			}` |
|     7 |  614 | `			z = pTm->tm_zone ? pTm->tm_zone : pCtx->pVm->zDefTz;` |
|    25 |  615 | `			while( *z ){` |
|    19 |  616 | `				int c = (unsigned char)*z;` |
|    19 |  617 | `				if( c >= 'a' && c <= 'z' ){` |
|   ! 0 |  618 | `					c -= 'a' - 'A';` |
|   ! 0 |  619 | `				}` |
|    19 |  620 | `				ph7_result_string_format(pCtx,"%c",c);` |
|    19 |  621 | `				z++;` |
|     1 |  622 | `			}` |
|     7 |  623 | `			break;` |
|     - |  624 | `				 }` |
|     1 |  625 | `		case 'I':` |
|     - |  626 | `			/* Whether or not the date is in daylight saving time. Use the` |
|     - |  627 | `			 * broken-down time's own tm_isdst (as every other platform does):` |
|     - |  628 | `			 * the old Windows _get_daylight() override reported whether the` |
|     - |  629 | `			 * timezone observes DST at all, not whether THIS date is in it. */` |
|     3 |  630 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|     3 |  631 | `			break;` |
|     2 |  632 | `		case 'r':{` |
|     - |  633 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200 */` |
|     5 |  634 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     5 |  635 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d %c%02d%02d",` |
|     2 |  636 | `				SyTimeGetDay(pTm->tm_wday),` |
|     2 |  637 | `				pTm->tm_mday,` |
|     2 |  638 | `				SyTimeGetMonth(pTm->tm_mon),` |
|     2 |  639 | `				pTm->tm_year,` |
|     2 |  640 | `				pTm->tm_hour,` |
|     2 |  641 | `				pTm->tm_min,` |
|     2 |  642 | `				pTm->tm_sec,` |
|     4 |  643 | `				pTm->tm_gmtoff < 0 ? '-' : '+',` |
|     4 |  644 | `				(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  645 | `				);` |
|     5 |  646 | `			break;` |
|     - |  647 | `				 }` |
|     4 |  648 | `		case 'U':` |
|     - |  649 | `			/* Seconds since the Unix Epoch FOR THIS Sytm (php: the timestamp` |
|     - |  650 | `			 * being formatted — pre-fix this printed time(0) regardless of the` |
|     - |  651 | `			 * date under format). */` |
|    13 |  652 | `			ph7_result_string_format(pCtx,"%qd",` |
|     8 |  653 | `				DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400` |
|     8 |  654 | `				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec` |
|     8 |  655 | `				- (sxi64)pTm->tm_gmtoff);` |
|     9 |  656 | `			break;` |
|     3 |  657 | `		case 'O':{` |
|     - |  658 | `			/* Difference to GMT without colon: +0530 (php) */` |
|     7 |  659 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     7 |  660 | `			ph7_result_string_format(pCtx,"%c%02d%02d",` |
|     6 |  661 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|     7 |  662 | `			break;` |
|     - |  663 | `				 }` |
|     5 |  664 | `		case 'P':{` |
|     - |  665 | `			/* Difference to GMT with colon: +05:30 (php) */` |
|    11 |  666 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    11 |  667 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|    10 |  668 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    11 |  669 | `			break;` |
|     - |  670 | `				 }` |
|     2 |  671 | `		case 'p':{` |
|     - |  672 | `			/* Like P, but "Z" for UTC (php 8.0+) */` |
|     - |  673 | `			long a;` |
|     5 |  674 | `			if( pTm->tm_gmtoff == 0 ){` |
|     3 |  675 | `				ph7_result_string(pCtx,"Z",1);` |
|     3 |  676 | `				break;` |
|     - |  677 | `			}` |
|     3 |  678 | `			a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     3 |  679 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|     2 |  680 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|     3 |  681 | `			break;` |
|     - |  682 | `				 }` |
|     2 |  683 | `		case 'Z':` |
|     - |  684 | `			/* Timezone offset in seconds, plain integer (php) */` |
|     5 |  685 | `			ph7_result_string_format(pCtx,"%d",(int)pTm->tm_gmtoff);` |
|     5 |  686 | `			break;` |
|     6 |  687 | `		case 'c':{` |
|     - |  688 | `			/* 	ISO 8601 date: 2004-02-12T15:19:21+00:00 (php) */` |
|    13 |  689 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    19 |  690 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",` |
|     6 |  691 | `				pTm->tm_year,` |
|    12 |  692 | `				pTm->tm_mon+1,` |
|     6 |  693 | `				pTm->tm_mday,` |
|     6 |  694 | `				pTm->tm_hour,` |
|     6 |  695 | `				pTm->tm_min,` |
|     6 |  696 | `				pTm->tm_sec,` |
|    12 |  697 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  698 | `				);` |
|    13 |  699 | `			break;` |
|     - |  700 | `				 }` |
|     4 |  701 | `		case '\\':` |
|     9 |  702 | `			zIn++;` |
|     - |  703 | `			/* Expand verbatim */` |
|     9 |  704 | `			if( zIn < zEnd ){` |
|     9 |  705 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     4 |  706 | `			}` |
|     9 |  707 | `			break;` |
|   448 |  708 | `		default:` |
|     - |  709 | `			/* Unknown format specifer,expand verbatim */` |
|   897 |  710 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|   896 |  711 | `			break;` |
|     - |  712 | `		}` |
|     - |  713 | `		/* Point to the next character */` |
|  2209 |  714 | `		zIn++;` |
|     1 |  715 | `	}` |
|   411 |  716 | `	return SXRET_OK;` |
|     1 |  717 | `}` |
|     - |  718 | `/*` |
|     - |  719 | ` * PH7 implementation of the strftime() function.` |
|     - |  720 | ` * The following formats are supported:` |
|     - |  721 | ` * %a 	An abbreviated textual representation of the day` |
|     - |  722 | ` * %A 	A full textual representation of the day` |
|     - |  723 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|     - |  724 | ` * %e 	Day of the month, with a space preceding single digits.` |
|     - |  725 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|     - |  726 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|     - |  727 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|     - |  728 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|     - |  729 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|     - |  730 | ` *   4 weekdays, with Monday being the start of the week.` |
|     - |  731 | ` * %W 	A numeric representation of the week of the year` |
|     - |  732 | ` * %b 	Abbreviated month name, based on the locale` |
|     - |  733 | ` * %B 	Full month name, based on the locale` |
|     - |  734 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|     - |  735 | ` * %m 	Two digit representation of the month` |
|     - |  736 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|     - |  737 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|     - |  738 | ` * %G 	The full four-digit version of %g` |
|     - |  739 | ` * %y 	Two digit representation of the year` |
|     - |  740 | ` * %Y 	Four digit representation for the year` |
|     - |  741 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|     - |  742 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|     - |  743 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|     - |  744 | ` * %M 	Two digit representation of the minute` |
|     - |  745 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|     - |  746 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|     - |  747 | ` * %r 	Same as "%I:%M:%S %p"` |
|     - |  748 | ` * %R 	Same as "%H:%M"` |
|     - |  749 | ` * %S 	Two digit representation of the second` |
|     - |  750 | ` * %T 	Same as "%H:%M:%S"` |
|     - |  751 | ` * %X 	Preferred time representation based on locale, without the date` |
|     - |  752 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|     - |  753 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|     - |  754 | ` * %c 	Preferred date and time stamp based on local` |
|     - |  755 | ` * %D 	Same as "%m/%d/%y"` |
|     - |  756 | ` * %F 	Same as "%Y-%m-%d"` |
|     - |  757 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|     - |  758 | ` * %x 	Preferred date representation based on locale, without the time` |
|     - |  759 | ` * %n 	A newline character ("\n")` |
|     - |  760 | ` * %t 	A Tab character ("\t")` |
|     - |  761 | ` * %% 	A literal percentage character ("%")` |
|     - |  762 | ` */` |
|    18 |  763 | `static int PH7_Strftime(` |
|     - |  764 | `	ph7_context *pCtx,  /* Call context */` |
|     - |  765 | `	const char *zIn,    /* Input string */` |
|     - |  766 | `	int nLen,           /* Input length */` |
|     - |  767 | `	Sytm *pTm           /* Parse of the given time */` |
|     - |  768 | `	)` |
|     1 |  769 | `{` |
|    19 |  770 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|     - |  771 | `	int c;` |
|     - |  772 | `	/* Start the format process */` |
|    20 |  773 | `	for(;;){` |
|    41 |  774 | `		zCur = zIn;` |
|    45 |  775 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|     5 |  776 | `			zIn++;` |
|     1 |  777 | `		}` |
|    41 |  778 | `		if( zIn > zCur ){` |
|     - |  779 | `			/* Consume input verbatim */` |
|     5 |  780 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     2 |  781 | `		}` |
|    41 |  782 | `		zIn++; /* Jump the percent sign */` |
|    41 |  783 | `		if( zIn >= zEnd ){` |
|     - |  784 | `			/* No more input to process */` |
|    19 |  785 | `			break;` |
|     - |  786 | `		}` |
|    23 |  787 | `		c = zIn[0];` |
|     - |  788 | `		/* Act according to the current specifer */` |
|    23 |  789 | `		switch(c){` |
|   ! 0 |  790 | `		case '%':` |
|     - |  791 | `			/* A literal percentage character ("%") */` |
|   ! 0 |  792 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|   ! 0 |  793 | `			break;` |
|   ! 0 |  794 | `		case 't':` |
|     - |  795 | `			/* A Tab character */` |
|   ! 0 |  796 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|   ! 0 |  797 | `			break;` |
|   ! 0 |  798 | `		case 'n':` |
|     - |  799 | `			/* A newline character */` |
|   ! 0 |  800 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|   ! 0 |  801 | `			break;` |
|     1 |  802 | `		case 'a':` |
|     - |  803 | `			/* An abbreviated textual representation of the day */` |
|     3 |  804 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|     3 |  805 | `			break;` |
|   ! 0 |  806 | `		case 'A':` |
|     - |  807 | `			/* A full textual representation of the day */` |
|   ! 0 |  808 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|   ! 0 |  809 | `			break;` |
|   ! 0 |  810 | `		case 'e':` |
|     - |  811 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|   ! 0 |  812 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|   ! 0 |  813 | `			break;` |
|     2 |  814 | `		case 'd':` |
|     - |  815 | `			/* Two-digit day of the month (with leading zeros) */` |
|     5 |  816 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|     5 |  817 | `			break;` |
|   ! 0 |  818 | `		case 'j':` |
|     - |  819 | `			/*The day of the year,3 digits with leading zeros*/` |
|   ! 0 |  820 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|   ! 0 |  821 | `			break;` |
|   ! 0 |  822 | `		case 'u':` |
|     - |  823 | `			/* ISO-8601 numeric representation of the day of the week */` |
|   ! 0 |  824 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|   ! 0 |  825 | `			break;` |
|   ! 0 |  826 | `		case 'w':` |
|     - |  827 | `			/* Numeric representation of the day of the week */` |
|   ! 0 |  828 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|   ! 0 |  829 | `			break;` |
|   ! 0 |  830 | `		case 'b':` |
|     - |  831 | `		case 'h':` |
|     - |  832 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|   ! 0 |  833 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|   ! 0 |  834 | `			break;` |
|   ! 0 |  835 | `		case 'B':` |
|     - |  836 | `			/* Full month name (Not based on locale) */` |
|   ! 0 |  837 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|   ! 0 |  838 | `			break;` |
|     2 |  839 | `		case 'm':` |
|     - |  840 | `			/*Numeric representation of a month, with leading zeros*/` |
|     5 |  841 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     5 |  842 | `			break;` |
|   ! 0 |  843 | `		case 'C':` |
|     - |  844 | `			/* Two digit representation of the century */` |
|   ! 0 |  845 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|   ! 0 |  846 | `			break;` |
|   ! 0 |  847 | `		case 'y':` |
|     - |  848 | `		case 'g':` |
|     - |  849 | `			/* Two digit representation of the year */` |
|   ! 0 |  850 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|   ! 0 |  851 | `			break;` |
|     3 |  852 | `		case 'Y':` |
|     - |  853 | `		case 'G':` |
|     - |  854 | `			/* Four digit representation of the year */` |
|     7 |  855 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     7 |  856 | `			break;` |
|   ! 0 |  857 | `		case 'I':` |
|     - |  858 | `			/* 12-hour format of an hour with leading zeros */` |
|   ! 0 |  859 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|   ! 0 |  860 | `			break;` |
|   ! 0 |  861 | `		case 'l':` |
|     - |  862 | `			/* 12-hour format of an hour with leading space */` |
|   ! 0 |  863 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|   ! 0 |  864 | `			break;` |
|     1 |  865 | `		case 'H':` |
|     - |  866 | `			/* 24-hour format of an hour with leading zeros */` |
|     3 |  867 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|     3 |  868 | `			break;` |
|     1 |  869 | `		case 'M':` |
|     - |  870 | `			/* Minutes with leading zeros */` |
|     3 |  871 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|     3 |  872 | `			break;` |
|   ! 0 |  873 | `		case 'S':` |
|     - |  874 | `			/* Seconds with leading zeros */` |
|   ! 0 |  875 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|   ! 0 |  876 | `			break;` |
|   ! 0 |  877 | `		case 'z':` |
|     - |  878 | `		case 'Z':` |
|     - |  879 | `			/* 	Timezone identifier */` |
|   ! 0 |  880 | `			zCur = pTm->tm_zone;` |
|   ! 0 |  881 | `			if( zCur == 0 ){` |
|     - |  882 | `				/* date()-family fills: the script default timezone */` |
|   ! 0 |  883 | `				zCur = pCtx->pVm->zDefTz;` |
|   ! 0 |  884 | `			}` |
|   ! 0 |  885 | `			ph7_result_string(pCtx,zCur,-1);` |
|   ! 0 |  886 | `			break;` |
|   ! 0 |  887 | `		case 'T':` |
|     - |  888 | `		case 'X':` |
|     - |  889 | `			/* Same as "%H:%M:%S" */` |
|   ! 0 |  890 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|   ! 0 |  891 | `			break;` |
|   ! 0 |  892 | `		case 'R':` |
|     - |  893 | `			/* Same as "%H:%M" */` |
|   ! 0 |  894 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|   ! 0 |  895 | `			break;` |
|   ! 0 |  896 | `		case 'P':` |
|     - |  897 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|   ! 0 |  898 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|   ! 0 |  899 | `			break;` |
|   ! 0 |  900 | `		case 'p':` |
|     - |  901 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|   ! 0 |  902 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|   ! 0 |  903 | `			break;` |
|   ! 0 |  904 | `		case 'r':` |
|     - |  905 | `			/* Same as "%I:%M:%S %p" */` |
|   ! 0 |  906 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|   ! 0 |  907 | `				1+(pTm->tm_hour%12),` |
|   ! 0 |  908 | `				pTm->tm_min,` |
|   ! 0 |  909 | `				pTm->tm_sec,` |
|   ! 0 |  910 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|     - |  911 | `				);` |
|   ! 0 |  912 | `			break;` |
|     1 |  913 | `		case 'D':` |
|     - |  914 | `		case 'x':` |
|     - |  915 | `			/* Same as "%m/%d/%y" */` |
|     4 |  916 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|     2 |  917 | `				pTm->tm_mon+1,` |
|     1 |  918 | `				pTm->tm_mday,` |
|     2 |  919 | `				pTm->tm_year%100` |
|     - |  920 | `				);` |
|     3 |  921 | `			break;` |
|   ! 0 |  922 | `		case 'F':` |
|     - |  923 | `			/* Same as "%Y-%m-%d" */` |
|   ! 0 |  924 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|   ! 0 |  925 | `				pTm->tm_year,` |
|   ! 0 |  926 | `				pTm->tm_mon+1,` |
|   ! 0 |  927 | `				pTm->tm_mday` |
|     - |  928 | `				);` |
|   ! 0 |  929 | `			break;` |
|   ! 0 |  930 | `		case 'c':` |
|   ! 0 |  931 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|   ! 0 |  932 | `				pTm->tm_year,` |
|   ! 0 |  933 | `				pTm->tm_mon+1,` |
|   ! 0 |  934 | `				pTm->tm_mday,` |
|   ! 0 |  935 | `				pTm->tm_hour,` |
|   ! 0 |  936 | `				pTm->tm_min,` |
|   ! 0 |  937 | `				pTm->tm_sec` |
|     - |  938 | `				);` |
|   ! 0 |  939 | `			break;` |
|   ! 0 |  940 | `		case 's':{` |
|     - |  941 | `			time_t tt;` |
|     - |  942 | `			/* Seconds since the Unix Epoch */` |
|   ! 0 |  943 | `			time(&tt);` |
|   ! 0 |  944 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|   ! 0 |  945 | `			break;` |
|     - |  946 | `				 }` |
|   ! 0 |  947 | `		default:` |
|     - |  948 | `			/* unknown specifer,simply ignore*/` |
|   ! 0 |  949 | `			break;` |
|     - |  950 | `		}` |
|     - |  951 | `		/* Advance the cursor */` |
|    23 |  952 | `		zIn++;` |
|     1 |  953 | `	}` |
|    19 |  954 | `	return SXRET_OK;` |
|     1 |  955 | `}` |
|     - |  956 | `/*` |
|     - |  957 | ` * Resolve a date()/gmdate() $timestamp argument under php 8's ?int weak ZPP:` |
|     - |  958 | ` *   - null            -> *pbUseNow = 1 (caller uses the current time)` |
|     - |  959 | ` *   - int/bool/float  -> coerce to a Unix timestamp (float truncates; php's` |
|     - |  960 | ` *                        float->int precision E_DEPRECATED is not emitted, §3.7)` |
|     - |  961 | ` *   - numeric string  -> coerce via php's is_numeric_string grammar` |
|     - |  962 | ` *                        (RangeStrToNumber: " 100 "/"1e3"/".5"/"+5" ok)` |
|     - |  963 | ` *   - anything else (non-numeric string, array, object, resource)` |
|     - |  964 | ` *                     -> catchable TypeError, byte-exact with php.` |
|     - |  965 | ` * Returns PH7_OK with *pbUseNow / *pT set, or the PH7_VmThrowException status.` |
|     - |  966 | ` */` |
|   194 |  967 | `static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)` |
|     1 |  968 | `{` |
|     - |  969 | `	char zBuf[64];` |
|   195 |  970 | `	*pbUseNow = 0;` |
|   195 |  971 | `	if( ph7_value_is_null(pArg) ){` |
|     3 |  972 | `		*pbUseNow = 1;` |
|     3 |  973 | `		return PH7_OK;` |
|     - |  974 | `	}` |
|   193 |  975 | `	if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_float(pArg) ){` |
|   175 |  976 | `		*pT = (time_t)ph7_value_to_int64(pArg);` |
|   175 |  977 | `		return PH7_OK;` |
|     - |  978 | `	}` |
|    19 |  979 | `	if( ph7_value_is_string(pArg) ){` |
|     - |  980 | `		int nStr;` |
|    19 |  981 | `		const char *zStr = ph7_value_to_string(pArg,&nStr);` |
|     - |  982 | `		sxi64 iLong; double dReal;` |
|    19 |  983 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|    19 |  984 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|     3 |  985 | `			*pT = (time_t)dReal;` |
|     6 |  986 | `			return PH7_OK;` |
|     - |  987 | `		}` |
|    17 |  988 | `		if( iKind == RANGE_IN_LONG ){` |
|     7 |  989 | `			*pT = (time_t)iLong;` |
|     7 |  990 | `			return PH7_OK;` |
|     - |  991 | `		}` |
|     - |  992 | `		/* Not a numeric string: fall through to the TypeError. */` |
|     5 |  993 | `	}` |
|    16 |  994 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  995 | `		"%s(): Argument #2 ($timestamp) must be of type ?int, %s given",` |
|     5 |  996 | `		ph7_function_name(pCtx),VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|    98 |  997 | `}` |
|     - |  998 | `/*` |
|     - |  999 | ` * string date(string $format [, int $timestamp = time() ] )` |
|     - | 1000 | ` *  Returns a string formatted according to the given format string using` |
|     - | 1001 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|     - | 1002 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|     - | 1003 | ` * Parameters` |
|     - | 1004 | ` *  $format` |
|     - | 1005 | ` *   The format of the outputted date string (See code above)` |
|     - | 1006 | ` * $timestamp` |
|     - | 1007 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1008 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1009 | ` *   In other words, it defaults to the value of time().` |
|     - | 1010 | ` * Return` |
|     - | 1011 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - | 1012 | ` */` |
|   136 | 1013 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1014 | `{` |
|     - | 1015 | `	const char *zFormat;` |
|     - | 1016 | `	int nLen;` |
|     - | 1017 | `	Sytm sTm;` |
|   137 | 1018 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1019 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1020 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1021 | `		return PH7_OK;` |
|     - | 1022 | `	}` |
|   137 | 1023 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   137 | 1024 | `	if( nLen < 1 ){` |
|     - | 1025 | `		/* Don't bother processing return the empty string */` |
|   ! 0 | 1026 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1027 | `	}` |
|   137 | 1028 | `	if( nArg < 2 ){` |
|     - | 1029 | `#ifdef __WINNT__` |
|     - | 1030 | `		SYSTEMTIME sOS;` |
|     1 | 1031 | `		GetSystemTime(&sOS);` |
|     1 | 1032 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1033 | `#else` |
|     - | 1034 | `		struct tm *pTm;` |
|     - | 1035 | `		time_t t;` |
|    30 | 1036 | `		time(&t);` |
|    30 | 1037 | `		pTm = gmtime(&t);` |
|    30 | 1038 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    30 | 1039 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1040 | `#endif` |
|    16 | 1041 | `	}else{` |
|     - | 1042 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|   107 | 1043 | `		time_t t = 0;` |
|     - | 1044 | `		struct tm *pTm;` |
|     - | 1045 | `		int bUseNow;` |
|   107 | 1046 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|   107 | 1047 | `		if( rc != PH7_OK ){` |
|     9 | 1048 | `			return rc;` |
|     - | 1049 | `		}` |
|    99 | 1050 | `		if( bUseNow ){` |
|   ! 0 | 1051 | `			time(&t);` |
|   ! 0 | 1052 | `		}` |
|    99 | 1053 | `		pTm = gmtime(&t);` |
|    99 | 1054 | `		if( pTm == 0 ){` |
|   ! 0 | 1055 | `			time(&t);` |
|   ! 0 | 1056 | `			pTm = gmtime(&t);` |
|   ! 0 | 1057 | `		}` |
|    99 | 1058 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    99 | 1059 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1060 | `	}` |
|     - | 1061 | `	/* Format the given string */` |
|   129 | 1062 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|   129 | 1063 | `	return PH7_OK;` |
|    69 | 1064 | `}` |
|     - | 1065 | `/*` |
|     - | 1066 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|     - | 1067 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|     - | 1068 | ` * Parameters` |
|     - | 1069 | ` *  $format` |
|     - | 1070 | ` *   The format of the outputted date string (See code above)` |
|     - | 1071 | ` * $timestamp` |
|     - | 1072 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1073 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1074 | ` *   In other words, it defaults to the value of time().` |
|     - | 1075 | ` * Return` |
|     - | 1076 | ` * Returns a string formatted according format using the given timestamp` |
|     - | 1077 | ` * or the current local time if no timestamp is given.` |
|     - | 1078 | ` */` |
|    18 | 1079 | `PH7_PRIVATE int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1080 | `{` |
|     - | 1081 | `	const char *zFormat;` |
|     - | 1082 | `	int nLen;` |
|     - | 1083 | `	Sytm sTm;` |
|     - | 1084 | `	/* The php 8.1 whole-function deprecation is declared in aBuiltinDeprecated[] and` |
|     - | 1085 | `	 * emitted at the OP_CALL choke point, which is what puts it BEFORE the` |
|     - | 1086 | `	 * ArgumentCountError for a no-arg call — php's order. */` |
|    19 | 1087 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1088 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1089 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1090 | `		return PH7_OK;` |
|     - | 1091 | `	}` |
|    19 | 1092 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 1093 | `	if( nLen < 1 ){` |
|     - | 1094 | `		/* Don't bother processing return FALSE */` |
|   ! 0 | 1095 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1096 | `	}` |
|    19 | 1097 | `	if( nArg < 2 ){` |
|     - | 1098 | `#ifdef __WINNT__` |
|     - | 1099 | `		SYSTEMTIME sOS;` |
|     1 | 1100 | `		GetSystemTime(&sOS);` |
|     1 | 1101 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1102 | `#else` |
|     - | 1103 | `		struct tm *pTm;` |
|     - | 1104 | `		time_t t;` |
|    16 | 1105 | `		time(&t);` |
|    16 | 1106 | `		pTm = gmtime(&t);` |
|    16 | 1107 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    16 | 1108 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1109 | `#endif` |
|     9 | 1110 | `	}else{` |
|     - | 1111 | `		/* Use the given timestamp */` |
|     - | 1112 | `		time_t t;` |
|     - | 1113 | `		struct tm *pTm;` |
|     3 | 1114 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     3 | 1115 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     3 | 1116 | `			pTm = gmtime(&t);` |
|     3 | 1117 | `			if( pTm == 0 ){` |
|   ! 0 | 1118 | `				time(&t);` |
|   ! 0 | 1119 | `			}` |
|     2 | 1120 | `		}else{` |
|   ! 0 | 1121 | `			time(&t);` |
|     - | 1122 | `		}` |
|     3 | 1123 | `		pTm = gmtime(&t);` |
|     3 | 1124 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     3 | 1125 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1126 | `	}` |
|     - | 1127 | `	/* Format the given string */` |
|    19 | 1128 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|    19 | 1129 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     - | 1130 | `		/* Nothing was formatted,return FALSE */` |
|   ! 0 | 1131 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1132 | `	}` |
|    19 | 1133 | `	return PH7_OK;` |
|    10 | 1134 | `}` |
|     - | 1135 | `/*` |
|     - | 1136 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|     - | 1137 | ` *  Identical to the date() function except that the time returned` |
|     - | 1138 | ` *  is Greenwich Mean Time (GMT).` |
|     - | 1139 | ` * Parameters` |
|     - | 1140 | ` *  $format` |
|     - | 1141 | ` *  The format of the outputted date string (See code above)` |
|     - | 1142 | ` *  $timestamp` |
|     - | 1143 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1144 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1145 | ` *   In other words, it defaults to the value of time().` |
|     - | 1146 | ` * Return` |
|     - | 1147 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - | 1148 | ` */` |
|   102 | 1149 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1150 | `{` |
|     - | 1151 | `	const char *zFormat;` |
|     - | 1152 | `	int nLen;` |
|     - | 1153 | `	Sytm sTm;` |
|   103 | 1154 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1155 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1156 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1157 | `		return PH7_OK;` |
|     - | 1158 | `	}` |
|   103 | 1159 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   103 | 1160 | `	if( nLen < 1 ){` |
|     - | 1161 | `		/* Don't bother processing return the empty string */` |
|   ! 0 | 1162 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1163 | `	}` |
|   103 | 1164 | `	if( nArg < 2 ){` |
|     - | 1165 | `#ifdef __WINNT__` |
|     - | 1166 | `		SYSTEMTIME sOS;` |
|     1 | 1167 | `		GetSystemTime(&sOS);` |
|     1 | 1168 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1169 | `#else` |
|     - | 1170 | `		struct tm *pTm;` |
|     - | 1171 | `		time_t t;` |
|    14 | 1172 | `		time(&t);` |
|    14 | 1173 | `		pTm = gmtime(&t);` |
|    14 | 1174 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    14 | 1175 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1176 | `#endif` |
|     8 | 1177 | `	}else{` |
|     - | 1178 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|    89 | 1179 | `		time_t t = 0;` |
|     - | 1180 | `		struct tm *pTm;` |
|     - | 1181 | `		int bUseNow;` |
|    89 | 1182 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|    89 | 1183 | `		if( rc != PH7_OK ){` |
|     3 | 1184 | `			return rc;` |
|     - | 1185 | `		}` |
|    87 | 1186 | `		if( bUseNow ){` |
|     3 | 1187 | `			time(&t);` |
|     1 | 1188 | `		}` |
|    87 | 1189 | `		pTm = gmtime(&t);` |
|    87 | 1190 | `		if( pTm == 0 ){` |
|   ! 0 | 1191 | `			time(&t);` |
|   ! 0 | 1192 | `			pTm = gmtime(&t);` |
|   ! 0 | 1193 | `		}` |
|    87 | 1194 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    87 | 1195 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1196 | `	}` |
|     - | 1197 | `	/* Format the given string */` |
|   101 | 1198 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|   101 | 1199 | `	return PH7_OK;` |
|    52 | 1200 | `}` |
|     - | 1201 | `/*` |
|     - | 1202 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|     - | 1203 | ` *  Return the local time.` |
|     - | 1204 | ` * Parameter` |
|     - | 1205 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1206 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - | 1207 | ` *     In other words, it defaults to the value of time().` |
|     - | 1208 | ` * $is_associative` |
|     - | 1209 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|     - | 1210 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|     - | 1211 | ` *   array containing all the different elements of the structure returned by the C function` |
|     - | 1212 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|     - | 1213 | ` *      "tm_sec" - seconds, 0 to 59` |
|     - | 1214 | ` *      "tm_min" - minutes, 0 to 59` |
|     - | 1215 | ` *      "tm_hour" - hours, 0 to 23` |
|     - | 1216 | ` *      "tm_mday" - day of the month, 1 to 31` |
|     - | 1217 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|     - | 1218 | ` *      "tm_year" - years since 1900` |
|     - | 1219 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|     - | 1220 | ` *      "tm_yday" - day of the year, 0 to 365` |
|     - | 1221 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|     - | 1222 | ` * Returns` |
|     - | 1223 | ` *  An associative array of information related to the timestamp.` |
|     - | 1224 | ` */` |
|     8 | 1225 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1226 | `{` |
|     - | 1227 | `	ph7_value *pValue,*pArray;` |
|     9 | 1228 | `	int isAssoc = 0;` |
|     - | 1229 | `	Sytm sTm;` |
|     9 | 1230 | `	if( nArg < 1 ){` |
|     - | 1231 | `#ifdef __WINNT__` |
|     - | 1232 | `		SYSTEMTIME sOS;` |
|     1 | 1233 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|     1 | 1234 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1235 | `#else` |
|     - | 1236 | `		struct tm *pTm;` |
|     - | 1237 | `		time_t t;` |
|     4 | 1238 | `		time(&t);` |
|     4 | 1239 | `		pTm = gmtime(&t);` |
|     4 | 1240 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     4 | 1241 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1242 | `#endif` |
|     3 | 1243 | `	}else{` |
|     - | 1244 | `		/* Use the given timestamp */` |
|     - | 1245 | `		time_t t;` |
|     - | 1246 | `		struct tm *pTm;` |
|     5 | 1247 | `		if( ph7_value_is_int(apArg[0]) ){` |
|     5 | 1248 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|     5 | 1249 | `			pTm = gmtime(&t);` |
|     5 | 1250 | `			if( pTm == 0 ){` |
|   ! 0 | 1251 | `				time(&t);` |
|   ! 0 | 1252 | `			}` |
|     3 | 1253 | `		}else{` |
|   ! 0 | 1254 | `			time(&t);` |
|     - | 1255 | `		}` |
|     5 | 1256 | `		pTm = gmtime(&t);` |
|     5 | 1257 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     5 | 1258 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1259 | `	}` |
|     - | 1260 | `	/* Element value */` |
|     9 | 1261 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     9 | 1262 | `	if( pValue == 0 ){` |
|     - | 1263 | `		/* Return NULL */` |
|   ! 0 | 1264 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1265 | `		return PH7_OK;` |
|     - | 1266 | `	}` |
|     - | 1267 | `	/* Create a new array */` |
|     9 | 1268 | `	pArray = ph7_context_new_array(pCtx);` |
|     9 | 1269 | `	if( pArray == 0 ){` |
|     - | 1270 | `		/* Return NULL */` |
|   ! 0 | 1271 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1272 | `		return PH7_OK;` |
|     - | 1273 | `	}` |
|     9 | 1274 | `	if( nArg > 1 ){` |
|     3 | 1275 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|     1 | 1276 | `	}` |
|     - | 1277 | `	/* Fill the array */` |
|     - | 1278 | `	/* Seconds */` |
|     9 | 1279 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|     9 | 1280 | `	if( isAssoc ){` |
|     3 | 1281 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|     2 | 1282 | `	}else{` |
|     7 | 1283 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1284 | `	}` |
|     - | 1285 | `	/* Minutes */` |
|     9 | 1286 | `	ph7_value_int(pValue,sTm.tm_min);` |
|     9 | 1287 | `	if( isAssoc ){` |
|     3 | 1288 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|     2 | 1289 | `	}else{` |
|     7 | 1290 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1291 | `	}` |
|     - | 1292 | `	/* Hours */` |
|     9 | 1293 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|     9 | 1294 | `	if( isAssoc ){` |
|     3 | 1295 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|     2 | 1296 | `	}else{` |
|     7 | 1297 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1298 | `	}` |
|     - | 1299 | `	/* mday */` |
|     9 | 1300 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|     9 | 1301 | `	if( isAssoc ){` |
|     3 | 1302 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|     2 | 1303 | `	}else{` |
|     7 | 1304 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1305 | `	}` |
|     - | 1306 | `	/* mon */` |
|     9 | 1307 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|     9 | 1308 | `	if( isAssoc ){` |
|     3 | 1309 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|     2 | 1310 | `	}else{` |
|     7 | 1311 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1312 | `	}` |
|     - | 1313 | `	/* year since 1900 */` |
|     9 | 1314 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|     9 | 1315 | `	if( isAssoc ){` |
|     3 | 1316 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|     2 | 1317 | `	}else{` |
|     7 | 1318 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1319 | `	}` |
|     - | 1320 | `	/* wday */` |
|     9 | 1321 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|     9 | 1322 | `	if( isAssoc ){` |
|     3 | 1323 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|     2 | 1324 | `	}else{` |
|     7 | 1325 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1326 | `	}` |
|     - | 1327 | `	/* yday */` |
|     9 | 1328 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|     9 | 1329 | `	if( isAssoc ){` |
|     3 | 1330 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|     2 | 1331 | `	}else{` |
|     7 | 1332 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1333 | `	}` |
|     - | 1334 | `	/* isdst */` |
|     - | 1335 | `#ifdef __WINNT__` |
|     - | 1336 | `#ifdef _MSC_VER` |
|     - | 1337 | `#ifndef _WIN32_WCE` |
|     1 | 1338 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1339 | `#endif` |
|     - | 1340 | `#endif` |
|     - | 1341 | `#endif` |
|     9 | 1342 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|     9 | 1343 | `	if( isAssoc ){` |
|     3 | 1344 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|     2 | 1345 | `	}else{` |
|     7 | 1346 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1347 | `	}` |
|     - | 1348 | `	/* Return the array */` |
|     9 | 1349 | `	ph7_result_value(pCtx,pArray);` |
|     9 | 1350 | `	return PH7_OK;` |
|     5 | 1351 | `}` |
|     - | 1352 | `/*` |
|     - | 1353 | ` * int idate(string $format [, int $timestamp = time() ])` |
|     - | 1354 | ` *  Returns a number formatted according to the given format string` |
|     - | 1355 | ` *  using the given integer timestamp or the current local time if` |
|     - | 1356 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|     - | 1357 | ` *  to the value of time().` |
|     - | 1358 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|     - | 1359 | ` *  parameter.` |
|     - | 1360 | ` * $Parameters` |
|     - | 1361 | ` *  Supported format` |
|     - | 1362 | ` *   d 	Day of the month` |
|     - | 1363 | ` *   h 	Hour (12 hour format)` |
|     - | 1364 | ` *   H 	Hour (24 hour format)` |
|     - | 1365 | ` *   i 	Minutes` |
|     - | 1366 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|     - | 1367 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|     - | 1368 | ` *   m 	Month number` |
|     - | 1369 | ` *   s 	Seconds` |
|     - | 1370 | ` *   t 	Days in current month` |
|     - | 1371 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|     - | 1372 | ` *   w 	Day of the week (0 on Sunday)` |
|     - | 1373 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|     - | 1374 | ` *   y 	Year (1 or 2 digits - check note below)` |
|     - | 1375 | ` *   Y 	Year (4 digits)` |
|     - | 1376 | ` *   z 	Day of the year` |
|     - | 1377 | ` *   Z 	Timezone offset in seconds` |
|     - | 1378 | ` * $timestamp` |
|     - | 1379 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|     - | 1380 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|     - | 1381 | ` *  to the value of time().` |
|     - | 1382 | ` * Return` |
|     - | 1383 | ` *  An integer.` |
|     - | 1384 | ` */` |
|    38 | 1385 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1386 | `{` |
|     - | 1387 | `	const char *zFormat;` |
|    40 | 1388 | `	ph7_int64 iVal = 0;` |
|     - | 1389 | `	int nLen;` |
|     - | 1390 | `	Sytm sTm;` |
|    40 | 1391 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1392 | `		/* Missing/Invalid argument,return -1 */` |
|   ! 0 | 1393 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1394 | `		return PH7_OK;` |
|     - | 1395 | `	}` |
|    40 | 1396 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    40 | 1397 | `	if( nLen < 1 ){` |
|     - | 1398 | `		/* Don't bother processing return -1*/` |
|   ! 0 | 1399 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1400 | `	}` |
|    40 | 1401 | `	if( nArg < 2 ){` |
|     - | 1402 | `#ifdef __WINNT__` |
|     - | 1403 | `		SYSTEMTIME sOS;` |
|     2 | 1404 | `		GetSystemTime(&sOS);` |
|     2 | 1405 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1406 | `#else` |
|     - | 1407 | `		struct tm *pTm;` |
|     - | 1408 | `		time_t t;` |
|    28 | 1409 | `		time(&t);` |
|    28 | 1410 | `		pTm = gmtime(&t);` |
|    28 | 1411 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    28 | 1412 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1413 | `#endif` |
|    16 | 1414 | `	}else{` |
|     - | 1415 | `		/* Use the given timestamp */` |
|     - | 1416 | `		time_t t;` |
|     - | 1417 | `		struct tm *pTm;` |
|    11 | 1418 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    11 | 1419 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    11 | 1420 | `			pTm = gmtime(&t);` |
|    11 | 1421 | `			if( pTm == 0 ){` |
|   ! 0 | 1422 | `				time(&t);` |
|   ! 0 | 1423 | `			}` |
|     6 | 1424 | `		}else{` |
|   ! 0 | 1425 | `			time(&t);` |
|     - | 1426 | `		}` |
|    11 | 1427 | `		pTm = gmtime(&t);` |
|    11 | 1428 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    11 | 1429 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1430 | `	}` |
|     - | 1431 | `	/* Perform the requested operation */` |
|    40 | 1432 | `	switch(zFormat[0]){` |
|     2 | 1433 | `	case 'd':` |
|     - | 1434 | `		/* Day of the month */` |
|     5 | 1435 | `		iVal = sTm.tm_mday;` |
|     5 | 1436 | `		break;` |
|   ! 0 | 1437 | `	case 'h':` |
|     - | 1438 | `		/*	Hour (12 hour format)*/` |
|   ! 0 | 1439 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|   ! 0 | 1440 | `		break;` |
|     1 | 1441 | `	case 'H':` |
|     - | 1442 | `		/* Hour (24 hour format)*/` |
|     3 | 1443 | `		iVal = sTm.tm_hour;` |
|     3 | 1444 | `		break;` |
|     1 | 1445 | `	case 'i':` |
|     - | 1446 | `		/*Minutes*/` |
|     3 | 1447 | `		iVal = sTm.tm_min;` |
|     3 | 1448 | `		break;` |
|     1 | 1449 | `	case 'I':` |
|     - | 1450 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|     - | 1451 | `#ifdef __WINNT__` |
|     - | 1452 | `#ifdef _MSC_VER` |
|     - | 1453 | `#ifndef _WIN32_WCE` |
|     1 | 1454 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1455 | `#endif` |
|     - | 1456 | `#endif` |
|     - | 1457 | `#endif` |
|     3 | 1458 | `		iVal = sTm.tm_isdst;` |
|     3 | 1459 | `		break;` |
|     1 | 1460 | `	case 'L':` |
|     - | 1461 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|     3 | 1462 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|     3 | 1463 | `		break;` |
|     2 | 1464 | `	case 'm':` |
|     - | 1465 | `		/* Month number*/` |
|     5 | 1466 | `		iVal = sTm.tm_mon;` |
|     5 | 1467 | `		break;` |
|     1 | 1468 | `	case 's':` |
|     - | 1469 | `		/*Seconds*/` |
|     3 | 1470 | `		iVal = sTm.tm_sec;` |
|     3 | 1471 | `		break;` |
|     1 | 1472 | `	case 't':{` |
|     - | 1473 | `		/*Days in current month*/` |
|     - | 1474 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|     3 | 1475 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|     3 | 1476 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|   ! 0 | 1477 | `			nDays = 28;` |
|   ! 0 | 1478 | `		}` |
|     3 | 1479 | `		iVal = nDays;` |
|     3 | 1480 | `		break;` |
|     - | 1481 | `			 }` |
|     1 | 1482 | `	case 'U':` |
|     - | 1483 | `		/*Seconds since the Unix Epoch*/` |
|     3 | 1484 | `		iVal = (ph7_int64)time(0);` |
|     3 | 1485 | `		break;` |
|     1 | 1486 | `	case 'w':` |
|     - | 1487 | `		/*	Day of the week (0 on Sunday) */` |
|     3 | 1488 | `		iVal = sTm.tm_wday;` |
|     3 | 1489 | `		break;` |
|     1 | 1490 | `	case 'W': {` |
|     - | 1491 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|     - | 1492 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|     3 | 1493 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|     3 | 1494 | `		break;` |
|     - | 1495 | `			  }` |
|   ! 0 | 1496 | `	case 'y':` |
|     - | 1497 | `		/* Year (2 digits) */` |
|   ! 0 | 1498 | `		iVal = sTm.tm_year % 100;` |
|   ! 0 | 1499 | `		break;` |
|     3 | 1500 | `	case 'Y':` |
|     - | 1501 | `		/* Year (4 digits) */` |
|     7 | 1502 | `		iVal = sTm.tm_year;` |
|     7 | 1503 | `		break;` |
|     1 | 1504 | `	case 'z':` |
|     - | 1505 | `		/* Day of the year */` |
|     3 | 1506 | `		iVal = sTm.tm_yday;` |
|     3 | 1507 | `		break;` |
|     1 | 1508 | `	case 'Z':` |
|     - | 1509 | `		/*Timezone offset in seconds*/` |
|     3 | 1510 | `		iVal = sTm.tm_gmtoff;` |
|     3 | 1511 | `		break;` |
|     1 | 1512 | `	default:` |
|     - | 1513 | `		/* unknown format,throw a warning */` |
|     3 | 1514 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|     2 | 1515 | `		break;` |
|     - | 1516 | `	}` |
|     - | 1517 | `	/* Return the time value */` |
|    40 | 1518 | `	ph7_result_int64(pCtx,iVal);` |
|    40 | 1519 | `	return PH7_OK;` |
|    21 | 1520 | `}` |
|     - | 1521 | `/*` |
|     - | 1522 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|     - | 1523 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|     - | 1524 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|     - | 1525 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|     - | 1526 | ` *  specified.` |
|     - | 1527 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|     - | 1528 | ` *  the current value according to the local date and time.` |
|     - | 1529 | ` * Parameters` |
|     - | 1530 | ` * $hour` |
|     - | 1531 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|     - | 1532 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|     - | 1533 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|     - | 1534 | ` * $minute` |
|     - | 1535 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|     - | 1536 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|     - | 1537 | ` *  in the following hour(s).` |
|     - | 1538 | ` * $second` |
|     - | 1539 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|     - | 1540 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|     - | 1541 | ` * second in the following minute(s).` |
|     - | 1542 | ` * $month` |
|     - | 1543 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|     - | 1544 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|     - | 1545 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|     - | 1546 | ` * $day` |
|     - | 1547 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|     - | 1548 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|     - | 1549 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|     - | 1550 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|     - | 1551 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|     - | 1552 | ` * $year` |
|     - | 1553 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|     - | 1554 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|     - | 1555 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|     - | 1556 | ` * $is_dst` |
|     - | 1557 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|     - | 1558 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|     - | 1559 | ` * Return` |
|     - | 1560 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|     - | 1561 | ` *   If the arguments are invalid, the function returns FALSE` |
|     - | 1562 | ` */` |
|    44 | 1563 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1564 | `{` |
|     - | 1565 | `	const char *zFunction;` |
|     - | 1566 | `	ph7_int64 iVal;` |
|     - | 1567 | `	sxi64 h,mi,s,mo,d,y,yAdj;` |
|     - | 1568 | `	int moN;` |
|     - | 1569 | `	struct tm *pTm;` |
|     - | 1570 | `	time_t t;` |
|     - | 1571 | `	/* Extract function name */` |
|    45 | 1572 | `	zFunction = ph7_function_name(pCtx);` |
|     - | 1573 | `	/* PHP 8 dropped the legacy $is_dst 7th parameter: mktime()/gmmktime() now` |
|     - | 1574 | `	 * accept at most 6 arguments and throw a catchable ArgumentCountError` |
|     - | 1575 | `	 * otherwise (the central aBuiltinArity table only enforces the minimum, so` |
|     - | 1576 | `	 * this maximum is checked here). */` |
|    45 | 1577 | `	if( nArg > 6 ){` |
|    10 | 1578 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     3 | 1579 | `			"%s() expects at most 6 arguments, %d given",zFunction,nArg);` |
|     - | 1580 | `	}` |
|    39 | 1581 | `	if( nArg < 1 ){` |
|   ! 0 | 1582 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1583 | `			"%s() expects at least 1 argument, 0 given",zFunction);` |
|     - | 1584 | `	}` |
|     - | 1585 | `	/* Missing components default from the current time in php's default` |
|     - | 1586 | `	 * timezone. PHL's date_default_timezone_set() only accepts UTC/GMT (no tz` |
|     - | 1587 | `	 * database), so mktime() and gmmktime() agree and both read gmtime(). */` |
|    39 | 1588 | `	time(&t);` |
|    39 | 1589 | `	pTm = gmtime(&t);` |
|    19 | 1590 | `	SXUNUSED(zFunction);` |
|    39 | 1591 | `	h  = pTm->tm_hour;` |
|    39 | 1592 | `	mi = pTm->tm_min;` |
|    39 | 1593 | `	s  = pTm->tm_sec;` |
|    39 | 1594 | `	mo = pTm->tm_mon + 1;` |
|    39 | 1595 | `	d  = pTm->tm_mday;` |
|    39 | 1596 | `	y  = pTm->tm_year + 1900;` |
|    39 | 1597 | `	h = ph7_value_to_int64(apArg[0]);` |
|    39 | 1598 | `	if( nArg > 1 ){` |
|    39 | 1599 | `		mi = ph7_value_to_int64(apArg[1]);` |
|    39 | 1600 | `		if( nArg > 2 ){` |
|    39 | 1601 | `			s = ph7_value_to_int64(apArg[2]);` |
|    39 | 1602 | `			if( nArg > 3 ){` |
|    39 | 1603 | `				mo = ph7_value_to_int64(apArg[3]);` |
|    39 | 1604 | `				if( nArg > 4 ){` |
|    39 | 1605 | `					d = ph7_value_to_int64(apArg[4]);` |
|    39 | 1606 | `					if( nArg > 5 ){` |
|     - | 1607 | `						/* php's legacy two-digit mapping: 0-69 -> 2000-2069,` |
|     - | 1608 | `						 * 70-100 -> 1970-2000; anything else is verbatim */` |
|    39 | 1609 | `						y = ph7_value_to_int64(apArg[5]);` |
|    39 | 1610 | `						if( y >= 0 && y <= 69 ){` |
|     7 | 1611 | `							y += 2000;` |
|    36 | 1612 | `						}else if( y >= 70 && y <= 100 ){` |
|     5 | 1613 | `							y += 1900;` |
|     2 | 1614 | `						}` |
|    19 | 1615 | `					}` |
|    19 | 1616 | `				}` |
|    19 | 1617 | `			}` |
|    19 | 1618 | `		}` |
|    19 | 1619 | `	}` |
|     - | 1620 | `	/* Normalize the month with floor semantics, then let day/time components` |
|     - | 1621 | `	 * overflow linearly (php: mktime(25,-30,0,1,1,2024) == Jan 2 00:30). */` |
|    39 | 1622 | `	yAdj = y + DtFloorDiv(mo - 1,12);` |
|    39 | 1623 | `	moN  = (int)(mo - 1 - DtFloorDiv(mo - 1,12) * 12) + 1;` |
|    39 | 1624 | `	iVal = (DtDaysFromCivil(yAdj,moN,1) + (d - 1)) * 86400 + h*3600 + mi*60 + s;` |
|     - | 1625 | `	/* Return the timestamp as a 64bit integer */` |
|    39 | 1626 | `	ph7_result_int64(pCtx,iVal);` |
|    39 | 1627 | `	return PH7_OK;` |
|    23 | 1628 | `}` |
|     - | 1629 | `/*` |
|     - | 1630 | ` * string date_default_timezone_get(void)` |
|     - | 1631 | ` *  Gets the default timezone used by all date/time functions in a script.` |
|     - | 1632 | ` */` |
|     4 | 1633 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1634 | `{` |
|     5 | 1635 | `	ph7_vm *pVm = pCtx->pVm;` |
|     2 | 1636 | `	SXUNUSED(nArg);` |
|     2 | 1637 | `	SXUNUSED(apArg);` |
|     5 | 1638 | `	ph7_result_string(pCtx,pVm->zDefTz,(int)pVm->nDefTz);` |
|     5 | 1639 | `	return PH7_OK;` |
|     1 | 1640 | `}` |
|     - | 1641 | `/*` |
|     - | 1642 | ` * bool date_default_timezone_set(string $timezoneId)` |
|     - | 1643 | ` *  Sets the default timezone used by all date/time functions in a script.` |
|     - | 1644 | ` *  php validates against the tz database and stores the id verbatim (get()` |
|     - | 1645 | ` *  echoes back "utc" if that's what was set). PHL ships no tz database, so` |
|     - | 1646 | ` *  only UTC and GMT are accepted; every other id — including region names php` |
|     - | 1647 | ` *  would accept — is rejected with php's invalid-id notice (recorded scope cut).` |
|     - | 1648 | ` */` |
|    24 | 1649 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1650 | `{` |
|    25 | 1651 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1652 | `	const char *zId;` |
|     - | 1653 | `	int nId;` |
|    25 | 1654 | `	if( nArg < 1 ){` |
|   ! 0 | 1655 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1656 | `		return PH7_OK;` |
|     - | 1657 | `	}` |
|    25 | 1658 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|    25 | 1659 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|    25 | 1660 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|    25 | 1661 | `		pVm->zDefTz[3] = 0;` |
|    25 | 1662 | `		pVm->nDefTz = 3;` |
|    25 | 1663 | `		ph7_result_bool(pCtx,1);` |
|    25 | 1664 | `		return PH7_OK;` |
|     - | 1665 | `	}` |
|     - | 1666 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|     - | 1667 | `	 * — exactly php's notice shape here */` |
|   ! 0 | 1668 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|   ! 0 | 1669 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 1670 | `	return PH7_OK;` |
|    13 | 1671 | `}` |
|     - | 1672 |  |
|     - | 1673 | `/* ===========================================================================` |
|     - | 1674 | ` * DateTime family (NEWPLAN band D slice 1): DateTimeInterface, DateTime,` |
|     - | 1675 | ` * DateTimeImmutable, DateTimeZone (UTC + fixed offsets), date_create(),` |
|     - | 1676 | ` * date_create_immutable(). Embedded-PHP chunk + C thunks, following the` |
|     - | 1677 | ` * Reflection architecture (installed inside the bCompilingBuiltin window).` |
|     - | 1678 | ` * Timezone SCOPE: UTC and fixed "+HH:MM" offsets only — no tz database` |
|     - | 1679 | ` * (recorded §10 scope cut; named region zones throw like unknown zones).` |
|     - | 1680 | ` * ======================================================================== */` |
|     - | 1681 |  |
|     - | 1682 | `/*` |
|     - | 1683 | ` * Proleptic-Gregorian civil <-> day-count conversions (Howard Hinnant's` |
|     - | 1684 | ` * algorithms): no time_t / libc dependence, correct far past 2038 and` |
|     - | 1685 | ` * before 1970 on every platform. Day 0 == 1970-01-01.` |
|     - | 1686 | ` */` |
|   968 | 1687 | `static sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|     1 | 1688 | `{` |
|     - | 1689 | `	sxi64 era;` |
|     - | 1690 | `	unsigned yoe,doy,doe;` |
|   969 | 1691 | `	y -= (m <= 2);` |
|   969 | 1692 | `	era = (y >= 0 ? y : y - 399) / 400;` |
|   969 | 1693 | `	yoe = (unsigned)(y - era * 400);` |
|   969 | 1694 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|   969 | 1695 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|   969 | 1696 | `	return era * 146097 + (sxi64)doe - 719468;` |
|     1 | 1697 | `}` |
|   430 | 1698 | `static void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|     1 | 1699 | `{` |
|     - | 1700 | `	sxi64 era;` |
|     - | 1701 | `	unsigned doe,yoe,doy,mp;` |
|   431 | 1702 | `	z += 719468;` |
|   431 | 1703 | `	era = (z >= 0 ? z : z - 146096) / 146097;` |
|   431 | 1704 | `	doe = (unsigned)(z - era * 146097);` |
|   431 | 1705 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|   431 | 1706 | `	*py = (sxi64)yoe + era * 400;` |
|   431 | 1707 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|   431 | 1708 | `	mp = (5 * doy + 2) / 153;` |
|   431 | 1709 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|   431 | 1710 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|   431 | 1711 | `	if( *pm <= 2 ){` |
|   195 | 1712 | `		*py += 1;` |
|    97 | 1713 | `	}` |
|   431 | 1714 | `}` |
|   722 | 1715 | `static sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|     1 | 1716 | `{` |
|   723 | 1717 | `	sxi64 q = a / b;` |
|   723 | 1718 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|     3 | 1719 | `		q--;` |
|     1 | 1720 | `	}` |
|   723 | 1721 | `	return q;` |
|     1 | 1722 | `}` |
|     - | 1723 | `/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z) */` |
|   182 | 1724 | `static void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)` |
|     1 | 1725 | `{` |
|   183 | 1726 | `	sxi64 t = iTs + iOff;` |
|   183 | 1727 | `	sxi64 days = DtFloorDiv(t,86400);` |
|   183 | 1728 | `	sxi64 secs = t - days * 86400;` |
|     - | 1729 | `	sxi64 y;` |
|     - | 1730 | `	int mo,d;` |
|   183 | 1731 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|   183 | 1732 | `	pTm->tm_sec  = (int)(secs % 60);` |
|   183 | 1733 | `	pTm->tm_min  = (int)((secs / 60) % 60);` |
|   183 | 1734 | `	pTm->tm_hour = (int)(secs / 3600);` |
|   183 | 1735 | `	pTm->tm_mday = d;` |
|   183 | 1736 | `	pTm->tm_mon  = mo - 1;` |
|   183 | 1737 | `	pTm->tm_year = (int)y;` |
|   183 | 1738 | `	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */` |
|   183 | 1739 | `	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));` |
|   183 | 1740 | `	pTm->tm_isdst = 0;` |
|   183 | 1741 | `	pTm->tm_zone = zZone;` |
|   183 | 1742 | `	pTm->tm_gmtoff = (long)iOff;` |
|   183 | 1743 | `}` |
|   244 | 1744 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|     1 | 1745 | `{` |
|   245 | 1746 | `	return DtDaysFromCivil(y,mo,d) * 86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     1 | 1747 | `}` |
|     - | 1748 | `/* Month-arithmetic with php's overflow semantics (Jan 31 +1 month -> Mar 2/3):` |
|     - | 1749 | ` * normalize the month, keep the day — the civil day-count formula is linear in` |
|     - | 1750 | ` * d, so an out-of-range day simply lands in the following month. */` |
|    22 | 1751 | `static sxi64 DtAddMonths(sxi64 iTs,sxi32 iOff,sxi64 nMonths)` |
|     1 | 1752 | `{` |
|    23 | 1753 | `	sxi64 t = iTs + iOff;` |
|    23 | 1754 | `	sxi64 days = DtFloorDiv(t,86400);` |
|    23 | 1755 | `	sxi64 secs = t - days * 86400;` |
|     - | 1756 | `	sxi64 y;` |
|     - | 1757 | `	int mo,d;` |
|     - | 1758 | `	sxi64 m0;` |
|    23 | 1759 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|    23 | 1760 | `	m0 = (y * 12 + (mo - 1)) + nMonths;` |
|    23 | 1761 | `	y  = DtFloorDiv(m0,12);` |
|    23 | 1762 | `	mo = (int)(m0 - y * 12) + 1;` |
|    23 | 1763 | `	return DtDaysFromCivil(y,mo,d) * 86400 + secs - iOff;` |
|     1 | 1764 | `}` |
|     - | 1765 | `/*` |
|     - | 1766 | ` * Read a fractional-seconds part at z (which points at the '.'): up to 6 digits` |
|     - | 1767 | ` * become microseconds (right-padded to 6, extra digits ignored). Advances *pz.` |
|     - | 1768 | ` */` |
|    24 | 1769 | `static int DtReadFraction(const char **pz,const char *zEnd)` |
|     1 | 1770 | `{` |
|    25 | 1771 | `	const char *z = *pz;` |
|    25 | 1772 | `	int us = 0,n = 0;` |
|    25 | 1773 | `	z++; /* skip '.' */` |
|   111 | 1774 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|    87 | 1775 | `		if( n < 6 ){ us = us*10 + (z[0]-'0'); n++; }` |
|    87 | 1776 | `		z++;` |
|     1 | 1777 | `	}` |
|    83 | 1778 | `	while( n < 6 ){ us *= 10; n++; }` |
|    25 | 1779 | `	*pz = z;` |
|    25 | 1780 | `	return us;` |
|     1 | 1781 | `}` |
|     - | 1782 | `/*` |
|     - | 1783 | ` * Parse an OPTIONAL time-of-day suffix after a date component:` |
|     - | 1784 | ` * "[( \|T)]HH:MM[:SS][.frac][Z\|±hh[:mm]]". On entry *pz points just past the date;` |
|     - | 1785 | ` * the h/mi/s outs must be pre-zeroed and the offset outs pre-seeded with the current` |
|     - | 1786 | ` * offset; *pUs receives the microseconds from a fractional part (unchanged when` |
|     - | 1787 | ` * absent). Advances *pz over whatever it consumes. Returns 0 on success (whether or` |
|     - | 1788 | ` * not a time was present), or a 1-based error position into zIn (negative encodes` |
|     - | 1789 | ` * php's "Double time specification"). Shared by every absolute-date branch.` |
|     - | 1790 | ` */` |
|   196 | 1791 | `static int DtTimeSuffix(const char **pz,const char *zEnd,const char *zIn,` |
|     - | 1792 | `	int *ph,int *pmi,int *ps,sxi32 *piOff,int *pbOffSet,int *pUs)` |
|     1 | 1793 | `{` |
|   197 | 1794 | `	const char *z = *pz;` |
|   196 | 1795 | `	if( z < zEnd && (z[0]=='T' \|\| z[0]==' ') && zEnd-z >= 6` |
|    82 | 1796 | `	 && SyisDigit(z[1]) && SyisDigit(z[2]) && z[3]==':' ){` |
|    79 | 1797 | `		z++;` |
|    79 | 1798 | `		*ph  = (z[0]-'0')*10 + (z[1]-'0');` |
|    79 | 1799 | `		*pmi = (z[3]-'0')*10 + (z[4]-'0');` |
|     - | 1800 | `		/* a 25+ hour kills php's whole time token: error at its start */` |
|    79 | 1801 | `		if( *ph > 24 ){ return (int)(z - zIn) + 1; }` |
|     - | 1802 | `		/* php lexes HH:M, then the minute's second digit starts a SECOND time` |
|     - | 1803 | `		 * token: "Double time specification" (negative encoding) */` |
|    77 | 1804 | `		if( *pmi > 59 ){ return -((int)(&z[4] - zIn) + 1); }` |
|    75 | 1805 | `		z += 5;` |
|    75 | 1806 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|    73 | 1807 | `			*ps = (z[1]-'0')*10 + (z[2]-'0');` |
|    73 | 1808 | `			if( *ps > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|    71 | 1809 | `			z += 3;` |
|    35 | 1810 | `		}` |
|    73 | 1811 | `		if( z < zEnd && z[0]=='.' && zEnd-z >= 2 && SyisDigit(z[1]) ){ /* fractional seconds */` |
|    23 | 1812 | `			*pUs = DtReadFraction(&z,zEnd);` |
|    11 | 1813 | `		}` |
|    73 | 1814 | `		if( z < zEnd && (z[0]=='Z' \|\| z[0]=='z') ){` |
|     7 | 1815 | `			*piOff = 0; *pbOffSet = 2; z++;` |
|    70 | 1816 | `		}else if( z < zEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|     7 | 1817 | `			int sign = (z[0]=='-') ? -1 : 1;` |
|     7 | 1818 | `			int oh,om = 0;` |
|     7 | 1819 | `			z++;` |
|     7 | 1820 | `			if( zEnd-z < 2 \|\| !SyisDigit(z[0]) \|\| !SyisDigit(z[1]) ){ return (int)(z - zIn) + 1; }` |
|     7 | 1821 | `			oh = (z[0]-'0')*10 + (z[1]-'0');` |
|     7 | 1822 | `			z += 2;` |
|     7 | 1823 | `			if( z < zEnd && z[0]==':' ){ z++; }` |
|     7 | 1824 | `			if( zEnd-z >= 2 && SyisDigit(z[0]) && SyisDigit(z[1]) ){` |
|     7 | 1825 | `				om = (z[0]-'0')*10 + (z[1]-'0');` |
|     7 | 1826 | `				z += 2;` |
|     3 | 1827 | `			}` |
|     7 | 1828 | `			*piOff = sign * (oh*3600 + om*60);` |
|     7 | 1829 | `			*pbOffSet = 1;` |
|     3 | 1830 | `		}` |
|    36 | 1831 | `	}` |
|   191 | 1832 | `	*pz = z;` |
|   191 | 1833 | `	return 0;` |
|    99 | 1834 | `}` |
|     - | 1835 | `/*` |
|     - | 1836 | ` * Read one or two decimal digits at z (z<zEnd guaranteed by caller for the first).` |
|     - | 1837 | ` * Returns the value; *pn = digits consumed (1 or 2).` |
|     - | 1838 | ` */` |
|   118 | 1839 | `static int DtRead1or2(const char *z,const char *zEnd,int *pn)` |
|     1 | 1840 | `{` |
|   119 | 1841 | `	int v = z[0]-'0';` |
|   119 | 1842 | `	if( z+1 < zEnd && SyisDigit(z[1]) ){ v = v*10 + (z[1]-'0'); *pn = 2; }` |
|    13 | 1843 | `	else { *pn = 1; }` |
|   119 | 1844 | `	return v;` |
|     1 | 1845 | `}` |
|     - | 1846 | `/*` |
|     - | 1847 | ` * Try to read a non-ISO numeric date at z: three integer components joined by ONE` |
|     - | 1848 | ` * consistent separator, plus an optional time suffix. php's field order depends on` |
|     - | 1849 | ` * the separator:` |
|     - | 1850 | ` *   '/'      -> YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY` |
|     - | 1851 | ` *   '-','.'  -> DD-MM-YYYY (day first); a 4-digit-first '.' date (YYYY.MM.DD) is` |
|     - | 1852 | ` *               NOT a php format and is rejected. (ISO YYYY-MM-DD is matched by the` |
|     - | 1853 | ` *               dedicated branch BEFORE this one, so a 4-digit-first '-' never` |
|     - | 1854 | ` *               reaches here.)` |
|     - | 1855 | ` * A 1-2 digit year maps php-style (00-69 -> 2000s, 70-99 -> 1900s). Returns 0 when` |
|     - | 1856 | ` * the text is not such a date (caller falls through), 1 on success (the ts/off outs` |
|     - | 1857 | ` * set and *pzOut advanced past the whole token), or an error code in DtParse's own` |
|     - | 1858 | ` * convention (positive 1-based position into zIn, negative = "double time") when the` |
|     - | 1859 | ` * shape matched but a component is out of range.` |
|     - | 1860 | ` */` |
|    90 | 1861 | `static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,` |
|     - | 1862 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,int *pUs)` |
|     1 | 1863 | `{` |
|     - | 1864 | `	int a,b,c,na,nb,nc;` |
|     - | 1865 | `	char sep;` |
|    91 | 1866 | `	int y,mo,d,h = 0,mi = 0,s = 0,us = 0;` |
|    91 | 1867 | `	sxi32 iOff = *pOff;` |
|     - | 1868 | `	int rcT;` |
|     - | 1869 | `	/* first field: 1-4 digits */` |
|    91 | 1870 | `	if( !SyisDigit(z[0]) ){ return 0; }` |
|    91 | 1871 | `	a = 0; na = 0;` |
|   289 | 1872 | `	while( z < zEnd && SyisDigit(z[0]) && na < 4 ){ a = a*10 + (z[0]-'0'); z++; na++; }` |
|    91 | 1873 | `	if( z >= zEnd \|\| (z[0] != '-' && z[0] != '/' && z[0] != '.') ){ return 0; }` |
|    57 | 1874 | `	sep = z[0];` |
|    57 | 1875 | `	z++;` |
|     - | 1876 | `	/* second field: 1-2 digits */` |
|    57 | 1877 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    57 | 1878 | `	b = DtRead1or2(z,zEnd,&nb); z += nb;` |
|    57 | 1879 | `	if( z >= zEnd \|\| z[0] != sep ){ return 0; }` |
|    57 | 1880 | `	z++;` |
|     - | 1881 | `	/* third field: 1-4 digits */` |
|    57 | 1882 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    57 | 1883 | `	c = 0; nc = 0;` |
|   239 | 1884 | `	while( z < zEnd && SyisDigit(z[0]) && nc < 4 ){ c = c*10 + (z[0]-'0'); z++; nc++; }` |
|     - | 1885 | `	/* map fields to Y/M/D; nyear tracks the year field's width for 2-digit mapping.` |
|     - | 1886 | `	 * '/'  : YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY.` |
|     - | 1887 | `	 * '-'/'.': a 4-digit LAST field is DD-MM-YYYY (day first); otherwise YY-MM-DD` |
|     - | 1888 | `	 *          (year first) — php's width heuristic. (A 4-digit FIRST '-' field is` |
|     - | 1889 | `	 *          ISO and never reaches here; a 4-digit-first '.' is not a php format.) */` |
|     - | 1890 | `	{` |
|     - | 1891 | `		int nyear;` |
|    57 | 1892 | `		if( sep == '/' ){` |
|    27 | 1893 | `			if( na == 4 ){ y = a; mo = b; d = c; nyear = na; }` |
|    19 | 1894 | `			else{ mo = a; d = b; y = c; nyear = nc; }` |
|    44 | 1895 | `		}else if( sep == '.' ){` |
|     - | 1896 | `			/* php's dot date is DD.MM.YYYY only (a 4-digit year, day first). Other` |
|     - | 1897 | `			 * widths are not a clean php format (php itself yields garbage there),` |
|     - | 1898 | `			 * so don't claim the match — let the caller fail the parse. */` |
|     5 | 1899 | `			if( na == 4 \|\| nc != 4 ){ return 0; }` |
|     3 | 1900 | `			d = a; mo = b; y = c; nyear = nc;` |
|     2 | 1901 | `		}else{ /* '-' : a 4-digit LAST field is DD-MM-YYYY, else YY-MM-DD */` |
|    27 | 1902 | `			if( nc == 4 ){ d = a; mo = b; y = c; nyear = nc; }` |
|    11 | 1903 | `			else{ y = a; mo = b; d = c; nyear = na; }` |
|     - | 1904 | `		}` |
|    55 | 1905 | `		if( nyear <= 2 ){` |
|    11 | 1906 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|     3 | 1907 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|     5 | 1908 | `		}` |
|     - | 1909 | `	}` |
|     - | 1910 | `	/* php normalizes month 0 to December of the previous year (like the ISO branch)` |
|     - | 1911 | `	 * but fails a month past 12; a day past 31 fails, while day 0 normalizes in` |
|     - | 1912 | `	 * DtMakeTs. Errors point at the field end. */` |
|    55 | 1913 | `	if( mo > 12 ){ return (int)(z - zIn) + 1; }` |
|    51 | 1914 | `	if( mo == 0 ){ mo = 12; y--; }` |
|    51 | 1915 | `	if( d > 31 ){ return (int)(z - zIn) + 1; }` |
|     - | 1916 | `	/* optional time-of-day suffix, then commit */` |
|    47 | 1917 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff,&us);` |
|    47 | 1918 | `	if( rcT != 0 ){ return rcT; }` |
|    47 | 1919 | `	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    47 | 1920 | `	*pOff = iOff;` |
|    47 | 1921 | `	*pUs = us;` |
|    47 | 1922 | `	*pzOut = z;` |
|    47 | 1923 | `	return 1;` |
|    46 | 1924 | `}` |
|     - | 1925 | `/*` |
|     - | 1926 | ` * Match a month name at z (full name or its distinct 3-letter abbreviation, plus` |
|     - | 1927 | ` * "sept"), case-insensitively and only at a word boundary. Returns the month 1-12` |
|     - | 1928 | ` * and sets *pAdv to the bytes consumed, or 0 when no month name is present.` |
|     - | 1929 | ` */` |
|   236 | 1930 | `static int DtMatchMonth(const char *z,const char *zEnd,int *pAdv)` |
|     1 | 1931 | `{` |
|     - | 1932 | `	static const struct { const char *z; int n; int mo; } aM[] = {` |
|     - | 1933 | `		{ "january",7,1 },{ "february",8,2 },{ "march",5,3 },{ "april",5,4 },` |
|     - | 1934 | `		{ "june",4,6 },{ "july",4,7 },{ "august",6,8 },{ "september",9,9 },` |
|     - | 1935 | `		{ "sept",4,9 },{ "october",7,10 },{ "november",8,11 },{ "december",8,12 },` |
|     - | 1936 | `		{ "may",3,5 },` |
|     - | 1937 | `		{ "jan",3,1 },{ "feb",3,2 },{ "mar",3,3 },{ "apr",3,4 },{ "jun",3,6 },` |
|     - | 1938 | `		{ "jul",3,7 },{ "aug",3,8 },{ "sep",3,9 },{ "oct",3,10 },{ "nov",3,11 },` |
|     - | 1939 | `		{ "dec",3,12 }` |
|     - | 1940 | `	};` |
|     - | 1941 | `	sxu32 i;` |
|  4805 | 1942 | `	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){` |
|  4631 | 1943 | `		int n = aM[i].n;` |
|  4630 | 1944 | `		if( zEnd - z >= n && SyStrnicmp(z,aM[i].z,(sxu32)n) == 0` |
|  2186 | 1945 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|    63 | 1946 | `			*pAdv = n;` |
|    63 | 1947 | `			return aM[i].mo;` |
|     - | 1948 | `		}` |
|  2285 | 1949 | `	}` |
|   175 | 1950 | `	return 0;` |
|   119 | 1951 | `}` |
|     - | 1952 | `/* Match a weekday name at z (full or 3-letter, case-insensitive, word boundary).` |
|     - | 1953 | ` * Returns the day-of-week 0=Sunday..6=Saturday and sets *pAdv, or -1. */` |
|   154 | 1954 | `static int DtMatchWeekday(const char *z,const char *zEnd,int *pAdv)` |
|     1 | 1955 | `{` |
|     - | 1956 | `	static const struct { const char *z; int n; int dow; } aW[] = {` |
|     - | 1957 | `		{ "sunday",6,0 },{ "monday",6,1 },{ "tuesday",7,2 },{ "wednesday",9,3 },` |
|     - | 1958 | `		{ "thursday",8,4 },{ "friday",6,5 },{ "saturday",8,6 },` |
|     - | 1959 | `		{ "sun",3,0 },{ "mon",3,1 },{ "tue",3,2 },{ "wed",3,3 },{ "thu",3,4 },` |
|     - | 1960 | `		{ "fri",3,5 },{ "sat",3,6 }` |
|     - | 1961 | `	};` |
|     - | 1962 | `	sxu32 i;` |
|  1865 | 1963 | `	for( i = 0 ; i < SX_ARRAYSIZE(aW) ; ++i ){` |
|  1755 | 1964 | `		int n = aW[i].n;` |
|  1754 | 1965 | `		if( zEnd - z >= n && SyStrnicmp(z,aW[i].z,(sxu32)n) == 0` |
|   737 | 1966 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|    45 | 1967 | `			*pAdv = n;` |
|    45 | 1968 | `			return aW[i].dow;` |
|     - | 1969 | `		}` |
|   856 | 1970 | `	}` |
|   111 | 1971 | `	return -1;` |
|    78 | 1972 | `}` |
|     - | 1973 | `/* True if z points at a two-letter English ordinal suffix (st/nd/rd/th). */` |
|    62 | 1974 | `static int DtIsOrdinal(const char *z,const char *zEnd)` |
|     1 | 1975 | `{` |
|    63 | 1976 | `	if( zEnd - z < 2 ){ return 0; }` |
|   119 | 1977 | `	return SyStrnicmp(z,"st",2) == 0 \|\| SyStrnicmp(z,"nd",2) == 0` |
|    89 | 1978 | `		\|\| SyStrnicmp(z,"rd",2) == 0 \|\| SyStrnicmp(z,"th",2) == 0;` |
|    32 | 1979 | `}` |
|     - | 1980 | `/*` |
|     - | 1981 | ` * Try to read a textual-month date at z, in either order:` |
|     - | 1982 | ` *   MonthName [Day] [Year]   ("Jan 15 2020", "January", "January 2020")` |
|     - | 1983 | ` *   Day MonthName [Year]     ("15 January 2020", "15th Jan")` |
|     - | 1984 | ` * A missing day defaults to 1, a missing year to the base timestamp's year (php).` |
|     - | 1985 | ` * Day may carry an ordinal suffix, fields may be comma-separated, month names are` |
|     - | 1986 | ` * case-insensitive, and an optional time-of-day suffix + trailing UTC/GMT is` |
|     - | 1987 | ` * consumed. Returns 0 (not a month date — caller falls through, *pzOut untouched),` |
|     - | 1988 | ` * 1 on success, or a DtParse error code (out-of-range day).` |
|     - | 1989 | ` */` |
|   188 | 1990 | `static int DtTryMonthDate(const char *z,const char *zEnd,const char **pzOut,` |
|     - | 1991 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,sxi64 iBaseTs,int *pUs)` |
|     1 | 1992 | `{` |
|   189 | 1993 | `	int mo,d = 1,adv,haveDay = 0,haveYear = 0;` |
|   189 | 1994 | `	sxi64 y = 0;` |
|   189 | 1995 | `	int h = 0,mi = 0,s = 0,us = 0;` |
|   189 | 1996 | `	sxi32 iOff = *pOff;` |
|     - | 1997 | `	int rcT;` |
|     - | 1998 | `#define MDSKIPWS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|   189 | 1999 | `	if( (mo = DtMatchMonth(z,zEnd,&adv)) != 0 ){` |
|     - | 2000 | `		/* MonthName [Day] [Year]. A 4-digit number here is the YEAR, not the day` |
|     - | 2001 | `		 * ("January 2020" is month+year, day defaults); a 1-2 digit number is the day. */` |
|    31 | 2002 | `		z += adv;` |
|    76 | 2003 | `		MDSKIPWS();` |
|    31 | 2004 | `		if( z < zEnd && SyisDigit(z[0]) ){` |
|    31 | 2005 | `			int nrun = 0;` |
|    31 | 2006 | `			const char *zp = z;` |
|    95 | 2007 | `			while( zp < zEnd && SyisDigit(zp[0]) && nrun < 4 ){ zp++; nrun++; }` |
|    31 | 2008 | `			if( nrun < 4 ){` |
|    27 | 2009 | `				d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|    27 | 2010 | `				if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|    27 | 2011 | `				haveDay = 1;` |
|    68 | 2012 | `				MDSKIPWS();` |
|    13 | 2013 | `			}` |
|    16 | 2014 | `		}` |
|   174 | 2015 | `	}else if( SyisDigit(z[0]) ){` |
|     - | 2016 | `		/* Day MonthName [Year] */` |
|    37 | 2017 | `		d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|    37 | 2018 | `		if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|    37 | 2019 | `		haveDay = 1;` |
|    77 | 2020 | `		MDSKIPWS();` |
|    37 | 2021 | `		if( (mo = DtMatchMonth(z,zEnd,&adv)) == 0 ){ return 0; }` |
|    21 | 2022 | `		z += adv;` |
|    49 | 2023 | `		MDSKIPWS();` |
|    11 | 2024 | `	}else{` |
|   123 | 2025 | `		return 0;` |
|     - | 2026 | `	}` |
|     - | 2027 | `	/* optional year */` |
|    51 | 2028 | `	if( z < zEnd && SyisDigit(z[0]) ){` |
|    47 | 2029 | `		int ny = 0;` |
|    47 | 2030 | `		y = 0;` |
|   231 | 2031 | `		while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ y = y*10 + (z[0]-'0'); z++; ny++; }` |
|    47 | 2032 | `		if( ny <= 2 ){` |
|   ! 0 | 2033 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|   ! 0 | 2034 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|   ! 0 | 2035 | `		}` |
|    47 | 2036 | `		haveYear = 1;` |
|    23 | 2037 | `	}` |
|     - | 2038 | `	/* Default the unspecified fields from the base timestamp. php overlays: a` |
|     - | 2039 | `	 * missing year takes the base year; a missing day is 1 when a year WAS given` |
|     - | 2040 | `	 * ("January 2020" -> the 1st) but the base day when only the month was named` |
|     - | 2041 | `	 * ("January" -> the base day). */` |
|     - | 2042 | `	{` |
|     - | 2043 | `		sxi64 by; int bm,bd;` |
|    51 | 2044 | `		DtCivilFromDays(DtFloorDiv(iBaseTs + *pOff,86400),&by,&bm,&bd);` |
|    51 | 2045 | `		if( !haveYear ){ y = by; }` |
|    51 | 2046 | `		if( !haveDay ){ d = haveYear ? 1 : bd; }` |
|     - | 2047 | `	}` |
|    51 | 2048 | `	if( d > 31 ){ return (int)(z - zIn) + 1; }` |
|     - | 2049 | `	/* optional time-of-day suffix */` |
|    51 | 2050 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff,&us);` |
|    51 | 2051 | `	if( rcT != 0 ){ return rcT; }` |
|     - | 2052 | `	/* optional trailing UTC/GMT zone name (PHL's default zone is already UTC) */` |
|    55 | 2053 | `	MDSKIPWS();` |
|    50 | 2054 | `	if( (zEnd-z >= 3 && SyStrnicmp(z,"utc",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3])))` |
|    49 | 2055 | `	 \|\| (zEnd-z >= 3 && SyStrnicmp(z,"gmt",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3]))) ){` |
|     3 | 2056 | `		iOff = 0; z += 3;` |
|     1 | 2057 | `	}` |
|    51 | 2058 | `	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    51 | 2059 | `	*pOff = iOff;` |
|    51 | 2060 | `	*pUs = us;` |
|    51 | 2061 | `	*pzOut = z;` |
|    51 | 2062 | `	return 1;` |
|     - | 2063 | `#undef MDSKIPWS` |
|    95 | 2064 | `}` |
|     - | 2065 | `/*` |
|     - | 2066 | ` * Minimal php-datetime-string parser (slice 1): absolute forms` |
|     - | 2067 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]",` |
|     - | 2068 | ` * keywords today/midnight/noon/tomorrow/yesterday, and relative sequences` |
|     - | 2069 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". Returns 0 on` |
|     - | 2070 | ` * success (ts/off/bOffSet out), or the byte position of the first` |
|     - | 2071 | ` * unparseable character +1 (for php's "at position N" message).` |
|     - | 2072 | ` */` |
|   458 | 2073 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,` |
|     - | 2074 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs)` |
|     1 | 2075 | `{` |
|   459 | 2076 | `	const char *z = zIn, *zEnd = &zIn[nLen];` |
|   459 | 2077 | `	sxi64 iTs = iBaseTs;` |
|   459 | 2078 | `	sxi32 iOff = iBaseOff;` |
|   459 | 2079 | `	int bOffSet = 0;` |
|   459 | 2080 | `	int bAny = 0;` |
|     - | 2081 | `	int iNumRc,iMonRc;` |
|   459 | 2082 | `	int uSec = 0;` |
|   459 | 2083 | `	*pUs = 0;` |
|     - | 2084 | `#define DT_SKIP_WS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|     - | 2085 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|     - | 2086 | `	&& (zEnd-z == (nKw) \|\| !SyisAlpha(z[(nKw)])))` |
|   694 | 2087 | `	DT_SKIP_WS();` |
|   459 | 2088 | `	if( z >= zEnd ){` |
|     - | 2089 | `		/* php: the empty string is "now" */` |
|     3 | 2090 | `		*pTs = iTs;` |
|     3 | 2091 | `		*pOff = iOff;` |
|     3 | 2092 | `		*pbOffSet = bOffSet;` |
|     3 | 2093 | `		return 0;` |
|     - | 2094 | `	}` |
|     - | 2095 | `	/* "@<seconds>" absolute epoch */` |
|   457 | 2096 | `	if( z[0] == '@' ){` |
|    81 | 2097 | `		int neg = 0;` |
|    81 | 2098 | `		sxi64 v = 0;` |
|    81 | 2099 | `		const char *zAt = z;` |
|    81 | 2100 | `		z++;` |
|    81 | 2101 | `		if( z < zEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|     - | 2102 | `		/* php's lexer rejects the whole token: the error points at the '@' */` |
|    81 | 2103 | `		if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zAt - zIn) + 1; }` |
|   225 | 2104 | `		while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|     - | 2105 | `		/* php accepts a fractional epoch ("@1600000000.5" -> .5s = 500000us) */` |
|    79 | 2106 | `		if( z < zEnd && z[0]=='.' && zEnd-z >= 2 && SyisDigit(z[1]) ){` |
|     3 | 2107 | `			*pUs = DtReadFraction(&z,zEnd);` |
|     1 | 2108 | `		}` |
|    79 | 2109 | `		*pTs = neg ? -v : v;` |
|    79 | 2110 | `		*pOff = 0;` |
|    79 | 2111 | `		*pbOffSet = 1;` |
|    79 | 2112 | `		DT_SKIP_WS();` |
|    79 | 2113 | `		return (z < zEnd) ? (int)(z - zIn) + 1 : 0;` |
|     - | 2114 | `	}` |
|     - | 2115 | `	/* Absolute date: YYYY-MM-DD[...] */` |
|   376 | 2116 | `	if( zEnd-z >= 10 && SyisDigit(z[0]) && SyisDigit(z[1]) && SyisDigit(z[2])` |
|   147 | 2117 | `	 && SyisDigit(z[3]) && z[4]=='-' ){` |
|   111 | 2118 | `		sxi64 y = (z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0');` |
|   111 | 2119 | `		int mo,d,h=0,mi=0,s=0;` |
|   111 | 2120 | `		if( !SyisDigit(z[5])\|\|!SyisDigit(z[6])\|\|z[7] != '-'\|\|!SyisDigit(z[8])\|\|!SyisDigit(z[9]) ){` |
|   ! 0 | 2121 | `			return (int)(z - zIn) + 1;` |
|     - | 2122 | `		}` |
|   111 | 2123 | `		mo = (z[5]-'0')*10 + (z[6]-'0');` |
|   111 | 2124 | `		d  = (z[8]-'0')*10 + (z[9]-'0');` |
|     - | 2125 | `		/* php's lexer dies on the SECOND digit of an out-of-range month/day` |
|     - | 2126 | `		 * (either the two-digit pattern fails there, or a one-digit component` |
|     - | 2127 | `		 * matched and the separator check fails there); "00" lexes fine and` |
|     - | 2128 | `		 * normalizes (month 0 == December of the previous year). */` |
|   111 | 2129 | `		if( mo > 12 ){ return (int)(&z[6] - zIn) + 1; }` |
|   105 | 2130 | `		if( d > 31 ){ return (int)(&z[9] - zIn) + 1; }` |
|   101 | 2131 | `		if( mo == 0 ){ mo = 12; y--; }` |
|   101 | 2132 | `		z += 10;` |
|     - | 2133 | `		{` |
|   101 | 2134 | `			int rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,&bOffSet,&uSec);` |
|   101 | 2135 | `			if( rcT != 0 ){ return rcT; }` |
|     - | 2136 | `		}` |
|    95 | 2137 | `		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    95 | 2138 | `		bAny = 1;` |
|   314 | 2139 | `	}else if( SyisDigit(z[0])` |
|   179 | 2140 | `	 && (iNumRc = DtTryNumericDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,&uSec)) != 0 ){` |
|     - | 2141 | `		/* DD-MM-YYYY / DD.MM.YYYY (day first), MM/DD/YYYY (slash, American), and` |
|     - | 2142 | `		 * YYYY/MM/DD (slash, year first) — see DtTryNumericDate. Anything other than` |
|     - | 2143 | `		 * 1 is an error code in DtParse's own convention (positive position / negative` |
|     - | 2144 | `		 * "double time"); propagate it verbatim. */` |
|    55 | 2145 | `		if( iNumRc != 1 ){ return iNumRc; }` |
|    47 | 2146 | `		bAny = 1;` |
|   236 | 2147 | `	}else if( (SyisAlpha(z[0]) \|\| SyisDigit(z[0]))` |
|   201 | 2148 | `	 && (iMonRc = DtTryMonthDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,iBaseTs,&uSec)) != 0 ){` |
|     - | 2149 | `		/* MonthName Day Year / Day MonthName Year, in any of php's spellings. As with` |
|     - | 2150 | `		 * DtTryNumericDate, anything other than 1 is an error code to propagate. */` |
|    51 | 2151 | `		if( iMonRc != 1 ){ return iMonRc; }` |
|    51 | 2152 | `		bAny = 1;` |
|   188 | 2153 | `	}else if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|    14 | 2154 | `	 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|     - | 2155 | `		/* Time-only: HH:MM[:SS] on the base date */` |
|    11 | 2156 | `		sxi64 t = iTs + iOff;` |
|    11 | 2157 | `		sxi64 days = DtFloorDiv(t,86400);` |
|    11 | 2158 | `		int h  = (z[0]-'0')*10 + (z[1]-'0');` |
|    11 | 2159 | `		int mi = (z[3]-'0')*10 + (z[4]-'0');` |
|    11 | 2160 | `		int s = 0;` |
|     - | 2161 | `		/* php: bad hour kills the token (error at its start); bad minute /` |
|     - | 2162 | `		 * second dies on the component's second digit */` |
|    11 | 2163 | `		if( h > 24 ){ return (int)(z - zIn) + 1; }` |
|     9 | 2164 | `		if( mi > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|     7 | 2165 | `		z += 5;` |
|     7 | 2166 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     5 | 2167 | `			s = (z[1]-'0')*10 + (z[2]-'0');` |
|     5 | 2168 | `			if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|     3 | 2169 | `			z += 3;` |
|     1 | 2170 | `		}` |
|     5 | 2171 | `		iTs = days*86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     5 | 2172 | `		bAny = 1;` |
|   155 | 2173 | `	}else if( DT_LOWEQ("now",3) ){` |
|     3 | 2174 | `		z += 3;` |
|     3 | 2175 | `		bAny = 1;` |
|     1 | 2176 | `	}` |
|     - | 2177 | `	/* Relative / keyword sequence */` |
|   173 | 2178 | `	for(;;){` |
|   608 | 2179 | `		DT_SKIP_WS();` |
|   497 | 2180 | `		if( z >= zEnd ){` |
|   329 | 2181 | `			break;` |
|     - | 2182 | `		}` |
|   169 | 2183 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|     9 | 2184 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     9 | 2185 | `			iTs = days*86400 - iOff;` |
|     9 | 2186 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|     9 | 2187 | `			bAny = 1;` |
|     9 | 2188 | `			continue;` |
|     - | 2189 | `		}` |
|   161 | 2190 | `		if( DT_LOWEQ("noon",4) ){` |
|     3 | 2191 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     3 | 2192 | `			iTs = days*86400 + 12*3600 - iOff;` |
|     3 | 2193 | `			z += 4;` |
|     3 | 2194 | `			bAny = 1;` |
|     3 | 2195 | `			continue;` |
|     - | 2196 | `		}` |
|   159 | 2197 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|     3 | 2198 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) + 1;` |
|     3 | 2199 | `			iTs = days*86400 - iOff;` |
|     3 | 2200 | `			z += 8;` |
|     3 | 2201 | `			bAny = 1;` |
|     3 | 2202 | `			continue;` |
|     - | 2203 | `		}` |
|   157 | 2204 | `		if( DT_LOWEQ("yesterday",9) ){` |
|     3 | 2205 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) - 1;` |
|     3 | 2206 | `			iTs = days*86400 - iOff;` |
|     3 | 2207 | `			z += 9;` |
|     3 | 2208 | `			bAny = 1;` |
|     3 | 2209 | `			continue;` |
|     - | 2210 | `		}` |
|     - | 2211 | `		/* Weekday navigation: "[next\|last\|previous\|this] <weekday>" moves to the` |
|     - | 2212 | `		 * midnight of the target weekday. Bare/"this" = the this-week occurrence on` |
|     - | 2213 | `		 * or after the base day; "next"/"last"/"previous" skip a matching base day. */` |
|     - | 2214 | `		{` |
|   155 | 2215 | `			const char *zSave = z;` |
|   155 | 2216 | `			int dir = 0;         /* 0 = this-week occurrence, 1 = next, -1 = last */` |
|     - | 2217 | `			int adv,dow;` |
|   188 | 2218 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; DT_SKIP_WS(); }` |
|   136 | 2219 | `			else if( DT_LOWEQ("previous",8) ){ dir = -1; z += 8; DT_SKIP_WS(); }` |
|   179 | 2220 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; DT_SKIP_WS(); }` |
|   114 | 2221 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; DT_SKIP_WS(); }` |
|   155 | 2222 | `			dow = DtMatchWeekday(z,zEnd,&adv);` |
|   155 | 2223 | `			if( dow >= 0 ){` |
|    45 | 2224 | `				sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|    45 | 2225 | `				int bdow = (int)(((days + 4) % 7 + 7) % 7); /* 1970-01-01 was Thursday */` |
|     - | 2226 | `				sxi64 delta;` |
|    45 | 2227 | `				if( dir == 1 ){` |
|    13 | 2228 | `					delta = ((dow - bdow) % 7 + 7) % 7;` |
|    13 | 2229 | `					if( delta == 0 ){ delta = 7; }` |
|    39 | 2230 | `				}else if( dir == -1 ){` |
|    13 | 2231 | `					delta = -(((bdow - dow) % 7 + 7) % 7);` |
|    13 | 2232 | `					if( delta == 0 ){ delta = -7; }` |
|     7 | 2233 | `				}else{` |
|    21 | 2234 | `					delta = ((dow - bdow) % 7 + 7) % 7;` |
|     - | 2235 | `				}` |
|    45 | 2236 | `				iTs = (days + delta)*86400 - iOff; /* midnight of the target day */` |
|    45 | 2237 | `				z += adv;` |
|    45 | 2238 | `				bAny = 1;` |
|    45 | 2239 | `				continue;` |
|     - | 2240 | `			}` |
|   111 | 2241 | `			z = zSave; /* prefix did not introduce a weekday: rewind and try the rest */` |
|     - | 2242 | `		}` |
|     - | 2243 | `		/* "first\|last day of (this\|next\|last month \| MonthName [Year])": jump to the` |
|     - | 2244 | `		 * first or last day of a target month. A this/next/last-month target keeps the` |
|     - | 2245 | `		 * base time-of-day; an absolute MonthName [Year] target resets it to midnight` |
|     - | 2246 | `		 * (php). */` |
|   111 | 2247 | `		if( DT_LOWEQ("first",5) \|\| DT_LOWEQ("last",4) ){` |
|    39 | 2248 | `			const char *zSave = z;` |
|    39 | 2249 | `			int bFirst = (SyToLower((unsigned char)z[0]) == 'f');` |
|    39 | 2250 | `			z += bFirst ? 5 : 4;` |
|    96 | 2251 | `			DT_SKIP_WS();` |
|    39 | 2252 | `			if( DT_LOWEQ("day",3) ){` |
|    31 | 2253 | `				z += 3;` |
|    76 | 2254 | `				DT_SKIP_WS();` |
|    31 | 2255 | `				if( DT_LOWEQ("of",2) ){` |
|    31 | 2256 | `					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);` |
|     - | 2257 | `					sxi64 yy,tod;` |
|    31 | 2258 | `					int mm,dd0,keepTime = 1,ok = 1;` |
|    31 | 2259 | `					z += 2;` |
|    74 | 2260 | `					DT_SKIP_WS();` |
|    31 | 2261 | `					DtCivilFromDays(days0,&yy,&mm,&dd0);` |
|    31 | 2262 | `					tod = (iTs + iOff) - days0*86400;` |
|    40 | 2263 | `					if( DT_LOWEQ("this",4) ){ z += 4; DT_SKIP_WS();` |
|     7 | 2264 | `						if( DT_LOWEQ("month",5) ){ z += 5; }else{ ok = 0; } }` |
|    34 | 2265 | `					else if( DT_LOWEQ("next",4) ){ z += 4; DT_SKIP_WS();` |
|     7 | 2266 | `						if( DT_LOWEQ("month",5) ){ z += 5; mm++; if(mm>12){ mm=1; yy++; } }else{ ok = 0; } }` |
|    25 | 2267 | `					else if( DT_LOWEQ("last",4) ){ z += 4; DT_SKIP_WS();` |
|     5 | 2268 | `						if( DT_LOWEQ("month",5) ){ z += 5; mm--; if(mm<1){ mm=12; yy--; } }else{ ok = 0; } }` |
|    15 | 2269 | `					else if( z < zEnd ){` |
|     - | 2270 | `						int mo,adv;` |
|    13 | 2271 | `						mo = DtMatchMonth(z,zEnd,&adv);` |
|    13 | 2272 | `						if( mo == 0 ){ return (int)(z - zIn) + 1; }` |
|    27 | 2273 | `						z += adv; DT_SKIP_WS();` |
|    13 | 2274 | `						mm = mo; keepTime = 0; tod = 0;` |
|    13 | 2275 | `						if( z < zEnd && SyisDigit(z[0]) ){` |
|     9 | 2276 | `							int ny = 0; sxi64 yv = 0;` |
|    41 | 2277 | `							while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ yv = yv*10 + (z[0]-'0'); z++; ny++; }` |
|     9 | 2278 | `							if( ny <= 2 ){ if( yv <= 69 ){ yv += 2000; } else if( yv <= 99 ){ yv += 1900; } }` |
|     9 | 2279 | `							yy = yv;` |
|     4 | 2280 | `						}` |
|     6 | 2281 | `					}` |
|     - | 2282 | `					/* else: "... day of" with nothing after — php defaults to this` |
|     - | 2283 | `					 * month (mm/yy/tod stay the base, keepTime stays 1). */` |
|    31 | 2284 | `					if( ok ){` |
|    31 | 2285 | `						int dim = (int)(DtDaysFromCivil(yy,mm+1,1) - DtDaysFromCivil(yy,mm,1));` |
|    31 | 2286 | `						int day = bFirst ? 1 : dim;` |
|    31 | 2287 | `						iTs = DtDaysFromCivil(yy,mm,day)*86400 + (keepTime ? tod : 0) - iOff;` |
|    31 | 2288 | `						bAny = 1;` |
|    31 | 2289 | `						continue;` |
|     - | 2290 | `					}` |
|   ! 0 | 2291 | `				}` |
|   ! 0 | 2292 | `			}` |
|     9 | 2293 | `			z = zSave; /* not the "first\|last day of ..." shape: rewind */` |
|     4 | 2294 | `		}` |
|     - | 2295 | `		/* Standalone "this\|next\|last (month\|week)": month shifts by ±1 keeping the` |
|     - | 2296 | `		 * day/time; week moves to the Monday of this/next/last ISO week keeping the` |
|     - | 2297 | `		 * time-of-day (php: weeks start on Monday). */` |
|     - | 2298 | `		{` |
|   103 | 2299 | `			const char *zSave = z;` |
|   103 | 2300 | `			int dir = 2; /* 2 = no prefix */` |
|   103 | 2301 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; }` |
|    71 | 2302 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; }` |
|    63 | 2303 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; }` |
|    81 | 2304 | `			if( dir != 2 ){` |
|    66 | 2305 | `				DT_SKIP_WS();` |
|    27 | 2306 | `				if( DT_LOWEQ("month",5) ){` |
|    13 | 2307 | `					z += 5;` |
|    13 | 2308 | `					iTs = DtAddMonths(iTs,iOff,dir);` |
|    13 | 2309 | `					bAny = 1;` |
|    13 | 2310 | `					continue;` |
|     - | 2311 | `				}` |
|    15 | 2312 | `				if( DT_LOWEQ("week",4) ){` |
|    13 | 2313 | `					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);` |
|    13 | 2314 | `					sxi64 tod = (iTs + iOff) - days0*86400;` |
|    13 | 2315 | `					int bdow = (int)(((days0 + 4) % 7 + 7) % 7);` |
|    13 | 2316 | `					sxi64 monday = days0 - ((bdow + 6) % 7); /* Monday of the base week */` |
|    13 | 2317 | `					z += 4;` |
|    13 | 2318 | `					monday += (sxi64)dir * 7;` |
|    13 | 2319 | `					iTs = monday*86400 + tod - iOff;` |
|    13 | 2320 | `					bAny = 1;` |
|    13 | 2321 | `					continue;` |
|     - | 2322 | `				}` |
|     1 | 2323 | `			}` |
|    57 | 2324 | `			z = zSave;` |
|     - | 2325 | `		}` |
|     - | 2326 | `		/* Trailing time-of-day in a relative sequence ("next thursday 15:00"): set` |
|     - | 2327 | `		 * the clock on the current day. The leading absolute HH:MM branch handles a` |
|     - | 2328 | `		 * time at the START; this handles one AFTER a date/relative token. */` |
|    56 | 2329 | `		if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|    16 | 2330 | `		 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|    13 | 2331 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|    13 | 2332 | `			int hh = (z[0]-'0')*10 + (z[1]-'0');` |
|    13 | 2333 | `			int mm = (z[3]-'0')*10 + (z[4]-'0');` |
|    13 | 2334 | `			int ss = 0;` |
|    13 | 2335 | `			if( hh > 24 ){ return (int)(z - zIn) + 1; }` |
|    13 | 2336 | `			if( mm > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|    13 | 2337 | `			z += 5;` |
|    13 | 2338 | `			if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     5 | 2339 | `				ss = (z[1]-'0')*10 + (z[2]-'0');` |
|     5 | 2340 | `				if( ss > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|     5 | 2341 | `				z += 3;` |
|     2 | 2342 | `			}` |
|    13 | 2343 | `			iTs = days*86400 + (sxi64)hh*3600 + (sxi64)mm*60 + ss - iOff;` |
|    13 | 2344 | `			bAny = 1;` |
|    13 | 2345 | `			continue;` |
|     - | 2346 | `		}` |
|    45 | 2347 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|    33 | 2348 | `			int neg = 0;` |
|    33 | 2349 | `			sxi64 v = 0;` |
|    33 | 2350 | `			const char *zNumStart = z;` |
|    33 | 2351 | `			if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; }` |
|    33 | 2352 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zNumStart - zIn) + 1; }` |
|    81 | 2353 | `			while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|    33 | 2354 | `			if( neg ){ v = -v; }` |
|    79 | 2355 | `			DT_SKIP_WS();` |
|    33 | 2356 | `			if( DT_LOWEQ("seconds",7) )     { iTs += v;            z += 7; }` |
|    33 | 2357 | `			else if( DT_LOWEQ("second",6) ) { iTs += v;            z += 6; }` |
|    33 | 2358 | `			else if( DT_LOWEQ("secs",4) )   { iTs += v;            z += 4; }` |
|    33 | 2359 | `			else if( DT_LOWEQ("sec",3) )    { iTs += v;            z += 3; }` |
|    33 | 2360 | `			else if( DT_LOWEQ("minutes",7) ){ iTs += v*60;         z += 7; }` |
|    31 | 2361 | `			else if( DT_LOWEQ("minute",6) ) { iTs += v*60;         z += 6; }` |
|    31 | 2362 | `			else if( DT_LOWEQ("mins",4) )   { iTs += v*60;         z += 4; }` |
|    31 | 2363 | `			else if( DT_LOWEQ("min",3) )    { iTs += v*60;         z += 3; }` |
|    31 | 2364 | `			else if( DT_LOWEQ("hours",5) )  { iTs += v*3600;       z += 5; }` |
|    29 | 2365 | `			else if( DT_LOWEQ("hour",4) )   { iTs += v*3600;       z += 4; }` |
|    29 | 2366 | `			else if( DT_LOWEQ("days",4) )   { iTs += v*86400;      z += 4; }` |
|    29 | 2367 | `			else if( DT_LOWEQ("day",3) )    { iTs += v*86400;      z += 3; }` |
|    23 | 2368 | `			else if( DT_LOWEQ("weeks",5) )  { iTs += v*7*86400;    z += 5; }` |
|    21 | 2369 | `			else if( DT_LOWEQ("week",4) )   { iTs += v*7*86400;    z += 4; }` |
|    19 | 2370 | `			else if( DT_LOWEQ("fortnights",10) ){ iTs += v*14*86400; z += 10; }` |
|    17 | 2371 | `			else if( DT_LOWEQ("fortnight",9) )  { iTs += v*14*86400; z += 9; }` |
|    17 | 2372 | `			else if( DT_LOWEQ("months",6) ) { iTs = DtAddMonths(iTs,iOff,v); z += 6; }` |
|    15 | 2373 | `			else if( DT_LOWEQ("month",5) )  { iTs = DtAddMonths(iTs,iOff,v); z += 5; }` |
|     9 | 2374 | `			else if( DT_LOWEQ("years",5) )  { iTs = DtAddMonths(iTs,iOff,v*12); z += 5; }` |
|     9 | 2375 | `			else if( DT_LOWEQ("year",4) )   { iTs = DtAddMonths(iTs,iOff,v*12); z += 4; }` |
|     - | 2376 | `			else{` |
|     7 | 2377 | `				return (int)(z - zIn) + 1;` |
|     - | 2378 | `			}` |
|    27 | 2379 | `			bAny = 1;` |
|    27 | 2380 | `			continue;` |
|     - | 2381 | `		}` |
|    13 | 2382 | `		return (int)(z - zIn) + 1;` |
|   ! 0 | 2383 | `	}` |
|   329 | 2384 | `	if( !bAny ){` |
|   ! 0 | 2385 | `		return 1;` |
|     - | 2386 | `	}` |
|   329 | 2387 | `	*pTs = iTs;` |
|   329 | 2388 | `	*pOff = iOff;` |
|   329 | 2389 | `	*pbOffSet = bOffSet;` |
|   329 | 2390 | `	*pUs = uSec;` |
|   329 | 2391 | `	return 0;` |
|     - | 2392 | `#undef DT_SKIP_WS` |
|     - | 2393 | `#undef DT_LOWEQ` |
|   230 | 2394 | `}` |
|     - | 2395 | `/* int __dt_now() */` |
|   240 | 2396 | `static int vm_builtin_dt_now(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2397 | `{` |
|   120 | 2398 | `	SXUNUSED(nArg);` |
|   120 | 2399 | `	SXUNUSED(apArg);` |
|   241 | 2400 | `	ph7_result_int64(pCtx,(ph7_int64)time(0));` |
|   241 | 2401 | `	return PH7_OK;` |
|     1 | 2402 | `}` |
|     - | 2403 | `/* mixed __dt_parse(string $s, int $baseTs, int $baseOff)` |
|     - | 2404 | ` *   -> [ts, off, offWasExplicit] on success; php's error MESSAGE string on` |
|     - | 2405 | ` *      failure (the chunk wraps it in DateMalformedStringException). */` |
|   458 | 2406 | `static int vm_builtin_dt_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2407 | `{` |
|     - | 2408 | `	const char *zIn;` |
|     - | 2409 | `	int nLen;` |
|     - | 2410 | `	sxi64 iBaseTs;` |
|     - | 2411 | `	sxi32 iBaseOff;` |
|   459 | 2412 | `	sxi64 iTs = 0;` |
|   459 | 2413 | `	sxi32 iOff = 0;` |
|   459 | 2414 | `	int bOffSet = 0;` |
|   459 | 2415 | `	int uSec = 0;` |
|     - | 2416 | `	int iErrPos;` |
|   459 | 2417 | `	if( nArg < 3 ){` |
|   ! 0 | 2418 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2419 | `		return PH7_OK;` |
|     - | 2420 | `	}` |
|   459 | 2421 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   459 | 2422 | `	iBaseTs  = ph7_value_to_int64(apArg[1]);` |
|   459 | 2423 | `	iBaseOff = (sxi32)ph7_value_to_int64(apArg[2]);` |
|   459 | 2424 | `	iErrPos = DtParse(zIn,nLen,iBaseTs,iBaseOff,&iTs,&iOff,&bOffSet,&uSec);` |
|   459 | 2425 | `	if( iErrPos != 0 ){` |
|     - | 2426 | `		/* Negative encoding: php's "Double time specification" reason */` |
|    51 | 2427 | `		int bDouble = iErrPos < 0;` |
|    51 | 2428 | `		int iPos = (bDouble ? -iErrPos : iErrPos) - 1;` |
|    51 | 2429 | `		char cAt = (iPos < nLen) ? zIn[iPos] : ' ';` |
|     - | 2430 | `		/* php appends a reason: an alphabetic token is assumed to be a timezone` |
|     - | 2431 | `		 * lookup miss, anything else an unexpected character. */` |
|   100 | 2432 | `		ph7_result_string_format(pCtx,` |
|     - | 2433 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    25 | 2434 | `			nLen,zIn,iPos,cAt,` |
|    49 | 2435 | `			bDouble ? "Double time specification"` |
|    48 | 2436 | `			: ((cAt >= 'a' && cAt <= 'z') \|\| (cAt >= 'A' && cAt <= 'Z'))` |
|     - | 2437 | `				? "The timezone could not be found in the database"` |
|    48 | 2438 | `				: "Unexpected character");` |
|    51 | 2439 | `		return PH7_OK;` |
|     - | 2440 | `	}` |
|     - | 2441 | `	{` |
|   409 | 2442 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|   409 | 2443 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|   409 | 2444 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2445 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2446 | `		}` |
|   409 | 2447 | `		ph7_value_int64(pV,iTs);` |
|   409 | 2448 | `		ph7_array_add_elem(pArr,0,pV);` |
|   409 | 2449 | `		ph7_value_int64(pV,iOff);` |
|   409 | 2450 | `		ph7_array_add_elem(pArr,0,pV);` |
|     - | 2451 | `		/* int, not bool: 0 = no explicit offset, 1 = numeric offset/@epoch,` |
|     - | 2452 | `		 * 2 = literal "Z" (php keeps the distinction in the zone name) */` |
|   409 | 2453 | `		ph7_value_int64(pV,bOffSet);` |
|   409 | 2454 | `		ph7_array_add_elem(pArr,0,pV);` |
|     - | 2455 | `		/* [3] = microseconds parsed from a fractional-seconds part (0 when absent) */` |
|   409 | 2456 | `		ph7_value_int64(pV,uSec);` |
|   409 | 2457 | `		ph7_array_add_elem(pArr,0,pV);` |
|   409 | 2458 | `		ph7_result_value(pCtx,pArr);` |
|     - | 2459 | `	}` |
|   409 | 2460 | `	return PH7_OK;` |
|   230 | 2461 | `}` |
|     - | 2462 | `/* string __dt_default_tz(void) — the date_default_timezone_set() identifier */` |
|   240 | 2463 | `static int vm_builtin_dt_default_tz(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2464 | `{` |
|   120 | 2465 | `	SXUNUSED(nArg);` |
|   120 | 2466 | `	SXUNUSED(apArg);` |
|   241 | 2467 | `	ph7_result_string(pCtx,pCtx->pVm->zDefTz,(int)pCtx->pVm->nDefTz);` |
|   241 | 2468 | `	return PH7_OK;` |
|     1 | 2469 | `}` |
|     - | 2470 | `/* string __dt_format(int $ts, int $off, string $tzname, string $format, int $us = 0) */` |
|   182 | 2471 | `static int vm_builtin_dt_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2472 | `{` |
|     - | 2473 | `	Sytm sTm;` |
|     - | 2474 | `	sxi64 iTs;` |
|     - | 2475 | `	sxi32 iOff;` |
|     - | 2476 | `	const char *zName,*zFmt;` |
|   183 | 2477 | `	int nName,nFmt,uSec = 0;` |
|     - | 2478 | `	char zZone[64];` |
|   183 | 2479 | `	if( nArg < 4 ){` |
|   ! 0 | 2480 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2481 | `		return PH7_OK;` |
|     - | 2482 | `	}` |
|   183 | 2483 | `	iTs  = ph7_value_to_int64(apArg[0]);` |
|   183 | 2484 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|   183 | 2485 | `	zName = ph7_value_to_string(apArg[2],&nName);` |
|   183 | 2486 | `	zFmt  = ph7_value_to_string(apArg[3],&nFmt);` |
|   183 | 2487 | `	if( nArg > 4 ){ uSec = ph7_value_to_int(apArg[4]); }` |
|   183 | 2488 | `	if( nName >= (int)sizeof(zZone) ){ nName = (int)sizeof(zZone) - 1; }` |
|   183 | 2489 | `	SyMemcpy(zName,zZone,(sxu32)nName);` |
|   183 | 2490 | `	zZone[nName] = 0;` |
|   183 | 2491 | `	DtFillSytm(iTs,iOff,zZone,&sTm);` |
|   183 | 2492 | `	DateFormat(pCtx,zFmt,nFmt,&sTm,uSec);` |
|   183 | 2493 | `	return PH7_OK;` |
|    92 | 2494 | `}` |
|     - | 2495 | `/* int __dt_make(int y, int mo, int d, int h, int i, int s, int off) */` |
|     8 | 2496 | `static int vm_builtin_dt_make(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2497 | `{` |
|     - | 2498 | `	sxi64 y;` |
|     - | 2499 | `	int mo,d,h,mi,s;` |
|     - | 2500 | `	sxi32 iOff;` |
|     9 | 2501 | `	if( nArg < 7 ){` |
|   ! 0 | 2502 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2503 | `		return PH7_OK;` |
|     - | 2504 | `	}` |
|     9 | 2505 | `	y   = ph7_value_to_int64(apArg[0]);` |
|     9 | 2506 | `	mo  = ph7_value_to_int(apArg[1]);` |
|     9 | 2507 | `	d   = ph7_value_to_int(apArg[2]);` |
|     9 | 2508 | `	h   = ph7_value_to_int(apArg[3]);` |
|     9 | 2509 | `	mi  = ph7_value_to_int(apArg[4]);` |
|     9 | 2510 | `	s   = ph7_value_to_int(apArg[5]);` |
|     9 | 2511 | `	iOff = (sxi32)ph7_value_to_int64(apArg[6]);` |
|     9 | 2512 | `	ph7_result_int64(pCtx,DtMakeTs(y,mo,d,h,mi,s,iOff));` |
|     9 | 2513 | `	return PH7_OK;` |
|     5 | 2514 | `}` |
|     - | 2515 | `/* Days in a civil month (php's overflow rules use it during diff borrows) */` |
|    62 | 2516 | `static int DtDaysInMonth(sxi64 y,int m)` |
|     1 | 2517 | `{` |
|     - | 2518 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|    63 | 2519 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|     9 | 2520 | `		return 29;` |
|     - | 2521 | `	}` |
|    55 | 2522 | `	return aMonDays[(m - 1) % 12];` |
|    32 | 2523 | `}` |
|     - | 2524 | `/* int __dt_civil_add(int ts, int off, int y, int m, int d, int h, int i,` |
|     - | 2525 | ` *                    int s, int sign)` |
|     - | 2526 | ` *   php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|     - | 2527 | ` *   (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset. */` |
|    54 | 2528 | `static int vm_builtin_dt_civil_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2529 | `{` |
|     - | 2530 | `	sxi64 iTs,iLocal,iDays,iSecs,y0,moT,dayCount;` |
|     - | 2531 | `	sxi32 iOff;` |
|     - | 2532 | `	int mo0,d0,iSign;` |
|     - | 2533 | `	sxi64 y,m,d,h,i,s;` |
|    55 | 2534 | `	if( nArg < 9 ){` |
|   ! 0 | 2535 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2536 | `		return PH7_OK;` |
|     - | 2537 | `	}` |
|    55 | 2538 | `	iTs   = ph7_value_to_int64(apArg[0]);` |
|    55 | 2539 | `	iOff  = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    55 | 2540 | `	y     = ph7_value_to_int64(apArg[2]);` |
|    55 | 2541 | `	m     = ph7_value_to_int64(apArg[3]);` |
|    55 | 2542 | `	d     = ph7_value_to_int64(apArg[4]);` |
|    55 | 2543 | `	h     = ph7_value_to_int64(apArg[5]);` |
|    55 | 2544 | `	i     = ph7_value_to_int64(apArg[6]);` |
|    55 | 2545 | `	s     = ph7_value_to_int64(apArg[7]);` |
|    55 | 2546 | `	iSign = ph7_value_to_int(apArg[8]) < 0 ? -1 : 1;` |
|    55 | 2547 | `	iLocal = iTs + iOff;` |
|    55 | 2548 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    55 | 2549 | `	iSecs  = iLocal - iDays*86400;` |
|    55 | 2550 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    55 | 2551 | `	y0 += iSign * y;` |
|    55 | 2552 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    55 | 2553 | `	y0 += DtFloorDiv(moT,12);` |
|    55 | 2554 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    55 | 2555 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    55 | 2556 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    55 | 2557 | `	ph7_result_int64(pCtx,iLocal - iOff);` |
|    55 | 2558 | `	return PH7_OK;` |
|    28 | 2559 | `}` |
|     - | 2560 | `/* array __dt_civil_diff(int ts1, int off1, int ts2)` |
|     - | 2561 | ` *   -> [y,m,d,h,i,s,days,invert]: timelib's breakdown — field-wise deltas in` |
|     - | 2562 | ` *   the FIRST operand's offset, then borrow seconds→minutes→hours→days, then` |
|     - | 2563 | ` *   the day borrow walks whole months backward from the later date (that walk` |
|     - | 2564 | ` *   is why Jan 31 → Mar 02 reports m=0 d=30, not "1 month"). */` |
|    14 | 2565 | `static int vm_builtin_dt_civil_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2566 | `{` |
|     - | 2567 | `	sxi64 iTs1,iTs2,iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|     - | 2568 | `	sxi32 iOff;` |
|     - | 2569 | `	int moA,dA,moB,dB,bInvert;` |
|     - | 2570 | `	sxi64 sA,sB,y,m,d,h,i,s;` |
|     - | 2571 | `	ph7_value *pArr,*pV;` |
|    15 | 2572 | `	if( nArg < 3 ){` |
|   ! 0 | 2573 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2574 | `		return PH7_OK;` |
|     - | 2575 | `	}` |
|    15 | 2576 | `	iTs1 = ph7_value_to_int64(apArg[0]);` |
|    15 | 2577 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    15 | 2578 | `	iTs2 = ph7_value_to_int64(apArg[2]);` |
|    15 | 2579 | `	bInvert = iTs1 > iTs2;` |
|    15 | 2580 | `	iA = bInvert ? iTs2 : iTs1;` |
|    15 | 2581 | `	iB = bInvert ? iTs1 : iTs2;` |
|    15 | 2582 | `	iLa = iA + iOff;` |
|    15 | 2583 | `	iLb = iB + iOff;` |
|    15 | 2584 | `	daysA = DtFloorDiv(iLa,86400);` |
|    15 | 2585 | `	daysB = DtFloorDiv(iLb,86400);` |
|    15 | 2586 | `	sA = iLa - daysA*86400;` |
|    15 | 2587 | `	sB = iLb - daysB*86400;` |
|    15 | 2588 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|    15 | 2589 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|    15 | 2590 | `	s = (sB % 60) - (sA % 60);` |
|    15 | 2591 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|    15 | 2592 | `	h = (sB / 3600) - (sA / 3600);` |
|    15 | 2593 | `	d = dB - dA;` |
|    15 | 2594 | `	m = moB - moA;` |
|    15 | 2595 | `	y = yB - yA;` |
|    15 | 2596 | `	if( s < 0 ){ s += 60; i--; }` |
|    15 | 2597 | `	if( i < 0 ){ i += 60; h--; }` |
|    15 | 2598 | `	if( h < 0 ){ h += 24; d--; }` |
|    27 | 2599 | `	while( d < 0 ){` |
|    13 | 2600 | `		moB--;` |
|    13 | 2601 | `		if( moB < 1 ){ moB = 12; yB--; }` |
|    13 | 2602 | `		d += DtDaysInMonth(yB,moB);` |
|    13 | 2603 | `		m--;` |
|     1 | 2604 | `	}` |
|    15 | 2605 | `	if( m < 0 ){ m += 12; y--; }` |
|    15 | 2606 | `	pArr = ph7_context_new_array(pCtx);` |
|    15 | 2607 | `	pV = ph7_context_new_scalar(pCtx);` |
|    15 | 2608 | `	if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2609 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2610 | `	}` |
|    15 | 2611 | `	ph7_value_int64(pV,y);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2612 | `	ph7_value_int64(pV,m);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2613 | `	ph7_value_int64(pV,d);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2614 | `	ph7_value_int64(pV,h);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2615 | `	ph7_value_int64(pV,i);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2616 | `	ph7_value_int64(pV,s);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2617 | `	ph7_value_int64(pV,(iB - iA) / 86400); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2618 | `	ph7_value_int64(pV,bInvert); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2619 | `	ph7_result_value(pCtx,pArr);` |
|    15 | 2620 | `	return PH7_OK;` |
|     8 | 2621 | `}` |
|     - | 2622 | `/* int __dt_isodate(int ts, int off, int y, int w, int dow)` |
|     - | 2623 | ` *   setISODate: jump to ISO year/week/weekday, preserving the time of day. */` |
|     8 | 2624 | `static int vm_builtin_dt_isodate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2625 | `{` |
|     - | 2626 | `	sxi64 iTs,iLocal,iTod,jan4,monday1,target,y;` |
|     - | 2627 | `	sxi32 iOff;` |
|     - | 2628 | `	sxi64 w,dow;` |
|     - | 2629 | `	int isoDow;` |
|     9 | 2630 | `	if( nArg < 5 ){` |
|   ! 0 | 2631 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2632 | `		return PH7_OK;` |
|     - | 2633 | `	}` |
|     9 | 2634 | `	iTs = ph7_value_to_int64(apArg[0]);` |
|     9 | 2635 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|     9 | 2636 | `	y   = ph7_value_to_int64(apArg[2]);` |
|     9 | 2637 | `	w   = ph7_value_to_int64(apArg[3]);` |
|     9 | 2638 | `	dow = ph7_value_to_int64(apArg[4]);` |
|     9 | 2639 | `	iLocal = iTs + iOff;` |
|     9 | 2640 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|     9 | 2641 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|     9 | 2642 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|     9 | 2643 | `	monday1 = jan4 - (isoDow - 1);` |
|     9 | 2644 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|     9 | 2645 | `	ph7_result_int64(pCtx,target*86400 + iTod - iOff);` |
|     9 | 2646 | `	return PH7_OK;` |
|     5 | 2647 | `}` |
|     - | 2648 | `/* Consume nMin..nMax digits from *pz; returns count consumed (0 = failure) */` |
|   184 | 2649 | `static int DtEatDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2650 | `{` |
|   185 | 2651 | `	const char *z = *pz;` |
|   185 | 2652 | `	sxi64 v = 0;` |
|   185 | 2653 | `	int n = 0;` |
|   663 | 2654 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|   479 | 2655 | `		v = v*10 + (z[0] - '0');` |
|   479 | 2656 | `		z++;` |
|   479 | 2657 | `		n++;` |
|     1 | 2658 | `	}` |
|   185 | 2659 | `	if( n < nMin ){` |
|     5 | 2660 | `		return 0;` |
|     - | 2661 | `	}` |
|   181 | 2662 | `	*pz = z;` |
|   181 | 2663 | `	*pVal = v;` |
|   181 | 2664 | `	return n;` |
|    93 | 2665 | `}` |
|     - | 2666 | `/* timelib_get_nr's recovery: skip non-digits hunting for the field.` |
|     - | 2667 | ` * Returns 1 = found+read, 0 = digits present but short, -1 = exhausted. */` |
|     4 | 2668 | `static int DtHuntDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2669 | `{` |
|     5 | 2670 | `	const char *z = *pz;` |
|    29 | 2671 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|     5 | 2672 | `	*pz = z;` |
|     5 | 2673 | `	if( z >= zEnd ){` |
|     5 | 2674 | `		return -1;` |
|     - | 2675 | `	}` |
|   ! 0 | 2676 | `	return DtEatDigits(pz,zEnd,nMin,nMax,pVal) ? 1 : 0;` |
|     3 | 2677 | `}` |
|     - | 2678 | `/* Case-insensitive name-table lookup; returns 1-based index or 0 */` |
|    14 | 2679 | `static int DtEatName(const char **pz,const char *zEnd,const char **azNames,int nNames)` |
|     1 | 2680 | `{` |
|     - | 2681 | `	int k;` |
|    23 | 2682 | `	for( k = 0 ; k < nNames ; k++ ){` |
|    23 | 2683 | `		int n = (int)SyStrlen(azNames[k]);` |
|    23 | 2684 | `		if( zEnd - *pz >= n && SyStrnicmp(*pz,azNames[k],(sxu32)n) == 0 ){` |
|    15 | 2685 | `			*pz += n;` |
|    15 | 2686 | `			return k + 1;` |
|     - | 2687 | `		}` |
|     5 | 2688 | `	}` |
|   ! 0 | 2689 | `	return 0;` |
|     8 | 2690 | `}` |
|     - | 2691 | `/* mixed __dt_from_format(string fmt, string input, int nowTs, int defOff)` |
|     - | 2692 | ` *   php's DateTime::createFromFormat engine. Success: [ts, off, offKind, name]` |
|     - | 2693 | ` *   where offKind 0=none-parsed, 1=numeric offset, 2=literal Z, 3=named id.` |
|     - | 2694 | ` *   Failure: "POS\tMESSAGE" (timelib's message strings; PHL reports the FIRST` |
|     - | 2695 | ` *   error where php may accumulate several — recorded). A trailing-data` |
|     - | 2696 | ` *   warning rides as [4]=pos, [5]=msg on the success array. */` |
|    58 | 2697 | `static int vm_builtin_dt_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2698 | `{` |
|     - | 2699 | `	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};` |
|     - | 2700 | `	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",` |
|     - | 2701 | `		"thursday","friday","saturday"};` |
|     - | 2702 | `	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",` |
|     - | 2703 | `		"aug","sep","oct","nov","dec"};` |
|     - | 2704 | `	static const char *azMonFull[] = {"january","february","march","april",` |
|     - | 2705 | `		"may","june","july","august","september","october","november","december"};` |
|     - | 2706 | `	const char *zFmt,*zIn,*zEnd,*zInEnd,*z;` |
|     - | 2707 | `	int nFmt,nIn;` |
|     - | 2708 | `	sxi64 iNow,v;` |
|     - | 2709 | `	sxi32 iDefOff;` |
|     - | 2710 | `	/* -1 == unset */` |
|    59 | 2711 | `	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;` |
|    59 | 2712 | `	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;` |
|    59 | 2713 | `	int uSecFF = 0,bHasUs = 0;` |
|    59 | 2714 | `	int iOffKind = 0;` |
|    59 | 2715 | `	sxi32 iOffVal = 0;` |
|     - | 2716 | `	char zName[16];` |
|    59 | 2717 | `	const char *zErr = 0;` |
|     - | 2718 | `	const char *aWarnMsg[3];` |
|     - | 2719 | `	int aWarnPos[3];` |
|    59 | 2720 | `	int nWarn = 0,bAborted = 0;` |
|     - | 2721 | `	const char *aErrMsg[8];` |
|     - | 2722 | `	int aErrPos[8];` |
|    59 | 2723 | `	int nErr = 0,nErrKept = 0;` |
|    59 | 2724 | `	if( nArg < 4 ){` |
|   ! 0 | 2725 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2726 | `		return PH7_OK;` |
|     - | 2727 | `	}` |
|    59 | 2728 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    59 | 2729 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    59 | 2730 | `	iNow = ph7_value_to_int64(apArg[2]);` |
|    59 | 2731 | `	iDefOff = (sxi32)ph7_value_to_int64(apArg[3]);` |
|    59 | 2732 | `	zEnd = &zFmt[nFmt];` |
|    59 | 2733 | `	zInEnd = &zIn[nIn];` |
|    59 | 2734 | `	z = zIn;` |
|    59 | 2735 | `	zName[0] = 0;` |
|     - | 2736 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|     - | 2737 | `	{ int _p = (iPos),_k,_f = -1; \` |
|     - | 2738 | `	  nErr++; \` |
|     - | 2739 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|     - | 2740 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|     - | 2741 | `	  else if( nErrKept < 8 ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|   411 | 2742 | `	while( zFmt < zEnd ){` |
|   357 | 2743 | `		char c = zFmt[0];` |
|   357 | 2744 | `		zFmt++;` |
|   357 | 2745 | `		zErr = 0;` |
|   357 | 2746 | `		if( c == '!' ){` |
|    11 | 2747 | `			y = 1970; mo = 1; d = 1; h = 0; mi = 0; s = 0;` |
|    11 | 2748 | `			h12 = -1; iMeridiem = -1;` |
|    11 | 2749 | `			continue;` |
|     - | 2750 | `		}` |
|   347 | 2751 | `		if( c == '\|' ){ bPipe = 1; continue; }` |
|   343 | 2752 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|   341 | 2753 | `		if( z >= zInEnd ){` |
|     - | 2754 | `			/* timelib aborts the scan once input is exhausted */` |
|     9 | 2755 | `			DT_FF_LOGERR(nIn,"Not enough data available to satisfy format");` |
|     5 | 2756 | `			break;` |
|     - | 2757 | `		}` |
|   337 | 2758 | `		switch( c ){` |
|    21 | 2759 | `		case 'd': case 'j':` |
|    43 | 2760 | `			if( !DtEatDigits(&z,zInEnd,1,2,&d) ){` |
|   ! 0 | 2761 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit day could not be found");` |
|   ! 0 | 2762 | `				if( DtHuntDigits(&z,zInEnd,1,2,&d) < 0 ){` |
|   ! 0 | 2763 | `					DT_FF_LOGERR(nIn,"A two digit day could not be found");` |
|   ! 0 | 2764 | `				}` |
|   ! 0 | 2765 | `			}` |
|    43 | 2766 | `			break;` |
|     1 | 2767 | `		case 'D':` |
|     3 | 2768 | `			if( !DtEatName(&z,zInEnd,azDay3,7) ){` |
|   ! 0 | 2769 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2770 | `			}` |
|     3 | 2771 | `			break;` |
|     1 | 2772 | `		case 'l':` |
|     3 | 2773 | `			if( !DtEatName(&z,zInEnd,azDayFull,7) ){` |
|   ! 0 | 2774 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2775 | `			}` |
|     3 | 2776 | `			break;` |
|     1 | 2777 | `		case 'S':` |
|     - | 2778 | `			/* ordinal suffix: st nd rd th */` |
|     4 | 2779 | `			if( zInEnd-z >= 2 && ((z[0]=='s'&&z[1]=='t')\|\|(z[0]=='n'&&z[1]=='d')` |
|     2 | 2780 | `			 \|\|(z[0]=='r'&&z[1]=='d')\|\|(z[0]=='t'&&z[1]=='h')) ){` |
|     3 | 2781 | `				z += 2;` |
|     1 | 2782 | `			}` |
|     3 | 2783 | `			break;` |
|    19 | 2784 | `		case 'm': case 'n':` |
|    39 | 2785 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mo) ){` |
|   ! 0 | 2786 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit month could not be found");` |
|   ! 0 | 2787 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mo) < 0 ){` |
|   ! 0 | 2788 | `					DT_FF_LOGERR(nIn,"A two digit month could not be found");` |
|   ! 0 | 2789 | `				}` |
|   ! 0 | 2790 | `			}` |
|    39 | 2791 | `			break;` |
|     1 | 2792 | `		case 'M':{` |
|     3 | 2793 | `			int k = DtEatName(&z,zInEnd,azMon3,12);` |
|     3 | 2794 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2795 | `			break;` |
|     - | 2796 | `				 }` |
|     1 | 2797 | `		case 'F':{` |
|     3 | 2798 | `			int k = DtEatName(&z,zInEnd,azMonFull,12);` |
|     3 | 2799 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2800 | `			break;` |
|     - | 2801 | `				 }` |
|   ! 0 | 2802 | `		case 'y':` |
|   ! 0 | 2803 | `			if( DtEatDigits(&z,zInEnd,2,2,&y) ){` |
|   ! 0 | 2804 | `				y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2805 | `			}else{` |
|   ! 0 | 2806 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit year could not be found");` |
|   ! 0 | 2807 | `				if( DtHuntDigits(&z,zInEnd,2,2,&y) < 0 ){` |
|   ! 0 | 2808 | `					DT_FF_LOGERR(nIn,"A two digit year could not be found");` |
|   ! 0 | 2809 | `				}else if( y >= 0 ){` |
|   ! 0 | 2810 | `					y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2811 | `				}` |
|     - | 2812 | `			}` |
|   ! 0 | 2813 | `			break;` |
|    24 | 2814 | `		case 'Y':{` |
|    49 | 2815 | `			int neg = 0;` |
|    49 | 2816 | `			if( z < zInEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|    49 | 2817 | `			if( DtEatDigits(&z,zInEnd,1,4,&y) ){` |
|    45 | 2818 | `				if( neg ){ y = -y; }` |
|    23 | 2819 | `			}else{` |
|     5 | 2820 | `				DT_FF_LOGERR((int)(z - zIn),"A four digit year could not be found");` |
|     5 | 2821 | `				if( DtHuntDigits(&z,zInEnd,1,4,&y) < 0 ){` |
|     9 | 2822 | `					DT_FF_LOGERR(nIn,"A four digit year could not be found");` |
|     2 | 2823 | `				}` |
|     - | 2824 | `			}` |
|    49 | 2825 | `			break;` |
|     - | 2826 | `				 }` |
|     7 | 2827 | `		case 'H': case 'G':` |
|    15 | 2828 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h) ){` |
|   ! 0 | 2829 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2830 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h) < 0 ){` |
|   ! 0 | 2831 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2832 | `				}` |
|   ! 0 | 2833 | `			}` |
|    15 | 2834 | `			break;` |
|     2 | 2835 | `		case 'h': case 'g':` |
|     5 | 2836 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h12) ){` |
|   ! 0 | 2837 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2838 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h12) < 0 ){` |
|   ! 0 | 2839 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2840 | `				}` |
|   ! 0 | 2841 | `			}` |
|     5 | 2842 | `			break;` |
|     9 | 2843 | `		case 'i':` |
|    19 | 2844 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mi) ){` |
|   ! 0 | 2845 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit minute could not be found");` |
|   ! 0 | 2846 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mi) < 0 ){` |
|   ! 0 | 2847 | `					DT_FF_LOGERR(nIn,"A two digit minute could not be found");` |
|   ! 0 | 2848 | `				}` |
|   ! 0 | 2849 | `			}` |
|    19 | 2850 | `			break;` |
|     3 | 2851 | `		case 's':` |
|     7 | 2852 | `			if( !DtEatDigits(&z,zInEnd,1,2,&s) ){` |
|   ! 0 | 2853 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit second could not be found");` |
|   ! 0 | 2854 | `				if( DtHuntDigits(&z,zInEnd,1,2,&s) < 0 ){` |
|   ! 0 | 2855 | `					DT_FF_LOGERR(nIn,"A two digit second could not be found");` |
|   ! 0 | 2856 | `				}` |
|   ! 0 | 2857 | `			}` |
|     7 | 2858 | `			break;` |
|     1 | 2859 | `		case 'u':{` |
|     - | 2860 | `			/* Microseconds: the digits parsed are right-padded to 6 (".5" -> 500000). */` |
|     3 | 2861 | `			const char *zStart = z;` |
|     3 | 2862 | `			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){` |
|   ! 0 | 2863 | `				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");` |
|   ! 0 | 2864 | `				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){` |
|   ! 0 | 2865 | `					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");` |
|   ! 0 | 2866 | `				}else{` |
|   ! 0 | 2867 | `					zStart = z; /* HuntDigits repositioned; treat as freshly read */` |
|     - | 2868 | `				}` |
|   ! 0 | 2869 | `			}` |
|     - | 2870 | `			{` |
|     3 | 2871 | `				int nd = (int)(z - zStart);` |
|     3 | 2872 | `				while( nd > 0 && nd < 6 ){ v *= 10; nd++; }` |
|     3 | 2873 | `				uSecFF = (int)v; bHasUs = 1;` |
|     - | 2874 | `			}` |
|     3 | 2875 | `			break;` |
|     - | 2876 | `				 }` |
|   ! 0 | 2877 | `		case 'v':{` |
|   ! 0 | 2878 | `			const char *zStart = z;` |
|   ! 0 | 2879 | `			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){` |
|   ! 0 | 2880 | `				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");` |
|   ! 0 | 2881 | `				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){` |
|   ! 0 | 2882 | `					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");` |
|   ! 0 | 2883 | `				}else{` |
|   ! 0 | 2884 | `					zStart = z;` |
|     - | 2885 | `				}` |
|   ! 0 | 2886 | `			}` |
|     - | 2887 | `			{` |
|   ! 0 | 2888 | `				int nd = (int)(z - zStart);` |
|   ! 0 | 2889 | `				while( nd > 0 && nd < 3 ){ v *= 10; nd++; }` |
|   ! 0 | 2890 | `				uSecFF = (int)v * 1000; bHasUs = 1; /* ms -> us */` |
|     - | 2891 | `			}` |
|   ! 0 | 2892 | `			break;` |
|     - | 2893 | `				 }` |
|     2 | 2894 | `		case 'a': case 'A':{` |
|     - | 2895 | `			static const char *azMer[] = {"am","pm","a.m.","p.m."};` |
|     5 | 2896 | `			int k = DtEatName(&z,zInEnd,azMer,4);` |
|     5 | 2897 | `			if( k ){` |
|     5 | 2898 | `				iMeridiem = ((k - 1) & 1);` |
|     3 | 2899 | `			}else{` |
|   ! 0 | 2900 | `				zErr = "A meridian could not be found";` |
|     - | 2901 | `			}` |
|     5 | 2902 | `			break;` |
|     - | 2903 | `				 }` |
|     2 | 2904 | `		case 'U':{` |
|     5 | 2905 | `			int neg = 0;` |
|     5 | 2906 | `			if( z < zInEnd && z[0]=='-' ){ neg = 1; z++; }` |
|     5 | 2907 | `			if( DtEatDigits(&z,zInEnd,1,19,&uVal) ){` |
|     5 | 2908 | `				if( neg ){ uVal = -uVal; }` |
|     5 | 2909 | `				bHasU = 1;` |
|     3 | 2910 | `			}else{` |
|   ! 0 | 2911 | `				DT_FF_LOGERR((int)(z - zIn),"A unix timestamp could not be found");` |
|   ! 0 | 2912 | `				if( DtHuntDigits(&z,zInEnd,1,19,&uVal) < 0 ){` |
|   ! 0 | 2913 | `					DT_FF_LOGERR(nIn,"A unix timestamp could not be found");` |
|   ! 0 | 2914 | `				}else{` |
|   ! 0 | 2915 | `					if( neg ){ uVal = -uVal; }` |
|   ! 0 | 2916 | `					bHasU = 1;` |
|     - | 2917 | `				}` |
|     - | 2918 | `			}` |
|     5 | 2919 | `			break;` |
|     - | 2920 | `				 }` |
|     1 | 2921 | `		case 'e': case 'T':{` |
|     - | 2922 | `			static const char *azZone[] = {"UTC","GMT","Z"};` |
|     3 | 2923 | `			int k = DtEatName(&z,zInEnd,azZone,3);` |
|     3 | 2924 | `			if( k == 3 ){` |
|   ! 0 | 2925 | `				iOffKind = 2; iOffVal = 0;` |
|     3 | 2926 | `			}else if( k ){` |
|     3 | 2927 | `				iOffKind = 3; iOffVal = 0;` |
|     3 | 2928 | `				SyMemcpy(azZone[k-1],zName,4);` |
|     1 | 2929 | `			}else if( z < zInEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|   ! 0 | 2930 | `				goto parse_num_off;` |
|   ! 0 | 2931 | `			}else{` |
|   ! 0 | 2932 | `				zErr = "The timezone could not be found in the database";` |
|     - | 2933 | `			}` |
|     3 | 2934 | `			break;` |
|     2 | 2935 | `				 }` |
|     - | 2936 | `		case 'O': case 'P':` |
|     2 | 2937 | `parse_num_off:	{` |
|     5 | 2938 | `			int sign,oh,om = 0;` |
|     - | 2939 | `			sxi64 t;` |
|     5 | 2940 | `			if( z >= zInEnd \|\| (z[0] != '+' && z[0] != '-') ){` |
|   ! 0 | 2941 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2942 | `				break;` |
|     - | 2943 | `			}` |
|     5 | 2944 | `			sign = (z[0]=='-') ? -1 : 1;` |
|     5 | 2945 | `			z++;` |
|     5 | 2946 | `			if( !DtEatDigits(&z,zInEnd,2,2,&t) ){` |
|   ! 0 | 2947 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2948 | `				break;` |
|     - | 2949 | `			}` |
|     5 | 2950 | `			oh = (int)t;` |
|     5 | 2951 | `			if( z < zInEnd && z[0]==':' ){ z++; }` |
|     5 | 2952 | `			if( DtEatDigits(&z,zInEnd,2,2,&t) ){ om = (int)t; }` |
|     5 | 2953 | `			iOffKind = 1;` |
|     5 | 2954 | `			iOffVal = sign * (oh*3600 + om*60);` |
|     5 | 2955 | `			break;` |
|     - | 2956 | `				 }` |
|   ! 0 | 2957 | `		case '?':` |
|   ! 0 | 2958 | `			if( z < zInEnd ){ z++; }` |
|   ! 0 | 2959 | `			break;` |
|   ! 0 | 2960 | `		case '*':` |
|     - | 2961 | `			/* skip input until the next separator byte */` |
|   ! 0 | 2962 | `			while( z < zInEnd && !SyisDigit(z[0]) && z[0] != ';' && z[0] != ':'` |
|   ! 0 | 2963 | `			 && z[0] != '/' && z[0] != '.' && z[0] != ',' && z[0] != '-'` |
|   ! 0 | 2964 | `			 && z[0] != '(' && z[0] != ')' && z[0] != ' ' ){` |
|   ! 0 | 2965 | `				z++;` |
|   ! 0 | 2966 | `			}` |
|   ! 0 | 2967 | `			break;` |
|     1 | 2968 | `		case '#':` |
|     3 | 2969 | `			if( z < zInEnd && (z[0]==';'\|\|z[0]==':'\|\|z[0]=='/'\|\|z[0]=='.'` |
|   ! 0 | 2970 | `			 \|\|z[0]==','\|\|z[0]=='-'\|\|z[0]=='('\|\|z[0]==')') ){` |
|     3 | 2971 | `				z++;` |
|     2 | 2972 | `			}else{` |
|   ! 0 | 2973 | `				zErr = "The separation symbol could not be found";` |
|     - | 2974 | `			}` |
|     3 | 2975 | `			break;` |
|     1 | 2976 | `		case '\\':` |
|     3 | 2977 | `			if( zFmt < zEnd ){` |
|     3 | 2978 | `				if( z < zInEnd && z[0] == zFmt[0] ){` |
|     3 | 2979 | `					z++;` |
|     3 | 2980 | `					zFmt++;` |
|     2 | 2981 | `				}else{` |
|     - | 2982 | `					/* a literal mismatch aborts timelib's scan */` |
|   ! 0 | 2983 | `					DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|   ! 0 | 2984 | `					zFmt = zEnd;` |
|   ! 0 | 2985 | `					bAborted = 1;` |
|     - | 2986 | `				}` |
|     1 | 2987 | `			}` |
|     3 | 2988 | `			break;` |
|    51 | 2989 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|     - | 2990 | `		case '(' : case ')':` |
|   103 | 2991 | `			if( z < zInEnd && z[0] == c ){` |
|   103 | 2992 | `				z++;` |
|    52 | 2993 | `			}else{` |
|     - | 2994 | `				/* timelib logs BOTH messages (count +2, last-wins on the` |
|     - | 2995 | `				 * position), consumes the offending byte, and keeps going */` |
|   ! 0 | 2996 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 2997 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 2998 | `				z++;` |
|     - | 2999 | `			}` |
|   103 | 3000 | `			break;` |
|    16 | 3001 | `		case ' ':` |
|    33 | 3002 | `			if( z < zInEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|    33 | 3003 | `				z++;` |
|    17 | 3004 | `			}else{` |
|   ! 0 | 3005 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 3006 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 3007 | `				z++;` |
|     - | 3008 | `			}` |
|    33 | 3009 | `			break;` |
|     1 | 3010 | `		default:` |
|     - | 3011 | `			/* any other format byte must match the input verbatim; a mismatch` |
|     - | 3012 | `			 * aborts timelib's scan */` |
|     3 | 3013 | `			if( z < zInEnd && z[0] == c ){` |
|   ! 0 | 3014 | `				z++;` |
|   ! 0 | 3015 | `			}else{` |
|     3 | 3016 | `				DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|     3 | 3017 | `				zFmt = zEnd;` |
|     3 | 3018 | `				bAborted = 1;` |
|     - | 3019 | `			}` |
|     2 | 3020 | `			break;` |
|     - | 3021 | `		}` |
|   337 | 3022 | `		if( zErr ){` |
|     - | 3023 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|   ! 0 | 3024 | `			DT_FF_LOGERR((int)(z - zIn),zErr);` |
|   ! 0 | 3025 | `		}` |
|     1 | 3026 | `	}` |
|    59 | 3027 | `	if( z < zInEnd && !bAborted ){` |
|     5 | 3028 | `		if( bPlus ){` |
|     - | 3029 | `			/* '+' downgrades trailing data to a warning */` |
|     3 | 3030 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     3 | 3031 | `			aWarnMsg[nWarn] = "Trailing data";` |
|     3 | 3032 | `			nWarn++;` |
|     2 | 3033 | `		}else{` |
|     3 | 3034 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|     - | 3035 | `		}` |
|     2 | 3036 | `	}` |
|    59 | 3037 | `	if( nErr > 0 ){` |
|     - | 3038 | `		SyBlob sOut;` |
|     - | 3039 | `		int k;` |
|     9 | 3040 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     9 | 3041 | `		SyBlobFormat(&sOut,"%d",nErr);` |
|    21 | 3042 | `		for( k = 0 ; k < nErrKept ; k++ ){` |
|    13 | 3043 | `			SyBlobFormat(&sOut,"\n%d\t%s",aErrPos[k],aErrMsg[k]);` |
|     7 | 3044 | `		}` |
|     9 | 3045 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     9 | 3046 | `		SyBlobRelease(&sOut);` |
|     9 | 3047 | `		return PH7_OK;` |
|     - | 3048 | `	}` |
|    51 | 3049 | `	if( bPipe ){` |
|     5 | 3050 | `		if( y < 0 ){ y = 1970; }` |
|     5 | 3051 | `		if( mo < 0 ){ mo = 1; }` |
|     5 | 3052 | `		if( d < 0 ){ d = 1; }` |
|     5 | 3053 | `		if( h < 0 && h12 < 0 ){ h = 0; }` |
|     5 | 3054 | `		if( mi < 0 ){ mi = 0; }` |
|     5 | 3055 | `		if( s < 0 ){ s = 0; }` |
|     2 | 3056 | `	}` |
|     - | 3057 | `	{` |
|     - | 3058 | `		/* remaining unset fields come from "now" in the default offset */` |
|    51 | 3059 | `		sxi64 iLocal = iNow + iDefOff;` |
|    51 | 3060 | `		sxi64 days = DtFloorDiv(iLocal,86400);` |
|    51 | 3061 | `		sxi64 secs = iLocal - days*86400;` |
|     - | 3062 | `		sxi64 ny;` |
|     - | 3063 | `		int nmo,nd;` |
|    51 | 3064 | `		DtCivilFromDays(days,&ny,&nmo,&nd);` |
|    51 | 3065 | `		if( y < 0 ){ y = ny; }` |
|    51 | 3066 | `		if( mo < 0 ){ mo = nmo; }` |
|    51 | 3067 | `		if( d < 0 ){ d = nd; }` |
|    51 | 3068 | `		if( h12 >= 0 ){` |
|     5 | 3069 | `			h = (h12 % 12) + ((iMeridiem == 1) ? 12 : 0);` |
|     2 | 3070 | `		}` |
|     - | 3071 | `		/* php: parsing a time component zeroes the finer unset units */` |
|    51 | 3072 | `		if( h >= 0 ){` |
|    27 | 3073 | `			if( mi < 0 ){ mi = 0; }` |
|    27 | 3074 | `			if( s < 0 ){ s = 0; }` |
|    38 | 3075 | `		}else if( mi >= 0 ){` |
|     3 | 3076 | `			if( s < 0 ){ s = 0; }` |
|     1 | 3077 | `		}` |
|    51 | 3078 | `		if( h < 0 ){ h = secs / 3600; }` |
|    51 | 3079 | `		if( mi < 0 ){ mi = (secs / 60) % 60; }` |
|    51 | 3080 | `		if( s < 0 ){ s = secs % 60; }` |
|     - | 3081 | `	}` |
|     - | 3082 | `	/* php validates the RESOLVED fields and warns (parse still succeeds,` |
|     - | 3083 | `	 * values roll over via civil arithmetic) */` |
|    51 | 3084 | `	if( mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo) ){` |
|     3 | 3085 | `		if( nWarn < 3 ){` |
|     3 | 3086 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 3087 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|     3 | 3088 | `			nWarn++;` |
|     1 | 3089 | `		}` |
|     1 | 3090 | `	}` |
|    51 | 3091 | `	if( h > 24 \|\| mi > 59 \|\| s > 59 ){` |
|     3 | 3092 | `		if( nWarn < 3 ){` |
|     3 | 3093 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 3094 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|     3 | 3095 | `			nWarn++;` |
|     1 | 3096 | `		}` |
|     1 | 3097 | `	}` |
|     - | 3098 | `	{` |
|    51 | 3099 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    51 | 3100 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|     - | 3101 | `		sxi64 iTs;` |
|    51 | 3102 | `		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;` |
|    51 | 3103 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 3104 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 3105 | `		}` |
|    51 | 3106 | `		if( bHasU ){` |
|     5 | 3107 | `			iTs = uVal;` |
|     5 | 3108 | `			iUseOff = 0;` |
|     5 | 3109 | `			iOffKind = 1;` |
|     3 | 3110 | `		}else{` |
|    47 | 3111 | `			iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);` |
|     - | 3112 | `		}` |
|    51 | 3113 | `		ph7_value_int64(pV,iTs);           ph7_array_add_elem(pArr,0,pV);` |
|    51 | 3114 | `		ph7_value_int64(pV,iUseOff);       ph7_array_add_elem(pArr,0,pV);` |
|    51 | 3115 | `		ph7_value_int64(pV,iOffKind);      ph7_array_add_elem(pArr,0,pV);` |
|    51 | 3116 | `		ph7_value_string(pV,zName,-1);     ph7_array_add_elem(pArr,0,pV);` |
|     - | 3117 | `		/* microseconds from a u/v token ride an associative key so they never` |
|     - | 3118 | `		 * collide with the numeric [4+] trailing-warning pairs */` |
|    51 | 3119 | `		if( bHasUs ){ ph7_value_int64(pV,uSecFF); ph7_array_add_strkey_elem(pArr,"us",pV); }` |
|     - | 3120 | `		{` |
|     - | 3121 | `			int k;` |
|    57 | 3122 | `			for( k = 0 ; k < nWarn ; k++ ){` |
|     7 | 3123 | `				ph7_value_int64(pV,aWarnPos[k]);` |
|     7 | 3124 | `				ph7_array_add_elem(pArr,0,pV);` |
|     7 | 3125 | `				ph7_value_string(pV,aWarnMsg[k],-1);` |
|     7 | 3126 | `				ph7_array_add_elem(pArr,0,pV);` |
|     4 | 3127 | `			}` |
|     - | 3128 | `		}` |
|    51 | 3129 | `		ph7_result_value(pCtx,pArr);` |
|     - | 3130 | `	}` |
|    51 | 3131 | `	return PH7_OK;` |
|    30 | 3132 | `}` |
|     - | 3133 | `/*` |
|     - | 3134 | ` * The embedded DateTime library. Timezone scope: UTC + fixed offsets.` |
|     - | 3135 | ` */` |
|     - | 3136 | `static const char zDateTimeLib[] =` |
|     - | 3137 | `"class DateException extends Exception {}"` |
|     - | 3138 | `"class DateMalformedStringException extends DateException {}"` |
|     - | 3139 | `"class DateInvalidTimeZoneException extends DateException {}"` |
|     - | 3140 | `"class DateMalformedIntervalStringException extends DateException {}"` |
|     - | 3141 | `"class DateMalformedPeriodStringException extends DateException {}"` |
|     - | 3142 | `"interface DateTimeInterface {"` |
|     - | 3143 | `" const ATOM = 'Y-m-d\\TH:i:sP';"` |
|     - | 3144 | `" const COOKIE = 'l, d-M-Y H:i:s T';"` |
|     - | 3145 | `" const ISO8601 = 'Y-m-d\\TH:i:sO';"` |
|     - | 3146 | `" const ISO8601_EXPANDED = 'X-m-d\\TH:i:sP';"` |
|     - | 3147 | `" const RFC822 = 'D, d M y H:i:s O';"` |
|     - | 3148 | `" const RFC850 = 'l, d-M-y H:i:s T';"` |
|     - | 3149 | `" const RFC1036 = 'D, d M y H:i:s O';"` |
|     - | 3150 | `" const RFC1123 = 'D, d M Y H:i:s O';"` |
|     - | 3151 | `" const RFC7231 = 'D, d M Y H:i:s \\G\\M\\T';"` |
|     - | 3152 | `" const RFC2822 = 'D, d M Y H:i:s O';"` |
|     - | 3153 | `" const RFC3339 = 'Y-m-d\\TH:i:sP';"` |
|     - | 3154 | `" const RFC3339_EXTENDED = 'Y-m-d\\TH:i:s.vP';"` |
|     - | 3155 | `" const RSS = 'D, d M Y H:i:s O';"` |
|     - | 3156 | `" const W3C = 'Y-m-d\\TH:i:sP';"` |
|     - | 3157 | `"}"` |
|     - | 3158 | `"class DateTimeZone {"` |
|     - | 3159 | `" private $__dtzOff = 0;"` |
|     - | 3160 | `" private $__dtzName = 'UTC';"` |
|     - | 3161 | `" public function __construct($timezone = 'UTC'){"` |
|     - | 3162 | `"  $tz = (string)$timezone;"` |
|     - | 3163 | `"  if( strcasecmp($tz, 'UTC') === 0 ){"` |
|     - | 3164 | `"   $this->__dtzOff = 0; $this->__dtzName = 'UTC';"` |
|     - | 3165 | `"   return;"` |
|     - | 3166 | `"  }"` |
|     - | 3167 | `"  if( $tz === 'Z' ){"` |
|     - | 3168 | `"   $this->__dtzOff = 0; $this->__dtzName = 'Z';"` |
|     - | 3169 | `"   return;"` |
|     - | 3170 | `"  }"` |
|     - | 3171 | `"  if( strcasecmp($tz, 'GMT') === 0 ){"` |
|     - | 3172 | `"   $this->__dtzOff = 0; $this->__dtzName = 'GMT';"` |
|     - | 3173 | `"   return;"` |
|     - | 3174 | `"  }"` |
|     - | 3175 | `"  $m = null;"` |
|     - | 3176 | `"  if( preg_match('/^([+-])(\\d{2}):?(\\d{2})$/', $tz, $m) ){"` |
|     - | 3177 | `"   $off = ((int)$m[2]) * 3600 + ((int)$m[3]) * 60;"` |
|     - | 3178 | `"   if( $m[1] === '-' ){ $off = -$off; }"` |
|     - | 3179 | `"   $this->__dtzOff = $off;"` |
|     - | 3180 | `"   $this->__dtzName = $m[1] . $m[2] . ':' . $m[3];"` |
|     - | 3181 | `"   return;"` |
|     - | 3182 | `"  }"` |
|     - | 3183 | `"  throw new DateInvalidTimeZoneException("` |
|     - | 3184 | `"   'DateTimeZone::__construct(): Unknown or bad timezone (' . $tz . ')');"` |
|     - | 3185 | `" }"` |
|     - | 3186 | `" public function getName(){ return $this->__dtzName; }"` |
|     - | 3187 | `" public function getOffset($datetime = null){ return $this->__dtzOff; }"` |
|     - | 3188 | `"}"` |
|     - | 3189 | `"trait __DtCoreT {"` |
|     - | 3190 | `" private $__dtTs = 0;"` |
|     - | 3191 | `" private $__dtOff = 0;"` |
|     - | 3192 | `" private $__dtName = 'UTC';"` |
|     - | 3193 | `" private $__dtUs = 0;"` |
|     - | 3194 | `" private function __dtInit($datetime, $timezone){"` |
|     - | 3195 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 3196 | `"  if( $timezone !== null ){"` |
|     - | 3197 | `"   $off = $timezone->getOffset($this);"` |
|     - | 3198 | `"   $name = $timezone->getName();"` |
|     - | 3199 | `"  }"` |
|     - | 3200 | `"  $r = __dt_parse((string)$datetime, __dt_now(), $off);"` |
|     - | 3201 | `"  if( is_string($r) ){ throw new DateMalformedStringException($r); }"` |
|     - | 3202 | `"  $this->__dtTs = $r[0];"` |
|     - | 3203 | `"  $this->__dtUs = $r[3];"` |
|     - | 3204 | `"  if( $r[2] ){"` |
|     - | 3205 | `"   $this->__dtOff = $r[1];"` |
|     - | 3206 | `"   $this->__dtName = $r[2] === 2 ? 'Z' : $this->__dtOffName($r[1]);"` |
|     - | 3207 | `"  }else{"` |
|     - | 3208 | `"   $this->__dtOff = $off;"` |
|     - | 3209 | `"   $this->__dtName = $name;"` |
|     - | 3210 | `"  }"` |
|     - | 3211 | `" }"` |
|     - | 3212 | `" private function __dtOffName($off){"` |
|     - | 3213 | `"  $s = $off < 0 ? '-' : '+';"` |
|     - | 3214 | `"  $a = $off < 0 ? -$off : $off;"` |
|     - | 3215 | `"  return $s . sprintf('%02d:%02d', intdiv($a, 3600), intdiv($a % 3600, 60));"` |
|     - | 3216 | `" }"` |
|     - | 3217 | `" public function format($format){ return __dt_format($this->__dtTs, $this->__dtOff, $this->__dtName, (string)$format, $this->__dtUs); }"` |
|     - | 3218 | `" public function getTimestamp(){ return $this->__dtTs; }"` |
|     - | 3219 | `" public function getMicrosecond(){ return $this->__dtUs; }"` |
|     - | 3220 | `" public function getOffset(){ return $this->__dtOff; }"` |
|     - | 3221 | `" public function getTimezone(){ return new DateTimeZone($this->__dtName); }"` |
|     - | 3222 | `" public function diff($targetObject, $absolute = false){"` |
|     - | 3223 | `"  $r = __dt_civil_diff($this->__dtTs, $this->__dtOff, $targetObject->getTimestamp());"` |
|     - | 3224 | `"  $iv = new DateInterval('P0D');"` |
|     - | 3225 | `"  $iv->y = $r[0]; $iv->m = $r[1]; $iv->d = $r[2];"` |
|     - | 3226 | `"  $iv->h = $r[3]; $iv->i = $r[4]; $iv->s = $r[5];"` |
|     - | 3227 | `"  $iv->days = $r[6];"` |
|     - | 3228 | `"  $iv->invert = $absolute ? 0 : $r[7];"` |
|     - | 3229 | `"  return $iv;"` |
|     - | 3230 | `" }"` |
|     - | 3231 | `" private function __dtAddTs($interval, $sign){"` |
|     - | 3232 | `"  if( $interval->invert ){ $sign = -$sign; }"` |
|     - | 3233 | `"  return __dt_civil_add($this->__dtTs, $this->__dtOff, $interval->y, $interval->m,"` |
|     - | 3234 | `"   $interval->d, $interval->h, $interval->i, $interval->s, $sign);"` |
|     - | 3235 | `" }"` |
|     - | 3236 | `" private static function __dtFromFormat($format, $datetime, $timezone, $class){"` |
|     - | 3237 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 3238 | `"  if( $timezone !== null ){"` |
|     - | 3239 | `"   $off = $timezone->getOffset(null);"` |
|     - | 3240 | `"   $name = $timezone->getName();"` |
|     - | 3241 | `"  }"` |
|     - | 3242 | `"  $r = __dt_from_format((string)$format, (string)$datetime, __dt_now(), $off);"` |
|     - | 3243 | `"  if( is_string($r) ){"` |
|     - | 3244 | `"   $lines = explode(\"\\n\", $r);"` |
|     - | 3245 | `"   $errs = [];"` |
|     - | 3246 | `"   $nl = count($lines);"` |
|     - | 3247 | `"   for( $k = 1; $k < $nl; $k++ ){"` |
|     - | 3248 | `"    $p = strpos($lines[$k], \"\\t\");"` |
|     - | 3249 | `"    $errs[(int)substr($lines[$k], 0, $p)] = substr($lines[$k], $p + 1);"` |
|     - | 3250 | `"   }"` |
|     - | 3251 | `"   DateTime::$__dtLastErr = ['warning_count' => 0, 'warnings' => [],"` |
|     - | 3252 | `"    'error_count' => (int)$lines[0], 'errors' => $errs];"` |
|     - | 3253 | `"   return false;"` |
|     - | 3254 | `"  }"` |
|     - | 3255 | `"  if( isset($r[4]) ){"` |
|     - | 3256 | `"   $warns = [];"` |
|     - | 3257 | `"   $wc = 0;"` |
|     - | 3258 | `"   for( $k = 4; isset($r[$k]); $k += 2 ){"` |
|     - | 3259 | `"    $warns[$r[$k]] = $r[$k + 1];"` |
|     - | 3260 | `"    $wc++;"` |
|     - | 3261 | `"   }"` |
|     - | 3262 | `"   DateTime::$__dtLastErr = ['warning_count' => $wc, 'warnings' => $warns,"` |
|     - | 3263 | `"    'error_count' => 0, 'errors' => []];"` |
|     - | 3264 | `"  }else{"` |
|     - | 3265 | `"   DateTime::$__dtLastErr = false;"` |
|     - | 3266 | `"  }"` |
|     - | 3267 | `"  $obj = new $class('@0');"` |
|     - | 3268 | `"  $obj->__dtTs = $r[0];"` |
|     - | 3269 | `"  $obj->__dtUs = $r['us'] ?? 0;"` |
|     - | 3270 | `"  if( $r[2] === 0 ){ $obj->__dtOff = $off; $obj->__dtName = $name; }"` |
|     - | 3271 | `"  elseif( $r[2] === 2 ){ $obj->__dtOff = 0; $obj->__dtName = 'Z'; }"` |
|     - | 3272 | `"  elseif( $r[2] === 3 ){ $obj->__dtOff = $r[1]; $obj->__dtName = $r[3]; }"` |
|     - | 3273 | `"  else { $obj->__dtOff = $r[1]; $obj->__dtName = $obj->__dtOffName($r[1]); }"` |
|     - | 3274 | `"  return $obj;"` |
|     - | 3275 | `" }"` |
|     - | 3276 | `" private static function __dtCopyOf($object, $class){"` |
|     - | 3277 | `"  $d = new $class('@0');"` |
|     - | 3278 | `"  $d->__dtTs = $object->getTimestamp();"` |
|     - | 3279 | `"  $d->__dtUs = $object->getMicrosecond();"` |
|     - | 3280 | `"  $d->__dtOff = $object->getOffset();"` |
|     - | 3281 | `"  $d->__dtName = $object->getTimezone()->getName();"` |
|     - | 3282 | `"  return $d;"` |
|     - | 3283 | `" }"` |
|     - | 3284 | `"}"` |
|     - | 3285 | `"class DateTime implements DateTimeInterface {"` |
|     - | 3286 | `" use __DtCoreT;"` |
|     - | 3287 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 3288 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 3289 | `" }"` |
|     - | 3290 | `" public function modify($modifier){"` |
|     - | 3291 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 3292 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTime::modify(): ' . $r); }"` |
|     - | 3293 | `"  $this->__dtTs = $r[0];"` |
|     - | 3294 | `"  return $this;"` |
|     - | 3295 | `" }"` |
|     - | 3296 | `" public function setTimestamp($timestamp){ $this->__dtTs = (int)$timestamp; $this->__dtUs = 0; return $this; }"` |
|     - | 3297 | `" public function setMicrosecond($microsecond){ $this->__dtUs = (int)$microsecond; return $this; }"` |
|     - | 3298 | `" public function setTimezone($timezone){"` |
|     - | 3299 | `"  $this->__dtOff = $timezone->getOffset($this);"` |
|     - | 3300 | `"  $this->__dtName = $timezone->getName();"` |
|     - | 3301 | `"  return $this;"` |
|     - | 3302 | `" }"` |
|     - | 3303 | `" public function setDate($year, $month, $day){"` |
|     - | 3304 | `"  $this->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 3305 | `"  return $this;"` |
|     - | 3306 | `" }"` |
|     - | 3307 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3308 | `"  $this->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 3309 | `"  $this->__dtUs = (int)$microsecond;"` |
|     - | 3310 | `"  return $this;"` |
|     - | 3311 | `" }"` |
|     - | 3312 | `" public function add($interval){ $this->__dtTs = $this->__dtAddTs($interval, 1); return $this; }"` |
|     - | 3313 | `" public function sub($interval){ $this->__dtTs = $this->__dtAddTs($interval, -1); return $this; }"` |
|     - | 3314 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 3315 | `"  $this->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 3316 | `"  return $this;"` |
|     - | 3317 | `" }"` |
|     - | 3318 | `" public static $__dtLastErr = false;"` |
|     - | 3319 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 3320 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 3321 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTime');"` |
|     - | 3322 | `" }"` |
|     - | 3323 | `" public static function createFromImmutable($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 3324 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 3325 | `"}"` |
|     - | 3326 | `"class DateTimeImmutable implements DateTimeInterface {"` |
|     - | 3327 | `" use __DtCoreT;"` |
|     - | 3328 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 3329 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 3330 | `" }"` |
|     - | 3331 | `" public function modify($modifier){"` |
|     - | 3332 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 3333 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTimeImmutable::modify(): ' . $r); }"` |
|     - | 3334 | `"  $c = clone $this;"` |
|     - | 3335 | `"  $c->__dtTs = $r[0];"` |
|     - | 3336 | `"  return $c;"` |
|     - | 3337 | `" }"` |
|     - | 3338 | `" public function setTimestamp($timestamp){ $c = clone $this; $c->__dtTs = (int)$timestamp; $c->__dtUs = 0; return $c; }"` |
|     - | 3339 | `" public function setMicrosecond($microsecond){ $c = clone $this; $c->__dtUs = (int)$microsecond; return $c; }"` |
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
|  3814 | 3596 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
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
|  3819 | 3614 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|  3819 | 3615 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
| 38145 | 3616 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 34331 | 3617 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 17168 | 3618 | `	}` |
|  3819 | 3619 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zDateTimeLib,sizeof(zDateTimeLib)-1);` |
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
