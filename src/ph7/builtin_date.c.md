# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 453/699 lines (64.81%)

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
|   46 |  346 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|    1 |  347 | `{` |
|   47 |  348 | `	const char *zEnd = &zIn[nLen];` |
|    - |  349 | `	const char *zCur;` |
|    - |  350 | `	/* Start the format process */` |
|   78 |  351 | `	for(;;){` |
|  157 |  352 | `		if( zIn >= zEnd ){` |
|    - |  353 | `			/* No more input to process */` |
|   47 |  354 | `			break;` |
|    - |  355 | `		}` |
|  111 |  356 | `		switch(zIn[0]){` |
|    7 |  357 | `		case 'd':` |
|    - |  358 | `			/* Day of the month, 2 digits with leading zeros */` |
|   15 |  359 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|   15 |  360 | `			break;` |
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
|    7 |  393 | `		case 'm':` |
|    - |  394 | `			/*Numeric representation of a month, with leading zeros*/` |
|   15 |  395 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|   15 |  396 | `			break;` |
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
|    9 |  426 | `		case 'Y':` |
|    - |  427 | `			/*	A full numeric representation of a year, 4 digits */` |
|   19 |  428 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|   19 |  429 | `			break;` |
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
|    3 |  454 | `		case 'H':` |
|    - |  455 | `			/*	24-hour format of an hour with leading zeros */` |
|    7 |  456 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|    7 |  457 | `			break;` |
|    3 |  458 | `		case 'i':` |
|    - |  459 | `			/* 	Minutes with leading zeros */` |
|    7 |  460 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|    7 |  461 | `			break;` |
|    3 |  462 | `		case 's':` |
|    - |  463 | `			/* 	second with leading zeros */` |
|    7 |  464 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    7 |  465 | `			break;` |
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
|   17 |  546 | `		default:` |
|    - |  547 | `			/* Unknown format specifer,expand verbatim */` |
|   35 |  548 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|   34 |  549 | `			break;` |
|    - |  550 | `		}` |
|    - |  551 | `		/* Point to the next character */` |
|  111 |  552 | `		zIn++;` |
|    1 |  553 | `	}` |
|   47 |  554 | `	return SXRET_OK;` |
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
|    - |  795 | ` * string date(string $format [, int $timestamp = time() ] )` |
|    - |  796 | ` *  Returns a string formatted according to the given format string using` |
|    - |  797 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|    - |  798 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|    - |  799 | ` * Parameters` |
|    - |  800 | ` *  $format` |
|    - |  801 | ` *   The format of the outputted date string (See code above)` |
|    - |  802 | ` * $timestamp` |
|    - |  803 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  804 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  805 | ` *   In other words, it defaults to the value of time().` |
|    - |  806 | ` * Return` |
|    - |  807 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  808 | ` */` |
|   32 |  809 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  810 | `{` |
|    - |  811 | `	const char *zFormat;` |
|    - |  812 | `	int nLen;` |
|    - |  813 | `	Sytm sTm;` |
|   33 |  814 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  815 | `		/* Missing/Invalid argument,return FALSE */` |
|  ! 0 |  816 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  817 | `		return PH7_OK;` |
|    - |  818 | `	}` |
|   33 |  819 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   33 |  820 | `	if( nLen < 1 ){` |
|    - |  821 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  822 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  823 | `	}` |
|   33 |  824 | `	if( nArg < 2 ){` |
|    - |  825 | `#ifdef __WINNT__` |
|    - |  826 | `		SYSTEMTIME sOS;` |
|    1 |  827 | `		GetSystemTime(&sOS);` |
|    1 |  828 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  829 | `#else` |
|    - |  830 | `		struct tm *pTm;` |
|    - |  831 | `		time_t t;` |
|   30 |  832 | `		time(&t);` |
|   30 |  833 | `		pTm = localtime(&t);` |
|   30 |  834 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  835 | `#endif` |
|   16 |  836 | `	}else{` |
|    - |  837 | `		/* Use the given timestamp */` |
|    - |  838 | `		time_t t;` |
|    - |  839 | `		struct tm *pTm;` |
|    3 |  840 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    3 |  841 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    3 |  842 | `			pTm = localtime(&t);` |
|    3 |  843 | `			if( pTm == 0 ){` |
|  ! 0 |  844 | `				time(&t);` |
|  ! 0 |  845 | `			}` |
|    2 |  846 | `		}else{` |
|  ! 0 |  847 | `			time(&t);` |
|    - |  848 | `		}` |
|    3 |  849 | `		pTm = localtime(&t);` |
|    3 |  850 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  851 | `	}` |
|    - |  852 | `	/* Format the given string */` |
|   33 |  853 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|   33 |  854 | `	return PH7_OK;` |
|   17 |  855 | `}` |
|    - |  856 | `/*` |
|    - |  857 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|    - |  858 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|    - |  859 | ` * Parameters` |
|    - |  860 | ` *  $format` |
|    - |  861 | ` *   The format of the outputted date string (See code above)` |
|    - |  862 | ` * $timestamp` |
|    - |  863 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  864 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  865 | ` *   In other words, it defaults to the value of time().` |
|    - |  866 | ` * Return` |
|    - |  867 | ` * Returns a string formatted according format using the given timestamp` |
|    - |  868 | ` * or the current local time if no timestamp is given.` |
|    - |  869 | ` */` |
|   20 |  870 | `PH7_PRIVATE int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  871 | `{` |
|    - |  872 | `	const char *zFormat;` |
|    - |  873 | `	int nLen;` |
|    - |  874 | `	Sytm sTm;` |
|   21 |  875 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  876 | `		/* Missing/Invalid argument,return FALSE */` |
|    5 |  877 | `		ph7_result_bool(pCtx,0);` |
|    5 |  878 | `		return PH7_OK;` |
|    - |  879 | `	}` |
|   17 |  880 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   17 |  881 | `	if( nLen < 1 ){` |
|    - |  882 | `		/* Don't bother processing return FALSE */` |
|  ! 0 |  883 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  884 | `	}` |
|   17 |  885 | `	if( nArg < 2 ){` |
|    - |  886 | `#ifdef __WINNT__` |
|    - |  887 | `		SYSTEMTIME sOS;` |
|    1 |  888 | `		GetSystemTime(&sOS);` |
|    1 |  889 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  890 | `#else` |
|    - |  891 | `		struct tm *pTm;` |
|    - |  892 | `		time_t t;` |
|   14 |  893 | `		time(&t);` |
|   14 |  894 | `		pTm = localtime(&t);` |
|   14 |  895 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  896 | `#endif` |
|    8 |  897 | `	}else{` |
|    - |  898 | `		/* Use the given timestamp */` |
|    - |  899 | `		time_t t;` |
|    - |  900 | `		struct tm *pTm;` |
|    3 |  901 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    3 |  902 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    3 |  903 | `			pTm = localtime(&t);` |
|    3 |  904 | `			if( pTm == 0 ){` |
|  ! 0 |  905 | `				time(&t);` |
|  ! 0 |  906 | `			}` |
|    2 |  907 | `		}else{` |
|  ! 0 |  908 | `			time(&t);` |
|    - |  909 | `		}` |
|    3 |  910 | `		pTm = localtime(&t);` |
|    3 |  911 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  912 | `	}` |
|    - |  913 | `	/* Format the given string */` |
|   17 |  914 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|   17 |  915 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|    - |  916 | `		/* Nothing was formatted,return FALSE */` |
|  ! 0 |  917 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  918 | `	}` |
|   17 |  919 | `	return PH7_OK;` |
|   11 |  920 | `}` |
|    - |  921 | `/*` |
|    - |  922 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|    - |  923 | ` *  Identical to the date() function except that the time returned` |
|    - |  924 | ` *  is Greenwich Mean Time (GMT).` |
|    - |  925 | ` * Parameters` |
|    - |  926 | ` *  $format` |
|    - |  927 | ` *  The format of the outputted date string (See code above)` |
|    - |  928 | ` *  $timestamp` |
|    - |  929 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  930 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  931 | ` *   In other words, it defaults to the value of time().` |
|    - |  932 | ` * Return` |
|    - |  933 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  934 | ` */` |
|   14 |  935 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  936 | `{` |
|    - |  937 | `	const char *zFormat;` |
|    - |  938 | `	int nLen;` |
|    - |  939 | `	Sytm sTm;` |
|   15 |  940 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  941 | `		/* Missing/Invalid argument,return FALSE */` |
|  ! 0 |  942 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  943 | `		return PH7_OK;` |
|    - |  944 | `	}` |
|   15 |  945 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   15 |  946 | `	if( nLen < 1 ){` |
|    - |  947 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  948 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  949 | `	}` |
|   15 |  950 | `	if( nArg < 2 ){` |
|    - |  951 | `#ifdef __WINNT__` |
|    - |  952 | `		SYSTEMTIME sOS;` |
|    1 |  953 | `		GetSystemTime(&sOS);` |
|    1 |  954 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  955 | `#else` |
|    - |  956 | `		struct tm *pTm;` |
|    - |  957 | `		time_t t;` |
|   12 |  958 | `		time(&t);` |
|   12 |  959 | `		pTm = gmtime(&t);` |
|   12 |  960 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  961 | `#endif` |
|    7 |  962 | `	}else{` |
|    - |  963 | `		/* Use the given timestamp */` |
|    - |  964 | `		time_t t;` |
|    - |  965 | `		struct tm *pTm;` |
|    3 |  966 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    3 |  967 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    3 |  968 | `			pTm = gmtime(&t);` |
|    3 |  969 | `			if( pTm == 0 ){` |
|  ! 0 |  970 | `				time(&t);` |
|  ! 0 |  971 | `			}` |
|    2 |  972 | `		}else{` |
|  ! 0 |  973 | `			time(&t);` |
|    - |  974 | `		}` |
|    3 |  975 | `		pTm = gmtime(&t);` |
|    3 |  976 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  977 | `	}` |
|    - |  978 | `	/* Format the given string */` |
|   15 |  979 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|   15 |  980 | `	return PH7_OK;` |
|    8 |  981 | `}` |
|    - |  982 | `/*` |
|    - |  983 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|    - |  984 | ` *  Return the local time.` |
|    - |  985 | ` * Parameter` |
|    - |  986 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|    - |  987 | ` *     that defaults to the current local time if a timestamp is not given.` |
|    - |  988 | ` *     In other words, it defaults to the value of time().` |
|    - |  989 | ` * $is_associative` |
|    - |  990 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|    - |  991 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|    - |  992 | ` *   array containing all the different elements of the structure returned by the C function` |
|    - |  993 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|    - |  994 | ` *      "tm_sec" - seconds, 0 to 59` |
|    - |  995 | ` *      "tm_min" - minutes, 0 to 59` |
|    - |  996 | ` *      "tm_hour" - hours, 0 to 23` |
|    - |  997 | ` *      "tm_mday" - day of the month, 1 to 31` |
|    - |  998 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|    - |  999 | ` *      "tm_year" - years since 1900` |
|    - | 1000 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|    - | 1001 | ` *      "tm_yday" - day of the year, 0 to 365` |
|    - | 1002 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|    - | 1003 | ` * Returns` |
|    - | 1004 | ` *  An associative array of information related to the timestamp.` |
|    - | 1005 | ` */` |
|    8 | 1006 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1007 | `{` |
|    - | 1008 | `	ph7_value *pValue,*pArray;` |
|    9 | 1009 | `	int isAssoc = 0;` |
|    - | 1010 | `	Sytm sTm;` |
|    9 | 1011 | `	if( nArg < 1 ){` |
|    - | 1012 | `#ifdef __WINNT__` |
|    - | 1013 | `		SYSTEMTIME sOS;` |
|    1 | 1014 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|    1 | 1015 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - | 1016 | `#else` |
|    - | 1017 | `		struct tm *pTm;` |
|    - | 1018 | `		time_t t;` |
|    4 | 1019 | `		time(&t);` |
|    4 | 1020 | `		pTm = localtime(&t);` |
|    4 | 1021 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1022 | `#endif` |
|    3 | 1023 | `	}else{` |
|    - | 1024 | `		/* Use the given timestamp */` |
|    - | 1025 | `		time_t t;` |
|    - | 1026 | `		struct tm *pTm;` |
|    5 | 1027 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    5 | 1028 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|    5 | 1029 | `			pTm = localtime(&t);` |
|    5 | 1030 | `			if( pTm == 0 ){` |
|  ! 0 | 1031 | `				time(&t);` |
|  ! 0 | 1032 | `			}` |
|    3 | 1033 | `		}else{` |
|  ! 0 | 1034 | `			time(&t);` |
|    - | 1035 | `		}` |
|    5 | 1036 | `		pTm = localtime(&t);` |
|    5 | 1037 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1038 | `	}` |
|    - | 1039 | `	/* Element value */` |
|    9 | 1040 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    9 | 1041 | `	if( pValue == 0 ){` |
|    - | 1042 | `		/* Return NULL */` |
|  ! 0 | 1043 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1044 | `		return PH7_OK;` |
|    - | 1045 | `	}` |
|    - | 1046 | `	/* Create a new array */` |
|    9 | 1047 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 | 1048 | `	if( pArray == 0 ){` |
|    - | 1049 | `		/* Return NULL */` |
|  ! 0 | 1050 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1051 | `		return PH7_OK;` |
|    - | 1052 | `	}` |
|    9 | 1053 | `	if( nArg > 1 ){` |
|    3 | 1054 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|    1 | 1055 | `	}` |
|    - | 1056 | `	/* Fill the array */` |
|    - | 1057 | `	/* Seconds */` |
|    9 | 1058 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    9 | 1059 | `	if( isAssoc ){` |
|    3 | 1060 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|    2 | 1061 | `	}else{` |
|    7 | 1062 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1063 | `	}` |
|    - | 1064 | `	/* Minutes */` |
|    9 | 1065 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    9 | 1066 | `	if( isAssoc ){` |
|    3 | 1067 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|    2 | 1068 | `	}else{` |
|    7 | 1069 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1070 | `	}` |
|    - | 1071 | `	/* Hours */` |
|    9 | 1072 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    9 | 1073 | `	if( isAssoc ){` |
|    3 | 1074 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|    2 | 1075 | `	}else{` |
|    7 | 1076 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1077 | `	}` |
|    - | 1078 | `	/* mday */` |
|    9 | 1079 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    9 | 1080 | `	if( isAssoc ){` |
|    3 | 1081 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|    2 | 1082 | `	}else{` |
|    7 | 1083 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1084 | `	}` |
|    - | 1085 | `	/* mon */` |
|    9 | 1086 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|    9 | 1087 | `	if( isAssoc ){` |
|    3 | 1088 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|    2 | 1089 | `	}else{` |
|    7 | 1090 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1091 | `	}` |
|    - | 1092 | `	/* year since 1900 */` |
|    9 | 1093 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|    9 | 1094 | `	if( isAssoc ){` |
|    3 | 1095 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|    2 | 1096 | `	}else{` |
|    7 | 1097 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1098 | `	}` |
|    - | 1099 | `	/* wday */` |
|    9 | 1100 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    9 | 1101 | `	if( isAssoc ){` |
|    3 | 1102 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|    2 | 1103 | `	}else{` |
|    7 | 1104 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1105 | `	}` |
|    - | 1106 | `	/* yday */` |
|    9 | 1107 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    9 | 1108 | `	if( isAssoc ){` |
|    3 | 1109 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|    2 | 1110 | `	}else{` |
|    7 | 1111 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1112 | `	}` |
|    - | 1113 | `	/* isdst */` |
|    - | 1114 | `#ifdef __WINNT__` |
|    - | 1115 | `#ifdef _MSC_VER` |
|    - | 1116 | `#ifndef _WIN32_WCE` |
|    1 | 1117 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1118 | `#endif` |
|    - | 1119 | `#endif` |
|    - | 1120 | `#endif` |
|    9 | 1121 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|    9 | 1122 | `	if( isAssoc ){` |
|    3 | 1123 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|    2 | 1124 | `	}else{` |
|    7 | 1125 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1126 | `	}` |
|    - | 1127 | `	/* Return the array */` |
|    9 | 1128 | `	ph7_result_value(pCtx,pArray);` |
|    9 | 1129 | `	return PH7_OK;` |
|    5 | 1130 | `}` |
|    - | 1131 | `/*` |
|    - | 1132 | ` * int idate(string $format [, int $timestamp = time() ])` |
|    - | 1133 | ` *  Returns a number formatted according to the given format string` |
|    - | 1134 | ` *  using the given integer timestamp or the current local time if` |
|    - | 1135 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|    - | 1136 | ` *  to the value of time().` |
|    - | 1137 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|    - | 1138 | ` *  parameter.` |
|    - | 1139 | ` * $Parameters` |
|    - | 1140 | ` *  Supported format` |
|    - | 1141 | ` *   d 	Day of the month` |
|    - | 1142 | ` *   h 	Hour (12 hour format)` |
|    - | 1143 | ` *   H 	Hour (24 hour format)` |
|    - | 1144 | ` *   i 	Minutes` |
|    - | 1145 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|    - | 1146 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|    - | 1147 | ` *   m 	Month number` |
|    - | 1148 | ` *   s 	Seconds` |
|    - | 1149 | ` *   t 	Days in current month` |
|    - | 1150 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|    - | 1151 | ` *   w 	Day of the week (0 on Sunday)` |
|    - | 1152 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|    - | 1153 | ` *   y 	Year (1 or 2 digits - check note below)` |
|    - | 1154 | ` *   Y 	Year (4 digits)` |
|    - | 1155 | ` *   z 	Day of the year` |
|    - | 1156 | ` *   Z 	Timezone offset in seconds` |
|    - | 1157 | ` * $timestamp` |
|    - | 1158 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|    - | 1159 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|    - | 1160 | ` *  to the value of time().` |
|    - | 1161 | ` * Return` |
|    - | 1162 | ` *  An integer.` |
|    - | 1163 | ` */` |
|   38 | 1164 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1165 | `{` |
|    - | 1166 | `	const char *zFormat;` |
|   40 | 1167 | `	ph7_int64 iVal = 0;` |
|    - | 1168 | `	int nLen;` |
|    - | 1169 | `	Sytm sTm;` |
|   40 | 1170 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1171 | `		/* Missing/Invalid argument,return -1 */` |
|  ! 0 | 1172 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1173 | `		return PH7_OK;` |
|    - | 1174 | `	}` |
|   40 | 1175 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   40 | 1176 | `	if( nLen < 1 ){` |
|    - | 1177 | `		/* Don't bother processing return -1*/` |
|  ! 0 | 1178 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1179 | `	}` |
|   40 | 1180 | `	if( nArg < 2 ){` |
|    - | 1181 | `#ifdef __WINNT__` |
|    - | 1182 | `		SYSTEMTIME sOS;` |
|    2 | 1183 | `		GetSystemTime(&sOS);` |
|    2 | 1184 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - | 1185 | `#else` |
|    - | 1186 | `		struct tm *pTm;` |
|    - | 1187 | `		time_t t;` |
|   28 | 1188 | `		time(&t);` |
|   28 | 1189 | `		pTm = localtime(&t);` |
|   28 | 1190 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1191 | `#endif` |
|   16 | 1192 | `	}else{` |
|    - | 1193 | `		/* Use the given timestamp */` |
|    - | 1194 | `		time_t t;` |
|    - | 1195 | `		struct tm *pTm;` |
|   11 | 1196 | `		if( ph7_value_is_int(apArg[1]) ){` |
|   11 | 1197 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|   11 | 1198 | `			pTm = localtime(&t);` |
|   11 | 1199 | `			if( pTm == 0 ){` |
|  ! 0 | 1200 | `				time(&t);` |
|  ! 0 | 1201 | `			}` |
|    6 | 1202 | `		}else{` |
|  ! 0 | 1203 | `			time(&t);` |
|    - | 1204 | `		}` |
|   11 | 1205 | `		pTm = localtime(&t);` |
|   11 | 1206 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1207 | `	}` |
|    - | 1208 | `	/* Perform the requested operation */` |
|   40 | 1209 | `	switch(zFormat[0]){` |
|    2 | 1210 | `	case 'd':` |
|    - | 1211 | `		/* Day of the month */` |
|    5 | 1212 | `		iVal = sTm.tm_mday;` |
|    5 | 1213 | `		break;` |
|  ! 0 | 1214 | `	case 'h':` |
|    - | 1215 | `		/*	Hour (12 hour format)*/` |
|  ! 0 | 1216 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|  ! 0 | 1217 | `		break;` |
|    1 | 1218 | `	case 'H':` |
|    - | 1219 | `		/* Hour (24 hour format)*/` |
|    3 | 1220 | `		iVal = sTm.tm_hour;` |
|    3 | 1221 | `		break;` |
|    1 | 1222 | `	case 'i':` |
|    - | 1223 | `		/*Minutes*/` |
|    3 | 1224 | `		iVal = sTm.tm_min;` |
|    3 | 1225 | `		break;` |
|    1 | 1226 | `	case 'I':` |
|    - | 1227 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|    - | 1228 | `#ifdef __WINNT__` |
|    - | 1229 | `#ifdef _MSC_VER` |
|    - | 1230 | `#ifndef _WIN32_WCE` |
|    1 | 1231 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1232 | `#endif` |
|    - | 1233 | `#endif` |
|    - | 1234 | `#endif` |
|    3 | 1235 | `		iVal = sTm.tm_isdst;` |
|    3 | 1236 | `		break;` |
|    1 | 1237 | `	case 'L':` |
|    - | 1238 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|    3 | 1239 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|    3 | 1240 | `		break;` |
|    2 | 1241 | `	case 'm':` |
|    - | 1242 | `		/* Month number*/` |
|    5 | 1243 | `		iVal = sTm.tm_mon;` |
|    5 | 1244 | `		break;` |
|    1 | 1245 | `	case 's':` |
|    - | 1246 | `		/*Seconds*/` |
|    3 | 1247 | `		iVal = sTm.tm_sec;` |
|    3 | 1248 | `		break;` |
|    1 | 1249 | `	case 't':{` |
|    - | 1250 | `		/*Days in current month*/` |
|    - | 1251 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    3 | 1252 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|    3 | 1253 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|  ! 0 | 1254 | `			nDays = 28;` |
|  ! 0 | 1255 | `		}` |
|    3 | 1256 | `		iVal = nDays;` |
|    3 | 1257 | `		break;` |
|    - | 1258 | `			 }` |
|    1 | 1259 | `	case 'U':` |
|    - | 1260 | `		/*Seconds since the Unix Epoch*/` |
|    3 | 1261 | `		iVal = (ph7_int64)time(0);` |
|    3 | 1262 | `		break;` |
|    1 | 1263 | `	case 'w':` |
|    - | 1264 | `		/*	Day of the week (0 on Sunday) */` |
|    3 | 1265 | `		iVal = sTm.tm_wday;` |
|    3 | 1266 | `		break;` |
|    1 | 1267 | `	case 'W': {` |
|    - | 1268 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|    - | 1269 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|    3 | 1270 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|    3 | 1271 | `		break;` |
|    - | 1272 | `			  }` |
|  ! 0 | 1273 | `	case 'y':` |
|    - | 1274 | `		/* Year (2 digits) */` |
|  ! 0 | 1275 | `		iVal = sTm.tm_year % 100;` |
|  ! 0 | 1276 | `		break;` |
|    3 | 1277 | `	case 'Y':` |
|    - | 1278 | `		/* Year (4 digits) */` |
|    7 | 1279 | `		iVal = sTm.tm_year;` |
|    7 | 1280 | `		break;` |
|    1 | 1281 | `	case 'z':` |
|    - | 1282 | `		/* Day of the year */` |
|    3 | 1283 | `		iVal = sTm.tm_yday;` |
|    3 | 1284 | `		break;` |
|    1 | 1285 | `	case 'Z':` |
|    - | 1286 | `		/*Timezone offset in seconds*/` |
|    3 | 1287 | `		iVal = sTm.tm_gmtoff;` |
|    3 | 1288 | `		break;` |
|    1 | 1289 | `	default:` |
|    - | 1290 | `		/* unknown format,throw a warning */` |
|    3 | 1291 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|    2 | 1292 | `		break;` |
|    - | 1293 | `	}` |
|    - | 1294 | `	/* Return the time value */` |
|   40 | 1295 | `	ph7_result_int64(pCtx,iVal);` |
|   40 | 1296 | `	return PH7_OK;` |
|   21 | 1297 | `}` |
|    - | 1298 | `/*` |
|    - | 1299 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|    - | 1300 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|    - | 1301 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|    - | 1302 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|    - | 1303 | ` *  specified.` |
|    - | 1304 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|    - | 1305 | ` *  the current value according to the local date and time.` |
|    - | 1306 | ` * Parameters` |
|    - | 1307 | ` * $hour` |
|    - | 1308 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|    - | 1309 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|    - | 1310 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|    - | 1311 | ` * $minute` |
|    - | 1312 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|    - | 1313 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|    - | 1314 | ` *  in the following hour(s).` |
|    - | 1315 | ` * $second` |
|    - | 1316 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|    - | 1317 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|    - | 1318 | ` * second in the following minute(s).` |
|    - | 1319 | ` * $month` |
|    - | 1320 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|    - | 1321 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|    - | 1322 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|    - | 1323 | ` * $day` |
|    - | 1324 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|    - | 1325 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|    - | 1326 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|    - | 1327 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|    - | 1328 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|    - | 1329 | ` * $year` |
|    - | 1330 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|    - | 1331 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|    - | 1332 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|    - | 1333 | ` * $is_dst` |
|    - | 1334 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|    - | 1335 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|    - | 1336 | ` * Return` |
|    - | 1337 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|    - | 1338 | ` *   If the arguments are invalid, the function returns FALSE` |
|    - | 1339 | ` */` |
|    8 | 1340 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1341 | `{` |
|    - | 1342 | `	const char *zFunction;` |
|    9 | 1343 | `	ph7_int64 iVal = 0;` |
|    - | 1344 | `	struct tm *pTm;` |
|    - | 1345 | `	time_t t;` |
|    - | 1346 | `	/* Extract function name */` |
|    9 | 1347 | `	zFunction = ph7_function_name(pCtx);` |
|    - | 1348 | `	/* Get the current time */` |
|    9 | 1349 | `	time(&t);` |
|    9 | 1350 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|    3 | 1351 | `		pTm = gmtime(&t);` |
|    2 | 1352 | `	}else{` |
|    - | 1353 | `		/* localtime */` |
|    7 | 1354 | `		pTm = localtime(&t);` |
|    - | 1355 | `	}` |
|    9 | 1356 | `	if( nArg > 0 ){` |
|    - | 1357 | `		int iTmp;` |
|    - | 1358 | `		/* Hour */` |
|    9 | 1359 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|    9 | 1360 | `		pTm->tm_hour = iTmp;` |
|    9 | 1361 | `		if( nArg > 1 ){` |
|    - | 1362 | `			/* Minutes */` |
|    9 | 1363 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|    9 | 1364 | `			pTm->tm_min = iTmp;` |
|    9 | 1365 | `			if( nArg > 2 ){` |
|    - | 1366 | `				/* Seconds */` |
|    9 | 1367 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|    9 | 1368 | `				pTm->tm_sec = iTmp;` |
|    9 | 1369 | `				if( nArg > 3 ){` |
|    - | 1370 | `					/* Month */` |
|    9 | 1371 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|    9 | 1372 | `					pTm->tm_mon = iTmp - 1;` |
|    9 | 1373 | `					if( nArg > 4 ){` |
|    - | 1374 | `						/* mday */` |
|    9 | 1375 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|    9 | 1376 | `						pTm->tm_mday = iTmp;` |
|    9 | 1377 | `						if( nArg > 5 ){` |
|    - | 1378 | `							/* Year */` |
|    9 | 1379 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|    9 | 1380 | `							if( iTmp > 1900 ){` |
|    9 | 1381 | `								iTmp -= 1900;` |
|    4 | 1382 | `							}` |
|    9 | 1383 | `							pTm->tm_year = iTmp;` |
|    9 | 1384 | `							if( nArg > 6 ){` |
|    - | 1385 | `								/* is_dst */` |
|  ! 0 | 1386 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|  ! 0 | 1387 | `								pTm->tm_isdst = iTmp;` |
|  ! 0 | 1388 | `							}` |
|    4 | 1389 | `						}` |
|    4 | 1390 | `					}` |
|    4 | 1391 | `				}` |
|    4 | 1392 | `			}` |
|    4 | 1393 | `		}` |
|    4 | 1394 | `	}` |
|    - | 1395 | `	/* Make the time */` |
|    9 | 1396 | `	iVal = (ph7_int64)mktime(pTm);` |
|    - | 1397 | `	/* Return the timesatmp as a 64bit integer */` |
|    9 | 1398 | `	ph7_result_int64(pCtx,iVal);` |
|    9 | 1399 | `	return PH7_OK;` |
|    1 | 1400 | `}` |
|    - | 1401 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1402 |  |
