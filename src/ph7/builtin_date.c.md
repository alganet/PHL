# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1218/1662 lines (73.29%)

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
|     6 | 1652 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1653 | `{` |
|     7 | 1654 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1655 | `	const char *zId;` |
|     - | 1656 | `	int nId;` |
|     7 | 1657 | `	if( nArg < 1 ){` |
|   ! 0 | 1658 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1659 | `		return PH7_OK;` |
|     - | 1660 | `	}` |
|     7 | 1661 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|     7 | 1662 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|     7 | 1663 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|     7 | 1664 | `		pVm->zDefTz[3] = 0;` |
|     7 | 1665 | `		pVm->nDefTz = 3;` |
|     7 | 1666 | `		ph7_result_bool(pCtx,1);` |
|     7 | 1667 | `		return PH7_OK;` |
|     - | 1668 | `	}` |
|     - | 1669 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|     - | 1670 | `	 * — exactly php's notice shape here */` |
|   ! 0 | 1671 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|   ! 0 | 1672 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 1673 | `	return PH7_OK;` |
|     4 | 1674 | `}` |
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
|   472 | 1690 | `static sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|     1 | 1691 | `{` |
|     - | 1692 | `	sxi64 era;` |
|     - | 1693 | `	unsigned yoe,doy,doe;` |
|   473 | 1694 | `	y -= (m <= 2);` |
|   473 | 1695 | `	era = (y >= 0 ? y : y - 399) / 400;` |
|   473 | 1696 | `	yoe = (unsigned)(y - era * 400);` |
|   473 | 1697 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|   473 | 1698 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|   473 | 1699 | `	return era * 146097 + (sxi64)doe - 719468;` |
|     1 | 1700 | `}` |
|   254 | 1701 | `static void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|     1 | 1702 | `{` |
|     - | 1703 | `	sxi64 era;` |
|     - | 1704 | `	unsigned doe,yoe,doy,mp;` |
|   255 | 1705 | `	z += 719468;` |
|   255 | 1706 | `	era = (z >= 0 ? z : z - 146096) / 146097;` |
|   255 | 1707 | `	doe = (unsigned)(z - era * 146097);` |
|   255 | 1708 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|   255 | 1709 | `	*py = (sxi64)yoe + era * 400;` |
|   255 | 1710 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|   255 | 1711 | `	mp = (5 * doy + 2) / 153;` |
|   255 | 1712 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|   255 | 1713 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|   255 | 1714 | `	if( *pm <= 2 ){` |
|   155 | 1715 | `		*py += 1;` |
|    77 | 1716 | `	}` |
|   255 | 1717 | `}` |
|   440 | 1718 | `static sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|     1 | 1719 | `{` |
|   441 | 1720 | `	sxi64 q = a / b;` |
|   441 | 1721 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|     3 | 1722 | `		q--;` |
|     1 | 1723 | `	}` |
|   441 | 1724 | `	return q;` |
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
|    94 | 1747 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|     1 | 1748 | `{` |
|    95 | 1749 | `	return DtDaysFromCivil(y,mo,d) * 86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     1 | 1750 | `}` |
|     - | 1751 | `/* Month-arithmetic with php's overflow semantics (Jan 31 +1 month -> Mar 2/3):` |
|     - | 1752 | ` * normalize the month, keep the day — the civil day-count formula is linear in` |
|     - | 1753 | ` * d, so an out-of-range day simply lands in the following month. */` |
|     4 | 1754 | `static sxi64 DtAddMonths(sxi64 iTs,sxi32 iOff,sxi64 nMonths)` |
|     1 | 1755 | `{` |
|     5 | 1756 | `	sxi64 t = iTs + iOff;` |
|     5 | 1757 | `	sxi64 days = DtFloorDiv(t,86400);` |
|     5 | 1758 | `	sxi64 secs = t - days * 86400;` |
|     - | 1759 | `	sxi64 y;` |
|     - | 1760 | `	int mo,d;` |
|     - | 1761 | `	sxi64 m0;` |
|     5 | 1762 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|     5 | 1763 | `	m0 = (y * 12 + (mo - 1)) + nMonths;` |
|     5 | 1764 | `	y  = DtFloorDiv(m0,12);` |
|     5 | 1765 | `	mo = (int)(m0 - y * 12) + 1;` |
|     5 | 1766 | `	return DtDaysFromCivil(y,mo,d) * 86400 + secs - iOff;` |
|     1 | 1767 | `}` |
|     - | 1768 | `/*` |
|     - | 1769 | ` * Minimal php-datetime-string parser (slice 1): absolute forms` |
|     - | 1770 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]",` |
|     - | 1771 | ` * keywords today/midnight/noon/tomorrow/yesterday, and relative sequences` |
|     - | 1772 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". Returns 0 on` |
|     - | 1773 | ` * success (ts/off/bOffSet out), or the byte position of the first` |
|     - | 1774 | ` * unparseable character +1 (for php's "at position N" message).` |
|     - | 1775 | ` */` |
|   144 | 1776 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,` |
|     - | 1777 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet)` |
|     1 | 1778 | `{` |
|   145 | 1779 | `	const char *z = zIn, *zEnd = &zIn[nLen];` |
|   145 | 1780 | `	sxi64 iTs = iBaseTs;` |
|   145 | 1781 | `	sxi32 iOff = iBaseOff;` |
|   145 | 1782 | `	int bOffSet = 0;` |
|   145 | 1783 | `	int bAny = 0;` |
|     - | 1784 | `#define DT_SKIP_WS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|     - | 1785 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|     - | 1786 | `	&& (zEnd-z == (nKw) \|\| !SyisAlpha(z[(nKw)])))` |
|   217 | 1787 | `	DT_SKIP_WS();` |
|   145 | 1788 | `	if( z >= zEnd ){` |
|     - | 1789 | `		/* php: the empty string is "now" */` |
|   ! 0 | 1790 | `		*pTs = iTs;` |
|   ! 0 | 1791 | `		*pOff = iOff;` |
|   ! 0 | 1792 | `		*pbOffSet = bOffSet;` |
|   ! 0 | 1793 | `		return 0;` |
|     - | 1794 | `	}` |
|     - | 1795 | `	/* "@<seconds>" absolute epoch */` |
|   145 | 1796 | `	if( z[0] == '@' ){` |
|    57 | 1797 | `		int neg = 0;` |
|    57 | 1798 | `		sxi64 v = 0;` |
|    57 | 1799 | `		const char *zAt = z;` |
|    57 | 1800 | `		z++;` |
|    57 | 1801 | `		if( z < zEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|     - | 1802 | `		/* php's lexer rejects the whole token: the error points at the '@' */` |
|    57 | 1803 | `		if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zAt - zIn) + 1; }` |
|   137 | 1804 | `		while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|    55 | 1805 | `		*pTs = neg ? -v : v;` |
|    55 | 1806 | `		*pOff = 0;` |
|    55 | 1807 | `		*pbOffSet = 1;` |
|    55 | 1808 | `		DT_SKIP_WS();` |
|    55 | 1809 | `		return (z < zEnd) ? (int)(z - zIn) + 1 : 0;` |
|     - | 1810 | `	}` |
|     - | 1811 | `	/* Absolute date: YYYY-MM-DD[...] */` |
|    88 | 1812 | `	if( zEnd-z >= 10 && SyisDigit(z[0]) && SyisDigit(z[1]) && SyisDigit(z[2])` |
|    69 | 1813 | `	 && SyisDigit(z[3]) && z[4]=='-' ){` |
|    69 | 1814 | `		sxi64 y = (z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0');` |
|    69 | 1815 | `		int mo,d,h=0,mi=0,s=0;` |
|    69 | 1816 | `		if( !SyisDigit(z[5])\|\|!SyisDigit(z[6])\|\|z[7] != '-'\|\|!SyisDigit(z[8])\|\|!SyisDigit(z[9]) ){` |
|   ! 0 | 1817 | `			return (int)(z - zIn) + 1;` |
|     - | 1818 | `		}` |
|    69 | 1819 | `		mo = (z[5]-'0')*10 + (z[6]-'0');` |
|    69 | 1820 | `		d  = (z[8]-'0')*10 + (z[9]-'0');` |
|     - | 1821 | `		/* php's lexer dies on the SECOND digit of an out-of-range month/day` |
|     - | 1822 | `		 * (either the two-digit pattern fails there, or a one-digit component` |
|     - | 1823 | `		 * matched and the separator check fails there); "00" lexes fine and` |
|     - | 1824 | `		 * normalizes (month 0 == December of the previous year). */` |
|    69 | 1825 | `		if( mo > 12 ){ return (int)(&z[6] - zIn) + 1; }` |
|    65 | 1826 | `		if( d > 31 ){ return (int)(&z[9] - zIn) + 1; }` |
|    63 | 1827 | `		if( mo == 0 ){ mo = 12; y--; }` |
|    63 | 1828 | `		z += 10;` |
|    62 | 1829 | `		if( z < zEnd && (z[0]=='T' \|\| z[0]==' ') && zEnd-z >= 6` |
|    35 | 1830 | `		 && SyisDigit(z[1]) && SyisDigit(z[2]) && z[3]==':' ){` |
|    35 | 1831 | `			z++;` |
|    35 | 1832 | `			h  = (z[0]-'0')*10 + (z[1]-'0');` |
|    35 | 1833 | `			mi = (z[3]-'0')*10 + (z[4]-'0');` |
|     - | 1834 | `			/* a 25+ hour kills php's whole time token: error at its start */` |
|    35 | 1835 | `			if( h > 24 ){ return (int)(z - zIn) + 1; }` |
|     - | 1836 | `			/* php lexes HH:M, then the minute's second digit starts a SECOND` |
|     - | 1837 | `			 * time token: "Double time specification" (negative encoding) */` |
|    33 | 1838 | `			if( mi > 59 ){ return -((int)(&z[4] - zIn) + 1); }` |
|    31 | 1839 | `			z += 5;` |
|    30 | 1840 | `			if( z+2 < zEnd+1 && z < zEnd && z[0]==':' && zEnd-z >= 3` |
|    31 | 1841 | `			 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|    31 | 1842 | `				s = (z[1]-'0')*10 + (z[2]-'0');` |
|    31 | 1843 | `				if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|    29 | 1844 | `				z += 3;` |
|    14 | 1845 | `			}` |
|    29 | 1846 | `			if( z < zEnd && z[0]=='.' ){ /* fractional seconds: consume */` |
|   ! 0 | 1847 | `				z++;` |
|   ! 0 | 1848 | `				while( z < zEnd && SyisDigit(z[0]) ){ z++; }` |
|   ! 0 | 1849 | `			}` |
|    29 | 1850 | `			if( z < zEnd && (z[0]=='Z' \|\| z[0]=='z') ){` |
|     - | 1851 | `				/* 2 = explicit "Z" zone: php names it "Z", not "+00:00" */` |
|     3 | 1852 | `				iOff = 0; bOffSet = 2; z++;` |
|    28 | 1853 | `			}else if( z < zEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|     3 | 1854 | `				int sign = (z[0]=='-') ? -1 : 1;` |
|     3 | 1855 | `				int oh,om = 0;` |
|     3 | 1856 | `				z++;` |
|     3 | 1857 | `				if( zEnd-z < 2 \|\| !SyisDigit(z[0]) \|\| !SyisDigit(z[1]) ){ return (int)(z - zIn) + 1; }` |
|     3 | 1858 | `				oh = (z[0]-'0')*10 + (z[1]-'0');` |
|     3 | 1859 | `				z += 2;` |
|     3 | 1860 | `				if( z < zEnd && z[0]==':' ){ z++; }` |
|     3 | 1861 | `				if( zEnd-z >= 2 && SyisDigit(z[0]) && SyisDigit(z[1]) ){` |
|     3 | 1862 | `					om = (z[0]-'0')*10 + (z[1]-'0');` |
|     3 | 1863 | `					z += 2;` |
|     1 | 1864 | `				}` |
|     3 | 1865 | `				iOff = sign * (oh*3600 + om*60);` |
|     3 | 1866 | `				bOffSet = 1;` |
|     1 | 1867 | `			}` |
|    14 | 1868 | `		}` |
|    57 | 1869 | `		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    57 | 1870 | `		bAny = 1;` |
|    49 | 1871 | `	}else if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|     7 | 1872 | `	 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|     - | 1873 | `		/* Time-only: HH:MM[:SS] on the base date */` |
|     7 | 1874 | `		sxi64 t = iTs + iOff;` |
|     7 | 1875 | `		sxi64 days = DtFloorDiv(t,86400);` |
|     7 | 1876 | `		int h  = (z[0]-'0')*10 + (z[1]-'0');` |
|     7 | 1877 | `		int mi = (z[3]-'0')*10 + (z[4]-'0');` |
|     7 | 1878 | `		int s = 0;` |
|     - | 1879 | `		/* php: bad hour kills the token (error at its start); bad minute /` |
|     - | 1880 | `		 * second dies on the component's second digit */` |
|     7 | 1881 | `		if( h > 24 ){ return (int)(z - zIn) + 1; }` |
|     5 | 1882 | `		if( mi > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|     3 | 1883 | `		z += 5;` |
|     3 | 1884 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     3 | 1885 | `			s = (z[1]-'0')*10 + (z[2]-'0');` |
|     3 | 1886 | `			if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|   ! 0 | 1887 | `			z += 3;` |
|   ! 0 | 1888 | `		}` |
|   ! 0 | 1889 | `		iTs = days*86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|   ! 0 | 1890 | `		bAny = 1;` |
|    15 | 1891 | `	}else if( DT_LOWEQ("now",3) ){` |
|   ! 0 | 1892 | `		z += 3;` |
|   ! 0 | 1893 | `		bAny = 1;` |
|   ! 0 | 1894 | `	}` |
|     - | 1895 | `	/* Relative / keyword sequence */` |
|    35 | 1896 | `	for(;;){` |
|    84 | 1897 | `		DT_SKIP_WS();` |
|    77 | 1898 | `		if( z >= zEnd ){` |
|    63 | 1899 | `			break;` |
|     - | 1900 | `		}` |
|    15 | 1901 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|   ! 0 | 1902 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|   ! 0 | 1903 | `			iTs = days*86400 - iOff;` |
|   ! 0 | 1904 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|   ! 0 | 1905 | `			bAny = 1;` |
|   ! 0 | 1906 | `			continue;` |
|     - | 1907 | `		}` |
|    15 | 1908 | `		if( DT_LOWEQ("noon",4) ){` |
|   ! 0 | 1909 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|   ! 0 | 1910 | `			iTs = days*86400 + 12*3600 - iOff;` |
|   ! 0 | 1911 | `			z += 4;` |
|   ! 0 | 1912 | `			bAny = 1;` |
|   ! 0 | 1913 | `			continue;` |
|     - | 1914 | `		}` |
|    15 | 1915 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|   ! 0 | 1916 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) + 1;` |
|   ! 0 | 1917 | `			iTs = days*86400 - iOff;` |
|   ! 0 | 1918 | `			z += 8;` |
|   ! 0 | 1919 | `			bAny = 1;` |
|   ! 0 | 1920 | `			continue;` |
|     - | 1921 | `		}` |
|    15 | 1922 | `		if( DT_LOWEQ("yesterday",9) ){` |
|   ! 0 | 1923 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) - 1;` |
|   ! 0 | 1924 | `			iTs = days*86400 - iOff;` |
|   ! 0 | 1925 | `			z += 9;` |
|   ! 0 | 1926 | `			bAny = 1;` |
|   ! 0 | 1927 | `			continue;` |
|     - | 1928 | `		}` |
|    15 | 1929 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|     7 | 1930 | `			int neg = 0;` |
|     7 | 1931 | `			sxi64 v = 0;` |
|     7 | 1932 | `			const char *zNumStart = z;` |
|     7 | 1933 | `			if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; }` |
|     7 | 1934 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zNumStart - zIn) + 1; }` |
|    13 | 1935 | `			while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|     7 | 1936 | `			if( neg ){ v = -v; }` |
|    16 | 1937 | `			DT_SKIP_WS();` |
|     7 | 1938 | `			if( DT_LOWEQ("seconds",7) )     { iTs += v;            z += 7; }` |
|     7 | 1939 | `			else if( DT_LOWEQ("second",6) ) { iTs += v;            z += 6; }` |
|     7 | 1940 | `			else if( DT_LOWEQ("secs",4) )   { iTs += v;            z += 4; }` |
|     7 | 1941 | `			else if( DT_LOWEQ("sec",3) )    { iTs += v;            z += 3; }` |
|     7 | 1942 | `			else if( DT_LOWEQ("minutes",7) ){ iTs += v*60;         z += 7; }` |
|     7 | 1943 | `			else if( DT_LOWEQ("minute",6) ) { iTs += v*60;         z += 6; }` |
|     7 | 1944 | `			else if( DT_LOWEQ("mins",4) )   { iTs += v*60;         z += 4; }` |
|     7 | 1945 | `			else if( DT_LOWEQ("min",3) )    { iTs += v*60;         z += 3; }` |
|     7 | 1946 | `			else if( DT_LOWEQ("hours",5) )  { iTs += v*3600;       z += 5; }` |
|     7 | 1947 | `			else if( DT_LOWEQ("hour",4) )   { iTs += v*3600;       z += 4; }` |
|     7 | 1948 | `			else if( DT_LOWEQ("days",4) )   { iTs += v*86400;      z += 4; }` |
|     7 | 1949 | `			else if( DT_LOWEQ("day",3) )    { iTs += v*86400;      z += 3; }` |
|     5 | 1950 | `			else if( DT_LOWEQ("weeks",5) )  { iTs += v*7*86400;    z += 5; }` |
|     5 | 1951 | `			else if( DT_LOWEQ("week",4) )   { iTs += v*7*86400;    z += 4; }` |
|     5 | 1952 | `			else if( DT_LOWEQ("fortnights",10) ){ iTs += v*14*86400; z += 10; }` |
|     5 | 1953 | `			else if( DT_LOWEQ("fortnight",9) )  { iTs += v*14*86400; z += 9; }` |
|     5 | 1954 | `			else if( DT_LOWEQ("months",6) ) { iTs = DtAddMonths(iTs,iOff,v); z += 6; }` |
|     3 | 1955 | `			else if( DT_LOWEQ("month",5) )  { iTs = DtAddMonths(iTs,iOff,v); z += 5; }` |
|   ! 0 | 1956 | `			else if( DT_LOWEQ("years",5) )  { iTs = DtAddMonths(iTs,iOff,v*12); z += 5; }` |
|   ! 0 | 1957 | `			else if( DT_LOWEQ("year",4) )   { iTs = DtAddMonths(iTs,iOff,v*12); z += 4; }` |
|     - | 1958 | `			else{` |
|   ! 0 | 1959 | `				return (int)(z - zIn) + 1;` |
|     - | 1960 | `			}` |
|     7 | 1961 | `			bAny = 1;` |
|     7 | 1962 | `			continue;` |
|     - | 1963 | `		}` |
|     9 | 1964 | `		return (int)(z - zIn) + 1;` |
|   ! 0 | 1965 | `	}` |
|    63 | 1966 | `	if( !bAny ){` |
|   ! 0 | 1967 | `		return 1;` |
|     - | 1968 | `	}` |
|    63 | 1969 | `	*pTs = iTs;` |
|    63 | 1970 | `	*pOff = iOff;` |
|    63 | 1971 | `	*pbOffSet = bOffSet;` |
|    63 | 1972 | `	return 0;` |
|     - | 1973 | `#undef DT_SKIP_WS` |
|     - | 1974 | `#undef DT_LOWEQ` |
|    73 | 1975 | `}` |
|     - | 1976 | `/* int __dt_now() */` |
|   180 | 1977 | `static int vm_builtin_dt_now(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1978 | `{` |
|    90 | 1979 | `	SXUNUSED(nArg);` |
|    90 | 1980 | `	SXUNUSED(apArg);` |
|   181 | 1981 | `	ph7_result_int64(pCtx,(ph7_int64)time(0));` |
|   181 | 1982 | `	return PH7_OK;` |
|     1 | 1983 | `}` |
|     - | 1984 | `/* mixed __dt_parse(string $s, int $baseTs, int $baseOff)` |
|     - | 1985 | ` *   -> [ts, off, offWasExplicit] on success; php's error MESSAGE string on` |
|     - | 1986 | ` *      failure (the chunk wraps it in DateMalformedStringException). */` |
|   144 | 1987 | `static int vm_builtin_dt_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1988 | `{` |
|     - | 1989 | `	const char *zIn;` |
|     - | 1990 | `	int nLen;` |
|     - | 1991 | `	sxi64 iBaseTs;` |
|     - | 1992 | `	sxi32 iBaseOff;` |
|   145 | 1993 | `	sxi64 iTs = 0;` |
|   145 | 1994 | `	sxi32 iOff = 0;` |
|   145 | 1995 | `	int bOffSet = 0;` |
|     - | 1996 | `	int iErrPos;` |
|   145 | 1997 | `	if( nArg < 3 ){` |
|   ! 0 | 1998 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1999 | `		return PH7_OK;` |
|     - | 2000 | `	}` |
|   145 | 2001 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   145 | 2002 | `	iBaseTs  = ph7_value_to_int64(apArg[1]);` |
|   145 | 2003 | `	iBaseOff = (sxi32)ph7_value_to_int64(apArg[2]);` |
|   145 | 2004 | `	iErrPos = DtParse(zIn,nLen,iBaseTs,iBaseOff,&iTs,&iOff,&bOffSet);` |
|   145 | 2005 | `	if( iErrPos != 0 ){` |
|     - | 2006 | `		/* Negative encoding: php's "Double time specification" reason */` |
|    29 | 2007 | `		int bDouble = iErrPos < 0;` |
|    29 | 2008 | `		int iPos = (bDouble ? -iErrPos : iErrPos) - 1;` |
|    29 | 2009 | `		char cAt = (iPos < nLen) ? zIn[iPos] : ' ';` |
|     - | 2010 | `		/* php appends a reason: an alphabetic token is assumed to be a timezone` |
|     - | 2011 | `		 * lookup miss, anything else an unexpected character. */` |
|    56 | 2012 | `		ph7_result_string_format(pCtx,` |
|     - | 2013 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    14 | 2014 | `			nLen,zIn,iPos,cAt,` |
|    27 | 2015 | `			bDouble ? "Double time specification"` |
|    26 | 2016 | `			: ((cAt >= 'a' && cAt <= 'z') \|\| (cAt >= 'A' && cAt <= 'Z'))` |
|     - | 2017 | `				? "The timezone could not be found in the database"` |
|    26 | 2018 | `				: "Unexpected character");` |
|    29 | 2019 | `		return PH7_OK;` |
|     - | 2020 | `	}` |
|     - | 2021 | `	{` |
|   117 | 2022 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|   117 | 2023 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|   117 | 2024 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2025 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2026 | `		}` |
|   117 | 2027 | `		ph7_value_int64(pV,iTs);` |
|   117 | 2028 | `		ph7_array_add_elem(pArr,0,pV);` |
|   117 | 2029 | `		ph7_value_int64(pV,iOff);` |
|   117 | 2030 | `		ph7_array_add_elem(pArr,0,pV);` |
|     - | 2031 | `		/* int, not bool: 0 = no explicit offset, 1 = numeric offset/@epoch,` |
|     - | 2032 | `		 * 2 = literal "Z" (php keeps the distinction in the zone name) */` |
|   117 | 2033 | `		ph7_value_int64(pV,bOffSet);` |
|   117 | 2034 | `		ph7_array_add_elem(pArr,0,pV);` |
|   117 | 2035 | `		ph7_result_value(pCtx,pArr);` |
|     - | 2036 | `	}` |
|   117 | 2037 | `	return PH7_OK;` |
|    73 | 2038 | `}` |
|     - | 2039 | `/* string __dt_default_tz(void) — the date_default_timezone_set() identifier */` |
|   180 | 2040 | `static int vm_builtin_dt_default_tz(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2041 | `{` |
|    90 | 2042 | `	SXUNUSED(nArg);` |
|    90 | 2043 | `	SXUNUSED(apArg);` |
|   181 | 2044 | `	ph7_result_string(pCtx,pCtx->pVm->zDefTz,(int)pCtx->pVm->nDefTz);` |
|   181 | 2045 | `	return PH7_OK;` |
|     1 | 2046 | `}` |
|     - | 2047 | `/* string __dt_format(int $ts, int $off, string $tzname, string $format) */` |
|   130 | 2048 | `static int vm_builtin_dt_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2049 | `{` |
|     - | 2050 | `	Sytm sTm;` |
|     - | 2051 | `	sxi64 iTs;` |
|     - | 2052 | `	sxi32 iOff;` |
|     - | 2053 | `	const char *zName,*zFmt;` |
|     - | 2054 | `	int nName,nFmt;` |
|     - | 2055 | `	char zZone[64];` |
|   131 | 2056 | `	if( nArg < 4 ){` |
|   ! 0 | 2057 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2058 | `		return PH7_OK;` |
|     - | 2059 | `	}` |
|   131 | 2060 | `	iTs  = ph7_value_to_int64(apArg[0]);` |
|   131 | 2061 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|   131 | 2062 | `	zName = ph7_value_to_string(apArg[2],&nName);` |
|   131 | 2063 | `	zFmt  = ph7_value_to_string(apArg[3],&nFmt);` |
|   131 | 2064 | `	if( nName >= (int)sizeof(zZone) ){ nName = (int)sizeof(zZone) - 1; }` |
|   131 | 2065 | `	SyMemcpy(zName,zZone,(sxu32)nName);` |
|   131 | 2066 | `	zZone[nName] = 0;` |
|   131 | 2067 | `	DtFillSytm(iTs,iOff,zZone,&sTm);` |
|   131 | 2068 | `	DateFormat(pCtx,zFmt,nFmt,&sTm);` |
|   131 | 2069 | `	return PH7_OK;` |
|    66 | 2070 | `}` |
|     - | 2071 | `/* int __dt_make(int y, int mo, int d, int h, int i, int s, int off) */` |
|     4 | 2072 | `static int vm_builtin_dt_make(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2073 | `{` |
|     - | 2074 | `	sxi64 y;` |
|     - | 2075 | `	int mo,d,h,mi,s;` |
|     - | 2076 | `	sxi32 iOff;` |
|     5 | 2077 | `	if( nArg < 7 ){` |
|   ! 0 | 2078 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2079 | `		return PH7_OK;` |
|     - | 2080 | `	}` |
|     5 | 2081 | `	y   = ph7_value_to_int64(apArg[0]);` |
|     5 | 2082 | `	mo  = ph7_value_to_int(apArg[1]);` |
|     5 | 2083 | `	d   = ph7_value_to_int(apArg[2]);` |
|     5 | 2084 | `	h   = ph7_value_to_int(apArg[3]);` |
|     5 | 2085 | `	mi  = ph7_value_to_int(apArg[4]);` |
|     5 | 2086 | `	s   = ph7_value_to_int(apArg[5]);` |
|     5 | 2087 | `	iOff = (sxi32)ph7_value_to_int64(apArg[6]);` |
|     5 | 2088 | `	ph7_result_int64(pCtx,DtMakeTs(y,mo,d,h,mi,s,iOff));` |
|     5 | 2089 | `	return PH7_OK;` |
|     3 | 2090 | `}` |
|     - | 2091 | `/* Days in a civil month (php's overflow rules use it during diff borrows) */` |
|    50 | 2092 | `static int DtDaysInMonth(sxi64 y,int m)` |
|     1 | 2093 | `{` |
|     - | 2094 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|    51 | 2095 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|     9 | 2096 | `		return 29;` |
|     - | 2097 | `	}` |
|    43 | 2098 | `	return aMonDays[(m - 1) % 12];` |
|    26 | 2099 | `}` |
|     - | 2100 | `/* int __dt_civil_add(int ts, int off, int y, int m, int d, int h, int i,` |
|     - | 2101 | ` *                    int s, int sign)` |
|     - | 2102 | ` *   php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|     - | 2103 | ` *   (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset. */` |
|    54 | 2104 | `static int vm_builtin_dt_civil_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2105 | `{` |
|     - | 2106 | `	sxi64 iTs,iLocal,iDays,iSecs,y0,moT,dayCount;` |
|     - | 2107 | `	sxi32 iOff;` |
|     - | 2108 | `	int mo0,d0,iSign;` |
|     - | 2109 | `	sxi64 y,m,d,h,i,s;` |
|    55 | 2110 | `	if( nArg < 9 ){` |
|   ! 0 | 2111 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2112 | `		return PH7_OK;` |
|     - | 2113 | `	}` |
|    55 | 2114 | `	iTs   = ph7_value_to_int64(apArg[0]);` |
|    55 | 2115 | `	iOff  = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    55 | 2116 | `	y     = ph7_value_to_int64(apArg[2]);` |
|    55 | 2117 | `	m     = ph7_value_to_int64(apArg[3]);` |
|    55 | 2118 | `	d     = ph7_value_to_int64(apArg[4]);` |
|    55 | 2119 | `	h     = ph7_value_to_int64(apArg[5]);` |
|    55 | 2120 | `	i     = ph7_value_to_int64(apArg[6]);` |
|    55 | 2121 | `	s     = ph7_value_to_int64(apArg[7]);` |
|    55 | 2122 | `	iSign = ph7_value_to_int(apArg[8]) < 0 ? -1 : 1;` |
|    55 | 2123 | `	iLocal = iTs + iOff;` |
|    55 | 2124 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    55 | 2125 | `	iSecs  = iLocal - iDays*86400;` |
|    55 | 2126 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    55 | 2127 | `	y0 += iSign * y;` |
|    55 | 2128 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    55 | 2129 | `	y0 += DtFloorDiv(moT,12);` |
|    55 | 2130 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    55 | 2131 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    55 | 2132 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    55 | 2133 | `	ph7_result_int64(pCtx,iLocal - iOff);` |
|    55 | 2134 | `	return PH7_OK;` |
|    28 | 2135 | `}` |
|     - | 2136 | `/* array __dt_civil_diff(int ts1, int off1, int ts2)` |
|     - | 2137 | ` *   -> [y,m,d,h,i,s,days,invert]: timelib's breakdown — field-wise deltas in` |
|     - | 2138 | ` *   the FIRST operand's offset, then borrow seconds→minutes→hours→days, then` |
|     - | 2139 | ` *   the day borrow walks whole months backward from the later date (that walk` |
|     - | 2140 | ` *   is why Jan 31 → Mar 02 reports m=0 d=30, not "1 month"). */` |
|    14 | 2141 | `static int vm_builtin_dt_civil_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2142 | `{` |
|     - | 2143 | `	sxi64 iTs1,iTs2,iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|     - | 2144 | `	sxi32 iOff;` |
|     - | 2145 | `	int moA,dA,moB,dB,bInvert;` |
|     - | 2146 | `	sxi64 sA,sB,y,m,d,h,i,s;` |
|     - | 2147 | `	ph7_value *pArr,*pV;` |
|    15 | 2148 | `	if( nArg < 3 ){` |
|   ! 0 | 2149 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2150 | `		return PH7_OK;` |
|     - | 2151 | `	}` |
|    15 | 2152 | `	iTs1 = ph7_value_to_int64(apArg[0]);` |
|    15 | 2153 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    15 | 2154 | `	iTs2 = ph7_value_to_int64(apArg[2]);` |
|    15 | 2155 | `	bInvert = iTs1 > iTs2;` |
|    15 | 2156 | `	iA = bInvert ? iTs2 : iTs1;` |
|    15 | 2157 | `	iB = bInvert ? iTs1 : iTs2;` |
|    15 | 2158 | `	iLa = iA + iOff;` |
|    15 | 2159 | `	iLb = iB + iOff;` |
|    15 | 2160 | `	daysA = DtFloorDiv(iLa,86400);` |
|    15 | 2161 | `	daysB = DtFloorDiv(iLb,86400);` |
|    15 | 2162 | `	sA = iLa - daysA*86400;` |
|    15 | 2163 | `	sB = iLb - daysB*86400;` |
|    15 | 2164 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|    15 | 2165 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|    15 | 2166 | `	s = (sB % 60) - (sA % 60);` |
|    15 | 2167 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|    15 | 2168 | `	h = (sB / 3600) - (sA / 3600);` |
|    15 | 2169 | `	d = dB - dA;` |
|    15 | 2170 | `	m = moB - moA;` |
|    15 | 2171 | `	y = yB - yA;` |
|    15 | 2172 | `	if( s < 0 ){ s += 60; i--; }` |
|    15 | 2173 | `	if( i < 0 ){ i += 60; h--; }` |
|    15 | 2174 | `	if( h < 0 ){ h += 24; d--; }` |
|    27 | 2175 | `	while( d < 0 ){` |
|    13 | 2176 | `		moB--;` |
|    13 | 2177 | `		if( moB < 1 ){ moB = 12; yB--; }` |
|    13 | 2178 | `		d += DtDaysInMonth(yB,moB);` |
|    13 | 2179 | `		m--;` |
|     1 | 2180 | `	}` |
|    15 | 2181 | `	if( m < 0 ){ m += 12; y--; }` |
|    15 | 2182 | `	pArr = ph7_context_new_array(pCtx);` |
|    15 | 2183 | `	pV = ph7_context_new_scalar(pCtx);` |
|    15 | 2184 | `	if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2185 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2186 | `	}` |
|    15 | 2187 | `	ph7_value_int64(pV,y);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2188 | `	ph7_value_int64(pV,m);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2189 | `	ph7_value_int64(pV,d);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2190 | `	ph7_value_int64(pV,h);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2191 | `	ph7_value_int64(pV,i);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2192 | `	ph7_value_int64(pV,s);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2193 | `	ph7_value_int64(pV,(iB - iA) / 86400); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2194 | `	ph7_value_int64(pV,bInvert); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2195 | `	ph7_result_value(pCtx,pArr);` |
|    15 | 2196 | `	return PH7_OK;` |
|     8 | 2197 | `}` |
|     - | 2198 | `/* int __dt_isodate(int ts, int off, int y, int w, int dow)` |
|     - | 2199 | ` *   setISODate: jump to ISO year/week/weekday, preserving the time of day. */` |
|     8 | 2200 | `static int vm_builtin_dt_isodate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2201 | `{` |
|     - | 2202 | `	sxi64 iTs,iLocal,iTod,jan4,monday1,target,y;` |
|     - | 2203 | `	sxi32 iOff;` |
|     - | 2204 | `	sxi64 w,dow;` |
|     - | 2205 | `	int isoDow;` |
|     9 | 2206 | `	if( nArg < 5 ){` |
|   ! 0 | 2207 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2208 | `		return PH7_OK;` |
|     - | 2209 | `	}` |
|     9 | 2210 | `	iTs = ph7_value_to_int64(apArg[0]);` |
|     9 | 2211 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|     9 | 2212 | `	y   = ph7_value_to_int64(apArg[2]);` |
|     9 | 2213 | `	w   = ph7_value_to_int64(apArg[3]);` |
|     9 | 2214 | `	dow = ph7_value_to_int64(apArg[4]);` |
|     9 | 2215 | `	iLocal = iTs + iOff;` |
|     9 | 2216 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|     9 | 2217 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|     9 | 2218 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|     9 | 2219 | `	monday1 = jan4 - (isoDow - 1);` |
|     9 | 2220 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|     9 | 2221 | `	ph7_result_int64(pCtx,target*86400 + iTod - iOff);` |
|     9 | 2222 | `	return PH7_OK;` |
|     5 | 2223 | `}` |
|     - | 2224 | `/* Consume nMin..nMax digits from *pz; returns count consumed (0 = failure) */` |
|   130 | 2225 | `static int DtEatDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2226 | `{` |
|   131 | 2227 | `	const char *z = *pz;` |
|   131 | 2228 | `	sxi64 v = 0;` |
|   131 | 2229 | `	int n = 0;` |
|   473 | 2230 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|   343 | 2231 | `		v = v*10 + (z[0] - '0');` |
|   343 | 2232 | `		z++;` |
|   343 | 2233 | `		n++;` |
|     1 | 2234 | `	}` |
|   131 | 2235 | `	if( n < nMin ){` |
|     3 | 2236 | `		return 0;` |
|     - | 2237 | `	}` |
|   129 | 2238 | `	*pz = z;` |
|   129 | 2239 | `	*pVal = v;` |
|   129 | 2240 | `	return n;` |
|    66 | 2241 | `}` |
|     - | 2242 | `/* timelib_get_nr's recovery: skip non-digits hunting for the field.` |
|     - | 2243 | ` * Returns 1 = found+read, 0 = digits present but short, -1 = exhausted. */` |
|     2 | 2244 | `static int DtHuntDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2245 | `{` |
|     3 | 2246 | `	const char *z = *pz;` |
|    13 | 2247 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|     3 | 2248 | `	*pz = z;` |
|     3 | 2249 | `	if( z >= zEnd ){` |
|     3 | 2250 | `		return -1;` |
|     - | 2251 | `	}` |
|   ! 0 | 2252 | `	return DtEatDigits(pz,zEnd,nMin,nMax,pVal) ? 1 : 0;` |
|     2 | 2253 | `}` |
|     - | 2254 | `/* Case-insensitive name-table lookup; returns 1-based index or 0 */` |
|    14 | 2255 | `static int DtEatName(const char **pz,const char *zEnd,const char **azNames,int nNames)` |
|     1 | 2256 | `{` |
|     - | 2257 | `	int k;` |
|    23 | 2258 | `	for( k = 0 ; k < nNames ; k++ ){` |
|    23 | 2259 | `		int n = (int)SyStrlen(azNames[k]);` |
|    23 | 2260 | `		if( zEnd - *pz >= n && SyStrnicmp(*pz,azNames[k],(sxu32)n) == 0 ){` |
|    15 | 2261 | `			*pz += n;` |
|    15 | 2262 | `			return k + 1;` |
|     - | 2263 | `		}` |
|     5 | 2264 | `	}` |
|   ! 0 | 2265 | `	return 0;` |
|     8 | 2266 | `}` |
|     - | 2267 | `/* mixed __dt_from_format(string fmt, string input, int nowTs, int defOff)` |
|     - | 2268 | ` *   php's DateTime::createFromFormat engine. Success: [ts, off, offKind, name]` |
|     - | 2269 | ` *   where offKind 0=none-parsed, 1=numeric offset, 2=literal Z, 3=named id.` |
|     - | 2270 | ` *   Failure: "POS\tMESSAGE" (timelib's message strings; PHL reports the FIRST` |
|     - | 2271 | ` *   error where php may accumulate several — recorded). A trailing-data` |
|     - | 2272 | ` *   warning rides as [4]=pos, [5]=msg on the success array. */` |
|    44 | 2273 | `static int vm_builtin_dt_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2274 | `{` |
|     - | 2275 | `	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};` |
|     - | 2276 | `	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",` |
|     - | 2277 | `		"thursday","friday","saturday"};` |
|     - | 2278 | `	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",` |
|     - | 2279 | `		"aug","sep","oct","nov","dec"};` |
|     - | 2280 | `	static const char *azMonFull[] = {"january","february","march","april",` |
|     - | 2281 | `		"may","june","july","august","september","october","november","december"};` |
|     - | 2282 | `	const char *zFmt,*zIn,*zEnd,*zInEnd,*z;` |
|     - | 2283 | `	int nFmt,nIn;` |
|     - | 2284 | `	sxi64 iNow,v;` |
|     - | 2285 | `	sxi32 iDefOff;` |
|     - | 2286 | `	/* -1 == unset */` |
|    45 | 2287 | `	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;` |
|    45 | 2288 | `	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;` |
|    45 | 2289 | `	int iOffKind = 0;` |
|    45 | 2290 | `	sxi32 iOffVal = 0;` |
|     - | 2291 | `	char zName[16];` |
|    45 | 2292 | `	const char *zErr = 0;` |
|     - | 2293 | `	const char *aWarnMsg[3];` |
|     - | 2294 | `	int aWarnPos[3];` |
|    45 | 2295 | `	int nWarn = 0,bAborted = 0;` |
|     - | 2296 | `	const char *aErrMsg[8];` |
|     - | 2297 | `	int aErrPos[8];` |
|    45 | 2298 | `	int nErr = 0,nErrKept = 0;` |
|    45 | 2299 | `	if( nArg < 4 ){` |
|   ! 0 | 2300 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2301 | `		return PH7_OK;` |
|     - | 2302 | `	}` |
|    45 | 2303 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    45 | 2304 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    45 | 2305 | `	iNow = ph7_value_to_int64(apArg[2]);` |
|    45 | 2306 | `	iDefOff = (sxi32)ph7_value_to_int64(apArg[3]);` |
|    45 | 2307 | `	zEnd = &zFmt[nFmt];` |
|    45 | 2308 | `	zInEnd = &zIn[nIn];` |
|    45 | 2309 | `	z = zIn;` |
|    45 | 2310 | `	zName[0] = 0;` |
|     - | 2311 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|     - | 2312 | `	{ int _p = (iPos),_k,_f = -1; \` |
|     - | 2313 | `	  nErr++; \` |
|     - | 2314 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|     - | 2315 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|     - | 2316 | `	  else if( nErrKept < 8 ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|   299 | 2317 | `	while( zFmt < zEnd ){` |
|   257 | 2318 | `		char c = zFmt[0];` |
|   257 | 2319 | `		zFmt++;` |
|   257 | 2320 | `		zErr = 0;` |
|   257 | 2321 | `		if( c == '!' ){` |
|     7 | 2322 | `			y = 1970; mo = 1; d = 1; h = 0; mi = 0; s = 0;` |
|     7 | 2323 | `			h12 = -1; iMeridiem = -1;` |
|     7 | 2324 | `			continue;` |
|     - | 2325 | `		}` |
|   251 | 2326 | `		if( c == '\|' ){ bPipe = 1; continue; }` |
|   247 | 2327 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|   245 | 2328 | `		if( z >= zInEnd ){` |
|     - | 2329 | `			/* timelib aborts the scan once input is exhausted */` |
|     5 | 2330 | `			DT_FF_LOGERR(nIn,"Not enough data available to satisfy format");` |
|     3 | 2331 | `			break;` |
|     - | 2332 | `		}` |
|   243 | 2333 | `		switch( c ){` |
|    15 | 2334 | `		case 'd': case 'j':` |
|    31 | 2335 | `			if( !DtEatDigits(&z,zInEnd,1,2,&d) ){` |
|   ! 0 | 2336 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit day could not be found");` |
|   ! 0 | 2337 | `				if( DtHuntDigits(&z,zInEnd,1,2,&d) < 0 ){` |
|   ! 0 | 2338 | `					DT_FF_LOGERR(nIn,"A two digit day could not be found");` |
|   ! 0 | 2339 | `				}` |
|   ! 0 | 2340 | `			}` |
|    31 | 2341 | `			break;` |
|     1 | 2342 | `		case 'D':` |
|     3 | 2343 | `			if( !DtEatName(&z,zInEnd,azDay3,7) ){` |
|   ! 0 | 2344 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2345 | `			}` |
|     3 | 2346 | `			break;` |
|     1 | 2347 | `		case 'l':` |
|     3 | 2348 | `			if( !DtEatName(&z,zInEnd,azDayFull,7) ){` |
|   ! 0 | 2349 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2350 | `			}` |
|     3 | 2351 | `			break;` |
|     1 | 2352 | `		case 'S':` |
|     - | 2353 | `			/* ordinal suffix: st nd rd th */` |
|     4 | 2354 | `			if( zInEnd-z >= 2 && ((z[0]=='s'&&z[1]=='t')\|\|(z[0]=='n'&&z[1]=='d')` |
|     2 | 2355 | `			 \|\|(z[0]=='r'&&z[1]=='d')\|\|(z[0]=='t'&&z[1]=='h')) ){` |
|     3 | 2356 | `				z += 2;` |
|     1 | 2357 | `			}` |
|     3 | 2358 | `			break;` |
|    13 | 2359 | `		case 'm': case 'n':` |
|    27 | 2360 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mo) ){` |
|   ! 0 | 2361 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit month could not be found");` |
|   ! 0 | 2362 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mo) < 0 ){` |
|   ! 0 | 2363 | `					DT_FF_LOGERR(nIn,"A two digit month could not be found");` |
|   ! 0 | 2364 | `				}` |
|   ! 0 | 2365 | `			}` |
|    27 | 2366 | `			break;` |
|     1 | 2367 | `		case 'M':{` |
|     3 | 2368 | `			int k = DtEatName(&z,zInEnd,azMon3,12);` |
|     3 | 2369 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2370 | `			break;` |
|     - | 2371 | `				 }` |
|     1 | 2372 | `		case 'F':{` |
|     3 | 2373 | `			int k = DtEatName(&z,zInEnd,azMonFull,12);` |
|     3 | 2374 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2375 | `			break;` |
|     - | 2376 | `				 }` |
|   ! 0 | 2377 | `		case 'y':` |
|   ! 0 | 2378 | `			if( DtEatDigits(&z,zInEnd,2,2,&y) ){` |
|   ! 0 | 2379 | `				y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2380 | `			}else{` |
|   ! 0 | 2381 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit year could not be found");` |
|   ! 0 | 2382 | `				if( DtHuntDigits(&z,zInEnd,2,2,&y) < 0 ){` |
|   ! 0 | 2383 | `					DT_FF_LOGERR(nIn,"A two digit year could not be found");` |
|   ! 0 | 2384 | `				}else if( y >= 0 ){` |
|   ! 0 | 2385 | `					y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2386 | `				}` |
|     - | 2387 | `			}` |
|   ! 0 | 2388 | `			break;` |
|    17 | 2389 | `		case 'Y':{` |
|    35 | 2390 | `			int neg = 0;` |
|    35 | 2391 | `			if( z < zInEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|    35 | 2392 | `			if( DtEatDigits(&z,zInEnd,1,4,&y) ){` |
|    33 | 2393 | `				if( neg ){ y = -y; }` |
|    17 | 2394 | `			}else{` |
|     3 | 2395 | `				DT_FF_LOGERR((int)(z - zIn),"A four digit year could not be found");` |
|     3 | 2396 | `				if( DtHuntDigits(&z,zInEnd,1,4,&y) < 0 ){` |
|     5 | 2397 | `					DT_FF_LOGERR(nIn,"A four digit year could not be found");` |
|     1 | 2398 | `				}` |
|     - | 2399 | `			}` |
|    35 | 2400 | `			break;` |
|     - | 2401 | `				 }` |
|     4 | 2402 | `		case 'H': case 'G':` |
|     9 | 2403 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h) ){` |
|   ! 0 | 2404 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2405 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h) < 0 ){` |
|   ! 0 | 2406 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2407 | `				}` |
|   ! 0 | 2408 | `			}` |
|     9 | 2409 | `			break;` |
|     2 | 2410 | `		case 'h': case 'g':` |
|     5 | 2411 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h12) ){` |
|   ! 0 | 2412 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2413 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h12) < 0 ){` |
|   ! 0 | 2414 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2415 | `				}` |
|   ! 0 | 2416 | `			}` |
|     5 | 2417 | `			break;` |
|     6 | 2418 | `		case 'i':` |
|    13 | 2419 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mi) ){` |
|   ! 0 | 2420 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit minute could not be found");` |
|   ! 0 | 2421 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mi) < 0 ){` |
|   ! 0 | 2422 | `					DT_FF_LOGERR(nIn,"A two digit minute could not be found");` |
|   ! 0 | 2423 | `				}` |
|   ! 0 | 2424 | `			}` |
|    13 | 2425 | `			break;` |
|     2 | 2426 | `		case 's':` |
|     5 | 2427 | `			if( !DtEatDigits(&z,zInEnd,1,2,&s) ){` |
|   ! 0 | 2428 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit second could not be found");` |
|   ! 0 | 2429 | `				if( DtHuntDigits(&z,zInEnd,1,2,&s) < 0 ){` |
|   ! 0 | 2430 | `					DT_FF_LOGERR(nIn,"A two digit second could not be found");` |
|   ! 0 | 2431 | `				}` |
|   ! 0 | 2432 | `			}` |
|     5 | 2433 | `			break;` |
|   ! 0 | 2434 | `		case 'u':` |
|     - | 2435 | `			/* micro parsed then dropped: PHL keeps whole seconds (recorded) */` |
|   ! 0 | 2436 | `			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){` |
|   ! 0 | 2437 | `				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");` |
|   ! 0 | 2438 | `				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){` |
|   ! 0 | 2439 | `					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");` |
|   ! 0 | 2440 | `				}` |
|   ! 0 | 2441 | `			}` |
|   ! 0 | 2442 | `			break;` |
|   ! 0 | 2443 | `		case 'v':` |
|   ! 0 | 2444 | `			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){` |
|   ! 0 | 2445 | `				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");` |
|   ! 0 | 2446 | `				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){` |
|   ! 0 | 2447 | `					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");` |
|   ! 0 | 2448 | `				}` |
|   ! 0 | 2449 | `			}` |
|   ! 0 | 2450 | `			break;` |
|     2 | 2451 | `		case 'a': case 'A':{` |
|     - | 2452 | `			static const char *azMer[] = {"am","pm","a.m.","p.m."};` |
|     5 | 2453 | `			int k = DtEatName(&z,zInEnd,azMer,4);` |
|     5 | 2454 | `			if( k ){` |
|     5 | 2455 | `				iMeridiem = ((k - 1) & 1);` |
|     3 | 2456 | `			}else{` |
|   ! 0 | 2457 | `				zErr = "A meridian could not be found";` |
|     - | 2458 | `			}` |
|     5 | 2459 | `			break;` |
|     - | 2460 | `				 }` |
|     2 | 2461 | `		case 'U':{` |
|     5 | 2462 | `			int neg = 0;` |
|     5 | 2463 | `			if( z < zInEnd && z[0]=='-' ){ neg = 1; z++; }` |
|     5 | 2464 | `			if( DtEatDigits(&z,zInEnd,1,19,&uVal) ){` |
|     5 | 2465 | `				if( neg ){ uVal = -uVal; }` |
|     5 | 2466 | `				bHasU = 1;` |
|     3 | 2467 | `			}else{` |
|   ! 0 | 2468 | `				DT_FF_LOGERR((int)(z - zIn),"A unix timestamp could not be found");` |
|   ! 0 | 2469 | `				if( DtHuntDigits(&z,zInEnd,1,19,&uVal) < 0 ){` |
|   ! 0 | 2470 | `					DT_FF_LOGERR(nIn,"A unix timestamp could not be found");` |
|   ! 0 | 2471 | `				}else{` |
|   ! 0 | 2472 | `					if( neg ){ uVal = -uVal; }` |
|   ! 0 | 2473 | `					bHasU = 1;` |
|     - | 2474 | `				}` |
|     - | 2475 | `			}` |
|     5 | 2476 | `			break;` |
|     - | 2477 | `				 }` |
|     1 | 2478 | `		case 'e': case 'T':{` |
|     - | 2479 | `			static const char *azZone[] = {"UTC","GMT","Z"};` |
|     3 | 2480 | `			int k = DtEatName(&z,zInEnd,azZone,3);` |
|     3 | 2481 | `			if( k == 3 ){` |
|   ! 0 | 2482 | `				iOffKind = 2; iOffVal = 0;` |
|     3 | 2483 | `			}else if( k ){` |
|     3 | 2484 | `				iOffKind = 3; iOffVal = 0;` |
|     3 | 2485 | `				SyMemcpy(azZone[k-1],zName,4);` |
|     1 | 2486 | `			}else if( z < zInEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|   ! 0 | 2487 | `				goto parse_num_off;` |
|   ! 0 | 2488 | `			}else{` |
|   ! 0 | 2489 | `				zErr = "The timezone could not be found in the database";` |
|     - | 2490 | `			}` |
|     3 | 2491 | `			break;` |
|     2 | 2492 | `				 }` |
|     - | 2493 | `		case 'O': case 'P':` |
|     2 | 2494 | `parse_num_off:	{` |
|     5 | 2495 | `			int sign,oh,om = 0;` |
|     - | 2496 | `			sxi64 t;` |
|     5 | 2497 | `			if( z >= zInEnd \|\| (z[0] != '+' && z[0] != '-') ){` |
|   ! 0 | 2498 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2499 | `				break;` |
|     - | 2500 | `			}` |
|     5 | 2501 | `			sign = (z[0]=='-') ? -1 : 1;` |
|     5 | 2502 | `			z++;` |
|     5 | 2503 | `			if( !DtEatDigits(&z,zInEnd,2,2,&t) ){` |
|   ! 0 | 2504 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2505 | `				break;` |
|     - | 2506 | `			}` |
|     5 | 2507 | `			oh = (int)t;` |
|     5 | 2508 | `			if( z < zInEnd && z[0]==':' ){ z++; }` |
|     5 | 2509 | `			if( DtEatDigits(&z,zInEnd,2,2,&t) ){ om = (int)t; }` |
|     5 | 2510 | `			iOffKind = 1;` |
|     5 | 2511 | `			iOffVal = sign * (oh*3600 + om*60);` |
|     5 | 2512 | `			break;` |
|     - | 2513 | `				 }` |
|   ! 0 | 2514 | `		case '?':` |
|   ! 0 | 2515 | `			if( z < zInEnd ){ z++; }` |
|   ! 0 | 2516 | `			break;` |
|   ! 0 | 2517 | `		case '*':` |
|     - | 2518 | `			/* skip input until the next separator byte */` |
|   ! 0 | 2519 | `			while( z < zInEnd && !SyisDigit(z[0]) && z[0] != ';' && z[0] != ':'` |
|   ! 0 | 2520 | `			 && z[0] != '/' && z[0] != '.' && z[0] != ',' && z[0] != '-'` |
|   ! 0 | 2521 | `			 && z[0] != '(' && z[0] != ')' && z[0] != ' ' ){` |
|   ! 0 | 2522 | `				z++;` |
|   ! 0 | 2523 | `			}` |
|   ! 0 | 2524 | `			break;` |
|     1 | 2525 | `		case '#':` |
|     3 | 2526 | `			if( z < zInEnd && (z[0]==';'\|\|z[0]==':'\|\|z[0]=='/'\|\|z[0]=='.'` |
|   ! 0 | 2527 | `			 \|\|z[0]==','\|\|z[0]=='-'\|\|z[0]=='('\|\|z[0]==')') ){` |
|     3 | 2528 | `				z++;` |
|     2 | 2529 | `			}else{` |
|   ! 0 | 2530 | `				zErr = "The separation symbol could not be found";` |
|     - | 2531 | `			}` |
|     3 | 2532 | `			break;` |
|     1 | 2533 | `		case '\\':` |
|     3 | 2534 | `			if( zFmt < zEnd ){` |
|     3 | 2535 | `				if( z < zInEnd && z[0] == zFmt[0] ){` |
|     3 | 2536 | `					z++;` |
|     3 | 2537 | `					zFmt++;` |
|     2 | 2538 | `				}else{` |
|     - | 2539 | `					/* a literal mismatch aborts timelib's scan */` |
|   ! 0 | 2540 | `					DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|   ! 0 | 2541 | `					zFmt = zEnd;` |
|   ! 0 | 2542 | `					bAborted = 1;` |
|     - | 2543 | `				}` |
|     1 | 2544 | `			}` |
|     3 | 2545 | `			break;` |
|    34 | 2546 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|     - | 2547 | `		case '(' : case ')':` |
|    69 | 2548 | `			if( z < zInEnd && z[0] == c ){` |
|    69 | 2549 | `				z++;` |
|    35 | 2550 | `			}else{` |
|     - | 2551 | `				/* timelib logs BOTH messages (count +2, last-wins on the` |
|     - | 2552 | `				 * position), consumes the offending byte, and keeps going */` |
|   ! 0 | 2553 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 2554 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 2555 | `				z++;` |
|     - | 2556 | `			}` |
|    69 | 2557 | `			break;` |
|    13 | 2558 | `		case ' ':` |
|    27 | 2559 | `			if( z < zInEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|    27 | 2560 | `				z++;` |
|    14 | 2561 | `			}else{` |
|   ! 0 | 2562 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 2563 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 2564 | `				z++;` |
|     - | 2565 | `			}` |
|    27 | 2566 | `			break;` |
|     1 | 2567 | `		default:` |
|     - | 2568 | `			/* any other format byte must match the input verbatim; a mismatch` |
|     - | 2569 | `			 * aborts timelib's scan */` |
|     3 | 2570 | `			if( z < zInEnd && z[0] == c ){` |
|   ! 0 | 2571 | `				z++;` |
|   ! 0 | 2572 | `			}else{` |
|     3 | 2573 | `				DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|     3 | 2574 | `				zFmt = zEnd;` |
|     3 | 2575 | `				bAborted = 1;` |
|     - | 2576 | `			}` |
|     2 | 2577 | `			break;` |
|     - | 2578 | `		}` |
|   243 | 2579 | `		if( zErr ){` |
|     - | 2580 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|   ! 0 | 2581 | `			DT_FF_LOGERR((int)(z - zIn),zErr);` |
|   ! 0 | 2582 | `		}` |
|     1 | 2583 | `	}` |
|    45 | 2584 | `	if( z < zInEnd && !bAborted ){` |
|     5 | 2585 | `		if( bPlus ){` |
|     - | 2586 | `			/* '+' downgrades trailing data to a warning */` |
|     3 | 2587 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     3 | 2588 | `			aWarnMsg[nWarn] = "Trailing data";` |
|     3 | 2589 | `			nWarn++;` |
|     2 | 2590 | `		}else{` |
|     3 | 2591 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|     - | 2592 | `		}` |
|     2 | 2593 | `	}` |
|    45 | 2594 | `	if( nErr > 0 ){` |
|     - | 2595 | `		SyBlob sOut;` |
|     - | 2596 | `		int k;` |
|     7 | 2597 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     7 | 2598 | `		SyBlobFormat(&sOut,"%d",nErr);` |
|    15 | 2599 | `		for( k = 0 ; k < nErrKept ; k++ ){` |
|     9 | 2600 | `			SyBlobFormat(&sOut,"\n%d\t%s",aErrPos[k],aErrMsg[k]);` |
|     5 | 2601 | `		}` |
|     7 | 2602 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     7 | 2603 | `		SyBlobRelease(&sOut);` |
|     7 | 2604 | `		return PH7_OK;` |
|     - | 2605 | `	}` |
|    39 | 2606 | `	if( bPipe ){` |
|     5 | 2607 | `		if( y < 0 ){ y = 1970; }` |
|     5 | 2608 | `		if( mo < 0 ){ mo = 1; }` |
|     5 | 2609 | `		if( d < 0 ){ d = 1; }` |
|     5 | 2610 | `		if( h < 0 && h12 < 0 ){ h = 0; }` |
|     5 | 2611 | `		if( mi < 0 ){ mi = 0; }` |
|     5 | 2612 | `		if( s < 0 ){ s = 0; }` |
|     2 | 2613 | `	}` |
|     - | 2614 | `	{` |
|     - | 2615 | `		/* remaining unset fields come from "now" in the default offset */` |
|    39 | 2616 | `		sxi64 iLocal = iNow + iDefOff;` |
|    39 | 2617 | `		sxi64 days = DtFloorDiv(iLocal,86400);` |
|    39 | 2618 | `		sxi64 secs = iLocal - days*86400;` |
|     - | 2619 | `		sxi64 ny;` |
|     - | 2620 | `		int nmo,nd;` |
|    39 | 2621 | `		DtCivilFromDays(days,&ny,&nmo,&nd);` |
|    39 | 2622 | `		if( y < 0 ){ y = ny; }` |
|    39 | 2623 | `		if( mo < 0 ){ mo = nmo; }` |
|    39 | 2624 | `		if( d < 0 ){ d = nd; }` |
|    39 | 2625 | `		if( h12 >= 0 ){` |
|     5 | 2626 | `			h = (h12 % 12) + ((iMeridiem == 1) ? 12 : 0);` |
|     2 | 2627 | `		}` |
|     - | 2628 | `		/* php: parsing a time component zeroes the finer unset units */` |
|    39 | 2629 | `		if( h >= 0 ){` |
|    19 | 2630 | `			if( mi < 0 ){ mi = 0; }` |
|    19 | 2631 | `			if( s < 0 ){ s = 0; }` |
|    30 | 2632 | `		}else if( mi >= 0 ){` |
|     3 | 2633 | `			if( s < 0 ){ s = 0; }` |
|     1 | 2634 | `		}` |
|    39 | 2635 | `		if( h < 0 ){ h = secs / 3600; }` |
|    39 | 2636 | `		if( mi < 0 ){ mi = (secs / 60) % 60; }` |
|    39 | 2637 | `		if( s < 0 ){ s = secs % 60; }` |
|     - | 2638 | `	}` |
|     - | 2639 | `	/* php validates the RESOLVED fields and warns (parse still succeeds,` |
|     - | 2640 | `	 * values roll over via civil arithmetic) */` |
|    39 | 2641 | `	if( mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo) ){` |
|     3 | 2642 | `		if( nWarn < 3 ){` |
|     3 | 2643 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 2644 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|     3 | 2645 | `			nWarn++;` |
|     1 | 2646 | `		}` |
|     1 | 2647 | `	}` |
|    39 | 2648 | `	if( h > 24 \|\| mi > 59 \|\| s > 59 ){` |
|     3 | 2649 | `		if( nWarn < 3 ){` |
|     3 | 2650 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 2651 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|     3 | 2652 | `			nWarn++;` |
|     1 | 2653 | `		}` |
|     1 | 2654 | `	}` |
|     - | 2655 | `	{` |
|    39 | 2656 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    39 | 2657 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|     - | 2658 | `		sxi64 iTs;` |
|    39 | 2659 | `		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;` |
|    39 | 2660 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2661 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2662 | `		}` |
|    39 | 2663 | `		if( bHasU ){` |
|     5 | 2664 | `			iTs = uVal;` |
|     5 | 2665 | `			iUseOff = 0;` |
|     5 | 2666 | `			iOffKind = 1;` |
|     3 | 2667 | `		}else{` |
|    35 | 2668 | `			iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);` |
|     - | 2669 | `		}` |
|    39 | 2670 | `		ph7_value_int64(pV,iTs);           ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2671 | `		ph7_value_int64(pV,iUseOff);       ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2672 | `		ph7_value_int64(pV,iOffKind);      ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2673 | `		ph7_value_string(pV,zName,-1);     ph7_array_add_elem(pArr,0,pV);` |
|     - | 2674 | `		{` |
|     - | 2675 | `			int k;` |
|    45 | 2676 | `			for( k = 0 ; k < nWarn ; k++ ){` |
|     7 | 2677 | `				ph7_value_int64(pV,aWarnPos[k]);` |
|     7 | 2678 | `				ph7_array_add_elem(pArr,0,pV);` |
|     7 | 2679 | `				ph7_value_string(pV,aWarnMsg[k],-1);` |
|     7 | 2680 | `				ph7_array_add_elem(pArr,0,pV);` |
|     4 | 2681 | `			}` |
|     - | 2682 | `		}` |
|    39 | 2683 | `		ph7_result_value(pCtx,pArr);` |
|     - | 2684 | `	}` |
|    39 | 2685 | `	return PH7_OK;` |
|    23 | 2686 | `}` |
|     - | 2687 | `/*` |
|     - | 2688 | ` * The embedded DateTime library. Timezone scope: UTC + fixed offsets.` |
|     - | 2689 | ` */` |
|     - | 2690 | `static const char zDateTimeLib[] =` |
|     - | 2691 | `"class DateException extends Exception {}"` |
|     - | 2692 | `"class DateMalformedStringException extends DateException {}"` |
|     - | 2693 | `"class DateInvalidTimeZoneException extends DateException {}"` |
|     - | 2694 | `"class DateMalformedIntervalStringException extends DateException {}"` |
|     - | 2695 | `"class DateMalformedPeriodStringException extends DateException {}"` |
|     - | 2696 | `"interface DateTimeInterface {"` |
|     - | 2697 | `" const ATOM = 'Y-m-d\\TH:i:sP';"` |
|     - | 2698 | `" const COOKIE = 'l, d-M-Y H:i:s T';"` |
|     - | 2699 | `" const ISO8601 = 'Y-m-d\\TH:i:sO';"` |
|     - | 2700 | `" const ISO8601_EXPANDED = 'X-m-d\\TH:i:sP';"` |
|     - | 2701 | `" const RFC822 = 'D, d M y H:i:s O';"` |
|     - | 2702 | `" const RFC850 = 'l, d-M-y H:i:s T';"` |
|     - | 2703 | `" const RFC1036 = 'D, d M y H:i:s O';"` |
|     - | 2704 | `" const RFC1123 = 'D, d M Y H:i:s O';"` |
|     - | 2705 | `" const RFC7231 = 'D, d M Y H:i:s \\G\\M\\T';"` |
|     - | 2706 | `" const RFC2822 = 'D, d M Y H:i:s O';"` |
|     - | 2707 | `" const RFC3339 = 'Y-m-d\\TH:i:sP';"` |
|     - | 2708 | `" const RFC3339_EXTENDED = 'Y-m-d\\TH:i:s.vP';"` |
|     - | 2709 | `" const RSS = 'D, d M Y H:i:s O';"` |
|     - | 2710 | `" const W3C = 'Y-m-d\\TH:i:sP';"` |
|     - | 2711 | `"}"` |
|     - | 2712 | `"class DateTimeZone {"` |
|     - | 2713 | `" private $__dtzOff = 0;"` |
|     - | 2714 | `" private $__dtzName = 'UTC';"` |
|     - | 2715 | `" public function __construct($timezone = 'UTC'){"` |
|     - | 2716 | `"  $tz = (string)$timezone;"` |
|     - | 2717 | `"  if( strcasecmp($tz, 'UTC') === 0 ){"` |
|     - | 2718 | `"   $this->__dtzOff = 0; $this->__dtzName = 'UTC';"` |
|     - | 2719 | `"   return;"` |
|     - | 2720 | `"  }"` |
|     - | 2721 | `"  if( $tz === 'Z' ){"` |
|     - | 2722 | `"   $this->__dtzOff = 0; $this->__dtzName = 'Z';"` |
|     - | 2723 | `"   return;"` |
|     - | 2724 | `"  }"` |
|     - | 2725 | `"  if( strcasecmp($tz, 'GMT') === 0 ){"` |
|     - | 2726 | `"   $this->__dtzOff = 0; $this->__dtzName = 'GMT';"` |
|     - | 2727 | `"   return;"` |
|     - | 2728 | `"  }"` |
|     - | 2729 | `"  $m = null;"` |
|     - | 2730 | `"  if( preg_match('/^([+-])(\\d{2}):?(\\d{2})$/', $tz, $m) ){"` |
|     - | 2731 | `"   $off = ((int)$m[2]) * 3600 + ((int)$m[3]) * 60;"` |
|     - | 2732 | `"   if( $m[1] === '-' ){ $off = -$off; }"` |
|     - | 2733 | `"   $this->__dtzOff = $off;"` |
|     - | 2734 | `"   $this->__dtzName = $m[1] . $m[2] . ':' . $m[3];"` |
|     - | 2735 | `"   return;"` |
|     - | 2736 | `"  }"` |
|     - | 2737 | `"  throw new DateInvalidTimeZoneException("` |
|     - | 2738 | `"   'DateTimeZone::__construct(): Unknown or bad timezone (' . $tz . ')');"` |
|     - | 2739 | `" }"` |
|     - | 2740 | `" public function getName(){ return $this->__dtzName; }"` |
|     - | 2741 | `" public function getOffset($datetime = null){ return $this->__dtzOff; }"` |
|     - | 2742 | `"}"` |
|     - | 2743 | `"trait __DtCoreT {"` |
|     - | 2744 | `" private $__dtTs = 0;"` |
|     - | 2745 | `" private $__dtOff = 0;"` |
|     - | 2746 | `" private $__dtName = 'UTC';"` |
|     - | 2747 | `" private function __dtInit($datetime, $timezone){"` |
|     - | 2748 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 2749 | `"  if( $timezone !== null ){"` |
|     - | 2750 | `"   $off = $timezone->getOffset($this);"` |
|     - | 2751 | `"   $name = $timezone->getName();"` |
|     - | 2752 | `"  }"` |
|     - | 2753 | `"  $r = __dt_parse((string)$datetime, __dt_now(), $off);"` |
|     - | 2754 | `"  if( is_string($r) ){ throw new DateMalformedStringException($r); }"` |
|     - | 2755 | `"  $this->__dtTs = $r[0];"` |
|     - | 2756 | `"  if( $r[2] ){"` |
|     - | 2757 | `"   $this->__dtOff = $r[1];"` |
|     - | 2758 | `"   $this->__dtName = $r[2] === 2 ? 'Z' : $this->__dtOffName($r[1]);"` |
|     - | 2759 | `"  }else{"` |
|     - | 2760 | `"   $this->__dtOff = $off;"` |
|     - | 2761 | `"   $this->__dtName = $name;"` |
|     - | 2762 | `"  }"` |
|     - | 2763 | `" }"` |
|     - | 2764 | `" private function __dtOffName($off){"` |
|     - | 2765 | `"  $s = $off < 0 ? '-' : '+';"` |
|     - | 2766 | `"  $a = $off < 0 ? -$off : $off;"` |
|     - | 2767 | `"  return $s . sprintf('%02d:%02d', intdiv($a, 3600), intdiv($a % 3600, 60));"` |
|     - | 2768 | `" }"` |
|     - | 2769 | `" public function format($format){ return __dt_format($this->__dtTs, $this->__dtOff, $this->__dtName, (string)$format); }"` |
|     - | 2770 | `" public function getTimestamp(){ return $this->__dtTs; }"` |
|     - | 2771 | `" public function getOffset(){ return $this->__dtOff; }"` |
|     - | 2772 | `" public function getTimezone(){ return new DateTimeZone($this->__dtName); }"` |
|     - | 2773 | `" public function diff($targetObject, $absolute = false){"` |
|     - | 2774 | `"  $r = __dt_civil_diff($this->__dtTs, $this->__dtOff, $targetObject->getTimestamp());"` |
|     - | 2775 | `"  $iv = new DateInterval('P0D');"` |
|     - | 2776 | `"  $iv->y = $r[0]; $iv->m = $r[1]; $iv->d = $r[2];"` |
|     - | 2777 | `"  $iv->h = $r[3]; $iv->i = $r[4]; $iv->s = $r[5];"` |
|     - | 2778 | `"  $iv->days = $r[6];"` |
|     - | 2779 | `"  $iv->invert = $absolute ? 0 : $r[7];"` |
|     - | 2780 | `"  return $iv;"` |
|     - | 2781 | `" }"` |
|     - | 2782 | `" private function __dtAddTs($interval, $sign){"` |
|     - | 2783 | `"  if( $interval->invert ){ $sign = -$sign; }"` |
|     - | 2784 | `"  return __dt_civil_add($this->__dtTs, $this->__dtOff, $interval->y, $interval->m,"` |
|     - | 2785 | `"   $interval->d, $interval->h, $interval->i, $interval->s, $sign);"` |
|     - | 2786 | `" }"` |
|     - | 2787 | `" private static function __dtFromFormat($format, $datetime, $timezone, $class){"` |
|     - | 2788 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 2789 | `"  if( $timezone !== null ){"` |
|     - | 2790 | `"   $off = $timezone->getOffset(null);"` |
|     - | 2791 | `"   $name = $timezone->getName();"` |
|     - | 2792 | `"  }"` |
|     - | 2793 | `"  $r = __dt_from_format((string)$format, (string)$datetime, __dt_now(), $off);"` |
|     - | 2794 | `"  if( is_string($r) ){"` |
|     - | 2795 | `"   $lines = explode(\"\\n\", $r);"` |
|     - | 2796 | `"   $errs = [];"` |
|     - | 2797 | `"   $nl = count($lines);"` |
|     - | 2798 | `"   for( $k = 1; $k < $nl; $k++ ){"` |
|     - | 2799 | `"    $p = strpos($lines[$k], \"\\t\");"` |
|     - | 2800 | `"    $errs[(int)substr($lines[$k], 0, $p)] = substr($lines[$k], $p + 1);"` |
|     - | 2801 | `"   }"` |
|     - | 2802 | `"   DateTime::$__dtLastErr = ['warning_count' => 0, 'warnings' => [],"` |
|     - | 2803 | `"    'error_count' => (int)$lines[0], 'errors' => $errs];"` |
|     - | 2804 | `"   return false;"` |
|     - | 2805 | `"  }"` |
|     - | 2806 | `"  if( isset($r[4]) ){"` |
|     - | 2807 | `"   $warns = [];"` |
|     - | 2808 | `"   $wc = 0;"` |
|     - | 2809 | `"   for( $k = 4; isset($r[$k]); $k += 2 ){"` |
|     - | 2810 | `"    $warns[$r[$k]] = $r[$k + 1];"` |
|     - | 2811 | `"    $wc++;"` |
|     - | 2812 | `"   }"` |
|     - | 2813 | `"   DateTime::$__dtLastErr = ['warning_count' => $wc, 'warnings' => $warns,"` |
|     - | 2814 | `"    'error_count' => 0, 'errors' => []];"` |
|     - | 2815 | `"  }else{"` |
|     - | 2816 | `"   DateTime::$__dtLastErr = false;"` |
|     - | 2817 | `"  }"` |
|     - | 2818 | `"  $obj = new $class('@0');"` |
|     - | 2819 | `"  $obj->__dtTs = $r[0];"` |
|     - | 2820 | `"  if( $r[2] === 0 ){ $obj->__dtOff = $off; $obj->__dtName = $name; }"` |
|     - | 2821 | `"  elseif( $r[2] === 2 ){ $obj->__dtOff = 0; $obj->__dtName = 'Z'; }"` |
|     - | 2822 | `"  elseif( $r[2] === 3 ){ $obj->__dtOff = $r[1]; $obj->__dtName = $r[3]; }"` |
|     - | 2823 | `"  else { $obj->__dtOff = $r[1]; $obj->__dtName = $obj->__dtOffName($r[1]); }"` |
|     - | 2824 | `"  return $obj;"` |
|     - | 2825 | `" }"` |
|     - | 2826 | `" private static function __dtCopyOf($object, $class){"` |
|     - | 2827 | `"  $d = new $class('@0');"` |
|     - | 2828 | `"  $d->__dtTs = $object->getTimestamp();"` |
|     - | 2829 | `"  $d->__dtOff = $object->getOffset();"` |
|     - | 2830 | `"  $d->__dtName = $object->getTimezone()->getName();"` |
|     - | 2831 | `"  return $d;"` |
|     - | 2832 | `" }"` |
|     - | 2833 | `"}"` |
|     - | 2834 | `"class DateTime implements DateTimeInterface {"` |
|     - | 2835 | `" use __DtCoreT;"` |
|     - | 2836 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 2837 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 2838 | `" }"` |
|     - | 2839 | `" public function modify($modifier){"` |
|     - | 2840 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 2841 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTime::modify(): ' . $r); }"` |
|     - | 2842 | `"  $this->__dtTs = $r[0];"` |
|     - | 2843 | `"  return $this;"` |
|     - | 2844 | `" }"` |
|     - | 2845 | `" public function setTimestamp($timestamp){ $this->__dtTs = (int)$timestamp; return $this; }"` |
|     - | 2846 | `" public function setTimezone($timezone){"` |
|     - | 2847 | `"  $this->__dtOff = $timezone->getOffset($this);"` |
|     - | 2848 | `"  $this->__dtName = $timezone->getName();"` |
|     - | 2849 | `"  return $this;"` |
|     - | 2850 | `" }"` |
|     - | 2851 | `" public function setDate($year, $month, $day){"` |
|     - | 2852 | `"  $this->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 2853 | `"  return $this;"` |
|     - | 2854 | `" }"` |
|     - | 2855 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 2856 | `"  $this->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 2857 | `"  return $this;"` |
|     - | 2858 | `" }"` |
|     - | 2859 | `" public function add($interval){ $this->__dtTs = $this->__dtAddTs($interval, 1); return $this; }"` |
|     - | 2860 | `" public function sub($interval){ $this->__dtTs = $this->__dtAddTs($interval, -1); return $this; }"` |
|     - | 2861 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 2862 | `"  $this->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 2863 | `"  return $this;"` |
|     - | 2864 | `" }"` |
|     - | 2865 | `" public static $__dtLastErr = false;"` |
|     - | 2866 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 2867 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 2868 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTime');"` |
|     - | 2869 | `" }"` |
|     - | 2870 | `" public static function createFromImmutable($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 2871 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 2872 | `"}"` |
|     - | 2873 | `"class DateTimeImmutable implements DateTimeInterface {"` |
|     - | 2874 | `" use __DtCoreT;"` |
|     - | 2875 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 2876 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 2877 | `" }"` |
|     - | 2878 | `" public function modify($modifier){"` |
|     - | 2879 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 2880 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTimeImmutable::modify(): ' . $r); }"` |
|     - | 2881 | `"  $c = clone $this;"` |
|     - | 2882 | `"  $c->__dtTs = $r[0];"` |
|     - | 2883 | `"  return $c;"` |
|     - | 2884 | `" }"` |
|     - | 2885 | `" public function setTimestamp($timestamp){ $c = clone $this; $c->__dtTs = (int)$timestamp; return $c; }"` |
|     - | 2886 | `" public function setTimezone($timezone){"` |
|     - | 2887 | `"  $c = clone $this;"` |
|     - | 2888 | `"  $c->__dtOff = $timezone->getOffset($this);"` |
|     - | 2889 | `"  $c->__dtName = $timezone->getName();"` |
|     - | 2890 | `"  return $c;"` |
|     - | 2891 | `" }"` |
|     - | 2892 | `" public function setDate($year, $month, $day){"` |
|     - | 2893 | `"  $c = clone $this;"` |
|     - | 2894 | `"  $c->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 2895 | `"  return $c;"` |
|     - | 2896 | `" }"` |
|     - | 2897 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 2898 | `"  $c = clone $this;"` |
|     - | 2899 | `"  $c->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 2900 | `"  return $c;"` |
|     - | 2901 | `" }"` |
|     - | 2902 | `" public function add($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, 1); return $c; }"` |
|     - | 2903 | `" public function sub($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, -1); return $c; }"` |
|     - | 2904 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 2905 | `"  $c = clone $this;"` |
|     - | 2906 | `"  $c->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 2907 | `"  return $c;"` |
|     - | 2908 | `" }"` |
|     - | 2909 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 2910 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 2911 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTimeImmutable');"` |
|     - | 2912 | `" }"` |
|     - | 2913 | `" public static function createFromMutable($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 2914 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 2915 | `"}"` |
|     - | 2916 | `"function date_create($datetime = 'now', $timezone = null){"` |
|     - | 2917 | `" try { return new DateTime($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 2918 | `"}"` |
|     - | 2919 | `"function date_create_immutable($datetime = 'now', $timezone = null){"` |
|     - | 2920 | `" try { return new DateTimeImmutable($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 2921 | `"}"` |
|     - | 2922 | `"class DateInterval {"` |
|     - | 2923 | `" public $y = 0;"` |
|     - | 2924 | `" public $m = 0;"` |
|     - | 2925 | `" public $d = 0;"` |
|     - | 2926 | `" public $h = 0;"` |
|     - | 2927 | `" public $i = 0;"` |
|     - | 2928 | `" public $s = 0;"` |
|     - | 2929 | `" public $f = 0;"` |
|     - | 2930 | `" public $invert = 0;"` |
|     - | 2931 | `" public $days = false;"` |
|     - | 2932 | `" public $from_string = false;"` |
|     - | 2933 | `" public function __construct($duration = 'P0D'){"` |
|     - | 2934 | `"  $dur = (string)$duration;"` |
|     - | 2935 | `"  $mm = null;"` |
|     - | 2936 | `"  if( strlen($dur) < 2 \|\| substr($dur, -1) === 'T'"` |
|     - | 2937 | `"   \|\| !preg_match('/^P(?:(\\d+)Y)?(?:(\\d+)M)?(?:(\\d+)W)?(?:(\\d+)D)?(?:T(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?)?$/', $dur, $mm) ){"` |
|     - | 2938 | `"   throw new DateMalformedIntervalStringException('Unknown or bad format (' . $dur . ')');"` |
|     - | 2939 | `"  }"` |
|     - | 2940 | `"  $this->y = (int)($mm[1] ?? 0);"` |
|     - | 2941 | `"  $this->m = (int)($mm[2] ?? 0);"` |
|     - | 2942 | `"  $this->d = (int)($mm[4] ?? 0) + 7 * (int)($mm[3] ?? 0);"` |
|     - | 2943 | `"  $this->h = (int)($mm[5] ?? 0);"` |
|     - | 2944 | `"  $this->i = (int)($mm[6] ?? 0);"` |
|     - | 2945 | `"  $this->s = (int)($mm[7] ?? 0);"` |
|     - | 2946 | `" }"` |
|     - | 2947 | `" public static function createFromDateString($datetime){"` |
|     - | 2948 | `"  $s = trim((string)$datetime);"` |
|     - | 2949 | `"  $iv = new DateInterval('P0D');"` |
|     - | 2950 | `"  $rest = $s;"` |
|     - | 2951 | `"  $any = false;"` |
|     - | 2952 | `"  while( $rest !== '' ){"` |
|     - | 2953 | `"   $mm = null;"` |
|     - | 2954 | `"   if( !preg_match('/^[\\s,+]*([+-]?\\d+)\\s*(sec\|secs\|second\|seconds\|min\|mins\|minute\|minutes\|hour\|hours\|day\|days\|week\|weeks\|fortnight\|fortnights\|month\|months\|year\|years)\\b/i', $rest, $mm) ){"` |
|     - | 2955 | `"    throw new DateMalformedIntervalStringException("` |
|     - | 2956 | `"     'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 2957 | `"   }"` |
|     - | 2958 | `"   $n = (int)$mm[1];"` |
|     - | 2959 | `"   $u = strtolower($mm[2]);"` |
|     - | 2960 | `"   if( $u === 'sec' \|\| $u === 'secs' \|\| $u === 'second' \|\| $u === 'seconds' ){ $iv->s += $n; }"` |
|     - | 2961 | `"   elseif( $u === 'min' \|\| $u === 'mins' \|\| $u === 'minute' \|\| $u === 'minutes' ){ $iv->i += $n; }"` |
|     - | 2962 | `"   elseif( $u === 'hour' \|\| $u === 'hours' ){ $iv->h += $n; }"` |
|     - | 2963 | `"   elseif( $u === 'day' \|\| $u === 'days' ){ $iv->d += $n; }"` |
|     - | 2964 | `"   elseif( $u === 'week' \|\| $u === 'weeks' ){ $iv->d += 7 * $n; }"` |
|     - | 2965 | `"   elseif( $u === 'fortnight' \|\| $u === 'fortnights' ){ $iv->d += 14 * $n; }"` |
|     - | 2966 | `"   elseif( $u === 'month' \|\| $u === 'months' ){ $iv->m += $n; }"` |
|     - | 2967 | `"   else { $iv->y += $n; }"` |
|     - | 2968 | `"   $any = true;"` |
|     - | 2969 | `"   $rest = ltrim(substr($rest, strlen($mm[0])));"` |
|     - | 2970 | `"  }"` |
|     - | 2971 | `"  if( !$any ){"` |
|     - | 2972 | `"   throw new DateMalformedIntervalStringException("` |
|     - | 2973 | `"    'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 2974 | `"  }"` |
|     - | 2975 | `"  return $iv;"` |
|     - | 2976 | `" }"` |
|     - | 2977 | `" public function format($format){"` |
|     - | 2978 | `"  $f = (string)$format;"` |
|     - | 2979 | `"  $out = '';"` |
|     - | 2980 | `"  $n = strlen($f);"` |
|     - | 2981 | `"  for( $k = 0; $k < $n; $k++ ){"` |
|     - | 2982 | `"   $c = $f[$k];"` |
|     - | 2983 | `"   if( $c !== '%' ){ $out .= $c; continue; }"` |
|     - | 2984 | `"   $k++;"` |
|     - | 2985 | `"   if( $k >= $n ){ $out .= '%'; break; }"` |
|     - | 2986 | `"   $t = $f[$k];"` |
|     - | 2987 | `"   if( $t === 'Y' ){ $out .= sprintf('%02d', $this->y); }"` |
|     - | 2988 | `"   elseif( $t === 'y' ){ $out .= $this->y; }"` |
|     - | 2989 | `"   elseif( $t === 'M' ){ $out .= sprintf('%02d', $this->m); }"` |
|     - | 2990 | `"   elseif( $t === 'm' ){ $out .= $this->m; }"` |
|     - | 2991 | `"   elseif( $t === 'D' ){ $out .= sprintf('%02d', $this->d); }"` |
|     - | 2992 | `"   elseif( $t === 'd' ){ $out .= $this->d; }"` |
|     - | 2993 | `"   elseif( $t === 'H' ){ $out .= sprintf('%02d', $this->h); }"` |
|     - | 2994 | `"   elseif( $t === 'h' ){ $out .= $this->h; }"` |
|     - | 2995 | `"   elseif( $t === 'I' ){ $out .= sprintf('%02d', $this->i); }"` |
|     - | 2996 | `"   elseif( $t === 'i' ){ $out .= $this->i; }"` |
|     - | 2997 | `"   elseif( $t === 'S' ){ $out .= sprintf('%02d', $this->s); }"` |
|     - | 2998 | `"   elseif( $t === 's' ){ $out .= $this->s; }"` |
|     - | 2999 | `"   elseif( $t === 'F' ){ $out .= sprintf('%06d', (int)round($this->f * 1000000)); }"` |
|     - | 3000 | `"   elseif( $t === 'f' ){ $out .= (int)round($this->f * 1000000); }"` |
|     - | 3001 | `"   elseif( $t === 'R' ){ $out .= $this->invert ? '-' : '+'; }"` |
|     - | 3002 | `"   elseif( $t === 'r' ){ $out .= $this->invert ? '-' : ''; }"` |
|     - | 3003 | `"   elseif( $t === 'a' ){ $out .= $this->days === false ? '(unknown)' : $this->days; }"` |
|     - | 3004 | `"   elseif( $t === '%' ){ $out .= '%'; }"` |
|     - | 3005 | `"   else { $out .= $t; }"` |
|     - | 3006 | `"  }"` |
|     - | 3007 | `"  return $out;"` |
|     - | 3008 | `" }"` |
|     - | 3009 | `"}"` |
|     - | 3010 | `"class DatePeriod implements IteratorAggregate {"` |
|     - | 3011 | `" const EXCLUDE_START_DATE = 1;"` |
|     - | 3012 | `" const INCLUDE_END_DATE = 2;"` |
|     - | 3013 | `" public $start = null;"` |
|     - | 3014 | `" public $current = null;"` |
|     - | 3015 | `" public $end = null;"` |
|     - | 3016 | `" public $interval = null;"` |
|     - | 3017 | `" public $recurrences = 1;"` |
|     - | 3018 | `" public $include_start_date = true;"` |
|     - | 3019 | `" public $include_end_date = false;"` |
|     - | 3020 | `" private $__dpN = null;"` |
|     - | 3021 | `" public function __construct($start, $interval = null, $end = null, $options = 0){"` |
|     - | 3022 | `"  if( is_string($start) ){"` |
|     - | 3023 | `"   $mm = null;"` |
|     - | 3024 | `"   if( !preg_match('/^R(\\d+)\\/(.+)\\/(P.+)$/', $start, $mm) ){"` |
|     - | 3025 | `"    throw new DateMalformedPeriodStringException("` |
|     - | 3026 | `"     'DatePeriod::__construct(): Unknown or bad format (' . $start . ')');"` |
|     - | 3027 | `"   }"` |
|     - | 3028 | `"   $options = is_int($interval) ? $interval : 0;"` |
|     - | 3029 | `"   $this->start = new DateTimeImmutable($mm[2]);"` |
|     - | 3030 | `"   $this->interval = new DateInterval($mm[3]);"` |
|     - | 3031 | `"   $this->__dpN = (int)$mm[1];"` |
|     - | 3032 | `"   $this->recurrences = $this->__dpN + 1;"` |
|     - | 3033 | `"  }else{"` |
|     - | 3034 | `"   $this->start = clone $start;"` |
|     - | 3035 | `"   $this->interval = $interval;"` |
|     - | 3036 | `"   if( is_int($end) ){"` |
|     - | 3037 | `"    $this->__dpN = $end;"` |
|     - | 3038 | `"    $this->recurrences = $end + 1;"` |
|     - | 3039 | `"   }else{"` |
|     - | 3040 | `"    $this->end = $end === null ? null : (clone $end);"` |
|     - | 3041 | `"   }"` |
|     - | 3042 | `"  }"` |
|     - | 3043 | `"  $this->include_start_date = !((int)$options & 1);"` |
|     - | 3044 | `"  $this->include_end_date = ((int)$options & 2) !== 0;"` |
|     - | 3045 | `" }"` |
|     - | 3046 | `" public static function createFromISO8601String($specification, $options = 0){"` |
|     - | 3047 | `"  return new DatePeriod((string)$specification, (int)$options);"` |
|     - | 3048 | `" }"` |
|     - | 3049 | `" public function getStartDate(){ return $this->start; }"` |
|     - | 3050 | `" public function getEndDate(){ return $this->end; }"` |
|     - | 3051 | `" public function getDateInterval(){ return $this->interval; }"` |
|     - | 3052 | `" public function getRecurrences(){ return $this->__dpN; }"` |
|     - | 3053 | `" public function getIterator(): Generator {"` |
|     - | 3054 | `"  $cur = $this->start;"` |
|     - | 3055 | `"  $iv = $this->interval;"` |
|     - | 3056 | `"  $k = 0;"` |
|     - | 3057 | `"  if( $this->end !== null ){"` |
|     - | 3058 | `"   $endTs = $this->end->getTimestamp();"` |
|     - | 3059 | `"   $first = true;"` |
|     - | 3060 | `"   while( true ){"` |
|     - | 3061 | `"    $ts = $cur->getTimestamp();"` |
|     - | 3062 | `"    if( $this->include_end_date ? ($ts > $endTs) : ($ts >= $endTs) ){ break; }"` |
|     - | 3063 | `"    if( !$first \|\| $this->include_start_date ){"` |
|     - | 3064 | `"     yield $k => (clone $cur);"` |
|     - | 3065 | `"     $k++;"` |
|     - | 3066 | `"    }"` |
|     - | 3067 | `"    $first = false;"` |
|     - | 3068 | `"    $next = clone $cur;"` |
|     - | 3069 | `"    $cur = $next->add($iv);"` |
|     - | 3070 | `"   }"` |
|     - | 3071 | `"   return;"` |
|     - | 3072 | `"  }"` |
|     - | 3073 | `"  $total = $this->__dpN + 1 + ($this->include_end_date ? 1 : 0);"` |
|     - | 3074 | `"  for( $j = 0; $j < $total; $j++ ){"` |
|     - | 3075 | `"   if( $j > 0 \|\| $this->include_start_date ){"` |
|     - | 3076 | `"    yield $k => (clone $cur);"` |
|     - | 3077 | `"    $k++;"` |
|     - | 3078 | `"   }"` |
|     - | 3079 | `"   $next = clone $cur;"` |
|     - | 3080 | `"   $cur = $next->add($iv);"` |
|     - | 3081 | `"  }"` |
|     - | 3082 | `" }"` |
|     - | 3083 | `"}"` |
|     - | 3084 | `"function date_format($object, $format){ return $object->format($format); }"` |
|     - | 3085 | `"function date_modify($object, $modifier){"` |
|     - | 3086 | `" try { return $object->modify($modifier); } catch (Exception $e) { return false; }"` |
|     - | 3087 | `"}"` |
|     - | 3088 | `"function date_add($object, $interval){ return $object->add($interval); }"` |
|     - | 3089 | `"function date_sub($object, $interval){ return $object->sub($interval); }"` |
|     - | 3090 | `"function date_diff($baseObject, $targetObject, $absolute = false){"` |
|     - | 3091 | `" return $baseObject->diff($targetObject, $absolute);"` |
|     - | 3092 | `"}"` |
|     - | 3093 | `"function date_timestamp_get($object){ return $object->getTimestamp(); }"` |
|     - | 3094 | `"function date_timestamp_set($object, $timestamp){ return $object->setTimestamp($timestamp); }"` |
|     - | 3095 | `"function date_timezone_get($object){ return $object->getTimezone(); }"` |
|     - | 3096 | `"function date_timezone_set($object, $timezone){ return $object->setTimezone($timezone); }"` |
|     - | 3097 | `"function date_offset_get($object){ return $object->getOffset(); }"` |
|     - | 3098 | `"function date_date_set($object, $year, $month, $day){ return $object->setDate($year, $month, $day); }"` |
|     - | 3099 | `"function date_time_set($object, $hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3100 | `" return $object->setTime($hour, $minute, $second, $microsecond);"` |
|     - | 3101 | `"}"` |
|     - | 3102 | `"function date_isodate_set($object, $year, $week, $dayOfWeek = 1){"` |
|     - | 3103 | `" return $object->setISODate($year, $week, $dayOfWeek);"` |
|     - | 3104 | `"}"` |
|     - | 3105 | `"function date_interval_create_from_date_string($datetime){"` |
|     - | 3106 | `" return DateInterval::createFromDateString($datetime);"` |
|     - | 3107 | `"}"` |
|     - | 3108 | `"function date_interval_format($object, $format){ return $object->format($format); }"` |
|     - | 3109 | `"function date_get_last_errors(){ return DateTime::getLastErrors(); }"` |
|     - | 3110 | `"function timezone_open($timezone){"` |
|     - | 3111 | `" try { return new DateTimeZone($timezone); } catch (Exception $e) { return false; }"` |
|     - | 3112 | `"}"` |
|     - | 3113 | `"function timezone_name_get($object){ return $object->getName(); }"` |
|     - | 3114 | `"function timezone_offset_get($object, $datetime){ return $object->getOffset($datetime); }"` |
|     - | 3115 | `;` |
|     - | 3116 | `/*` |
|     - | 3117 | ` * Install the DateTime family: thunks first, then the chunk. Called from` |
|     - | 3118 | ` * PH7_VmInit inside the bCompilingBuiltin window, after the Reflection` |
|     - | 3119 | ` * install (Exception must exist).` |
|     - | 3120 | ` */` |
|  3812 | 3121 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
|     5 | 3122 | `{` |
|     - | 3123 | `	static const struct {` |
|     - | 3124 | `		const char *zName;` |
|     - | 3125 | `		ProchHostFunction xFunc;` |
|     - | 3126 | `	} aFunc[] = {` |
|     - | 3127 | `		{ "__dt_now",    vm_builtin_dt_now },` |
|     - | 3128 | `		{ "__dt_default_tz", vm_builtin_dt_default_tz },` |
|     - | 3129 | `		{ "__dt_civil_add",  vm_builtin_dt_civil_add },` |
|     - | 3130 | `		{ "__dt_civil_diff", vm_builtin_dt_civil_diff },` |
|     - | 3131 | `		{ "__dt_isodate",    vm_builtin_dt_isodate },` |
|     - | 3132 | `		{ "__dt_from_format", vm_builtin_dt_from_format },` |
|     - | 3133 | `		{ "__dt_parse",  vm_builtin_dt_parse },` |
|     - | 3134 | `		{ "__dt_format", vm_builtin_dt_format },` |
|     - | 3135 | `		{ "__dt_make",   vm_builtin_dt_make },` |
|     - | 3136 | `	};` |
|     - | 3137 | `	sxu32 n;` |
|     - | 3138 | `	/* php's date.timezone default */` |
|  3817 | 3139 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|  3817 | 3140 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
| 38125 | 3141 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 34313 | 3142 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 17159 | 3143 | `	}` |
|  3817 | 3144 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zDateTimeLib,sizeof(zDateTimeLib)-1);` |
|     5 | 3145 | `}` |
|     - | 3146 |  |
|     - | 3147 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 3148 |  |
|     - | 3149 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 3150 | `/* Tiny build: no DateTime family (builtin layer disabled) */` |
|     - | 3151 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){` |
|     - | 3152 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|     - | 3153 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|     - | 3154 | `	return SXRET_OK;` |
|     - | 3155 | `}` |
|     - | 3156 | `#endif` |
|     - | 3157 |  |
