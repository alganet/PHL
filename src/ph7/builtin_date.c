/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * Date/Time functions
 * Status:
 *    Devel.
 */
#include <time.h>
/* Civil-date helpers (defined with the DateTime layer below) */
static sxi64 DtDaysFromCivil(sxi64 y,int m,int d);
static void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd);
static sxi64 DtFloorDiv(sxi64 a,sxi64 b);
/*
 * STRUCT_TM_TO_SYTM zeroes tm_gmtoff (struct tm carries it only as a BSD/glibc
 * extension, absent on newlib/ESP32). Derive the zone offset portably from the
 * broken-down civil fields and the timestamp they came from: for localtime()
 * fills this yields the local UTC offset, for gmtime() fills it yields 0.
 */
static void DtSytmFillOffset(Sytm *pSTm,time_t t)
{
	sxi64 iCivil = DtDaysFromCivil((sxi64)pSTm->tm_year,pSTm->tm_mon+1,pSTm->tm_mday) * 86400
		+ (sxi64)pSTm->tm_hour*3600 + (sxi64)pSTm->tm_min*60 + (sxi64)pSTm->tm_sec;
	pSTm->tm_gmtoff = (long)(iCivil - (sxi64)t);
}
#ifdef __WINNT__
#ifdef _MSC_VER
#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */
#pragma warning(disable:4996) /* _CRT_SECURE_NO_WARNINGS */
#endif
#endif
#endif
#ifdef __WINNT__
/* GetSystemTime() */
#include <Windows.h>
#ifdef _WIN32_WCE
/* SPDX-SnippetBegin */
/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */
/* SPDX-License-Identifier: blessing */
/*
** WindowsCE does not have a localtime() function.  So create a
** substitute.
** Taken from the SQLite3 source tree.
** Status: Public domain
*/
struct tm *__cdecl localtime(const time_t *t)
{
  static struct tm y;
  FILETIME uTm, lTm;
  SYSTEMTIME pTm;
  ph7_int64 t64;
  t64 = *t;
  t64 = (t64 + 11644473600)*10000000;
  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);
  uTm.dwHighDateTime= (DWORD)(t64 >> 32);
  FileTimeToLocalFileTime(&uTm,&lTm);
  FileTimeToSystemTime(&lTm,&pTm);
  y.tm_year = pTm.wYear - 1900;
  y.tm_mon = pTm.wMonth - 1;
  y.tm_wday = pTm.wDayOfWeek;
  y.tm_mday = pTm.wDay;
  y.tm_hour = pTm.wHour;
  y.tm_min = pTm.wMinute;
  y.tm_sec = pTm.wSecond;
  return &y;
}
/* SPDX-SnippetEnd */
#endif /*_WIN32_WCE */
#elif defined(__UNIXES__)
#include <sys/time.h>
#endif /* __WINNT__*/
/*
 * Resolve the current wall-clock time (epoch seconds + sub-second microseconds).
 *
 * An embedder may override the platform clock via PH7_CONFIG_CLOCK (e.g. the
 * ESP32 port routes this through esp_timer); when no hook is registered we use
 * gettimeofday() on Unix and fall back to a second-resolution time() elsewhere.
 * Centralising this here gives microtime()/gettimeofday() a single sub-second
 * source instead of the old nonsensical `tt % SX_USEC_PER_SEC` off-Unix path.
 */
static void DateNow(ph7_vm *pVm,sytime *pOut)
{
	if( pVm && pVm->pEngine->xConf.xClock ){
		ph7_int64 sec = 0,usec = 0;
		if( pVm->pEngine->xConf.xClock(pVm->pEngine->xConf.pClockData,&sec,&usec) == PH7_OK ){
			pOut->tm_sec  = (long)sec;
			pOut->tm_usec = (long)usec;
			return;
		}
	}
#if defined(__UNIXES__)
	{
		struct timeval tv;
		gettimeofday(&tv,0);
		pOut->tm_sec  = (long)tv.tv_sec;
		pOut->tm_usec = (long)tv.tv_usec;
	}
#elif defined(__WINNT__)
	{
		/* FILETIME is 100-ns ticks since 1601-01-01 UTC; convert to the Unix
		 * epoch with microsecond resolution (GetSystemTime() only carries
		 * milliseconds, and time() has no sub-second part at all). */
		FILETIME ft;
		ph7_int64 t;
		GetSystemTimeAsFileTime(&ft);
		t  = (ph7_int64)ft.dwHighDateTime << 32;
		t += ft.dwLowDateTime;
		t -= 116444736000000000LL; /* 100-ns ticks between 1601 and 1970 */
		pOut->tm_sec  = (long)(t / 10000000);
		pOut->tm_usec = (long)((t % 10000000) / 10);
	}
#else
	{
		time_t tt;
		time(&tt);
		pOut->tm_sec  = (long)tt;
		pOut->tm_usec = 0; /* no sub-second source; embedders supply one via PH7_CONFIG_CLOCK */
	}
#endif /* __UNIXES__ */
}
 /*
  * int64 time(void)
  *  Current Unix timestamp
  * Parameters
  *  None.
  * Return
  *  Returns the current time measured in the number of seconds
  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).
  */
PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	time_t tt;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* Extract the current time */
	time(&tt);
	/* Return as 64-bit integer */
	ph7_result_int64(pCtx,(ph7_int64)tt);
	return  PH7_OK;
}
/*
  * string/float microtime([ bool $get_as_float = false ])
  *  microtime() returns the current Unix timestamp with microseconds.
  * Parameters
  *  $get_as_float
  *   If used and set to TRUE, microtime() will return a float instead of a string
  *   as described in the return values section below.
  * Return
  *  By default, microtime() returns a string in the form "msec sec", where sec
  *  is the current time measured in the number of seconds since the Unix
  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds
  *  that have elapsed since sec expressed in seconds.
  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents
  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.
  */
PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int bFloat = 0;
	sytime sTime;
	DateNow(pCtx->pVm,&sTime);
	if( nArg > 0 ){
		bFloat = ph7_value_to_bool(apArg[0]);
	}
	if( bFloat ){
		/* Return as float: seconds accurate to the nearest microsecond */
		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);
	}else{
		/* Return PHP's "msec sec" form: the sub-second part as fractional
		 * seconds to 8 decimals, e.g. "0.50667100 1700000000". tm_usec is in
		 * microseconds (0..999999), so scaling by 100 yields the 8-digit
		 * fraction — matching PHP's "%.8F" output exactly. */
		ph7_result_string_format(pCtx,"0.%08ld %ld",sTime.tm_usec*100,sTime.tm_sec);
	}
	return PH7_OK;
}
/*
 * array getdate ([ int $timestamp = time() ])
 *  Returns an associative array containing the date information
 *  of the timestamp, or the current local time if no timestamp is given.
 * Parameter
 *  $timestamp: The optional timestamp parameter is an integer Unix timestamp
 *     that defaults to the current local time if a timestamp is not given.
 *     In other words, it defaults to the value of time().
 * Returns
 *  Returns an associative array of information related to the timestamp.
 */
PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pValue,*pArray;
	Sytm sTm;
	if( nArg < 1 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS);
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[0]) ){
			t = (time_t)ph7_value_to_int64(apArg[0]);
			pTm = gmtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
	}
	/* Element value */
	pValue = ph7_context_new_scalar(pCtx);
	if( pValue == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array */
	/* Seconds */
	ph7_value_int(pValue,sTm.tm_sec);
	ph7_array_add_strkey_elem(pArray,"seconds",pValue);
	/* Minutes */
	ph7_value_int(pValue,sTm.tm_min);
	ph7_array_add_strkey_elem(pArray,"minutes",pValue);
	/* Hours */
	ph7_value_int(pValue,sTm.tm_hour);
	ph7_array_add_strkey_elem(pArray,"hours",pValue);
	/* mday */
	ph7_value_int(pValue,sTm.tm_mday);
	ph7_array_add_strkey_elem(pArray,"mday",pValue);
	/* wday */
	ph7_value_int(pValue,sTm.tm_wday);
	ph7_array_add_strkey_elem(pArray,"wday",pValue);
	/* mon */
	ph7_value_int(pValue,sTm.tm_mon+1);
	ph7_array_add_strkey_elem(pArray,"mon",pValue);
	/* year */
	ph7_value_int(pValue,sTm.tm_year);
	ph7_array_add_strkey_elem(pArray,"year",pValue);
	/* yday */
	ph7_value_int(pValue,sTm.tm_yday);
	ph7_array_add_strkey_elem(pArray,"yday",pValue);
	/* Weekday [i.e: Monday,Tuesday,...] */
	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);
	ph7_array_add_strkey_elem(pArray,"weekday",pValue);
	/* Reset the string cursor */
	ph7_value_reset_string_cursor(pValue);
	/* Month [i.e: January,February,...] */
	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);
	ph7_array_add_strkey_elem(pArray,"month",pValue);
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * mixed gettimeofday([ bool $return_float = false ] )
 *  Returns an associative array containing the data returned from the system call.
 * Parameters
 *  $return_float
 *   When set to TRUE, a float instead of an array is returned.
 * Return
 *  By default an array is returned. If return_float is set, then
 *  a float is returned.
 */
PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int bFloat = 0;
	sytime sTime;
	DateNow(pCtx->pVm,&sTime);
	if( nArg > 0 ){
		bFloat = ph7_value_to_bool(apArg[0]);
	}
	if( bFloat ){
		/* Return as float: seconds accurate to the nearest microsecond */
		ph7_result_double(pCtx,(double)sTime.tm_sec + (double)sTime.tm_usec/(double)SX_USEC_PER_SEC);
	}else{
		/* Return an associative array */
		ph7_value *pValue,*pArray;
		/* Create a new array */
		pArray = ph7_context_new_array(pCtx);
		/* Element value */
		pValue = ph7_context_new_scalar(pCtx);
		if( pArray == 0 || pValue == 0 ){
			/* Return NULL */
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		/* Fill the array */
		/* sec */
		ph7_value_int64(pValue,sTime.tm_sec);
		ph7_array_add_strkey_elem(pArray,"sec",pValue);
		/* usec */
		ph7_value_int64(pValue,sTime.tm_usec);
		ph7_array_add_strkey_elem(pArray,"usec",pValue);
		/* Return the array */
		ph7_result_value(pCtx,pArray);
	}
	return PH7_OK;
}
/* Check if the given year is leap or not */
#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)
/* ISO-8601 numeric representation of the day of the week */
static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };
/*
 * Format a given date string.
 * Supported format: (Taken from PHP online docs)
 * character 	Description
 * d          Day of the month, 2 digits with leading zeros
 * D          A textual representation of a day, three letters
 * j          Day of the month without leading zeros
 * l          A full textual representation of the day of the week
 * N          ISO-8601 numeric representation of the day of the week
 * w          Numeric representation of the day of the week
 * z          The day of the year (starting from 0)
 * F          A full textual representation of a month, such as January or March
 * m          Numeric representation of a month, with leading zeros 	01 through 12
 * M          A short textual representation of a month, three letters
 * n          Numeric representation of a month, without leading zeros
 * t          Number of days in the given month
 * L          Whether it's a leap year
 * o          ISO-8601 year number. This has the same value as Y
 * Y          A full numeric representation of a year, 4 digits
 * y          A two digit representation of a year
 * a          Lowercase Ante meridiem and Post meridiem 	am or pm
 * A          Uppercase Ante meridiem and Post meridiem
 * g          12-hour format of an hour without leading zeros
 * G          24-hour format of an hour without leading zeros 	0 through 23
 * h          12-hour format of an hour with leading zeros
 * H          24-hour format of an hour with leading zeros
 * i          Minutes with leading zeros
 * s          Seconds, with leading zeros
 * u          Microseconds
 * e          Timezone identifier
 * I          Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.
 * r          RFC 2822 formatted date
 * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)
 * S          English ordinal suffix for the day of the month, 2 characters
 * O          Difference to Greenwich time (GMT) in hours
 * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those
 *            east of UTC is always positive.
 * c         ISO 8601 date
 */
static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)
{
	const char *zEnd = &zIn[nLen];
	const char *zCur;
	/* Start the format process */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		switch(zIn[0]){
		case 'd':
			/* Day of the month, 2 digits with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);
			break;
		case 'D':
			/*A textual representation of a day, three letters*/
			zCur = SyTimeGetDay(pTm->tm_wday);
			ph7_result_string(pCtx,zCur,3);
			break;
		case 'j':
			/*	Day of the month without leading zeros */
			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);
			break;
		case 'l':
			/* A full textual representation of the day of the week */
			zCur = SyTimeGetDay(pTm->tm_wday);
			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);
			break;
		case 'N':{
			/* ISO-8601 numeric representation of the day of the week */
			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);
			break;
				 }
		case 'w':
			/*Numeric representation of the day of the week*/
			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);
			break;
		case 'z':
			/*The day of the year*/
			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);
			break;
		case 'F':
			/*A full textual representation of a month, such as January or March*/
			zCur = SyTimeGetMonth(pTm->tm_mon);
			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);
			break;
		case 'm':
			/*Numeric representation of a month, with leading zeros*/
			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);
			break;
		case 'M':
			/*A short textual representation of a month, three letters*/
			zCur = SyTimeGetMonth(pTm->tm_mon);
			ph7_result_string(pCtx,zCur,3);
			break;
		case 'n':
			/*Numeric representation of a month, without leading zeros*/
			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);
			break;
		case 't':{
			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };
			int nDays = aMonDays[pTm->tm_mon % 12 ];
			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){
				nDays = 28;
			}
			/*Number of days in the given month*/
			ph7_result_string_format(pCtx,"%d",nDays);
			break;
				 }
		case 'L':{
			int isLeap = IS_LEAP_YEAR(pTm->tm_year);
			/* Whether it's a leap year */
			ph7_result_string_format(pCtx,"%d",isLeap);
			break;
				 }
		case 'o': case 'W': {
			/* ISO-8601 week-numbering year / week number: both belong to the
			 * year owning the Thursday of the civil week (php: 2024-12-31 is
			 * 2025-W01, 2027-01-01 is 2026-W53). php pads W but not o. */
			sxi64 days = DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday);
			int isoDow = (int)(((days + 3) % 7 + 7) % 7) + 1; /* Mon=1..Sun=7 */
			sxi64 thu = days + (4 - isoDow);
			sxi64 wy;
			int wm,wd;
			DtCivilFromDays(thu,&wy,&wm,&wd);
			if( zIn[0] == 'o' ){
				ph7_result_string_format(pCtx,"%d",(int)wy);
			}else{
				ph7_result_string_format(pCtx,"%02d",
					(int)((thu - DtDaysFromCivil(wy,1,1)) / 7) + 1);
			}
			break;
				 }
		case 'Y':
			/*	A full numeric representation of a year, 4 digits */
			ph7_result_string_format(pCtx,"%04d",pTm->tm_year);
			break;
		case 'X':
			/* Expanded full year, always signed (php 8.2+): +2024 */
			ph7_result_string_format(pCtx,"%c%04d",
				pTm->tm_year < 0 ? '-' : '+',
				pTm->tm_year < 0 ? -pTm->tm_year : pTm->tm_year);
			break;
		case 'x':
			/* Expanded year, signed only past 4 digits (php 8.2+) */
			if( pTm->tm_year > 9999 ){
				ph7_result_string_format(pCtx,"+%d",pTm->tm_year);
			}else{
				ph7_result_string_format(pCtx,"%04d",pTm->tm_year);
			}
			break;
		case 'y':
			/*A two digit representation of a year*/
			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);
			break;
		case 'a':
			/*	Lowercase Ante meridiem and Post meridiem */
			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "pm" : "am",2);
			break;
		case 'A':
			/*	Uppercase Ante meridiem and Post meridiem */
			ph7_result_string(pCtx,pTm->tm_hour >= 12 ? "PM" : "AM",2);
			break;
		case 'B':{
			/* Swatch Internet time: thousandths of the UTC+1 day */
			sxi64 iUtc = DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400
				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec
				- (sxi64)pTm->tm_gmtoff;
			sxi64 iBie = (iUtc + 3600) % 86400;
			if( iBie < 0 ){
				iBie += 86400;
			}
			ph7_result_string_format(pCtx,"%03d",(int)(iBie * 1000 / 86400));
			break;
				 }
		case 'g':
			/*	12-hour format of an hour without leading zeros*/
			ph7_result_string_format(pCtx,"%d",
				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);
			break;
		case 'G':
			/* 24-hour format of an hour without leading zeros */
			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);
			break;
		case 'h':
			/* 12-hour format of an hour with leading zeros */
			ph7_result_string_format(pCtx,"%02d",
				(pTm->tm_hour % 12) == 0 ? 12 : pTm->tm_hour % 12);
			break;
		case 'H':
			/*	24-hour format of an hour with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);
			break;
		case 'i':
			/* 	Minutes with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);
			break;
		case 's':
			/* 	second with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);
			break;
		case 'u':
			/* 	Microseconds (whole-second timestamps only: php pads zeros) */
			ph7_result_string(pCtx,"000000",6);
			break;
		case 'v':
			/* 	Milliseconds (same) */
			ph7_result_string(pCtx,"000",3);
			break;
		case 'S':{
			/* English ordinal suffix for the day of the month, 2 characters */
			static const char zSuffix[] = "thstndrdthththththth";
			int v = pTm->tm_mday;
			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);
			break;
				 }
		case 'e':
			/* 	Timezone identifier */
			zCur = pTm->tm_zone;
			if( zCur == 0 ){
				/* date()-family fills: the script default timezone */
				zCur = pCtx->pVm->zDefTz;
			}
			ph7_result_string(pCtx,zCur,-1);
			break;
		case 'T':{
			/* Timezone abbreviation: "UTC" for offset 0, "GMT+0530" for a
			 * fixed offset (php's shape). PHL has no tz database, so the
			 * zone-name path only ever sees UTC/GMT, uppercased. */
			const char *z;
			if( pTm->tm_gmtoff != 0 ){
				long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;
				ph7_result_string_format(pCtx,"GMT%c%02d%02d",
					pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));
				break;
			}
			z = pTm->tm_zone ? pTm->tm_zone : pCtx->pVm->zDefTz;
			while( *z ){
				int c = (unsigned char)*z;
				if( c >= 'a' && c <= 'z' ){
					c -= 'a' - 'A';
				}
				ph7_result_string_format(pCtx,"%c",c);
				z++;
			}
			break;
				 }
		case 'I':
			/* Whether or not the date is in daylight saving time */
#ifdef __WINNT__
#ifdef _MSC_VER
#ifndef _WIN32_WCE
			_get_daylight(&pTm->tm_isdst);
#endif
#endif
#endif
			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);
			break;
		case 'r':{
			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200 */
			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;
			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d %c%02d%02d",
				SyTimeGetDay(pTm->tm_wday),
				pTm->tm_mday,
				SyTimeGetMonth(pTm->tm_mon),
				pTm->tm_year,
				pTm->tm_hour,
				pTm->tm_min,
				pTm->tm_sec,
				pTm->tm_gmtoff < 0 ? '-' : '+',
				(int)(a / 3600),(int)((a % 3600) / 60)
				);
			break;
				 }
		case 'U':
			/* Seconds since the Unix Epoch FOR THIS Sytm (php: the timestamp
			 * being formatted — pre-fix this printed time(0) regardless of the
			 * date under format). */
			ph7_result_string_format(pCtx,"%qd",
				DtDaysFromCivil((sxi64)pTm->tm_year,pTm->tm_mon+1,pTm->tm_mday) * 86400
				+ (sxi64)pTm->tm_hour*3600 + (sxi64)pTm->tm_min*60 + (sxi64)pTm->tm_sec
				- (sxi64)pTm->tm_gmtoff);
			break;
		case 'O':{
			/* Difference to GMT without colon: +0530 (php) */
			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;
			ph7_result_string_format(pCtx,"%c%02d%02d",
				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));
			break;
				 }
		case 'P':{
			/* Difference to GMT with colon: +05:30 (php) */
			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;
			ph7_result_string_format(pCtx,"%c%02d:%02d",
				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));
			break;
				 }
		case 'p':{
			/* Like P, but "Z" for UTC (php 8.0+) */
			long a;
			if( pTm->tm_gmtoff == 0 ){
				ph7_result_string(pCtx,"Z",1);
				break;
			}
			a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;
			ph7_result_string_format(pCtx,"%c%02d:%02d",
				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));
			break;
				 }
		case 'Z':
			/* Timezone offset in seconds, plain integer (php) */
			ph7_result_string_format(pCtx,"%d",(int)pTm->tm_gmtoff);
			break;
		case 'c':{
			/* 	ISO 8601 date: 2004-02-12T15:19:21+00:00 (php) */
			long a = pTm->tm_gmtoff < 0 ? -pTm->tm_gmtoff : pTm->tm_gmtoff;
			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
				pTm->tm_year,
				pTm->tm_mon+1,
				pTm->tm_mday,
				pTm->tm_hour,
				pTm->tm_min,
				pTm->tm_sec,
				pTm->tm_gmtoff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60)
				);
			break;
				 }
		case '\\':
			zIn++;
			/* Expand verbatim */
			if( zIn < zEnd ){
				ph7_result_string(pCtx,zIn,(int)sizeof(char));
			}
			break;
		default:
			/* Unknown format specifer,expand verbatim */
			ph7_result_string(pCtx,zIn,(int)sizeof(char));
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	return SXRET_OK;
}
/*
 * PH7 implementation of the strftime() function.
 * The following formats are supported:
 * %a 	An abbreviated textual representation of the day
 * %A 	A full textual representation of the day
 * %d 	Two-digit day of the month (with leading zeros)
 * %e 	Day of the month, with a space preceding single digits.
 * %j 	Day of the year, 3 digits with leading zeros
 * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)
 * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)
 * %U 	Week number of the given year, starting with the first Sunday as the first week
 * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least
 *   4 weekdays, with Monday being the start of the week.
 * %W 	A numeric representation of the week of the year
 * %b 	Abbreviated month name, based on the locale
 * %B 	Full month name, based on the locale
 * %h 	Abbreviated month name, based on the locale (an alias of %b)
 * %m 	Two digit representation of the month
 * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)
 * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)
 * %G 	The full four-digit version of %g
 * %y 	Two digit representation of the year
 * %Y 	Four digit representation for the year
 * %H 	Two digit representation of the hour in 24-hour format
 * %I 	Two digit representation of the hour in 12-hour format
 * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits
 * %M 	Two digit representation of the minute
 * %p 	UPPER-CASE 'AM' or 'PM' based on the given time
 * %P 	lower-case 'am' or 'pm' based on the given time
 * %r 	Same as "%I:%M:%S %p"
 * %R 	Same as "%H:%M"
 * %S 	Two digit representation of the second
 * %T 	Same as "%H:%M:%S"
 * %X 	Preferred time representation based on locale, without the date
 * %z 	Either the time zone offset from UTC or the abbreviation
 * %Z 	The time zone offset/abbreviation option NOT given by %z
 * %c 	Preferred date and time stamp based on local
 * %D 	Same as "%m/%d/%y"
 * %F 	Same as "%Y-%m-%d"
 * %s 	Unix Epoch Time timestamp (same as the time() function)
 * %x 	Preferred date representation based on locale, without the time
 * %n 	A newline character ("\n")
 * %t 	A Tab character ("\t")
 * %% 	A literal percentage character ("%")
 */
static int PH7_Strftime(
	ph7_context *pCtx,  /* Call context */
	const char *zIn,    /* Input string */
	int nLen,           /* Input length */
	Sytm *pTm           /* Parse of the given time */
	)
{
	const char *zCur,*zEnd = &zIn[nLen];
	int c;
	/* Start the format process */
	for(;;){
		zCur = zIn;
		while(zIn < zEnd && zIn[0] != '%' ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Consume input verbatim */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		zIn++; /* Jump the percent sign */
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		c = zIn[0];
		/* Act according to the current specifer */
		switch(c){
		case '%':
			/* A literal percentage character ("%") */
			ph7_result_string(pCtx,"%",(int)sizeof(char));
			break;
		case 't':
			/* A Tab character */
			ph7_result_string(pCtx,"\t",(int)sizeof(char));
			break;
		case 'n':
			/* A newline character */
			ph7_result_string(pCtx,"\n",(int)sizeof(char));
			break;
		case 'a':
			/* An abbreviated textual representation of the day */
			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);
			break;
		case 'A':
			/* A full textual representation of the day */
			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);
			break;
		case 'e':
			/* Day of the month, 2 digits with leading space for single digit*/
			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);
			break;
		case 'd':
			/* Two-digit day of the month (with leading zeros) */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);
			break;
		case 'j':
			/*The day of the year,3 digits with leading zeros*/
			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);
			break;
		case 'u':
			/* ISO-8601 numeric representation of the day of the week */
			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);
			break;
		case 'w':
			/* Numeric representation of the day of the week */
			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);
			break;
		case 'b':
		case 'h':
			/*A short textual representation of a month, three letters (Not based on locale)*/
			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);
			break;
		case 'B':
			/* Full month name (Not based on locale) */
			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);
			break;
		case 'm':
			/*Numeric representation of a month, with leading zeros*/
			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);
			break;
		case 'C':
			/* Two digit representation of the century */
			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);
			break;
		case 'y':
		case 'g':
			/* Two digit representation of the year */
			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);
			break;
		case 'Y':
		case 'G':
			/* Four digit representation of the year */
			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);
			break;
		case 'I':
			/* 12-hour format of an hour with leading zeros */
			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));
			break;
		case 'l':
			/* 12-hour format of an hour with leading space */
			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));
			break;
		case 'H':
			/* 24-hour format of an hour with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);
			break;
		case 'M':
			/* Minutes with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);
			break;
		case 'S':
			/* Seconds with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);
			break;
		case 'z':
		case 'Z':
			/* 	Timezone identifier */
			zCur = pTm->tm_zone;
			if( zCur == 0 ){
				/* date()-family fills: the script default timezone */
				zCur = pCtx->pVm->zDefTz;
			}
			ph7_result_string(pCtx,zCur,-1);
			break;
		case 'T':
		case 'X':
			/* Same as "%H:%M:%S" */
			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);
			break;
		case 'R':
			/* Same as "%H:%M" */
			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);
			break;
		case 'P':
			/*	Lowercase Ante meridiem and Post meridiem */
			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);
			break;
		case 'p':
			/*	Uppercase Ante meridiem and Post meridiem */
			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);
			break;
		case 'r':
			/* Same as "%I:%M:%S %p" */
			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",
				1+(pTm->tm_hour%12),
				pTm->tm_min,
				pTm->tm_sec,
				pTm->tm_hour > 12 ? "PM" : "AM"
				);
			break;
		case 'D':
		case 'x':
			/* Same as "%m/%d/%y" */
			ph7_result_string_format(pCtx,"%02d/%02d/%02d",
				pTm->tm_mon+1,
				pTm->tm_mday,
				pTm->tm_year%100
				);
			break;
		case 'F':
			/* Same as "%Y-%m-%d" */
			ph7_result_string_format(pCtx,"%d-%02d-%02d",
				pTm->tm_year,
				pTm->tm_mon+1,
				pTm->tm_mday
				);
			break;
		case 'c':
			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",
				pTm->tm_year,
				pTm->tm_mon+1,
				pTm->tm_mday,
				pTm->tm_hour,
				pTm->tm_min,
				pTm->tm_sec
				);
			break;
		case 's':{
			time_t tt;
			/* Seconds since the Unix Epoch */
			time(&tt);
			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);
			break;
				 }
		default:
			/* unknown specifer,simply ignore*/
			break;
		}
		/* Advance the cursor */
		zIn++;
	}
	return SXRET_OK;
}
/*
 * Resolve a date()/gmdate() $timestamp argument under php 8's ?int weak ZPP:
 *   - null            -> *pbUseNow = 1 (caller uses the current time)
 *   - int/bool/float  -> coerce to a Unix timestamp (float truncates; php's
 *                        float->int precision E_DEPRECATED is not emitted, §3.7)
 *   - numeric string  -> coerce via php's is_numeric_string grammar
 *                        (RangeStrToNumber: " 100 "/"1e3"/".5"/"+5" ok)
 *   - anything else (non-numeric string, array, object, resource)
 *                     -> catchable TypeError, byte-exact with php.
 * Returns PH7_OK with *pbUseNow / *pT set, or the PH7_VmThrowException status.
 */
static int DateResolveTimestamp(ph7_context *pCtx,ph7_value *pArg,int *pbUseNow,time_t *pT)
{
	char zBuf[64];
	*pbUseNow = 0;
	if( ph7_value_is_null(pArg) ){
		*pbUseNow = 1;
		return PH7_OK;
	}
	if( ph7_value_is_int(pArg) || ph7_value_is_bool(pArg) || ph7_value_is_float(pArg) ){
		*pT = (time_t)ph7_value_to_int64(pArg);
		return PH7_OK;
	}
	if( ph7_value_is_string(pArg) ){
		int nStr;
		const char *zStr = ph7_value_to_string(pArg,&nStr);
		sxi64 iLong; double dReal;
		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);
		if( iKind == RANGE_IN_DOUBLE ){
			*pT = (time_t)dReal;
			return PH7_OK;
		}
		if( iKind == RANGE_IN_LONG ){
			*pT = (time_t)iLong;
			return PH7_OK;
		}
		/* Not a numeric string: fall through to the TypeError. */
	}
	return PH7_VmThrowException(pCtx,"TypeError",
		"%s(): Argument #2 ($timestamp) must be of type ?int, %s given",
		ph7_function_name(pCtx),VmValueGivenName(pArg,zBuf,sizeof(zBuf)));
}
/*
 * string date(string $format [, int $timestamp = time() ] )
 *  Returns a string formatted according to the given format string using
 *  the given integer timestamp or the current time if no timestamp is given.
 *  In other words, timestamp is optional and defaults to the value of time().
 * Parameters
 *  $format
 *   The format of the outputted date string (See code above)
 * $timestamp
 *   The optional timestamp parameter is an integer Unix timestamp
 *   that defaults to the current local time if a timestamp is not given.
 *   In other words, it defaults to the value of time().
 * Return
 *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.
 */
PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	int nLen;
	Sytm sTm;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Don't bother processing return the empty string */
		ph7_result_string(pCtx,"",0);
	}
	if( nArg < 2 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS);
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
#endif
	}else{
		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */
		time_t t = 0;
		struct tm *pTm;
		int bUseNow;
		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);
		if( rc != PH7_OK ){
			return rc;
		}
		if( bUseNow ){
			time(&t);
		}
		pTm = gmtime(&t);
		if( pTm == 0 ){
			time(&t);
			pTm = gmtime(&t);
		}
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
	}
	/* Format the given string */
	DateFormat(pCtx,zFormat,nLen,&sTm);
	return PH7_OK;
}
/*
 * string strftime(string $format [, int $timestamp = time() ] )
 *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)
 * Parameters
 *  $format
 *   The format of the outputted date string (See code above)
 * $timestamp
 *   The optional timestamp parameter is an integer Unix timestamp
 *   that defaults to the current local time if a timestamp is not given.
 *   In other words, it defaults to the value of time().
 * Return
 * Returns a string formatted according format using the given timestamp
 * or the current local time if no timestamp is given.
 */
PH7_PRIVATE int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	int nLen;
	Sytm sTm;
	/* The php 8.1 whole-function deprecation is declared in aBuiltinDeprecated[] and
	 * emitted at the OP_CALL choke point, which is what puts it BEFORE the
	 * ArgumentCountError for a no-arg call — php's order. */
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Don't bother processing return FALSE */
		ph7_result_bool(pCtx,0);
	}
	if( nArg < 2 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS);
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[1]) ){
			t = (time_t)ph7_value_to_int64(apArg[1]);
			pTm = gmtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
	}
	/* Format the given string */
	PH7_Strftime(pCtx,zFormat,nLen,&sTm);
	if( ph7_context_result_buf_length(pCtx) < 1 ){
		/* Nothing was formatted,return FALSE */
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * string gmdate(string $format [, int $timestamp = time() ] )
 *  Identical to the date() function except that the time returned
 *  is Greenwich Mean Time (GMT).
 * Parameters
 *  $format
 *  The format of the outputted date string (See code above)
 *  $timestamp
 *   The optional timestamp parameter is an integer Unix timestamp
 *   that defaults to the current local time if a timestamp is not given.
 *   In other words, it defaults to the value of time().
 * Return
 *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.
 */
PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	int nLen;
	Sytm sTm;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Don't bother processing return the empty string */
		ph7_result_string(pCtx,"",0);
	}
	if( nArg < 2 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS);
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
#endif
	}else{
		/* Use the given timestamp (php 8 ?int weak ZPP; TypeError otherwise) */
		time_t t = 0;
		struct tm *pTm;
		int bUseNow;
		int rc = DateResolveTimestamp(pCtx,apArg[1],&bUseNow,&t);
		if( rc != PH7_OK ){
			return rc;
		}
		if( bUseNow ){
			time(&t);
		}
		pTm = gmtime(&t);
		if( pTm == 0 ){
			time(&t);
			pTm = gmtime(&t);
		}
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
	}
	/* Format the given string */
	DateFormat(pCtx,zFormat,nLen,&sTm);
	return PH7_OK;
}
/*
 * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])
 *  Return the local time.
 * Parameter
 *  $timestamp: The optional timestamp parameter is an integer Unix timestamp
 *     that defaults to the current local time if a timestamp is not given.
 *     In other words, it defaults to the value of time().
 * $is_associative
 *   If set to FALSE or not supplied then the array is returned as a regular, numerically
 *   indexed array. If the argument is set to TRUE then localtime() returns an associative
 *   array containing all the different elements of the structure returned by the C function
 *   call to localtime. The names of the different keys of the associative array are as follows:
 *      "tm_sec" - seconds, 0 to 59
 *      "tm_min" - minutes, 0 to 59
 *      "tm_hour" - hours, 0 to 23
 *      "tm_mday" - day of the month, 1 to 31
 *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)
 *      "tm_year" - years since 1900
 *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)
 *      "tm_yday" - day of the year, 0 to 365
 *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.
 * Returns
 *  An associative array of information related to the timestamp.
 */
PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pValue,*pArray;
	int isAssoc = 0;
	Sytm sTm;
	if( nArg < 1 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS); /* TODO(chems): GMT not local */
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[0]) ){
			t = (time_t)ph7_value_to_int64(apArg[0]);
			pTm = gmtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
	}
	/* Element value */
	pValue = ph7_context_new_scalar(pCtx);
	if( pValue == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg > 1 ){
		isAssoc = ph7_value_to_bool(apArg[1]);
	}
	/* Fill the array */
	/* Seconds */
	ph7_value_int(pValue,sTm.tm_sec);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* Minutes */
	ph7_value_int(pValue,sTm.tm_min);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* Hours */
	ph7_value_int(pValue,sTm.tm_hour);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* mday */
	ph7_value_int(pValue,sTm.tm_mday);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* mon */
	ph7_value_int(pValue,sTm.tm_mon);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* year since 1900 */
	ph7_value_int(pValue,sTm.tm_year-1900);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* wday */
	ph7_value_int(pValue,sTm.tm_wday);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* yday */
	ph7_value_int(pValue,sTm.tm_yday);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* isdst */
#ifdef __WINNT__
#ifdef _MSC_VER
#ifndef _WIN32_WCE
			_get_daylight(&sTm.tm_isdst);
#endif
#endif
#endif
	ph7_value_int(pValue,sTm.tm_isdst);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* Return the array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * int idate(string $format [, int $timestamp = time() ])
 *  Returns a number formatted according to the given format string
 *  using the given integer timestamp or the current local time if
 *  no timestamp is given. In other words, timestamp is optional and defaults
 *  to the value of time().
 *  Unlike the function date(), idate() accepts just one char in the format
 *  parameter.
 * $Parameters
 *  Supported format
 *   d 	Day of the month
 *   h 	Hour (12 hour format)
 *   H 	Hour (24 hour format)
 *   i 	Minutes
 *   I (uppercase i)1 if DST is activated, 0 otherwise
 *   L (uppercase l) returns 1 for leap year, 0 otherwise
 *   m 	Month number
 *   s 	Seconds
 *   t 	Days in current month
 *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()
 *   w 	Day of the week (0 on Sunday)
 *   W 	ISO-8601 week number of year, weeks starting on Monday
 *   y 	Year (1 or 2 digits - check note below)
 *   Y 	Year (4 digits)
 *   z 	Day of the year
 *   Z 	Timezone offset in seconds
 * $timestamp
 *  The optional timestamp parameter is an integer Unix timestamp that defaults
 *  to the current local time if a timestamp is not given. In other words, it defaults
 *  to the value of time().
 * Return
 *  An integer.
 */
PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	ph7_int64 iVal = 0;
	int nLen;
	Sytm sTm;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Don't bother processing return -1*/
		ph7_result_int(pCtx,-1);
	}
	if( nArg < 2 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS);
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[1]) ){
			t = (time_t)ph7_value_to_int64(apArg[1]);
			pTm = gmtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
		DtSytmFillOffset(&sTm,t);
	}
	/* Perform the requested operation */
	switch(zFormat[0]){
	case 'd':
		/* Day of the month */
		iVal = sTm.tm_mday;
		break;
	case 'h':
		/*	Hour (12 hour format)*/
		iVal = 1 + (sTm.tm_hour % 12);
		break;
	case 'H':
		/* Hour (24 hour format)*/
		iVal = sTm.tm_hour;
		break;
	case 'i':
		/*Minutes*/
		iVal = sTm.tm_min;
		break;
	case 'I':
		/*	returns 1 if DST is activated, 0 otherwise */
#ifdef __WINNT__
#ifdef _MSC_VER
#ifndef _WIN32_WCE
			_get_daylight(&sTm.tm_isdst);
#endif
#endif
#endif
		iVal = sTm.tm_isdst;
		break;
	case 'L':
		/* 	returns 1 for leap year, 0 otherwise */
		iVal = IS_LEAP_YEAR(sTm.tm_year);
		break;
	case 'm':
		/* Month number*/
		iVal = sTm.tm_mon;
		break;
	case 's':
		/*Seconds*/
		iVal = sTm.tm_sec;
		break;
	case 't':{
		/*Days in current month*/
		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };
		int nDays = aMonDays[sTm.tm_mon % 12 ];
		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){
			nDays = 28;
		}
		iVal = nDays;
		break;
			 }
	case 'U':
		/*Seconds since the Unix Epoch*/
		iVal = (ph7_int64)time(0);
		break;
	case 'w':
		/*	Day of the week (0 on Sunday) */
		iVal = sTm.tm_wday;
		break;
	case 'W': {
		/* ISO-8601 week number of year, weeks starting on Monday */
		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };
		iVal = aISO8601_local[sTm.tm_wday % 7 ];
		break;
			  }
	case 'y':
		/* Year (2 digits) */
		iVal = sTm.tm_year % 100;
		break;
	case 'Y':
		/* Year (4 digits) */
		iVal = sTm.tm_year;
		break;
	case 'z':
		/* Day of the year */
		iVal = sTm.tm_yday;
		break;
	case 'Z':
		/*Timezone offset in seconds*/
		iVal = sTm.tm_gmtoff;
		break;
	default:
		/* unknown format,throw a warning */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");
		break;
	}
	/* Return the time value */
	ph7_result_int64(pCtx,iVal);
	return PH7_OK;
}
/*
 * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")
 *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )
 *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer
 *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time
 *  specified.
 *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to
 *  the current value according to the local date and time.
 * Parameters
 * $hour
 *  The number of the hour relevant to the start of the day determined by month, day and year.
 *  Negative values reference the hour before midnight of the day in question. Values greater
 *  than 23 reference the appropriate hour in the following day(s).
 * $minute
 *  The number of the minute relevant to the start of the hour. Negative values reference
 *  the minute in the previous hour. Values greater than 59 reference the appropriate minute
 *  in the following hour(s).
 * $second
 *  The number of seconds relevant to the start of the minute. Negative values reference
 *  the second in the previous minute. Values greater than 59 reference the appropriate
 * second in the following minute(s).
 * $month
 *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference
 *  the normal calendar months of the year in question. Values less than 1 (including negative values)
 *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...
 * $day
 *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31
 *  (depending upon the month) reference the normal days in the relevant month. Values less than 1
 *  (including negative values) reference the days in the previous month, so 0 is the last day
 *  of the previous month, -1 is the day before that, etc. Values greater than the number of days
 *  in the relevant month reference the appropriate day in the following month(s).
 * $year
 *  The number of the year, may be a two or four digit value, with values between 0-69 mapping
 *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as
 *  most common today, the valid range for year is somewhere between 1901 and 2038.
 * $is_dst
 *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,
 *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.
 * Return
 *   mktime() returns the Unix timestamp of the arguments given.
 *   If the arguments are invalid, the function returns FALSE
 */
PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFunction;
	ph7_int64 iVal;
	sxi64 h,mi,s,mo,d,y,yAdj;
	int moN;
	struct tm *pTm;
	time_t t;
	/* Extract function name */
	zFunction = ph7_function_name(pCtx);
	/* PHP 8 dropped the legacy $is_dst 7th parameter: mktime()/gmmktime() now
	 * accept at most 6 arguments and throw a catchable ArgumentCountError
	 * otherwise (the central aBuiltinArity table only enforces the minimum, so
	 * this maximum is checked here). */
	if( nArg > 6 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"%s() expects at most 6 arguments, %d given",zFunction,nArg);
	}
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"%s() expects at least 1 argument, 0 given",zFunction);
	}
	/* Missing components default from the current time in php's default
	 * timezone. PHL's date_default_timezone_set() only accepts UTC/GMT (no tz
	 * database), so mktime() and gmmktime() agree and both read gmtime(). */
	time(&t);
	pTm = gmtime(&t);
	SXUNUSED(zFunction);
	h  = pTm->tm_hour;
	mi = pTm->tm_min;
	s  = pTm->tm_sec;
	mo = pTm->tm_mon + 1;
	d  = pTm->tm_mday;
	y  = pTm->tm_year + 1900;
	h = ph7_value_to_int64(apArg[0]);
	if( nArg > 1 ){
		mi = ph7_value_to_int64(apArg[1]);
		if( nArg > 2 ){
			s = ph7_value_to_int64(apArg[2]);
			if( nArg > 3 ){
				mo = ph7_value_to_int64(apArg[3]);
				if( nArg > 4 ){
					d = ph7_value_to_int64(apArg[4]);
					if( nArg > 5 ){
						/* php's legacy two-digit mapping: 0-69 -> 2000-2069,
						 * 70-100 -> 1970-2000; anything else is verbatim */
						y = ph7_value_to_int64(apArg[5]);
						if( y >= 0 && y <= 69 ){
							y += 2000;
						}else if( y >= 70 && y <= 100 ){
							y += 1900;
						}
					}
				}
			}
		}
	}
	/* Normalize the month with floor semantics, then let day/time components
	 * overflow linearly (php: mktime(25,-30,0,1,1,2024) == Jan 2 00:30). */
	yAdj = y + DtFloorDiv(mo - 1,12);
	moN  = (int)(mo - 1 - DtFloorDiv(mo - 1,12) * 12) + 1;
	iVal = (DtDaysFromCivil(yAdj,moN,1) + (d - 1)) * 86400 + h*3600 + mi*60 + s;
	/* Return the timestamp as a 64bit integer */
	ph7_result_int64(pCtx,iVal);
	return PH7_OK;
}
/*
 * string date_default_timezone_get(void)
 *  Gets the default timezone used by all date/time functions in a script.
 */
PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_string(pCtx,pVm->zDefTz,(int)pVm->nDefTz);
	return PH7_OK;
}
/*
 * bool date_default_timezone_set(string $timezoneId)
 *  Sets the default timezone used by all date/time functions in a script.
 *  php validates against the tz database and stores the id verbatim (get()
 *  echoes back "utc" if that's what was set). PHL ships no tz database, so
 *  only UTC and GMT are accepted; every other id — including region names php
 *  would accept — is rejected with php's invalid-id notice (recorded scope cut).
 */
PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zId;
	int nId;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zId = ph7_value_to_string(apArg[0],&nId);
	if( nId == 3 && (SyStrnicmp(zId,"UTC",3) == 0 || SyStrnicmp(zId,"GMT",3) == 0) ){
		SyMemcpy(zId,pVm->zDefTz,3);
		pVm->zDefTz[3] = 0;
		pVm->nDefTz = 3;
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	/* ph7_context_throw_error_format prepends "date_default_timezone_set(): "
	 * — exactly php's notice shape here */
	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Timezone ID '%.*s' is invalid",nId,zId);
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}

/* ===========================================================================
 * DateTime family (NEWPLAN band D slice 1): DateTimeInterface, DateTime,
 * DateTimeImmutable, DateTimeZone (UTC + fixed offsets), date_create(),
 * date_create_immutable(). Embedded-PHP chunk + C thunks, following the
 * Reflection architecture (installed inside the bCompilingBuiltin window).
 * Timezone SCOPE: UTC and fixed "+HH:MM" offsets only — no tz database
 * (recorded §10 scope cut; named region zones throw like unknown zones).
 * ======================================================================== */

/*
 * Proleptic-Gregorian civil <-> day-count conversions (Howard Hinnant's
 * algorithms): no time_t / libc dependence, correct far past 2038 and
 * before 1970 on every platform. Day 0 == 1970-01-01.
 */
static sxi64 DtDaysFromCivil(sxi64 y,int m,int d)
{
	sxi64 era;
	unsigned yoe,doy,doe;
	y -= (m <= 2);
	era = (y >= 0 ? y : y - 399) / 400;
	yoe = (unsigned)(y - era * 400);
	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + (sxi64)doe - 719468;
}
static void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)
{
	sxi64 era;
	unsigned doe,yoe,doy,mp;
	z += 719468;
	era = (z >= 0 ? z : z - 146096) / 146097;
	doe = (unsigned)(z - era * 146097);
	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
	*py = (sxi64)yoe + era * 400;
	doy = doe - (365 * yoe + yoe/4 - yoe/100);
	mp = (5 * doy + 2) / 153;
	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);
	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);
	if( *pm <= 2 ){
		*py += 1;
	}
}
static sxi64 DtFloorDiv(sxi64 a,sxi64 b)
{
	sxi64 q = a / b;
	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){
		q--;
	}
	return q;
}
/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z) */
static void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)
{
	sxi64 t = iTs + iOff;
	sxi64 days = DtFloorDiv(t,86400);
	sxi64 secs = t - days * 86400;
	sxi64 y;
	int mo,d;
	DtCivilFromDays(days,&y,&mo,&d);
	pTm->tm_sec  = (int)(secs % 60);
	pTm->tm_min  = (int)((secs / 60) % 60);
	pTm->tm_hour = (int)(secs / 3600);
	pTm->tm_mday = d;
	pTm->tm_mon  = mo - 1;
	pTm->tm_year = (int)y;
	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */
	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));
	pTm->tm_isdst = 0;
	pTm->tm_zone = zZone;
	pTm->tm_gmtoff = (long)iOff;
}
static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)
{
	return DtDaysFromCivil(y,mo,d) * 86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;
}
/* Month-arithmetic with php's overflow semantics (Jan 31 +1 month -> Mar 2/3):
 * normalize the month, keep the day — the civil day-count formula is linear in
 * d, so an out-of-range day simply lands in the following month. */
static sxi64 DtAddMonths(sxi64 iTs,sxi32 iOff,sxi64 nMonths)
{
	sxi64 t = iTs + iOff;
	sxi64 days = DtFloorDiv(t,86400);
	sxi64 secs = t - days * 86400;
	sxi64 y;
	int mo,d;
	sxi64 m0;
	DtCivilFromDays(days,&y,&mo,&d);
	m0 = (y * 12 + (mo - 1)) + nMonths;
	y  = DtFloorDiv(m0,12);
	mo = (int)(m0 - y * 12) + 1;
	return DtDaysFromCivil(y,mo,d) * 86400 + secs - iOff;
}
/*
 * Minimal php-datetime-string parser (slice 1): absolute forms
 * "now" | "@<ts>" | "YYYY-MM-DD[( |T)HH:MM[:SS]][Z|±HH[:MM]]" | "HH:MM[:SS]",
 * keywords today/midnight/noon/tomorrow/yesterday, and relative sequences
 * "[+|-]N (sec|min|hour|day|week|fortnight|month|year)[s]". Returns 0 on
 * success (ts/off/bOffSet out), or the byte position of the first
 * unparseable character +1 (for php's "at position N" message).
 */
static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,
	sxi64 *pTs,sxi32 *pOff,int *pbOffSet)
{
	const char *z = zIn, *zEnd = &zIn[nLen];
	sxi64 iTs = iBaseTs;
	sxi32 iOff = iBaseOff;
	int bOffSet = 0;
	int bAny = 0;
#define DT_SKIP_WS() while( z < zEnd && (z[0]==' '||z[0]=='\t'||z[0]==',') ){ z++; }
#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \
	&& (zEnd-z == (nKw) || !SyisAlpha(z[(nKw)])))
	DT_SKIP_WS();
	if( z >= zEnd ){
		/* php: the empty string is "now" */
		*pTs = iTs;
		*pOff = iOff;
		*pbOffSet = bOffSet;
		return 0;
	}
	/* "@<seconds>" absolute epoch */
	if( z[0] == '@' ){
		int neg = 0;
		sxi64 v = 0;
		const char *zAt = z;
		z++;
		if( z < zEnd && (z[0]=='-'||z[0]=='+') ){ neg = (z[0]=='-'); z++; }
		/* php's lexer rejects the whole token: the error points at the '@' */
		if( z >= zEnd || !SyisDigit(z[0]) ){ return (int)(zAt - zIn) + 1; }
		while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }
		*pTs = neg ? -v : v;
		*pOff = 0;
		*pbOffSet = 1;
		DT_SKIP_WS();
		return (z < zEnd) ? (int)(z - zIn) + 1 : 0;
	}
	/* Absolute date: YYYY-MM-DD[...] */
	if( zEnd-z >= 10 && SyisDigit(z[0]) && SyisDigit(z[1]) && SyisDigit(z[2])
	 && SyisDigit(z[3]) && z[4]=='-' ){
		sxi64 y = (z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0');
		int mo,d,h=0,mi=0,s=0;
		if( !SyisDigit(z[5])||!SyisDigit(z[6])||z[7] != '-'||!SyisDigit(z[8])||!SyisDigit(z[9]) ){
			return (int)(z - zIn) + 1;
		}
		mo = (z[5]-'0')*10 + (z[6]-'0');
		d  = (z[8]-'0')*10 + (z[9]-'0');
		/* php's lexer dies on the SECOND digit of an out-of-range month/day
		 * (either the two-digit pattern fails there, or a one-digit component
		 * matched and the separator check fails there); "00" lexes fine and
		 * normalizes (month 0 == December of the previous year). */
		if( mo > 12 ){ return (int)(&z[6] - zIn) + 1; }
		if( d > 31 ){ return (int)(&z[9] - zIn) + 1; }
		if( mo == 0 ){ mo = 12; y--; }
		z += 10;
		if( z < zEnd && (z[0]=='T' || z[0]==' ') && zEnd-z >= 6
		 && SyisDigit(z[1]) && SyisDigit(z[2]) && z[3]==':' ){
			z++;
			h  = (z[0]-'0')*10 + (z[1]-'0');
			mi = (z[3]-'0')*10 + (z[4]-'0');
			/* a 25+ hour kills php's whole time token: error at its start */
			if( h > 24 ){ return (int)(z - zIn) + 1; }
			/* php lexes HH:M, then the minute's second digit starts a SECOND
			 * time token: "Double time specification" (negative encoding) */
			if( mi > 59 ){ return -((int)(&z[4] - zIn) + 1); }
			z += 5;
			if( z+2 < zEnd+1 && z < zEnd && z[0]==':' && zEnd-z >= 3
			 && SyisDigit(z[1]) && SyisDigit(z[2]) ){
				s = (z[1]-'0')*10 + (z[2]-'0');
				if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }
				z += 3;
			}
			if( z < zEnd && z[0]=='.' ){ /* fractional seconds: consume */
				z++;
				while( z < zEnd && SyisDigit(z[0]) ){ z++; }
			}
			if( z < zEnd && (z[0]=='Z' || z[0]=='z') ){
				/* 2 = explicit "Z" zone: php names it "Z", not "+00:00" */
				iOff = 0; bOffSet = 2; z++;
			}else if( z < zEnd && (z[0]=='+' || z[0]=='-') ){
				int sign = (z[0]=='-') ? -1 : 1;
				int oh,om = 0;
				z++;
				if( zEnd-z < 2 || !SyisDigit(z[0]) || !SyisDigit(z[1]) ){ return (int)(z - zIn) + 1; }
				oh = (z[0]-'0')*10 + (z[1]-'0');
				z += 2;
				if( z < zEnd && z[0]==':' ){ z++; }
				if( zEnd-z >= 2 && SyisDigit(z[0]) && SyisDigit(z[1]) ){
					om = (z[0]-'0')*10 + (z[1]-'0');
					z += 2;
				}
				iOff = sign * (oh*3600 + om*60);
				bOffSet = 1;
			}
		}
		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);
		bAny = 1;
	}else if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'
	 && SyisDigit(z[3]) && SyisDigit(z[4]) ){
		/* Time-only: HH:MM[:SS] on the base date */
		sxi64 t = iTs + iOff;
		sxi64 days = DtFloorDiv(t,86400);
		int h  = (z[0]-'0')*10 + (z[1]-'0');
		int mi = (z[3]-'0')*10 + (z[4]-'0');
		int s = 0;
		/* php: bad hour kills the token (error at its start); bad minute /
		 * second dies on the component's second digit */
		if( h > 24 ){ return (int)(z - zIn) + 1; }
		if( mi > 59 ){ return (int)(&z[4] - zIn) + 1; }
		z += 5;
		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){
			s = (z[1]-'0')*10 + (z[2]-'0');
			if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }
			z += 3;
		}
		iTs = days*86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;
		bAny = 1;
	}else if( DT_LOWEQ("now",3) ){
		z += 3;
		bAny = 1;
	}
	/* Relative / keyword sequence */
	for(;;){
		DT_SKIP_WS();
		if( z >= zEnd ){
			break;
		}
		if( DT_LOWEQ("today",5) || DT_LOWEQ("midnight",8) ){
			sxi64 days = DtFloorDiv(iTs + iOff,86400);
			iTs = days*86400 - iOff;
			z += (SyToLower(z[0])=='t') ? 5 : 8;
			bAny = 1;
			continue;
		}
		if( DT_LOWEQ("noon",4) ){
			sxi64 days = DtFloorDiv(iTs + iOff,86400);
			iTs = days*86400 + 12*3600 - iOff;
			z += 4;
			bAny = 1;
			continue;
		}
		if( DT_LOWEQ("tomorrow",8) ){
			sxi64 days = DtFloorDiv(iTs + iOff,86400) + 1;
			iTs = days*86400 - iOff;
			z += 8;
			bAny = 1;
			continue;
		}
		if( DT_LOWEQ("yesterday",9) ){
			sxi64 days = DtFloorDiv(iTs + iOff,86400) - 1;
			iTs = days*86400 - iOff;
			z += 9;
			bAny = 1;
			continue;
		}
		if( SyisDigit(z[0]) || z[0]=='+' || z[0]=='-' ){
			int neg = 0;
			sxi64 v = 0;
			const char *zNumStart = z;
			if( z[0]=='+' || z[0]=='-' ){ neg = (z[0]=='-'); z++; }
			if( z >= zEnd || !SyisDigit(z[0]) ){ return (int)(zNumStart - zIn) + 1; }
			while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }
			if( neg ){ v = -v; }
			DT_SKIP_WS();
			if( DT_LOWEQ("seconds",7) )     { iTs += v;            z += 7; }
			else if( DT_LOWEQ("second",6) ) { iTs += v;            z += 6; }
			else if( DT_LOWEQ("secs",4) )   { iTs += v;            z += 4; }
			else if( DT_LOWEQ("sec",3) )    { iTs += v;            z += 3; }
			else if( DT_LOWEQ("minutes",7) ){ iTs += v*60;         z += 7; }
			else if( DT_LOWEQ("minute",6) ) { iTs += v*60;         z += 6; }
			else if( DT_LOWEQ("mins",4) )   { iTs += v*60;         z += 4; }
			else if( DT_LOWEQ("min",3) )    { iTs += v*60;         z += 3; }
			else if( DT_LOWEQ("hours",5) )  { iTs += v*3600;       z += 5; }
			else if( DT_LOWEQ("hour",4) )   { iTs += v*3600;       z += 4; }
			else if( DT_LOWEQ("days",4) )   { iTs += v*86400;      z += 4; }
			else if( DT_LOWEQ("day",3) )    { iTs += v*86400;      z += 3; }
			else if( DT_LOWEQ("weeks",5) )  { iTs += v*7*86400;    z += 5; }
			else if( DT_LOWEQ("week",4) )   { iTs += v*7*86400;    z += 4; }
			else if( DT_LOWEQ("fortnights",10) ){ iTs += v*14*86400; z += 10; }
			else if( DT_LOWEQ("fortnight",9) )  { iTs += v*14*86400; z += 9; }
			else if( DT_LOWEQ("months",6) ) { iTs = DtAddMonths(iTs,iOff,v); z += 6; }
			else if( DT_LOWEQ("month",5) )  { iTs = DtAddMonths(iTs,iOff,v); z += 5; }
			else if( DT_LOWEQ("years",5) )  { iTs = DtAddMonths(iTs,iOff,v*12); z += 5; }
			else if( DT_LOWEQ("year",4) )   { iTs = DtAddMonths(iTs,iOff,v*12); z += 4; }
			else{
				return (int)(z - zIn) + 1;
			}
			bAny = 1;
			continue;
		}
		return (int)(z - zIn) + 1;
	}
	if( !bAny ){
		return 1;
	}
	*pTs = iTs;
	*pOff = iOff;
	*pbOffSet = bOffSet;
	return 0;
#undef DT_SKIP_WS
#undef DT_LOWEQ
}
/* int __dt_now() */
static int vm_builtin_dt_now(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,(ph7_int64)time(0));
	return PH7_OK;
}
/* mixed __dt_parse(string $s, int $baseTs, int $baseOff)
 *   -> [ts, off, offWasExplicit] on success; php's error MESSAGE string on
 *      failure (the chunk wraps it in DateMalformedStringException). */
static int vm_builtin_dt_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	sxi64 iBaseTs;
	sxi32 iBaseOff;
	sxi64 iTs = 0;
	sxi32 iOff = 0;
	int bOffSet = 0;
	int iErrPos;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nLen);
	iBaseTs  = ph7_value_to_int64(apArg[1]);
	iBaseOff = (sxi32)ph7_value_to_int64(apArg[2]);
	iErrPos = DtParse(zIn,nLen,iBaseTs,iBaseOff,&iTs,&iOff,&bOffSet);
	if( iErrPos != 0 ){
		/* Negative encoding: php's "Double time specification" reason */
		int bDouble = iErrPos < 0;
		int iPos = (bDouble ? -iErrPos : iErrPos) - 1;
		char cAt = (iPos < nLen) ? zIn[iPos] : ' ';
		/* php appends a reason: an alphabetic token is assumed to be a timezone
		 * lookup miss, anything else an unexpected character. */
		ph7_result_string_format(pCtx,
			"Failed to parse time string (%.*s) at position %d (%c): %s",
			nLen,zIn,iPos,cAt,
			bDouble ? "Double time specification"
			: ((cAt >= 'a' && cAt <= 'z') || (cAt >= 'A' && cAt <= 'Z'))
				? "The timezone could not be found in the database"
				: "Unexpected character");
		return PH7_OK;
	}
	{
		ph7_value *pArr = ph7_context_new_array(pCtx);
		ph7_value *pV = ph7_context_new_scalar(pCtx);
		if( pArr == 0 || pV == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		ph7_value_int64(pV,iTs);
		ph7_array_add_elem(pArr,0,pV);
		ph7_value_int64(pV,iOff);
		ph7_array_add_elem(pArr,0,pV);
		/* int, not bool: 0 = no explicit offset, 1 = numeric offset/@epoch,
		 * 2 = literal "Z" (php keeps the distinction in the zone name) */
		ph7_value_int64(pV,bOffSet);
		ph7_array_add_elem(pArr,0,pV);
		ph7_result_value(pCtx,pArr);
	}
	return PH7_OK;
}
/* string __dt_default_tz(void) — the date_default_timezone_set() identifier */
static int vm_builtin_dt_default_tz(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_string(pCtx,pCtx->pVm->zDefTz,(int)pCtx->pVm->nDefTz);
	return PH7_OK;
}
/* string __dt_format(int $ts, int $off, string $tzname, string $format) */
static int vm_builtin_dt_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	Sytm sTm;
	sxi64 iTs;
	sxi32 iOff;
	const char *zName,*zFmt;
	int nName,nFmt;
	char zZone[64];
	if( nArg < 4 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iTs  = ph7_value_to_int64(apArg[0]);
	iOff = (sxi32)ph7_value_to_int64(apArg[1]);
	zName = ph7_value_to_string(apArg[2],&nName);
	zFmt  = ph7_value_to_string(apArg[3],&nFmt);
	if( nName >= (int)sizeof(zZone) ){ nName = (int)sizeof(zZone) - 1; }
	SyMemcpy(zName,zZone,(sxu32)nName);
	zZone[nName] = 0;
	DtFillSytm(iTs,iOff,zZone,&sTm);
	DateFormat(pCtx,zFmt,nFmt,&sTm);
	return PH7_OK;
}
/* int __dt_make(int y, int mo, int d, int h, int i, int s, int off) */
static int vm_builtin_dt_make(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 y;
	int mo,d,h,mi,s;
	sxi32 iOff;
	if( nArg < 7 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	y   = ph7_value_to_int64(apArg[0]);
	mo  = ph7_value_to_int(apArg[1]);
	d   = ph7_value_to_int(apArg[2]);
	h   = ph7_value_to_int(apArg[3]);
	mi  = ph7_value_to_int(apArg[4]);
	s   = ph7_value_to_int(apArg[5]);
	iOff = (sxi32)ph7_value_to_int64(apArg[6]);
	ph7_result_int64(pCtx,DtMakeTs(y,mo,d,h,mi,s,iOff));
	return PH7_OK;
}
/* Days in a civil month (php's overflow rules use it during diff borrows) */
static int DtDaysInMonth(sxi64 y,int m)
{
	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ){
		return 29;
	}
	return aMonDays[(m - 1) % 12];
}
/* int __dt_civil_add(int ts, int off, int y, int m, int d, int h, int i,
 *                    int s, int sign)
 *   php's DateTime::add/sub: month arithmetic with linear day/time overflow
 *   (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset. */
static int vm_builtin_dt_civil_add(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iTs,iLocal,iDays,iSecs,y0,moT,dayCount;
	sxi32 iOff;
	int mo0,d0,iSign;
	sxi64 y,m,d,h,i,s;
	if( nArg < 9 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iTs   = ph7_value_to_int64(apArg[0]);
	iOff  = (sxi32)ph7_value_to_int64(apArg[1]);
	y     = ph7_value_to_int64(apArg[2]);
	m     = ph7_value_to_int64(apArg[3]);
	d     = ph7_value_to_int64(apArg[4]);
	h     = ph7_value_to_int64(apArg[5]);
	i     = ph7_value_to_int64(apArg[6]);
	s     = ph7_value_to_int64(apArg[7]);
	iSign = ph7_value_to_int(apArg[8]) < 0 ? -1 : 1;
	iLocal = iTs + iOff;
	iDays  = DtFloorDiv(iLocal,86400);
	iSecs  = iLocal - iDays*86400;
	DtCivilFromDays(iDays,&y0,&mo0,&d0);
	y0 += iSign * y;
	moT = (sxi64)(mo0 - 1) + iSign * m;
	y0 += DtFloorDiv(moT,12);
	moT -= DtFloorDiv(moT,12) * 12;
	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;
	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);
	ph7_result_int64(pCtx,iLocal - iOff);
	return PH7_OK;
}
/* array __dt_civil_diff(int ts1, int off1, int ts2)
 *   -> [y,m,d,h,i,s,days,invert]: timelib's breakdown — field-wise deltas in
 *   the FIRST operand's offset, then borrow seconds→minutes→hours→days, then
 *   the day borrow walks whole months backward from the later date (that walk
 *   is why Jan 31 → Mar 02 reports m=0 d=30, not "1 month"). */
static int vm_builtin_dt_civil_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iTs1,iTs2,iA,iB,iLa,iLb,daysA,daysB,yA,yB;
	sxi32 iOff;
	int moA,dA,moB,dB,bInvert;
	sxi64 sA,sB,y,m,d,h,i,s;
	ph7_value *pArr,*pV;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iTs1 = ph7_value_to_int64(apArg[0]);
	iOff = (sxi32)ph7_value_to_int64(apArg[1]);
	iTs2 = ph7_value_to_int64(apArg[2]);
	bInvert = iTs1 > iTs2;
	iA = bInvert ? iTs2 : iTs1;
	iB = bInvert ? iTs1 : iTs2;
	iLa = iA + iOff;
	iLb = iB + iOff;
	daysA = DtFloorDiv(iLa,86400);
	daysB = DtFloorDiv(iLb,86400);
	sA = iLa - daysA*86400;
	sB = iLb - daysB*86400;
	DtCivilFromDays(daysA,&yA,&moA,&dA);
	DtCivilFromDays(daysB,&yB,&moB,&dB);
	s = (sB % 60) - (sA % 60);
	i = ((sB / 60) % 60) - ((sA / 60) % 60);
	h = (sB / 3600) - (sA / 3600);
	d = dB - dA;
	m = moB - moA;
	y = yB - yA;
	if( s < 0 ){ s += 60; i--; }
	if( i < 0 ){ i += 60; h--; }
	if( h < 0 ){ h += 24; d--; }
	while( d < 0 ){
		moB--;
		if( moB < 1 ){ moB = 12; yB--; }
		d += DtDaysInMonth(yB,moB);
		m--;
	}
	if( m < 0 ){ m += 12; y--; }
	pArr = ph7_context_new_array(pCtx);
	pV = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pV == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_value_int64(pV,y);  ph7_array_add_elem(pArr,0,pV);
	ph7_value_int64(pV,m);  ph7_array_add_elem(pArr,0,pV);
	ph7_value_int64(pV,d);  ph7_array_add_elem(pArr,0,pV);
	ph7_value_int64(pV,h);  ph7_array_add_elem(pArr,0,pV);
	ph7_value_int64(pV,i);  ph7_array_add_elem(pArr,0,pV);
	ph7_value_int64(pV,s);  ph7_array_add_elem(pArr,0,pV);
	ph7_value_int64(pV,(iB - iA) / 86400); ph7_array_add_elem(pArr,0,pV);
	ph7_value_int64(pV,bInvert); ph7_array_add_elem(pArr,0,pV);
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/* int __dt_isodate(int ts, int off, int y, int w, int dow)
 *   setISODate: jump to ISO year/week/weekday, preserving the time of day. */
static int vm_builtin_dt_isodate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iTs,iLocal,iTod,jan4,monday1,target,y;
	sxi32 iOff;
	sxi64 w,dow;
	int isoDow;
	if( nArg < 5 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iTs = ph7_value_to_int64(apArg[0]);
	iOff = (sxi32)ph7_value_to_int64(apArg[1]);
	y   = ph7_value_to_int64(apArg[2]);
	w   = ph7_value_to_int64(apArg[3]);
	dow = ph7_value_to_int64(apArg[4]);
	iLocal = iTs + iOff;
	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;
	jan4 = DtDaysFromCivil(y,1,4);
	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;
	monday1 = jan4 - (isoDow - 1);
	target = monday1 + (w - 1)*7 + (dow - 1);
	ph7_result_int64(pCtx,target*86400 + iTod - iOff);
	return PH7_OK;
}
/* Consume nMin..nMax digits from *pz; returns count consumed (0 = failure) */
static int DtEatDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)
{
	const char *z = *pz;
	sxi64 v = 0;
	int n = 0;
	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){
		v = v*10 + (z[0] - '0');
		z++;
		n++;
	}
	if( n < nMin ){
		return 0;
	}
	*pz = z;
	*pVal = v;
	return n;
}
/* timelib_get_nr's recovery: skip non-digits hunting for the field.
 * Returns 1 = found+read, 0 = digits present but short, -1 = exhausted. */
static int DtHuntDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)
{
	const char *z = *pz;
	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }
	*pz = z;
	if( z >= zEnd ){
		return -1;
	}
	return DtEatDigits(pz,zEnd,nMin,nMax,pVal) ? 1 : 0;
}
/* Case-insensitive name-table lookup; returns 1-based index or 0 */
static int DtEatName(const char **pz,const char *zEnd,const char **azNames,int nNames)
{
	int k;
	for( k = 0 ; k < nNames ; k++ ){
		int n = (int)SyStrlen(azNames[k]);
		if( zEnd - *pz >= n && SyStrnicmp(*pz,azNames[k],(sxu32)n) == 0 ){
			*pz += n;
			return k + 1;
		}
	}
	return 0;
}
/* mixed __dt_from_format(string fmt, string input, int nowTs, int defOff)
 *   php's DateTime::createFromFormat engine. Success: [ts, off, offKind, name]
 *   where offKind 0=none-parsed, 1=numeric offset, 2=literal Z, 3=named id.
 *   Failure: "POS\tMESSAGE" (timelib's message strings; PHL reports the FIRST
 *   error where php may accumulate several — recorded). A trailing-data
 *   warning rides as [4]=pos, [5]=msg on the success array. */
static int vm_builtin_dt_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};
	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",
		"thursday","friday","saturday"};
	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",
		"aug","sep","oct","nov","dec"};
	static const char *azMonFull[] = {"january","february","march","april",
		"may","june","july","august","september","october","november","december"};
	const char *zFmt,*zIn,*zEnd,*zInEnd,*z;
	int nFmt,nIn;
	sxi64 iNow,v;
	sxi32 iDefOff;
	/* -1 == unset */
	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;
	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;
	int iOffKind = 0;
	sxi32 iOffVal = 0;
	char zName[16];
	const char *zErr = 0;
	const char *aWarnMsg[3];
	int aWarnPos[3];
	int nWarn = 0,bAborted = 0;
	const char *aErrMsg[8];
	int aErrPos[8];
	int nErr = 0,nErrKept = 0;
	if( nArg < 4 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFmt = ph7_value_to_string(apArg[0],&nFmt);
	zIn  = ph7_value_to_string(apArg[1],&nIn);
	iNow = ph7_value_to_int64(apArg[2]);
	iDefOff = (sxi32)ph7_value_to_int64(apArg[3]);
	zEnd = &zFmt[nFmt];
	zInEnd = &zIn[nIn];
	z = zIn;
	zName[0] = 0;
#define DT_FF_LOGERR(iPos,zMsg) \
	{ int _p = (iPos),_k,_f = -1; \
	  nErr++; \
	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \
	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \
	  else if( nErrKept < 8 ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }
	while( zFmt < zEnd ){
		char c = zFmt[0];
		zFmt++;
		zErr = 0;
		if( c == '!' ){
			y = 1970; mo = 1; d = 1; h = 0; mi = 0; s = 0;
			h12 = -1; iMeridiem = -1;
			continue;
		}
		if( c == '|' ){ bPipe = 1; continue; }
		if( c == '+' ){ bPlus = 1; continue; }
		if( z >= zInEnd ){
			/* timelib aborts the scan once input is exhausted */
			DT_FF_LOGERR(nIn,"Not enough data available to satisfy format");
			break;
		}
		switch( c ){
		case 'd': case 'j':
			if( !DtEatDigits(&z,zInEnd,1,2,&d) ){
				DT_FF_LOGERR((int)(z - zIn),"A two digit day could not be found");
				if( DtHuntDigits(&z,zInEnd,1,2,&d) < 0 ){
					DT_FF_LOGERR(nIn,"A two digit day could not be found");
				}
			}
			break;
		case 'D':
			if( !DtEatName(&z,zInEnd,azDay3,7) ){
				zErr = "A textual day could not be found";
			}
			break;
		case 'l':
			if( !DtEatName(&z,zInEnd,azDayFull,7) ){
				zErr = "A textual day could not be found";
			}
			break;
		case 'S':
			/* ordinal suffix: st nd rd th */
			if( zInEnd-z >= 2 && ((z[0]=='s'&&z[1]=='t')||(z[0]=='n'&&z[1]=='d')
			 ||(z[0]=='r'&&z[1]=='d')||(z[0]=='t'&&z[1]=='h')) ){
				z += 2;
			}
			break;
		case 'm': case 'n':
			if( !DtEatDigits(&z,zInEnd,1,2,&mo) ){
				DT_FF_LOGERR((int)(z - zIn),"A two digit month could not be found");
				if( DtHuntDigits(&z,zInEnd,1,2,&mo) < 0 ){
					DT_FF_LOGERR(nIn,"A two digit month could not be found");
				}
			}
			break;
		case 'M':{
			int k = DtEatName(&z,zInEnd,azMon3,12);
			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }
			break;
				 }
		case 'F':{
			int k = DtEatName(&z,zInEnd,azMonFull,12);
			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }
			break;
				 }
		case 'y':
			if( DtEatDigits(&z,zInEnd,2,2,&y) ){
				y += (y <= 69) ? 2000 : 1900;
			}else{
				DT_FF_LOGERR((int)(z - zIn),"A two digit year could not be found");
				if( DtHuntDigits(&z,zInEnd,2,2,&y) < 0 ){
					DT_FF_LOGERR(nIn,"A two digit year could not be found");
				}else if( y >= 0 ){
					y += (y <= 69) ? 2000 : 1900;
				}
			}
			break;
		case 'Y':{
			int neg = 0;
			if( z < zInEnd && (z[0]=='-'||z[0]=='+') ){ neg = (z[0]=='-'); z++; }
			if( DtEatDigits(&z,zInEnd,1,4,&y) ){
				if( neg ){ y = -y; }
			}else{
				DT_FF_LOGERR((int)(z - zIn),"A four digit year could not be found");
				if( DtHuntDigits(&z,zInEnd,1,4,&y) < 0 ){
					DT_FF_LOGERR(nIn,"A four digit year could not be found");
				}
			}
			break;
				 }
		case 'H': case 'G':
			if( !DtEatDigits(&z,zInEnd,1,2,&h) ){
				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");
				if( DtHuntDigits(&z,zInEnd,1,2,&h) < 0 ){
					DT_FF_LOGERR(nIn,"A two digit hour could not be found");
				}
			}
			break;
		case 'h': case 'g':
			if( !DtEatDigits(&z,zInEnd,1,2,&h12) ){
				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");
				if( DtHuntDigits(&z,zInEnd,1,2,&h12) < 0 ){
					DT_FF_LOGERR(nIn,"A two digit hour could not be found");
				}
			}
			break;
		case 'i':
			if( !DtEatDigits(&z,zInEnd,1,2,&mi) ){
				DT_FF_LOGERR((int)(z - zIn),"A two digit minute could not be found");
				if( DtHuntDigits(&z,zInEnd,1,2,&mi) < 0 ){
					DT_FF_LOGERR(nIn,"A two digit minute could not be found");
				}
			}
			break;
		case 's':
			if( !DtEatDigits(&z,zInEnd,1,2,&s) ){
				DT_FF_LOGERR((int)(z - zIn),"A two digit second could not be found");
				if( DtHuntDigits(&z,zInEnd,1,2,&s) < 0 ){
					DT_FF_LOGERR(nIn,"A two digit second could not be found");
				}
			}
			break;
		case 'u':
			/* micro parsed then dropped: PHL keeps whole seconds (recorded) */
			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){
				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");
				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){
					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");
				}
			}
			break;
		case 'v':
			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){
				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");
				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){
					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");
				}
			}
			break;
		case 'a': case 'A':{
			static const char *azMer[] = {"am","pm","a.m.","p.m."};
			int k = DtEatName(&z,zInEnd,azMer,4);
			if( k ){
				iMeridiem = ((k - 1) & 1);
			}else{
				zErr = "A meridian could not be found";
			}
			break;
				 }
		case 'U':{
			int neg = 0;
			if( z < zInEnd && z[0]=='-' ){ neg = 1; z++; }
			if( DtEatDigits(&z,zInEnd,1,19,&uVal) ){
				if( neg ){ uVal = -uVal; }
				bHasU = 1;
			}else{
				DT_FF_LOGERR((int)(z - zIn),"A unix timestamp could not be found");
				if( DtHuntDigits(&z,zInEnd,1,19,&uVal) < 0 ){
					DT_FF_LOGERR(nIn,"A unix timestamp could not be found");
				}else{
					if( neg ){ uVal = -uVal; }
					bHasU = 1;
				}
			}
			break;
				 }
		case 'e': case 'T':{
			static const char *azZone[] = {"UTC","GMT","Z"};
			int k = DtEatName(&z,zInEnd,azZone,3);
			if( k == 3 ){
				iOffKind = 2; iOffVal = 0;
			}else if( k ){
				iOffKind = 3; iOffVal = 0;
				SyMemcpy(azZone[k-1],zName,4);
			}else if( z < zInEnd && (z[0]=='+' || z[0]=='-') ){
				goto parse_num_off;
			}else{
				zErr = "The timezone could not be found in the database";
			}
			break;
				 }
		case 'O': case 'P':
parse_num_off:	{
			int sign,oh,om = 0;
			sxi64 t;
			if( z >= zInEnd || (z[0] != '+' && z[0] != '-') ){
				zErr = "The timezone could not be found in the database";
				break;
			}
			sign = (z[0]=='-') ? -1 : 1;
			z++;
			if( !DtEatDigits(&z,zInEnd,2,2,&t) ){
				zErr = "The timezone could not be found in the database";
				break;
			}
			oh = (int)t;
			if( z < zInEnd && z[0]==':' ){ z++; }
			if( DtEatDigits(&z,zInEnd,2,2,&t) ){ om = (int)t; }
			iOffKind = 1;
			iOffVal = sign * (oh*3600 + om*60);
			break;
				 }
		case '?':
			if( z < zInEnd ){ z++; }
			break;
		case '*':
			/* skip input until the next separator byte */
			while( z < zInEnd && !SyisDigit(z[0]) && z[0] != ';' && z[0] != ':'
			 && z[0] != '/' && z[0] != '.' && z[0] != ',' && z[0] != '-'
			 && z[0] != '(' && z[0] != ')' && z[0] != ' ' ){
				z++;
			}
			break;
		case '#':
			if( z < zInEnd && (z[0]==';'||z[0]==':'||z[0]=='/'||z[0]=='.'
			 ||z[0]==','||z[0]=='-'||z[0]=='('||z[0]==')') ){
				z++;
			}else{
				zErr = "The separation symbol could not be found";
			}
			break;
		case '\\':
			if( zFmt < zEnd ){
				if( z < zInEnd && z[0] == zFmt[0] ){
					z++;
					zFmt++;
				}else{
					/* a literal mismatch aborts timelib's scan */
					DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");
					zFmt = zEnd;
					bAborted = 1;
				}
			}
			break;
		case ';': case ':': case '/': case '.': case ',': case '-':
		case '(' : case ')':
			if( z < zInEnd && z[0] == c ){
				z++;
			}else{
				/* timelib logs BOTH messages (count +2, last-wins on the
				 * position), consumes the offending byte, and keeps going */
				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");
				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");
				z++;
			}
			break;
		case ' ':
			if( z < zInEnd && (z[0] == ' ' || z[0] == '\t') ){
				z++;
			}else{
				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");
				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");
				z++;
			}
			break;
		default:
			/* any other format byte must match the input verbatim; a mismatch
			 * aborts timelib's scan */
			if( z < zInEnd && z[0] == c ){
				z++;
			}else{
				DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");
				zFmt = zEnd;
				bAborted = 1;
			}
			break;
		}
		if( zErr ){
			/* name/zone/separator mismatch: log and keep scanning (timelib) */
			DT_FF_LOGERR((int)(z - zIn),zErr);
		}
	}
	if( z < zInEnd && !bAborted ){
		if( bPlus ){
			/* '+' downgrades trailing data to a warning */
			aWarnPos[nWarn] = (int)(z - zIn);
			aWarnMsg[nWarn] = "Trailing data";
			nWarn++;
		}else{
			DT_FF_LOGERR((int)(z - zIn),"Trailing data");
		}
	}
	if( nErr > 0 ){
		SyBlob sOut;
		int k;
		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
		SyBlobFormat(&sOut,"%d",nErr);
		for( k = 0 ; k < nErrKept ; k++ ){
			SyBlobFormat(&sOut,"\n%d\t%s",aErrPos[k],aErrMsg[k]);
		}
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
		SyBlobRelease(&sOut);
		return PH7_OK;
	}
	if( bPipe ){
		if( y < 0 ){ y = 1970; }
		if( mo < 0 ){ mo = 1; }
		if( d < 0 ){ d = 1; }
		if( h < 0 && h12 < 0 ){ h = 0; }
		if( mi < 0 ){ mi = 0; }
		if( s < 0 ){ s = 0; }
	}
	{
		/* remaining unset fields come from "now" in the default offset */
		sxi64 iLocal = iNow + iDefOff;
		sxi64 days = DtFloorDiv(iLocal,86400);
		sxi64 secs = iLocal - days*86400;
		sxi64 ny;
		int nmo,nd;
		DtCivilFromDays(days,&ny,&nmo,&nd);
		if( y < 0 ){ y = ny; }
		if( mo < 0 ){ mo = nmo; }
		if( d < 0 ){ d = nd; }
		if( h12 >= 0 ){
			h = (h12 % 12) + ((iMeridiem == 1) ? 12 : 0);
		}
		/* php: parsing a time component zeroes the finer unset units */
		if( h >= 0 ){
			if( mi < 0 ){ mi = 0; }
			if( s < 0 ){ s = 0; }
		}else if( mi >= 0 ){
			if( s < 0 ){ s = 0; }
		}
		if( h < 0 ){ h = secs / 3600; }
		if( mi < 0 ){ mi = (secs / 60) % 60; }
		if( s < 0 ){ s = secs % 60; }
	}
	/* php validates the RESOLVED fields and warns (parse still succeeds,
	 * values roll over via civil arithmetic) */
	if( mo < 1 || mo > 12 || d < 1 || d > DtDaysInMonth(y,(int)mo) ){
		if( nWarn < 3 ){
			aWarnPos[nWarn] = nIn;
			aWarnMsg[nWarn] = "The parsed date was invalid";
			nWarn++;
		}
	}
	if( h > 24 || mi > 59 || s > 59 ){
		if( nWarn < 3 ){
			aWarnPos[nWarn] = nIn;
			aWarnMsg[nWarn] = "The parsed time was invalid";
			nWarn++;
		}
	}
	{
		ph7_value *pArr = ph7_context_new_array(pCtx);
		ph7_value *pV = ph7_context_new_scalar(pCtx);
		sxi64 iTs;
		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;
		if( pArr == 0 || pV == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		if( bHasU ){
			iTs = uVal;
			iUseOff = 0;
			iOffKind = 1;
		}else{
			iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);
		}
		ph7_value_int64(pV,iTs);           ph7_array_add_elem(pArr,0,pV);
		ph7_value_int64(pV,iUseOff);       ph7_array_add_elem(pArr,0,pV);
		ph7_value_int64(pV,iOffKind);      ph7_array_add_elem(pArr,0,pV);
		ph7_value_string(pV,zName,-1);     ph7_array_add_elem(pArr,0,pV);
		{
			int k;
			for( k = 0 ; k < nWarn ; k++ ){
				ph7_value_int64(pV,aWarnPos[k]);
				ph7_array_add_elem(pArr,0,pV);
				ph7_value_string(pV,aWarnMsg[k],-1);
				ph7_array_add_elem(pArr,0,pV);
			}
		}
		ph7_result_value(pCtx,pArr);
	}
	return PH7_OK;
}
/*
 * The embedded DateTime library. Timezone scope: UTC + fixed offsets.
 */
static const char zDateTimeLib[] =
"class DateException extends Exception {}"
"class DateMalformedStringException extends DateException {}"
"class DateInvalidTimeZoneException extends DateException {}"
"class DateMalformedIntervalStringException extends DateException {}"
"class DateMalformedPeriodStringException extends DateException {}"
"interface DateTimeInterface {"
" const ATOM = 'Y-m-d\\TH:i:sP';"
" const COOKIE = 'l, d-M-Y H:i:s T';"
" const ISO8601 = 'Y-m-d\\TH:i:sO';"
" const ISO8601_EXPANDED = 'X-m-d\\TH:i:sP';"
" const RFC822 = 'D, d M y H:i:s O';"
" const RFC850 = 'l, d-M-y H:i:s T';"
" const RFC1036 = 'D, d M y H:i:s O';"
" const RFC1123 = 'D, d M Y H:i:s O';"
" const RFC7231 = 'D, d M Y H:i:s \\G\\M\\T';"
" const RFC2822 = 'D, d M Y H:i:s O';"
" const RFC3339 = 'Y-m-d\\TH:i:sP';"
" const RFC3339_EXTENDED = 'Y-m-d\\TH:i:s.vP';"
" const RSS = 'D, d M Y H:i:s O';"
" const W3C = 'Y-m-d\\TH:i:sP';"
"}"
"class DateTimeZone {"
" private $__dtzOff = 0;"
" private $__dtzName = 'UTC';"
" public function __construct($timezone = 'UTC'){"
"  $tz = (string)$timezone;"
"  if( strcasecmp($tz, 'UTC') === 0 ){"
"   $this->__dtzOff = 0; $this->__dtzName = 'UTC';"
"   return;"
"  }"
"  if( $tz === 'Z' ){"
"   $this->__dtzOff = 0; $this->__dtzName = 'Z';"
"   return;"
"  }"
"  if( strcasecmp($tz, 'GMT') === 0 ){"
"   $this->__dtzOff = 0; $this->__dtzName = 'GMT';"
"   return;"
"  }"
"  $m = null;"
"  if( preg_match('/^([+-])(\\d{2}):?(\\d{2})$/', $tz, $m) ){"
"   $off = ((int)$m[2]) * 3600 + ((int)$m[3]) * 60;"
"   if( $m[1] === '-' ){ $off = -$off; }"
"   $this->__dtzOff = $off;"
"   $this->__dtzName = $m[1] . $m[2] . ':' . $m[3];"
"   return;"
"  }"
"  throw new DateInvalidTimeZoneException("
"   'DateTimeZone::__construct(): Unknown or bad timezone (' . $tz . ')');"
" }"
" public function getName(){ return $this->__dtzName; }"
" public function getOffset($datetime = null){ return $this->__dtzOff; }"
"}"
"trait __DtCoreT {"
" private $__dtTs = 0;"
" private $__dtOff = 0;"
" private $__dtName = 'UTC';"
" private function __dtInit($datetime, $timezone){"
"  $off = 0; $name = __dt_default_tz();"
"  if( $timezone !== null ){"
"   $off = $timezone->getOffset($this);"
"   $name = $timezone->getName();"
"  }"
"  $r = __dt_parse((string)$datetime, __dt_now(), $off);"
"  if( is_string($r) ){ throw new DateMalformedStringException($r); }"
"  $this->__dtTs = $r[0];"
"  if( $r[2] ){"
"   $this->__dtOff = $r[1];"
"   $this->__dtName = $r[2] === 2 ? 'Z' : $this->__dtOffName($r[1]);"
"  }else{"
"   $this->__dtOff = $off;"
"   $this->__dtName = $name;"
"  }"
" }"
" private function __dtOffName($off){"
"  $s = $off < 0 ? '-' : '+';"
"  $a = $off < 0 ? -$off : $off;"
"  return $s . sprintf('%02d:%02d', intdiv($a, 3600), intdiv($a % 3600, 60));"
" }"
" public function format($format){ return __dt_format($this->__dtTs, $this->__dtOff, $this->__dtName, (string)$format); }"
" public function getTimestamp(){ return $this->__dtTs; }"
" public function getOffset(){ return $this->__dtOff; }"
" public function getTimezone(){ return new DateTimeZone($this->__dtName); }"
" public function diff($targetObject, $absolute = false){"
"  $r = __dt_civil_diff($this->__dtTs, $this->__dtOff, $targetObject->getTimestamp());"
"  $iv = new DateInterval('P0D');"
"  $iv->y = $r[0]; $iv->m = $r[1]; $iv->d = $r[2];"
"  $iv->h = $r[3]; $iv->i = $r[4]; $iv->s = $r[5];"
"  $iv->days = $r[6];"
"  $iv->invert = $absolute ? 0 : $r[7];"
"  return $iv;"
" }"
" private function __dtAddTs($interval, $sign){"
"  if( $interval->invert ){ $sign = -$sign; }"
"  return __dt_civil_add($this->__dtTs, $this->__dtOff, $interval->y, $interval->m,"
"   $interval->d, $interval->h, $interval->i, $interval->s, $sign);"
" }"
" private static function __dtFromFormat($format, $datetime, $timezone, $class){"
"  $off = 0; $name = __dt_default_tz();"
"  if( $timezone !== null ){"
"   $off = $timezone->getOffset(null);"
"   $name = $timezone->getName();"
"  }"
"  $r = __dt_from_format((string)$format, (string)$datetime, __dt_now(), $off);"
"  if( is_string($r) ){"
"   $lines = explode(\"\\n\", $r);"
"   $errs = [];"
"   $nl = count($lines);"
"   for( $k = 1; $k < $nl; $k++ ){"
"    $p = strpos($lines[$k], \"\\t\");"
"    $errs[(int)substr($lines[$k], 0, $p)] = substr($lines[$k], $p + 1);"
"   }"
"   DateTime::$__dtLastErr = ['warning_count' => 0, 'warnings' => [],"
"    'error_count' => (int)$lines[0], 'errors' => $errs];"
"   return false;"
"  }"
"  if( isset($r[4]) ){"
"   $warns = [];"
"   $wc = 0;"
"   for( $k = 4; isset($r[$k]); $k += 2 ){"
"    $warns[$r[$k]] = $r[$k + 1];"
"    $wc++;"
"   }"
"   DateTime::$__dtLastErr = ['warning_count' => $wc, 'warnings' => $warns,"
"    'error_count' => 0, 'errors' => []];"
"  }else{"
"   DateTime::$__dtLastErr = false;"
"  }"
"  $obj = new $class('@0');"
"  $obj->__dtTs = $r[0];"
"  if( $r[2] === 0 ){ $obj->__dtOff = $off; $obj->__dtName = $name; }"
"  elseif( $r[2] === 2 ){ $obj->__dtOff = 0; $obj->__dtName = 'Z'; }"
"  elseif( $r[2] === 3 ){ $obj->__dtOff = $r[1]; $obj->__dtName = $r[3]; }"
"  else { $obj->__dtOff = $r[1]; $obj->__dtName = $obj->__dtOffName($r[1]); }"
"  return $obj;"
" }"
" private static function __dtCopyOf($object, $class){"
"  $d = new $class('@0');"
"  $d->__dtTs = $object->getTimestamp();"
"  $d->__dtOff = $object->getOffset();"
"  $d->__dtName = $object->getTimezone()->getName();"
"  return $d;"
" }"
"}"
"class DateTime implements DateTimeInterface {"
" use __DtCoreT;"
" public function __construct($datetime = 'now', $timezone = null){"
"  $this->__dtInit($datetime, $timezone);"
" }"
" public function modify($modifier){"
"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"
"  if( is_string($r) ){ throw new DateMalformedStringException('DateTime::modify(): ' . $r); }"
"  $this->__dtTs = $r[0];"
"  return $this;"
" }"
" public function setTimestamp($timestamp){ $this->__dtTs = (int)$timestamp; return $this; }"
" public function setTimezone($timezone){"
"  $this->__dtOff = $timezone->getOffset($this);"
"  $this->__dtName = $timezone->getName();"
"  return $this;"
" }"
" public function setDate($year, $month, $day){"
"  $this->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"
"  return $this;"
" }"
" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"
"  $this->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"
"  return $this;"
" }"
" public function add($interval){ $this->__dtTs = $this->__dtAddTs($interval, 1); return $this; }"
" public function sub($interval){ $this->__dtTs = $this->__dtAddTs($interval, -1); return $this; }"
" public function setISODate($year, $week, $dayOfWeek = 1){"
"  $this->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"
"  return $this;"
" }"
" public static $__dtLastErr = false;"
" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"
" public static function createFromFormat($format, $datetime, $timezone = null){"
"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTime');"
" }"
" public static function createFromImmutable($object){ return self::__dtCopyOf($object, 'DateTime'); }"
" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTime'); }"
"}"
"class DateTimeImmutable implements DateTimeInterface {"
" use __DtCoreT;"
" public function __construct($datetime = 'now', $timezone = null){"
"  $this->__dtInit($datetime, $timezone);"
" }"
" public function modify($modifier){"
"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"
"  if( is_string($r) ){ throw new DateMalformedStringException('DateTimeImmutable::modify(): ' . $r); }"
"  $c = clone $this;"
"  $c->__dtTs = $r[0];"
"  return $c;"
" }"
" public function setTimestamp($timestamp){ $c = clone $this; $c->__dtTs = (int)$timestamp; return $c; }"
" public function setTimezone($timezone){"
"  $c = clone $this;"
"  $c->__dtOff = $timezone->getOffset($this);"
"  $c->__dtName = $timezone->getName();"
"  return $c;"
" }"
" public function setDate($year, $month, $day){"
"  $c = clone $this;"
"  $c->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"
"  return $c;"
" }"
" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"
"  $c = clone $this;"
"  $c->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"
"  return $c;"
" }"
" public function add($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, 1); return $c; }"
" public function sub($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, -1); return $c; }"
" public function setISODate($year, $week, $dayOfWeek = 1){"
"  $c = clone $this;"
"  $c->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"
"  return $c;"
" }"
" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"
" public static function createFromFormat($format, $datetime, $timezone = null){"
"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTimeImmutable');"
" }"
" public static function createFromMutable($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"
" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"
"}"
"function date_create($datetime = 'now', $timezone = null){"
" try { return new DateTime($datetime, $timezone); } catch (Exception $e) { return false; }"
"}"
"function date_create_immutable($datetime = 'now', $timezone = null){"
" try { return new DateTimeImmutable($datetime, $timezone); } catch (Exception $e) { return false; }"
"}"
"class DateInterval {"
" public $y = 0;"
" public $m = 0;"
" public $d = 0;"
" public $h = 0;"
" public $i = 0;"
" public $s = 0;"
" public $f = 0;"
" public $invert = 0;"
" public $days = false;"
" public $from_string = false;"
" public function __construct($duration = 'P0D'){"
"  $dur = (string)$duration;"
"  $mm = null;"
"  if( strlen($dur) < 2 || substr($dur, -1) === 'T'"
"   || !preg_match('/^P(?:(\\d+)Y)?(?:(\\d+)M)?(?:(\\d+)W)?(?:(\\d+)D)?(?:T(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?)?$/', $dur, $mm) ){"
"   throw new DateMalformedIntervalStringException('Unknown or bad format (' . $dur . ')');"
"  }"
"  $this->y = (int)($mm[1] ?? 0);"
"  $this->m = (int)($mm[2] ?? 0);"
"  $this->d = (int)($mm[4] ?? 0) + 7 * (int)($mm[3] ?? 0);"
"  $this->h = (int)($mm[5] ?? 0);"
"  $this->i = (int)($mm[6] ?? 0);"
"  $this->s = (int)($mm[7] ?? 0);"
" }"
" public static function createFromDateString($datetime){"
"  $s = trim((string)$datetime);"
"  $iv = new DateInterval('P0D');"
"  $rest = $s;"
"  $any = false;"
"  while( $rest !== '' ){"
"   $mm = null;"
"   if( !preg_match('/^[\\s,+]*([+-]?\\d+)\\s*(sec|secs|second|seconds|min|mins|minute|minutes|hour|hours|day|days|week|weeks|fortnight|fortnights|month|months|year|years)\\b/i', $rest, $mm) ){"
"    throw new DateMalformedIntervalStringException("
"     'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"
"   }"
"   $n = (int)$mm[1];"
"   $u = strtolower($mm[2]);"
"   if( $u === 'sec' || $u === 'secs' || $u === 'second' || $u === 'seconds' ){ $iv->s += $n; }"
"   elseif( $u === 'min' || $u === 'mins' || $u === 'minute' || $u === 'minutes' ){ $iv->i += $n; }"
"   elseif( $u === 'hour' || $u === 'hours' ){ $iv->h += $n; }"
"   elseif( $u === 'day' || $u === 'days' ){ $iv->d += $n; }"
"   elseif( $u === 'week' || $u === 'weeks' ){ $iv->d += 7 * $n; }"
"   elseif( $u === 'fortnight' || $u === 'fortnights' ){ $iv->d += 14 * $n; }"
"   elseif( $u === 'month' || $u === 'months' ){ $iv->m += $n; }"
"   else { $iv->y += $n; }"
"   $any = true;"
"   $rest = ltrim(substr($rest, strlen($mm[0])));"
"  }"
"  if( !$any ){"
"   throw new DateMalformedIntervalStringException("
"    'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"
"  }"
"  return $iv;"
" }"
" public function format($format){"
"  $f = (string)$format;"
"  $out = '';"
"  $n = strlen($f);"
"  for( $k = 0; $k < $n; $k++ ){"
"   $c = $f[$k];"
"   if( $c !== '%' ){ $out .= $c; continue; }"
"   $k++;"
"   if( $k >= $n ){ $out .= '%'; break; }"
"   $t = $f[$k];"
"   if( $t === 'Y' ){ $out .= sprintf('%02d', $this->y); }"
"   elseif( $t === 'y' ){ $out .= $this->y; }"
"   elseif( $t === 'M' ){ $out .= sprintf('%02d', $this->m); }"
"   elseif( $t === 'm' ){ $out .= $this->m; }"
"   elseif( $t === 'D' ){ $out .= sprintf('%02d', $this->d); }"
"   elseif( $t === 'd' ){ $out .= $this->d; }"
"   elseif( $t === 'H' ){ $out .= sprintf('%02d', $this->h); }"
"   elseif( $t === 'h' ){ $out .= $this->h; }"
"   elseif( $t === 'I' ){ $out .= sprintf('%02d', $this->i); }"
"   elseif( $t === 'i' ){ $out .= $this->i; }"
"   elseif( $t === 'S' ){ $out .= sprintf('%02d', $this->s); }"
"   elseif( $t === 's' ){ $out .= $this->s; }"
"   elseif( $t === 'F' ){ $out .= sprintf('%06d', (int)round($this->f * 1000000)); }"
"   elseif( $t === 'f' ){ $out .= (int)round($this->f * 1000000); }"
"   elseif( $t === 'R' ){ $out .= $this->invert ? '-' : '+'; }"
"   elseif( $t === 'r' ){ $out .= $this->invert ? '-' : ''; }"
"   elseif( $t === 'a' ){ $out .= $this->days === false ? '(unknown)' : $this->days; }"
"   elseif( $t === '%' ){ $out .= '%'; }"
"   else { $out .= $t; }"
"  }"
"  return $out;"
" }"
"}"
"class DatePeriod implements IteratorAggregate {"
" const EXCLUDE_START_DATE = 1;"
" const INCLUDE_END_DATE = 2;"
" public $start = null;"
" public $current = null;"
" public $end = null;"
" public $interval = null;"
" public $recurrences = 1;"
" public $include_start_date = true;"
" public $include_end_date = false;"
" private $__dpN = null;"
" public function __construct($start, $interval = null, $end = null, $options = 0){"
"  if( is_string($start) ){"
"   $mm = null;"
"   if( !preg_match('/^R(\\d+)\\/(.+)\\/(P.+)$/', $start, $mm) ){"
"    throw new DateMalformedPeriodStringException("
"     'DatePeriod::__construct(): Unknown or bad format (' . $start . ')');"
"   }"
"   $options = is_int($interval) ? $interval : 0;"
"   $this->start = new DateTimeImmutable($mm[2]);"
"   $this->interval = new DateInterval($mm[3]);"
"   $this->__dpN = (int)$mm[1];"
"   $this->recurrences = $this->__dpN + 1;"
"  }else{"
"   $this->start = clone $start;"
"   $this->interval = $interval;"
"   if( is_int($end) ){"
"    $this->__dpN = $end;"
"    $this->recurrences = $end + 1;"
"   }else{"
"    $this->end = $end === null ? null : (clone $end);"
"   }"
"  }"
"  $this->include_start_date = !((int)$options & 1);"
"  $this->include_end_date = ((int)$options & 2) !== 0;"
" }"
" public static function createFromISO8601String($specification, $options = 0){"
"  return new DatePeriod((string)$specification, (int)$options);"
" }"
" public function getStartDate(){ return $this->start; }"
" public function getEndDate(){ return $this->end; }"
" public function getDateInterval(){ return $this->interval; }"
" public function getRecurrences(){ return $this->__dpN; }"
" public function getIterator(): Generator {"
"  $cur = $this->start;"
"  $iv = $this->interval;"
"  $k = 0;"
"  if( $this->end !== null ){"
"   $endTs = $this->end->getTimestamp();"
"   $first = true;"
"   while( true ){"
"    $ts = $cur->getTimestamp();"
"    if( $this->include_end_date ? ($ts > $endTs) : ($ts >= $endTs) ){ break; }"
"    if( !$first || $this->include_start_date ){"
"     yield $k => (clone $cur);"
"     $k++;"
"    }"
"    $first = false;"
"    $next = clone $cur;"
"    $cur = $next->add($iv);"
"   }"
"   return;"
"  }"
"  $total = $this->__dpN + 1 + ($this->include_end_date ? 1 : 0);"
"  for( $j = 0; $j < $total; $j++ ){"
"   if( $j > 0 || $this->include_start_date ){"
"    yield $k => (clone $cur);"
"    $k++;"
"   }"
"   $next = clone $cur;"
"   $cur = $next->add($iv);"
"  }"
" }"
"}"
"function date_format($object, $format){ return $object->format($format); }"
"function date_modify($object, $modifier){"
" try { return $object->modify($modifier); } catch (Exception $e) { return false; }"
"}"
"function date_add($object, $interval){ return $object->add($interval); }"
"function date_sub($object, $interval){ return $object->sub($interval); }"
"function date_diff($baseObject, $targetObject, $absolute = false){"
" return $baseObject->diff($targetObject, $absolute);"
"}"
"function date_timestamp_get($object){ return $object->getTimestamp(); }"
"function date_timestamp_set($object, $timestamp){ return $object->setTimestamp($timestamp); }"
"function date_timezone_get($object){ return $object->getTimezone(); }"
"function date_timezone_set($object, $timezone){ return $object->setTimezone($timezone); }"
"function date_offset_get($object){ return $object->getOffset(); }"
"function date_date_set($object, $year, $month, $day){ return $object->setDate($year, $month, $day); }"
"function date_time_set($object, $hour, $minute, $second = 0, $microsecond = 0){"
" return $object->setTime($hour, $minute, $second, $microsecond);"
"}"
"function date_isodate_set($object, $year, $week, $dayOfWeek = 1){"
" return $object->setISODate($year, $week, $dayOfWeek);"
"}"
"function date_interval_create_from_date_string($datetime){"
" return DateInterval::createFromDateString($datetime);"
"}"
"function date_interval_format($object, $format){ return $object->format($format); }"
"function date_get_last_errors(){ return DateTime::getLastErrors(); }"
"function timezone_open($timezone){"
" try { return new DateTimeZone($timezone); } catch (Exception $e) { return false; }"
"}"
"function timezone_name_get($object){ return $object->getName(); }"
"function timezone_offset_get($object, $datetime){ return $object->getOffset($datetime); }"
;
/*
 * Install the DateTime family: thunks first, then the chunk. Called from
 * PH7_VmInit inside the bCompilingBuiltin window, after the Reflection
 * install (Exception must exist).
 */
PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "__dt_now",    vm_builtin_dt_now },
		{ "__dt_default_tz", vm_builtin_dt_default_tz },
		{ "__dt_civil_add",  vm_builtin_dt_civil_add },
		{ "__dt_civil_diff", vm_builtin_dt_civil_diff },
		{ "__dt_isodate",    vm_builtin_dt_isodate },
		{ "__dt_from_format", vm_builtin_dt_from_format },
		{ "__dt_parse",  vm_builtin_dt_parse },
		{ "__dt_format", vm_builtin_dt_format },
		{ "__dt_make",   vm_builtin_dt_make },
	};
	sxu32 n;
	/* php's date.timezone default */
	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));
	pVm->nDefTz = sizeof("UTC") - 1;
	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	return PH7_VmEvalBuiltinChunk(&(*pVm),zDateTimeLib,sizeof(zDateTimeLib)-1);
}

#endif /* PH7_DISABLE_BUILTIN_FUNC */

#ifdef PH7_DISABLE_BUILTIN_FUNC
/* Tiny build: no DateTime family (builtin layer disabled) */
PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){
	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));
	pVm->nDefTz = sizeof("UTC") - 1;
	return SXRET_OK;
}
#endif
