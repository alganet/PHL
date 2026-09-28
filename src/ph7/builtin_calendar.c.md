# src/ph7/builtin_calendar.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 767/822 lines (93.31%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    4 | ` *` |
|       - |    5 | ` * The four serial-day-number conversions below follow the algorithms of Scott` |
|       - |    6 | ` * E. Lee's calendar package -- Copyright 1993-1995, Scott E. Lee, all rights` |
|       - |    7 | ` * reserved; permission granted to use, copy, modify, distribute and sell so` |
|       - |    8 | ` * long as the above copyright and this permission statement are retained in` |
|       - |    9 | ` * all copies. THERE IS NO WARRANTY - USE AT YOUR OWN RISK. php's ext/calendar` |
|       - |   10 | ` * carries the same package, which is why the arithmetic here is reproduced` |
|       - |   11 | ` * step for step rather than re-derived: the overflow guards, the truncating` |
|       - |   12 | ` * divisions and the "some invalid dates return a positive value" contract are` |
|       - |   13 | ` * all observable through the PHP surface.` |
|       - |   14 | ` */` |
|       - |   15 | `#include "ph7int.h"` |
|       - |   16 | `#include <time.h>    /* localtime/mktime -- the two doors this extension has` |
|       - |   17 | `                      * onto the clock, and php uses the C library's own */` |
|       - |   18 | `/*` |
|       - |   19 | ` * Section:` |
|       - |   20 | ` *    ext/calendar: the serial day number (SDN) and the four calendars php` |
|       - |   21 | ` *    converts to and from it.` |
|       - |   22 | ` * Status:` |
|       - |   23 | ` *    Stable.` |
|       - |   24 | ` *` |
|       - |   25 | ` * An SDN is a plain day counter: SDN 1 is 25 November 4714 B.C. in the` |
|       - |   26 | ` * Gregorian calendar and SDN 2447893 is 1 January 1990. Every function in this` |
|       - |   27 | ` * extension is that counter with a calendar on one side of it, so the whole` |
|       - |   28 | ` * surface is integer arithmetic with no clock, no locale and no timezone --` |
|       - |   29 | ` * a Windows build answers what a POSIX one does by construction.` |
|       - |   30 | ` *` |
|       - |   31 | ` * Five rules of the package are visible from PHP and are easy to get wrong by` |
|       - |   32 | ` * re-deriving instead of porting:` |
|       - |   33 | ` *` |
|       - |   34 | ` *   - ZERO is the failure answer in BOTH directions. There is no year 0 in any` |
|       - |   35 | ` *     of these calendars, so an SDN of 0 means "no such date" and a converter` |
|       - |   36 | ` *     handed one answers the string "0/0/0" rather than raising anything.` |
|       - |   37 | `` *   - a positive SDN does not mean the input was valid. `GregorianToSdn` only`` |
|       - |   38 | ` *     screens month 1-12 and day 1-31, so 31 February converts happily; the` |
|       - |   39 | ` *     package's own documented validity test is to convert back and compare.` |
|       - |   40 | ` *   - the year jumps from -1 to 1. The internal arithmetic adds 4801 to a` |
|       - |   41 | ` *     negative year and 4800 to a positive one, which is what closes that gap.` |
|       - |   42 | `` *   - php reads every argument as a `zend_long` and then passes it to a`` |
|       - |   43 | `` *     routine taking `int`, so a value past 32 bits is TRUNCATED rather than`` |
|       - |   44 | `` *     refused: `gregoriantojd(1, 1, PHP_INT_MAX)` is the year -1. CalTruncInt()`` |
|       - |   45 | ` *     below reproduces that wrap in a defined way on every platform.` |
|       - |   46 | `` *   - the day-of-week is `(sdn % 7 + 8) % 7`, which is why a NEGATIVE SDN --`` |
|       - |   47 | ` *     one every converter here rejects -- still has a weekday.` |
|       - |   48 | ` */` |
|       - |   49 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       - |   50 | `/*` |
|       - |   51 | `` * php's argument path is `zend_long` -> `int` parameter, i.e. an`` |
|       - |   52 | ` * implementation-defined narrowing that every platform it builds on` |
|       - |   53 | ` * implements as a two's-complement wrap. Spelled out so the answer is the` |
|       - |   54 | ` * same one everywhere and so no build's overflow sanitizer has an opinion.` |
|       - |   55 | ` */` |
| 1868606 |   56 | `static int CalTruncInt(sxi64 iVal)` |
|       2 |   57 | `{` |
| 1868608 |   58 | `	sxu64 uVal = (sxu64)iVal & (sxu64)0xFFFFFFFFu;` |
| 1868608 |   59 | `	if( uVal >= (sxu64)0x80000000u ){` |
|      41 |   60 | `		return (int)(sxi32)(uVal - (sxu64)0x100000000u);` |
|       - |   61 | `	}` |
| 1868568 |   62 | `	return (int)(sxi32)uVal;` |
|  934305 |   63 | `}` |
|       - |   64 | `/* The three jdtojewish() flags, spelled here as well as in the constant table` |
|       - |   65 | ` * (constant.c) because the Hebrew numeral builder reads them directly. */` |
|       - |   66 | `#define CAL_JEWISH_ADD_ALAFIM_GERESH 0x2` |
|       - |   67 | `#define CAL_JEWISH_ADD_ALAFIM        0x4` |
|       - |   68 | `#define CAL_JEWISH_ADD_GERESHAYIM    0x8` |
|       - |   69 | `/* The largest/smallest values the C arithmetic below is guarded against. */` |
|       - |   70 | `#define CAL_INT_MAX  2147483647` |
|       - |   71 | `#define CAL_INT_MIN  (-2147483647 - 1)` |
|       - |   72 | `#define CAL_I64_MAX  SXI64_HIGH` |
|       - |   73 |  |
|       - |   74 | `/* ------------------------------------------------------------------ *` |
|       - |   75 | ` *  Gregorian                                                          *` |
|       - |   76 | ` * ------------------------------------------------------------------ */` |
|       - |   77 | `#define GREGOR_SDN_OFFSET  32045` |
|       - |   78 | `#define DAYS_PER_5_MONTHS  153` |
|       - |   79 | `#define DAYS_PER_4_YEARS   1461` |
|       - |   80 | `#define DAYS_PER_400_YEARS 146097` |
|       - |   81 |  |
|   97074 |   82 | `static void CalSdnToGregorian(sxi64 sdn,int *pYear,int *pMonth,int *pDay)` |
|       1 |   83 | `{` |
|       - |   84 | `	int century,year,month,day,dayOfYear;` |
|       - |   85 | `	sxi64 temp;` |
|   97075 |   86 | `	if( sdn <= 0 \|\| sdn > (CAL_I64_MAX - 4 * GREGOR_SDN_OFFSET) / 4 ){` |
|      31 |   87 | `		goto fail;` |
|       - |   88 | `	}` |
|   97045 |   89 | `	temp = (sdn + GREGOR_SDN_OFFSET) * 4 - 1;` |
|   97045 |   90 | `	if( temp < 0 \|\| (temp / DAYS_PER_400_YEARS) > CAL_INT_MAX ){` |
|     ! 0 |   91 | `		goto fail;` |
|       - |   92 | `	}` |
|       - |   93 | `	/* Calculate the century (year/100). */` |
|   97045 |   94 | `	century = (int)(temp / DAYS_PER_400_YEARS);` |
|       - |   95 | `	/* Calculate the year and day of year (1 <= dayOfYear <= 366). */` |
|   97045 |   96 | `	temp = ((temp % DAYS_PER_400_YEARS) / 4) * 4 + 3;` |
|   97045 |   97 | `	if( century > ((CAL_INT_MAX / 100) - (int)(temp / DAYS_PER_4_YEARS)) ){` |
|     ! 0 |   98 | `		goto fail;` |
|       - |   99 | `	}` |
|   97045 |  100 | `	year = (century * 100) + (int)(temp / DAYS_PER_4_YEARS);` |
|   97045 |  101 | `	dayOfYear = (int)((temp % DAYS_PER_4_YEARS) / 4) + 1;` |
|       - |  102 | `	/* Calculate the month and day of month. */` |
|   97045 |  103 | `	temp = dayOfYear * 5 - 3;` |
|   97045 |  104 | `	month = (int)(temp / DAYS_PER_5_MONTHS);` |
|   97045 |  105 | `	day = (int)((temp % DAYS_PER_5_MONTHS) / 5) + 1;` |
|       - |  106 | `	/* Convert to the normal beginning of the year. */` |
|   97045 |  107 | `	if( month < 10 ){` |
|   81311 |  108 | `		month += 3;` |
|   40656 |  109 | `	}else{` |
|   15735 |  110 | `		year += 1;` |
|   15735 |  111 | `		month -= 9;` |
|       - |  112 | `	}` |
|       - |  113 | `	/* Adjust to the B.C./A.D. type numbering: there is no year 0. */` |
|   97045 |  114 | `	year -= 4800;` |
|   97045 |  115 | `	if( year <= 0 ){` |
|      13 |  116 | `		year--;` |
|       6 |  117 | `	}` |
|   97045 |  118 | `	*pYear = year; *pMonth = month; *pDay = day;` |
|   97045 |  119 | `	return;` |
|      15 |  120 | `fail:` |
|      31 |  121 | `	*pYear = 0; *pMonth = 0; *pDay = 0;` |
|   48538 |  122 | `}` |
|  108270 |  123 | `static sxi64 CalGregorianToSdn(int inputYear,int inputMonth,int inputDay)` |
|       2 |  124 | `{` |
|       - |  125 | `	sxi64 year;` |
|       - |  126 | `	int month;` |
|       - |  127 | `	/* check for invalid dates */` |
|  108270 |  128 | `	if( inputYear == 0 \|\| inputYear < -4714` |
|  108260 |  129 | `	 \|\| inputYear > CAL_INT_MAX - 4800` |
|  108259 |  130 | `	 \|\| inputMonth <= 0 \|\| inputMonth > 12` |
|  108253 |  131 | `	 \|\| inputDay <= 0 \|\| inputDay > 31 ){` |
|      29 |  132 | `		return 0;` |
|       - |  133 | `	}` |
|       - |  134 | `	/* check for dates before SDN 1 (Nov 25, 4714 B.C.) */` |
|  108244 |  135 | `	if( inputYear == -4714 ){` |
|       9 |  136 | `		if( inputMonth < 11 ){` |
|       5 |  137 | `			return 0;` |
|       - |  138 | `		}` |
|       5 |  139 | `		if( inputMonth == 11 && inputDay < 25 ){` |
|       3 |  140 | `			return 0;` |
|       - |  141 | `		}` |
|       1 |  142 | `	}` |
|       - |  143 | `	/* Make year always a positive number. */` |
|  108238 |  144 | `	year = inputYear < 0 ? (sxi64)inputYear + 4801 : (sxi64)inputYear + 4800;` |
|       - |  145 | `	/* Adjust the start of the year. */` |
|  108238 |  146 | `	if( inputMonth > 2 ){` |
|   85258 |  147 | `		month = inputMonth - 3;` |
|   42630 |  148 | `	}else{` |
|   22982 |  149 | `		month = inputMonth + 9;` |
|   22982 |  150 | `		year--;` |
|       - |  151 | `	}` |
|  162356 |  152 | `	return (((year / 100) * DAYS_PER_400_YEARS) / 4` |
|  108236 |  153 | `			+ ((year % 100) * DAYS_PER_4_YEARS) / 4` |
|  108236 |  154 | `			+ (month * DAYS_PER_5_MONTHS + 2) / 5` |
|  108236 |  155 | `			+ inputDay` |
|  108236 |  156 | `			- GREGOR_SDN_OFFSET);` |
|   54137 |  157 | `}` |
|       - |  158 | `/* The Julian calendar shares these two: same month names, same lengths. */` |
|       - |  159 | `static const char * const azMonthShort[13] = {` |
|       - |  160 | `	"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"` |
|       - |  161 | `};` |
|       - |  162 | `static const char * const azMonthLong[13] = {` |
|       - |  163 | `	"","January","February","March","April","May","June","July","August",` |
|       - |  164 | `	"September","October","November","December"` |
|       - |  165 | `};` |
|       - |  166 | `/* ------------------------------------------------------------------ *` |
|       - |  167 | ` *  Julian                                                             *` |
|       - |  168 | ` * ------------------------------------------------------------------ */` |
|       - |  169 | `#define JULIAN_SDN_OFFSET 32083` |
|       - |  170 |  |
|   88440 |  171 | `static void CalSdnToJulian(sxi64 sdn,int *pYear,int *pMonth,int *pDay)` |
|       1 |  172 | `{` |
|       - |  173 | `	int year,month,day,dayOfYear;` |
|       - |  174 | `	sxi64 temp,yearl;` |
|   88441 |  175 | `	if( sdn <= 0 ){` |
|      15 |  176 | `		goto fail;` |
|       - |  177 | `	}` |
|       - |  178 | `	/* Check for overflow */` |
|   88427 |  179 | `	if( sdn > (CAL_I64_MAX - JULIAN_SDN_OFFSET * 4 + 1) / 4 ){` |
|       7 |  180 | `		goto fail;` |
|       - |  181 | `	}` |
|   88421 |  182 | `	temp = sdn * 4 + (JULIAN_SDN_OFFSET * 4 - 1);` |
|       - |  183 | `	/* Calculate the year and day of year (1 <= dayOfYear <= 366). */` |
|   88421 |  184 | `	yearl = temp / DAYS_PER_4_YEARS;` |
|   88421 |  185 | `	if( yearl > CAL_INT_MAX \|\| yearl < CAL_INT_MIN ){` |
|     ! 0 |  186 | `		goto fail;` |
|       - |  187 | `	}` |
|   88421 |  188 | `	year = (int)yearl;` |
|   88421 |  189 | `	dayOfYear = (int)((temp % DAYS_PER_4_YEARS) / 4) + 1;` |
|       - |  190 | `	/* Calculate the month and day of month. */` |
|   88421 |  191 | `	temp = dayOfYear * 5 - 3;` |
|   88421 |  192 | `	month = (int)(temp / DAYS_PER_5_MONTHS);` |
|   88421 |  193 | `	day = (int)((temp % DAYS_PER_5_MONTHS) / 5) + 1;` |
|       - |  194 | `	/* Convert to the normal beginning of the year. */` |
|   88421 |  195 | `	if( month < 10 ){` |
|   74089 |  196 | `		month += 3;` |
|   37045 |  197 | `	}else{` |
|   14333 |  198 | `		year += 1;` |
|   14333 |  199 | `		month -= 9;` |
|       - |  200 | `	}` |
|       - |  201 | `	/* Adjust to the B.C./A.D. type numbering. */` |
|   88421 |  202 | `	year -= 4800;` |
|   88421 |  203 | `	if( year <= 0 ){` |
|       9 |  204 | `		year--;` |
|       4 |  205 | `	}` |
|   88421 |  206 | `	*pYear = year; *pMonth = month; *pDay = day;` |
|   88421 |  207 | `	return;` |
|      10 |  208 | `fail:` |
|      21 |  209 | `	*pYear = 0; *pMonth = 0; *pDay = 0;` |
|   44221 |  210 | `}` |
|   88428 |  211 | `static sxi64 CalJulianToSdn(int inputYear,int inputMonth,int inputDay)` |
|       1 |  212 | `{` |
|       - |  213 | `	sxi64 year;` |
|       - |  214 | `	int month;` |
|       - |  215 | `	/* check for invalid dates */` |
|   88428 |  216 | `	if( inputYear == 0 \|\| inputYear < -4713` |
|   88425 |  217 | `	 \|\| inputYear > CAL_INT_MAX - 4800` |
|   88424 |  218 | `	 \|\| inputMonth <= 0 \|\| inputMonth > 12` |
|   88421 |  219 | `	 \|\| inputDay <= 0 \|\| inputDay > 31 ){` |
|      15 |  220 | `		return 0;` |
|       - |  221 | `	}` |
|       - |  222 | `	/* check for dates before SDN 1 (Jan 2, 4713 B.C.) */` |
|   88415 |  223 | `	if( inputYear == -4713 ){` |
|       7 |  224 | `		if( inputMonth == 1 && inputDay == 1 ){` |
|       5 |  225 | `			return 0;` |
|       - |  226 | `		}` |
|       1 |  227 | `	}` |
|       - |  228 | `	/* Make year always a positive number. */` |
|   88411 |  229 | `	year = inputYear < 0 ? (sxi64)inputYear + 4801 : (sxi64)inputYear + 4800;` |
|       - |  230 | `	/* Adjust the start of the year. */` |
|   88411 |  231 | `	if( inputMonth > 2 ){` |
|   74077 |  232 | `		month = inputMonth - 3;` |
|   37039 |  233 | `	}else{` |
|   14335 |  234 | `		month = inputMonth + 9;` |
|   14335 |  235 | `		year--;` |
|       - |  236 | `	}` |
|  132616 |  237 | `	return ((year * DAYS_PER_4_YEARS) / 4` |
|   88410 |  238 | `			+ (month * DAYS_PER_5_MONTHS + 2) / 5` |
|   88410 |  239 | `			+ inputDay` |
|   88410 |  240 | `			- JULIAN_SDN_OFFSET);` |
|   44215 |  241 | `}` |
|       - |  242 |  |
|       - |  243 | `/* ------------------------------------------------------------------ *` |
|       - |  244 | ` *  Jewish                                                             *` |
|       - |  245 | ` * ------------------------------------------------------------------ */` |
|       - |  246 | `#define HALAKIM_PER_HOUR 1080` |
|       - |  247 | `#define HALAKIM_PER_DAY 25920` |
|       - |  248 | `#define HALAKIM_PER_LUNAR_CYCLE ((29 * HALAKIM_PER_DAY) + 13753)` |
|       - |  249 | `#define HALAKIM_PER_METONIC_CYCLE (HALAKIM_PER_LUNAR_CYCLE * (12 * 19 + 7))` |
|       - |  250 |  |
|       - |  251 | `#define JEWISH_SDN_OFFSET 347997` |
|       - |  252 | `/* 12/13/887605; a greater value overflows the molad arithmetic below. */` |
|       - |  253 | `#define JEWISH_SDN_MAX 324542846L` |
|       - |  254 | `#define NEW_MOON_OF_CREATION 31524` |
|       - |  255 |  |
|       - |  256 | `#define CAL_SUNDAY    0` |
|       - |  257 | `#define CAL_MONDAY    1` |
|       - |  258 | `#define CAL_TUESDAY   2` |
|       - |  259 | `#define CAL_WEDNESDAY 3` |
|       - |  260 | `#define CAL_FRIDAY    5` |
|       - |  261 |  |
|       - |  262 | `#define CAL_NOON      (18 * HALAKIM_PER_HOUR)` |
|       - |  263 | `#define CAL_AM3_11_20 ((9 * HALAKIM_PER_HOUR) + 204)` |
|       - |  264 | `#define CAL_AM9_32_43 ((15 * HALAKIM_PER_HOUR) + 589)` |
|       - |  265 |  |
|       - |  266 | `static const int aMonthsPerYear[19] = {` |
|       - |  267 | `	12,12,13,12,12,13,12,13,12,12,13,12,12,13,12,12,13,12,13` |
|       - |  268 | `};` |
|       - |  269 | `static const int aYearOffset[19] = {` |
|       - |  270 | `	0,12,24,37,49,61,74,86,99,111,123,136,148,160,173,185,197,210,222` |
|       - |  271 | `};` |
|       - |  272 | `/*` |
|       - |  273 | ` * A leap year has an Adar I and an Adar II; a regular one has neither, only` |
|       - |  274 | ` * "Adar" in slot 7 -- so slot 6 of the regular table is the empty string and` |
|       - |  275 | ` * the two tables are picked between by the YEAR, not by the calendar. The` |
|       - |  276 | ` * Hebrew pair below is the same two tables in ISO-8859-8, php's own bytes.` |
|       - |  277 | ` */` |
|       - |  278 | `static const char * const azJewishMonthLeap[14] = {` |
|       - |  279 | `	"","Tishri","Heshvan","Kislev","Tevet","Shevat","Adar I","Adar II",` |
|       - |  280 | `	"Nisan","Iyyar","Sivan","Tammuz","Av","Elul"` |
|       - |  281 | `};` |
|       - |  282 | `static const char * const azJewishMonth[14] = {` |
|       - |  283 | `	"","Tishri","Heshvan","Kislev","Tevet","Shevat","","Adar",` |
|       - |  284 | `	"Nisan","Iyyar","Sivan","Tammuz","Av","Elul"` |
|       - |  285 | `};` |
|       - |  286 | `static const char * const azJewishHebMonthLeap[14] = {` |
|       - |  287 | `	"","\xFA\xF9\xF8\xE9","\xE7\xF9\xE5\xEF","\xEB\xF1\xEC\xE5","\xE8\xE1\xFA",` |
|       - |  288 | `	"\xF9\xE1\xE8","\xE0\xE3\xF8 \xE0'","\xE0\xE3\xF8 \xE1'","\xF0\xE9\xF1\xEF",` |
|       - |  289 | `	"\xE0\xE9\xE9\xF8","\xF1\xE9\xE5\xEF","\xFA\xEE\xE5\xE6","\xE0\xE1",` |
|       - |  290 | `	"\xE0\xEC\xE5\xEC"` |
|       - |  291 | `};` |
|       - |  292 | `static const char * const azJewishHebMonth[14] = {` |
|       - |  293 | `	"","\xFA\xF9\xF8\xE9","\xE7\xF9\xE5\xEF","\xEB\xF1\xEC\xE5","\xE8\xE1\xFA",` |
|       - |  294 | `	"\xF9\xE1\xE8","","\xE0\xE3\xF8","\xF0\xE9\xF1\xEF","\xE0\xE9\xE9\xF8",` |
|       - |  295 | `	"\xF1\xE9\xE5\xEF","\xFA\xEE\xE5\xE6","\xE0\xE1","\xE0\xEC\xE5\xEC"` |
|       - |  296 | `};` |
|       - |  297 | `/* Which of the two name tables a Jewish year takes. */` |
|       - |  298 | `#define CAL_JEWISH_MONTH_NAME(y) \` |
|       - |  299 | `	((aMonthsPerYear[((y)-1) % 19] == 13) ? azJewishMonthLeap : azJewishMonth)` |
|       - |  300 | `#define CAL_JEWISH_HEB_MONTH_NAME(y) \` |
|       - |  301 | `	((aMonthsPerYear[((y)-1) % 19] == 13) ? azJewishHebMonthLeap : azJewishHebMonth)` |
|       - |  302 | `/*` |
|       - |  303 | ` * Given the year within the 19-year metonic cycle and the time of the molad` |
|       - |  304 | ` * (new moon) that starts it, find the day Tishri 1 (Rosh Ha-Shanah) actually` |
|       - |  305 | ` * falls on. Four rules (the dehiyyot) can push it up to two days later.` |
|       - |  306 | ` */` |
|  439352 |  307 | `static sxi64 CalTishri1(int metonicYear,sxi64 moladDay,sxi64 moladHalakim)` |
|       1 |  308 | `{` |
|  439353 |  309 | `	sxi64 tishri1 = moladDay;` |
|  439353 |  310 | `	int dow = (int)(tishri1 % 7);` |
|  614299 |  311 | `	int leapYear = metonicYear == 2 \|\| metonicYear == 5 \|\| metonicYear == 7` |
|  383393 |  312 | `		\|\| metonicYear == 10 \|\| metonicYear == 13 \|\| metonicYear == 16` |
|  647930 |  313 | `		\|\| metonicYear == 18;` |
|  623027 |  314 | `	int lastWasLeapYear = metonicYear == 3 \|\| metonicYear == 6` |
|  403350 |  315 | `		\|\| metonicYear == 8 \|\| metonicYear == 11 \|\| metonicYear == 14` |
|  647117 |  316 | `		\|\| metonicYear == 17 \|\| metonicYear == 0;` |
|       - |  317 | `	/* Apply rules 2, 3 and 4. */` |
|  439352 |  318 | `	if( (moladHalakim >= CAL_NOON)` |
|  385251 |  319 | `	 \|\| ((!leapYear) && dow == CAL_TUESDAY && moladHalakim >= CAL_AM3_11_20)` |
|  323843 |  320 | `	 \|\| (lastWasLeapYear && dow == CAL_MONDAY && moladHalakim >= CAL_AM9_32_43) ){` |
|  138127 |  321 | `		tishri1++;` |
|  138127 |  322 | `		dow++;` |
|  138127 |  323 | `		if( dow == 7 ){` |
|   15047 |  324 | `			dow = 0;` |
|    7523 |  325 | `		}` |
|   62687 |  326 | `	}` |
|       - |  327 | `	/* Rule 1 comes last because it can add a second day on top. */` |
|  426601 |  328 | `	if( dow == CAL_WEDNESDAY \|\| dow == CAL_FRIDAY \|\| dow == CAL_SUNDAY ){` |
|  202911 |  329 | `		tishri1++;` |
|  101455 |  330 | `	}` |
|  426601 |  331 | `	return tishri1;` |
|       1 |  332 | `}` |
|       - |  333 | `/*` |
|       - |  334 | ` * The molad that starts a metonic cycle. The intermediate product needs more` |
|       - |  335 | ` * than 32 bits, so it is carried in two halves exactly as the package does.` |
|       - |  336 | ` */` |
|  414892 |  337 | `static void CalMoladOfMetonicCycle(int metonicCycle,sxi64 *pMoladDay,sxi64 *pMoladHalakim)` |
|       1 |  338 | `{` |
|       - |  339 | `	sxu64 r1,r2,d1,d2;` |
|       - |  340 | `	sxi64 chk;` |
|       - |  341 | `	/* Start with the time of the first molad after creation. */` |
|  414893 |  342 | `	r1 = NEW_MOON_OF_CREATION;` |
|  414893 |  343 | `	chk = (sxi64)metonicCycle;` |
|  414893 |  344 | `	if( chk > (CAL_I64_MAX - NEW_MOON_OF_CREATION) / (HALAKIM_PER_METONIC_CYCLE & 0xFFFF) ){` |
|     ! 0 |  345 | `		*pMoladDay = 0; *pMoladHalakim = 0;` |
|     ! 0 |  346 | `		return;` |
|       - |  347 | `	}` |
|       - |  348 | `	/* metonicCycle * HALAKIM_PER_METONIC_CYCLE, upper 32 bits in r2 and lower` |
|       - |  349 | `	 * 16 in r1. */` |
|  414893 |  350 | `	r1 += (sxu64)chk * (HALAKIM_PER_METONIC_CYCLE & 0xFFFF);` |
|  414893 |  351 | `	if( chk > (sxi64)((CAL_I64_MAX - (sxi64)(r1 >> 16)) / ((HALAKIM_PER_METONIC_CYCLE >> 16) & 0xFFFF)) ){` |
|     ! 0 |  352 | `		*pMoladDay = 0; *pMoladHalakim = 0;` |
|     ! 0 |  353 | `		return;` |
|       - |  354 | `	}` |
|  414893 |  355 | `	r2 = r1 >> 16;` |
|  414893 |  356 | `	r2 += (sxu64)chk * ((HALAKIM_PER_METONIC_CYCLE >> 16) & 0xFFFF);` |
|       - |  357 | `	/* r2r1 / HALAKIM_PER_DAY: remainder in r1, quotient halves in d2/d1. */` |
|  414893 |  358 | `	d2 = r2 / HALAKIM_PER_DAY;` |
|  414893 |  359 | `	r2 -= d2 * HALAKIM_PER_DAY;` |
|  414893 |  360 | `	r1 = (r2 << 16) \| (r1 & 0xFFFF);` |
|  414893 |  361 | `	d1 = r1 / HALAKIM_PER_DAY;` |
|  414893 |  362 | `	r1 -= d1 * HALAKIM_PER_DAY;` |
|  414893 |  363 | `	*pMoladDay = (sxi64)((d2 << 16) \| d1);` |
|  414893 |  364 | `	*pMoladHalakim = (sxi64)r1;` |
|  207447 |  365 | `}` |
|       - |  366 | `/*` |
|       - |  367 | ` * Find the molad of Tishri nearest a day number -- "nearest" in the package's` |
|       - |  368 | ` * own biased sense: for a day in the first two months it answers the molad at` |
|       - |  369 | ` * the START of the year, from the fourth month on the one at the END, and in` |
|       - |  370 | ` * the third month either, because both are needed there anyway.` |
|       - |  371 | ` */` |
|  211556 |  372 | `static void CalFindTishriMolad(sxi64 inputDay,int *pMetonicCycle,int *pMetonicYear,` |
|       - |  373 | `	sxi64 *pMoladDay,sxi64 *pMoladHalakim)` |
|       1 |  374 | `{` |
|       - |  375 | `	sxi64 moladDay,moladHalakim;` |
|       - |  376 | `	int metonicCycle,metonicYear;` |
|       - |  377 | `	/* Estimate the metonic cycle number. A metonic cycle is 6939.6896 days,` |
|       - |  378 | `	 * not 6940, so this can only ever UNDERestimate; the loop corrects it. */` |
|  211557 |  379 | `	metonicCycle = (int)((inputDay + 310) / 6940);` |
|  211557 |  380 | `	CalMoladOfMetonicCycle(metonicCycle,&moladDay,&moladHalakim);` |
|  213581 |  381 | `	while( moladDay < inputDay - 6940 + 310 ){` |
|    2025 |  382 | `		metonicCycle++;` |
|    2025 |  383 | `		moladHalakim += HALAKIM_PER_METONIC_CYCLE;` |
|    2025 |  384 | `		moladDay += moladHalakim / HALAKIM_PER_DAY;` |
|    2025 |  385 | `		moladHalakim = moladHalakim % HALAKIM_PER_DAY;` |
|       1 |  386 | `	}` |
|       - |  387 | `	/* Walk forward year by year to the molad of Tishri closest to the date. */` |
| 2117015 |  388 | `	for( metonicYear = 0 ; metonicYear < 18 ; metonicYear++ ){` |
| 2106317 |  389 | `		if( moladDay > inputDay - 74 ){` |
|  200859 |  390 | `			break;` |
|       - |  391 | `		}` |
| 1905459 |  392 | `		moladHalakim += HALAKIM_PER_LUNAR_CYCLE * aMonthsPerYear[metonicYear];` |
| 1905459 |  393 | `		moladDay += moladHalakim / HALAKIM_PER_DAY;` |
| 1905459 |  394 | `		moladHalakim = moladHalakim % HALAKIM_PER_DAY;` |
|  952730 |  395 | `	}` |
|  211557 |  396 | `	*pMetonicCycle = metonicCycle;` |
|  211557 |  397 | `	*pMetonicYear = metonicYear;` |
|  211557 |  398 | `	*pMoladDay = moladDay;` |
|  211557 |  399 | `	*pMoladHalakim = moladHalakim;` |
|  211557 |  400 | `}` |
|       - |  401 | `/*` |
|       - |  402 | ` * The first day of a Jewish year, and the molad that starts it.` |
|       - |  403 | ` *` |
|       - |  404 | `` * pTishri1 is an `int` on purpose: php's own FindStartOfYear declares it that`` |
|       - |  405 | ` * way, so a year large enough to push the day count past 32 bits comes back` |
|       - |  406 | ` * TRUNCATED and every date built on it inherits the wrap. It is reachable --` |
|       - |  407 | `` * `jewishtojd(1, 1, 2147483645)` answers a negative serial day number in php`` |
|       - |  408 | ` * -- so the narrowing is part of the contract rather than a bug to fix here.` |
|       - |  409 | ` */` |
|  203336 |  410 | `static void CalFindStartOfYear(int year,int *pMetonicCycle,int *pMetonicYear,` |
|       - |  411 | `	sxi64 *pMoladDay,sxi64 *pMoladHalakim,int *pTishri1)` |
|       1 |  412 | `{` |
|  203337 |  413 | `	*pMetonicCycle = (year - 1) / 19;` |
|  203337 |  414 | `	*pMetonicYear = (year - 1) % 19;` |
|  203337 |  415 | `	CalMoladOfMetonicCycle(*pMetonicCycle,pMoladDay,pMoladHalakim);` |
|  203337 |  416 | `	*pMoladHalakim += (sxi64)HALAKIM_PER_LUNAR_CYCLE * aYearOffset[*pMetonicYear];` |
|  203337 |  417 | `	*pMoladDay += *pMoladHalakim / HALAKIM_PER_DAY;` |
|  203337 |  418 | `	*pMoladHalakim = *pMoladHalakim % HALAKIM_PER_DAY;` |
|  203337 |  419 | `	*pTishri1 = CalTruncInt(CalTishri1(*pMetonicYear,*pMoladDay,*pMoladHalakim));` |
|  203337 |  420 | `}` |
|  202772 |  421 | `static void CalSdnToJewish(sxi64 sdn,int *pYear,int *pMonth,int *pDay)` |
|       1 |  422 | `{` |
|       - |  423 | `	sxi64 inputDay,day,halakim;` |
|       - |  424 | `	int tishri1,tishri1After;` |
|       - |  425 | `	int metonicCycle,metonicYear,yearLength;` |
|  202773 |  426 | `	if( sdn <= JEWISH_SDN_OFFSET \|\| sdn > JEWISH_SDN_MAX ){` |
|      29 |  427 | `		*pYear = 0; *pMonth = 0; *pDay = 0;` |
|   93149 |  428 | `		return;` |
|       - |  429 | `	}` |
|  202745 |  430 | `	inputDay = sdn - JEWISH_SDN_OFFSET;` |
|  202745 |  431 | `	CalFindTishriMolad(inputDay,&metonicCycle,&metonicYear,&day,&halakim);` |
|  202745 |  432 | `	tishri1 = CalTruncInt(CalTishri1(metonicYear,day,halakim));` |
|  202745 |  433 | `	if( inputDay >= tishri1 ){` |
|       - |  434 | `		/* It found Tishri 1 at the start of the year. */` |
|   40673 |  435 | `		*pYear = metonicCycle * 19 + metonicYear + 1;` |
|   40673 |  436 | `		if( inputDay < tishri1 + 59 ){` |
|       - |  437 | `			/* The first 59 days are the same whatever the year's length is. */` |
|   32733 |  438 | `			if( inputDay < tishri1 + 30 ){` |
|   16649 |  439 | `				*pMonth = 1;` |
|   16649 |  440 | `				*pDay = (int)(inputDay - tishri1 + 1);` |
|    8325 |  441 | `			}else{` |
|   16085 |  442 | `				*pMonth = 2;` |
|   16085 |  443 | `				*pDay = (int)(inputDay - tishri1 - 29);` |
|       - |  444 | `			}` |
|   32733 |  445 | `			return;` |
|       - |  446 | `		}` |
|       - |  447 | `		/* Past that the year's length decides, so find the next Tishri 1. */` |
|    7941 |  448 | `		halakim += (sxi64)HALAKIM_PER_LUNAR_CYCLE * aMonthsPerYear[metonicYear];` |
|    7941 |  449 | `		day += halakim / HALAKIM_PER_DAY;` |
|    7941 |  450 | `		halakim = halakim % HALAKIM_PER_DAY;` |
|    7941 |  451 | `		tishri1After = CalTruncInt(CalTishri1((metonicYear + 1) % 19,day,halakim));` |
|    3971 |  452 | `	}else{` |
|       - |  453 | `		/* It found Tishri 1 at the end of the year. */` |
|  162073 |  454 | `		*pYear = metonicCycle * 19 + metonicYear;` |
|  162073 |  455 | `		if( inputDay >= tishri1 - 177 ){` |
|       - |  456 | `			/* One of the last 6 months, whose lengths never vary. */` |
|   98237 |  457 | `			if( inputDay > tishri1 - 30 ){` |
|   16095 |  458 | `				*pMonth = 13; *pDay = (int)(inputDay - tishri1 + 30);` |
|   90190 |  459 | `			}else if( inputDay > tishri1 - 60 ){` |
|   16647 |  460 | `				*pMonth = 12; *pDay = (int)(inputDay - tishri1 + 60);` |
|   73820 |  461 | `			}else if( inputDay > tishri1 - 89 ){` |
|   16103 |  462 | `				*pMonth = 11; *pDay = (int)(inputDay - tishri1 + 89);` |
|   57446 |  463 | `			}else if( inputDay > tishri1 - 119 ){` |
|   16649 |  464 | `				*pMonth = 10; *pDay = (int)(inputDay - tishri1 + 119);` |
|   41071 |  465 | `			}else if( inputDay > tishri1 - 148 ){` |
|   16087 |  466 | `				*pMonth = 9;  *pDay = (int)(inputDay - tishri1 + 148);` |
|    8044 |  467 | `			}else{` |
|   16661 |  468 | `				*pMonth = 8;  *pDay = (int)(inputDay - tishri1 + 178);` |
|       - |  469 | `			}` |
|   98237 |  470 | `			return;` |
|     ! 0 |  471 | `		}else{` |
|   63837 |  472 | `			if( aMonthsPerYear[(*pYear - 1) % 19] == 13 ){` |
|   27309 |  473 | `				*pMonth = 7;` |
|   27309 |  474 | `				*pDay = (int)(inputDay - tishri1 + 207);` |
|   27309 |  475 | `				if( *pDay > 0 ) return;` |
|   21373 |  476 | `				(*pMonth)--; (*pDay) += 30;` |
|   21373 |  477 | `				if( *pDay > 0 ) return;` |
|   15237 |  478 | `				(*pMonth)--; (*pDay) += 30;` |
|    7619 |  479 | `			}else{` |
|   36529 |  480 | `				*pMonth = 7;` |
|   36529 |  481 | `				*pDay = (int)(inputDay - tishri1 + 207);` |
|   36529 |  482 | `				if( *pDay > 0 ) return;` |
|   26363 |  483 | `				(*pMonth) -= 2; (*pDay) += 30;` |
|       - |  484 | `			}` |
|   41599 |  485 | `			if( *pDay > 0 ) return;` |
|   24945 |  486 | `			(*pMonth)--; (*pDay) += 29;` |
|   24945 |  487 | `			if( *pDay > 0 ) return;` |
|       - |  488 | `			/* Kislev or Heshvan: the year's length is needed after all. */` |
|    8813 |  489 | `			tishri1After = tishri1;` |
|    8813 |  490 | `			CalFindTishriMolad(day - 365,&metonicCycle,&metonicYear,&day,&halakim);` |
|    8813 |  491 | `			tishri1 = CalTruncInt(CalTishri1(metonicYear,day,halakim));` |
|       - |  492 | `		}` |
|       - |  493 | `	}` |
|   16753 |  494 | `	yearLength = CalTruncInt((sxi64)tishri1After - tishri1);` |
|   16753 |  495 | `	day = inputDay - tishri1 - 29;` |
|   16753 |  496 | `	if( yearLength == 355 \|\| yearLength == 385 ){` |
|       - |  497 | `		/* Heshvan has 30 days */` |
|    7727 |  498 | `		if( day <= 30 ){` |
|     249 |  499 | `			*pMonth = 2; *pDay = (int)day;` |
|     249 |  500 | `			return;` |
|       - |  501 | `		}` |
|    7479 |  502 | `		day -= 30;` |
|    3740 |  503 | `	}else{` |
|       - |  504 | `		/* Heshvan has 29 days */` |
|    9027 |  505 | `		if( day <= 29 ){` |
|     ! 0 |  506 | `			*pMonth = 2; *pDay = (int)day;` |
|     ! 0 |  507 | `			return;` |
|       - |  508 | `		}` |
|    9027 |  509 | `		day -= 29;` |
|       - |  510 | `	}` |
|       - |  511 | `	/* It has to be Kislev. */` |
|   16505 |  512 | `	*pMonth = 3;` |
|   16505 |  513 | `	*pDay = (int)day;` |
|  101387 |  514 | `}` |
|  203352 |  515 | `static sxi64 CalJewishToSdn(int year,int month,int day)` |
|       1 |  516 | `{` |
|       - |  517 | `	sxi64 sdn,moladDay,moladHalakim;` |
|       - |  518 | `	int tishri1,tishri1After;` |
|       - |  519 | `	int metonicCycle,metonicYear,yearLength,lengthOfAdarIAndII;` |
|  203353 |  520 | `	if( year <= 0 \|\| year >= CAL_INT_MAX - 1 \|\| day <= 0 \|\| day > 30 ){` |
|      17 |  521 | `		return 0;` |
|       - |  522 | `	}` |
|  203337 |  523 | `	switch( month ){` |
|   16711 |  524 | `		case 1:` |
|       - |  525 | `		case 2:` |
|       - |  526 | `			/* Tishri or Heshvan -- the year's length is not needed. */` |
|   33423 |  527 | `			CalFindStartOfYear(year,&metonicCycle,&metonicYear,` |
|       - |  528 | `				&moladDay,&moladHalakim,&tishri1);` |
|   50134 |  529 | `			sdn = CalTruncInt(month == 1` |
|   33422 |  530 | `				? (sxi64)tishri1 + day - 1 : (sxi64)tishri1 + day + 29);` |
|   33423 |  531 | `			break;` |
|    8260 |  532 | `		case 3:` |
|       - |  533 | `			/* Kislev -- the one month whose start needs the year's length. */` |
|   16521 |  534 | `			CalFindStartOfYear(year,&metonicCycle,&metonicYear,` |
|       - |  535 | `				&moladDay,&moladHalakim,&tishri1);` |
|   16521 |  536 | `			moladHalakim += (sxi64)HALAKIM_PER_LUNAR_CYCLE * aMonthsPerYear[metonicYear];` |
|   16521 |  537 | `			moladDay += moladHalakim / HALAKIM_PER_DAY;` |
|   16521 |  538 | `			moladHalakim = moladHalakim % HALAKIM_PER_DAY;` |
|   16521 |  539 | `			tishri1After = CalTruncInt(CalTishri1((metonicYear + 1) % 19,moladDay,moladHalakim));` |
|   16521 |  540 | `			yearLength = CalTruncInt((sxi64)tishri1After - tishri1);` |
|   24781 |  541 | `			sdn = CalTruncInt((yearLength == 355 \|\| yearLength == 385)` |
|   16520 |  542 | `				? (sxi64)tishri1 + day + 59 : (sxi64)tishri1 + day + 58);` |
|   16521 |  543 | `			break;` |
|   19470 |  544 | `		case 4:` |
|       - |  545 | `		case 5:` |
|       - |  546 | `		case 6:` |
|       - |  547 | `			/* Tevet, Shevat or Adar I -- counted back from the next year. */` |
|   38941 |  548 | `			CalFindStartOfYear(year + 1,&metonicCycle,&metonicYear,` |
|       - |  549 | `				&moladDay,&moladHalakim,&tishri1After);` |
|   38941 |  550 | `			lengthOfAdarIAndII = aMonthsPerYear[(year - 1) % 19] == 12 ? 29 : 59;` |
|   38941 |  551 | `			if( month == 4 ){` |
|   16119 |  552 | `				sdn = CalTruncInt((sxi64)tishri1After + day - lengthOfAdarIAndII - 237);` |
|   30882 |  553 | `			}else if( month == 5 ){` |
|   16669 |  554 | `				sdn = CalTruncInt((sxi64)tishri1After + day - lengthOfAdarIAndII - 208);` |
|    8335 |  555 | `			}else{` |
|    6155 |  556 | `				sdn = CalTruncInt((sxi64)tishri1After + day - lengthOfAdarIAndII - 178);` |
|       - |  557 | `			}` |
|   38941 |  558 | `			break;` |
|   57227 |  559 | `		default:` |
|       - |  560 | `			/* Adar II or later -- also counted back from the next year. */` |
|  114455 |  561 | `			CalFindStartOfYear(year + 1,&metonicCycle,&metonicYear,` |
|       - |  562 | `				&moladDay,&moladHalakim,&tishri1After);` |
|  114455 |  563 | `			switch( month ){` |
|   16109 |  564 | `				case 7:  sdn = CalTruncInt((sxi64)tishri1After + day - 207); break;` |
|   16677 |  565 | `				case 8:  sdn = CalTruncInt((sxi64)tishri1After + day - 178); break;` |
|   16105 |  566 | `				case 9:  sdn = CalTruncInt((sxi64)tishri1After + day - 148); break;` |
|   16665 |  567 | `				case 10: sdn = CalTruncInt((sxi64)tishri1After + day - 119); break;` |
|   16119 |  568 | `				case 11: sdn = CalTruncInt((sxi64)tishri1After + day - 89);  break;` |
|   16661 |  569 | `				case 12: sdn = CalTruncInt((sxi64)tishri1After + day - 60);  break;` |
|   16111 |  570 | `				case 13: sdn = CalTruncInt((sxi64)tishri1After + day - 30);  break;` |
|      15 |  571 | `				default: return 0;` |
|       - |  572 | `			}` |
|   57220 |  573 | `	}` |
|  203323 |  574 | `	return sdn + JEWISH_SDN_OFFSET;` |
|  101677 |  575 | `}` |
|       - |  576 |  |
|       - |  577 | `/* ------------------------------------------------------------------ *` |
|       - |  578 | ` *  French republican                                                  *` |
|       - |  579 | ` * ------------------------------------------------------------------ */` |
|       - |  580 | `#define FRENCH_SDN_OFFSET  2375474` |
|       - |  581 | `#define FRENCH_DAYS_PER_MONTH 30` |
|       - |  582 | `#define FRENCH_FIRST_VALID 2375840` |
|       - |  583 | `#define FRENCH_LAST_VALID  2380952` |
|       - |  584 |  |
|   12944 |  585 | `static void CalSdnToFrench(sxi64 sdn,int *pYear,int *pMonth,int *pDay)` |
|       1 |  586 | `{` |
|       - |  587 | `	sxi64 temp;` |
|       - |  588 | `	int dayOfYear;` |
|   12945 |  589 | `	if( sdn < FRENCH_FIRST_VALID \|\| sdn > FRENCH_LAST_VALID ){` |
|      31 |  590 | `		*pYear = 0; *pMonth = 0; *pDay = 0;` |
|      31 |  591 | `		return;` |
|       - |  592 | `	}` |
|   12915 |  593 | `	temp = (sdn - FRENCH_SDN_OFFSET) * 4 - 1;` |
|   12915 |  594 | `	*pYear = (int)(temp / DAYS_PER_4_YEARS);` |
|   12915 |  595 | `	dayOfYear = (int)((temp % DAYS_PER_4_YEARS) / 4);` |
|   12915 |  596 | `	*pMonth = dayOfYear / FRENCH_DAYS_PER_MONTH + 1;` |
|   12915 |  597 | `	*pDay = dayOfYear % FRENCH_DAYS_PER_MONTH + 1;` |
|    6473 |  598 | `}` |
|   12944 |  599 | `static sxi64 CalFrenchToSdn(int year,int month,int day)` |
|       1 |  600 | `{` |
|       - |  601 | `	/* The calendar only ever ran 14 years, and the package refuses the rest. */` |
|   12945 |  602 | `	if( year < 1 \|\| year > 14 \|\| month < 1 \|\| month > 13 \|\| day < 1 \|\| day > 30 ){` |
|      27 |  603 | `		return 0;` |
|       - |  604 | `	}` |
|   19378 |  605 | `	return (((sxi64)year * DAYS_PER_4_YEARS) / 4` |
|   12918 |  606 | `			+ (month - 1) * FRENCH_DAYS_PER_MONTH` |
|   12918 |  607 | `			+ day` |
|   12918 |  608 | `			+ FRENCH_SDN_OFFSET);` |
|    6473 |  609 | `}` |
|       - |  610 | `/* Slot 13 is the five or six holidays that close a year, not a month. */` |
|       - |  611 | `static const char * const azFrenchMonth[14] = {` |
|       - |  612 | `	"","Vendemiaire","Brumaire","Frimaire","Nivose","Pluviose","Ventose",` |
|       - |  613 | `	"Germinal","Floreal","Prairial","Messidor","Thermidor","Fructidor","Extra"` |
|       - |  614 | `};` |
|       - |  615 |  |
|       - |  616 | `/* ------------------------------------------------------------------ *` |
|       - |  617 | ` *  Day of week                                                        *` |
|       - |  618 | ` * ------------------------------------------------------------------ */` |
|       - |  619 | `/*` |
|       - |  620 | ` * Plain arithmetic on the counter, with no calendar consulted -- which is why` |
|       - |  621 | ` * a serial day number every converter above rejects (0, a negative one, one` |
|       - |  622 | `` * past the Jewish maximum) still has a weekday, and why the `+ 8` is there: C`` |
|       - |  623 | ` * gives a negative remainder for a negative operand.` |
|       - |  624 | ` */` |
|   10798 |  625 | `static int CalDayOfWeek(sxi64 sdn)` |
|       1 |  626 | `{` |
|   10799 |  627 | `	return (int)(sdn % 7 + 8) % 7;` |
|       1 |  628 | `}` |
|       - |  629 | `static const char * const azDayShort[7] = {` |
|       - |  630 | `	"Sun","Mon","Tue","Wed","Thu","Fri","Sat"` |
|       - |  631 | `};` |
|       - |  632 | `static const char * const azDayLong[7] = {` |
|       - |  633 | `	"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"` |
|       - |  634 | `};` |
|       - |  635 |  |
|       - |  636 | `/* ------------------------------------------------------------------ *` |
|       - |  637 | ` *  The calendar table                                                 *` |
|       - |  638 | ` * ------------------------------------------------------------------ */` |
|       - |  639 | `/*` |
|       - |  640 | ` * The four calendars behind one id, in the order the CAL_* constants number` |
|       - |  641 | ` * them. The two lunisolar ones carry no separate abbreviations -- one table` |
|       - |  642 | ` * answers both of cal_info()'s name keys -- and the Jewish entry holds the` |
|       - |  643 | ` * LEAP-year spelling, which is what cal_info() shows; the per-YEAR choice` |
|       - |  644 | ` * between the two Jewish tables is made at the two places that know a year.` |
|       - |  645 | ` */` |
|       - |  646 | `#define CAL_NUM_CALS 4` |
|       - |  647 | `/* The ids the CAL_* constants carry, named here because three routines below` |
|       - |  648 | ` * ask "is this the Jewish one?" or "is this the French one?" about them. */` |
|       - |  649 | `#define CAL_ID_GREGORIAN 0` |
|       - |  650 | `#define CAL_ID_JULIAN    1` |
|       - |  651 | `#define CAL_ID_JEWISH    2` |
|       - |  652 | `#define CAL_ID_FRENCH    3` |
|       - |  653 | `typedef struct cal_entry cal_entry;` |
|       - |  654 | `struct cal_entry {` |
|       - |  655 | `	const char *zName;                          /* "Gregorian" */` |
|       - |  656 | `	const char *zSymbol;                        /* "CAL_GREGORIAN" */` |
|       - |  657 | `	sxi64 (*xToSdn)(int,int,int);` |
|       - |  658 | `	void (*xFromSdn)(sxi64,int *,int *,int *);` |
|       - |  659 | `	int nMonth;                                 /* 12, or 13 for the lunisolar pair */` |
|       - |  660 | `	int nMaxDayInMonth;` |
|       - |  661 | `	const char * const *azShort;` |
|       - |  662 | `	const char * const *azLong;` |
|       - |  663 | `};` |
|       - |  664 | `static const cal_entry aCalendar[CAL_NUM_CALS] = {` |
|       - |  665 | `	{ "Gregorian","CAL_GREGORIAN",CalGregorianToSdn,CalSdnToGregorian,12,31,` |
|       - |  666 | `	  azMonthShort,azMonthLong },` |
|       - |  667 | `	{ "Julian","CAL_JULIAN",CalJulianToSdn,CalSdnToJulian,12,31,` |
|       - |  668 | `	  azMonthShort,azMonthLong },` |
|       - |  669 | `	{ "Jewish","CAL_JEWISH",CalJewishToSdn,CalSdnToJewish,13,30,` |
|       - |  670 | `	  azJewishMonthLeap,azJewishMonthLeap },` |
|       - |  671 | `	{ "French","CAL_FRENCH",CalFrenchToSdn,CalSdnToFrench,13,30,` |
|       - |  672 | `	  azFrenchMonth,azFrenchMonth }` |
|       - |  673 | `};` |
|       - |  674 |  |
|       - |  675 | `/* ------------------------------------------------------------------ *` |
|       - |  676 | ` *  The PHP surface                                                    *` |
|       - |  677 | ` * ------------------------------------------------------------------ */` |
|       - |  678 | `/*` |
|       - |  679 | `` * Every `<calendar>tojd` builtin has the same shape: three int arguments read`` |
|       - |  680 | ` * in php's (month, day, year) ORDER and handed to the converter in the` |
|       - |  681 | ` * package's (year, month, day) one, each narrowed to an int on the way.` |
|       - |  682 | ` */` |
|  386670 |  683 | `static int CalToJdCommon(ph7_context *pCtx,ph7_value **apArg,` |
|       - |  684 | `	sxi64 (*xToSdn)(int,int,int))` |
|       1 |  685 | `{` |
|  386671 |  686 | `	int month = CalTruncInt(ph7_value_to_int64(apArg[0]));` |
|  386671 |  687 | `	int day   = CalTruncInt(ph7_value_to_int64(apArg[1]));` |
|  386671 |  688 | `	int year  = CalTruncInt(ph7_value_to_int64(apArg[2]));` |
|  386671 |  689 | `	ph7_result_int64(pCtx,xToSdn(year,month,day));` |
|  386671 |  690 | `	return PH7_OK;` |
|       1 |  691 | `}` |
|       - |  692 | ``/* And every `jdto<calendar>` the same "month/day/year" string, "0/0/0" when`` |
|       - |  693 | ` * the SDN falls outside the calendar. */` |
|  190336 |  694 | `static int CalFromJdCommon(ph7_context *pCtx,ph7_value **apArg,` |
|       - |  695 | `	void (*xFromSdn)(sxi64,int *,int *,int *))` |
|       1 |  696 | `{` |
|       - |  697 | `	int year,month,day;` |
|  190337 |  698 | `	xFromSdn(ph7_value_to_int64(apArg[0]),&year,&month,&day);` |
|  190337 |  699 | `	ph7_result_string_format(pCtx,"%d/%d/%d",month,day,year);` |
|  190337 |  700 | `	return PH7_OK;` |
|       1 |  701 | `}` |
|       - |  702 | `/*` |
|       - |  703 | ` * int gregoriantojd(int $month, int $day, int $year)` |
|       - |  704 | ` */` |
|   90220 |  705 | `PH7_PRIVATE int PH7_builtin_gregoriantojd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  706 | `{` |
|   90221 |  707 | `	if( nArg < 3 ){` |
|       - |  708 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|     ! 0 |  709 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  710 | `		return PH7_OK;` |
|       - |  711 | `	}` |
|   90221 |  712 | `	return CalToJdCommon(pCtx,apArg,CalGregorianToSdn);` |
|   45111 |  713 | `}` |
|       - |  714 | `/*` |
|       - |  715 | ` * string jdtogregorian(int $julian_day)` |
|       - |  716 | ` */` |
|   94336 |  717 | `PH7_PRIVATE int PH7_builtin_jdtogregorian(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  718 | `{` |
|   94337 |  719 | `	if( nArg < 1 ){` |
|     ! 0 |  720 | `		ph7_result_string(pCtx,"0/0/0",(int)sizeof("0/0/0") - 1);` |
|     ! 0 |  721 | `		return PH7_OK;` |
|       - |  722 | `	}` |
|   94337 |  723 | `	return CalFromJdCommon(pCtx,apArg,CalSdnToGregorian);` |
|   47169 |  724 | `}` |
|       - |  725 | `/*` |
|       - |  726 | ` * int juliantojd(int $month, int $day, int $year)` |
|       - |  727 | ` */` |
|   85742 |  728 | `PH7_PRIVATE int PH7_builtin_juliantojd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  729 | `{` |
|   85743 |  730 | `	if( nArg < 3 ){` |
|     ! 0 |  731 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  732 | `		return PH7_OK;` |
|       - |  733 | `	}` |
|   85743 |  734 | `	return CalToJdCommon(pCtx,apArg,CalJulianToSdn);` |
|   42872 |  735 | `}` |
|       - |  736 | `/*` |
|       - |  737 | ` * string jdtojulian(int $julian_day)` |
|       - |  738 | ` */` |
|   85742 |  739 | `PH7_PRIVATE int PH7_builtin_jdtojulian(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  740 | `{` |
|   85743 |  741 | `	if( nArg < 1 ){` |
|     ! 0 |  742 | `		ph7_result_string(pCtx,"0/0/0",(int)sizeof("0/0/0") - 1);` |
|     ! 0 |  743 | `		return PH7_OK;` |
|       - |  744 | `	}` |
|   85743 |  745 | `	return CalFromJdCommon(pCtx,apArg,CalSdnToJulian);` |
|   42872 |  746 | `}` |
|       - |  747 | `/*` |
|       - |  748 | ` * int frenchtojd(int $month, int $day, int $year)` |
|       - |  749 | ` */` |
|   10252 |  750 | `PH7_PRIVATE int PH7_builtin_frenchtojd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  751 | `{` |
|   10253 |  752 | `	if( nArg < 3 ){` |
|     ! 0 |  753 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  754 | `		return PH7_OK;` |
|       - |  755 | `	}` |
|   10253 |  756 | `	return CalToJdCommon(pCtx,apArg,CalFrenchToSdn);` |
|    5127 |  757 | `}` |
|       - |  758 | `/*` |
|       - |  759 | ` * string jdtofrench(int $julian_day)` |
|       - |  760 | ` */` |
|   10258 |  761 | `PH7_PRIVATE int PH7_builtin_jdtofrench(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  762 | `{` |
|   10259 |  763 | `	if( nArg < 1 ){` |
|     ! 0 |  764 | `		ph7_result_string(pCtx,"0/0/0",(int)sizeof("0/0/0") - 1);` |
|     ! 0 |  765 | `		return PH7_OK;` |
|       - |  766 | `	}` |
|   10259 |  767 | `	return CalFromJdCommon(pCtx,apArg,CalSdnToFrench);` |
|    5130 |  768 | `}` |
|       - |  769 | `/*` |
|       - |  770 | ` * int jewishtojd(int $month, int $day, int $year)` |
|       - |  771 | ` *  The one converter with a range check of its own: php screens the YEAR` |
|       - |  772 | ` *  against the int range instead of truncating it, so the diagnostic here` |
|       - |  773 | ` *  exists where the other three silently wrap.` |
|       - |  774 | ` */` |
|  200464 |  775 | `PH7_PRIVATE int PH7_builtin_jewishtojd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  776 | `{` |
|       - |  777 | `	sxi64 iYear;` |
|  200465 |  778 | `	if( nArg < 3 ){` |
|     ! 0 |  779 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  780 | `		return PH7_OK;` |
|       - |  781 | `	}` |
|  200465 |  782 | `	iYear = ph7_value_to_int64(apArg[2]);` |
|  200465 |  783 | `	if( iYear > CAL_INT_MAX \|\| iYear < CAL_INT_MIN ){` |
|       9 |  784 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  785 | `			"jewishtojd(): Argument #3 ($year) must be between %d and %d",` |
|       - |  786 | `			CAL_INT_MIN,CAL_INT_MAX);` |
|       - |  787 | `	}` |
|  200457 |  788 | `	return CalToJdCommon(pCtx,apArg,CalJewishToSdn);` |
|  100233 |  789 | `}` |
|       - |  790 | `/*` |
|       - |  791 | ` * The Hebrew numeral spelling of a number 1..9999, in ISO-8859-8 -- php's own` |
|       - |  792 | `` * `heb_number_to_chars`. The result is NOT unique: 5 and 5000 both spell to a`` |
|       - |  793 | ` * single he, which is why php's own comment says to use the numeric form for` |
|       - |  794 | ` * calculations. Answers 0 (and writes nothing) for a number outside the range.` |
|       - |  795 | ` *` |
|       - |  796 | ` * zBuf must hold at least 18 bytes plus the terminator, which is what the` |
|       - |  797 | ` * widest spelling (four alafim characters, the " alafim " word, tav-tav-...)` |
|       - |  798 | ` * can reach.` |
|       - |  799 | ` */` |
|       - |  800 | `#define CAL_HEB_BUF 24` |
|      48 |  801 | `static int CalHebNumberToChars(int n,int fl,char *zBuf)` |
|       1 |  802 | `{` |
|       - |  803 | `	/* "0" then the 22 letters of the alphabet, ISO-8859-8. */` |
|       - |  804 | `	static const char zAlefBet[24] =` |
|       - |  805 | `		"0\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEB\xEC\xEE\xF0\xF1\xF2\xF4\xF6\xF7\xF8\xF9\xFA";` |
|       - |  806 | `	char *p,*zEndOfAlafim;` |
|      49 |  807 | `	p = zEndOfAlafim = zBuf;` |
|       - |  808 | `	/* Prevents the option breaking the jewish beliefs, php says. */` |
|      49 |  809 | `	if( n > 9999 \|\| n < 1 ){` |
|     ! 0 |  810 | `		zBuf[0] = 0;` |
|     ! 0 |  811 | `		return 0;` |
|       - |  812 | `	}` |
|       - |  813 | `	/* alafim (thousands) case */` |
|      49 |  814 | `	if( n / 1000 ){` |
|      25 |  815 | `		*p++ = zAlefBet[n / 1000];` |
|      25 |  816 | `		if( CAL_JEWISH_ADD_ALAFIM_GERESH & fl ){` |
|       5 |  817 | `			*p++ = '\'';` |
|       2 |  818 | `		}` |
|      25 |  819 | `		if( CAL_JEWISH_ADD_ALAFIM & fl ){` |
|       - |  820 | `			/* The word "alafim" itself, spaced on both sides. */` |
|       5 |  821 | `			SyMemcpy(" \xE0\xEC\xF4\xE9\xED ",p,7);` |
|       5 |  822 | `			p += 7;` |
|       2 |  823 | `		}` |
|      25 |  824 | `		zEndOfAlafim = p;` |
|      25 |  825 | `		n = n % 1000;` |
|      12 |  826 | `	}` |
|       - |  827 | `	/* tav-tav (tav=400) case */` |
|      73 |  828 | `	while( n >= 400 ){` |
|      25 |  829 | `		*p++ = zAlefBet[22];` |
|      25 |  830 | `		n -= 400;` |
|       1 |  831 | `	}` |
|       - |  832 | `	/* meot (hundreds) case */` |
|      49 |  833 | `	if( n >= 100 ){` |
|      25 |  834 | `		*p++ = zAlefBet[18 + n / 100];` |
|      25 |  835 | `		n = n % 100;` |
|      12 |  836 | `	}` |
|      49 |  837 | `	if( n == 15 \|\| n == 16 ){` |
|       - |  838 | `		/* tet-vav and tet-zayin: 15 and 16 are never spelled with the divine` |
|       - |  839 | `		 * name's two letters. */` |
|       5 |  840 | `		*p++ = zAlefBet[9];` |
|       5 |  841 | `		*p++ = zAlefBet[n - 9];` |
|       3 |  842 | `	}else{` |
|       - |  843 | `		/* asarot (tens) case */` |
|      45 |  844 | `		if( n >= 10 ){` |
|      27 |  845 | `			*p++ = zAlefBet[9 + n / 10];` |
|      27 |  846 | `			n = n % 10;` |
|      13 |  847 | `		}` |
|       - |  848 | `		/* yehidot (ones) case */` |
|      45 |  849 | `		if( n > 0 ){` |
|      27 |  850 | `			*p++ = zAlefBet[n];` |
|      13 |  851 | `		}` |
|       - |  852 | `	}` |
|      49 |  853 | `	if( CAL_JEWISH_ADD_GERESHAYIM & fl ){` |
|      13 |  854 | `		switch( p - zEndOfAlafim ){` |
|     ! 0 |  855 | `			case 0:` |
|     ! 0 |  856 | `				break;` |
|       3 |  857 | `			case 1:` |
|       7 |  858 | `				*p++ = '\'';` |
|       7 |  859 | `				break;` |
|       3 |  860 | `			default:` |
|       - |  861 | `				/* The gershayim goes BEFORE the last letter, so that letter` |
|       - |  862 | `				 * moves one place along. */` |
|       7 |  863 | `				*p = *(p - 1);` |
|       7 |  864 | `				*(p - 1) = '"';` |
|       7 |  865 | `				p++;` |
|       6 |  866 | `				break;` |
|       - |  867 | `		}` |
|       6 |  868 | `	}` |
|      49 |  869 | `	*p = 0;` |
|      49 |  870 | `	return (int)(p - zBuf);` |
|      25 |  871 | `}` |
|       - |  872 | `/*` |
|       - |  873 | ` * string jdtojewish(int $julian_day, bool $hebrew = false, int $flags = 0)` |
|       - |  874 | ` */` |
|  200068 |  875 | `PH7_PRIVATE int PH7_builtin_jdtojewish(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  876 | `{` |
|       - |  877 | `	char zDay[CAL_HEB_BUF],zYear[CAL_HEB_BUF];` |
|  200069 |  878 | `	int year,month,day,fl = 0,bHeb = 0;` |
|  200069 |  879 | `	if( nArg < 1 ){` |
|     ! 0 |  880 | `		ph7_result_string(pCtx,"0/0/0",(int)sizeof("0/0/0") - 1);` |
|     ! 0 |  881 | `		return PH7_OK;` |
|       - |  882 | `	}` |
|  200069 |  883 | `	if( nArg > 1 ){` |
|      31 |  884 | `		bHeb = ph7_value_to_bool(apArg[1]);` |
|      31 |  885 | `		if( nArg > 2 ){` |
|      13 |  886 | `			fl = (int)ph7_value_to_int64(apArg[2]);` |
|       6 |  887 | `		}` |
|      15 |  888 | `	}` |
|  200069 |  889 | `	CalSdnToJewish(ph7_value_to_int64(apArg[0]),&year,&month,&day);` |
|  200069 |  890 | `	if( !bHeb ){` |
|  200039 |  891 | `		ph7_result_string_format(pCtx,"%d/%d/%d",month,day,year);` |
|  200039 |  892 | `		return PH7_OK;` |
|       - |  893 | `	}` |
|      31 |  894 | `	if( year <= 0 \|\| year > 9999 ){` |
|       - |  895 | `		/* The Hebrew spelling has no numeral for a year outside this range,` |
|       - |  896 | `		 * and php refuses rather than answering an ambiguous one. */` |
|       7 |  897 | `		return PH7_VmThrowException(pCtx,"ValueError","Year out of range (0-9999)");` |
|       - |  898 | `	}` |
|      25 |  899 | `	CalHebNumberToChars(day,fl,zDay);` |
|      25 |  900 | `	CalHebNumberToChars(year,fl,zYear);` |
|      37 |  901 | `	ph7_result_string_format(pCtx,"%s %s %s",` |
|      24 |  902 | `		zDay,CAL_JEWISH_HEB_MONTH_NAME(year)[month],zYear);` |
|      25 |  903 | `	return PH7_OK;` |
|  100035 |  904 | `}` |
|       - |  905 | `/*` |
|       - |  906 | ` * The calendar id every generic entry point screens first. Answers 1 and` |
|       - |  907 | ` * raises php's ValueError -- whose ARGUMENT NUMBER differs per function --` |
|       - |  908 | ` * when the id names no calendar.` |
|       - |  909 | ` */` |
|   21596 |  910 | `static int CalBadId(ph7_context *pCtx,sxi64 iCal,const char *zFn,int nPos)` |
|       2 |  911 | `{` |
|   21598 |  912 | `	if( iCal >= 0 && iCal < CAL_NUM_CALS ){` |
|   21574 |  913 | `		return 0;` |
|       - |  914 | `	}` |
|      37 |  915 | `	PH7_VmThrowException(pCtx,"ValueError",` |
|      12 |  916 | `		"%s(): Argument #%d ($calendar) must be a valid calendar ID",zFn,nPos);` |
|      25 |  917 | `	return 1;` |
|   10800 |  918 | `}` |
|       - |  919 | `/* One calendar's row of cal_info(), written into pOut. */` |
|      42 |  920 | `static void CalInfoOne(ph7_context *pCtx,const cal_entry *pCal,ph7_value *pOut)` |
|       1 |  921 | `{` |
|       - |  922 | `	ph7_value *pMonths,*pShort,*pVal;` |
|       - |  923 | `	int i;` |
|      43 |  924 | `	pMonths = ph7_context_new_array(pCtx);` |
|      43 |  925 | `	pShort  = ph7_context_new_array(pCtx);` |
|      43 |  926 | `	pVal    = ph7_context_new_scalar(pCtx);` |
|      43 |  927 | `	if( pMonths == 0 \|\| pShort == 0 \|\| pVal == 0 ){` |
|     ! 0 |  928 | `		return;` |
|       - |  929 | `	}` |
|     569 |  930 | `	for( i = 1 ; i <= pCal->nMonth ; ++i ){` |
|     527 |  931 | `		ph7_value_string(pVal,pCal->azLong[i],-1);` |
|     527 |  932 | `		ph7_array_add_intkey_elem(pMonths,i,pVal);` |
|     527 |  933 | `		ph7_value_reset_string_cursor(pVal);` |
|     527 |  934 | `		ph7_value_string(pVal,pCal->azShort[i],-1);` |
|     527 |  935 | `		ph7_array_add_intkey_elem(pShort,i,pVal);` |
|     527 |  936 | `		ph7_value_reset_string_cursor(pVal);` |
|     264 |  937 | `	}` |
|      43 |  938 | `	ph7_array_add_strkey_elem(pOut,"months",pMonths);` |
|      43 |  939 | `	ph7_array_add_strkey_elem(pOut,"abbrevmonths",pShort);` |
|      43 |  940 | `	ph7_value_int(pVal,pCal->nMaxDayInMonth);` |
|      43 |  941 | `	ph7_array_add_strkey_elem(pOut,"maxdaysinmonth",pVal);` |
|      43 |  942 | `	ph7_value_string(pVal,pCal->zName,-1);` |
|      43 |  943 | `	ph7_array_add_strkey_elem(pOut,"calname",pVal);` |
|      43 |  944 | `	ph7_value_reset_string_cursor(pVal);` |
|      43 |  945 | `	ph7_value_string(pVal,pCal->zSymbol,-1);` |
|      43 |  946 | `	ph7_array_add_strkey_elem(pOut,"calsymbol",pVal);` |
|      43 |  947 | `	ph7_context_release_value(pCtx,pVal);` |
|      43 |  948 | `	ph7_context_release_value(pCtx,pShort);` |
|      43 |  949 | `	ph7_context_release_value(pCtx,pMonths);` |
|      22 |  950 | `}` |
|       - |  951 | `/*` |
|       - |  952 | ` * array cal_info(int $calendar = -1)` |
|       - |  953 | ` *  -1 -- the default -- is not an error but a REQUEST for all four, keyed by` |
|       - |  954 | ` *  calendar id; every other negative value is refused.` |
|       - |  955 | ` */` |
|      28 |  956 | `PH7_PRIVATE int PH7_builtin_cal_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  957 | `{` |
|       - |  958 | `	ph7_value *pArray;` |
|      29 |  959 | `	sxi64 iCal = -1;` |
|      29 |  960 | `	if( nArg > 0 ){` |
|      27 |  961 | `		iCal = ph7_value_to_int64(apArg[0]);` |
|      13 |  962 | `	}` |
|      29 |  963 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 |  964 | `	if( pArray == 0 ){` |
|     ! 0 |  965 | `		ph7_result_null(pCtx);` |
|     ! 0 |  966 | `		return PH7_OK;` |
|       - |  967 | `	}` |
|      29 |  968 | `	if( iCal == -1 ){` |
|       - |  969 | `		int i;` |
|      41 |  970 | `		for( i = 0 ; i < CAL_NUM_CALS ; ++i ){` |
|      33 |  971 | `			ph7_value *pOne = ph7_context_new_array(pCtx);` |
|      33 |  972 | `			if( pOne == 0 ){` |
|     ! 0 |  973 | `				continue;` |
|       - |  974 | `			}` |
|      33 |  975 | `			CalInfoOne(pCtx,&aCalendar[i],pOne);` |
|      33 |  976 | `			ph7_array_add_intkey_elem(pArray,i,pOne);` |
|      33 |  977 | `			ph7_context_release_value(pCtx,pOne);` |
|      17 |  978 | `		}` |
|       9 |  979 | `		ph7_result_value(pCtx,pArray);` |
|       9 |  980 | `		return PH7_OK;` |
|       - |  981 | `	}` |
|      21 |  982 | `	if( CalBadId(pCtx,iCal,"cal_info",1) ){` |
|      11 |  983 | `		return PH7_OK;` |
|       - |  984 | `	}` |
|      11 |  985 | `	CalInfoOne(pCtx,&aCalendar[iCal],pArray);` |
|      11 |  986 | `	ph7_result_value(pCtx,pArray);` |
|      11 |  987 | `	return PH7_OK;` |
|      15 |  988 | `}` |
|       - |  989 | `/*` |
|       - |  990 | ` * int cal_days_in_month(int $calendar, int $month, int $year)` |
|       - |  991 | ` *  There is no month-length table anywhere in this extension: php converts the` |
|       - |  992 | ` *  first of the month and the first of the NEXT month and subtracts, which is` |
|       - |  993 | ` *  what lets one routine answer for a lunisolar calendar whose months move` |
|       - |  994 | ` *  from year to year. A month the calendar cannot convert is therefore a bare` |
|       - |  995 | ` *  "Invalid date" ValueError -- no function prefix, unlike the three argument` |
|       - |  996 | ` *  screens above it.` |
|       - |  997 | ` */` |
|     158 |  998 | `PH7_PRIVATE int PH7_builtin_cal_days_in_month(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  999 | `{` |
|       - | 1000 | `	const cal_entry *pCal;` |
|       - | 1001 | `	sxi64 iCal,iMonth,iYear,sdnStart,sdnNext;` |
|     160 | 1002 | `	if( nArg < 3 ){` |
|       - | 1003 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|     ! 0 | 1004 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1005 | `		return PH7_OK;` |
|       - | 1006 | `	}` |
|     160 | 1007 | `	iCal   = ph7_value_to_int64(apArg[0]);` |
|     160 | 1008 | `	iMonth = ph7_value_to_int64(apArg[1]);` |
|     160 | 1009 | `	iYear  = ph7_value_to_int64(apArg[2]);` |
|     160 | 1010 | `	if( CalBadId(pCtx,iCal,"cal_days_in_month",1) ){` |
|       5 | 1011 | `		return PH7_OK;` |
|       - | 1012 | `	}` |
|     156 | 1013 | `	if( iMonth <= 0 \|\| iMonth > CAL_INT_MAX - 1 ){` |
|       5 | 1014 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1015 | `			"cal_days_in_month(): Argument #2 ($month) must be between 1 and %d",` |
|       - | 1016 | `			CAL_INT_MAX - 1);` |
|       - | 1017 | `	}` |
|     152 | 1018 | `	if( iYear > CAL_INT_MAX - 1 ){` |
|       3 | 1019 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1020 | `			"cal_days_in_month(): Argument #3 ($year) must be less than %d",` |
|       - | 1021 | `			CAL_INT_MAX - 1);` |
|       - | 1022 | `	}` |
|     150 | 1023 | `	pCal = &aCalendar[iCal];` |
|     150 | 1024 | `	sdnStart = pCal->xToSdn(CalTruncInt(iYear),CalTruncInt(iMonth),1);` |
|     150 | 1025 | `	if( sdnStart == 0 ){` |
|      19 | 1026 | `		return PH7_VmThrowException(pCtx,"ValueError","Invalid date");` |
|       - | 1027 | `	}` |
|     132 | 1028 | `	sdnNext = pCal->xToSdn(CalTruncInt(iYear),CalTruncInt(iMonth + 1),1);` |
|     132 | 1029 | `	if( sdnNext == 0 ){` |
|       - | 1030 | `		/* The next month is the next YEAR's first -- and the year after 1 B.C.` |
|       - | 1031 | `		 * is 1 A.D., not 0. */` |
|      17 | 1032 | `		if( iYear == -1 ){` |
|       5 | 1033 | `			sdnNext = pCal->xToSdn(1,1,1);` |
|       3 | 1034 | `		}else{` |
|      13 | 1035 | `			sdnNext = pCal->xToSdn(CalTruncInt(iYear + 1),1,1);` |
|      13 | 1036 | `			if( iCal == CAL_ID_FRENCH && sdnNext == 0 ){` |
|       - | 1037 | `				/* The French calendar ends at 0014-13-05, so its last month has` |
|       - | 1038 | `				 * no next year to be measured against. */` |
|       3 | 1039 | `				sdnNext = 2380953;` |
|       1 | 1040 | `			}` |
|       - | 1041 | `		}` |
|       8 | 1042 | `	}` |
|     132 | 1043 | `	ph7_result_int64(pCtx,sdnNext - sdnStart);` |
|     132 | 1044 | `	return PH7_OK;` |
|      81 | 1045 | `}` |
|       - | 1046 | `/*` |
|       - | 1047 | ` * int cal_to_jd(int $calendar, int $month, int $day, int $year)` |
|       - | 1048 | ` *  Each argument gets a different screen: the day the whole int range, the` |
|       - | 1049 | ` *  month a 1-based one, and the year only an UPPER bound -- so a year below` |
|       - | 1050 | ` *  the int range is narrowed rather than refused.` |
|       - | 1051 | ` */` |
|   10708 | 1052 | `PH7_PRIVATE int PH7_builtin_cal_to_jd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1053 | `{` |
|       - | 1054 | `	sxi64 iCal,iMonth,iDay,iYear;` |
|   10709 | 1055 | `	if( nArg < 4 ){` |
|     ! 0 | 1056 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1057 | `		return PH7_OK;` |
|       - | 1058 | `	}` |
|   10709 | 1059 | `	iCal   = ph7_value_to_int64(apArg[0]);` |
|   10709 | 1060 | `	iMonth = ph7_value_to_int64(apArg[1]);` |
|   10709 | 1061 | `	iDay   = ph7_value_to_int64(apArg[2]);` |
|   10709 | 1062 | `	iYear  = ph7_value_to_int64(apArg[3]);` |
|   10709 | 1063 | `	if( CalBadId(pCtx,iCal,"cal_to_jd",1) ){` |
|       5 | 1064 | `		return PH7_OK;` |
|       - | 1065 | `	}` |
|   10705 | 1066 | `	if( iMonth <= 0 \|\| iMonth > CAL_INT_MAX - 1 ){` |
|       5 | 1067 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1068 | `			"cal_to_jd(): Argument #2 ($month) must be between 1 and %d",CAL_INT_MAX - 1);` |
|       - | 1069 | `	}` |
|   10701 | 1070 | `	if( iDay > CAL_INT_MAX \|\| iDay < CAL_INT_MIN ){` |
|       5 | 1071 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1072 | `			"cal_to_jd(): Argument #3 ($day) must be between %d and %d",` |
|       - | 1073 | `			CAL_INT_MIN,CAL_INT_MAX);` |
|       - | 1074 | `	}` |
|   10697 | 1075 | `	if( iYear > CAL_INT_MAX - 1 ){` |
|       3 | 1076 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1077 | `			"cal_to_jd(): Argument #4 ($year) must be less than %d",CAL_INT_MAX - 1);` |
|       - | 1078 | `	}` |
|   16042 | 1079 | `	ph7_result_int64(pCtx,aCalendar[iCal].xToSdn(CalTruncInt(iYear),` |
|    5347 | 1080 | `		CalTruncInt(iMonth),CalTruncInt(iDay)));` |
|   10695 | 1081 | `	return PH7_OK;` |
|    5355 | 1082 | `}` |
|       - | 1083 | `/*` |
|       - | 1084 | ` * array cal_from_jd(int $julian_day, int $calendar)` |
|       - | 1085 | ` *  Nine keys. Two are special-cased for the Jewish calendar and for it alone:` |
|       - | 1086 | ` *  a serial day number BEFORE that calendar begins answers a NULL day of week` |
|       - | 1087 | ` *  and two empty day names, where every other calendar still names one -- the` |
|       - | 1088 | ` *  weekday is arithmetic on the counter and does not care whether the date` |
|       - | 1089 | ` *  exists -- and its month names come from the YEAR's own leap/regular table.` |
|       - | 1090 | ` */` |
|   10710 | 1091 | `PH7_PRIVATE int PH7_builtin_cal_from_jd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1092 | `{` |
|       - | 1093 | `	const cal_entry *pCal;` |
|       - | 1094 | `	ph7_value *pArray,*pVal;` |
|       - | 1095 | `	sxi64 iJd,iCal;` |
|       - | 1096 | `	int year,month,day;` |
|   10711 | 1097 | `	if( nArg < 2 ){` |
|     ! 0 | 1098 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1099 | `		return PH7_OK;` |
|       - | 1100 | `	}` |
|   10711 | 1101 | `	iJd  = ph7_value_to_int64(apArg[0]);` |
|   10711 | 1102 | `	iCal = ph7_value_to_int64(apArg[1]);` |
|   10711 | 1103 | `	if( CalBadId(pCtx,iCal,"cal_from_jd",2) ){` |
|       7 | 1104 | `		return PH7_OK;` |
|       - | 1105 | `	}` |
|   10705 | 1106 | `	pCal = &aCalendar[iCal];` |
|   10705 | 1107 | `	pArray = ph7_context_new_array(pCtx);` |
|   10705 | 1108 | `	pVal   = ph7_context_new_scalar(pCtx);` |
|   10705 | 1109 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|     ! 0 | 1110 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1111 | `		return PH7_OK;` |
|       - | 1112 | `	}` |
|   10705 | 1113 | `	pCal->xFromSdn(iJd,&year,&month,&day);` |
|   10705 | 1114 | `	ph7_value_string_format(pVal,"%d/%d/%d",month,day,year);` |
|   10705 | 1115 | `	ph7_array_add_strkey_elem(pArray,"date",pVal);` |
|   10705 | 1116 | `	ph7_value_reset_string_cursor(pVal);` |
|   10705 | 1117 | `	ph7_value_int(pVal,month); ph7_array_add_strkey_elem(pArray,"month",pVal);` |
|   10705 | 1118 | `	ph7_value_int(pVal,day);   ph7_array_add_strkey_elem(pArray,"day",pVal);` |
|   10705 | 1119 | `	ph7_value_int(pVal,year);  ph7_array_add_strkey_elem(pArray,"year",pVal);` |
|   16053 | 1120 | `	if( iCal != CAL_ID_JEWISH \|\| year > 0 ){` |
|   10697 | 1121 | `		int dow = CalDayOfWeek(iJd);` |
|   10697 | 1122 | `		ph7_value_int(pVal,dow);` |
|   10697 | 1123 | `		ph7_array_add_strkey_elem(pArray,"dow",pVal);` |
|   10697 | 1124 | `		ph7_value_string(pVal,azDayShort[dow],-1);` |
|   10697 | 1125 | `		ph7_array_add_strkey_elem(pArray,"abbrevdayname",pVal);` |
|   10697 | 1126 | `		ph7_value_reset_string_cursor(pVal);` |
|   10697 | 1127 | `		ph7_value_string(pVal,azDayLong[dow],-1);` |
|   10697 | 1128 | `		ph7_array_add_strkey_elem(pArray,"dayname",pVal);` |
|   10697 | 1129 | `		ph7_value_reset_string_cursor(pVal);` |
|    5349 | 1130 | `	}else{` |
|       9 | 1131 | `		ph7_value_null(pVal);` |
|       9 | 1132 | `		ph7_array_add_strkey_elem(pArray,"dow",pVal);` |
|       9 | 1133 | `		ph7_value_string(pVal,"",0);` |
|       9 | 1134 | `		ph7_array_add_strkey_elem(pArray,"abbrevdayname",pVal);` |
|       9 | 1135 | `		ph7_array_add_strkey_elem(pArray,"dayname",pVal);` |
|       9 | 1136 | `		ph7_value_reset_string_cursor(pVal);` |
|       - | 1137 | `	}` |
|   10705 | 1138 | `	if( iCal == CAL_ID_JEWISH ){` |
|    2685 | 1139 | `		const char *zMonth = year > 0 ? CAL_JEWISH_MONTH_NAME(year)[month] : "";` |
|    2685 | 1140 | `		ph7_value_string(pVal,zMonth,-1);` |
|    2685 | 1141 | `		ph7_array_add_strkey_elem(pArray,"abbrevmonth",pVal);` |
|    2685 | 1142 | `		ph7_array_add_strkey_elem(pArray,"monthname",pVal);` |
|    1343 | 1143 | `	}else{` |
|    8021 | 1144 | `		ph7_value_string(pVal,pCal->azShort[month],-1);` |
|    8021 | 1145 | `		ph7_array_add_strkey_elem(pArray,"abbrevmonth",pVal);` |
|    8021 | 1146 | `		ph7_value_reset_string_cursor(pVal);` |
|    8021 | 1147 | `		ph7_value_string(pVal,pCal->azLong[month],-1);` |
|    8021 | 1148 | `		ph7_array_add_strkey_elem(pArray,"monthname",pVal);` |
|       - | 1149 | `	}` |
|   10705 | 1150 | `	ph7_context_release_value(pCtx,pVal);` |
|   10705 | 1151 | `	ph7_result_value(pCtx,pArray);` |
|   10705 | 1152 | `	return PH7_OK;` |
|    5356 | 1153 | `}` |
|       - | 1154 | `/*` |
|       - | 1155 | ` * int\|string jddayofweek(int $julian_day, int $mode = CAL_DOW_DAYNO)` |
|       - | 1156 | ` *  Only two modes answer a name; every other value -- negative, unknown, out` |
|       - | 1157 | ` *  of range -- falls through to the day NUMBER.` |
|       - | 1158 | ` */` |
|     102 | 1159 | `PH7_PRIVATE int PH7_builtin_jddayofweek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1160 | `{` |
|     103 | 1161 | `	sxi64 iMode = 0;` |
|       - | 1162 | `	int dow;` |
|     103 | 1163 | `	if( nArg < 1 ){` |
|     ! 0 | 1164 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1165 | `		return PH7_OK;` |
|       - | 1166 | `	}` |
|     103 | 1167 | `	dow = CalDayOfWeek(ph7_value_to_int64(apArg[0]));` |
|     103 | 1168 | `	if( nArg > 1 ){` |
|      75 | 1169 | `		iMode = ph7_value_to_int64(apArg[1]);` |
|      37 | 1170 | `	}` |
|     103 | 1171 | `	if( iMode == 1 ){` |
|      37 | 1172 | `		ph7_result_string(pCtx,azDayLong[dow],-1);` |
|      85 | 1173 | `	}else if( iMode == 2 ){` |
|      27 | 1174 | `		ph7_result_string(pCtx,azDayShort[dow],-1);` |
|      14 | 1175 | `	}else{` |
|      41 | 1176 | `		ph7_result_int(pCtx,dow);` |
|       - | 1177 | `	}` |
|     103 | 1178 | `	return PH7_OK;` |
|      52 | 1179 | `}` |
|       - | 1180 | `/*` |
|       - | 1181 | ` * string jdmonthname(int $julian_day, int $mode)` |
|       - | 1182 | ` *  $mode picks the CALENDAR as well as the spelling, and its six values are` |
|       - | 1183 | ` *  not in the order the calendar ids are: 0/1 Gregorian short/long, 2/3` |
|       - | 1184 | ` *  Julian, 4 Jewish, 5 French. Anything else is the Gregorian short name. A` |
|       - | 1185 | ` *  day the chosen calendar cannot place lands on month slot 0, which is the` |
|       - | 1186 | ` *  empty string in every table.` |
|       - | 1187 | ` */` |
|     122 | 1188 | `PH7_PRIVATE int PH7_builtin_jdmonthname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1189 | `{` |
|       - | 1190 | `	const char *zMonth;` |
|       - | 1191 | `	sxi64 iJd,iMode;` |
|       - | 1192 | `	int year,month,day;` |
|     123 | 1193 | `	if( nArg < 2 ){` |
|     ! 0 | 1194 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1195 | `		return PH7_OK;` |
|       - | 1196 | `	}` |
|     123 | 1197 | `	iJd = ph7_value_to_int64(apArg[0]);` |
|     123 | 1198 | `	iMode = ph7_value_to_int64(apArg[1]);` |
|     123 | 1199 | `	switch( iMode ){` |
|      18 | 1200 | `		case 1:` |
|      37 | 1201 | `			CalSdnToGregorian(iJd,&year,&month,&day);` |
|      37 | 1202 | `			zMonth = azMonthLong[month];` |
|      37 | 1203 | `			break;` |
|       7 | 1204 | `		case 2:` |
|      15 | 1205 | `			CalSdnToJulian(iJd,&year,&month,&day);` |
|      15 | 1206 | `			zMonth = azMonthShort[month];` |
|      15 | 1207 | `			break;` |
|       7 | 1208 | `		case 3:` |
|      15 | 1209 | `			CalSdnToJulian(iJd,&year,&month,&day);` |
|      15 | 1210 | `			zMonth = azMonthLong[month];` |
|      15 | 1211 | `			break;` |
|      10 | 1212 | `		case 4:` |
|      21 | 1213 | `			CalSdnToJewish(iJd,&year,&month,&day);` |
|      21 | 1214 | `			zMonth = year > 0 ? CAL_JEWISH_MONTH_NAME(year)[month] : "";` |
|      21 | 1215 | `			break;` |
|       7 | 1216 | `		case 5:` |
|      15 | 1217 | `			CalSdnToFrench(iJd,&year,&month,&day);` |
|      15 | 1218 | `			zMonth = azFrenchMonth[month];` |
|      15 | 1219 | `			break;` |
|      12 | 1220 | `		default:` |
|      25 | 1221 | `			CalSdnToGregorian(iJd,&year,&month,&day);` |
|      25 | 1222 | `			zMonth = azMonthShort[month];` |
|      24 | 1223 | `			break;` |
|       - | 1224 | `	}` |
|     123 | 1225 | `	ph7_result_string(pCtx,zMonth,-1);` |
|     123 | 1226 | `	return PH7_OK;` |
|      62 | 1227 | `}` |
|       - | 1228 |  |
|       - | 1229 | `/* ------------------------------------------------------------------ *` |
|       - | 1230 | ` *  The clock                                                          *` |
|       - | 1231 | ` * ------------------------------------------------------------------ */` |
|       - | 1232 | `/*` |
|       - | 1233 | ` * These four are the only doors this extension has onto a clock, and both of` |
|       - | 1234 | ` * them are the C LIBRARY's rather than the engine's: php breaks a timestamp` |
|       - | 1235 | ` * down with localtime() and builds one with mktime(), so unixtojd() and` |
|       - | 1236 | `` * easter_date() read the PROCESS timezone and not `date.timezone`. Reproduced`` |
|       - | 1237 | ` * that way here -- an engine-local UTC breakdown would answer differently from` |
|       - | 1238 | ` * php on any box that is not set to UTC, which is a divergence nobody asked` |
|       - | 1239 | ` * for.` |
|       - | 1240 | ` */` |
|       - | 1241 | `#define CAL_SECS_PER_DAY (24 * 3600)` |
|       - | 1242 | `/* The serial day number of 1 January 1970. */` |
|       - | 1243 | `#define CAL_UNIX_EPOCH_JD 2440588` |
|       - | 1244 | `/* easter_days()/easter_date()'s four $mode policies, spelled here as well as` |
|       - | 1245 | ` * in the constant table (constant.c) because the computation branches on them. */` |
|       - | 1246 | `#define CAL_EASTER_DEFAULT          0` |
|       - | 1247 | `#define CAL_EASTER_ROMAN            1` |
|       - | 1248 | `#define CAL_EASTER_ALWAYS_GREGORIAN 2` |
|       - | 1249 | `#define CAL_EASTER_ALWAYS_JULIAN    3` |
|       - | 1250 | `/*` |
|       - | 1251 | ` * localtime(3) into a caller-supplied buffer. The plain form is what php` |
|       - | 1252 | ` * calls, but MSVC deprecates it and this tree builds with /WX, so the two` |
|       - | 1253 | ` * reentrant spellings are used instead -- and they take their arguments in` |
|       - | 1254 | ` * OPPOSITE orders, which is why this wrapper exists at all. Same split as` |
|       - | 1255 | ` * src/phl/server.c.` |
|       - | 1256 | ` */` |
|   15340 | 1257 | `static struct tm *CalLocalTime(const time_t *pWhen,struct tm *pOut)` |
|       1 | 1258 | `{` |
|       - | 1259 | `#ifdef __WINNT__` |
|       1 | 1260 | `	return localtime_s(pOut,pWhen) == 0 ? pOut : 0;` |
|       - | 1261 | `#else` |
|   15340 | 1262 | `	return localtime_r(pWhen,pOut);` |
|       - | 1263 | `#endif` |
|       1 | 1264 | `}` |
|       - | 1265 | `/*` |
|       - | 1266 | `` * `a + b` with php's own wrap. easter_days()'s year screen admits values up to`` |
|       - | 1267 | `` * `LONG_MAX / 5 * 4`, which is exactly the headroom the GREGORIAN branch's`` |
|       - | 1268 | `` * `year + year/4 - year/100 + year/400` needs -- but the JULIAN branch adds`` |
|       - | 1269 | `` * `year + year/4 + 5`, i.e. 1.25 * year, and that overflows for the last two`` |
|       - | 1270 | ` * years the screen lets through. php's answer for those two is built on the` |
|       - | 1271 | `` * wrap (`easter_days(LONG_MAX/5*4, CAL_EASTER_ALWAYS_JULIAN)` is 4), so the`` |
|       - | 1272 | ` * wrap is reproduced here in UNSIGNED arithmetic rather than left to signed` |
|       - | 1273 | ` * overflow -- which is undefined, and which this tree's UBSan build traps.` |
|       - | 1274 | ` */` |
|     684 | 1275 | `static sxi64 CalWrapAdd(sxi64 a,sxi64 b)` |
|       1 | 1276 | `{` |
|     685 | 1277 | `	sxu64 u = (sxu64)a + (sxu64)b;` |
|     685 | 1278 | `	if( u >= ((sxu64)1 << 63) ){` |
|     ! 0 | 1279 | `		return (sxi64)(u - ((sxu64)1 << 63)) + SMALLEST_INT64;` |
|       - | 1280 | `	}` |
|     685 | 1281 | `	return (sxi64)u;` |
|     343 | 1282 | `}` |
|       - | 1283 | `/*` |
|       - | 1284 | ` * int\|false unixtojd(?int $timestamp = null)` |
|       - | 1285 | ` *  Not the plain inverse of jdtounix(): the timestamp is broken down into a` |
|       - | 1286 | ` *  local calendar date first, and that date is what gets converted.` |
|       - | 1287 | ` */` |
|   15342 | 1288 | `PH7_PRIVATE int PH7_builtin_unixtojd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1289 | `{` |
|       - | 1290 | `	struct tm sTm,*pTm;` |
|       - | 1291 | `	time_t ts;` |
|   15343 | 1292 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|       5 | 1293 | `		time(&ts);` |
|       3 | 1294 | `	}else{` |
|   15339 | 1295 | `		sxi64 iTs = ph7_value_to_int64(apArg[0]);` |
|   15339 | 1296 | `		if( iTs < 0 ){` |
|       7 | 1297 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1298 | `				"unixtojd(): Argument #1 ($timestamp) must be greater than or equal to 0");` |
|       - | 1299 | `		}` |
|   15333 | 1300 | `		ts = (time_t)iTs;` |
|       - | 1301 | `	}` |
|   15337 | 1302 | `	pTm = CalLocalTime(&ts,&sTm);` |
|   15337 | 1303 | `	if( pTm == 0 ){` |
|       - | 1304 | `		/* The one FALSE in this extension: a timestamp the platform's own` |
|       - | 1305 | `		 * breakdown will not accept. */` |
|     ! 0 | 1306 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1307 | `		return PH7_OK;` |
|       - | 1308 | `	}` |
|   23005 | 1309 | `	ph7_result_int64(pCtx,` |
|   15336 | 1310 | `		CalGregorianToSdn(pTm->tm_year + 1900,pTm->tm_mon + 1,pTm->tm_mday));` |
|   15337 | 1311 | `	return PH7_OK;` |
|    7672 | 1312 | `}` |
|       - | 1313 | `/*` |
|       - | 1314 | ` * int jdtounix(int $julian_day)` |
|       - | 1315 | ` *  Pure arithmetic, so the answer is always midnight UTC. Its ValueError is` |
|       - | 1316 | ` *  the only one in this extension that names neither the function nor the` |
|       - | 1317 | ` *  argument -- php raises it with zend_value_error() rather than the argument` |
|       - | 1318 | ` *  helper, and the wording says "jday" rather than "$julian_day".` |
|       - | 1319 | ` */` |
|    8026 | 1320 | `PH7_PRIVATE int PH7_builtin_jdtounix(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1321 | `{` |
|       - | 1322 | `	sxi64 uday;` |
|    8027 | 1323 | `	if( nArg < 1 ){` |
|       - | 1324 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|     ! 0 | 1325 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1326 | `		return PH7_OK;` |
|       - | 1327 | `	}` |
|    8027 | 1328 | `	uday = ph7_value_to_int64(apArg[0]);` |
|       - | 1329 | `	/* The lower test runs first, so the subtraction below it cannot underflow. */` |
|    8026 | 1330 | `	if( uday < CAL_UNIX_EPOCH_JD` |
|    8023 | 1331 | `	 \|\| (uday - CAL_UNIX_EPOCH_JD) > (CAL_I64_MAX / CAL_SECS_PER_DAY) ){` |
|      13 | 1332 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1333 | `			"jday must be between %d and %qd",CAL_UNIX_EPOCH_JD,` |
|       - | 1334 | `			CAL_I64_MAX / CAL_SECS_PER_DAY + CAL_UNIX_EPOCH_JD);` |
|       - | 1335 | `	}` |
|    8015 | 1336 | `	uday -= CAL_UNIX_EPOCH_JD;` |
|    8015 | 1337 | `	ph7_result_int64(pCtx,uday * CAL_SECS_PER_DAY);` |
|    8015 | 1338 | `	return PH7_OK;` |
|    4014 | 1339 | `}` |
|       - | 1340 | `/*` |
|       - | 1341 | ` * The Easter computation both easter_days() and easter_date() run, from Simon` |
|       - | 1342 | `` * Kershaw's by way of php. `bGm` picks which of the two answers comes out: the`` |
|       - | 1343 | ` * number of days after 21 March, or the timestamp of midnight that morning.` |
|       - | 1344 | ` *` |
|       - | 1345 | ` * $mode is not a choice between two rules but between four POLICIES, and the` |
|       - | 1346 | ` * default is date-dependent: Julian up to 1582, Julian again for 1583-1752` |
|       - | 1347 | ` * (England kept the old calendar that long), Gregorian after that.` |
|       - | 1348 | ` */` |
|    2700 | 1349 | `static int CalEaster(ph7_context *pCtx,int nArg,ph7_value **apArg,int bGm)` |
|       1 | 1350 | `{` |
|    2701 | 1351 | `	const char *zFn = bGm ? "easter_date" : "easter_days";` |
|    2701 | 1352 | `	const sxi64 maxYear = (CAL_I64_MAX / 5) * 4;` |
|    2701 | 1353 | `	sxi64 year = 0,method = 0;` |
|       - | 1354 | `	sxi64 golden,solar,lunar,pfm,dom,tmp,easter;` |
|    2701 | 1355 | `	int bYearNull = 1;` |
|    2701 | 1356 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|    2697 | 1357 | `		year = ph7_value_to_int64(apArg[0]);` |
|    2697 | 1358 | `		bYearNull = 0;` |
|    1348 | 1359 | `	}` |
|    2701 | 1360 | `	if( nArg > 1 ){` |
|      95 | 1361 | `		method = ph7_value_to_int64(apArg[1]);` |
|      47 | 1362 | `	}` |
|    2701 | 1363 | `	if( bYearNull ){` |
|       - | 1364 | `		/* Default to the current year, read the same way php reads it. */` |
|       - | 1365 | `		struct tm sNow,*pTm;` |
|       - | 1366 | `		time_t now;` |
|       5 | 1367 | `		time(&now);` |
|       5 | 1368 | `		pTm = CalLocalTime(&now,&sNow);` |
|       5 | 1369 | `		year = pTm == 0 ? 1900 : 1900 + pTm->tm_year;` |
|       2 | 1370 | `	}` |
|    2701 | 1371 | `	if( year <= 0 \|\| year > maxYear ){` |
|      22 | 1372 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       7 | 1373 | `			"%s(): Argument #1 ($year) must be between 1 and %qd",zFn,maxYear);` |
|       - | 1374 | `	}` |
|    2687 | 1375 | `	if( bGm ){` |
|       - | 1376 | `		/* The timestamp form narrows the year twice more: there is no timestamp` |
|       - | 1377 | `		 * before 1970, and php stops at the year two billion. */` |
|     527 | 1378 | `		if( year < 1970 ){` |
|       5 | 1379 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1380 | `				"easter_date(): Argument #1 ($year) must be a year after 1970 (inclusive)");` |
|       - | 1381 | `		}` |
|     523 | 1382 | `		if( year > 2000000000 ){` |
|       5 | 1383 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1384 | `				"easter_date(): Argument #1 ($year) must be a year before 2.000.000.000 (inclusive)");` |
|       - | 1385 | `		}` |
|     259 | 1386 | `	}` |
|    2679 | 1387 | `	golden = (year % 19) + 1;                       /* the Golden number */` |
|    2678 | 1388 | `	if( (year <= 1582 && method != CAL_EASTER_ALWAYS_GREGORIAN)` |
|    2673 | 1389 | `	 \|\| (year >= 1583 && year <= 1752 && method != CAL_EASTER_ROMAN` |
|     327 | 1390 | `	     && method != CAL_EASTER_ALWAYS_GREGORIAN)` |
|    2509 | 1391 | `	 \|\| method == CAL_EASTER_ALWAYS_JULIAN ){` |
|       - | 1392 | `		/* JULIAN CALENDAR */` |
|       - | 1393 | `		/* CalWrapAdd(): 1.25 * year overflows at the top of the year screen,` |
|       - | 1394 | `		 * and php's answer there is the wrapped one. */` |
|     343 | 1395 | `		dom = CalWrapAdd(CalWrapAdd(year,year / 4),5) % 7;  /* "Dominical number" */` |
|     343 | 1396 | `		if( dom < 0 ){` |
|     ! 0 | 1397 | `			dom += 7;` |
|     ! 0 | 1398 | `		}` |
|     343 | 1399 | `		pfm = (3 - (11 * golden) - 7) % 30;         /* the Paschal full moon */` |
|     343 | 1400 | `		if( pfm < 0 ){` |
|     327 | 1401 | `			pfm += 30;` |
|     163 | 1402 | `		}` |
|     172 | 1403 | `	}else{` |
|       - | 1404 | `		/* GREGORIAN CALENDAR */` |
|    2337 | 1405 | `		dom = (year + (year / 4) - (year / 100) + (year / 400)) % 7;` |
|    2337 | 1406 | `		if( dom < 0 ){` |
|     ! 0 | 1407 | `			dom += 7;` |
|     ! 0 | 1408 | `		}` |
|    2337 | 1409 | `		solar = (year - 1600) / 100 - (year - 1600) / 400;` |
|    2337 | 1410 | `		lunar = (((year - 1400) / 100) * 8) / 25;` |
|    2337 | 1411 | `		pfm = (3 - (11 * golden) + solar - lunar) % 30;` |
|    2337 | 1412 | `		if( pfm < 0 ){` |
|    2293 | 1413 | `			pfm += 30;` |
|    1146 | 1414 | `		}` |
|       - | 1415 | `	}` |
|    2679 | 1416 | `	if( (pfm == 29) \|\| (pfm == 28 && golden > 11) ){` |
|     179 | 1417 | `		pfm--;                                      /* corrected full moon */` |
|      89 | 1418 | `	}` |
|    2679 | 1419 | `	tmp = (4 - pfm - dom) % 7;` |
|    2679 | 1420 | `	if( tmp < 0 ){` |
|    2165 | 1421 | `		tmp += 7;` |
|    1082 | 1422 | `	}` |
|    2679 | 1423 | `	easter = pfm + tmp + 1;    /* Easter, as days after 21 March */` |
|    2679 | 1424 | `	if( !bGm ){` |
|    2161 | 1425 | `		ph7_result_int64(pCtx,easter);` |
|    2161 | 1426 | `		return PH7_OK;` |
|       - | 1427 | `	}` |
|       - | 1428 | `	{` |
|       - | 1429 | `		struct tm te;` |
|     519 | 1430 | `		SyZero(&te,sizeof(te));` |
|     519 | 1431 | `		te.tm_isdst = -1;` |
|     519 | 1432 | `		te.tm_year = (int)(year - 1900);` |
|     519 | 1433 | `		te.tm_sec = 0;` |
|     519 | 1434 | `		te.tm_min = 0;` |
|     519 | 1435 | `		te.tm_hour = 0;` |
|     519 | 1436 | `		if( easter < 11 ){` |
|     119 | 1437 | `			te.tm_mon = 2;                          /* March */` |
|     119 | 1438 | `			te.tm_mday = (int)easter + 21;` |
|      60 | 1439 | `		}else{` |
|     401 | 1440 | `			te.tm_mon = 3;                          /* April */` |
|     401 | 1441 | `			te.tm_mday = (int)easter - 10;` |
|       - | 1442 | `		}` |
|     519 | 1443 | `		ph7_result_int64(pCtx,(sxi64)mktime(&te));` |
|       - | 1444 | `	}` |
|     519 | 1445 | `	return PH7_OK;` |
|    1351 | 1446 | `}` |
|       - | 1447 | `/*` |
|       - | 1448 | ` * int easter_days(?int $year = null, int $mode = CAL_EASTER_DEFAULT)` |
|       - | 1449 | ` */` |
|    2168 | 1450 | `PH7_PRIVATE int PH7_builtin_easter_days(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1451 | `{` |
|    2169 | 1452 | `	return CalEaster(pCtx,nArg,apArg,0);` |
|       1 | 1453 | `}` |
|       - | 1454 | `/*` |
|       - | 1455 | ` * int easter_date(?int $year = null, int $mode = CAL_EASTER_DEFAULT)` |
|       - | 1456 | ` */` |
|     532 | 1457 | `PH7_PRIVATE int PH7_builtin_easter_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1458 | `{` |
|     533 | 1459 | `	return CalEaster(pCtx,nArg,apArg,1);` |
|       1 | 1460 | `}` |
|       - | 1461 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|       - | 1462 |  |
