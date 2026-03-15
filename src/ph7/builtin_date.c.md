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
|    - |   25 | `/*` |
|    - |   26 | `** WindowsCE does not have a localtime() function.  So create a` |
|    - |   27 | `** substitute.` |
|    - |   28 | `** Taken from the SQLite3 source tree.` |
|    - |   29 | `** Status: Public domain` |
|    - |   30 | `*/` |
|    - |   31 | `struct tm *__cdecl localtime(const time_t *t)` |
|    - |   32 |  |
|    - |   33 | `  static struct tm y;` |
|    - |   34 | `  FILETIME uTm, lTm;` |
|    - |   35 | `  SYSTEMTIME pTm;` |
|    - |   36 | `  ph7_int64 t64;` |
|    - |   37 | `  t64 = *t;` |
|    - |   38 | `  t64 = (t64 + 11644473600)*10000000;` |
|    - |   39 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|    - |   40 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|    - |   41 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|    - |   42 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|    - |   43 | `  y.tm_year = pTm.wYear - 1900;` |
|    - |   44 | `  y.tm_mon = pTm.wMonth - 1;` |
|    - |   45 | `  y.tm_wday = pTm.wDayOfWeek;` |
|    - |   46 | `  y.tm_mday = pTm.wDay;` |
|    - |   47 | `  y.tm_hour = pTm.wHour;` |
|    - |   48 | `  y.tm_min = pTm.wMinute;` |
|    - |   49 | `  y.tm_sec = pTm.wSecond;` |
|    - |   50 | `  return &y;` |
|    - |   51 |  |
|    - |   52 | `#endif /*_WIN32_WCE */` |
|    - |   53 | `#elif defined(__UNIXES__)` |
|    - |   54 | `#include <sys/time.h>` |
|    - |   55 | `#endif /* __WINNT__*/` |
|    - |   56 | ` /*` |
|    - |   57 | `  * int64 time(void)` |
|    - |   58 | `  *  Current Unix timestamp` |
|    - |   59 | `  * Parameters` |
|    - |   60 | `  *  None.` |
|    - |   61 | `  * Return` |
|    - |   62 | `  *  Returns the current time measured in the number of seconds` |
|    - |   63 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|    - |   64 | `  */` |
|    8 |   65 | `PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   66 |  |
|    - |   67 | `	time_t tt;` |
|    4 |   68 | `	SXUNUSED(nArg); /* cc warning */` |
|    4 |   69 | `	SXUNUSED(apArg);` |
|    - |   70 | `	/* Extract the current time */` |
|    9 |   71 | `	time(&tt);` |
|    - |   72 | `	/* Return as 64-bit integer */` |
|    9 |   73 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|    9 |   74 | `	return  PH7_OK;` |
|    1 |   75 |  |
|    - |   76 | `/*` |
|    - |   77 | `  * string/float microtime([ bool $get_as_float = false ])` |
|    - |   78 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|    - |   79 | `  * Parameters` |
|    - |   80 | `  *  $get_as_float` |
|    - |   81 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|    - |   82 | `  *   as described in the return values section below.` |
|    - |   83 | `  * Return` |
|    - |   84 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|    - |   85 | `  *  is the current time measured in the number of seconds since the Unix` |
|    - |   86 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|    - |   87 | `  *  that have elapsed since sec expressed in seconds.` |
|    - |   88 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|    - |   89 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|    - |   90 | `  */` |
|   20 |   91 | `PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   92 |  |
|   21 |   93 | `	int bFloat = 0;` |
|    - |   94 | `	sytime sTime;` |
|    - |   95 | `#if defined(__UNIXES__)` |
|    - |   96 | `	struct timeval tv;` |
|   20 |   97 | `	gettimeofday(&tv,0);` |
|   20 |   98 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|   20 |   99 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|    - |  100 | `#else` |
|    - |  101 | `	time_t tt;` |
|    1 |  102 | `	time(&tt);` |
|    1 |  103 | `	sTime.tm_sec  = (long)tt;` |
|    1 |  104 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|    - |  105 | `#endif /* __UNIXES__ */` |
|   21 |  106 | `	if( nArg > 0 ){` |
|   17 |  107 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|    8 |  108 | `	}` |
|   21 |  109 | `	if( bFloat ){` |
|    - |  110 | `		/* Return as float */` |
|   17 |  111 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|    9 |  112 | `	}else{` |
|    - |  113 | `		/* Return as string */` |
|    5 |  114 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|    - |  115 | `	}` |
|   21 |  116 | `	return PH7_OK;` |
|    1 |  117 |  |
|    - |  118 | `/*` |
|    - |  119 | ` * array getdate ([ int $timestamp = time() ])` |
|    - |  120 | ` *  Returns an associative array containing the date information` |
|    - |  121 | ` *  of the timestamp, or the current local time if no timestamp is given.` |
|    - |  122 | ` * Parameter` |
|    - |  123 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|    - |  124 | ` *     that defaults to the current local time if a timestamp is not given.` |
|    - |  125 | ` *     In other words, it defaults to the value of time().` |
|    - |  126 | ` * Returns` |
|    - |  127 | ` *  Returns an associative array of information related to the timestamp.` |
|    - |  128 | ` */` |
|    8 |  129 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  130 |  |
|    - |  131 | `	ph7_value *pValue,*pArray;` |
|    - |  132 | `	Sytm sTm;` |
|    9 |  133 | `	if( nArg < 1 ){` |
|    - |  134 | `#ifdef __WINNT__` |
|    - |  135 | `		SYSTEMTIME sOS;` |
|    1 |  136 | `		GetSystemTime(&sOS);` |
|    1 |  137 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  138 | `#else` |
|    - |  139 | `		struct tm *pTm;` |
|    - |  140 | `		time_t t;` |
|    4 |  141 | `		time(&t);` |
|    4 |  142 | `		pTm = localtime(&t);` |
|    4 |  143 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  144 | `#endif` |
|    3 |  145 | `	}else{` |
|    - |  146 | `		/* Use the given timestamp */` |
|    - |  147 | `		time_t t;` |
|    - |  148 | `		struct tm *pTm;` |
|    5 |  149 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    5 |  150 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|    5 |  151 | `			pTm = localtime(&t);` |
|    5 |  152 | `			if( pTm == 0 ){` |
|  ! 0 |  153 | `				time(&t);` |
|  ! 0 |  154 | `			}` |
|    3 |  155 | `		}else{` |
|  ! 0 |  156 | `			time(&t);` |
|    - |  157 | `		}` |
|    5 |  158 | `		pTm = localtime(&t);` |
|    5 |  159 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  160 | `	}` |
|    - |  161 | `	/* Element value */` |
|    9 |  162 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    9 |  163 | `	if( pValue == 0 ){` |
|    - |  164 | `		/* Return NULL */` |
|  ! 0 |  165 | `		ph7_result_null(pCtx);` |
|  ! 0 |  166 | `		return PH7_OK;` |
|    - |  167 | `	}` |
|    - |  168 | `	/* Create a new array */` |
|    9 |  169 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 |  170 | `	if( pArray == 0 ){` |
|    - |  171 | `		/* Return NULL */` |
|  ! 0 |  172 | `		ph7_result_null(pCtx);` |
|  ! 0 |  173 | `		return PH7_OK;` |
|    - |  174 | `	}` |
|    - |  175 | `	/* Fill the array */` |
|    - |  176 | `	/* Seconds */` |
|    9 |  177 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    9 |  178 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|    - |  179 | `	/* Minutes */` |
|    9 |  180 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    9 |  181 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|    - |  182 | `	/* Hours */` |
|    9 |  183 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    9 |  184 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|    - |  185 | `	/* mday */` |
|    9 |  186 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    9 |  187 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|    - |  188 | `	/* wday */` |
|    9 |  189 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    9 |  190 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|    - |  191 | `	/* mon */` |
|    9 |  192 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|    9 |  193 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|    - |  194 | `	/* year */` |
|    9 |  195 | `	ph7_value_int(pValue,sTm.tm_year);` |
|    9 |  196 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|    - |  197 | `	/* yday */` |
|    9 |  198 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    9 |  199 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|    - |  200 | `	/* Weekday [i.e: Monday,Tuesday,...] */` |
|    9 |  201 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|    9 |  202 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|    - |  203 | `	/* Reset the string cursor */` |
|    9 |  204 | `	ph7_value_reset_string_cursor(pValue);` |
|    - |  205 | `	/* Month [i.e: January,February,...] */` |
|    9 |  206 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|    9 |  207 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|    - |  208 | `	/* Return the freshly created array */` |
|    9 |  209 | `	ph7_result_value(pCtx,pArray);` |
|    9 |  210 | `	return PH7_OK;` |
|    5 |  211 |  |
|    - |  212 | `/*` |
|    - |  213 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|    - |  214 | ` *  Returns an associative array containing the data returned from the system call.` |
|    - |  215 | ` * Parameters` |
|    - |  216 | ` *  $return_float` |
|    - |  217 | ` *   When set to TRUE, a float instead of an array is returned.` |
|    - |  218 | ` * Return` |
|    - |  219 | ` *  By default an array is returned. If return_float is set, then` |
|    - |  220 | ` *  a float is returned.` |
|    - |  221 | ` */` |
|    4 |  222 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  223 |  |
|    5 |  224 | `	int bFloat = 0;` |
|    - |  225 | `	sytime sTime;` |
|    - |  226 | `#if defined(__UNIXES__)` |
|    - |  227 | `	struct timeval tv;` |
|    4 |  228 | `	gettimeofday(&tv,0);` |
|    4 |  229 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|    4 |  230 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|    - |  231 | `#else` |
|    - |  232 | `	time_t tt;` |
|    1 |  233 | `	time(&tt);` |
|    1 |  234 | `	sTime.tm_sec  = (long)tt;` |
|    1 |  235 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|    - |  236 | `#endif /* __UNIXES__ */` |
|    5 |  237 | `	if( nArg > 0 ){` |
|    5 |  238 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|    2 |  239 | `	}` |
|    5 |  240 | `	if( bFloat ){` |
|    - |  241 | `		/* Return as float */` |
|    3 |  242 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|    2 |  243 | `	}else{` |
|    - |  244 | `		/* Return an associative array */` |
|    - |  245 | `		ph7_value *pValue,*pArray;` |
|    - |  246 | `		/* Create a new array */` |
|    3 |  247 | `		pArray = ph7_context_new_array(pCtx);` |
|    - |  248 | `		/* Element value */` |
|    3 |  249 | `		pValue = ph7_context_new_scalar(pCtx);` |
|    3 |  250 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    - |  251 | `			/* Return NULL */` |
|  ! 0 |  252 | `			ph7_result_null(pCtx);` |
|  ! 0 |  253 | `			return PH7_OK;` |
|    - |  254 | `		}` |
|    - |  255 | `		/* Fill the array */` |
|    - |  256 | `		/* sec */` |
|    3 |  257 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|    3 |  258 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|    - |  259 | `		/* usec */` |
|    3 |  260 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|    3 |  261 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|    - |  262 | `		/* Return the array */` |
|    3 |  263 | `		ph7_result_value(pCtx,pArray);` |
|    - |  264 | `	}` |
|    5 |  265 | `	return PH7_OK;` |
|    3 |  266 |  |
|    - |  267 | `/* Check if the given year is leap or not */` |
|    - |  268 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|    - |  269 | `/* ISO-8601 numeric representation of the day of the week */` |
|    - |  270 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|    - |  271 | `/*` |
|    - |  272 | ` * Format a given date string.` |
|    - |  273 | ` * Supported format: (Taken from PHP online docs)` |
|    - |  274 | ` * character 	Description` |
|    - |  275 | ` * d          Day of the month, 2 digits with leading zeros` |
|    - |  276 | ` * D          A textual representation of a day, three letters` |
|    - |  277 | ` * j          Day of the month without leading zeros` |
|    - |  278 | ` * l          A full textual representation of the day of the week` |
|    - |  279 | ` * N          ISO-8601 numeric representation of the day of the week` |
|    - |  280 | ` * w          Numeric representation of the day of the week` |
|    - |  281 | ` * z          The day of the year (starting from 0)` |
|    - |  282 | ` * F          A full textual representation of a month, such as January or March` |
|    - |  283 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|    - |  284 | ` * M          A short textual representation of a month, three letters` |
|    - |  285 | ` * n          Numeric representation of a month, without leading zeros` |
|    - |  286 | ` * t          Number of days in the given month` |
|    - |  287 | ` * L          Whether it's a leap year` |
|    - |  288 | ` * o          ISO-8601 year number. This has the same value as Y` |
|    - |  289 | ` * Y          A full numeric representation of a year, 4 digits` |
|    - |  290 | ` * y          A two digit representation of a year` |
|    - |  291 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|    - |  292 | ` * A          Uppercase Ante meridiem and Post meridiem` |
|    - |  293 | ` * g          12-hour format of an hour without leading zeros` |
|    - |  294 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|    - |  295 | ` * h          12-hour format of an hour with leading zeros` |
|    - |  296 | ` * H          24-hour format of an hour with leading zeros` |
|    - |  297 | ` * i          Minutes with leading zeros` |
|    - |  298 | ` * s          Seconds, with leading zeros` |
|    - |  299 | ` * u          Microseconds` |
|    - |  300 | ` * e          Timezone identifier` |
|    - |  301 | ` * I          Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|    - |  302 | ` * r          RFC 2822 formatted date` |
|    - |  303 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|    - |  304 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|    - |  305 | ` * O          Difference to Greenwich time (GMT) in hours` |
|    - |  306 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|    - |  307 | ` *            east of UTC is always positive.` |
|    - |  308 | ` * c         ISO 8601 date` |
|    - |  309 | ` */` |
|   46 |  310 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|    1 |  311 |  |
|   47 |  312 | `	const char *zEnd = &zIn[nLen];` |
|    - |  313 | `	const char *zCur;` |
|    - |  314 | `	/* Start the format process */` |
|   78 |  315 | `	for(;;){` |
|  157 |  316 | `		if( zIn >= zEnd ){` |
|    - |  317 | `			/* No more input to process */` |
|   47 |  318 | `			break;` |
|    - |  319 | `		}` |
|  111 |  320 | `		switch(zIn[0]){` |
|    7 |  321 | `		case 'd':` |
|    - |  322 | `			/* Day of the month, 2 digits with leading zeros */` |
|   15 |  323 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|   15 |  324 | `			break;` |
|  ! 0 |  325 | `		case 'D':` |
|    - |  326 | `			/*A textual representation of a day, three letters*/` |
|  ! 0 |  327 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|  ! 0 |  328 | `			ph7_result_string(pCtx,zCur,3);` |
|  ! 0 |  329 | `			break;` |
|  ! 0 |  330 | `		case 'j':` |
|    - |  331 | `			/*	Day of the month without leading zeros */` |
|  ! 0 |  332 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|  ! 0 |  333 | `			break;` |
|    2 |  334 | `		case 'l':` |
|    - |  335 | `			/* A full textual representation of the day of the week */` |
|    5 |  336 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    5 |  337 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|    5 |  338 | `			break;` |
|  ! 0 |  339 | `		case 'N':{` |
|    - |  340 | `			/* ISO-8601 numeric representation of the day of the week */` |
|  ! 0 |  341 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|  ! 0 |  342 | `			break;` |
|    - |  343 | `				 }` |
|  ! 0 |  344 | `		case 'w':` |
|    - |  345 | `			/*Numeric representation of the day of the week*/` |
|  ! 0 |  346 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|  ! 0 |  347 | `			break;` |
|  ! 0 |  348 | `		case 'z':` |
|    - |  349 | `			/*The day of the year*/` |
|  ! 0 |  350 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|  ! 0 |  351 | `			break;` |
|    2 |  352 | `		case 'F':` |
|    - |  353 | `			/*A full textual representation of a month, such as January or March*/` |
|    5 |  354 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    5 |  355 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|    5 |  356 | `			break;` |
|    7 |  357 | `		case 'm':` |
|    - |  358 | `			/*Numeric representation of a month, with leading zeros*/` |
|   15 |  359 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|   15 |  360 | `			break;` |
|  ! 0 |  361 | `		case 'M':` |
|    - |  362 | `			/*A short textual representation of a month, three letters*/` |
|  ! 0 |  363 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|  ! 0 |  364 | `			ph7_result_string(pCtx,zCur,3);` |
|  ! 0 |  365 | `			break;` |
|  ! 0 |  366 | `		case 'n':` |
|    - |  367 | `			/*Numeric representation of a month, without leading zeros*/` |
|  ! 0 |  368 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|  ! 0 |  369 | `			break;` |
|  ! 0 |  370 | `		case 't':{` |
|    - |  371 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|  ! 0 |  372 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|  ! 0 |  373 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|  ! 0 |  374 | `				nDays = 28;` |
|  ! 0 |  375 | `			}` |
|    - |  376 | `			/*Number of days in the given month*/` |
|  ! 0 |  377 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|  ! 0 |  378 | `			break;` |
|    - |  379 | `				 }` |
|  ! 0 |  380 | `		case 'L':{` |
|  ! 0 |  381 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|    - |  382 | `			/* Whether it's a leap year */` |
|  ! 0 |  383 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|  ! 0 |  384 | `			break;` |
|    - |  385 | `				 }` |
|  ! 0 |  386 | `		case 'o':` |
|    - |  387 | `			/* ISO-8601 year number.*/` |
|  ! 0 |  388 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|  ! 0 |  389 | `			break;` |
|    9 |  390 | `		case 'Y':` |
|    - |  391 | `			/*	A full numeric representation of a year, 4 digits */` |
|   19 |  392 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|   19 |  393 | `			break;` |
|  ! 0 |  394 | `		case 'y':` |
|    - |  395 | `			/*A two digit representation of a year*/` |
|  ! 0 |  396 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|  ! 0 |  397 | `			break;` |
|  ! 0 |  398 | `		case 'a':` |
|    - |  399 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|  ! 0 |  400 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|  ! 0 |  401 | `			break;` |
|  ! 0 |  402 | `		case 'A':` |
|    - |  403 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|  ! 0 |  404 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|  ! 0 |  405 | `			break;` |
|  ! 0 |  406 | `		case 'g':` |
|    - |  407 | `			/*	12-hour format of an hour without leading zeros*/` |
|  ! 0 |  408 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|  ! 0 |  409 | `			break;` |
|  ! 0 |  410 | `		case 'G':` |
|    - |  411 | `			/* 24-hour format of an hour without leading zeros */` |
|  ! 0 |  412 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|  ! 0 |  413 | `			break;` |
|  ! 0 |  414 | `		case 'h':` |
|    - |  415 | `			/* 12-hour format of an hour with leading zeros */` |
|  ! 0 |  416 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|  ! 0 |  417 | `			break;` |
|    3 |  418 | `		case 'H':` |
|    - |  419 | `			/*	24-hour format of an hour with leading zeros */` |
|    7 |  420 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|    7 |  421 | `			break;` |
|    3 |  422 | `		case 'i':` |
|    - |  423 | `			/* 	Minutes with leading zeros */` |
|    7 |  424 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|    7 |  425 | `			break;` |
|    3 |  426 | `		case 's':` |
|    - |  427 | `			/* 	second with leading zeros */` |
|    7 |  428 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    7 |  429 | `			break;` |
|  ! 0 |  430 | `		case 'u':` |
|    - |  431 | `			/* 	Microseconds */` |
|  ! 0 |  432 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|  ! 0 |  433 | `			break;` |
|  ! 0 |  434 | `		case 'S':{` |
|    - |  435 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|    - |  436 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|  ! 0 |  437 | `			int v = pTm->tm_mday;` |
|  ! 0 |  438 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|  ! 0 |  439 | `			break;` |
|    - |  440 | `				 }` |
|  ! 0 |  441 | `		case 'e':` |
|    - |  442 | `			/* 	Timezone identifier */` |
|  ! 0 |  443 | `			zCur = pTm->tm_zone;` |
|  ! 0 |  444 | `			if( zCur == 0 ){` |
|    - |  445 | `				/* Assume GMT */` |
|  ! 0 |  446 | `				zCur = "GMT";` |
|  ! 0 |  447 | `			}` |
|  ! 0 |  448 | `			ph7_result_string(pCtx,zCur,-1);` |
|  ! 0 |  449 | `			break;` |
|  ! 0 |  450 | `		case 'I':` |
|    - |  451 | `			/* Whether or not the date is in daylight saving time */` |
|    - |  452 | `#ifdef __WINNT__` |
|    - |  453 | `#ifdef _MSC_VER` |
|    - |  454 | `#ifndef _WIN32_WCE` |
|  ! 0 |  455 | `			_get_daylight(&pTm->tm_isdst);` |
|    - |  456 | `#endif` |
|    - |  457 | `#endif` |
|    - |  458 | `#endif` |
|  ! 0 |  459 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|  ! 0 |  460 | `			break;` |
|  ! 0 |  461 | `		case 'r':` |
|    - |  462 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|  ! 0 |  463 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|  ! 0 |  464 | `				SyTimeGetDay(pTm->tm_wday),` |
|  ! 0 |  465 | `				pTm->tm_mday,` |
|  ! 0 |  466 | `				SyTimeGetMonth(pTm->tm_mon),` |
|  ! 0 |  467 | `				pTm->tm_year,` |
|  ! 0 |  468 | `				pTm->tm_hour,` |
|  ! 0 |  469 | `				pTm->tm_min,` |
|  ! 0 |  470 | `				pTm->tm_sec` |
|    - |  471 | `				);` |
|  ! 0 |  472 | `			break;` |
|  ! 0 |  473 | `		case 'U':{` |
|    - |  474 | `			time_t tt;` |
|    - |  475 | `			/* Seconds since the Unix Epoch */` |
|  ! 0 |  476 | `			time(&tt);` |
|  ! 0 |  477 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|  ! 0 |  478 | `			break;` |
|    - |  479 | `				 }` |
|  ! 0 |  480 | `		case 'O':` |
|    - |  481 | `		case 'P':` |
|    - |  482 | `			/* Difference to Greenwich time (GMT) in hours */` |
|  ! 0 |  483 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|  ! 0 |  484 | `			break;` |
|  ! 0 |  485 | `		case 'Z':` |
|    - |  486 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|    - |  487 | `			 * is always negative, and for those east of UTC is always positive.` |
|    - |  488 | `			 */` |
|  ! 0 |  489 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|  ! 0 |  490 | `			break;` |
|    1 |  491 | `		case 'c':` |
|    - |  492 | `			/* 	ISO 8601 date */` |
|    4 |  493 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|    1 |  494 | `				pTm->tm_year,` |
|    2 |  495 | `				pTm->tm_mon+1,` |
|    1 |  496 | `				pTm->tm_mday,` |
|    1 |  497 | `				pTm->tm_hour,` |
|    1 |  498 | `				pTm->tm_min,` |
|    1 |  499 | `				pTm->tm_sec,` |
|    1 |  500 | `				pTm->tm_gmtoff` |
|    - |  501 | `				);` |
|    3 |  502 | `			break;` |
|    1 |  503 | `		case '\\':` |
|    3 |  504 | `			zIn++;` |
|    - |  505 | `			/* Expand verbatim */` |
|    3 |  506 | `			if( zIn < zEnd ){` |
|    3 |  507 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|    1 |  508 | `			}` |
|    3 |  509 | `			break;` |
|   17 |  510 | `		default:` |
|    - |  511 | `			/* Unknown format specifer,expand verbatim */` |
|   35 |  512 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|   34 |  513 | `			break;` |
|    - |  514 | `		}` |
|    - |  515 | `		/* Point to the next character */` |
|  111 |  516 | `		zIn++;` |
|    1 |  517 | `	}` |
|   47 |  518 | `	return SXRET_OK;` |
|    1 |  519 |  |
|    - |  520 | `/*` |
|    - |  521 | ` * PH7 implementation of the strftime() function.` |
|    - |  522 | ` * The following formats are supported:` |
|    - |  523 | ` * %a 	An abbreviated textual representation of the day` |
|    - |  524 | ` * %A 	A full textual representation of the day` |
|    - |  525 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|    - |  526 | ` * %e 	Day of the month, with a space preceding single digits.` |
|    - |  527 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|    - |  528 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|    - |  529 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|    - |  530 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|    - |  531 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|    - |  532 | ` *   4 weekdays, with Monday being the start of the week.` |
|    - |  533 | ` * %W 	A numeric representation of the week of the year` |
|    - |  534 | ` * %b 	Abbreviated month name, based on the locale` |
|    - |  535 | ` * %B 	Full month name, based on the locale` |
|    - |  536 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|    - |  537 | ` * %m 	Two digit representation of the month` |
|    - |  538 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|    - |  539 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|    - |  540 | ` * %G 	The full four-digit version of %g` |
|    - |  541 | ` * %y 	Two digit representation of the year` |
|    - |  542 | ` * %Y 	Four digit representation for the year` |
|    - |  543 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|    - |  544 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|    - |  545 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|    - |  546 | ` * %M 	Two digit representation of the minute` |
|    - |  547 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|    - |  548 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|    - |  549 | ` * %r 	Same as "%I:%M:%S %p"` |
|    - |  550 | ` * %R 	Same as "%H:%M"` |
|    - |  551 | ` * %S 	Two digit representation of the second` |
|    - |  552 | ` * %T 	Same as "%H:%M:%S"` |
|    - |  553 | ` * %X 	Preferred time representation based on locale, without the date` |
|    - |  554 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|    - |  555 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|    - |  556 | ` * %c 	Preferred date and time stamp based on local` |
|    - |  557 | ` * %D 	Same as "%m/%d/%y"` |
|    - |  558 | ` * %F 	Same as "%Y-%m-%d"` |
|    - |  559 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|    - |  560 | ` * %x 	Preferred date representation based on locale, without the time` |
|    - |  561 | ` * %n 	A newline character ("\n")` |
|    - |  562 | ` * %t 	A Tab character ("\t")` |
|    - |  563 | ` * %% 	A literal percentage character ("%")` |
|    - |  564 | ` */` |
|   16 |  565 | `static int PH7_Strftime(` |
|    - |  566 | `	ph7_context *pCtx,  /* Call context */` |
|    - |  567 | `	const char *zIn,    /* Input string */` |
|    - |  568 | `	int nLen,           /* Input length */` |
|    - |  569 | `	Sytm *pTm           /* Parse of the given time */` |
|    - |  570 | `	)` |
|    1 |  571 |  |
|   17 |  572 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|    - |  573 | `	int c;` |
|    - |  574 | `	/* Start the format process */` |
|   18 |  575 | `	for(;;){` |
|   37 |  576 | `		zCur = zIn;` |
|   41 |  577 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|    5 |  578 | `			zIn++;` |
|    1 |  579 | `		}` |
|   37 |  580 | `		if( zIn > zCur ){` |
|    - |  581 | `			/* Consume input verbatim */` |
|    5 |  582 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|    2 |  583 | `		}` |
|   37 |  584 | `		zIn++; /* Jump the percent sign */` |
|   37 |  585 | `		if( zIn >= zEnd ){` |
|    - |  586 | `			/* No more input to process */` |
|   17 |  587 | `			break;` |
|    - |  588 | `		}` |
|   21 |  589 | `		c = zIn[0];` |
|    - |  590 | `		/* Act according to the current specifer */` |
|   21 |  591 | `		switch(c){` |
|  ! 0 |  592 | `		case '%':` |
|    - |  593 | `			/* A literal percentage character ("%") */` |
|  ! 0 |  594 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|  ! 0 |  595 | `			break;` |
|  ! 0 |  596 | `		case 't':` |
|    - |  597 | `			/* A Tab character */` |
|  ! 0 |  598 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|  ! 0 |  599 | `			break;` |
|  ! 0 |  600 | `		case 'n':` |
|    - |  601 | `			/* A newline character */` |
|  ! 0 |  602 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|  ! 0 |  603 | `			break;` |
|    1 |  604 | `		case 'a':` |
|    - |  605 | `			/* An abbreviated textual representation of the day */` |
|    3 |  606 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|    3 |  607 | `			break;` |
|  ! 0 |  608 | `		case 'A':` |
|    - |  609 | `			/* A full textual representation of the day */` |
|  ! 0 |  610 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|  ! 0 |  611 | `			break;` |
|  ! 0 |  612 | `		case 'e':` |
|    - |  613 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|  ! 0 |  614 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|  ! 0 |  615 | `			break;` |
|    2 |  616 | `		case 'd':` |
|    - |  617 | `			/* Two-digit day of the month (with leading zeros) */` |
|    5 |  618 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|    5 |  619 | `			break;` |
|  ! 0 |  620 | `		case 'j':` |
|    - |  621 | `			/*The day of the year,3 digits with leading zeros*/` |
|  ! 0 |  622 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|  ! 0 |  623 | `			break;` |
|  ! 0 |  624 | `		case 'u':` |
|    - |  625 | `			/* ISO-8601 numeric representation of the day of the week */` |
|  ! 0 |  626 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|  ! 0 |  627 | `			break;` |
|  ! 0 |  628 | `		case 'w':` |
|    - |  629 | `			/* Numeric representation of the day of the week */` |
|  ! 0 |  630 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|  ! 0 |  631 | `			break;` |
|  ! 0 |  632 | `		case 'b':` |
|    - |  633 | `		case 'h':` |
|    - |  634 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|  ! 0 |  635 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|  ! 0 |  636 | `			break;` |
|  ! 0 |  637 | `		case 'B':` |
|    - |  638 | `			/* Full month name (Not based on locale) */` |
|  ! 0 |  639 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|  ! 0 |  640 | `			break;` |
|    2 |  641 | `		case 'm':` |
|    - |  642 | `			/*Numeric representation of a month, with leading zeros*/` |
|    5 |  643 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|    5 |  644 | `			break;` |
|  ! 0 |  645 | `		case 'C':` |
|    - |  646 | `			/* Two digit representation of the century */` |
|  ! 0 |  647 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|  ! 0 |  648 | `			break;` |
|  ! 0 |  649 | `		case 'y':` |
|    - |  650 | `		case 'g':` |
|    - |  651 | `			/* Two digit representation of the year */` |
|  ! 0 |  652 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|  ! 0 |  653 | `			break;` |
|    2 |  654 | `		case 'Y':` |
|    - |  655 | `		case 'G':` |
|    - |  656 | `			/* Four digit representation of the year */` |
|    5 |  657 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    5 |  658 | `			break;` |
|  ! 0 |  659 | `		case 'I':` |
|    - |  660 | `			/* 12-hour format of an hour with leading zeros */` |
|  ! 0 |  661 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|  ! 0 |  662 | `			break;` |
|  ! 0 |  663 | `		case 'l':` |
|    - |  664 | `			/* 12-hour format of an hour with leading space */` |
|  ! 0 |  665 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|  ! 0 |  666 | `			break;` |
|    1 |  667 | `		case 'H':` |
|    - |  668 | `			/* 24-hour format of an hour with leading zeros */` |
|    3 |  669 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|    3 |  670 | `			break;` |
|    1 |  671 | `		case 'M':` |
|    - |  672 | `			/* Minutes with leading zeros */` |
|    3 |  673 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|    3 |  674 | `			break;` |
|  ! 0 |  675 | `		case 'S':` |
|    - |  676 | `			/* Seconds with leading zeros */` |
|  ! 0 |  677 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|  ! 0 |  678 | `			break;` |
|  ! 0 |  679 | `		case 'z':` |
|    - |  680 | `		case 'Z':` |
|    - |  681 | `			/* 	Timezone identifier */` |
|  ! 0 |  682 | `			zCur = pTm->tm_zone;` |
|  ! 0 |  683 | `			if( zCur == 0 ){` |
|    - |  684 | `				/* Assume GMT */` |
|  ! 0 |  685 | `				zCur = "GMT";` |
|  ! 0 |  686 | `			}` |
|  ! 0 |  687 | `			ph7_result_string(pCtx,zCur,-1);` |
|  ! 0 |  688 | `			break;` |
|  ! 0 |  689 | `		case 'T':` |
|    - |  690 | `		case 'X':` |
|    - |  691 | `			/* Same as "%H:%M:%S" */` |
|  ! 0 |  692 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|  ! 0 |  693 | `			break;` |
|  ! 0 |  694 | `		case 'R':` |
|    - |  695 | `			/* Same as "%H:%M" */` |
|  ! 0 |  696 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|  ! 0 |  697 | `			break;` |
|  ! 0 |  698 | `		case 'P':` |
|    - |  699 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|  ! 0 |  700 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|  ! 0 |  701 | `			break;` |
|  ! 0 |  702 | `		case 'p':` |
|    - |  703 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|  ! 0 |  704 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|  ! 0 |  705 | `			break;` |
|  ! 0 |  706 | `		case 'r':` |
|    - |  707 | `			/* Same as "%I:%M:%S %p" */` |
|  ! 0 |  708 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|  ! 0 |  709 | `				1+(pTm->tm_hour%12),` |
|  ! 0 |  710 | `				pTm->tm_min,` |
|  ! 0 |  711 | `				pTm->tm_sec,` |
|  ! 0 |  712 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|    - |  713 | `				);` |
|  ! 0 |  714 | `			break;` |
|    1 |  715 | `		case 'D':` |
|    - |  716 | `		case 'x':` |
|    - |  717 | `			/* Same as "%m/%d/%y" */` |
|    4 |  718 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|    2 |  719 | `				pTm->tm_mon+1,` |
|    1 |  720 | `				pTm->tm_mday,` |
|    2 |  721 | `				pTm->tm_year%100` |
|    - |  722 | `				);` |
|    3 |  723 | `			break;` |
|  ! 0 |  724 | `		case 'F':` |
|    - |  725 | `			/* Same as "%Y-%m-%d" */` |
|  ! 0 |  726 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|  ! 0 |  727 | `				pTm->tm_year,` |
|  ! 0 |  728 | `				pTm->tm_mon+1,` |
|  ! 0 |  729 | `				pTm->tm_mday` |
|    - |  730 | `				);` |
|  ! 0 |  731 | `			break;` |
|  ! 0 |  732 | `		case 'c':` |
|  ! 0 |  733 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|  ! 0 |  734 | `				pTm->tm_year,` |
|  ! 0 |  735 | `				pTm->tm_mon+1,` |
|  ! 0 |  736 | `				pTm->tm_mday,` |
|  ! 0 |  737 | `				pTm->tm_hour,` |
|  ! 0 |  738 | `				pTm->tm_min,` |
|  ! 0 |  739 | `				pTm->tm_sec` |
|    - |  740 | `				);` |
|  ! 0 |  741 | `			break;` |
|  ! 0 |  742 | `		case 's':{` |
|    - |  743 | `			time_t tt;` |
|    - |  744 | `			/* Seconds since the Unix Epoch */` |
|  ! 0 |  745 | `			time(&tt);` |
|  ! 0 |  746 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|  ! 0 |  747 | `			break;` |
|    - |  748 | `				 }` |
|  ! 0 |  749 | `		default:` |
|    - |  750 | `			/* unknown specifer,simply ignore*/` |
|  ! 0 |  751 | `			break;` |
|    - |  752 | `		}` |
|    - |  753 | `		/* Advance the cursor */` |
|   21 |  754 | `		zIn++;` |
|    1 |  755 | `	}` |
|   17 |  756 | `	return SXRET_OK;` |
|    1 |  757 |  |
|    - |  758 | `/*` |
|    - |  759 | ` * string date(string $format [, int $timestamp = time() ] )` |
|    - |  760 | ` *  Returns a string formatted according to the given format string using` |
|    - |  761 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|    - |  762 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|    - |  763 | ` * Parameters` |
|    - |  764 | ` *  $format` |
|    - |  765 | ` *   The format of the outputted date string (See code above)` |
|    - |  766 | ` * $timestamp` |
|    - |  767 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  768 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  769 | ` *   In other words, it defaults to the value of time().` |
|    - |  770 | ` * Return` |
|    - |  771 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  772 | ` */` |
|   36 |  773 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  774 |  |
|    - |  775 | `	const char *zFormat;` |
|    - |  776 | `	int nLen;` |
|    - |  777 | `	Sytm sTm;` |
|   37 |  778 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  779 | `		/* Missing/Invalid argument,return FALSE */` |
|    5 |  780 | `		ph7_result_bool(pCtx,0);` |
|    5 |  781 | `		return PH7_OK;` |
|    - |  782 | `	}` |
|   33 |  783 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   33 |  784 | `	if( nLen < 1 ){` |
|    - |  785 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  786 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  787 | `	}` |
|   33 |  788 | `	if( nArg < 2 ){` |
|    - |  789 | `#ifdef __WINNT__` |
|    - |  790 | `		SYSTEMTIME sOS;` |
|    1 |  791 | `		GetSystemTime(&sOS);` |
|    1 |  792 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  793 | `#else` |
|    - |  794 | `		struct tm *pTm;` |
|    - |  795 | `		time_t t;` |
|   30 |  796 | `		time(&t);` |
|   30 |  797 | `		pTm = localtime(&t);` |
|   30 |  798 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  799 | `#endif` |
|   16 |  800 | `	}else{` |
|    - |  801 | `		/* Use the given timestamp */` |
|    - |  802 | `		time_t t;` |
|    - |  803 | `		struct tm *pTm;` |
|    3 |  804 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    3 |  805 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    3 |  806 | `			pTm = localtime(&t);` |
|    3 |  807 | `			if( pTm == 0 ){` |
|  ! 0 |  808 | `				time(&t);` |
|  ! 0 |  809 | `			}` |
|    2 |  810 | `		}else{` |
|  ! 0 |  811 | `			time(&t);` |
|    - |  812 | `		}` |
|    3 |  813 | `		pTm = localtime(&t);` |
|    3 |  814 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  815 | `	}` |
|    - |  816 | `	/* Format the given string */` |
|   33 |  817 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|   33 |  818 | `	return PH7_OK;` |
|   19 |  819 |  |
|    - |  820 | `/*` |
|    - |  821 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|    - |  822 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|    - |  823 | ` * Parameters` |
|    - |  824 | ` *  $format` |
|    - |  825 | ` *   The format of the outputted date string (See code above)` |
|    - |  826 | ` * $timestamp` |
|    - |  827 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  828 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  829 | ` *   In other words, it defaults to the value of time().` |
|    - |  830 | ` * Return` |
|    - |  831 | ` * Returns a string formatted according format using the given timestamp` |
|    - |  832 | ` * or the current local time if no timestamp is given.` |
|    - |  833 | ` */` |
|   20 |  834 | `PH7_PRIVATE int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  835 |  |
|    - |  836 | `	const char *zFormat;` |
|    - |  837 | `	int nLen;` |
|    - |  838 | `	Sytm sTm;` |
|   21 |  839 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  840 | `		/* Missing/Invalid argument,return FALSE */` |
|    5 |  841 | `		ph7_result_bool(pCtx,0);` |
|    5 |  842 | `		return PH7_OK;` |
|    - |  843 | `	}` |
|   17 |  844 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   17 |  845 | `	if( nLen < 1 ){` |
|    - |  846 | `		/* Don't bother processing return FALSE */` |
|  ! 0 |  847 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  848 | `	}` |
|   17 |  849 | `	if( nArg < 2 ){` |
|    - |  850 | `#ifdef __WINNT__` |
|    - |  851 | `		SYSTEMTIME sOS;` |
|    1 |  852 | `		GetSystemTime(&sOS);` |
|    1 |  853 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  854 | `#else` |
|    - |  855 | `		struct tm *pTm;` |
|    - |  856 | `		time_t t;` |
|   14 |  857 | `		time(&t);` |
|   14 |  858 | `		pTm = localtime(&t);` |
|   14 |  859 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  860 | `#endif` |
|    8 |  861 | `	}else{` |
|    - |  862 | `		/* Use the given timestamp */` |
|    - |  863 | `		time_t t;` |
|    - |  864 | `		struct tm *pTm;` |
|    3 |  865 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    3 |  866 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    3 |  867 | `			pTm = localtime(&t);` |
|    3 |  868 | `			if( pTm == 0 ){` |
|  ! 0 |  869 | `				time(&t);` |
|  ! 0 |  870 | `			}` |
|    2 |  871 | `		}else{` |
|  ! 0 |  872 | `			time(&t);` |
|    - |  873 | `		}` |
|    3 |  874 | `		pTm = localtime(&t);` |
|    3 |  875 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  876 | `	}` |
|    - |  877 | `	/* Format the given string */` |
|   17 |  878 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|   17 |  879 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|    - |  880 | `		/* Nothing was formatted,return FALSE */` |
|  ! 0 |  881 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  882 | `	}` |
|   17 |  883 | `	return PH7_OK;` |
|   11 |  884 |  |
|    - |  885 | `/*` |
|    - |  886 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|    - |  887 | ` *  Identical to the date() function except that the time returned` |
|    - |  888 | ` *  is Greenwich Mean Time (GMT).` |
|    - |  889 | ` * Parameters` |
|    - |  890 | ` *  $format` |
|    - |  891 | ` *  The format of the outputted date string (See code above)` |
|    - |  892 | ` *  $timestamp` |
|    - |  893 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|    - |  894 | ` *   that defaults to the current local time if a timestamp is not given.` |
|    - |  895 | ` *   In other words, it defaults to the value of time().` |
|    - |  896 | ` * Return` |
|    - |  897 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|    - |  898 | ` */` |
|   16 |  899 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  900 |  |
|    - |  901 | `	const char *zFormat;` |
|    - |  902 | `	int nLen;` |
|    - |  903 | `	Sytm sTm;` |
|   17 |  904 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  905 | `		/* Missing/Invalid argument,return FALSE */` |
|    3 |  906 | `		ph7_result_bool(pCtx,0);` |
|    3 |  907 | `		return PH7_OK;` |
|    - |  908 | `	}` |
|   15 |  909 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   15 |  910 | `	if( nLen < 1 ){` |
|    - |  911 | `		/* Don't bother processing return the empty string */` |
|  ! 0 |  912 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  913 | `	}` |
|   15 |  914 | `	if( nArg < 2 ){` |
|    - |  915 | `#ifdef __WINNT__` |
|    - |  916 | `		SYSTEMTIME sOS;` |
|    1 |  917 | `		GetSystemTime(&sOS);` |
|    1 |  918 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  919 | `#else` |
|    - |  920 | `		struct tm *pTm;` |
|    - |  921 | `		time_t t;` |
|   12 |  922 | `		time(&t);` |
|   12 |  923 | `		pTm = gmtime(&t);` |
|   12 |  924 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  925 | `#endif` |
|    7 |  926 | `	}else{` |
|    - |  927 | `		/* Use the given timestamp */` |
|    - |  928 | `		time_t t;` |
|    - |  929 | `		struct tm *pTm;` |
|    3 |  930 | `		if( ph7_value_is_int(apArg[1]) ){` |
|    3 |  931 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|    3 |  932 | `			pTm = gmtime(&t);` |
|    3 |  933 | `			if( pTm == 0 ){` |
|  ! 0 |  934 | `				time(&t);` |
|  ! 0 |  935 | `			}` |
|    2 |  936 | `		}else{` |
|  ! 0 |  937 | `			time(&t);` |
|    - |  938 | `		}` |
|    3 |  939 | `		pTm = gmtime(&t);` |
|    3 |  940 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  941 | `	}` |
|    - |  942 | `	/* Format the given string */` |
|   15 |  943 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|   15 |  944 | `	return PH7_OK;` |
|    9 |  945 |  |
|    - |  946 | `/*` |
|    - |  947 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|    - |  948 | ` *  Return the local time.` |
|    - |  949 | ` * Parameter` |
|    - |  950 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|    - |  951 | ` *     that defaults to the current local time if a timestamp is not given.` |
|    - |  952 | ` *     In other words, it defaults to the value of time().` |
|    - |  953 | ` * $is_associative` |
|    - |  954 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|    - |  955 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|    - |  956 | ` *   array containing all the different elements of the structure returned by the C function` |
|    - |  957 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|    - |  958 | ` *      "tm_sec" - seconds, 0 to 59` |
|    - |  959 | ` *      "tm_min" - minutes, 0 to 59` |
|    - |  960 | ` *      "tm_hour" - hours, 0 to 23` |
|    - |  961 | ` *      "tm_mday" - day of the month, 1 to 31` |
|    - |  962 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|    - |  963 | ` *      "tm_year" - years since 1900` |
|    - |  964 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|    - |  965 | ` *      "tm_yday" - day of the year, 0 to 365` |
|    - |  966 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|    - |  967 | ` * Returns` |
|    - |  968 | ` *  An associative array of information related to the timestamp.` |
|    - |  969 | ` */` |
|    8 |  970 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  971 |  |
|    - |  972 | `	ph7_value *pValue,*pArray;` |
|    9 |  973 | `	int isAssoc = 0;` |
|    - |  974 | `	Sytm sTm;` |
|    9 |  975 | `	if( nArg < 1 ){` |
|    - |  976 | `#ifdef __WINNT__` |
|    - |  977 | `		SYSTEMTIME sOS;` |
|    1 |  978 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|    1 |  979 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - |  980 | `#else` |
|    - |  981 | `		struct tm *pTm;` |
|    - |  982 | `		time_t t;` |
|    4 |  983 | `		time(&t);` |
|    4 |  984 | `		pTm = localtime(&t);` |
|    4 |  985 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - |  986 | `#endif` |
|    3 |  987 | `	}else{` |
|    - |  988 | `		/* Use the given timestamp */` |
|    - |  989 | `		time_t t;` |
|    - |  990 | `		struct tm *pTm;` |
|    5 |  991 | `		if( ph7_value_is_int(apArg[0]) ){` |
|    5 |  992 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|    5 |  993 | `			pTm = localtime(&t);` |
|    5 |  994 | `			if( pTm == 0 ){` |
|  ! 0 |  995 | `				time(&t);` |
|  ! 0 |  996 | `			}` |
|    3 |  997 | `		}else{` |
|  ! 0 |  998 | `			time(&t);` |
|    - |  999 | `		}` |
|    5 | 1000 | `		pTm = localtime(&t);` |
|    5 | 1001 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1002 | `	}` |
|    - | 1003 | `	/* Element value */` |
|    9 | 1004 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    9 | 1005 | `	if( pValue == 0 ){` |
|    - | 1006 | `		/* Return NULL */` |
|  ! 0 | 1007 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1008 | `		return PH7_OK;` |
|    - | 1009 | `	}` |
|    - | 1010 | `	/* Create a new array */` |
|    9 | 1011 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 | 1012 | `	if( pArray == 0 ){` |
|    - | 1013 | `		/* Return NULL */` |
|  ! 0 | 1014 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1015 | `		return PH7_OK;` |
|    - | 1016 | `	}` |
|    9 | 1017 | `	if( nArg > 1 ){` |
|    3 | 1018 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|    1 | 1019 | `	}` |
|    - | 1020 | `	/* Fill the array */` |
|    - | 1021 | `	/* Seconds */` |
|    9 | 1022 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|    9 | 1023 | `	if( isAssoc ){` |
|    3 | 1024 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|    2 | 1025 | `	}else{` |
|    7 | 1026 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1027 | `	}` |
|    - | 1028 | `	/* Minutes */` |
|    9 | 1029 | `	ph7_value_int(pValue,sTm.tm_min);` |
|    9 | 1030 | `	if( isAssoc ){` |
|    3 | 1031 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|    2 | 1032 | `	}else{` |
|    7 | 1033 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1034 | `	}` |
|    - | 1035 | `	/* Hours */` |
|    9 | 1036 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|    9 | 1037 | `	if( isAssoc ){` |
|    3 | 1038 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|    2 | 1039 | `	}else{` |
|    7 | 1040 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1041 | `	}` |
|    - | 1042 | `	/* mday */` |
|    9 | 1043 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|    9 | 1044 | `	if( isAssoc ){` |
|    3 | 1045 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|    2 | 1046 | `	}else{` |
|    7 | 1047 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1048 | `	}` |
|    - | 1049 | `	/* mon */` |
|    9 | 1050 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|    9 | 1051 | `	if( isAssoc ){` |
|    3 | 1052 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|    2 | 1053 | `	}else{` |
|    7 | 1054 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1055 | `	}` |
|    - | 1056 | `	/* year since 1900 */` |
|    9 | 1057 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|    9 | 1058 | `	if( isAssoc ){` |
|    3 | 1059 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|    2 | 1060 | `	}else{` |
|    7 | 1061 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1062 | `	}` |
|    - | 1063 | `	/* wday */` |
|    9 | 1064 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|    9 | 1065 | `	if( isAssoc ){` |
|    3 | 1066 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|    2 | 1067 | `	}else{` |
|    7 | 1068 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1069 | `	}` |
|    - | 1070 | `	/* yday */` |
|    9 | 1071 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|    9 | 1072 | `	if( isAssoc ){` |
|    3 | 1073 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|    2 | 1074 | `	}else{` |
|    7 | 1075 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1076 | `	}` |
|    - | 1077 | `	/* isdst */` |
|    - | 1078 | `#ifdef __WINNT__` |
|    - | 1079 | `#ifdef _MSC_VER` |
|    - | 1080 | `#ifndef _WIN32_WCE` |
|    1 | 1081 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1082 | `#endif` |
|    - | 1083 | `#endif` |
|    - | 1084 | `#endif` |
|    9 | 1085 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|    9 | 1086 | `	if( isAssoc ){` |
|    3 | 1087 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|    2 | 1088 | `	}else{` |
|    7 | 1089 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|    - | 1090 | `	}` |
|    - | 1091 | `	/* Return the array */` |
|    9 | 1092 | `	ph7_result_value(pCtx,pArray);` |
|    9 | 1093 | `	return PH7_OK;` |
|    5 | 1094 |  |
|    - | 1095 | `/*` |
|    - | 1096 | ` * int idate(string $format [, int $timestamp = time() ])` |
|    - | 1097 | ` *  Returns a number formatted according to the given format string` |
|    - | 1098 | ` *  using the given integer timestamp or the current local time if` |
|    - | 1099 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|    - | 1100 | ` *  to the value of time().` |
|    - | 1101 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|    - | 1102 | ` *  parameter.` |
|    - | 1103 | ` * $Parameters` |
|    - | 1104 | ` *  Supported format` |
|    - | 1105 | ` *   d 	Day of the month` |
|    - | 1106 | ` *   h 	Hour (12 hour format)` |
|    - | 1107 | ` *   H 	Hour (24 hour format)` |
|    - | 1108 | ` *   i 	Minutes` |
|    - | 1109 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|    - | 1110 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|    - | 1111 | ` *   m 	Month number` |
|    - | 1112 | ` *   s 	Seconds` |
|    - | 1113 | ` *   t 	Days in current month` |
|    - | 1114 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|    - | 1115 | ` *   w 	Day of the week (0 on Sunday)` |
|    - | 1116 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|    - | 1117 | ` *   y 	Year (1 or 2 digits - check note below)` |
|    - | 1118 | ` *   Y 	Year (4 digits)` |
|    - | 1119 | ` *   z 	Day of the year` |
|    - | 1120 | ` *   Z 	Timezone offset in seconds` |
|    - | 1121 | ` * $timestamp` |
|    - | 1122 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|    - | 1123 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|    - | 1124 | ` *  to the value of time().` |
|    - | 1125 | ` * Return` |
|    - | 1126 | ` *  An integer.` |
|    - | 1127 | ` */` |
|   42 | 1128 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1129 |  |
|    - | 1130 | `	const char *zFormat;` |
|   44 | 1131 | `	ph7_int64 iVal = 0;` |
|    - | 1132 | `	int nLen;` |
|    - | 1133 | `	Sytm sTm;` |
|   44 | 1134 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1135 | `		/* Missing/Invalid argument,return -1 */` |
|    5 | 1136 | `		ph7_result_int(pCtx,-1);` |
|    5 | 1137 | `		return PH7_OK;` |
|    - | 1138 | `	}` |
|   40 | 1139 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   40 | 1140 | `	if( nLen < 1 ){` |
|    - | 1141 | `		/* Don't bother processing return -1*/` |
|  ! 0 | 1142 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1143 | `	}` |
|   40 | 1144 | `	if( nArg < 2 ){` |
|    - | 1145 | `#ifdef __WINNT__` |
|    - | 1146 | `		SYSTEMTIME sOS;` |
|    2 | 1147 | `		GetSystemTime(&sOS);` |
|    2 | 1148 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|    - | 1149 | `#else` |
|    - | 1150 | `		struct tm *pTm;` |
|    - | 1151 | `		time_t t;` |
|   28 | 1152 | `		time(&t);` |
|   28 | 1153 | `		pTm = localtime(&t);` |
|   28 | 1154 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1155 | `#endif` |
|   16 | 1156 | `	}else{` |
|    - | 1157 | `		/* Use the given timestamp */` |
|    - | 1158 | `		time_t t;` |
|    - | 1159 | `		struct tm *pTm;` |
|   11 | 1160 | `		if( ph7_value_is_int(apArg[1]) ){` |
|   11 | 1161 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|   11 | 1162 | `			pTm = localtime(&t);` |
|   11 | 1163 | `			if( pTm == 0 ){` |
|  ! 0 | 1164 | `				time(&t);` |
|  ! 0 | 1165 | `			}` |
|    6 | 1166 | `		}else{` |
|  ! 0 | 1167 | `			time(&t);` |
|    - | 1168 | `		}` |
|   11 | 1169 | `		pTm = localtime(&t);` |
|   11 | 1170 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|    - | 1171 | `	}` |
|    - | 1172 | `	/* Perform the requested operation */` |
|   40 | 1173 | `	switch(zFormat[0]){` |
|    2 | 1174 | `	case 'd':` |
|    - | 1175 | `		/* Day of the month */` |
|    5 | 1176 | `		iVal = sTm.tm_mday;` |
|    5 | 1177 | `		break;` |
|  ! 0 | 1178 | `	case 'h':` |
|    - | 1179 | `		/*	Hour (12 hour format)*/` |
|  ! 0 | 1180 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|  ! 0 | 1181 | `		break;` |
|    1 | 1182 | `	case 'H':` |
|    - | 1183 | `		/* Hour (24 hour format)*/` |
|    3 | 1184 | `		iVal = sTm.tm_hour;` |
|    3 | 1185 | `		break;` |
|    1 | 1186 | `	case 'i':` |
|    - | 1187 | `		/*Minutes*/` |
|    3 | 1188 | `		iVal = sTm.tm_min;` |
|    3 | 1189 | `		break;` |
|    1 | 1190 | `	case 'I':` |
|    - | 1191 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|    - | 1192 | `#ifdef __WINNT__` |
|    - | 1193 | `#ifdef _MSC_VER` |
|    - | 1194 | `#ifndef _WIN32_WCE` |
|    1 | 1195 | `			_get_daylight(&sTm.tm_isdst);` |
|    - | 1196 | `#endif` |
|    - | 1197 | `#endif` |
|    - | 1198 | `#endif` |
|    3 | 1199 | `		iVal = sTm.tm_isdst;` |
|    3 | 1200 | `		break;` |
|    1 | 1201 | `	case 'L':` |
|    - | 1202 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|    3 | 1203 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|    3 | 1204 | `		break;` |
|    2 | 1205 | `	case 'm':` |
|    - | 1206 | `		/* Month number*/` |
|    5 | 1207 | `		iVal = sTm.tm_mon;` |
|    5 | 1208 | `		break;` |
|    1 | 1209 | `	case 's':` |
|    - | 1210 | `		/*Seconds*/` |
|    3 | 1211 | `		iVal = sTm.tm_sec;` |
|    3 | 1212 | `		break;` |
|    1 | 1213 | `	case 't':{` |
|    - | 1214 | `		/*Days in current month*/` |
|    - | 1215 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    3 | 1216 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|    3 | 1217 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|  ! 0 | 1218 | `			nDays = 28;` |
|  ! 0 | 1219 | `		}` |
|    3 | 1220 | `		iVal = nDays;` |
|    3 | 1221 | `		break;` |
|    - | 1222 | `			 }` |
|    1 | 1223 | `	case 'U':` |
|    - | 1224 | `		/*Seconds since the Unix Epoch*/` |
|    3 | 1225 | `		iVal = (ph7_int64)time(0);` |
|    3 | 1226 | `		break;` |
|    1 | 1227 | `	case 'w':` |
|    - | 1228 | `		/*	Day of the week (0 on Sunday) */` |
|    3 | 1229 | `		iVal = sTm.tm_wday;` |
|    3 | 1230 | `		break;` |
|    1 | 1231 | `	case 'W': {` |
|    - | 1232 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|    - | 1233 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|    3 | 1234 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|    3 | 1235 | `		break;` |
|    - | 1236 | `			  }` |
|  ! 0 | 1237 | `	case 'y':` |
|    - | 1238 | `		/* Year (2 digits) */` |
|  ! 0 | 1239 | `		iVal = sTm.tm_year % 100;` |
|  ! 0 | 1240 | `		break;` |
|    3 | 1241 | `	case 'Y':` |
|    - | 1242 | `		/* Year (4 digits) */` |
|    7 | 1243 | `		iVal = sTm.tm_year;` |
|    7 | 1244 | `		break;` |
|    1 | 1245 | `	case 'z':` |
|    - | 1246 | `		/* Day of the year */` |
|    3 | 1247 | `		iVal = sTm.tm_yday;` |
|    3 | 1248 | `		break;` |
|    1 | 1249 | `	case 'Z':` |
|    - | 1250 | `		/*Timezone offset in seconds*/` |
|    3 | 1251 | `		iVal = sTm.tm_gmtoff;` |
|    3 | 1252 | `		break;` |
|    1 | 1253 | `	default:` |
|    - | 1254 | `		/* unknown format,throw a warning */` |
|    3 | 1255 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|    2 | 1256 | `		break;` |
|    - | 1257 | `	}` |
|    - | 1258 | `	/* Return the time value */` |
|   40 | 1259 | `	ph7_result_int64(pCtx,iVal);` |
|   40 | 1260 | `	return PH7_OK;` |
|   23 | 1261 |  |
|    - | 1262 | `/*` |
|    - | 1263 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|    - | 1264 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|    - | 1265 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|    - | 1266 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|    - | 1267 | ` *  specified.` |
|    - | 1268 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|    - | 1269 | ` *  the current value according to the local date and time.` |
|    - | 1270 | ` * Parameters` |
|    - | 1271 | ` * $hour` |
|    - | 1272 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|    - | 1273 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|    - | 1274 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|    - | 1275 | ` * $minute` |
|    - | 1276 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|    - | 1277 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|    - | 1278 | ` *  in the following hour(s).` |
|    - | 1279 | ` * $second` |
|    - | 1280 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|    - | 1281 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|    - | 1282 | ` * second in the following minute(s).` |
|    - | 1283 | ` * $month` |
|    - | 1284 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|    - | 1285 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|    - | 1286 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|    - | 1287 | ` * $day` |
|    - | 1288 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|    - | 1289 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|    - | 1290 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|    - | 1291 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|    - | 1292 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|    - | 1293 | ` * $year` |
|    - | 1294 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|    - | 1295 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|    - | 1296 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|    - | 1297 | ` * $is_dst` |
|    - | 1298 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|    - | 1299 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|    - | 1300 | ` * Return` |
|    - | 1301 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|    - | 1302 | ` *   If the arguments are invalid, the function returns FALSE` |
|    - | 1303 | ` */` |
|    8 | 1304 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1305 |  |
|    - | 1306 | `	const char *zFunction;` |
|    9 | 1307 | `	ph7_int64 iVal = 0;` |
|    - | 1308 | `	struct tm *pTm;` |
|    - | 1309 | `	time_t t;` |
|    - | 1310 | `	/* Extract function name */` |
|    9 | 1311 | `	zFunction = ph7_function_name(pCtx);` |
|    - | 1312 | `	/* Get the current time */` |
|    9 | 1313 | `	time(&t);` |
|    9 | 1314 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|    3 | 1315 | `		pTm = gmtime(&t);` |
|    2 | 1316 | `	}else{` |
|    - | 1317 | `		/* localtime */` |
|    7 | 1318 | `		pTm = localtime(&t);` |
|    - | 1319 | `	}` |
|    9 | 1320 | `	if( nArg > 0 ){` |
|    - | 1321 | `		int iTmp;` |
|    - | 1322 | `		/* Hour */` |
|    9 | 1323 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|    9 | 1324 | `		pTm->tm_hour = iTmp;` |
|    9 | 1325 | `		if( nArg > 1 ){` |
|    - | 1326 | `			/* Minutes */` |
|    9 | 1327 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|    9 | 1328 | `			pTm->tm_min = iTmp;` |
|    9 | 1329 | `			if( nArg > 2 ){` |
|    - | 1330 | `				/* Seconds */` |
|    9 | 1331 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|    9 | 1332 | `				pTm->tm_sec = iTmp;` |
|    9 | 1333 | `				if( nArg > 3 ){` |
|    - | 1334 | `					/* Month */` |
|    9 | 1335 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|    9 | 1336 | `					pTm->tm_mon = iTmp - 1;` |
|    9 | 1337 | `					if( nArg > 4 ){` |
|    - | 1338 | `						/* mday */` |
|    9 | 1339 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|    9 | 1340 | `						pTm->tm_mday = iTmp;` |
|    9 | 1341 | `						if( nArg > 5 ){` |
|    - | 1342 | `							/* Year */` |
|    9 | 1343 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|    9 | 1344 | `							if( iTmp > 1900 ){` |
|    9 | 1345 | `								iTmp -= 1900;` |
|    4 | 1346 | `							}` |
|    9 | 1347 | `							pTm->tm_year = iTmp;` |
|    9 | 1348 | `							if( nArg > 6 ){` |
|    - | 1349 | `								/* is_dst */` |
|  ! 0 | 1350 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|  ! 0 | 1351 | `								pTm->tm_isdst = iTmp;` |
|  ! 0 | 1352 | `							}` |
|    4 | 1353 | `						}` |
|    4 | 1354 | `					}` |
|    4 | 1355 | `				}` |
|    4 | 1356 | `			}` |
|    4 | 1357 | `		}` |
|    4 | 1358 | `	}` |
|    - | 1359 | `	/* Make the time */` |
|    9 | 1360 | `	iVal = (ph7_int64)mktime(pTm);` |
|    - | 1361 | `	/* Return the timesatmp as a 64bit integer */` |
|    9 | 1362 | `	ph7_result_int64(pCtx,iVal);` |
|    9 | 1363 | `	return PH7_OK;` |
|    1 | 1364 |  |
|    - | 1365 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1366 |  |
