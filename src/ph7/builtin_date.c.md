# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 647/722 lines (89.61%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |    8 | `/*` |
|    - |    9 | ` * Date/Time functions` |
|    - |   10 | ` * Status:` |
|    - |   11 | ` *    Devel.` |
|    - |   12 | ` */` |
|    - |   13 | `#include <time.h>` |
|    - |   14 | `/* Civil-date helpers (defined with the DateTime layer below) */` |
|    - |   15 | `/*` |
|    - |   16 | ` * STRUCT_TM_TO_SYTM zeroes tm_gmtoff (struct tm carries it only as a BSD/glibc` |
|    - |   17 | ` * extension, absent on newlib/ESP32). Derive the zone offset portably from the` |
|    - |   18 | ` * broken-down civil fields and the timestamp they came from: for localtime()` |
|    - |   19 | ` * fills this yields the local UTC offset, for gmtime() fills it yields 0.` |
|    - |   20 | ` */` |
|  422 |   21 | `static void DtSytmFillOffset(Sytm *pSTm,time_t t)` |
|    1 |   22 | `{` |
|  634 |   23 | `	sxi64 iCivil = DtDaysFromCivil((sxi64)pSTm->tm_year,pSTm->tm_mon+1,pSTm->tm_mday) * 86400` |
|  422 |   24 | `		+ (sxi64)pSTm->tm_hour*3600 + (sxi64)pSTm->tm_min*60 + (sxi64)pSTm->tm_sec;` |
|  423 |   25 | `	pSTm->tm_gmtoff = (long)(iCivil - (sxi64)t);` |
|  423 |   26 | `}` |
|    - |   27 | `#ifdef __WINNT__` |
|    - |   28 | `#ifdef _MSC_VER` |
|    - |   29 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|    - |   30 | `#pragma warning(disable:4996) /* _CRT_SECURE_NO_WARNINGS */` |
|    - |   31 | `#endif` |
|    - |   32 | `#endif` |
|    - |   33 | `#endif` |
|    - |   34 | `#ifdef __WINNT__` |
|    - |   35 | `/* GetSystemTime() */` |
|    - |   36 | `#include <Windows.h>` |
|    - |   37 | `#ifdef _WIN32_WCE` |
|    - |   38 | `/* SPDX-SnippetBegin */` |
|    - |   39 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|    - |   40 | `/* SPDX-License-Identifier: blessing */` |
|    - |   41 | `/*` |
|    - |   42 | `** WindowsCE does not have a localtime() function.  So create a` |
|    - |   43 | `** substitute.` |
|    - |   44 | `** Taken from the SQLite3 source tree.` |
|    - |   45 | `** Status: Public domain` |
|    - |   46 | `*/` |
|    - |   47 | `struct tm *__cdecl localtime(const time_t *t)` |
|    - |   48 | `{` |
|    - |   49 | `  static struct tm y;` |
|    - |   50 | `  FILETIME uTm, lTm;` |
|    - |   51 | `  SYSTEMTIME pTm;` |
|    - |   52 | `  ph7_int64 t64;` |
|    - |   53 | `  t64 = *t;` |
|    - |   54 | `  t64 = (t64 + 11644473600)*10000000;` |
|    - |   55 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|    - |   56 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|    - |   57 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|    - |   58 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|    - |   59 | `  y.tm_year = pTm.wYear - 1900;` |
|    - |   60 | `  y.tm_mon = pTm.wMonth - 1;` |
|    - |   61 | `  y.tm_wday = pTm.wDayOfWeek;` |
|    - |   62 | `  y.tm_mday = pTm.wDay;` |
|    - |   63 | `  y.tm_hour = pTm.wHour;` |
|    - |   64 | `  y.tm_min = pTm.wMinute;` |
|    - |   65 | `  y.tm_sec = pTm.wSecond;` |
|    - |   66 | `  return &y;` |
|    - |   67 | `}` |
|    - |   68 | `/* SPDX-SnippetEnd */` |
|    - |   69 | `#endif /*_WIN32_WCE */` |
|    - |   70 | `#elif defined(__UNIXES__)` |
|    - |   71 | `#include <sys/time.h>` |
|    - |   72 | `#endif /* __WINNT__*/` |
|    - |   73 | `/*` |
|    - |   74 | ` * Resolve the current wall-clock time (epoch seconds + sub-second microseconds).` |
|    - |   75 | ` *` |
|    - |   76 | ` * An embedder may override the platform clock via PH7_CONFIG_CLOCK (e.g. the` |
|    - |   77 | ` * ESP32 port routes this through esp_timer); when no hook is registered we use` |
|    - |   78 | ` * gettimeofday() on Unix and fall back to a second-resolution time() elsewhere.` |
|    - |   79 | ` * Centralising this here gives microtime()/gettimeofday() a single sub-second` |
|    - |   80 | `` * source instead of the old nonsensical `tt % SX_USEC_PER_SEC` off-Unix path.`` |
|    - |   81 | ` */` |
|   38 |   82 | `static void DateNow(ph7_vm *pVm,sytime *pOut)` |
|    1 |   83 | `{` |
|   39 |   84 | `	if( pVm && pVm->pEngine->xConf.xClock ){` |
|  ! 0 |   85 | `		ph7_int64 sec = 0,usec = 0;` |
|  ! 0 |   86 | `		if( pVm->pEngine->xConf.xClock(pVm->pEngine->xConf.pClockData,&sec,&usec) == PH7_OK ){` |
|  ! 0 |   87 | `			pOut->tm_sec  = (long)sec;` |
|  ! 0 |   88 | `			pOut->tm_usec = (long)usec;` |
|  ! 0 |   89 | `			return;` |
|    - |   90 | `		}` |
|  ! 0 |   91 | `	}` |
|    - |   92 | `#if defined(__UNIXES__)` |
|    - |   93 | `	{` |
|    - |   94 | `		struct timeval tv;` |
|   38 |   95 | `		gettimeofday(&tv,0);` |
|   38 |   96 | `		pOut->tm_sec  = (long)tv.tv_sec;` |
|   38 |   97 | `		pOut->tm_usec = (long)tv.tv_usec;` |
|    - |   98 | `	}` |
|    - |   99 | `#elif defined(__WINNT__)` |
|    - |  100 | `	{` |
|    - |  101 | `		/* FILETIME is 100-ns ticks since 1601-01-01 UTC; convert to the Unix` |
|    - |  102 | `		 * epoch with microsecond resolution (GetSystemTime() only carries` |
|    - |  103 | `		 * milliseconds, and time() has no sub-second part at all). */` |
|    - |  104 | `		FILETIME ft;` |
|    - |  105 | `		ph7_int64 t;` |
|    1 |  106 | `		GetSystemTimeAsFileTime(&ft);` |
|    1 |  107 | `		t  = (ph7_int64)ft.dwHighDateTime << 32;` |
|    1 |  108 | `		t += ft.dwLowDateTime;` |
|    1 |  109 | `		t -= 116444736000000000LL; /* 100-ns ticks between 1601 and 1970 */` |
|    1 |  110 | `		pOut->tm_sec  = (long)(t / 10000000);` |
|    1 |  111 | `		pOut->tm_usec = (long)((t % 10000000) / 10);` |
|    - |  112 | `	}` |
|    - |  113 | `#else` |
|    - |  114 | `	{` |
|    - |  115 | `		time_t tt;` |
|    - |  116 | `		time(&tt);` |
|    - |  117 | `		pOut->tm_sec  = (long)tt;` |
|    - |  118 | `		pOut->tm_usec = 0; /* no sub-second source; embedders supply one via PH7_CONFIG_CLOCK */` |
|    - |  119 | `	}` |
|    - |  120 | `#endif /* __UNIXES__ */` |
|   20 |  121 | `}` |
|    - |  122 | ` /*` |
|    - |  123 | `  * int64 time(void)` |
|    - |  124 | `  *  Current Unix timestamp` |
|    - |  125 | `  * Parameters` |
|    - |  126 | `  *  None.` |
|    - |  127 | `  * Return` |
|    - |  128 | `  *  Returns the current time measured in the number of seconds` |
|    - |  129 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|    - |  130 | `  */` |
|    8 |  131 | `PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  132 | `{` |
|    - |  133 | `	time_t tt;` |
|    4 |  134 | `	SXUNUSED(nArg); /* cc warning */` |
|    4 |  135 | `	SXUNUSED(apArg);` |
|    - |  136 | `	/* Extract the current time */` |
|    9 |  137 | `	time(&tt);` |
|    - |  138 | `	/* Return as 64-bit integer */` |
|    9 |  139 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|    9 |  140 | `	return  PH7_OK;` |
|    1 |  141 | `}` |
|    - |  142 | `/*` |
|    - |  143 | `  * string/float microtime([ bool $get_as_float = false ])` |
|    - |  144 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|    - |  145 | `  * Parameters` |
|    - |  146 | `  *  $get_as_float` |
|    - |  147 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|    - |  148 | `  *   as described in the return values section below.` |
|    - |  149 | `  * Return` |
|    - |  150 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|    - |  151 | `  *  is the current time measured in the number of seconds since the Unix` |
|    - |  152 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|    - |  153 | `  *  that have elapsed since sec expressed in seconds.` |
|    - |  154 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|    - |  155 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|    - |  156 | `  */` |
|   30 |  157 | `PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  158 | `{` |
|   31 |  159 | `	int bFloat = 0;` |
|    - |  160 | `	sytime sTime;` |
|   31 |  161 | `	DateNow(pCtx->pVm,&sTime);` |
|   31 |  162 | `	if( nArg > 0 ){` |
|   25 |  163 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|   12 |  164 | `	}` |
|   31 |  165 | `	if( bFloat ){` |
|    - |  166 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|   25 |  167 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|   13 |  168 | `	}else{` |
|    - |  169 | `		/* Return PHP's "msec sec" form: the sub-second part as fractional` |
|    - |  170 | `		 * seconds to 8 decimals, e.g. "0.50667100 1700000000". tm_usec is in` |
|    - |  171 | `		 * microseconds (0..999999), so scaling by 100 yields the 8-digit` |
|    - |  172 | `		 * fraction — matching PHP's "%.8F" output exactly. */` |
|    7 |  173 | `		ph7_result_string_format(pCtx,"0.%08ld %ld",sTime.tm_usec*100,sTime.tm_sec);` |
|    - |  174 | `	}` |
|   31 |  175 | `	return PH7_OK;` |
|    1 |  176 | `}` |
|    - |  177 | `/*` |
|    - |  178 | ` * array\|int hrtime(bool $as_number = false)` |
|    - |  179 | ` *  The system's high-resolution time, counted from an arbitrary monotonic` |
|    - |  180 | ` *  point in nanoseconds. Returns [seconds, nanoseconds] by default, or the` |
|    - |  181 | ` *  total nanoseconds as an int when $as_number is true.` |
|    - |  182 | ` */` |
|    8 |  183 | `PH7_PRIVATE int PH7_builtin_hrtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  184 | `{` |
|    9 |  185 | `	ph7_int64 sec = 0,nsec = 0;` |
|    9 |  186 | `	int bAsNumber = 0;` |
|    9 |  187 | `	if( nArg > 0 ){` |
|    7 |  188 | `		bAsNumber = ph7_value_to_bool(apArg[0]);` |
|    3 |  189 | `	}` |
|    - |  190 | `#if defined(CLOCK_MONOTONIC)` |
|    - |  191 | `	{` |
|    - |  192 | `		struct timespec ts;` |
|    8 |  193 | `		if( clock_gettime(CLOCK_MONOTONIC,&ts) == 0 ){` |
|    8 |  194 | `			sec  = (ph7_int64)ts.tv_sec;` |
|    8 |  195 | `			nsec = (ph7_int64)ts.tv_nsec;` |
|    4 |  196 | `		}` |
|    - |  197 | `	}` |
|    - |  198 | `#else` |
|    - |  199 | `	{` |
|    - |  200 | `		/* No monotonic clock available: fall back to the wall-clock microsecond` |
|    - |  201 | `		 * source (embedder clock / gettimeofday). Coarser and not strictly` |
|    - |  202 | `		 * monotonic, but keeps hrtime() usable off-Unix. */` |
|    - |  203 | `		sytime sTime;` |
|    1 |  204 | `		DateNow(pCtx->pVm,&sTime);` |
|    1 |  205 | `		sec  = (ph7_int64)sTime.tm_sec;` |
|    1 |  206 | `		nsec = (ph7_int64)sTime.tm_usec * 1000;` |
|    - |  207 | `	}` |
|    - |  208 | `#endif` |
|    9 |  209 | `	if( bAsNumber ){` |
|    7 |  210 | `		ph7_result_int64(pCtx,sec * 1000000000LL + nsec);` |
|    4 |  211 | `	}else{` |
|    - |  212 | `		ph7_value *pValue,*pArray;` |
|    3 |  213 | `		pArray = ph7_context_new_array(pCtx);` |
|    3 |  214 | `		pValue = ph7_context_new_scalar(pCtx);` |
|    3 |  215 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|  ! 0 |  216 | `			ph7_result_null(pCtx);` |
|  ! 0 |  217 | `			return PH7_OK;` |
|    - |  218 | `		}` |
|    3 |  219 | `		ph7_value_int64(pValue,sec);` |
|    3 |  220 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    3 |  221 | `		ph7_value_int64(pValue,nsec);` |
|    3 |  222 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    3 |  223 | `		ph7_result_value(pCtx,pArray);` |
|    3 |  224 | `		ph7_context_release_value(pCtx,pValue);` |
|    3 |  225 | `		ph7_context_release_value(pCtx,pArray);` |
|    - |  226 | `	}` |
|    9 |  227 | `	return PH7_OK;` |
|    5 |  228 | `}` |
|    - |  229 | `/*` |
|    - |  230 | ` * array getdate ([ int $timestamp = time() ])` |
|    - |  231 | ` *  Returns an associative array containing the date information` |
|    - |  232 | ` *  of the timestamp, or the current local time if no timestamp is given.` |
|    - |  233 | ` * Parameter` |
|    - |  234 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|    - |  235 | ` *     that defaults to the current local time if a timestamp is not given.` |
|    - |  236 | ` *     In other words, it defaults to the value of time().` |
|    - |  237 | ` * Returns` |
|    - |  238 | ` *  Returns an associative array of information related to the timestamp.` |
|    - |  239 | ` */` |
|    8 |  240 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  241 | `{` |
|    - |  242 | `	ph7_value *pValue,*pArray;` |
|    - |  243 | `	Sytm sTm;` |
|    9 |  244 | `	if( nArg < 1 ){` |
|    - |  245 | `#ifdef __WINNT__` |
|    - |  246 | `		SYSTEMTIME sOS;` |
|    1 |  247 | `		GetSystemTime(&sOS);` |
|    1 |  248 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  249 | `#else` |
|    - |  250 | `		struct tm *pTm;` |
|    - |  251 | `		time_t t;` |
|    4 |  252 | `		time(&t);` |
|    4 |  253 | `		pTm = gmtime(&t);` |
|    4 |  254 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    4 |  255 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  256 | `#endif` |
|    3 |  257 | `	}else{` |
|    - |  258 | `		/* Use the given timestamp */` |
|    - |  259 | `		time_t t;` |
|    - |  260 | `		struct tm *pTm;` |
|    5 |  261 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    5 |  262 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|    5 |  263 | `			pTm = gmtime(&t);` |
|    5 |  264 | `			if( pTm == 0 ){` |
|  ! 0 |  265 | `				time(&t);` |
|  ! 0 |  266 | `			}` |
|    3 |  267 | `		}else{` |
|  ! 0 |  268 | `			time(&t);` |
|    - |  269 | `		}` |
|    5 |  270 | `		pTm = gmtime(&t);` |
|    5 |  271 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    5 |  272 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  273 | `	}` |
|    - |  274 | `	/* Element value */` |
|    9 |  275 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    9 |  276 | `	if( pValue == 0 ){` |
|    - |  277 | `		/* Return NULL */` |
|  ! 0 |  278 | `		ph7_result_null(pCtx);` |
|  ! 0 |  279 | `		return PH7_OK;` |
|    - |  280 | `	}` |
|    - |  281 | `	/* Create a new array */` |
|    9 |  282 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 |  283 | `	if( pArray == 0 ){` |
|    - |  284 | `		/* Return NULL */` |
|  ! 0 |  285 | `		ph7_result_null(pCtx);` |
|  ! 0 |  286 | `		return PH7_OK;` |
|    - |  287 | `	}` |
|    - |  288 | `	/* Fill the array */` |
|    - |  289 | `	/* Seconds */` |
|    9 |  290 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    9 |  291 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|    - |  292 | `	/* Minutes */` |
|    9 |  293 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    9 |  294 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|    - |  295 | `	/* Hours */` |
|    9 |  296 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    9 |  297 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|    - |  298 | `	/* mday */` |
|    9 |  299 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    9 |  300 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|    - |  301 | `	/* wday */` |
|    9 |  302 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    9 |  303 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|    - |  304 | `	/* mon */` |
|    9 |  305 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|    9 |  306 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|    - |  307 | `	/* year */` |
|    9 |  308 | `	ph7_value_int(pValue,sTm.tm_year);` |
|    9 |  309 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|    - |  310 | `	/* yday */` |
|    9 |  311 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    9 |  312 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|    - |  313 | `	/* Weekday [i.e: Monday,Tuesday,...] */` |
|    9 |  314 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|    9 |  315 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|    - |  316 | `	/* Reset the string cursor */` |
|    9 |  317 | `	ph7_value_reset_string_cursor(pValue);` |
|    - |  318 | `	/* Month [i.e: January,February,...] */` |
|    9 |  319 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|    9 |  320 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|    - |  321 | `	/* Return the freshly created array */` |
|    9 |  322 | `	ph7_result_value(pCtx,pArray);` |
|    9 |  323 | `	return PH7_OK;` |
|    5 |  324 | `}` |
|    - |  325 | `/*` |
|    - |  326 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|    - |  327 | ` *  Returns an associative array containing the data returned from the system call.` |
|    - |  328 | ` * Parameters` |
|    - |  329 | ` *  $return_float` |
|    - |  330 | ` *   When set to TRUE, a float instead of an array is returned.` |
|    - |  331 | ` * Return` |
|    - |  332 | ` *  By default an array is returned. If return_float is set, then` |
|    - |  333 | ` *  a float is returned.` |
|    - |  334 | ` */` |
|    8 |  335 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  336 | `{` |
|    9 |  337 | `	int bFloat = 0;` |
|    - |  338 | `	sytime sTime;` |
|    9 |  339 | `	DateNow(pCtx->pVm,&sTime);` |
|    9 |  340 | `	if( nArg > 0 ){` |
|    7 |  341 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|    3 |  342 | `	}` |
|    9 |  343 | `	if( bFloat ){` |
|    - |  344 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|    5 |  345 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|    3 |  346 | `	}else{` |
|    - |  347 | `		/* Return an associative array */` |
|    - |  348 | `		ph7_value *pValue,*pArray;` |
|    - |  349 | `		/* Create a new array */` |
|    5 |  350 | `		pArray = ph7_context_new_array(pCtx);` |
|    - |  351 | `		/* Element value */` |
|    5 |  352 | `		pValue = ph7_context_new_scalar(pCtx);` |
|    5 |  353 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    - |  354 | `			/* Return NULL */` |
|  ! 0 |  355 | `			ph7_result_null(pCtx);` |
|  ! 0 |  356 | `			return PH7_OK;` |
|    - |  357 | `		}` |
|    - |  358 | `		/* Fill the array */` |
|    - |  359 | `		/* sec */` |
|    5 |  360 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|    5 |  361 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|    - |  362 | `		/* usec */` |
|    5 |  363 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|    5 |  364 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|    - |  365 | `		/* Return the array */` |
|    5 |  366 | `		ph7_result_value(pCtx,pArray);` |
|    - |  367 | `	}` |
|    9 |  368 | `	return PH7_OK;` |
|    5 |  369 | `}` |
|    - |  370 | `/* Check if the given year is leap or not */` |
|    - |  371 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|    - |  372 | `/* ISO-8601 numeric representation of the day of the week */` |
|    - |  373 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|    - |  374 | `/*` |
|    - |  375 | ` * Format a given date string.` |
|    - |  376 | ` * Supported format: (Taken from PHP online docs)` |
|    - |  377 | ` * character 	Description` |
|    - |  378 | ` * d          Day of the month, 2 digits with leading zeros` |
|    - |  379 | ` * D          A textual representation of a day, three letters` |
|    - |  380 | ` * j          Day of the month without leading zeros` |
|    - |  381 | ` * l          A full textual representation of the day of the week` |
|    - |  382 | ` * N          ISO-8601 numeric representation of the day of the week` |
|    - |  383 | ` * w          Numeric representation of the day of the week` |
|    - |  384 | ` * z          The day of the year (starting from 0)` |
|    - |  385 | ` * F          A full textual representation of a month, such as January or March` |
|    - |  386 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|    - |  387 | ` * M          A short textual representation of a month, three letters` |
|    - |  388 | ` * n          Numeric representation of a month, without leading zeros` |
|    - |  389 | ` * t          Number of days in the given month` |
|    - |  390 | ` * L          Whether it's a leap year` |
|    - |  391 | ` * o          ISO-8601 year number. This has the same value as Y` |
|    - |  392 | ` * Y          A full numeric representation of a year, 4 digits` |
|    - |  393 | ` * y          A two digit representation of a year` |
|    - |  394 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|    - |  395 | ` * A          Uppercase Ante meridiem and Post meridiem` |
|    - |  396 | ` * g          12-hour format of an hour without leading zeros` |
|    - |  397 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|    - |  398 | ` * h          12-hour format of an hour with leading zeros` |
|    - |  399 | ` * H          24-hour format of an hour with leading zeros` |
|    - |  400 | ` * i          Minutes with leading zeros` |
|    - |  401 | ` * s          Seconds, with leading zeros` |
|    - |  402 | ` * u          Microseconds` |
|    - |  403 | ` * e          Timezone identifier` |
|    - |  404 | ` * I          Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|    - |  405 | ` * r          RFC 2822 formatted date` |
|    - |  406 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|    - |  407 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|    - |  408 | ` * O          Difference to Greenwich time (GMT) in hours` |
|    - |  409 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|    - |  410 | ` *            east of UTC is always positive.` |
|    - |  411 | ` * c         ISO 8601 date` |
|    - |  412 | ` */` |
|  410 |  413 | `PH7_PRIVATE sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec)` |
|    1 |  414 | `{` |
|  411 |  415 | `	const char *zEnd = &zIn[nLen];` |
|    - |  416 | `	const char *zCur;` |
|    - |  417 | `	/* Start the format process */` |
| 1309 |  418 | `	for(;;){` |
| 2619 |  419 | `		if( zIn >= zEnd ){` |
|    - |  420 | `			/* No more input to process */` |
|  411 |  421 | `			break;` |
|    - |  422 | `		}` |
| 2209 |  423 | `		switch(zIn[0]){` |
|  107 |  424 | `		case 'd':` |
|    - |  425 | `			/* Day of the month, 2 digits with leading zeros */` |
|  215 |  426 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|  215 |  427 | `			break;` |
|   32 |  428 | `		case 'D':` |
|    - |  429 | `			/*A textual representation of a day, three letters*/` |
|   65 |  430 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|   65 |  431 | `			ph7_result_string(pCtx,zCur,3);` |
|   65 |  432 | `			break;` |
|    4 |  433 | `		case 'j':` |
|    - |  434 | `			/*	Day of the month without leading zeros */` |
|    9 |  435 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    9 |  436 | `			break;` |
|    3 |  437 | `		case 'l':` |
|    - |  438 | `			/* A full textual representation of the day of the week */` |
|    7 |  439 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    7 |  440 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|    7 |  441 | `			break;` |
|    1 |  442 | `		case 'N':{` |
|    - |  443 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    3 |  444 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    3 |  445 | `			break;` |
|    - |  446 | `				 }` |
|    1 |  447 | `		case 'w':` |
|    - |  448 | `			/*Numeric representation of the day of the week*/` |
|    3 |  449 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    3 |  450 | `			break;` |
|    1 |  451 | `		case 'z':` |
|    - |  452 | `			/*The day of the year*/` |
|    3 |  453 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|    3 |  454 | `			break;` |
|    3 |  455 | `		case 'F':` |
|    - |  456 | `			/*A full textual representation of a month, such as January or March*/` |
|    7 |  457 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    7 |  458 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|    7 |  459 | `			break;` |
|  107 |  460 | `		case 'm':` |
|    - |  461 | `			/*Numeric representation of a month, with leading zeros*/` |
|  215 |  462 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|  215 |  463 | `			break;` |
|    1 |  464 | `		case 'M':` |
|    - |  465 | `			/*A short textual representation of a month, three letters*/` |
|    3 |  466 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    3 |  467 | `			ph7_result_string(pCtx,zCur,3);` |
|    3 |  468 | `			break;` |
|    4 |  469 | `		case 'n':` |
|    - |  470 | `			/*Numeric representation of a month, without leading zeros*/` |
|    9 |  471 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    9 |  472 | `			break;` |
|    1 |  473 | `		case 't':{` |
|    - |  474 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    3 |  475 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|    3 |  476 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|  ! 0 |  477 | `				nDays = 28;` |
|  ! 0 |  478 | `			}` |
|    - |  479 | `			/*Number of days in the given month*/` |
|    3 |  480 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|    3 |  481 | `			break;` |
|    - |  482 | `				 }` |
|    1 |  483 | `		case 'L':{` |
|    3 |  484 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|    - |  485 | `			/* Whether it's a leap year */` |
|    3 |  486 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|    3 |  487 | `			break;` |
|    - |  488 | `				 }` |
|    7 |  489 | `		case 'o': case 'W': {` |
|    - |  490 | `			/* ISO-8601 week-numbering year / week number: both belong to the` |
|    - |  491 | `			 * year owning the Thursday of the civil week (php: 2024-12-31 is` |
|    - |  492 | `			 * 2025-W01, 2027-01-01 is 2026-W53). php pads W but not o. */` |
|   15 |  493 | `			sxi64 days = DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday);` |
|   15 |  494 | `			int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|   15 |  495 | `			sxi64 thu = days + (4 - isoDow);` |
|    - |  496 | `			sxi64 wy;` |
|    - |  497 | `			int wm,wd;` |
|   15 |  498 | `			DtCivilFromDays(thu,&wy,&wm,&wd);` |
|   15 |  499 | `			if( zIn[0] == 'o' ){` |
|    9 |  500 | `				ph7_result_string_format(pCtx,"%d",(int)wy);` |
|    5 |  501 | `			}else{` |
|   10 |  502 | `				ph7_result_string_format(pCtx,"%02d",` |
|    6 |  503 | `					(int)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1);` |
|    - |  504 | `			}` |
|   15 |  505 | `			break;` |
|    - |  506 | `				 }` |
|   97 |  507 | `		case 'Y':` |
|    - |  508 | `			/*	A full numeric representation of a year, 4 digits */` |
|  195 |  509 | `			ph7_result_string_format(pCtx,"%04d",pTm->tm_year);` |
|  195 |  510 | `			break;` |
|    2 |  511 | `		case 'X':` |
|    - |  512 | `			/* Expanded full year, always signed (php 8.2+): +2024 */` |
|    5 |  513 | `			ph7_result_string_format(pCtx,"%c%04d",` |
|    4 |  514 | `				pTm->tm_year < 0 ? '-' : '+',` |
|    4 |  515 | `				pTm->tm_year < 0 ? -pTm->tm_year : pTm->tm_year);` |
|    5 |  516 | `			break;` |
|    2 |  517 | `		case 'x':` |
|    - |  518 | `			/* Expanded year, signed only past 4 digits (php 8.2+) */` |
|    5 |  519 | `			if( pTm->tm_year > 9999 ){` |
|  ! 0 |  520 | `				ph7_result_string_format(pCtx,"+%d",pTm->tm_year);` |
|  ! 0 |  521 | `			}else{` |
|    5 |  522 | `				ph7_result_string_format(pCtx,"%04d",pTm->tm_year);` |
|    - |  523 | `			}` |
|    5 |  524 | `			break;` |
|    2 |  525 | `		case 'y':` |
|    - |  526 | `			/*A two digit representation of a year*/` |
|    5 |  527 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|    5 |  528 | `			break;` |
|    3 |  529 | `		case 'a':` |
|    - |  530 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    7 |  531 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "pm" : "am",2);` |
|    7 |  532 | `			break;` |
|    3 |  533 | `		case 'A':` |
|    - |  534 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    7 |  535 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "PM" : "AM",2);` |
|    7 |  536 | `			break;` |
|    1 |  537 | `		case 'B':{` |
|    - |  538 | `			/* Swatch Internet time: thousandths of the UTC+1 day */` |
|    4 |  539 | `			sxi64 iUtc = DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400` |
|    2 |  540 | `				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec` |
|    2 |  541 | `				- (sxi64)pTm->tm_gmtoff;` |
|    3 |  542 | `			sxi64 iBie = (iUtc + 3600) % 86400;` |
|    3 |  543 | `			if( iBie < 0 ){` |
|  ! 0 |  544 | `				iBie += 86400;` |
|  ! 0 |  545 | `			}` |
|    3 |  546 | `			ph7_result_string_format(pCtx,"%03d",(int)(iBie * 1000 / 86400));` |
|    3 |  547 | `			break;` |
|    - |  548 | `				 }` |
|    3 |  549 | `		case 'g':` |
|    - |  550 | `			/*	12-hour format of an hour without leading zeros*/` |
|   10 |  551 | `			ph7_result_string_format(pCtx,"%d",` |
|    6 |  552 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|    7 |  553 | `			break;` |
|    2 |  554 | `		case 'G':` |
|    - |  555 | `			/* 24-hour format of an hour without leading zeros */` |
|    5 |  556 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    5 |  557 | `			break;` |
|    3 |  558 | `		case 'h':` |
|    - |  559 | `			/* 12-hour format of an hour with leading zeros */` |
|   10 |  560 | `			ph7_result_string_format(pCtx,"%02d",` |
|    6 |  561 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|    7 |  562 | `			break;` |
|   67 |  563 | `		case 'H':` |
|    - |  564 | `			/*	24-hour format of an hour with leading zeros */` |
|  135 |  565 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|  135 |  566 | `			break;` |
|   68 |  567 | `		case 'i':` |
|    - |  568 | `			/* 	Minutes with leading zeros */` |
|  137 |  569 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|  137 |  570 | `			break;` |
|   70 |  571 | `		case 's':` |
|    - |  572 | `			/* 	second with leading zeros */` |
|  141 |  573 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|  141 |  574 | `			break;` |
|   13 |  575 | `		case 'u':` |
|    - |  576 | `			/* 	Microseconds. date()/gmdate() have no sub-second part (uSec == 0);` |
|    - |  577 | `			 * 	DateTime::format passes its stored microseconds. */` |
|   27 |  578 | `			ph7_result_string_format(pCtx,"%06d",uSec);` |
|   27 |  579 | `			break;` |
|    4 |  580 | `		case 'v':` |
|    - |  581 | `			/* 	Milliseconds */` |
|    9 |  582 | `			ph7_result_string_format(pCtx,"%03d",uSec/1000);` |
|    9 |  583 | `			break;` |
|    1 |  584 | `		case 'S':{` |
|    - |  585 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|    - |  586 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|    3 |  587 | `			int v = pTm->tm_mday;` |
|    3 |  588 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|    3 |  589 | `			break;` |
|    - |  590 | `				 }` |
|    9 |  591 | `		case 'e':` |
|    - |  592 | `			/* 	Timezone identifier */` |
|   19 |  593 | `			zCur = pTm->tm_zone;` |
|   19 |  594 | `			if( zCur == 0 ){` |
|    - |  595 | `				/* date()-family fills: the script default timezone */` |
|    7 |  596 | `				zCur = pCtx->pVm->zDefTz;` |
|    3 |  597 | `			}` |
|   19 |  598 | `			ph7_result_string(pCtx,zCur,-1);` |
|   19 |  599 | `			break;` |
|    4 |  600 | `		case 'T':{` |
|    - |  601 | `			/* Timezone abbreviation: "UTC" for offset 0, "GMT+0530" for a` |
|    - |  602 | `			 * fixed offset (php's shape). PHL has no tz database, so the` |
|    - |  603 | `			 * zone-name path only ever sees UTC/GMT, uppercased. */` |
|    - |  604 | `			const char *z;` |
|    9 |  605 | `			if( pTm->tm_gmtoff != 0 ){` |
|    3 |  606 | `				long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    3 |  607 | `				ph7_result_string_format(pCtx,"GMT%c%02d%02d",` |
|    2 |  608 | `					pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    3 |  609 | `				break;` |
|    - |  610 | `			}` |
|    7 |  611 | `			z = pTm->tm_zone ? pTm->tm_zone : pCtx->pVm->zDefTz;` |
|   25 |  612 | `			while( *z ){` |
|   19 |  613 | `				int c = (unsigned char)*z;` |
|   19 |  614 | `				if( c >= 'a' && c <= 'z' ){` |
|  ! 0 |  615 | `					c -= 'a' - 'A';` |
|  ! 0 |  616 | `				}` |
|   19 |  617 | `				ph7_result_string_format(pCtx,"%c",c);` |
|   19 |  618 | `				z++;` |
|    1 |  619 | `			}` |
|    7 |  620 | `			break;` |
|    - |  621 | `				 }` |
|    1 |  622 | `		case 'I':` |
|    - |  623 | `			/* Whether or not the date is in daylight saving time. Use the` |
|    - |  624 | `			 * broken-down time's own tm_isdst (as every other platform does):` |
|    - |  625 | `			 * the old Windows _get_daylight() override reported whether the` |
|    - |  626 | `			 * timezone observes DST at all, not whether THIS date is in it. */` |
|    3 |  627 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    3 |  628 | `			break;` |
|    2 |  629 | `		case 'r':{` |
|    - |  630 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200 */` |
|    5 |  631 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    5 |  632 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d %c%02d%02d",` |
|    2 |  633 | `				SyTimeGetDay(pTm->tm_wday),` |
|    2 |  634 | `				pTm->tm_mday,` |
|    2 |  635 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    2 |  636 | `				pTm->tm_year,` |
|    2 |  637 | `				pTm->tm_hour,` |
|    2 |  638 | `				pTm->tm_min,` |
|    2 |  639 | `				pTm->tm_sec,` |
|    4 |  640 | `				pTm->tm_gmtoff < 0 ? '-' : '+',` |
|    4 |  641 | `				(int)(a / 3600),(int)((a % 3600) / 60)` |
|    - |  642 | `				);` |
|    5 |  643 | `			break;` |
|    - |  644 | `				 }` |
|    4 |  645 | `		case 'U':` |
|    - |  646 | `			/* Seconds since the Unix Epoch FOR THIS Sytm (php: the timestamp` |
|    - |  647 | `			 * being formatted — pre-fix this printed time(0) regardless of the` |
|    - |  648 | `			 * date under format). */` |
|   13 |  649 | `			ph7_result_string_format(pCtx,"%qd",` |
|    8 |  650 | `				DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400` |
|    8 |  651 | `				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec` |
|    8 |  652 | `				- (sxi64)pTm->tm_gmtoff);` |
|    9 |  653 | `			break;` |
|    3 |  654 | `		case 'O':{` |
|    - |  655 | `			/* Difference to GMT without colon: +0530 (php) */` |
|    7 |  656 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    7 |  657 | `			ph7_result_string_format(pCtx,"%c%02d%02d",` |
|    6 |  658 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    7 |  659 | `			break;` |
|    - |  660 | `				 }` |
|    5 |  661 | `		case 'P':{` |
|    - |  662 | `			/* Difference to GMT with colon: +05:30 (php) */` |
|   11 |  663 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   11 |  664 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|   10 |  665 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|   11 |  666 | `			break;` |
|    - |  667 | `				 }` |
|    2 |  668 | `		case 'p':{` |
|    - |  669 | `			/* Like P, but "Z" for UTC (php 8.0+) */` |
|    - |  670 | `			long a;` |
|    5 |  671 | `			if( pTm->tm_gmtoff == 0 ){` |
|    3 |  672 | `				ph7_result_string(pCtx,"Z",1);` |
|    3 |  673 | `				break;` |
|    - |  674 | `			}` |
|    3 |  675 | `			a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    3 |  676 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|    2 |  677 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    3 |  678 | `			break;` |
|    - |  679 | `				 }` |
|    2 |  680 | `		case 'Z':` |
|    - |  681 | `			/* Timezone offset in seconds, plain integer (php) */` |
|    5 |  682 | `			ph7_result_string_format(pCtx,"%d",(int)pTm->tm_gmtoff);` |
|    5 |  683 | `			break;` |
|    6 |  684 | `		case 'c':{` |
|    - |  685 | `			/* 	ISO 8601 date: 2004-02-12T15:19:21+00:00 (php) */` |
|   13 |  686 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   19 |  687 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",` |
|    6 |  688 | `				pTm->tm_year,` |
|   12 |  689 | `				pTm->tm_mon+1,` |
|    6 |  690 | `				pTm->tm_mday,` |
|    6 |  691 | `				pTm->tm_hour,` |
|    6 |  692 | `				pTm->tm_min,` |
|    6 |  693 | `				pTm->tm_sec,` |
|   12 |  694 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60)` |
|    - |  695 | `				);` |
|   13 |  696 | `			break;` |
|    - |  697 | `				 }` |
|    4 |  698 | `		case '\\':` |
|    9 |  699 | `			zIn++;` |
|    - |  700 | `			/* Expand verbatim */` |
|    9 |  701 | `			if( zIn < zEnd ){` |
|    9 |  702 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|    4 |  703 | `			}` |
|    9 |  704 | `			break;` |
|  448 |  705 | `		default:` |
|    - |  706 | `			/* Unknown format specifer,expand verbatim */` |
|  897 |  707 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|  896 |  708 | `			break;` |
|    - |  709 | `		}` |
|    - |  710 | `		/* Point to the next character */` |
| 2209 |  711 | `		zIn++;` |
|    1 |  712 | `	}` |
|  411 |  713 | `	return SXRET_OK;` |
|    1 |  714 | `}` |
|    - |  715 | `/*` |
|    - |  716 | ` * Resolve a date()/gmdate() $timestamp argument under php 8's ?int weak ZPP:` |
|    - |  717 | ` *   - null            -> *pbUseNow = 1 (caller uses the current time)` |
|    - |  718 | ` *   - int/bool/float  -> coerce to a Unix timestamp (float truncates; php's` |
|    - |  719 | ` *                        float->int precision E_DEPRECATED is not emitted, §3.7)` |
|    - |  720 | ` *   - numeric string  -> coerce via php's is_numeric_string grammar` |
|    - |  721 | ` *                        (RangeStrToNumber: " 100 "/"1e3"/".5"/"+5" ok)` |
|    - |  722 | ` *   - anything else (non-numeric string, array, object, resource)` |
|    - |  723 | ` *                     -> catchable TypeError, byte-exact with php.` |
|    - |  724 | ` * Returns PH7_OK with *pbUseNow / *pT set, or the PH7_VmThrowException status.` |
|    - |  725 | ` */` |
|  194 |  726 | `static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)` |
|    1 |  727 | `{` |
|    - |  728 | `	char zBuf[64];` |
|  195 |  729 | `	*pbUseNow = 0;` |
|  195 |  730 | `	if( ph7_value_is_null(pArg) ){` |
|    3 |  731 | `		*pbUseNow = 1;` |
|    3 |  732 | `		return PH7_OK;` |
|    - |  733 | `	}` |
|  193 |  734 | `	if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_float(pArg) ){` |
|  175 |  735 | `		*pT = (time_t)ph7_value_to_int64(pArg);` |
|  175 |  736 | `		return PH7_OK;` |
|    - |  737 | `	}` |
|   19 |  738 | `	if( ph7_value_is_string(pArg) ){` |
|    - |  739 | `		int nStr;` |
|   19 |  740 | `		const char *zStr = ph7_value_to_string(pArg,&nStr);` |
|    - |  741 | `		sxi64 iLong; double dReal;` |
|   19 |  742 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|   19 |  743 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|    3 |  744 | `			*pT = (time_t)dReal;` |
|    6 |  745 | `			return PH7_OK;` |
|    - |  746 | `		}` |
|   17 |  747 | `		if( iKind == RANGE_IN_LONG ){` |
|    7 |  748 | `			*pT = (time_t)iLong;` |
|    7 |  749 | `			return PH7_OK;` |
|    - |  750 | `		}` |
|    - |  751 | `		/* Not a numeric string: fall through to the TypeError. */` |
|    5 |  752 | `	}` |
|   16 |  753 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  754 | `		"%s(): Argument #2 ($timestamp) must be of type ?int, %s given",` |
|    5 |  755 | `		ph7_function_name(pCtx),VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|   98 |  756 | `}` |
|    - |  757 | `/*` |
|    - |  758 | ` * string date(string $format [, int $timestamp = time() ] )` |
|    - |  759 | ` *  Returns a string formatted according to the given format string using` |
|    - |  760 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|    - |  761 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|    - |  762 | ` * Parameters` |
|    - |  763 | ` *  $format` |
|    - |  764 | ` *   The format of the outputted date string (See code above)` |
|    - |  765 | ` * $timestamp` |
|    - |  766 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  767 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  768 | ` *   In other words, it defaults to the value of time().` |
|    - |  769 | ` * Return` |
|    - |  770 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  771 | ` */` |
|  136 |  772 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  773 | `{` |
|    - |  774 | `	const char *zFormat;` |
|    - |  775 | `	int nLen;` |
|    - |  776 | `	Sytm sTm;` |
|  137 |  777 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  778 | `		/* Missing/Invalid argument,return FALSE */` |
|  ! 0 |  779 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  780 | `		return PH7_OK;` |
|    - |  781 | `	}` |
|  137 |  782 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|  137 |  783 | `	if( nLen < 1 ){` |
|    - |  784 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  785 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  786 | `	}` |
|  137 |  787 | `	if( nArg < 2 ){` |
|    - |  788 | `#ifdef __WINNT__` |
|    - |  789 | `		SYSTEMTIME sOS;` |
|    1 |  790 | `		GetSystemTime(&sOS);` |
|    1 |  791 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  792 | `#else` |
|    - |  793 | `		struct tm *pTm;` |
|    - |  794 | `		time_t t;` |
|   30 |  795 | `		time(&t);` |
|   30 |  796 | `		pTm = gmtime(&t);` |
|   30 |  797 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|   30 |  798 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  799 | `#endif` |
|   16 |  800 | `	}else{` |
|    - |  801 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|  107 |  802 | `		time_t t = 0;` |
|    - |  803 | `		struct tm *pTm;` |
|    - |  804 | `		int bUseNow;` |
|  107 |  805 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|  107 |  806 | `		if( rc != PH7_OK ){` |
|    9 |  807 | `			return rc;` |
|    - |  808 | `		}` |
|   99 |  809 | `		if( bUseNow ){` |
|  ! 0 |  810 | `			time(&t);` |
|  ! 0 |  811 | `		}` |
|   99 |  812 | `		pTm = gmtime(&t);` |
|   99 |  813 | `		if( pTm == 0 ){` |
|  ! 0 |  814 | `			time(&t);` |
|  ! 0 |  815 | `			pTm = gmtime(&t);` |
|  ! 0 |  816 | `		}` |
|   99 |  817 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|   99 |  818 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  819 | `	}` |
|    - |  820 | `	/* Format the given string */` |
|  129 |  821 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|  129 |  822 | `	return PH7_OK;` |
|   69 |  823 | `}` |
|    - |  824 | `/*` |
|    - |  825 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|    - |  826 | ` *  Identical to the date() function except that the time returned` |
|    - |  827 | ` *  is Greenwich Mean Time (GMT).` |
|    - |  828 | ` * Parameters` |
|    - |  829 | ` *  $format` |
|    - |  830 | ` *  The format of the outputted date string (See code above)` |
|    - |  831 | ` *  $timestamp` |
|    - |  832 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  833 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  834 | ` *   In other words, it defaults to the value of time().` |
|    - |  835 | ` * Return` |
|    - |  836 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  837 | ` */` |
|  102 |  838 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  839 | `{` |
|    - |  840 | `	const char *zFormat;` |
|    - |  841 | `	int nLen;` |
|    - |  842 | `	Sytm sTm;` |
|  103 |  843 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  844 | `		/* Missing/Invalid argument,return FALSE */` |
|  ! 0 |  845 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  846 | `		return PH7_OK;` |
|    - |  847 | `	}` |
|  103 |  848 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|  103 |  849 | `	if( nLen < 1 ){` |
|    - |  850 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  851 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  852 | `	}` |
|  103 |  853 | `	if( nArg < 2 ){` |
|    - |  854 | `#ifdef __WINNT__` |
|    - |  855 | `		SYSTEMTIME sOS;` |
|    1 |  856 | `		GetSystemTime(&sOS);` |
|    1 |  857 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  858 | `#else` |
|    - |  859 | `		struct tm *pTm;` |
|    - |  860 | `		time_t t;` |
|   14 |  861 | `		time(&t);` |
|   14 |  862 | `		pTm = gmtime(&t);` |
|   14 |  863 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|   14 |  864 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  865 | `#endif` |
|    8 |  866 | `	}else{` |
|    - |  867 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|   89 |  868 | `		time_t t = 0;` |
|    - |  869 | `		struct tm *pTm;` |
|    - |  870 | `		int bUseNow;` |
|   89 |  871 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|   89 |  872 | `		if( rc != PH7_OK ){` |
|    3 |  873 | `			return rc;` |
|    - |  874 | `		}` |
|   87 |  875 | `		if( bUseNow ){` |
|    3 |  876 | `			time(&t);` |
|    1 |  877 | `		}` |
|   87 |  878 | `		pTm = gmtime(&t);` |
|   87 |  879 | `		if( pTm == 0 ){` |
|  ! 0 |  880 | `			time(&t);` |
|  ! 0 |  881 | `			pTm = gmtime(&t);` |
|  ! 0 |  882 | `		}` |
|   87 |  883 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|   87 |  884 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  885 | `	}` |
|    - |  886 | `	/* Format the given string */` |
|  101 |  887 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|  101 |  888 | `	return PH7_OK;` |
|   52 |  889 | `}` |
|    - |  890 | `/*` |
|    - |  891 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|    - |  892 | ` *  Return the local time.` |
|    - |  893 | ` * Parameter` |
|    - |  894 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|    - |  895 | ` *     that defaults to the current local time if a timestamp is not given.` |
|    - |  896 | ` *     In other words, it defaults to the value of time().` |
|    - |  897 | ` * $is_associative` |
|    - |  898 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|    - |  899 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|    - |  900 | ` *   array containing all the different elements of the structure returned by the C function` |
|    - |  901 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|    - |  902 | ` *      "tm_sec" - seconds, 0 to 59` |
|    - |  903 | ` *      "tm_min" - minutes, 0 to 59` |
|    - |  904 | ` *      "tm_hour" - hours, 0 to 23` |
|    - |  905 | ` *      "tm_mday" - day of the month, 1 to 31` |
|    - |  906 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|    - |  907 | ` *      "tm_year" - years since 1900` |
|    - |  908 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|    - |  909 | ` *      "tm_yday" - day of the year, 0 to 365` |
|    - |  910 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|    - |  911 | ` * Returns` |
|    - |  912 | ` *  An associative array of information related to the timestamp.` |
|    - |  913 | ` */` |
|    8 |  914 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  915 | `{` |
|    - |  916 | `	ph7_value *pValue,*pArray;` |
|    9 |  917 | `	int isAssoc = 0;` |
|    - |  918 | `	Sytm sTm;` |
|    9 |  919 | `	if( nArg < 1 ){` |
|    - |  920 | `#ifdef __WINNT__` |
|    - |  921 | `		SYSTEMTIME sOS;` |
|    1 |  922 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|    1 |  923 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  924 | `#else` |
|    - |  925 | `		struct tm *pTm;` |
|    - |  926 | `		time_t t;` |
|    4 |  927 | `		time(&t);` |
|    4 |  928 | `		pTm = gmtime(&t);` |
|    4 |  929 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    4 |  930 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  931 | `#endif` |
|    3 |  932 | `	}else{` |
|    - |  933 | `		/* Use the given timestamp */` |
|    - |  934 | `		time_t t;` |
|    - |  935 | `		struct tm *pTm;` |
|    5 |  936 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    5 |  937 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|    5 |  938 | `			pTm = gmtime(&t);` |
|    5 |  939 | `			if( pTm == 0 ){` |
|  ! 0 |  940 | `				time(&t);` |
|  ! 0 |  941 | `			}` |
|    3 |  942 | `		}else{` |
|  ! 0 |  943 | `			time(&t);` |
|    - |  944 | `		}` |
|    5 |  945 | `		pTm = gmtime(&t);` |
|    5 |  946 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    5 |  947 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  948 | `	}` |
|    - |  949 | `	/* Element value */` |
|    9 |  950 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    9 |  951 | `	if( pValue == 0 ){` |
|    - |  952 | `		/* Return NULL */` |
|  ! 0 |  953 | `		ph7_result_null(pCtx);` |
|  ! 0 |  954 | `		return PH7_OK;` |
|    - |  955 | `	}` |
|    - |  956 | `	/* Create a new array */` |
|    9 |  957 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 |  958 | `	if( pArray == 0 ){` |
|    - |  959 | `		/* Return NULL */` |
|  ! 0 |  960 | `		ph7_result_null(pCtx);` |
|  ! 0 |  961 | `		return PH7_OK;` |
|    - |  962 | `	}` |
|    9 |  963 | `	if( nArg > 1 ){` |
|    3 |  964 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|    1 |  965 | `	}` |
|    - |  966 | `	/* Fill the array */` |
|    - |  967 | `	/* Seconds */` |
|    9 |  968 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    9 |  969 | `	if( isAssoc ){` |
|    3 |  970 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|    2 |  971 | `	}else{` |
|    7 |  972 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - |  973 | `	}` |
|    - |  974 | `	/* Minutes */` |
|    9 |  975 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    9 |  976 | `	if( isAssoc ){` |
|    3 |  977 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|    2 |  978 | `	}else{` |
|    7 |  979 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - |  980 | `	}` |
|    - |  981 | `	/* Hours */` |
|    9 |  982 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    9 |  983 | `	if( isAssoc ){` |
|    3 |  984 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|    2 |  985 | `	}else{` |
|    7 |  986 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - |  987 | `	}` |
|    - |  988 | `	/* mday */` |
|    9 |  989 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    9 |  990 | `	if( isAssoc ){` |
|    3 |  991 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|    2 |  992 | `	}else{` |
|    7 |  993 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - |  994 | `	}` |
|    - |  995 | `	/* mon */` |
|    9 |  996 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|    9 |  997 | `	if( isAssoc ){` |
|    3 |  998 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|    2 |  999 | `	}else{` |
|    7 | 1000 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1001 | `	}` |
|    - | 1002 | `	/* year since 1900 */` |
|    9 | 1003 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|    9 | 1004 | `	if( isAssoc ){` |
|    3 | 1005 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|    2 | 1006 | `	}else{` |
|    7 | 1007 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1008 | `	}` |
|    - | 1009 | `	/* wday */` |
|    9 | 1010 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    9 | 1011 | `	if( isAssoc ){` |
|    3 | 1012 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|    2 | 1013 | `	}else{` |
|    7 | 1014 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1015 | `	}` |
|    - | 1016 | `	/* yday */` |
|    9 | 1017 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    9 | 1018 | `	if( isAssoc ){` |
|    3 | 1019 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|    2 | 1020 | `	}else{` |
|    7 | 1021 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1022 | `	}` |
|    - | 1023 | `	/* isdst */` |
|    - | 1024 | `#ifdef __WINNT__` |
|    - | 1025 | `#ifdef _MSC_VER` |
|    - | 1026 | `#ifndef _WIN32_WCE` |
|    1 | 1027 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1028 | `#endif` |
|    - | 1029 | `#endif` |
|    - | 1030 | `#endif` |
|    9 | 1031 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|    9 | 1032 | `	if( isAssoc ){` |
|    3 | 1033 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|    2 | 1034 | `	}else{` |
|    7 | 1035 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1036 | `	}` |
|    - | 1037 | `	/* Return the array */` |
|    9 | 1038 | `	ph7_result_value(pCtx,pArray);` |
|    9 | 1039 | `	return PH7_OK;` |
|    5 | 1040 | `}` |
|    - | 1041 | `/*` |
|    - | 1042 | ` * int idate(string $format [, int $timestamp = time() ])` |
|    - | 1043 | ` *  Returns a number formatted according to the given format string` |
|    - | 1044 | ` *  using the given integer timestamp or the current local time if` |
|    - | 1045 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|    - | 1046 | ` *  to the value of time().` |
|    - | 1047 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|    - | 1048 | ` *  parameter.` |
|    - | 1049 | ` * $Parameters` |
|    - | 1050 | ` *  Supported format` |
|    - | 1051 | ` *   d 	Day of the month` |
|    - | 1052 | ` *   h 	Hour (12 hour format)` |
|    - | 1053 | ` *   H 	Hour (24 hour format)` |
|    - | 1054 | ` *   i 	Minutes` |
|    - | 1055 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|    - | 1056 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|    - | 1057 | ` *   m 	Month number` |
|    - | 1058 | ` *   s 	Seconds` |
|    - | 1059 | ` *   t 	Days in current month` |
|    - | 1060 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|    - | 1061 | ` *   w 	Day of the week (0 on Sunday)` |
|    - | 1062 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|    - | 1063 | ` *   y 	Year (1 or 2 digits - check note below)` |
|    - | 1064 | ` *   Y 	Year (4 digits)` |
|    - | 1065 | ` *   z 	Day of the year` |
|    - | 1066 | ` *   Z 	Timezone offset in seconds` |
|    - | 1067 | ` * $timestamp` |
|    - | 1068 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|    - | 1069 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|    - | 1070 | ` *  to the value of time().` |
|    - | 1071 | ` * Return` |
|    - | 1072 | ` *  An integer.` |
|    - | 1073 | ` */` |
|  178 | 1074 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1075 | `{` |
|    - | 1076 | `	const char *zFormat;` |
|  180 | 1077 | `	ph7_int64 iVal = 0;` |
|    - | 1078 | `	int nLen;` |
|    - | 1079 | `	Sytm sTm;` |
|  180 | 1080 | `	time_t t = 0; /* The resolved timestamp; 'U' must report THIS, not time(0) */` |
|  180 | 1081 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1082 | `		/* Missing/Invalid argument,return -1 */` |
|  ! 0 | 1083 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1084 | `		return PH7_OK;` |
|    - | 1085 | `	}` |
|  180 | 1086 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|  180 | 1087 | `	if( nLen < 1 ){` |
|    - | 1088 | `		/* Don't bother processing return -1*/` |
|  ! 0 | 1089 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1090 | `	}` |
|  180 | 1091 | `	if( nArg < 2 ){` |
|    - | 1092 | `#ifdef __WINNT__` |
|    - | 1093 | `		SYSTEMTIME sOS;` |
|    2 | 1094 | `		GetSystemTime(&sOS);` |
|    2 | 1095 | `		time(&t);` |
|    2 | 1096 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - | 1097 | `#else` |
|    - | 1098 | `		struct tm *pTm;` |
|   14 | 1099 | `		time(&t);` |
|   14 | 1100 | `		pTm = gmtime(&t);` |
|   14 | 1101 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|   14 | 1102 | `		DtSytmFillOffset(&sTm,t);` |
|    - | 1103 | `#endif` |
|    9 | 1104 | `	}else{` |
|    - | 1105 | `		/* Use the given timestamp */` |
|    - | 1106 | `		struct tm *pTm;` |
|  165 | 1107 | `		if( ph7_value_is_int(apArg[1]) ){` |
|  165 | 1108 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|  165 | 1109 | `			pTm = gmtime(&t);` |
|  165 | 1110 | `			if( pTm == 0 ){` |
|  ! 0 | 1111 | `				time(&t);` |
|  ! 0 | 1112 | `			}` |
|   83 | 1113 | `		}else{` |
|  ! 0 | 1114 | `			time(&t);` |
|    - | 1115 | `		}` |
|  165 | 1116 | `		pTm = gmtime(&t);` |
|  165 | 1117 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|  165 | 1118 | `		DtSytmFillOffset(&sTm,t);` |
|    - | 1119 | `	}` |
|    - | 1120 | `	/* Perform the requested operation */` |
|  180 | 1121 | `	switch(zFormat[0]){` |
|    9 | 1122 | `	case 'd':` |
|    - | 1123 | `	case 'j':` |
|    - | 1124 | `		/* Day of the month ('j' differs from 'd' only in zero padding, which an` |
|    - | 1125 | `		 * integer result cannot carry) */` |
|   19 | 1126 | `		iVal = sTm.tm_mday;` |
|   19 | 1127 | `		break;` |
|    8 | 1128 | `	case 'h':` |
|    - | 1129 | `	case 'g':` |
|    - | 1130 | `		/* Hour (12 hour format): php reports midnight and noon as 12, not 0 —` |
|    - | 1131 | ``		 * `1 + hour % 12` answered 1 for both. */`` |
|   17 | 1132 | `		iVal = sTm.tm_hour % 12;` |
|   17 | 1133 | `		if( iVal == 0 ){` |
|   17 | 1134 | `			iVal = 12;` |
|    8 | 1135 | `		}` |
|   17 | 1136 | `		break;` |
|    9 | 1137 | `	case 'H':` |
|    - | 1138 | `	case 'G':` |
|    - | 1139 | `		/* Hour (24 hour format) */` |
|   19 | 1140 | `		iVal = sTm.tm_hour;` |
|   19 | 1141 | `		break;` |
|    4 | 1142 | `	case 'B': {` |
|    - | 1143 | `		/* Swatch Internet time: 1000 "beats" per day in UTC+1, no fractions.` |
|    - | 1144 | `		 * Integer math throughout so the tiny build (no floating point) agrees. */` |
|    9 | 1145 | `		ph7_int64 iSec = ((ph7_int64)t + 3600) % 86400;` |
|    9 | 1146 | `		if( iSec < 0 ){` |
|  ! 0 | 1147 | `			iSec += 86400;` |
|  ! 0 | 1148 | `		}` |
|    9 | 1149 | `		iVal = iSec * 1000 / 86400;` |
|    9 | 1150 | `		break;` |
|    - | 1151 | `			  }` |
|    5 | 1152 | `	case 'i':` |
|    - | 1153 | `		/*Minutes*/` |
|   11 | 1154 | `		iVal = sTm.tm_min;` |
|   11 | 1155 | `		break;` |
|  ! 0 | 1156 | `	case 'I':` |
|    - | 1157 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|    - | 1158 | `#ifdef __WINNT__` |
|    - | 1159 | `#ifdef _MSC_VER` |
|    - | 1160 | `#ifndef _WIN32_WCE` |
|  ! 0 | 1161 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1162 | `#endif` |
|    - | 1163 | `#endif` |
|    - | 1164 | `#endif` |
|  ! 0 | 1165 | `		iVal = sTm.tm_isdst;` |
|  ! 0 | 1166 | `		break;` |
|    4 | 1167 | `	case 'L':` |
|    - | 1168 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|    9 | 1169 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|    9 | 1170 | `		break;` |
|    9 | 1171 | `	case 'm':` |
|    - | 1172 | `	case 'n':` |
|    - | 1173 | `		/* Month number. Sytm keeps tm_mon 0-based (see 't' below, which tests` |
|    - | 1174 | ``		 * `tm_mon == 1` for February), so July used to answer 6. */`` |
|   19 | 1175 | `		iVal = sTm.tm_mon + 1;` |
|   19 | 1176 | `		break;` |
|    5 | 1177 | `	case 's':` |
|    - | 1178 | `		/*Seconds*/` |
|   11 | 1179 | `		iVal = sTm.tm_sec;` |
|   11 | 1180 | `		break;` |
|    4 | 1181 | `	case 't':{` |
|    - | 1182 | `		/*Days in current month*/` |
|    - | 1183 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    9 | 1184 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|    9 | 1185 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|  ! 0 | 1186 | `			nDays = 28;` |
|  ! 0 | 1187 | `		}` |
|    9 | 1188 | `		iVal = nDays;` |
|    9 | 1189 | `		break;` |
|    - | 1190 | `			 }` |
|    4 | 1191 | `	case 'U':` |
|    - | 1192 | `		/* Seconds since the Unix Epoch. This used to call time(0), ignoring the` |
|    - | 1193 | `		 * $timestamp argument entirely and always answering "now". */` |
|    9 | 1194 | `		iVal = (ph7_int64)t;` |
|    9 | 1195 | `		break;` |
|    4 | 1196 | `	case 'w':` |
|    - | 1197 | `		/*	Day of the week (0 on Sunday) */` |
|    9 | 1198 | `		iVal = sTm.tm_wday;` |
|    9 | 1199 | `		break;` |
|    8 | 1200 | `	case 'W':` |
|    - | 1201 | `	case 'o': {` |
|    - | 1202 | `		/* ISO-8601 week number / week-numbering year: both belong to the year` |
|    - | 1203 | `		 * owning the Thursday of the civil week, so 2021-01-01 is 2020-W53.` |
|    - | 1204 | `		 * The old code indexed a weekday table and returned a DAY number` |
|    - | 1205 | `		 * (1..7) as if it were a week number — idate("W") answered 4 in the` |
|    - | 1206 | `		 * middle of July. Same derivation as date()'s 'o'/'W' above. */` |
|   17 | 1207 | `		sxi64 days = DtDaysFromCivil((sxi64)sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|   17 | 1208 | `		int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|   17 | 1209 | `		sxi64 thu = days + (4 - isoDow);` |
|    - | 1210 | `		sxi64 wy;` |
|    - | 1211 | `		int wm,wd;` |
|   17 | 1212 | `		DtCivilFromDays(thu,&wy,&wm,&wd);` |
|   17 | 1213 | `		if( zFormat[0] == 'o' ){` |
|    9 | 1214 | `			iVal = (ph7_int64)wy;` |
|    5 | 1215 | `		}else{` |
|    9 | 1216 | `			iVal = (ph7_int64)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1;` |
|    - | 1217 | `		}` |
|   17 | 1218 | `		break;` |
|    - | 1219 | `			  }` |
|    4 | 1220 | `	case 'y':` |
|    - | 1221 | `		/* Year (2 digits) */` |
|    9 | 1222 | `		iVal = sTm.tm_year % 100;` |
|    9 | 1223 | `		break;` |
|    6 | 1224 | `	case 'Y':` |
|    - | 1225 | `		/* Year (4 digits) */` |
|   13 | 1226 | `		iVal = sTm.tm_year;` |
|   13 | 1227 | `		break;` |
|    4 | 1228 | `	case 'z':` |
|    - | 1229 | `		/* Day of the year */` |
|    9 | 1230 | `		iVal = sTm.tm_yday;` |
|    9 | 1231 | `		break;` |
|  ! 0 | 1232 | `	case 'Z':` |
|    - | 1233 | `		/*Timezone offset in seconds*/` |
|  ! 0 | 1234 | `		iVal = sTm.tm_gmtoff;` |
|  ! 0 | 1235 | `		break;` |
|    2 | 1236 | `	default:` |
|    - | 1237 | `		/* unknown format,throw a warning */` |
|    6 | 1238 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unrecognized date format token");` |
|    - | 1239 | `		/* php returns FALSE for an unrecognized token, not 0 — the two are` |
|    - | 1240 | ``		 * distinguishable (`idate($t) === false` is the documented check) and`` |
|    - | 1241 | `		 * 0 is a legitimate result for several real tokens. */` |
|    6 | 1242 | `		ph7_result_bool(pCtx,0);` |
|    6 | 1243 | `		return PH7_OK;` |
|    - | 1244 | `	}` |
|    - | 1245 | `	/* Return the time value */` |
|  175 | 1246 | `	ph7_result_int64(pCtx,iVal);` |
|  175 | 1247 | `	return PH7_OK;` |
|   91 | 1248 | `}` |
|    - | 1249 | `/*` |
|    - | 1250 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|    - | 1251 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|    - | 1252 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|    - | 1253 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|    - | 1254 | ` *  specified.` |
|    - | 1255 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|    - | 1256 | ` *  the current value according to the local date and time.` |
|    - | 1257 | ` * Parameters` |
|    - | 1258 | ` * $hour` |
|    - | 1259 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|    - | 1260 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|    - | 1261 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|    - | 1262 | ` * $minute` |
|    - | 1263 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|    - | 1264 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|    - | 1265 | ` *  in the following hour(s).` |
|    - | 1266 | ` * $second` |
|    - | 1267 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|    - | 1268 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|    - | 1269 | ` * second in the following minute(s).` |
|    - | 1270 | ` * $month` |
|    - | 1271 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|    - | 1272 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|    - | 1273 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|    - | 1274 | ` * $day` |
|    - | 1275 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|    - | 1276 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|    - | 1277 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|    - | 1278 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|    - | 1279 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|    - | 1280 | ` * $year` |
|    - | 1281 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|    - | 1282 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|    - | 1283 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|    - | 1284 | ` * $is_dst` |
|    - | 1285 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|    - | 1286 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|    - | 1287 | ` * Return` |
|    - | 1288 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|    - | 1289 | ` *   If the arguments are invalid, the function returns FALSE` |
|    - | 1290 | ` */` |
|   38 | 1291 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1292 | `{` |
|    - | 1293 | `	const char *zFunction;` |
|    - | 1294 | `	ph7_int64 iVal;` |
|    - | 1295 | `	sxi64 h,mi,s,mo,d,y,yAdj;` |
|    - | 1296 | `	int moN;` |
|    - | 1297 | `	struct tm *pTm;` |
|    - | 1298 | `	time_t t;` |
|    - | 1299 | `	/* Extract function name */` |
|   39 | 1300 | `	zFunction = ph7_function_name(pCtx);` |
|    - | 1301 | `	/* PHP 8 dropped the legacy $is_dst 7th parameter: mktime()/gmmktime() now` |
|    - | 1302 | `	 * accept at most 6 arguments and throw a catchable ArgumentCountError` |
|    - | 1303 | `	 * otherwise (the central aBuiltinArity table only enforces the minimum, so` |
|    - | 1304 | `	 * this maximum is checked here). */` |
|   39 | 1305 | `	if( nArg > 6 ){` |
|  ! 0 | 1306 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|  ! 0 | 1307 | `			"%s() expects at most 6 arguments, %d given",zFunction,nArg);` |
|    - | 1308 | `	}` |
|   39 | 1309 | `	if( nArg < 1 ){` |
|  ! 0 | 1310 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|  ! 0 | 1311 | `			"%s() expects at least 1 argument, 0 given",zFunction);` |
|    - | 1312 | `	}` |
|    - | 1313 | `	/* Missing components default from the current time in php's default` |
|    - | 1314 | `	 * timezone. PHL's date_default_timezone_set() only accepts UTC/GMT (no tz` |
|    - | 1315 | `	 * database), so mktime() and gmmktime() agree and both read gmtime(). */` |
|   39 | 1316 | `	time(&t);` |
|   39 | 1317 | `	pTm = gmtime(&t);` |
|   19 | 1318 | `	SXUNUSED(zFunction);` |
|   39 | 1319 | `	h  = pTm->tm_hour;` |
|   39 | 1320 | `	mi = pTm->tm_min;` |
|   39 | 1321 | `	s  = pTm->tm_sec;` |
|   39 | 1322 | `	mo = pTm->tm_mon + 1;` |
|   39 | 1323 | `	d  = pTm->tm_mday;` |
|   39 | 1324 | `	y  = pTm->tm_year + 1900;` |
|   39 | 1325 | `	h = ph7_value_to_int64(apArg[0]);` |
|   39 | 1326 | `	if( nArg > 1 ){` |
|   39 | 1327 | `		mi = ph7_value_to_int64(apArg[1]);` |
|   39 | 1328 | `		if( nArg > 2 ){` |
|   39 | 1329 | `			s = ph7_value_to_int64(apArg[2]);` |
|   39 | 1330 | `			if( nArg > 3 ){` |
|   39 | 1331 | `				mo = ph7_value_to_int64(apArg[3]);` |
|   39 | 1332 | `				if( nArg > 4 ){` |
|   39 | 1333 | `					d = ph7_value_to_int64(apArg[4]);` |
|   39 | 1334 | `					if( nArg > 5 ){` |
|    - | 1335 | `						/* php's legacy two-digit mapping: 0-69 -> 2000-2069,` |
|    - | 1336 | `						 * 70-100 -> 1970-2000; anything else is verbatim */` |
|   39 | 1337 | `						y = ph7_value_to_int64(apArg[5]);` |
|   39 | 1338 | `						if( y >= 0 && y <= 69 ){` |
|    7 | 1339 | `							y += 2000;` |
|   36 | 1340 | `						}else if( y >= 70 && y <= 100 ){` |
|    5 | 1341 | `							y += 1900;` |
|    2 | 1342 | `						}` |
|   19 | 1343 | `					}` |
|   19 | 1344 | `				}` |
|   19 | 1345 | `			}` |
|   19 | 1346 | `		}` |
|   19 | 1347 | `	}` |
|    - | 1348 | `	/* Normalize the month with floor semantics, then let day/time components` |
|    - | 1349 | `	 * overflow linearly (php: mktime(25,-30,0,1,1,2024) == Jan 2 00:30). */` |
|   39 | 1350 | `	yAdj = y + DtFloorDiv(mo - 1,12);` |
|   39 | 1351 | `	moN  = (int)(mo - 1 - DtFloorDiv(mo - 1,12) * 12) + 1;` |
|   39 | 1352 | `	iVal = (DtDaysFromCivil(yAdj,moN,1) + (d - 1)) * 86400 + h*3600 + mi*60 + s;` |
|    - | 1353 | `	/* Return the timestamp as a 64bit integer */` |
|   39 | 1354 | `	ph7_result_int64(pCtx,iVal);` |
|   39 | 1355 | `	return PH7_OK;` |
|   20 | 1356 | `}` |
|    - | 1357 | `/*` |
|    - | 1358 | ` * string date_default_timezone_get(void)` |
|    - | 1359 | ` *  Gets the default timezone used by all date/time functions in a script.` |
|    - | 1360 | ` */` |
|    4 | 1361 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1362 | `{` |
|    5 | 1363 | `	ph7_vm *pVm = pCtx->pVm;` |
|    2 | 1364 | `	SXUNUSED(nArg);` |
|    2 | 1365 | `	SXUNUSED(apArg);` |
|    5 | 1366 | `	ph7_result_string(pCtx,pVm->zDefTz,(int)pVm->nDefTz);` |
|    5 | 1367 | `	return PH7_OK;` |
|    1 | 1368 | `}` |
|    - | 1369 | `/*` |
|    - | 1370 | ` * bool date_default_timezone_set(string $timezoneId)` |
|    - | 1371 | ` *  Sets the default timezone used by all date/time functions in a script.` |
|    - | 1372 | ` *  php validates against the tz database and stores the id verbatim (get()` |
|    - | 1373 | ` *  echoes back "utc" if that's what was set). PHL ships no tz database, so` |
|    - | 1374 | ` *  only UTC and GMT are accepted; every other id — including region names php` |
|    - | 1375 | ` *  would accept — is rejected with php's invalid-id notice (recorded scope cut).` |
|    - | 1376 | ` */` |
|   26 | 1377 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1378 | `{` |
|   27 | 1379 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1380 | `	const char *zId;` |
|    - | 1381 | `	int nId;` |
|   27 | 1382 | `	if( nArg < 1 ){` |
|  ! 0 | 1383 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1384 | `		return PH7_OK;` |
|    - | 1385 | `	}` |
|   27 | 1386 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|   27 | 1387 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|   27 | 1388 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|   27 | 1389 | `		pVm->zDefTz[3] = 0;` |
|   27 | 1390 | `		pVm->nDefTz = 3;` |
|   27 | 1391 | `		ph7_result_bool(pCtx,1);` |
|   27 | 1392 | `		return PH7_OK;` |
|    - | 1393 | `	}` |
|    - | 1394 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|    - | 1395 | `	 * — exactly php's notice shape here */` |
|  ! 0 | 1396 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|  ! 0 | 1397 | `	ph7_result_bool(pCtx,0);` |
|  ! 0 | 1398 | `	return PH7_OK;` |
|   14 | 1399 | `}` |
|    - | 1400 |  |
|    - | 1401 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1402 |  |
