# src/ph7/builtin_date_parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4514/4756 lines (94.91%)

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
|      - |   17 | ` * DateTime family (NEWPLAN band D slice 1): DateTimeInterface, DateTime,` |
|      - |   18 | ` * DateTimeImmutable, DateTimeZone (UTC + fixed offsets), date_create(),` |
|      - |   19 | ` * date_create_immutable(). Embedded-PHP chunk + C thunks, following the` |
|      - |   20 | ` * Reflection architecture (installed inside the bCompilingBuiltin window).` |
|      - |   21 | ` * Timezone SCOPE: UTC and fixed "+HH:MM" offsets only — no tz database` |
|      - |   22 | ` * (recorded §10 scope cut; named region zones throw like unknown zones).` |
|      - |   23 | ` * ======================================================================== */` |
|      - |   24 |  |
|      - |   25 | `/*` |
|      - |   26 | ` * Proleptic-Gregorian civil <-> day-count conversions (Howard Hinnant's` |
|      - |   27 | ` * algorithms): no time_t / libc dependence, correct far past 2038 and` |
|      - |   28 | ` * before 1970 on every platform. Day 0 == 1970-01-01.` |
|      - |   29 | ` */` |
|  24164 |   30 | `PH7_PRIVATE sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|      4 |   31 | `{` |
|      - |   32 | `	sxi64 era;` |
|      - |   33 | `	unsigned yoe,doy,doe;` |
|      - |   34 | `	/* Every step is spelled in UNSIGNED arithmetic: the year reaching here is` |
|      - |   35 | `	 * whatever the string held, php's own answer for one past the clock is the` |
|      - |   36 | `	 * WRAP below, and a signed overflow on the way to it is undefined (this` |
|      - |   37 | `	 * build gates on UBSan). The bits are the same either way. */` |
|  24168 |   38 | `	y = (sxi64)((sxu64)y - (sxu64)(m <= 2));` |
|  24168 |   39 | `	era = (y >= 0 ? y : (sxi64)((sxu64)y - 399u)) / 400;` |
|  24168 |   40 | `	yoe = (unsigned)((sxu64)y - (sxu64)era * 400u);` |
|  24168 |   41 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|  24168 |   42 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|      - |   43 | `	/* Unsigned tail: php's expanded ISO year has no width limit, and php's own` |
|      - |   44 | `	 * answer for one past the clock is a WRAP of the seconds it converts to --` |
|      - |   45 | ``	 * `new DateTime('-999999999999-01-01')` reads back as year 169108098508`` |
|      - |   46 | `	 * there. Wrapping through sxu64 reproduces that instead of overflowing a` |
|      - |   47 | `	 * signed product, which is undefined. */` |
|  24168 |   48 | `	return (sxi64)((sxu64)era * 146097u + (sxu64)doe - 719468u);` |
|      4 |   49 | `}` |
|  19156 |   50 | `PH7_PRIVATE void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|      4 |   51 | `{` |
|      - |   52 | `	sxi64 era;` |
|      - |   53 | `	unsigned doe,yoe,doy,mp;` |
|  19160 |   54 | `	z = (sxi64)((sxu64)z + 719468u);` |
|  19160 |   55 | `	era = (z >= 0 ? z : (sxi64)((sxu64)z - 146096u)) / 146097;` |
|  19160 |   56 | `	doe = (unsigned)((sxu64)z - (sxu64)era * 146097u);` |
|  19160 |   57 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|  19160 |   58 | `	*py = (sxi64)((sxu64)yoe + (sxu64)era * 400u);` |
|  19160 |   59 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|  19160 |   60 | `	mp = (5 * doy + 2) / 153;` |
|  19160 |   61 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|  19160 |   62 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|  19160 |   63 | `	if( *pm <= 2 ){` |
|   6809 |   64 | `		*py = (sxi64)((sxu64)*py + 1u);` |
|   3403 |   65 | `	}` |
|  19160 |   66 | `}` |
|  67330 |   67 | `PH7_PRIVATE sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|      4 |   68 | `{` |
|  67334 |   69 | `	sxi64 q = a / b;` |
|  67334 |   70 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|    315 |   71 | `		q--;` |
|    157 |   72 | `	}` |
|  67334 |   73 | `	return q;` |
|      4 |   74 | `}` |
|      - |   75 | `/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z).` |
|      - |   76 | ` * Shared with builtin_date.c, whose procedural doors used to reach for the` |
|      - |   77 | ` * platform's gmtime() and lose every year past an int. */` |
|   4258 |   78 | `PH7_PRIVATE void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)` |
|      4 |   79 | `{` |
|      - |   80 | `	/* Both steps are written to stay DEFINED at the ends of php's clock:` |
|      - |   81 | ``	 * `iTs + iOff` overflows for a timestamp near the int64 floor, and so does`` |
|      - |   82 | ``	 * rebuilding the day's start as `days * 86400` (UBSan caught the second at`` |
|      - |   83 | `	 * setTimestamp(PHP_INT_MIN)->format()). The remainder gives the same` |
|      - |   84 | `	 * seconds-of-day with no product at all. */` |
|   4262 |   85 | `	sxi64 t = (sxi64)((sxu64)iTs + (sxu64)iOff);` |
|   4262 |   86 | `	sxi64 days = DtFloorDiv(t,86400);` |
|   4262 |   87 | `	sxi64 secs = t % 86400;` |
|      - |   88 | `	sxi64 y;` |
|      - |   89 | `	int mo,d;` |
|   4262 |   90 | `	if( secs < 0 ){` |
|     91 |   91 | `		secs += 86400;` |
|     45 |   92 | `	}` |
|   4262 |   93 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|   4262 |   94 | `	pTm->tm_sec  = (int)(secs % 60);` |
|   4262 |   95 | `	pTm->tm_min  = (int)((secs / 60) % 60);` |
|   4262 |   96 | `	pTm->tm_hour = (int)(secs / 3600);` |
|   4262 |   97 | `	pTm->tm_mday = d;` |
|   4262 |   98 | `	pTm->tm_mon  = mo - 1;` |
|   4262 |   99 | `	pTm->tm_year = y;` |
|   4262 |  100 | `	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */` |
|   4262 |  101 | `	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));` |
|   4262 |  102 | `	pTm->tm_isdst = 0;` |
|   4262 |  103 | `	pTm->tm_zone = zZone;` |
|   4262 |  104 | `	pTm->tm_gmtoff = (long)iOff;` |
|   4262 |  105 | `}` |
|   4816 |  106 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|      3 |  107 | `{` |
|      - |  108 | `	/* Unsigned throughout: a year outside the clock's own range (the parser` |
|      - |  109 | `	 * accepts php's expanded form, which has no width limit) would otherwise` |
|      - |  110 | `	 * overflow this product, which is undefined rather than merely wrong. */` |
|   4819 |  111 | `	sxu64 t = (sxu64)DtDaysFromCivil(y,mo,d) * 86400u;` |
|   4819 |  112 | `	t += (sxu64)((sxi64)h*3600 + (sxi64)mi*60 + s - iOff);` |
|   4819 |  113 | `	return (sxi64)t;` |
|      3 |  114 | `}` |
|      - |  115 | `/*` |
|      - |  116 | ` * php's date string parse is a FIELD parse. timelib fills a civil y/m/d/h/i/s/us` |
|      - |  117 | ` * vector plus a SEPARATE relative one and applies NOTHING until the whole string` |
|      - |  118 | ` * has been read, which is what makes the written ORDER of a relative string` |
|      - |  119 | `` * irrelevant there -- `+1 day +1 month` and `+1 month +1 day` are one answer --`` |
|      - |  120 | ` * and what puts the months on the day before the days move it. PHL applied every` |
|      - |  121 | `` * unit to the clock as it scanned, so `+30 days +1 month` was a day or two off.`` |
|      - |  122 | ` *` |
|      - |  123 | ` * A field the string never mentions stays DT_UNSET and is filled from the BASE` |
|      - |  124 | ` * moment afterwards (php's timelib_fill_holes), which is what lets a bare month` |
|      - |  125 | ` * name keep the base day and a bare date keep the base time of day.` |
|      - |  126 | ` */` |
|      - |  127 | `/* php's TIMELIB_UNSET, and the NUMBER matters as well as the marking: its` |
|      - |  128 | ` * normalizer carries an unset field into the one above it like any other, so` |
|      - |  129 | ` * the date a half-read clock ends up publishing is a function of this value. */` |
|      - |  130 | `#define DT_UNSET ((sxi64)-9999999)` |
|      - |  131 | `typedef struct dt_parsed dt_parsed;` |
|      - |  132 | `struct dt_parsed` |
|      - |  133 | `{` |
|      - |  134 | `	sxi64 y,m,d;                    /* absolute date fields, or DT_UNSET */` |
|      - |  135 | `	sxi64 h,i,s,us;                 /* absolute time fields, or DT_UNSET */` |
|      - |  136 | `	sxi64 ry,rm,rd,rh,ri,rs,rus;    /* the relative vector */` |
|      - |  137 | `	int bHaveDate;                  /* the string set an absolute date element */` |
|      - |  138 | `	int nTimeTok;                   /* php's have_time: 0 = the string named no time` |
|      - |  139 | `	                                 * of day, 1 = it named one, 2 = a second bare` |
|      - |  140 | `	                                 * digit run then read as a YEAR */` |
|      - |  141 | `	int bWday;                      /* the string named a weekday */` |
|      - |  142 | `	int iWday;                      /* that weekday, 0=Sunday..6 -- or NEGATIVE,` |
|      - |  143 | ``	                                 * which is what `ago` makes of it */`` |
|      - |  144 | `	int iWdayBehavior;              /* php's 0 (next/last), 1 (bare name), 2 (... this week) */` |
|      - |  145 | `	int iFirstLast;                 /* php's first_last_day_of: 0 none, 1 first, 2 last */` |
|      - |  146 | ``	int bWdayOf;                    /* php's `first monday of` special: a weekday`` |
|      - |  147 | `	                                 * hunted inside the MONTH the rest of the` |
|      - |  148 | `	                                 * string lands on, rather than from the day */` |
|      - |  149 | `	int iWdayOfNext;                /* ...and whether it starts from the month` |
|      - |  150 | ``	                                 * AFTER, which is php's `last` and `this` */`` |
|      - |  151 | ``	int bWeekdays;                  /* php's `weekday` special was named */`` |
|      - |  152 | `	sxi64 iWeekdays;                /* ... this many BUSINESS days */` |
|      - |  153 | `	sxi32 iOff;                     /* the offset in force */` |
|      - |  154 | `	int bOffSet;                    /* 0 = the string named no zone, 1 = an offset,` |
|      - |  155 | `	                                 * 2 = a NAME the string spelled (zZone below) */` |
|      - |  156 | `	const char *zZone;              /* that name, a literal: "Z", "UTC", "GMT" */` |
|      - |  157 | `	int nZone;` |
|      - |  158 | `	int bZoneIdent;                 /* php's timezone_type 3 rather than 2 -- only` |
|      - |  159 | `	                                 * "UTC" spelled in that exact case */` |
|      - |  160 | `	int nZoneTok;                   /* how many zone TOKENS the string spelled: the` |
|      - |  161 | `	                                 * second is ignored and the third refused */` |
|      - |  162 | ``	int bEpoch;                     /* the string named an `@epoch`, which is the`` |
|      - |  163 | `	                                 * one form modify() lets name a ZONE */` |
|      - |  164 | `	int bUsUnset;                   /* php's vector leaves its microseconds UNSET` |
|      - |  165 | `	                                 * where PHL writes a zero for the CLOCK's` |
|      - |  166 | `	                                 * sake: the two nocolon arms php reaches` |
|      - |  167 | `	                                 * without its HAVE_TIME ever running, a bare` |
|      - |  168 | `	                                 * four-digit clock and a bare year. It is` |
|      - |  169 | ``	                                 * what date_parse() shows as `false`. */`` |
|      - |  170 | `	int bHaveRel;                   /* php's have_relative: the string spelled a` |
|      - |  171 | `	                                 * RELATIVE element, which is what puts the` |
|      - |  172 | ``	                                 * `relative` block in date_parse()'s answer.`` |
|      - |  173 | ``	                                 * `now`, `today` and a bare `ago` do not. */`` |
|      - |  174 | `	/* Warnings a RULE raises but only the scan can publish. They ride the vector` |
|      - |  175 | `	 * so that the copy a longest-match probe runs on discards them with itself:` |
|      - |  176 | `	 * a probe that succeeds makes its caller stand down and the real rule raises` |
|      - |  177 | `	 * them again, a probe that fails raised nothing. */` |
|      - |  178 | `	int nWarnPend;` |
|      - |  179 | `	int aWarnPos[PH7_DT_MAX_WARN];` |
|      - |  180 | `	const char *azWarn[PH7_DT_MAX_WARN];` |
|      - |  181 | `};` |
|      - |  182 | `/* php's add_warning, held until the scan can publish it. */` |
|    140 |  183 | `static void DtWarnPend(dt_parsed *p,int iPos,const char *zMsg)` |
|      1 |  184 | `{` |
|    141 |  185 | `	if( p->nWarnPend < PH7_DT_MAX_WARN ){` |
|    141 |  186 | `		p->aWarnPos[p->nWarnPend] = iPos;` |
|    141 |  187 | `		p->azWarn[p->nWarnPend] = zMsg;` |
|     70 |  188 | `	}` |
|    141 |  189 | `	p->nWarnPend++;` |
|    141 |  190 | `}` |
|      - |  191 | `/* Wrapping add: a relative vector holds whatever the string spelled, and php's` |
|      - |  192 | ` * own answer past the int64 ceiling is garbage of its own -- but the OVERFLOW` |
|      - |  193 | ` * would be undefined here, and this build gates on UBSan. */` |
|  90476 |  194 | `static sxi64 DtWAdd(sxi64 a,sxi64 b)` |
|      3 |  195 | `{` |
|  90479 |  196 | `	return (sxi64)((sxu64)a + (sxu64)b);` |
|      3 |  197 | `}` |
|    452 |  198 | `static sxi64 DtWMul(sxi64 a,sxi64 b)` |
|      1 |  199 | `{` |
|    453 |  200 | `	return (sxi64)((sxu64)a * (sxu64)b);` |
|      1 |  201 | `}` |
|   5270 |  202 | `static void DtFieldsInit(dt_parsed *p,sxi32 iBaseOff)` |
|      3 |  203 | `{` |
|   5273 |  204 | `	p->y = p->m = p->d = DT_UNSET;` |
|   5273 |  205 | `	p->h = p->i = p->s = p->us = DT_UNSET;` |
|   5273 |  206 | `	p->ry = p->rm = p->rd = p->rh = p->ri = p->rs = p->rus = 0;` |
|   5273 |  207 | `	p->bHaveDate = p->nTimeTok = 0;` |
|   5273 |  208 | `	p->bWday = 0;` |
|   5273 |  209 | `	p->iWday = 0;` |
|   5273 |  210 | `	p->iWdayBehavior = 0;` |
|   5273 |  211 | `	p->iFirstLast = 0;` |
|   5273 |  212 | `	p->bWdayOf = 0;` |
|   5273 |  213 | `	p->iWdayOfNext = 0;` |
|   5273 |  214 | `	p->bWeekdays = 0;` |
|   5273 |  215 | `	p->iWeekdays = 0;` |
|   5273 |  216 | `	p->iOff = iBaseOff;` |
|   5273 |  217 | `	p->bOffSet = 0;` |
|   5273 |  218 | `	p->zZone = 0;` |
|   5273 |  219 | `	p->nZone = 0;` |
|   5273 |  220 | `	p->bZoneIdent = 0;` |
|   5273 |  221 | `	p->nZoneTok = 0;` |
|   5273 |  222 | `	p->bEpoch = 0;` |
|   5273 |  223 | `	p->nWarnPend = 0;` |
|   5273 |  224 | `	p->bHaveRel = 0;` |
|   5273 |  225 | `	p->bUsUnset = 0;` |
|   5273 |  226 | `}` |
|      - |  227 | `/* php's TIMELIB_UNHAVE_TIME: the clock is ZEROED rather than unset, and the` |
|      - |  228 | ``  * string still counts as carrying no time of its own -- which is why `tomorrow` `` |
|      - |  229 | ` * lands on midnight even through modify(), whose other fields keep the` |
|      - |  230 | ` * receiver's. */` |
|    734 |  231 | `static void DtUnhaveTime(dt_parsed *p)` |
|      2 |  232 | `{` |
|    736 |  233 | `	p->h = p->i = p->s = p->us = 0;` |
|    736 |  234 | `	p->bUsUnset = 0;` |
|    736 |  235 | `	p->nTimeTok = 0;` |
|    736 |  236 | `}` |
|  37672 |  237 | `static void DtCarry(sxi64 *pLo,sxi64 *pHi,sxi64 iUnit)` |
|      3 |  238 | `{` |
|  37675 |  239 | `	sxi64 c = DtFloorDiv(*pLo,iUnit);` |
|  37675 |  240 | `	*pLo -= c * iUnit;` |
|  37675 |  241 | `	*pHi = DtWAdd(*pHi,c);` |
|  37675 |  242 | `}` |
|      - |  243 | `/*` |
|      - |  244 | ` * php's timelib_do_normalize: carry the clock up into the days, fold the months` |
|      - |  245 | ` * into the years, then let the civil day count absorb whatever the day field` |
|      - |  246 | ` * holds. That formula is linear in d, so an out-of-range day simply lands in the` |
|      - |  247 | `` * month after -- which is php's `2020-01-31 +1 month` == 2020-03-02.`` |
|      - |  248 | ` */` |
|   9932 |  249 | `static sxi64 DtDayCountOf(sxi64 y,sxi64 m,sxi64 d)` |
|      3 |  250 | `{` |
|   9935 |  251 | `	sxi64 c = DtFloorDiv(m - 1,12);` |
|   9935 |  252 | `	sxi64 mm = (m - 1) - c*12 + 1;` |
|   9935 |  253 | `	return DtWAdd(DtDaysFromCivil(DtWAdd(y,c),(int)mm,1),d - 1);` |
|      3 |  254 | `}` |
|   9418 |  255 | `static void DtNormalize(dt_parsed *p)` |
|      3 |  256 | `{` |
|      - |  257 | `	sxi64 days,yy;` |
|      - |  258 | `	int mm,dd;` |
|   9421 |  259 | `	DtCarry(&p->us,&p->s,1000000);` |
|   9421 |  260 | `	DtCarry(&p->s,&p->i,60);` |
|   9421 |  261 | `	DtCarry(&p->i,&p->h,60);` |
|   9421 |  262 | `	DtCarry(&p->h,&p->d,24);` |
|   9421 |  263 | `	days = DtDayCountOf(p->y,p->m,p->d);` |
|   9421 |  264 | `	DtCivilFromDays(days,&yy,&mm,&dd);` |
|   9421 |  265 | `	p->y = yy;` |
|   9421 |  266 | `	p->m = mm;` |
|   9421 |  267 | `	p->d = dd;` |
|   9421 |  268 | `}` |
|      - |  269 | `/* php's day of week, 0 = Sunday, from a day count (1970-01-01 was a Thursday). */` |
|    608 |  270 | `static int DtDowOf(sxi64 days)` |
|      1 |  271 | `{` |
|    609 |  272 | `	return (int)(((days + 4) % 7 + 7) % 7);` |
|      1 |  273 | `}` |
|      - |  274 | `/*` |
|      - |  275 | ` * php's do_adjust_for_weekday, which runs BEFORE the relative vector is applied` |
|      - |  276 | `` * -- so `+30 days next monday` moves to the Monday and then adds the days, in`` |
|      - |  277 | ``  * either written order. The three behaviours are php's own: 0 for `next`/`last` `` |
|      - |  278 | `` * (a matching base day is skipped), 1 for a bare name or `this monday` (a`` |
|      - |  279 | `` * matching base day is kept), and 2 for the `... this week` spellings, which`` |
|      - |  280 | ` * count from the WEEK rather than from the day.` |
|      - |  281 | ` */` |
|    358 |  282 | `static void DtAdjustWeekday(dt_parsed *p)` |
|      1 |  283 | `{` |
|    359 |  284 | `	sxi64 dow = DtDowOf(DtDayCountOf(p->y,p->m,p->d));` |
|    359 |  285 | `	sxi64 wd = p->iWday,diff;` |
|    359 |  286 | `	if( p->iWdayBehavior == 2 ){` |
|      - |  287 | `		/* php's two corrections: a Sunday base counts as the week's END, and a` |
|      - |  288 | `		 * Sunday target asked for from any other day is the week's end too. */` |
|     81 |  289 | `		if( dow == 0 && wd != 0 ){ wd -= 7; }` |
|     81 |  290 | `		if( wd == 0 && dow != 0 ){ wd = 7; }` |
|     81 |  291 | `		p->d = p->d - dow + wd;` |
|     81 |  292 | `		return;` |
|      - |  293 | `	}` |
|    279 |  294 | `	if( wd < 0 ){` |
|      - |  295 | ``		/* php's mirror of the hunt, which only `ago` reaches: it turns the target`` |
|      - |  296 | ``		 * weekday negative, and `next monday ago` is the Monday before. */`` |
|      9 |  297 | `		sxi64 nwd = -wd;` |
|      9 |  298 | `		p->d = DtWAdd(p->d,-(7 - (nwd - dow)));` |
|      9 |  299 | `		return;` |
|      - |  300 | `	}` |
|    271 |  301 | `	diff = wd - dow;` |
|    271 |  302 | `	if( (p->rd < 0 && diff < 0) \|\| (p->rd >= 0 && diff <= -p->iWdayBehavior) ){` |
|    147 |  303 | `		diff += 7;` |
|     73 |  304 | `	}` |
|    271 |  305 | `	p->d = DtWAdd(p->d,diff);` |
|    180 |  306 | `}` |
|      - |  307 | `/*` |
|      - |  308 | `` * php's `weekday` special: a count of BUSINESS days, which php applies before`` |
|      - |  309 | ` * everything else. Whole fives are whole weeks (the day of the week is kept),` |
|      - |  310 | ` * the remainder walks past the weekend, and a count that lands on one is pushed` |
|      - |  311 | ` * off it -- forward to Monday when the count is zero, back to Friday when a` |
|      - |  312 | ` * positive count ends there.` |
|      - |  313 | ` */` |
|    156 |  314 | `static void DtAdjustWeekdays(dt_parsed *p)` |
|      1 |  315 | `{` |
|    157 |  316 | `	sxi64 dow = DtDowOf(DtDayCountOf(p->y,p->m,p->d));` |
|    157 |  317 | `	sxi64 count = p->iWeekdays,rem;` |
|    157 |  318 | `	p->d = DtWAdd(p->d,(count / 5) * 7);` |
|    157 |  319 | `	rem = count % 5;` |
|    157 |  320 | `	if( count == 0 ){` |
|     17 |  321 | `		if( dow == 0 ){ p->d += 1; }` |
|     15 |  322 | `		else if( dow == 6 ){ p->d += 2; }` |
|     17 |  323 | `		return;` |
|      - |  324 | `	}` |
|    141 |  325 | `	if( count > 0 ){` |
|     71 |  326 | `		if( rem == 0 ){` |
|     15 |  327 | `			if( dow == 0 ){ p->d -= 2; }` |
|     13 |  328 | `			else if( dow == 6 ){ p->d -= 1; }` |
|     15 |  329 | `			return;` |
|      - |  330 | `		}` |
|     57 |  331 | `		if( dow == 6 ){ p->d += 2; rem--; dow = 1; }` |
|     51 |  332 | `		else if( dow == 0 ){ p->d += 1; rem--; dow = 1; }` |
|     57 |  333 | `		if( rem > 0 ){` |
|     53 |  334 | `			if( dow + rem > 5 ){ p->d += 2; }` |
|     53 |  335 | `			p->d += rem;` |
|     26 |  336 | `		}` |
|     57 |  337 | `		return;` |
|      - |  338 | `	}` |
|     71 |  339 | `	if( rem == 0 ){` |
|     17 |  340 | `		if( dow == 0 ){ p->d += 1; }` |
|     15 |  341 | `		else if( dow == 6 ){ p->d += 2; }` |
|     17 |  342 | `		return;` |
|      - |  343 | `	}` |
|     55 |  344 | `	if( dow == 0 ){ p->d -= 2; rem++; dow = 5; }` |
|     49 |  345 | `	else if( dow == 6 ){ p->d -= 1; rem++; dow = 5; }` |
|     55 |  346 | `	if( rem < 0 ){` |
|     51 |  347 | `		if( dow + rem < 1 ){ p->d -= 2; }` |
|     51 |  348 | `		p->d += rem;` |
|     25 |  349 | `	}` |
|     79 |  350 | `}` |
|      - |  351 | ``/* php's `first\|last day of`: the first is the day 1, the last is day 0 of the`` |
|      - |  352 | ` * month AFTER -- which the normalizer then reads back as the month's own last. */` |
|   8828 |  353 | `static void DtFirstLastDay(dt_parsed *p)` |
|      3 |  354 | `{` |
|   8831 |  355 | `	if( p->iFirstLast == 1 ){` |
|    113 |  356 | `		p->d = 1;` |
|   8775 |  357 | `	}else if( p->iFirstLast == 2 ){` |
|     57 |  358 | `		p->d = 0;` |
|     57 |  359 | `		p->m = DtWAdd(p->m,1);` |
|     28 |  360 | `	}` |
|   8831 |  361 | `}` |
|      - |  362 | `/*` |
|      - |  363 | ` * php's timelib_fill_holes + timelib_update_ts over a parsed vector: fill what` |
|      - |  364 | ` * the string left unset from the base moment, then apply in php's ORDER --` |
|      - |  365 | ``  * weekday first, the whole relative vector next, and the `first\|last day of` `` |
|      - |  366 | ` * flag LAST, which is why that flag swallows any relative DAYS beside it` |
|      - |  367 | `` * (`first day of next month +40 days` is the 1st) while the hours still count.`` |
|      - |  368 | ` *` |
|      - |  369 | ` * DT_PARSE_OVERRIDE_TIME is php's flag of the same name: modify() writes only` |
|      - |  370 | ` * the fields the string really set, so a bare date there keeps the receiver's` |
|      - |  371 | ` * time of day, where a fresh parse zeroes it.` |
|      - |  372 | ` */` |
|      - |  373 | `#define DT_PARSE_OVERRIDE_TIME 0x01` |
|      - |  374 | `/* DT_PARSE_KEEP_ZONE is modify()'s other half: php copies the FIELDS the string` |
|      - |  375 | ` * parsed into the object and nothing else, so a zone the modifier names moves` |
|      - |  376 | `` * nothing -- `$d->modify('2020-01-01T12:00:00Z')` on a +05:00 date is noon at`` |
|      - |  377 | `` * +05:00 there. The one exception is the `@epoch` form, which names an absolute`` |
|      - |  378 | ` * INSTANT (and, in php, re-zones the object to +00:00 with it). */` |
|      - |  379 | `#define DT_PARSE_KEEP_ZONE     0x02` |
|      - |  380 | `/*` |
|      - |  381 | `` * php's DAY_OF_WEEK_IN_MONTH special -- `first monday of`, `last sunday of`` |
|      - |  382 | `` * february 2020` -- and it is not a rule of its own so much as a way IN to the`` |
|      - |  383 | ` * weekday hunt the vector already carries: the MONTH is settled first (the` |
|      - |  384 | ` * relative MONTHS are applied and consumed here, though not the years, which` |
|      - |  385 | ` * ride on to the ordinary pass), the day becomes that month's 1st, and the` |
|      - |  386 | ` * ordinary hunt walks forward to the weekday from there. The count rides the` |
|      - |  387 | `` * relative DAYS -- a week per count past the first -- so `tenth tuesday of` is`` |
|      - |  388 | ` * the first one plus nine weeks, and the relative days a string spells beside` |
|      - |  389 | ` * it simply add on.` |
|      - |  390 | ` *` |
|      - |  391 | `` * `last` and `previous` are the same rule one month on with a week taken off`` |
|      - |  392 | ` * (php's own encoding: the 1st of the NEXT month, then -7 from the weekday it` |
|      - |  393 | `` * finds), and `this` is that month shift with nothing taken off, which is why it`` |
|      - |  394 | ` * answers the FIRST such weekday of the month after.` |
|      - |  395 | ` */` |
|     76 |  396 | `static void DtWeekdayOfMonth(dt_parsed *p)` |
|      1 |  397 | `{` |
|     77 |  398 | `	p->m = DtWAdd(p->m,DtWAdd(p->rm,(sxi64)p->iWdayOfNext));` |
|     77 |  399 | `	p->rm = 0;` |
|     77 |  400 | `	p->d = 1;` |
|     77 |  401 | `	DtNormalize(p);` |
|     77 |  402 | `}` |
|   4414 |  403 | `static sxi64 DtApplyFields(dt_parsed *p,sxi64 iBaseTs,sxi32 iBaseOff,int iBaseUs,` |
|      - |  404 | `	int iFlags,int *pUs)` |
|      3 |  405 | `{` |
|   4417 |  406 | `	sxi64 days = DtFloorDiv(iBaseTs + iBaseOff,86400);` |
|   4417 |  407 | `	sxi64 tod  = (iBaseTs + iBaseOff) - days*86400;` |
|      - |  408 | `	sxi64 by;` |
|      - |  409 | `	int bm,bd;` |
|   4417 |  410 | `	DtCivilFromDays(days,&by,&bm,&bd);` |
|   4417 |  411 | `	if( !(iFlags & DT_PARSE_OVERRIDE_TIME) && p->bHaveDate && !p->nTimeTok ){` |
|    540 |  412 | `		p->h = p->i = p->s = p->us = 0;` |
|    269 |  413 | `	}` |
|   4417 |  414 | `	if( p->y  == DT_UNSET ){ p->y  = by; }` |
|   4417 |  415 | `	if( p->m  == DT_UNSET ){ p->m  = bm; }` |
|   4417 |  416 | `	if( p->d  == DT_UNSET ){ p->d  = bd; }` |
|   4417 |  417 | `	if( p->h  == DT_UNSET ){ p->h  = tod / 3600; }` |
|   4417 |  418 | `	if( p->i  == DT_UNSET ){ p->i  = (tod / 60) % 60; }` |
|   4417 |  419 | `	if( p->s  == DT_UNSET ){ p->s  = tod % 60; }` |
|   4417 |  420 | `	if( p->us == DT_UNSET ){ p->us = iBaseUs; }` |
|      - |  421 | `	/* php applies the flag TWICE, and both are visible: once here, so a weekday` |
|      - |  422 | ``	 * hunt and a relative month start from the month's edge (`last monday first`` |
|      - |  423 | ``	 * day of this month` never leaves January), and once at the end, which is what`` |
|      - |  424 | `	 * makes it swallow the relative DAYS beside it. */` |
|   4417 |  425 | `	if( p->bWdayOf ){` |
|     77 |  426 | `		DtWeekdayOfMonth(p);` |
|     38 |  427 | `	}` |
|   4417 |  428 | `	DtFirstLastDay(p);` |
|   4417 |  429 | `	DtNormalize(p);` |
|   4417 |  430 | `	if( p->bWday ){` |
|    359 |  431 | `		DtAdjustWeekday(p);` |
|    359 |  432 | `		DtNormalize(p);` |
|    179 |  433 | `	}` |
|   4417 |  434 | `	p->us = DtWAdd(p->us,p->rus);` |
|   4417 |  435 | `	p->s  = DtWAdd(p->s,p->rs);` |
|   4417 |  436 | `	p->i  = DtWAdd(p->i,p->ri);` |
|   4417 |  437 | `	p->h  = DtWAdd(p->h,p->rh);` |
|   4417 |  438 | `	p->d  = DtWAdd(p->d,p->rd);` |
|   4417 |  439 | `	p->m  = DtWAdd(p->m,p->rm);` |
|   4417 |  440 | `	p->y  = DtWAdd(p->y,p->ry);` |
|   4417 |  441 | `	DtFirstLastDay(p);` |
|      - |  442 | `	/* php's business-day count runs AFTER the relative vector AND after the` |
|      - |  443 | ``	 * `first\|last day of` flag, so `last weekday -8 months` walks back from the`` |
|      - |  444 | ``	 * month it landed on and `first day of next month +9 weekdays` counts from`` |
|      - |  445 | `	 * that 1st. */` |
|   4417 |  446 | `	if( p->bWeekdays ){` |
|    157 |  447 | `		DtNormalize(p);` |
|    157 |  448 | `		DtAdjustWeekdays(p);` |
|     78 |  449 | `	}` |
|   4417 |  450 | `	DtNormalize(p);` |
|   4417 |  451 | `	*pUs = (int)p->us;` |
|   6624 |  452 | `	return DtMakeTs(p->y,(int)p->m,(int)p->d,(int)p->h,(int)p->i,(int)p->s,` |
|   4414 |  453 | `		((iFlags & DT_PARSE_KEEP_ZONE) && !p->bEpoch) ? iBaseOff : p->iOff);` |
|      3 |  454 | `}` |
|      - |  455 | `/*` |
|      - |  456 | ` * How WIDE a run of digits may be, which php bounds per grammar and PHL did not` |
|      - |  457 | ` * bound at all -- so a long run silently wrapped the int64 it was accumulated` |
|      - |  458 | `` * into (`@99999999999999999999` answered 7766279631452241919 here; UBSan called`` |
|      - |  459 | ` * the overflow what it is). Each limit is php's, measured:` |
|      - |  460 | ` *` |
|      - |  461 | `` *   an `@epoch`          18 digits, then "Number out of range"`` |
|      - |  462 | ` *   a RELATIVE number    13 digits (php's scanner answers gibberish past that --` |
|      - |  463 | ` *                        a 14-digit run comes back as ten digits' worth -- so` |
|      - |  464 | ` *                        PHL refuses instead of guessing, recorded in §7.4)` |
|      - |  465 | ` *   an ISO duration      12 digits, then "Unknown or bad format"` |
|      - |  466 | ` *` |
|      - |  467 | ` * The accumulators themselves stop adding past DT_DIGITS_SAFE so that COUNTING a` |
|      - |  468 | ` * run that will be refused cannot overflow on the way.` |
|      - |  469 | ` */` |
|      - |  470 | `#define DT_DIGITS_EPOCH 18` |
|      - |  471 | `#define DT_DIGITS_REL   13` |
|      - |  472 | `#define DT_DIGITS_ISO   12` |
|      - |  473 | `#define DT_DIGITS_SAFE  18` |
|      - |  474 | `/* Whole bands below the position encoding, so one refusal cannot be mistaken for` |
|      - |  475 | ` * another: the bare negative is php's "Double time specification", a band lower` |
|      - |  476 | ` * is "Number out of range", one lower still "Double date specification", and the` |
|      - |  477 | ` * lowest "Double timezone specification". */` |
|      - |  478 | `#define DT_ERR_RANGE    1000000` |
|      - |  479 | `#define DT_ERR_DDATE    2000000` |
|      - |  480 | `#define DT_ERR_DZONE    3000000` |
|      - |  481 | `#define DT_ERR_TZID     4000000` |
|      - |  482 | `#define DT_ERR_UNEXPDATA 5000000` |
|      - |  483 | `#define DT_ERR_EMPTY    6000000` |
|      - |  484 | `/*` |
|      - |  485 | `` * php refuses a SECOND absolute date outright -- `2020-01-01 january` and`` |
|      - |  486 | `` * `20240102 20240102` are both "Double date specification" there, reported at the`` |
|      - |  487 | ` * offending token's start. Every date rule marks its answer through this; the bare` |
|      - |  488 | `` * four-digit YEAR does not, which is why `1234 5678` is a year beside a clock.`` |
|      - |  489 | ` */` |
|   3144 |  490 | `static int DtMarkDate(dt_parsed *p,const char *zTok,const char *zIn)` |
|      2 |  491 | `{` |
|   3146 |  492 | `	if( p->bHaveDate ){` |
|     57 |  493 | `		return -((int)(zTok - zIn) + 1) - DT_ERR_DDATE;` |
|      - |  494 | `	}` |
|   3090 |  495 | `	p->bHaveDate = 1;` |
|   3090 |  496 | `	return 0;` |
|   1574 |  497 | `}` |
|      - |  498 | `/*` |
|      - |  499 | ` * Read a fractional-seconds part at z (which points at the '.'): up to 6 digits` |
|      - |  500 | ` * become microseconds (right-padded to 6, extra digits ignored). Advances *pz.` |
|      - |  501 | ` */` |
|    262 |  502 | `static int DtReadFraction(const char **pz,const char *zEnd)` |
|      1 |  503 | `{` |
|    263 |  504 | `	const char *z = *pz;` |
|    263 |  505 | `	int us = 0,n = 0;` |
|    263 |  506 | `	z++; /* skip '.' */` |
|   1709 |  507 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|   1447 |  508 | `		if( n < 6 ){ us = us*10 + (z[0]-'0'); n++; }` |
|   1447 |  509 | `		z++;` |
|      1 |  510 | `	}` |
|    389 |  511 | `	while( n < 6 ){ us *= 10; n++; }` |
|    263 |  512 | `	*pz = z;` |
|    263 |  513 | `	return us;` |
|      1 |  514 | `}` |
|      - |  515 | `/*` |
|      - |  516 | ` * Read one of php's time-of-day FIELDS at z: one or two digits, greedily -- the` |
|      - |  517 | ` * two-digit reading is taken when its value is in range and the one-digit reading` |
|      - |  518 | `` * otherwise, which is what makes `12:60` php's 12:06 with a stray `0` left over`` |
|      - |  519 | `` * (and the error then lands on that `0`, not on the minute). Answers the digits`` |
|      - |  520 | ` * consumed, or 0 when there is no field here.` |
|      - |  521 | ` */` |
|   9502 |  522 | `static int DtReadField(const char *z,const char *zEnd,int iMax,int *pVal)` |
|      3 |  523 | `{` |
|      - |  524 | `	int v;` |
|   9505 |  525 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|   8006 |  526 | `	if( z+1 < zEnd && SyisDigit(z[1]) ){` |
|   7216 |  527 | `		v = (z[0]-'0')*10 + (z[1]-'0');` |
|   7216 |  528 | `		if( v <= iMax ){ *pVal = v; return 2; }` |
|    138 |  529 | `	}` |
|   1068 |  530 | `	*pVal = z[0]-'0';` |
|   1068 |  531 | `	return 1;` |
|   4754 |  532 | `}` |
|      - |  533 | `/*` |
|      - |  534 | `` * php's MERIDIAN token, the twelve-hour clock's half: `am` or `pm` in any case,`` |
|      - |  535 | ` * with an optional dot after either letter, and nothing but whitespace or the` |
|      - |  536 | `` * end of the string behind it -- so `3pm.` is three in the afternoon while`` |
|      - |  537 | `` * `3pm..`, `3pm,` and `3pmx` are not a time at all.`` |
|      - |  538 | ` *` |
|      - |  539 | ` * Answers the bytes it takes (0 for anything else) and sets *pbPm.` |
|      - |  540 | ` */` |
|   2660 |  541 | `static int DtMeridian(const char *z,const char *zEnd,int *pbPm)` |
|      2 |  542 | `{` |
|      - |  543 | `	int n,c;` |
|   2662 |  544 | `	if( z >= zEnd ){` |
|   1697 |  545 | `		return 0;` |
|      - |  546 | `	}` |
|    966 |  547 | `	c = SyToLower(z[0]);` |
|    966 |  548 | `	if( c != 'a' && c != 'p' ){` |
|    826 |  549 | `		return 0;` |
|      - |  550 | `	}` |
|    141 |  551 | `	n = 1;` |
|    141 |  552 | `	if( &z[n] < zEnd && z[n] == '.' ){ n++; }` |
|    141 |  553 | `	if( &z[n] >= zEnd \|\| SyToLower(z[n]) != 'm' ){` |
|     11 |  554 | `		return 0;` |
|      - |  555 | `	}` |
|    131 |  556 | `	n++;` |
|    131 |  557 | `	if( &z[n] < zEnd && z[n] == '.' ){ n++; }` |
|      - |  558 | ``	/* php spells a trailing byte INTO the rule -- `meridian = [AaPp] "."? [Mm]`` |
|      - |  559 | ``	 * "."? [\000\t ]` -- and its buffer is NUL-padded, so the end of the string`` |
|      - |  560 | ``	 * satisfies it too. Nothing else does: `3pm,`, `3pm\nx` and `3pm.x` are no`` |
|      - |  561 | ``	 * meridian at all, and the `am` of `11:30am\nx` is read as a zone. */`` |
|    131 |  562 | `	if( &z[n] < zEnd && z[n] != ' ' && z[n] != '\t' && z[n] != 0 ){` |
|     27 |  563 | `		return 0;` |
|      - |  564 | `	}` |
|    105 |  565 | `	*pbPm = (c == 'p');` |
|    105 |  566 | `	return n;` |
|   1332 |  567 | `}` |
|      - |  568 | `/* The hour a twelve-hour clock means: php's noon is 12 and its midnight is 0,` |
|      - |  569 | ` * and every other hour is itself or itself plus twelve. */` |
|    100 |  570 | `static int DtHour12(int h,int bPm)` |
|      1 |  571 | `{` |
|    101 |  572 | `	if( h == 12 ){` |
|      7 |  573 | `		return bPm ? 12 : 0;` |
|      - |  574 | `	}` |
|     95 |  575 | `	return bPm ? h + 12 : h;` |
|     51 |  576 | `}` |
|      - |  577 | `/*` |
|      - |  578 | `` * php's time of day: `[t] H[H] (:\|.) M[M] [(:\|.) S[S] [.frac]]`. Either separator`` |
|      - |  579 | `` * is php's, and only the SECONDS take a fraction -- which is why `12:34.5` is`` |
|      - |  580 | ` * php's 12:34:05 and not a half second. Answers 1 when a time was read (the` |
|      - |  581 | ` * vector's clock set), 0 when there is none here, and a NEGATIVE DtParse error` |
|      - |  582 | ` * code -- the only one it can raise is php's "Double time specification".` |
|      - |  583 | ` */` |
|   5076 |  584 | `static int DtReadTimeOfDay(const char **pz,const char *zEnd,const char *zIn,dt_parsed *p)` |
|      3 |  585 | `{` |
|   5079 |  586 | `	const char *z = *pz;` |
|   5079 |  587 | `	const char *zTok = *pz;   /* the byte a refusal names: this token's first */` |
|   5079 |  588 | `	int h,mi,s = 0,n,bT = 0,bPm = 0,nMer,nMin = 0,nSec = 0,bFrac = 0;` |
|   5079 |  589 | `	sxi64 uSec = 0;` |
|   5079 |  590 | `	char cSep1 = 0,cSep2 = 0;` |
|   5079 |  591 | `	if( z < zEnd && (z[0]=='t' \|\| z[0]=='T') ){ z++; bT = 1; }` |
|   5079 |  592 | `	if( (n = DtReadField(z,zEnd,24,&h)) == 0 ){ return 0; }` |
|   3580 |  593 | `	if( z+n >= zEnd \|\| (z[n] != ':' && z[n] != '.') ){` |
|      - |  594 | ``		/* php's twelve-hour clock with no fields under it: `3pm`, `12 a.m.`.`` |
|      - |  595 | `` 		 * The hour has to be one a twelve-hour clock can name -- `0am`, `00am` `` |
|      - |  596 | ``		 * and `13pm` are refusals, not times -- and the `t` prefix belongs to`` |
|      - |  597 | ``		 * the ISO spelling alone, so `t3pm` is the hour 03 with `pm` left over. */`` |
|   1180 |  598 | `		const char *zMer = &z[n];` |
|   1180 |  599 | `		if( bT \|\| h < 1 \|\| h > 12 ){` |
|    315 |  600 | `			return 0;` |
|      - |  601 | `		}` |
|   1528 |  602 | `		while( zMer < zEnd && (zMer[0]==' ' \|\| zMer[0]=='\t') ){ zMer++; }` |
|    866 |  603 | `		if( (nMer = DtMeridian(zMer,zEnd,&bPm)) == 0 ){` |
|    792 |  604 | `			return 0;` |
|      - |  605 | `		}` |
|      - |  606 | `		/* php's TIMELIB_HAVE_TIME runs inside the ACTION, so the token is matched` |
|      - |  607 | `		 * and behind the cursor before the refusal is raised: the walk resumes` |
|      - |  608 | `		 * past it, not on it. */` |
|     75 |  609 | `		if( p->nTimeTok ){` |
|      5 |  610 | `			*pz = &zMer[nMer];` |
|      5 |  611 | `			return -((int)(zTok - zIn) + 1);` |
|      - |  612 | `		}` |
|     71 |  613 | `		p->h = DtHour12(h,bPm);` |
|     71 |  614 | `		p->i = p->s = p->us = 0;` |
|     71 |  615 | `		p->bUsUnset = 0;` |
|     71 |  616 | `		p->nTimeTok = 1;` |
|     71 |  617 | `		*pz = &zMer[nMer];` |
|     71 |  618 | `		return 1;` |
|      - |  619 | `	}` |
|   2401 |  620 | `	cSep1 = z[n];` |
|   2401 |  621 | `	z += n + 1;` |
|   2401 |  622 | `	if( (n = DtReadField(z,zEnd,59,&mi)) == 0 ){ return 0; }` |
|   2401 |  623 | `	nMin = n;` |
|   2401 |  624 | `	z += n;` |
|   2401 |  625 | `	if( z < zEnd && (z[0]==':' \|\| z[0]=='.') && z+1 < zEnd && SyisDigit(z[1]) ){` |
|   2005 |  626 | `		cSep2 = z[0];` |
|   2005 |  627 | `		n = DtReadField(&z[1],zEnd,60,&s);` |
|   2005 |  628 | `		nSec = n;` |
|   2005 |  629 | `		z += n + 1;` |
|   2005 |  630 | `		if( z < zEnd && z[0]=='.' && z+1 < zEnd && SyisDigit(z[1]) ){` |
|    263 |  631 | `			bFrac = 1;` |
|    263 |  632 | `			uSec = DtReadFraction(&z,zEnd);` |
|    131 |  633 | `		}` |
|   1002 |  634 | `	}` |
|      - |  635 | `	/* ...and the twelve-hour half of the same clock, which php spells behind the` |
|      - |  636 | ``	 * fields: `3:04pm`, `3:04:05 a.m.`. It is only a meridian when the hour is`` |
|      - |  637 | ``	 * one a twelve-hour clock names, so `13:00pm` keeps its 13 and leaves the`` |
|      - |  638 | ``	 * `pm` to the string, which then reads it as an unknown zone. */`` |
|      - |  639 | `	/* ...and php spells the LAST field of a twelve-hour clock with both its` |
|      - |  640 | `` 	 * digits: `3:04pm` and `3:4:05pm` are times where `3:4pm` and `3:04:5pm` `` |
|      - |  641 | ``	 * are not, and the `pm` those two leave behind is an unknown zone.`` |
|      - |  642 | `	 *` |
|      - |  643 | `	 * A FRACTION narrows the shape to php's one spelling of it: both separators` |
|      - |  644 | `	 * are colons, both fields carry both digits, and the meridian follows the` |
|      - |  645 | ``	 * fraction with nothing between them -- `3:04:05.5pm` is a time and`` |
|      - |  646 | ``	 * `3:04:05.5 pm` is not. */`` |
|   2400 |  647 | `	if( h >= 1 && h <= 12 && !bT` |
|   2760 |  648 | `	 && (bFrac ? (nMin == 2 && nSec == 2 && cSep1 == ':' && cSep2 == ':')` |
|    894 |  649 | `	           : ((nSec > 0 ? nSec : nMin) == 2)) ){` |
|   1797 |  650 | `		const char *zMer = z;` |
|   1797 |  651 | `		if( !bFrac ){` |
|   2100 |  652 | `			while( zMer < zEnd && (zMer[0]==' ' \|\| zMer[0]=='\t') ){ zMer++; }` |
|    862 |  653 | `		}` |
|   1797 |  654 | `		if( (nMer = DtMeridian(zMer,zEnd,&bPm)) > 0 ){` |
|     31 |  655 | `			h = DtHour12(h,bPm);` |
|     31 |  656 | `			z = &zMer[nMer];` |
|     15 |  657 | `		}` |
|    898 |  658 | `	}` |
|      - |  659 | `	/* php's "Double time specification". Its TIMELIB_HAVE_TIME sits in the` |
|      - |  660 | `	 * action, so the whole token -- meridian and fraction included -- has been` |
|      - |  661 | `	 * matched and the cursor is past it before the refusal is raised; the byte it` |
|      - |  662 | `	 * names is still the token's first. Nothing is written: the clock a second` |
|      - |  663 | `	 * time token would set is not php's answer either. */` |
|   2401 |  664 | `	if( p->nTimeTok ){` |
|     19 |  665 | `		*pz = z;` |
|     19 |  666 | `		return -((int)(zTok - zIn) + 1);` |
|      - |  667 | `	}` |
|      - |  668 | `	/* A time of day sets the whole clock, sub-second included: php writes the` |
|      - |  669 | `	 * microseconds of a time WITHOUT a fraction as zero. */` |
|   2383 |  670 | `	p->h = h;` |
|   2383 |  671 | `	p->i = mi;` |
|   2383 |  672 | `	p->s = s;` |
|   2383 |  673 | `	p->us = uSec;` |
|   2383 |  674 | `	p->bUsUnset = 0;` |
|   2383 |  675 | `	p->nTimeTok = 1;` |
|   2383 |  676 | `	*pz = z;` |
|   2383 |  677 | `	return 1;` |
|   2541 |  678 | `}` |
|      - |  679 | `/*` |
|      - |  680 | ` * The zone a string NAMED, recorded once: php reads a timezone token wherever it` |
|      - |  681 | `` * stands and the FIRST one wins outright, silently -- `+0200 +0300` is +02:00,`` |
|      - |  682 | `` * `UTC GMT` is UTC and `2020-01-01T12:00:00Z +0300` keeps its `Z`. Every door`` |
|      - |  683 | `` * that reads a zone (the attached ISO offset, a trailing name, `@epoch`'s UTC and`` |
|      - |  684 | ` * the standalone token below) goes through here, so the rule is one line.` |
|      - |  685 | ` *` |
|      - |  686 | ` * A THIRD one is php's refusal, though: it counts the tokens and raises "Double` |
|      - |  687 | ` * timezone specification" on the one past the ignored second, which is what makes` |
|      - |  688 | `` * `-123-03-04` -- three offsets to php's scanner, and no date at all -- an error`` |
|      - |  689 | `` * at its last `-`. Answers 1 for that, 0 otherwise.`` |
|      - |  690 | ` *` |
|      - |  691 | ` * zName NULL means a fixed OFFSET, whose name php builds from the offset itself.` |
|      - |  692 | ` */` |
|   1420 |  693 | `static int DtZoneCount(dt_parsed *p)` |
|      2 |  694 | `{` |
|   1422 |  695 | `	int n = p->nZoneTok;` |
|   1422 |  696 | `	if( n < 2 ){` |
|   1364 |  697 | `		p->nZoneTok = n + 1;` |
|    681 |  698 | `	}` |
|   1422 |  699 | `	return n == 0 ? 0 : (n == 1 ? 1 : -1);` |
|      2 |  700 | `}` |
|      - |  701 | `/* ...and the VALUE, written only for the token the rule above accepted. */` |
|    942 |  702 | `static void DtZoneStore(dt_parsed *p,sxi32 iOff,const char *zName,int nName,int bIdent)` |
|      2 |  703 | `{` |
|    944 |  704 | `	p->iOff = iOff;` |
|    944 |  705 | `	p->bOffSet = zName ? 2 : 1;` |
|    944 |  706 | `	p->zZone = zName;` |
|    944 |  707 | `	p->nZone = nName;` |
|    944 |  708 | `	p->bZoneIdent = bIdent;` |
|    944 |  709 | `}` |
|      - |  710 | ``/* Exactly two digits whose value is <= iMax -- php's `minutelz`/`secondlz`, and`` |
|      - |  711 | ` * the hour of its two-colon spelling. */` |
|    580 |  712 | `static int DtZoneLz(const char *z,const char *zEnd,int iMax)` |
|      1 |  713 | `{` |
|    700 |  714 | `	return zEnd-z >= 2 && SyisDigit(z[0]) && SyisDigit(z[1])` |
|    814 |  715 | `		&& (z[0]-'0')*10 + (z[1]-'0') <= iMax;` |
|      1 |  716 | `}` |
|      - |  717 | ``/* php's `hour24` (<= 24) and `minute` (<= 59) fields: one digit, or two when the`` |
|      - |  718 | `` * two-digit reading is in range -- so `96` is the hour 9 with a `6` left over and`` |
|      - |  719 | `` * `24` is the hour 24. Answers the digits taken. */`` |
|    556 |  720 | `static int DtZoneField(const char *z,const char *zEnd,int iMax)` |
|      1 |  721 | `{` |
|    557 |  722 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|    113 |  723 | `		return 0;` |
|      - |  724 | `	}` |
|    445 |  725 | `	if( z+1 < zEnd && SyisDigit(z[1]) && (z[0]-'0')*10 + (z[1]-'0') <= iMax ){` |
|    299 |  726 | `		return 2;` |
|      - |  727 | `	}` |
|    147 |  728 | `	return 1;` |
|    279 |  729 | `}` |
|      - |  730 | `/*` |
|      - |  731 | ` * How many bytes of digits and colons after the sign belong to php's UTC-offset` |
|      - |  732 | ` * token. php's scanner takes the LONGEST of three spellings and leaves the rest` |
|      - |  733 | `` * of the run to the string, which is why `+2460` is +02:46 with a `0` left over`` |
|      - |  734 | `` * and `+9999` is +99:00 with `99`:`` |
|      - |  735 | ` *` |
|      - |  736 | ` *   HH:MM:SS   two colons, two digits everywhere, hours <= 24 and seconds <= 60` |
|      - |  737 | ` *   HHMMSS     six digits, the same three bounds` |
|      - |  738 | ` *   H[H] [:] M[M]    the hour alone (0-99 when nothing follows it), or an hour` |
|      - |  739 | ` *                    <= 24 and a minute <= 59, the colon optional` |
|      - |  740 | ` *` |
|      - |  741 | ` * The VALUE is not read here: php computes it from the byte COUNT afterwards` |
|      - |  742 | `` * (DtZoneOffsetDigits), and the two disagree on purpose -- `+099` matches as the`` |
|      - |  743 | `` * hour `09` and the minute `9`, then counts as three digits and answers 0h99m.`` |
|      - |  744 | ` */` |
|    294 |  745 | `static int DtZoneCorrLen(const char *z,const char *zEnd)` |
|      1 |  746 | `{` |
|      - |  747 | `	int nH,nM;` |
|      - |  748 | `	const char *zm;` |
|    294 |  749 | `	if( zEnd-z >= 8 && z[2] == ':' && z[5] == ':'` |
|      8 |  750 | `	 && DtZoneLz(z,zEnd,24) && DtZoneLz(&z[3],zEnd,59) && DtZoneLz(&z[6],zEnd,60) ){` |
|      5 |  751 | `		return 8;` |
|      - |  752 | `	}` |
|    291 |  753 | `	if( DtZoneLz(z,zEnd,24) && DtZoneLz(&z[2],zEnd,59) && DtZoneLz(&z[4],zEnd,60) ){` |
|     13 |  754 | `		return 6;` |
|      - |  755 | `	}` |
|    279 |  756 | `	if( (nH = DtZoneField(z,zEnd,24)) == 0 ){` |
|    ! 0 |  757 | `		return 0;` |
|      - |  758 | `	}` |
|    279 |  759 | `	zm = &z[nH];` |
|    279 |  760 | `	if( zm < zEnd && zm[0] == ':' ){ zm++; }` |
|    279 |  761 | `	if( (nM = DtZoneField(zm,zEnd,59)) != 0 ){` |
|    167 |  762 | `		return (int)(zm - z) + nM;` |
|      - |  763 | `	}` |
|    113 |  764 | `	return nH;` |
|    148 |  765 | `}` |
|      - |  766 | `/*` |
|      - |  767 | `` * php's MILITARY zones: a single LETTER is a whole-hour offset -- `A`..`I` are`` |
|      - |  768 | `` * +1..+9, `K`..`M` +10..+12 and `N`..`Y` -1..-12, with `Z` the zero ISO 8601`` |
|      - |  769 | `` * spells and no `J` at all. php names one by its UPPERCASE letter whatever case`` |
|      - |  770 | ` * it was written in, and calls it an ABBREVIATION; no tz database is involved,` |
|      - |  771 | ` * which is why this engine can answer the whole set exactly. Answers 1 and fills` |
|      - |  772 | ` * the offset and the name (a static literal, as every stored zone name here is),` |
|      - |  773 | `` * or 0 for `J` and for anything that is not a letter.`` |
|      - |  774 | ` */` |
|    318 |  775 | `static int DtZoneMil(int c,sxi32 *piOff,const char **pzName)` |
|      1 |  776 | `{` |
|      - |  777 | `	static const char zLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";` |
|      - |  778 | `	int i;` |
|    319 |  779 | `	if( c >= 'a' && c <= 'z' ){` |
|    151 |  780 | `		c -= 'a' - 'A';` |
|     75 |  781 | `	}` |
|    319 |  782 | `	if( c < 'A' \|\| c > 'Z' \|\| c == 'J' ){` |
|     23 |  783 | `		return 0;` |
|      - |  784 | `	}` |
|    297 |  785 | `	i = c - 'A';` |
|    297 |  786 | `	*pzName = &zLetters[i];` |
|    297 |  787 | `	if( c == 'Z' ){` |
|     65 |  788 | `		*piOff = 0;` |
|    265 |  789 | `	}else if( c < 'J' ){` |
|     45 |  790 | `		*piOff = (sxi32)(i + 1) * 3600;    /* A..I: +1..+9 */` |
|    211 |  791 | `	}else if( c <= 'M' ){` |
|     21 |  792 | `		*piOff = (sxi32)i * 3600;          /* K..M: +10..+12 (the missing J shifts them) */` |
|     11 |  793 | `	}else{` |
|    169 |  794 | `		*piOff = -(sxi32)(i - 12) * 3600;  /* N..Y: -1..-12 */` |
|      - |  795 | `	}` |
|    297 |  796 | `	return 1;` |
|    160 |  797 | `}` |
|      - |  798 | `/*` |
|      - |  799 | ` * php's longest match, seen from a WORD's side. Every rule spelled in letters` |
|      - |  800 | ` * competes with the TIMEZONE token, which reads at most SIX of them` |
|      - |  801 | ` * (DtZoneShape), so a keyword wins only when it is at least as long as that` |
|      - |  802 | ` * read -- which is to say when it runs to the end of the letter run, or is six` |
|      - |  803 | ` * letters itself and ties (a tie goes to whichever rule timelib spells first,` |
|      - |  804 | `` * and the zone is its last). `nowx`, `janx` and `todayx` are unknown zones;`` |
|      - |  805 | `` * `januaryx`, `tomorrowx` and `augustx` are the word with a military zone`` |
|      - |  806 | ` * behind it.` |
|      - |  807 | ` *` |
|      - |  808 | ` * The rules that are only ever PART of a longer one -- the ordinal and` |
|      - |  809 | ` * navigation words, which need a unit or a weekday after them -- do not get` |
|      - |  810 | `` * this: `previousx month` is a zone in php, because `previous` alone is not a`` |
|      - |  811 | ` * token there at all.` |
|      - |  812 | ` */` |
|    928 |  813 | `static int DtWordEnds(const char *z,const char *zEnd,int nKw)` |
|      1 |  814 | `{` |
|    929 |  815 | `	return nKw >= 6 \|\| &z[nKw] >= zEnd \|\| !SyisAlpha((unsigned char)z[nKw]);` |
|      1 |  816 | `}` |
|      - |  817 | `/*` |
|      - |  818 | ` * php's timezone token by SHAPE. Its scanner matches the re2c rule and asks what` |
|      - |  819 | `` * the letters SPELL only afterwards, which is why `Z,tues` reports a double`` |
|      - |  820 | `` * timezone at the `tues` rather than a name the database does not have -- and`` |
|      - |  821 | ` * why an unknown word is a token that the string reads PAST rather than a byte` |
|      - |  822 | ` * it stops on. The rule is two alternatives and, as every re2c rule does, the` |
|      - |  823 | ` * longer of the two wins:` |
|      - |  824 | ` *` |
|      - |  825 | ` *   "("? [A-Za-z]{1,6} ")"?          the abbreviation -- each paren optional on` |
|      - |  826 | `` *                                    its own, so `(abc` and `abc)` both match`` |
|      - |  827 | ``  *   [A-Z][a-z]+([_/-][A-Za-z]+)+     the tz-database identifier, `Europe/Paris` `` |
|      - |  828 | ` *` |
|      - |  829 | ` * The SIX-letter cap on the first is what every other word-shaped rule competes` |
|      - |  830 | `` * against (DtWordEnds): `janx` is an unknown zone where `januaryx` is January`` |
|      - |  831 | ` * beside the military zone X.` |
|      - |  832 | ` *` |
|      - |  833 | ` * Answers the bytes the token takes and reports the letters inside it, the` |
|      - |  834 | ` * parens dropped.` |
|      - |  835 | ` */` |
|   1546 |  836 | `static int DtZoneShape(const char *z,const char *zEnd,const char **pzName,int *pnName)` |
|      1 |  837 | `{` |
|   1547 |  838 | `	int nPar = 0,nLet = 0,nBare = 0,nId = 0;` |
|   1547 |  839 | `	if( z < zEnd && z[0] == '(' ){` |
|     17 |  840 | `		nPar = 1;` |
|      8 |  841 | `	}` |
|   3209 |  842 | `	while( nLet < 6 && &z[nPar+nLet] < zEnd && SyisAlpha((unsigned char)z[nPar+nLet]) ){` |
|   1663 |  843 | `		nLet++;` |
|      1 |  844 | `	}` |
|      - |  845 | ``	/* ...except a lone `t` with a digit behind it, which php's clock reads`` |
|      - |  846 | ``	 * LONGER than any zone: `t9` is nine in the morning, not the military zone T`` |
|      - |  847 | ``	 * with a stray digit, and `12:00t9` is php's second time specification. */`` |
|   1546 |  848 | `	if( nPar == 0 && nLet == 1 && (z[0] == 't' \|\| z[0] == 'T')` |
|    132 |  849 | `	 && &z[1] < zEnd && SyisDigit(z[1]) ){` |
|    ! 0 |  850 | `		nLet = 0;` |
|    ! 0 |  851 | `	}` |
|   1547 |  852 | `	if( nLet > 0 ){` |
|    713 |  853 | `		nBare = nPar + nLet;` |
|    713 |  854 | `		if( &z[nBare] < zEnd && z[nBare] == ')' ){` |
|     13 |  855 | `			nBare++;` |
|      6 |  856 | `		}` |
|    356 |  857 | `	}` |
|   1547 |  858 | `	if( z < zEnd && z[0] >= 'A' && z[0] <= 'Z' ){` |
|    255 |  859 | `		int k = 1,nSeg = 0;` |
|    269 |  860 | `		while( &z[k] < zEnd && z[k] >= 'a' && z[k] <= 'z' ){ k++; }` |
|    255 |  861 | `		if( k > 1 ){` |
|     13 |  862 | `			for(;;){` |
|     19 |  863 | `				int j = k;` |
|     19 |  864 | `				if( &z[j] >= zEnd \|\| (z[j] != '_' && z[j] != '/' && z[j] != '-') ){` |
|      6 |  865 | `					break;` |
|      - |  866 | `				}` |
|      9 |  867 | `				j++;` |
|      9 |  868 | `				if( &z[j] >= zEnd \|\| !SyisAlpha((unsigned char)z[j]) ){` |
|    ! 0 |  869 | `					break;` |
|      - |  870 | `				}` |
|     25 |  871 | `				while( &z[j] < zEnd && SyisAlpha((unsigned char)z[j]) ){ j++; }` |
|      9 |  872 | `				k = j;` |
|      9 |  873 | `				nSeg++;` |
|      1 |  874 | `			}` |
|     11 |  875 | `			if( nSeg > 0 ){` |
|      5 |  876 | `				nId = k;` |
|      2 |  877 | `			}` |
|      5 |  878 | `		}` |
|    127 |  879 | `	}` |
|   1547 |  880 | `	if( nId > nBare ){` |
|      5 |  881 | `		*pzName = z;` |
|      5 |  882 | `		*pnName = nId;` |
|      5 |  883 | `		return nId;` |
|      - |  884 | `	}` |
|   1543 |  885 | `	if( nBare == 0 ){` |
|    835 |  886 | `		return 0;` |
|      - |  887 | `	}` |
|    709 |  888 | `	*pzName = &z[nPar];` |
|    709 |  889 | `	*pnName = nLet;` |
|    709 |  890 | `	return nBare;` |
|    774 |  891 | `}` |
|      - |  892 | `/* php's timezone_type -- 1 = a fixed UTC OFFSET, 2 = an ABBREVIATION, 3 = an` |
|      - |  893 | ` * IDENTIFIER. Both parsers answer in these, so they stand above both. */` |
|      - |  894 | `#define DT_ZONE_OFFSET 1` |
|      - |  895 | `#define DT_ZONE_ABBR   2` |
|      - |  896 | `#define DT_ZONE_ID     3` |
|      - |  897 | `/*` |
|      - |  898 | ` * ...and what the letters spell, the spellings this engine has without a tz` |
|      - |  899 | `` * database: `UTC` (an IDENTIFIER in that exact case, an abbreviation in any`` |
|      - |  900 | `` * other), `GMT`, and the military letters above. Answers 1 when the name is one`` |
|      - |  901 | ` * of them, 0 for every other shape php would look up and this build cannot.` |
|      - |  902 | ` */` |
|    786 |  903 | `static int DtZoneName(const char *z,int n,sxi32 *piOff,const char **pzName,` |
|      - |  904 | `	int *pnName,int *pbIdent)` |
|      1 |  905 | `{` |
|    787 |  906 | `	if( n == 3 && (SyStrnicmp(z,"utc",3) == 0 \|\| SyStrnicmp(z,"gmt",3) == 0) ){` |
|    183 |  907 | `		int bUtc = (z[0] == 'u' \|\| z[0] == 'U');` |
|    183 |  908 | `		*piOff = 0;` |
|    183 |  909 | `		*pzName = bUtc ? "UTC" : "GMT";` |
|    183 |  910 | `		*pnName = 3;` |
|    183 |  911 | `		*pbIdent = (bUtc && SyMemcmp(z,"UTC",3) == 0);` |
|    183 |  912 | `		return 1;` |
|      - |  913 | `	}` |
|    605 |  914 | `	if( n == 1 && DtZoneMil(z[0],piOff,pzName) ){` |
|    263 |  915 | `		*pnName = 1;` |
|    263 |  916 | `		*pbIdent = 0;` |
|    263 |  917 | `		return 1;` |
|      - |  918 | `	}` |
|    343 |  919 | `	return 0;` |
|    394 |  920 | `}` |
|      - |  921 | `/* Forward: the offset's VALUE is the one DateTimeZone reads too (the door that` |
|      - |  922 | ` * takes a whole string rather than a token), so both spellings share it. */` |
|      - |  923 | `static int DtZoneOffsetDigits(const char *z,int n,sxi32 *piOff,int *pnUsed);` |
|      - |  924 | `/*` |
|      - |  925 | ` * A SIGNED offset at z, the whole token: the sign, then the digits and colons` |
|      - |  926 | ` * DtZoneCorrLen claims. Answers the bytes taken (0 when this is not one).` |
|      - |  927 | ` */` |
|   2844 |  928 | `static int DtZoneCorr(const char *z,const char *zEnd,sxi32 *piOff)` |
|      1 |  929 | `{` |
|   2845 |  930 | `	int n,nUsed = 0;` |
|   2845 |  931 | `	if( zEnd-z < 2 \|\| (z[0] != '+' && z[0] != '-') ){` |
|   2551 |  932 | `		return 0;` |
|      - |  933 | `	}` |
|    294 |  934 | `	if( (n = DtZoneCorrLen(&z[1],zEnd)) == 0` |
|    295 |  935 | `	 \|\| DtZoneOffsetDigits(&z[1],n,piOff,&nUsed) != 0 ){` |
|    ! 0 |  936 | `		return 0;` |
|      - |  937 | `	}` |
|    295 |  938 | `	if( z[0] == '-' ){` |
|    135 |  939 | `		*piOff = -*piOff;` |
|     67 |  940 | `	}` |
|    295 |  941 | `	return n + 1;` |
|   1423 |  942 | `}` |
|      - |  943 | `/*` |
|      - |  944 | ` * php's standalone TIMEZONE token, which its scanner takes anywhere in a date` |
|      - |  945 | `` * string: a name, a name inside PARENTHESES (`2020-01-01 (UTC)`), or a UTC`` |
|      - |  946 | `` * offset with an optional uppercase `GMT` in front of it (`GMT+02:00`; the`` |
|      - |  947 | `` * lowercase spelling is the ABBREVIATION `gmt` with a relative number after it).`` |
|      - |  948 | ` * Advances *pz over what it took and answers 1, answers 0 leaving *pz alone, or` |
|      - |  949 | ` * answers php's "Double timezone specification" in DtParse's own convention.` |
|      - |  950 | ` */` |
|   1554 |  951 | `static int DtZoneTok(const char **pz,const char *zEnd,dt_parsed *p,const char *zIn)` |
|      1 |  952 | `{` |
|   1555 |  953 | `	const char *z = *pz;` |
|   1555 |  954 | `	const char *zName = 0;` |
|   1555 |  955 | `	int nName = 0,bIdent = 0,n = 0,nTok = 0,bKnown = 0,rc;` |
|   1555 |  956 | `	sxi32 iOff = 0;` |
|      - |  957 | ``	/* The `GMT` in front of an offset is read before the NAME of the same three`` |
|      - |  958 | `	 * bytes, because php's scanner takes the longer token -- and only when a whole` |
|      - |  959 | ``	 * offset follows it, which is what makes `GMT+02:00` the offset, `gmt+2` the`` |
|      - |  960 | ``	 * zone GMT with a stray relative number after it, and `GMT+` the zone GMT with`` |
|      - |  961 | `	 * a refusal ON the sign. */` |
|   1554 |  962 | `	if( zEnd-z > 3 && SyMemcmp(z,"GMT",3) == 0` |
|    296 |  963 | `	 && (n = DtZoneCorr(&z[3],zEnd,&iOff)) != 0 ){` |
|      9 |  964 | `		zName = 0;` |
|      9 |  965 | `		nTok = n + 3;` |
|      9 |  966 | `		bKnown = 1;` |
|      5 |  967 | `	}` |
|   1547 |  968 | `	else if( (n = DtZoneShape(z,zEnd,&zName,&nName)) != 0 ){` |
|    713 |  969 | `		nTok = n;` |
|    713 |  970 | `		bKnown = DtZoneName(zName,nName,&iOff,&zName,&nName,&bIdent);` |
|    357 |  971 | `	}` |
|    835 |  972 | `	else if( (n = DtZoneCorr(z,zEnd,&iOff)) != 0 ){` |
|    269 |  973 | `		zName = 0;` |
|    269 |  974 | `		nTok = n;` |
|    269 |  975 | `		bKnown = 1;` |
|    134 |  976 | `	}` |
|   1555 |  977 | `	if( nTok == 0 ){` |
|    567 |  978 | `		return 0;` |
|      - |  979 | `	}` |
|      - |  980 | `	/* php's TIMELIB_HAVE_TZ runs BEFORE the lookup, so only the string's FIRST` |
|      - |  981 | `	 * zone token is ever asked what it spells: a second is dropped whatever it` |
|      - |  982 | `	 * says, a third is the refusal, and neither is reported as a name the` |
|      - |  983 | `	 * database does not have. The token is consumed either way. */` |
|    989 |  984 | `	*pz = &z[nTok];` |
|    989 |  985 | `	rc = DtZoneCount(p);` |
|    989 |  986 | `	if( rc < 0 ){` |
|     59 |  987 | `		return -((int)(z - zIn) + 1) - DT_ERR_DZONE;` |
|      - |  988 | `	}` |
|    931 |  989 | `	if( rc > 0 ){` |
|      - |  990 | `		/* php's TIMELIB_HAVE_TZ warns on the SECOND and refuses only the third */` |
|    129 |  991 | `		DtWarnPend(p,(int)(z - zIn),"Double timezone specification");` |
|     64 |  992 | `	}` |
|    931 |  993 | `	if( rc == 0 ){` |
|    803 |  994 | `		if( !bKnown ){` |
|    281 |  995 | `			return -((int)(z - zIn) + 1) - DT_ERR_TZID;` |
|      - |  996 | `		}` |
|    523 |  997 | `		DtZoneStore(p,iOff,zName,nName,bIdent);` |
|    261 |  998 | `	}` |
|    651 |  999 | `	return 1;` |
|    778 | 1000 | `}` |
|      - | 1001 | `/* Forward: the time SUFFIX has to know whether a DATE would read longer at the` |
|      - | 1002 | ` * same position, and the date rules read a time suffix of their own. */` |
|      - | 1003 | `static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1004 | `	dt_parsed *p,const char *zIn);` |
|      - | 1005 | ``/* True if php's `hour24 [:.] minute` reads here -- the head of every clock its`` |
|      - | 1006 | ` * combined date-and-time rules end in, and the reason a month, a day and a clock` |
|      - | 1007 | ` * are ONE token there: it reads longer than the YEAR the same digits would be. */` |
|     80 | 1008 | `static int DtClockFollows(const char *z,const char *zEnd)` |
|      1 | 1009 | `{` |
|      - | 1010 | `	int h,i,n;` |
|     81 | 1011 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|     31 | 1012 | `		return 0;` |
|      - | 1013 | `	}` |
|     51 | 1014 | `	h = z[0] - '0';` |
|     51 | 1015 | `	n = 1;` |
|     51 | 1016 | `	if( &z[1] < zEnd && SyisDigit(z[1]) && (z[0]-'0')*10 + (z[1]-'0') <= 24 ){` |
|     43 | 1017 | `		h = (z[0]-'0')*10 + (z[1]-'0');` |
|     43 | 1018 | `		n = 2;` |
|     21 | 1019 | `	}` |
|     50 | 1020 | `	if( h > 24 \|\| &z[n] >= zEnd \|\| (z[n] != ':' && z[n] != '.') \|\| &z[n+1] >= zEnd` |
|      4 | 1021 | `	 \|\| !SyisDigit(z[n+1]) ){` |
|     49 | 1022 | `		return 0;` |
|      - | 1023 | `	}` |
|      5 | 1024 | `	i = z[n+1] - '0';` |
|      5 | 1025 | `	if( &z[n+2] < zEnd && SyisDigit(z[n+2]) ){` |
|      5 | 1026 | `		i = i*10 + (z[n+2]-'0');` |
|      2 | 1027 | `	}` |
|      5 | 1028 | `	return i <= 59;` |
|     40 | 1029 | `}` |
|      - | 1030 | `/* True if z points at a two-letter English ordinal suffix (st/nd/rd/th). */` |
|   7890 | 1031 | `static int DtIsOrdinal(const char *z,const char *zEnd)` |
|      3 | 1032 | `{` |
|   7893 | 1033 | `	if( zEnd - z < 2 ){ return 0; }` |
|  13648 | 1034 | `	return SyStrnicmp(z,"st",2) == 0 \|\| SyStrnicmp(z,"nd",2) == 0` |
|  10244 | 1035 | `		\|\| SyStrnicmp(z,"rd",2) == 0 \|\| SyStrnicmp(z,"th",2) == 0;` |
|   3948 | 1036 | `}` |
|      - | 1037 | `/*` |
|      - | 1038 | `` * Parse an OPTIONAL time-of-day suffix after a date component: a space or `T`,`` |
|      - | 1039 | `` * then php's time of day, then a `Z` or a UTC offset. On entry *pz points just`` |
|      - | 1040 | ` * past the date. Advances *pz over whatever it consumes. Returns 0 on success` |
|      - | 1041 | ` * (whether or not a time was present), or a 1-based error position into zIn` |
|      - | 1042 | ` * (negative encodes php's "Double time specification"). Shared by every` |
|      - | 1043 | ` * absolute-date branch.` |
|      - | 1044 | ` */` |
|   2930 | 1045 | `static int DtTimeSuffix(const char **pz,const char *zEnd,const char *zIn,dt_parsed *p)` |
|      2 | 1046 | `{` |
|   2932 | 1047 | `	const char *z = *pz;` |
|   2932 | 1048 | `	if( z < zEnd && (z[0]=='T' \|\| z[0]==' ' \|\| z[0]=='.') && z+1 < zEnd && SyisDigit(z[1]) ){` |
|   2093 | 1049 | `		const char *zTime = &z[1];` |
|      - | 1050 | `		int rc;` |
|      - | 1051 | `		{` |
|      - | 1052 | `			/* php reads whichever token is LONGER at this position, and a dotted` |
|      - | 1053 | `			 * DATE is longer than the clock hiding in its head: the tail of` |
|      - | 1054 | ``			 * `01/02/2020 03.04.2021` is a second date (its refusal), not 03:04:20.`` |
|      - | 1055 | `			 * The probe runs on a copy, and with the date flag cleared so that the` |
|      - | 1056 | `			 * refusal this call would raise cannot answer the question. */` |
|   2093 | 1057 | `			dt_parsed sTry = *p;` |
|   2093 | 1058 | `			const char *zProbe = zTime;` |
|   2093 | 1059 | `			sTry.bHaveDate = 0;` |
|   2093 | 1060 | `			if( DtTryNumericDate(zTime,zEnd,&zProbe,&sTry,zIn) == 1 ){` |
|      9 | 1061 | `				*pz = z;` |
|      9 | 1062 | `				return 0;` |
|      - | 1063 | `			}` |
|      - | 1064 | `		}` |
|   2085 | 1065 | `		rc = DtReadTimeOfDay(&zTime,zEnd,zIn,p);` |
|   2085 | 1066 | `		if( rc < 0 ){ *pz = zTime; return rc; }` |
|   2085 | 1067 | `		if( rc == 0 ){` |
|     31 | 1068 | `			*pz = z;` |
|     31 | 1069 | `			return 0;` |
|      - | 1070 | `		}` |
|   2055 | 1071 | `		z = zTime;` |
|      - | 1072 | ``		/* The zone ATTACHED to the time is php's `iso8601normtz`: a `Z` or a`` |
|      - | 1073 | ``		 * numeric offset, whose seconds `...T12:00:00+02:00:30` reads too. A`` |
|      - | 1074 | `		 * NAME is not part of this token -- it is one of the string's own, which` |
|      - | 1075 | ``		 * is what leaves the `this` of `24:00:00this week` to the relative rule`` |
|      - | 1076 | `		 * that reads it longer. */` |
|   2054 | 1077 | `		if( z < zEnd && (z[0] == 'Z' \|\| z[0] == 'z')` |
|    145 | 1078 | `		 && !(z+1 < zEnd && SyisAlpha((unsigned char)z[1])) ){` |
|     59 | 1079 | `			if( DtZoneCount(p) < 0 ){` |
|    ! 0 | 1080 | `				*pz = &z[1];` |
|    ! 0 | 1081 | `				return -((int)(z - zIn) + 1) - DT_ERR_DZONE;` |
|      - | 1082 | `			}` |
|     59 | 1083 | `			if( p->nZoneTok == 1 ){` |
|     59 | 1084 | `				DtZoneStore(p,0,"Z",1,0);` |
|     30 | 1085 | `			}else{` |
|    ! 0 | 1086 | `				DtWarnPend(p,(int)(z - zIn),"Double timezone specification");` |
|      - | 1087 | `			}` |
|     59 | 1088 | `			z++;` |
|     30 | 1089 | `		}else{` |
|   1997 | 1090 | `			sxi32 iOffTz = 0;` |
|   1997 | 1091 | `			int nTz = DtZoneCorr(z,zEnd,&iOffTz);` |
|   1997 | 1092 | `			if( nTz > 0 ){` |
|     19 | 1093 | `				int rcZ = DtZoneCount(p);` |
|     19 | 1094 | `				const char *zTz = z;` |
|     19 | 1095 | `				z += nTz;` |
|     19 | 1096 | `				if( rcZ < 0 ){` |
|    ! 0 | 1097 | `					*pz = z;` |
|    ! 0 | 1098 | `					return -((int)(zTz - zIn) + 1) - DT_ERR_DZONE;` |
|      - | 1099 | `				}` |
|     19 | 1100 | `				if( rcZ == 0 ){` |
|     19 | 1101 | `					DtZoneStore(p,iOffTz,0,0,0);` |
|     10 | 1102 | `				}else{` |
|    ! 0 | 1103 | `					DtWarnPend(p,(int)(zTz - zIn),"Double timezone specification");` |
|      - | 1104 | `				}` |
|      9 | 1105 | `			}` |
|      - | 1106 | `		}` |
|   1027 | 1107 | `	}` |
|   2894 | 1108 | `	*pz = z;` |
|   2894 | 1109 | `	return 0;` |
|   1467 | 1110 | `}` |
|      - | 1111 | `/*` |
|      - | 1112 | ` * Read one or two decimal digits at z (z<zEnd guaranteed by caller for the first).` |
|      - | 1113 | ` * Returns the value; *pn = digits consumed (1 or 2).` |
|      - | 1114 | ` */` |
|   2122 | 1115 | `static int DtRead1or2(const char *z,const char *zEnd,int *pn)` |
|      2 | 1116 | `{` |
|   2124 | 1117 | `	int v = z[0]-'0';` |
|   2124 | 1118 | `	if( z+1 < zEnd && SyisDigit(z[1]) ){ v = v*10 + (z[1]-'0'); *pn = 2; }` |
|    893 | 1119 | `	else { *pn = 1; }` |
|   2124 | 1120 | `	return v;` |
|      2 | 1121 | `}` |
|      - | 1122 | `/*` |
|      - | 1123 | ` * The YEAR of php's ISO date, at the head of a string: four digits, or php's` |
|      - | 1124 | ` * EXPANDED form -- a sign in front of AT LEAST four digits, with no upper width` |
|      - | 1125 | `` * (`-1234-03-04`, `+12345-01-01`, `-123456789-01-01`). The sign is what admits`` |
|      - | 1126 | ` * the extra digits: an unsigned five-digit run is not a date to php at all, and` |
|      - | 1127 | `` * a signed run shorter than four is not one either (`-123-03-04` fails there).`` |
|      - | 1128 | ` *` |
|      - | 1129 | ` * Answers the bytes the year occupies -- the caller finds the '-' that closes it` |
|      - | 1130 | ` * at that offset -- or 0 when the head is not one. A magnitude past the int64` |
|      - | 1131 | ` * ceiling SATURATES there instead of overflowing; php's own answer past that` |
|      - | 1132 | ` * point is garbage of its own (a 20-digit year reads back as 1999 there), so` |
|      - | 1133 | ` * nothing pins that corner -- only the absence of undefined behaviour.` |
|      - | 1134 | ` */` |
|   5932 | 1135 | `static int DtTryIsoYear(const char *z,const char *zEnd,sxi64 *pY,int *pbRange)` |
|      3 | 1136 | `{` |
|      - | 1137 | `	static const sxu64 iCeil = (sxu64)0x7FFFFFFFFFFFFFFF;` |
|   8901 | 1138 | `	int nSign = (z < zEnd && (z[0] == '+' \|\| z[0] == '-')) ? 1 : 0;` |
|   5935 | 1139 | `	const char *zDig = &z[nSign];` |
|   5935 | 1140 | `	const char *zScan = zDig;` |
|   5935 | 1141 | `	sxu64 y = 0;` |
|   5935 | 1142 | `	int bOver = 0;` |
|      - | 1143 | `	/* php's own ceiling for the field: the magnitude an int64 holds, which is one` |
|      - | 1144 | `	 * larger on the negative side. */` |
|   5935 | 1145 | `	sxu64 iMax = (nSign && z[0] == '-') ? iCeil + 1 : iCeil;` |
|  22575 | 1146 | `	while( zScan < zEnd && SyisDigit(zScan[0]) ){` |
|  16643 | 1147 | `		sxu64 dig = (sxu64)(zScan[0] - '0');` |
|  16643 | 1148 | `		if( bOver \|\| y > (iMax - dig) / 10 ){` |
|     62 | 1149 | `			bOver = 1;` |
|     62 | 1150 | `			y = iMax;` |
|     32 | 1151 | `		}else{` |
|  16583 | 1152 | `			y = y * 10 + dig;` |
|      - | 1153 | `		}` |
|  16643 | 1154 | `		zScan++;` |
|      3 | 1155 | `	}` |
|   5935 | 1156 | `	if( zScan - zDig < 4 \|\| (nSign == 0 && zScan - zDig != 4) ){` |
|   3147 | 1157 | `		return 0;` |
|      - | 1158 | `	}` |
|   2791 | 1159 | `	if( zScan >= zEnd \|\| zScan[0] != '-' ){` |
|    284 | 1160 | `		return 0;` |
|      - | 1161 | `	}` |
|   2508 | 1162 | `	if( nSign && zScan - zDig > 19 ){` |
|      - | 1163 | `		/* php's EXPANDED year is at most nineteen digits; a wider run is not this` |
|      - | 1164 | `		 * token at all and the string re-reads it with whatever else fits. */` |
|     13 | 1165 | `		return 0;` |
|      - | 1166 | `	}` |
|      - | 1167 | `	/* ...and one that no int64 holds is php's own refusal -- but only once the` |
|      - | 1168 | `	 * REST of the token has matched too, so the caller is told rather than` |
|      - | 1169 | ``	 * answered: `+9296228446195592075-1-1` is no expanded date at all there and`` |
|      - | 1170 | `	 * reports the byte its re-reading trips on instead. */` |
|   2496 | 1171 | `	*pbRange = bOver;` |
|      - | 1172 | `	/* the negative bound IS the int64's own, so the sign is applied in UNSIGNED` |
|      - | 1173 | `	 * arithmetic: negating -9223372036854775808 as a signed value is undefined` |
|      - | 1174 | `	 * and this build gates on UBSan. */` |
|   2496 | 1175 | `	*pY = (nSign && z[0] == '-') ? (sxi64)((sxu64)0 - y) : (sxi64)y;` |
|   2496 | 1176 | `	return (int)(zScan - z);` |
|   2969 | 1177 | `}` |
|      - | 1178 | `/*` |
|      - | 1179 | ` * php's ISO WEEK DATE, the spelling ISO 8601 gives a week rather than a day:` |
|      - | 1180 | `` * `YYYY[-]Www` and `YYYY[-]Www[-]D`, with the year exactly four digits and`` |
|      - | 1181 | ` * unsigned, the week exactly two and inside 01..53, and the day ONE digit` |
|      - | 1182 | ` * inside 0..7. Everything outside that is not this token at all, which is why` |
|      - | 1183 | `` * `2020-W54`, `2020-W5` and `2020-w05` refuse where the loop runs out of rules`` |
|      - | 1184 | `` * rather than here -- and why `2020-W05-8` is the week alone with `-8` left`` |
|      - | 1185 | ` * standing as a zone OFFSET, which is the answer php gives it.` |
|      - | 1186 | ` *` |
|      - | 1187 | ` * php does not resolve the week to a calendar date: timelib writes the year` |
|      - | 1188 | ` * with January 1st and puts the whole distance on the RELATIVE day count, which` |
|      - | 1189 | `` * is what `date_parse('2020-W05')` shows as `day => 26`. The distance runs from`` |
|      - | 1190 | ` * that January 1st to the Monday of week 1 -- the week holding the 4th -- plus` |
|      - | 1191 | ` * a week per week and a day per day.` |
|      - | 1192 | ` *` |
|      - | 1193 | ` * Returns 0 when the text is not one (caller falls through), 1 on success, or` |
|      - | 1194 | ` * an error code in DtParse's own convention.` |
|      - | 1195 | ` */` |
|   4210 | 1196 | `static int DtTryIsoWeek(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1197 | `	dt_parsed *p,const char *zIn)` |
|      3 | 1198 | `{` |
|   4213 | 1199 | `	const char *zTok = z;` |
|   4213 | 1200 | `	sxi64 y = 0;` |
|   4213 | 1201 | `	int i,w,iDow = 1,dow1,rcT;` |
|      - | 1202 | ``	/* the shortest spelling is the compact `2020W05` */`` |
|   4213 | 1203 | `	if( zEnd - z < 7 ){` |
|    851 | 1204 | `		return 0;` |
|      - | 1205 | `	}` |
|  15279 | 1206 | `	for( i = 0 ; i < 4 ; i++ ){` |
|  12537 | 1207 | `		if( !SyisDigit(z[i]) ){` |
|    619 | 1208 | `			return 0;` |
|      - | 1209 | `		}` |
|  11919 | 1210 | `		y = y*10 + (z[i] - '0');` |
|   5961 | 1211 | `	}` |
|   2745 | 1212 | `	if( z[i] == '-' ){` |
|   2474 | 1213 | `		i++;` |
|   1236 | 1214 | `	}` |
|   2745 | 1215 | `	if( &z[i+2] >= zEnd \|\| z[i] != 'W' ){` |
|   2675 | 1216 | `		return 0;` |
|      - | 1217 | `	}` |
|     71 | 1218 | `	i++;` |
|     71 | 1219 | `	if( !SyisDigit(z[i]) \|\| !SyisDigit(z[i+1]) ){` |
|    ! 0 | 1220 | `		return 0;` |
|      - | 1221 | `	}` |
|     71 | 1222 | `	w = (z[i]-'0')*10 + (z[i+1]-'0');` |
|     71 | 1223 | `	i += 2;` |
|     71 | 1224 | `	if( w < 1 \|\| w > 53 ){` |
|      7 | 1225 | `		return 0;` |
|      - | 1226 | `	}` |
|      - | 1227 | `	{` |
|      - | 1228 | `		/* the day, with its own separator: a digit past 7 belongs to whatever` |
|      - | 1229 | `		 * follows the token, its sign included */` |
|     65 | 1230 | `		int j = i;` |
|     65 | 1231 | `		if( &z[j] < zEnd && z[j] == '-' ){` |
|     23 | 1232 | `			j++;` |
|     11 | 1233 | `		}` |
|     65 | 1234 | `		if( &z[j] < zEnd && z[j] >= '0' && z[j] <= '7' ){` |
|     21 | 1235 | `			iDow = z[j] - '0';` |
|     21 | 1236 | `			i = j + 1;` |
|     10 | 1237 | `		}` |
|      - | 1238 | `	}` |
|     65 | 1239 | `	z = &z[i];` |
|     65 | 1240 | `	*pzOut = z;` |
|     65 | 1241 | `	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }` |
|      - | 1242 | `	/* php's weekday numbering here is 0 = Sunday, and week 1 is the one whose` |
|      - | 1243 | `	 * Monday is at most three days after New Year's Day. */` |
|     63 | 1244 | `	dow1 = DtDowOf(DtDaysFromCivil(y,1,1));` |
|     63 | 1245 | `	p->y = y;` |
|     63 | 1246 | `	p->m = 1;` |
|     63 | 1247 | `	p->d = 1;` |
|      - | 1248 | ``	/* php ASSIGNS that count rather than adding to it, the way `tomorrow` and`` |
|      - | 1249 | ``	 * `yesterday` do -- so a `+1 week` written BEFORE the week date is discarded`` |
|      - | 1250 | ``	 * by it (`+1 week 2020-W05` is the week's own Monday) while one written after`` |
|      - | 1251 | `	 * moves on from it. */` |
|     63 | 1252 | `	p->bHaveRel = 1;` |
|     63 | 1253 | `	p->rd = (sxi64)(1 - (dow1 > 4 ? dow1 - 7 : dow1) + (w - 1)*7 + (iDow - 1));` |
|     63 | 1254 | `	*pzOut = z;` |
|     63 | 1255 | `	return 1;` |
|   2108 | 1256 | `}` |
|      - | 1257 | `/*` |
|      - | 1258 | ` * Try to read php's ISO date at z: [+-]YYYY-MM-DD plus an optional time suffix.` |
|      - | 1259 | ` * Returns 0 when the text is not one (caller falls through), 1 on success, or an` |
|      - | 1260 | ` * error code in DtParse's own convention.` |
|      - | 1261 | ` */` |
|   5932 | 1262 | `static int DtTryIsoDate(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1263 | `	dt_parsed *p,const char *zIn)` |
|      3 | 1264 | `{` |
|      - | 1265 | `	const char *zRest;` |
|   5935 | 1266 | `	const char *zTok = z;` |
|   5935 | 1267 | `	sxi64 y = 0;` |
|   5935 | 1268 | `	int nYr,mo = 0,d = 0,rcT,bRange = 0,bFull;` |
|   5935 | 1269 | `	if( (nYr = DtTryIsoYear(z,zEnd,&y,&bRange)) == 0 ){` |
|   3441 | 1270 | `		return 0;` |
|      - | 1271 | `	}` |
|   2496 | 1272 | `	zRest = &z[nYr];   /* the '-' that closed the year */` |
|   4894 | 1273 | `	bFull = !(zEnd-z < nYr + 6` |
|   2446 | 1274 | `	 \|\| !SyisDigit(zRest[1])\|\|!SyisDigit(zRest[2])\|\|zRest[3] != '-'` |
|   2372 | 1275 | `	 \|\|!SyisDigit(zRest[4])\|\|!SyisDigit(zRest[5]));` |
|   2496 | 1276 | `	if( bFull ){` |
|   2360 | 1277 | `		mo = (zRest[1]-'0')*10 + (zRest[2]-'0');` |
|   2360 | 1278 | `		d  = (zRest[4]-'0')*10 + (zRest[5]-'0');` |
|      - | 1279 | `		/* php spells the ranges INSIDE the pattern, so a month past 12 or a day` |
|      - | 1280 | `		 * past 31 means this spelling never matched at all -- the shorter rule` |
|      - | 1281 | `		 * behind it did, and the digits it did not take are left to the string.` |
|      - | 1282 | ``		 * `2020-13-45` is January the 1st of 2020 with a refusal on its `3`, not`` |
|      - | 1283 | `		 * a refusal and no date; the difference is invisible in what either` |
|      - | 1284 | `		 * engine THROWS and plain in what it collects, because a date already` |
|      - | 1285 | ``		 * read is what makes the next one a `Double date specification`.`` |
|      - | 1286 | `		 * ("00" lexes fine and normalizes: month 0 is December of the year` |
|      - | 1287 | `		 * before, which the field normalizer does on its own.) */` |
|   2360 | 1288 | `		if( mo > 12 \|\| d > 31 ){` |
|     37 | 1289 | `			bFull = 0;` |
|     18 | 1290 | `		}` |
|   1179 | 1291 | `	}` |
|   2496 | 1292 | `	if( !bFull ){` |
|      - | 1293 | `		/* Not the full spelling. Two SHORTER ones stand behind it, both php's and` |
|      - | 1294 | ``		 * both only after a plain four-digit year (`+12345-01` is neither): the`` |
|      - | 1295 | ``		 * YEAR-MONTH `2020-01`, whose day is the 1st, and the ISO ORDINAL`` |
|      - | 1296 | ``		 * `2020-102`, whose three digits are the day of the YEAR. At most three`` |
|      - | 1297 | `		 * digits belong to either, and whatever is left of the run is the string's` |
|      - | 1298 | ``		 * -- as is the whole token when the DAY-FIRST numeric rule (`2020-1-1`),`` |
|      - | 1299 | `		 * php's own separate one, reads longer here.` |
|      - | 1300 | `		 *` |
|      - | 1301 | `		 * Anything else hands the text on rather than refusing: the position this` |
|      - | 1302 | `		 * would report is the TOKEN's, and a token at position 0 encodes as the 1` |
|      - | 1303 | `		 * that means "matched", which spun the parse loop forever. */` |
|    173 | 1304 | `		int nd = 0;` |
|    489 | 1305 | `		while( &zRest[1+nd] < zEnd && SyisDigit(zRest[1+nd]) ){ nd++; }` |
|    173 | 1306 | `		if( nYr != 4 \|\| !SyisDigit(z[0]) \|\| nd == 0 ){` |
|     27 | 1307 | `			return 0;` |
|      - | 1308 | `		}` |
|    147 | 1309 | `		if( nd > 3 ){ nd = 3; }` |
|      - | 1310 | `		{` |
|      - | 1311 | ``			/* The DAY-FIRST rule reads `2020-1-1` whole, which is LONGER than the`` |
|      - | 1312 | `			 * year-month reading of its head -- php's scanner takes the longer` |
|      - | 1313 | `			 * token, so let it. The probe runs on a copy with the date flag` |
|      - | 1314 | `			 * cleared, so a refusal it would raise cannot answer the question. */` |
|    147 | 1315 | `			dt_parsed sTry = *p;` |
|    147 | 1316 | `			const char *zProbe = zTok;` |
|      - | 1317 | `			int rcP;` |
|    147 | 1318 | `			sTry.bHaveDate = 0;` |
|    147 | 1319 | `			rcP = DtTryNumericDate(zTok,zEnd,&zProbe,&sTry,zIn);` |
|    147 | 1320 | `			if( rcP == 1 && zProbe > &zRest[1+nd] ){` |
|     43 | 1321 | `				return 0;` |
|      - | 1322 | `			}` |
|      - | 1323 | `			/* The longer token matched and its own field check REFUSED it, which` |
|      - | 1324 | ``			 * is php's answer for the whole string -- `3854-2-40` is a day out of`` |
|      - | 1325 | ``			 * range there, not the year-month `3854-2` with `-40` behind it. */`` |
|    105 | 1326 | `			if( rcP != 0 && rcP != 1 ){` |
|    ! 0 | 1327 | `				*pzOut = zProbe;` |
|    ! 0 | 1328 | `				return rcP;` |
|      - | 1329 | `			}` |
|      - | 1330 | `		}` |
|      - | 1331 | `		/* The LONGEST reading that validates wins and the rest of the run is left` |
|      - | 1332 | `		 * to the string, which is where php's refusals for this shape really come` |
|      - | 1333 | ``		 * from: `2020-13` is the month 1 with a stray `3` after it (its "position`` |
|      - | 1334 | ``		 * 6"), and `1526-797-45` the month 7 with `97-45` left over. */`` |
|      - | 1335 | `		{` |
|    105 | 1336 | `			int doy = nd == 3 ? (zRest[1]-'0')*100 + (zRest[2]-'0')*10 + (zRest[3]-'0') : 0;` |
|    105 | 1337 | `			int mo2 = nd >= 2 ? (zRest[1]-'0')*10 + (zRest[2]-'0') : 99;` |
|    105 | 1338 | `			if( nd == 3 && doy >= 1 && doy <= 366 ){` |
|     27 | 1339 | `				mo = 1;` |
|     27 | 1340 | `				d = doy;   /* the field normalizer resolves it out of January */` |
|     92 | 1341 | `			}else if( nd >= 2 && mo2 <= 12 ){` |
|     33 | 1342 | `				nd = 2;` |
|     33 | 1343 | `				mo = mo2;` |
|     33 | 1344 | `				d = 1;` |
|     17 | 1345 | `			}else{` |
|     47 | 1346 | `				nd = 1;` |
|     47 | 1347 | `				mo = zRest[1]-'0';` |
|     47 | 1348 | `				d = 1;` |
|      - | 1349 | `			}` |
|      - | 1350 | `		}` |
|    105 | 1351 | `		z = &zRest[1+nd];` |
|    105 | 1352 | `		*pzOut = z;` |
|    105 | 1353 | `		if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }` |
|    103 | 1354 | `		p->y = y;` |
|    103 | 1355 | `		p->m = mo;` |
|    103 | 1356 | `		p->d = d;` |
|    103 | 1357 | `		if( (rcT = DtTimeSuffix(&z,zEnd,zIn,p)) != 0 ){` |
|    ! 0 | 1358 | `			*pzOut = z;` |
|    ! 0 | 1359 | `			return rcT;` |
|      - | 1360 | `		}` |
|    103 | 1361 | `		*pzOut = z;` |
|    103 | 1362 | `		return 1;` |
|      - | 1363 | `	}` |
|   2324 | 1364 | `	if( bRange ){` |
|      - | 1365 | ``		/* the whole `[+-]YYYY-MM-DD` matched and its year is past the int64 the`` |
|      - | 1366 | `		 * field is kept in: php's "Number out of range", at the sign */` |
|     11 | 1367 | `		*pzOut = &zRest[6];` |
|     11 | 1368 | `		return -((int)(zTok - zIn) + 1) - DT_ERR_RANGE;` |
|      - | 1369 | `	}` |
|   2314 | 1370 | `	z = &zRest[6];` |
|      - | 1371 | `	/* php's DAY carries an optional ordinal suffix wherever a day stands, this` |
|      - | 1372 | ``	 * spelling included: `2020-01-02nd` is the 2nd. Only behind a plain`` |
|      - | 1373 | `	 * four-digit year, though -- the EXPANDED form is a rule of its own, and` |
|      - | 1374 | ``	 * `+12345-01-02nd` leaves the `nd` to the string as an unknown zone. */`` |
|   2314 | 1375 | `	if( nYr == 4 && DtIsOrdinal(z,zEnd) ){` |
|      7 | 1376 | `		z += 2;` |
|      3 | 1377 | `	}` |
|   2314 | 1378 | `	*pzOut = z;` |
|   2314 | 1379 | `	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }` |
|   2308 | 1380 | `	p->y = y;` |
|   2308 | 1381 | `	p->m = mo;` |
|   2308 | 1382 | `	p->d = d;` |
|   2308 | 1383 | `	if( (rcT = DtTimeSuffix(&z,zEnd,zIn,p)) != 0 ){` |
|    ! 0 | 1384 | `		*pzOut = z;` |
|    ! 0 | 1385 | `		return rcT;` |
|      - | 1386 | `	}` |
|   2308 | 1387 | `	*pzOut = z;` |
|   2308 | 1388 | `	return 1;` |
|   2969 | 1389 | `}` |
|      - | 1390 | `/*` |
|      - | 1391 | `` * php's ISO ORDINAL date spelled with a FULL STOP, `YYYY.DDD` -- the same date`` |
|      - | 1392 | ``  * `2020-102` gives, and the ONLY dotted form a four-digit year takes: `2020.1` `` |
|      - | 1393 | `` * and `2020.12` are no date at all there (the four digits are a clock and the`` |
|      - | 1394 | ` * rest is refused), the run is exactly three digits inside 001..366, the sign` |
|      - | 1395 | `` * belongs to a rule of its own (`+2020.102` is not one), and a time suffix may`` |
|      - | 1396 | ` * follow.` |
|      - | 1397 | ` *` |
|      - | 1398 | ` * It matters beyond its own spelling, because php's full stop between two digit` |
|      - | 1399 | ` * runs is an ordinary SEPARATOR and this is the only rule that competes for it.` |
|      - | 1400 | ` * PHL had no such rule and stood the competition down instead -- a dot before a` |
|      - | 1401 | ` * digit was simply not a separator -- which refused every string where no dotted` |
|      - | 1402 | `` * date is there to claim it: `20240102.2020` is a date, a separator and a clock`` |
|      - | 1403 | `` * in php, and `1234.2020` is THIS date with a digit left over (php refuses on`` |
|      - | 1404 | ` * the fifth byte of the run, not on the dot).` |
|      - | 1405 | ` */` |
|   1778 | 1406 | `static int DtTryIsoOrdinalDot(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1407 | `	dt_parsed *p,const char *zIn)` |
|      2 | 1408 | `{` |
|   1780 | 1409 | `	const char *zTok = z;` |
|      - | 1410 | `	sxi64 y;` |
|      - | 1411 | `	int i,doy,rcT;` |
|   1780 | 1412 | `	if( zEnd - z < 8 ){` |
|    945 | 1413 | `		return 0;` |
|      - | 1414 | `	}` |
|   2856 | 1415 | `	for( i = 0 ; i < 4 ; i++ ){` |
|   2556 | 1416 | `		if( !SyisDigit(z[i]) ){ return 0; }` |
|   1012 | 1417 | `	}` |
|    302 | 1418 | `	if( z[4] != '.' \|\| !SyisDigit(z[5]) \|\| !SyisDigit(z[6]) \|\| !SyisDigit(z[7]) ){` |
|    270 | 1419 | `		return 0;` |
|      - | 1420 | `	}` |
|     33 | 1421 | `	doy = (z[5]-'0')*100 + (z[6]-'0')*10 + (z[7]-'0');` |
|     33 | 1422 | `	if( doy < 1 \|\| doy > 366 ){` |
|      5 | 1423 | `		return 0;` |
|      - | 1424 | `	}` |
|     29 | 1425 | `	y = (sxi64)((z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0'));` |
|     29 | 1426 | `	z += 8;` |
|     29 | 1427 | `	*pzOut = z;` |
|     29 | 1428 | `	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){` |
|    ! 0 | 1429 | `		return rcT;` |
|      - | 1430 | `	}` |
|     29 | 1431 | `	p->y = y;` |
|     29 | 1432 | `	p->m = 1;` |
|     29 | 1433 | `	p->d = doy;   /* the field normalizer resolves it out of January */` |
|     29 | 1434 | `	if( (rcT = DtTimeSuffix(&z,zEnd,zIn,p)) != 0 ){` |
|    ! 0 | 1435 | `		*pzOut = z;` |
|    ! 0 | 1436 | `		return rcT;` |
|      - | 1437 | `	}` |
|     29 | 1438 | `	*pzOut = z;` |
|     29 | 1439 | `	return 1;` |
|    891 | 1440 | `}` |
|      - | 1441 | `/*` |
|      - | 1442 | ` * Try to read a non-ISO numeric date at z: three integer components joined by ONE` |
|      - | 1443 | ` * consistent separator, plus an optional time suffix. php's field order depends on` |
|      - | 1444 | ` * the separator:` |
|      - | 1445 | ` *   '/'      -> YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY` |
|      - | 1446 | ` *   '-','.'  -> DD-MM-YYYY (day first); a 4-digit-first '.' date (YYYY.MM.DD) is` |
|      - | 1447 | ` *               NOT a php format and is rejected. (ISO YYYY-MM-DD is matched by the` |
|      - | 1448 | ` *               dedicated branch BEFORE this one, so a 4-digit-first '-' never` |
|      - | 1449 | ` *               reaches here.)` |
|      - | 1450 | ` * A 1-2 digit year maps php-style (00-69 -> 2000s, 70-99 -> 1900s). Returns 0 when` |
|      - | 1451 | ` * the text is not such a date (caller falls through), 1 on success (the vector's` |
|      - | 1452 | ` * date fields set and *pzOut advanced past the whole token), or an error code in` |
|      - | 1453 | ` * DtParse's own convention (positive 1-based position into zIn, negative = "double` |
|      - | 1454 | ` * time") when the shape matched but a component is out of range.` |
|      - | 1455 | ` */` |
|   3988 | 1456 | `static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1457 | `	dt_parsed *p,const char *zIn)` |
|      2 | 1458 | `{` |
|   3990 | 1459 | `	const char *zTok = z;` |
|      - | 1460 | `	int a,b,c,na,nb,nc;` |
|      - | 1461 | `	char sep;` |
|      - | 1462 | `	int y,mo,d;` |
|      - | 1463 | `	int rcT;` |
|   3990 | 1464 | `	int bOrd1 = 0,bOrd2 = 0,iDayField;` |
|      - | 1465 | `	/* first field: 1-4 digits */` |
|   3990 | 1466 | `	if( !SyisDigit(z[0]) ){ return 0; }` |
|   3990 | 1467 | `	a = 0; na = 0;` |
|  12300 | 1468 | `	while( z < zEnd && SyisDigit(z[0]) && na < 4 ){ a = a*10 + (z[0]-'0'); z++; na++; }` |
|      - | 1469 | `	/* php's ordinal suffix belongs to the DAY, and which FIELD that is depends on` |
|      - | 1470 | `	 * the separator and the widths -- both of them known only further down. So it` |
|      - | 1471 | `	 * is read where it may stand and judged once the mapping is: a suffix on the` |
|      - | 1472 | `` 	 * year or the month is not this token at all (`2020th-1-2` and `20-1th-2020` `` |
|      - | 1473 | `	 * are refusals in php too, since the separator behind it never matches). A` |
|      - | 1474 | `	 * day is at most TWO digits wide there, so a wider run does not carry one` |
|      - | 1475 | ``	 * either -- `020th-1-2020` is a refusal on its first byte. */`` |
|   3990 | 1476 | `	if( na <= 2 && DtIsOrdinal(z,zEnd) ){ bOrd1 = 1; z += 2; }` |
|   3990 | 1477 | `	if( z >= zEnd \|\| (z[0] != '-' && z[0] != '/' && z[0] != '.') ){ return 0; }` |
|    567 | 1478 | `	sep = z[0];` |
|    567 | 1479 | `	z++;` |
|      - | 1480 | `	/* second field: 1-2 digits */` |
|    567 | 1481 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    549 | 1482 | `	b = DtRead1or2(z,zEnd,&nb); z += nb;` |
|    549 | 1483 | `	if( DtIsOrdinal(z,zEnd) ){ bOrd2 = 1; z += 2; }` |
|    549 | 1484 | `	if( z >= zEnd \|\| z[0] != sep ){ return 0; }` |
|    331 | 1485 | `	z++;` |
|      - | 1486 | `	/* third field: 1-4 digits */` |
|    331 | 1487 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    325 | 1488 | `	c = 0; nc = 0;` |
|   1107 | 1489 | `	while( z < zEnd && SyisDigit(z[0]) && nc < 4 ){ c = c*10 + (z[0]-'0'); z++; nc++; }` |
|      - | 1490 | `	/* Which of the three the DAY is -- the separator and the widths decide, and` |
|      - | 1491 | `	 * both the field WIDTH below and php's ordinal suffix follow from it. */` |
|    325 | 1492 | `	iDayField = sep == '/' ? (na == 4 ? 3 : 2) : (sep == '.' ? 1 : (nc == 4 ? 1 : 3));` |
|      - | 1493 | `	/* map fields to Y/M/D; nyear tracks the year field's width for 2-digit mapping.` |
|      - | 1494 | `	 * '/'  : YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY.` |
|      - | 1495 | `	 * '-'/'.': a 4-digit LAST field is DD-MM-YYYY (day first); otherwise YY-MM-DD` |
|      - | 1496 | `	 *          (year first) — php's width heuristic. (A 4-digit FIRST '-' field is` |
|      - | 1497 | `	 *          ISO and never reaches here; a 4-digit-first '.' is not a php format.) */` |
|      - | 1498 | `	{` |
|      - | 1499 | `		int nyear;` |
|    325 | 1500 | `		if( sep == '/' ){` |
|     47 | 1501 | `			if( na == 4 ){ y = a; mo = b; d = c; nyear = na; }` |
|     33 | 1502 | `			else{ mo = a; d = b; y = c; nyear = nc; }` |
|    302 | 1503 | `		}else if( sep == '.' ){` |
|      - | 1504 | ``			/* php's dot date is day-first with a 2- or 4-digit YEAR (`20.03.67` is`` |
|      - | 1505 | `			 * 2067-03-20). Any other width is not a clean php format -- php itself` |
|      - | 1506 | `			 * yields garbage there -- so don't claim the match.` |
|      - | 1507 | `			 *` |
|      - | 1508 | `			 * A 2-digit year is the same BYTES as php's dotted CLOCK, and the clock` |
|      - | 1509 | `			 * is the rule its scanner declares first, so the clock wins whenever it` |
|      - | 1510 | ``			 * READS: `20.03.00` is 20:03:00 and `20.03.67`, whose seconds no clock`` |
|      - | 1511 | `			 * can hold, is the date. The probe runs on a copy with the time flag` |
|      - | 1512 | `			 * cleared, so a "Double time specification" this string would raise` |
|      - | 1513 | `			 * cannot answer the question. */` |
|    103 | 1514 | `			if( na == 4 ){ return 0; }` |
|     99 | 1515 | `			if( nc == 3 ){` |
|      - | 1516 | `				/* php takes FOUR digits or two, never three: the year of` |
|      - | 1517 | ``				 * `20.03.671` is 67 and the `1` is left to the string. */`` |
|      9 | 1518 | `				c /= 10;` |
|      9 | 1519 | `				nc = 2;` |
|      9 | 1520 | `				z--;` |
|      4 | 1521 | `			}` |
|     99 | 1522 | `			if( nc != 4 && nc != 2 ){ return 0; }` |
|     85 | 1523 | `			if( nc == 2 ){` |
|     49 | 1524 | `				dt_parsed sTry = *p;` |
|     49 | 1525 | `				const char *zProbe = zTok;` |
|     49 | 1526 | `				sTry.nTimeTok = 0;` |
|      - | 1527 | `				/* php takes the LONGER token, and a twelve-hour clock reads past` |
|      - | 1528 | `` 				 * where the date would end: `3.04.05` is a date and `3.04.05pm` `` |
|      - | 1529 | `				 * the time under it. */` |
|     49 | 1530 | `				if( DtReadTimeOfDay(&zProbe,zEnd,zIn,&sTry) == 1 && zProbe >= z ){` |
|     11 | 1531 | `					return 0;` |
|      - | 1532 | `				}` |
|     19 | 1533 | `			}` |
|     75 | 1534 | `			d = a; mo = b; y = c; nyear = nc;` |
|     38 | 1535 | `		}else{ /* '-' : a 4-digit LAST field is DD-MM-YYYY, else YY-MM-DD */` |
|    177 | 1536 | `			if( nc == 4 ){ d = a; mo = b; y = c; nyear = nc; }` |
|    147 | 1537 | `			else{ y = a; mo = b; d = c; nyear = na; }` |
|      - | 1538 | `		}` |
|    297 | 1539 | `		if( nyear <= 2 ){` |
|     63 | 1540 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|     19 | 1541 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|     31 | 1542 | `		}` |
|      - | 1543 | `	}` |
|      - | 1544 | `	/* php spells the ranges INSIDE the pattern, so a month past 12 or a day past` |
|      - | 1545 | `	 * 31 means this is not a date at all and the scanner tries its other rules --` |
|      - | 1546 | ``	 * which is what makes `9.30.359699` the time 09:30:35 and the year 9699. Month`` |
|      - | 1547 | `	 * 0 and day 0 do match, and normalize (month 0 is December of the year` |
|      - | 1548 | `	 * before). */` |
|      - | 1549 | `	/* A DAY is at most two digits wide in every one of php's spellings, where a` |
|      - | 1550 | `	 * YEAR may be four, so the third field's width depends on which of the two` |
|      - | 1551 | ``	 * the mapping made it: `2020/1/22020-01-02` is the 22nd with a second date`` |
|      - | 1552 | `	 * behind it, not a day of 2202. */` |
|    297 | 1553 | `	if( iDayField == 3 && nc > 2 ){` |
|    ! 0 | 1554 | `		int nDrop = nc - 2;` |
|    ! 0 | 1555 | `		while( nDrop-- > 0 ){ c /= 10; z--; }` |
|    ! 0 | 1556 | `		nc = 2;` |
|    ! 0 | 1557 | `		d = c;` |
|    ! 0 | 1558 | `	}` |
|      - | 1559 | `	/* php's DAY pattern is one or two digits and the two-digit reading only when` |
|      - | 1560 | `	 * it is in range, so an out-of-range pair leaves its second digit to the` |
|      - | 1561 | ``	 * string rather than sinking the rule: `3854-2-40` is the 4th with a stray`` |
|      - | 1562 | ``	 * `0` after it there (its refusal), not the year-month `3854-2`. Only the`` |
|      - | 1563 | `	 * year-first dash shape spells its day that way. */` |
|    297 | 1564 | `	if( d > 31 && sep == '-' && na == 4 && nc == 2 && d / 10 <= 31 ){` |
|     53 | 1565 | `		d /= 10;` |
|     53 | 1566 | `		z--;` |
|     26 | 1567 | `	}` |
|    297 | 1568 | `	if( mo > 12 \|\| d > 31 ){ return 0; }` |
|    261 | 1569 | `	if( (bOrd1 && iDayField != 1) \|\| (bOrd2 && iDayField != 2) ){` |
|     15 | 1570 | `		return 0;` |
|      - | 1571 | `	}` |
|    247 | 1572 | `	if( iDayField == 3 && nc <= 2 && DtIsOrdinal(z,zEnd) ){` |
|      7 | 1573 | `		z += 2;` |
|      3 | 1574 | `	}` |
|      - | 1575 | `	/* optional time-of-day suffix, then commit */` |
|    247 | 1576 | `	*pzOut = z;` |
|    247 | 1577 | `	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }` |
|      - | 1578 | `	/* php's American rule reads its year through a helper that comes back UNSET` |
|      - | 1579 | ``	 * once the ordinal has been stepped over, so `4/20th/2020` is the 20th of`` |
|      - | 1580 | ``	 * April on the BASE moment's year where `4/20/2020` is 2020's. */`` |
|    239 | 1581 | `	if( !bOrd2 ){` |
|    235 | 1582 | `		p->y = y;` |
|    117 | 1583 | `	}` |
|    239 | 1584 | `	p->m = mo;` |
|    239 | 1585 | `	p->d = d;` |
|    239 | 1586 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,p);` |
|    239 | 1587 | `	*pzOut = z;` |
|    239 | 1588 | `	if( rcT != 0 ){ return rcT; }` |
|    239 | 1589 | `	return 1;` |
|   1992 | 1590 | `}` |
|      - | 1591 | `/*` |
|      - | 1592 | ` * Match a month name at z (full name or its distinct 3-letter abbreviation, plus` |
|      - | 1593 | ` * "sept"), case-insensitively and only at a word boundary. Returns the month 1-12` |
|      - | 1594 | ` * and sets *pAdv to the bytes consumed, or 0 when no month name is present.` |
|      - | 1595 | ` */` |
|   4072 | 1596 | `static int DtMatchMonth(const char *z,const char *zEnd,int *pAdv)` |
|      2 | 1597 | `{` |
|      - | 1598 | `	static const struct { const char *z; int n; int mo; } aM[] = {` |
|      - | 1599 | `		{ "january",7,1 },{ "february",8,2 },{ "march",5,3 },{ "april",5,4 },` |
|      - | 1600 | `		{ "june",4,6 },{ "july",4,7 },{ "august",6,8 },{ "september",9,9 },` |
|      - | 1601 | `		{ "sept",4,9 },{ "october",7,10 },{ "november",8,11 },{ "december",8,12 },` |
|      - | 1602 | `		{ "may",3,5 },` |
|      - | 1603 | `		{ "jan",3,1 },{ "feb",3,2 },{ "mar",3,3 },{ "apr",3,4 },{ "jun",3,6 },` |
|      - | 1604 | `		{ "jul",3,7 },{ "aug",3,8 },{ "sep",3,9 },{ "oct",3,10 },{ "nov",3,11 },` |
|      - | 1605 | `		{ "dec",3,12 }` |
|      - | 1606 | `	};` |
|      - | 1607 | `	sxu32 i;` |
|  95626 | 1608 | `	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){` |
|  91848 | 1609 | `		int n = aM[i].n;` |
|  91846 | 1610 | `		if( zEnd - z >= n && SyStrnicmp(z,aM[i].z,(sxu32)n) == 0` |
|  25387 | 1611 | `		 && DtWordEnds(z,zEnd,n) ){` |
|    295 | 1612 | `			*pAdv = n;` |
|    295 | 1613 | `			return aM[i].mo;` |
|      - | 1614 | `		}` |
|  45778 | 1615 | `	}` |
|   3780 | 1616 | `	return 0;` |
|   2038 | 1617 | `}` |
|      - | 1618 | `/*` |
|      - | 1619 | ` * php's SEPARATOR bytes -- the run between two tokens, and it is wider than a` |
|      - | 1620 | ` * space: NUL, tab, newline, space and comma are all skipped there, which is` |
|      - | 1621 | ` * what lets a date string keep the newline of the file it was read from. The` |
|      - | 1622 | ` * full stop is one too but only in places (DtIsSepAt below).` |
|      - | 1623 | ` */` |
|  13316 | 1624 | `static int DtIsSep(int c)` |
|      3 | 1625 | `{` |
|  13319 | 1626 | `	return c == ' ' \|\| c == '\t' \|\| c == '\n' \|\| c == '\0' \|\| c == ',';` |
|      3 | 1627 | `}` |
|      - | 1628 | `/*` |
|      - | 1629 | `` * php's `space` -- the run allowed INSIDE one token, between a count and its`` |
|      - | 1630 | `` * unit, a sign and its digits, or `day` and the `of` behind it. It is narrower`` |
|      - | 1631 | `` * than the run between two tokens: `2 days` and `2\tdays` are php's, while`` |
|      - | 1632 | `` * `2,days`, `2.days` and `2\ndays` are not a relative token at all there.`` |
|      - | 1633 | ` */` |
|   6722 | 1634 | `static int DtIsSpace(int c)` |
|      3 | 1635 | `{` |
|   6725 | 1636 | `	return c == ' ' \|\| c == '\t';` |
|      3 | 1637 | `}` |
|      - | 1638 | `/*` |
|      - | 1639 | ` * ...and the full stop, which php separates with unconditionally. The rules that` |
|      - | 1640 | `` * SPELL a dot -- the day-first `1.2.2020`, the clock's second separator, the`` |
|      - | 1641 | `` * ordinal `2020.102` -- claim their own bytes before the run between tokens is`` |
|      - | 1642 | ``  * ever consulted, so nothing is lost by stepping over the rest: `20240102.2020` `` |
|      - | 1643 | ` * is a date, a separator and a clock there.` |
|      - | 1644 | ` */` |
|  18580 | 1645 | `static int DtIsSepAt(const char *z,const char *zEnd)` |
|      3 | 1646 | `{` |
|  18583 | 1647 | `	if( z >= zEnd ){` |
|   5169 | 1648 | `		return 0;` |
|      - | 1649 | `	}` |
|  13417 | 1650 | `	return z[0] == '.' \|\| DtIsSep((unsigned char)z[0]);` |
|   9293 | 1651 | `}` |
|      - | 1652 | `/*` |
|      - | 1653 | ` * ...and the wider set php tolerates at the ENDS of the string, where a` |
|      - | 1654 | ` * carriage return, a vertical tab and a form feed are allowed as well: a string` |
|      - | 1655 | `` * read from a file keeps its `\r\n` and still parses, while the same bytes`` |
|      - | 1656 | ` * BETWEEN two tokens are an unexpected character in both engines. The comma and` |
|      - | 1657 | ` * the full stop go the other way -- they separate tokens but do not close the` |
|      - | 1658 | `` * string, so `12:00,` parses and `3pm,` is not a meridian at all.`` |
|      - | 1659 | ` */` |
|   5168 | 1660 | `static int DtIsEdgeSep(int c)` |
|      3 | 1661 | `{` |
|   7753 | 1662 | `	return c == ' ' \|\| c == '\t' \|\| c == '\n' \|\| c == '\0'` |
|   7752 | 1663 | `	    \|\| c == '\r' \|\| c == '\v' \|\| c == '\f';` |
|      3 | 1664 | `}` |
|      - | 1665 | `/*` |
|      - | 1666 | ` * php's own first act on a date string is a TRIM -- timelib_strtotime walks` |
|      - | 1667 | ` * isspace() off both ends and hands its scanner what is left -- so every` |
|      - | 1668 | ` * position it reports afterwards is the TRIMMED string's, while the message` |
|      - | 1669 | `` * still prints the string the caller wrote: `new DateTime('  xyz')` blames`` |
|      - | 1670 | `` * position 0 and shows `(x)`, where PHL blamed position 2. The trim is isspace`` |
|      - | 1671 | ` * and NOTHING else, which is what keeps a leading NUL or full stop counting --` |
|      - | 1672 | ` * those are separators the scanner steps over, and stepping over one is a byte` |
|      - | 1673 | `` * gone by (`.xyz` refuses at 1).`` |
|      - | 1674 | ` */` |
|  14026 | 1675 | `static int DtIsCSpace(int c)` |
|      3 | 1676 | `{` |
|  20822 | 1677 | `	return c == ' ' \|\| c == '\t' \|\| c == '\n'` |
|  20942 | 1678 | `	    \|\| c == '\v' \|\| c == '\f' \|\| c == '\r';` |
|      3 | 1679 | `}` |
|   6866 | 1680 | `static void DtTrimEnds(const char **pz,int *pn)` |
|      3 | 1681 | `{` |
|   6869 | 1682 | `	const char *z = *pz;` |
|   6869 | 1683 | `	int n = *pn;` |
|   7095 | 1684 | `	while( n > 0 && DtIsCSpace((unsigned char)z[0]) ){ z++; n--; }` |
|   6941 | 1685 | `	while( n > 0 && DtIsCSpace((unsigned char)z[n-1]) ){ n--; }` |
|   6869 | 1686 | `	*pz = z;` |
|   6869 | 1687 | `	*pn = n;` |
|   6869 | 1688 | `}` |
|      - | 1689 | `/*` |
|      - | 1690 | ` * ...and what the SENTENCE shows of it stops at the first NUL, because php` |
|      - | 1691 | `` * hands the string to a C `%s`. A date string may well carry one -- the scanner`` |
|      - | 1692 | `` * reads a NUL as an ordinary separator, so `"15 january 2020\0),/"` is a real`` |
|      - | 1693 | ` * parse that fails at byte 16 -- and php names that byte while printing only` |
|      - | 1694 | ` * the sixteen before it.` |
|      - | 1695 | ` */` |
|   1940 | 1696 | `static int DtCStrLen(const char *z,int n)` |
|      2 | 1697 | `{` |
|   1942 | 1698 | `	int k = 0;` |
|  14512 | 1699 | `	while( k < n && z[k] != 0 ){ k++; }` |
|   1942 | 1700 | `	return k;` |
|      2 | 1701 | `}` |
|      - | 1702 | `/*` |
|      - | 1703 | ` * Match a weekday name at z (full or 3-letter, case-insensitive). Returns the` |
|      - | 1704 | ` * day-of-week 0=Sunday..6=Saturday and sets *pAdv, or -1.` |
|      - | 1705 | ` *` |
|      - | 1706 | `` * `bLoose` is php's longest-match rule seen from the other side. A name STANDING`` |
|      - | 1707 | ` * ALONE competes with the timezone-name token, which is the longer read of` |
|      - | 1708 | `` * `mons` and `tues` -- so those are an unknown zone there, not a weekday -- while`` |
|      - | 1709 | ` * a name behind a COUNT is inside one rule with it, nothing longer matches, and` |
|      - | 1710 | `` * the letters left over become a zone of their own (`3 mons` is the third Monday`` |
|      - | 1711 | ` * in the military zone S). Only the counted spellings pass it.` |
|      - | 1712 | ` */` |
|   7614 | 1713 | `static int DtMatchWeekdayEx(const char *z,const char *zEnd,int *pAdv,int bLoose)` |
|      3 | 1714 | `{` |
|      - | 1715 | `	static const struct { const char *z; int n; int dow; } aW[] = {` |
|      - | 1716 | `		{ "sunday",6,0 },{ "monday",6,1 },{ "tuesday",7,2 },{ "wednesday",9,3 },` |
|      - | 1717 | `		{ "thursday",8,4 },{ "friday",6,5 },{ "saturday",8,6 },` |
|      - | 1718 | `		{ "sun",3,0 },{ "mon",3,1 },{ "tue",3,2 },{ "wed",3,3 },{ "thu",3,4 },` |
|      - | 1719 | `		{ "fri",3,5 },{ "sat",3,6 }` |
|      - | 1720 | `	};` |
|      - | 1721 | `	sxu32 i;` |
| 110183 | 1722 | `	for( i = 0 ; i < SX_ARRAYSIZE(aW) ; ++i ){` |
| 102953 | 1723 | `		int n = aW[i].n;` |
| 102953 | 1724 | `		if( zEnd - z < n \|\| SyStrnicmp(z,aW[i].z,(sxu32)n) != 0 ){` |
| 102555 | 1725 | `			continue;` |
|      - | 1726 | `		}` |
|      - | 1727 | ``		/* php spells the FULL names with an optional plural `s` and the`` |
|      - | 1728 | ``		 * three-letter abbreviations without one, so `mondays` is a weekday where`` |
|      - | 1729 | ``		 * `mons` is `mon` with an `s` left standing -- which the string then reads`` |
|      - | 1730 | `		 * as a military zone. */` |
|    399 | 1731 | `		if( n > 3 && zEnd - z > n && (z[n] == 's' \|\| z[n] == 'S') ){` |
|     15 | 1732 | `			*pAdv = n + 1;` |
|     15 | 1733 | `			return aW[i].dow;` |
|      - | 1734 | `		}` |
|      - | 1735 | `		/* A full name is six bytes or more and php's timezone-name token stops at` |
|      - | 1736 | `		 * six, so nothing longer competes with it and letters behind it are the` |
|      - | 1737 | ``		 * next token's (`mondayx` is Monday in the military zone X). */`` |
|    385 | 1738 | `		if( bLoose \|\| n > 3 \|\| zEnd - z == n \|\| !SyisAlpha(z[n]) ){` |
|    371 | 1739 | `			*pAdv = n;` |
|    371 | 1740 | `			return aW[i].dow;` |
|      - | 1741 | `		}` |
|      8 | 1742 | `	}` |
|   7233 | 1743 | `	return -1;` |
|   3810 | 1744 | `}` |
|      - | 1745 | `/*` |
|      - | 1746 | `` * php's `americanshort`, `month "/" day` -- the American date with no year at`` |
|      - | 1747 | `` * all (`4/20`, `12/31`, `10/2`), which this engine had only in its`` |
|      - | 1748 | ` * month/day/year form, so the most common way an American program spells a date` |
|      - | 1749 | ` * without one did not parse.` |
|      - | 1750 | ` *` |
|      - | 1751 | ` * Its fields are spelled INSIDE the pattern rather than range-checked` |
|      - | 1752 | `` * afterwards, `"0"? [0-9] \| "1"[0-2]` and `[0-2]?[0-9] \| "3"[01]`, so a`` |
|      - | 1753 | ` * two-digit reading that is out of range leaves its second digit to the string` |
|      - | 1754 | `` * instead of sinking the rule: `4/32` is the 3rd with a stray `2` behind it and`` |
|      - | 1755 | `` * `13/20` an unexpected `1` and then March the 20th. Zero matches both fields`` |
|      - | 1756 | `` * and normalizes, which is what makes `0/1` December of the year before.`` |
|      - | 1757 | `` * php's ordinal suffix rides the day with NOTHING between them, so `4/20th` is`` |
|      - | 1758 | `` * the 20th where `4/20 th` is the 20th beside a zone it cannot find.`` |
|      - | 1759 | ` *` |
|      - | 1760 | ` * The YEAR is left alone -- php's action writes only the month and the day --` |
|      - | 1761 | `` * which is what keeps `4/20` on the base moment's year.`` |
|      - | 1762 | ` */` |
|   1554 | 1763 | `static int DtTryAmericanShort(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1764 | `	dt_parsed *p,const char *zIn)` |
|      2 | 1765 | `{` |
|   1556 | 1766 | `	const char *zTok = z;` |
|      - | 1767 | `	int mo,d,rc;` |
|   1556 | 1768 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|    ! 0 | 1769 | `		return 0;` |
|      - | 1770 | `	}` |
|   1556 | 1771 | `	mo = z[0] - '0';` |
|   1556 | 1772 | `	if( z+1 < zEnd && SyisDigit(z[1]) && (z[0] == '0' \|\| (z[0] == '1' && z[1] <= '2')) ){` |
|    369 | 1773 | `		mo = mo*10 + (z[1]-'0');` |
|    369 | 1774 | `		z += 2;` |
|    185 | 1775 | `	}else{` |
|   1188 | 1776 | `		z++;` |
|      - | 1777 | `	}` |
|   1556 | 1778 | `	if( z >= zEnd \|\| z[0] != '/' ){` |
|   1496 | 1779 | `		return 0;` |
|      - | 1780 | `	}` |
|     61 | 1781 | `	z++;` |
|     61 | 1782 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|      3 | 1783 | `		return 0;` |
|      - | 1784 | `	}` |
|     59 | 1785 | `	d = z[0] - '0';` |
|     58 | 1786 | `	if( z+1 < zEnd && SyisDigit(z[1])` |
|     44 | 1787 | `	 && (z[0] <= '2' \|\| (z[0] == '3' && z[1] <= '1')) ){` |
|     41 | 1788 | `		d = d*10 + (z[1]-'0');` |
|     41 | 1789 | `		z += 2;` |
|     21 | 1790 | `	}else{` |
|     19 | 1791 | `		z++;` |
|      - | 1792 | `	}` |
|     59 | 1793 | `	if( DtIsOrdinal(z,zEnd) ){` |
|     11 | 1794 | `		z += 2;` |
|      5 | 1795 | `	}` |
|     59 | 1796 | `	*pzOut = z;` |
|     59 | 1797 | `	if( (rc = DtMarkDate(p,zTok,zIn)) != 0 ){` |
|      7 | 1798 | `		return rc;` |
|      - | 1799 | `	}` |
|     53 | 1800 | `	p->m = mo;` |
|     53 | 1801 | `	p->d = d;` |
|     53 | 1802 | `	return 1;` |
|    779 | 1803 | `}` |
|      - | 1804 | `/*` |
|      - | 1805 | ` * Try to read a textual-month date at z, in either order:` |
|      - | 1806 | ` *   MonthName [Day] [Year]   ("Jan 15 2020", "January", "January 2020")` |
|      - | 1807 | ` *   Day MonthName [Year]     ("15 January 2020", "15th Jan")` |
|      - | 1808 | ` * Only what the string SPELLS is written: a missing day stays unset (so a bare` |
|      - | 1809 | ` * month name keeps the base day, php's answer) except when a year was given,` |
|      - | 1810 | ` * which is php's own "January 2020" -> the 1st. Day may carry an ordinal suffix,` |
|      - | 1811 | ` * fields may be comma-separated, month names are case-insensitive, and an` |
|      - | 1812 | ` * optional time-of-day suffix + trailing UTC/GMT is consumed. Returns 0 (not a` |
|      - | 1813 | ` * month date — caller falls through, *pzOut untouched), 1 on success, or a` |
|      - | 1814 | ` * DtParse error code (out-of-range day).` |
|      - | 1815 | ` */` |
|   2432 | 1816 | `static int DtTryMonthDate(const char *z,const char *zEnd,const char **pzOut,` |
|      - | 1817 | `	dt_parsed *p,const char *zIn)` |
|      2 | 1818 | `{` |
|   2434 | 1819 | `	const char *zTok = z;` |
|   2434 | 1820 | `	const char *zAfterMon = 0;` |
|   2434 | 1821 | `	int mo,d = 1,adv,haveDay = 0,haveYear = 0;` |
|   2434 | 1822 | `	int bMonthFirst = 0,nSuf = 0,bClock = 0;` |
|   2434 | 1823 | `	sxi64 y = 0;` |
|      - | 1824 | `	int rcT;` |
|      - | 1825 | `` /* php's textual-date rule spells its run with the full stop in it (`5.january` `` |
|      - | 1826 | `` * and `january.5.2020` are dates there) and without the comma -- except between`` |
|      - | 1827 | ` * the DAY and the YEAR, which is where the comma everyone writes goes` |
|      - | 1828 | `` * (`January 15, 2020`, and `January, 15 2020` is no date at all). */`` |
|      - | 1829 | `#define MDSKIPWS() while( z < zEnd && (DtIsSpace((unsigned char)z[0]) \|\| z[0]=='.') ){ z++; }` |
|      - | 1830 | ``/* ...and the run BEHIND the day, which is php's `[,.stndrh\t ]+` -- the ordinal`` |
|      - | 1831 | ` * suffix and the comma before a year are the same set, greedy, and it is` |
|      - | 1832 | ` * REQUIRED (or a NUL, or the end of the string) when no year follows: that is` |
|      - | 1833 | `` * what makes `january 12x` no date at all while `january 12sd2020` is one, and`` |
|      - | 1834 | `` * what leaves `january 12 sat` reading `at` as a zone. */`` |
|      - | 1835 | `#define MDISSUF(c) ((c)==','\|\|(c)=='.'\|\|(c)=='s'\|\|(c)=='t'\|\|(c)=='n'\|\|(c)=='d' \` |
|      - | 1836 | `	\|\|(c)=='r'\|\|(c)=='h'\|\|(c)=='\t'\|\|(c)==' ')` |
|   2434 | 1837 | `	if( (mo = DtMatchMonth(z,zEnd,&adv)) != 0 ){` |
|      - | 1838 | `		/* MonthName [Day] [Year]. A 4-digit number here is the YEAR, not the day` |
|      - | 1839 | `		 * ("January 2020" is month+year, day defaults); a 1-2 digit number is the day. */` |
|    201 | 1840 | `		z += adv;` |
|    201 | 1841 | `		zAfterMon = z;` |
|    439 | 1842 | `		MDSKIPWS();` |
|    201 | 1843 | `		if( z < zEnd && SyisDigit(z[0]) ){` |
|    119 | 1844 | `			int nrun = 0;` |
|    119 | 1845 | `			const char *zp = z;` |
|    421 | 1846 | `			while( zp < zEnd && SyisDigit(zp[0]) && nrun < 4 ){ zp++; nrun++; }` |
|    119 | 1847 | `			if( nrun < 4 ){` |
|     79 | 1848 | `				d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|     79 | 1849 | `				haveDay = 1;` |
|     79 | 1850 | `				bMonthFirst = 1;` |
|    210 | 1851 | `				while( z < zEnd && MDISSUF((unsigned char)z[0]) ){ z++; nSuf++; }` |
|      - | 1852 | ``				/* php's `dateshortwithtimeshort`: a month, a day and a CLOCK are`` |
|      - | 1853 | `				 * ONE token there, and it reads longer than the year the same` |
|      - | 1854 | ``				 * digits would be -- which is what makes `january 12 12:00` noon`` |
|      - | 1855 | ``				 * on the 12th where `january 12 12` is the year 2012, and`` |
|      - | 1856 | ``				 * `january 12 123:00` the year 123 with a refusal behind it. */`` |
|     79 | 1857 | `				bClock = DtClockFollows(z,zEnd);` |
|     39 | 1858 | `			}` |
|     60 | 1859 | `		}` |
|   2334 | 1860 | `	}else if( SyisDigit(z[0]) ){` |
|      - | 1861 | `		/* Day MonthName [Year] */` |
|   1498 | 1862 | `		d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|   1498 | 1863 | `		if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|   1498 | 1864 | `		haveDay = 1;` |
|   2636 | 1865 | `		MDSKIPWS();` |
|   1498 | 1866 | `		if( (mo = DtMatchMonth(z,zEnd,&adv)) == 0 ){ return 0; }` |
|     87 | 1867 | `		z += adv;` |
|    200 | 1868 | `		MDSKIPWS();` |
|     44 | 1869 | `	}else{` |
|    737 | 1870 | `		return 0;` |
|      - | 1871 | `	}` |
|      - | 1872 | `	/* The optional YEAR -- but php's rule spells the run between the day and it` |
|      - | 1873 | ``	 * as REQUIRED, so `january 124` is no date at all where `january 12 4` and`` |
|      - | 1874 | ``	 * `january 12s4` are the year 2004. */`` |
|    286 | 1875 | `	if( !bClock && !(bMonthFirst && haveDay && nSuf == 0)` |
|    272 | 1876 | `	 && z < zEnd && SyisDigit(z[0]) ){` |
|    153 | 1877 | `		int ny = 0;` |
|    153 | 1878 | `		y = 0;` |
|    751 | 1879 | `		while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ y = y*10 + (z[0]-'0'); z++; ny++; }` |
|    153 | 1880 | `		if( ny <= 2 ){` |
|      5 | 1881 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|    ! 0 | 1882 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|      2 | 1883 | `		}` |
|    153 | 1884 | `		haveYear = 1;` |
|     76 | 1885 | `	}` |
|      - | 1886 | `	/* php spells the day's range inside the pattern too, so a day past 31 is not` |
|      - | 1887 | ``	 * this rule at all and the token is refused where it STARTS (`87 january` is`` |
|      - | 1888 | `	 * php's position 0), not where the month name ends. */` |
|      - | 1889 | ``	/* php's `datenoyear` ends in that run, and spells the day's range inside the`` |
|      - | 1890 | `	 * pattern as well -- so with a day past 31, or with nothing behind the day` |
|      - | 1891 | `	 * and no year to close the rule, this text is not that token. The MONTH NAME` |
|      - | 1892 | `	 * still is one of its own, though, and only the month-FIRST spelling can fall` |
|      - | 1893 | ``	 * back to it: `january 12x` is January with the digits left to the string,`` |
|      - | 1894 | ``	 * while `87 january` is php's refusal at position 0. */`` |
|    287 | 1895 | `	if( d > 31 \|\| (bMonthFirst && !haveYear && nSuf == 0 && z < zEnd && z[0] != 0) ){` |
|     15 | 1896 | `		if( !bMonthFirst ){` |
|      3 | 1897 | `			return 0;` |
|      - | 1898 | `		}` |
|     13 | 1899 | `		z = zAfterMon;` |
|     13 | 1900 | `		haveDay = 0;` |
|     13 | 1901 | `		haveYear = 0;` |
|     13 | 1902 | `		d = 1;` |
|      6 | 1903 | `	}` |
|      - | 1904 | `	/* optional time-of-day suffix */` |
|    285 | 1905 | `	*pzOut = z;` |
|    285 | 1906 | `	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }` |
|    261 | 1907 | `	p->m = mo;` |
|    261 | 1908 | `	if( haveDay ){ p->d = d; }` |
|    111 | 1909 | `	else if( haveYear ){ p->d = 1; }` |
|    261 | 1910 | `	if( haveYear ){ p->y = y; }` |
|    113 | 1911 | `	else if( bMonthFirst && haveDay ){` |
|      - | 1912 | ``		/* php's `datenoyear` writes the month and the day and UNSETS the year,`` |
|      - | 1913 | `		 * which is the whole difference between it and the day-first spelling:` |
|      - | 1914 | ``		 * `@100 january 12` has no year where `@100 12 january` keeps 1970. */`` |
|     25 | 1915 | `		p->y = DT_UNSET;` |
|     12 | 1916 | `	}` |
|    261 | 1917 | `	if( bClock ){` |
|      5 | 1918 | `		rcT = DtReadTimeOfDay(&z,zEnd,zIn,p);` |
|      5 | 1919 | `		*pzOut = z;` |
|      5 | 1920 | `		if( rcT < 0 ){ return rcT; }` |
|      5 | 1921 | `		return 1;` |
|      - | 1922 | `	}` |
|    257 | 1923 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,p);` |
|    257 | 1924 | `	*pzOut = z;` |
|    257 | 1925 | `	if( rcT != 0 ){ return rcT; }` |
|    257 | 1926 | `	return 1;` |
|      - | 1927 | `#undef MDSKIPWS` |
|      - | 1928 | `#undef MDISSUF` |
|   1218 | 1929 | `}` |
|      - | 1930 | `/*` |
|      - | 1931 | ` * php's reltextnumber -- the ORDINAL WORDS that stand where a relative COUNT` |
|      - | 1932 | `` * would. `first` through `twelfth` are 1..12 and the navigation four are the`` |
|      - | 1933 | `` * same rule's 1, 0 and -1, which is why `next day` and `first day` are one`` |
|      - | 1934 | `` * move and `second day` two of them.`` |
|      - | 1935 | ` *` |
|      - | 1936 | ` * Answers the bytes the word takes (0 for anything else) and says which half it` |
|      - | 1937 | `` * came from: php's `... week` SPECIAL -- the move to that week's Monday --`` |
|      - | 1938 | `` * belongs to the navigation words alone, so `next week` is that Monday while`` |
|      - | 1939 | `` * `first week` is a refusal and `first weeks` seven ordinary days.`` |
|      - | 1940 | ` */` |
|  12802 | 1941 | `static int DtRelWord(const char *z,const char *zEnd,sxi64 *pVal,int *pbNav)` |
|      3 | 1942 | `{` |
|      - | 1943 | `	static const struct { const char *zWord; int nWord; int iVal; int bNav; } aWord[] = {` |
|      - | 1944 | `		{ "previous", 8, -1, 1 }, { "next",     4,  1, 1 },` |
|      - | 1945 | `		{ "last",     4, -1, 1 }, { "this",     4,  0, 1 },` |
|      - | 1946 | `		{ "first",    5,  1, 0 }, { "second",   6,  2, 0 },` |
|      - | 1947 | `		{ "third",    5,  3, 0 }, { "fourth",   6,  4, 0 },` |
|      - | 1948 | `		{ "fifth",    5,  5, 0 }, { "sixth",    5,  6, 0 },` |
|      - | 1949 | `		{ "seventh",  7,  7, 0 }, { "eighth",   6,  8, 0 },` |
|      - | 1950 | `		{ "ninth",    5,  9, 0 }, { "tenth",    5, 10, 0 },` |
|      - | 1951 | `		{ "eleventh", 8, 11, 0 }, { "twelfth",  7, 12, 0 }` |
|      - | 1952 | `	};` |
|      - | 1953 | `	int k;` |
| 209149 | 1954 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(aWord) ; k++ ){` |
| 196991 | 1955 | `		int n = aWord[k].nWord;` |
| 196988 | 1956 | `		if( zEnd - z >= n && SyStrnicmp(z,aWord[k].zWord,n) == 0` |
|  73937 | 1957 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|    645 | 1958 | `			*pVal  = (sxi64)aWord[k].iVal;` |
|    645 | 1959 | `			*pbNav = aWord[k].bNav;` |
|    645 | 1960 | `			return n;` |
|      - | 1961 | `		}` |
|  98175 | 1962 | `	}` |
|  12161 | 1963 | `	return 0;` |
|   6404 | 1964 | `}` |
|      - | 1965 | `/*` |
|      - | 1966 | ` * Apply php's relative UNIT word at z with the amount v, and answer the bytes it` |
|      - | 1967 | ` * takes -- 0 when there is no unit word here. Shared by the two spellings that` |
|      - | 1968 | `` * reach one: a number in front of it, and php's `this`/`next`/`last`/`previous`,`` |
|      - | 1969 | `` * which is the same rule with the amount 0, 1 or -1 (`next hour`, `last year`).`` |
|      - | 1970 | ` */` |
|   2328 | 1971 | `static int DtRelUnit(const char *z,const char *zEnd,sxi64 v,dt_parsed *p,int *pbSpecial)` |
|      3 | 1972 | `{` |
|   2331 | 1973 | `	*pbSpecial = 0;` |
|      - | 1974 | `/* A unit word is never a token on its own -- the NUMBER (or the navigation` |
|      - | 1975 | ` * word) in front of it started the match, so nothing competes with it at its` |
|      - | 1976 | ``  * own position and letters behind it belong to whatever comes next: `+1 dayx` `` |
|      - | 1977 | `` * is a day and the military zone X, `+1 dayxyz` a day and an unknown zone. */`` |
|      - | 1978 | `#define DT_UNITEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0)` |
|      - | 1979 | `	/* php's SUB-SECOND relative units, checked before the words they are` |
|      - | 1980 | ``	 * prefixes of ("ms" would otherwise swallow "msec"). `us` is NOT one of`` |
|      - | 1981 | `	 * them there, and neither is the Greek mu -- only U+00B5, the MICRO` |
|      - | 1982 | `	 * SIGN, which is the two bytes 0xC2 0xB5 here. They accumulate apart` |
|      - | 1983 | ``	 * from the seconds and carry into them in DtApplyFields, so `-500`` |
|      - | 1984 | ``	 * microseconds` from midnight is the previous day's 23:59:59.999500. */`` |
|   2331 | 1985 | `	if( DT_UNITEQ("microseconds",12) ){ p->rus = DtWAdd(p->rus,v);        return 12; }` |
|   2295 | 1986 | `	else if( DT_UNITEQ("microsecond",11) ){ p->rus = DtWAdd(p->rus,v);    return 11; }` |
|   2281 | 1987 | `	else if( DT_UNITEQ("milliseconds",12) ){ p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 12; }` |
|   2275 | 1988 | `	else if( DT_UNITEQ("millisecond",11) ){ p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 11; }` |
|   2273 | 1989 | `	else if( DT_UNITEQ("usecs",5) )  { p->rus = DtWAdd(p->rus,v);         return 5; }` |
|   2271 | 1990 | `	else if( DT_UNITEQ("usec",4) )   { p->rus = DtWAdd(p->rus,v);         return 4; }` |
|   2265 | 1991 | `	else if( DT_UNITEQ("msecs",5) )  { p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 5; }` |
|   2261 | 1992 | `	else if( DT_UNITEQ("msec",4) )   { p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 4; }` |
|   2251 | 1993 | `	else if( DT_UNITEQ("\xc2\xb5s",3) ){ p->rus = DtWAdd(p->rus,v);       return 3; }` |
|   2243 | 1994 | `	else if( DT_UNITEQ("ms",2) )     { p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 2; }` |
|   2225 | 1995 | `	else if( DT_UNITEQ("seconds",7) ){ p->rs = DtWAdd(p->rs,v);           return 7; }` |
|   2205 | 1996 | `	else if( DT_UNITEQ("second",6) ) { p->rs = DtWAdd(p->rs,v);           return 6; }` |
|   2199 | 1997 | `	else if( DT_UNITEQ("secs",4) )   { p->rs = DtWAdd(p->rs,v);           return 4; }` |
|   2197 | 1998 | `	else if( DT_UNITEQ("sec",3) )    { p->rs = DtWAdd(p->rs,v);           return 3; }` |
|   2193 | 1999 | `	else if( DT_UNITEQ("minutes",7) ){ p->ri = DtWAdd(p->ri,v);           return 7; }` |
|   2187 | 2000 | `	else if( DT_UNITEQ("minute",6) ) { p->ri = DtWAdd(p->ri,v);           return 6; }` |
|   2185 | 2001 | `	else if( DT_UNITEQ("mins",4) )   { p->ri = DtWAdd(p->ri,v);           return 4; }` |
|   2183 | 2002 | `	else if( DT_UNITEQ("min",3) )    { p->ri = DtWAdd(p->ri,v);           return 3; }` |
|   2181 | 2003 | `	else if( DT_UNITEQ("hours",5) )  { p->rh = DtWAdd(p->rh,v);           return 5; }` |
|   2151 | 2004 | `	else if( DT_UNITEQ("hour",4) )   { p->rh = DtWAdd(p->rh,v);           return 4; }` |
|   2133 | 2005 | `	else if( DT_UNITEQ("days",4) )   { p->rd = DtWAdd(p->rd,v);           return 4; }` |
|   2054 | 2006 | `	else if( DT_UNITEQ("day",3) )    { p->rd = DtWAdd(p->rd,v);           return 3; }` |
|      - | 2007 | `	/* php's business-day words go BEFORE the plain week, because its scanner` |
|      - | 2008 | ``	 * takes the longest of the two and `week` is their prefix. */`` |
|   1945 | 2009 | `	else if( DT_UNITEQ("weekdays",8) ){ p->bWeekdays = 1; *pbSpecial = 1; p->iWeekdays = v; return 8; }` |
|   1805 | 2010 | `	else if( DT_UNITEQ("weekday",7) ) { p->bWeekdays = 1; *pbSpecial = 1; p->iWeekdays = v; return 7; }` |
|   1771 | 2011 | `	else if( DT_UNITEQ("weeks",5) )  { p->rd = DtWAdd(p->rd,DtWMul(v,7)); return 5; }` |
|   1749 | 2012 | `	else if( DT_UNITEQ("week",4) )   { p->rd = DtWAdd(p->rd,DtWMul(v,7)); return 4; }` |
|   1651 | 2013 | `	else if( DT_UNITEQ("fortnights",10) ){ p->rd = DtWAdd(p->rd,DtWMul(v,14)); return 10; }` |
|   1649 | 2014 | `	else if( DT_UNITEQ("fortnight",9) )  { p->rd = DtWAdd(p->rd,DtWMul(v,14)); return 9; }` |
|   1641 | 2015 | `	else if( DT_UNITEQ("months",6) ) { p->rm = DtWAdd(p->rm,v);           return 6; }` |
|   1629 | 2016 | `	else if( DT_UNITEQ("month",5) )  { p->rm = DtWAdd(p->rm,v);           return 5; }` |
|   1423 | 2017 | `	else if( DT_UNITEQ("years",5) )  { p->ry = DtWAdd(p->ry,v);           return 5; }` |
|   1419 | 2018 | `	else if( DT_UNITEQ("year",4) )   { p->ry = DtWAdd(p->ry,v);           return 4; }` |
|      - | 2019 | `	/* php's BUSINESS-day count, which is not a field of the vector at all but a` |
|      - | 2020 | `	 * move of its own (see DtAdjustWeekdays) */` |
|      - | 2021 | `	/* php SETS this one rather than adding to it, so the last count in the string` |
|      - | 2022 | `	 * is the only one that moves anything. */` |
|   1399 | 2023 | `	return 0;` |
|      - | 2024 | `#undef DT_UNITEQ` |
|   1167 | 2025 | `}` |
|      - | 2026 | `/*` |
|      - | 2027 | ` * How many bytes a relative UNIT word would take here, without applying it.` |
|      - | 2028 | ` * php's scanner takes the LONGEST rule that matches, and a weekday name is the` |
|      - | 2029 | `` * prefix of two unit words (`mon` of `month`, `sat` of nothing but `mon` is`` |
|      - | 2030 | ` * enough): the counted spellings ask this before they claim a weekday, which is` |
|      - | 2031 | `` * what keeps `next month` a month and `3 months` three of them.`` |
|      - | 2032 | ` */` |
|    486 | 2033 | `static int DtRelUnitLen(const char *z,const char *zEnd,const dt_parsed *p)` |
|      1 | 2034 | `{` |
|    487 | 2035 | `	dt_parsed sTmp = *p;` |
|    487 | 2036 | `	int bSpec = 0;` |
|    487 | 2037 | `	return DtRelUnit(z,zEnd,0,&sTmp,&bSpec);` |
|      1 | 2038 | `}` |
|      - | 2039 | `/*` |
|      - | 2040 | ` * php's date-string parse, onto the field vector: absolute forms` |
|      - | 2041 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]" \|` |
|      - | 2042 | ` * a textual month date, the keywords today/midnight/noon/tomorrow/yesterday, the` |
|      - | 2043 | ` * weekday and month navigation words, and relative sequences` |
|      - | 2044 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". NOTHING is applied` |
|      - | 2045 | ` * here -- DtApplyFields does that, in php's order, once the whole string is read.` |
|      - | 2046 | ` * Returns 0 on success, or the byte position of the first unparseable character` |
|      - | 2047 | ` * +1 (for php's "at position N" message).` |
|      - | 2048 | ` */` |
|      - | 2049 | `/*` |
|      - | 2050 | `` * php's `@epoch` token, which its scanner reads ANYWHERE in a string rather`` |
|      - | 2051 | `` * than only at its head: `2020-01-02 @100` and `12:00 @100` are the epoch`` |
|      - | 2052 | ` * there, not refusals.` |
|      - | 2053 | ` *` |
|      - | 2054 | `` * The shape is `"@" "-"? [0-9]+ ("." [0-9]{0,6})?`. A PLUS is no part of it, so`` |
|      - | 2055 | `` * `@+100` is an unexpected `@` with a UTC offset behind it; and the fraction`` |
|      - | 2056 | ``  * stops at SIX digits, which leaves the seventh to the string (`@100.1234567` `` |
|      - | 2057 | `` * refuses on it while `@100.1234567890` reads the trailing four as a year).`` |
|      - | 2058 | ` *` |
|      - | 2059 | ` * The ACTION is php's own order, and the order is what shows: TIMELIB_UNHAVE_DATE` |
|      - | 2060 | ` * and TIMELIB_UNHAVE_TIME first -- which ZERO the civil fields rather than` |
|      - | 2061 | ` * unsetting them, so an epoch behind a date reads back as the year 0, not as the` |
|      - | 2062 | ` * date -- then TIMELIB_HAVE_TZ, then the value. The middle step is a RETURN when` |
|      - | 2063 | `` * the string already named a zone, so `UTC @100` leaves nothing behind but those`` |
|      - | 2064 | ` * zeroes: neither 1970 nor the seconds are ever written. An empty fraction is` |
|      - | 2065 | `` * php's `Found unexpected data`, raised at the `@` and after the value, which is`` |
|      - | 2066 | `` * why `@100.,UTC` still reads back as 1970 plus a hundred seconds.`` |
|      - | 2067 | ` *` |
|      - | 2068 | ` * Advances *pz over what it took; answers 0 when this is not the token, 1 when` |
|      - | 2069 | ` * it is, or an error code in DtParse's own convention.` |
|      - | 2070 | ` */` |
|   7134 | 2071 | `static int DtTryEpoch(const char **pz,const char *zEnd,dt_parsed *p,const char *zIn)` |
|      3 | 2072 | `{` |
|   7137 | 2073 | `	const char *z = *pz,*zAt = z;` |
|   7137 | 2074 | `	sxi64 v = 0,us = 0;` |
|   7137 | 2075 | `	int neg = 0,nDig = 0,bDot = 0,nFrac = 0,k,rc;` |
|   7137 | 2076 | `	if( z >= zEnd \|\| z[0] != '@' ){` |
|   6765 | 2077 | `		return 0;` |
|      - | 2078 | `	}` |
|    374 | 2079 | `	z++;` |
|    374 | 2080 | `	if( z < zEnd && z[0] == '-' ){ neg = 1; z++; }` |
|    374 | 2081 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|      - | 2082 | ``		/* php's lexer never matched a token here at all: the `@` is the refusal */`` |
|      9 | 2083 | `		return (int)(zAt - zIn) + 1;` |
|      - | 2084 | `	}` |
|   1506 | 2085 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|   1142 | 2086 | `		if( nDig < DT_DIGITS_SAFE ){ v = v*10 + (z[0]-'0'); }` |
|   1142 | 2087 | `		nDig++;` |
|   1142 | 2088 | `		z++;` |
|      2 | 2089 | `	}` |
|    366 | 2090 | `	if( z < zEnd && z[0] == '.' ){` |
|     45 | 2091 | `		bDot = 1;` |
|     45 | 2092 | `		z++;` |
|    129 | 2093 | `		while( nFrac < 6 && z < zEnd && SyisDigit(z[0]) ){` |
|     85 | 2094 | `			us = us*10 + (z[0]-'0');` |
|     85 | 2095 | `			nFrac++;` |
|     85 | 2096 | `			z++;` |
|      1 | 2097 | `		}` |
|    225 | 2098 | `		for( k = nFrac ; k < 6 ; k++ ){ us *= 10; }` |
|     22 | 2099 | `	}` |
|    366 | 2100 | `	*pz = z;` |
|    366 | 2101 | `	if( nDig > DT_DIGITS_EPOCH ){` |
|      - | 2102 | ``		/* php reports it at the `@`, with its own reason. */`` |
|      9 | 2103 | `		return -((int)(zAt - zIn) + 1) - DT_ERR_RANGE;` |
|      - | 2104 | `	}` |
|      - | 2105 | `	/* php's order: HAVE_RELATIVE, then UNHAVE_DATE and UNHAVE_TIME, then` |
|      - | 2106 | `	 * HAVE_TZ -- so an epoch that bails on the zone has already marked the parse` |
|      - | 2107 | `	 * relative, and date_parse() shows an all-zero block for it. */` |
|    358 | 2108 | `	p->bHaveRel = 1;` |
|    358 | 2109 | `	p->y = p->m = p->d = 0;` |
|    358 | 2110 | `	p->bHaveDate = 0;` |
|    358 | 2111 | `	DtUnhaveTime(p);` |
|    358 | 2112 | `	rc = DtZoneCount(p);` |
|    358 | 2113 | `	if( rc < 0 ){` |
|    ! 0 | 2114 | `		return -((int)(zAt - zIn) + 1) - DT_ERR_DZONE;` |
|      - | 2115 | `	}` |
|    358 | 2116 | `	if( rc > 0 ){` |
|     13 | 2117 | `		DtWarnPend(p,(int)(zAt - zIn),"Double timezone specification");` |
|     13 | 2118 | `		return 1;   /* php returns from inside HAVE_TZ: nothing below runs */` |
|      - | 2119 | `	}` |
|    346 | 2120 | `	DtZoneStore(p,0,0,0,0);` |
|    346 | 2121 | `	p->bEpoch = 1;` |
|    346 | 2122 | `	p->y = 1970; p->m = 1; p->d = 1;` |
|    346 | 2123 | `	p->h = p->i = p->s = p->us = 0;` |
|    346 | 2124 | `	p->bUsUnset = 0;` |
|    346 | 2125 | `	if( neg ){ v = -v; }` |
|      - | 2126 | `	/* php adds the fraction to the RELATIVE microseconds with the token's own` |
|      - | 2127 | ``	 * sign rather than borrowing a second for it, so `@-1.5` is -1s and -500000us`` |
|      - | 2128 | `	 * there -- the same instant, and the count date_parse() shows. Being relative` |
|      - | 2129 | ``	 * is also what makes a later `tomorrow`, whose whole job is to zero the`` |
|      - | 2130 | `	 * clock, leave it standing. */` |
|    346 | 2131 | `	if( nFrac > 0 ){` |
|     35 | 2132 | `		p->rus = neg ? -us : us;` |
|     17 | 2133 | `	}` |
|    346 | 2134 | `	p->rs = DtWAdd(p->rs,v);` |
|    346 | 2135 | `	if( bDot && nFrac == 0 ){` |
|      9 | 2136 | `		return -((int)(zAt - zIn) + 1) - DT_ERR_UNEXPDATA;` |
|      - | 2137 | `	}` |
|    338 | 2138 | `	return 1;` |
|   3570 | 2139 | `}` |
|      - | 2140 | `/* Forward: the scan publishes what it collects, and words a refusal in php's` |
|      - | 2141 | ` * own terms -- both live below, beside the record they write to. */` |
|      - | 2142 | `static const char * DtParseErr(const char *zIn,int nLen,int iErrPos,int *piPos,char *pcAt);` |
|      - | 2143 | `static void DtRecErr(phl_dt_lasterr *pRec,int iPos,const char *zMsg);` |
|      - | 2144 | `static void DtRecWarn(phl_dt_lasterr *pRec,int iPos,const char *zMsg);` |
|      - | 2145 | `static void DtRecReset(phl_dt_lasterr *pRec);` |
|      - | 2146 | `static int DtDaysInMonth(sxi64 y,int m);` |
|   5168 | 2147 | `static int DtParseFields(const char *zIn,int nLen,dt_parsed *p,phl_dt_lasterr *pRec)` |
|      3 | 2148 | `{` |
|      - | 2149 | `	const char *z,*zEnd;` |
|   5171 | 2150 | `	const char *zPrev = 0;` |
|   5171 | 2151 | `	int bAny = 0;` |
|   5171 | 2152 | `	int iRc,iFirst = 0,k;` |
|      - | 2153 | `	/* php refuses an EMPTY string before it does anything else, and the test is` |
|      - | 2154 | `	 * on what the caller handed over rather than on what the trim leaves: a` |
|      - | 2155 | ``	 * string of blanks parses as `now`. The constructors never see it, because`` |
|      - | 2156 | ``	 * php hands THEM the word `now` in its place -- only modify() and the`` |
|      - | 2157 | `	 * component readers do. */` |
|   5171 | 2158 | `	if( nLen < 1 ){` |
|      3 | 2159 | `		DtRecErr(pRec,0,"Empty string");` |
|      3 | 2160 | `		return -1 - DT_ERR_EMPTY;` |
|      - | 2161 | `	}` |
|      - | 2162 | `	/* php's trim comes FIRST and the positions below are all measured from what` |
|      - | 2163 | `	 * it leaves, so rebase on it here and every rule inherits the answer. */` |
|   5169 | 2164 | `	DtTrimEnds(&zIn,&nLen);` |
|   5169 | 2165 | `	z = zIn;` |
|   5169 | 2166 | `	zEnd = &zIn[nLen];` |
|      - | 2167 | `	/* The wider set php tolerates at the trailing END -- a carriage return, a` |
|      - | 2168 | `	 * vertical tab, a form feed, a NUL -- closes the string; at the FRONT the` |
|      - | 2169 | `	 * trim has taken what it takes and everything left is an ordinary token` |
|      - | 2170 | `	 * separator, which DT_SKIP_WS below reads. Stepping over more than that` |
|      - | 2171 | ``	 * loses a byte php refuses: `.\rjanuary` is an unexpected `\r` there. */`` |
|   5173 | 2172 | `	while( zEnd > z && DtIsEdgeSep((unsigned char)zEnd[-1]) ){ zEnd--; }` |
|      - | 2173 | `#define DT_SKIP_WS() while( DtIsSepAt(z,zEnd) ){ z++; }` |
|      - | 2174 | ``/* ...and the run INSIDE one token, which is php's narrower `space`. */`` |
|      - | 2175 | `#define DT_SPACE() while( z < zEnd && DtIsSpace((unsigned char)z[0]) ){ z++; }` |
|      - | 2176 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|      - | 2177 | `	&& DtWordEnds(z,zEnd,nKw))` |
|      - | 2178 | ``/* php's SINGULAR `week`, the word its `... week` special is spelled with. It is`` |
|      - | 2179 | ` * only ever the TAIL of a longer rule, so nothing competes at its own position` |
|      - | 2180 | `` * and letters behind it belong to whatever comes next (`this weekjanuary` is`` |
|      - | 2181 | ` * that week and then January) -- except any UNIT word that reads LONGER here,` |
|      - | 2182 | `` * which is that rule instead: `weeks`, `weekday` and `weekdays`. */`` |
|      - | 2183 | `#define DT_WEEKSING() (zEnd-z >= 4 && SyStrnicmp(z,"week",4) == 0 \` |
|      - | 2184 | `	&& DtRelUnitLen(z,zEnd,p) <= 4)` |
|      - | 2185 | `/*` |
|      - | 2186 | ` * php's scanner RECORDS a refusal and reads on, so an error is not the end of` |
|      - | 2187 | ` * the parse: the reason and the byte are published and the walk resumes one` |
|      - | 2188 | ` * byte past what was named -- which is exactly where php's catch-all rule` |
|      - | 2189 | ` * leaves its cursor. A token that MATCHED and whose action then complained is` |
|      - | 2190 | ` * already behind the cursor, because the rule that raised it moved past it, so` |
|      - | 2191 | ` * taking whichever of the two is FURTHER covers both kinds. The first code is` |
|      - | 2192 | ` * kept: it is the one the constructors put in their sentence.` |
|      - | 2193 | ` */` |
|      - | 2194 | `#define DT_FAIL(iCode) do{ \` |
|      - | 2195 | `		int _p = 0; \` |
|      - | 2196 | `		char _c = ' '; \` |
|      - | 2197 | `		const char *_m = DtParseErr(zIn,nLen,(iCode),&_p,&_c); \` |
|      - | 2198 | `		DtRecErr(pRec,_p,_m); \` |
|      - | 2199 | `		if( iFirst == 0 ){ iFirst = (iCode); } \` |
|      - | 2200 | `		if( z < &zIn[_p + 1] ){ z = &zIn[_p + 1]; } \` |
|      - | 2201 | `		if( z > zEnd ){ z = zEnd; } \` |
|      - | 2202 | `	}while(0)` |
|   5219 | 2203 | `	DT_SKIP_WS();` |
|   5169 | 2204 | `	if( z >= zEnd ){` |
|      - | 2205 | `		/* php: the empty string is "now" */` |
|      3 | 2206 | `		return 0;` |
|      - | 2207 | `	}` |
|      - | 2208 | `	/*` |
|      - | 2209 | `	 * One rule set, tried at every position -- php's scanner has no head of its` |
|      - | 2210 | `	 * own and neither does this walk. The keyword rules come first and are` |
|      - | 2211 | `	 * spelled in LETTERS, so nothing they could claim reaches the date rules` |
|      - | 2212 | `	 * behind them by another route.` |
|      - | 2213 | `	 */` |
|   2865 | 2214 | `	for(;;){` |
|      - | 2215 | `		/* Every pass must CONSUME something. A shape rule that claims a token` |
|      - | 2216 | `		 * without advancing the cursor would spin here forever -- and one did:` |
|      - | 2217 | `		 * the ISO rule's refusal encodes the token's POSITION, and a token at` |
|      - | 2218 | `		 * position 0 encodes as the same 1 that means "matched", so` |
|      - | 2219 | ``		 * `new DateTime('2020-1-1 12:00')` hung the engine outright. The rule is`` |
|      - | 2220 | `		 * fixed above; this makes the whole class of it a refusal instead. */` |
|  12309 | 2221 | `		if( z == zPrev ){` |
|      9 | 2222 | `			DT_FAIL((int)(z - zIn) + 1);` |
|      9 | 2223 | `			if( z == zPrev ){` |
|      - | 2224 | `				/* Nothing could advance it -- stop rather than spin, which is the` |
|      - | 2225 | `				 * failure this guard exists for. */` |
|    ! 0 | 2226 | `				break;` |
|      - | 2227 | `			}` |
|      9 | 2228 | `			continue;` |
|      - | 2229 | `		}` |
|  12301 | 2230 | `		zPrev = z;` |
|  13367 | 2231 | `		DT_SKIP_WS();` |
|  12301 | 2232 | `		if( z >= zEnd ){` |
|   5167 | 2233 | `			break;` |
|      - | 2234 | `		}` |
|   7137 | 2235 | `		if( (iRc = DtTryEpoch(&z,zEnd,p,zIn)) != 0 ){` |
|    374 | 2236 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|    358 | 2237 | `			bAny = 1;` |
|    358 | 2238 | `			continue;` |
|      - | 2239 | `		}` |
|   6765 | 2240 | `		if( DT_LOWEQ("now",3) ){` |
|     39 | 2241 | `			z += 3;` |
|     39 | 2242 | `			bAny = 1;` |
|     39 | 2243 | `			continue;` |
|      - | 2244 | `		}` |
|   6727 | 2245 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|     21 | 2246 | `			DtUnhaveTime(p);` |
|     21 | 2247 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|     21 | 2248 | `			bAny = 1;` |
|     21 | 2249 | `			continue;` |
|      - | 2250 | `		}` |
|   6707 | 2251 | `		if( DT_LOWEQ("noon",4) ){` |
|     15 | 2252 | `			DtUnhaveTime(p);` |
|     15 | 2253 | `			p->h = 12;` |
|     15 | 2254 | `			p->nTimeTok = 1;` |
|     15 | 2255 | `			z += 4;` |
|     15 | 2256 | `			bAny = 1;` |
|     15 | 2257 | `			continue;` |
|      - | 2258 | `		}` |
|      - | 2259 | `		/* php SETS the relative day for these two rather than adding to it, so` |
|      - | 2260 | ``		 * either one wipes whatever days came before it: `+3 days tomorrow` is one`` |
|      - | 2261 | ``		 * day on, and `tomorrow yesterday` is yesterday. */`` |
|   6693 | 2262 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|     39 | 2263 | `			DtUnhaveTime(p);` |
|     39 | 2264 | `			p->bHaveRel = 1;` |
|     39 | 2265 | `			p->rd = 1;` |
|     39 | 2266 | `			z += 8;` |
|     39 | 2267 | `			bAny = 1;` |
|     39 | 2268 | `			continue;` |
|      - | 2269 | `		}` |
|   6655 | 2270 | `		if( DT_LOWEQ("yesterday",9) ){` |
|     31 | 2271 | `			DtUnhaveTime(p);` |
|     31 | 2272 | `			p->bHaveRel = 1;` |
|     31 | 2273 | `			p->rd = -1;` |
|     31 | 2274 | `			z += 9;` |
|     31 | 2275 | `			bAny = 1;` |
|     31 | 2276 | `			continue;` |
|      - | 2277 | `		}` |
|      - | 2278 | `		/* "first\|last day of": php's own standalone token. It records the flag and` |
|      - | 2279 | `		 * nothing else — whatever names the target month ("next month", "January` |
|      - | 2280 | `		 * 2021", or nothing at all) is an ordinary token after it, and the flag is` |
|      - | 2281 | `		 * applied LAST, which is what makes it swallow any relative days beside it. */` |
|   6625 | 2282 | `		if( DT_LOWEQ("first",5) \|\| DT_LOWEQ("last",4) ){` |
|    229 | 2283 | `			const char *zSave = z;` |
|    229 | 2284 | `			int bFirst = (SyToLower((unsigned char)z[0]) == 'f');` |
|    229 | 2285 | `			z += bFirst ? 5 : 4;` |
|    457 | 2286 | `			DT_SPACE();` |
|    229 | 2287 | `			if( DT_LOWEQ("day",3) ){` |
|    101 | 2288 | `				z += 3;` |
|    197 | 2289 | `				DT_SPACE();` |
|    101 | 2290 | `				if( DT_LOWEQ("of",2) ){` |
|     97 | 2291 | `					z += 2;` |
|     97 | 2292 | `					p->iFirstLast = bFirst ? 1 : 2;` |
|     97 | 2293 | `					p->bHaveRel = 1;` |
|     97 | 2294 | `					bAny = 1;` |
|     97 | 2295 | `					continue;` |
|      - | 2296 | `				}` |
|      2 | 2297 | `			}` |
|    133 | 2298 | `			z = zSave; /* not the "first\|last day of" shape: rewind */` |
|     66 | 2299 | `		}` |
|      - | 2300 | `		/* Weekday navigation: "[next\|last\|previous\|this] <weekday>" moves to the` |
|      - | 2301 | `		 * target weekday's midnight. php's behaviour code decides whether a base` |
|      - | 2302 | `		 * day that already matches counts: "next"/"last" skip it, a bare name or` |
|      - | 2303 | `		 * "this" keeps it. The move itself happens in DtAdjustWeekday, BEFORE the` |
|      - | 2304 | `		 * relative vector, which is php's order. */` |
|      - | 2305 | `		{` |
|   6529 | 2306 | `			const char *zSave = z;` |
|   6529 | 2307 | `			sxi64 iCnt = 0;` |
|   6529 | 2308 | `			int bNav = 0,bHavePrefix = 0,nWord;` |
|      - | 2309 | `			int adv,dow;` |
|   6529 | 2310 | `			if( (nWord = DtRelWord(z,zEnd,&iCnt,&bNav)) > 0 ){` |
|    407 | 2311 | `				z += nWord;` |
|    813 | 2312 | `				DT_SPACE();` |
|    407 | 2313 | `				bHavePrefix = 1;` |
|    203 | 2314 | `			}` |
|   6529 | 2315 | `			dow = DtMatchWeekdayEx(z,zEnd,&adv,bHavePrefix);` |
|   6529 | 2316 | `			if( dow >= 0 && DtRelUnitLen(z,zEnd,p) > adv ){` |
|     77 | 2317 | ``				dow = -1;   /* `next month` is the UNIT, not `mon` and a stray `th` */`` |
|     38 | 2318 | `			}` |
|   6529 | 2319 | `			if( dow >= 0 && bHavePrefix ){` |
|      - | 2320 | ``				/* php's `first monday of`: a WORD count, a weekday and the word`` |
|      - | 2321 | ``				 * `of` are one token, and what it names is a weekday inside a`` |
|      - | 2322 | ``				 * MONTH. The digit spelling does not reach it -- `1 monday of` is`` |
|      - | 2323 | `				 * a refusal there -- and neither does a bare name. */` |
|    169 | 2324 | `				const char *zOf = &z[adv];` |
|    261 | 2325 | `				while( zOf < zEnd && DtIsSpace((unsigned char)zOf[0]) ){ zOf++; }` |
|    168 | 2326 | `				if( zEnd - zOf >= 2 && SyStrnicmp(zOf,"of",2) == 0` |
|     86 | 2327 | `				 && (zEnd - zOf == 2 \|\| !SyisAlpha(zOf[2])) ){` |
|     79 | 2328 | `					DtUnhaveTime(p);` |
|     79 | 2329 | `					p->bWdayOf = 1;` |
|     79 | 2330 | `					p->bHaveRel = 1;` |
|     79 | 2331 | `					p->bWday = 1;` |
|     79 | 2332 | `					p->iWday = dow;` |
|     79 | 2333 | `					if( p->iWdayBehavior != 2 ){` |
|      - | 2334 | `						/* a day that already matches counts for every count php` |
|      - | 2335 | ``						 * spells forward (behaviour 1); `last` and `previous`,`` |
|      - | 2336 | `						 * which walk back a week from the month after, skip it */` |
|     79 | 2337 | `						p->iWdayBehavior = iCnt >= 0 ? 1 : 0;` |
|     39 | 2338 | `					}` |
|     79 | 2339 | `					if( iCnt >= 1 ){` |
|     55 | 2340 | `						p->rd = DtWAdd(p->rd,DtWMul(iCnt - 1,7));` |
|     28 | 2341 | `					}else{` |
|     25 | 2342 | `						p->iWdayOfNext = 1;` |
|     25 | 2343 | `						if( iCnt < 0 ){` |
|     19 | 2344 | `							p->rd = DtWAdd(p->rd,-7);` |
|      9 | 2345 | `						}` |
|      - | 2346 | `					}` |
|     79 | 2347 | `					z = &zOf[2];` |
|     79 | 2348 | `					bAny = 1;` |
|    165 | 2349 | `					continue;` |
|      - | 2350 | `				}` |
|     45 | 2351 | `			}` |
|   6451 | 2352 | `			if( dow >= 0 ){` |
|    173 | 2353 | `				DtUnhaveTime(p);` |
|    173 | 2354 | `				p->bHaveRel = 1;` |
|    173 | 2355 | `				p->bWday = 1;` |
|    173 | 2356 | `				p->iWday = dow;` |
|      - | 2357 | `				/* php: a COUNT word carries behaviour 0 and shifts a week per` |
|      - | 2358 | ``				 * count past the first (`last monday` is -7 days from the`` |
|      - | 2359 | ``				 * matching one, `second monday` +7); "this" is that rule's zero`` |
|      - | 2360 | `				 * and carries behaviour 1, as a bare name does. A bare name does` |
|      - | 2361 | ``				 * NOT overwrite the WEEK behaviour a `... week` word already set,`` |
|      - | 2362 | ``				 * which is what keeps `last week monday` in that week. */`` |
|    173 | 2363 | `				if( bHavePrefix ){` |
|     91 | 2364 | `					p->iWdayBehavior = (iCnt != 0) ? 0 : 1;` |
|     91 | 2365 | `					p->rd = DtWAdd(p->rd,DtWMul(iCnt > 0 ? iCnt - 1 : iCnt,7));` |
|    128 | 2366 | `				}else if( p->iWdayBehavior != 2 ){` |
|     83 | 2367 | `					p->iWdayBehavior = 1;` |
|     41 | 2368 | `				}` |
|    173 | 2369 | `				z += adv;` |
|    173 | 2370 | `				bAny = 1;` |
|    173 | 2371 | `				continue;` |
|      - | 2372 | `			}` |
|   6279 | 2373 | `			z = zSave; /* prefix did not introduce a weekday: rewind and try the rest */` |
|      - | 2374 | `		}` |
|      - | 2375 | `		/* Standalone "this\|next\|last (month\|week)". php's month is an ordinary` |
|      - | 2376 | `		 * relative month; its WEEK is a weekday-relative move to the Monday of the` |
|      - | 2377 | `		 * week (behaviour 2) plus the whole weeks, which is why "next week" is that` |
|      - | 2378 | `		 * Monday and not seven days from the base day. */` |
|      - | 2379 | `		{` |
|   6279 | 2380 | `			const char *zSave = z;` |
|   6279 | 2381 | `			sxi64 iCnt = 0;` |
|   6279 | 2382 | `			int bNav = 0,nWord;` |
|   6279 | 2383 | `			if( (nWord = DtRelWord(z,zEnd,&iCnt,&bNav)) > 0 ){` |
|    239 | 2384 | `				z += nWord;` |
|    477 | 2385 | `				DT_SPACE();` |
|    239 | 2386 | `				if( bNav && DT_WEEKSING() ){` |
|     81 | 2387 | `					z += 4;` |
|     81 | 2388 | `					p->bHaveRel = 1;` |
|     81 | 2389 | `					p->rd = DtWAdd(p->rd,DtWMul(iCnt,7));` |
|     81 | 2390 | `					if( !p->bWday ){        /* php: Monday, unless a weekday was` |
|      - | 2391 | `						* already named ("monday this week") */` |
|     55 | 2392 | `						p->bWday = 1;` |
|     55 | 2393 | `						p->iWday = 1;` |
|     27 | 2394 | `					}` |
|     81 | 2395 | `					p->iWdayBehavior = 2;` |
|     81 | 2396 | `					bAny = 1;` |
|    157 | 2397 | `					continue;` |
|      - | 2398 | `				}` |
|      - | 2399 | ``				/* The SINGULAR `week` is that special's own word and no other`` |
|      - | 2400 | ``				 * count reaches it: `first week` is a refusal in php where`` |
|      - | 2401 | ``				 * `first weeks` is seven ordinary days. */`` |
|    159 | 2402 | `				if( bNav \|\| !DT_WEEKSING() ){` |
|      - | 2403 | `					int nU,bSpec;` |
|    155 | 2404 | `					nU = DtRelUnit(z,zEnd,iCnt,p,&bSpec);` |
|    155 | 2405 | `					if( nU > 0 ){` |
|      - | 2406 | `						/* php's business-day count zeroes the clock when a WORD` |
|      - | 2407 | ``						 * asked for it (`next weekday` is midnight) and leaves it`` |
|      - | 2408 | ``						 * alone when a number did (`2 weekdays` keeps the hour). */`` |
|    153 | 2409 | `						if( bSpec ){ DtUnhaveTime(p); }` |
|    153 | 2410 | `						p->bHaveRel = 1;` |
|    153 | 2411 | `						z += nU;` |
|    153 | 2412 | `						bAny = 1;` |
|    153 | 2413 | `						continue;` |
|      - | 2414 | `					}` |
|      1 | 2415 | `				}` |
|      3 | 2416 | `			}` |
|   6047 | 2417 | `			z = zSave;` |
|      - | 2418 | `		}` |
|      - | 2419 | ``		/* A bare `weekday`, with no count in front of it, is not the business-day`` |
|      - | 2420 | `		 * move at all in php but the MONDAY hunt -- the same answer a bare weekday` |
|      - | 2421 | `		 * NAME gives. */` |
|   6047 | 2422 | `		if( DT_LOWEQ("weekdays",8) \|\| DT_LOWEQ("weekday",7) ){` |
|     13 | 2423 | `			DtUnhaveTime(p);` |
|     13 | 2424 | `			p->bHaveRel = 1;` |
|     13 | 2425 | `			p->bWday = 1;` |
|     13 | 2426 | `			p->iWday = 1;` |
|     13 | 2427 | `			if( p->iWdayBehavior != 2 ){ p->iWdayBehavior = 1; }` |
|     13 | 2428 | `			z += DT_LOWEQ("weekdays",8) ? 8 : 7;` |
|     13 | 2429 | `			bAny = 1;` |
|     13 | 2430 | `			continue;` |
|      - | 2431 | `		}` |
|      - | 2432 | ``		/* php's `ago` NEGATES the relative vector as it stands -- the weekday it`` |
|      - | 2433 | ``		 * hunts for included, which is what makes `next monday ago` the Monday`` |
|      - | 2434 | ``		 * before -- so a second `ago` puts it back. */`` |
|   6035 | 2435 | `		if( DT_LOWEQ("ago",3) ){` |
|     37 | 2436 | `			p->ry = -p->ry; p->rm = -p->rm; p->rd = -p->rd;` |
|     37 | 2437 | `			p->rh = -p->rh; p->ri = -p->ri; p->rs = -p->rs;   /* NOT the micro-` |
|      - | 2438 | ``				* seconds: php's `ago` leaves that one field standing */`` |
|     37 | 2439 | `			p->iWday = -p->iWday;` |
|     37 | 2440 | `			p->iWeekdays = -p->iWeekdays;` |
|     37 | 2441 | `			z += 3;` |
|     37 | 2442 | `			bAny = 1;` |
|     37 | 2443 | `			continue;` |
|      - | 2444 | `		}` |
|      - | 2445 | `		/* The absolute DATE tokens, php's own rule that any of them may stand` |
|      - | 2446 | `		 * anywhere in the string: "first day of january", "+1 day january",` |
|      - | 2447 | ``		 * "march 3" and the tail of `12345-01-01` (a compact time, then a date)`` |
|      - | 2448 | `		 * all reach here. The dates go first so that the longest reading wins --` |
|      - | 2449 | ``		 * `1.2.2020` is a date where a bare `1.2` is the time 01:02. */`` |
|   5999 | 2450 | `		if( SyisDigit(z[0]) && (iRc = DtTryIsoWeek(z,zEnd,&z,p,zIn)) != 0 ){` |
|     65 | 2451 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|     63 | 2452 | `			bAny = 1;` |
|     63 | 2453 | `			continue;` |
|      - | 2454 | `		}` |
|   5935 | 2455 | `		if( (iRc = DtTryIsoDate(z,zEnd,&z,p,zIn)) != 0 ){` |
|   2428 | 2456 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|   2410 | 2457 | `			bAny = 1;` |
|   2410 | 2458 | `			continue;` |
|      - | 2459 | `		}` |
|   3509 | 2460 | `		if( SyisDigit(z[0]) && (iRc = DtTryIsoOrdinalDot(z,zEnd,&z,p,zIn)) != 0 ){` |
|     29 | 2461 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|     29 | 2462 | `			bAny = 1;` |
|     29 | 2463 | `			continue;` |
|      - | 2464 | `		}` |
|   3481 | 2465 | `		if( SyisDigit(z[0]) && (iRc = DtTryNumericDate(z,zEnd,&z,p,zIn)) != 0 ){` |
|    197 | 2466 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|    189 | 2467 | `			bAny = 1;` |
|    189 | 2468 | `			continue;` |
|      - | 2469 | `		}` |
|      - | 2470 | `		/* ...and the same date with no YEAR, which is a shorter read than the` |
|      - | 2471 | `		 * three-field one above and so is tried after it. */` |
|   3285 | 2472 | `		if( SyisDigit(z[0]) && (iRc = DtTryAmericanShort(z,zEnd,&z,p,zIn)) != 0 ){` |
|     59 | 2473 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|     53 | 2474 | `			bAny = 1;` |
|     53 | 2475 | `			continue;` |
|      - | 2476 | `		}` |
|   3224 | 2477 | `		if( (SyisAlpha(z[0]) \|\| SyisDigit(z[0]))` |
|   2831 | 2478 | `		 && (iRc = DtTryMonthDate(z,zEnd,&z,p,zIn)) != 0 ){` |
|    285 | 2479 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|    261 | 2480 | `			bAny = 1;` |
|    261 | 2481 | `			continue;` |
|      - | 2482 | `		}` |
|      - | 2483 | `		/* A time of day ("next thursday 15:00", or one standing alone). */` |
|   2943 | 2484 | `		if( (iRc = DtReadTimeOfDay(&z,zEnd,zIn,p)) != 0 ){` |
|    379 | 2485 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|    357 | 2486 | `			bAny = 1;` |
|    357 | 2487 | `			continue;` |
|      - | 2488 | `		}` |
|   2565 | 2489 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|   1729 | 2490 | `			int neg = 0,bNoUnit = 0,nDig = 0;` |
|   1729 | 2491 | `			sxi64 v = 0;` |
|   1729 | 2492 | `			const char *zNumStart = z;` |
|      - | 2493 | `			const char *zDig;` |
|   1729 | 2494 | `			if( z[0]=='+' \|\| z[0]=='-' ){` |
|    691 | 2495 | `				neg = (z[0]=='-');` |
|    691 | 2496 | `				z++;` |
|      - | 2497 | `				/* php's lexer takes the sign as its own token, so whitespace may` |
|      - | 2498 | `				 * follow it: "1 year + 3 months" is a relative sequence there and` |
|      - | 2499 | `` 				 * was a parse FAILURE here. Its `space` alone, though: `+,3 days` `` |
|      - | 2500 | ``				 * and `+.3 days` are refusals there. */`` |
|    699 | 2501 | `				DT_SPACE();` |
|    344 | 2502 | `			}` |
|   1729 | 2503 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){` |
|     39 | 2504 | `				z = zNumStart;` |
|     39 | 2505 | `				DT_FAIL((int)(zNumStart - zIn) + 1);` |
|     39 | 2506 | `				continue;` |
|      - | 2507 | `			}` |
|   1691 | 2508 | `			zDig = z;` |
|   6727 | 2509 | `			while( z < zEnd && SyisDigit(z[0]) ){` |
|   5039 | 2510 | `				if( nDig < DT_DIGITS_SAFE ){ v = v*10 + (z[0]-'0'); }` |
|   5039 | 2511 | `				nDig++;` |
|   5039 | 2512 | `				z++;` |
|      3 | 2513 | `			}` |
|   1691 | 2514 | `			if( neg ){ v = -v; }` |
|   2441 | 2515 | `			DT_SPACE();` |
|      - | 2516 | `			/* php's unit words, sub-second ones first: they are all one rule (see` |
|      - | 2517 | `			 * DtRelUnit), and the microseconds accumulate apart from the seconds` |
|      - | 2518 | ``			 * so `-500 microseconds` from midnight borrows a whole second. */`` |
|      - | 2519 | `			{` |
|      - | 2520 | `				int nU,bSpec;` |
|   1691 | 2521 | `				nU = DtRelUnit(z,zEnd,v,p,&bSpec);` |
|   1691 | 2522 | `				if( nU > 0 ){ p->bHaveRel = 1; z += nU; }` |
|   1089 | 2523 | `				else{ bNoUnit = 1; }` |
|      - | 2524 | `			}` |
|   1691 | 2525 | `			if( bNoUnit ){` |
|      - | 2526 | ``				/* php's COUNTED weekday, `2 monday`: the count is whole WEEKS`` |
|      - | 2527 | `				 * past the first, the hunt is the bare name's (behaviour 1, so a` |
|      - | 2528 | `				 * base day that already matches counts), and -- unlike every` |
|      - | 2529 | `				 * spelling that reaches one through a WORD -- the clock is left` |
|      - | 2530 | `				 * standing. */` |
|   1089 | 2531 | `				int adv,dow = DtMatchWeekdayEx(z,zEnd,&adv,1);` |
|   1089 | 2532 | `				if( dow >= 0 && DtRelUnitLen(z,zEnd,p) > adv ){` |
|    ! 0 | 2533 | ``					dow = -1;   /* `3 months` is the unit, not `mon` and `ths` */`` |
|    ! 0 | 2534 | `				}` |
|   1089 | 2535 | `				if( dow >= 0 ){` |
|     59 | 2536 | `					p->bHaveRel = 1;` |
|     59 | 2537 | `					p->bWday = 1;` |
|     59 | 2538 | `					p->iWday = dow;` |
|     59 | 2539 | `					if( p->iWdayBehavior != 2 ){ p->iWdayBehavior = 1; }` |
|     59 | 2540 | `					p->rd = DtWAdd(p->rd,DtWMul(v > 0 ? v - 1 : v,7));` |
|     59 | 2541 | `					z += adv;` |
|     59 | 2542 | `					bNoUnit = 0;` |
|     29 | 2543 | `				}` |
|    544 | 2544 | `			}` |
|   1691 | 2545 | `			if( !bNoUnit ){` |
|      - | 2546 | `				/* php's own ceiling on a relative number, checked once the UNIT` |
|      - | 2547 | `				 * has claimed the run (the nocolon rules below have their own` |
|      - | 2548 | ``				 * widths, and a bare `20240102123456` is a date and a time). */`` |
|    661 | 2549 | `				if( nDig > DT_DIGITS_REL ){` |
|     19 | 2550 | `					DT_FAIL(-((int)(zDig - zIn) + 1) - DT_ERR_RANGE);` |
|     19 | 2551 | `					continue;` |
|      - | 2552 | `				}` |
|    643 | 2553 | `				bAny = 1;` |
|    643 | 2554 | `				continue;` |
|      - | 2555 | `			}` |
|      - | 2556 | `			/* No unit word: this is not a relative token at all. php's scanner` |
|      - | 2557 | `			 * would have taken a LONGER match, so rewind and let the nocolon` |
|      - | 2558 | `			 * rules below read the same digits as a date, a clock or a year. */` |
|   1031 | 2559 | `			z = zNumStart;` |
|    515 | 2560 | `		}` |
|      - | 2561 | `		/* php's NOCOLON spellings, a bare run of digits read by WIDTH: eight are a` |
|      - | 2562 | ``		 * date (`20240102`), six a time (`123456`), four a time (`1234`) -- or, if`` |
|      - | 2563 | `		 * the string already named a time, a YEAR, which is php's own dispatch and` |
|      - | 2564 | ``		 * what makes `12:00 1234` the year 1234 and `1234 12:00` a refusal. A run`` |
|      - | 2565 | ``		 * of four that is no valid clock is a year outright (`2500`), and a third`` |
|      - | 2566 | `		 * such run is php's "Double time specification".` |
|      - | 2567 | `		 *` |
|      - | 2568 | `		 * Only the longest reading counts, so an over-wide run leaves its tail to` |
|      - | 2569 | ``		 * the loop: `12345-01-01` is 12:34 on 2005-01-01, and `1234567` is a parse`` |
|      - | 2570 | `		 * failure at its last digit. */` |
|   1867 | 2571 | `		if( SyisDigit(z[0]) \|\| ((z[0]=='t' \|\| z[0]=='T') && z+1 < zEnd && SyisDigit(z[1])) ){` |
|    789 | 2572 | `			int bT = !SyisDigit(z[0]);` |
|    789 | 2573 | `			const char *zd = &z[bT];` |
|    789 | 2574 | `			int n = 0,bNoRun = 0,h,mi,se,mo,d,doy;` |
|   3539 | 2575 | `			while( zd+n < zEnd && SyisDigit(zd[n]) ){ n++; }` |
|      - | 2576 | `#define DTNUM2(k) ((zd[k]-'0')*10 + (zd[(k)+1]-'0'))` |
|    789 | 2577 | `			mo = (n >= 8) ? DTNUM2(4) : 99;` |
|    789 | 2578 | `			d  = (n >= 8) ? DTNUM2(6) : 99;` |
|    789 | 2579 | `			h  = (n >= 4) ? DTNUM2(0) : 99;` |
|    789 | 2580 | `			mi = (n >= 4) ? DTNUM2(2) : 99;` |
|    789 | 2581 | `			se = (n >= 6) ? DTNUM2(4) : 99;` |
|    789 | 2582 | `			doy = (n >= 7) ? (zd[4]-'0')*100 + DTNUM2(5) : 0;` |
|    789 | 2583 | `			if( !bT && n == 4 ){` |
|      - | 2584 | ``				/* php's REVERSE no-day date, `2020 Jan`: a four-digit year and a`` |
|      - | 2585 | `				 * month name are one token there, and a longer one than the clock` |
|      - | 2586 | `` 				 * reading of those same four digits -- which is why `2020 Jan 15` `` |
|      - | 2587 | `				 * is a parse failure and not a time. */` |
|    145 | 2588 | `				const char *zm = &zd[4];` |
|      - | 2589 | `				int adv,mo2;` |
|    283 | 2590 | `				while( zm < zEnd && (zm[0]==' '\|\|zm[0]=='\t'\|\|zm[0]=='.'\|\|zm[0]=='-') ){ zm++; }` |
|    145 | 2591 | `				if( (mo2 = DtMatchMonth(zm,zEnd,&adv)) != 0 ){` |
|      9 | 2592 | `					int rcD = DtMarkDate(p,z,zIn);` |
|     13 | 2593 | `					if( rcD != 0 ){ z = &zm[adv]; DT_FAIL(rcD); continue; }` |
|      9 | 2594 | `					p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));` |
|      9 | 2595 | `					p->m = mo2;` |
|      9 | 2596 | `					p->d = 1;` |
|      9 | 2597 | `					z = &zm[adv];` |
|      9 | 2598 | `					bAny = 1;` |
|      9 | 2599 | `					continue;` |
|      - | 2600 | `				}` |
|     68 | 2601 | `			}` |
|    781 | 2602 | `			if( !bT && n >= 8 && mo <= 12 && d <= 31 ){` |
|     35 | 2603 | `				int rcD = DtMarkDate(p,z,zIn);` |
|     35 | 2604 | `				if( rcD != 0 ){ z = &zd[8]; DT_FAIL(rcD); continue; }` |
|     27 | 2605 | `				p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));` |
|     27 | 2606 | `				p->m = mo;` |
|     27 | 2607 | `				p->d = d;` |
|     27 | 2608 | `				z = &zd[8];` |
|    760 | 2609 | `			}else if( !bT && n >= 7 && doy >= 1 && doy <= 366 ){` |
|      - | 2610 | `				/* php's ISO ORDINAL date, YYYYDDD: the day of the year, which the` |
|      - | 2611 | `				 * field normalizer resolves out of January. */` |
|      7 | 2612 | `				int rcD = DtMarkDate(p,z,zIn);` |
|      7 | 2613 | `				if( rcD != 0 ){ z = &zd[7]; DT_FAIL(rcD); continue; }` |
|      7 | 2614 | `				p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));` |
|      7 | 2615 | `				p->m = 1;` |
|      7 | 2616 | `				p->d = doy;` |
|      7 | 2617 | `				z = &zd[7];` |
|    744 | 2618 | `			}else if( n >= 6 && h <= 24 && mi <= 59 && se <= 60 ){` |
|      - | 2619 | `				/* six digits are php's whole clock, and a SECOND clock is its` |
|      - | 2620 | `				 * refusal rather than the year the four-digit run falls back to */` |
|     19 | 2621 | `				if( p->nTimeTok ){` |
|    ! 0 | 2622 | `					iRc = -((int)(z - zIn) + 1);` |
|    ! 0 | 2623 | `					z = &zd[6];` |
|    ! 0 | 2624 | `					DT_FAIL(iRc);` |
|    ! 0 | 2625 | `					continue;` |
|      - | 2626 | `				}` |
|     19 | 2627 | `				p->h = h; p->i = mi; p->s = se; p->us = 0;` |
|     19 | 2628 | `				p->bUsUnset = 0;` |
|     19 | 2629 | `				p->nTimeTok = 1;` |
|     19 | 2630 | `				z = &zd[6];` |
|    732 | 2631 | `			}else if( n >= 4 && h <= 24 && mi <= 59 ){` |
|    113 | 2632 | `				if( p->nTimeTok >= 2 ){` |
|      3 | 2633 | `					iRc = -((int)(z - zIn) + 1);` |
|      3 | 2634 | `					z = &zd[4];` |
|      3 | 2635 | `					DT_FAIL(iRc);` |
|      3 | 2636 | `					continue;` |
|      - | 2637 | `				}` |
|    111 | 2638 | `				if( p->nTimeTok == 1 ){` |
|     11 | 2639 | `					p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));` |
|      6 | 2640 | `				}else{` |
|      - | 2641 | ``					/* php's `gnunocolon` writes the fields itself rather than`` |
|      - | 2642 | `					 * through TIMELIB_HAVE_TIME, so the sub-second one is never` |
|      - | 2643 | ``					 * touched: `date_parse('1234')` shows no fraction at all,`` |
|      - | 2644 | ``					 * while `12:00 1234` keeps the zero its clock wrote. */`` |
|    101 | 2645 | `					p->bUsUnset = (p->us == DT_UNSET);` |
|    101 | 2646 | `					p->h = h; p->i = mi; p->s = 0; p->us = 0;` |
|      - | 2647 | `				}` |
|    111 | 2648 | `				p->nTimeTok++;` |
|    111 | 2649 | `				z = &zd[4];` |
|    666 | 2650 | `			}else if( !bT && n >= 4 ){` |
|      - | 2651 | `				/* php's bare year4, which does NOT count as a date: the month, the` |
|      - | 2652 | `				 * day and the clock all stay the base moment's -- the MICROSECONDS` |
|      - | 2653 | `				 * excepted. The run reached this branch through php's have_time` |
|      - | 2654 | `				 * bookkeeping, which zeroes the sub-second field on the way past,` |
|      - | 2655 | ``				 * so `new DateTime('7609')` is the current time of day on that`` |
|      - | 2656 | `				 * year with nothing under the second. */` |
|    113 | 2657 | `				p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));` |
|    113 | 2658 | ``				p->bUsUnset = (p->us == DT_UNSET);   /* php's `year4` writes the`` |
|      - | 2659 | `					* year and nothing else */` |
|    113 | 2660 | `				p->us = 0;` |
|    113 | 2661 | `				z = &zd[4];` |
|    555 | 2662 | `			}else if( bT ){` |
|      - | 2663 | ``				/* php's `t` + an HOUR alone: `t9` is 09:00 where a bare `9` is`` |
|      - | 2664 | `				 * nothing at all, and the hour is read the same greedy way as the` |
|      - | 2665 | ``				 * one before a colon (`t95846` is 09:00 and the year 5846). */`` |
|     23 | 2666 | `				int nh = DtReadField(zd,zEnd,24,&h);` |
|     23 | 2667 | `				if( p->nTimeTok ){` |
|      3 | 2668 | `					iRc = -((int)(z - zIn) + 1);` |
|      3 | 2669 | `					z = &zd[nh];` |
|      3 | 2670 | `					DT_FAIL(iRc);` |
|      3 | 2671 | `					continue;` |
|      - | 2672 | `				}` |
|     21 | 2673 | `				p->h = h;` |
|     21 | 2674 | `				p->i = p->s = p->us = 0;` |
|     21 | 2675 | `				p->bUsUnset = 0;` |
|     21 | 2676 | `				p->nTimeTok = 1;` |
|     21 | 2677 | `				z = &zd[nh];` |
|     11 | 2678 | `			}else{` |
|    477 | 2679 | `				bNoRun = 1;   /* no nocolon rule claims it; z is untouched */` |
|      - | 2680 | `			}` |
|    769 | 2681 | `			if( !bNoRun ){` |
|    293 | 2682 | `				bAny = 1;` |
|    293 | 2683 | `				continue;` |
|      - | 2684 | `			}` |
|      - | 2685 | `#undef DTNUM2` |
|    238 | 2686 | `		}` |
|      - | 2687 | `		/* php's TIMEZONE token stands anywhere in a string and is a token in its` |
|      - | 2688 | ``		 * own right: `2020-01-01 12:00 +0200` (its own serialization spelling, and`` |
|      - | 2689 | ``		 * every RFC-2822 date there is), `12:00 UTC`, `1234z`, and `UTC` alone --`` |
|      - | 2690 | `		 * none of which parsed here at all. It goes LAST because php's scanner` |
|      - | 2691 | ``		 * takes the longest reading: `+1 day` is a relative and `t9` a clock, and`` |
|      - | 2692 | `		 * both would otherwise be read as a zone. */` |
|   1555 | 2693 | `		if( (iRc = DtZoneTok(&z,zEnd,p,zIn)) != 0 ){` |
|    989 | 2694 | `			if( iRc != 1 ){ DT_FAIL(iRc); continue; }` |
|    651 | 2695 | `			bAny = 1;` |
|    651 | 2696 | `			continue;` |
|      - | 2697 | `		}` |
|    567 | 2698 | `		DT_FAIL((int)(z - zIn) + 1);` |
|      1 | 2699 | `	}` |
|      - | 2700 | `	/* The warnings a RULE raised, in the order the scan met them ... */` |
|   5307 | 2701 | `	for( k = 0 ; k < p->nWarnPend && k < PH7_DT_MAX_WARN ; k++ ){` |
|    141 | 2702 | `		DtRecWarn(pRec,p->aWarnPos[k],p->azWarn[k]);` |
|     71 | 2703 | `	}` |
|      - | 2704 | `	/*` |
|      - | 2705 | `	 * ...and the two php raises once the scan is over, both at the byte one past` |
|      - | 2706 | `	 * the string. They are about what the string SPELLED rather than what any` |
|      - | 2707 | ``	 * rule refused, so a date nobody could hold (`2020-02-31`, `2020-102`) and a`` |
|      - | 2708 | ``	 * clock nobody could show (`24:00:00`, `23:59:60`) are warnings on a parse`` |
|      - | 2709 | `	 * that otherwise succeeds. php asks the time first, and both land on the same` |
|      - | 2710 | `	 * key -- so a string with both counts two and shows the date's.` |
|      - | 2711 | `	 */` |
|   5164 | 2712 | `	if( p->nTimeTok` |
|   3859 | 2713 | `	 && (p->h > 23 \|\| p->i > 59 \|\| p->s > 59 \|\| p->h < 0 \|\| p->i < 0 \|\| p->s < 0) ){` |
|     23 | 2714 | `		DtRecWarn(pRec,nLen + 1,"The parsed time was invalid");` |
|     11 | 2715 | `	}` |
|   5164 | 2716 | `	if( p->bHaveDate` |
|   4103 | 2717 | `	 && (p->m < 1 \|\| p->m > 12 \|\| p->d < 1` |
|   2974 | 2718 | `	     \|\| p->d > DtDaysInMonth(p->y,(int)p->m)) ){` |
|    167 | 2719 | `		DtRecWarn(pRec,nLen + 1,"The parsed date was invalid");` |
|     83 | 2720 | `	}` |
|   5167 | 2721 | `	if( iFirst != 0 ){` |
|    754 | 2722 | `		return iFirst;` |
|      - | 2723 | `	}` |
|   4415 | 2724 | `	if( !bAny ){` |
|    ! 0 | 2725 | `		return 1;` |
|      - | 2726 | `	}` |
|   4415 | 2727 | `	return 0;` |
|      - | 2728 | `#undef DT_SKIP_WS` |
|      - | 2729 | `#undef DT_LOWEQ` |
|      - | 2730 | `#undef DT_WEEKSING` |
|      - | 2731 | `#undef DT_FAIL` |
|   2587 | 2732 | `}` |
|      - | 2733 | `/*` |
|      - | 2734 | ` * Parse zIn against the base moment and answer the timestamp it names. The` |
|      - | 2735 | ` * vector the string filled is applied here (DtApplyFields), so nothing about the` |
|      - | 2736 | ` * order the string spelled its units in reaches the clock.` |
|      - | 2737 | ` */` |
|   5168 | 2738 | `static int DtParseEx(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,int iBaseUs,` |
|      - | 2739 | `	int iFlags,sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs,dt_parsed *pVec,` |
|      - | 2740 | `	phl_dt_lasterr *pRec)` |
|      3 | 2741 | `{` |
|      - | 2742 | `	dt_parsed sP;` |
|      - | 2743 | `	int iErr;` |
|   5171 | 2744 | `	DtFieldsInit(&sP,iBaseOff);` |
|   5171 | 2745 | `	*pUs = iBaseUs;` |
|   5171 | 2746 | `	if( pRec ){` |
|   4285 | 2747 | `		DtRecReset(pRec);` |
|   2141 | 2748 | `	}` |
|   5171 | 2749 | `	iErr = DtParseFields(zIn,nLen,&sP,pRec);` |
|   5171 | 2750 | `	if( pVec ){` |
|   4471 | 2751 | `		*pVec = sP;` |
|   2234 | 2752 | `	}` |
|   5171 | 2753 | `	if( iErr != 0 ){` |
|    756 | 2754 | `		return iErr;` |
|      - | 2755 | `	}` |
|   4417 | 2756 | `	*pTs = DtApplyFields(&sP,iBaseTs,iBaseOff,iBaseUs,iFlags,pUs);` |
|   4417 | 2757 | `	*pOff = sP.iOff;` |
|   4417 | 2758 | `	*pbOffSet = sP.bOffSet;` |
|   4417 | 2759 | `	return 0;` |
|   2587 | 2760 | `}` |
|    700 | 2761 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,int iBaseUs,` |
|      - | 2762 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs)` |
|      2 | 2763 | `{` |
|    702 | 2764 | `	return DtParseEx(zIn,nLen,iBaseTs,iBaseOff,iBaseUs,0,pTs,pOff,pbOffSet,pUs,0,0);` |
|      2 | 2765 | `}` |
|      - | 2766 | `/*` |
|      - | 2767 | ` * php's parse-failure reason, from DtParse's error code.` |
|      - | 2768 | ` *` |
|      - | 2769 | ` * The reason and the offending byte used to be formatted straight into the message` |
|      - | 2770 | `` * the `__dt_parse` thunk RETURNED as a string; the constructor also has to publish`` |
|      - | 2771 | ` * them as getLastErrors()'s error map now, so the decision lives here.` |
|      - | 2772 | ` */` |
|   1700 | 2773 | `static const char * DtParseErr(const char *zIn,int nLen,int iErrPos,int *piPos,char *pcAt)` |
|      2 | 2774 | `{` |
|      - | 2775 | `	/* Negative encodings: php's "Double time specification" reason, and -- one` |
|      - | 2776 | `	 * whole DT_ERR_RANGE band lower -- its "Number out of range", which is what a` |
|      - | 2777 | `	 * digit run too wide for the clock reports; then "Double date specification"` |
|      - | 2778 | `	 * and, lowest, "Double timezone specification". */` |
|   1702 | 2779 | `	int bRange = 0,bDDate = 0,bDZone = 0,bTzId = 0,bUnexp = 0,bEmpty = 0;` |
|      - | 2780 | `	int bDouble;` |
|      - | 2781 | `	int iPos;` |
|   1702 | 2782 | `	DtTrimEnds(&zIn,&nLen);   /* the position is php's, i.e. the trimmed string's */` |
|   1702 | 2783 | `	if( iErrPos < -DT_ERR_EMPTY ){` |
|    ! 0 | 2784 | `		bEmpty = 1;` |
|    ! 0 | 2785 | `		iErrPos += DT_ERR_EMPTY;` |
|   1702 | 2786 | `	}else if( iErrPos < -DT_ERR_UNEXPDATA ){` |
|     17 | 2787 | `		bUnexp = 1;` |
|     17 | 2788 | `		iErrPos += DT_ERR_UNEXPDATA;` |
|   1694 | 2789 | `	}else if( iErrPos < -DT_ERR_TZID ){` |
|    513 | 2790 | `		bTzId = 1;` |
|    513 | 2791 | `		iErrPos += DT_ERR_TZID;` |
|   1430 | 2792 | `	}else if( iErrPos < -DT_ERR_DZONE ){` |
|     91 | 2793 | `		bDZone = 1;` |
|     91 | 2794 | `		iErrPos += DT_ERR_DZONE;` |
|   1129 | 2795 | `	}else if( iErrPos < -DT_ERR_DDATE ){` |
|     91 | 2796 | `		bDDate = 1;` |
|     91 | 2797 | `		iErrPos += DT_ERR_DDATE;` |
|   1039 | 2798 | `	}else if( iErrPos < -DT_ERR_RANGE ){` |
|     68 | 2799 | `		bRange = 1;` |
|     68 | 2800 | `		iErrPos += DT_ERR_RANGE;` |
|     33 | 2801 | `	}` |
|   2132 | 2802 | `	bDouble = !bRange && !bDDate && !bDZone && !bTzId && !bUnexp && !bEmpty` |
|   2517 | 2803 | `		&& iErrPos < 0;` |
|   1702 | 2804 | `	iPos = (iErrPos < 0 ? -iErrPos : iErrPos) - 1;` |
|   1702 | 2805 | `	char cAt = (iPos < nLen) ? zIn[iPos] : ' ';` |
|   1702 | 2806 | `	*piPos = iPos;` |
|   1702 | 2807 | `	*pcAt = cAt;` |
|      - | 2808 | `	/* php appends a reason: an alphabetic token is assumed to be a timezone` |
|      - | 2809 | `	 * lookup miss, anything else an unexpected character. */` |
|   1702 | 2810 | `	if( bRange ){` |
|     68 | 2811 | `		return "Number out of range";` |
|      - | 2812 | `	}` |
|   1635 | 2813 | `	if( bDDate ){` |
|     91 | 2814 | `		return "Double date specification";` |
|      - | 2815 | `	}` |
|   1545 | 2816 | `	if( bDZone ){` |
|     91 | 2817 | `		return "Double timezone specification";` |
|      - | 2818 | `	}` |
|   1455 | 2819 | `	if( bTzId ){` |
|    513 | 2820 | `		return "The timezone could not be found in the database";` |
|      - | 2821 | `	}` |
|    943 | 2822 | `	if( bUnexp ){` |
|     17 | 2823 | `		return "Found unexpected data";` |
|      - | 2824 | `	}` |
|    927 | 2825 | `	if( bEmpty ){` |
|    ! 0 | 2826 | `		return "Empty string";` |
|      - | 2827 | `	}` |
|      - | 2828 | ``	/* php's own parenthesized-zone token starts at the `(`, so a name it cannot`` |
|      - | 2829 | `	 * find there is reported at the paren with the zone reason, not the byte. */` |
|    927 | 2830 | `	if( cAt == '(' && iPos + 1 < nLen && SyisAlpha(zIn[iPos+1]) ){` |
|    ! 0 | 2831 | `		return "The timezone could not be found in the database";` |
|      - | 2832 | `	}` |
|    908 | 2833 | `	return bDouble ? "Double time specification"` |
|    907 | 2834 | `		: ((cAt >= 'a' && cAt <= 'z') \|\| (cAt >= 'A' && cAt <= 'Z'))` |
|      - | 2835 | `			? "The timezone could not be found in the database"` |
|      - | 2836 | `			: "Unexpected character";` |
|    852 | 2837 | `}` |
|      - | 2838 | `/* Days in a civil month (php's overflow rules use it during diff borrows) */` |
|  12518 | 2839 | `static int DtDaysInMonth(sxi64 y,int m)` |
|      2 | 2840 | `{` |
|      - | 2841 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|  12520 | 2842 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|    295 | 2843 | `		return 29;` |
|      - | 2844 | `	}` |
|  12226 | 2845 | `	return aMonDays[(m - 1) % 12];` |
|   6258 | 2846 | `}` |
|      - | 2847 | `/*` |
|      - | 2848 | ` * php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|      - | 2849 | ` * (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset.` |
|      - | 2850 | ` */` |
|    346 | 2851 | `static sxi64 DtCivilAdd(sxi64 iTs,sxi32 iOff,sxi64 y,sxi64 m,sxi64 d,` |
|      - | 2852 | `	sxi64 h,sxi64 i,sxi64 s,int iSign)` |
|      1 | 2853 | `{` |
|      - | 2854 | `	sxi64 iLocal,iDays,iSecs,y0,moT,dayCount;` |
|      - | 2855 | `	int mo0,d0;` |
|    347 | 2856 | `	iSign = iSign < 0 ? -1 : 1;` |
|    347 | 2857 | `	iLocal = iTs + iOff;` |
|    347 | 2858 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    347 | 2859 | `	iSecs  = iLocal - iDays*86400;` |
|    347 | 2860 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    347 | 2861 | `	y0 += iSign * y;` |
|    347 | 2862 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    347 | 2863 | `	y0 += DtFloorDiv(moT,12);` |
|    347 | 2864 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    347 | 2865 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    347 | 2866 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    347 | 2867 | `	return iLocal - iOff;` |
|      1 | 2868 | `}` |
|      - | 2869 | `/* One DateInterval's worth of fields, as diff() computes them. */` |
|      - | 2870 | `typedef struct dt_diff dt_diff;` |
|      - | 2871 | `struct dt_diff` |
|      - | 2872 | `{` |
|      - | 2873 | `	sxi64 y,m,d,h,i,s,uSec,nDays;` |
|      - | 2874 | `	int bInvert;` |
|      - | 2875 | `};` |
|      - | 2876 | `/*` |
|      - | 2877 | ` * timelib's diff breakdown: field-wise deltas in the FIRST operand's offset, then` |
|      - | 2878 | ` * borrow seconds->minutes->hours->days, then borrow whole months for the day.` |
|      - | 2879 | ` *` |
|      - | 2880 | ` * That last borrow is ASYMMETRIC in php, and PHL answered the symmetric result: a` |
|      - | 2881 | ` * non-inverted diff walks months BACKWARD from the later date (which is why` |
|      - | 2882 | ` * Jan 31 -> Mar 02 reports m=0 d=30, not "1 month"), while an inverted one borrows` |
|      - | 2883 | ` * the month of the ORIGINAL first operand — the later date — walking forward. So` |
|      - | 2884 | `` * `$later->diff($earlier)` is not `$earlier->diff($later)` with the sign flipped:`` |
|      - | 2885 | ` * php answers y=1 m=1 d=2 where PHL answered y=1 m=0 d=30. One iteration always` |
|      - | 2886 | ` * settles the inverted case: \|d\| < 31 and the borrowed month has at least 28 days,` |
|      - | 2887 | ` * while a 28-day base month can only be reached from a day-of-month <= 29.` |
|      - | 2888 | ` */` |
|     66 | 2889 | `static void DtCivilDiff(sxi64 iTs1,int uSec1,sxi32 iOff,sxi64 iTs2,int uSec2,dt_diff *pOut)` |
|      1 | 2890 | `{` |
|      - | 2891 | `	sxi64 iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|      - | 2892 | `	int moA,dA,moB,dB,bInvert,usA,usB;` |
|      - | 2893 | `	sxi64 sA,sB,y,m,d,h,i,s,us;` |
|      - | 2894 | ``	/* The MICROSECONDS are part of which date comes first -- `$a->diff($b)` on two`` |
|      - | 2895 | `	 * dates inside the same second is an INVERTED interval when $a is the later of` |
|      - | 2896 | `	 * them -- and their borrow is a whole second off the later date, so every field` |
|      - | 2897 | `	 * below and the day COUNT are computed from the borrowed instant: a difference` |
|      - | 2898 | `	 * of one microsecond less than a day is 23:59:59.999999 with days = 0, not a` |
|      - | 2899 | `	 * day. */` |
|     67 | 2900 | `	bInvert = iTs1 > iTs2 \|\| (iTs1 == iTs2 && uSec1 > uSec2);` |
|     67 | 2901 | `	iA = bInvert ? iTs2 : iTs1;` |
|     67 | 2902 | `	iB = bInvert ? iTs1 : iTs2;` |
|     67 | 2903 | `	usA = bInvert ? uSec2 : uSec1;` |
|     67 | 2904 | `	usB = bInvert ? uSec1 : uSec2;` |
|     67 | 2905 | `	us = usB - usA;` |
|     67 | 2906 | `	if( us < 0 ){` |
|     21 | 2907 | `		us += 1000000;` |
|     21 | 2908 | `		iB--;` |
|     10 | 2909 | `	}` |
|     67 | 2910 | `	iLa = iA + iOff;` |
|     67 | 2911 | `	iLb = iB + iOff;` |
|     67 | 2912 | `	daysA = DtFloorDiv(iLa,86400);` |
|     67 | 2913 | `	daysB = DtFloorDiv(iLb,86400);` |
|     67 | 2914 | `	sA = iLa - daysA*86400;` |
|     67 | 2915 | `	sB = iLb - daysB*86400;` |
|     67 | 2916 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|     67 | 2917 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|     67 | 2918 | `	s = (sB % 60) - (sA % 60);` |
|     67 | 2919 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|     67 | 2920 | `	h = (sB / 3600) - (sA / 3600);` |
|     67 | 2921 | `	d = dB - dA;` |
|     67 | 2922 | `	m = moB - moA;` |
|     67 | 2923 | `	y = yB - yA;` |
|     67 | 2924 | `	if( s < 0 ){ s += 60; i--; }` |
|     67 | 2925 | `	if( i < 0 ){ i += 60; h--; }` |
|     67 | 2926 | `	if( h < 0 ){ h += 24; d--; }` |
|     67 | 2927 | `	if( bInvert ){` |
|     37 | 2928 | `		while( d < 0 ){` |
|     11 | 2929 | `			d += DtDaysInMonth(yA,moA);` |
|     11 | 2930 | `			m--;` |
|     11 | 2931 | `			moA++;` |
|     11 | 2932 | `			if( moA > 12 ){ moA = 1; yA++; }` |
|      1 | 2933 | `		}` |
|     14 | 2934 | `	}else{` |
|     55 | 2935 | `		while( d < 0 ){` |
|     15 | 2936 | `			moB--;` |
|     15 | 2937 | `			if( moB < 1 ){ moB = 12; yB--; }` |
|     15 | 2938 | `			d += DtDaysInMonth(yB,moB);` |
|     15 | 2939 | `			m--;` |
|      1 | 2940 | `		}` |
|      - | 2941 | `	}` |
|     67 | 2942 | `	if( m < 0 ){ m += 12; y--; }` |
|     67 | 2943 | `	pOut->y = y;` |
|     67 | 2944 | `	pOut->m = m;` |
|     67 | 2945 | `	pOut->d = d;` |
|     67 | 2946 | `	pOut->h = h;` |
|     67 | 2947 | `	pOut->i = i;` |
|     67 | 2948 | `	pOut->s = s;` |
|     67 | 2949 | `	pOut->uSec = us;` |
|     67 | 2950 | `	pOut->nDays = (iB - iA) / 86400;` |
|     67 | 2951 | `	pOut->bInvert = bInvert;` |
|     67 | 2952 | `}` |
|      - | 2953 | `/*` |
|      - | 2954 | ` * setISODate: jump to an ISO year/week/weekday, preserving the time of day.` |
|      - | 2955 | ` */` |
|      8 | 2956 | `static sxi64 DtIsoDate(sxi64 iTs,sxi32 iOff,sxi64 y,sxi64 w,sxi64 dow)` |
|      1 | 2957 | `{` |
|      - | 2958 | `	sxi64 iLocal,iTod,jan4,monday1,target;` |
|      - | 2959 | `	int isoDow;` |
|      9 | 2960 | `	iLocal = iTs + iOff;` |
|      9 | 2961 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|      9 | 2962 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|      9 | 2963 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|      9 | 2964 | `	monday1 = jan4 - (isoDow - 1);` |
|      9 | 2965 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|      9 | 2966 | `	return target*86400 + iTod - iOff;` |
|      1 | 2967 | `}` |
|      - | 2968 | `/*` |
|      - | 2969 | ` * php's DateTime::createFromFormat engine.` |
|      - | 2970 | ` *` |
|      - | 2971 | `` * This was the `__dt_from_format()` thunk, whose answer had to survive a trip`` |
|      - | 2972 | ` * through PHP: an ARRAY on success and a "COUNT\nPOS\tMESSAGE" string on failure,` |
|      - | 2973 | ` * which the chunk then re-parsed. Both encodings are gone — the native methods call` |
|      - | 2974 | ` * this directly and read the diagnostics as a struct. That is also why a parse that` |
|      - | 2975 | ` * has BOTH errors and warnings can now report both: the failure encoding had no room` |
|      - | 2976 | `` * for warnings, so php's `warning_count` was silently 0 whenever an error was present.`` |
|      - | 2977 | ` *` |
|      - | 2978 | ` * Returns 0 when the parse produced a time and non-zero when it did not; pOut->sDiag` |
|      - | 2979 | ` * carries the warnings/errors either way (offKind: 0 none parsed, 1 numeric offset,` |
|      - | 2980 | ` * 2 literal Z, 3 named identifier).` |
|      - | 2981 | ` */` |
|      - | 2982 | `/*` |
|      - | 2983 | ` * Publish one scan's warnings and errors as the record getLastErrors() answers.` |
|      - | 2984 | ` * The messages are static literals, so the record copies pointers, never bytes.` |
|      - | 2985 | ` */` |
|      - | 2986 | `/*` |
|      - | 2987 | ` * Record one parse's diagnostics as getLastErrors()'s answer.` |
|      - | 2988 | ` *` |
|      - | 2989 | ` * php resets the record on EVERY constructor, modify() and createFromFormat()` |
|      - | 2990 | `` * call -- a clean parse answers `false` again -- and publishes what the scan`` |
|      - | 2991 | ` * collected, which is every error it met rather than the one it stopped on.` |
|      - | 2992 | ` * The error rows grow with the string (one per byte at worst), the warnings` |
|      - | 2993 | ` * cannot exceed their own three rules, and every message is a static literal.` |
|      - | 2994 | ` */` |
|  11637 | 2995 | `static void DtRecReset(phl_dt_lasterr *pRec)` |
|      5 | 2996 | `{` |
|  11642 | 2997 | `	pRec->bSet = 0;` |
|  11642 | 2998 | `	pRec->nWarn = pRec->nWarnKept = 0;` |
|  11642 | 2999 | `	pRec->nErr = pRec->nErrKept = 0;` |
|  11642 | 3000 | `	SyBlobReset(&pRec->sErr);` |
|  11642 | 3001 | `}` |
|   1324 | 3002 | `static void DtRecErr(phl_dt_lasterr *pRec,int iPos,const char *zMsg)` |
|      2 | 3003 | `{` |
|      - | 3004 | `	phl_dt_diag_row sRow;` |
|   1326 | 3005 | `	if( pRec == 0 ){` |
|    212 | 3006 | `		return;` |
|      - | 3007 | `	}` |
|   1116 | 3008 | `	pRec->bSet = 1;` |
|   1116 | 3009 | `	pRec->nErr++;` |
|   1116 | 3010 | `	sRow.iPos = iPos;` |
|   1116 | 3011 | `	sRow.zMsg = zMsg;` |
|   1116 | 3012 | `	if( SyBlobAppend(&pRec->sErr,(const void *)&sRow,sizeof(sRow)) == SXRET_OK ){` |
|   1116 | 3013 | `		pRec->nErrKept++;` |
|    557 | 3014 | `	}` |
|    664 | 3015 | `}` |
|    380 | 3016 | `static void DtRecWarn(phl_dt_lasterr *pRec,int iPos,const char *zMsg)` |
|      1 | 3017 | `{` |
|    381 | 3018 | `	if( pRec == 0 ){` |
|     75 | 3019 | `		return;` |
|      - | 3020 | `	}` |
|    307 | 3021 | `	pRec->bSet = 1;` |
|    307 | 3022 | `	pRec->nWarn++;` |
|    307 | 3023 | `	if( pRec->nWarnKept < PH7_DT_MAX_WARN ){` |
|    307 | 3024 | `		pRec->aWarnPos[pRec->nWarnKept] = iPos;` |
|    307 | 3025 | `		pRec->azWarn[pRec->nWarnKept] = zMsg;` |
|    307 | 3026 | `		pRec->nWarnKept++;` |
|    153 | 3027 | `	}` |
|    191 | 3028 | `}` |
|   6721 | 3029 | `static void DtLastErrClear(ph7_vm *pVm)` |
|      5 | 3030 | `{` |
|   6726 | 3031 | `	DtRecReset(&pVm->sDtLastErr);` |
|   6726 | 3032 | `}` |
|      - | 3033 | `typedef struct dt_ff_diag dt_ff_diag;` |
|      - | 3034 | `struct dt_ff_diag` |
|      - | 3035 | `{` |
|      - | 3036 | `	int nErr,nErrKept;` |
|      - | 3037 | `	int aErrPos[PH7_DT_MAX_ERR];` |
|      - | 3038 | `	const char *azErr[PH7_DT_MAX_ERR];` |
|      - | 3039 | `	int nWarn;` |
|      - | 3040 | `	int aWarnPos[PH7_DT_MAX_WARN];` |
|      - | 3041 | `	const char *azWarn[PH7_DT_MAX_WARN];` |
|      - | 3042 | `};` |
|    634 | 3043 | `static void DtFfDiag(dt_ff_diag *pDiag,int nErr,int nErrKept,const int *aErrPos,` |
|      - | 3044 | `	const char **azErr,int nWarn,const int *aWarnPos,const char **azWarn)` |
|      1 | 3045 | `{` |
|      - | 3046 | `	int k;` |
|    635 | 3047 | `	pDiag->nErr = nErr;` |
|    635 | 3048 | `	pDiag->nErrKept = nErrKept;` |
|    881 | 3049 | `	for( k = 0 ; k < nErrKept ; k++ ){` |
|    247 | 3050 | `		pDiag->aErrPos[k] = aErrPos[k];` |
|    247 | 3051 | `		pDiag->azErr[k] = azErr[k];` |
|    124 | 3052 | `	}` |
|    635 | 3053 | `	pDiag->nWarn = nWarn;` |
|    687 | 3054 | `	for( k = 0 ; k < nWarn ; k++ ){` |
|     53 | 3055 | `		pDiag->aWarnPos[k] = aWarnPos[k];` |
|     53 | 3056 | `		pDiag->azWarn[k] = azWarn[k];` |
|     27 | 3057 | `	}` |
|    635 | 3058 | `}` |
|      - | 3059 | `/* ...poured into a record of the shape a string scan fills, so that both` |
|      - | 3060 | ` * readers of a format scan -- getLastErrors() and the component view -- show` |
|      - | 3061 | ` * it through the same presenter. */` |
|    634 | 3062 | `static void DtFfDiagInto(phl_dt_lasterr *pRec,const dt_ff_diag *pDiag)` |
|      1 | 3063 | `{` |
|      - | 3064 | `	int k;` |
|    635 | 3065 | `	DtRecReset(pRec);` |
|    687 | 3066 | `	for( k = 0 ; k < pDiag->nWarn ; k++ ){` |
|     53 | 3067 | `		DtRecWarn(pRec,pDiag->aWarnPos[k],pDiag->azWarn[k]);` |
|     27 | 3068 | `	}` |
|    881 | 3069 | `	for( k = 0 ; k < pDiag->nErrKept ; k++ ){` |
|    247 | 3070 | `		DtRecErr(pRec,pDiag->aErrPos[k],pDiag->azErr[k]);` |
|    124 | 3071 | `	}` |
|    635 | 3072 | `	pRec->nErr = pDiag->nErr;   /* php counts what it dropped too */` |
|    635 | 3073 | `}` |
|      - | 3074 | `/* ...and the VM's own, which getLastErrors() answers from. */` |
|    512 | 3075 | `static void DtLastErrFf(ph7_vm *pVm,const dt_ff_diag *pDiag)` |
|      1 | 3076 | `{` |
|    513 | 3077 | `	DtFfDiagInto(&pVm->sDtLastErr,pDiag);` |
|    513 | 3078 | `}` |
|      - | 3079 | `/*` |
|      - | 3080 | ` * php's do_range_limit: carry *pa into *pb until *pa sits inside [iStart,iEnd).` |
|      - | 3081 | ` * Spelled the way php spells it, the arithmetic on a field nothing ever set` |
|      - | 3082 | ` * included -- an unset minute is just a very negative number to this code, and` |
|      - | 3083 | `` * what it carries into the hour is what a `z` beside a half-read clock shows.`` |
|      - | 3084 | ` */` |
|   3226 | 3085 | `static void DtFfRangeLimit(sxi64 iStart,sxi64 iEnd,sxi64 iAdj,sxi64 *pa,sxi64 *pb)` |
|      1 | 3086 | `{` |
|   3227 | 3087 | `	if( *pa < iStart ){` |
|    725 | 3088 | `		sxi64 a1 = *pa + 1;` |
|    725 | 3089 | `		*pb -= (iStart - a1) / iAdj + 1;` |
|    725 | 3090 | `		*pa += iAdj * ((iStart - a1) / iAdj);` |
|    725 | 3091 | `		*pa += iAdj;` |
|    362 | 3092 | `	}` |
|   3227 | 3093 | `	if( *pa >= iEnd ){` |
|     49 | 3094 | `		*pb += *pa / iAdj;` |
|     49 | 3095 | `		*pa -= iAdj * (*pa / iAdj);` |
|     24 | 3096 | `	}` |
|   3227 | 3097 | `}` |
|      - | 3098 | `/* ...and its day half, which walks whole months rather than dividing: one call` |
|      - | 3099 | ` * takes the day inside the current month or gives up at the end of a year, and` |
|      - | 3100 | ` * the caller runs it until it has nothing left to move. */` |
|   1008 | 3101 | `static int DtFfRangeLimitDays(sxi64 *py,sxi64 *pm,sxi64 *pd)` |
|      1 | 3102 | `{` |
|   1009 | 3103 | `	int rc = 0;` |
|   1009 | 3104 | `	if( *pd >= 146097 \|\| *pd <= -146097 ){` |
|      - | 3105 | `		/* a whole 400-year era at a time */` |
|      3 | 3106 | `		*py += 400 * (*pd / 146097);` |
|      3 | 3107 | `		*pd -= 146097 * (*pd / 146097);` |
|      1 | 3108 | `	}` |
|   1009 | 3109 | `	DtFfRangeLimit(1,13,12,pm,py);` |
|   9639 | 3110 | `	while( *pd <= 0 && *pm > 0 ){` |
|   8631 | 3111 | `		sxi64 iPrevM = *pm - 1,iPrevY = *py;` |
|   8631 | 3112 | `		if( iPrevM < 1 ){` |
|    721 | 3113 | `			iPrevM += 12;` |
|    721 | 3114 | `			iPrevY = *py - 1;` |
|    360 | 3115 | `		}` |
|   8631 | 3116 | `		*pd += DtDaysInMonth(iPrevY,(int)iPrevM);` |
|   8631 | 3117 | `		(*pm)--;` |
|   8631 | 3118 | `		rc = 1;` |
|      1 | 3119 | `	}` |
|   1111 | 3120 | `	while( *pd > 0 && *pm >= 1 && *pm <= 12 && *pd > DtDaysInMonth(*py,(int)*pm) ){` |
|    103 | 3121 | `		*pd -= DtDaysInMonth(*py,(int)*pm);` |
|    103 | 3122 | `		(*pm)++;` |
|    103 | 3123 | `		rc = 1;` |
|      1 | 3124 | `	}` |
|   1009 | 3125 | `	return rc;` |
|      1 | 3126 | `}` |
|      - | 3127 | `/* php's timelib_do_normalize, asked of the whole vector wherever a format's` |
|      - | 3128 | ` * day-of-year stands. The clock is only carried when the SECOND was read --` |
|      - | 3129 | ` * php's own guard, and not the one anybody would write. */` |
|    412 | 3130 | `static void DtFfNormalize(sxi64 *py,sxi64 *pm,sxi64 *pd,sxi64 *ph,sxi64 *pi,` |
|      - | 3131 | `	sxi64 *ps,sxi64 *pus)` |
|      1 | 3132 | `{` |
|    413 | 3133 | `	if( *pus != DT_UNSET ){ DtFfRangeLimit(0,1000000,1000000,pus,ps); }` |
|    413 | 3134 | `	if( *ps != DT_UNSET ){` |
|    391 | 3135 | `		DtFfRangeLimit(0,60,60,ps,pi);` |
|    391 | 3136 | `		DtFfRangeLimit(0,60,60,pi,ph);` |
|    391 | 3137 | `		DtFfRangeLimit(0,24,24,ph,pd);` |
|    195 | 3138 | `	}` |
|    413 | 3139 | `	DtFfRangeLimit(1,13,12,pm,py);` |
|    413 | 3140 | `	if( *py == 1970 && *pm == 1 ){` |
|      - | 3141 | `		/* php's short cut past the walk, straight off the epoch */` |
|      - | 3142 | `		sxi64 iY;` |
|      - | 3143 | `		int iM,iD;` |
|    163 | 3144 | `		DtCivilFromDays(*pd - 1,&iY,&iM,&iD);` |
|    163 | 3145 | `		*py = iY; *pm = iM; *pd = iD;` |
|    163 | 3146 | `		return;` |
|      - | 3147 | `	}` |
|   1009 | 3148 | `	while( DtFfRangeLimitDays(py,pm,pd) ){}` |
|    251 | 3149 | `	DtFfRangeLimit(1,13,12,pm,py);` |
|    207 | 3150 | `}` |
|      - | 3151 | `/* strtol over a bounded run: it reads the digits it finds and stops at the` |
|      - | 3152 | ` * first byte that is not one, which is how php's offset arithmetic reads each` |
|      - | 3153 | ` * group of a colon spelling out of the middle of the run. */` |
|     68 | 3154 | `static sxi64 DtFfZoneNum(const char *z,const char *zEnd)` |
|      1 | 3155 | `{` |
|     69 | 3156 | `	sxi64 v = 0;` |
|    211 | 3157 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|    143 | 3158 | `		v = v*10 + (z[0] - '0');` |
|    143 | 3159 | `		z++;` |
|      1 | 3160 | `	}` |
|     69 | 3161 | `	return v;` |
|      1 | 3162 | `}` |
|      - | 3163 | `/*` |
|      - | 3164 | ` * php's timelib_parse_tz_cor, the digits behind a format zone's sign. It takes` |
|      - | 3165 | ` * the whole run of digits and colons and then decides what the run MEANT from` |
|      - | 3166 | ``  * its length alone, which is why `+9999` is 99 hours and 99 minutes and `+2460` `` |
|      - | 3167 | ` * is 24 hours and 60: nothing here is in range of anything. A length the switch` |
|      - | 3168 | ` * does not name is no offset at all. Answers 1 when the run spelled one.` |
|      - | 3169 | ` */` |
|     54 | 3170 | `static int DtFfZoneCor(const char **pz,const char *zEnd,sxi32 *piOff)` |
|      1 | 3171 | `{` |
|     55 | 3172 | `	const char *z = *pz,*zBeg = *pz;` |
|      - | 3173 | `	int n;` |
|      - | 3174 | `	sxi64 v;` |
|    274 | 3175 | `	while( z < zEnd && (SyisDigit(z[0]) \|\| z[0] == ':') ){ z++; }` |
|     55 | 3176 | `	n = (int)(z - zBeg);` |
|     55 | 3177 | `	*pz = z;` |
|     55 | 3178 | `	*piOff = 0;` |
|     55 | 3179 | `	switch( n ){` |
|      5 | 3180 | `	case 1: case 2:` |
|     11 | 3181 | `		*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600);` |
|     11 | 3182 | `		return 1;` |
|      6 | 3183 | `	case 3: case 4:` |
|     13 | 3184 | `		if( zBeg[1] == ':' ){` |
|      7 | 3185 | `			*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600` |
|      4 | 3186 | `				+ DtFfZoneNum(&zBeg[2],zEnd) * 60);` |
|     11 | 3187 | `		}else if( zBeg[2] == ':' ){` |
|      4 | 3188 | `			*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600` |
|      2 | 3189 | `				+ DtFfZoneNum(&zBeg[3],zEnd) * 60);` |
|      2 | 3190 | `		}else{` |
|      7 | 3191 | `			v = DtFfZoneNum(zBeg,zEnd);` |
|      7 | 3192 | `			*piOff = (sxi32)((v / 100) * 3600 + (v % 100) * 60);` |
|      - | 3193 | `		}` |
|     13 | 3194 | `		return 1;` |
|      9 | 3195 | `	case 5:` |
|     19 | 3196 | `		if( zBeg[2] != ':' ){ break; }` |
|     25 | 3197 | `		*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600` |
|     16 | 3198 | `			+ DtFfZoneNum(&zBeg[3],zEnd) * 60);` |
|     17 | 3199 | `		return 1;` |
|      1 | 3200 | `	case 6:` |
|      3 | 3201 | `		v = DtFfZoneNum(zBeg,zEnd);` |
|      3 | 3202 | `		*piOff = (sxi32)((v / 10000) * 3600 + ((v / 100) % 100) * 60 + (v % 100));` |
|      3 | 3203 | `		return 1;` |
|      1 | 3204 | `	case 8:` |
|      3 | 3205 | `		if( zBeg[2] != ':' \|\| zBeg[5] != ':' ){ break; }` |
|      4 | 3206 | `		*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600` |
|      2 | 3207 | `			+ DtFfZoneNum(&zBeg[3],zEnd) * 60 + DtFfZoneNum(&zBeg[6],zEnd));` |
|      3 | 3208 | `		return 1;` |
|      5 | 3209 | `	default:` |
|     10 | 3210 | `		break;` |
|      - | 3211 | `	}` |
|     13 | 3212 | `	return 0;` |
|     28 | 3213 | `}` |
|      - | 3214 | `/*` |
|      - | 3215 | ` * php's timelib_parse_zone, and there is no SHAPE to match here the way the` |
|      - | 3216 | ` * string scanner matches one: a format's zone specifier reads whatever stands` |
|      - | 3217 | `` * at the cursor. Blanks and opening parens go first, an uppercase `GMT` in`` |
|      - | 3218 | ` * front of a sign is dropped, a sign is a UTC OFFSET whatever follows it, and` |
|      - | 3219 | `` * anything else is a NAME taken to the end of its run -- letters, digits, `/`,`` |
|      - | 3220 | `` * `_`, `+` and `-` all belong to it, which is why `gmt+3` is one unknown word`` |
|      - | 3221 | `` * where `GMT+3` is three hours.`` |
|      - | 3222 | ` *` |
|      - | 3223 | ` * A sign settles the KIND before the digits are read, so an offset nothing` |
|      - | 3224 | ` * follows is still an offset -- of zero, with a refusal beside it. Answers 1` |
|      - | 3225 | ` * when the zone resolved, 0 when it did not.` |
|      - | 3226 | ` */` |
|    142 | 3227 | `static int DtFfZone(const char **pz,const char *zEnd,int *piKind,sxi32 *piOff,` |
|      - | 3228 | `	const char **pzName,int *pnName)` |
|      1 | 3229 | `{` |
|    143 | 3230 | `	const char *z = *pz;` |
|    143 | 3231 | `	int nPar = 0,bNeg,bIdent = 0,rc;` |
|      - | 3232 | `	/* The OFFSET is written whatever happens -- php assigns the reader's answer,` |
|      - | 3233 | `	 * which is zero when it resolved nothing -- while the KIND and the NAME are` |
|      - | 3234 | `	 * touched only by a zone that DID resolve. So a second specifier that finds` |
|      - | 3235 | `	 * nothing zeroes the offset the first one read and leaves its kind standing. */` |
|    143 | 3236 | `	*piOff = 0;` |
|    229 | 3237 | `	while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t' \|\| z[0] == '(') ){` |
|     11 | 3238 | `		if( z[0] == '(' ){ nPar++; }` |
|     11 | 3239 | `		z++;` |
|      1 | 3240 | `	}` |
|    143 | 3241 | `	if( zEnd - z > 3 && SyMemcmp(z,"GMT",3) == 0 && (z[3] == '+' \|\| z[3] == '-') ){` |
|      5 | 3242 | `		z += 3;` |
|      2 | 3243 | `	}` |
|    143 | 3244 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|     55 | 3245 | `		bNeg = (z[0] == '-');` |
|     55 | 3246 | `		z++;` |
|     55 | 3247 | `		*piKind = DT_ZONE_OFFSET;` |
|     55 | 3248 | `		rc = DtFfZoneCor(&z,zEnd,piOff);` |
|     55 | 3249 | `		if( bNeg ){ *piOff = -*piOff; }` |
|     55 | 3250 | `		*pz = z;` |
|     55 | 3251 | `		return rc;` |
|      - | 3252 | `	}` |
|      - | 3253 | `	{` |
|     89 | 3254 | `		const char *zWord = z;` |
|      - | 3255 | `		int nWord;` |
|    329 | 3256 | `		while( z < zEnd && (SyisAlphaNum((unsigned char)z[0]) \|\| z[0] == '/'` |
|     43 | 3257 | `		 \|\| z[0] == '_' \|\| z[0] == '-' \|\| z[0] == '+') ){` |
|    197 | 3258 | `			z++;` |
|      1 | 3259 | `		}` |
|     89 | 3260 | `		nWord = (int)(z - zWord);` |
|     89 | 3261 | `		rc = nWord > 0 && DtZoneName(zWord,nWord,piOff,pzName,pnName,&bIdent);` |
|     89 | 3262 | `		if( rc ){` |
|     59 | 3263 | `			*piKind = bIdent ? DT_ZONE_ID : DT_ZONE_ABBR;` |
|     29 | 3264 | `		}` |
|     95 | 3265 | `		while( nPar > 0 && z < zEnd && z[0] == ')' ){` |
|      7 | 3266 | `			z++;` |
|      7 | 3267 | `			nPar--;` |
|      1 | 3268 | `		}` |
|     89 | 3269 | `		*pz = z;` |
|     89 | 3270 | `		return rc;` |
|      - | 3271 | `	}` |
|     72 | 3272 | `}` |
|      - | 3273 | `/*` |
|      - | 3274 | ` * php's timelib_get_nr, the reader behind every plain digit field of a format:` |
|      - | 3275 | ` * it steps over whatever is NOT a digit -- to the end of the input if it has` |
|      - | 3276 | ` * to -- and then takes at most nMax of them. Answers how many digits it took,` |
|      - | 3277 | ` * or -1 when the input ran out before it found one; the cursor moves either way.` |
|      - | 3278 | ` */` |
|    696 | 3279 | `static int DtFfGetNr(const char **pz,const char *zEnd,int nMax,sxi64 *pVal)` |
|      1 | 3280 | `{` |
|    697 | 3281 | `	const char *z = *pz;` |
|    697 | 3282 | `	sxi64 v = 0;` |
|    697 | 3283 | `	int n = 0;` |
|    847 | 3284 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|    697 | 3285 | `	if( z >= zEnd ){` |
|     19 | 3286 | `		*pz = z;` |
|     19 | 3287 | `		return -1;` |
|      - | 3288 | `	}` |
|   2365 | 3289 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|   1687 | 3290 | `		v = v*10 + (z[0] - '0');` |
|   1687 | 3291 | `		z++;` |
|   1687 | 3292 | `		n++;` |
|      1 | 3293 | `	}` |
|    679 | 3294 | `	*pz = z;` |
|    679 | 3295 | `	*pVal = v;` |
|    679 | 3296 | `	return n;` |
|    349 | 3297 | `}` |
|      - | 3298 | `/*` |
|      - | 3299 | `` * php's timelib_get_signed_nr, which `U` reads through: it steps over anything`` |
|      - | 3300 | ` * that is neither a digit nor a sign, takes a RUN of signs (each minus flipping` |
|      - | 3301 | ` * it), steps over non-digits again, and reads at most nMax digits. Its two ways` |
|      - | 3302 | ` * of giving up -- an input that ends before a digit, and a value no int64 can` |
|      - | 3303 | ` * hold -- are refusals php raises through its STRING scanner's door rather than` |
|      - | 3304 | ` * the format one's, so both are reported at position 0 whatever the format was` |
|      - | 3305 | ` * doing, and both answer zero.` |
|      - | 3306 | ` */` |
|     44 | 3307 | `static int DtFfGetSignedNr(const char **pz,const char *zEnd,int nMax,sxi64 *pVal,` |
|      - | 3308 | `	const char **pzErr)` |
|      1 | 3309 | `{` |
|     45 | 3310 | `	const char *z = *pz;` |
|     45 | 3311 | `	sxu64 u = 0,uLimit;` |
|     45 | 3312 | `	int bNeg = 0,n = 0,bOver = 0;` |
|     45 | 3313 | `	*pzErr = 0;` |
|     45 | 3314 | `	*pVal = 0;` |
|     61 | 3315 | `	while( z < zEnd && !SyisDigit(z[0]) && z[0] != '+' && z[0] != '-' ){ z++; }` |
|     45 | 3316 | `	if( z >= zEnd ){` |
|      5 | 3317 | `		*pz = z;` |
|      5 | 3318 | `		*pzErr = "Found unexpected data";` |
|      5 | 3319 | `		return 0;` |
|      - | 3320 | `	}` |
|    106 | 3321 | `	while( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|     31 | 3322 | `		if( z[0] == '-' ){ bNeg = !bNeg; }` |
|     31 | 3323 | `		z++;` |
|      1 | 3324 | `	}` |
|     41 | 3325 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|     41 | 3326 | `	if( z >= zEnd ){` |
|    ! 0 | 3327 | `		*pz = z;` |
|    ! 0 | 3328 | `		*pzErr = "Found unexpected data";` |
|    ! 0 | 3329 | `		return 0;` |
|      - | 3330 | `	}` |
|      - | 3331 | `	/* php's ceiling is strtoll's, so the negative side reaches one further */` |
|     41 | 3332 | `	uLimit = bNeg ? ((sxu64)SXI64_HIGH + 1) : (sxu64)SXI64_HIGH;` |
|    259 | 3333 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|    219 | 3334 | `		sxu64 dg = (sxu64)(z[0] - '0');` |
|    219 | 3335 | `		if( u > (uLimit - dg) / 10 ){` |
|     15 | 3336 | `			bOver = 1;` |
|      7 | 3337 | `		}` |
|    219 | 3338 | `		if( !bOver ){` |
|    205 | 3339 | `			u = u*10 + dg;` |
|    102 | 3340 | `		}` |
|    219 | 3341 | `		z++;` |
|    219 | 3342 | `		n++;` |
|      1 | 3343 | `	}` |
|     41 | 3344 | `	*pz = z;` |
|     41 | 3345 | `	if( bOver ){` |
|      5 | 3346 | `		*pzErr = "Number out of range";` |
|      5 | 3347 | `		return 0;` |
|      - | 3348 | `	}` |
|      - | 3349 | `	/* the negation is spelled unsigned: the floor has no positive twin */` |
|     37 | 3350 | `	*pVal = bNeg ? (sxi64)(0 - u) : (sxi64)u;` |
|     37 | 3351 | `	return 1;` |
|     23 | 3352 | `}` |
|      - | 3353 | `/*` |
|      - | 3354 | ` * php's MONTH table, which a format matches as a whole WORD: the letters are` |
|      - | 3355 | ` * taken to the end of their run and the run has to spell one of the names` |
|      - | 3356 | `` * exactly, so `janx` is no month at all where the string parser reads January`` |
|      - | 3357 | `` * out of it. The names are php's own -- three letters, `sept`, the full months,`` |
|      - | 3358 | `` * and the ROMAN numerals `i` through `xii`, which is why a format's `F` reads`` |
|      - | 3359 | `` * `x` as October. Advances *pz over the run whether or not it spelled one.`` |
|      - | 3360 | ` */` |
|     38 | 3361 | `static int DtFfMonth(const char **pz,const char *zEnd)` |
|      1 | 3362 | `{` |
|      - | 3363 | `	static const struct { const char *z; int n; int mo; } aM[] = {` |
|      - | 3364 | `		{ "jan",3,1 },{ "feb",3,2 },{ "mar",3,3 },{ "apr",3,4 },{ "may",3,5 },` |
|      - | 3365 | `		{ "jun",3,6 },{ "jul",3,7 },{ "aug",3,8 },{ "sep",3,9 },{ "sept",4,9 },` |
|      - | 3366 | `		{ "oct",3,10 },{ "nov",3,11 },{ "dec",3,12 },` |
|      - | 3367 | `		{ "i",1,1 },{ "ii",2,2 },{ "iii",3,3 },{ "iv",2,4 },{ "v",1,5 },` |
|      - | 3368 | `		{ "vi",2,6 },{ "vii",3,7 },{ "viii",4,8 },{ "ix",2,9 },{ "x",1,10 },` |
|      - | 3369 | `		{ "xi",2,11 },{ "xii",3,12 },` |
|      - | 3370 | `		{ "january",7,1 },{ "february",8,2 },{ "march",5,3 },{ "april",5,4 },` |
|      - | 3371 | `		{ "june",4,6 },{ "july",4,7 },{ "august",6,8 },{ "september",9,9 },` |
|      - | 3372 | `		{ "october",7,10 },{ "november",8,11 },{ "december",8,12 }` |
|      - | 3373 | `	};` |
|     39 | 3374 | `	const char *z = *pz,*zWord = *pz;` |
|      - | 3375 | `	sxu32 i;` |
|      - | 3376 | `	int n;` |
|    189 | 3377 | `	while( z < zEnd && SyisAlpha((unsigned char)z[0]) ){ z++; }` |
|     39 | 3378 | `	n = (int)(z - zWord);` |
|     39 | 3379 | `	*pz = z;` |
|    509 | 3380 | `	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){` |
|    507 | 3381 | `		if( aM[i].n == n && SyStrnicmp(zWord,aM[i].z,(sxu32)n) == 0 ){` |
|     37 | 3382 | `			return aM[i].mo;` |
|      - | 3383 | `		}` |
|    236 | 3384 | `	}` |
|      3 | 3385 | `	return 0;` |
|     20 | 3386 | `}` |
|      - | 3387 | `/*` |
|      - | 3388 | ` * ...and php's RELATIVE-UNIT table, which is where a format's textual DAY is` |
|      - | 3389 | `` * looked up. `D` and `l` do not read a weekday name at all there: they read a`` |
|      - | 3390 | ` * word up to the next separator and ask the relative-unit table what it is, so` |
|      - | 3391 | `` * every unit spelling answers one -- `week` is the weekday 7 and `ms` the`` |
|      - | 3392 | ` * weekday 1000, neither of which is a day of any week. Advances *pz over the` |
|      - | 3393 | ` * word whether or not it spelled one; answers 1 and fills *piWday when it did.` |
|      - | 3394 | ` */` |
|     46 | 3395 | `static int DtFfRelunit(const char **pz,const char *zEnd,sxi64 *piWday)` |
|      1 | 3396 | `{` |
|      - | 3397 | `	static const struct { const char *z; int n; int mul; } aU[] = {` |
|      - | 3398 | `		{ "ms",2,1000 },{ "msec",4,1000 },{ "msecs",5,1000 },` |
|      - | 3399 | `		{ "millisecond",11,1000 },{ "milliseconds",12,1000 },` |
|      - | 3400 | `		{ "\xc2\xb5s",3,1 },{ "usec",4,1 },{ "usecs",5,1 },` |
|      - | 3401 | `		{ "\xc2\xb5sec",5,1 },{ "\xc2\xb5secs",6,1 },` |
|      - | 3402 | `		{ "microsecond",11,1 },{ "microseconds",12,1 },` |
|      - | 3403 | `		{ "sec",3,1 },{ "secs",4,1 },{ "second",6,1 },{ "seconds",7,1 },` |
|      - | 3404 | `		{ "min",3,1 },{ "mins",4,1 },{ "minute",6,1 },{ "minutes",7,1 },` |
|      - | 3405 | `		{ "hour",4,1 },{ "hours",5,1 },` |
|      - | 3406 | `		{ "day",3,1 },{ "days",4,1 },` |
|      - | 3407 | `		{ "week",4,7 },{ "weeks",5,7 },` |
|      - | 3408 | `		{ "fortnight",9,14 },{ "fortnights",10,14 },` |
|      - | 3409 | `		{ "forthnight",10,14 },{ "forthnights",11,14 },` |
|      - | 3410 | `		{ "month",5,1 },{ "months",6,1 },` |
|      - | 3411 | `		{ "year",4,1 },{ "years",5,1 },` |
|      - | 3412 | `		{ "mondays",7,1 },{ "monday",6,1 },{ "mon",3,1 },` |
|      - | 3413 | `		{ "tuesdays",8,2 },{ "tuesday",7,2 },{ "tue",3,2 },` |
|      - | 3414 | `		{ "wednesdays",10,3 },{ "wednesday",9,3 },{ "wed",3,3 },` |
|      - | 3415 | `		{ "thursdays",9,4 },{ "thursday",8,4 },{ "thu",3,4 },` |
|      - | 3416 | `		{ "fridays",7,5 },{ "friday",6,5 },{ "fri",3,5 },` |
|      - | 3417 | `		{ "saturdays",9,6 },{ "saturday",8,6 },{ "sat",3,6 },` |
|      - | 3418 | `		{ "sundays",7,0 },{ "sunday",6,0 },{ "sun",3,0 },` |
|      - | 3419 | `		{ "weekday",7,1 },{ "weekdays",8,1 }` |
|      - | 3420 | `	};` |
|     47 | 3421 | `	const char *z = *pz,*zWord = *pz;` |
|      - | 3422 | `	sxu32 i;` |
|      - | 3423 | `	int n;` |
|    269 | 3424 | `	while( z < zEnd && z[0] != ' ' && z[0] != ',' && z[0] != '\t' && z[0] != ';'` |
|    190 | 3425 | `	 && z[0] != ':' && z[0] != '/' && z[0] != '.' && z[0] != '-'` |
|    309 | 3426 | `	 && z[0] != '(' && z[0] != ')' ){` |
|    191 | 3427 | `		z++;` |
|      1 | 3428 | `	}` |
|     47 | 3429 | `	n = (int)(z - zWord);` |
|     47 | 3430 | `	*pz = z;` |
|   1875 | 3431 | `	for( i = 0 ; i < SX_ARRAYSIZE(aU) ; ++i ){` |
|   1873 | 3432 | `		if( aU[i].n == n && SyStrnicmp(zWord,aU[i].z,(sxu32)n) == 0 ){` |
|     45 | 3433 | `			*piWday = aU[i].mul;` |
|     45 | 3434 | `			return 1;` |
|      - | 3435 | `		}` |
|    915 | 3436 | `	}` |
|      3 | 3437 | `	return 0;` |
|     24 | 3438 | `}` |
|      - | 3439 | `/*` |
|      - | 3440 | ` * php's meridian, which is an ADJUSTMENT to whatever hour was already read` |
|      - | 3441 | `` * rather than a reading of its own: `am` takes noon back to midnight and leaves`` |
|      - | 3442 | `` * every other hour standing, `pm` adds twelve to all but twelve itself.`` |
|      - | 3443 | ` *` |
|      - | 3444 | `` * It hunts for its own letter -- anything that is not one of `AaPp` is stepped`` |
|      - | 3445 | `` * over, so `1 xx pm` is one in the afternoon -- and then wants either a bare`` |
|      - | 3446 | `` * `m` or the whole `.m.`; the cursor stays wherever the spelling ran out when`` |
|      - | 3447 | ` * it turns out to be neither. Answers 1 and fills *piAdj, or 0.` |
|      - | 3448 | ` */` |
|     38 | 3449 | `static int DtFfMeridian(const char **pz,const char *zEnd,sxi64 h,sxi64 *piAdj)` |
|      1 | 3450 | `{` |
|     39 | 3451 | `	const char *z = *pz;` |
|      - | 3452 | `	int bAm;` |
|     51 | 3453 | `	while( z < zEnd && z[0] != 'A' && z[0] != 'a' && z[0] != 'P' && z[0] != 'p' ){` |
|     13 | 3454 | `		z++;` |
|      1 | 3455 | `	}` |
|     39 | 3456 | `	if( z >= zEnd ){` |
|    ! 0 | 3457 | `		*pz = z;` |
|    ! 0 | 3458 | `		return 0;` |
|      - | 3459 | `	}` |
|     39 | 3460 | `	bAm = (z[0] == 'a' \|\| z[0] == 'A');` |
|     39 | 3461 | `	*piAdj = bAm ? ((h == 12) ? -12 : 0) : ((h != 12) ? 12 : 0);` |
|     39 | 3462 | `	z++;` |
|     39 | 3463 | `	if( z < zEnd && z[0] == '.' ){` |
|      5 | 3464 | `		z++;` |
|      5 | 3465 | `		if( z >= zEnd \|\| (z[0] != 'm' && z[0] != 'M') ){ *pz = z; return 0; }` |
|      5 | 3466 | `		z++;` |
|      5 | 3467 | `		if( z >= zEnd \|\| z[0] != '.' ){ *pz = z; return 0; }` |
|    ! 0 | 3468 | `		z++;` |
|     35 | 3469 | `	}else if( z < zEnd && (z[0] == 'm' \|\| z[0] == 'M') ){` |
|     33 | 3470 | `		z++;` |
|     17 | 3471 | `	}else{` |
|      3 | 3472 | `		*pz = z;` |
|      3 | 3473 | `		return 0;` |
|      - | 3474 | `	}` |
|     33 | 3475 | `	*pz = z;` |
|     33 | 3476 | `	return 1;` |
|     20 | 3477 | `}` |
|      - | 3478 | `/*` |
|      - | 3479 | `` * The eight bytes php's format map calls SEPARATORS: what `#` accepts, and what`` |
|      - | 3480 | ` * each of them demands of the input when it stands in a format itself.` |
|      - | 3481 | ` */` |
|      8 | 3482 | `static int DtFfIsSep(int c)` |
|      1 | 3483 | `{` |
|     10 | 3484 | `	return c==';' \|\| c==':' \|\| c=='/' \|\| c=='.' \|\| c==',' \|\| c=='-'` |
|     11 | 3485 | `		\|\| c=='(' \|\| c==')';` |
|      1 | 3486 | `}` |
|      - | 3487 | `/*` |
|      - | 3488 | ` * php's run of blanks -- the two ASCII ones and the two Unicode spaces its` |
|      - | 3489 | ` * scanner spells out. A format space eats the whole run and never refuses, so a` |
|      - | 3490 | ` * space beside an input that has none is simply nothing.` |
|      - | 3491 | ` */` |
|    144 | 3492 | `static void DtFfEatSpaces(const char **pz,const char *zEnd)` |
|      1 | 3493 | `{` |
|    145 | 3494 | `	const char *z = *pz;` |
|     72 | 3495 | `	for(;;){` |
|    297 | 3496 | `		if( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|    149 | 3497 | `			z++;` |
|    149 | 3498 | `			continue;` |
|      - | 3499 | `		}` |
|    148 | 3500 | `		if( zEnd - z >= 3 && (unsigned char)z[0] == 0xE2` |
|     68 | 3501 | `		 && (unsigned char)z[1] == 0x80 && (unsigned char)z[2] == 0xAF ){` |
|      3 | 3502 | `			z += 3;    /* NARROW NO-BREAK SPACE */` |
|      3 | 3503 | `			continue;` |
|      - | 3504 | `		}` |
|    146 | 3505 | `		if( zEnd - z >= 2 && (unsigned char)z[0] == 0xC2` |
|     70 | 3506 | `		 && (unsigned char)z[1] == 0xA0 ){` |
|      3 | 3507 | `			z += 2;    /* NO-BREAK SPACE */` |
|      3 | 3508 | `			continue;` |
|      - | 3509 | `		}` |
|    145 | 3510 | `		break;` |
|    ! 0 | 3511 | `	}` |
|    145 | 3512 | `	*pz = z;` |
|    145 | 3513 | `}` |
|      - | 3514 | `/*` |
|      - | 3515 | ` * What one run of the FORMAT scanner read, field by field.` |
|      - | 3516 | ` *` |
|      - | 3517 | ` * php's format parser starts every field UNSET and never consults the clock: the` |
|      - | 3518 | ` * struct below is what the scan itself put there, so a format that named no year` |
|      - | 3519 | `` * leaves `y` unset rather than this year's. The moment a DateTime wants is built`` |
|      - | 3520 | ` * from it afterwards (DtFfResolve), which is where the current instant finally` |
|      - | 3521 | ` * fills what the format never mentioned -- php's own timelib_fill_holes, run once` |
|      - | 3522 | ` * the scan is over rather than while it is going on.` |
|      - | 3523 | ` */` |
|      - | 3524 | `typedef struct dt_ff_res dt_ff_res;` |
|      - | 3525 | `struct dt_ff_res` |
|      - | 3526 | `{` |
|      - | 3527 | `	sxi64 y,mo,d,h,mi,s,us;   /* DT_UNSET == php's TIMELIB_UNSET */` |
|      - | 3528 | `	sxi32 iOff;` |
|      - | 3529 | `	int bLocal;               /* php's is_localtime -- a zone was READ */` |
|      - | 3530 | `	int iOffKind;             /* ...and php's zone_type, 0 when it meant nothing */` |
|      - | 3531 | `	const char *zName;        /* a static literal, as every zone name here is */` |
|      - | 3532 | `	int nName;` |
|      - | 3533 | `	int bWday;                /* php's relative.have_weekday_relative */` |
|      - | 3534 | `	sxi64 iWday;` |
|      - | 3535 | `	dt_ff_diag sDiag;` |
|      - | 3536 | `};` |
|    634 | 3537 | `static int DtFromFormat(const char *zFmt,int nFmt,const char *zIn,int nIn,` |
|      - | 3538 | `	dt_ff_res *pOut)` |
|      1 | 3539 | `{` |
|      - | 3540 | `	const char *zEnd,*zInEnd,*z;` |
|      - | 3541 | `	sxi64 v;` |
|    635 | 3542 | `	sxi64 y = DT_UNSET,mo = DT_UNSET,d = DT_UNSET;` |
|    635 | 3543 | `	sxi64 h = DT_UNSET,mi = DT_UNSET,s = DT_UNSET,us = DT_UNSET;` |
|    635 | 3544 | `	sxi64 uVal = 0;` |
|    635 | 3545 | `	int bPlus = 0,bLocal = 0;` |
|    635 | 3546 | `	int bWday = 0;` |
|    635 | 3547 | `	sxi64 iWday = 0;` |
|    635 | 3548 | `	int iOffKind = 0,nName = 0;` |
|    635 | 3549 | `	sxi32 iOffVal = 0;` |
|    635 | 3550 | `	const char *zName = 0;` |
|    635 | 3551 | `	const char *zErr = 0;` |
|      - | 3552 | `	const char *aWarnMsg[PH7_DT_MAX_WARN];` |
|      - | 3553 | `	int aWarnPos[PH7_DT_MAX_WARN];` |
|    635 | 3554 | `	int nWarn = 0;` |
|      - | 3555 | `	const char *aErrMsg[PH7_DT_MAX_ERR];` |
|      - | 3556 | `	int aErrPos[PH7_DT_MAX_ERR];` |
|    635 | 3557 | `	int nErr = 0,nErrKept = 0;` |
|    635 | 3558 | `	SyZero(pOut,sizeof(*pOut));` |
|      - | 3559 | `	/* Both strings end where php's C string ends: a NUL inside a format simply` |
|      - | 3560 | `	 * truncates it, and one inside the input ends the scan there. */` |
|    635 | 3561 | `	zEnd = &zFmt[DtCStrLen(zFmt,nFmt)];` |
|    635 | 3562 | `	zInEnd = &zIn[DtCStrLen(zIn,nIn)];` |
|    635 | 3563 | `	z = zIn;` |
|      - | 3564 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|      - | 3565 | `	{ int _p = (iPos),_k,_f = -1; \` |
|      - | 3566 | `	  nErr++; \` |
|      - | 3567 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|      - | 3568 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|      - | 3569 | `	  else if( nErrKept < PH7_DT_MAX_ERR ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|      - | 3570 | `/* php's two RESET specifiers act where they stand rather than at the end of the` |
|      - | 3571 | `` * scan: `!` puts every field at its 1970 default whatever the format already`` |
|      - | 3572 | `` * read, `\|` only fills what nothing has read yet, and a specifier after either`` |
|      - | 3573 | ` * one overwrites what it left. */` |
|      - | 3574 | `#define DT_FF_RESET(bUnsetOnly) \` |
|      - | 3575 | `	{ int _u = (bUnsetOnly); \` |
|      - | 3576 | `	  if( !_u \|\| y  == DT_UNSET ){ y  = 1970; } \` |
|      - | 3577 | `	  if( !_u \|\| mo == DT_UNSET ){ mo = 1; } \` |
|      - | 3578 | `	  if( !_u \|\| d  == DT_UNSET ){ d  = 1; } \` |
|      - | 3579 | `	  if( !_u \|\| h  == DT_UNSET ){ h  = 0; } \` |
|      - | 3580 | `	  if( !_u \|\| mi == DT_UNSET ){ mi = 0; } \` |
|      - | 3581 | `	  if( !_u \|\| s  == DT_UNSET ){ s  = 0; } \` |
|      - | 3582 | `	  if( !_u \|\| us == DT_UNSET ){ us = 0; } }` |
|      - | 3583 | `	/* The scan runs while BOTH strings still have something in them: php's` |
|      - | 3584 | `	 * format loop ends the moment the input does, and what is left of the` |
|      - | 3585 | `	 * format is judged afterwards rather than refused here. */` |
|   2227 | 3586 | `	while( zFmt < zEnd && z < zInEnd ){` |
|   1593 | 3587 | `		char c = zFmt[0];` |
|      - | 3588 | `		/* every refusal below reports the byte the specifier STARTED on, not` |
|      - | 3589 | `		 * wherever the reading of it gave up */` |
|   1593 | 3590 | `		int iBegin = (int)(z - zIn);` |
|   1593 | 3591 | `		zFmt++;` |
|   1593 | 3592 | `		zErr = 0;` |
|   1593 | 3593 | `		if( c == '!' ){` |
|     41 | 3594 | `			DT_FF_RESET(0);` |
|     41 | 3595 | `			continue;` |
|      - | 3596 | `		}` |
|   1553 | 3597 | `		if( c == '\|' ){` |
|     71 | 3598 | `			DT_FF_RESET(1);` |
|     71 | 3599 | `			continue;` |
|      - | 3600 | `		}` |
|   1483 | 3601 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|      - | 3602 | `/* php asks of every digit field, BEFORE reading it, whether the cursor is on a` |
|      - | 3603 | ` * digit at all -- and merely says so: the reader that follows hunts for its` |
|      - | 3604 | `` * digits regardless, so `x5` is the day 5 with one refusal behind it. */`` |
|      - | 3605 | `#define DT_FF_CHECKNUM \` |
|      - | 3606 | `	if( !SyisDigit(z[0]) ){ DT_FF_LOGERR(iBegin,"Unexpected data found."); }` |
|      - | 3607 | `#define DT_FF_CHECKSIGNED \` |
|      - | 3608 | `	if( !SyisDigit(z[0]) && z[0] != '+' && z[0] != '-' ){ \` |
|      - | 3609 | `		DT_FF_LOGERR(iBegin,"Unexpected data found."); }` |
|   1475 | 3610 | `		switch( c ){` |
|     60 | 3611 | `		case 'd': case 'j':` |
|    125 | 3612 | `			DT_FF_CHECKNUM;` |
|    121 | 3613 | `			if( DtFfGetNr(&z,zInEnd,2,&d) < 0 ){` |
|      3 | 3614 | `				DT_FF_LOGERR(iBegin,"A two digit day could not be found");` |
|      3 | 3615 | `				d = DT_UNSET;` |
|      1 | 3616 | `			}` |
|    121 | 3617 | `			break;` |
|     23 | 3618 | `		case 'D': case 'l':` |
|      - | 3619 | `			/* php's textual day is a RELATIVE weekday, not decoration: it moves` |
|      - | 3620 | `			 * the date it was read beside, forward to that weekday and keeping a` |
|      - | 3621 | `			 * day that already matches. Both spellings read the same table --` |
|      - | 3622 | `			 * the three-letter and the full name are one rule there. */` |
|     47 | 3623 | `			if( DtFfRelunit(&z,zInEnd,&iWday) ){` |
|     45 | 3624 | `				bWday = 1;` |
|     23 | 3625 | `			}else{` |
|      3 | 3626 | `				zErr = "A textual day could not be found";` |
|      - | 3627 | `			}` |
|     47 | 3628 | `			break;` |
|     21 | 3629 | `		case 'z':` |
|      - | 3630 | `			/* php's DAY OF YEAR is a whole date rather than a field: it needs a` |
|      - | 3631 | `			 * year already read, puts the month back at January and the day at` |
|      - | 3632 | `			 * the count, and normalizes the vector where it stands. */` |
|     46 | 3633 | `			DT_FF_CHECKNUM;` |
|     43 | 3634 | `			if( y == DT_UNSET ){` |
|      9 | 3635 | `				DT_FF_LOGERR(iBegin,"A 'day of year' can only come after a year has been found");` |
|      4 | 3636 | `			}` |
|     43 | 3637 | `			if( DtFfGetNr(&z,zInEnd,3,&v) < 0 ){` |
|      5 | 3638 | `				DT_FF_LOGERR(iBegin,"A three digit day-of-year could not be found");` |
|      5 | 3639 | `				break;` |
|      - | 3640 | `			}` |
|     39 | 3641 | `			if( y != DT_UNSET ){` |
|     33 | 3642 | `				mo = 1;` |
|     33 | 3643 | `				d = v + 1;` |
|     33 | 3644 | `				DtFfNormalize(&y,&mo,&d,&h,&mi,&s,&us);` |
|     16 | 3645 | `			}` |
|     39 | 3646 | `			break;` |
|     10 | 3647 | `		case 'x': case 'X':{` |
|      - | 3648 | `			/* the EXPANDED year: a sign and up to nineteen digits, and the year` |
|      - | 3649 | `			 * php takes from a run it could not read is zero rather than none. */` |
|     21 | 3650 | `			const char *zNrErr = 0;` |
|     22 | 3651 | `			DT_FF_CHECKSIGNED;` |
|     21 | 3652 | `			DtFfGetSignedNr(&z,zInEnd,19,&y,&zNrErr);` |
|     21 | 3653 | `			if( zNrErr ){` |
|      5 | 3654 | `				DT_FF_LOGERR(0,zNrErr);` |
|      2 | 3655 | `			}` |
|     21 | 3656 | `			break;` |
|      - | 3657 | `				 }` |
|      5 | 3658 | `		case 'S':` |
|      - | 3659 | `			/* the ordinal suffix, which php declines to look at when the cursor` |
|      - | 3660 | `			 * is on a blank and otherwise takes in either case */` |
|     10 | 3661 | `			if( !SyisSpace((unsigned char)z[0]) && zInEnd-z >= 2` |
|     11 | 3662 | `			 && (SyStrnicmp(z,"st",2) == 0 \|\| SyStrnicmp(z,"nd",2) == 0` |
|      5 | 3663 | `			  \|\| SyStrnicmp(z,"rd",2) == 0 \|\| SyStrnicmp(z,"th",2) == 0) ){` |
|      7 | 3664 | `				z += 2;` |
|      3 | 3665 | `			}` |
|     11 | 3666 | `			break;` |
|     49 | 3667 | `		case 'm': case 'n':` |
|     99 | 3668 | `			DT_FF_CHECKNUM;` |
|     99 | 3669 | `			if( DtFfGetNr(&z,zInEnd,2,&mo) < 0 ){` |
|    ! 0 | 3670 | `				DT_FF_LOGERR(iBegin,"A two digit month could not be found");` |
|    ! 0 | 3671 | `				mo = DT_UNSET;` |
|    ! 0 | 3672 | `			}` |
|     99 | 3673 | `			break;` |
|     19 | 3674 | `		case 'M': case 'F':{` |
|      - | 3675 | `			int k;` |
|     39 | 3676 | `			k = DtFfMonth(&z,zInEnd);` |
|     39 | 3677 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     39 | 3678 | `			break;` |
|      - | 3679 | `				 }` |
|      5 | 3680 | `		case 'y':` |
|     11 | 3681 | `			DT_FF_CHECKNUM;` |
|     11 | 3682 | `			if( DtFfGetNr(&z,zInEnd,2,&y) < 0 ){` |
|    ! 0 | 3683 | `				DT_FF_LOGERR(iBegin,"A two digit year could not be found");` |
|    ! 0 | 3684 | `				y = DT_UNSET;` |
|     11 | 3685 | `			}else if( y < 100 ){` |
|      - | 3686 | `				/* php's two-digit century, which cuts at seventy */` |
|     11 | 3687 | `				y += (y < 70) ? 2000 : 1900;` |
|      5 | 3688 | `			}` |
|     11 | 3689 | `			break;` |
|     91 | 3690 | `		case 'Y':` |
|    189 | 3691 | `			DT_FF_CHECKNUM;` |
|    183 | 3692 | `			if( DtFfGetNr(&z,zInEnd,4,&y) < 0 ){` |
|     11 | 3693 | `				DT_FF_LOGERR(iBegin,"A four digit year could not be found");` |
|     11 | 3694 | `				y = DT_UNSET;` |
|      5 | 3695 | `			}` |
|    183 | 3696 | `			break;` |
|     31 | 3697 | `		case 'H': case 'G':` |
|     63 | 3698 | `			DT_FF_CHECKNUM;` |
|     63 | 3699 | `			if( DtFfGetNr(&z,zInEnd,2,&h) < 0 ){` |
|    ! 0 | 3700 | `				DT_FF_LOGERR(iBegin,"A two digit hour could not be found");` |
|    ! 0 | 3701 | `				h = DT_UNSET;` |
|    ! 0 | 3702 | `			}` |
|     63 | 3703 | `			break;` |
|     16 | 3704 | `		case 'h': case 'g':` |
|     33 | 3705 | `			DT_FF_CHECKNUM;` |
|     33 | 3706 | `			if( DtFfGetNr(&z,zInEnd,2,&h) < 0 ){` |
|    ! 0 | 3707 | `				DT_FF_LOGERR(iBegin,"A two digit hour could not be found");` |
|    ! 0 | 3708 | `				h = DT_UNSET;` |
|     33 | 3709 | `			}else if( h > 12 ){` |
|      - | 3710 | `				/* the twelve-hour spellings refuse a bigger one -- and keep it */` |
|      9 | 3711 | `				DT_FF_LOGERR(iBegin,"Hour cannot be higher than 12");` |
|      4 | 3712 | `			}` |
|     33 | 3713 | `			break;` |
|     52 | 3714 | `		case 'i': case 's':{` |
|      - | 3715 | `			/* the minute and the second are php's only EXACTLY two-digit` |
|      - | 3716 | `			 * fields: a lone digit is no minute there, however many follow` |
|      - | 3717 | `			 * it -- and a reading that fails leaves whatever was read before */` |
|    105 | 3718 | `			sxi64 t = 0;` |
|    105 | 3719 | `			DT_FF_CHECKNUM;` |
|    105 | 3720 | `			if( DtFfGetNr(&z,zInEnd,2,&t) != 2 ){` |
|      9 | 3721 | `				DT_FF_LOGERR(iBegin,c == 'i'` |
|      - | 3722 | `					? "A two digit minute could not be found"` |
|      1 | 3723 | `					: "A two digit second could not be found");` |
|    101 | 3724 | `			}else if( c == 'i' ){` |
|     57 | 3725 | `				mi = t;` |
|     29 | 3726 | `			}else{` |
|     41 | 3727 | `				s = t;` |
|      - | 3728 | `			}` |
|    105 | 3729 | `			break;` |
|      - | 3730 | `				 }` |
|     23 | 3731 | `		case 'u': case 'v':{` |
|      - | 3732 | `			/* the fraction is scaled by what the READER walked, not by the` |
|      - | 3733 | `			 * digits it found: the bytes it stepped over hunting for them count` |
|      - | 3734 | ``			 * against the width too, so `x1` under `u` is a hundredth */`` |
|     47 | 3735 | `			const char *zStart = z;` |
|     47 | 3736 | `			int nMax = (c == 'u') ? 6 : 3;` |
|     61 | 3737 | `			DT_FF_CHECKNUM;` |
|     47 | 3738 | `			if( DtFfGetNr(&z,zInEnd,nMax,&v) < 0 ){` |
|      3 | 3739 | `				DT_FF_LOGERR(iBegin,c == 'u'` |
|      - | 3740 | `					? "A six digit microsecond could not be found"` |
|      1 | 3741 | `					: "A three digit millisecond could not be found");` |
|      3 | 3742 | `				break;` |
|      - | 3743 | `			}` |
|      - | 3744 | `			{` |
|      - | 3745 | `				/* ...and BOTH spellings land on the same scale, because php` |
|      - | 3746 | `				 * multiplies the millisecond by a thousand after dividing it by` |
|      - | 3747 | `				 * the width: what either one answers is the digits it read` |
|      - | 3748 | `				 * times ten to the six-minus-bytes-walked, and a reader that` |
|      - | 3749 | `				 * walked more than six bytes answers a truncated fraction. */` |
|     45 | 3750 | `				int k = 6 - (int)(z - zStart);` |
|    159 | 3751 | `				while( k > 0 ){ v *= 10; k--; }` |
|     55 | 3752 | `				while( k < 0 ){ v /= 10; k++; }` |
|     45 | 3753 | `				us = v;` |
|      - | 3754 | `			}` |
|     45 | 3755 | `			break;` |
|      - | 3756 | `				 }` |
|     19 | 3757 | `		case 'a': case 'A':{` |
|     39 | 3758 | `			sxi64 iAdj = 0;` |
|     39 | 3759 | `			if( h == DT_UNSET ){` |
|      9 | 3760 | `				DT_FF_LOGERR(iBegin,"Meridian can only come after an hour has been found");` |
|      4 | 3761 | `			}` |
|     39 | 3762 | `			if( !DtFfMeridian(&z,zInEnd,h,&iAdj) ){` |
|      7 | 3763 | `				zErr = "A meridian could not be found";` |
|     36 | 3764 | `			}else if( h != DT_UNSET ){` |
|     29 | 3765 | `				h += iAdj;` |
|     14 | 3766 | `			}` |
|     39 | 3767 | `			break;` |
|      - | 3768 | `				 }` |
|     12 | 3769 | `		case 'U':{` |
|      - | 3770 | `			/* php's epoch seconds are not a field but a whole MOMENT: it spreads` |
|      - | 3771 | `			 * the timestamp back over y/m/d/h/i/s at UTC right here, so a` |
|      - | 3772 | ``			 * meridian behind one has an hour to move and a `Y` behind one`` |
|      - | 3773 | `			 * overwrites the year it just wrote. The microseconds are the one` |
|      - | 3774 | `			 * part it does not touch. */` |
|     25 | 3775 | `			const char *zNrErr = 0;` |
|      - | 3776 | `			Sytm sTm;` |
|     28 | 3777 | `			DT_FF_CHECKSIGNED;` |
|     25 | 3778 | `			DtFfGetSignedNr(&z,zInEnd,24,&uVal,&zNrErr);` |
|     25 | 3779 | `			if( zNrErr ){` |
|      - | 3780 | `				/* php reports these at position 0 and takes the zero anyway */` |
|      5 | 3781 | `				DT_FF_LOGERR(0,zNrErr);` |
|      2 | 3782 | `			}` |
|     25 | 3783 | `			DtFillSytm(uVal,0,0,&sTm);` |
|     25 | 3784 | `			y = sTm.tm_year; mo = sTm.tm_mon + 1; d = sTm.tm_mday;` |
|     25 | 3785 | `			h = sTm.tm_hour; mi = sTm.tm_min; s = sTm.tm_sec;` |
|     25 | 3786 | `			bLocal = 1;` |
|     25 | 3787 | `			iOffKind = DT_ZONE_OFFSET;` |
|     25 | 3788 | `			iOffVal = 0;` |
|     25 | 3789 | `			zName = 0;` |
|     25 | 3790 | `			nName = 0;` |
|     25 | 3791 | `			break;` |
|      - | 3792 | `				 }` |
|     71 | 3793 | `		case 'e': case 'T': case 'P': case 'p': case 'O':` |
|      - | 3794 | `			/* php's five zone specifiers are ONE rule, and it is the whole of` |
|      - | 3795 | `			 * timelib_parse_zone rather than the shape each letter is named` |
|      - | 3796 | ``			 * after: `O` reads `UTC` and `e` reads `+02:00`. The zone is LOCAL`` |
|      - | 3797 | `			 * from here whatever the answer -- only the KIND is left at zero` |
|      - | 3798 | `			 * when the name meant nothing. */` |
|    143 | 3799 | `			bLocal = 1;` |
|    143 | 3800 | `			if( !DtFfZone(&z,zInEnd,&iOffKind,&iOffVal,&zName,&nName) ){` |
|     43 | 3801 | `				zErr = "The timezone could not be found in the database";` |
|     21 | 3802 | `			}` |
|    143 | 3803 | `			break;` |
|      1 | 3804 | `		case '?':` |
|      3 | 3805 | `			z++;` |
|      3 | 3806 | `			break;` |
|      4 | 3807 | `		case '*':` |
|      - | 3808 | `			/* php's "skip to a separator": one byte goes whatever it is, and the` |
|      - | 3809 | ``			 * run after it stops at a blank, a digit or one of `.,:;/-`. The`` |
|      - | 3810 | `			 * parens are NOT in that set, though every other rule here treats` |
|      - | 3811 | `			 * them as separators. */` |
|      9 | 3812 | `			z++;` |
|     35 | 3813 | `			while( z < zInEnd && z[0] != ' ' && z[0] != '\t' && z[0] != '.'` |
|     22 | 3814 | `			 && z[0] != ',' && z[0] != ':' && z[0] != ';' && z[0] != '/'` |
|     38 | 3815 | `			 && z[0] != '-' && !SyisDigit(z[0]) ){` |
|     23 | 3816 | `				z++;` |
|      1 | 3817 | `			}` |
|      9 | 3818 | `			break;` |
|      4 | 3819 | `		case '#':` |
|      9 | 3820 | `			if( DtFfIsSep((unsigned char)z[0]) ){` |
|      7 | 3821 | `				z++;` |
|      4 | 3822 | `			}else{` |
|      3 | 3823 | `				zErr = "The separation symbol ([;:/.,-]) could not be found";` |
|      - | 3824 | `			}` |
|      9 | 3825 | `			break;` |
|      4 | 3826 | `		case '\\':` |
|      - | 3827 | `			/* the escape takes the NEXT format byte literally, and refuses on` |
|      - | 3828 | `			 * its own account when the format ends before there is one */` |
|      9 | 3829 | `			if( zFmt >= zEnd ){` |
|      3 | 3830 | `				zErr = "Escaped character expected";` |
|      3 | 3831 | `				break;` |
|      - | 3832 | `			}` |
|      7 | 3833 | `			if( z[0] == zFmt[0] ){` |
|      5 | 3834 | `				z++;` |
|      3 | 3835 | `			}else{` |
|      3 | 3836 | `				zErr = "The escaped character could not be found";` |
|      - | 3837 | `			}` |
|      7 | 3838 | `			zFmt++;` |
|      7 | 3839 | `			break;` |
|    136 | 3840 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|      - | 3841 | `		case '(' : case ')':` |
|      - | 3842 | `			/* a separator in the format wants exactly that byte; a mismatch is` |
|      - | 3843 | `			 * ONE refusal, and the input byte stays where it is */` |
|    273 | 3844 | `			if( z[0] == c ){` |
|    265 | 3845 | `				z++;` |
|    133 | 3846 | `			}else{` |
|      9 | 3847 | `				zErr = "The separation symbol could not be found";` |
|      - | 3848 | `			}` |
|    273 | 3849 | `			break;` |
|     72 | 3850 | `		case ' ':` |
|    145 | 3851 | `			DtFfEatSpaces(&z,zInEnd);` |
|    145 | 3852 | `			break;` |
|      9 | 3853 | `		default:` |
|      - | 3854 | `			/* any other format byte must match the input verbatim -- and php` |
|      - | 3855 | `			 * steps over the input byte either way, so a mismatch costs one` |
|      - | 3856 | `			 * refusal and the two strings carry on in step */` |
|     19 | 3857 | `			if( z[0] != c ){` |
|     19 | 3858 | `				DT_FF_LOGERR(iBegin,"The format separator does not match");` |
|      7 | 3859 | `			}` |
|     19 | 3860 | `			z++;` |
|     18 | 3861 | `			break;` |
|      - | 3862 | `		}` |
|   1475 | 3863 | `		if( zErr ){` |
|      - | 3864 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|     69 | 3865 | `			DT_FF_LOGERR(iBegin,zErr);` |
|     33 | 3866 | `		}` |
|      1 | 3867 | `	}` |
|    635 | 3868 | `	if( z < zInEnd ){` |
|     81 | 3869 | `		if( bPlus ){` |
|      - | 3870 | `			/* '+' downgrades trailing data to a warning */` |
|      9 | 3871 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|      9 | 3872 | `			aWarnMsg[nWarn] = "Trailing data";` |
|      9 | 3873 | `			nWarn++;` |
|      5 | 3874 | `		}else{` |
|     87 | 3875 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|      - | 3876 | `		}` |
|     40 | 3877 | `	}` |
|      - | 3878 | `	/* ...and what is left of a FORMAT the input ran out under. The two reset` |
|      - | 3879 | ``	 * specifiers and `+` need no input and are allowed to stand there; the`` |
|      - | 3880 | `	 * first specifier that does want input is one refusal, and the rest of the` |
|      - | 3881 | `	 * format is never looked at. */` |
|   1287 | 3882 | `	while( zFmt < zEnd ){` |
|    683 | 3883 | `		char c = zFmt[0];` |
|    683 | 3884 | `		zFmt++;` |
|    683 | 3885 | `		if( c == '!' ){` |
|     15 | 3886 | `			DT_FF_RESET(0);` |
|    676 | 3887 | `		}else if( c == '\|' ){` |
|    635 | 3888 | `			DT_FF_RESET(1);` |
|    352 | 3889 | `		}else if( c != '+' ){` |
|     41 | 3890 | `			DT_FF_LOGERR((int)(z - zIn),"Not enough data available to satisfy format");` |
|     31 | 3891 | `			break;` |
|      - | 3892 | `		}` |
|      1 | 3893 | `	}` |
|      - | 3894 | `	/* php's own clean-up: naming ANY part of the clock puts the rest of it at` |
|      - | 3895 | `	 * zero, so a format that read only the minute is that minute past midnight` |
|      - | 3896 | `	 * rather than past the current hour. */` |
|    635 | 3897 | `	if( h != DT_UNSET \|\| mi != DT_UNSET \|\| s != DT_UNSET \|\| us != DT_UNSET ){` |
|    437 | 3898 | `		if( h == DT_UNSET ){ h = 0; }` |
|    437 | 3899 | `		if( mi == DT_UNSET ){ mi = 0; }` |
|    437 | 3900 | `		if( s == DT_UNSET ){ s = 0; }` |
|    437 | 3901 | `		if( us == DT_UNSET ){ us = 0; }` |
|    218 | 3902 | `	}` |
|      - | 3903 | `	/* ...and the two validity WARNINGS, each asked only of a whole component` |
|      - | 3904 | `	 * the scan actually filled, at wherever in the input the scan stopped. */` |
|    634 | 3905 | `	if( h != DT_UNSET && mi != DT_UNSET && s != DT_UNSET` |
|    437 | 3906 | `	 && (h < 0 \|\| h > 23 \|\| mi < 0 \|\| mi > 59 \|\| s < 0 \|\| s > 59) ){` |
|     23 | 3907 | `		if( nWarn < PH7_DT_MAX_WARN ){` |
|     23 | 3908 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     23 | 3909 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|     23 | 3910 | `			nWarn++;` |
|     11 | 3911 | `		}` |
|     11 | 3912 | `	}` |
|    634 | 3913 | `	if( y != DT_UNSET && mo != DT_UNSET && d != DT_UNSET` |
|    450 | 3914 | `	 && (mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo)) ){` |
|     23 | 3915 | `		if( nWarn < PH7_DT_MAX_WARN ){` |
|     23 | 3916 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     23 | 3917 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|     23 | 3918 | `			nWarn++;` |
|     11 | 3919 | `		}` |
|     11 | 3920 | `	}` |
|    635 | 3921 | `	pOut->y = y; pOut->mo = mo; pOut->d = d;` |
|    635 | 3922 | `	pOut->h = h; pOut->mi = mi; pOut->s = s; pOut->us = us;` |
|    635 | 3923 | `	pOut->iOff = iOffVal;` |
|    635 | 3924 | `	pOut->bLocal = bLocal;` |
|    635 | 3925 | `	pOut->iOffKind = iOffKind;` |
|    635 | 3926 | `	pOut->zName = zName;` |
|    635 | 3927 | `	pOut->nName = nName;` |
|    635 | 3928 | `	pOut->bWday = bWday;` |
|    635 | 3929 | `	pOut->iWday = iWday;` |
|    635 | 3930 | `	DtFfDiag(&pOut->sDiag,nErr,nErrKept,aErrPos,aErrMsg,nWarn,aWarnPos,aWarnMsg);` |
|    635 | 3931 | `	return nErr > 0 ? -1 : 0;` |
|      - | 3932 | `#undef DT_FF_CHECKSIGNED` |
|      - | 3933 | `#undef DT_FF_CHECKNUM` |
|      - | 3934 | `#undef DT_FF_RESET` |
|      - | 3935 | `#undef DT_FF_LOGERR` |
|      1 | 3936 | `}` |
|      - | 3937 | `/*` |
|      - | 3938 | ` * php's timelib_fill_holes and timelib_update_ts, the step between the scan` |
|      - | 3939 | ` * above and the moment a DateTime carries.` |
|      - | 3940 | ` *` |
|      - | 3941 | ` * Everything the format never named comes from the current instant -- in the` |
|      - | 3942 | `` * zone the call was given, because php builds its `now` there -- and the`` |
|      - | 3943 | ` * relative WEEKDAY a textual day left behind moves the resulting date forward` |
|      - | 3944 | ` * to that weekday, a day that already matches counting as a match.` |
|      - | 3945 | ` */` |
|    348 | 3946 | `static sxi64 DtFfResolve(const dt_ff_res *pRes,sxi64 iNow,int iNowUs,` |
|      - | 3947 | `	sxi32 iDefOff,int *piUs)` |
|      1 | 3948 | `{` |
|    349 | 3949 | `	sxi64 y = pRes->y,mo = pRes->mo,d = pRes->d;` |
|    349 | 3950 | `	sxi64 h = pRes->h,mi = pRes->mi,s = pRes->s,us = pRes->us;` |
|    349 | 3951 | `	sxi64 iLocal = iNow + iDefOff;` |
|    349 | 3952 | `	sxi64 days = DtFloorDiv(iLocal,86400);` |
|    349 | 3953 | `	sxi64 secs = iLocal - days*86400;` |
|      - | 3954 | `	sxi64 ny;` |
|      - | 3955 | `	int nmo,nd;` |
|    349 | 3956 | `	DtCivilFromDays(days,&ny,&nmo,&nd);` |
|    349 | 3957 | `	if( us == DT_UNSET ){` |
|      - | 3958 | `		/* php reads the microseconds off the clock only for a format that read` |
|      - | 3959 | `		 * no part of the moment at all. */` |
|     91 | 3960 | `		us = (y != DT_UNSET \|\| mo != DT_UNSET \|\| d != DT_UNSET` |
|     55 | 3961 | `		   \|\| h != DT_UNSET \|\| mi != DT_UNSET \|\| s != DT_UNSET) ? 0 : iNowUs;` |
|     35 | 3962 | `	}` |
|    349 | 3963 | `	if( y == DT_UNSET ){ y = ny; }` |
|    349 | 3964 | `	if( mo == DT_UNSET ){ mo = nmo; }` |
|    349 | 3965 | `	if( d == DT_UNSET ){ d = nd; }` |
|    349 | 3966 | `	if( h == DT_UNSET ){ h = secs / 3600; }` |
|    349 | 3967 | `	if( mi == DT_UNSET ){ mi = (secs / 60) % 60; }` |
|    349 | 3968 | `	if( s == DT_UNSET ){ s = secs % 60; }` |
|      - | 3969 | `	/* php normalizes the filled vector BEFORE it hunts for a weekday and again` |
|      - | 3970 | `	 * afterwards, and its month carry is a CALENDAR one -- the fortieth month` |
|      - | 3971 | `	 * of 1970 is April 1973, not forty thirty-day steps from January. */` |
|    349 | 3972 | `	DtFfNormalize(&y,&mo,&d,&h,&mi,&s,&us);` |
|    349 | 3973 | `	if( pRes->bWday ){` |
|      - | 3974 | `		/* php's forward hunt, and it is a DIFFERENCE rather than a remainder:` |
|      - | 3975 | ``		 * a weekday the relative-unit table answers past six -- `week` is 7 --`` |
|      - | 3976 | `		 * moves the date by that much more. */` |
|     33 | 3977 | `		sxi64 iDays = DtDaysFromCivil(y,(int)mo,1) + (d - 1);` |
|     33 | 3978 | `		sxi64 iDiff = pRes->iWday - DtDowOf(iDays);` |
|     33 | 3979 | `		if( iDiff < 0 ){ iDiff += 7; }` |
|     33 | 3980 | `		d += iDiff;` |
|     33 | 3981 | `		DtFfNormalize(&y,&mo,&d,&h,&mi,&s,&us);` |
|     16 | 3982 | `	}` |
|    349 | 3983 | `	*piUs = (int)us;` |
|    523 | 3984 | `	return DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,` |
|    348 | 3985 | `		pRes->iOffKind != 0 ? pRes->iOff : iDefOff);` |
|      1 | 3986 | `}` |
|      - | 3987 | `/*` |
|      - | 3988 | ` * ---------------------------------------------------------------------------` |
|      - | 3989 | ` * DateTimeZone, DateTime and DateTimeImmutable, declared from C.` |
|      - | 3990 | ` *` |
|      - | 3991 | `` * These three used to be embedded PHP over nine global `__dt_*` thunks, with a`` |
|      - | 3992 | `` * private `trait __DtCoreT` holding the state and the shared half of both date`` |
|      - | 3993 | ` * classes. Every operation therefore crossed C -> PHP -> C and marshalled its` |
|      - | 3994 | ` * answer through a throwaway PHP array. The bodies below call the same routines` |
|      - | 3995 | ` * directly; the thunks, the trait and their chunk classes are gone.` |
|      - | 3996 | ` *` |
|      - | 3997 | `` * The instance state is unchanged, so `clone`, `serialize` and `var_dump` see what`` |
|      - | 3998 | `` * they always saw (minus the `__DtCoreT` declaring-class name): four private slots`` |
|      - | 3999 | ` * on each date class, two on DateTimeZone. Native traits do not exist, so the` |
|      - | 4000 | `` * shared method table is simply installed on both classes -- which is what `use`` |
|      - | 4001 | `` * __DtCoreT` did anyway.`` |
|      - | 4002 | ` * ---------------------------------------------------------------------------` |
|      - | 4003 | ` */` |
|      - | 4004 | `#define DT_TS    "__dtTs"` |
|      - | 4005 | `#define DT_OFF   "__dtOff"` |
|      - | 4006 | `#define DT_NAME  "__dtName"` |
|      - | 4007 | `#define DT_US    "__dtUs"` |
|      - | 4008 | `#define DTZ_OFF  "__dtzOff"` |
|      - | 4009 | `#define DTZ_NAME "__dtzName"` |
|      - | 4010 | `/*` |
|      - | 4011 | ` * php's timezone_type -- 1 = a fixed UTC OFFSET, 2 = an ABBREVIATION, 3 = an` |
|      - | 4012 | ` * IDENTIFIER -- STORED beside the name rather than read back off it, because the` |
|      - | 4013 | ``  * name does not carry it: `new DateTimeZone('utc')` and `new DateTimeZone('UTC')` `` |
|      - | 4014 | ` * are both named "UTC" there and are an abbreviation and an identifier` |
|      - | 4015 | ` * respectively, which is what makes them refuse to compare with each other. The` |
|      - | 4016 | ` * date objects keep their own copy (DT_ZKIND) for the same reason: php presents a` |
|      - | 4017 | ` * DateTime built with the lowercase zone as type 2.` |
|      - | 4018 | ` *` |
|      - | 4019 | ` * DtZoneTypeOf() remains the rule for a name that arrives with NO kind -- php's` |
|      - | 4020 | ` * own __unserialize re-derives it that way, which is why a serialized type-2 "UTC"` |
|      - | 4021 | ` * comes back as type 3 in both engines.` |
|      - | 4022 | ` */` |
|      - | 4023 | `#define DTZ_KIND "__dtzKind"` |
|      - | 4024 | `#define DT_ZKIND "__dtZKind"` |
|      - | 4025 | `/*` |
|      - | 4026 | ` * Has this object been CONSTRUCTED?` |
|      - | 4027 | ` *` |
|      - | 4028 | ` * php keeps its date state in a C struct hanging off the object and allocates it` |
|      - | 4029 | ` * in the constructor, so an object that never ran one -- what` |
|      - | 4030 | `` * `newInstanceWithoutConstructor()` answers, and what a subclass whose own`` |
|      - | 4031 | `` * constructor forgets `parent::__construct()` IS -- has no state at all, and every`` |
|      - | 4032 | `` * door raises `DateObjectError` rather than reading it. PHL's state lives in`` |
|      - | 4033 | ` * ordinary (hidden) slots, which are there from instantiation and hold their` |
|      - | 4034 | ` * declared defaults, so such an object silently WAS 1970-01-01 UTC.` |
|      - | 4035 | ` *` |
|      - | 4036 | ` * This is that struct's presence, as the one thing a slot can carry: zero until` |
|      - | 4037 | ` * some constructor -- or one of the C factories, which build a complete object` |
|      - | 4038 | ` * without running one -- says otherwise. Every class in the family declares it,` |
|      - | 4039 | ` * every method reaches it through DtThis(), and a clone inherits it the way php's` |
|      - | 4040 | ` * cloned struct does.` |
|      - | 4041 | ` */` |
|      - | 4042 | `#define DT_INIT  "__dtInit"` |
|      - | 4043 | `/* The kind the script DEFAULT zone has, and it is not the name's own rule:` |
|      - | 4044 | ` * date_default_timezone_set() takes a tz-database IDENTIFIER and nothing else, so` |
|      - | 4045 | `` * php reports a date built under a `GMT` default as type 3 while`` |
|      - | 4046 | `` * `new DateTimeZone('GMT')` -- the same three letters spelled as a zone -- is the`` |
|      - | 4047 | ` * abbreviation, type 2. */` |
|      - | 4048 | `#define DT_ZONE_DEFAULT_KIND DT_ZONE_ID` |
|      - | 4049 | `/*` |
|      - | 4050 | ` * ---------------------------------------------------------------------------` |
|      - | 4051 | ` * A DateInterval's MICROSECONDS.` |
|      - | 4052 | ` *` |
|      - | 4053 | ` * php stores them as an int64 COUNT (timelib_rel_time.us) and shows that count` |
|      - | 4054 | ` * divided by a million, so the float is a rendering and the integer is the` |
|      - | 4055 | `` * value: `$i->f = 0.1234567` reads back 0.123456 because the write truncated to`` |
|      - | 4056 | `` * 123456 microseconds, and `f` is what diff() fills, what add()/sub() move the`` |
|      - | 4057 | ` * clock by, and what format()'s %f prints.` |
|      - | 4058 | ` *` |
|      - | 4059 | `` * PHL's `f` is a real property slot a script reads directly, so the count lives`` |
|      - | 4060 | ` * beside it in a hidden one. The two are written together by every door that` |
|      - | 4061 | ` * owns the value (the write handler, diff, the constructors); a write that` |
|      - | 4062 | ` * arrives from somewhere else — unserialize's raw property store, or one of the` |
|      - | 4063 | `` * §7.4 shapes php answers with a temporary — leaves only `f` behind, so the`` |
|      - | 4064 | ` * count is trusted only while it still RENDERS to the float on show, and is` |
|      - | 4065 | ` * re-derived from the float when it does not.` |
|      - | 4066 | ` * ---------------------------------------------------------------------------` |
|      - | 4067 | ` */` |
|      - | 4068 | `#define DT_IV_US "__ivUs"` |
|      - | 4069 | ``/* php's conversion of the `f` property to its stored count, cast contract and`` |
|      - | 4070 | ` * all: it TRUNCATES toward zero, WRAPS what no int64 can hold, and answers 0 for` |
|      - | 4071 | ` * a NaN or an infinity. */` |
|    508 | 4072 | `static sxi64 DtIvUsecOfReal(double r)` |
|      2 | 4073 | `{` |
|    510 | 4074 | `	return PH7_RealToInt64(r * 1000000.0);` |
|      2 | 4075 | `}` |
|      - | 4076 | `/* The interval's microseconds. */` |
|    964 | 4077 | `static sxi64 DtIvUsec(ph7_class_instance *pIv)` |
|      2 | 4078 | `{` |
|    966 | 4079 | `	ph7_value *pF = PH7_NativeAttr(pIv,"f");` |
|    966 | 4080 | `	sxi64 us = PH7_NativeAttrInt(pIv,DT_IV_US);` |
|    966 | 4081 | `	double r = 0.0;` |
|    966 | 4082 | `	if( pF && (pF->iFlags & MEMOBJ_REAL) ){` |
|    966 | 4083 | `		r = (double)pF->rVal;` |
|    482 | 4084 | `	}else if( pF && (pF->iFlags & MEMOBJ_INT) ){` |
|    ! 0 | 4085 | `		r = (double)pF->x.iVal;` |
|    ! 0 | 4086 | `	}` |
|    966 | 4087 | `	if( (double)us / 1000000.0 == r ){` |
|    966 | 4088 | ``		return us;   /* the count `f` was rendered from: exact past 2^53, where the float is not */`` |
|      - | 4089 | `	}` |
|    ! 0 | 4090 | `	return DtIvUsecOfReal(r);` |
|    484 | 4091 | `}` |
|      - | 4092 | `/* Store a microsecond count and the float php shows for it -- the two halves of` |
|      - | 4093 | ` * the same value, written together by every door that owns it. */` |
|    462 | 4094 | `static void DtIvSetUsec(ph7_vm *pVm,ph7_class_instance *pIv,sxi64 us)` |
|      3 | 4095 | `{` |
|    465 | 4096 | `	PH7_NativeSetAttrInt(pVm,pIv,DT_IV_US,us);` |
|    465 | 4097 | `	PH7_NativeSetAttrReal(pVm,pIv,"f",(ph7_real)((double)us / 1000000.0));` |
|    465 | 4098 | `}` |
|      - | 4099 | `/* One date object's state, as the bodies below pass it around. */` |
|      - | 4100 | `typedef struct dt_state dt_state;` |
|      - | 4101 | `struct dt_state` |
|      - | 4102 | `{` |
|      - | 4103 | `	sxi64 iTs;` |
|      - | 4104 | `	sxi32 iOff;` |
|      - | 4105 | `	int uSec;` |
|      - | 4106 | `	const char *zName;   /* borrowed from the instance's own slot */` |
|      - | 4107 | `	int nName;` |
|      - | 4108 | `	int iZoneKind;       /* php's timezone_type: DT_ZONE_OFFSET / _ABBR / _ID */` |
|      - | 4109 | `};` |
|      - | 4110 | `/* php's name for a fixed offset: "+HH:MM" (and "+00:00" for zero, never "-00:00"). */` |
|    704 | 4111 | `static int DtOffName(char *zBuf,sxu32 nBuf,sxi32 iOff)` |
|      2 | 4112 | `{` |
|    706 | 4113 | `	sxi32 a = iOff < 0 ? -iOff : iOff;` |
|   1058 | 4114 | `	return (int)SyBufferFormat(zBuf,nBuf,"%c%02d:%02d",` |
|    704 | 4115 | `		iOff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|      2 | 4116 | `}` |
|      - | 4117 | `/*` |
|      - | 4118 | ` * The same name with php's SECONDS field, which it appends only when there is` |
|      - | 4119 | `` * one: `new DateTimeZone('+01:00:59')` is named "+01:00:59" and answers that to`` |
|      - | 4120 | `` * getName() and to format('e'), while `P`, `p`, `O` and `T` -- built from`` |
|      - | 4121 | ` * DtOffName above -- still stop at the minute there. So the two spellings are` |
|      - | 4122 | ` * separate on purpose.` |
|      - | 4123 | ` */` |
|    688 | 4124 | `static int DtOffNameSec(char *zBuf,sxu32 nBuf,sxi32 iOff)` |
|      2 | 4125 | `{` |
|    690 | 4126 | `	sxi32 a = iOff < 0 ? -iOff : iOff;` |
|      - | 4127 | `	/* php renders this into a buffer sized for its own example -- "+05:00" or` |
|      - | 4128 | `	 * "+05:00:01" -- so an offset whose hours want three digits comes back CUT.` |
|      - | 4129 | `	 * Only a format can build one: every other door caps the offset below 100` |
|      - | 4130 | ``	 * hours, and `e` on `+9999` is 100 hours 39 minutes, named "+100:3". */`` |
|    690 | 4131 | `	int nMax = (a % 60 == 0) ? 6 : 9;` |
|      - | 4132 | `	int n;` |
|    690 | 4133 | `	if( a % 60 == 0 ){` |
|    638 | 4134 | `		n = DtOffName(zBuf,nBuf,iOff);` |
|    320 | 4135 | `	}else{` |
|     53 | 4136 | `		n = (int)SyBufferFormat(zBuf,nBuf,"%c%02d:%02d:%02d",` |
|     52 | 4137 | `			iOff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60),(int)(a % 60));` |
|      - | 4138 | `	}` |
|    690 | 4139 | `	if( n > nMax ){` |
|      - | 4140 | `		/* php's snprintf CUTS the text at the buffer and still answers the` |
|      - | 4141 | `		 * length it WANTED, so the name a script reads back carries php's own` |
|      - | 4142 | ``		 * terminator inside it: `+9999` is the seven bytes "+100:3\0". */`` |
|      3 | 4143 | `		zBuf[nMax] = 0;` |
|      3 | 4144 | `		if( n > nMax + 1 ){ n = nMax + 1; }` |
|      1 | 4145 | `	}` |
|    690 | 4146 | `	return n;` |
|      2 | 4147 | `}` |
|      - | 4148 | `/*` |
|      - | 4149 | ` * php's timezone_type read off a NAME alone -- the fallback for a zone that` |
|      - | 4150 | `` * reached the engine without one: a payload `__unserialize()` re-parses (php`` |
|      - | 4151 | ` * re-derives there too, which is why a serialized type-2 "UTC" comes back a` |
|      - | 4152 | ` * type 3 in both engines), and an object whose slots are still at their` |
|      - | 4153 | ` * defaults. Everywhere a SPELLING was seen, the kind stored with it wins --` |
|      - | 4154 | ` * "UTC" and "utc" are one name and two kinds.` |
|      - | 4155 | ` *` |
|      - | 4156 | ` * 1 = a fixed UTC OFFSET ("+02:00"), 2 = an ABBREVIATION ("GMT", "Z"),` |
|      - | 4157 | ` * 3 = an IDENTIFIER ("UTC", "Europe/Paris"). PHL accepts offsets, UTC, GMT and Z` |
|      - | 4158 | ` * today; the identifier arm is written for the whole rule so a tz database can` |
|      - | 4159 | ` * only add names, never change the tagging.` |
|      - | 4160 | ` */` |
|    ! 0 | 4161 | `static int DtZoneTypeOf(const char *zName,int nName)` |
|    ! 0 | 4162 | `{` |
|    ! 0 | 4163 | `	sxu32 nPos = 0;` |
|    ! 0 | 4164 | `	if( nName > 0 && (zName[0] == '+' \|\| zName[0] == '-') ){` |
|    ! 0 | 4165 | `		return DT_ZONE_OFFSET;` |
|      - | 4166 | `	}` |
|    ! 0 | 4167 | `	if( nName == 3 && SyMemcmp(zName,"UTC",3) == 0 ){` |
|    ! 0 | 4168 | `		return DT_ZONE_ID;` |
|      - | 4169 | `	}` |
|    ! 0 | 4170 | `	if( nName > 0 && SyByteFind(zName,(sxu32)nName,'/',&nPos) == SXRET_OK ){` |
|    ! 0 | 4171 | `		return DT_ZONE_ID;` |
|      - | 4172 | `	}` |
|    ! 0 | 4173 | `	return DT_ZONE_ABBR;` |
|    ! 0 | 4174 | `}` |
|      - | 4175 | `/* The kind an instance carries in zSlot, or the name's own rule when the slot is` |
|      - | 4176 | ` * still zero -- an object built by newInstanceWithoutConstructor, or one whose` |
|      - | 4177 | ` * state predates the slot. */` |
|   4872 | 4178 | `static int DtZoneKindOf(ph7_class_instance *pObj,const char *zSlot,const char *zName,int nName)` |
|      3 | 4179 | `{` |
|   4875 | 4180 | `	int iKind = (int)PH7_NativeAttrInt(pObj,zSlot);` |
|   4875 | 4181 | `	if( iKind < DT_ZONE_OFFSET \|\| iKind > DT_ZONE_ID ){` |
|    ! 0 | 4182 | `		return DtZoneTypeOf(zName ? zName : "",nName);` |
|      - | 4183 | `	}` |
|   4875 | 4184 | `	return iKind;` |
|   2439 | 4185 | `}` |
|   3542 | 4186 | `static void DtLoad(ph7_class_instance *pObj,dt_state *pOut)` |
|      3 | 4187 | `{` |
|   3545 | 4188 | `	pOut->iTs  = PH7_NativeAttrInt(pObj,DT_TS);` |
|   3545 | 4189 | `	pOut->iOff = (sxi32)PH7_NativeAttrInt(pObj,DT_OFF);` |
|   3545 | 4190 | `	pOut->uSec = (int)PH7_NativeAttrInt(pObj,DT_US);` |
|   3545 | 4191 | `	PH7_NativeAttrStr(pObj,DT_NAME,&pOut->zName,&pOut->nName);` |
|   3545 | 4192 | `	pOut->iZoneKind = DtZoneKindOf(pObj,DT_ZKIND,pOut->zName,pOut->nName);` |
|   3545 | 4193 | `}` |
|   2926 | 4194 | `static void DtStore(ph7_vm *pVm,ph7_class_instance *pObj,const dt_state *pIn)` |
|      3 | 4195 | `{` |
|   2929 | 4196 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_TS,pIn->iTs);` |
|   2929 | 4197 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,pIn->iOff);` |
|   2929 | 4198 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_US,pIn->uSec);` |
|   2929 | 4199 | `	PH7_NativeSetAttrStr(pVm,pObj,DT_NAME,pIn->zName,pIn->nName);` |
|   2929 | 4200 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_ZKIND,pIn->iZoneKind);` |
|   2929 | 4201 | `}` |
|      - | 4202 | `/*` |
|      - | 4203 | ` * modify()'s one zone rule. php copies the parsed FIELDS into the object and` |
|      - | 4204 | ` * leaves its zone alone -- so a modifier that names a zone moves nothing -- with` |
|      - | 4205 | `` * `@epoch` the single exception: that form names an absolute instant, and php`` |
|      - | 4206 | `` * re-zones the object to the fixed `+00:00` along with it. Every other modifier`` |
|      - | 4207 | ` * leaves this a no-op.` |
|      - | 4208 | ` */` |
|   1082 | 4209 | `static void DtEpochRezone(ph7_vm *pVm,ph7_class_instance *pObj,const dt_parsed *pVec)` |
|      3 | 4210 | `{` |
|      - | 4211 | `	char zBuf[16];` |
|      - | 4212 | `	int nName;` |
|   1085 | 4213 | `	if( !pVec->bEpoch ){` |
|   1065 | 4214 | `		return;` |
|      - | 4215 | `	}` |
|     21 | 4216 | `	nName = DtOffName(zBuf,sizeof(zBuf),0);` |
|     21 | 4217 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,0);` |
|     21 | 4218 | `	PH7_NativeSetAttrStr(pVm,pObj,DT_NAME,zBuf,nName);` |
|     21 | 4219 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_ZKIND,DT_ZONE_OFFSET);` |
|    544 | 4220 | `}` |
|  16482 | 4221 | `static ph7_class * DtClass(ph7_vm *pVm,const char *zName)` |
|      5 | 4222 | `{` |
|  16487 | 4223 | `	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);` |
|      5 | 4224 | `}` |
|      - | 4225 | `/* Is this instance an instance of the named date class? */` |
|    538 | 4226 | `static int DtIsA(ph7_vm *pVm,ph7_class_instance *pObj,const char *zClass)` |
|      3 | 4227 | `{` |
|      - | 4228 | `	ph7_class *pClass;` |
|    541 | 4229 | `	if( pObj == 0 ){` |
|      3 | 4230 | `		return 0;` |
|      - | 4231 | `	}` |
|    539 | 4232 | `	pClass = DtClass(&(*pVm),zClass);` |
|    539 | 4233 | `	return pClass != 0 && PH7_VmInstanceOf(pObj->pClass,pClass);` |
|    272 | 4234 | `}` |
|      - | 4235 | `/* Has this object run a constructor (or been built whole by a C factory)? */` |
|  10588 | 4236 | `static int DtIsInit(ph7_class_instance *pObj)` |
|      3 | 4237 | `{` |
|  10591 | 4238 | `	return pObj != 0 && PH7_NativeAttrInt(pObj,DT_INIT) != 0;` |
|      3 | 4239 | `}` |
|      - | 4240 | `/* Say so. Called by every constructor that SUCCEEDS -- a failing one leaves the` |
|      - | 4241 | ` * object as it found it, which is php's answer too: an object whose` |
|      - | 4242 | `` * `__construct()` threw is still an uninitialized one. */`` |
|   4712 | 4243 | `static void DtSetInit(ph7_vm *pVm,ph7_class_instance *pObj)` |
|      3 | 4244 | `{` |
|   4715 | 4245 | `	PH7_NativeSetAttrInt(&(*pVm),pObj,DT_INIT,1);` |
|   4715 | 4246 | `}` |
|      - | 4247 | `/* A date object built from C rather than by a constructor: complete on arrival, so` |
|      - | 4248 | ` * it is born initialized. Every factory in this file goes through here. */` |
|    912 | 4249 | `static ph7_class_instance * DtNewInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 | 4250 | `{` |
|    914 | 4251 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|    914 | 4252 | `	if( pObj ){` |
|    914 | 4253 | `		DtSetInit(&(*pVm),pObj);` |
|    456 | 4254 | `	}` |
|    914 | 4255 | `	return pObj;` |
|      2 | 4256 | `}` |
|      - | 4257 | `/*` |
|      - | 4258 | ` * php's DateObjectError, worded for the object it is raised on: the class's own` |
|      - | 4259 | ` * name, and -- for a SUBCLASS -- the internal class it inherits, whatever the` |
|      - | 4260 | `` * depth of the chain (`class B extends A extends DateTime` reports`` |
|      - | 4261 | ` * "B (inheriting DateTime)").` |
|      - | 4262 | ` */` |
|    110 | 4263 | `static const char * DtNativeBase(ph7_vm *pVm,ph7_class_instance *pObj)` |
|      1 | 4264 | `{` |
|      - | 4265 | `	static const char * const azBase[] = {` |
|      - | 4266 | `		"DateTime","DateTimeImmutable","DateTimeZone","DateInterval","DatePeriod"` |
|      - | 4267 | `	};` |
|      - | 4268 | `	sxu32 n;` |
|    225 | 4269 | `	for( n = 0 ; n < SX_ARRAYSIZE(azBase) ; ++n ){` |
|    225 | 4270 | `		if( DtIsA(&(*pVm),pObj,azBase[n]) ){` |
|    111 | 4271 | `			return azBase[n];` |
|      - | 4272 | `		}` |
|     58 | 4273 | `	}` |
|    ! 0 | 4274 | `	return 0;` |
|     56 | 4275 | `}` |
|    110 | 4276 | `static int DtThrowUninit(ph7_context *pCtx,ph7_class_instance *pObj)` |
|      1 | 4277 | `{` |
|    111 | 4278 | `	const char *zBase = DtNativeBase(pCtx->pVm,pObj);` |
|    111 | 4279 | `	SyString *pName = &pObj->pClass->sName;` |
|    110 | 4280 | `	if( zBase == 0` |
|    111 | 4281 | `	 \|\| (pName->nByte == SyStrlen(zBase) && SyMemcmp(pName->zString,zBase,pName->nByte) == 0) ){` |
|    151 | 4282 | `		return PH7_VmThrowException(pCtx,"DateObjectError",` |
|      - | 4283 | `			"Object of type %z has not been correctly initialized by calling "` |
|     50 | 4284 | `			"parent::__construct() in its constructor",pName);` |
|      - | 4285 | `	}` |
|     16 | 4286 | `	return PH7_VmThrowException(pCtx,"DateObjectError",` |
|      - | 4287 | `		"Object of type %z (inheriting %s) has not been correctly initialized by "` |
|      5 | 4288 | `		"calling parent::__construct() in its constructor",pName,zBase);` |
|     56 | 4289 | `}` |
|      - | 4290 | `/*` |
|      - | 4291 | ` * The receiver of a native method, or NULL when the call has no object (which the` |
|      - | 4292 | ` * dispatcher only allows for a static one) -- and NULL as well for an object that` |
|      - | 4293 | ` * was never constructed, whose DateObjectError is raised here.` |
|      - | 4294 | ` *` |
|      - | 4295 | ` * This is the one screen the whole family shares: every method body already treats` |
|      - | 4296 | ` * a null receiver as "nothing to do" and returns PH7_OK, and the raise records the` |
|      - | 4297 | ` * status on the context, which the host-call boundary reports (VmHostFuncThrowRc).` |
|      - | 4298 | ` * The four doors php lets through -- the constructors, __unserialize and __wakeup,` |
|      - | 4299 | ` * which exist to initialize the object, and DatePeriod's two nullable getters --` |
|      - | 4300 | ` * take DtThisRaw() instead.` |
|      - | 4301 | ` */` |
|  10700 | 4302 | `static ph7_class_instance * DtThisRaw(ph7_context *pCtx)` |
|      3 | 4303 | `{` |
|  10703 | 4304 | `	return PH7_ContextThis(pCtx);` |
|      3 | 4305 | `}` |
|   6268 | 4306 | `static ph7_class_instance * DtThis(ph7_context *pCtx)` |
|      3 | 4307 | `{` |
|   6271 | 4308 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|   6271 | 4309 | `	if( pThis == 0 \|\| DtIsInit(pThis) ){` |
|   6203 | 4310 | `		return pThis;` |
|      - | 4311 | `	}` |
|     69 | 4312 | `	DtThrowUninit(pCtx,pThis);` |
|     69 | 4313 | `	return 0;` |
|   3137 | 4314 | `}` |
|      - | 4315 | `/*` |
|      - | 4316 | ` * An object ARGUMENT that must be constructed: php raises the same DateObjectError` |
|      - | 4317 | ` * for a date it is HANDED as for the one it is called on. Answers -1 when it` |
|      - | 4318 | ` * raised; a value that is not an object at all was refused by the declared type` |
|      - | 4319 | ` * upstream, so it passes through.` |
|      - | 4320 | ` */` |
|    464 | 4321 | `static int DtArgInit(ph7_context *pCtx,ph7_value *pArg)` |
|      1 | 4322 | `{` |
|      - | 4323 | `	ph7_class_instance *pObj;` |
|    465 | 4324 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 4325 | `		return 0;` |
|      - | 4326 | `	}` |
|    465 | 4327 | `	pObj = (ph7_class_instance *)pArg->x.pOther;` |
|    465 | 4328 | `	if( DtIsInit(pObj) ){` |
|    451 | 4329 | `		return 0;` |
|      - | 4330 | `	}` |
|     15 | 4331 | `	DtThrowUninit(pCtx,pObj);` |
|     15 | 4332 | `	return -1;` |
|    233 | 4333 | `}` |
|      - | 4334 | `/*` |
|      - | 4335 | ` * The same refusal for the one door that names the parameter's DECLARED type` |
|      - | 4336 | ` * instead of the object's class: php reports DatePeriod's start and end dates as` |
|      - | 4337 | ` * "DateTimeInterface" whatever they really are.` |
|      - | 4338 | ` */` |
|    338 | 4339 | `static int DtArgInitNamed(ph7_context *pCtx,ph7_value *pArg,const char *zName)` |
|      1 | 4340 | `{` |
|    338 | 4341 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0` |
|    265 | 4342 | `	 \|\| DtIsInit((ph7_class_instance *)pArg->x.pOther) ){` |
|    335 | 4343 | `		return 0;` |
|      - | 4344 | `	}` |
|      7 | 4345 | `	PH7_VmThrowException(pCtx,"DateObjectError",` |
|      - | 4346 | `		"Object of type %s has not been correctly initialized by calling "` |
|      2 | 4347 | `		"parent::__construct() in its constructor",zName);` |
|      5 | 4348 | `	return -1;` |
|    170 | 4349 | `}` |
|      - | 4350 | `/* An immutable receiver mutates a COPY; a mutable one mutates itself. That is the` |
|      - | 4351 | ` * only difference between the two classes' method tables, so both share one body. */` |
|   1570 | 4352 | `static int DtIsImmutable(ph7_vm *pVm,ph7_class_instance *pObj)` |
|      3 | 4353 | `{` |
|   1573 | 4354 | `	ph7_class *pImm = DtClass(pVm,"DateTimeImmutable");` |
|   1573 | 4355 | `	return pImm != 0 && PH7_VmInstanceOf(pObj->pClass,pImm);` |
|      3 | 4356 | `}` |
|      - | 4357 | `/*` |
|      - | 4358 | ` * The object a mutator writes: $this itself, or a clone for DateTimeImmutable.` |
|      - | 4359 | ` * Either way the caller returns it, so a mutable method answers the same object` |
|      - | 4360 | `` * php's does (`$d->modify(...) === $d`).`` |
|      - | 4361 | ` */` |
|   1256 | 4362 | `static ph7_class_instance * DtMutTarget(ph7_context *pCtx,ph7_class_instance *pThis,int *pbCopy)` |
|      3 | 4363 | `{` |
|   1259 | 4364 | `	if( DtIsImmutable(pCtx->pVm,pThis) ){` |
|     90 | 4365 | `		*pbCopy = 1;` |
|     90 | 4366 | `		return PH7_CloneClassInstance(pThis);` |
|      - | 4367 | `	}` |
|   1171 | 4368 | `	*pbCopy = 0;` |
|   1171 | 4369 | `	return pThis;` |
|    631 | 4370 | `}` |
|      - | 4371 | `/* Return a mutator's target the way php returns it: the clone (whose reference we` |
|      - | 4372 | ` * own) or the receiver itself (whose value the context already holds). */` |
|   1256 | 4373 | `static void DtMutResult(ph7_context *pCtx,ph7_class_instance *pTarget,int bCopy)` |
|      3 | 4374 | `{` |
|   1259 | 4375 | `	if( bCopy ){` |
|     90 | 4376 | `		PH7_NativeResultObject(pCtx,pTarget);` |
|     46 | 4377 | `	}else{` |
|   1171 | 4378 | `		ph7_result_value(pCtx,PH7_ContextThisValue(pCtx));` |
|      - | 4379 | `	}` |
|   1259 | 4380 | `}` |
|      - | 4381 | `/* Read a DateTimeZone argument's two slots. php's ext/date reads its own internal` |
|      - | 4382 | ` * timezone struct here, so an overridden getName()/getOffset() is ignored by both` |
|      - | 4383 | ` * engines. Answers 0 when the value is not a DateTimeZone at all. */` |
|    838 | 4384 | `static int DtZoneOf(ph7_value *pArg,sxi32 *piOff,const char **pzName,int *pnName,int *piKind)` |
|      2 | 4385 | `{` |
|      - | 4386 | `	ph7_class_instance *pObj;` |
|    840 | 4387 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 4388 | `		return 0;` |
|      - | 4389 | `	}` |
|    840 | 4390 | `	pObj = (ph7_class_instance *)pArg->x.pOther;` |
|    840 | 4391 | `	if( PH7_NativeAttr(pObj,DTZ_NAME) == 0 ){` |
|    ! 0 | 4392 | `		return 0;` |
|      - | 4393 | `	}` |
|    840 | 4394 | `	if( !DtIsInit(pObj) ){` |
|      - | 4395 | `		/* php reads the zone's C struct here too, and an unconstructed one has` |
|      - | 4396 | `		 * none: its timelib fallback is a fixed UTC OFFSET, which is why` |
|      - | 4397 | ``		 * `$d->setTimezone($uninitialized)` answers "+00:00" rather than raising.`` |
|      - | 4398 | `		 * The doors that BUILD a date from a zone refuse instead -- see` |
|      - | 4399 | `		 * DtZoneArgInit(), which they call first. */` |
|      5 | 4400 | `		*piOff = 0;` |
|      5 | 4401 | `		*pzName = "+00:00";` |
|      5 | 4402 | `		*pnName = (int)sizeof("+00:00") - 1;` |
|      5 | 4403 | `		*piKind = DT_ZONE_OFFSET;` |
|      5 | 4404 | `		return 1;` |
|      - | 4405 | `	}` |
|    836 | 4406 | `	*piOff = (sxi32)PH7_NativeAttrInt(pObj,DTZ_OFF);` |
|    836 | 4407 | `	PH7_NativeAttrStr(pObj,DTZ_NAME,pzName,pnName);` |
|    836 | 4408 | `	*piKind = DtZoneKindOf(pObj,DTZ_KIND,*pzName,*pnName);` |
|    836 | 4409 | `	return 1;` |
|    421 | 4410 | `}` |
|      - | 4411 | `/*` |
|      - | 4412 | ` * The zone argument of a door that INITIALIZES a date from it -- the two` |
|      - | 4413 | `` * constructors, `date_create()` and `createFromFormat()`. php refuses an`` |
|      - | 4414 | ` * unconstructed zone there, and with a different sentence and a different class` |
|      - | 4415 | `` * from every other uninitialized-object refusal in the family: a plain `Error`,`` |
|      - | 4416 | ` * naming no method. Answers -1 when it raised.` |
|      - | 4417 | ` */` |
|    838 | 4418 | `static int DtZoneArgInit(ph7_context *pCtx,ph7_value *pArg)` |
|      2 | 4419 | `{` |
|    838 | 4420 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0` |
|    840 | 4421 | `	 \|\| DtIsInit((ph7_class_instance *)pArg->x.pOther) ){` |
|    832 | 4422 | `		return 0;` |
|      - | 4423 | `	}` |
|      9 | 4424 | `	PH7_VmThrowException(pCtx,"Error",` |
|      - | 4425 | `		"The DateTimeZone object has not been correctly initialized by its constructor");` |
|      9 | 4426 | `	return -1;` |
|    421 | 4427 | `}` |
|      - | 4428 | `/*` |
|      - | 4429 | ` * Parse $datetime into a date object's state, php's constructor rules: an explicit` |
|      - | 4430 | ` * offset in the string wins over the $timezone argument, a literal "Z" keeps its` |
|      - | 4431 | ` * own name, and everything else takes the argument's (or the default) zone.` |
|      - | 4432 | ` * Returns 0 on success; on failure the caller throws with the reason and position` |
|      - | 4433 | ` * this reports.` |
|      - | 4434 | ` */` |
|   2808 | 4435 | `static int DtInitState(ph7_context *pCtx,const char *zIn,int nIn,sxi32 iZoneOff,` |
|      - | 4436 | `	const char *zZoneName,int nZoneName,int iZoneKind,dt_state *pOut,char *zNameBuf,` |
|      - | 4437 | `	sxu32 nNameBuf,const char **pzErr,int *piPos,char *pcAt)` |
|      3 | 4438 | `{` |
|   2811 | 4439 | `	sxi64 iTs = 0,iNow = 0;` |
|   2811 | 4440 | `	sxi32 iOff = 0;` |
|   2811 | 4441 | `	int bOffSet = 0,uSec = 0,iErrPos,uNow = 0;` |
|      - | 4442 | `	dt_parsed sVec;` |
|      - | 4443 | `	/* php's base moment is the whole clock, microseconds included: a string that` |
|      - | 4444 | ``	 * names no time of day keeps them (`new DateTime()`, `+1 day`), and one that`` |
|      - | 4445 | `	 * does zeroes them along with the rest of the clock. */` |
|   2811 | 4446 | `	DtNowUs(pCtx->pVm,&iNow,&uNow);` |
|      - | 4447 | ``	/* php's constructors pass the word `now` in place of an empty string, which`` |
|      - | 4448 | ``	 * is why `new DateTime('')` is the current moment where `modify('')` is its`` |
|      - | 4449 | ``	 * `Empty string` refusal. */`` |
|   2811 | 4450 | `	if( nIn < 1 ){` |
|      3 | 4451 | `		zIn = "now";` |
|      3 | 4452 | `		nIn = 3;` |
|      1 | 4453 | `	}` |
|      - | 4454 | `	/* php publishes what this scan collected through getLastErrors(), whether or` |
|      - | 4455 | ``	 * not it throws, and a clean parse puts the record back to `false`. */`` |
|   4215 | 4456 | `	iErrPos = DtParseEx(zIn,nIn,iNow,iZoneOff,uNow,0,&iTs,&iOff,&bOffSet,&uSec,&sVec,` |
|   2808 | 4457 | `		&pCtx->pVm->sDtLastErr);` |
|   2811 | 4458 | `	if( iErrPos != 0 ){` |
|    293 | 4459 | `		*pzErr = DtParseErr(zIn,nIn,iErrPos,piPos,pcAt);` |
|    293 | 4460 | `		return -1;` |
|      - | 4461 | `	}` |
|   2519 | 4462 | `	pOut->iTs = iTs;` |
|   2519 | 4463 | `	pOut->uSec = uSec;` |
|   2519 | 4464 | `	if( bOffSet ){` |
|    540 | 4465 | `		pOut->iOff = iOff;` |
|    540 | 4466 | `		if( bOffSet == 2 ){` |
|      - | 4467 | ``			/* a zone the STRING named -- php's `Z`, or a trailing UTC/GMT */`` |
|    179 | 4468 | `			pOut->zName = sVec.zZone;` |
|    179 | 4469 | `			pOut->nName = sVec.nZone;` |
|    179 | 4470 | `			pOut->iZoneKind = sVec.bZoneIdent ? DT_ZONE_ID : DT_ZONE_ABBR;` |
|     90 | 4471 | `		}else{` |
|      - | 4472 | `			/* ...Sec: an offset the string spelled with SECONDS is named with` |
|      - | 4473 | ``			 * them (`+02:00:30`), which is the same name DateTimeZone gives it. */`` |
|    362 | 4474 | `			pOut->nName = DtOffNameSec(zNameBuf,nNameBuf,iOff);` |
|    362 | 4475 | `			pOut->zName = zNameBuf;` |
|    362 | 4476 | `			pOut->iZoneKind = DT_ZONE_OFFSET;` |
|      - | 4477 | `		}` |
|    271 | 4478 | `	}else{` |
|   1980 | 4479 | `		pOut->iOff = iZoneOff;` |
|   1980 | 4480 | `		pOut->zName = zZoneName;` |
|   1980 | 4481 | `		pOut->nName = nZoneName;` |
|   1980 | 4482 | `		pOut->iZoneKind = iZoneKind;` |
|      - | 4483 | `	}` |
|   2519 | 4484 | `	return 0;` |
|   1407 | 4485 | `}` |
|      - | 4486 | `/*` |
|      - | 4487 | ` * php's UTC-OFFSET spellings, the whole set of them. Reads the digits and colons` |
|      - | 4488 | ` * after the sign and dispatches on their SHAPE, which is what php's scanner does:` |
|      - | 4489 | ` *` |
|      - | 4490 | ` *   D \| DD              the HOURS alone            +1     +01    +59` |
|      - | 4491 | ` *   DDD                 H then MM                  +130 = +01:30, +999 = +10:39` |
|      - | 4492 | ` *   DDDD                HH then MM                 +0100  +0060 = +01:00` |
|      - | 4493 | ` *   DDDDDD              HH then MM then SS         +010059 = +01:00:59` |
|      - | 4494 | ` *   D:D \| DD:D \| D:DD \| DD:DD    hours then minutes` |
|      - | 4495 | ` *   DD:DD:DD            hours, minutes and seconds` |
|      - | 4496 | ` *` |
|      - | 4497 | ` * Five digits, seven digits and a one-digit hour before two colons are php's own` |
|      - | 4498 | `` * refusals. Minutes and seconds are NOT bounded on their own -- `+00:60` is an`` |
|      - | 4499 | `` * hour and `+01:99` is +02:39 -- only the TOTAL is, and a total at or past 100`` |
|      - | 4500 | ` * hours is php's separate "Timezone offset is out of range" (answered here as -2,` |
|      - | 4501 | ` * because the two refusals are worded differently at every door).` |
|      - | 4502 | ` *` |
|      - | 4503 | ` * Answers the KIND as well (php's timezone_type), because the name cannot carry` |
|      - | 4504 | ` * it: "UTC" spelled exactly is an IDENTIFIER and any other casing of it is an` |
|      - | 4505 | ` * ABBREVIATION, and php refuses to compare the two.` |
|      - | 4506 | ` */` |
|      - | 4507 | `#define DT_ZONE_OFF_LIMIT 360000   /* php's ceiling: \|offset\| < 100 hours */` |
|    664 | 4508 | `static int DtZoneOffsetDigits(const char *z,int n,sxi32 *piOff,int *pnUsed)` |
|      1 | 4509 | `{` |
|      - | 4510 | `	int aVal[3];` |
|      - | 4511 | `	int aWidth[3];` |
|    665 | 4512 | `	int nPart = 0;` |
|      - | 4513 | `	int i;` |
|      - | 4514 | `	sxi64 iOff;` |
|    665 | 4515 | `	aVal[0] = aVal[1] = aVal[2] = 0;` |
|    665 | 4516 | `	aWidth[0] = aWidth[1] = aWidth[2] = 0;` |
|      - | 4517 | `	/* Read the RUN of digits and colons and stop at anything else; the caller` |
|      - | 4518 | `	 * decides what a tail means. At most two colons, and no group may be empty` |
|      - | 4519 | `	 * or wider than two -- except the single group of a colonless spelling,` |
|      - | 4520 | `	 * which is split by WIDTH below instead. */` |
|   3493 | 4521 | `	for( i = 0 ; i < n ; ++i ){` |
|   2853 | 4522 | `		if( z[i] == ':' ){` |
|    419 | 4523 | `			if( nPart >= 2 ){` |
|    ! 0 | 4524 | `				break;` |
|      - | 4525 | `			}` |
|    419 | 4526 | `			nPart++;` |
|    419 | 4527 | `			continue;` |
|      - | 4528 | `		}` |
|   2435 | 4529 | `		if( !SyisDigit(z[i]) ){` |
|     17 | 4530 | `			break;` |
|      - | 4531 | `		}` |
|   2419 | 4532 | `		if( aWidth[nPart] >= 6 ){` |
|      9 | 4533 | `			return -1;` |
|      - | 4534 | `		}` |
|   2411 | 4535 | `		aVal[nPart] = aVal[nPart] * 10 + (z[i] - '0');` |
|   2411 | 4536 | `		aWidth[nPart]++;` |
|   1206 | 4537 | `	}` |
|    657 | 4538 | `	*pnUsed = i;` |
|    657 | 4539 | `	if( nPart == 0 ){` |
|      - | 4540 | `		/* No colon: the WIDTH says how the digits split. */` |
|    321 | 4541 | `		int v = aVal[0];` |
|    321 | 4542 | `		switch( aWidth[0] ){` |
|     83 | 4543 | `			case 1: case 2:  /* H, HH */` |
|    167 | 4544 | `				iOff = (sxi64)v * 3600;` |
|    167 | 4545 | `				break;` |
|     12 | 4546 | `			case 3:          /* H MM */` |
|     25 | 4547 | `				iOff = (sxi64)(v / 100) * 3600 + (v % 100) * 60;` |
|     25 | 4548 | `				break;` |
|     45 | 4549 | `			case 4:          /* HH MM */` |
|     91 | 4550 | `				iOff = (sxi64)(v / 100) * 3600 + (v % 100) * 60;` |
|     91 | 4551 | `				break;` |
|     12 | 4552 | `			case 6:          /* HH MM SS */` |
|     25 | 4553 | `				iOff = (sxi64)(v / 10000) * 3600 + ((v / 100) % 100) * 60 + (v % 100);` |
|     25 | 4554 | `				break;` |
|      8 | 4555 | `			default:         /* five, or seven and up */` |
|     17 | 4556 | `				return -1;` |
|      - | 4557 | `		}` |
|    153 | 4558 | `	}else{` |
|      - | 4559 | `		/* Colons: H:M through HH:MM, or HH:MM:SS with two digits everywhere` |
|      - | 4560 | ``		 * (php refuses `+1:00:00` and `+01:00:0` alike, and takes `+1:1` for`` |
|      - | 4561 | `		 * +01:01). A trailing colon leaves an empty group, which is a refusal. */` |
|    337 | 4562 | `		int nWant = nPart == 2 ? 2 : 0;` |
|   1027 | 4563 | `		for( i = 0 ; i <= nPart ; ++i ){` |
|    715 | 4564 | `			if( aWidth[i] < 1 \|\| aWidth[i] > 2 \|\| (nWant && aWidth[i] != nWant) ){` |
|     25 | 4565 | `				return -1;` |
|      - | 4566 | `			}` |
|    346 | 4567 | `		}` |
|    313 | 4568 | `		iOff = (sxi64)aVal[0] * 3600 + (sxi64)aVal[1] * 60 + aVal[2];` |
|      - | 4569 | `	}` |
|    617 | 4570 | `	if( iOff >= DT_ZONE_OFF_LIMIT ){` |
|      - | 4571 | `		/* Past php's ceiling, and php answers THAT even when the spelling has a` |
|      - | 4572 | `		 * tail it would otherwise reject: the range is checked on what the` |
|      - | 4573 | `		 * scanner read, before anything is said about what follows. */` |
|     33 | 4574 | `		return -2;` |
|      - | 4575 | `	}` |
|    585 | 4576 | `	*piOff = (sxi32)iOff;` |
|    585 | 4577 | `	return 0;` |
|    333 | 4578 | `}` |
|      - | 4579 | `/*` |
|      - | 4580 | ` * The timezone spellings PHL understands with no tz database: UTC, GMT, Z and a` |
|      - | 4581 | `` * fixed offset, optionally behind a `GMT` prefix and behind leading blanks.`` |
|      - | 4582 | ` * Shared by DateTimeZone::__construct(), which throws on a miss, and` |
|      - | 4583 | ` * timezone_open(), which warns and answers false. Answers 0, -1 (unknown or bad)` |
|      - | 4584 | ` * or -2 (an offset past php's range).` |
|      - | 4585 | ` */` |
|   1154 | 4586 | `static int DtZoneParse(const char *zTz,int nTz,sxi32 *piOff,const char **pzName,` |
|      - | 4587 | `	int *pnName,int *piKind,char *zBuf,sxu32 nBuf)` |
|      2 | 4588 | `{` |
|   1156 | 4589 | `	int rc,nUsed = 0;` |
|      - | 4590 | `	/* php's scanner skips leading blanks and nothing else -- a TRAILING one is a` |
|      - | 4591 | `	 * refusal, and so is a newline before the sign. */` |
|   1737 | 4592 | `	while( nTz > 0 && (zTz[0] == ' ' \|\| zTz[0] == '\t') ){` |
|      5 | 4593 | `		zTz++;` |
|      5 | 4594 | `		nTz--;` |
|      1 | 4595 | `	}` |
|      - | 4596 | `` 	/* A single letter is php's military zone here too -- `new DateTimeZone('t')` `` |
|      - | 4597 | ``	 * is named `T` and answers -07:00 -- which is what lets a date carrying one`` |
|      - | 4598 | `	 * round-trip through serialize()/__unserialize(). */` |
|   1156 | 4599 | `	if( nTz == 1 && DtZoneMil(zTz[0],piOff,pzName) ){` |
|     35 | 4600 | `		*pnName = 1;` |
|     35 | 4601 | `		*piKind = DT_ZONE_ABBR;` |
|     35 | 4602 | `		return 0;` |
|      - | 4603 | `	}` |
|   1122 | 4604 | `	if( nTz == 3 && (SyStrnicmp(zTz,"UTC",3) == 0 \|\| SyStrnicmp(zTz,"GMT",3) == 0) ){` |
|      - | 4605 | `		/* php answers the canonical spelling, whatever case the caller used --` |
|      - | 4606 | `		 * and only the exact "UTC" is one of its tz-database IDENTIFIERS. */` |
|    700 | 4607 | `		int bUtc = (zTz[0] == 'u' \|\| zTz[0] == 'U');` |
|    700 | 4608 | `		*piOff = 0;` |
|    700 | 4609 | `		*pzName = bUtc ? "UTC" : "GMT";` |
|    700 | 4610 | `		*pnName = 3;` |
|    700 | 4611 | `		*piKind = (bUtc && SyMemcmp(zTz,"UTC",3) == 0) ? DT_ZONE_ID : DT_ZONE_ABBR;` |
|    700 | 4612 | `		return 0;` |
|      - | 4613 | `	}` |
|    423 | 4614 | `	if( nTz > 3 && SyMemcmp(zTz,"GMT",3) == 0 ){` |
|      - | 4615 | ``		/* `GMT+01:00` is php's offset, named for the offset alone. The prefix is`` |
|      - | 4616 | ``		 * UPPERCASE only there (`gmt+1` is a refusal where the bare `gmt` is a`` |
|      - | 4617 | ``		 * zone), no blank is allowed between the two halves, and `UTC+1` is not a`` |
|      - | 4618 | `		 * spelling at all. */` |
|     13 | 4619 | `		zTz += 3;` |
|     13 | 4620 | `		nTz -= 3;` |
|      6 | 4621 | `	}` |
|    423 | 4622 | `	if( nTz < 2 \|\| (zTz[0] != '+' && zTz[0] != '-') ){` |
|     53 | 4623 | `		return -1;` |
|      - | 4624 | `	}` |
|    371 | 4625 | `	rc = DtZoneOffsetDigits(zTz + 1,nTz - 1,piOff,&nUsed);` |
|    371 | 4626 | `	if( rc != 0 ){` |
|     81 | 4627 | `		return rc;` |
|      - | 4628 | `	}` |
|    291 | 4629 | `	if( nUsed != nTz - 1 ){` |
|      9 | 4630 | `		return -1;   /* a tail the scanner did not read: not a zone at all */` |
|      - | 4631 | `	}` |
|    283 | 4632 | `	if( zTz[0] == '-' ){` |
|     49 | 4633 | `		*piOff = -*piOff;` |
|     24 | 4634 | `	}` |
|      - | 4635 | `	/* php normalizes the NAME through the offset, so "-00:00" is "+00:00", and` |
|      - | 4636 | `	 * carries the SECONDS field only when there is one. */` |
|    283 | 4637 | `	*pnName = DtOffNameSec(zBuf,nBuf,*piOff);` |
|    283 | 4638 | `	*pzName = zBuf;` |
|    283 | 4639 | `	*piKind = DT_ZONE_OFFSET;` |
|    283 | 4640 | `	return 0;` |
|    579 | 4641 | `}` |
|      - | 4642 | `/* DateTimeZone::__construct(string $timezone) */` |
|    990 | 4643 | `static int vm_builtin_DateTimeZone_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4644 | `{` |
|    992 | 4645 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */` |
|      - | 4646 | `	const char *zTz,*zName;` |
|    992 | 4647 | `	int nTz,nName,iKind = DT_ZONE_ID,rc;` |
|    992 | 4648 | `	sxi32 iOff = 0;` |
|      - | 4649 | `	char zBuf[16];` |
|    992 | 4650 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 4651 | `		return PH7_OK;` |
|      - | 4652 | `	}` |
|    992 | 4653 | `	zTz = ph7_value_to_string(apArg[0],&nTz);` |
|    992 | 4654 | `	rc = DtZoneParse(zTz,nTz,&iOff,&zName,&nName,&iKind,zBuf,sizeof(zBuf));` |
|    992 | 4655 | `	if( rc != 0 ){` |
|    109 | 4656 | `		return PH7_VmThrowException(pCtx,"DateInvalidTimeZoneException",` |
|     36 | 4657 | `			rc == -2 ? "DateTimeZone::__construct(): Timezone offset is out of range (%.*s)"` |
|     36 | 4658 | `			         : "DateTimeZone::__construct(): Unknown or bad timezone (%.*s)",nTz,zTz);` |
|      - | 4659 | `	}` |
|    920 | 4660 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,DTZ_OFF,iOff);` |
|    920 | 4661 | `	PH7_NativeSetAttrStr(pCtx->pVm,pThis,DTZ_NAME,zName,nName);` |
|    920 | 4662 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,DTZ_KIND,iKind);` |
|    920 | 4663 | `	DtSetInit(pCtx->pVm,pThis);` |
|    920 | 4664 | `	return PH7_OK;` |
|    497 | 4665 | `}` |
|      - | 4666 | `/* DateTimeZone::getName() */` |
|    326 | 4667 | `static int vm_builtin_DateTimeZone_getName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4668 | `{` |
|    327 | 4669 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 4670 | `	const char *zName;` |
|      - | 4671 | `	int nName;` |
|    163 | 4672 | `	SXUNUSED(nArg);` |
|    163 | 4673 | `	SXUNUSED(apArg);` |
|    327 | 4674 | `	if( pThis == 0 ){` |
|      5 | 4675 | `		return PH7_OK;` |
|      - | 4676 | `	}` |
|    323 | 4677 | `	PH7_NativeAttrStr(pThis,DTZ_NAME,&zName,&nName);` |
|    323 | 4678 | `	ph7_result_string(pCtx,zName,nName);` |
|    323 | 4679 | `	return PH7_OK;` |
|    164 | 4680 | `}` |
|      - | 4681 | `/* DateTimeZone::getOffset(DateTimeInterface $datetime) */` |
|    126 | 4682 | `static int vm_builtin_DateTimeZone_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4683 | `{` |
|    127 | 4684 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 4685 | `	/* php screens the date it is handed even though a fixed offset does not` |
|      - | 4686 | `	 * depend on the instant. */` |
|    127 | 4687 | `	if( pThis == 0 \|\| (nArg > 0 && DtArgInit(pCtx,apArg[0]) != 0) ){` |
|      5 | 4688 | `		return PH7_OK;` |
|      - | 4689 | `	}` |
|      - | 4690 | `	/* Fixed-offset zones only, so the instant does not change the answer. */` |
|    123 | 4691 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DTZ_OFF));` |
|    123 | 4692 | `	return PH7_OK;` |
|     64 | 4693 | `}` |
|      - | 4694 | `/* DateTime::__construct(string $datetime = 'now', ?DateTimeZone $timezone = null) */` |
|   2696 | 4695 | `static int vm_builtin_DateTime_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4696 | `{` |
|   2699 | 4697 | `	ph7_vm *pVm = pCtx->pVm;` |
|   2699 | 4698 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */` |
|   2699 | 4699 | `	const char *zIn = "now",*zZone;` |
|   2699 | 4700 | `	int nIn = 3,nZone;` |
|   2699 | 4701 | `	sxi32 iZoneOff = 0;` |
|      - | 4702 | `	dt_state sState;` |
|      - | 4703 | `	char zNameBuf[16];` |
|      - | 4704 | `	const char *zErr;` |
|      - | 4705 | `	int iPos;` |
|      - | 4706 | `	char cAt;` |
|      - | 4707 | `	int iZoneKind;` |
|   2699 | 4708 | `	if( pThis == 0 ){` |
|    ! 0 | 4709 | `		return PH7_OK;` |
|      - | 4710 | `	}` |
|   2699 | 4711 | `	zZone = pVm->zDefTz;` |
|   2699 | 4712 | `	nZone = (int)pVm->nDefTz;` |
|   2699 | 4713 | `	iZoneKind = DT_ZONE_DEFAULT_KIND;` |
|   2699 | 4714 | `	if( nArg > 0 ){` |
|   2675 | 4715 | `		zIn = ph7_value_to_string(apArg[0],&nIn);` |
|   1336 | 4716 | `	}` |
|   2699 | 4717 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    366 | 4718 | `		if( DtZoneArgInit(pCtx,apArg[1]) != 0 ){` |
|      5 | 4719 | `			return PH7_OK;` |
|      - | 4720 | `		}` |
|    362 | 4721 | `		DtZoneOf(apArg[1],&iZoneOff,&zZone,&nZone,&iZoneKind);` |
|    180 | 4722 | `	}` |
|   2692 | 4723 | `	if( DtInitState(pCtx,zIn,nIn,iZoneOff,zZone,nZone,iZoneKind,&sState,zNameBuf,` |
|   1349 | 4724 | `		sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0 ){` |
|    427 | 4725 | `		return PH7_VmThrowException(pCtx,"DateMalformedStringException",` |
|      - | 4726 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    142 | 4727 | `			DtCStrLen(zIn,nIn),zIn,iPos,cAt,zErr);` |
|      - | 4728 | `	}` |
|   2411 | 4729 | `	DtStore(pVm,pThis,&sState);` |
|   2411 | 4730 | `	DtSetInit(pVm,pThis);` |
|   2411 | 4731 | `	return PH7_OK;` |
|   1351 | 4732 | `}` |
|      - | 4733 | ``/* One date object's `format()`, shared with the date_format() alias. */`` |
|   3086 | 4734 | `static void DtFormatOf(ph7_context *pCtx,ph7_class_instance *pObj,const char *zFmt,int nFmt)` |
|      3 | 4735 | `{` |
|      - | 4736 | `	dt_state sState;` |
|      - | 4737 | `	Sytm sTm;` |
|      - | 4738 | `	char zZone[64];` |
|      - | 4739 | `	int nName;` |
|   3089 | 4740 | `	DtLoad(pObj,&sState);` |
|   3089 | 4741 | `	nName = sState.nName;` |
|   3089 | 4742 | `	if( nName >= (int)sizeof(zZone) ){` |
|    ! 0 | 4743 | `		nName = (int)sizeof(zZone) - 1;` |
|    ! 0 | 4744 | `	}` |
|   3089 | 4745 | `	SyMemcpy(sState.zName,zZone,(sxu32)nName);` |
|   3089 | 4746 | `	zZone[nName] = 0;` |
|   3089 | 4747 | `	DtFillSytm(sState.iTs,sState.iOff,zZone,&sTm);` |
|   3089 | 4748 | `	DateFormat(pCtx,zFmt,nFmt,&sTm,sState.uSec);` |
|   3089 | 4749 | `}` |
|      - | 4750 | `/* DateTime::format(string $format) */` |
|   3086 | 4751 | `static int vm_builtin_DateTime_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4752 | `{` |
|   3089 | 4753 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 4754 | `	const char *zFmt;` |
|      - | 4755 | `	int nFmt;` |
|   3089 | 4756 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|     11 | 4757 | `		return PH7_OK;` |
|      - | 4758 | `	}` |
|   3079 | 4759 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|   3079 | 4760 | `	DtFormatOf(pCtx,pThis,zFmt,nFmt);` |
|   3079 | 4761 | `	return PH7_OK;` |
|   1546 | 4762 | `}` |
|      - | 4763 | `/* DateTime::getTimestamp() / getMicrosecond() / getOffset() */` |
|     12 | 4764 | `static int vm_builtin_DateTime_getTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4765 | `{` |
|     13 | 4766 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      6 | 4767 | `	SXUNUSED(nArg);` |
|      6 | 4768 | `	SXUNUSED(apArg);` |
|     13 | 4769 | `	if( pThis ){` |
|     11 | 4770 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_TS));` |
|      5 | 4771 | `	}` |
|     13 | 4772 | `	return PH7_OK;` |
|      1 | 4773 | `}` |
|     14 | 4774 | `static int vm_builtin_DateTime_getMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4775 | `{` |
|     15 | 4776 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      7 | 4777 | `	SXUNUSED(nArg);` |
|      7 | 4778 | `	SXUNUSED(apArg);` |
|     15 | 4779 | `	if( pThis ){` |
|     13 | 4780 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_US));` |
|      6 | 4781 | `	}` |
|     15 | 4782 | `	return PH7_OK;` |
|      1 | 4783 | `}` |
|      6 | 4784 | `static int vm_builtin_DateTime_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4785 | `{` |
|      7 | 4786 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      3 | 4787 | `	SXUNUSED(nArg);` |
|      3 | 4788 | `	SXUNUSED(apArg);` |
|      7 | 4789 | `	if( pThis ){` |
|      5 | 4790 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_OFF));` |
|      2 | 4791 | `	}` |
|      7 | 4792 | `	return PH7_OK;` |
|      1 | 4793 | `}` |
|      - | 4794 | `/* The zone object of a date, built from its stored name and offset, so an` |
|      - | 4795 | ` * identifier PHL stored but cannot re-parse still round-trips. Shared with the` |
|      - | 4796 | ` * date_timezone_get() alias. */` |
|    204 | 4797 | `static int DtTimezoneResult(ph7_context *pCtx,ph7_class_instance *pObj)` |
|      1 | 4798 | `{` |
|    205 | 4799 | `	ph7_vm *pVm = pCtx->pVm;` |
|    205 | 4800 | `	ph7_class *pZoneClass = DtClass(pVm,"DateTimeZone");` |
|      - | 4801 | `	ph7_class_instance *pZone;` |
|      - | 4802 | `	const char *zName;` |
|      - | 4803 | `	int nName;` |
|    205 | 4804 | `	if( pZoneClass == 0 ){` |
|    ! 0 | 4805 | `		return PH7_OK;` |
|      - | 4806 | `	}` |
|    205 | 4807 | `	pZone = DtNewInstance(pVm,pZoneClass);` |
|    205 | 4808 | `	if( pZone == 0 ){` |
|    ! 0 | 4809 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4810 | `	}` |
|    205 | 4811 | `	PH7_NativeAttrStr(pObj,DT_NAME,&zName,&nName);` |
|    205 | 4812 | `	PH7_NativeSetAttrInt(pVm,pZone,DTZ_OFF,PH7_NativeAttrInt(pObj,DT_OFF));` |
|    205 | 4813 | `	PH7_NativeSetAttrStr(pVm,pZone,DTZ_NAME,zName,nName);` |
|    205 | 4814 | `	PH7_NativeSetAttrInt(pVm,pZone,DTZ_KIND,DtZoneKindOf(pObj,DT_ZKIND,zName,nName));` |
|    205 | 4815 | `	PH7_NativeResultObject(pCtx,pZone);` |
|    205 | 4816 | `	return PH7_OK;` |
|    103 | 4817 | `}` |
|      - | 4818 | `/* DateTime::getTimezone() */` |
|    204 | 4819 | `static int vm_builtin_DateTime_getTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4820 | `{` |
|    205 | 4821 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|    102 | 4822 | `	SXUNUSED(nArg);` |
|    102 | 4823 | `	SXUNUSED(apArg);` |
|    205 | 4824 | `	if( pThis == 0 ){` |
|      3 | 4825 | `		return PH7_OK;` |
|      - | 4826 | `	}` |
|    203 | 4827 | `	return DtTimezoneResult(pCtx,pThis);` |
|    103 | 4828 | `}` |
|      - | 4829 | `/* The truth of a value, without converting the caller's copy of it. */` |
|      4 | 4830 | `static int DtValueTruth(ph7_vm *pVm,ph7_value *pVal)` |
|      1 | 4831 | `{` |
|      - | 4832 | `	ph7_value sTmp;` |
|      - | 4833 | `	int bRes;` |
|      5 | 4834 | `	PH7_MemObjInit(&(*pVm),&sTmp);` |
|      5 | 4835 | `	PH7_MemObjStore(pVal,&sTmp);` |
|      - | 4836 | `	/* PH7_MemObjToBool converts IN PLACE and returns a STATUS: the answer is in` |
|      - | 4837 | `	 * x.iVal (reading the return is a silent always-false). */` |
|      5 | 4838 | `	PH7_MemObjToBool(&sTmp);` |
|      5 | 4839 | `	bRes = sTmp.x.iVal != 0;` |
|      5 | 4840 | `	PH7_MemObjRelease(&sTmp);` |
|      5 | 4841 | `	return bRes;` |
|      1 | 4842 | `}` |
|      - | 4843 | `/* The DateInterval two dates differ by. Shared with the date_diff() alias. */` |
|     66 | 4844 | `static int DtDiffResult(ph7_context *pCtx,ph7_class_instance *pBase,` |
|      - | 4845 | `	ph7_class_instance *pTarget,int bAbsolute)` |
|      1 | 4846 | `{` |
|     67 | 4847 | `	ph7_vm *pVm = pCtx->pVm;` |
|     67 | 4848 | `	ph7_class *pIvClass = DtClass(pVm,"DateInterval");` |
|      - | 4849 | `	ph7_class_instance *pIv;` |
|      - | 4850 | `	dt_diff sDiff;` |
|     67 | 4851 | `	if( pIvClass == 0 ){` |
|    ! 0 | 4852 | `		return PH7_OK;` |
|      - | 4853 | `	}` |
|    133 | 4854 | `	DtCivilDiff(PH7_NativeAttrInt(pBase,DT_TS),(int)PH7_NativeAttrInt(pBase,DT_US),` |
|     66 | 4855 | `		(sxi32)PH7_NativeAttrInt(pBase,DT_OFF),` |
|     66 | 4856 | `		PH7_NativeAttrInt(pTarget,DT_TS),(int)PH7_NativeAttrInt(pTarget,DT_US),&sDiff);` |
|     67 | 4857 | `	pIv = DtNewInstance(pVm,pIvClass);` |
|     67 | 4858 | `	if( pIv == 0 ){` |
|    ! 0 | 4859 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4860 | `	}` |
|     67 | 4861 | `	PH7_NativeSetAttrInt(pVm,pIv,"y",sDiff.y);` |
|     67 | 4862 | `	PH7_NativeSetAttrInt(pVm,pIv,"m",sDiff.m);` |
|     67 | 4863 | `	PH7_NativeSetAttrInt(pVm,pIv,"d",sDiff.d);` |
|     67 | 4864 | `	PH7_NativeSetAttrInt(pVm,pIv,"h",sDiff.h);` |
|     67 | 4865 | `	PH7_NativeSetAttrInt(pVm,pIv,"i",sDiff.i);` |
|     67 | 4866 | `	PH7_NativeSetAttrInt(pVm,pIv,"s",sDiff.s);` |
|     67 | 4867 | `	DtIvSetUsec(pVm,pIv,sDiff.uSec);` |
|     67 | 4868 | `	PH7_NativeSetAttrInt(pVm,pIv,"days",sDiff.nDays);` |
|     67 | 4869 | `	PH7_NativeSetAttrInt(pVm,pIv,"invert",bAbsolute ? 0 : sDiff.bInvert);` |
|     67 | 4870 | `	PH7_NativeResultObject(pCtx,pIv);` |
|     67 | 4871 | `	return PH7_OK;` |
|     34 | 4872 | `}` |
|      - | 4873 | `/* DateTime::diff(DateTimeInterface $targetObject, bool $absolute = false) */` |
|     64 | 4874 | `static int vm_builtin_DateTime_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4875 | `{` |
|     65 | 4876 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 4877 | `	ph7_class_instance *pTarget;` |
|     65 | 4878 | `	int bAbsolute = 0;` |
|     64 | 4879 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|     63 | 4880 | `	 \|\| DtArgInit(pCtx,apArg[0]) != 0 ){` |
|      5 | 4881 | `		return PH7_OK;` |
|      - | 4882 | `	}` |
|     61 | 4883 | `	pTarget = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     61 | 4884 | `	if( nArg > 1 ){` |
|      5 | 4885 | `		bAbsolute = DtValueTruth(pCtx->pVm,apArg[1]);` |
|      2 | 4886 | `	}` |
|     61 | 4887 | `	return DtDiffResult(pCtx,pThis,pTarget,bAbsolute);` |
|     33 | 4888 | `}` |
|      - | 4889 | `/* DateTime::modify(string $modifier) */` |
|   1368 | 4890 | `static int vm_builtin_DateTime_modify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4891 | `{` |
|   1371 | 4892 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1371 | 4893 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 4894 | `	ph7_class_instance *pTarget;` |
|      - | 4895 | `	const char *zMod,*zErr;` |
|   1371 | 4896 | `	int nMod,iPos,bCopy = 0,iErrPos;` |
|      - | 4897 | `	char cAt;` |
|   1371 | 4898 | `	sxi64 iTs = 0;` |
|   1371 | 4899 | `	sxi32 iOff = 0;` |
|   1371 | 4900 | `	int bOffSet = 0,uSec = 0;` |
|      - | 4901 | `	dt_parsed sVec;` |
|   1371 | 4902 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|      5 | 4903 | `		return PH7_OK;` |
|      - | 4904 | `	}` |
|   1367 | 4905 | `	zMod = ph7_value_to_string(apArg[0],&nMod);` |
|      - | 4906 | `	/* php's modify() writes only the fields the string really SET, so a modifier` |
|      - | 4907 | `	 * that names no time of day keeps the receiver's -- DT_PARSE_OVERRIDE_TIME is` |
|      - | 4908 | `	 * php's own flag for exactly that, and the constructor's parse does not pass it. */` |
|   2049 | 4909 | `	iErrPos = DtParseEx(zMod,nMod,PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),` |
|   1364 | 4910 | `		(int)PH7_NativeAttrInt(pThis,DT_US),DT_PARSE_OVERRIDE_TIME\|DT_PARSE_KEEP_ZONE,` |
|    682 | 4911 | `		&iTs,&iOff,&bOffSet,&uSec,&sVec,&pVm->sDtLastErr);` |
|   1367 | 4912 | `	if( iErrPos != 0 ){` |
|    290 | 4913 | `		int bImm = DtIsImmutable(pVm,pThis);` |
|    290 | 4914 | `		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);` |
|    434 | 4915 | `		return PH7_VmThrowException(pCtx,"DateMalformedStringException",` |
|      - | 4916 | `			"%s::modify(): Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    144 | 4917 | `			bImm ? "DateTimeImmutable" : "DateTime",DtCStrLen(zMod,nMod),zMod,iPos,cAt,zErr);` |
|      - | 4918 | `	}` |
|   1079 | 4919 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|   1079 | 4920 | `	PH7_NativeSetAttrInt(pVm,pTarget,DT_TS,iTs);` |
|      - | 4921 | ``	/* The modifier may have moved the SUB-SECOND clock too (`+1 microsecond`,`` |
|      - | 4922 | ``	 * `+250 ms`) or set it outright (a time of day with a fraction); the parse`` |
|      - | 4923 | `	 * started from the object's own, so this is the whole answer either way. */` |
|   1079 | 4924 | `	PH7_NativeSetAttrInt(pVm,pTarget,DT_US,uSec);` |
|   1079 | 4925 | `	DtEpochRezone(pVm,pTarget,&sVec);` |
|   1079 | 4926 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|   1079 | 4927 | `	return PH7_OK;` |
|    687 | 4928 | `}` |
|      - | 4929 | `/* DateTime::setTimestamp(int $timestamp) — php clears the microseconds with it */` |
|     18 | 4930 | `static int vm_builtin_DateTime_setTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4931 | `{` |
|     19 | 4932 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 4933 | `	ph7_class_instance *pTarget;` |
|     19 | 4934 | `	int bCopy = 0;` |
|     19 | 4935 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|      3 | 4936 | `		return PH7_OK;` |
|      - | 4937 | `	}` |
|     17 | 4938 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     17 | 4939 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_TS,ph7_value_to_int64(apArg[0]));` |
|     17 | 4940 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_US,0);` |
|     17 | 4941 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     17 | 4942 | `	return PH7_OK;` |
|     10 | 4943 | `}` |
|      - | 4944 | `/* DateTime::setMicrosecond(int $microsecond) */` |
|     44 | 4945 | `static int vm_builtin_DateTime_setMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4946 | `{` |
|     45 | 4947 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 4948 | `	ph7_class_instance *pTarget;` |
|      - | 4949 | `	sxi64 iUs;` |
|     45 | 4950 | `	int bCopy = 0;` |
|     45 | 4951 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|      3 | 4952 | `		return PH7_OK;` |
|      - | 4953 | `	}` |
|     43 | 4954 | `	iUs = ph7_value_to_int64(apArg[0]);` |
|     43 | 4955 | `	if( iUs < 0 \|\| iUs > 999999 ){` |
|      - | 4956 | `		/* php's range refusal, and the reason a date's microseconds can be` |
|      - | 4957 | `		 * assumed to be a fraction of ONE second everywhere else: PHL stored` |
|      - | 4958 | ``		 * whatever int it was handed, so `setMicrosecond(1000000)` formatted as`` |
|      - | 4959 | ``		 * `00:00:00.1000000` and a negative one as `00:00:00.-00001` -- neither`` |
|      - | 4960 | `		 * of them a time. The message names the DECLARING class, so a subclass` |
|      - | 4961 | `		 * of DateTime still reports DateTime. */` |
|     40 | 4962 | `		return PH7_VmThrowException(pCtx,"DateRangeError",` |
|      - | 4963 | `			"%s::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, %qd given",` |
|     26 | 4964 | `			DtIsImmutable(pCtx->pVm,pThis) ? "DateTimeImmutable" : "DateTime",iUs);` |
|      - | 4965 | `	}` |
|     17 | 4966 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     17 | 4967 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_US,iUs);` |
|     17 | 4968 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     17 | 4969 | `	return PH7_OK;` |
|     23 | 4970 | `}` |
|      - | 4971 | `/* DateTime::setTimezone(DateTimeZone $timezone) */` |
|      8 | 4972 | `static int vm_builtin_DateTime_setTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4973 | `{` |
|      9 | 4974 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 4975 | `	ph7_class_instance *pTarget;` |
|      9 | 4976 | `	const char *zName = "UTC";` |
|      9 | 4977 | `	int nName = 3,bCopy = 0,iKind = DT_ZONE_ID;` |
|      9 | 4978 | `	sxi32 iOff = 0;` |
|      9 | 4979 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|      3 | 4980 | `		return PH7_OK;` |
|      - | 4981 | `	}` |
|      7 | 4982 | `	if( !DtZoneOf(apArg[0],&iOff,&zName,&nName,&iKind) ){` |
|    ! 0 | 4983 | `		return PH7_OK;` |
|      - | 4984 | `	}` |
|      7 | 4985 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|      7 | 4986 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_OFF,iOff);` |
|      7 | 4987 | `	PH7_NativeSetAttrStr(pCtx->pVm,pTarget,DT_NAME,zName,nName);` |
|      7 | 4988 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_ZKIND,iKind);` |
|      7 | 4989 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|      7 | 4990 | `	return PH7_OK;` |
|      5 | 4991 | `}` |
|      - | 4992 | `/* Replace the DATE of an object, keeping its time of day (the offset it is` |
|      - | 4993 | ` * expressed in never changes). Shared with the date_date_set() alias. */` |
|     46 | 4994 | `static void DtSetDateOf(ph7_context *pCtx,ph7_class_instance *pObj,sxi64 y,int mo,int d)` |
|      1 | 4995 | `{` |
|     47 | 4996 | `	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);` |
|     47 | 4997 | `	sxi64 iDays = DtFloorDiv(iLocal,86400);` |
|     47 | 4998 | `	sxi64 iSecs = iLocal - iDays*86400;` |
|     70 | 4999 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,` |
|     69 | 5000 | `		DtMakeTs(y,mo,d,(int)(iSecs / 3600),(int)((iSecs / 60) % 60),(int)(iSecs % 60),` |
|     46 | 5001 | `			(sxi32)PH7_NativeAttrInt(pObj,DT_OFF)));` |
|     47 | 5002 | `}` |
|      - | 5003 | `/* Replace the TIME of day, keeping the date. Shared with date_time_set(). */` |
|      8 | 5004 | `static void DtSetTimeOf(ph7_context *pCtx,ph7_class_instance *pObj,int h,int mi,int s,sxi64 uSec)` |
|      1 | 5005 | `{` |
|      9 | 5006 | `	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);` |
|      9 | 5007 | `	sxi64 iDays = DtFloorDiv(iLocal,86400);` |
|      - | 5008 | `	sxi64 y;` |
|      - | 5009 | `	int mo,d;` |
|      9 | 5010 | `	DtCivilFromDays(iDays,&y,&mo,&d);` |
|      9 | 5011 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,DtMakeTs(y,mo,d,h,mi,s,(sxi32)PH7_NativeAttrInt(pObj,DT_OFF)));` |
|      9 | 5012 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,uSec);` |
|      9 | 5013 | `}` |
|      - | 5014 | `/* DateTime::setDate(int $year, int $month, int $day) — the time of day is kept */` |
|     48 | 5015 | `static int vm_builtin_DateTime_setDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5016 | `{` |
|     49 | 5017 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5018 | `	ph7_class_instance *pTarget;` |
|     49 | 5019 | `	int bCopy = 0;` |
|     49 | 5020 | `	if( pThis == 0 \|\| nArg < 3 ){` |
|      3 | 5021 | `		return PH7_OK;` |
|      - | 5022 | `	}` |
|     47 | 5023 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     70 | 5024 | `	DtSetDateOf(pCtx,pTarget,ph7_value_to_int64(apArg[0]),ph7_value_to_int(apArg[1]),` |
|     46 | 5025 | `		ph7_value_to_int(apArg[2]));` |
|     47 | 5026 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     47 | 5027 | `	return PH7_OK;` |
|     25 | 5028 | `}` |
|      - | 5029 | `/* DateTime::setTime(int $hour, int $minute, int $second = 0, int $microsecond = 0) */` |
|     10 | 5030 | `static int vm_builtin_DateTime_setTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5031 | `{` |
|     11 | 5032 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5033 | `	ph7_class_instance *pTarget;` |
|     11 | 5034 | `	int bCopy = 0;` |
|     11 | 5035 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|      3 | 5036 | `		return PH7_OK;` |
|      - | 5037 | `	}` |
|      9 | 5038 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     18 | 5039 | `	DtSetTimeOf(pCtx,pTarget,ph7_value_to_int(apArg[0]),ph7_value_to_int(apArg[1]),` |
|      7 | 5040 | `		nArg > 2 ? ph7_value_to_int(apArg[2]) : 0,` |
|      6 | 5041 | `		nArg > 3 ? ph7_value_to_int64(apArg[3]) : 0);` |
|      9 | 5042 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|      9 | 5043 | `	return PH7_OK;` |
|      6 | 5044 | `}` |
|      - | 5045 | `/* DateTime::setISODate(int $year, int $week, int $dayOfWeek = 1) */` |
|      8 | 5046 | `static int vm_builtin_DateTime_setISODate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5047 | `{` |
|      9 | 5048 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5049 | `	ph7_class_instance *pTarget;` |
|      9 | 5050 | `	int bCopy = 0;` |
|      9 | 5051 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|      3 | 5052 | `		return PH7_OK;` |
|      - | 5053 | `	}` |
|      7 | 5054 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     16 | 5055 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_TS,` |
|      9 | 5056 | `		DtIsoDate(PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),` |
|      6 | 5057 | `			ph7_value_to_int64(apArg[0]),ph7_value_to_int64(apArg[1]),` |
|      5 | 5058 | `			nArg > 2 ? ph7_value_to_int64(apArg[2]) : 1));` |
|      7 | 5059 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|      7 | 5060 | `	return PH7_OK;` |
|      5 | 5061 | `}` |
|      - | 5062 | `/*` |
|      - | 5063 | ` * One interval applied to a date -- the whole of what add(), sub(), their two` |
|      - | 5064 | ` * procedural aliases and the DatePeriod walk each did by hand.` |
|      - | 5065 | ` *` |
|      - | 5066 | `` * The MICROSECONDS are php's `f`, and php's `f` is a signed count of SECONDS'`` |
|      - | 5067 | ` * fractions that moves the clock like any other field: an interval carrying` |
|      - | 5068 | ` * f = 2.5 and nothing else moves it two and a half seconds, and its carry into` |
|      - | 5069 | ` * the second is ordinary floor division (so a sub() past the second borrows).` |
|      - | 5070 | `` * PHL ignored `f` at all four sites, which left `DatePeriod` over a sub-second`` |
|      - | 5071 | ` * interval standing STILL -- every step answering the start date.` |
|      - | 5072 | ` *` |
|      - | 5073 | `` * The arithmetic is done on unsigned intermediates: `f` is a whole int64 count`` |
|      - | 5074 | ` * of microseconds a script may write anything into, and the sum of two of them` |
|      - | 5075 | ` * is exactly the wrap php's own C arrives at rather than an overflow this build` |
|      - | 5076 | ` * would trap on.` |
|      - | 5077 | ` *` |
|      - | 5078 | `` * iSign is the FINAL direction, `invert` already folded in by whichever caller`` |
|      - | 5079 | ` * honours it -- add()/sub() and their aliases do, and the period walk does not` |
|      - | 5080 | ` * (php's own split, below).` |
|      - | 5081 | ` */` |
|    346 | 5082 | `static void DtApplyInterval(ph7_vm *pVm,ph7_class_instance *pSrc,ph7_class_instance *pDst,` |
|      - | 5083 | `	ph7_class_instance *pIv,int iSign)` |
|      1 | 5084 | `{` |
|      - | 5085 | `	sxi64 iUsIv,iUs,iCarry;` |
|    347 | 5086 | `	iUsIv = DtIvUsec(pIv);` |
|    347 | 5087 | `	if( iSign < 0 ){` |
|     41 | 5088 | `		iUsIv = (sxi64)((sxu64)0 - (sxu64)iUsIv);` |
|     20 | 5089 | `	}` |
|    347 | 5090 | `	iUs = (sxi64)((sxu64)PH7_NativeAttrInt(pSrc,DT_US) + (sxu64)iUsIv);` |
|    347 | 5091 | `	iCarry = DtFloorDiv(iUs,1000000);` |
|    520 | 5092 | `	PH7_NativeSetAttrInt(pVm,pDst,DT_TS,` |
|    519 | 5093 | `		(sxi64)((sxu64)DtCivilAdd(PH7_NativeAttrInt(pSrc,DT_TS),(sxi32)PH7_NativeAttrInt(pSrc,DT_OFF),` |
|    173 | 5094 | `			PH7_NativeAttrInt(pIv,"y"),PH7_NativeAttrInt(pIv,"m"),PH7_NativeAttrInt(pIv,"d"),` |
|    173 | 5095 | `			PH7_NativeAttrInt(pIv,"h"),PH7_NativeAttrInt(pIv,"i"),PH7_NativeAttrInt(pIv,"s"),iSign)` |
|    346 | 5096 | `			+ (sxu64)iCarry));` |
|    520 | 5097 | `	PH7_NativeSetAttrInt(pVm,pDst,DT_US,` |
|    346 | 5098 | `		(sxi64)((sxu64)iUs - (sxu64)iCarry * 1000000));` |
|    347 | 5099 | `}` |
|      - | 5100 | `/* add()/sub(): one body, the sign is the difference. */` |
|     90 | 5101 | `static int DtAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)` |
|      1 | 5102 | `{` |
|     91 | 5103 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5104 | `	ph7_class_instance *pTarget,*pIv;` |
|     91 | 5105 | `	int bCopy = 0;` |
|     90 | 5106 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|     87 | 5107 | `	 \|\| DtArgInit(pCtx,apArg[0]) != 0 ){` |
|      9 | 5108 | `		return PH7_OK;` |
|      - | 5109 | `	}` |
|     83 | 5110 | `	pIv = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     83 | 5111 | `	if( PH7_NativeAttrInt(pIv,"invert") ){` |
|     17 | 5112 | `		iSign = -iSign;   /* an inverted interval subtracts from add() (php) */` |
|      8 | 5113 | `	}` |
|     83 | 5114 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     83 | 5115 | `	DtApplyInterval(pCtx->pVm,pThis,pTarget,pIv,iSign);` |
|     83 | 5116 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     83 | 5117 | `	return PH7_OK;` |
|     46 | 5118 | `}` |
|     54 | 5119 | `static int vm_builtin_DateTime_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5120 | `{` |
|     55 | 5121 | `	return DtAddSub(pCtx,nArg,apArg,1);` |
|      1 | 5122 | `}` |
|     36 | 5123 | `static int vm_builtin_DateTime_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5124 | `{` |
|     37 | 5125 | `	return DtAddSub(pCtx,nArg,apArg,-1);` |
|      1 | 5126 | `}` |
|      - | 5127 | ``/* DateTime::getLastErrors() — php's array, or `false` when the last parse was clean */`` |
|    502 | 5128 | `static int vm_builtin_DateTime_getLastErrors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5129 | `{` |
|    503 | 5130 | `	ph7_vm *pVm = pCtx->pVm;` |
|    503 | 5131 | `	phl_dt_lasterr *pErr = &pVm->sDtLastErr;` |
|      - | 5132 | `	ph7_value *pArr,*pWarn,*pErrs,*pVal;` |
|      - | 5133 | `	int k;` |
|    251 | 5134 | `	SXUNUSED(nArg);` |
|    251 | 5135 | `	SXUNUSED(apArg);` |
|    503 | 5136 | `	if( !pErr->bSet ){` |
|    259 | 5137 | `		ph7_result_bool(pCtx,0);` |
|    259 | 5138 | `		return PH7_OK;` |
|      - | 5139 | `	}` |
|    245 | 5140 | `	pArr = ph7_context_new_array(pCtx);` |
|    245 | 5141 | `	pWarn = ph7_context_new_array(pCtx);` |
|    245 | 5142 | `	pErrs = ph7_context_new_array(pCtx);` |
|    245 | 5143 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    245 | 5144 | `	if( pArr == 0 \|\| pWarn == 0 \|\| pErrs == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5145 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5146 | `	}` |
|    317 | 5147 | `	for( k = 0 ; k < pErr->nWarnKept ; k++ ){` |
|     73 | 5148 | `		ph7_value_string(pVal,pErr->azWarn[k],-1);` |
|     73 | 5149 | `		ph7_array_add_intkey_elem(pWarn,pErr->aWarnPos[k],pVal);` |
|     73 | 5150 | `		ph7_value_reset_string_cursor(pVal);` |
|     37 | 5151 | `	}` |
|      - | 5152 | `	{` |
|    245 | 5153 | `		const phl_dt_diag_row *aRow = (const phl_dt_diag_row *)SyBlobData(&pErr->sErr);` |
|    483 | 5154 | `		for( k = 0 ; k < pErr->nErrKept ; k++ ){` |
|    239 | 5155 | `			ph7_value_string(pVal,aRow[k].zMsg,-1);` |
|    239 | 5156 | `			ph7_array_add_intkey_elem(pErrs,aRow[k].iPos,pVal);` |
|    239 | 5157 | `			ph7_value_reset_string_cursor(pVal);` |
|    120 | 5158 | `		}` |
|      - | 5159 | `	}` |
|    245 | 5160 | `	ph7_value_int(pVal,pErr->nWarn);` |
|    245 | 5161 | `	ph7_array_add_strkey_elem(pArr,"warning_count",pVal);` |
|    245 | 5162 | `	ph7_array_add_strkey_elem(pArr,"warnings",pWarn);` |
|    245 | 5163 | `	ph7_value_int(pVal,pErr->nErr);` |
|    245 | 5164 | `	ph7_array_add_strkey_elem(pArr,"error_count",pVal);` |
|    245 | 5165 | `	ph7_array_add_strkey_elem(pArr,"errors",pErrs);` |
|    245 | 5166 | `	ph7_result_value(pCtx,pArr);` |
|    245 | 5167 | `	return PH7_OK;` |
|    252 | 5168 | `}` |
|      - | 5169 | `/*` |
|      - | 5170 | ` * The class a static factory builds. php uses LATE STATIC BINDING here, so` |
|      - | 5171 | `` * `D::createFromFormat()` on a subclass answers a D — where the chunk hardcoded`` |
|      - | 5172 | ` * the literal class name and always answered a DateTime.` |
|      - | 5173 | ` */` |
|    802 | 5174 | `static ph7_class * DtFactoryClass(ph7_context *pCtx,const char *zFallback)` |
|      2 | 5175 | `{` |
|    804 | 5176 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|    804 | 5177 | `	return pClass ? pClass : DtClass(pCtx->pVm,zFallback);` |
|      2 | 5178 | `}` |
|      - | 5179 | `/* DateTime::createFromFormat(string $format, string $datetime, ?DateTimeZone $timezone = null) */` |
|    514 | 5180 | `static int DtCreateFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)` |
|      1 | 5181 | `{` |
|    515 | 5182 | `	ph7_vm *pVm = pCtx->pVm;` |
|    515 | 5183 | `	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);` |
|      - | 5184 | `	ph7_class_instance *pObj;` |
|      - | 5185 | `	dt_ff_res sRes;` |
|      - | 5186 | `	dt_state sState;` |
|      - | 5187 | `	int iZoneKind;` |
|      - | 5188 | `	char zNameBuf[16];` |
|      - | 5189 | `	const char *zZone;` |
|      - | 5190 | `	int nZone;` |
|    515 | 5191 | `	sxi32 iZoneOff = 0;` |
|      - | 5192 | `	const char *zFmt,*zIn;` |
|    515 | 5193 | `	int nFmt,nIn,iNowUs = 0,iResUs = 0;` |
|    515 | 5194 | `	sxi64 iNowFf = 0;` |
|    515 | 5195 | `	if( pClass == 0 \|\| nArg < 2 ){` |
|    ! 0 | 5196 | `		return PH7_OK;` |
|      - | 5197 | `	}` |
|    515 | 5198 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    515 | 5199 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    515 | 5200 | `	zZone = pVm->zDefTz;` |
|    515 | 5201 | `	nZone = (int)pVm->nDefTz;` |
|    515 | 5202 | `	iZoneKind = DT_ZONE_DEFAULT_KIND;` |
|    515 | 5203 | `	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    467 | 5204 | `		if( DtZoneArgInit(pCtx,apArg[2]) != 0 ){` |
|      3 | 5205 | `			return PH7_OK;` |
|      - | 5206 | `		}` |
|    465 | 5207 | `		DtZoneOf(apArg[2],&iZoneOff,&zZone,&nZone,&iZoneKind);` |
|    232 | 5208 | `	}` |
|    513 | 5209 | `	DtNowUs(pCtx->pVm,&iNowFf,&iNowUs);` |
|    513 | 5210 | `	if( DtFromFormat(zFmt,nFmt,zIn,nIn,&sRes) != 0 ){` |
|    165 | 5211 | `		DtLastErrFf(pVm,&sRes.sDiag);` |
|    165 | 5212 | `		ph7_result_bool(pCtx,0);` |
|    165 | 5213 | `		return PH7_OK;` |
|      - | 5214 | `	}` |
|    349 | 5215 | `	DtLastErrFf(pVm,&sRes.sDiag);` |
|    349 | 5216 | `	sState.iTs = DtFfResolve(&sRes,iNowFf,iNowUs,iZoneOff,&iResUs);` |
|    349 | 5217 | `	sState.uSec = iResUs;` |
|    349 | 5218 | `	if( sRes.iOffKind == 0 ){` |
|      - | 5219 | `		/* nothing the format read resolved, so the call's own zone stands */` |
|    267 | 5220 | `		sState.iOff = iZoneOff;` |
|    267 | 5221 | `		sState.zName = zZone;` |
|    267 | 5222 | `		sState.nName = nZone;` |
|    267 | 5223 | `		sState.iZoneKind = iZoneKind;` |
|    216 | 5224 | `	}else if( sRes.iOffKind == DT_ZONE_OFFSET ){` |
|     47 | 5225 | `		sState.iOff = sRes.iOff;` |
|     47 | 5226 | `		sState.nName = DtOffNameSec(zNameBuf,sizeof(zNameBuf),sRes.iOff);` |
|     47 | 5227 | `		sState.zName = zNameBuf;` |
|     47 | 5228 | `		sState.iZoneKind = DT_ZONE_OFFSET;` |
|     24 | 5229 | `	}else{` |
|     37 | 5230 | `		sState.iOff = sRes.iOff;` |
|     37 | 5231 | `		sState.zName = sRes.zName;` |
|     37 | 5232 | `		sState.nName = sRes.nName;` |
|     37 | 5233 | `		sState.iZoneKind = sRes.iOffKind;` |
|      - | 5234 | `	}` |
|    349 | 5235 | `	pObj = DtNewInstance(pVm,pClass);` |
|    349 | 5236 | `	if( pObj == 0 ){` |
|    ! 0 | 5237 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5238 | `	}` |
|    349 | 5239 | `	DtStore(pVm,pObj,&sState);` |
|    349 | 5240 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    349 | 5241 | `	return PH7_OK;` |
|    258 | 5242 | `}` |
|    498 | 5243 | `static int vm_builtin_DateTime_createFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5244 | `{` |
|    499 | 5245 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTime");` |
|      1 | 5246 | `}` |
|      2 | 5247 | `static int vm_builtin_DateTimeImmutable_createFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5248 | `{` |
|      3 | 5249 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 5250 | `}` |
|      - | 5251 | `/* createFromImmutable()/createFromMutable()/createFromInterface(): one copy body */` |
|     18 | 5252 | `static int DtCopyOf(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)` |
|      1 | 5253 | `{` |
|     19 | 5254 | `	ph7_vm *pVm = pCtx->pVm;` |
|     19 | 5255 | `	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);` |
|      - | 5256 | `	ph7_class_instance *pSrc,*pObj;` |
|      - | 5257 | `	dt_state sState;` |
|     18 | 5258 | `	if( pClass == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|     19 | 5259 | `	 \|\| DtArgInit(pCtx,apArg[0]) != 0 ){` |
|      5 | 5260 | `		return PH7_OK;` |
|      - | 5261 | `	}` |
|     15 | 5262 | `	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     15 | 5263 | `	DtLoad(pSrc,&sState);` |
|     15 | 5264 | `	pObj = DtNewInstance(pVm,pClass);` |
|     15 | 5265 | `	if( pObj == 0 ){` |
|    ! 0 | 5266 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5267 | `	}` |
|     15 | 5268 | `	DtStore(pVm,pObj,&sState);` |
|     15 | 5269 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     15 | 5270 | `	return PH7_OK;` |
|     10 | 5271 | `}` |
|      - | 5272 | `/*` |
|      - | 5273 | `` * php's rendering of the timestamp its DateRangeError names -- its own `%g`:`` |
|      - | 5274 | ` * six significant digits, and an exponent form that keeps a fractional digit,` |
|      - | 5275 | ` * so 2^63 prints "9.22337e+18" and 1e19 prints "1.0e+19". NaN and the` |
|      - | 5276 | ` * infinities print as the bare words php prints them as everywhere else.` |
|      - | 5277 | ` *` |
|      - | 5278 | ` * libc is the digit engine (the byte-exact-floats rule the printf family` |
|      - | 5279 | ` * already follows) and PH7_PhpFloatShape turns its output into php's shape.` |
|      - | 5280 | ` */` |
|     12 | 5281 | `static void DtRealText(double r,char *zBuf,int nBuf)` |
|      1 | 5282 | `{` |
|      - | 5283 | `	int n;` |
|     13 | 5284 | `	if( PH7_IS_NAN(r) ){` |
|      3 | 5285 | `		SyMemcpy("NAN",zBuf,sizeof("NAN"));` |
|      3 | 5286 | `		return;` |
|      - | 5287 | `	}` |
|     11 | 5288 | `	if( PH7_IS_INF(r) ){` |
|      5 | 5289 | `		SyMemcpy(r < 0 ? "-INF" : "INF",zBuf,r < 0 ? sizeof("-INF") : sizeof("INF"));` |
|      5 | 5290 | `		return;` |
|      - | 5291 | `	}` |
|      7 | 5292 | `	n = snprintf(zBuf,(size_t)nBuf,"%.6g",r);` |
|      7 | 5293 | `	if( n < 0 \|\| n >= nBuf - 2 ){` |
|    ! 0 | 5294 | `		zBuf[0] = 0;` |
|    ! 0 | 5295 | `		return;` |
|      - | 5296 | `	}` |
|      7 | 5297 | `	zBuf[PH7_PhpFloatShape(zBuf,n,1)] = 0;` |
|      7 | 5298 | `}` |
|      - | 5299 | `/*` |
|      - | 5300 | ` * DateTime::createFromTimestamp(int\|float $timestamp) (php 8.4), and the same` |
|      - | 5301 | ` * on DateTimeImmutable -- the float door onto the clock, and the only factory` |
|      - | 5302 | ` * that reads MICROSECONDS out of its argument.` |
|      - | 5303 | ` *` |
|      - | 5304 | ` * The zone is a fixed +00:00 whatever the default timezone is, exactly as` |
|      - | 5305 | `` * `new DateTime('@0')` answers; the seconds floor and the fraction rounds to`` |
|      - | 5306 | ` * the nearest microsecond (php's 1.9999999 is 2.000000, and its -1.9999999 is` |
|      - | 5307 | ` * -2.000000, both of which fall out of taking the floor first).` |
|      - | 5308 | ` */` |
|     60 | 5309 | `static int DtCreateFromTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)` |
|      1 | 5310 | `{` |
|     61 | 5311 | `	ph7_vm *pVm = pCtx->pVm;` |
|     61 | 5312 | `	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);` |
|      - | 5313 | `	ph7_class_instance *pObj;` |
|      - | 5314 | `	dt_state sState;` |
|      - | 5315 | `	char zNameBuf[16];` |
|     61 | 5316 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 5317 | `		return PH7_OK;` |
|      - | 5318 | `	}` |
|     61 | 5319 | `	sState.uSec = 0;` |
|      - | 5320 | `	{` |
|      - | 5321 | ``		/* Which ARM of `int\|float` the argument satisfies decides the rest, and`` |
|      - | 5322 | `		 * a numeric STRING picks its own: php reads "5" as an int and "5.5" as` |
|      - | 5323 | `		 * a float, so the SHAPE of the digits is the test rather than the` |
|      - | 5324 | `		 * value's storage. Everything that is not a number at all was refused` |
|      - | 5325 | `		 * upstream by the declared type. */` |
|     61 | 5326 | `		double r = 0.0;` |
|     61 | 5327 | `		int bReal = 0;` |
|     61 | 5328 | `		if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|     31 | 5329 | `			bReal = 1;` |
|     31 | 5330 | `			r = (double)apArg[0]->rVal;` |
|     46 | 5331 | `		}else if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|      - | 5332 | `			int nStr;` |
|      5 | 5333 | `			const char *zStr = ph7_value_to_string(apArg[0],&nStr);` |
|      - | 5334 | `			sxi64 iLong;` |
|      - | 5335 | `			double dReal;` |
|      5 | 5336 | `			if( RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal) == RANGE_IN_DOUBLE ){` |
|      3 | 5337 | `				bReal = 1;` |
|      3 | 5338 | `				r = dReal;` |
|      1 | 5339 | `			}` |
|      2 | 5340 | `		}` |
|     61 | 5341 | `		if( !bReal ){` |
|     29 | 5342 | `			sState.iTs = ph7_value_to_int64(apArg[0]);` |
|     47 | 5343 | `		}else if( !PH7_RealFitsInt64(r) ){` |
|      - | 5344 | `			/* php's own bounds, and its own words for them: the ceiling is` |
|      - | 5345 | `			 * printed as the last microsecond below 2^63 even though the test` |
|      - | 5346 | `			 * is against 2^63 itself (no double lies between the two).` |
|      - | 5347 | `			 * "%z" takes the class name as the length+pointer pair it is. */` |
|      - | 5348 | `			char zVal[64];` |
|     13 | 5349 | `			DtRealText(r,zVal,(int)sizeof(zVal));` |
|     19 | 5350 | `			return PH7_VmThrowException(pCtx,"DateRangeError",` |
|      - | 5351 | `				"%z::createFromTimestamp(): Argument #1 ($timestamp) must be a finite "` |
|      - | 5352 | `				"number between -9223372036854775808 and 9223372036854775807.999999, "` |
|      6 | 5353 | `				"%s given",&pClass->sName,zVal);` |
|    ! 0 | 5354 | `		}else{` |
|      - | 5355 | `			/* floor(), by hand: <math.h> belongs to the optional math module and` |
|      - | 5356 | `			 * the clock does not depend on it. The C cast truncates toward zero,` |
|      - | 5357 | `			 * so only a negative value with a fraction needs the step down. */` |
|      - | 5358 | `			double fFrac;` |
|     21 | 5359 | `			sState.iTs = (sxi64)r;` |
|     21 | 5360 | `			if( (double)sState.iTs > r ){` |
|      9 | 5361 | `				sState.iTs--;` |
|      4 | 5362 | `			}` |
|     21 | 5363 | `			fFrac = (r - (double)sState.iTs) * 1000000.0;` |
|     21 | 5364 | `			sState.uSec = (int)(fFrac + 0.5);` |
|     21 | 5365 | `			if( sState.uSec >= 1000000 ){` |
|      - | 5366 | `				/* The rounding carried into the second (php's 1.9999999). */` |
|      3 | 5367 | `				sState.uSec -= 1000000;` |
|      3 | 5368 | `				sState.iTs++;` |
|      1 | 5369 | `			}` |
|      - | 5370 | `		}` |
|      - | 5371 | `	}` |
|     49 | 5372 | `	sState.iOff = 0;` |
|     49 | 5373 | `	sState.nName = DtOffName(zNameBuf,sizeof(zNameBuf),0);` |
|     49 | 5374 | `	sState.zName = zNameBuf;` |
|     49 | 5375 | ``	sState.iZoneKind = DT_ZONE_OFFSET;   /* php's fixed `+00:00`, not the UTC id */`` |
|     49 | 5376 | `	pObj = DtNewInstance(pVm,pClass);` |
|     49 | 5377 | `	if( pObj == 0 ){` |
|    ! 0 | 5378 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5379 | `	}` |
|     49 | 5380 | `	DtStore(pVm,pObj,&sState);` |
|     49 | 5381 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     49 | 5382 | `	return PH7_OK;` |
|     31 | 5383 | `}` |
|     56 | 5384 | `static int vm_builtin_DateTime_createFromTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5385 | `{` |
|     57 | 5386 | `	return DtCreateFromTimestamp(pCtx,nArg,apArg,"DateTime");` |
|      1 | 5387 | `}` |
|      4 | 5388 | `static int vm_builtin_DateTimeImmutable_createFromTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5389 | `{` |
|      5 | 5390 | `	return DtCreateFromTimestamp(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 5391 | `}` |
|      8 | 5392 | `static int vm_builtin_DateTime_copyOf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5393 | `{` |
|      9 | 5394 | `	return DtCopyOf(pCtx,nArg,apArg,"DateTime");` |
|      1 | 5395 | `}` |
|     10 | 5396 | `static int vm_builtin_DateTimeImmutable_copyOf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5397 | `{` |
|     11 | 5398 | `	return DtCopyOf(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 5399 | `}` |
|      - | 5400 | `/*` |
|      - | 5401 | ` * int\|false strtotime(string $datetime, ?int $baseTimestamp = null)` |
|      - | 5402 | ` *` |
|      - | 5403 | ` * Rides the same DtParse the constructor uses, so its format coverage is identical.` |
|      - | 5404 | ` * php: the EMPTY string is false, but whitespace-only is 'now'; a parse failure is` |
|      - | 5405 | ` * false (never an exception), and the default timezone is offset 0 — exactly what` |
|      - | 5406 | ` * the constructor does for a null $timezone.` |
|      - | 5407 | ` */` |
|    702 | 5408 | `static int vm_builtin_strtotime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5409 | `{` |
|      - | 5410 | `	const char *zIn;` |
|      - | 5411 | `	int nIn;` |
|      - | 5412 | `	sxi64 iBase;` |
|    704 | 5413 | `	sxi64 iTs = 0;` |
|    704 | 5414 | `	sxi32 iOff = 0;` |
|    704 | 5415 | `	int bOffSet = 0,uSec = 0;` |
|    704 | 5416 | `	if( nArg < 1 ){` |
|    ! 0 | 5417 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5418 | `		return PH7_OK;` |
|      - | 5419 | `	}` |
|    704 | 5420 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    704 | 5421 | `	if( nIn < 1 ){` |
|      3 | 5422 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5423 | `		return PH7_OK;` |
|      - | 5424 | `	}` |
|    702 | 5425 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    622 | 5426 | `		iBase = ph7_value_to_int64(apArg[1]);` |
|    312 | 5427 | `	}else{` |
|     81 | 5428 | `		DtNowUs(pCtx->pVm,&iBase,0);` |
|      - | 5429 | `	}` |
|    702 | 5430 | `	if( DtParse(zIn,nIn,iBase,0,0,&iTs,&iOff,&bOffSet,&uSec) != 0 ){` |
|    122 | 5431 | `		ph7_result_bool(pCtx,0);` |
|    122 | 5432 | `		return PH7_OK;` |
|      - | 5433 | `	}` |
|    582 | 5434 | `	ph7_result_int64(pCtx,iTs);` |
|    582 | 5435 | `	return PH7_OK;` |
|    353 | 5436 | `}` |
|      - | 5437 | `/*` |
|      - | 5438 | ` * ---------------------------------------------------------------------------` |
|      - | 5439 | ` * DateInterval, DatePeriod and its iterator, declared from C.` |
|      - | 5440 | ` *` |
|      - | 5441 | ``  * The rest of the date chunk. DateInterval's two constructors were `preg_match` `` |
|      - | 5442 | `` * calls in PHP; DatePeriod's `getIterator()` was a PHP GENERATOR, which a C body`` |
|      - | 5443 | `` * cannot be -- so it answers a native `InternalIterator`, which is exactly the`` |
|      - | 5444 | ` * class php answers there.` |
|      - | 5445 | ` * ---------------------------------------------------------------------------` |
|      - | 5446 | ` */` |
|      - | 5447 | `/* The MICROSECOND slot of the parsed vector: php keeps it apart from the six` |
|      - | 5448 | ` * relative fields (timelib_rel_time.us), and it does not carry into the seconds` |
|      - | 5449 | ``  * the way the CLOCK's does -- `1000000 microseconds` is an interval whose `%f` `` |
|      - | 5450 | `` * prints 1000000 and whose `s` is 0. */`` |
|      - | 5451 | `#define DT_IV_FIELDS 6` |
|      - | 5452 | `#define DT_IV_USLOT  6` |
|      - | 5453 | `static const char * const azDtIvField[] = { "y", "m", "d", "h", "i", "s" };` |
|      - | 5454 | `/* Read an unsigned run of digits; returns the count consumed. Stops ACCUMULATING` |
|      - | 5455 | ` * past DT_DIGITS_SAFE while still counting, so a caller that is about to refuse` |
|      - | 5456 | ` * an over-wide run does not overflow measuring it (see DT_DIGITS_ISO). */` |
|    360 | 5457 | `static int DtIvDigits(const char *z,const char *zEnd,sxi64 *pVal)` |
|      3 | 5458 | `{` |
|    363 | 5459 | `	int n = 0;` |
|    363 | 5460 | `	sxi64 v = 0;` |
|   1295 | 5461 | `	while( &z[n] < zEnd && SyisDigit(z[n]) ){` |
|    935 | 5462 | `		if( n < DT_DIGITS_SAFE ){` |
|    935 | 5463 | `			v = v*10 + (z[n] - '0');` |
|    466 | 5464 | `		}` |
|    935 | 5465 | `		n++;` |
|      3 | 5466 | `	}` |
|    363 | 5467 | `	*pVal = v;` |
|    363 | 5468 | `	return n;` |
|      3 | 5469 | `}` |
|      - | 5470 | `/*` |
|      - | 5471 | ` * php's ISO-8601 duration grammar: P[nY][nM][nW][nD][T[nH][nM][nS]], every field` |
|      - | 5472 | ` * an unsigned integer. A bare "P", a trailing "T" and a fractional second are all` |
|      - | 5473 | ` * rejected, as php rejects them.` |
|      - | 5474 | ` */` |
|    266 | 5475 | `static int DtIvParseIso(const char *zIn,int nIn,sxi64 *aOut)` |
|      3 | 5476 | `{` |
|    269 | 5477 | `	const char *z = zIn,*zEnd = &zIn[nIn];` |
|    269 | 5478 | `	int bTime = 0,bAny = 0;` |
|      - | 5479 | `	int k;` |
|   2131 | 5480 | `	for( k = 0 ; k <= DT_IV_USLOT ; k++ ){` |
|   1865 | 5481 | `		aOut[k] = 0;` |
|    934 | 5482 | `	}` |
|    269 | 5483 | `	if( nIn < 2 \|\| zIn[0] != 'P' \|\| zIn[nIn-1] == 'T' ){` |
|     11 | 5484 | `		return -1;` |
|      - | 5485 | `	}` |
|    259 | 5486 | `	z++;` |
|    613 | 5487 | `	while( z < zEnd ){` |
|      - | 5488 | `		sxi64 v;` |
|      - | 5489 | `		int n;` |
|    377 | 5490 | `		if( z[0] == 'T' ){` |
|     76 | 5491 | `			if( bTime ){` |
|    ! 0 | 5492 | `				return -1;` |
|      - | 5493 | `			}` |
|     76 | 5494 | `			bTime = 1;` |
|     76 | 5495 | `			z++;` |
|     76 | 5496 | `			continue;` |
|      - | 5497 | `		}` |
|    303 | 5498 | `		n = DtIvDigits(z,zEnd,&v);` |
|    303 | 5499 | `		if( n == 0 \|\| n > DT_DIGITS_ISO \|\| z + n >= zEnd ){` |
|      - | 5500 | ``			/* php's duration fields stop at twelve digits: `P999999999999D` is an`` |
|      - | 5501 | ``			 * interval there and `P9999999999999D` is "Unknown or bad format". */`` |
|     15 | 5502 | `			return -1;` |
|      - | 5503 | `		}` |
|    289 | 5504 | `		z += n;` |
|    289 | 5505 | `		switch( z[0] ){` |
|     15 | 5506 | `			case 'Y': if( bTime ){ return -1; } aOut[0] += v; break;` |
|      9 | 5507 | `			case 'W': if( bTime ){ return -1; } aOut[2] += v * 7; break;` |
|    156 | 5508 | `			case 'D': if( bTime ){ return -1; } aOut[2] += v; break;` |
|     15 | 5509 | `			case 'H': if( !bTime ){ return -1; } aOut[3] += v; break;` |
|     64 | 5510 | `			case 'S': if( !bTime ){ return -1; } aOut[5] += v; break;` |
|     14 | 5511 | `			case 'M':` |
|      - | 5512 | `				/* The one ambiguous designator: months before T, minutes after. */` |
|     29 | 5513 | `				if( bTime ){ aOut[4] += v; }else{ aOut[1] += v; }` |
|     29 | 5514 | `				break;` |
|      3 | 5515 | `			default:` |
|      7 | 5516 | `				return -1;` |
|      - | 5517 | `		}` |
|    283 | 5518 | `		z++;` |
|    283 | 5519 | `		bAny = 1;` |
|      3 | 5520 | `	}` |
|    239 | 5521 | `	return bAny ? 0 : -1;` |
|    136 | 5522 | `}` |
|      - | 5523 | `/*` |
|      - | 5524 | ` * php's relative-string interval. The string is VALIDATED by the same parser` |
|      - | 5525 | ` * strtotime() uses -- which is where php's "at position N (c): reason" wording` |
|      - | 5526 | ` * comes from -- and the number/unit pairs it understands are then summed. A` |
|      - | 5527 | ` * string the parser accepts but that names no unit ("next monday") is php's` |
|      - | 5528 | ` * all-zero interval, not an error.` |
|      - | 5529 | ` */` |
|    188 | 5530 | `static int DtIvParseRelative(const char *zIn,int nIn,sxi64 *aOut,int *piPos,` |
|      - | 5531 | `	char *pcAt,const char **pzReason)` |
|      2 | 5532 | `{` |
|      - | 5533 | `	dt_parsed sVec;` |
|    190 | 5534 | `	sxi64 iTs = 0;` |
|    190 | 5535 | `	sxi32 iOff = 0;` |
|    190 | 5536 | `	int bOffSet = 0,uSec = 0,iErr;` |
|      - | 5537 | `	int k;` |
|   1506 | 5538 | `	for( k = 0 ; k <= DT_IV_USLOT ; k++ ){` |
|   1318 | 5539 | `		aOut[k] = 0;` |
|    660 | 5540 | `	}` |
|    190 | 5541 | `	if( nIn < 1 ){` |
|      3 | 5542 | `		*piPos = 0;` |
|      3 | 5543 | `		*pcAt = ' ';` |
|      3 | 5544 | `		*pzReason = "Empty string";` |
|      3 | 5545 | `		return -1;` |
|      - | 5546 | `	}` |
|      - | 5547 | `	/* createFromDateString() does NOT publish getLastErrors() in php: the record` |
|      - | 5548 | `	 * keeps whatever the last constructor or modify() left in it. */` |
|    188 | 5549 | `	iErr = DtParseEx(zIn,nIn,0,0,0,0,&iTs,&iOff,&bOffSet,&uSec,&sVec,0);` |
|    188 | 5550 | `	if( iErr != 0 ){` |
|     44 | 5551 | `		*pzReason = DtParseErr(zIn,nIn,iErr,piPos,pcAt);` |
|     44 | 5552 | `		return -1;` |
|      - | 5553 | `	}` |
|      - | 5554 | `	/* php REFUSES a string carrying any absolute element -- a date, a time of day` |
|      - | 5555 | `	 * or a zone -- rather than reading an interval out of what is left: it checks` |
|      - | 5556 | `	 * the same three flags the parse already carries. A relative NAVIGATION word` |
|      - | 5557 | ``	 * is not one of them, which is what makes `tomorrow` the interval d = 1. */`` |
|    146 | 5558 | `	if( sVec.bHaveDate \|\| sVec.nTimeTok \|\| sVec.bOffSet ){` |
|     23 | 5559 | `		return -2;` |
|      - | 5560 | `	}` |
|      - | 5561 | `	/* The vector IS the interval: php reads timelib_rel_time's own fields, so a` |
|      - | 5562 | `	 * week is already days there and the microseconds stand apart from the` |
|      - | 5563 | `	 * seconds. */` |
|    124 | 5564 | `	aOut[0] = sVec.ry;` |
|    124 | 5565 | `	aOut[1] = sVec.rm;` |
|    124 | 5566 | `	aOut[2] = sVec.rd;` |
|    124 | 5567 | `	aOut[3] = sVec.rh;` |
|    124 | 5568 | `	aOut[4] = sVec.ri;` |
|    124 | 5569 | `	aOut[5] = sVec.rs;` |
|    124 | 5570 | `	aOut[DT_IV_USLOT] = sVec.rus;` |
|    124 | 5571 | `	return 0;` |
|     96 | 5572 | `}` |
|      - | 5573 | `/*` |
|      - | 5574 | ` * php's from-string interval is a different OBJECT: it keeps the STRING and` |
|      - | 5575 | `` * presents `from_string` and `date_string` alone, answering the ten fields from`` |
|      - | 5576 | ` * what it parsed whenever a script asks for one. PHL fills the ten as it always` |
|      - | 5577 | ` * did -- every reader, the write filter, format(), add()/sub() and DatePeriod go` |
|      - | 5578 | ` * on reading real slots -- and hides them from the surfaces that SHOW the` |
|      - | 5579 | ` * object, which is the whole of the difference php's shape makes.` |
|      - | 5580 | ` */` |
|    122 | 5581 | `static void DtIvFromString(ph7_vm *pVm,ph7_class_instance *pObj,const char *zIn,int nIn)` |
|      2 | 5582 | `{` |
|      - | 5583 | `	static const char * const azHide[] = {` |
|      - | 5584 | `		"y","m","d","h","i","s","f","invert","days"` |
|      - | 5585 | `	};` |
|      - | 5586 | `	int k;` |
|    124 | 5587 | `	PH7_NativeSetAttrBool(&(*pVm),pObj,"from_string",1);` |
|      - | 5588 | `	/* the ON-DEMAND slot: this write is what puts the name on the object, and it` |
|      - | 5589 | ``	 * lands behind `from_string`, which is php's order */`` |
|    124 | 5590 | `	PH7_NativeSetAttrStr(&(*pVm),pObj,"date_string",zIn,nIn);` |
|   1222 | 5591 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azHide) ; k++ ){` |
|   1100 | 5592 | `		PH7_NativeHideAttr(pObj,azHide[k]);` |
|    551 | 5593 | `	}` |
|    124 | 5594 | `}` |
|      - | 5595 | `/* Write the six relative fields onto a DateInterval instance. */` |
|    358 | 5596 | `static void DtIvStore(ph7_vm *pVm,ph7_class_instance *pObj,const sxi64 *aVal)` |
|      3 | 5597 | `{` |
|      - | 5598 | `	int k;` |
|   2509 | 5599 | `	for( k = 0 ; k < DT_IV_FIELDS ; k++ ){` |
|   2151 | 5600 | `		PH7_NativeSetAttrInt(pVm,pObj,azDtIvField[k],aVal[k]);` |
|   1077 | 5601 | `	}` |
|      - | 5602 | ``	/* ...and the microseconds through the pair that owns them, so `f` and the`` |
|      - | 5603 | `	 * hidden count stay one value. */` |
|    361 | 5604 | `	DtIvSetUsec(pVm,pObj,aVal[DT_IV_USLOT]);` |
|    361 | 5605 | `}` |
|      - | 5606 | `/*` |
|      - | 5607 | ` * php's date_interval_write_property: what a write to one of DateInterval's` |
|      - | 5608 | ` * properties CONVERTS to, since every one of them is a field of php's own C` |
|      - | 5609 | ` * struct rather than a slot a script's value lands in.` |
|      - | 5610 | ` *` |
|      - | 5611 | `` * The six relative fields and `invert` take php's int cast — a float truncates`` |
|      - | 5612 | ` * and warns where it wraps, a string reads its numeric prefix, an array is 1 —` |
|      - | 5613 | `` * with `invert` narrowed to the 32-bit `int` timelib declares it as (so`` |
|      - | 5614 | `` * `$i->invert = 3000000000` is -1294967296 in both engines). `f` is the`` |
|      - | 5615 | ` * microsecond count above, so its cast warning is raised HERE, on the SCALED` |
|      - | 5616 | ` * value, which is where php raises it.` |
|      - | 5617 | ` *` |
|      - | 5618 | `` * `days` and `from_string` are answered by php's read handler and refused by its`` |
|      - | 5619 | ` * write one: a script that assigns them creates a deprecated DYNAMIC property` |
|      - | 5620 | `` * that never reaches the interval (`$i->days = 5` leaves `$i->days` false`` |
|      - | 5621 | ` * there). §10 refuses a deprecation, and PHL refuses a dynamic property outright,` |
|      - | 5622 | `` * so the two meet at the Error PHL already raises for `$i->anythingElse = v`.`` |
|      - | 5623 | ` */` |
|    616 | 5624 | `static void DtIntervalSet(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)` |
|      3 | 5625 | `{` |
|    619 | 5626 | `	const char *zName = SyStringData(pCtx->pName);` |
|    619 | 5627 | `	sxu32 nName = SyStringLength(pCtx->pName);` |
|    619 | 5628 | `	ph7_value *pVal = pCtx->pValue;` |
|      - | 5629 | `	int bInvert;` |
|    619 | 5630 | `	if( nName == sizeof("days")-1 && SyMemcmp(zName,"days",nName) == 0 ){` |
|      3 | 5631 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 5632 | `			"Cannot create dynamic property DateInterval::$days");` |
|      3 | 5633 | `		pCtx->zThrowClass = "Error";` |
|      3 | 5634 | `		return;` |
|      - | 5635 | `	}` |
|    617 | 5636 | `	if( nName == sizeof("from_string")-1 && SyMemcmp(zName,"from_string",nName) == 0 ){` |
|      3 | 5637 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 5638 | `			"Cannot create dynamic property DateInterval::$from_string");` |
|      3 | 5639 | `		pCtx->zThrowClass = "Error";` |
|      3 | 5640 | `		return;` |
|      - | 5641 | `	}` |
|    615 | 5642 | `	if( nName == sizeof("date_string")-1 && SyMemcmp(zName,"date_string",nName) == 0 ){` |
|    ! 0 | 5643 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 5644 | `			"Cannot create dynamic property DateInterval::$date_string");` |
|    ! 0 | 5645 | `		pCtx->zThrowClass = "Error";` |
|    ! 0 | 5646 | `		return;` |
|      - | 5647 | `	}` |
|    615 | 5648 | `	if( nName == sizeof("f")-1 && zName[0] == 'f' ){` |
|    492 | 5649 | `		double r = (double)PH7_ValuePeekReal(pVal);` |
|      - | 5650 | `		sxi64 us;` |
|    492 | 5651 | `		PH7_RealWarnIntCast(pVm,r * 1000000.0);` |
|    492 | 5652 | `		us = DtIvUsecOfReal(r);` |
|    492 | 5653 | `		PH7_NativeSetAttrInt(pVm,pThis,DT_IV_US,us);` |
|    492 | 5654 | `		PH7_MemObjRelease(pVal);` |
|    492 | 5655 | `		PH7_MemObjInitFromReal(pVm,pVal,(ph7_real)((double)us / 1000000.0));` |
|    492 | 5656 | `		return;` |
|      - | 5657 | `	}` |
|    124 | 5658 | `	bInvert = nName == sizeof("invert")-1 && SyMemcmp(zName,"invert",nName) == 0;` |
|    124 | 5659 | `	if( !bInvert ){` |
|      - | 5660 | `		int k;` |
|    188 | 5661 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(azDtIvField) ; k++ ){` |
|    188 | 5662 | `			if( nName == 1 && zName[0] == azDtIvField[k][0] ){` |
|     84 | 5663 | `				break;` |
|      - | 5664 | `			}` |
|     53 | 5665 | `		}` |
|     84 | 5666 | `		if( k >= (int)SX_ARRAYSIZE(azDtIvField) ){` |
|    ! 0 | 5667 | `			return;   /* the hidden count slot: written from C, never through here */` |
|      - | 5668 | `		}` |
|     41 | 5669 | `	}` |
|      - | 5670 | `	{` |
|      - | 5671 | `		/* y/m/d/h/i/s and invert, all of them php's int cast. */` |
|      - | 5672 | `		sxi64 iVal;` |
|    124 | 5673 | `		PH7_MemObjWarnIntCast(pVal);` |
|    124 | 5674 | `		iVal = PH7_ValuePeekInt64(pVal);` |
|    124 | 5675 | `		if( bInvert ){` |
|     41 | 5676 | `			iVal = (sxi64)(sxi32)iVal;` |
|     20 | 5677 | `		}` |
|    124 | 5678 | `		PH7_MemObjRelease(pVal);` |
|    124 | 5679 | `		PH7_MemObjInitFromInt(pVm,pVal,iVal);` |
|      - | 5680 | `	}` |
|    311 | 5681 | `}` |
|      - | 5682 | `/* DateInterval::__construct(string $duration) */` |
|    236 | 5683 | `static int vm_builtin_DateInterval_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5684 | `{` |
|    239 | 5685 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */` |
|      - | 5686 | `	const char *zDur;` |
|      - | 5687 | `	int nDur;` |
|      - | 5688 | `	sxi64 aVal[DT_IV_USLOT + 1];` |
|    239 | 5689 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 5690 | `		return PH7_OK;` |
|      - | 5691 | `	}` |
|    239 | 5692 | `	zDur = ph7_value_to_string(apArg[0],&nDur);` |
|    239 | 5693 | `	if( DtIvParseIso(zDur,nDur,aVal) != 0 ){` |
|     46 | 5694 | `		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",` |
|     15 | 5695 | `			"Unknown or bad format (%.*s)",DtCStrLen(zDur,nDur),zDur);` |
|      - | 5696 | `	}` |
|    209 | 5697 | `	DtIvStore(pCtx->pVm,pThis,aVal);` |
|    209 | 5698 | `	DtSetInit(pCtx->pVm,pThis);` |
|    209 | 5699 | `	return PH7_OK;` |
|    121 | 5700 | `}` |
|      - | 5701 | `/*` |
|      - | 5702 | ` * DateInterval::createFromDateString(string $datetime). Shared with the` |
|      - | 5703 | ` * date_interval_create_from_date_string() alias, which WARNS and answers false` |
|      - | 5704 | ` * where the method throws.` |
|      - | 5705 | ` */` |
|    186 | 5706 | `static ph7_class_instance * DtIvFromDateString(ph7_context *pCtx,const char *zIn,int nIn,` |
|      - | 5707 | `	int *piPos,char *pcAt,const char **pzReason,int *pbNonRel)` |
|      2 | 5708 | `{` |
|    188 | 5709 | `	ph7_vm *pVm = pCtx->pVm;` |
|    188 | 5710 | `	ph7_class *pClass = DtFactoryClass(pCtx,"DateInterval");` |
|      - | 5711 | `	ph7_class_instance *pObj;` |
|      - | 5712 | `	sxi64 aVal[DT_IV_USLOT + 1];` |
|      - | 5713 | `	int rc;` |
|    188 | 5714 | `	*pbNonRel = 0;` |
|    188 | 5715 | `	if( pClass == 0 ){` |
|    ! 0 | 5716 | `		return 0;` |
|      - | 5717 | `	}` |
|    188 | 5718 | `	if( (rc = DtIvParseRelative(zIn,nIn,aVal,piPos,pcAt,pzReason)) != 0 ){` |
|     68 | 5719 | `		*pbNonRel = (rc == -2);` |
|     68 | 5720 | `		return 0;` |
|      - | 5721 | `	}` |
|    122 | 5722 | `	pObj = DtNewInstance(pVm,pClass);` |
|    122 | 5723 | `	if( pObj == 0 ){` |
|    ! 0 | 5724 | `		return 0;` |
|      - | 5725 | `	}` |
|    122 | 5726 | `	DtIvStore(pVm,pObj,aVal);` |
|    122 | 5727 | `	DtIvFromString(pVm,pObj,zIn,nIn);` |
|    122 | 5728 | `	return pObj;` |
|     95 | 5729 | `}` |
|    176 | 5730 | `static int vm_builtin_DateInterval_createFromDateString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5731 | `{` |
|    178 | 5732 | `	const char *zIn,*zReason = "";` |
|    178 | 5733 | `	int nIn,iPos = 0,bNonRel = 0;` |
|    178 | 5734 | `	char cAt = ' ';` |
|      - | 5735 | `	ph7_class_instance *pObj;` |
|    178 | 5736 | `	if( nArg < 1 ){` |
|    ! 0 | 5737 | `		return PH7_OK;` |
|      - | 5738 | `	}` |
|    178 | 5739 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    178 | 5740 | `	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason,&bNonRel);` |
|    178 | 5741 | `	if( pObj == 0 ){` |
|     66 | 5742 | `		if( bNonRel ){` |
|     31 | 5743 | `			return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",` |
|     10 | 5744 | `				"String '%.*s' contains non-relative elements",DtCStrLen(zIn,nIn),zIn);` |
|      - | 5745 | `		}` |
|     68 | 5746 | `		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",` |
|      - | 5747 | `			"Unknown or bad format (%.*s) at position %d (%c): %s",` |
|     22 | 5748 | `			DtCStrLen(zIn,nIn),zIn,iPos,cAt,zReason);` |
|      - | 5749 | `	}` |
|    114 | 5750 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    114 | 5751 | `	return PH7_OK;` |
|     90 | 5752 | `}` |
|      - | 5753 | `/*` |
|      - | 5754 | ` * DateInterval::format(string $format) -- php's own %-token loop, including the` |
|      - | 5755 | `` * rule the chunk got wrong: an UNKNOWN token keeps its '%' (`%q` is "%q").`` |
|      - | 5756 | ` */` |
|    626 | 5757 | `static void DtIvFormat(ph7_context *pCtx,ph7_class_instance *pObj,const char *zFmt,int nFmt)` |
|      2 | 5758 | `{` |
|      - | 5759 | `	SyBlob sOut;` |
|      - | 5760 | `	ph7_value *pDays;` |
|      - | 5761 | `	int k;` |
|    628 | 5762 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   1998 | 5763 | `	for( k = 0 ; k < nFmt ; k++ ){` |
|   1374 | 5764 | `		char c = zFmt[k];` |
|      - | 5765 | `		char t;` |
|   1374 | 5766 | `		if( c != '%' ){` |
|    389 | 5767 | `			SyBlobAppend(&sOut,&c,1);` |
|    389 | 5768 | `			continue;` |
|      - | 5769 | `		}` |
|    986 | 5770 | `		k++;` |
|    986 | 5771 | `		if( k >= nFmt ){` |
|      - | 5772 | `			/* php drops a trailing lone '%' rather than echoing it. */` |
|      3 | 5773 | `			break;` |
|      - | 5774 | `		}` |
|    984 | 5775 | `		t = zFmt[k];` |
|    984 | 5776 | `		switch( t ){` |
|      - | 5777 | ``			/* php prints five of the six through an `(int)` — a 32-bit NARROWING`` |
|      - | 5778 | `			 * of a property it stores as an int64 and hands back whole, so` |
|      - | 5779 | ``			 * `$i->y = 7960523868075137518` reads back in full and prints`` |
|      - | 5780 | `			 * 111352302. The SECONDS are the exception: php formats those with` |
|      - | 5781 | `			 * its long specifier, in both cases. */` |
|      7 | 5782 | `			case 'Y': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"y")); break;` |
|     25 | 5783 | `			case 'y': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"y")); break;` |
|      7 | 5784 | `			case 'M': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"m")); break;` |
|     19 | 5785 | `			case 'm': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"m")); break;` |
|      7 | 5786 | `			case 'D': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"d")); break;` |
|     39 | 5787 | `			case 'd': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"d")); break;` |
|      7 | 5788 | `			case 'H': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"h")); break;` |
|     47 | 5789 | `			case 'h': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"h")); break;` |
|      7 | 5790 | `			case 'I': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"i")); break;` |
|     45 | 5791 | `			case 'i': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"i")); break;` |
|      7 | 5792 | `			case 'S': SyBlobFormat(&sOut,"%02qd",PH7_NativeAttrInt(pObj,"s")); break;` |
|     47 | 5793 | `			case 's': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"s")); break;` |
|    309 | 5794 | `			case 'F': case 'f': {` |
|      - | 5795 | `				/* php prints the STORED microsecond count, which is why` |
|      - | 5796 | ``				 * `$i->f = 0.1234567` prints 123456 rather than the 123457 a`` |
|      - | 5797 | `				 * rounding of the float would give, and why a count no double` |
|      - | 5798 | `				 * holds exactly still prints its own digits. The conversion the` |
|      - | 5799 | `				 * cast contract lives in — truncate toward zero, wrap what no` |
|      - | 5800 | `				 * int64 holds, 0 for a NaN or an infinity, and php's warning` |
|      - | 5801 | `				 * beside it — happens at the property WRITE, where php does it. */` |
|    620 | 5802 | `				sxi64 uS = DtIvUsec(pObj);` |
|    620 | 5803 | `				if( t == 'F' ){` |
|     85 | 5804 | `					SyBlobFormat(&sOut,"%06qd",uS);` |
|     43 | 5805 | `				}else{` |
|    536 | 5806 | `					SyBlobFormat(&sOut,"%qd",uS);` |
|      - | 5807 | `				}` |
|    620 | 5808 | `				break;` |
|      - | 5809 | `			}` |
|     49 | 5810 | `			case 'R': SyBlobAppend(&sOut,PH7_NativeAttrInt(pObj,"invert") ? "-" : "+",1); break;` |
|      5 | 5811 | `			case 'r': if( PH7_NativeAttrInt(pObj,"invert") ){ SyBlobAppend(&sOut,"-",1); } break;` |
|     26 | 5812 | `			case 'a':` |
|     53 | 5813 | `				pDays = PH7_NativeAttr(pObj,"days");` |
|     53 | 5814 | `				if( pDays && (pDays->iFlags & MEMOBJ_INT) ){` |
|     45 | 5815 | `					SyBlobFormat(&sOut,"%qd",pDays->x.iVal);` |
|     23 | 5816 | `				}else{` |
|      9 | 5817 | `					SyBlobAppend(&sOut,"(unknown)",sizeof("(unknown)")-1);` |
|      - | 5818 | `				}` |
|     53 | 5819 | `				break;` |
|      7 | 5820 | `			case '%': SyBlobAppend(&sOut,"%",1); break;` |
|      1 | 5821 | `			default:` |
|      - | 5822 | `				/* php keeps BOTH bytes of an unrecognised token. */` |
|      3 | 5823 | `				SyBlobAppend(&sOut,"%",1);` |
|      3 | 5824 | `				SyBlobAppend(&sOut,&t,1);` |
|      2 | 5825 | `				break;` |
|      - | 5826 | `		}` |
|    493 | 5827 | `	}` |
|    628 | 5828 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    628 | 5829 | `	SyBlobRelease(&sOut);` |
|    628 | 5830 | `}` |
|    626 | 5831 | `static int vm_builtin_DateInterval_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5832 | `{` |
|    628 | 5833 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 5834 | `	const char *zFmt;` |
|      - | 5835 | `	int nFmt;` |
|    628 | 5836 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|      5 | 5837 | `		return PH7_OK;` |
|      - | 5838 | `	}` |
|    624 | 5839 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    624 | 5840 | `	DtIvFormat(pCtx,pThis,zFmt,nFmt);` |
|    624 | 5841 | `	return PH7_OK;` |
|    315 | 5842 | `}` |
|      - | 5843 | `/* Is this value an instance of the named class? */` |
|    404 | 5844 | `static int DtValueIsA(ph7_vm *pVm,ph7_value *pVal,const char *zClass)` |
|      1 | 5845 | `{` |
|      - | 5846 | `	ph7_class *pClass;` |
|    405 | 5847 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      7 | 5848 | `		return 0;` |
|      - | 5849 | `	}` |
|    399 | 5850 | `	pClass = DtClass(&(*pVm),zClass);` |
|    399 | 5851 | `	return pClass != 0` |
|    398 | 5852 | `		&& PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass);` |
|    203 | 5853 | `}` |
|      - | 5854 | `/*` |
|      - | 5855 | ` * Write an object (or null) into a declared property of another object.` |
|      - | 5856 | ` *` |
|      - | 5857 | ` * The scratch value ALIASES the instance rather than owning it, and` |
|      - | 5858 | ` * PH7_MemObjStore takes the reference the slot keeps -- so releasing the scratch` |
|      - | 5859 | ` * afterwards would hand back the slot's own reference and free the object out from` |
|      - | 5860 | `` * under it (which is what it did: `foreach` over a DatePeriod crashed on the second`` |
|      - | 5861 | `` * element's `->format()`). The caller keeps owning whatever it passed in.`` |
|      - | 5862 | ` */` |
|      - | 5863 | `/*` |
|      - | 5864 | ` * DatePeriod::__construct($start, $interval, $end, $options)` |
|      - | 5865 | ` *` |
|      - | 5866 | ` * php overloads it three ways and rejects everything else with ONE message, which` |
|      - | 5867 | ` * is why the signature stays unenforced and the shapes are checked here.` |
|      - | 5868 | ` */` |
|      - | 5869 | `/* php's ceiling for a recurrence count, and it is checked TWICE: once on the` |
|      - | 5870 | ` * number the caller wrote, and once on that number plus the dates the OPTIONS` |
|      - | 5871 | ` * add -- two different exception classes and two different sentences. */` |
|      - | 5872 | `#define DP_REC_LIMIT 2147483640` |
|    222 | 5873 | `static int DpConstructInto(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,ph7_value **apArg,` |
|      - | 5874 | `	const char *zIsoStartClass,const char *zCallee)` |
|      1 | 5875 | `{` |
|    223 | 5876 | `	ph7_vm *pVm = pCtx->pVm;` |
|    223 | 5877 | `	sxi64 iOptions = 0;` |
|      - | 5878 | `	static const char *zBadArgs =` |
|      - | 5879 | `		"DatePeriod::__construct() accepts (DateTimeInterface, DateInterval, int [, int]), "` |
|      - | 5880 | `		"or (DateTimeInterface, DateInterval, DateTime [, int]), or (string [, int]) as arguments";` |
|    223 | 5881 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 5882 | `		return PH7_VmThrowException(pCtx,"TypeError","%s",zBadArgs);` |
|      - | 5883 | `	}` |
|    223 | 5884 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|      - | 5885 | `		/* The ISO-8601 form: "R<n>/<start>/<duration>". php's second argument is` |
|      - | 5886 | `		 * then the OPTIONS bitmask, not an interval. */` |
|     49 | 5887 | `		const char *zSpec = (const char *)SyBlobData(&apArg[0]->sBlob);` |
|     49 | 5888 | `		int nSpec = (int)SyBlobLength(&apArg[0]->sBlob);` |
|      - | 5889 | `		const char *zStart,*zDur;` |
|      - | 5890 | `		int nStart,nDur,k;` |
|     49 | 5891 | `		sxi64 nRec = 0;` |
|      - | 5892 | `		sxi64 aIv[DT_IV_USLOT + 1];` |
|      - | 5893 | `		dt_state sState;` |
|      - | 5894 | `		char zNameBuf[16];` |
|      - | 5895 | `		const char *zErr;` |
|      - | 5896 | `		int iPos,nDigits;` |
|      - | 5897 | `		char cAt;` |
|      - | 5898 | `		ph7_class_instance *pStart,*pIv;` |
|     49 | 5899 | `		if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_INT) ){` |
|    ! 0 | 5900 | `			iOptions = apArg[1]->x.iVal;` |
|    ! 0 | 5901 | `		}` |
|     49 | 5902 | `		if( nSpec < 2 \|\| zSpec[0] != 'R' ){` |
|    ! 0 | 5903 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 5904 | `				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);` |
|      - | 5905 | `		}` |
|     49 | 5906 | `		nDigits = DtIvDigits(&zSpec[1],&zSpec[nSpec],&nRec);` |
|     49 | 5907 | `		k = 1 + nDigits;` |
|     49 | 5908 | `		if( nDigits == 0 \|\| k >= nSpec \|\| zSpec[k] != '/' ){` |
|    ! 0 | 5909 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 5910 | `				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);` |
|      - | 5911 | `		}` |
|     49 | 5912 | `		if( nDigits > 9 ){` |
|      - | 5913 | `			/* php's ISO scanner reads at most NINE digits of the count and drops` |
|      - | 5914 | ``			 * the rest on the floor -- `R2147483639/...` is 214748363 recurrences`` |
|      - | 5915 | ``			 * there, and `R99999999999999999999/...` is 999999999. Reading them`` |
|      - | 5916 | `			 * all was a silent wrong answer here. */` |
|     13 | 5917 | `			nRec = 0;` |
|     13 | 5918 | `			DtIvDigits(&zSpec[1],&zSpec[1 + 9],&nRec);` |
|      6 | 5919 | `		}` |
|     49 | 5920 | `		if( nRec < 1 ){` |
|      - | 5921 | ``			/* `R0` is php's refusal, and it is worded as a MISSING count rather`` |
|      - | 5922 | `			 * than an out-of-range one. */` |
|     25 | 5923 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|      - | 5924 | `				"%s(): ISO interval must contain an end date or a recurrence count, "` |
|      8 | 5925 | `				"\"%.*s\" given",zCallee,nSpec,zSpec);` |
|      - | 5926 | `		}` |
|     33 | 5927 | `		zStart = &zSpec[k+1];` |
|     33 | 5928 | `		nStart = 0;` |
|    641 | 5929 | `		while( &zStart[nStart] < &zSpec[nSpec] && zStart[nStart] != '/' ){` |
|    609 | 5930 | `			nStart++;` |
|      1 | 5931 | `		}` |
|     33 | 5932 | `		if( &zStart[nStart] >= &zSpec[nSpec] ){` |
|    ! 0 | 5933 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 5934 | `				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);` |
|      - | 5935 | `		}` |
|     33 | 5936 | `		zDur = &zStart[nStart+1];` |
|     33 | 5937 | `		nDur = (int)(&zSpec[nSpec] - zDur);` |
|     48 | 5938 | `		if( DtInitState(pCtx,zStart,nStart,0,pVm->zDefTz,(int)pVm->nDefTz,` |
|      - | 5939 | `			DT_ZONE_DEFAULT_KIND,&sState,` |
|     32 | 5940 | `			zNameBuf,sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0` |
|     32 | 5941 | `		 \|\| DtIvParseIso(zDur,nDur,aIv) != 0 ){` |
|      4 | 5942 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|      1 | 5943 | `				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);` |
|      - | 5944 | `		}` |
|      - | 5945 | `		/* php's two ISO entry points disagree on the class they build, and both` |
|      - | 5946 | ``		 * answers are load-bearing: `new DatePeriod("R2/...")` yields DateTime`` |
|      - | 5947 | `		 * where DatePeriod::createFromISO8601String() yields DateTimeImmutable. */` |
|     31 | 5948 | `		pStart = DtNewInstance(pVm,DtClass(pVm,zIsoStartClass));` |
|     31 | 5949 | `		pIv = DtNewInstance(pVm,DtClass(pVm,"DateInterval"));` |
|     31 | 5950 | `		if( pStart == 0 \|\| pIv == 0 ){` |
|    ! 0 | 5951 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5952 | `		}` |
|     31 | 5953 | `		DtStore(pVm,pStart,&sState);` |
|     31 | 5954 | `		DtIvStore(pVm,pIv,aIv);` |
|     31 | 5955 | `		PH7_NativeSetAttrObj(pVm,pThis,"start",pStart);` |
|     31 | 5956 | `		PH7_NativeSetAttrObj(pVm,pThis,"interval",pIv);` |
|     31 | 5957 | `		PH7_ClassInstanceUnref(pStart);` |
|     31 | 5958 | `		PH7_ClassInstanceUnref(pIv);` |
|     31 | 5959 | `		PH7_NativeSetAttrInt(pVm,pThis,"recurrences",nRec + 1);` |
|     16 | 5960 | `	}else{` |
|      - | 5961 | `		ph7_class_instance *pStart,*pIv,*pEnd;` |
|    174 | 5962 | `		if( !DtValueIsA(pVm,apArg[0],"DateTimeInterface")` |
|    174 | 5963 | `		 \|\| nArg < 3` |
|    173 | 5964 | `		 \|\| !DtValueIsA(pVm,apArg[1],"DateInterval")` |
|    172 | 5965 | `		 \|\| ((apArg[2]->iFlags & MEMOBJ_INT) == 0` |
|     95 | 5966 | `		     && !DtValueIsA(pVm,apArg[2],"DateTimeInterface")) ){` |
|      5 | 5967 | `			return PH7_VmThrowException(pCtx,"TypeError","%s",zBadArgs);` |
|      - | 5968 | `		}` |
|      - | 5969 | `		/* An unconstructed date on either end. php's sentence here names the` |
|      - | 5970 | `		 * INTERFACE its argument is declared as and not the object's own class --` |
|      - | 5971 | `		 * a subclass of DateTime is still reported as "DateTimeInterface" -- so` |
|      - | 5972 | `		 * this one door words the refusal itself. (php reaches the INTERVAL` |
|      - | 5973 | `		 * argument's state without a screen at all and segfaults on an` |
|      - | 5974 | `		 * unconstructed one; PHL refuses it the way every other door does, which` |
|      - | 5975 | `		 * is PLAN §10.) */` |
|    170 | 5976 | `		if( DtArgInitNamed(pCtx,apArg[0],"DateTimeInterface") != 0` |
|    169 | 5977 | `		 \|\| DtArgInit(pCtx,apArg[1]) != 0` |
|    169 | 5978 | `		 \|\| DtArgInitNamed(pCtx,apArg[2],"DateTimeInterface") != 0 ){` |
|      5 | 5979 | `			return PH7_OK;` |
|      - | 5980 | `		}` |
|    167 | 5981 | `		if( nArg > 3 ){` |
|     99 | 5982 | `			iOptions = ph7_value_to_int64(apArg[3]);` |
|     49 | 5983 | `		}` |
|    167 | 5984 | `		if( apArg[2]->iFlags & MEMOBJ_INT ){` |
|      - | 5985 | `			/* php's two range checks on a recurrence COUNT, in its order and with` |
|      - | 5986 | `			 * its two exception classes: the bare number first, then the number` |
|      - | 5987 | `			 * plus the dates the options ask for (the start date unless` |
|      - | 5988 | `			 * EXCLUDE_START_DATE, and the end date if INCLUDE_END_DATE) -- which` |
|      - | 5989 | ``			 * is why `2147483639` alone is refused while the same count with`` |
|      - | 5990 | `			 * EXCLUDE_START_DATE is built. PHL accepted every one of them,` |
|      - | 5991 | ``			 * `0` and `-1` included, and iterated a period php refuses to make. */`` |
|    149 | 5992 | `			sxi64 iRec = apArg[2]->x.iVal;` |
|      - | 5993 | `			sxi64 iWithOpt;` |
|    149 | 5994 | `			if( iRec < 1 \|\| iRec >= DP_REC_LIMIT ){` |
|     61 | 5995 | `				return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|      - | 5996 | `					"%s(): Recurrence count must be greater or equal to 1 and lower than %d",` |
|     20 | 5997 | `					zCallee,DP_REC_LIMIT);` |
|      - | 5998 | `			}` |
|      - | 5999 | `			/* Only now: the sum is computed on a count already known to be under` |
|      - | 6000 | `			 * the ceiling, so the two dates the options may add cannot overflow it` |
|      - | 6001 | `			 * (PHP_INT_MAX + 1 did, and UBSan said so). */` |
|    163 | 6002 | `			iWithOpt = iRec + ((iOptions & 1) == 0 ? 1 : 0)` |
|    108 | 6003 | `			         + ((iOptions & 2) != 0 ? 1 : 0);` |
|    109 | 6004 | `			if( iWithOpt >= DP_REC_LIMIT ){` |
|     13 | 6005 | `				return PH7_VmThrowException(pCtx,"DateMalformedStringException",` |
|      - | 6006 | `					"%s(): Recurrence count must be greater or equal to 1 and lower than %d "` |
|      4 | 6007 | `					"(including options)",zCallee,DP_REC_LIMIT);` |
|      - | 6008 | `			}` |
|     50 | 6009 | `		}` |
|    119 | 6010 | `		pStart = PH7_CloneClassInstance((ph7_class_instance *)apArg[0]->x.pOther);` |
|    119 | 6011 | `		pIv = (ph7_class_instance *)apArg[1]->x.pOther;` |
|    119 | 6012 | `		if( pStart == 0 ){` |
|    ! 0 | 6013 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6014 | `		}` |
|    119 | 6015 | `		PH7_NativeSetAttrObj(pVm,pThis,"start",pStart);` |
|    119 | 6016 | `		PH7_ClassInstanceUnref(pStart);` |
|    119 | 6017 | `		PH7_NativeSetAttrObj(pVm,pThis,"interval",pIv);` |
|    119 | 6018 | `		if( apArg[2]->iFlags & MEMOBJ_INT ){` |
|    101 | 6019 | `			PH7_NativeSetAttrInt(pVm,pThis,"recurrences",apArg[2]->x.iVal + 1);` |
|     51 | 6020 | `		}else{` |
|     19 | 6021 | `			pEnd = PH7_CloneClassInstance((ph7_class_instance *)apArg[2]->x.pOther);` |
|     19 | 6022 | `			if( pEnd == 0 ){` |
|    ! 0 | 6023 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 6024 | `			}` |
|     19 | 6025 | `			PH7_NativeSetAttrObj(pVm,pThis,"end",pEnd);` |
|     19 | 6026 | `			PH7_ClassInstanceUnref(pEnd);` |
|      - | 6027 | `			/* php's own answer for a period bounded by a DATE rather than a count,` |
|      - | 6028 | `			 * and it has to be written rather than defaulted now that an` |
|      - | 6029 | `			 * unconstructed period reads 0. */` |
|     19 | 6030 | `			PH7_NativeSetAttrInt(pVm,pThis,"recurrences",1);` |
|      - | 6031 | `		}` |
|      - | 6032 | `	}` |
|    149 | 6033 | `	PH7_NativeSetAttrBool(pVm,pThis,"include_start_date",(iOptions & 1) == 0);` |
|    149 | 6034 | `	PH7_NativeSetAttrBool(pVm,pThis,"include_end_date",(iOptions & 2) != 0);` |
|    149 | 6035 | `	DtSetInit(pVm,pThis);` |
|    149 | 6036 | `	return PH7_OK;` |
|    112 | 6037 | `}` |
|    198 | 6038 | `static int vm_builtin_DatePeriod_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6039 | `{` |
|    199 | 6040 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */` |
|    199 | 6041 | `	if( pThis == 0 ){` |
|    ! 0 | 6042 | `		return PH7_OK;` |
|      - | 6043 | `	}` |
|    199 | 6044 | `	return DpConstructInto(pCtx,pThis,nArg,apArg,"DateTime","DatePeriod::__construct");` |
|    100 | 6045 | `}` |
|      - | 6046 | `/* DatePeriod::createFromISO8601String(string $specification, int $options = 0) */` |
|     24 | 6047 | `static int vm_builtin_DatePeriod_createFromISO8601String(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6048 | `{` |
|     25 | 6049 | `	ph7_vm *pVm = pCtx->pVm;` |
|     25 | 6050 | `	ph7_class *pClass = DtFactoryClass(pCtx,"DatePeriod");` |
|      - | 6051 | `	ph7_class_instance *pObj;` |
|      - | 6052 | `	sxi32 rc;` |
|     25 | 6053 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 6054 | `		return PH7_OK;` |
|      - | 6055 | `	}` |
|      - | 6056 | `	/* Not DtNewInstance(): this object is INITIALIZED by the shared constructor` |
|      - | 6057 | `	 * body below, and only if that succeeds. */` |
|     25 | 6058 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     25 | 6059 | `	if( pObj == 0 ){` |
|    ! 0 | 6060 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6061 | `	}` |
|      - | 6062 | `	/* php's factory IS the constructor, with the same overloaded argument shape. */` |
|     25 | 6063 | `	rc = DpConstructInto(pCtx,pObj,nArg,apArg,"DateTimeImmutable",` |
|      - | 6064 | `		"DatePeriod::createFromISO8601String");` |
|     25 | 6065 | `	if( rc != PH7_OK \|\| pCtx->nThrowRc != 0 ){` |
|      9 | 6066 | `		PH7_ClassInstanceUnref(pObj);` |
|      9 | 6067 | `		return rc;` |
|      - | 6068 | `	}` |
|     17 | 6069 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     17 | 6070 | `	return PH7_OK;` |
|     13 | 6071 | `}` |
|     34 | 6072 | `static int vm_builtin_DatePeriod_getStartDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6073 | `{` |
|     35 | 6074 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|     17 | 6075 | `	SXUNUSED(nArg);` |
|     17 | 6076 | `	SXUNUSED(apArg);` |
|     35 | 6077 | `	if( pThis ){` |
|     29 | 6078 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"start");` |
|     29 | 6079 | `		if( pVal ){` |
|     29 | 6080 | `			ph7_result_value(pCtx,pVal);` |
|     14 | 6081 | `		}` |
|     14 | 6082 | `	}` |
|     35 | 6083 | `	return PH7_OK;` |
|      1 | 6084 | `}` |
|      - | 6085 | `/* php's two NULLABLE getters read the struct without screening it, so an` |
|      - | 6086 | ` * unconstructed period answers null from both where every other door raises. */` |
|      8 | 6087 | `static int vm_builtin_DatePeriod_getEndDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6088 | `{` |
|      9 | 6089 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|      4 | 6090 | `	SXUNUSED(nArg);` |
|      4 | 6091 | `	SXUNUSED(apArg);` |
|      9 | 6092 | `	if( pThis ){` |
|      9 | 6093 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"end");` |
|      9 | 6094 | `		if( pVal ){` |
|      5 | 6095 | `			ph7_result_value(pCtx,pVal);` |
|      2 | 6096 | `		}` |
|      4 | 6097 | `	}` |
|      9 | 6098 | `	return PH7_OK;` |
|      1 | 6099 | `}` |
|      4 | 6100 | `static int vm_builtin_DatePeriod_getDateInterval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6101 | `{` |
|      5 | 6102 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      2 | 6103 | `	SXUNUSED(nArg);` |
|      2 | 6104 | `	SXUNUSED(apArg);` |
|      5 | 6105 | `	if( pThis ){` |
|      3 | 6106 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"interval");` |
|      3 | 6107 | `		if( pVal ){` |
|      3 | 6108 | `			ph7_result_value(pCtx,pVal);` |
|      1 | 6109 | `		}` |
|      1 | 6110 | `	}` |
|      5 | 6111 | `	return PH7_OK;` |
|      1 | 6112 | `}` |
|      - | 6113 | `/*` |
|      - | 6114 | ` * DatePeriod::getRecurrences() -- php answers NULL for a period bounded by an END` |
|      - | 6115 | `` * DATE and the recurrence COUNT otherwise, which is `recurrences - 1` (php stores`` |
|      - | 6116 | ` * the count of dates, one more than the recurrences). No private slot needed.` |
|      - | 6117 | ` */` |
|     50 | 6118 | `static int vm_builtin_DatePeriod_getRecurrences(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6119 | `{` |
|     51 | 6120 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|     25 | 6121 | `	SXUNUSED(nArg);` |
|     25 | 6122 | `	SXUNUSED(apArg);` |
|     51 | 6123 | `	if( pThis == 0 \|\| !DtIsInit(pThis) \|\| PH7_NativeAttrObj(pThis,"end") != 0 ){` |
|      7 | 6124 | `		ph7_result_null(pCtx);` |
|      7 | 6125 | `		return PH7_OK;` |
|      - | 6126 | `	}` |
|     45 | 6127 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,"recurrences") - 1);` |
|     45 | 6128 | `	return PH7_OK;` |
|     26 | 6129 | `}` |
|      - | 6130 | `/*` |
|      - | 6131 | ` * The period walk, expressed as the vtable an InternalIterator drives (oo_native.c).` |
|      - | 6132 | ` * It uses the shared cursor slots: SRC is the period, CUR the date the cursor sits` |
|      - | 6133 | ` * on, KEY the emitted position and POS the loop counter (which differs from KEY,` |
|      - | 6134 | ` * since an excluded start date is stepped over without emitting one).` |
|      - | 6135 | ` */` |
|      - | 6136 | `#define DP_IT_STEP PH7_NATIVE_IT_POS` |
|      - | 6137 | `/*` |
|      - | 6138 | ` * One interval step from a date object: a NEW object, so a value already handed` |
|      - | 6139 | ` * to the caller is never mutated underneath it (php's iterator answers a fresh` |
|      - | 6140 | ` * object per position too).` |
|      - | 6141 | ` *` |
|      - | 6142 | `` * The step always ADDS, whatever the interval's `invert` says -- php's period`` |
|      - | 6143 | ` * walk reads the fields and not the flag, so a period built on an interval a` |
|      - | 6144 | `` * diff() answered (or on `$iv->invert = 1`) still runs FORWARD, while a`` |
|      - | 6145 | `` * negative FIELD (`createFromDateString('-1 day')` leaves d = -1 and invert 0)`` |
|      - | 6146 | ` * really does step backward. PHL honoured the flag, so such a period walked the` |
|      - | 6147 | ` * wrong way -- and with an END date rather than a recurrence count it walked` |
|      - | 6148 | ` * away from that end, stopped by nothing.` |
|      - | 6149 | ` */` |
|    254 | 6150 | `static ph7_class_instance * DpAdvance(ph7_vm *pVm,ph7_class_instance *pCur,` |
|      - | 6151 | `	ph7_class_instance *pIv)` |
|      1 | 6152 | `{` |
|    255 | 6153 | `	ph7_class_instance *pNext = PH7_CloneClassInstance(pCur);` |
|    255 | 6154 | `	if( pNext == 0 ){` |
|    ! 0 | 6155 | `		return 0;` |
|      - | 6156 | `	}` |
|    255 | 6157 | `	DtApplyInterval(&(*pVm),pCur,pNext,pIv,1);` |
|    255 | 6158 | `	return pNext;` |
|    128 | 6159 | `}` |
|      - | 6160 | `/*` |
|      - | 6161 | ` * Settle the cursor on the next date the period EMITS, mirroring the generator` |
|      - | 6162 | ` * this replaced: a start excluded by EXCLUDE_START_DATE is stepped over, an end` |
|      - | 6163 | ` * date stops the walk (inclusively under INCLUDE_END_DATE) and a recurrence count` |
|      - | 6164 | ` * bounds the number of steps instead.` |
|      - | 6165 | ` */` |
|    396 | 6166 | `static void DpSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 | 6167 | `{` |
|    397 | 6168 | `	ph7_class_instance *pPeriod = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|      - | 6169 | `	ph7_class_instance *pEnd,*pIv;` |
|      - | 6170 | `	int bInclStart,bInclEnd;` |
|      - | 6171 | `	sxi64 nTotal;` |
|    397 | 6172 | `	if( pPeriod == 0 ){` |
|    ! 0 | 6173 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 6174 | `		return;` |
|      - | 6175 | `	}` |
|    397 | 6176 | `	pEnd = PH7_NativeAttrObj(pPeriod,"end");` |
|    397 | 6177 | `	pIv = PH7_NativeAttrObj(pPeriod,"interval");` |
|    397 | 6178 | `	bInclStart = PH7_NativeAttrTruthy(pPeriod,"include_start_date");` |
|    397 | 6179 | `	bInclEnd = PH7_NativeAttrTruthy(pPeriod,"include_end_date");` |
|    397 | 6180 | `	nTotal = PH7_NativeAttrInt(pPeriod,"recurrences") + (bInclEnd ? 1 : 0);` |
|    219 | 6181 | `	for(;;){` |
|    419 | 6182 | `		ph7_class_instance *pCur = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_CUR);` |
|    419 | 6183 | `		sxi64 iStep = PH7_NativeAttrInt(pIt,DP_IT_STEP);` |
|      - | 6184 | `		ph7_class_instance *pNext;` |
|    419 | 6185 | `		if( pCur == 0 ){` |
|    ! 0 | 6186 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 6187 | `			return;` |
|      - | 6188 | `		}` |
|    419 | 6189 | `		if( pEnd != 0 ){` |
|     73 | 6190 | `			sxi64 iTs = PH7_NativeAttrInt(pCur,DT_TS);` |
|     73 | 6191 | `			sxi64 iEndTs = PH7_NativeAttrInt(pEnd,DT_TS);` |
|     73 | 6192 | `			if( bInclEnd ? (iTs > iEndTs) : (iTs >= iEndTs) ){` |
|     15 | 6193 | `				PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|     15 | 6194 | `				return;` |
|      1 | 6195 | `			}` |
|    378 | 6196 | `		}else if( iStep >= nTotal ){` |
|     63 | 6197 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|     63 | 6198 | `			return;` |
|      - | 6199 | `		}` |
|    345 | 6200 | `		if( iStep > 0 \|\| bInclStart ){` |
|    323 | 6201 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|    323 | 6202 | `			return;` |
|      - | 6203 | `		}` |
|      - | 6204 | `		/* The excluded start: step over it without emitting a key. */` |
|     23 | 6205 | `		if( pIv == 0 ){` |
|    ! 0 | 6206 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 6207 | `			return;` |
|      - | 6208 | `		}` |
|     23 | 6209 | `		pNext = DpAdvance(&(*pVm),pCur,pIv);` |
|     23 | 6210 | `		if( pNext == 0 ){` |
|    ! 0 | 6211 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 6212 | `			return;` |
|      - | 6213 | `		}` |
|     23 | 6214 | `		PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_CUR,pNext);` |
|     23 | 6215 | `		PH7_ClassInstanceUnref(pNext);` |
|     23 | 6216 | `		PH7_NativeSetAttrInt(&(*pVm),pIt,DP_IT_STEP,iStep + 1);` |
|      1 | 6217 | `	}` |
|    200 | 6218 | `}` |
|    182 | 6219 | `static void DpRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 | 6220 | `{` |
|      - | 6221 | `	ph7_class_instance *pPeriod,*pStart,*pCur;` |
|    183 | 6222 | `	pPeriod = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_SRC);` |
|    183 | 6223 | `	pStart = pPeriod ? PH7_NativeAttrObj(pPeriod,"start") : 0;` |
|    183 | 6224 | `	pCur = pStart ? PH7_CloneClassInstance(pStart) : 0;` |
|    183 | 6225 | `	if( pCur == 0 ){` |
|     19 | 6226 | `		PH7_NativeSetAttrBool(pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|     19 | 6227 | `		return;` |
|      - | 6228 | `	}` |
|    165 | 6229 | `	PH7_NativeSetAttrObj(pVm,pThis,PH7_NATIVE_IT_CUR,pCur);` |
|    165 | 6230 | `	PH7_ClassInstanceUnref(pCur);` |
|    165 | 6231 | `	PH7_NativeSetAttrInt(pVm,pThis,PH7_NATIVE_IT_KEY,0);` |
|    165 | 6232 | `	PH7_NativeSetAttrInt(pVm,pThis,DP_IT_STEP,0);` |
|    165 | 6233 | `	DpSettle(pVm,pThis);` |
|     92 | 6234 | `}` |
|    234 | 6235 | `static void DpNext(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 | 6236 | `{` |
|      - | 6237 | `	ph7_class_instance *pPeriod,*pIv,*pCur,*pNext;` |
|    235 | 6238 | `	pPeriod = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_SRC);` |
|    235 | 6239 | `	pIv = pPeriod ? PH7_NativeAttrObj(pPeriod,"interval") : 0;` |
|    235 | 6240 | `	pCur = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_CUR);` |
|    235 | 6241 | `	pNext = (pIv && pCur) ? DpAdvance(pVm,pCur,pIv) : 0;` |
|    235 | 6242 | `	if( pNext == 0 ){` |
|    ! 0 | 6243 | `		PH7_NativeSetAttrBool(pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 6244 | `		return;` |
|      - | 6245 | `	}` |
|    235 | 6246 | `	PH7_NativeSetAttrObj(pVm,pThis,PH7_NATIVE_IT_CUR,pNext);` |
|    235 | 6247 | `	PH7_ClassInstanceUnref(pNext);` |
|    235 | 6248 | `	PH7_NativeSetAttrInt(pVm,pThis,DP_IT_STEP,PH7_NativeAttrInt(pThis,DP_IT_STEP) + 1);` |
|    235 | 6249 | `	PH7_NativeSetAttrInt(pVm,pThis,PH7_NATIVE_IT_KEY,PH7_NativeAttrInt(pThis,PH7_NATIVE_IT_KEY) + 1);` |
|    235 | 6250 | `	DpSettle(pVm,pThis);` |
|    118 | 6251 | `}` |
|      - | 6252 | `/*` |
|      - | 6253 | ` * php's DatePeriod::$current IS the walk's cursor: the date the iterator sits on,` |
|      - | 6254 | ` * and -- once the walk is over -- the one PAST the end, the date that failed the` |
|      - | 6255 | ` * test. php writes it from the iterator's METHODS rather than from the walk, so a` |
|      - | 6256 | ` * getIterator() nobody has touched yet leaves it where the last walk left it, and` |
|      - | 6257 | ` * the first valid()/current()/key()/rewind()/next() moves it. PHL left it NULL` |
|      - | 6258 | ` * forever, so a program reading the period mid-walk (or after one) saw nothing.` |
|      - | 6259 | ` */` |
|    948 | 6260 | `static void DpPublish(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 | 6261 | `{` |
|    949 | 6262 | `	ph7_class_instance *pPeriod = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    949 | 6263 | `	if( pPeriod == 0 ){` |
|    ! 0 | 6264 | `		return;` |
|      - | 6265 | `	}` |
|    949 | 6266 | `	PH7_NativeSetAttrObj(&(*pVm),pPeriod,"current",PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_CUR));` |
|    475 | 6267 | `}` |
|      - | 6268 | `/*` |
|      - | 6269 | ` * php refuses the WALK of an unconstructed period, not the door to it: its` |
|      - | 6270 | ` * getIterator() hands back a real InternalIterator and the DateObjectError` |
|      - | 6271 | ` * arrives at the first rewind(). The sentence is the ITERATOR's too, and it` |
|      - | 6272 | ` * differs from every other one in this family -- it names DatePeriod plainly` |
|      - | 6273 | ` * whatever the object's own class is, where a method called on a subclass reports` |
|      - | 6274 | `` * `SubP (inheriting DatePeriod)`.`` |
|      - | 6275 | ` */` |
|    962 | 6276 | `static int DpIterGuard(ph7_context *pCtx,ph7_class_instance *pIt)` |
|      1 | 6277 | `{` |
|    963 | 6278 | `	ph7_class_instance *pPeriod = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    963 | 6279 | `	if( pPeriod != 0 && DtIsInit(pPeriod) ){` |
|    949 | 6280 | `		return 0;` |
|      - | 6281 | `	}` |
|     15 | 6282 | `	PH7_VmThrowException(pCtx,"DateObjectError",` |
|      - | 6283 | `		"Object of type DatePeriod has not been correctly initialized by calling "` |
|      - | 6284 | `		"parent::__construct() in its constructor");` |
|     15 | 6285 | `	return 1;` |
|    482 | 6286 | `}` |
|      - | 6287 | `static const PH7_NativeIterVtab sDpIterVtab = { DpRewind, DpNext, DpPublish, DpIterGuard };` |
|      - | 6288 | `/*` |
|      - | 6289 | ` * DatePeriod::getIterator(): Iterator` |
|      - | 6290 | ` *` |
|      - | 6291 | ` * This was a PHP GENERATOR, the one thing a C body cannot be. php answers an` |
|      - | 6292 | ` * InternalIterator here, so PHL answers the shared one (oo_native.c) driven by` |
|      - | 6293 | `` * the vtable above -- and stops diverging on `get_class($period->getIterator())`.`` |
|      - | 6294 | ` * A fresh one per call, as php's is.` |
|      - | 6295 | ` */` |
|    104 | 6296 | `static int vm_builtin_DatePeriod_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6297 | `{` |
|      - | 6298 | `	/* Raw: php's door does not screen the struct -- the ITERATOR does, at the` |
|      - | 6299 | `	 * first walk (DpIterGuard). */` |
|    105 | 6300 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|      - | 6301 | `	ph7_class_instance *pIt;` |
|     52 | 6302 | `	SXUNUSED(nArg);` |
|     52 | 6303 | `	SXUNUSED(apArg);` |
|    105 | 6304 | `	if( pThis == 0 ){` |
|    ! 0 | 6305 | `		return PH7_OK;` |
|      - | 6306 | `	}` |
|    105 | 6307 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|    105 | 6308 | `	if( pIt == 0 ){` |
|    ! 0 | 6309 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6310 | `	}` |
|    105 | 6311 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    105 | 6312 | `	return PH7_OK;` |
|     53 | 6313 | `}` |
|      - | 6314 | `/*` |
|      - | 6315 | ` * ---------------------------------------------------------------------------` |
|      - | 6316 | ` * The procedural date API.` |
|      - | 6317 | ` *` |
|      - | 6318 | ` * php's aliases are functions in their own right, not forwards: they reach the` |
|      - | 6319 | ` * same implementation the methods do, so an overridden method in a subclass is` |
|      - | 6320 | ` * NOT what they call, and the ones that can fail WARN and answer false where the` |
|      - | 6321 | ` * method throws. Each owes aBuiltinSig[] a row (vm_arg_check.c).` |
|      - | 6322 | ` * ---------------------------------------------------------------------------` |
|      - | 6323 | ` */` |
|      - | 6324 | `/*` |
|      - | 6325 | ` * The receiver argument of a procedural alias (already type-screened by its row),` |
|      - | 6326 | ` * or NULL for an object that was never constructed -- php's aliases reach the same` |
|      - | 6327 | ` * implementation the methods do, so they raise the same DateObjectError there` |
|      - | 6328 | ` * rather than warning the way the aliases that can FAIL do. Every caller already` |
|      - | 6329 | ` * treats a null the way the methods treat a null receiver: nothing to do, PH7_OK,` |
|      - | 6330 | ` * and the parked status reported at the host-call boundary.` |
|      - | 6331 | ` */` |
|    104 | 6332 | `static ph7_class_instance * DtArgObj(ph7_context *pCtx,int nArg,ph7_value **apArg,int iArg)` |
|      1 | 6333 | `{` |
|      - | 6334 | `	ph7_class_instance *pObj;` |
|    105 | 6335 | `	if( iArg >= nArg \|\| (apArg[iArg]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 6336 | `		return 0;` |
|      - | 6337 | `	}` |
|    105 | 6338 | `	pObj = (ph7_class_instance *)apArg[iArg]->x.pOther;` |
|    105 | 6339 | `	if( !DtIsInit(pObj) ){` |
|     29 | 6340 | `		DtThrowUninit(pCtx,pObj);` |
|     29 | 6341 | `		return 0;` |
|      - | 6342 | `	}` |
|     77 | 6343 | `	return pObj;` |
|     53 | 6344 | `}` |
|      - | 6345 | `/* Answer the receiver itself, the way every mutating alias does. */` |
|     20 | 6346 | `static void DtResultArg(ph7_context *pCtx,ph7_value **apArg)` |
|      1 | 6347 | `{` |
|     21 | 6348 | `	ph7_result_value(pCtx,apArg[0]);` |
|     21 | 6349 | `}` |
|      - | 6350 | `/* date_create()/date_create_immutable(): php answers false on a parse failure and` |
|      - | 6351 | ` * says nothing -- the constructor's exception does not escape the alias. */` |
|     42 | 6352 | `static int DtProcCreate(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zClass)` |
|      1 | 6353 | `{` |
|     43 | 6354 | `	ph7_vm *pVm = pCtx->pVm;` |
|     43 | 6355 | `	ph7_class *pClass = DtClass(pVm,zClass);` |
|      - | 6356 | `	ph7_class_instance *pObj;` |
|     43 | 6357 | `	const char *zIn = "now",*zZone;` |
|     43 | 6358 | `	int nIn = 3,nZone,iPos,iZoneKind;` |
|     43 | 6359 | `	sxi32 iZoneOff = 0;` |
|      - | 6360 | `	dt_state sState;` |
|      - | 6361 | `	char zNameBuf[16],cAt;` |
|      - | 6362 | `	const char *zErr;` |
|     43 | 6363 | `	if( pClass == 0 ){` |
|    ! 0 | 6364 | `		return PH7_OK;` |
|      - | 6365 | `	}` |
|     43 | 6366 | `	zZone = pVm->zDefTz;` |
|     43 | 6367 | `	nZone = (int)pVm->nDefTz;` |
|     43 | 6368 | `	iZoneKind = DT_ZONE_DEFAULT_KIND;` |
|     43 | 6369 | `	if( nArg > 0 ){` |
|     41 | 6370 | `		zIn = ph7_value_to_string(apArg[0],&nIn);` |
|     20 | 6371 | `	}` |
|     43 | 6372 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|      9 | 6373 | `		if( DtZoneArgInit(pCtx,apArg[1]) != 0 ){` |
|      3 | 6374 | `			return PH7_OK;` |
|      - | 6375 | `		}` |
|      7 | 6376 | `		DtZoneOf(apArg[1],&iZoneOff,&zZone,&nZone,&iZoneKind);` |
|      3 | 6377 | `	}` |
|     40 | 6378 | `	if( DtInitState(pCtx,zIn,nIn,iZoneOff,zZone,nZone,iZoneKind,&sState,zNameBuf,` |
|     21 | 6379 | `		sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0 ){` |
|      7 | 6380 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6381 | `		return PH7_OK;` |
|      - | 6382 | `	}` |
|     35 | 6383 | `	pObj = DtNewInstance(pVm,pClass);` |
|     35 | 6384 | `	if( pObj == 0 ){` |
|    ! 0 | 6385 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6386 | `	}` |
|     35 | 6387 | `	DtStore(pVm,pObj,&sState);` |
|     35 | 6388 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     35 | 6389 | `	return PH7_OK;` |
|     22 | 6390 | `}` |
|     38 | 6391 | `static int vm_builtin_date_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6392 | `{` |
|     39 | 6393 | `	return DtProcCreate(pCtx,nArg,apArg,"DateTime");` |
|      1 | 6394 | `}` |
|      4 | 6395 | `static int vm_builtin_date_create_immutable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6396 | `{` |
|      5 | 6397 | `	return DtProcCreate(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 6398 | `}` |
|     10 | 6399 | `static int vm_builtin_date_create_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6400 | `{` |
|     11 | 6401 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTime");` |
|      1 | 6402 | `}` |
|      4 | 6403 | `static int vm_builtin_date_create_immutable_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6404 | `{` |
|      5 | 6405 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 6406 | `}` |
|     12 | 6407 | `static int vm_builtin_date_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6408 | `{` |
|     13 | 6409 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      - | 6410 | `	const char *zFmt;` |
|      - | 6411 | `	int nFmt;` |
|     13 | 6412 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|      3 | 6413 | `		return PH7_OK;` |
|      - | 6414 | `	}` |
|     11 | 6415 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|     11 | 6416 | `	DtFormatOf(pCtx,pObj,zFmt,nFmt);` |
|     11 | 6417 | `	return PH7_OK;` |
|      7 | 6418 | `}` |
|      - | 6419 | `/* date_modify(): php WARNS and answers false where DateTime::modify() throws. */` |
|     10 | 6420 | `static int vm_builtin_date_modify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6421 | `{` |
|     11 | 6422 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      - | 6423 | `	const char *zMod,*zErr;` |
|      - | 6424 | `	int nMod,iPos,iErrPos;` |
|      - | 6425 | `	char cAt;` |
|     11 | 6426 | `	sxi64 iTs = 0;` |
|     11 | 6427 | `	sxi32 iOff = 0;` |
|     11 | 6428 | `	int bOffSet = 0,uSec = 0;` |
|      - | 6429 | `	dt_parsed sVec;` |
|     11 | 6430 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|      3 | 6431 | `		return PH7_OK;` |
|      - | 6432 | `	}` |
|      9 | 6433 | `	zMod = ph7_value_to_string(apArg[1],&nMod);` |
|     13 | 6434 | `	iErrPos = DtParseEx(zMod,nMod,PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),` |
|      8 | 6435 | `		(int)PH7_NativeAttrInt(pObj,DT_US),DT_PARSE_OVERRIDE_TIME\|DT_PARSE_KEEP_ZONE,` |
|      8 | 6436 | `		&iTs,&iOff,&bOffSet,&uSec,&sVec,&pCtx->pVm->sDtLastErr);` |
|      9 | 6437 | `	if( iErrPos != 0 ){` |
|      3 | 6438 | `		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);` |
|      4 | 6439 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 6440 | `			"date_modify(): Failed to parse time string (%.*s) at position %d (%c): %s",` |
|      1 | 6441 | `			DtCStrLen(zMod,nMod),zMod,iPos,cAt,zErr);` |
|      3 | 6442 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6443 | `		return PH7_OK;` |
|      - | 6444 | `	}` |
|      7 | 6445 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,iTs);` |
|      7 | 6446 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,uSec);   /* see DateTime::modify() */` |
|      7 | 6447 | `	DtEpochRezone(pCtx->pVm,pObj,&sVec);` |
|      7 | 6448 | `	DtResultArg(pCtx,apArg);` |
|      7 | 6449 | `	return PH7_OK;` |
|      6 | 6450 | `}` |
|     12 | 6451 | `static int DtProcAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)` |
|      1 | 6452 | `{` |
|     13 | 6453 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|     13 | 6454 | `	ph7_class_instance *pIv = DtArgObj(pCtx,nArg,apArg,1);` |
|     13 | 6455 | `	if( pObj == 0 \|\| pIv == 0 ){` |
|      3 | 6456 | `		return PH7_OK;` |
|      - | 6457 | `	}` |
|     11 | 6458 | `	if( PH7_NativeAttrInt(pIv,"invert") ){` |
|    ! 0 | 6459 | `		iSign = -iSign;` |
|    ! 0 | 6460 | `	}` |
|     11 | 6461 | `	DtApplyInterval(pCtx->pVm,pObj,pObj,pIv,iSign);` |
|     11 | 6462 | `	DtResultArg(pCtx,apArg);` |
|     11 | 6463 | `	return PH7_OK;` |
|      7 | 6464 | `}` |
|      8 | 6465 | `static int vm_builtin_date_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6466 | `{` |
|      9 | 6467 | `	return DtProcAddSub(pCtx,nArg,apArg,1);` |
|      1 | 6468 | `}` |
|      4 | 6469 | `static int vm_builtin_date_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6470 | `{` |
|      5 | 6471 | `	return DtProcAddSub(pCtx,nArg,apArg,-1);` |
|      1 | 6472 | `}` |
|      8 | 6473 | `static int vm_builtin_date_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6474 | `{` |
|      9 | 6475 | `	ph7_class_instance *pBase = DtArgObj(pCtx,nArg,apArg,0);` |
|      9 | 6476 | `	ph7_class_instance *pTarget = DtArgObj(pCtx,nArg,apArg,1);` |
|      9 | 6477 | `	int bAbsolute = 0;` |
|      9 | 6478 | `	if( pBase == 0 \|\| pTarget == 0 ){` |
|      3 | 6479 | `		return PH7_OK;` |
|      - | 6480 | `	}` |
|      7 | 6481 | `	if( nArg > 2 ){` |
|    ! 0 | 6482 | `		bAbsolute = DtValueTruth(pCtx->pVm,apArg[2]);` |
|    ! 0 | 6483 | `	}` |
|      7 | 6484 | `	return DtDiffResult(pCtx,pBase,pTarget,bAbsolute);` |
|      5 | 6485 | `}` |
|      4 | 6486 | `static int vm_builtin_date_timestamp_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6487 | `{` |
|      5 | 6488 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      5 | 6489 | `	if( pObj ){` |
|      3 | 6490 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_TS));` |
|      1 | 6491 | `	}` |
|      5 | 6492 | `	return PH7_OK;` |
|      1 | 6493 | `}` |
|      2 | 6494 | `static int vm_builtin_date_timestamp_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6495 | `{` |
|      3 | 6496 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      3 | 6497 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|      3 | 6498 | `		return PH7_OK;` |
|      - | 6499 | `	}` |
|    ! 0 | 6500 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,ph7_value_to_int64(apArg[1]));` |
|    ! 0 | 6501 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,0);` |
|    ! 0 | 6502 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 6503 | `	return PH7_OK;` |
|      2 | 6504 | `}` |
|      4 | 6505 | `static int vm_builtin_date_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6506 | `{` |
|      5 | 6507 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      5 | 6508 | `	if( pObj == 0 ){` |
|      3 | 6509 | `		return PH7_OK;` |
|      - | 6510 | `	}` |
|      3 | 6511 | `	return DtTimezoneResult(pCtx,pObj);` |
|      3 | 6512 | `}` |
|      2 | 6513 | `static int vm_builtin_date_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6514 | `{` |
|      3 | 6515 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      3 | 6516 | `	const char *zName = "UTC";` |
|      3 | 6517 | `	int nName = 3,iKind = DT_ZONE_ID;` |
|      3 | 6518 | `	sxi32 iOff = 0;` |
|      3 | 6519 | `	if( pObj == 0 \|\| nArg < 2 \|\| !DtZoneOf(apArg[1],&iOff,&zName,&nName,&iKind) ){` |
|    ! 0 | 6520 | `		return PH7_OK;` |
|      - | 6521 | `	}` |
|      3 | 6522 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_OFF,iOff);` |
|      3 | 6523 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,DT_NAME,zName,nName);` |
|      3 | 6524 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_ZKIND,iKind);` |
|      3 | 6525 | `	DtResultArg(pCtx,apArg);` |
|      3 | 6526 | `	return PH7_OK;` |
|      2 | 6527 | `}` |
|      4 | 6528 | `static int vm_builtin_date_offset_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6529 | `{` |
|      5 | 6530 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      5 | 6531 | `	if( pObj ){` |
|      3 | 6532 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_OFF));` |
|      1 | 6533 | `	}` |
|      5 | 6534 | `	return PH7_OK;` |
|      1 | 6535 | `}` |
|      2 | 6536 | `static int vm_builtin_date_date_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6537 | `{` |
|      3 | 6538 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      3 | 6539 | `	if( pObj == 0 \|\| nArg < 4 ){` |
|      3 | 6540 | `		return PH7_OK;` |
|      - | 6541 | `	}` |
|    ! 0 | 6542 | `	DtSetDateOf(pCtx,pObj,ph7_value_to_int64(apArg[1]),ph7_value_to_int(apArg[2]),` |
|    ! 0 | 6543 | `		ph7_value_to_int(apArg[3]));` |
|    ! 0 | 6544 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 6545 | `	return PH7_OK;` |
|      2 | 6546 | `}` |
|      2 | 6547 | `static int vm_builtin_date_time_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6548 | `{` |
|      3 | 6549 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      3 | 6550 | `	if( pObj == 0 \|\| nArg < 3 ){` |
|      3 | 6551 | `		return PH7_OK;` |
|      - | 6552 | `	}` |
|    ! 0 | 6553 | `	DtSetTimeOf(pCtx,pObj,ph7_value_to_int(apArg[1]),ph7_value_to_int(apArg[2]),` |
|    ! 0 | 6554 | `		nArg > 3 ? ph7_value_to_int(apArg[3]) : 0,` |
|    ! 0 | 6555 | `		nArg > 4 ? ph7_value_to_int64(apArg[4]) : 0);` |
|    ! 0 | 6556 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 6557 | `	return PH7_OK;` |
|      2 | 6558 | `}` |
|      4 | 6559 | `static int vm_builtin_date_isodate_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6560 | `{` |
|      5 | 6561 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      5 | 6562 | `	if( pObj == 0 \|\| nArg < 3 ){` |
|      3 | 6563 | `		return PH7_OK;` |
|      - | 6564 | `	}` |
|      5 | 6565 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,` |
|      3 | 6566 | `		DtIsoDate(PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),` |
|      2 | 6567 | `			ph7_value_to_int64(apArg[1]),ph7_value_to_int64(apArg[2]),` |
|      2 | 6568 | `			nArg > 3 ? ph7_value_to_int64(apArg[3]) : 1));` |
|      3 | 6569 | `	DtResultArg(pCtx,apArg);` |
|      3 | 6570 | `	return PH7_OK;` |
|      3 | 6571 | `}` |
|      - | 6572 | `/*` |
|      - | 6573 | ` * The COMPONENT view php's two parse readers answer with: what the scanner READ,` |
|      - | 6574 | ` * field by field, rather than what a constructor would make of it. Nothing about` |
|      - | 6575 | `` * the clock reaches it -- a field the string never mentioned is `false`, not the`` |
|      - | 6576 | ` * base moment's -- and three parts of the shape are conditional.` |
|      - | 6577 | ` *` |
|      - | 6578 | `` * `is_localtime` is whether a TIMEZONE token was seen at all, which is not the`` |
|      - | 6579 | ` * same as one having been understood: an unknown name sets it and leaves` |
|      - | 6580 | `` * `zone_type` 0, with nothing else shown. A fixed OFFSET shows `zone` and`` |
|      - | 6581 | `` * `is_dst`, an ABBREVIATION shows those and its `tz_abbr`, and an IDENTIFIER`` |
|      - | 6582 | `` * shows only its name, twice. And the `relative` block appears when the parse`` |
|      - | 6583 | `` * spelled a relative element -- `now` and `today` do not -- carrying the weekday`` |
|      - | 6584 | ` * when one was hunted, the business-day count when that special was named, and` |
|      - | 6585 | `` * the `first\|last day of` flag as `true`.`` |
|      - | 6586 | ` */` |
|      - | 6587 | `typedef struct dt_comp dt_comp;` |
|      - | 6588 | `struct dt_comp` |
|      - | 6589 | `{` |
|      - | 6590 | ``	sxi64 y,mo,d,h,mi,s,us;   /* DT_UNSET is php's own, and `false` on show */`` |
|      - | 6591 | `	int iZoneSeen;            /* php's is_localtime */` |
|      - | 6592 | `	int iZoneKind;            /* php's timezone_type, 0 when nothing resolved */` |
|      - | 6593 | `	sxi32 iOff;` |
|      - | 6594 | `	const char *zName;` |
|      - | 6595 | `	int nName;` |
|      - | 6596 | `	int bHaveRel;` |
|      - | 6597 | `	sxi64 ry,rm,rd,rh,ri,rs;` |
|      - | 6598 | `	int bWday,iWday;` |
|      - | 6599 | `	int bWeekdays;` |
|      - | 6600 | `	sxi64 iWeekdays;` |
|      - | 6601 | `	int iFirstLast;` |
|      - | 6602 | `};` |
|    224 | 6603 | `static int DtCompResult(ph7_context *pCtx,const dt_comp *pC,const phl_dt_lasterr *pRec)` |
|      1 | 6604 | `{` |
|      - | 6605 | `	ph7_value *pArr,*pRel,*pWarn,*pErrs,*pVal;` |
|      - | 6606 | `	int k;` |
|    225 | 6607 | `	pArr = ph7_context_new_array(pCtx);` |
|    225 | 6608 | `	pWarn = ph7_context_new_array(pCtx);` |
|    225 | 6609 | `	pErrs = ph7_context_new_array(pCtx);` |
|    225 | 6610 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    225 | 6611 | `	if( pArr == 0 \|\| pWarn == 0 \|\| pErrs == 0 \|\| pVal == 0 ){` |
|    ! 0 | 6612 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6613 | `	}` |
|      - | 6614 | `#define DT_PUT(zKey) ph7_array_add_strkey_elem(pArr,zKey,pVal)` |
|      - | 6615 | `#define DT_PUTFIELD(zKey,iVal) do{ \` |
|      - | 6616 | `		if( (iVal) == DT_UNSET ){ ph7_value_bool(pVal,0); } \` |
|      - | 6617 | `		else { ph7_value_int64(pVal,(iVal)); } \` |
|      - | 6618 | `		DT_PUT(zKey); \` |
|      - | 6619 | `	}while(0)` |
|    225 | 6620 | `	DT_PUTFIELD("year",pC->y);` |
|    225 | 6621 | `	DT_PUTFIELD("month",pC->mo);` |
|    225 | 6622 | `	DT_PUTFIELD("day",pC->d);` |
|    225 | 6623 | `	DT_PUTFIELD("hour",pC->h);` |
|    225 | 6624 | `	DT_PUTFIELD("minute",pC->mi);` |
|    225 | 6625 | `	DT_PUTFIELD("second",pC->s);` |
|    225 | 6626 | `	if( pC->us == DT_UNSET ){` |
|    135 | 6627 | `		ph7_value_bool(pVal,0);` |
|     68 | 6628 | `	}else{` |
|     91 | 6629 | `		ph7_value_double(pVal,(ph7_real)((double)pC->us / 1000000.0));` |
|      - | 6630 | `	}` |
|    225 | 6631 | `	DT_PUT("fraction");` |
|    245 | 6632 | `	for( k = 0 ; k < pRec->nWarnKept ; k++ ){` |
|     21 | 6633 | `		ph7_value_string(pVal,pRec->azWarn[k],-1);` |
|     21 | 6634 | `		ph7_array_add_intkey_elem(pWarn,pRec->aWarnPos[k],pVal);` |
|     21 | 6635 | `		ph7_value_reset_string_cursor(pVal);` |
|     11 | 6636 | `	}` |
|      - | 6637 | `	{` |
|    225 | 6638 | `		const phl_dt_diag_row *aRow = (const phl_dt_diag_row *)SyBlobData(&pRec->sErr);` |
|    305 | 6639 | `		for( k = 0 ; k < pRec->nErrKept ; k++ ){` |
|     81 | 6640 | `			ph7_value_string(pVal,aRow[k].zMsg,-1);` |
|     81 | 6641 | `			ph7_array_add_intkey_elem(pErrs,aRow[k].iPos,pVal);` |
|     81 | 6642 | `			ph7_value_reset_string_cursor(pVal);` |
|     41 | 6643 | `		}` |
|      - | 6644 | `	}` |
|    225 | 6645 | `	ph7_value_int(pVal,pRec->nWarn);` |
|    225 | 6646 | `	DT_PUT("warning_count");` |
|    225 | 6647 | `	ph7_array_add_strkey_elem(pArr,"warnings",pWarn);` |
|    225 | 6648 | `	ph7_value_int(pVal,pRec->nErr);` |
|    225 | 6649 | `	DT_PUT("error_count");` |
|    225 | 6650 | `	ph7_array_add_strkey_elem(pArr,"errors",pErrs);` |
|    225 | 6651 | `	ph7_value_bool(pVal,pC->iZoneSeen != 0);` |
|    225 | 6652 | `	DT_PUT("is_localtime");` |
|    225 | 6653 | `	if( pC->iZoneSeen ){` |
|     61 | 6654 | `		ph7_value_int(pVal,pC->iZoneKind);` |
|     61 | 6655 | `		DT_PUT("zone_type");` |
|     61 | 6656 | `		if( pC->iZoneKind == DT_ZONE_OFFSET \|\| pC->iZoneKind == DT_ZONE_ABBR ){` |
|     47 | 6657 | `			ph7_value_int64(pVal,(sxi64)pC->iOff);` |
|     47 | 6658 | `			DT_PUT("zone");` |
|     47 | 6659 | `			ph7_value_bool(pVal,0);   /* no tz database, so nothing is ever DST */` |
|     47 | 6660 | `			DT_PUT("is_dst");` |
|     23 | 6661 | `		}` |
|     61 | 6662 | `		if( pC->iZoneKind == DT_ZONE_ABBR \|\| pC->iZoneKind == DT_ZONE_ID ){` |
|     23 | 6663 | `			ph7_value_string(pVal,pC->zName,pC->nName);` |
|     23 | 6664 | `			DT_PUT("tz_abbr");` |
|     23 | 6665 | `			ph7_value_reset_string_cursor(pVal);` |
|     11 | 6666 | `		}` |
|     61 | 6667 | `		if( pC->iZoneKind == DT_ZONE_ID ){` |
|      7 | 6668 | `			ph7_value_string(pVal,pC->zName,pC->nName);` |
|      7 | 6669 | `			DT_PUT("tz_id");` |
|      7 | 6670 | `			ph7_value_reset_string_cursor(pVal);` |
|      3 | 6671 | `		}` |
|     30 | 6672 | `	}` |
|    225 | 6673 | `	if( pC->bHaveRel ){` |
|     53 | 6674 | `		pRel = ph7_context_new_array(pCtx);` |
|     53 | 6675 | `		if( pRel == 0 ){` |
|    ! 0 | 6676 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6677 | `		}` |
|      - | 6678 | `#define DT_PUTREL(zKey,iVal) do{ \` |
|      - | 6679 | `			ph7_value_int64(pVal,(iVal)); \` |
|      - | 6680 | `			ph7_array_add_strkey_elem(pRel,zKey,pVal); \` |
|      - | 6681 | `		}while(0)` |
|     53 | 6682 | `		DT_PUTREL("year",pC->ry);` |
|     53 | 6683 | `		DT_PUTREL("month",pC->rm);` |
|     53 | 6684 | `		DT_PUTREL("day",pC->rd);` |
|     53 | 6685 | `		DT_PUTREL("hour",pC->rh);` |
|     53 | 6686 | `		DT_PUTREL("minute",pC->ri);` |
|     53 | 6687 | `		DT_PUTREL("second",pC->rs);` |
|     53 | 6688 | `		if( pC->bWday ){` |
|     23 | 6689 | `			DT_PUTREL("weekday",(sxi64)pC->iWday);` |
|     11 | 6690 | `		}` |
|     53 | 6691 | `		if( pC->bWeekdays ){` |
|      5 | 6692 | `			DT_PUTREL("weekdays",pC->iWeekdays);` |
|      2 | 6693 | `		}` |
|     53 | 6694 | `		if( pC->iFirstLast ){` |
|      5 | 6695 | `			ph7_value_bool(pVal,1);` |
|      7 | 6696 | `			ph7_array_add_strkey_elem(pRel,` |
|      4 | 6697 | `				pC->iFirstLast == 1 ? "first_day_of_month" : "last_day_of_month",pVal);` |
|      2 | 6698 | `		}` |
|      - | 6699 | `#undef DT_PUTREL` |
|     53 | 6700 | `		ph7_array_add_strkey_elem(pArr,"relative",pRel);` |
|     26 | 6701 | `	}` |
|      - | 6702 | `#undef DT_PUTFIELD` |
|      - | 6703 | `#undef DT_PUT` |
|    225 | 6704 | `	ph7_result_value(pCtx,pArr);` |
|    225 | 6705 | `	return PH7_OK;` |
|    113 | 6706 | `}` |
|      - | 6707 | `/*` |
|      - | 6708 | ` * date_parse(): the same scanner every constructor runs, showing what it read.` |
|      - | 6709 | ` * The diagnostics are the scan's own (see DtParseFields) and are NOT published` |
|      - | 6710 | ` * as getLastErrors() -- php leaves that record to the constructors -- so the` |
|      - | 6711 | ` * scan writes into one of this call's own.` |
|      - | 6712 | ` */` |
|    102 | 6713 | `static int vm_builtin_date_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6714 | `{` |
|    103 | 6715 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 6716 | `	phl_dt_lasterr sRec;` |
|      - | 6717 | `	dt_parsed sVec;` |
|      - | 6718 | `	dt_comp sC;` |
|    103 | 6719 | `	const char *zIn = "";` |
|    103 | 6720 | `	sxi64 iTs = 0;` |
|    103 | 6721 | `	sxi32 iOff = 0;` |
|    103 | 6722 | `	int nIn = 0,bOffSet = 0,uSec = 0,rc;` |
|    103 | 6723 | `	if( nArg < 1 ){` |
|    ! 0 | 6724 | `		return PH7_OK;` |
|      - | 6725 | `	}` |
|    103 | 6726 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    103 | 6727 | `	SyZero(&sRec,sizeof(sRec));` |
|    103 | 6728 | `	SyBlobInit(&sRec.sErr,&pVm->sAllocator);` |
|    103 | 6729 | `	DtFieldsInit(&sVec,0);` |
|    103 | 6730 | `	DtParseEx(zIn,nIn,0,0,0,0,&iTs,&iOff,&bOffSet,&uSec,&sVec,&sRec);` |
|    103 | 6731 | `	SyZero(&sC,sizeof(sC));` |
|    103 | 6732 | `	sC.y = sVec.y; sC.mo = sVec.m; sC.d = sVec.d;` |
|    103 | 6733 | `	sC.h = sVec.h; sC.mi = sVec.i; sC.s = sVec.s;` |
|    103 | 6734 | `	sC.us = sVec.bUsUnset ? DT_UNSET : sVec.us;` |
|    103 | 6735 | `	sC.iZoneSeen = sVec.nZoneTok > 0;` |
|    165 | 6736 | `	sC.iZoneKind = sVec.bOffSet == 0 ? 0` |
|     73 | 6737 | `		: (sVec.bOffSet == 1 ? DT_ZONE_OFFSET` |
|     13 | 6738 | `		   : (sVec.bZoneIdent ? DT_ZONE_ID : DT_ZONE_ABBR));` |
|    103 | 6739 | `	sC.iOff = sVec.iOff;` |
|    103 | 6740 | `	sC.zName = sVec.zZone;` |
|    103 | 6741 | `	sC.nName = sVec.nZone;` |
|    103 | 6742 | `	sC.bHaveRel = sVec.bHaveRel;` |
|    103 | 6743 | `	sC.ry = sVec.ry; sC.rm = sVec.rm; sC.rd = sVec.rd;` |
|    103 | 6744 | `	sC.rh = sVec.rh; sC.ri = sVec.ri; sC.rs = sVec.rs;` |
|    103 | 6745 | `	sC.bWday = sVec.bWday; sC.iWday = sVec.iWday;` |
|    103 | 6746 | `	sC.bWeekdays = sVec.bWeekdays; sC.iWeekdays = sVec.iWeekdays;` |
|    103 | 6747 | `	sC.iFirstLast = sVec.iFirstLast;` |
|    103 | 6748 | `	rc = DtCompResult(pCtx,&sC,&sRec);` |
|    103 | 6749 | `	SyBlobRelease(&sRec.sErr);` |
|    103 | 6750 | `	return rc;` |
|     52 | 6751 | `}` |
|      - | 6752 | `/*` |
|      - | 6753 | ` * date_parse_from_format(): the FORMAT scanner's components, through the very` |
|      - | 6754 | ` * presenter date_parse() answers with -- php reads both of them out of one` |
|      - | 6755 | `` * function, so a field the scan left unset is `false` in either, and the`` |
|      - | 6756 | `` * `relative` block appears here for exactly one reason: a textual DAY, which`` |
|      - | 6757 | ` * this parser records as a weekday to move to rather than as a day.` |
|      - | 6758 | ` *` |
|      - | 6759 | ` * Like date_parse(), it publishes NOTHING into getLastErrors() -- php leaves` |
|      - | 6760 | ` * that record to the constructors -- so the scan's diagnostics go into a record` |
|      - | 6761 | ` * of this call's own.` |
|      - | 6762 | ` */` |
|    122 | 6763 | `static int vm_builtin_date_parse_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6764 | `{` |
|    123 | 6765 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 6766 | `	phl_dt_lasterr sRec;` |
|      - | 6767 | `	dt_ff_res sRes;` |
|      - | 6768 | `	dt_comp sC;` |
|      - | 6769 | `	const char *zFmt,*zIn;` |
|      - | 6770 | `	int nFmt,nIn,rc;` |
|    123 | 6771 | `	if( nArg < 2 ){` |
|    ! 0 | 6772 | `		return PH7_OK;` |
|      - | 6773 | `	}` |
|    123 | 6774 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    123 | 6775 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    123 | 6776 | `	DtFromFormat(zFmt,nFmt,zIn,nIn,&sRes);` |
|    123 | 6777 | `	SyZero(&sRec,sizeof(sRec));` |
|    123 | 6778 | `	SyBlobInit(&sRec.sErr,&pVm->sAllocator);` |
|    123 | 6779 | `	DtFfDiagInto(&sRec,&sRes.sDiag);` |
|    123 | 6780 | `	SyZero(&sC,sizeof(sC));` |
|    123 | 6781 | `	sC.y = sRes.y; sC.mo = sRes.mo; sC.d = sRes.d;` |
|    123 | 6782 | `	sC.h = sRes.h; sC.mi = sRes.mi; sC.s = sRes.s; sC.us = sRes.us;` |
|    123 | 6783 | `	sC.iZoneSeen = sRes.bLocal;` |
|    123 | 6784 | `	sC.iZoneKind = sRes.iOffKind;` |
|    123 | 6785 | `	sC.iOff = sRes.iOff;` |
|    123 | 6786 | `	sC.zName = sRes.zName;` |
|    123 | 6787 | `	sC.nName = sRes.nName;` |
|    123 | 6788 | `	sC.bHaveRel = sRes.bWday;` |
|    123 | 6789 | `	sC.bWday = sRes.bWday;` |
|    123 | 6790 | `	sC.iWday = (int)sRes.iWday;` |
|    123 | 6791 | `	rc = DtCompResult(pCtx,&sC,&sRec);` |
|    123 | 6792 | `	SyBlobRelease(&sRec.sErr);` |
|    123 | 6793 | `	return rc;` |
|     62 | 6794 | `}` |
|      - | 6795 | `/* date_interval_create_from_date_string(): warns and answers false where the` |
|      - | 6796 | ` * method throws. */` |
|     10 | 6797 | `static int vm_builtin_date_interval_create_from_date_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6798 | `{` |
|     11 | 6799 | `	const char *zIn,*zReason = "";` |
|     11 | 6800 | `	int nIn,iPos = 0,bNonRel = 0;` |
|     11 | 6801 | `	char cAt = ' ';` |
|      - | 6802 | `	ph7_class_instance *pObj;` |
|     11 | 6803 | `	if( nArg < 1 ){` |
|    ! 0 | 6804 | `		return PH7_OK;` |
|      - | 6805 | `	}` |
|     11 | 6806 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|     11 | 6807 | `	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason,&bNonRel);` |
|     11 | 6808 | `	if( pObj == 0 ){` |
|      3 | 6809 | `		if( bNonRel ){` |
|      4 | 6810 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 6811 | `				"date_interval_create_from_date_string(): String '%.*s' contains "` |
|      1 | 6812 | `				"non-relative elements",DtCStrLen(zIn,nIn),zIn);` |
|      3 | 6813 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6814 | `			return PH7_OK;` |
|      - | 6815 | `		}` |
|    ! 0 | 6816 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 6817 | `			"date_interval_create_from_date_string(): Unknown or bad format (%.*s) "` |
|    ! 0 | 6818 | `			"at position %d (%c): %s",DtCStrLen(zIn,nIn),zIn,iPos,cAt,zReason);` |
|    ! 0 | 6819 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6820 | `		return PH7_OK;` |
|      - | 6821 | `	}` |
|      9 | 6822 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      9 | 6823 | `	return PH7_OK;` |
|      6 | 6824 | `}` |
|      6 | 6825 | `static int vm_builtin_date_interval_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6826 | `{` |
|      7 | 6827 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      - | 6828 | `	const char *zFmt;` |
|      - | 6829 | `	int nFmt;` |
|      7 | 6830 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|      3 | 6831 | `		return PH7_OK;` |
|      - | 6832 | `	}` |
|      5 | 6833 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|      5 | 6834 | `	DtIvFormat(pCtx,pObj,zFmt,nFmt);` |
|      5 | 6835 | `	return PH7_OK;` |
|      4 | 6836 | `}` |
|    ! 0 | 6837 | `static int vm_builtin_date_get_last_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 6838 | `{` |
|    ! 0 | 6839 | `	return vm_builtin_DateTime_getLastErrors(pCtx,nArg,apArg);` |
|    ! 0 | 6840 | `}` |
|      - | 6841 | `/* timezone_open(): warns and answers false where the constructor throws. */` |
|     72 | 6842 | `static int vm_builtin_timezone_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6843 | `{` |
|     73 | 6844 | `	ph7_vm *pVm = pCtx->pVm;` |
|     73 | 6845 | `	ph7_class *pClass = DtClass(pVm,"DateTimeZone");` |
|      - | 6846 | `	ph7_class_instance *pObj;` |
|      - | 6847 | `	const char *zTz,*zName;` |
|     73 | 6848 | `	int nTz,nName,iKind = DT_ZONE_ID,rc;` |
|     73 | 6849 | `	sxi32 iOff = 0;` |
|      - | 6850 | `	char zBuf[16];` |
|     73 | 6851 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 6852 | `		return PH7_OK;` |
|      - | 6853 | `	}` |
|     73 | 6854 | `	zTz = ph7_value_to_string(apArg[0],&nTz);` |
|     73 | 6855 | `	rc = DtZoneParse(zTz,nTz,&iOff,&zName,&nName,&iKind,zBuf,sizeof(zBuf));` |
|     73 | 6856 | `	if( rc != 0 ){` |
|    100 | 6857 | `		PH7_VmThrowWarningFmt(pVm,` |
|     33 | 6858 | `			rc == -2 ? "timezone_open(): Timezone offset is out of range (%.*s)"` |
|     33 | 6859 | `			         : "timezone_open(): Unknown or bad timezone (%.*s)",nTz,zTz);` |
|     67 | 6860 | `		ph7_result_bool(pCtx,0);` |
|     67 | 6861 | `		return PH7_OK;` |
|      - | 6862 | `	}` |
|      7 | 6863 | `	pObj = DtNewInstance(pVm,pClass);` |
|      7 | 6864 | `	if( pObj == 0 ){` |
|    ! 0 | 6865 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6866 | `	}` |
|      7 | 6867 | `	PH7_NativeSetAttrInt(pVm,pObj,DTZ_OFF,iOff);` |
|      7 | 6868 | `	PH7_NativeSetAttrStr(pVm,pObj,DTZ_NAME,zName,nName);` |
|      7 | 6869 | `	PH7_NativeSetAttrInt(pVm,pObj,DTZ_KIND,iKind);` |
|      7 | 6870 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      7 | 6871 | `	return PH7_OK;` |
|     37 | 6872 | `}` |
|      6 | 6873 | `static int vm_builtin_timezone_name_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6874 | `{` |
|      7 | 6875 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      - | 6876 | `	const char *zName;` |
|      - | 6877 | `	int nName;` |
|      7 | 6878 | `	if( pObj == 0 ){` |
|      3 | 6879 | `		return PH7_OK;` |
|      - | 6880 | `	}` |
|      5 | 6881 | `	PH7_NativeAttrStr(pObj,DTZ_NAME,&zName,&nName);` |
|      5 | 6882 | `	ph7_result_string(pCtx,zName,nName);` |
|      5 | 6883 | `	return PH7_OK;` |
|      4 | 6884 | `}` |
|      6 | 6885 | `static int vm_builtin_timezone_offset_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6886 | `{` |
|      7 | 6887 | `	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);` |
|      7 | 6888 | `	if( nArg > 1 && DtArgInit(pCtx,apArg[1]) != 0 ){` |
|      3 | 6889 | `		return PH7_OK;` |
|      - | 6890 | `	}` |
|      5 | 6891 | `	if( pObj ){` |
|      3 | 6892 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DTZ_OFF));` |
|      1 | 6893 | `	}` |
|      5 | 6894 | `	return PH7_OK;` |
|      4 | 6895 | `}` |
|      - | 6896 | `/*` |
|      - | 6897 | ` * The four private slots a date object keeps its state in. Both classes declare` |
|      - | 6898 | `` * them: `trait __DtCoreT` had no native equivalent, and replaying the table is`` |
|      - | 6899 | `` * exactly what `use __DtCoreT` did.`` |
|      - | 6900 | ` */` |
|      - | 6901 | `#define DT_NATIVE_STATE_PROPS \` |
|      - | 6902 | `	{ DT_TS,   PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \` |
|      - | 6903 | `	{ DT_OFF,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \` |
|      - | 6904 | `	{ DT_NAME, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 }, 0 }, \` |
|      - | 6905 | `	{ DT_US,   PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \` |
|      - | 6906 | `	{ DT_ZKIND,PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, DT_ZONE_ID, 0, 0.0 }, 0 }, \` |
|      - | 6907 | `	{ DT_INIT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }` |
|      - | 6908 | `/*` |
|      - | 6909 | ` * The methods DateTime and DateTimeImmutable share -- the whole of the old trait` |
|      - | 6910 | ` * plus the mutators, whose one difference (write $this, or write a clone) the` |
|      - | 6911 | ` * bodies decide from the receiver's class. php's own signatures: they are the` |
|      - | 6912 | ` * single source of truth for arity, coercion and Reflection here, so the casts the` |
|      - | 6913 | `` * chunk wrote by hand (`(string)$format`, `(int)$timestamp`) are declared types now`` |
|      - | 6914 | ` * and the methods reject what php rejects.` |
|      - | 6915 | ` */` |
|      - | 6916 | `/* The methods DateTime and DateTimeImmutable share. CLS is the OWNING class name` |
|      - | 6917 | ` * as a string literal, because php's stubs write the concrete class rather than` |
|      - | 6918 | `` * `static` for the legacy mutators -- DateTime::add reports DateTime and`` |
|      - | 6919 | ` * DateTimeImmutable::add reports DateTimeImmutable. The two that php really does` |
|      - | 6920 | `` * declare `static` (setMicrosecond) and the two it declares for real rather than`` |
|      - | 6921 | ` * tentatively (getMicrosecond, and setMicrosecond again) are written as they are:` |
|      - | 6922 | `` * a leading `@` is php's @tentative-return-type, and nearly every method here has`` |
|      - | 6923 | ` * one. */` |
|      - | 6924 | `#define DT_NATIVE_SHARED_METHODS(CLS) \` |
|      - | 6925 | `	{ "__construct",     PH7_MOD_PUBLIC, "string $datetime = 'now', ?DateTimeZone $timezone = null", "", \` |
|      - | 6926 | `	  vm_builtin_DateTime_construct }, \` |
|      - | 6927 | `	{ "format",          PH7_MOD_PUBLIC, "string $format", "@string", vm_builtin_DateTime_format }, \` |
|      - | 6928 | `	{ "getTimestamp",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_DateTime_getTimestamp }, \` |
|      - | 6929 | `	{ "getMicrosecond",  PH7_MOD_PUBLIC, "", "int", vm_builtin_DateTime_getMicrosecond }, \` |
|      - | 6930 | `	{ "getOffset",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_DateTime_getOffset }, \` |
|      - | 6931 | `	{ "getTimezone",     PH7_MOD_PUBLIC, "", "@DateTimeZone\|false", vm_builtin_DateTime_getTimezone }, \` |
|      - | 6932 | `	{ "diff",            PH7_MOD_PUBLIC, "DateTimeInterface $targetObject, bool $absolute = false", \` |
|      - | 6933 | `	  "@DateInterval", vm_builtin_DateTime_diff }, \` |
|      - | 6934 | `	{ "modify",          PH7_MOD_PUBLIC, "string $modifier", "@" CLS, vm_builtin_DateTime_modify }, \` |
|      - | 6935 | `	{ "setTimestamp",    PH7_MOD_PUBLIC, "int $timestamp", "@" CLS, vm_builtin_DateTime_setTimestamp }, \` |
|      - | 6936 | `	{ "setMicrosecond",  PH7_MOD_PUBLIC, "int $microsecond", "static", vm_builtin_DateTime_setMicrosecond }, \` |
|      - | 6937 | `	{ "setTimezone",     PH7_MOD_PUBLIC, "DateTimeZone $timezone", "@" CLS, vm_builtin_DateTime_setTimezone }, \` |
|      - | 6938 | `	{ "setDate",         PH7_MOD_PUBLIC, "int $year, int $month, int $day", "@" CLS, \` |
|      - | 6939 | `	  vm_builtin_DateTime_setDate }, \` |
|      - | 6940 | `	{ "setTime",         PH7_MOD_PUBLIC, \` |
|      - | 6941 | `	  "int $hour, int $minute, int $second = 0, int $microsecond = 0", "@" CLS, \` |
|      - | 6942 | `	  vm_builtin_DateTime_setTime }, \` |
|      - | 6943 | `	{ "setISODate",      PH7_MOD_PUBLIC, "int $year, int $week, int $dayOfWeek = 1", "@" CLS, \` |
|      - | 6944 | `	  vm_builtin_DateTime_setISODate }, \` |
|      - | 6945 | `	{ "add",             PH7_MOD_PUBLIC, "DateInterval $interval", "@" CLS, vm_builtin_DateTime_add }, \` |
|      - | 6946 | `	{ "sub",             PH7_MOD_PUBLIC, "DateInterval $interval", "@" CLS, vm_builtin_DateTime_sub }, \` |
|      - | 6947 | `	{ "getLastErrors",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "@array\|false", \` |
|      - | 6948 | `	  vm_builtin_DateTime_getLastErrors }` |
|      - | 6949 | `/*` |
|      - | 6950 | ` * php's add_common_properties(): after the presented shape, the instance's own` |
|      - | 6951 | ` * php-visible slots -- a SUBCLASS's declared properties, which php serializes` |
|      - | 6952 | ` * alongside the internal state. A key the presented shape already wrote WINS` |
|      - | 6953 | ` * (zend_hash_add, not update), and a hidden engine slot is never a candidate:` |
|      - | 6954 | ` * this is the one walk in the date family that must skip them.` |
|      - | 6955 | ` */` |
|    684 | 6956 | `static void DtAddCommonProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|      2 | 6957 | `{` |
|    342 | 6958 | `	SXUNUSED(pVm);` |
|    686 | 6959 | `	if( (pOut->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 6960 | `		return;` |
|      - | 6961 | `	}` |
|      - | 6962 | `	/* MANGLED, the way php mangles a non-public property everywhere it hands an` |
|      - | 6963 | ``	 * object's own table out: a subclass's `protected $p` serializes as`` |
|      - | 6964 | `	 * "\0*\0p" and casts to that key, and only the mangling keeps two same-named` |
|      - | 6965 | `	 * members from different visibility levels apart. */` |
|    686 | 6966 | `	PH7_ClassInstanceOwnPropsToHashmap(pThis,(ph7_hashmap *)pOut->x.pOther);` |
|    344 | 6967 | `}` |
|      - | 6968 | `/*` |
|      - | 6969 | ` * The INTERNAL shape of a class whose state is its own public properties -- the` |
|      - | 6970 | ` * ones the named class declares, in declared order, read off the instance.` |
|      - | 6971 | ` *` |
|      - | 6972 | ` * Not a walk of the instance's TABLE: a subclass that redeclares one of the names` |
|      - | 6973 | ` * takes over its slot and its POSITION, and php still shows the value where its` |
|      - | 6974 | ` * own shape puts it.` |
|      - | 6975 | ` */` |
|     46 | 6976 | `static void DtAddNativeProps(ph7_vm *pVm,ph7_class_instance *pThis,const char *zBase,ph7_value *pOut)` |
|      2 | 6977 | `{` |
|     48 | 6978 | `	ph7_class *pClass = DtClass(&(*pVm),zBase);` |
|      - | 6979 | `	SyHashEntry *pEntry;` |
|     48 | 6980 | `	if( pClass == 0 ){` |
|    ! 0 | 6981 | `		return;` |
|      - | 6982 | `	}` |
|     48 | 6983 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    576 | 6984 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    530 | 6985 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      - | 6986 | `		ph7_value *pVal;` |
|      - | 6987 | `		ph7_value sKey;` |
|      - | 6988 | `		SyHashEntry *pOwn;` |
|    530 | 6989 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|    112 | 6990 | `			continue;` |
|      - | 6991 | `		}` |
|      - | 6992 | `		/* ...and a slot THIS object hides is not part of its shape either: a` |
|      - | 6993 | `		 * from-string interval serializes as the two names php writes. */` |
|    452 | 6994 | `		pOwn = SyHashGet(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName));` |
|    452 | 6995 | `		if( pOwn && (((VmClassAttr *)pOwn->pUserData)->iState & VM_CLASS_ATTR_UNSEEN) ){` |
|     37 | 6996 | `			continue;` |
|      - | 6997 | `		}` |
|    416 | 6998 | `		pVal = PH7_ClassInstanceFetchAttr(pThis,&pAttr->sName);` |
|    416 | 6999 | `		if( pVal == 0 ){` |
|     30 | 7000 | `			continue;` |
|      - | 7001 | `		}` |
|    388 | 7002 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    388 | 7003 | `		PH7_MemObjStringAppend(&sKey,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName));` |
|    388 | 7004 | `		ph7_array_add_elem(pOut,&sKey,pVal);` |
|    388 | 7005 | `		PH7_MemObjRelease(&sKey);` |
|      2 | 7006 | `	}` |
|     25 | 7007 | `}` |
|      - | 7008 | `/*` |
|      - | 7009 | ` * php's presentation for the date classes (ph7_class::xPresent).` |
|      - | 7010 | ` *` |
|      - | 7011 | ` * php keeps a timelib struct and SHOWS date/timezone_type/timezone; PHL keeps a` |
|      - | 7012 | ` * timestamp, an offset, a zone name and microseconds, all hidden. These build php's` |
|      - | 7013 | ` * shape out of that state, so var_dump/print_r, var_export and the (array) cast` |
|      - | 7014 | ` * agree with the oracle without changing what the C bodies read.` |
|      - | 7015 | ` *` |
|      - | 7016 | ` * timezone_type is php's own three-way tag: 1 = a fixed UTC OFFSET ("+02:00"),` |
|      - | 7017 | ` * 2 = an ABBREVIATION ("GMT", "Z"), 3 = an IDENTIFIER ("UTC", "Europe/Paris").` |
|      - | 7018 | ` * PHL accepts offsets, UTC, GMT and Z today; the identifier arm is written for the` |
|      - | 7019 | ` * whole rule so a tz database can only add names, never change the tagging.` |
|      - | 7020 | ` */` |
|   1546 | 7021 | `static void DtPresentPut(ph7_vm *pVm,ph7_value *pOut,const char *zKey,ph7_value *pVal)` |
|      1 | 7022 | `{` |
|      - | 7023 | `	ph7_value sKey;` |
|   1547 | 7024 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|   1547 | 7025 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|   1547 | 7026 | `	ph7_array_add_elem(pOut,&sKey,pVal);` |
|   1547 | 7027 | `	PH7_MemObjRelease(&sKey);` |
|   1547 | 7028 | `}` |
|    574 | 7029 | `static void DtPresentZone(ph7_vm *pVm,ph7_value *pOut,const char *zName,int nName,int iKind)` |
|      1 | 7030 | `{` |
|      - | 7031 | `	ph7_value sVal;` |
|    575 | 7032 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,iKind);` |
|    575 | 7033 | `	DtPresentPut(&(*pVm),pOut,"timezone_type",&sVal);` |
|    575 | 7034 | `	PH7_MemObjRelease(&sVal);` |
|    575 | 7035 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,0);` |
|    575 | 7036 | `	PH7_MemObjStringAppend(&sVal,zName,(sxu32)nName);` |
|    575 | 7037 | `	DtPresentPut(&(*pVm),pOut,"timezone",&sVal);` |
|    575 | 7038 | `	PH7_MemObjRelease(&sVal);` |
|    575 | 7039 | `}` |
|      - | 7040 | `/*` |
|      - | 7041 | ` * php builds this shape FROM the struct the constructor allocates, so an object` |
|      - | 7042 | ` * that has none contributes nothing to it -- var_dump, print_r, var_export, the` |
|      - | 7043 | ` * (array) cast and json_encode all answer an empty shape where PHL published a` |
|      - | 7044 | ` * 1970 date nothing had asked for.` |
|      - | 7045 | ` */` |
|    418 | 7046 | `static void DtDateTimeShape(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|      1 | 7047 | `{` |
|      - | 7048 | `	dt_state sState;` |
|      - | 7049 | `	Sytm sTm;` |
|      - | 7050 | `	char zZone[64];` |
|      - | 7051 | `	char zDate[64];` |
|      - | 7052 | `	ph7_value sVal;` |
|      - | 7053 | `	int nName;` |
|    419 | 7054 | `	if( !DtIsInit(pThis) ){` |
|     21 | 7055 | `		return;` |
|      - | 7056 | `	}` |
|    399 | 7057 | `	DtLoad(pThis,&sState);` |
|    399 | 7058 | `	nName = sState.nName;` |
|    399 | 7059 | `	if( nName >= (int)sizeof(zZone) ){` |
|    ! 0 | 7060 | `		nName = (int)sizeof(zZone) - 1;` |
|    ! 0 | 7061 | `	}` |
|    399 | 7062 | `	if( nName > 0 ){` |
|    399 | 7063 | `		SyMemcpy(sState.zName,zZone,(sxu32)nName);` |
|    199 | 7064 | `	}` |
|    399 | 7065 | `	zZone[nName] = 0;` |
|    399 | 7066 | `	DtFillSytm(sState.iTs,sState.iOff,zZone,&sTm);` |
|      - | 7067 | `	/* php's fixed shape here, not a format string: "Y-m-d H:i:s.uuuuuu". */` |
|    598 | 7068 | `	SyBufferFormat(zDate,sizeof(zDate),"%04qd-%02d-%02d %02d:%02d:%02d.%06d",` |
|    398 | 7069 | `		sTm.tm_year,sTm.tm_mon + 1,sTm.tm_mday,sTm.tm_hour,sTm.tm_min,sTm.tm_sec,` |
|    199 | 7070 | `		sState.uSec);` |
|    399 | 7071 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,0);` |
|    399 | 7072 | `	PH7_MemObjStringAppend(&sVal,zDate,(sxu32)SyStrlen(zDate));` |
|    399 | 7073 | `	DtPresentPut(&(*pVm),pOut,"date",&sVal);` |
|    399 | 7074 | `	PH7_MemObjRelease(&sVal);` |
|    399 | 7075 | `	DtPresentZone(&(*pVm),pOut,zZone,nName,sState.iZoneKind);` |
|    210 | 7076 | `}` |
|      - | 7077 | `/*` |
|      - | 7078 | ` * The hook itself: php's get_properties starts from the object's OWN table and` |
|      - | 7079 | ` * writes the struct's keys into it, so a subclass's properties come FIRST and a` |
|      - | 7080 | `` * subclass property named `date` keeps its position while taking the internal`` |
|      - | 7081 | ` * value. PHL built the three keys alone, so every property a subclass declared was` |
|      - | 7082 | ` * missing from var_dump, var_export, the (array) cast and json_encode.` |
|      - | 7083 | ` */` |
|    366 | 7084 | `static sxi32 DtPresentDateTime(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      1 | 7085 | `{` |
|    183 | 7086 | `	SXUNUSED(bDebug); /* php shows the same three keys to both handlers */` |
|    367 | 7087 | `	DtAddCommonProps(&(*pVm),pThis,pOut);` |
|    367 | 7088 | `	DtDateTimeShape(&(*pVm),pThis,pOut);` |
|    367 | 7089 | `	return SXRET_OK;` |
|      1 | 7090 | `}` |
|      - | 7091 | `/*` |
|      - | 7092 | ` * DateInterval and DatePeriod present their OWN property table -- php's state for` |
|      - | 7093 | ` * these two is the visible properties themselves -- so the hook exists for one` |
|      - | 7094 | ` * reason: an object that was never constructed has no table at all there, and` |
|      - | 7095 | ` * showed ten (or seven) default fields here. The initialized case is the slot walk` |
|      - | 7096 | ` * the cast already fell back to, so nothing else about them changes.` |
|      - | 7097 | ` */` |
|     86 | 7098 | `static sxi32 DtPresentProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      2 | 7099 | `{` |
|     43 | 7100 | `	SXUNUSED(bDebug);` |
|     88 | 7101 | `	if( DtIsInit(pThis) ){` |
|     58 | 7102 | `		PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pOut->x.pOther);` |
|     30 | 7103 | `	}else{` |
|      - | 7104 | `		/* No table there at all -- but a subclass's own properties are the` |
|      - | 7105 | `		 * object's, and php still shows those. */` |
|     31 | 7106 | `		DtAddCommonProps(&(*pVm),pThis,pOut);` |
|      - | 7107 | `	}` |
|     88 | 7108 | `	return SXRET_OK;` |
|      2 | 7109 | `}` |
|    184 | 7110 | `static void DtTimeZoneShape(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|      1 | 7111 | `{` |
|    185 | 7112 | `	const char *zName = 0;` |
|    185 | 7113 | `	int nName = 0;` |
|    185 | 7114 | `	if( !DtIsInit(pThis) ){` |
|      9 | 7115 | `		return;` |
|      - | 7116 | `	}` |
|    177 | 7117 | `	PH7_NativeAttrStr(pThis,DTZ_NAME,&zName,&nName);` |
|    177 | 7118 | `	DtPresentZone(&(*pVm),pOut,zName ? zName : "",nName,` |
|     88 | 7119 | `		DtZoneKindOf(pThis,DTZ_KIND,zName,nName));` |
|     93 | 7120 | `}` |
|    120 | 7121 | `static sxi32 DtPresentTimeZone(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      1 | 7122 | `{` |
|     60 | 7123 | `	SXUNUSED(bDebug);` |
|    121 | 7124 | `	DtAddCommonProps(&(*pVm),pThis,pOut);   /* see DtPresentDateTime */` |
|    121 | 7125 | `	DtTimeZoneShape(&(*pVm),pThis,pOut);` |
|    121 | 7126 | `	return SXRET_OK;` |
|      1 | 7127 | `}` |
|      - | 7128 | `/*` |
|      - | 7129 | ` * ---------------------------------------------------------------------------` |
|      - | 7130 | ` * php's serialization pair for the three date classes whose state is HIDDEN.` |
|      - | 7131 | ` *` |
|      - | 7132 | ` * serialize() had been walking the engine slots, so a DateTime round-tripped as` |
|      - | 7133 | `` * `__dtTs`/`__dtOff`/`__dtName`/`__dtUs` and a payload php WROTE could not be read`` |
|      - | 7134 | `` * back at all -- `unserialize('O:8:"DateTime":3:{s:4:"date";…}')` found none of the`` |
|      - | 7135 | ` * names it wanted, silently kept the 1970 defaults and answered a valid object with` |
|      - | 7136 | ` * the wrong instant. php's answer is not a hidden-slot rule but a pair of methods:` |
|      - | 7137 | ` * __serialize() hands back the PRESENTED shape (date/timezone_type/timezone, the` |
|      - | 7138 | ` * same hash date_object_get_properties_for builds) and __unserialize() re-parses it,` |
|      - | 7139 | ` * so the payload is the class's public model rather than its storage.` |
|      - | 7140 | ` *` |
|      - | 7141 | ` * The four methods php declares are all here, because they are one contract:` |
|      - | 7142 | ` * __serialize/__unserialize is what serialize() uses, __wakeup reads a LEGACY` |
|      - | 7143 | ` * payload out of the object's own properties, and __set_state is what var_export's` |
|      - | 7144 | `` * `\DateTime::__set_state(array(…))` text evaluates to. All four fail with the same`` |
|      - | 7145 | `` * plain `Error`, and php's sentence for it names the class.`` |
|      - | 7146 | ` * ---------------------------------------------------------------------------` |
|      - | 7147 | ` */` |
|      - | 7148 | `/*` |
|      - | 7149 | ` * ---------------------------------------------------------------------------` |
|      - | 7150 | ` * How the date classes COMPARE (ph7_class::xCmp -- php's compare handlers).` |
|      - | 7151 | ` *` |
|      - | 7152 | ` * php compares a date object by what it MEANS, not by what it stores, and the` |
|      - | 7153 | ` * property walk PHL fell back to disagreed with every one of them: a DateTime` |
|      - | 7154 | ` * and a DateTimeImmutable of the same instant were unequal here because the` |
|      - | 7155 | ` * classes differ, two dates one second apart in different zones were unequal` |
|      - | 7156 | `` * because the zone NAME is a property, two `P1D` intervals were equal where php`` |
|      - | 7157 | ` * refuses to compare intervals at all, and two DateTimeZones ordered by name` |
|      - | 7158 | ` * where php refuses to compare different KINDS of zone.` |
|      - | 7159 | ` *` |
|      - | 7160 | ` * All three handlers screen their partner by INSTANCE OF, not by class` |
|      - | 7161 | ` * identity: a subclass of DateTime still compares as an instant (and its extra` |
|      - | 7162 | ` * properties are invisible to the comparison, since php's date handler never` |
|      - | 7163 | ` * looks at properties), while anything that is not a date at all is php's` |
|      - | 7164 | ` * ZEND_UNCOMPARABLE -- the 1-from-both-sides the caller defaults to.` |
|      - | 7165 | ` * ---------------------------------------------------------------------------` |
|      - | 7166 | ` */` |
|      - | 7167 | `/*` |
|      - | 7168 | ` * DateTime / DateTimeImmutable: php's date_object_compare_date, which is` |
|      - | 7169 | ` * timelib_time_compare on the two INSTANTS -- the epoch second first, the` |
|      - | 7170 | ` * microseconds to break a tie. The zone is not part of it (php compares the` |
|      - | 7171 | ` * instant the two name, so 00:00 UTC equals 01:00+01:00), and neither is any` |
|      - | 7172 | ` * property, declared or dynamic.` |
|      - | 7173 | ` */` |
|     48 | 7174 | `static void DtCmpDateTime(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)` |
|      2 | 7175 | `{` |
|      - | 7176 | `	dt_state sL,sR;` |
|     48 | 7177 | `	if( !DtIsA(&(*pVm),pThis,"DateTimeInterface")` |
|     50 | 7178 | `	 \|\| !DtIsA(&(*pVm),pCtx->pOther,"DateTimeInterface") ){` |
|     20 | 7179 | `		return;   /* uncomparable, which is what the caller pre-loaded */` |
|      - | 7180 | `	}` |
|     40 | 7181 | `	if( !DtIsInit(pThis) \|\| !DtIsInit(pCtx->pOther) ){` |
|      - | 7182 | `		/* An unconstructed date has no instant to compare, and php refuses from` |
|      - | 7183 | ``		 * EITHER side -- so `$fresh == $uninitialized` raises as well. The screen`` |
|      - | 7184 | `		 * sits below the both-are-dates one on purpose: a date against something` |
|      - | 7185 | `		 * that is not one stays php's silent uncomparable. */` |
|     17 | 7186 | `		pCtx->zThrowClass = "DateObjectError";` |
|     17 | 7187 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 7188 | `			"Trying to compare an incomplete DateTime or DateTimeImmutable object");` |
|     17 | 7189 | `		return;` |
|      - | 7190 | `	}` |
|     24 | 7191 | `	DtLoad(pThis,&sL);` |
|     24 | 7192 | `	DtLoad(pCtx->pOther,&sR);` |
|     24 | 7193 | `	if( sL.iTs != sR.iTs ){` |
|      5 | 7194 | `		pCtx->iResult = sL.iTs < sR.iTs ? -1 : 1;` |
|     22 | 7195 | `	}else if( sL.uSec != sR.uSec ){` |
|      5 | 7196 | `		pCtx->iResult = sL.uSec < sR.uSec ? -1 : 1;` |
|      3 | 7197 | `	}else{` |
|     16 | 7198 | `		pCtx->iResult = 0;` |
|      - | 7199 | `	}` |
|     26 | 7200 | `}` |
|      - | 7201 | `/*` |
|      - | 7202 | ` * DateInterval: php refuses. Two intervals carry no common unit -- a month is` |
|      - | 7203 | ` * not a fixed number of days -- so php's handler answers ZEND_UNCOMPARABLE` |
|      - | 7204 | `` * behind an E_WARNING for every pair, `P1D` against `P1D` included. The one`` |
|      - | 7205 | `` * comparison that succeeds is `$i == $i`, and that never reaches a handler:`` |
|      - | 7206 | ` * zend's identity shortcut answers it first (and PH7_ClassInstanceCmp's does` |
|      - | 7207 | ` * too, above this call).` |
|      - | 7208 | ` *` |
|      - | 7209 | ` * The warning is emitted from inside the comparator on purpose -- php emits it` |
|      - | 7210 | ` * from inside the handler, so a sort() over intervals warns once per COMPARISON` |
|      - | 7211 | ` * there as it does here.` |
|      - | 7212 | ` */` |
|     18 | 7213 | `static void DtCmpInterval(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)` |
|      1 | 7214 | `{` |
|     18 | 7215 | `	if( !DtIsA(&(*pVm),pThis,"DateInterval")` |
|     19 | 7216 | `	 \|\| !DtIsA(&(*pVm),pCtx->pOther,"DateInterval") ){` |
|      3 | 7217 | `		return;   /* an interval against something else: uncomparable, and silent */` |
|      - | 7218 | `	}` |
|     17 | 7219 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Cannot compare DateInterval objects");` |
|     17 | 7220 | `	pCtx->iResult = 1;` |
|     10 | 7221 | `}` |
|      - | 7222 | `/*` |
|      - | 7223 | ` * DateTimeZone: php compares two zones of the SAME kind and refuses two of` |
|      - | 7224 | ` * different kinds outright (a DateException raised from inside the comparison).` |
|      - | 7225 | ` * Same-kind zones answer 0 or php's uncomparable 1 and never an ordering, so` |
|      - | 7226 | `` * `+01:00 < +02:00` is false there: an OFFSET zone is compared by its offset`` |
|      - | 7227 | `` * (`+0100` and `+01:00` are one zone), an abbreviation and an identifier by`` |
|      - | 7228 | ` * their normalized names.` |
|      - | 7229 | ` */` |
|     68 | 7230 | `static void DtCmpTimeZone(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)` |
|      1 | 7231 | `{` |
|     69 | 7232 | `	const char *zL = 0,*zR = 0;` |
|     69 | 7233 | `	int nL = 0,nR = 0;` |
|      - | 7234 | `	int iKindL,iKindR;` |
|     68 | 7235 | `	if( !DtIsA(&(*pVm),pThis,"DateTimeZone")` |
|     69 | 7236 | `	 \|\| !DtIsA(&(*pVm),pCtx->pOther,"DateTimeZone") ){` |
|     30 | 7237 | `		return;` |
|      - | 7238 | `	}` |
|     65 | 7239 | `	if( !DtIsInit(pThis) \|\| !DtIsInit(pCtx->pOther) ){` |
|      - | 7240 | `		/* php's own sentence for the zone half, and its own class: a DateException` |
|      - | 7241 | `		 * carries the KIND mismatch below, an unconstructed operand a` |
|      - | 7242 | `		 * DateObjectError. */` |
|      7 | 7243 | `		pCtx->zThrowClass = "DateObjectError";` |
|      7 | 7244 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 7245 | `			"Trying to compare uninitialized DateTimeZone objects");` |
|      7 | 7246 | `		return;` |
|      - | 7247 | `	}` |
|     59 | 7248 | `	PH7_NativeAttrStr(pThis,DTZ_NAME,&zL,&nL);` |
|     59 | 7249 | `	PH7_NativeAttrStr(pCtx->pOther,DTZ_NAME,&zR,&nR);` |
|     59 | 7250 | `	iKindL = DtZoneKindOf(pThis,DTZ_KIND,zL,nL);` |
|     59 | 7251 | `	iKindR = DtZoneKindOf(pCtx->pOther,DTZ_KIND,zR,nR);` |
|     59 | 7252 | `	if( iKindL != iKindR ){` |
|     31 | 7253 | `		pCtx->zThrowClass = "DateException";` |
|     31 | 7254 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 7255 | `			"Cannot compare two different kinds of DateTimeZone objects");` |
|     31 | 7256 | `		return;   /* the uncomparable 1 stands in until the refusal is raised */` |
|      - | 7257 | `	}` |
|     29 | 7258 | `	if( iKindL == DT_ZONE_OFFSET ){` |
|     22 | 7259 | `		pCtx->iResult = PH7_NativeAttrInt(pThis,DTZ_OFF)` |
|     14 | 7260 | `		              == PH7_NativeAttrInt(pCtx->pOther,DTZ_OFF) ? 0 : 1;` |
|     15 | 7261 | `		return;` |
|      - | 7262 | `	}` |
|     21 | 7263 | `	pCtx->iResult = (nL == nR && (nL == 0 \|\| SyMemcmp(zL,zR,(sxu32)nL) == 0)) ? 0 : 1;` |
|     35 | 7264 | `}` |
|      - | 7265 | `/* Build a payload array: the class's presented shape, then its own visible slots. */` |
|    116 | 7266 | `static int DtSerializePayload(ph7_context *pCtx,ph7_class_instance *pThis,int bZoneOnly,` |
|      - | 7267 | `	ph7_value *pOut)` |
|      1 | 7268 | `{` |
|    117 | 7269 | `	PH7_MemObjInit(pCtx->pVm,pOut);` |
|    117 | 7270 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|    ! 0 | 7271 | `		PH7_MemObjRelease(pOut);` |
|    ! 0 | 7272 | `		return -1;` |
|      - | 7273 | `	}` |
|      - | 7274 | `	/* The SHAPE first here, the object's own properties behind it: php's` |
|      - | 7275 | `	 * __serialize builds a fresh array from the struct and calls` |
|      - | 7276 | `	 * add_common_properties on the END of it, which is the opposite order from the` |
|      - | 7277 | `	 * presentation above (where the object's table is what the struct writes` |
|      - | 7278 | `	 * into). serialize() and var_dump therefore disagree about where a subclass's` |
|      - | 7279 | `	 * property sits, in both engines. */` |
|    117 | 7280 | `	if( bZoneOnly ){` |
|     65 | 7281 | `		DtTimeZoneShape(pCtx->pVm,pThis,pOut);` |
|     33 | 7282 | `	}else{` |
|     53 | 7283 | `		DtDateTimeShape(pCtx->pVm,pThis,pOut);` |
|      - | 7284 | `	}` |
|    117 | 7285 | `	DtAddCommonProps(pCtx->pVm,pThis,pOut);` |
|    117 | 7286 | `	return 0;` |
|     59 | 7287 | `}` |
|      - | 7288 | ``/* php's `Error: Invalid serialization data for <Class> object`, the one refusal all`` |
|      - | 7289 | ` * four methods share. Named for the DECLARING class, not the receiver's. */` |
|     42 | 7290 | `static int DtSerialError(ph7_context *pCtx,const char *zClass)` |
|      1 | 7291 | `{` |
|     64 | 7292 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     21 | 7293 | `		"Invalid serialization data for %s object",zClass);` |
|      1 | 7294 | `}` |
|      - | 7295 | ``/* The `array $data` parameter's own screen: the shared ZPP does not judge a scalar`` |
|      - | 7296 | `` * against a bare `array` (rule 18's §2 gap), so each caller words php's TypeError. */`` |
|    142 | 7297 | `static int DtCheckDataArg(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zClass)` |
|      1 | 7298 | `{` |
|      - | 7299 | `	char zBuf[64];` |
|    143 | 7300 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    143 | 7301 | `		return 0;` |
|      - | 7302 | `	}` |
|    ! 0 | 7303 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 7304 | `		"%s::__unserialize(): Argument #1 ($data) must be of type array, %s given",` |
|    ! 0 | 7305 | `		zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|    ! 0 | 7306 | `	return -1;` |
|     72 | 7307 | `}` |
|      - | 7308 | `/*` |
|      - | 7309 | `` * php's php_date_timezone_initialize_from_hash(): `timezone_type` must be an int in`` |
|      - | 7310 | `` * 1..3 and `timezone` a string, and then the NAME alone rebuilds the zone -- the tag`` |
|      - | 7311 | ` * is validated but never trusted, which is why a payload tagged 1 whose name is` |
|      - | 7312 | ` * "UTC" restores a UTC zone rather than an offset one. Answers 0 on success.` |
|      - | 7313 | ` */` |
|     46 | 7314 | `static int DtZoneRestore(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 7315 | `{` |
|      - | 7316 | `	ph7_value *pType,*pName;` |
|      - | 7317 | `	const char *zTz,*zName;` |
|     47 | 7318 | `	int nTz,nName,iKind = DT_ZONE_ID;` |
|     47 | 7319 | `	sxi32 iOff = 0;` |
|      - | 7320 | `	sxi64 iType;` |
|      - | 7321 | `	char zBuf[16];` |
|     47 | 7322 | `	pType = ph7_array_fetch(pData,"timezone_type",(int)sizeof("timezone_type")-1);` |
|     47 | 7323 | `	if( pType == 0 \|\| (pType->iFlags & MEMOBJ_INT) == 0 ){` |
|      5 | 7324 | `		return -1;` |
|      - | 7325 | `	}` |
|     43 | 7326 | `	iType = pType->x.iVal;` |
|     43 | 7327 | `	if( iType < 1 \|\| iType > 3 ){` |
|      3 | 7328 | `		return -1;` |
|      - | 7329 | `	}` |
|     41 | 7330 | `	pName = ph7_array_fetch(pData,"timezone",(int)sizeof("timezone")-1);` |
|     41 | 7331 | `	if( pName == 0 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 7332 | `		return -1;` |
|      - | 7333 | `	}` |
|     41 | 7334 | `	zTz = (const char *)SyBlobData(&pName->sBlob);` |
|     41 | 7335 | `	nTz = (int)SyBlobLength(&pName->sBlob);` |
|     41 | 7336 | `	if( DtZoneParse(zTz,nTz,&iOff,&zName,&nName,&iKind,zBuf,sizeof(zBuf)) != 0 ){` |
|    ! 0 | 7337 | `		return -1;` |
|      - | 7338 | `	}` |
|     41 | 7339 | `	PH7_NativeSetAttrInt(&(*pVm),pThis,DTZ_OFF,iOff);` |
|     41 | 7340 | `	PH7_NativeSetAttrStr(&(*pVm),pThis,DTZ_NAME,zName,nName);` |
|      - | 7341 | ``	/* The payload's own `timezone_type` is NOT read back: php re-derives the kind`` |
|      - | 7342 | `	 * from the name here too, which is what turns a serialized type-2 "UTC" into a` |
|      - | 7343 | `	 * type 3 on the way in. */` |
|     41 | 7344 | `	PH7_NativeSetAttrInt(&(*pVm),pThis,DTZ_KIND,iKind);` |
|     41 | 7345 | `	return 0;` |
|     24 | 7346 | `}` |
|      - | 7347 | `/*` |
|      - | 7348 | `` * php's php_date_initialize_from_hash(): `date`, `timezone_type` and `timezone` must`` |
|      - | 7349 | ` * all be present and well-typed, and the tag must be one php writes.` |
|      - | 7350 | ` *` |
|      - | 7351 | ` * php restores an OFFSET or ABBREVIATION payload by CONCATENATING the two and running` |
|      - | 7352 | ` * its ordinary parser over "<date> <timezone>", and an IDENTIFIER one by resolving the` |
|      - | 7353 | ` * name first. Resolving the name for all three is the same answer here and does not` |
|      - | 7354 | `` * lean on the parser: `date` is always php's own `x-m-d H:i:s.u`, which carries no`` |
|      - | 7355 | ` * zone of its own, so nothing is left for the concatenated text to decide. It is also` |
|      - | 7356 | ` * the only spelling that works today -- PHL's parser accepts an offset only when it is` |
|      - | 7357 | ` * ATTACHED to the time ("…07+02:30", never "…07 +02:30") and accepts no trailing zone` |
|      - | 7358 | ` * NAME at all, so php's own round-trip string does not parse here (a §10 gap of its` |
|      - | 7359 | ` * own, recorded rather than worked around).` |
|      - | 7360 | ` *` |
|      - | 7361 | ` * Reading the NAME rather than the tag is also what php ends up doing: a payload` |
|      - | 7362 | ` * tagged 1 whose timezone is "UTC" restores a UTC zone in both engines.` |
|      - | 7363 | ` * Answers 0 on success.` |
|      - | 7364 | ` */` |
|     60 | 7365 | `static int DtDateRestore(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 7366 | `{` |
|     61 | 7367 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 7368 | `	ph7_value *pDate,*pType,*pName;` |
|      - | 7369 | `	const char *zDate,*zTz,*zZone,*zErr;` |
|     61 | 7370 | `	int nDate,nTz,nZone,iPos,iKind = DT_ZONE_ID;` |
|     61 | 7371 | `	sxi32 iOff = 0;` |
|      - | 7372 | `	sxi64 iType;` |
|      - | 7373 | `	dt_state sState;` |
|      - | 7374 | `	char zNameBuf[16],zZoneBuf[16],cAt;` |
|     61 | 7375 | `	pDate = ph7_array_fetch(pData,"date",(int)sizeof("date")-1);` |
|     61 | 7376 | `	if( pDate == 0 \|\| (pDate->iFlags & MEMOBJ_STRING) == 0 ){` |
|      9 | 7377 | `		return -1;` |
|      - | 7378 | `	}` |
|     53 | 7379 | `	pType = ph7_array_fetch(pData,"timezone_type",(int)sizeof("timezone_type")-1);` |
|     53 | 7380 | `	if( pType == 0 \|\| (pType->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 7381 | `		return -1;` |
|      - | 7382 | `	}` |
|     53 | 7383 | `	pName = ph7_array_fetch(pData,"timezone",(int)sizeof("timezone")-1);` |
|     53 | 7384 | `	if( pName == 0 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 7385 | `		return -1;` |
|      - | 7386 | `	}` |
|     53 | 7387 | `	zDate = (const char *)SyBlobData(&pDate->sBlob);` |
|     53 | 7388 | `	nDate = (int)SyBlobLength(&pDate->sBlob);` |
|     53 | 7389 | `	zTz   = (const char *)SyBlobData(&pName->sBlob);` |
|     53 | 7390 | `	nTz   = (int)SyBlobLength(&pName->sBlob);` |
|     53 | 7391 | `	iType = pType->x.iVal;` |
|     53 | 7392 | `	if( iType < 1 \|\| iType > 3 ){` |
|    ! 0 | 7393 | `		return -1;` |
|      - | 7394 | `	}` |
|     53 | 7395 | `	if( DtZoneParse(zTz,nTz,&iOff,&zZone,&nZone,&iKind,zZoneBuf,sizeof(zZoneBuf)) != 0 ){` |
|      3 | 7396 | `		return -1;` |
|      - | 7397 | `	}` |
|     51 | 7398 | `	if( (iOff < 0 ? -iOff : iOff) / 3600 > 24 ){` |
|      - | 7399 | `		/* php reads the payload's zone back through its DATE-STRING grammar, whose` |
|      - | 7400 | `		 * offsets stop at hour 24 -- narrower than the zone constructor's 99. So a` |
|      - | 7401 | ``		 * DateTime carrying `+25:00` serializes there and refuses to come back,`` |
|      - | 7402 | `		 * while the DateTimeZone alone round-trips. Only the HOUR field is bounded:` |
|      - | 7403 | ``		 * `+24:59` and `+24:00:01` are both fine. */`` |
|      7 | 7404 | `		return -1;` |
|      - | 7405 | `	}` |
|     44 | 7406 | `	if( DtInitState(pCtx,zDate,nDate,iOff,zZone,nZone,iKind,&sState,zNameBuf,` |
|     23 | 7407 | `		sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0 ){` |
|    ! 0 | 7408 | `		return -1;` |
|      - | 7409 | `	}` |
|     45 | 7410 | `	DtStore(pVm,pThis,&sState);` |
|     45 | 7411 | `	return 0;` |
|     31 | 7412 | `}` |
|      - | 7413 | `/*` |
|      - | 7414 | ` * php's restore_custom_datetime_properties(): every payload key that is not part of` |
|      - | 7415 | ` * the internal shape becomes a property of the object. A REFERENCE is skipped, which` |
|      - | 7416 | ` * PHL cannot receive here (the pairs arrive already dereferenced).` |
|      - | 7417 | ` */` |
|      - | 7418 | `typedef struct dt_restore_ctx dt_restore_ctx;` |
|      - | 7419 | `struct dt_restore_ctx` |
|      - | 7420 | `{` |
|      - | 7421 | `	ph7_class_instance *pThis;` |
|      - | 7422 | `	int bZoneOnly;` |
|      - | 7423 | `};` |
|    168 | 7424 | `static int DtRestoreWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 | 7425 | `{` |
|    169 | 7426 | `	dt_restore_ctx *pRes = (dt_restore_ctx *)pUserData;` |
|      - | 7427 | `	const char *zKey;` |
|      - | 7428 | `	int nKey;` |
|    169 | 7429 | `	if( !ph7_value_is_string(pKey) ){` |
|    ! 0 | 7430 | `		return PH7_OK;` |
|      - | 7431 | `	}` |
|    169 | 7432 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|    168 | 7433 | `	if( (nKey == 13 && SyMemcmp(zKey,"timezone_type",13) == 0)` |
|    126 | 7434 | `	 \|\| (nKey == 8  && SyMemcmp(zKey,"timezone",8) == 0)` |
|     69 | 7435 | `	 \|\| (!pRes->bZoneOnly && nKey == 4 && SyMemcmp(zKey,"date",4) == 0) ){` |
|    213 | 7436 | `		return PH7_OK;` |
|      - | 7437 | `	}` |
|      - | 7438 | `	/* A name the class does not DECLARE is dropped, which is what the engine's own` |
|      - | 7439 | `	 * unserialize does with one: PHL has no dynamic properties, where php creates` |
|      - | 7440 | `	 * (and deprecates) them. */` |
|     47 | 7441 | `	PH7_NativeSetProp(pRes->pThis->pVm,pRes->pThis,zKey,(sxu32)nKey,pVal);` |
|     47 | 7442 | `	return PH7_OK;` |
|    108 | 7443 | `}` |
|     84 | 7444 | `static void DtRestoreCustomProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData,` |
|      - | 7445 | `	int bZoneOnly)` |
|      1 | 7446 | `{` |
|      - | 7447 | `	dt_restore_ctx sRes;` |
|     42 | 7448 | `	SXUNUSED(pVm);` |
|     85 | 7449 | `	if( (pData->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 7450 | `		return;` |
|      - | 7451 | `	}` |
|     85 | 7452 | `	sRes.pThis = pThis;` |
|     85 | 7453 | `	sRes.bZoneOnly = bZoneOnly;` |
|     85 | 7454 | `	ph7_array_walk(pData,DtRestoreWalk,&sRes);` |
|     43 | 7455 | `}` |
|      - | 7456 | `/* DateTimeZone::__serialize() / DateTime\|DateTimeImmutable::__serialize() */` |
|    120 | 7457 | `static int DtSerializeMagic(ph7_context *pCtx,int bZoneOnly)` |
|      1 | 7458 | `{` |
|    121 | 7459 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 7460 | `	ph7_value sOut;` |
|    121 | 7461 | `	if( pThis == 0 ){` |
|      5 | 7462 | `		return PH7_OK;` |
|      - | 7463 | `	}` |
|    117 | 7464 | `	if( DtSerializePayload(pCtx,pThis,bZoneOnly,&sOut) != 0 ){` |
|    ! 0 | 7465 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7466 | `	}` |
|    117 | 7467 | `	ph7_result_value(pCtx,&sOut);` |
|    117 | 7468 | `	PH7_MemObjRelease(&sOut);` |
|    117 | 7469 | `	return PH7_OK;` |
|     61 | 7470 | `}` |
|     66 | 7471 | `static int vm_builtin_DateTimeZone_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7472 | `{` |
|     33 | 7473 | `	SXUNUSED(nArg);` |
|     33 | 7474 | `	SXUNUSED(apArg);` |
|     67 | 7475 | `	return DtSerializeMagic(pCtx,1);` |
|      1 | 7476 | `}` |
|     54 | 7477 | `static int vm_builtin_DateTime_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7478 | `{` |
|     27 | 7479 | `	SXUNUSED(nArg);` |
|     27 | 7480 | `	SXUNUSED(apArg);` |
|     55 | 7481 | `	return DtSerializeMagic(pCtx,0);` |
|      1 | 7482 | `}` |
|      - | 7483 | `/* __unserialize(array $data): restore the state, then the subclass's own slots. */` |
|     88 | 7484 | `static int DtUnserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg,int bZoneOnly,` |
|      - | 7485 | `	const char *zClass)` |
|      1 | 7486 | `{` |
|      - | 7487 | `	/* Raw: this is a door that INITIALIZES -- unserialize() calls it on an object` |
|      - | 7488 | `	 * the engine built without a constructor, which is the whole point of it. */` |
|     89 | 7489 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|      - | 7490 | `	int rc;` |
|     89 | 7491 | `	if( pThis == 0 ){` |
|    ! 0 | 7492 | `		return PH7_OK;` |
|      - | 7493 | `	}` |
|     89 | 7494 | `	if( DtCheckDataArg(pCtx,nArg,apArg,zClass) != 0 ){` |
|    ! 0 | 7495 | `		return PH7_EXCEPTION;` |
|      - | 7496 | `	}` |
|     66 | 7497 | `	rc = bZoneOnly ? DtZoneRestore(pCtx->pVm,pThis,apArg[0])` |
|     67 | 7498 | `	               : DtDateRestore(pCtx,pThis,apArg[0]);` |
|     89 | 7499 | `	if( rc != 0 ){` |
|     15 | 7500 | `		return DtSerialError(pCtx,zClass);` |
|      - | 7501 | `	}` |
|     75 | 7502 | `	DtRestoreCustomProps(pCtx->pVm,pThis,apArg[0],bZoneOnly);` |
|     75 | 7503 | `	DtSetInit(pCtx->pVm,pThis);` |
|     75 | 7504 | `	return PH7_OK;` |
|     45 | 7505 | `}` |
|     42 | 7506 | `static int vm_builtin_DateTimeZone_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7507 | `{` |
|     43 | 7508 | `	return DtUnserializeMagic(pCtx,nArg,apArg,1,"DateTimeZone");` |
|      1 | 7509 | `}` |
|     46 | 7510 | `static int vm_builtin_DateTime_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7511 | `{` |
|     47 | 7512 | `	return DtUnserializeMagic(pCtx,nArg,apArg,0,"DateTime");` |
|      1 | 7513 | `}` |
|    ! 0 | 7514 | `static int vm_builtin_DateTimeImmutable_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7515 | `{` |
|    ! 0 | 7516 | `	return DtUnserializeMagic(pCtx,nArg,apArg,0,"DateTimeImmutable");` |
|    ! 0 | 7517 | `}` |
|      - | 7518 | `/*` |
|      - | 7519 | ` * __wakeup(): the LEGACY payload, whose pairs the engine wrote into the object's own` |
|      - | 7520 | ` * properties before calling this. php reads Z_OBJPROP and restores from it, so an` |
|      - | 7521 | `` * object that has no such properties -- a plain `new DateTime` -- is exactly the`` |
|      - | 7522 | ` * failure case, and php raises the same Error there.` |
|      - | 7523 | ` */` |
|      6 | 7524 | `static int DtWakeupMagic(ph7_context *pCtx,int bZoneOnly,const char *zClass)` |
|      1 | 7525 | `{` |
|      - | 7526 | `	/* Raw, for the reason __unserialize() is: php reads the object's own properties` |
|      - | 7527 | ``	 * here and raises `Invalid serialization data` when they do not describe a`` |
|      - | 7528 | `	 * date -- which is what an unconstructed object's empty set does. */` |
|      7 | 7529 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|      - | 7530 | `	ph7_value sProps;` |
|      - | 7531 | `	int rc;` |
|      7 | 7532 | `	if( pThis == 0 ){` |
|    ! 0 | 7533 | `		return PH7_OK;` |
|      - | 7534 | `	}` |
|      7 | 7535 | `	PH7_MemObjInit(pCtx->pVm,&sProps);` |
|      7 | 7536 | `	if( PH7_MemObjToHashmap(&sProps) != SXRET_OK ){` |
|    ! 0 | 7537 | `		PH7_MemObjRelease(&sProps);` |
|    ! 0 | 7538 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7539 | `	}` |
|      7 | 7540 | `	DtAddCommonProps(pCtx->pVm,pThis,&sProps);` |
|      5 | 7541 | `	rc = bZoneOnly ? DtZoneRestore(pCtx->pVm,pThis,&sProps)` |
|      5 | 7542 | `	               : DtDateRestore(pCtx,pThis,&sProps);` |
|      7 | 7543 | `	PH7_MemObjRelease(&sProps);` |
|      7 | 7544 | `	if( rc != 0 ){` |
|      7 | 7545 | `		return DtSerialError(pCtx,zClass);` |
|      - | 7546 | `	}` |
|    ! 0 | 7547 | `	DtSetInit(pCtx->pVm,pThis);` |
|    ! 0 | 7548 | `	return PH7_OK;` |
|      4 | 7549 | `}` |
|      2 | 7550 | `static int vm_builtin_DateTimeZone_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7551 | `{` |
|      1 | 7552 | `	SXUNUSED(nArg);` |
|      1 | 7553 | `	SXUNUSED(apArg);` |
|      3 | 7554 | `	return DtWakeupMagic(pCtx,1,"DateTimeZone");` |
|      1 | 7555 | `}` |
|      4 | 7556 | `static int vm_builtin_DateTime_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7557 | `{` |
|      2 | 7558 | `	SXUNUSED(nArg);` |
|      2 | 7559 | `	SXUNUSED(apArg);` |
|      5 | 7560 | `	return DtWakeupMagic(pCtx,0,"DateTime");` |
|      1 | 7561 | `}` |
|    ! 0 | 7562 | `static int vm_builtin_DateTimeImmutable_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7563 | `{` |
|    ! 0 | 7564 | `	SXUNUSED(nArg);` |
|    ! 0 | 7565 | `	SXUNUSED(apArg);` |
|    ! 0 | 7566 | `	return DtWakeupMagic(pCtx,0,"DateTimeImmutable");` |
|    ! 0 | 7567 | `}` |
|      - | 7568 | `/*` |
|      - | 7569 | ``  * __set_state(array $array): what var_export's `\DateTime::__set_state(array(…))` `` |
|      - | 7570 | ` * text evaluates to. php instantiates the class the method is DECLARED on and not` |
|      - | 7571 | `` * the called one -- `MyDateTime::__set_state(…)` answers a plain DateTime there --`` |
|      - | 7572 | ` * so this deliberately does not go through DtFactoryClass().` |
|      - | 7573 | ` */` |
|     12 | 7574 | `static int DtSetStateMagic(ph7_context *pCtx,int nArg,ph7_value **apArg,int bZoneOnly,` |
|      - | 7575 | `	const char *zClass)` |
|      1 | 7576 | `{` |
|     13 | 7577 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 | 7578 | `	ph7_class *pClass = DtClass(pVm,zClass);` |
|      - | 7579 | `	ph7_class_instance *pObj;` |
|      - | 7580 | `	int rc;` |
|     13 | 7581 | `	if( pClass == 0 ){` |
|    ! 0 | 7582 | `		return PH7_OK;` |
|      - | 7583 | `	}` |
|     13 | 7584 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - | 7585 | `		char zBuf[64];` |
|    ! 0 | 7586 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 7587 | `			"%s::__set_state(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 | 7588 | `			zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|      - | 7589 | `	}` |
|     13 | 7590 | `	pObj = DtNewInstance(pVm,pClass);` |
|     13 | 7591 | `	if( pObj == 0 ){` |
|    ! 0 | 7592 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7593 | `	}` |
|      8 | 7594 | `	rc = bZoneOnly ? DtZoneRestore(pVm,pObj,apArg[0])` |
|     11 | 7595 | `	               : DtDateRestore(pCtx,pObj,apArg[0]);` |
|     13 | 7596 | `	if( rc != 0 ){` |
|      3 | 7597 | `		PH7_ClassInstanceUnref(pObj);` |
|      3 | 7598 | `		return DtSerialError(pCtx,zClass);` |
|      - | 7599 | `	}` |
|     11 | 7600 | `	DtRestoreCustomProps(pVm,pObj,apArg[0],bZoneOnly);` |
|     11 | 7601 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     11 | 7602 | `	return PH7_OK;` |
|      7 | 7603 | `}` |
|      2 | 7604 | `static int vm_builtin_DateTimeZone_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7605 | `{` |
|      3 | 7606 | `	return DtSetStateMagic(pCtx,nArg,apArg,1,"DateTimeZone");` |
|      1 | 7607 | `}` |
|     10 | 7608 | `static int vm_builtin_DateTime_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7609 | `{` |
|     11 | 7610 | `	return DtSetStateMagic(pCtx,nArg,apArg,0,"DateTime");` |
|      1 | 7611 | `}` |
|    ! 0 | 7612 | `static int vm_builtin_DateTimeImmutable_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7613 | `{` |
|    ! 0 | 7614 | `	return DtSetStateMagic(pCtx,nArg,apArg,0,"DateTimeImmutable");` |
|    ! 0 | 7615 | `}` |
|      - | 7616 | `/*` |
|      - | 7617 | ` * ---------------------------------------------------------------------------` |
|      - | 7618 | ` * php's serialization quartet for DateInterval and DatePeriod.` |
|      - | 7619 | ` *` |
|      - | 7620 | ` * Both classes declare __serialize/__unserialize/__wakeup/__set_state there and` |
|      - | 7621 | `` * neither declared any of them here, so `serialize()` walked the slots by luck`` |
|      - | 7622 | ` * (the bytes matched, because for these two classes the state IS the php-visible` |
|      - | 7623 | `` * property table), `var_export()`'s `\DateInterval::__set_state(array(...))` text`` |
|      - | 7624 | `` * evaluated to "Call to undefined method", and `serialize()` of an object that`` |
|      - | 7625 | ` * was never constructed answered a payload where php raises.` |
|      - | 7626 | ` *` |
|      - | 7627 | ` * The two RESTORE rules are not the same rule, and both are php's:` |
|      - | 7628 | ` *` |
|      - | 7629 | ` *   DateInterval reads each field on its own and fills a MISSING one with -1 --` |
|      - | 7630 | ``  *   timelib's "unset" marker, which is why `unserialize('O:12:"DateInterval":0:{}')` `` |
|      - | 7631 | `` *   is an interval of -1 years. `invert`, `f` and `from_string` are the three`` |
|      - | 7632 | ` *   exceptions, absent as 0, 0.0 and false. Nothing is refused.` |
|      - | 7633 | ` *` |
|      - | 7634 | ` *   DatePeriod refuses ANY payload that is not complete and well-typed: all seven` |
|      - | 7635 | ` *   keys, the three dates null or a DateTimeInterface, the interval a` |
|      - | 7636 | ` *   DateInterval, the count a real int and the two flags real bools -- one miss` |
|      - | 7637 | `` *   and it is php's `Invalid serialization data for DatePeriod object`.`` |
|      - | 7638 | ` * ---------------------------------------------------------------------------` |
|      - | 7639 | ` */` |
|      - | 7640 | `/* __serialize(): the object's own visible slots, mangled the way php mangles a` |
|      - | 7641 | ` * non-public one -- which is what a subclass's private property serializes as. */` |
|     52 | 7642 | `static int DtSerializeProps(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7643 | `{` |
|     54 | 7644 | `	ph7_class_instance *pThis = DtThis(pCtx);   /* php raises on an unconstructed one */` |
|      - | 7645 | `	ph7_value sOut;` |
|     26 | 7646 | `	SXUNUSED(nArg);` |
|     26 | 7647 | `	SXUNUSED(apArg);` |
|     54 | 7648 | `	if( pThis == 0 ){` |
|      7 | 7649 | `		return PH7_OK;` |
|      - | 7650 | `	}` |
|     48 | 7651 | `	PH7_MemObjInit(pCtx->pVm,&sOut);` |
|     48 | 7652 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|    ! 0 | 7653 | `		PH7_MemObjRelease(&sOut);` |
|    ! 0 | 7654 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7655 | `	}` |
|      - | 7656 | `	/* The class's own shape first and the object's additions behind it, which is` |
|      - | 7657 | `	 * php's order here and the opposite of the presentation's (where the struct` |
|      - | 7658 | `	 * writes into the object's table). */` |
|     71 | 7659 | `	DtAddNativeProps(pCtx->pVm,pThis,` |
|     46 | 7660 | `		DtIsA(pCtx->pVm,pThis,"DateInterval") ? "DateInterval" : "DatePeriod",&sOut);` |
|     48 | 7661 | `	DtAddCommonProps(pCtx->pVm,pThis,&sOut);` |
|     48 | 7662 | `	ph7_result_value(pCtx,&sOut);` |
|     48 | 7663 | `	PH7_MemObjRelease(&sOut);` |
|     48 | 7664 | `	return PH7_OK;` |
|     28 | 7665 | `}` |
|      - | 7666 | `/* One payload field, with the value php gives an absent one. A container is not a` |
|      - | 7667 | ` * number to php's reader either, so it counts as absent. */` |
|    286 | 7668 | `static sxi64 DtIvRestoreInt(ph7_value *pData,const char *zKey,sxi64 iAbsent)` |
|      1 | 7669 | `{` |
|    287 | 7670 | `	ph7_value *pVal = ph7_array_fetch(pData,zKey,-1);` |
|    287 | 7671 | `	if( pVal == 0 \|\| (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_STRING)) == 0 ){` |
|    149 | 7672 | `		return iAbsent;` |
|      - | 7673 | `	}` |
|    139 | 7674 | `	return PH7_ValuePeekInt64(pVal);` |
|    144 | 7675 | `}` |
|      - | 7676 | `/* php's date_interval_initialize_from_hash(), field by field. */` |
|     38 | 7677 | `static void DtIvRestore(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 7678 | `{` |
|      - | 7679 | `	static const char * const azMinusOne[] = { "y","m","d","h","i","s" };` |
|      - | 7680 | `	ph7_value *pVal;` |
|      - | 7681 | `	sxu32 n;` |
|    267 | 7682 | `	for( n = 0 ; n < SX_ARRAYSIZE(azMinusOne) ; ++n ){` |
|    343 | 7683 | `		PH7_NativeSetAttrInt(&(*pVm),pThis,azMinusOne[n],` |
|    228 | 7684 | `			DtIvRestoreInt(pData,azMinusOne[n],-1));` |
|    115 | 7685 | `	}` |
|     39 | 7686 | `	PH7_NativeSetAttrInt(&(*pVm),pThis,"invert",DtIvRestoreInt(pData,"invert",0));` |
|      - | 7687 | ``	/* `days` is php's one field with two TYPES -- a day count, or false when the`` |
|      - | 7688 | `	 * interval was not measured between two dates -- so a bool payload stays a` |
|      - | 7689 | `	 * bool where every other field is narrowed to an int. */` |
|     39 | 7690 | `	pVal = ph7_array_fetch(pData,"days",-1);` |
|     39 | 7691 | `	if( pVal && (pVal->iFlags & MEMOBJ_BOOL) ){` |
|     19 | 7692 | `		PH7_NativeSetAttrBool(&(*pVm),pThis,"days",pVal->x.iVal != 0);` |
|     10 | 7693 | `	}else{` |
|     21 | 7694 | `		PH7_NativeSetAttrInt(&(*pVm),pThis,"days",DtIvRestoreInt(pData,"days",-1));` |
|      - | 7695 | `	}` |
|     39 | 7696 | `	pVal = ph7_array_fetch(pData,"f",-1);` |
|     67 | 7697 | `	DtIvSetUsec(&(*pVm),pThis,` |
|     28 | 7698 | `		pVal && (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_STRING))` |
|     18 | 7699 | `			? DtIvUsecOfReal((double)PH7_ValuePeekReal(pVal)) : 0);` |
|      - | 7700 | `	/* php's LAZY interval, and what it keys on is the STRING rather than the` |
|      - | 7701 | ``	 * flag beside it: a payload carrying `date_string` comes back as an interval`` |
|      - | 7702 | `	 * built from that string -- the ten fields parsed out of it and hidden behind` |
|      - | 7703 | ``	 * the two names php shows -- while one carrying `from_string` alone is an`` |
|      - | 7704 | `	 * ordinary interval whose fields the reader above already filled. */` |
|     39 | 7705 | `	pVal = ph7_array_fetch(pData,"date_string",-1);` |
|     39 | 7706 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|      - | 7707 | `		sxi64 aVal[DT_IV_USLOT + 1];` |
|      3 | 7708 | `		int nIn,iPos = 0;` |
|      3 | 7709 | `		char cAt = ' ';` |
|      3 | 7710 | `		const char *zIn,*zReason = "";` |
|      3 | 7711 | `		zIn = (const char *)SyBlobData(&pVal->sBlob);` |
|      3 | 7712 | `		nIn = (int)SyBlobLength(&pVal->sBlob);` |
|      3 | 7713 | `		if( DtIvParseRelative(zIn,nIn,aVal,&iPos,&cAt,&zReason) == 0 ){` |
|      - | 7714 | ``			/* DtIvStore writes the six fields and the microseconds; `days` is`` |
|      - | 7715 | `			 * php's own answer for an interval that was not measured between two` |
|      - | 7716 | `			 * dates. */` |
|      3 | 7717 | `			DtIvStore(&(*pVm),pThis,aVal);` |
|      3 | 7718 | `			PH7_NativeSetAttrBool(&(*pVm),pThis,"days",0);` |
|      1 | 7719 | `		}` |
|      3 | 7720 | `		DtIvFromString(&(*pVm),pThis,zIn,nIn);` |
|      3 | 7721 | `		return;` |
|      - | 7722 | `	}` |
|      - | 7723 | `	/* ...and a payload without one is an ordinary interval whatever its` |
|      - | 7724 | ``	 * `from_string` says: php answers false there even for the `b:1` a hand-made`` |
|      - | 7725 | `	 * payload carries. */` |
|     37 | 7726 | `	PH7_NativeSetAttrBool(&(*pVm),pThis,"from_string",0);` |
|     20 | 7727 | `}` |
|      - | 7728 | `/*` |
|      - | 7729 | ` * php's date_period_initialize_from_hash(): the whole payload or nothing.` |
|      - | 7730 | ` * Answers 0 when it restored, -1 when the caller must raise.` |
|      - | 7731 | ` */` |
|    128 | 7732 | `static int DpRestoreOne(ph7_vm *pVm,ph7_value *pData,const char *zKey,const char *zClass,` |
|      - | 7733 | `	int iFlags,ph7_value **ppOut)` |
|      1 | 7734 | `{` |
|    129 | 7735 | `	ph7_value *pVal = ph7_array_fetch(pData,zKey,-1);` |
|    129 | 7736 | `	if( pVal == 0 ){` |
|     13 | 7737 | `		return -1;` |
|      - | 7738 | `	}` |
|    117 | 7739 | `	if( zClass ){` |
|     80 | 7740 | `		if( (pVal->iFlags & MEMOBJ_NULL) == 0` |
|     60 | 7741 | `		 && !DtValueIsA(&(*pVm),pVal,zClass) ){` |
|      5 | 7742 | `			return -1;` |
|      1 | 7743 | `		}` |
|     75 | 7744 | `	}else if( (pVal->iFlags & iFlags) == 0 ){` |
|      5 | 7745 | `		return -1;` |
|      - | 7746 | `	}` |
|    109 | 7747 | `	*ppOut = pVal;` |
|    109 | 7748 | `	return 0;` |
|     65 | 7749 | `}` |
|     28 | 7750 | `static int DpRestore(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 7751 | `{` |
|      - | 7752 | `	ph7_value *pStart,*pCur,*pEnd,*pIv,*pRec,*pIncS,*pIncE;` |
|     28 | 7753 | `	if( (pData->iFlags & MEMOBJ_HASHMAP) == 0` |
|     28 | 7754 | `	 \|\| DpRestoreOne(&(*pVm),pData,"start","DateTimeInterface",0,&pStart) != 0` |
|     24 | 7755 | `	 \|\| DpRestoreOne(&(*pVm),pData,"current","DateTimeInterface",0,&pCur) != 0` |
|     20 | 7756 | `	 \|\| DpRestoreOne(&(*pVm),pData,"end","DateTimeInterface",0,&pEnd) != 0` |
|     20 | 7757 | `	 \|\| DpRestoreOne(&(*pVm),pData,"interval","DateInterval",0,&pIv) != 0` |
|     18 | 7758 | `	 \|\| DpRestoreOne(&(*pVm),pData,"recurrences",0,MEMOBJ_INT,&pRec) != 0` |
|     14 | 7759 | `	 \|\| DpRestoreOne(&(*pVm),pData,"include_start_date",0,MEMOBJ_BOOL,&pIncS) != 0` |
|     13 | 7760 | `	 \|\| DpRestoreOne(&(*pVm),pData,"include_end_date",0,MEMOBJ_BOOL,&pIncE) != 0 ){` |
|     21 | 7761 | `		return -1;` |
|      - | 7762 | `	}` |
|      9 | 7763 | `	PH7_NativeSetProp(&(*pVm),pThis,"start",sizeof("start")-1,pStart);` |
|      9 | 7764 | `	PH7_NativeSetProp(&(*pVm),pThis,"current",sizeof("current")-1,pCur);` |
|      9 | 7765 | `	PH7_NativeSetProp(&(*pVm),pThis,"end",sizeof("end")-1,pEnd);` |
|      9 | 7766 | `	PH7_NativeSetProp(&(*pVm),pThis,"interval",sizeof("interval")-1,pIv);` |
|      9 | 7767 | `	PH7_NativeSetProp(&(*pVm),pThis,"recurrences",sizeof("recurrences")-1,pRec);` |
|      9 | 7768 | `	PH7_NativeSetProp(&(*pVm),pThis,"include_start_date",sizeof("include_start_date")-1,pIncS);` |
|      9 | 7769 | `	PH7_NativeSetProp(&(*pVm),pThis,"include_end_date",sizeof("include_end_date")-1,pIncE);` |
|      9 | 7770 | `	return 0;` |
|     15 | 7771 | `}` |
|      - | 7772 | `/* Every payload key that is not part of the class's own shape becomes a property,` |
|      - | 7773 | ` * the way php's restore_custom_* does (a name the class does not DECLARE is` |
|      - | 7774 | ` * dropped here, which is what PHL's own unserialize does with one). */` |
|      - | 7775 | `typedef struct dt_prop_restore dt_prop_restore;` |
|      - | 7776 | `struct dt_prop_restore` |
|      - | 7777 | `{` |
|      - | 7778 | `	ph7_class_instance *pThis;` |
|      - | 7779 | `	const char * const *azOwn;   /* the keys the class's own restore already read */` |
|      - | 7780 | `	sxu32 nOwn;` |
|      - | 7781 | `};` |
|    256 | 7782 | `static int DtPropRestoreWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 | 7783 | `{` |
|    257 | 7784 | `	dt_prop_restore *pRes = (dt_prop_restore *)pUserData;` |
|      - | 7785 | `	const char *zKey;` |
|      - | 7786 | `	int nKey;` |
|      - | 7787 | `	sxu32 n;` |
|    257 | 7788 | `	if( !ph7_value_is_string(pKey) ){` |
|    ! 0 | 7789 | `		return PH7_OK;` |
|      - | 7790 | `	}` |
|    257 | 7791 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|   1339 | 7792 | `	for( n = 0 ; n < pRes->nOwn ; ++n ){` |
|   1338 | 7793 | `		if( (int)SyStrlen(pRes->azOwn[n]) == nKey` |
|    996 | 7794 | `		 && SyMemcmp(pRes->azOwn[n],zKey,(sxu32)nKey) == 0 ){` |
|    257 | 7795 | `			return PH7_OK;` |
|      - | 7796 | `		}` |
|    542 | 7797 | `	}` |
|    ! 0 | 7798 | `	PH7_NativeSetProp(pRes->pThis->pVm,pRes->pThis,zKey,(sxu32)nKey,pVal);` |
|    ! 0 | 7799 | `	return PH7_OK;` |
|    129 | 7800 | `}` |
|      - | 7801 | `/* The shared body of __unserialize/__wakeup/__set_state for the two classes:` |
|      - | 7802 | ` * bPeriod picks the rule, pThis is the object being filled. */` |
|     66 | 7803 | `static int DtPropsRestoreInto(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pData,` |
|      - | 7804 | `	int bPeriod,const char *zClass)` |
|      1 | 7805 | `{` |
|      - | 7806 | `	static const char * const azIvOwn[] = {` |
|      - | 7807 | `		"y","m","d","h","i","s","f","invert","days","from_string","date_string"` |
|      - | 7808 | `	};` |
|      - | 7809 | `	static const char * const azDpOwn[] = {` |
|      - | 7810 | `		"start","current","end","interval","recurrences",` |
|      - | 7811 | `		"include_start_date","include_end_date"` |
|      - | 7812 | `	};` |
|      - | 7813 | `	dt_prop_restore sRes;` |
|     67 | 7814 | `	if( bPeriod ){` |
|     29 | 7815 | `		if( DpRestore(pCtx->pVm,pThis,pData) != 0 ){` |
|     21 | 7816 | `			return DtSerialError(pCtx,zClass);` |
|      - | 7817 | `		}` |
|      5 | 7818 | `	}else{` |
|     39 | 7819 | `		DtIvRestore(pCtx->pVm,pThis,pData);` |
|      - | 7820 | `	}` |
|     47 | 7821 | `	sRes.pThis = pThis;` |
|     47 | 7822 | `	sRes.azOwn = bPeriod ? azDpOwn : azIvOwn;` |
|     47 | 7823 | `	sRes.nOwn = bPeriod ? SX_ARRAYSIZE(azDpOwn) : SX_ARRAYSIZE(azIvOwn);` |
|     47 | 7824 | `	ph7_array_walk(pData,DtPropRestoreWalk,&sRes);` |
|     47 | 7825 | `	DtSetInit(pCtx->pVm,pThis);` |
|     47 | 7826 | `	return PH7_OK;` |
|     34 | 7827 | `}` |
|     54 | 7828 | `static int DtUnserializeProps(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPeriod,` |
|      - | 7829 | `	const char *zClass)` |
|      1 | 7830 | `{` |
|      - | 7831 | `	/* Raw: this is a door that INITIALIZES -- unserialize() calls it on an object` |
|      - | 7832 | `	 * the engine built without a constructor. */` |
|     55 | 7833 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|     55 | 7834 | `	if( pThis == 0 ){` |
|    ! 0 | 7835 | `		return PH7_OK;` |
|      - | 7836 | `	}` |
|     55 | 7837 | `	if( DtCheckDataArg(pCtx,nArg,apArg,zClass) != 0 ){` |
|    ! 0 | 7838 | `		return PH7_EXCEPTION;` |
|      - | 7839 | `	}` |
|     55 | 7840 | `	return DtPropsRestoreInto(pCtx,pThis,apArg[0],bPeriod,zClass);` |
|     28 | 7841 | `}` |
|     30 | 7842 | `static int vm_builtin_DateInterval_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7843 | `{` |
|     31 | 7844 | `	return DtUnserializeProps(pCtx,nArg,apArg,0,"DateInterval");` |
|      1 | 7845 | `}` |
|     24 | 7846 | `static int vm_builtin_DatePeriod_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7847 | `{` |
|     25 | 7848 | `	return DtUnserializeProps(pCtx,nArg,apArg,1,"DatePeriod");` |
|      1 | 7849 | `}` |
|      - | 7850 | `/*` |
|      - | 7851 | ` * __wakeup(): the LEGACY payload, which the engine has already written into the` |
|      - | 7852 | ` * object's own properties -- so the restore rule reads them back off the object` |
|      - | 7853 | ` * itself. An interval whose payload said nothing becomes php's all -1 interval;` |
|      - | 7854 | `` * a period whose payload is incomplete is php's Error, `new DatePeriod` included.`` |
|      - | 7855 | ` */` |
|      2 | 7856 | `static int DtWakeupProps(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPeriod,` |
|      - | 7857 | `	const char *zClass)` |
|      1 | 7858 | `{` |
|      3 | 7859 | `	ph7_class_instance *pThis = DtThisRaw(pCtx);` |
|      - | 7860 | `	ph7_value sProps;` |
|      - | 7861 | `	int rc;` |
|      1 | 7862 | `	SXUNUSED(nArg);` |
|      1 | 7863 | `	SXUNUSED(apArg);` |
|      3 | 7864 | `	if( pThis == 0 ){` |
|    ! 0 | 7865 | `		return PH7_OK;` |
|      - | 7866 | `	}` |
|      3 | 7867 | `	PH7_MemObjInit(pCtx->pVm,&sProps);` |
|      3 | 7868 | `	if( PH7_MemObjToHashmap(&sProps) != SXRET_OK ){` |
|    ! 0 | 7869 | `		PH7_MemObjRelease(&sProps);` |
|    ! 0 | 7870 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7871 | `	}` |
|      3 | 7872 | `	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)sProps.x.pOther);` |
|      3 | 7873 | `	rc = DtPropsRestoreInto(pCtx,pThis,&sProps,bPeriod,zClass);` |
|      3 | 7874 | `	PH7_MemObjRelease(&sProps);` |
|      3 | 7875 | `	return rc;` |
|      2 | 7876 | `}` |
|      2 | 7877 | `static int vm_builtin_DateInterval_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7878 | `{` |
|      3 | 7879 | `	return DtWakeupProps(pCtx,nArg,apArg,0,"DateInterval");` |
|      1 | 7880 | `}` |
|    ! 0 | 7881 | `static int vm_builtin_DatePeriod_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7882 | `{` |
|    ! 0 | 7883 | `	return DtWakeupProps(pCtx,nArg,apArg,1,"DatePeriod");` |
|    ! 0 | 7884 | `}` |
|      - | 7885 | `/* __set_state(array $array): what var_export's text evaluates to. php builds the` |
|      - | 7886 | ` * class the method is DECLARED on, as it does for the date classes. */` |
|     10 | 7887 | `static int DtSetStateProps(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPeriod,` |
|      - | 7888 | `	const char *zClass)` |
|      1 | 7889 | `{` |
|     11 | 7890 | `	ph7_class *pClass = DtClass(pCtx->pVm,zClass);` |
|      - | 7891 | `	ph7_class_instance *pObj;` |
|      - | 7892 | `	int rc;` |
|     11 | 7893 | `	if( pClass == 0 ){` |
|    ! 0 | 7894 | `		return PH7_OK;` |
|      - | 7895 | `	}` |
|     11 | 7896 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - | 7897 | `		char zBuf[64];` |
|    ! 0 | 7898 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 7899 | `			"%s::__set_state(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 | 7900 | `			zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|      - | 7901 | `	}` |
|      - | 7902 | `	/* Not DtNewInstance(): the restore below is what initializes it, and only if` |
|      - | 7903 | `	 * it succeeds. */` |
|     11 | 7904 | `	pObj = PH7_NewClassInstance(pCtx->pVm,pClass);` |
|     11 | 7905 | `	if( pObj == 0 ){` |
|    ! 0 | 7906 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7907 | `	}` |
|     11 | 7908 | `	rc = DtPropsRestoreInto(pCtx,pObj,apArg[0],bPeriod,zClass);` |
|     11 | 7909 | `	if( rc != PH7_OK \|\| pCtx->nThrowRc != 0 ){` |
|      3 | 7910 | `		PH7_ClassInstanceUnref(pObj);` |
|      3 | 7911 | `		return rc;` |
|      - | 7912 | `	}` |
|      9 | 7913 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      9 | 7914 | `	return PH7_OK;` |
|      6 | 7915 | `}` |
|      6 | 7916 | `static int vm_builtin_DateInterval_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7917 | `{` |
|      7 | 7918 | `	return DtSetStateProps(pCtx,nArg,apArg,0,"DateInterval");` |
|      1 | 7919 | `}` |
|      4 | 7920 | `static int vm_builtin_DatePeriod_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7921 | `{` |
|      5 | 7922 | `	return DtSetStateProps(pCtx,nArg,apArg,1,"DatePeriod");` |
|      1 | 7923 | `}` |
|      - | 7924 | `/* The four rows both date classes take. __serialize/__unserialize are php's only` |
|      - | 7925 | ` * NON-tentative internal returns in this family; __wakeup and __set_state carry the` |
|      - | 7926 | `` * `@`, and __set_state's return names the CONCRETE class php's stub writes. */`` |
|      - | 7927 | `#define DT_NATIVE_SERIAL_METHODS(CLS) \` |
|      - | 7928 | `	{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_DateTime_serialize }, \` |
|      - | 7929 | `	{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void", \` |
|      - | 7930 | `	  vm_builtin_##CLS##_unserialize }, \` |
|      - | 7931 | `	{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_##CLS##_wakeup }, \` |
|      - | 7932 | `	{ "__set_state",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@" #CLS, \` |
|      - | 7933 | `	  vm_builtin_##CLS##_setState }` |
|      - | 7934 | `/* php's DateTimeInterface constants. */` |
|      - | 7935 | `#define DT_IFACE_CONST(NAME,VALUE) \` |
|      - | 7936 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, VALUE, 0.0 }` |
|      - | 7937 | `/*` |
|      - | 7938 | ` * Install the whole date family from C: the exceptions and DateTimeInterface, then` |
|      - | 7939 | ` * DateTimeZone / DateTime / DateTimeImmutable, DateInterval and DatePeriod, then the` |
|      - | 7940 | ` * procedural aliases. The InternalIterator its getIterator() answers is not declared` |
|      - | 7941 | ` * here — it is shared native machinery (oo_native.c), reached through the vtable` |
|      - | 7942 | ` * DatePeriod's spec row names.` |
|      - | 7943 | ` *` |
|      - | 7944 | ` * Called from PH7_VmInit inside the bCompilingBuiltin window, after the Reflection` |
|      - | 7945 | ` * install (Exception must exist). IteratorAggregate is attached AFTER DatePeriod's` |
|      - | 7946 | ` * methods exist, for the abstract-stub reason above; DateTimeInterface declares no` |
|      - | 7947 | ` * method, so it can ride the spec table.` |
|      - | 7948 | ` */` |
|   6721 | 7949 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
|      5 | 7950 | `{` |
|      - | 7951 | `	static const PH7_NativeConstDef aIfaceConst[] = {` |
|      - | 7952 | `		DT_IFACE_CONST("ATOM","Y-m-d\\TH:i:sP"),` |
|      - | 7953 | `		DT_IFACE_CONST("COOKIE","l, d-M-Y H:i:s T"),` |
|      - | 7954 | `		DT_IFACE_CONST("ISO8601","Y-m-d\\TH:i:sO"),` |
|      - | 7955 | `		DT_IFACE_CONST("ISO8601_EXPANDED","X-m-d\\TH:i:sP"),` |
|      - | 7956 | `		DT_IFACE_CONST("RFC822","D, d M y H:i:s O"),` |
|      - | 7957 | `		DT_IFACE_CONST("RFC850","l, d-M-y H:i:s T"),` |
|      - | 7958 | `		DT_IFACE_CONST("RFC1036","D, d M y H:i:s O"),` |
|      - | 7959 | `		DT_IFACE_CONST("RFC1123","D, d M Y H:i:s O"),` |
|      - | 7960 | `		DT_IFACE_CONST("RFC7231","D, d M Y H:i:s \\G\\M\\T"),` |
|      - | 7961 | `		DT_IFACE_CONST("RFC2822","D, d M Y H:i:s O"),` |
|      - | 7962 | `		DT_IFACE_CONST("RFC3339","Y-m-d\\TH:i:sP"),` |
|      - | 7963 | `		DT_IFACE_CONST("RFC3339_EXTENDED","Y-m-d\\TH:i:s.vP"),` |
|      - | 7964 | `		DT_IFACE_CONST("RSS","D, d M Y H:i:s O"),` |
|      - | 7965 | `		DT_IFACE_CONST("W3C","Y-m-d\\TH:i:sP"),` |
|      - | 7966 | `	};` |
|      - | 7967 | `	/*` |
|      - | 7968 | `	 * php's DateTimeInterface METHODS, which this engine did not declare at` |
|      - | 7969 | ``	 * all -- so the nine it contracts for reported no `prototype` in the`` |
|      - | 7970 | `	 * export (18 rows across DateTime and DateTimeImmutable), and the` |
|      - | 7971 | `	 * interface itself answered isAbstract() false for want of a member.` |
|      - | 7972 | `	 * Declared in php's own order, which is the order its export lists them.` |
|      - | 7973 | `	 *` |
|      - | 7974 | `	 * Safe on the spec table because PH7_InstallNativeClasses fills every` |
|      - | 7975 | `	 * class's METHODS before it wires any interface: PH7_ClassImplement's` |
|      - | 7976 | `	 * abstract stubbing then finds DateTime's own nine already there and` |
|      - | 7977 | `	 * skips them, which is the same reason DatePeriod's IteratorAggregate is` |
|      - | 7978 | `	 * attached by hand AFTER its methods (it is not in this table).` |
|      - | 7979 | `	 */` |
|      - | 7980 | `	static const PH7_NativeMethodDef aIfaceMethod[] = {` |
|      - | 7981 | `		{ "format",        PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $format", "@string", 0 },` |
|      - | 7982 | `		{ "getTimezone",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@DateTimeZone\|false", 0 },` |
|      - | 7983 | `		{ "getOffset",     PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@int", 0 },` |
|      - | 7984 | `		{ "getTimestamp",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@int", 0 },` |
|      - | 7985 | `		{ "getMicrosecond",PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "int", 0 },` |
|      - | 7986 | `		{ "diff",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT,` |
|      - | 7987 | `		  "DateTimeInterface $targetObject, bool $absolute = false", "@DateInterval", 0 },` |
|      - | 7988 | `		{ "__wakeup",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|      - | 7989 | `		{ "__serialize",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|      - | 7990 | `		{ "__unserialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "array $data", "void", 0 },` |
|      - | 7991 | `	};` |
|      - | 7992 | `	static const PH7_NativePropDef aZoneProp[] = {` |
|      - | 7993 | `		{ DTZ_OFF,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 7994 | `		{ DTZ_NAME, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 }, 0 },` |
|      - | 7995 | `		{ DTZ_KIND, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|      - | 7996 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DT_ZONE_ID, 0, 0.0 }, 0 },` |
|      - | 7997 | `		{ DT_INIT,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|      - | 7998 | `	};` |
|      - | 7999 | `	static const PH7_NativeMethodDef aZoneMethod[] = {` |
|      - | 8000 | `		{ "__construct", PH7_MOD_PUBLIC, "string $timezone", "", vm_builtin_DateTimeZone_construct },` |
|      - | 8001 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_DateTimeZone_getName },` |
|      - | 8002 | `		{ "getOffset",   PH7_MOD_PUBLIC, "DateTimeInterface $datetime", "@int",` |
|      - | 8003 | `		  vm_builtin_DateTimeZone_getOffset },` |
|      - | 8004 | `		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_DateTimeZone_serialize },` |
|      - | 8005 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|      - | 8006 | `		  vm_builtin_DateTimeZone_unserialize },` |
|      - | 8007 | `		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DateTimeZone_wakeup },` |
|      - | 8008 | `		{ "__set_state",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@DateTimeZone",` |
|      - | 8009 | `		  vm_builtin_DateTimeZone_setState },` |
|      - | 8010 | `	};` |
|      - | 8011 | `	static const PH7_NativePropDef aDtProp[] = { DT_NATIVE_STATE_PROPS };` |
|      - | 8012 | `	static const PH7_NativeMethodDef aDtMethod[] = {` |
|      - | 8013 | `		DT_NATIVE_SHARED_METHODS("DateTime"),` |
|      - | 8014 | `		{ "createFromFormat",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 8015 | `		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTime\|false",` |
|      - | 8016 | `		  vm_builtin_DateTime_createFromFormat },` |
|      - | 8017 | `		{ "createFromImmutable", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeImmutable $object", "@static",` |
|      - | 8018 | `		  vm_builtin_DateTime_copyOf },` |
|      - | 8019 | `		{ "createFromTimestamp", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "int\|float $timestamp", "@static",` |
|      - | 8020 | `		  vm_builtin_DateTime_createFromTimestamp },` |
|      - | 8021 | `		{ "createFromInterface", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTime",` |
|      - | 8022 | `		  vm_builtin_DateTime_copyOf },` |
|      - | 8023 | `		DT_NATIVE_SERIAL_METHODS(DateTime),` |
|      - | 8024 | `	};` |
|      - | 8025 | `	static const PH7_NativeMethodDef aImmMethod[] = {` |
|      - | 8026 | `		DT_NATIVE_SHARED_METHODS("DateTimeImmutable"),` |
|      - | 8027 | `		{ "createFromFormat",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 8028 | `		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTimeImmutable\|false",` |
|      - | 8029 | `		  vm_builtin_DateTimeImmutable_createFromFormat },` |
|      - | 8030 | `		{ "createFromMutable",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTime $object", "@static",` |
|      - | 8031 | `		  vm_builtin_DateTimeImmutable_copyOf },` |
|      - | 8032 | `		{ "createFromTimestamp", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "int\|float $timestamp", "@static",` |
|      - | 8033 | `		  vm_builtin_DateTimeImmutable_createFromTimestamp },` |
|      - | 8034 | `		{ "createFromInterface", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTimeImmutable",` |
|      - | 8035 | `		  vm_builtin_DateTimeImmutable_copyOf },` |
|      - | 8036 | `		DT_NATIVE_SERIAL_METHODS(DateTimeImmutable),` |
|      - | 8037 | `	};` |
|      - | 8038 | `	static const PH7_NativePropDef aIvProp[] = {` |
|      - | 8039 | `		{ "y",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 8040 | `		{ "m",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 8041 | `		{ "d",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 8042 | `		{ "h",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 8043 | `		{ "i",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 8044 | `		{ "s",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 8045 | ``		/* php's `f` is a FLOAT; the chunk's `= 0` made it an int. */`` |
|      - | 8046 | `		{ "f",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_DOUBLE, 0, 0, 0.0 }, 0 },` |
|      - | 8047 | `		{ "invert",      PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 8048 | `		{ "days",        PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - | 8049 | `		{ "from_string", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - | 8050 | ``		/* php's `date_string` is on an interval built from a STRING and on no`` |
|      - | 8051 | `		 * other, so it is installed by the write that names it: an ordinary` |
|      - | 8052 | ``		 * interval does not carry the name at all, and `isset()` says so. */`` |
|      - | 8053 | `		{ "date_string", PH7_MOD_PUBLIC\|PH7_MOD_ONDEMAND,` |
|      - | 8054 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 8055 | ``		/* php's timelib_rel_time.us, the count `f` renders: see DtIvUsec. */`` |
|      - | 8056 | `		{ DT_IV_US,      PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|      - | 8057 | `		{ DT_INIT,       PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|      - | 8058 | `	};` |
|      - | 8059 | `	static const PH7_NativeMethodDef aIvMethod[] = {` |
|      - | 8060 | `		{ "__construct", PH7_MOD_PUBLIC, "string $duration", "",` |
|      - | 8061 | `		  vm_builtin_DateInterval_construct },` |
|      - | 8062 | `		{ "createFromDateString", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $datetime", "@DateInterval",` |
|      - | 8063 | `		  vm_builtin_DateInterval_createFromDateString },` |
|      - | 8064 | `		{ "format",      PH7_MOD_PUBLIC, "string $format", "@string", vm_builtin_DateInterval_format },` |
|      - | 8065 | `		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", DtSerializeProps },` |
|      - | 8066 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|      - | 8067 | `		  vm_builtin_DateInterval_unserialize },` |
|      - | 8068 | `		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DateInterval_wakeup },` |
|      - | 8069 | `		{ "__set_state",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@DateInterval",` |
|      - | 8070 | `		  vm_builtin_DateInterval_setState },` |
|      - | 8071 | `	};` |
|      - | 8072 | `	/* php models all seven as VIRTUAL hooked properties, so it reports no default` |
|      - | 8073 | `	 * for any of them; PHL's are real slots and keep theirs, because a read before` |
|      - | 8074 | `	 * the first write must answer what php's getter answers rather than raise. The` |
|      - | 8075 | `	 * TYPE is what a spec row can state exactly — the virtual half is PLAN §7.4. */` |
|      - | 8076 | `	static const PH7_NativePropDef aDpProp[] = {` |
|      - | 8077 | `		{ "start",              PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 8078 | `		{ "current",            PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 8079 | `		{ "end",                PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 8080 | `		{ "interval",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateInterval" },` |
|      - | 8081 | `		/* php FABRICATES these from its struct, so an object with no struct reads` |
|      - | 8082 | `		 * them as the zeroed one: 0 and false, not the 1 and true a constructed` |
|      - | 8083 | `		 * period ends up with. Every constructor path writes all three. */` |
|      - | 8084 | `		{ "recurrences",        PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, "int" },` |
|      - | 8085 | `		{ "include_start_date", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|      - | 8086 | `		{ "include_end_date",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|      - | 8087 | `		{ DT_INIT,              PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|      - | 8088 | `		  { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|      - | 8089 | `	};` |
|      - | 8090 | `	static const PH7_NativeConstDef aDpConst[] = {` |
|      - | 8091 | `		{ "EXCLUDE_START_DATE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|      - | 8092 | `		{ "INCLUDE_END_DATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|      - | 8093 | `	};` |
|      - | 8094 | `	static const PH7_NativeMethodDef aDpMethod[] = {` |
|      - | 8095 | `		/* php overloads this constructor three ways and rejects everything else with` |
|      - | 8096 | `		 * ONE message, so the signature stays unenforced and the body decides. */` |
|      - | 8097 | `		/* No signature ON PURPOSE, which is why Reflection reports no parameters` |
|      - | 8098 | `		 * for it. php declares four and enforces NEITHER end of the arity: the` |
|      - | 8099 | `		 * constructor has three shapes (start+interval+end, start+interval+count,` |
|      - | 8100 | ``		 * and the ISO string), and both `new DatePeriod()` and a five-argument`` |
|      - | 8101 | `		 * call reach the body and answer its own three-shape TypeError. A zSig` |
|      - | 8102 | `		 * here would enforce both bounds, so the choice is php's DIAGNOSTIC or` |
|      - | 8103 | `		 * php's parameter list, and the diagnostic wins. */` |
|      - | 8104 | `		{ "__construct",     PH7_MOD_PUBLIC, 0, "", vm_builtin_DatePeriod_construct },` |
|      - | 8105 | `		{ "createFromISO8601String", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 8106 | `		  "string $specification, int $options = 0", "static",` |
|      - | 8107 | `		  vm_builtin_DatePeriod_createFromISO8601String },` |
|      - | 8108 | `		{ "getStartDate",    PH7_MOD_PUBLIC, "", "@DateTimeInterface",` |
|      - | 8109 | `		  vm_builtin_DatePeriod_getStartDate },` |
|      - | 8110 | `		{ "getEndDate",      PH7_MOD_PUBLIC, "", "@?DateTimeInterface",` |
|      - | 8111 | `		  vm_builtin_DatePeriod_getEndDate },` |
|      - | 8112 | `		{ "getDateInterval", PH7_MOD_PUBLIC, "", "@DateInterval",` |
|      - | 8113 | `		  vm_builtin_DatePeriod_getDateInterval },` |
|      - | 8114 | `		{ "getRecurrences",  PH7_MOD_PUBLIC, "", "@?int", vm_builtin_DatePeriod_getRecurrences },` |
|      - | 8115 | `		{ "getIterator",     PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_DatePeriod_getIterator },` |
|      - | 8116 | `		{ "__serialize",     PH7_MOD_PUBLIC, "", "array", DtSerializeProps },` |
|      - | 8117 | `		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "void",` |
|      - | 8118 | `		  vm_builtin_DatePeriod_unserialize },` |
|      - | 8119 | `		{ "__wakeup",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_DatePeriod_wakeup },` |
|      - | 8120 | `		{ "__set_state",     PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@DatePeriod",` |
|      - | 8121 | `		  vm_builtin_DatePeriod_setState },` |
|      - | 8122 | `	};` |
|      - | 8123 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 8124 | `		/* Exceptions first: the classes below throw them. */` |
|      - | 8125 | `		{ "DateException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8126 | `		{ "DateMalformedStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8127 | `		{ "DateInvalidTimeZoneException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8128 | `		{ "DateMalformedIntervalStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8129 | `		{ "DateMalformedPeriodStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8130 | `		{ "DateInvalidOperationException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8131 | `		/* php's date tree has an ERROR half beside the exception one -- what a` |
|      - | 8132 | `		 * caller catches when an argument is out of RANGE (setMicrosecond) or the` |
|      - | 8133 | `		 * object was never constructed. All three were undefined here, so` |
|      - | 8134 | ``		 * `catch (DateRangeError $e)` could not be spelled at all. */`` |
|      - | 8135 | `		{ "DateError", "Error", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8136 | `		{ "DateRangeError", "DateError", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8137 | `		{ "DateObjectError", "DateError", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8138 | `		{ "DateTimeInterface", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 8139 | `		  aIfaceMethod, SX_ARRAYSIZE(aIfaceMethod),` |
|      - | 8140 | `		  aIfaceConst, SX_ARRAYSIZE(aIfaceConst), 0, 0, 0, 0, 0 },` |
|      - | 8141 | `		{ "DateTimeZone", 0, 0, 0,` |
|      - | 8142 | `		  aZoneMethod, SX_ARRAYSIZE(aZoneMethod), 0, 0, aZoneProp, SX_ARRAYSIZE(aZoneProp),` |
|      - | 8143 | `		  0, 0, DtPresentTimeZone },` |
|      - | 8144 | `		{ "DateTime", 0, "DateTimeInterface", 0,` |
|      - | 8145 | `		  aDtMethod, SX_ARRAYSIZE(aDtMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),` |
|      - | 8146 | `		  0, 0, DtPresentDateTime },` |
|      - | 8147 | `		{ "DateTimeImmutable", 0, "DateTimeInterface", 0,` |
|      - | 8148 | `		  aImmMethod, SX_ARRAYSIZE(aImmMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),` |
|      - | 8149 | `		  0, 0, DtPresentDateTime },` |
|      - | 8150 | `		{ "DateInterval", 0, 0, 0,` |
|      - | 8151 | `		  aIvMethod, SX_ARRAYSIZE(aIvMethod), 0, 0, aIvProp, SX_ARRAYSIZE(aIvProp),` |
|      - | 8152 | `		  0, 0, DtPresentProps },` |
|      - | 8153 | `		{ "DatePeriod", 0, 0, 0,` |
|      - | 8154 | `		  aDpMethod, SX_ARRAYSIZE(aDpMethod), aDpConst, SX_ARRAYSIZE(aDpConst),` |
|      - | 8155 | `		  aDpProp, SX_ARRAYSIZE(aDpProp), 0, &sDpIterVtab, DtPresentProps },` |
|      - | 8156 | `	};` |
|      - | 8157 | `	/* php's procedural aliases. Each is a function in its own right, not a forward,` |
|      - | 8158 | `	 * and each owes aBuiltinSig[] a row (vm_arg_check.c). */` |
|      - | 8159 | `	static const struct {` |
|      - | 8160 | `		const char *zName;` |
|      - | 8161 | `		ProchHostFunction xFunc;` |
|      - | 8162 | `	} aFunc[] = {` |
|      - | 8163 | `		{ "strtotime",                    vm_builtin_strtotime },` |
|      - | 8164 | `		{ "date_create",                  vm_builtin_date_create },` |
|      - | 8165 | `		{ "date_create_immutable",        vm_builtin_date_create_immutable },` |
|      - | 8166 | `		{ "date_create_from_format",      vm_builtin_date_create_from_format },` |
|      - | 8167 | `		{ "date_create_immutable_from_format", vm_builtin_date_create_immutable_from_format },` |
|      - | 8168 | `		{ "date_format",                  vm_builtin_date_format },` |
|      - | 8169 | `		{ "date_modify",                  vm_builtin_date_modify },` |
|      - | 8170 | `		{ "date_add",                     vm_builtin_date_add },` |
|      - | 8171 | `		{ "date_sub",                     vm_builtin_date_sub },` |
|      - | 8172 | `		{ "date_diff",                    vm_builtin_date_diff },` |
|      - | 8173 | `		{ "date_timestamp_get",           vm_builtin_date_timestamp_get },` |
|      - | 8174 | `		{ "date_timestamp_set",           vm_builtin_date_timestamp_set },` |
|      - | 8175 | `		{ "date_timezone_get",            vm_builtin_date_timezone_get },` |
|      - | 8176 | `		{ "date_timezone_set",            vm_builtin_date_timezone_set },` |
|      - | 8177 | `		{ "date_offset_get",              vm_builtin_date_offset_get },` |
|      - | 8178 | `		{ "date_date_set",                vm_builtin_date_date_set },` |
|      - | 8179 | `		{ "date_time_set",                vm_builtin_date_time_set },` |
|      - | 8180 | `		{ "date_isodate_set",             vm_builtin_date_isodate_set },` |
|      - | 8181 | `		{ "date_interval_create_from_date_string", vm_builtin_date_interval_create_from_date_string },` |
|      - | 8182 | `		{ "date_interval_format",         vm_builtin_date_interval_format },` |
|      - | 8183 | `		{ "date_get_last_errors",         vm_builtin_date_get_last_errors },` |
|      - | 8184 | `		{ "date_parse",                   vm_builtin_date_parse },` |
|      - | 8185 | `		{ "date_parse_from_format",       vm_builtin_date_parse_from_format },` |
|      - | 8186 | `		{ "timezone_open",                vm_builtin_timezone_open },` |
|      - | 8187 | `		{ "timezone_name_get",            vm_builtin_timezone_name_get },` |
|      - | 8188 | `		{ "timezone_offset_get",          vm_builtin_timezone_offset_get },` |
|      - | 8189 | `	};` |
|      - | 8190 | `	sxu32 n;` |
|      - | 8191 | `	sxi32 rc;` |
|      - | 8192 | `	/* php's date.timezone default */` |
|   6726 | 8193 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|   6726 | 8194 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|      - | 8195 | `	/* The error rows are allocated from the VM's own backend and released` |
|      - | 8196 | `	 * wholesale with it, so this is the only lifetime call they need. */` |
|   6726 | 8197 | `	SyBlobInit(&pVm->sDtLastErr.sErr,&pVm->sAllocator);` |
|   6726 | 8198 | `	DtLastErrClear(&(*pVm));` |
| 181472 | 8199 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 174751 | 8200 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  87261 | 8201 | `	}` |
|   6726 | 8202 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|   6726 | 8203 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 8204 | `		return rc;` |
|      - | 8205 | `	}` |
|      - | 8206 | `	/* php's write_property handler for DateInterval (ph7_class::xSet), assigned` |
|      - | 8207 | `	 * here for the reason the DOM's clone and dimension hooks are: the spec table` |
|      - | 8208 | `	 * carries no field for a hook. It also flags the class's properties, which is` |
|      - | 8209 | ``	 * what makes `new` register their slots with the store filter. */`` |
|   6726 | 8210 | `	rc = PH7_NativeClassInstallSetHook(&(*pVm),"DateInterval",DtIntervalSet);` |
|   6726 | 8211 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 8212 | `		return rc;` |
|      - | 8213 | `	}` |
|      - | 8214 | `	/* php's compare handlers (ph7_class::xCmp), assigned here for the same reason` |
|      - | 8215 | `	 * the write handler is. DatePeriod gets none: php has no handler for it, its` |
|      - | 8216 | `	 * real property table is EMPTY (the seven it shows are fabricated), and the` |
|      - | 8217 | `	 * ordinary walk over nothing is what makes any two of them equal -- which is` |
|      - | 8218 | `	 * what marking those seven virtual reproduces. */` |
|      - | 8219 | `	{` |
|      - | 8220 | `		static const struct {` |
|      - | 8221 | `			const char *zClass;` |
|      - | 8222 | `			void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *);` |
|      - | 8223 | `		} aCmp[] = {` |
|      - | 8224 | `			{ "DateTime",          DtCmpDateTime },` |
|      - | 8225 | `			{ "DateTimeImmutable", DtCmpDateTime },` |
|      - | 8226 | `			{ "DateInterval",      DtCmpInterval },` |
|      - | 8227 | `			{ "DateTimeZone",      DtCmpTimeZone },` |
|      - | 8228 | `		};` |
|  33610 | 8229 | `		for( n = 0 ; n < SX_ARRAYSIZE(aCmp) ; n++ ){` |
|  26889 | 8230 | `			rc = PH7_NativeClassInstallCmpHook(&(*pVm),aCmp[n].zClass,aCmp[n].xCmp);` |
|  26889 | 8231 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8232 | `				return rc;` |
|      - | 8233 | `			}` |
|  13429 | 8234 | `		}` |
|      - | 8235 | `	}` |
|   6726 | 8236 | `	rc = PH7_NativeClassMarkVirtualProps(&(*pVm),"DatePeriod");` |
|   6726 | 8237 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 8238 | `		return rc;` |
|      - | 8239 | `	}` |
|      - | 8240 | `	/* php 8.5 marks every IMMUTABLE mutator #[\NoDiscard]: these nine answer a NEW` |
|      - | 8241 | `	 * object and change nothing, so a caller who drops the answer wrote a` |
|      - | 8242 | `	 * statement that does nothing at all -- the single most common way to misuse` |
|      - | 8243 | `	 * DateTimeImmutable. The mutable DateTime twins are NOT marked (there the` |
|      - | 8244 | `	 * object really did change), and neither is any other internal member: this is` |
|      - | 8245 | `	 * php's whole internal NoDiscard set. The message is php's own wording, with` |
|      - | 8246 | `	 * the method named in it. */` |
|      - | 8247 | `	{` |
|      - | 8248 | `		/* Nine rows, nine static literals: PH7_NativeMethodSetNoDiscard borrows the` |
|      - | 8249 | `		 * argument record for the VM's lifetime. php's stub spells the message as a` |
|      - | 8250 | `		 * NAMED argument, and getArguments() shows the key. */` |
|      - | 8251 | `		static const PH7_NativeAttrArg aNdWhy[] = {` |
|      - | 8252 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 8253 | `			  "as DateTimeImmutable::modify() does not modify the object itself", 0.0 } },` |
|      - | 8254 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 8255 | `			  "as DateTimeImmutable::add() does not modify the object itself", 0.0 } },` |
|      - | 8256 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 8257 | `			  "as DateTimeImmutable::sub() does not modify the object itself", 0.0 } },` |
|      - | 8258 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 8259 | `			  "as DateTimeImmutable::setTimezone() does not modify the object itself", 0.0 } },` |
|      - | 8260 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 8261 | `			  "as DateTimeImmutable::setTime() does not modify the object itself", 0.0 } },` |
|      - | 8262 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 8263 | `			  "as DateTimeImmutable::setDate() does not modify the object itself", 0.0 } },` |
|      - | 8264 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 8265 | `			  "as DateTimeImmutable::setISODate() does not modify the object itself", 0.0 } },` |
|      - | 8266 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 8267 | `			  "as DateTimeImmutable::setTimestamp() does not modify the object itself", 0.0 } },` |
|      - | 8268 | `			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,` |
|      - | 8269 | `			  "as DateTimeImmutable::setMicrosecond() does not modify the object itself", 0.0 } },` |
|      - | 8270 | `		};` |
|      - | 8271 | `		static const char *const azNdMethod[] = {` |
|      - | 8272 | `			"modify","add","sub","setTimezone","setTime","setDate","setISODate",` |
|      - | 8273 | `			"setTimestamp","setMicrosecond"` |
|      - | 8274 | `		};` |
|   6726 | 8275 | `		ph7_class *pImm = PH7_VmExtractClass(&(*pVm),"DateTimeImmutable",` |
|      - | 8276 | `			sizeof("DateTimeImmutable")-1,FALSE,0);` |
|  67215 | 8277 | `		for( n = 0 ; n < SX_ARRAYSIZE(azNdMethod) ; n++ ){` |
|  60494 | 8278 | `			rc = PH7_NativeMethodSetNoDiscard(&(*pVm),pImm,azNdMethod[n],&aNdWhy[n],1);` |
|  60494 | 8279 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8280 | `				return rc;` |
|      - | 8281 | `			}` |
|  30209 | 8282 | `		}` |
|      - | 8283 | `	}` |
|      - | 8284 | `	/* php's state for these two IS their properties, and the table is written FROM` |
|      - | 8285 | `	 * the C struct its constructor allocates -- so an object nobody constructed has` |
|      - | 8286 | ``	 * no such property at all. PHL declared them from `new`, so an unconstructed`` |
|      - | 8287 | `	 * interval answered ten defaults to a read, ten to isset(), ten to` |
|      - | 8288 | `	 * get_object_vars() and ten to a property foreach, beside the empty shape the` |
|      - | 8289 | `	 * presentation hook was already showing. The two classes differ in what a read` |
|      - | 8290 | `	 * of a still-absent slot answers, which is php's split between its two` |
|      - | 8291 | `	 * handlers: DatePeriod reads its seven from the zeroed struct (null/0/false, in` |
|      - | 8292 | `	 * silence), DateInterval has no such fallback and its ten really are undefined` |
|      - | 8293 | `	 * until the constructor runs. */` |
|   6726 | 8294 | `	rc = PH7_NativeClassMarkLazyProps(&(*pVm),"DateInterval",0);` |
|   6726 | 8295 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 8296 | `		return rc;` |
|      - | 8297 | `	}` |
|   6726 | 8298 | `	rc = PH7_NativeClassMarkLazyProps(&(*pVm),"DatePeriod",1);` |
|   6726 | 8299 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 8300 | `		return rc;` |
|      - | 8301 | `	}` |
|      - | 8302 | `	/* php's write_property handler for DatePeriod refuses OUTRIGHT: the seven are` |
|      - | 8303 | `	 * a view of its struct and a script may only read them. PHL kept real slots a` |
|      - | 8304 | ``	 * script could write, so `$p->recurrences = 99` and `$p->start = 5` landed and`` |
|      - | 8305 | `	 * the period then iterated to a shape no constructor would have built --` |
|      - | 8306 | ``	 * `unset($p->interval)` left one with no interval at all. */`` |
|   6726 | 8307 | `	rc = PH7_NativeClassMarkNoWriteProps(&(*pVm),"DatePeriod");` |
|   6726 | 8308 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 8309 | `		return rc;` |
|      - | 8310 | `	}` |
|      - | 8311 | `	/* IteratorAggregate declares a METHOD, so it is attached now that DatePeriod has` |
|      - | 8312 | `	 * its own: PH7_ClassImplement stubs a missing one as ABSTRACT, which would have` |
|      - | 8313 | `	 * made the class uninstantiable. */` |
|      - | 8314 | `	{` |
|   6726 | 8315 | `		ph7_class *pPeriod = DtClass(&(*pVm),"DatePeriod");` |
|   6726 | 8316 | `		ph7_class *pAggregate = DtClass(&(*pVm),"IteratorAggregate");` |
|   6726 | 8317 | `		if( pPeriod == 0 \|\| pAggregate == 0 ){` |
|    ! 0 | 8318 | `			return SXERR_NOTFOUND;` |
|      - | 8319 | `		}` |
|   6726 | 8320 | `		rc = PH7_ClassImplement(pPeriod,pAggregate);` |
|      - | 8321 | `	}` |
|   6726 | 8322 | `	return rc;` |
|   3361 | 8323 | `}` |
|      - | 8324 |  |
|      - | 8325 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 8326 |  |
|      - | 8327 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 8328 | `/* Tiny build: no DateTime family (builtin layer disabled) */` |
|      - | 8329 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){` |
|      - | 8330 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|      - | 8331 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|      - | 8332 | `	return SXRET_OK;` |
|      - | 8333 | `}` |
|      - | 8334 | `#endif` |
|      - | 8335 |  |
