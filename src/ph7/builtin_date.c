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
#if defined(__UNIXES__)
	struct timeval tv;
	gettimeofday(&tv,0);
	sTime.tm_sec  = (long)tv.tv_sec;
	sTime.tm_usec = (long)tv.tv_usec;
#else
	time_t tt;
	time(&tt);
	sTime.tm_sec  = (long)tt;
	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);
#endif /* __UNIXES__ */
	if( nArg > 0 ){
		bFloat = ph7_value_to_bool(apArg[0]);
	}
	if( bFloat ){
		/* Return as float */
		ph7_result_double(pCtx,(double)sTime.tm_sec);
	}else{
		/* Return as string */
		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);
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
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[0]) ){
			t = (time_t)ph7_value_to_int64(apArg[0]);
			pTm = localtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
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
#if defined(__UNIXES__)
	struct timeval tv;
	gettimeofday(&tv,0);
	sTime.tm_sec  = (long)tv.tv_sec;
	sTime.tm_usec = (long)tv.tv_usec;
#else
	time_t tt;
	time(&tt);
	sTime.tm_sec  = (long)tt;
	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);
#endif /* __UNIXES__ */
	if( nArg > 0 ){
		bFloat = ph7_value_to_bool(apArg[0]);
	}
	if( bFloat ){
		/* Return as float */
		ph7_result_double(pCtx,(double)sTime.tm_sec);
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
		case 'o':
			/* ISO-8601 year number.*/
			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);
			break;
		case 'Y':
			/*	A full numeric representation of a year, 4 digits */
			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);
			break;
		case 'y':
			/*A two digit representation of a year*/
			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);
			break;
		case 'a':
			/*	Lowercase Ante meridiem and Post meridiem */
			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);
			break;
		case 'A':
			/*	Uppercase Ante meridiem and Post meridiem */
			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);
			break;
		case 'g':
			/*	12-hour format of an hour without leading zeros*/
			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));
			break;
		case 'G':
			/* 24-hour format of an hour without leading zeros */
			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);
			break;
		case 'h':
			/* 12-hour format of an hour with leading zeros */
			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));
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
			/* 	Microseconds */
			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);
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
				/* Assume GMT */
				zCur = "GMT";
			}
			ph7_result_string(pCtx,zCur,-1);
			break;
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
		case 'r':
			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */
			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",
				SyTimeGetDay(pTm->tm_wday),
				pTm->tm_mday,
				SyTimeGetMonth(pTm->tm_mon),
				pTm->tm_year,
				pTm->tm_hour,
				pTm->tm_min,
				pTm->tm_sec
				);
			break;
		case 'U':{
			time_t tt;
			/* Seconds since the Unix Epoch */
			time(&tt);
			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);
			break;
				 }
		case 'O':
		case 'P':
			/* Difference to Greenwich time (GMT) in hours */
			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);
			break;
		case 'Z':
			/* Timezone offset in seconds. The offset for timezones west of UTC
			 * is always negative, and for those east of UTC is always positive.
			 */
			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);
			break;
		case 'c':
			/* 	ISO 8601 date */
			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",
				pTm->tm_year,
				pTm->tm_mon+1,
				pTm->tm_mday,
				pTm->tm_hour,
				pTm->tm_min,
				pTm->tm_sec,
				pTm->tm_gmtoff
				);
			break;
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
				/* Assume GMT */
				zCur = "GMT";
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
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[1]) ){
			t = (time_t)ph7_value_to_int64(apArg[1]);
			pTm = localtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
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
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[1]) ){
			t = (time_t)ph7_value_to_int64(apArg[1]);
			pTm = localtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
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
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[0]) ){
			t = (time_t)ph7_value_to_int64(apArg[0]);
			pTm = localtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
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
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[1]) ){
			t = (time_t)ph7_value_to_int64(apArg[1]);
			pTm = localtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
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
	ph7_int64 iVal = 0;
	struct tm *pTm;
	time_t t;
	/* Extract function name */
	zFunction = ph7_function_name(pCtx);
	/* Get the current time */
	time(&t);
	if( zFunction[0] == 'g' /* gmmktime */ ){
		pTm = gmtime(&t);
	}else{
		/* localtime */
		pTm = localtime(&t);
	}
	if( nArg > 0 ){
		int iTmp;
		/* Hour */
		iTmp = ph7_value_to_int(apArg[0]);
		pTm->tm_hour = iTmp;
		if( nArg > 1 ){
			/* Minutes */
			iTmp = ph7_value_to_int(apArg[1]);
			pTm->tm_min = iTmp;
			if( nArg > 2 ){
				/* Seconds */
				iTmp = ph7_value_to_int(apArg[2]);
				pTm->tm_sec = iTmp;
				if( nArg > 3 ){
					/* Month */
					iTmp = ph7_value_to_int(apArg[3]);
					pTm->tm_mon = iTmp - 1;
					if( nArg > 4 ){
						/* mday */
						iTmp = ph7_value_to_int(apArg[4]);
						pTm->tm_mday = iTmp;
						if( nArg > 5 ){
							/* Year */
							iTmp = ph7_value_to_int(apArg[5]);
							if( iTmp > 1900 ){
								iTmp -= 1900;
							}
							pTm->tm_year = iTmp;
							if( nArg > 6 ){
								/* is_dst */
								iTmp = ph7_value_to_bool(apArg[6]);
								pTm->tm_isdst = iTmp;
							}
						}
					}
				}
			}
		}
	}
	/* Make the time */
	iVal = (ph7_int64)mktime(pTm);
	/* Return the timesatmp as a 64bit integer */
	ph7_result_int64(pCtx,iVal);
	return PH7_OK;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
