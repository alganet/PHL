# src/ph7/builtin_date_parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4964/5234 lines (94.84%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <stdio.h>   /* snprintf: the digit engine for php's own %g rendering */` |
|      - |    8 | `/*` |
|      - |    9 | ` * The DateTime family: proleptic-Gregorian date math, the date/time` |
|      - |   10 | ` * string parser, the __dt_* host thunks, the embedded zDateTimeLib PHP` |
|      - |   11 | ` * chunk and PH7_VmInstallDateTime. The classic procedural date functions` |
|      - |   12 | ` * (date/gmdate/mktime/...) stay in builtin_date.c.` |
|      - |   13 | ` */` |
|      - |   14 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |   15 | `#include <time.h>` |
|      - |   16 | `/* ===========================================================================` |
|      - |   17 | ` * DateTime family: DateTimeInterface, DateTime,` |
|      - |   18 | ` * DateTimeImmutable, DateTimeZone (UTC + fixed offsets), date_create(),` |
|      - |   19 | ` * date_create_immutable(). Embedded-PHP chunk + C thunks, following the` |
|      - |   20 | ` * Reflection architecture (installed inside the bCompilingBuiltin window).` |
|      - |   21 | ` * Timezone SCOPE: UTC and fixed "+HH:MM" offsets only — no tz database` |
|      - |   22 | ` * (recorded the scope policy scope cut; named region zones throw like unknown zones).` |
|      - |   23 | ` * ======================================================================== */` |
|      - |   24 |  |
|      - |   25 | `/*` |
|      - |   26 | ` * Proleptic-Gregorian civil <-> day-count conversions (Howard Hinnant's` |
|      - |   27 | ` * algorithms): no time_t / libc dependence, correct far past 2038 and` |
|      - |   28 | ` * before 1970 on every platform. Day 0 == 1970-01-01.` |
|      - |   29 | ` */` |
|  29742 |   30 | `PH7_PRIVATE sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|      5 |   31 | `{` |
|      - |   32 | `	sxi64 era;` |
|      - |   33 | `	unsigned yoe,doy,doe;` |
|      - |   34 | `	/* Every step is spelled in UNSIGNED arithmetic: the year reaching here is` |
|      - |   35 | `	 * whatever the string held, php's own answer for one past the clock is the` |
|      - |   36 | `	 * WRAP below, and a signed overflow on the way to it is undefined (this` |
|      - |   37 | `	 * build gates on UBSan). The bits are the same either way. */` |
|  29747 |   38 | `	y = (sxi64)((sxu64)y - (sxu64)(m <= 2));` |
|  29747 |   39 | `	era = (y >= 0 ? y : (sxi64)((sxu64)y - 399u)) / 400;` |
|  29747 |   40 | `	yoe = (unsigned)((sxu64)y - (sxu64)era * 400u);` |
|  29747 |   41 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|  29747 |   42 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|      - |   43 | `	/* Unsigned tail: php's expanded ISO year has no width limit, and php's own` |
|      - |   44 | `	 * answer for one past the clock is a WRAP of the seconds it converts to --` |
|      - |   45 | ``	 * `new DateTime('-999999999999-01-01')` reads back as year 169108098508`` |
|      - |   46 | `	 * there. Wrapping through sxu64 reproduces that instead of overflowing a` |
|      - |   47 | `	 * signed product, which is undefined. */` |
|  29747 |   48 | `	return (sxi64)((sxu64)era * 146097u + (sxu64)doe - 719468u);` |
|      5 |   49 | `}` |
|  25424 |   50 | `PH7_PRIVATE void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|      5 |   51 | `{` |
|      - |   52 | `	sxi64 era;` |
|      - |   53 | `	unsigned doe,yoe,doy,mp;` |
|  25429 |   54 | `	z = (sxi64)((sxu64)z + 719468u);` |
|  25429 |   55 | `	era = (z >= 0 ? z : (sxi64)((sxu64)z - 146096u)) / 146097;` |
|  25429 |   56 | `	doe = (unsigned)((sxu64)z - (sxu64)era * 146097u);` |
|  25429 |   57 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|  25429 |   58 | `	*py = (sxi64)((sxu64)yoe + (sxu64)era * 400u);` |
|  25429 |   59 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|  25429 |   60 | `	mp = (5 * doy + 2) / 153;` |
|  25429 |   61 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|  25429 |   62 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|  25429 |   63 | `	if( *pm <= 2 ){` |
|   8968 |   64 | `		*py = (sxi64)((sxu64)*py + 1u);` |
|   4482 |   65 | `	}` |
|  25429 |   66 | `}` |
|  81570 |   67 | `PH7_PRIVATE sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|      5 |   68 | `{` |
|  81575 |   69 | `	sxi64 q = a / b;` |
|  81575 |   70 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|    918 |   71 | `		q--;` |
|    457 |   72 | `	}` |
|  81575 |   73 | `	return q;` |
|      5 |   74 | `}` |
|      - |   75 | `/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z).` |
|      - |   76 | ` * Shared with builtin_date.c, whose procedural doors used to reach for the` |
|      - |   77 | ` * platform's gmtime() and lose every year past an int. */` |
|   5878 |   78 | `PH7_PRIVATE void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)` |
|      5 |   79 | `{` |
|      - |   80 | `	/* Both steps are written to stay DEFINED at the ends of php's clock:` |
|      - |   81 | ``	 * `iTs + iOff` overflows for a timestamp near the int64 floor, and so does`` |
|      - |   82 | ``	 * rebuilding the day's start as `days * 86400` (UBSan caught the second at`` |
|      - |   83 | `	 * setTimestamp(PHP_INT_MIN)->format()). The remainder gives the same` |
|      - |   84 | `	 * seconds-of-day with no product at all. */` |
|   5883 |   85 | `	sxi64 t = (sxi64)((sxu64)iTs + (sxu64)iOff);` |
|   5883 |   86 | `	sxi64 days = DtFloorDiv(t,86400);` |
|   5883 |   87 | `	sxi64 secs = t % 86400;` |
|      - |   88 | `	sxi64 y;` |
|      - |   89 | `	int mo,d;` |
|   5883 |   90 | `	if( secs < 0 ){` |
|    285 |   91 | `		secs += 86400;` |
|    141 |   92 | `	}` |
|   5883 |   93 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|   5883 |   94 | `	pTm->tm_sec  = (int)(secs % 60);` |
|   5883 |   95 | `	pTm->tm_min  = (int)((secs / 60) % 60);` |
|   5883 |   96 | `	pTm->tm_hour = (int)(secs / 3600);` |
|   5883 |   97 | `	pTm->tm_mday = d;` |
|   5883 |   98 | `	pTm->tm_mon  = mo - 1;` |
|   5883 |   99 | `	pTm->tm_year = y;` |
|   5883 |  100 | `	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */` |
|   5883 |  101 | `	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));` |
|   5883 |  102 | `	pTm->tm_isdst = 0;` |
|   5883 |  103 | `	pTm->tm_zone = zZone;` |
|      - |  104 | `	/* No abbreviation and no DST unless a caller that KNOWS the zone fills them` |
|      - |  105 | `	 * in afterwards -- which only a database zone can. Every other caller gets` |
|      - |  106 | `	 * the answers this family has always given. */` |
|   5883 |  107 | `	pTm->tm_abbr = 0;` |
|   5883 |  108 | `	pTm->tm_nabbr = 0;` |
|   5883 |  109 | `	pTm->tm_gmtoff = (long)iOff;` |
|   5883 |  110 | `}` |
|   5822 |  111 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|      4 |  112 | `{` |
|      - |  113 | `	/* Unsigned throughout: a year outside the clock's own range (the parser` |
|      - |  114 | `	 * accepts php's expanded form, which has no width limit) would otherwise` |
|      - |  115 | `	 * overflow this product, which is undefined rather than merely wrong. */` |
|   5826 |  116 | `	sxu64 t = (sxu64)DtDaysFromCivil(y,mo,d) * 86400u;` |
|   5826 |  117 | `	t += (sxu64)((sxi64)h*3600 + (sxi64)mi*60 + s - iOff);` |
|   5826 |  118 | `	return (sxi64)t;` |
|      4 |  119 | `}` |
|      - |  120 | `/*` |
|      - |  121 | ` * php's date string parse is a FIELD parse. timelib fills a civil y/m/d/h/i/s/us` |
|      - |  122 | ` * vector plus a SEPARATE relative one and applies NOTHING until the whole string` |
|      - |  123 | ` * has been read, which is what makes the written ORDER of a relative string` |
|      - |  124 | `` * irrelevant there -- `+1 day +1 month` and `+1 month +1 day` are one answer --`` |
|      - |  125 | ` * and what puts the months on the day before the days move it. PHL applied every` |
|      - |  126 | `` * unit to the clock as it scanned, so `+30 days +1 month` was a day or two off.`` |
|      - |  127 | ` *` |
|      - |  128 | ` * A field the string never mentions stays DT_UNSET and is filled from the BASE` |
|      - |  129 | ` * moment afterwards (php's timelib_fill_holes), which is what lets a bare month` |
|      - |  130 | ` * name keep the base day and a bare date keep the base time of day.` |
|      - |  131 | ` */` |
|      - |  132 | `/* php's TIMELIB_UNSET, and the NUMBER matters as well as the marking: its` |
|      - |  133 | ` * normalizer carries an unset field into the one above it like any other, so` |
|      - |  134 | ` * the date a half-read clock ends up publishing is a function of this value. */` |
|      - |  135 | `#define DT_UNSET ((sxi64)-9999999)` |
|      - |  136 | `typedef struct dt_parsed dt_parsed;` |
|      - |  137 | `struct dt_parsed` |
|      - |  138 | `{` |
|      - |  139 | `	sxi64 y,m,d;                    /* absolute date fields, or DT_UNSET */` |
|      - |  140 | `	sxi64 h,i,s,us;                 /* absolute time fields, or DT_UNSET */` |
|      - |  141 | `	sxi64 ry,rm,rd,rh,ri,rs,rus;    /* the relative vector */` |
|      - |  142 | `	int bHaveDate;                  /* the string set an absolute date element */` |
|      - |  143 | `	int nTimeTok;                   /* php's have_time: 0 = the string named no time` |
|      - |  144 | `	                                 * of day, 1 = it named one, 2 = a second bare` |
|      - |  145 | `	                                 * digit run then read as a YEAR */` |
|      - |  146 | `	int bWday;                      /* the string named a weekday */` |
|      - |  147 | `	int iWday;                      /* that weekday, 0=Sunday..6 -- or NEGATIVE,` |
|      - |  148 | ``	                                 * which is what `ago` makes of it */`` |
|      - |  149 | `	int iWdayBehavior;              /* php's 0 (next/last), 1 (bare name), 2 (... this week) */` |
|      - |  150 | `	int iFirstLast;                 /* php's first_last_day_of: 0 none, 1 first, 2 last */` |
|      - |  151 | ``	int bWdayOf;                    /* php's `first monday of` special: a weekday`` |
|      - |  152 | `	                                 * hunted inside the MONTH the rest of the` |
|      - |  153 | `	                                 * string lands on, rather than from the day */` |
|      - |  154 | `	int iWdayOfNext;                /* ...and whether it starts from the month` |
|      - |  155 | ``	                                 * AFTER, which is php's `last` and `this` */`` |
|      - |  156 | ``	int bWeekdays;                  /* php's `weekday` special was named */`` |
|      - |  157 | `	sxi64 iWeekdays;                /* ... this many BUSINESS days */` |
|      - |  158 | `	sxi32 iOff;                     /* the offset in force */` |
|      - |  159 | `	int bOffSet;                    /* 0 = the string named no zone, 1 = an offset,` |
|      - |  160 | `	                                 * 2 = a NAME the string spelled (zZone below) */` |
|      - |  161 | `	const char *zZone;              /* that name, a literal: "Z", "UTC", "GMT" */` |
|      - |  162 | `	int nZone;` |
|      - |  163 | `	int bZoneIdent;                 /* php's timezone_type 3 rather than 2 -- only` |
|      - |  164 | `	                                 * "UTC" spelled in that exact case */` |
|      - |  165 | `	int nZoneTok;                   /* how many zone TOKENS the string spelled: the` |
|      - |  166 | `	                                 * second is ignored and the third refused */` |
|      - |  167 | ``	int bEpoch;                     /* the string named an `@epoch`, which is the`` |
|      - |  168 | `	                                 * one form modify() lets name a ZONE */` |
|      - |  169 | `	int bUsUnset;                   /* php's vector leaves its microseconds UNSET` |
|      - |  170 | `	                                 * where PHL writes a zero for the CLOCK's` |
|      - |  171 | `	                                 * sake: the two nocolon arms php reaches` |
|      - |  172 | `	                                 * without its HAVE_TIME ever running, a bare` |
|      - |  173 | `	                                 * four-digit clock and a bare year. It is` |
|      - |  174 | ``	                                 * what date_parse() shows as `false`. */`` |
|      - |  175 | `	int bHaveRel;                   /* php's have_relative: the string spelled a` |
|      - |  176 | `	                                 * RELATIVE element, which is what puts the` |
|      - |  177 | ``	                                 * `relative` block in date_parse()'s answer.`` |
|      - |  178 | ``	                                 * `now`, `today` and a bare `ago` do not. */`` |
|      - |  179 | `	/* Warnings a RULE raises but only the scan can publish. They ride the vector` |
|      - |  180 | `	 * so that the copy a longest-match probe runs on discards them with itself:` |
|      - |  181 | `	 * a probe that succeeds makes its caller stand down and the real rule raises` |
|      - |  182 | `	 * them again, a probe that fails raised nothing. */` |
|      - |  183 | `	int nWarnPend;` |
|      - |  184 | `	int aWarnPos[PH7_DT_MAX_WARN];` |
|      - |  185 | `	const char *azWarn[PH7_DT_MAX_WARN];` |
|      - |  186 | `};` |
|      - |  187 | `/* php's add_warning, held until the scan can publish it. */` |
|    154 |  188 | `static void DtWarnPend(dt_parsed *p,int iPos,const char *zMsg)` |
|      2 |  189 | `{` |
|    156 |  190 | `	if( p->nWarnPend < PH7_DT_MAX_WARN ){` |
|    156 |  191 | `		p->aWarnPos[p->nWarnPend] = iPos;` |
|    156 |  192 | `		p->azWarn[p->nWarnPend] = zMsg;` |
|     77 |  193 | `	}` |
|    156 |  194 | `	p->nWarnPend++;` |
|    156 |  195 | `}` |
|      - |  196 | `/* Wrapping add: a relative vector holds whatever the string spelled, and php's` |
|      - |  197 | ` * own answer past the int64 ceiling is garbage of its own -- but the OVERFLOW` |
|      - |  198 | ` * would be undefined here, and this build gates on UBSan. */` |
| 109452 |  199 | `static sxi64 DtWAdd(sxi64 a,sxi64 b)` |
|      4 |  200 | `{` |
| 109456 |  201 | `	return (sxi64)((sxu64)a + (sxu64)b);` |
|      4 |  202 | `}` |
|    452 |  203 | `static sxi64 DtWMul(sxi64 a,sxi64 b)` |
|      1 |  204 | `{` |
|    453 |  205 | `	return (sxi64)((sxu64)a * (sxu64)b);` |
|      1 |  206 | `}` |
|   6242 |  207 | `static void DtFieldsInit(dt_parsed *p,sxi32 iBaseOff)` |
|      4 |  208 | `{` |
|   6246 |  209 | `	p->y = p->m = p->d = DT_UNSET;` |
|   6246 |  210 | `	p->h = p->i = p->s = p->us = DT_UNSET;` |
|   6246 |  211 | `	p->ry = p->rm = p->rd = p->rh = p->ri = p->rs = p->rus = 0;` |
|   6246 |  212 | `	p->bHaveDate = p->nTimeTok = 0;` |
|   6246 |  213 | `	p->bWday = 0;` |
|   6246 |  214 | `	p->iWday = 0;` |
|   6246 |  215 | `	p->iWdayBehavior = 0;` |
|   6246 |  216 | `	p->iFirstLast = 0;` |
|   6246 |  217 | `	p->bWdayOf = 0;` |
|   6246 |  218 | `	p->iWdayOfNext = 0;` |
|   6246 |  219 | `	p->bWeekdays = 0;` |
|   6246 |  220 | `	p->iWeekdays = 0;` |
|   6246 |  221 | `	p->iOff = iBaseOff;` |
|   6246 |  222 | `	p->bOffSet = 0;` |
|   6246 |  223 | `	p->zZone = 0;` |
|   6246 |  224 | `	p->nZone = 0;` |
|   6246 |  225 | `	p->bZoneIdent = 0;` |
|   6246 |  226 | `	p->nZoneTok = 0;` |
|   6246 |  227 | `	p->bEpoch = 0;` |
|   6246 |  228 | `	p->nWarnPend = 0;` |
|   6246 |  229 | `	p->bHaveRel = 0;` |
|   6246 |  230 | `	p->bUsUnset = 0;` |
|   6246 |  231 | `}` |
|      - |  232 | `/* php's TIMELIB_UNHAVE_TIME: the clock is ZEROED rather than unset, and the` |
|      - |  233 | ``  * string still counts as carrying no time of its own -- which is why `tomorrow` `` |
|      - |  234 | ` * lands on midnight even through modify(), whose other fields keep the` |
|      - |  235 | ` * receiver's. */` |
|   1378 |  236 | `static void DtUnhaveTime(dt_parsed *p)` |
|      3 |  237 | `{` |
|   1381 |  238 | `	p->h = p->i = p->s = p->us = 0;` |
|   1381 |  239 | `	p->bUsUnset = 0;` |
|   1381 |  240 | `	p->nTimeTok = 0;` |
|   1381 |  241 | `}` |
|  45368 |  242 | `static void DtCarry(sxi64 *pLo,sxi64 *pHi,sxi64 iUnit)` |
|      4 |  243 | `{` |
|  45372 |  244 | `	sxi64 c = DtFloorDiv(*pLo,iUnit);` |
|  45372 |  245 | `	*pLo -= c * iUnit;` |
|  45372 |  246 | `	*pHi = DtWAdd(*pHi,c);` |
|  45372 |  247 | `}` |
|      - |  248 | `/*` |
|      - |  249 | ` * php's timelib_do_normalize: carry the clock up into the days, fold the months` |
|      - |  250 | ` * into the years, then let the civil day count absorb whatever the day field` |
|      - |  251 | ` * holds. That formula is linear in d, so an out-of-range day simply lands in the` |
|      - |  252 | `` * month after -- which is php's `2020-01-31 +1 month` == 2020-03-02.`` |
|      - |  253 | ` */` |
|  11856 |  254 | `static sxi64 DtDayCountOf(sxi64 y,sxi64 m,sxi64 d)` |
|      4 |  255 | `{` |
|  11860 |  256 | `	sxi64 c = DtFloorDiv(m - 1,12);` |
|  11860 |  257 | `	sxi64 mm = (m - 1) - c*12 + 1;` |
|  11860 |  258 | `	return DtWAdd(DtDaysFromCivil(DtWAdd(y,c),(int)mm,1),d - 1);` |
|      4 |  259 | `}` |
|  11342 |  260 | `static void DtNormalize(dt_parsed *p)` |
|      4 |  261 | `{` |
|      - |  262 | `	sxi64 days,yy;` |
|      - |  263 | `	int mm,dd;` |
|  11346 |  264 | `	DtCarry(&p->us,&p->s,1000000);` |
|  11346 |  265 | `	DtCarry(&p->s,&p->i,60);` |
|  11346 |  266 | `	DtCarry(&p->i,&p->h,60);` |
|  11346 |  267 | `	DtCarry(&p->h,&p->d,24);` |
|  11346 |  268 | `	days = DtDayCountOf(p->y,p->m,p->d);` |
|  11346 |  269 | `	DtCivilFromDays(days,&yy,&mm,&dd);` |
|  11346 |  270 | `	p->y = yy;` |
|  11346 |  271 | `	p->m = mm;` |
|  11346 |  272 | `	p->d = dd;` |
|  11346 |  273 | `}` |
|      - |  274 | `/* php's day of week, 0 = Sunday, from a day count (1970-01-01 was a Thursday). */` |
|    608 |  275 | `static int DtDowOf(sxi64 days)` |
|      1 |  276 | `{` |
|    609 |  277 | `	return (int)(((days + 4) % 7 + 7) % 7);` |
|      1 |  278 | `}` |
|      - |  279 | `/*` |
|      - |  280 | ` * php's do_adjust_for_weekday, which runs BEFORE the relative vector is applied` |
|      - |  281 | `` * -- so `+30 days next monday` moves to the Monday and then adds the days, in`` |
|      - |  282 | ``  * either written order. The three behaviours are php's own: 0 for `next`/`last` `` |
|      - |  283 | `` * (a matching base day is skipped), 1 for a bare name or `this monday` (a`` |
|      - |  284 | `` * matching base day is kept), and 2 for the `... this week` spellings, which`` |
|      - |  285 | ` * count from the WEEK rather than from the day.` |
|      - |  286 | ` */` |
|    358 |  287 | `static void DtAdjustWeekday(dt_parsed *p)` |
|      1 |  288 | `{` |
|    359 |  289 | `	sxi64 dow = DtDowOf(DtDayCountOf(p->y,p->m,p->d));` |
|    359 |  290 | `	sxi64 wd = p->iWday,diff;` |
|    359 |  291 | `	if( p->iWdayBehavior == 2 ){` |
|      - |  292 | `		/* php's two corrections: a Sunday base counts as the week's END, and a` |
|      - |  293 | `		 * Sunday target asked for from any other day is the week's end too. */` |
|     81 |  294 | `		if( dow == 0 && wd != 0 ){ wd -= 7; }` |
|     81 |  295 | `		if( wd == 0 && dow != 0 ){ wd = 7; }` |
|     81 |  296 | `		p->d = p->d - dow + wd;` |
|     81 |  297 | `		return;` |
|      - |  298 | `	}` |
|    279 |  299 | `	if( wd < 0 ){` |
|      - |  300 | ``		/* php's mirror of the hunt, which only `ago` reaches: it turns the target`` |
|      - |  301 | ``		 * weekday negative, and `next monday ago` is the Monday before. */`` |
|      9 |  302 | `		sxi64 nwd = -wd;` |
|      9 |  303 | `		p->d = DtWAdd(p->d,-(7 - (nwd - dow)));` |
|      9 |  304 | `		return;` |
|      - |  305 | `	}` |
|    271 |  306 | `	diff = wd - dow;` |
|    271 |  307 | `	if( (p->rd < 0 && diff < 0) \|\| (p->rd >= 0 && diff <= -p->iWdayBehavior) ){` |
|    147 |  308 | `		diff += 7;` |
|     73 |  309 | `	}` |
|    271 |  310 | `	p->d = DtWAdd(p->d,diff);` |
|    180 |  311 | `}` |
|      - |  312 | `/*` |
|      - |  313 | `` * php's `weekday` special: a count of BUSINESS days, which php applies before`` |
|      - |  314 | ` * everything else. Whole fives are whole weeks (the day of the week is kept),` |
|      - |  315 | ` * the remainder walks past the weekend, and a count that lands on one is pushed` |
|      - |  316 | ` * off it -- forward to Monday when the count is zero, back to Friday when a` |
|      - |  317 | ` * positive count ends there.` |
|      - |  318 | ` */` |
|    156 |  319 | `static void DtAdjustWeekdays(dt_parsed *p)` |
|      1 |  320 | `{` |
|    157 |  321 | `	sxi64 dow = DtDowOf(DtDayCountOf(p->y,p->m,p->d));` |
|    157 |  322 | `	sxi64 count = p->iWeekdays,rem;` |
|    157 |  323 | `	p->d = DtWAdd(p->d,(count / 5) * 7);` |
|    157 |  324 | `	rem = count % 5;` |
|    157 |  325 | `	if( count == 0 ){` |
|     17 |  326 | `		if( dow == 0 ){ p->d += 1; }` |
|     15 |  327 | `		else if( dow == 6 ){ p->d += 2; }` |
|     17 |  328 | `		return;` |
|      - |  329 | `	}` |
|    141 |  330 | `	if( count > 0 ){` |
|     71 |  331 | `		if( rem == 0 ){` |
|     15 |  332 | `			if( dow == 0 ){ p->d -= 2; }` |
|     13 |  333 | `			else if( dow == 6 ){ p->d -= 1; }` |
|     15 |  334 | `			return;` |
|      - |  335 | `		}` |
|     57 |  336 | `		if( dow == 6 ){ p->d += 2; rem--; dow = 1; }` |
|     51 |  337 | `		else if( dow == 0 ){ p->d += 1; rem--; dow = 1; }` |
|     57 |  338 | `		if( rem > 0 ){` |
|     53 |  339 | `			if( dow + rem > 5 ){ p->d += 2; }` |
|     53 |  340 | `			p->d += rem;` |
|     26 |  341 | `		}` |
|     57 |  342 | `		return;` |
|      - |  343 | `	}` |
|     71 |  344 | `	if( rem == 0 ){` |
|     17 |  345 | `		if( dow == 0 ){ p->d += 1; }` |
|     15 |  346 | `		else if( dow == 6 ){ p->d += 2; }` |
|     17 |  347 | `		return;` |
|      - |  348 | `	}` |
|     55 |  349 | `	if( dow == 0 ){ p->d -= 2; rem++; dow = 5; }` |
|     49 |  350 | `	else if( dow == 6 ){ p->d -= 1; rem++; dow = 5; }` |
|     55 |  351 | `	if( rem < 0 ){` |
|     51 |  352 | `		if( dow + rem < 1 ){ p->d -= 2; }` |
|     51 |  353 | `		p->d += rem;` |
|     25 |  354 | `	}` |
|     79 |  355 | `}` |
|      - |  356 | ``/* php's `first\|last day of`: the first is the day 1, the last is day 0 of the`` |
|      - |  357 | ` * month AFTER -- which the normalizer then reads back as the month's own last. */` |
|  10752 |  358 | `static void DtFirstLastDay(dt_parsed *p)` |
|      4 |  359 | `{` |
|  10756 |  360 | `	if( p->iFirstLast == 1 ){` |
|    113 |  361 | `		p->d = 1;` |
|  10700 |  362 | `	}else if( p->iFirstLast == 2 ){` |
|     57 |  363 | `		p->d = 0;` |
|     57 |  364 | `		p->m = DtWAdd(p->m,1);` |
|     28 |  365 | `	}` |
|  10756 |  366 | `}` |
|      - |  367 | `/*` |
|      - |  368 | ` * php's timelib_fill_holes + timelib_update_ts over a parsed vector: fill what` |
|      - |  369 | ` * the string left unset from the base moment, then apply in php's ORDER --` |
|      - |  370 | ``  * weekday first, the whole relative vector next, and the `first\|last day of` `` |
|      - |  371 | ` * flag LAST, which is why that flag swallows any relative DAYS beside it` |
|      - |  372 | `` * (`first day of next month +40 days` is the 1st) while the hours still count.`` |
|      - |  373 | ` *` |
|      - |  374 | ` * DT_PARSE_OVERRIDE_TIME is php's flag of the same name: modify() writes only` |
|      - |  375 | ` * the fields the string really set, so a bare date there keeps the receiver's` |
|      - |  376 | ` * time of day, where a fresh parse zeroes it.` |
|      - |  377 | ` */` |
|      - |  378 | `#define DT_PARSE_OVERRIDE_TIME 0x01` |
|      - |  379 | `/* DT_PARSE_KEEP_ZONE is modify()'s other half: php copies the FIELDS the string` |
|      - |  380 | ` * parsed into the object and nothing else, so a zone the modifier names moves` |
|      - |  381 | `` * nothing -- `$d->modify('2020-01-01T12:00:00Z')` on a +05:00 date is noon at`` |
|      - |  382 | `` * +05:00 there. The one exception is the `@epoch` form, which names an absolute`` |
|      - |  383 | ` * INSTANT (and, in php, re-zones the object to +00:00 with it). */` |
|      - |  384 | `#define DT_PARSE_KEEP_ZONE     0x02` |
|      - |  385 | `/*` |
|      - |  386 | `` * php's DAY_OF_WEEK_IN_MONTH special -- `first monday of`, `last sunday of`` |
|      - |  387 | `` * february 2020` -- and it is not a rule of its own so much as a way IN to the`` |
|      - |  388 | ` * weekday hunt the vector already carries: the MONTH is settled first (the` |
|      - |  389 | ` * relative MONTHS are applied and consumed here, though not the years, which` |
|      - |  390 | ` * ride on to the ordinary pass), the day becomes that month's 1st, and the` |
|      - |  391 | ` * ordinary hunt walks forward to the weekday from there. The count rides the` |
|      - |  392 | `` * relative DAYS -- a week per count past the first -- so `tenth tuesday of` is`` |
|      - |  393 | ` * the first one plus nine weeks, and the relative days a string spells beside` |
|      - |  394 | ` * it simply add on.` |
|      - |  395 | ` *` |
|      - |  396 | `` * `last` and `previous` are the same rule one month on with a week taken off`` |
|      - |  397 | ` * (php's own encoding: the 1st of the NEXT month, then -7 from the weekday it` |
|      - |  398 | `` * finds), and `this` is that month shift with nothing taken off, which is why it`` |
|      - |  399 | ` * answers the FIRST such weekday of the month after.` |
|      - |  400 | ` */` |
|     76 |  401 | `static void DtWeekdayOfMonth(dt_parsed *p)` |
|      1 |  402 | `{` |
|     77 |  403 | `	p->m = DtWAdd(p->m,DtWAdd(p->rm,(sxi64)p->iWdayOfNext));` |
|     77 |  404 | `	p->rm = 0;` |
|     77 |  405 | `	p->d = 1;` |
|     77 |  406 | `	DtNormalize(p);` |
|     77 |  407 | `}` |
|   5376 |  408 | `static sxi64 DtApplyFields(dt_parsed *p,sxi64 iBaseTs,sxi32 iBaseOff,int iBaseUs,` |
|      - |  409 | `	int iFlags,int *pUs)` |
|      4 |  410 | `{` |
|   5380 |  411 | `	sxi64 days = DtFloorDiv(iBaseTs + iBaseOff,86400);` |
|   5380 |  412 | `	sxi64 tod  = (iBaseTs + iBaseOff) - days*86400;` |
|      - |  413 | `	sxi64 by;` |
|      - |  414 | `	int bm,bd;` |
|   5380 |  415 | `	DtCivilFromDays(days,&by,&bm,&bd);` |
|   5380 |  416 | `	if( !(iFlags & DT_PARSE_OVERRIDE_TIME) && p->bHaveDate && !p->nTimeTok ){` |
|    541 |  417 | `		p->h = p->i = p->s = p->us = 0;` |
|    269 |  418 | `	}` |
|   5380 |  419 | `	if( p->y  == DT_UNSET ){ p->y  = by; }` |
|   5380 |  420 | `	if( p->m  == DT_UNSET ){ p->m  = bm; }` |
|   5380 |  421 | `	if( p->d  == DT_UNSET ){ p->d  = bd; }` |
|   5380 |  422 | `	if( p->h  == DT_UNSET ){ p->h  = tod / 3600; }` |
|   5380 |  423 | `	if( p->i  == DT_UNSET ){ p->i  = (tod / 60) % 60; }` |
|   5380 |  424 | `	if( p->s  == DT_UNSET ){ p->s  = tod % 60; }` |
|   5380 |  425 | `	if( p->us == DT_UNSET ){ p->us = iBaseUs; }` |
|      - |  426 | `	/* php applies the flag TWICE, and both are visible: once here, so a weekday` |
|      - |  427 | ``	 * hunt and a relative month start from the month's edge (`last monday first`` |
|      - |  428 | ``	 * day of this month` never leaves January), and once at the end, which is what`` |
|      - |  429 | `	 * makes it swallow the relative DAYS beside it. */` |
|   5380 |  430 | `	if( p->bWdayOf ){` |
|     77 |  431 | `		DtWeekdayOfMonth(p);` |
|     38 |  432 | `	}` |
|   5380 |  433 | `	DtFirstLastDay(p);` |
|   5380 |  434 | `	DtNormalize(p);` |
|   5380 |  435 | `	if( p->bWday ){` |
|    359 |  436 | `		DtAdjustWeekday(p);` |
|    359 |  437 | `		DtNormalize(p);` |
|    179 |  438 | `	}` |
|   5380 |  439 | `	p->us = DtWAdd(p->us,p->rus);` |
|   5380 |  440 | `	p->s  = DtWAdd(p->s,p->rs);` |
|   5380 |  441 | `	p->i  = DtWAdd(p->i,p->ri);` |
|   5380 |  442 | `	p->h  = DtWAdd(p->h,p->rh);` |
|   5380 |  443 | `	p->d  = DtWAdd(p->d,p->rd);` |
|   5380 |  444 | `	p->m  = DtWAdd(p->m,p->rm);` |
|   5380 |  445 | `	p->y  = DtWAdd(p->y,p->ry);` |
|   5380 |  446 | `	DtFirstLastDay(p);` |
|      - |  447 | `	/* php's business-day count runs AFTER the relative vector AND after the` |
|      - |  448 | ``	 * `first\|last day of` flag, so `last weekday -8 months` walks back from the`` |
|      - |  449 | ``	 * month it landed on and `first day of next month +9 weekdays` counts from`` |
|      - |  450 | `	 * that 1st. */` |
|   5380 |  451 | `	if( p->bWeekdays ){` |
|    157 |  452 | `		DtNormalize(p);` |
|    157 |  453 | `		DtAdjustWeekdays(p);` |
|     78 |  454 | `	}` |
|   5380 |  455 | `	DtNormalize(p);` |
|   5380 |  456 | `	*pUs = (int)p->us;` |
|   8068 |  457 | `	return DtMakeTs(p->y,(int)p->m,(int)p->d,(int)p->h,(int)p->i,(int)p->s,` |
|   5376 |  458 | `		((iFlags & DT_PARSE_KEEP_ZONE) && !p->bEpoch) ? iBaseOff : p->iOff);` |
|      4 |  459 | `}` |
|      - |  460 | `/*` |
|      - |  461 | ` * How WIDE a run of digits may be, which php bounds per grammar and PHL did not` |
|      - |  462 | ` * bound at all -- so a long run silently wrapped the int64 it was accumulated` |
|      - |  463 | `` * into (`@99999999999999999999` answered 7766279631452241919 here; UBSan called`` |
|      - |  464 | ` * the overflow what it is). Each limit is php's, measured:` |
|      - |  465 | ` *` |
|      - |  466 | `` *   an `@epoch`          18 digits, then "Number out of range"`` |
|      - |  467 | ` *   a RELATIVE number    13 digits (php's scanner answers gibberish past that --` |
|      - |  468 | ` *                        a 14-digit run comes back as ten digits' worth -- so` |
|      - |  469 | ` *                        PHL refuses instead of guessing, recorded)` |
|      - |  470 | ` *   an ISO duration      12 digits, then "Unknown or bad format"` |
|      - |  471 | ` *` |
|      - |  472 | ` * The accumulators themselves stop adding past DT_DIGITS_SAFE so that COUNTING a` |
|      - |  473 | ` * run that will be refused cannot overflow on the way.` |
|      - |  474 | ` */` |
|      - |  475 | `#define DT_DIGITS_EPOCH 18` |
|      - |  476 | `#define DT_DIGITS_REL   13` |
|      - |  477 | `#define DT_DIGITS_ISO   12` |
|      - |  478 | `#define DT_DIGITS_SAFE  18` |
|      - |  479 | `/* Whole bands below the position encoding, so one refusal cannot be mistaken for` |
|      - |  480 | ` * another: the bare negative is php's "Double time specification", a band lower` |
|      - |  481 | ` * is "Number out of range", one lower still "Double date specification", and the` |
|      - |  482 | ` * lowest "Double timezone specification". */` |
|      - |  483 | `#define DT_ERR_RANGE    1000000` |
|      - |  484 | `#define DT_ERR_DDATE    2000000` |
|      - |  485 | `#define DT_ERR_DZONE    3000000` |
|      - |  486 | `#define DT_ERR_TZID     4000000` |
|      - |  487 | `#define DT_ERR_UNEXPDATA 5000000` |
|      - |  488 | `#define DT_ERR_EMPTY    6000000` |
|      - |  489 | `/*` |
|      - |  490 | `` * php refuses a SECOND absolute date outright -- `2020-01-01 january` and`` |
|      - |  491 | `` * `20240102 20240102` are both "Double date specification" there, reported at the`` |
|      - |  492 | ` * offending token's start. Every date rule marks its answer through this; the bare` |
|      - |  493 | `` * four-digit YEAR does not, which is why `1234 5678` is a year beside a clock.`` |
|      - |  494 | ` */` |
|   3412 |  495 | `static int DtMarkDate(dt_parsed *p,const char *zTok,const char *zIn)` |
|      4 |  496 | `{` |
|   3416 |  497 | `	if( p->bHaveDate ){` |
|     57 |  498 | `		return -((int)(zTok - zIn) + 1) - DT_ERR_DDATE;` |
|      - |  499 | `	}` |
|   3360 |  500 | `	p->bHaveDate = 1;` |
|   3360 |  501 | `	return 0;` |
|   1710 |  502 | `}` |
|      - |  503 | `/*` |
|      - |  504 | ` * Read a fractional-seconds part at z (which points at the '.'): up to 6 digits` |
|      - |  505 | ` * become microseconds (right-padded to 6, extra digits ignored). Advances *pz.` |
|      - |  506 | ` */` |
|    262 |  507 | `static int DtReadFraction(const char **pz,const char *zEnd)` |
|      1 |  508 | `{` |
|    263 |  509 | `	const char *z = *pz;` |
|    263 |  510 | `	int us = 0,n = 0;` |
|    263 |  511 | `	z++; /* skip '.' */` |
|   1709 |  512 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|   1447 |  513 | `		if( n < 6 ){ us = us*10 + (z[0]-'0'); n++; }` |
|   1447 |  514 | `		z++;` |
|      1 |  515 | `	}` |
|    389 |  516 | `	while( n < 6 ){ us *= 10; n++; }` |
|    263 |  517 | `	*pz = z;` |
|    263 |  518 | `	return us;` |
|      1 |  519 | `}` |
|      - |  520 | `/*` |
|      - |  521 | ` * Read one of php's time-of-day FIELDS at z: one or two digits, greedily -- the` |
|      - |  522 | ` * two-digit reading is taken when its value is in range and the one-digit reading` |
|      - |  523 | `` * otherwise, which is what makes `12:60` php's 12:06 with a stray `0` left over`` |
|      - |  524 | `` * (and the error then lands on that `0`, not on the minute). Answers the digits`` |
|      - |  525 | ` * consumed, or 0 when there is no field here.` |
|      - |  526 | ` */` |
|  10456 |  527 | `static int DtReadField(const char *z,const char *zEnd,int iMax,int *pVal)` |
|      4 |  528 | `{` |
|      - |  529 | `	int v;` |
|  10460 |  530 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|   8811 |  531 | `	if( z+1 < zEnd && SyisDigit(z[1]) ){` |
|   8021 |  532 | `		v = (z[0]-'0')*10 + (z[1]-'0');` |
|   8021 |  533 | `		if( v <= iMax ){ *pVal = v; return 2; }` |
|    138 |  534 | `	}` |
|   1068 |  535 | `	*pVal = z[0]-'0';` |
|   1068 |  536 | `	return 1;` |
|   5232 |  537 | `}` |
|      - |  538 | `/*` |
|      - |  539 | `` * php's MERIDIAN token, the twelve-hour clock's half: `am` or `pm` in any case,`` |
|      - |  540 | ` * with an optional dot after either letter, and nothing but whitespace or the` |
|      - |  541 | `` * end of the string behind it -- so `3pm.` is three in the afternoon while`` |
|      - |  542 | `` * `3pm..`, `3pm,` and `3pmx` are not a time at all.`` |
|      - |  543 | ` *` |
|      - |  544 | ` * Answers the bytes it takes (0 for anything else) and sets *pbPm.` |
|      - |  545 | ` */` |
|   2890 |  546 | `static int DtMeridian(const char *z,const char *zEnd,int *pbPm)` |
|      3 |  547 | `{` |
|      - |  548 | `	int n,c;` |
|   2893 |  549 | `	if( z >= zEnd ){` |
|   1865 |  550 | `		return 0;` |
|      - |  551 | `	}` |
|   1031 |  552 | `	c = SyToLower(z[0]);` |
|   1031 |  553 | `	if( c != 'a' && c != 'p' ){` |
|    865 |  554 | `		return 0;` |
|      - |  555 | `	}` |
|    168 |  556 | `	n = 1;` |
|    168 |  557 | `	if( &z[n] < zEnd && z[n] == '.' ){ n++; }` |
|    168 |  558 | `	if( &z[n] >= zEnd \|\| SyToLower(z[n]) != 'm' ){` |
|     22 |  559 | `		return 0;` |
|      - |  560 | `	}` |
|    148 |  561 | `	n++;` |
|    148 |  562 | `	if( &z[n] < zEnd && z[n] == '.' ){ n++; }` |
|      - |  563 | ``	/* php spells a trailing byte INTO the rule -- `meridian = [AaPp] "."? [Mm]`` |
|      - |  564 | ``	 * "."? [\000\t ]` -- and its buffer is NUL-padded, so the end of the string`` |
|      - |  565 | ``	 * satisfies it too. Nothing else does: `3pm,`, `3pm\nx` and `3pm.x` are no`` |
|      - |  566 | ``	 * meridian at all, and the `am` of `11:30am\nx` is read as a zone. */`` |
|    148 |  567 | `	if( &z[n] < zEnd && z[n] != ' ' && z[n] != '\t' && z[n] != 0 ){` |
|     44 |  568 | `		return 0;` |
|      - |  569 | `	}` |
|    105 |  570 | `	*pbPm = (c == 'p');` |
|    105 |  571 | `	return n;` |
|   1448 |  572 | `}` |
|      - |  573 | `/* The hour a twelve-hour clock means: php's noon is 12 and its midnight is 0,` |
|      - |  574 | ` * and every other hour is itself or itself plus twelve. */` |
|    100 |  575 | `static int DtHour12(int h,int bPm)` |
|      1 |  576 | `{` |
|    101 |  577 | `	if( h == 12 ){` |
|      7 |  578 | `		return bPm ? 12 : 0;` |
|      - |  579 | `	}` |
|     95 |  580 | `	return bPm ? h + 12 : h;` |
|     51 |  581 | `}` |
|      - |  582 | `/*` |
|      - |  583 | `` * php's time of day: `[t] H[H] (:\|.) M[M] [(:\|.) S[S] [.frac]]`. Either separator`` |
|      - |  584 | `` * is php's, and only the SECONDS take a fraction -- which is why `12:34.5` is`` |
|      - |  585 | ` * php's 12:34:05 and not a half second. Answers 1 when a time was read (the` |
|      - |  586 | ` * vector's clock set), 0 when there is none here, and a NEGATIVE DtParse error` |
|      - |  587 | ` * code -- the only one it can raise is php's "Double time specification".` |
|      - |  588 | ` */` |
|   5494 |  589 | `static int DtReadTimeOfDay(const char **pz,const char *zEnd,const char *zIn,dt_parsed *p)` |
|      4 |  590 | `{` |
|   5498 |  591 | `	const char *z = *pz;` |
|   5498 |  592 | `	const char *zTok = *pz;   /* the byte a refusal names: this token's first */` |
|   5498 |  593 | `	int h,mi,s = 0,n,bT = 0,bPm = 0,nMer,nMin = 0,nSec = 0,bFrac = 0;` |
|   5498 |  594 | `	sxi64 uSec = 0;` |
|   5498 |  595 | `	char cSep1 = 0,cSep2 = 0;` |
|   5498 |  596 | `	if( z < zEnd && (z[0]=='t' \|\| z[0]=='T') ){ z++; bT = 1; }` |
|   5498 |  597 | `	if( (n = DtReadField(z,zEnd,24,&h)) == 0 ){ return 0; }` |
|   3849 |  598 | `	if( z+n >= zEnd \|\| (z[n] != ':' && z[n] != '.') ){` |
|      - |  599 | ``		/* php's twelve-hour clock with no fields under it: `3pm`, `12 a.m.`.`` |
|      - |  600 | `` 		 * The hour has to be one a twelve-hour clock can name -- `0am`, `00am` `` |
|      - |  601 | ``		 * and `13pm` are refusals, not times -- and the `t` prefix belongs to`` |
|      - |  602 | ``		 * the ISO spelling alone, so `t3pm` is the hour 03 with `pm` left over. */`` |
|   1180 |  603 | `		const char *zMer = &z[n];` |
|   1180 |  604 | `		if( bT \|\| h < 1 \|\| h > 12 ){` |
|    315 |  605 | `			return 0;` |
|      - |  606 | `		}` |
|   1528 |  607 | `		while( zMer < zEnd && (zMer[0]==' ' \|\| zMer[0]=='\t') ){ zMer++; }` |
|    866 |  608 | `		if( (nMer = DtMeridian(zMer,zEnd,&bPm)) == 0 ){` |
|    792 |  609 | `			return 0;` |
|      - |  610 | `		}` |
|      - |  611 | `		/* php's TIMELIB_HAVE_TIME runs inside the ACTION, so the token is matched` |
|      - |  612 | `		 * and behind the cursor before the refusal is raised: the walk resumes` |
|      - |  613 | `		 * past it, not on it. */` |
|     75 |  614 | `		if( p->nTimeTok ){` |
|      5 |  615 | `			*pz = &zMer[nMer];` |
|      5 |  616 | `			return -((int)(zTok - zIn) + 1);` |
|      - |  617 | `		}` |
|     71 |  618 | `		p->h = DtHour12(h,bPm);` |
|     71 |  619 | `		p->i = p->s = p->us = 0;` |
|     71 |  620 | `		p->bUsUnset = 0;` |
|     71 |  621 | `		p->nTimeTok = 1;` |
|     71 |  622 | `		*pz = &zMer[nMer];` |
|     71 |  623 | `		return 1;` |
|      - |  624 | `	}` |
|   2671 |  625 | `	cSep1 = z[n];` |
|   2671 |  626 | `	z += n + 1;` |
|   2671 |  627 | `	if( (n = DtReadField(z,zEnd,59,&mi)) == 0 ){ return 0; }` |
|   2671 |  628 | `	nMin = n;` |
|   2671 |  629 | `	z += n;` |
|   2671 |  630 | `	if( z < zEnd && (z[0]==':' \|\| z[0]=='.') && z+1 < zEnd && SyisDigit(z[1]) ){` |
|   2275 |  631 | `		cSep2 = z[0];` |
|   2275 |  632 | `		n = DtReadField(&z[1],zEnd,60,&s);` |
|   2275 |  633 | `		nSec = n;` |
|   2275 |  634 | `		z += n + 1;` |
|   2275 |  635 | `		if( z < zEnd && z[0]=='.' && z+1 < zEnd && SyisDigit(z[1]) ){` |
|    263 |  636 | `			bFrac = 1;` |
|    263 |  637 | `			uSec = DtReadFraction(&z,zEnd);` |
|    131 |  638 | `		}` |
|   1136 |  639 | `	}` |
|      - |  640 | `	/* ...and the twelve-hour half of the same clock, which php spells behind the` |
|      - |  641 | ``	 * fields: `3:04pm`, `3:04:05 a.m.`. It is only a meridian when the hour is`` |
|      - |  642 | ``	 * one a twelve-hour clock names, so `13:00pm` keeps its 13 and leaves the`` |
|      - |  643 | ``	 * `pm` to the string, which then reads it as an unknown zone. */`` |
|      - |  644 | `	/* ...and php spells the LAST field of a twelve-hour clock with both its` |
|      - |  645 | `` 	 * digits: `3:04pm` and `3:4:05pm` are times where `3:4pm` and `3:04:5pm` `` |
|      - |  646 | ``	 * are not, and the `pm` those two leave behind is an unknown zone.`` |
|      - |  647 | `	 *` |
|      - |  648 | `	 * A FRACTION narrows the shape to php's one spelling of it: both separators` |
|      - |  649 | `	 * are colons, both fields carry both digits, and the meridian follows the` |
|      - |  650 | ``	 * fraction with nothing between them -- `3:04:05.5pm` is a time and`` |
|      - |  651 | ``	 * `3:04:05.5 pm` is not. */`` |
|   2668 |  652 | `	if( h >= 1 && h <= 12 && !bT` |
|   3107 |  653 | `	 && (bFrac ? (nMin == 2 && nSec == 2 && cSep1 == ':' && cSep2 == ':')` |
|   1009 |  654 | `	           : ((nSec > 0 ? nSec : nMin) == 2)) ){` |
|   2029 |  655 | `		const char *zMer = z;` |
|   2029 |  656 | `		if( !bFrac ){` |
|   2460 |  657 | `			while( zMer < zEnd && (zMer[0]==' ' \|\| zMer[0]=='\t') ){ zMer++; }` |
|    977 |  658 | `		}` |
|   2029 |  659 | `		if( (nMer = DtMeridian(zMer,zEnd,&bPm)) > 0 ){` |
|     31 |  660 | `			h = DtHour12(h,bPm);` |
|     31 |  661 | `			z = &zMer[nMer];` |
|     15 |  662 | `		}` |
|   1013 |  663 | `	}` |
|      - |  664 | `	/* php's "Double time specification". Its TIMELIB_HAVE_TIME sits in the` |
|      - |  665 | `	 * action, so the whole token -- meridian and fraction included -- has been` |
|      - |  666 | `	 * matched and the cursor is past it before the refusal is raised; the byte it` |
|      - |  667 | `	 * names is still the token's first. Nothing is written: the clock a second` |
|      - |  668 | `	 * time token would set is not php's answer either. */` |
|   2671 |  669 | `	if( p->nTimeTok ){` |
|     19 |  670 | `		*pz = z;` |
|     19 |  671 | `		return -((int)(zTok - zIn) + 1);` |
|      - |  672 | `	}` |
|      - |  673 | `	/* A time of day sets the whole clock, sub-second included: php writes the` |
|      - |  674 | `	 * microseconds of a time WITHOUT a fraction as zero. */` |
|   2653 |  675 | `	p->h = h;` |
|   2653 |  676 | `	p->i = mi;` |
|   2653 |  677 | `	p->s = s;` |
|   2653 |  678 | `	p->us = uSec;` |
|   2653 |  679 | `	p->bUsUnset = 0;` |
|   2653 |  680 | `	p->nTimeTok = 1;` |
|   2653 |  681 | `	*pz = z;` |
|   2653 |  682 | `	return 1;` |
|   2751 |  683 | `}` |
|      - |  684 | `/*` |
|      - |  685 | ` * The zone a string NAMED, recorded once: php reads a timezone token wherever it` |
|      - |  686 | `` * stands and the FIRST one wins outright, silently -- `+0200 +0300` is +02:00,`` |
|      - |  687 | `` * `UTC GMT` is UTC and `2020-01-01T12:00:00Z +0300` keeps its `Z`. Every door`` |
|      - |  688 | `` * that reads a zone (the attached ISO offset, a trailing name, `@epoch`'s UTC and`` |
|      - |  689 | ` * the standalone token below) goes through here, so the rule is one line.` |
|      - |  690 | ` *` |
|      - |  691 | ` * A THIRD one is php's refusal, though: it counts the tokens and raises "Double` |
|      - |  692 | ` * timezone specification" on the one past the ignored second, which is what makes` |
|      - |  693 | `` * `-123-03-04` -- three offsets to php's scanner, and no date at all -- an error`` |
|      - |  694 | `` * at its last `-`. Answers 1 for that, 0 otherwise.`` |
|      - |  695 | ` *` |
|      - |  696 | ` * zName NULL means a fixed OFFSET, whose name php builds from the offset itself.` |
|      - |  697 | ` */` |
|   2152 |  698 | `static int DtZoneCount(dt_parsed *p)` |
|      3 |  699 | `{` |
|   2155 |  700 | `	int n = p->nZoneTok;` |
|   2155 |  701 | `	if( n < 2 ){` |
|   2091 |  702 | `		p->nZoneTok = n + 1;` |
|   1044 |  703 | `	}` |
|   2155 |  704 | `	return n == 0 ? 0 : (n == 1 ? 1 : -1);` |
|      3 |  705 | `}` |
|      - |  706 | `/* ...and the VALUE, written only for the token the rule above accepted. */` |
|   1646 |  707 | `static void DtZoneStore(dt_parsed *p,sxi32 iOff,const char *zName,int nName,int bIdent)` |
|      3 |  708 | `{` |
|   1649 |  709 | `	p->iOff = iOff;` |
|   1649 |  710 | `	p->bOffSet = zName ? 2 : 1;` |
|   1649 |  711 | `	p->zZone = zName;` |
|   1649 |  712 | `	p->nZone = nName;` |
|   1649 |  713 | `	p->bZoneIdent = bIdent;` |
|   1649 |  714 | `}` |
|      - |  715 | ``/* Exactly two digits whose value is <= iMax -- php's `minutelz`/`secondlz`, and`` |
|      - |  716 | ` * the hour of its two-colon spelling. */` |
|    610 |  717 | `static int DtZoneLz(const char *z,const char *zEnd,int iMax)` |
|      3 |  718 | `{` |
|    738 |  719 | `	return zEnd-z >= 2 && SyisDigit(z[0]) && SyisDigit(z[1])` |
|    858 |  720 | `		&& (z[0]-'0')*10 + (z[1]-'0') <= iMax;` |
|      3 |  721 | `}` |
|      - |  722 | ``/* php's `hour24` (<= 24) and `minute` (<= 59) fields: one digit, or two when the`` |
|      - |  723 | `` * two-digit reading is in range -- so `96` is the hour 9 with a `6` left over and`` |
|      - |  724 | `` * `24` is the hour 24. Answers the digits taken. */`` |
|    588 |  725 | `static int DtZoneField(const char *z,const char *zEnd,int iMax)` |
|      3 |  726 | `{` |
|    591 |  727 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|    116 |  728 | `		return 0;` |
|      - |  729 | `	}` |
|    477 |  730 | `	if( z+1 < zEnd && SyisDigit(z[1]) && (z[0]-'0')*10 + (z[1]-'0') <= iMax ){` |
|    329 |  731 | `		return 2;` |
|      - |  732 | `	}` |
|    150 |  733 | `	return 1;` |
|    297 |  734 | `}` |
|      - |  735 | `/*` |
|      - |  736 | ` * How many bytes of digits and colons after the sign belong to php's UTC-offset` |
|      - |  737 | ` * token. php's scanner takes the LONGEST of three spellings and leaves the rest` |
|      - |  738 | `` * of the run to the string, which is why `+2460` is +02:46 with a `0` left over`` |
|      - |  739 | `` * and `+9999` is +99:00 with `99`:`` |
|      - |  740 | ` *` |
|      - |  741 | ` *   HH:MM:SS   two colons, two digits everywhere, hours <= 24 and seconds <= 60` |
|      - |  742 | ` *   HHMMSS     six digits, the same three bounds` |
|      - |  743 | ` *   H[H] [:] M[M]    the hour alone (0-99 when nothing follows it), or an hour` |
|      - |  744 | ` *                    <= 24 and a minute <= 59, the colon optional` |
|      - |  745 | ` *` |
|      - |  746 | ` * The VALUE is not read here: php computes it from the byte COUNT afterwards` |
|      - |  747 | `` * (DtZoneOffsetDigits), and the two disagree on purpose -- `+099` matches as the`` |
|      - |  748 | `` * hour `09` and the minute `9`, then counts as three digits and answers 0h99m.`` |
|      - |  749 | ` */` |
|    310 |  750 | `static int DtZoneCorrLen(const char *z,const char *zEnd)` |
|      3 |  751 | `{` |
|      - |  752 | `	int nH,nM;` |
|      - |  753 | `	const char *zm;` |
|    310 |  754 | `	if( zEnd-z >= 8 && z[2] == ':' && z[5] == ':'` |
|     11 |  755 | `	 && DtZoneLz(z,zEnd,24) && DtZoneLz(&z[3],zEnd,59) && DtZoneLz(&z[6],zEnd,60) ){` |
|      5 |  756 | `		return 8;` |
|      - |  757 | `	}` |
|    309 |  758 | `	if( DtZoneLz(z,zEnd,24) && DtZoneLz(&z[2],zEnd,59) && DtZoneLz(&z[4],zEnd,60) ){` |
|     13 |  759 | `		return 6;` |
|      - |  760 | `	}` |
|    297 |  761 | `	if( (nH = DtZoneField(z,zEnd,24)) == 0 ){` |
|    ! 0 |  762 | `		return 0;` |
|      - |  763 | `	}` |
|    297 |  764 | `	zm = &z[nH];` |
|    297 |  765 | `	if( zm < zEnd && zm[0] == ':' ){ zm++; }` |
|    297 |  766 | `	if( (nM = DtZoneField(zm,zEnd,59)) != 0 ){` |
|    183 |  767 | `		return (int)(zm - z) + nM;` |
|      - |  768 | `	}` |
|    116 |  769 | `	return nH;` |
|    158 |  770 | `}` |
|      - |  771 | `/*` |
|      - |  772 | `` * php's MILITARY zones: a single LETTER is a whole-hour offset -- `A`..`I` are`` |
|      - |  773 | `` * +1..+9, `K`..`M` +10..+12 and `N`..`Y` -1..-12, with `Z` the zero ISO 8601`` |
|      - |  774 | `` * spells and no `J` at all. php names one by its UPPERCASE letter whatever case`` |
|      - |  775 | ` * it was written in, and calls it an ABBREVIATION; no tz database is involved,` |
|      - |  776 | ` * which is why this engine can answer the whole set exactly. Answers 1 and fills` |
|      - |  777 | ` * the offset and the name (a static literal, as every stored zone name here is),` |
|      - |  778 | `` * or 0 for `J` and for anything that is not a letter.`` |
|      - |  779 | ` */` |
|    382 |  780 | `static int DtZoneMil(int c,sxi32 *piOff,const char **pzName)` |
|      2 |  781 | `{` |
|      - |  782 | `	static const char zLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";` |
|      - |  783 | `	int i;` |
|    384 |  784 | `	if( c >= 'a' && c <= 'z' ){` |
|    208 |  785 | `		c -= 'a' - 'A';` |
|    103 |  786 | `	}` |
|    384 |  787 | `	if( c < 'A' \|\| c > 'Z' \|\| c == 'J' ){` |
|     25 |  788 | `		return 0;` |
|      - |  789 | `	}` |
|    360 |  790 | `	i = c - 'A';` |
|    360 |  791 | `	*pzName = &zLetters[i];` |
|    360 |  792 | `	if( c == 'Z' ){` |
|     70 |  793 | `		*piOff = 0;` |
|    326 |  794 | `	}else if( c < 'J' ){` |
|     68 |  795 | `		*piOff = (sxi32)(i + 1) * 3600;    /* A..I: +1..+9 */` |
|    259 |  796 | `	}else if( c <= 'M' ){` |
|     28 |  797 | `		*piOff = (sxi32)i * 3600;          /* K..M: +10..+12 (the missing J shifts them) */` |
|     15 |  798 | `	}else{` |
|    200 |  799 | `		*piOff = -(sxi32)(i - 12) * 3600;  /* N..Y: -1..-12 */` |
|      - |  800 | `	}` |
|    360 |  801 | `	return 1;` |
|    193 |  802 | `}` |
|      - |  803 | `/*` |
|      - |  804 | ` * php's longest match, seen from a WORD's side. Every rule spelled in letters` |
|      - |  805 | ` * competes with the TIMEZONE token, which reads at most SIX of them` |
|      - |  806 | ` * (DtZoneShape), so a keyword wins only when it is at least as long as that` |
|      - |  807 | ` * read -- which is to say when it runs to the end of the letter run, or is six` |
|      - |  808 | ` * letters itself and ties (a tie goes to whichever rule timelib spells first,` |
|      - |  809 | `` * and the zone is its last). `nowx`, `janx` and `todayx` are unknown zones;`` |
|      - |  810 | `` * `januaryx`, `tomorrowx` and `augustx` are the word with a military zone`` |
|      - |  811 | ` * behind it.` |
|      - |  812 | ` *` |
|      - |  813 | ` * The rules that are only ever PART of a longer one -- the ordinal and` |
|      - |  814 | ` * navigation words, which need a unit or a weekday after them -- do not get` |
|      - |  815 | `` * this: `previousx month` is a zone in php, because `previous` alone is not a`` |
|      - |  816 | ` * token there at all.` |
|      - |  817 | ` */` |
|    930 |  818 | `static int DtWordEnds(const char *z,const char *zEnd,int nKw)` |
|      2 |  819 | `{` |
|    932 |  820 | `	return nKw >= 6 \|\| &z[nKw] >= zEnd \|\| !SyisAlpha((unsigned char)z[nKw]);` |
|      2 |  821 | `}` |
|      - |  822 | `/*` |
|      - |  823 | ` * php's timezone token by SHAPE. Its scanner matches the re2c rule and asks what` |
|      - |  824 | `` * the letters SPELL only afterwards, which is why `Z,tues` reports a double`` |
|      - |  825 | `` * timezone at the `tues` rather than a name the database does not have -- and`` |
|      - |  826 | ` * why an unknown word is a token that the string reads PAST rather than a byte` |
|      - |  827 | ` * it stops on. The rule is two alternatives and, as every re2c rule does, the` |
|      - |  828 | ` * longer of the two wins:` |
|      - |  829 | ` *` |
|      - |  830 | ` *   "("? [A-Za-z]{1,6} ")"?          the abbreviation -- each paren optional on` |
|      - |  831 | `` *                                    its own, so `(abc` and `abc)` both match`` |
|      - |  832 | ``  *   [A-Z][a-z]+([_/-][A-Za-z]+)+     the tz-database identifier, `Europe/Paris` `` |
|      - |  833 | ` *` |
|      - |  834 | ` * The SIX-letter cap on the first is what every other word-shaped rule competes` |
|      - |  835 | `` * against (DtWordEnds): `janx` is an unknown zone where `januaryx` is January`` |
|      - |  836 | ` * beside the military zone X.` |
|      - |  837 | ` *` |
|      - |  838 | ` * Answers the bytes the token takes and reports the letters inside it, the` |
|      - |  839 | ` * parens dropped.` |
|      - |  840 | ` */` |
|   1642 |  841 | `static int DtZoneShape(const char *z,const char *zEnd,const char **pzName,int *pnName)` |
|      3 |  842 | `{` |
|   1645 |  843 | `	int nPar = 0,nLet = 0,nBare = 0,nId = 0;` |
|   1645 |  844 | `	if( z < zEnd && z[0] == '(' ){` |
|     20 |  845 | `		nPar = 1;` |
|      9 |  846 | `	}` |
|   3653 |  847 | `	while( nLet < 6 && &z[nPar+nLet] < zEnd && SyisAlpha((unsigned char)z[nPar+nLet]) ){` |
|   2010 |  848 | `		nLet++;` |
|      2 |  849 | `	}` |
|      - |  850 | ``	/* ...except a lone `t` with a digit behind it, which php's clock reads`` |
|      - |  851 | ``	 * LONGER than any zone: `t9` is nine in the morning, not the military zone T`` |
|      - |  852 | ``	 * with a stray digit, and `12:00t9` is php's second time specification. */`` |
|   1642 |  853 | `	if( nPar == 0 && nLet == 1 && (z[0] == 't' \|\| z[0] == 'T')` |
|    136 |  854 | `	 && &z[1] < zEnd && SyisDigit(z[1]) ){` |
|    ! 0 |  855 | `		nLet = 0;` |
|    ! 0 |  856 | `	}` |
|   1645 |  857 | `	if( nLet > 0 ){` |
|    786 |  858 | `		nBare = nPar + nLet;` |
|    786 |  859 | `		if( &z[nBare] < zEnd && z[nBare] == ')' ){` |
|     16 |  860 | `			nBare++;` |
|      7 |  861 | `		}` |
|    392 |  862 | `	}` |
|   1645 |  863 | `	if( z < zEnd && z[0] >= 'A' && z[0] <= 'Z' ){` |
|    314 |  864 | `		int k = 1,nSeg = 0;` |
|    588 |  865 | `		while( &z[k] < zEnd && z[k] >= 'a' && z[k] <= 'z' ){ k++; }` |
|    314 |  866 | `		if( k > 1 ){` |
|     99 |  867 | `			for(;;){` |
|    130 |  868 | `				int j = k;` |
|    130 |  869 | `				if( &z[j] >= zEnd \|\| (z[j] != '_' && z[j] != '/' && z[j] != '-') ){` |
|     31 |  870 | `					break;` |
|      - |  871 | `				}` |
|     72 |  872 | `				j++;` |
|     72 |  873 | `				if( &z[j] >= zEnd \|\| !SyisAlpha((unsigned char)z[j]) ){` |
|    ! 0 |  874 | `					break;` |
|      - |  875 | `				}` |
|    384 |  876 | `				while( &z[j] < zEnd && SyisAlpha((unsigned char)z[j]) ){ j++; }` |
|     72 |  877 | `				k = j;` |
|     72 |  878 | `				nSeg++;` |
|      2 |  879 | `			}` |
|     60 |  880 | `			if( nSeg > 0 ){` |
|     48 |  881 | `				nId = k;` |
|     23 |  882 | `			}` |
|     29 |  883 | `		}` |
|    156 |  884 | `	}` |
|   1645 |  885 | `	if( nId > nBare ){` |
|     48 |  886 | `		*pzName = z;` |
|     48 |  887 | `		*pnName = nId;` |
|     48 |  888 | `		return nId;` |
|      - |  889 | `	}` |
|   1599 |  890 | `	if( nBare == 0 ){` |
|    861 |  891 | `		return 0;` |
|      - |  892 | `	}` |
|    740 |  893 | `	*pzName = &z[nPar];` |
|    740 |  894 | `	*pnName = nLet;` |
|    740 |  895 | `	return nBare;` |
|    824 |  896 | `}` |
|      - |  897 | `/* php's timezone_type -- 1 = a fixed UTC OFFSET, 2 = an ABBREVIATION, 3 = an` |
|      - |  898 | ` * IDENTIFIER. Both parsers answer in these, so they stand above both. */` |
|      - |  899 | `#define DT_ZONE_OFFSET 1` |
|      - |  900 | `#define DT_ZONE_ABBR   2` |
|      - |  901 | `#define DT_ZONE_ID     3` |
|      - |  902 |  |
|      - |  903 | `/*` |
|      - |  904 | ` * ---------------------------------------------------------------------------` |
|      - |  905 | ` * The tz DATABASE, seen from the date family.` |
|      - |  906 | ` *` |
|      - |  907 | ` * Every fixed spelling this engine has always understood keeps its own path:` |
|      - |  908 | `` * the ±HH:MM:SS grammar, php's military letters, `GMT`, `Z` and `UTC` are`` |
|      - |  909 | ` * decided before any of this is asked, so a build with PH7_ENABLE_TZDB off and` |
|      - |  910 | ` * one with it on answer those identically. The database can only ADD names.` |
|      - |  911 | ` *` |
|      - |  912 | ` * A zone is a DATABASE zone when its name resolves in the table AND its` |
|      - |  913 | `` * timezone_type is 3 -- the identifier kind. `UTC` is deliberately excluded`` |
|      - |  914 | ` * even though the table holds it: it is already a fixed zone here, and routing` |
|      - |  915 | ` * it through the table would move a well-tested answer for no gain.` |
|      - |  916 | ` *` |
|      - |  917 | ` * The index is not STORED anywhere. It is re-derived from the name whenever it` |
|      - |  918 | ` * is needed, because the name is the object's state and a serialized date has` |
|      - |  919 | ` * to come back the same way. The lookup is a binary search over 599 rows.` |
|      - |  920 | ` */` |
|  11948 |  921 | `static int DtTzIndex(const char *zName,int nName,int iZoneKind)` |
|      5 |  922 | `{` |
|      - |  923 | `#ifdef PH7_ENABLE_TZDB` |
|  11953 |  924 | `	if( iZoneKind != DT_ZONE_ID \|\| zName == 0 \|\| nName < 1 ){` |
|   1949 |  925 | `		return -1;` |
|      - |  926 | `	}` |
|  10007 |  927 | `	if( nName == 3 && SyStrnicmp(zName,"UTC",3) == 0 ){` |
|   8931 |  928 | `		return -1;` |
|      - |  929 | `	}` |
|   1080 |  930 | `	return PH7_TzFind(zName,nName);` |
|      - |  931 | `#else` |
|      - |  932 | `	SXUNUSED(zName);` |
|      - |  933 | `	SXUNUSED(nName);` |
|      - |  934 | `	SXUNUSED(iZoneKind);` |
|      - |  935 | `	return -1;` |
|      - |  936 | `#endif` |
|   5979 |  937 | `}` |
|      - |  938 | `/*` |
|      - |  939 | ` * The offset a zone is on at iTs. iTz is a database index or -1, and -1 means` |
|      - |  940 | ` * "the fixed offset the caller already has", which is every pre-database zone.` |
|      - |  941 | ``  * pzAbbr/pnAbbr come back 0/0 for a fixed zone -- the marker DateFormat's `T` `` |
|      - |  942 | ` * reads as "use the old rule".` |
|      - |  943 | ` */` |
|   1766 |  944 | `static sxi32 DtTzOffsetAt(int iTz,sxi32 iFixed,sxi64 iTs,int *pbDst,` |
|      - |  945 | `	const char **pzAbbr,int *pnAbbr)` |
|      5 |  946 | `{` |
|      - |  947 | `#ifdef PH7_ENABLE_TZDB` |
|   1771 |  948 | `	if( iTz >= 0 ){` |
|    894 |  949 | `		sxi32 iOff = iFixed;` |
|    894 |  950 | `		if( PH7_TzOffsetAt(iTz,iTs,&iOff,pbDst,pzAbbr,pnAbbr) ){` |
|    894 |  951 | `			return iOff;` |
|      - |  952 | `		}` |
|    ! 0 |  953 | `	}` |
|      - |  954 | `#else` |
|      - |  955 | `	SXUNUSED(iTz);` |
|      - |  956 | `	SXUNUSED(iTs);` |
|      - |  957 | `#endif` |
|    880 |  958 | `	*pbDst = 0;` |
|    880 |  959 | `	*pzAbbr = 0;` |
|    880 |  960 | `	*pnAbbr = 0;` |
|    880 |  961 | `	return iFixed;` |
|    888 |  962 | `}` |
|      - |  963 | `/* Forward: a name a date string spells is looked up in the very tables` |
|      - |  964 | ` * DateTimeZone reads, in the same order -- an abbreviation before an` |
|      - |  965 | `` * identifier, so `CET` in a string is the fixed +01:00 there too. */`` |
|      - |  966 | `static int DtZoneNamed(const char *zTz,int nTz,sxi32 *piOff,const char **pzName,` |
|      - |  967 | `	int *pnName,int *piKind);` |
|      - |  968 | `/*` |
|      - |  969 | `` * ...and what the letters spell: `UTC` (an IDENTIFIER in that exact case, an`` |
|      - |  970 | `` * abbreviation in any other), `GMT`, the military letters above, and then`` |
|      - |  971 | ` * whatever the tz database has. Answers 1 when the name resolved, 0 for a shape` |
|      - |  972 | ` * php would look up and this build cannot -- which, with PH7_ENABLE_TZDB off,` |
|      - |  973 | ` * is every name past the three fixed spellings.` |
|      - |  974 | ` */` |
|    866 |  975 | `static int DtZoneName(const char *z,int n,sxi32 *piOff,const char **pzName,` |
|      - |  976 | `	int *pnName,int *pbIdent)` |
|      2 |  977 | `{` |
|    868 |  978 | `	int iKind = 0;` |
|    868 |  979 | `	if( n == 3 && (SyStrnicmp(z,"utc",3) == 0 \|\| SyStrnicmp(z,"gmt",3) == 0) ){` |
|    183 |  980 | `		int bUtc = (z[0] == 'u' \|\| z[0] == 'U');` |
|    183 |  981 | `		*piOff = 0;` |
|    183 |  982 | `		*pzName = bUtc ? "UTC" : "GMT";` |
|    183 |  983 | `		*pnName = 3;` |
|    183 |  984 | `		*pbIdent = (bUtc && SyMemcmp(z,"UTC",3) == 0);` |
|    183 |  985 | `		return 1;` |
|      - |  986 | `	}` |
|    686 |  987 | `	if( n == 1 && DtZoneMil(z[0],piOff,pzName) ){` |
|    268 |  988 | `		*pnName = 1;` |
|    268 |  989 | `		*pbIdent = 0;` |
|    268 |  990 | `		return 1;` |
|      - |  991 | `	}` |
|      - |  992 | `	/* An IDENTIFIER carries no offset of its own -- the instant the rest of the` |
|      - |  993 | `	 * string names picks one, and DtParseEx re-solves the reading against the` |
|      - |  994 | `	 * zone once it has the whole vector. Zero is the placeholder until then. */` |
|    420 |  995 | `	if( DtZoneNamed(z,n,piOff,pzName,pnName,&iKind) == 0 ){` |
|     61 |  996 | `		*pbIdent = (iKind == DT_ZONE_ID);` |
|     61 |  997 | `		return 1;` |
|      - |  998 | `	}` |
|    360 |  999 | `	return 0;` |
|    435 | 1000 | `}` |
|      - | 1001 | `/* Forward: the offset's VALUE is the one DateTimeZone reads too (the door that` |
|      - | 1002 | ` * takes a whole string rather than a token), so both spellings share it. */` |
|      - | 1003 | `static int DtZoneOffsetDigits(const char *z,int n,sxi32 *piOff,int *pnUsed);` |
|      - | 1004 | `/*` |
|      - | 1005 | ` * A SIGNED offset at z, the whole token: the sign, then the digits and colons` |
|      - | 1006 | ` * DtZoneCorrLen claims. Answers the bytes taken (0 when this is not one).` |
|      - | 1007 | ` */` |
|   3136 | 1008 | `static int DtZoneCorr(const char *z,const char *zEnd,sxi32 *piOff)` |
|      3 | 1009 | `{` |
|   3139 | 1010 | `	int n,nUsed = 0;` |
|   3139 | 1011 | `	if( zEnd-z < 2 \|\| (z[0] != '+' && z[0] != '-') ){` |
|   2829 | 1012 | `		return 0;` |
|      - | 1013 | `	}` |
|    310 | 1014 | `	if( (n = DtZoneCorrLen(&z[1],zEnd)) == 0` |
|    313 | 1015 | `	 \|\| DtZoneOffsetDigits(&z[1],n,piOff,&nUsed) != 0 ){` |
|    ! 0 | 1016 | `		return 0;` |
|      - | 1017 | `	}` |
|    313 | 1018 | `	if( z[0] == '-' ){` |
|    135 | 1019 | `		*piOff = -*piOff;` |
|     67 | 1020 | `	}` |
|    313 | 1021 | `	return n + 1;` |
|   1571 | 1022 | `}` |
|      - | 1023 | `/*` |
|      - | 1024 | ` * php's standalone TIMEZONE token, which its scanner takes anywhere in a date` |
|      - | 1025 | `` * string: a name, a name inside PARENTHESES (`2020-01-01 (UTC)`), or a UTC`` |
|      - | 1026 | `` * offset with an optional uppercase `GMT` in front of it (`GMT+02:00`; the`` |
|      - | 1027 | `` * lowercase spelling is the ABBREVIATION `gmt` with a relative number after it).`` |
|      - | 1028 | ` * Advances *pz over what it took and answers 1, answers 0 leaving *pz alone, or` |
|      - | 1029 | ` * answers php's "Double timezone specification" in DtParse's own convention.` |
|      - | 1030 | ` */` |
|   1650 | 1031 | `static int DtZoneTok(const char **pz,const char *zEnd,dt_parsed *p,const char *zIn)` |
|      3 | 1032 | `{` |
|   1653 | 1033 | `	const char *z = *pz;` |
|   1653 | 1034 | `	const char *zName = 0;` |
|   1653 | 1035 | `	int nName = 0,bIdent = 0,n = 0,nTok = 0,bKnown = 0,rc;` |
|   1653 | 1036 | `	sxi32 iOff = 0;` |
|      - | 1037 | ``	/* The `GMT` in front of an offset is read before the NAME of the same three`` |
|      - | 1038 | `	 * bytes, because php's scanner takes the longer token -- and only when a whole` |
|      - | 1039 | ``	 * offset follows it, which is what makes `GMT+02:00` the offset, `gmt+2` the`` |
|      - | 1040 | ``	 * zone GMT with a stray relative number after it, and `GMT+` the zone GMT with`` |
|      - | 1041 | `	 * a refusal ON the sign. */` |
|   1650 | 1042 | `	if( zEnd-z > 3 && SyMemcmp(z,"GMT",3) == 0` |
|    339 | 1043 | `	 && (n = DtZoneCorr(&z[3],zEnd,&iOff)) != 0 ){` |
|      9 | 1044 | `		zName = 0;` |
|      9 | 1045 | `		nTok = n + 3;` |
|      9 | 1046 | `		bKnown = 1;` |
|      5 | 1047 | `	}` |
|   1645 | 1048 | `	else if( (n = DtZoneShape(z,zEnd,&zName,&nName)) != 0 ){` |
|    786 | 1049 | `		nTok = n;` |
|    786 | 1050 | `		bKnown = DtZoneName(zName,nName,&iOff,&zName,&nName,&bIdent);` |
|    394 | 1051 | `	}` |
|    861 | 1052 | `	else if( (n = DtZoneCorr(z,zEnd,&iOff)) != 0 ){` |
|    287 | 1053 | `		zName = 0;` |
|    287 | 1054 | `		nTok = n;` |
|    287 | 1055 | `		bKnown = 1;` |
|    142 | 1056 | `	}` |
|   1653 | 1057 | `	if( nTok == 0 ){` |
|    576 | 1058 | `		return 0;` |
|      - | 1059 | `	}` |
|      - | 1060 | `	/* php's TIMELIB_HAVE_TZ runs BEFORE the lookup, so only the string's FIRST` |
|      - | 1061 | `	 * zone token is ever asked what it spells: a second is dropped whatever it` |
|      - | 1062 | `	 * says, a third is the refusal, and neither is reported as a name the` |
|      - | 1063 | `	 * database does not have. The token is consumed either way. */` |
|   1079 | 1064 | `	*pz = &z[nTok];` |
|   1079 | 1065 | `	rc = DtZoneCount(p);` |
|   1079 | 1066 | `	if( rc < 0 ){` |
|     66 | 1067 | `		return -((int)(z - zIn) + 1) - DT_ERR_DZONE;` |
|      - | 1068 | `	}` |
|   1015 | 1069 | `	if( rc > 0 ){` |
|      - | 1070 | `		/* php's TIMELIB_HAVE_TZ warns on the SECOND and refuses only the third */` |
|    144 | 1071 | `		DtWarnPend(p,(int)(z - zIn),"Double timezone specification");` |
|     71 | 1072 | `	}` |
|   1015 | 1073 | `	if( rc == 0 ){` |
|    873 | 1074 | `		if( !bKnown ){` |
|    290 | 1075 | `			return -((int)(z - zIn) + 1) - DT_ERR_TZID;` |
|      - | 1076 | `		}` |
|    585 | 1077 | `		DtZoneStore(p,iOff,zName,nName,bIdent);` |
|    291 | 1078 | `	}` |
|    727 | 1079 | `	return 1;` |
|    828 | 1080 | `}` |
|      - | 1081 | `/* Forward: the time SUFFIX has to know whether a DATE would read longer at the` |
|      - | 1082 | ` * same position, and the date rules read a time suffix of their own. */` |
|      - | 1083 | `static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1084 | `	dt_parsed *p,const char *zIn);` |
|      - | 1085 | ``/* True if php's `hour24 [:.] minute` reads here -- the head of every clock its`` |
|      - | 1086 | ` * combined date-and-time rules end in, and the reason a month, a day and a clock` |
|      - | 1087 | ` * are ONE token there: it reads longer than the YEAR the same digits would be. */` |
|     80 | 1088 | `static int DtClockFollows(const char *z,const char *zEnd)` |
|      1 | 1089 | `{` |
|      - | 1090 | `	int h,i,n;` |
|     81 | 1091 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|     31 | 1092 | `		return 0;` |
|      - | 1093 | `	}` |
|     51 | 1094 | `	h = z[0] - '0';` |
|     51 | 1095 | `	n = 1;` |
|     51 | 1096 | `	if( &z[1] < zEnd && SyisDigit(z[1]) && (z[0]-'0')*10 + (z[1]-'0') <= 24 ){` |
|     43 | 1097 | `		h = (z[0]-'0')*10 + (z[1]-'0');` |
|     43 | 1098 | `		n = 2;` |
|     21 | 1099 | `	}` |
|     50 | 1100 | `	if( h > 24 \|\| &z[n] >= zEnd \|\| (z[n] != ':' && z[n] != '.') \|\| &z[n+1] >= zEnd` |
|      4 | 1101 | `	 \|\| !SyisDigit(z[n+1]) ){` |
|     49 | 1102 | `		return 0;` |
|      - | 1103 | `	}` |
|      5 | 1104 | `	i = z[n+1] - '0';` |
|      5 | 1105 | `	if( &z[n+2] < zEnd && SyisDigit(z[n+2]) ){` |
|      5 | 1106 | `		i = i*10 + (z[n+2]-'0');` |
|      2 | 1107 | `	}` |
|      5 | 1108 | `	return i <= 59;` |
|     40 | 1109 | `}` |
|      - | 1110 | `/* True if z points at a two-letter English ordinal suffix (st/nd/rd/th). */` |
|   8426 | 1111 | `static int DtIsOrdinal(const char *z,const char *zEnd)` |
|      4 | 1112 | `{` |
|   8430 | 1113 | `	if( zEnd - z < 2 ){ return 0; }` |
|  14721 | 1114 | `	return SyStrnicmp(z,"st",2) == 0 \|\| SyStrnicmp(z,"nd",2) == 0` |
|  11048 | 1115 | `		\|\| SyStrnicmp(z,"rd",2) == 0 \|\| SyStrnicmp(z,"th",2) == 0;` |
|   4217 | 1116 | `}` |
|      - | 1117 | `/*` |
|      - | 1118 | `` * Parse an OPTIONAL time-of-day suffix after a date component: a space or `T`,`` |
|      - | 1119 | `` * then php's time of day, then a `Z` or a UTC offset. On entry *pz points just`` |
|      - | 1120 | ` * past the date. Advances *pz over whatever it consumes. Returns 0 on success` |
|      - | 1121 | ` * (whether or not a time was present), or a 1-based error position into zIn` |
|      - | 1122 | ` * (negative encodes php's "Double time specification"). Shared by every` |
|      - | 1123 | ` * absolute-date branch.` |
|      - | 1124 | ` */` |
|   3198 | 1125 | `static int DtTimeSuffix(const char **pz,const char *zEnd,const char *zIn,dt_parsed *p)` |
|      4 | 1126 | `{` |
|   3202 | 1127 | `	const char *z = *pz;` |
|   3202 | 1128 | `	if( z < zEnd && (z[0]=='T' \|\| z[0]==' ' \|\| z[0]=='.') && z+1 < zEnd && SyisDigit(z[1]) ){` |
|   2363 | 1129 | `		const char *zTime = &z[1];` |
|      - | 1130 | `		int rc;` |
|      - | 1131 | `		{` |
|      - | 1132 | `			/* php reads whichever token is LONGER at this position, and a dotted` |
|      - | 1133 | `			 * DATE is longer than the clock hiding in its head: the tail of` |
|      - | 1134 | ``			 * `01/02/2020 03.04.2021` is a second date (its refusal), not 03:04:20.`` |
|      - | 1135 | `			 * The probe runs on a copy, and with the date flag cleared so that the` |
|      - | 1136 | `			 * refusal this call would raise cannot answer the question. */` |
|   2363 | 1137 | `			dt_parsed sTry = *p;` |
|   2363 | 1138 | `			const char *zProbe = zTime;` |
|   2363 | 1139 | `			sTry.bHaveDate = 0;` |
|   2363 | 1140 | `			if( DtTryNumericDate(zTime,zEnd,&zProbe,&sTry,zIn) == 1 ){` |
|      9 | 1141 | `				*pz = z;` |
|      9 | 1142 | `				return 0;` |
|      - | 1143 | `			}` |
|      - | 1144 | `		}` |
|   2355 | 1145 | `		rc = DtReadTimeOfDay(&zTime,zEnd,zIn,p);` |
|   2355 | 1146 | `		if( rc < 0 ){ *pz = zTime; return rc; }` |
|   2355 | 1147 | `		if( rc == 0 ){` |
|     31 | 1148 | `			*pz = z;` |
|     31 | 1149 | `			return 0;` |
|      - | 1150 | `		}` |
|   2325 | 1151 | `		z = zTime;` |
|      - | 1152 | ``		/* The zone ATTACHED to the time is php's `iso8601normtz`: a `Z` or a`` |
|      - | 1153 | ``		 * numeric offset, whose seconds `...T12:00:00+02:00:30` reads too. A`` |
|      - | 1154 | `		 * NAME is not part of this token -- it is one of the string's own, which` |
|      - | 1155 | ``		 * is what leaves the `this` of `24:00:00this week` to the relative rule`` |
|      - | 1156 | `		 * that reads it longer. */` |
|   2322 | 1157 | `		if( z < zEnd && (z[0] == 'Z' \|\| z[0] == 'z')` |
|    179 | 1158 | `		 && !(z+1 < zEnd && SyisAlpha((unsigned char)z[1])) ){` |
|     59 | 1159 | `			if( DtZoneCount(p) < 0 ){` |
|    ! 0 | 1160 | `				*pz = &z[1];` |
|    ! 0 | 1161 | `				return -((int)(z - zIn) + 1) - DT_ERR_DZONE;` |
|      - | 1162 | `			}` |
|     59 | 1163 | `			if( p->nZoneTok == 1 ){` |
|     59 | 1164 | `				DtZoneStore(p,0,"Z",1,0);` |
|     30 | 1165 | `			}else{` |
|    ! 0 | 1166 | `				DtWarnPend(p,(int)(z - zIn),"Double timezone specification");` |
|      - | 1167 | `			}` |
|     59 | 1168 | `			z++;` |
|     30 | 1169 | `		}else{` |
|   2267 | 1170 | `			sxi32 iOffTz = 0;` |
|   2267 | 1171 | `			int nTz = DtZoneCorr(z,zEnd,&iOffTz);` |
|   2267 | 1172 | `			if( nTz > 0 ){` |
|     19 | 1173 | `				int rcZ = DtZoneCount(p);` |
|     19 | 1174 | `				const char *zTz = z;` |
|     19 | 1175 | `				z += nTz;` |
|     19 | 1176 | `				if( rcZ < 0 ){` |
|    ! 0 | 1177 | `					*pz = z;` |
|    ! 0 | 1178 | `					return -((int)(zTz - zIn) + 1) - DT_ERR_DZONE;` |
|      - | 1179 | `				}` |
|     19 | 1180 | `				if( rcZ == 0 ){` |
|     19 | 1181 | `					DtZoneStore(p,iOffTz,0,0,0);` |
|     10 | 1182 | `				}else{` |
|    ! 0 | 1183 | `					DtWarnPend(p,(int)(zTz - zIn),"Double timezone specification");` |
|      - | 1184 | `				}` |
|      9 | 1185 | `			}` |
|      - | 1186 | `		}` |
|   1161 | 1187 | `	}` |
|   3164 | 1188 | `	*pz = z;` |
|   3164 | 1189 | `	return 0;` |
|   1603 | 1190 | `}` |
|      - | 1191 | `/*` |
|      - | 1192 | ` * Read one or two decimal digits at z (z<zEnd guaranteed by caller for the first).` |
|      - | 1193 | ` * Returns the value; *pn = digits consumed (1 or 2).` |
|      - | 1194 | ` */` |
|   2122 | 1195 | `static int DtRead1or2(const char *z,const char *zEnd,int *pn)` |
|      2 | 1196 | `{` |
|   2124 | 1197 | `	int v = z[0]-'0';` |
|   2124 | 1198 | `	if( z+1 < zEnd && SyisDigit(z[1]) ){ v = v*10 + (z[1]-'0'); *pn = 2; }` |
|    893 | 1199 | `	else { *pn = 1; }` |
|   2124 | 1200 | `	return v;` |
|      2 | 1201 | `}` |
|      - | 1202 | `/*` |
|      - | 1203 | ` * The YEAR of php's ISO date, at the head of a string: four digits, or php's` |
|      - | 1204 | ` * EXPANDED form -- a sign in front of AT LEAST four digits, with no upper width` |
|      - | 1205 | `` * (`-1234-03-04`, `+12345-01-01`, `-123456789-01-01`). The sign is what admits`` |
|      - | 1206 | ` * the extra digits: an unsigned five-digit run is not a date to php at all, and` |
|      - | 1207 | `` * a signed run shorter than four is not one either (`-123-03-04` fails there).`` |
|      - | 1208 | ` *` |
|      - | 1209 | ` * Answers the bytes the year occupies -- the caller finds the '-' that closes it` |
|      - | 1210 | ` * at that offset -- or 0 when the head is not one. A magnitude past the int64` |
|      - | 1211 | ` * ceiling SATURATES there instead of overflowing; php's own answer past that` |
|      - | 1212 | ` * point is garbage of its own (a 20-digit year reads back as 1999 there), so` |
|      - | 1213 | ` * nothing pins that corner -- only the absence of undefined behaviour.` |
|      - | 1214 | ` */` |
|   6350 | 1215 | `static int DtTryIsoYear(const char *z,const char *zEnd,sxi64 *pY,int *pbRange)` |
|      4 | 1216 | `{` |
|      - | 1217 | `	static const sxu64 iCeil = (sxu64)0x7FFFFFFFFFFFFFFF;` |
|   9529 | 1218 | `	int nSign = (z < zEnd && (z[0] == '+' \|\| z[0] == '-')) ? 1 : 0;` |
|   6354 | 1219 | `	const char *zDig = &z[nSign];` |
|   6354 | 1220 | `	const char *zScan = zDig;` |
|   6354 | 1221 | `	sxu64 y = 0;` |
|   6354 | 1222 | `	int bOver = 0;` |
|      - | 1223 | `	/* php's own ceiling for the field: the magnitude an int64 holds, which is one` |
|      - | 1224 | `	 * larger on the negative side. */` |
|   6354 | 1225 | `	sxu64 iMax = (nSign && z[0] == '-') ? iCeil + 1 : iCeil;` |
|  24168 | 1226 | `	while( zScan < zEnd && SyisDigit(zScan[0]) ){` |
|  17818 | 1227 | `		sxu64 dig = (sxu64)(zScan[0] - '0');` |
|  17818 | 1228 | `		if( bOver \|\| y > (iMax - dig) / 10 ){` |
|     62 | 1229 | `			bOver = 1;` |
|     62 | 1230 | `			y = iMax;` |
|     32 | 1231 | `		}else{` |
|  17758 | 1232 | `			y = y * 10 + dig;` |
|      - | 1233 | `		}` |
|  17818 | 1234 | `		zScan++;` |
|      4 | 1235 | `	}` |
|   6354 | 1236 | `	if( zScan - zDig < 4 \|\| (nSign == 0 && zScan - zDig != 4) ){` |
|   3298 | 1237 | `		return 0;` |
|      - | 1238 | `	}` |
|   3060 | 1239 | `	if( zScan >= zEnd \|\| zScan[0] != '-' ){` |
|    284 | 1240 | `		return 0;` |
|      - | 1241 | `	}` |
|   2778 | 1242 | `	if( nSign && zScan - zDig > 19 ){` |
|      - | 1243 | `		/* php's EXPANDED year is at most nineteen digits; a wider run is not this` |
|      - | 1244 | `		 * token at all and the string re-reads it with whatever else fits. */` |
|     13 | 1245 | `		return 0;` |
|      - | 1246 | `	}` |
|      - | 1247 | `	/* ...and one that no int64 holds is php's own refusal -- but only once the` |
|      - | 1248 | `	 * REST of the token has matched too, so the caller is told rather than` |
|      - | 1249 | ``	 * answered: `+9296228446195592075-1-1` is no expanded date at all there and`` |
|      - | 1250 | `	 * reports the byte its re-reading trips on instead. */` |
|   2766 | 1251 | `	*pbRange = bOver;` |
|      - | 1252 | `	/* the negative bound IS the int64's own, so the sign is applied in UNSIGNED` |
|      - | 1253 | `	 * arithmetic: negating -9223372036854775808 as a signed value is undefined` |
|      - | 1254 | `	 * and this build gates on UBSan. */` |
|   2766 | 1255 | `	*pY = (nSign && z[0] == '-') ? (sxi64)((sxu64)0 - y) : (sxi64)y;` |
|   2766 | 1256 | `	return (int)(zScan - z);` |
|   3179 | 1257 | `}` |
|      - | 1258 | `/*` |
|      - | 1259 | ` * php's ISO WEEK DATE, the spelling ISO 8601 gives a week rather than a day:` |
|      - | 1260 | `` * `YYYY[-]Www` and `YYYY[-]Www[-]D`, with the year exactly four digits and`` |
|      - | 1261 | ` * unsigned, the week exactly two and inside 01..53, and the day ONE digit` |
|      - | 1262 | ` * inside 0..7. Everything outside that is not this token at all, which is why` |
|      - | 1263 | `` * `2020-W54`, `2020-W5` and `2020-w05` refuse where the loop runs out of rules`` |
|      - | 1264 | `` * rather than here -- and why `2020-W05-8` is the week alone with `-8` left`` |
|      - | 1265 | ` * standing as a zone OFFSET, which is the answer php gives it.` |
|      - | 1266 | ` *` |
|      - | 1267 | ` * php does not resolve the week to a calendar date: timelib writes the year` |
|      - | 1268 | ` * with January 1st and puts the whole distance on the RELATIVE day count, which` |
|      - | 1269 | `` * is what `date_parse('2020-W05')` shows as `day => 26`. The distance runs from`` |
|      - | 1270 | ` * that January 1st to the Monday of week 1 -- the week holding the 4th -- plus` |
|      - | 1271 | ` * a week per week and a day per day.` |
|      - | 1272 | ` *` |
|      - | 1273 | ` * Returns 0 when the text is not one (caller falls through), 1 on success, or` |
|      - | 1274 | ` * an error code in DtParse's own convention.` |
|      - | 1275 | ` */` |
|   4478 | 1276 | `static int DtTryIsoWeek(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1277 | `	dt_parsed *p,const char *zIn)` |
|      4 | 1278 | `{` |
|   4482 | 1279 | `	const char *zTok = z;` |
|   4482 | 1280 | `	sxi64 y = 0;` |
|   4482 | 1281 | `	int i,w,iDow = 1,dow1,rcT;` |
|      - | 1282 | ``	/* the shortest spelling is the compact `2020W05` */`` |
|   4482 | 1283 | `	if( zEnd - z < 7 ){` |
|    851 | 1284 | `		return 0;` |
|      - | 1285 | `	}` |
|  16620 | 1286 | `	for( i = 0 ; i < 4 ; i++ ){` |
|  13610 | 1287 | `		if( !SyisDigit(z[i]) ){` |
|    619 | 1288 | `			return 0;` |
|      - | 1289 | `		}` |
|  12992 | 1290 | `		y = y*10 + (z[i] - '0');` |
|   6498 | 1291 | `	}` |
|   3014 | 1292 | `	if( z[i] == '-' ){` |
|   2744 | 1293 | `		i++;` |
|   1370 | 1294 | `	}` |
|   3014 | 1295 | `	if( &z[i+2] >= zEnd \|\| z[i] != 'W' ){` |
|   2944 | 1296 | `		return 0;` |
|      - | 1297 | `	}` |
|     71 | 1298 | `	i++;` |
|     71 | 1299 | `	if( !SyisDigit(z[i]) \|\| !SyisDigit(z[i+1]) ){` |
|    ! 0 | 1300 | `		return 0;` |
|      - | 1301 | `	}` |
|     71 | 1302 | `	w = (z[i]-'0')*10 + (z[i+1]-'0');` |
|     71 | 1303 | `	i += 2;` |
|     71 | 1304 | `	if( w < 1 \|\| w > 53 ){` |
|      7 | 1305 | `		return 0;` |
|      - | 1306 | `	}` |
|      - | 1307 | `	{` |
|      - | 1308 | `		/* the day, with its own separator: a digit past 7 belongs to whatever` |
|      - | 1309 | `		 * follows the token, its sign included */` |
|     65 | 1310 | `		int j = i;` |
|     65 | 1311 | `		if( &z[j] < zEnd && z[j] == '-' ){` |
|     23 | 1312 | `			j++;` |
|     11 | 1313 | `		}` |
|     65 | 1314 | `		if( &z[j] < zEnd && z[j] >= '0' && z[j] <= '7' ){` |
|     21 | 1315 | `			iDow = z[j] - '0';` |
|     21 | 1316 | `			i = j + 1;` |
|     10 | 1317 | `		}` |
|      - | 1318 | `	}` |
|     65 | 1319 | `	z = &z[i];` |
|     65 | 1320 | `	*pzOut = z;` |
|     65 | 1321 | `	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }` |
|      - | 1322 | `	/* php's weekday numbering here is 0 = Sunday, and week 1 is the one whose` |
|      - | 1323 | `	 * Monday is at most three days after New Year's Day. */` |
|     63 | 1324 | `	dow1 = DtDowOf(DtDaysFromCivil(y,1,1));` |
|     63 | 1325 | `	p->y = y;` |
|     63 | 1326 | `	p->m = 1;` |
|     63 | 1327 | `	p->d = 1;` |
|      - | 1328 | ``	/* php ASSIGNS that count rather than adding to it, the way `tomorrow` and`` |
|      - | 1329 | ``	 * `yesterday` do -- so a `+1 week` written BEFORE the week date is discarded`` |
|      - | 1330 | ``	 * by it (`+1 week 2020-W05` is the week's own Monday) while one written after`` |
|      - | 1331 | `	 * moves on from it. */` |
|     63 | 1332 | `	p->bHaveRel = 1;` |
|     63 | 1333 | `	p->rd = (sxi64)(1 - (dow1 > 4 ? dow1 - 7 : dow1) + (w - 1)*7 + (iDow - 1));` |
|     63 | 1334 | `	*pzOut = z;` |
|     63 | 1335 | `	return 1;` |
|   2243 | 1336 | `}` |
|      - | 1337 | `/*` |
|      - | 1338 | ` * Try to read php's ISO date at z: [+-]YYYY-MM-DD plus an optional time suffix.` |
|      - | 1339 | ` * Returns 0 when the text is not one (caller falls through), 1 on success, or an` |
|      - | 1340 | ` * error code in DtParse's own convention.` |
|      - | 1341 | ` */` |
|   6350 | 1342 | `static int DtTryIsoDate(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1343 | `	dt_parsed *p,const char *zIn)` |
|      4 | 1344 | `{` |
|      - | 1345 | `	const char *zRest;` |
|   6354 | 1346 | `	const char *zTok = z;` |
|   6354 | 1347 | `	sxi64 y = 0;` |
|   6354 | 1348 | `	int nYr,mo = 0,d = 0,rcT,bRange = 0,bFull;` |
|   6354 | 1349 | `	if( (nYr = DtTryIsoYear(z,zEnd,&y,&bRange)) == 0 ){` |
|   3592 | 1350 | `		return 0;` |
|      - | 1351 | `	}` |
|   2766 | 1352 | `	zRest = &z[nYr];   /* the '-' that closed the year */` |
|   5432 | 1353 | `	bFull = !(zEnd-z < nYr + 6` |
|   2714 | 1354 | `	 \|\| !SyisDigit(zRest[1])\|\|!SyisDigit(zRest[2])\|\|zRest[3] != '-'` |
|   2640 | 1355 | `	 \|\|!SyisDigit(zRest[4])\|\|!SyisDigit(zRest[5]));` |
|   2766 | 1356 | `	if( bFull ){` |
|   2630 | 1357 | `		mo = (zRest[1]-'0')*10 + (zRest[2]-'0');` |
|   2630 | 1358 | `		d  = (zRest[4]-'0')*10 + (zRest[5]-'0');` |
|      - | 1359 | `		/* php spells the ranges INSIDE the pattern, so a month past 12 or a day` |
|      - | 1360 | `		 * past 31 means this spelling never matched at all -- the shorter rule` |
|      - | 1361 | `		 * behind it did, and the digits it did not take are left to the string.` |
|      - | 1362 | ``		 * `2020-13-45` is January the 1st of 2020 with a refusal on its `3`, not`` |
|      - | 1363 | `		 * a refusal and no date; the difference is invisible in what either` |
|      - | 1364 | `		 * engine THROWS and plain in what it collects, because a date already` |
|      - | 1365 | ``		 * read is what makes the next one a `Double date specification`.`` |
|      - | 1366 | `		 * ("00" lexes fine and normalizes: month 0 is December of the year` |
|      - | 1367 | `		 * before, which the field normalizer does on its own.) */` |
|   2630 | 1368 | `		if( mo > 12 \|\| d > 31 ){` |
|     37 | 1369 | `			bFull = 0;` |
|     18 | 1370 | `		}` |
|   1313 | 1371 | `	}` |
|   2766 | 1372 | `	if( !bFull ){` |
|      - | 1373 | `		/* Not the full spelling. Two SHORTER ones stand behind it, both php's and` |
|      - | 1374 | ``		 * both only after a plain four-digit year (`+12345-01` is neither): the`` |
|      - | 1375 | ``		 * YEAR-MONTH `2020-01`, whose day is the 1st, and the ISO ORDINAL`` |
|      - | 1376 | ``		 * `2020-102`, whose three digits are the day of the YEAR. At most three`` |
|      - | 1377 | `		 * digits belong to either, and whatever is left of the run is the string's` |
|      - | 1378 | ``		 * -- as is the whole token when the DAY-FIRST numeric rule (`2020-1-1`),`` |
|      - | 1379 | `		 * php's own separate one, reads longer here.` |
|      - | 1380 | `		 *` |
|      - | 1381 | `		 * Anything else hands the text on rather than refusing: the position this` |
|      - | 1382 | `		 * would report is the TOKEN's, and a token at position 0 encodes as the 1` |
|      - | 1383 | `		 * that means "matched", which spun the parse loop forever. */` |
|    173 | 1384 | `		int nd = 0;` |
|    489 | 1385 | `		while( &zRest[1+nd] < zEnd && SyisDigit(zRest[1+nd]) ){ nd++; }` |
|    173 | 1386 | `		if( nYr != 4 \|\| !SyisDigit(z[0]) \|\| nd == 0 ){` |
|     27 | 1387 | `			return 0;` |
|      - | 1388 | `		}` |
|    147 | 1389 | `		if( nd > 3 ){ nd = 3; }` |
|      - | 1390 | `		{` |
|      - | 1391 | ``			/* The DAY-FIRST rule reads `2020-1-1` whole, which is LONGER than the`` |
|      - | 1392 | `			 * year-month reading of its head -- php's scanner takes the longer` |
|      - | 1393 | `			 * token, so let it. The probe runs on a copy with the date flag` |
|      - | 1394 | `			 * cleared, so a refusal it would raise cannot answer the question. */` |
|    147 | 1395 | `			dt_parsed sTry = *p;` |
|    147 | 1396 | `			const char *zProbe = zTok;` |
|      - | 1397 | `			int rcP;` |
|    147 | 1398 | `			sTry.bHaveDate = 0;` |
|    147 | 1399 | `			rcP = DtTryNumericDate(zTok,zEnd,&zProbe,&sTry,zIn);` |
|    147 | 1400 | `			if( rcP == 1 && zProbe > &zRest[1+nd] ){` |
|     43 | 1401 | `				return 0;` |
|      - | 1402 | `			}` |
|      - | 1403 | `			/* The longer token matched and its own field check REFUSED it, which` |
|      - | 1404 | ``			 * is php's answer for the whole string -- `3854-2-40` is a day out of`` |
|      - | 1405 | ``			 * range there, not the year-month `3854-2` with `-40` behind it. */`` |
|    105 | 1406 | `			if( rcP != 0 && rcP != 1 ){` |
|    ! 0 | 1407 | `				*pzOut = zProbe;` |
|    ! 0 | 1408 | `				return rcP;` |
|      - | 1409 | `			}` |
|      - | 1410 | `		}` |
|      - | 1411 | `		/* The LONGEST reading that validates wins and the rest of the run is left` |
|      - | 1412 | `		 * to the string, which is where php's refusals for this shape really come` |
|      - | 1413 | ``		 * from: `2020-13` is the month 1 with a stray `3` after it (its "position`` |
|      - | 1414 | ``		 * 6"), and `1526-797-45` the month 7 with `97-45` left over. */`` |
|      - | 1415 | `		{` |
|    105 | 1416 | `			int doy = nd == 3 ? (zRest[1]-'0')*100 + (zRest[2]-'0')*10 + (zRest[3]-'0') : 0;` |
|    105 | 1417 | `			int mo2 = nd >= 2 ? (zRest[1]-'0')*10 + (zRest[2]-'0') : 99;` |
|    105 | 1418 | `			if( nd == 3 && doy >= 1 && doy <= 366 ){` |
|     27 | 1419 | `				mo = 1;` |
|     27 | 1420 | `				d = doy;   /* the field normalizer resolves it out of January */` |
|     92 | 1421 | `			}else if( nd >= 2 && mo2 <= 12 ){` |
|     33 | 1422 | `				nd = 2;` |
|     33 | 1423 | `				mo = mo2;` |
|     33 | 1424 | `				d = 1;` |
|     17 | 1425 | `			}else{` |
|     47 | 1426 | `				nd = 1;` |
|     47 | 1427 | `				mo = zRest[1]-'0';` |
|     47 | 1428 | `				d = 1;` |
|      - | 1429 | `			}` |
|      - | 1430 | `		}` |
|    105 | 1431 | `		z = &zRest[1+nd];` |
|    105 | 1432 | `		*pzOut = z;` |
|    105 | 1433 | `		if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }` |
|    103 | 1434 | `		p->y = y;` |
|    103 | 1435 | `		p->m = mo;` |
|    103 | 1436 | `		p->d = d;` |
|    103 | 1437 | `		if( (rcT = DtTimeSuffix(&z,zEnd,zIn,p)) != 0 ){` |
|    ! 0 | 1438 | `			*pzOut = z;` |
|    ! 0 | 1439 | `			return rcT;` |
|      - | 1440 | `		}` |
|    103 | 1441 | `		*pzOut = z;` |
|    103 | 1442 | `		return 1;` |
|      - | 1443 | `	}` |
|   2594 | 1444 | `	if( bRange ){` |
|      - | 1445 | ``		/* the whole `[+-]YYYY-MM-DD` matched and its year is past the int64 the`` |
|      - | 1446 | `		 * field is kept in: php's "Number out of range", at the sign */` |
|     11 | 1447 | `		*pzOut = &zRest[6];` |
|     11 | 1448 | `		return -((int)(zTok - zIn) + 1) - DT_ERR_RANGE;` |
|      - | 1449 | `	}` |
|   2584 | 1450 | `	z = &zRest[6];` |
|      - | 1451 | `	/* php's DAY carries an optional ordinal suffix wherever a day stands, this` |
|      - | 1452 | ``	 * spelling included: `2020-01-02nd` is the 2nd. Only behind a plain`` |
|      - | 1453 | `	 * four-digit year, though -- the EXPANDED form is a rule of its own, and` |
|      - | 1454 | ``	 * `+12345-01-02nd` leaves the `nd` to the string as an unknown zone. */`` |
|   2584 | 1455 | `	if( nYr == 4 && DtIsOrdinal(z,zEnd) ){` |
|      7 | 1456 | `		z += 2;` |
|      3 | 1457 | `	}` |
|   2584 | 1458 | `	*pzOut = z;` |
|   2584 | 1459 | `	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }` |
|   2578 | 1460 | `	p->y = y;` |
|   2578 | 1461 | `	p->m = mo;` |
|   2578 | 1462 | `	p->d = d;` |
|   2578 | 1463 | `	if( (rcT = DtTimeSuffix(&z,zEnd,zIn,p)) != 0 ){` |
|    ! 0 | 1464 | `		*pzOut = z;` |
|    ! 0 | 1465 | `		return rcT;` |
|      - | 1466 | `	}` |
|   2578 | 1467 | `	*pzOut = z;` |
|   2578 | 1468 | `	return 1;` |
|   3179 | 1469 | `}` |
|      - | 1470 | `/*` |
|      - | 1471 | `` * php's ISO ORDINAL date spelled with a FULL STOP, `YYYY.DDD` -- the same date`` |
|      - | 1472 | ``  * `2020-102` gives, and the ONLY dotted form a four-digit year takes: `2020.1` `` |
|      - | 1473 | `` * and `2020.12` are no date at all there (the four digits are a clock and the`` |
|      - | 1474 | ` * rest is refused), the run is exactly three digits inside 001..366, the sign` |
|      - | 1475 | `` * belongs to a rule of its own (`+2020.102` is not one), and a time suffix may`` |
|      - | 1476 | ` * follow.` |
|      - | 1477 | ` *` |
|      - | 1478 | ` * It matters beyond its own spelling, because php's full stop between two digit` |
|      - | 1479 | ` * runs is an ordinary SEPARATOR and this is the only rule that competes for it.` |
|      - | 1480 | ` * PHL had no such rule and stood the competition down instead -- a dot before a` |
|      - | 1481 | ` * digit was simply not a separator -- which refused every string where no dotted` |
|      - | 1482 | `` * date is there to claim it: `20240102.2020` is a date, a separator and a clock`` |
|      - | 1483 | `` * in php, and `1234.2020` is THIS date with a digit left over (php refuses on`` |
|      - | 1484 | ` * the fifth byte of the run, not on the dot).` |
|      - | 1485 | ` */` |
|   1778 | 1486 | `static int DtTryIsoOrdinalDot(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1487 | `	dt_parsed *p,const char *zIn)` |
|      2 | 1488 | `{` |
|   1780 | 1489 | `	const char *zTok = z;` |
|      - | 1490 | `	sxi64 y;` |
|      - | 1491 | `	int i,doy,rcT;` |
|   1780 | 1492 | `	if( zEnd - z < 8 ){` |
|    945 | 1493 | `		return 0;` |
|      - | 1494 | `	}` |
|   2856 | 1495 | `	for( i = 0 ; i < 4 ; i++ ){` |
|   2556 | 1496 | `		if( !SyisDigit(z[i]) ){ return 0; }` |
|   1012 | 1497 | `	}` |
|    302 | 1498 | `	if( z[4] != '.' \|\| !SyisDigit(z[5]) \|\| !SyisDigit(z[6]) \|\| !SyisDigit(z[7]) ){` |
|    270 | 1499 | `		return 0;` |
|      - | 1500 | `	}` |
|     33 | 1501 | `	doy = (z[5]-'0')*100 + (z[6]-'0')*10 + (z[7]-'0');` |
|     33 | 1502 | `	if( doy < 1 \|\| doy > 366 ){` |
|      5 | 1503 | `		return 0;` |
|      - | 1504 | `	}` |
|     29 | 1505 | `	y = (sxi64)((z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0'));` |
|     29 | 1506 | `	z += 8;` |
|     29 | 1507 | `	*pzOut = z;` |
|     29 | 1508 | `	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){` |
|    ! 0 | 1509 | `		return rcT;` |
|      - | 1510 | `	}` |
|     29 | 1511 | `	p->y = y;` |
|     29 | 1512 | `	p->m = 1;` |
|     29 | 1513 | `	p->d = doy;   /* the field normalizer resolves it out of January */` |
|     29 | 1514 | `	if( (rcT = DtTimeSuffix(&z,zEnd,zIn,p)) != 0 ){` |
|    ! 0 | 1515 | `		*pzOut = z;` |
|    ! 0 | 1516 | `		return rcT;` |
|      - | 1517 | `	}` |
|     29 | 1518 | `	*pzOut = z;` |
|     29 | 1519 | `	return 1;` |
|    891 | 1520 | `}` |
|      - | 1521 | `/*` |
|      - | 1522 | ` * Try to read a non-ISO numeric date at z: three integer components joined by ONE` |
|      - | 1523 | ` * consistent separator, plus an optional time suffix. php's field order depends on` |
|      - | 1524 | ` * the separator:` |
|      - | 1525 | ` *   '/'      -> YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY` |
|      - | 1526 | ` *   '-','.'  -> DD-MM-YYYY (day first); a 4-digit-first '.' date (YYYY.MM.DD) is` |
|      - | 1527 | ` *               NOT a php format and is rejected. (ISO YYYY-MM-DD is matched by the` |
|      - | 1528 | ` *               dedicated branch BEFORE this one, so a 4-digit-first '-' never` |
|      - | 1529 | ` *               reaches here.)` |
|      - | 1530 | ` * A 1-2 digit year maps php-style (00-69 -> 2000s, 70-99 -> 1900s). Returns 0 when` |
|      - | 1531 | ` * the text is not such a date (caller falls through), 1 on success (the vector's` |
|      - | 1532 | ` * date fields set and *pzOut advanced past the whole token), or an error code in` |
|      - | 1533 | ` * DtParse's own convention (positive 1-based position into zIn, negative = "double` |
|      - | 1534 | ` * time") when the shape matched but a component is out of range.` |
|      - | 1535 | ` */` |
|   4256 | 1536 | `static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1537 | `	dt_parsed *p,const char *zIn)` |
|      3 | 1538 | `{` |
|   4259 | 1539 | `	const char *zTok = z;` |
|      - | 1540 | `	int a,b,c,na,nb,nc;` |
|      - | 1541 | `	char sep;` |
|      - | 1542 | `	int y,mo,d;` |
|      - | 1543 | `	int rcT;` |
|   4259 | 1544 | `	int bOrd1 = 0,bOrd2 = 0,iDayField;` |
|      - | 1545 | `	/* first field: 1-4 digits */` |
|   4259 | 1546 | `	if( !SyisDigit(z[0]) ){ return 0; }` |
|   4259 | 1547 | `	a = 0; na = 0;` |
|  13105 | 1548 | `	while( z < zEnd && SyisDigit(z[0]) && na < 4 ){ a = a*10 + (z[0]-'0'); z++; na++; }` |
|      - | 1549 | `	/* php's ordinal suffix belongs to the DAY, and which FIELD that is depends on` |
|      - | 1550 | `	 * the separator and the widths -- both of them known only further down. So it` |
|      - | 1551 | `	 * is read where it may stand and judged once the mapping is: a suffix on the` |
|      - | 1552 | `` 	 * year or the month is not this token at all (`2020th-1-2` and `20-1th-2020` `` |
|      - | 1553 | `	 * are refusals in php too, since the separator behind it never matches). A` |
|      - | 1554 | `	 * day is at most TWO digits wide there, so a wider run does not carry one` |
|      - | 1555 | ``	 * either -- `020th-1-2020` is a refusal on its first byte. */`` |
|   4259 | 1556 | `	if( na <= 2 && DtIsOrdinal(z,zEnd) ){ bOrd1 = 1; z += 2; }` |
|   4259 | 1557 | `	if( z >= zEnd \|\| (z[0] != '-' && z[0] != '/' && z[0] != '.') ){ return 0; }` |
|    567 | 1558 | `	sep = z[0];` |
|    567 | 1559 | `	z++;` |
|      - | 1560 | `	/* second field: 1-2 digits */` |
|    567 | 1561 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    549 | 1562 | `	b = DtRead1or2(z,zEnd,&nb); z += nb;` |
|    549 | 1563 | `	if( DtIsOrdinal(z,zEnd) ){ bOrd2 = 1; z += 2; }` |
|    549 | 1564 | `	if( z >= zEnd \|\| z[0] != sep ){ return 0; }` |
|    331 | 1565 | `	z++;` |
|      - | 1566 | `	/* third field: 1-4 digits */` |
|    331 | 1567 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    325 | 1568 | `	c = 0; nc = 0;` |
|   1107 | 1569 | `	while( z < zEnd && SyisDigit(z[0]) && nc < 4 ){ c = c*10 + (z[0]-'0'); z++; nc++; }` |
|      - | 1570 | `	/* Which of the three the DAY is -- the separator and the widths decide, and` |
|      - | 1571 | `	 * both the field WIDTH below and php's ordinal suffix follow from it. */` |
|    325 | 1572 | `	iDayField = sep == '/' ? (na == 4 ? 3 : 2) : (sep == '.' ? 1 : (nc == 4 ? 1 : 3));` |
|      - | 1573 | `	/* map fields to Y/M/D; nyear tracks the year field's width for 2-digit mapping.` |
|      - | 1574 | `	 * '/'  : YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY.` |
|      - | 1575 | `	 * '-'/'.': a 4-digit LAST field is DD-MM-YYYY (day first); otherwise YY-MM-DD` |
|      - | 1576 | `	 *          (year first) — php's width heuristic. (A 4-digit FIRST '-' field is` |
|      - | 1577 | `	 *          ISO and never reaches here; a 4-digit-first '.' is not a php format.) */` |
|      - | 1578 | `	{` |
|      - | 1579 | `		int nyear;` |
|    325 | 1580 | `		if( sep == '/' ){` |
|     47 | 1581 | `			if( na == 4 ){ y = a; mo = b; d = c; nyear = na; }` |
|     33 | 1582 | `			else{ mo = a; d = b; y = c; nyear = nc; }` |
|    302 | 1583 | `		}else if( sep == '.' ){` |
|      - | 1584 | ``			/* php's dot date is day-first with a 2- or 4-digit YEAR (`20.03.67` is`` |
|      - | 1585 | `			 * 2067-03-20). Any other width is not a clean php format -- php itself` |
|      - | 1586 | `			 * yields garbage there -- so don't claim the match.` |
|      - | 1587 | `			 *` |
|      - | 1588 | `			 * A 2-digit year is the same BYTES as php's dotted CLOCK, and the clock` |
|      - | 1589 | `			 * is the rule its scanner declares first, so the clock wins whenever it` |
|      - | 1590 | ``			 * READS: `20.03.00` is 20:03:00 and `20.03.67`, whose seconds no clock`` |
|      - | 1591 | `			 * can hold, is the date. The probe runs on a copy with the time flag` |
|      - | 1592 | `			 * cleared, so a "Double time specification" this string would raise` |
|      - | 1593 | `			 * cannot answer the question. */` |
|    103 | 1594 | `			if( na == 4 ){ return 0; }` |
|     99 | 1595 | `			if( nc == 3 ){` |
|      - | 1596 | `				/* php takes FOUR digits or two, never three: the year of` |
|      - | 1597 | ``				 * `20.03.671` is 67 and the `1` is left to the string. */`` |
|      9 | 1598 | `				c /= 10;` |
|      9 | 1599 | `				nc = 2;` |
|      9 | 1600 | `				z--;` |
|      4 | 1601 | `			}` |
|     99 | 1602 | `			if( nc != 4 && nc != 2 ){ return 0; }` |
|     85 | 1603 | `			if( nc == 2 ){` |
|     49 | 1604 | `				dt_parsed sTry = *p;` |
|     49 | 1605 | `				const char *zProbe = zTok;` |
|     49 | 1606 | `				sTry.nTimeTok = 0;` |
|      - | 1607 | `				/* php takes the LONGER token, and a twelve-hour clock reads past` |
|      - | 1608 | `` 				 * where the date would end: `3.04.05` is a date and `3.04.05pm` `` |
|      - | 1609 | `				 * the time under it. */` |
|     49 | 1610 | `				if( DtReadTimeOfDay(&zProbe,zEnd,zIn,&sTry) == 1 && zProbe >= z ){` |
|     11 | 1611 | `					return 0;` |
|      - | 1612 | `				}` |
|     19 | 1613 | `			}` |
|     75 | 1614 | `			d = a; mo = b; y = c; nyear = nc;` |
|     38 | 1615 | `		}else{ /* '-' : a 4-digit LAST field is DD-MM-YYYY, else YY-MM-DD */` |
|    177 | 1616 | `			if( nc == 4 ){ d = a; mo = b; y = c; nyear = nc; }` |
|    147 | 1617 | `			else{ y = a; mo = b; d = c; nyear = na; }` |
|      - | 1618 | `		}` |
|    297 | 1619 | `		if( nyear <= 2 ){` |
|     63 | 1620 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|     19 | 1621 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|     31 | 1622 | `		}` |
|      - | 1623 | `	}` |
|      - | 1624 | `	/* php spells the ranges INSIDE the pattern, so a month past 12 or a day past` |
|      - | 1625 | `	 * 31 means this is not a date at all and the scanner tries its other rules --` |
|      - | 1626 | ``	 * which is what makes `9.30.359699` the time 09:30:35 and the year 9699. Month`` |
|      - | 1627 | `	 * 0 and day 0 do match, and normalize (month 0 is December of the year` |
|      - | 1628 | `	 * before). */` |
|      - | 1629 | `	/* A DAY is at most two digits wide in every one of php's spellings, where a` |
|      - | 1630 | `	 * YEAR may be four, so the third field's width depends on which of the two` |
|      - | 1631 | ``	 * the mapping made it: `2020/1/22020-01-02` is the 22nd with a second date`` |
|      - | 1632 | `	 * behind it, not a day of 2202. */` |
|    297 | 1633 | `	if( iDayField == 3 && nc > 2 ){` |
|    ! 0 | 1634 | `		int nDrop = nc - 2;` |
|    ! 0 | 1635 | `		while( nDrop-- > 0 ){ c /= 10; z--; }` |
|    ! 0 | 1636 | `		nc = 2;` |
|    ! 0 | 1637 | `		d = c;` |
|    ! 0 | 1638 | `	}` |
|      - | 1639 | `	/* php's DAY pattern is one or two digits and the two-digit reading only when` |
|      - | 1640 | `	 * it is in range, so an out-of-range pair leaves its second digit to the` |
|      - | 1641 | ``	 * string rather than sinking the rule: `3854-2-40` is the 4th with a stray`` |
|      - | 1642 | ``	 * `0` after it there (its refusal), not the year-month `3854-2`. Only the`` |
|      - | 1643 | `	 * year-first dash shape spells its day that way. */` |
|    297 | 1644 | `	if( d > 31 && sep == '-' && na == 4 && nc == 2 && d / 10 <= 31 ){` |
|     53 | 1645 | `		d /= 10;` |
|     53 | 1646 | `		z--;` |
|     26 | 1647 | `	}` |
|    297 | 1648 | `	if( mo > 12 \|\| d > 31 ){ return 0; }` |
|    261 | 1649 | `	if( (bOrd1 && iDayField != 1) \|\| (bOrd2 && iDayField != 2) ){` |
|     15 | 1650 | `		return 0;` |
|      - | 1651 | `	}` |
|    247 | 1652 | `	if( iDayField == 3 && nc <= 2 && DtIsOrdinal(z,zEnd) ){` |
|      7 | 1653 | `		z += 2;` |
|      3 | 1654 | `	}` |
|      - | 1655 | `	/* optional time-of-day suffix, then commit */` |
|    247 | 1656 | `	*pzOut = z;` |
|    247 | 1657 | `	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }` |
|      - | 1658 | `	/* php's American rule reads its year through a helper that comes back UNSET` |
|      - | 1659 | ``	 * once the ordinal has been stepped over, so `4/20th/2020` is the 20th of`` |
|      - | 1660 | ``	 * April on the BASE moment's year where `4/20/2020` is 2020's. */`` |
|    239 | 1661 | `	if( !bOrd2 ){` |
|    235 | 1662 | `		p->y = y;` |
|    117 | 1663 | `	}` |
|    239 | 1664 | `	p->m = mo;` |
|    239 | 1665 | `	p->d = d;` |
|    239 | 1666 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,p);` |
|    239 | 1667 | `	*pzOut = z;` |
|    239 | 1668 | `	if( rcT != 0 ){ return rcT; }` |
|    239 | 1669 | `	return 1;` |
|   2127 | 1670 | `}` |
|      - | 1671 | `/*` |
|      - | 1672 | ` * Match a month name at z (full name or its distinct 3-letter abbreviation, plus` |
|      - | 1673 | ` * "sept"), case-insensitively and only at a word boundary. Returns the month 1-12` |
|      - | 1674 | ` * and sets *pAdv to the bytes consumed, or 0 when no month name is present.` |
|      - | 1675 | ` */` |
|   4142 | 1676 | `static int DtMatchMonth(const char *z,const char *zEnd,int *pAdv)` |
|      2 | 1677 | `{` |
|      - | 1678 | `	static const struct { const char *z; int n; int mo; } aM[] = {` |
|      - | 1679 | `		{ "january",7,1 },{ "february",8,2 },{ "march",5,3 },{ "april",5,4 },` |
|      - | 1680 | `		{ "june",4,6 },{ "july",4,7 },{ "august",6,8 },{ "september",9,9 },` |
|      - | 1681 | `		{ "sept",4,9 },{ "october",7,10 },{ "november",8,11 },{ "december",8,12 },` |
|      - | 1682 | `		{ "may",3,5 },` |
|      - | 1683 | `		{ "jan",3,1 },{ "feb",3,2 },{ "mar",3,3 },{ "apr",3,4 },{ "jun",3,6 },` |
|      - | 1684 | `		{ "jul",3,7 },{ "aug",3,8 },{ "sep",3,9 },{ "oct",3,10 },{ "nov",3,11 },` |
|      - | 1685 | `		{ "dec",3,12 }` |
|      - | 1686 | `	};` |
|      - | 1687 | `	sxu32 i;` |
|  97376 | 1688 | `	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){` |
|  93528 | 1689 | `		int n = aM[i].n;` |
|  93526 | 1690 | `		if( zEnd - z >= n && SyStrnicmp(z,aM[i].z,(sxu32)n) == 0` |
|  26110 | 1691 | `		 && DtWordEnds(z,zEnd,n) ){` |
|    295 | 1692 | `			*pAdv = n;` |
|    295 | 1693 | `			return aM[i].mo;` |
|      - | 1694 | `		}` |
|  46618 | 1695 | `	}` |
|   3850 | 1696 | `	return 0;` |
|   2073 | 1697 | `}` |
|      - | 1698 | `/*` |
|      - | 1699 | ` * php's SEPARATOR bytes -- the run between two tokens, and it is wider than a` |
|      - | 1700 | ` * space: NUL, tab, newline, space and comma are all skipped there, which is` |
|      - | 1701 | ` * what lets a date string keep the newline of the file it was read from. The` |
|      - | 1702 | ` * full stop is one too but only in places (DtIsSepAt below).` |
|      - | 1703 | ` */` |
|  15422 | 1704 | `static int DtIsSep(int c)` |
|      4 | 1705 | `{` |
|  15426 | 1706 | `	return c == ' ' \|\| c == '\t' \|\| c == '\n' \|\| c == '\0' \|\| c == ',';` |
|      4 | 1707 | `}` |
|      - | 1708 | `/*` |
|      - | 1709 | `` * php's `space` -- the run allowed INSIDE one token, between a count and its`` |
|      - | 1710 | `` * unit, a sign and its digits, or `day` and the `of` behind it. It is narrower`` |
|      - | 1711 | `` * than the run between two tokens: `2 days` and `2\tdays` are php's, while`` |
|      - | 1712 | `` * `2,days`, `2.days` and `2\ndays` are not a relative token at all there.`` |
|      - | 1713 | ` */` |
|   6914 | 1714 | `static int DtIsSpace(int c)` |
|      4 | 1715 | `{` |
|   6918 | 1716 | `	return c == ' ' \|\| c == '\t';` |
|      4 | 1717 | `}` |
|      - | 1718 | `/*` |
|      - | 1719 | ` * ...and the full stop, which php separates with unconditionally. The rules that` |
|      - | 1720 | `` * SPELL a dot -- the day-first `1.2.2020`, the clock's second separator, the`` |
|      - | 1721 | `` * ordinal `2020.102` -- claim their own bytes before the run between tokens is`` |
|      - | 1722 | ``  * ever consulted, so nothing is lost by stepping over the rest: `20240102.2020` `` |
|      - | 1723 | ` * is a date, a separator and a clock there.` |
|      - | 1724 | ` */` |
|  21656 | 1725 | `static int DtIsSepAt(const char *z,const char *zEnd)` |
|      4 | 1726 | `{` |
|  21660 | 1727 | `	if( z >= zEnd ){` |
|   6140 | 1728 | `		return 0;` |
|      - | 1729 | `	}` |
|  15524 | 1730 | `	return z[0] == '.' \|\| DtIsSep((unsigned char)z[0]);` |
|  10832 | 1731 | `}` |
|      - | 1732 | `/*` |
|      - | 1733 | ` * ...and the wider set php tolerates at the ENDS of the string, where a` |
|      - | 1734 | ` * carriage return, a vertical tab and a form feed are allowed as well: a string` |
|      - | 1735 | `` * read from a file keeps its `\r\n` and still parses, while the same bytes`` |
|      - | 1736 | ` * BETWEEN two tokens are an unexpected character in both engines. The comma and` |
|      - | 1737 | ` * the full stop go the other way -- they separate tokens but do not close the` |
|      - | 1738 | `` * string, so `12:00,` parses and `3pm,` is not a meridian at all.`` |
|      - | 1739 | ` */` |
|   6138 | 1740 | `static int DtIsEdgeSep(int c)` |
|      4 | 1741 | `{` |
|   9209 | 1742 | `	return c == ' ' \|\| c == '\t' \|\| c == '\n' \|\| c == '\0'` |
|   9207 | 1743 | `	    \|\| c == '\r' \|\| c == '\v' \|\| c == '\f';` |
|      4 | 1744 | `}` |
|      - | 1745 | `/*` |
|      - | 1746 | ` * php's own first act on a date string is a TRIM -- timelib_strtotime walks` |
|      - | 1747 | ` * isspace() off both ends and hands its scanner what is left -- so every` |
|      - | 1748 | ` * position it reports afterwards is the TRIMMED string's, while the message` |
|      - | 1749 | `` * still prints the string the caller wrote: `new DateTime('  xyz')` blames`` |
|      - | 1750 | `` * position 0 and shows `(x)`, where PHL blamed position 2. The trim is isspace`` |
|      - | 1751 | ` * and NOTHING else, which is what keeps a leading NUL or full stop counting --` |
|      - | 1752 | ` * those are separators the scanner steps over, and stepping over one is a byte` |
|      - | 1753 | `` * gone by (`.xyz` refuses at 1).`` |
|      - | 1754 | ` */` |
|  16026 | 1755 | `static int DtIsCSpace(int c)` |
|      4 | 1756 | `{` |
|  23823 | 1757 | `	return c == ' ' \|\| c == '\t' \|\| c == '\n'` |
|  23942 | 1758 | `	    \|\| c == '\v' \|\| c == '\f' \|\| c == '\r';` |
|      4 | 1759 | `}` |
|   7866 | 1760 | `static void DtTrimEnds(const char **pz,int *pn)` |
|      4 | 1761 | `{` |
|   7870 | 1762 | `	const char *z = *pz;` |
|   7870 | 1763 | `	int n = *pn;` |
|   8096 | 1764 | `	while( n > 0 && DtIsCSpace((unsigned char)z[0]) ){ z++; n--; }` |
|   7942 | 1765 | `	while( n > 0 && DtIsCSpace((unsigned char)z[n-1]) ){ n--; }` |
|   7870 | 1766 | `	*pz = z;` |
|   7870 | 1767 | `	*pn = n;` |
|   7870 | 1768 | `}` |
|      - | 1769 | `/*` |
|      - | 1770 | ` * ...and what the SENTENCE shows of it stops at the first NUL, because php` |
|      - | 1771 | `` * hands the string to a C `%s`. A date string may well carry one -- the scanner`` |
|      - | 1772 | `` * reads a NUL as an ordinary separator, so `"15 january 2020\0),/"` is a real`` |
|      - | 1773 | ` * parse that fails at byte 16 -- and php names that byte while printing only` |
|      - | 1774 | ` * the sixteen before it.` |
|      - | 1775 | ` */` |
|   1964 | 1776 | `static int DtCStrLen(const char *z,int n)` |
|      2 | 1777 | `{` |
|   1966 | 1778 | `	int k = 0;` |
|  15132 | 1779 | `	while( k < n && z[k] != 0 ){ k++; }` |
|   1966 | 1780 | `	return k;` |
|      2 | 1781 | `}` |
|      - | 1782 | `/*` |
|      - | 1783 | ` * Match a weekday name at z (full or 3-letter, case-insensitive). Returns the` |
|      - | 1784 | ` * day-of-week 0=Sunday..6=Saturday and sets *pAdv, or -1.` |
|      - | 1785 | ` *` |
|      - | 1786 | `` * `bLoose` is php's longest-match rule seen from the other side. A name STANDING`` |
|      - | 1787 | ` * ALONE competes with the timezone-name token, which is the longer read of` |
|      - | 1788 | `` * `mons` and `tues` -- so those are an unknown zone there, not a weekday -- while`` |
|      - | 1789 | ` * a name behind a COUNT is inside one rule with it, nothing longer matches, and` |
|      - | 1790 | `` * the letters left over become a zone of their own (`3 mons` is the third Monday`` |
|      - | 1791 | ` * in the military zone S). Only the counted spellings pass it.` |
|      - | 1792 | ` */` |
|   8048 | 1793 | `static int DtMatchWeekdayEx(const char *z,const char *zEnd,int *pAdv,int bLoose)` |
|      4 | 1794 | `{` |
|      - | 1795 | `	static const struct { const char *z; int n; int dow; } aW[] = {` |
|      - | 1796 | `		{ "sunday",6,0 },{ "monday",6,1 },{ "tuesday",7,2 },{ "wednesday",9,3 },` |
|      - | 1797 | `		{ "thursday",8,4 },{ "friday",6,5 },{ "saturday",8,6 },` |
|      - | 1798 | `		{ "sun",3,0 },{ "mon",3,1 },{ "tue",3,2 },{ "wed",3,3 },{ "thu",3,4 },` |
|      - | 1799 | `		{ "fri",3,5 },{ "sat",3,6 }` |
|      - | 1800 | `	};` |
|      - | 1801 | `	sxu32 i;` |
| 116694 | 1802 | `	for( i = 0 ; i < SX_ARRAYSIZE(aW) ; ++i ){` |
| 109030 | 1803 | `		int n = aW[i].n;` |
| 109030 | 1804 | `		if( zEnd - z < n \|\| SyStrnicmp(z,aW[i].z,(sxu32)n) != 0 ){` |
| 108632 | 1805 | `			continue;` |
|      - | 1806 | `		}` |
|      - | 1807 | ``		/* php spells the FULL names with an optional plural `s` and the`` |
|      - | 1808 | ``		 * three-letter abbreviations without one, so `mondays` is a weekday where`` |
|      - | 1809 | ``		 * `mons` is `mon` with an `s` left standing -- which the string then reads`` |
|      - | 1810 | `		 * as a military zone. */` |
|    399 | 1811 | `		if( n > 3 && zEnd - z > n && (z[n] == 's' \|\| z[n] == 'S') ){` |
|     15 | 1812 | `			*pAdv = n + 1;` |
|     15 | 1813 | `			return aW[i].dow;` |
|      - | 1814 | `		}` |
|      - | 1815 | `		/* A full name is six bytes or more and php's timezone-name token stops at` |
|      - | 1816 | `		 * six, so nothing longer competes with it and letters behind it are the` |
|      - | 1817 | ``		 * next token's (`mondayx` is Monday in the military zone X). */`` |
|    385 | 1818 | `		if( bLoose \|\| n > 3 \|\| zEnd - z == n \|\| !SyisAlpha(z[n]) ){` |
|    371 | 1819 | `			*pAdv = n;` |
|    371 | 1820 | `			return aW[i].dow;` |
|      - | 1821 | `		}` |
|      8 | 1822 | `	}` |
|   7668 | 1823 | `	return -1;` |
|   4028 | 1824 | `}` |
|      - | 1825 | `/*` |
|      - | 1826 | `` * php's `americanshort`, `month "/" day` -- the American date with no year at`` |
|      - | 1827 | `` * all (`4/20`, `12/31`, `10/2`), which this engine had only in its`` |
|      - | 1828 | ` * month/day/year form, so the most common way an American program spells a date` |
|      - | 1829 | ` * without one did not parse.` |
|      - | 1830 | ` *` |
|      - | 1831 | ` * Its fields are spelled INSIDE the pattern rather than range-checked` |
|      - | 1832 | `` * afterwards, `"0"? [0-9] \| "1"[0-2]` and `[0-2]?[0-9] \| "3"[01]`, so a`` |
|      - | 1833 | ` * two-digit reading that is out of range leaves its second digit to the string` |
|      - | 1834 | `` * instead of sinking the rule: `4/32` is the 3rd with a stray `2` behind it and`` |
|      - | 1835 | `` * `13/20` an unexpected `1` and then March the 20th. Zero matches both fields`` |
|      - | 1836 | `` * and normalizes, which is what makes `0/1` December of the year before.`` |
|      - | 1837 | `` * php's ordinal suffix rides the day with NOTHING between them, so `4/20th` is`` |
|      - | 1838 | `` * the 20th where `4/20 th` is the 20th beside a zone it cannot find.`` |
|      - | 1839 | ` *` |
|      - | 1840 | ` * The YEAR is left alone -- php's action writes only the month and the day --` |
|      - | 1841 | `` * which is what keeps `4/20` on the base moment's year.`` |
|      - | 1842 | ` */` |
|   1554 | 1843 | `static int DtTryAmericanShort(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1844 | `	dt_parsed *p,const char *zIn)` |
|      2 | 1845 | `{` |
|   1556 | 1846 | `	const char *zTok = z;` |
|      - | 1847 | `	int mo,d,rc;` |
|   1556 | 1848 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|    ! 0 | 1849 | `		return 0;` |
|      - | 1850 | `	}` |
|   1556 | 1851 | `	mo = z[0] - '0';` |
|   1556 | 1852 | `	if( z+1 < zEnd && SyisDigit(z[1]) && (z[0] == '0' \|\| (z[0] == '1' && z[1] <= '2')) ){` |
|    369 | 1853 | `		mo = mo*10 + (z[1]-'0');` |
|    369 | 1854 | `		z += 2;` |
|    185 | 1855 | `	}else{` |
|   1188 | 1856 | `		z++;` |
|      - | 1857 | `	}` |
|   1556 | 1858 | `	if( z >= zEnd \|\| z[0] != '/' ){` |
|   1496 | 1859 | `		return 0;` |
|      - | 1860 | `	}` |
|     61 | 1861 | `	z++;` |
|     61 | 1862 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|      3 | 1863 | `		return 0;` |
|      - | 1864 | `	}` |
|     59 | 1865 | `	d = z[0] - '0';` |
|     58 | 1866 | `	if( z+1 < zEnd && SyisDigit(z[1])` |
|     44 | 1867 | `	 && (z[0] <= '2' \|\| (z[0] == '3' && z[1] <= '1')) ){` |
|     41 | 1868 | `		d = d*10 + (z[1]-'0');` |
|     41 | 1869 | `		z += 2;` |
|     21 | 1870 | `	}else{` |
|     19 | 1871 | `		z++;` |
|      - | 1872 | `	}` |
|     59 | 1873 | `	if( DtIsOrdinal(z,zEnd) ){` |
|     11 | 1874 | `		z += 2;` |
|      5 | 1875 | `	}` |
|     59 | 1876 | `	*pzOut = z;` |
|     59 | 1877 | `	if( (rc = DtMarkDate(p,zTok,zIn)) != 0 ){` |
|      7 | 1878 | `		return rc;` |
|      - | 1879 | `	}` |
|     53 | 1880 | `	p->m = mo;` |
|     53 | 1881 | `	p->d = d;` |
|     53 | 1882 | `	return 1;` |
|    779 | 1883 | `}` |
|      - | 1884 | `/*` |
|      - | 1885 | ` * Try to read a textual-month date at z, in either order:` |
|      - | 1886 | ` *   MonthName [Day] [Year]   ("Jan 15 2020", "January", "January 2020")` |
|      - | 1887 | ` *   Day MonthName [Year]     ("15 January 2020", "15th Jan")` |
|      - | 1888 | ` * Only what the string SPELLS is written: a missing day stays unset (so a bare` |
|      - | 1889 | ` * month name keeps the base day, php's answer) except when a year was given,` |
|      - | 1890 | ` * which is php's own "January 2020" -> the 1st. Day may carry an ordinal suffix,` |
|      - | 1891 | ` * fields may be comma-separated, month names are case-insensitive, and an` |
|      - | 1892 | ` * optional time-of-day suffix + trailing UTC/GMT is consumed. Returns 0 (not a` |
|      - | 1893 | ` * month date — caller falls through, *pzOut untouched), 1 on success, or a` |
|      - | 1894 | ` * DtParse error code (out-of-range day).` |
|      - | 1895 | ` */` |
|   2502 | 1896 | `static int DtTryMonthDate(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1897 | `	dt_parsed *p,const char *zIn)` |
|      2 | 1898 | `{` |
|   2504 | 1899 | `	const char *zTok = z;` |
|   2504 | 1900 | `	const char *zAfterMon = 0;` |
|   2504 | 1901 | `	int mo,d = 1,adv,haveDay = 0,haveYear = 0;` |
|   2504 | 1902 | `	int bMonthFirst = 0,nSuf = 0,bClock = 0;` |
|   2504 | 1903 | `	sxi64 y = 0;` |
|      - | 1904 | `	int rcT;` |
|      - | 1905 | `` /* php's textual-date rule spells its run with the full stop in it (`5.january` `` |
|      - | 1906 | `` * and `january.5.2020` are dates there) and without the comma -- except between`` |
|      - | 1907 | ` * the DAY and the YEAR, which is where the comma everyone writes goes` |
|      - | 1908 | `` * (`January 15, 2020`, and `January, 15 2020` is no date at all). */`` |
|      - | 1909 | `#define MDSKIPWS() while( z < zEnd && (DtIsSpace((unsigned char)z[0]) \|\| z[0]=='.') ){ z++; }` |
|      - | 1910 | ``/* ...and the run BEHIND the day, which is php's `[,.stndrh\t ]+` -- the ordinal`` |
|      - | 1911 | ` * suffix and the comma before a year are the same set, greedy, and it is` |
|      - | 1912 | ` * REQUIRED (or a NUL, or the end of the string) when no year follows: that is` |
|      - | 1913 | `` * what makes `january 12x` no date at all while `january 12sd2020` is one, and`` |
|      - | 1914 | `` * what leaves `january 12 sat` reading `at` as a zone. */`` |
|      - | 1915 | `#define MDISSUF(c) ((c)==','\|\|(c)=='.'\|\|(c)=='s'\|\|(c)=='t'\|\|(c)=='n'\|\|(c)=='d' \` |
|      - | 1916 | `	\|\|(c)=='r'\|\|(c)=='h'\|\|(c)=='\t'\|\|(c)==' ')` |
|   2504 | 1917 | `	if( (mo = DtMatchMonth(z,zEnd,&adv)) != 0 ){` |
|      - | 1918 | `		/* MonthName [Day] [Year]. A 4-digit number here is the YEAR, not the day` |
|      - | 1919 | `		 * ("January 2020" is month+year, day defaults); a 1-2 digit number is the day. */` |
|    201 | 1920 | `		z += adv;` |
|    201 | 1921 | `		zAfterMon = z;` |
|    439 | 1922 | `		MDSKIPWS();` |
|    201 | 1923 | `		if( z < zEnd && SyisDigit(z[0]) ){` |
|    119 | 1924 | `			int nrun = 0;` |
|    119 | 1925 | `			const char *zp = z;` |
|    421 | 1926 | `			while( zp < zEnd && SyisDigit(zp[0]) && nrun < 4 ){ zp++; nrun++; }` |
|    119 | 1927 | `			if( nrun < 4 ){` |
|     79 | 1928 | `				d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|     79 | 1929 | `				haveDay = 1;` |
|     79 | 1930 | `				bMonthFirst = 1;` |
|    210 | 1931 | `				while( z < zEnd && MDISSUF((unsigned char)z[0]) ){ z++; nSuf++; }` |
|      - | 1932 | ``				/* php's `dateshortwithtimeshort`: a month, a day and a CLOCK are`` |
|      - | 1933 | `				 * ONE token there, and it reads longer than the year the same` |
|      - | 1934 | ``				 * digits would be -- which is what makes `january 12 12:00` noon`` |
|      - | 1935 | ``				 * on the 12th where `january 12 12` is the year 2012, and`` |
|      - | 1936 | ``				 * `january 12 123:00` the year 123 with a refusal behind it. */`` |
|     79 | 1937 | `				bClock = DtClockFollows(z,zEnd);` |
|     39 | 1938 | `			}` |
|     60 | 1939 | `		}` |
|   2404 | 1940 | `	}else if( SyisDigit(z[0]) ){` |
|      - | 1941 | `		/* Day MonthName [Year] */` |
|   1498 | 1942 | `		d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|   1498 | 1943 | `		if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|   1498 | 1944 | `		haveDay = 1;` |
|   2636 | 1945 | `		MDSKIPWS();` |
|   1498 | 1946 | `		if( (mo = DtMatchMonth(z,zEnd,&adv)) == 0 ){ return 0; }` |
|     87 | 1947 | `		z += adv;` |
|    200 | 1948 | `		MDSKIPWS();` |
|     44 | 1949 | `	}else{` |
|    808 | 1950 | `		return 0;` |
|      - | 1951 | `	}` |
|      - | 1952 | `	/* The optional YEAR -- but php's rule spells the run between the day and it` |
|      - | 1953 | ``	 * as REQUIRED, so `january 124` is no date at all where `january 12 4` and`` |
|      - | 1954 | ``	 * `january 12s4` are the year 2004. */`` |
|    286 | 1955 | `	if( !bClock && !(bMonthFirst && haveDay && nSuf == 0)` |
|    272 | 1956 | `	 && z < zEnd && SyisDigit(z[0]) ){` |
|    153 | 1957 | `		int ny = 0;` |
|    153 | 1958 | `		y = 0;` |
|    751 | 1959 | `		while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ y = y*10 + (z[0]-'0'); z++; ny++; }` |
|    153 | 1960 | `		if( ny <= 2 ){` |
|      5 | 1961 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|    ! 0 | 1962 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|      2 | 1963 | `		}` |
|    153 | 1964 | `		haveYear = 1;` |
|     76 | 1965 | `	}` |
|      - | 1966 | `	/* php spells the day's range inside the pattern too, so a day past 31 is not` |
|      - | 1967 | ``	 * this rule at all and the token is refused where it STARTS (`87 january` is`` |
|      - | 1968 | `	 * php's position 0), not where the month name ends. */` |
|      - | 1969 | ``	/* php's `datenoyear` ends in that run, and spells the day's range inside the`` |
|      - | 1970 | `	 * pattern as well -- so with a day past 31, or with nothing behind the day` |
|      - | 1971 | `	 * and no year to close the rule, this text is not that token. The MONTH NAME` |
|      - | 1972 | `	 * still is one of its own, though, and only the month-FIRST spelling can fall` |
|      - | 1973 | ``	 * back to it: `january 12x` is January with the digits left to the string,`` |
|      - | 1974 | ``	 * while `87 january` is php's refusal at position 0. */`` |
|    287 | 1975 | `	if( d > 31 \|\| (bMonthFirst && !haveYear && nSuf == 0 && z < zEnd && z[0] != 0) ){` |
|     15 | 1976 | `		if( !bMonthFirst ){` |
|      3 | 1977 | `			return 0;` |
|      - | 1978 | `		}` |
|     13 | 1979 | `		z = zAfterMon;` |
|     13 | 1980 | `		haveDay = 0;` |
|     13 | 1981 | `		haveYear = 0;` |
|     13 | 1982 | `		d = 1;` |
|      6 | 1983 | `	}` |
|      - | 1984 | `	/* optional time-of-day suffix */` |
|    285 | 1985 | `	*pzOut = z;` |
|    285 | 1986 | `	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }` |
|    261 | 1987 | `	p->m = mo;` |
|    261 | 1988 | `	if( haveDay ){ p->d = d; }` |
|    111 | 1989 | `	else if( haveYear ){ p->d = 1; }` |
|    261 | 1990 | `	if( haveYear ){ p->y = y; }` |
|    113 | 1991 | `	else if( bMonthFirst && haveDay ){` |
|      - | 1992 | ``		/* php's `datenoyear` writes the month and the day and UNSETS the year,`` |
|      - | 1993 | `		 * which is the whole difference between it and the day-first spelling:` |
|      - | 1994 | ``		 * `@100 january 12` has no year where `@100 12 january` keeps 1970. */`` |
|     25 | 1995 | `		p->y = DT_UNSET;` |
|     12 | 1996 | `	}` |
|    261 | 1997 | `	if( bClock ){` |
|      5 | 1998 | `		rcT = DtReadTimeOfDay(&z,zEnd,zIn,p);` |
|      5 | 1999 | `		*pzOut = z;` |
|      5 | 2000 | `		if( rcT < 0 ){ return rcT; }` |
|      5 | 2001 | `		return 1;` |
|      - | 2002 | `	}` |
|    257 | 2003 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,p);` |
|    257 | 2004 | `	*pzOut = z;` |
|    257 | 2005 | `	if( rcT != 0 ){ return rcT; }` |
|    257 | 2006 | `	return 1;` |
|      - | 2007 | `#undef MDSKIPWS` |
|      - | 2008 | `#undef MDISSUF` |
|   1253 | 2009 | `}` |
|      - | 2010 | `/*` |
|      - | 2011 | ` * php's reltextnumber -- the ORDINAL WORDS that stand where a relative COUNT` |
|      - | 2012 | `` * would. `first` through `twelfth` are 1..12 and the navigation four are the`` |
|      - | 2013 | `` * same rule's 1, 0 and -1, which is why `next day` and `first day` are one`` |
|      - | 2014 | `` * move and `second day` two of them.`` |
|      - | 2015 | ` *` |
|      - | 2016 | ` * Answers the bytes the word takes (0 for anything else) and says which half it` |
|      - | 2017 | `` * came from: php's `... week` SPECIAL -- the move to that week's Monday --`` |
|      - | 2018 | `` * belongs to the navigation words alone, so `next week` is that Monday while`` |
|      - | 2019 | `` * `first week` is a refusal and `first weeks` seven ordinary days.`` |
|      - | 2020 | ` */` |
|  13638 | 2021 | `static int DtRelWord(const char *z,const char *zEnd,sxi64 *pVal,int *pbNav)` |
|      4 | 2022 | `{` |
|      - | 2023 | `	static const struct { const char *zWord; int nWord; int iVal; int bNav; } aWord[] = {` |
|      - | 2024 | `		{ "previous", 8, -1, 1 }, { "next",     4,  1, 1 },` |
|      - | 2025 | `		{ "last",     4, -1, 1 }, { "this",     4,  0, 1 },` |
|      - | 2026 | `		{ "first",    5,  1, 0 }, { "second",   6,  2, 0 },` |
|      - | 2027 | `		{ "third",    5,  3, 0 }, { "fourth",   6,  4, 0 },` |
|      - | 2028 | `		{ "fifth",    5,  5, 0 }, { "sixth",    5,  6, 0 },` |
|      - | 2029 | `		{ "seventh",  7,  7, 0 }, { "eighth",   6,  8, 0 },` |
|      - | 2030 | `		{ "ninth",    5,  9, 0 }, { "tenth",    5, 10, 0 },` |
|      - | 2031 | `		{ "eleventh", 8, 11, 0 }, { "twelfth",  7, 12, 0 }` |
|      - | 2032 | `	};` |
|      - | 2033 | `	int k;` |
| 223362 | 2034 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(aWord) ; k++ ){` |
| 210368 | 2035 | `		int n = aWord[k].nWord;` |
| 210364 | 2036 | `		if( zEnd - z >= n && SyStrnicmp(z,aWord[k].zWord,n) == 0` |
|  80196 | 2037 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|    645 | 2038 | `			*pVal  = (sxi64)aWord[k].iVal;` |
|    645 | 2039 | `			*pbNav = aWord[k].bNav;` |
|    645 | 2040 | `			return n;` |
|      - | 2041 | `		}` |
| 104864 | 2042 | `	}` |
|  12998 | 2043 | `	return 0;` |
|   6823 | 2044 | `}` |
|      - | 2045 | `/*` |
|      - | 2046 | ` * Apply php's relative UNIT word at z with the amount v, and answer the bytes it` |
|      - | 2047 | ` * takes -- 0 when there is no unit word here. Shared by the two spellings that` |
|      - | 2048 | `` * reach one: a number in front of it, and php's `this`/`next`/`last`/`previous`,`` |
|      - | 2049 | `` * which is the same rule with the amount 0, 1 or -1 (`next hour`, `last year`).`` |
|      - | 2050 | ` */` |
|   2398 | 2051 | `static int DtRelUnit(const char *z,const char *zEnd,sxi64 v,dt_parsed *p,int *pbSpecial)` |
|      4 | 2052 | `{` |
|   2402 | 2053 | `	*pbSpecial = 0;` |
|      - | 2054 | `/* A unit word is never a token on its own -- the NUMBER (or the navigation` |
|      - | 2055 | ` * word) in front of it started the match, so nothing competes with it at its` |
|      - | 2056 | ``  * own position and letters behind it belong to whatever comes next: `+1 dayx` `` |
|      - | 2057 | `` * is a day and the military zone X, `+1 dayxyz` a day and an unknown zone. */`` |
|      - | 2058 | `#define DT_UNITEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0)` |
|      - | 2059 | `	/* php's SUB-SECOND relative units, checked before the words they are` |
|      - | 2060 | ``	 * prefixes of ("ms" would otherwise swallow "msec"). `us` is NOT one of`` |
|      - | 2061 | `	 * them there, and neither is the Greek mu -- only U+00B5, the MICRO` |
|      - | 2062 | `	 * SIGN, which is the two bytes 0xC2 0xB5 here. They accumulate apart` |
|      - | 2063 | ``	 * from the seconds and carry into them in DtApplyFields, so `-500`` |
|      - | 2064 | ``	 * microseconds` from midnight is the previous day's 23:59:59.999500. */`` |
|   2402 | 2065 | `	if( DT_UNITEQ("microseconds",12) ){ p->rus = DtWAdd(p->rus,v);        return 12; }` |
|   2366 | 2066 | `	else if( DT_UNITEQ("microsecond",11) ){ p->rus = DtWAdd(p->rus,v);    return 11; }` |
|   2352 | 2067 | `	else if( DT_UNITEQ("milliseconds",12) ){ p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 12; }` |
|   2346 | 2068 | `	else if( DT_UNITEQ("millisecond",11) ){ p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 11; }` |
|   2344 | 2069 | `	else if( DT_UNITEQ("usecs",5) )  { p->rus = DtWAdd(p->rus,v);         return 5; }` |
|   2342 | 2070 | `	else if( DT_UNITEQ("usec",4) )   { p->rus = DtWAdd(p->rus,v);         return 4; }` |
|   2336 | 2071 | `	else if( DT_UNITEQ("msecs",5) )  { p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 5; }` |
|   2332 | 2072 | `	else if( DT_UNITEQ("msec",4) )   { p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 4; }` |
|   2322 | 2073 | `	else if( DT_UNITEQ("\xc2\xb5s",3) ){ p->rus = DtWAdd(p->rus,v);       return 3; }` |
|   2314 | 2074 | `	else if( DT_UNITEQ("ms",2) )     { p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 2; }` |
|   2296 | 2075 | `	else if( DT_UNITEQ("seconds",7) ){ p->rs = DtWAdd(p->rs,v);           return 7; }` |
|   2276 | 2076 | `	else if( DT_UNITEQ("second",6) ) { p->rs = DtWAdd(p->rs,v);           return 6; }` |
|   2270 | 2077 | `	else if( DT_UNITEQ("secs",4) )   { p->rs = DtWAdd(p->rs,v);           return 4; }` |
|   2268 | 2078 | `	else if( DT_UNITEQ("sec",3) )    { p->rs = DtWAdd(p->rs,v);           return 3; }` |
|   2264 | 2079 | `	else if( DT_UNITEQ("minutes",7) ){ p->ri = DtWAdd(p->ri,v);           return 7; }` |
|   2258 | 2080 | `	else if( DT_UNITEQ("minute",6) ) { p->ri = DtWAdd(p->ri,v);           return 6; }` |
|   2256 | 2081 | `	else if( DT_UNITEQ("mins",4) )   { p->ri = DtWAdd(p->ri,v);           return 4; }` |
|   2254 | 2082 | `	else if( DT_UNITEQ("min",3) )    { p->ri = DtWAdd(p->ri,v);           return 3; }` |
|   2252 | 2083 | `	else if( DT_UNITEQ("hours",5) )  { p->rh = DtWAdd(p->rh,v);           return 5; }` |
|   2204 | 2084 | `	else if( DT_UNITEQ("hour",4) )   { p->rh = DtWAdd(p->rh,v);           return 4; }` |
|   2186 | 2085 | `	else if( DT_UNITEQ("days",4) )   { p->rd = DtWAdd(p->rd,v);           return 4; }` |
|   2108 | 2086 | `	else if( DT_UNITEQ("day",3) )    { p->rd = DtWAdd(p->rd,v);           return 3; }` |
|      - | 2087 | `	/* php's business-day words go BEFORE the plain week, because its scanner` |
|      - | 2088 | ``	 * takes the longest of the two and `week` is their prefix. */`` |
|   1981 | 2089 | `	else if( DT_UNITEQ("weekdays",8) ){ p->bWeekdays = 1; *pbSpecial = 1; p->iWeekdays = v; return 8; }` |
|   1841 | 2090 | `	else if( DT_UNITEQ("weekday",7) ) { p->bWeekdays = 1; *pbSpecial = 1; p->iWeekdays = v; return 7; }` |
|   1807 | 2091 | `	else if( DT_UNITEQ("weeks",5) )  { p->rd = DtWAdd(p->rd,DtWMul(v,7)); return 5; }` |
|   1785 | 2092 | `	else if( DT_UNITEQ("week",4) )   { p->rd = DtWAdd(p->rd,DtWMul(v,7)); return 4; }` |
|   1687 | 2093 | `	else if( DT_UNITEQ("fortnights",10) ){ p->rd = DtWAdd(p->rd,DtWMul(v,14)); return 10; }` |
|   1685 | 2094 | `	else if( DT_UNITEQ("fortnight",9) )  { p->rd = DtWAdd(p->rd,DtWMul(v,14)); return 9; }` |
|   1677 | 2095 | `	else if( DT_UNITEQ("months",6) ) { p->rm = DtWAdd(p->rm,v);           return 6; }` |
|   1647 | 2096 | `	else if( DT_UNITEQ("month",5) )  { p->rm = DtWAdd(p->rm,v);           return 5; }` |
|   1441 | 2097 | `	else if( DT_UNITEQ("years",5) )  { p->ry = DtWAdd(p->ry,v);           return 5; }` |
|   1437 | 2098 | `	else if( DT_UNITEQ("year",4) )   { p->ry = DtWAdd(p->ry,v);           return 4; }` |
|      - | 2099 | `	/* php's BUSINESS-day count, which is not a field of the vector at all but a` |
|      - | 2100 | `	 * move of its own (see DtAdjustWeekdays) */` |
|      - | 2101 | `	/* php SETS this one rather than adding to it, so the last count in the string` |
|      - | 2102 | `	 * is the only one that moves anything. */` |
|   1417 | 2103 | `	return 0;` |
|      - | 2104 | `#undef DT_UNITEQ` |
|   1203 | 2105 | `}` |
|      - | 2106 | `/*` |
|      - | 2107 | ` * How many bytes a relative UNIT word would take here, without applying it.` |
|      - | 2108 | ` * php's scanner takes the LONGEST rule that matches, and a weekday name is the` |
|      - | 2109 | `` * prefix of two unit words (`mon` of `month`, `sat` of nothing but `mon` is`` |
|      - | 2110 | ` * enough): the counted spellings ask this before they claim a weekday, which is` |
|      - | 2111 | `` * what keeps `next month` a month and `3 months` three of them.`` |
|      - | 2112 | ` */` |
|    486 | 2113 | `static int DtRelUnitLen(const char *z,const char *zEnd,const dt_parsed *p)` |
|      1 | 2114 | `{` |
|    487 | 2115 | `	dt_parsed sTmp = *p;` |
|    487 | 2116 | `	int bSpec = 0;` |
|    487 | 2117 | `	return DtRelUnit(z,zEnd,0,&sTmp,&bSpec);` |
|      1 | 2118 | `}` |
|      - | 2119 | `/*` |
|      - | 2120 | ` * php's date-string parse, onto the field vector: absolute forms` |
|      - | 2121 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]" \|` |
|      - | 2122 | ` * a textual month date, the keywords today/midnight/noon/tomorrow/yesterday, the` |
|      - | 2123 | ` * weekday and month navigation words, and relative sequences` |
|      - | 2124 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". NOTHING is applied` |
|      - | 2125 | ` * here -- DtApplyFields does that, in php's order, once the whole string is read.` |
|      - | 2126 | ` * Returns 0 on success, or the byte position of the first unparseable character` |
|      - | 2127 | ` * +1 (for php's "at position N" message).` |
|      - | 2128 | ` */` |
|      - | 2129 | `/*` |
|      - | 2130 | `` * php's `@epoch` token, which its scanner reads ANYWHERE in a string rather`` |
|      - | 2131 | `` * than only at its head: `2020-01-02 @100` and `12:00 @100` are the epoch`` |
|      - | 2132 | ` * there, not refusals.` |
|      - | 2133 | ` *` |
|      - | 2134 | `` * The shape is `"@" "-"? [0-9]+ ("." [0-9]{0,6})?`. A PLUS is no part of it, so`` |
|      - | 2135 | `` * `@+100` is an unexpected `@` with a UTC offset behind it; and the fraction`` |
|      - | 2136 | ``  * stops at SIX digits, which leaves the seventh to the string (`@100.1234567` `` |
|      - | 2137 | `` * refuses on it while `@100.1234567890` reads the trailing four as a year).`` |
|      - | 2138 | ` *` |
|      - | 2139 | ` * The ACTION is php's own order, and the order is what shows: TIMELIB_UNHAVE_DATE` |
|      - | 2140 | ` * and TIMELIB_UNHAVE_TIME first -- which ZERO the civil fields rather than` |
|      - | 2141 | ` * unsetting them, so an epoch behind a date reads back as the year 0, not as the` |
|      - | 2142 | ` * date -- then TIMELIB_HAVE_TZ, then the value. The middle step is a RETURN when` |
|      - | 2143 | `` * the string already named a zone, so `UTC @100` leaves nothing behind but those`` |
|      - | 2144 | ` * zeroes: neither 1970 nor the seconds are ever written. An empty fraction is` |
|      - | 2145 | `` * php's `Found unexpected data`, raised at the `@` and after the value, which is`` |
|      - | 2146 | `` * why `@100.,UTC` still reads back as 1970 plus a hundred seconds.`` |
|      - | 2147 | ` *` |
|      - | 2148 | ` * Advances *pz over what it took; answers 0 when this is not the token, 1 when` |
|      - | 2149 | ` * it is, or an error code in DtParse's own convention.` |
|      - | 2150 | ` */` |
|   8198 | 2151 | `static int DtTryEpoch(const char **pz,const char *zEnd,dt_parsed *p,const char *zIn)` |
|      4 | 2152 | `{` |
|   8202 | 2153 | `	const char *z = *pz,*zAt = z;` |
|   8202 | 2154 | `	sxi64 v = 0,us = 0;` |
|   8202 | 2155 | `	int neg = 0,nDig = 0,bDot = 0,nFrac = 0,k,rc;` |
|   8202 | 2156 | `	if( z >= zEnd \|\| z[0] != '@' ){` |
|   7186 | 2157 | `		return 0;` |
|      - | 2158 | `	}` |
|   1019 | 2159 | `	z++;` |
|   1019 | 2160 | `	if( z < zEnd && z[0] == '-' ){ neg = 1; z++; }` |
|   1019 | 2161 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|      - | 2162 | ``		/* php's lexer never matched a token here at all: the `@` is the refusal */`` |
|      9 | 2163 | `		return (int)(zAt - zIn) + 1;` |
|      - | 2164 | `	}` |
|   3379 | 2165 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|   2371 | 2166 | `		if( nDig < DT_DIGITS_SAFE ){ v = v*10 + (z[0]-'0'); }` |
|   2371 | 2167 | `		nDig++;` |
|   2371 | 2168 | `		z++;` |
|      3 | 2169 | `	}` |
|   1011 | 2170 | `	if( z < zEnd && z[0] == '.' ){` |
|     45 | 2171 | `		bDot = 1;` |
|     45 | 2172 | `		z++;` |
|    129 | 2173 | `		while( nFrac < 6 && z < zEnd && SyisDigit(z[0]) ){` |
|     85 | 2174 | `			us = us*10 + (z[0]-'0');` |
|     85 | 2175 | `			nFrac++;` |
|     85 | 2176 | `			z++;` |
|      1 | 2177 | `		}` |
|    225 | 2178 | `		for( k = nFrac ; k < 6 ; k++ ){ us *= 10; }` |
|     22 | 2179 | `	}` |
|   1011 | 2180 | `	*pz = z;` |
|   1011 | 2181 | `	if( nDig > DT_DIGITS_EPOCH ){` |
|      - | 2182 | ``		/* php reports it at the `@`, with its own reason. */`` |
|      9 | 2183 | `		return -((int)(zAt - zIn) + 1) - DT_ERR_RANGE;` |
|      - | 2184 | `	}` |
|      - | 2185 | `	/* php's order: HAVE_RELATIVE, then UNHAVE_DATE and UNHAVE_TIME, then` |
|      - | 2186 | `	 * HAVE_TZ -- so an epoch that bails on the zone has already marked the parse` |
|      - | 2187 | `	 * relative, and date_parse() shows an all-zero block for it. */` |
|   1003 | 2188 | `	p->bHaveRel = 1;` |
|   1003 | 2189 | `	p->y = p->m = p->d = 0;` |
|   1003 | 2190 | `	p->bHaveDate = 0;` |
|   1003 | 2191 | `	DtUnhaveTime(p);` |
|   1003 | 2192 | `	rc = DtZoneCount(p);` |
|   1003 | 2193 | `	if( rc < 0 ){` |
|    ! 0 | 2194 | `		return -((int)(zAt - zIn) + 1) - DT_ERR_DZONE;` |
|      - | 2195 | `	}` |
|   1003 | 2196 | `	if( rc > 0 ){` |
|     13 | 2197 | `		DtWarnPend(p,(int)(zAt - zIn),"Double timezone specification");` |
|     13 | 2198 | `		return 1;   /* php returns from inside HAVE_TZ: nothing below runs */` |
|      - | 2199 | `	}` |
|    991 | 2200 | `	DtZoneStore(p,0,0,0,0);` |
|    991 | 2201 | `	p->bEpoch = 1;` |
|    991 | 2202 | `	p->y = 1970; p->m = 1; p->d = 1;` |
|    991 | 2203 | `	p->h = p->i = p->s = p->us = 0;` |
|    991 | 2204 | `	p->bUsUnset = 0;` |
|    991 | 2205 | `	if( neg ){ v = -v; }` |
|      - | 2206 | `	/* php adds the fraction to the RELATIVE microseconds with the token's own` |
|      - | 2207 | ``	 * sign rather than borrowing a second for it, so `@-1.5` is -1s and -500000us`` |
|      - | 2208 | `	 * there -- the same instant, and the count date_parse() shows. Being relative` |
|      - | 2209 | ``	 * is also what makes a later `tomorrow`, whose whole job is to zero the`` |
|      - | 2210 | `	 * clock, leave it standing. */` |
|    991 | 2211 | `	if( nFrac > 0 ){` |
|     35 | 2212 | `		p->rus = neg ? -us : us;` |
|     17 | 2213 | `	}` |
|    991 | 2214 | `	p->rs = DtWAdd(p->rs,v);` |
|    991 | 2215 | `	if( bDot && nFrac == 0 ){` |
|      9 | 2216 | `		return -((int)(zAt - zIn) + 1) - DT_ERR_UNEXPDATA;` |
|      - | 2217 | `	}` |
|    983 | 2218 | `	return 1;` |
|   4103 | 2219 | `}` |
|      - | 2220 | `/* Forward: the scan publishes what it collects, and words a refusal in php's` |
|      - | 2221 | ` * own terms -- both live below, beside the record they write to. */` |
|      - | 2222 | `static const char * DtParseErr(const char *zIn,int nLen,int iErrPos,int *piPos,char *pcAt);` |
|      - | 2223 | `static void DtRecErr(phl_dt_lasterr *pRec,int iPos,const char *zMsg);` |
|      - | 2224 | `static void DtRecWarn(phl_dt_lasterr *pRec,int iPos,const char *zMsg);` |
|      - | 2225 | `static void DtRecReset(phl_dt_lasterr *pRec);` |
|      - | 2226 | `static int DtDaysInMonth(sxi64 y,int m);` |
|   6138 | 2227 | `static int DtParseFields(const char *zIn,int nLen,dt_parsed *p,phl_dt_lasterr *pRec)` |
|      4 | 2228 | `{` |
|      - | 2229 | `	const char *z,*zEnd;` |
|   6142 | 2230 | `	const char *zPrev = 0;` |
|   6142 | 2231 | `	int bAny = 0;` |
|   6142 | 2232 | `	int iRc,iFirst = 0,k;` |
|      - | 2233 | `	/* php refuses an EMPTY string before it does anything else, and the test is` |
|      - | 2234 | `	 * on what the caller handed over rather than on what the trim leaves: a` |
|      - | 2235 | ``	 * string of blanks parses as `now`. The constructors never see it, because`` |
|      - | 2236 | ``	 * php hands THEM the word `now` in its place -- only modify() and the`` |
|      - | 2237 | `	 * component readers do. */` |
|   6142 | 2238 | `	if( nLen < 1 ){` |
|      3 | 2239 | `		DtRecErr(pRec,0,"Empty string");` |
|      3 | 2240 | `		return -1 - DT_ERR_EMPTY;` |
|      - | 2241 | `	}` |
|      - | 2242 | `	/* php's trim comes FIRST and the positions below are all measured from what` |
|      - | 2243 | `	 * it leaves, so rebase on it here and every rule inherits the answer. */` |
|   6140 | 2244 | `	DtTrimEnds(&zIn,&nLen);` |
|   6140 | 2245 | `	z = zIn;` |
|   6140 | 2246 | `	zEnd = &zIn[nLen];` |
|      - | 2247 | `	/* The wider set php tolerates at the trailing END -- a carriage return, a` |
|      - | 2248 | `	 * vertical tab, a form feed, a NUL -- closes the string; at the FRONT the` |
|      - | 2249 | `	 * trim has taken what it takes and everything left is an ordinary token` |
|      - | 2250 | `	 * separator, which DT_SKIP_WS below reads. Stepping over more than that` |
|      - | 2251 | ``	 * loses a byte php refuses: `.\rjanuary` is an unexpected `\r` there. */`` |
|   6144 | 2252 | `	while( zEnd > z && DtIsEdgeSep((unsigned char)zEnd[-1]) ){ zEnd--; }` |
|      - | 2253 | `#define DT_SKIP_WS() while( DtIsSepAt(z,zEnd) ){ z++; }` |
|      - | 2254 | ``/* ...and the run INSIDE one token, which is php's narrower `space`. */`` |
|      - | 2255 | `#define DT_SPACE() while( z < zEnd && DtIsSpace((unsigned char)z[0]) ){ z++; }` |
|      - | 2256 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|      - | 2257 | `	&& DtWordEnds(z,zEnd,nKw))` |
|      - | 2258 | ``/* php's SINGULAR `week`, the word its `... week` special is spelled with. It is`` |
|      - | 2259 | ` * only ever the TAIL of a longer rule, so nothing competes at its own position` |
|      - | 2260 | `` * and letters behind it belong to whatever comes next (`this weekjanuary` is`` |
|      - | 2261 | ` * that week and then January) -- except any UNIT word that reads LONGER here,` |
|      - | 2262 | `` * which is that rule instead: `weeks`, `weekday` and `weekdays`. */`` |
|      - | 2263 | `#define DT_WEEKSING() (zEnd-z >= 4 && SyStrnicmp(z,"week",4) == 0 \` |
|      - | 2264 | `	&& DtRelUnitLen(z,zEnd,p) <= 4)` |
|      - | 2265 | `/*` |
|      - | 2266 | ` * php's scanner RECORDS a refusal and reads on, so an error is not the end of` |
|      - | 2267 | ` * the parse: the reason and the byte are published and the walk resumes one` |
|      - | 2268 | ` * byte past what was named -- which is exactly where php's catch-all rule` |
|      - | 2269 | ` * leaves its cursor. A token that MATCHED and whose action then complained is` |
|      - | 2270 | ` * already behind the cursor, because the rule that raised it moved past it, so` |
|      - | 2271 | ` * taking whichever of the two is FURTHER covers both kinds. The first code is` |
|      - | 2272 | ` * kept: it is the one the constructors put in their sentence.` |
|      - | 2273 | ` */` |
|      - | 2274 | `#define DT_FAIL(iCode) do{ \` |
|      - | 2275 | `		int _p = 0; \` |
|      - | 2276 | `		char _c = ' '; \` |
|      - | 2277 | `		const char *_m = DtParseErr(zIn,nLen,(iCode),&_p,&_c); \` |
|      - | 2278 | `		DtRecErr(pRec,_p,_m); \` |
|      - | 2279 | `		if( iFirst == 0 ){ iFirst = (iCode); } \` |
|      - | 2280 | `		if( z < &zIn[_p + 1] ){ z = &zIn[_p + 1]; } \` |
|      - | 2281 | `		if( z > zEnd ){ z = zEnd; } \` |
|      - | 2282 | `	}while(0)` |
|   6190 | 2283 | `	DT_SKIP_WS();` |
|   6140 | 2284 | `	if( z >= zEnd ){` |
|      - | 2285 | `		/* php: the empty string is "now" */` |
|      3 | 2286 | `		return 0;` |
|      - | 2287 | `	}` |
|      - | 2288 | `	/*` |
|      - | 2289 | `	 * One rule set, tried at every position -- php's scanner has no head of its` |
|      - | 2290 | `	 * own and neither does this walk. The keyword rules come first and are` |
|      - | 2291 | `	 * spelled in LETTERS, so nothing they could claim reaches the date rules` |
|      - | 2292 | `	 * behind them by another route.` |
|      - | 2293 | `	 */` |
|   3354 | 2294 | `	for(;;){` |
|      - | 2295 | `		/* Every pass must CONSUME something. A shape rule that claims a token` |
|      - | 2296 | `		 * without advancing the cursor would spin here forever -- and one did:` |
|      - | 2297 | `		 * the ISO rule's refusal encodes the token's POSITION, and a token at` |
|      - | 2298 | `		 * position 0 encodes as the same 1 that means "matched", so` |
|      - | 2299 | ``		 * `new DateTime('2020-1-1 12:00')` hung the engine outright. The rule is`` |
|      - | 2300 | `		 * fixed above; this makes the whole class of it a refusal instead. */` |
|  14344 | 2301 | `		if( z == zPrev ){` |
|      9 | 2302 | `			DT_FAIL((int)(z - zIn) + 1);` |
|      9 | 2303 | `			if( z == zPrev ){` |
|      - | 2304 | `				/* Nothing could advance it -- stop rather than spin, which is the` |
|      - | 2305 | `				 * failure this guard exists for. */` |
|    ! 0 | 2306 | `				break;` |
|      - | 2307 | `			}` |
|      9 | 2308 | `			continue;` |
|      - | 2309 | `		}` |
|  14336 | 2310 | `		zPrev = z;` |
|  15474 | 2311 | `		DT_SKIP_WS();` |
|  14336 | 2312 | `		if( z >= zEnd ){` |
|   6138 | 2313 | `			break;` |
|      - | 2314 | `		}` |
|   8202 | 2315 | `		if( (iRc = DtTryEpoch(&z,zEnd,p,zIn)) != 0 ){` |
|   1019 | 2316 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|   1003 | 2317 | `			bAny = 1;` |
|   1003 | 2318 | `			continue;` |
|      - | 2319 | `		}` |
|   7186 | 2320 | `		if( DT_LOWEQ("now",3) ){` |
|     42 | 2321 | `			z += 3;` |
|     42 | 2322 | `			bAny = 1;` |
|     42 | 2323 | `			continue;` |
|      - | 2324 | `		}` |
|   7146 | 2325 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|     21 | 2326 | `			DtUnhaveTime(p);` |
|     21 | 2327 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|     21 | 2328 | `			bAny = 1;` |
|     21 | 2329 | `			continue;` |
|      - | 2330 | `		}` |
|   7126 | 2331 | `		if( DT_LOWEQ("noon",4) ){` |
|     15 | 2332 | `			DtUnhaveTime(p);` |
|     15 | 2333 | `			p->h = 12;` |
|     15 | 2334 | `			p->nTimeTok = 1;` |
|     15 | 2335 | `			z += 4;` |
|     15 | 2336 | `			bAny = 1;` |
|     15 | 2337 | `			continue;` |
|      - | 2338 | `		}` |
|      - | 2339 | `		/* php SETS the relative day for these two rather than adding to it, so` |
|      - | 2340 | ``		 * either one wipes whatever days came before it: `+3 days tomorrow` is one`` |
|      - | 2341 | ``		 * day on, and `tomorrow yesterday` is yesterday. */`` |
|   7112 | 2342 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|     39 | 2343 | `			DtUnhaveTime(p);` |
|     39 | 2344 | `			p->bHaveRel = 1;` |
|     39 | 2345 | `			p->rd = 1;` |
|     39 | 2346 | `			z += 8;` |
|     39 | 2347 | `			bAny = 1;` |
|     39 | 2348 | `			continue;` |
|      - | 2349 | `		}` |
|   7074 | 2350 | `		if( DT_LOWEQ("yesterday",9) ){` |
|     31 | 2351 | `			DtUnhaveTime(p);` |
|     31 | 2352 | `			p->bHaveRel = 1;` |
|     31 | 2353 | `			p->rd = -1;` |
|     31 | 2354 | `			z += 9;` |
|     31 | 2355 | `			bAny = 1;` |
|     31 | 2356 | `			continue;` |
|      - | 2357 | `		}` |
|      - | 2358 | `		/* "first\|last day of": php's own standalone token. It records the flag and` |
|      - | 2359 | `		 * nothing else — whatever names the target month ("next month", "January` |
|      - | 2360 | `		 * 2021", or nothing at all) is an ordinary token after it, and the flag is` |
|      - | 2361 | `		 * applied LAST, which is what makes it swallow any relative days beside it. */` |
|   7044 | 2362 | `		if( DT_LOWEQ("first",5) \|\| DT_LOWEQ("last",4) ){` |
|    229 | 2363 | `			const char *zSave = z;` |
|    229 | 2364 | `			int bFirst = (SyToLower((unsigned char)z[0]) == 'f');` |
|    229 | 2365 | `			z += bFirst ? 5 : 4;` |
|    457 | 2366 | `			DT_SPACE();` |
|    229 | 2367 | `			if( DT_LOWEQ("day",3) ){` |
|    101 | 2368 | `				z += 3;` |
|    197 | 2369 | `				DT_SPACE();` |
|    101 | 2370 | `				if( DT_LOWEQ("of",2) ){` |
|     97 | 2371 | `					z += 2;` |
|     97 | 2372 | `					p->iFirstLast = bFirst ? 1 : 2;` |
|     97 | 2373 | `					p->bHaveRel = 1;` |
|     97 | 2374 | `					bAny = 1;` |
|     97 | 2375 | `					continue;` |
|      - | 2376 | `				}` |
|      2 | 2377 | `			}` |
|    133 | 2378 | `			z = zSave; /* not the "first\|last day of" shape: rewind */` |
|     66 | 2379 | `		}` |
|      - | 2380 | `		/* Weekday navigation: "[next\|last\|previous\|this] <weekday>" moves to the` |
|      - | 2381 | `		 * target weekday's midnight. php's behaviour code decides whether a base` |
|      - | 2382 | `		 * day that already matches counts: "next"/"last" skip it, a bare name or` |
|      - | 2383 | `		 * "this" keeps it. The move itself happens in DtAdjustWeekday, BEFORE the` |
|      - | 2384 | `		 * relative vector, which is php's order. */` |
|      - | 2385 | `		{` |
|   6948 | 2386 | `			const char *zSave = z;` |
|   6948 | 2387 | `			sxi64 iCnt = 0;` |
|   6948 | 2388 | `			int bNav = 0,bHavePrefix = 0,nWord;` |
|      - | 2389 | `			int adv,dow;` |
|   6948 | 2390 | `			if( (nWord = DtRelWord(z,zEnd,&iCnt,&bNav)) > 0 ){` |
|    407 | 2391 | `				z += nWord;` |
|    813 | 2392 | `				DT_SPACE();` |
|    407 | 2393 | `				bHavePrefix = 1;` |
|    203 | 2394 | `			}` |
|   6948 | 2395 | `			dow = DtMatchWeekdayEx(z,zEnd,&adv,bHavePrefix);` |
|   6948 | 2396 | `			if( dow >= 0 && DtRelUnitLen(z,zEnd,p) > adv ){` |
|     77 | 2397 | ``				dow = -1;   /* `next month` is the UNIT, not `mon` and a stray `th` */`` |
|     38 | 2398 | `			}` |
|   6948 | 2399 | `			if( dow >= 0 && bHavePrefix ){` |
|      - | 2400 | ``				/* php's `first monday of`: a WORD count, a weekday and the word`` |
|      - | 2401 | ``				 * `of` are one token, and what it names is a weekday inside a`` |
|      - | 2402 | ``				 * MONTH. The digit spelling does not reach it -- `1 monday of` is`` |
|      - | 2403 | `				 * a refusal there -- and neither does a bare name. */` |
|    169 | 2404 | `				const char *zOf = &z[adv];` |
|    261 | 2405 | `				while( zOf < zEnd && DtIsSpace((unsigned char)zOf[0]) ){ zOf++; }` |
|    168 | 2406 | `				if( zEnd - zOf >= 2 && SyStrnicmp(zOf,"of",2) == 0` |
|     86 | 2407 | `				 && (zEnd - zOf == 2 \|\| !SyisAlpha(zOf[2])) ){` |
|     79 | 2408 | `					DtUnhaveTime(p);` |
|     79 | 2409 | `					p->bWdayOf = 1;` |
|     79 | 2410 | `					p->bHaveRel = 1;` |
|     79 | 2411 | `					p->bWday = 1;` |
|     79 | 2412 | `					p->iWday = dow;` |
|     79 | 2413 | `					if( p->iWdayBehavior != 2 ){` |
|      - | 2414 | `						/* a day that already matches counts for every count php` |
|      - | 2415 | ``						 * spells forward (behaviour 1); `last` and `previous`,`` |
|      - | 2416 | `						 * which walk back a week from the month after, skip it */` |
|     79 | 2417 | `						p->iWdayBehavior = iCnt >= 0 ? 1 : 0;` |
|     39 | 2418 | `					}` |
|     79 | 2419 | `					if( iCnt >= 1 ){` |
|     55 | 2420 | `						p->rd = DtWAdd(p->rd,DtWMul(iCnt - 1,7));` |
|     28 | 2421 | `					}else{` |
|     25 | 2422 | `						p->iWdayOfNext = 1;` |
|     25 | 2423 | `						if( iCnt < 0 ){` |
|     19 | 2424 | `							p->rd = DtWAdd(p->rd,-7);` |
|      9 | 2425 | `						}` |
|      - | 2426 | `					}` |
|     79 | 2427 | `					z = &zOf[2];` |
|     79 | 2428 | `					bAny = 1;` |
|    165 | 2429 | `					continue;` |
|      - | 2430 | `				}` |
|     45 | 2431 | `			}` |
|   6870 | 2432 | `			if( dow >= 0 ){` |
|    173 | 2433 | `				DtUnhaveTime(p);` |
|    173 | 2434 | `				p->bHaveRel = 1;` |
|    173 | 2435 | `				p->bWday = 1;` |
|    173 | 2436 | `				p->iWday = dow;` |
|      - | 2437 | `				/* php: a COUNT word carries behaviour 0 and shifts a week per` |
|      - | 2438 | ``				 * count past the first (`last monday` is -7 days from the`` |
|      - | 2439 | ``				 * matching one, `second monday` +7); "this" is that rule's zero`` |
|      - | 2440 | `				 * and carries behaviour 1, as a bare name does. A bare name does` |
|      - | 2441 | ``				 * NOT overwrite the WEEK behaviour a `... week` word already set,`` |
|      - | 2442 | ``				 * which is what keeps `last week monday` in that week. */`` |
|    173 | 2443 | `				if( bHavePrefix ){` |
|     91 | 2444 | `					p->iWdayBehavior = (iCnt != 0) ? 0 : 1;` |
|     91 | 2445 | `					p->rd = DtWAdd(p->rd,DtWMul(iCnt > 0 ? iCnt - 1 : iCnt,7));` |
|    128 | 2446 | `				}else if( p->iWdayBehavior != 2 ){` |
|     83 | 2447 | `					p->iWdayBehavior = 1;` |
|     41 | 2448 | `				}` |
|    173 | 2449 | `				z += adv;` |
|    173 | 2450 | `				bAny = 1;` |
|    173 | 2451 | `				continue;` |
|      - | 2452 | `			}` |
|   6698 | 2453 | `			z = zSave; /* prefix did not introduce a weekday: rewind and try the rest */` |
|      - | 2454 | `		}` |
|      - | 2455 | `		/* Standalone "this\|next\|last (month\|week)". php's month is an ordinary` |
|      - | 2456 | `		 * relative month; its WEEK is a weekday-relative move to the Monday of the` |
|      - | 2457 | `		 * week (behaviour 2) plus the whole weeks, which is why "next week" is that` |
|      - | 2458 | `		 * Monday and not seven days from the base day. */` |
|      - | 2459 | `		{` |
|   6698 | 2460 | `			const char *zSave = z;` |
|   6698 | 2461 | `			sxi64 iCnt = 0;` |
|   6698 | 2462 | `			int bNav = 0,nWord;` |
|   6698 | 2463 | `			if( (nWord = DtRelWord(z,zEnd,&iCnt,&bNav)) > 0 ){` |
|    239 | 2464 | `				z += nWord;` |
|    477 | 2465 | `				DT_SPACE();` |
|    239 | 2466 | `				if( bNav && DT_WEEKSING() ){` |
|     81 | 2467 | `					z += 4;` |
|     81 | 2468 | `					p->bHaveRel = 1;` |
|     81 | 2469 | `					p->rd = DtWAdd(p->rd,DtWMul(iCnt,7));` |
|     81 | 2470 | `					if( !p->bWday ){        /* php: Monday, unless a weekday was` |
|      - | 2471 | `						* already named ("monday this week") */` |
|     55 | 2472 | `						p->bWday = 1;` |
|     55 | 2473 | `						p->iWday = 1;` |
|     27 | 2474 | `					}` |
|     81 | 2475 | `					p->iWdayBehavior = 2;` |
|     81 | 2476 | `					bAny = 1;` |
|    157 | 2477 | `					continue;` |
|      - | 2478 | `				}` |
|      - | 2479 | ``				/* The SINGULAR `week` is that special's own word and no other`` |
|      - | 2480 | ``				 * count reaches it: `first week` is a refusal in php where`` |
|      - | 2481 | ``				 * `first weeks` is seven ordinary days. */`` |
|    159 | 2482 | `				if( bNav \|\| !DT_WEEKSING() ){` |
|      - | 2483 | `					int nU,bSpec;` |
|    155 | 2484 | `					nU = DtRelUnit(z,zEnd,iCnt,p,&bSpec);` |
|    155 | 2485 | `					if( nU > 0 ){` |
|      - | 2486 | `						/* php's business-day count zeroes the clock when a WORD` |
|      - | 2487 | ``						 * asked for it (`next weekday` is midnight) and leaves it`` |
|      - | 2488 | ``						 * alone when a number did (`2 weekdays` keeps the hour). */`` |
|    153 | 2489 | `						if( bSpec ){ DtUnhaveTime(p); }` |
|    153 | 2490 | `						p->bHaveRel = 1;` |
|    153 | 2491 | `						z += nU;` |
|    153 | 2492 | `						bAny = 1;` |
|    153 | 2493 | `						continue;` |
|      - | 2494 | `					}` |
|      1 | 2495 | `				}` |
|      3 | 2496 | `			}` |
|   6466 | 2497 | `			z = zSave;` |
|      - | 2498 | `		}` |
|      - | 2499 | ``		/* A bare `weekday`, with no count in front of it, is not the business-day`` |
|      - | 2500 | `		 * move at all in php but the MONDAY hunt -- the same answer a bare weekday` |
|      - | 2501 | `		 * NAME gives. */` |
|   6466 | 2502 | `		if( DT_LOWEQ("weekdays",8) \|\| DT_LOWEQ("weekday",7) ){` |
|     13 | 2503 | `			DtUnhaveTime(p);` |
|     13 | 2504 | `			p->bHaveRel = 1;` |
|     13 | 2505 | `			p->bWday = 1;` |
|     13 | 2506 | `			p->iWday = 1;` |
|     13 | 2507 | `			if( p->iWdayBehavior != 2 ){ p->iWdayBehavior = 1; }` |
|     13 | 2508 | `			z += DT_LOWEQ("weekdays",8) ? 8 : 7;` |
|     13 | 2509 | `			bAny = 1;` |
|     13 | 2510 | `			continue;` |
|      - | 2511 | `		}` |
|      - | 2512 | ``		/* php's `ago` NEGATES the relative vector as it stands -- the weekday it`` |
|      - | 2513 | ``		 * hunts for included, which is what makes `next monday ago` the Monday`` |
|      - | 2514 | ``		 * before -- so a second `ago` puts it back. */`` |
|   6454 | 2515 | `		if( DT_LOWEQ("ago",3) ){` |
|     37 | 2516 | `			p->ry = -p->ry; p->rm = -p->rm; p->rd = -p->rd;` |
|     37 | 2517 | `			p->rh = -p->rh; p->ri = -p->ri; p->rs = -p->rs;   /* NOT the micro-` |
|      - | 2518 | ``				* seconds: php's `ago` leaves that one field standing */`` |
|     37 | 2519 | `			p->iWday = -p->iWday;` |
|     37 | 2520 | `			p->iWeekdays = -p->iWeekdays;` |
|     37 | 2521 | `			z += 3;` |
|     37 | 2522 | `			bAny = 1;` |
|     37 | 2523 | `			continue;` |
|      - | 2524 | `		}` |
|      - | 2525 | `		/* The absolute DATE tokens, php's own rule that any of them may stand` |
|      - | 2526 | `		 * anywhere in the string: "first day of january", "+1 day january",` |
|      - | 2527 | ``		 * "march 3" and the tail of `12345-01-01` (a compact time, then a date)`` |
|      - | 2528 | `		 * all reach here. The dates go first so that the longest reading wins --` |
|      - | 2529 | ``		 * `1.2.2020` is a date where a bare `1.2` is the time 01:02. */`` |
|   6418 | 2530 | `		if( SyisDigit(z[0]) && (iRc = DtTryIsoWeek(z,zEnd,&z,p,zIn)) != 0 ){` |
|     65 | 2531 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|     63 | 2532 | `			bAny = 1;` |
|     63 | 2533 | `			continue;` |
|      - | 2534 | `		}` |
|   6354 | 2535 | `		if( (iRc = DtTryIsoDate(z,zEnd,&z,p,zIn)) != 0 ){` |
|   2698 | 2536 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|   2680 | 2537 | `			bAny = 1;` |
|   2680 | 2538 | `			continue;` |
|      - | 2539 | `		}` |
|   3660 | 2540 | `		if( SyisDigit(z[0]) && (iRc = DtTryIsoOrdinalDot(z,zEnd,&z,p,zIn)) != 0 ){` |
|     29 | 2541 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|     29 | 2542 | `			bAny = 1;` |
|     29 | 2543 | `			continue;` |
|      - | 2544 | `		}` |
|   3632 | 2545 | `		if( SyisDigit(z[0]) && (iRc = DtTryNumericDate(z,zEnd,&z,p,zIn)) != 0 ){` |
|    197 | 2546 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|    189 | 2547 | `			bAny = 1;` |
|    189 | 2548 | `			continue;` |
|      - | 2549 | `		}` |
|      - | 2550 | `		/* ...and the same date with no YEAR, which is a shorter read than the` |
|      - | 2551 | `		 * three-field one above and so is tried after it. */` |
|   3436 | 2552 | `		if( SyisDigit(z[0]) && (iRc = DtTryAmericanShort(z,zEnd,&z,p,zIn)) != 0 ){` |
|     59 | 2553 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|     53 | 2554 | `			bAny = 1;` |
|     53 | 2555 | `			continue;` |
|      - | 2556 | `		}` |
|   3374 | 2557 | `		if( (SyisAlpha(z[0]) \|\| SyisDigit(z[0]))` |
|   2942 | 2558 | `		 && (iRc = DtTryMonthDate(z,zEnd,&z,p,zIn)) != 0 ){` |
|    285 | 2559 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|    261 | 2560 | `			bAny = 1;` |
|    261 | 2561 | `			continue;` |
|      - | 2562 | `		}` |
|      - | 2563 | `		/* A time of day ("next thursday 15:00", or one standing alone). */` |
|   3094 | 2564 | `		if( (iRc = DtReadTimeOfDay(&z,zEnd,zIn,p)) != 0 ){` |
|    379 | 2565 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|    357 | 2566 | `			bAny = 1;` |
|    357 | 2567 | `			continue;` |
|      - | 2568 | `		}` |
|   2716 | 2569 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|   1800 | 2570 | `			int neg = 0,bNoUnit = 0,nDig = 0;` |
|   1800 | 2571 | `			sxi64 v = 0;` |
|   1800 | 2572 | `			const char *zNumStart = z;` |
|      - | 2573 | `			const char *zDig;` |
|   1800 | 2574 | `			if( z[0]=='+' \|\| z[0]=='-' ){` |
|    762 | 2575 | `				neg = (z[0]=='-');` |
|    762 | 2576 | `				z++;` |
|      - | 2577 | `				/* php's lexer takes the sign as its own token, so whitespace may` |
|      - | 2578 | `				 * follow it: "1 year + 3 months" is a relative sequence there and` |
|      - | 2579 | `` 				 * was a parse FAILURE here. Its `space` alone, though: `+,3 days` `` |
|      - | 2580 | ``				 * and `+.3 days` are refusals there. */`` |
|    770 | 2581 | `				DT_SPACE();` |
|    379 | 2582 | `			}` |
|   1800 | 2583 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|     39 | 2584 | `				z = zNumStart;` |
|     39 | 2585 | `				DT_FAIL((int)(zNumStart - zIn) + 1);` |
|     39 | 2586 | `				continue;` |
|      - | 2587 | `			}` |
|   1762 | 2588 | `			zDig = z;` |
|   6900 | 2589 | `			while( z < zEnd && SyisDigit(z[0]) ){` |
|   5142 | 2590 | `				if( nDig < DT_DIGITS_SAFE ){ v = v*10 + (z[0]-'0'); }` |
|   5142 | 2591 | `				nDig++;` |
|   5142 | 2592 | `				z++;` |
|      4 | 2593 | `			}` |
|   1762 | 2594 | `			if( neg ){ v = -v; }` |
|   2566 | 2595 | `			DT_SPACE();` |
|      - | 2596 | `			/* php's unit words, sub-second ones first: they are all one rule (see` |
|      - | 2597 | `			 * DtRelUnit), and the microseconds accumulate apart from the seconds` |
|      - | 2598 | ``			 * so `-500 microseconds` from midnight borrows a whole second. */`` |
|      - | 2599 | `			{` |
|      - | 2600 | `				int nU,bSpec;` |
|   1762 | 2601 | `				nU = DtRelUnit(z,zEnd,v,p,&bSpec);` |
|   1762 | 2602 | `				if( nU > 0 ){ p->bHaveRel = 1; z += nU; }` |
|   1107 | 2603 | `				else{ bNoUnit = 1; }` |
|      - | 2604 | `			}` |
|   1762 | 2605 | `			if( bNoUnit ){` |
|      - | 2606 | ``				/* php's COUNTED weekday, `2 monday`: the count is whole WEEKS`` |
|      - | 2607 | `				 * past the first, the hunt is the bare name's (behaviour 1, so a` |
|      - | 2608 | `				 * base day that already matches counts), and -- unlike every` |
|      - | 2609 | `				 * spelling that reaches one through a WORD -- the clock is left` |
|      - | 2610 | `				 * standing. */` |
|   1107 | 2611 | `				int adv,dow = DtMatchWeekdayEx(z,zEnd,&adv,1);` |
|   1107 | 2612 | `				if( dow >= 0 && DtRelUnitLen(z,zEnd,p) > adv ){` |
|    ! 0 | 2613 | ``					dow = -1;   /* `3 months` is the unit, not `mon` and `ths` */`` |
|    ! 0 | 2614 | `				}` |
|   1107 | 2615 | `				if( dow >= 0 ){` |
|     59 | 2616 | `					p->bHaveRel = 1;` |
|     59 | 2617 | `					p->bWday = 1;` |
|     59 | 2618 | `					p->iWday = dow;` |
|     59 | 2619 | `					if( p->iWdayBehavior != 2 ){ p->iWdayBehavior = 1; }` |
|     59 | 2620 | `					p->rd = DtWAdd(p->rd,DtWMul(v > 0 ? v - 1 : v,7));` |
|     59 | 2621 | `					z += adv;` |
|     59 | 2622 | `					bNoUnit = 0;` |
|     29 | 2623 | `				}` |
|    552 | 2624 | `			}` |
|   1762 | 2625 | `			if( !bNoUnit ){` |
|      - | 2626 | `				/* php's own ceiling on a relative number, checked once the UNIT` |
|      - | 2627 | `				 * has claimed the run (the nocolon rules below have their own` |
|      - | 2628 | ``				 * widths, and a bare `20240102123456` is a date and a time). */`` |
|    715 | 2629 | `				if( nDig > DT_DIGITS_REL ){` |
|     19 | 2630 | `					DT_FAIL(-((int)(zDig - zIn) + 1) - DT_ERR_RANGE);` |
|     19 | 2631 | `					continue;` |
|      - | 2632 | `				}` |
|    697 | 2633 | `				bAny = 1;` |
|    697 | 2634 | `				continue;` |
|      - | 2635 | `			}` |
|      - | 2636 | `			/* No unit word: this is not a relative token at all. php's scanner` |
|      - | 2637 | `			 * would have taken a LONGER match, so rewind and let the nocolon` |
|      - | 2638 | `			 * rules below read the same digits as a date, a clock or a year. */` |
|   1049 | 2639 | `			z = zNumStart;` |
|    523 | 2640 | `		}` |
|      - | 2641 | `		/* php's NOCOLON spellings, a bare run of digits read by WIDTH: eight are a` |
|      - | 2642 | ``		 * date (`20240102`), six a time (`123456`), four a time (`1234`) -- or, if`` |
|      - | 2643 | `		 * the string already named a time, a YEAR, which is php's own dispatch and` |
|      - | 2644 | ``		 * what makes `12:00 1234` the year 1234 and `1234 12:00` a refusal. A run`` |
|      - | 2645 | ``		 * of four that is no valid clock is a year outright (`2500`), and a third`` |
|      - | 2646 | `		 * such run is php's "Double time specification".` |
|      - | 2647 | `		 *` |
|      - | 2648 | `		 * Only the longest reading counts, so an over-wide run leaves its tail to` |
|      - | 2649 | ``		 * the loop: `12345-01-01` is 12:34 on 2005-01-01, and `1234567` is a parse`` |
|      - | 2650 | `		 * failure at its last digit. */` |
|   1965 | 2651 | `		if( SyisDigit(z[0]) \|\| ((z[0]=='t' \|\| z[0]=='T') && z+1 < zEnd && SyisDigit(z[1])) ){` |
|    789 | 2652 | `			int bT = !SyisDigit(z[0]);` |
|    789 | 2653 | `			const char *zd = &z[bT];` |
|    789 | 2654 | `			int n = 0,bNoRun = 0,h,mi,se,mo,d,doy;` |
|   3539 | 2655 | `			while( zd+n < zEnd && SyisDigit(zd[n]) ){ n++; }` |
|      - | 2656 | `#define DTNUM2(k) ((zd[k]-'0')*10 + (zd[(k)+1]-'0'))` |
|    789 | 2657 | `			mo = (n >= 8) ? DTNUM2(4) : 99;` |
|    789 | 2658 | `			d  = (n >= 8) ? DTNUM2(6) : 99;` |
|    789 | 2659 | `			h  = (n >= 4) ? DTNUM2(0) : 99;` |
|    789 | 2660 | `			mi = (n >= 4) ? DTNUM2(2) : 99;` |
|    789 | 2661 | `			se = (n >= 6) ? DTNUM2(4) : 99;` |
|    789 | 2662 | `			doy = (n >= 7) ? (zd[4]-'0')*100 + DTNUM2(5) : 0;` |
|    789 | 2663 | `			if( !bT && n == 4 ){` |
|      - | 2664 | ``				/* php's REVERSE no-day date, `2020 Jan`: a four-digit year and a`` |
|      - | 2665 | `				 * month name are one token there, and a longer one than the clock` |
|      - | 2666 | `` 				 * reading of those same four digits -- which is why `2020 Jan 15` `` |
|      - | 2667 | `				 * is a parse failure and not a time. */` |
|    145 | 2668 | `				const char *zm = &zd[4];` |
|      - | 2669 | `				int adv,mo2;` |
|    283 | 2670 | `				while( zm < zEnd && (zm[0]==' '\|\|zm[0]=='\t'\|\|zm[0]=='.'\|\|zm[0]=='-') ){ zm++; }` |
|    145 | 2671 | `				if( (mo2 = DtMatchMonth(zm,zEnd,&adv)) != 0 ){` |
|      9 | 2672 | `					int rcD = DtMarkDate(p,z,zIn);` |
|     13 | 2673 | `					if( rcD != 0 ){ z = &zm[adv]; DT_FAIL(rcD); continue; }` |
|      9 | 2674 | `					p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));` |
|      9 | 2675 | `					p->m = mo2;` |
|      9 | 2676 | `					p->d = 1;` |
|      9 | 2677 | `					z = &zm[adv];` |
|      9 | 2678 | `					bAny = 1;` |
|      9 | 2679 | `					continue;` |
|      - | 2680 | `				}` |
|     68 | 2681 | `			}` |
|    781 | 2682 | `			if( !bT && n >= 8 && mo <= 12 && d <= 31 ){` |
|     35 | 2683 | `				int rcD = DtMarkDate(p,z,zIn);` |
|     35 | 2684 | `				if( rcD != 0 ){ z = &zd[8]; DT_FAIL(rcD); continue; }` |
|     27 | 2685 | `				p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));` |
|     27 | 2686 | `				p->m = mo;` |
|     27 | 2687 | `				p->d = d;` |
|     27 | 2688 | `				z = &zd[8];` |
|    760 | 2689 | `			}else if( !bT && n >= 7 && doy >= 1 && doy <= 366 ){` |
|      - | 2690 | `				/* php's ISO ORDINAL date, YYYYDDD: the day of the year, which the` |
|      - | 2691 | `				 * field normalizer resolves out of January. */` |
|      7 | 2692 | `				int rcD = DtMarkDate(p,z,zIn);` |
|      7 | 2693 | `				if( rcD != 0 ){ z = &zd[7]; DT_FAIL(rcD); continue; }` |
|      7 | 2694 | `				p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));` |
|      7 | 2695 | `				p->m = 1;` |
|      7 | 2696 | `				p->d = doy;` |
|      7 | 2697 | `				z = &zd[7];` |
|    744 | 2698 | `			}else if( n >= 6 && h <= 24 && mi <= 59 && se <= 60 ){` |
|      - | 2699 | `				/* six digits are php's whole clock, and a SECOND clock is its` |
|      - | 2700 | `				 * refusal rather than the year the four-digit run falls back to */` |
|     19 | 2701 | `				if( p->nTimeTok ){` |
|    ! 0 | 2702 | `					iRc = -((int)(z - zIn) + 1);` |
|    ! 0 | 2703 | `					z = &zd[6];` |
|    ! 0 | 2704 | `					DT_FAIL(iRc);` |
|    ! 0 | 2705 | `					continue;` |
|      - | 2706 | `				}` |
|     19 | 2707 | `				p->h = h; p->i = mi; p->s = se; p->us = 0;` |
|     19 | 2708 | `				p->bUsUnset = 0;` |
|     19 | 2709 | `				p->nTimeTok = 1;` |
|     19 | 2710 | `				z = &zd[6];` |
|    732 | 2711 | `			}else if( n >= 4 && h <= 24 && mi <= 59 ){` |
|    113 | 2712 | `				if( p->nTimeTok >= 2 ){` |
|      3 | 2713 | `					iRc = -((int)(z - zIn) + 1);` |
|      3 | 2714 | `					z = &zd[4];` |
|      3 | 2715 | `					DT_FAIL(iRc);` |
|      3 | 2716 | `					continue;` |
|      - | 2717 | `				}` |
|    111 | 2718 | `				if( p->nTimeTok == 1 ){` |
|     11 | 2719 | `					p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));` |
|      6 | 2720 | `				}else{` |
|      - | 2721 | ``					/* php's `gnunocolon` writes the fields itself rather than`` |
|      - | 2722 | `					 * through TIMELIB_HAVE_TIME, so the sub-second one is never` |
|      - | 2723 | ``					 * touched: `date_parse('1234')` shows no fraction at all,`` |
|      - | 2724 | ``					 * while `12:00 1234` keeps the zero its clock wrote. */`` |
|    101 | 2725 | `					p->bUsUnset = (p->us == DT_UNSET);` |
|    101 | 2726 | `					p->h = h; p->i = mi; p->s = 0; p->us = 0;` |
|      - | 2727 | `				}` |
|    111 | 2728 | `				p->nTimeTok++;` |
|    111 | 2729 | `				z = &zd[4];` |
|    666 | 2730 | `			}else if( !bT && n >= 4 ){` |
|      - | 2731 | `				/* php's bare year4, which does NOT count as a date: the month, the` |
|      - | 2732 | `				 * day and the clock all stay the base moment's -- the MICROSECONDS` |
|      - | 2733 | `				 * excepted. The run reached this branch through php's have_time` |
|      - | 2734 | `				 * bookkeeping, which zeroes the sub-second field on the way past,` |
|      - | 2735 | ``				 * so `new DateTime('7609')` is the current time of day on that`` |
|      - | 2736 | `				 * year with nothing under the second. */` |
|    113 | 2737 | `				p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));` |
|    113 | 2738 | ``				p->bUsUnset = (p->us == DT_UNSET);   /* php's `year4` writes the`` |
|      - | 2739 | `					* year and nothing else */` |
|    113 | 2740 | `				p->us = 0;` |
|    113 | 2741 | `				z = &zd[4];` |
|    555 | 2742 | `			}else if( bT ){` |
|      - | 2743 | ``				/* php's `t` + an HOUR alone: `t9` is 09:00 where a bare `9` is`` |
|      - | 2744 | `				 * nothing at all, and the hour is read the same greedy way as the` |
|      - | 2745 | ``				 * one before a colon (`t95846` is 09:00 and the year 5846). */`` |
|     23 | 2746 | `				int nh = DtReadField(zd,zEnd,24,&h);` |
|     23 | 2747 | `				if( p->nTimeTok ){` |
|      3 | 2748 | `					iRc = -((int)(z - zIn) + 1);` |
|      3 | 2749 | `					z = &zd[nh];` |
|      3 | 2750 | `					DT_FAIL(iRc);` |
|      3 | 2751 | `					continue;` |
|      - | 2752 | `				}` |
|     21 | 2753 | `				p->h = h;` |
|     21 | 2754 | `				p->i = p->s = p->us = 0;` |
|     21 | 2755 | `				p->bUsUnset = 0;` |
|     21 | 2756 | `				p->nTimeTok = 1;` |
|     21 | 2757 | `				z = &zd[nh];` |
|     11 | 2758 | `			}else{` |
|    477 | 2759 | `				bNoRun = 1;   /* no nocolon rule claims it; z is untouched */` |
|      - | 2760 | `			}` |
|    769 | 2761 | `			if( !bNoRun ){` |
|    293 | 2762 | `				bAny = 1;` |
|    293 | 2763 | `				continue;` |
|      - | 2764 | `			}` |
|      - | 2765 | `#undef DTNUM2` |
|    238 | 2766 | `		}` |
|      - | 2767 | `		/* php's TIMEZONE token stands anywhere in a string and is a token in its` |
|      - | 2768 | ``		 * own right: `2020-01-01 12:00 +0200` (its own serialization spelling, and`` |
|      - | 2769 | ``		 * every RFC-2822 date there is), `12:00 UTC`, `1234z`, and `UTC` alone --`` |
|      - | 2770 | `		 * none of which parsed here at all. It goes LAST because php's scanner` |
|      - | 2771 | ``		 * takes the longest reading: `+1 day` is a relative and `t9` a clock, and`` |
|      - | 2772 | `		 * both would otherwise be read as a zone. */` |
|   1653 | 2773 | `		if( (iRc = DtZoneTok(&z,zEnd,p,zIn)) != 0 ){` |
|   1079 | 2774 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|    727 | 2775 | `			bAny = 1;` |
|    727 | 2776 | `			continue;` |
|      - | 2777 | `		}` |
|    576 | 2778 | `		DT_FAIL((int)(z - zIn) + 1);` |
|      2 | 2779 | `	}` |
|      - | 2780 | `	/* The warnings a RULE raised, in the order the scan met them ... */` |
|   6292 | 2781 | `	for( k = 0 ; k < p->nWarnPend && k < PH7_DT_MAX_WARN ; k++ ){` |
|    156 | 2782 | `		DtRecWarn(pRec,p->aWarnPos[k],p->azWarn[k]);` |
|     79 | 2783 | `	}` |
|      - | 2784 | `	/*` |
|      - | 2785 | `	 * ...and the two php raises once the scan is over, both at the byte one past` |
|      - | 2786 | `	 * the string. They are about what the string SPELLED rather than what any` |
|      - | 2787 | ``	 * rule refused, so a date nobody could hold (`2020-02-31`, `2020-102`) and a`` |
|      - | 2788 | ``	 * clock nobody could show (`24:00:00`, `23:59:60`) are warnings on a parse`` |
|      - | 2789 | `	 * that otherwise succeeds. php asks the time first, and both land on the same` |
|      - | 2790 | `	 * key -- so a string with both counts two and shows the date's.` |
|      - | 2791 | `	 */` |
|   6134 | 2792 | `	if( p->nTimeTok` |
|   4479 | 2793 | `	 && (p->h > 23 \|\| p->i > 59 \|\| p->s > 59 \|\| p->h < 0 \|\| p->i < 0 \|\| p->s < 0) ){` |
|     23 | 2794 | `		DtRecWarn(pRec,nLen + 1,"The parsed time was invalid");` |
|     11 | 2795 | `	}` |
|   6134 | 2796 | `	if( p->bHaveDate` |
|   4723 | 2797 | `	 && (p->m < 1 \|\| p->m > 12 \|\| p->d < 1` |
|   3242 | 2798 | `	     \|\| p->d > DtDaysInMonth(p->y,(int)p->m)) ){` |
|    167 | 2799 | `		DtRecWarn(pRec,nLen + 1,"The parsed date was invalid");` |
|     83 | 2800 | `	}` |
|   6138 | 2801 | `	if( iFirst != 0 ){` |
|    762 | 2802 | `		return iFirst;` |
|      - | 2803 | `	}` |
|   5378 | 2804 | `	if( !bAny ){` |
|    ! 0 | 2805 | `		return 1;` |
|      - | 2806 | `	}` |
|   5378 | 2807 | `	return 0;` |
|      - | 2808 | `#undef DT_SKIP_WS` |
|      - | 2809 | `#undef DT_LOWEQ` |
|      - | 2810 | `#undef DT_WEEKSING` |
|      - | 2811 | `#undef DT_FAIL` |
|   3073 | 2812 | `}` |
|      - | 2813 | `/*` |
|      - | 2814 | ` * Parse zIn against the base moment and answer the timestamp it names. The` |
|      - | 2815 | ` * vector the string filled is applied here (DtApplyFields), so nothing about the` |
|      - | 2816 | ` * order the string spelled its units in reaches the clock.` |
|      - | 2817 | ` */` |
|   6138 | 2818 | `static int DtParseEx(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,int iBaseUs,` |
|      - | 2819 | `	int iFlags,sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs,dt_parsed *pVec,` |
|      - | 2820 | `	phl_dt_lasterr *pRec)` |
|      4 | 2821 | `{` |
|      - | 2822 | `	dt_parsed sP;` |
|      - | 2823 | `	int iErr,iTz;` |
|   6142 | 2824 | `	DtFieldsInit(&sP,iBaseOff);` |
|   6142 | 2825 | `	*pUs = iBaseUs;` |
|   6142 | 2826 | `	if( pRec ){` |
|   5214 | 2827 | `		DtRecReset(pRec);` |
|   2605 | 2828 | `	}` |
|   6142 | 2829 | `	iErr = DtParseFields(zIn,nLen,&sP,pRec);` |
|   6142 | 2830 | `	if( pVec ){` |
|   5400 | 2831 | `		*pVec = sP;` |
|   2698 | 2832 | `	}` |
|   6142 | 2833 | `	if( iErr != 0 ){` |
|    764 | 2834 | `		return iErr;` |
|      - | 2835 | `	}` |
|      - | 2836 | `	/* A DATABASE zone the string named governs the reading twice over. It is the` |
|      - | 2837 | ``	 * clock the base moment is read on -- `America/New_York` alone is now in New`` |
|      - | 2838 | ``	 * York whatever the default zone is, where the fixed-offset `EST` is the`` |
|      - | 2839 | `	 * DEFAULT zone's wall clock wearing a different label -- and it is the zone` |
|      - | 2840 | `	 * the finished reading is an instant in, which is a different offset from the` |
|      - | 2841 | `	 * one at NOW for every date on the other side of a switch.` |
|      - | 2842 | `	 *` |
|      - | 2843 | `	 * modify() is excepted because it discards the zone it parses altogether` |
|      - | 2844 | ``	 * (DT_PARSE_KEEP_ZONE), and `@epoch` because an epoch is already an instant. */`` |
|   2880 | 2845 | `	iTz = (sP.bOffSet == 2 && sP.bZoneIdent && !sP.bEpoch` |
|    118 | 2846 | `	       && !(iFlags & DT_PARSE_KEEP_ZONE))` |
|   2915 | 2847 | `		? DtTzIndex(sP.zZone,sP.nZone,DT_ZONE_ID) : -1;` |
|   5380 | 2848 | `	if( iTz >= 0 ){` |
|     37 | 2849 | `		int bDst = 0,nAbbr = 0;` |
|     37 | 2850 | `		const char *zAbbr = 0;` |
|     37 | 2851 | `		iBaseOff = DtTzOffsetAt(iTz,0,iBaseTs,&bDst,&zAbbr,&nAbbr);` |
|     37 | 2852 | `		sP.iOff = iBaseOff;` |
|     18 | 2853 | `	}` |
|   5380 | 2854 | `	*pTs = DtApplyFields(&sP,iBaseTs,iBaseOff,iBaseUs,iFlags,pUs);` |
|      - | 2855 | `#ifdef PH7_ENABLE_TZDB` |
|   5380 | 2856 | `	if( iTz >= 0 ){` |
|     37 | 2857 | `		sxi64 iFixed = *pTs;` |
|     37 | 2858 | `		sxi32 iOffAt = sP.iOff;` |
|     37 | 2859 | `		if( PH7_TzLocalToUtcFirst(iTz,*pTs + sP.iOff,&iFixed,&iOffAt) ){` |
|     37 | 2860 | `			*pTs = iFixed;` |
|     37 | 2861 | `			sP.iOff = iOffAt;` |
|     18 | 2862 | `		}` |
|     18 | 2863 | `	}` |
|      - | 2864 | `#endif` |
|   5380 | 2865 | `	*pOff = sP.iOff;` |
|   5380 | 2866 | `	*pbOffSet = sP.bOffSet;` |
|   5380 | 2867 | `	return 0;` |
|   3073 | 2868 | `}` |
|    742 | 2869 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,int iBaseUs,` |
|      - | 2870 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs)` |
|      3 | 2871 | `{` |
|    745 | 2872 | `	return DtParseEx(zIn,nLen,iBaseTs,iBaseOff,iBaseUs,0,pTs,pOff,pbOffSet,pUs,0,0);` |
|      3 | 2873 | `}` |
|      - | 2874 | `/*` |
|      - | 2875 | ` * php's parse-failure reason, from DtParse's error code.` |
|      - | 2876 | ` *` |
|      - | 2877 | ` * The reason and the offending byte used to be formatted straight into the message` |
|      - | 2878 | `` * the `__dt_parse` thunk RETURNED as a string; the constructor also has to publish`` |
|      - | 2879 | ` * them as getLastErrors()'s error map now, so the decision lives here.` |
|      - | 2880 | ` */` |
|   1730 | 2881 | `static const char * DtParseErr(const char *zIn,int nLen,int iErrPos,int *piPos,char *pcAt)` |
|      2 | 2882 | `{` |
|      - | 2883 | `	/* Negative encodings: php's "Double time specification" reason, and -- one` |
|      - | 2884 | `	 * whole DT_ERR_RANGE band lower -- its "Number out of range", which is what a` |
|      - | 2885 | `	 * digit run too wide for the clock reports; then "Double date specification"` |
|      - | 2886 | `	 * and, lowest, "Double timezone specification". */` |
|   1732 | 2887 | `	int bRange = 0,bDDate = 0,bDZone = 0,bTzId = 0,bUnexp = 0,bEmpty = 0;` |
|      - | 2888 | `	int bDouble;` |
|      - | 2889 | `	int iPos;` |
|   1732 | 2890 | `	DtTrimEnds(&zIn,&nLen);   /* the position is php's, i.e. the trimmed string's */` |
|   1732 | 2891 | `	if( iErrPos < -DT_ERR_EMPTY ){` |
|    ! 0 | 2892 | `		bEmpty = 1;` |
|    ! 0 | 2893 | `		iErrPos += DT_ERR_EMPTY;` |
|   1732 | 2894 | `	}else if( iErrPos < -DT_ERR_UNEXPDATA ){` |
|     17 | 2895 | `		bUnexp = 1;` |
|     17 | 2896 | `		iErrPos += DT_ERR_UNEXPDATA;` |
|   1724 | 2897 | `	}else if( iErrPos < -DT_ERR_TZID ){` |
|    530 | 2898 | `		bTzId = 1;` |
|    530 | 2899 | `		iErrPos += DT_ERR_TZID;` |
|   1452 | 2900 | `	}else if( iErrPos < -DT_ERR_DZONE ){` |
|     98 | 2901 | `		bDZone = 1;` |
|     98 | 2902 | `		iErrPos += DT_ERR_DZONE;` |
|   1140 | 2903 | `	}else if( iErrPos < -DT_ERR_DDATE ){` |
|     91 | 2904 | `		bDDate = 1;` |
|     91 | 2905 | `		iErrPos += DT_ERR_DDATE;` |
|   1047 | 2906 | `	}else if( iErrPos < -DT_ERR_RANGE ){` |
|     68 | 2907 | `		bRange = 1;` |
|     68 | 2908 | `		iErrPos += DT_ERR_RANGE;` |
|     33 | 2909 | `	}` |
|   2166 | 2910 | `	bDouble = !bRange && !bDDate && !bDZone && !bTzId && !bUnexp && !bEmpty` |
|   2562 | 2911 | `		&& iErrPos < 0;` |
|   1732 | 2912 | `	iPos = (iErrPos < 0 ? -iErrPos : iErrPos) - 1;` |
|   1732 | 2913 | `	char cAt = (iPos < nLen) ? zIn[iPos] : ' ';` |
|   1732 | 2914 | `	*piPos = iPos;` |
|   1732 | 2915 | `	*pcAt = cAt;` |
|      - | 2916 | `	/* php appends a reason: an alphabetic token is assumed to be a timezone` |
|      - | 2917 | `	 * lookup miss, anything else an unexpected character. */` |
|   1732 | 2918 | `	if( bRange ){` |
|     68 | 2919 | `		return "Number out of range";` |
|      - | 2920 | `	}` |
|   1666 | 2921 | `	if( bDDate ){` |
|     91 | 2922 | `		return "Double date specification";` |
|      - | 2923 | `	}` |
|   1576 | 2924 | `	if( bDZone ){` |
|     98 | 2925 | `		return "Double timezone specification";` |
|      - | 2926 | `	}` |
|   1480 | 2927 | `	if( bTzId ){` |
|    530 | 2928 | `		return "The timezone could not be found in the database";` |
|      - | 2929 | `	}` |
|    952 | 2930 | `	if( bUnexp ){` |
|     17 | 2931 | `		return "Found unexpected data";` |
|      - | 2932 | `	}` |
|    936 | 2933 | `	if( bEmpty ){` |
|    ! 0 | 2934 | `		return "Empty string";` |
|      - | 2935 | `	}` |
|      - | 2936 | ``	/* php's own parenthesized-zone token starts at the `(`, so a name it cannot`` |
|      - | 2937 | `	 * find there is reported at the paren with the zone reason, not the byte. */` |
|    936 | 2938 | `	if( cAt == '(' && iPos + 1 < nLen && SyisAlpha(zIn[iPos+1]) ){` |
|    ! 0 | 2939 | `		return "The timezone could not be found in the database";` |
|      - | 2940 | `	}` |
|    917 | 2941 | `	return bDouble ? "Double time specification"` |
|    915 | 2942 | `		: ((cAt >= 'a' && cAt <= 'z') \|\| (cAt >= 'A' && cAt <= 'Z'))` |
|      - | 2943 | `			? "The timezone could not be found in the database"` |
|      - | 2944 | `			: "Unexpected character";` |
|    867 | 2945 | `}` |
|      - | 2946 | `/* Days in a civil month (php's overflow rules use it during diff borrows) */` |
|  12802 | 2947 | `static int DtDaysInMonth(sxi64 y,int m)` |
|      4 | 2948 | `{` |
|      - | 2949 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|  12806 | 2950 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|    295 | 2951 | `		return 29;` |
|      - | 2952 | `	}` |
|  12512 | 2953 | `	return aMonDays[(m - 1) % 12];` |
|   6402 | 2954 | `}` |
|      - | 2955 | `/*` |
|      - | 2956 | ` * php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|      - | 2957 | ` * (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset.` |
|      - | 2958 | ` */` |
|    398 | 2959 | `static sxi64 DtCivilAdd(sxi64 iTs,sxi32 iOff,sxi64 y,sxi64 m,sxi64 d,` |
|      - | 2960 | `	sxi64 h,sxi64 i,sxi64 s,int iSign)` |
|      1 | 2961 | `{` |
|      - | 2962 | `	sxi64 iLocal,iDays,iSecs,y0,moT,dayCount;` |
|      - | 2963 | `	int mo0,d0;` |
|    399 | 2964 | `	iSign = iSign < 0 ? -1 : 1;` |
|    399 | 2965 | `	iLocal = iTs + iOff;` |
|    399 | 2966 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    399 | 2967 | `	iSecs  = iLocal - iDays*86400;` |
|    399 | 2968 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    399 | 2969 | `	y0 += iSign * y;` |
|    399 | 2970 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    399 | 2971 | `	y0 += DtFloorDiv(moT,12);` |
|    399 | 2972 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    399 | 2973 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    399 | 2974 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    399 | 2975 | `	return iLocal - iOff;` |
|      1 | 2976 | `}` |
|      - | 2977 | `/* One DateInterval's worth of fields, as diff() computes them. */` |
|      - | 2978 | `typedef struct dt_diff dt_diff;` |
|      - | 2979 | `struct dt_diff` |
|      - | 2980 | `{` |
|      - | 2981 | `	sxi64 y,m,d,h,i,s,uSec,nDays;` |
|      - | 2982 | `	int bInvert;` |
|      - | 2983 | `};` |
|      - | 2984 | `/*` |
|      - | 2985 | ` * timelib's diff breakdown: field-wise deltas in the FIRST operand's offset, then` |
|      - | 2986 | ` * borrow seconds->minutes->hours->days, then borrow whole months for the day.` |
|      - | 2987 | ` *` |
|      - | 2988 | ` * That last borrow is ASYMMETRIC in php, and PHL answered the symmetric result: a` |
|      - | 2989 | ` * non-inverted diff walks months BACKWARD from the later date (which is why` |
|      - | 2990 | ` * Jan 31 -> Mar 02 reports m=0 d=30, not "1 month"), while an inverted one borrows` |
|      - | 2991 | ` * the month of the ORIGINAL first operand — the later date — walking forward. So` |
|      - | 2992 | `` * `$later->diff($earlier)` is not `$earlier->diff($later)` with the sign flipped:`` |
|      - | 2993 | ` * php answers y=1 m=1 d=2 where PHL answered y=1 m=0 d=30. One iteration always` |
|      - | 2994 | ` * settles the inverted case: \|d\| < 31 and the borrowed month has at least 28 days,` |
|      - | 2995 | ` * while a 28-day base month can only be reached from a day-of-month <= 29.` |
|      - | 2996 | ` */` |
|    178 | 2997 | `static void DtCivilDiff(sxi64 iTs1,int uSec1,sxi32 iOff1,sxi64 iTs2,int uSec2,sxi32 iOff2,` |
|      - | 2998 | `	int bSameZone,dt_diff *pOut)` |
|      1 | 2999 | `{` |
|      - | 3000 | `	sxi64 iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|      - | 3001 | `	sxi32 iOffA,iOffB;` |
|      - | 3002 | `	int moA,dA,moB,dB,bInvert,usA,usB;` |
|      - | 3003 | `	sxi64 sA,sB,y,m,d,h,i,s,us;` |
|      - | 3004 | ``	/* The MICROSECONDS are part of which date comes first -- `$a->diff($b)` on two`` |
|      - | 3005 | `	 * dates inside the same second is an INVERTED interval when $a is the later of` |
|      - | 3006 | `	 * them -- and their borrow is a whole second off the later date, so every field` |
|      - | 3007 | `	 * below and the day COUNT are computed from the borrowed instant: a difference` |
|      - | 3008 | `	 * of one microsecond less than a day is 23:59:59.999999 with days = 0, not a` |
|      - | 3009 | `	 * day. */` |
|    179 | 3010 | `	bInvert = iTs1 > iTs2 \|\| (iTs1 == iTs2 && uSec1 > uSec2);` |
|    179 | 3011 | `	iA = bInvert ? iTs2 : iTs1;` |
|    179 | 3012 | `	iB = bInvert ? iTs1 : iTs2;` |
|    179 | 3013 | `	usA = bInvert ? uSec2 : uSec1;` |
|    179 | 3014 | `	usB = bInvert ? uSec1 : uSec2;` |
|    179 | 3015 | `	us = usB - usA;` |
|    179 | 3016 | `	if( us < 0 ){` |
|     21 | 3017 | `		us += 1000000;` |
|     21 | 3018 | `		iB--;` |
|     10 | 3019 | `	}` |
|      - | 3020 | `	/*` |
|      - | 3021 | `	 * WHICH CLOCK the two instants are read on, and php has two answers.` |
|      - | 3022 | `	 *` |
|      - | 3023 | `	 * When both dates are in the SAME zone -- both identifiers, spelled the same` |
|      - | 3024 | ``	 * bytes -- each is read on its own, so `2010-01-01 00:00` and`` |
|      - | 3025 | ``	 * `2010-08-01 00:00` in New York are seven months apart exactly, with the`` |
|      - | 3026 | `	 * hour daylight saving took not in the answer at all.` |
|      - | 3027 | `	 *` |
|      - | 3028 | `	 * Otherwise both are read on the EARLIER one's offset, which is timelib` |
|      - | 3029 | ``	 * subtracting `two->z - one->z` from a field-wise difference and comes to`` |
|      - | 3030 | `	 * the same thing. So the same pair with the second date spelled` |
|      - | 3031 | ``	 * `america/new_york` -- the same place, a different spelling, and to php a`` |
|      - | 3032 | `	 * different zone -- is seven months LESS AN HOUR. The comparison really is` |
|      - | 3033 | ``	 * byte-exact: `US/Eastern` is the same data and not the same zone either.`` |
|      - | 3034 | `	 *` |
|      - | 3035 | `	 * With no database in the build no zone's offset can vary, so the two arms` |
|      - | 3036 | `	 * agree and this is the single offset the code always applied.` |
|      - | 3037 | `	 */` |
|    179 | 3038 | `	iOffA = bInvert ? iOff2 : iOff1;` |
|    179 | 3039 | `	iOffB = bInvert ? iOff1 : iOff2;` |
|    179 | 3040 | `	iLa = iA + iOffA;` |
|    179 | 3041 | `	iLb = iB + (bSameZone ? iOffB : iOffA);` |
|    179 | 3042 | `	daysA = DtFloorDiv(iLa,86400);` |
|    179 | 3043 | `	daysB = DtFloorDiv(iLb,86400);` |
|    179 | 3044 | `	sA = iLa - daysA*86400;` |
|    179 | 3045 | `	sB = iLb - daysB*86400;` |
|    179 | 3046 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|    179 | 3047 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|    179 | 3048 | `	s = (sB % 60) - (sA % 60);` |
|    179 | 3049 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|    179 | 3050 | `	h = (sB / 3600) - (sA / 3600);` |
|    179 | 3051 | `	d = dB - dA;` |
|    179 | 3052 | `	m = moB - moA;` |
|    179 | 3053 | `	y = yB - yA;` |
|    179 | 3054 | `	if( s < 0 ){ s += 60; i--; }` |
|    179 | 3055 | `	if( i < 0 ){ i += 60; h--; }` |
|    179 | 3056 | `	if( h < 0 ){ h += 24; d--; }` |
|    179 | 3057 | `	if( bInvert ){` |
|     91 | 3058 | `		while( d < 0 ){` |
|     11 | 3059 | `			d += DtDaysInMonth(yA,moA);` |
|     11 | 3060 | `			m--;` |
|     11 | 3061 | `			moA++;` |
|     11 | 3062 | `			if( moA > 12 ){ moA = 1; yA++; }` |
|      1 | 3063 | `		}` |
|     41 | 3064 | `	}else{` |
|    113 | 3065 | `		while( d < 0 ){` |
|     15 | 3066 | `			moB--;` |
|     15 | 3067 | `			if( moB < 1 ){ moB = 12; yB--; }` |
|     15 | 3068 | `			d += DtDaysInMonth(yB,moB);` |
|     15 | 3069 | `			m--;` |
|      1 | 3070 | `		}` |
|      - | 3071 | `	}` |
|    179 | 3072 | `	if( m < 0 ){ m += 12; y--; }` |
|    179 | 3073 | `	pOut->y = y;` |
|    179 | 3074 | `	pOut->m = m;` |
|    179 | 3075 | `	pOut->d = d;` |
|    179 | 3076 | `	pOut->h = h;` |
|    179 | 3077 | `	pOut->i = i;` |
|    179 | 3078 | `	pOut->s = s;` |
|    179 | 3079 | `	pOut->uSec = us;` |
|      - | 3080 | `	/* The day COUNT rides the same clock: two local noons a daylight switch` |
|      - | 3081 | `	 * apart are one day, not a day less an hour rounded down to zero. */` |
|    179 | 3082 | `	pOut->nDays = (iLb - iLa) / 86400;` |
|    179 | 3083 | `	pOut->bInvert = bInvert;` |
|    179 | 3084 | `}` |
|      - | 3085 | `/*` |
|      - | 3086 | ` * setISODate: jump to an ISO year/week/weekday, preserving the time of day.` |
|      - | 3087 | ` */` |
|     26 | 3088 | `static sxi64 DtIsoDate(sxi64 iTs,sxi32 iOff,sxi64 y,sxi64 w,sxi64 dow)` |
|      1 | 3089 | `{` |
|      - | 3090 | `	sxi64 iLocal,iTod,jan4,monday1,target;` |
|      - | 3091 | `	int isoDow;` |
|     27 | 3092 | `	iLocal = iTs + iOff;` |
|     27 | 3093 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|     27 | 3094 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|     27 | 3095 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|     27 | 3096 | `	monday1 = jan4 - (isoDow - 1);` |
|     27 | 3097 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|     27 | 3098 | `	return target*86400 + iTod - iOff;` |
|      1 | 3099 | `}` |
|      - | 3100 | `/*` |
|      - | 3101 | ` * php's DateTime::createFromFormat engine.` |
|      - | 3102 | ` *` |
|      - | 3103 | `` * This was the `__dt_from_format()` thunk, whose answer had to survive a trip`` |
|      - | 3104 | ` * through PHP: an ARRAY on success and a "COUNT\nPOS\tMESSAGE" string on failure,` |
|      - | 3105 | ` * which the chunk then re-parsed. Both encodings are gone — the native methods call` |
|      - | 3106 | ` * this directly and read the diagnostics as a struct. That is also why a parse that` |
|      - | 3107 | ` * has BOTH errors and warnings can now report both: the failure encoding had no room` |
|      - | 3108 | `` * for warnings, so php's `warning_count` was silently 0 whenever an error was present.`` |
|      - | 3109 | ` *` |
|      - | 3110 | ` * Returns 0 when the parse produced a time and non-zero when it did not; pOut->sDiag` |
|      - | 3111 | ` * carries the warnings/errors either way (offKind: 0 none parsed, 1 numeric offset,` |
|      - | 3112 | ` * 2 literal Z, 3 named identifier).` |
|      - | 3113 | ` */` |
|      - | 3114 | `/*` |
|      - | 3115 | ` * Publish one scan's warnings and errors as the record getLastErrors() answers.` |
|      - | 3116 | ` * The messages are static literals, so the record copies pointers, never bytes.` |
|      - | 3117 | ` */` |
|      - | 3118 | `/*` |
|      - | 3119 | ` * Record one parse's diagnostics as getLastErrors()'s answer.` |
|      - | 3120 | ` *` |
|      - | 3121 | ` * php resets the record on EVERY constructor, modify() and createFromFormat()` |
|      - | 3122 | `` * call -- a clean parse answers `false` again -- and publishes what the scan`` |
|      - | 3123 | ` * collected, which is every error it met rather than the one it stopped on.` |
|      - | 3124 | ` * The error rows grow with the string (one per byte at worst), the warnings` |
|      - | 3125 | ` * cannot exceed their own three rules, and every message is a static literal.` |
|      - | 3126 | ` */` |
|  13777 | 3127 | `static void DtRecReset(phl_dt_lasterr *pRec)` |
|      5 | 3128 | `{` |
|  13782 | 3129 | `	pRec->bSet = 0;` |
|  13782 | 3130 | `	pRec->nWarn = pRec->nWarnKept = 0;` |
|  13782 | 3131 | `	pRec->nErr = pRec->nErrKept = 0;` |
|  13782 | 3132 | `	SyBlobReset(&pRec->sErr);` |
|  13782 | 3133 | `}` |
|   1346 | 3134 | `static void DtRecErr(phl_dt_lasterr *pRec,int iPos,const char *zMsg)` |
|      2 | 3135 | `{` |
|      - | 3136 | `	phl_dt_diag_row sRow;` |
|   1348 | 3137 | `	if( pRec == 0 ){` |
|    212 | 3138 | `		return;` |
|      - | 3139 | `	}` |
|   1138 | 3140 | `	pRec->bSet = 1;` |
|   1138 | 3141 | `	pRec->nErr++;` |
|   1138 | 3142 | `	sRow.iPos = iPos;` |
|   1138 | 3143 | `	sRow.zMsg = zMsg;` |
|   1138 | 3144 | `	if( SyBlobAppend(&pRec->sErr,(const void *)&sRow,sizeof(sRow)) == SXRET_OK ){` |
|   1138 | 3145 | `		pRec->nErrKept++;` |
|    568 | 3146 | `	}` |
|    675 | 3147 | `}` |
|    394 | 3148 | `static void DtRecWarn(phl_dt_lasterr *pRec,int iPos,const char *zMsg)` |
|      2 | 3149 | `{` |
|    396 | 3150 | `	if( pRec == 0 ){` |
|     75 | 3151 | `		return;` |
|      - | 3152 | `	}` |
|    322 | 3153 | `	pRec->bSet = 1;` |
|    322 | 3154 | `	pRec->nWarn++;` |
|    322 | 3155 | `	if( pRec->nWarnKept < PH7_DT_MAX_WARN ){` |
|    322 | 3156 | `		pRec->aWarnPos[pRec->nWarnKept] = iPos;` |
|    322 | 3157 | `		pRec->azWarn[pRec->nWarnKept] = zMsg;` |
|    322 | 3158 | `		pRec->nWarnKept++;` |
|    160 | 3159 | `	}` |
|    199 | 3160 | `}` |
|   7925 | 3161 | `static void DtLastErrClear(ph7_vm *pVm)` |
|      5 | 3162 | `{` |
|   7930 | 3163 | `	DtRecReset(&pVm->sDtLastErr);` |
|   7930 | 3164 | `}` |
|      - | 3165 | `typedef struct dt_ff_diag dt_ff_diag;` |
|      - | 3166 | `struct dt_ff_diag` |
|      - | 3167 | `{` |
|      - | 3168 | `	int nErr,nErrKept;` |
|      - | 3169 | `	int aErrPos[PH7_DT_MAX_ERR];` |
|      - | 3170 | `	const char *azErr[PH7_DT_MAX_ERR];` |
|      - | 3171 | `	int nWarn;` |
|      - | 3172 | `	int aWarnPos[PH7_DT_MAX_WARN];` |
|      - | 3173 | `	const char *azWarn[PH7_DT_MAX_WARN];` |
|      - | 3174 | `};` |
|    642 | 3175 | `static void DtFfDiag(dt_ff_diag *pDiag,int nErr,int nErrKept,const int *aErrPos,` |
|      - | 3176 | `	const char **azErr,int nWarn,const int *aWarnPos,const char **azWarn)` |
|      2 | 3177 | `{` |
|      - | 3178 | `	int k;` |
|    644 | 3179 | `	pDiag->nErr = nErr;` |
|    644 | 3180 | `	pDiag->nErrKept = nErrKept;` |
|    890 | 3181 | `	for( k = 0 ; k < nErrKept ; k++ ){` |
|    247 | 3182 | `		pDiag->aErrPos[k] = aErrPos[k];` |
|    247 | 3183 | `		pDiag->azErr[k] = azErr[k];` |
|    124 | 3184 | `	}` |
|    644 | 3185 | `	pDiag->nWarn = nWarn;` |
|    696 | 3186 | `	for( k = 0 ; k < nWarn ; k++ ){` |
|     53 | 3187 | `		pDiag->aWarnPos[k] = aWarnPos[k];` |
|     53 | 3188 | `		pDiag->azWarn[k] = azWarn[k];` |
|     27 | 3189 | `	}` |
|    644 | 3190 | `}` |
|      - | 3191 | `/* ...poured into a record of the shape a string scan fills, so that both` |
|      - | 3192 | ` * readers of a format scan -- getLastErrors() and the component view -- show` |
|      - | 3193 | ` * it through the same presenter. */` |
|    642 | 3194 | `static void DtFfDiagInto(phl_dt_lasterr *pRec,const dt_ff_diag *pDiag)` |
|      2 | 3195 | `{` |
|      - | 3196 | `	int k;` |
|    644 | 3197 | `	DtRecReset(pRec);` |
|    696 | 3198 | `	for( k = 0 ; k < pDiag->nWarn ; k++ ){` |
|     53 | 3199 | `		DtRecWarn(pRec,pDiag->aWarnPos[k],pDiag->azWarn[k]);` |
|     27 | 3200 | `	}` |
|    890 | 3201 | `	for( k = 0 ; k < pDiag->nErrKept ; k++ ){` |
|    247 | 3202 | `		DtRecErr(pRec,pDiag->aErrPos[k],pDiag->azErr[k]);` |
|    124 | 3203 | `	}` |
|    644 | 3204 | `	pRec->nErr = pDiag->nErr;   /* php counts what it dropped too */` |
|    644 | 3205 | `}` |
|      - | 3206 | `/* ...and the VM's own, which getLastErrors() answers from. */` |
|    520 | 3207 | `static void DtLastErrFf(ph7_vm *pVm,const dt_ff_diag *pDiag)` |
|      2 | 3208 | `{` |
|    522 | 3209 | `	DtFfDiagInto(&pVm->sDtLastErr,pDiag);` |
|    522 | 3210 | `}` |
|      - | 3211 | `/*` |
|      - | 3212 | ` * php's do_range_limit: carry *pa into *pb until *pa sits inside [iStart,iEnd).` |
|      - | 3213 | ` * Spelled the way php spells it, the arithmetic on a field nothing ever set` |
|      - | 3214 | ` * included -- an unset minute is just a very negative number to this code, and` |
|      - | 3215 | `` * what it carries into the hour is what a `z` beside a half-read clock shows.`` |
|      - | 3216 | ` */` |
|   3282 | 3217 | `static void DtFfRangeLimit(sxi64 iStart,sxi64 iEnd,sxi64 iAdj,sxi64 *pa,sxi64 *pb)` |
|      2 | 3218 | `{` |
|   3284 | 3219 | `	if( *pa < iStart ){` |
|    725 | 3220 | `		sxi64 a1 = *pa + 1;` |
|    725 | 3221 | `		*pb -= (iStart - a1) / iAdj + 1;` |
|    725 | 3222 | `		*pa += iAdj * ((iStart - a1) / iAdj);` |
|    725 | 3223 | `		*pa += iAdj;` |
|    362 | 3224 | `	}` |
|   3284 | 3225 | `	if( *pa >= iEnd ){` |
|     49 | 3226 | `		*pb += *pa / iAdj;` |
|     49 | 3227 | `		*pa -= iAdj * (*pa / iAdj);` |
|     24 | 3228 | `	}` |
|   3284 | 3229 | `}` |
|      - | 3230 | `/* ...and its day half, which walks whole months rather than dividing: one call` |
|      - | 3231 | ` * takes the day inside the current month or gives up at the end of a year, and` |
|      - | 3232 | ` * the caller runs it until it has nothing left to move. */` |
|   1016 | 3233 | `static int DtFfRangeLimitDays(sxi64 *py,sxi64 *pm,sxi64 *pd)` |
|      2 | 3234 | `{` |
|   1018 | 3235 | `	int rc = 0;` |
|   1018 | 3236 | `	if( *pd >= 146097 \|\| *pd <= -146097 ){` |
|      - | 3237 | `		/* a whole 400-year era at a time */` |
|      3 | 3238 | `		*py += 400 * (*pd / 146097);` |
|      3 | 3239 | `		*pd -= 146097 * (*pd / 146097);` |
|      1 | 3240 | `	}` |
|   1018 | 3241 | `	DtFfRangeLimit(1,13,12,pm,py);` |
|   9648 | 3242 | `	while( *pd <= 0 && *pm > 0 ){` |
|   8631 | 3243 | `		sxi64 iPrevM = *pm - 1,iPrevY = *py;` |
|   8631 | 3244 | `		if( iPrevM < 1 ){` |
|    721 | 3245 | `			iPrevM += 12;` |
|    721 | 3246 | `			iPrevY = *py - 1;` |
|    360 | 3247 | `		}` |
|   8631 | 3248 | `		*pd += DtDaysInMonth(iPrevY,(int)iPrevM);` |
|   8631 | 3249 | `		(*pm)--;` |
|   8631 | 3250 | `		rc = 1;` |
|      1 | 3251 | `	}` |
|   1120 | 3252 | `	while( *pd > 0 && *pm >= 1 && *pm <= 12 && *pd > DtDaysInMonth(*py,(int)*pm) ){` |
|    103 | 3253 | `		*pd -= DtDaysInMonth(*py,(int)*pm);` |
|    103 | 3254 | `		(*pm)++;` |
|    103 | 3255 | `		rc = 1;` |
|      1 | 3256 | `	}` |
|   1018 | 3257 | `	return rc;` |
|      2 | 3258 | `}` |
|      - | 3259 | `/* php's timelib_do_normalize, asked of the whole vector wherever a format's` |
|      - | 3260 | ` * day-of-year stands. The clock is only carried when the SECOND was read --` |
|      - | 3261 | ` * php's own guard, and not the one anybody would write. */` |
|    420 | 3262 | `static void DtFfNormalize(sxi64 *py,sxi64 *pm,sxi64 *pd,sxi64 *ph,sxi64 *pi,` |
|      - | 3263 | `	sxi64 *ps,sxi64 *pus)` |
|      2 | 3264 | `{` |
|    422 | 3265 | `	if( *pus != DT_UNSET ){ DtFfRangeLimit(0,1000000,1000000,pus,ps); }` |
|    422 | 3266 | `	if( *ps != DT_UNSET ){` |
|    400 | 3267 | `		DtFfRangeLimit(0,60,60,ps,pi);` |
|    400 | 3268 | `		DtFfRangeLimit(0,60,60,pi,ph);` |
|    400 | 3269 | `		DtFfRangeLimit(0,24,24,ph,pd);` |
|    199 | 3270 | `	}` |
|    422 | 3271 | `	DtFfRangeLimit(1,13,12,pm,py);` |
|    422 | 3272 | `	if( *py == 1970 && *pm == 1 ){` |
|      - | 3273 | `		/* php's short cut past the walk, straight off the epoch */` |
|      - | 3274 | `		sxi64 iY;` |
|      - | 3275 | `		int iM,iD;` |
|    163 | 3276 | `		DtCivilFromDays(*pd - 1,&iY,&iM,&iD);` |
|    163 | 3277 | `		*py = iY; *pm = iM; *pd = iD;` |
|    163 | 3278 | `		return;` |
|      - | 3279 | `	}` |
|   1018 | 3280 | `	while( DtFfRangeLimitDays(py,pm,pd) ){}` |
|    260 | 3281 | `	DtFfRangeLimit(1,13,12,pm,py);` |
|    212 | 3282 | `}` |
|      - | 3283 | `/* strtol over a bounded run: it reads the digits it finds and stops at the` |
|      - | 3284 | ` * first byte that is not one, which is how php's offset arithmetic reads each` |
|      - | 3285 | ` * group of a colon spelling out of the middle of the run. */` |
|     68 | 3286 | `static sxi64 DtFfZoneNum(const char *z,const char *zEnd)` |
|      1 | 3287 | `{` |
|     69 | 3288 | `	sxi64 v = 0;` |
|    211 | 3289 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|    143 | 3290 | `		v = v*10 + (z[0] - '0');` |
|    143 | 3291 | `		z++;` |
|      1 | 3292 | `	}` |
|     69 | 3293 | `	return v;` |
|      1 | 3294 | `}` |
|      - | 3295 | `/*` |
|      - | 3296 | ` * php's timelib_parse_tz_cor, the digits behind a format zone's sign. It takes` |
|      - | 3297 | ` * the whole run of digits and colons and then decides what the run MEANT from` |
|      - | 3298 | ``  * its length alone, which is why `+9999` is 99 hours and 99 minutes and `+2460` `` |
|      - | 3299 | ` * is 24 hours and 60: nothing here is in range of anything. A length the switch` |
|      - | 3300 | ` * does not name is no offset at all. Answers 1 when the run spelled one.` |
|      - | 3301 | ` */` |
|     54 | 3302 | `static int DtFfZoneCor(const char **pz,const char *zEnd,sxi32 *piOff)` |
|      1 | 3303 | `{` |
|     55 | 3304 | `	const char *z = *pz,*zBeg = *pz;` |
|      - | 3305 | `	int n;` |
|      - | 3306 | `	sxi64 v;` |
|    274 | 3307 | `	while( z < zEnd && (SyisDigit(z[0]) \|\| z[0] == ':') ){ z++; }` |
|     55 | 3308 | `	n = (int)(z - zBeg);` |
|     55 | 3309 | `	*pz = z;` |
|     55 | 3310 | `	*piOff = 0;` |
|     55 | 3311 | `	switch( n ){` |
|      5 | 3312 | `	case 1: case 2:` |
|     11 | 3313 | `		*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600);` |
|     11 | 3314 | `		return 1;` |
|      6 | 3315 | `	case 3: case 4:` |
|     13 | 3316 | `		if( zBeg[1] == ':' ){` |
|      7 | 3317 | `			*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600` |
|      4 | 3318 | `				+ DtFfZoneNum(&zBeg[2],zEnd) * 60);` |
|     11 | 3319 | `		}else if( zBeg[2] == ':' ){` |
|      4 | 3320 | `			*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600` |
|      2 | 3321 | `				+ DtFfZoneNum(&zBeg[3],zEnd) * 60);` |
|      2 | 3322 | `		}else{` |
|      7 | 3323 | `			v = DtFfZoneNum(zBeg,zEnd);` |
|      7 | 3324 | `			*piOff = (sxi32)((v / 100) * 3600 + (v % 100) * 60);` |
|      - | 3325 | `		}` |
|     13 | 3326 | `		return 1;` |
|      9 | 3327 | `	case 5:` |
|     19 | 3328 | `		if( zBeg[2] != ':' ){ break; }` |
|     25 | 3329 | `		*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600` |
|     16 | 3330 | `			+ DtFfZoneNum(&zBeg[3],zEnd) * 60);` |
|     17 | 3331 | `		return 1;` |
|      1 | 3332 | `	case 6:` |
|      3 | 3333 | `		v = DtFfZoneNum(zBeg,zEnd);` |
|      3 | 3334 | `		*piOff = (sxi32)((v / 10000) * 3600 + ((v / 100) % 100) * 60 + (v % 100));` |
|      3 | 3335 | `		return 1;` |
|      1 | 3336 | `	case 8:` |
|      3 | 3337 | `		if( zBeg[2] != ':' \|\| zBeg[5] != ':' ){ break; }` |
|      4 | 3338 | `		*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600` |
|      2 | 3339 | `			+ DtFfZoneNum(&zBeg[3],zEnd) * 60 + DtFfZoneNum(&zBeg[6],zEnd));` |
|      3 | 3340 | `		return 1;` |
|      5 | 3341 | `	default:` |
|     10 | 3342 | `		break;` |
|      - | 3343 | `	}` |
|     13 | 3344 | `	return 0;` |
|     28 | 3345 | `}` |
|      - | 3346 | `/*` |
|      - | 3347 | ` * php's timelib_parse_zone, and there is no SHAPE to match here the way the` |
|      - | 3348 | ` * string scanner matches one: a format's zone specifier reads whatever stands` |
|      - | 3349 | `` * at the cursor. Blanks and opening parens go first, an uppercase `GMT` in`` |
|      - | 3350 | ` * front of a sign is dropped, a sign is a UTC OFFSET whatever follows it, and` |
|      - | 3351 | `` * anything else is a NAME taken to the end of its run -- letters, digits, `/`,`` |
|      - | 3352 | `` * `_`, `+` and `-` all belong to it, which is why `gmt+3` is one unknown word`` |
|      - | 3353 | `` * where `GMT+3` is three hours.`` |
|      - | 3354 | ` *` |
|      - | 3355 | ` * A sign settles the KIND before the digits are read, so an offset nothing` |
|      - | 3356 | ` * follows is still an offset -- of zero, with a refusal beside it. Answers 1` |
|      - | 3357 | ` * when the zone resolved, 0 when it did not.` |
|      - | 3358 | ` */` |
|    150 | 3359 | `static int DtFfZone(const char **pz,const char *zEnd,int *piKind,sxi32 *piOff,` |
|      - | 3360 | `	const char **pzName,int *pnName)` |
|      2 | 3361 | `{` |
|    152 | 3362 | `	const char *z = *pz;` |
|    152 | 3363 | `	int nPar = 0,bNeg,bIdent = 0,rc;` |
|      - | 3364 | `	/* The OFFSET is written whatever happens -- php assigns the reader's answer,` |
|      - | 3365 | `	 * which is zero when it resolved nothing -- while the KIND and the NAME are` |
|      - | 3366 | `	 * touched only by a zone that DID resolve. So a second specifier that finds` |
|      - | 3367 | `	 * nothing zeroes the offset the first one read and leaves its kind standing. */` |
|    152 | 3368 | `	*piOff = 0;` |
|    242 | 3369 | `	while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t' \|\| z[0] == '(') ){` |
|     11 | 3370 | `		if( z[0] == '(' ){ nPar++; }` |
|     11 | 3371 | `		z++;` |
|      1 | 3372 | `	}` |
|    152 | 3373 | `	if( zEnd - z > 3 && SyMemcmp(z,"GMT",3) == 0 && (z[3] == '+' \|\| z[3] == '-') ){` |
|      5 | 3374 | `		z += 3;` |
|      2 | 3375 | `	}` |
|    152 | 3376 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|     55 | 3377 | `		bNeg = (z[0] == '-');` |
|     55 | 3378 | `		z++;` |
|     55 | 3379 | `		*piKind = DT_ZONE_OFFSET;` |
|     55 | 3380 | `		rc = DtFfZoneCor(&z,zEnd,piOff);` |
|     55 | 3381 | `		if( bNeg ){ *piOff = -*piOff; }` |
|     55 | 3382 | `		*pz = z;` |
|     55 | 3383 | `		return rc;` |
|      - | 3384 | `	}` |
|      - | 3385 | `	{` |
|     98 | 3386 | `		const char *zWord = z;` |
|      - | 3387 | `		int nWord;` |
|    420 | 3388 | `		while( z < zEnd && (SyisAlphaNum((unsigned char)z[0]) \|\| z[0] == '/'` |
|     46 | 3389 | `		 \|\| z[0] == '_' \|\| z[0] == '-' \|\| z[0] == '+') ){` |
|    276 | 3390 | `			z++;` |
|      2 | 3391 | `		}` |
|     98 | 3392 | `		nWord = (int)(z - zWord);` |
|     98 | 3393 | `		rc = nWord > 0 && DtZoneName(zWord,nWord,piOff,pzName,pnName,&bIdent);` |
|     98 | 3394 | `		if( rc ){` |
|     68 | 3395 | `			*piKind = bIdent ? DT_ZONE_ID : DT_ZONE_ABBR;` |
|     33 | 3396 | `		}` |
|    104 | 3397 | `		while( nPar > 0 && z < zEnd && z[0] == ')' ){` |
|      7 | 3398 | `			z++;` |
|      7 | 3399 | `			nPar--;` |
|      1 | 3400 | `		}` |
|     98 | 3401 | `		*pz = z;` |
|     98 | 3402 | `		return rc;` |
|      - | 3403 | `	}` |
|     77 | 3404 | `}` |
|      - | 3405 | `/*` |
|      - | 3406 | ` * php's timelib_get_nr, the reader behind every plain digit field of a format:` |
|      - | 3407 | ` * it steps over whatever is NOT a digit -- to the end of the input if it has` |
|      - | 3408 | ` * to -- and then takes at most nMax of them. Answers how many digits it took,` |
|      - | 3409 | ` * or -1 when the input ran out before it found one; the cursor moves either way.` |
|      - | 3410 | ` */` |
|    744 | 3411 | `static int DtFfGetNr(const char **pz,const char *zEnd,int nMax,sxi64 *pVal)` |
|      2 | 3412 | `{` |
|    746 | 3413 | `	const char *z = *pz;` |
|    746 | 3414 | `	sxi64 v = 0;` |
|    746 | 3415 | `	int n = 0;` |
|    896 | 3416 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|    746 | 3417 | `	if( z >= zEnd ){` |
|     19 | 3418 | `		*pz = z;` |
|     19 | 3419 | `		return -1;` |
|      - | 3420 | `	}` |
|   2526 | 3421 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|   1800 | 3422 | `		v = v*10 + (z[0] - '0');` |
|   1800 | 3423 | `		z++;` |
|   1800 | 3424 | `		n++;` |
|      2 | 3425 | `	}` |
|    728 | 3426 | `	*pz = z;` |
|    728 | 3427 | `	*pVal = v;` |
|    728 | 3428 | `	return n;` |
|    374 | 3429 | `}` |
|      - | 3430 | `/*` |
|      - | 3431 | `` * php's timelib_get_signed_nr, which `U` reads through: it steps over anything`` |
|      - | 3432 | ` * that is neither a digit nor a sign, takes a RUN of signs (each minus flipping` |
|      - | 3433 | ` * it), steps over non-digits again, and reads at most nMax digits. Its two ways` |
|      - | 3434 | ` * of giving up -- an input that ends before a digit, and a value no int64 can` |
|      - | 3435 | ` * hold -- are refusals php raises through its STRING scanner's door rather than` |
|      - | 3436 | ` * the format one's, so both are reported at position 0 whatever the format was` |
|      - | 3437 | ` * doing, and both answer zero.` |
|      - | 3438 | ` */` |
|     44 | 3439 | `static int DtFfGetSignedNr(const char **pz,const char *zEnd,int nMax,sxi64 *pVal,` |
|      - | 3440 | `	const char **pzErr)` |
|      1 | 3441 | `{` |
|     45 | 3442 | `	const char *z = *pz;` |
|     45 | 3443 | `	sxu64 u = 0,uLimit;` |
|     45 | 3444 | `	int bNeg = 0,n = 0,bOver = 0;` |
|     45 | 3445 | `	*pzErr = 0;` |
|     45 | 3446 | `	*pVal = 0;` |
|     61 | 3447 | `	while( z < zEnd && !SyisDigit(z[0]) && z[0] != '+' && z[0] != '-' ){ z++; }` |
|     45 | 3448 | `	if( z >= zEnd ){` |
|      5 | 3449 | `		*pz = z;` |
|      5 | 3450 | `		*pzErr = "Found unexpected data";` |
|      5 | 3451 | `		return 0;` |
|      - | 3452 | `	}` |
|    106 | 3453 | `	while( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|     31 | 3454 | `		if( z[0] == '-' ){ bNeg = !bNeg; }` |
|     31 | 3455 | `		z++;` |
|      1 | 3456 | `	}` |
|     41 | 3457 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|     41 | 3458 | `	if( z >= zEnd ){` |
|    ! 0 | 3459 | `		*pz = z;` |
|    ! 0 | 3460 | `		*pzErr = "Found unexpected data";` |
|    ! 0 | 3461 | `		return 0;` |
|      - | 3462 | `	}` |
|      - | 3463 | `	/* php's ceiling is strtoll's, so the negative side reaches one further */` |
|     41 | 3464 | `	uLimit = bNeg ? ((sxu64)SXI64_HIGH + 1) : (sxu64)SXI64_HIGH;` |
|    259 | 3465 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|    219 | 3466 | `		sxu64 dg = (sxu64)(z[0] - '0');` |
|    219 | 3467 | `		if( u > (uLimit - dg) / 10 ){` |
|     15 | 3468 | `			bOver = 1;` |
|      7 | 3469 | `		}` |
|    219 | 3470 | `		if( !bOver ){` |
|    205 | 3471 | `			u = u*10 + dg;` |
|    102 | 3472 | `		}` |
|    219 | 3473 | `		z++;` |
|    219 | 3474 | `		n++;` |
|      1 | 3475 | `	}` |
|     41 | 3476 | `	*pz = z;` |
|     41 | 3477 | `	if( bOver ){` |
|      5 | 3478 | `		*pzErr = "Number out of range";` |
|      5 | 3479 | `		return 0;` |
|      - | 3480 | `	}` |
|      - | 3481 | `	/* the negation is spelled unsigned: the floor has no positive twin */` |
|     37 | 3482 | `	*pVal = bNeg ? (sxi64)(0 - u) : (sxi64)u;` |
|     37 | 3483 | `	return 1;` |
|     23 | 3484 | `}` |
|      - | 3485 | `/*` |
|      - | 3486 | ` * php's MONTH table, which a format matches as a whole WORD: the letters are` |
|      - | 3487 | ` * taken to the end of their run and the run has to spell one of the names` |
|      - | 3488 | `` * exactly, so `janx` is no month at all where the string parser reads January`` |
|      - | 3489 | `` * out of it. The names are php's own -- three letters, `sept`, the full months,`` |
|      - | 3490 | `` * and the ROMAN numerals `i` through `xii`, which is why a format's `F` reads`` |
|      - | 3491 | `` * `x` as October. Advances *pz over the run whether or not it spelled one.`` |
|      - | 3492 | ` */` |
|     38 | 3493 | `static int DtFfMonth(const char **pz,const char *zEnd)` |
|      1 | 3494 | `{` |
|      - | 3495 | `	static const struct { const char *z; int n; int mo; } aM[] = {` |
|      - | 3496 | `		{ "jan",3,1 },{ "feb",3,2 },{ "mar",3,3 },{ "apr",3,4 },{ "may",3,5 },` |
|      - | 3497 | `		{ "jun",3,6 },{ "jul",3,7 },{ "aug",3,8 },{ "sep",3,9 },{ "sept",4,9 },` |
|      - | 3498 | `		{ "oct",3,10 },{ "nov",3,11 },{ "dec",3,12 },` |
|      - | 3499 | `		{ "i",1,1 },{ "ii",2,2 },{ "iii",3,3 },{ "iv",2,4 },{ "v",1,5 },` |
|      - | 3500 | `		{ "vi",2,6 },{ "vii",3,7 },{ "viii",4,8 },{ "ix",2,9 },{ "x",1,10 },` |
|      - | 3501 | `		{ "xi",2,11 },{ "xii",3,12 },` |
|      - | 3502 | `		{ "january",7,1 },{ "february",8,2 },{ "march",5,3 },{ "april",5,4 },` |
|      - | 3503 | `		{ "june",4,6 },{ "july",4,7 },{ "august",6,8 },{ "september",9,9 },` |
|      - | 3504 | `		{ "october",7,10 },{ "november",8,11 },{ "december",8,12 }` |
|      - | 3505 | `	};` |
|     39 | 3506 | `	const char *z = *pz,*zWord = *pz;` |
|      - | 3507 | `	sxu32 i;` |
|      - | 3508 | `	int n;` |
|    189 | 3509 | `	while( z < zEnd && SyisAlpha((unsigned char)z[0]) ){ z++; }` |
|     39 | 3510 | `	n = (int)(z - zWord);` |
|     39 | 3511 | `	*pz = z;` |
|    509 | 3512 | `	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){` |
|    507 | 3513 | `		if( aM[i].n == n && SyStrnicmp(zWord,aM[i].z,(sxu32)n) == 0 ){` |
|     37 | 3514 | `			return aM[i].mo;` |
|      - | 3515 | `		}` |
|    236 | 3516 | `	}` |
|      3 | 3517 | `	return 0;` |
|     20 | 3518 | `}` |
|      - | 3519 | `/*` |
|      - | 3520 | ` * ...and php's RELATIVE-UNIT table, which is where a format's textual DAY is` |
|      - | 3521 | `` * looked up. `D` and `l` do not read a weekday name at all there: they read a`` |
|      - | 3522 | ` * word up to the next separator and ask the relative-unit table what it is, so` |
|      - | 3523 | `` * every unit spelling answers one -- `week` is the weekday 7 and `ms` the`` |
|      - | 3524 | ` * weekday 1000, neither of which is a day of any week. Advances *pz over the` |
|      - | 3525 | ` * word whether or not it spelled one; answers 1 and fills *piWday when it did.` |
|      - | 3526 | ` */` |
|     46 | 3527 | `static int DtFfRelunit(const char **pz,const char *zEnd,sxi64 *piWday)` |
|      1 | 3528 | `{` |
|      - | 3529 | `	static const struct { const char *z; int n; int mul; } aU[] = {` |
|      - | 3530 | `		{ "ms",2,1000 },{ "msec",4,1000 },{ "msecs",5,1000 },` |
|      - | 3531 | `		{ "millisecond",11,1000 },{ "milliseconds",12,1000 },` |
|      - | 3532 | `		{ "\xc2\xb5s",3,1 },{ "usec",4,1 },{ "usecs",5,1 },` |
|      - | 3533 | `		{ "\xc2\xb5sec",5,1 },{ "\xc2\xb5secs",6,1 },` |
|      - | 3534 | `		{ "microsecond",11,1 },{ "microseconds",12,1 },` |
|      - | 3535 | `		{ "sec",3,1 },{ "secs",4,1 },{ "second",6,1 },{ "seconds",7,1 },` |
|      - | 3536 | `		{ "min",3,1 },{ "mins",4,1 },{ "minute",6,1 },{ "minutes",7,1 },` |
|      - | 3537 | `		{ "hour",4,1 },{ "hours",5,1 },` |
|      - | 3538 | `		{ "day",3,1 },{ "days",4,1 },` |
|      - | 3539 | `		{ "week",4,7 },{ "weeks",5,7 },` |
|      - | 3540 | `		{ "fortnight",9,14 },{ "fortnights",10,14 },` |
|      - | 3541 | `		{ "forthnight",10,14 },{ "forthnights",11,14 },` |
|      - | 3542 | `		{ "month",5,1 },{ "months",6,1 },` |
|      - | 3543 | `		{ "year",4,1 },{ "years",5,1 },` |
|      - | 3544 | `		{ "mondays",7,1 },{ "monday",6,1 },{ "mon",3,1 },` |
|      - | 3545 | `		{ "tuesdays",8,2 },{ "tuesday",7,2 },{ "tue",3,2 },` |
|      - | 3546 | `		{ "wednesdays",10,3 },{ "wednesday",9,3 },{ "wed",3,3 },` |
|      - | 3547 | `		{ "thursdays",9,4 },{ "thursday",8,4 },{ "thu",3,4 },` |
|      - | 3548 | `		{ "fridays",7,5 },{ "friday",6,5 },{ "fri",3,5 },` |
|      - | 3549 | `		{ "saturdays",9,6 },{ "saturday",8,6 },{ "sat",3,6 },` |
|      - | 3550 | `		{ "sundays",7,0 },{ "sunday",6,0 },{ "sun",3,0 },` |
|      - | 3551 | `		{ "weekday",7,1 },{ "weekdays",8,1 }` |
|      - | 3552 | `	};` |
|     47 | 3553 | `	const char *z = *pz,*zWord = *pz;` |
|      - | 3554 | `	sxu32 i;` |
|      - | 3555 | `	int n;` |
|    269 | 3556 | `	while( z < zEnd && z[0] != ' ' && z[0] != ',' && z[0] != '\t' && z[0] != ';'` |
|    190 | 3557 | `	 && z[0] != ':' && z[0] != '/' && z[0] != '.' && z[0] != '-'` |
|    309 | 3558 | `	 && z[0] != '(' && z[0] != ')' ){` |
|    191 | 3559 | `		z++;` |
|      1 | 3560 | `	}` |
|     47 | 3561 | `	n = (int)(z - zWord);` |
|     47 | 3562 | `	*pz = z;` |
|   1875 | 3563 | `	for( i = 0 ; i < SX_ARRAYSIZE(aU) ; ++i ){` |
|   1873 | 3564 | `		if( aU[i].n == n && SyStrnicmp(zWord,aU[i].z,(sxu32)n) == 0 ){` |
|     45 | 3565 | `			*piWday = aU[i].mul;` |
|     45 | 3566 | `			return 1;` |
|      - | 3567 | `		}` |
|    915 | 3568 | `	}` |
|      3 | 3569 | `	return 0;` |
|     24 | 3570 | `}` |
|      - | 3571 | `/*` |
|      - | 3572 | ` * php's meridian, which is an ADJUSTMENT to whatever hour was already read` |
|      - | 3573 | `` * rather than a reading of its own: `am` takes noon back to midnight and leaves`` |
|      - | 3574 | `` * every other hour standing, `pm` adds twelve to all but twelve itself.`` |
|      - | 3575 | ` *` |
|      - | 3576 | `` * It hunts for its own letter -- anything that is not one of `AaPp` is stepped`` |
|      - | 3577 | `` * over, so `1 xx pm` is one in the afternoon -- and then wants either a bare`` |
|      - | 3578 | `` * `m` or the whole `.m.`; the cursor stays wherever the spelling ran out when`` |
|      - | 3579 | ` * it turns out to be neither. Answers 1 and fills *piAdj, or 0.` |
|      - | 3580 | ` */` |
|     38 | 3581 | `static int DtFfMeridian(const char **pz,const char *zEnd,sxi64 h,sxi64 *piAdj)` |
|      1 | 3582 | `{` |
|     39 | 3583 | `	const char *z = *pz;` |
|      - | 3584 | `	int bAm;` |
|     51 | 3585 | `	while( z < zEnd && z[0] != 'A' && z[0] != 'a' && z[0] != 'P' && z[0] != 'p' ){` |
|     13 | 3586 | `		z++;` |
|      1 | 3587 | `	}` |
|     39 | 3588 | `	if( z >= zEnd ){` |
|    ! 0 | 3589 | `		*pz = z;` |
|    ! 0 | 3590 | `		return 0;` |
|      - | 3591 | `	}` |
|     39 | 3592 | `	bAm = (z[0] == 'a' \|\| z[0] == 'A');` |
|     39 | 3593 | `	*piAdj = bAm ? ((h == 12) ? -12 : 0) : ((h != 12) ? 12 : 0);` |
|     39 | 3594 | `	z++;` |
|     39 | 3595 | `	if( z < zEnd && z[0] == '.' ){` |
|      5 | 3596 | `		z++;` |
|      5 | 3597 | `		if( z >= zEnd \|\| (z[0] != 'm' && z[0] != 'M') ){ *pz = z; return 0; }` |
|      5 | 3598 | `		z++;` |
|      5 | 3599 | `		if( z >= zEnd \|\| z[0] != '.' ){ *pz = z; return 0; }` |
|    ! 0 | 3600 | `		z++;` |
|     35 | 3601 | `	}else if( z < zEnd && (z[0] == 'm' \|\| z[0] == 'M') ){` |
|     33 | 3602 | `		z++;` |
|     17 | 3603 | `	}else{` |
|      3 | 3604 | `		*pz = z;` |
|      3 | 3605 | `		return 0;` |
|      - | 3606 | `	}` |
|     33 | 3607 | `	*pz = z;` |
|     33 | 3608 | `	return 1;` |
|     20 | 3609 | `}` |
|      - | 3610 | `/*` |
|      - | 3611 | `` * The eight bytes php's format map calls SEPARATORS: what `#` accepts, and what`` |
|      - | 3612 | ` * each of them demands of the input when it stands in a format itself.` |
|      - | 3613 | ` */` |
|      8 | 3614 | `static int DtFfIsSep(int c)` |
|      1 | 3615 | `{` |
|     10 | 3616 | `	return c==';' \|\| c==':' \|\| c=='/' \|\| c=='.' \|\| c==',' \|\| c=='-'` |
|     11 | 3617 | `		\|\| c=='(' \|\| c==')';` |
|      1 | 3618 | `}` |
|      - | 3619 | `/*` |
|      - | 3620 | ` * php's run of blanks -- the two ASCII ones and the two Unicode spaces its` |
|      - | 3621 | ` * scanner spells out. A format space eats the whole run and never refuses, so a` |
|      - | 3622 | ` * space beside an input that has none is simply nothing.` |
|      - | 3623 | ` */` |
|    160 | 3624 | `static void DtFfEatSpaces(const char **pz,const char *zEnd)` |
|      2 | 3625 | `{` |
|    162 | 3626 | `	const char *z = *pz;` |
|     80 | 3627 | `	for(;;){` |
|    330 | 3628 | `		if( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|    166 | 3629 | `			z++;` |
|    166 | 3630 | `			continue;` |
|      - | 3631 | `		}` |
|    164 | 3632 | `		if( zEnd - z >= 3 && (unsigned char)z[0] == 0xE2` |
|     77 | 3633 | `		 && (unsigned char)z[1] == 0x80 && (unsigned char)z[2] == 0xAF ){` |
|      3 | 3634 | `			z += 3;    /* NARROW NO-BREAK SPACE */` |
|      3 | 3635 | `			continue;` |
|      - | 3636 | `		}` |
|    162 | 3637 | `		if( zEnd - z >= 2 && (unsigned char)z[0] == 0xC2` |
|     79 | 3638 | `		 && (unsigned char)z[1] == 0xA0 ){` |
|      3 | 3639 | `			z += 2;    /* NO-BREAK SPACE */` |
|      3 | 3640 | `			continue;` |
|      - | 3641 | `		}` |
|    162 | 3642 | `		break;` |
|    ! 0 | 3643 | `	}` |
|    162 | 3644 | `	*pz = z;` |
|    162 | 3645 | `}` |
|      - | 3646 | `/*` |
|      - | 3647 | ` * What one run of the FORMAT scanner read, field by field.` |
|      - | 3648 | ` *` |
|      - | 3649 | ` * php's format parser starts every field UNSET and never consults the clock: the` |
|      - | 3650 | ` * struct below is what the scan itself put there, so a format that named no year` |
|      - | 3651 | `` * leaves `y` unset rather than this year's. The moment a DateTime wants is built`` |
|      - | 3652 | ` * from it afterwards (DtFfResolve), which is where the current instant finally` |
|      - | 3653 | ` * fills what the format never mentioned -- php's own timelib_fill_holes, run once` |
|      - | 3654 | ` * the scan is over rather than while it is going on.` |
|      - | 3655 | ` */` |
|      - | 3656 | `typedef struct dt_ff_res dt_ff_res;` |
|      - | 3657 | `struct dt_ff_res` |
|      - | 3658 | `{` |
|      - | 3659 | `	sxi64 y,mo,d,h,mi,s,us;   /* DT_UNSET == php's TIMELIB_UNSET */` |
|      - | 3660 | `	sxi32 iOff;` |
|      - | 3661 | `	int bLocal;               /* php's is_localtime -- a zone was READ */` |
|      - | 3662 | `	int iOffKind;             /* ...and php's zone_type, 0 when it meant nothing */` |
|      - | 3663 | `	const char *zName;        /* a static literal, as every zone name here is */` |
|      - | 3664 | `	int nName;` |
|      - | 3665 | `	int bWday;                /* php's relative.have_weekday_relative */` |
|      - | 3666 | `	sxi64 iWday;` |
|      - | 3667 | `	dt_ff_diag sDiag;` |
|      - | 3668 | `};` |
|    642 | 3669 | `static int DtFromFormat(const char *zFmt,int nFmt,const char *zIn,int nIn,` |
|      - | 3670 | `	dt_ff_res *pOut)` |
|      2 | 3671 | `{` |
|      - | 3672 | `	const char *zEnd,*zInEnd,*z;` |
|      - | 3673 | `	sxi64 v;` |
|    644 | 3674 | `	sxi64 y = DT_UNSET,mo = DT_UNSET,d = DT_UNSET;` |
|    644 | 3675 | `	sxi64 h = DT_UNSET,mi = DT_UNSET,s = DT_UNSET,us = DT_UNSET;` |
|    644 | 3676 | `	sxi64 uVal = 0;` |
|    644 | 3677 | `	int bPlus = 0,bLocal = 0;` |
|    644 | 3678 | `	int bWday = 0;` |
|    644 | 3679 | `	sxi64 iWday = 0;` |
|    644 | 3680 | `	int iOffKind = 0,nName = 0;` |
|    644 | 3681 | `	sxi32 iOffVal = 0;` |
|    644 | 3682 | `	const char *zName = 0;` |
|    644 | 3683 | `	const char *zErr = 0;` |
|      - | 3684 | `	const char *aWarnMsg[PH7_DT_MAX_WARN];` |
|      - | 3685 | `	int aWarnPos[PH7_DT_MAX_WARN];` |
|    644 | 3686 | `	int nWarn = 0;` |
|      - | 3687 | `	const char *aErrMsg[PH7_DT_MAX_ERR];` |
|      - | 3688 | `	int aErrPos[PH7_DT_MAX_ERR];` |
|    644 | 3689 | `	int nErr = 0,nErrKept = 0;` |
|    644 | 3690 | `	SyZero(pOut,sizeof(*pOut));` |
|      - | 3691 | `	/* Both strings end where php's C string ends: a NUL inside a format simply` |
|      - | 3692 | `	 * truncates it, and one inside the input ends the scan there. */` |
|    644 | 3693 | `	zEnd = &zFmt[DtCStrLen(zFmt,nFmt)];` |
|    644 | 3694 | `	zInEnd = &zIn[DtCStrLen(zIn,nIn)];` |
|    644 | 3695 | `	z = zIn;` |
|      - | 3696 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|      - | 3697 | `	{ int _p = (iPos),_k,_f = -1; \` |
|      - | 3698 | `	  nErr++; \` |
|      - | 3699 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|      - | 3700 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|      - | 3701 | `	  else if( nErrKept < PH7_DT_MAX_ERR ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|      - | 3702 | `/* php's two RESET specifiers act where they stand rather than at the end of the` |
|      - | 3703 | `` * scan: `!` puts every field at its 1970 default whatever the format already`` |
|      - | 3704 | `` * read, `\|` only fills what nothing has read yet, and a specifier after either`` |
|      - | 3705 | ` * one overwrites what it left. */` |
|      - | 3706 | `#define DT_FF_RESET(bUnsetOnly) \` |
|      - | 3707 | `	{ int _u = (bUnsetOnly); \` |
|      - | 3708 | `	  if( !_u \|\| y  == DT_UNSET ){ y  = 1970; } \` |
|      - | 3709 | `	  if( !_u \|\| mo == DT_UNSET ){ mo = 1; } \` |
|      - | 3710 | `	  if( !_u \|\| d  == DT_UNSET ){ d  = 1; } \` |
|      - | 3711 | `	  if( !_u \|\| h  == DT_UNSET ){ h  = 0; } \` |
|      - | 3712 | `	  if( !_u \|\| mi == DT_UNSET ){ mi = 0; } \` |
|      - | 3713 | `	  if( !_u \|\| s  == DT_UNSET ){ s  = 0; } \` |
|      - | 3714 | `	  if( !_u \|\| us == DT_UNSET ){ us = 0; } }` |
|      - | 3715 | `	/* The scan runs while BOTH strings still have something in them: php's` |
|      - | 3716 | `	 * format loop ends the moment the input does, and what is left of the` |
|      - | 3717 | `	 * format is judged afterwards rather than refused here. */` |
|   2340 | 3718 | `	while( zFmt < zEnd && z < zInEnd ){` |
|   1698 | 3719 | `		char c = zFmt[0];` |
|      - | 3720 | `		/* every refusal below reports the byte the specifier STARTED on, not` |
|      - | 3721 | `		 * wherever the reading of it gave up */` |
|   1698 | 3722 | `		int iBegin = (int)(z - zIn);` |
|   1698 | 3723 | `		zFmt++;` |
|   1698 | 3724 | `		zErr = 0;` |
|   1698 | 3725 | `		if( c == '!' ){` |
|     41 | 3726 | `			DT_FF_RESET(0);` |
|     41 | 3727 | `			continue;` |
|      - | 3728 | `		}` |
|   1658 | 3729 | `		if( c == '\|' ){` |
|     71 | 3730 | `			DT_FF_RESET(1);` |
|     71 | 3731 | `			continue;` |
|      - | 3732 | `		}` |
|   1588 | 3733 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|      - | 3734 | `/* php asks of every digit field, BEFORE reading it, whether the cursor is on a` |
|      - | 3735 | ` * digit at all -- and merely says so: the reader that follows hunts for its` |
|      - | 3736 | `` * digits regardless, so `x5` is the day 5 with one refusal behind it. */`` |
|      - | 3737 | `#define DT_FF_CHECKNUM \` |
|      - | 3738 | `	if( !SyisDigit(z[0]) ){ DT_FF_LOGERR(iBegin,"Unexpected data found."); }` |
|      - | 3739 | `#define DT_FF_CHECKSIGNED \` |
|      - | 3740 | `	if( !SyisDigit(z[0]) && z[0] != '+' && z[0] != '-' ){ \` |
|      - | 3741 | `		DT_FF_LOGERR(iBegin,"Unexpected data found."); }` |
|   1580 | 3742 | `		switch( c ){` |
|     64 | 3743 | `		case 'd': case 'j':` |
|    134 | 3744 | `			DT_FF_CHECKNUM;` |
|    130 | 3745 | `			if( DtFfGetNr(&z,zInEnd,2,&d) < 0 ){` |
|      3 | 3746 | `				DT_FF_LOGERR(iBegin,"A two digit day could not be found");` |
|      3 | 3747 | `				d = DT_UNSET;` |
|      1 | 3748 | `			}` |
|    130 | 3749 | `			break;` |
|     23 | 3750 | `		case 'D': case 'l':` |
|      - | 3751 | `			/* php's textual day is a RELATIVE weekday, not decoration: it moves` |
|      - | 3752 | `			 * the date it was read beside, forward to that weekday and keeping a` |
|      - | 3753 | `			 * day that already matches. Both spellings read the same table --` |
|      - | 3754 | `			 * the three-letter and the full name are one rule there. */` |
|     47 | 3755 | `			if( DtFfRelunit(&z,zInEnd,&iWday) ){` |
|     45 | 3756 | `				bWday = 1;` |
|     23 | 3757 | `			}else{` |
|      3 | 3758 | `				zErr = "A textual day could not be found";` |
|      - | 3759 | `			}` |
|     47 | 3760 | `			break;` |
|     21 | 3761 | `		case 'z':` |
|      - | 3762 | `			/* php's DAY OF YEAR is a whole date rather than a field: it needs a` |
|      - | 3763 | `			 * year already read, puts the month back at January and the day at` |
|      - | 3764 | `			 * the count, and normalizes the vector where it stands. */` |
|     46 | 3765 | `			DT_FF_CHECKNUM;` |
|     43 | 3766 | `			if( y == DT_UNSET ){` |
|      9 | 3767 | `				DT_FF_LOGERR(iBegin,"A 'day of year' can only come after a year has been found");` |
|      4 | 3768 | `			}` |
|     43 | 3769 | `			if( DtFfGetNr(&z,zInEnd,3,&v) < 0 ){` |
|      5 | 3770 | `				DT_FF_LOGERR(iBegin,"A three digit day-of-year could not be found");` |
|      5 | 3771 | `				break;` |
|      - | 3772 | `			}` |
|     39 | 3773 | `			if( y != DT_UNSET ){` |
|     33 | 3774 | `				mo = 1;` |
|     33 | 3775 | `				d = v + 1;` |
|     33 | 3776 | `				DtFfNormalize(&y,&mo,&d,&h,&mi,&s,&us);` |
|     16 | 3777 | `			}` |
|     39 | 3778 | `			break;` |
|     10 | 3779 | `		case 'x': case 'X':{` |
|      - | 3780 | `			/* the EXPANDED year: a sign and up to nineteen digits, and the year` |
|      - | 3781 | `			 * php takes from a run it could not read is zero rather than none. */` |
|     21 | 3782 | `			const char *zNrErr = 0;` |
|     22 | 3783 | `			DT_FF_CHECKSIGNED;` |
|     21 | 3784 | `			DtFfGetSignedNr(&z,zInEnd,19,&y,&zNrErr);` |
|     21 | 3785 | `			if( zNrErr ){` |
|      5 | 3786 | `				DT_FF_LOGERR(0,zNrErr);` |
|      2 | 3787 | `			}` |
|     21 | 3788 | `			break;` |
|      - | 3789 | `				 }` |
|      5 | 3790 | `		case 'S':` |
|      - | 3791 | `			/* the ordinal suffix, which php declines to look at when the cursor` |
|      - | 3792 | `			 * is on a blank and otherwise takes in either case */` |
|     10 | 3793 | `			if( !SyisSpace((unsigned char)z[0]) && zInEnd-z >= 2` |
|     11 | 3794 | `			 && (SyStrnicmp(z,"st",2) == 0 \|\| SyStrnicmp(z,"nd",2) == 0` |
|      5 | 3795 | `			  \|\| SyStrnicmp(z,"rd",2) == 0 \|\| SyStrnicmp(z,"th",2) == 0) ){` |
|      7 | 3796 | `				z += 2;` |
|      3 | 3797 | `			}` |
|     11 | 3798 | `			break;` |
|     53 | 3799 | `		case 'm': case 'n':` |
|    108 | 3800 | `			DT_FF_CHECKNUM;` |
|    108 | 3801 | `			if( DtFfGetNr(&z,zInEnd,2,&mo) < 0 ){` |
|    ! 0 | 3802 | `				DT_FF_LOGERR(iBegin,"A two digit month could not be found");` |
|    ! 0 | 3803 | `				mo = DT_UNSET;` |
|    ! 0 | 3804 | `			}` |
|    108 | 3805 | `			break;` |
|     19 | 3806 | `		case 'M': case 'F':{` |
|      - | 3807 | `			int k;` |
|     39 | 3808 | `			k = DtFfMonth(&z,zInEnd);` |
|     39 | 3809 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     39 | 3810 | `			break;` |
|      - | 3811 | `				 }` |
|      5 | 3812 | `		case 'y':` |
|     11 | 3813 | `			DT_FF_CHECKNUM;` |
|     11 | 3814 | `			if( DtFfGetNr(&z,zInEnd,2,&y) < 0 ){` |
|    ! 0 | 3815 | `				DT_FF_LOGERR(iBegin,"A two digit year could not be found");` |
|    ! 0 | 3816 | `				y = DT_UNSET;` |
|     11 | 3817 | `			}else if( y < 100 ){` |
|      - | 3818 | `				/* php's two-digit century, which cuts at seventy */` |
|     11 | 3819 | `				y += (y < 70) ? 2000 : 1900;` |
|      5 | 3820 | `			}` |
|     11 | 3821 | `			break;` |
|     95 | 3822 | `		case 'Y':` |
|    198 | 3823 | `			DT_FF_CHECKNUM;` |
|    192 | 3824 | `			if( DtFfGetNr(&z,zInEnd,4,&y) < 0 ){` |
|     11 | 3825 | `				DT_FF_LOGERR(iBegin,"A four digit year could not be found");` |
|     11 | 3826 | `				y = DT_UNSET;` |
|      5 | 3827 | `			}` |
|    192 | 3828 | `			break;` |
|     35 | 3829 | `		case 'H': case 'G':` |
|     72 | 3830 | `			DT_FF_CHECKNUM;` |
|     72 | 3831 | `			if( DtFfGetNr(&z,zInEnd,2,&h) < 0 ){` |
|    ! 0 | 3832 | `				DT_FF_LOGERR(iBegin,"A two digit hour could not be found");` |
|    ! 0 | 3833 | `				h = DT_UNSET;` |
|    ! 0 | 3834 | `			}` |
|     72 | 3835 | `			break;` |
|     16 | 3836 | `		case 'h': case 'g':` |
|     33 | 3837 | `			DT_FF_CHECKNUM;` |
|     33 | 3838 | `			if( DtFfGetNr(&z,zInEnd,2,&h) < 0 ){` |
|    ! 0 | 3839 | `				DT_FF_LOGERR(iBegin,"A two digit hour could not be found");` |
|    ! 0 | 3840 | `				h = DT_UNSET;` |
|     33 | 3841 | `			}else if( h > 12 ){` |
|      - | 3842 | `				/* the twelve-hour spellings refuse a bigger one -- and keep it */` |
|      9 | 3843 | `				DT_FF_LOGERR(iBegin,"Hour cannot be higher than 12");` |
|      4 | 3844 | `			}` |
|     33 | 3845 | `			break;` |
|     60 | 3846 | `		case 'i': case 's':{` |
|      - | 3847 | `			/* the minute and the second are php's only EXACTLY two-digit` |
|      - | 3848 | `			 * fields: a lone digit is no minute there, however many follow` |
|      - | 3849 | `			 * it -- and a reading that fails leaves whatever was read before */` |
|    122 | 3850 | `			sxi64 t = 0;` |
|    122 | 3851 | `			DT_FF_CHECKNUM;` |
|    122 | 3852 | `			if( DtFfGetNr(&z,zInEnd,2,&t) != 2 ){` |
|      9 | 3853 | `				DT_FF_LOGERR(iBegin,c == 'i'` |
|      - | 3854 | `					? "A two digit minute could not be found"` |
|      1 | 3855 | `					: "A two digit second could not be found");` |
|    118 | 3856 | `			}else if( c == 'i' ){` |
|     66 | 3857 | `				mi = t;` |
|     34 | 3858 | `			}else{` |
|     50 | 3859 | `				s = t;` |
|      - | 3860 | `			}` |
|    122 | 3861 | `			break;` |
|      - | 3862 | `				 }` |
|     23 | 3863 | `		case 'u': case 'v':{` |
|      - | 3864 | `			/* the fraction is scaled by what the READER walked, not by the` |
|      - | 3865 | `			 * digits it found: the bytes it stepped over hunting for them count` |
|      - | 3866 | ``			 * against the width too, so `x1` under `u` is a hundredth */`` |
|     47 | 3867 | `			const char *zStart = z;` |
|     47 | 3868 | `			int nMax = (c == 'u') ? 6 : 3;` |
|     61 | 3869 | `			DT_FF_CHECKNUM;` |
|     47 | 3870 | `			if( DtFfGetNr(&z,zInEnd,nMax,&v) < 0 ){` |
|      3 | 3871 | `				DT_FF_LOGERR(iBegin,c == 'u'` |
|      - | 3872 | `					? "A six digit microsecond could not be found"` |
|      1 | 3873 | `					: "A three digit millisecond could not be found");` |
|      3 | 3874 | `				break;` |
|      - | 3875 | `			}` |
|      - | 3876 | `			{` |
|      - | 3877 | `				/* ...and BOTH spellings land on the same scale, because php` |
|      - | 3878 | `				 * multiplies the millisecond by a thousand after dividing it by` |
|      - | 3879 | `				 * the width: what either one answers is the digits it read` |
|      - | 3880 | `				 * times ten to the six-minus-bytes-walked, and a reader that` |
|      - | 3881 | `				 * walked more than six bytes answers a truncated fraction. */` |
|     45 | 3882 | `				int k = 6 - (int)(z - zStart);` |
|    159 | 3883 | `				while( k > 0 ){ v *= 10; k--; }` |
|     55 | 3884 | `				while( k < 0 ){ v /= 10; k++; }` |
|     45 | 3885 | `				us = v;` |
|      - | 3886 | `			}` |
|     45 | 3887 | `			break;` |
|      - | 3888 | `				 }` |
|     19 | 3889 | `		case 'a': case 'A':{` |
|     39 | 3890 | `			sxi64 iAdj = 0;` |
|     39 | 3891 | `			if( h == DT_UNSET ){` |
|      9 | 3892 | `				DT_FF_LOGERR(iBegin,"Meridian can only come after an hour has been found");` |
|      4 | 3893 | `			}` |
|     39 | 3894 | `			if( !DtFfMeridian(&z,zInEnd,h,&iAdj) ){` |
|      7 | 3895 | `				zErr = "A meridian could not be found";` |
|     36 | 3896 | `			}else if( h != DT_UNSET ){` |
|     29 | 3897 | `				h += iAdj;` |
|     14 | 3898 | `			}` |
|     39 | 3899 | `			break;` |
|      - | 3900 | `				 }` |
|     12 | 3901 | `		case 'U':{` |
|      - | 3902 | `			/* php's epoch seconds are not a field but a whole MOMENT: it spreads` |
|      - | 3903 | `			 * the timestamp back over y/m/d/h/i/s at UTC right here, so a` |
|      - | 3904 | ``			 * meridian behind one has an hour to move and a `Y` behind one`` |
|      - | 3905 | `			 * overwrites the year it just wrote. The microseconds are the one` |
|      - | 3906 | `			 * part it does not touch. */` |
|     25 | 3907 | `			const char *zNrErr = 0;` |
|      - | 3908 | `			Sytm sTm;` |
|     28 | 3909 | `			DT_FF_CHECKSIGNED;` |
|     25 | 3910 | `			DtFfGetSignedNr(&z,zInEnd,24,&uVal,&zNrErr);` |
|     25 | 3911 | `			if( zNrErr ){` |
|      - | 3912 | `				/* php reports these at position 0 and takes the zero anyway */` |
|      5 | 3913 | `				DT_FF_LOGERR(0,zNrErr);` |
|      2 | 3914 | `			}` |
|     25 | 3915 | `			DtFillSytm(uVal,0,0,&sTm);` |
|     25 | 3916 | `			y = sTm.tm_year; mo = sTm.tm_mon + 1; d = sTm.tm_mday;` |
|     25 | 3917 | `			h = sTm.tm_hour; mi = sTm.tm_min; s = sTm.tm_sec;` |
|     25 | 3918 | `			bLocal = 1;` |
|     25 | 3919 | `			iOffKind = DT_ZONE_OFFSET;` |
|     25 | 3920 | `			iOffVal = 0;` |
|     25 | 3921 | `			zName = 0;` |
|     25 | 3922 | `			nName = 0;` |
|     25 | 3923 | `			break;` |
|      - | 3924 | `				 }` |
|     75 | 3925 | `		case 'e': case 'T': case 'P': case 'p': case 'O':` |
|      - | 3926 | `			/* php's five zone specifiers are ONE rule, and it is the whole of` |
|      - | 3927 | `			 * timelib_parse_zone rather than the shape each letter is named` |
|      - | 3928 | ``			 * after: `O` reads `UTC` and `e` reads `+02:00`. The zone is LOCAL`` |
|      - | 3929 | `			 * from here whatever the answer -- only the KIND is left at zero` |
|      - | 3930 | `			 * when the name meant nothing. */` |
|    152 | 3931 | `			bLocal = 1;` |
|    152 | 3932 | `			if( !DtFfZone(&z,zInEnd,&iOffKind,&iOffVal,&zName,&nName) ){` |
|     43 | 3933 | `				zErr = "The timezone could not be found in the database";` |
|     21 | 3934 | `			}` |
|    152 | 3935 | `			break;` |
|      1 | 3936 | `		case '?':` |
|      3 | 3937 | `			z++;` |
|      3 | 3938 | `			break;` |
|      4 | 3939 | `		case '*':` |
|      - | 3940 | `			/* php's "skip to a separator": one byte goes whatever it is, and the` |
|      - | 3941 | ``			 * run after it stops at a blank, a digit or one of `.,:;/-`. The`` |
|      - | 3942 | `			 * parens are NOT in that set, though every other rule here treats` |
|      - | 3943 | `			 * them as separators. */` |
|      9 | 3944 | `			z++;` |
|     35 | 3945 | `			while( z < zInEnd && z[0] != ' ' && z[0] != '\t' && z[0] != '.'` |
|     22 | 3946 | `			 && z[0] != ',' && z[0] != ':' && z[0] != ';' && z[0] != '/'` |
|     38 | 3947 | `			 && z[0] != '-' && !SyisDigit(z[0]) ){` |
|     23 | 3948 | `				z++;` |
|      1 | 3949 | `			}` |
|      9 | 3950 | `			break;` |
|      4 | 3951 | `		case '#':` |
|      9 | 3952 | `			if( DtFfIsSep((unsigned char)z[0]) ){` |
|      7 | 3953 | `				z++;` |
|      4 | 3954 | `			}else{` |
|      3 | 3955 | `				zErr = "The separation symbol ([;:/.,-]) could not be found";` |
|      - | 3956 | `			}` |
|      9 | 3957 | `			break;` |
|      4 | 3958 | `		case '\\':` |
|      - | 3959 | `			/* the escape takes the NEXT format byte literally, and refuses on` |
|      - | 3960 | `			 * its own account when the format ends before there is one */` |
|      9 | 3961 | `			if( zFmt >= zEnd ){` |
|      3 | 3962 | `				zErr = "Escaped character expected";` |
|      3 | 3963 | `				break;` |
|      - | 3964 | `			}` |
|      7 | 3965 | `			if( z[0] == zFmt[0] ){` |
|      5 | 3966 | `				z++;` |
|      3 | 3967 | `			}else{` |
|      3 | 3968 | `				zErr = "The escaped character could not be found";` |
|      - | 3969 | `			}` |
|      7 | 3970 | `			zFmt++;` |
|      7 | 3971 | `			break;` |
|    152 | 3972 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|      - | 3973 | `		case '(' : case ')':` |
|      - | 3974 | `			/* a separator in the format wants exactly that byte; a mismatch is` |
|      - | 3975 | `			 * ONE refusal, and the input byte stays where it is */` |
|    306 | 3976 | `			if( z[0] == c ){` |
|    298 | 3977 | `				z++;` |
|    150 | 3978 | `			}else{` |
|      9 | 3979 | `				zErr = "The separation symbol could not be found";` |
|      - | 3980 | `			}` |
|    306 | 3981 | `			break;` |
|     80 | 3982 | `		case ' ':` |
|    162 | 3983 | `			DtFfEatSpaces(&z,zInEnd);` |
|    162 | 3984 | `			break;` |
|      9 | 3985 | `		default:` |
|      - | 3986 | `			/* any other format byte must match the input verbatim -- and php` |
|      - | 3987 | `			 * steps over the input byte either way, so a mismatch costs one` |
|      - | 3988 | `			 * refusal and the two strings carry on in step */` |
|     19 | 3989 | `			if( z[0] != c ){` |
|     19 | 3990 | `				DT_FF_LOGERR(iBegin,"The format separator does not match");` |
|      7 | 3991 | `			}` |
|     19 | 3992 | `			z++;` |
|     18 | 3993 | `			break;` |
|      - | 3994 | `		}` |
|   1580 | 3995 | `		if( zErr ){` |
|      - | 3996 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|     69 | 3997 | `			DT_FF_LOGERR(iBegin,zErr);` |
|     33 | 3998 | `		}` |
|      2 | 3999 | `	}` |
|    644 | 4000 | `	if( z < zInEnd ){` |
|     81 | 4001 | `		if( bPlus ){` |
|      - | 4002 | `			/* '+' downgrades trailing data to a warning */` |
|      9 | 4003 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|      9 | 4004 | `			aWarnMsg[nWarn] = "Trailing data";` |
|      9 | 4005 | `			nWarn++;` |
|      5 | 4006 | `		}else{` |
|     87 | 4007 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|      - | 4008 | `		}` |
|     40 | 4009 | `	}` |
|      - | 4010 | `	/* ...and what is left of a FORMAT the input ran out under. The two reset` |
|      - | 4011 | ``	 * specifiers and `+` need no input and are allowed to stand there; the`` |
|      - | 4012 | `	 * first specifier that does want input is one refusal, and the rest of the` |
|      - | 4013 | `	 * format is never looked at. */` |
|   1296 | 4014 | `	while( zFmt < zEnd ){` |
|    683 | 4015 | `		char c = zFmt[0];` |
|    683 | 4016 | `		zFmt++;` |
|    683 | 4017 | `		if( c == '!' ){` |
|     15 | 4018 | `			DT_FF_RESET(0);` |
|    676 | 4019 | `		}else if( c == '\|' ){` |
|    635 | 4020 | `			DT_FF_RESET(1);` |
|    352 | 4021 | `		}else if( c != '+' ){` |
|     41 | 4022 | `			DT_FF_LOGERR((int)(z - zIn),"Not enough data available to satisfy format");` |
|     31 | 4023 | `			break;` |
|      - | 4024 | `		}` |
|      1 | 4025 | `	}` |
|      - | 4026 | `	/* php's own clean-up: naming ANY part of the clock puts the rest of it at` |
|      - | 4027 | `	 * zero, so a format that read only the minute is that minute past midnight` |
|      - | 4028 | `	 * rather than past the current hour. */` |
|    644 | 4029 | `	if( h != DT_UNSET \|\| mi != DT_UNSET \|\| s != DT_UNSET \|\| us != DT_UNSET ){` |
|    446 | 4030 | `		if( h == DT_UNSET ){ h = 0; }` |
|    446 | 4031 | `		if( mi == DT_UNSET ){ mi = 0; }` |
|    446 | 4032 | `		if( s == DT_UNSET ){ s = 0; }` |
|    446 | 4033 | `		if( us == DT_UNSET ){ us = 0; }` |
|    222 | 4034 | `	}` |
|      - | 4035 | `	/* ...and the two validity WARNINGS, each asked only of a whole component` |
|      - | 4036 | `	 * the scan actually filled, at wherever in the input the scan stopped. */` |
|    642 | 4037 | `	if( h != DT_UNSET && mi != DT_UNSET && s != DT_UNSET` |
|    446 | 4038 | `	 && (h < 0 \|\| h > 23 \|\| mi < 0 \|\| mi > 59 \|\| s < 0 \|\| s > 59) ){` |
|     23 | 4039 | `		if( nWarn < PH7_DT_MAX_WARN ){` |
|     23 | 4040 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     23 | 4041 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|     23 | 4042 | `			nWarn++;` |
|     11 | 4043 | `		}` |
|     11 | 4044 | `	}` |
|    642 | 4045 | `	if( y != DT_UNSET && mo != DT_UNSET && d != DT_UNSET` |
|    459 | 4046 | `	 && (mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo)) ){` |
|     23 | 4047 | `		if( nWarn < PH7_DT_MAX_WARN ){` |
|     23 | 4048 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     23 | 4049 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|     23 | 4050 | `			nWarn++;` |
|     11 | 4051 | `		}` |
|     11 | 4052 | `	}` |
|    644 | 4053 | `	pOut->y = y; pOut->mo = mo; pOut->d = d;` |
|    644 | 4054 | `	pOut->h = h; pOut->mi = mi; pOut->s = s; pOut->us = us;` |
|    644 | 4055 | `	pOut->iOff = iOffVal;` |
|    644 | 4056 | `	pOut->bLocal = bLocal;` |
|    644 | 4057 | `	pOut->iOffKind = iOffKind;` |
|    644 | 4058 | `	pOut->zName = zName;` |
|    644 | 4059 | `	pOut->nName = nName;` |
|    644 | 4060 | `	pOut->bWday = bWday;` |
|    644 | 4061 | `	pOut->iWday = iWday;` |
|    644 | 4062 | `	DtFfDiag(&pOut->sDiag,nErr,nErrKept,aErrPos,aErrMsg,nWarn,aWarnPos,aWarnMsg);` |
|    644 | 4063 | `	return nErr > 0 ? -1 : 0;` |
|      - | 4064 | `#undef DT_FF_CHECKSIGNED` |
|      - | 4065 | `#undef DT_FF_CHECKNUM` |
|      - | 4066 | `#undef DT_FF_RESET` |
|      - | 4067 | `#undef DT_FF_LOGERR` |
|      2 | 4068 | `}` |
|      - | 4069 | `/*` |
|      - | 4070 | ` * php's timelib_fill_holes and timelib_update_ts, the step between the scan` |
|      - | 4071 | ` * above and the moment a DateTime carries.` |
|      - | 4072 | ` *` |
|      - | 4073 | ` * Everything the format never named comes from the current instant -- in the` |
|      - | 4074 | `` * zone the call was given, because php builds its `now` there -- and the`` |
|      - | 4075 | ` * relative WEEKDAY a textual day left behind moves the resulting date forward` |
|      - | 4076 | ` * to that weekday, a day that already matches counting as a match.` |
|      - | 4077 | ` */` |
|    356 | 4078 | `static sxi64 DtFfResolve(const dt_ff_res *pRes,sxi64 iNow,int iNowUs,` |
|      - | 4079 | `	sxi32 iDefOff,int *piUs)` |
|      2 | 4080 | `{` |
|    358 | 4081 | `	sxi64 y = pRes->y,mo = pRes->mo,d = pRes->d;` |
|    358 | 4082 | `	sxi64 h = pRes->h,mi = pRes->mi,s = pRes->s,us = pRes->us;` |
|    358 | 4083 | `	sxi64 iLocal = iNow + iDefOff;` |
|    358 | 4084 | `	sxi64 days = DtFloorDiv(iLocal,86400);` |
|    358 | 4085 | `	sxi64 secs = iLocal - days*86400;` |
|      - | 4086 | `	sxi64 ny;` |
|      - | 4087 | `	int nmo,nd;` |
|    358 | 4088 | `	DtCivilFromDays(days,&ny,&nmo,&nd);` |
|    358 | 4089 | `	if( us == DT_UNSET ){` |
|      - | 4090 | `		/* php reads the microseconds off the clock only for a format that read` |
|      - | 4091 | `		 * no part of the moment at all. */` |
|     91 | 4092 | `		us = (y != DT_UNSET \|\| mo != DT_UNSET \|\| d != DT_UNSET` |
|     55 | 4093 | `		   \|\| h != DT_UNSET \|\| mi != DT_UNSET \|\| s != DT_UNSET) ? 0 : iNowUs;` |
|     35 | 4094 | `	}` |
|    358 | 4095 | `	if( y == DT_UNSET ){ y = ny; }` |
|    358 | 4096 | `	if( mo == DT_UNSET ){ mo = nmo; }` |
|    358 | 4097 | `	if( d == DT_UNSET ){ d = nd; }` |
|    358 | 4098 | `	if( h == DT_UNSET ){ h = secs / 3600; }` |
|    358 | 4099 | `	if( mi == DT_UNSET ){ mi = (secs / 60) % 60; }` |
|    358 | 4100 | `	if( s == DT_UNSET ){ s = secs % 60; }` |
|      - | 4101 | `	/* php normalizes the filled vector BEFORE it hunts for a weekday and again` |
|      - | 4102 | `	 * afterwards, and its month carry is a CALENDAR one -- the fortieth month` |
|      - | 4103 | `	 * of 1970 is April 1973, not forty thirty-day steps from January. */` |
|    358 | 4104 | `	DtFfNormalize(&y,&mo,&d,&h,&mi,&s,&us);` |
|    358 | 4105 | `	if( pRes->bWday ){` |
|      - | 4106 | `		/* php's forward hunt, and it is a DIFFERENCE rather than a remainder:` |
|      - | 4107 | ``		 * a weekday the relative-unit table answers past six -- `week` is 7 --`` |
|      - | 4108 | `		 * moves the date by that much more. */` |
|     33 | 4109 | `		sxi64 iDays = DtDaysFromCivil(y,(int)mo,1) + (d - 1);` |
|     33 | 4110 | `		sxi64 iDiff = pRes->iWday - DtDowOf(iDays);` |
|     33 | 4111 | `		if( iDiff < 0 ){ iDiff += 7; }` |
|     33 | 4112 | `		d += iDiff;` |
|     33 | 4113 | `		DtFfNormalize(&y,&mo,&d,&h,&mi,&s,&us);` |
|     16 | 4114 | `	}` |
|    358 | 4115 | `	*piUs = (int)us;` |
|    536 | 4116 | `	return DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,` |
|    356 | 4117 | `		pRes->iOffKind != 0 ? pRes->iOff : iDefOff);` |
|      2 | 4118 | `}` |
|      - | 4119 | `/*` |
|      - | 4120 | ` * ---------------------------------------------------------------------------` |
|      - | 4121 | ` * DateTimeZone, DateTime and DateTimeImmutable, declared from C.` |
|      - | 4122 | ` *` |
|      - | 4123 | `` * These three used to be embedded PHP over nine global `__dt_*` thunks, with a`` |
|      - | 4124 | `` * private `trait __DtCoreT` holding the state and the shared half of both date`` |
|      - | 4125 | ` * classes. Every operation therefore crossed C -> PHP -> C and marshalled its` |
|      - | 4126 | ` * answer through a throwaway PHP array. The bodies below call the same routines` |
|      - | 4127 | ` * directly; the thunks, the trait and their chunk classes are gone.` |
|      - | 4128 | ` *` |
|      - | 4129 | `` * The instance state is unchanged, so `clone`, `serialize` and `var_dump` see what`` |
|      - | 4130 | `` * they always saw (minus the `__DtCoreT` declaring-class name): four private slots`` |
|      - | 4131 | ` * on each date class, two on DateTimeZone. Native traits do not exist, so the` |
|      - | 4132 | `` * shared method table is simply installed on both classes -- which is what `use`` |
|      - | 4133 | `` * __DtCoreT` did anyway.`` |
|      - | 4134 | ` * ---------------------------------------------------------------------------` |
|      - | 4135 | ` */` |
|      - | 4136 | `#define DT_TS    "__dtTs"` |
|      - | 4137 | `#define DT_OFF   "__dtOff"` |
|      - | 4138 | `#define DT_NAME  "__dtName"` |
|      - | 4139 | `#define DT_US    "__dtUs"` |
|      - | 4140 | `#define DTZ_OFF  "__dtzOff"` |
|      - | 4141 | `#define DTZ_NAME "__dtzName"` |
|      - | 4142 | `/*` |
|      - | 4143 | ` * php's timezone_type -- 1 = a fixed UTC OFFSET, 2 = an ABBREVIATION, 3 = an` |
|      - | 4144 | ` * IDENTIFIER -- STORED beside the name rather than read back off it, because the` |
|      - | 4145 | ``  * name does not carry it: `new DateTimeZone('utc')` and `new DateTimeZone('UTC')` `` |
|      - | 4146 | ` * are both named "UTC" there and are an abbreviation and an identifier` |
|      - | 4147 | ` * respectively, which is what makes them refuse to compare with each other. The` |
|      - | 4148 | ` * date objects keep their own copy (DT_ZKIND) for the same reason: php presents a` |
|      - | 4149 | ` * DateTime built with the lowercase zone as type 2.` |
|      - | 4150 | ` *` |
|      - | 4151 | ` * DtZoneTypeOf() remains the rule for a name that arrives with NO kind -- php's` |
|      - | 4152 | ` * own __unserialize re-derives it that way, which is why a serialized type-2 "UTC"` |
|      - | 4153 | ` * comes back as type 3 in both engines.` |
|      - | 4154 | ` */` |
|      - | 4155 | `#define DTZ_KIND "__dtzKind"` |
|      - | 4156 | `#define DT_ZKIND "__dtZKind"` |
|      - | 4157 | `/*` |
|      - | 4158 | ` * Has this object been CONSTRUCTED?` |
|      - | 4159 | ` *` |
|      - | 4160 | ` * php keeps its date state in a C struct hanging off the object and allocates it` |
|      - | 4161 | ` * in the constructor, so an object that never ran one -- what` |
|      - | 4162 | `` * `newInstanceWithoutConstructor()` answers, and what a subclass whose own`` |
|      - | 4163 | `` * constructor forgets `parent::__construct()` IS -- has no state at all, and every`` |
|      - | 4164 | `` * door raises `DateObjectError` rather than reading it. PHL's state lives in`` |
|      - | 4165 | ` * ordinary (hidden) slots, which are there from instantiation and hold their` |
|      - | 4166 | ` * declared defaults, so such an object silently WAS 1970-01-01 UTC.` |
|      - | 4167 | ` *` |
|      - | 4168 | ` * This is that struct's presence, as the one thing a slot can carry: zero until` |
|      - | 4169 | ` * some constructor -- or one of the C factories, which build a complete object` |
|      - | 4170 | ` * without running one -- says otherwise. Every class in the family declares it,` |
|      - | 4171 | ` * every method reaches it through DtThis(), and a clone inherits it the way php's` |
|      - | 4172 | ` * cloned struct does.` |
|      - | 4173 | ` */` |
|      - | 4174 | `#define DT_INIT  "__dtInit"` |
|      - | 4175 | `/* The kind the script DEFAULT zone has, and it is not the name's own rule:` |
|      - | 4176 | ` * date_default_timezone_set() takes a tz-database IDENTIFIER and nothing else, so` |
|      - | 4177 | `` * php reports a date built under a `GMT` default as type 3 while`` |
|      - | 4178 | `` * `new DateTimeZone('GMT')` -- the same three letters spelled as a zone -- is the`` |
|      - | 4179 | ` * abbreviation, type 2. */` |
|      - | 4180 | `#define DT_ZONE_DEFAULT_KIND DT_ZONE_ID` |
|      - | 4181 | `/*` |
|      - | 4182 | ` * ---------------------------------------------------------------------------` |
|      - | 4183 | ` * A DateInterval's MICROSECONDS.` |
|      - | 4184 | ` *` |
|      - | 4185 | ` * php stores them as an int64 COUNT (timelib_rel_time.us) and shows that count` |
|      - | 4186 | ` * divided by a million, so the float is a rendering and the integer is the` |
|      - | 4187 | `` * value: `$i->f = 0.1234567` reads back 0.123456 because the write truncated to`` |
|      - | 4188 | `` * 123456 microseconds, and `f` is what diff() fills, what add()/sub() move the`` |
|      - | 4189 | ` * clock by, and what format()'s %f prints.` |
|      - | 4190 | ` *` |
|      - | 4191 | `` * PHL's `f` is a real property slot a script reads directly, so the count lives`` |
|      - | 4192 | ` * beside it in a hidden one. The two are written together by every door that` |
|      - | 4193 | ` * owns the value (the write handler, diff, the constructors); a write that` |
|      - | 4194 | ` * arrives from somewhere else — unserialize's raw property store, or one of the` |
|      - | 4195 | `` * Recorded shapes php answers with a temporary — leaves only `f` behind, so the`` |
|      - | 4196 | ` * count is trusted only while it still RENDERS to the float on show, and is` |
|      - | 4197 | ` * re-derived from the float when it does not.` |
|      - | 4198 | ` * ---------------------------------------------------------------------------` |
|      - | 4199 | ` */` |
|      - | 4200 | `#define DT_IV_US "__ivUs"` |
|      - | 4201 | ``/* php's conversion of the `f` property to its stored count, cast contract and`` |
|      - | 4202 | ` * all: it TRUNCATES toward zero, WRAPS what no int64 can hold, and answers 0 for` |
|      - | 4203 | ` * a NaN or an infinity. */` |
|    508 | 4204 | `static sxi64 DtIvUsecOfReal(double r)` |
|      2 | 4205 | `{` |
|    510 | 4206 | `	return PH7_RealToInt64(r * 1000000.0);` |
|      2 | 4207 | `}` |
|      - | 4208 | `/* The interval's microseconds. */` |
|   1016 | 4209 | `static sxi64 DtIvUsec(ph7_class_instance *pIv)` |
|      2 | 4210 | `{` |
|   1018 | 4211 | `	ph7_value *pF = PH7_NativeAttr(pIv,"f");` |
|   1018 | 4212 | `	sxi64 us = PH7_NativeAttrInt(pIv,DT_IV_US);` |
|   1018 | 4213 | `	double r = 0.0;` |
|   1018 | 4214 | `	if( pF && (pF->iFlags & MEMOBJ_REAL) ){` |
|   1018 | 4215 | `		r = (double)pF->rVal;` |
|    508 | 4216 | `	}else if( pF && (pF->iFlags & MEMOBJ_INT) ){` |
|    ! 0 | 4217 | `		r = (double)pF->x.iVal;` |
|    ! 0 | 4218 | `	}` |
|   1018 | 4219 | `	if( (double)us / 1000000.0 == r ){` |
|   1018 | 4220 | ``		return us;   /* the count `f` was rendered from: exact past 2^53, where the float is not */`` |
|      - | 4221 | `	}` |
|    ! 0 | 4222 | `	return DtIvUsecOfReal(r);` |
|    510 | 4223 | `}` |
|      - | 4224 | `/* Store a microsecond count and the float php shows for it -- the two halves of` |
|      - | 4225 | ` * the same value, written together by every door that owns it. */` |
|    614 | 4226 | `static void DtIvSetUsec(ph7_vm *pVm,ph7_class_instance *pIv,sxi64 us)` |
|      3 | 4227 | `{` |
|    617 | 4228 | `	PH7_NativeSetAttrInt(pVm,pIv,DT_IV_US,us);` |
|    617 | 4229 | `	PH7_NativeSetAttrReal(pVm,pIv,"f",(ph7_real)((double)us / 1000000.0));` |
|    617 | 4230 | `}` |
|      - | 4231 | `/* One date object's state, as the bodies below pass it around. */` |
|      - | 4232 | `typedef struct dt_state dt_state;` |
|      - | 4233 | `struct dt_state` |
|      - | 4234 | `{` |
|      - | 4235 | `	sxi64 iTs;` |
|      - | 4236 | `	sxi32 iOff;` |
|      - | 4237 | `	int uSec;` |
|      - | 4238 | `	const char *zName;   /* borrowed from the instance's own slot */` |
|      - | 4239 | `	int nName;` |
|      - | 4240 | `	int iZoneKind;       /* php's timezone_type: DT_ZONE_OFFSET / _ABBR / _ID */` |
|      - | 4241 | `};` |
|      - | 4242 | `/* php's name for a fixed offset: "+HH:MM" (and "+00:00" for zero, never "-00:00"). */` |
|   1358 | 4243 | `static int DtOffName(char *zBuf,sxu32 nBuf,sxi32 iOff)` |
|      4 | 4244 | `{` |
|   1362 | 4245 | `	sxi32 a = iOff < 0 ? -iOff : iOff;` |
|   2041 | 4246 | `	return (int)SyBufferFormat(zBuf,nBuf,"%c%02d:%02d",` |
|   1358 | 4247 | `		iOff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|      4 | 4248 | `}` |
|      - | 4249 | `/*` |
|      - | 4250 | ` * The same name with php's SECONDS field, which it appends only when there is` |
|      - | 4251 | `` * one: `new DateTimeZone('+01:00:59')` is named "+01:00:59" and answers that to`` |
|      - | 4252 | `` * getName() and to format('e'), while `P`, `p`, `O` and `T` -- built from`` |
|      - | 4253 | ` * DtOffName above -- still stop at the minute there. So the two spellings are` |
|      - | 4254 | ` * separate on purpose.` |
|      - | 4255 | ` */` |
|   1342 | 4256 | `static int DtOffNameSec(char *zBuf,sxu32 nBuf,sxi32 iOff)` |
|      4 | 4257 | `{` |
|   1346 | 4258 | `	sxi32 a = iOff < 0 ? -iOff : iOff;` |
|      - | 4259 | `	/* php renders this into a buffer sized for its own example -- "+05:00" or` |
|      - | 4260 | `	 * "+05:00:01" -- so an offset whose hours want three digits comes back CUT.` |
|      - | 4261 | `	 * Only a format can build one: every other door caps the offset below 100` |
|      - | 4262 | ``	 * hours, and `e` on `+9999` is 100 hours 39 minutes, named "+100:3". */`` |
|   1346 | 4263 | `	int nMax = (a % 60 == 0) ? 6 : 9;` |
|      - | 4264 | `	int n;` |
|   1346 | 4265 | `	if( a % 60 == 0 ){` |
|   1294 | 4266 | `		n = DtOffName(zBuf,nBuf,iOff);` |
|    649 | 4267 | `	}else{` |
|     53 | 4268 | `		n = (int)SyBufferFormat(zBuf,nBuf,"%c%02d:%02d:%02d",` |
|     52 | 4269 | `			iOff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60),(int)(a % 60));` |
|      - | 4270 | `	}` |
|   1346 | 4271 | `	if( n > nMax ){` |
|      - | 4272 | `		/* php's snprintf CUTS the text at the buffer and still answers the` |
|      - | 4273 | `		 * length it WANTED, so the name a script reads back carries php's own` |
|      - | 4274 | ``		 * terminator inside it: `+9999` is the seven bytes "+100:3\0". */`` |
|      3 | 4275 | `		zBuf[nMax] = 0;` |
|      3 | 4276 | `		if( n > nMax + 1 ){ n = nMax + 1; }` |
|      1 | 4277 | `	}` |
|   1346 | 4278 | `	return n;` |
|      4 | 4279 | `}` |
|      - | 4280 | `/*` |
|      - | 4281 | ` * php's timezone_type read off a NAME alone -- the fallback for a zone that` |
|      - | 4282 | `` * reached the engine without one: a payload `__unserialize()` re-parses (php`` |
|      - | 4283 | ` * re-derives there too, which is why a serialized type-2 "UTC" comes back a` |
|      - | 4284 | ` * type 3 in both engines), and an object whose slots are still at their` |
|      - | 4285 | ` * defaults. Everywhere a SPELLING was seen, the kind stored with it wins --` |
|      - | 4286 | ` * "UTC" and "utc" are one name and two kinds.` |
|      - | 4287 | ` *` |
|      - | 4288 | ` * 1 = a fixed UTC OFFSET ("+02:00"), 2 = an ABBREVIATION ("GMT", "Z"),` |
|      - | 4289 | ` * 3 = an IDENTIFIER ("UTC", "Europe/Paris"). PHL accepts offsets, UTC, GMT and Z` |
|      - | 4290 | ` * today; the identifier arm is written for the whole rule so a tz database can` |
|      - | 4291 | ` * only add names, never change the tagging.` |
|      - | 4292 | ` */` |
|    ! 0 | 4293 | `static int DtZoneTypeOf(const char *zName,int nName)` |
|    ! 0 | 4294 | `{` |
|    ! 0 | 4295 | `	sxu32 nPos = 0;` |
|    ! 0 | 4296 | `	if( nName > 0 && (zName[0] == '+' \|\| zName[0] == '-') ){` |
|    ! 0 | 4297 | `		return DT_ZONE_OFFSET;` |
|      - | 4298 | `	}` |
|    ! 0 | 4299 | `	if( nName == 3 && SyMemcmp(zName,"UTC",3) == 0 ){` |
|    ! 0 | 4300 | `		return DT_ZONE_ID;` |
|      - | 4301 | `	}` |
|    ! 0 | 4302 | `	if( nName > 0 && SyByteFind(zName,(sxu32)nName,'/',&nPos) == SXRET_OK ){` |
|    ! 0 | 4303 | `		return DT_ZONE_ID;` |
|      - | 4304 | `	}` |
|    ! 0 | 4305 | `	return DT_ZONE_ABBR;` |
|    ! 0 | 4306 | `}` |
|      - | 4307 | `/* The kind an instance carries in zSlot, or the name's own rule when the slot is` |
|      - | 4308 | ` * still zero -- an object built by newInstanceWithoutConstructor, or one whose` |
|      - | 4309 | ` * state predates the slot. */` |
|   9254 | 4310 | `static int DtZoneKindOf(ph7_class_instance *pObj,const char *zSlot,const char *zName,int nName)` |
|      5 | 4311 | `{` |
|   9259 | 4312 | `	int iKind = (int)PH7_NativeAttrInt(pObj,zSlot);` |
|   9259 | 4313 | `	if( iKind < DT_ZONE_OFFSET \|\| iKind > DT_ZONE_ID ){` |
|    ! 0 | 4314 | `		return DtZoneTypeOf(zName ? zName : "",nName);` |
|      - | 4315 | `	}` |
|   9259 | 4316 | `	return iKind;` |
|   4632 | 4317 | `}` |
|      - | 4318 | `/*` |
|      - | 4319 | ` * The database index of the SCRIPT's default zone, or -1.` |
|      - | 4320 | ` *` |
|      - | 4321 | ` * date_default_timezone_set() takes an identifier and nothing else, so the` |
|      - | 4322 | ` * default is always kind 3 and this is DtTzIndex() with that filled in. It is` |
|      - | 4323 | ` * exported because the procedural doors -- date(), mktime(), strtotime() and` |
|      - | 4324 | ` * the rest, which live in builtin_date.c -- have to ask the same question, and` |
|      - | 4325 | ` * the answer must not be spelled twice.` |
|      - | 4326 | ` */` |
|   1848 | 4327 | `PH7_PRIVATE int DtDefaultTzIndex(ph7_vm *pVm)` |
|      5 | 4328 | `{` |
|   1853 | 4329 | `	return DtTzIndex(pVm->zDefTz,(int)pVm->nDefTz,DT_ZONE_ID);` |
|      5 | 4330 | `}` |
|      - | 4331 | `/* DtTzOffsetAt() under an exported name, for the same callers. */` |
|   1090 | 4332 | `PH7_PRIVATE sxi32 DtTzOffsetOf(int iTz,sxi32 iFixed,sxi64 iTs,int *pbDst,` |
|      - | 4333 | `	const char **pzAbbr,int *pnAbbr)` |
|      4 | 4334 | `{` |
|   1094 | 4335 | `	return DtTzOffsetAt(iTz,iFixed,iTs,pbDst,pzAbbr,pnAbbr);` |
|      4 | 4336 | `}` |
|      - | 4337 | `/* The same question asked of an instance's own name/kind slots. */` |
|   2460 | 4338 | `static int DtTzIndexOf(ph7_class_instance *pObj,const char *zNameSlot,const char *zKindSlot)` |
|      3 | 4339 | `{` |
|      - | 4340 | `	const char *zName;` |
|      - | 4341 | `	int nName;` |
|   2463 | 4342 | `	PH7_NativeAttrStr(pObj,zNameSlot,&zName,&nName);` |
|   2463 | 4343 | `	return DtTzIndex(zName,nName,DtZoneKindOf(pObj,zKindSlot,zName,nName));` |
|      3 | 4344 | `}` |
|   4480 | 4345 | `static void DtLoad(ph7_class_instance *pObj,dt_state *pOut)` |
|      4 | 4346 | `{` |
|   4484 | 4347 | `	pOut->iTs  = PH7_NativeAttrInt(pObj,DT_TS);` |
|   4484 | 4348 | `	pOut->iOff = (sxi32)PH7_NativeAttrInt(pObj,DT_OFF);` |
|   4484 | 4349 | `	pOut->uSec = (int)PH7_NativeAttrInt(pObj,DT_US);` |
|   4484 | 4350 | `	PH7_NativeAttrStr(pObj,DT_NAME,&pOut->zName,&pOut->nName);` |
|   4484 | 4351 | `	pOut->iZoneKind = DtZoneKindOf(pObj,DT_ZKIND,pOut->zName,pOut->nName);` |
|   4484 | 4352 | `}` |
|   3796 | 4353 | `static void DtStore(ph7_vm *pVm,ph7_class_instance *pObj,const dt_state *pIn)` |
|      4 | 4354 | `{` |
|   3800 | 4355 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_TS,pIn->iTs);` |
|   3800 | 4356 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,pIn->iOff);` |
|   3800 | 4357 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_US,pIn->uSec);` |
|   3800 | 4358 | `	PH7_NativeSetAttrStr(pVm,pObj,DT_NAME,pIn->zName,pIn->nName);` |
|   3800 | 4359 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_ZKIND,pIn->iZoneKind);` |
|   3800 | 4360 | `}` |
|      - | 4361 | `/*` |
|      - | 4362 | ` * Put DT_OFF back in step with DT_TS.` |
|      - | 4363 | ` *` |
|      - | 4364 | ` * A fixed zone's offset does not depend on the instant, so this was never` |
|      - | 4365 | ` * needed and DT_OFF could be written once and left. A DATABASE zone's does, so` |
|      - | 4366 | ` * every door that moves the timestamp on its own -- setTimestamp(), setDate(),` |
|      - | 4367 | ` * setTime(), modify(), add()/sub(), the period walker -- has to say so, and` |
|      - | 4368 | ` * they write DT_TS directly rather than through DtStore(). This is the one` |
|      - | 4369 | ` * call each of them owes; with a fixed zone it reads the name, finds no` |
|      - | 4370 | ` * database row and changes nothing.` |
|      - | 4371 | ` *` |
|      - | 4372 | ` * It re-derives the offset from the INSTANT, which is right for a door that` |
|      - | 4373 | ` * names an instant (setTimestamp) and is only half the answer for one that` |
|      - | 4374 | ` * names a WALL CLOCK -- php's setTime() and modify() work in local time and` |
|      - | 4375 | ` * have to re-solve the reading, which is a separate matter from the offset` |
|      - | 4376 | ` * being stale.` |
|      - | 4377 | ` */` |
|    376 | 4378 | `static void DtRezone(ph7_vm *pVm,ph7_class_instance *pObj)` |
|      2 | 4379 | `{` |
|    378 | 4380 | `	int iTz = DtTzIndexOf(pObj,DT_NAME,DT_ZKIND);` |
|      - | 4381 | `	int bDst,nAbbr;` |
|      - | 4382 | `	const char *zAbbr;` |
|      - | 4383 | `	sxi32 iOff;` |
|    378 | 4384 | `	if( iTz < 0 ){` |
|    356 | 4385 | `		return;` |
|      - | 4386 | `	}` |
|     34 | 4387 | `	iOff = DtTzOffsetAt(iTz,(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),` |
|     11 | 4388 | `		PH7_NativeAttrInt(pObj,DT_TS),&bDst,&zAbbr,&nAbbr);` |
|     23 | 4389 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,iOff);` |
|    190 | 4390 | `}` |
|      - | 4391 | `/*` |
|      - | 4392 | ` * modify()'s one zone rule. php copies the parsed FIELDS into the object and` |
|      - | 4393 | ` * leaves its zone alone -- so a modifier that names a zone moves nothing -- with` |
|      - | 4394 | `` * `@epoch` the single exception: that form names an absolute instant, and php`` |
|      - | 4395 | `` * re-zones the object to the fixed `+00:00` along with it. Every other modifier`` |
|      - | 4396 | ` * leaves this a no-op.` |
|      - | 4397 | ` */` |
|   1138 | 4398 | `static void DtEpochRezone(ph7_vm *pVm,ph7_class_instance *pObj,const dt_parsed *pVec)` |
|      3 | 4399 | `{` |
|      - | 4400 | `	char zBuf[16];` |
|      - | 4401 | `	int nName;` |
|   1141 | 4402 | `	if( !pVec->bEpoch ){` |
|   1121 | 4403 | `		return;` |
|      - | 4404 | `	}` |
|     21 | 4405 | `	nName = DtOffName(zBuf,sizeof(zBuf),0);` |
|     21 | 4406 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,0);` |
|     21 | 4407 | `	PH7_NativeSetAttrStr(pVm,pObj,DT_NAME,zBuf,nName);` |
|     21 | 4408 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_ZKIND,DT_ZONE_OFFSET);` |
|    572 | 4409 | `}` |
|  19570 | 4410 | `static ph7_class * DtClass(ph7_vm *pVm,const char *zName)` |
|      5 | 4411 | `{` |
|  19575 | 4412 | `	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);` |
|      5 | 4413 | `}` |
|      - | 4414 | `/* Is this instance an instance of the named date class? */` |
|    538 | 4415 | `static int DtIsA(ph7_vm *pVm,ph7_class_instance *pObj,const char *zClass)` |
|      2 | 4416 | `{` |
|      - | 4417 | `	ph7_class *pClass;` |
|    540 | 4418 | `	if( pObj == 0 ){` |
|      3 | 4419 | `		return 0;` |
|      - | 4420 | `	}` |
|    538 | 4421 | `	pClass = DtClass(&(*pVm),zClass);` |
|    538 | 4422 | `	return pClass != 0 && PH7_VmInstanceOf(pObj->pClass,pClass);` |
|    271 | 4423 | `}` |
|      - | 4424 | `/* Has this object run a constructor (or been built whole by a C factory)? */` |
|  14016 | 4425 | `static int DtIsInit(ph7_class_instance *pObj)` |
|      5 | 4426 | `{` |
|  14021 | 4427 | `	return pObj != 0 && PH7_NativeAttrInt(pObj,DT_INIT) != 0;` |
|      5 | 4428 | `}` |
|      - | 4429 | `/* Say so. Called by every constructor that SUCCEEDS -- a failing one leaves the` |
|      - | 4430 | ` * object as it found it, which is php's answer too: an object whose` |
|      - | 4431 | `` * `__construct()` threw is still an uninitialized one. */`` |
|   7096 | 4432 | `static void DtSetInit(ph7_vm *pVm,ph7_class_instance *pObj)` |
|      5 | 4433 | `{` |
|   7101 | 4434 | `	PH7_NativeSetAttrInt(&(*pVm),pObj,DT_INIT,1);` |
|   7101 | 4435 | `}` |
|      - | 4436 | `/* A date object built from C rather than by a constructor: complete on arrival, so` |
|      - | 4437 | ` * it is born initialized. Every factory in this file goes through here. */` |
|   1092 | 4438 | `static ph7_class_instance * DtNewInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      4 | 4439 | `{` |
|   1096 | 4440 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|   1096 | 4441 | `	if( pObj ){` |
|   1096 | 4442 | `		DtSetInit(&(*pVm),pObj);` |
|    546 | 4443 | `	}` |
|   1096 | 4444 | `	return pObj;` |
|      4 | 4445 | `}` |
|      - | 4446 | `/*` |
|      - | 4447 | ` * php's DateObjectError, worded for the object it is raised on: the class's own` |
|      - | 4448 | ` * name, and -- for a SUBCLASS -- the internal class it inherits, whatever the` |
|      - | 4449 | `` * depth of the chain (`class B extends A extends DateTime` reports`` |
|      - | 4450 | ` * "B (inheriting DateTime)").` |
|      - | 4451 | ` */` |
|    110 | 4452 | `static const char * DtNativeBase(ph7_vm *pVm,ph7_class_instance *pObj)` |
|      1 | 4453 | `{` |
|      - | 4454 | `	static const char * const azBase[] = {` |
|      - | 4455 | `		"DateTime","DateTimeImmutable","DateTimeZone","DateInterval","DatePeriod"` |
|      - | 4456 | `	};` |
|      - | 4457 | `	sxu32 n;` |
|    225 | 4458 | `	for( n = 0 ; n < SX_ARRAYSIZE(azBase) ; ++n ){` |
|    225 | 4459 | `		if( DtIsA(&(*pVm),pObj,azBase[n]) ){` |
|    111 | 4460 | `			return azBase[n];` |
|      - | 4461 | `		}` |
|     58 | 4462 | `	}` |
|    ! 0 | 4463 | `	return 0;` |
|     56 | 4464 | `}` |
|    110 | 4465 | `static int DtThrowUninit(ph7_context *pCtx,ph7_class_instance *pObj)` |
|      1 | 4466 | `{` |
|    111 | 4467 | `	const char *zBase = DtNativeBase(pCtx->pVm,pObj);` |
|    111 | 4468 | `	SyString *pName = &pObj->pClass->sName;` |
|    110 | 4469 | `	if( zBase == 0` |
|    111 | 4470 | `	 \|\| (pName->nByte == SyStrlen(zBase) && SyMemcmp(pName->zString,zBase,pName->nByte) == 0) ){` |
|    151 | 4471 | `		return PH7_VmThrowException(pCtx,"DateObjectError",` |
|      - | 4472 | `			"Object of type %z has not been correctly initialized by calling "` |
|     50 | 4473 | `			"parent::__construct() in its constructor",pName);` |
|      - | 4474 | `	}` |
|     16 | 4475 | `	return PH7_VmThrowException(pCtx,"DateObjectError",` |
|      - | 4476 | `		"Object of type %z (inheriting %s) has not been correctly initialized by "` |
|      5 | 4477 | `		"calling parent::__construct() in its constructor",pName,zBase);` |
|     56 | 4478 | `}` |
|      - | 4479 | `/*` |
|      - | 4480 | ` * The receiver of a native method, or NULL when the call has no object (which the` |
|      - | 4481 | ` * dispatcher only allows for a static one) -- and NULL as well for an object that` |
|      - | 4482 | ` * was never constructed, whose DateObjectError is raised here.` |
|      - | 4483 | ` *` |
|      - | 4484 | ` * This is the one screen the whole family shares: every method body already treats` |
|      - | 4485 | ` * a null receiver as "nothing to do" and returns PH7_OK, and the raise records the` |
|      - | 4486 | ` * status on the context, which the host-call boundary reports (VmHostFuncThrowRc).` |
|      - | 4487 | ` * The four doors php lets through -- the constructors, __unserialize and __wakeup,` |
|      - | 4488 | ` * which exist to initialize the object, and DatePeriod's two nullable getters --` |
|      - | 4489 | ` * take DtThisRaw() instead.` |
|      - | 4490 | ` */` |
|  15064 | 4491 | `static ph7_class_instance * DtThisRaw(ph7_context *pCtx)` |
|      5 | 4492 | `{` |
|  15069 | 4493 | `	return PH7_ContextThis(pCtx);` |
|      5 | 4494 | `}` |
|   8404 | 4495 | `static ph7_class_instance * DtThis(ph7_context *pCtx)` |
|      5 | 4496 | `{` |
|   8409 | 4497 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|   8409 | 4498 | `	if( pThis == 0 \|\| DtIsInit(pThis) ){` |
|   8341 | 4499 | `		return pThis;` |
|      - | 4500 | `	}` |
|     69 | 4501 | `	DtThrowUninit(pCtx,pThis);` |
|     69 | 4502 | `	return 0;` |
|   4207 | 4503 | `}` |
|      - | 4504 | `/*` |
|      - | 4505 | ` * An object ARGUMENT that must be constructed: php raises the same DateObjectError` |
|      - | 4506 | ` * for a date it is HANDED as for the one it is called on. Answers -1 when it` |
|      - | 4507 | ` * raised; a value that is not an object at all was refused by the declared type` |
|      - | 4508 | ` * upstream, so it passes through.` |
|      - | 4509 | ` */` |
|    912 | 4510 | `static int DtArgInit(ph7_context *pCtx,ph7_value *pArg)` |
|      2 | 4511 | `{` |
|      - | 4512 | `	ph7_class_instance *pObj;` |
|    914 | 4513 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 4514 | `		return 0;` |
|      - | 4515 | `	}` |
|    914 | 4516 | `	pObj = (ph7_class_instance *)pArg->x.pOther;` |
|    914 | 4517 | `	if( DtIsInit(pObj) ){` |
|    900 | 4518 | `		return 0;` |
|      - | 4519 | `	}` |
|     15 | 4520 | `	DtThrowUninit(pCtx,pObj);` |
|     15 | 4521 | `	return -1;` |
|    458 | 4522 | `}` |
|      - | 4523 | `/*` |
|      - | 4524 | ` * The same refusal for the one door that names the parameter's DECLARED type` |
|      - | 4525 | ` * instead of the object's class: php reports DatePeriod's start and end dates as` |
|      - | 4526 | ` * "DateTimeInterface" whatever they really are.` |
|      - | 4527 | ` */` |
|    346 | 4528 | `static int DtArgInitNamed(ph7_context *pCtx,ph7_value *pArg,const char *zName)` |
|      1 | 4529 | `{` |
|    346 | 4530 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0` |
|    271 | 4531 | `	 \|\| DtIsInit((ph7_class_instance *)pArg->x.pOther) ){` |
|    343 | 4532 | `		return 0;` |
|      - | 4533 | `	}` |
|      7 | 4534 | `	PH7_VmThrowException(pCtx,"DateObjectError",` |
|      - | 4535 | `		"Object of type %s has not been correctly initialized by calling "` |
|      2 | 4536 | `		"parent::__construct() in its constructor",zName);` |
|      5 | 4537 | `	return -1;` |
|    174 | 4538 | `}` |
|      - | 4539 | `/* An immutable receiver mutates a COPY; a mutable one mutates itself. That is the` |
|      - | 4540 | ` * only difference between the two classes' method tables, so both share one body. */` |
|   2070 | 4541 | `static int DtIsImmutable(ph7_vm *pVm,ph7_class_instance *pObj)` |
|      3 | 4542 | `{` |
|   2073 | 4543 | `	ph7_class *pImm = DtClass(pVm,"DateTimeImmutable");` |
|   2073 | 4544 | `	return pImm != 0 && PH7_VmInstanceOf(pObj->pClass,pImm);` |
|      3 | 4545 | `}` |
|      - | 4546 | `/*` |
|      - | 4547 | ` * The object a mutator writes: $this itself, or a clone for DateTimeImmutable.` |
|      - | 4548 | ` * Either way the caller returns it, so a mutable method answers the same object` |
|      - | 4549 | `` * php's does (`$d->modify(...) === $d`).`` |
|      - | 4550 | ` */` |
|   1756 | 4551 | `static ph7_class_instance * DtMutTarget(ph7_context *pCtx,ph7_class_instance *pThis,int *pbCopy)` |
|      3 | 4552 | `{` |
|   1759 | 4553 | `	if( DtIsImmutable(pCtx->pVm,pThis) ){` |
|     90 | 4554 | `		*pbCopy = 1;` |
|     90 | 4555 | `		return PH7_CloneClassInstance(pThis);` |
|      - | 4556 | `	}` |
|   1671 | 4557 | `	*pbCopy = 0;` |
|   1671 | 4558 | `	return pThis;` |
|    881 | 4559 | `}` |
|      - | 4560 | `/* Return a mutator's target the way php returns it: the clone (whose reference we` |
|      - | 4561 | ` * own) or the receiver itself (whose value the context already holds). */` |
|   1756 | 4562 | `static void DtMutResult(ph7_context *pCtx,ph7_class_instance *pTarget,int bCopy)` |
|      3 | 4563 | `{` |
|   1759 | 4564 | `	if( bCopy ){` |
|     90 | 4565 | `		PH7_NativeResultObject(pCtx,pTarget);` |
|     46 | 4566 | `	}else{` |
|   1671 | 4567 | `		ph7_result_value(pCtx,PH7_ContextThisValue(pCtx));` |
|      - | 4568 | `	}` |
|   1759 | 4569 | `}` |
|      - | 4570 | `/* Read a DateTimeZone argument's two slots. php's ext/date reads its own internal` |
|      - | 4571 | ` * timezone struct here, so an overridden getName()/getOffset() is ignored by both` |
|      - | 4572 | ` * engines. Answers 0 when the value is not a DateTimeZone at all. */` |
|   1334 | 4573 | `static int DtZoneOf(ph7_value *pArg,sxi32 *piOff,const char **pzName,int *pnName,int *piKind)` |
|      3 | 4574 | `{` |
|      - | 4575 | `	ph7_class_instance *pObj;` |
|   1337 | 4576 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 4577 | `		return 0;` |
|      - | 4578 | `	}` |
|   1337 | 4579 | `	pObj = (ph7_class_instance *)pArg->x.pOther;` |
|   1337 | 4580 | `	if( PH7_NativeAttr(pObj,DTZ_NAME) == 0 ){` |
|    ! 0 | 4581 | `		return 0;` |
|      - | 4582 | `	}` |
|   1337 | 4583 | `	if( !DtIsInit(pObj) ){` |
|      - | 4584 | `		/* php reads the zone's C struct here too, and an unconstructed one has` |
|      - | 4585 | `		 * none: its timelib fallback is a fixed UTC OFFSET, which is why` |
|      - | 4586 | ``		 * `$d->setTimezone($uninitialized)` answers "+00:00" rather than raising.`` |
|      - | 4587 | `		 * The doors that BUILD a date from a zone refuse instead -- see` |
|      - | 4588 | `		 * DtZoneArgInit(), which they call first. */` |
|      5 | 4589 | `		*piOff = 0;` |
|      5 | 4590 | `		*pzName = "+00:00";` |
|      5 | 4591 | `		*pnName = (int)sizeof("+00:00") - 1;` |
|      5 | 4592 | `		*piKind = DT_ZONE_OFFSET;` |
|      5 | 4593 | `		return 1;` |
|      - | 4594 | `	}` |
|   1333 | 4595 | `	*piOff = (sxi32)PH7_NativeAttrInt(pObj,DTZ_OFF);` |
|   1333 | 4596 | `	PH7_NativeAttrStr(pObj,DTZ_NAME,pzName,pnName);` |
|   1333 | 4597 | `	*piKind = DtZoneKindOf(pObj,DTZ_KIND,*pzName,*pnName);` |
|   1333 | 4598 | `	return 1;` |
|    670 | 4599 | `}` |
|      - | 4600 | `/*` |
|      - | 4601 | ` * The zone argument of a door that INITIALIZES a date from it -- the two` |
|      - | 4602 | `` * constructors, `date_create()` and `createFromFormat()`. php refuses an`` |
|      - | 4603 | ` * unconstructed zone there, and with a different sentence and a different class` |
|      - | 4604 | `` * from every other uninitialized-object refusal in the family: a plain `Error`,`` |
|      - | 4605 | ` * naming no method. Answers -1 when it raised.` |
|      - | 4606 | ` */` |
|    998 | 4607 | `static int DtZoneArgInit(ph7_context *pCtx,ph7_value *pArg)` |
|      2 | 4608 | `{` |
|    998 | 4609 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0` |
|   1000 | 4610 | `	 \|\| DtIsInit((ph7_class_instance *)pArg->x.pOther) ){` |
|    992 | 4611 | `		return 0;` |
|      - | 4612 | `	}` |
|      9 | 4613 | `	PH7_VmThrowException(pCtx,"Error",` |
|      - | 4614 | `		"The DateTimeZone object has not been correctly initialized by its constructor");` |
|      9 | 4615 | `	return -1;` |
|    501 | 4616 | `}` |
|      - | 4617 | `/*` |
|      - | 4618 | ` * Write a WALL-CLOCK reading -- seconds since the epoch as the zone's own clock` |
|      - | 4619 | ` * shows them -- into an object's timestamp, and put DT_OFF in step with it.` |
|      - | 4620 | ` *` |
|      - | 4621 | ` * For a fixed zone this is one subtraction, which is what these doors always` |
|      - | 4622 | ` * did inline. For a DATABASE zone the reading may name two instants or none, so` |
|      - | 4623 | ` * the zone decides; and the offset it lands on is not the one it started from,` |
|      - | 4624 | `` * which is why `setTime()` across a spring-forward morning has to write both`` |
|      - | 4625 | ` * slots rather than keeping the offset it read the clock with.` |
|      - | 4626 | ` */` |
|   1632 | 4627 | `static void DtStoreLocalOf(ph7_vm *pVm,ph7_class_instance *pObj,sxi64 iLocal)` |
|      3 | 4628 | `{` |
|   1635 | 4629 | `	sxi32 iOff = (sxi32)PH7_NativeAttrInt(pObj,DT_OFF);` |
|      - | 4630 | `#ifdef PH7_ENABLE_TZDB` |
|   1635 | 4631 | `	int iTz = DtTzIndexOf(pObj,DT_NAME,DT_ZKIND);` |
|   1635 | 4632 | `	if( iTz >= 0 ){` |
|    161 | 4633 | `		sxi64 iTs = iLocal;` |
|    161 | 4634 | `		if( PH7_TzLocalToUtc(iTz,iLocal,&iTs,&iOff) ){` |
|    161 | 4635 | `			PH7_NativeSetAttrInt(pVm,pObj,DT_TS,iTs);` |
|    161 | 4636 | `			PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,iOff);` |
|    161 | 4637 | `			return;` |
|      - | 4638 | `		}` |
|    ! 0 | 4639 | `	}` |
|      - | 4640 | `#endif` |
|      - | 4641 | `	/* Unsigned, because a reading at either end of php's clock overflows the` |
|      - | 4642 | `	 * signed subtraction and every other arithmetic door here is written the` |
|      - | 4643 | `	 * same way. */` |
|   1475 | 4644 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_TS,(sxi64)((sxu64)iLocal - (sxu64)iOff));` |
|    819 | 4645 | `}` |
|      - | 4646 | `/*` |
|      - | 4647 | ` * modify()'s timestamp write, shared with the procedural date_modify().` |
|      - | 4648 | ` *` |
|      - | 4649 | ` * The parse ran under DT_PARSE_KEEP_ZONE, so what it produced is the object's` |
|      - | 4650 | ` * own wall clock less the offset it STARTED from -- and a modifier can walk` |
|      - | 4651 | ` * that clock across a DST switch, where the offset it ends on is a different` |
|      - | 4652 | `` * one. `@epoch` is the exception: it names an absolute instant, and`` |
|      - | 4653 | `` * DtEpochRezone moves the object to a fixed `+00:00` along with it, so there is`` |
|      - | 4654 | ` * no reading to re-solve.` |
|      - | 4655 | ` */` |
|   1138 | 4656 | `static void DtStoreModified(ph7_vm *pVm,ph7_class_instance *pObj,sxi64 iTs,` |
|      - | 4657 | `	sxi32 iOffBase,const dt_parsed *pVec)` |
|      3 | 4658 | `{` |
|   1141 | 4659 | `	if( pVec->bEpoch ){` |
|     21 | 4660 | `		PH7_NativeSetAttrInt(pVm,pObj,DT_TS,iTs);` |
|     21 | 4661 | `		return;` |
|      - | 4662 | `	}` |
|   1121 | 4663 | `	DtStoreLocalOf(pVm,pObj,(sxi64)((sxu64)iTs + (sxu64)iOffBase));` |
|    572 | 4664 | `}` |
|      - | 4665 | `/*` |
|      - | 4666 | ` * Parse $datetime into a date object's state, php's constructor rules: an explicit` |
|      - | 4667 | ` * offset in the string wins over the $timezone argument, a literal "Z" keeps its` |
|      - | 4668 | ` * own name, and everything else takes the argument's (or the default) zone.` |
|      - | 4669 | ` * Returns 0 on success; on failure the caller throws with the reason and position` |
|      - | 4670 | ` * this reports.` |
|      - | 4671 | ` */` |
|   3678 | 4672 | `static int DtInitState(ph7_context *pCtx,const char *zIn,int nIn,sxi32 iZoneOff,` |
|      - | 4673 | `	const char *zZoneName,int nZoneName,int iZoneKind,dt_state *pOut,char *zNameBuf,` |
|      - | 4674 | `	sxu32 nNameBuf,const char **pzErr,int *piPos,char *pcAt)` |
|      4 | 4675 | `{` |
|   3682 | 4676 | `	sxi64 iTs = 0,iNow = 0;` |
|   3682 | 4677 | `	sxi32 iOff = 0;` |
|   3682 | 4678 | `	int bOffSet = 0,uSec = 0,iErrPos,uNow = 0;` |
|      - | 4679 | `	int iTz,bDst,nAbbr;` |
|      - | 4680 | `	const char *zAbbr;` |
|      - | 4681 | `	dt_parsed sVec;` |
|      - | 4682 | `	/* php's base moment is the whole clock, microseconds included: a string that` |
|      - | 4683 | ``	 * names no time of day keeps them (`new DateTime()`, `+1 day`), and one that`` |
|      - | 4684 | `	 * does zeroes them along with the rest of the clock. */` |
|   3682 | 4685 | `	DtNowUs(pCtx->pVm,&iNow,&uNow);` |
|      - | 4686 | `	/* A DATABASE zone has no offset until an instant picks one, and the parse` |
|      - | 4687 | `	 * below needs one BEFORE it has an instant -- the base moment is what fills` |
|      - | 4688 | `	 * in every field the string leaves out, so reading it in the wrong offset` |
|      - | 4689 | ``	 * puts `today` on the wrong day. The offset at NOW is that answer, and the`` |
|      - | 4690 | `	 * reading the parse produces is re-solved against the zone afterwards. */` |
|   3682 | 4691 | `	iTz = DtTzIndex(zZoneName,nZoneName,iZoneKind);` |
|   3682 | 4692 | `	if( iTz >= 0 ){` |
|    170 | 4693 | `		iZoneOff = DtTzOffsetAt(iTz,iZoneOff,iNow,&bDst,&zAbbr,&nAbbr);` |
|     84 | 4694 | `	}` |
|      - | 4695 | ``	/* php's constructors pass the word `now` in place of an empty string, which`` |
|      - | 4696 | ``	 * is why `new DateTime('')` is the current moment where `modify('')` is its`` |
|      - | 4697 | ``	 * `Empty string` refusal. */`` |
|   3682 | 4698 | `	if( nIn < 1 ){` |
|      3 | 4699 | `		zIn = "now";` |
|      3 | 4700 | `		nIn = 3;` |
|      1 | 4701 | `	}` |
|      - | 4702 | `	/* php publishes what this scan collected through getLastErrors(), whether or` |
|      - | 4703 | ``	 * not it throws, and a clean parse puts the record back to `false`. */`` |
|   5521 | 4704 | `	iErrPos = DtParseEx(zIn,nIn,iNow,iZoneOff,uNow,0,&iTs,&iOff,&bOffSet,&uSec,&sVec,` |
|   3678 | 4705 | `		&pCtx->pVm->sDtLastErr);` |
|   3682 | 4706 | `	if( iErrPos != 0 ){` |
|    302 | 4707 | `		*pzErr = DtParseErr(zIn,nIn,iErrPos,piPos,pcAt);` |
|    302 | 4708 | `		return -1;` |
|      - | 4709 | `	}` |
|   3382 | 4710 | `	pOut->iTs = iTs;` |
|   3382 | 4711 | `	pOut->uSec = uSec;` |
|   3382 | 4712 | `	if( bOffSet ){` |
|   1218 | 4713 | `		pOut->iOff = iOff;` |
|   1218 | 4714 | `		if( bOffSet == 2 ){` |
|      - | 4715 | ``			/* a zone the STRING named -- php's `Z`, or a trailing UTC/GMT */`` |
|    222 | 4716 | `			pOut->zName = sVec.zZone;` |
|    222 | 4717 | `			pOut->nName = sVec.nZone;` |
|    222 | 4718 | `			pOut->iZoneKind = sVec.bZoneIdent ? DT_ZONE_ID : DT_ZONE_ABBR;` |
|    112 | 4719 | `		}else{` |
|      - | 4720 | `			/* ...Sec: an offset the string spelled with SECONDS is named with` |
|      - | 4721 | ``			 * them (`+02:00:30`), which is the same name DateTimeZone gives it. */`` |
|    998 | 4722 | `			pOut->nName = DtOffNameSec(zNameBuf,nNameBuf,iOff);` |
|    998 | 4723 | `			pOut->zName = zNameBuf;` |
|    998 | 4724 | `			pOut->iZoneKind = DT_ZONE_OFFSET;` |
|      - | 4725 | `		}` |
|    610 | 4726 | `	}else{` |
|   2166 | 4727 | `		pOut->iOff = iZoneOff;` |
|   2166 | 4728 | `		pOut->zName = zZoneName;` |
|   2166 | 4729 | `		pOut->nName = nZoneName;` |
|   2166 | 4730 | `		pOut->iZoneKind = iZoneKind;` |
|      - | 4731 | `#ifdef PH7_ENABLE_TZDB` |
|   2166 | 4732 | `		if( iTz >= 0 ){` |
|      - | 4733 | `			/* The parse read the string against the offset at NOW, so what it` |
|      - | 4734 | `			 * produced is a WALL-CLOCK reading in this zone, shifted by that` |
|      - | 4735 | `			 * offset. Recover the reading and ask the zone which instant it` |
|      - | 4736 | `			 * names -- the answer differs from the guess for every date on the` |
|      - | 4737 | `			 * other side of a DST switch from today, which is most of the year.` |
|      - | 4738 | `			 */` |
|    170 | 4739 | `			sxi64 iFixed = iTs;` |
|    170 | 4740 | `			sxi32 iOffAt = iZoneOff;` |
|    170 | 4741 | `			if( PH7_TzLocalToUtc(iTz,iTs + iZoneOff,&iFixed,&iOffAt) ){` |
|    170 | 4742 | `				pOut->iTs = iFixed;` |
|    170 | 4743 | `				pOut->iOff = iOffAt;` |
|     84 | 4744 | `			}` |
|     84 | 4745 | `		}` |
|      - | 4746 | `#endif` |
|      - | 4747 | `	}` |
|   3382 | 4748 | `	return 0;` |
|   1843 | 4749 | `}` |
|      - | 4750 | `/*` |
|      - | 4751 | ` * php's UTC-OFFSET spellings, the whole set of them. Reads the digits and colons` |
|      - | 4752 | ` * after the sign and dispatches on their SHAPE, which is what php's scanner does:` |
|      - | 4753 | ` *` |
|      - | 4754 | ` *   D \| DD              the HOURS alone            +1     +01    +59` |
|      - | 4755 | ` *   DDD                 H then MM                  +130 = +01:30, +999 = +10:39` |
|      - | 4756 | ` *   DDDD                HH then MM                 +0100  +0060 = +01:00` |
|      - | 4757 | ` *   DDDDDD              HH then MM then SS         +010059 = +01:00:59` |
|      - | 4758 | ` *   D:D \| DD:D \| D:DD \| DD:DD    hours then minutes` |
|      - | 4759 | ` *   DD:DD:DD            hours, minutes and seconds` |
|      - | 4760 | ` *` |
|      - | 4761 | ` * Five digits, seven digits and a one-digit hour before two colons are php's own` |
|      - | 4762 | `` * refusals. Minutes and seconds are NOT bounded on their own -- `+00:60` is an`` |
|      - | 4763 | `` * hour and `+01:99` is +02:39 -- only the TOTAL is, and a total at or past 100`` |
|      - | 4764 | ` * hours is php's separate "Timezone offset is out of range" (answered here as -2,` |
|      - | 4765 | ` * because the two refusals are worded differently at every door).` |
|      - | 4766 | ` *` |
|      - | 4767 | ` * Answers the KIND as well (php's timezone_type), because the name cannot carry` |
|      - | 4768 | ` * it: "UTC" spelled exactly is an IDENTIFIER and any other casing of it is an` |
|      - | 4769 | ` * ABBREVIATION, and php refuses to compare the two.` |
|      - | 4770 | ` */` |
|      - | 4771 | `#define DT_ZONE_OFF_LIMIT 360000   /* php's ceiling: \|offset\| < 100 hours */` |
|    698 | 4772 | `static int DtZoneOffsetDigits(const char *z,int n,sxi32 *piOff,int *pnUsed)` |
|      5 | 4773 | `{` |
|      - | 4774 | `	int aVal[3];` |
|      - | 4775 | `	int aWidth[3];` |
|    703 | 4776 | `	int nPart = 0;` |
|      - | 4777 | `	int i;` |
|      - | 4778 | `	sxi64 iOff;` |
|    703 | 4779 | `	aVal[0] = aVal[1] = aVal[2] = 0;` |
|    703 | 4780 | `	aWidth[0] = aWidth[1] = aWidth[2] = 0;` |
|      - | 4781 | `	/* Read the RUN of digits and colons and stop at anything else; the caller` |
|      - | 4782 | `	 * decides what a tail means. At most two colons, and no group may be empty` |
|      - | 4783 | `	 * or wider than two -- except the single group of a colonless spelling,` |
|      - | 4784 | `	 * which is split by WIDTH below instead. */` |
|   3677 | 4785 | `	for( i = 0 ; i < n ; ++i ){` |
|   3003 | 4786 | `		if( z[i] == ':' ){` |
|    451 | 4787 | `			if( nPart >= 2 ){` |
|    ! 0 | 4788 | `				break;` |
|      - | 4789 | `			}` |
|    451 | 4790 | `			nPart++;` |
|    451 | 4791 | `			continue;` |
|      - | 4792 | `		}` |
|   2557 | 4793 | `		if( !SyisDigit(z[i]) ){` |
|     17 | 4794 | `			break;` |
|      - | 4795 | `		}` |
|   2541 | 4796 | `		if( aWidth[nPart] >= 6 ){` |
|      9 | 4797 | `			return -1;` |
|      - | 4798 | `		}` |
|   2533 | 4799 | `		aVal[nPart] = aVal[nPart] * 10 + (z[i] - '0');` |
|   2533 | 4800 | `		aWidth[nPart]++;` |
|   1269 | 4801 | `	}` |
|    695 | 4802 | `	*pnUsed = i;` |
|    695 | 4803 | `	if( nPart == 0 ){` |
|      - | 4804 | `		/* No colon: the WIDTH says how the digits split. */` |
|    329 | 4805 | `		int v = aVal[0];` |
|    329 | 4806 | `		switch( aWidth[0] ){` |
|     86 | 4807 | `			case 1: case 2:  /* H, HH */` |
|    175 | 4808 | `				iOff = (sxi64)v * 3600;` |
|    175 | 4809 | `				break;` |
|     12 | 4810 | `			case 3:          /* H MM */` |
|     25 | 4811 | `				iOff = (sxi64)(v / 100) * 3600 + (v % 100) * 60;` |
|     25 | 4812 | `				break;` |
|     45 | 4813 | `			case 4:          /* HH MM */` |
|     91 | 4814 | `				iOff = (sxi64)(v / 100) * 3600 + (v % 100) * 60;` |
|     91 | 4815 | `				break;` |
|     12 | 4816 | `			case 6:          /* HH MM SS */` |
|     25 | 4817 | `				iOff = (sxi64)(v / 10000) * 3600 + ((v / 100) % 100) * 60 + (v % 100);` |
|     25 | 4818 | `				break;` |
|      8 | 4819 | `			default:         /* five, or seven and up */` |
|     17 | 4820 | `				return -1;` |
|      - | 4821 | `		}` |
|    158 | 4822 | `	}else{` |
|      - | 4823 | `		/* Colons: H:M through HH:MM, or HH:MM:SS with two digits everywhere` |
|      - | 4824 | ``		 * (php refuses `+1:00:00` and `+01:00:0` alike, and takes `+1:1` for`` |
|      - | 4825 | `		 * +01:01). A trailing colon leaves an empty group, which is a refusal. */` |
|    369 | 4826 | `		int nWant = nPart == 2 ? 2 : 0;` |
|   1115 | 4827 | `		for( i = 0 ; i <= nPart ; ++i ){` |
|    775 | 4828 | `			if( aWidth[i] < 1 \|\| aWidth[i] > 2 \|\| (nWant && aWidth[i] != nWant) ){` |
|     25 | 4829 | `				return -1;` |
|      - | 4830 | `			}` |
|    378 | 4831 | `		}` |
|    345 | 4832 | `		iOff = (sxi64)aVal[0] * 3600 + (sxi64)aVal[1] * 60 + aVal[2];` |
|      - | 4833 | `	}` |
|    655 | 4834 | `	if( iOff >= DT_ZONE_OFF_LIMIT ){` |
|      - | 4835 | `		/* Past php's ceiling, and php answers THAT even when the spelling has a` |
|      - | 4836 | `		 * tail it would otherwise reject: the range is checked on what the` |
|      - | 4837 | `		 * scanner read, before anything is said about what follows. */` |
|     33 | 4838 | `		return -2;` |
|      - | 4839 | `	}` |
|    623 | 4840 | `	*piOff = (sxi32)iOff;` |
|    623 | 4841 | `	return 0;` |
|    354 | 4842 | `}` |
|      - | 4843 | `/*` |
|      - | 4844 | ` * The two spellings that are NAMES rather than arithmetic, in php's own order:` |
|      - | 4845 | ` * an ABBREVIATION first, then a tz-database IDENTIFIER. Reached only once every` |
|      - | 4846 | ` * offset spelling has failed, and answers -1 when neither table has the name --` |
|      - | 4847 | ` * which, with PH7_ENABLE_TZDB off, is always.` |
|      - | 4848 | ` *` |
|      - | 4849 | `` * The order is not a detail. Ten names are in both tables (`CET`, `EET`, `EST`,`` |
|      - | 4850 | `` * `GMT`, `HST`, `MET`, `MST`, `UCT`, `UTC`, `WET`) and php takes the`` |
|      - | 4851 | `` * abbreviation for every one of them, so `new DateTimeZone('CET')` is a FIXED`` |
|      - | 4852 | ` * +01:00 that never observes daylight time while the zone file of that name` |
|      - | 4853 | ` * switches twice a year. Getting this backwards turns a loud refusal into a` |
|      - | 4854 | ` * quietly wrong summer offset.` |
|      - | 4855 | ` *` |
|      - | 4856 | ` * The two also differ in what they NAME. An abbreviation answers the table's` |
|      - | 4857 | ` * canonical upper-case spelling whatever the caller wrote; an identifier` |
|      - | 4858 | `` * answers the caller's own bytes -- `new DateTimeZone('europe/paris')` is named`` |
|      - | 4859 | `` * `europe/paris` and still knows about Paris.`` |
|      - | 4860 | ` */` |
|   1700 | 4861 | `static int DtZoneNamed(const char *zTz,int nTz,sxi32 *piOff,const char **pzName,` |
|      - | 4862 | `	int *pnName,int *piKind)` |
|      4 | 4863 | `{` |
|      - | 4864 | `#ifdef PH7_ENABLE_TZDB` |
|   1704 | 4865 | `	int bDst = 0;` |
|   1704 | 4866 | `	if( PH7_TzAbbrFind(zTz,nTz,piOff,&bDst,pzName,pnName) ){` |
|    286 | 4867 | `		*piKind = DT_ZONE_ABBR;` |
|    286 | 4868 | `		return 0;` |
|      - | 4869 | `	}` |
|   1422 | 4870 | `	if( PH7_TzFind(zTz,nTz) >= 0 ){` |
|   1000 | 4871 | `		*piOff = 0;` |
|   1000 | 4872 | `		*pzName = zTz;` |
|   1000 | 4873 | `		*pnName = nTz;` |
|   1000 | 4874 | `		*piKind = DT_ZONE_ID;` |
|   1000 | 4875 | `		return 0;` |
|      - | 4876 | `	}` |
|      - | 4877 | `#else` |
|      - | 4878 | `	SXUNUSED(zTz);` |
|      - | 4879 | `	SXUNUSED(nTz);` |
|      - | 4880 | `	SXUNUSED(piOff);` |
|      - | 4881 | `	SXUNUSED(pzName);` |
|      - | 4882 | `	SXUNUSED(pnName);` |
|      - | 4883 | `	SXUNUSED(piKind);` |
|      - | 4884 | `#endif` |
|    424 | 4885 | `	return -1;` |
|    854 | 4886 | `}` |
|      - | 4887 | `/*` |
|      - | 4888 | ` * The timezone spellings PHL understands with no tz database: UTC, GMT, Z and a` |
|      - | 4889 | `` * fixed offset, optionally behind a `GMT` prefix and behind leading blanks.`` |
|      - | 4890 | ` * Shared by DateTimeZone::__construct(), which throws on a miss, and` |
|      - | 4891 | ` * timezone_open(), which warns and answers false. Answers 0, -1 (unknown or bad)` |
|      - | 4892 | ` * or -2 (an offset past php's range).` |
|      - | 4893 | ` */` |
|   2478 | 4894 | `static int DtZoneParse(const char *zTz,int nTz,sxi32 *piOff,const char **pzName,` |
|      - | 4895 | `	int *pnName,int *piKind,char *zBuf,sxu32 nBuf)` |
|      4 | 4896 | `{` |
|   2482 | 4897 | `	int rc,nUsed = 0;` |
|      - | 4898 | `	/* php's scanner skips leading blanks and nothing else -- a TRAILING one is a` |
|      - | 4899 | `	 * refusal, and so is a newline before the sign. */` |
|   3727 | 4900 | `	while( nTz > 0 && (zTz[0] == ' ' \|\| zTz[0] == '\t') ){` |
|      7 | 4901 | `		zTz++;` |
|      7 | 4902 | `		nTz--;` |
|      1 | 4903 | `	}` |
|      - | 4904 | `` 	/* A single letter is php's military zone here too -- `new DateTimeZone('t')` `` |
|      - | 4905 | ``	 * is named `T` and answers -07:00 -- which is what lets a date carrying one`` |
|      - | 4906 | `	 * round-trip through serialize()/__unserialize(). */` |
|   2482 | 4907 | `	if( nTz == 1 && DtZoneMil(zTz[0],piOff,pzName) ){` |
|     94 | 4908 | `		*pnName = 1;` |
|     94 | 4909 | `		*piKind = DT_ZONE_ABBR;` |
|     94 | 4910 | `		return 0;` |
|      - | 4911 | `	}` |
|   2390 | 4912 | `	if( nTz == 3 && (SyStrnicmp(zTz,"UTC",3) == 0 \|\| SyStrnicmp(zTz,"GMT",3) == 0) ){` |
|      - | 4913 | `		/* php answers the canonical spelling, whatever case the caller used --` |
|      - | 4914 | `		 * and only the exact "UTC" is one of its tz-database IDENTIFIERS. */` |
|    720 | 4915 | `		int bUtc = (zTz[0] == 'u' \|\| zTz[0] == 'U');` |
|    720 | 4916 | `		*piOff = 0;` |
|    720 | 4917 | `		*pzName = bUtc ? "UTC" : "GMT";` |
|    720 | 4918 | `		*pnName = 3;` |
|    720 | 4919 | `		*piKind = (bUtc && SyMemcmp(zTz,"UTC",3) == 0) ? DT_ZONE_ID : DT_ZONE_ABBR;` |
|    720 | 4920 | `		return 0;` |
|      - | 4921 | `	}` |
|      - | 4922 | `	{` |
|      - | 4923 | ``		/* `GMT+01:00` is php's offset, named for the offset alone. The prefix is`` |
|      - | 4924 | ``		 * UPPERCASE only there (`gmt+1` is a refusal where the bare `gmt` is a`` |
|      - | 4925 | ``		 * zone), no blank is allowed between the two halves, and `UTC+1` is not`` |
|      - | 4926 | `		 * a spelling at all.` |
|      - | 4927 | `		 *` |
|      - | 4928 | ``		 * It is a prefix only when a SIGN follows it. `GMT0` and `gmt0` are not`` |
|      - | 4929 | `		 * "GMT plus nothing" -- they are tz-database identifiers, stored` |
|      - | 4930 | ``		 * verbatim like any other, and `GMT8` is neither and is refused. So the`` |
|      - | 4931 | `		 * stripped spelling is kept beside the original rather than replacing` |
|      - | 4932 | `		 * it, and the name tables below are asked about what the caller wrote.` |
|      - | 4933 | `		 */` |
|   1674 | 4934 | `		const char *zNum = zTz;` |
|   1674 | 4935 | `		int nNum = nTz;` |
|   1670 | 4936 | `		if( nNum > 3 && SyMemcmp(zNum,"GMT",3) == 0` |
|    742 | 4937 | `		 && (zNum[3] == '+' \|\| zNum[3] == '-') ){` |
|     18 | 4938 | `			zNum += 3;` |
|     18 | 4939 | `			nNum -= 3;` |
|      8 | 4940 | `		}` |
|   1674 | 4941 | `		if( nNum < 2 \|\| (zNum[0] != '+' && zNum[0] != '-') ){` |
|   1286 | 4942 | `			return DtZoneNamed(zTz,nTz,piOff,pzName,pnName,piKind);` |
|      - | 4943 | `		}` |
|    391 | 4944 | `		rc = DtZoneOffsetDigits(zNum + 1,nNum - 1,piOff,&nUsed);` |
|    391 | 4945 | `		if( rc != 0 ){` |
|     81 | 4946 | `			return rc;` |
|      - | 4947 | `		}` |
|    311 | 4948 | `		if( nUsed != nNum - 1 ){` |
|      9 | 4949 | `			return -1;   /* a tail the scanner did not read: not a zone at all */` |
|      - | 4950 | `		}` |
|    303 | 4951 | `		if( zNum[0] == '-' ){` |
|     54 | 4952 | `			*piOff = -*piOff;` |
|     26 | 4953 | `		}` |
|      - | 4954 | `	}` |
|      - | 4955 | `	/* php normalizes the NAME through the offset, so "-00:00" is "+00:00", and` |
|      - | 4956 | `	 * carries the SECONDS field only when there is one. */` |
|    303 | 4957 | `	*pnName = DtOffNameSec(zBuf,nBuf,*piOff);` |
|    303 | 4958 | `	*pzName = zBuf;` |
|    303 | 4959 | `	*piKind = DT_ZONE_OFFSET;` |
|    303 | 4960 | `	return 0;` |
|   1243 | 4961 | `}` |
|      - | 4962 | `/*` |
|      - | 4963 | ` * ---------------------------------------------------------------------------` |
|      - | 4964 | ` * DateTimeZone::listIdentifiers() and its timezone_identifiers_list() twin.` |
|      - | 4965 | ` *` |
|      - | 4966 | `` * The group argument is a BITMASK of the ten continents plus `UTC`, and the`` |
|      - | 4967 | ` * rule for reading it is not the plain mask it looks like -- two values are` |
|      - | 4968 | `` * compared EXACTLY, which is why `-1` is ALL and not ALL_WITH_BC even though`` |
|      - | 4969 | ` * it has every bit:` |
|      - | 4970 | ` *` |
|      - | 4971 | ` *   == PER_COUNTRY (4096)     the country code decides, and a null or` |
|      - | 4972 | ` *                             non-two-character one is a ValueError. Any two` |
|      - | 4973 | ` *                             bytes are accepted and simply match nothing, so` |
|      - | 4974 | `` *                             `12` and the lower-case `br` are empty arrays`` |
|      - | 4975 | `` *                             where `BR` is sixteen zones.`` |
|      - | 4976 | ` *   == ALL_WITH_BC (4095)     every identifier, backward links included.` |
|      - | 4977 | `` *   anything else             `group & 2047` over the group bits, backward`` |
|      - | 4978 | `` *                             links excluded -- so `4097` is Africa alone and`` |
|      - | 4979 | ` *                             the country code beside it is ignored.` |
|      - | 4980 | ` *` |
|      - | 4981 | ` * The order is the table's, which is the identifiers' own byte order.` |
|      - | 4982 | ` */` |
|      - | 4983 | `#define DT_TZ_GROUP_ALL         2047` |
|      - | 4984 | `#define DT_TZ_GROUP_ALL_W_BC    4095` |
|      - | 4985 | `#define DT_TZ_GROUP_PER_COUNTRY 4096` |
|     90 | 4986 | `static int DtZoneListResult(ph7_context *pCtx,sxi64 iGroup,ph7_value *pCc,` |
|      - | 4987 | `	const char *zWho)` |
|      1 | 4988 | `{` |
|      - | 4989 | `	ph7_value *pArray,*pVal;` |
|     91 | 4990 | `	const char *zCc = 0;` |
|     91 | 4991 | `	int nCc = 0,i,nZone;` |
|     45 | 4992 | `	SXUNUSED(zWho);` |
|     91 | 4993 | `	if( iGroup == DT_TZ_GROUP_PER_COUNTRY ){` |
|     41 | 4994 | `		if( pCc != 0 && (pCc->iFlags & MEMOBJ_NULL) == 0 ){` |
|     37 | 4995 | `			zCc = ph7_value_to_string(pCc,&nCc);` |
|     18 | 4996 | `		}` |
|     41 | 4997 | `		if( nCc != 2 ){` |
|     13 | 4998 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4999 | `				"%s(): Argument #2 ($countryCode) must be a two-letter ISO 3166-1 "` |
|      - | 5000 | `				"compatible country code when argument #1 ($timezoneGroup) is "` |
|      4 | 5001 | `				"DateTimeZone::PER_COUNTRY",zWho);` |
|      - | 5002 | `		}` |
|     16 | 5003 | `	}` |
|     83 | 5004 | `	pArray = ph7_context_new_array(pCtx);` |
|     83 | 5005 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     83 | 5006 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5007 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5008 | `	}` |
|     83 | 5009 | `	nZone = 0;` |
|      - | 5010 | `#ifdef PH7_ENABLE_TZDB` |
|     83 | 5011 | `	nZone = PH7_TzCount();` |
|      - | 5012 | `#else` |
|      - | 5013 | `	/* No database, so no identifier to list -- the loop below runs zero times` |
|      - | 5014 | `	 * and the country code the PER_COUNTRY screen above read goes unused. The` |
|      - | 5015 | `	 * screen itself stays: its ValueError is about the ARGUMENTS, not about` |
|      - | 5016 | `	 * what the build happens to carry. */` |
|      - | 5017 | `	SXUNUSED(zCc);` |
|      - | 5018 | `#endif` |
|  49201 | 5019 | `	for( i = 0 ; i < nZone ; ++i ){` |
|      - | 5020 | `#ifdef PH7_ENABLE_TZDB` |
|      - | 5021 | `` 		/* Walked in php's own print order, which is case-INSENSITIVE: `CET` `` |
|      - | 5022 | `` 		 * sits between `Canada/Yukon` and `Chile/Continental`, and `localtime` `` |
|      - | 5023 | ``		 * among the `L` names. The 419 canonical `Continent/City` names come out`` |
|      - | 5024 | `		 * the same either way, so only ALL_WITH_BC shows it. */` |
|  49119 | 5025 | `		int iZone = PH7_TzAt(i);` |
|  49119 | 5026 | `		int nName = 0,bBack = 0;` |
|  49119 | 5027 | `		const char *zName = PH7_TzName(iZone,&nName,&bBack);` |
|  49119 | 5028 | `		if( zName == 0 ){` |
|    ! 0 | 5029 | `			continue;` |
|      - | 5030 | `		}` |
|  49119 | 5031 | `		if( iGroup == DT_TZ_GROUP_PER_COUNTRY ){` |
|  19169 | 5032 | `			if( SyMemcmp(PH7_TzCountry(iZone),zCc,2) != 0 ){` |
|  18819 | 5033 | `				continue;` |
|      1 | 5034 | `			}` |
|  30126 | 5035 | `		}else if( iGroup != DT_TZ_GROUP_ALL_W_BC ){` |
|  29951 | 5036 | `			if( bBack \|\| (PH7_TzGroup(iZone) & (int)(iGroup & DT_TZ_GROUP_ALL)) == 0 ){` |
|  22661 | 5037 | `				continue;` |
|      - | 5038 | `			}` |
|   3645 | 5039 | `		}` |
|   7641 | 5040 | `		ph7_value_string(pVal,zName,nName);` |
|   7641 | 5041 | `		ph7_array_add_elem(pArray,0,pVal);` |
|   7641 | 5042 | `		ph7_value_reset_string_cursor(pVal);` |
|      - | 5043 | `#endif` |
|   3821 | 5044 | `	}` |
|     83 | 5045 | `	ph7_result_value(pCtx,pArray);` |
|     83 | 5046 | `	return PH7_OK;` |
|     46 | 5047 | `}` |
|      - | 5048 | `/* DateTimeZone::listIdentifiers(int $timezoneGroup = ALL, ?string $countryCode = null) */` |
|     84 | 5049 | `static int vm_builtin_DateTimeZone_listIdentifiers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5050 | `{` |
|    167 | 5051 | `	return DtZoneListResult(pCtx,` |
|     82 | 5052 | `		nArg > 0 ? ph7_value_to_int64(apArg[0]) : DT_TZ_GROUP_ALL,` |
|     42 | 5053 | `		nArg > 1 ? apArg[1] : 0,"DateTimeZone::listIdentifiers");` |
|      1 | 5054 | `}` |
|      6 | 5055 | `static int vm_builtin_timezone_identifiers_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5056 | `{` |
|     12 | 5057 | `	return DtZoneListResult(pCtx,` |
|      5 | 5058 | `		nArg > 0 ? ph7_value_to_int64(apArg[0]) : DT_TZ_GROUP_ALL,` |
|      3 | 5059 | `		nArg > 1 ? apArg[1] : 0,"timezone_identifiers_list");` |
|      1 | 5060 | `}` |
|      - | 5061 | `/*` |
|      - | 5062 | ` * ---------------------------------------------------------------------------` |
|      - | 5063 | ` * DateTimeZone::listAbbreviations() and its timezone_abbreviations_list()` |
|      - | 5064 | ` * twin -- every (daylight, offset, zone) triple the database wrote each of the` |
|      - | 5065 | ` * 144 abbreviations for, keyed by the LOWER-CASE spelling. The canonical` |
|      - | 5066 | `` * upper-case one getName() answers is the key of nothing: `CET` is stored`` |
|      - | 5067 | `` * upper-case and printed `cet`.`` |
|      - | 5068 | ` *` |
|      - | 5069 | `` * A triple whose zone is unknown prints a null `timezone_id` rather than being`` |
|      - | 5070 | ` * left out, so a group's count is what the table says and not what it could` |
|      - | 5071 | ` * name.` |
|      - | 5072 | ` */` |
|      4 | 5073 | `static int DtZoneAbbrListResult(ph7_context *pCtx)` |
|      1 | 5074 | `{` |
|      - | 5075 | `	ph7_value *pArray,*pVal,*pKey;` |
|      5 | 5076 | `	int i,nAbbr = 0;` |
|      5 | 5077 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 5078 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      5 | 5079 | `	pKey = ph7_context_new_scalar(pCtx);` |
|      5 | 5080 | `	if( pArray == 0 \|\| pVal == 0 \|\| pKey == 0 ){` |
|    ! 0 | 5081 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5082 | `	}` |
|      - | 5083 | `#ifdef PH7_ENABLE_TZDB` |
|      5 | 5084 | `	nAbbr = PH7_TzAbbrCount();` |
|      - | 5085 | `#endif` |
|    581 | 5086 | `	for( i = 0 ; i < nAbbr ; ++i ){` |
|      - | 5087 | `#ifdef PH7_ENABLE_TZDB` |
|    577 | 5088 | `		int nName = 0,nRow = 0,j;` |
|    577 | 5089 | `		const char *zName = PH7_TzAbbrAt(i,&nName,&nRow);` |
|      - | 5090 | `		ph7_value *pGroup,*pRow;` |
|      - | 5091 | `		char zLower[8];` |
|    577 | 5092 | `		if( zName == 0 \|\| nName > (int)sizeof(zLower) ){` |
|    ! 0 | 5093 | `			continue;` |
|      - | 5094 | `		}` |
|      - | 5095 | `		/* The names are letters only, so folding is a byte at a time. */` |
|   2273 | 5096 | `		for( j = 0 ; j < nName ; ++j ){` |
|   1697 | 5097 | `			zLower[j] = (char)SyCharToLower(zName[j]);` |
|    849 | 5098 | `		}` |
|    577 | 5099 | `		pGroup = ph7_context_new_array(pCtx);` |
|    577 | 5100 | `		if( pGroup == 0 ){` |
|    ! 0 | 5101 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5102 | `		}` |
|   5085 | 5103 | `		for( j = 0 ; j < nRow ; ++j ){` |
|   4509 | 5104 | `			sxi32 iOff = 0;` |
|   4509 | 5105 | `			int bDst = 0,iZone = -1,nZone = 0;` |
|      - | 5106 | `			const char *zZone;` |
|   4509 | 5107 | `			if( !PH7_TzAbbrRowAt(i,j,&iOff,&bDst,&iZone) ){` |
|    ! 0 | 5108 | `				break;` |
|      - | 5109 | `			}` |
|   4509 | 5110 | `			pRow = ph7_context_new_array(pCtx);` |
|   4509 | 5111 | `			if( pRow == 0 ){` |
|    ! 0 | 5112 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5113 | `			}` |
|   4509 | 5114 | `			ph7_value_bool(pVal,bDst);` |
|   4509 | 5115 | `			ph7_array_add_strkey_elem(pRow,"dst",pVal);` |
|   4509 | 5116 | `			ph7_value_int64(pVal,(sxi64)iOff);` |
|   4509 | 5117 | `			ph7_array_add_strkey_elem(pRow,"offset",pVal);` |
|   4509 | 5118 | `			zZone = iZone < 0 ? 0 : PH7_TzName(iZone,&nZone,0);` |
|   4509 | 5119 | `			if( zZone == 0 ){` |
|    101 | 5120 | `				ph7_value_null(pVal);` |
|     51 | 5121 | `			}else{` |
|   4409 | 5122 | `				ph7_value_string(pVal,zZone,nZone);` |
|      - | 5123 | `			}` |
|   4509 | 5124 | `			ph7_array_add_strkey_elem(pRow,"timezone_id",pVal);` |
|   4509 | 5125 | `			ph7_value_reset_string_cursor(pVal);` |
|   4509 | 5126 | `			ph7_array_add_elem(pGroup,0,pRow);` |
|   2255 | 5127 | `		}` |
|    577 | 5128 | `		ph7_value_string(pKey,zLower,nName);` |
|    577 | 5129 | `		ph7_array_add_elem(pArray,pKey,pGroup);` |
|    577 | 5130 | `		ph7_value_reset_string_cursor(pKey);` |
|      - | 5131 | `#endif` |
|    289 | 5132 | `	}` |
|      5 | 5133 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 5134 | `	return PH7_OK;` |
|      3 | 5135 | `}` |
|      - | 5136 | `/* DateTimeZone::listAbbreviations() */` |
|      2 | 5137 | `static int vm_builtin_DateTimeZone_listAbbreviations(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5138 | `{` |
|      1 | 5139 | `	SXUNUSED(nArg);` |
|      1 | 5140 | `	SXUNUSED(apArg);` |
|      3 | 5141 | `	return DtZoneAbbrListResult(pCtx);` |
|      1 | 5142 | `}` |
|      2 | 5143 | `static int vm_builtin_timezone_abbreviations_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5144 | `{` |
|      1 | 5145 | `	SXUNUSED(nArg);` |
|      1 | 5146 | `	SXUNUSED(apArg);` |
|      3 | 5147 | `	return DtZoneAbbrListResult(pCtx);` |
|      1 | 5148 | `}` |
|      - | 5149 | `/*` |
|      - | 5150 | ` * timezone_name_from_abbr(string $abbr, int $utcOffset = -1, int $isDST = -1).` |
|      - | 5151 | ` *` |
|      - | 5152 | ` * There is no DateTimeZone method for this one -- it is a bare function and` |
|      - | 5153 | ` * has been since php 5.1. The four rules it runs are in PH7_TzAbbrZoneFind();` |
|      - | 5154 | `` * what belongs here is that `-1` is how php spells "no offset given", so the`` |
|      - | 5155 | ` * default argument and a caller who really means one second west of UTC are` |
|      - | 5156 | ` * the same call. Answers false where nothing resolves.` |
|      - | 5157 | ` */` |
|     58 | 5158 | `static int vm_builtin_timezone_name_from_abbr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5159 | `{` |
|      - | 5160 | `	const char *zAbbr;` |
|     59 | 5161 | `	int nAbbr = 0;` |
|     59 | 5162 | `	if( nArg < 1 ){` |
|    ! 0 | 5163 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5164 | `		return PH7_OK;` |
|      - | 5165 | `	}` |
|     59 | 5166 | `	zAbbr = ph7_value_to_string(apArg[0],&nAbbr);` |
|      - | 5167 | `#ifdef PH7_ENABLE_TZDB` |
|      - | 5168 | `	{` |
|     59 | 5169 | `		sxi64 iOff = nArg > 1 ? ph7_value_to_int64(apArg[1]) : -1;` |
|     59 | 5170 | `		sxi64 iDst = nArg > 2 ? ph7_value_to_int64(apArg[2]) : -1;` |
|     59 | 5171 | `		int nZone = 0;` |
|     59 | 5172 | `		const char *zZone = PH7_TzAbbrZoneFind(zAbbr,nAbbr,iOff,iDst,&nZone);` |
|     59 | 5173 | `		if( zZone ){` |
|     45 | 5174 | `			ph7_result_string(pCtx,zZone,nZone);` |
|     45 | 5175 | `			return PH7_OK;` |
|      - | 5176 | `		}` |
|      - | 5177 | `	}` |
|      - | 5178 | `#else` |
|      - | 5179 | `	SXUNUSED(zAbbr);` |
|      - | 5180 | `#endif` |
|     15 | 5181 | `	ph7_result_bool(pCtx,0);` |
|     15 | 5182 | `	return PH7_OK;` |
|     30 | 5183 | `}` |
|      - | 5184 | `/* DateTimeZone::__construct(string $timezone) */` |
|   2310 | 5185 | `static int vm_builtin_DateTimeZone_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5186 | `{` |
|   2314 | 5187 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */` |
|      - | 5188 | `	const char *zTz,*zName;` |
|   2314 | 5189 | `	int nTz,nName,iKind = DT_ZONE_ID,rc;` |
|   2314 | 5190 | `	sxi32 iOff = 0;` |
|      - | 5191 | `	char zBuf[16];` |
|   2314 | 5192 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 5193 | `		return PH7_OK;` |
|      - | 5194 | `	}` |
|   2314 | 5195 | `	zTz = ph7_value_to_string(apArg[0],&nTz);` |
|   2314 | 5196 | `	rc = DtZoneParse(zTz,nTz,&iOff,&zName,&nName,&iKind,zBuf,sizeof(zBuf));` |
|   2314 | 5197 | `	if( rc != 0 ){` |
|    127 | 5198 | `		return PH7_VmThrowException(pCtx,"DateInvalidTimeZoneException",` |
|     42 | 5199 | `			rc == -2 ? "DateTimeZone::__construct(): Timezone offset is out of range (%.*s)"` |
|     42 | 5200 | `			         : "DateTimeZone::__construct(): Unknown or bad timezone (%.*s)",nTz,zTz);` |
|      - | 5201 | `	}` |
|   2230 | 5202 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,DTZ_OFF,iOff);` |
|   2230 | 5203 | `	PH7_NativeSetAttrStr(pCtx->pVm,pThis,DTZ_NAME,zName,nName);` |
|   2230 | 5204 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,DTZ_KIND,iKind);` |
|   2230 | 5205 | `	DtSetInit(pCtx->pVm,pThis);` |
|   2230 | 5206 | `	return PH7_OK;` |
|   1159 | 5207 | `}` |
|      - | 5208 | `/* DateTimeZone::getName() */` |
|    446 | 5209 | `static int vm_builtin_DateTimeZone_getName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5210 | `{` |
|    448 | 5211 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5212 | `	const char *zName;` |
|      - | 5213 | `	int nName;` |
|    223 | 5214 | `	SXUNUSED(nArg);` |
|    223 | 5215 | `	SXUNUSED(apArg);` |
|    448 | 5216 | `	if( pThis == 0 ){` |
|      5 | 5217 | `		return PH7_OK;` |
|      - | 5218 | `	}` |
|    444 | 5219 | `	PH7_NativeAttrStr(pThis,DTZ_NAME,&zName,&nName);` |
|    444 | 5220 | `	ph7_result_string(pCtx,zName,nName);` |
|    444 | 5221 | `	return PH7_OK;` |
|    225 | 5222 | `}` |
|      - | 5223 | `/*` |
|      - | 5224 | ` * The offset a ZONE object is on at a given date -- DateTimeZone::getOffset()` |
|      - | 5225 | ` * and its timezone_offset_get() alias.` |
|      - | 5226 | ` *` |
|      - | 5227 | ` * php screens the date it is handed even for a fixed zone, whose answer does` |
|      - | 5228 | ` * not depend on it. A DATABASE zone's does, and the instant it is read at is` |
|      - | 5229 | ` * the date's TIMESTAMP -- the date's own zone is irrelevant, since two` |
|      - | 5230 | ` * expressions of one instant are the same instant.` |
|      - | 5231 | ` */` |
|    420 | 5232 | `static void DtZoneOffsetResult(ph7_context *pCtx,ph7_class_instance *pZone,` |
|      - | 5233 | `	ph7_class_instance *pDate)` |
|      2 | 5234 | `{` |
|    422 | 5235 | `	sxi32 iOff = (sxi32)PH7_NativeAttrInt(pZone,DTZ_OFF);` |
|    422 | 5236 | `	int iTz = DtTzIndexOf(pZone,DTZ_NAME,DTZ_KIND);` |
|    422 | 5237 | `	if( iTz >= 0 && pDate != 0 ){` |
|      - | 5238 | `		int bDst,nAbbr;` |
|      - | 5239 | `		const char *zAbbr;` |
|      9 | 5240 | `		iOff = DtTzOffsetAt(iTz,iOff,PH7_NativeAttrInt(pDate,DT_TS),&bDst,&zAbbr,&nAbbr);` |
|      4 | 5241 | `	}` |
|    422 | 5242 | `	ph7_result_int64(pCtx,iOff);` |
|    422 | 5243 | `}` |
|      - | 5244 | `/* DateTimeZone::getOffset(DateTimeInterface $datetime) */` |
|    422 | 5245 | `static int vm_builtin_DateTimeZone_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5246 | `{` |
|    424 | 5247 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|    424 | 5248 | `	if( pThis == 0 \|\| (nArg > 0 && DtArgInit(pCtx,apArg[0]) != 0) ){` |
|      5 | 5249 | `		return PH7_OK;` |
|      - | 5250 | `	}` |
|    838 | 5251 | `	DtZoneOffsetResult(pCtx,pThis,` |
|    418 | 5252 | `		nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ?` |
|    418 | 5253 | `			(ph7_class_instance *)apArg[0]->x.pOther : 0);` |
|    420 | 5254 | `	return PH7_OK;` |
|    213 | 5255 | `}` |
|      - | 5256 | `/*` |
|      - | 5257 | ` * DateTimeZone::getTransitions() and its timezone_transitions_get() twin.` |
|      - | 5258 | ` *` |
|      - | 5259 | ` * php answers FALSE for anything that is not a DATABASE zone. A fixed offset` |
|      - | 5260 | ` * and an abbreviation each name one clock for all time, and php declines to` |
|      - | 5261 | ` * describe that as a transition list rather than answering a one-row one -- so` |
|      - | 5262 | `` * `(new DateTimeZone('+02:00'))->getTransitions()` and the same call on `CET`,`` |
|      - | 5263 | ` * which is an abbreviation here and never observes daylight time, are both` |
|      - | 5264 | `` * `false` where `UTC` is an identifier and answers a single row.`` |
|      - | 5265 | ` *` |
|      - | 5266 | ` * The FIRST row is synthesized at the range's start and is not a switch: it is` |
|      - | 5267 | ``  * what the clock was already doing when the range opened, which is why its `ts` `` |
|      - | 5268 | ` * is the caller's own bound and why a range that opens exactly ON a transition` |
|      - | 5269 | ` * shows that transition once rather than twice.` |
|      - | 5270 | ` *` |
|      - | 5271 | ` * THE TWO BOUNDS ARE NOT SYMMETRIC, and both halves had to be measured:` |
|      - | 5272 | ` *` |
|      - | 5273 | ` *   the begin is EXCLUSIVE   a transition exactly at it is the synthesized row` |
|      - | 5274 | ` *                            and is not repeated.` |
|      - | 5275 | ` *   the end is EXCLUSIVE too a transition exactly at it is left out, so` |
|      - | 5276 | `` *                            `getTransitions($t, $x)` and `getTransitions($t,`` |
|      - | 5277 | `` *                            $x + 1)` differ when a switch lands on `$x`.`` |
|      - | 5278 | ` *` |
|      - | 5279 | ` * And the end DEFAULTS to 2147483647 rather than to PHP_INT_MAX -- timelib's` |
|      - | 5280 | ` * 32-bit horizon, still visible in php 8.5. That one constant is why a` |
|      - | 5281 | ` * no-argument call stops in 2037 for the American zones while an explicit end` |
|      - | 5282 | ` * past 2038 keeps generating from the POSIX footer, and why php's own call with` |
|      - | 5283 | ` * an explicit PHP_INT_MAX runs until it exhausts memory. Read as "the default` |
|      - | 5284 | ` * is unbounded" it looks instead like the walk refuses to extrapolate, which is` |
|      - | 5285 | ` * a rule that holds on every zone whose data happens to end before 2038 and` |
|      - | 5286 | ` * fails on the ones that do not.` |
|      - | 5287 | ` */` |
|      - | 5288 | `#ifdef PH7_ENABLE_TZDB` |
|   1258 | 5289 | `static int DtTransRow(ph7_context *pCtx,ph7_value *pArray,ph7_value *pVal,` |
|      - | 5290 | `	sxi64 iTs,sxi32 iOff,int bDst,const char *zAbbr,int nAbbr)` |
|      1 | 5291 | `{` |
|   1259 | 5292 | `	ph7_value *pRow = ph7_context_new_array(pCtx);` |
|      - | 5293 | `	sxi64 y;` |
|      - | 5294 | `	int m,d,nSec;` |
|      - | 5295 | `	char zBuf[64];` |
|      - | 5296 | `	int nBuf;` |
|   1259 | 5297 | `	if( pRow == 0 ){` |
|    ! 0 | 5298 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5299 | `	}` |
|      - | 5300 | `	/* The seconds of the day as a FLOORED remainder, taken without ever` |
|      - | 5301 | `	 * rebuilding the day in seconds: the nominal first row sits at PHP_INT_MIN,` |
|      - | 5302 | ``	 * and `iTs - iDay * 86400` overflows there long before it can be`` |
|      - | 5303 | ``	 * subtracted. `%` cannot overflow for this divisor, and the correction`` |
|      - | 5304 | `	 * turns C's truncation towards zero into the floor the date wants. */` |
|   1259 | 5305 | `	nSec = (int)(iTs % 86400);` |
|   1259 | 5306 | `	if( nSec < 0 ){` |
|    395 | 5307 | `		nSec += 86400;` |
|    197 | 5308 | `	}` |
|   1259 | 5309 | `	DtCivilFromDays(DtFloorDiv(iTs,86400),&y,&m,&d);` |
|      - | 5310 | ``	/* Always spelled in UT -- the `+00:00` is a constant, not the zone's own`` |
|      - | 5311 | `	 * offset, so the row carries the instant twice and the offset once. */` |
|   2517 | 5312 | `	nBuf = (int)SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 5313 | `		"%04qd-%02d-%02dT%02d:%02d:%02d+00:00",` |
|   1258 | 5314 | `		y,m,d,nSec / 3600,(nSec / 60) % 60,nSec % 60);` |
|   1259 | 5315 | `	ph7_value_int64(pVal,iTs);` |
|   1259 | 5316 | `	ph7_array_add_strkey_elem(pRow,"ts",pVal);` |
|   1259 | 5317 | `	ph7_value_string(pVal,zBuf,nBuf);` |
|   1259 | 5318 | `	ph7_array_add_strkey_elem(pRow,"time",pVal);` |
|   1259 | 5319 | `	ph7_value_reset_string_cursor(pVal);` |
|   1259 | 5320 | `	ph7_value_int64(pVal,iOff);` |
|   1259 | 5321 | `	ph7_array_add_strkey_elem(pRow,"offset",pVal);` |
|   1259 | 5322 | `	ph7_value_bool(pVal,bDst);` |
|   1259 | 5323 | `	ph7_array_add_strkey_elem(pRow,"isdst",pVal);` |
|   1259 | 5324 | `	ph7_value_string(pVal,zAbbr,nAbbr);` |
|   1259 | 5325 | `	ph7_array_add_strkey_elem(pRow,"abbr",pVal);` |
|   1259 | 5326 | `	ph7_value_reset_string_cursor(pVal);` |
|   1259 | 5327 | `	ph7_array_add_elem(pArray,0,pRow);` |
|      - | 5328 | `	/* The parent took a copy, so the row goes back to the context rather than` |
|      - | 5329 | `	 * being held for the length of a walk that can run to hundreds of rows. */` |
|   1259 | 5330 | `	ph7_context_release_value(pCtx,pRow);` |
|   1259 | 5331 | `	return PH7_OK;` |
|    630 | 5332 | `}` |
|      - | 5333 | `#endif /* PH7_ENABLE_TZDB */` |
|     32 | 5334 | `static int DtZoneTransitionsResult(ph7_context *pCtx,ph7_class_instance *pZone,` |
|      - | 5335 | `	sxi64 iBegin,sxi64 iEnd)` |
|      1 | 5336 | `{` |
|     33 | 5337 | `	int iTz = DtTzIndexOf(pZone,DTZ_NAME,DTZ_KIND);` |
|      - | 5338 | `#ifdef PH7_ENABLE_TZDB` |
|     33 | 5339 | `	if( iTz < 0 ){` |
|      - | 5340 | `		const char *zName;` |
|      - | 5341 | `		int nName;` |
|      7 | 5342 | `		PH7_NativeAttrStr(pZone,DTZ_NAME,&zName,&nName);` |
|      - | 5343 | ``		/* `UTC` is an IDENTIFIER, and php describes it like any other -- one`` |
|      - | 5344 | `		 * row, no switches. DtTzIndex() excludes it from the table only to keep` |
|      - | 5345 | `		 * the fixed-offset path every other door is tested on, and that` |
|      - | 5346 | `		 * exclusion is wrong for this one question. An abbreviation stays out:` |
|      - | 5347 | ``		 * `CET` is a fixed +01:00 here, not the file of that name, so it keeps`` |
|      - | 5348 | ``		 * the `false` php gives it. */`` |
|      7 | 5349 | `		if( DtZoneKindOf(pZone,DTZ_KIND,zName,nName) == DT_ZONE_ID ){` |
|      3 | 5350 | `			iTz = PH7_TzFind(zName,nName);` |
|      1 | 5351 | `		}` |
|      3 | 5352 | `	}` |
|      - | 5353 | `#endif` |
|     33 | 5354 | `	if( iTz < 0 ){` |
|      5 | 5355 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5356 | `		return PH7_OK;` |
|      - | 5357 | `	}` |
|      - | 5358 | `#ifdef PH7_ENABLE_TZDB` |
|      - | 5359 | `	{` |
|     29 | 5360 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|     29 | 5361 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|     29 | 5362 | `		sxi64 iTs = 0,iLast = iBegin,iFileEnd = iBegin;` |
|     29 | 5363 | `		sxi32 iOff = 0;` |
|     29 | 5364 | `		int bDst = 0,nAbbr = 0,i,nTrans;` |
|     29 | 5365 | `		const char *zAbbr = "";` |
|     29 | 5366 | `		if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5367 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5368 | `		}` |
|     29 | 5369 | `		if( PH7_TzOffsetAt(iTz,iBegin,&iOff,&bDst,&zAbbr,&nAbbr) ){` |
|     29 | 5370 | `			DtTransRow(pCtx,pArray,pVal,iBegin,iOff,bDst,zAbbr,nAbbr);` |
|     14 | 5371 | `		}` |
|     29 | 5372 | `		nTrans = PH7_TzTransCount(iTz);` |
|   4625 | 5373 | `		for( i = 0 ; i < nTrans ; ++i ){` |
|   4609 | 5374 | `			if( !PH7_TzTransAt(iTz,i,&iTs,&iOff,&bDst,&zAbbr,&nAbbr) ){` |
|    ! 0 | 5375 | `				break;` |
|      - | 5376 | `			}` |
|   4609 | 5377 | `			iFileEnd = iTs;` |
|   4609 | 5378 | `			if( iTs <= iBegin ){` |
|   3377 | 5379 | `				continue;` |
|      - | 5380 | `			}` |
|   1233 | 5381 | `			if( iTs >= iEnd ){` |
|      - | 5382 | `				/* The file has more rows, and iFileEnd must name the LAST of` |
|      - | 5383 | `				 * them rather than the one this range stopped on: it is the` |
|      - | 5384 | `				 * boundary the footer takes over at, not a position in the` |
|      - | 5385 | `				 * walk. */` |
|     13 | 5386 | `				if( PH7_TzTransAt(iTz,nTrans - 1,&iTs,&iOff,&bDst,&zAbbr,&nAbbr) ){` |
|     13 | 5387 | `					iFileEnd = iTs;` |
|      6 | 5388 | `				}` |
|     13 | 5389 | `				break;` |
|      - | 5390 | `			}` |
|   1221 | 5391 | `			DtTransRow(pCtx,pArray,pVal,iTs,iOff,bDst,zAbbr,nAbbr);` |
|   1221 | 5392 | `			iLast = iTs;` |
|    611 | 5393 | `		}` |
|      - | 5394 | `		/* The footer governs only what comes AFTER the file's last row, so the` |
|      - | 5395 | `		 * walk is seeded there rather than at the last row EMITTED. Seeded at` |
|      - | 5396 | `		 * the latter it would re-derive from the rule inside territory the file` |
|      - | 5397 | `		 * already describes, and answer the CURRENT switch dates for years that` |
|      - | 5398 | `		 * ran on older ones -- an extra November row in 2004, and a decade of` |
|      - | 5399 | `		 * invented ones in 1900. */` |
|     29 | 5400 | `		if( iLast < iFileEnd ){` |
|     13 | 5401 | `			iLast = iFileEnd;` |
|      6 | 5402 | `		}` |
|     39 | 5403 | `		while( PH7_TzTransNextPosix(iTz,iLast,&iTs,&iOff,&bDst,&zAbbr,&nAbbr) ){` |
|     35 | 5404 | `			if( iTs >= iEnd ){` |
|     25 | 5405 | `				break;` |
|      - | 5406 | `			}` |
|     11 | 5407 | `			DtTransRow(pCtx,pArray,pVal,iTs,iOff,bDst,zAbbr,nAbbr);` |
|     11 | 5408 | `			iLast = iTs;` |
|      1 | 5409 | `		}` |
|     29 | 5410 | `		ph7_result_value(pCtx,pArray);` |
|      - | 5411 | `	}` |
|      - | 5412 | `#else` |
|      - | 5413 | `	SXUNUSED(iBegin);` |
|      - | 5414 | `	SXUNUSED(iEnd);` |
|      - | 5415 | `#endif` |
|     29 | 5416 | `	return PH7_OK;` |
|     17 | 5417 | `}` |
|      - | 5418 | `/*` |
|      - | 5419 | ` * DateTimeZone::getLocation() and its timezone_location_get() twin -- the` |
|      - | 5420 | `` * `zone.tab` row behind an identifier: its country, its point, and the note`` |
|      - | 5421 | ` * tzdata writes beside it.` |
|      - | 5422 | ` *` |
|      - | 5423 | ` * The gate is getTransitions()'s, and for the same reason: php answers FALSE` |
|      - | 5424 | ` * for anything that is not a DATABASE zone, so a fixed offset and an` |
|      - | 5425 | ` * abbreviation both decline. That is what puts the eleven names which are BOTH` |
|      - | 5426 | `` * a zone file and an abbreviation -- `CET`, `EET`, `EST`, `GMT`, `GMT+0`,`` |
|      - | 5427 | `` * `GMT-0`, `HST`, `MET`, `MST`, `UCT`, `WET` -- on the `false` side while `UTC`,`` |
|      - | 5428 | ` * which is an identifier, answers a row.` |
|      - | 5429 | ` *` |
|      - | 5430 | `` * A zone the tab does not list still answers a row rather than false: `??`, the`` |
|      - | 5431 | `` * origin, and the literal comment `?`. 170 of the 599 read that way.`` |
|      - | 5432 | ` */` |
|     36 | 5433 | `static int DtZoneLocationResult(ph7_context *pCtx,ph7_class_instance *pZone)` |
|      1 | 5434 | `{` |
|     37 | 5435 | `	int iTz = -1;` |
|      - | 5436 | `#ifdef PH7_ENABLE_TZDB` |
|      - | 5437 | `	{` |
|      - | 5438 | `		const char *zName;` |
|      - | 5439 | `		int nName;` |
|     37 | 5440 | `		PH7_NativeAttrStr(pZone,DTZ_NAME,&zName,&nName);` |
|     37 | 5441 | `		if( DtZoneKindOf(pZone,DTZ_KIND,zName,nName) == DT_ZONE_ID ){` |
|     25 | 5442 | `			iTz = PH7_TzFind(zName,nName);` |
|     12 | 5443 | `		}` |
|      - | 5444 | `	}` |
|      - | 5445 | `#else` |
|      - | 5446 | `	SXUNUSED(pZone);` |
|      - | 5447 | `#endif` |
|     37 | 5448 | `	if( iTz < 0 ){` |
|     13 | 5449 | `		ph7_result_bool(pCtx,0);` |
|     13 | 5450 | `		return PH7_OK;` |
|      - | 5451 | `	}` |
|      - | 5452 | `#ifdef PH7_ENABLE_TZDB` |
|      - | 5453 | `	{` |
|     25 | 5454 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|     25 | 5455 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|      - | 5456 | `		const char *zComment;` |
|     25 | 5457 | `		double rLat = 0.0,rLong = 0.0;` |
|     25 | 5458 | `		int nComment = 0;` |
|     25 | 5459 | `		if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5460 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5461 | `		}` |
|     25 | 5462 | `		PH7_TzLocation(iTz,&rLat,&rLong,&zComment,&nComment);` |
|     25 | 5463 | `		ph7_value_string(pVal,PH7_TzCountry(iTz),2);` |
|     25 | 5464 | `		ph7_array_add_strkey_elem(pArray,"country_code",pVal);` |
|     25 | 5465 | `		ph7_value_reset_string_cursor(pVal);` |
|     25 | 5466 | `		ph7_value_double(pVal,rLat);` |
|     25 | 5467 | `		ph7_array_add_strkey_elem(pArray,"latitude",pVal);` |
|     25 | 5468 | `		ph7_value_double(pVal,rLong);` |
|     25 | 5469 | `		ph7_array_add_strkey_elem(pArray,"longitude",pVal);` |
|     25 | 5470 | `		ph7_value_string(pVal,zComment,nComment);` |
|     25 | 5471 | `		ph7_array_add_strkey_elem(pArray,"comments",pVal);` |
|     25 | 5472 | `		ph7_result_value(pCtx,pArray);` |
|      - | 5473 | `	}` |
|      - | 5474 | `#endif` |
|     25 | 5475 | `	return PH7_OK;` |
|     19 | 5476 | `}` |
|      - | 5477 | `/* DateTimeZone::getLocation() */` |
|     32 | 5478 | `static int vm_builtin_DateTimeZone_getLocation(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5479 | `{` |
|     33 | 5480 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|     16 | 5481 | `	SXUNUSED(nArg);` |
|     16 | 5482 | `	SXUNUSED(apArg);` |
|     33 | 5483 | `	if( pThis == 0 ){` |
|    ! 0 | 5484 | `		return PH7_OK;` |
|      - | 5485 | `	}` |
|     33 | 5486 | `	return DtZoneLocationResult(pCtx,pThis);` |
|     17 | 5487 | `}` |
|      - | 5488 | `/*` |
|      - | 5489 | ` * timezone_version_get() -- timelib's spelling of the IANA release the embedded` |
|      - | 5490 | ` * database was cut from. A build with no database has none to name.` |
|      - | 5491 | ` */` |
|      2 | 5492 | `static int vm_builtin_timezone_version_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5493 | `{` |
|      1 | 5494 | `	SXUNUSED(nArg);` |
|      1 | 5495 | `	SXUNUSED(apArg);` |
|      - | 5496 | `#ifdef PH7_ENABLE_TZDB` |
|      3 | 5497 | `	ph7_result_string(pCtx,PH7_TzVersion(),-1);` |
|      - | 5498 | `#else` |
|      - | 5499 | `	ph7_result_string(pCtx,"0.system",-1);` |
|      - | 5500 | `#endif` |
|      3 | 5501 | `	return PH7_OK;` |
|      1 | 5502 | `}` |
|      - | 5503 | `/* DateTimeZone::getTransitions(int $timestampBegin = PHP_INT_MIN, int $timestampEnd = 2147483647) */` |
|     28 | 5504 | `static int vm_builtin_DateTimeZone_getTransitions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5505 | `{` |
|     29 | 5506 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|     29 | 5507 | `	if( pThis == 0 ){` |
|    ! 0 | 5508 | `		return PH7_OK;` |
|      - | 5509 | `	}` |
|     68 | 5510 | `	return DtZoneTransitionsResult(pCtx,pThis,` |
|     27 | 5511 | `		nArg > 0 ? ph7_value_to_int64(apArg[0]) : (-(sxi64)0x7FFFFFFFFFFFFFFF - 1),` |
|     26 | 5512 | `		nArg > 1 ? ph7_value_to_int64(apArg[1]) : (sxi64)0x7FFFFFFF);` |
|     15 | 5513 | `}` |
|      - | 5514 | `/* DateTime::__construct(string $datetime = 'now', ?DateTimeZone $timezone = null) */` |
|   3556 | 5515 | `static int vm_builtin_DateTime_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5516 | `{` |
|   3560 | 5517 | `	ph7_vm *pVm = pCtx->pVm;` |
|   3560 | 5518 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */` |
|   3560 | 5519 | `	const char *zIn = "now",*zZone;` |
|   3560 | 5520 | `	int nIn = 3,nZone;` |
|   3560 | 5521 | `	sxi32 iZoneOff = 0;` |
|      - | 5522 | `	dt_state sState;` |
|      - | 5523 | `	char zNameBuf[16];` |
|      - | 5524 | `	const char *zErr;` |
|      - | 5525 | `	int iPos;` |
|      - | 5526 | `	char cAt;` |
|      - | 5527 | `	int iZoneKind;` |
|   3560 | 5528 | `	if( pThis == 0 ){` |
|    ! 0 | 5529 | `		return PH7_OK;` |
|      - | 5530 | `	}` |
|   3560 | 5531 | `	zZone = pVm->zDefTz;` |
|   3560 | 5532 | `	nZone = (int)pVm->nDefTz;` |
|   3560 | 5533 | `	iZoneKind = DT_ZONE_DEFAULT_KIND;` |
|   3560 | 5534 | `	if( nArg > 0 ){` |
|   3534 | 5535 | `		zIn = ph7_value_to_string(apArg[0],&nIn);` |
|   1765 | 5536 | `	}` |
|   3560 | 5537 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    526 | 5538 | `		if( DtZoneArgInit(pCtx,apArg[1]) != 0 ){` |
|      5 | 5539 | `			return PH7_OK;` |
|      - | 5540 | `		}` |
|    522 | 5541 | `		DtZoneOf(apArg[1],&iZoneOff,&zZone,&nZone,&iZoneKind);` |
|    260 | 5542 | `	}` |
|   3552 | 5543 | `	if( DtInitState(pCtx,zIn,nIn,iZoneOff,zZone,nZone,iZoneKind,&sState,zNameBuf,` |
|   1780 | 5544 | `		sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0 ){` |
|    440 | 5545 | `		return PH7_VmThrowException(pCtx,"DateMalformedStringException",` |
|      - | 5546 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    146 | 5547 | `			DtCStrLen(zIn,nIn),zIn,iPos,cAt,zErr);` |
|      - | 5548 | `	}` |
|   3264 | 5549 | `	DtStore(pVm,pThis,&sState);` |
|   3264 | 5550 | `	DtSetInit(pVm,pThis);` |
|   3264 | 5551 | `	return PH7_OK;` |
|   1782 | 5552 | `}` |
|      - | 5553 | ``/* One date object's `format()`, shared with the date_format() alias. */`` |
|   3978 | 5554 | `static void DtFormatOf(ph7_context *pCtx,ph7_class_instance *pObj,const char *zFmt,int nFmt)` |
|      4 | 5555 | `{` |
|      - | 5556 | `	dt_state sState;` |
|      - | 5557 | `	Sytm sTm;` |
|      - | 5558 | `	char zZone[64];` |
|      - | 5559 | `	int nName;` |
|   3982 | 5560 | `	DtLoad(pObj,&sState);` |
|   3982 | 5561 | `	nName = sState.nName;` |
|   3982 | 5562 | `	if( nName >= (int)sizeof(zZone) ){` |
|    ! 0 | 5563 | `		nName = (int)sizeof(zZone) - 1;` |
|    ! 0 | 5564 | `	}` |
|   3982 | 5565 | `	SyMemcpy(sState.zName,zZone,(sxu32)nName);` |
|   3982 | 5566 | `	zZone[nName] = 0;` |
|   3982 | 5567 | `	DtFillSytm(sState.iTs,sState.iOff,zZone,&sTm);` |
|      - | 5568 | `	/*` |
|      - | 5569 | ``	 * What `T` prints, and what `I` prints.`` |
|      - | 5570 | `	 *` |
|      - | 5571 | ``	 * php decides `T` on the zone's TYPE. A type-1 fixed OFFSET is spelled`` |
|      - | 5572 | `	 * "GMT+0530"; a type-2 ABBREVIATION and a type-3 IDENTIFIER both print a` |
|      - | 5573 | `	 * NAME -- the abbreviation itself for the first, and for the second the` |
|      - | 5574 | `	 * abbreviation tzdata records for that instant, which is a different string` |
|      - | 5575 | `	 * from the identifier ("EDT", not "America/New_York").` |
|      - | 5576 | `	 *` |
|      - | 5577 | `	 * Handing the name down as tm_abbr is how both say so. The specifier's` |
|      - | 5578 | `	 * fallback -- build "GMT±HHMM" from the offset -- is then reached by` |
|      - | 5579 | `	 * exactly the zones that want it, where it used to be reached by any zone` |
|      - | 5580 | ``	 * whose offset was not zero: a date in zone `T` printed "GMT-0700" for php's`` |
|      - | 5581 | `	 * "T", and so did every other military letter and every abbreviation with an` |
|      - | 5582 | `	 * offset.` |
|      - | 5583 | `	 */` |
|   3982 | 5584 | `	if( sState.iZoneKind == DT_ZONE_ABBR ){` |
|    634 | 5585 | `		sTm.tm_abbr = zZone;` |
|    634 | 5586 | `		sTm.tm_nabbr = nName;` |
|      - | 5587 | `#ifdef PH7_ENABLE_TZDB` |
|      - | 5588 | `		{` |
|      - | 5589 | ``			/* `I` on an abbreviation is the table's own daylight flag, not`` |
|      - | 5590 | ``			 * anything about the instant: a date in `EDT` reads 1 forever and`` |
|      - | 5591 | ``			 * one in `EST` reads 0, because each names one side of the switch`` |
|      - | 5592 | `			 * rather than a place that crosses it. */` |
|      - | 5593 | `			sxi32 iAbbrOff;` |
|      - | 5594 | `			int bDst,nCanon;` |
|      - | 5595 | `			const char *zCanon;` |
|    634 | 5596 | `			if( PH7_TzAbbrFind(zZone,nName,&iAbbrOff,&bDst,&zCanon,&nCanon) ){` |
|    634 | 5597 | `				sTm.tm_isdst = bDst;` |
|    316 | 5598 | `			}` |
|      - | 5599 | `		}` |
|      - | 5600 | `#endif` |
|    318 | 5601 | `	}else{` |
|   3350 | 5602 | `		int iTz = DtTzIndex(sState.zName,sState.nName,sState.iZoneKind);` |
|   3350 | 5603 | `		if( iTz >= 0 ){` |
|    439 | 5604 | `			int bDst = 0,nAbbr = 0;` |
|    439 | 5605 | `			const char *zAbbr = 0;` |
|      - | 5606 | `			/* The offset is re-read with them rather than trusted: it is the` |
|      - | 5607 | `			 * one field two states could disagree about, and this is the door` |
|      - | 5608 | `			 * that prints it. */` |
|    439 | 5609 | `			sxi32 iOff = DtTzOffsetAt(iTz,sState.iOff,sState.iTs,&bDst,&zAbbr,&nAbbr);` |
|    439 | 5610 | `			DtFillSytm(sState.iTs,iOff,zZone,&sTm);` |
|    439 | 5611 | `			sTm.tm_isdst = bDst;` |
|    439 | 5612 | `			sTm.tm_abbr = zAbbr;` |
|    439 | 5613 | `			sTm.tm_nabbr = nAbbr;` |
|    218 | 5614 | `		}` |
|      - | 5615 | `	}` |
|   3982 | 5616 | `	DateFormat(pCtx,zFmt,nFmt,&sTm,sState.uSec);` |
|   3982 | 5617 | `}` |
|      - | 5618 | `/* DateTime::format(string $format) */` |
|   3978 | 5619 | `static int vm_builtin_DateTime_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5620 | `{` |
|   3982 | 5621 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5622 | `	const char *zFmt;` |
|      - | 5623 | `	int nFmt;` |
|   3982 | 5624 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|     11 | 5625 | `		return PH7_OK;` |
|      - | 5626 | `	}` |
|   3972 | 5627 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|   3972 | 5628 | `	DtFormatOf(pCtx,pThis,zFmt,nFmt);` |
|   3972 | 5629 | `	return PH7_OK;` |
|   1993 | 5630 | `}` |
|      - | 5631 | `/* DateTime::getTimestamp() / getMicrosecond() / getOffset() */` |
|     82 | 5632 | `static int vm_builtin_DateTime_getTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5633 | `{` |
|     84 | 5634 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|     41 | 5635 | `	SXUNUSED(nArg);` |
|     41 | 5636 | `	SXUNUSED(apArg);` |
|     84 | 5637 | `	if( pThis ){` |
|     82 | 5638 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_TS));` |
|     40 | 5639 | `	}` |
|     84 | 5640 | `	return PH7_OK;` |
|      2 | 5641 | `}` |
|     14 | 5642 | `static int vm_builtin_DateTime_getMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5643 | `{` |
|     15 | 5644 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      7 | 5645 | `	SXUNUSED(nArg);` |
|      7 | 5646 | `	SXUNUSED(apArg);` |
|     15 | 5647 | `	if( pThis ){` |
|     13 | 5648 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_US));` |
|      6 | 5649 | `	}` |
|     15 | 5650 | `	return PH7_OK;` |
|      1 | 5651 | `}` |
|      6 | 5652 | `static int vm_builtin_DateTime_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5653 | `{` |
|      7 | 5654 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      3 | 5655 | `	SXUNUSED(nArg);` |
|      3 | 5656 | `	SXUNUSED(apArg);` |
|      7 | 5657 | `	if( pThis ){` |
|      5 | 5658 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_OFF));` |
|      2 | 5659 | `	}` |
|      7 | 5660 | `	return PH7_OK;` |
|      1 | 5661 | `}` |
|      - | 5662 | `/* The zone object of a date, built from its stored name and offset, so an` |
|      - | 5663 | ` * identifier PHL stored but cannot re-parse still round-trips. Shared with the` |
|      - | 5664 | ` * date_timezone_get() alias. */` |
|    250 | 5665 | `static int DtTimezoneResult(ph7_context *pCtx,ph7_class_instance *pObj)` |
|      2 | 5666 | `{` |
|    252 | 5667 | `	ph7_vm *pVm = pCtx->pVm;` |
|    252 | 5668 | `	ph7_class *pZoneClass = DtClass(pVm,"DateTimeZone");` |
|      - | 5669 | `	ph7_class_instance *pZone;` |
|      - | 5670 | `	const char *zName;` |
|      - | 5671 | `	int nName;` |
|    252 | 5672 | `	if( pZoneClass == 0 ){` |
|    ! 0 | 5673 | `		return PH7_OK;` |
|      - | 5674 | `	}` |
|    252 | 5675 | `	pZone = DtNewInstance(pVm,pZoneClass);` |
|    252 | 5676 | `	if( pZone == 0 ){` |
|    ! 0 | 5677 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5678 | `	}` |
|    252 | 5679 | `	PH7_NativeAttrStr(pObj,DT_NAME,&zName,&nName);` |
|    252 | 5680 | `	PH7_NativeSetAttrInt(pVm,pZone,DTZ_OFF,PH7_NativeAttrInt(pObj,DT_OFF));` |
|    252 | 5681 | `	PH7_NativeSetAttrStr(pVm,pZone,DTZ_NAME,zName,nName);` |
|    252 | 5682 | `	PH7_NativeSetAttrInt(pVm,pZone,DTZ_KIND,DtZoneKindOf(pObj,DT_ZKIND,zName,nName));` |
|    252 | 5683 | `	PH7_NativeResultObject(pCtx,pZone);` |
|    252 | 5684 | `	return PH7_OK;` |
|    127 | 5685 | `}` |
|      - | 5686 | `/* DateTime::getTimezone() */` |
|    250 | 5687 | `static int vm_builtin_DateTime_getTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5688 | `{` |
|    252 | 5689 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|    125 | 5690 | `	SXUNUSED(nArg);` |
|    125 | 5691 | `	SXUNUSED(apArg);` |
|    252 | 5692 | `	if( pThis == 0 ){` |
|      3 | 5693 | `		return PH7_OK;` |
|      - | 5694 | `	}` |
|    250 | 5695 | `	return DtTimezoneResult(pCtx,pThis);` |
|    127 | 5696 | `}` |
|      - | 5697 | `/* The truth of a value, without converting the caller's copy of it. */` |
|      4 | 5698 | `static int DtValueTruth(ph7_vm *pVm,ph7_value *pVal)` |
|      1 | 5699 | `{` |
|      - | 5700 | `	ph7_value sTmp;` |
|      - | 5701 | `	int bRes;` |
|      5 | 5702 | `	PH7_MemObjInit(&(*pVm),&sTmp);` |
|      5 | 5703 | `	PH7_MemObjStore(pVal,&sTmp);` |
|      - | 5704 | `	/* PH7_MemObjToBool converts IN PLACE and returns a STATUS: the answer is in` |
|      - | 5705 | `	 * x.iVal (reading the return is a silent always-false). */` |
|      5 | 5706 | `	PH7_MemObjToBool(&sTmp);` |
|      5 | 5707 | `	bRes = sTmp.x.iVal != 0;` |
|      5 | 5708 | `	PH7_MemObjRelease(&sTmp);` |
|      5 | 5709 | `	return bRes;` |
|      1 | 5710 | `}` |
|      - | 5711 | `/*` |
|      - | 5712 | ` * Are two dates in the ONE zone, as diff() means it? php compares the loaded` |
|      - | 5713 | ` * timezone STRUCTS, which are cached per spelling, so the test is: both are` |
|      - | 5714 | `` * identifiers, and their names are the same bytes. `America/New_York` and`` |
|      - | 5715 | `` * `america/new_york` name one place and fail it; so do `America/New_York` and`` |
|      - | 5716 | `` * `US/Eastern`, which are the same data under two names.`` |
|      - | 5717 | ` */` |
|    178 | 5718 | `static int DtSameZone(ph7_class_instance *pA,ph7_class_instance *pB)` |
|      1 | 5719 | `{` |
|      - | 5720 | `	const char *zA,*zB;` |
|      - | 5721 | `	int nA,nB;` |
|    179 | 5722 | `	PH7_NativeAttrStr(pA,DT_NAME,&zA,&nA);` |
|    179 | 5723 | `	PH7_NativeAttrStr(pB,DT_NAME,&zB,&nB);` |
|    178 | 5724 | `	if( DtZoneKindOf(pA,DT_ZKIND,zA,nA) != DT_ZONE_ID` |
|    164 | 5725 | `	 \|\| DtZoneKindOf(pB,DT_ZKIND,zB,nB) != DT_ZONE_ID ){` |
|     37 | 5726 | `		return 0;` |
|      - | 5727 | `	}` |
|    196 | 5728 | `	return nA == nB && (nA == 0 \|\| SyMemcmp(zA,zB,(sxu32)nA) == 0);` |
|     90 | 5729 | `}` |
|      - | 5730 | `/* The DateInterval two dates differ by. Shared with the date_diff() alias. */` |
|    178 | 5731 | `static int DtDiffResult(ph7_context *pCtx,ph7_class_instance *pBase,` |
|      - | 5732 | `	ph7_class_instance *pTarget,int bAbsolute)` |
|      1 | 5733 | `{` |
|    179 | 5734 | `	ph7_vm *pVm = pCtx->pVm;` |
|    179 | 5735 | `	ph7_class *pIvClass = DtClass(pVm,"DateInterval");` |
|      - | 5736 | `	ph7_class_instance *pIv;` |
|      - | 5737 | `	dt_diff sDiff;` |
|    179 | 5738 | `	if( pIvClass == 0 ){` |
|    ! 0 | 5739 | `		return PH7_OK;` |
|      - | 5740 | `	}` |
|    446 | 5741 | `	DtCivilDiff(PH7_NativeAttrInt(pBase,DT_TS),(int)PH7_NativeAttrInt(pBase,DT_US),` |
|    178 | 5742 | `		(sxi32)PH7_NativeAttrInt(pBase,DT_OFF),` |
|    178 | 5743 | `		PH7_NativeAttrInt(pTarget,DT_TS),(int)PH7_NativeAttrInt(pTarget,DT_US),` |
|    178 | 5744 | `		(sxi32)PH7_NativeAttrInt(pTarget,DT_OFF),DtSameZone(pBase,pTarget),&sDiff);` |
|    179 | 5745 | `	pIv = DtNewInstance(pVm,pIvClass);` |
|    179 | 5746 | `	if( pIv == 0 ){` |
|    ! 0 | 5747 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5748 | `	}` |
|    179 | 5749 | `	PH7_NativeSetAttrInt(pVm,pIv,"y",sDiff.y);` |
|    179 | 5750 | `	PH7_NativeSetAttrInt(pVm,pIv,"m",sDiff.m);` |
|    179 | 5751 | `	PH7_NativeSetAttrInt(pVm,pIv,"d",sDiff.d);` |
|    179 | 5752 | `	PH7_NativeSetAttrInt(pVm,pIv,"h",sDiff.h);` |
|    179 | 5753 | `	PH7_NativeSetAttrInt(pVm,pIv,"i",sDiff.i);` |
|    179 | 5754 | `	PH7_NativeSetAttrInt(pVm,pIv,"s",sDiff.s);` |
|    179 | 5755 | `	DtIvSetUsec(pVm,pIv,sDiff.uSec);` |
|    179 | 5756 | `	PH7_NativeSetAttrInt(pVm,pIv,"days",sDiff.nDays);` |
|    179 | 5757 | `	PH7_NativeSetAttrInt(pVm,pIv,"invert",bAbsolute ? 0 : sDiff.bInvert);` |
|    179 | 5758 | `	PH7_NativeResultObject(pCtx,pIv);` |
|    179 | 5759 | `	return PH7_OK;` |
|     90 | 5760 | `}` |
|      - | 5761 | `/* DateTime::diff(DateTimeInterface $targetObject, bool $absolute = false) */` |
|    176 | 5762 | `static int vm_builtin_DateTime_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5763 | `{` |
|    177 | 5764 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5765 | `	ph7_class_instance *pTarget;` |
|    177 | 5766 | `	int bAbsolute = 0;` |
|    176 | 5767 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|    175 | 5768 | `	 \|\| DtArgInit(pCtx,apArg[0]) != 0 ){` |
|      5 | 5769 | `		return PH7_OK;` |
|      - | 5770 | `	}` |
|    173 | 5771 | `	pTarget = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    173 | 5772 | `	if( nArg > 1 ){` |
|      5 | 5773 | `		bAbsolute = DtValueTruth(pCtx->pVm,apArg[1]);` |
|      2 | 5774 | `	}` |
|    173 | 5775 | `	return DtDiffResult(pCtx,pThis,pTarget,bAbsolute);` |
|     89 | 5776 | `}` |
|      - | 5777 | `/* DateTime::modify(string $modifier) */` |
|   1424 | 5778 | `static int vm_builtin_DateTime_modify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5779 | `{` |
|   1427 | 5780 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1427 | 5781 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5782 | `	ph7_class_instance *pTarget;` |
|      - | 5783 | `	const char *zMod,*zErr;` |
|   1427 | 5784 | `	int nMod,iPos,bCopy = 0,iErrPos;` |
|      - | 5785 | `	char cAt;` |
|   1427 | 5786 | `	sxi64 iTs = 0;` |
|   1427 | 5787 | `	sxi32 iOff = 0;` |
|   1427 | 5788 | `	int bOffSet = 0,uSec = 0;` |
|      - | 5789 | `	dt_parsed sVec;` |
|   1427 | 5790 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|      5 | 5791 | `		return PH7_OK;` |
|      - | 5792 | `	}` |
|   1423 | 5793 | `	zMod = ph7_value_to_string(apArg[0],&nMod);` |
|      - | 5794 | `	/* php's modify() writes only the fields the string really SET, so a modifier` |
|      - | 5795 | `	 * that names no time of day keeps the receiver's -- DT_PARSE_OVERRIDE_TIME is` |
|      - | 5796 | `	 * php's own flag for exactly that, and the constructor's parse does not pass it. */` |
|   2133 | 5797 | `	iErrPos = DtParseEx(zMod,nMod,PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),` |
|   1420 | 5798 | `		(int)PH7_NativeAttrInt(pThis,DT_US),DT_PARSE_OVERRIDE_TIME\|DT_PARSE_KEEP_ZONE,` |
|    710 | 5799 | `		&iTs,&iOff,&bOffSet,&uSec,&sVec,&pVm->sDtLastErr);` |
|   1423 | 5800 | `	if( iErrPos != 0 ){` |
|    290 | 5801 | `		int bImm = DtIsImmutable(pVm,pThis);` |
|    290 | 5802 | `		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);` |
|    434 | 5803 | `		return PH7_VmThrowException(pCtx,"DateMalformedStringException",` |
|      - | 5804 | `			"%s::modify(): Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    144 | 5805 | `			bImm ? "DateTimeImmutable" : "DateTime",DtCStrLen(zMod,nMod),zMod,iPos,cAt,zErr);` |
|      - | 5806 | `	}` |
|   1135 | 5807 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|   1135 | 5808 | `	DtStoreModified(pVm,pTarget,iTs,(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),&sVec);` |
|      - | 5809 | ``	/* The modifier may have moved the SUB-SECOND clock too (`+1 microsecond`,`` |
|      - | 5810 | ``	 * `+250 ms`) or set it outright (a time of day with a fraction); the parse`` |
|      - | 5811 | `	 * started from the object's own, so this is the whole answer either way. */` |
|   1135 | 5812 | `	PH7_NativeSetAttrInt(pVm,pTarget,DT_US,uSec);` |
|   1135 | 5813 | `	DtEpochRezone(pVm,pTarget,&sVec);` |
|   1135 | 5814 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|   1135 | 5815 | `	return PH7_OK;` |
|    715 | 5816 | `}` |
|      - | 5817 | `/* DateTime::setTimestamp(int $timestamp) — php clears the microseconds with it */` |
|     36 | 5818 | `static int vm_builtin_DateTime_setTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5819 | `{` |
|     37 | 5820 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5821 | `	ph7_class_instance *pTarget;` |
|     37 | 5822 | `	int bCopy = 0;` |
|     37 | 5823 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|      3 | 5824 | `		return PH7_OK;` |
|      - | 5825 | `	}` |
|     35 | 5826 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     35 | 5827 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_TS,ph7_value_to_int64(apArg[0]));` |
|     35 | 5828 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_US,0);` |
|      - | 5829 | `	/* This door names an INSTANT, so the zone keeps its name and the offset` |
|      - | 5830 | `	 * follows the new timestamp. */` |
|     35 | 5831 | `	DtRezone(pCtx->pVm,pTarget);` |
|     35 | 5832 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     35 | 5833 | `	return PH7_OK;` |
|     19 | 5834 | `}` |
|      - | 5835 | `/* DateTime::setMicrosecond(int $microsecond) */` |
|     44 | 5836 | `static int vm_builtin_DateTime_setMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5837 | `{` |
|     45 | 5838 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5839 | `	ph7_class_instance *pTarget;` |
|      - | 5840 | `	sxi64 iUs;` |
|     45 | 5841 | `	int bCopy = 0;` |
|     45 | 5842 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|      3 | 5843 | `		return PH7_OK;` |
|      - | 5844 | `	}` |
|     43 | 5845 | `	iUs = ph7_value_to_int64(apArg[0]);` |
|     43 | 5846 | `	if( iUs < 0 \|\| iUs > 999999 ){` |
|      - | 5847 | `		/* php's range refusal, and the reason a date's microseconds can be` |
|      - | 5848 | `		 * assumed to be a fraction of ONE second everywhere else: PHL stored` |
|      - | 5849 | ``		 * whatever int it was handed, so `setMicrosecond(1000000)` formatted as`` |
|      - | 5850 | ``		 * `00:00:00.1000000` and a negative one as `00:00:00.-00001` -- neither`` |
|      - | 5851 | `		 * of them a time. The message names the DECLARING class, so a subclass` |
|      - | 5852 | `		 * of DateTime still reports DateTime. */` |
|     40 | 5853 | `		return PH7_VmThrowException(pCtx,"DateRangeError",` |
|      - | 5854 | `			"%s::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, %qd given",` |
|     26 | 5855 | `			DtIsImmutable(pCtx->pVm,pThis) ? "DateTimeImmutable" : "DateTime",iUs);` |
|      - | 5856 | `	}` |
|     17 | 5857 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     17 | 5858 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_US,iUs);` |
|     17 | 5859 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     17 | 5860 | `	return PH7_OK;` |
|     23 | 5861 | `}` |
|      - | 5862 | `/* DateTime::setTimezone(DateTimeZone $timezone) */` |
|    344 | 5863 | `static int vm_builtin_DateTime_setTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5864 | `{` |
|    346 | 5865 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5866 | `	ph7_class_instance *pTarget;` |
|    346 | 5867 | `	const char *zName = "UTC";` |
|    346 | 5868 | `	int nName = 3,bCopy = 0,iKind = DT_ZONE_ID;` |
|    346 | 5869 | `	sxi32 iOff = 0;` |
|    346 | 5870 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|      3 | 5871 | `		return PH7_OK;` |
|      - | 5872 | `	}` |
|    344 | 5873 | `	if( !DtZoneOf(apArg[0],&iOff,&zName,&nName,&iKind) ){` |
|    ! 0 | 5874 | `		return PH7_OK;` |
|      - | 5875 | `	}` |
|    344 | 5876 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|    344 | 5877 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_OFF,iOff);` |
|    344 | 5878 | `	PH7_NativeSetAttrStr(pCtx->pVm,pTarget,DT_NAME,zName,nName);` |
|    344 | 5879 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_ZKIND,iKind);` |
|      - | 5880 | `	/* The instant does not move -- setTimezone() re-expresses it -- so the new` |
|      - | 5881 | `	 * zone's offset is read at the timestamp already there. */` |
|    344 | 5882 | `	DtRezone(pCtx->pVm,pTarget);` |
|    344 | 5883 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|    344 | 5884 | `	return PH7_OK;` |
|    174 | 5885 | `}` |
|      - | 5886 | `/* Replace the DATE of an object, keeping its time of day. Shared with the` |
|      - | 5887 | ` * date_date_set() alias. */` |
|     64 | 5888 | `static void DtSetDateOf(ph7_context *pCtx,ph7_class_instance *pObj,sxi64 y,int mo,int d)` |
|      1 | 5889 | `{` |
|     65 | 5890 | `	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);` |
|     65 | 5891 | `	sxi64 iDays = DtFloorDiv(iLocal,86400);` |
|     65 | 5892 | `	sxi64 iSecs = iLocal - iDays*86400;` |
|     97 | 5893 | `	DtStoreLocalOf(pCtx->pVm,pObj,` |
|     64 | 5894 | `		DtMakeTs(y,mo,d,(int)(iSecs / 3600),(int)((iSecs / 60) % 60),(int)(iSecs % 60),0));` |
|     65 | 5895 | `}` |
|      - | 5896 | `/* Replace the TIME of day, keeping the date. Shared with date_time_set(). */` |
|     26 | 5897 | `static void DtSetTimeOf(ph7_context *pCtx,ph7_class_instance *pObj,int h,int mi,int s,sxi64 uSec)` |
|      1 | 5898 | `{` |
|     27 | 5899 | `	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);` |
|     27 | 5900 | `	sxi64 iDays = DtFloorDiv(iLocal,86400);` |
|      - | 5901 | `	sxi64 y;` |
|      - | 5902 | `	int mo,d;` |
|     27 | 5903 | `	DtCivilFromDays(iDays,&y,&mo,&d);` |
|     27 | 5904 | `	DtStoreLocalOf(pCtx->pVm,pObj,DtMakeTs(y,mo,d,h,mi,s,0));` |
|     27 | 5905 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,uSec);` |
|     27 | 5906 | `}` |
|      - | 5907 | `/* DateTime::setDate(int $year, int $month, int $day) — the time of day is kept */` |
|     66 | 5908 | `static int vm_builtin_DateTime_setDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5909 | `{` |
|     67 | 5910 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5911 | `	ph7_class_instance *pTarget;` |
|     67 | 5912 | `	int bCopy = 0;` |
|     67 | 5913 | `	if( pThis == 0 \|\| nArg < 3 ){` |
|      3 | 5914 | `		return PH7_OK;` |
|      - | 5915 | `	}` |
|     65 | 5916 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     97 | 5917 | `	DtSetDateOf(pCtx,pTarget,ph7_value_to_int64(apArg[0]),ph7_value_to_int(apArg[1]),` |
|     64 | 5918 | `		ph7_value_to_int(apArg[2]));` |
|     65 | 5919 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     65 | 5920 | `	return PH7_OK;` |
|     34 | 5921 | `}` |
|      - | 5922 | `/* DateTime::setTime(int $hour, int $minute, int $second = 0, int $microsecond = 0) */` |
|     28 | 5923 | `static int vm_builtin_DateTime_setTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5924 | `{` |
|     29 | 5925 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5926 | `	ph7_class_instance *pTarget;` |
|     29 | 5927 | `	int bCopy = 0;` |
|     29 | 5928 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|      3 | 5929 | `		return PH7_OK;` |
|      - | 5930 | `	}` |
|     27 | 5931 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     54 | 5932 | `	DtSetTimeOf(pCtx,pTarget,ph7_value_to_int(apArg[0]),ph7_value_to_int(apArg[1]),` |
|     25 | 5933 | `		nArg > 2 ? ph7_value_to_int(apArg[2]) : 0,` |
|     15 | 5934 | `		nArg > 3 ? ph7_value_to_int64(apArg[3]) : 0);` |
|     27 | 5935 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     27 | 5936 | `	return PH7_OK;` |
|     15 | 5937 | `}` |
|      - | 5938 | `/* DateTime::setISODate(int $year, int $week, int $dayOfWeek = 1) */` |
|     26 | 5939 | `static int vm_builtin_DateTime_setISODate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5940 | `{` |
|     27 | 5941 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5942 | `	ph7_class_instance *pTarget;` |
|     27 | 5943 | `	int bCopy = 0;` |
|     27 | 5944 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|      3 | 5945 | `		return PH7_OK;` |
|      - | 5946 | `	}` |
|     25 | 5947 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|      - | 5948 | `	{` |
|      - | 5949 | `		/* Another civil-fields door: DtIsoDate puts the offset back on, and a` |
|      - | 5950 | `		 * database zone re-solves the reading it produced. */` |
|     25 | 5951 | `		sxi32 iOff = (sxi32)PH7_NativeAttrInt(pThis,DT_OFF);` |
|     37 | 5952 | `		DtStoreLocalOf(pCtx->pVm,pTarget,` |
|     48 | 5953 | `			(sxi64)((sxu64)DtIsoDate(PH7_NativeAttrInt(pThis,DT_TS),iOff,` |
|     24 | 5954 | `				ph7_value_to_int64(apArg[0]),ph7_value_to_int64(apArg[1]),` |
|     35 | 5955 | `				nArg > 2 ? ph7_value_to_int64(apArg[2]) : 1) + (sxu64)iOff));` |
|      - | 5956 | `	}` |
|     25 | 5957 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     25 | 5958 | `	return PH7_OK;` |
|     14 | 5959 | `}` |
|      - | 5960 | `/*` |
|      - | 5961 | ` * One interval applied to a date -- the whole of what add(), sub(), their two` |
|      - | 5962 | ` * procedural aliases and the DatePeriod walk each did by hand.` |
|      - | 5963 | ` *` |
|      - | 5964 | `` * The MICROSECONDS are php's `f`, and php's `f` is a signed count of SECONDS'`` |
|      - | 5965 | ` * fractions that moves the clock like any other field: an interval carrying` |
|      - | 5966 | ` * f = 2.5 and nothing else moves it two and a half seconds, and its carry into` |
|      - | 5967 | ` * the second is ordinary floor division (so a sub() past the second borrows).` |
|      - | 5968 | `` * PHL ignored `f` at all four sites, which left `DatePeriod` over a sub-second`` |
|      - | 5969 | ` * interval standing STILL -- every step answering the start date.` |
|      - | 5970 | ` *` |
|      - | 5971 | `` * The arithmetic is done on unsigned intermediates: `f` is a whole int64 count`` |
|      - | 5972 | ` * of microseconds a script may write anything into, and the sum of two of them` |
|      - | 5973 | ` * is exactly the wrap php's own C arrives at rather than an overflow this build` |
|      - | 5974 | ` * would trap on.` |
|      - | 5975 | ` *` |
|      - | 5976 | `` * iSign is the FINAL direction, `invert` already folded in by whichever caller`` |
|      - | 5977 | ` * honours it -- add()/sub() and their aliases do, and the period walk does not` |
|      - | 5978 | ` * (php's own split, below).` |
|      - | 5979 | ` */` |
|    398 | 5980 | `static void DtApplyInterval(ph7_vm *pVm,ph7_class_instance *pSrc,ph7_class_instance *pDst,` |
|      - | 5981 | `	ph7_class_instance *pIv,int iSign)` |
|      1 | 5982 | `{` |
|      - | 5983 | `	sxi64 iUsIv,iUs,iCarry;` |
|    399 | 5984 | `	iUsIv = DtIvUsec(pIv);` |
|    399 | 5985 | `	if( iSign < 0 ){` |
|     59 | 5986 | `		iUsIv = (sxi64)((sxu64)0 - (sxu64)iUsIv);` |
|     29 | 5987 | `	}` |
|    399 | 5988 | `	iUs = (sxi64)((sxu64)PH7_NativeAttrInt(pSrc,DT_US) + (sxu64)iUsIv);` |
|    399 | 5989 | `	iCarry = DtFloorDiv(iUs,1000000);` |
|      - | 5990 | `	{` |
|      - | 5991 | `		/* DtCivilAdd walks the CIVIL fields and hands back an instant, having` |
|      - | 5992 | `		 * put the source's offset back on at the end. A database zone may not` |
|      - | 5993 | `		 * still be on that offset where it landed -- adding six months to a` |
|      - | 5994 | `		 * January date in New York crosses into daylight time -- so the offset` |
|      - | 5995 | `		 * is peeled back off and the reading re-solved. php's arithmetic is` |
|      - | 5996 | ``		 * wall-clock arithmetic for exactly this reason: `+1 day` over a spring`` |
|      - | 5997 | `		 * forward is 23 hours of real time and the clock still reads the same.` |
|      - | 5998 | `		 */` |
|    399 | 5999 | `		sxi32 iOffSrc = (sxi32)PH7_NativeAttrInt(pSrc,DT_OFF);` |
|    797 | 6000 | `		sxi64 iLocal = (sxi64)((sxu64)DtCivilAdd(PH7_NativeAttrInt(pSrc,DT_TS),iOffSrc,` |
|    199 | 6001 | `			PH7_NativeAttrInt(pIv,"y"),PH7_NativeAttrInt(pIv,"m"),PH7_NativeAttrInt(pIv,"d"),` |
|    199 | 6002 | `			PH7_NativeAttrInt(pIv,"h"),PH7_NativeAttrInt(pIv,"i"),PH7_NativeAttrInt(pIv,"s"),iSign)` |
|    398 | 6003 | `			+ (sxu64)iCarry + (sxu64)iOffSrc);` |
|    399 | 6004 | `		DtStoreLocalOf(pVm,pDst,iLocal);` |
|      - | 6005 | `	}` |
|    598 | 6006 | `	PH7_NativeSetAttrInt(pVm,pDst,DT_US,` |
|    398 | 6007 | `		(sxi64)((sxu64)iUs - (sxu64)iCarry * 1000000));` |
|    399 | 6008 | `}` |
|      - | 6009 | `/* add()/sub(): one body, the sign is the difference. */` |
|    126 | 6010 | `static int DtAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)` |
|      1 | 6011 | `{` |
|    127 | 6012 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 6013 | `	ph7_class_instance *pTarget,*pIv;` |
|    127 | 6014 | `	int bCopy = 0;` |
|    126 | 6015 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|    123 | 6016 | `	 \|\| DtArgInit(pCtx,apArg[0]) != 0 ){` |
|      9 | 6017 | `		return PH7_OK;` |
|      - | 6018 | `	}` |
|    119 | 6019 | `	pIv = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    119 | 6020 | `	if( PH7_NativeAttrInt(pIv,"invert") ){` |
|     17 | 6021 | `		iSign = -iSign;   /* an inverted interval subtracts from add() (php) */` |
|      8 | 6022 | `	}` |
|    119 | 6023 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|    119 | 6024 | `	DtApplyInterval(pCtx->pVm,pThis,pTarget,pIv,iSign);` |
|    119 | 6025 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|    119 | 6026 | `	return PH7_OK;` |
|     64 | 6027 | `}` |
|     72 | 6028 | `static int vm_builtin_DateTime_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6029 | `{` |
|     73 | 6030 | `	return DtAddSub(pCtx,nArg,apArg,1);` |
|      1 | 6031 | `}` |
|     54 | 6032 | `static int vm_builtin_DateTime_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6033 | `{` |
|     55 | 6034 | `	return DtAddSub(pCtx,nArg,apArg,-1);` |
|      1 | 6035 | `}` |
|      - | 6036 | ``/* DateTime::getLastErrors() — php's array, or `false` when the last parse was clean */`` |
|    502 | 6037 | `static int vm_builtin_DateTime_getLastErrors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6038 | `{` |
|    503 | 6039 | `	ph7_vm *pVm = pCtx->pVm;` |
|    503 | 6040 | `	phl_dt_lasterr *pErr = &pVm->sDtLastErr;` |
|      - | 6041 | `	ph7_value *pArr,*pWarn,*pErrs,*pVal;` |
|      - | 6042 | `	int k;` |
|    251 | 6043 | `	SXUNUSED(nArg);` |
|    251 | 6044 | `	SXUNUSED(apArg);` |
|    503 | 6045 | `	if( !pErr->bSet ){` |
|    259 | 6046 | `		ph7_result_bool(pCtx,0);` |
|    259 | 6047 | `		return PH7_OK;` |
|      - | 6048 | `	}` |
|    245 | 6049 | `	pArr = ph7_context_new_array(pCtx);` |
|    245 | 6050 | `	pWarn = ph7_context_new_array(pCtx);` |
|    245 | 6051 | `	pErrs = ph7_context_new_array(pCtx);` |
|    245 | 6052 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    245 | 6053 | `	if( pArr == 0 \|\| pWarn == 0 \|\| pErrs == 0 \|\| pVal == 0 ){` |
|    ! 0 | 6054 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6055 | `	}` |
|    317 | 6056 | `	for( k = 0 ; k < pErr->nWarnKept ; k++ ){` |
|     73 | 6057 | `		ph7_value_string(pVal,pErr->azWarn[k],-1);` |
|     73 | 6058 | `		ph7_array_add_intkey_elem(pWarn,pErr->aWarnPos[k],pVal);` |
|     73 | 6059 | `		ph7_value_reset_string_cursor(pVal);` |
|     37 | 6060 | `	}` |
|      - | 6061 | `	{` |
|    245 | 6062 | `		const phl_dt_diag_row *aRow = (const phl_dt_diag_row *)SyBlobData(&pErr->sErr);` |
|    483 | 6063 | `		for( k = 0 ; k < pErr->nErrKept ; k++ ){` |
|    239 | 6064 | `			ph7_value_string(pVal,aRow[k].zMsg,-1);` |
|    239 | 6065 | `			ph7_array_add_intkey_elem(pErrs,aRow[k].iPos,pVal);` |
|    239 | 6066 | `			ph7_value_reset_string_cursor(pVal);` |
|    120 | 6067 | `		}` |
|      - | 6068 | `	}` |
|    245 | 6069 | `	ph7_value_int(pVal,pErr->nWarn);` |
|    245 | 6070 | `	ph7_array_add_strkey_elem(pArr,"warning_count",pVal);` |
|    245 | 6071 | `	ph7_array_add_strkey_elem(pArr,"warnings",pWarn);` |
|    245 | 6072 | `	ph7_value_int(pVal,pErr->nErr);` |
|    245 | 6073 | `	ph7_array_add_strkey_elem(pArr,"error_count",pVal);` |
|    245 | 6074 | `	ph7_array_add_strkey_elem(pArr,"errors",pErrs);` |
|    245 | 6075 | `	ph7_result_value(pCtx,pArr);` |
|    245 | 6076 | `	return PH7_OK;` |
|    252 | 6077 | `}` |
|      - | 6078 | `/*` |
|      - | 6079 | ` * The class a static factory builds. php uses LATE STATIC BINDING here, so` |
|      - | 6080 | `` * `D::createFromFormat()` on a subclass answers a D — where the chunk hardcoded`` |
|      - | 6081 | ` * the literal class name and always answered a DateTime.` |
|      - | 6082 | ` */` |
|    810 | 6083 | `static ph7_class * DtFactoryClass(ph7_context *pCtx,const char *zFallback)` |
|      2 | 6084 | `{` |
|    812 | 6085 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|    812 | 6086 | `	return pClass ? pClass : DtClass(pCtx->pVm,zFallback);` |
|      2 | 6087 | `}` |
|      - | 6088 | `/* DateTime::createFromFormat(string $format, string $datetime, ?DateTimeZone $timezone = null) */` |
|    522 | 6089 | `static int DtCreateFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)` |
|      2 | 6090 | `{` |
|    524 | 6091 | `	ph7_vm *pVm = pCtx->pVm;` |
|    524 | 6092 | `	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);` |
|      - | 6093 | `	ph7_class_instance *pObj;` |
|      - | 6094 | `	dt_ff_res sRes;` |
|      - | 6095 | `	dt_state sState;` |
|      - | 6096 | `	int iZoneKind;` |
|      - | 6097 | `	char zNameBuf[16];` |
|      - | 6098 | `	const char *zZone;` |
|      - | 6099 | `	int nZone;` |
|    524 | 6100 | `	sxi32 iZoneOff = 0,iFillOff;` |
|      - | 6101 | `	const char *zFmt,*zIn;` |
|    524 | 6102 | `	int nFmt,nIn,iNowUs = 0,iResUs = 0,iTzFf;` |
|    524 | 6103 | `	sxi64 iNowFf = 0;` |
|    524 | 6104 | `	if( pClass == 0 \|\| nArg < 2 ){` |
|    ! 0 | 6105 | `		return PH7_OK;` |
|      - | 6106 | `	}` |
|    524 | 6107 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    524 | 6108 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    524 | 6109 | `	zZone = pVm->zDefTz;` |
|    524 | 6110 | `	nZone = (int)pVm->nDefTz;` |
|    524 | 6111 | `	iZoneKind = DT_ZONE_DEFAULT_KIND;` |
|    524 | 6112 | `	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    467 | 6113 | `		if( DtZoneArgInit(pCtx,apArg[2]) != 0 ){` |
|      3 | 6114 | `			return PH7_OK;` |
|      - | 6115 | `		}` |
|    465 | 6116 | `		DtZoneOf(apArg[2],&iZoneOff,&zZone,&nZone,&iZoneKind);` |
|    232 | 6117 | `	}` |
|    522 | 6118 | `	DtNowUs(pCtx->pVm,&iNowFf,&iNowUs);` |
|      - | 6119 | `	/* The same two-step a database zone owes the constructor's parse: read the` |
|      - | 6120 | `	 * scan against the offset at NOW -- every field the format did not fill` |
|      - | 6121 | `	 * comes from that moment -- then re-solve the WALL-CLOCK reading it` |
|      - | 6122 | `	 * produced against the zone. Without it a format that reads a plain local` |
|      - | 6123 | `	 * time answers the instant that reading names in UTC. */` |
|    522 | 6124 | `	iTzFf = DtTzIndex(zZone,nZone,iZoneKind);` |
|    522 | 6125 | `	if( iTzFf >= 0 ){` |
|      - | 6126 | `		int bDstFf,nAbbrFf;` |
|      - | 6127 | `		const char *zAbbrFf;` |
|    ! 0 | 6128 | `		iZoneOff = DtTzOffsetAt(iTzFf,iZoneOff,iNowFf,&bDstFf,&zAbbrFf,&nAbbrFf);` |
|    ! 0 | 6129 | `	}` |
|    522 | 6130 | `	iFillOff = iZoneOff;` |
|    522 | 6131 | `	if( DtFromFormat(zFmt,nFmt,zIn,nIn,&sRes) != 0 ){` |
|    165 | 6132 | `		DtLastErrFf(pVm,&sRes.sDiag);` |
|    165 | 6133 | `		ph7_result_bool(pCtx,0);` |
|    165 | 6134 | `		return PH7_OK;` |
|      - | 6135 | `	}` |
|    358 | 6136 | `	DtLastErrFf(pVm,&sRes.sDiag);` |
|      - | 6137 | `	/* ...and again for a database zone the FORMAT itself read, which arrives with` |
|      - | 6138 | `	 * no offset of its own: it displaces the call's zone as the clock every unset` |
|      - | 6139 | ``	 * field is filled from -- `createFromFormat('e','Pacific/Auckland')` is the`` |
|      - | 6140 | `	 * moment in Auckland whatever zone the call was made in -- and the reading it` |
|      - | 6141 | `	 * produces is local time THERE. */` |
|    358 | 6142 | `	if( sRes.iOffKind == DT_ZONE_ID ){` |
|     20 | 6143 | `		int iTzRes = DtTzIndex(sRes.zName,sRes.nName,DT_ZONE_ID);` |
|     20 | 6144 | `		if( iTzRes >= 0 ){` |
|      - | 6145 | `			int bDstRes,nAbbrRes;` |
|      - | 6146 | `			const char *zAbbrRes;` |
|      7 | 6147 | `			iTzFf = iTzRes;` |
|      7 | 6148 | `			sRes.iOff = DtTzOffsetAt(iTzRes,0,iNowFf,&bDstRes,&zAbbrRes,&nAbbrRes);` |
|      7 | 6149 | `			iFillOff = sRes.iOff;` |
|      3 | 6150 | `		}` |
|      9 | 6151 | `	}` |
|    358 | 6152 | `	sState.iTs = DtFfResolve(&sRes,iNowFf,iNowUs,iFillOff,&iResUs);` |
|    358 | 6153 | `	sState.uSec = iResUs;` |
|    358 | 6154 | `	if( sRes.iOffKind == 0 ){` |
|      - | 6155 | `		/* nothing the format read resolved, so the call's own zone stands */` |
|    267 | 6156 | `		sState.iOff = iZoneOff;` |
|    267 | 6157 | `		sState.zName = zZone;` |
|    267 | 6158 | `		sState.nName = nZone;` |
|    267 | 6159 | `		sState.iZoneKind = iZoneKind;` |
|      - | 6160 | `#ifdef PH7_ENABLE_TZDB` |
|    267 | 6161 | `		if( iTzFf >= 0 ){` |
|    ! 0 | 6162 | `			sxi64 iFixed = sState.iTs;` |
|    ! 0 | 6163 | `			sxi32 iOffAt = iZoneOff;` |
|    ! 0 | 6164 | `			if( PH7_TzLocalToUtc(iTzFf,sState.iTs + iZoneOff,&iFixed,&iOffAt) ){` |
|    ! 0 | 6165 | `				sState.iTs = iFixed;` |
|    ! 0 | 6166 | `				sState.iOff = iOffAt;` |
|    ! 0 | 6167 | `			}` |
|      1 | 6168 | `		}` |
|      - | 6169 | `#endif` |
|    225 | 6170 | `	}else if( sRes.iOffKind == DT_ZONE_OFFSET ){` |
|     47 | 6171 | `		sState.iOff = sRes.iOff;` |
|     47 | 6172 | `		sState.nName = DtOffNameSec(zNameBuf,sizeof(zNameBuf),sRes.iOff);` |
|     47 | 6173 | `		sState.zName = zNameBuf;` |
|     47 | 6174 | `		sState.iZoneKind = DT_ZONE_OFFSET;` |
|     24 | 6175 | `	}else{` |
|     46 | 6176 | `		sState.iOff = sRes.iOff;` |
|     46 | 6177 | `		sState.zName = sRes.zName;` |
|     46 | 6178 | `		sState.nName = sRes.nName;` |
|     46 | 6179 | `		sState.iZoneKind = sRes.iOffKind;` |
|      - | 6180 | `#ifdef PH7_ENABLE_TZDB` |
|     46 | 6181 | `		if( sRes.iOffKind == DT_ZONE_ID && iTzFf >= 0 ){` |
|      7 | 6182 | `			sxi64 iFixed = sState.iTs;` |
|      7 | 6183 | `			sxi32 iOffAt = sRes.iOff;` |
|      7 | 6184 | `			if( PH7_TzLocalToUtcFirst(iTzFf,sState.iTs + sRes.iOff,&iFixed,&iOffAt) ){` |
|      7 | 6185 | `				sState.iTs = iFixed;` |
|      7 | 6186 | `				sState.iOff = iOffAt;` |
|      3 | 6187 | `			}` |
|      3 | 6188 | `		}` |
|      - | 6189 | `#endif` |
|      - | 6190 | `	}` |
|    358 | 6191 | `	pObj = DtNewInstance(pVm,pClass);` |
|    358 | 6192 | `	if( pObj == 0 ){` |
|    ! 0 | 6193 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6194 | `	}` |
|    358 | 6195 | `	DtStore(pVm,pObj,&sState);` |
|    358 | 6196 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    358 | 6197 | `	return PH7_OK;` |
|    263 | 6198 | `}` |
|    506 | 6199 | `static int vm_builtin_DateTime_createFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6200 | `{` |
|    508 | 6201 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTime");` |
|      2 | 6202 | `}` |
|      2 | 6203 | `static int vm_builtin_DateTimeImmutable_createFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6204 | `{` |
|      3 | 6205 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 6206 | `}` |
|      - | 6207 | `/* createFromImmutable()/createFromMutable()/createFromInterface(): one copy body */` |
|     18 | 6208 | `static int DtCopyOf(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)` |
|      1 | 6209 | `{` |
|     19 | 6210 | `	ph7_vm *pVm = pCtx->pVm;` |
|     19 | 6211 | `	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);` |
|      - | 6212 | `	ph7_class_instance *pSrc,*pObj;` |
|      - | 6213 | `	dt_state sState;` |
|     18 | 6214 | `	if( pClass == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|     19 | 6215 | `	 \|\| DtArgInit(pCtx,apArg[0]) != 0 ){` |
|      5 | 6216 | `		return PH7_OK;` |
|      - | 6217 | `	}` |
|     15 | 6218 | `	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     15 | 6219 | `	DtLoad(pSrc,&sState);` |
|     15 | 6220 | `	pObj = DtNewInstance(pVm,pClass);` |
|     15 | 6221 | `	if( pObj == 0 ){` |
|    ! 0 | 6222 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6223 | `	}` |
|     15 | 6224 | `	DtStore(pVm,pObj,&sState);` |
|     15 | 6225 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     15 | 6226 | `	return PH7_OK;` |
|     10 | 6227 | `}` |
|      - | 6228 | `/*` |
|      - | 6229 | `` * php's rendering of the timestamp its DateRangeError names -- its own `%g`:`` |
|      - | 6230 | ` * six significant digits, and an exponent form that keeps a fractional digit,` |
|      - | 6231 | ` * so 2^63 prints "9.22337e+18" and 1e19 prints "1.0e+19". NaN and the` |
|      - | 6232 | ` * infinities print as the bare words php prints them as everywhere else.` |
|      - | 6233 | ` *` |
|      - | 6234 | ` * libc is the digit engine (the byte-exact-floats rule the printf family` |
|      - | 6235 | ` * already follows) and PH7_PhpFloatShape turns its output into php's shape.` |
|      - | 6236 | ` */` |
|     12 | 6237 | `static void DtRealText(double r,char *zBuf,int nBuf)` |
|      1 | 6238 | `{` |
|      - | 6239 | `	int n;` |
|     13 | 6240 | `	if( PH7_IS_NAN(r) ){` |
|      3 | 6241 | `		SyMemcpy("NAN",zBuf,sizeof("NAN"));` |
|      3 | 6242 | `		return;` |
|      - | 6243 | `	}` |
|     11 | 6244 | `	if( PH7_IS_INF(r) ){` |
|      5 | 6245 | `		SyMemcpy(r < 0 ? "-INF" : "INF",zBuf,r < 0 ? sizeof("-INF") : sizeof("INF"));` |
|      5 | 6246 | `		return;` |
|      - | 6247 | `	}` |
|      7 | 6248 | `	n = snprintf(zBuf,(size_t)nBuf,"%.6g",r);` |
|      7 | 6249 | `	if( n < 0 \|\| n >= nBuf - 2 ){` |
|    ! 0 | 6250 | `		zBuf[0] = 0;` |
|    ! 0 | 6251 | `		return;` |
|      - | 6252 | `	}` |
|      7 | 6253 | `	zBuf[PH7_PhpFloatShape(zBuf,n,1)] = 0;` |
|      7 | 6254 | `}` |
|      - | 6255 | `/*` |
|      - | 6256 | ` * DateTime::createFromTimestamp(int\|float $timestamp) (php 8.4), and the same` |
|      - | 6257 | ` * on DateTimeImmutable -- the float door onto the clock, and the only factory` |
|      - | 6258 | ` * that reads MICROSECONDS out of its argument.` |
|      - | 6259 | ` *` |
|      - | 6260 | ` * The zone is a fixed +00:00 whatever the default timezone is, exactly as` |
|      - | 6261 | `` * `new DateTime('@0')` answers; the seconds floor and the fraction rounds to`` |
|      - | 6262 | ` * the nearest microsecond (php's 1.9999999 is 2.000000, and its -1.9999999 is` |
|      - | 6263 | ` * -2.000000, both of which fall out of taking the floor first).` |
|      - | 6264 | ` */` |
|     60 | 6265 | `static int DtCreateFromTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)` |
|      1 | 6266 | `{` |
|     61 | 6267 | `	ph7_vm *pVm = pCtx->pVm;` |
|     61 | 6268 | `	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);` |
|      - | 6269 | `	ph7_class_instance *pObj;` |
|      - | 6270 | `	dt_state sState;` |
|      - | 6271 | `	char zNameBuf[16];` |
|     61 | 6272 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 6273 | `		return PH7_OK;` |
|      - | 6274 | `	}` |
|     61 | 6275 | `	sState.uSec = 0;` |
|      - | 6276 | `	{` |
|      - | 6277 | ``		/* Which ARM of `int\|float` the argument satisfies decides the rest, and`` |
|      - | 6278 | `		 * a numeric STRING picks its own: php reads "5" as an int and "5.5" as` |
|      - | 6279 | `		 * a float, so the SHAPE of the digits is the test rather than the` |
|      - | 6280 | `		 * value's storage. Everything that is not a number at all was refused` |
|      - | 6281 | `		 * upstream by the declared type. */` |
|     61 | 6282 | `		double r = 0.0;` |
|     61 | 6283 | `		int bReal = 0;` |
|     61 | 6284 | `		if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|     31 | 6285 | `			bReal = 1;` |
|     31 | 6286 | `			r = (double)apArg[0]->rVal;` |
|     46 | 6287 | `		}else if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|      - | 6288 | `			int nStr;` |
|      5 | 6289 | `			const char *zStr = ph7_value_to_string(apArg[0],&nStr);` |
|      - | 6290 | `			sxi64 iLong;` |
|      - | 6291 | `			double dReal;` |
|      5 | 6292 | `			if( RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal) == RANGE_IN_DOUBLE ){` |
|      3 | 6293 | `				bReal = 1;` |
|      3 | 6294 | `				r = dReal;` |
|      1 | 6295 | `			}` |
|      2 | 6296 | `		}` |
|     61 | 6297 | `		if( !bReal ){` |
|     29 | 6298 | `			sState.iTs = ph7_value_to_int64(apArg[0]);` |
|     47 | 6299 | `		}else if( !PH7_RealFitsInt64(r) ){` |
|      - | 6300 | `			/* php's own bounds, and its own words for them: the ceiling is` |
|      - | 6301 | `			 * printed as the last microsecond below 2^63 even though the test` |
|      - | 6302 | `			 * is against 2^63 itself (no double lies between the two).` |
|      - | 6303 | `			 * "%z" takes the class name as the length+pointer pair it is. */` |
|      - | 6304 | `			char zVal[64];` |
|     13 | 6305 | `			DtRealText(r,zVal,(int)sizeof(zVal));` |
|     19 | 6306 | `			return PH7_VmThrowException(pCtx,"DateRangeError",` |
|      - | 6307 | `				"%z::createFromTimestamp(): Argument #1 ($timestamp) must be a finite "` |
|      - | 6308 | `				"number between -9223372036854775808 and 9223372036854775807.999999, "` |
|      6 | 6309 | `				"%s given",&pClass->sDisp,zVal);` |
|    ! 0 | 6310 | `		}else{` |
|      - | 6311 | `			/* floor(), by hand: <math.h> belongs to the optional math module and` |
|      - | 6312 | `			 * the clock does not depend on it. The C cast truncates toward zero,` |
|      - | 6313 | `			 * so only a negative value with a fraction needs the step down. */` |
|      - | 6314 | `			double fFrac;` |
|     21 | 6315 | `			sState.iTs = (sxi64)r;` |
|     21 | 6316 | `			if( (double)sState.iTs > r ){` |
|      9 | 6317 | `				sState.iTs--;` |
|      4 | 6318 | `			}` |
|     21 | 6319 | `			fFrac = (r - (double)sState.iTs) * 1000000.0;` |
|     21 | 6320 | `			sState.uSec = (int)(fFrac + 0.5);` |
|     21 | 6321 | `			if( sState.uSec >= 1000000 ){` |
|      - | 6322 | `				/* The rounding carried into the second (php's 1.9999999). */` |
|      3 | 6323 | `				sState.uSec -= 1000000;` |
|      3 | 6324 | `				sState.iTs++;` |
|      1 | 6325 | `			}` |
|      - | 6326 | `		}` |
|      - | 6327 | `	}` |
|     49 | 6328 | `	sState.iOff = 0;` |
|     49 | 6329 | `	sState.nName = DtOffName(zNameBuf,sizeof(zNameBuf),0);` |
|     49 | 6330 | `	sState.zName = zNameBuf;` |
|     49 | 6331 | ``	sState.iZoneKind = DT_ZONE_OFFSET;   /* php's fixed `+00:00`, not the UTC id */`` |
|     49 | 6332 | `	pObj = DtNewInstance(pVm,pClass);` |
|     49 | 6333 | `	if( pObj == 0 ){` |
|    ! 0 | 6334 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6335 | `	}` |
|     49 | 6336 | `	DtStore(pVm,pObj,&sState);` |
|     49 | 6337 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     49 | 6338 | `	return PH7_OK;` |
|     31 | 6339 | `}` |
|     56 | 6340 | `static int vm_builtin_DateTime_createFromTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6341 | `{` |
|     57 | 6342 | `	return DtCreateFromTimestamp(pCtx,nArg,apArg,"DateTime");` |
|      1 | 6343 | `}` |
|      4 | 6344 | `static int vm_builtin_DateTimeImmutable_createFromTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6345 | `{` |
|      5 | 6346 | `	return DtCreateFromTimestamp(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 6347 | `}` |
|      8 | 6348 | `static int vm_builtin_DateTime_copyOf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6349 | `{` |
|      9 | 6350 | `	return DtCopyOf(pCtx,nArg,apArg,"DateTime");` |
|      1 | 6351 | `}` |
|     10 | 6352 | `static int vm_builtin_DateTimeImmutable_copyOf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6353 | `{` |
|     11 | 6354 | `	return DtCopyOf(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 6355 | `}` |
|      - | 6356 | `/*` |
|      - | 6357 | ` * int\|false strtotime(string $datetime, ?int $baseTimestamp = null)` |
|      - | 6358 | ` *` |
|      - | 6359 | ` * Rides the same DtParse the constructor uses, so its format coverage is identical.` |
|      - | 6360 | ` * php: the EMPTY string is false, but whitespace-only is 'now'; a parse failure is` |
|      - | 6361 | ` * false (never an exception), and the default timezone is offset 0 — exactly what` |
|      - | 6362 | ` * the constructor does for a null $timezone.` |
|      - | 6363 | ` */` |
|    744 | 6364 | `static int vm_builtin_strtotime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 6365 | `{` |
|      - | 6366 | `	const char *zIn;` |
|      - | 6367 | `	int nIn;` |
|      - | 6368 | `	sxi64 iBase;` |
|    747 | 6369 | `	sxi64 iTs = 0;` |
|    747 | 6370 | `	sxi32 iOff = 0,iZoneOff = 0;` |
|    747 | 6371 | `	int bOffSet = 0,uSec = 0,iTz,bDst,nAbbr;` |
|      - | 6372 | `	const char *zAbbr;` |
|    747 | 6373 | `	if( nArg < 1 ){` |
|    ! 0 | 6374 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6375 | `		return PH7_OK;` |
|      - | 6376 | `	}` |
|    747 | 6377 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    747 | 6378 | `	if( nIn < 1 ){` |
|      3 | 6379 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6380 | `		return PH7_OK;` |
|      - | 6381 | `	}` |
|    745 | 6382 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    622 | 6383 | `		iBase = ph7_value_to_int64(apArg[1]);` |
|    312 | 6384 | `	}else{` |
|    125 | 6385 | `		DtNowUs(pCtx->pVm,&iBase,0);` |
|      - | 6386 | `	}` |
|      - | 6387 | `	/* The default zone is the constructor's rule, and it is the same two-step: a` |
|      - | 6388 | `	 * DATABASE zone has no offset until an instant picks one, so the scan runs` |
|      - | 6389 | `	 * against the offset at the BASE moment -- which is what fills in every` |
|      - | 6390 | `	 * field the string leaves out -- and the wall-clock reading it produces is` |
|      - | 6391 | `	 * then re-solved against the zone. A string that named its own offset keeps` |
|      - | 6392 | `	 * it and skips all of this. */` |
|    745 | 6393 | `	iTz = DtDefaultTzIndex(pCtx->pVm);` |
|    745 | 6394 | `	if( iTz >= 0 ){` |
|     33 | 6395 | `		iZoneOff = DtTzOffsetOf(iTz,0,iBase,&bDst,&zAbbr,&nAbbr);` |
|     16 | 6396 | `	}` |
|    745 | 6397 | `	if( DtParse(zIn,nIn,iBase,iZoneOff,0,&iTs,&iOff,&bOffSet,&uSec) != 0 ){` |
|    122 | 6398 | `		ph7_result_bool(pCtx,0);` |
|    122 | 6399 | `		return PH7_OK;` |
|      - | 6400 | `	}` |
|      - | 6401 | `#ifdef PH7_ENABLE_TZDB` |
|    625 | 6402 | `	if( iTz >= 0 && !bOffSet ){` |
|     17 | 6403 | `		sxi64 iFixed = iTs;` |
|     17 | 6404 | `		sxi32 iOffAt = iZoneOff;` |
|     17 | 6405 | `		if( PH7_TzLocalToUtc(iTz,iTs + iZoneOff,&iFixed,&iOffAt) ){` |
|     17 | 6406 | `			iTs = iFixed;` |
|      8 | 6407 | `		}` |
|      8 | 6408 | `	}` |
|      - | 6409 | `#endif` |
|    625 | 6410 | `	ph7_result_int64(pCtx,iTs);` |
|    625 | 6411 | `	return PH7_OK;` |
|    375 | 6412 | `}` |
|      - | 6413 | `/*` |
|      - | 6414 | ` * ---------------------------------------------------------------------------` |
|      - | 6415 | ` * DateInterval, DatePeriod and its iterator, declared from C.` |
|      - | 6416 | ` *` |
|      - | 6417 | ``  * The rest of the date chunk. DateInterval's two constructors were `preg_match` `` |
|      - | 6418 | `` * calls in PHP; DatePeriod's `getIterator()` was a PHP GENERATOR, which a C body`` |
|      - | 6419 | `` * cannot be -- so it answers a native `InternalIterator`, which is exactly the`` |
|      - | 6420 | ` * class php answers there.` |
|      - | 6421 | ` * ---------------------------------------------------------------------------` |
|      - | 6422 | ` */` |
|      - | 6423 | `/* The MICROSECOND slot of the parsed vector: php keeps it apart from the six` |
|      - | 6424 | ` * relative fields (timelib_rel_time.us), and it does not carry into the seconds` |
|      - | 6425 | ``  * the way the CLOCK's does -- `1000000 microseconds` is an interval whose `%f` `` |
|      - | 6426 | `` * prints 1000000 and whose `s` is 0. */`` |
|      - | 6427 | `#define DT_IV_FIELDS 6` |
|      - | 6428 | `#define DT_IV_USLOT  6` |
|      - | 6429 | `static const char * const azDtIvField[] = { "y", "m", "d", "h", "i", "s" };` |
|      - | 6430 | `/* Read an unsigned run of digits; returns the count consumed. Stops ACCUMULATING` |
|      - | 6431 | ` * past DT_DIGITS_SAFE while still counting, so a caller that is about to refuse` |
|      - | 6432 | ` * an over-wide run does not overflow measuring it (see DT_DIGITS_ISO). */` |
|    400 | 6433 | `static int DtIvDigits(const char *z,const char *zEnd,sxi64 *pVal)` |
|      3 | 6434 | `{` |
|    403 | 6435 | `	int n = 0;` |
|    403 | 6436 | `	sxi64 v = 0;` |
|   1375 | 6437 | `	while( &z[n] < zEnd && SyisDigit(z[n]) ){` |
|    975 | 6438 | `		if( n < DT_DIGITS_SAFE ){` |
|    975 | 6439 | `			v = v*10 + (z[n] - '0');` |
|    486 | 6440 | `		}` |
|    975 | 6441 | `		n++;` |
|      3 | 6442 | `	}` |
|    403 | 6443 | `	*pVal = v;` |
|    403 | 6444 | `	return n;` |
|      3 | 6445 | `}` |
|      - | 6446 | `/*` |
|      - | 6447 | ` * php's ISO-8601 duration grammar: P[nY][nM][nW][nD][T[nH][nM][nS]], every field` |
|      - | 6448 | ` * an unsigned integer. A bare "P", a trailing "T" and a fractional second are all` |
|      - | 6449 | ` * rejected, as php rejects them.` |
|      - | 6450 | ` */` |
|    306 | 6451 | `static int DtIvParseIso(const char *zIn,int nIn,sxi64 *aOut)` |
|      3 | 6452 | `{` |
|    309 | 6453 | `	const char *z = zIn,*zEnd = &zIn[nIn];` |
|    309 | 6454 | `	int bTime = 0,bAny = 0;` |
|      - | 6455 | `	int k;` |
|   2451 | 6456 | `	for( k = 0 ; k <= DT_IV_USLOT ; k++ ){` |
|   2145 | 6457 | `		aOut[k] = 0;` |
|   1074 | 6458 | `	}` |
|    309 | 6459 | `	if( nIn < 2 \|\| zIn[0] != 'P' \|\| zIn[nIn-1] == 'T' ){` |
|     11 | 6460 | `		return -1;` |
|      - | 6461 | `	}` |
|    299 | 6462 | `	z++;` |
|    693 | 6463 | `	while( z < zEnd ){` |
|      - | 6464 | `		sxi64 v;` |
|      - | 6465 | `		int n;` |
|    417 | 6466 | `		if( z[0] == 'T' ){` |
|     77 | 6467 | `			if( bTime ){` |
|    ! 0 | 6468 | `				return -1;` |
|      - | 6469 | `			}` |
|     77 | 6470 | `			bTime = 1;` |
|     77 | 6471 | `			z++;` |
|     77 | 6472 | `			continue;` |
|      - | 6473 | `		}` |
|    343 | 6474 | `		n = DtIvDigits(z,zEnd,&v);` |
|    343 | 6475 | `		if( n == 0 \|\| n > DT_DIGITS_ISO \|\| z + n >= zEnd ){` |
|      - | 6476 | ``			/* php's duration fields stop at twelve digits: `P999999999999D` is an`` |
|      - | 6477 | ``			 * interval there and `P9999999999999D` is "Unknown or bad format". */`` |
|     15 | 6478 | `			return -1;` |
|      - | 6479 | `		}` |
|    329 | 6480 | `		z += n;` |
|    329 | 6481 | `		switch( z[0] ){` |
|     15 | 6482 | `			case 'Y': if( bTime ){ return -1; } aOut[0] += v; break;` |
|      9 | 6483 | `			case 'W': if( bTime ){ return -1; } aOut[2] += v * 7; break;` |
|    160 | 6484 | `			case 'D': if( bTime ){ return -1; } aOut[2] += v; break;` |
|     15 | 6485 | `			case 'H': if( !bTime ){ return -1; } aOut[3] += v; break;` |
|     65 | 6486 | `			case 'S': if( !bTime ){ return -1; } aOut[5] += v; break;` |
|     32 | 6487 | `			case 'M':` |
|      - | 6488 | `				/* The one ambiguous designator: months before T, minutes after. */` |
|     65 | 6489 | `				if( bTime ){ aOut[4] += v; }else{ aOut[1] += v; }` |
|     65 | 6490 | `				break;` |
|      3 | 6491 | `			default:` |
|      7 | 6492 | `				return -1;` |
|      - | 6493 | `		}` |
|    323 | 6494 | `		z++;` |
|    323 | 6495 | `		bAny = 1;` |
|      3 | 6496 | `	}` |
|    279 | 6497 | `	return bAny ? 0 : -1;` |
|    156 | 6498 | `}` |
|      - | 6499 | `/*` |
|      - | 6500 | ` * php's relative-string interval. The string is VALIDATED by the same parser` |
|      - | 6501 | ` * strtotime() uses -- which is where php's "at position N (c): reason" wording` |
|      - | 6502 | ` * comes from -- and the number/unit pairs it understands are then summed. A` |
|      - | 6503 | ` * string the parser accepts but that names no unit ("next monday") is php's` |
|      - | 6504 | ` * all-zero interval, not an error.` |
|      - | 6505 | ` */` |
|    188 | 6506 | `static int DtIvParseRelative(const char *zIn,int nIn,sxi64 *aOut,int *piPos,` |
|      - | 6507 | `	char *pcAt,const char **pzReason)` |
|      2 | 6508 | `{` |
|      - | 6509 | `	dt_parsed sVec;` |
|    190 | 6510 | `	sxi64 iTs = 0;` |
|    190 | 6511 | `	sxi32 iOff = 0;` |
|    190 | 6512 | `	int bOffSet = 0,uSec = 0,iErr;` |
|      - | 6513 | `	int k;` |
|   1506 | 6514 | `	for( k = 0 ; k <= DT_IV_USLOT ; k++ ){` |
|   1318 | 6515 | `		aOut[k] = 0;` |
|    660 | 6516 | `	}` |
|    190 | 6517 | `	if( nIn < 1 ){` |
|      3 | 6518 | `		*piPos = 0;` |
|      3 | 6519 | `		*pcAt = ' ';` |
|      3 | 6520 | `		*pzReason = "Empty string";` |
|      3 | 6521 | `		return -1;` |
|      - | 6522 | `	}` |
|      - | 6523 | `	/* createFromDateString() does NOT publish getLastErrors() in php: the record` |
|      - | 6524 | `	 * keeps whatever the last constructor or modify() left in it. */` |
|    188 | 6525 | `	iErr = DtParseEx(zIn,nIn,0,0,0,0,&iTs,&iOff,&bOffSet,&uSec,&sVec,0);` |
|    188 | 6526 | `	if( iErr != 0 ){` |
|     44 | 6527 | `		*pzReason = DtParseErr(zIn,nIn,iErr,piPos,pcAt);` |
|     44 | 6528 | `		return -1;` |
|      - | 6529 | `	}` |
|      - | 6530 | `	/* php REFUSES a string carrying any absolute element -- a date, a time of day` |
|      - | 6531 | `	 * or a zone -- rather than reading an interval out of what is left: it checks` |
|      - | 6532 | `	 * the same three flags the parse already carries. A relative NAVIGATION word` |
|      - | 6533 | ``	 * is not one of them, which is what makes `tomorrow` the interval d = 1. */`` |
|    146 | 6534 | `	if( sVec.bHaveDate \|\| sVec.nTimeTok \|\| sVec.bOffSet ){` |
|     23 | 6535 | `		return -2;` |
|      - | 6536 | `	}` |
|      - | 6537 | `	/* The vector IS the interval: php reads timelib_rel_time's own fields, so a` |
|      - | 6538 | `	 * week is already days there and the microseconds stand apart from the` |
|      - | 6539 | `	 * seconds. */` |
|    124 | 6540 | `	aOut[0] = sVec.ry;` |
|    124 | 6541 | `	aOut[1] = sVec.rm;` |
|    124 | 6542 | `	aOut[2] = sVec.rd;` |
|    124 | 6543 | `	aOut[3] = sVec.rh;` |
|    124 | 6544 | `	aOut[4] = sVec.ri;` |
|    124 | 6545 | `	aOut[5] = sVec.rs;` |
|    124 | 6546 | `	aOut[DT_IV_USLOT] = sVec.rus;` |
|    124 | 6547 | `	return 0;` |
|     96 | 6548 | `}` |
|      - | 6549 | `/*` |
|      - | 6550 | ` * php's from-string interval is a different OBJECT: it keeps the STRING and` |
|      - | 6551 | `` * presents `from_string` and `date_string` alone, answering the ten fields from`` |
|      - | 6552 | ` * what it parsed whenever a script asks for one. PHL fills the ten as it always` |
|      - | 6553 | ` * did -- every reader, the write filter, format(), add()/sub() and DatePeriod go` |
|      - | 6554 | ` * on reading real slots -- and hides them from the surfaces that SHOW the` |
|      - | 6555 | ` * object, which is the whole of the difference php's shape makes.` |
|      - | 6556 | ` */` |
|    122 | 6557 | `static void DtIvFromString(ph7_vm *pVm,ph7_class_instance *pObj,const char *zIn,int nIn)` |
|      2 | 6558 | `{` |
|      - | 6559 | `	static const char * const azHide[] = {` |
|      - | 6560 | `		"y","m","d","h","i","s","f","invert","days"` |
|      - | 6561 | `	};` |
|      - | 6562 | `	int k;` |
|    124 | 6563 | `	PH7_NativeSetAttrBool(&(*pVm),pObj,"from_string",1);` |
|      - | 6564 | `	/* the ON-DEMAND slot: this write is what puts the name on the object, and it` |
|      - | 6565 | ``	 * lands behind `from_string`, which is php's order */`` |
|    124 | 6566 | `	PH7_NativeSetAttrStr(&(*pVm),pObj,"date_string",zIn,nIn);` |
|   1222 | 6567 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azHide) ; k++ ){` |
|   1100 | 6568 | `		PH7_NativeHideAttr(pObj,azHide[k]);` |
|    551 | 6569 | `	}` |
|    124 | 6570 | `}` |
|      - | 6571 | `/* Write the six relative fields onto a DateInterval instance. */` |
|    398 | 6572 | `static void DtIvStore(ph7_vm *pVm,ph7_class_instance *pObj,const sxi64 *aVal)` |
|      3 | 6573 | `{` |
|      - | 6574 | `	int k;` |
|   2789 | 6575 | `	for( k = 0 ; k < DT_IV_FIELDS ; k++ ){` |
|   2391 | 6576 | `		PH7_NativeSetAttrInt(pVm,pObj,azDtIvField[k],aVal[k]);` |
|   1197 | 6577 | `	}` |
|      - | 6578 | ``	/* ...and the microseconds through the pair that owns them, so `f` and the`` |
|      - | 6579 | `	 * hidden count stay one value. */` |
|    401 | 6580 | `	DtIvSetUsec(pVm,pObj,aVal[DT_IV_USLOT]);` |
|    401 | 6581 | `}` |
|      - | 6582 | `/*` |
|      - | 6583 | ` * php's date_interval_write_property: what a write to one of DateInterval's` |
|      - | 6584 | ` * properties CONVERTS to, since every one of them is a field of php's own C` |
|      - | 6585 | ` * struct rather than a slot a script's value lands in.` |
|      - | 6586 | ` *` |
|      - | 6587 | `` * The six relative fields and `invert` take php's int cast — a float truncates`` |
|      - | 6588 | ` * and warns where it wraps, a string reads its numeric prefix, an array is 1 —` |
|      - | 6589 | `` * with `invert` narrowed to the 32-bit `int` timelib declares it as (so`` |
|      - | 6590 | `` * `$i->invert = 3000000000` is -1294967296 in both engines). `f` is the`` |
|      - | 6591 | ` * microsecond count above, so its cast warning is raised HERE, on the SCALED` |
|      - | 6592 | ` * value, which is where php raises it.` |
|      - | 6593 | ` *` |
|      - | 6594 | `` * `days` and `from_string` are answered by php's read handler and refused by its`` |
|      - | 6595 | ` * write one: a script that assigns them creates a deprecated DYNAMIC property` |
|      - | 6596 | `` * that never reaches the interval (`$i->days = 5` leaves `$i->days` false`` |
|      - | 6597 | ` * there). The scope policy refuses a deprecation, and PHL refuses a dynamic property outright,` |
|      - | 6598 | `` * so the two meet at the Error PHL already raises for `$i->anythingElse = v`.`` |
|      - | 6599 | ` */` |
|    616 | 6600 | `static void DtIntervalSet(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)` |
|      3 | 6601 | `{` |
|    619 | 6602 | `	const char *zName = SyStringData(pCtx->pName);` |
|    619 | 6603 | `	sxu32 nName = SyStringLength(pCtx->pName);` |
|    619 | 6604 | `	ph7_value *pVal = pCtx->pValue;` |
|      - | 6605 | `	int bInvert;` |
|    619 | 6606 | `	if( nName == sizeof("days")-1 && SyMemcmp(zName,"days",nName) == 0 ){` |
|      3 | 6607 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 6608 | `			"Cannot create dynamic property DateInterval::$days");` |
|      3 | 6609 | `		pCtx->zThrowClass = "Error";` |
|      3 | 6610 | `		return;` |
|      - | 6611 | `	}` |
|    617 | 6612 | `	if( nName == sizeof("from_string")-1 && SyMemcmp(zName,"from_string",nName) == 0 ){` |
|      3 | 6613 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 6614 | `			"Cannot create dynamic property DateInterval::$from_string");` |
|      3 | 6615 | `		pCtx->zThrowClass = "Error";` |
|      3 | 6616 | `		return;` |
|      - | 6617 | `	}` |
|    615 | 6618 | `	if( nName == sizeof("date_string")-1 && SyMemcmp(zName,"date_string",nName) == 0 ){` |
|    ! 0 | 6619 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 6620 | `			"Cannot create dynamic property DateInterval::$date_string");` |
|    ! 0 | 6621 | `		pCtx->zThrowClass = "Error";` |
|    ! 0 | 6622 | `		return;` |
|      - | 6623 | `	}` |
|    615 | 6624 | `	if( nName == sizeof("f")-1 && zName[0] == 'f' ){` |
|    492 | 6625 | `		double r = (double)PH7_ValuePeekReal(pVal);` |
|      - | 6626 | `		sxi64 us;` |
|    492 | 6627 | `		PH7_RealWarnIntCast(pVm,r * 1000000.0);` |
|    492 | 6628 | `		us = DtIvUsecOfReal(r);` |
|    492 | 6629 | `		PH7_NativeSetAttrInt(pVm,pThis,DT_IV_US,us);` |
|    492 | 6630 | `		PH7_MemObjRelease(pVal);` |
|    492 | 6631 | `		PH7_MemObjInitFromReal(pVm,pVal,(ph7_real)((double)us / 1000000.0));` |
|    492 | 6632 | `		return;` |
|      - | 6633 | `	}` |
|    124 | 6634 | `	bInvert = nName == sizeof("invert")-1 && SyMemcmp(zName,"invert",nName) == 0;` |
|    124 | 6635 | `	if( !bInvert ){` |
|      - | 6636 | `		int k;` |
|    188 | 6637 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(azDtIvField) ; k++ ){` |
|    188 | 6638 | `			if( nName == 1 && zName[0] == azDtIvField[k][0] ){` |
|     84 | 6639 | `				break;` |
|      - | 6640 | `			}` |
|     53 | 6641 | `		}` |
|     84 | 6642 | `		if( k >= (int)SX_ARRAYSIZE(azDtIvField) ){` |
|    ! 0 | 6643 | `			return;   /* the hidden count slot: written from C, never through here */` |
|      - | 6644 | `		}` |
|     41 | 6645 | `	}` |
|      - | 6646 | `	{` |
|      - | 6647 | `		/* y/m/d/h/i/s and invert, all of them php's int cast. */` |
|      - | 6648 | `		sxi64 iVal;` |
|    124 | 6649 | `		PH7_MemObjWarnIntCast(pVal);` |
|    124 | 6650 | `		iVal = PH7_ValuePeekInt64(pVal);` |
|    124 | 6651 | `		if( bInvert ){` |
|     41 | 6652 | `			iVal = (sxi64)(sxi32)iVal;` |
|     20 | 6653 | `		}` |
|    124 | 6654 | `		PH7_MemObjRelease(pVal);` |
|    124 | 6655 | `		PH7_MemObjInitFromInt(pVm,pVal,iVal);` |
|      - | 6656 | `	}` |
|    311 | 6657 | `}` |
|      - | 6658 | `/* DateInterval::__construct(string $duration) */` |
|    276 | 6659 | `static int vm_builtin_DateInterval_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 6660 | `{` |
|    279 | 6661 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */` |
|      - | 6662 | `	const char *zDur;` |
|      - | 6663 | `	int nDur;` |
|      - | 6664 | `	sxi64 aVal[DT_IV_USLOT + 1];` |
|    279 | 6665 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 6666 | `		return PH7_OK;` |
|      - | 6667 | `	}` |
|    279 | 6668 | `	zDur = ph7_value_to_string(apArg[0],&nDur);` |
|    279 | 6669 | `	if( DtIvParseIso(zDur,nDur,aVal) != 0 ){` |
|     46 | 6670 | `		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",` |
|     15 | 6671 | `			"Unknown or bad format (%.*s)",DtCStrLen(zDur,nDur),zDur);` |
|      - | 6672 | `	}` |
|    249 | 6673 | `	DtIvStore(pCtx->pVm,pThis,aVal);` |
|    249 | 6674 | `	DtSetInit(pCtx->pVm,pThis);` |
|    249 | 6675 | `	return PH7_OK;` |
|    141 | 6676 | `}` |
|      - | 6677 | `/*` |
|      - | 6678 | ` * DateInterval::createFromDateString(string $datetime). Shared with the` |
|      - | 6679 | ` * date_interval_create_from_date_string() alias, which WARNS and answers false` |
|      - | 6680 | ` * where the method throws.` |
|      - | 6681 | ` */` |
|    186 | 6682 | `static ph7_class_instance * DtIvFromDateString(ph7_context *pCtx,const char *zIn,int nIn,` |
|      - | 6683 | `	int *piPos,char *pcAt,const char **pzReason,int *pbNonRel)` |
|      2 | 6684 | `{` |
|    188 | 6685 | `	ph7_vm *pVm = pCtx->pVm;` |
|    188 | 6686 | `	ph7_class *pClass = DtFactoryClass(pCtx,"DateInterval");` |
|      - | 6687 | `	ph7_class_instance *pObj;` |
|      - | 6688 | `	sxi64 aVal[DT_IV_USLOT + 1];` |
|      - | 6689 | `	int rc;` |
|    188 | 6690 | `	*pbNonRel = 0;` |
|    188 | 6691 | `	if( pClass == 0 ){` |
|    ! 0 | 6692 | `		return 0;` |
|      - | 6693 | `	}` |
|    188 | 6694 | `	if( (rc = DtIvParseRelative(zIn,nIn,aVal,piPos,pcAt,pzReason)) != 0 ){` |
|     68 | 6695 | `		*pbNonRel = (rc == -2);` |
|     68 | 6696 | `		return 0;` |
|      - | 6697 | `	}` |
|    122 | 6698 | `	pObj = DtNewInstance(pVm,pClass);` |
|    122 | 6699 | `	if( pObj == 0 ){` |
|    ! 0 | 6700 | `		return 0;` |
|      - | 6701 | `	}` |
|    122 | 6702 | `	DtIvStore(pVm,pObj,aVal);` |
|    122 | 6703 | `	DtIvFromString(pVm,pObj,zIn,nIn);` |
|    122 | 6704 | `	return pObj;` |
|     95 | 6705 | `}` |
|    176 | 6706 | `static int vm_builtin_DateInterval_createFromDateString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6707 | `{` |
|    178 | 6708 | `	const char *zIn,*zReason = "";` |
|    178 | 6709 | `	int nIn,iPos = 0,bNonRel = 0;` |
|    178 | 6710 | `	char cAt = ' ';` |
|      - | 6711 | `	ph7_class_instance *pObj;` |
|    178 | 6712 | `	if( nArg < 1 ){` |
|    ! 0 | 6713 | `		return PH7_OK;` |
|      - | 6714 | `	}` |
|    178 | 6715 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    178 | 6716 | `	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason,&bNonRel);` |
|    178 | 6717 | `	if( pObj == 0 ){` |
|     66 | 6718 | `		if( bNonRel ){` |
|     31 | 6719 | `			return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",` |
|     10 | 6720 | `				"String '%.*s' contains non-relative elements",DtCStrLen(zIn,nIn),zIn);` |
|      - | 6721 | `		}` |
|     68 | 6722 | `		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",` |
|      - | 6723 | `			"Unknown or bad format (%.*s) at position %d (%c): %s",` |
|     22 | 6724 | `			DtCStrLen(zIn,nIn),zIn,iPos,cAt,zReason);` |
|      - | 6725 | `	}` |
|    114 | 6726 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    114 | 6727 | `	return PH7_OK;` |
|     90 | 6728 | `}` |
|      - | 6729 | `/*` |
|      - | 6730 | ` * DateInterval::format(string $format) -- php's own %-token loop, including the` |
|      - | 6731 | `` * rule the chunk got wrong: an UNKNOWN token keeps its '%' (`%q` is "%q").`` |
|      - | 6732 | ` */` |
|    666 | 6733 | `static void DtIvFormat(ph7_context *pCtx,ph7_class_instance *pObj,const char *zFmt,int nFmt)` |
|      2 | 6734 | `{` |
|      - | 6735 | `	SyBlob sOut;` |
|      - | 6736 | `	ph7_value *pDays;` |
|      - | 6737 | `	int k;` |
|    668 | 6738 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   2518 | 6739 | `	for( k = 0 ; k < nFmt ; k++ ){` |
|   1854 | 6740 | `		char c = zFmt[k];` |
|      - | 6741 | `		char t;` |
|   1854 | 6742 | `		if( c != '%' ){` |
|    589 | 6743 | `			SyBlobAppend(&sOut,&c,1);` |
|    589 | 6744 | `			continue;` |
|      - | 6745 | `		}` |
|   1266 | 6746 | `		k++;` |
|   1266 | 6747 | `		if( k >= nFmt ){` |
|      - | 6748 | `			/* php drops a trailing lone '%' rather than echoing it. */` |
|      3 | 6749 | `			break;` |
|      - | 6750 | `		}` |
|   1264 | 6751 | `		t = zFmt[k];` |
|   1264 | 6752 | `		switch( t ){` |
|      - | 6753 | ``			/* php prints five of the six through an `(int)` — a 32-bit NARROWING`` |
|      - | 6754 | `			 * of a property it stores as an int64 and hands back whole, so` |
|      - | 6755 | ``			 * `$i->y = 7960523868075137518` reads back in full and prints`` |
|      - | 6756 | `			 * 111352302. The SECONDS are the exception: php formats those with` |
|      - | 6757 | `			 * its long specifier, in both cases. */` |
|      7 | 6758 | `			case 'Y': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"y")); break;` |
|     65 | 6759 | `			case 'y': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"y")); break;` |
|      7 | 6760 | `			case 'M': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"m")); break;` |
|     59 | 6761 | `			case 'm': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"m")); break;` |
|      7 | 6762 | `			case 'D': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"d")); break;` |
|     79 | 6763 | `			case 'd': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"d")); break;` |
|      7 | 6764 | `			case 'H': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"h")); break;` |
|     87 | 6765 | `			case 'h': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"h")); break;` |
|      7 | 6766 | `			case 'I': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"i")); break;` |
|     85 | 6767 | `			case 'i': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"i")); break;` |
|      7 | 6768 | `			case 'S': SyBlobFormat(&sOut,"%02qd",PH7_NativeAttrInt(pObj,"s")); break;` |
|     87 | 6769 | `			case 's': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"s")); break;` |
|    309 | 6770 | `			case 'F': case 'f': {` |
|      - | 6771 | `				/* php prints the STORED microsecond count, which is why` |
|      - | 6772 | ``				 * `$i->f = 0.1234567` prints 123456 rather than the 123457 a`` |
|      - | 6773 | `				 * rounding of the float would give, and why a count no double` |
|      - | 6774 | `				 * holds exactly still prints its own digits. The conversion the` |
|      - | 6775 | `				 * cast contract lives in — truncate toward zero, wrap what no` |
|      - | 6776 | `				 * int64 holds, 0 for a NaN or an infinity, and php's warning` |
|      - | 6777 | `				 * beside it — happens at the property WRITE, where php does it. */` |
|    620 | 6778 | `				sxi64 uS = DtIvUsec(pObj);` |
|    620 | 6779 | `				if( t == 'F' ){` |
|     85 | 6780 | `					SyBlobFormat(&sOut,"%06qd",uS);` |
|     43 | 6781 | `				}else{` |
|    536 | 6782 | `					SyBlobFormat(&sOut,"%qd",uS);` |
|      - | 6783 | `				}` |
|    620 | 6784 | `				break;` |
|      - | 6785 | `			}` |
|     89 | 6786 | `			case 'R': SyBlobAppend(&sOut,PH7_NativeAttrInt(pObj,"invert") ? "-" : "+",1); break;` |
|      5 | 6787 | `			case 'r': if( PH7_NativeAttrInt(pObj,"invert") ){ SyBlobAppend(&sOut,"-",1); } break;` |
|     26 | 6788 | `			case 'a':` |
|     53 | 6789 | `				pDays = PH7_NativeAttr(pObj,"days");` |
|     53 | 6790 | `				if( pDays && (pDays->iFlags & MEMOBJ_INT) ){` |
|     45 | 6791 | `					SyBlobFormat(&sOut,"%qd",pDays->x.iVal);` |
|     23 | 6792 | `				}else{` |
|      9 | 6793 | `					SyBlobAppend(&sOut,"(unknown)",sizeof("(unknown)")-1);` |
|      - | 6794 | `				}` |
|     53 | 6795 | `				break;` |
|      7 | 6796 | `			case '%': SyBlobAppend(&sOut,"%",1); break;` |
|      1 | 6797 | `			default:` |
|      - | 6798 | `				/* php keeps BOTH bytes of an unrecognised token. */` |
|      3 | 6799 | `				SyBlobAppend(&sOut,"%",1);` |
|      3 | 6800 | `				SyBlobAppend(&sOut,&t,1);` |
|      2 | 6801 | `				break;` |
|      - | 6802 | `		}` |
|    633 | 6803 | `	}` |
|    668 | 6804 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    668 | 6805 | `	SyBlobRelease(&sOut);` |
|    668 | 6806 | `}` |
|    666 | 6807 | `static int vm_builtin_DateInterval_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6808 | `{` |
|    668 | 6809 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 6810 | `	const char *zFmt;` |
|      - | 6811 | `	int nFmt;` |
|    668 | 6812 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|      5 | 6813 | `		return PH7_OK;` |
|      - | 6814 | `	}` |
|    664 | 6815 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    664 | 6816 | `	DtIvFormat(pCtx,pThis,zFmt,nFmt);` |
|    664 | 6817 | `	return PH7_OK;` |
|    335 | 6818 | `}` |
|      - | 6819 | `/* Is this value an instance of the named class? */` |
|    412 | 6820 | `static int DtValueIsA(ph7_vm *pVm,ph7_value *pVal,const char *zClass)` |
|      1 | 6821 | `{` |
|      - | 6822 | `	ph7_class *pClass;` |
|    413 | 6823 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      7 | 6824 | `		return 0;` |
|      - | 6825 | `	}` |
|    407 | 6826 | `	pClass = DtClass(&(*pVm),zClass);` |
|    407 | 6827 | `	return pClass != 0` |
|    406 | 6828 | `		&& PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass);` |
|    207 | 6829 | `}` |
|      - | 6830 | `/*` |
|      - | 6831 | ` * Write an object (or null) into a declared property of another object.` |
|      - | 6832 | ` *` |
|      - | 6833 | ` * The scratch value ALIASES the instance rather than owning it, and` |
|      - | 6834 | ` * PH7_MemObjStore takes the reference the slot keeps -- so releasing the scratch` |
|      - | 6835 | ` * afterwards would hand back the slot's own reference and free the object out from` |
|      - | 6836 | `` * under it (which is what it did: `foreach` over a DatePeriod crashed on the second`` |
|      - | 6837 | `` * element's `->format()`). The caller keeps owning whatever it passed in.`` |
|      - | 6838 | ` */` |
|      - | 6839 | `/*` |
|      - | 6840 | ` * DatePeriod::__construct($start, $interval, $end, $options)` |
|      - | 6841 | ` *` |
|      - | 6842 | ` * php overloads it three ways and rejects everything else with ONE message, which` |
|      - | 6843 | ` * is why the signature stays unenforced and the shapes are checked here.` |
|      - | 6844 | ` */` |
|      - | 6845 | `/* php's ceiling for a recurrence count, and it is checked TWICE: once on the` |
|      - | 6846 | ` * number the caller wrote, and once on that number plus the dates the OPTIONS` |
|      - | 6847 | ` * add -- two different exception classes and two different sentences. */` |
|      - | 6848 | `#define DP_REC_LIMIT 2147483640` |
|    226 | 6849 | `static int DpConstructInto(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,ph7_value **apArg,` |
|      - | 6850 | `	const char *zIsoStartClass,const char *zCallee)` |
|      1 | 6851 | `{` |
|    227 | 6852 | `	ph7_vm *pVm = pCtx->pVm;` |
|    227 | 6853 | `	sxi64 iOptions = 0;` |
|      - | 6854 | `	static const char *zBadArgs =` |
|      - | 6855 | `		"DatePeriod::__construct() accepts (DateTimeInterface, DateInterval, int [, int]), "` |
|      - | 6856 | `		"or (DateTimeInterface, DateInterval, DateTime [, int]), or (string [, int]) as arguments";` |
|    227 | 6857 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 6858 | `		return PH7_VmThrowException(pCtx,"TypeError","%s",zBadArgs);` |
|      - | 6859 | `	}` |
|    227 | 6860 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|      - | 6861 | `		/* The ISO-8601 form: "R<n>/<start>/<duration>". php's second argument is` |
|      - | 6862 | `		 * then the OPTIONS bitmask, not an interval. */` |
|     49 | 6863 | `		const char *zSpec = (const char *)SyBlobData(&apArg[0]->sBlob);` |
|     49 | 6864 | `		int nSpec = (int)SyBlobLength(&apArg[0]->sBlob);` |
|      - | 6865 | `		const char *zStart,*zDur;` |
|      - | 6866 | `		int nStart,nDur,k;` |
|     49 | 6867 | `		sxi64 nRec = 0;` |
|      - | 6868 | `		sxi64 aIv[DT_IV_USLOT + 1];` |
|      - | 6869 | `		dt_state sState;` |
|      - | 6870 | `		char zNameBuf[16];` |
|      - | 6871 | `		const char *zErr;` |
|      - | 6872 | `		int iPos,nDigits;` |
|      - | 6873 | `		char cAt;` |
|      - | 6874 | `		ph7_class_instance *pStart,*pIv;` |
|     49 | 6875 | `		if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_INT) ){` |
|    ! 0 | 6876 | `			iOptions = apArg[1]->x.iVal;` |
|    ! 0 | 6877 | `		}` |
|     49 | 6878 | `		if( nSpec < 2 \|\| zSpec[0] != 'R' ){` |
|    ! 0 | 6879 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 6880 | `				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);` |
|      - | 6881 | `		}` |
|     49 | 6882 | `		nDigits = DtIvDigits(&zSpec[1],&zSpec[nSpec],&nRec);` |
|     49 | 6883 | `		k = 1 + nDigits;` |
|     49 | 6884 | `		if( nDigits == 0 \|\| k >= nSpec \|\| zSpec[k] != '/' ){` |
|    ! 0 | 6885 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 6886 | `				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);` |
|      - | 6887 | `		}` |
|     49 | 6888 | `		if( nDigits > 9 ){` |
|      - | 6889 | `			/* php's ISO scanner reads at most NINE digits of the count and drops` |
|      - | 6890 | ``			 * the rest on the floor -- `R2147483639/...` is 214748363 recurrences`` |
|      - | 6891 | ``			 * there, and `R99999999999999999999/...` is 999999999. Reading them`` |
|      - | 6892 | `			 * all was a silent wrong answer here. */` |
|     13 | 6893 | `			nRec = 0;` |
|     13 | 6894 | `			DtIvDigits(&zSpec[1],&zSpec[1 + 9],&nRec);` |
|      6 | 6895 | `		}` |
|     49 | 6896 | `		if( nRec < 1 ){` |
|      - | 6897 | ``			/* `R0` is php's refusal, and it is worded as a MISSING count rather`` |
|      - | 6898 | `			 * than an out-of-range one. */` |
|     25 | 6899 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|      - | 6900 | `				"%s(): ISO interval must contain an end date or a recurrence count, "` |
|      8 | 6901 | `				"\"%.*s\" given",zCallee,nSpec,zSpec);` |
|      - | 6902 | `		}` |
|     33 | 6903 | `		zStart = &zSpec[k+1];` |
|     33 | 6904 | `		nStart = 0;` |
|    641 | 6905 | `		while( &zStart[nStart] < &zSpec[nSpec] && zStart[nStart] != '/' ){` |
|    609 | 6906 | `			nStart++;` |
|      1 | 6907 | `		}` |
|     33 | 6908 | `		if( &zStart[nStart] >= &zSpec[nSpec] ){` |
|    ! 0 | 6909 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 6910 | `				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);` |
|      - | 6911 | `		}` |
|     33 | 6912 | `		zDur = &zStart[nStart+1];` |
|     33 | 6913 | `		nDur = (int)(&zSpec[nSpec] - zDur);` |
|     48 | 6914 | `		if( DtInitState(pCtx,zStart,nStart,0,pVm->zDefTz,(int)pVm->nDefTz,` |
|      - | 6915 | `			DT_ZONE_DEFAULT_KIND,&sState,` |
|     32 | 6916 | `			zNameBuf,sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0` |
|     32 | 6917 | `		 \|\| DtIvParseIso(zDur,nDur,aIv) != 0 ){` |
|      4 | 6918 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|      1 | 6919 | `				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);` |
|      - | 6920 | `		}` |
|      - | 6921 | `		/* php's two ISO entry points disagree on the class they build, and both` |
|      - | 6922 | ``		 * answers are load-bearing: `new DatePeriod("R2/...")` yields DateTime`` |
|      - | 6923 | `		 * where DatePeriod::createFromISO8601String() yields DateTimeImmutable. */` |
|     31 | 6924 | `		pStart = DtNewInstance(pVm,DtClass(pVm,zIsoStartClass));` |
|     31 | 6925 | `		pIv = DtNewInstance(pVm,DtClass(pVm,"DateInterval"));` |
|     31 | 6926 | `		if( pStart == 0 \|\| pIv == 0 ){` |
|    ! 0 | 6927 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6928 | `		}` |
|     31 | 6929 | `		DtStore(pVm,pStart,&sState);` |
|     31 | 6930 | `		DtIvStore(pVm,pIv,aIv);` |
|     31 | 6931 | `		PH7_NativeSetAttrObj(pVm,pThis,"start",pStart);` |
|     31 | 6932 | `		PH7_NativeSetAttrObj(pVm,pThis,"interval",pIv);` |
|     31 | 6933 | `		PH7_ClassInstanceUnref(pStart);` |
|     31 | 6934 | `		PH7_ClassInstanceUnref(pIv);` |
|     31 | 6935 | `		PH7_NativeSetAttrInt(pVm,pThis,"recurrences",nRec + 1);` |
|     16 | 6936 | `	}else{` |
|      - | 6937 | `		ph7_class_instance *pStart,*pIv,*pEnd;` |
|    178 | 6938 | `		if( !DtValueIsA(pVm,apArg[0],"DateTimeInterface")` |
|    178 | 6939 | `		 \|\| nArg < 3` |
|    177 | 6940 | `		 \|\| !DtValueIsA(pVm,apArg[1],"DateInterval")` |
|    176 | 6941 | `		 \|\| ((apArg[2]->iFlags & MEMOBJ_INT) == 0` |
|     97 | 6942 | `		     && !DtValueIsA(pVm,apArg[2],"DateTimeInterface")) ){` |
|      5 | 6943 | `			return PH7_VmThrowException(pCtx,"TypeError","%s",zBadArgs);` |
|      - | 6944 | `		}` |
|      - | 6945 | `		/* An unconstructed date on either end. php's sentence here names the` |
|      - | 6946 | `		 * INTERFACE its argument is declared as and not the object's own class --` |
|      - | 6947 | `		 * a subclass of DateTime is still reported as "DateTimeInterface" -- so` |
|      - | 6948 | `		 * this one door words the refusal itself. (php reaches the INTERVAL` |
|      - | 6949 | `		 * argument's state without a screen at all and segfaults on an` |
|      - | 6950 | `		 * unconstructed one; PHL refuses it the way every other door does, which` |
|      - | 6951 | `		 * is the scope policy.) */` |
|    174 | 6952 | `		if( DtArgInitNamed(pCtx,apArg[0],"DateTimeInterface") != 0` |
|    173 | 6953 | `		 \|\| DtArgInit(pCtx,apArg[1]) != 0` |
|    173 | 6954 | `		 \|\| DtArgInitNamed(pCtx,apArg[2],"DateTimeInterface") != 0 ){` |
|      5 | 6955 | `			return PH7_OK;` |
|      - | 6956 | `		}` |
|    171 | 6957 | `		if( nArg > 3 ){` |
|     99 | 6958 | `			iOptions = ph7_value_to_int64(apArg[3]);` |
|     49 | 6959 | `		}` |
|    171 | 6960 | `		if( apArg[2]->iFlags & MEMOBJ_INT ){` |
|      - | 6961 | `			/* php's two range checks on a recurrence COUNT, in its order and with` |
|      - | 6962 | `			 * its two exception classes: the bare number first, then the number` |
|      - | 6963 | `			 * plus the dates the options ask for (the start date unless` |
|      - | 6964 | `			 * EXCLUDE_START_DATE, and the end date if INCLUDE_END_DATE) -- which` |
|      - | 6965 | ``			 * is why `2147483639` alone is refused while the same count with`` |
|      - | 6966 | `			 * EXCLUDE_START_DATE is built. PHL accepted every one of them,` |
|      - | 6967 | ``			 * `0` and `-1` included, and iterated a period php refuses to make. */`` |
|    153 | 6968 | `			sxi64 iRec = apArg[2]->x.iVal;` |
|      - | 6969 | `			sxi64 iWithOpt;` |
|    153 | 6970 | `			if( iRec < 1 \|\| iRec >= DP_REC_LIMIT ){` |
|     61 | 6971 | `				return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|      - | 6972 | `					"%s(): Recurrence count must be greater or equal to 1 and lower than %d",` |
|     20 | 6973 | `					zCallee,DP_REC_LIMIT);` |
|      - | 6974 | `			}` |
|      - | 6975 | `			/* Only now: the sum is computed on a count already known to be under` |
|      - | 6976 | `			 * the ceiling, so the two dates the options may add cannot overflow it` |
|      - | 6977 | `			 * (PHP_INT_MAX + 1 did, and UBSan said so). */` |
|    169 | 6978 | `			iWithOpt = iRec + ((iOptions & 1) == 0 ? 1 : 0)` |
|    112 | 6979 | `			         + ((iOptions & 2) != 0 ? 1 : 0);` |
|    113 | 6980 | `			if( iWithOpt >= DP_REC_LIMIT ){` |
|     13 | 6981 | `				return PH7_VmThrowException(pCtx,"DateMalformedStringException",` |
|      - | 6982 | `					"%s(): Recurrence count must be greater or equal to 1 and lower than %d "` |
|      4 | 6983 | `					"(including options)",zCallee,DP_REC_LIMIT);` |
|      - | 6984 | `			}` |
|     52 | 6985 | `		}` |
|    123 | 6986 | `		pStart = PH7_CloneClassInstance((ph7_class_instance *)apArg[0]->x.pOther);` |
|    123 | 6987 | `		pIv = (ph7_class_instance *)apArg[1]->x.pOther;` |
|    123 | 6988 | `		if( pStart == 0 ){` |
|    ! 0 | 6989 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6990 | `		}` |
|    123 | 6991 | `		PH7_NativeSetAttrObj(pVm,pThis,"start",pStart);` |
|    123 | 6992 | `		PH7_ClassInstanceUnref(pStart);` |
|    123 | 6993 | `		PH7_NativeSetAttrObj(pVm,pThis,"interval",pIv);` |
|    123 | 6994 | `		if( apArg[2]->iFlags & MEMOBJ_INT ){` |
|    105 | 6995 | `			PH7_NativeSetAttrInt(pVm,pThis,"recurrences",apArg[2]->x.iVal + 1);` |
|     53 | 6996 | `		}else{` |
|     19 | 6997 | `			pEnd = PH7_CloneClassInstance((ph7_class_instance *)apArg[2]->x.pOther);` |
|     19 | 6998 | `			if( pEnd == 0 ){` |
|    ! 0 | 6999 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7000 | `			}` |
|     19 | 7001 | `			PH7_NativeSetAttrObj(pVm,pThis,"end",pEnd);` |
|     19 | 7002 | `			PH7_ClassInstanceUnref(pEnd);` |
|      - | 7003 | `			/* php's own answer for a period bounded by a DATE rather than a count,` |
|      - | 7004 | `			 * and it has to be written rather than defaulted now that an` |
|      - | 7005 | `			 * unconstructed period reads 0. */` |
|     19 | 7006 | `			PH7_NativeSetAttrInt(pVm,pThis,"recurrences",1);` |
|      - | 7007 | `		}` |
|      - | 7008 | `	}` |
|    153 | 7009 | `	PH7_NativeSetAttrBool(pVm,pThis,"include_start_date",(iOptions & 1) == 0);` |
|    153 | 7010 | `	PH7_NativeSetAttrBool(pVm,pThis,"include_end_date",(iOptions & 2) != 0);` |
|    153 | 7011 | `	DtSetInit(pVm,pThis);` |
|    153 | 7012 | `	return PH7_OK;` |
|    114 | 7013 | `}` |
|    202 | 7014 | `static int vm_builtin_DatePeriod_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7015 | `{` |
|    203 | 7016 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */` |
|    203 | 7017 | `	if( pThis == 0 ){` |
|    ! 0 | 7018 | `		return PH7_OK;` |
|      - | 7019 | `	}` |
|    203 | 7020 | `	return DpConstructInto(pCtx,pThis,nArg,apArg,"DateTime","DatePeriod::__construct");` |
|    102 | 7021 | `}` |
|      - | 7022 | `/* DatePeriod::createFromISO8601String(string $specification, int $options = 0) */` |
|     24 | 7023 | `static int vm_builtin_DatePeriod_createFromISO8601String(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7024 | `{` |
|     25 | 7025 | `	ph7_vm *pVm = pCtx->pVm;` |
|     25 | 7026 | `	ph7_class *pClass = DtFactoryClass(pCtx,"DatePeriod");` |
|      - | 7027 | `	ph7_class_instance *pObj;` |
|      - | 7028 | `	sxi32 rc;` |
|     25 | 7029 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 7030 | `		return PH7_OK;` |
|      - | 7031 | `	}` |
|      - | 7032 | `	/* Not DtNewInstance(): this object is INITIALIZED by the shared constructor` |
|      - | 7033 | `	 * body below, and only if that succeeds. */` |
|     25 | 7034 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     25 | 7035 | `	if( pObj == 0 ){` |
|    ! 0 | 7036 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7037 | `	}` |
|      - | 7038 | `	/* php's factory IS the constructor, with the same overloaded argument shape. */` |
|     25 | 7039 | `	rc = DpConstructInto(pCtx,pObj,nArg,apArg,"DateTimeImmutable",` |
|      - | 7040 | `		"DatePeriod::createFromISO8601String");` |
|     25 | 7041 | `	if( rc != PH7_OK \|\| pCtx->nThrowRc != 0 ){` |
|      9 | 7042 | `		PH7_ClassInstanceUnref(pObj);` |
|      9 | 7043 | `		return rc;` |
|      - | 7044 | `	}` |
|     17 | 7045 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     17 | 7046 | `	return PH7_OK;` |
|     13 | 7047 | `}` |
|     34 | 7048 | `static int vm_builtin_DatePeriod_getStartDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7049 | `{` |
|     35 | 7050 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|     17 | 7051 | `	SXUNUSED(nArg);` |
|     17 | 7052 | `	SXUNUSED(apArg);` |
|     35 | 7053 | `	if( pThis ){` |
|     29 | 7054 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"start");` |
|     29 | 7055 | `		if( pVal ){` |
|     29 | 7056 | `			ph7_result_value(pCtx,pVal);` |
|     14 | 7057 | `		}` |
|     14 | 7058 | `	}` |
|     35 | 7059 | `	return PH7_OK;` |
|      1 | 7060 | `}` |
|      - | 7061 | `/* php's two NULLABLE getters read the struct without screening it, so an` |
|      - | 7062 | ` * unconstructed period answers null from both where every other door raises. */` |
|      8 | 7063 | `static int vm_builtin_DatePeriod_getEndDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7064 | `{` |
|      9 | 7065 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|      4 | 7066 | `	SXUNUSED(nArg);` |
|      4 | 7067 | `	SXUNUSED(apArg);` |
|      9 | 7068 | `	if( pThis ){` |
|      9 | 7069 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"end");` |
|      9 | 7070 | `		if( pVal ){` |
|      5 | 7071 | `			ph7_result_value(pCtx,pVal);` |
|      2 | 7072 | `		}` |
|      4 | 7073 | `	}` |
|      9 | 7074 | `	return PH7_OK;` |
|      1 | 7075 | `}` |
|      4 | 7076 | `static int vm_builtin_DatePeriod_getDateInterval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7077 | `{` |
|      5 | 7078 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      2 | 7079 | `	SXUNUSED(nArg);` |
|      2 | 7080 | `	SXUNUSED(apArg);` |
|      5 | 7081 | `	if( pThis ){` |
|      3 | 7082 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"interval");` |
|      3 | 7083 | `		if( pVal ){` |
|      3 | 7084 | `			ph7_result_value(pCtx,pVal);` |
|      1 | 7085 | `		}` |
|      1 | 7086 | `	}` |
|      5 | 7087 | `	return PH7_OK;` |
|      1 | 7088 | `}` |
|      - | 7089 | `/*` |
|      - | 7090 | ` * DatePeriod::getRecurrences() -- php answers NULL for a period bounded by an END` |
|      - | 7091 | `` * DATE and the recurrence COUNT otherwise, which is `recurrences - 1` (php stores`` |
|      - | 7092 | ` * the count of dates, one more than the recurrences). No private slot needed.` |
|      - | 7093 | ` */` |
|     50 | 7094 | `static int vm_builtin_DatePeriod_getRecurrences(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7095 | `{` |
|     51 | 7096 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|     25 | 7097 | `	SXUNUSED(nArg);` |
|     25 | 7098 | `	SXUNUSED(apArg);` |
|     51 | 7099 | `	if( pThis == 0 \|\| !DtIsInit(pThis) \|\| PH7_NativeAttrObj(pThis,"end") != 0 ){` |
|      7 | 7100 | `		ph7_result_null(pCtx);` |
|      7 | 7101 | `		return PH7_OK;` |
|      - | 7102 | `	}` |
|     45 | 7103 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,"recurrences") - 1);` |
|     45 | 7104 | `	return PH7_OK;` |
|     26 | 7105 | `}` |
|      - | 7106 | `/*` |
|      - | 7107 | ` * The period walk, expressed as the vtable an InternalIterator drives (oo_native.c).` |
|      - | 7108 | ` * It uses the shared cursor slots: SRC is the period, CUR the date the cursor sits` |
|      - | 7109 | ` * on, KEY the emitted position and POS the loop counter (which differs from KEY,` |
|      - | 7110 | ` * since an excluded start date is stepped over without emitting one).` |
|      - | 7111 | ` */` |
|      - | 7112 | `#define DP_IT_STEP PH7_NATIVE_IT_POS` |
|      - | 7113 | `/*` |
|      - | 7114 | ` * One interval step from a date object: a NEW object, so a value already handed` |
|      - | 7115 | ` * to the caller is never mutated underneath it (php's iterator answers a fresh` |
|      - | 7116 | ` * object per position too).` |
|      - | 7117 | ` *` |
|      - | 7118 | `` * The step always ADDS, whatever the interval's `invert` says -- php's period`` |
|      - | 7119 | ` * walk reads the fields and not the flag, so a period built on an interval a` |
|      - | 7120 | `` * diff() answered (or on `$iv->invert = 1`) still runs FORWARD, while a`` |
|      - | 7121 | `` * negative FIELD (`createFromDateString('-1 day')` leaves d = -1 and invert 0)`` |
|      - | 7122 | ` * really does step backward. PHL honoured the flag, so such a period walked the` |
|      - | 7123 | ` * wrong way -- and with an END date rather than a recurrence count it walked` |
|      - | 7124 | ` * away from that end, stopped by nothing.` |
|      - | 7125 | ` */` |
|    270 | 7126 | `static ph7_class_instance * DpAdvance(ph7_vm *pVm,ph7_class_instance *pCur,` |
|      - | 7127 | `	ph7_class_instance *pIv)` |
|      1 | 7128 | `{` |
|    271 | 7129 | `	ph7_class_instance *pNext = PH7_CloneClassInstance(pCur);` |
|    271 | 7130 | `	if( pNext == 0 ){` |
|    ! 0 | 7131 | `		return 0;` |
|      - | 7132 | `	}` |
|    271 | 7133 | `	DtApplyInterval(&(*pVm),pCur,pNext,pIv,1);` |
|    271 | 7134 | `	return pNext;` |
|    136 | 7135 | `}` |
|      - | 7136 | `/*` |
|      - | 7137 | ` * Settle the cursor on the next date the period EMITS, mirroring the generator` |
|      - | 7138 | ` * this replaced: a start excluded by EXCLUDE_START_DATE is stepped over, an end` |
|      - | 7139 | ` * date stops the walk (inclusively under INCLUDE_END_DATE) and a recurrence count` |
|      - | 7140 | ` * bounds the number of steps instead.` |
|      - | 7141 | ` */` |
|    420 | 7142 | `static void DpSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 | 7143 | `{` |
|    421 | 7144 | `	ph7_class_instance *pPeriod = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|      - | 7145 | `	ph7_class_instance *pEnd,*pIv;` |
|      - | 7146 | `	int bInclStart,bInclEnd;` |
|      - | 7147 | `	sxi64 nTotal;` |
|    421 | 7148 | `	if( pPeriod == 0 ){` |
|    ! 0 | 7149 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 7150 | `		return;` |
|      - | 7151 | `	}` |
|    421 | 7152 | `	pEnd = PH7_NativeAttrObj(pPeriod,"end");` |
|    421 | 7153 | `	pIv = PH7_NativeAttrObj(pPeriod,"interval");` |
|    421 | 7154 | `	bInclStart = PH7_NativeAttrTruthy(pPeriod,"include_start_date");` |
|    421 | 7155 | `	bInclEnd = PH7_NativeAttrTruthy(pPeriod,"include_end_date");` |
|    421 | 7156 | `	nTotal = PH7_NativeAttrInt(pPeriod,"recurrences") + (bInclEnd ? 1 : 0);` |
|    231 | 7157 | `	for(;;){` |
|    443 | 7158 | `		ph7_class_instance *pCur = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_CUR);` |
|    443 | 7159 | `		sxi64 iStep = PH7_NativeAttrInt(pIt,DP_IT_STEP);` |
|      - | 7160 | `		ph7_class_instance *pNext;` |
|    443 | 7161 | `		if( pCur == 0 ){` |
|    ! 0 | 7162 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 7163 | `			return;` |
|      - | 7164 | `		}` |
|    443 | 7165 | `		if( pEnd != 0 ){` |
|     73 | 7166 | `			sxi64 iTs = PH7_NativeAttrInt(pCur,DT_TS);` |
|     73 | 7167 | `			sxi64 iEndTs = PH7_NativeAttrInt(pEnd,DT_TS);` |
|     73 | 7168 | `			if( bInclEnd ? (iTs > iEndTs) : (iTs >= iEndTs) ){` |
|     15 | 7169 | `				PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|     15 | 7170 | `				return;` |
|      1 | 7171 | `			}` |
|    402 | 7172 | `		}else if( iStep >= nTotal ){` |
|     67 | 7173 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|     67 | 7174 | `			return;` |
|      - | 7175 | `		}` |
|    365 | 7176 | `		if( iStep > 0 \|\| bInclStart ){` |
|    343 | 7177 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|    343 | 7178 | `			return;` |
|      - | 7179 | `		}` |
|      - | 7180 | `		/* The excluded start: step over it without emitting a key. */` |
|     23 | 7181 | `		if( pIv == 0 ){` |
|    ! 0 | 7182 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 7183 | `			return;` |
|      - | 7184 | `		}` |
|     23 | 7185 | `		pNext = DpAdvance(&(*pVm),pCur,pIv);` |
|     23 | 7186 | `		if( pNext == 0 ){` |
|    ! 0 | 7187 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 7188 | `			return;` |
|      - | 7189 | `		}` |
|     23 | 7190 | `		PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_CUR,pNext);` |
|     23 | 7191 | `		PH7_ClassInstanceUnref(pNext);` |
|     23 | 7192 | `		PH7_NativeSetAttrInt(&(*pVm),pIt,DP_IT_STEP,iStep + 1);` |
|      1 | 7193 | `	}` |
|    212 | 7194 | `}` |
|    190 | 7195 | `static void DpRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 | 7196 | `{` |
|      - | 7197 | `	ph7_class_instance *pPeriod,*pStart,*pCur;` |
|    191 | 7198 | `	pPeriod = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_SRC);` |
|    191 | 7199 | `	pStart = pPeriod ? PH7_NativeAttrObj(pPeriod,"start") : 0;` |
|    191 | 7200 | `	pCur = pStart ? PH7_CloneClassInstance(pStart) : 0;` |
|    191 | 7201 | `	if( pCur == 0 ){` |
|     19 | 7202 | `		PH7_NativeSetAttrBool(pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|     19 | 7203 | `		return;` |
|      - | 7204 | `	}` |
|    173 | 7205 | `	PH7_NativeSetAttrObj(pVm,pThis,PH7_NATIVE_IT_CUR,pCur);` |
|    173 | 7206 | `	PH7_ClassInstanceUnref(pCur);` |
|    173 | 7207 | `	PH7_NativeSetAttrInt(pVm,pThis,PH7_NATIVE_IT_KEY,0);` |
|    173 | 7208 | `	PH7_NativeSetAttrInt(pVm,pThis,DP_IT_STEP,0);` |
|    173 | 7209 | `	DpSettle(pVm,pThis);` |
|     96 | 7210 | `}` |
|    250 | 7211 | `static void DpNext(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 | 7212 | `{` |
|      - | 7213 | `	ph7_class_instance *pPeriod,*pIv,*pCur,*pNext;` |
|    251 | 7214 | `	pPeriod = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_SRC);` |
|    251 | 7215 | `	pIv = pPeriod ? PH7_NativeAttrObj(pPeriod,"interval") : 0;` |
|    251 | 7216 | `	pCur = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_CUR);` |
|    251 | 7217 | `	pNext = (pIv && pCur) ? DpAdvance(pVm,pCur,pIv) : 0;` |
|    251 | 7218 | `	if( pNext == 0 ){` |
|    ! 0 | 7219 | `		PH7_NativeSetAttrBool(pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 7220 | `		return;` |
|      - | 7221 | `	}` |
|    251 | 7222 | `	PH7_NativeSetAttrObj(pVm,pThis,PH7_NATIVE_IT_CUR,pNext);` |
|    251 | 7223 | `	PH7_ClassInstanceUnref(pNext);` |
|    251 | 7224 | `	PH7_NativeSetAttrInt(pVm,pThis,DP_IT_STEP,PH7_NativeAttrInt(pThis,DP_IT_STEP) + 1);` |
|    251 | 7225 | `	PH7_NativeSetAttrInt(pVm,pThis,PH7_NATIVE_IT_KEY,PH7_NativeAttrInt(pThis,PH7_NATIVE_IT_KEY) + 1);` |
|    251 | 7226 | `	DpSettle(pVm,pThis);` |
|    126 | 7227 | `}` |
|      - | 7228 | `/*` |
|      - | 7229 | ` * php's DatePeriod::$current IS the walk's cursor: the date the iterator sits on,` |
|      - | 7230 | ` * and -- once the walk is over -- the one PAST the end, the date that failed the` |
|      - | 7231 | ` * test. php writes it from the iterator's METHODS rather than from the walk, so a` |
|      - | 7232 | ` * getIterator() nobody has touched yet leaves it where the last walk left it, and` |
|      - | 7233 | ` * the first valid()/current()/key()/rewind()/next() moves it. PHL left it NULL` |
|      - | 7234 | ` * forever, so a program reading the period mid-walk (or after one) saw nothing.` |
|      - | 7235 | ` */` |
|   1004 | 7236 | `static void DpPublish(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 | 7237 | `{` |
|   1005 | 7238 | `	ph7_class_instance *pPeriod = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|   1005 | 7239 | `	if( pPeriod == 0 ){` |
|    ! 0 | 7240 | `		return;` |
|      - | 7241 | `	}` |
|   1005 | 7242 | `	PH7_NativeSetAttrObj(&(*pVm),pPeriod,"current",PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_CUR));` |
|    503 | 7243 | `}` |
|      - | 7244 | `/*` |
|      - | 7245 | ` * php refuses the WALK of an unconstructed period, not the door to it: its` |
|      - | 7246 | ` * getIterator() hands back a real InternalIterator and the DateObjectError` |
|      - | 7247 | ` * arrives at the first rewind(). The sentence is the ITERATOR's too, and it` |
|      - | 7248 | ` * differs from every other one in this family -- it names DatePeriod plainly` |
|      - | 7249 | ` * whatever the object's own class is, where a method called on a subclass reports` |
|      - | 7250 | `` * `SubP (inheriting DatePeriod)`.`` |
|      - | 7251 | ` */` |
|   1018 | 7252 | `static int DpIterGuard(ph7_context *pCtx,ph7_class_instance *pIt)` |
|      1 | 7253 | `{` |
|   1019 | 7254 | `	ph7_class_instance *pPeriod = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|   1019 | 7255 | `	if( pPeriod != 0 && DtIsInit(pPeriod) ){` |
|   1005 | 7256 | `		return 0;` |
|      - | 7257 | `	}` |
|     15 | 7258 | `	PH7_VmThrowException(pCtx,"DateObjectError",` |
|      - | 7259 | `		"Object of type DatePeriod has not been correctly initialized by calling "` |
|      - | 7260 | `		"parent::__construct() in its constructor");` |
|     15 | 7261 | `	return 1;` |
|    510 | 7262 | `}` |
|      - | 7263 | `static const PH7_NativeIterVtab sDpIterVtab = { DpRewind, DpNext, DpPublish, DpIterGuard };` |
|      - | 7264 | `/*` |
|      - | 7265 | ` * DatePeriod::getIterator(): Iterator` |
|      - | 7266 | ` *` |
|      - | 7267 | ` * This was a PHP GENERATOR, the one thing a C body cannot be. php answers an` |
|      - | 7268 | ` * InternalIterator here, so PHL answers the shared one (oo_native.c) driven by` |
|      - | 7269 | `` * the vtable above -- and stops diverging on `get_class($period->getIterator())`.`` |
|      - | 7270 | ` * A fresh one per call, as php's is.` |
|      - | 7271 | ` */` |
|    108 | 7272 | `static int vm_builtin_DatePeriod_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7273 | `{` |
|      - | 7274 | `	/* Raw: php's door does not screen the struct -- the ITERATOR does, at the` |
|      - | 7275 | `	 * first walk (DpIterGuard). */` |
|    109 | 7276 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|      - | 7277 | `	ph7_class_instance *pIt;` |
|     54 | 7278 | `	SXUNUSED(nArg);` |
|     54 | 7279 | `	SXUNUSED(apArg);` |
|    109 | 7280 | `	if( pThis == 0 ){` |
|    ! 0 | 7281 | `		return PH7_OK;` |
|      - | 7282 | `	}` |
|    109 | 7283 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|    109 | 7284 | `	if( pIt == 0 ){` |
|    ! 0 | 7285 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7286 | `	}` |
|    109 | 7287 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    109 | 7288 | `	return PH7_OK;` |
|     55 | 7289 | `}` |
|      - | 7290 | `/*` |
|      - | 7291 | ` * ---------------------------------------------------------------------------` |
|      - | 7292 | ` * The procedural date API.` |
|      - | 7293 | ` *` |
|      - | 7294 | ` * php's aliases are functions in their own right, not forwards: they reach the` |
|      - | 7295 | ` * same implementation the methods do, so an overridden method in a subclass is` |
|      - | 7296 | ` * NOT what they call, and the ones that can fail WARN and answer false where the` |
|      - | 7297 | ` * method throws. Each owes aBuiltinSig[] a row (vm_arg_check.c).` |
|      - | 7298 | ` * ---------------------------------------------------------------------------` |
|      - | 7299 | ` */` |
|      - | 7300 | `/*` |
|      - | 7301 | ` * The receiver argument of a procedural alias (already type-screened by its row),` |
|      - | 7302 | ` * or NULL for an object that was never constructed -- php's aliases reach the same` |
|      - | 7303 | ` * implementation the methods do, so they raise the same DateObjectError there` |
|      - | 7304 | ` * rather than warning the way the aliases that can FAIL do. Every caller already` |
|      - | 7305 | ` * treats a null the way the methods treat a null receiver: nothing to do, PH7_OK,` |
|      - | 7306 | ` * and the parked status reported at the host-call boundary.` |
|      - | 7307 | ` */` |
|    112 | 7308 | `static ph7_class_instance * DtArgObj(ph7_context *pCtx,int nArg,ph7_value **apArg,int iArg)` |
|      3 | 7309 | `{` |
|      - | 7310 | `	ph7_class_instance *pObj;` |
|    115 | 7311 | `	if( iArg >= nArg \|\| (apArg[iArg]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 7312 | `		return 0;` |
|      - | 7313 | `	}` |
|    115 | 7314 | `	pObj = (ph7_class_instance *)apArg[iArg]->x.pOther;` |
|    115 | 7315 | `	if( !DtIsInit(pObj) ){` |
|     29 | 7316 | `		DtThrowUninit(pCtx,pObj);` |
|     29 | 7317 | `		return 0;` |
|      - | 7318 | `	}` |
|     87 | 7319 | `	return pObj;` |
|     59 | 7320 | `}` |
|      - | 7321 | `/* Answer the receiver itself, the way every mutating alias does. */` |
|     20 | 7322 | `static void DtResultArg(ph7_context *pCtx,ph7_value **apArg)` |
|      1 | 7323 | `{` |
|     21 | 7324 | `	ph7_result_value(pCtx,apArg[0]);` |
|     21 | 7325 | `}` |
|      - | 7326 | `/* date_create()/date_create_immutable(): php answers false on a parse failure and` |
|      - | 7327 | ` * says nothing -- the constructor's exception does not escape the alias. */` |
|     52 | 7328 | `static int DtProcCreate(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zClass)` |
|      2 | 7329 | `{` |
|     54 | 7330 | `	ph7_vm *pVm = pCtx->pVm;` |
|     54 | 7331 | `	ph7_class *pClass = DtClass(pVm,zClass);` |
|      - | 7332 | `	ph7_class_instance *pObj;` |
|     54 | 7333 | `	const char *zIn = "now",*zZone;` |
|     54 | 7334 | `	int nIn = 3,nZone,iPos,iZoneKind;` |
|     54 | 7335 | `	sxi32 iZoneOff = 0;` |
|      - | 7336 | `	dt_state sState;` |
|      - | 7337 | `	char zNameBuf[16],cAt;` |
|      - | 7338 | `	const char *zErr;` |
|     54 | 7339 | `	if( pClass == 0 ){` |
|    ! 0 | 7340 | `		return PH7_OK;` |
|      - | 7341 | `	}` |
|     54 | 7342 | `	zZone = pVm->zDefTz;` |
|     54 | 7343 | `	nZone = (int)pVm->nDefTz;` |
|     54 | 7344 | `	iZoneKind = DT_ZONE_DEFAULT_KIND;` |
|     54 | 7345 | `	if( nArg > 0 ){` |
|     52 | 7346 | `		zIn = ph7_value_to_string(apArg[0],&nIn);` |
|     25 | 7347 | `	}` |
|     54 | 7348 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|      9 | 7349 | `		if( DtZoneArgInit(pCtx,apArg[1]) != 0 ){` |
|      3 | 7350 | `			return PH7_OK;` |
|      - | 7351 | `		}` |
|      7 | 7352 | `		DtZoneOf(apArg[1],&iZoneOff,&zZone,&nZone,&iZoneKind);` |
|      3 | 7353 | `	}` |
|     50 | 7354 | `	if( DtInitState(pCtx,zIn,nIn,iZoneOff,zZone,nZone,iZoneKind,&sState,zNameBuf,` |
|     27 | 7355 | `		sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0 ){` |
|      7 | 7356 | `		ph7_result_bool(pCtx,0);` |
|      7 | 7357 | `		return PH7_OK;` |
|      - | 7358 | `	}` |
|     46 | 7359 | `	pObj = DtNewInstance(pVm,pClass);` |
|     46 | 7360 | `	if( pObj == 0 ){` |
|    ! 0 | 7361 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7362 | `	}` |
|     46 | 7363 | `	DtStore(pVm,pObj,&sState);` |
|     46 | 7364 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     46 | 7365 | `	return PH7_OK;` |
|     28 | 7366 | `}` |
|     48 | 7367 | `static int vm_builtin_date_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7368 | `{` |
|     50 | 7369 | `	return DtProcCreate(pCtx,nArg,apArg,"DateTime");` |
|      2 | 7370 | `}` |
|      4 | 7371 | `static int vm_builtin_date_create_immutable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7372 | `{` |
|      5 | 7373 | `	return DtProcCreate(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 7374 | `}` |
|     10 | 7375 | `static int vm_builtin_date_create_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7376 | `{` |
|     11 | 7377 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTime");` |
|      1 | 7378 | `}` |
|      4 | 7379 | `static int vm_builtin_date_create_immutable_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7380 | `{` |
|      5 | 7381 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 7382 | `}` |
|     12 | 7383 | `static int vm_builtin_date_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7384 | `{` |
|     13 | 7385 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      - | 7386 | `	const char *zFmt;` |
|      - | 7387 | `	int nFmt;` |
|     13 | 7388 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|      3 | 7389 | `		return PH7_OK;` |
|      - | 7390 | `	}` |
|     11 | 7391 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|     11 | 7392 | `	DtFormatOf(pCtx,pObj,zFmt,nFmt);` |
|     11 | 7393 | `	return PH7_OK;` |
|      7 | 7394 | `}` |
|      - | 7395 | `/* date_modify(): php WARNS and answers false where DateTime::modify() throws. */` |
|     10 | 7396 | `static int vm_builtin_date_modify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7397 | `{` |
|     11 | 7398 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      - | 7399 | `	const char *zMod,*zErr;` |
|      - | 7400 | `	int nMod,iPos,iErrPos;` |
|      - | 7401 | `	char cAt;` |
|     11 | 7402 | `	sxi64 iTs = 0;` |
|     11 | 7403 | `	sxi32 iOff = 0;` |
|     11 | 7404 | `	int bOffSet = 0,uSec = 0;` |
|      - | 7405 | `	dt_parsed sVec;` |
|     11 | 7406 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|      3 | 7407 | `		return PH7_OK;` |
|      - | 7408 | `	}` |
|      9 | 7409 | `	zMod = ph7_value_to_string(apArg[1],&nMod);` |
|     13 | 7410 | `	iErrPos = DtParseEx(zMod,nMod,PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),` |
|      8 | 7411 | `		(int)PH7_NativeAttrInt(pObj,DT_US),DT_PARSE_OVERRIDE_TIME\|DT_PARSE_KEEP_ZONE,` |
|      8 | 7412 | `		&iTs,&iOff,&bOffSet,&uSec,&sVec,&pCtx->pVm->sDtLastErr);` |
|      9 | 7413 | `	if( iErrPos != 0 ){` |
|      3 | 7414 | `		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);` |
|      4 | 7415 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 7416 | `			"date_modify(): Failed to parse time string (%.*s) at position %d (%c): %s",` |
|      1 | 7417 | `			DtCStrLen(zMod,nMod),zMod,iPos,cAt,zErr);` |
|      3 | 7418 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7419 | `		return PH7_OK;` |
|      - | 7420 | `	}` |
|      7 | 7421 | `	DtStoreModified(pCtx->pVm,pObj,iTs,(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),&sVec);` |
|      7 | 7422 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,uSec);   /* see DateTime::modify() */` |
|      7 | 7423 | `	DtEpochRezone(pCtx->pVm,pObj,&sVec);` |
|      7 | 7424 | `	DtResultArg(pCtx,apArg);` |
|      7 | 7425 | `	return PH7_OK;` |
|      6 | 7426 | `}` |
|     12 | 7427 | `static int DtProcAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)` |
|      1 | 7428 | `{` |
|     13 | 7429 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|     13 | 7430 | `	ph7_class_instance *pIv = DtArgObj(pCtx,nArg,apArg,1);` |
|     13 | 7431 | `	if( pObj == 0 \|\| pIv == 0 ){` |
|      3 | 7432 | `		return PH7_OK;` |
|      - | 7433 | `	}` |
|     11 | 7434 | `	if( PH7_NativeAttrInt(pIv,"invert") ){` |
|    ! 0 | 7435 | `		iSign = -iSign;` |
|    ! 0 | 7436 | `	}` |
|     11 | 7437 | `	DtApplyInterval(pCtx->pVm,pObj,pObj,pIv,iSign);` |
|     11 | 7438 | `	DtResultArg(pCtx,apArg);` |
|     11 | 7439 | `	return PH7_OK;` |
|      7 | 7440 | `}` |
|      8 | 7441 | `static int vm_builtin_date_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7442 | `{` |
|      9 | 7443 | `	return DtProcAddSub(pCtx,nArg,apArg,1);` |
|      1 | 7444 | `}` |
|      4 | 7445 | `static int vm_builtin_date_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7446 | `{` |
|      5 | 7447 | `	return DtProcAddSub(pCtx,nArg,apArg,-1);` |
|      1 | 7448 | `}` |
|      8 | 7449 | `static int vm_builtin_date_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7450 | `{` |
|      9 | 7451 | `	ph7_class_instance *pBase = DtArgObj(pCtx,nArg,apArg,0);` |
|      9 | 7452 | `	ph7_class_instance *pTarget = DtArgObj(pCtx,nArg,apArg,1);` |
|      9 | 7453 | `	int bAbsolute = 0;` |
|      9 | 7454 | `	if( pBase == 0 \|\| pTarget == 0 ){` |
|      3 | 7455 | `		return PH7_OK;` |
|      - | 7456 | `	}` |
|      7 | 7457 | `	if( nArg > 2 ){` |
|    ! 0 | 7458 | `		bAbsolute = DtValueTruth(pCtx->pVm,apArg[2]);` |
|    ! 0 | 7459 | `	}` |
|      7 | 7460 | `	return DtDiffResult(pCtx,pBase,pTarget,bAbsolute);` |
|      5 | 7461 | `}` |
|      4 | 7462 | `static int vm_builtin_date_timestamp_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7463 | `{` |
|      5 | 7464 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      5 | 7465 | `	if( pObj ){` |
|      3 | 7466 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_TS));` |
|      1 | 7467 | `	}` |
|      5 | 7468 | `	return PH7_OK;` |
|      1 | 7469 | `}` |
|      2 | 7470 | `static int vm_builtin_date_timestamp_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7471 | `{` |
|      3 | 7472 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      3 | 7473 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|      3 | 7474 | `		return PH7_OK;` |
|      - | 7475 | `	}` |
|    ! 0 | 7476 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,ph7_value_to_int64(apArg[1]));` |
|    ! 0 | 7477 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,0);` |
|    ! 0 | 7478 | `	DtRezone(pCtx->pVm,pObj);   /* see DateTime::setTimestamp() */` |
|    ! 0 | 7479 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 7480 | `	return PH7_OK;` |
|      2 | 7481 | `}` |
|      4 | 7482 | `static int vm_builtin_date_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7483 | `{` |
|      5 | 7484 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      5 | 7485 | `	if( pObj == 0 ){` |
|      3 | 7486 | `		return PH7_OK;` |
|      - | 7487 | `	}` |
|      3 | 7488 | `	return DtTimezoneResult(pCtx,pObj);` |
|      3 | 7489 | `}` |
|      2 | 7490 | `static int vm_builtin_date_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7491 | `{` |
|      3 | 7492 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      3 | 7493 | `	const char *zName = "UTC";` |
|      3 | 7494 | `	int nName = 3,iKind = DT_ZONE_ID;` |
|      3 | 7495 | `	sxi32 iOff = 0;` |
|      3 | 7496 | `	if( pObj == 0 \|\| nArg < 2 \|\| !DtZoneOf(apArg[1],&iOff,&zName,&nName,&iKind) ){` |
|    ! 0 | 7497 | `		return PH7_OK;` |
|      - | 7498 | `	}` |
|      3 | 7499 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_OFF,iOff);` |
|      3 | 7500 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,DT_NAME,zName,nName);` |
|      3 | 7501 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_ZKIND,iKind);` |
|      3 | 7502 | `	DtResultArg(pCtx,apArg);` |
|      3 | 7503 | `	return PH7_OK;` |
|      2 | 7504 | `}` |
|      4 | 7505 | `static int vm_builtin_date_offset_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7506 | `{` |
|      5 | 7507 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      5 | 7508 | `	if( pObj ){` |
|      3 | 7509 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_OFF));` |
|      1 | 7510 | `	}` |
|      5 | 7511 | `	return PH7_OK;` |
|      1 | 7512 | `}` |
|      2 | 7513 | `static int vm_builtin_date_date_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7514 | `{` |
|      3 | 7515 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      3 | 7516 | `	if( pObj == 0 \|\| nArg < 4 ){` |
|      3 | 7517 | `		return PH7_OK;` |
|      - | 7518 | `	}` |
|    ! 0 | 7519 | `	DtSetDateOf(pCtx,pObj,ph7_value_to_int64(apArg[1]),ph7_value_to_int(apArg[2]),` |
|    ! 0 | 7520 | `		ph7_value_to_int(apArg[3]));` |
|    ! 0 | 7521 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 7522 | `	return PH7_OK;` |
|      2 | 7523 | `}` |
|      2 | 7524 | `static int vm_builtin_date_time_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7525 | `{` |
|      3 | 7526 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      3 | 7527 | `	if( pObj == 0 \|\| nArg < 3 ){` |
|      3 | 7528 | `		return PH7_OK;` |
|      - | 7529 | `	}` |
|    ! 0 | 7530 | `	DtSetTimeOf(pCtx,pObj,ph7_value_to_int(apArg[1]),ph7_value_to_int(apArg[2]),` |
|    ! 0 | 7531 | `		nArg > 3 ? ph7_value_to_int(apArg[3]) : 0,` |
|    ! 0 | 7532 | `		nArg > 4 ? ph7_value_to_int64(apArg[4]) : 0);` |
|    ! 0 | 7533 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 7534 | `	return PH7_OK;` |
|      2 | 7535 | `}` |
|      4 | 7536 | `static int vm_builtin_date_isodate_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7537 | `{` |
|      5 | 7538 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      5 | 7539 | `	if( pObj == 0 \|\| nArg < 3 ){` |
|      3 | 7540 | `		return PH7_OK;` |
|      - | 7541 | `	}` |
|      - | 7542 | `	{` |
|      3 | 7543 | `		sxi32 iOff = (sxi32)PH7_NativeAttrInt(pObj,DT_OFF);` |
|      4 | 7544 | `		DtStoreLocalOf(pCtx->pVm,pObj,` |
|      4 | 7545 | `			(sxi64)((sxu64)DtIsoDate(PH7_NativeAttrInt(pObj,DT_TS),iOff,` |
|      2 | 7546 | `				ph7_value_to_int64(apArg[1]),ph7_value_to_int64(apArg[2]),` |
|      3 | 7547 | `				nArg > 3 ? ph7_value_to_int64(apArg[3]) : 1) + (sxu64)iOff));` |
|      - | 7548 | `	}` |
|      3 | 7549 | `	DtResultArg(pCtx,apArg);` |
|      3 | 7550 | `	return PH7_OK;` |
|      3 | 7551 | `}` |
|      - | 7552 | `/*` |
|      - | 7553 | ` * The COMPONENT view php's two parse readers answer with: what the scanner READ,` |
|      - | 7554 | ` * field by field, rather than what a constructor would make of it. Nothing about` |
|      - | 7555 | `` * the clock reaches it -- a field the string never mentioned is `false`, not the`` |
|      - | 7556 | ` * base moment's -- and three parts of the shape are conditional.` |
|      - | 7557 | ` *` |
|      - | 7558 | `` * `is_localtime` is whether a TIMEZONE token was seen at all, which is not the`` |
|      - | 7559 | ` * same as one having been understood: an unknown name sets it and leaves` |
|      - | 7560 | `` * `zone_type` 0, with nothing else shown. A fixed OFFSET shows `zone` and`` |
|      - | 7561 | `` * `is_dst`, an ABBREVIATION shows those and its `tz_abbr`, and an IDENTIFIER`` |
|      - | 7562 | `` * shows only its name, twice. And the `relative` block appears when the parse`` |
|      - | 7563 | `` * spelled a relative element -- `now` and `today` do not -- carrying the weekday`` |
|      - | 7564 | ` * when one was hunted, the business-day count when that special was named, and` |
|      - | 7565 | `` * the `first\|last day of` flag as `true`.`` |
|      - | 7566 | ` */` |
|      - | 7567 | `typedef struct dt_comp dt_comp;` |
|      - | 7568 | `struct dt_comp` |
|      - | 7569 | `{` |
|      - | 7570 | ``	sxi64 y,mo,d,h,mi,s,us;   /* DT_UNSET is php's own, and `false` on show */`` |
|      - | 7571 | `	int iZoneSeen;            /* php's is_localtime */` |
|      - | 7572 | `	int iZoneKind;            /* php's timezone_type, 0 when nothing resolved */` |
|      - | 7573 | `	sxi32 iOff;` |
|      - | 7574 | `	const char *zName;` |
|      - | 7575 | `	int nName;` |
|      - | 7576 | `	int bHaveRel;` |
|      - | 7577 | `	sxi64 ry,rm,rd,rh,ri,rs;` |
|      - | 7578 | `	int bWday,iWday;` |
|      - | 7579 | `	int bWeekdays;` |
|      - | 7580 | `	sxi64 iWeekdays;` |
|      - | 7581 | `	int iFirstLast;` |
|      - | 7582 | `};` |
|    226 | 7583 | `static int DtCompResult(ph7_context *pCtx,const dt_comp *pC,const phl_dt_lasterr *pRec)` |
|      2 | 7584 | `{` |
|      - | 7585 | `	ph7_value *pArr,*pRel,*pWarn,*pErrs,*pVal;` |
|      - | 7586 | `	int k;` |
|    228 | 7587 | `	pArr = ph7_context_new_array(pCtx);` |
|    228 | 7588 | `	pWarn = ph7_context_new_array(pCtx);` |
|    228 | 7589 | `	pErrs = ph7_context_new_array(pCtx);` |
|    228 | 7590 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    228 | 7591 | `	if( pArr == 0 \|\| pWarn == 0 \|\| pErrs == 0 \|\| pVal == 0 ){` |
|    ! 0 | 7592 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7593 | `	}` |
|      - | 7594 | `#define DT_PUT(zKey) ph7_array_add_strkey_elem(pArr,zKey,pVal)` |
|      - | 7595 | `#define DT_PUTFIELD(zKey,iVal) do{ \` |
|      - | 7596 | `		if( (iVal) == DT_UNSET ){ ph7_value_bool(pVal,0); } \` |
|      - | 7597 | `		else { ph7_value_int64(pVal,(iVal)); } \` |
|      - | 7598 | `		DT_PUT(zKey); \` |
|      - | 7599 | `	}while(0)` |
|    228 | 7600 | `	DT_PUTFIELD("year",pC->y);` |
|    228 | 7601 | `	DT_PUTFIELD("month",pC->mo);` |
|    228 | 7602 | `	DT_PUTFIELD("day",pC->d);` |
|    228 | 7603 | `	DT_PUTFIELD("hour",pC->h);` |
|    228 | 7604 | `	DT_PUTFIELD("minute",pC->mi);` |
|    228 | 7605 | `	DT_PUTFIELD("second",pC->s);` |
|    228 | 7606 | `	if( pC->us == DT_UNSET ){` |
|    135 | 7607 | `		ph7_value_bool(pVal,0);` |
|     68 | 7608 | `	}else{` |
|     94 | 7609 | `		ph7_value_double(pVal,(ph7_real)((double)pC->us / 1000000.0));` |
|      - | 7610 | `	}` |
|    228 | 7611 | `	DT_PUT("fraction");` |
|    248 | 7612 | `	for( k = 0 ; k < pRec->nWarnKept ; k++ ){` |
|     21 | 7613 | `		ph7_value_string(pVal,pRec->azWarn[k],-1);` |
|     21 | 7614 | `		ph7_array_add_intkey_elem(pWarn,pRec->aWarnPos[k],pVal);` |
|     21 | 7615 | `		ph7_value_reset_string_cursor(pVal);` |
|     11 | 7616 | `	}` |
|      - | 7617 | `	{` |
|    228 | 7618 | `		const phl_dt_diag_row *aRow = (const phl_dt_diag_row *)SyBlobData(&pRec->sErr);` |
|    308 | 7619 | `		for( k = 0 ; k < pRec->nErrKept ; k++ ){` |
|     81 | 7620 | `			ph7_value_string(pVal,aRow[k].zMsg,-1);` |
|     81 | 7621 | `			ph7_array_add_intkey_elem(pErrs,aRow[k].iPos,pVal);` |
|     81 | 7622 | `			ph7_value_reset_string_cursor(pVal);` |
|     41 | 7623 | `		}` |
|      - | 7624 | `	}` |
|    228 | 7625 | `	ph7_value_int(pVal,pRec->nWarn);` |
|    228 | 7626 | `	DT_PUT("warning_count");` |
|    228 | 7627 | `	ph7_array_add_strkey_elem(pArr,"warnings",pWarn);` |
|    228 | 7628 | `	ph7_value_int(pVal,pRec->nErr);` |
|    228 | 7629 | `	DT_PUT("error_count");` |
|    228 | 7630 | `	ph7_array_add_strkey_elem(pArr,"errors",pErrs);` |
|    228 | 7631 | `	ph7_value_bool(pVal,pC->iZoneSeen != 0);` |
|    228 | 7632 | `	DT_PUT("is_localtime");` |
|    228 | 7633 | `	if( pC->iZoneSeen ){` |
|     64 | 7634 | `		ph7_value_int(pVal,pC->iZoneKind);` |
|     64 | 7635 | `		DT_PUT("zone_type");` |
|     64 | 7636 | `		if( pC->iZoneKind == DT_ZONE_OFFSET \|\| pC->iZoneKind == DT_ZONE_ABBR ){` |
|     47 | 7637 | `			ph7_value_int64(pVal,(sxi64)pC->iOff);` |
|     47 | 7638 | `			DT_PUT("zone");` |
|     47 | 7639 | `			ph7_value_bool(pVal,0);   /* no tz database, so nothing is ever DST */` |
|     47 | 7640 | `			DT_PUT("is_dst");` |
|     23 | 7641 | `		}` |
|     64 | 7642 | `		if( pC->iZoneKind == DT_ZONE_ABBR \|\| pC->iZoneKind == DT_ZONE_ID ){` |
|     26 | 7643 | `			ph7_value_string(pVal,pC->zName,pC->nName);` |
|     26 | 7644 | `			DT_PUT("tz_abbr");` |
|     26 | 7645 | `			ph7_value_reset_string_cursor(pVal);` |
|     12 | 7646 | `		}` |
|     64 | 7647 | `		if( pC->iZoneKind == DT_ZONE_ID ){` |
|     10 | 7648 | `			ph7_value_string(pVal,pC->zName,pC->nName);` |
|     10 | 7649 | `			DT_PUT("tz_id");` |
|     10 | 7650 | `			ph7_value_reset_string_cursor(pVal);` |
|      4 | 7651 | `		}` |
|     31 | 7652 | `	}` |
|    228 | 7653 | `	if( pC->bHaveRel ){` |
|     53 | 7654 | `		pRel = ph7_context_new_array(pCtx);` |
|     53 | 7655 | `		if( pRel == 0 ){` |
|    ! 0 | 7656 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7657 | `		}` |
|      - | 7658 | `#define DT_PUTREL(zKey,iVal) do{ \` |
|      - | 7659 | `			ph7_value_int64(pVal,(iVal)); \` |
|      - | 7660 | `			ph7_array_add_strkey_elem(pRel,zKey,pVal); \` |
|      - | 7661 | `		}while(0)` |
|     53 | 7662 | `		DT_PUTREL("year",pC->ry);` |
|     53 | 7663 | `		DT_PUTREL("month",pC->rm);` |
|     53 | 7664 | `		DT_PUTREL("day",pC->rd);` |
|     53 | 7665 | `		DT_PUTREL("hour",pC->rh);` |
|     53 | 7666 | `		DT_PUTREL("minute",pC->ri);` |
|     53 | 7667 | `		DT_PUTREL("second",pC->rs);` |
|     53 | 7668 | `		if( pC->bWday ){` |
|     23 | 7669 | `			DT_PUTREL("weekday",(sxi64)pC->iWday);` |
|     11 | 7670 | `		}` |
|     53 | 7671 | `		if( pC->bWeekdays ){` |
|      5 | 7672 | `			DT_PUTREL("weekdays",pC->iWeekdays);` |
|      2 | 7673 | `		}` |
|     53 | 7674 | `		if( pC->iFirstLast ){` |
|      5 | 7675 | `			ph7_value_bool(pVal,1);` |
|      7 | 7676 | `			ph7_array_add_strkey_elem(pRel,` |
|      4 | 7677 | `				pC->iFirstLast == 1 ? "first_day_of_month" : "last_day_of_month",pVal);` |
|      2 | 7678 | `		}` |
|      - | 7679 | `#undef DT_PUTREL` |
|     53 | 7680 | `		ph7_array_add_strkey_elem(pArr,"relative",pRel);` |
|     26 | 7681 | `	}` |
|      - | 7682 | `#undef DT_PUTFIELD` |
|      - | 7683 | `#undef DT_PUT` |
|    228 | 7684 | `	ph7_result_value(pCtx,pArr);` |
|    228 | 7685 | `	return PH7_OK;` |
|    115 | 7686 | `}` |
|      - | 7687 | `/*` |
|      - | 7688 | ` * date_parse(): the same scanner every constructor runs, showing what it read.` |
|      - | 7689 | ` * The diagnostics are the scan's own (see DtParseFields) and are NOT published` |
|      - | 7690 | ` * as getLastErrors() -- php leaves that record to the constructors -- so the` |
|      - | 7691 | ` * scan writes into one of this call's own.` |
|      - | 7692 | ` */` |
|    104 | 7693 | `static int vm_builtin_date_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7694 | `{` |
|    106 | 7695 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 7696 | `	phl_dt_lasterr sRec;` |
|      - | 7697 | `	dt_parsed sVec;` |
|      - | 7698 | `	dt_comp sC;` |
|    106 | 7699 | `	const char *zIn = "";` |
|    106 | 7700 | `	sxi64 iTs = 0;` |
|    106 | 7701 | `	sxi32 iOff = 0;` |
|    106 | 7702 | `	int nIn = 0,bOffSet = 0,uSec = 0,rc;` |
|    106 | 7703 | `	if( nArg < 1 ){` |
|    ! 0 | 7704 | `		return PH7_OK;` |
|      - | 7705 | `	}` |
|    106 | 7706 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    106 | 7707 | `	SyZero(&sRec,sizeof(sRec));` |
|    106 | 7708 | `	SyBlobInit(&sRec.sErr,&pVm->sAllocator);` |
|    106 | 7709 | `	DtFieldsInit(&sVec,0);` |
|    106 | 7710 | `	DtParseEx(zIn,nIn,0,0,0,0,&iTs,&iOff,&bOffSet,&uSec,&sVec,&sRec);` |
|    106 | 7711 | `	SyZero(&sC,sizeof(sC));` |
|    106 | 7712 | `	sC.y = sVec.y; sC.mo = sVec.m; sC.d = sVec.d;` |
|    106 | 7713 | `	sC.h = sVec.h; sC.mi = sVec.i; sC.s = sVec.s;` |
|    106 | 7714 | `	sC.us = sVec.bUsUnset ? DT_UNSET : sVec.us;` |
|    106 | 7715 | `	sC.iZoneSeen = sVec.nZoneTok > 0;` |
|    170 | 7716 | `	sC.iZoneKind = sVec.bOffSet == 0 ? 0` |
|     76 | 7717 | `		: (sVec.bOffSet == 1 ? DT_ZONE_OFFSET` |
|     15 | 7718 | `		   : (sVec.bZoneIdent ? DT_ZONE_ID : DT_ZONE_ABBR));` |
|    106 | 7719 | `	sC.iOff = sVec.iOff;` |
|    106 | 7720 | `	sC.zName = sVec.zZone;` |
|    106 | 7721 | `	sC.nName = sVec.nZone;` |
|    106 | 7722 | `	sC.bHaveRel = sVec.bHaveRel;` |
|    106 | 7723 | `	sC.ry = sVec.ry; sC.rm = sVec.rm; sC.rd = sVec.rd;` |
|    106 | 7724 | `	sC.rh = sVec.rh; sC.ri = sVec.ri; sC.rs = sVec.rs;` |
|    106 | 7725 | `	sC.bWday = sVec.bWday; sC.iWday = sVec.iWday;` |
|    106 | 7726 | `	sC.bWeekdays = sVec.bWeekdays; sC.iWeekdays = sVec.iWeekdays;` |
|    106 | 7727 | `	sC.iFirstLast = sVec.iFirstLast;` |
|    106 | 7728 | `	rc = DtCompResult(pCtx,&sC,&sRec);` |
|    106 | 7729 | `	SyBlobRelease(&sRec.sErr);` |
|    106 | 7730 | `	return rc;` |
|     54 | 7731 | `}` |
|      - | 7732 | `/*` |
|      - | 7733 | ` * date_parse_from_format(): the FORMAT scanner's components, through the very` |
|      - | 7734 | ` * presenter date_parse() answers with -- php reads both of them out of one` |
|      - | 7735 | `` * function, so a field the scan left unset is `false` in either, and the`` |
|      - | 7736 | `` * `relative` block appears here for exactly one reason: a textual DAY, which`` |
|      - | 7737 | ` * this parser records as a weekday to move to rather than as a day.` |
|      - | 7738 | ` *` |
|      - | 7739 | ` * Like date_parse(), it publishes NOTHING into getLastErrors() -- php leaves` |
|      - | 7740 | ` * that record to the constructors -- so the scan's diagnostics go into a record` |
|      - | 7741 | ` * of this call's own.` |
|      - | 7742 | ` */` |
|    122 | 7743 | `static int vm_builtin_date_parse_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7744 | `{` |
|    123 | 7745 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 7746 | `	phl_dt_lasterr sRec;` |
|      - | 7747 | `	dt_ff_res sRes;` |
|      - | 7748 | `	dt_comp sC;` |
|      - | 7749 | `	const char *zFmt,*zIn;` |
|      - | 7750 | `	int nFmt,nIn,rc;` |
|    123 | 7751 | `	if( nArg < 2 ){` |
|    ! 0 | 7752 | `		return PH7_OK;` |
|      - | 7753 | `	}` |
|    123 | 7754 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    123 | 7755 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    123 | 7756 | `	DtFromFormat(zFmt,nFmt,zIn,nIn,&sRes);` |
|    123 | 7757 | `	SyZero(&sRec,sizeof(sRec));` |
|    123 | 7758 | `	SyBlobInit(&sRec.sErr,&pVm->sAllocator);` |
|    123 | 7759 | `	DtFfDiagInto(&sRec,&sRes.sDiag);` |
|    123 | 7760 | `	SyZero(&sC,sizeof(sC));` |
|    123 | 7761 | `	sC.y = sRes.y; sC.mo = sRes.mo; sC.d = sRes.d;` |
|    123 | 7762 | `	sC.h = sRes.h; sC.mi = sRes.mi; sC.s = sRes.s; sC.us = sRes.us;` |
|    123 | 7763 | `	sC.iZoneSeen = sRes.bLocal;` |
|    123 | 7764 | `	sC.iZoneKind = sRes.iOffKind;` |
|    123 | 7765 | `	sC.iOff = sRes.iOff;` |
|    123 | 7766 | `	sC.zName = sRes.zName;` |
|    123 | 7767 | `	sC.nName = sRes.nName;` |
|    123 | 7768 | `	sC.bHaveRel = sRes.bWday;` |
|    123 | 7769 | `	sC.bWday = sRes.bWday;` |
|    123 | 7770 | `	sC.iWday = (int)sRes.iWday;` |
|    123 | 7771 | `	rc = DtCompResult(pCtx,&sC,&sRec);` |
|    123 | 7772 | `	SyBlobRelease(&sRec.sErr);` |
|    123 | 7773 | `	return rc;` |
|     62 | 7774 | `}` |
|      - | 7775 | `/* date_interval_create_from_date_string(): warns and answers false where the` |
|      - | 7776 | ` * method throws. */` |
|     10 | 7777 | `static int vm_builtin_date_interval_create_from_date_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7778 | `{` |
|     11 | 7779 | `	const char *zIn,*zReason = "";` |
|     11 | 7780 | `	int nIn,iPos = 0,bNonRel = 0;` |
|     11 | 7781 | `	char cAt = ' ';` |
|      - | 7782 | `	ph7_class_instance *pObj;` |
|     11 | 7783 | `	if( nArg < 1 ){` |
|    ! 0 | 7784 | `		return PH7_OK;` |
|      - | 7785 | `	}` |
|     11 | 7786 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|     11 | 7787 | `	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason,&bNonRel);` |
|     11 | 7788 | `	if( pObj == 0 ){` |
|      3 | 7789 | `		if( bNonRel ){` |
|      4 | 7790 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 7791 | `				"date_interval_create_from_date_string(): String '%.*s' contains "` |
|      1 | 7792 | `				"non-relative elements",DtCStrLen(zIn,nIn),zIn);` |
|      3 | 7793 | `			ph7_result_bool(pCtx,0);` |
|      3 | 7794 | `			return PH7_OK;` |
|      - | 7795 | `		}` |
|    ! 0 | 7796 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 7797 | `			"date_interval_create_from_date_string(): Unknown or bad format (%.*s) "` |
|    ! 0 | 7798 | `			"at position %d (%c): %s",DtCStrLen(zIn,nIn),zIn,iPos,cAt,zReason);` |
|    ! 0 | 7799 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7800 | `		return PH7_OK;` |
|      - | 7801 | `	}` |
|      9 | 7802 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      9 | 7803 | `	return PH7_OK;` |
|      6 | 7804 | `}` |
|      6 | 7805 | `static int vm_builtin_date_interval_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7806 | `{` |
|      7 | 7807 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      - | 7808 | `	const char *zFmt;` |
|      - | 7809 | `	int nFmt;` |
|      7 | 7810 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|      3 | 7811 | `		return PH7_OK;` |
|      - | 7812 | `	}` |
|      5 | 7813 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|      5 | 7814 | `	DtIvFormat(pCtx,pObj,zFmt,nFmt);` |
|      5 | 7815 | `	return PH7_OK;` |
|      4 | 7816 | `}` |
|    ! 0 | 7817 | `static int vm_builtin_date_get_last_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7818 | `{` |
|    ! 0 | 7819 | `	return vm_builtin_DateTime_getLastErrors(pCtx,nArg,apArg);` |
|    ! 0 | 7820 | `}` |
|      - | 7821 | `/* timezone_open(): warns and answers false where the constructor throws. */` |
|     76 | 7822 | `static int vm_builtin_timezone_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7823 | `{` |
|     78 | 7824 | `	ph7_vm *pVm = pCtx->pVm;` |
|     78 | 7825 | `	ph7_class *pClass = DtClass(pVm,"DateTimeZone");` |
|      - | 7826 | `	ph7_class_instance *pObj;` |
|      - | 7827 | `	const char *zTz,*zName;` |
|     78 | 7828 | `	int nTz,nName,iKind = DT_ZONE_ID,rc;` |
|     78 | 7829 | `	sxi32 iOff = 0;` |
|      - | 7830 | `	char zBuf[16];` |
|     78 | 7831 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 7832 | `		return PH7_OK;` |
|      - | 7833 | `	}` |
|     78 | 7834 | `	zTz = ph7_value_to_string(apArg[0],&nTz);` |
|     78 | 7835 | `	rc = DtZoneParse(zTz,nTz,&iOff,&zName,&nName,&iKind,zBuf,sizeof(zBuf));` |
|     78 | 7836 | `	if( rc != 0 ){` |
|    100 | 7837 | `		PH7_VmThrowWarningFmt(pVm,` |
|     33 | 7838 | `			rc == -2 ? "timezone_open(): Timezone offset is out of range (%.*s)"` |
|     33 | 7839 | `			         : "timezone_open(): Unknown or bad timezone (%.*s)",nTz,zTz);` |
|     67 | 7840 | `		ph7_result_bool(pCtx,0);` |
|     67 | 7841 | `		return PH7_OK;` |
|      - | 7842 | `	}` |
|     12 | 7843 | `	pObj = DtNewInstance(pVm,pClass);` |
|     12 | 7844 | `	if( pObj == 0 ){` |
|    ! 0 | 7845 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7846 | `	}` |
|     12 | 7847 | `	PH7_NativeSetAttrInt(pVm,pObj,DTZ_OFF,iOff);` |
|     12 | 7848 | `	PH7_NativeSetAttrStr(pVm,pObj,DTZ_NAME,zName,nName);` |
|     12 | 7849 | `	PH7_NativeSetAttrInt(pVm,pObj,DTZ_KIND,iKind);` |
|     12 | 7850 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     12 | 7851 | `	return PH7_OK;` |
|     40 | 7852 | `}` |
|      6 | 7853 | `static int vm_builtin_timezone_name_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7854 | `{` |
|      7 | 7855 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      - | 7856 | `	const char *zName;` |
|      - | 7857 | `	int nName;` |
|      7 | 7858 | `	if( pObj == 0 ){` |
|      3 | 7859 | `		return PH7_OK;` |
|      - | 7860 | `	}` |
|      5 | 7861 | `	PH7_NativeAttrStr(pObj,DTZ_NAME,&zName,&nName);` |
|      5 | 7862 | `	ph7_result_string(pCtx,zName,nName);` |
|      5 | 7863 | `	return PH7_OK;` |
|      4 | 7864 | `}` |
|      6 | 7865 | `static int vm_builtin_timezone_offset_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7866 | `{` |
|      7 | 7867 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      7 | 7868 | `	if( nArg > 1 && DtArgInit(pCtx,apArg[1]) != 0 ){` |
|      3 | 7869 | `		return PH7_OK;` |
|      - | 7870 | `	}` |
|      5 | 7871 | `	if( pObj ){` |
|      5 | 7872 | `		DtZoneOffsetResult(pCtx,pObj,` |
|      2 | 7873 | `			nArg > 1 && (apArg[1]->iFlags & MEMOBJ_OBJ) ?` |
|      2 | 7874 | `				(ph7_class_instance *)apArg[1]->x.pOther : 0);` |
|      1 | 7875 | `	}` |
|      5 | 7876 | `	return PH7_OK;` |
|      4 | 7877 | `}` |
|      - | 7878 | `/* timezone_location_get(DateTimeZone $object) */` |
|      4 | 7879 | `static int vm_builtin_timezone_location_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7880 | `{` |
|      5 | 7881 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      5 | 7882 | `	if( pObj == 0 ){` |
|    ! 0 | 7883 | `		return PH7_OK;` |
|      - | 7884 | `	}` |
|      5 | 7885 | `	return DtZoneLocationResult(pCtx,pObj);` |
|      3 | 7886 | `}` |
|      4 | 7887 | `static int vm_builtin_timezone_transitions_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7888 | `{` |
|      5 | 7889 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      5 | 7890 | `	if( pObj == 0 ){` |
|    ! 0 | 7891 | `		return PH7_OK;` |
|      - | 7892 | `	}` |
|      - | 7893 | `	/* The zone shifts every argument along by one. */` |
|      9 | 7894 | `	return DtZoneTransitionsResult(pCtx,pObj,` |
|      3 | 7895 | `		nArg > 1 ? ph7_value_to_int64(apArg[1]) : (-(sxi64)0x7FFFFFFFFFFFFFFF - 1),` |
|      3 | 7896 | `		nArg > 2 ? ph7_value_to_int64(apArg[2]) : (sxi64)0x7FFFFFFF);` |
|      3 | 7897 | `}` |
|      - | 7898 | `/*` |
|      - | 7899 | ` * The four private slots a date object keeps its state in. Both classes declare` |
|      - | 7900 | `` * them: `trait __DtCoreT` had no native equivalent, and replaying the table is`` |
|      - | 7901 | `` * exactly what `use __DtCoreT` did.`` |
|      - | 7902 | ` */` |
|      - | 7903 | `#define DT_NATIVE_STATE_PROPS \` |
|      - | 7904 | `	{ DT_TS,   PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \` |
|      - | 7905 | `	{ DT_OFF,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \` |
|      - | 7906 | `	{ DT_NAME, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 }, 0 }, \` |
|      - | 7907 | `	{ DT_US,   PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \` |
|      - | 7908 | `	{ DT_ZKIND,PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, DT_ZONE_ID, 0, 0.0 }, 0 }, \` |
|      - | 7909 | `	{ DT_INIT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }` |
|      - | 7910 | `/*` |
|      - | 7911 | ` * The methods DateTime and DateTimeImmutable share -- the whole of the old trait` |
|      - | 7912 | ` * plus the mutators, whose one difference (write $this, or write a clone) the` |
|      - | 7913 | ` * bodies decide from the receiver's class. php's own signatures: they are the` |
|      - | 7914 | ` * single source of truth for arity, coercion and Reflection here, so the casts the` |
|      - | 7915 | `` * chunk wrote by hand (`(string)$format`, `(int)$timestamp`) are declared types now`` |
|      - | 7916 | ` * and the methods reject what php rejects.` |
|      - | 7917 | ` */` |
|      - | 7918 | `/* The methods DateTime and DateTimeImmutable share. CLS is the OWNING class name` |
|      - | 7919 | ` * as a string literal, because php's stubs write the concrete class rather than` |
|      - | 7920 | `` * `static` for the legacy mutators -- DateTime::add reports DateTime and`` |
|      - | 7921 | ` * DateTimeImmutable::add reports DateTimeImmutable. The two that php really does` |
|      - | 7922 | `` * declare `static` (setMicrosecond) and the two it declares for real rather than`` |
|      - | 7923 | ` * tentatively (getMicrosecond, and setMicrosecond again) are written as they are:` |
|      - | 7924 | `` * a leading `@` is php's @tentative-return-type, and nearly every method here has`` |
|      - | 7925 | ` * one. */` |
|      - | 7926 | `#define DT_NATIVE_SHARED_METHODS(CLS) \` |
|      - | 7927 | `	{ "__construct",     PH7_MOD_PUBLIC, "string $datetime = 'now', ?DateTimeZone $timezone = null", "", \` |
|      - | 7928 | `	  vm_builtin_DateTime_construct }, \` |
|      - | 7929 | `	{ "format",          PH7_MOD_PUBLIC, "string $format", "@string", vm_builtin_DateTime_format }, \` |
|      - | 7930 | `	{ "getTimestamp",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_DateTime_getTimestamp }, \` |
|      - | 7931 | `	{ "getMicrosecond",  PH7_MOD_PUBLIC, "", "int", vm_builtin_DateTime_getMicrosecond }, \` |
|      - | 7932 | `	{ "getOffset",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_DateTime_getOffset }, \` |
|      - | 7933 | `	{ "getTimezone",     PH7_MOD_PUBLIC, "", "@DateTimeZone\|false", vm_builtin_DateTime_getTimezone }, \` |
|      - | 7934 | `	{ "diff",            PH7_MOD_PUBLIC, "DateTimeInterface $targetObject, bool $absolute = false", \` |
|      - | 7935 | `	  "@DateInterval", vm_builtin_DateTime_diff }, \` |
|      - | 7936 | `	{ "modify",          PH7_MOD_PUBLIC, "string $modifier", "@" CLS, vm_builtin_DateTime_modify }, \` |
|      - | 7937 | `	{ "setTimestamp",    PH7_MOD_PUBLIC, "int $timestamp", "@" CLS, vm_builtin_DateTime_setTimestamp }, \` |
|      - | 7938 | `	{ "setMicrosecond",  PH7_MOD_PUBLIC, "int $microsecond", "static", vm_builtin_DateTime_setMicrosecond }, \` |
|      - | 7939 | `	{ "setTimezone",     PH7_MOD_PUBLIC, "DateTimeZone $timezone", "@" CLS, vm_builtin_DateTime_setTimezone }, \` |
|      - | 7940 | `	{ "setDate",         PH7_MOD_PUBLIC, "int $year, int $month, int $day", "@" CLS, \` |
|      - | 7941 | `	  vm_builtin_DateTime_setDate }, \` |
|      - | 7942 | `	{ "setTime",         PH7_MOD_PUBLIC, \` |
|      - | 7943 | `	  "int $hour, int $minute, int $second = 0, int $microsecond = 0", "@" CLS, \` |
|      - | 7944 | `	  vm_builtin_DateTime_setTime }, \` |
|      - | 7945 | `	{ "setISODate",      PH7_MOD_PUBLIC, "int $year, int $week, int $dayOfWeek = 1", "@" CLS, \` |
|      - | 7946 | `	  vm_builtin_DateTime_setISODate }, \` |
|      - | 7947 | `	{ "add",             PH7_MOD_PUBLIC, "DateInterval $interval", "@" CLS, vm_builtin_DateTime_add }, \` |
|      - | 7948 | `	{ "sub",             PH7_MOD_PUBLIC, "DateInterval $interval", "@" CLS, vm_builtin_DateTime_sub }, \` |
|      - | 7949 | `	{ "getLastErrors",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "@array\|false", \` |
|      - | 7950 | `	  vm_builtin_DateTime_getLastErrors }` |
|      - | 7951 | `/*` |
|      - | 7952 | ` * php's add_common_properties(): after the presented shape, the instance's own` |
|      - | 7953 | ` * php-visible slots -- a SUBCLASS's declared properties, which php serializes` |
|      - | 7954 | ` * alongside the internal state. A key the presented shape already wrote WINS` |
|      - | 7955 | ` * (zend_hash_add, not update), and a hidden engine slot is never a candidate:` |
|      - | 7956 | ` * this is the one walk in the date family that must skip them.` |
|      - | 7957 | ` */` |
|    804 | 7958 | `static void DtAddCommonProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|      2 | 7959 | `{` |
|    402 | 7960 | `	SXUNUSED(pVm);` |
|    806 | 7961 | `	if( (pOut->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 7962 | `		return;` |
|      - | 7963 | `	}` |
|      - | 7964 | `	/* MANGLED, the way php mangles a non-public property everywhere it hands an` |
|      - | 7965 | ``	 * object's own table out: a subclass's `protected $p` serializes as`` |
|      - | 7966 | `	 * "\0*\0p" and casts to that key, and only the mangling keeps two same-named` |
|      - | 7967 | `	 * members from different visibility levels apart. */` |
|    806 | 7968 | `	PH7_ClassInstanceOwnPropsToHashmap(pThis,(ph7_hashmap *)pOut->x.pOther);` |
|    404 | 7969 | `}` |
|      - | 7970 | `/*` |
|      - | 7971 | ` * The INTERNAL shape of a class whose state is its own public properties -- the` |
|      - | 7972 | ` * ones the named class declares, in declared order, read off the instance.` |
|      - | 7973 | ` *` |
|      - | 7974 | ` * Not a walk of the instance's TABLE: a subclass that redeclares one of the names` |
|      - | 7975 | ` * takes over its slot and its POSITION, and php still shows the value where its` |
|      - | 7976 | ` * own shape puts it.` |
|      - | 7977 | ` */` |
|     46 | 7978 | `static void DtAddNativeProps(ph7_vm *pVm,ph7_class_instance *pThis,const char *zBase,ph7_value *pOut)` |
|      2 | 7979 | `{` |
|     48 | 7980 | `	ph7_class *pClass = DtClass(&(*pVm),zBase);` |
|      - | 7981 | `	SyHashEntry *pEntry;` |
|     48 | 7982 | `	if( pClass == 0 ){` |
|    ! 0 | 7983 | `		return;` |
|      - | 7984 | `	}` |
|     48 | 7985 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    576 | 7986 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    530 | 7987 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      - | 7988 | `		ph7_value *pVal;` |
|      - | 7989 | `		ph7_value sKey;` |
|      - | 7990 | `		SyHashEntry *pOwn;` |
|    530 | 7991 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|    112 | 7992 | `			continue;` |
|      - | 7993 | `		}` |
|      - | 7994 | `		/* ...and a slot THIS object hides is not part of its shape either: a` |
|      - | 7995 | `		 * from-string interval serializes as the two names php writes. */` |
|    452 | 7996 | `		pOwn = SyHashGet(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName));` |
|    452 | 7997 | `		if( pOwn && (((VmClassAttr *)pOwn->pUserData)->iState & VM_CLASS_ATTR_UNSEEN) ){` |
|     37 | 7998 | `			continue;` |
|      - | 7999 | `		}` |
|    416 | 8000 | `		pVal = PH7_ClassInstanceFetchAttr(pThis,&pAttr->sName);` |
|    416 | 8001 | `		if( pVal == 0 ){` |
|     30 | 8002 | `			continue;` |
|      - | 8003 | `		}` |
|    388 | 8004 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    388 | 8005 | `		PH7_MemObjStringAppend(&sKey,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName));` |
|    388 | 8006 | `		ph7_array_add_elem(pOut,&sKey,pVal);` |
|    388 | 8007 | `		PH7_MemObjRelease(&sKey);` |
|      2 | 8008 | `	}` |
|     25 | 8009 | `}` |
|      - | 8010 | `/*` |
|      - | 8011 | ` * php's presentation for the date classes (ph7_class::xPresent).` |
|      - | 8012 | ` *` |
|      - | 8013 | ` * php keeps a timelib struct and SHOWS date/timezone_type/timezone; PHL keeps a` |
|      - | 8014 | ` * timestamp, an offset, a zone name and microseconds, all hidden. These build php's` |
|      - | 8015 | ` * shape out of that state, so var_dump/print_r, var_export and the (array) cast` |
|      - | 8016 | ` * agree with the oracle without changing what the C bodies read.` |
|      - | 8017 | ` *` |
|      - | 8018 | ` * timezone_type is php's own three-way tag: 1 = a fixed UTC OFFSET ("+02:00"),` |
|      - | 8019 | ` * 2 = an ABBREVIATION ("GMT", "Z"), 3 = an IDENTIFIER ("UTC", "Europe/Paris").` |
|      - | 8020 | ` * PHL accepts offsets, UTC, GMT and Z today; the identifier arm is written for the` |
|      - | 8021 | ` * whole rule so a tz database can only add names, never change the tagging.` |
|      - | 8022 | ` */` |
|   1832 | 8023 | `static void DtPresentPut(ph7_vm *pVm,ph7_value *pOut,const char *zKey,ph7_value *pVal)` |
|      2 | 8024 | `{` |
|      - | 8025 | `	ph7_value sKey;` |
|   1834 | 8026 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|   1834 | 8027 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|   1834 | 8028 | `	ph7_array_add_elem(pOut,&sKey,pVal);` |
|   1834 | 8029 | `	PH7_MemObjRelease(&sKey);` |
|   1834 | 8030 | `}` |
|    694 | 8031 | `static void DtPresentZone(ph7_vm *pVm,ph7_value *pOut,const char *zName,int nName,int iKind)` |
|      2 | 8032 | `{` |
|      - | 8033 | `	ph7_value sVal;` |
|    696 | 8034 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,iKind);` |
|    696 | 8035 | `	DtPresentPut(&(*pVm),pOut,"timezone_type",&sVal);` |
|    696 | 8036 | `	PH7_MemObjRelease(&sVal);` |
|    696 | 8037 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,0);` |
|    696 | 8038 | `	PH7_MemObjStringAppend(&sVal,zName,(sxu32)nName);` |
|    696 | 8039 | `	DtPresentPut(&(*pVm),pOut,"timezone",&sVal);` |
|    696 | 8040 | `	PH7_MemObjRelease(&sVal);` |
|    696 | 8041 | `}` |
|      - | 8042 | `/*` |
|      - | 8043 | ` * php builds this shape FROM the struct the constructor allocates, so an object` |
|      - | 8044 | ` * that has none contributes nothing to it -- var_dump, print_r, var_export, the` |
|      - | 8045 | ` * (array) cast and json_encode all answer an empty shape where PHL published a` |
|      - | 8046 | ` * 1970 date nothing had asked for.` |
|      - | 8047 | ` */` |
|    464 | 8048 | `static void DtDateTimeShape(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|      2 | 8049 | `{` |
|      - | 8050 | `	dt_state sState;` |
|      - | 8051 | `	Sytm sTm;` |
|      - | 8052 | `	char zZone[64];` |
|      - | 8053 | `	char zDate[64];` |
|      - | 8054 | `	ph7_value sVal;` |
|      - | 8055 | `	int nName;` |
|    466 | 8056 | `	if( !DtIsInit(pThis) ){` |
|     21 | 8057 | `		return;` |
|      - | 8058 | `	}` |
|    446 | 8059 | `	DtLoad(pThis,&sState);` |
|    446 | 8060 | `	nName = sState.nName;` |
|    446 | 8061 | `	if( nName >= (int)sizeof(zZone) ){` |
|    ! 0 | 8062 | `		nName = (int)sizeof(zZone) - 1;` |
|    ! 0 | 8063 | `	}` |
|    446 | 8064 | `	if( nName > 0 ){` |
|    446 | 8065 | `		SyMemcpy(sState.zName,zZone,(sxu32)nName);` |
|    222 | 8066 | `	}` |
|    446 | 8067 | `	zZone[nName] = 0;` |
|    446 | 8068 | `	DtFillSytm(sState.iTs,sState.iOff,zZone,&sTm);` |
|      - | 8069 | `	/* php's fixed shape here, not a format string: "Y-m-d H:i:s.uuuuuu". */` |
|    668 | 8070 | `	SyBufferFormat(zDate,sizeof(zDate),"%04qd-%02d-%02d %02d:%02d:%02d.%06d",` |
|    444 | 8071 | `		sTm.tm_year,sTm.tm_mon + 1,sTm.tm_mday,sTm.tm_hour,sTm.tm_min,sTm.tm_sec,` |
|    222 | 8072 | `		sState.uSec);` |
|    446 | 8073 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,0);` |
|    446 | 8074 | `	PH7_MemObjStringAppend(&sVal,zDate,(sxu32)SyStrlen(zDate));` |
|    446 | 8075 | `	DtPresentPut(&(*pVm),pOut,"date",&sVal);` |
|    446 | 8076 | `	PH7_MemObjRelease(&sVal);` |
|    446 | 8077 | `	DtPresentZone(&(*pVm),pOut,zZone,nName,sState.iZoneKind);` |
|    234 | 8078 | `}` |
|      - | 8079 | `/*` |
|      - | 8080 | ` * The hook itself: php's get_properties starts from the object's OWN table and` |
|      - | 8081 | ` * writes the struct's keys into it, so a subclass's properties come FIRST and a` |
|      - | 8082 | `` * subclass property named `date` keeps its position while taking the internal`` |
|      - | 8083 | ` * value. PHL built the three keys alone, so every property a subclass declared was` |
|      - | 8084 | ` * missing from var_dump, var_export, the (array) cast and json_encode.` |
|      - | 8085 | ` */` |
|    412 | 8086 | `static sxi32 DtPresentDateTime(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      2 | 8087 | `{` |
|    206 | 8088 | `	SXUNUSED(bDebug); /* php shows the same three keys to both handlers */` |
|    414 | 8089 | `	DtAddCommonProps(&(*pVm),pThis,pOut);` |
|    414 | 8090 | `	DtDateTimeShape(&(*pVm),pThis,pOut);` |
|    414 | 8091 | `	return SXRET_OK;` |
|      2 | 8092 | `}` |
|      - | 8093 | `/*` |
|      - | 8094 | ` * DateInterval and DatePeriod present their OWN property table -- php's state for` |
|      - | 8095 | ` * these two is the visible properties themselves -- so the hook exists for one` |
|      - | 8096 | ` * reason: an object that was never constructed has no table at all there, and` |
|      - | 8097 | ` * showed ten (or seven) default fields here. The initialized case is the slot walk` |
|      - | 8098 | ` * the cast already fell back to, so nothing else about them changes.` |
|      - | 8099 | ` */` |
|     86 | 8100 | `static sxi32 DtPresentProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      2 | 8101 | `{` |
|     43 | 8102 | `	SXUNUSED(bDebug);` |
|     88 | 8103 | `	if( DtIsInit(pThis) ){` |
|     58 | 8104 | `		PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pOut->x.pOther);` |
|     30 | 8105 | `	}else{` |
|      - | 8106 | `		/* No table there at all -- but a subclass's own properties are the` |
|      - | 8107 | `		 * object's, and php still shows those. */` |
|     31 | 8108 | `		DtAddCommonProps(&(*pVm),pThis,pOut);` |
|      - | 8109 | `	}` |
|     88 | 8110 | `	return SXRET_OK;` |
|      2 | 8111 | `}` |
|    258 | 8112 | `static void DtTimeZoneShape(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|      1 | 8113 | `{` |
|    259 | 8114 | `	const char *zName = 0;` |
|    259 | 8115 | `	int nName = 0;` |
|    259 | 8116 | `	if( !DtIsInit(pThis) ){` |
|      9 | 8117 | `		return;` |
|      - | 8118 | `	}` |
|    251 | 8119 | `	PH7_NativeAttrStr(pThis,DTZ_NAME,&zName,&nName);` |
|    251 | 8120 | `	DtPresentZone(&(*pVm),pOut,zName ? zName : "",nName,` |
|    125 | 8121 | `		DtZoneKindOf(pThis,DTZ_KIND,zName,nName));` |
|    130 | 8122 | `}` |
|    194 | 8123 | `static sxi32 DtPresentTimeZone(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      1 | 8124 | `{` |
|     97 | 8125 | `	SXUNUSED(bDebug);` |
|    195 | 8126 | `	DtAddCommonProps(&(*pVm),pThis,pOut);   /* see DtPresentDateTime */` |
|    195 | 8127 | `	DtTimeZoneShape(&(*pVm),pThis,pOut);` |
|    195 | 8128 | `	return SXRET_OK;` |
|      1 | 8129 | `}` |
|      - | 8130 | `/*` |
|      - | 8131 | ` * ---------------------------------------------------------------------------` |
|      - | 8132 | ` * php's serialization pair for the three date classes whose state is HIDDEN.` |
|      - | 8133 | ` *` |
|      - | 8134 | ` * serialize() had been walking the engine slots, so a DateTime round-tripped as` |
|      - | 8135 | `` * `__dtTs`/`__dtOff`/`__dtName`/`__dtUs` and a payload php WROTE could not be read`` |
|      - | 8136 | `` * back at all -- `unserialize('O:8:"DateTime":3:{s:4:"date";…}')` found none of the`` |
|      - | 8137 | ` * names it wanted, silently kept the 1970 defaults and answered a valid object with` |
|      - | 8138 | ` * the wrong instant. php's answer is not a hidden-slot rule but a pair of methods:` |
|      - | 8139 | ` * __serialize() hands back the PRESENTED shape (date/timezone_type/timezone, the` |
|      - | 8140 | ` * same hash date_object_get_properties_for builds) and __unserialize() re-parses it,` |
|      - | 8141 | ` * so the payload is the class's public model rather than its storage.` |
|      - | 8142 | ` *` |
|      - | 8143 | ` * The four methods php declares are all here, because they are one contract:` |
|      - | 8144 | ` * __serialize/__unserialize is what serialize() uses, __wakeup reads a LEGACY` |
|      - | 8145 | ` * payload out of the object's own properties, and __set_state is what var_export's` |
|      - | 8146 | `` * `\DateTime::__set_state(array(…))` text evaluates to. All four fail with the same`` |
|      - | 8147 | `` * plain `Error`, and php's sentence for it names the class.`` |
|      - | 8148 | ` * ---------------------------------------------------------------------------` |
|      - | 8149 | ` */` |
|      - | 8150 | `/*` |
|      - | 8151 | ` * ---------------------------------------------------------------------------` |
|      - | 8152 | ` * How the date classes COMPARE (ph7_class::xCmp -- php's compare handlers).` |
|      - | 8153 | ` *` |
|      - | 8154 | ` * php compares a date object by what it MEANS, not by what it stores, and the` |
|      - | 8155 | ` * property walk PHL fell back to disagreed with every one of them: a DateTime` |
|      - | 8156 | ` * and a DateTimeImmutable of the same instant were unequal here because the` |
|      - | 8157 | ` * classes differ, two dates one second apart in different zones were unequal` |
|      - | 8158 | `` * because the zone NAME is a property, two `P1D` intervals were equal where php`` |
|      - | 8159 | ` * refuses to compare intervals at all, and two DateTimeZones ordered by name` |
|      - | 8160 | ` * where php refuses to compare different KINDS of zone.` |
|      - | 8161 | ` *` |
|      - | 8162 | ` * All three handlers screen their partner by INSTANCE OF, not by class` |
|      - | 8163 | ` * identity: a subclass of DateTime still compares as an instant (and its extra` |
|      - | 8164 | ` * properties are invisible to the comparison, since php's date handler never` |
|      - | 8165 | ` * looks at properties), while anything that is not a date at all is php's` |
|      - | 8166 | ` * ZEND_UNCOMPARABLE -- the 1-from-both-sides the caller defaults to.` |
|      - | 8167 | ` * ---------------------------------------------------------------------------` |
|      - | 8168 | ` */` |
|      - | 8169 | `/*` |
|      - | 8170 | ` * DateTime / DateTimeImmutable: php's date_object_compare_date, which is` |
|      - | 8171 | ` * timelib_time_compare on the two INSTANTS -- the epoch second first, the` |
|      - | 8172 | ` * microseconds to break a tie. The zone is not part of it (php compares the` |
|      - | 8173 | ` * instant the two name, so 00:00 UTC equals 01:00+01:00), and neither is any` |
|      - | 8174 | ` * property, declared or dynamic.` |
|      - | 8175 | ` */` |
|     48 | 8176 | `static void DtCmpDateTime(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)` |
|      2 | 8177 | `{` |
|      - | 8178 | `	dt_state sL,sR;` |
|     48 | 8179 | `	if( !DtIsA(&(*pVm),pThis,"DateTimeInterface")` |
|     50 | 8180 | `	 \|\| !DtIsA(&(*pVm),pCtx->pOther,"DateTimeInterface") ){` |
|     20 | 8181 | `		return;   /* uncomparable, which is what the caller pre-loaded */` |
|      - | 8182 | `	}` |
|     40 | 8183 | `	if( !DtIsInit(pThis) \|\| !DtIsInit(pCtx->pOther) ){` |
|      - | 8184 | `		/* An unconstructed date has no instant to compare, and php refuses from` |
|      - | 8185 | ``		 * EITHER side -- so `$fresh == $uninitialized` raises as well. The screen`` |
|      - | 8186 | `		 * sits below the both-are-dates one on purpose: a date against something` |
|      - | 8187 | `		 * that is not one stays php's silent uncomparable. */` |
|     17 | 8188 | `		pCtx->zThrowClass = "DateObjectError";` |
|     17 | 8189 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 8190 | `			"Trying to compare an incomplete DateTime or DateTimeImmutable object");` |
|     17 | 8191 | `		return;` |
|      - | 8192 | `	}` |
|     24 | 8193 | `	DtLoad(pThis,&sL);` |
|     24 | 8194 | `	DtLoad(pCtx->pOther,&sR);` |
|     24 | 8195 | `	if( sL.iTs != sR.iTs ){` |
|      5 | 8196 | `		pCtx->iResult = sL.iTs < sR.iTs ? -1 : 1;` |
|     22 | 8197 | `	}else if( sL.uSec != sR.uSec ){` |
|      5 | 8198 | `		pCtx->iResult = sL.uSec < sR.uSec ? -1 : 1;` |
|      3 | 8199 | `	}else{` |
|     16 | 8200 | `		pCtx->iResult = 0;` |
|      - | 8201 | `	}` |
|     26 | 8202 | `}` |
|      - | 8203 | `/*` |
|      - | 8204 | ` * DateInterval: php refuses. Two intervals carry no common unit -- a month is` |
|      - | 8205 | ` * not a fixed number of days -- so php's handler answers ZEND_UNCOMPARABLE` |
|      - | 8206 | `` * behind an E_WARNING for every pair, `P1D` against `P1D` included. The one`` |
|      - | 8207 | `` * comparison that succeeds is `$i == $i`, and that never reaches a handler:`` |
|      - | 8208 | ` * zend's identity shortcut answers it first (and PH7_ClassInstanceCmp's does` |
|      - | 8209 | ` * too, above this call).` |
|      - | 8210 | ` *` |
|      - | 8211 | ` * The warning is emitted from inside the comparator on purpose -- php emits it` |
|      - | 8212 | ` * from inside the handler, so a sort() over intervals warns once per COMPARISON` |
|      - | 8213 | ` * there as it does here.` |
|      - | 8214 | ` */` |
|     18 | 8215 | `static void DtCmpInterval(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)` |
|      1 | 8216 | `{` |
|     18 | 8217 | `	if( !DtIsA(&(*pVm),pThis,"DateInterval")` |
|     19 | 8218 | `	 \|\| !DtIsA(&(*pVm),pCtx->pOther,"DateInterval") ){` |
|      3 | 8219 | `		return;   /* an interval against something else: uncomparable, and silent */` |
|      - | 8220 | `	}` |
|     17 | 8221 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Cannot compare DateInterval objects");` |
|     17 | 8222 | `	pCtx->iResult = 1;` |
|     10 | 8223 | `}` |
|      - | 8224 | `/*` |
|      - | 8225 | ` * DateTimeZone: php compares two zones of the SAME kind and refuses two of` |
|      - | 8226 | ` * different kinds outright (a DateException raised from inside the comparison).` |
|      - | 8227 | ` * Same-kind zones answer 0 or php's uncomparable 1 and never an ordering, so` |
|      - | 8228 | `` * `+01:00 < +02:00` is false there: an OFFSET zone is compared by its offset`` |
|      - | 8229 | `` * (`+0100` and `+01:00` are one zone), an abbreviation and an identifier by`` |
|      - | 8230 | ` * their normalized names.` |
|      - | 8231 | ` */` |
|     68 | 8232 | `static void DtCmpTimeZone(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)` |
|      1 | 8233 | `{` |
|     69 | 8234 | `	const char *zL = 0,*zR = 0;` |
|     69 | 8235 | `	int nL = 0,nR = 0;` |
|      - | 8236 | `	int iKindL,iKindR;` |
|     68 | 8237 | `	if( !DtIsA(&(*pVm),pThis,"DateTimeZone")` |
|     69 | 8238 | `	 \|\| !DtIsA(&(*pVm),pCtx->pOther,"DateTimeZone") ){` |
|     30 | 8239 | `		return;` |
|      - | 8240 | `	}` |
|     65 | 8241 | `	if( !DtIsInit(pThis) \|\| !DtIsInit(pCtx->pOther) ){` |
|      - | 8242 | `		/* php's own sentence for the zone half, and its own class: a DateException` |
|      - | 8243 | `		 * carries the KIND mismatch below, an unconstructed operand a` |
|      - | 8244 | `		 * DateObjectError. */` |
|      7 | 8245 | `		pCtx->zThrowClass = "DateObjectError";` |
|      7 | 8246 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 8247 | `			"Trying to compare uninitialized DateTimeZone objects");` |
|      7 | 8248 | `		return;` |
|      - | 8249 | `	}` |
|     59 | 8250 | `	PH7_NativeAttrStr(pThis,DTZ_NAME,&zL,&nL);` |
|     59 | 8251 | `	PH7_NativeAttrStr(pCtx->pOther,DTZ_NAME,&zR,&nR);` |
|     59 | 8252 | `	iKindL = DtZoneKindOf(pThis,DTZ_KIND,zL,nL);` |
|     59 | 8253 | `	iKindR = DtZoneKindOf(pCtx->pOther,DTZ_KIND,zR,nR);` |
|     59 | 8254 | `	if( iKindL != iKindR ){` |
|     31 | 8255 | `		pCtx->zThrowClass = "DateException";` |
|     31 | 8256 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 8257 | `			"Cannot compare two different kinds of DateTimeZone objects");` |
|     31 | 8258 | `		return;   /* the uncomparable 1 stands in until the refusal is raised */` |
|      - | 8259 | `	}` |
|     29 | 8260 | `	if( iKindL == DT_ZONE_OFFSET ){` |
|     22 | 8261 | `		pCtx->iResult = PH7_NativeAttrInt(pThis,DTZ_OFF)` |
|     14 | 8262 | `		              == PH7_NativeAttrInt(pCtx->pOther,DTZ_OFF) ? 0 : 1;` |
|     15 | 8263 | `		return;` |
|      - | 8264 | `	}` |
|     21 | 8265 | `	pCtx->iResult = (nL == nR && (nL == 0 \|\| SyMemcmp(zL,zR,(sxu32)nL) == 0)) ? 0 : 1;` |
|     35 | 8266 | `}` |
|      - | 8267 | `/* Build a payload array: the class's presented shape, then its own visible slots. */` |
|    116 | 8268 | `static int DtSerializePayload(ph7_context *pCtx,ph7_class_instance *pThis,int bZoneOnly,` |
|      - | 8269 | `	ph7_value *pOut)` |
|      1 | 8270 | `{` |
|    117 | 8271 | `	PH7_MemObjInit(pCtx->pVm,pOut);` |
|    117 | 8272 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|    ! 0 | 8273 | `		PH7_MemObjRelease(pOut);` |
|    ! 0 | 8274 | `		return -1;` |
|      - | 8275 | `	}` |
|      - | 8276 | `	/* The SHAPE first here, the object's own properties behind it: php's` |
|      - | 8277 | `	 * __serialize builds a fresh array from the struct and calls` |
|      - | 8278 | `	 * add_common_properties on the END of it, which is the opposite order from the` |
|      - | 8279 | `	 * presentation above (where the object's table is what the struct writes` |
|      - | 8280 | `	 * into). serialize() and var_dump therefore disagree about where a subclass's` |
|      - | 8281 | `	 * property sits, in both engines. */` |
|    117 | 8282 | `	if( bZoneOnly ){` |
|     65 | 8283 | `		DtTimeZoneShape(pCtx->pVm,pThis,pOut);` |
|     33 | 8284 | `	}else{` |
|     53 | 8285 | `		DtDateTimeShape(pCtx->pVm,pThis,pOut);` |
|      - | 8286 | `	}` |
|    117 | 8287 | `	DtAddCommonProps(pCtx->pVm,pThis,pOut);` |
|    117 | 8288 | `	return 0;` |
|     59 | 8289 | `}` |
|      - | 8290 | ``/* php's `Error: Invalid serialization data for <Class> object`, the one refusal all`` |
|      - | 8291 | ` * four methods share. Named for the DECLARING class, not the receiver's. */` |
|     42 | 8292 | `static int DtSerialError(ph7_context *pCtx,const char *zClass)` |
|      1 | 8293 | `{` |
|     64 | 8294 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     21 | 8295 | `		"Invalid serialization data for %s object",zClass);` |
|      1 | 8296 | `}` |
|      - | 8297 | ``/* The `array $data` parameter's own screen: the shared ZPP does not judge a scalar`` |
|      - | 8298 | `` * against a bare `array` (a recorded gap), so each caller words php's TypeError. */`` |
|    142 | 8299 | `static int DtCheckDataArg(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zClass)` |
|      1 | 8300 | `{` |
|      - | 8301 | `	char zBuf[64];` |
|    143 | 8302 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    143 | 8303 | `		return 0;` |
|      - | 8304 | `	}` |
|    ! 0 | 8305 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 8306 | `		"%s::__unserialize(): Argument #1 ($data) must be of type array, %s given",` |
|    ! 0 | 8307 | `		zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|    ! 0 | 8308 | `	return -1;` |
|     72 | 8309 | `}` |
|      - | 8310 | `/*` |
|      - | 8311 | `` * php's php_date_timezone_initialize_from_hash(): `timezone_type` must be an int in`` |
|      - | 8312 | `` * 1..3 and `timezone` a string, and then the NAME alone rebuilds the zone -- the tag`` |
|      - | 8313 | ` * is validated but never trusted, which is why a payload tagged 1 whose name is` |
|      - | 8314 | ` * "UTC" restores a UTC zone rather than an offset one. Answers 0 on success.` |
|      - | 8315 | ` */` |
|     46 | 8316 | `static int DtZoneRestore(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 8317 | `{` |
|      - | 8318 | `	ph7_value *pType,*pName;` |
|      - | 8319 | `	const char *zTz,*zName;` |
|     47 | 8320 | `	int nTz,nName,iKind = DT_ZONE_ID;` |
|     47 | 8321 | `	sxi32 iOff = 0;` |
|      - | 8322 | `	sxi64 iType;` |
|      - | 8323 | `	char zBuf[16];` |
|     47 | 8324 | `	pType = ph7_array_fetch(pData,"timezone_type",(int)sizeof("timezone_type")-1);` |
|     47 | 8325 | `	if( pType == 0 \|\| (pType->iFlags & MEMOBJ_INT) == 0 ){` |
|      5 | 8326 | `		return -1;` |
|      - | 8327 | `	}` |
|     43 | 8328 | `	iType = pType->x.iVal;` |
|     43 | 8329 | `	if( iType < 1 \|\| iType > 3 ){` |
|      3 | 8330 | `		return -1;` |
|      - | 8331 | `	}` |
|     41 | 8332 | `	pName = ph7_array_fetch(pData,"timezone",(int)sizeof("timezone")-1);` |
|     41 | 8333 | `	if( pName == 0 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 8334 | `		return -1;` |
|      - | 8335 | `	}` |
|     41 | 8336 | `	zTz = (const char *)SyBlobData(&pName->sBlob);` |
|     41 | 8337 | `	nTz = (int)SyBlobLength(&pName->sBlob);` |
|     41 | 8338 | `	if( DtZoneParse(zTz,nTz,&iOff,&zName,&nName,&iKind,zBuf,sizeof(zBuf)) != 0 ){` |
|    ! 0 | 8339 | `		return -1;` |
|      - | 8340 | `	}` |
|     41 | 8341 | `	PH7_NativeSetAttrInt(&(*pVm),pThis,DTZ_OFF,iOff);` |
|     41 | 8342 | `	PH7_NativeSetAttrStr(&(*pVm),pThis,DTZ_NAME,zName,nName);` |
|      - | 8343 | ``	/* The payload's own `timezone_type` is NOT read back: php re-derives the kind`` |
|      - | 8344 | `	 * from the name here too, which is what turns a serialized type-2 "UTC" into a` |
|      - | 8345 | `	 * type 3 on the way in. */` |
|     41 | 8346 | `	PH7_NativeSetAttrInt(&(*pVm),pThis,DTZ_KIND,iKind);` |
|     41 | 8347 | `	return 0;` |
|     24 | 8348 | `}` |
|      - | 8349 | `/*` |
|      - | 8350 | `` * php's php_date_initialize_from_hash(): `date`, `timezone_type` and `timezone` must`` |
|      - | 8351 | ` * all be present and well-typed, and the tag must be one php writes.` |
|      - | 8352 | ` *` |
|      - | 8353 | ` * php restores an OFFSET or ABBREVIATION payload by CONCATENATING the two and running` |
|      - | 8354 | ` * its ordinary parser over "<date> <timezone>", and an IDENTIFIER one by resolving the` |
|      - | 8355 | ` * name first. Resolving the name for all three is the same answer here and does not` |
|      - | 8356 | `` * lean on the parser: `date` is always php's own `x-m-d H:i:s.u`, which carries no`` |
|      - | 8357 | ` * zone of its own, so nothing is left for the concatenated text to decide. It is also` |
|      - | 8358 | ` * the only spelling that works today -- PHL's parser accepts an offset only when it is` |
|      - | 8359 | ` * ATTACHED to the time ("…07+02:30", never "…07 +02:30") and accepts no trailing zone` |
|      - | 8360 | ` * NAME at all, so php's own round-trip string does not parse here (a scope-policy gap of its` |
|      - | 8361 | ` * own, recorded rather than worked around).` |
|      - | 8362 | ` *` |
|      - | 8363 | ` * Reading the NAME rather than the tag is also what php ends up doing: a payload` |
|      - | 8364 | ` * tagged 1 whose timezone is "UTC" restores a UTC zone in both engines.` |
|      - | 8365 | ` * Answers 0 on success.` |
|      - | 8366 | ` */` |
|     60 | 8367 | `static int DtDateRestore(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 8368 | `{` |
|     61 | 8369 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 8370 | `	ph7_value *pDate,*pType,*pName;` |
|      - | 8371 | `	const char *zDate,*zTz,*zZone,*zErr;` |
|     61 | 8372 | `	int nDate,nTz,nZone,iPos,iKind = DT_ZONE_ID;` |
|     61 | 8373 | `	sxi32 iOff = 0;` |
|      - | 8374 | `	sxi64 iType;` |
|      - | 8375 | `	dt_state sState;` |
|      - | 8376 | `	char zNameBuf[16],zZoneBuf[16],cAt;` |
|     61 | 8377 | `	pDate = ph7_array_fetch(pData,"date",(int)sizeof("date")-1);` |
|     61 | 8378 | `	if( pDate == 0 \|\| (pDate->iFlags & MEMOBJ_STRING) == 0 ){` |
|      9 | 8379 | `		return -1;` |
|      - | 8380 | `	}` |
|     53 | 8381 | `	pType = ph7_array_fetch(pData,"timezone_type",(int)sizeof("timezone_type")-1);` |
|     53 | 8382 | `	if( pType == 0 \|\| (pType->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 8383 | `		return -1;` |
|      - | 8384 | `	}` |
|     53 | 8385 | `	pName = ph7_array_fetch(pData,"timezone",(int)sizeof("timezone")-1);` |
|     53 | 8386 | `	if( pName == 0 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 8387 | `		return -1;` |
|      - | 8388 | `	}` |
|     53 | 8389 | `	zDate = (const char *)SyBlobData(&pDate->sBlob);` |
|     53 | 8390 | `	nDate = (int)SyBlobLength(&pDate->sBlob);` |
|     53 | 8391 | `	zTz   = (const char *)SyBlobData(&pName->sBlob);` |
|     53 | 8392 | `	nTz   = (int)SyBlobLength(&pName->sBlob);` |
|     53 | 8393 | `	iType = pType->x.iVal;` |
|     53 | 8394 | `	if( iType < 1 \|\| iType > 3 ){` |
|    ! 0 | 8395 | `		return -1;` |
|      - | 8396 | `	}` |
|     53 | 8397 | `	if( DtZoneParse(zTz,nTz,&iOff,&zZone,&nZone,&iKind,zZoneBuf,sizeof(zZoneBuf)) != 0 ){` |
|      3 | 8398 | `		return -1;` |
|      - | 8399 | `	}` |
|     51 | 8400 | `	if( (iOff < 0 ? -iOff : iOff) / 3600 > 24 ){` |
|      - | 8401 | `		/* php reads the payload's zone back through its DATE-STRING grammar, whose` |
|      - | 8402 | `		 * offsets stop at hour 24 -- narrower than the zone constructor's 99. So a` |
|      - | 8403 | ``		 * DateTime carrying `+25:00` serializes there and refuses to come back,`` |
|      - | 8404 | `		 * while the DateTimeZone alone round-trips. Only the HOUR field is bounded:` |
|      - | 8405 | ``		 * `+24:59` and `+24:00:01` are both fine. */`` |
|      7 | 8406 | `		return -1;` |
|      - | 8407 | `	}` |
|     44 | 8408 | `	if( DtInitState(pCtx,zDate,nDate,iOff,zZone,nZone,iKind,&sState,zNameBuf,` |
|     23 | 8409 | `		sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0 ){` |
|    ! 0 | 8410 | `		return -1;` |
|      - | 8411 | `	}` |
|     45 | 8412 | `	DtStore(pVm,pThis,&sState);` |
|     45 | 8413 | `	return 0;` |
|     31 | 8414 | `}` |
|      - | 8415 | `/*` |
|      - | 8416 | ` * php's restore_custom_datetime_properties(): every payload key that is not part of` |
|      - | 8417 | ` * the internal shape becomes a property of the object. A REFERENCE is skipped, which` |
|      - | 8418 | ` * PHL cannot receive here (the pairs arrive already dereferenced).` |
|      - | 8419 | ` */` |
|      - | 8420 | `typedef struct dt_restore_ctx dt_restore_ctx;` |
|      - | 8421 | `struct dt_restore_ctx` |
|      - | 8422 | `{` |
|      - | 8423 | `	ph7_class_instance *pThis;` |
|      - | 8424 | `	int bZoneOnly;` |
|      - | 8425 | `};` |
|    168 | 8426 | `static int DtRestoreWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 | 8427 | `{` |
|    169 | 8428 | `	dt_restore_ctx *pRes = (dt_restore_ctx *)pUserData;` |
|      - | 8429 | `	const char *zKey;` |
|      - | 8430 | `	int nKey;` |
|    169 | 8431 | `	if( !ph7_value_is_string(pKey) ){` |
|    ! 0 | 8432 | `		return PH7_OK;` |
|      - | 8433 | `	}` |
|    169 | 8434 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|    168 | 8435 | `	if( (nKey == 13 && SyMemcmp(zKey,"timezone_type",13) == 0)` |
|    126 | 8436 | `	 \|\| (nKey == 8  && SyMemcmp(zKey,"timezone",8) == 0)` |
|     69 | 8437 | `	 \|\| (!pRes->bZoneOnly && nKey == 4 && SyMemcmp(zKey,"date",4) == 0) ){` |
|    213 | 8438 | `		return PH7_OK;` |
|      - | 8439 | `	}` |
|      - | 8440 | `	/* A name the class does not DECLARE is dropped, which is what the engine's own` |
|      - | 8441 | `	 * unserialize does with one: PHL has no dynamic properties, where php creates` |
|      - | 8442 | `	 * (and deprecates) them. */` |
|     47 | 8443 | `	PH7_NativeSetProp(pRes->pThis->pVm,pRes->pThis,zKey,(sxu32)nKey,pVal);` |
|     47 | 8444 | `	return PH7_OK;` |
|    108 | 8445 | `}` |
|     84 | 8446 | `static void DtRestoreCustomProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData,` |
|      - | 8447 | `	int bZoneOnly)` |
|      1 | 8448 | `{` |
|      - | 8449 | `	dt_restore_ctx sRes;` |
|     42 | 8450 | `	SXUNUSED(pVm);` |
|     85 | 8451 | `	if( (pData->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 8452 | `		return;` |
|      - | 8453 | `	}` |
|     85 | 8454 | `	sRes.pThis = pThis;` |
|     85 | 8455 | `	sRes.bZoneOnly = bZoneOnly;` |
|     85 | 8456 | `	ph7_array_walk(pData,DtRestoreWalk,&sRes);` |
|     43 | 8457 | `}` |
|      - | 8458 | `/* DateTimeZone::__serialize() / DateTime\|DateTimeImmutable::__serialize() */` |
|    120 | 8459 | `static int DtSerializeMagic(ph7_context *pCtx,int bZoneOnly)` |
|      1 | 8460 | `{` |
|    121 | 8461 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 8462 | `	ph7_value sOut;` |
|    121 | 8463 | `	if( pThis == 0 ){` |
|      5 | 8464 | `		return PH7_OK;` |
|      - | 8465 | `	}` |
|    117 | 8466 | `	if( DtSerializePayload(pCtx,pThis,bZoneOnly,&sOut) != 0 ){` |
|    ! 0 | 8467 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8468 | `	}` |
|    117 | 8469 | `	ph7_result_value(pCtx,&sOut);` |
|    117 | 8470 | `	PH7_MemObjRelease(&sOut);` |
|    117 | 8471 | `	return PH7_OK;` |
|     61 | 8472 | `}` |
|     66 | 8473 | `static int vm_builtin_DateTimeZone_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8474 | `{` |
|     33 | 8475 | `	SXUNUSED(nArg);` |
|     33 | 8476 | `	SXUNUSED(apArg);` |
|     67 | 8477 | `	return DtSerializeMagic(pCtx,1);` |
|      1 | 8478 | `}` |
|     54 | 8479 | `static int vm_builtin_DateTime_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8480 | `{` |
|     27 | 8481 | `	SXUNUSED(nArg);` |
|     27 | 8482 | `	SXUNUSED(apArg);` |
|     55 | 8483 | `	return DtSerializeMagic(pCtx,0);` |
|      1 | 8484 | `}` |
|      - | 8485 | `/* __unserialize(array $data): restore the state, then the subclass's own slots. */` |
|     88 | 8486 | `static int DtUnserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg,int bZoneOnly,` |
|      - | 8487 | `	const char *zClass)` |
|      1 | 8488 | `{` |
|      - | 8489 | `	/* Raw: this is a door that INITIALIZES -- unserialize() calls it on an object` |
|      - | 8490 | `	 * the engine built without a constructor, which is the whole point of it. */` |
|     89 | 8491 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|      - | 8492 | `	int rc;` |
|     89 | 8493 | `	if( pThis == 0 ){` |
|    ! 0 | 8494 | `		return PH7_OK;` |
|      - | 8495 | `	}` |
|     89 | 8496 | `	if( DtCheckDataArg(pCtx,nArg,apArg,zClass) != 0 ){` |
|    ! 0 | 8497 | `		return PH7_EXCEPTION;` |
|      - | 8498 | `	}` |
|     66 | 8499 | `	rc = bZoneOnly ? DtZoneRestore(pCtx->pVm,pThis,apArg[0])` |
|     67 | 8500 | `	               : DtDateRestore(pCtx,pThis,apArg[0]);` |
|     89 | 8501 | `	if( rc != 0 ){` |
|     15 | 8502 | `		return DtSerialError(pCtx,zClass);` |
|      - | 8503 | `	}` |
|     75 | 8504 | `	DtRestoreCustomProps(pCtx->pVm,pThis,apArg[0],bZoneOnly);` |
|     75 | 8505 | `	DtSetInit(pCtx->pVm,pThis);` |
|     75 | 8506 | `	return PH7_OK;` |
|     45 | 8507 | `}` |
|     42 | 8508 | `static int vm_builtin_DateTimeZone_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8509 | `{` |
|     43 | 8510 | `	return DtUnserializeMagic(pCtx,nArg,apArg,1,"DateTimeZone");` |
|      1 | 8511 | `}` |
|     46 | 8512 | `static int vm_builtin_DateTime_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8513 | `{` |
|     47 | 8514 | `	return DtUnserializeMagic(pCtx,nArg,apArg,0,"DateTime");` |
|      1 | 8515 | `}` |
|    ! 0 | 8516 | `static int vm_builtin_DateTimeImmutable_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 8517 | `{` |
|    ! 0 | 8518 | `	return DtUnserializeMagic(pCtx,nArg,apArg,0,"DateTimeImmutable");` |
|    ! 0 | 8519 | `}` |
|      - | 8520 | `/*` |
|      - | 8521 | ` * __wakeup(): the LEGACY payload, whose pairs the engine wrote into the object's own` |
|      - | 8522 | ` * properties before calling this. php reads Z_OBJPROP and restores from it, so an` |
|      - | 8523 | `` * object that has no such properties -- a plain `new DateTime` -- is exactly the`` |
|      - | 8524 | ` * failure case, and php raises the same Error there.` |
|      - | 8525 | ` */` |
|      6 | 8526 | `static int DtWakeupMagic(ph7_context *pCtx,int bZoneOnly,const char *zClass)` |
|      1 | 8527 | `{` |
|      - | 8528 | `	/* Raw, for the reason __unserialize() is: php reads the object's own properties` |
|      - | 8529 | ``	 * here and raises `Invalid serialization data` when they do not describe a`` |
|      - | 8530 | `	 * date -- which is what an unconstructed object's empty set does. */` |
|      7 | 8531 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|      - | 8532 | `	ph7_value sProps;` |
|      - | 8533 | `	int rc;` |
|      7 | 8534 | `	if( pThis == 0 ){` |
|    ! 0 | 8535 | `		return PH7_OK;` |
|      - | 8536 | `	}` |
|      7 | 8537 | `	PH7_MemObjInit(pCtx->pVm,&sProps);` |
|      7 | 8538 | `	if( PH7_MemObjToHashmap(&sProps) != SXRET_OK ){` |
|    ! 0 | 8539 | `		PH7_MemObjRelease(&sProps);` |
|    ! 0 | 8540 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8541 | `	}` |
|      7 | 8542 | `	DtAddCommonProps(pCtx->pVm,pThis,&sProps);` |
|      5 | 8543 | `	rc = bZoneOnly ? DtZoneRestore(pCtx->pVm,pThis,&sProps)` |
|      5 | 8544 | `	               : DtDateRestore(pCtx,pThis,&sProps);` |
|      7 | 8545 | `	PH7_MemObjRelease(&sProps);` |
|      7 | 8546 | `	if( rc != 0 ){` |
|      7 | 8547 | `		return DtSerialError(pCtx,zClass);` |
|      - | 8548 | `	}` |
|    ! 0 | 8549 | `	DtSetInit(pCtx->pVm,pThis);` |
|    ! 0 | 8550 | `	return PH7_OK;` |
|      4 | 8551 | `}` |
|      2 | 8552 | `static int vm_builtin_DateTimeZone_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8553 | `{` |
|      1 | 8554 | `	SXUNUSED(nArg);` |
|      1 | 8555 | `	SXUNUSED(apArg);` |
|      3 | 8556 | `	return DtWakeupMagic(pCtx,1,"DateTimeZone");` |
|      1 | 8557 | `}` |
|      4 | 8558 | `static int vm_builtin_DateTime_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8559 | `{` |
|      2 | 8560 | `	SXUNUSED(nArg);` |
|      2 | 8561 | `	SXUNUSED(apArg);` |
|      5 | 8562 | `	return DtWakeupMagic(pCtx,0,"DateTime");` |
|      1 | 8563 | `}` |
|    ! 0 | 8564 | `static int vm_builtin_DateTimeImmutable_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 8565 | `{` |
|    ! 0 | 8566 | `	SXUNUSED(nArg);` |
|    ! 0 | 8567 | `	SXUNUSED(apArg);` |
|    ! 0 | 8568 | `	return DtWakeupMagic(pCtx,0,"DateTimeImmutable");` |
|    ! 0 | 8569 | `}` |
|      - | 8570 | `/*` |
|      - | 8571 | ``  * __set_state(array $array): what var_export's `\DateTime::__set_state(array(…))` `` |
|      - | 8572 | ` * text evaluates to. php instantiates the class the method is DECLARED on and not` |
|      - | 8573 | `` * the called one -- `MyDateTime::__set_state(…)` answers a plain DateTime there --`` |
|      - | 8574 | ` * so this deliberately does not go through DtFactoryClass().` |
|      - | 8575 | ` */` |
|     12 | 8576 | `static int DtSetStateMagic(ph7_context *pCtx,int nArg,ph7_value **apArg,int bZoneOnly,` |
|      - | 8577 | `	const char *zClass)` |
|      1 | 8578 | `{` |
|     13 | 8579 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 | 8580 | `	ph7_class *pClass = DtClass(pVm,zClass);` |
|      - | 8581 | `	ph7_class_instance *pObj;` |
|      - | 8582 | `	int rc;` |
|     13 | 8583 | `	if( pClass == 0 ){` |
|    ! 0 | 8584 | `		return PH7_OK;` |
|      - | 8585 | `	}` |
|     13 | 8586 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - | 8587 | `		char zBuf[64];` |
|    ! 0 | 8588 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 8589 | `			"%s::__set_state(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 | 8590 | `			zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|      - | 8591 | `	}` |
|     13 | 8592 | `	pObj = DtNewInstance(pVm,pClass);` |
|     13 | 8593 | `	if( pObj == 0 ){` |
|    ! 0 | 8594 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8595 | `	}` |
|      8 | 8596 | `	rc = bZoneOnly ? DtZoneRestore(pVm,pObj,apArg[0])` |
|     11 | 8597 | `	               : DtDateRestore(pCtx,pObj,apArg[0]);` |
|     13 | 8598 | `	if( rc != 0 ){` |
|      3 | 8599 | `		PH7_ClassInstanceUnref(pObj);` |
|      3 | 8600 | `		return DtSerialError(pCtx,zClass);` |
|      - | 8601 | `	}` |
|     11 | 8602 | `	DtRestoreCustomProps(pVm,pObj,apArg[0],bZoneOnly);` |
|     11 | 8603 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     11 | 8604 | `	return PH7_OK;` |
|      7 | 8605 | `}` |
|      2 | 8606 | `static int vm_builtin_DateTimeZone_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8607 | `{` |
|      3 | 8608 | `	return DtSetStateMagic(pCtx,nArg,apArg,1,"DateTimeZone");` |
|      1 | 8609 | `}` |
|     10 | 8610 | `static int vm_builtin_DateTime_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8611 | `{` |
|     11 | 8612 | `	return DtSetStateMagic(pCtx,nArg,apArg,0,"DateTime");` |
|      1 | 8613 | `}` |
|    ! 0 | 8614 | `static int vm_builtin_DateTimeImmutable_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 8615 | `{` |
|    ! 0 | 8616 | `	return DtSetStateMagic(pCtx,nArg,apArg,0,"DateTimeImmutable");` |
|    ! 0 | 8617 | `}` |
|      - | 8618 | `/*` |
|      - | 8619 | ` * ---------------------------------------------------------------------------` |
|      - | 8620 | ` * php's serialization quartet for DateInterval and DatePeriod.` |
|      - | 8621 | ` *` |
|      - | 8622 | ` * Both classes declare __serialize/__unserialize/__wakeup/__set_state there and` |
|      - | 8623 | `` * neither declared any of them here, so `serialize()` walked the slots by luck`` |
|      - | 8624 | ` * (the bytes matched, because for these two classes the state IS the php-visible` |
|      - | 8625 | `` * property table), `var_export()`'s `\DateInterval::__set_state(array(...))` text`` |
|      - | 8626 | `` * evaluated to "Call to undefined method", and `serialize()` of an object that`` |
|      - | 8627 | ` * was never constructed answered a payload where php raises.` |
|      - | 8628 | ` *` |
|      - | 8629 | ` * The two RESTORE rules are not the same rule, and both are php's:` |
|      - | 8630 | ` *` |
|      - | 8631 | ` *   DateInterval reads each field on its own and fills a MISSING one with -1 --` |
|      - | 8632 | ``  *   timelib's "unset" marker, which is why `unserialize('O:12:"DateInterval":0:{}')` `` |
|      - | 8633 | `` *   is an interval of -1 years. `invert`, `f` and `from_string` are the three`` |
|      - | 8634 | ` *   exceptions, absent as 0, 0.0 and false. Nothing is refused.` |
|      - | 8635 | ` *` |
|      - | 8636 | ` *   DatePeriod refuses ANY payload that is not complete and well-typed: all seven` |
|      - | 8637 | ` *   keys, the three dates null or a DateTimeInterface, the interval a` |
|      - | 8638 | ` *   DateInterval, the count a real int and the two flags real bools -- one miss` |
|      - | 8639 | `` *   and it is php's `Invalid serialization data for DatePeriod object`.`` |
|      - | 8640 | ` * ---------------------------------------------------------------------------` |
|      - | 8641 | ` */` |
|      - | 8642 | `/* __serialize(): the object's own visible slots, mangled the way php mangles a` |
|      - | 8643 | ` * non-public one -- which is what a subclass's private property serializes as. */` |
|     52 | 8644 | `static int DtSerializeProps(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8645 | `{` |
|     54 | 8646 | `	ph7_class_instance *pThis = DtThis(pCtx);   /* php raises on an unconstructed one */` |
|      - | 8647 | `	ph7_value sOut;` |
|     26 | 8648 | `	SXUNUSED(nArg);` |
|     26 | 8649 | `	SXUNUSED(apArg);` |
|     54 | 8650 | `	if( pThis == 0 ){` |
|      7 | 8651 | `		return PH7_OK;` |
|      - | 8652 | `	}` |
|     48 | 8653 | `	PH7_MemObjInit(pCtx->pVm,&sOut);` |
|     48 | 8654 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|    ! 0 | 8655 | `		PH7_MemObjRelease(&sOut);` |
|    ! 0 | 8656 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8657 | `	}` |
|      - | 8658 | `	/* The class's own shape first and the object's additions behind it, which is` |
|      - | 8659 | `	 * php's order here and the opposite of the presentation's (where the struct` |
|      - | 8660 | `	 * writes into the object's table). */` |
|     71 | 8661 | `	DtAddNativeProps(pCtx->pVm,pThis,` |
|     46 | 8662 | `		DtIsA(pCtx->pVm,pThis,"DateInterval") ? "DateInterval" : "DatePeriod",&sOut);` |
|     48 | 8663 | `	DtAddCommonProps(pCtx->pVm,pThis,&sOut);` |
|     48 | 8664 | `	ph7_result_value(pCtx,&sOut);` |
|     48 | 8665 | `	PH7_MemObjRelease(&sOut);` |
|     48 | 8666 | `	return PH7_OK;` |
|     28 | 8667 | `}` |
|      - | 8668 | `/* One payload field, with the value php gives an absent one. A container is not a` |
|      - | 8669 | ` * number to php's reader either, so it counts as absent. */` |
|    286 | 8670 | `static sxi64 DtIvRestoreInt(ph7_value *pData,const char *zKey,sxi64 iAbsent)` |
|      1 | 8671 | `{` |
|    287 | 8672 | `	ph7_value *pVal = ph7_array_fetch(pData,zKey,-1);` |
|    287 | 8673 | `	if( pVal == 0 \|\| (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_STRING)) == 0 ){` |
|    149 | 8674 | `		return iAbsent;` |
|      - | 8675 | `	}` |
|    139 | 8676 | `	return PH7_ValuePeekInt64(pVal);` |
|    144 | 8677 | `}` |
|      - | 8678 | `/* php's date_interval_initialize_from_hash(), field by field. */` |
|     38 | 8679 | `static void DtIvRestore(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 8680 | `{` |
|      - | 8681 | `	static const char * const azMinusOne[] = { "y","m","d","h","i","s" };` |
|      - | 8682 | `	ph7_value *pVal;` |
|      - | 8683 | `	sxu32 n;` |
|    267 | 8684 | `	for( n = 0 ; n < SX_ARRAYSIZE(azMinusOne) ; ++n ){` |
|    343 | 8685 | `		PH7_NativeSetAttrInt(&(*pVm),pThis,azMinusOne[n],` |
|    228 | 8686 | `			DtIvRestoreInt(pData,azMinusOne[n],-1));` |
|    115 | 8687 | `	}` |
|     39 | 8688 | `	PH7_NativeSetAttrInt(&(*pVm),pThis,"invert",DtIvRestoreInt(pData,"invert",0));` |
|      - | 8689 | ``	/* `days` is php's one field with two TYPES -- a day count, or false when the`` |
|      - | 8690 | `	 * interval was not measured between two dates -- so a bool payload stays a` |
|      - | 8691 | `	 * bool where every other field is narrowed to an int. */` |
|     39 | 8692 | `	pVal = ph7_array_fetch(pData,"days",-1);` |
|     39 | 8693 | `	if( pVal && (pVal->iFlags & MEMOBJ_BOOL) ){` |
|     19 | 8694 | `		PH7_NativeSetAttrBool(&(*pVm),pThis,"days",pVal->x.iVal != 0);` |
|     10 | 8695 | `	}else{` |
|     21 | 8696 | `		PH7_NativeSetAttrInt(&(*pVm),pThis,"days",DtIvRestoreInt(pData,"days",-1));` |
|      - | 8697 | `	}` |
|     39 | 8698 | `	pVal = ph7_array_fetch(pData,"f",-1);` |
|     67 | 8699 | `	DtIvSetUsec(&(*pVm),pThis,` |
|     28 | 8700 | `		pVal && (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_STRING))` |
|     18 | 8701 | `			? DtIvUsecOfReal((double)PH7_ValuePeekReal(pVal)) : 0);` |
|      - | 8702 | `	/* php's LAZY interval, and what it keys on is the STRING rather than the` |
|      - | 8703 | ``	 * flag beside it: a payload carrying `date_string` comes back as an interval`` |
|      - | 8704 | `	 * built from that string -- the ten fields parsed out of it and hidden behind` |
|      - | 8705 | ``	 * the two names php shows -- while one carrying `from_string` alone is an`` |
|      - | 8706 | `	 * ordinary interval whose fields the reader above already filled. */` |
|     39 | 8707 | `	pVal = ph7_array_fetch(pData,"date_string",-1);` |
|     39 | 8708 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|      - | 8709 | `		sxi64 aVal[DT_IV_USLOT + 1];` |
|      3 | 8710 | `		int nIn,iPos = 0;` |
|      3 | 8711 | `		char cAt = ' ';` |
|      3 | 8712 | `		const char *zIn,*zReason = "";` |
|      3 | 8713 | `		zIn = (const char *)SyBlobData(&pVal->sBlob);` |
|      3 | 8714 | `		nIn = (int)SyBlobLength(&pVal->sBlob);` |
|      3 | 8715 | `		if( DtIvParseRelative(zIn,nIn,aVal,&iPos,&cAt,&zReason) == 0 ){` |
|      - | 8716 | ``			/* DtIvStore writes the six fields and the microseconds; `days` is`` |
|      - | 8717 | `			 * php's own answer for an interval that was not measured between two` |
|      - | 8718 | `			 * dates. */` |
|      3 | 8719 | `			DtIvStore(&(*pVm),pThis,aVal);` |
|      3 | 8720 | `			PH7_NativeSetAttrBool(&(*pVm),pThis,"days",0);` |
|      1 | 8721 | `		}` |
|      3 | 8722 | `		DtIvFromString(&(*pVm),pThis,zIn,nIn);` |
|      3 | 8723 | `		return;` |
|      - | 8724 | `	}` |
|      - | 8725 | `	/* ...and a payload without one is an ordinary interval whatever its` |
|      - | 8726 | ``	 * `from_string` says: php answers false there even for the `b:1` a hand-made`` |
|      - | 8727 | `	 * payload carries. */` |
|     37 | 8728 | `	PH7_NativeSetAttrBool(&(*pVm),pThis,"from_string",0);` |
|     20 | 8729 | `}` |
|      - | 8730 | `/*` |
|      - | 8731 | ` * php's date_period_initialize_from_hash(): the whole payload or nothing.` |
|      - | 8732 | ` * Answers 0 when it restored, -1 when the caller must raise.` |
|      - | 8733 | ` */` |
|    128 | 8734 | `static int DpRestoreOne(ph7_vm *pVm,ph7_value *pData,const char *zKey,const char *zClass,` |
|      - | 8735 | `	int iFlags,ph7_value **ppOut)` |
|      1 | 8736 | `{` |
|    129 | 8737 | `	ph7_value *pVal = ph7_array_fetch(pData,zKey,-1);` |
|    129 | 8738 | `	if( pVal == 0 ){` |
|     13 | 8739 | `		return -1;` |
|      - | 8740 | `	}` |
|    117 | 8741 | `	if( zClass ){` |
|     80 | 8742 | `		if( (pVal->iFlags & MEMOBJ_NULL) == 0` |
|     60 | 8743 | `		 && !DtValueIsA(&(*pVm),pVal,zClass) ){` |
|      5 | 8744 | `			return -1;` |
|      1 | 8745 | `		}` |
|     75 | 8746 | `	}else if( (pVal->iFlags & iFlags) == 0 ){` |
|      5 | 8747 | `		return -1;` |
|      - | 8748 | `	}` |
|    109 | 8749 | `	*ppOut = pVal;` |
|    109 | 8750 | `	return 0;` |
|     65 | 8751 | `}` |
|     28 | 8752 | `static int DpRestore(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 8753 | `{` |
|      - | 8754 | `	ph7_value *pStart,*pCur,*pEnd,*pIv,*pRec,*pIncS,*pIncE;` |
|     28 | 8755 | `	if( (pData->iFlags & MEMOBJ_HASHMAP) == 0` |
|     28 | 8756 | `	 \|\| DpRestoreOne(&(*pVm),pData,"start","DateTimeInterface",0,&pStart) != 0` |
|     24 | 8757 | `	 \|\| DpRestoreOne(&(*pVm),pData,"current","DateTimeInterface",0,&pCur) != 0` |
|     20 | 8758 | `	 \|\| DpRestoreOne(&(*pVm),pData,"end","DateTimeInterface",0,&pEnd) != 0` |
|     20 | 8759 | `	 \|\| DpRestoreOne(&(*pVm),pData,"interval","DateInterval",0,&pIv) != 0` |
|     18 | 8760 | `	 \|\| DpRestoreOne(&(*pVm),pData,"recurrences",0,MEMOBJ_INT,&pRec) != 0` |
|     14 | 8761 | `	 \|\| DpRestoreOne(&(*pVm),pData,"include_start_date",0,MEMOBJ_BOOL,&pIncS) != 0` |
|     13 | 8762 | `	 \|\| DpRestoreOne(&(*pVm),pData,"include_end_date",0,MEMOBJ_BOOL,&pIncE) != 0 ){` |
|     21 | 8763 | `		return -1;` |
|      - | 8764 | `	}` |
|      9 | 8765 | `	PH7_NativeSetProp(&(*pVm),pThis,"start",sizeof("start")-1,pStart);` |
|      9 | 8766 | `	PH7_NativeSetProp(&(*pVm),pThis,"current",sizeof("current")-1,pCur);` |
|      9 | 8767 | `	PH7_NativeSetProp(&(*pVm),pThis,"end",sizeof("end")-1,pEnd);` |
|      9 | 8768 | `	PH7_NativeSetProp(&(*pVm),pThis,"interval",sizeof("interval")-1,pIv);` |
|      9 | 8769 | `	PH7_NativeSetProp(&(*pVm),pThis,"recurrences",sizeof("recurrences")-1,pRec);` |
|      9 | 8770 | `	PH7_NativeSetProp(&(*pVm),pThis,"include_start_date",sizeof("include_start_date")-1,pIncS);` |
|      9 | 8771 | `	PH7_NativeSetProp(&(*pVm),pThis,"include_end_date",sizeof("include_end_date")-1,pIncE);` |
|      9 | 8772 | `	return 0;` |
|     15 | 8773 | `}` |
|      - | 8774 | `/* Every payload key that is not part of the class's own shape becomes a property,` |
|      - | 8775 | ` * the way php's restore_custom_* does (a name the class does not DECLARE is` |
|      - | 8776 | ` * dropped here, which is what PHL's own unserialize does with one). */` |
|      - | 8777 | `typedef struct dt_prop_restore dt_prop_restore;` |
|      - | 8778 | `struct dt_prop_restore` |
|      - | 8779 | `{` |
|      - | 8780 | `	ph7_class_instance *pThis;` |
|      - | 8781 | `	const char * const *azOwn;   /* the keys the class's own restore already read */` |
|      - | 8782 | `	sxu32 nOwn;` |
|      - | 8783 | `};` |
|    256 | 8784 | `static int DtPropRestoreWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 | 8785 | `{` |
|    257 | 8786 | `	dt_prop_restore *pRes = (dt_prop_restore *)pUserData;` |
|      - | 8787 | `	const char *zKey;` |
|      - | 8788 | `	int nKey;` |
|      - | 8789 | `	sxu32 n;` |
|    257 | 8790 | `	if( !ph7_value_is_string(pKey) ){` |
|    ! 0 | 8791 | `		return PH7_OK;` |
|      - | 8792 | `	}` |
|    257 | 8793 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|   1339 | 8794 | `	for( n = 0 ; n < pRes->nOwn ; ++n ){` |
|   1338 | 8795 | `		if( (int)SyStrlen(pRes->azOwn[n]) == nKey` |
|    996 | 8796 | `		 && SyMemcmp(pRes->azOwn[n],zKey,(sxu32)nKey) == 0 ){` |
|    257 | 8797 | `			return PH7_OK;` |
|      - | 8798 | `		}` |
|    542 | 8799 | `	}` |
|    ! 0 | 8800 | `	PH7_NativeSetProp(pRes->pThis->pVm,pRes->pThis,zKey,(sxu32)nKey,pVal);` |
|    ! 0 | 8801 | `	return PH7_OK;` |
|    129 | 8802 | `}` |
|      - | 8803 | `/* The shared body of __unserialize/__wakeup/__set_state for the two classes:` |
|      - | 8804 | ` * bPeriod picks the rule, pThis is the object being filled. */` |
|     66 | 8805 | `static int DtPropsRestoreInto(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pData,` |
|      - | 8806 | `	int bPeriod,const char *zClass)` |
|      1 | 8807 | `{` |
|      - | 8808 | `	static const char * const azIvOwn[] = {` |
|      - | 8809 | `		"y","m","d","h","i","s","f","invert","days","from_string","date_string"` |
|      - | 8810 | `	};` |
|      - | 8811 | `	static const char * const azDpOwn[] = {` |
|      - | 8812 | `		"start","current","end","interval","recurrences",` |
|      - | 8813 | `		"include_start_date","include_end_date"` |
|      - | 8814 | `	};` |
|      - | 8815 | `	dt_prop_restore sRes;` |
|     67 | 8816 | `	if( bPeriod ){` |
|     29 | 8817 | `		if( DpRestore(pCtx->pVm,pThis,pData) != 0 ){` |
|     21 | 8818 | `			return DtSerialError(pCtx,zClass);` |
|      - | 8819 | `		}` |
|      5 | 8820 | `	}else{` |
|     39 | 8821 | `		DtIvRestore(pCtx->pVm,pThis,pData);` |
|      - | 8822 | `	}` |
|     47 | 8823 | `	sRes.pThis = pThis;` |
|     47 | 8824 | `	sRes.azOwn = bPeriod ? azDpOwn : azIvOwn;` |
|     47 | 8825 | `	sRes.nOwn = bPeriod ? SX_ARRAYSIZE(azDpOwn) : SX_ARRAYSIZE(azIvOwn);` |
|     47 | 8826 | `	ph7_array_walk(pData,DtPropRestoreWalk,&sRes);` |
|     47 | 8827 | `	DtSetInit(pCtx->pVm,pThis);` |
|     47 | 8828 | `	return PH7_OK;` |
|     34 | 8829 | `}` |
|     54 | 8830 | `static int DtUnserializeProps(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPeriod,` |
|      - | 8831 | `	const char *zClass)` |
|      1 | 8832 | `{` |
|      - | 8833 | `	/* Raw: this is a door that INITIALIZES -- unserialize() calls it on an object` |
|      - | 8834 | `	 * the engine built without a constructor. */` |
|     55 | 8835 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|     55 | 8836 | `	if( pThis == 0 ){` |
|    ! 0 | 8837 | `		return PH7_OK;` |
|      - | 8838 | `	}` |
|     55 | 8839 | `	if( DtCheckDataArg(pCtx,nArg,apArg,zClass) != 0 ){` |
|    ! 0 | 8840 | `		return PH7_EXCEPTION;` |
|      - | 8841 | `	}` |
|     55 | 8842 | `	return DtPropsRestoreInto(pCtx,pThis,apArg[0],bPeriod,zClass);` |
|     28 | 8843 | `}` |
|     30 | 8844 | `static int vm_builtin_DateInterval_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8845 | `{` |
|     31 | 8846 | `	return DtUnserializeProps(pCtx,nArg,apArg,0,"DateInterval");` |
|      1 | 8847 | `}` |
|     24 | 8848 | `static int vm_builtin_DatePeriod_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8849 | `{` |
|     25 | 8850 | `	return DtUnserializeProps(pCtx,nArg,apArg,1,"DatePeriod");` |
|      1 | 8851 | `}` |
|      - | 8852 | `/*` |
|      - | 8853 | ` * __wakeup(): the LEGACY payload, which the engine has already written into the` |
|      - | 8854 | ` * object's own properties -- so the restore rule reads them back off the object` |
|      - | 8855 | ` * itself. An interval whose payload said nothing becomes php's all -1 interval;` |
|      - | 8856 | `` * a period whose payload is incomplete is php's Error, `new DatePeriod` included.`` |
|      - | 8857 | ` */` |
|      2 | 8858 | `static int DtWakeupProps(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPeriod,` |
|      - | 8859 | `	const char *zClass)` |
|      1 | 8860 | `{` |
|      3 | 8861 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|      - | 8862 | `	ph7_value sProps;` |
|      - | 8863 | `	int rc;` |
|      1 | 8864 | `	SXUNUSED(nArg);` |
|      1 | 8865 | `	SXUNUSED(apArg);` |
|      3 | 8866 | `	if( pThis == 0 ){` |
|    ! 0 | 8867 | `		return PH7_OK;` |
|      - | 8868 | `	}` |
|      3 | 8869 | `	PH7_MemObjInit(pCtx->pVm,&sProps);` |
|      3 | 8870 | `	if( PH7_MemObjToHashmap(&sProps) != SXRET_OK ){` |
|    ! 0 | 8871 | `		PH7_MemObjRelease(&sProps);` |
|    ! 0 | 8872 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8873 | `	}` |
|      3 | 8874 | `	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)sProps.x.pOther);` |
|      3 | 8875 | `	rc = DtPropsRestoreInto(pCtx,pThis,&sProps,bPeriod,zClass);` |
|      3 | 8876 | `	PH7_MemObjRelease(&sProps);` |
|      3 | 8877 | `	return rc;` |
|      2 | 8878 | `}` |
|      2 | 8879 | `static int vm_builtin_DateInterval_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8880 | `{` |
|      3 | 8881 | `	return DtWakeupProps(pCtx,nArg,apArg,0,"DateInterval");` |
|      1 | 8882 | `}` |
|    ! 0 | 8883 | `static int vm_builtin_DatePeriod_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 8884 | `{` |
|    ! 0 | 8885 | `	return DtWakeupProps(pCtx,nArg,apArg,1,"DatePeriod");` |
|    ! 0 | 8886 | `}` |
|      - | 8887 | `/* __set_state(array $array): what var_export's text evaluates to. php builds the` |
|      - | 8888 | ` * class the method is DECLARED on, as it does for the date classes. */` |
|     10 | 8889 | `static int DtSetStateProps(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPeriod,` |
|      - | 8890 | `	const char *zClass)` |
|      1 | 8891 | `{` |
|     11 | 8892 | `	ph7_class *pClass = DtClass(pCtx->pVm,zClass);` |
|      - | 8893 | `	ph7_class_instance *pObj;` |
|      - | 8894 | `	int rc;` |
|     11 | 8895 | `	if( pClass == 0 ){` |
|    ! 0 | 8896 | `		return PH7_OK;` |
|      - | 8897 | `	}` |
|     11 | 8898 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - | 8899 | `		char zBuf[64];` |
|    ! 0 | 8900 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 8901 | `			"%s::__set_state(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 | 8902 | `			zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|      - | 8903 | `	}` |
|      - | 8904 | `	/* Not DtNewInstance(): the restore below is what initializes it, and only if` |
|      - | 8905 | `	 * it succeeds. */` |
|     11 | 8906 | `	pObj = PH7_NewClassInstance(pCtx->pVm,pClass);` |
|     11 | 8907 | `	if( pObj == 0 ){` |
|    ! 0 | 8908 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8909 | `	}` |
|     11 | 8910 | `	rc = DtPropsRestoreInto(pCtx,pObj,apArg[0],bPeriod,zClass);` |
|     11 | 8911 | `	if( rc != PH7_OK \|\| pCtx->nThrowRc != 0 ){` |
|      3 | 8912 | `		PH7_ClassInstanceUnref(pObj);` |
|      3 | 8913 | `		return rc;` |
|      - | 8914 | `	}` |
|      9 | 8915 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      9 | 8916 | `	return PH7_OK;` |
|      6 | 8917 | `}` |
|      6 | 8918 | `static int vm_builtin_DateInterval_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8919 | `{` |
|      7 | 8920 | `	return DtSetStateProps(pCtx,nArg,apArg,0,"DateInterval");` |
|      1 | 8921 | `}` |
|      4 | 8922 | `static int vm_builtin_DatePeriod_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8923 | `{` |
|      5 | 8924 | `	return DtSetStateProps(pCtx,nArg,apArg,1,"DatePeriod");` |
|      1 | 8925 | `}` |
|      - | 8926 | `/* The four rows both date classes take. __serialize/__unserialize are php's only` |
|      - | 8927 | ` * NON-tentative internal returns in this family; __wakeup and __set_state carry the` |
|      - | 8928 | `` * `@`, and __set_state's return names the CONCRETE class php's stub writes. */`` |
|      - | 8929 | `#define DT_NATIVE_SERIAL_METHODS(CLS) \` |
|      - | 8930 | `	{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_DateTime_serialize }, \` |
|      - | 8931 | `	{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void", \` |
|      - | 8932 | `	  vm_builtin_##CLS##_unserialize }, \` |
|      - | 8933 | `	{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_##CLS##_wakeup }, \` |
|      - | 8934 | `	{ "__set_state",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@" #CLS, \` |
|      - | 8935 | `	  vm_builtin_##CLS##_setState }` |
|      - | 8936 | `/* php's DateTimeInterface constants. */` |
|      - | 8937 | `#define DT_IFACE_CONST(NAME,VALUE) \` |
|      - | 8938 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, VALUE, 0.0 }` |
|      - | 8939 | `/*` |
|      - | 8940 | ` * Install the whole date family from C: the exceptions and DateTimeInterface, then` |
|      - | 8941 | ` * DateTimeZone / DateTime / DateTimeImmutable, DateInterval and DatePeriod, then the` |
|      - | 8942 | ` * procedural aliases. The InternalIterator its getIterator() answers is not declared` |
|      - | 8943 | ` * here — it is shared native machinery (oo_native.c), reached through the vtable` |
|      - | 8944 | ` * DatePeriod's spec row names.` |
|      - | 8945 | ` *` |
|      - | 8946 | ` * Called from PH7_VmInit inside the bCompilingBuiltin window, after the Reflection` |
|      - | 8947 | ` * install (Exception must exist). IteratorAggregate is attached AFTER DatePeriod's` |
|      - | 8948 | ` * methods exist, for the abstract-stub reason above; DateTimeInterface declares no` |
|      - | 8949 | ` * method, so it can ride the spec table.` |
|      - | 8950 | ` */` |
|   7925 | 8951 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
|      5 | 8952 | `{` |
|      - | 8953 | `	static const PH7_NativeConstDef aIfaceConst[] = {` |
|      - | 8954 | `		DT_IFACE_CONST("ATOM","Y-m-d\\TH:i:sP"),` |
|      - | 8955 | `		DT_IFACE_CONST("COOKIE","l, d-M-Y H:i:s T"),` |
|      - | 8956 | `		DT_IFACE_CONST("ISO8601","Y-m-d\\TH:i:sO"),` |
|      - | 8957 | `		DT_IFACE_CONST("ISO8601_EXPANDED","X-m-d\\TH:i:sP"),` |
|      - | 8958 | `		DT_IFACE_CONST("RFC822","D, d M y H:i:s O"),` |
|      - | 8959 | `		DT_IFACE_CONST("RFC850","l, d-M-y H:i:s T"),` |
|      - | 8960 | `		DT_IFACE_CONST("RFC1036","D, d M y H:i:s O"),` |
|      - | 8961 | `		DT_IFACE_CONST("RFC1123","D, d M Y H:i:s O"),` |
|      - | 8962 | `		DT_IFACE_CONST("RFC7231","D, d M Y H:i:s \\G\\M\\T"),` |
|      - | 8963 | `		DT_IFACE_CONST("RFC2822","D, d M Y H:i:s O"),` |
|      - | 8964 | `		DT_IFACE_CONST("RFC3339","Y-m-d\\TH:i:sP"),` |
|      - | 8965 | `		DT_IFACE_CONST("RFC3339_EXTENDED","Y-m-d\\TH:i:s.vP"),` |
|      - | 8966 | `		DT_IFACE_CONST("RSS","D, d M Y H:i:s O"),` |
|      - | 8967 | `		DT_IFACE_CONST("W3C","Y-m-d\\TH:i:sP"),` |
|      - | 8968 | `	};` |
|      - | 8969 | `	/*` |
|      - | 8970 | `	 * php's DateTimeInterface METHODS, which this engine did not declare at` |
|      - | 8971 | ``	 * all -- so the nine it contracts for reported no `prototype` in the`` |
|      - | 8972 | `	 * export (18 rows across DateTime and DateTimeImmutable), and the` |
|      - | 8973 | `	 * interface itself answered isAbstract() false for want of a member.` |
|      - | 8974 | `	 * Declared in php's own order, which is the order its export lists them.` |
|      - | 8975 | `	 *` |
|      - | 8976 | `	 * Safe on the spec table because PH7_InstallNativeClasses fills every` |
|      - | 8977 | `	 * class's METHODS before it wires any interface: PH7_ClassImplement's` |
|      - | 8978 | `	 * abstract stubbing then finds DateTime's own nine already there and` |
|      - | 8979 | `	 * skips them, which is the same reason DatePeriod's IteratorAggregate is` |
|      - | 8980 | `	 * attached by hand AFTER its methods (it is not in this table).` |
|      - | 8981 | `	 */` |
|      - | 8982 | `	static const PH7_NativeMethodDef aIfaceMethod[] = {` |
|      - | 8983 | `		{ "format",        PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $format", "@string", 0 },` |
|      - | 8984 | `		{ "getTimezone",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@DateTimeZone\|false", 0 },` |
|      - | 8985 | `		{ "getOffset",     PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@int", 0 },` |
|      - | 8986 | `		{ "getTimestamp",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@int", 0 },` |
|      - | 8987 | `		{ "getMicrosecond",PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "int", 0 },` |
|      - | 8988 | `		{ "diff",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT,` |
|      - | 8989 | `		  "DateTimeInterface $targetObject, bool $absolute = false", "@DateInterval", 0 },` |
|      - | 8990 | `		{ "__wakeup",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|      - | 8991 | `		{ "__serialize",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|      - | 8992 | `		{ "__unserialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "array $data", "void", 0 },` |
|      - | 8993 | `	};` |
|      - | 8994 | `	static const PH7_NativePropDef aZoneProp[] = {` |
|      - | 8995 | `		{ DTZ_OFF,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 8996 | `		{ DTZ_NAME, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 }, 0 },` |
|      - | 8997 | `		{ DTZ_KIND, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|      - | 8998 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DT_ZONE_ID, 0, 0.0 }, 0 },` |
|      - | 8999 | `		{ DT_INIT,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|      - | 9000 | `	};` |
|      - | 9001 | `	static const PH7_NativeMethodDef aZoneMethod[] = {` |
|      - | 9002 | `		{ "__construct", PH7_MOD_PUBLIC, "string $timezone", "", vm_builtin_DateTimeZone_construct },` |
|      - | 9003 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_DateTimeZone_getName },` |
|      - | 9004 | `		{ "getOffset",   PH7_MOD_PUBLIC, "DateTimeInterface $datetime", "@int",` |
|      - | 9005 | `		  vm_builtin_DateTimeZone_getOffset },` |
|      - | 9006 | `		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_DateTimeZone_serialize },` |
|      - | 9007 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|      - | 9008 | `		  vm_builtin_DateTimeZone_unserialize },` |
|      - | 9009 | `		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DateTimeZone_wakeup },` |
|      - | 9010 | `		{ "__set_state",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@DateTimeZone",` |
|      - | 9011 | `		  vm_builtin_DateTimeZone_setState },` |
|      - | 9012 | `		{ "listIdentifiers", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 9013 | `		  "int $timezoneGroup = 2047, ?string $countryCode = null", "@array",` |
|      - | 9014 | `		  vm_builtin_DateTimeZone_listIdentifiers },` |
|      - | 9015 | `		{ "getTransitions", PH7_MOD_PUBLIC,` |
|      - | 9016 | `		  "int $timestampBegin = PHP_INT_MIN, int $timestampEnd = 2147483647", "@array\|false",` |
|      - | 9017 | `		  vm_builtin_DateTimeZone_getTransitions },` |
|      - | 9018 | `		{ "listAbbreviations", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "@array",` |
|      - | 9019 | `		  vm_builtin_DateTimeZone_listAbbreviations },` |
|      - | 9020 | `		{ "getLocation", PH7_MOD_PUBLIC, "", "@array\|false",` |
|      - | 9021 | `		  vm_builtin_DateTimeZone_getLocation },` |
|      - | 9022 | `	};` |
|      - | 9023 | `	/* php's group bitmask. ALL and ALL_WITH_BC are the two the reader compares` |
|      - | 9024 | `	 * EXACTLY rather than masking (see DtZoneListResult). */` |
|      - | 9025 | `	static const PH7_NativeConstDef aZoneConst[] = {` |
|      - | 9026 | `		{ "AFRICA",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,    0, 0.0 },` |
|      - | 9027 | `		{ "AMERICA",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,    0, 0.0 },` |
|      - | 9028 | `		{ "ANTARCTICA",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,    0, 0.0 },` |
|      - | 9029 | `		{ "ARCTIC",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8,    0, 0.0 },` |
|      - | 9030 | `		{ "ASIA",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,   0, 0.0 },` |
|      - | 9031 | `		{ "ATLANTIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,   0, 0.0 },` |
|      - | 9032 | `		{ "AUSTRALIA",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,   0, 0.0 },` |
|      - | 9033 | `		{ "EUROPE",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128,  0, 0.0 },` |
|      - | 9034 | `		{ "INDIAN",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 256,  0, 0.0 },` |
|      - | 9035 | `		{ "PACIFIC",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 512,  0, 0.0 },` |
|      - | 9036 | `		{ "UTC",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1024, 0, 0.0 },` |
|      - | 9037 | `		{ "ALL",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DT_TZ_GROUP_ALL, 0, 0.0 },` |
|      - | 9038 | `		{ "ALL_WITH_BC", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DT_TZ_GROUP_ALL_W_BC, 0, 0.0 },` |
|      - | 9039 | `		{ "PER_COUNTRY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DT_TZ_GROUP_PER_COUNTRY, 0, 0.0 }` |
|      - | 9040 | `	};` |
|      - | 9041 | `	static const PH7_NativePropDef aDtProp[] = { DT_NATIVE_STATE_PROPS };` |
|      - | 9042 | `	static const PH7_NativeMethodDef aDtMethod[] = {` |
|      - | 9043 | `		DT_NATIVE_SHARED_METHODS("DateTime"),` |
|      - | 9044 | `		{ "createFromFormat",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 9045 | `		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTime\|false",` |
|      - | 9046 | `		  vm_builtin_DateTime_createFromFormat },` |
|      - | 9047 | `		{ "createFromImmutable", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeImmutable $object", "@static",` |
|      - | 9048 | `		  vm_builtin_DateTime_copyOf },` |
|      - | 9049 | `		{ "createFromTimestamp", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "int\|float $timestamp", "@static",` |
|      - | 9050 | `		  vm_builtin_DateTime_createFromTimestamp },` |
|      - | 9051 | `		{ "createFromInterface", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTime",` |
|      - | 9052 | `		  vm_builtin_DateTime_copyOf },` |
|      - | 9053 | `		DT_NATIVE_SERIAL_METHODS(DateTime),` |
|      - | 9054 | `	};` |
|      - | 9055 | `	static const PH7_NativeMethodDef aImmMethod[] = {` |
|      - | 9056 | `		DT_NATIVE_SHARED_METHODS("DateTimeImmutable"),` |
|      - | 9057 | `		{ "createFromFormat",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 9058 | `		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTimeImmutable\|false",` |
|      - | 9059 | `		  vm_builtin_DateTimeImmutable_createFromFormat },` |
|      - | 9060 | `		{ "createFromMutable",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTime $object", "@static",` |
|      - | 9061 | `		  vm_builtin_DateTimeImmutable_copyOf },` |
|      - | 9062 | `		{ "createFromTimestamp", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "int\|float $timestamp", "@static",` |
|      - | 9063 | `		  vm_builtin_DateTimeImmutable_createFromTimestamp },` |
|      - | 9064 | `		{ "createFromInterface", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTimeImmutable",` |
|      - | 9065 | `		  vm_builtin_DateTimeImmutable_copyOf },` |
|      - | 9066 | `		DT_NATIVE_SERIAL_METHODS(DateTimeImmutable),` |
|      - | 9067 | `	};` |
|      - | 9068 | `	static const PH7_NativePropDef aIvProp[] = {` |
|      - | 9069 | `		{ "y",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 9070 | `		{ "m",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 9071 | `		{ "d",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 9072 | `		{ "h",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 9073 | `		{ "i",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 9074 | `		{ "s",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 9075 | ``		/* php's `f` is a FLOAT; the chunk's `= 0` made it an int. */`` |
|      - | 9076 | `		{ "f",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_DOUBLE, 0, 0, 0.0 }, 0 },` |
|      - | 9077 | `		{ "invert",      PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 9078 | `		{ "days",        PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - | 9079 | `		{ "from_string", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - | 9080 | ``		/* php's `date_string` is on an interval built from a STRING and on no`` |
|      - | 9081 | `		 * other, so it is installed by the write that names it: an ordinary` |
|      - | 9082 | ``		 * interval does not carry the name at all, and `isset()` says so. */`` |
|      - | 9083 | `		{ "date_string", PH7_MOD_PUBLIC\|PH7_MOD_ONDEMAND,` |
|      - | 9084 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 9085 | ``		/* php's timelib_rel_time.us, the count `f` renders: see DtIvUsec. */`` |
|      - | 9086 | `		{ DT_IV_US,      PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|      - | 9087 | `		{ DT_INIT,       PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|      - | 9088 | `	};` |
|      - | 9089 | `	static const PH7_NativeMethodDef aIvMethod[] = {` |
|      - | 9090 | `		{ "__construct", PH7_MOD_PUBLIC, "string $duration", "",` |
|      - | 9091 | `		  vm_builtin_DateInterval_construct },` |
|      - | 9092 | `		{ "createFromDateString", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $datetime", "@DateInterval",` |
|      - | 9093 | `		  vm_builtin_DateInterval_createFromDateString },` |
|      - | 9094 | `		{ "format",      PH7_MOD_PUBLIC, "string $format", "@string", vm_builtin_DateInterval_format },` |
|      - | 9095 | `		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", DtSerializeProps },` |
|      - | 9096 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|      - | 9097 | `		  vm_builtin_DateInterval_unserialize },` |
|      - | 9098 | `		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DateInterval_wakeup },` |
|      - | 9099 | `		{ "__set_state",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@DateInterval",` |
|      - | 9100 | `		  vm_builtin_DateInterval_setState },` |
|      - | 9101 | `	};` |
|      - | 9102 | `	/* php models all seven as VIRTUAL hooked properties, so it reports no default` |
|      - | 9103 | `	 * for any of them; PHL's are real slots and keep theirs, because a read before` |
|      - | 9104 | `	 * the first write must answer what php's getter answers rather than raise. The` |
|      - | 9105 | `	 * TYPE is what a spec row can state exactly — the virtual half is recorded. */` |
|      - | 9106 | `	static const PH7_NativePropDef aDpProp[] = {` |
|      - | 9107 | `		{ "start",              PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 9108 | `		{ "current",            PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 9109 | `		{ "end",                PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 9110 | `		{ "interval",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateInterval" },` |
|      - | 9111 | `		/* php FABRICATES these from its struct, so an object with no struct reads` |
|      - | 9112 | `		 * them as the zeroed one: 0 and false, not the 1 and true a constructed` |
|      - | 9113 | `		 * period ends up with. Every constructor path writes all three. */` |
|      - | 9114 | `		{ "recurrences",        PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, "int" },` |
|      - | 9115 | `		{ "include_start_date", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|      - | 9116 | `		{ "include_end_date",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|      - | 9117 | `		{ DT_INIT,              PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|      - | 9118 | `		  { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|      - | 9119 | `	};` |
|      - | 9120 | `	static const PH7_NativeConstDef aDpConst[] = {` |
|      - | 9121 | `		{ "EXCLUDE_START_DATE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|      - | 9122 | `		{ "INCLUDE_END_DATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|      - | 9123 | `	};` |
|      - | 9124 | `	static const PH7_NativeMethodDef aDpMethod[] = {` |
|      - | 9125 | `		/* php overloads this constructor three ways and rejects everything else with` |
|      - | 9126 | `		 * ONE message, so the signature stays unenforced and the body decides. */` |
|      - | 9127 | `		/* No signature ON PURPOSE, which is why Reflection reports no parameters` |
|      - | 9128 | `		 * for it. php declares four and enforces NEITHER end of the arity: the` |
|      - | 9129 | `		 * constructor has three shapes (start+interval+end, start+interval+count,` |
|      - | 9130 | ``		 * and the ISO string), and both `new DatePeriod()` and a five-argument`` |
|      - | 9131 | `		 * call reach the body and answer its own three-shape TypeError. A zSig` |
|      - | 9132 | `		 * here would enforce both bounds, so the choice is php's DIAGNOSTIC or` |
|      - | 9133 | `		 * php's parameter list, and the diagnostic wins. */` |
|      - | 9134 | `		{ "__construct",     PH7_MOD_PUBLIC, 0, "", vm_builtin_DatePeriod_construct },` |
|      - | 9135 | `		{ "createFromISO8601String", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 9136 | `		  "string $specification, int $options = 0", "static",` |
|      - | 9137 | `		  vm_builtin_DatePeriod_createFromISO8601String },` |
|      - | 9138 | `		{ "getStartDate",    PH7_MOD_PUBLIC, "", "@DateTimeInterface",` |
|      - | 9139 | `		  vm_builtin_DatePeriod_getStartDate },` |
|      - | 9140 | `		{ "getEndDate",      PH7_MOD_PUBLIC, "", "@?DateTimeInterface",` |
|      - | 9141 | `		  vm_builtin_DatePeriod_getEndDate },` |
|      - | 9142 | `		{ "getDateInterval", PH7_MOD_PUBLIC, "", "@DateInterval",` |
|      - | 9143 | `		  vm_builtin_DatePeriod_getDateInterval },` |
|      - | 9144 | `		{ "getRecurrences",  PH7_MOD_PUBLIC, "", "@?int", vm_builtin_DatePeriod_getRecurrences },` |
|      - | 9145 | `		{ "getIterator",     PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_DatePeriod_getIterator },` |
|      - | 9146 | `		{ "__serialize",     PH7_MOD_PUBLIC, "", "array", DtSerializeProps },` |
|      - | 9147 | `		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "void",` |
|      - | 9148 | `		  vm_builtin_DatePeriod_unserialize },` |
|      - | 9149 | `		{ "__wakeup",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_DatePeriod_wakeup },` |
|      - | 9150 | `		{ "__set_state",     PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@DatePeriod",` |
|      - | 9151 | `		  vm_builtin_DatePeriod_setState },` |
|      - | 9152 | `	};` |
|      - | 9153 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 9154 | `		/* Exceptions first: the classes below throw them. */` |
|      - | 9155 | `		{ "DateException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 9156 | `		{ "DateMalformedStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 9157 | `		{ "DateInvalidTimeZoneException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 9158 | `		{ "DateMalformedIntervalStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 9159 | `		{ "DateMalformedPeriodStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 9160 | `		{ "DateInvalidOperationException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 9161 | `		/* php's date tree has an ERROR half beside the exception one -- what a` |
|      - | 9162 | `		 * caller catches when an argument is out of RANGE (setMicrosecond) or the` |
|      - | 9163 | `		 * object was never constructed. All three were undefined here, so` |
|      - | 9164 | ``		 * `catch (DateRangeError $e)` could not be spelled at all. */`` |
|      - | 9165 | `		{ "DateError", "Error", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 9166 | `		{ "DateRangeError", "DateError", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 9167 | `		{ "DateObjectError", "DateError", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 9168 | `		{ "DateTimeInterface", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 9169 | `		  aIfaceMethod, SX_ARRAYSIZE(aIfaceMethod),` |
|      - | 9170 | `		  aIfaceConst, SX_ARRAYSIZE(aIfaceConst), 0, 0, 0, 0, 0 },` |
|      - | 9171 | `		{ "DateTimeZone", 0, 0, 0,` |
|      - | 9172 | `		  aZoneMethod, SX_ARRAYSIZE(aZoneMethod), aZoneConst, SX_ARRAYSIZE(aZoneConst),` |
|      - | 9173 | `		  aZoneProp, SX_ARRAYSIZE(aZoneProp),` |
|      - | 9174 | `		  0, 0, DtPresentTimeZone },` |
|      - | 9175 | `		{ "DateTime", 0, "DateTimeInterface", 0,` |
|      - | 9176 | `		  aDtMethod, SX_ARRAYSIZE(aDtMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),` |
|      - | 9177 | `		  0, 0, DtPresentDateTime },` |
|      - | 9178 | `		{ "DateTimeImmutable", 0, "DateTimeInterface", 0,` |
|      - | 9179 | `		  aImmMethod, SX_ARRAYSIZE(aImmMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),` |
|      - | 9180 | `		  0, 0, DtPresentDateTime },` |
|      - | 9181 | `		{ "DateInterval", 0, 0, 0,` |
|      - | 9182 | `		  aIvMethod, SX_ARRAYSIZE(aIvMethod), 0, 0, aIvProp, SX_ARRAYSIZE(aIvProp),` |
|      - | 9183 | `		  0, 0, DtPresentProps },` |
|      - | 9184 | `		{ "DatePeriod", 0, 0, 0,` |
|      - | 9185 | `		  aDpMethod, SX_ARRAYSIZE(aDpMethod), aDpConst, SX_ARRAYSIZE(aDpConst),` |
|      - | 9186 | `		  aDpProp, SX_ARRAYSIZE(aDpProp), 0, &sDpIterVtab, DtPresentProps },` |
|      - | 9187 | `	};` |
|      - | 9188 | `	/* php's procedural aliases. Each is a function in its own right, not a forward,` |
|      - | 9189 | `	 * and each owes aBuiltinSig[] a row (vm_arg_check.c). */` |
|      - | 9190 | `	static const struct {` |
|      - | 9191 | `		const char *zName;` |
|      - | 9192 | `		ProchHostFunction xFunc;` |
|      - | 9193 | `	} aFunc[] = {` |
|      - | 9194 | `		{ "strtotime",                    vm_builtin_strtotime },` |
|      - | 9195 | `		{ "date_create",                  vm_builtin_date_create },` |
|      - | 9196 | `		{ "date_create_immutable",        vm_builtin_date_create_immutable },` |
|      - | 9197 | `		{ "date_create_from_format",      vm_builtin_date_create_from_format },` |
|      - | 9198 | `		{ "date_create_immutable_from_format", vm_builtin_date_create_immutable_from_format },` |
|      - | 9199 | `		{ "date_format",                  vm_builtin_date_format },` |
|      - | 9200 | `		{ "date_modify",                  vm_builtin_date_modify },` |
|      - | 9201 | `		{ "date_add",                     vm_builtin_date_add },` |
|      - | 9202 | `		{ "date_sub",                     vm_builtin_date_sub },` |
|      - | 9203 | `		{ "date_diff",                    vm_builtin_date_diff },` |
|      - | 9204 | `		{ "date_timestamp_get",           vm_builtin_date_timestamp_get },` |
|      - | 9205 | `		{ "date_timestamp_set",           vm_builtin_date_timestamp_set },` |
|      - | 9206 | `		{ "date_timezone_get",            vm_builtin_date_timezone_get },` |
|      - | 9207 | `		{ "date_timezone_set",            vm_builtin_date_timezone_set },` |
|      - | 9208 | `		{ "date_offset_get",              vm_builtin_date_offset_get },` |
|      - | 9209 | `		{ "date_date_set",                vm_builtin_date_date_set },` |
|      - | 9210 | `		{ "date_time_set",                vm_builtin_date_time_set },` |
|      - | 9211 | `		{ "date_isodate_set",             vm_builtin_date_isodate_set },` |
|      - | 9212 | `		{ "date_interval_create_from_date_string", vm_builtin_date_interval_create_from_date_string },` |
|      - | 9213 | `		{ "date_interval_format",         vm_builtin_date_interval_format },` |
|      - | 9214 | `		{ "date_get_last_errors",         vm_builtin_date_get_last_errors },` |
|      - | 9215 | `		{ "date_parse",                   vm_builtin_date_parse },` |
|      - | 9216 | `		{ "date_parse_from_format",       vm_builtin_date_parse_from_format },` |
|      - | 9217 | `		{ "timezone_open",                vm_builtin_timezone_open },` |
|      - | 9218 | `		{ "timezone_name_get",            vm_builtin_timezone_name_get },` |
|      - | 9219 | `		{ "timezone_offset_get",          vm_builtin_timezone_offset_get },` |
|      - | 9220 | `		{ "timezone_identifiers_list",    vm_builtin_timezone_identifiers_list },` |
|      - | 9221 | `		{ "timezone_transitions_get",     vm_builtin_timezone_transitions_get },` |
|      - | 9222 | `		{ "timezone_abbreviations_list",  vm_builtin_timezone_abbreviations_list },` |
|      - | 9223 | `		{ "timezone_name_from_abbr",      vm_builtin_timezone_name_from_abbr },` |
|      - | 9224 | `		{ "timezone_location_get",        vm_builtin_timezone_location_get },` |
|      - | 9225 | `		{ "timezone_version_get",         vm_builtin_timezone_version_get },` |
|      - | 9226 | `	};` |
|      - | 9227 | `	sxu32 n;` |
|      - | 9228 | `	sxi32 rc;` |
|      - | 9229 | `	/* php's date.timezone default */` |
|   7930 | 9230 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|   7930 | 9231 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|      - | 9232 | `	/* The error rows are allocated from the VM's own backend and released` |
|      - | 9233 | `	 * wholesale with it, so this is the only lifetime call they need. */` |
|   7930 | 9234 | `	SyBlobInit(&pVm->sDtLastErr.sErr,&pVm->sAllocator);` |
|   7930 | 9235 | `	DtLastErrClear(&(*pVm));` |
| 261530 | 9236 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 253605 | 9237 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 126629 | 9238 | `	}` |
|   7930 | 9239 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|   7930 | 9240 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9241 | `		return rc;` |
|      - | 9242 | `	}` |
|      - | 9243 | `	/* php's write_property handler for DateInterval (ph7_class::xSet), assigned` |
|      - | 9244 | `	 * here for the reason the DOM's clone and dimension hooks are: the spec table` |
|      - | 9245 | `	 * carries no field for a hook. It also flags the class's properties, which is` |
|      - | 9246 | ``	 * what makes `new` register their slots with the store filter. */`` |
|   7930 | 9247 | `	rc = PH7_NativeClassInstallSetHook(&(*pVm),"DateInterval",DtIntervalSet);` |
|   7930 | 9248 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9249 | `		return rc;` |
|      - | 9250 | `	}` |
|      - | 9251 | `	/* php's compare handlers (ph7_class::xCmp), assigned here for the same reason` |
|      - | 9252 | `	 * the write handler is. DatePeriod gets none: php has no handler for it, its` |
|      - | 9253 | `	 * real property table is EMPTY (the seven it shows are fabricated), and the` |
|      - | 9254 | `	 * ordinary walk over nothing is what makes any two of them equal -- which is` |
|      - | 9255 | `	 * what marking those seven virtual reproduces. */` |
|      - | 9256 | `	{` |
|      - | 9257 | `		static const struct {` |
|      - | 9258 | `			const char *zClass;` |
|      - | 9259 | `			void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *);` |
|      - | 9260 | `		} aCmp[] = {` |
|      - | 9261 | `			{ "DateTime",          DtCmpDateTime },` |
|      - | 9262 | `			{ "DateTimeImmutable", DtCmpDateTime },` |
|      - | 9263 | `			{ "DateInterval",      DtCmpInterval },` |
|      - | 9264 | `			{ "DateTimeZone",      DtCmpTimeZone },` |
|      - | 9265 | `		};` |
|  39630 | 9266 | `		for( n = 0 ; n < SX_ARRAYSIZE(aCmp) ; n++ ){` |
|  31705 | 9267 | `			rc = PH7_NativeClassInstallCmpHook(&(*pVm),aCmp[n].zClass,aCmp[n].xCmp);` |
|  31705 | 9268 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 9269 | `				return rc;` |
|      - | 9270 | `			}` |
|  15833 | 9271 | `		}` |
|      - | 9272 | `	}` |
|   7930 | 9273 | `	rc = PH7_NativeClassMarkVirtualProps(&(*pVm),"DatePeriod");` |
|   7930 | 9274 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9275 | `		return rc;` |
|      - | 9276 | `	}` |
|      - | 9277 | `	/* php 8.5 marks every IMMUTABLE mutator #[\NoDiscard]: these nine answer a NEW` |
|      - | 9278 | `	 * object and change nothing, so a caller who drops the answer wrote a` |
|      - | 9279 | `	 * statement that does nothing at all -- the single most common way to misuse` |
|      - | 9280 | `	 * DateTimeImmutable. The mutable DateTime twins are NOT marked (there the` |
|      - | 9281 | `	 * object really did change), and neither is any other internal member: this is` |
|      - | 9282 | `	 * php's whole internal NoDiscard set. The message is php's own wording, with` |
|      - | 9283 | `	 * the method named in it. */` |
|      - | 9284 | `	{` |
|      - | 9285 | `		/* Nine rows, nine static literals: PH7_NativeMethodSetNoDiscard borrows the` |
|      - | 9286 | `		 * argument record for the VM's lifetime. php's stub spells the message as a` |
|      - | 9287 | `		 * NAMED argument, and getArguments() shows the key. */` |
|      - | 9288 | `		static const PH7_NativeAttrArg aNdWhy[] = {` |
|      - | 9289 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 9290 | `			  "as DateTimeImmutable::modify() does not modify the object itself", 0.0 } },` |
|      - | 9291 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 9292 | `			  "as DateTimeImmutable::add() does not modify the object itself", 0.0 } },` |
|      - | 9293 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 9294 | `			  "as DateTimeImmutable::sub() does not modify the object itself", 0.0 } },` |
|      - | 9295 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 9296 | `			  "as DateTimeImmutable::setTimezone() does not modify the object itself", 0.0 } },` |
|      - | 9297 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 9298 | `			  "as DateTimeImmutable::setTime() does not modify the object itself", 0.0 } },` |
|      - | 9299 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 9300 | `			  "as DateTimeImmutable::setDate() does not modify the object itself", 0.0 } },` |
|      - | 9301 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 9302 | `			  "as DateTimeImmutable::setISODate() does not modify the object itself", 0.0 } },` |
|      - | 9303 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 9304 | `			  "as DateTimeImmutable::setTimestamp() does not modify the object itself", 0.0 } },` |
|      - | 9305 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 9306 | `			  "as DateTimeImmutable::setMicrosecond() does not modify the object itself", 0.0 } },` |
|      - | 9307 | `		};` |
|      - | 9308 | `		static const char *const azNdMethod[] = {` |
|      - | 9309 | `			"modify","add","sub","setTimezone","setTime","setDate","setISODate",` |
|      - | 9310 | `			"setTimestamp","setMicrosecond"` |
|      - | 9311 | `		};` |
|   7930 | 9312 | `		ph7_class *pImm = PH7_VmExtractClass(&(*pVm),"DateTimeImmutable",` |
|      - | 9313 | `			sizeof("DateTimeImmutable")-1,FALSE,0);` |
|  79255 | 9314 | `		for( n = 0 ; n < SX_ARRAYSIZE(azNdMethod) ; n++ ){` |
|  71330 | 9315 | `			rc = PH7_NativeMethodSetNoDiscard(&(*pVm),pImm,azNdMethod[n],&aNdWhy[n],1);` |
|  71330 | 9316 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 9317 | `				return rc;` |
|      - | 9318 | `			}` |
|  35618 | 9319 | `		}` |
|      - | 9320 | `	}` |
|      - | 9321 | `	/* php's state for these two IS their properties, and the table is written FROM` |
|      - | 9322 | `	 * the C struct its constructor allocates -- so an object nobody constructed has` |
|      - | 9323 | ``	 * no such property at all. PHL declared them from `new`, so an unconstructed`` |
|      - | 9324 | `	 * interval answered ten defaults to a read, ten to isset(), ten to` |
|      - | 9325 | `	 * get_object_vars() and ten to a property foreach, beside the empty shape the` |
|      - | 9326 | `	 * presentation hook was already showing. The two classes differ in what a read` |
|      - | 9327 | `	 * of a still-absent slot answers, which is php's split between its two` |
|      - | 9328 | `	 * handlers: DatePeriod reads its seven from the zeroed struct (null/0/false, in` |
|      - | 9329 | `	 * silence), DateInterval has no such fallback and its ten really are undefined` |
|      - | 9330 | `	 * until the constructor runs. */` |
|   7930 | 9331 | `	rc = PH7_NativeClassMarkLazyProps(&(*pVm),"DateInterval",0);` |
|   7930 | 9332 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9333 | `		return rc;` |
|      - | 9334 | `	}` |
|   7930 | 9335 | `	rc = PH7_NativeClassMarkLazyProps(&(*pVm),"DatePeriod",1);` |
|   7930 | 9336 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9337 | `		return rc;` |
|      - | 9338 | `	}` |
|      - | 9339 | `	/* php's write_property handler for DatePeriod refuses OUTRIGHT: the seven are` |
|      - | 9340 | `	 * a view of its struct and a script may only read them. PHL kept real slots a` |
|      - | 9341 | ``	 * script could write, so `$p->recurrences = 99` and `$p->start = 5` landed and`` |
|      - | 9342 | `	 * the period then iterated to a shape no constructor would have built --` |
|      - | 9343 | ``	 * `unset($p->interval)` left one with no interval at all. */`` |
|   7930 | 9344 | `	rc = PH7_NativeClassMarkNoWriteProps(&(*pVm),"DatePeriod");` |
|   7930 | 9345 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9346 | `		return rc;` |
|      - | 9347 | `	}` |
|      - | 9348 | `	/* IteratorAggregate declares a METHOD, so it is attached now that DatePeriod has` |
|      - | 9349 | `	 * its own: PH7_ClassImplement stubs a missing one as ABSTRACT, which would have` |
|      - | 9350 | `	 * made the class uninstantiable. */` |
|      - | 9351 | `	{` |
|   7930 | 9352 | `		ph7_class *pPeriod = DtClass(&(*pVm),"DatePeriod");` |
|   7930 | 9353 | `		ph7_class *pAggregate = DtClass(&(*pVm),"IteratorAggregate");` |
|   7930 | 9354 | `		if( pPeriod == 0 \|\| pAggregate == 0 ){` |
|    ! 0 | 9355 | `			return SXERR_NOTFOUND;` |
|      - | 9356 | `		}` |
|   7930 | 9357 | `		rc = PH7_ClassImplement(pPeriod,pAggregate);` |
|      - | 9358 | `	}` |
|   7930 | 9359 | `	return rc;` |
|   3962 | 9360 | `}` |
|      - | 9361 |  |
|      - | 9362 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 9363 |  |
|      - | 9364 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 9365 | `/* Tiny build: no DateTime family (builtin layer disabled) */` |
|      - | 9366 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){` |
|      - | 9367 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|      - | 9368 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|      - | 9369 | `	return SXRET_OK;` |
|      - | 9370 | `}` |
|      - | 9371 | `#endif` |
|      - | 9372 |  |
