# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1189/1631 lines (72.90%)

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
|     - |  181 | ` * array getdate ([ int $timestamp = time() ])` |
|     - |  182 | ` *  Returns an associative array containing the date information` |
|     - |  183 | ` *  of the timestamp, or the current local time if no timestamp is given.` |
|     - |  184 | ` * Parameter` |
|     - |  185 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - |  186 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - |  187 | ` *     In other words, it defaults to the value of time().` |
|     - |  188 | ` * Returns` |
|     - |  189 | ` *  Returns an associative array of information related to the timestamp.` |
|     - |  190 | ` */` |
|     8 |  191 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  192 | `{` |
|     - |  193 | `	ph7_value *pValue,*pArray;` |
|     - |  194 | `	Sytm sTm;` |
|     9 |  195 | `	if( nArg < 1 ){` |
|     - |  196 | `#ifdef __WINNT__` |
|     - |  197 | `		SYSTEMTIME sOS;` |
|     1 |  198 | `		GetSystemTime(&sOS);` |
|     1 |  199 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - |  200 | `#else` |
|     - |  201 | `		struct tm *pTm;` |
|     - |  202 | `		time_t t;` |
|     4 |  203 | `		time(&t);` |
|     4 |  204 | `		pTm = gmtime(&t);` |
|     4 |  205 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     4 |  206 | `		DtSytmFillOffset(&sTm,t);` |
|     - |  207 | `#endif` |
|     3 |  208 | `	}else{` |
|     - |  209 | `		/* Use the given timestamp */` |
|     - |  210 | `		time_t t;` |
|     - |  211 | `		struct tm *pTm;` |
|     5 |  212 | `		if( ph7_value_is_int(apArg[0]) ){` |
|     5 |  213 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|     5 |  214 | `			pTm = gmtime(&t);` |
|     5 |  215 | `			if( pTm == 0 ){` |
|   ! 0 |  216 | `				time(&t);` |
|   ! 0 |  217 | `			}` |
|     3 |  218 | `		}else{` |
|   ! 0 |  219 | `			time(&t);` |
|     - |  220 | `		}` |
|     5 |  221 | `		pTm = gmtime(&t);` |
|     5 |  222 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     5 |  223 | `		DtSytmFillOffset(&sTm,t);` |
|     - |  224 | `	}` |
|     - |  225 | `	/* Element value */` |
|     9 |  226 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     9 |  227 | `	if( pValue == 0 ){` |
|     - |  228 | `		/* Return NULL */` |
|   ! 0 |  229 | `		ph7_result_null(pCtx);` |
|   ! 0 |  230 | `		return PH7_OK;` |
|     - |  231 | `	}` |
|     - |  232 | `	/* Create a new array */` |
|     9 |  233 | `	pArray = ph7_context_new_array(pCtx);` |
|     9 |  234 | `	if( pArray == 0 ){` |
|     - |  235 | `		/* Return NULL */` |
|   ! 0 |  236 | `		ph7_result_null(pCtx);` |
|   ! 0 |  237 | `		return PH7_OK;` |
|     - |  238 | `	}` |
|     - |  239 | `	/* Fill the array */` |
|     - |  240 | `	/* Seconds */` |
|     9 |  241 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|     9 |  242 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|     - |  243 | `	/* Minutes */` |
|     9 |  244 | `	ph7_value_int(pValue,sTm.tm_min);` |
|     9 |  245 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|     - |  246 | `	/* Hours */` |
|     9 |  247 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|     9 |  248 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|     - |  249 | `	/* mday */` |
|     9 |  250 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|     9 |  251 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|     - |  252 | `	/* wday */` |
|     9 |  253 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|     9 |  254 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|     - |  255 | `	/* mon */` |
|     9 |  256 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|     9 |  257 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|     - |  258 | `	/* year */` |
|     9 |  259 | `	ph7_value_int(pValue,sTm.tm_year);` |
|     9 |  260 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|     - |  261 | `	/* yday */` |
|     9 |  262 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|     9 |  263 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|     - |  264 | `	/* Weekday [i.e: Monday,Tuesday,...] */` |
|     9 |  265 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|     9 |  266 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|     - |  267 | `	/* Reset the string cursor */` |
|     9 |  268 | `	ph7_value_reset_string_cursor(pValue);` |
|     - |  269 | `	/* Month [i.e: January,February,...] */` |
|     9 |  270 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|     9 |  271 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|     - |  272 | `	/* Return the freshly created array */` |
|     9 |  273 | `	ph7_result_value(pCtx,pArray);` |
|     9 |  274 | `	return PH7_OK;` |
|     5 |  275 | `}` |
|     - |  276 | `/*` |
|     - |  277 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|     - |  278 | ` *  Returns an associative array containing the data returned from the system call.` |
|     - |  279 | ` * Parameters` |
|     - |  280 | ` *  $return_float` |
|     - |  281 | ` *   When set to TRUE, a float instead of an array is returned.` |
|     - |  282 | ` * Return` |
|     - |  283 | ` *  By default an array is returned. If return_float is set, then` |
|     - |  284 | ` *  a float is returned.` |
|     - |  285 | ` */` |
|     8 |  286 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  287 | `{` |
|     9 |  288 | `	int bFloat = 0;` |
|     - |  289 | `	sytime sTime;` |
|     9 |  290 | `	DateNow(pCtx->pVm,&sTime);` |
|     9 |  291 | `	if( nArg > 0 ){` |
|     7 |  292 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|     3 |  293 | `	}` |
|     9 |  294 | `	if( bFloat ){` |
|     - |  295 | `		/* Return as float: seconds accurate to the nearest microsecond */` |
|     5 |  296 | `		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);` |
|     3 |  297 | `	}else{` |
|     - |  298 | `		/* Return an associative array */` |
|     - |  299 | `		ph7_value *pValue,*pArray;` |
|     - |  300 | `		/* Create a new array */` |
|     5 |  301 | `		pArray = ph7_context_new_array(pCtx);` |
|     - |  302 | `		/* Element value */` |
|     5 |  303 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     5 |  304 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|     - |  305 | `			/* Return NULL */` |
|   ! 0 |  306 | `			ph7_result_null(pCtx);` |
|   ! 0 |  307 | `			return PH7_OK;` |
|     - |  308 | `		}` |
|     - |  309 | `		/* Fill the array */` |
|     - |  310 | `		/* sec */` |
|     5 |  311 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|     5 |  312 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|     - |  313 | `		/* usec */` |
|     5 |  314 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|     5 |  315 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|     - |  316 | `		/* Return the array */` |
|     5 |  317 | `		ph7_result_value(pCtx,pArray);` |
|     - |  318 | `	}` |
|     9 |  319 | `	return PH7_OK;` |
|     5 |  320 | `}` |
|     - |  321 | `/* Check if the given year is leap or not */` |
|     - |  322 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|     - |  323 | `/* ISO-8601 numeric representation of the day of the week */` |
|     - |  324 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|     - |  325 | `/*` |
|     - |  326 | ` * Format a given date string.` |
|     - |  327 | ` * Supported format: (Taken from PHP online docs)` |
|     - |  328 | ` * character 	Description` |
|     - |  329 | ` * d          Day of the month, 2 digits with leading zeros` |
|     - |  330 | ` * D          A textual representation of a day, three letters` |
|     - |  331 | ` * j          Day of the month without leading zeros` |
|     - |  332 | ` * l          A full textual representation of the day of the week` |
|     - |  333 | ` * N          ISO-8601 numeric representation of the day of the week` |
|     - |  334 | ` * w          Numeric representation of the day of the week` |
|     - |  335 | ` * z          The day of the year (starting from 0)` |
|     - |  336 | ` * F          A full textual representation of a month, such as January or March` |
|     - |  337 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|     - |  338 | ` * M          A short textual representation of a month, three letters` |
|     - |  339 | ` * n          Numeric representation of a month, without leading zeros` |
|     - |  340 | ` * t          Number of days in the given month` |
|     - |  341 | ` * L          Whether it's a leap year` |
|     - |  342 | ` * o          ISO-8601 year number. This has the same value as Y` |
|     - |  343 | ` * Y          A full numeric representation of a year, 4 digits` |
|     - |  344 | ` * y          A two digit representation of a year` |
|     - |  345 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|     - |  346 | ` * A          Uppercase Ante meridiem and Post meridiem` |
|     - |  347 | ` * g          12-hour format of an hour without leading zeros` |
|     - |  348 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|     - |  349 | ` * h          12-hour format of an hour with leading zeros` |
|     - |  350 | ` * H          24-hour format of an hour with leading zeros` |
|     - |  351 | ` * i          Minutes with leading zeros` |
|     - |  352 | ` * s          Seconds, with leading zeros` |
|     - |  353 | ` * u          Microseconds` |
|     - |  354 | ` * e          Timezone identifier` |
|     - |  355 | ` * I          Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|     - |  356 | ` * r          RFC 2822 formatted date` |
|     - |  357 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|     - |  358 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|     - |  359 | ` * O          Difference to Greenwich time (GMT) in hours` |
|     - |  360 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|     - |  361 | ` *            east of UTC is always positive.` |
|     - |  362 | ` * c         ISO 8601 date` |
|     - |  363 | ` */` |
|   206 |  364 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|     1 |  365 | `{` |
|   207 |  366 | `	const char *zEnd = &zIn[nLen];` |
|     - |  367 | `	const char *zCur;` |
|     - |  368 | `	/* Start the format process */` |
|   608 |  369 | `	for(;;){` |
|  1217 |  370 | `		if( zIn >= zEnd ){` |
|     - |  371 | `			/* No more input to process */` |
|   207 |  372 | `			break;` |
|     - |  373 | `		}` |
|  1011 |  374 | `		switch(zIn[0]){` |
|    68 |  375 | `		case 'd':` |
|     - |  376 | `			/* Day of the month, 2 digits with leading zeros */` |
|   137 |  377 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|   137 |  378 | `			break;` |
|   ! 0 |  379 | `		case 'D':` |
|     - |  380 | `			/*A textual representation of a day, three letters*/` |
|   ! 0 |  381 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|   ! 0 |  382 | `			ph7_result_string(pCtx,zCur,3);` |
|   ! 0 |  383 | `			break;` |
|     1 |  384 | `		case 'j':` |
|     - |  385 | `			/*	Day of the month without leading zeros */` |
|     3 |  386 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|     3 |  387 | `			break;` |
|     2 |  388 | `		case 'l':` |
|     - |  389 | `			/* A full textual representation of the day of the week */` |
|     5 |  390 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|     5 |  391 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     5 |  392 | `			break;` |
|   ! 0 |  393 | `		case 'N':{` |
|     - |  394 | `			/* ISO-8601 numeric representation of the day of the week */` |
|   ! 0 |  395 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|   ! 0 |  396 | `			break;` |
|     - |  397 | `				 }` |
|   ! 0 |  398 | `		case 'w':` |
|     - |  399 | `			/*Numeric representation of the day of the week*/` |
|   ! 0 |  400 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|   ! 0 |  401 | `			break;` |
|   ! 0 |  402 | `		case 'z':` |
|     - |  403 | `			/*The day of the year*/` |
|   ! 0 |  404 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|   ! 0 |  405 | `			break;` |
|     2 |  406 | `		case 'F':` |
|     - |  407 | `			/*A full textual representation of a month, such as January or March*/` |
|     5 |  408 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|     5 |  409 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|     5 |  410 | `			break;` |
|    68 |  411 | `		case 'm':` |
|     - |  412 | `			/*Numeric representation of a month, with leading zeros*/` |
|   137 |  413 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|   137 |  414 | `			break;` |
|   ! 0 |  415 | `		case 'M':` |
|     - |  416 | `			/*A short textual representation of a month, three letters*/` |
|   ! 0 |  417 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|   ! 0 |  418 | `			ph7_result_string(pCtx,zCur,3);` |
|   ! 0 |  419 | `			break;` |
|     1 |  420 | `		case 'n':` |
|     - |  421 | `			/*Numeric representation of a month, without leading zeros*/` |
|     3 |  422 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|     3 |  423 | `			break;` |
|   ! 0 |  424 | `		case 't':{` |
|     - |  425 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|   ! 0 |  426 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|   ! 0 |  427 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|   ! 0 |  428 | `				nDays = 28;` |
|   ! 0 |  429 | `			}` |
|     - |  430 | `			/*Number of days in the given month*/` |
|   ! 0 |  431 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|   ! 0 |  432 | `			break;` |
|     - |  433 | `				 }` |
|   ! 0 |  434 | `		case 'L':{` |
|   ! 0 |  435 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|     - |  436 | `			/* Whether it's a leap year */` |
|   ! 0 |  437 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|   ! 0 |  438 | `			break;` |
|     - |  439 | `				 }` |
|   ! 0 |  440 | `		case 'o': case 'W': {` |
|     - |  441 | `			/* ISO-8601 week-numbering year / week number: both belong to the` |
|     - |  442 | `			 * year owning the Thursday of the civil week (php: 2024-12-31 is` |
|     - |  443 | `			 * 2025-W01, 2027-01-01 is 2026-W53). php pads W but not o. */` |
|   ! 0 |  444 | `			sxi64 days = DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday);` |
|   ! 0 |  445 | `			int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */` |
|   ! 0 |  446 | `			sxi64 thu = days + (4 - isoDow);` |
|     - |  447 | `			sxi64 wy;` |
|     - |  448 | `			int wm,wd;` |
|   ! 0 |  449 | `			DtCivilFromDays(thu,&wy,&wm,&wd);` |
|   ! 0 |  450 | `			if( zIn[0] == 'o' ){` |
|   ! 0 |  451 | `				ph7_result_string_format(pCtx,"%d",(int)wy);` |
|   ! 0 |  452 | `			}else{` |
|   ! 0 |  453 | `				ph7_result_string_format(pCtx,"%02d",` |
|   ! 0 |  454 | `					(int)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1);` |
|     - |  455 | `			}` |
|   ! 0 |  456 | `			break;` |
|     - |  457 | `				 }` |
|    55 |  458 | `		case 'Y':` |
|     - |  459 | `			/*	A full numeric representation of a year, 4 digits */` |
|   111 |  460 | `			ph7_result_string_format(pCtx,"%04d",pTm->tm_year);` |
|   111 |  461 | `			break;` |
|   ! 0 |  462 | `		case 'X':` |
|     - |  463 | `			/* Expanded full year, always signed (php 8.2+): +2024 */` |
|   ! 0 |  464 | `			ph7_result_string_format(pCtx,"%c%04d",` |
|   ! 0 |  465 | `				pTm->tm_year < 0 ? '-' : '+',` |
|   ! 0 |  466 | `				pTm->tm_year < 0 ? -pTm->tm_year : pTm->tm_year);` |
|   ! 0 |  467 | `			break;` |
|   ! 0 |  468 | `		case 'x':` |
|     - |  469 | `			/* Expanded year, signed only past 4 digits (php 8.2+) */` |
|   ! 0 |  470 | `			if( pTm->tm_year > 9999 ){` |
|   ! 0 |  471 | `				ph7_result_string_format(pCtx,"+%d",pTm->tm_year);` |
|   ! 0 |  472 | `			}else{` |
|   ! 0 |  473 | `				ph7_result_string_format(pCtx,"%04d",pTm->tm_year);` |
|     - |  474 | `			}` |
|   ! 0 |  475 | `			break;` |
|   ! 0 |  476 | `		case 'y':` |
|     - |  477 | `			/*A two digit representation of a year*/` |
|   ! 0 |  478 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|   ! 0 |  479 | `			break;` |
|   ! 0 |  480 | `		case 'a':` |
|     - |  481 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|   ! 0 |  482 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "pm" : "am",2);` |
|   ! 0 |  483 | `			break;` |
|   ! 0 |  484 | `		case 'A':` |
|     - |  485 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|   ! 0 |  486 | `			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "PM" : "AM",2);` |
|   ! 0 |  487 | `			break;` |
|   ! 0 |  488 | `		case 'B':{` |
|     - |  489 | `			/* Swatch Internet time: thousandths of the UTC+1 day */` |
|   ! 0 |  490 | `			sxi64 iUtc = DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400` |
|   ! 0 |  491 | `				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec` |
|   ! 0 |  492 | `				- (sxi64)pTm->tm_gmtoff;` |
|   ! 0 |  493 | `			sxi64 iBie = (iUtc + 3600) % 86400;` |
|   ! 0 |  494 | `			if( iBie < 0 ){` |
|   ! 0 |  495 | `				iBie += 86400;` |
|   ! 0 |  496 | `			}` |
|   ! 0 |  497 | `			ph7_result_string_format(pCtx,"%03d",(int)(iBie * 1000 / 86400));` |
|   ! 0 |  498 | `			break;` |
|     - |  499 | `				 }` |
|   ! 0 |  500 | `		case 'g':` |
|     - |  501 | `			/*	12-hour format of an hour without leading zeros*/` |
|   ! 0 |  502 | `			ph7_result_string_format(pCtx,"%d",` |
|   ! 0 |  503 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|   ! 0 |  504 | `			break;` |
|     1 |  505 | `		case 'G':` |
|     - |  506 | `			/* 24-hour format of an hour without leading zeros */` |
|     3 |  507 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|     3 |  508 | `			break;` |
|   ! 0 |  509 | `		case 'h':` |
|     - |  510 | `			/* 12-hour format of an hour with leading zeros */` |
|   ! 0 |  511 | `			ph7_result_string_format(pCtx,"%02d",` |
|   ! 0 |  512 | `				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);` |
|   ! 0 |  513 | `			break;` |
|    28 |  514 | `		case 'H':` |
|     - |  515 | `			/*	24-hour format of an hour with leading zeros */` |
|    57 |  516 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|    57 |  517 | `			break;` |
|    29 |  518 | `		case 'i':` |
|     - |  519 | `			/* 	Minutes with leading zeros */` |
|    59 |  520 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|    59 |  521 | `			break;` |
|    29 |  522 | `		case 's':` |
|     - |  523 | `			/* 	second with leading zeros */` |
|    59 |  524 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    59 |  525 | `			break;` |
|   ! 0 |  526 | `		case 'u':` |
|     - |  527 | `			/* 	Microseconds (whole-second timestamps only: php pads zeros) */` |
|   ! 0 |  528 | `			ph7_result_string(pCtx,"000000",6);` |
|   ! 0 |  529 | `			break;` |
|   ! 0 |  530 | `		case 'v':` |
|     - |  531 | `			/* 	Milliseconds (same) */` |
|   ! 0 |  532 | `			ph7_result_string(pCtx,"000",3);` |
|   ! 0 |  533 | `			break;` |
|   ! 0 |  534 | `		case 'S':{` |
|     - |  535 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|     - |  536 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|   ! 0 |  537 | `			int v = pTm->tm_mday;` |
|   ! 0 |  538 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|   ! 0 |  539 | `			break;` |
|     - |  540 | `				 }` |
|     7 |  541 | `		case 'e':` |
|     - |  542 | `			/* 	Timezone identifier */` |
|    15 |  543 | `			zCur = pTm->tm_zone;` |
|    15 |  544 | `			if( zCur == 0 ){` |
|     - |  545 | `				/* date()-family fills: the script default timezone */` |
|     5 |  546 | `				zCur = pCtx->pVm->zDefTz;` |
|     2 |  547 | `			}` |
|    15 |  548 | `			ph7_result_string(pCtx,zCur,-1);` |
|    15 |  549 | `			break;` |
|     2 |  550 | `		case 'T':{` |
|     - |  551 | `			/* Timezone abbreviation: "UTC" for offset 0, "GMT+0530" for a` |
|     - |  552 | `			 * fixed offset (php's shape). PHL has no tz database, so the` |
|     - |  553 | `			 * zone-name path only ever sees UTC/GMT, uppercased. */` |
|     - |  554 | `			const char *z;` |
|     5 |  555 | `			if( pTm->tm_gmtoff != 0 ){` |
|   ! 0 |  556 | `				long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   ! 0 |  557 | `				ph7_result_string_format(pCtx,"GMT%c%02d%02d",` |
|   ! 0 |  558 | `					pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|   ! 0 |  559 | `				break;` |
|     - |  560 | `			}` |
|     5 |  561 | `			z = pTm->tm_zone ? pTm->tm_zone : pCtx->pVm->zDefTz;` |
|    17 |  562 | `			while( *z ){` |
|    13 |  563 | `				int c = (unsigned char)*z;` |
|    13 |  564 | `				if( c >= 'a' && c <= 'z' ){` |
|   ! 0 |  565 | `					c -= 'a' - 'A';` |
|   ! 0 |  566 | `				}` |
|    13 |  567 | `				ph7_result_string_format(pCtx,"%c",c);` |
|    13 |  568 | `				z++;` |
|     1 |  569 | `			}` |
|     5 |  570 | `			break;` |
|     - |  571 | `				 }` |
|   ! 0 |  572 | `		case 'I':` |
|     - |  573 | `			/* Whether or not the date is in daylight saving time */` |
|     - |  574 | `#ifdef __WINNT__` |
|     - |  575 | `#ifdef _MSC_VER` |
|     - |  576 | `#ifndef _WIN32_WCE` |
|   ! 0 |  577 | `			_get_daylight(&pTm->tm_isdst);` |
|     - |  578 | `#endif` |
|     - |  579 | `#endif` |
|     - |  580 | `#endif` |
|   ! 0 |  581 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|   ! 0 |  582 | `			break;` |
|   ! 0 |  583 | `		case 'r':{` |
|     - |  584 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200 */` |
|   ! 0 |  585 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   ! 0 |  586 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d %c%02d%02d",` |
|   ! 0 |  587 | `				SyTimeGetDay(pTm->tm_wday),` |
|   ! 0 |  588 | `				pTm->tm_mday,` |
|   ! 0 |  589 | `				SyTimeGetMonth(pTm->tm_mon),` |
|   ! 0 |  590 | `				pTm->tm_year,` |
|   ! 0 |  591 | `				pTm->tm_hour,` |
|   ! 0 |  592 | `				pTm->tm_min,` |
|   ! 0 |  593 | `				pTm->tm_sec,` |
|   ! 0 |  594 | `				pTm->tm_gmtoff < 0 ? '-' : '+',` |
|   ! 0 |  595 | `				(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  596 | `				);` |
|   ! 0 |  597 | `			break;` |
|     - |  598 | `				 }` |
|     2 |  599 | `		case 'U':` |
|     - |  600 | `			/* Seconds since the Unix Epoch FOR THIS Sytm (php: the timestamp` |
|     - |  601 | `			 * being formatted — pre-fix this printed time(0) regardless of the` |
|     - |  602 | `			 * date under format). */` |
|     7 |  603 | `			ph7_result_string_format(pCtx,"%qd",` |
|     4 |  604 | `				DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400` |
|     4 |  605 | `				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec` |
|     4 |  606 | `				- (sxi64)pTm->tm_gmtoff);` |
|     5 |  607 | `			break;` |
|     1 |  608 | `		case 'O':{` |
|     - |  609 | `			/* Difference to GMT without colon: +0530 (php) */` |
|     3 |  610 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     3 |  611 | `			ph7_result_string_format(pCtx,"%c%02d%02d",` |
|     2 |  612 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|     3 |  613 | `			break;` |
|     - |  614 | `				 }` |
|     2 |  615 | `		case 'P':{` |
|     - |  616 | `			/* Difference to GMT with colon: +05:30 (php) */` |
|     5 |  617 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|     5 |  618 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|     4 |  619 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|     5 |  620 | `			break;` |
|     - |  621 | `				 }` |
|   ! 0 |  622 | `		case 'p':{` |
|     - |  623 | `			/* Like P, but "Z" for UTC (php 8.0+) */` |
|     - |  624 | `			long a;` |
|   ! 0 |  625 | `			if( pTm->tm_gmtoff == 0 ){` |
|   ! 0 |  626 | `				ph7_result_string(pCtx,"Z",1);` |
|   ! 0 |  627 | `				break;` |
|     - |  628 | `			}` |
|   ! 0 |  629 | `			a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|   ! 0 |  630 | `			ph7_result_string_format(pCtx,"%c%02d:%02d",` |
|   ! 0 |  631 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|   ! 0 |  632 | `			break;` |
|     - |  633 | `				 }` |
|   ! 0 |  634 | `		case 'Z':` |
|     - |  635 | `			/* Timezone offset in seconds, plain integer (php) */` |
|   ! 0 |  636 | `			ph7_result_string_format(pCtx,"%d",(int)pTm->tm_gmtoff);` |
|   ! 0 |  637 | `			break;` |
|     4 |  638 | `		case 'c':{` |
|     - |  639 | `			/* 	ISO 8601 date: 2004-02-12T15:19:21+00:00 (php) */` |
|     9 |  640 | `			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;` |
|    13 |  641 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",` |
|     4 |  642 | `				pTm->tm_year,` |
|     8 |  643 | `				pTm->tm_mon+1,` |
|     4 |  644 | `				pTm->tm_mday,` |
|     4 |  645 | `				pTm->tm_hour,` |
|     4 |  646 | `				pTm->tm_min,` |
|     4 |  647 | `				pTm->tm_sec,` |
|     8 |  648 | `				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60)` |
|     - |  649 | `				);` |
|     9 |  650 | `			break;` |
|     - |  651 | `				 }` |
|     1 |  652 | `		case '\\':` |
|     3 |  653 | `			zIn++;` |
|     - |  654 | `			/* Expand verbatim */` |
|     3 |  655 | `			if( zIn < zEnd ){` |
|     3 |  656 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     1 |  657 | `			}` |
|     3 |  658 | `			break;` |
|   202 |  659 | `		default:` |
|     - |  660 | `			/* Unknown format specifer,expand verbatim */` |
|   405 |  661 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|   404 |  662 | `			break;` |
|     - |  663 | `		}` |
|     - |  664 | `		/* Point to the next character */` |
|  1011 |  665 | `		zIn++;` |
|     1 |  666 | `	}` |
|   207 |  667 | `	return SXRET_OK;` |
|     1 |  668 | `}` |
|     - |  669 | `/*` |
|     - |  670 | ` * PH7 implementation of the strftime() function.` |
|     - |  671 | ` * The following formats are supported:` |
|     - |  672 | ` * %a 	An abbreviated textual representation of the day` |
|     - |  673 | ` * %A 	A full textual representation of the day` |
|     - |  674 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|     - |  675 | ` * %e 	Day of the month, with a space preceding single digits.` |
|     - |  676 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|     - |  677 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|     - |  678 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|     - |  679 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|     - |  680 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|     - |  681 | ` *   4 weekdays, with Monday being the start of the week.` |
|     - |  682 | ` * %W 	A numeric representation of the week of the year` |
|     - |  683 | ` * %b 	Abbreviated month name, based on the locale` |
|     - |  684 | ` * %B 	Full month name, based on the locale` |
|     - |  685 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|     - |  686 | ` * %m 	Two digit representation of the month` |
|     - |  687 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|     - |  688 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|     - |  689 | ` * %G 	The full four-digit version of %g` |
|     - |  690 | ` * %y 	Two digit representation of the year` |
|     - |  691 | ` * %Y 	Four digit representation for the year` |
|     - |  692 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|     - |  693 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|     - |  694 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|     - |  695 | ` * %M 	Two digit representation of the minute` |
|     - |  696 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|     - |  697 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|     - |  698 | ` * %r 	Same as "%I:%M:%S %p"` |
|     - |  699 | ` * %R 	Same as "%H:%M"` |
|     - |  700 | ` * %S 	Two digit representation of the second` |
|     - |  701 | ` * %T 	Same as "%H:%M:%S"` |
|     - |  702 | ` * %X 	Preferred time representation based on locale, without the date` |
|     - |  703 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|     - |  704 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|     - |  705 | ` * %c 	Preferred date and time stamp based on local` |
|     - |  706 | ` * %D 	Same as "%m/%d/%y"` |
|     - |  707 | ` * %F 	Same as "%Y-%m-%d"` |
|     - |  708 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|     - |  709 | ` * %x 	Preferred date representation based on locale, without the time` |
|     - |  710 | ` * %n 	A newline character ("\n")` |
|     - |  711 | ` * %t 	A Tab character ("\t")` |
|     - |  712 | ` * %% 	A literal percentage character ("%")` |
|     - |  713 | ` */` |
|    18 |  714 | `static int PH7_Strftime(` |
|     - |  715 | `	ph7_context *pCtx,  /* Call context */` |
|     - |  716 | `	const char *zIn,    /* Input string */` |
|     - |  717 | `	int nLen,           /* Input length */` |
|     - |  718 | `	Sytm *pTm           /* Parse of the given time */` |
|     - |  719 | `	)` |
|     1 |  720 | `{` |
|    19 |  721 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|     - |  722 | `	int c;` |
|     - |  723 | `	/* Start the format process */` |
|    20 |  724 | `	for(;;){` |
|    41 |  725 | `		zCur = zIn;` |
|    45 |  726 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|     5 |  727 | `			zIn++;` |
|     1 |  728 | `		}` |
|    41 |  729 | `		if( zIn > zCur ){` |
|     - |  730 | `			/* Consume input verbatim */` |
|     5 |  731 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     2 |  732 | `		}` |
|    41 |  733 | `		zIn++; /* Jump the percent sign */` |
|    41 |  734 | `		if( zIn >= zEnd ){` |
|     - |  735 | `			/* No more input to process */` |
|    19 |  736 | `			break;` |
|     - |  737 | `		}` |
|    23 |  738 | `		c = zIn[0];` |
|     - |  739 | `		/* Act according to the current specifer */` |
|    23 |  740 | `		switch(c){` |
|   ! 0 |  741 | `		case '%':` |
|     - |  742 | `			/* A literal percentage character ("%") */` |
|   ! 0 |  743 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|   ! 0 |  744 | `			break;` |
|   ! 0 |  745 | `		case 't':` |
|     - |  746 | `			/* A Tab character */` |
|   ! 0 |  747 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|   ! 0 |  748 | `			break;` |
|   ! 0 |  749 | `		case 'n':` |
|     - |  750 | `			/* A newline character */` |
|   ! 0 |  751 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|   ! 0 |  752 | `			break;` |
|     1 |  753 | `		case 'a':` |
|     - |  754 | `			/* An abbreviated textual representation of the day */` |
|     3 |  755 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|     3 |  756 | `			break;` |
|   ! 0 |  757 | `		case 'A':` |
|     - |  758 | `			/* A full textual representation of the day */` |
|   ! 0 |  759 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|   ! 0 |  760 | `			break;` |
|   ! 0 |  761 | `		case 'e':` |
|     - |  762 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|   ! 0 |  763 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|   ! 0 |  764 | `			break;` |
|     2 |  765 | `		case 'd':` |
|     - |  766 | `			/* Two-digit day of the month (with leading zeros) */` |
|     5 |  767 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|     5 |  768 | `			break;` |
|   ! 0 |  769 | `		case 'j':` |
|     - |  770 | `			/*The day of the year,3 digits with leading zeros*/` |
|   ! 0 |  771 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|   ! 0 |  772 | `			break;` |
|   ! 0 |  773 | `		case 'u':` |
|     - |  774 | `			/* ISO-8601 numeric representation of the day of the week */` |
|   ! 0 |  775 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|   ! 0 |  776 | `			break;` |
|   ! 0 |  777 | `		case 'w':` |
|     - |  778 | `			/* Numeric representation of the day of the week */` |
|   ! 0 |  779 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|   ! 0 |  780 | `			break;` |
|   ! 0 |  781 | `		case 'b':` |
|     - |  782 | `		case 'h':` |
|     - |  783 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|   ! 0 |  784 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|   ! 0 |  785 | `			break;` |
|   ! 0 |  786 | `		case 'B':` |
|     - |  787 | `			/* Full month name (Not based on locale) */` |
|   ! 0 |  788 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|   ! 0 |  789 | `			break;` |
|     2 |  790 | `		case 'm':` |
|     - |  791 | `			/*Numeric representation of a month, with leading zeros*/` |
|     5 |  792 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     5 |  793 | `			break;` |
|   ! 0 |  794 | `		case 'C':` |
|     - |  795 | `			/* Two digit representation of the century */` |
|   ! 0 |  796 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|   ! 0 |  797 | `			break;` |
|   ! 0 |  798 | `		case 'y':` |
|     - |  799 | `		case 'g':` |
|     - |  800 | `			/* Two digit representation of the year */` |
|   ! 0 |  801 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|   ! 0 |  802 | `			break;` |
|     3 |  803 | `		case 'Y':` |
|     - |  804 | `		case 'G':` |
|     - |  805 | `			/* Four digit representation of the year */` |
|     7 |  806 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     7 |  807 | `			break;` |
|   ! 0 |  808 | `		case 'I':` |
|     - |  809 | `			/* 12-hour format of an hour with leading zeros */` |
|   ! 0 |  810 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|   ! 0 |  811 | `			break;` |
|   ! 0 |  812 | `		case 'l':` |
|     - |  813 | `			/* 12-hour format of an hour with leading space */` |
|   ! 0 |  814 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|   ! 0 |  815 | `			break;` |
|     1 |  816 | `		case 'H':` |
|     - |  817 | `			/* 24-hour format of an hour with leading zeros */` |
|     3 |  818 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|     3 |  819 | `			break;` |
|     1 |  820 | `		case 'M':` |
|     - |  821 | `			/* Minutes with leading zeros */` |
|     3 |  822 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|     3 |  823 | `			break;` |
|   ! 0 |  824 | `		case 'S':` |
|     - |  825 | `			/* Seconds with leading zeros */` |
|   ! 0 |  826 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|   ! 0 |  827 | `			break;` |
|   ! 0 |  828 | `		case 'z':` |
|     - |  829 | `		case 'Z':` |
|     - |  830 | `			/* 	Timezone identifier */` |
|   ! 0 |  831 | `			zCur = pTm->tm_zone;` |
|   ! 0 |  832 | `			if( zCur == 0 ){` |
|     - |  833 | `				/* date()-family fills: the script default timezone */` |
|   ! 0 |  834 | `				zCur = pCtx->pVm->zDefTz;` |
|   ! 0 |  835 | `			}` |
|   ! 0 |  836 | `			ph7_result_string(pCtx,zCur,-1);` |
|   ! 0 |  837 | `			break;` |
|   ! 0 |  838 | `		case 'T':` |
|     - |  839 | `		case 'X':` |
|     - |  840 | `			/* Same as "%H:%M:%S" */` |
|   ! 0 |  841 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|   ! 0 |  842 | `			break;` |
|   ! 0 |  843 | `		case 'R':` |
|     - |  844 | `			/* Same as "%H:%M" */` |
|   ! 0 |  845 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|   ! 0 |  846 | `			break;` |
|   ! 0 |  847 | `		case 'P':` |
|     - |  848 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|   ! 0 |  849 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|   ! 0 |  850 | `			break;` |
|   ! 0 |  851 | `		case 'p':` |
|     - |  852 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|   ! 0 |  853 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|   ! 0 |  854 | `			break;` |
|   ! 0 |  855 | `		case 'r':` |
|     - |  856 | `			/* Same as "%I:%M:%S %p" */` |
|   ! 0 |  857 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|   ! 0 |  858 | `				1+(pTm->tm_hour%12),` |
|   ! 0 |  859 | `				pTm->tm_min,` |
|   ! 0 |  860 | `				pTm->tm_sec,` |
|   ! 0 |  861 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|     - |  862 | `				);` |
|   ! 0 |  863 | `			break;` |
|     1 |  864 | `		case 'D':` |
|     - |  865 | `		case 'x':` |
|     - |  866 | `			/* Same as "%m/%d/%y" */` |
|     4 |  867 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|     2 |  868 | `				pTm->tm_mon+1,` |
|     1 |  869 | `				pTm->tm_mday,` |
|     2 |  870 | `				pTm->tm_year%100` |
|     - |  871 | `				);` |
|     3 |  872 | `			break;` |
|   ! 0 |  873 | `		case 'F':` |
|     - |  874 | `			/* Same as "%Y-%m-%d" */` |
|   ! 0 |  875 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|   ! 0 |  876 | `				pTm->tm_year,` |
|   ! 0 |  877 | `				pTm->tm_mon+1,` |
|   ! 0 |  878 | `				pTm->tm_mday` |
|     - |  879 | `				);` |
|   ! 0 |  880 | `			break;` |
|   ! 0 |  881 | `		case 'c':` |
|   ! 0 |  882 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|   ! 0 |  883 | `				pTm->tm_year,` |
|   ! 0 |  884 | `				pTm->tm_mon+1,` |
|   ! 0 |  885 | `				pTm->tm_mday,` |
|   ! 0 |  886 | `				pTm->tm_hour,` |
|   ! 0 |  887 | `				pTm->tm_min,` |
|   ! 0 |  888 | `				pTm->tm_sec` |
|     - |  889 | `				);` |
|   ! 0 |  890 | `			break;` |
|   ! 0 |  891 | `		case 's':{` |
|     - |  892 | `			time_t tt;` |
|     - |  893 | `			/* Seconds since the Unix Epoch */` |
|   ! 0 |  894 | `			time(&tt);` |
|   ! 0 |  895 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|   ! 0 |  896 | `			break;` |
|     - |  897 | `				 }` |
|   ! 0 |  898 | `		default:` |
|     - |  899 | `			/* unknown specifer,simply ignore*/` |
|   ! 0 |  900 | `			break;` |
|     - |  901 | `		}` |
|     - |  902 | `		/* Advance the cursor */` |
|    23 |  903 | `		zIn++;` |
|     1 |  904 | `	}` |
|    19 |  905 | `	return SXRET_OK;` |
|     1 |  906 | `}` |
|     - |  907 | `/*` |
|     - |  908 | ` * Resolve a date()/gmdate() $timestamp argument under php 8's ?int weak ZPP:` |
|     - |  909 | ` *   - null            -> *pbUseNow = 1 (caller uses the current time)` |
|     - |  910 | ` *   - int/bool/float  -> coerce to a Unix timestamp (float truncates; php's` |
|     - |  911 | ` *                        float->int precision E_DEPRECATED is not emitted, §3.7)` |
|     - |  912 | ` *   - numeric string  -> coerce via php's is_numeric_string grammar` |
|     - |  913 | ` *                        (RangeStrToNumber: " 100 "/"1e3"/".5"/"+5" ok)` |
|     - |  914 | ` *   - anything else (non-numeric string, array, object, resource)` |
|     - |  915 | ` *                     -> catchable TypeError, byte-exact with php.` |
|     - |  916 | ` * Returns PH7_OK with *pbUseNow / *pT set, or the PH7_VmThrowException status.` |
|     - |  917 | ` */` |
|    42 |  918 | `static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)` |
|     1 |  919 | `{` |
|     - |  920 | `	char zBuf[64];` |
|    43 |  921 | `	*pbUseNow = 0;` |
|    43 |  922 | `	if( ph7_value_is_null(pArg) ){` |
|     3 |  923 | `		*pbUseNow = 1;` |
|     3 |  924 | `		return PH7_OK;` |
|     - |  925 | `	}` |
|    41 |  926 | `	if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) \|\| ph7_value_is_float(pArg) ){` |
|    23 |  927 | `		*pT = (time_t)ph7_value_to_int64(pArg);` |
|    23 |  928 | `		return PH7_OK;` |
|     - |  929 | `	}` |
|    19 |  930 | `	if( ph7_value_is_string(pArg) ){` |
|     - |  931 | `		int nStr;` |
|    19 |  932 | `		const char *zStr = ph7_value_to_string(pArg,&nStr);` |
|     - |  933 | `		sxi64 iLong; double dReal;` |
|    19 |  934 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|    19 |  935 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|     3 |  936 | `			*pT = (time_t)dReal;` |
|     6 |  937 | `			return PH7_OK;` |
|     - |  938 | `		}` |
|    17 |  939 | `		if( iKind == RANGE_IN_LONG ){` |
|     7 |  940 | `			*pT = (time_t)iLong;` |
|     7 |  941 | `			return PH7_OK;` |
|     - |  942 | `		}` |
|     - |  943 | `		/* Not a numeric string: fall through to the TypeError. */` |
|     5 |  944 | `	}` |
|    16 |  945 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  946 | `		"%s(): Argument #2 ($timestamp) must be of type ?int, %s given",` |
|     5 |  947 | `		ph7_function_name(pCtx),VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|    22 |  948 | `}` |
|     - |  949 | `/*` |
|     - |  950 | ` * string date(string $format [, int $timestamp = time() ] )` |
|     - |  951 | ` *  Returns a string formatted according to the given format string using` |
|     - |  952 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|     - |  953 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|     - |  954 | ` * Parameters` |
|     - |  955 | ` *  $format` |
|     - |  956 | ` *   The format of the outputted date string (See code above)` |
|     - |  957 | ` * $timestamp` |
|     - |  958 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - |  959 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - |  960 | ` *   In other words, it defaults to the value of time().` |
|     - |  961 | ` * Return` |
|     - |  962 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - |  963 | ` */` |
|    46 |  964 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  965 | `{` |
|     - |  966 | `	const char *zFormat;` |
|     - |  967 | `	int nLen;` |
|     - |  968 | `	Sytm sTm;` |
|    47 |  969 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - |  970 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 |  971 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  972 | `		return PH7_OK;` |
|     - |  973 | `	}` |
|    47 |  974 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    47 |  975 | `	if( nLen < 1 ){` |
|     - |  976 | `		/* Don't bother processing return the empty string */` |
|   ! 0 |  977 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  978 | `	}` |
|    47 |  979 | `	if( nArg < 2 ){` |
|     - |  980 | `#ifdef __WINNT__` |
|     - |  981 | `		SYSTEMTIME sOS;` |
|     1 |  982 | `		GetSystemTime(&sOS);` |
|     1 |  983 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - |  984 | `#else` |
|     - |  985 | `		struct tm *pTm;` |
|     - |  986 | `		time_t t;` |
|    30 |  987 | `		time(&t);` |
|    30 |  988 | `		pTm = gmtime(&t);` |
|    30 |  989 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    30 |  990 | `		DtSytmFillOffset(&sTm,t);` |
|     - |  991 | `#endif` |
|    16 |  992 | `	}else{` |
|     - |  993 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|    17 |  994 | `		time_t t = 0;` |
|     - |  995 | `		struct tm *pTm;` |
|     - |  996 | `		int bUseNow;` |
|    17 |  997 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|    17 |  998 | `		if( rc != PH7_OK ){` |
|     9 |  999 | `			return rc;` |
|     - | 1000 | `		}` |
|     9 | 1001 | `		if( bUseNow ){` |
|   ! 0 | 1002 | `			time(&t);` |
|   ! 0 | 1003 | `		}` |
|     9 | 1004 | `		pTm = gmtime(&t);` |
|     9 | 1005 | `		if( pTm == 0 ){` |
|   ! 0 | 1006 | `			time(&t);` |
|   ! 0 | 1007 | `			pTm = gmtime(&t);` |
|   ! 0 | 1008 | `		}` |
|     9 | 1009 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     9 | 1010 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1011 | `	}` |
|     - | 1012 | `	/* Format the given string */` |
|    39 | 1013 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|    39 | 1014 | `	return PH7_OK;` |
|    24 | 1015 | `}` |
|     - | 1016 | `/*` |
|     - | 1017 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|     - | 1018 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|     - | 1019 | ` * Parameters` |
|     - | 1020 | ` *  $format` |
|     - | 1021 | ` *   The format of the outputted date string (See code above)` |
|     - | 1022 | ` * $timestamp` |
|     - | 1023 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1024 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1025 | ` *   In other words, it defaults to the value of time().` |
|     - | 1026 | ` * Return` |
|     - | 1027 | ` * Returns a string formatted according format using the given timestamp` |
|     - | 1028 | ` * or the current local time if no timestamp is given.` |
|     - | 1029 | ` */` |
|    18 | 1030 | `PH7_PRIVATE int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1031 | `{` |
|     - | 1032 | `	const char *zFormat;` |
|     - | 1033 | `	int nLen;` |
|     - | 1034 | `	Sytm sTm;` |
|     - | 1035 | `	/* The php 8.1 whole-function deprecation is declared in aBuiltinDeprecated[] and` |
|     - | 1036 | `	 * emitted at the OP_CALL choke point, which is what puts it BEFORE the` |
|     - | 1037 | `	 * ArgumentCountError for a no-arg call — php's order. */` |
|    19 | 1038 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1039 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1040 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1041 | `		return PH7_OK;` |
|     - | 1042 | `	}` |
|    19 | 1043 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 1044 | `	if( nLen < 1 ){` |
|     - | 1045 | `		/* Don't bother processing return FALSE */` |
|   ! 0 | 1046 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1047 | `	}` |
|    19 | 1048 | `	if( nArg < 2 ){` |
|     - | 1049 | `#ifdef __WINNT__` |
|     - | 1050 | `		SYSTEMTIME sOS;` |
|     1 | 1051 | `		GetSystemTime(&sOS);` |
|     1 | 1052 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1053 | `#else` |
|     - | 1054 | `		struct tm *pTm;` |
|     - | 1055 | `		time_t t;` |
|    16 | 1056 | `		time(&t);` |
|    16 | 1057 | `		pTm = gmtime(&t);` |
|    16 | 1058 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    16 | 1059 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1060 | `#endif` |
|     9 | 1061 | `	}else{` |
|     - | 1062 | `		/* Use the given timestamp */` |
|     - | 1063 | `		time_t t;` |
|     - | 1064 | `		struct tm *pTm;` |
|     3 | 1065 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     3 | 1066 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     3 | 1067 | `			pTm = gmtime(&t);` |
|     3 | 1068 | `			if( pTm == 0 ){` |
|   ! 0 | 1069 | `				time(&t);` |
|   ! 0 | 1070 | `			}` |
|     2 | 1071 | `		}else{` |
|   ! 0 | 1072 | `			time(&t);` |
|     - | 1073 | `		}` |
|     3 | 1074 | `		pTm = gmtime(&t);` |
|     3 | 1075 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     3 | 1076 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1077 | `	}` |
|     - | 1078 | `	/* Format the given string */` |
|    19 | 1079 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|    19 | 1080 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     - | 1081 | `		/* Nothing was formatted,return FALSE */` |
|   ! 0 | 1082 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1083 | `	}` |
|    19 | 1084 | `	return PH7_OK;` |
|    10 | 1085 | `}` |
|     - | 1086 | `/*` |
|     - | 1087 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|     - | 1088 | ` *  Identical to the date() function except that the time returned` |
|     - | 1089 | ` *  is Greenwich Mean Time (GMT).` |
|     - | 1090 | ` * Parameters` |
|     - | 1091 | ` *  $format` |
|     - | 1092 | ` *  The format of the outputted date string (See code above)` |
|     - | 1093 | ` *  $timestamp` |
|     - | 1094 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1095 | ` *   that defaults to the current local time if a timestamp is not given.` |
|     - | 1096 | ` *   In other words, it defaults to the value of time().` |
|     - | 1097 | ` * Return` |
|     - | 1098 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|     - | 1099 | ` */` |
|    40 | 1100 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1101 | `{` |
|     - | 1102 | `	const char *zFormat;` |
|     - | 1103 | `	int nLen;` |
|     - | 1104 | `	Sytm sTm;` |
|    41 | 1105 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1106 | `		/* Missing/Invalid argument,return FALSE */` |
|   ! 0 | 1107 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1108 | `		return PH7_OK;` |
|     - | 1109 | `	}` |
|    41 | 1110 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    41 | 1111 | `	if( nLen < 1 ){` |
|     - | 1112 | `		/* Don't bother processing return the empty string */` |
|   ! 0 | 1113 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1114 | `	}` |
|    41 | 1115 | `	if( nArg < 2 ){` |
|     - | 1116 | `#ifdef __WINNT__` |
|     - | 1117 | `		SYSTEMTIME sOS;` |
|     1 | 1118 | `		GetSystemTime(&sOS);` |
|     1 | 1119 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1120 | `#else` |
|     - | 1121 | `		struct tm *pTm;` |
|     - | 1122 | `		time_t t;` |
|    14 | 1123 | `		time(&t);` |
|    14 | 1124 | `		pTm = gmtime(&t);` |
|    14 | 1125 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    14 | 1126 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1127 | `#endif` |
|     8 | 1128 | `	}else{` |
|     - | 1129 | `		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */` |
|    27 | 1130 | `		time_t t = 0;` |
|     - | 1131 | `		struct tm *pTm;` |
|     - | 1132 | `		int bUseNow;` |
|    27 | 1133 | `		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);` |
|    27 | 1134 | `		if( rc != PH7_OK ){` |
|     3 | 1135 | `			return rc;` |
|     - | 1136 | `		}` |
|    25 | 1137 | `		if( bUseNow ){` |
|     3 | 1138 | `			time(&t);` |
|     1 | 1139 | `		}` |
|    25 | 1140 | `		pTm = gmtime(&t);` |
|    25 | 1141 | `		if( pTm == 0 ){` |
|   ! 0 | 1142 | `			time(&t);` |
|   ! 0 | 1143 | `			pTm = gmtime(&t);` |
|   ! 0 | 1144 | `		}` |
|    25 | 1145 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    25 | 1146 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1147 | `	}` |
|     - | 1148 | `	/* Format the given string */` |
|    39 | 1149 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|    39 | 1150 | `	return PH7_OK;` |
|    21 | 1151 | `}` |
|     - | 1152 | `/*` |
|     - | 1153 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|     - | 1154 | ` *  Return the local time.` |
|     - | 1155 | ` * Parameter` |
|     - | 1156 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|     - | 1157 | ` *     that defaults to the current local time if a timestamp is not given.` |
|     - | 1158 | ` *     In other words, it defaults to the value of time().` |
|     - | 1159 | ` * $is_associative` |
|     - | 1160 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|     - | 1161 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|     - | 1162 | ` *   array containing all the different elements of the structure returned by the C function` |
|     - | 1163 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|     - | 1164 | ` *      "tm_sec" - seconds, 0 to 59` |
|     - | 1165 | ` *      "tm_min" - minutes, 0 to 59` |
|     - | 1166 | ` *      "tm_hour" - hours, 0 to 23` |
|     - | 1167 | ` *      "tm_mday" - day of the month, 1 to 31` |
|     - | 1168 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|     - | 1169 | ` *      "tm_year" - years since 1900` |
|     - | 1170 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|     - | 1171 | ` *      "tm_yday" - day of the year, 0 to 365` |
|     - | 1172 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|     - | 1173 | ` * Returns` |
|     - | 1174 | ` *  An associative array of information related to the timestamp.` |
|     - | 1175 | ` */` |
|     8 | 1176 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1177 | `{` |
|     - | 1178 | `	ph7_value *pValue,*pArray;` |
|     9 | 1179 | `	int isAssoc = 0;` |
|     - | 1180 | `	Sytm sTm;` |
|     9 | 1181 | `	if( nArg < 1 ){` |
|     - | 1182 | `#ifdef __WINNT__` |
|     - | 1183 | `		SYSTEMTIME sOS;` |
|     1 | 1184 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|     1 | 1185 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1186 | `#else` |
|     - | 1187 | `		struct tm *pTm;` |
|     - | 1188 | `		time_t t;` |
|     4 | 1189 | `		time(&t);` |
|     4 | 1190 | `		pTm = gmtime(&t);` |
|     4 | 1191 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     4 | 1192 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1193 | `#endif` |
|     3 | 1194 | `	}else{` |
|     - | 1195 | `		/* Use the given timestamp */` |
|     - | 1196 | `		time_t t;` |
|     - | 1197 | `		struct tm *pTm;` |
|     5 | 1198 | `		if( ph7_value_is_int(apArg[0]) ){` |
|     5 | 1199 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|     5 | 1200 | `			pTm = gmtime(&t);` |
|     5 | 1201 | `			if( pTm == 0 ){` |
|   ! 0 | 1202 | `				time(&t);` |
|   ! 0 | 1203 | `			}` |
|     3 | 1204 | `		}else{` |
|   ! 0 | 1205 | `			time(&t);` |
|     - | 1206 | `		}` |
|     5 | 1207 | `		pTm = gmtime(&t);` |
|     5 | 1208 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|     5 | 1209 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1210 | `	}` |
|     - | 1211 | `	/* Element value */` |
|     9 | 1212 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     9 | 1213 | `	if( pValue == 0 ){` |
|     - | 1214 | `		/* Return NULL */` |
|   ! 0 | 1215 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1216 | `		return PH7_OK;` |
|     - | 1217 | `	}` |
|     - | 1218 | `	/* Create a new array */` |
|     9 | 1219 | `	pArray = ph7_context_new_array(pCtx);` |
|     9 | 1220 | `	if( pArray == 0 ){` |
|     - | 1221 | `		/* Return NULL */` |
|   ! 0 | 1222 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1223 | `		return PH7_OK;` |
|     - | 1224 | `	}` |
|     9 | 1225 | `	if( nArg > 1 ){` |
|     3 | 1226 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|     1 | 1227 | `	}` |
|     - | 1228 | `	/* Fill the array */` |
|     - | 1229 | `	/* Seconds */` |
|     9 | 1230 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|     9 | 1231 | `	if( isAssoc ){` |
|     3 | 1232 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|     2 | 1233 | `	}else{` |
|     7 | 1234 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1235 | `	}` |
|     - | 1236 | `	/* Minutes */` |
|     9 | 1237 | `	ph7_value_int(pValue,sTm.tm_min);` |
|     9 | 1238 | `	if( isAssoc ){` |
|     3 | 1239 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|     2 | 1240 | `	}else{` |
|     7 | 1241 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1242 | `	}` |
|     - | 1243 | `	/* Hours */` |
|     9 | 1244 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|     9 | 1245 | `	if( isAssoc ){` |
|     3 | 1246 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|     2 | 1247 | `	}else{` |
|     7 | 1248 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1249 | `	}` |
|     - | 1250 | `	/* mday */` |
|     9 | 1251 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|     9 | 1252 | `	if( isAssoc ){` |
|     3 | 1253 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|     2 | 1254 | `	}else{` |
|     7 | 1255 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1256 | `	}` |
|     - | 1257 | `	/* mon */` |
|     9 | 1258 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|     9 | 1259 | `	if( isAssoc ){` |
|     3 | 1260 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|     2 | 1261 | `	}else{` |
|     7 | 1262 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1263 | `	}` |
|     - | 1264 | `	/* year since 1900 */` |
|     9 | 1265 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|     9 | 1266 | `	if( isAssoc ){` |
|     3 | 1267 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|     2 | 1268 | `	}else{` |
|     7 | 1269 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1270 | `	}` |
|     - | 1271 | `	/* wday */` |
|     9 | 1272 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|     9 | 1273 | `	if( isAssoc ){` |
|     3 | 1274 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|     2 | 1275 | `	}else{` |
|     7 | 1276 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1277 | `	}` |
|     - | 1278 | `	/* yday */` |
|     9 | 1279 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|     9 | 1280 | `	if( isAssoc ){` |
|     3 | 1281 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|     2 | 1282 | `	}else{` |
|     7 | 1283 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1284 | `	}` |
|     - | 1285 | `	/* isdst */` |
|     - | 1286 | `#ifdef __WINNT__` |
|     - | 1287 | `#ifdef _MSC_VER` |
|     - | 1288 | `#ifndef _WIN32_WCE` |
|     1 | 1289 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1290 | `#endif` |
|     - | 1291 | `#endif` |
|     - | 1292 | `#endif` |
|     9 | 1293 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|     9 | 1294 | `	if( isAssoc ){` |
|     3 | 1295 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|     2 | 1296 | `	}else{` |
|     7 | 1297 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|     - | 1298 | `	}` |
|     - | 1299 | `	/* Return the array */` |
|     9 | 1300 | `	ph7_result_value(pCtx,pArray);` |
|     9 | 1301 | `	return PH7_OK;` |
|     5 | 1302 | `}` |
|     - | 1303 | `/*` |
|     - | 1304 | ` * int idate(string $format [, int $timestamp = time() ])` |
|     - | 1305 | ` *  Returns a number formatted according to the given format string` |
|     - | 1306 | ` *  using the given integer timestamp or the current local time if` |
|     - | 1307 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|     - | 1308 | ` *  to the value of time().` |
|     - | 1309 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|     - | 1310 | ` *  parameter.` |
|     - | 1311 | ` * $Parameters` |
|     - | 1312 | ` *  Supported format` |
|     - | 1313 | ` *   d 	Day of the month` |
|     - | 1314 | ` *   h 	Hour (12 hour format)` |
|     - | 1315 | ` *   H 	Hour (24 hour format)` |
|     - | 1316 | ` *   i 	Minutes` |
|     - | 1317 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|     - | 1318 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|     - | 1319 | ` *   m 	Month number` |
|     - | 1320 | ` *   s 	Seconds` |
|     - | 1321 | ` *   t 	Days in current month` |
|     - | 1322 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|     - | 1323 | ` *   w 	Day of the week (0 on Sunday)` |
|     - | 1324 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|     - | 1325 | ` *   y 	Year (1 or 2 digits - check note below)` |
|     - | 1326 | ` *   Y 	Year (4 digits)` |
|     - | 1327 | ` *   z 	Day of the year` |
|     - | 1328 | ` *   Z 	Timezone offset in seconds` |
|     - | 1329 | ` * $timestamp` |
|     - | 1330 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|     - | 1331 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|     - | 1332 | ` *  to the value of time().` |
|     - | 1333 | ` * Return` |
|     - | 1334 | ` *  An integer.` |
|     - | 1335 | ` */` |
|    38 | 1336 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1337 | `{` |
|     - | 1338 | `	const char *zFormat;` |
|    40 | 1339 | `	ph7_int64 iVal = 0;` |
|     - | 1340 | `	int nLen;` |
|     - | 1341 | `	Sytm sTm;` |
|    40 | 1342 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1343 | `		/* Missing/Invalid argument,return -1 */` |
|   ! 0 | 1344 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1345 | `		return PH7_OK;` |
|     - | 1346 | `	}` |
|    40 | 1347 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    40 | 1348 | `	if( nLen < 1 ){` |
|     - | 1349 | `		/* Don't bother processing return -1*/` |
|   ! 0 | 1350 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1351 | `	}` |
|    40 | 1352 | `	if( nArg < 2 ){` |
|     - | 1353 | `#ifdef __WINNT__` |
|     - | 1354 | `		SYSTEMTIME sOS;` |
|     2 | 1355 | `		GetSystemTime(&sOS);` |
|     2 | 1356 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|     - | 1357 | `#else` |
|     - | 1358 | `		struct tm *pTm;` |
|     - | 1359 | `		time_t t;` |
|    28 | 1360 | `		time(&t);` |
|    28 | 1361 | `		pTm = gmtime(&t);` |
|    28 | 1362 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    28 | 1363 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1364 | `#endif` |
|    16 | 1365 | `	}else{` |
|     - | 1366 | `		/* Use the given timestamp */` |
|     - | 1367 | `		time_t t;` |
|     - | 1368 | `		struct tm *pTm;` |
|    11 | 1369 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    11 | 1370 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    11 | 1371 | `			pTm = gmtime(&t);` |
|    11 | 1372 | `			if( pTm == 0 ){` |
|   ! 0 | 1373 | `				time(&t);` |
|   ! 0 | 1374 | `			}` |
|     6 | 1375 | `		}else{` |
|   ! 0 | 1376 | `			time(&t);` |
|     - | 1377 | `		}` |
|    11 | 1378 | `		pTm = gmtime(&t);` |
|    11 | 1379 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    11 | 1380 | `		DtSytmFillOffset(&sTm,t);` |
|     - | 1381 | `	}` |
|     - | 1382 | `	/* Perform the requested operation */` |
|    40 | 1383 | `	switch(zFormat[0]){` |
|     2 | 1384 | `	case 'd':` |
|     - | 1385 | `		/* Day of the month */` |
|     5 | 1386 | `		iVal = sTm.tm_mday;` |
|     5 | 1387 | `		break;` |
|   ! 0 | 1388 | `	case 'h':` |
|     - | 1389 | `		/*	Hour (12 hour format)*/` |
|   ! 0 | 1390 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|   ! 0 | 1391 | `		break;` |
|     1 | 1392 | `	case 'H':` |
|     - | 1393 | `		/* Hour (24 hour format)*/` |
|     3 | 1394 | `		iVal = sTm.tm_hour;` |
|     3 | 1395 | `		break;` |
|     1 | 1396 | `	case 'i':` |
|     - | 1397 | `		/*Minutes*/` |
|     3 | 1398 | `		iVal = sTm.tm_min;` |
|     3 | 1399 | `		break;` |
|     1 | 1400 | `	case 'I':` |
|     - | 1401 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|     - | 1402 | `#ifdef __WINNT__` |
|     - | 1403 | `#ifdef _MSC_VER` |
|     - | 1404 | `#ifndef _WIN32_WCE` |
|     1 | 1405 | `			_get_daylight(&sTm.tm_isdst);` |
|     - | 1406 | `#endif` |
|     - | 1407 | `#endif` |
|     - | 1408 | `#endif` |
|     3 | 1409 | `		iVal = sTm.tm_isdst;` |
|     3 | 1410 | `		break;` |
|     1 | 1411 | `	case 'L':` |
|     - | 1412 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|     3 | 1413 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|     3 | 1414 | `		break;` |
|     2 | 1415 | `	case 'm':` |
|     - | 1416 | `		/* Month number*/` |
|     5 | 1417 | `		iVal = sTm.tm_mon;` |
|     5 | 1418 | `		break;` |
|     1 | 1419 | `	case 's':` |
|     - | 1420 | `		/*Seconds*/` |
|     3 | 1421 | `		iVal = sTm.tm_sec;` |
|     3 | 1422 | `		break;` |
|     1 | 1423 | `	case 't':{` |
|     - | 1424 | `		/*Days in current month*/` |
|     - | 1425 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|     3 | 1426 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|     3 | 1427 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|   ! 0 | 1428 | `			nDays = 28;` |
|   ! 0 | 1429 | `		}` |
|     3 | 1430 | `		iVal = nDays;` |
|     3 | 1431 | `		break;` |
|     - | 1432 | `			 }` |
|     1 | 1433 | `	case 'U':` |
|     - | 1434 | `		/*Seconds since the Unix Epoch*/` |
|     3 | 1435 | `		iVal = (ph7_int64)time(0);` |
|     3 | 1436 | `		break;` |
|     1 | 1437 | `	case 'w':` |
|     - | 1438 | `		/*	Day of the week (0 on Sunday) */` |
|     3 | 1439 | `		iVal = sTm.tm_wday;` |
|     3 | 1440 | `		break;` |
|     1 | 1441 | `	case 'W': {` |
|     - | 1442 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|     - | 1443 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|     3 | 1444 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|     3 | 1445 | `		break;` |
|     - | 1446 | `			  }` |
|   ! 0 | 1447 | `	case 'y':` |
|     - | 1448 | `		/* Year (2 digits) */` |
|   ! 0 | 1449 | `		iVal = sTm.tm_year % 100;` |
|   ! 0 | 1450 | `		break;` |
|     3 | 1451 | `	case 'Y':` |
|     - | 1452 | `		/* Year (4 digits) */` |
|     7 | 1453 | `		iVal = sTm.tm_year;` |
|     7 | 1454 | `		break;` |
|     1 | 1455 | `	case 'z':` |
|     - | 1456 | `		/* Day of the year */` |
|     3 | 1457 | `		iVal = sTm.tm_yday;` |
|     3 | 1458 | `		break;` |
|     1 | 1459 | `	case 'Z':` |
|     - | 1460 | `		/*Timezone offset in seconds*/` |
|     3 | 1461 | `		iVal = sTm.tm_gmtoff;` |
|     3 | 1462 | `		break;` |
|     1 | 1463 | `	default:` |
|     - | 1464 | `		/* unknown format,throw a warning */` |
|     3 | 1465 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|     2 | 1466 | `		break;` |
|     - | 1467 | `	}` |
|     - | 1468 | `	/* Return the time value */` |
|    40 | 1469 | `	ph7_result_int64(pCtx,iVal);` |
|    40 | 1470 | `	return PH7_OK;` |
|    21 | 1471 | `}` |
|     - | 1472 | `/*` |
|     - | 1473 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|     - | 1474 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|     - | 1475 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|     - | 1476 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|     - | 1477 | ` *  specified.` |
|     - | 1478 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|     - | 1479 | ` *  the current value according to the local date and time.` |
|     - | 1480 | ` * Parameters` |
|     - | 1481 | ` * $hour` |
|     - | 1482 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|     - | 1483 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|     - | 1484 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|     - | 1485 | ` * $minute` |
|     - | 1486 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|     - | 1487 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|     - | 1488 | ` *  in the following hour(s).` |
|     - | 1489 | ` * $second` |
|     - | 1490 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|     - | 1491 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|     - | 1492 | ` * second in the following minute(s).` |
|     - | 1493 | ` * $month` |
|     - | 1494 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|     - | 1495 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|     - | 1496 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|     - | 1497 | ` * $day` |
|     - | 1498 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|     - | 1499 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|     - | 1500 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|     - | 1501 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|     - | 1502 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|     - | 1503 | ` * $year` |
|     - | 1504 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|     - | 1505 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|     - | 1506 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|     - | 1507 | ` * $is_dst` |
|     - | 1508 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|     - | 1509 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|     - | 1510 | ` * Return` |
|     - | 1511 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|     - | 1512 | ` *   If the arguments are invalid, the function returns FALSE` |
|     - | 1513 | ` */` |
|    36 | 1514 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1515 | `{` |
|     - | 1516 | `	const char *zFunction;` |
|     - | 1517 | `	ph7_int64 iVal;` |
|     - | 1518 | `	sxi64 h,mi,s,mo,d,y,yAdj;` |
|     - | 1519 | `	int moN;` |
|     - | 1520 | `	struct tm *pTm;` |
|     - | 1521 | `	time_t t;` |
|     - | 1522 | `	/* Extract function name */` |
|    37 | 1523 | `	zFunction = ph7_function_name(pCtx);` |
|     - | 1524 | `	/* PHP 8 dropped the legacy $is_dst 7th parameter: mktime()/gmmktime() now` |
|     - | 1525 | `	 * accept at most 6 arguments and throw a catchable ArgumentCountError` |
|     - | 1526 | `	 * otherwise (the central aBuiltinArity table only enforces the minimum, so` |
|     - | 1527 | `	 * this maximum is checked here). */` |
|    37 | 1528 | `	if( nArg > 6 ){` |
|    10 | 1529 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     3 | 1530 | `			"%s() expects at most 6 arguments, %d given",zFunction,nArg);` |
|     - | 1531 | `	}` |
|    31 | 1532 | `	if( nArg < 1 ){` |
|   ! 0 | 1533 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1534 | `			"%s() expects at least 1 argument, 0 given",zFunction);` |
|     - | 1535 | `	}` |
|     - | 1536 | `	/* Missing components default from the current time in php's default` |
|     - | 1537 | `	 * timezone. PHL's date_default_timezone_set() only accepts UTC/GMT (no tz` |
|     - | 1538 | `	 * database), so mktime() and gmmktime() agree and both read gmtime(). */` |
|    31 | 1539 | `	time(&t);` |
|    31 | 1540 | `	pTm = gmtime(&t);` |
|    15 | 1541 | `	SXUNUSED(zFunction);` |
|    31 | 1542 | `	h  = pTm->tm_hour;` |
|    31 | 1543 | `	mi = pTm->tm_min;` |
|    31 | 1544 | `	s  = pTm->tm_sec;` |
|    31 | 1545 | `	mo = pTm->tm_mon + 1;` |
|    31 | 1546 | `	d  = pTm->tm_mday;` |
|    31 | 1547 | `	y  = pTm->tm_year + 1900;` |
|    31 | 1548 | `	h = ph7_value_to_int64(apArg[0]);` |
|    31 | 1549 | `	if( nArg > 1 ){` |
|    31 | 1550 | `		mi = ph7_value_to_int64(apArg[1]);` |
|    31 | 1551 | `		if( nArg > 2 ){` |
|    31 | 1552 | `			s = ph7_value_to_int64(apArg[2]);` |
|    31 | 1553 | `			if( nArg > 3 ){` |
|    31 | 1554 | `				mo = ph7_value_to_int64(apArg[3]);` |
|    31 | 1555 | `				if( nArg > 4 ){` |
|    31 | 1556 | `					d = ph7_value_to_int64(apArg[4]);` |
|    31 | 1557 | `					if( nArg > 5 ){` |
|     - | 1558 | `						/* php's legacy two-digit mapping: 0-69 -> 2000-2069,` |
|     - | 1559 | `						 * 70-100 -> 1970-2000; anything else is verbatim */` |
|    31 | 1560 | `						y = ph7_value_to_int64(apArg[5]);` |
|    31 | 1561 | `						if( y >= 0 && y <= 69 ){` |
|     7 | 1562 | `							y += 2000;` |
|    28 | 1563 | `						}else if( y >= 70 && y <= 100 ){` |
|     5 | 1564 | `							y += 1900;` |
|     2 | 1565 | `						}` |
|    15 | 1566 | `					}` |
|    15 | 1567 | `				}` |
|    15 | 1568 | `			}` |
|    15 | 1569 | `		}` |
|    15 | 1570 | `	}` |
|     - | 1571 | `	/* Normalize the month with floor semantics, then let day/time components` |
|     - | 1572 | `	 * overflow linearly (php: mktime(25,-30,0,1,1,2024) == Jan 2 00:30). */` |
|    31 | 1573 | `	yAdj = y + DtFloorDiv(mo - 1,12);` |
|    31 | 1574 | `	moN  = (int)(mo - 1 - DtFloorDiv(mo - 1,12) * 12) + 1;` |
|    31 | 1575 | `	iVal = (DtDaysFromCivil(yAdj,moN,1) + (d - 1)) * 86400 + h*3600 + mi*60 + s;` |
|     - | 1576 | `	/* Return the timestamp as a 64bit integer */` |
|    31 | 1577 | `	ph7_result_int64(pCtx,iVal);` |
|    31 | 1578 | `	return PH7_OK;` |
|    19 | 1579 | `}` |
|     - | 1580 | `/*` |
|     - | 1581 | ` * string date_default_timezone_get(void)` |
|     - | 1582 | ` *  Gets the default timezone used by all date/time functions in a script.` |
|     - | 1583 | ` */` |
|     4 | 1584 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1585 | `{` |
|     5 | 1586 | `	ph7_vm *pVm = pCtx->pVm;` |
|     2 | 1587 | `	SXUNUSED(nArg);` |
|     2 | 1588 | `	SXUNUSED(apArg);` |
|     5 | 1589 | `	ph7_result_string(pCtx,pVm->zDefTz,(int)pVm->nDefTz);` |
|     5 | 1590 | `	return PH7_OK;` |
|     1 | 1591 | `}` |
|     - | 1592 | `/*` |
|     - | 1593 | ` * bool date_default_timezone_set(string $timezoneId)` |
|     - | 1594 | ` *  Sets the default timezone used by all date/time functions in a script.` |
|     - | 1595 | ` *  php validates against the tz database and stores the id verbatim (get()` |
|     - | 1596 | ` *  echoes back "utc" if that's what was set). PHL ships no tz database, so` |
|     - | 1597 | ` *  only UTC and GMT are accepted; every other id — including region names php` |
|     - | 1598 | ` *  would accept — is rejected with php's invalid-id notice (recorded scope cut).` |
|     - | 1599 | ` */` |
|     6 | 1600 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1601 | `{` |
|     7 | 1602 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1603 | `	const char *zId;` |
|     - | 1604 | `	int nId;` |
|     7 | 1605 | `	if( nArg < 1 ){` |
|   ! 0 | 1606 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1607 | `		return PH7_OK;` |
|     - | 1608 | `	}` |
|     7 | 1609 | `	zId = ph7_value_to_string(apArg[0],&nId);` |
|     7 | 1610 | `	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 \|\| SyStrnicmp(zId,"GMT",3) == 0) ){` |
|     7 | 1611 | `		SyMemcpy(zId,pVm->zDefTz,3);` |
|     7 | 1612 | `		pVm->zDefTz[3] = 0;` |
|     7 | 1613 | `		pVm->nDefTz = 3;` |
|     7 | 1614 | `		ph7_result_bool(pCtx,1);` |
|     7 | 1615 | `		return PH7_OK;` |
|     - | 1616 | `	}` |
|     - | 1617 | `	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "` |
|     - | 1618 | `	 * — exactly php's notice shape here */` |
|   ! 0 | 1619 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);` |
|   ! 0 | 1620 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 1621 | `	return PH7_OK;` |
|     4 | 1622 | `}` |
|     - | 1623 |  |
|     - | 1624 | `/* ===========================================================================` |
|     - | 1625 | ` * DateTime family (NEWPLAN band D slice 1): DateTimeInterface, DateTime,` |
|     - | 1626 | ` * DateTimeImmutable, DateTimeZone (UTC + fixed offsets), date_create(),` |
|     - | 1627 | ` * date_create_immutable(). Embedded-PHP chunk + C thunks, following the` |
|     - | 1628 | ` * Reflection architecture (installed inside the bCompilingBuiltin window).` |
|     - | 1629 | ` * Timezone SCOPE: UTC and fixed "+HH:MM" offsets only — no tz database` |
|     - | 1630 | ` * (recorded §10 scope cut; named region zones throw like unknown zones).` |
|     - | 1631 | ` * ======================================================================== */` |
|     - | 1632 |  |
|     - | 1633 | `/*` |
|     - | 1634 | ` * Proleptic-Gregorian civil <-> day-count conversions (Howard Hinnant's` |
|     - | 1635 | ` * algorithms): no time_t / libc dependence, correct far past 2038 and` |
|     - | 1636 | ` * before 1970 on every platform. Day 0 == 1970-01-01.` |
|     - | 1637 | ` */` |
|   472 | 1638 | `static sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|     1 | 1639 | `{` |
|     - | 1640 | `	sxi64 era;` |
|     - | 1641 | `	unsigned yoe,doy,doe;` |
|   473 | 1642 | `	y -= (m <= 2);` |
|   473 | 1643 | `	era = (y >= 0 ? y : y - 399) / 400;` |
|   473 | 1644 | `	yoe = (unsigned)(y - era * 400);` |
|   473 | 1645 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|   473 | 1646 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|   473 | 1647 | `	return era * 146097 + (sxi64)doe - 719468;` |
|     1 | 1648 | `}` |
|   254 | 1649 | `static void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|     1 | 1650 | `{` |
|     - | 1651 | `	sxi64 era;` |
|     - | 1652 | `	unsigned doe,yoe,doy,mp;` |
|   255 | 1653 | `	z += 719468;` |
|   255 | 1654 | `	era = (z >= 0 ? z : z - 146096) / 146097;` |
|   255 | 1655 | `	doe = (unsigned)(z - era * 146097);` |
|   255 | 1656 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|   255 | 1657 | `	*py = (sxi64)yoe + era * 400;` |
|   255 | 1658 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|   255 | 1659 | `	mp = (5 * doy + 2) / 153;` |
|   255 | 1660 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|   255 | 1661 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|   255 | 1662 | `	if( *pm <= 2 ){` |
|   155 | 1663 | `		*py += 1;` |
|    77 | 1664 | `	}` |
|   255 | 1665 | `}` |
|   440 | 1666 | `static sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|     1 | 1667 | `{` |
|   441 | 1668 | `	sxi64 q = a / b;` |
|   441 | 1669 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|     3 | 1670 | `		q--;` |
|     1 | 1671 | `	}` |
|   441 | 1672 | `	return q;` |
|     1 | 1673 | `}` |
|     - | 1674 | `/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z) */` |
|   130 | 1675 | `static void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)` |
|     1 | 1676 | `{` |
|   131 | 1677 | `	sxi64 t = iTs + iOff;` |
|   131 | 1678 | `	sxi64 days = DtFloorDiv(t,86400);` |
|   131 | 1679 | `	sxi64 secs = t - days * 86400;` |
|     - | 1680 | `	sxi64 y;` |
|     - | 1681 | `	int mo,d;` |
|   131 | 1682 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|   131 | 1683 | `	pTm->tm_sec  = (int)(secs % 60);` |
|   131 | 1684 | `	pTm->tm_min  = (int)((secs / 60) % 60);` |
|   131 | 1685 | `	pTm->tm_hour = (int)(secs / 3600);` |
|   131 | 1686 | `	pTm->tm_mday = d;` |
|   131 | 1687 | `	pTm->tm_mon  = mo - 1;` |
|   131 | 1688 | `	pTm->tm_year = (int)y;` |
|   131 | 1689 | `	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */` |
|   131 | 1690 | `	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));` |
|   131 | 1691 | `	pTm->tm_isdst = 0;` |
|   131 | 1692 | `	pTm->tm_zone = zZone;` |
|   131 | 1693 | `	pTm->tm_gmtoff = (long)iOff;` |
|   131 | 1694 | `}` |
|    94 | 1695 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|     1 | 1696 | `{` |
|    95 | 1697 | `	return DtDaysFromCivil(y,mo,d) * 86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     1 | 1698 | `}` |
|     - | 1699 | `/* Month-arithmetic with php's overflow semantics (Jan 31 +1 month -> Mar 2/3):` |
|     - | 1700 | ` * normalize the month, keep the day — the civil day-count formula is linear in` |
|     - | 1701 | ` * d, so an out-of-range day simply lands in the following month. */` |
|     4 | 1702 | `static sxi64 DtAddMonths(sxi64 iTs,sxi32 iOff,sxi64 nMonths)` |
|     1 | 1703 | `{` |
|     5 | 1704 | `	sxi64 t = iTs + iOff;` |
|     5 | 1705 | `	sxi64 days = DtFloorDiv(t,86400);` |
|     5 | 1706 | `	sxi64 secs = t - days * 86400;` |
|     - | 1707 | `	sxi64 y;` |
|     - | 1708 | `	int mo,d;` |
|     - | 1709 | `	sxi64 m0;` |
|     5 | 1710 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|     5 | 1711 | `	m0 = (y * 12 + (mo - 1)) + nMonths;` |
|     5 | 1712 | `	y  = DtFloorDiv(m0,12);` |
|     5 | 1713 | `	mo = (int)(m0 - y * 12) + 1;` |
|     5 | 1714 | `	return DtDaysFromCivil(y,mo,d) * 86400 + secs - iOff;` |
|     1 | 1715 | `}` |
|     - | 1716 | `/*` |
|     - | 1717 | ` * Minimal php-datetime-string parser (slice 1): absolute forms` |
|     - | 1718 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]",` |
|     - | 1719 | ` * keywords today/midnight/noon/tomorrow/yesterday, and relative sequences` |
|     - | 1720 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". Returns 0 on` |
|     - | 1721 | ` * success (ts/off/bOffSet out), or the byte position of the first` |
|     - | 1722 | ` * unparseable character +1 (for php's "at position N" message).` |
|     - | 1723 | ` */` |
|   144 | 1724 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,` |
|     - | 1725 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet)` |
|     1 | 1726 | `{` |
|   145 | 1727 | `	const char *z = zIn, *zEnd = &zIn[nLen];` |
|   145 | 1728 | `	sxi64 iTs = iBaseTs;` |
|   145 | 1729 | `	sxi32 iOff = iBaseOff;` |
|   145 | 1730 | `	int bOffSet = 0;` |
|   145 | 1731 | `	int bAny = 0;` |
|     - | 1732 | `#define DT_SKIP_WS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|     - | 1733 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|     - | 1734 | `	&& (zEnd-z == (nKw) \|\| !SyisAlpha(z[(nKw)])))` |
|   217 | 1735 | `	DT_SKIP_WS();` |
|   145 | 1736 | `	if( z >= zEnd ){` |
|     - | 1737 | `		/* php: the empty string is "now" */` |
|   ! 0 | 1738 | `		*pTs = iTs;` |
|   ! 0 | 1739 | `		*pOff = iOff;` |
|   ! 0 | 1740 | `		*pbOffSet = bOffSet;` |
|   ! 0 | 1741 | `		return 0;` |
|     - | 1742 | `	}` |
|     - | 1743 | `	/* "@<seconds>" absolute epoch */` |
|   145 | 1744 | `	if( z[0] == '@' ){` |
|    57 | 1745 | `		int neg = 0;` |
|    57 | 1746 | `		sxi64 v = 0;` |
|    57 | 1747 | `		const char *zAt = z;` |
|    57 | 1748 | `		z++;` |
|    57 | 1749 | `		if( z < zEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|     - | 1750 | `		/* php's lexer rejects the whole token: the error points at the '@' */` |
|    57 | 1751 | `		if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zAt - zIn) + 1; }` |
|   137 | 1752 | `		while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|    55 | 1753 | `		*pTs = neg ? -v : v;` |
|    55 | 1754 | `		*pOff = 0;` |
|    55 | 1755 | `		*pbOffSet = 1;` |
|    55 | 1756 | `		DT_SKIP_WS();` |
|    55 | 1757 | `		return (z < zEnd) ? (int)(z - zIn) + 1 : 0;` |
|     - | 1758 | `	}` |
|     - | 1759 | `	/* Absolute date: YYYY-MM-DD[...] */` |
|    88 | 1760 | `	if( zEnd-z >= 10 && SyisDigit(z[0]) && SyisDigit(z[1]) && SyisDigit(z[2])` |
|    69 | 1761 | `	 && SyisDigit(z[3]) && z[4]=='-' ){` |
|    69 | 1762 | `		sxi64 y = (z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0');` |
|    69 | 1763 | `		int mo,d,h=0,mi=0,s=0;` |
|    69 | 1764 | `		if( !SyisDigit(z[5])\|\|!SyisDigit(z[6])\|\|z[7] != '-'\|\|!SyisDigit(z[8])\|\|!SyisDigit(z[9]) ){` |
|   ! 0 | 1765 | `			return (int)(z - zIn) + 1;` |
|     - | 1766 | `		}` |
|    69 | 1767 | `		mo = (z[5]-'0')*10 + (z[6]-'0');` |
|    69 | 1768 | `		d  = (z[8]-'0')*10 + (z[9]-'0');` |
|     - | 1769 | `		/* php's lexer dies on the SECOND digit of an out-of-range month/day` |
|     - | 1770 | `		 * (either the two-digit pattern fails there, or a one-digit component` |
|     - | 1771 | `		 * matched and the separator check fails there); "00" lexes fine and` |
|     - | 1772 | `		 * normalizes (month 0 == December of the previous year). */` |
|    69 | 1773 | `		if( mo > 12 ){ return (int)(&z[6] - zIn) + 1; }` |
|    65 | 1774 | `		if( d > 31 ){ return (int)(&z[9] - zIn) + 1; }` |
|    63 | 1775 | `		if( mo == 0 ){ mo = 12; y--; }` |
|    63 | 1776 | `		z += 10;` |
|    62 | 1777 | `		if( z < zEnd && (z[0]=='T' \|\| z[0]==' ') && zEnd-z >= 6` |
|    35 | 1778 | `		 && SyisDigit(z[1]) && SyisDigit(z[2]) && z[3]==':' ){` |
|    35 | 1779 | `			z++;` |
|    35 | 1780 | `			h  = (z[0]-'0')*10 + (z[1]-'0');` |
|    35 | 1781 | `			mi = (z[3]-'0')*10 + (z[4]-'0');` |
|     - | 1782 | `			/* a 25+ hour kills php's whole time token: error at its start */` |
|    35 | 1783 | `			if( h > 24 ){ return (int)(z - zIn) + 1; }` |
|     - | 1784 | `			/* php lexes HH:M, then the minute's second digit starts a SECOND` |
|     - | 1785 | `			 * time token: "Double time specification" (negative encoding) */` |
|    33 | 1786 | `			if( mi > 59 ){ return -((int)(&z[4] - zIn) + 1); }` |
|    31 | 1787 | `			z += 5;` |
|    30 | 1788 | `			if( z+2 < zEnd+1 && z < zEnd && z[0]==':' && zEnd-z >= 3` |
|    31 | 1789 | `			 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|    31 | 1790 | `				s = (z[1]-'0')*10 + (z[2]-'0');` |
|    31 | 1791 | `				if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|    29 | 1792 | `				z += 3;` |
|    14 | 1793 | `			}` |
|    29 | 1794 | `			if( z < zEnd && z[0]=='.' ){ /* fractional seconds: consume */` |
|   ! 0 | 1795 | `				z++;` |
|   ! 0 | 1796 | `				while( z < zEnd && SyisDigit(z[0]) ){ z++; }` |
|   ! 0 | 1797 | `			}` |
|    29 | 1798 | `			if( z < zEnd && (z[0]=='Z' \|\| z[0]=='z') ){` |
|     - | 1799 | `				/* 2 = explicit "Z" zone: php names it "Z", not "+00:00" */` |
|     3 | 1800 | `				iOff = 0; bOffSet = 2; z++;` |
|    28 | 1801 | `			}else if( z < zEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|     3 | 1802 | `				int sign = (z[0]=='-') ? -1 : 1;` |
|     3 | 1803 | `				int oh,om = 0;` |
|     3 | 1804 | `				z++;` |
|     3 | 1805 | `				if( zEnd-z < 2 \|\| !SyisDigit(z[0]) \|\| !SyisDigit(z[1]) ){ return (int)(z - zIn) + 1; }` |
|     3 | 1806 | `				oh = (z[0]-'0')*10 + (z[1]-'0');` |
|     3 | 1807 | `				z += 2;` |
|     3 | 1808 | `				if( z < zEnd && z[0]==':' ){ z++; }` |
|     3 | 1809 | `				if( zEnd-z >= 2 && SyisDigit(z[0]) && SyisDigit(z[1]) ){` |
|     3 | 1810 | `					om = (z[0]-'0')*10 + (z[1]-'0');` |
|     3 | 1811 | `					z += 2;` |
|     1 | 1812 | `				}` |
|     3 | 1813 | `				iOff = sign * (oh*3600 + om*60);` |
|     3 | 1814 | `				bOffSet = 1;` |
|     1 | 1815 | `			}` |
|    14 | 1816 | `		}` |
|    57 | 1817 | `		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    57 | 1818 | `		bAny = 1;` |
|    49 | 1819 | `	}else if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|     7 | 1820 | `	 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|     - | 1821 | `		/* Time-only: HH:MM[:SS] on the base date */` |
|     7 | 1822 | `		sxi64 t = iTs + iOff;` |
|     7 | 1823 | `		sxi64 days = DtFloorDiv(t,86400);` |
|     7 | 1824 | `		int h  = (z[0]-'0')*10 + (z[1]-'0');` |
|     7 | 1825 | `		int mi = (z[3]-'0')*10 + (z[4]-'0');` |
|     7 | 1826 | `		int s = 0;` |
|     - | 1827 | `		/* php: bad hour kills the token (error at its start); bad minute /` |
|     - | 1828 | `		 * second dies on the component's second digit */` |
|     7 | 1829 | `		if( h > 24 ){ return (int)(z - zIn) + 1; }` |
|     5 | 1830 | `		if( mi > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|     3 | 1831 | `		z += 5;` |
|     3 | 1832 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     3 | 1833 | `			s = (z[1]-'0')*10 + (z[2]-'0');` |
|     3 | 1834 | `			if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|   ! 0 | 1835 | `			z += 3;` |
|   ! 0 | 1836 | `		}` |
|   ! 0 | 1837 | `		iTs = days*86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|   ! 0 | 1838 | `		bAny = 1;` |
|    15 | 1839 | `	}else if( DT_LOWEQ("now",3) ){` |
|   ! 0 | 1840 | `		z += 3;` |
|   ! 0 | 1841 | `		bAny = 1;` |
|   ! 0 | 1842 | `	}` |
|     - | 1843 | `	/* Relative / keyword sequence */` |
|    35 | 1844 | `	for(;;){` |
|    84 | 1845 | `		DT_SKIP_WS();` |
|    77 | 1846 | `		if( z >= zEnd ){` |
|    63 | 1847 | `			break;` |
|     - | 1848 | `		}` |
|    15 | 1849 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|   ! 0 | 1850 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|   ! 0 | 1851 | `			iTs = days*86400 - iOff;` |
|   ! 0 | 1852 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|   ! 0 | 1853 | `			bAny = 1;` |
|   ! 0 | 1854 | `			continue;` |
|     - | 1855 | `		}` |
|    15 | 1856 | `		if( DT_LOWEQ("noon",4) ){` |
|   ! 0 | 1857 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|   ! 0 | 1858 | `			iTs = days*86400 + 12*3600 - iOff;` |
|   ! 0 | 1859 | `			z += 4;` |
|   ! 0 | 1860 | `			bAny = 1;` |
|   ! 0 | 1861 | `			continue;` |
|     - | 1862 | `		}` |
|    15 | 1863 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|   ! 0 | 1864 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) + 1;` |
|   ! 0 | 1865 | `			iTs = days*86400 - iOff;` |
|   ! 0 | 1866 | `			z += 8;` |
|   ! 0 | 1867 | `			bAny = 1;` |
|   ! 0 | 1868 | `			continue;` |
|     - | 1869 | `		}` |
|    15 | 1870 | `		if( DT_LOWEQ("yesterday",9) ){` |
|   ! 0 | 1871 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) - 1;` |
|   ! 0 | 1872 | `			iTs = days*86400 - iOff;` |
|   ! 0 | 1873 | `			z += 9;` |
|   ! 0 | 1874 | `			bAny = 1;` |
|   ! 0 | 1875 | `			continue;` |
|     - | 1876 | `		}` |
|    15 | 1877 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|     7 | 1878 | `			int neg = 0;` |
|     7 | 1879 | `			sxi64 v = 0;` |
|     7 | 1880 | `			const char *zNumStart = z;` |
|     7 | 1881 | `			if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; }` |
|     7 | 1882 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zNumStart - zIn) + 1; }` |
|    13 | 1883 | `			while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|     7 | 1884 | `			if( neg ){ v = -v; }` |
|    16 | 1885 | `			DT_SKIP_WS();` |
|     7 | 1886 | `			if( DT_LOWEQ("seconds",7) )     { iTs += v;            z += 7; }` |
|     7 | 1887 | `			else if( DT_LOWEQ("second",6) ) { iTs += v;            z += 6; }` |
|     7 | 1888 | `			else if( DT_LOWEQ("secs",4) )   { iTs += v;            z += 4; }` |
|     7 | 1889 | `			else if( DT_LOWEQ("sec",3) )    { iTs += v;            z += 3; }` |
|     7 | 1890 | `			else if( DT_LOWEQ("minutes",7) ){ iTs += v*60;         z += 7; }` |
|     7 | 1891 | `			else if( DT_LOWEQ("minute",6) ) { iTs += v*60;         z += 6; }` |
|     7 | 1892 | `			else if( DT_LOWEQ("mins",4) )   { iTs += v*60;         z += 4; }` |
|     7 | 1893 | `			else if( DT_LOWEQ("min",3) )    { iTs += v*60;         z += 3; }` |
|     7 | 1894 | `			else if( DT_LOWEQ("hours",5) )  { iTs += v*3600;       z += 5; }` |
|     7 | 1895 | `			else if( DT_LOWEQ("hour",4) )   { iTs += v*3600;       z += 4; }` |
|     7 | 1896 | `			else if( DT_LOWEQ("days",4) )   { iTs += v*86400;      z += 4; }` |
|     7 | 1897 | `			else if( DT_LOWEQ("day",3) )    { iTs += v*86400;      z += 3; }` |
|     5 | 1898 | `			else if( DT_LOWEQ("weeks",5) )  { iTs += v*7*86400;    z += 5; }` |
|     5 | 1899 | `			else if( DT_LOWEQ("week",4) )   { iTs += v*7*86400;    z += 4; }` |
|     5 | 1900 | `			else if( DT_LOWEQ("fortnights",10) ){ iTs += v*14*86400; z += 10; }` |
|     5 | 1901 | `			else if( DT_LOWEQ("fortnight",9) )  { iTs += v*14*86400; z += 9; }` |
|     5 | 1902 | `			else if( DT_LOWEQ("months",6) ) { iTs = DtAddMonths(iTs,iOff,v); z += 6; }` |
|     3 | 1903 | `			else if( DT_LOWEQ("month",5) )  { iTs = DtAddMonths(iTs,iOff,v); z += 5; }` |
|   ! 0 | 1904 | `			else if( DT_LOWEQ("years",5) )  { iTs = DtAddMonths(iTs,iOff,v*12); z += 5; }` |
|   ! 0 | 1905 | `			else if( DT_LOWEQ("year",4) )   { iTs = DtAddMonths(iTs,iOff,v*12); z += 4; }` |
|     - | 1906 | `			else{` |
|   ! 0 | 1907 | `				return (int)(z - zIn) + 1;` |
|     - | 1908 | `			}` |
|     7 | 1909 | `			bAny = 1;` |
|     7 | 1910 | `			continue;` |
|     - | 1911 | `		}` |
|     9 | 1912 | `		return (int)(z - zIn) + 1;` |
|   ! 0 | 1913 | `	}` |
|    63 | 1914 | `	if( !bAny ){` |
|   ! 0 | 1915 | `		return 1;` |
|     - | 1916 | `	}` |
|    63 | 1917 | `	*pTs = iTs;` |
|    63 | 1918 | `	*pOff = iOff;` |
|    63 | 1919 | `	*pbOffSet = bOffSet;` |
|    63 | 1920 | `	return 0;` |
|     - | 1921 | `#undef DT_SKIP_WS` |
|     - | 1922 | `#undef DT_LOWEQ` |
|    73 | 1923 | `}` |
|     - | 1924 | `/* int __dt_now() */` |
|   180 | 1925 | `static int vm_builtin_dt_now(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1926 | `{` |
|    90 | 1927 | `	SXUNUSED(nArg);` |
|    90 | 1928 | `	SXUNUSED(apArg);` |
|   181 | 1929 | `	ph7_result_int64(pCtx,(ph7_int64)time(0));` |
|   181 | 1930 | `	return PH7_OK;` |
|     1 | 1931 | `}` |
|     - | 1932 | `/* mixed __dt_parse(string $s, int $baseTs, int $baseOff)` |
|     - | 1933 | ` *   -> [ts, off, offWasExplicit] on success; php's error MESSAGE string on` |
|     - | 1934 | ` *      failure (the chunk wraps it in DateMalformedStringException). */` |
|   144 | 1935 | `static int vm_builtin_dt_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1936 | `{` |
|     - | 1937 | `	const char *zIn;` |
|     - | 1938 | `	int nLen;` |
|     - | 1939 | `	sxi64 iBaseTs;` |
|     - | 1940 | `	sxi32 iBaseOff;` |
|   145 | 1941 | `	sxi64 iTs = 0;` |
|   145 | 1942 | `	sxi32 iOff = 0;` |
|   145 | 1943 | `	int bOffSet = 0;` |
|     - | 1944 | `	int iErrPos;` |
|   145 | 1945 | `	if( nArg < 3 ){` |
|   ! 0 | 1946 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1947 | `		return PH7_OK;` |
|     - | 1948 | `	}` |
|   145 | 1949 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   145 | 1950 | `	iBaseTs  = ph7_value_to_int64(apArg[1]);` |
|   145 | 1951 | `	iBaseOff = (sxi32)ph7_value_to_int64(apArg[2]);` |
|   145 | 1952 | `	iErrPos = DtParse(zIn,nLen,iBaseTs,iBaseOff,&iTs,&iOff,&bOffSet);` |
|   145 | 1953 | `	if( iErrPos != 0 ){` |
|     - | 1954 | `		/* Negative encoding: php's "Double time specification" reason */` |
|    29 | 1955 | `		int bDouble = iErrPos < 0;` |
|    29 | 1956 | `		int iPos = (bDouble ? -iErrPos : iErrPos) - 1;` |
|    29 | 1957 | `		char cAt = (iPos < nLen) ? zIn[iPos] : ' ';` |
|     - | 1958 | `		/* php appends a reason: an alphabetic token is assumed to be a timezone` |
|     - | 1959 | `		 * lookup miss, anything else an unexpected character. */` |
|    56 | 1960 | `		ph7_result_string_format(pCtx,` |
|     - | 1961 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    14 | 1962 | `			nLen,zIn,iPos,cAt,` |
|    27 | 1963 | `			bDouble ? "Double time specification"` |
|    26 | 1964 | `			: ((cAt >= 'a' && cAt <= 'z') \|\| (cAt >= 'A' && cAt <= 'Z'))` |
|     - | 1965 | `				? "The timezone could not be found in the database"` |
|    26 | 1966 | `				: "Unexpected character");` |
|    29 | 1967 | `		return PH7_OK;` |
|     - | 1968 | `	}` |
|     - | 1969 | `	{` |
|   117 | 1970 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|   117 | 1971 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|   117 | 1972 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 1973 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 1974 | `		}` |
|   117 | 1975 | `		ph7_value_int64(pV,iTs);` |
|   117 | 1976 | `		ph7_array_add_elem(pArr,0,pV);` |
|   117 | 1977 | `		ph7_value_int64(pV,iOff);` |
|   117 | 1978 | `		ph7_array_add_elem(pArr,0,pV);` |
|     - | 1979 | `		/* int, not bool: 0 = no explicit offset, 1 = numeric offset/@epoch,` |
|     - | 1980 | `		 * 2 = literal "Z" (php keeps the distinction in the zone name) */` |
|   117 | 1981 | `		ph7_value_int64(pV,bOffSet);` |
|   117 | 1982 | `		ph7_array_add_elem(pArr,0,pV);` |
|   117 | 1983 | `		ph7_result_value(pCtx,pArr);` |
|     - | 1984 | `	}` |
|   117 | 1985 | `	return PH7_OK;` |
|    73 | 1986 | `}` |
|     - | 1987 | `/* string __dt_default_tz(void) — the date_default_timezone_set() identifier */` |
|   180 | 1988 | `static int vm_builtin_dt_default_tz(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1989 | `{` |
|    90 | 1990 | `	SXUNUSED(nArg);` |
|    90 | 1991 | `	SXUNUSED(apArg);` |
|   181 | 1992 | `	ph7_result_string(pCtx,pCtx->pVm->zDefTz,(int)pCtx->pVm->nDefTz);` |
|   181 | 1993 | `	return PH7_OK;` |
|     1 | 1994 | `}` |
|     - | 1995 | `/* string __dt_format(int $ts, int $off, string $tzname, string $format) */` |
|   130 | 1996 | `static int vm_builtin_dt_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1997 | `{` |
|     - | 1998 | `	Sytm sTm;` |
|     - | 1999 | `	sxi64 iTs;` |
|     - | 2000 | `	sxi32 iOff;` |
|     - | 2001 | `	const char *zName,*zFmt;` |
|     - | 2002 | `	int nName,nFmt;` |
|     - | 2003 | `	char zZone[64];` |
|   131 | 2004 | `	if( nArg < 4 ){` |
|   ! 0 | 2005 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2006 | `		return PH7_OK;` |
|     - | 2007 | `	}` |
|   131 | 2008 | `	iTs  = ph7_value_to_int64(apArg[0]);` |
|   131 | 2009 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|   131 | 2010 | `	zName = ph7_value_to_string(apArg[2],&nName);` |
|   131 | 2011 | `	zFmt  = ph7_value_to_string(apArg[3],&nFmt);` |
|   131 | 2012 | `	if( nName >= (int)sizeof(zZone) ){ nName = (int)sizeof(zZone) - 1; }` |
|   131 | 2013 | `	SyMemcpy(zName,zZone,(sxu32)nName);` |
|   131 | 2014 | `	zZone[nName] = 0;` |
|   131 | 2015 | `	DtFillSytm(iTs,iOff,zZone,&sTm);` |
|   131 | 2016 | `	DateFormat(pCtx,zFmt,nFmt,&sTm);` |
|   131 | 2017 | `	return PH7_OK;` |
|    66 | 2018 | `}` |
|     - | 2019 | `/* int __dt_make(int y, int mo, int d, int h, int i, int s, int off) */` |
|     4 | 2020 | `static int vm_builtin_dt_make(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2021 | `{` |
|     - | 2022 | `	sxi64 y;` |
|     - | 2023 | `	int mo,d,h,mi,s;` |
|     - | 2024 | `	sxi32 iOff;` |
|     5 | 2025 | `	if( nArg < 7 ){` |
|   ! 0 | 2026 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2027 | `		return PH7_OK;` |
|     - | 2028 | `	}` |
|     5 | 2029 | `	y   = ph7_value_to_int64(apArg[0]);` |
|     5 | 2030 | `	mo  = ph7_value_to_int(apArg[1]);` |
|     5 | 2031 | `	d   = ph7_value_to_int(apArg[2]);` |
|     5 | 2032 | `	h   = ph7_value_to_int(apArg[3]);` |
|     5 | 2033 | `	mi  = ph7_value_to_int(apArg[4]);` |
|     5 | 2034 | `	s   = ph7_value_to_int(apArg[5]);` |
|     5 | 2035 | `	iOff = (sxi32)ph7_value_to_int64(apArg[6]);` |
|     5 | 2036 | `	ph7_result_int64(pCtx,DtMakeTs(y,mo,d,h,mi,s,iOff));` |
|     5 | 2037 | `	return PH7_OK;` |
|     3 | 2038 | `}` |
|     - | 2039 | `/* Days in a civil month (php's overflow rules use it during diff borrows) */` |
|    50 | 2040 | `static int DtDaysInMonth(sxi64 y,int m)` |
|     1 | 2041 | `{` |
|     - | 2042 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|    51 | 2043 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|     9 | 2044 | `		return 29;` |
|     - | 2045 | `	}` |
|    43 | 2046 | `	return aMonDays[(m - 1) % 12];` |
|    26 | 2047 | `}` |
|     - | 2048 | `/* int __dt_civil_add(int ts, int off, int y, int m, int d, int h, int i,` |
|     - | 2049 | ` *                    int s, int sign)` |
|     - | 2050 | ` *   php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|     - | 2051 | ` *   (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset. */` |
|    54 | 2052 | `static int vm_builtin_dt_civil_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2053 | `{` |
|     - | 2054 | `	sxi64 iTs,iLocal,iDays,iSecs,y0,moT,dayCount;` |
|     - | 2055 | `	sxi32 iOff;` |
|     - | 2056 | `	int mo0,d0,iSign;` |
|     - | 2057 | `	sxi64 y,m,d,h,i,s;` |
|    55 | 2058 | `	if( nArg < 9 ){` |
|   ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2060 | `		return PH7_OK;` |
|     - | 2061 | `	}` |
|    55 | 2062 | `	iTs   = ph7_value_to_int64(apArg[0]);` |
|    55 | 2063 | `	iOff  = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    55 | 2064 | `	y     = ph7_value_to_int64(apArg[2]);` |
|    55 | 2065 | `	m     = ph7_value_to_int64(apArg[3]);` |
|    55 | 2066 | `	d     = ph7_value_to_int64(apArg[4]);` |
|    55 | 2067 | `	h     = ph7_value_to_int64(apArg[5]);` |
|    55 | 2068 | `	i     = ph7_value_to_int64(apArg[6]);` |
|    55 | 2069 | `	s     = ph7_value_to_int64(apArg[7]);` |
|    55 | 2070 | `	iSign = ph7_value_to_int(apArg[8]) < 0 ? -1 : 1;` |
|    55 | 2071 | `	iLocal = iTs + iOff;` |
|    55 | 2072 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    55 | 2073 | `	iSecs  = iLocal - iDays*86400;` |
|    55 | 2074 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    55 | 2075 | `	y0 += iSign * y;` |
|    55 | 2076 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    55 | 2077 | `	y0 += DtFloorDiv(moT,12);` |
|    55 | 2078 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    55 | 2079 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    55 | 2080 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    55 | 2081 | `	ph7_result_int64(pCtx,iLocal - iOff);` |
|    55 | 2082 | `	return PH7_OK;` |
|    28 | 2083 | `}` |
|     - | 2084 | `/* array __dt_civil_diff(int ts1, int off1, int ts2)` |
|     - | 2085 | ` *   -> [y,m,d,h,i,s,days,invert]: timelib's breakdown — field-wise deltas in` |
|     - | 2086 | ` *   the FIRST operand's offset, then borrow seconds→minutes→hours→days, then` |
|     - | 2087 | ` *   the day borrow walks whole months backward from the later date (that walk` |
|     - | 2088 | ` *   is why Jan 31 → Mar 02 reports m=0 d=30, not "1 month"). */` |
|    14 | 2089 | `static int vm_builtin_dt_civil_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2090 | `{` |
|     - | 2091 | `	sxi64 iTs1,iTs2,iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|     - | 2092 | `	sxi32 iOff;` |
|     - | 2093 | `	int moA,dA,moB,dB,bInvert;` |
|     - | 2094 | `	sxi64 sA,sB,y,m,d,h,i,s;` |
|     - | 2095 | `	ph7_value *pArr,*pV;` |
|    15 | 2096 | `	if( nArg < 3 ){` |
|   ! 0 | 2097 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2098 | `		return PH7_OK;` |
|     - | 2099 | `	}` |
|    15 | 2100 | `	iTs1 = ph7_value_to_int64(apArg[0]);` |
|    15 | 2101 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    15 | 2102 | `	iTs2 = ph7_value_to_int64(apArg[2]);` |
|    15 | 2103 | `	bInvert = iTs1 > iTs2;` |
|    15 | 2104 | `	iA = bInvert ? iTs2 : iTs1;` |
|    15 | 2105 | `	iB = bInvert ? iTs1 : iTs2;` |
|    15 | 2106 | `	iLa = iA + iOff;` |
|    15 | 2107 | `	iLb = iB + iOff;` |
|    15 | 2108 | `	daysA = DtFloorDiv(iLa,86400);` |
|    15 | 2109 | `	daysB = DtFloorDiv(iLb,86400);` |
|    15 | 2110 | `	sA = iLa - daysA*86400;` |
|    15 | 2111 | `	sB = iLb - daysB*86400;` |
|    15 | 2112 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|    15 | 2113 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|    15 | 2114 | `	s = (sB % 60) - (sA % 60);` |
|    15 | 2115 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|    15 | 2116 | `	h = (sB / 3600) - (sA / 3600);` |
|    15 | 2117 | `	d = dB - dA;` |
|    15 | 2118 | `	m = moB - moA;` |
|    15 | 2119 | `	y = yB - yA;` |
|    15 | 2120 | `	if( s < 0 ){ s += 60; i--; }` |
|    15 | 2121 | `	if( i < 0 ){ i += 60; h--; }` |
|    15 | 2122 | `	if( h < 0 ){ h += 24; d--; }` |
|    27 | 2123 | `	while( d < 0 ){` |
|    13 | 2124 | `		moB--;` |
|    13 | 2125 | `		if( moB < 1 ){ moB = 12; yB--; }` |
|    13 | 2126 | `		d += DtDaysInMonth(yB,moB);` |
|    13 | 2127 | `		m--;` |
|     1 | 2128 | `	}` |
|    15 | 2129 | `	if( m < 0 ){ m += 12; y--; }` |
|    15 | 2130 | `	pArr = ph7_context_new_array(pCtx);` |
|    15 | 2131 | `	pV = ph7_context_new_scalar(pCtx);` |
|    15 | 2132 | `	if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2133 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2134 | `	}` |
|    15 | 2135 | `	ph7_value_int64(pV,y);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2136 | `	ph7_value_int64(pV,m);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2137 | `	ph7_value_int64(pV,d);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2138 | `	ph7_value_int64(pV,h);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2139 | `	ph7_value_int64(pV,i);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2140 | `	ph7_value_int64(pV,s);  ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2141 | `	ph7_value_int64(pV,(iB - iA) / 86400); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2142 | `	ph7_value_int64(pV,bInvert); ph7_array_add_elem(pArr,0,pV);` |
|    15 | 2143 | `	ph7_result_value(pCtx,pArr);` |
|    15 | 2144 | `	return PH7_OK;` |
|     8 | 2145 | `}` |
|     - | 2146 | `/* int __dt_isodate(int ts, int off, int y, int w, int dow)` |
|     - | 2147 | ` *   setISODate: jump to ISO year/week/weekday, preserving the time of day. */` |
|     8 | 2148 | `static int vm_builtin_dt_isodate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2149 | `{` |
|     - | 2150 | `	sxi64 iTs,iLocal,iTod,jan4,monday1,target,y;` |
|     - | 2151 | `	sxi32 iOff;` |
|     - | 2152 | `	sxi64 w,dow;` |
|     - | 2153 | `	int isoDow;` |
|     9 | 2154 | `	if( nArg < 5 ){` |
|   ! 0 | 2155 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2156 | `		return PH7_OK;` |
|     - | 2157 | `	}` |
|     9 | 2158 | `	iTs = ph7_value_to_int64(apArg[0]);` |
|     9 | 2159 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|     9 | 2160 | `	y   = ph7_value_to_int64(apArg[2]);` |
|     9 | 2161 | `	w   = ph7_value_to_int64(apArg[3]);` |
|     9 | 2162 | `	dow = ph7_value_to_int64(apArg[4]);` |
|     9 | 2163 | `	iLocal = iTs + iOff;` |
|     9 | 2164 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|     9 | 2165 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|     9 | 2166 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|     9 | 2167 | `	monday1 = jan4 - (isoDow - 1);` |
|     9 | 2168 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|     9 | 2169 | `	ph7_result_int64(pCtx,target*86400 + iTod - iOff);` |
|     9 | 2170 | `	return PH7_OK;` |
|     5 | 2171 | `}` |
|     - | 2172 | `/* Consume nMin..nMax digits from *pz; returns count consumed (0 = failure) */` |
|   130 | 2173 | `static int DtEatDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2174 | `{` |
|   131 | 2175 | `	const char *z = *pz;` |
|   131 | 2176 | `	sxi64 v = 0;` |
|   131 | 2177 | `	int n = 0;` |
|   473 | 2178 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|   343 | 2179 | `		v = v*10 + (z[0] - '0');` |
|   343 | 2180 | `		z++;` |
|   343 | 2181 | `		n++;` |
|     1 | 2182 | `	}` |
|   131 | 2183 | `	if( n < nMin ){` |
|     3 | 2184 | `		return 0;` |
|     - | 2185 | `	}` |
|   129 | 2186 | `	*pz = z;` |
|   129 | 2187 | `	*pVal = v;` |
|   129 | 2188 | `	return n;` |
|    66 | 2189 | `}` |
|     - | 2190 | `/* timelib_get_nr's recovery: skip non-digits hunting for the field.` |
|     - | 2191 | ` * Returns 1 = found+read, 0 = digits present but short, -1 = exhausted. */` |
|     2 | 2192 | `static int DtHuntDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 2193 | `{` |
|     3 | 2194 | `	const char *z = *pz;` |
|    13 | 2195 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|     3 | 2196 | `	*pz = z;` |
|     3 | 2197 | `	if( z >= zEnd ){` |
|     3 | 2198 | `		return -1;` |
|     - | 2199 | `	}` |
|   ! 0 | 2200 | `	return DtEatDigits(pz,zEnd,nMin,nMax,pVal) ? 1 : 0;` |
|     2 | 2201 | `}` |
|     - | 2202 | `/* Case-insensitive name-table lookup; returns 1-based index or 0 */` |
|    14 | 2203 | `static int DtEatName(const char **pz,const char *zEnd,const char **azNames,int nNames)` |
|     1 | 2204 | `{` |
|     - | 2205 | `	int k;` |
|    23 | 2206 | `	for( k = 0 ; k < nNames ; k++ ){` |
|    23 | 2207 | `		int n = (int)SyStrlen(azNames[k]);` |
|    23 | 2208 | `		if( zEnd - *pz >= n && SyStrnicmp(*pz,azNames[k],(sxu32)n) == 0 ){` |
|    15 | 2209 | `			*pz += n;` |
|    15 | 2210 | `			return k + 1;` |
|     - | 2211 | `		}` |
|     5 | 2212 | `	}` |
|   ! 0 | 2213 | `	return 0;` |
|     8 | 2214 | `}` |
|     - | 2215 | `/* mixed __dt_from_format(string fmt, string input, int nowTs, int defOff)` |
|     - | 2216 | ` *   php's DateTime::createFromFormat engine. Success: [ts, off, offKind, name]` |
|     - | 2217 | ` *   where offKind 0=none-parsed, 1=numeric offset, 2=literal Z, 3=named id.` |
|     - | 2218 | ` *   Failure: "POS\tMESSAGE" (timelib's message strings; PHL reports the FIRST` |
|     - | 2219 | ` *   error where php may accumulate several — recorded). A trailing-data` |
|     - | 2220 | ` *   warning rides as [4]=pos, [5]=msg on the success array. */` |
|    44 | 2221 | `static int vm_builtin_dt_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2222 | `{` |
|     - | 2223 | `	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};` |
|     - | 2224 | `	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",` |
|     - | 2225 | `		"thursday","friday","saturday"};` |
|     - | 2226 | `	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",` |
|     - | 2227 | `		"aug","sep","oct","nov","dec"};` |
|     - | 2228 | `	static const char *azMonFull[] = {"january","february","march","april",` |
|     - | 2229 | `		"may","june","july","august","september","october","november","december"};` |
|     - | 2230 | `	const char *zFmt,*zIn,*zEnd,*zInEnd,*z;` |
|     - | 2231 | `	int nFmt,nIn;` |
|     - | 2232 | `	sxi64 iNow,v;` |
|     - | 2233 | `	sxi32 iDefOff;` |
|     - | 2234 | `	/* -1 == unset */` |
|    45 | 2235 | `	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;` |
|    45 | 2236 | `	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;` |
|    45 | 2237 | `	int iOffKind = 0;` |
|    45 | 2238 | `	sxi32 iOffVal = 0;` |
|     - | 2239 | `	char zName[16];` |
|    45 | 2240 | `	const char *zErr = 0;` |
|     - | 2241 | `	const char *aWarnMsg[3];` |
|     - | 2242 | `	int aWarnPos[3];` |
|    45 | 2243 | `	int nWarn = 0,bAborted = 0;` |
|     - | 2244 | `	const char *aErrMsg[8];` |
|     - | 2245 | `	int aErrPos[8];` |
|    45 | 2246 | `	int nErr = 0,nErrKept = 0;` |
|    45 | 2247 | `	if( nArg < 4 ){` |
|   ! 0 | 2248 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2249 | `		return PH7_OK;` |
|     - | 2250 | `	}` |
|    45 | 2251 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    45 | 2252 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    45 | 2253 | `	iNow = ph7_value_to_int64(apArg[2]);` |
|    45 | 2254 | `	iDefOff = (sxi32)ph7_value_to_int64(apArg[3]);` |
|    45 | 2255 | `	zEnd = &zFmt[nFmt];` |
|    45 | 2256 | `	zInEnd = &zIn[nIn];` |
|    45 | 2257 | `	z = zIn;` |
|    45 | 2258 | `	zName[0] = 0;` |
|     - | 2259 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|     - | 2260 | `	{ int _p = (iPos),_k,_f = -1; \` |
|     - | 2261 | `	  nErr++; \` |
|     - | 2262 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|     - | 2263 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|     - | 2264 | `	  else if( nErrKept < 8 ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|   299 | 2265 | `	while( zFmt < zEnd ){` |
|   257 | 2266 | `		char c = zFmt[0];` |
|   257 | 2267 | `		zFmt++;` |
|   257 | 2268 | `		zErr = 0;` |
|   257 | 2269 | `		if( c == '!' ){` |
|     7 | 2270 | `			y = 1970; mo = 1; d = 1; h = 0; mi = 0; s = 0;` |
|     7 | 2271 | `			h12 = -1; iMeridiem = -1;` |
|     7 | 2272 | `			continue;` |
|     - | 2273 | `		}` |
|   251 | 2274 | `		if( c == '\|' ){ bPipe = 1; continue; }` |
|   247 | 2275 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|   245 | 2276 | `		if( z >= zInEnd ){` |
|     - | 2277 | `			/* timelib aborts the scan once input is exhausted */` |
|     5 | 2278 | `			DT_FF_LOGERR(nIn,"Not enough data available to satisfy format");` |
|     3 | 2279 | `			break;` |
|     - | 2280 | `		}` |
|   243 | 2281 | `		switch( c ){` |
|    15 | 2282 | `		case 'd': case 'j':` |
|    31 | 2283 | `			if( !DtEatDigits(&z,zInEnd,1,2,&d) ){` |
|   ! 0 | 2284 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit day could not be found");` |
|   ! 0 | 2285 | `				if( DtHuntDigits(&z,zInEnd,1,2,&d) < 0 ){` |
|   ! 0 | 2286 | `					DT_FF_LOGERR(nIn,"A two digit day could not be found");` |
|   ! 0 | 2287 | `				}` |
|   ! 0 | 2288 | `			}` |
|    31 | 2289 | `			break;` |
|     1 | 2290 | `		case 'D':` |
|     3 | 2291 | `			if( !DtEatName(&z,zInEnd,azDay3,7) ){` |
|   ! 0 | 2292 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2293 | `			}` |
|     3 | 2294 | `			break;` |
|     1 | 2295 | `		case 'l':` |
|     3 | 2296 | `			if( !DtEatName(&z,zInEnd,azDayFull,7) ){` |
|   ! 0 | 2297 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 2298 | `			}` |
|     3 | 2299 | `			break;` |
|     1 | 2300 | `		case 'S':` |
|     - | 2301 | `			/* ordinal suffix: st nd rd th */` |
|     4 | 2302 | `			if( zInEnd-z >= 2 && ((z[0]=='s'&&z[1]=='t')\|\|(z[0]=='n'&&z[1]=='d')` |
|     2 | 2303 | `			 \|\|(z[0]=='r'&&z[1]=='d')\|\|(z[0]=='t'&&z[1]=='h')) ){` |
|     3 | 2304 | `				z += 2;` |
|     1 | 2305 | `			}` |
|     3 | 2306 | `			break;` |
|    13 | 2307 | `		case 'm': case 'n':` |
|    27 | 2308 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mo) ){` |
|   ! 0 | 2309 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit month could not be found");` |
|   ! 0 | 2310 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mo) < 0 ){` |
|   ! 0 | 2311 | `					DT_FF_LOGERR(nIn,"A two digit month could not be found");` |
|   ! 0 | 2312 | `				}` |
|   ! 0 | 2313 | `			}` |
|    27 | 2314 | `			break;` |
|     1 | 2315 | `		case 'M':{` |
|     3 | 2316 | `			int k = DtEatName(&z,zInEnd,azMon3,12);` |
|     3 | 2317 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2318 | `			break;` |
|     - | 2319 | `				 }` |
|     1 | 2320 | `		case 'F':{` |
|     3 | 2321 | `			int k = DtEatName(&z,zInEnd,azMonFull,12);` |
|     3 | 2322 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 2323 | `			break;` |
|     - | 2324 | `				 }` |
|   ! 0 | 2325 | `		case 'y':` |
|   ! 0 | 2326 | `			if( DtEatDigits(&z,zInEnd,2,2,&y) ){` |
|   ! 0 | 2327 | `				y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2328 | `			}else{` |
|   ! 0 | 2329 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit year could not be found");` |
|   ! 0 | 2330 | `				if( DtHuntDigits(&z,zInEnd,2,2,&y) < 0 ){` |
|   ! 0 | 2331 | `					DT_FF_LOGERR(nIn,"A two digit year could not be found");` |
|   ! 0 | 2332 | `				}else if( y >= 0 ){` |
|   ! 0 | 2333 | `					y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 2334 | `				}` |
|     - | 2335 | `			}` |
|   ! 0 | 2336 | `			break;` |
|    17 | 2337 | `		case 'Y':{` |
|    35 | 2338 | `			int neg = 0;` |
|    35 | 2339 | `			if( z < zInEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|    35 | 2340 | `			if( DtEatDigits(&z,zInEnd,1,4,&y) ){` |
|    33 | 2341 | `				if( neg ){ y = -y; }` |
|    17 | 2342 | `			}else{` |
|     3 | 2343 | `				DT_FF_LOGERR((int)(z - zIn),"A four digit year could not be found");` |
|     3 | 2344 | `				if( DtHuntDigits(&z,zInEnd,1,4,&y) < 0 ){` |
|     5 | 2345 | `					DT_FF_LOGERR(nIn,"A four digit year could not be found");` |
|     1 | 2346 | `				}` |
|     - | 2347 | `			}` |
|    35 | 2348 | `			break;` |
|     - | 2349 | `				 }` |
|     4 | 2350 | `		case 'H': case 'G':` |
|     9 | 2351 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h) ){` |
|   ! 0 | 2352 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2353 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h) < 0 ){` |
|   ! 0 | 2354 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2355 | `				}` |
|   ! 0 | 2356 | `			}` |
|     9 | 2357 | `			break;` |
|     2 | 2358 | `		case 'h': case 'g':` |
|     5 | 2359 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h12) ){` |
|   ! 0 | 2360 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 2361 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h12) < 0 ){` |
|   ! 0 | 2362 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 2363 | `				}` |
|   ! 0 | 2364 | `			}` |
|     5 | 2365 | `			break;` |
|     6 | 2366 | `		case 'i':` |
|    13 | 2367 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mi) ){` |
|   ! 0 | 2368 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit minute could not be found");` |
|   ! 0 | 2369 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mi) < 0 ){` |
|   ! 0 | 2370 | `					DT_FF_LOGERR(nIn,"A two digit minute could not be found");` |
|   ! 0 | 2371 | `				}` |
|   ! 0 | 2372 | `			}` |
|    13 | 2373 | `			break;` |
|     2 | 2374 | `		case 's':` |
|     5 | 2375 | `			if( !DtEatDigits(&z,zInEnd,1,2,&s) ){` |
|   ! 0 | 2376 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit second could not be found");` |
|   ! 0 | 2377 | `				if( DtHuntDigits(&z,zInEnd,1,2,&s) < 0 ){` |
|   ! 0 | 2378 | `					DT_FF_LOGERR(nIn,"A two digit second could not be found");` |
|   ! 0 | 2379 | `				}` |
|   ! 0 | 2380 | `			}` |
|     5 | 2381 | `			break;` |
|   ! 0 | 2382 | `		case 'u':` |
|     - | 2383 | `			/* micro parsed then dropped: PHL keeps whole seconds (recorded) */` |
|   ! 0 | 2384 | `			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){` |
|   ! 0 | 2385 | `				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");` |
|   ! 0 | 2386 | `				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){` |
|   ! 0 | 2387 | `					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");` |
|   ! 0 | 2388 | `				}` |
|   ! 0 | 2389 | `			}` |
|   ! 0 | 2390 | `			break;` |
|   ! 0 | 2391 | `		case 'v':` |
|   ! 0 | 2392 | `			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){` |
|   ! 0 | 2393 | `				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");` |
|   ! 0 | 2394 | `				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){` |
|   ! 0 | 2395 | `					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");` |
|   ! 0 | 2396 | `				}` |
|   ! 0 | 2397 | `			}` |
|   ! 0 | 2398 | `			break;` |
|     2 | 2399 | `		case 'a': case 'A':{` |
|     - | 2400 | `			static const char *azMer[] = {"am","pm","a.m.","p.m."};` |
|     5 | 2401 | `			int k = DtEatName(&z,zInEnd,azMer,4);` |
|     5 | 2402 | `			if( k ){` |
|     5 | 2403 | `				iMeridiem = ((k - 1) & 1);` |
|     3 | 2404 | `			}else{` |
|   ! 0 | 2405 | `				zErr = "A meridian could not be found";` |
|     - | 2406 | `			}` |
|     5 | 2407 | `			break;` |
|     - | 2408 | `				 }` |
|     2 | 2409 | `		case 'U':{` |
|     5 | 2410 | `			int neg = 0;` |
|     5 | 2411 | `			if( z < zInEnd && z[0]=='-' ){ neg = 1; z++; }` |
|     5 | 2412 | `			if( DtEatDigits(&z,zInEnd,1,19,&uVal) ){` |
|     5 | 2413 | `				if( neg ){ uVal = -uVal; }` |
|     5 | 2414 | `				bHasU = 1;` |
|     3 | 2415 | `			}else{` |
|   ! 0 | 2416 | `				DT_FF_LOGERR((int)(z - zIn),"A unix timestamp could not be found");` |
|   ! 0 | 2417 | `				if( DtHuntDigits(&z,zInEnd,1,19,&uVal) < 0 ){` |
|   ! 0 | 2418 | `					DT_FF_LOGERR(nIn,"A unix timestamp could not be found");` |
|   ! 0 | 2419 | `				}else{` |
|   ! 0 | 2420 | `					if( neg ){ uVal = -uVal; }` |
|   ! 0 | 2421 | `					bHasU = 1;` |
|     - | 2422 | `				}` |
|     - | 2423 | `			}` |
|     5 | 2424 | `			break;` |
|     - | 2425 | `				 }` |
|     1 | 2426 | `		case 'e': case 'T':{` |
|     - | 2427 | `			static const char *azZone[] = {"UTC","GMT","Z"};` |
|     3 | 2428 | `			int k = DtEatName(&z,zInEnd,azZone,3);` |
|     3 | 2429 | `			if( k == 3 ){` |
|   ! 0 | 2430 | `				iOffKind = 2; iOffVal = 0;` |
|     3 | 2431 | `			}else if( k ){` |
|     3 | 2432 | `				iOffKind = 3; iOffVal = 0;` |
|     3 | 2433 | `				SyMemcpy(azZone[k-1],zName,4);` |
|     1 | 2434 | `			}else if( z < zInEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|   ! 0 | 2435 | `				goto parse_num_off;` |
|   ! 0 | 2436 | `			}else{` |
|   ! 0 | 2437 | `				zErr = "The timezone could not be found in the database";` |
|     - | 2438 | `			}` |
|     3 | 2439 | `			break;` |
|     2 | 2440 | `				 }` |
|     - | 2441 | `		case 'O': case 'P':` |
|     2 | 2442 | `parse_num_off:	{` |
|     5 | 2443 | `			int sign,oh,om = 0;` |
|     - | 2444 | `			sxi64 t;` |
|     5 | 2445 | `			if( z >= zInEnd \|\| (z[0] != '+' && z[0] != '-') ){` |
|   ! 0 | 2446 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2447 | `				break;` |
|     - | 2448 | `			}` |
|     5 | 2449 | `			sign = (z[0]=='-') ? -1 : 1;` |
|     5 | 2450 | `			z++;` |
|     5 | 2451 | `			if( !DtEatDigits(&z,zInEnd,2,2,&t) ){` |
|   ! 0 | 2452 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 2453 | `				break;` |
|     - | 2454 | `			}` |
|     5 | 2455 | `			oh = (int)t;` |
|     5 | 2456 | `			if( z < zInEnd && z[0]==':' ){ z++; }` |
|     5 | 2457 | `			if( DtEatDigits(&z,zInEnd,2,2,&t) ){ om = (int)t; }` |
|     5 | 2458 | `			iOffKind = 1;` |
|     5 | 2459 | `			iOffVal = sign * (oh*3600 + om*60);` |
|     5 | 2460 | `			break;` |
|     - | 2461 | `				 }` |
|   ! 0 | 2462 | `		case '?':` |
|   ! 0 | 2463 | `			if( z < zInEnd ){ z++; }` |
|   ! 0 | 2464 | `			break;` |
|   ! 0 | 2465 | `		case '*':` |
|     - | 2466 | `			/* skip input until the next separator byte */` |
|   ! 0 | 2467 | `			while( z < zInEnd && !SyisDigit(z[0]) && z[0] != ';' && z[0] != ':'` |
|   ! 0 | 2468 | `			 && z[0] != '/' && z[0] != '.' && z[0] != ',' && z[0] != '-'` |
|   ! 0 | 2469 | `			 && z[0] != '(' && z[0] != ')' && z[0] != ' ' ){` |
|   ! 0 | 2470 | `				z++;` |
|   ! 0 | 2471 | `			}` |
|   ! 0 | 2472 | `			break;` |
|     1 | 2473 | `		case '#':` |
|     3 | 2474 | `			if( z < zInEnd && (z[0]==';'\|\|z[0]==':'\|\|z[0]=='/'\|\|z[0]=='.'` |
|   ! 0 | 2475 | `			 \|\|z[0]==','\|\|z[0]=='-'\|\|z[0]=='('\|\|z[0]==')') ){` |
|     3 | 2476 | `				z++;` |
|     2 | 2477 | `			}else{` |
|   ! 0 | 2478 | `				zErr = "The separation symbol could not be found";` |
|     - | 2479 | `			}` |
|     3 | 2480 | `			break;` |
|     1 | 2481 | `		case '\\':` |
|     3 | 2482 | `			if( zFmt < zEnd ){` |
|     3 | 2483 | `				if( z < zInEnd && z[0] == zFmt[0] ){` |
|     3 | 2484 | `					z++;` |
|     3 | 2485 | `					zFmt++;` |
|     2 | 2486 | `				}else{` |
|     - | 2487 | `					/* a literal mismatch aborts timelib's scan */` |
|   ! 0 | 2488 | `					DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|   ! 0 | 2489 | `					zFmt = zEnd;` |
|   ! 0 | 2490 | `					bAborted = 1;` |
|     - | 2491 | `				}` |
|     1 | 2492 | `			}` |
|     3 | 2493 | `			break;` |
|    34 | 2494 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|     - | 2495 | `		case '(' : case ')':` |
|    69 | 2496 | `			if( z < zInEnd && z[0] == c ){` |
|    69 | 2497 | `				z++;` |
|    35 | 2498 | `			}else{` |
|     - | 2499 | `				/* timelib logs BOTH messages (count +2, last-wins on the` |
|     - | 2500 | `				 * position), consumes the offending byte, and keeps going */` |
|   ! 0 | 2501 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 2502 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 2503 | `				z++;` |
|     - | 2504 | `			}` |
|    69 | 2505 | `			break;` |
|    13 | 2506 | `		case ' ':` |
|    27 | 2507 | `			if( z < zInEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|    27 | 2508 | `				z++;` |
|    14 | 2509 | `			}else{` |
|   ! 0 | 2510 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 2511 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 2512 | `				z++;` |
|     - | 2513 | `			}` |
|    27 | 2514 | `			break;` |
|     1 | 2515 | `		default:` |
|     - | 2516 | `			/* any other format byte must match the input verbatim; a mismatch` |
|     - | 2517 | `			 * aborts timelib's scan */` |
|     3 | 2518 | `			if( z < zInEnd && z[0] == c ){` |
|   ! 0 | 2519 | `				z++;` |
|   ! 0 | 2520 | `			}else{` |
|     3 | 2521 | `				DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|     3 | 2522 | `				zFmt = zEnd;` |
|     3 | 2523 | `				bAborted = 1;` |
|     - | 2524 | `			}` |
|     2 | 2525 | `			break;` |
|     - | 2526 | `		}` |
|   243 | 2527 | `		if( zErr ){` |
|     - | 2528 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|   ! 0 | 2529 | `			DT_FF_LOGERR((int)(z - zIn),zErr);` |
|   ! 0 | 2530 | `		}` |
|     1 | 2531 | `	}` |
|    45 | 2532 | `	if( z < zInEnd && !bAborted ){` |
|     5 | 2533 | `		if( bPlus ){` |
|     - | 2534 | `			/* '+' downgrades trailing data to a warning */` |
|     3 | 2535 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     3 | 2536 | `			aWarnMsg[nWarn] = "Trailing data";` |
|     3 | 2537 | `			nWarn++;` |
|     2 | 2538 | `		}else{` |
|     3 | 2539 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|     - | 2540 | `		}` |
|     2 | 2541 | `	}` |
|    45 | 2542 | `	if( nErr > 0 ){` |
|     - | 2543 | `		SyBlob sOut;` |
|     - | 2544 | `		int k;` |
|     7 | 2545 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     7 | 2546 | `		SyBlobFormat(&sOut,"%d",nErr);` |
|    15 | 2547 | `		for( k = 0 ; k < nErrKept ; k++ ){` |
|     9 | 2548 | `			SyBlobFormat(&sOut,"\n%d\t%s",aErrPos[k],aErrMsg[k]);` |
|     5 | 2549 | `		}` |
|     7 | 2550 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     7 | 2551 | `		SyBlobRelease(&sOut);` |
|     7 | 2552 | `		return PH7_OK;` |
|     - | 2553 | `	}` |
|    39 | 2554 | `	if( bPipe ){` |
|     5 | 2555 | `		if( y < 0 ){ y = 1970; }` |
|     5 | 2556 | `		if( mo < 0 ){ mo = 1; }` |
|     5 | 2557 | `		if( d < 0 ){ d = 1; }` |
|     5 | 2558 | `		if( h < 0 && h12 < 0 ){ h = 0; }` |
|     5 | 2559 | `		if( mi < 0 ){ mi = 0; }` |
|     5 | 2560 | `		if( s < 0 ){ s = 0; }` |
|     2 | 2561 | `	}` |
|     - | 2562 | `	{` |
|     - | 2563 | `		/* remaining unset fields come from "now" in the default offset */` |
|    39 | 2564 | `		sxi64 iLocal = iNow + iDefOff;` |
|    39 | 2565 | `		sxi64 days = DtFloorDiv(iLocal,86400);` |
|    39 | 2566 | `		sxi64 secs = iLocal - days*86400;` |
|     - | 2567 | `		sxi64 ny;` |
|     - | 2568 | `		int nmo,nd;` |
|    39 | 2569 | `		DtCivilFromDays(days,&ny,&nmo,&nd);` |
|    39 | 2570 | `		if( y < 0 ){ y = ny; }` |
|    39 | 2571 | `		if( mo < 0 ){ mo = nmo; }` |
|    39 | 2572 | `		if( d < 0 ){ d = nd; }` |
|    39 | 2573 | `		if( h12 >= 0 ){` |
|     5 | 2574 | `			h = (h12 % 12) + ((iMeridiem == 1) ? 12 : 0);` |
|     2 | 2575 | `		}` |
|     - | 2576 | `		/* php: parsing a time component zeroes the finer unset units */` |
|    39 | 2577 | `		if( h >= 0 ){` |
|    19 | 2578 | `			if( mi < 0 ){ mi = 0; }` |
|    19 | 2579 | `			if( s < 0 ){ s = 0; }` |
|    30 | 2580 | `		}else if( mi >= 0 ){` |
|     3 | 2581 | `			if( s < 0 ){ s = 0; }` |
|     1 | 2582 | `		}` |
|    39 | 2583 | `		if( h < 0 ){ h = secs / 3600; }` |
|    39 | 2584 | `		if( mi < 0 ){ mi = (secs / 60) % 60; }` |
|    39 | 2585 | `		if( s < 0 ){ s = secs % 60; }` |
|     - | 2586 | `	}` |
|     - | 2587 | `	/* php validates the RESOLVED fields and warns (parse still succeeds,` |
|     - | 2588 | `	 * values roll over via civil arithmetic) */` |
|    39 | 2589 | `	if( mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo) ){` |
|     3 | 2590 | `		if( nWarn < 3 ){` |
|     3 | 2591 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 2592 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|     3 | 2593 | `			nWarn++;` |
|     1 | 2594 | `		}` |
|     1 | 2595 | `	}` |
|    39 | 2596 | `	if( h > 24 \|\| mi > 59 \|\| s > 59 ){` |
|     3 | 2597 | `		if( nWarn < 3 ){` |
|     3 | 2598 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 2599 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|     3 | 2600 | `			nWarn++;` |
|     1 | 2601 | `		}` |
|     1 | 2602 | `	}` |
|     - | 2603 | `	{` |
|    39 | 2604 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    39 | 2605 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|     - | 2606 | `		sxi64 iTs;` |
|    39 | 2607 | `		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;` |
|    39 | 2608 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 2609 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2610 | `		}` |
|    39 | 2611 | `		if( bHasU ){` |
|     5 | 2612 | `			iTs = uVal;` |
|     5 | 2613 | `			iUseOff = 0;` |
|     5 | 2614 | `			iOffKind = 1;` |
|     3 | 2615 | `		}else{` |
|    35 | 2616 | `			iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);` |
|     - | 2617 | `		}` |
|    39 | 2618 | `		ph7_value_int64(pV,iTs);           ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2619 | `		ph7_value_int64(pV,iUseOff);       ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2620 | `		ph7_value_int64(pV,iOffKind);      ph7_array_add_elem(pArr,0,pV);` |
|    39 | 2621 | `		ph7_value_string(pV,zName,-1);     ph7_array_add_elem(pArr,0,pV);` |
|     - | 2622 | `		{` |
|     - | 2623 | `			int k;` |
|    45 | 2624 | `			for( k = 0 ; k < nWarn ; k++ ){` |
|     7 | 2625 | `				ph7_value_int64(pV,aWarnPos[k]);` |
|     7 | 2626 | `				ph7_array_add_elem(pArr,0,pV);` |
|     7 | 2627 | `				ph7_value_string(pV,aWarnMsg[k],-1);` |
|     7 | 2628 | `				ph7_array_add_elem(pArr,0,pV);` |
|     4 | 2629 | `			}` |
|     - | 2630 | `		}` |
|    39 | 2631 | `		ph7_result_value(pCtx,pArr);` |
|     - | 2632 | `	}` |
|    39 | 2633 | `	return PH7_OK;` |
|    23 | 2634 | `}` |
|     - | 2635 | `/*` |
|     - | 2636 | ` * The embedded DateTime library. Timezone scope: UTC + fixed offsets.` |
|     - | 2637 | ` */` |
|     - | 2638 | `static const char zDateTimeLib[] =` |
|     - | 2639 | `"class DateException extends Exception {}"` |
|     - | 2640 | `"class DateMalformedStringException extends DateException {}"` |
|     - | 2641 | `"class DateInvalidTimeZoneException extends DateException {}"` |
|     - | 2642 | `"class DateMalformedIntervalStringException extends DateException {}"` |
|     - | 2643 | `"class DateMalformedPeriodStringException extends DateException {}"` |
|     - | 2644 | `"interface DateTimeInterface {"` |
|     - | 2645 | `" const ATOM = 'Y-m-d\\TH:i:sP';"` |
|     - | 2646 | `" const COOKIE = 'l, d-M-Y H:i:s T';"` |
|     - | 2647 | `" const ISO8601 = 'Y-m-d\\TH:i:sO';"` |
|     - | 2648 | `" const ISO8601_EXPANDED = 'X-m-d\\TH:i:sP';"` |
|     - | 2649 | `" const RFC822 = 'D, d M y H:i:s O';"` |
|     - | 2650 | `" const RFC850 = 'l, d-M-y H:i:s T';"` |
|     - | 2651 | `" const RFC1036 = 'D, d M y H:i:s O';"` |
|     - | 2652 | `" const RFC1123 = 'D, d M Y H:i:s O';"` |
|     - | 2653 | `" const RFC7231 = 'D, d M Y H:i:s \\G\\M\\T';"` |
|     - | 2654 | `" const RFC2822 = 'D, d M Y H:i:s O';"` |
|     - | 2655 | `" const RFC3339 = 'Y-m-d\\TH:i:sP';"` |
|     - | 2656 | `" const RFC3339_EXTENDED = 'Y-m-d\\TH:i:s.vP';"` |
|     - | 2657 | `" const RSS = 'D, d M Y H:i:s O';"` |
|     - | 2658 | `" const W3C = 'Y-m-d\\TH:i:sP';"` |
|     - | 2659 | `"}"` |
|     - | 2660 | `"class DateTimeZone {"` |
|     - | 2661 | `" private $__dtzOff = 0;"` |
|     - | 2662 | `" private $__dtzName = 'UTC';"` |
|     - | 2663 | `" public function __construct($timezone = 'UTC'){"` |
|     - | 2664 | `"  $tz = (string)$timezone;"` |
|     - | 2665 | `"  if( strcasecmp($tz, 'UTC') === 0 ){"` |
|     - | 2666 | `"   $this->__dtzOff = 0; $this->__dtzName = 'UTC';"` |
|     - | 2667 | `"   return;"` |
|     - | 2668 | `"  }"` |
|     - | 2669 | `"  if( $tz === 'Z' ){"` |
|     - | 2670 | `"   $this->__dtzOff = 0; $this->__dtzName = 'Z';"` |
|     - | 2671 | `"   return;"` |
|     - | 2672 | `"  }"` |
|     - | 2673 | `"  if( strcasecmp($tz, 'GMT') === 0 ){"` |
|     - | 2674 | `"   $this->__dtzOff = 0; $this->__dtzName = 'GMT';"` |
|     - | 2675 | `"   return;"` |
|     - | 2676 | `"  }"` |
|     - | 2677 | `"  $m = null;"` |
|     - | 2678 | `"  if( preg_match('/^([+-])(\\d{2}):?(\\d{2})$/', $tz, $m) ){"` |
|     - | 2679 | `"   $off = ((int)$m[2]) * 3600 + ((int)$m[3]) * 60;"` |
|     - | 2680 | `"   if( $m[1] === '-' ){ $off = -$off; }"` |
|     - | 2681 | `"   $this->__dtzOff = $off;"` |
|     - | 2682 | `"   $this->__dtzName = $m[1] . $m[2] . ':' . $m[3];"` |
|     - | 2683 | `"   return;"` |
|     - | 2684 | `"  }"` |
|     - | 2685 | `"  throw new DateInvalidTimeZoneException("` |
|     - | 2686 | `"   'DateTimeZone::__construct(): Unknown or bad timezone (' . $tz . ')');"` |
|     - | 2687 | `" }"` |
|     - | 2688 | `" public function getName(){ return $this->__dtzName; }"` |
|     - | 2689 | `" public function getOffset($datetime = null){ return $this->__dtzOff; }"` |
|     - | 2690 | `"}"` |
|     - | 2691 | `"trait __DtCoreT {"` |
|     - | 2692 | `" private $__dtTs = 0;"` |
|     - | 2693 | `" private $__dtOff = 0;"` |
|     - | 2694 | `" private $__dtName = 'UTC';"` |
|     - | 2695 | `" private function __dtInit($datetime, $timezone){"` |
|     - | 2696 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 2697 | `"  if( $timezone !== null ){"` |
|     - | 2698 | `"   $off = $timezone->getOffset($this);"` |
|     - | 2699 | `"   $name = $timezone->getName();"` |
|     - | 2700 | `"  }"` |
|     - | 2701 | `"  $r = __dt_parse((string)$datetime, __dt_now(), $off);"` |
|     - | 2702 | `"  if( is_string($r) ){ throw new DateMalformedStringException($r); }"` |
|     - | 2703 | `"  $this->__dtTs = $r[0];"` |
|     - | 2704 | `"  if( $r[2] ){"` |
|     - | 2705 | `"   $this->__dtOff = $r[1];"` |
|     - | 2706 | `"   $this->__dtName = $r[2] === 2 ? 'Z' : $this->__dtOffName($r[1]);"` |
|     - | 2707 | `"  }else{"` |
|     - | 2708 | `"   $this->__dtOff = $off;"` |
|     - | 2709 | `"   $this->__dtName = $name;"` |
|     - | 2710 | `"  }"` |
|     - | 2711 | `" }"` |
|     - | 2712 | `" private function __dtOffName($off){"` |
|     - | 2713 | `"  $s = $off < 0 ? '-' : '+';"` |
|     - | 2714 | `"  $a = $off < 0 ? -$off : $off;"` |
|     - | 2715 | `"  return $s . sprintf('%02d:%02d', intdiv($a, 3600), intdiv($a % 3600, 60));"` |
|     - | 2716 | `" }"` |
|     - | 2717 | `" public function format($format){ return __dt_format($this->__dtTs, $this->__dtOff, $this->__dtName, (string)$format); }"` |
|     - | 2718 | `" public function getTimestamp(){ return $this->__dtTs; }"` |
|     - | 2719 | `" public function getOffset(){ return $this->__dtOff; }"` |
|     - | 2720 | `" public function getTimezone(){ return new DateTimeZone($this->__dtName); }"` |
|     - | 2721 | `" public function diff($targetObject, $absolute = false){"` |
|     - | 2722 | `"  $r = __dt_civil_diff($this->__dtTs, $this->__dtOff, $targetObject->getTimestamp());"` |
|     - | 2723 | `"  $iv = new DateInterval('P0D');"` |
|     - | 2724 | `"  $iv->y = $r[0]; $iv->m = $r[1]; $iv->d = $r[2];"` |
|     - | 2725 | `"  $iv->h = $r[3]; $iv->i = $r[4]; $iv->s = $r[5];"` |
|     - | 2726 | `"  $iv->days = $r[6];"` |
|     - | 2727 | `"  $iv->invert = $absolute ? 0 : $r[7];"` |
|     - | 2728 | `"  return $iv;"` |
|     - | 2729 | `" }"` |
|     - | 2730 | `" private function __dtAddTs($interval, $sign){"` |
|     - | 2731 | `"  if( $interval->invert ){ $sign = -$sign; }"` |
|     - | 2732 | `"  return __dt_civil_add($this->__dtTs, $this->__dtOff, $interval->y, $interval->m,"` |
|     - | 2733 | `"   $interval->d, $interval->h, $interval->i, $interval->s, $sign);"` |
|     - | 2734 | `" }"` |
|     - | 2735 | `" private static function __dtFromFormat($format, $datetime, $timezone, $class){"` |
|     - | 2736 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 2737 | `"  if( $timezone !== null ){"` |
|     - | 2738 | `"   $off = $timezone->getOffset(null);"` |
|     - | 2739 | `"   $name = $timezone->getName();"` |
|     - | 2740 | `"  }"` |
|     - | 2741 | `"  $r = __dt_from_format((string)$format, (string)$datetime, __dt_now(), $off);"` |
|     - | 2742 | `"  if( is_string($r) ){"` |
|     - | 2743 | `"   $lines = explode(\"\\n\", $r);"` |
|     - | 2744 | `"   $errs = [];"` |
|     - | 2745 | `"   $nl = count($lines);"` |
|     - | 2746 | `"   for( $k = 1; $k < $nl; $k++ ){"` |
|     - | 2747 | `"    $p = strpos($lines[$k], \"\\t\");"` |
|     - | 2748 | `"    $errs[(int)substr($lines[$k], 0, $p)] = substr($lines[$k], $p + 1);"` |
|     - | 2749 | `"   }"` |
|     - | 2750 | `"   DateTime::$__dtLastErr = ['warning_count' => 0, 'warnings' => [],"` |
|     - | 2751 | `"    'error_count' => (int)$lines[0], 'errors' => $errs];"` |
|     - | 2752 | `"   return false;"` |
|     - | 2753 | `"  }"` |
|     - | 2754 | `"  if( isset($r[4]) ){"` |
|     - | 2755 | `"   $warns = [];"` |
|     - | 2756 | `"   $wc = 0;"` |
|     - | 2757 | `"   for( $k = 4; isset($r[$k]); $k += 2 ){"` |
|     - | 2758 | `"    $warns[$r[$k]] = $r[$k + 1];"` |
|     - | 2759 | `"    $wc++;"` |
|     - | 2760 | `"   }"` |
|     - | 2761 | `"   DateTime::$__dtLastErr = ['warning_count' => $wc, 'warnings' => $warns,"` |
|     - | 2762 | `"    'error_count' => 0, 'errors' => []];"` |
|     - | 2763 | `"  }else{"` |
|     - | 2764 | `"   DateTime::$__dtLastErr = false;"` |
|     - | 2765 | `"  }"` |
|     - | 2766 | `"  $obj = new $class('@0');"` |
|     - | 2767 | `"  $obj->__dtTs = $r[0];"` |
|     - | 2768 | `"  if( $r[2] === 0 ){ $obj->__dtOff = $off; $obj->__dtName = $name; }"` |
|     - | 2769 | `"  elseif( $r[2] === 2 ){ $obj->__dtOff = 0; $obj->__dtName = 'Z'; }"` |
|     - | 2770 | `"  elseif( $r[2] === 3 ){ $obj->__dtOff = $r[1]; $obj->__dtName = $r[3]; }"` |
|     - | 2771 | `"  else { $obj->__dtOff = $r[1]; $obj->__dtName = $obj->__dtOffName($r[1]); }"` |
|     - | 2772 | `"  return $obj;"` |
|     - | 2773 | `" }"` |
|     - | 2774 | `" private static function __dtCopyOf($object, $class){"` |
|     - | 2775 | `"  $d = new $class('@0');"` |
|     - | 2776 | `"  $d->__dtTs = $object->getTimestamp();"` |
|     - | 2777 | `"  $d->__dtOff = $object->getOffset();"` |
|     - | 2778 | `"  $d->__dtName = $object->getTimezone()->getName();"` |
|     - | 2779 | `"  return $d;"` |
|     - | 2780 | `" }"` |
|     - | 2781 | `"}"` |
|     - | 2782 | `"class DateTime implements DateTimeInterface {"` |
|     - | 2783 | `" use __DtCoreT;"` |
|     - | 2784 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 2785 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 2786 | `" }"` |
|     - | 2787 | `" public function modify($modifier){"` |
|     - | 2788 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 2789 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTime::modify(): ' . $r); }"` |
|     - | 2790 | `"  $this->__dtTs = $r[0];"` |
|     - | 2791 | `"  return $this;"` |
|     - | 2792 | `" }"` |
|     - | 2793 | `" public function setTimestamp($timestamp){ $this->__dtTs = (int)$timestamp; return $this; }"` |
|     - | 2794 | `" public function setTimezone($timezone){"` |
|     - | 2795 | `"  $this->__dtOff = $timezone->getOffset($this);"` |
|     - | 2796 | `"  $this->__dtName = $timezone->getName();"` |
|     - | 2797 | `"  return $this;"` |
|     - | 2798 | `" }"` |
|     - | 2799 | `" public function setDate($year, $month, $day){"` |
|     - | 2800 | `"  $this->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 2801 | `"  return $this;"` |
|     - | 2802 | `" }"` |
|     - | 2803 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 2804 | `"  $this->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 2805 | `"  return $this;"` |
|     - | 2806 | `" }"` |
|     - | 2807 | `" public function add($interval){ $this->__dtTs = $this->__dtAddTs($interval, 1); return $this; }"` |
|     - | 2808 | `" public function sub($interval){ $this->__dtTs = $this->__dtAddTs($interval, -1); return $this; }"` |
|     - | 2809 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 2810 | `"  $this->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 2811 | `"  return $this;"` |
|     - | 2812 | `" }"` |
|     - | 2813 | `" public static $__dtLastErr = false;"` |
|     - | 2814 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 2815 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 2816 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTime');"` |
|     - | 2817 | `" }"` |
|     - | 2818 | `" public static function createFromImmutable($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 2819 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 2820 | `"}"` |
|     - | 2821 | `"class DateTimeImmutable implements DateTimeInterface {"` |
|     - | 2822 | `" use __DtCoreT;"` |
|     - | 2823 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 2824 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 2825 | `" }"` |
|     - | 2826 | `" public function modify($modifier){"` |
|     - | 2827 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 2828 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTimeImmutable::modify(): ' . $r); }"` |
|     - | 2829 | `"  $c = clone $this;"` |
|     - | 2830 | `"  $c->__dtTs = $r[0];"` |
|     - | 2831 | `"  return $c;"` |
|     - | 2832 | `" }"` |
|     - | 2833 | `" public function setTimestamp($timestamp){ $c = clone $this; $c->__dtTs = (int)$timestamp; return $c; }"` |
|     - | 2834 | `" public function setTimezone($timezone){"` |
|     - | 2835 | `"  $c = clone $this;"` |
|     - | 2836 | `"  $c->__dtOff = $timezone->getOffset($this);"` |
|     - | 2837 | `"  $c->__dtName = $timezone->getName();"` |
|     - | 2838 | `"  return $c;"` |
|     - | 2839 | `" }"` |
|     - | 2840 | `" public function setDate($year, $month, $day){"` |
|     - | 2841 | `"  $c = clone $this;"` |
|     - | 2842 | `"  $c->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 2843 | `"  return $c;"` |
|     - | 2844 | `" }"` |
|     - | 2845 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 2846 | `"  $c = clone $this;"` |
|     - | 2847 | `"  $c->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 2848 | `"  return $c;"` |
|     - | 2849 | `" }"` |
|     - | 2850 | `" public function add($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, 1); return $c; }"` |
|     - | 2851 | `" public function sub($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, -1); return $c; }"` |
|     - | 2852 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 2853 | `"  $c = clone $this;"` |
|     - | 2854 | `"  $c->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 2855 | `"  return $c;"` |
|     - | 2856 | `" }"` |
|     - | 2857 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 2858 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 2859 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTimeImmutable');"` |
|     - | 2860 | `" }"` |
|     - | 2861 | `" public static function createFromMutable($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 2862 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 2863 | `"}"` |
|     - | 2864 | `"function date_create($datetime = 'now', $timezone = null){"` |
|     - | 2865 | `" try { return new DateTime($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 2866 | `"}"` |
|     - | 2867 | `"function date_create_immutable($datetime = 'now', $timezone = null){"` |
|     - | 2868 | `" try { return new DateTimeImmutable($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 2869 | `"}"` |
|     - | 2870 | `"class DateInterval {"` |
|     - | 2871 | `" public $y = 0;"` |
|     - | 2872 | `" public $m = 0;"` |
|     - | 2873 | `" public $d = 0;"` |
|     - | 2874 | `" public $h = 0;"` |
|     - | 2875 | `" public $i = 0;"` |
|     - | 2876 | `" public $s = 0;"` |
|     - | 2877 | `" public $f = 0;"` |
|     - | 2878 | `" public $invert = 0;"` |
|     - | 2879 | `" public $days = false;"` |
|     - | 2880 | `" public $from_string = false;"` |
|     - | 2881 | `" public function __construct($duration = 'P0D'){"` |
|     - | 2882 | `"  $dur = (string)$duration;"` |
|     - | 2883 | `"  $mm = null;"` |
|     - | 2884 | `"  if( strlen($dur) < 2 \|\| substr($dur, -1) === 'T'"` |
|     - | 2885 | `"   \|\| !preg_match('/^P(?:(\\d+)Y)?(?:(\\d+)M)?(?:(\\d+)W)?(?:(\\d+)D)?(?:T(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?)?$/', $dur, $mm) ){"` |
|     - | 2886 | `"   throw new DateMalformedIntervalStringException('Unknown or bad format (' . $dur . ')');"` |
|     - | 2887 | `"  }"` |
|     - | 2888 | `"  $this->y = (int)($mm[1] ?? 0);"` |
|     - | 2889 | `"  $this->m = (int)($mm[2] ?? 0);"` |
|     - | 2890 | `"  $this->d = (int)($mm[4] ?? 0) + 7 * (int)($mm[3] ?? 0);"` |
|     - | 2891 | `"  $this->h = (int)($mm[5] ?? 0);"` |
|     - | 2892 | `"  $this->i = (int)($mm[6] ?? 0);"` |
|     - | 2893 | `"  $this->s = (int)($mm[7] ?? 0);"` |
|     - | 2894 | `" }"` |
|     - | 2895 | `" public static function createFromDateString($datetime){"` |
|     - | 2896 | `"  $s = trim((string)$datetime);"` |
|     - | 2897 | `"  $iv = new DateInterval('P0D');"` |
|     - | 2898 | `"  $rest = $s;"` |
|     - | 2899 | `"  $any = false;"` |
|     - | 2900 | `"  while( $rest !== '' ){"` |
|     - | 2901 | `"   $mm = null;"` |
|     - | 2902 | `"   if( !preg_match('/^[\\s,+]*([+-]?\\d+)\\s*(sec\|secs\|second\|seconds\|min\|mins\|minute\|minutes\|hour\|hours\|day\|days\|week\|weeks\|fortnight\|fortnights\|month\|months\|year\|years)\\b/i', $rest, $mm) ){"` |
|     - | 2903 | `"    throw new DateMalformedIntervalStringException("` |
|     - | 2904 | `"     'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 2905 | `"   }"` |
|     - | 2906 | `"   $n = (int)$mm[1];"` |
|     - | 2907 | `"   $u = strtolower($mm[2]);"` |
|     - | 2908 | `"   if( $u === 'sec' \|\| $u === 'secs' \|\| $u === 'second' \|\| $u === 'seconds' ){ $iv->s += $n; }"` |
|     - | 2909 | `"   elseif( $u === 'min' \|\| $u === 'mins' \|\| $u === 'minute' \|\| $u === 'minutes' ){ $iv->i += $n; }"` |
|     - | 2910 | `"   elseif( $u === 'hour' \|\| $u === 'hours' ){ $iv->h += $n; }"` |
|     - | 2911 | `"   elseif( $u === 'day' \|\| $u === 'days' ){ $iv->d += $n; }"` |
|     - | 2912 | `"   elseif( $u === 'week' \|\| $u === 'weeks' ){ $iv->d += 7 * $n; }"` |
|     - | 2913 | `"   elseif( $u === 'fortnight' \|\| $u === 'fortnights' ){ $iv->d += 14 * $n; }"` |
|     - | 2914 | `"   elseif( $u === 'month' \|\| $u === 'months' ){ $iv->m += $n; }"` |
|     - | 2915 | `"   else { $iv->y += $n; }"` |
|     - | 2916 | `"   $any = true;"` |
|     - | 2917 | `"   $rest = ltrim(substr($rest, strlen($mm[0])));"` |
|     - | 2918 | `"  }"` |
|     - | 2919 | `"  if( !$any ){"` |
|     - | 2920 | `"   throw new DateMalformedIntervalStringException("` |
|     - | 2921 | `"    'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 2922 | `"  }"` |
|     - | 2923 | `"  return $iv;"` |
|     - | 2924 | `" }"` |
|     - | 2925 | `" public function format($format){"` |
|     - | 2926 | `"  $f = (string)$format;"` |
|     - | 2927 | `"  $out = '';"` |
|     - | 2928 | `"  $n = strlen($f);"` |
|     - | 2929 | `"  for( $k = 0; $k < $n; $k++ ){"` |
|     - | 2930 | `"   $c = $f[$k];"` |
|     - | 2931 | `"   if( $c !== '%' ){ $out .= $c; continue; }"` |
|     - | 2932 | `"   $k++;"` |
|     - | 2933 | `"   if( $k >= $n ){ $out .= '%'; break; }"` |
|     - | 2934 | `"   $t = $f[$k];"` |
|     - | 2935 | `"   if( $t === 'Y' ){ $out .= sprintf('%02d', $this->y); }"` |
|     - | 2936 | `"   elseif( $t === 'y' ){ $out .= $this->y; }"` |
|     - | 2937 | `"   elseif( $t === 'M' ){ $out .= sprintf('%02d', $this->m); }"` |
|     - | 2938 | `"   elseif( $t === 'm' ){ $out .= $this->m; }"` |
|     - | 2939 | `"   elseif( $t === 'D' ){ $out .= sprintf('%02d', $this->d); }"` |
|     - | 2940 | `"   elseif( $t === 'd' ){ $out .= $this->d; }"` |
|     - | 2941 | `"   elseif( $t === 'H' ){ $out .= sprintf('%02d', $this->h); }"` |
|     - | 2942 | `"   elseif( $t === 'h' ){ $out .= $this->h; }"` |
|     - | 2943 | `"   elseif( $t === 'I' ){ $out .= sprintf('%02d', $this->i); }"` |
|     - | 2944 | `"   elseif( $t === 'i' ){ $out .= $this->i; }"` |
|     - | 2945 | `"   elseif( $t === 'S' ){ $out .= sprintf('%02d', $this->s); }"` |
|     - | 2946 | `"   elseif( $t === 's' ){ $out .= $this->s; }"` |
|     - | 2947 | `"   elseif( $t === 'F' ){ $out .= sprintf('%06d', (int)round($this->f * 1000000)); }"` |
|     - | 2948 | `"   elseif( $t === 'f' ){ $out .= (int)round($this->f * 1000000); }"` |
|     - | 2949 | `"   elseif( $t === 'R' ){ $out .= $this->invert ? '-' : '+'; }"` |
|     - | 2950 | `"   elseif( $t === 'r' ){ $out .= $this->invert ? '-' : ''; }"` |
|     - | 2951 | `"   elseif( $t === 'a' ){ $out .= $this->days === false ? '(unknown)' : $this->days; }"` |
|     - | 2952 | `"   elseif( $t === '%' ){ $out .= '%'; }"` |
|     - | 2953 | `"   else { $out .= $t; }"` |
|     - | 2954 | `"  }"` |
|     - | 2955 | `"  return $out;"` |
|     - | 2956 | `" }"` |
|     - | 2957 | `"}"` |
|     - | 2958 | `"class DatePeriod implements IteratorAggregate {"` |
|     - | 2959 | `" const EXCLUDE_START_DATE = 1;"` |
|     - | 2960 | `" const INCLUDE_END_DATE = 2;"` |
|     - | 2961 | `" public $start = null;"` |
|     - | 2962 | `" public $current = null;"` |
|     - | 2963 | `" public $end = null;"` |
|     - | 2964 | `" public $interval = null;"` |
|     - | 2965 | `" public $recurrences = 1;"` |
|     - | 2966 | `" public $include_start_date = true;"` |
|     - | 2967 | `" public $include_end_date = false;"` |
|     - | 2968 | `" private $__dpN = null;"` |
|     - | 2969 | `" public function __construct($start, $interval = null, $end = null, $options = 0){"` |
|     - | 2970 | `"  if( is_string($start) ){"` |
|     - | 2971 | `"   $mm = null;"` |
|     - | 2972 | `"   if( !preg_match('/^R(\\d+)\\/(.+)\\/(P.+)$/', $start, $mm) ){"` |
|     - | 2973 | `"    throw new DateMalformedPeriodStringException("` |
|     - | 2974 | `"     'DatePeriod::__construct(): Unknown or bad format (' . $start . ')');"` |
|     - | 2975 | `"   }"` |
|     - | 2976 | `"   $options = is_int($interval) ? $interval : 0;"` |
|     - | 2977 | `"   $this->start = new DateTimeImmutable($mm[2]);"` |
|     - | 2978 | `"   $this->interval = new DateInterval($mm[3]);"` |
|     - | 2979 | `"   $this->__dpN = (int)$mm[1];"` |
|     - | 2980 | `"   $this->recurrences = $this->__dpN + 1;"` |
|     - | 2981 | `"  }else{"` |
|     - | 2982 | `"   $this->start = clone $start;"` |
|     - | 2983 | `"   $this->interval = $interval;"` |
|     - | 2984 | `"   if( is_int($end) ){"` |
|     - | 2985 | `"    $this->__dpN = $end;"` |
|     - | 2986 | `"    $this->recurrences = $end + 1;"` |
|     - | 2987 | `"   }else{"` |
|     - | 2988 | `"    $this->end = $end === null ? null : (clone $end);"` |
|     - | 2989 | `"   }"` |
|     - | 2990 | `"  }"` |
|     - | 2991 | `"  $this->include_start_date = !((int)$options & 1);"` |
|     - | 2992 | `"  $this->include_end_date = ((int)$options & 2) !== 0;"` |
|     - | 2993 | `" }"` |
|     - | 2994 | `" public static function createFromISO8601String($specification, $options = 0){"` |
|     - | 2995 | `"  return new DatePeriod((string)$specification, (int)$options);"` |
|     - | 2996 | `" }"` |
|     - | 2997 | `" public function getStartDate(){ return $this->start; }"` |
|     - | 2998 | `" public function getEndDate(){ return $this->end; }"` |
|     - | 2999 | `" public function getDateInterval(){ return $this->interval; }"` |
|     - | 3000 | `" public function getRecurrences(){ return $this->__dpN; }"` |
|     - | 3001 | `" public function getIterator(): Generator {"` |
|     - | 3002 | `"  $cur = $this->start;"` |
|     - | 3003 | `"  $iv = $this->interval;"` |
|     - | 3004 | `"  $k = 0;"` |
|     - | 3005 | `"  if( $this->end !== null ){"` |
|     - | 3006 | `"   $endTs = $this->end->getTimestamp();"` |
|     - | 3007 | `"   $first = true;"` |
|     - | 3008 | `"   while( true ){"` |
|     - | 3009 | `"    $ts = $cur->getTimestamp();"` |
|     - | 3010 | `"    if( $this->include_end_date ? ($ts > $endTs) : ($ts >= $endTs) ){ break; }"` |
|     - | 3011 | `"    if( !$first \|\| $this->include_start_date ){"` |
|     - | 3012 | `"     yield $k => (clone $cur);"` |
|     - | 3013 | `"     $k++;"` |
|     - | 3014 | `"    }"` |
|     - | 3015 | `"    $first = false;"` |
|     - | 3016 | `"    $next = clone $cur;"` |
|     - | 3017 | `"    $cur = $next->add($iv);"` |
|     - | 3018 | `"   }"` |
|     - | 3019 | `"   return;"` |
|     - | 3020 | `"  }"` |
|     - | 3021 | `"  $total = $this->__dpN + 1 + ($this->include_end_date ? 1 : 0);"` |
|     - | 3022 | `"  for( $j = 0; $j < $total; $j++ ){"` |
|     - | 3023 | `"   if( $j > 0 \|\| $this->include_start_date ){"` |
|     - | 3024 | `"    yield $k => (clone $cur);"` |
|     - | 3025 | `"    $k++;"` |
|     - | 3026 | `"   }"` |
|     - | 3027 | `"   $next = clone $cur;"` |
|     - | 3028 | `"   $cur = $next->add($iv);"` |
|     - | 3029 | `"  }"` |
|     - | 3030 | `" }"` |
|     - | 3031 | `"}"` |
|     - | 3032 | `"function date_format($object, $format){ return $object->format($format); }"` |
|     - | 3033 | `"function date_modify($object, $modifier){"` |
|     - | 3034 | `" try { return $object->modify($modifier); } catch (Exception $e) { return false; }"` |
|     - | 3035 | `"}"` |
|     - | 3036 | `"function date_add($object, $interval){ return $object->add($interval); }"` |
|     - | 3037 | `"function date_sub($object, $interval){ return $object->sub($interval); }"` |
|     - | 3038 | `"function date_diff($baseObject, $targetObject, $absolute = false){"` |
|     - | 3039 | `" return $baseObject->diff($targetObject, $absolute);"` |
|     - | 3040 | `"}"` |
|     - | 3041 | `"function date_timestamp_get($object){ return $object->getTimestamp(); }"` |
|     - | 3042 | `"function date_timestamp_set($object, $timestamp){ return $object->setTimestamp($timestamp); }"` |
|     - | 3043 | `"function date_timezone_get($object){ return $object->getTimezone(); }"` |
|     - | 3044 | `"function date_timezone_set($object, $timezone){ return $object->setTimezone($timezone); }"` |
|     - | 3045 | `"function date_offset_get($object){ return $object->getOffset(); }"` |
|     - | 3046 | `"function date_date_set($object, $year, $month, $day){ return $object->setDate($year, $month, $day); }"` |
|     - | 3047 | `"function date_time_set($object, $hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 3048 | `" return $object->setTime($hour, $minute, $second, $microsecond);"` |
|     - | 3049 | `"}"` |
|     - | 3050 | `"function date_isodate_set($object, $year, $week, $dayOfWeek = 1){"` |
|     - | 3051 | `" return $object->setISODate($year, $week, $dayOfWeek);"` |
|     - | 3052 | `"}"` |
|     - | 3053 | `"function date_interval_create_from_date_string($datetime){"` |
|     - | 3054 | `" return DateInterval::createFromDateString($datetime);"` |
|     - | 3055 | `"}"` |
|     - | 3056 | `"function date_interval_format($object, $format){ return $object->format($format); }"` |
|     - | 3057 | `"function date_get_last_errors(){ return DateTime::getLastErrors(); }"` |
|     - | 3058 | `"function timezone_open($timezone){"` |
|     - | 3059 | `" try { return new DateTimeZone($timezone); } catch (Exception $e) { return false; }"` |
|     - | 3060 | `"}"` |
|     - | 3061 | `"function timezone_name_get($object){ return $object->getName(); }"` |
|     - | 3062 | `"function timezone_offset_get($object, $datetime){ return $object->getOffset($datetime); }"` |
|     - | 3063 | `;` |
|     - | 3064 | `/*` |
|     - | 3065 | ` * Install the DateTime family: thunks first, then the chunk. Called from` |
|     - | 3066 | ` * PH7_VmInit inside the bCompilingBuiltin window, after the Reflection` |
|     - | 3067 | ` * install (Exception must exist).` |
|     - | 3068 | ` */` |
|  3840 | 3069 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
|     5 | 3070 | `{` |
|     - | 3071 | `	static const struct {` |
|     - | 3072 | `		const char *zName;` |
|     - | 3073 | `		ProchHostFunction xFunc;` |
|     - | 3074 | `	} aFunc[] = {` |
|     - | 3075 | `		{ "__dt_now",    vm_builtin_dt_now },` |
|     - | 3076 | `		{ "__dt_default_tz", vm_builtin_dt_default_tz },` |
|     - | 3077 | `		{ "__dt_civil_add",  vm_builtin_dt_civil_add },` |
|     - | 3078 | `		{ "__dt_civil_diff", vm_builtin_dt_civil_diff },` |
|     - | 3079 | `		{ "__dt_isodate",    vm_builtin_dt_isodate },` |
|     - | 3080 | `		{ "__dt_from_format", vm_builtin_dt_from_format },` |
|     - | 3081 | `		{ "__dt_parse",  vm_builtin_dt_parse },` |
|     - | 3082 | `		{ "__dt_format", vm_builtin_dt_format },` |
|     - | 3083 | `		{ "__dt_make",   vm_builtin_dt_make },` |
|     - | 3084 | `	};` |
|     - | 3085 | `	sxu32 n;` |
|     - | 3086 | `	/* php's date.timezone default */` |
|  3845 | 3087 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|  3845 | 3088 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
| 38405 | 3089 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 34565 | 3090 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 17285 | 3091 | `	}` |
|  3845 | 3092 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zDateTimeLib,sizeof(zDateTimeLib)-1);` |
|     5 | 3093 | `}` |
|     - | 3094 |  |
|     - | 3095 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 3096 |  |
|     - | 3097 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 3098 | `/* Tiny build: no DateTime family (builtin layer disabled) */` |
|     - | 3099 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){` |
|     - | 3100 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|     - | 3101 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|     - | 3102 | `	return SXRET_OK;` |
|     - | 3103 | `}` |
|     - | 3104 | `#endif` |
|     - | 3105 |  |
