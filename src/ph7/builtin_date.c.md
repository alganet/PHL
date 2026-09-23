# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 643/723 lines (88.93%)

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
|  426 |   21 | `static void DtSytmFillOffset(Sytm *pSTm,time_t t)` |
|    1 |   22 | `{` |
|  640 |   23 | `	sxi64 iCivil = DtDaysFromCivil((sxi64)pSTm->tm_year,pSTm->tm_mon+1,pSTm->tm_mday) * 86400` |
|  426 |   24 | `		+ (sxi64)pSTm->tm_hour*3600 + (sxi64)pSTm->tm_min*60 + (sxi64)pSTm->tm_sec;` |
|  427 |   25 | `	pSTm->tm_gmtoff = (long)(iCivil - (sxi64)t);` |
|  427 |   26 | `}` |
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
|  468 |  413 | `PH7_PRIVATE sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec)` |
|    1 |  414 | `{` |
|  469 |  415 | `	const char *zEnd = &zIn[nLen];` |
|    - |  416 | `	const char *zCur;` |
|    - |  417 | `	/* Start the format process */` |
| 1459 |  418 | `	for(;;){` |
| 2919 |  419 | `		if( zIn >= zEnd ){` |
|    - |  420 | `			/* No more input to process */` |
|  469 |  421 | `			break;` |
|    - |  422 | `		}` |
| 2451 |  423 | `		switch(zIn[0]){` |
|  134 |  424 | `		case 'd':` |
|    - |  425 | `			/* Day of the month, 2 digits with leading zeros */` |
|  269 |  426 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|  269 |  427 | `			break;` |
|   32 |  428 | `		case 'D':` |
|    - |  429 | `			/*A textual representation of a day, three letters*/` |
|   65 |  430 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|   65 |  431 | `			ph7_result_string(pCtx,zCur,3);` |
|   65 |  432 | `			break;` |
|    1 |  433 | `		case 'j':` |
|    - |  434 | `			/*	Day of the month without leading zeros */` |
|    3 |  435 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    3 |  436 | `			break;` |
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
|  134 |  460 | `		case 'm':` |
|    - |  461 | `			/*Numeric representation of a month, with leading zeros*/` |
|  269 |  462 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|  269 |  463 | `			break;` |
|    1 |  464 | `		case 'M':` |
|    - |  465 | `			/*A short textual representation of a month, three letters*/` |
|    3 |  466 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    3 |  467 | `			ph7_result_string(pCtx,zCur,3);` |
|    3 |  468 | `			break;` |
|    1 |  469 | `		case 'n':` |
|    - |  470 | `			/*Numeric representation of a month, without leading zeros*/` |
|    3 |  471 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    3 |  472 | `			break;` |
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
|  103 |  507 | `		case 'Y':` |
|    - |  508 | `			/*	A full numeric representation of a year, 4 digits */` |
|  207 |  509 | `			ph7_result_string_format(pCtx,"%04d",pTm->tm_year);` |
|  207 |  510 | `			break;` |
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
|    1 |  554 | `		case 'G':` |
|    - |  555 | `			/* 24-hour format of an hour without leading zeros */` |
|    3 |  556 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    3 |  557 | `			break;` |
|    3 |  558 | `		case 'h':` |
|    - |  559 | `			/* 12-hour format of an hour with leading zeros */` |
|   10 |  560 | `			ph7_result_string_format(pCtx,"%02d",` |
|    6 |  561 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|    7 |  562 | `			break;` |
|   70 |  563 | `		case 'H':` |
|    - |  564 | `			/*	24-hour format of an hour with leading zeros */` |
|  141 |  565 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|  141 |  566 | `			break;` |
|   70 |  567 | `		case 'i':` |
|    - |  568 | `			/* 	Minutes with leading zeros */` |
|  141 |  569 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|  141 |  570 | `			break;` |
|   72 |  571 | `		case 's':` |
|    - |  572 | `			/* 	second with leading zeros */` |
|  145 |  573 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|  145 |  574 | `			break;` |
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
|   12 |  591 | `		case 'e':` |
|    - |  592 | `			/* 	Timezone identifier */` |
|   25 |  593 | `			zCur = pTm->tm_zone;` |
|   25 |  594 | `			if( zCur == 0 ){` |
|    - |  595 | `				/* date()-family fills: the script default timezone */` |
|    7 |  596 | `				zCur = pCtx->pVm->zDefTz;` |
|    3 |  597 | `			}` |
|   25 |  598 | `			ph7_result_string(pCtx,zCur,-1);` |
|   25 |  599 | `			break;` |
|    7 |  600 | `		case 'T':{` |
|    - |  601 | `			/* Timezone abbreviation: "GMT+0530" for a fixed offset, the name` |
|    - |  602 | `			 * itself (uppercased) for a named zone. php decides on the zone's` |
|    - |  603 | `			 * TYPE, not on the offset's value, so a zone whose name is an offset` |
|    - |  604 | ``			 * spelling — `new DateTime('@0')`, `new DateTimeZone('+00:00')` —`` |
|    - |  605 | `			 * prints "GMT+0000" where PHL printed the name "+00:00". PHL has no` |
|    - |  606 | `			 * tz database, so the name path only ever sees UTC/GMT/Z. */` |
|    - |  607 | `			const char *z;` |
|   14 |  608 | `			if( pTm->tm_gmtoff != 0` |
|   14 |  609 | `			 \|\| (pTm->tm_zone && (pTm->tm_zone[0] == '+' \|\| pTm->tm_zone[0] == '-')) ){` |
|    7 |  610 | `				long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    7 |  611 | `				ph7_result_string_format(pCtx,"GMT%c%02d%02d",` |
|    6 |  612 | `					pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    7 |  613 | `				break;` |
|    - |  614 | `			}` |
|    9 |  615 | `			z = pTm->tm_zone ? pTm->tm_zone : pCtx->pVm->zDefTz;` |
|   33 |  616 | `			while( *z ){` |
|   25 |  617 | `				int c = (unsigned char)*z;` |
|   25 |  618 | `				if( c >= 'a' && c <= 'z' ){` |
|  ! 0 |  619 | `					c -= 'a' - 'A';` |
|  ! 0 |  620 | `				}` |
|   25 |  621 | `				ph7_result_string_format(pCtx,"%c",c);` |
|   25 |  622 | `				z++;` |
|    1 |  623 | `			}` |
|    9 |  624 | `			break;` |
|    - |  625 | `				 }` |
|    1 |  626 | `		case 'I':` |
|    - |  627 | `			/* Whether or not the date is in daylight saving time. Use the` |
|    - |  628 | `			 * broken-down time's own tm_isdst (as every other platform does):` |
|    - |  629 | `			 * the old Windows _get_daylight() override reported whether the` |
|    - |  630 | `			 * timezone observes DST at all, not whether THIS date is in it. */` |
|    3 |  631 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    3 |  632 | `			break;` |
|    2 |  633 | `		case 'r':{` |
|    - |  634 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200 */` |
|    5 |  635 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    5 |  636 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d %c%02d%02d",` |
|    2 |  637 | `				SyTimeGetDay(pTm->tm_wday),` |
|    2 |  638 | `				pTm->tm_mday,` |
|    2 |  639 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    2 |  640 | `				pTm->tm_year,` |
|    2 |  641 | `				pTm->tm_hour,` |
|    2 |  642 | `				pTm->tm_min,` |
|    2 |  643 | `				pTm->tm_sec,` |
|    4 |  644 | `				pTm->tm_gmtoff < 0 ? '-' : '+',` |
|    4 |  645 | `				(int)(a / 3600),(int)((a % 3600) / 60)` |
|    - |  646 | `				);` |
|    5 |  647 | `			break;` |
|    - |  648 | `				 }` |
|    4 |  649 | `		case 'U':` |
|    - |  650 | `			/* Seconds since the Unix Epoch FOR THIS Sytm (php: the timestamp` |
|    - |  651 | `			 * being formatted — pre-fix this printed time(0) regardless of the` |
|    - |  652 | `			 * date under format). */` |
|   13 |  653 | `			ph7_result_string_format(pCtx,"%qd",` |
|    8 |  654 | `				DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400` |
|    8 |  655 | `				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec` |
|    8 |  656 | `				- (sxi64)pTm->tm_gmtoff);` |
|    9 |  657 | `			break;` |
|    3 |  658 | `		case 'O':{` |
|    - |  659 | `			/* Difference to GMT without colon: +0530 (php) */` |
|    7 |  660 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    7 |  661 | `			ph7_result_string_format(pCtx,"%c%02d%02d",` |
|    6 |  662 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    7 |  663 | `			break;` |
|    - |  664 | `				 }` |
|    5 |  665 | `		case 'P':{` |
|    - |  666 | `			/* Difference to GMT with colon: +05:30 (php) */` |
|   11 |  667 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   11 |  668 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|   10 |  669 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|   11 |  670 | `			break;` |
|    - |  671 | `				 }` |
|    2 |  672 | `		case 'p':{` |
|    - |  673 | `			/* Like P, but "Z" for UTC (php 8.0+) */` |
|    - |  674 | `			long a;` |
|    5 |  675 | `			if( pTm->tm_gmtoff == 0 ){` |
|    3 |  676 | `				ph7_result_string(pCtx,"Z",1);` |
|    3 |  677 | `				break;` |
|    - |  678 | `			}` |
|    3 |  679 | `			a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    3 |  680 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|    2 |  681 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|    3 |  682 | `			break;` |
|    - |  683 | `				 }` |
|    2 |  684 | `		case 'Z':` |
|    - |  685 | `			/* Timezone offset in seconds, plain integer (php) */` |
|    5 |  686 | `			ph7_result_string_format(pCtx,"%d",(int)pTm->tm_gmtoff);` |
|    5 |  687 | `			break;` |
|   15 |  688 | `		case 'c':{` |
|    - |  689 | `			/* 	ISO 8601 date: 2004-02-12T15:19:21+00:00 (php) */` |
|   31 |  690 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   46 |  691 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",` |
|   15 |  692 | `				pTm->tm_year,` |
|   30 |  693 | `				pTm->tm_mon+1,` |
|   15 |  694 | `				pTm->tm_mday,` |
|   15 |  695 | `				pTm->tm_hour,` |
|   15 |  696 | `				pTm->tm_min,` |
|   15 |  697 | `				pTm->tm_sec,` |
|   30 |  698 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60)` |
|    - |  699 | `				);` |
|   31 |  700 | `			break;` |
|    - |  701 | `				 }` |
|    4 |  702 | `		case '\\':` |
|    9 |  703 | `			zIn++;` |
|    - |  704 | `			/* Expand verbatim */` |
|    9 |  705 | `			if( zIn < zEnd ){` |
|    9 |  706 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|    4 |  707 | `			}` |
|    9 |  708 | `			break;` |
|  494 |  709 | `		default:` |
|    - |  710 | `			/* Unknown format specifer,expand verbatim */` |
|  989 |  711 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|  988 |  712 | `			break;` |
|    - |  713 | `		}` |
|    - |  714 | `		/* Point to the next character */` |
| 2451 |  715 | `		zIn++;` |
|    1 |  716 | `	}` |
|  469 |  717 | `	return SXRET_OK;` |
|    1 |  718 | `}` |
|    - |  719 | `/*` |
|    - |  720 | ` * Resolve a date()/gmdate() $timestamp argument under php 8's ?int weak ZPP:` |
|    - |  721 | ` *   - null            -> *pbUseNow = 1 (caller uses the current time)` |
|    - |  722 | ` *   - int/bool/float  -> coerce to a Unix timestamp (float truncates; php's` |
|    - |  723 | ` *                        float->int precision E_DEPRECATED is not emitted, §3.7)` |
|    - |  724 | ` *   - numeric string  -> coerce via php's is_numeric_string grammar` |
|    - |  725 | ` *                        (RangeStrToNumber: " 100 "/"1e3"/".5"/"+5" ok)` |
|    - |  726 | ` *   - anything else (non-numeric string, array, object, resource)` |
|    - |  727 | ` *                     -> catchable TypeError, byte-exact with php.` |
|    - |  728 | ` * Returns PH7_OK with *pbUseNow / *pT set, or the PH7_VmThrowException status.` |
|    - |  729 | ` */` |
|  188 |  730 | `static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)` |
|    1 |  731 | `{` |
|    - |  732 | `	char zBuf[64];` |
|  189 |  733 | `	*pbUseNow = 0;` |
|  189 |  734 | `	if( ph7_value_is_null(pArg) ){` |
|    3 |  735 | `		*pbUseNow = 1;` |
|    3 |  736 | `		return PH7_OK;` |
|    - |  737 | `	}` |
|  187 |  738 | `	if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_float(pArg) ){` |
|  179 |  739 | `		*pT = (time_t)ph7_value_to_int64(pArg);` |
|  179 |  740 | `		return PH7_OK;` |
|    - |  741 | `	}` |
|    9 |  742 | `	if( ph7_value_is_string(pArg) ){` |
|    - |  743 | `		int nStr;` |
|    9 |  744 | `		const char *zStr = ph7_value_to_string(pArg,&nStr);` |
|    - |  745 | `		sxi64 iLong; double dReal;` |
|    9 |  746 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|    9 |  747 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|    3 |  748 | `			*pT = (time_t)dReal;` |
|    6 |  749 | `			return PH7_OK;` |
|    - |  750 | `		}` |
|    7 |  751 | `		if( iKind == RANGE_IN_LONG ){` |
|    7 |  752 | `			*pT = (time_t)iLong;` |
|    7 |  753 | `			return PH7_OK;` |
|    - |  754 | `		}` |
|    - |  755 | `		/* Not a numeric string: fall through to the TypeError. */` |
|  ! 0 |  756 | `	}` |
|  ! 0 |  757 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  758 | `		"%s(): Argument #2 ($timestamp) must be of type ?int, %s given",` |
|  ! 0 |  759 | `		ph7_function_name(pCtx),VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|   95 |  760 | `}` |
|    - |  761 | `/*` |
|    - |  762 | ` * string date(string $format [, int $timestamp = time() ] )` |
|    - |  763 | ` *  Returns a string formatted according to the given format string using` |
|    - |  764 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|    - |  765 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|    - |  766 | ` * Parameters` |
|    - |  767 | ` *  $format` |
|    - |  768 | ` *   The format of the outputted date string (See code above)` |
|    - |  769 | ` * $timestamp` |
|    - |  770 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  771 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  772 | ` *   In other words, it defaults to the value of time().` |
|    - |  773 | ` * Return` |
|    - |  774 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  775 | ` */` |
|  132 |  776 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  777 | `{` |
|    - |  778 | `	const char *zFormat;` |
|    - |  779 | `	int nLen;` |
|    - |  780 | `	Sytm sTm;` |
|  133 |  781 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  782 | `		/* Missing/Invalid argument,return FALSE */` |
|  ! 0 |  783 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  784 | `		return PH7_OK;` |
|    - |  785 | `	}` |
|  133 |  786 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|  133 |  787 | `	if( nLen < 1 ){` |
|    - |  788 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  789 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  790 | `	}` |
|  133 |  791 | `	if( nArg < 2 ){` |
|    - |  792 | `#ifdef __WINNT__` |
|    - |  793 | `		SYSTEMTIME sOS;` |
|    1 |  794 | `		GetSystemTime(&sOS);` |
|    1 |  795 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  796 | `#else` |
|    - |  797 | `		struct tm *pTm;` |
|    - |  798 | `		time_t t;` |
|   30 |  799 | `		time(&t);` |
|   30 |  800 | `		pTm = gmtime(&t);` |
|   30 |  801 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|   30 |  802 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  803 | `#endif` |
|   16 |  804 | `	}else{` |
|    - |  805 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|  103 |  806 | `		time_t t = 0;` |
|    - |  807 | `		struct tm *pTm;` |
|    - |  808 | `		int bUseNow;` |
|  103 |  809 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|  103 |  810 | `		if( rc != PH7_OK ){` |
|  ! 0 |  811 | `			return rc;` |
|    - |  812 | `		}` |
|  103 |  813 | `		if( bUseNow ){` |
|  ! 0 |  814 | `			time(&t);` |
|  ! 0 |  815 | `		}` |
|  103 |  816 | `		pTm = gmtime(&t);` |
|  103 |  817 | `		if( pTm == 0 ){` |
|  ! 0 |  818 | `			time(&t);` |
|  ! 0 |  819 | `			pTm = gmtime(&t);` |
|  ! 0 |  820 | `		}` |
|  103 |  821 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|  103 |  822 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  823 | `	}` |
|    - |  824 | `	/* Format the given string */` |
|  133 |  825 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|  133 |  826 | `	return PH7_OK;` |
|   67 |  827 | `}` |
|    - |  828 | `/*` |
|    - |  829 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|    - |  830 | ` *  Identical to the date() function except that the time returned` |
|    - |  831 | ` *  is Greenwich Mean Time (GMT).` |
|    - |  832 | ` * Parameters` |
|    - |  833 | ` *  $format` |
|    - |  834 | ` *  The format of the outputted date string (See code above)` |
|    - |  835 | ` *  $timestamp` |
|    - |  836 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  837 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  838 | ` *   In other words, it defaults to the value of time().` |
|    - |  839 | ` * Return` |
|    - |  840 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  841 | ` */` |
|  100 |  842 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  843 | `{` |
|    - |  844 | `	const char *zFormat;` |
|    - |  845 | `	int nLen;` |
|    - |  846 | `	Sytm sTm;` |
|  101 |  847 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  848 | `		/* Missing/Invalid argument,return FALSE */` |
|  ! 0 |  849 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  850 | `		return PH7_OK;` |
|    - |  851 | `	}` |
|  101 |  852 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|  101 |  853 | `	if( nLen < 1 ){` |
|    - |  854 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  855 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  856 | `	}` |
|  101 |  857 | `	if( nArg < 2 ){` |
|    - |  858 | `#ifdef __WINNT__` |
|    - |  859 | `		SYSTEMTIME sOS;` |
|    1 |  860 | `		GetSystemTime(&sOS);` |
|    1 |  861 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  862 | `#else` |
|    - |  863 | `		struct tm *pTm;` |
|    - |  864 | `		time_t t;` |
|   14 |  865 | `		time(&t);` |
|   14 |  866 | `		pTm = gmtime(&t);` |
|   14 |  867 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|   14 |  868 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  869 | `#endif` |
|    8 |  870 | `	}else{` |
|    - |  871 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|   87 |  872 | `		time_t t = 0;` |
|    - |  873 | `		struct tm *pTm;` |
|    - |  874 | `		int bUseNow;` |
|   87 |  875 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|   87 |  876 | `		if( rc != PH7_OK ){` |
|  ! 0 |  877 | `			return rc;` |
|    - |  878 | `		}` |
|   87 |  879 | `		if( bUseNow ){` |
|    3 |  880 | `			time(&t);` |
|    1 |  881 | `		}` |
|   87 |  882 | `		pTm = gmtime(&t);` |
|   87 |  883 | `		if( pTm == 0 ){` |
|  ! 0 |  884 | `			time(&t);` |
|  ! 0 |  885 | `			pTm = gmtime(&t);` |
|  ! 0 |  886 | `		}` |
|   87 |  887 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|   87 |  888 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  889 | `	}` |
|    - |  890 | `	/* Format the given string */` |
|  101 |  891 | `	DateFormat(pCtx,zFormat,nLen,&sTm,0);` |
|  101 |  892 | `	return PH7_OK;` |
|   51 |  893 | `}` |
|    - |  894 | `/*` |
|    - |  895 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|    - |  896 | ` *  Return the local time.` |
|    - |  897 | ` * Parameter` |
|    - |  898 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|    - |  899 | ` *     that defaults to the current local time if a timestamp is not given.` |
|    - |  900 | ` *     In other words, it defaults to the value of time().` |
|    - |  901 | ` * $is_associative` |
|    - |  902 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|    - |  903 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|    - |  904 | ` *   array containing all the different elements of the structure returned by the C function` |
|    - |  905 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|    - |  906 | ` *      "tm_sec" - seconds, 0 to 59` |
|    - |  907 | ` *      "tm_min" - minutes, 0 to 59` |
|    - |  908 | ` *      "tm_hour" - hours, 0 to 23` |
|    - |  909 | ` *      "tm_mday" - day of the month, 1 to 31` |
|    - |  910 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|    - |  911 | ` *      "tm_year" - years since 1900` |
|    - |  912 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|    - |  913 | ` *      "tm_yday" - day of the year, 0 to 365` |
|    - |  914 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|    - |  915 | ` * Returns` |
|    - |  916 | ` *  An associative array of information related to the timestamp.` |
|    - |  917 | ` */` |
|    8 |  918 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  919 | `{` |
|    - |  920 | `	ph7_value *pValue,*pArray;` |
|    9 |  921 | `	int isAssoc = 0;` |
|    - |  922 | `	Sytm sTm;` |
|    9 |  923 | `	if( nArg < 1 ){` |
|    - |  924 | `#ifdef __WINNT__` |
|    - |  925 | `		SYSTEMTIME sOS;` |
|    1 |  926 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|    1 |  927 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  928 | `#else` |
|    - |  929 | `		struct tm *pTm;` |
|    - |  930 | `		time_t t;` |
|    4 |  931 | `		time(&t);` |
|    4 |  932 | `		pTm = gmtime(&t);` |
|    4 |  933 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    4 |  934 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  935 | `#endif` |
|    3 |  936 | `	}else{` |
|    - |  937 | `		/* Use the given timestamp */` |
|    - |  938 | `		time_t t;` |
|    - |  939 | `		struct tm *pTm;` |
|    5 |  940 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    5 |  941 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|    5 |  942 | `			pTm = gmtime(&t);` |
|    5 |  943 | `			if( pTm == 0 ){` |
|  ! 0 |  944 | `				time(&t);` |
|  ! 0 |  945 | `			}` |
|    3 |  946 | `		}else{` |
|  ! 0 |  947 | `			time(&t);` |
|    - |  948 | `		}` |
|    5 |  949 | `		pTm = gmtime(&t);` |
|    5 |  950 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    5 |  951 | `		DtSytmFillOffset(&sTm,t);` |
|    - |  952 | `	}` |
|    - |  953 | `	/* Element value */` |
|    9 |  954 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    9 |  955 | `	if( pValue == 0 ){` |
|    - |  956 | `		/* Return NULL */` |
|  ! 0 |  957 | `		ph7_result_null(pCtx);` |
|  ! 0 |  958 | `		return PH7_OK;` |
|    - |  959 | `	}` |
|    - |  960 | `	/* Create a new array */` |
|    9 |  961 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 |  962 | `	if( pArray == 0 ){` |
|    - |  963 | `		/* Return NULL */` |
|  ! 0 |  964 | `		ph7_result_null(pCtx);` |
|  ! 0 |  965 | `		return PH7_OK;` |
|    - |  966 | `	}` |
|    9 |  967 | `	if( nArg > 1 ){` |
|    3 |  968 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|    1 |  969 | `	}` |
|    - |  970 | `	/* Fill the array */` |
|    - |  971 | `	/* Seconds */` |
|    9 |  972 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    9 |  973 | `	if( isAssoc ){` |
|    3 |  974 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|    2 |  975 | `	}else{` |
|    7 |  976 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - |  977 | `	}` |
|    - |  978 | `	/* Minutes */` |
|    9 |  979 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    9 |  980 | `	if( isAssoc ){` |
|    3 |  981 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|    2 |  982 | `	}else{` |
|    7 |  983 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - |  984 | `	}` |
|    - |  985 | `	/* Hours */` |
|    9 |  986 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    9 |  987 | `	if( isAssoc ){` |
|    3 |  988 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|    2 |  989 | `	}else{` |
|    7 |  990 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - |  991 | `	}` |
|    - |  992 | `	/* mday */` |
|    9 |  993 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    9 |  994 | `	if( isAssoc ){` |
|    3 |  995 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|    2 |  996 | `	}else{` |
|    7 |  997 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - |  998 | `	}` |
|    - |  999 | `	/* mon */` |
|    9 | 1000 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|    9 | 1001 | `	if( isAssoc ){` |
|    3 | 1002 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|    2 | 1003 | `	}else{` |
|    7 | 1004 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1005 | `	}` |
|    - | 1006 | `	/* year since 1900 */` |
|    9 | 1007 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|    9 | 1008 | `	if( isAssoc ){` |
|    3 | 1009 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|    2 | 1010 | `	}else{` |
|    7 | 1011 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1012 | `	}` |
|    - | 1013 | `	/* wday */` |
|    9 | 1014 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    9 | 1015 | `	if( isAssoc ){` |
|    3 | 1016 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|    2 | 1017 | `	}else{` |
|    7 | 1018 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1019 | `	}` |
|    - | 1020 | `	/* yday */` |
|    9 | 1021 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    9 | 1022 | `	if( isAssoc ){` |
|    3 | 1023 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|    2 | 1024 | `	}else{` |
|    7 | 1025 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1026 | `	}` |
|    - | 1027 | `	/* isdst */` |
|    - | 1028 | `#ifdef __WINNT__` |
|    - | 1029 | `#ifdef _MSC_VER` |
|    - | 1030 | `#ifndef _WIN32_WCE` |
|    1 | 1031 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1032 | `#endif` |
|    - | 1033 | `#endif` |
|    - | 1034 | `#endif` |
|    9 | 1035 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|    9 | 1036 | `	if( isAssoc ){` |
|    3 | 1037 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|    2 | 1038 | `	}else{` |
|    7 | 1039 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1040 | `	}` |
|    - | 1041 | `	/* Return the array */` |
|    9 | 1042 | `	ph7_result_value(pCtx,pArray);` |
|    9 | 1043 | `	return PH7_OK;` |
|    5 | 1044 | `}` |
|    - | 1045 | `/*` |
|    - | 1046 | ` * int idate(string $format [, int $timestamp = time() ])` |
|    - | 1047 | ` *  Returns a number formatted according to the given format string` |
|    - | 1048 | ` *  using the given integer timestamp or the current local time if` |
|    - | 1049 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|    - | 1050 | ` *  to the value of time().` |
|    - | 1051 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|    - | 1052 | ` *  parameter.` |
|    - | 1053 | ` * $Parameters` |
|    - | 1054 | ` *  Supported format` |
|    - | 1055 | ` *   d 	Day of the month` |
|    - | 1056 | ` *   h 	Hour (12 hour format)` |
|    - | 1057 | ` *   H 	Hour (24 hour format)` |
|    - | 1058 | ` *   i 	Minutes` |
|    - | 1059 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|    - | 1060 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|    - | 1061 | ` *   m 	Month number` |
|    - | 1062 | ` *   s 	Seconds` |
|    - | 1063 | ` *   t 	Days in current month` |
|    - | 1064 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|    - | 1065 | ` *   w 	Day of the week (0 on Sunday)` |
|    - | 1066 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|    - | 1067 | ` *   y 	Year (1 or 2 digits - check note below)` |
|    - | 1068 | ` *   Y 	Year (4 digits)` |
|    - | 1069 | ` *   z 	Day of the year` |
|    - | 1070 | ` *   Z 	Timezone offset in seconds` |
|    - | 1071 | ` * $timestamp` |
|    - | 1072 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|    - | 1073 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|    - | 1074 | ` *  to the value of time().` |
|    - | 1075 | ` * Return` |
|    - | 1076 | ` *  An integer.` |
|    - | 1077 | ` */` |
|  178 | 1078 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1079 | `{` |
|    - | 1080 | `	const char *zFormat;` |
|  180 | 1081 | `	ph7_int64 iVal = 0;` |
|    - | 1082 | `	int nLen;` |
|    - | 1083 | `	Sytm sTm;` |
|  180 | 1084 | `	time_t t = 0; /* The resolved timestamp; 'U' must report THIS, not time(0) */` |
|  180 | 1085 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1086 | `		/* Missing/Invalid argument,return -1 */` |
|  ! 0 | 1087 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1088 | `		return PH7_OK;` |
|    - | 1089 | `	}` |
|  180 | 1090 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|  180 | 1091 | `	if( nLen < 1 ){` |
|    - | 1092 | `		/* Don't bother processing return -1*/` |
|  ! 0 | 1093 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1094 | `	}` |
|  180 | 1095 | `	if( nArg < 2 ){` |
|    - | 1096 | `#ifdef __WINNT__` |
|    - | 1097 | `		SYSTEMTIME sOS;` |
|    2 | 1098 | `		GetSystemTime(&sOS);` |
|    2 | 1099 | `		time(&t);` |
|    2 | 1100 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - | 1101 | `#else` |
|    - | 1102 | `		struct tm *pTm;` |
|   14 | 1103 | `		time(&t);` |
|   14 | 1104 | `		pTm = gmtime(&t);` |
|   14 | 1105 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|   14 | 1106 | `		DtSytmFillOffset(&sTm,t);` |
|    - | 1107 | `#endif` |
|    9 | 1108 | `	}else{` |
|    - | 1109 | `		/* Use the given timestamp */` |
|    - | 1110 | `		struct tm *pTm;` |
|  165 | 1111 | `		if( ph7_value_is_int(apArg[1]) ){` |
|  165 | 1112 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|  165 | 1113 | `			pTm = gmtime(&t);` |
|  165 | 1114 | `			if( pTm == 0 ){` |
|  ! 0 | 1115 | `				time(&t);` |
|  ! 0 | 1116 | `			}` |
|   83 | 1117 | `		}else{` |
|  ! 0 | 1118 | `			time(&t);` |
|    - | 1119 | `		}` |
|  165 | 1120 | `		pTm = gmtime(&t);` |
|  165 | 1121 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|  165 | 1122 | `		DtSytmFillOffset(&sTm,t);` |
|    - | 1123 | `	}` |
|    - | 1124 | `	/* Perform the requested operation */` |
|  180 | 1125 | `	switch(zFormat[0]){` |
|    9 | 1126 | `	case 'd':` |
|    - | 1127 | `	case 'j':` |
|    - | 1128 | `		/* Day of the month ('j' differs from 'd' only in zero padding, which an` |
|    - | 1129 | `		 * integer result cannot carry) */` |
|   19 | 1130 | `		iVal = sTm.tm_mday;` |
|   19 | 1131 | `		break;` |
|    8 | 1132 | `	case 'h':` |
|    - | 1133 | `	case 'g':` |
|    - | 1134 | `		/* Hour (12 hour format): php reports midnight and noon as 12, not 0 —` |
|    - | 1135 | ``		 * `1 + hour % 12` answered 1 for both. */`` |
|   17 | 1136 | `		iVal = sTm.tm_hour % 12;` |
|   17 | 1137 | `		if( iVal == 0 ){` |
|   17 | 1138 | `			iVal = 12;` |
|    8 | 1139 | `		}` |
|   17 | 1140 | `		break;` |
|    9 | 1141 | `	case 'H':` |
|    - | 1142 | `	case 'G':` |
|    - | 1143 | `		/* Hour (24 hour format) */` |
|   19 | 1144 | `		iVal = sTm.tm_hour;` |
|   19 | 1145 | `		break;` |
|    4 | 1146 | `	case 'B': {` |
|    - | 1147 | `		/* Swatch Internet time: 1000 "beats" per day in UTC+1, no fractions.` |
|    - | 1148 | `		 * Integer math throughout so the tiny build (no floating point) agrees. */` |
|    9 | 1149 | `		ph7_int64 iSec = ((ph7_int64)t + 3600) % 86400;` |
|    9 | 1150 | `		if( iSec < 0 ){` |
|  ! 0 | 1151 | `			iSec += 86400;` |
|  ! 0 | 1152 | `		}` |
|    9 | 1153 | `		iVal = iSec * 1000 / 86400;` |
|    9 | 1154 | `		break;` |
|    - | 1155 | `			  }` |
|    5 | 1156 | `	case 'i':` |
|    - | 1157 | `		/*Minutes*/` |
|   11 | 1158 | `		iVal = sTm.tm_min;` |
|   11 | 1159 | `		break;` |
|  ! 0 | 1160 | `	case 'I':` |
|    - | 1161 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|    - | 1162 | `#ifdef __WINNT__` |
|    - | 1163 | `#ifdef _MSC_VER` |
|    - | 1164 | `#ifndef _WIN32_WCE` |
|  ! 0 | 1165 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1166 | `#endif` |
|    - | 1167 | `#endif` |
|    - | 1168 | `#endif` |
|  ! 0 | 1169 | `		iVal = sTm.tm_isdst;` |
|  ! 0 | 1170 | `		break;` |
|    4 | 1171 | `	case 'L':` |
|    - | 1172 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|    9 | 1173 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|    9 | 1174 | `		break;` |
|    9 | 1175 | `	case 'm':` |
|    - | 1176 | `	case 'n':` |
|    - | 1177 | `		/* Month number. Sytm keeps tm_mon 0-based (see 't' below, which tests` |
|    - | 1178 | ``		 * `tm_mon == 1` for February), so July used to answer 6. */`` |
|   19 | 1179 | `		iVal = sTm.tm_mon + 1;` |
|   19 | 1180 | `		break;` |
|    5 | 1181 | `	case 's':` |
|    - | 1182 | `		/*Seconds*/` |
|   11 | 1183 | `		iVal = sTm.tm_sec;` |
|   11 | 1184 | `		break;` |
|    4 | 1185 | `	case 't':{` |
|    - | 1186 | `		/*Days in current month*/` |
|    - | 1187 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    9 | 1188 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|    9 | 1189 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|  ! 0 | 1190 | `			nDays = 28;` |
|  ! 0 | 1191 | `		}` |
|    9 | 1192 | `		iVal = nDays;` |
|    9 | 1193 | `		break;` |
|    - | 1194 | `			 }` |
|    4 | 1195 | `	case 'U':` |
|    - | 1196 | `		/* Seconds since the Unix Epoch. This used to call time(0), ignoring the` |
|    - | 1197 | `		 * $timestamp argument entirely and always answering "now". */` |
|    9 | 1198 | `		iVal = (ph7_int64)t;` |
|    9 | 1199 | `		break;` |
|    4 | 1200 | `	case 'w':` |
|    - | 1201 | `		/*	Day of the week (0 on Sunday) */` |
|    9 | 1202 | `		iVal = sTm.tm_wday;` |
|    9 | 1203 | `		break;` |
|    8 | 1204 | `	case 'W':` |
|    - | 1205 | `	case 'o': {` |
|    - | 1206 | `		/* ISO-8601 week number / week-numbering year: both belong to the year` |
|    - | 1207 | `		 * owning the Thursday of the civil week, so 2021-01-01 is 2020-W53.` |
|    - | 1208 | `		 * The old code indexed a weekday table and returned a DAY number` |
|    - | 1209 | `		 * (1..7) as if it were a week number — idate("W") answered 4 in the` |
|    - | 1210 | `		 * middle of July. Same derivation as date()'s 'o'/'W' above. */` |
|   17 | 1211 | `		sxi64 days = DtDaysFromCivil((sxi64)sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|   17 | 1212 | `		int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|   17 | 1213 | `		sxi64 thu = days + (4 - isoDow);` |
|    - | 1214 | `		sxi64 wy;` |
|    - | 1215 | `		int wm,wd;` |
|   17 | 1216 | `		DtCivilFromDays(thu,&wy,&wm,&wd);` |
|   17 | 1217 | `		if( zFormat[0] == 'o' ){` |
|    9 | 1218 | `			iVal = (ph7_int64)wy;` |
|    5 | 1219 | `		}else{` |
|    9 | 1220 | `			iVal = (ph7_int64)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1;` |
|    - | 1221 | `		}` |
|   17 | 1222 | `		break;` |
|    - | 1223 | `			  }` |
|    4 | 1224 | `	case 'y':` |
|    - | 1225 | `		/* Year (2 digits) */` |
|    9 | 1226 | `		iVal = sTm.tm_year % 100;` |
|    9 | 1227 | `		break;` |
|    6 | 1228 | `	case 'Y':` |
|    - | 1229 | `		/* Year (4 digits) */` |
|   13 | 1230 | `		iVal = sTm.tm_year;` |
|   13 | 1231 | `		break;` |
|    4 | 1232 | `	case 'z':` |
|    - | 1233 | `		/* Day of the year */` |
|    9 | 1234 | `		iVal = sTm.tm_yday;` |
|    9 | 1235 | `		break;` |
|  ! 0 | 1236 | `	case 'Z':` |
|    - | 1237 | `		/*Timezone offset in seconds*/` |
|  ! 0 | 1238 | `		iVal = sTm.tm_gmtoff;` |
|  ! 0 | 1239 | `		break;` |
|    2 | 1240 | `	default:` |
|    - | 1241 | `		/* unknown format,throw a warning */` |
|    6 | 1242 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unrecognized date format token");` |
|    - | 1243 | `		/* php returns FALSE for an unrecognized token, not 0 — the two are` |
|    - | 1244 | ``		 * distinguishable (`idate($t) === false` is the documented check) and`` |
|    - | 1245 | `		 * 0 is a legitimate result for several real tokens. */` |
|    6 | 1246 | `		ph7_result_bool(pCtx,0);` |
|    6 | 1247 | `		return PH7_OK;` |
|    - | 1248 | `	}` |
|    - | 1249 | `	/* Return the time value */` |
|  175 | 1250 | `	ph7_result_int64(pCtx,iVal);` |
|  175 | 1251 | `	return PH7_OK;` |
|   91 | 1252 | `}` |
|    - | 1253 | `/*` |
|    - | 1254 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|    - | 1255 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|    - | 1256 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|    - | 1257 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|    - | 1258 | ` *  specified.` |
|    - | 1259 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|    - | 1260 | ` *  the current value according to the local date and time.` |
|    - | 1261 | ` * Parameters` |
|    - | 1262 | ` * $hour` |
|    - | 1263 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|    - | 1264 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|    - | 1265 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|    - | 1266 | ` * $minute` |
|    - | 1267 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|    - | 1268 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|    - | 1269 | ` *  in the following hour(s).` |
|    - | 1270 | ` * $second` |
|    - | 1271 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|    - | 1272 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|    - | 1273 | ` * second in the following minute(s).` |
|    - | 1274 | ` * $month` |
|    - | 1275 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|    - | 1276 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|    - | 1277 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|    - | 1278 | ` * $day` |
|    - | 1279 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|    - | 1280 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|    - | 1281 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|    - | 1282 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|    - | 1283 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|    - | 1284 | ` * $year` |
|    - | 1285 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|    - | 1286 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|    - | 1287 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|    - | 1288 | ` * $is_dst` |
|    - | 1289 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|    - | 1290 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|    - | 1291 | ` * Return` |
|    - | 1292 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|    - | 1293 | ` *   If the arguments are invalid, the function returns FALSE` |
|    - | 1294 | ` */` |
|   38 | 1295 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1296 | `{` |
|    - | 1297 | `	const char *zFunction;` |
|    - | 1298 | `	ph7_int64 iVal;` |
|    - | 1299 | `	sxi64 h,mi,s,mo,d,y,yAdj;` |
|    - | 1300 | `	int moN;` |
|    - | 1301 | `	struct tm *pTm;` |
|    - | 1302 | `	time_t t;` |
|    - | 1303 | `	/* Extract function name */` |
|   39 | 1304 | `	zFunction = ph7_function_name(pCtx);` |
|    - | 1305 | `	/* PHP 8 dropped the legacy $is_dst 7th parameter: mktime()/gmmktime() now` |
|    - | 1306 | `	 * accept at most 6 arguments and throw a catchable ArgumentCountError` |
|    - | 1307 | `	 * otherwise (the central aBuiltinArity table only enforces the minimum, so` |
|    - | 1308 | `	 * this maximum is checked here). */` |
|   39 | 1309 | `	if( nArg > 6 ){` |
|  ! 0 | 1310 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|  ! 0 | 1311 | `			"%s() expects at most 6 arguments, %d given",zFunction,nArg);` |
|    - | 1312 | `	}` |
|   39 | 1313 | `	if( nArg < 1 ){` |
|  ! 0 | 1314 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|  ! 0 | 1315 | `			"%s() expects at least 1 argument, 0 given",zFunction);` |
|    - | 1316 | `	}` |
|    - | 1317 | `	/* Missing components default from the current time in php's default` |
|    - | 1318 | `	 * timezone. PHL's date_default_timezone_set() only accepts UTC/GMT (no tz` |
|    - | 1319 | `	 * database), so mktime() and gmmktime() agree and both read gmtime(). */` |
|   39 | 1320 | `	time(&t);` |
|   39 | 1321 | `	pTm = gmtime(&t);` |
|   19 | 1322 | `	SXUNUSED(zFunction);` |
|   39 | 1323 | `	h  = pTm->tm_hour;` |
|   39 | 1324 | `	mi = pTm->tm_min;` |
|   39 | 1325 | `	s  = pTm->tm_sec;` |
|   39 | 1326 | `	mo = pTm->tm_mon + 1;` |
|   39 | 1327 | `	d  = pTm->tm_mday;` |
|   39 | 1328 | `	y  = pTm->tm_year + 1900;` |
|   39 | 1329 | `	h = ph7_value_to_int64(apArg[0]);` |
|   39 | 1330 | `	if( nArg > 1 ){` |
|   39 | 1331 | `		mi = ph7_value_to_int64(apArg[1]);` |
|   39 | 1332 | `		if( nArg > 2 ){` |
|   39 | 1333 | `			s = ph7_value_to_int64(apArg[2]);` |
|   39 | 1334 | `			if( nArg > 3 ){` |
|   39 | 1335 | `				mo = ph7_value_to_int64(apArg[3]);` |
|   39 | 1336 | `				if( nArg > 4 ){` |
|   39 | 1337 | `					d = ph7_value_to_int64(apArg[4]);` |
|   39 | 1338 | `					if( nArg > 5 ){` |
|    - | 1339 | `						/* php's legacy two-digit mapping: 0-69 -> 2000-2069,` |
|    - | 1340 | `						 * 70-100 -> 1970-2000; anything else is verbatim */` |
|   39 | 1341 | `						y = ph7_value_to_int64(apArg[5]);` |
|   39 | 1342 | `						if( y >= 0 && y <= 69 ){` |
|    7 | 1343 | `							y += 2000;` |
|   36 | 1344 | `						}else if( y >= 70 && y <= 100 ){` |
|    5 | 1345 | `							y += 1900;` |
|    2 | 1346 | `						}` |
|   19 | 1347 | `					}` |
|   19 | 1348 | `				}` |
|   19 | 1349 | `			}` |
|   19 | 1350 | `		}` |
|   19 | 1351 | `	}` |
|    - | 1352 | `	/* Normalize the month with floor semantics, then let day/time components` |
|    - | 1353 | `	 * overflow linearly (php: mktime(25,-30,0,1,1,2024) == Jan 2 00:30). */` |
|   39 | 1354 | `	yAdj = y + DtFloorDiv(mo - 1,12);` |
|   39 | 1355 | `	moN  = (int)(mo - 1 - DtFloorDiv(mo - 1,12) * 12) + 1;` |
|   39 | 1356 | `	iVal = (DtDaysFromCivil(yAdj,moN,1) + (d - 1)) * 86400 + h*3600 + mi*60 + s;` |
|    - | 1357 | `	/* Return the timestamp as a 64bit integer */` |
|   39 | 1358 | `	ph7_result_int64(pCtx,iVal);` |
|   39 | 1359 | `	return PH7_OK;` |
|   20 | 1360 | `}` |
|    - | 1361 | `/*` |
|    - | 1362 | ` * string date_default_timezone_get(void)` |
|    - | 1363 | ` *  Gets the default timezone used by all date/time functions in a script.` |
|    - | 1364 | ` */` |
|    4 | 1365 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1366 | `{` |
|    5 | 1367 | `	ph7_vm *pVm = pCtx->pVm;` |
|    2 | 1368 | `	SXUNUSED(nArg);` |
|    2 | 1369 | `	SXUNUSED(apArg);` |
|    5 | 1370 | `	ph7_result_string(pCtx,pVm->zDefTz,(int)pVm->nDefTz);` |
|    5 | 1371 | `	return PH7_OK;` |
|    1 | 1372 | `}` |
|    - | 1373 | `/*` |
|    - | 1374 | ` * bool date_default_timezone_set(string $timezoneId)` |
|    - | 1375 | ` *  Sets the default timezone used by all date/time functions in a script.` |
|    - | 1376 | ` *  php validates against the tz database and stores the id verbatim (get()` |
|    - | 1377 | ` *  echoes back "utc" if that's what was set). PHL ships no tz database, so` |
|    - | 1378 | ` *  only UTC and GMT are accepted; every other id — including region names php` |
|    - | 1379 | ` *  would accept — is rejected with php's invalid-id notice (recorded scope cut).` |
|    - | 1380 | ` */` |
|   26 | 1381 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1382 | `{` |
|   27 | 1383 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1384 | `	const char *zId;` |
|    - | 1385 | `	int nId;` |
|   27 | 1386 | `	if( nArg < 1 ){` |
|  ! 0 | 1387 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1388 | `		return PH7_OK;` |
|    - | 1389 | `	}` |
|   27 | 1390 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|   27 | 1391 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|   27 | 1392 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|   27 | 1393 | `		pVm->zDefTz[3] = 0;` |
|   27 | 1394 | `		pVm->nDefTz = 3;` |
|   27 | 1395 | `		ph7_result_bool(pCtx,1);` |
|   27 | 1396 | `		return PH7_OK;` |
|    - | 1397 | `	}` |
|    - | 1398 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|    - | 1399 | `	 * — exactly php's notice shape here */` |
|  ! 0 | 1400 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|  ! 0 | 1401 | `	ph7_result_bool(pCtx,0);` |
|  ! 0 | 1402 | `	return PH7_OK;` |
|   14 | 1403 | `}` |
|    - | 1404 |  |
|    - | 1405 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1406 |  |
