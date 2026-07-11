# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 481/726 lines (66.25%)

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
|    - |   14 | `#ifdef __WINNT__` |
|    - |   15 | `#ifdef _MSC_VER` |
|    - |   16 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|    - |   17 | `#pragma warning(disable:4996) /* _CRT_SECURE_NO_WARNINGS */` |
|    - |   18 | `#endif` |
|    - |   19 | `#endif` |
|    - |   20 | `#endif` |
|    - |   21 | `#ifdef __WINNT__` |
|    - |   22 | `/* GetSystemTime() */` |
|    - |   23 | `#include <Windows.h>` |
|    - |   24 | `#ifdef _WIN32_WCE` |
|    - |   25 | `/* SPDX-SnippetBegin */` |
|    - |   26 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|    - |   27 | `/* SPDX-License-Identifier: blessing */` |
|    - |   28 | `/*` |
|    - |   29 | `** WindowsCE does not have a localtime() function.  So create a` |
|    - |   30 | `** substitute.` |
|    - |   31 | `** Taken from the SQLite3 source tree.` |
|    - |   32 | `** Status: Public domain` |
|    - |   33 | `*/` |
|    - |   34 | `struct tm *__cdecl localtime(const time_t *t)` |
|    - |   35 | `{` |
|    - |   36 | `  static struct tm y;` |
|    - |   37 | `  FILETIME uTm, lTm;` |
|    - |   38 | `  SYSTEMTIME pTm;` |
|    - |   39 | `  ph7_int64 t64;` |
|    - |   40 | `  t64 = *t;` |
|    - |   41 | `  t64 = (t64 + 11644473600)*10000000;` |
|    - |   42 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|    - |   43 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|    - |   44 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|    - |   45 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|    - |   46 | `  y.tm_year = pTm.wYear - 1900;` |
|    - |   47 | `  y.tm_mon = pTm.wMonth - 1;` |
|    - |   48 | `  y.tm_wday = pTm.wDayOfWeek;` |
|    - |   49 | `  y.tm_mday = pTm.wDay;` |
|    - |   50 | `  y.tm_hour = pTm.wHour;` |
|    - |   51 | `  y.tm_min = pTm.wMinute;` |
|    - |   52 | `  y.tm_sec = pTm.wSecond;` |
|    - |   53 | `  return &y;` |
|    - |   54 | `}` |
|    - |   55 | `/* SPDX-SnippetEnd */` |
|    - |   56 | `#endif /*_WIN32_WCE */` |
|    - |   57 | `#elif defined(__UNIXES__)` |
|    - |   58 | `#include <sys/time.h>` |
|    - |   59 | `#endif /* __WINNT__*/` |
|    - |   60 | `/*` |
|    - |   61 | ` * Resolve the current wall-clock time (epoch seconds + sub-second microseconds).` |
|    - |   62 | ` *` |
|    - |   63 | ` * An embedder may override the platform clock via PH7_CONFIG_CLOCK (e.g. the` |
|    - |   64 | ` * ESP32 port routes this through esp_timer); when no hook is registered we use` |
|    - |   65 | ` * gettimeofday() on Unix and fall back to a second-resolution time() elsewhere.` |
|    - |   66 | ` * Centralising this here gives microtime()/gettimeofday() a single sub-second` |
|    - |   67 | `` * source instead of the old nonsensical `tt % SX_USEC_PER_SEC` off-Unix path.`` |
|    - |   68 | ` */` |
|   34 |   69 | `static void DateNow(ph7_vm *pVm,sytime *pOut)` |
|    1 |   70 | `{` |
|   35 |   71 | `	if( pVm && pVm->pEngine->xConf.xClock ){` |
|  ! 0 |   72 | `		ph7_int64 sec = 0,usec = 0;` |
|  ! 0 |   73 | `		if( pVm->pEngine->xConf.xClock(pVm->pEngine->xConf.pClockData,&sec,&usec) == PH7_OK ){` |
|  ! 0 |   74 | `			pOut->tm_sec  = (long)sec;` |
|  ! 0 |   75 | `			pOut->tm_usec = (long)usec;` |
|  ! 0 |   76 | `			return;` |
|    - |   77 | `		}` |
|  ! 0 |   78 | `	}` |
|    - |   79 | `#if defined(__UNIXES__)` |
|    - |   80 | `	{` |
|    - |   81 | `		struct timeval tv;` |
|   34 |   82 | `		gettimeofday(&tv,0);` |
|   34 |   83 | `		pOut->tm_sec  = (long)tv.tv_sec;` |
|   34 |   84 | `		pOut->tm_usec = (long)tv.tv_usec;` |
|    - |   85 | `	}` |
|    - |   86 | `#elif defined(__WINNT__)` |
|    - |   87 | `	{` |
|    - |   88 | `		/* FILETIME is 100-ns ticks since 1601-01-01 UTC; convert to the Unix` |
|    - |   89 | `		 * epoch with microsecond resolution (GetSystemTime() only carries` |
|    - |   90 | `		 * milliseconds, and time() has no sub-second part at all). */` |
|    - |   91 | `		FILETIME ft;` |
|    - |   92 | `		ph7_int64 t;` |
|    1 |   93 | `		GetSystemTimeAsFileTime(&ft);` |
|    1 |   94 | `		t  = (ph7_int64)ft.dwHighDateTime << 32;` |
|    1 |   95 | `		t += ft.dwLowDateTime;` |
|    1 |   96 | `		t -= 116444736000000000LL; /* 100-ns ticks between 1601 and 1970 */` |
|    1 |   97 | `		pOut->tm_sec  = (long)(t / 10000000);` |
|    1 |   98 | `		pOut->tm_usec = (long)((t % 10000000) / 10);` |
|    - |   99 | `	}` |
|    - |  100 | `#else` |
|    - |  101 | `	{` |
|    - |  102 | `		time_t tt;` |
|    - |  103 | `		time(&tt);` |
|    - |  104 | `		pOut->tm_sec  = (long)tt;` |
|    - |  105 | `		pOut->tm_usec = 0; /* no sub-second source; embedders supply one via PH7_CONFIG_CLOCK */` |
|    - |  106 | `	}` |
|    - |  107 | `#endif /* __UNIXES__ */` |
|   18 |  108 | `}` |
|    - |  109 | ` /*` |
|    - |  110 | `  * int64 time(void)` |
|    - |  111 | `  *  Current Unix timestamp` |
|    - |  112 | `  * Parameters` |
|    - |  113 | `  *  None.` |
|    - |  114 | `  * Return` |
|    - |  115 | `  *  Returns the current time measured in the number of seconds` |
|    - |  116 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|    - |  117 | `  */` |
|    8 |  118 | `PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  119 | `{` |
|    - |  120 | `	time_t tt;` |
|    4 |  121 | `	SXUNUSED(nArg); /* cc warning */` |
|    4 |  122 | `	SXUNUSED(apArg);` |
|    - |  123 | `	/* Extract the current time */` |
|    9 |  124 | `	time(&tt);` |
|    - |  125 | `	/* Return as 64-bit integer */` |
|    9 |  126 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|    9 |  127 | `	return  PH7_OK;` |
|    1 |  128 | `}` |
|    - |  129 | `/*` |
|    - |  130 | `  * string/float microtime([ bool $get_as_float = false ])` |
|    - |  131 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|    - |  132 | `  * Parameters` |
|    - |  133 | `  *  $get_as_float` |
|    - |  134 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|    - |  135 | `  *   as described in the return values section below.` |
|    - |  136 | `  * Return` |
|    - |  137 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|    - |  138 | `  *  is the current time measured in the number of seconds since the Unix` |
|    - |  139 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|    - |  140 | `  *  that have elapsed since sec expressed in seconds.` |
|    - |  141 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|    - |  142 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|    - |  143 | `  */` |
|   26 |  144 | `PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  145 | `{` |
|   27 |  146 | `	int bFloat = 0;` |
|    - |  147 | `	sytime sTime;` |
|   27 |  148 | `	DateNow(pCtx->pVm,&sTime);` |
|   27 |  149 | `	if( nArg > 0 ){` |
|   21 |  150 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|   10 |  151 | `	}` |
|   27 |  152 | `	if( bFloat ){` |
|    - |  153 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|   21 |  154 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|   11 |  155 | `	}else{` |
|    - |  156 | `		/* Return PHP's "msec sec" form: the sub-second part as fractional` |
|    - |  157 | `		 * seconds to 8 decimals, e.g. "0.50667100 1700000000". tm_usec is in` |
|    - |  158 | `		 * microseconds (0..999999), so scaling by 100 yields the 8-digit` |
|    - |  159 | `		 * fraction — matching PHP's "%.8F" output exactly. */` |
|    7 |  160 | `		ph7_result_string_format(pCtx,"0.%08ld %ld",sTime.tm_usec*100,sTime.tm_sec);` |
|    - |  161 | `	}` |
|   27 |  162 | `	return PH7_OK;` |
|    1 |  163 | `}` |
|    - |  164 | `/*` |
|    - |  165 | ` * array getdate ([ int $timestamp = time() ])` |
|    - |  166 | ` *  Returns an associative array containing the date information` |
|    - |  167 | ` *  of the timestamp, or the current local time if no timestamp is given.` |
|    - |  168 | ` * Parameter` |
|    - |  169 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|    - |  170 | ` *     that defaults to the current local time if a timestamp is not given.` |
|    - |  171 | ` *     In other words, it defaults to the value of time().` |
|    - |  172 | ` * Returns` |
|    - |  173 | ` *  Returns an associative array of information related to the timestamp.` |
|    - |  174 | ` */` |
|    8 |  175 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  176 | `{` |
|    - |  177 | `	ph7_value *pValue,*pArray;` |
|    - |  178 | `	Sytm sTm;` |
|    9 |  179 | `	if( nArg < 1 ){` |
|    - |  180 | `#ifdef __WINNT__` |
|    - |  181 | `		SYSTEMTIME sOS;` |
|    1 |  182 | `		GetSystemTime(&sOS);` |
|    1 |  183 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  184 | `#else` |
|    - |  185 | `		struct tm *pTm;` |
|    - |  186 | `		time_t t;` |
|    4 |  187 | `		time(&t);` |
|    4 |  188 | `		pTm = localtime(&t);` |
|    4 |  189 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  190 | `#endif` |
|    3 |  191 | `	}else{` |
|    - |  192 | `		/* Use the given timestamp */` |
|    - |  193 | `		time_t t;` |
|    - |  194 | `		struct tm *pTm;` |
|    5 |  195 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    5 |  196 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|    5 |  197 | `			pTm = localtime(&t);` |
|    5 |  198 | `			if( pTm == 0 ){` |
|  ! 0 |  199 | `				time(&t);` |
|  ! 0 |  200 | `			}` |
|    3 |  201 | `		}else{` |
|  ! 0 |  202 | `			time(&t);` |
|    - |  203 | `		}` |
|    5 |  204 | `		pTm = localtime(&t);` |
|    5 |  205 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  206 | `	}` |
|    - |  207 | `	/* Element value */` |
|    9 |  208 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    9 |  209 | `	if( pValue == 0 ){` |
|    - |  210 | `		/* Return NULL */` |
|  ! 0 |  211 | `		ph7_result_null(pCtx);` |
|  ! 0 |  212 | `		return PH7_OK;` |
|    - |  213 | `	}` |
|    - |  214 | `	/* Create a new array */` |
|    9 |  215 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 |  216 | `	if( pArray == 0 ){` |
|    - |  217 | `		/* Return NULL */` |
|  ! 0 |  218 | `		ph7_result_null(pCtx);` |
|  ! 0 |  219 | `		return PH7_OK;` |
|    - |  220 | `	}` |
|    - |  221 | `	/* Fill the array */` |
|    - |  222 | `	/* Seconds */` |
|    9 |  223 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    9 |  224 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|    - |  225 | `	/* Minutes */` |
|    9 |  226 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    9 |  227 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|    - |  228 | `	/* Hours */` |
|    9 |  229 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    9 |  230 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|    - |  231 | `	/* mday */` |
|    9 |  232 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    9 |  233 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|    - |  234 | `	/* wday */` |
|    9 |  235 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    9 |  236 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|    - |  237 | `	/* mon */` |
|    9 |  238 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|    9 |  239 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|    - |  240 | `	/* year */` |
|    9 |  241 | `	ph7_value_int(pValue,sTm.tm_year);` |
|    9 |  242 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|    - |  243 | `	/* yday */` |
|    9 |  244 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    9 |  245 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|    - |  246 | `	/* Weekday [i.e: Monday,Tuesday,...] */` |
|    9 |  247 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|    9 |  248 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|    - |  249 | `	/* Reset the string cursor */` |
|    9 |  250 | `	ph7_value_reset_string_cursor(pValue);` |
|    - |  251 | `	/* Month [i.e: January,February,...] */` |
|    9 |  252 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|    9 |  253 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|    - |  254 | `	/* Return the freshly created array */` |
|    9 |  255 | `	ph7_result_value(pCtx,pArray);` |
|    9 |  256 | `	return PH7_OK;` |
|    5 |  257 | `}` |
|    - |  258 | `/*` |
|    - |  259 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|    - |  260 | ` *  Returns an associative array containing the data returned from the system call.` |
|    - |  261 | ` * Parameters` |
|    - |  262 | ` *  $return_float` |
|    - |  263 | ` *   When set to TRUE, a float instead of an array is returned.` |
|    - |  264 | ` * Return` |
|    - |  265 | ` *  By default an array is returned. If return_float is set, then` |
|    - |  266 | ` *  a float is returned.` |
|    - |  267 | ` */` |
|    8 |  268 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  269 | `{` |
|    9 |  270 | `	int bFloat = 0;` |
|    - |  271 | `	sytime sTime;` |
|    9 |  272 | `	DateNow(pCtx->pVm,&sTime);` |
|    9 |  273 | `	if( nArg > 0 ){` |
|    7 |  274 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|    3 |  275 | `	}` |
|    9 |  276 | `	if( bFloat ){` |
|    - |  277 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|    5 |  278 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|    3 |  279 | `	}else{` |
|    - |  280 | `		/* Return an associative array */` |
|    - |  281 | `		ph7_value *pValue,*pArray;` |
|    - |  282 | `		/* Create a new array */` |
|    5 |  283 | `		pArray = ph7_context_new_array(pCtx);` |
|    - |  284 | `		/* Element value */` |
|    5 |  285 | `		pValue = ph7_context_new_scalar(pCtx);` |
|    5 |  286 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    - |  287 | `			/* Return NULL */` |
|  ! 0 |  288 | `			ph7_result_null(pCtx);` |
|  ! 0 |  289 | `			return PH7_OK;` |
|    - |  290 | `		}` |
|    - |  291 | `		/* Fill the array */` |
|    - |  292 | `		/* sec */` |
|    5 |  293 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|    5 |  294 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|    - |  295 | `		/* usec */` |
|    5 |  296 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|    5 |  297 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|    - |  298 | `		/* Return the array */` |
|    5 |  299 | `		ph7_result_value(pCtx,pArray);` |
|    - |  300 | `	}` |
|    9 |  301 | `	return PH7_OK;` |
|    5 |  302 | `}` |
|    - |  303 | `/* Check if the given year is leap or not */` |
|    - |  304 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|    - |  305 | `/* ISO-8601 numeric representation of the day of the week */` |
|    - |  306 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|    - |  307 | `/*` |
|    - |  308 | ` * Format a given date string.` |
|    - |  309 | ` * Supported format: (Taken from PHP online docs)` |
|    - |  310 | ` * character 	Description` |
|    - |  311 | ` * d          Day of the month, 2 digits with leading zeros` |
|    - |  312 | ` * D          A textual representation of a day, three letters` |
|    - |  313 | ` * j          Day of the month without leading zeros` |
|    - |  314 | ` * l          A full textual representation of the day of the week` |
|    - |  315 | ` * N          ISO-8601 numeric representation of the day of the week` |
|    - |  316 | ` * w          Numeric representation of the day of the week` |
|    - |  317 | ` * z          The day of the year (starting from 0)` |
|    - |  318 | ` * F          A full textual representation of a month, such as January or March` |
|    - |  319 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|    - |  320 | ` * M          A short textual representation of a month, three letters` |
|    - |  321 | ` * n          Numeric representation of a month, without leading zeros` |
|    - |  322 | ` * t          Number of days in the given month` |
|    - |  323 | ` * L          Whether it's a leap year` |
|    - |  324 | ` * o          ISO-8601 year number. This has the same value as Y` |
|    - |  325 | ` * Y          A full numeric representation of a year, 4 digits` |
|    - |  326 | ` * y          A two digit representation of a year` |
|    - |  327 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|    - |  328 | ` * A          Uppercase Ante meridiem and Post meridiem` |
|    - |  329 | ` * g          12-hour format of an hour without leading zeros` |
|    - |  330 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|    - |  331 | ` * h          12-hour format of an hour with leading zeros` |
|    - |  332 | ` * H          24-hour format of an hour with leading zeros` |
|    - |  333 | ` * i          Minutes with leading zeros` |
|    - |  334 | ` * s          Seconds, with leading zeros` |
|    - |  335 | ` * u          Microseconds` |
|    - |  336 | ` * e          Timezone identifier` |
|    - |  337 | ` * I          Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|    - |  338 | ` * r          RFC 2822 formatted date` |
|    - |  339 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|    - |  340 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|    - |  341 | ` * O          Difference to Greenwich time (GMT) in hours` |
|    - |  342 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|    - |  343 | ` *            east of UTC is always positive.` |
|    - |  344 | ` * c         ISO 8601 date` |
|    - |  345 | ` */` |
|   68 |  346 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|    1 |  347 | `{` |
|   69 |  348 | `	const char *zEnd = &zIn[nLen];` |
|    - |  349 | `	const char *zCur;` |
|    - |  350 | `	/* Start the format process */` |
|  198 |  351 | `	for(;;){` |
|  397 |  352 | `		if( zIn >= zEnd ){` |
|    - |  353 | `			/* No more input to process */` |
|   69 |  354 | `			break;` |
|    - |  355 | `		}` |
|  329 |  356 | `		switch(zIn[0]){` |
|   18 |  357 | `		case 'd':` |
|    - |  358 | `			/* Day of the month, 2 digits with leading zeros */` |
|   37 |  359 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|   37 |  360 | `			break;` |
|  ! 0 |  361 | `		case 'D':` |
|    - |  362 | `			/*A textual representation of a day, three letters*/` |
|  ! 0 |  363 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|  ! 0 |  364 | `			ph7_result_string(pCtx,zCur,3);` |
|  ! 0 |  365 | `			break;` |
|  ! 0 |  366 | `		case 'j':` |
|    - |  367 | `			/*	Day of the month without leading zeros */` |
|  ! 0 |  368 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|  ! 0 |  369 | `			break;` |
|    2 |  370 | `		case 'l':` |
|    - |  371 | `			/* A full textual representation of the day of the week */` |
|    5 |  372 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    5 |  373 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|    5 |  374 | `			break;` |
|  ! 0 |  375 | `		case 'N':{` |
|    - |  376 | `			/* ISO-8601 numeric representation of the day of the week */` |
|  ! 0 |  377 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|  ! 0 |  378 | `			break;` |
|    - |  379 | `				 }` |
|  ! 0 |  380 | `		case 'w':` |
|    - |  381 | `			/*Numeric representation of the day of the week*/` |
|  ! 0 |  382 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|  ! 0 |  383 | `			break;` |
|  ! 0 |  384 | `		case 'z':` |
|    - |  385 | `			/*The day of the year*/` |
|  ! 0 |  386 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|  ! 0 |  387 | `			break;` |
|    2 |  388 | `		case 'F':` |
|    - |  389 | `			/*A full textual representation of a month, such as January or March*/` |
|    5 |  390 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    5 |  391 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|    5 |  392 | `			break;` |
|   18 |  393 | `		case 'm':` |
|    - |  394 | `			/*Numeric representation of a month, with leading zeros*/` |
|   37 |  395 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|   37 |  396 | `			break;` |
|  ! 0 |  397 | `		case 'M':` |
|    - |  398 | `			/*A short textual representation of a month, three letters*/` |
|  ! 0 |  399 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|  ! 0 |  400 | `			ph7_result_string(pCtx,zCur,3);` |
|  ! 0 |  401 | `			break;` |
|  ! 0 |  402 | `		case 'n':` |
|    - |  403 | `			/*Numeric representation of a month, without leading zeros*/` |
|  ! 0 |  404 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|  ! 0 |  405 | `			break;` |
|  ! 0 |  406 | `		case 't':{` |
|    - |  407 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|  ! 0 |  408 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|  ! 0 |  409 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|  ! 0 |  410 | `				nDays = 28;` |
|  ! 0 |  411 | `			}` |
|    - |  412 | `			/*Number of days in the given month*/` |
|  ! 0 |  413 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|  ! 0 |  414 | `			break;` |
|    - |  415 | `				 }` |
|  ! 0 |  416 | `		case 'L':{` |
|  ! 0 |  417 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|    - |  418 | `			/* Whether it's a leap year */` |
|  ! 0 |  419 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|  ! 0 |  420 | `			break;` |
|    - |  421 | `				 }` |
|  ! 0 |  422 | `		case 'o':` |
|    - |  423 | `			/* ISO-8601 year number.*/` |
|  ! 0 |  424 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|  ! 0 |  425 | `			break;` |
|   20 |  426 | `		case 'Y':` |
|    - |  427 | `			/*	A full numeric representation of a year, 4 digits */` |
|   41 |  428 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|   41 |  429 | `			break;` |
|  ! 0 |  430 | `		case 'y':` |
|    - |  431 | `			/*A two digit representation of a year*/` |
|  ! 0 |  432 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|  ! 0 |  433 | `			break;` |
|  ! 0 |  434 | `		case 'a':` |
|    - |  435 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|  ! 0 |  436 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|  ! 0 |  437 | `			break;` |
|  ! 0 |  438 | `		case 'A':` |
|    - |  439 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|  ! 0 |  440 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|  ! 0 |  441 | `			break;` |
|  ! 0 |  442 | `		case 'g':` |
|    - |  443 | `			/*	12-hour format of an hour without leading zeros*/` |
|  ! 0 |  444 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|  ! 0 |  445 | `			break;` |
|  ! 0 |  446 | `		case 'G':` |
|    - |  447 | `			/* 24-hour format of an hour without leading zeros */` |
|  ! 0 |  448 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|  ! 0 |  449 | `			break;` |
|  ! 0 |  450 | `		case 'h':` |
|    - |  451 | `			/* 12-hour format of an hour with leading zeros */` |
|  ! 0 |  452 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|  ! 0 |  453 | `			break;` |
|   12 |  454 | `		case 'H':` |
|    - |  455 | `			/*	24-hour format of an hour with leading zeros */` |
|   25 |  456 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|   25 |  457 | `			break;` |
|   12 |  458 | `		case 'i':` |
|    - |  459 | `			/* 	Minutes with leading zeros */` |
|   25 |  460 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|   25 |  461 | `			break;` |
|   12 |  462 | `		case 's':` |
|    - |  463 | `			/* 	second with leading zeros */` |
|   25 |  464 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|   25 |  465 | `			break;` |
|  ! 0 |  466 | `		case 'u':` |
|    - |  467 | `			/* 	Microseconds */` |
|  ! 0 |  468 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|  ! 0 |  469 | `			break;` |
|  ! 0 |  470 | `		case 'S':{` |
|    - |  471 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|    - |  472 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|  ! 0 |  473 | `			int v = pTm->tm_mday;` |
|  ! 0 |  474 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|  ! 0 |  475 | `			break;` |
|    - |  476 | `				 }` |
|  ! 0 |  477 | `		case 'e':` |
|    - |  478 | `			/* 	Timezone identifier */` |
|  ! 0 |  479 | `			zCur = pTm->tm_zone;` |
|  ! 0 |  480 | `			if( zCur == 0 ){` |
|    - |  481 | `				/* Assume GMT */` |
|  ! 0 |  482 | `				zCur = "GMT";` |
|  ! 0 |  483 | `			}` |
|  ! 0 |  484 | `			ph7_result_string(pCtx,zCur,-1);` |
|  ! 0 |  485 | `			break;` |
|  ! 0 |  486 | `		case 'I':` |
|    - |  487 | `			/* Whether or not the date is in daylight saving time */` |
|    - |  488 | `#ifdef __WINNT__` |
|    - |  489 | `#ifdef _MSC_VER` |
|    - |  490 | `#ifndef _WIN32_WCE` |
|  ! 0 |  491 | `			_get_daylight(&pTm->tm_isdst);` |
|    - |  492 | `#endif` |
|    - |  493 | `#endif` |
|    - |  494 | `#endif` |
|  ! 0 |  495 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|  ! 0 |  496 | `			break;` |
|  ! 0 |  497 | `		case 'r':` |
|    - |  498 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|  ! 0 |  499 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|  ! 0 |  500 | `				SyTimeGetDay(pTm->tm_wday),` |
|  ! 0 |  501 | `				pTm->tm_mday,` |
|  ! 0 |  502 | `				SyTimeGetMonth(pTm->tm_mon),` |
|  ! 0 |  503 | `				pTm->tm_year,` |
|  ! 0 |  504 | `				pTm->tm_hour,` |
|  ! 0 |  505 | `				pTm->tm_min,` |
|  ! 0 |  506 | `				pTm->tm_sec` |
|    - |  507 | `				);` |
|  ! 0 |  508 | `			break;` |
|  ! 0 |  509 | `		case 'U':{` |
|    - |  510 | `			time_t tt;` |
|    - |  511 | `			/* Seconds since the Unix Epoch */` |
|  ! 0 |  512 | `			time(&tt);` |
|  ! 0 |  513 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|  ! 0 |  514 | `			break;` |
|    - |  515 | `				 }` |
|  ! 0 |  516 | `		case 'O':` |
|    - |  517 | `		case 'P':` |
|    - |  518 | `			/* Difference to Greenwich time (GMT) in hours */` |
|  ! 0 |  519 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|  ! 0 |  520 | `			break;` |
|  ! 0 |  521 | `		case 'Z':` |
|    - |  522 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|    - |  523 | `			 * is always negative, and for those east of UTC is always positive.` |
|    - |  524 | `			 */` |
|  ! 0 |  525 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|  ! 0 |  526 | `			break;` |
|    1 |  527 | `		case 'c':` |
|    - |  528 | `			/* 	ISO 8601 date */` |
|    4 |  529 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|    1 |  530 | `				pTm->tm_year,` |
|    2 |  531 | `				pTm->tm_mon+1,` |
|    1 |  532 | `				pTm->tm_mday,` |
|    1 |  533 | `				pTm->tm_hour,` |
|    1 |  534 | `				pTm->tm_min,` |
|    1 |  535 | `				pTm->tm_sec,` |
|    1 |  536 | `				pTm->tm_gmtoff` |
|    - |  537 | `				);` |
|    3 |  538 | `			break;` |
|    1 |  539 | `		case '\\':` |
|    3 |  540 | `			zIn++;` |
|    - |  541 | `			/* Expand verbatim */` |
|    3 |  542 | `			if( zIn < zEnd ){` |
|    3 |  543 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|    1 |  544 | `			}` |
|    3 |  545 | `			break;` |
|   66 |  546 | `		default:` |
|    - |  547 | `			/* Unknown format specifer,expand verbatim */` |
|  133 |  548 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|  132 |  549 | `			break;` |
|    - |  550 | `		}` |
|    - |  551 | `		/* Point to the next character */` |
|  329 |  552 | `		zIn++;` |
|    1 |  553 | `	}` |
|   69 |  554 | `	return SXRET_OK;` |
|    1 |  555 | `}` |
|    - |  556 | `/*` |
|    - |  557 | ` * PH7 implementation of the strftime() function.` |
|    - |  558 | ` * The following formats are supported:` |
|    - |  559 | ` * %a 	An abbreviated textual representation of the day` |
|    - |  560 | ` * %A 	A full textual representation of the day` |
|    - |  561 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|    - |  562 | ` * %e 	Day of the month, with a space preceding single digits.` |
|    - |  563 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|    - |  564 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|    - |  565 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|    - |  566 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|    - |  567 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|    - |  568 | ` *   4 weekdays, with Monday being the start of the week.` |
|    - |  569 | ` * %W 	A numeric representation of the week of the year` |
|    - |  570 | ` * %b 	Abbreviated month name, based on the locale` |
|    - |  571 | ` * %B 	Full month name, based on the locale` |
|    - |  572 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|    - |  573 | ` * %m 	Two digit representation of the month` |
|    - |  574 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|    - |  575 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|    - |  576 | ` * %G 	The full four-digit version of %g` |
|    - |  577 | ` * %y 	Two digit representation of the year` |
|    - |  578 | ` * %Y 	Four digit representation for the year` |
|    - |  579 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|    - |  580 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|    - |  581 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|    - |  582 | ` * %M 	Two digit representation of the minute` |
|    - |  583 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|    - |  584 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|    - |  585 | ` * %r 	Same as "%I:%M:%S %p"` |
|    - |  586 | ` * %R 	Same as "%H:%M"` |
|    - |  587 | ` * %S 	Two digit representation of the second` |
|    - |  588 | ` * %T 	Same as "%H:%M:%S"` |
|    - |  589 | ` * %X 	Preferred time representation based on locale, without the date` |
|    - |  590 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|    - |  591 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|    - |  592 | ` * %c 	Preferred date and time stamp based on local` |
|    - |  593 | ` * %D 	Same as "%m/%d/%y"` |
|    - |  594 | ` * %F 	Same as "%Y-%m-%d"` |
|    - |  595 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|    - |  596 | ` * %x 	Preferred date representation based on locale, without the time` |
|    - |  597 | ` * %n 	A newline character ("\n")` |
|    - |  598 | ` * %t 	A Tab character ("\t")` |
|    - |  599 | ` * %% 	A literal percentage character ("%")` |
|    - |  600 | ` */` |
|   16 |  601 | `static int PH7_Strftime(` |
|    - |  602 | `	ph7_context *pCtx,  /* Call context */` |
|    - |  603 | `	const char *zIn,    /* Input string */` |
|    - |  604 | `	int nLen,           /* Input length */` |
|    - |  605 | `	Sytm *pTm           /* Parse of the given time */` |
|    - |  606 | `	)` |
|    1 |  607 | `{` |
|   17 |  608 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|    - |  609 | `	int c;` |
|    - |  610 | `	/* Start the format process */` |
|   18 |  611 | `	for(;;){` |
|   37 |  612 | `		zCur = zIn;` |
|   41 |  613 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|    5 |  614 | `			zIn++;` |
|    1 |  615 | `		}` |
|   37 |  616 | `		if( zIn > zCur ){` |
|    - |  617 | `			/* Consume input verbatim */` |
|    5 |  618 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|    2 |  619 | `		}` |
|   37 |  620 | `		zIn++; /* Jump the percent sign */` |
|   37 |  621 | `		if( zIn >= zEnd ){` |
|    - |  622 | `			/* No more input to process */` |
|   17 |  623 | `			break;` |
|    - |  624 | `		}` |
|   21 |  625 | `		c = zIn[0];` |
|    - |  626 | `		/* Act according to the current specifer */` |
|   21 |  627 | `		switch(c){` |
|  ! 0 |  628 | `		case '%':` |
|    - |  629 | `			/* A literal percentage character ("%") */` |
|  ! 0 |  630 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|  ! 0 |  631 | `			break;` |
|  ! 0 |  632 | `		case 't':` |
|    - |  633 | `			/* A Tab character */` |
|  ! 0 |  634 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|  ! 0 |  635 | `			break;` |
|  ! 0 |  636 | `		case 'n':` |
|    - |  637 | `			/* A newline character */` |
|  ! 0 |  638 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|  ! 0 |  639 | `			break;` |
|    1 |  640 | `		case 'a':` |
|    - |  641 | `			/* An abbreviated textual representation of the day */` |
|    3 |  642 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|    3 |  643 | `			break;` |
|  ! 0 |  644 | `		case 'A':` |
|    - |  645 | `			/* A full textual representation of the day */` |
|  ! 0 |  646 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|  ! 0 |  647 | `			break;` |
|  ! 0 |  648 | `		case 'e':` |
|    - |  649 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|  ! 0 |  650 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|  ! 0 |  651 | `			break;` |
|    2 |  652 | `		case 'd':` |
|    - |  653 | `			/* Two-digit day of the month (with leading zeros) */` |
|    5 |  654 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|    5 |  655 | `			break;` |
|  ! 0 |  656 | `		case 'j':` |
|    - |  657 | `			/*The day of the year,3 digits with leading zeros*/` |
|  ! 0 |  658 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|  ! 0 |  659 | `			break;` |
|  ! 0 |  660 | `		case 'u':` |
|    - |  661 | `			/* ISO-8601 numeric representation of the day of the week */` |
|  ! 0 |  662 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|  ! 0 |  663 | `			break;` |
|  ! 0 |  664 | `		case 'w':` |
|    - |  665 | `			/* Numeric representation of the day of the week */` |
|  ! 0 |  666 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|  ! 0 |  667 | `			break;` |
|  ! 0 |  668 | `		case 'b':` |
|    - |  669 | `		case 'h':` |
|    - |  670 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|  ! 0 |  671 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|  ! 0 |  672 | `			break;` |
|  ! 0 |  673 | `		case 'B':` |
|    - |  674 | `			/* Full month name (Not based on locale) */` |
|  ! 0 |  675 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|  ! 0 |  676 | `			break;` |
|    2 |  677 | `		case 'm':` |
|    - |  678 | `			/*Numeric representation of a month, with leading zeros*/` |
|    5 |  679 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|    5 |  680 | `			break;` |
|  ! 0 |  681 | `		case 'C':` |
|    - |  682 | `			/* Two digit representation of the century */` |
|  ! 0 |  683 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|  ! 0 |  684 | `			break;` |
|  ! 0 |  685 | `		case 'y':` |
|    - |  686 | `		case 'g':` |
|    - |  687 | `			/* Two digit representation of the year */` |
|  ! 0 |  688 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|  ! 0 |  689 | `			break;` |
|    2 |  690 | `		case 'Y':` |
|    - |  691 | `		case 'G':` |
|    - |  692 | `			/* Four digit representation of the year */` |
|    5 |  693 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    5 |  694 | `			break;` |
|  ! 0 |  695 | `		case 'I':` |
|    - |  696 | `			/* 12-hour format of an hour with leading zeros */` |
|  ! 0 |  697 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|  ! 0 |  698 | `			break;` |
|  ! 0 |  699 | `		case 'l':` |
|    - |  700 | `			/* 12-hour format of an hour with leading space */` |
|  ! 0 |  701 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|  ! 0 |  702 | `			break;` |
|    1 |  703 | `		case 'H':` |
|    - |  704 | `			/* 24-hour format of an hour with leading zeros */` |
|    3 |  705 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|    3 |  706 | `			break;` |
|    1 |  707 | `		case 'M':` |
|    - |  708 | `			/* Minutes with leading zeros */` |
|    3 |  709 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|    3 |  710 | `			break;` |
|  ! 0 |  711 | `		case 'S':` |
|    - |  712 | `			/* Seconds with leading zeros */` |
|  ! 0 |  713 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|  ! 0 |  714 | `			break;` |
|  ! 0 |  715 | `		case 'z':` |
|    - |  716 | `		case 'Z':` |
|    - |  717 | `			/* 	Timezone identifier */` |
|  ! 0 |  718 | `			zCur = pTm->tm_zone;` |
|  ! 0 |  719 | `			if( zCur == 0 ){` |
|    - |  720 | `				/* Assume GMT */` |
|  ! 0 |  721 | `				zCur = "GMT";` |
|  ! 0 |  722 | `			}` |
|  ! 0 |  723 | `			ph7_result_string(pCtx,zCur,-1);` |
|  ! 0 |  724 | `			break;` |
|  ! 0 |  725 | `		case 'T':` |
|    - |  726 | `		case 'X':` |
|    - |  727 | `			/* Same as "%H:%M:%S" */` |
|  ! 0 |  728 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|  ! 0 |  729 | `			break;` |
|  ! 0 |  730 | `		case 'R':` |
|    - |  731 | `			/* Same as "%H:%M" */` |
|  ! 0 |  732 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|  ! 0 |  733 | `			break;` |
|  ! 0 |  734 | `		case 'P':` |
|    - |  735 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|  ! 0 |  736 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|  ! 0 |  737 | `			break;` |
|  ! 0 |  738 | `		case 'p':` |
|    - |  739 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|  ! 0 |  740 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|  ! 0 |  741 | `			break;` |
|  ! 0 |  742 | `		case 'r':` |
|    - |  743 | `			/* Same as "%I:%M:%S %p" */` |
|  ! 0 |  744 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|  ! 0 |  745 | `				1+(pTm->tm_hour%12),` |
|  ! 0 |  746 | `				pTm->tm_min,` |
|  ! 0 |  747 | `				pTm->tm_sec,` |
|  ! 0 |  748 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|    - |  749 | `				);` |
|  ! 0 |  750 | `			break;` |
|    1 |  751 | `		case 'D':` |
|    - |  752 | `		case 'x':` |
|    - |  753 | `			/* Same as "%m/%d/%y" */` |
|    4 |  754 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|    2 |  755 | `				pTm->tm_mon+1,` |
|    1 |  756 | `				pTm->tm_mday,` |
|    2 |  757 | `				pTm->tm_year%100` |
|    - |  758 | `				);` |
|    3 |  759 | `			break;` |
|  ! 0 |  760 | `		case 'F':` |
|    - |  761 | `			/* Same as "%Y-%m-%d" */` |
|  ! 0 |  762 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|  ! 0 |  763 | `				pTm->tm_year,` |
|  ! 0 |  764 | `				pTm->tm_mon+1,` |
|  ! 0 |  765 | `				pTm->tm_mday` |
|    - |  766 | `				);` |
|  ! 0 |  767 | `			break;` |
|  ! 0 |  768 | `		case 'c':` |
|  ! 0 |  769 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|  ! 0 |  770 | `				pTm->tm_year,` |
|  ! 0 |  771 | `				pTm->tm_mon+1,` |
|  ! 0 |  772 | `				pTm->tm_mday,` |
|  ! 0 |  773 | `				pTm->tm_hour,` |
|  ! 0 |  774 | `				pTm->tm_min,` |
|  ! 0 |  775 | `				pTm->tm_sec` |
|    - |  776 | `				);` |
|  ! 0 |  777 | `			break;` |
|  ! 0 |  778 | `		case 's':{` |
|    - |  779 | `			time_t tt;` |
|    - |  780 | `			/* Seconds since the Unix Epoch */` |
|  ! 0 |  781 | `			time(&tt);` |
|  ! 0 |  782 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|  ! 0 |  783 | `			break;` |
|    - |  784 | `				 }` |
|  ! 0 |  785 | `		default:` |
|    - |  786 | `			/* unknown specifer,simply ignore*/` |
|  ! 0 |  787 | `			break;` |
|    - |  788 | `		}` |
|    - |  789 | `		/* Advance the cursor */` |
|   21 |  790 | `		zIn++;` |
|    1 |  791 | `	}` |
|   17 |  792 | `	return SXRET_OK;` |
|    1 |  793 | `}` |
|    - |  794 | `/*` |
|    - |  795 | ` * Resolve a date()/gmdate() $timestamp argument under php 8's ?int weak ZPP:` |
|    - |  796 | ` *   - null            -> *pbUseNow = 1 (caller uses the current time)` |
|    - |  797 | ` *   - int/bool/float  -> coerce to a Unix timestamp (float truncates; php's` |
|    - |  798 | ` *                        float->int precision E_DEPRECATED is not emitted, §3.7)` |
|    - |  799 | ` *   - numeric string  -> coerce via php's is_numeric_string grammar` |
|    - |  800 | ` *                        (RangeStrToNumber: " 100 "/"1e3"/".5"/"+5" ok)` |
|    - |  801 | ` *   - anything else (non-numeric string, array, object, resource)` |
|    - |  802 | ` *                     -> catchable TypeError, byte-exact with php.` |
|    - |  803 | ` * Returns PH7_OK with *pbUseNow / *pT set, or the PH7_VmThrowException status.` |
|    - |  804 | ` */` |
|   40 |  805 | `static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)` |
|    1 |  806 | `{` |
|    - |  807 | `	char zBuf[64];` |
|   41 |  808 | `	*pbUseNow = 0;` |
|   41 |  809 | `	if( ph7_value_is_null(pArg) ){` |
|    3 |  810 | `		*pbUseNow = 1;` |
|    3 |  811 | `		return PH7_OK;` |
|    - |  812 | `	}` |
|   39 |  813 | `	if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_float(pArg) ){` |
|   15 |  814 | `		*pT = (time_t)ph7_value_to_int64(pArg);` |
|   15 |  815 | `		return PH7_OK;` |
|    - |  816 | `	}` |
|   25 |  817 | `	if( ph7_value_is_string(pArg) ){` |
|    - |  818 | `		int nStr;` |
|   19 |  819 | `		const char *zStr = ph7_value_to_string(pArg,&nStr);` |
|    - |  820 | `		sxi64 iLong; double dReal;` |
|   19 |  821 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|   19 |  822 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|    3 |  823 | `			*pT = (time_t)dReal;` |
|    6 |  824 | `			return PH7_OK;` |
|    - |  825 | `		}` |
|   17 |  826 | `		if( iKind == RANGE_IN_LONG ){` |
|    7 |  827 | `			*pT = (time_t)iLong;` |
|    7 |  828 | `			return PH7_OK;` |
|    - |  829 | `		}` |
|    - |  830 | `		/* Not a numeric string: fall through to the TypeError. */` |
|    5 |  831 | `	}` |
|   25 |  832 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  833 | `		"%s(): Argument #2 ($timestamp) must be of type ?int, %s given",` |
|    8 |  834 | `		ph7_function_name(pCtx),VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|   21 |  835 | `}` |
|    - |  836 | `/*` |
|    - |  837 | ` * string date(string $format [, int $timestamp = time() ] )` |
|    - |  838 | ` *  Returns a string formatted according to the given format string using` |
|    - |  839 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|    - |  840 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|    - |  841 | ` * Parameters` |
|    - |  842 | ` *  $format` |
|    - |  843 | ` *   The format of the outputted date string (See code above)` |
|    - |  844 | ` * $timestamp` |
|    - |  845 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  846 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  847 | ` *   In other words, it defaults to the value of time().` |
|    - |  848 | ` * Return` |
|    - |  849 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  850 | ` */` |
|   44 |  851 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  852 | `{` |
|    - |  853 | `	const char *zFormat;` |
|    - |  854 | `	int nLen;` |
|    - |  855 | `	Sytm sTm;` |
|   45 |  856 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  857 | `		/* Missing/Invalid argument,return FALSE */` |
|  ! 0 |  858 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  859 | `		return PH7_OK;` |
|    - |  860 | `	}` |
|   45 |  861 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   45 |  862 | `	if( nLen < 1 ){` |
|    - |  863 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  864 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  865 | `	}` |
|   45 |  866 | `	if( nArg < 2 ){` |
|    - |  867 | `#ifdef __WINNT__` |
|    - |  868 | `		SYSTEMTIME sOS;` |
|    1 |  869 | `		GetSystemTime(&sOS);` |
|    1 |  870 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  871 | `#else` |
|    - |  872 | `		struct tm *pTm;` |
|    - |  873 | `		time_t t;` |
|   30 |  874 | `		time(&t);` |
|   30 |  875 | `		pTm = localtime(&t);` |
|   30 |  876 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  877 | `#endif` |
|   16 |  878 | `	}else{` |
|    - |  879 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|   15 |  880 | `		time_t t = 0;` |
|    - |  881 | `		struct tm *pTm;` |
|    - |  882 | `		int bUseNow;` |
|   15 |  883 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|   15 |  884 | `		if( rc != PH7_OK ){` |
|   13 |  885 | `			return rc;` |
|    - |  886 | `		}` |
|    3 |  887 | `		if( bUseNow ){` |
|  ! 0 |  888 | `			time(&t);` |
|  ! 0 |  889 | `		}` |
|    3 |  890 | `		pTm = localtime(&t);` |
|    3 |  891 | `		if( pTm == 0 ){` |
|  ! 0 |  892 | `			time(&t);` |
|  ! 0 |  893 | `			pTm = localtime(&t);` |
|  ! 0 |  894 | `		}` |
|    3 |  895 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  896 | `	}` |
|    - |  897 | `	/* Format the given string */` |
|   33 |  898 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|   33 |  899 | `	return PH7_OK;` |
|   23 |  900 | `}` |
|    - |  901 | `/*` |
|    - |  902 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|    - |  903 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|    - |  904 | ` * Parameters` |
|    - |  905 | ` *  $format` |
|    - |  906 | ` *   The format of the outputted date string (See code above)` |
|    - |  907 | ` * $timestamp` |
|    - |  908 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  909 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  910 | ` *   In other words, it defaults to the value of time().` |
|    - |  911 | ` * Return` |
|    - |  912 | ` * Returns a string formatted according format using the given timestamp` |
|    - |  913 | ` * or the current local time if no timestamp is given.` |
|    - |  914 | ` */` |
|   20 |  915 | `PH7_PRIVATE int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  916 | `{` |
|    - |  917 | `	const char *zFormat;` |
|    - |  918 | `	int nLen;` |
|    - |  919 | `	Sytm sTm;` |
|   21 |  920 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  921 | `		/* Missing/Invalid argument,return FALSE */` |
|    5 |  922 | `		ph7_result_bool(pCtx,0);` |
|    5 |  923 | `		return PH7_OK;` |
|    - |  924 | `	}` |
|   17 |  925 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   17 |  926 | `	if( nLen < 1 ){` |
|    - |  927 | `		/* Don't bother processing return FALSE */` |
|  ! 0 |  928 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  929 | `	}` |
|   17 |  930 | `	if( nArg < 2 ){` |
|    - |  931 | `#ifdef __WINNT__` |
|    - |  932 | `		SYSTEMTIME sOS;` |
|    1 |  933 | `		GetSystemTime(&sOS);` |
|    1 |  934 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  935 | `#else` |
|    - |  936 | `		struct tm *pTm;` |
|    - |  937 | `		time_t t;` |
|   14 |  938 | `		time(&t);` |
|   14 |  939 | `		pTm = localtime(&t);` |
|   14 |  940 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  941 | `#endif` |
|    8 |  942 | `	}else{` |
|    - |  943 | `		/* Use the given timestamp */` |
|    - |  944 | `		time_t t;` |
|    - |  945 | `		struct tm *pTm;` |
|    3 |  946 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    3 |  947 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    3 |  948 | `			pTm = localtime(&t);` |
|    3 |  949 | `			if( pTm == 0 ){` |
|  ! 0 |  950 | `				time(&t);` |
|  ! 0 |  951 | `			}` |
|    2 |  952 | `		}else{` |
|  ! 0 |  953 | `			time(&t);` |
|    - |  954 | `		}` |
|    3 |  955 | `		pTm = localtime(&t);` |
|    3 |  956 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  957 | `	}` |
|    - |  958 | `	/* Format the given string */` |
|   17 |  959 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|   17 |  960 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|    - |  961 | `		/* Nothing was formatted,return FALSE */` |
|  ! 0 |  962 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  963 | `	}` |
|   17 |  964 | `	return PH7_OK;` |
|   11 |  965 | `}` |
|    - |  966 | `/*` |
|    - |  967 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|    - |  968 | ` *  Identical to the date() function except that the time returned` |
|    - |  969 | ` *  is Greenwich Mean Time (GMT).` |
|    - |  970 | ` * Parameters` |
|    - |  971 | ` *  $format` |
|    - |  972 | ` *  The format of the outputted date string (See code above)` |
|    - |  973 | ` *  $timestamp` |
|    - |  974 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  975 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  976 | ` *   In other words, it defaults to the value of time().` |
|    - |  977 | ` * Return` |
|    - |  978 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  979 | ` */` |
|   40 |  980 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  981 | `{` |
|    - |  982 | `	const char *zFormat;` |
|    - |  983 | `	int nLen;` |
|    - |  984 | `	Sytm sTm;` |
|   41 |  985 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  986 | `		/* Missing/Invalid argument,return FALSE */` |
|  ! 0 |  987 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  988 | `		return PH7_OK;` |
|    - |  989 | `	}` |
|   41 |  990 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   41 |  991 | `	if( nLen < 1 ){` |
|    - |  992 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  993 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  994 | `	}` |
|   41 |  995 | `	if( nArg < 2 ){` |
|    - |  996 | `#ifdef __WINNT__` |
|    - |  997 | `		SYSTEMTIME sOS;` |
|    1 |  998 | `		GetSystemTime(&sOS);` |
|    1 |  999 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - | 1000 | `#else` |
|    - | 1001 | `		struct tm *pTm;` |
|    - | 1002 | `		time_t t;` |
|   14 | 1003 | `		time(&t);` |
|   14 | 1004 | `		pTm = gmtime(&t);` |
|   14 | 1005 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1006 | `#endif` |
|    8 | 1007 | `	}else{` |
|    - | 1008 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|   27 | 1009 | `		time_t t = 0;` |
|    - | 1010 | `		struct tm *pTm;` |
|    - | 1011 | `		int bUseNow;` |
|   27 | 1012 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|   27 | 1013 | `		if( rc != PH7_OK ){` |
|    5 | 1014 | `			return rc;` |
|    - | 1015 | `		}` |
|   23 | 1016 | `		if( bUseNow ){` |
|    3 | 1017 | `			time(&t);` |
|    1 | 1018 | `		}` |
|   23 | 1019 | `		pTm = gmtime(&t);` |
|   23 | 1020 | `		if( pTm == 0 ){` |
|  ! 0 | 1021 | `			time(&t);` |
|  ! 0 | 1022 | `			pTm = gmtime(&t);` |
|  ! 0 | 1023 | `		}` |
|   23 | 1024 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1025 | `	}` |
|    - | 1026 | `	/* Format the given string */` |
|   37 | 1027 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|   37 | 1028 | `	return PH7_OK;` |
|   21 | 1029 | `}` |
|    - | 1030 | `/*` |
|    - | 1031 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|    - | 1032 | ` *  Return the local time.` |
|    - | 1033 | ` * Parameter` |
|    - | 1034 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|    - | 1035 | ` *     that defaults to the current local time if a timestamp is not given.` |
|    - | 1036 | ` *     In other words, it defaults to the value of time().` |
|    - | 1037 | ` * $is_associative` |
|    - | 1038 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|    - | 1039 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|    - | 1040 | ` *   array containing all the different elements of the structure returned by the C function` |
|    - | 1041 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|    - | 1042 | ` *      "tm_sec" - seconds, 0 to 59` |
|    - | 1043 | ` *      "tm_min" - minutes, 0 to 59` |
|    - | 1044 | ` *      "tm_hour" - hours, 0 to 23` |
|    - | 1045 | ` *      "tm_mday" - day of the month, 1 to 31` |
|    - | 1046 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|    - | 1047 | ` *      "tm_year" - years since 1900` |
|    - | 1048 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|    - | 1049 | ` *      "tm_yday" - day of the year, 0 to 365` |
|    - | 1050 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|    - | 1051 | ` * Returns` |
|    - | 1052 | ` *  An associative array of information related to the timestamp.` |
|    - | 1053 | ` */` |
|    8 | 1054 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1055 | `{` |
|    - | 1056 | `	ph7_value *pValue,*pArray;` |
|    9 | 1057 | `	int isAssoc = 0;` |
|    - | 1058 | `	Sytm sTm;` |
|    9 | 1059 | `	if( nArg < 1 ){` |
|    - | 1060 | `#ifdef __WINNT__` |
|    - | 1061 | `		SYSTEMTIME sOS;` |
|    1 | 1062 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|    1 | 1063 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - | 1064 | `#else` |
|    - | 1065 | `		struct tm *pTm;` |
|    - | 1066 | `		time_t t;` |
|    4 | 1067 | `		time(&t);` |
|    4 | 1068 | `		pTm = localtime(&t);` |
|    4 | 1069 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1070 | `#endif` |
|    3 | 1071 | `	}else{` |
|    - | 1072 | `		/* Use the given timestamp */` |
|    - | 1073 | `		time_t t;` |
|    - | 1074 | `		struct tm *pTm;` |
|    5 | 1075 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    5 | 1076 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|    5 | 1077 | `			pTm = localtime(&t);` |
|    5 | 1078 | `			if( pTm == 0 ){` |
|  ! 0 | 1079 | `				time(&t);` |
|  ! 0 | 1080 | `			}` |
|    3 | 1081 | `		}else{` |
|  ! 0 | 1082 | `			time(&t);` |
|    - | 1083 | `		}` |
|    5 | 1084 | `		pTm = localtime(&t);` |
|    5 | 1085 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1086 | `	}` |
|    - | 1087 | `	/* Element value */` |
|    9 | 1088 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    9 | 1089 | `	if( pValue == 0 ){` |
|    - | 1090 | `		/* Return NULL */` |
|  ! 0 | 1091 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1092 | `		return PH7_OK;` |
|    - | 1093 | `	}` |
|    - | 1094 | `	/* Create a new array */` |
|    9 | 1095 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 | 1096 | `	if( pArray == 0 ){` |
|    - | 1097 | `		/* Return NULL */` |
|  ! 0 | 1098 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1099 | `		return PH7_OK;` |
|    - | 1100 | `	}` |
|    9 | 1101 | `	if( nArg > 1 ){` |
|    3 | 1102 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|    1 | 1103 | `	}` |
|    - | 1104 | `	/* Fill the array */` |
|    - | 1105 | `	/* Seconds */` |
|    9 | 1106 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    9 | 1107 | `	if( isAssoc ){` |
|    3 | 1108 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|    2 | 1109 | `	}else{` |
|    7 | 1110 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1111 | `	}` |
|    - | 1112 | `	/* Minutes */` |
|    9 | 1113 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    9 | 1114 | `	if( isAssoc ){` |
|    3 | 1115 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|    2 | 1116 | `	}else{` |
|    7 | 1117 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1118 | `	}` |
|    - | 1119 | `	/* Hours */` |
|    9 | 1120 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    9 | 1121 | `	if( isAssoc ){` |
|    3 | 1122 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|    2 | 1123 | `	}else{` |
|    7 | 1124 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1125 | `	}` |
|    - | 1126 | `	/* mday */` |
|    9 | 1127 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    9 | 1128 | `	if( isAssoc ){` |
|    3 | 1129 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|    2 | 1130 | `	}else{` |
|    7 | 1131 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1132 | `	}` |
|    - | 1133 | `	/* mon */` |
|    9 | 1134 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|    9 | 1135 | `	if( isAssoc ){` |
|    3 | 1136 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|    2 | 1137 | `	}else{` |
|    7 | 1138 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1139 | `	}` |
|    - | 1140 | `	/* year since 1900 */` |
|    9 | 1141 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|    9 | 1142 | `	if( isAssoc ){` |
|    3 | 1143 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|    2 | 1144 | `	}else{` |
|    7 | 1145 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1146 | `	}` |
|    - | 1147 | `	/* wday */` |
|    9 | 1148 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    9 | 1149 | `	if( isAssoc ){` |
|    3 | 1150 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|    2 | 1151 | `	}else{` |
|    7 | 1152 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1153 | `	}` |
|    - | 1154 | `	/* yday */` |
|    9 | 1155 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    9 | 1156 | `	if( isAssoc ){` |
|    3 | 1157 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|    2 | 1158 | `	}else{` |
|    7 | 1159 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1160 | `	}` |
|    - | 1161 | `	/* isdst */` |
|    - | 1162 | `#ifdef __WINNT__` |
|    - | 1163 | `#ifdef _MSC_VER` |
|    - | 1164 | `#ifndef _WIN32_WCE` |
|    1 | 1165 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1166 | `#endif` |
|    - | 1167 | `#endif` |
|    - | 1168 | `#endif` |
|    9 | 1169 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|    9 | 1170 | `	if( isAssoc ){` |
|    3 | 1171 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|    2 | 1172 | `	}else{` |
|    7 | 1173 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1174 | `	}` |
|    - | 1175 | `	/* Return the array */` |
|    9 | 1176 | `	ph7_result_value(pCtx,pArray);` |
|    9 | 1177 | `	return PH7_OK;` |
|    5 | 1178 | `}` |
|    - | 1179 | `/*` |
|    - | 1180 | ` * int idate(string $format [, int $timestamp = time() ])` |
|    - | 1181 | ` *  Returns a number formatted according to the given format string` |
|    - | 1182 | ` *  using the given integer timestamp or the current local time if` |
|    - | 1183 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|    - | 1184 | ` *  to the value of time().` |
|    - | 1185 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|    - | 1186 | ` *  parameter.` |
|    - | 1187 | ` * $Parameters` |
|    - | 1188 | ` *  Supported format` |
|    - | 1189 | ` *   d 	Day of the month` |
|    - | 1190 | ` *   h 	Hour (12 hour format)` |
|    - | 1191 | ` *   H 	Hour (24 hour format)` |
|    - | 1192 | ` *   i 	Minutes` |
|    - | 1193 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|    - | 1194 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|    - | 1195 | ` *   m 	Month number` |
|    - | 1196 | ` *   s 	Seconds` |
|    - | 1197 | ` *   t 	Days in current month` |
|    - | 1198 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|    - | 1199 | ` *   w 	Day of the week (0 on Sunday)` |
|    - | 1200 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|    - | 1201 | ` *   y 	Year (1 or 2 digits - check note below)` |
|    - | 1202 | ` *   Y 	Year (4 digits)` |
|    - | 1203 | ` *   z 	Day of the year` |
|    - | 1204 | ` *   Z 	Timezone offset in seconds` |
|    - | 1205 | ` * $timestamp` |
|    - | 1206 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|    - | 1207 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|    - | 1208 | ` *  to the value of time().` |
|    - | 1209 | ` * Return` |
|    - | 1210 | ` *  An integer.` |
|    - | 1211 | ` */` |
|   38 | 1212 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1213 | `{` |
|    - | 1214 | `	const char *zFormat;` |
|   40 | 1215 | `	ph7_int64 iVal = 0;` |
|    - | 1216 | `	int nLen;` |
|    - | 1217 | `	Sytm sTm;` |
|   40 | 1218 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1219 | `		/* Missing/Invalid argument,return -1 */` |
|  ! 0 | 1220 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1221 | `		return PH7_OK;` |
|    - | 1222 | `	}` |
|   40 | 1223 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   40 | 1224 | `	if( nLen < 1 ){` |
|    - | 1225 | `		/* Don't bother processing return -1*/` |
|  ! 0 | 1226 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1227 | `	}` |
|   40 | 1228 | `	if( nArg < 2 ){` |
|    - | 1229 | `#ifdef __WINNT__` |
|    - | 1230 | `		SYSTEMTIME sOS;` |
|    2 | 1231 | `		GetSystemTime(&sOS);` |
|    2 | 1232 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - | 1233 | `#else` |
|    - | 1234 | `		struct tm *pTm;` |
|    - | 1235 | `		time_t t;` |
|   28 | 1236 | `		time(&t);` |
|   28 | 1237 | `		pTm = localtime(&t);` |
|   28 | 1238 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1239 | `#endif` |
|   16 | 1240 | `	}else{` |
|    - | 1241 | `		/* Use the given timestamp */` |
|    - | 1242 | `		time_t t;` |
|    - | 1243 | `		struct tm *pTm;` |
|   11 | 1244 | `		if( ph7_value_is_int(apArg[1]) ){` |
|   11 | 1245 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|   11 | 1246 | `			pTm = localtime(&t);` |
|   11 | 1247 | `			if( pTm == 0 ){` |
|  ! 0 | 1248 | `				time(&t);` |
|  ! 0 | 1249 | `			}` |
|    6 | 1250 | `		}else{` |
|  ! 0 | 1251 | `			time(&t);` |
|    - | 1252 | `		}` |
|   11 | 1253 | `		pTm = localtime(&t);` |
|   11 | 1254 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1255 | `	}` |
|    - | 1256 | `	/* Perform the requested operation */` |
|   40 | 1257 | `	switch(zFormat[0]){` |
|    2 | 1258 | `	case 'd':` |
|    - | 1259 | `		/* Day of the month */` |
|    5 | 1260 | `		iVal = sTm.tm_mday;` |
|    5 | 1261 | `		break;` |
|  ! 0 | 1262 | `	case 'h':` |
|    - | 1263 | `		/*	Hour (12 hour format)*/` |
|  ! 0 | 1264 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|  ! 0 | 1265 | `		break;` |
|    1 | 1266 | `	case 'H':` |
|    - | 1267 | `		/* Hour (24 hour format)*/` |
|    3 | 1268 | `		iVal = sTm.tm_hour;` |
|    3 | 1269 | `		break;` |
|    1 | 1270 | `	case 'i':` |
|    - | 1271 | `		/*Minutes*/` |
|    3 | 1272 | `		iVal = sTm.tm_min;` |
|    3 | 1273 | `		break;` |
|    1 | 1274 | `	case 'I':` |
|    - | 1275 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|    - | 1276 | `#ifdef __WINNT__` |
|    - | 1277 | `#ifdef _MSC_VER` |
|    - | 1278 | `#ifndef _WIN32_WCE` |
|    1 | 1279 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1280 | `#endif` |
|    - | 1281 | `#endif` |
|    - | 1282 | `#endif` |
|    3 | 1283 | `		iVal = sTm.tm_isdst;` |
|    3 | 1284 | `		break;` |
|    1 | 1285 | `	case 'L':` |
|    - | 1286 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|    3 | 1287 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|    3 | 1288 | `		break;` |
|    2 | 1289 | `	case 'm':` |
|    - | 1290 | `		/* Month number*/` |
|    5 | 1291 | `		iVal = sTm.tm_mon;` |
|    5 | 1292 | `		break;` |
|    1 | 1293 | `	case 's':` |
|    - | 1294 | `		/*Seconds*/` |
|    3 | 1295 | `		iVal = sTm.tm_sec;` |
|    3 | 1296 | `		break;` |
|    1 | 1297 | `	case 't':{` |
|    - | 1298 | `		/*Days in current month*/` |
|    - | 1299 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    3 | 1300 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|    3 | 1301 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|  ! 0 | 1302 | `			nDays = 28;` |
|  ! 0 | 1303 | `		}` |
|    3 | 1304 | `		iVal = nDays;` |
|    3 | 1305 | `		break;` |
|    - | 1306 | `			 }` |
|    1 | 1307 | `	case 'U':` |
|    - | 1308 | `		/*Seconds since the Unix Epoch*/` |
|    3 | 1309 | `		iVal = (ph7_int64)time(0);` |
|    3 | 1310 | `		break;` |
|    1 | 1311 | `	case 'w':` |
|    - | 1312 | `		/*	Day of the week (0 on Sunday) */` |
|    3 | 1313 | `		iVal = sTm.tm_wday;` |
|    3 | 1314 | `		break;` |
|    1 | 1315 | `	case 'W': {` |
|    - | 1316 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|    - | 1317 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|    3 | 1318 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|    3 | 1319 | `		break;` |
|    - | 1320 | `			  }` |
|  ! 0 | 1321 | `	case 'y':` |
|    - | 1322 | `		/* Year (2 digits) */` |
|  ! 0 | 1323 | `		iVal = sTm.tm_year % 100;` |
|  ! 0 | 1324 | `		break;` |
|    3 | 1325 | `	case 'Y':` |
|    - | 1326 | `		/* Year (4 digits) */` |
|    7 | 1327 | `		iVal = sTm.tm_year;` |
|    7 | 1328 | `		break;` |
|    1 | 1329 | `	case 'z':` |
|    - | 1330 | `		/* Day of the year */` |
|    3 | 1331 | `		iVal = sTm.tm_yday;` |
|    3 | 1332 | `		break;` |
|    1 | 1333 | `	case 'Z':` |
|    - | 1334 | `		/*Timezone offset in seconds*/` |
|    3 | 1335 | `		iVal = sTm.tm_gmtoff;` |
|    3 | 1336 | `		break;` |
|    1 | 1337 | `	default:` |
|    - | 1338 | `		/* unknown format,throw a warning */` |
|    3 | 1339 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|    2 | 1340 | `		break;` |
|    - | 1341 | `	}` |
|    - | 1342 | `	/* Return the time value */` |
|   40 | 1343 | `	ph7_result_int64(pCtx,iVal);` |
|   40 | 1344 | `	return PH7_OK;` |
|   21 | 1345 | `}` |
|    - | 1346 | `/*` |
|    - | 1347 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|    - | 1348 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|    - | 1349 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|    - | 1350 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|    - | 1351 | ` *  specified.` |
|    - | 1352 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|    - | 1353 | ` *  the current value according to the local date and time.` |
|    - | 1354 | ` * Parameters` |
|    - | 1355 | ` * $hour` |
|    - | 1356 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|    - | 1357 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|    - | 1358 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|    - | 1359 | ` * $minute` |
|    - | 1360 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|    - | 1361 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|    - | 1362 | ` *  in the following hour(s).` |
|    - | 1363 | ` * $second` |
|    - | 1364 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|    - | 1365 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|    - | 1366 | ` * second in the following minute(s).` |
|    - | 1367 | ` * $month` |
|    - | 1368 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|    - | 1369 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|    - | 1370 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|    - | 1371 | ` * $day` |
|    - | 1372 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|    - | 1373 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|    - | 1374 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|    - | 1375 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|    - | 1376 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|    - | 1377 | ` * $year` |
|    - | 1378 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|    - | 1379 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|    - | 1380 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|    - | 1381 | ` * $is_dst` |
|    - | 1382 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|    - | 1383 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|    - | 1384 | ` * Return` |
|    - | 1385 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|    - | 1386 | ` *   If the arguments are invalid, the function returns FALSE` |
|    - | 1387 | ` */` |
|   18 | 1388 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1389 | `{` |
|    - | 1390 | `	const char *zFunction;` |
|   19 | 1391 | `	ph7_int64 iVal = 0;` |
|    - | 1392 | `	struct tm *pTm;` |
|    - | 1393 | `	time_t t;` |
|    - | 1394 | `	/* Extract function name */` |
|   19 | 1395 | `	zFunction = ph7_function_name(pCtx);` |
|    - | 1396 | `	/* PHP 8 dropped the legacy $is_dst 7th parameter: mktime()/gmmktime() now` |
|    - | 1397 | `	 * accept at most 6 arguments and throw a catchable ArgumentCountError` |
|    - | 1398 | `	 * otherwise (the central aBuiltinArity table only enforces the minimum, so` |
|    - | 1399 | `	 * this maximum is checked here). */` |
|   19 | 1400 | `	if( nArg > 6 ){` |
|   10 | 1401 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    3 | 1402 | `			"%s() expects at most 6 arguments, %d given",zFunction,nArg);` |
|    - | 1403 | `	}` |
|    - | 1404 | `	/* Get the current time */` |
|   13 | 1405 | `	time(&t);` |
|   13 | 1406 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|    5 | 1407 | `		pTm = gmtime(&t);` |
|    3 | 1408 | `	}else{` |
|    - | 1409 | `		/* localtime */` |
|    9 | 1410 | `		pTm = localtime(&t);` |
|    - | 1411 | `	}` |
|   13 | 1412 | `	if( nArg > 0 ){` |
|    - | 1413 | `		int iTmp;` |
|    - | 1414 | `		/* Hour */` |
|   13 | 1415 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|   13 | 1416 | `		pTm->tm_hour = iTmp;` |
|   13 | 1417 | `		if( nArg > 1 ){` |
|    - | 1418 | `			/* Minutes */` |
|   13 | 1419 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|   13 | 1420 | `			pTm->tm_min = iTmp;` |
|   13 | 1421 | `			if( nArg > 2 ){` |
|    - | 1422 | `				/* Seconds */` |
|   13 | 1423 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|   13 | 1424 | `				pTm->tm_sec = iTmp;` |
|   13 | 1425 | `				if( nArg > 3 ){` |
|    - | 1426 | `					/* Month */` |
|   13 | 1427 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|   13 | 1428 | `					pTm->tm_mon = iTmp - 1;` |
|   13 | 1429 | `					if( nArg > 4 ){` |
|    - | 1430 | `						/* mday */` |
|   13 | 1431 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|   13 | 1432 | `						pTm->tm_mday = iTmp;` |
|   13 | 1433 | `						if( nArg > 5 ){` |
|    - | 1434 | `							/* Year */` |
|   13 | 1435 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|   13 | 1436 | `							if( iTmp > 1900 ){` |
|    9 | 1437 | `								iTmp -= 1900;` |
|    4 | 1438 | `							}` |
|   13 | 1439 | `							pTm->tm_year = iTmp;` |
|    6 | 1440 | `						}` |
|    6 | 1441 | `					}` |
|    6 | 1442 | `				}` |
|    6 | 1443 | `			}` |
|    6 | 1444 | `		}` |
|    6 | 1445 | `	}` |
|    - | 1446 | `	/* Make the time */` |
|   13 | 1447 | `	iVal = (ph7_int64)mktime(pTm);` |
|    - | 1448 | `	/* Return the timesatmp as a 64bit integer */` |
|   13 | 1449 | `	ph7_result_int64(pCtx,iVal);` |
|   13 | 1450 | `	return PH7_OK;` |
|   10 | 1451 | `}` |
|    - | 1452 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1453 |  |
