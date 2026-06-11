# src/ph7/builtin_date.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 456/690 lines (66.09%)

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
|    - |   35 |  |
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
|    - |   54 |  |
|    - |   55 | `/* SPDX-SnippetEnd */` |
|    - |   56 | `#endif /*_WIN32_WCE */` |
|    - |   57 | `#elif defined(__UNIXES__)` |
|    - |   58 | `#include <sys/time.h>` |
|    - |   59 | `#endif /* __WINNT__*/` |
|    - |   60 | ` /*` |
|    - |   61 | `  * int64 time(void)` |
|    - |   62 | `  *  Current Unix timestamp` |
|    - |   63 | `  * Parameters` |
|    - |   64 | `  *  None.` |
|    - |   65 | `  * Return` |
|    - |   66 | `  *  Returns the current time measured in the number of seconds` |
|    - |   67 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|    - |   68 | `  */` |
|    8 |   69 | `PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   70 |  |
|    - |   71 | `	time_t tt;` |
|    4 |   72 | `	SXUNUSED(nArg); /* cc warning */` |
|    4 |   73 | `	SXUNUSED(apArg);` |
|    - |   74 | `	/* Extract the current time */` |
|    9 |   75 | `	time(&tt);` |
|    - |   76 | `	/* Return as 64-bit integer */` |
|    9 |   77 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|    9 |   78 | `	return  PH7_OK;` |
|    1 |   79 |  |
|    - |   80 | `/*` |
|    - |   81 | `  * string/float microtime([ bool $get_as_float = false ])` |
|    - |   82 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|    - |   83 | `  * Parameters` |
|    - |   84 | `  *  $get_as_float` |
|    - |   85 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|    - |   86 | `  *   as described in the return values section below.` |
|    - |   87 | `  * Return` |
|    - |   88 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|    - |   89 | `  *  is the current time measured in the number of seconds since the Unix` |
|    - |   90 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|    - |   91 | `  *  that have elapsed since sec expressed in seconds.` |
|    - |   92 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|    - |   93 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|    - |   94 | `  */` |
|   20 |   95 | `PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   96 |  |
|   21 |   97 | `	int bFloat = 0;` |
|    - |   98 | `	sytime sTime;` |
|    - |   99 | `#if defined(__UNIXES__)` |
|    - |  100 | `	struct timeval tv;` |
|   20 |  101 | `	gettimeofday(&tv,0);` |
|   20 |  102 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|   20 |  103 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|    - |  104 | `#else` |
|    - |  105 | `	time_t tt;` |
|    1 |  106 | `	time(&tt);` |
|    1 |  107 | `	sTime.tm_sec  = (long)tt;` |
|    1 |  108 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|    - |  109 | `#endif /* __UNIXES__ */` |
|   21 |  110 | `	if( nArg > 0 ){` |
|   17 |  111 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|    8 |  112 | `	}` |
|   21 |  113 | `	if( bFloat ){` |
|    - |  114 | `		/* Return as float */` |
|   17 |  115 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|    9 |  116 | `	}else{` |
|    - |  117 | `		/* Return as string */` |
|    5 |  118 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|    - |  119 | `	}` |
|   21 |  120 | `	return PH7_OK;` |
|    1 |  121 |  |
|    - |  122 | `/*` |
|    - |  123 | ` * array getdate ([ int $timestamp = time() ])` |
|    - |  124 | ` *  Returns an associative array containing the date information` |
|    - |  125 | ` *  of the timestamp, or the current local time if no timestamp is given.` |
|    - |  126 | ` * Parameter` |
|    - |  127 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|    - |  128 | ` *     that defaults to the current local time if a timestamp is not given.` |
|    - |  129 | ` *     In other words, it defaults to the value of time().` |
|    - |  130 | ` * Returns` |
|    - |  131 | ` *  Returns an associative array of information related to the timestamp.` |
|    - |  132 | ` */` |
|    8 |  133 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  134 |  |
|    - |  135 | `	ph7_value *pValue,*pArray;` |
|    - |  136 | `	Sytm sTm;` |
|    9 |  137 | `	if( nArg < 1 ){` |
|    - |  138 | `#ifdef __WINNT__` |
|    - |  139 | `		SYSTEMTIME sOS;` |
|    1 |  140 | `		GetSystemTime(&sOS);` |
|    1 |  141 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  142 | `#else` |
|    - |  143 | `		struct tm *pTm;` |
|    - |  144 | `		time_t t;` |
|    4 |  145 | `		time(&t);` |
|    4 |  146 | `		pTm = localtime(&t);` |
|    4 |  147 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  148 | `#endif` |
|    3 |  149 | `	}else{` |
|    - |  150 | `		/* Use the given timestamp */` |
|    - |  151 | `		time_t t;` |
|    - |  152 | `		struct tm *pTm;` |
|    5 |  153 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    5 |  154 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|    5 |  155 | `			pTm = localtime(&t);` |
|    5 |  156 | `			if( pTm == 0 ){` |
|  ! 0 |  157 | `				time(&t);` |
|  ! 0 |  158 | `			}` |
|    3 |  159 | `		}else{` |
|  ! 0 |  160 | `			time(&t);` |
|    - |  161 | `		}` |
|    5 |  162 | `		pTm = localtime(&t);` |
|    5 |  163 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  164 | `	}` |
|    - |  165 | `	/* Element value */` |
|    9 |  166 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    9 |  167 | `	if( pValue == 0 ){` |
|    - |  168 | `		/* Return NULL */` |
|  ! 0 |  169 | `		ph7_result_null(pCtx);` |
|  ! 0 |  170 | `		return PH7_OK;` |
|    - |  171 | `	}` |
|    - |  172 | `	/* Create a new array */` |
|    9 |  173 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 |  174 | `	if( pArray == 0 ){` |
|    - |  175 | `		/* Return NULL */` |
|  ! 0 |  176 | `		ph7_result_null(pCtx);` |
|  ! 0 |  177 | `		return PH7_OK;` |
|    - |  178 | `	}` |
|    - |  179 | `	/* Fill the array */` |
|    - |  180 | `	/* Seconds */` |
|    9 |  181 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    9 |  182 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|    - |  183 | `	/* Minutes */` |
|    9 |  184 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    9 |  185 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|    - |  186 | `	/* Hours */` |
|    9 |  187 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    9 |  188 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|    - |  189 | `	/* mday */` |
|    9 |  190 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    9 |  191 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|    - |  192 | `	/* wday */` |
|    9 |  193 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    9 |  194 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|    - |  195 | `	/* mon */` |
|    9 |  196 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|    9 |  197 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|    - |  198 | `	/* year */` |
|    9 |  199 | `	ph7_value_int(pValue,sTm.tm_year);` |
|    9 |  200 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|    - |  201 | `	/* yday */` |
|    9 |  202 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    9 |  203 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|    - |  204 | `	/* Weekday [i.e: Monday,Tuesday,...] */` |
|    9 |  205 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|    9 |  206 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|    - |  207 | `	/* Reset the string cursor */` |
|    9 |  208 | `	ph7_value_reset_string_cursor(pValue);` |
|    - |  209 | `	/* Month [i.e: January,February,...] */` |
|    9 |  210 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|    9 |  211 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|    - |  212 | `	/* Return the freshly created array */` |
|    9 |  213 | `	ph7_result_value(pCtx,pArray);` |
|    9 |  214 | `	return PH7_OK;` |
|    5 |  215 |  |
|    - |  216 | `/*` |
|    - |  217 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|    - |  218 | ` *  Returns an associative array containing the data returned from the system call.` |
|    - |  219 | ` * Parameters` |
|    - |  220 | ` *  $return_float` |
|    - |  221 | ` *   When set to TRUE, a float instead of an array is returned.` |
|    - |  222 | ` * Return` |
|    - |  223 | ` *  By default an array is returned. If return_float is set, then` |
|    - |  224 | ` *  a float is returned.` |
|    - |  225 | ` */` |
|    4 |  226 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  227 |  |
|    5 |  228 | `	int bFloat = 0;` |
|    - |  229 | `	sytime sTime;` |
|    - |  230 | `#if defined(__UNIXES__)` |
|    - |  231 | `	struct timeval tv;` |
|    4 |  232 | `	gettimeofday(&tv,0);` |
|    4 |  233 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|    4 |  234 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|    - |  235 | `#else` |
|    - |  236 | `	time_t tt;` |
|    1 |  237 | `	time(&tt);` |
|    1 |  238 | `	sTime.tm_sec  = (long)tt;` |
|    1 |  239 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|    - |  240 | `#endif /* __UNIXES__ */` |
|    5 |  241 | `	if( nArg > 0 ){` |
|    5 |  242 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|    2 |  243 | `	}` |
|    5 |  244 | `	if( bFloat ){` |
|    - |  245 | `		/* Return as float */` |
|    3 |  246 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|    2 |  247 | `	}else{` |
|    - |  248 | `		/* Return an associative array */` |
|    - |  249 | `		ph7_value *pValue,*pArray;` |
|    - |  250 | `		/* Create a new array */` |
|    3 |  251 | `		pArray = ph7_context_new_array(pCtx);` |
|    - |  252 | `		/* Element value */` |
|    3 |  253 | `		pValue = ph7_context_new_scalar(pCtx);` |
|    3 |  254 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    - |  255 | `			/* Return NULL */` |
|  ! 0 |  256 | `			ph7_result_null(pCtx);` |
|  ! 0 |  257 | `			return PH7_OK;` |
|    - |  258 | `		}` |
|    - |  259 | `		/* Fill the array */` |
|    - |  260 | `		/* sec */` |
|    3 |  261 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|    3 |  262 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|    - |  263 | `		/* usec */` |
|    3 |  264 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|    3 |  265 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|    - |  266 | `		/* Return the array */` |
|    3 |  267 | `		ph7_result_value(pCtx,pArray);` |
|    - |  268 | `	}` |
|    5 |  269 | `	return PH7_OK;` |
|    3 |  270 |  |
|    - |  271 | `/* Check if the given year is leap or not */` |
|    - |  272 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|    - |  273 | `/* ISO-8601 numeric representation of the day of the week */` |
|    - |  274 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|    - |  275 | `/*` |
|    - |  276 | ` * Format a given date string.` |
|    - |  277 | ` * Supported format: (Taken from PHP online docs)` |
|    - |  278 | ` * character 	Description` |
|    - |  279 | ` * d          Day of the month, 2 digits with leading zeros` |
|    - |  280 | ` * D          A textual representation of a day, three letters` |
|    - |  281 | ` * j          Day of the month without leading zeros` |
|    - |  282 | ` * l          A full textual representation of the day of the week` |
|    - |  283 | ` * N          ISO-8601 numeric representation of the day of the week` |
|    - |  284 | ` * w          Numeric representation of the day of the week` |
|    - |  285 | ` * z          The day of the year (starting from 0)` |
|    - |  286 | ` * F          A full textual representation of a month, such as January or March` |
|    - |  287 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|    - |  288 | ` * M          A short textual representation of a month, three letters` |
|    - |  289 | ` * n          Numeric representation of a month, without leading zeros` |
|    - |  290 | ` * t          Number of days in the given month` |
|    - |  291 | ` * L          Whether it's a leap year` |
|    - |  292 | ` * o          ISO-8601 year number. This has the same value as Y` |
|    - |  293 | ` * Y          A full numeric representation of a year, 4 digits` |
|    - |  294 | ` * y          A two digit representation of a year` |
|    - |  295 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|    - |  296 | ` * A          Uppercase Ante meridiem and Post meridiem` |
|    - |  297 | ` * g          12-hour format of an hour without leading zeros` |
|    - |  298 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|    - |  299 | ` * h          12-hour format of an hour with leading zeros` |
|    - |  300 | ` * H          24-hour format of an hour with leading zeros` |
|    - |  301 | ` * i          Minutes with leading zeros` |
|    - |  302 | ` * s          Seconds, with leading zeros` |
|    - |  303 | ` * u          Microseconds` |
|    - |  304 | ` * e          Timezone identifier` |
|    - |  305 | ` * I          Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|    - |  306 | ` * r          RFC 2822 formatted date` |
|    - |  307 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|    - |  308 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|    - |  309 | ` * O          Difference to Greenwich time (GMT) in hours` |
|    - |  310 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|    - |  311 | ` *            east of UTC is always positive.` |
|    - |  312 | ` * c         ISO 8601 date` |
|    - |  313 | ` */` |
|   46 |  314 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|    1 |  315 |  |
|   47 |  316 | `	const char *zEnd = &zIn[nLen];` |
|    - |  317 | `	const char *zCur;` |
|    - |  318 | `	/* Start the format process */` |
|   78 |  319 | `	for(;;){` |
|  157 |  320 | `		if( zIn >= zEnd ){` |
|    - |  321 | `			/* No more input to process */` |
|   47 |  322 | `			break;` |
|    - |  323 | `		}` |
|  111 |  324 | `		switch(zIn[0]){` |
|    7 |  325 | `		case 'd':` |
|    - |  326 | `			/* Day of the month, 2 digits with leading zeros */` |
|   15 |  327 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|   15 |  328 | `			break;` |
|  ! 0 |  329 | `		case 'D':` |
|    - |  330 | `			/*A textual representation of a day, three letters*/` |
|  ! 0 |  331 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|  ! 0 |  332 | `			ph7_result_string(pCtx,zCur,3);` |
|  ! 0 |  333 | `			break;` |
|  ! 0 |  334 | `		case 'j':` |
|    - |  335 | `			/*	Day of the month without leading zeros */` |
|  ! 0 |  336 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|  ! 0 |  337 | `			break;` |
|    2 |  338 | `		case 'l':` |
|    - |  339 | `			/* A full textual representation of the day of the week */` |
|    5 |  340 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    5 |  341 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|    5 |  342 | `			break;` |
|  ! 0 |  343 | `		case 'N':{` |
|    - |  344 | `			/* ISO-8601 numeric representation of the day of the week */` |
|  ! 0 |  345 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|  ! 0 |  346 | `			break;` |
|    - |  347 | `				 }` |
|  ! 0 |  348 | `		case 'w':` |
|    - |  349 | `			/*Numeric representation of the day of the week*/` |
|  ! 0 |  350 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|  ! 0 |  351 | `			break;` |
|  ! 0 |  352 | `		case 'z':` |
|    - |  353 | `			/*The day of the year*/` |
|  ! 0 |  354 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|  ! 0 |  355 | `			break;` |
|    2 |  356 | `		case 'F':` |
|    - |  357 | `			/*A full textual representation of a month, such as January or March*/` |
|    5 |  358 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    5 |  359 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|    5 |  360 | `			break;` |
|    7 |  361 | `		case 'm':` |
|    - |  362 | `			/*Numeric representation of a month, with leading zeros*/` |
|   15 |  363 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|   15 |  364 | `			break;` |
|  ! 0 |  365 | `		case 'M':` |
|    - |  366 | `			/*A short textual representation of a month, three letters*/` |
|  ! 0 |  367 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|  ! 0 |  368 | `			ph7_result_string(pCtx,zCur,3);` |
|  ! 0 |  369 | `			break;` |
|  ! 0 |  370 | `		case 'n':` |
|    - |  371 | `			/*Numeric representation of a month, without leading zeros*/` |
|  ! 0 |  372 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|  ! 0 |  373 | `			break;` |
|  ! 0 |  374 | `		case 't':{` |
|    - |  375 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|  ! 0 |  376 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|  ! 0 |  377 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|  ! 0 |  378 | `				nDays = 28;` |
|  ! 0 |  379 | `			}` |
|    - |  380 | `			/*Number of days in the given month*/` |
|  ! 0 |  381 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|  ! 0 |  382 | `			break;` |
|    - |  383 | `				 }` |
|  ! 0 |  384 | `		case 'L':{` |
|  ! 0 |  385 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|    - |  386 | `			/* Whether it's a leap year */` |
|  ! 0 |  387 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|  ! 0 |  388 | `			break;` |
|    - |  389 | `				 }` |
|  ! 0 |  390 | `		case 'o':` |
|    - |  391 | `			/* ISO-8601 year number.*/` |
|  ! 0 |  392 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|  ! 0 |  393 | `			break;` |
|    9 |  394 | `		case 'Y':` |
|    - |  395 | `			/*	A full numeric representation of a year, 4 digits */` |
|   19 |  396 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|   19 |  397 | `			break;` |
|  ! 0 |  398 | `		case 'y':` |
|    - |  399 | `			/*A two digit representation of a year*/` |
|  ! 0 |  400 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|  ! 0 |  401 | `			break;` |
|  ! 0 |  402 | `		case 'a':` |
|    - |  403 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|  ! 0 |  404 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|  ! 0 |  405 | `			break;` |
|  ! 0 |  406 | `		case 'A':` |
|    - |  407 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|  ! 0 |  408 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|  ! 0 |  409 | `			break;` |
|  ! 0 |  410 | `		case 'g':` |
|    - |  411 | `			/*	12-hour format of an hour without leading zeros*/` |
|  ! 0 |  412 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|  ! 0 |  413 | `			break;` |
|  ! 0 |  414 | `		case 'G':` |
|    - |  415 | `			/* 24-hour format of an hour without leading zeros */` |
|  ! 0 |  416 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|  ! 0 |  417 | `			break;` |
|  ! 0 |  418 | `		case 'h':` |
|    - |  419 | `			/* 12-hour format of an hour with leading zeros */` |
|  ! 0 |  420 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|  ! 0 |  421 | `			break;` |
|    3 |  422 | `		case 'H':` |
|    - |  423 | `			/*	24-hour format of an hour with leading zeros */` |
|    7 |  424 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|    7 |  425 | `			break;` |
|    3 |  426 | `		case 'i':` |
|    - |  427 | `			/* 	Minutes with leading zeros */` |
|    7 |  428 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|    7 |  429 | `			break;` |
|    3 |  430 | `		case 's':` |
|    - |  431 | `			/* 	second with leading zeros */` |
|    7 |  432 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    7 |  433 | `			break;` |
|  ! 0 |  434 | `		case 'u':` |
|    - |  435 | `			/* 	Microseconds */` |
|  ! 0 |  436 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|  ! 0 |  437 | `			break;` |
|  ! 0 |  438 | `		case 'S':{` |
|    - |  439 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|    - |  440 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|  ! 0 |  441 | `			int v = pTm->tm_mday;` |
|  ! 0 |  442 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|  ! 0 |  443 | `			break;` |
|    - |  444 | `				 }` |
|  ! 0 |  445 | `		case 'e':` |
|    - |  446 | `			/* 	Timezone identifier */` |
|  ! 0 |  447 | `			zCur = pTm->tm_zone;` |
|  ! 0 |  448 | `			if( zCur == 0 ){` |
|    - |  449 | `				/* Assume GMT */` |
|  ! 0 |  450 | `				zCur = "GMT";` |
|  ! 0 |  451 | `			}` |
|  ! 0 |  452 | `			ph7_result_string(pCtx,zCur,-1);` |
|  ! 0 |  453 | `			break;` |
|  ! 0 |  454 | `		case 'I':` |
|    - |  455 | `			/* Whether or not the date is in daylight saving time */` |
|    - |  456 | `#ifdef __WINNT__` |
|    - |  457 | `#ifdef _MSC_VER` |
|    - |  458 | `#ifndef _WIN32_WCE` |
|  ! 0 |  459 | `			_get_daylight(&pTm->tm_isdst);` |
|    - |  460 | `#endif` |
|    - |  461 | `#endif` |
|    - |  462 | `#endif` |
|  ! 0 |  463 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|  ! 0 |  464 | `			break;` |
|  ! 0 |  465 | `		case 'r':` |
|    - |  466 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|  ! 0 |  467 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|  ! 0 |  468 | `				SyTimeGetDay(pTm->tm_wday),` |
|  ! 0 |  469 | `				pTm->tm_mday,` |
|  ! 0 |  470 | `				SyTimeGetMonth(pTm->tm_mon),` |
|  ! 0 |  471 | `				pTm->tm_year,` |
|  ! 0 |  472 | `				pTm->tm_hour,` |
|  ! 0 |  473 | `				pTm->tm_min,` |
|  ! 0 |  474 | `				pTm->tm_sec` |
|    - |  475 | `				);` |
|  ! 0 |  476 | `			break;` |
|  ! 0 |  477 | `		case 'U':{` |
|    - |  478 | `			time_t tt;` |
|    - |  479 | `			/* Seconds since the Unix Epoch */` |
|  ! 0 |  480 | `			time(&tt);` |
|  ! 0 |  481 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|  ! 0 |  482 | `			break;` |
|    - |  483 | `				 }` |
|  ! 0 |  484 | `		case 'O':` |
|    - |  485 | `		case 'P':` |
|    - |  486 | `			/* Difference to Greenwich time (GMT) in hours */` |
|  ! 0 |  487 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|  ! 0 |  488 | `			break;` |
|  ! 0 |  489 | `		case 'Z':` |
|    - |  490 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|    - |  491 | `			 * is always negative, and for those east of UTC is always positive.` |
|    - |  492 | `			 */` |
|  ! 0 |  493 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|  ! 0 |  494 | `			break;` |
|    1 |  495 | `		case 'c':` |
|    - |  496 | `			/* 	ISO 8601 date */` |
|    4 |  497 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|    1 |  498 | `				pTm->tm_year,` |
|    2 |  499 | `				pTm->tm_mon+1,` |
|    1 |  500 | `				pTm->tm_mday,` |
|    1 |  501 | `				pTm->tm_hour,` |
|    1 |  502 | `				pTm->tm_min,` |
|    1 |  503 | `				pTm->tm_sec,` |
|    1 |  504 | `				pTm->tm_gmtoff` |
|    - |  505 | `				);` |
|    3 |  506 | `			break;` |
|    1 |  507 | `		case '\\':` |
|    3 |  508 | `			zIn++;` |
|    - |  509 | `			/* Expand verbatim */` |
|    3 |  510 | `			if( zIn < zEnd ){` |
|    3 |  511 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|    1 |  512 | `			}` |
|    3 |  513 | `			break;` |
|   17 |  514 | `		default:` |
|    - |  515 | `			/* Unknown format specifer,expand verbatim */` |
|   35 |  516 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|   34 |  517 | `			break;` |
|    - |  518 | `		}` |
|    - |  519 | `		/* Point to the next character */` |
|  111 |  520 | `		zIn++;` |
|    1 |  521 | `	}` |
|   47 |  522 | `	return SXRET_OK;` |
|    1 |  523 |  |
|    - |  524 | `/*` |
|    - |  525 | ` * PH7 implementation of the strftime() function.` |
|    - |  526 | ` * The following formats are supported:` |
|    - |  527 | ` * %a 	An abbreviated textual representation of the day` |
|    - |  528 | ` * %A 	A full textual representation of the day` |
|    - |  529 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|    - |  530 | ` * %e 	Day of the month, with a space preceding single digits.` |
|    - |  531 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|    - |  532 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|    - |  533 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|    - |  534 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|    - |  535 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|    - |  536 | ` *   4 weekdays, with Monday being the start of the week.` |
|    - |  537 | ` * %W 	A numeric representation of the week of the year` |
|    - |  538 | ` * %b 	Abbreviated month name, based on the locale` |
|    - |  539 | ` * %B 	Full month name, based on the locale` |
|    - |  540 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|    - |  541 | ` * %m 	Two digit representation of the month` |
|    - |  542 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|    - |  543 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|    - |  544 | ` * %G 	The full four-digit version of %g` |
|    - |  545 | ` * %y 	Two digit representation of the year` |
|    - |  546 | ` * %Y 	Four digit representation for the year` |
|    - |  547 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|    - |  548 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|    - |  549 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|    - |  550 | ` * %M 	Two digit representation of the minute` |
|    - |  551 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|    - |  552 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|    - |  553 | ` * %r 	Same as "%I:%M:%S %p"` |
|    - |  554 | ` * %R 	Same as "%H:%M"` |
|    - |  555 | ` * %S 	Two digit representation of the second` |
|    - |  556 | ` * %T 	Same as "%H:%M:%S"` |
|    - |  557 | ` * %X 	Preferred time representation based on locale, without the date` |
|    - |  558 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|    - |  559 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|    - |  560 | ` * %c 	Preferred date and time stamp based on local` |
|    - |  561 | ` * %D 	Same as "%m/%d/%y"` |
|    - |  562 | ` * %F 	Same as "%Y-%m-%d"` |
|    - |  563 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|    - |  564 | ` * %x 	Preferred date representation based on locale, without the time` |
|    - |  565 | ` * %n 	A newline character ("\n")` |
|    - |  566 | ` * %t 	A Tab character ("\t")` |
|    - |  567 | ` * %% 	A literal percentage character ("%")` |
|    - |  568 | ` */` |
|   16 |  569 | `static int PH7_Strftime(` |
|    - |  570 | `	ph7_context *pCtx,  /* Call context */` |
|    - |  571 | `	const char *zIn,    /* Input string */` |
|    - |  572 | `	int nLen,           /* Input length */` |
|    - |  573 | `	Sytm *pTm           /* Parse of the given time */` |
|    - |  574 | `	)` |
|    1 |  575 |  |
|   17 |  576 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|    - |  577 | `	int c;` |
|    - |  578 | `	/* Start the format process */` |
|   18 |  579 | `	for(;;){` |
|   37 |  580 | `		zCur = zIn;` |
|   41 |  581 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|    5 |  582 | `			zIn++;` |
|    1 |  583 | `		}` |
|   37 |  584 | `		if( zIn > zCur ){` |
|    - |  585 | `			/* Consume input verbatim */` |
|    5 |  586 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|    2 |  587 | `		}` |
|   37 |  588 | `		zIn++; /* Jump the percent sign */` |
|   37 |  589 | `		if( zIn >= zEnd ){` |
|    - |  590 | `			/* No more input to process */` |
|   17 |  591 | `			break;` |
|    - |  592 | `		}` |
|   21 |  593 | `		c = zIn[0];` |
|    - |  594 | `		/* Act according to the current specifer */` |
|   21 |  595 | `		switch(c){` |
|  ! 0 |  596 | `		case '%':` |
|    - |  597 | `			/* A literal percentage character ("%") */` |
|  ! 0 |  598 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|  ! 0 |  599 | `			break;` |
|  ! 0 |  600 | `		case 't':` |
|    - |  601 | `			/* A Tab character */` |
|  ! 0 |  602 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|  ! 0 |  603 | `			break;` |
|  ! 0 |  604 | `		case 'n':` |
|    - |  605 | `			/* A newline character */` |
|  ! 0 |  606 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|  ! 0 |  607 | `			break;` |
|    1 |  608 | `		case 'a':` |
|    - |  609 | `			/* An abbreviated textual representation of the day */` |
|    3 |  610 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|    3 |  611 | `			break;` |
|  ! 0 |  612 | `		case 'A':` |
|    - |  613 | `			/* A full textual representation of the day */` |
|  ! 0 |  614 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|  ! 0 |  615 | `			break;` |
|  ! 0 |  616 | `		case 'e':` |
|    - |  617 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|  ! 0 |  618 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|  ! 0 |  619 | `			break;` |
|    2 |  620 | `		case 'd':` |
|    - |  621 | `			/* Two-digit day of the month (with leading zeros) */` |
|    5 |  622 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|    5 |  623 | `			break;` |
|  ! 0 |  624 | `		case 'j':` |
|    - |  625 | `			/*The day of the year,3 digits with leading zeros*/` |
|  ! 0 |  626 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|  ! 0 |  627 | `			break;` |
|  ! 0 |  628 | `		case 'u':` |
|    - |  629 | `			/* ISO-8601 numeric representation of the day of the week */` |
|  ! 0 |  630 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|  ! 0 |  631 | `			break;` |
|  ! 0 |  632 | `		case 'w':` |
|    - |  633 | `			/* Numeric representation of the day of the week */` |
|  ! 0 |  634 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|  ! 0 |  635 | `			break;` |
|  ! 0 |  636 | `		case 'b':` |
|    - |  637 | `		case 'h':` |
|    - |  638 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|  ! 0 |  639 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|  ! 0 |  640 | `			break;` |
|  ! 0 |  641 | `		case 'B':` |
|    - |  642 | `			/* Full month name (Not based on locale) */` |
|  ! 0 |  643 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|  ! 0 |  644 | `			break;` |
|    2 |  645 | `		case 'm':` |
|    - |  646 | `			/*Numeric representation of a month, with leading zeros*/` |
|    5 |  647 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|    5 |  648 | `			break;` |
|  ! 0 |  649 | `		case 'C':` |
|    - |  650 | `			/* Two digit representation of the century */` |
|  ! 0 |  651 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|  ! 0 |  652 | `			break;` |
|  ! 0 |  653 | `		case 'y':` |
|    - |  654 | `		case 'g':` |
|    - |  655 | `			/* Two digit representation of the year */` |
|  ! 0 |  656 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|  ! 0 |  657 | `			break;` |
|    2 |  658 | `		case 'Y':` |
|    - |  659 | `		case 'G':` |
|    - |  660 | `			/* Four digit representation of the year */` |
|    5 |  661 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    5 |  662 | `			break;` |
|  ! 0 |  663 | `		case 'I':` |
|    - |  664 | `			/* 12-hour format of an hour with leading zeros */` |
|  ! 0 |  665 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|  ! 0 |  666 | `			break;` |
|  ! 0 |  667 | `		case 'l':` |
|    - |  668 | `			/* 12-hour format of an hour with leading space */` |
|  ! 0 |  669 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|  ! 0 |  670 | `			break;` |
|    1 |  671 | `		case 'H':` |
|    - |  672 | `			/* 24-hour format of an hour with leading zeros */` |
|    3 |  673 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|    3 |  674 | `			break;` |
|    1 |  675 | `		case 'M':` |
|    - |  676 | `			/* Minutes with leading zeros */` |
|    3 |  677 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|    3 |  678 | `			break;` |
|  ! 0 |  679 | `		case 'S':` |
|    - |  680 | `			/* Seconds with leading zeros */` |
|  ! 0 |  681 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|  ! 0 |  682 | `			break;` |
|  ! 0 |  683 | `		case 'z':` |
|    - |  684 | `		case 'Z':` |
|    - |  685 | `			/* 	Timezone identifier */` |
|  ! 0 |  686 | `			zCur = pTm->tm_zone;` |
|  ! 0 |  687 | `			if( zCur == 0 ){` |
|    - |  688 | `				/* Assume GMT */` |
|  ! 0 |  689 | `				zCur = "GMT";` |
|  ! 0 |  690 | `			}` |
|  ! 0 |  691 | `			ph7_result_string(pCtx,zCur,-1);` |
|  ! 0 |  692 | `			break;` |
|  ! 0 |  693 | `		case 'T':` |
|    - |  694 | `		case 'X':` |
|    - |  695 | `			/* Same as "%H:%M:%S" */` |
|  ! 0 |  696 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|  ! 0 |  697 | `			break;` |
|  ! 0 |  698 | `		case 'R':` |
|    - |  699 | `			/* Same as "%H:%M" */` |
|  ! 0 |  700 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|  ! 0 |  701 | `			break;` |
|  ! 0 |  702 | `		case 'P':` |
|    - |  703 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|  ! 0 |  704 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|  ! 0 |  705 | `			break;` |
|  ! 0 |  706 | `		case 'p':` |
|    - |  707 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|  ! 0 |  708 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|  ! 0 |  709 | `			break;` |
|  ! 0 |  710 | `		case 'r':` |
|    - |  711 | `			/* Same as "%I:%M:%S %p" */` |
|  ! 0 |  712 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|  ! 0 |  713 | `				1+(pTm->tm_hour%12),` |
|  ! 0 |  714 | `				pTm->tm_min,` |
|  ! 0 |  715 | `				pTm->tm_sec,` |
|  ! 0 |  716 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|    - |  717 | `				);` |
|  ! 0 |  718 | `			break;` |
|    1 |  719 | `		case 'D':` |
|    - |  720 | `		case 'x':` |
|    - |  721 | `			/* Same as "%m/%d/%y" */` |
|    4 |  722 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|    2 |  723 | `				pTm->tm_mon+1,` |
|    1 |  724 | `				pTm->tm_mday,` |
|    2 |  725 | `				pTm->tm_year%100` |
|    - |  726 | `				);` |
|    3 |  727 | `			break;` |
|  ! 0 |  728 | `		case 'F':` |
|    - |  729 | `			/* Same as "%Y-%m-%d" */` |
|  ! 0 |  730 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|  ! 0 |  731 | `				pTm->tm_year,` |
|  ! 0 |  732 | `				pTm->tm_mon+1,` |
|  ! 0 |  733 | `				pTm->tm_mday` |
|    - |  734 | `				);` |
|  ! 0 |  735 | `			break;` |
|  ! 0 |  736 | `		case 'c':` |
|  ! 0 |  737 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|  ! 0 |  738 | `				pTm->tm_year,` |
|  ! 0 |  739 | `				pTm->tm_mon+1,` |
|  ! 0 |  740 | `				pTm->tm_mday,` |
|  ! 0 |  741 | `				pTm->tm_hour,` |
|  ! 0 |  742 | `				pTm->tm_min,` |
|  ! 0 |  743 | `				pTm->tm_sec` |
|    - |  744 | `				);` |
|  ! 0 |  745 | `			break;` |
|  ! 0 |  746 | `		case 's':{` |
|    - |  747 | `			time_t tt;` |
|    - |  748 | `			/* Seconds since the Unix Epoch */` |
|  ! 0 |  749 | `			time(&tt);` |
|  ! 0 |  750 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|  ! 0 |  751 | `			break;` |
|    - |  752 | `				 }` |
|  ! 0 |  753 | `		default:` |
|    - |  754 | `			/* unknown specifer,simply ignore*/` |
|  ! 0 |  755 | `			break;` |
|    - |  756 | `		}` |
|    - |  757 | `		/* Advance the cursor */` |
|   21 |  758 | `		zIn++;` |
|    1 |  759 | `	}` |
|   17 |  760 | `	return SXRET_OK;` |
|    1 |  761 |  |
|    - |  762 | `/*` |
|    - |  763 | ` * string date(string $format [, int $timestamp = time() ] )` |
|    - |  764 | ` *  Returns a string formatted according to the given format string using` |
|    - |  765 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|    - |  766 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|    - |  767 | ` * Parameters` |
|    - |  768 | ` *  $format` |
|    - |  769 | ` *   The format of the outputted date string (See code above)` |
|    - |  770 | ` * $timestamp` |
|    - |  771 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  772 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  773 | ` *   In other words, it defaults to the value of time().` |
|    - |  774 | ` * Return` |
|    - |  775 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  776 | ` */` |
|   36 |  777 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  778 |  |
|    - |  779 | `	const char *zFormat;` |
|    - |  780 | `	int nLen;` |
|    - |  781 | `	Sytm sTm;` |
|   37 |  782 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  783 | `		/* Missing/Invalid argument,return FALSE */` |
|    5 |  784 | `		ph7_result_bool(pCtx,0);` |
|    5 |  785 | `		return PH7_OK;` |
|    - |  786 | `	}` |
|   33 |  787 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   33 |  788 | `	if( nLen < 1 ){` |
|    - |  789 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  790 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  791 | `	}` |
|   33 |  792 | `	if( nArg < 2 ){` |
|    - |  793 | `#ifdef __WINNT__` |
|    - |  794 | `		SYSTEMTIME sOS;` |
|    1 |  795 | `		GetSystemTime(&sOS);` |
|    1 |  796 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  797 | `#else` |
|    - |  798 | `		struct tm *pTm;` |
|    - |  799 | `		time_t t;` |
|   30 |  800 | `		time(&t);` |
|   30 |  801 | `		pTm = localtime(&t);` |
|   30 |  802 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  803 | `#endif` |
|   16 |  804 | `	}else{` |
|    - |  805 | `		/* Use the given timestamp */` |
|    - |  806 | `		time_t t;` |
|    - |  807 | `		struct tm *pTm;` |
|    3 |  808 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    3 |  809 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    3 |  810 | `			pTm = localtime(&t);` |
|    3 |  811 | `			if( pTm == 0 ){` |
|  ! 0 |  812 | `				time(&t);` |
|  ! 0 |  813 | `			}` |
|    2 |  814 | `		}else{` |
|  ! 0 |  815 | `			time(&t);` |
|    - |  816 | `		}` |
|    3 |  817 | `		pTm = localtime(&t);` |
|    3 |  818 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  819 | `	}` |
|    - |  820 | `	/* Format the given string */` |
|   33 |  821 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|   33 |  822 | `	return PH7_OK;` |
|   19 |  823 |  |
|    - |  824 | `/*` |
|    - |  825 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|    - |  826 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|    - |  827 | ` * Parameters` |
|    - |  828 | ` *  $format` |
|    - |  829 | ` *   The format of the outputted date string (See code above)` |
|    - |  830 | ` * $timestamp` |
|    - |  831 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  832 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  833 | ` *   In other words, it defaults to the value of time().` |
|    - |  834 | ` * Return` |
|    - |  835 | ` * Returns a string formatted according format using the given timestamp` |
|    - |  836 | ` * or the current local time if no timestamp is given.` |
|    - |  837 | ` */` |
|   20 |  838 | `PH7_PRIVATE int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  839 |  |
|    - |  840 | `	const char *zFormat;` |
|    - |  841 | `	int nLen;` |
|    - |  842 | `	Sytm sTm;` |
|   21 |  843 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  844 | `		/* Missing/Invalid argument,return FALSE */` |
|    5 |  845 | `		ph7_result_bool(pCtx,0);` |
|    5 |  846 | `		return PH7_OK;` |
|    - |  847 | `	}` |
|   17 |  848 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   17 |  849 | `	if( nLen < 1 ){` |
|    - |  850 | `		/* Don't bother processing return FALSE */` |
|  ! 0 |  851 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  852 | `	}` |
|   17 |  853 | `	if( nArg < 2 ){` |
|    - |  854 | `#ifdef __WINNT__` |
|    - |  855 | `		SYSTEMTIME sOS;` |
|    1 |  856 | `		GetSystemTime(&sOS);` |
|    1 |  857 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  858 | `#else` |
|    - |  859 | `		struct tm *pTm;` |
|    - |  860 | `		time_t t;` |
|   14 |  861 | `		time(&t);` |
|   14 |  862 | `		pTm = localtime(&t);` |
|   14 |  863 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  864 | `#endif` |
|    8 |  865 | `	}else{` |
|    - |  866 | `		/* Use the given timestamp */` |
|    - |  867 | `		time_t t;` |
|    - |  868 | `		struct tm *pTm;` |
|    3 |  869 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    3 |  870 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    3 |  871 | `			pTm = localtime(&t);` |
|    3 |  872 | `			if( pTm == 0 ){` |
|  ! 0 |  873 | `				time(&t);` |
|  ! 0 |  874 | `			}` |
|    2 |  875 | `		}else{` |
|  ! 0 |  876 | `			time(&t);` |
|    - |  877 | `		}` |
|    3 |  878 | `		pTm = localtime(&t);` |
|    3 |  879 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  880 | `	}` |
|    - |  881 | `	/* Format the given string */` |
|   17 |  882 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|   17 |  883 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|    - |  884 | `		/* Nothing was formatted,return FALSE */` |
|  ! 0 |  885 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  886 | `	}` |
|   17 |  887 | `	return PH7_OK;` |
|   11 |  888 |  |
|    - |  889 | `/*` |
|    - |  890 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|    - |  891 | ` *  Identical to the date() function except that the time returned` |
|    - |  892 | ` *  is Greenwich Mean Time (GMT).` |
|    - |  893 | ` * Parameters` |
|    - |  894 | ` *  $format` |
|    - |  895 | ` *  The format of the outputted date string (See code above)` |
|    - |  896 | ` *  $timestamp` |
|    - |  897 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  898 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  899 | ` *   In other words, it defaults to the value of time().` |
|    - |  900 | ` * Return` |
|    - |  901 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  902 | ` */` |
|   16 |  903 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  904 |  |
|    - |  905 | `	const char *zFormat;` |
|    - |  906 | `	int nLen;` |
|    - |  907 | `	Sytm sTm;` |
|   17 |  908 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  909 | `		/* Missing/Invalid argument,return FALSE */` |
|    3 |  910 | `		ph7_result_bool(pCtx,0);` |
|    3 |  911 | `		return PH7_OK;` |
|    - |  912 | `	}` |
|   15 |  913 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   15 |  914 | `	if( nLen < 1 ){` |
|    - |  915 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  916 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  917 | `	}` |
|   15 |  918 | `	if( nArg < 2 ){` |
|    - |  919 | `#ifdef __WINNT__` |
|    - |  920 | `		SYSTEMTIME sOS;` |
|    1 |  921 | `		GetSystemTime(&sOS);` |
|    1 |  922 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  923 | `#else` |
|    - |  924 | `		struct tm *pTm;` |
|    - |  925 | `		time_t t;` |
|   12 |  926 | `		time(&t);` |
|   12 |  927 | `		pTm = gmtime(&t);` |
|   12 |  928 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  929 | `#endif` |
|    7 |  930 | `	}else{` |
|    - |  931 | `		/* Use the given timestamp */` |
|    - |  932 | `		time_t t;` |
|    - |  933 | `		struct tm *pTm;` |
|    3 |  934 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    3 |  935 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    3 |  936 | `			pTm = gmtime(&t);` |
|    3 |  937 | `			if( pTm == 0 ){` |
|  ! 0 |  938 | `				time(&t);` |
|  ! 0 |  939 | `			}` |
|    2 |  940 | `		}else{` |
|  ! 0 |  941 | `			time(&t);` |
|    - |  942 | `		}` |
|    3 |  943 | `		pTm = gmtime(&t);` |
|    3 |  944 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  945 | `	}` |
|    - |  946 | `	/* Format the given string */` |
|   15 |  947 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|   15 |  948 | `	return PH7_OK;` |
|    9 |  949 |  |
|    - |  950 | `/*` |
|    - |  951 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|    - |  952 | ` *  Return the local time.` |
|    - |  953 | ` * Parameter` |
|    - |  954 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|    - |  955 | ` *     that defaults to the current local time if a timestamp is not given.` |
|    - |  956 | ` *     In other words, it defaults to the value of time().` |
|    - |  957 | ` * $is_associative` |
|    - |  958 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|    - |  959 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|    - |  960 | ` *   array containing all the different elements of the structure returned by the C function` |
|    - |  961 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|    - |  962 | ` *      "tm_sec" - seconds, 0 to 59` |
|    - |  963 | ` *      "tm_min" - minutes, 0 to 59` |
|    - |  964 | ` *      "tm_hour" - hours, 0 to 23` |
|    - |  965 | ` *      "tm_mday" - day of the month, 1 to 31` |
|    - |  966 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|    - |  967 | ` *      "tm_year" - years since 1900` |
|    - |  968 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|    - |  969 | ` *      "tm_yday" - day of the year, 0 to 365` |
|    - |  970 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|    - |  971 | ` * Returns` |
|    - |  972 | ` *  An associative array of information related to the timestamp.` |
|    - |  973 | ` */` |
|    8 |  974 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  975 |  |
|    - |  976 | `	ph7_value *pValue,*pArray;` |
|    9 |  977 | `	int isAssoc = 0;` |
|    - |  978 | `	Sytm sTm;` |
|    9 |  979 | `	if( nArg < 1 ){` |
|    - |  980 | `#ifdef __WINNT__` |
|    - |  981 | `		SYSTEMTIME sOS;` |
|    1 |  982 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|    1 |  983 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  984 | `#else` |
|    - |  985 | `		struct tm *pTm;` |
|    - |  986 | `		time_t t;` |
|    4 |  987 | `		time(&t);` |
|    4 |  988 | `		pTm = localtime(&t);` |
|    4 |  989 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  990 | `#endif` |
|    3 |  991 | `	}else{` |
|    - |  992 | `		/* Use the given timestamp */` |
|    - |  993 | `		time_t t;` |
|    - |  994 | `		struct tm *pTm;` |
|    5 |  995 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    5 |  996 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|    5 |  997 | `			pTm = localtime(&t);` |
|    5 |  998 | `			if( pTm == 0 ){` |
|  ! 0 |  999 | `				time(&t);` |
|  ! 0 | 1000 | `			}` |
|    3 | 1001 | `		}else{` |
|  ! 0 | 1002 | `			time(&t);` |
|    - | 1003 | `		}` |
|    5 | 1004 | `		pTm = localtime(&t);` |
|    5 | 1005 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1006 | `	}` |
|    - | 1007 | `	/* Element value */` |
|    9 | 1008 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    9 | 1009 | `	if( pValue == 0 ){` |
|    - | 1010 | `		/* Return NULL */` |
|  ! 0 | 1011 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1012 | `		return PH7_OK;` |
|    - | 1013 | `	}` |
|    - | 1014 | `	/* Create a new array */` |
|    9 | 1015 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 | 1016 | `	if( pArray == 0 ){` |
|    - | 1017 | `		/* Return NULL */` |
|  ! 0 | 1018 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1019 | `		return PH7_OK;` |
|    - | 1020 | `	}` |
|    9 | 1021 | `	if( nArg > 1 ){` |
|    3 | 1022 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|    1 | 1023 | `	}` |
|    - | 1024 | `	/* Fill the array */` |
|    - | 1025 | `	/* Seconds */` |
|    9 | 1026 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    9 | 1027 | `	if( isAssoc ){` |
|    3 | 1028 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|    2 | 1029 | `	}else{` |
|    7 | 1030 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1031 | `	}` |
|    - | 1032 | `	/* Minutes */` |
|    9 | 1033 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    9 | 1034 | `	if( isAssoc ){` |
|    3 | 1035 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|    2 | 1036 | `	}else{` |
|    7 | 1037 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1038 | `	}` |
|    - | 1039 | `	/* Hours */` |
|    9 | 1040 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    9 | 1041 | `	if( isAssoc ){` |
|    3 | 1042 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|    2 | 1043 | `	}else{` |
|    7 | 1044 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1045 | `	}` |
|    - | 1046 | `	/* mday */` |
|    9 | 1047 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    9 | 1048 | `	if( isAssoc ){` |
|    3 | 1049 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|    2 | 1050 | `	}else{` |
|    7 | 1051 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1052 | `	}` |
|    - | 1053 | `	/* mon */` |
|    9 | 1054 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|    9 | 1055 | `	if( isAssoc ){` |
|    3 | 1056 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|    2 | 1057 | `	}else{` |
|    7 | 1058 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1059 | `	}` |
|    - | 1060 | `	/* year since 1900 */` |
|    9 | 1061 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|    9 | 1062 | `	if( isAssoc ){` |
|    3 | 1063 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|    2 | 1064 | `	}else{` |
|    7 | 1065 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1066 | `	}` |
|    - | 1067 | `	/* wday */` |
|    9 | 1068 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    9 | 1069 | `	if( isAssoc ){` |
|    3 | 1070 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|    2 | 1071 | `	}else{` |
|    7 | 1072 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1073 | `	}` |
|    - | 1074 | `	/* yday */` |
|    9 | 1075 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    9 | 1076 | `	if( isAssoc ){` |
|    3 | 1077 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|    2 | 1078 | `	}else{` |
|    7 | 1079 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1080 | `	}` |
|    - | 1081 | `	/* isdst */` |
|    - | 1082 | `#ifdef __WINNT__` |
|    - | 1083 | `#ifdef _MSC_VER` |
|    - | 1084 | `#ifndef _WIN32_WCE` |
|    1 | 1085 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1086 | `#endif` |
|    - | 1087 | `#endif` |
|    - | 1088 | `#endif` |
|    9 | 1089 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|    9 | 1090 | `	if( isAssoc ){` |
|    3 | 1091 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|    2 | 1092 | `	}else{` |
|    7 | 1093 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1094 | `	}` |
|    - | 1095 | `	/* Return the array */` |
|    9 | 1096 | `	ph7_result_value(pCtx,pArray);` |
|    9 | 1097 | `	return PH7_OK;` |
|    5 | 1098 |  |
|    - | 1099 | `/*` |
|    - | 1100 | ` * int idate(string $format [, int $timestamp = time() ])` |
|    - | 1101 | ` *  Returns a number formatted according to the given format string` |
|    - | 1102 | ` *  using the given integer timestamp or the current local time if` |
|    - | 1103 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|    - | 1104 | ` *  to the value of time().` |
|    - | 1105 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|    - | 1106 | ` *  parameter.` |
|    - | 1107 | ` * $Parameters` |
|    - | 1108 | ` *  Supported format` |
|    - | 1109 | ` *   d 	Day of the month` |
|    - | 1110 | ` *   h 	Hour (12 hour format)` |
|    - | 1111 | ` *   H 	Hour (24 hour format)` |
|    - | 1112 | ` *   i 	Minutes` |
|    - | 1113 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|    - | 1114 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|    - | 1115 | ` *   m 	Month number` |
|    - | 1116 | ` *   s 	Seconds` |
|    - | 1117 | ` *   t 	Days in current month` |
|    - | 1118 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|    - | 1119 | ` *   w 	Day of the week (0 on Sunday)` |
|    - | 1120 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|    - | 1121 | ` *   y 	Year (1 or 2 digits - check note below)` |
|    - | 1122 | ` *   Y 	Year (4 digits)` |
|    - | 1123 | ` *   z 	Day of the year` |
|    - | 1124 | ` *   Z 	Timezone offset in seconds` |
|    - | 1125 | ` * $timestamp` |
|    - | 1126 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|    - | 1127 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|    - | 1128 | ` *  to the value of time().` |
|    - | 1129 | ` * Return` |
|    - | 1130 | ` *  An integer.` |
|    - | 1131 | ` */` |
|   42 | 1132 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1133 |  |
|    - | 1134 | `	const char *zFormat;` |
|   44 | 1135 | `	ph7_int64 iVal = 0;` |
|    - | 1136 | `	int nLen;` |
|    - | 1137 | `	Sytm sTm;` |
|   44 | 1138 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1139 | `		/* Missing/Invalid argument,return -1 */` |
|    5 | 1140 | `		ph7_result_int(pCtx,-1);` |
|    5 | 1141 | `		return PH7_OK;` |
|    - | 1142 | `	}` |
|   40 | 1143 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   40 | 1144 | `	if( nLen < 1 ){` |
|    - | 1145 | `		/* Don't bother processing return -1*/` |
|  ! 0 | 1146 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1147 | `	}` |
|   40 | 1148 | `	if( nArg < 2 ){` |
|    - | 1149 | `#ifdef __WINNT__` |
|    - | 1150 | `		SYSTEMTIME sOS;` |
|    2 | 1151 | `		GetSystemTime(&sOS);` |
|    2 | 1152 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - | 1153 | `#else` |
|    - | 1154 | `		struct tm *pTm;` |
|    - | 1155 | `		time_t t;` |
|   28 | 1156 | `		time(&t);` |
|   28 | 1157 | `		pTm = localtime(&t);` |
|   28 | 1158 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1159 | `#endif` |
|   16 | 1160 | `	}else{` |
|    - | 1161 | `		/* Use the given timestamp */` |
|    - | 1162 | `		time_t t;` |
|    - | 1163 | `		struct tm *pTm;` |
|   11 | 1164 | `		if( ph7_value_is_int(apArg[1]) ){` |
|   11 | 1165 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|   11 | 1166 | `			pTm = localtime(&t);` |
|   11 | 1167 | `			if( pTm == 0 ){` |
|  ! 0 | 1168 | `				time(&t);` |
|  ! 0 | 1169 | `			}` |
|    6 | 1170 | `		}else{` |
|  ! 0 | 1171 | `			time(&t);` |
|    - | 1172 | `		}` |
|   11 | 1173 | `		pTm = localtime(&t);` |
|   11 | 1174 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1175 | `	}` |
|    - | 1176 | `	/* Perform the requested operation */` |
|   40 | 1177 | `	switch(zFormat[0]){` |
|    2 | 1178 | `	case 'd':` |
|    - | 1179 | `		/* Day of the month */` |
|    5 | 1180 | `		iVal = sTm.tm_mday;` |
|    5 | 1181 | `		break;` |
|  ! 0 | 1182 | `	case 'h':` |
|    - | 1183 | `		/*	Hour (12 hour format)*/` |
|  ! 0 | 1184 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|  ! 0 | 1185 | `		break;` |
|    1 | 1186 | `	case 'H':` |
|    - | 1187 | `		/* Hour (24 hour format)*/` |
|    3 | 1188 | `		iVal = sTm.tm_hour;` |
|    3 | 1189 | `		break;` |
|    1 | 1190 | `	case 'i':` |
|    - | 1191 | `		/*Minutes*/` |
|    3 | 1192 | `		iVal = sTm.tm_min;` |
|    3 | 1193 | `		break;` |
|    1 | 1194 | `	case 'I':` |
|    - | 1195 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|    - | 1196 | `#ifdef __WINNT__` |
|    - | 1197 | `#ifdef _MSC_VER` |
|    - | 1198 | `#ifndef _WIN32_WCE` |
|    1 | 1199 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1200 | `#endif` |
|    - | 1201 | `#endif` |
|    - | 1202 | `#endif` |
|    3 | 1203 | `		iVal = sTm.tm_isdst;` |
|    3 | 1204 | `		break;` |
|    1 | 1205 | `	case 'L':` |
|    - | 1206 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|    3 | 1207 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|    3 | 1208 | `		break;` |
|    2 | 1209 | `	case 'm':` |
|    - | 1210 | `		/* Month number*/` |
|    5 | 1211 | `		iVal = sTm.tm_mon;` |
|    5 | 1212 | `		break;` |
|    1 | 1213 | `	case 's':` |
|    - | 1214 | `		/*Seconds*/` |
|    3 | 1215 | `		iVal = sTm.tm_sec;` |
|    3 | 1216 | `		break;` |
|    1 | 1217 | `	case 't':{` |
|    - | 1218 | `		/*Days in current month*/` |
|    - | 1219 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    3 | 1220 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|    3 | 1221 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|  ! 0 | 1222 | `			nDays = 28;` |
|  ! 0 | 1223 | `		}` |
|    3 | 1224 | `		iVal = nDays;` |
|    3 | 1225 | `		break;` |
|    - | 1226 | `			 }` |
|    1 | 1227 | `	case 'U':` |
|    - | 1228 | `		/*Seconds since the Unix Epoch*/` |
|    3 | 1229 | `		iVal = (ph7_int64)time(0);` |
|    3 | 1230 | `		break;` |
|    1 | 1231 | `	case 'w':` |
|    - | 1232 | `		/*	Day of the week (0 on Sunday) */` |
|    3 | 1233 | `		iVal = sTm.tm_wday;` |
|    3 | 1234 | `		break;` |
|    1 | 1235 | `	case 'W': {` |
|    - | 1236 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|    - | 1237 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|    3 | 1238 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|    3 | 1239 | `		break;` |
|    - | 1240 | `			  }` |
|  ! 0 | 1241 | `	case 'y':` |
|    - | 1242 | `		/* Year (2 digits) */` |
|  ! 0 | 1243 | `		iVal = sTm.tm_year % 100;` |
|  ! 0 | 1244 | `		break;` |
|    3 | 1245 | `	case 'Y':` |
|    - | 1246 | `		/* Year (4 digits) */` |
|    7 | 1247 | `		iVal = sTm.tm_year;` |
|    7 | 1248 | `		break;` |
|    1 | 1249 | `	case 'z':` |
|    - | 1250 | `		/* Day of the year */` |
|    3 | 1251 | `		iVal = sTm.tm_yday;` |
|    3 | 1252 | `		break;` |
|    1 | 1253 | `	case 'Z':` |
|    - | 1254 | `		/*Timezone offset in seconds*/` |
|    3 | 1255 | `		iVal = sTm.tm_gmtoff;` |
|    3 | 1256 | `		break;` |
|    1 | 1257 | `	default:` |
|    - | 1258 | `		/* unknown format,throw a warning */` |
|    3 | 1259 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|    2 | 1260 | `		break;` |
|    - | 1261 | `	}` |
|    - | 1262 | `	/* Return the time value */` |
|   40 | 1263 | `	ph7_result_int64(pCtx,iVal);` |
|   40 | 1264 | `	return PH7_OK;` |
|   23 | 1265 |  |
|    - | 1266 | `/*` |
|    - | 1267 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|    - | 1268 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|    - | 1269 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|    - | 1270 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|    - | 1271 | ` *  specified.` |
|    - | 1272 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|    - | 1273 | ` *  the current value according to the local date and time.` |
|    - | 1274 | ` * Parameters` |
|    - | 1275 | ` * $hour` |
|    - | 1276 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|    - | 1277 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|    - | 1278 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|    - | 1279 | ` * $minute` |
|    - | 1280 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|    - | 1281 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|    - | 1282 | ` *  in the following hour(s).` |
|    - | 1283 | ` * $second` |
|    - | 1284 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|    - | 1285 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|    - | 1286 | ` * second in the following minute(s).` |
|    - | 1287 | ` * $month` |
|    - | 1288 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|    - | 1289 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|    - | 1290 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|    - | 1291 | ` * $day` |
|    - | 1292 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|    - | 1293 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|    - | 1294 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|    - | 1295 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|    - | 1296 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|    - | 1297 | ` * $year` |
|    - | 1298 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|    - | 1299 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|    - | 1300 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|    - | 1301 | ` * $is_dst` |
|    - | 1302 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|    - | 1303 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|    - | 1304 | ` * Return` |
|    - | 1305 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|    - | 1306 | ` *   If the arguments are invalid, the function returns FALSE` |
|    - | 1307 | ` */` |
|    8 | 1308 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1309 |  |
|    - | 1310 | `	const char *zFunction;` |
|    9 | 1311 | `	ph7_int64 iVal = 0;` |
|    - | 1312 | `	struct tm *pTm;` |
|    - | 1313 | `	time_t t;` |
|    - | 1314 | `	/* Extract function name */` |
|    9 | 1315 | `	zFunction = ph7_function_name(pCtx);` |
|    - | 1316 | `	/* Get the current time */` |
|    9 | 1317 | `	time(&t);` |
|    9 | 1318 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|    3 | 1319 | `		pTm = gmtime(&t);` |
|    2 | 1320 | `	}else{` |
|    - | 1321 | `		/* localtime */` |
|    7 | 1322 | `		pTm = localtime(&t);` |
|    - | 1323 | `	}` |
|    9 | 1324 | `	if( nArg > 0 ){` |
|    - | 1325 | `		int iTmp;` |
|    - | 1326 | `		/* Hour */` |
|    9 | 1327 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|    9 | 1328 | `		pTm->tm_hour = iTmp;` |
|    9 | 1329 | `		if( nArg > 1 ){` |
|    - | 1330 | `			/* Minutes */` |
|    9 | 1331 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|    9 | 1332 | `			pTm->tm_min = iTmp;` |
|    9 | 1333 | `			if( nArg > 2 ){` |
|    - | 1334 | `				/* Seconds */` |
|    9 | 1335 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|    9 | 1336 | `				pTm->tm_sec = iTmp;` |
|    9 | 1337 | `				if( nArg > 3 ){` |
|    - | 1338 | `					/* Month */` |
|    9 | 1339 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|    9 | 1340 | `					pTm->tm_mon = iTmp - 1;` |
|    9 | 1341 | `					if( nArg > 4 ){` |
|    - | 1342 | `						/* mday */` |
|    9 | 1343 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|    9 | 1344 | `						pTm->tm_mday = iTmp;` |
|    9 | 1345 | `						if( nArg > 5 ){` |
|    - | 1346 | `							/* Year */` |
|    9 | 1347 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|    9 | 1348 | `							if( iTmp > 1900 ){` |
|    9 | 1349 | `								iTmp -= 1900;` |
|    4 | 1350 | `							}` |
|    9 | 1351 | `							pTm->tm_year = iTmp;` |
|    9 | 1352 | `							if( nArg > 6 ){` |
|    - | 1353 | `								/* is_dst */` |
|  ! 0 | 1354 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|  ! 0 | 1355 | `								pTm->tm_isdst = iTmp;` |
|  ! 0 | 1356 | `							}` |
|    4 | 1357 | `						}` |
|    4 | 1358 | `					}` |
|    4 | 1359 | `				}` |
|    4 | 1360 | `			}` |
|    4 | 1361 | `		}` |
|    4 | 1362 | `	}` |
|    - | 1363 | `	/* Make the time */` |
|    9 | 1364 | `	iVal = (ph7_int64)mktime(pTm);` |
|    - | 1365 | `	/* Return the timesatmp as a 64bit integer */` |
|    9 | 1366 | `	ph7_result_int64(pCtx,iVal);` |
|    9 | 1367 | `	return PH7_OK;` |
|    1 | 1368 |  |
|    - | 1369 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1370 |  |
