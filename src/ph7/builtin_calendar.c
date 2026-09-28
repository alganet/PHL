/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The four serial-day-number conversions below follow the algorithms of Scott
 * E. Lee's calendar package -- Copyright 1993-1995, Scott E. Lee, all rights
 * reserved; permission granted to use, copy, modify, distribute and sell so
 * long as the above copyright and this permission statement are retained in
 * all copies. THERE IS NO WARRANTY - USE AT YOUR OWN RISK. php's ext/calendar
 * carries the same package, which is why the arithmetic here is reproduced
 * step for step rather than re-derived: the overflow guards, the truncating
 * divisions and the "some invalid dates return a positive value" contract are
 * all observable through the PHP surface.
 */
#include "ph7int.h"
#include <time.h>    /* localtime/mktime -- the two doors this extension has
                      * onto the clock, and php uses the C library's own */
/*
 * Section:
 *    ext/calendar: the serial day number (SDN) and the four calendars php
 *    converts to and from it.
 * Status:
 *    Stable.
 *
 * An SDN is a plain day counter: SDN 1 is 25 November 4714 B.C. in the
 * Gregorian calendar and SDN 2447893 is 1 January 1990. Every function in this
 * extension is that counter with a calendar on one side of it, so the whole
 * surface is integer arithmetic with no clock, no locale and no timezone --
 * a Windows build answers what a POSIX one does by construction.
 *
 * Five rules of the package are visible from PHP and are easy to get wrong by
 * re-deriving instead of porting:
 *
 *   - ZERO is the failure answer in BOTH directions. There is no year 0 in any
 *     of these calendars, so an SDN of 0 means "no such date" and a converter
 *     handed one answers the string "0/0/0" rather than raising anything.
 *   - a positive SDN does not mean the input was valid. `GregorianToSdn` only
 *     screens month 1-12 and day 1-31, so 31 February converts happily; the
 *     package's own documented validity test is to convert back and compare.
 *   - the year jumps from -1 to 1. The internal arithmetic adds 4801 to a
 *     negative year and 4800 to a positive one, which is what closes that gap.
 *   - php reads every argument as a `zend_long` and then passes it to a
 *     routine taking `int`, so a value past 32 bits is TRUNCATED rather than
 *     refused: `gregoriantojd(1, 1, PHP_INT_MAX)` is the year -1. CalTruncInt()
 *     below reproduces that wrap in a defined way on every platform.
 *   - the day-of-week is `(sdn % 7 + 8) % 7`, which is why a NEGATIVE SDN --
 *     one every converter here rejects -- still has a weekday.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * php's argument path is `zend_long` -> `int` parameter, i.e. an
 * implementation-defined narrowing that every platform it builds on
 * implements as a two's-complement wrap. Spelled out so the answer is the
 * same one everywhere and so no build's overflow sanitizer has an opinion.
 */
static int CalTruncInt(sxi64 iVal)
{
	sxu64 uVal = (sxu64)iVal & (sxu64)0xFFFFFFFFu;
	if( uVal >= (sxu64)0x80000000u ){
		return (int)(sxi32)(uVal - (sxu64)0x100000000u);
	}
	return (int)(sxi32)uVal;
}
/* The three jdtojewish() flags, spelled here as well as in the constant table
 * (constant.c) because the Hebrew numeral builder reads them directly. */
#define CAL_JEWISH_ADD_ALAFIM_GERESH 0x2
#define CAL_JEWISH_ADD_ALAFIM        0x4
#define CAL_JEWISH_ADD_GERESHAYIM    0x8
/* The largest/smallest values the C arithmetic below is guarded against. */
#define CAL_INT_MAX  2147483647
#define CAL_INT_MIN  (-2147483647 - 1)
#define CAL_I64_MAX  SXI64_HIGH

/* ------------------------------------------------------------------ *
 *  Gregorian                                                          *
 * ------------------------------------------------------------------ */
#define GREGOR_SDN_OFFSET  32045
#define DAYS_PER_5_MONTHS  153
#define DAYS_PER_4_YEARS   1461
#define DAYS_PER_400_YEARS 146097

static void CalSdnToGregorian(sxi64 sdn,int *pYear,int *pMonth,int *pDay)
{
	int century,year,month,day,dayOfYear;
	sxi64 temp;
	if( sdn <= 0 || sdn > (CAL_I64_MAX - 4 * GREGOR_SDN_OFFSET) / 4 ){
		goto fail;
	}
	temp = (sdn + GREGOR_SDN_OFFSET) * 4 - 1;
	if( temp < 0 || (temp / DAYS_PER_400_YEARS) > CAL_INT_MAX ){
		goto fail;
	}
	/* Calculate the century (year/100). */
	century = (int)(temp / DAYS_PER_400_YEARS);
	/* Calculate the year and day of year (1 <= dayOfYear <= 366). */
	temp = ((temp % DAYS_PER_400_YEARS) / 4) * 4 + 3;
	if( century > ((CAL_INT_MAX / 100) - (int)(temp / DAYS_PER_4_YEARS)) ){
		goto fail;
	}
	year = (century * 100) + (int)(temp / DAYS_PER_4_YEARS);
	dayOfYear = (int)((temp % DAYS_PER_4_YEARS) / 4) + 1;
	/* Calculate the month and day of month. */
	temp = dayOfYear * 5 - 3;
	month = (int)(temp / DAYS_PER_5_MONTHS);
	day = (int)((temp % DAYS_PER_5_MONTHS) / 5) + 1;
	/* Convert to the normal beginning of the year. */
	if( month < 10 ){
		month += 3;
	}else{
		year += 1;
		month -= 9;
	}
	/* Adjust to the B.C./A.D. type numbering: there is no year 0. */
	year -= 4800;
	if( year <= 0 ){
		year--;
	}
	*pYear = year; *pMonth = month; *pDay = day;
	return;
fail:
	*pYear = 0; *pMonth = 0; *pDay = 0;
}
static sxi64 CalGregorianToSdn(int inputYear,int inputMonth,int inputDay)
{
	sxi64 year;
	int month;
	/* check for invalid dates */
	if( inputYear == 0 || inputYear < -4714
	 || inputYear > CAL_INT_MAX - 4800
	 || inputMonth <= 0 || inputMonth > 12
	 || inputDay <= 0 || inputDay > 31 ){
		return 0;
	}
	/* check for dates before SDN 1 (Nov 25, 4714 B.C.) */
	if( inputYear == -4714 ){
		if( inputMonth < 11 ){
			return 0;
		}
		if( inputMonth == 11 && inputDay < 25 ){
			return 0;
		}
	}
	/* Make year always a positive number. */
	year = inputYear < 0 ? (sxi64)inputYear + 4801 : (sxi64)inputYear + 4800;
	/* Adjust the start of the year. */
	if( inputMonth > 2 ){
		month = inputMonth - 3;
	}else{
		month = inputMonth + 9;
		year--;
	}
	return (((year / 100) * DAYS_PER_400_YEARS) / 4
			+ ((year % 100) * DAYS_PER_4_YEARS) / 4
			+ (month * DAYS_PER_5_MONTHS + 2) / 5
			+ inputDay
			- GREGOR_SDN_OFFSET);
}
/* The Julian calendar shares these two: same month names, same lengths. */
static const char * const azMonthShort[13] = {
	"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};
static const char * const azMonthLong[13] = {
	"","January","February","March","April","May","June","July","August",
	"September","October","November","December"
};
/* ------------------------------------------------------------------ *
 *  Julian                                                             *
 * ------------------------------------------------------------------ */
#define JULIAN_SDN_OFFSET 32083

static void CalSdnToJulian(sxi64 sdn,int *pYear,int *pMonth,int *pDay)
{
	int year,month,day,dayOfYear;
	sxi64 temp,yearl;
	if( sdn <= 0 ){
		goto fail;
	}
	/* Check for overflow */
	if( sdn > (CAL_I64_MAX - JULIAN_SDN_OFFSET * 4 + 1) / 4 ){
		goto fail;
	}
	temp = sdn * 4 + (JULIAN_SDN_OFFSET * 4 - 1);
	/* Calculate the year and day of year (1 <= dayOfYear <= 366). */
	yearl = temp / DAYS_PER_4_YEARS;
	if( yearl > CAL_INT_MAX || yearl < CAL_INT_MIN ){
		goto fail;
	}
	year = (int)yearl;
	dayOfYear = (int)((temp % DAYS_PER_4_YEARS) / 4) + 1;
	/* Calculate the month and day of month. */
	temp = dayOfYear * 5 - 3;
	month = (int)(temp / DAYS_PER_5_MONTHS);
	day = (int)((temp % DAYS_PER_5_MONTHS) / 5) + 1;
	/* Convert to the normal beginning of the year. */
	if( month < 10 ){
		month += 3;
	}else{
		year += 1;
		month -= 9;
	}
	/* Adjust to the B.C./A.D. type numbering. */
	year -= 4800;
	if( year <= 0 ){
		year--;
	}
	*pYear = year; *pMonth = month; *pDay = day;
	return;
fail:
	*pYear = 0; *pMonth = 0; *pDay = 0;
}
static sxi64 CalJulianToSdn(int inputYear,int inputMonth,int inputDay)
{
	sxi64 year;
	int month;
	/* check for invalid dates */
	if( inputYear == 0 || inputYear < -4713
	 || inputYear > CAL_INT_MAX - 4800
	 || inputMonth <= 0 || inputMonth > 12
	 || inputDay <= 0 || inputDay > 31 ){
		return 0;
	}
	/* check for dates before SDN 1 (Jan 2, 4713 B.C.) */
	if( inputYear == -4713 ){
		if( inputMonth == 1 && inputDay == 1 ){
			return 0;
		}
	}
	/* Make year always a positive number. */
	year = inputYear < 0 ? (sxi64)inputYear + 4801 : (sxi64)inputYear + 4800;
	/* Adjust the start of the year. */
	if( inputMonth > 2 ){
		month = inputMonth - 3;
	}else{
		month = inputMonth + 9;
		year--;
	}
	return ((year * DAYS_PER_4_YEARS) / 4
			+ (month * DAYS_PER_5_MONTHS + 2) / 5
			+ inputDay
			- JULIAN_SDN_OFFSET);
}

/* ------------------------------------------------------------------ *
 *  Jewish                                                             *
 * ------------------------------------------------------------------ */
#define HALAKIM_PER_HOUR 1080
#define HALAKIM_PER_DAY 25920
#define HALAKIM_PER_LUNAR_CYCLE ((29 * HALAKIM_PER_DAY) + 13753)
#define HALAKIM_PER_METONIC_CYCLE (HALAKIM_PER_LUNAR_CYCLE * (12 * 19 + 7))

#define JEWISH_SDN_OFFSET 347997
/* 12/13/887605; a greater value overflows the molad arithmetic below. */
#define JEWISH_SDN_MAX 324542846L
#define NEW_MOON_OF_CREATION 31524

#define CAL_SUNDAY    0
#define CAL_MONDAY    1
#define CAL_TUESDAY   2
#define CAL_WEDNESDAY 3
#define CAL_FRIDAY    5

#define CAL_NOON      (18 * HALAKIM_PER_HOUR)
#define CAL_AM3_11_20 ((9 * HALAKIM_PER_HOUR) + 204)
#define CAL_AM9_32_43 ((15 * HALAKIM_PER_HOUR) + 589)

static const int aMonthsPerYear[19] = {
	12,12,13,12,12,13,12,13,12,12,13,12,12,13,12,12,13,12,13
};
static const int aYearOffset[19] = {
	0,12,24,37,49,61,74,86,99,111,123,136,148,160,173,185,197,210,222
};
/*
 * A leap year has an Adar I and an Adar II; a regular one has neither, only
 * "Adar" in slot 7 -- so slot 6 of the regular table is the empty string and
 * the two tables are picked between by the YEAR, not by the calendar. The
 * Hebrew pair below is the same two tables in ISO-8859-8, php's own bytes.
 */
static const char * const azJewishMonthLeap[14] = {
	"","Tishri","Heshvan","Kislev","Tevet","Shevat","Adar I","Adar II",
	"Nisan","Iyyar","Sivan","Tammuz","Av","Elul"
};
static const char * const azJewishMonth[14] = {
	"","Tishri","Heshvan","Kislev","Tevet","Shevat","","Adar",
	"Nisan","Iyyar","Sivan","Tammuz","Av","Elul"
};
static const char * const azJewishHebMonthLeap[14] = {
	"","\xFA\xF9\xF8\xE9","\xE7\xF9\xE5\xEF","\xEB\xF1\xEC\xE5","\xE8\xE1\xFA",
	"\xF9\xE1\xE8","\xE0\xE3\xF8 \xE0'","\xE0\xE3\xF8 \xE1'","\xF0\xE9\xF1\xEF",
	"\xE0\xE9\xE9\xF8","\xF1\xE9\xE5\xEF","\xFA\xEE\xE5\xE6","\xE0\xE1",
	"\xE0\xEC\xE5\xEC"
};
static const char * const azJewishHebMonth[14] = {
	"","\xFA\xF9\xF8\xE9","\xE7\xF9\xE5\xEF","\xEB\xF1\xEC\xE5","\xE8\xE1\xFA",
	"\xF9\xE1\xE8","","\xE0\xE3\xF8","\xF0\xE9\xF1\xEF","\xE0\xE9\xE9\xF8",
	"\xF1\xE9\xE5\xEF","\xFA\xEE\xE5\xE6","\xE0\xE1","\xE0\xEC\xE5\xEC"
};
/* Which of the two name tables a Jewish year takes. */
#define CAL_JEWISH_MONTH_NAME(y) \
	((aMonthsPerYear[((y)-1) % 19] == 13) ? azJewishMonthLeap : azJewishMonth)
#define CAL_JEWISH_HEB_MONTH_NAME(y) \
	((aMonthsPerYear[((y)-1) % 19] == 13) ? azJewishHebMonthLeap : azJewishHebMonth)
/*
 * Given the year within the 19-year metonic cycle and the time of the molad
 * (new moon) that starts it, find the day Tishri 1 (Rosh Ha-Shanah) actually
 * falls on. Four rules (the dehiyyot) can push it up to two days later.
 */
static sxi64 CalTishri1(int metonicYear,sxi64 moladDay,sxi64 moladHalakim)
{
	sxi64 tishri1 = moladDay;
	int dow = (int)(tishri1 % 7);
	int leapYear = metonicYear == 2 || metonicYear == 5 || metonicYear == 7
		|| metonicYear == 10 || metonicYear == 13 || metonicYear == 16
		|| metonicYear == 18;
	int lastWasLeapYear = metonicYear == 3 || metonicYear == 6
		|| metonicYear == 8 || metonicYear == 11 || metonicYear == 14
		|| metonicYear == 17 || metonicYear == 0;
	/* Apply rules 2, 3 and 4. */
	if( (moladHalakim >= CAL_NOON)
	 || ((!leapYear) && dow == CAL_TUESDAY && moladHalakim >= CAL_AM3_11_20)
	 || (lastWasLeapYear && dow == CAL_MONDAY && moladHalakim >= CAL_AM9_32_43) ){
		tishri1++;
		dow++;
		if( dow == 7 ){
			dow = 0;
		}
	}
	/* Rule 1 comes last because it can add a second day on top. */
	if( dow == CAL_WEDNESDAY || dow == CAL_FRIDAY || dow == CAL_SUNDAY ){
		tishri1++;
	}
	return tishri1;
}
/*
 * The molad that starts a metonic cycle. The intermediate product needs more
 * than 32 bits, so it is carried in two halves exactly as the package does.
 */
static void CalMoladOfMetonicCycle(int metonicCycle,sxi64 *pMoladDay,sxi64 *pMoladHalakim)
{
	sxu64 r1,r2,d1,d2;
	sxi64 chk;
	/* Start with the time of the first molad after creation. */
	r1 = NEW_MOON_OF_CREATION;
	chk = (sxi64)metonicCycle;
	if( chk > (CAL_I64_MAX - NEW_MOON_OF_CREATION) / (HALAKIM_PER_METONIC_CYCLE & 0xFFFF) ){
		*pMoladDay = 0; *pMoladHalakim = 0;
		return;
	}
	/* metonicCycle * HALAKIM_PER_METONIC_CYCLE, upper 32 bits in r2 and lower
	 * 16 in r1. */
	r1 += (sxu64)chk * (HALAKIM_PER_METONIC_CYCLE & 0xFFFF);
	if( chk > (sxi64)((CAL_I64_MAX - (sxi64)(r1 >> 16)) / ((HALAKIM_PER_METONIC_CYCLE >> 16) & 0xFFFF)) ){
		*pMoladDay = 0; *pMoladHalakim = 0;
		return;
	}
	r2 = r1 >> 16;
	r2 += (sxu64)chk * ((HALAKIM_PER_METONIC_CYCLE >> 16) & 0xFFFF);
	/* r2r1 / HALAKIM_PER_DAY: remainder in r1, quotient halves in d2/d1. */
	d2 = r2 / HALAKIM_PER_DAY;
	r2 -= d2 * HALAKIM_PER_DAY;
	r1 = (r2 << 16) | (r1 & 0xFFFF);
	d1 = r1 / HALAKIM_PER_DAY;
	r1 -= d1 * HALAKIM_PER_DAY;
	*pMoladDay = (sxi64)((d2 << 16) | d1);
	*pMoladHalakim = (sxi64)r1;
}
/*
 * Find the molad of Tishri nearest a day number -- "nearest" in the package's
 * own biased sense: for a day in the first two months it answers the molad at
 * the START of the year, from the fourth month on the one at the END, and in
 * the third month either, because both are needed there anyway.
 */
static void CalFindTishriMolad(sxi64 inputDay,int *pMetonicCycle,int *pMetonicYear,
	sxi64 *pMoladDay,sxi64 *pMoladHalakim)
{
	sxi64 moladDay,moladHalakim;
	int metonicCycle,metonicYear;
	/* Estimate the metonic cycle number. A metonic cycle is 6939.6896 days,
	 * not 6940, so this can only ever UNDERestimate; the loop corrects it. */
	metonicCycle = (int)((inputDay + 310) / 6940);
	CalMoladOfMetonicCycle(metonicCycle,&moladDay,&moladHalakim);
	while( moladDay < inputDay - 6940 + 310 ){
		metonicCycle++;
		moladHalakim += HALAKIM_PER_METONIC_CYCLE;
		moladDay += moladHalakim / HALAKIM_PER_DAY;
		moladHalakim = moladHalakim % HALAKIM_PER_DAY;
	}
	/* Walk forward year by year to the molad of Tishri closest to the date. */
	for( metonicYear = 0 ; metonicYear < 18 ; metonicYear++ ){
		if( moladDay > inputDay - 74 ){
			break;
		}
		moladHalakim += HALAKIM_PER_LUNAR_CYCLE * aMonthsPerYear[metonicYear];
		moladDay += moladHalakim / HALAKIM_PER_DAY;
		moladHalakim = moladHalakim % HALAKIM_PER_DAY;
	}
	*pMetonicCycle = metonicCycle;
	*pMetonicYear = metonicYear;
	*pMoladDay = moladDay;
	*pMoladHalakim = moladHalakim;
}
/*
 * The first day of a Jewish year, and the molad that starts it.
 *
 * pTishri1 is an `int` on purpose: php's own FindStartOfYear declares it that
 * way, so a year large enough to push the day count past 32 bits comes back
 * TRUNCATED and every date built on it inherits the wrap. It is reachable --
 * `jewishtojd(1, 1, 2147483645)` answers a negative serial day number in php
 * -- so the narrowing is part of the contract rather than a bug to fix here.
 */
static void CalFindStartOfYear(int year,int *pMetonicCycle,int *pMetonicYear,
	sxi64 *pMoladDay,sxi64 *pMoladHalakim,int *pTishri1)
{
	*pMetonicCycle = (year - 1) / 19;
	*pMetonicYear = (year - 1) % 19;
	CalMoladOfMetonicCycle(*pMetonicCycle,pMoladDay,pMoladHalakim);
	*pMoladHalakim += (sxi64)HALAKIM_PER_LUNAR_CYCLE * aYearOffset[*pMetonicYear];
	*pMoladDay += *pMoladHalakim / HALAKIM_PER_DAY;
	*pMoladHalakim = *pMoladHalakim % HALAKIM_PER_DAY;
	*pTishri1 = CalTruncInt(CalTishri1(*pMetonicYear,*pMoladDay,*pMoladHalakim));
}
static void CalSdnToJewish(sxi64 sdn,int *pYear,int *pMonth,int *pDay)
{
	sxi64 inputDay,day,halakim;
	int tishri1,tishri1After;
	int metonicCycle,metonicYear,yearLength;
	if( sdn <= JEWISH_SDN_OFFSET || sdn > JEWISH_SDN_MAX ){
		*pYear = 0; *pMonth = 0; *pDay = 0;
		return;
	}
	inputDay = sdn - JEWISH_SDN_OFFSET;
	CalFindTishriMolad(inputDay,&metonicCycle,&metonicYear,&day,&halakim);
	tishri1 = CalTruncInt(CalTishri1(metonicYear,day,halakim));
	if( inputDay >= tishri1 ){
		/* It found Tishri 1 at the start of the year. */
		*pYear = metonicCycle * 19 + metonicYear + 1;
		if( inputDay < tishri1 + 59 ){
			/* The first 59 days are the same whatever the year's length is. */
			if( inputDay < tishri1 + 30 ){
				*pMonth = 1;
				*pDay = (int)(inputDay - tishri1 + 1);
			}else{
				*pMonth = 2;
				*pDay = (int)(inputDay - tishri1 - 29);
			}
			return;
		}
		/* Past that the year's length decides, so find the next Tishri 1. */
		halakim += (sxi64)HALAKIM_PER_LUNAR_CYCLE * aMonthsPerYear[metonicYear];
		day += halakim / HALAKIM_PER_DAY;
		halakim = halakim % HALAKIM_PER_DAY;
		tishri1After = CalTruncInt(CalTishri1((metonicYear + 1) % 19,day,halakim));
	}else{
		/* It found Tishri 1 at the end of the year. */
		*pYear = metonicCycle * 19 + metonicYear;
		if( inputDay >= tishri1 - 177 ){
			/* One of the last 6 months, whose lengths never vary. */
			if( inputDay > tishri1 - 30 ){
				*pMonth = 13; *pDay = (int)(inputDay - tishri1 + 30);
			}else if( inputDay > tishri1 - 60 ){
				*pMonth = 12; *pDay = (int)(inputDay - tishri1 + 60);
			}else if( inputDay > tishri1 - 89 ){
				*pMonth = 11; *pDay = (int)(inputDay - tishri1 + 89);
			}else if( inputDay > tishri1 - 119 ){
				*pMonth = 10; *pDay = (int)(inputDay - tishri1 + 119);
			}else if( inputDay > tishri1 - 148 ){
				*pMonth = 9;  *pDay = (int)(inputDay - tishri1 + 148);
			}else{
				*pMonth = 8;  *pDay = (int)(inputDay - tishri1 + 178);
			}
			return;
		}else{
			if( aMonthsPerYear[(*pYear - 1) % 19] == 13 ){
				*pMonth = 7;
				*pDay = (int)(inputDay - tishri1 + 207);
				if( *pDay > 0 ) return;
				(*pMonth)--; (*pDay) += 30;
				if( *pDay > 0 ) return;
				(*pMonth)--; (*pDay) += 30;
			}else{
				*pMonth = 7;
				*pDay = (int)(inputDay - tishri1 + 207);
				if( *pDay > 0 ) return;
				(*pMonth) -= 2; (*pDay) += 30;
			}
			if( *pDay > 0 ) return;
			(*pMonth)--; (*pDay) += 29;
			if( *pDay > 0 ) return;
			/* Kislev or Heshvan: the year's length is needed after all. */
			tishri1After = tishri1;
			CalFindTishriMolad(day - 365,&metonicCycle,&metonicYear,&day,&halakim);
			tishri1 = CalTruncInt(CalTishri1(metonicYear,day,halakim));
		}
	}
	yearLength = CalTruncInt((sxi64)tishri1After - tishri1);
	day = inputDay - tishri1 - 29;
	if( yearLength == 355 || yearLength == 385 ){
		/* Heshvan has 30 days */
		if( day <= 30 ){
			*pMonth = 2; *pDay = (int)day;
			return;
		}
		day -= 30;
	}else{
		/* Heshvan has 29 days */
		if( day <= 29 ){
			*pMonth = 2; *pDay = (int)day;
			return;
		}
		day -= 29;
	}
	/* It has to be Kislev. */
	*pMonth = 3;
	*pDay = (int)day;
}
static sxi64 CalJewishToSdn(int year,int month,int day)
{
	sxi64 sdn,moladDay,moladHalakim;
	int tishri1,tishri1After;
	int metonicCycle,metonicYear,yearLength,lengthOfAdarIAndII;
	if( year <= 0 || year >= CAL_INT_MAX - 1 || day <= 0 || day > 30 ){
		return 0;
	}
	switch( month ){
		case 1:
		case 2:
			/* Tishri or Heshvan -- the year's length is not needed. */
			CalFindStartOfYear(year,&metonicCycle,&metonicYear,
				&moladDay,&moladHalakim,&tishri1);
			sdn = CalTruncInt(month == 1
				? (sxi64)tishri1 + day - 1 : (sxi64)tishri1 + day + 29);
			break;
		case 3:
			/* Kislev -- the one month whose start needs the year's length. */
			CalFindStartOfYear(year,&metonicCycle,&metonicYear,
				&moladDay,&moladHalakim,&tishri1);
			moladHalakim += (sxi64)HALAKIM_PER_LUNAR_CYCLE * aMonthsPerYear[metonicYear];
			moladDay += moladHalakim / HALAKIM_PER_DAY;
			moladHalakim = moladHalakim % HALAKIM_PER_DAY;
			tishri1After = CalTruncInt(CalTishri1((metonicYear + 1) % 19,moladDay,moladHalakim));
			yearLength = CalTruncInt((sxi64)tishri1After - tishri1);
			sdn = CalTruncInt((yearLength == 355 || yearLength == 385)
				? (sxi64)tishri1 + day + 59 : (sxi64)tishri1 + day + 58);
			break;
		case 4:
		case 5:
		case 6:
			/* Tevet, Shevat or Adar I -- counted back from the next year. */
			CalFindStartOfYear(year + 1,&metonicCycle,&metonicYear,
				&moladDay,&moladHalakim,&tishri1After);
			lengthOfAdarIAndII = aMonthsPerYear[(year - 1) % 19] == 12 ? 29 : 59;
			if( month == 4 ){
				sdn = CalTruncInt((sxi64)tishri1After + day - lengthOfAdarIAndII - 237);
			}else if( month == 5 ){
				sdn = CalTruncInt((sxi64)tishri1After + day - lengthOfAdarIAndII - 208);
			}else{
				sdn = CalTruncInt((sxi64)tishri1After + day - lengthOfAdarIAndII - 178);
			}
			break;
		default:
			/* Adar II or later -- also counted back from the next year. */
			CalFindStartOfYear(year + 1,&metonicCycle,&metonicYear,
				&moladDay,&moladHalakim,&tishri1After);
			switch( month ){
				case 7:  sdn = CalTruncInt((sxi64)tishri1After + day - 207); break;
				case 8:  sdn = CalTruncInt((sxi64)tishri1After + day - 178); break;
				case 9:  sdn = CalTruncInt((sxi64)tishri1After + day - 148); break;
				case 10: sdn = CalTruncInt((sxi64)tishri1After + day - 119); break;
				case 11: sdn = CalTruncInt((sxi64)tishri1After + day - 89);  break;
				case 12: sdn = CalTruncInt((sxi64)tishri1After + day - 60);  break;
				case 13: sdn = CalTruncInt((sxi64)tishri1After + day - 30);  break;
				default: return 0;
			}
	}
	return sdn + JEWISH_SDN_OFFSET;
}

/* ------------------------------------------------------------------ *
 *  French republican                                                  *
 * ------------------------------------------------------------------ */
#define FRENCH_SDN_OFFSET  2375474
#define FRENCH_DAYS_PER_MONTH 30
#define FRENCH_FIRST_VALID 2375840
#define FRENCH_LAST_VALID  2380952

static void CalSdnToFrench(sxi64 sdn,int *pYear,int *pMonth,int *pDay)
{
	sxi64 temp;
	int dayOfYear;
	if( sdn < FRENCH_FIRST_VALID || sdn > FRENCH_LAST_VALID ){
		*pYear = 0; *pMonth = 0; *pDay = 0;
		return;
	}
	temp = (sdn - FRENCH_SDN_OFFSET) * 4 - 1;
	*pYear = (int)(temp / DAYS_PER_4_YEARS);
	dayOfYear = (int)((temp % DAYS_PER_4_YEARS) / 4);
	*pMonth = dayOfYear / FRENCH_DAYS_PER_MONTH + 1;
	*pDay = dayOfYear % FRENCH_DAYS_PER_MONTH + 1;
}
static sxi64 CalFrenchToSdn(int year,int month,int day)
{
	/* The calendar only ever ran 14 years, and the package refuses the rest. */
	if( year < 1 || year > 14 || month < 1 || month > 13 || day < 1 || day > 30 ){
		return 0;
	}
	return (((sxi64)year * DAYS_PER_4_YEARS) / 4
			+ (month - 1) * FRENCH_DAYS_PER_MONTH
			+ day
			+ FRENCH_SDN_OFFSET);
}
/* Slot 13 is the five or six holidays that close a year, not a month. */
static const char * const azFrenchMonth[14] = {
	"","Vendemiaire","Brumaire","Frimaire","Nivose","Pluviose","Ventose",
	"Germinal","Floreal","Prairial","Messidor","Thermidor","Fructidor","Extra"
};

/* ------------------------------------------------------------------ *
 *  Day of week                                                        *
 * ------------------------------------------------------------------ */
/*
 * Plain arithmetic on the counter, with no calendar consulted -- which is why
 * a serial day number every converter above rejects (0, a negative one, one
 * past the Jewish maximum) still has a weekday, and why the `+ 8` is there: C
 * gives a negative remainder for a negative operand.
 */
static int CalDayOfWeek(sxi64 sdn)
{
	return (int)(sdn % 7 + 8) % 7;
}
static const char * const azDayShort[7] = {
	"Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};
static const char * const azDayLong[7] = {
	"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};

/* ------------------------------------------------------------------ *
 *  The calendar table                                                 *
 * ------------------------------------------------------------------ */
/*
 * The four calendars behind one id, in the order the CAL_* constants number
 * them. The two lunisolar ones carry no separate abbreviations -- one table
 * answers both of cal_info()'s name keys -- and the Jewish entry holds the
 * LEAP-year spelling, which is what cal_info() shows; the per-YEAR choice
 * between the two Jewish tables is made at the two places that know a year.
 */
#define CAL_NUM_CALS 4
/* The ids the CAL_* constants carry, named here because three routines below
 * ask "is this the Jewish one?" or "is this the French one?" about them. */
#define CAL_ID_GREGORIAN 0
#define CAL_ID_JULIAN    1
#define CAL_ID_JEWISH    2
#define CAL_ID_FRENCH    3
typedef struct cal_entry cal_entry;
struct cal_entry {
	const char *zName;                          /* "Gregorian" */
	const char *zSymbol;                        /* "CAL_GREGORIAN" */
	sxi64 (*xToSdn)(int,int,int);
	void (*xFromSdn)(sxi64,int *,int *,int *);
	int nMonth;                                 /* 12, or 13 for the lunisolar pair */
	int nMaxDayInMonth;
	const char * const *azShort;
	const char * const *azLong;
};
static const cal_entry aCalendar[CAL_NUM_CALS] = {
	{ "Gregorian","CAL_GREGORIAN",CalGregorianToSdn,CalSdnToGregorian,12,31,
	  azMonthShort,azMonthLong },
	{ "Julian","CAL_JULIAN",CalJulianToSdn,CalSdnToJulian,12,31,
	  azMonthShort,azMonthLong },
	{ "Jewish","CAL_JEWISH",CalJewishToSdn,CalSdnToJewish,13,30,
	  azJewishMonthLeap,azJewishMonthLeap },
	{ "French","CAL_FRENCH",CalFrenchToSdn,CalSdnToFrench,13,30,
	  azFrenchMonth,azFrenchMonth }
};

/* ------------------------------------------------------------------ *
 *  The PHP surface                                                    *
 * ------------------------------------------------------------------ */
/*
 * Every `<calendar>tojd` builtin has the same shape: three int arguments read
 * in php's (month, day, year) ORDER and handed to the converter in the
 * package's (year, month, day) one, each narrowed to an int on the way.
 */
static int CalToJdCommon(ph7_context *pCtx,ph7_value **apArg,
	sxi64 (*xToSdn)(int,int,int))
{
	int month = CalTruncInt(ph7_value_to_int64(apArg[0]));
	int day   = CalTruncInt(ph7_value_to_int64(apArg[1]));
	int year  = CalTruncInt(ph7_value_to_int64(apArg[2]));
	ph7_result_int64(pCtx,xToSdn(year,month,day));
	return PH7_OK;
}
/* And every `jdto<calendar>` the same "month/day/year" string, "0/0/0" when
 * the SDN falls outside the calendar. */
static int CalFromJdCommon(ph7_context *pCtx,ph7_value **apArg,
	void (*xFromSdn)(sxi64,int *,int *,int *))
{
	int year,month,day;
	xFromSdn(ph7_value_to_int64(apArg[0]),&year,&month,&day);
	ph7_result_string_format(pCtx,"%d/%d/%d",month,day,year);
	return PH7_OK;
}
/*
 * int gregoriantojd(int $month, int $day, int $year)
 */
PH7_PRIVATE int PH7_builtin_gregoriantojd(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 3 ){
		/* Arity is enforced from aBuiltinSig[] before the call. */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	return CalToJdCommon(pCtx,apArg,CalGregorianToSdn);
}
/*
 * string jdtogregorian(int $julian_day)
 */
PH7_PRIVATE int PH7_builtin_jdtogregorian(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		ph7_result_string(pCtx,"0/0/0",(int)sizeof("0/0/0") - 1);
		return PH7_OK;
	}
	return CalFromJdCommon(pCtx,apArg,CalSdnToGregorian);
}
/*
 * int juliantojd(int $month, int $day, int $year)
 */
PH7_PRIVATE int PH7_builtin_juliantojd(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 3 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	return CalToJdCommon(pCtx,apArg,CalJulianToSdn);
}
/*
 * string jdtojulian(int $julian_day)
 */
PH7_PRIVATE int PH7_builtin_jdtojulian(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		ph7_result_string(pCtx,"0/0/0",(int)sizeof("0/0/0") - 1);
		return PH7_OK;
	}
	return CalFromJdCommon(pCtx,apArg,CalSdnToJulian);
}
/*
 * int frenchtojd(int $month, int $day, int $year)
 */
PH7_PRIVATE int PH7_builtin_frenchtojd(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 3 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	return CalToJdCommon(pCtx,apArg,CalFrenchToSdn);
}
/*
 * string jdtofrench(int $julian_day)
 */
PH7_PRIVATE int PH7_builtin_jdtofrench(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		ph7_result_string(pCtx,"0/0/0",(int)sizeof("0/0/0") - 1);
		return PH7_OK;
	}
	return CalFromJdCommon(pCtx,apArg,CalSdnToFrench);
}
/*
 * int jewishtojd(int $month, int $day, int $year)
 *  The one converter with a range check of its own: php screens the YEAR
 *  against the int range instead of truncating it, so the diagnostic here
 *  exists where the other three silently wrap.
 */
PH7_PRIVATE int PH7_builtin_jewishtojd(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iYear;
	if( nArg < 3 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	iYear = ph7_value_to_int64(apArg[2]);
	if( iYear > CAL_INT_MAX || iYear < CAL_INT_MIN ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"jewishtojd(): Argument #3 ($year) must be between %d and %d",
			CAL_INT_MIN,CAL_INT_MAX);
	}
	return CalToJdCommon(pCtx,apArg,CalJewishToSdn);
}
/*
 * The Hebrew numeral spelling of a number 1..9999, in ISO-8859-8 -- php's own
 * `heb_number_to_chars`. The result is NOT unique: 5 and 5000 both spell to a
 * single he, which is why php's own comment says to use the numeric form for
 * calculations. Answers 0 (and writes nothing) for a number outside the range.
 *
 * zBuf must hold at least 18 bytes plus the terminator, which is what the
 * widest spelling (four alafim characters, the " alafim " word, tav-tav-...)
 * can reach.
 */
#define CAL_HEB_BUF 24
static int CalHebNumberToChars(int n,int fl,char *zBuf)
{
	/* "0" then the 22 letters of the alphabet, ISO-8859-8. */
	static const char zAlefBet[24] =
		"0\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEB\xEC\xEE\xF0\xF1\xF2\xF4\xF6\xF7\xF8\xF9\xFA";
	char *p,*zEndOfAlafim;
	p = zEndOfAlafim = zBuf;
	/* Prevents the option breaking the jewish beliefs, php says. */
	if( n > 9999 || n < 1 ){
		zBuf[0] = 0;
		return 0;
	}
	/* alafim (thousands) case */
	if( n / 1000 ){
		*p++ = zAlefBet[n / 1000];
		if( CAL_JEWISH_ADD_ALAFIM_GERESH & fl ){
			*p++ = '\'';
		}
		if( CAL_JEWISH_ADD_ALAFIM & fl ){
			/* The word "alafim" itself, spaced on both sides. */
			SyMemcpy(" \xE0\xEC\xF4\xE9\xED ",p,7);
			p += 7;
		}
		zEndOfAlafim = p;
		n = n % 1000;
	}
	/* tav-tav (tav=400) case */
	while( n >= 400 ){
		*p++ = zAlefBet[22];
		n -= 400;
	}
	/* meot (hundreds) case */
	if( n >= 100 ){
		*p++ = zAlefBet[18 + n / 100];
		n = n % 100;
	}
	if( n == 15 || n == 16 ){
		/* tet-vav and tet-zayin: 15 and 16 are never spelled with the divine
		 * name's two letters. */
		*p++ = zAlefBet[9];
		*p++ = zAlefBet[n - 9];
	}else{
		/* asarot (tens) case */
		if( n >= 10 ){
			*p++ = zAlefBet[9 + n / 10];
			n = n % 10;
		}
		/* yehidot (ones) case */
		if( n > 0 ){
			*p++ = zAlefBet[n];
		}
	}
	if( CAL_JEWISH_ADD_GERESHAYIM & fl ){
		switch( p - zEndOfAlafim ){
			case 0:
				break;
			case 1:
				*p++ = '\'';
				break;
			default:
				/* The gershayim goes BEFORE the last letter, so that letter
				 * moves one place along. */
				*p = *(p - 1);
				*(p - 1) = '"';
				p++;
				break;
		}
	}
	*p = 0;
	return (int)(p - zBuf);
}
/*
 * string jdtojewish(int $julian_day, bool $hebrew = false, int $flags = 0)
 */
PH7_PRIVATE int PH7_builtin_jdtojewish(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	char zDay[CAL_HEB_BUF],zYear[CAL_HEB_BUF];
	int year,month,day,fl = 0,bHeb = 0;
	if( nArg < 1 ){
		ph7_result_string(pCtx,"0/0/0",(int)sizeof("0/0/0") - 1);
		return PH7_OK;
	}
	if( nArg > 1 ){
		bHeb = ph7_value_to_bool(apArg[1]);
		if( nArg > 2 ){
			fl = (int)ph7_value_to_int64(apArg[2]);
		}
	}
	CalSdnToJewish(ph7_value_to_int64(apArg[0]),&year,&month,&day);
	if( !bHeb ){
		ph7_result_string_format(pCtx,"%d/%d/%d",month,day,year);
		return PH7_OK;
	}
	if( year <= 0 || year > 9999 ){
		/* The Hebrew spelling has no numeral for a year outside this range,
		 * and php refuses rather than answering an ambiguous one. */
		return PH7_VmThrowException(pCtx,"ValueError","Year out of range (0-9999)");
	}
	CalHebNumberToChars(day,fl,zDay);
	CalHebNumberToChars(year,fl,zYear);
	ph7_result_string_format(pCtx,"%s %s %s",
		zDay,CAL_JEWISH_HEB_MONTH_NAME(year)[month],zYear);
	return PH7_OK;
}
/*
 * The calendar id every generic entry point screens first. Answers 1 and
 * raises php's ValueError -- whose ARGUMENT NUMBER differs per function --
 * when the id names no calendar.
 */
static int CalBadId(ph7_context *pCtx,sxi64 iCal,const char *zFn,int nPos)
{
	if( iCal >= 0 && iCal < CAL_NUM_CALS ){
		return 0;
	}
	PH7_VmThrowException(pCtx,"ValueError",
		"%s(): Argument #%d ($calendar) must be a valid calendar ID",zFn,nPos);
	return 1;
}
/* One calendar's row of cal_info(), written into pOut. */
static void CalInfoOne(ph7_context *pCtx,const cal_entry *pCal,ph7_value *pOut)
{
	ph7_value *pMonths,*pShort,*pVal;
	int i;
	pMonths = ph7_context_new_array(pCtx);
	pShort  = ph7_context_new_array(pCtx);
	pVal    = ph7_context_new_scalar(pCtx);
	if( pMonths == 0 || pShort == 0 || pVal == 0 ){
		return;
	}
	for( i = 1 ; i <= pCal->nMonth ; ++i ){
		ph7_value_string(pVal,pCal->azLong[i],-1);
		ph7_array_add_intkey_elem(pMonths,i,pVal);
		ph7_value_reset_string_cursor(pVal);
		ph7_value_string(pVal,pCal->azShort[i],-1);
		ph7_array_add_intkey_elem(pShort,i,pVal);
		ph7_value_reset_string_cursor(pVal);
	}
	ph7_array_add_strkey_elem(pOut,"months",pMonths);
	ph7_array_add_strkey_elem(pOut,"abbrevmonths",pShort);
	ph7_value_int(pVal,pCal->nMaxDayInMonth);
	ph7_array_add_strkey_elem(pOut,"maxdaysinmonth",pVal);
	ph7_value_string(pVal,pCal->zName,-1);
	ph7_array_add_strkey_elem(pOut,"calname",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,pCal->zSymbol,-1);
	ph7_array_add_strkey_elem(pOut,"calsymbol",pVal);
	ph7_context_release_value(pCtx,pVal);
	ph7_context_release_value(pCtx,pShort);
	ph7_context_release_value(pCtx,pMonths);
}
/*
 * array cal_info(int $calendar = -1)
 *  -1 -- the default -- is not an error but a REQUEST for all four, keyed by
 *  calendar id; every other negative value is refused.
 */
PH7_PRIVATE int PH7_builtin_cal_info(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray;
	sxi64 iCal = -1;
	if( nArg > 0 ){
		iCal = ph7_value_to_int64(apArg[0]);
	}
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( iCal == -1 ){
		int i;
		for( i = 0 ; i < CAL_NUM_CALS ; ++i ){
			ph7_value *pOne = ph7_context_new_array(pCtx);
			if( pOne == 0 ){
				continue;
			}
			CalInfoOne(pCtx,&aCalendar[i],pOne);
			ph7_array_add_intkey_elem(pArray,i,pOne);
			ph7_context_release_value(pCtx,pOne);
		}
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
	if( CalBadId(pCtx,iCal,"cal_info",1) ){
		return PH7_OK;
	}
	CalInfoOne(pCtx,&aCalendar[iCal],pArray);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * int cal_days_in_month(int $calendar, int $month, int $year)
 *  There is no month-length table anywhere in this extension: php converts the
 *  first of the month and the first of the NEXT month and subtracts, which is
 *  what lets one routine answer for a lunisolar calendar whose months move
 *  from year to year. A month the calendar cannot convert is therefore a bare
 *  "Invalid date" ValueError -- no function prefix, unlike the three argument
 *  screens above it.
 */
PH7_PRIVATE int PH7_builtin_cal_days_in_month(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const cal_entry *pCal;
	sxi64 iCal,iMonth,iYear,sdnStart,sdnNext;
	if( nArg < 3 ){
		/* Arity is enforced from aBuiltinSig[] before the call. */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	iCal   = ph7_value_to_int64(apArg[0]);
	iMonth = ph7_value_to_int64(apArg[1]);
	iYear  = ph7_value_to_int64(apArg[2]);
	if( CalBadId(pCtx,iCal,"cal_days_in_month",1) ){
		return PH7_OK;
	}
	if( iMonth <= 0 || iMonth > CAL_INT_MAX - 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"cal_days_in_month(): Argument #2 ($month) must be between 1 and %d",
			CAL_INT_MAX - 1);
	}
	if( iYear > CAL_INT_MAX - 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"cal_days_in_month(): Argument #3 ($year) must be less than %d",
			CAL_INT_MAX - 1);
	}
	pCal = &aCalendar[iCal];
	sdnStart = pCal->xToSdn(CalTruncInt(iYear),CalTruncInt(iMonth),1);
	if( sdnStart == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError","Invalid date");
	}
	sdnNext = pCal->xToSdn(CalTruncInt(iYear),CalTruncInt(iMonth + 1),1);
	if( sdnNext == 0 ){
		/* The next month is the next YEAR's first -- and the year after 1 B.C.
		 * is 1 A.D., not 0. */
		if( iYear == -1 ){
			sdnNext = pCal->xToSdn(1,1,1);
		}else{
			sdnNext = pCal->xToSdn(CalTruncInt(iYear + 1),1,1);
			if( iCal == CAL_ID_FRENCH && sdnNext == 0 ){
				/* The French calendar ends at 0014-13-05, so its last month has
				 * no next year to be measured against. */
				sdnNext = 2380953;
			}
		}
	}
	ph7_result_int64(pCtx,sdnNext - sdnStart);
	return PH7_OK;
}
/*
 * int cal_to_jd(int $calendar, int $month, int $day, int $year)
 *  Each argument gets a different screen: the day the whole int range, the
 *  month a 1-based one, and the year only an UPPER bound -- so a year below
 *  the int range is narrowed rather than refused.
 */
PH7_PRIVATE int PH7_builtin_cal_to_jd(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iCal,iMonth,iDay,iYear;
	if( nArg < 4 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	iCal   = ph7_value_to_int64(apArg[0]);
	iMonth = ph7_value_to_int64(apArg[1]);
	iDay   = ph7_value_to_int64(apArg[2]);
	iYear  = ph7_value_to_int64(apArg[3]);
	if( CalBadId(pCtx,iCal,"cal_to_jd",1) ){
		return PH7_OK;
	}
	if( iMonth <= 0 || iMonth > CAL_INT_MAX - 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"cal_to_jd(): Argument #2 ($month) must be between 1 and %d",CAL_INT_MAX - 1);
	}
	if( iDay > CAL_INT_MAX || iDay < CAL_INT_MIN ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"cal_to_jd(): Argument #3 ($day) must be between %d and %d",
			CAL_INT_MIN,CAL_INT_MAX);
	}
	if( iYear > CAL_INT_MAX - 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"cal_to_jd(): Argument #4 ($year) must be less than %d",CAL_INT_MAX - 1);
	}
	ph7_result_int64(pCtx,aCalendar[iCal].xToSdn(CalTruncInt(iYear),
		CalTruncInt(iMonth),CalTruncInt(iDay)));
	return PH7_OK;
}
/*
 * array cal_from_jd(int $julian_day, int $calendar)
 *  Nine keys. Two are special-cased for the Jewish calendar and for it alone:
 *  a serial day number BEFORE that calendar begins answers a NULL day of week
 *  and two empty day names, where every other calendar still names one -- the
 *  weekday is arithmetic on the counter and does not care whether the date
 *  exists -- and its month names come from the YEAR's own leap/regular table.
 */
PH7_PRIVATE int PH7_builtin_cal_from_jd(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const cal_entry *pCal;
	ph7_value *pArray,*pVal;
	sxi64 iJd,iCal;
	int year,month,day;
	if( nArg < 2 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	iJd  = ph7_value_to_int64(apArg[0]);
	iCal = ph7_value_to_int64(apArg[1]);
	if( CalBadId(pCtx,iCal,"cal_from_jd",2) ){
		return PH7_OK;
	}
	pCal = &aCalendar[iCal];
	pArray = ph7_context_new_array(pCtx);
	pVal   = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pCal->xFromSdn(iJd,&year,&month,&day);
	ph7_value_string_format(pVal,"%d/%d/%d",month,day,year);
	ph7_array_add_strkey_elem(pArray,"date",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_int(pVal,month); ph7_array_add_strkey_elem(pArray,"month",pVal);
	ph7_value_int(pVal,day);   ph7_array_add_strkey_elem(pArray,"day",pVal);
	ph7_value_int(pVal,year);  ph7_array_add_strkey_elem(pArray,"year",pVal);
	if( iCal != CAL_ID_JEWISH || year > 0 ){
		int dow = CalDayOfWeek(iJd);
		ph7_value_int(pVal,dow);
		ph7_array_add_strkey_elem(pArray,"dow",pVal);
		ph7_value_string(pVal,azDayShort[dow],-1);
		ph7_array_add_strkey_elem(pArray,"abbrevdayname",pVal);
		ph7_value_reset_string_cursor(pVal);
		ph7_value_string(pVal,azDayLong[dow],-1);
		ph7_array_add_strkey_elem(pArray,"dayname",pVal);
		ph7_value_reset_string_cursor(pVal);
	}else{
		ph7_value_null(pVal);
		ph7_array_add_strkey_elem(pArray,"dow",pVal);
		ph7_value_string(pVal,"",0);
		ph7_array_add_strkey_elem(pArray,"abbrevdayname",pVal);
		ph7_array_add_strkey_elem(pArray,"dayname",pVal);
		ph7_value_reset_string_cursor(pVal);
	}
	if( iCal == CAL_ID_JEWISH ){
		const char *zMonth = year > 0 ? CAL_JEWISH_MONTH_NAME(year)[month] : "";
		ph7_value_string(pVal,zMonth,-1);
		ph7_array_add_strkey_elem(pArray,"abbrevmonth",pVal);
		ph7_array_add_strkey_elem(pArray,"monthname",pVal);
	}else{
		ph7_value_string(pVal,pCal->azShort[month],-1);
		ph7_array_add_strkey_elem(pArray,"abbrevmonth",pVal);
		ph7_value_reset_string_cursor(pVal);
		ph7_value_string(pVal,pCal->azLong[month],-1);
		ph7_array_add_strkey_elem(pArray,"monthname",pVal);
	}
	ph7_context_release_value(pCtx,pVal);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * string|int jddayofweek(int $julian_day, int $mode = CAL_DOW_DAYNO)
 *  Only two modes answer a name; every other value -- negative, unknown, out
 *  of range -- falls through to the day NUMBER. The union prints in the STUB's
 *  order, which puts the rarer answer first here.
 */
PH7_PRIVATE int PH7_builtin_jddayofweek(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iMode = 0;
	int dow;
	if( nArg < 1 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	dow = CalDayOfWeek(ph7_value_to_int64(apArg[0]));
	if( nArg > 1 ){
		iMode = ph7_value_to_int64(apArg[1]);
	}
	if( iMode == 1 ){
		ph7_result_string(pCtx,azDayLong[dow],-1);
	}else if( iMode == 2 ){
		ph7_result_string(pCtx,azDayShort[dow],-1);
	}else{
		ph7_result_int(pCtx,dow);
	}
	return PH7_OK;
}
/*
 * string jdmonthname(int $julian_day, int $mode)
 *  $mode picks the CALENDAR as well as the spelling, and its six values are
 *  not in the order the calendar ids are: 0/1 Gregorian short/long, 2/3
 *  Julian, 4 Jewish, 5 French. Anything else is the Gregorian short name. A
 *  day the chosen calendar cannot place lands on month slot 0, which is the
 *  empty string in every table.
 */
PH7_PRIVATE int PH7_builtin_jdmonthname(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zMonth;
	sxi64 iJd,iMode;
	int year,month,day;
	if( nArg < 2 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	iJd = ph7_value_to_int64(apArg[0]);
	iMode = ph7_value_to_int64(apArg[1]);
	switch( iMode ){
		case 1:
			CalSdnToGregorian(iJd,&year,&month,&day);
			zMonth = azMonthLong[month];
			break;
		case 2:
			CalSdnToJulian(iJd,&year,&month,&day);
			zMonth = azMonthShort[month];
			break;
		case 3:
			CalSdnToJulian(iJd,&year,&month,&day);
			zMonth = azMonthLong[month];
			break;
		case 4:
			CalSdnToJewish(iJd,&year,&month,&day);
			zMonth = year > 0 ? CAL_JEWISH_MONTH_NAME(year)[month] : "";
			break;
		case 5:
			CalSdnToFrench(iJd,&year,&month,&day);
			zMonth = azFrenchMonth[month];
			break;
		default:
			CalSdnToGregorian(iJd,&year,&month,&day);
			zMonth = azMonthShort[month];
			break;
	}
	ph7_result_string(pCtx,zMonth,-1);
	return PH7_OK;
}

/* ------------------------------------------------------------------ *
 *  The clock                                                          *
 * ------------------------------------------------------------------ */
/*
 * These four are the only doors this extension has onto a clock, and both of
 * them are the C LIBRARY's rather than the engine's: php breaks a timestamp
 * down with localtime() and builds one with mktime(), so unixtojd() and
 * easter_date() read the PROCESS timezone and not `date.timezone`. Reproduced
 * that way here -- an engine-local UTC breakdown would answer differently from
 * php on any box that is not set to UTC, which is a divergence nobody asked
 * for.
 */
#define CAL_SECS_PER_DAY (24 * 3600)
/* The serial day number of 1 January 1970. */
#define CAL_UNIX_EPOCH_JD 2440588
/* easter_days()/easter_date()'s four $mode policies, spelled here as well as
 * in the constant table (constant.c) because the computation branches on them. */
#define CAL_EASTER_DEFAULT          0
#define CAL_EASTER_ROMAN            1
#define CAL_EASTER_ALWAYS_GREGORIAN 2
#define CAL_EASTER_ALWAYS_JULIAN    3
/*
 * localtime(3) into a caller-supplied buffer. The plain form is what php
 * calls, but MSVC deprecates it and this tree builds with /WX, so the two
 * reentrant spellings are used instead -- and they take their arguments in
 * OPPOSITE orders, which is why this wrapper exists at all. Same split as
 * src/phl/server.c.
 */
static struct tm *CalLocalTime(const time_t *pWhen,struct tm *pOut)
{
#ifdef __WINNT__
	return localtime_s(pOut,pWhen) == 0 ? pOut : 0;
#else
	return localtime_r(pWhen,pOut);
#endif
}
/*
 * `a + b` with php's own wrap. easter_days()'s year screen admits values up to
 * `LONG_MAX / 5 * 4`, which is exactly the headroom the GREGORIAN branch's
 * `year + year/4 - year/100 + year/400` needs -- but the JULIAN branch adds
 * `year + year/4 + 5`, i.e. 1.25 * year, and that overflows for the last two
 * years the screen lets through. php's answer for those two is built on the
 * wrap (`easter_days(LONG_MAX/5*4, CAL_EASTER_ALWAYS_JULIAN)` is 4), so the
 * wrap is reproduced here in UNSIGNED arithmetic rather than left to signed
 * overflow -- which is undefined, and which this tree's UBSan build traps.
 */
static sxi64 CalWrapAdd(sxi64 a,sxi64 b)
{
	sxu64 u = (sxu64)a + (sxu64)b;
	if( u >= ((sxu64)1 << 63) ){
		return (sxi64)(u - ((sxu64)1 << 63)) + SMALLEST_INT64;
	}
	return (sxi64)u;
}
/*
 * int|false unixtojd(?int $timestamp = null)
 *  Not the plain inverse of jdtounix(): the timestamp is broken down into a
 *  local calendar date first, and that date is what gets converted.
 */
PH7_PRIVATE int PH7_builtin_unixtojd(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct tm sTm,*pTm;
	time_t ts;
	if( nArg < 1 || ph7_value_is_null(apArg[0]) ){
		time(&ts);
	}else{
		sxi64 iTs = ph7_value_to_int64(apArg[0]);
		if( iTs < 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"unixtojd(): Argument #1 ($timestamp) must be greater than or equal to 0");
		}
		ts = (time_t)iTs;
	}
	pTm = CalLocalTime(&ts,&sTm);
	if( pTm == 0 ){
		/* The one FALSE in this extension: a timestamp the platform's own
		 * breakdown will not accept. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,
		CalGregorianToSdn(pTm->tm_year + 1900,pTm->tm_mon + 1,pTm->tm_mday));
	return PH7_OK;
}
/*
 * int jdtounix(int $julian_day)
 *  Pure arithmetic, so the answer is always midnight UTC. Its ValueError is
 *  the only one in this extension that names neither the function nor the
 *  argument -- php raises it with zend_value_error() rather than the argument
 *  helper, and the wording says "jday" rather than "$julian_day".
 */
PH7_PRIVATE int PH7_builtin_jdtounix(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 uday;
	if( nArg < 1 ){
		/* Arity is enforced from aBuiltinSig[] before the call. */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	uday = ph7_value_to_int64(apArg[0]);
	/* The lower test runs first, so the subtraction below it cannot underflow. */
	if( uday < CAL_UNIX_EPOCH_JD
	 || (uday - CAL_UNIX_EPOCH_JD) > (CAL_I64_MAX / CAL_SECS_PER_DAY) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"jday must be between %d and %qd",CAL_UNIX_EPOCH_JD,
			CAL_I64_MAX / CAL_SECS_PER_DAY + CAL_UNIX_EPOCH_JD);
	}
	uday -= CAL_UNIX_EPOCH_JD;
	ph7_result_int64(pCtx,uday * CAL_SECS_PER_DAY);
	return PH7_OK;
}
/*
 * The Easter computation both easter_days() and easter_date() run, from Simon
 * Kershaw's by way of php. `bGm` picks which of the two answers comes out: the
 * number of days after 21 March, or the timestamp of midnight that morning.
 *
 * $mode is not a choice between two rules but between four POLICIES, and the
 * default is date-dependent: Julian up to 1582, Julian again for 1583-1752
 * (England kept the old calendar that long), Gregorian after that.
 */
static int CalEaster(ph7_context *pCtx,int nArg,ph7_value **apArg,int bGm)
{
	const char *zFn = bGm ? "easter_date" : "easter_days";
	const sxi64 maxYear = (CAL_I64_MAX / 5) * 4;
	sxi64 year = 0,method = 0;
	sxi64 golden,solar,lunar,pfm,dom,tmp,easter;
	int bYearNull = 1;
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		year = ph7_value_to_int64(apArg[0]);
		bYearNull = 0;
	}
	if( nArg > 1 ){
		method = ph7_value_to_int64(apArg[1]);
	}
	if( bYearNull ){
		/* Default to the current year, read the same way php reads it. */
		struct tm sNow,*pTm;
		time_t now;
		time(&now);
		pTm = CalLocalTime(&now,&sNow);
		year = pTm == 0 ? 1900 : 1900 + pTm->tm_year;
	}
	if( year <= 0 || year > maxYear ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($year) must be between 1 and %qd",zFn,maxYear);
	}
	if( bGm ){
		/* The timestamp form narrows the year twice more: there is no timestamp
		 * before 1970, and php stops at the year two billion. */
		if( year < 1970 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"easter_date(): Argument #1 ($year) must be a year after 1970 (inclusive)");
		}
		if( year > 2000000000 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"easter_date(): Argument #1 ($year) must be a year before 2.000.000.000 (inclusive)");
		}
	}
	golden = (year % 19) + 1;                       /* the Golden number */
	if( (year <= 1582 && method != CAL_EASTER_ALWAYS_GREGORIAN)
	 || (year >= 1583 && year <= 1752 && method != CAL_EASTER_ROMAN
	     && method != CAL_EASTER_ALWAYS_GREGORIAN)
	 || method == CAL_EASTER_ALWAYS_JULIAN ){
		/* JULIAN CALENDAR */
		/* CalWrapAdd(): 1.25 * year overflows at the top of the year screen,
		 * and php's answer there is the wrapped one. */
		dom = CalWrapAdd(CalWrapAdd(year,year / 4),5) % 7;  /* "Dominical number" */
		if( dom < 0 ){
			dom += 7;
		}
		pfm = (3 - (11 * golden) - 7) % 30;         /* the Paschal full moon */
		if( pfm < 0 ){
			pfm += 30;
		}
	}else{
		/* GREGORIAN CALENDAR */
		dom = (year + (year / 4) - (year / 100) + (year / 400)) % 7;
		if( dom < 0 ){
			dom += 7;
		}
		solar = (year - 1600) / 100 - (year - 1600) / 400;
		lunar = (((year - 1400) / 100) * 8) / 25;
		pfm = (3 - (11 * golden) + solar - lunar) % 30;
		if( pfm < 0 ){
			pfm += 30;
		}
	}
	if( (pfm == 29) || (pfm == 28 && golden > 11) ){
		pfm--;                                      /* corrected full moon */
	}
	tmp = (4 - pfm - dom) % 7;
	if( tmp < 0 ){
		tmp += 7;
	}
	easter = pfm + tmp + 1;    /* Easter, as days after 21 March */
	if( !bGm ){
		ph7_result_int64(pCtx,easter);
		return PH7_OK;
	}
	{
		struct tm te;
		SyZero(&te,sizeof(te));
		te.tm_isdst = -1;
		te.tm_year = (int)(year - 1900);
		te.tm_sec = 0;
		te.tm_min = 0;
		te.tm_hour = 0;
		if( easter < 11 ){
			te.tm_mon = 2;                          /* March */
			te.tm_mday = (int)easter + 21;
		}else{
			te.tm_mon = 3;                          /* April */
			te.tm_mday = (int)easter - 10;
		}
		ph7_result_int64(pCtx,(sxi64)mktime(&te));
	}
	return PH7_OK;
}
/*
 * int easter_days(?int $year = null, int $mode = CAL_EASTER_DEFAULT)
 */
PH7_PRIVATE int PH7_builtin_easter_days(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return CalEaster(pCtx,nArg,apArg,0);
}
/*
 * int easter_date(?int $year = null, int $mode = CAL_EASTER_DEFAULT)
 */
PH7_PRIVATE int PH7_builtin_easter_date(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return CalEaster(pCtx,nArg,apArg,1);
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
