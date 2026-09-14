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
 * array|int hrtime(bool $as_number = false)
 *  The system's high-resolution time, counted from an arbitrary monotonic
 *  point in nanoseconds. Returns [seconds, nanoseconds] by default, or the
 *  total nanoseconds as an int when $as_number is true.
 */
PH7_PRIVATE int PH7_builtin_hrtime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 sec = 0,nsec = 0;
	int bAsNumber = 0;
	if( nArg > 0 ){
		bAsNumber = ph7_value_to_bool(apArg[0]);
	}
#if defined(CLOCK_MONOTONIC)
	{
		struct timespec ts;
		if( clock_gettime(CLOCK_MONOTONIC,&ts) == 0 ){
			sec  = (ph7_int64)ts.tv_sec;
			nsec = (ph7_int64)ts.tv_nsec;
		}
	}
#else
	{
		/* No monotonic clock available: fall back to the wall-clock microsecond
		 * source (embedder clock / gettimeofday). Coarser and not strictly
		 * monotonic, but keeps hrtime() usable off-Unix. */
		sytime sTime;
		DateNow(pCtx->pVm,&sTime);
		sec  = (ph7_int64)sTime.tm_sec;
		nsec = (ph7_int64)sTime.tm_usec * 1000;
	}
#endif
	if( bAsNumber ){
		ph7_result_int64(pCtx,sec * 1000000000LL + nsec);
	}else{
		ph7_value *pValue,*pArray;
		pArray = ph7_context_new_array(pCtx);
		pValue = ph7_context_new_scalar(pCtx);
		if( pArray == 0 || pValue == 0 ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		ph7_value_int64(pValue,sec);
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
		ph7_value_int64(pValue,nsec);
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
		ph7_result_value(pCtx,pArray);
		ph7_context_release_value(pCtx,pValue);
		ph7_context_release_value(pCtx,pArray);
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
PH7_PRIVATE sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec)
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
			/* 	Microseconds. date()/gmdate() have no sub-second part (uSec == 0);
			 * 	DateTime::format passes its stored microseconds. */
			ph7_result_string_format(pCtx,"%06d",uSec);
			break;
		case 'v':
			/* 	Milliseconds */
			ph7_result_string_format(pCtx,"%03d",uSec/1000);
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
			/* Whether or not the date is in daylight saving time. Use the
			 * broken-down time's own tm_isdst (as every other platform does):
			 * the old Windows _get_daylight() override reported whether the
			 * timezone observes DST at all, not whether THIS date is in it. */
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
	DateFormat(pCtx,zFormat,nLen,&sTm,0);
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
	DateFormat(pCtx,zFormat,nLen,&sTm,0);
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

#endif /* PH7_DISABLE_BUILTIN_FUNC */
