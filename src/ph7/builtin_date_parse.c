/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdio.h>   /* snprintf: the digit engine for php's own %g rendering */
/*
 * The DateTime family: proleptic-Gregorian date math, the date/time
 * string parser, the __dt_* host thunks, the embedded zDateTimeLib PHP
 * chunk and PH7_VmInstallDateTime. The classic procedural date functions
 * (date/gmdate/mktime/...) stay in builtin_date.c.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
#include <time.h>
/* ===========================================================================
 * DateTime family: DateTimeInterface, DateTime,
 * DateTimeImmutable, DateTimeZone (UTC + fixed offsets), date_create(),
 * date_create_immutable(). Embedded-PHP chunk + C thunks, following the
 * Reflection architecture (installed inside the bCompilingBuiltin window).
 * Timezone SCOPE: UTC and fixed "+HH:MM" offsets only — no tz database
 * (recorded the scope policy scope cut; named region zones throw like unknown zones).
 * ======================================================================== */

/*
 * Proleptic-Gregorian civil <-> day-count conversions (Howard Hinnant's
 * algorithms): no time_t / libc dependence, correct far past 2038 and
 * before 1970 on every platform. Day 0 == 1970-01-01.
 */
PH7_PRIVATE sxi64 DtDaysFromCivil(sxi64 y,int m,int d)
{
	sxi64 era;
	unsigned yoe,doy,doe;
	/* Every step is spelled in UNSIGNED arithmetic: the year reaching here is
	 * whatever the string held, php's own answer for one past the clock is the
	 * WRAP below, and a signed overflow on the way to it is undefined (this
	 * build gates on UBSan). The bits are the same either way. */
	y = (sxi64)((sxu64)y - (sxu64)(m <= 2));
	era = (y >= 0 ? y : (sxi64)((sxu64)y - 399u)) / 400;
	yoe = (unsigned)((sxu64)y - (sxu64)era * 400u);
	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	/* Unsigned tail: php's expanded ISO year has no width limit, and php's own
	 * answer for one past the clock is a WRAP of the seconds it converts to --
	 * `new DateTime('-999999999999-01-01')` reads back as year 169108098508
	 * there. Wrapping through sxu64 reproduces that instead of overflowing a
	 * signed product, which is undefined. */
	return (sxi64)((sxu64)era * 146097u + (sxu64)doe - 719468u);
}
PH7_PRIVATE void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)
{
	sxi64 era;
	unsigned doe,yoe,doy,mp;
	z = (sxi64)((sxu64)z + 719468u);
	era = (z >= 0 ? z : (sxi64)((sxu64)z - 146096u)) / 146097;
	doe = (unsigned)((sxu64)z - (sxu64)era * 146097u);
	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
	*py = (sxi64)((sxu64)yoe + (sxu64)era * 400u);
	doy = doe - (365 * yoe + yoe/4 - yoe/100);
	mp = (5 * doy + 2) / 153;
	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);
	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);
	if( *pm <= 2 ){
		*py = (sxi64)((sxu64)*py + 1u);
	}
}
PH7_PRIVATE sxi64 DtFloorDiv(sxi64 a,sxi64 b)
{
	sxi64 q = a / b;
	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){
		q--;
	}
	return q;
}
/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z).
 * Shared with builtin_date.c, whose procedural doors used to reach for the
 * platform's gmtime() and lose every year past an int. */
PH7_PRIVATE void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)
{
	/* Both steps are written to stay DEFINED at the ends of php's clock:
	 * `iTs + iOff` overflows for a timestamp near the int64 floor, and so does
	 * rebuilding the day's start as `days * 86400` (UBSan caught the second at
	 * setTimestamp(PHP_INT_MIN)->format()). The remainder gives the same
	 * seconds-of-day with no product at all. */
	sxi64 t = (sxi64)((sxu64)iTs + (sxu64)iOff);
	sxi64 days = DtFloorDiv(t,86400);
	sxi64 secs = t % 86400;
	sxi64 y;
	int mo,d;
	if( secs < 0 ){
		secs += 86400;
	}
	DtCivilFromDays(days,&y,&mo,&d);
	pTm->tm_sec  = (int)(secs % 60);
	pTm->tm_min  = (int)((secs / 60) % 60);
	pTm->tm_hour = (int)(secs / 3600);
	pTm->tm_mday = d;
	pTm->tm_mon  = mo - 1;
	pTm->tm_year = y;
	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */
	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));
	pTm->tm_isdst = 0;
	pTm->tm_zone = zZone;
	/* No abbreviation and no DST unless a caller that KNOWS the zone fills them
	 * in afterwards -- which only a database zone can. Every other caller gets
	 * the answers this family has always given. */
	pTm->tm_abbr = 0;
	pTm->tm_nabbr = 0;
	pTm->tm_gmtoff = (long)iOff;
}
static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)
{
	/* Unsigned throughout: a year outside the clock's own range (the parser
	 * accepts php's expanded form, which has no width limit) would otherwise
	 * overflow this product, which is undefined rather than merely wrong. */
	sxu64 t = (sxu64)DtDaysFromCivil(y,mo,d) * 86400u;
	t += (sxu64)((sxi64)h*3600 + (sxi64)mi*60 + s - iOff);
	return (sxi64)t;
}
/*
 * php's date string parse is a FIELD parse. timelib fills a civil y/m/d/h/i/s/us
 * vector plus a SEPARATE relative one and applies NOTHING until the whole string
 * has been read, which is what makes the written ORDER of a relative string
 * irrelevant there -- `+1 day +1 month` and `+1 month +1 day` are one answer --
 * and what puts the months on the day before the days move it. PHL applied every
 * unit to the clock as it scanned, so `+30 days +1 month` was a day or two off.
 *
 * A field the string never mentions stays DT_UNSET and is filled from the BASE
 * moment afterwards (php's timelib_fill_holes), which is what lets a bare month
 * name keep the base day and a bare date keep the base time of day.
 */
/* php's TIMELIB_UNSET, and the NUMBER matters as well as the marking: its
 * normalizer carries an unset field into the one above it like any other, so
 * the date a half-read clock ends up publishing is a function of this value. */
#define DT_UNSET ((sxi64)-9999999)
typedef struct dt_parsed dt_parsed;
struct dt_parsed
{
	sxi64 y,m,d;                    /* absolute date fields, or DT_UNSET */
	sxi64 h,i,s,us;                 /* absolute time fields, or DT_UNSET */
	sxi64 ry,rm,rd,rh,ri,rs,rus;    /* the relative vector */
	int bHaveDate;                  /* the string set an absolute date element */
	int nTimeTok;                   /* php's have_time: 0 = the string named no time
	                                 * of day, 1 = it named one, 2 = a second bare
	                                 * digit run then read as a YEAR */
	int bWday;                      /* the string named a weekday */
	int iWday;                      /* that weekday, 0=Sunday..6 -- or NEGATIVE,
	                                 * which is what `ago` makes of it */
	int iWdayBehavior;              /* php's 0 (next/last), 1 (bare name), 2 (... this week) */
	int iFirstLast;                 /* php's first_last_day_of: 0 none, 1 first, 2 last */
	int bWdayOf;                    /* php's `first monday of` special: a weekday
	                                 * hunted inside the MONTH the rest of the
	                                 * string lands on, rather than from the day */
	int iWdayOfNext;                /* ...and whether it starts from the month
	                                 * AFTER, which is php's `last` and `this` */
	int bWeekdays;                  /* php's `weekday` special was named */
	sxi64 iWeekdays;                /* ... this many BUSINESS days */
	sxi32 iOff;                     /* the offset in force */
	int bOffSet;                    /* 0 = the string named no zone, 1 = an offset,
	                                 * 2 = a NAME the string spelled (zZone below) */
	const char *zZone;              /* that name, a literal: "Z", "UTC", "GMT" */
	int nZone;
	int bZoneIdent;                 /* php's timezone_type 3 rather than 2 -- only
	                                 * "UTC" spelled in that exact case */
	int nZoneTok;                   /* how many zone TOKENS the string spelled: the
	                                 * second is ignored and the third refused */
	int bEpoch;                     /* the string named an `@epoch`, which is the
	                                 * one form modify() lets name a ZONE */
	int bUsUnset;                   /* php's vector leaves its microseconds UNSET
	                                 * where PHL writes a zero for the CLOCK's
	                                 * sake: the two nocolon arms php reaches
	                                 * without its HAVE_TIME ever running, a bare
	                                 * four-digit clock and a bare year. It is
	                                 * what date_parse() shows as `false`. */
	int bHaveRel;                   /* php's have_relative: the string spelled a
	                                 * RELATIVE element, which is what puts the
	                                 * `relative` block in date_parse()'s answer.
	                                 * `now`, `today` and a bare `ago` do not. */
	/* Warnings a RULE raises but only the scan can publish. They ride the vector
	 * so that the copy a longest-match probe runs on discards them with itself:
	 * a probe that succeeds makes its caller stand down and the real rule raises
	 * them again, a probe that fails raised nothing. */
	int nWarnPend;
	int aWarnPos[PH7_DT_MAX_WARN];
	const char *azWarn[PH7_DT_MAX_WARN];
};
/* php's add_warning, held until the scan can publish it. */
static void DtWarnPend(dt_parsed *p,int iPos,const char *zMsg)
{
	if( p->nWarnPend < PH7_DT_MAX_WARN ){
		p->aWarnPos[p->nWarnPend] = iPos;
		p->azWarn[p->nWarnPend] = zMsg;
	}
	p->nWarnPend++;
}
/* Wrapping add: a relative vector holds whatever the string spelled, and php's
 * own answer past the int64 ceiling is garbage of its own -- but the OVERFLOW
 * would be undefined here, and this build gates on UBSan. */
static sxi64 DtWAdd(sxi64 a,sxi64 b)
{
	return (sxi64)((sxu64)a + (sxu64)b);
}
static sxi64 DtWMul(sxi64 a,sxi64 b)
{
	return (sxi64)((sxu64)a * (sxu64)b);
}
static void DtFieldsInit(dt_parsed *p,sxi32 iBaseOff)
{
	p->y = p->m = p->d = DT_UNSET;
	p->h = p->i = p->s = p->us = DT_UNSET;
	p->ry = p->rm = p->rd = p->rh = p->ri = p->rs = p->rus = 0;
	p->bHaveDate = p->nTimeTok = 0;
	p->bWday = 0;
	p->iWday = 0;
	p->iWdayBehavior = 0;
	p->iFirstLast = 0;
	p->bWdayOf = 0;
	p->iWdayOfNext = 0;
	p->bWeekdays = 0;
	p->iWeekdays = 0;
	p->iOff = iBaseOff;
	p->bOffSet = 0;
	p->zZone = 0;
	p->nZone = 0;
	p->bZoneIdent = 0;
	p->nZoneTok = 0;
	p->bEpoch = 0;
	p->nWarnPend = 0;
	p->bHaveRel = 0;
	p->bUsUnset = 0;
}
/* php's TIMELIB_UNHAVE_TIME: the clock is ZEROED rather than unset, and the
 * string still counts as carrying no time of its own -- which is why `tomorrow`
 * lands on midnight even through modify(), whose other fields keep the
 * receiver's. */
static void DtUnhaveTime(dt_parsed *p)
{
	p->h = p->i = p->s = p->us = 0;
	p->bUsUnset = 0;
	p->nTimeTok = 0;
}
static void DtCarry(sxi64 *pLo,sxi64 *pHi,sxi64 iUnit)
{
	sxi64 c = DtFloorDiv(*pLo,iUnit);
	*pLo -= c * iUnit;
	*pHi = DtWAdd(*pHi,c);
}
/*
 * php's timelib_do_normalize: carry the clock up into the days, fold the months
 * into the years, then let the civil day count absorb whatever the day field
 * holds. That formula is linear in d, so an out-of-range day simply lands in the
 * month after -- which is php's `2020-01-31 +1 month` == 2020-03-02.
 */
static sxi64 DtDayCountOf(sxi64 y,sxi64 m,sxi64 d)
{
	sxi64 c = DtFloorDiv(m - 1,12);
	sxi64 mm = (m - 1) - c*12 + 1;
	return DtWAdd(DtDaysFromCivil(DtWAdd(y,c),(int)mm,1),d - 1);
}
static void DtNormalize(dt_parsed *p)
{
	sxi64 days,yy;
	int mm,dd;
	DtCarry(&p->us,&p->s,1000000);
	DtCarry(&p->s,&p->i,60);
	DtCarry(&p->i,&p->h,60);
	DtCarry(&p->h,&p->d,24);
	days = DtDayCountOf(p->y,p->m,p->d);
	DtCivilFromDays(days,&yy,&mm,&dd);
	p->y = yy;
	p->m = mm;
	p->d = dd;
}
/* php's day of week, 0 = Sunday, from a day count (1970-01-01 was a Thursday). */
static int DtDowOf(sxi64 days)
{
	return (int)(((days + 4) % 7 + 7) % 7);
}
/*
 * php's do_adjust_for_weekday, which runs BEFORE the relative vector is applied
 * -- so `+30 days next monday` moves to the Monday and then adds the days, in
 * either written order. The three behaviours are php's own: 0 for `next`/`last`
 * (a matching base day is skipped), 1 for a bare name or `this monday` (a
 * matching base day is kept), and 2 for the `... this week` spellings, which
 * count from the WEEK rather than from the day.
 */
static void DtAdjustWeekday(dt_parsed *p)
{
	sxi64 dow = DtDowOf(DtDayCountOf(p->y,p->m,p->d));
	sxi64 wd = p->iWday,diff;
	if( p->iWdayBehavior == 2 ){
		/* php's two corrections: a Sunday base counts as the week's END, and a
		 * Sunday target asked for from any other day is the week's end too. */
		if( dow == 0 && wd != 0 ){ wd -= 7; }
		if( wd == 0 && dow != 0 ){ wd = 7; }
		p->d = p->d - dow + wd;
		return;
	}
	if( wd < 0 ){
		/* php's mirror of the hunt, which only `ago` reaches: it turns the target
		 * weekday negative, and `next monday ago` is the Monday before. */
		sxi64 nwd = -wd;
		p->d = DtWAdd(p->d,-(7 - (nwd - dow)));
		return;
	}
	diff = wd - dow;
	if( (p->rd < 0 && diff < 0) || (p->rd >= 0 && diff <= -p->iWdayBehavior) ){
		diff += 7;
	}
	p->d = DtWAdd(p->d,diff);
}
/*
 * php's `weekday` special: a count of BUSINESS days, which php applies before
 * everything else. Whole fives are whole weeks (the day of the week is kept),
 * the remainder walks past the weekend, and a count that lands on one is pushed
 * off it -- forward to Monday when the count is zero, back to Friday when a
 * positive count ends there.
 */
static void DtAdjustWeekdays(dt_parsed *p)
{
	sxi64 dow = DtDowOf(DtDayCountOf(p->y,p->m,p->d));
	sxi64 count = p->iWeekdays,rem;
	p->d = DtWAdd(p->d,(count / 5) * 7);
	rem = count % 5;
	if( count == 0 ){
		if( dow == 0 ){ p->d += 1; }
		else if( dow == 6 ){ p->d += 2; }
		return;
	}
	if( count > 0 ){
		if( rem == 0 ){
			if( dow == 0 ){ p->d -= 2; }
			else if( dow == 6 ){ p->d -= 1; }
			return;
		}
		if( dow == 6 ){ p->d += 2; rem--; dow = 1; }
		else if( dow == 0 ){ p->d += 1; rem--; dow = 1; }
		if( rem > 0 ){
			if( dow + rem > 5 ){ p->d += 2; }
			p->d += rem;
		}
		return;
	}
	if( rem == 0 ){
		if( dow == 0 ){ p->d += 1; }
		else if( dow == 6 ){ p->d += 2; }
		return;
	}
	if( dow == 0 ){ p->d -= 2; rem++; dow = 5; }
	else if( dow == 6 ){ p->d -= 1; rem++; dow = 5; }
	if( rem < 0 ){
		if( dow + rem < 1 ){ p->d -= 2; }
		p->d += rem;
	}
}
/* php's `first|last day of`: the first is the day 1, the last is day 0 of the
 * month AFTER -- which the normalizer then reads back as the month's own last. */
static void DtFirstLastDay(dt_parsed *p)
{
	if( p->iFirstLast == 1 ){
		p->d = 1;
	}else if( p->iFirstLast == 2 ){
		p->d = 0;
		p->m = DtWAdd(p->m,1);
	}
}
/*
 * php's timelib_fill_holes + timelib_update_ts over a parsed vector: fill what
 * the string left unset from the base moment, then apply in php's ORDER --
 * weekday first, the whole relative vector next, and the `first|last day of`
 * flag LAST, which is why that flag swallows any relative DAYS beside it
 * (`first day of next month +40 days` is the 1st) while the hours still count.
 *
 * DT_PARSE_OVERRIDE_TIME is php's flag of the same name: modify() writes only
 * the fields the string really set, so a bare date there keeps the receiver's
 * time of day, where a fresh parse zeroes it.
 */
#define DT_PARSE_OVERRIDE_TIME 0x01
/* DT_PARSE_KEEP_ZONE is modify()'s other half: php copies the FIELDS the string
 * parsed into the object and nothing else, so a zone the modifier names moves
 * nothing -- `$d->modify('2020-01-01T12:00:00Z')` on a +05:00 date is noon at
 * +05:00 there. The one exception is the `@epoch` form, which names an absolute
 * INSTANT (and, in php, re-zones the object to +00:00 with it). */
#define DT_PARSE_KEEP_ZONE     0x02
/*
 * php's DAY_OF_WEEK_IN_MONTH special -- `first monday of`, `last sunday of
 * february 2020` -- and it is not a rule of its own so much as a way IN to the
 * weekday hunt the vector already carries: the MONTH is settled first (the
 * relative MONTHS are applied and consumed here, though not the years, which
 * ride on to the ordinary pass), the day becomes that month's 1st, and the
 * ordinary hunt walks forward to the weekday from there. The count rides the
 * relative DAYS -- a week per count past the first -- so `tenth tuesday of` is
 * the first one plus nine weeks, and the relative days a string spells beside
 * it simply add on.
 *
 * `last` and `previous` are the same rule one month on with a week taken off
 * (php's own encoding: the 1st of the NEXT month, then -7 from the weekday it
 * finds), and `this` is that month shift with nothing taken off, which is why it
 * answers the FIRST such weekday of the month after.
 */
static void DtWeekdayOfMonth(dt_parsed *p)
{
	p->m = DtWAdd(p->m,DtWAdd(p->rm,(sxi64)p->iWdayOfNext));
	p->rm = 0;
	p->d = 1;
	DtNormalize(p);
}
static sxi64 DtApplyFields(dt_parsed *p,sxi64 iBaseTs,sxi32 iBaseOff,int iBaseUs,
	int iFlags,int *pUs)
{
	sxi64 days = DtFloorDiv(iBaseTs + iBaseOff,86400);
	sxi64 tod  = (iBaseTs + iBaseOff) - days*86400;
	sxi64 by;
	int bm,bd;
	DtCivilFromDays(days,&by,&bm,&bd);
	if( !(iFlags & DT_PARSE_OVERRIDE_TIME) && p->bHaveDate && !p->nTimeTok ){
		p->h = p->i = p->s = p->us = 0;
	}
	if( p->y  == DT_UNSET ){ p->y  = by; }
	if( p->m  == DT_UNSET ){ p->m  = bm; }
	if( p->d  == DT_UNSET ){ p->d  = bd; }
	if( p->h  == DT_UNSET ){ p->h  = tod / 3600; }
	if( p->i  == DT_UNSET ){ p->i  = (tod / 60) % 60; }
	if( p->s  == DT_UNSET ){ p->s  = tod % 60; }
	if( p->us == DT_UNSET ){ p->us = iBaseUs; }
	/* php applies the flag TWICE, and both are visible: once here, so a weekday
	 * hunt and a relative month start from the month's edge (`last monday first
	 * day of this month` never leaves January), and once at the end, which is what
	 * makes it swallow the relative DAYS beside it. */
	if( p->bWdayOf ){
		DtWeekdayOfMonth(p);
	}
	DtFirstLastDay(p);
	DtNormalize(p);
	if( p->bWday ){
		DtAdjustWeekday(p);
		DtNormalize(p);
	}
	p->us = DtWAdd(p->us,p->rus);
	p->s  = DtWAdd(p->s,p->rs);
	p->i  = DtWAdd(p->i,p->ri);
	p->h  = DtWAdd(p->h,p->rh);
	p->d  = DtWAdd(p->d,p->rd);
	p->m  = DtWAdd(p->m,p->rm);
	p->y  = DtWAdd(p->y,p->ry);
	DtFirstLastDay(p);
	/* php's business-day count runs AFTER the relative vector AND after the
	 * `first|last day of` flag, so `last weekday -8 months` walks back from the
	 * month it landed on and `first day of next month +9 weekdays` counts from
	 * that 1st. */
	if( p->bWeekdays ){
		DtNormalize(p);
		DtAdjustWeekdays(p);
	}
	DtNormalize(p);
	*pUs = (int)p->us;
	return DtMakeTs(p->y,(int)p->m,(int)p->d,(int)p->h,(int)p->i,(int)p->s,
		((iFlags & DT_PARSE_KEEP_ZONE) && !p->bEpoch) ? iBaseOff : p->iOff);
}
/*
 * How WIDE a run of digits may be, which php bounds per grammar and PHL did not
 * bound at all -- so a long run silently wrapped the int64 it was accumulated
 * into (`@99999999999999999999` answered 7766279631452241919 here; UBSan called
 * the overflow what it is). Each limit is php's, measured:
 *
 *   an `@epoch`          18 digits, then "Number out of range"
 *   a RELATIVE number    13 digits (php's scanner answers gibberish past that --
 *                        a 14-digit run comes back as ten digits' worth -- so
 *                        PHL refuses instead of guessing, recorded)
 *   an ISO duration      12 digits, then "Unknown or bad format"
 *
 * The accumulators themselves stop adding past DT_DIGITS_SAFE so that COUNTING a
 * run that will be refused cannot overflow on the way.
 */
#define DT_DIGITS_EPOCH 18
#define DT_DIGITS_REL   13
#define DT_DIGITS_ISO   12
#define DT_DIGITS_SAFE  18
/* Whole bands below the position encoding, so one refusal cannot be mistaken for
 * another: the bare negative is php's "Double time specification", a band lower
 * is "Number out of range", one lower still "Double date specification", and the
 * lowest "Double timezone specification". */
#define DT_ERR_RANGE    1000000
#define DT_ERR_DDATE    2000000
#define DT_ERR_DZONE    3000000
#define DT_ERR_TZID     4000000
#define DT_ERR_UNEXPDATA 5000000
#define DT_ERR_EMPTY    6000000
/*
 * php refuses a SECOND absolute date outright -- `2020-01-01 january` and
 * `20240102 20240102` are both "Double date specification" there, reported at the
 * offending token's start. Every date rule marks its answer through this; the bare
 * four-digit YEAR does not, which is why `1234 5678` is a year beside a clock.
 */
static int DtMarkDate(dt_parsed *p,const char *zTok,const char *zIn)
{
	if( p->bHaveDate ){
		return -((int)(zTok - zIn) + 1) - DT_ERR_DDATE;
	}
	p->bHaveDate = 1;
	return 0;
}
/*
 * Read a fractional-seconds part at z (which points at the '.'): up to 6 digits
 * become microseconds (right-padded to 6, extra digits ignored). Advances *pz.
 */
static int DtReadFraction(const char **pz,const char *zEnd)
{
	const char *z = *pz;
	int us = 0,n = 0;
	z++; /* skip '.' */
	while( z < zEnd && SyisDigit(z[0]) ){
		if( n < 6 ){ us = us*10 + (z[0]-'0'); n++; }
		z++;
	}
	while( n < 6 ){ us *= 10; n++; }
	*pz = z;
	return us;
}
/*
 * Read one of php's time-of-day FIELDS at z: one or two digits, greedily -- the
 * two-digit reading is taken when its value is in range and the one-digit reading
 * otherwise, which is what makes `12:60` php's 12:06 with a stray `0` left over
 * (and the error then lands on that `0`, not on the minute). Answers the digits
 * consumed, or 0 when there is no field here.
 */
static int DtReadField(const char *z,const char *zEnd,int iMax,int *pVal)
{
	int v;
	if( z >= zEnd || !SyisDigit(z[0]) ){ return 0; }
	if( z+1 < zEnd && SyisDigit(z[1]) ){
		v = (z[0]-'0')*10 + (z[1]-'0');
		if( v <= iMax ){ *pVal = v; return 2; }
	}
	*pVal = z[0]-'0';
	return 1;
}
/*
 * php's MERIDIAN token, the twelve-hour clock's half: `am` or `pm` in any case,
 * with an optional dot after either letter, and nothing but whitespace or the
 * end of the string behind it -- so `3pm.` is three in the afternoon while
 * `3pm..`, `3pm,` and `3pmx` are not a time at all.
 *
 * Answers the bytes it takes (0 for anything else) and sets *pbPm.
 */
static int DtMeridian(const char *z,const char *zEnd,int *pbPm)
{
	int n,c;
	if( z >= zEnd ){
		return 0;
	}
	c = SyToLower(z[0]);
	if( c != 'a' && c != 'p' ){
		return 0;
	}
	n = 1;
	if( &z[n] < zEnd && z[n] == '.' ){ n++; }
	if( &z[n] >= zEnd || SyToLower(z[n]) != 'm' ){
		return 0;
	}
	n++;
	if( &z[n] < zEnd && z[n] == '.' ){ n++; }
	/* php spells a trailing byte INTO the rule -- `meridian = [AaPp] "."? [Mm]
	 * "."? [\000\t ]` -- and its buffer is NUL-padded, so the end of the string
	 * satisfies it too. Nothing else does: `3pm,`, `3pm\nx` and `3pm.x` are no
	 * meridian at all, and the `am` of `11:30am\nx` is read as a zone. */
	if( &z[n] < zEnd && z[n] != ' ' && z[n] != '\t' && z[n] != 0 ){
		return 0;
	}
	*pbPm = (c == 'p');
	return n;
}
/* The hour a twelve-hour clock means: php's noon is 12 and its midnight is 0,
 * and every other hour is itself or itself plus twelve. */
static int DtHour12(int h,int bPm)
{
	if( h == 12 ){
		return bPm ? 12 : 0;
	}
	return bPm ? h + 12 : h;
}
/*
 * php's time of day: `[t] H[H] (:|.) M[M] [(:|.) S[S] [.frac]]`. Either separator
 * is php's, and only the SECONDS take a fraction -- which is why `12:34.5` is
 * php's 12:34:05 and not a half second. Answers 1 when a time was read (the
 * vector's clock set), 0 when there is none here, and a NEGATIVE DtParse error
 * code -- the only one it can raise is php's "Double time specification".
 */
static int DtReadTimeOfDay(const char **pz,const char *zEnd,const char *zIn,dt_parsed *p)
{
	const char *z = *pz;
	const char *zTok = *pz;   /* the byte a refusal names: this token's first */
	int h,mi,s = 0,n,bT = 0,bPm = 0,nMer,nMin = 0,nSec = 0,bFrac = 0;
	sxi64 uSec = 0;
	char cSep1 = 0,cSep2 = 0;
	if( z < zEnd && (z[0]=='t' || z[0]=='T') ){ z++; bT = 1; }
	if( (n = DtReadField(z,zEnd,24,&h)) == 0 ){ return 0; }
	if( z+n >= zEnd || (z[n] != ':' && z[n] != '.') ){
		/* php's twelve-hour clock with no fields under it: `3pm`, `12 a.m.`.
		 * The hour has to be one a twelve-hour clock can name -- `0am`, `00am`
		 * and `13pm` are refusals, not times -- and the `t` prefix belongs to
		 * the ISO spelling alone, so `t3pm` is the hour 03 with `pm` left over. */
		const char *zMer = &z[n];
		if( bT || h < 1 || h > 12 ){
			return 0;
		}
		while( zMer < zEnd && (zMer[0]==' ' || zMer[0]=='\t') ){ zMer++; }
		if( (nMer = DtMeridian(zMer,zEnd,&bPm)) == 0 ){
			return 0;
		}
		/* php's TIMELIB_HAVE_TIME runs inside the ACTION, so the token is matched
		 * and behind the cursor before the refusal is raised: the walk resumes
		 * past it, not on it. */
		if( p->nTimeTok ){
			*pz = &zMer[nMer];
			return -((int)(zTok - zIn) + 1);
		}
		p->h = DtHour12(h,bPm);
		p->i = p->s = p->us = 0;
		p->bUsUnset = 0;
		p->nTimeTok = 1;
		*pz = &zMer[nMer];
		return 1;
	}
	cSep1 = z[n];
	z += n + 1;
	if( (n = DtReadField(z,zEnd,59,&mi)) == 0 ){ return 0; }
	nMin = n;
	z += n;
	if( z < zEnd && (z[0]==':' || z[0]=='.') && z+1 < zEnd && SyisDigit(z[1]) ){
		cSep2 = z[0];
		n = DtReadField(&z[1],zEnd,60,&s);
		nSec = n;
		z += n + 1;
		if( z < zEnd && z[0]=='.' && z+1 < zEnd && SyisDigit(z[1]) ){
			bFrac = 1;
			uSec = DtReadFraction(&z,zEnd);
		}
	}
	/* ...and the twelve-hour half of the same clock, which php spells behind the
	 * fields: `3:04pm`, `3:04:05 a.m.`. It is only a meridian when the hour is
	 * one a twelve-hour clock names, so `13:00pm` keeps its 13 and leaves the
	 * `pm` to the string, which then reads it as an unknown zone. */
	/* ...and php spells the LAST field of a twelve-hour clock with both its
	 * digits: `3:04pm` and `3:4:05pm` are times where `3:4pm` and `3:04:5pm`
	 * are not, and the `pm` those two leave behind is an unknown zone.
	 *
	 * A FRACTION narrows the shape to php's one spelling of it: both separators
	 * are colons, both fields carry both digits, and the meridian follows the
	 * fraction with nothing between them -- `3:04:05.5pm` is a time and
	 * `3:04:05.5 pm` is not. */
	if( h >= 1 && h <= 12 && !bT
	 && (bFrac ? (nMin == 2 && nSec == 2 && cSep1 == ':' && cSep2 == ':')
	           : ((nSec > 0 ? nSec : nMin) == 2)) ){
		const char *zMer = z;
		if( !bFrac ){
			while( zMer < zEnd && (zMer[0]==' ' || zMer[0]=='\t') ){ zMer++; }
		}
		if( (nMer = DtMeridian(zMer,zEnd,&bPm)) > 0 ){
			h = DtHour12(h,bPm);
			z = &zMer[nMer];
		}
	}
	/* php's "Double time specification". Its TIMELIB_HAVE_TIME sits in the
	 * action, so the whole token -- meridian and fraction included -- has been
	 * matched and the cursor is past it before the refusal is raised; the byte it
	 * names is still the token's first. Nothing is written: the clock a second
	 * time token would set is not php's answer either. */
	if( p->nTimeTok ){
		*pz = z;
		return -((int)(zTok - zIn) + 1);
	}
	/* A time of day sets the whole clock, sub-second included: php writes the
	 * microseconds of a time WITHOUT a fraction as zero. */
	p->h = h;
	p->i = mi;
	p->s = s;
	p->us = uSec;
	p->bUsUnset = 0;
	p->nTimeTok = 1;
	*pz = z;
	return 1;
}
/*
 * The zone a string NAMED, recorded once: php reads a timezone token wherever it
 * stands and the FIRST one wins outright, silently -- `+0200 +0300` is +02:00,
 * `UTC GMT` is UTC and `2020-01-01T12:00:00Z +0300` keeps its `Z`. Every door
 * that reads a zone (the attached ISO offset, a trailing name, `@epoch`'s UTC and
 * the standalone token below) goes through here, so the rule is one line.
 *
 * A THIRD one is php's refusal, though: it counts the tokens and raises "Double
 * timezone specification" on the one past the ignored second, which is what makes
 * `-123-03-04` -- three offsets to php's scanner, and no date at all -- an error
 * at its last `-`. Answers 1 for that, 0 otherwise.
 *
 * zName NULL means a fixed OFFSET, whose name php builds from the offset itself.
 */
static int DtZoneCount(dt_parsed *p)
{
	int n = p->nZoneTok;
	if( n < 2 ){
		p->nZoneTok = n + 1;
	}
	return n == 0 ? 0 : (n == 1 ? 1 : -1);
}
/* ...and the VALUE, written only for the token the rule above accepted. */
static void DtZoneStore(dt_parsed *p,sxi32 iOff,const char *zName,int nName,int bIdent)
{
	p->iOff = iOff;
	p->bOffSet = zName ? 2 : 1;
	p->zZone = zName;
	p->nZone = nName;
	p->bZoneIdent = bIdent;
}
/* Exactly two digits whose value is <= iMax -- php's `minutelz`/`secondlz`, and
 * the hour of its two-colon spelling. */
static int DtZoneLz(const char *z,const char *zEnd,int iMax)
{
	return zEnd-z >= 2 && SyisDigit(z[0]) && SyisDigit(z[1])
		&& (z[0]-'0')*10 + (z[1]-'0') <= iMax;
}
/* php's `hour24` (<= 24) and `minute` (<= 59) fields: one digit, or two when the
 * two-digit reading is in range -- so `96` is the hour 9 with a `6` left over and
 * `24` is the hour 24. Answers the digits taken. */
static int DtZoneField(const char *z,const char *zEnd,int iMax)
{
	if( z >= zEnd || !SyisDigit(z[0]) ){
		return 0;
	}
	if( z+1 < zEnd && SyisDigit(z[1]) && (z[0]-'0')*10 + (z[1]-'0') <= iMax ){
		return 2;
	}
	return 1;
}
/*
 * How many bytes of digits and colons after the sign belong to php's UTC-offset
 * token. php's scanner takes the LONGEST of three spellings and leaves the rest
 * of the run to the string, which is why `+2460` is +02:46 with a `0` left over
 * and `+9999` is +99:00 with `99`:
 *
 *   HH:MM:SS   two colons, two digits everywhere, hours <= 24 and seconds <= 60
 *   HHMMSS     six digits, the same three bounds
 *   H[H] [:] M[M]    the hour alone (0-99 when nothing follows it), or an hour
 *                    <= 24 and a minute <= 59, the colon optional
 *
 * The VALUE is not read here: php computes it from the byte COUNT afterwards
 * (DtZoneOffsetDigits), and the two disagree on purpose -- `+099` matches as the
 * hour `09` and the minute `9`, then counts as three digits and answers 0h99m.
 */
static int DtZoneCorrLen(const char *z,const char *zEnd)
{
	int nH,nM;
	const char *zm;
	if( zEnd-z >= 8 && z[2] == ':' && z[5] == ':'
	 && DtZoneLz(z,zEnd,24) && DtZoneLz(&z[3],zEnd,59) && DtZoneLz(&z[6],zEnd,60) ){
		return 8;
	}
	if( DtZoneLz(z,zEnd,24) && DtZoneLz(&z[2],zEnd,59) && DtZoneLz(&z[4],zEnd,60) ){
		return 6;
	}
	if( (nH = DtZoneField(z,zEnd,24)) == 0 ){
		return 0;
	}
	zm = &z[nH];
	if( zm < zEnd && zm[0] == ':' ){ zm++; }
	if( (nM = DtZoneField(zm,zEnd,59)) != 0 ){
		return (int)(zm - z) + nM;
	}
	return nH;
}
/*
 * php's MILITARY zones: a single LETTER is a whole-hour offset -- `A`..`I` are
 * +1..+9, `K`..`M` +10..+12 and `N`..`Y` -1..-12, with `Z` the zero ISO 8601
 * spells and no `J` at all. php names one by its UPPERCASE letter whatever case
 * it was written in, and calls it an ABBREVIATION; no tz database is involved,
 * which is why this engine can answer the whole set exactly. Answers 1 and fills
 * the offset and the name (a static literal, as every stored zone name here is),
 * or 0 for `J` and for anything that is not a letter.
 */
static int DtZoneMil(int c,sxi32 *piOff,const char **pzName)
{
	static const char zLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;
	if( c >= 'a' && c <= 'z' ){
		c -= 'a' - 'A';
	}
	if( c < 'A' || c > 'Z' || c == 'J' ){
		return 0;
	}
	i = c - 'A';
	*pzName = &zLetters[i];
	if( c == 'Z' ){
		*piOff = 0;
	}else if( c < 'J' ){
		*piOff = (sxi32)(i + 1) * 3600;    /* A..I: +1..+9 */
	}else if( c <= 'M' ){
		*piOff = (sxi32)i * 3600;          /* K..M: +10..+12 (the missing J shifts them) */
	}else{
		*piOff = -(sxi32)(i - 12) * 3600;  /* N..Y: -1..-12 */
	}
	return 1;
}
/*
 * php's longest match, seen from a WORD's side. Every rule spelled in letters
 * competes with the TIMEZONE token, which reads at most SIX of them
 * (DtZoneShape), so a keyword wins only when it is at least as long as that
 * read -- which is to say when it runs to the end of the letter run, or is six
 * letters itself and ties (a tie goes to whichever rule timelib spells first,
 * and the zone is its last). `nowx`, `janx` and `todayx` are unknown zones;
 * `januaryx`, `tomorrowx` and `augustx` are the word with a military zone
 * behind it.
 *
 * The rules that are only ever PART of a longer one -- the ordinal and
 * navigation words, which need a unit or a weekday after them -- do not get
 * this: `previousx month` is a zone in php, because `previous` alone is not a
 * token there at all.
 */
static int DtWordEnds(const char *z,const char *zEnd,int nKw)
{
	return nKw >= 6 || &z[nKw] >= zEnd || !SyisAlpha((unsigned char)z[nKw]);
}
/*
 * php's timezone token by SHAPE. Its scanner matches the re2c rule and asks what
 * the letters SPELL only afterwards, which is why `Z,tues` reports a double
 * timezone at the `tues` rather than a name the database does not have -- and
 * why an unknown word is a token that the string reads PAST rather than a byte
 * it stops on. The rule is two alternatives and, as every re2c rule does, the
 * longer of the two wins:
 *
 *   "("? [A-Za-z]{1,6} ")"?          the abbreviation -- each paren optional on
 *                                    its own, so `(abc` and `abc)` both match
 *   [A-Z][a-z]+([_/-][A-Za-z]+)+     the tz-database identifier, `Europe/Paris`
 *
 * The SIX-letter cap on the first is what every other word-shaped rule competes
 * against (DtWordEnds): `janx` is an unknown zone where `januaryx` is January
 * beside the military zone X.
 *
 * Answers the bytes the token takes and reports the letters inside it, the
 * parens dropped.
 */
static int DtZoneShape(const char *z,const char *zEnd,const char **pzName,int *pnName)
{
	int nPar = 0,nLet = 0,nBare = 0,nId = 0;
	if( z < zEnd && z[0] == '(' ){
		nPar = 1;
	}
	while( nLet < 6 && &z[nPar+nLet] < zEnd && SyisAlpha((unsigned char)z[nPar+nLet]) ){
		nLet++;
	}
	/* ...except a lone `t` with a digit behind it, which php's clock reads
	 * LONGER than any zone: `t9` is nine in the morning, not the military zone T
	 * with a stray digit, and `12:00t9` is php's second time specification. */
	if( nPar == 0 && nLet == 1 && (z[0] == 't' || z[0] == 'T')
	 && &z[1] < zEnd && SyisDigit(z[1]) ){
		nLet = 0;
	}
	if( nLet > 0 ){
		nBare = nPar + nLet;
		if( &z[nBare] < zEnd && z[nBare] == ')' ){
			nBare++;
		}
	}
	if( z < zEnd && z[0] >= 'A' && z[0] <= 'Z' ){
		int k = 1,nSeg = 0;
		while( &z[k] < zEnd && z[k] >= 'a' && z[k] <= 'z' ){ k++; }
		if( k > 1 ){
			for(;;){
				int j = k;
				if( &z[j] >= zEnd || (z[j] != '_' && z[j] != '/' && z[j] != '-') ){
					break;
				}
				j++;
				if( &z[j] >= zEnd || !SyisAlpha((unsigned char)z[j]) ){
					break;
				}
				while( &z[j] < zEnd && SyisAlpha((unsigned char)z[j]) ){ j++; }
				k = j;
				nSeg++;
			}
			if( nSeg > 0 ){
				nId = k;
			}
		}
	}
	if( nId > nBare ){
		*pzName = z;
		*pnName = nId;
		return nId;
	}
	if( nBare == 0 ){
		return 0;
	}
	*pzName = &z[nPar];
	*pnName = nLet;
	return nBare;
}
/* php's timezone_type -- 1 = a fixed UTC OFFSET, 2 = an ABBREVIATION, 3 = an
 * IDENTIFIER. Both parsers answer in these, so they stand above both. */
#define DT_ZONE_OFFSET 1
#define DT_ZONE_ABBR   2
#define DT_ZONE_ID     3

/*
 * ---------------------------------------------------------------------------
 * The tz DATABASE, seen from the date family.
 *
 * Every fixed spelling this engine has always understood keeps its own path:
 * the ±HH:MM:SS grammar, php's military letters, `GMT`, `Z` and `UTC` are
 * decided before any of this is asked, so a build with PH7_ENABLE_TZDB off and
 * one with it on answer those identically. The database can only ADD names.
 *
 * A zone is a DATABASE zone when its name resolves in the table AND its
 * timezone_type is 3 -- the identifier kind. `UTC` is deliberately excluded
 * even though the table holds it: it is already a fixed zone here, and routing
 * it through the table would move a well-tested answer for no gain.
 *
 * The index is not STORED anywhere. It is re-derived from the name whenever it
 * is needed, because the name is the object's state and a serialized date has
 * to come back the same way. The lookup is a binary search over 599 rows.
 */
static int DtTzIndex(const char *zName,int nName,int iZoneKind)
{
#ifdef PH7_ENABLE_TZDB
	if( iZoneKind != DT_ZONE_ID || zName == 0 || nName < 1 ){
		return -1;
	}
	if( nName == 3 && SyStrnicmp(zName,"UTC",3) == 0 ){
		return -1;
	}
	return PH7_TzFind(zName,nName);
#else
	SXUNUSED(zName);
	SXUNUSED(nName);
	SXUNUSED(iZoneKind);
	return -1;
#endif
}
/*
 * The offset a zone is on at iTs. iTz is a database index or -1, and -1 means
 * "the fixed offset the caller already has", which is every pre-database zone.
 * pzAbbr/pnAbbr come back 0/0 for a fixed zone -- the marker DateFormat's `T`
 * reads as "use the old rule".
 */
static sxi32 DtTzOffsetAt(int iTz,sxi32 iFixed,sxi64 iTs,int *pbDst,
	const char **pzAbbr,int *pnAbbr)
{
#ifdef PH7_ENABLE_TZDB
	if( iTz >= 0 ){
		sxi32 iOff = iFixed;
		if( PH7_TzOffsetAt(iTz,iTs,&iOff,pbDst,pzAbbr,pnAbbr) ){
			return iOff;
		}
	}
#else
	SXUNUSED(iTz);
	SXUNUSED(iTs);
#endif
	*pbDst = 0;
	*pzAbbr = 0;
	*pnAbbr = 0;
	return iFixed;
}
/*
 * ...and what the letters spell, the spellings this engine has without a tz
 * database: `UTC` (an IDENTIFIER in that exact case, an abbreviation in any
 * other), `GMT`, and the military letters above. Answers 1 when the name is one
 * of them, 0 for every other shape php would look up and this build cannot.
 */
static int DtZoneName(const char *z,int n,sxi32 *piOff,const char **pzName,
	int *pnName,int *pbIdent)
{
	if( n == 3 && (SyStrnicmp(z,"utc",3) == 0 || SyStrnicmp(z,"gmt",3) == 0) ){
		int bUtc = (z[0] == 'u' || z[0] == 'U');
		*piOff = 0;
		*pzName = bUtc ? "UTC" : "GMT";
		*pnName = 3;
		*pbIdent = (bUtc && SyMemcmp(z,"UTC",3) == 0);
		return 1;
	}
	if( n == 1 && DtZoneMil(z[0],piOff,pzName) ){
		*pnName = 1;
		*pbIdent = 0;
		return 1;
	}
	return 0;
}
/* Forward: the offset's VALUE is the one DateTimeZone reads too (the door that
 * takes a whole string rather than a token), so both spellings share it. */
static int DtZoneOffsetDigits(const char *z,int n,sxi32 *piOff,int *pnUsed);
/*
 * A SIGNED offset at z, the whole token: the sign, then the digits and colons
 * DtZoneCorrLen claims. Answers the bytes taken (0 when this is not one).
 */
static int DtZoneCorr(const char *z,const char *zEnd,sxi32 *piOff)
{
	int n,nUsed = 0;
	if( zEnd-z < 2 || (z[0] != '+' && z[0] != '-') ){
		return 0;
	}
	if( (n = DtZoneCorrLen(&z[1],zEnd)) == 0
	 || DtZoneOffsetDigits(&z[1],n,piOff,&nUsed) != 0 ){
		return 0;
	}
	if( z[0] == '-' ){
		*piOff = -*piOff;
	}
	return n + 1;
}
/*
 * php's standalone TIMEZONE token, which its scanner takes anywhere in a date
 * string: a name, a name inside PARENTHESES (`2020-01-01 (UTC)`), or a UTC
 * offset with an optional uppercase `GMT` in front of it (`GMT+02:00`; the
 * lowercase spelling is the ABBREVIATION `gmt` with a relative number after it).
 * Advances *pz over what it took and answers 1, answers 0 leaving *pz alone, or
 * answers php's "Double timezone specification" in DtParse's own convention.
 */
static int DtZoneTok(const char **pz,const char *zEnd,dt_parsed *p,const char *zIn)
{
	const char *z = *pz;
	const char *zName = 0;
	int nName = 0,bIdent = 0,n = 0,nTok = 0,bKnown = 0,rc;
	sxi32 iOff = 0;
	/* The `GMT` in front of an offset is read before the NAME of the same three
	 * bytes, because php's scanner takes the longer token -- and only when a whole
	 * offset follows it, which is what makes `GMT+02:00` the offset, `gmt+2` the
	 * zone GMT with a stray relative number after it, and `GMT+` the zone GMT with
	 * a refusal ON the sign. */
	if( zEnd-z > 3 && SyMemcmp(z,"GMT",3) == 0
	 && (n = DtZoneCorr(&z[3],zEnd,&iOff)) != 0 ){
		zName = 0;
		nTok = n + 3;
		bKnown = 1;
	}
	else if( (n = DtZoneShape(z,zEnd,&zName,&nName)) != 0 ){
		nTok = n;
		bKnown = DtZoneName(zName,nName,&iOff,&zName,&nName,&bIdent);
	}
	else if( (n = DtZoneCorr(z,zEnd,&iOff)) != 0 ){
		zName = 0;
		nTok = n;
		bKnown = 1;
	}
	if( nTok == 0 ){
		return 0;
	}
	/* php's TIMELIB_HAVE_TZ runs BEFORE the lookup, so only the string's FIRST
	 * zone token is ever asked what it spells: a second is dropped whatever it
	 * says, a third is the refusal, and neither is reported as a name the
	 * database does not have. The token is consumed either way. */
	*pz = &z[nTok];
	rc = DtZoneCount(p);
	if( rc < 0 ){
		return -((int)(z - zIn) + 1) - DT_ERR_DZONE;
	}
	if( rc > 0 ){
		/* php's TIMELIB_HAVE_TZ warns on the SECOND and refuses only the third */
		DtWarnPend(p,(int)(z - zIn),"Double timezone specification");
	}
	if( rc == 0 ){
		if( !bKnown ){
			return -((int)(z - zIn) + 1) - DT_ERR_TZID;
		}
		DtZoneStore(p,iOff,zName,nName,bIdent);
	}
	return 1;
}
/* Forward: the time SUFFIX has to know whether a DATE would read longer at the
 * same position, and the date rules read a time suffix of their own. */
static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,
	dt_parsed *p,const char *zIn);
/* True if php's `hour24 [:.] minute` reads here -- the head of every clock its
 * combined date-and-time rules end in, and the reason a month, a day and a clock
 * are ONE token there: it reads longer than the YEAR the same digits would be. */
static int DtClockFollows(const char *z,const char *zEnd)
{
	int h,i,n;
	if( z >= zEnd || !SyisDigit(z[0]) ){
		return 0;
	}
	h = z[0] - '0';
	n = 1;
	if( &z[1] < zEnd && SyisDigit(z[1]) && (z[0]-'0')*10 + (z[1]-'0') <= 24 ){
		h = (z[0]-'0')*10 + (z[1]-'0');
		n = 2;
	}
	if( h > 24 || &z[n] >= zEnd || (z[n] != ':' && z[n] != '.') || &z[n+1] >= zEnd
	 || !SyisDigit(z[n+1]) ){
		return 0;
	}
	i = z[n+1] - '0';
	if( &z[n+2] < zEnd && SyisDigit(z[n+2]) ){
		i = i*10 + (z[n+2]-'0');
	}
	return i <= 59;
}
/* True if z points at a two-letter English ordinal suffix (st/nd/rd/th). */
static int DtIsOrdinal(const char *z,const char *zEnd)
{
	if( zEnd - z < 2 ){ return 0; }
	return SyStrnicmp(z,"st",2) == 0 || SyStrnicmp(z,"nd",2) == 0
		|| SyStrnicmp(z,"rd",2) == 0 || SyStrnicmp(z,"th",2) == 0;
}
/*
 * Parse an OPTIONAL time-of-day suffix after a date component: a space or `T`,
 * then php's time of day, then a `Z` or a UTC offset. On entry *pz points just
 * past the date. Advances *pz over whatever it consumes. Returns 0 on success
 * (whether or not a time was present), or a 1-based error position into zIn
 * (negative encodes php's "Double time specification"). Shared by every
 * absolute-date branch.
 */
static int DtTimeSuffix(const char **pz,const char *zEnd,const char *zIn,dt_parsed *p)
{
	const char *z = *pz;
	if( z < zEnd && (z[0]=='T' || z[0]==' ' || z[0]=='.') && z+1 < zEnd && SyisDigit(z[1]) ){
		const char *zTime = &z[1];
		int rc;
		{
			/* php reads whichever token is LONGER at this position, and a dotted
			 * DATE is longer than the clock hiding in its head: the tail of
			 * `01/02/2020 03.04.2021` is a second date (its refusal), not 03:04:20.
			 * The probe runs on a copy, and with the date flag cleared so that the
			 * refusal this call would raise cannot answer the question. */
			dt_parsed sTry = *p;
			const char *zProbe = zTime;
			sTry.bHaveDate = 0;
			if( DtTryNumericDate(zTime,zEnd,&zProbe,&sTry,zIn) == 1 ){
				*pz = z;
				return 0;
			}
		}
		rc = DtReadTimeOfDay(&zTime,zEnd,zIn,p);
		if( rc < 0 ){ *pz = zTime; return rc; }
		if( rc == 0 ){
			*pz = z;
			return 0;
		}
		z = zTime;
		/* The zone ATTACHED to the time is php's `iso8601normtz`: a `Z` or a
		 * numeric offset, whose seconds `...T12:00:00+02:00:30` reads too. A
		 * NAME is not part of this token -- it is one of the string's own, which
		 * is what leaves the `this` of `24:00:00this week` to the relative rule
		 * that reads it longer. */
		if( z < zEnd && (z[0] == 'Z' || z[0] == 'z')
		 && !(z+1 < zEnd && SyisAlpha((unsigned char)z[1])) ){
			if( DtZoneCount(p) < 0 ){
				*pz = &z[1];
				return -((int)(z - zIn) + 1) - DT_ERR_DZONE;
			}
			if( p->nZoneTok == 1 ){
				DtZoneStore(p,0,"Z",1,0);
			}else{
				DtWarnPend(p,(int)(z - zIn),"Double timezone specification");
			}
			z++;
		}else{
			sxi32 iOffTz = 0;
			int nTz = DtZoneCorr(z,zEnd,&iOffTz);
			if( nTz > 0 ){
				int rcZ = DtZoneCount(p);
				const char *zTz = z;
				z += nTz;
				if( rcZ < 0 ){
					*pz = z;
					return -((int)(zTz - zIn) + 1) - DT_ERR_DZONE;
				}
				if( rcZ == 0 ){
					DtZoneStore(p,iOffTz,0,0,0);
				}else{
					DtWarnPend(p,(int)(zTz - zIn),"Double timezone specification");
				}
			}
		}
	}
	*pz = z;
	return 0;
}
/*
 * Read one or two decimal digits at z (z<zEnd guaranteed by caller for the first).
 * Returns the value; *pn = digits consumed (1 or 2).
 */
static int DtRead1or2(const char *z,const char *zEnd,int *pn)
{
	int v = z[0]-'0';
	if( z+1 < zEnd && SyisDigit(z[1]) ){ v = v*10 + (z[1]-'0'); *pn = 2; }
	else { *pn = 1; }
	return v;
}
/*
 * The YEAR of php's ISO date, at the head of a string: four digits, or php's
 * EXPANDED form -- a sign in front of AT LEAST four digits, with no upper width
 * (`-1234-03-04`, `+12345-01-01`, `-123456789-01-01`). The sign is what admits
 * the extra digits: an unsigned five-digit run is not a date to php at all, and
 * a signed run shorter than four is not one either (`-123-03-04` fails there).
 *
 * Answers the bytes the year occupies -- the caller finds the '-' that closes it
 * at that offset -- or 0 when the head is not one. A magnitude past the int64
 * ceiling SATURATES there instead of overflowing; php's own answer past that
 * point is garbage of its own (a 20-digit year reads back as 1999 there), so
 * nothing pins that corner -- only the absence of undefined behaviour.
 */
static int DtTryIsoYear(const char *z,const char *zEnd,sxi64 *pY,int *pbRange)
{
	static const sxu64 iCeil = (sxu64)0x7FFFFFFFFFFFFFFF;
	int nSign = (z < zEnd && (z[0] == '+' || z[0] == '-')) ? 1 : 0;
	const char *zDig = &z[nSign];
	const char *zScan = zDig;
	sxu64 y = 0;
	int bOver = 0;
	/* php's own ceiling for the field: the magnitude an int64 holds, which is one
	 * larger on the negative side. */
	sxu64 iMax = (nSign && z[0] == '-') ? iCeil + 1 : iCeil;
	while( zScan < zEnd && SyisDigit(zScan[0]) ){
		sxu64 dig = (sxu64)(zScan[0] - '0');
		if( bOver || y > (iMax - dig) / 10 ){
			bOver = 1;
			y = iMax;
		}else{
			y = y * 10 + dig;
		}
		zScan++;
	}
	if( zScan - zDig < 4 || (nSign == 0 && zScan - zDig != 4) ){
		return 0;
	}
	if( zScan >= zEnd || zScan[0] != '-' ){
		return 0;
	}
	if( nSign && zScan - zDig > 19 ){
		/* php's EXPANDED year is at most nineteen digits; a wider run is not this
		 * token at all and the string re-reads it with whatever else fits. */
		return 0;
	}
	/* ...and one that no int64 holds is php's own refusal -- but only once the
	 * REST of the token has matched too, so the caller is told rather than
	 * answered: `+9296228446195592075-1-1` is no expanded date at all there and
	 * reports the byte its re-reading trips on instead. */
	*pbRange = bOver;
	/* the negative bound IS the int64's own, so the sign is applied in UNSIGNED
	 * arithmetic: negating -9223372036854775808 as a signed value is undefined
	 * and this build gates on UBSan. */
	*pY = (nSign && z[0] == '-') ? (sxi64)((sxu64)0 - y) : (sxi64)y;
	return (int)(zScan - z);
}
/*
 * php's ISO WEEK DATE, the spelling ISO 8601 gives a week rather than a day:
 * `YYYY[-]Www` and `YYYY[-]Www[-]D`, with the year exactly four digits and
 * unsigned, the week exactly two and inside 01..53, and the day ONE digit
 * inside 0..7. Everything outside that is not this token at all, which is why
 * `2020-W54`, `2020-W5` and `2020-w05` refuse where the loop runs out of rules
 * rather than here -- and why `2020-W05-8` is the week alone with `-8` left
 * standing as a zone OFFSET, which is the answer php gives it.
 *
 * php does not resolve the week to a calendar date: timelib writes the year
 * with January 1st and puts the whole distance on the RELATIVE day count, which
 * is what `date_parse('2020-W05')` shows as `day => 26`. The distance runs from
 * that January 1st to the Monday of week 1 -- the week holding the 4th -- plus
 * a week per week and a day per day.
 *
 * Returns 0 when the text is not one (caller falls through), 1 on success, or
 * an error code in DtParse's own convention.
 */
static int DtTryIsoWeek(const char *z,const char *zEnd,const char **pzOut,
	dt_parsed *p,const char *zIn)
{
	const char *zTok = z;
	sxi64 y = 0;
	int i,w,iDow = 1,dow1,rcT;
	/* the shortest spelling is the compact `2020W05` */
	if( zEnd - z < 7 ){
		return 0;
	}
	for( i = 0 ; i < 4 ; i++ ){
		if( !SyisDigit(z[i]) ){
			return 0;
		}
		y = y*10 + (z[i] - '0');
	}
	if( z[i] == '-' ){
		i++;
	}
	if( &z[i+2] >= zEnd || z[i] != 'W' ){
		return 0;
	}
	i++;
	if( !SyisDigit(z[i]) || !SyisDigit(z[i+1]) ){
		return 0;
	}
	w = (z[i]-'0')*10 + (z[i+1]-'0');
	i += 2;
	if( w < 1 || w > 53 ){
		return 0;
	}
	{
		/* the day, with its own separator: a digit past 7 belongs to whatever
		 * follows the token, its sign included */
		int j = i;
		if( &z[j] < zEnd && z[j] == '-' ){
			j++;
		}
		if( &z[j] < zEnd && z[j] >= '0' && z[j] <= '7' ){
			iDow = z[j] - '0';
			i = j + 1;
		}
	}
	z = &z[i];
	*pzOut = z;
	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }
	/* php's weekday numbering here is 0 = Sunday, and week 1 is the one whose
	 * Monday is at most three days after New Year's Day. */
	dow1 = DtDowOf(DtDaysFromCivil(y,1,1));
	p->y = y;
	p->m = 1;
	p->d = 1;
	/* php ASSIGNS that count rather than adding to it, the way `tomorrow` and
	 * `yesterday` do -- so a `+1 week` written BEFORE the week date is discarded
	 * by it (`+1 week 2020-W05` is the week's own Monday) while one written after
	 * moves on from it. */
	p->bHaveRel = 1;
	p->rd = (sxi64)(1 - (dow1 > 4 ? dow1 - 7 : dow1) + (w - 1)*7 + (iDow - 1));
	*pzOut = z;
	return 1;
}
/*
 * Try to read php's ISO date at z: [+-]YYYY-MM-DD plus an optional time suffix.
 * Returns 0 when the text is not one (caller falls through), 1 on success, or an
 * error code in DtParse's own convention.
 */
static int DtTryIsoDate(const char *z,const char *zEnd,const char **pzOut,
	dt_parsed *p,const char *zIn)
{
	const char *zRest;
	const char *zTok = z;
	sxi64 y = 0;
	int nYr,mo = 0,d = 0,rcT,bRange = 0,bFull;
	if( (nYr = DtTryIsoYear(z,zEnd,&y,&bRange)) == 0 ){
		return 0;
	}
	zRest = &z[nYr];   /* the '-' that closed the year */
	bFull = !(zEnd-z < nYr + 6
	 || !SyisDigit(zRest[1])||!SyisDigit(zRest[2])||zRest[3] != '-'
	 ||!SyisDigit(zRest[4])||!SyisDigit(zRest[5]));
	if( bFull ){
		mo = (zRest[1]-'0')*10 + (zRest[2]-'0');
		d  = (zRest[4]-'0')*10 + (zRest[5]-'0');
		/* php spells the ranges INSIDE the pattern, so a month past 12 or a day
		 * past 31 means this spelling never matched at all -- the shorter rule
		 * behind it did, and the digits it did not take are left to the string.
		 * `2020-13-45` is January the 1st of 2020 with a refusal on its `3`, not
		 * a refusal and no date; the difference is invisible in what either
		 * engine THROWS and plain in what it collects, because a date already
		 * read is what makes the next one a `Double date specification`.
		 * ("00" lexes fine and normalizes: month 0 is December of the year
		 * before, which the field normalizer does on its own.) */
		if( mo > 12 || d > 31 ){
			bFull = 0;
		}
	}
	if( !bFull ){
		/* Not the full spelling. Two SHORTER ones stand behind it, both php's and
		 * both only after a plain four-digit year (`+12345-01` is neither): the
		 * YEAR-MONTH `2020-01`, whose day is the 1st, and the ISO ORDINAL
		 * `2020-102`, whose three digits are the day of the YEAR. At most three
		 * digits belong to either, and whatever is left of the run is the string's
		 * -- as is the whole token when the DAY-FIRST numeric rule (`2020-1-1`),
		 * php's own separate one, reads longer here.
		 *
		 * Anything else hands the text on rather than refusing: the position this
		 * would report is the TOKEN's, and a token at position 0 encodes as the 1
		 * that means "matched", which spun the parse loop forever. */
		int nd = 0;
		while( &zRest[1+nd] < zEnd && SyisDigit(zRest[1+nd]) ){ nd++; }
		if( nYr != 4 || !SyisDigit(z[0]) || nd == 0 ){
			return 0;
		}
		if( nd > 3 ){ nd = 3; }
		{
			/* The DAY-FIRST rule reads `2020-1-1` whole, which is LONGER than the
			 * year-month reading of its head -- php's scanner takes the longer
			 * token, so let it. The probe runs on a copy with the date flag
			 * cleared, so a refusal it would raise cannot answer the question. */
			dt_parsed sTry = *p;
			const char *zProbe = zTok;
			int rcP;
			sTry.bHaveDate = 0;
			rcP = DtTryNumericDate(zTok,zEnd,&zProbe,&sTry,zIn);
			if( rcP == 1 && zProbe > &zRest[1+nd] ){
				return 0;
			}
			/* The longer token matched and its own field check REFUSED it, which
			 * is php's answer for the whole string -- `3854-2-40` is a day out of
			 * range there, not the year-month `3854-2` with `-40` behind it. */
			if( rcP != 0 && rcP != 1 ){
				*pzOut = zProbe;
				return rcP;
			}
		}
		/* The LONGEST reading that validates wins and the rest of the run is left
		 * to the string, which is where php's refusals for this shape really come
		 * from: `2020-13` is the month 1 with a stray `3` after it (its "position
		 * 6"), and `1526-797-45` the month 7 with `97-45` left over. */
		{
			int doy = nd == 3 ? (zRest[1]-'0')*100 + (zRest[2]-'0')*10 + (zRest[3]-'0') : 0;
			int mo2 = nd >= 2 ? (zRest[1]-'0')*10 + (zRest[2]-'0') : 99;
			if( nd == 3 && doy >= 1 && doy <= 366 ){
				mo = 1;
				d = doy;   /* the field normalizer resolves it out of January */
			}else if( nd >= 2 && mo2 <= 12 ){
				nd = 2;
				mo = mo2;
				d = 1;
			}else{
				nd = 1;
				mo = zRest[1]-'0';
				d = 1;
			}
		}
		z = &zRest[1+nd];
		*pzOut = z;
		if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }
		p->y = y;
		p->m = mo;
		p->d = d;
		if( (rcT = DtTimeSuffix(&z,zEnd,zIn,p)) != 0 ){
			*pzOut = z;
			return rcT;
		}
		*pzOut = z;
		return 1;
	}
	if( bRange ){
		/* the whole `[+-]YYYY-MM-DD` matched and its year is past the int64 the
		 * field is kept in: php's "Number out of range", at the sign */
		*pzOut = &zRest[6];
		return -((int)(zTok - zIn) + 1) - DT_ERR_RANGE;
	}
	z = &zRest[6];
	/* php's DAY carries an optional ordinal suffix wherever a day stands, this
	 * spelling included: `2020-01-02nd` is the 2nd. Only behind a plain
	 * four-digit year, though -- the EXPANDED form is a rule of its own, and
	 * `+12345-01-02nd` leaves the `nd` to the string as an unknown zone. */
	if( nYr == 4 && DtIsOrdinal(z,zEnd) ){
		z += 2;
	}
	*pzOut = z;
	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }
	p->y = y;
	p->m = mo;
	p->d = d;
	if( (rcT = DtTimeSuffix(&z,zEnd,zIn,p)) != 0 ){
		*pzOut = z;
		return rcT;
	}
	*pzOut = z;
	return 1;
}
/*
 * php's ISO ORDINAL date spelled with a FULL STOP, `YYYY.DDD` -- the same date
 * `2020-102` gives, and the ONLY dotted form a four-digit year takes: `2020.1`
 * and `2020.12` are no date at all there (the four digits are a clock and the
 * rest is refused), the run is exactly three digits inside 001..366, the sign
 * belongs to a rule of its own (`+2020.102` is not one), and a time suffix may
 * follow.
 *
 * It matters beyond its own spelling, because php's full stop between two digit
 * runs is an ordinary SEPARATOR and this is the only rule that competes for it.
 * PHL had no such rule and stood the competition down instead -- a dot before a
 * digit was simply not a separator -- which refused every string where no dotted
 * date is there to claim it: `20240102.2020` is a date, a separator and a clock
 * in php, and `1234.2020` is THIS date with a digit left over (php refuses on
 * the fifth byte of the run, not on the dot).
 */
static int DtTryIsoOrdinalDot(const char *z,const char *zEnd,const char **pzOut,
	dt_parsed *p,const char *zIn)
{
	const char *zTok = z;
	sxi64 y;
	int i,doy,rcT;
	if( zEnd - z < 8 ){
		return 0;
	}
	for( i = 0 ; i < 4 ; i++ ){
		if( !SyisDigit(z[i]) ){ return 0; }
	}
	if( z[4] != '.' || !SyisDigit(z[5]) || !SyisDigit(z[6]) || !SyisDigit(z[7]) ){
		return 0;
	}
	doy = (z[5]-'0')*100 + (z[6]-'0')*10 + (z[7]-'0');
	if( doy < 1 || doy > 366 ){
		return 0;
	}
	y = (sxi64)((z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0'));
	z += 8;
	*pzOut = z;
	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){
		return rcT;
	}
	p->y = y;
	p->m = 1;
	p->d = doy;   /* the field normalizer resolves it out of January */
	if( (rcT = DtTimeSuffix(&z,zEnd,zIn,p)) != 0 ){
		*pzOut = z;
		return rcT;
	}
	*pzOut = z;
	return 1;
}
/*
 * Try to read a non-ISO numeric date at z: three integer components joined by ONE
 * consistent separator, plus an optional time suffix. php's field order depends on
 * the separator:
 *   '/'      -> YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY
 *   '-','.'  -> DD-MM-YYYY (day first); a 4-digit-first '.' date (YYYY.MM.DD) is
 *               NOT a php format and is rejected. (ISO YYYY-MM-DD is matched by the
 *               dedicated branch BEFORE this one, so a 4-digit-first '-' never
 *               reaches here.)
 * A 1-2 digit year maps php-style (00-69 -> 2000s, 70-99 -> 1900s). Returns 0 when
 * the text is not such a date (caller falls through), 1 on success (the vector's
 * date fields set and *pzOut advanced past the whole token), or an error code in
 * DtParse's own convention (positive 1-based position into zIn, negative = "double
 * time") when the shape matched but a component is out of range.
 */
static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,
	dt_parsed *p,const char *zIn)
{
	const char *zTok = z;
	int a,b,c,na,nb,nc;
	char sep;
	int y,mo,d;
	int rcT;
	int bOrd1 = 0,bOrd2 = 0,iDayField;
	/* first field: 1-4 digits */
	if( !SyisDigit(z[0]) ){ return 0; }
	a = 0; na = 0;
	while( z < zEnd && SyisDigit(z[0]) && na < 4 ){ a = a*10 + (z[0]-'0'); z++; na++; }
	/* php's ordinal suffix belongs to the DAY, and which FIELD that is depends on
	 * the separator and the widths -- both of them known only further down. So it
	 * is read where it may stand and judged once the mapping is: a suffix on the
	 * year or the month is not this token at all (`2020th-1-2` and `20-1th-2020`
	 * are refusals in php too, since the separator behind it never matches). A
	 * day is at most TWO digits wide there, so a wider run does not carry one
	 * either -- `020th-1-2020` is a refusal on its first byte. */
	if( na <= 2 && DtIsOrdinal(z,zEnd) ){ bOrd1 = 1; z += 2; }
	if( z >= zEnd || (z[0] != '-' && z[0] != '/' && z[0] != '.') ){ return 0; }
	sep = z[0];
	z++;
	/* second field: 1-2 digits */
	if( z >= zEnd || !SyisDigit(z[0]) ){ return 0; }
	b = DtRead1or2(z,zEnd,&nb); z += nb;
	if( DtIsOrdinal(z,zEnd) ){ bOrd2 = 1; z += 2; }
	if( z >= zEnd || z[0] != sep ){ return 0; }
	z++;
	/* third field: 1-4 digits */
	if( z >= zEnd || !SyisDigit(z[0]) ){ return 0; }
	c = 0; nc = 0;
	while( z < zEnd && SyisDigit(z[0]) && nc < 4 ){ c = c*10 + (z[0]-'0'); z++; nc++; }
	/* Which of the three the DAY is -- the separator and the widths decide, and
	 * both the field WIDTH below and php's ordinal suffix follow from it. */
	iDayField = sep == '/' ? (na == 4 ? 3 : 2) : (sep == '.' ? 1 : (nc == 4 ? 1 : 3));
	/* map fields to Y/M/D; nyear tracks the year field's width for 2-digit mapping.
	 * '/'  : YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY.
	 * '-'/'.': a 4-digit LAST field is DD-MM-YYYY (day first); otherwise YY-MM-DD
	 *          (year first) — php's width heuristic. (A 4-digit FIRST '-' field is
	 *          ISO and never reaches here; a 4-digit-first '.' is not a php format.) */
	{
		int nyear;
		if( sep == '/' ){
			if( na == 4 ){ y = a; mo = b; d = c; nyear = na; }
			else{ mo = a; d = b; y = c; nyear = nc; }
		}else if( sep == '.' ){
			/* php's dot date is day-first with a 2- or 4-digit YEAR (`20.03.67` is
			 * 2067-03-20). Any other width is not a clean php format -- php itself
			 * yields garbage there -- so don't claim the match.
			 *
			 * A 2-digit year is the same BYTES as php's dotted CLOCK, and the clock
			 * is the rule its scanner declares first, so the clock wins whenever it
			 * READS: `20.03.00` is 20:03:00 and `20.03.67`, whose seconds no clock
			 * can hold, is the date. The probe runs on a copy with the time flag
			 * cleared, so a "Double time specification" this string would raise
			 * cannot answer the question. */
			if( na == 4 ){ return 0; }
			if( nc == 3 ){
				/* php takes FOUR digits or two, never three: the year of
				 * `20.03.671` is 67 and the `1` is left to the string. */
				c /= 10;
				nc = 2;
				z--;
			}
			if( nc != 4 && nc != 2 ){ return 0; }
			if( nc == 2 ){
				dt_parsed sTry = *p;
				const char *zProbe = zTok;
				sTry.nTimeTok = 0;
				/* php takes the LONGER token, and a twelve-hour clock reads past
				 * where the date would end: `3.04.05` is a date and `3.04.05pm`
				 * the time under it. */
				if( DtReadTimeOfDay(&zProbe,zEnd,zIn,&sTry) == 1 && zProbe >= z ){
					return 0;
				}
			}
			d = a; mo = b; y = c; nyear = nc;
		}else{ /* '-' : a 4-digit LAST field is DD-MM-YYYY, else YY-MM-DD */
			if( nc == 4 ){ d = a; mo = b; y = c; nyear = nc; }
			else{ y = a; mo = b; d = c; nyear = na; }
		}
		if( nyear <= 2 ){
			if( y >= 0 && y <= 69 ){ y += 2000; }
			else if( y >= 70 && y <= 99 ){ y += 1900; }
		}
	}
	/* php spells the ranges INSIDE the pattern, so a month past 12 or a day past
	 * 31 means this is not a date at all and the scanner tries its other rules --
	 * which is what makes `9.30.359699` the time 09:30:35 and the year 9699. Month
	 * 0 and day 0 do match, and normalize (month 0 is December of the year
	 * before). */
	/* A DAY is at most two digits wide in every one of php's spellings, where a
	 * YEAR may be four, so the third field's width depends on which of the two
	 * the mapping made it: `2020/1/22020-01-02` is the 22nd with a second date
	 * behind it, not a day of 2202. */
	if( iDayField == 3 && nc > 2 ){
		int nDrop = nc - 2;
		while( nDrop-- > 0 ){ c /= 10; z--; }
		nc = 2;
		d = c;
	}
	/* php's DAY pattern is one or two digits and the two-digit reading only when
	 * it is in range, so an out-of-range pair leaves its second digit to the
	 * string rather than sinking the rule: `3854-2-40` is the 4th with a stray
	 * `0` after it there (its refusal), not the year-month `3854-2`. Only the
	 * year-first dash shape spells its day that way. */
	if( d > 31 && sep == '-' && na == 4 && nc == 2 && d / 10 <= 31 ){
		d /= 10;
		z--;
	}
	if( mo > 12 || d > 31 ){ return 0; }
	if( (bOrd1 && iDayField != 1) || (bOrd2 && iDayField != 2) ){
		return 0;
	}
	if( iDayField == 3 && nc <= 2 && DtIsOrdinal(z,zEnd) ){
		z += 2;
	}
	/* optional time-of-day suffix, then commit */
	*pzOut = z;
	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }
	/* php's American rule reads its year through a helper that comes back UNSET
	 * once the ordinal has been stepped over, so `4/20th/2020` is the 20th of
	 * April on the BASE moment's year where `4/20/2020` is 2020's. */
	if( !bOrd2 ){
		p->y = y;
	}
	p->m = mo;
	p->d = d;
	rcT = DtTimeSuffix(&z,zEnd,zIn,p);
	*pzOut = z;
	if( rcT != 0 ){ return rcT; }
	return 1;
}
/*
 * Match a month name at z (full name or its distinct 3-letter abbreviation, plus
 * "sept"), case-insensitively and only at a word boundary. Returns the month 1-12
 * and sets *pAdv to the bytes consumed, or 0 when no month name is present.
 */
static int DtMatchMonth(const char *z,const char *zEnd,int *pAdv)
{
	static const struct { const char *z; int n; int mo; } aM[] = {
		{ "january",7,1 },{ "february",8,2 },{ "march",5,3 },{ "april",5,4 },
		{ "june",4,6 },{ "july",4,7 },{ "august",6,8 },{ "september",9,9 },
		{ "sept",4,9 },{ "october",7,10 },{ "november",8,11 },{ "december",8,12 },
		{ "may",3,5 },
		{ "jan",3,1 },{ "feb",3,2 },{ "mar",3,3 },{ "apr",3,4 },{ "jun",3,6 },
		{ "jul",3,7 },{ "aug",3,8 },{ "sep",3,9 },{ "oct",3,10 },{ "nov",3,11 },
		{ "dec",3,12 }
	};
	sxu32 i;
	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){
		int n = aM[i].n;
		if( zEnd - z >= n && SyStrnicmp(z,aM[i].z,(sxu32)n) == 0
		 && DtWordEnds(z,zEnd,n) ){
			*pAdv = n;
			return aM[i].mo;
		}
	}
	return 0;
}
/*
 * php's SEPARATOR bytes -- the run between two tokens, and it is wider than a
 * space: NUL, tab, newline, space and comma are all skipped there, which is
 * what lets a date string keep the newline of the file it was read from. The
 * full stop is one too but only in places (DtIsSepAt below).
 */
static int DtIsSep(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\0' || c == ',';
}
/*
 * php's `space` -- the run allowed INSIDE one token, between a count and its
 * unit, a sign and its digits, or `day` and the `of` behind it. It is narrower
 * than the run between two tokens: `2 days` and `2\tdays` are php's, while
 * `2,days`, `2.days` and `2\ndays` are not a relative token at all there.
 */
static int DtIsSpace(int c)
{
	return c == ' ' || c == '\t';
}
/*
 * ...and the full stop, which php separates with unconditionally. The rules that
 * SPELL a dot -- the day-first `1.2.2020`, the clock's second separator, the
 * ordinal `2020.102` -- claim their own bytes before the run between tokens is
 * ever consulted, so nothing is lost by stepping over the rest: `20240102.2020`
 * is a date, a separator and a clock there.
 */
static int DtIsSepAt(const char *z,const char *zEnd)
{
	if( z >= zEnd ){
		return 0;
	}
	return z[0] == '.' || DtIsSep((unsigned char)z[0]);
}
/*
 * ...and the wider set php tolerates at the ENDS of the string, where a
 * carriage return, a vertical tab and a form feed are allowed as well: a string
 * read from a file keeps its `\r\n` and still parses, while the same bytes
 * BETWEEN two tokens are an unexpected character in both engines. The comma and
 * the full stop go the other way -- they separate tokens but do not close the
 * string, so `12:00,` parses and `3pm,` is not a meridian at all.
 */
static int DtIsEdgeSep(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\0'
	    || c == '\r' || c == '\v' || c == '\f';
}
/*
 * php's own first act on a date string is a TRIM -- timelib_strtotime walks
 * isspace() off both ends and hands its scanner what is left -- so every
 * position it reports afterwards is the TRIMMED string's, while the message
 * still prints the string the caller wrote: `new DateTime('  xyz')` blames
 * position 0 and shows `(x)`, where PHL blamed position 2. The trim is isspace
 * and NOTHING else, which is what keeps a leading NUL or full stop counting --
 * those are separators the scanner steps over, and stepping over one is a byte
 * gone by (`.xyz` refuses at 1).
 */
static int DtIsCSpace(int c)
{
	return c == ' ' || c == '\t' || c == '\n'
	    || c == '\v' || c == '\f' || c == '\r';
}
static void DtTrimEnds(const char **pz,int *pn)
{
	const char *z = *pz;
	int n = *pn;
	while( n > 0 && DtIsCSpace((unsigned char)z[0]) ){ z++; n--; }
	while( n > 0 && DtIsCSpace((unsigned char)z[n-1]) ){ n--; }
	*pz = z;
	*pn = n;
}
/*
 * ...and what the SENTENCE shows of it stops at the first NUL, because php
 * hands the string to a C `%s`. A date string may well carry one -- the scanner
 * reads a NUL as an ordinary separator, so `"15 january 2020\0),/"` is a real
 * parse that fails at byte 16 -- and php names that byte while printing only
 * the sixteen before it.
 */
static int DtCStrLen(const char *z,int n)
{
	int k = 0;
	while( k < n && z[k] != 0 ){ k++; }
	return k;
}
/*
 * Match a weekday name at z (full or 3-letter, case-insensitive). Returns the
 * day-of-week 0=Sunday..6=Saturday and sets *pAdv, or -1.
 *
 * `bLoose` is php's longest-match rule seen from the other side. A name STANDING
 * ALONE competes with the timezone-name token, which is the longer read of
 * `mons` and `tues` -- so those are an unknown zone there, not a weekday -- while
 * a name behind a COUNT is inside one rule with it, nothing longer matches, and
 * the letters left over become a zone of their own (`3 mons` is the third Monday
 * in the military zone S). Only the counted spellings pass it.
 */
static int DtMatchWeekdayEx(const char *z,const char *zEnd,int *pAdv,int bLoose)
{
	static const struct { const char *z; int n; int dow; } aW[] = {
		{ "sunday",6,0 },{ "monday",6,1 },{ "tuesday",7,2 },{ "wednesday",9,3 },
		{ "thursday",8,4 },{ "friday",6,5 },{ "saturday",8,6 },
		{ "sun",3,0 },{ "mon",3,1 },{ "tue",3,2 },{ "wed",3,3 },{ "thu",3,4 },
		{ "fri",3,5 },{ "sat",3,6 }
	};
	sxu32 i;
	for( i = 0 ; i < SX_ARRAYSIZE(aW) ; ++i ){
		int n = aW[i].n;
		if( zEnd - z < n || SyStrnicmp(z,aW[i].z,(sxu32)n) != 0 ){
			continue;
		}
		/* php spells the FULL names with an optional plural `s` and the
		 * three-letter abbreviations without one, so `mondays` is a weekday where
		 * `mons` is `mon` with an `s` left standing -- which the string then reads
		 * as a military zone. */
		if( n > 3 && zEnd - z > n && (z[n] == 's' || z[n] == 'S') ){
			*pAdv = n + 1;
			return aW[i].dow;
		}
		/* A full name is six bytes or more and php's timezone-name token stops at
		 * six, so nothing longer competes with it and letters behind it are the
		 * next token's (`mondayx` is Monday in the military zone X). */
		if( bLoose || n > 3 || zEnd - z == n || !SyisAlpha(z[n]) ){
			*pAdv = n;
			return aW[i].dow;
		}
	}
	return -1;
}
/*
 * php's `americanshort`, `month "/" day` -- the American date with no year at
 * all (`4/20`, `12/31`, `10/2`), which this engine had only in its
 * month/day/year form, so the most common way an American program spells a date
 * without one did not parse.
 *
 * Its fields are spelled INSIDE the pattern rather than range-checked
 * afterwards, `"0"? [0-9] | "1"[0-2]` and `[0-2]?[0-9] | "3"[01]`, so a
 * two-digit reading that is out of range leaves its second digit to the string
 * instead of sinking the rule: `4/32` is the 3rd with a stray `2` behind it and
 * `13/20` an unexpected `1` and then March the 20th. Zero matches both fields
 * and normalizes, which is what makes `0/1` December of the year before.
 * php's ordinal suffix rides the day with NOTHING between them, so `4/20th` is
 * the 20th where `4/20 th` is the 20th beside a zone it cannot find.
 *
 * The YEAR is left alone -- php's action writes only the month and the day --
 * which is what keeps `4/20` on the base moment's year.
 */
static int DtTryAmericanShort(const char *z,const char *zEnd,const char **pzOut,
	dt_parsed *p,const char *zIn)
{
	const char *zTok = z;
	int mo,d,rc;
	if( z >= zEnd || !SyisDigit(z[0]) ){
		return 0;
	}
	mo = z[0] - '0';
	if( z+1 < zEnd && SyisDigit(z[1]) && (z[0] == '0' || (z[0] == '1' && z[1] <= '2')) ){
		mo = mo*10 + (z[1]-'0');
		z += 2;
	}else{
		z++;
	}
	if( z >= zEnd || z[0] != '/' ){
		return 0;
	}
	z++;
	if( z >= zEnd || !SyisDigit(z[0]) ){
		return 0;
	}
	d = z[0] - '0';
	if( z+1 < zEnd && SyisDigit(z[1])
	 && (z[0] <= '2' || (z[0] == '3' && z[1] <= '1')) ){
		d = d*10 + (z[1]-'0');
		z += 2;
	}else{
		z++;
	}
	if( DtIsOrdinal(z,zEnd) ){
		z += 2;
	}
	*pzOut = z;
	if( (rc = DtMarkDate(p,zTok,zIn)) != 0 ){
		return rc;
	}
	p->m = mo;
	p->d = d;
	return 1;
}
/*
 * Try to read a textual-month date at z, in either order:
 *   MonthName [Day] [Year]   ("Jan 15 2020", "January", "January 2020")
 *   Day MonthName [Year]     ("15 January 2020", "15th Jan")
 * Only what the string SPELLS is written: a missing day stays unset (so a bare
 * month name keeps the base day, php's answer) except when a year was given,
 * which is php's own "January 2020" -> the 1st. Day may carry an ordinal suffix,
 * fields may be comma-separated, month names are case-insensitive, and an
 * optional time-of-day suffix + trailing UTC/GMT is consumed. Returns 0 (not a
 * month date — caller falls through, *pzOut untouched), 1 on success, or a
 * DtParse error code (out-of-range day).
 */
static int DtTryMonthDate(const char *z,const char *zEnd,const char **pzOut,
	dt_parsed *p,const char *zIn)
{
	const char *zTok = z;
	const char *zAfterMon = 0;
	int mo,d = 1,adv,haveDay = 0,haveYear = 0;
	int bMonthFirst = 0,nSuf = 0,bClock = 0;
	sxi64 y = 0;
	int rcT;
/* php's textual-date rule spells its run with the full stop in it (`5.january`
 * and `january.5.2020` are dates there) and without the comma -- except between
 * the DAY and the YEAR, which is where the comma everyone writes goes
 * (`January 15, 2020`, and `January, 15 2020` is no date at all). */
#define MDSKIPWS() while( z < zEnd && (DtIsSpace((unsigned char)z[0]) || z[0]=='.') ){ z++; }
/* ...and the run BEHIND the day, which is php's `[,.stndrh\t ]+` -- the ordinal
 * suffix and the comma before a year are the same set, greedy, and it is
 * REQUIRED (or a NUL, or the end of the string) when no year follows: that is
 * what makes `january 12x` no date at all while `january 12sd2020` is one, and
 * what leaves `january 12 sat` reading `at` as a zone. */
#define MDISSUF(c) ((c)==','||(c)=='.'||(c)=='s'||(c)=='t'||(c)=='n'||(c)=='d' \
	||(c)=='r'||(c)=='h'||(c)=='\t'||(c)==' ')
	if( (mo = DtMatchMonth(z,zEnd,&adv)) != 0 ){
		/* MonthName [Day] [Year]. A 4-digit number here is the YEAR, not the day
		 * ("January 2020" is month+year, day defaults); a 1-2 digit number is the day. */
		z += adv;
		zAfterMon = z;
		MDSKIPWS();
		if( z < zEnd && SyisDigit(z[0]) ){
			int nrun = 0;
			const char *zp = z;
			while( zp < zEnd && SyisDigit(zp[0]) && nrun < 4 ){ zp++; nrun++; }
			if( nrun < 4 ){
				d = DtRead1or2(z,zEnd,&adv); z += adv;
				haveDay = 1;
				bMonthFirst = 1;
				while( z < zEnd && MDISSUF((unsigned char)z[0]) ){ z++; nSuf++; }
				/* php's `dateshortwithtimeshort`: a month, a day and a CLOCK are
				 * ONE token there, and it reads longer than the year the same
				 * digits would be -- which is what makes `january 12 12:00` noon
				 * on the 12th where `january 12 12` is the year 2012, and
				 * `january 12 123:00` the year 123 with a refusal behind it. */
				bClock = DtClockFollows(z,zEnd);
			}
		}
	}else if( SyisDigit(z[0]) ){
		/* Day MonthName [Year] */
		d = DtRead1or2(z,zEnd,&adv); z += adv;
		if( DtIsOrdinal(z,zEnd) ){ z += 2; }
		haveDay = 1;
		MDSKIPWS();
		if( (mo = DtMatchMonth(z,zEnd,&adv)) == 0 ){ return 0; }
		z += adv;
		MDSKIPWS();
	}else{
		return 0;
	}
	/* The optional YEAR -- but php's rule spells the run between the day and it
	 * as REQUIRED, so `january 124` is no date at all where `january 12 4` and
	 * `january 12s4` are the year 2004. */
	if( !bClock && !(bMonthFirst && haveDay && nSuf == 0)
	 && z < zEnd && SyisDigit(z[0]) ){
		int ny = 0;
		y = 0;
		while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ y = y*10 + (z[0]-'0'); z++; ny++; }
		if( ny <= 2 ){
			if( y >= 0 && y <= 69 ){ y += 2000; }
			else if( y >= 70 && y <= 99 ){ y += 1900; }
		}
		haveYear = 1;
	}
	/* php spells the day's range inside the pattern too, so a day past 31 is not
	 * this rule at all and the token is refused where it STARTS (`87 january` is
	 * php's position 0), not where the month name ends. */
	/* php's `datenoyear` ends in that run, and spells the day's range inside the
	 * pattern as well -- so with a day past 31, or with nothing behind the day
	 * and no year to close the rule, this text is not that token. The MONTH NAME
	 * still is one of its own, though, and only the month-FIRST spelling can fall
	 * back to it: `january 12x` is January with the digits left to the string,
	 * while `87 january` is php's refusal at position 0. */
	if( d > 31 || (bMonthFirst && !haveYear && nSuf == 0 && z < zEnd && z[0] != 0) ){
		if( !bMonthFirst ){
			return 0;
		}
		z = zAfterMon;
		haveDay = 0;
		haveYear = 0;
		d = 1;
	}
	/* optional time-of-day suffix */
	*pzOut = z;
	if( (rcT = DtMarkDate(p,zTok,zIn)) != 0 ){ return rcT; }
	p->m = mo;
	if( haveDay ){ p->d = d; }
	else if( haveYear ){ p->d = 1; }
	if( haveYear ){ p->y = y; }
	else if( bMonthFirst && haveDay ){
		/* php's `datenoyear` writes the month and the day and UNSETS the year,
		 * which is the whole difference between it and the day-first spelling:
		 * `@100 january 12` has no year where `@100 12 january` keeps 1970. */
		p->y = DT_UNSET;
	}
	if( bClock ){
		rcT = DtReadTimeOfDay(&z,zEnd,zIn,p);
		*pzOut = z;
		if( rcT < 0 ){ return rcT; }
		return 1;
	}
	rcT = DtTimeSuffix(&z,zEnd,zIn,p);
	*pzOut = z;
	if( rcT != 0 ){ return rcT; }
	return 1;
#undef MDSKIPWS
#undef MDISSUF
}
/*
 * php's reltextnumber -- the ORDINAL WORDS that stand where a relative COUNT
 * would. `first` through `twelfth` are 1..12 and the navigation four are the
 * same rule's 1, 0 and -1, which is why `next day` and `first day` are one
 * move and `second day` two of them.
 *
 * Answers the bytes the word takes (0 for anything else) and says which half it
 * came from: php's `... week` SPECIAL -- the move to that week's Monday --
 * belongs to the navigation words alone, so `next week` is that Monday while
 * `first week` is a refusal and `first weeks` seven ordinary days.
 */
static int DtRelWord(const char *z,const char *zEnd,sxi64 *pVal,int *pbNav)
{
	static const struct { const char *zWord; int nWord; int iVal; int bNav; } aWord[] = {
		{ "previous", 8, -1, 1 }, { "next",     4,  1, 1 },
		{ "last",     4, -1, 1 }, { "this",     4,  0, 1 },
		{ "first",    5,  1, 0 }, { "second",   6,  2, 0 },
		{ "third",    5,  3, 0 }, { "fourth",   6,  4, 0 },
		{ "fifth",    5,  5, 0 }, { "sixth",    5,  6, 0 },
		{ "seventh",  7,  7, 0 }, { "eighth",   6,  8, 0 },
		{ "ninth",    5,  9, 0 }, { "tenth",    5, 10, 0 },
		{ "eleventh", 8, 11, 0 }, { "twelfth",  7, 12, 0 }
	};
	int k;
	for( k = 0 ; k < (int)SX_ARRAYSIZE(aWord) ; k++ ){
		int n = aWord[k].nWord;
		if( zEnd - z >= n && SyStrnicmp(z,aWord[k].zWord,n) == 0
		 && (zEnd - z == n || !SyisAlpha(z[n])) ){
			*pVal  = (sxi64)aWord[k].iVal;
			*pbNav = aWord[k].bNav;
			return n;
		}
	}
	return 0;
}
/*
 * Apply php's relative UNIT word at z with the amount v, and answer the bytes it
 * takes -- 0 when there is no unit word here. Shared by the two spellings that
 * reach one: a number in front of it, and php's `this`/`next`/`last`/`previous`,
 * which is the same rule with the amount 0, 1 or -1 (`next hour`, `last year`).
 */
static int DtRelUnit(const char *z,const char *zEnd,sxi64 v,dt_parsed *p,int *pbSpecial)
{
	*pbSpecial = 0;
/* A unit word is never a token on its own -- the NUMBER (or the navigation
 * word) in front of it started the match, so nothing competes with it at its
 * own position and letters behind it belong to whatever comes next: `+1 dayx`
 * is a day and the military zone X, `+1 dayxyz` a day and an unknown zone. */
#define DT_UNITEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0)
	/* php's SUB-SECOND relative units, checked before the words they are
	 * prefixes of ("ms" would otherwise swallow "msec"). `us` is NOT one of
	 * them there, and neither is the Greek mu -- only U+00B5, the MICRO
	 * SIGN, which is the two bytes 0xC2 0xB5 here. They accumulate apart
	 * from the seconds and carry into them in DtApplyFields, so `-500
	 * microseconds` from midnight is the previous day's 23:59:59.999500. */
	if( DT_UNITEQ("microseconds",12) ){ p->rus = DtWAdd(p->rus,v);        return 12; }
	else if( DT_UNITEQ("microsecond",11) ){ p->rus = DtWAdd(p->rus,v);    return 11; }
	else if( DT_UNITEQ("milliseconds",12) ){ p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 12; }
	else if( DT_UNITEQ("millisecond",11) ){ p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 11; }
	else if( DT_UNITEQ("usecs",5) )  { p->rus = DtWAdd(p->rus,v);         return 5; }
	else if( DT_UNITEQ("usec",4) )   { p->rus = DtWAdd(p->rus,v);         return 4; }
	else if( DT_UNITEQ("msecs",5) )  { p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 5; }
	else if( DT_UNITEQ("msec",4) )   { p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 4; }
	else if( DT_UNITEQ("\xc2\xb5s",3) ){ p->rus = DtWAdd(p->rus,v);       return 3; }
	else if( DT_UNITEQ("ms",2) )     { p->rus = DtWAdd(p->rus,DtWMul(v,1000)); return 2; }
	else if( DT_UNITEQ("seconds",7) ){ p->rs = DtWAdd(p->rs,v);           return 7; }
	else if( DT_UNITEQ("second",6) ) { p->rs = DtWAdd(p->rs,v);           return 6; }
	else if( DT_UNITEQ("secs",4) )   { p->rs = DtWAdd(p->rs,v);           return 4; }
	else if( DT_UNITEQ("sec",3) )    { p->rs = DtWAdd(p->rs,v);           return 3; }
	else if( DT_UNITEQ("minutes",7) ){ p->ri = DtWAdd(p->ri,v);           return 7; }
	else if( DT_UNITEQ("minute",6) ) { p->ri = DtWAdd(p->ri,v);           return 6; }
	else if( DT_UNITEQ("mins",4) )   { p->ri = DtWAdd(p->ri,v);           return 4; }
	else if( DT_UNITEQ("min",3) )    { p->ri = DtWAdd(p->ri,v);           return 3; }
	else if( DT_UNITEQ("hours",5) )  { p->rh = DtWAdd(p->rh,v);           return 5; }
	else if( DT_UNITEQ("hour",4) )   { p->rh = DtWAdd(p->rh,v);           return 4; }
	else if( DT_UNITEQ("days",4) )   { p->rd = DtWAdd(p->rd,v);           return 4; }
	else if( DT_UNITEQ("day",3) )    { p->rd = DtWAdd(p->rd,v);           return 3; }
	/* php's business-day words go BEFORE the plain week, because its scanner
	 * takes the longest of the two and `week` is their prefix. */
	else if( DT_UNITEQ("weekdays",8) ){ p->bWeekdays = 1; *pbSpecial = 1; p->iWeekdays = v; return 8; }
	else if( DT_UNITEQ("weekday",7) ) { p->bWeekdays = 1; *pbSpecial = 1; p->iWeekdays = v; return 7; }
	else if( DT_UNITEQ("weeks",5) )  { p->rd = DtWAdd(p->rd,DtWMul(v,7)); return 5; }
	else if( DT_UNITEQ("week",4) )   { p->rd = DtWAdd(p->rd,DtWMul(v,7)); return 4; }
	else if( DT_UNITEQ("fortnights",10) ){ p->rd = DtWAdd(p->rd,DtWMul(v,14)); return 10; }
	else if( DT_UNITEQ("fortnight",9) )  { p->rd = DtWAdd(p->rd,DtWMul(v,14)); return 9; }
	else if( DT_UNITEQ("months",6) ) { p->rm = DtWAdd(p->rm,v);           return 6; }
	else if( DT_UNITEQ("month",5) )  { p->rm = DtWAdd(p->rm,v);           return 5; }
	else if( DT_UNITEQ("years",5) )  { p->ry = DtWAdd(p->ry,v);           return 5; }
	else if( DT_UNITEQ("year",4) )   { p->ry = DtWAdd(p->ry,v);           return 4; }
	/* php's BUSINESS-day count, which is not a field of the vector at all but a
	 * move of its own (see DtAdjustWeekdays) */
	/* php SETS this one rather than adding to it, so the last count in the string
	 * is the only one that moves anything. */
	return 0;
#undef DT_UNITEQ
}
/*
 * How many bytes a relative UNIT word would take here, without applying it.
 * php's scanner takes the LONGEST rule that matches, and a weekday name is the
 * prefix of two unit words (`mon` of `month`, `sat` of nothing but `mon` is
 * enough): the counted spellings ask this before they claim a weekday, which is
 * what keeps `next month` a month and `3 months` three of them.
 */
static int DtRelUnitLen(const char *z,const char *zEnd,const dt_parsed *p)
{
	dt_parsed sTmp = *p;
	int bSpec = 0;
	return DtRelUnit(z,zEnd,0,&sTmp,&bSpec);
}
/*
 * php's date-string parse, onto the field vector: absolute forms
 * "now" | "@<ts>" | "YYYY-MM-DD[( |T)HH:MM[:SS]][Z|±HH[:MM]]" | "HH:MM[:SS]" |
 * a textual month date, the keywords today/midnight/noon/tomorrow/yesterday, the
 * weekday and month navigation words, and relative sequences
 * "[+|-]N (sec|min|hour|day|week|fortnight|month|year)[s]". NOTHING is applied
 * here -- DtApplyFields does that, in php's order, once the whole string is read.
 * Returns 0 on success, or the byte position of the first unparseable character
 * +1 (for php's "at position N" message).
 */
/*
 * php's `@epoch` token, which its scanner reads ANYWHERE in a string rather
 * than only at its head: `2020-01-02 @100` and `12:00 @100` are the epoch
 * there, not refusals.
 *
 * The shape is `"@" "-"? [0-9]+ ("." [0-9]{0,6})?`. A PLUS is no part of it, so
 * `@+100` is an unexpected `@` with a UTC offset behind it; and the fraction
 * stops at SIX digits, which leaves the seventh to the string (`@100.1234567`
 * refuses on it while `@100.1234567890` reads the trailing four as a year).
 *
 * The ACTION is php's own order, and the order is what shows: TIMELIB_UNHAVE_DATE
 * and TIMELIB_UNHAVE_TIME first -- which ZERO the civil fields rather than
 * unsetting them, so an epoch behind a date reads back as the year 0, not as the
 * date -- then TIMELIB_HAVE_TZ, then the value. The middle step is a RETURN when
 * the string already named a zone, so `UTC @100` leaves nothing behind but those
 * zeroes: neither 1970 nor the seconds are ever written. An empty fraction is
 * php's `Found unexpected data`, raised at the `@` and after the value, which is
 * why `@100.,UTC` still reads back as 1970 plus a hundred seconds.
 *
 * Advances *pz over what it took; answers 0 when this is not the token, 1 when
 * it is, or an error code in DtParse's own convention.
 */
static int DtTryEpoch(const char **pz,const char *zEnd,dt_parsed *p,const char *zIn)
{
	const char *z = *pz,*zAt = z;
	sxi64 v = 0,us = 0;
	int neg = 0,nDig = 0,bDot = 0,nFrac = 0,k,rc;
	if( z >= zEnd || z[0] != '@' ){
		return 0;
	}
	z++;
	if( z < zEnd && z[0] == '-' ){ neg = 1; z++; }
	if( z >= zEnd || !SyisDigit(z[0]) ){
		/* php's lexer never matched a token here at all: the `@` is the refusal */
		return (int)(zAt - zIn) + 1;
	}
	while( z < zEnd && SyisDigit(z[0]) ){
		if( nDig < DT_DIGITS_SAFE ){ v = v*10 + (z[0]-'0'); }
		nDig++;
		z++;
	}
	if( z < zEnd && z[0] == '.' ){
		bDot = 1;
		z++;
		while( nFrac < 6 && z < zEnd && SyisDigit(z[0]) ){
			us = us*10 + (z[0]-'0');
			nFrac++;
			z++;
		}
		for( k = nFrac ; k < 6 ; k++ ){ us *= 10; }
	}
	*pz = z;
	if( nDig > DT_DIGITS_EPOCH ){
		/* php reports it at the `@`, with its own reason. */
		return -((int)(zAt - zIn) + 1) - DT_ERR_RANGE;
	}
	/* php's order: HAVE_RELATIVE, then UNHAVE_DATE and UNHAVE_TIME, then
	 * HAVE_TZ -- so an epoch that bails on the zone has already marked the parse
	 * relative, and date_parse() shows an all-zero block for it. */
	p->bHaveRel = 1;
	p->y = p->m = p->d = 0;
	p->bHaveDate = 0;
	DtUnhaveTime(p);
	rc = DtZoneCount(p);
	if( rc < 0 ){
		return -((int)(zAt - zIn) + 1) - DT_ERR_DZONE;
	}
	if( rc > 0 ){
		DtWarnPend(p,(int)(zAt - zIn),"Double timezone specification");
		return 1;   /* php returns from inside HAVE_TZ: nothing below runs */
	}
	DtZoneStore(p,0,0,0,0);
	p->bEpoch = 1;
	p->y = 1970; p->m = 1; p->d = 1;
	p->h = p->i = p->s = p->us = 0;
	p->bUsUnset = 0;
	if( neg ){ v = -v; }
	/* php adds the fraction to the RELATIVE microseconds with the token's own
	 * sign rather than borrowing a second for it, so `@-1.5` is -1s and -500000us
	 * there -- the same instant, and the count date_parse() shows. Being relative
	 * is also what makes a later `tomorrow`, whose whole job is to zero the
	 * clock, leave it standing. */
	if( nFrac > 0 ){
		p->rus = neg ? -us : us;
	}
	p->rs = DtWAdd(p->rs,v);
	if( bDot && nFrac == 0 ){
		return -((int)(zAt - zIn) + 1) - DT_ERR_UNEXPDATA;
	}
	return 1;
}
/* Forward: the scan publishes what it collects, and words a refusal in php's
 * own terms -- both live below, beside the record they write to. */
static const char * DtParseErr(const char *zIn,int nLen,int iErrPos,int *piPos,char *pcAt);
static void DtRecErr(phl_dt_lasterr *pRec,int iPos,const char *zMsg);
static void DtRecWarn(phl_dt_lasterr *pRec,int iPos,const char *zMsg);
static void DtRecReset(phl_dt_lasterr *pRec);
static int DtDaysInMonth(sxi64 y,int m);
static int DtParseFields(const char *zIn,int nLen,dt_parsed *p,phl_dt_lasterr *pRec)
{
	const char *z,*zEnd;
	const char *zPrev = 0;
	int bAny = 0;
	int iRc,iFirst = 0,k;
	/* php refuses an EMPTY string before it does anything else, and the test is
	 * on what the caller handed over rather than on what the trim leaves: a
	 * string of blanks parses as `now`. The constructors never see it, because
	 * php hands THEM the word `now` in its place -- only modify() and the
	 * component readers do. */
	if( nLen < 1 ){
		DtRecErr(pRec,0,"Empty string");
		return -1 - DT_ERR_EMPTY;
	}
	/* php's trim comes FIRST and the positions below are all measured from what
	 * it leaves, so rebase on it here and every rule inherits the answer. */
	DtTrimEnds(&zIn,&nLen);
	z = zIn;
	zEnd = &zIn[nLen];
	/* The wider set php tolerates at the trailing END -- a carriage return, a
	 * vertical tab, a form feed, a NUL -- closes the string; at the FRONT the
	 * trim has taken what it takes and everything left is an ordinary token
	 * separator, which DT_SKIP_WS below reads. Stepping over more than that
	 * loses a byte php refuses: `.\rjanuary` is an unexpected `\r` there. */
	while( zEnd > z && DtIsEdgeSep((unsigned char)zEnd[-1]) ){ zEnd--; }
#define DT_SKIP_WS() while( DtIsSepAt(z,zEnd) ){ z++; }
/* ...and the run INSIDE one token, which is php's narrower `space`. */
#define DT_SPACE() while( z < zEnd && DtIsSpace((unsigned char)z[0]) ){ z++; }
#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \
	&& DtWordEnds(z,zEnd,nKw))
/* php's SINGULAR `week`, the word its `... week` special is spelled with. It is
 * only ever the TAIL of a longer rule, so nothing competes at its own position
 * and letters behind it belong to whatever comes next (`this weekjanuary` is
 * that week and then January) -- except any UNIT word that reads LONGER here,
 * which is that rule instead: `weeks`, `weekday` and `weekdays`. */
#define DT_WEEKSING() (zEnd-z >= 4 && SyStrnicmp(z,"week",4) == 0 \
	&& DtRelUnitLen(z,zEnd,p) <= 4)
/*
 * php's scanner RECORDS a refusal and reads on, so an error is not the end of
 * the parse: the reason and the byte are published and the walk resumes one
 * byte past what was named -- which is exactly where php's catch-all rule
 * leaves its cursor. A token that MATCHED and whose action then complained is
 * already behind the cursor, because the rule that raised it moved past it, so
 * taking whichever of the two is FURTHER covers both kinds. The first code is
 * kept: it is the one the constructors put in their sentence.
 */
#define DT_FAIL(iCode) do{ \
		int _p = 0; \
		char _c = ' '; \
		const char *_m = DtParseErr(zIn,nLen,(iCode),&_p,&_c); \
		DtRecErr(pRec,_p,_m); \
		if( iFirst == 0 ){ iFirst = (iCode); } \
		if( z < &zIn[_p + 1] ){ z = &zIn[_p + 1]; } \
		if( z > zEnd ){ z = zEnd; } \
	}while(0)
	DT_SKIP_WS();
	if( z >= zEnd ){
		/* php: the empty string is "now" */
		return 0;
	}
	/*
	 * One rule set, tried at every position -- php's scanner has no head of its
	 * own and neither does this walk. The keyword rules come first and are
	 * spelled in LETTERS, so nothing they could claim reaches the date rules
	 * behind them by another route.
	 */
	for(;;){
		/* Every pass must CONSUME something. A shape rule that claims a token
		 * without advancing the cursor would spin here forever -- and one did:
		 * the ISO rule's refusal encodes the token's POSITION, and a token at
		 * position 0 encodes as the same 1 that means "matched", so
		 * `new DateTime('2020-1-1 12:00')` hung the engine outright. The rule is
		 * fixed above; this makes the whole class of it a refusal instead. */
		if( z == zPrev ){
			DT_FAIL((int)(z - zIn) + 1);
			if( z == zPrev ){
				/* Nothing could advance it -- stop rather than spin, which is the
				 * failure this guard exists for. */
				break;
			}
			continue;
		}
		zPrev = z;
		DT_SKIP_WS();
		if( z >= zEnd ){
			break;
		}
		if( (iRc = DtTryEpoch(&z,zEnd,p,zIn)) != 0 ){
			if( iRc != 1 ){ DT_FAIL(iRc); continue; }
			bAny = 1;
			continue;
		}
		if( DT_LOWEQ("now",3) ){
			z += 3;
			bAny = 1;
			continue;
		}
		if( DT_LOWEQ("today",5) || DT_LOWEQ("midnight",8) ){
			DtUnhaveTime(p);
			z += (SyToLower(z[0])=='t') ? 5 : 8;
			bAny = 1;
			continue;
		}
		if( DT_LOWEQ("noon",4) ){
			DtUnhaveTime(p);
			p->h = 12;
			p->nTimeTok = 1;
			z += 4;
			bAny = 1;
			continue;
		}
		/* php SETS the relative day for these two rather than adding to it, so
		 * either one wipes whatever days came before it: `+3 days tomorrow` is one
		 * day on, and `tomorrow yesterday` is yesterday. */
		if( DT_LOWEQ("tomorrow",8) ){
			DtUnhaveTime(p);
			p->bHaveRel = 1;
			p->rd = 1;
			z += 8;
			bAny = 1;
			continue;
		}
		if( DT_LOWEQ("yesterday",9) ){
			DtUnhaveTime(p);
			p->bHaveRel = 1;
			p->rd = -1;
			z += 9;
			bAny = 1;
			continue;
		}
		/* "first|last day of": php's own standalone token. It records the flag and
		 * nothing else — whatever names the target month ("next month", "January
		 * 2021", or nothing at all) is an ordinary token after it, and the flag is
		 * applied LAST, which is what makes it swallow any relative days beside it. */
		if( DT_LOWEQ("first",5) || DT_LOWEQ("last",4) ){
			const char *zSave = z;
			int bFirst = (SyToLower((unsigned char)z[0]) == 'f');
			z += bFirst ? 5 : 4;
			DT_SPACE();
			if( DT_LOWEQ("day",3) ){
				z += 3;
				DT_SPACE();
				if( DT_LOWEQ("of",2) ){
					z += 2;
					p->iFirstLast = bFirst ? 1 : 2;
					p->bHaveRel = 1;
					bAny = 1;
					continue;
				}
			}
			z = zSave; /* not the "first|last day of" shape: rewind */
		}
		/* Weekday navigation: "[next|last|previous|this] <weekday>" moves to the
		 * target weekday's midnight. php's behaviour code decides whether a base
		 * day that already matches counts: "next"/"last" skip it, a bare name or
		 * "this" keeps it. The move itself happens in DtAdjustWeekday, BEFORE the
		 * relative vector, which is php's order. */
		{
			const char *zSave = z;
			sxi64 iCnt = 0;
			int bNav = 0,bHavePrefix = 0,nWord;
			int adv,dow;
			if( (nWord = DtRelWord(z,zEnd,&iCnt,&bNav)) > 0 ){
				z += nWord;
				DT_SPACE();
				bHavePrefix = 1;
			}
			dow = DtMatchWeekdayEx(z,zEnd,&adv,bHavePrefix);
			if( dow >= 0 && DtRelUnitLen(z,zEnd,p) > adv ){
				dow = -1;   /* `next month` is the UNIT, not `mon` and a stray `th` */
			}
			if( dow >= 0 && bHavePrefix ){
				/* php's `first monday of`: a WORD count, a weekday and the word
				 * `of` are one token, and what it names is a weekday inside a
				 * MONTH. The digit spelling does not reach it -- `1 monday of` is
				 * a refusal there -- and neither does a bare name. */
				const char *zOf = &z[adv];
				while( zOf < zEnd && DtIsSpace((unsigned char)zOf[0]) ){ zOf++; }
				if( zEnd - zOf >= 2 && SyStrnicmp(zOf,"of",2) == 0
				 && (zEnd - zOf == 2 || !SyisAlpha(zOf[2])) ){
					DtUnhaveTime(p);
					p->bWdayOf = 1;
					p->bHaveRel = 1;
					p->bWday = 1;
					p->iWday = dow;
					if( p->iWdayBehavior != 2 ){
						/* a day that already matches counts for every count php
						 * spells forward (behaviour 1); `last` and `previous`,
						 * which walk back a week from the month after, skip it */
						p->iWdayBehavior = iCnt >= 0 ? 1 : 0;
					}
					if( iCnt >= 1 ){
						p->rd = DtWAdd(p->rd,DtWMul(iCnt - 1,7));
					}else{
						p->iWdayOfNext = 1;
						if( iCnt < 0 ){
							p->rd = DtWAdd(p->rd,-7);
						}
					}
					z = &zOf[2];
					bAny = 1;
					continue;
				}
			}
			if( dow >= 0 ){
				DtUnhaveTime(p);
				p->bHaveRel = 1;
				p->bWday = 1;
				p->iWday = dow;
				/* php: a COUNT word carries behaviour 0 and shifts a week per
				 * count past the first (`last monday` is -7 days from the
				 * matching one, `second monday` +7); "this" is that rule's zero
				 * and carries behaviour 1, as a bare name does. A bare name does
				 * NOT overwrite the WEEK behaviour a `... week` word already set,
				 * which is what keeps `last week monday` in that week. */
				if( bHavePrefix ){
					p->iWdayBehavior = (iCnt != 0) ? 0 : 1;
					p->rd = DtWAdd(p->rd,DtWMul(iCnt > 0 ? iCnt - 1 : iCnt,7));
				}else if( p->iWdayBehavior != 2 ){
					p->iWdayBehavior = 1;
				}
				z += adv;
				bAny = 1;
				continue;
			}
			z = zSave; /* prefix did not introduce a weekday: rewind and try the rest */
		}
		/* Standalone "this|next|last (month|week)". php's month is an ordinary
		 * relative month; its WEEK is a weekday-relative move to the Monday of the
		 * week (behaviour 2) plus the whole weeks, which is why "next week" is that
		 * Monday and not seven days from the base day. */
		{
			const char *zSave = z;
			sxi64 iCnt = 0;
			int bNav = 0,nWord;
			if( (nWord = DtRelWord(z,zEnd,&iCnt,&bNav)) > 0 ){
				z += nWord;
				DT_SPACE();
				if( bNav && DT_WEEKSING() ){
					z += 4;
					p->bHaveRel = 1;
					p->rd = DtWAdd(p->rd,DtWMul(iCnt,7));
					if( !p->bWday ){        /* php: Monday, unless a weekday was
						* already named ("monday this week") */
						p->bWday = 1;
						p->iWday = 1;
					}
					p->iWdayBehavior = 2;
					bAny = 1;
					continue;
				}
				/* The SINGULAR `week` is that special's own word and no other
				 * count reaches it: `first week` is a refusal in php where
				 * `first weeks` is seven ordinary days. */
				if( bNav || !DT_WEEKSING() ){
					int nU,bSpec;
					nU = DtRelUnit(z,zEnd,iCnt,p,&bSpec);
					if( nU > 0 ){
						/* php's business-day count zeroes the clock when a WORD
						 * asked for it (`next weekday` is midnight) and leaves it
						 * alone when a number did (`2 weekdays` keeps the hour). */
						if( bSpec ){ DtUnhaveTime(p); }
						p->bHaveRel = 1;
						z += nU;
						bAny = 1;
						continue;
					}
				}
			}
			z = zSave;
		}
		/* A bare `weekday`, with no count in front of it, is not the business-day
		 * move at all in php but the MONDAY hunt -- the same answer a bare weekday
		 * NAME gives. */
		if( DT_LOWEQ("weekdays",8) || DT_LOWEQ("weekday",7) ){
			DtUnhaveTime(p);
			p->bHaveRel = 1;
			p->bWday = 1;
			p->iWday = 1;
			if( p->iWdayBehavior != 2 ){ p->iWdayBehavior = 1; }
			z += DT_LOWEQ("weekdays",8) ? 8 : 7;
			bAny = 1;
			continue;
		}
		/* php's `ago` NEGATES the relative vector as it stands -- the weekday it
		 * hunts for included, which is what makes `next monday ago` the Monday
		 * before -- so a second `ago` puts it back. */
		if( DT_LOWEQ("ago",3) ){
			p->ry = -p->ry; p->rm = -p->rm; p->rd = -p->rd;
			p->rh = -p->rh; p->ri = -p->ri; p->rs = -p->rs;   /* NOT the micro-
				* seconds: php's `ago` leaves that one field standing */
			p->iWday = -p->iWday;
			p->iWeekdays = -p->iWeekdays;
			z += 3;
			bAny = 1;
			continue;
		}
		/* The absolute DATE tokens, php's own rule that any of them may stand
		 * anywhere in the string: "first day of january", "+1 day january",
		 * "march 3" and the tail of `12345-01-01` (a compact time, then a date)
		 * all reach here. The dates go first so that the longest reading wins --
		 * `1.2.2020` is a date where a bare `1.2` is the time 01:02. */
		if( SyisDigit(z[0]) && (iRc = DtTryIsoWeek(z,zEnd,&z,p,zIn)) != 0 ){
			if( iRc != 1 ){ DT_FAIL(iRc); continue; }
			bAny = 1;
			continue;
		}
		if( (iRc = DtTryIsoDate(z,zEnd,&z,p,zIn)) != 0 ){
			if( iRc != 1 ){ DT_FAIL(iRc); continue; }
			bAny = 1;
			continue;
		}
		if( SyisDigit(z[0]) && (iRc = DtTryIsoOrdinalDot(z,zEnd,&z,p,zIn)) != 0 ){
			if( iRc != 1 ){ DT_FAIL(iRc); continue; }
			bAny = 1;
			continue;
		}
		if( SyisDigit(z[0]) && (iRc = DtTryNumericDate(z,zEnd,&z,p,zIn)) != 0 ){
			if( iRc != 1 ){ DT_FAIL(iRc); continue; }
			bAny = 1;
			continue;
		}
		/* ...and the same date with no YEAR, which is a shorter read than the
		 * three-field one above and so is tried after it. */
		if( SyisDigit(z[0]) && (iRc = DtTryAmericanShort(z,zEnd,&z,p,zIn)) != 0 ){
			if( iRc != 1 ){ DT_FAIL(iRc); continue; }
			bAny = 1;
			continue;
		}
		if( (SyisAlpha(z[0]) || SyisDigit(z[0]))
		 && (iRc = DtTryMonthDate(z,zEnd,&z,p,zIn)) != 0 ){
			if( iRc != 1 ){ DT_FAIL(iRc); continue; }
			bAny = 1;
			continue;
		}
		/* A time of day ("next thursday 15:00", or one standing alone). */
		if( (iRc = DtReadTimeOfDay(&z,zEnd,zIn,p)) != 0 ){
			if( iRc != 1 ){ DT_FAIL(iRc); continue; }
			bAny = 1;
			continue;
		}
		if( SyisDigit(z[0]) || z[0]=='+' || z[0]=='-' ){
			int neg = 0,bNoUnit = 0,nDig = 0;
			sxi64 v = 0;
			const char *zNumStart = z;
			const char *zDig;
			if( z[0]=='+' || z[0]=='-' ){
				neg = (z[0]=='-');
				z++;
				/* php's lexer takes the sign as its own token, so whitespace may
				 * follow it: "1 year + 3 months" is a relative sequence there and
				 * was a parse FAILURE here. Its `space` alone, though: `+,3 days`
				 * and `+.3 days` are refusals there. */
				DT_SPACE();
			}
			if( z >= zEnd || !SyisDigit(z[0]) ){
				z = zNumStart;
				DT_FAIL((int)(zNumStart - zIn) + 1);
				continue;
			}
			zDig = z;
			while( z < zEnd && SyisDigit(z[0]) ){
				if( nDig < DT_DIGITS_SAFE ){ v = v*10 + (z[0]-'0'); }
				nDig++;
				z++;
			}
			if( neg ){ v = -v; }
			DT_SPACE();
			/* php's unit words, sub-second ones first: they are all one rule (see
			 * DtRelUnit), and the microseconds accumulate apart from the seconds
			 * so `-500 microseconds` from midnight borrows a whole second. */
			{
				int nU,bSpec;
				nU = DtRelUnit(z,zEnd,v,p,&bSpec);
				if( nU > 0 ){ p->bHaveRel = 1; z += nU; }
				else{ bNoUnit = 1; }
			}
			if( bNoUnit ){
				/* php's COUNTED weekday, `2 monday`: the count is whole WEEKS
				 * past the first, the hunt is the bare name's (behaviour 1, so a
				 * base day that already matches counts), and -- unlike every
				 * spelling that reaches one through a WORD -- the clock is left
				 * standing. */
				int adv,dow = DtMatchWeekdayEx(z,zEnd,&adv,1);
				if( dow >= 0 && DtRelUnitLen(z,zEnd,p) > adv ){
					dow = -1;   /* `3 months` is the unit, not `mon` and `ths` */
				}
				if( dow >= 0 ){
					p->bHaveRel = 1;
					p->bWday = 1;
					p->iWday = dow;
					if( p->iWdayBehavior != 2 ){ p->iWdayBehavior = 1; }
					p->rd = DtWAdd(p->rd,DtWMul(v > 0 ? v - 1 : v,7));
					z += adv;
					bNoUnit = 0;
				}
			}
			if( !bNoUnit ){
				/* php's own ceiling on a relative number, checked once the UNIT
				 * has claimed the run (the nocolon rules below have their own
				 * widths, and a bare `20240102123456` is a date and a time). */
				if( nDig > DT_DIGITS_REL ){
					DT_FAIL(-((int)(zDig - zIn) + 1) - DT_ERR_RANGE);
					continue;
				}
				bAny = 1;
				continue;
			}
			/* No unit word: this is not a relative token at all. php's scanner
			 * would have taken a LONGER match, so rewind and let the nocolon
			 * rules below read the same digits as a date, a clock or a year. */
			z = zNumStart;
		}
		/* php's NOCOLON spellings, a bare run of digits read by WIDTH: eight are a
		 * date (`20240102`), six a time (`123456`), four a time (`1234`) -- or, if
		 * the string already named a time, a YEAR, which is php's own dispatch and
		 * what makes `12:00 1234` the year 1234 and `1234 12:00` a refusal. A run
		 * of four that is no valid clock is a year outright (`2500`), and a third
		 * such run is php's "Double time specification".
		 *
		 * Only the longest reading counts, so an over-wide run leaves its tail to
		 * the loop: `12345-01-01` is 12:34 on 2005-01-01, and `1234567` is a parse
		 * failure at its last digit. */
		if( SyisDigit(z[0]) || ((z[0]=='t' || z[0]=='T') && z+1 < zEnd && SyisDigit(z[1])) ){
			int bT = !SyisDigit(z[0]);
			const char *zd = &z[bT];
			int n = 0,bNoRun = 0,h,mi,se,mo,d,doy;
			while( zd+n < zEnd && SyisDigit(zd[n]) ){ n++; }
#define DTNUM2(k) ((zd[k]-'0')*10 + (zd[(k)+1]-'0'))
			mo = (n >= 8) ? DTNUM2(4) : 99;
			d  = (n >= 8) ? DTNUM2(6) : 99;
			h  = (n >= 4) ? DTNUM2(0) : 99;
			mi = (n >= 4) ? DTNUM2(2) : 99;
			se = (n >= 6) ? DTNUM2(4) : 99;
			doy = (n >= 7) ? (zd[4]-'0')*100 + DTNUM2(5) : 0;
			if( !bT && n == 4 ){
				/* php's REVERSE no-day date, `2020 Jan`: a four-digit year and a
				 * month name are one token there, and a longer one than the clock
				 * reading of those same four digits -- which is why `2020 Jan 15`
				 * is a parse failure and not a time. */
				const char *zm = &zd[4];
				int adv,mo2;
				while( zm < zEnd && (zm[0]==' '||zm[0]=='\t'||zm[0]=='.'||zm[0]=='-') ){ zm++; }
				if( (mo2 = DtMatchMonth(zm,zEnd,&adv)) != 0 ){
					int rcD = DtMarkDate(p,z,zIn);
					if( rcD != 0 ){ z = &zm[adv]; DT_FAIL(rcD); continue; }
					p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));
					p->m = mo2;
					p->d = 1;
					z = &zm[adv];
					bAny = 1;
					continue;
				}
			}
			if( !bT && n >= 8 && mo <= 12 && d <= 31 ){
				int rcD = DtMarkDate(p,z,zIn);
				if( rcD != 0 ){ z = &zd[8]; DT_FAIL(rcD); continue; }
				p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));
				p->m = mo;
				p->d = d;
				z = &zd[8];
			}else if( !bT && n >= 7 && doy >= 1 && doy <= 366 ){
				/* php's ISO ORDINAL date, YYYYDDD: the day of the year, which the
				 * field normalizer resolves out of January. */
				int rcD = DtMarkDate(p,z,zIn);
				if( rcD != 0 ){ z = &zd[7]; DT_FAIL(rcD); continue; }
				p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));
				p->m = 1;
				p->d = doy;
				z = &zd[7];
			}else if( n >= 6 && h <= 24 && mi <= 59 && se <= 60 ){
				/* six digits are php's whole clock, and a SECOND clock is its
				 * refusal rather than the year the four-digit run falls back to */
				if( p->nTimeTok ){
					iRc = -((int)(z - zIn) + 1);
					z = &zd[6];
					DT_FAIL(iRc);
					continue;
				}
				p->h = h; p->i = mi; p->s = se; p->us = 0;
				p->bUsUnset = 0;
				p->nTimeTok = 1;
				z = &zd[6];
			}else if( n >= 4 && h <= 24 && mi <= 59 ){
				if( p->nTimeTok >= 2 ){
					iRc = -((int)(z - zIn) + 1);
					z = &zd[4];
					DT_FAIL(iRc);
					continue;
				}
				if( p->nTimeTok == 1 ){
					p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));
				}else{
					/* php's `gnunocolon` writes the fields itself rather than
					 * through TIMELIB_HAVE_TIME, so the sub-second one is never
					 * touched: `date_parse('1234')` shows no fraction at all,
					 * while `12:00 1234` keeps the zero its clock wrote. */
					p->bUsUnset = (p->us == DT_UNSET);
					p->h = h; p->i = mi; p->s = 0; p->us = 0;
				}
				p->nTimeTok++;
				z = &zd[4];
			}else if( !bT && n >= 4 ){
				/* php's bare year4, which does NOT count as a date: the month, the
				 * day and the clock all stay the base moment's -- the MICROSECONDS
				 * excepted. The run reached this branch through php's have_time
				 * bookkeeping, which zeroes the sub-second field on the way past,
				 * so `new DateTime('7609')` is the current time of day on that
				 * year with nothing under the second. */
				p->y = (sxi64)(DTNUM2(0)*100 + DTNUM2(2));
				p->bUsUnset = (p->us == DT_UNSET);   /* php's `year4` writes the
					* year and nothing else */
				p->us = 0;
				z = &zd[4];
			}else if( bT ){
				/* php's `t` + an HOUR alone: `t9` is 09:00 where a bare `9` is
				 * nothing at all, and the hour is read the same greedy way as the
				 * one before a colon (`t95846` is 09:00 and the year 5846). */
				int nh = DtReadField(zd,zEnd,24,&h);
				if( p->nTimeTok ){
					iRc = -((int)(z - zIn) + 1);
					z = &zd[nh];
					DT_FAIL(iRc);
					continue;
				}
				p->h = h;
				p->i = p->s = p->us = 0;
				p->bUsUnset = 0;
				p->nTimeTok = 1;
				z = &zd[nh];
			}else{
				bNoRun = 1;   /* no nocolon rule claims it; z is untouched */
			}
			if( !bNoRun ){
				bAny = 1;
				continue;
			}
#undef DTNUM2
		}
		/* php's TIMEZONE token stands anywhere in a string and is a token in its
		 * own right: `2020-01-01 12:00 +0200` (its own serialization spelling, and
		 * every RFC-2822 date there is), `12:00 UTC`, `1234z`, and `UTC` alone --
		 * none of which parsed here at all. It goes LAST because php's scanner
		 * takes the longest reading: `+1 day` is a relative and `t9` a clock, and
		 * both would otherwise be read as a zone. */
		if( (iRc = DtZoneTok(&z,zEnd,p,zIn)) != 0 ){
			if( iRc != 1 ){ DT_FAIL(iRc); continue; }
			bAny = 1;
			continue;
		}
		DT_FAIL((int)(z - zIn) + 1);
	}
	/* The warnings a RULE raised, in the order the scan met them ... */
	for( k = 0 ; k < p->nWarnPend && k < PH7_DT_MAX_WARN ; k++ ){
		DtRecWarn(pRec,p->aWarnPos[k],p->azWarn[k]);
	}
	/*
	 * ...and the two php raises once the scan is over, both at the byte one past
	 * the string. They are about what the string SPELLED rather than what any
	 * rule refused, so a date nobody could hold (`2020-02-31`, `2020-102`) and a
	 * clock nobody could show (`24:00:00`, `23:59:60`) are warnings on a parse
	 * that otherwise succeeds. php asks the time first, and both land on the same
	 * key -- so a string with both counts two and shows the date's.
	 */
	if( p->nTimeTok
	 && (p->h > 23 || p->i > 59 || p->s > 59 || p->h < 0 || p->i < 0 || p->s < 0) ){
		DtRecWarn(pRec,nLen + 1,"The parsed time was invalid");
	}
	if( p->bHaveDate
	 && (p->m < 1 || p->m > 12 || p->d < 1
	     || p->d > DtDaysInMonth(p->y,(int)p->m)) ){
		DtRecWarn(pRec,nLen + 1,"The parsed date was invalid");
	}
	if( iFirst != 0 ){
		return iFirst;
	}
	if( !bAny ){
		return 1;
	}
	return 0;
#undef DT_SKIP_WS
#undef DT_LOWEQ
#undef DT_WEEKSING
#undef DT_FAIL
}
/*
 * Parse zIn against the base moment and answer the timestamp it names. The
 * vector the string filled is applied here (DtApplyFields), so nothing about the
 * order the string spelled its units in reaches the clock.
 */
static int DtParseEx(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,int iBaseUs,
	int iFlags,sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs,dt_parsed *pVec,
	phl_dt_lasterr *pRec)
{
	dt_parsed sP;
	int iErr;
	DtFieldsInit(&sP,iBaseOff);
	*pUs = iBaseUs;
	if( pRec ){
		DtRecReset(pRec);
	}
	iErr = DtParseFields(zIn,nLen,&sP,pRec);
	if( pVec ){
		*pVec = sP;
	}
	if( iErr != 0 ){
		return iErr;
	}
	*pTs = DtApplyFields(&sP,iBaseTs,iBaseOff,iBaseUs,iFlags,pUs);
	*pOff = sP.iOff;
	*pbOffSet = sP.bOffSet;
	return 0;
}
static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,int iBaseUs,
	sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs)
{
	return DtParseEx(zIn,nLen,iBaseTs,iBaseOff,iBaseUs,0,pTs,pOff,pbOffSet,pUs,0,0);
}
/*
 * php's parse-failure reason, from DtParse's error code.
 *
 * The reason and the offending byte used to be formatted straight into the message
 * the `__dt_parse` thunk RETURNED as a string; the constructor also has to publish
 * them as getLastErrors()'s error map now, so the decision lives here.
 */
static const char * DtParseErr(const char *zIn,int nLen,int iErrPos,int *piPos,char *pcAt)
{
	/* Negative encodings: php's "Double time specification" reason, and -- one
	 * whole DT_ERR_RANGE band lower -- its "Number out of range", which is what a
	 * digit run too wide for the clock reports; then "Double date specification"
	 * and, lowest, "Double timezone specification". */
	int bRange = 0,bDDate = 0,bDZone = 0,bTzId = 0,bUnexp = 0,bEmpty = 0;
	int bDouble;
	int iPos;
	DtTrimEnds(&zIn,&nLen);   /* the position is php's, i.e. the trimmed string's */
	if( iErrPos < -DT_ERR_EMPTY ){
		bEmpty = 1;
		iErrPos += DT_ERR_EMPTY;
	}else if( iErrPos < -DT_ERR_UNEXPDATA ){
		bUnexp = 1;
		iErrPos += DT_ERR_UNEXPDATA;
	}else if( iErrPos < -DT_ERR_TZID ){
		bTzId = 1;
		iErrPos += DT_ERR_TZID;
	}else if( iErrPos < -DT_ERR_DZONE ){
		bDZone = 1;
		iErrPos += DT_ERR_DZONE;
	}else if( iErrPos < -DT_ERR_DDATE ){
		bDDate = 1;
		iErrPos += DT_ERR_DDATE;
	}else if( iErrPos < -DT_ERR_RANGE ){
		bRange = 1;
		iErrPos += DT_ERR_RANGE;
	}
	bDouble = !bRange && !bDDate && !bDZone && !bTzId && !bUnexp && !bEmpty
		&& iErrPos < 0;
	iPos = (iErrPos < 0 ? -iErrPos : iErrPos) - 1;
	char cAt = (iPos < nLen) ? zIn[iPos] : ' ';
	*piPos = iPos;
	*pcAt = cAt;
	/* php appends a reason: an alphabetic token is assumed to be a timezone
	 * lookup miss, anything else an unexpected character. */
	if( bRange ){
		return "Number out of range";
	}
	if( bDDate ){
		return "Double date specification";
	}
	if( bDZone ){
		return "Double timezone specification";
	}
	if( bTzId ){
		return "The timezone could not be found in the database";
	}
	if( bUnexp ){
		return "Found unexpected data";
	}
	if( bEmpty ){
		return "Empty string";
	}
	/* php's own parenthesized-zone token starts at the `(`, so a name it cannot
	 * find there is reported at the paren with the zone reason, not the byte. */
	if( cAt == '(' && iPos + 1 < nLen && SyisAlpha(zIn[iPos+1]) ){
		return "The timezone could not be found in the database";
	}
	return bDouble ? "Double time specification"
		: ((cAt >= 'a' && cAt <= 'z') || (cAt >= 'A' && cAt <= 'Z'))
			? "The timezone could not be found in the database"
			: "Unexpected character";
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
/*
 * php's DateTime::add/sub: month arithmetic with linear day/time overflow
 * (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset.
 */
static sxi64 DtCivilAdd(sxi64 iTs,sxi32 iOff,sxi64 y,sxi64 m,sxi64 d,
	sxi64 h,sxi64 i,sxi64 s,int iSign)
{
	sxi64 iLocal,iDays,iSecs,y0,moT,dayCount;
	int mo0,d0;
	iSign = iSign < 0 ? -1 : 1;
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
	return iLocal - iOff;
}
/* One DateInterval's worth of fields, as diff() computes them. */
typedef struct dt_diff dt_diff;
struct dt_diff
{
	sxi64 y,m,d,h,i,s,uSec,nDays;
	int bInvert;
};
/*
 * timelib's diff breakdown: field-wise deltas in the FIRST operand's offset, then
 * borrow seconds->minutes->hours->days, then borrow whole months for the day.
 *
 * That last borrow is ASYMMETRIC in php, and PHL answered the symmetric result: a
 * non-inverted diff walks months BACKWARD from the later date (which is why
 * Jan 31 -> Mar 02 reports m=0 d=30, not "1 month"), while an inverted one borrows
 * the month of the ORIGINAL first operand — the later date — walking forward. So
 * `$later->diff($earlier)` is not `$earlier->diff($later)` with the sign flipped:
 * php answers y=1 m=1 d=2 where PHL answered y=1 m=0 d=30. One iteration always
 * settles the inverted case: |d| < 31 and the borrowed month has at least 28 days,
 * while a 28-day base month can only be reached from a day-of-month <= 29.
 */
static void DtCivilDiff(sxi64 iTs1,int uSec1,sxi32 iOff1,sxi64 iTs2,int uSec2,sxi32 iOff2,
	int bSameZone,dt_diff *pOut)
{
	sxi64 iA,iB,iLa,iLb,daysA,daysB,yA,yB;
	sxi32 iOffA,iOffB;
	int moA,dA,moB,dB,bInvert,usA,usB;
	sxi64 sA,sB,y,m,d,h,i,s,us;
	/* The MICROSECONDS are part of which date comes first -- `$a->diff($b)` on two
	 * dates inside the same second is an INVERTED interval when $a is the later of
	 * them -- and their borrow is a whole second off the later date, so every field
	 * below and the day COUNT are computed from the borrowed instant: a difference
	 * of one microsecond less than a day is 23:59:59.999999 with days = 0, not a
	 * day. */
	bInvert = iTs1 > iTs2 || (iTs1 == iTs2 && uSec1 > uSec2);
	iA = bInvert ? iTs2 : iTs1;
	iB = bInvert ? iTs1 : iTs2;
	usA = bInvert ? uSec2 : uSec1;
	usB = bInvert ? uSec1 : uSec2;
	us = usB - usA;
	if( us < 0 ){
		us += 1000000;
		iB--;
	}
	/*
	 * WHICH CLOCK the two instants are read on, and php has two answers.
	 *
	 * When both dates are in the SAME zone -- both identifiers, spelled the same
	 * bytes -- each is read on its own, so `2010-01-01 00:00` and
	 * `2010-08-01 00:00` in New York are seven months apart exactly, with the
	 * hour daylight saving took not in the answer at all.
	 *
	 * Otherwise both are read on the EARLIER one's offset, which is timelib
	 * subtracting `two->z - one->z` from a field-wise difference and comes to
	 * the same thing. So the same pair with the second date spelled
	 * `america/new_york` -- the same place, a different spelling, and to php a
	 * different zone -- is seven months LESS AN HOUR. The comparison really is
	 * byte-exact: `US/Eastern` is the same data and not the same zone either.
	 *
	 * With no database in the build no zone's offset can vary, so the two arms
	 * agree and this is the single offset the code always applied.
	 */
	iOffA = bInvert ? iOff2 : iOff1;
	iOffB = bInvert ? iOff1 : iOff2;
	iLa = iA + iOffA;
	iLb = iB + (bSameZone ? iOffB : iOffA);
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
	if( bInvert ){
		while( d < 0 ){
			d += DtDaysInMonth(yA,moA);
			m--;
			moA++;
			if( moA > 12 ){ moA = 1; yA++; }
		}
	}else{
		while( d < 0 ){
			moB--;
			if( moB < 1 ){ moB = 12; yB--; }
			d += DtDaysInMonth(yB,moB);
			m--;
		}
	}
	if( m < 0 ){ m += 12; y--; }
	pOut->y = y;
	pOut->m = m;
	pOut->d = d;
	pOut->h = h;
	pOut->i = i;
	pOut->s = s;
	pOut->uSec = us;
	/* The day COUNT rides the same clock: two local noons a daylight switch
	 * apart are one day, not a day less an hour rounded down to zero. */
	pOut->nDays = (iLb - iLa) / 86400;
	pOut->bInvert = bInvert;
}
/*
 * setISODate: jump to an ISO year/week/weekday, preserving the time of day.
 */
static sxi64 DtIsoDate(sxi64 iTs,sxi32 iOff,sxi64 y,sxi64 w,sxi64 dow)
{
	sxi64 iLocal,iTod,jan4,monday1,target;
	int isoDow;
	iLocal = iTs + iOff;
	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;
	jan4 = DtDaysFromCivil(y,1,4);
	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;
	monday1 = jan4 - (isoDow - 1);
	target = monday1 + (w - 1)*7 + (dow - 1);
	return target*86400 + iTod - iOff;
}
/*
 * php's DateTime::createFromFormat engine.
 *
 * This was the `__dt_from_format()` thunk, whose answer had to survive a trip
 * through PHP: an ARRAY on success and a "COUNT\nPOS\tMESSAGE" string on failure,
 * which the chunk then re-parsed. Both encodings are gone — the native methods call
 * this directly and read the diagnostics as a struct. That is also why a parse that
 * has BOTH errors and warnings can now report both: the failure encoding had no room
 * for warnings, so php's `warning_count` was silently 0 whenever an error was present.
 *
 * Returns 0 when the parse produced a time and non-zero when it did not; pOut->sDiag
 * carries the warnings/errors either way (offKind: 0 none parsed, 1 numeric offset,
 * 2 literal Z, 3 named identifier).
 */
/*
 * Publish one scan's warnings and errors as the record getLastErrors() answers.
 * The messages are static literals, so the record copies pointers, never bytes.
 */
/*
 * Record one parse's diagnostics as getLastErrors()'s answer.
 *
 * php resets the record on EVERY constructor, modify() and createFromFormat()
 * call -- a clean parse answers `false` again -- and publishes what the scan
 * collected, which is every error it met rather than the one it stopped on.
 * The error rows grow with the string (one per byte at worst), the warnings
 * cannot exceed their own three rules, and every message is a static literal.
 */
static void DtRecReset(phl_dt_lasterr *pRec)
{
	pRec->bSet = 0;
	pRec->nWarn = pRec->nWarnKept = 0;
	pRec->nErr = pRec->nErrKept = 0;
	SyBlobReset(&pRec->sErr);
}
static void DtRecErr(phl_dt_lasterr *pRec,int iPos,const char *zMsg)
{
	phl_dt_diag_row sRow;
	if( pRec == 0 ){
		return;
	}
	pRec->bSet = 1;
	pRec->nErr++;
	sRow.iPos = iPos;
	sRow.zMsg = zMsg;
	if( SyBlobAppend(&pRec->sErr,(const void *)&sRow,sizeof(sRow)) == SXRET_OK ){
		pRec->nErrKept++;
	}
}
static void DtRecWarn(phl_dt_lasterr *pRec,int iPos,const char *zMsg)
{
	if( pRec == 0 ){
		return;
	}
	pRec->bSet = 1;
	pRec->nWarn++;
	if( pRec->nWarnKept < PH7_DT_MAX_WARN ){
		pRec->aWarnPos[pRec->nWarnKept] = iPos;
		pRec->azWarn[pRec->nWarnKept] = zMsg;
		pRec->nWarnKept++;
	}
}
static void DtLastErrClear(ph7_vm *pVm)
{
	DtRecReset(&pVm->sDtLastErr);
}
typedef struct dt_ff_diag dt_ff_diag;
struct dt_ff_diag
{
	int nErr,nErrKept;
	int aErrPos[PH7_DT_MAX_ERR];
	const char *azErr[PH7_DT_MAX_ERR];
	int nWarn;
	int aWarnPos[PH7_DT_MAX_WARN];
	const char *azWarn[PH7_DT_MAX_WARN];
};
static void DtFfDiag(dt_ff_diag *pDiag,int nErr,int nErrKept,const int *aErrPos,
	const char **azErr,int nWarn,const int *aWarnPos,const char **azWarn)
{
	int k;
	pDiag->nErr = nErr;
	pDiag->nErrKept = nErrKept;
	for( k = 0 ; k < nErrKept ; k++ ){
		pDiag->aErrPos[k] = aErrPos[k];
		pDiag->azErr[k] = azErr[k];
	}
	pDiag->nWarn = nWarn;
	for( k = 0 ; k < nWarn ; k++ ){
		pDiag->aWarnPos[k] = aWarnPos[k];
		pDiag->azWarn[k] = azWarn[k];
	}
}
/* ...poured into a record of the shape a string scan fills, so that both
 * readers of a format scan -- getLastErrors() and the component view -- show
 * it through the same presenter. */
static void DtFfDiagInto(phl_dt_lasterr *pRec,const dt_ff_diag *pDiag)
{
	int k;
	DtRecReset(pRec);
	for( k = 0 ; k < pDiag->nWarn ; k++ ){
		DtRecWarn(pRec,pDiag->aWarnPos[k],pDiag->azWarn[k]);
	}
	for( k = 0 ; k < pDiag->nErrKept ; k++ ){
		DtRecErr(pRec,pDiag->aErrPos[k],pDiag->azErr[k]);
	}
	pRec->nErr = pDiag->nErr;   /* php counts what it dropped too */
}
/* ...and the VM's own, which getLastErrors() answers from. */
static void DtLastErrFf(ph7_vm *pVm,const dt_ff_diag *pDiag)
{
	DtFfDiagInto(&pVm->sDtLastErr,pDiag);
}
/*
 * php's do_range_limit: carry *pa into *pb until *pa sits inside [iStart,iEnd).
 * Spelled the way php spells it, the arithmetic on a field nothing ever set
 * included -- an unset minute is just a very negative number to this code, and
 * what it carries into the hour is what a `z` beside a half-read clock shows.
 */
static void DtFfRangeLimit(sxi64 iStart,sxi64 iEnd,sxi64 iAdj,sxi64 *pa,sxi64 *pb)
{
	if( *pa < iStart ){
		sxi64 a1 = *pa + 1;
		*pb -= (iStart - a1) / iAdj + 1;
		*pa += iAdj * ((iStart - a1) / iAdj);
		*pa += iAdj;
	}
	if( *pa >= iEnd ){
		*pb += *pa / iAdj;
		*pa -= iAdj * (*pa / iAdj);
	}
}
/* ...and its day half, which walks whole months rather than dividing: one call
 * takes the day inside the current month or gives up at the end of a year, and
 * the caller runs it until it has nothing left to move. */
static int DtFfRangeLimitDays(sxi64 *py,sxi64 *pm,sxi64 *pd)
{
	int rc = 0;
	if( *pd >= 146097 || *pd <= -146097 ){
		/* a whole 400-year era at a time */
		*py += 400 * (*pd / 146097);
		*pd -= 146097 * (*pd / 146097);
	}
	DtFfRangeLimit(1,13,12,pm,py);
	while( *pd <= 0 && *pm > 0 ){
		sxi64 iPrevM = *pm - 1,iPrevY = *py;
		if( iPrevM < 1 ){
			iPrevM += 12;
			iPrevY = *py - 1;
		}
		*pd += DtDaysInMonth(iPrevY,(int)iPrevM);
		(*pm)--;
		rc = 1;
	}
	while( *pd > 0 && *pm >= 1 && *pm <= 12 && *pd > DtDaysInMonth(*py,(int)*pm) ){
		*pd -= DtDaysInMonth(*py,(int)*pm);
		(*pm)++;
		rc = 1;
	}
	return rc;
}
/* php's timelib_do_normalize, asked of the whole vector wherever a format's
 * day-of-year stands. The clock is only carried when the SECOND was read --
 * php's own guard, and not the one anybody would write. */
static void DtFfNormalize(sxi64 *py,sxi64 *pm,sxi64 *pd,sxi64 *ph,sxi64 *pi,
	sxi64 *ps,sxi64 *pus)
{
	if( *pus != DT_UNSET ){ DtFfRangeLimit(0,1000000,1000000,pus,ps); }
	if( *ps != DT_UNSET ){
		DtFfRangeLimit(0,60,60,ps,pi);
		DtFfRangeLimit(0,60,60,pi,ph);
		DtFfRangeLimit(0,24,24,ph,pd);
	}
	DtFfRangeLimit(1,13,12,pm,py);
	if( *py == 1970 && *pm == 1 ){
		/* php's short cut past the walk, straight off the epoch */
		sxi64 iY;
		int iM,iD;
		DtCivilFromDays(*pd - 1,&iY,&iM,&iD);
		*py = iY; *pm = iM; *pd = iD;
		return;
	}
	while( DtFfRangeLimitDays(py,pm,pd) ){}
	DtFfRangeLimit(1,13,12,pm,py);
}
/* strtol over a bounded run: it reads the digits it finds and stops at the
 * first byte that is not one, which is how php's offset arithmetic reads each
 * group of a colon spelling out of the middle of the run. */
static sxi64 DtFfZoneNum(const char *z,const char *zEnd)
{
	sxi64 v = 0;
	while( z < zEnd && SyisDigit(z[0]) ){
		v = v*10 + (z[0] - '0');
		z++;
	}
	return v;
}
/*
 * php's timelib_parse_tz_cor, the digits behind a format zone's sign. It takes
 * the whole run of digits and colons and then decides what the run MEANT from
 * its length alone, which is why `+9999` is 99 hours and 99 minutes and `+2460`
 * is 24 hours and 60: nothing here is in range of anything. A length the switch
 * does not name is no offset at all. Answers 1 when the run spelled one.
 */
static int DtFfZoneCor(const char **pz,const char *zEnd,sxi32 *piOff)
{
	const char *z = *pz,*zBeg = *pz;
	int n;
	sxi64 v;
	while( z < zEnd && (SyisDigit(z[0]) || z[0] == ':') ){ z++; }
	n = (int)(z - zBeg);
	*pz = z;
	*piOff = 0;
	switch( n ){
	case 1: case 2:
		*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600);
		return 1;
	case 3: case 4:
		if( zBeg[1] == ':' ){
			*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600
				+ DtFfZoneNum(&zBeg[2],zEnd) * 60);
		}else if( zBeg[2] == ':' ){
			*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600
				+ DtFfZoneNum(&zBeg[3],zEnd) * 60);
		}else{
			v = DtFfZoneNum(zBeg,zEnd);
			*piOff = (sxi32)((v / 100) * 3600 + (v % 100) * 60);
		}
		return 1;
	case 5:
		if( zBeg[2] != ':' ){ break; }
		*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600
			+ DtFfZoneNum(&zBeg[3],zEnd) * 60);
		return 1;
	case 6:
		v = DtFfZoneNum(zBeg,zEnd);
		*piOff = (sxi32)((v / 10000) * 3600 + ((v / 100) % 100) * 60 + (v % 100));
		return 1;
	case 8:
		if( zBeg[2] != ':' || zBeg[5] != ':' ){ break; }
		*piOff = (sxi32)(DtFfZoneNum(zBeg,zEnd) * 3600
			+ DtFfZoneNum(&zBeg[3],zEnd) * 60 + DtFfZoneNum(&zBeg[6],zEnd));
		return 1;
	default:
		break;
	}
	return 0;
}
/*
 * php's timelib_parse_zone, and there is no SHAPE to match here the way the
 * string scanner matches one: a format's zone specifier reads whatever stands
 * at the cursor. Blanks and opening parens go first, an uppercase `GMT` in
 * front of a sign is dropped, a sign is a UTC OFFSET whatever follows it, and
 * anything else is a NAME taken to the end of its run -- letters, digits, `/`,
 * `_`, `+` and `-` all belong to it, which is why `gmt+3` is one unknown word
 * where `GMT+3` is three hours.
 *
 * A sign settles the KIND before the digits are read, so an offset nothing
 * follows is still an offset -- of zero, with a refusal beside it. Answers 1
 * when the zone resolved, 0 when it did not.
 */
static int DtFfZone(const char **pz,const char *zEnd,int *piKind,sxi32 *piOff,
	const char **pzName,int *pnName)
{
	const char *z = *pz;
	int nPar = 0,bNeg,bIdent = 0,rc;
	/* The OFFSET is written whatever happens -- php assigns the reader's answer,
	 * which is zero when it resolved nothing -- while the KIND and the NAME are
	 * touched only by a zone that DID resolve. So a second specifier that finds
	 * nothing zeroes the offset the first one read and leaves its kind standing. */
	*piOff = 0;
	while( z < zEnd && (z[0] == ' ' || z[0] == '\t' || z[0] == '(') ){
		if( z[0] == '(' ){ nPar++; }
		z++;
	}
	if( zEnd - z > 3 && SyMemcmp(z,"GMT",3) == 0 && (z[3] == '+' || z[3] == '-') ){
		z += 3;
	}
	if( z < zEnd && (z[0] == '+' || z[0] == '-') ){
		bNeg = (z[0] == '-');
		z++;
		*piKind = DT_ZONE_OFFSET;
		rc = DtFfZoneCor(&z,zEnd,piOff);
		if( bNeg ){ *piOff = -*piOff; }
		*pz = z;
		return rc;
	}
	{
		const char *zWord = z;
		int nWord;
		while( z < zEnd && (SyisAlphaNum((unsigned char)z[0]) || z[0] == '/'
		 || z[0] == '_' || z[0] == '-' || z[0] == '+') ){
			z++;
		}
		nWord = (int)(z - zWord);
		rc = nWord > 0 && DtZoneName(zWord,nWord,piOff,pzName,pnName,&bIdent);
		if( rc ){
			*piKind = bIdent ? DT_ZONE_ID : DT_ZONE_ABBR;
		}
		while( nPar > 0 && z < zEnd && z[0] == ')' ){
			z++;
			nPar--;
		}
		*pz = z;
		return rc;
	}
}
/*
 * php's timelib_get_nr, the reader behind every plain digit field of a format:
 * it steps over whatever is NOT a digit -- to the end of the input if it has
 * to -- and then takes at most nMax of them. Answers how many digits it took,
 * or -1 when the input ran out before it found one; the cursor moves either way.
 */
static int DtFfGetNr(const char **pz,const char *zEnd,int nMax,sxi64 *pVal)
{
	const char *z = *pz;
	sxi64 v = 0;
	int n = 0;
	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }
	if( z >= zEnd ){
		*pz = z;
		return -1;
	}
	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){
		v = v*10 + (z[0] - '0');
		z++;
		n++;
	}
	*pz = z;
	*pVal = v;
	return n;
}
/*
 * php's timelib_get_signed_nr, which `U` reads through: it steps over anything
 * that is neither a digit nor a sign, takes a RUN of signs (each minus flipping
 * it), steps over non-digits again, and reads at most nMax digits. Its two ways
 * of giving up -- an input that ends before a digit, and a value no int64 can
 * hold -- are refusals php raises through its STRING scanner's door rather than
 * the format one's, so both are reported at position 0 whatever the format was
 * doing, and both answer zero.
 */
static int DtFfGetSignedNr(const char **pz,const char *zEnd,int nMax,sxi64 *pVal,
	const char **pzErr)
{
	const char *z = *pz;
	sxu64 u = 0,uLimit;
	int bNeg = 0,n = 0,bOver = 0;
	*pzErr = 0;
	*pVal = 0;
	while( z < zEnd && !SyisDigit(z[0]) && z[0] != '+' && z[0] != '-' ){ z++; }
	if( z >= zEnd ){
		*pz = z;
		*pzErr = "Found unexpected data";
		return 0;
	}
	while( z < zEnd && (z[0] == '+' || z[0] == '-') ){
		if( z[0] == '-' ){ bNeg = !bNeg; }
		z++;
	}
	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }
	if( z >= zEnd ){
		*pz = z;
		*pzErr = "Found unexpected data";
		return 0;
	}
	/* php's ceiling is strtoll's, so the negative side reaches one further */
	uLimit = bNeg ? ((sxu64)SXI64_HIGH + 1) : (sxu64)SXI64_HIGH;
	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){
		sxu64 dg = (sxu64)(z[0] - '0');
		if( u > (uLimit - dg) / 10 ){
			bOver = 1;
		}
		if( !bOver ){
			u = u*10 + dg;
		}
		z++;
		n++;
	}
	*pz = z;
	if( bOver ){
		*pzErr = "Number out of range";
		return 0;
	}
	/* the negation is spelled unsigned: the floor has no positive twin */
	*pVal = bNeg ? (sxi64)(0 - u) : (sxi64)u;
	return 1;
}
/*
 * php's MONTH table, which a format matches as a whole WORD: the letters are
 * taken to the end of their run and the run has to spell one of the names
 * exactly, so `janx` is no month at all where the string parser reads January
 * out of it. The names are php's own -- three letters, `sept`, the full months,
 * and the ROMAN numerals `i` through `xii`, which is why a format's `F` reads
 * `x` as October. Advances *pz over the run whether or not it spelled one.
 */
static int DtFfMonth(const char **pz,const char *zEnd)
{
	static const struct { const char *z; int n; int mo; } aM[] = {
		{ "jan",3,1 },{ "feb",3,2 },{ "mar",3,3 },{ "apr",3,4 },{ "may",3,5 },
		{ "jun",3,6 },{ "jul",3,7 },{ "aug",3,8 },{ "sep",3,9 },{ "sept",4,9 },
		{ "oct",3,10 },{ "nov",3,11 },{ "dec",3,12 },
		{ "i",1,1 },{ "ii",2,2 },{ "iii",3,3 },{ "iv",2,4 },{ "v",1,5 },
		{ "vi",2,6 },{ "vii",3,7 },{ "viii",4,8 },{ "ix",2,9 },{ "x",1,10 },
		{ "xi",2,11 },{ "xii",3,12 },
		{ "january",7,1 },{ "february",8,2 },{ "march",5,3 },{ "april",5,4 },
		{ "june",4,6 },{ "july",4,7 },{ "august",6,8 },{ "september",9,9 },
		{ "october",7,10 },{ "november",8,11 },{ "december",8,12 }
	};
	const char *z = *pz,*zWord = *pz;
	sxu32 i;
	int n;
	while( z < zEnd && SyisAlpha((unsigned char)z[0]) ){ z++; }
	n = (int)(z - zWord);
	*pz = z;
	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){
		if( aM[i].n == n && SyStrnicmp(zWord,aM[i].z,(sxu32)n) == 0 ){
			return aM[i].mo;
		}
	}
	return 0;
}
/*
 * ...and php's RELATIVE-UNIT table, which is where a format's textual DAY is
 * looked up. `D` and `l` do not read a weekday name at all there: they read a
 * word up to the next separator and ask the relative-unit table what it is, so
 * every unit spelling answers one -- `week` is the weekday 7 and `ms` the
 * weekday 1000, neither of which is a day of any week. Advances *pz over the
 * word whether or not it spelled one; answers 1 and fills *piWday when it did.
 */
static int DtFfRelunit(const char **pz,const char *zEnd,sxi64 *piWday)
{
	static const struct { const char *z; int n; int mul; } aU[] = {
		{ "ms",2,1000 },{ "msec",4,1000 },{ "msecs",5,1000 },
		{ "millisecond",11,1000 },{ "milliseconds",12,1000 },
		{ "\xc2\xb5s",3,1 },{ "usec",4,1 },{ "usecs",5,1 },
		{ "\xc2\xb5sec",5,1 },{ "\xc2\xb5secs",6,1 },
		{ "microsecond",11,1 },{ "microseconds",12,1 },
		{ "sec",3,1 },{ "secs",4,1 },{ "second",6,1 },{ "seconds",7,1 },
		{ "min",3,1 },{ "mins",4,1 },{ "minute",6,1 },{ "minutes",7,1 },
		{ "hour",4,1 },{ "hours",5,1 },
		{ "day",3,1 },{ "days",4,1 },
		{ "week",4,7 },{ "weeks",5,7 },
		{ "fortnight",9,14 },{ "fortnights",10,14 },
		{ "forthnight",10,14 },{ "forthnights",11,14 },
		{ "month",5,1 },{ "months",6,1 },
		{ "year",4,1 },{ "years",5,1 },
		{ "mondays",7,1 },{ "monday",6,1 },{ "mon",3,1 },
		{ "tuesdays",8,2 },{ "tuesday",7,2 },{ "tue",3,2 },
		{ "wednesdays",10,3 },{ "wednesday",9,3 },{ "wed",3,3 },
		{ "thursdays",9,4 },{ "thursday",8,4 },{ "thu",3,4 },
		{ "fridays",7,5 },{ "friday",6,5 },{ "fri",3,5 },
		{ "saturdays",9,6 },{ "saturday",8,6 },{ "sat",3,6 },
		{ "sundays",7,0 },{ "sunday",6,0 },{ "sun",3,0 },
		{ "weekday",7,1 },{ "weekdays",8,1 }
	};
	const char *z = *pz,*zWord = *pz;
	sxu32 i;
	int n;
	while( z < zEnd && z[0] != ' ' && z[0] != ',' && z[0] != '\t' && z[0] != ';'
	 && z[0] != ':' && z[0] != '/' && z[0] != '.' && z[0] != '-'
	 && z[0] != '(' && z[0] != ')' ){
		z++;
	}
	n = (int)(z - zWord);
	*pz = z;
	for( i = 0 ; i < SX_ARRAYSIZE(aU) ; ++i ){
		if( aU[i].n == n && SyStrnicmp(zWord,aU[i].z,(sxu32)n) == 0 ){
			*piWday = aU[i].mul;
			return 1;
		}
	}
	return 0;
}
/*
 * php's meridian, which is an ADJUSTMENT to whatever hour was already read
 * rather than a reading of its own: `am` takes noon back to midnight and leaves
 * every other hour standing, `pm` adds twelve to all but twelve itself.
 *
 * It hunts for its own letter -- anything that is not one of `AaPp` is stepped
 * over, so `1 xx pm` is one in the afternoon -- and then wants either a bare
 * `m` or the whole `.m.`; the cursor stays wherever the spelling ran out when
 * it turns out to be neither. Answers 1 and fills *piAdj, or 0.
 */
static int DtFfMeridian(const char **pz,const char *zEnd,sxi64 h,sxi64 *piAdj)
{
	const char *z = *pz;
	int bAm;
	while( z < zEnd && z[0] != 'A' && z[0] != 'a' && z[0] != 'P' && z[0] != 'p' ){
		z++;
	}
	if( z >= zEnd ){
		*pz = z;
		return 0;
	}
	bAm = (z[0] == 'a' || z[0] == 'A');
	*piAdj = bAm ? ((h == 12) ? -12 : 0) : ((h != 12) ? 12 : 0);
	z++;
	if( z < zEnd && z[0] == '.' ){
		z++;
		if( z >= zEnd || (z[0] != 'm' && z[0] != 'M') ){ *pz = z; return 0; }
		z++;
		if( z >= zEnd || z[0] != '.' ){ *pz = z; return 0; }
		z++;
	}else if( z < zEnd && (z[0] == 'm' || z[0] == 'M') ){
		z++;
	}else{
		*pz = z;
		return 0;
	}
	*pz = z;
	return 1;
}
/*
 * The eight bytes php's format map calls SEPARATORS: what `#` accepts, and what
 * each of them demands of the input when it stands in a format itself.
 */
static int DtFfIsSep(int c)
{
	return c==';' || c==':' || c=='/' || c=='.' || c==',' || c=='-'
		|| c=='(' || c==')';
}
/*
 * php's run of blanks -- the two ASCII ones and the two Unicode spaces its
 * scanner spells out. A format space eats the whole run and never refuses, so a
 * space beside an input that has none is simply nothing.
 */
static void DtFfEatSpaces(const char **pz,const char *zEnd)
{
	const char *z = *pz;
	for(;;){
		if( z < zEnd && (z[0] == ' ' || z[0] == '\t') ){
			z++;
			continue;
		}
		if( zEnd - z >= 3 && (unsigned char)z[0] == 0xE2
		 && (unsigned char)z[1] == 0x80 && (unsigned char)z[2] == 0xAF ){
			z += 3;    /* NARROW NO-BREAK SPACE */
			continue;
		}
		if( zEnd - z >= 2 && (unsigned char)z[0] == 0xC2
		 && (unsigned char)z[1] == 0xA0 ){
			z += 2;    /* NO-BREAK SPACE */
			continue;
		}
		break;
	}
	*pz = z;
}
/*
 * What one run of the FORMAT scanner read, field by field.
 *
 * php's format parser starts every field UNSET and never consults the clock: the
 * struct below is what the scan itself put there, so a format that named no year
 * leaves `y` unset rather than this year's. The moment a DateTime wants is built
 * from it afterwards (DtFfResolve), which is where the current instant finally
 * fills what the format never mentioned -- php's own timelib_fill_holes, run once
 * the scan is over rather than while it is going on.
 */
typedef struct dt_ff_res dt_ff_res;
struct dt_ff_res
{
	sxi64 y,mo,d,h,mi,s,us;   /* DT_UNSET == php's TIMELIB_UNSET */
	sxi32 iOff;
	int bLocal;               /* php's is_localtime -- a zone was READ */
	int iOffKind;             /* ...and php's zone_type, 0 when it meant nothing */
	const char *zName;        /* a static literal, as every zone name here is */
	int nName;
	int bWday;                /* php's relative.have_weekday_relative */
	sxi64 iWday;
	dt_ff_diag sDiag;
};
static int DtFromFormat(const char *zFmt,int nFmt,const char *zIn,int nIn,
	dt_ff_res *pOut)
{
	const char *zEnd,*zInEnd,*z;
	sxi64 v;
	sxi64 y = DT_UNSET,mo = DT_UNSET,d = DT_UNSET;
	sxi64 h = DT_UNSET,mi = DT_UNSET,s = DT_UNSET,us = DT_UNSET;
	sxi64 uVal = 0;
	int bPlus = 0,bLocal = 0;
	int bWday = 0;
	sxi64 iWday = 0;
	int iOffKind = 0,nName = 0;
	sxi32 iOffVal = 0;
	const char *zName = 0;
	const char *zErr = 0;
	const char *aWarnMsg[PH7_DT_MAX_WARN];
	int aWarnPos[PH7_DT_MAX_WARN];
	int nWarn = 0;
	const char *aErrMsg[PH7_DT_MAX_ERR];
	int aErrPos[PH7_DT_MAX_ERR];
	int nErr = 0,nErrKept = 0;
	SyZero(pOut,sizeof(*pOut));
	/* Both strings end where php's C string ends: a NUL inside a format simply
	 * truncates it, and one inside the input ends the scan there. */
	zEnd = &zFmt[DtCStrLen(zFmt,nFmt)];
	zInEnd = &zIn[DtCStrLen(zIn,nIn)];
	z = zIn;
#define DT_FF_LOGERR(iPos,zMsg) \
	{ int _p = (iPos),_k,_f = -1; \
	  nErr++; \
	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \
	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \
	  else if( nErrKept < PH7_DT_MAX_ERR ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }
/* php's two RESET specifiers act where they stand rather than at the end of the
 * scan: `!` puts every field at its 1970 default whatever the format already
 * read, `|` only fills what nothing has read yet, and a specifier after either
 * one overwrites what it left. */
#define DT_FF_RESET(bUnsetOnly) \
	{ int _u = (bUnsetOnly); \
	  if( !_u || y  == DT_UNSET ){ y  = 1970; } \
	  if( !_u || mo == DT_UNSET ){ mo = 1; } \
	  if( !_u || d  == DT_UNSET ){ d  = 1; } \
	  if( !_u || h  == DT_UNSET ){ h  = 0; } \
	  if( !_u || mi == DT_UNSET ){ mi = 0; } \
	  if( !_u || s  == DT_UNSET ){ s  = 0; } \
	  if( !_u || us == DT_UNSET ){ us = 0; } }
	/* The scan runs while BOTH strings still have something in them: php's
	 * format loop ends the moment the input does, and what is left of the
	 * format is judged afterwards rather than refused here. */
	while( zFmt < zEnd && z < zInEnd ){
		char c = zFmt[0];
		/* every refusal below reports the byte the specifier STARTED on, not
		 * wherever the reading of it gave up */
		int iBegin = (int)(z - zIn);
		zFmt++;
		zErr = 0;
		if( c == '!' ){
			DT_FF_RESET(0);
			continue;
		}
		if( c == '|' ){
			DT_FF_RESET(1);
			continue;
		}
		if( c == '+' ){ bPlus = 1; continue; }
/* php asks of every digit field, BEFORE reading it, whether the cursor is on a
 * digit at all -- and merely says so: the reader that follows hunts for its
 * digits regardless, so `x5` is the day 5 with one refusal behind it. */
#define DT_FF_CHECKNUM \
	if( !SyisDigit(z[0]) ){ DT_FF_LOGERR(iBegin,"Unexpected data found."); }
#define DT_FF_CHECKSIGNED \
	if( !SyisDigit(z[0]) && z[0] != '+' && z[0] != '-' ){ \
		DT_FF_LOGERR(iBegin,"Unexpected data found."); }
		switch( c ){
		case 'd': case 'j':
			DT_FF_CHECKNUM;
			if( DtFfGetNr(&z,zInEnd,2,&d) < 0 ){
				DT_FF_LOGERR(iBegin,"A two digit day could not be found");
				d = DT_UNSET;
			}
			break;
		case 'D': case 'l':
			/* php's textual day is a RELATIVE weekday, not decoration: it moves
			 * the date it was read beside, forward to that weekday and keeping a
			 * day that already matches. Both spellings read the same table --
			 * the three-letter and the full name are one rule there. */
			if( DtFfRelunit(&z,zInEnd,&iWday) ){
				bWday = 1;
			}else{
				zErr = "A textual day could not be found";
			}
			break;
		case 'z':
			/* php's DAY OF YEAR is a whole date rather than a field: it needs a
			 * year already read, puts the month back at January and the day at
			 * the count, and normalizes the vector where it stands. */
			DT_FF_CHECKNUM;
			if( y == DT_UNSET ){
				DT_FF_LOGERR(iBegin,"A 'day of year' can only come after a year has been found");
			}
			if( DtFfGetNr(&z,zInEnd,3,&v) < 0 ){
				DT_FF_LOGERR(iBegin,"A three digit day-of-year could not be found");
				break;
			}
			if( y != DT_UNSET ){
				mo = 1;
				d = v + 1;
				DtFfNormalize(&y,&mo,&d,&h,&mi,&s,&us);
			}
			break;
		case 'x': case 'X':{
			/* the EXPANDED year: a sign and up to nineteen digits, and the year
			 * php takes from a run it could not read is zero rather than none. */
			const char *zNrErr = 0;
			DT_FF_CHECKSIGNED;
			DtFfGetSignedNr(&z,zInEnd,19,&y,&zNrErr);
			if( zNrErr ){
				DT_FF_LOGERR(0,zNrErr);
			}
			break;
				 }
		case 'S':
			/* the ordinal suffix, which php declines to look at when the cursor
			 * is on a blank and otherwise takes in either case */
			if( !SyisSpace((unsigned char)z[0]) && zInEnd-z >= 2
			 && (SyStrnicmp(z,"st",2) == 0 || SyStrnicmp(z,"nd",2) == 0
			  || SyStrnicmp(z,"rd",2) == 0 || SyStrnicmp(z,"th",2) == 0) ){
				z += 2;
			}
			break;
		case 'm': case 'n':
			DT_FF_CHECKNUM;
			if( DtFfGetNr(&z,zInEnd,2,&mo) < 0 ){
				DT_FF_LOGERR(iBegin,"A two digit month could not be found");
				mo = DT_UNSET;
			}
			break;
		case 'M': case 'F':{
			int k;
			k = DtFfMonth(&z,zInEnd);
			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }
			break;
				 }
		case 'y':
			DT_FF_CHECKNUM;
			if( DtFfGetNr(&z,zInEnd,2,&y) < 0 ){
				DT_FF_LOGERR(iBegin,"A two digit year could not be found");
				y = DT_UNSET;
			}else if( y < 100 ){
				/* php's two-digit century, which cuts at seventy */
				y += (y < 70) ? 2000 : 1900;
			}
			break;
		case 'Y':
			DT_FF_CHECKNUM;
			if( DtFfGetNr(&z,zInEnd,4,&y) < 0 ){
				DT_FF_LOGERR(iBegin,"A four digit year could not be found");
				y = DT_UNSET;
			}
			break;
		case 'H': case 'G':
			DT_FF_CHECKNUM;
			if( DtFfGetNr(&z,zInEnd,2,&h) < 0 ){
				DT_FF_LOGERR(iBegin,"A two digit hour could not be found");
				h = DT_UNSET;
			}
			break;
		case 'h': case 'g':
			DT_FF_CHECKNUM;
			if( DtFfGetNr(&z,zInEnd,2,&h) < 0 ){
				DT_FF_LOGERR(iBegin,"A two digit hour could not be found");
				h = DT_UNSET;
			}else if( h > 12 ){
				/* the twelve-hour spellings refuse a bigger one -- and keep it */
				DT_FF_LOGERR(iBegin,"Hour cannot be higher than 12");
			}
			break;
		case 'i': case 's':{
			/* the minute and the second are php's only EXACTLY two-digit
			 * fields: a lone digit is no minute there, however many follow
			 * it -- and a reading that fails leaves whatever was read before */
			sxi64 t = 0;
			DT_FF_CHECKNUM;
			if( DtFfGetNr(&z,zInEnd,2,&t) != 2 ){
				DT_FF_LOGERR(iBegin,c == 'i'
					? "A two digit minute could not be found"
					: "A two digit second could not be found");
			}else if( c == 'i' ){
				mi = t;
			}else{
				s = t;
			}
			break;
				 }
		case 'u': case 'v':{
			/* the fraction is scaled by what the READER walked, not by the
			 * digits it found: the bytes it stepped over hunting for them count
			 * against the width too, so `x1` under `u` is a hundredth */
			const char *zStart = z;
			int nMax = (c == 'u') ? 6 : 3;
			DT_FF_CHECKNUM;
			if( DtFfGetNr(&z,zInEnd,nMax,&v) < 0 ){
				DT_FF_LOGERR(iBegin,c == 'u'
					? "A six digit microsecond could not be found"
					: "A three digit millisecond could not be found");
				break;
			}
			{
				/* ...and BOTH spellings land on the same scale, because php
				 * multiplies the millisecond by a thousand after dividing it by
				 * the width: what either one answers is the digits it read
				 * times ten to the six-minus-bytes-walked, and a reader that
				 * walked more than six bytes answers a truncated fraction. */
				int k = 6 - (int)(z - zStart);
				while( k > 0 ){ v *= 10; k--; }
				while( k < 0 ){ v /= 10; k++; }
				us = v;
			}
			break;
				 }
		case 'a': case 'A':{
			sxi64 iAdj = 0;
			if( h == DT_UNSET ){
				DT_FF_LOGERR(iBegin,"Meridian can only come after an hour has been found");
			}
			if( !DtFfMeridian(&z,zInEnd,h,&iAdj) ){
				zErr = "A meridian could not be found";
			}else if( h != DT_UNSET ){
				h += iAdj;
			}
			break;
				 }
		case 'U':{
			/* php's epoch seconds are not a field but a whole MOMENT: it spreads
			 * the timestamp back over y/m/d/h/i/s at UTC right here, so a
			 * meridian behind one has an hour to move and a `Y` behind one
			 * overwrites the year it just wrote. The microseconds are the one
			 * part it does not touch. */
			const char *zNrErr = 0;
			Sytm sTm;
			DT_FF_CHECKSIGNED;
			DtFfGetSignedNr(&z,zInEnd,24,&uVal,&zNrErr);
			if( zNrErr ){
				/* php reports these at position 0 and takes the zero anyway */
				DT_FF_LOGERR(0,zNrErr);
			}
			DtFillSytm(uVal,0,0,&sTm);
			y = sTm.tm_year; mo = sTm.tm_mon + 1; d = sTm.tm_mday;
			h = sTm.tm_hour; mi = sTm.tm_min; s = sTm.tm_sec;
			bLocal = 1;
			iOffKind = DT_ZONE_OFFSET;
			iOffVal = 0;
			zName = 0;
			nName = 0;
			break;
				 }
		case 'e': case 'T': case 'P': case 'p': case 'O':
			/* php's five zone specifiers are ONE rule, and it is the whole of
			 * timelib_parse_zone rather than the shape each letter is named
			 * after: `O` reads `UTC` and `e` reads `+02:00`. The zone is LOCAL
			 * from here whatever the answer -- only the KIND is left at zero
			 * when the name meant nothing. */
			bLocal = 1;
			if( !DtFfZone(&z,zInEnd,&iOffKind,&iOffVal,&zName,&nName) ){
				zErr = "The timezone could not be found in the database";
			}
			break;
		case '?':
			z++;
			break;
		case '*':
			/* php's "skip to a separator": one byte goes whatever it is, and the
			 * run after it stops at a blank, a digit or one of `.,:;/-`. The
			 * parens are NOT in that set, though every other rule here treats
			 * them as separators. */
			z++;
			while( z < zInEnd && z[0] != ' ' && z[0] != '\t' && z[0] != '.'
			 && z[0] != ',' && z[0] != ':' && z[0] != ';' && z[0] != '/'
			 && z[0] != '-' && !SyisDigit(z[0]) ){
				z++;
			}
			break;
		case '#':
			if( DtFfIsSep((unsigned char)z[0]) ){
				z++;
			}else{
				zErr = "The separation symbol ([;:/.,-]) could not be found";
			}
			break;
		case '\\':
			/* the escape takes the NEXT format byte literally, and refuses on
			 * its own account when the format ends before there is one */
			if( zFmt >= zEnd ){
				zErr = "Escaped character expected";
				break;
			}
			if( z[0] == zFmt[0] ){
				z++;
			}else{
				zErr = "The escaped character could not be found";
			}
			zFmt++;
			break;
		case ';': case ':': case '/': case '.': case ',': case '-':
		case '(' : case ')':
			/* a separator in the format wants exactly that byte; a mismatch is
			 * ONE refusal, and the input byte stays where it is */
			if( z[0] == c ){
				z++;
			}else{
				zErr = "The separation symbol could not be found";
			}
			break;
		case ' ':
			DtFfEatSpaces(&z,zInEnd);
			break;
		default:
			/* any other format byte must match the input verbatim -- and php
			 * steps over the input byte either way, so a mismatch costs one
			 * refusal and the two strings carry on in step */
			if( z[0] != c ){
				DT_FF_LOGERR(iBegin,"The format separator does not match");
			}
			z++;
			break;
		}
		if( zErr ){
			/* name/zone/separator mismatch: log and keep scanning (timelib) */
			DT_FF_LOGERR(iBegin,zErr);
		}
	}
	if( z < zInEnd ){
		if( bPlus ){
			/* '+' downgrades trailing data to a warning */
			aWarnPos[nWarn] = (int)(z - zIn);
			aWarnMsg[nWarn] = "Trailing data";
			nWarn++;
		}else{
			DT_FF_LOGERR((int)(z - zIn),"Trailing data");
		}
	}
	/* ...and what is left of a FORMAT the input ran out under. The two reset
	 * specifiers and `+` need no input and are allowed to stand there; the
	 * first specifier that does want input is one refusal, and the rest of the
	 * format is never looked at. */
	while( zFmt < zEnd ){
		char c = zFmt[0];
		zFmt++;
		if( c == '!' ){
			DT_FF_RESET(0);
		}else if( c == '|' ){
			DT_FF_RESET(1);
		}else if( c != '+' ){
			DT_FF_LOGERR((int)(z - zIn),"Not enough data available to satisfy format");
			break;
		}
	}
	/* php's own clean-up: naming ANY part of the clock puts the rest of it at
	 * zero, so a format that read only the minute is that minute past midnight
	 * rather than past the current hour. */
	if( h != DT_UNSET || mi != DT_UNSET || s != DT_UNSET || us != DT_UNSET ){
		if( h == DT_UNSET ){ h = 0; }
		if( mi == DT_UNSET ){ mi = 0; }
		if( s == DT_UNSET ){ s = 0; }
		if( us == DT_UNSET ){ us = 0; }
	}
	/* ...and the two validity WARNINGS, each asked only of a whole component
	 * the scan actually filled, at wherever in the input the scan stopped. */
	if( h != DT_UNSET && mi != DT_UNSET && s != DT_UNSET
	 && (h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) ){
		if( nWarn < PH7_DT_MAX_WARN ){
			aWarnPos[nWarn] = (int)(z - zIn);
			aWarnMsg[nWarn] = "The parsed time was invalid";
			nWarn++;
		}
	}
	if( y != DT_UNSET && mo != DT_UNSET && d != DT_UNSET
	 && (mo < 1 || mo > 12 || d < 1 || d > DtDaysInMonth(y,(int)mo)) ){
		if( nWarn < PH7_DT_MAX_WARN ){
			aWarnPos[nWarn] = (int)(z - zIn);
			aWarnMsg[nWarn] = "The parsed date was invalid";
			nWarn++;
		}
	}
	pOut->y = y; pOut->mo = mo; pOut->d = d;
	pOut->h = h; pOut->mi = mi; pOut->s = s; pOut->us = us;
	pOut->iOff = iOffVal;
	pOut->bLocal = bLocal;
	pOut->iOffKind = iOffKind;
	pOut->zName = zName;
	pOut->nName = nName;
	pOut->bWday = bWday;
	pOut->iWday = iWday;
	DtFfDiag(&pOut->sDiag,nErr,nErrKept,aErrPos,aErrMsg,nWarn,aWarnPos,aWarnMsg);
	return nErr > 0 ? -1 : 0;
#undef DT_FF_CHECKSIGNED
#undef DT_FF_CHECKNUM
#undef DT_FF_RESET
#undef DT_FF_LOGERR
}
/*
 * php's timelib_fill_holes and timelib_update_ts, the step between the scan
 * above and the moment a DateTime carries.
 *
 * Everything the format never named comes from the current instant -- in the
 * zone the call was given, because php builds its `now` there -- and the
 * relative WEEKDAY a textual day left behind moves the resulting date forward
 * to that weekday, a day that already matches counting as a match.
 */
static sxi64 DtFfResolve(const dt_ff_res *pRes,sxi64 iNow,int iNowUs,
	sxi32 iDefOff,int *piUs)
{
	sxi64 y = pRes->y,mo = pRes->mo,d = pRes->d;
	sxi64 h = pRes->h,mi = pRes->mi,s = pRes->s,us = pRes->us;
	sxi64 iLocal = iNow + iDefOff;
	sxi64 days = DtFloorDiv(iLocal,86400);
	sxi64 secs = iLocal - days*86400;
	sxi64 ny;
	int nmo,nd;
	DtCivilFromDays(days,&ny,&nmo,&nd);
	if( us == DT_UNSET ){
		/* php reads the microseconds off the clock only for a format that read
		 * no part of the moment at all. */
		us = (y != DT_UNSET || mo != DT_UNSET || d != DT_UNSET
		   || h != DT_UNSET || mi != DT_UNSET || s != DT_UNSET) ? 0 : iNowUs;
	}
	if( y == DT_UNSET ){ y = ny; }
	if( mo == DT_UNSET ){ mo = nmo; }
	if( d == DT_UNSET ){ d = nd; }
	if( h == DT_UNSET ){ h = secs / 3600; }
	if( mi == DT_UNSET ){ mi = (secs / 60) % 60; }
	if( s == DT_UNSET ){ s = secs % 60; }
	/* php normalizes the filled vector BEFORE it hunts for a weekday and again
	 * afterwards, and its month carry is a CALENDAR one -- the fortieth month
	 * of 1970 is April 1973, not forty thirty-day steps from January. */
	DtFfNormalize(&y,&mo,&d,&h,&mi,&s,&us);
	if( pRes->bWday ){
		/* php's forward hunt, and it is a DIFFERENCE rather than a remainder:
		 * a weekday the relative-unit table answers past six -- `week` is 7 --
		 * moves the date by that much more. */
		sxi64 iDays = DtDaysFromCivil(y,(int)mo,1) + (d - 1);
		sxi64 iDiff = pRes->iWday - DtDowOf(iDays);
		if( iDiff < 0 ){ iDiff += 7; }
		d += iDiff;
		DtFfNormalize(&y,&mo,&d,&h,&mi,&s,&us);
	}
	*piUs = (int)us;
	return DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,
		pRes->iOffKind != 0 ? pRes->iOff : iDefOff);
}
/*
 * ---------------------------------------------------------------------------
 * DateTimeZone, DateTime and DateTimeImmutable, declared from C.
 *
 * These three used to be embedded PHP over nine global `__dt_*` thunks, with a
 * private `trait __DtCoreT` holding the state and the shared half of both date
 * classes. Every operation therefore crossed C -> PHP -> C and marshalled its
 * answer through a throwaway PHP array. The bodies below call the same routines
 * directly; the thunks, the trait and their chunk classes are gone.
 *
 * The instance state is unchanged, so `clone`, `serialize` and `var_dump` see what
 * they always saw (minus the `__DtCoreT` declaring-class name): four private slots
 * on each date class, two on DateTimeZone. Native traits do not exist, so the
 * shared method table is simply installed on both classes -- which is what `use
 * __DtCoreT` did anyway.
 * ---------------------------------------------------------------------------
 */
#define DT_TS    "__dtTs"
#define DT_OFF   "__dtOff"
#define DT_NAME  "__dtName"
#define DT_US    "__dtUs"
#define DTZ_OFF  "__dtzOff"
#define DTZ_NAME "__dtzName"
/*
 * php's timezone_type -- 1 = a fixed UTC OFFSET, 2 = an ABBREVIATION, 3 = an
 * IDENTIFIER -- STORED beside the name rather than read back off it, because the
 * name does not carry it: `new DateTimeZone('utc')` and `new DateTimeZone('UTC')`
 * are both named "UTC" there and are an abbreviation and an identifier
 * respectively, which is what makes them refuse to compare with each other. The
 * date objects keep their own copy (DT_ZKIND) for the same reason: php presents a
 * DateTime built with the lowercase zone as type 2.
 *
 * DtZoneTypeOf() remains the rule for a name that arrives with NO kind -- php's
 * own __unserialize re-derives it that way, which is why a serialized type-2 "UTC"
 * comes back as type 3 in both engines.
 */
#define DTZ_KIND "__dtzKind"
#define DT_ZKIND "__dtZKind"
/*
 * Has this object been CONSTRUCTED?
 *
 * php keeps its date state in a C struct hanging off the object and allocates it
 * in the constructor, so an object that never ran one -- what
 * `newInstanceWithoutConstructor()` answers, and what a subclass whose own
 * constructor forgets `parent::__construct()` IS -- has no state at all, and every
 * door raises `DateObjectError` rather than reading it. PHL's state lives in
 * ordinary (hidden) slots, which are there from instantiation and hold their
 * declared defaults, so such an object silently WAS 1970-01-01 UTC.
 *
 * This is that struct's presence, as the one thing a slot can carry: zero until
 * some constructor -- or one of the C factories, which build a complete object
 * without running one -- says otherwise. Every class in the family declares it,
 * every method reaches it through DtThis(), and a clone inherits it the way php's
 * cloned struct does.
 */
#define DT_INIT  "__dtInit"
/* The kind the script DEFAULT zone has, and it is not the name's own rule:
 * date_default_timezone_set() takes a tz-database IDENTIFIER and nothing else, so
 * php reports a date built under a `GMT` default as type 3 while
 * `new DateTimeZone('GMT')` -- the same three letters spelled as a zone -- is the
 * abbreviation, type 2. */
#define DT_ZONE_DEFAULT_KIND DT_ZONE_ID
/*
 * ---------------------------------------------------------------------------
 * A DateInterval's MICROSECONDS.
 *
 * php stores them as an int64 COUNT (timelib_rel_time.us) and shows that count
 * divided by a million, so the float is a rendering and the integer is the
 * value: `$i->f = 0.1234567` reads back 0.123456 because the write truncated to
 * 123456 microseconds, and `f` is what diff() fills, what add()/sub() move the
 * clock by, and what format()'s %f prints.
 *
 * PHL's `f` is a real property slot a script reads directly, so the count lives
 * beside it in a hidden one. The two are written together by every door that
 * owns the value (the write handler, diff, the constructors); a write that
 * arrives from somewhere else — unserialize's raw property store, or one of the
 * Recorded shapes php answers with a temporary — leaves only `f` behind, so the
 * count is trusted only while it still RENDERS to the float on show, and is
 * re-derived from the float when it does not.
 * ---------------------------------------------------------------------------
 */
#define DT_IV_US "__ivUs"
/* php's conversion of the `f` property to its stored count, cast contract and
 * all: it TRUNCATES toward zero, WRAPS what no int64 can hold, and answers 0 for
 * a NaN or an infinity. */
static sxi64 DtIvUsecOfReal(double r)
{
	return PH7_RealToInt64(r * 1000000.0);
}
/* The interval's microseconds. */
static sxi64 DtIvUsec(ph7_class_instance *pIv)
{
	ph7_value *pF = PH7_NativeAttr(pIv,"f");
	sxi64 us = PH7_NativeAttrInt(pIv,DT_IV_US);
	double r = 0.0;
	if( pF && (pF->iFlags & MEMOBJ_REAL) ){
		r = (double)pF->rVal;
	}else if( pF && (pF->iFlags & MEMOBJ_INT) ){
		r = (double)pF->x.iVal;
	}
	if( (double)us / 1000000.0 == r ){
		return us;   /* the count `f` was rendered from: exact past 2^53, where the float is not */
	}
	return DtIvUsecOfReal(r);
}
/* Store a microsecond count and the float php shows for it -- the two halves of
 * the same value, written together by every door that owns it. */
static void DtIvSetUsec(ph7_vm *pVm,ph7_class_instance *pIv,sxi64 us)
{
	PH7_NativeSetAttrInt(pVm,pIv,DT_IV_US,us);
	PH7_NativeSetAttrReal(pVm,pIv,"f",(ph7_real)((double)us / 1000000.0));
}
/* One date object's state, as the bodies below pass it around. */
typedef struct dt_state dt_state;
struct dt_state
{
	sxi64 iTs;
	sxi32 iOff;
	int uSec;
	const char *zName;   /* borrowed from the instance's own slot */
	int nName;
	int iZoneKind;       /* php's timezone_type: DT_ZONE_OFFSET / _ABBR / _ID */
};
/* php's name for a fixed offset: "+HH:MM" (and "+00:00" for zero, never "-00:00"). */
static int DtOffName(char *zBuf,sxu32 nBuf,sxi32 iOff)
{
	sxi32 a = iOff < 0 ? -iOff : iOff;
	return (int)SyBufferFormat(zBuf,nBuf,"%c%02d:%02d",
		iOff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));
}
/*
 * The same name with php's SECONDS field, which it appends only when there is
 * one: `new DateTimeZone('+01:00:59')` is named "+01:00:59" and answers that to
 * getName() and to format('e'), while `P`, `p`, `O` and `T` -- built from
 * DtOffName above -- still stop at the minute there. So the two spellings are
 * separate on purpose.
 */
static int DtOffNameSec(char *zBuf,sxu32 nBuf,sxi32 iOff)
{
	sxi32 a = iOff < 0 ? -iOff : iOff;
	/* php renders this into a buffer sized for its own example -- "+05:00" or
	 * "+05:00:01" -- so an offset whose hours want three digits comes back CUT.
	 * Only a format can build one: every other door caps the offset below 100
	 * hours, and `e` on `+9999` is 100 hours 39 minutes, named "+100:3". */
	int nMax = (a % 60 == 0) ? 6 : 9;
	int n;
	if( a % 60 == 0 ){
		n = DtOffName(zBuf,nBuf,iOff);
	}else{
		n = (int)SyBufferFormat(zBuf,nBuf,"%c%02d:%02d:%02d",
			iOff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60),(int)(a % 60));
	}
	if( n > nMax ){
		/* php's snprintf CUTS the text at the buffer and still answers the
		 * length it WANTED, so the name a script reads back carries php's own
		 * terminator inside it: `+9999` is the seven bytes "+100:3\0". */
		zBuf[nMax] = 0;
		if( n > nMax + 1 ){ n = nMax + 1; }
	}
	return n;
}
/*
 * php's timezone_type read off a NAME alone -- the fallback for a zone that
 * reached the engine without one: a payload `__unserialize()` re-parses (php
 * re-derives there too, which is why a serialized type-2 "UTC" comes back a
 * type 3 in both engines), and an object whose slots are still at their
 * defaults. Everywhere a SPELLING was seen, the kind stored with it wins --
 * "UTC" and "utc" are one name and two kinds.
 *
 * 1 = a fixed UTC OFFSET ("+02:00"), 2 = an ABBREVIATION ("GMT", "Z"),
 * 3 = an IDENTIFIER ("UTC", "Europe/Paris"). PHL accepts offsets, UTC, GMT and Z
 * today; the identifier arm is written for the whole rule so a tz database can
 * only add names, never change the tagging.
 */
static int DtZoneTypeOf(const char *zName,int nName)
{
	sxu32 nPos = 0;
	if( nName > 0 && (zName[0] == '+' || zName[0] == '-') ){
		return DT_ZONE_OFFSET;
	}
	if( nName == 3 && SyMemcmp(zName,"UTC",3) == 0 ){
		return DT_ZONE_ID;
	}
	if( nName > 0 && SyByteFind(zName,(sxu32)nName,'/',&nPos) == SXRET_OK ){
		return DT_ZONE_ID;
	}
	return DT_ZONE_ABBR;
}
/* The kind an instance carries in zSlot, or the name's own rule when the slot is
 * still zero -- an object built by newInstanceWithoutConstructor, or one whose
 * state predates the slot. */
static int DtZoneKindOf(ph7_class_instance *pObj,const char *zSlot,const char *zName,int nName)
{
	int iKind = (int)PH7_NativeAttrInt(pObj,zSlot);
	if( iKind < DT_ZONE_OFFSET || iKind > DT_ZONE_ID ){
		return DtZoneTypeOf(zName ? zName : "",nName);
	}
	return iKind;
}
/*
 * The database index of the SCRIPT's default zone, or -1.
 *
 * date_default_timezone_set() takes an identifier and nothing else, so the
 * default is always kind 3 and this is DtTzIndex() with that filled in. It is
 * exported because the procedural doors -- date(), mktime(), strtotime() and
 * the rest, which live in builtin_date.c -- have to ask the same question, and
 * the answer must not be spelled twice.
 */
PH7_PRIVATE int DtDefaultTzIndex(ph7_vm *pVm)
{
	return DtTzIndex(pVm->zDefTz,(int)pVm->nDefTz,DT_ZONE_ID);
}
/* DtTzOffsetAt() under an exported name, for the same callers. */
PH7_PRIVATE sxi32 DtTzOffsetOf(int iTz,sxi32 iFixed,sxi64 iTs,int *pbDst,
	const char **pzAbbr,int *pnAbbr)
{
	return DtTzOffsetAt(iTz,iFixed,iTs,pbDst,pzAbbr,pnAbbr);
}
/* The same question asked of an instance's own name/kind slots. */
static int DtTzIndexOf(ph7_class_instance *pObj,const char *zNameSlot,const char *zKindSlot)
{
	const char *zName;
	int nName;
	PH7_NativeAttrStr(pObj,zNameSlot,&zName,&nName);
	return DtTzIndex(zName,nName,DtZoneKindOf(pObj,zKindSlot,zName,nName));
}
static void DtLoad(ph7_class_instance *pObj,dt_state *pOut)
{
	pOut->iTs  = PH7_NativeAttrInt(pObj,DT_TS);
	pOut->iOff = (sxi32)PH7_NativeAttrInt(pObj,DT_OFF);
	pOut->uSec = (int)PH7_NativeAttrInt(pObj,DT_US);
	PH7_NativeAttrStr(pObj,DT_NAME,&pOut->zName,&pOut->nName);
	pOut->iZoneKind = DtZoneKindOf(pObj,DT_ZKIND,pOut->zName,pOut->nName);
}
static void DtStore(ph7_vm *pVm,ph7_class_instance *pObj,const dt_state *pIn)
{
	PH7_NativeSetAttrInt(pVm,pObj,DT_TS,pIn->iTs);
	PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,pIn->iOff);
	PH7_NativeSetAttrInt(pVm,pObj,DT_US,pIn->uSec);
	PH7_NativeSetAttrStr(pVm,pObj,DT_NAME,pIn->zName,pIn->nName);
	PH7_NativeSetAttrInt(pVm,pObj,DT_ZKIND,pIn->iZoneKind);
}
/*
 * Put DT_OFF back in step with DT_TS.
 *
 * A fixed zone's offset does not depend on the instant, so this was never
 * needed and DT_OFF could be written once and left. A DATABASE zone's does, so
 * every door that moves the timestamp on its own -- setTimestamp(), setDate(),
 * setTime(), modify(), add()/sub(), the period walker -- has to say so, and
 * they write DT_TS directly rather than through DtStore(). This is the one
 * call each of them owes; with a fixed zone it reads the name, finds no
 * database row and changes nothing.
 *
 * It re-derives the offset from the INSTANT, which is right for a door that
 * names an instant (setTimestamp) and is only half the answer for one that
 * names a WALL CLOCK -- php's setTime() and modify() work in local time and
 * have to re-solve the reading, which is a separate matter from the offset
 * being stale.
 */
static void DtRezone(ph7_vm *pVm,ph7_class_instance *pObj)
{
	int iTz = DtTzIndexOf(pObj,DT_NAME,DT_ZKIND);
	int bDst,nAbbr;
	const char *zAbbr;
	sxi32 iOff;
	if( iTz < 0 ){
		return;
	}
	iOff = DtTzOffsetAt(iTz,(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),
		PH7_NativeAttrInt(pObj,DT_TS),&bDst,&zAbbr,&nAbbr);
	PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,iOff);
}
/*
 * modify()'s one zone rule. php copies the parsed FIELDS into the object and
 * leaves its zone alone -- so a modifier that names a zone moves nothing -- with
 * `@epoch` the single exception: that form names an absolute instant, and php
 * re-zones the object to the fixed `+00:00` along with it. Every other modifier
 * leaves this a no-op.
 */
static void DtEpochRezone(ph7_vm *pVm,ph7_class_instance *pObj,const dt_parsed *pVec)
{
	char zBuf[16];
	int nName;
	if( !pVec->bEpoch ){
		return;
	}
	nName = DtOffName(zBuf,sizeof(zBuf),0);
	PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,0);
	PH7_NativeSetAttrStr(pVm,pObj,DT_NAME,zBuf,nName);
	PH7_NativeSetAttrInt(pVm,pObj,DT_ZKIND,DT_ZONE_OFFSET);
}
static ph7_class * DtClass(ph7_vm *pVm,const char *zName)
{
	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);
}
/* Is this instance an instance of the named date class? */
static int DtIsA(ph7_vm *pVm,ph7_class_instance *pObj,const char *zClass)
{
	ph7_class *pClass;
	if( pObj == 0 ){
		return 0;
	}
	pClass = DtClass(&(*pVm),zClass);
	return pClass != 0 && PH7_VmInstanceOf(pObj->pClass,pClass);
}
/* Has this object run a constructor (or been built whole by a C factory)? */
static int DtIsInit(ph7_class_instance *pObj)
{
	return pObj != 0 && PH7_NativeAttrInt(pObj,DT_INIT) != 0;
}
/* Say so. Called by every constructor that SUCCEEDS -- a failing one leaves the
 * object as it found it, which is php's answer too: an object whose
 * `__construct()` threw is still an uninitialized one. */
static void DtSetInit(ph7_vm *pVm,ph7_class_instance *pObj)
{
	PH7_NativeSetAttrInt(&(*pVm),pObj,DT_INIT,1);
}
/* A date object built from C rather than by a constructor: complete on arrival, so
 * it is born initialized. Every factory in this file goes through here. */
static ph7_class_instance * DtNewInstance(ph7_vm *pVm,ph7_class *pClass)
{
	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;
	if( pObj ){
		DtSetInit(&(*pVm),pObj);
	}
	return pObj;
}
/*
 * php's DateObjectError, worded for the object it is raised on: the class's own
 * name, and -- for a SUBCLASS -- the internal class it inherits, whatever the
 * depth of the chain (`class B extends A extends DateTime` reports
 * "B (inheriting DateTime)").
 */
static const char * DtNativeBase(ph7_vm *pVm,ph7_class_instance *pObj)
{
	static const char * const azBase[] = {
		"DateTime","DateTimeImmutable","DateTimeZone","DateInterval","DatePeriod"
	};
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(azBase) ; ++n ){
		if( DtIsA(&(*pVm),pObj,azBase[n]) ){
			return azBase[n];
		}
	}
	return 0;
}
static int DtThrowUninit(ph7_context *pCtx,ph7_class_instance *pObj)
{
	const char *zBase = DtNativeBase(pCtx->pVm,pObj);
	SyString *pName = &pObj->pClass->sName;
	if( zBase == 0
	 || (pName->nByte == SyStrlen(zBase) && SyMemcmp(pName->zString,zBase,pName->nByte) == 0) ){
		return PH7_VmThrowException(pCtx,"DateObjectError",
			"Object of type %z has not been correctly initialized by calling "
			"parent::__construct() in its constructor",pName);
	}
	return PH7_VmThrowException(pCtx,"DateObjectError",
		"Object of type %z (inheriting %s) has not been correctly initialized by "
		"calling parent::__construct() in its constructor",pName,zBase);
}
/*
 * The receiver of a native method, or NULL when the call has no object (which the
 * dispatcher only allows for a static one) -- and NULL as well for an object that
 * was never constructed, whose DateObjectError is raised here.
 *
 * This is the one screen the whole family shares: every method body already treats
 * a null receiver as "nothing to do" and returns PH7_OK, and the raise records the
 * status on the context, which the host-call boundary reports (VmHostFuncThrowRc).
 * The four doors php lets through -- the constructors, __unserialize and __wakeup,
 * which exist to initialize the object, and DatePeriod's two nullable getters --
 * take DtThisRaw() instead.
 */
static ph7_class_instance * DtThisRaw(ph7_context *pCtx)
{
	return PH7_ContextThis(pCtx);
}
static ph7_class_instance * DtThis(ph7_context *pCtx)
{
	ph7_class_instance *pThis = DtThisRaw(pCtx);
	if( pThis == 0 || DtIsInit(pThis) ){
		return pThis;
	}
	DtThrowUninit(pCtx,pThis);
	return 0;
}
/*
 * An object ARGUMENT that must be constructed: php raises the same DateObjectError
 * for a date it is HANDED as for the one it is called on. Answers -1 when it
 * raised; a value that is not an object at all was refused by the declared type
 * upstream, so it passes through.
 */
static int DtArgInit(ph7_context *pCtx,ph7_value *pArg)
{
	ph7_class_instance *pObj;
	if( pArg == 0 || (pArg->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pObj = (ph7_class_instance *)pArg->x.pOther;
	if( DtIsInit(pObj) ){
		return 0;
	}
	DtThrowUninit(pCtx,pObj);
	return -1;
}
/*
 * The same refusal for the one door that names the parameter's DECLARED type
 * instead of the object's class: php reports DatePeriod's start and end dates as
 * "DateTimeInterface" whatever they really are.
 */
static int DtArgInitNamed(ph7_context *pCtx,ph7_value *pArg,const char *zName)
{
	if( pArg == 0 || (pArg->iFlags & MEMOBJ_OBJ) == 0
	 || DtIsInit((ph7_class_instance *)pArg->x.pOther) ){
		return 0;
	}
	PH7_VmThrowException(pCtx,"DateObjectError",
		"Object of type %s has not been correctly initialized by calling "
		"parent::__construct() in its constructor",zName);
	return -1;
}
/* An immutable receiver mutates a COPY; a mutable one mutates itself. That is the
 * only difference between the two classes' method tables, so both share one body. */
static int DtIsImmutable(ph7_vm *pVm,ph7_class_instance *pObj)
{
	ph7_class *pImm = DtClass(pVm,"DateTimeImmutable");
	return pImm != 0 && PH7_VmInstanceOf(pObj->pClass,pImm);
}
/*
 * The object a mutator writes: $this itself, or a clone for DateTimeImmutable.
 * Either way the caller returns it, so a mutable method answers the same object
 * php's does (`$d->modify(...) === $d`).
 */
static ph7_class_instance * DtMutTarget(ph7_context *pCtx,ph7_class_instance *pThis,int *pbCopy)
{
	if( DtIsImmutable(pCtx->pVm,pThis) ){
		*pbCopy = 1;
		return PH7_CloneClassInstance(pThis);
	}
	*pbCopy = 0;
	return pThis;
}
/* Return a mutator's target the way php returns it: the clone (whose reference we
 * own) or the receiver itself (whose value the context already holds). */
static void DtMutResult(ph7_context *pCtx,ph7_class_instance *pTarget,int bCopy)
{
	if( bCopy ){
		PH7_NativeResultObject(pCtx,pTarget);
	}else{
		ph7_result_value(pCtx,PH7_ContextThisValue(pCtx));
	}
}
/* Read a DateTimeZone argument's two slots. php's ext/date reads its own internal
 * timezone struct here, so an overridden getName()/getOffset() is ignored by both
 * engines. Answers 0 when the value is not a DateTimeZone at all. */
static int DtZoneOf(ph7_value *pArg,sxi32 *piOff,const char **pzName,int *pnName,int *piKind)
{
	ph7_class_instance *pObj;
	if( pArg == 0 || (pArg->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pObj = (ph7_class_instance *)pArg->x.pOther;
	if( PH7_NativeAttr(pObj,DTZ_NAME) == 0 ){
		return 0;
	}
	if( !DtIsInit(pObj) ){
		/* php reads the zone's C struct here too, and an unconstructed one has
		 * none: its timelib fallback is a fixed UTC OFFSET, which is why
		 * `$d->setTimezone($uninitialized)` answers "+00:00" rather than raising.
		 * The doors that BUILD a date from a zone refuse instead -- see
		 * DtZoneArgInit(), which they call first. */
		*piOff = 0;
		*pzName = "+00:00";
		*pnName = (int)sizeof("+00:00") - 1;
		*piKind = DT_ZONE_OFFSET;
		return 1;
	}
	*piOff = (sxi32)PH7_NativeAttrInt(pObj,DTZ_OFF);
	PH7_NativeAttrStr(pObj,DTZ_NAME,pzName,pnName);
	*piKind = DtZoneKindOf(pObj,DTZ_KIND,*pzName,*pnName);
	return 1;
}
/*
 * The zone argument of a door that INITIALIZES a date from it -- the two
 * constructors, `date_create()` and `createFromFormat()`. php refuses an
 * unconstructed zone there, and with a different sentence and a different class
 * from every other uninitialized-object refusal in the family: a plain `Error`,
 * naming no method. Answers -1 when it raised.
 */
static int DtZoneArgInit(ph7_context *pCtx,ph7_value *pArg)
{
	if( pArg == 0 || (pArg->iFlags & MEMOBJ_OBJ) == 0
	 || DtIsInit((ph7_class_instance *)pArg->x.pOther) ){
		return 0;
	}
	PH7_VmThrowException(pCtx,"Error",
		"The DateTimeZone object has not been correctly initialized by its constructor");
	return -1;
}
/*
 * Write a WALL-CLOCK reading -- seconds since the epoch as the zone's own clock
 * shows them -- into an object's timestamp, and put DT_OFF in step with it.
 *
 * For a fixed zone this is one subtraction, which is what these doors always
 * did inline. For a DATABASE zone the reading may name two instants or none, so
 * the zone decides; and the offset it lands on is not the one it started from,
 * which is why `setTime()` across a spring-forward morning has to write both
 * slots rather than keeping the offset it read the clock with.
 */
static void DtStoreLocalOf(ph7_vm *pVm,ph7_class_instance *pObj,sxi64 iLocal)
{
	sxi32 iOff = (sxi32)PH7_NativeAttrInt(pObj,DT_OFF);
#ifdef PH7_ENABLE_TZDB
	int iTz = DtTzIndexOf(pObj,DT_NAME,DT_ZKIND);
	if( iTz >= 0 ){
		sxi64 iTs = iLocal;
		if( PH7_TzLocalToUtc(iTz,iLocal,&iTs,&iOff) ){
			PH7_NativeSetAttrInt(pVm,pObj,DT_TS,iTs);
			PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,iOff);
			return;
		}
	}
#endif
	/* Unsigned, because a reading at either end of php's clock overflows the
	 * signed subtraction and every other arithmetic door here is written the
	 * same way. */
	PH7_NativeSetAttrInt(pVm,pObj,DT_TS,(sxi64)((sxu64)iLocal - (sxu64)iOff));
}
/*
 * modify()'s timestamp write, shared with the procedural date_modify().
 *
 * The parse ran under DT_PARSE_KEEP_ZONE, so what it produced is the object's
 * own wall clock less the offset it STARTED from -- and a modifier can walk
 * that clock across a DST switch, where the offset it ends on is a different
 * one. `@epoch` is the exception: it names an absolute instant, and
 * DtEpochRezone moves the object to a fixed `+00:00` along with it, so there is
 * no reading to re-solve.
 */
static void DtStoreModified(ph7_vm *pVm,ph7_class_instance *pObj,sxi64 iTs,
	sxi32 iOffBase,const dt_parsed *pVec)
{
	if( pVec->bEpoch ){
		PH7_NativeSetAttrInt(pVm,pObj,DT_TS,iTs);
		return;
	}
	DtStoreLocalOf(pVm,pObj,(sxi64)((sxu64)iTs + (sxu64)iOffBase));
}
/*
 * Parse $datetime into a date object's state, php's constructor rules: an explicit
 * offset in the string wins over the $timezone argument, a literal "Z" keeps its
 * own name, and everything else takes the argument's (or the default) zone.
 * Returns 0 on success; on failure the caller throws with the reason and position
 * this reports.
 */
static int DtInitState(ph7_context *pCtx,const char *zIn,int nIn,sxi32 iZoneOff,
	const char *zZoneName,int nZoneName,int iZoneKind,dt_state *pOut,char *zNameBuf,
	sxu32 nNameBuf,const char **pzErr,int *piPos,char *pcAt)
{
	sxi64 iTs = 0,iNow = 0;
	sxi32 iOff = 0;
	int bOffSet = 0,uSec = 0,iErrPos,uNow = 0;
	int iTz,bDst,nAbbr;
	const char *zAbbr;
	dt_parsed sVec;
	/* php's base moment is the whole clock, microseconds included: a string that
	 * names no time of day keeps them (`new DateTime()`, `+1 day`), and one that
	 * does zeroes them along with the rest of the clock. */
	DtNowUs(pCtx->pVm,&iNow,&uNow);
	/* A DATABASE zone has no offset until an instant picks one, and the parse
	 * below needs one BEFORE it has an instant -- the base moment is what fills
	 * in every field the string leaves out, so reading it in the wrong offset
	 * puts `today` on the wrong day. The offset at NOW is that answer, and the
	 * reading the parse produces is re-solved against the zone afterwards. */
	iTz = DtTzIndex(zZoneName,nZoneName,iZoneKind);
	if( iTz >= 0 ){
		iZoneOff = DtTzOffsetAt(iTz,iZoneOff,iNow,&bDst,&zAbbr,&nAbbr);
	}
	/* php's constructors pass the word `now` in place of an empty string, which
	 * is why `new DateTime('')` is the current moment where `modify('')` is its
	 * `Empty string` refusal. */
	if( nIn < 1 ){
		zIn = "now";
		nIn = 3;
	}
	/* php publishes what this scan collected through getLastErrors(), whether or
	 * not it throws, and a clean parse puts the record back to `false`. */
	iErrPos = DtParseEx(zIn,nIn,iNow,iZoneOff,uNow,0,&iTs,&iOff,&bOffSet,&uSec,&sVec,
		&pCtx->pVm->sDtLastErr);
	if( iErrPos != 0 ){
		*pzErr = DtParseErr(zIn,nIn,iErrPos,piPos,pcAt);
		return -1;
	}
	pOut->iTs = iTs;
	pOut->uSec = uSec;
	if( bOffSet ){
		pOut->iOff = iOff;
		if( bOffSet == 2 ){
			/* a zone the STRING named -- php's `Z`, or a trailing UTC/GMT */
			pOut->zName = sVec.zZone;
			pOut->nName = sVec.nZone;
			pOut->iZoneKind = sVec.bZoneIdent ? DT_ZONE_ID : DT_ZONE_ABBR;
		}else{
			/* ...Sec: an offset the string spelled with SECONDS is named with
			 * them (`+02:00:30`), which is the same name DateTimeZone gives it. */
			pOut->nName = DtOffNameSec(zNameBuf,nNameBuf,iOff);
			pOut->zName = zNameBuf;
			pOut->iZoneKind = DT_ZONE_OFFSET;
		}
	}else{
		pOut->iOff = iZoneOff;
		pOut->zName = zZoneName;
		pOut->nName = nZoneName;
		pOut->iZoneKind = iZoneKind;
#ifdef PH7_ENABLE_TZDB
		if( iTz >= 0 ){
			/* The parse read the string against the offset at NOW, so what it
			 * produced is a WALL-CLOCK reading in this zone, shifted by that
			 * offset. Recover the reading and ask the zone which instant it
			 * names -- the answer differs from the guess for every date on the
			 * other side of a DST switch from today, which is most of the year.
			 */
			sxi64 iFixed = iTs;
			sxi32 iOffAt = iZoneOff;
			if( PH7_TzLocalToUtc(iTz,iTs + iZoneOff,&iFixed,&iOffAt) ){
				pOut->iTs = iFixed;
				pOut->iOff = iOffAt;
			}
		}
#endif
	}
	return 0;
}
/*
 * php's UTC-OFFSET spellings, the whole set of them. Reads the digits and colons
 * after the sign and dispatches on their SHAPE, which is what php's scanner does:
 *
 *   D | DD              the HOURS alone            +1     +01    +59
 *   DDD                 H then MM                  +130 = +01:30, +999 = +10:39
 *   DDDD                HH then MM                 +0100  +0060 = +01:00
 *   DDDDDD              HH then MM then SS         +010059 = +01:00:59
 *   D:D | DD:D | D:DD | DD:DD    hours then minutes
 *   DD:DD:DD            hours, minutes and seconds
 *
 * Five digits, seven digits and a one-digit hour before two colons are php's own
 * refusals. Minutes and seconds are NOT bounded on their own -- `+00:60` is an
 * hour and `+01:99` is +02:39 -- only the TOTAL is, and a total at or past 100
 * hours is php's separate "Timezone offset is out of range" (answered here as -2,
 * because the two refusals are worded differently at every door).
 *
 * Answers the KIND as well (php's timezone_type), because the name cannot carry
 * it: "UTC" spelled exactly is an IDENTIFIER and any other casing of it is an
 * ABBREVIATION, and php refuses to compare the two.
 */
#define DT_ZONE_OFF_LIMIT 360000   /* php's ceiling: |offset| < 100 hours */
static int DtZoneOffsetDigits(const char *z,int n,sxi32 *piOff,int *pnUsed)
{
	int aVal[3];
	int aWidth[3];
	int nPart = 0;
	int i;
	sxi64 iOff;
	aVal[0] = aVal[1] = aVal[2] = 0;
	aWidth[0] = aWidth[1] = aWidth[2] = 0;
	/* Read the RUN of digits and colons and stop at anything else; the caller
	 * decides what a tail means. At most two colons, and no group may be empty
	 * or wider than two -- except the single group of a colonless spelling,
	 * which is split by WIDTH below instead. */
	for( i = 0 ; i < n ; ++i ){
		if( z[i] == ':' ){
			if( nPart >= 2 ){
				break;
			}
			nPart++;
			continue;
		}
		if( !SyisDigit(z[i]) ){
			break;
		}
		if( aWidth[nPart] >= 6 ){
			return -1;
		}
		aVal[nPart] = aVal[nPart] * 10 + (z[i] - '0');
		aWidth[nPart]++;
	}
	*pnUsed = i;
	if( nPart == 0 ){
		/* No colon: the WIDTH says how the digits split. */
		int v = aVal[0];
		switch( aWidth[0] ){
			case 1: case 2:  /* H, HH */
				iOff = (sxi64)v * 3600;
				break;
			case 3:          /* H MM */
				iOff = (sxi64)(v / 100) * 3600 + (v % 100) * 60;
				break;
			case 4:          /* HH MM */
				iOff = (sxi64)(v / 100) * 3600 + (v % 100) * 60;
				break;
			case 6:          /* HH MM SS */
				iOff = (sxi64)(v / 10000) * 3600 + ((v / 100) % 100) * 60 + (v % 100);
				break;
			default:         /* five, or seven and up */
				return -1;
		}
	}else{
		/* Colons: H:M through HH:MM, or HH:MM:SS with two digits everywhere
		 * (php refuses `+1:00:00` and `+01:00:0` alike, and takes `+1:1` for
		 * +01:01). A trailing colon leaves an empty group, which is a refusal. */
		int nWant = nPart == 2 ? 2 : 0;
		for( i = 0 ; i <= nPart ; ++i ){
			if( aWidth[i] < 1 || aWidth[i] > 2 || (nWant && aWidth[i] != nWant) ){
				return -1;
			}
		}
		iOff = (sxi64)aVal[0] * 3600 + (sxi64)aVal[1] * 60 + aVal[2];
	}
	if( iOff >= DT_ZONE_OFF_LIMIT ){
		/* Past php's ceiling, and php answers THAT even when the spelling has a
		 * tail it would otherwise reject: the range is checked on what the
		 * scanner read, before anything is said about what follows. */
		return -2;
	}
	*piOff = (sxi32)iOff;
	return 0;
}
/*
 * The two spellings that are NAMES rather than arithmetic, in php's own order:
 * an ABBREVIATION first, then a tz-database IDENTIFIER. Reached only once every
 * offset spelling has failed, and answers -1 when neither table has the name --
 * which, with PH7_ENABLE_TZDB off, is always.
 *
 * The order is not a detail. Ten names are in both tables (`CET`, `EET`, `EST`,
 * `GMT`, `HST`, `MET`, `MST`, `UCT`, `UTC`, `WET`) and php takes the
 * abbreviation for every one of them, so `new DateTimeZone('CET')` is a FIXED
 * +01:00 that never observes daylight time while the zone file of that name
 * switches twice a year. Getting this backwards turns a loud refusal into a
 * quietly wrong summer offset.
 *
 * The two also differ in what they NAME. An abbreviation answers the table's
 * canonical upper-case spelling whatever the caller wrote; an identifier
 * answers the caller's own bytes -- `new DateTimeZone('europe/paris')` is named
 * `europe/paris` and still knows about Paris.
 */
static int DtZoneNamed(const char *zTz,int nTz,sxi32 *piOff,const char **pzName,
	int *pnName,int *piKind)
{
#ifdef PH7_ENABLE_TZDB
	int bDst = 0;
	if( PH7_TzAbbrFind(zTz,nTz,piOff,&bDst,pzName,pnName) ){
		*piKind = DT_ZONE_ABBR;
		return 0;
	}
	if( PH7_TzFind(zTz,nTz) >= 0 ){
		*piOff = 0;
		*pzName = zTz;
		*pnName = nTz;
		*piKind = DT_ZONE_ID;
		return 0;
	}
#else
	SXUNUSED(zTz);
	SXUNUSED(nTz);
	SXUNUSED(piOff);
	SXUNUSED(pzName);
	SXUNUSED(pnName);
	SXUNUSED(piKind);
#endif
	return -1;
}
/*
 * The timezone spellings PHL understands with no tz database: UTC, GMT, Z and a
 * fixed offset, optionally behind a `GMT` prefix and behind leading blanks.
 * Shared by DateTimeZone::__construct(), which throws on a miss, and
 * timezone_open(), which warns and answers false. Answers 0, -1 (unknown or bad)
 * or -2 (an offset past php's range).
 */
static int DtZoneParse(const char *zTz,int nTz,sxi32 *piOff,const char **pzName,
	int *pnName,int *piKind,char *zBuf,sxu32 nBuf)
{
	int rc,nUsed = 0;
	/* php's scanner skips leading blanks and nothing else -- a TRAILING one is a
	 * refusal, and so is a newline before the sign. */
	while( nTz > 0 && (zTz[0] == ' ' || zTz[0] == '\t') ){
		zTz++;
		nTz--;
	}
	/* A single letter is php's military zone here too -- `new DateTimeZone('t')`
	 * is named `T` and answers -07:00 -- which is what lets a date carrying one
	 * round-trip through serialize()/__unserialize(). */
	if( nTz == 1 && DtZoneMil(zTz[0],piOff,pzName) ){
		*pnName = 1;
		*piKind = DT_ZONE_ABBR;
		return 0;
	}
	if( nTz == 3 && (SyStrnicmp(zTz,"UTC",3) == 0 || SyStrnicmp(zTz,"GMT",3) == 0) ){
		/* php answers the canonical spelling, whatever case the caller used --
		 * and only the exact "UTC" is one of its tz-database IDENTIFIERS. */
		int bUtc = (zTz[0] == 'u' || zTz[0] == 'U');
		*piOff = 0;
		*pzName = bUtc ? "UTC" : "GMT";
		*pnName = 3;
		*piKind = (bUtc && SyMemcmp(zTz,"UTC",3) == 0) ? DT_ZONE_ID : DT_ZONE_ABBR;
		return 0;
	}
	{
		/* `GMT+01:00` is php's offset, named for the offset alone. The prefix is
		 * UPPERCASE only there (`gmt+1` is a refusal where the bare `gmt` is a
		 * zone), no blank is allowed between the two halves, and `UTC+1` is not
		 * a spelling at all.
		 *
		 * It is a prefix only when a SIGN follows it. `GMT0` and `gmt0` are not
		 * "GMT plus nothing" -- they are tz-database identifiers, stored
		 * verbatim like any other, and `GMT8` is neither and is refused. So the
		 * stripped spelling is kept beside the original rather than replacing
		 * it, and the name tables below are asked about what the caller wrote.
		 */
		const char *zNum = zTz;
		int nNum = nTz;
		if( nNum > 3 && SyMemcmp(zNum,"GMT",3) == 0
		 && (zNum[3] == '+' || zNum[3] == '-') ){
			zNum += 3;
			nNum -= 3;
		}
		if( nNum < 2 || (zNum[0] != '+' && zNum[0] != '-') ){
			return DtZoneNamed(zTz,nTz,piOff,pzName,pnName,piKind);
		}
		rc = DtZoneOffsetDigits(zNum + 1,nNum - 1,piOff,&nUsed);
		if( rc != 0 ){
			return rc;
		}
		if( nUsed != nNum - 1 ){
			return -1;   /* a tail the scanner did not read: not a zone at all */
		}
		if( zNum[0] == '-' ){
			*piOff = -*piOff;
		}
	}
	/* php normalizes the NAME through the offset, so "-00:00" is "+00:00", and
	 * carries the SECONDS field only when there is one. */
	*pnName = DtOffNameSec(zBuf,nBuf,*piOff);
	*pzName = zBuf;
	*piKind = DT_ZONE_OFFSET;
	return 0;
}
/*
 * ---------------------------------------------------------------------------
 * DateTimeZone::listIdentifiers() and its timezone_identifiers_list() twin.
 *
 * The group argument is a BITMASK of the ten continents plus `UTC`, and the
 * rule for reading it is not the plain mask it looks like -- two values are
 * compared EXACTLY, which is why `-1` is ALL and not ALL_WITH_BC even though
 * it has every bit:
 *
 *   == PER_COUNTRY (4096)     the country code decides, and a null or
 *                             non-two-character one is a ValueError. Any two
 *                             bytes are accepted and simply match nothing, so
 *                             `12` and the lower-case `br` are empty arrays
 *                             where `BR` is sixteen zones.
 *   == ALL_WITH_BC (4095)     every identifier, backward links included.
 *   anything else             `group & 2047` over the group bits, backward
 *                             links excluded -- so `4097` is Africa alone and
 *                             the country code beside it is ignored.
 *
 * The order is the table's, which is the identifiers' own byte order.
 */
#define DT_TZ_GROUP_ALL         2047
#define DT_TZ_GROUP_ALL_W_BC    4095
#define DT_TZ_GROUP_PER_COUNTRY 4096
static int DtZoneListResult(ph7_context *pCtx,sxi64 iGroup,ph7_value *pCc,
	const char *zWho)
{
	ph7_value *pArray,*pVal;
	const char *zCc = 0;
	int nCc = 0,i,nZone;
	SXUNUSED(zWho);
	if( iGroup == DT_TZ_GROUP_PER_COUNTRY ){
		if( pCc != 0 && (pCc->iFlags & MEMOBJ_NULL) == 0 ){
			zCc = ph7_value_to_string(pCc,&nCc);
		}
		if( nCc != 2 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #2 ($countryCode) must be a two-letter ISO 3166-1 "
				"compatible country code when argument #1 ($timezoneGroup) is "
				"DateTimeZone::PER_COUNTRY",zWho);
		}
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	nZone = 0;
#ifdef PH7_ENABLE_TZDB
	nZone = PH7_TzCount();
#else
	/* No database, so no identifier to list -- the loop below runs zero times
	 * and the country code the PER_COUNTRY screen above read goes unused. The
	 * screen itself stays: its ValueError is about the ARGUMENTS, not about
	 * what the build happens to carry. */
	SXUNUSED(zCc);
#endif
	for( i = 0 ; i < nZone ; ++i ){
#ifdef PH7_ENABLE_TZDB
		/* Walked in php's own print order, which is case-INSENSITIVE: `CET`
		 * sits between `Canada/Yukon` and `Chile/Continental`, and `localtime`
		 * among the `L` names. The 419 canonical `Continent/City` names come out
		 * the same either way, so only ALL_WITH_BC shows it. */
		int iZone = PH7_TzAt(i);
		int nName = 0,bBack = 0;
		const char *zName = PH7_TzName(iZone,&nName,&bBack);
		if( zName == 0 ){
			continue;
		}
		if( iGroup == DT_TZ_GROUP_PER_COUNTRY ){
			if( SyMemcmp(PH7_TzCountry(iZone),zCc,2) != 0 ){
				continue;
			}
		}else if( iGroup != DT_TZ_GROUP_ALL_W_BC ){
			if( bBack || (PH7_TzGroup(iZone) & (int)(iGroup & DT_TZ_GROUP_ALL)) == 0 ){
				continue;
			}
		}
		ph7_value_string(pVal,zName,nName);
		ph7_array_add_elem(pArray,0,pVal);
		ph7_value_reset_string_cursor(pVal);
#endif
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* DateTimeZone::listIdentifiers(int $timezoneGroup = ALL, ?string $countryCode = null) */
static int vm_builtin_DateTimeZone_listIdentifiers(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtZoneListResult(pCtx,
		nArg > 0 ? ph7_value_to_int64(apArg[0]) : DT_TZ_GROUP_ALL,
		nArg > 1 ? apArg[1] : 0,"DateTimeZone::listIdentifiers");
}
static int vm_builtin_timezone_identifiers_list(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtZoneListResult(pCtx,
		nArg > 0 ? ph7_value_to_int64(apArg[0]) : DT_TZ_GROUP_ALL,
		nArg > 1 ? apArg[1] : 0,"timezone_identifiers_list");
}
/* DateTimeZone::__construct(string $timezone) */
static int vm_builtin_DateTimeZone_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */
	const char *zTz,*zName;
	int nTz,nName,iKind = DT_ZONE_ID,rc;
	sxi32 iOff = 0;
	char zBuf[16];
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zTz = ph7_value_to_string(apArg[0],&nTz);
	rc = DtZoneParse(zTz,nTz,&iOff,&zName,&nName,&iKind,zBuf,sizeof(zBuf));
	if( rc != 0 ){
		return PH7_VmThrowException(pCtx,"DateInvalidTimeZoneException",
			rc == -2 ? "DateTimeZone::__construct(): Timezone offset is out of range (%.*s)"
			         : "DateTimeZone::__construct(): Unknown or bad timezone (%.*s)",nTz,zTz);
	}
	PH7_NativeSetAttrInt(pCtx->pVm,pThis,DTZ_OFF,iOff);
	PH7_NativeSetAttrStr(pCtx->pVm,pThis,DTZ_NAME,zName,nName);
	PH7_NativeSetAttrInt(pCtx->pVm,pThis,DTZ_KIND,iKind);
	DtSetInit(pCtx->pVm,pThis);
	return PH7_OK;
}
/* DateTimeZone::getName() */
static int vm_builtin_DateTimeZone_getName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	const char *zName;
	int nName;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	PH7_NativeAttrStr(pThis,DTZ_NAME,&zName,&nName);
	ph7_result_string(pCtx,zName,nName);
	return PH7_OK;
}
/*
 * The offset a ZONE object is on at a given date -- DateTimeZone::getOffset()
 * and its timezone_offset_get() alias.
 *
 * php screens the date it is handed even for a fixed zone, whose answer does
 * not depend on it. A DATABASE zone's does, and the instant it is read at is
 * the date's TIMESTAMP -- the date's own zone is irrelevant, since two
 * expressions of one instant are the same instant.
 */
static void DtZoneOffsetResult(ph7_context *pCtx,ph7_class_instance *pZone,
	ph7_class_instance *pDate)
{
	sxi32 iOff = (sxi32)PH7_NativeAttrInt(pZone,DTZ_OFF);
	int iTz = DtTzIndexOf(pZone,DTZ_NAME,DTZ_KIND);
	if( iTz >= 0 && pDate != 0 ){
		int bDst,nAbbr;
		const char *zAbbr;
		iOff = DtTzOffsetAt(iTz,iOff,PH7_NativeAttrInt(pDate,DT_TS),&bDst,&zAbbr,&nAbbr);
	}
	ph7_result_int64(pCtx,iOff);
}
/* DateTimeZone::getOffset(DateTimeInterface $datetime) */
static int vm_builtin_DateTimeZone_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	if( pThis == 0 || (nArg > 0 && DtArgInit(pCtx,apArg[0]) != 0) ){
		return PH7_OK;
	}
	DtZoneOffsetResult(pCtx,pThis,
		nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ?
			(ph7_class_instance *)apArg[0]->x.pOther : 0);
	return PH7_OK;
}
/*
 * DateTimeZone::getTransitions() and its timezone_transitions_get() twin.
 *
 * php answers FALSE for anything that is not a DATABASE zone. A fixed offset
 * and an abbreviation each name one clock for all time, and php declines to
 * describe that as a transition list rather than answering a one-row one -- so
 * `(new DateTimeZone('+02:00'))->getTransitions()` and the same call on `CET`,
 * which is an abbreviation here and never observes daylight time, are both
 * `false` where `UTC` is an identifier and answers a single row.
 *
 * The FIRST row is synthesized at the range's start and is not a switch: it is
 * what the clock was already doing when the range opened, which is why its `ts`
 * is the caller's own bound and why a range that opens exactly ON a transition
 * shows that transition once rather than twice.
 *
 * THE TWO BOUNDS ARE NOT SYMMETRIC, and both halves had to be measured:
 *
 *   the begin is EXCLUSIVE   a transition exactly at it is the synthesized row
 *                            and is not repeated.
 *   the end is EXCLUSIVE too a transition exactly at it is left out, so
 *                            `getTransitions($t, $x)` and `getTransitions($t,
 *                            $x + 1)` differ when a switch lands on `$x`.
 *
 * And the end DEFAULTS to 2147483647 rather than to PHP_INT_MAX -- timelib's
 * 32-bit horizon, still visible in php 8.5. That one constant is why a
 * no-argument call stops in 2037 for the American zones while an explicit end
 * past 2038 keeps generating from the POSIX footer, and why php's own call with
 * an explicit PHP_INT_MAX runs until it exhausts memory. Read as "the default
 * is unbounded" it looks instead like the walk refuses to extrapolate, which is
 * a rule that holds on every zone whose data happens to end before 2038 and
 * fails on the ones that do not.
 */
#ifdef PH7_ENABLE_TZDB
static int DtTransRow(ph7_context *pCtx,ph7_value *pArray,ph7_value *pVal,
	sxi64 iTs,sxi32 iOff,int bDst,const char *zAbbr,int nAbbr)
{
	ph7_value *pRow = ph7_context_new_array(pCtx);
	sxi64 y;
	int m,d,nSec;
	char zBuf[64];
	int nBuf;
	if( pRow == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* The seconds of the day as a FLOORED remainder, taken without ever
	 * rebuilding the day in seconds: the nominal first row sits at PHP_INT_MIN,
	 * and `iTs - iDay * 86400` overflows there long before it can be
	 * subtracted. `%` cannot overflow for this divisor, and the correction
	 * turns C's truncation towards zero into the floor the date wants. */
	nSec = (int)(iTs % 86400);
	if( nSec < 0 ){
		nSec += 86400;
	}
	DtCivilFromDays(DtFloorDiv(iTs,86400),&y,&m,&d);
	/* Always spelled in UT -- the `+00:00` is a constant, not the zone's own
	 * offset, so the row carries the instant twice and the offset once. */
	nBuf = (int)SyBufferFormat(zBuf,sizeof(zBuf),
		"%04qd-%02d-%02dT%02d:%02d:%02d+00:00",
		y,m,d,nSec / 3600,(nSec / 60) % 60,nSec % 60);
	ph7_value_int64(pVal,iTs);
	ph7_array_add_strkey_elem(pRow,"ts",pVal);
	ph7_value_string(pVal,zBuf,nBuf);
	ph7_array_add_strkey_elem(pRow,"time",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_int64(pVal,iOff);
	ph7_array_add_strkey_elem(pRow,"offset",pVal);
	ph7_value_bool(pVal,bDst);
	ph7_array_add_strkey_elem(pRow,"isdst",pVal);
	ph7_value_string(pVal,zAbbr,nAbbr);
	ph7_array_add_strkey_elem(pRow,"abbr",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_array_add_elem(pArray,0,pRow);
	/* The parent took a copy, so the row goes back to the context rather than
	 * being held for the length of a walk that can run to hundreds of rows. */
	ph7_context_release_value(pCtx,pRow);
	return PH7_OK;
}
#endif /* PH7_ENABLE_TZDB */
static int DtZoneTransitionsResult(ph7_context *pCtx,ph7_class_instance *pZone,
	sxi64 iBegin,sxi64 iEnd)
{
	int iTz = DtTzIndexOf(pZone,DTZ_NAME,DTZ_KIND);
#ifdef PH7_ENABLE_TZDB
	if( iTz < 0 ){
		const char *zName;
		int nName;
		PH7_NativeAttrStr(pZone,DTZ_NAME,&zName,&nName);
		/* `UTC` is an IDENTIFIER, and php describes it like any other -- one
		 * row, no switches. DtTzIndex() excludes it from the table only to keep
		 * the fixed-offset path every other door is tested on, and that
		 * exclusion is wrong for this one question. An abbreviation stays out:
		 * `CET` is a fixed +01:00 here, not the file of that name, so it keeps
		 * the `false` php gives it. */
		if( DtZoneKindOf(pZone,DTZ_KIND,zName,nName) == DT_ZONE_ID ){
			iTz = PH7_TzFind(zName,nName);
		}
	}
#endif
	if( iTz < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
#ifdef PH7_ENABLE_TZDB
	{
		ph7_value *pArray = ph7_context_new_array(pCtx);
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		sxi64 iTs = 0,iLast = iBegin,iFileEnd = iBegin;
		sxi32 iOff = 0;
		int bDst = 0,nAbbr = 0,i,nTrans;
		const char *zAbbr = "";
		if( pArray == 0 || pVal == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		if( PH7_TzOffsetAt(iTz,iBegin,&iOff,&bDst,&zAbbr,&nAbbr) ){
			DtTransRow(pCtx,pArray,pVal,iBegin,iOff,bDst,zAbbr,nAbbr);
		}
		nTrans = PH7_TzTransCount(iTz);
		for( i = 0 ; i < nTrans ; ++i ){
			if( !PH7_TzTransAt(iTz,i,&iTs,&iOff,&bDst,&zAbbr,&nAbbr) ){
				break;
			}
			iFileEnd = iTs;
			if( iTs <= iBegin ){
				continue;
			}
			if( iTs >= iEnd ){
				/* The file has more rows, and iFileEnd must name the LAST of
				 * them rather than the one this range stopped on: it is the
				 * boundary the footer takes over at, not a position in the
				 * walk. */
				if( PH7_TzTransAt(iTz,nTrans - 1,&iTs,&iOff,&bDst,&zAbbr,&nAbbr) ){
					iFileEnd = iTs;
				}
				break;
			}
			DtTransRow(pCtx,pArray,pVal,iTs,iOff,bDst,zAbbr,nAbbr);
			iLast = iTs;
		}
		/* The footer governs only what comes AFTER the file's last row, so the
		 * walk is seeded there rather than at the last row EMITTED. Seeded at
		 * the latter it would re-derive from the rule inside territory the file
		 * already describes, and answer the CURRENT switch dates for years that
		 * ran on older ones -- an extra November row in 2004, and a decade of
		 * invented ones in 1900. */
		if( iLast < iFileEnd ){
			iLast = iFileEnd;
		}
		while( PH7_TzTransNextPosix(iTz,iLast,&iTs,&iOff,&bDst,&zAbbr,&nAbbr) ){
			if( iTs >= iEnd ){
				break;
			}
			DtTransRow(pCtx,pArray,pVal,iTs,iOff,bDst,zAbbr,nAbbr);
			iLast = iTs;
		}
		ph7_result_value(pCtx,pArray);
	}
#else
	SXUNUSED(iBegin);
	SXUNUSED(iEnd);
#endif
	return PH7_OK;
}
/* DateTimeZone::getTransitions(int $timestampBegin = PHP_INT_MIN, int $timestampEnd = 2147483647) */
static int vm_builtin_DateTimeZone_getTransitions(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	if( pThis == 0 ){
		return PH7_OK;
	}
	return DtZoneTransitionsResult(pCtx,pThis,
		nArg > 0 ? ph7_value_to_int64(apArg[0]) : (-(sxi64)0x7FFFFFFFFFFFFFFF - 1),
		nArg > 1 ? ph7_value_to_int64(apArg[1]) : (sxi64)0x7FFFFFFF);
}
/* DateTime::__construct(string $datetime = 'now', ?DateTimeZone $timezone = null) */
static int vm_builtin_DateTime_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */
	const char *zIn = "now",*zZone;
	int nIn = 3,nZone;
	sxi32 iZoneOff = 0;
	dt_state sState;
	char zNameBuf[16];
	const char *zErr;
	int iPos;
	char cAt;
	int iZoneKind;
	if( pThis == 0 ){
		return PH7_OK;
	}
	zZone = pVm->zDefTz;
	nZone = (int)pVm->nDefTz;
	iZoneKind = DT_ZONE_DEFAULT_KIND;
	if( nArg > 0 ){
		zIn = ph7_value_to_string(apArg[0],&nIn);
	}
	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){
		if( DtZoneArgInit(pCtx,apArg[1]) != 0 ){
			return PH7_OK;
		}
		DtZoneOf(apArg[1],&iZoneOff,&zZone,&nZone,&iZoneKind);
	}
	if( DtInitState(pCtx,zIn,nIn,iZoneOff,zZone,nZone,iZoneKind,&sState,zNameBuf,
		sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0 ){
		return PH7_VmThrowException(pCtx,"DateMalformedStringException",
			"Failed to parse time string (%.*s) at position %d (%c): %s",
			DtCStrLen(zIn,nIn),zIn,iPos,cAt,zErr);
	}
	DtStore(pVm,pThis,&sState);
	DtSetInit(pVm,pThis);
	return PH7_OK;
}
/* One date object's `format()`, shared with the date_format() alias. */
static void DtFormatOf(ph7_context *pCtx,ph7_class_instance *pObj,const char *zFmt,int nFmt)
{
	dt_state sState;
	Sytm sTm;
	char zZone[64];
	int nName;
	DtLoad(pObj,&sState);
	nName = sState.nName;
	if( nName >= (int)sizeof(zZone) ){
		nName = (int)sizeof(zZone) - 1;
	}
	SyMemcpy(sState.zName,zZone,(sxu32)nName);
	zZone[nName] = 0;
	DtFillSytm(sState.iTs,sState.iOff,zZone,&sTm);
	/*
	 * What `T` prints, and what `I` prints.
	 *
	 * php decides `T` on the zone's TYPE. A type-1 fixed OFFSET is spelled
	 * "GMT+0530"; a type-2 ABBREVIATION and a type-3 IDENTIFIER both print a
	 * NAME -- the abbreviation itself for the first, and for the second the
	 * abbreviation tzdata records for that instant, which is a different string
	 * from the identifier ("EDT", not "America/New_York").
	 *
	 * Handing the name down as tm_abbr is how both say so. The specifier's
	 * fallback -- build "GMT±HHMM" from the offset -- is then reached by
	 * exactly the zones that want it, where it used to be reached by any zone
	 * whose offset was not zero: a date in zone `T` printed "GMT-0700" for php's
	 * "T", and so did every other military letter and every abbreviation with an
	 * offset.
	 */
	if( sState.iZoneKind == DT_ZONE_ABBR ){
		sTm.tm_abbr = zZone;
		sTm.tm_nabbr = nName;
#ifdef PH7_ENABLE_TZDB
		{
			/* `I` on an abbreviation is the table's own daylight flag, not
			 * anything about the instant: a date in `EDT` reads 1 forever and
			 * one in `EST` reads 0, because each names one side of the switch
			 * rather than a place that crosses it. */
			sxi32 iAbbrOff;
			int bDst,nCanon;
			const char *zCanon;
			if( PH7_TzAbbrFind(zZone,nName,&iAbbrOff,&bDst,&zCanon,&nCanon) ){
				sTm.tm_isdst = bDst;
			}
		}
#endif
	}else{
		int iTz = DtTzIndex(sState.zName,sState.nName,sState.iZoneKind);
		if( iTz >= 0 ){
			int bDst = 0,nAbbr = 0;
			const char *zAbbr = 0;
			/* The offset is re-read with them rather than trusted: it is the
			 * one field two states could disagree about, and this is the door
			 * that prints it. */
			sxi32 iOff = DtTzOffsetAt(iTz,sState.iOff,sState.iTs,&bDst,&zAbbr,&nAbbr);
			DtFillSytm(sState.iTs,iOff,zZone,&sTm);
			sTm.tm_isdst = bDst;
			sTm.tm_abbr = zAbbr;
			sTm.tm_nabbr = nAbbr;
		}
	}
	DateFormat(pCtx,zFmt,nFmt,&sTm,sState.uSec);
}
/* DateTime::format(string $format) */
static int vm_builtin_DateTime_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	const char *zFmt;
	int nFmt;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zFmt = ph7_value_to_string(apArg[0],&nFmt);
	DtFormatOf(pCtx,pThis,zFmt,nFmt);
	return PH7_OK;
}
/* DateTime::getTimestamp() / getMicrosecond() / getOffset() */
static int vm_builtin_DateTime_getTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_TS));
	}
	return PH7_OK;
}
static int vm_builtin_DateTime_getMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_US));
	}
	return PH7_OK;
}
static int vm_builtin_DateTime_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_OFF));
	}
	return PH7_OK;
}
/* The zone object of a date, built from its stored name and offset, so an
 * identifier PHL stored but cannot re-parse still round-trips. Shared with the
 * date_timezone_get() alias. */
static int DtTimezoneResult(ph7_context *pCtx,ph7_class_instance *pObj)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pZoneClass = DtClass(pVm,"DateTimeZone");
	ph7_class_instance *pZone;
	const char *zName;
	int nName;
	if( pZoneClass == 0 ){
		return PH7_OK;
	}
	pZone = DtNewInstance(pVm,pZoneClass);
	if( pZone == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeAttrStr(pObj,DT_NAME,&zName,&nName);
	PH7_NativeSetAttrInt(pVm,pZone,DTZ_OFF,PH7_NativeAttrInt(pObj,DT_OFF));
	PH7_NativeSetAttrStr(pVm,pZone,DTZ_NAME,zName,nName);
	PH7_NativeSetAttrInt(pVm,pZone,DTZ_KIND,DtZoneKindOf(pObj,DT_ZKIND,zName,nName));
	PH7_NativeResultObject(pCtx,pZone);
	return PH7_OK;
}
/* DateTime::getTimezone() */
static int vm_builtin_DateTime_getTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	return DtTimezoneResult(pCtx,pThis);
}
/* The truth of a value, without converting the caller's copy of it. */
static int DtValueTruth(ph7_vm *pVm,ph7_value *pVal)
{
	ph7_value sTmp;
	int bRes;
	PH7_MemObjInit(&(*pVm),&sTmp);
	PH7_MemObjStore(pVal,&sTmp);
	/* PH7_MemObjToBool converts IN PLACE and returns a STATUS: the answer is in
	 * x.iVal (reading the return is a silent always-false). */
	PH7_MemObjToBool(&sTmp);
	bRes = sTmp.x.iVal != 0;
	PH7_MemObjRelease(&sTmp);
	return bRes;
}
/*
 * Are two dates in the ONE zone, as diff() means it? php compares the loaded
 * timezone STRUCTS, which are cached per spelling, so the test is: both are
 * identifiers, and their names are the same bytes. `America/New_York` and
 * `america/new_york` name one place and fail it; so do `America/New_York` and
 * `US/Eastern`, which are the same data under two names.
 */
static int DtSameZone(ph7_class_instance *pA,ph7_class_instance *pB)
{
	const char *zA,*zB;
	int nA,nB;
	PH7_NativeAttrStr(pA,DT_NAME,&zA,&nA);
	PH7_NativeAttrStr(pB,DT_NAME,&zB,&nB);
	if( DtZoneKindOf(pA,DT_ZKIND,zA,nA) != DT_ZONE_ID
	 || DtZoneKindOf(pB,DT_ZKIND,zB,nB) != DT_ZONE_ID ){
		return 0;
	}
	return nA == nB && (nA == 0 || SyMemcmp(zA,zB,(sxu32)nA) == 0);
}
/* The DateInterval two dates differ by. Shared with the date_diff() alias. */
static int DtDiffResult(ph7_context *pCtx,ph7_class_instance *pBase,
	ph7_class_instance *pTarget,int bAbsolute)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pIvClass = DtClass(pVm,"DateInterval");
	ph7_class_instance *pIv;
	dt_diff sDiff;
	if( pIvClass == 0 ){
		return PH7_OK;
	}
	DtCivilDiff(PH7_NativeAttrInt(pBase,DT_TS),(int)PH7_NativeAttrInt(pBase,DT_US),
		(sxi32)PH7_NativeAttrInt(pBase,DT_OFF),
		PH7_NativeAttrInt(pTarget,DT_TS),(int)PH7_NativeAttrInt(pTarget,DT_US),
		(sxi32)PH7_NativeAttrInt(pTarget,DT_OFF),DtSameZone(pBase,pTarget),&sDiff);
	pIv = DtNewInstance(pVm,pIvClass);
	if( pIv == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeSetAttrInt(pVm,pIv,"y",sDiff.y);
	PH7_NativeSetAttrInt(pVm,pIv,"m",sDiff.m);
	PH7_NativeSetAttrInt(pVm,pIv,"d",sDiff.d);
	PH7_NativeSetAttrInt(pVm,pIv,"h",sDiff.h);
	PH7_NativeSetAttrInt(pVm,pIv,"i",sDiff.i);
	PH7_NativeSetAttrInt(pVm,pIv,"s",sDiff.s);
	DtIvSetUsec(pVm,pIv,sDiff.uSec);
	PH7_NativeSetAttrInt(pVm,pIv,"days",sDiff.nDays);
	PH7_NativeSetAttrInt(pVm,pIv,"invert",bAbsolute ? 0 : sDiff.bInvert);
	PH7_NativeResultObject(pCtx,pIv);
	return PH7_OK;
}
/* DateTime::diff(DateTimeInterface $targetObject, bool $absolute = false) */
static int vm_builtin_DateTime_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	int bAbsolute = 0;
	if( pThis == 0 || nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0
	 || DtArgInit(pCtx,apArg[0]) != 0 ){
		return PH7_OK;
	}
	pTarget = (ph7_class_instance *)apArg[0]->x.pOther;
	if( nArg > 1 ){
		bAbsolute = DtValueTruth(pCtx->pVm,apArg[1]);
	}
	return DtDiffResult(pCtx,pThis,pTarget,bAbsolute);
}
/* DateTime::modify(string $modifier) */
static int vm_builtin_DateTime_modify(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	const char *zMod,*zErr;
	int nMod,iPos,bCopy = 0,iErrPos;
	char cAt;
	sxi64 iTs = 0;
	sxi32 iOff = 0;
	int bOffSet = 0,uSec = 0;
	dt_parsed sVec;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zMod = ph7_value_to_string(apArg[0],&nMod);
	/* php's modify() writes only the fields the string really SET, so a modifier
	 * that names no time of day keeps the receiver's -- DT_PARSE_OVERRIDE_TIME is
	 * php's own flag for exactly that, and the constructor's parse does not pass it. */
	iErrPos = DtParseEx(zMod,nMod,PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),
		(int)PH7_NativeAttrInt(pThis,DT_US),DT_PARSE_OVERRIDE_TIME|DT_PARSE_KEEP_ZONE,
		&iTs,&iOff,&bOffSet,&uSec,&sVec,&pVm->sDtLastErr);
	if( iErrPos != 0 ){
		int bImm = DtIsImmutable(pVm,pThis);
		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);
		return PH7_VmThrowException(pCtx,"DateMalformedStringException",
			"%s::modify(): Failed to parse time string (%.*s) at position %d (%c): %s",
			bImm ? "DateTimeImmutable" : "DateTime",DtCStrLen(zMod,nMod),zMod,iPos,cAt,zErr);
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	DtStoreModified(pVm,pTarget,iTs,(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),&sVec);
	/* The modifier may have moved the SUB-SECOND clock too (`+1 microsecond`,
	 * `+250 ms`) or set it outright (a time of day with a fraction); the parse
	 * started from the object's own, so this is the whole answer either way. */
	PH7_NativeSetAttrInt(pVm,pTarget,DT_US,uSec);
	DtEpochRezone(pVm,pTarget,&sVec);
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* DateTime::setTimestamp(int $timestamp) — php clears the microseconds with it */
static int vm_builtin_DateTime_setTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	int bCopy = 0;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_TS,ph7_value_to_int64(apArg[0]));
	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_US,0);
	/* This door names an INSTANT, so the zone keeps its name and the offset
	 * follows the new timestamp. */
	DtRezone(pCtx->pVm,pTarget);
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* DateTime::setMicrosecond(int $microsecond) */
static int vm_builtin_DateTime_setMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	sxi64 iUs;
	int bCopy = 0;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	iUs = ph7_value_to_int64(apArg[0]);
	if( iUs < 0 || iUs > 999999 ){
		/* php's range refusal, and the reason a date's microseconds can be
		 * assumed to be a fraction of ONE second everywhere else: PHL stored
		 * whatever int it was handed, so `setMicrosecond(1000000)` formatted as
		 * `00:00:00.1000000` and a negative one as `00:00:00.-00001` -- neither
		 * of them a time. The message names the DECLARING class, so a subclass
		 * of DateTime still reports DateTime. */
		return PH7_VmThrowException(pCtx,"DateRangeError",
			"%s::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, %qd given",
			DtIsImmutable(pCtx->pVm,pThis) ? "DateTimeImmutable" : "DateTime",iUs);
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_US,iUs);
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* DateTime::setTimezone(DateTimeZone $timezone) */
static int vm_builtin_DateTime_setTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	const char *zName = "UTC";
	int nName = 3,bCopy = 0,iKind = DT_ZONE_ID;
	sxi32 iOff = 0;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	if( !DtZoneOf(apArg[0],&iOff,&zName,&nName,&iKind) ){
		return PH7_OK;
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_OFF,iOff);
	PH7_NativeSetAttrStr(pCtx->pVm,pTarget,DT_NAME,zName,nName);
	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_ZKIND,iKind);
	/* The instant does not move -- setTimezone() re-expresses it -- so the new
	 * zone's offset is read at the timestamp already there. */
	DtRezone(pCtx->pVm,pTarget);
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* Replace the DATE of an object, keeping its time of day. Shared with the
 * date_date_set() alias. */
static void DtSetDateOf(ph7_context *pCtx,ph7_class_instance *pObj,sxi64 y,int mo,int d)
{
	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);
	sxi64 iDays = DtFloorDiv(iLocal,86400);
	sxi64 iSecs = iLocal - iDays*86400;
	DtStoreLocalOf(pCtx->pVm,pObj,
		DtMakeTs(y,mo,d,(int)(iSecs / 3600),(int)((iSecs / 60) % 60),(int)(iSecs % 60),0));
}
/* Replace the TIME of day, keeping the date. Shared with date_time_set(). */
static void DtSetTimeOf(ph7_context *pCtx,ph7_class_instance *pObj,int h,int mi,int s,sxi64 uSec)
{
	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);
	sxi64 iDays = DtFloorDiv(iLocal,86400);
	sxi64 y;
	int mo,d;
	DtCivilFromDays(iDays,&y,&mo,&d);
	DtStoreLocalOf(pCtx->pVm,pObj,DtMakeTs(y,mo,d,h,mi,s,0));
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,uSec);
}
/* DateTime::setDate(int $year, int $month, int $day) — the time of day is kept */
static int vm_builtin_DateTime_setDate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	int bCopy = 0;
	if( pThis == 0 || nArg < 3 ){
		return PH7_OK;
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	DtSetDateOf(pCtx,pTarget,ph7_value_to_int64(apArg[0]),ph7_value_to_int(apArg[1]),
		ph7_value_to_int(apArg[2]));
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* DateTime::setTime(int $hour, int $minute, int $second = 0, int $microsecond = 0) */
static int vm_builtin_DateTime_setTime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	int bCopy = 0;
	if( pThis == 0 || nArg < 2 ){
		return PH7_OK;
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	DtSetTimeOf(pCtx,pTarget,ph7_value_to_int(apArg[0]),ph7_value_to_int(apArg[1]),
		nArg > 2 ? ph7_value_to_int(apArg[2]) : 0,
		nArg > 3 ? ph7_value_to_int64(apArg[3]) : 0);
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* DateTime::setISODate(int $year, int $week, int $dayOfWeek = 1) */
static int vm_builtin_DateTime_setISODate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	int bCopy = 0;
	if( pThis == 0 || nArg < 2 ){
		return PH7_OK;
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	{
		/* Another civil-fields door: DtIsoDate puts the offset back on, and a
		 * database zone re-solves the reading it produced. */
		sxi32 iOff = (sxi32)PH7_NativeAttrInt(pThis,DT_OFF);
		DtStoreLocalOf(pCtx->pVm,pTarget,
			(sxi64)((sxu64)DtIsoDate(PH7_NativeAttrInt(pThis,DT_TS),iOff,
				ph7_value_to_int64(apArg[0]),ph7_value_to_int64(apArg[1]),
				nArg > 2 ? ph7_value_to_int64(apArg[2]) : 1) + (sxu64)iOff));
	}
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/*
 * One interval applied to a date -- the whole of what add(), sub(), their two
 * procedural aliases and the DatePeriod walk each did by hand.
 *
 * The MICROSECONDS are php's `f`, and php's `f` is a signed count of SECONDS'
 * fractions that moves the clock like any other field: an interval carrying
 * f = 2.5 and nothing else moves it two and a half seconds, and its carry into
 * the second is ordinary floor division (so a sub() past the second borrows).
 * PHL ignored `f` at all four sites, which left `DatePeriod` over a sub-second
 * interval standing STILL -- every step answering the start date.
 *
 * The arithmetic is done on unsigned intermediates: `f` is a whole int64 count
 * of microseconds a script may write anything into, and the sum of two of them
 * is exactly the wrap php's own C arrives at rather than an overflow this build
 * would trap on.
 *
 * iSign is the FINAL direction, `invert` already folded in by whichever caller
 * honours it -- add()/sub() and their aliases do, and the period walk does not
 * (php's own split, below).
 */
static void DtApplyInterval(ph7_vm *pVm,ph7_class_instance *pSrc,ph7_class_instance *pDst,
	ph7_class_instance *pIv,int iSign)
{
	sxi64 iUsIv,iUs,iCarry;
	iUsIv = DtIvUsec(pIv);
	if( iSign < 0 ){
		iUsIv = (sxi64)((sxu64)0 - (sxu64)iUsIv);
	}
	iUs = (sxi64)((sxu64)PH7_NativeAttrInt(pSrc,DT_US) + (sxu64)iUsIv);
	iCarry = DtFloorDiv(iUs,1000000);
	{
		/* DtCivilAdd walks the CIVIL fields and hands back an instant, having
		 * put the source's offset back on at the end. A database zone may not
		 * still be on that offset where it landed -- adding six months to a
		 * January date in New York crosses into daylight time -- so the offset
		 * is peeled back off and the reading re-solved. php's arithmetic is
		 * wall-clock arithmetic for exactly this reason: `+1 day` over a spring
		 * forward is 23 hours of real time and the clock still reads the same.
		 */
		sxi32 iOffSrc = (sxi32)PH7_NativeAttrInt(pSrc,DT_OFF);
		sxi64 iLocal = (sxi64)((sxu64)DtCivilAdd(PH7_NativeAttrInt(pSrc,DT_TS),iOffSrc,
			PH7_NativeAttrInt(pIv,"y"),PH7_NativeAttrInt(pIv,"m"),PH7_NativeAttrInt(pIv,"d"),
			PH7_NativeAttrInt(pIv,"h"),PH7_NativeAttrInt(pIv,"i"),PH7_NativeAttrInt(pIv,"s"),iSign)
			+ (sxu64)iCarry + (sxu64)iOffSrc);
		DtStoreLocalOf(pVm,pDst,iLocal);
	}
	PH7_NativeSetAttrInt(pVm,pDst,DT_US,
		(sxi64)((sxu64)iUs - (sxu64)iCarry * 1000000));
}
/* add()/sub(): one body, the sign is the difference. */
static int DtAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget,*pIv;
	int bCopy = 0;
	if( pThis == 0 || nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0
	 || DtArgInit(pCtx,apArg[0]) != 0 ){
		return PH7_OK;
	}
	pIv = (ph7_class_instance *)apArg[0]->x.pOther;
	if( PH7_NativeAttrInt(pIv,"invert") ){
		iSign = -iSign;   /* an inverted interval subtracts from add() (php) */
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	DtApplyInterval(pCtx->pVm,pThis,pTarget,pIv,iSign);
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
static int vm_builtin_DateTime_add(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtAddSub(pCtx,nArg,apArg,1);
}
static int vm_builtin_DateTime_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtAddSub(pCtx,nArg,apArg,-1);
}
/* DateTime::getLastErrors() — php's array, or `false` when the last parse was clean */
static int vm_builtin_DateTime_getLastErrors(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_dt_lasterr *pErr = &pVm->sDtLastErr;
	ph7_value *pArr,*pWarn,*pErrs,*pVal;
	int k;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !pErr->bSet ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArr = ph7_context_new_array(pCtx);
	pWarn = ph7_context_new_array(pCtx);
	pErrs = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pWarn == 0 || pErrs == 0 || pVal == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	for( k = 0 ; k < pErr->nWarnKept ; k++ ){
		ph7_value_string(pVal,pErr->azWarn[k],-1);
		ph7_array_add_intkey_elem(pWarn,pErr->aWarnPos[k],pVal);
		ph7_value_reset_string_cursor(pVal);
	}
	{
		const phl_dt_diag_row *aRow = (const phl_dt_diag_row *)SyBlobData(&pErr->sErr);
		for( k = 0 ; k < pErr->nErrKept ; k++ ){
			ph7_value_string(pVal,aRow[k].zMsg,-1);
			ph7_array_add_intkey_elem(pErrs,aRow[k].iPos,pVal);
			ph7_value_reset_string_cursor(pVal);
		}
	}
	ph7_value_int(pVal,pErr->nWarn);
	ph7_array_add_strkey_elem(pArr,"warning_count",pVal);
	ph7_array_add_strkey_elem(pArr,"warnings",pWarn);
	ph7_value_int(pVal,pErr->nErr);
	ph7_array_add_strkey_elem(pArr,"error_count",pVal);
	ph7_array_add_strkey_elem(pArr,"errors",pErrs);
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/*
 * The class a static factory builds. php uses LATE STATIC BINDING here, so
 * `D::createFromFormat()` on a subclass answers a D — where the chunk hardcoded
 * the literal class name and always answered a DateTime.
 */
static ph7_class * DtFactoryClass(ph7_context *pCtx,const char *zFallback)
{
	ph7_class *pClass = PH7_ContextCalledClass(pCtx);
	return pClass ? pClass : DtClass(pCtx->pVm,zFallback);
}
/* DateTime::createFromFormat(string $format, string $datetime, ?DateTimeZone $timezone = null) */
static int DtCreateFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);
	ph7_class_instance *pObj;
	dt_ff_res sRes;
	dt_state sState;
	int iZoneKind;
	char zNameBuf[16];
	const char *zZone;
	int nZone;
	sxi32 iZoneOff = 0;
	const char *zFmt,*zIn;
	int nFmt,nIn,iNowUs = 0,iResUs = 0,iTzFf;
	sxi64 iNowFf = 0;
	if( pClass == 0 || nArg < 2 ){
		return PH7_OK;
	}
	zFmt = ph7_value_to_string(apArg[0],&nFmt);
	zIn  = ph7_value_to_string(apArg[1],&nIn);
	zZone = pVm->zDefTz;
	nZone = (int)pVm->nDefTz;
	iZoneKind = DT_ZONE_DEFAULT_KIND;
	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){
		if( DtZoneArgInit(pCtx,apArg[2]) != 0 ){
			return PH7_OK;
		}
		DtZoneOf(apArg[2],&iZoneOff,&zZone,&nZone,&iZoneKind);
	}
	DtNowUs(pCtx->pVm,&iNowFf,&iNowUs);
	/* The same two-step a database zone owes the constructor's parse: read the
	 * scan against the offset at NOW -- every field the format did not fill
	 * comes from that moment -- then re-solve the WALL-CLOCK reading it
	 * produced against the zone. Without it a format that reads a plain local
	 * time answers the instant that reading names in UTC. */
	iTzFf = DtTzIndex(zZone,nZone,iZoneKind);
	if( iTzFf >= 0 ){
		int bDstFf,nAbbrFf;
		const char *zAbbrFf;
		iZoneOff = DtTzOffsetAt(iTzFf,iZoneOff,iNowFf,&bDstFf,&zAbbrFf,&nAbbrFf);
	}
	if( DtFromFormat(zFmt,nFmt,zIn,nIn,&sRes) != 0 ){
		DtLastErrFf(pVm,&sRes.sDiag);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	DtLastErrFf(pVm,&sRes.sDiag);
	sState.iTs = DtFfResolve(&sRes,iNowFf,iNowUs,iZoneOff,&iResUs);
	sState.uSec = iResUs;
	if( sRes.iOffKind == 0 ){
		/* nothing the format read resolved, so the call's own zone stands */
		sState.iOff = iZoneOff;
		sState.zName = zZone;
		sState.nName = nZone;
		sState.iZoneKind = iZoneKind;
#ifdef PH7_ENABLE_TZDB
		if( iTzFf >= 0 ){
			sxi64 iFixed = sState.iTs;
			sxi32 iOffAt = iZoneOff;
			if( PH7_TzLocalToUtc(iTzFf,sState.iTs + iZoneOff,&iFixed,&iOffAt) ){
				sState.iTs = iFixed;
				sState.iOff = iOffAt;
			}
		}
#endif
	}else if( sRes.iOffKind == DT_ZONE_OFFSET ){
		sState.iOff = sRes.iOff;
		sState.nName = DtOffNameSec(zNameBuf,sizeof(zNameBuf),sRes.iOff);
		sState.zName = zNameBuf;
		sState.iZoneKind = DT_ZONE_OFFSET;
	}else{
		sState.iOff = sRes.iOff;
		sState.zName = sRes.zName;
		sState.nName = sRes.nName;
		sState.iZoneKind = sRes.iOffKind;
	}
	pObj = DtNewInstance(pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	DtStore(pVm,pObj,&sState);
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
static int vm_builtin_DateTime_createFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTime");
}
static int vm_builtin_DateTimeImmutable_createFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTimeImmutable");
}
/* createFromImmutable()/createFromMutable()/createFromInterface(): one copy body */
static int DtCopyOf(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);
	ph7_class_instance *pSrc,*pObj;
	dt_state sState;
	if( pClass == 0 || nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0
	 || DtArgInit(pCtx,apArg[0]) != 0 ){
		return PH7_OK;
	}
	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;
	DtLoad(pSrc,&sState);
	pObj = DtNewInstance(pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	DtStore(pVm,pObj,&sState);
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/*
 * php's rendering of the timestamp its DateRangeError names -- its own `%g`:
 * six significant digits, and an exponent form that keeps a fractional digit,
 * so 2^63 prints "9.22337e+18" and 1e19 prints "1.0e+19". NaN and the
 * infinities print as the bare words php prints them as everywhere else.
 *
 * libc is the digit engine (the byte-exact-floats rule the printf family
 * already follows) and PH7_PhpFloatShape turns its output into php's shape.
 */
static void DtRealText(double r,char *zBuf,int nBuf)
{
	int n;
	if( PH7_IS_NAN(r) ){
		SyMemcpy("NAN",zBuf,sizeof("NAN"));
		return;
	}
	if( PH7_IS_INF(r) ){
		SyMemcpy(r < 0 ? "-INF" : "INF",zBuf,r < 0 ? sizeof("-INF") : sizeof("INF"));
		return;
	}
	n = snprintf(zBuf,(size_t)nBuf,"%.6g",r);
	if( n < 0 || n >= nBuf - 2 ){
		zBuf[0] = 0;
		return;
	}
	zBuf[PH7_PhpFloatShape(zBuf,n,1)] = 0;
}
/*
 * DateTime::createFromTimestamp(int|float $timestamp) (php 8.4), and the same
 * on DateTimeImmutable -- the float door onto the clock, and the only factory
 * that reads MICROSECONDS out of its argument.
 *
 * The zone is a fixed +00:00 whatever the default timezone is, exactly as
 * `new DateTime('@0')` answers; the seconds floor and the fraction rounds to
 * the nearest microsecond (php's 1.9999999 is 2.000000, and its -1.9999999 is
 * -2.000000, both of which fall out of taking the floor first).
 */
static int DtCreateFromTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);
	ph7_class_instance *pObj;
	dt_state sState;
	char zNameBuf[16];
	if( pClass == 0 || nArg < 1 ){
		return PH7_OK;
	}
	sState.uSec = 0;
	{
		/* Which ARM of `int|float` the argument satisfies decides the rest, and
		 * a numeric STRING picks its own: php reads "5" as an int and "5.5" as
		 * a float, so the SHAPE of the digits is the test rather than the
		 * value's storage. Everything that is not a number at all was refused
		 * upstream by the declared type. */
		double r = 0.0;
		int bReal = 0;
		if( apArg[0]->iFlags & MEMOBJ_REAL ){
			bReal = 1;
			r = (double)apArg[0]->rVal;
		}else if( apArg[0]->iFlags & MEMOBJ_STRING ){
			int nStr;
			const char *zStr = ph7_value_to_string(apArg[0],&nStr);
			sxi64 iLong;
			double dReal;
			if( RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal) == RANGE_IN_DOUBLE ){
				bReal = 1;
				r = dReal;
			}
		}
		if( !bReal ){
			sState.iTs = ph7_value_to_int64(apArg[0]);
		}else if( !PH7_RealFitsInt64(r) ){
			/* php's own bounds, and its own words for them: the ceiling is
			 * printed as the last microsecond below 2^63 even though the test
			 * is against 2^63 itself (no double lies between the two).
			 * "%z" takes the class name as the length+pointer pair it is. */
			char zVal[64];
			DtRealText(r,zVal,(int)sizeof(zVal));
			return PH7_VmThrowException(pCtx,"DateRangeError",
				"%z::createFromTimestamp(): Argument #1 ($timestamp) must be a finite "
				"number between -9223372036854775808 and 9223372036854775807.999999, "
				"%s given",&pClass->sDisp,zVal);
		}else{
			/* floor(), by hand: <math.h> belongs to the optional math module and
			 * the clock does not depend on it. The C cast truncates toward zero,
			 * so only a negative value with a fraction needs the step down. */
			double fFrac;
			sState.iTs = (sxi64)r;
			if( (double)sState.iTs > r ){
				sState.iTs--;
			}
			fFrac = (r - (double)sState.iTs) * 1000000.0;
			sState.uSec = (int)(fFrac + 0.5);
			if( sState.uSec >= 1000000 ){
				/* The rounding carried into the second (php's 1.9999999). */
				sState.uSec -= 1000000;
				sState.iTs++;
			}
		}
	}
	sState.iOff = 0;
	sState.nName = DtOffName(zNameBuf,sizeof(zNameBuf),0);
	sState.zName = zNameBuf;
	sState.iZoneKind = DT_ZONE_OFFSET;   /* php's fixed `+00:00`, not the UTC id */
	pObj = DtNewInstance(pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	DtStore(pVm,pObj,&sState);
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
static int vm_builtin_DateTime_createFromTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtCreateFromTimestamp(pCtx,nArg,apArg,"DateTime");
}
static int vm_builtin_DateTimeImmutable_createFromTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtCreateFromTimestamp(pCtx,nArg,apArg,"DateTimeImmutable");
}
static int vm_builtin_DateTime_copyOf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtCopyOf(pCtx,nArg,apArg,"DateTime");
}
static int vm_builtin_DateTimeImmutable_copyOf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtCopyOf(pCtx,nArg,apArg,"DateTimeImmutable");
}
/*
 * int|false strtotime(string $datetime, ?int $baseTimestamp = null)
 *
 * Rides the same DtParse the constructor uses, so its format coverage is identical.
 * php: the EMPTY string is false, but whitespace-only is 'now'; a parse failure is
 * false (never an exception), and the default timezone is offset 0 — exactly what
 * the constructor does for a null $timezone.
 */
static int vm_builtin_strtotime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nIn;
	sxi64 iBase;
	sxi64 iTs = 0;
	sxi32 iOff = 0,iZoneOff = 0;
	int bOffSet = 0,uSec = 0,iTz,bDst,nAbbr;
	const char *zAbbr;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	if( nIn < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){
		iBase = ph7_value_to_int64(apArg[1]);
	}else{
		DtNowUs(pCtx->pVm,&iBase,0);
	}
	/* The default zone is the constructor's rule, and it is the same two-step: a
	 * DATABASE zone has no offset until an instant picks one, so the scan runs
	 * against the offset at the BASE moment -- which is what fills in every
	 * field the string leaves out -- and the wall-clock reading it produces is
	 * then re-solved against the zone. A string that named its own offset keeps
	 * it and skips all of this. */
	iTz = DtDefaultTzIndex(pCtx->pVm);
	if( iTz >= 0 ){
		iZoneOff = DtTzOffsetOf(iTz,0,iBase,&bDst,&zAbbr,&nAbbr);
	}
	if( DtParse(zIn,nIn,iBase,iZoneOff,0,&iTs,&iOff,&bOffSet,&uSec) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
#ifdef PH7_ENABLE_TZDB
	if( iTz >= 0 && !bOffSet ){
		sxi64 iFixed = iTs;
		sxi32 iOffAt = iZoneOff;
		if( PH7_TzLocalToUtc(iTz,iTs + iZoneOff,&iFixed,&iOffAt) ){
			iTs = iFixed;
		}
	}
#endif
	ph7_result_int64(pCtx,iTs);
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * DateInterval, DatePeriod and its iterator, declared from C.
 *
 * The rest of the date chunk. DateInterval's two constructors were `preg_match`
 * calls in PHP; DatePeriod's `getIterator()` was a PHP GENERATOR, which a C body
 * cannot be -- so it answers a native `InternalIterator`, which is exactly the
 * class php answers there.
 * ---------------------------------------------------------------------------
 */
/* The MICROSECOND slot of the parsed vector: php keeps it apart from the six
 * relative fields (timelib_rel_time.us), and it does not carry into the seconds
 * the way the CLOCK's does -- `1000000 microseconds` is an interval whose `%f`
 * prints 1000000 and whose `s` is 0. */
#define DT_IV_FIELDS 6
#define DT_IV_USLOT  6
static const char * const azDtIvField[] = { "y", "m", "d", "h", "i", "s" };
/* Read an unsigned run of digits; returns the count consumed. Stops ACCUMULATING
 * past DT_DIGITS_SAFE while still counting, so a caller that is about to refuse
 * an over-wide run does not overflow measuring it (see DT_DIGITS_ISO). */
static int DtIvDigits(const char *z,const char *zEnd,sxi64 *pVal)
{
	int n = 0;
	sxi64 v = 0;
	while( &z[n] < zEnd && SyisDigit(z[n]) ){
		if( n < DT_DIGITS_SAFE ){
			v = v*10 + (z[n] - '0');
		}
		n++;
	}
	*pVal = v;
	return n;
}
/*
 * php's ISO-8601 duration grammar: P[nY][nM][nW][nD][T[nH][nM][nS]], every field
 * an unsigned integer. A bare "P", a trailing "T" and a fractional second are all
 * rejected, as php rejects them.
 */
static int DtIvParseIso(const char *zIn,int nIn,sxi64 *aOut)
{
	const char *z = zIn,*zEnd = &zIn[nIn];
	int bTime = 0,bAny = 0;
	int k;
	for( k = 0 ; k <= DT_IV_USLOT ; k++ ){
		aOut[k] = 0;
	}
	if( nIn < 2 || zIn[0] != 'P' || zIn[nIn-1] == 'T' ){
		return -1;
	}
	z++;
	while( z < zEnd ){
		sxi64 v;
		int n;
		if( z[0] == 'T' ){
			if( bTime ){
				return -1;
			}
			bTime = 1;
			z++;
			continue;
		}
		n = DtIvDigits(z,zEnd,&v);
		if( n == 0 || n > DT_DIGITS_ISO || z + n >= zEnd ){
			/* php's duration fields stop at twelve digits: `P999999999999D` is an
			 * interval there and `P9999999999999D` is "Unknown or bad format". */
			return -1;
		}
		z += n;
		switch( z[0] ){
			case 'Y': if( bTime ){ return -1; } aOut[0] += v; break;
			case 'W': if( bTime ){ return -1; } aOut[2] += v * 7; break;
			case 'D': if( bTime ){ return -1; } aOut[2] += v; break;
			case 'H': if( !bTime ){ return -1; } aOut[3] += v; break;
			case 'S': if( !bTime ){ return -1; } aOut[5] += v; break;
			case 'M':
				/* The one ambiguous designator: months before T, minutes after. */
				if( bTime ){ aOut[4] += v; }else{ aOut[1] += v; }
				break;
			default:
				return -1;
		}
		z++;
		bAny = 1;
	}
	return bAny ? 0 : -1;
}
/*
 * php's relative-string interval. The string is VALIDATED by the same parser
 * strtotime() uses -- which is where php's "at position N (c): reason" wording
 * comes from -- and the number/unit pairs it understands are then summed. A
 * string the parser accepts but that names no unit ("next monday") is php's
 * all-zero interval, not an error.
 */
static int DtIvParseRelative(const char *zIn,int nIn,sxi64 *aOut,int *piPos,
	char *pcAt,const char **pzReason)
{
	dt_parsed sVec;
	sxi64 iTs = 0;
	sxi32 iOff = 0;
	int bOffSet = 0,uSec = 0,iErr;
	int k;
	for( k = 0 ; k <= DT_IV_USLOT ; k++ ){
		aOut[k] = 0;
	}
	if( nIn < 1 ){
		*piPos = 0;
		*pcAt = ' ';
		*pzReason = "Empty string";
		return -1;
	}
	/* createFromDateString() does NOT publish getLastErrors() in php: the record
	 * keeps whatever the last constructor or modify() left in it. */
	iErr = DtParseEx(zIn,nIn,0,0,0,0,&iTs,&iOff,&bOffSet,&uSec,&sVec,0);
	if( iErr != 0 ){
		*pzReason = DtParseErr(zIn,nIn,iErr,piPos,pcAt);
		return -1;
	}
	/* php REFUSES a string carrying any absolute element -- a date, a time of day
	 * or a zone -- rather than reading an interval out of what is left: it checks
	 * the same three flags the parse already carries. A relative NAVIGATION word
	 * is not one of them, which is what makes `tomorrow` the interval d = 1. */
	if( sVec.bHaveDate || sVec.nTimeTok || sVec.bOffSet ){
		return -2;
	}
	/* The vector IS the interval: php reads timelib_rel_time's own fields, so a
	 * week is already days there and the microseconds stand apart from the
	 * seconds. */
	aOut[0] = sVec.ry;
	aOut[1] = sVec.rm;
	aOut[2] = sVec.rd;
	aOut[3] = sVec.rh;
	aOut[4] = sVec.ri;
	aOut[5] = sVec.rs;
	aOut[DT_IV_USLOT] = sVec.rus;
	return 0;
}
/*
 * php's from-string interval is a different OBJECT: it keeps the STRING and
 * presents `from_string` and `date_string` alone, answering the ten fields from
 * what it parsed whenever a script asks for one. PHL fills the ten as it always
 * did -- every reader, the write filter, format(), add()/sub() and DatePeriod go
 * on reading real slots -- and hides them from the surfaces that SHOW the
 * object, which is the whole of the difference php's shape makes.
 */
static void DtIvFromString(ph7_vm *pVm,ph7_class_instance *pObj,const char *zIn,int nIn)
{
	static const char * const azHide[] = {
		"y","m","d","h","i","s","f","invert","days"
	};
	int k;
	PH7_NativeSetAttrBool(&(*pVm),pObj,"from_string",1);
	/* the ON-DEMAND slot: this write is what puts the name on the object, and it
	 * lands behind `from_string`, which is php's order */
	PH7_NativeSetAttrStr(&(*pVm),pObj,"date_string",zIn,nIn);
	for( k = 0 ; k < (int)SX_ARRAYSIZE(azHide) ; k++ ){
		PH7_NativeHideAttr(pObj,azHide[k]);
	}
}
/* Write the six relative fields onto a DateInterval instance. */
static void DtIvStore(ph7_vm *pVm,ph7_class_instance *pObj,const sxi64 *aVal)
{
	int k;
	for( k = 0 ; k < DT_IV_FIELDS ; k++ ){
		PH7_NativeSetAttrInt(pVm,pObj,azDtIvField[k],aVal[k]);
	}
	/* ...and the microseconds through the pair that owns them, so `f` and the
	 * hidden count stay one value. */
	DtIvSetUsec(pVm,pObj,aVal[DT_IV_USLOT]);
}
/*
 * php's date_interval_write_property: what a write to one of DateInterval's
 * properties CONVERTS to, since every one of them is a field of php's own C
 * struct rather than a slot a script's value lands in.
 *
 * The six relative fields and `invert` take php's int cast — a float truncates
 * and warns where it wraps, a string reads its numeric prefix, an array is 1 —
 * with `invert` narrowed to the 32-bit `int` timelib declares it as (so
 * `$i->invert = 3000000000` is -1294967296 in both engines). `f` is the
 * microsecond count above, so its cast warning is raised HERE, on the SCALED
 * value, which is where php raises it.
 *
 * `days` and `from_string` are answered by php's read handler and refused by its
 * write one: a script that assigns them creates a deprecated DYNAMIC property
 * that never reaches the interval (`$i->days = 5` leaves `$i->days` false
 * there). The scope policy refuses a deprecation, and PHL refuses a dynamic property outright,
 * so the two meet at the Error PHL already raises for `$i->anythingElse = v`.
 */
static void DtIntervalSet(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)
{
	const char *zName = SyStringData(pCtx->pName);
	sxu32 nName = SyStringLength(pCtx->pName);
	ph7_value *pVal = pCtx->pValue;
	int bInvert;
	if( nName == sizeof("days")-1 && SyMemcmp(zName,"days",nName) == 0 ){
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
			"Cannot create dynamic property DateInterval::$days");
		pCtx->zThrowClass = "Error";
		return;
	}
	if( nName == sizeof("from_string")-1 && SyMemcmp(zName,"from_string",nName) == 0 ){
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
			"Cannot create dynamic property DateInterval::$from_string");
		pCtx->zThrowClass = "Error";
		return;
	}
	if( nName == sizeof("date_string")-1 && SyMemcmp(zName,"date_string",nName) == 0 ){
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
			"Cannot create dynamic property DateInterval::$date_string");
		pCtx->zThrowClass = "Error";
		return;
	}
	if( nName == sizeof("f")-1 && zName[0] == 'f' ){
		double r = (double)PH7_ValuePeekReal(pVal);
		sxi64 us;
		PH7_RealWarnIntCast(pVm,r * 1000000.0);
		us = DtIvUsecOfReal(r);
		PH7_NativeSetAttrInt(pVm,pThis,DT_IV_US,us);
		PH7_MemObjRelease(pVal);
		PH7_MemObjInitFromReal(pVm,pVal,(ph7_real)((double)us / 1000000.0));
		return;
	}
	bInvert = nName == sizeof("invert")-1 && SyMemcmp(zName,"invert",nName) == 0;
	if( !bInvert ){
		int k;
		for( k = 0 ; k < (int)SX_ARRAYSIZE(azDtIvField) ; k++ ){
			if( nName == 1 && zName[0] == azDtIvField[k][0] ){
				break;
			}
		}
		if( k >= (int)SX_ARRAYSIZE(azDtIvField) ){
			return;   /* the hidden count slot: written from C, never through here */
		}
	}
	{
		/* y/m/d/h/i/s and invert, all of them php's int cast. */
		sxi64 iVal;
		PH7_MemObjWarnIntCast(pVal);
		iVal = PH7_ValuePeekInt64(pVal);
		if( bInvert ){
			iVal = (sxi64)(sxi32)iVal;
		}
		PH7_MemObjRelease(pVal);
		PH7_MemObjInitFromInt(pVm,pVal,iVal);
	}
}
/* DateInterval::__construct(string $duration) */
static int vm_builtin_DateInterval_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */
	const char *zDur;
	int nDur;
	sxi64 aVal[DT_IV_USLOT + 1];
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zDur = ph7_value_to_string(apArg[0],&nDur);
	if( DtIvParseIso(zDur,nDur,aVal) != 0 ){
		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",
			"Unknown or bad format (%.*s)",DtCStrLen(zDur,nDur),zDur);
	}
	DtIvStore(pCtx->pVm,pThis,aVal);
	DtSetInit(pCtx->pVm,pThis);
	return PH7_OK;
}
/*
 * DateInterval::createFromDateString(string $datetime). Shared with the
 * date_interval_create_from_date_string() alias, which WARNS and answers false
 * where the method throws.
 */
static ph7_class_instance * DtIvFromDateString(ph7_context *pCtx,const char *zIn,int nIn,
	int *piPos,char *pcAt,const char **pzReason,int *pbNonRel)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = DtFactoryClass(pCtx,"DateInterval");
	ph7_class_instance *pObj;
	sxi64 aVal[DT_IV_USLOT + 1];
	int rc;
	*pbNonRel = 0;
	if( pClass == 0 ){
		return 0;
	}
	if( (rc = DtIvParseRelative(zIn,nIn,aVal,piPos,pcAt,pzReason)) != 0 ){
		*pbNonRel = (rc == -2);
		return 0;
	}
	pObj = DtNewInstance(pVm,pClass);
	if( pObj == 0 ){
		return 0;
	}
	DtIvStore(pVm,pObj,aVal);
	DtIvFromString(pVm,pObj,zIn,nIn);
	return pObj;
}
static int vm_builtin_DateInterval_createFromDateString(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zReason = "";
	int nIn,iPos = 0,bNonRel = 0;
	char cAt = ' ';
	ph7_class_instance *pObj;
	if( nArg < 1 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason,&bNonRel);
	if( pObj == 0 ){
		if( bNonRel ){
			return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",
				"String '%.*s' contains non-relative elements",DtCStrLen(zIn,nIn),zIn);
		}
		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",
			"Unknown or bad format (%.*s) at position %d (%c): %s",
			DtCStrLen(zIn,nIn),zIn,iPos,cAt,zReason);
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/*
 * DateInterval::format(string $format) -- php's own %-token loop, including the
 * rule the chunk got wrong: an UNKNOWN token keeps its '%' (`%q` is "%q").
 */
static void DtIvFormat(ph7_context *pCtx,ph7_class_instance *pObj,const char *zFmt,int nFmt)
{
	SyBlob sOut;
	ph7_value *pDays;
	int k;
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	for( k = 0 ; k < nFmt ; k++ ){
		char c = zFmt[k];
		char t;
		if( c != '%' ){
			SyBlobAppend(&sOut,&c,1);
			continue;
		}
		k++;
		if( k >= nFmt ){
			/* php drops a trailing lone '%' rather than echoing it. */
			break;
		}
		t = zFmt[k];
		switch( t ){
			/* php prints five of the six through an `(int)` — a 32-bit NARROWING
			 * of a property it stores as an int64 and hands back whole, so
			 * `$i->y = 7960523868075137518` reads back in full and prints
			 * 111352302. The SECONDS are the exception: php formats those with
			 * its long specifier, in both cases. */
			case 'Y': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"y")); break;
			case 'y': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"y")); break;
			case 'M': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"m")); break;
			case 'm': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"m")); break;
			case 'D': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"d")); break;
			case 'd': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"d")); break;
			case 'H': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"h")); break;
			case 'h': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"h")); break;
			case 'I': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"i")); break;
			case 'i': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"i")); break;
			case 'S': SyBlobFormat(&sOut,"%02qd",PH7_NativeAttrInt(pObj,"s")); break;
			case 's': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"s")); break;
			case 'F': case 'f': {
				/* php prints the STORED microsecond count, which is why
				 * `$i->f = 0.1234567` prints 123456 rather than the 123457 a
				 * rounding of the float would give, and why a count no double
				 * holds exactly still prints its own digits. The conversion the
				 * cast contract lives in — truncate toward zero, wrap what no
				 * int64 holds, 0 for a NaN or an infinity, and php's warning
				 * beside it — happens at the property WRITE, where php does it. */
				sxi64 uS = DtIvUsec(pObj);
				if( t == 'F' ){
					SyBlobFormat(&sOut,"%06qd",uS);
				}else{
					SyBlobFormat(&sOut,"%qd",uS);
				}
				break;
			}
			case 'R': SyBlobAppend(&sOut,PH7_NativeAttrInt(pObj,"invert") ? "-" : "+",1); break;
			case 'r': if( PH7_NativeAttrInt(pObj,"invert") ){ SyBlobAppend(&sOut,"-",1); } break;
			case 'a':
				pDays = PH7_NativeAttr(pObj,"days");
				if( pDays && (pDays->iFlags & MEMOBJ_INT) ){
					SyBlobFormat(&sOut,"%qd",pDays->x.iVal);
				}else{
					SyBlobAppend(&sOut,"(unknown)",sizeof("(unknown)")-1);
				}
				break;
			case '%': SyBlobAppend(&sOut,"%",1); break;
			default:
				/* php keeps BOTH bytes of an unrecognised token. */
				SyBlobAppend(&sOut,"%",1);
				SyBlobAppend(&sOut,&t,1);
				break;
		}
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
}
static int vm_builtin_DateInterval_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	const char *zFmt;
	int nFmt;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zFmt = ph7_value_to_string(apArg[0],&nFmt);
	DtIvFormat(pCtx,pThis,zFmt,nFmt);
	return PH7_OK;
}
/* Is this value an instance of the named class? */
static int DtValueIsA(ph7_vm *pVm,ph7_value *pVal,const char *zClass)
{
	ph7_class *pClass;
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pClass = DtClass(&(*pVm),zClass);
	return pClass != 0
		&& PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass);
}
/*
 * Write an object (or null) into a declared property of another object.
 *
 * The scratch value ALIASES the instance rather than owning it, and
 * PH7_MemObjStore takes the reference the slot keeps -- so releasing the scratch
 * afterwards would hand back the slot's own reference and free the object out from
 * under it (which is what it did: `foreach` over a DatePeriod crashed on the second
 * element's `->format()`). The caller keeps owning whatever it passed in.
 */
/*
 * DatePeriod::__construct($start, $interval, $end, $options)
 *
 * php overloads it three ways and rejects everything else with ONE message, which
 * is why the signature stays unenforced and the shapes are checked here.
 */
/* php's ceiling for a recurrence count, and it is checked TWICE: once on the
 * number the caller wrote, and once on that number plus the dates the OPTIONS
 * add -- two different exception classes and two different sentences. */
#define DP_REC_LIMIT 2147483640
static int DpConstructInto(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,ph7_value **apArg,
	const char *zIsoStartClass,const char *zCallee)
{
	ph7_vm *pVm = pCtx->pVm;
	sxi64 iOptions = 0;
	static const char *zBadArgs =
		"DatePeriod::__construct() accepts (DateTimeInterface, DateInterval, int [, int]), "
		"or (DateTimeInterface, DateInterval, DateTime [, int]), or (string [, int]) as arguments";
	if( pThis == 0 || nArg < 1 ){
		return PH7_VmThrowException(pCtx,"TypeError","%s",zBadArgs);
	}
	if( apArg[0]->iFlags & MEMOBJ_STRING ){
		/* The ISO-8601 form: "R<n>/<start>/<duration>". php's second argument is
		 * then the OPTIONS bitmask, not an interval. */
		const char *zSpec = (const char *)SyBlobData(&apArg[0]->sBlob);
		int nSpec = (int)SyBlobLength(&apArg[0]->sBlob);
		const char *zStart,*zDur;
		int nStart,nDur,k;
		sxi64 nRec = 0;
		sxi64 aIv[DT_IV_USLOT + 1];
		dt_state sState;
		char zNameBuf[16];
		const char *zErr;
		int iPos,nDigits;
		char cAt;
		ph7_class_instance *pStart,*pIv;
		if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_INT) ){
			iOptions = apArg[1]->x.iVal;
		}
		if( nSpec < 2 || zSpec[0] != 'R' ){
			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",
				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);
		}
		nDigits = DtIvDigits(&zSpec[1],&zSpec[nSpec],&nRec);
		k = 1 + nDigits;
		if( nDigits == 0 || k >= nSpec || zSpec[k] != '/' ){
			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",
				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);
		}
		if( nDigits > 9 ){
			/* php's ISO scanner reads at most NINE digits of the count and drops
			 * the rest on the floor -- `R2147483639/...` is 214748363 recurrences
			 * there, and `R99999999999999999999/...` is 999999999. Reading them
			 * all was a silent wrong answer here. */
			nRec = 0;
			DtIvDigits(&zSpec[1],&zSpec[1 + 9],&nRec);
		}
		if( nRec < 1 ){
			/* `R0` is php's refusal, and it is worded as a MISSING count rather
			 * than an out-of-range one. */
			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",
				"%s(): ISO interval must contain an end date or a recurrence count, "
				"\"%.*s\" given",zCallee,nSpec,zSpec);
		}
		zStart = &zSpec[k+1];
		nStart = 0;
		while( &zStart[nStart] < &zSpec[nSpec] && zStart[nStart] != '/' ){
			nStart++;
		}
		if( &zStart[nStart] >= &zSpec[nSpec] ){
			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",
				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);
		}
		zDur = &zStart[nStart+1];
		nDur = (int)(&zSpec[nSpec] - zDur);
		if( DtInitState(pCtx,zStart,nStart,0,pVm->zDefTz,(int)pVm->nDefTz,
			DT_ZONE_DEFAULT_KIND,&sState,
			zNameBuf,sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0
		 || DtIvParseIso(zDur,nDur,aIv) != 0 ){
			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",
				"Unknown or bad format (%.*s)",DtCStrLen(zSpec,nSpec),zSpec);
		}
		/* php's two ISO entry points disagree on the class they build, and both
		 * answers are load-bearing: `new DatePeriod("R2/...")` yields DateTime
		 * where DatePeriod::createFromISO8601String() yields DateTimeImmutable. */
		pStart = DtNewInstance(pVm,DtClass(pVm,zIsoStartClass));
		pIv = DtNewInstance(pVm,DtClass(pVm,"DateInterval"));
		if( pStart == 0 || pIv == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		DtStore(pVm,pStart,&sState);
		DtIvStore(pVm,pIv,aIv);
		PH7_NativeSetAttrObj(pVm,pThis,"start",pStart);
		PH7_NativeSetAttrObj(pVm,pThis,"interval",pIv);
		PH7_ClassInstanceUnref(pStart);
		PH7_ClassInstanceUnref(pIv);
		PH7_NativeSetAttrInt(pVm,pThis,"recurrences",nRec + 1);
	}else{
		ph7_class_instance *pStart,*pIv,*pEnd;
		if( !DtValueIsA(pVm,apArg[0],"DateTimeInterface")
		 || nArg < 3
		 || !DtValueIsA(pVm,apArg[1],"DateInterval")
		 || ((apArg[2]->iFlags & MEMOBJ_INT) == 0
		     && !DtValueIsA(pVm,apArg[2],"DateTimeInterface")) ){
			return PH7_VmThrowException(pCtx,"TypeError","%s",zBadArgs);
		}
		/* An unconstructed date on either end. php's sentence here names the
		 * INTERFACE its argument is declared as and not the object's own class --
		 * a subclass of DateTime is still reported as "DateTimeInterface" -- so
		 * this one door words the refusal itself. (php reaches the INTERVAL
		 * argument's state without a screen at all and segfaults on an
		 * unconstructed one; PHL refuses it the way every other door does, which
		 * is the scope policy.) */
		if( DtArgInitNamed(pCtx,apArg[0],"DateTimeInterface") != 0
		 || DtArgInit(pCtx,apArg[1]) != 0
		 || DtArgInitNamed(pCtx,apArg[2],"DateTimeInterface") != 0 ){
			return PH7_OK;
		}
		if( nArg > 3 ){
			iOptions = ph7_value_to_int64(apArg[3]);
		}
		if( apArg[2]->iFlags & MEMOBJ_INT ){
			/* php's two range checks on a recurrence COUNT, in its order and with
			 * its two exception classes: the bare number first, then the number
			 * plus the dates the options ask for (the start date unless
			 * EXCLUDE_START_DATE, and the end date if INCLUDE_END_DATE) -- which
			 * is why `2147483639` alone is refused while the same count with
			 * EXCLUDE_START_DATE is built. PHL accepted every one of them,
			 * `0` and `-1` included, and iterated a period php refuses to make. */
			sxi64 iRec = apArg[2]->x.iVal;
			sxi64 iWithOpt;
			if( iRec < 1 || iRec >= DP_REC_LIMIT ){
				return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",
					"%s(): Recurrence count must be greater or equal to 1 and lower than %d",
					zCallee,DP_REC_LIMIT);
			}
			/* Only now: the sum is computed on a count already known to be under
			 * the ceiling, so the two dates the options may add cannot overflow it
			 * (PHP_INT_MAX + 1 did, and UBSan said so). */
			iWithOpt = iRec + ((iOptions & 1) == 0 ? 1 : 0)
			         + ((iOptions & 2) != 0 ? 1 : 0);
			if( iWithOpt >= DP_REC_LIMIT ){
				return PH7_VmThrowException(pCtx,"DateMalformedStringException",
					"%s(): Recurrence count must be greater or equal to 1 and lower than %d "
					"(including options)",zCallee,DP_REC_LIMIT);
			}
		}
		pStart = PH7_CloneClassInstance((ph7_class_instance *)apArg[0]->x.pOther);
		pIv = (ph7_class_instance *)apArg[1]->x.pOther;
		if( pStart == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		PH7_NativeSetAttrObj(pVm,pThis,"start",pStart);
		PH7_ClassInstanceUnref(pStart);
		PH7_NativeSetAttrObj(pVm,pThis,"interval",pIv);
		if( apArg[2]->iFlags & MEMOBJ_INT ){
			PH7_NativeSetAttrInt(pVm,pThis,"recurrences",apArg[2]->x.iVal + 1);
		}else{
			pEnd = PH7_CloneClassInstance((ph7_class_instance *)apArg[2]->x.pOther);
			if( pEnd == 0 ){
				return PH7_ContextMemoryError(pCtx);
			}
			PH7_NativeSetAttrObj(pVm,pThis,"end",pEnd);
			PH7_ClassInstanceUnref(pEnd);
			/* php's own answer for a period bounded by a DATE rather than a count,
			 * and it has to be written rather than defaulted now that an
			 * unconstructed period reads 0. */
			PH7_NativeSetAttrInt(pVm,pThis,"recurrences",1);
		}
	}
	PH7_NativeSetAttrBool(pVm,pThis,"include_start_date",(iOptions & 1) == 0);
	PH7_NativeSetAttrBool(pVm,pThis,"include_end_date",(iOptions & 2) != 0);
	DtSetInit(pVm,pThis);
	return PH7_OK;
}
static int vm_builtin_DatePeriod_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThisRaw(pCtx);   /* the door that INITIALIZES */
	if( pThis == 0 ){
		return PH7_OK;
	}
	return DpConstructInto(pCtx,pThis,nArg,apArg,"DateTime","DatePeriod::__construct");
}
/* DatePeriod::createFromISO8601String(string $specification, int $options = 0) */
static int vm_builtin_DatePeriod_createFromISO8601String(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = DtFactoryClass(pCtx,"DatePeriod");
	ph7_class_instance *pObj;
	sxi32 rc;
	if( pClass == 0 || nArg < 1 ){
		return PH7_OK;
	}
	/* Not DtNewInstance(): this object is INITIALIZED by the shared constructor
	 * body below, and only if that succeeds. */
	pObj = PH7_NewClassInstance(pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* php's factory IS the constructor, with the same overloaded argument shape. */
	rc = DpConstructInto(pCtx,pObj,nArg,apArg,"DateTimeImmutable",
		"DatePeriod::createFromISO8601String");
	if( rc != PH7_OK || pCtx->nThrowRc != 0 ){
		PH7_ClassInstanceUnref(pObj);
		return rc;
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
static int vm_builtin_DatePeriod_getStartDate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		ph7_value *pVal = PH7_NativeAttr(pThis,"start");
		if( pVal ){
			ph7_result_value(pCtx,pVal);
		}
	}
	return PH7_OK;
}
/* php's two NULLABLE getters read the struct without screening it, so an
 * unconstructed period answers null from both where every other door raises. */
static int vm_builtin_DatePeriod_getEndDate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThisRaw(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		ph7_value *pVal = PH7_NativeAttr(pThis,"end");
		if( pVal ){
			ph7_result_value(pCtx,pVal);
		}
	}
	return PH7_OK;
}
static int vm_builtin_DatePeriod_getDateInterval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		ph7_value *pVal = PH7_NativeAttr(pThis,"interval");
		if( pVal ){
			ph7_result_value(pCtx,pVal);
		}
	}
	return PH7_OK;
}
/*
 * DatePeriod::getRecurrences() -- php answers NULL for a period bounded by an END
 * DATE and the recurrence COUNT otherwise, which is `recurrences - 1` (php stores
 * the count of dates, one more than the recurrences). No private slot needed.
 */
static int vm_builtin_DatePeriod_getRecurrences(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThisRaw(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || !DtIsInit(pThis) || PH7_NativeAttrObj(pThis,"end") != 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,"recurrences") - 1);
	return PH7_OK;
}
/*
 * The period walk, expressed as the vtable an InternalIterator drives (oo_native.c).
 * It uses the shared cursor slots: SRC is the period, CUR the date the cursor sits
 * on, KEY the emitted position and POS the loop counter (which differs from KEY,
 * since an excluded start date is stepped over without emitting one).
 */
#define DP_IT_STEP PH7_NATIVE_IT_POS
/*
 * One interval step from a date object: a NEW object, so a value already handed
 * to the caller is never mutated underneath it (php's iterator answers a fresh
 * object per position too).
 *
 * The step always ADDS, whatever the interval's `invert` says -- php's period
 * walk reads the fields and not the flag, so a period built on an interval a
 * diff() answered (or on `$iv->invert = 1`) still runs FORWARD, while a
 * negative FIELD (`createFromDateString('-1 day')` leaves d = -1 and invert 0)
 * really does step backward. PHL honoured the flag, so such a period walked the
 * wrong way -- and with an END date rather than a recurrence count it walked
 * away from that end, stopped by nothing.
 */
static ph7_class_instance * DpAdvance(ph7_vm *pVm,ph7_class_instance *pCur,
	ph7_class_instance *pIv)
{
	ph7_class_instance *pNext = PH7_CloneClassInstance(pCur);
	if( pNext == 0 ){
		return 0;
	}
	DtApplyInterval(&(*pVm),pCur,pNext,pIv,1);
	return pNext;
}
/*
 * Settle the cursor on the next date the period EMITS, mirroring the generator
 * this replaced: a start excluded by EXCLUDE_START_DATE is stepped over, an end
 * date stops the walk (inclusively under INCLUDE_END_DATE) and a recurrence count
 * bounds the number of steps instead.
 */
static void DpSettle(ph7_vm *pVm,ph7_class_instance *pIt)
{
	ph7_class_instance *pPeriod = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);
	ph7_class_instance *pEnd,*pIv;
	int bInclStart,bInclEnd;
	sxi64 nTotal;
	if( pPeriod == 0 ){
		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
		return;
	}
	pEnd = PH7_NativeAttrObj(pPeriod,"end");
	pIv = PH7_NativeAttrObj(pPeriod,"interval");
	bInclStart = PH7_NativeAttrTruthy(pPeriod,"include_start_date");
	bInclEnd = PH7_NativeAttrTruthy(pPeriod,"include_end_date");
	nTotal = PH7_NativeAttrInt(pPeriod,"recurrences") + (bInclEnd ? 1 : 0);
	for(;;){
		ph7_class_instance *pCur = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_CUR);
		sxi64 iStep = PH7_NativeAttrInt(pIt,DP_IT_STEP);
		ph7_class_instance *pNext;
		if( pCur == 0 ){
			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
			return;
		}
		if( pEnd != 0 ){
			sxi64 iTs = PH7_NativeAttrInt(pCur,DT_TS);
			sxi64 iEndTs = PH7_NativeAttrInt(pEnd,DT_TS);
			if( bInclEnd ? (iTs > iEndTs) : (iTs >= iEndTs) ){
				PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
				return;
			}
		}else if( iStep >= nTotal ){
			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
			return;
		}
		if( iStep > 0 || bInclStart ){
			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);
			return;
		}
		/* The excluded start: step over it without emitting a key. */
		if( pIv == 0 ){
			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
			return;
		}
		pNext = DpAdvance(&(*pVm),pCur,pIv);
		if( pNext == 0 ){
			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
			return;
		}
		PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_CUR,pNext);
		PH7_ClassInstanceUnref(pNext);
		PH7_NativeSetAttrInt(&(*pVm),pIt,DP_IT_STEP,iStep + 1);
	}
}
static void DpRewind(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_class_instance *pPeriod,*pStart,*pCur;
	pPeriod = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_SRC);
	pStart = pPeriod ? PH7_NativeAttrObj(pPeriod,"start") : 0;
	pCur = pStart ? PH7_CloneClassInstance(pStart) : 0;
	if( pCur == 0 ){
		PH7_NativeSetAttrBool(pVm,pThis,PH7_NATIVE_IT_DONE,1);
		return;
	}
	PH7_NativeSetAttrObj(pVm,pThis,PH7_NATIVE_IT_CUR,pCur);
	PH7_ClassInstanceUnref(pCur);
	PH7_NativeSetAttrInt(pVm,pThis,PH7_NATIVE_IT_KEY,0);
	PH7_NativeSetAttrInt(pVm,pThis,DP_IT_STEP,0);
	DpSettle(pVm,pThis);
}
static void DpNext(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_class_instance *pPeriod,*pIv,*pCur,*pNext;
	pPeriod = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_SRC);
	pIv = pPeriod ? PH7_NativeAttrObj(pPeriod,"interval") : 0;
	pCur = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_CUR);
	pNext = (pIv && pCur) ? DpAdvance(pVm,pCur,pIv) : 0;
	if( pNext == 0 ){
		PH7_NativeSetAttrBool(pVm,pThis,PH7_NATIVE_IT_DONE,1);
		return;
	}
	PH7_NativeSetAttrObj(pVm,pThis,PH7_NATIVE_IT_CUR,pNext);
	PH7_ClassInstanceUnref(pNext);
	PH7_NativeSetAttrInt(pVm,pThis,DP_IT_STEP,PH7_NativeAttrInt(pThis,DP_IT_STEP) + 1);
	PH7_NativeSetAttrInt(pVm,pThis,PH7_NATIVE_IT_KEY,PH7_NativeAttrInt(pThis,PH7_NATIVE_IT_KEY) + 1);
	DpSettle(pVm,pThis);
}
/*
 * php's DatePeriod::$current IS the walk's cursor: the date the iterator sits on,
 * and -- once the walk is over -- the one PAST the end, the date that failed the
 * test. php writes it from the iterator's METHODS rather than from the walk, so a
 * getIterator() nobody has touched yet leaves it where the last walk left it, and
 * the first valid()/current()/key()/rewind()/next() moves it. PHL left it NULL
 * forever, so a program reading the period mid-walk (or after one) saw nothing.
 */
static void DpPublish(ph7_vm *pVm,ph7_class_instance *pIt)
{
	ph7_class_instance *pPeriod = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);
	if( pPeriod == 0 ){
		return;
	}
	PH7_NativeSetAttrObj(&(*pVm),pPeriod,"current",PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_CUR));
}
/*
 * php refuses the WALK of an unconstructed period, not the door to it: its
 * getIterator() hands back a real InternalIterator and the DateObjectError
 * arrives at the first rewind(). The sentence is the ITERATOR's too, and it
 * differs from every other one in this family -- it names DatePeriod plainly
 * whatever the object's own class is, where a method called on a subclass reports
 * `SubP (inheriting DatePeriod)`.
 */
static int DpIterGuard(ph7_context *pCtx,ph7_class_instance *pIt)
{
	ph7_class_instance *pPeriod = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);
	if( pPeriod != 0 && DtIsInit(pPeriod) ){
		return 0;
	}
	PH7_VmThrowException(pCtx,"DateObjectError",
		"Object of type DatePeriod has not been correctly initialized by calling "
		"parent::__construct() in its constructor");
	return 1;
}
static const PH7_NativeIterVtab sDpIterVtab = { DpRewind, DpNext, DpPublish, DpIterGuard };
/*
 * DatePeriod::getIterator(): Iterator
 *
 * This was a PHP GENERATOR, the one thing a C body cannot be. php answers an
 * InternalIterator here, so PHL answers the shared one (oo_native.c) driven by
 * the vtable above -- and stops diverging on `get_class($period->getIterator())`.
 * A fresh one per call, as php's is.
 */
static int vm_builtin_DatePeriod_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	/* Raw: php's door does not screen the struct -- the ITERATOR does, at the
	 * first walk (DpIterGuard). */
	ph7_class_instance *pThis = DtThisRaw(pCtx);
	ph7_class_instance *pIt;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);
	if( pIt == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pIt);
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * The procedural date API.
 *
 * php's aliases are functions in their own right, not forwards: they reach the
 * same implementation the methods do, so an overridden method in a subclass is
 * NOT what they call, and the ones that can fail WARN and answer false where the
 * method throws. Each owes aBuiltinSig[] a row (vm_arg_check.c).
 * ---------------------------------------------------------------------------
 */
/*
 * The receiver argument of a procedural alias (already type-screened by its row),
 * or NULL for an object that was never constructed -- php's aliases reach the same
 * implementation the methods do, so they raise the same DateObjectError there
 * rather than warning the way the aliases that can FAIL do. Every caller already
 * treats a null the way the methods treat a null receiver: nothing to do, PH7_OK,
 * and the parked status reported at the host-call boundary.
 */
static ph7_class_instance * DtArgObj(ph7_context *pCtx,int nArg,ph7_value **apArg,int iArg)
{
	ph7_class_instance *pObj;
	if( iArg >= nArg || (apArg[iArg]->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pObj = (ph7_class_instance *)apArg[iArg]->x.pOther;
	if( !DtIsInit(pObj) ){
		DtThrowUninit(pCtx,pObj);
		return 0;
	}
	return pObj;
}
/* Answer the receiver itself, the way every mutating alias does. */
static void DtResultArg(ph7_context *pCtx,ph7_value **apArg)
{
	ph7_result_value(pCtx,apArg[0]);
}
/* date_create()/date_create_immutable(): php answers false on a parse failure and
 * says nothing -- the constructor's exception does not escape the alias. */
static int DtProcCreate(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zClass)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = DtClass(pVm,zClass);
	ph7_class_instance *pObj;
	const char *zIn = "now",*zZone;
	int nIn = 3,nZone,iPos,iZoneKind;
	sxi32 iZoneOff = 0;
	dt_state sState;
	char zNameBuf[16],cAt;
	const char *zErr;
	if( pClass == 0 ){
		return PH7_OK;
	}
	zZone = pVm->zDefTz;
	nZone = (int)pVm->nDefTz;
	iZoneKind = DT_ZONE_DEFAULT_KIND;
	if( nArg > 0 ){
		zIn = ph7_value_to_string(apArg[0],&nIn);
	}
	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){
		if( DtZoneArgInit(pCtx,apArg[1]) != 0 ){
			return PH7_OK;
		}
		DtZoneOf(apArg[1],&iZoneOff,&zZone,&nZone,&iZoneKind);
	}
	if( DtInitState(pCtx,zIn,nIn,iZoneOff,zZone,nZone,iZoneKind,&sState,zNameBuf,
		sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pObj = DtNewInstance(pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	DtStore(pVm,pObj,&sState);
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
static int vm_builtin_date_create(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtProcCreate(pCtx,nArg,apArg,"DateTime");
}
static int vm_builtin_date_create_immutable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtProcCreate(pCtx,nArg,apArg,"DateTimeImmutable");
}
static int vm_builtin_date_create_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTime");
}
static int vm_builtin_date_create_immutable_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTimeImmutable");
}
static int vm_builtin_date_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	const char *zFmt;
	int nFmt;
	if( pObj == 0 || nArg < 2 ){
		return PH7_OK;
	}
	zFmt = ph7_value_to_string(apArg[1],&nFmt);
	DtFormatOf(pCtx,pObj,zFmt,nFmt);
	return PH7_OK;
}
/* date_modify(): php WARNS and answers false where DateTime::modify() throws. */
static int vm_builtin_date_modify(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	const char *zMod,*zErr;
	int nMod,iPos,iErrPos;
	char cAt;
	sxi64 iTs = 0;
	sxi32 iOff = 0;
	int bOffSet = 0,uSec = 0;
	dt_parsed sVec;
	if( pObj == 0 || nArg < 2 ){
		return PH7_OK;
	}
	zMod = ph7_value_to_string(apArg[1],&nMod);
	iErrPos = DtParseEx(zMod,nMod,PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),
		(int)PH7_NativeAttrInt(pObj,DT_US),DT_PARSE_OVERRIDE_TIME|DT_PARSE_KEEP_ZONE,
		&iTs,&iOff,&bOffSet,&uSec,&sVec,&pCtx->pVm->sDtLastErr);
	if( iErrPos != 0 ){
		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);
		PH7_VmThrowWarningFmt(pCtx->pVm,
			"date_modify(): Failed to parse time string (%.*s) at position %d (%c): %s",
			DtCStrLen(zMod,nMod),zMod,iPos,cAt,zErr);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	DtStoreModified(pCtx->pVm,pObj,iTs,(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),&sVec);
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,uSec);   /* see DateTime::modify() */
	DtEpochRezone(pCtx->pVm,pObj,&sVec);
	DtResultArg(pCtx,apArg);
	return PH7_OK;
}
static int DtProcAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	ph7_class_instance *pIv = DtArgObj(pCtx,nArg,apArg,1);
	if( pObj == 0 || pIv == 0 ){
		return PH7_OK;
	}
	if( PH7_NativeAttrInt(pIv,"invert") ){
		iSign = -iSign;
	}
	DtApplyInterval(pCtx->pVm,pObj,pObj,pIv,iSign);
	DtResultArg(pCtx,apArg);
	return PH7_OK;
}
static int vm_builtin_date_add(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtProcAddSub(pCtx,nArg,apArg,1);
}
static int vm_builtin_date_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtProcAddSub(pCtx,nArg,apArg,-1);
}
static int vm_builtin_date_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pBase = DtArgObj(pCtx,nArg,apArg,0);
	ph7_class_instance *pTarget = DtArgObj(pCtx,nArg,apArg,1);
	int bAbsolute = 0;
	if( pBase == 0 || pTarget == 0 ){
		return PH7_OK;
	}
	if( nArg > 2 ){
		bAbsolute = DtValueTruth(pCtx->pVm,apArg[2]);
	}
	return DtDiffResult(pCtx,pBase,pTarget,bAbsolute);
}
static int vm_builtin_date_timestamp_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	if( pObj ){
		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_TS));
	}
	return PH7_OK;
}
static int vm_builtin_date_timestamp_set(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	if( pObj == 0 || nArg < 2 ){
		return PH7_OK;
	}
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,ph7_value_to_int64(apArg[1]));
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,0);
	DtRezone(pCtx->pVm,pObj);   /* see DateTime::setTimestamp() */
	DtResultArg(pCtx,apArg);
	return PH7_OK;
}
static int vm_builtin_date_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	if( pObj == 0 ){
		return PH7_OK;
	}
	return DtTimezoneResult(pCtx,pObj);
}
static int vm_builtin_date_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	const char *zName = "UTC";
	int nName = 3,iKind = DT_ZONE_ID;
	sxi32 iOff = 0;
	if( pObj == 0 || nArg < 2 || !DtZoneOf(apArg[1],&iOff,&zName,&nName,&iKind) ){
		return PH7_OK;
	}
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_OFF,iOff);
	PH7_NativeSetAttrStr(pCtx->pVm,pObj,DT_NAME,zName,nName);
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_ZKIND,iKind);
	DtResultArg(pCtx,apArg);
	return PH7_OK;
}
static int vm_builtin_date_offset_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	if( pObj ){
		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_OFF));
	}
	return PH7_OK;
}
static int vm_builtin_date_date_set(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	if( pObj == 0 || nArg < 4 ){
		return PH7_OK;
	}
	DtSetDateOf(pCtx,pObj,ph7_value_to_int64(apArg[1]),ph7_value_to_int(apArg[2]),
		ph7_value_to_int(apArg[3]));
	DtResultArg(pCtx,apArg);
	return PH7_OK;
}
static int vm_builtin_date_time_set(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	if( pObj == 0 || nArg < 3 ){
		return PH7_OK;
	}
	DtSetTimeOf(pCtx,pObj,ph7_value_to_int(apArg[1]),ph7_value_to_int(apArg[2]),
		nArg > 3 ? ph7_value_to_int(apArg[3]) : 0,
		nArg > 4 ? ph7_value_to_int64(apArg[4]) : 0);
	DtResultArg(pCtx,apArg);
	return PH7_OK;
}
static int vm_builtin_date_isodate_set(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	if( pObj == 0 || nArg < 3 ){
		return PH7_OK;
	}
	{
		sxi32 iOff = (sxi32)PH7_NativeAttrInt(pObj,DT_OFF);
		DtStoreLocalOf(pCtx->pVm,pObj,
			(sxi64)((sxu64)DtIsoDate(PH7_NativeAttrInt(pObj,DT_TS),iOff,
				ph7_value_to_int64(apArg[1]),ph7_value_to_int64(apArg[2]),
				nArg > 3 ? ph7_value_to_int64(apArg[3]) : 1) + (sxu64)iOff));
	}
	DtResultArg(pCtx,apArg);
	return PH7_OK;
}
/*
 * The COMPONENT view php's two parse readers answer with: what the scanner READ,
 * field by field, rather than what a constructor would make of it. Nothing about
 * the clock reaches it -- a field the string never mentioned is `false`, not the
 * base moment's -- and three parts of the shape are conditional.
 *
 * `is_localtime` is whether a TIMEZONE token was seen at all, which is not the
 * same as one having been understood: an unknown name sets it and leaves
 * `zone_type` 0, with nothing else shown. A fixed OFFSET shows `zone` and
 * `is_dst`, an ABBREVIATION shows those and its `tz_abbr`, and an IDENTIFIER
 * shows only its name, twice. And the `relative` block appears when the parse
 * spelled a relative element -- `now` and `today` do not -- carrying the weekday
 * when one was hunted, the business-day count when that special was named, and
 * the `first|last day of` flag as `true`.
 */
typedef struct dt_comp dt_comp;
struct dt_comp
{
	sxi64 y,mo,d,h,mi,s,us;   /* DT_UNSET is php's own, and `false` on show */
	int iZoneSeen;            /* php's is_localtime */
	int iZoneKind;            /* php's timezone_type, 0 when nothing resolved */
	sxi32 iOff;
	const char *zName;
	int nName;
	int bHaveRel;
	sxi64 ry,rm,rd,rh,ri,rs;
	int bWday,iWday;
	int bWeekdays;
	sxi64 iWeekdays;
	int iFirstLast;
};
static int DtCompResult(ph7_context *pCtx,const dt_comp *pC,const phl_dt_lasterr *pRec)
{
	ph7_value *pArr,*pRel,*pWarn,*pErrs,*pVal;
	int k;
	pArr = ph7_context_new_array(pCtx);
	pWarn = ph7_context_new_array(pCtx);
	pErrs = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pWarn == 0 || pErrs == 0 || pVal == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
#define DT_PUT(zKey) ph7_array_add_strkey_elem(pArr,zKey,pVal)
#define DT_PUTFIELD(zKey,iVal) do{ \
		if( (iVal) == DT_UNSET ){ ph7_value_bool(pVal,0); } \
		else { ph7_value_int64(pVal,(iVal)); } \
		DT_PUT(zKey); \
	}while(0)
	DT_PUTFIELD("year",pC->y);
	DT_PUTFIELD("month",pC->mo);
	DT_PUTFIELD("day",pC->d);
	DT_PUTFIELD("hour",pC->h);
	DT_PUTFIELD("minute",pC->mi);
	DT_PUTFIELD("second",pC->s);
	if( pC->us == DT_UNSET ){
		ph7_value_bool(pVal,0);
	}else{
		ph7_value_double(pVal,(ph7_real)((double)pC->us / 1000000.0));
	}
	DT_PUT("fraction");
	for( k = 0 ; k < pRec->nWarnKept ; k++ ){
		ph7_value_string(pVal,pRec->azWarn[k],-1);
		ph7_array_add_intkey_elem(pWarn,pRec->aWarnPos[k],pVal);
		ph7_value_reset_string_cursor(pVal);
	}
	{
		const phl_dt_diag_row *aRow = (const phl_dt_diag_row *)SyBlobData(&pRec->sErr);
		for( k = 0 ; k < pRec->nErrKept ; k++ ){
			ph7_value_string(pVal,aRow[k].zMsg,-1);
			ph7_array_add_intkey_elem(pErrs,aRow[k].iPos,pVal);
			ph7_value_reset_string_cursor(pVal);
		}
	}
	ph7_value_int(pVal,pRec->nWarn);
	DT_PUT("warning_count");
	ph7_array_add_strkey_elem(pArr,"warnings",pWarn);
	ph7_value_int(pVal,pRec->nErr);
	DT_PUT("error_count");
	ph7_array_add_strkey_elem(pArr,"errors",pErrs);
	ph7_value_bool(pVal,pC->iZoneSeen != 0);
	DT_PUT("is_localtime");
	if( pC->iZoneSeen ){
		ph7_value_int(pVal,pC->iZoneKind);
		DT_PUT("zone_type");
		if( pC->iZoneKind == DT_ZONE_OFFSET || pC->iZoneKind == DT_ZONE_ABBR ){
			ph7_value_int64(pVal,(sxi64)pC->iOff);
			DT_PUT("zone");
			ph7_value_bool(pVal,0);   /* no tz database, so nothing is ever DST */
			DT_PUT("is_dst");
		}
		if( pC->iZoneKind == DT_ZONE_ABBR || pC->iZoneKind == DT_ZONE_ID ){
			ph7_value_string(pVal,pC->zName,pC->nName);
			DT_PUT("tz_abbr");
			ph7_value_reset_string_cursor(pVal);
		}
		if( pC->iZoneKind == DT_ZONE_ID ){
			ph7_value_string(pVal,pC->zName,pC->nName);
			DT_PUT("tz_id");
			ph7_value_reset_string_cursor(pVal);
		}
	}
	if( pC->bHaveRel ){
		pRel = ph7_context_new_array(pCtx);
		if( pRel == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
#define DT_PUTREL(zKey,iVal) do{ \
			ph7_value_int64(pVal,(iVal)); \
			ph7_array_add_strkey_elem(pRel,zKey,pVal); \
		}while(0)
		DT_PUTREL("year",pC->ry);
		DT_PUTREL("month",pC->rm);
		DT_PUTREL("day",pC->rd);
		DT_PUTREL("hour",pC->rh);
		DT_PUTREL("minute",pC->ri);
		DT_PUTREL("second",pC->rs);
		if( pC->bWday ){
			DT_PUTREL("weekday",(sxi64)pC->iWday);
		}
		if( pC->bWeekdays ){
			DT_PUTREL("weekdays",pC->iWeekdays);
		}
		if( pC->iFirstLast ){
			ph7_value_bool(pVal,1);
			ph7_array_add_strkey_elem(pRel,
				pC->iFirstLast == 1 ? "first_day_of_month" : "last_day_of_month",pVal);
		}
#undef DT_PUTREL
		ph7_array_add_strkey_elem(pArr,"relative",pRel);
	}
#undef DT_PUTFIELD
#undef DT_PUT
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/*
 * date_parse(): the same scanner every constructor runs, showing what it read.
 * The diagnostics are the scan's own (see DtParseFields) and are NOT published
 * as getLastErrors() -- php leaves that record to the constructors -- so the
 * scan writes into one of this call's own.
 */
static int vm_builtin_date_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_dt_lasterr sRec;
	dt_parsed sVec;
	dt_comp sC;
	const char *zIn = "";
	sxi64 iTs = 0;
	sxi32 iOff = 0;
	int nIn = 0,bOffSet = 0,uSec = 0,rc;
	if( nArg < 1 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	SyZero(&sRec,sizeof(sRec));
	SyBlobInit(&sRec.sErr,&pVm->sAllocator);
	DtFieldsInit(&sVec,0);
	DtParseEx(zIn,nIn,0,0,0,0,&iTs,&iOff,&bOffSet,&uSec,&sVec,&sRec);
	SyZero(&sC,sizeof(sC));
	sC.y = sVec.y; sC.mo = sVec.m; sC.d = sVec.d;
	sC.h = sVec.h; sC.mi = sVec.i; sC.s = sVec.s;
	sC.us = sVec.bUsUnset ? DT_UNSET : sVec.us;
	sC.iZoneSeen = sVec.nZoneTok > 0;
	sC.iZoneKind = sVec.bOffSet == 0 ? 0
		: (sVec.bOffSet == 1 ? DT_ZONE_OFFSET
		   : (sVec.bZoneIdent ? DT_ZONE_ID : DT_ZONE_ABBR));
	sC.iOff = sVec.iOff;
	sC.zName = sVec.zZone;
	sC.nName = sVec.nZone;
	sC.bHaveRel = sVec.bHaveRel;
	sC.ry = sVec.ry; sC.rm = sVec.rm; sC.rd = sVec.rd;
	sC.rh = sVec.rh; sC.ri = sVec.ri; sC.rs = sVec.rs;
	sC.bWday = sVec.bWday; sC.iWday = sVec.iWday;
	sC.bWeekdays = sVec.bWeekdays; sC.iWeekdays = sVec.iWeekdays;
	sC.iFirstLast = sVec.iFirstLast;
	rc = DtCompResult(pCtx,&sC,&sRec);
	SyBlobRelease(&sRec.sErr);
	return rc;
}
/*
 * date_parse_from_format(): the FORMAT scanner's components, through the very
 * presenter date_parse() answers with -- php reads both of them out of one
 * function, so a field the scan left unset is `false` in either, and the
 * `relative` block appears here for exactly one reason: a textual DAY, which
 * this parser records as a weekday to move to rather than as a day.
 *
 * Like date_parse(), it publishes NOTHING into getLastErrors() -- php leaves
 * that record to the constructors -- so the scan's diagnostics go into a record
 * of this call's own.
 */
static int vm_builtin_date_parse_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_dt_lasterr sRec;
	dt_ff_res sRes;
	dt_comp sC;
	const char *zFmt,*zIn;
	int nFmt,nIn,rc;
	if( nArg < 2 ){
		return PH7_OK;
	}
	zFmt = ph7_value_to_string(apArg[0],&nFmt);
	zIn  = ph7_value_to_string(apArg[1],&nIn);
	DtFromFormat(zFmt,nFmt,zIn,nIn,&sRes);
	SyZero(&sRec,sizeof(sRec));
	SyBlobInit(&sRec.sErr,&pVm->sAllocator);
	DtFfDiagInto(&sRec,&sRes.sDiag);
	SyZero(&sC,sizeof(sC));
	sC.y = sRes.y; sC.mo = sRes.mo; sC.d = sRes.d;
	sC.h = sRes.h; sC.mi = sRes.mi; sC.s = sRes.s; sC.us = sRes.us;
	sC.iZoneSeen = sRes.bLocal;
	sC.iZoneKind = sRes.iOffKind;
	sC.iOff = sRes.iOff;
	sC.zName = sRes.zName;
	sC.nName = sRes.nName;
	sC.bHaveRel = sRes.bWday;
	sC.bWday = sRes.bWday;
	sC.iWday = (int)sRes.iWday;
	rc = DtCompResult(pCtx,&sC,&sRec);
	SyBlobRelease(&sRec.sErr);
	return rc;
}
/* date_interval_create_from_date_string(): warns and answers false where the
 * method throws. */
static int vm_builtin_date_interval_create_from_date_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zReason = "";
	int nIn,iPos = 0,bNonRel = 0;
	char cAt = ' ';
	ph7_class_instance *pObj;
	if( nArg < 1 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason,&bNonRel);
	if( pObj == 0 ){
		if( bNonRel ){
			PH7_VmThrowWarningFmt(pCtx->pVm,
				"date_interval_create_from_date_string(): String '%.*s' contains "
				"non-relative elements",DtCStrLen(zIn,nIn),zIn);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		PH7_VmThrowWarningFmt(pCtx->pVm,
			"date_interval_create_from_date_string(): Unknown or bad format (%.*s) "
			"at position %d (%c): %s",DtCStrLen(zIn,nIn),zIn,iPos,cAt,zReason);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
static int vm_builtin_date_interval_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	const char *zFmt;
	int nFmt;
	if( pObj == 0 || nArg < 2 ){
		return PH7_OK;
	}
	zFmt = ph7_value_to_string(apArg[1],&nFmt);
	DtIvFormat(pCtx,pObj,zFmt,nFmt);
	return PH7_OK;
}
static int vm_builtin_date_get_last_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return vm_builtin_DateTime_getLastErrors(pCtx,nArg,apArg);
}
/* timezone_open(): warns and answers false where the constructor throws. */
static int vm_builtin_timezone_open(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = DtClass(pVm,"DateTimeZone");
	ph7_class_instance *pObj;
	const char *zTz,*zName;
	int nTz,nName,iKind = DT_ZONE_ID,rc;
	sxi32 iOff = 0;
	char zBuf[16];
	if( pClass == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zTz = ph7_value_to_string(apArg[0],&nTz);
	rc = DtZoneParse(zTz,nTz,&iOff,&zName,&nName,&iKind,zBuf,sizeof(zBuf));
	if( rc != 0 ){
		PH7_VmThrowWarningFmt(pVm,
			rc == -2 ? "timezone_open(): Timezone offset is out of range (%.*s)"
			         : "timezone_open(): Unknown or bad timezone (%.*s)",nTz,zTz);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pObj = DtNewInstance(pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeSetAttrInt(pVm,pObj,DTZ_OFF,iOff);
	PH7_NativeSetAttrStr(pVm,pObj,DTZ_NAME,zName,nName);
	PH7_NativeSetAttrInt(pVm,pObj,DTZ_KIND,iKind);
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
static int vm_builtin_timezone_name_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	const char *zName;
	int nName;
	if( pObj == 0 ){
		return PH7_OK;
	}
	PH7_NativeAttrStr(pObj,DTZ_NAME,&zName,&nName);
	ph7_result_string(pCtx,zName,nName);
	return PH7_OK;
}
static int vm_builtin_timezone_offset_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	if( nArg > 1 && DtArgInit(pCtx,apArg[1]) != 0 ){
		return PH7_OK;
	}
	if( pObj ){
		DtZoneOffsetResult(pCtx,pObj,
			nArg > 1 && (apArg[1]->iFlags & MEMOBJ_OBJ) ?
				(ph7_class_instance *)apArg[1]->x.pOther : 0);
	}
	return PH7_OK;
}
static int vm_builtin_timezone_transitions_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(pCtx,nArg,apArg,0);
	if( pObj == 0 ){
		return PH7_OK;
	}
	/* The zone shifts every argument along by one. */
	return DtZoneTransitionsResult(pCtx,pObj,
		nArg > 1 ? ph7_value_to_int64(apArg[1]) : (-(sxi64)0x7FFFFFFFFFFFFFFF - 1),
		nArg > 2 ? ph7_value_to_int64(apArg[2]) : (sxi64)0x7FFFFFFF);
}
/*
 * The four private slots a date object keeps its state in. Both classes declare
 * them: `trait __DtCoreT` had no native equivalent, and replaying the table is
 * exactly what `use __DtCoreT` did.
 */
#define DT_NATIVE_STATE_PROPS \
	{ DT_TS,   PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \
	{ DT_OFF,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \
	{ DT_NAME, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 }, 0 }, \
	{ DT_US,   PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \
	{ DT_ZKIND,PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, DT_ZONE_ID, 0, 0.0 }, 0 }, \
	{ DT_INIT, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }
/*
 * The methods DateTime and DateTimeImmutable share -- the whole of the old trait
 * plus the mutators, whose one difference (write $this, or write a clone) the
 * bodies decide from the receiver's class. php's own signatures: they are the
 * single source of truth for arity, coercion and Reflection here, so the casts the
 * chunk wrote by hand (`(string)$format`, `(int)$timestamp`) are declared types now
 * and the methods reject what php rejects.
 */
/* The methods DateTime and DateTimeImmutable share. CLS is the OWNING class name
 * as a string literal, because php's stubs write the concrete class rather than
 * `static` for the legacy mutators -- DateTime::add reports DateTime and
 * DateTimeImmutable::add reports DateTimeImmutable. The two that php really does
 * declare `static` (setMicrosecond) and the two it declares for real rather than
 * tentatively (getMicrosecond, and setMicrosecond again) are written as they are:
 * a leading `@` is php's @tentative-return-type, and nearly every method here has
 * one. */
#define DT_NATIVE_SHARED_METHODS(CLS) \
	{ "__construct",     PH7_MOD_PUBLIC, "string $datetime = 'now', ?DateTimeZone $timezone = null", "", \
	  vm_builtin_DateTime_construct }, \
	{ "format",          PH7_MOD_PUBLIC, "string $format", "@string", vm_builtin_DateTime_format }, \
	{ "getTimestamp",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_DateTime_getTimestamp }, \
	{ "getMicrosecond",  PH7_MOD_PUBLIC, "", "int", vm_builtin_DateTime_getMicrosecond }, \
	{ "getOffset",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_DateTime_getOffset }, \
	{ "getTimezone",     PH7_MOD_PUBLIC, "", "@DateTimeZone|false", vm_builtin_DateTime_getTimezone }, \
	{ "diff",            PH7_MOD_PUBLIC, "DateTimeInterface $targetObject, bool $absolute = false", \
	  "@DateInterval", vm_builtin_DateTime_diff }, \
	{ "modify",          PH7_MOD_PUBLIC, "string $modifier", "@" CLS, vm_builtin_DateTime_modify }, \
	{ "setTimestamp",    PH7_MOD_PUBLIC, "int $timestamp", "@" CLS, vm_builtin_DateTime_setTimestamp }, \
	{ "setMicrosecond",  PH7_MOD_PUBLIC, "int $microsecond", "static", vm_builtin_DateTime_setMicrosecond }, \
	{ "setTimezone",     PH7_MOD_PUBLIC, "DateTimeZone $timezone", "@" CLS, vm_builtin_DateTime_setTimezone }, \
	{ "setDate",         PH7_MOD_PUBLIC, "int $year, int $month, int $day", "@" CLS, \
	  vm_builtin_DateTime_setDate }, \
	{ "setTime",         PH7_MOD_PUBLIC, \
	  "int $hour, int $minute, int $second = 0, int $microsecond = 0", "@" CLS, \
	  vm_builtin_DateTime_setTime }, \
	{ "setISODate",      PH7_MOD_PUBLIC, "int $year, int $week, int $dayOfWeek = 1", "@" CLS, \
	  vm_builtin_DateTime_setISODate }, \
	{ "add",             PH7_MOD_PUBLIC, "DateInterval $interval", "@" CLS, vm_builtin_DateTime_add }, \
	{ "sub",             PH7_MOD_PUBLIC, "DateInterval $interval", "@" CLS, vm_builtin_DateTime_sub }, \
	{ "getLastErrors",   PH7_MOD_PUBLIC|PH7_MOD_STATIC, "", "@array|false", \
	  vm_builtin_DateTime_getLastErrors }
/*
 * php's add_common_properties(): after the presented shape, the instance's own
 * php-visible slots -- a SUBCLASS's declared properties, which php serializes
 * alongside the internal state. A key the presented shape already wrote WINS
 * (zend_hash_add, not update), and a hidden engine slot is never a candidate:
 * this is the one walk in the date family that must skip them.
 */
static void DtAddCommonProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)
{
	SXUNUSED(pVm);
	if( (pOut->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return;
	}
	/* MANGLED, the way php mangles a non-public property everywhere it hands an
	 * object's own table out: a subclass's `protected $p` serializes as
	 * "\0*\0p" and casts to that key, and only the mangling keeps two same-named
	 * members from different visibility levels apart. */
	PH7_ClassInstanceOwnPropsToHashmap(pThis,(ph7_hashmap *)pOut->x.pOther);
}
/*
 * The INTERNAL shape of a class whose state is its own public properties -- the
 * ones the named class declares, in declared order, read off the instance.
 *
 * Not a walk of the instance's TABLE: a subclass that redeclares one of the names
 * takes over its slot and its POSITION, and php still shows the value where its
 * own shape puts it.
 */
static void DtAddNativeProps(ph7_vm *pVm,ph7_class_instance *pThis,const char *zBase,ph7_value *pOut)
{
	ph7_class *pClass = DtClass(&(*pVm),zBase);
	SyHashEntry *pEntry;
	if( pClass == 0 ){
		return;
	}
	SyHashResetLoopCursor(&pClass->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		ph7_value *pVal;
		ph7_value sKey;
		SyHashEntry *pOwn;
		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_HIDDEN) ){
			continue;
		}
		/* ...and a slot THIS object hides is not part of its shape either: a
		 * from-string interval serializes as the two names php writes. */
		pOwn = SyHashGet(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName));
		if( pOwn && (((VmClassAttr *)pOwn->pUserData)->iState & VM_CLASS_ATTR_UNSEEN) ){
			continue;
		}
		pVal = PH7_ClassInstanceFetchAttr(pThis,&pAttr->sName);
		if( pVal == 0 ){
			continue;
		}
		PH7_MemObjInitFromString(&(*pVm),&sKey,0);
		PH7_MemObjStringAppend(&sKey,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName));
		ph7_array_add_elem(pOut,&sKey,pVal);
		PH7_MemObjRelease(&sKey);
	}
}
/*
 * php's presentation for the date classes (ph7_class::xPresent).
 *
 * php keeps a timelib struct and SHOWS date/timezone_type/timezone; PHL keeps a
 * timestamp, an offset, a zone name and microseconds, all hidden. These build php's
 * shape out of that state, so var_dump/print_r, var_export and the (array) cast
 * agree with the oracle without changing what the C bodies read.
 *
 * timezone_type is php's own three-way tag: 1 = a fixed UTC OFFSET ("+02:00"),
 * 2 = an ABBREVIATION ("GMT", "Z"), 3 = an IDENTIFIER ("UTC", "Europe/Paris").
 * PHL accepts offsets, UTC, GMT and Z today; the identifier arm is written for the
 * whole rule so a tz database can only add names, never change the tagging.
 */
static void DtPresentPut(ph7_vm *pVm,ph7_value *pOut,const char *zKey,ph7_value *pVal)
{
	ph7_value sKey;
	PH7_MemObjInitFromString(&(*pVm),&sKey,0);
	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));
	ph7_array_add_elem(pOut,&sKey,pVal);
	PH7_MemObjRelease(&sKey);
}
static void DtPresentZone(ph7_vm *pVm,ph7_value *pOut,const char *zName,int nName,int iKind)
{
	ph7_value sVal;
	PH7_MemObjInitFromInt(&(*pVm),&sVal,iKind);
	DtPresentPut(&(*pVm),pOut,"timezone_type",&sVal);
	PH7_MemObjRelease(&sVal);
	PH7_MemObjInitFromString(&(*pVm),&sVal,0);
	PH7_MemObjStringAppend(&sVal,zName,(sxu32)nName);
	DtPresentPut(&(*pVm),pOut,"timezone",&sVal);
	PH7_MemObjRelease(&sVal);
}
/*
 * php builds this shape FROM the struct the constructor allocates, so an object
 * that has none contributes nothing to it -- var_dump, print_r, var_export, the
 * (array) cast and json_encode all answer an empty shape where PHL published a
 * 1970 date nothing had asked for.
 */
static void DtDateTimeShape(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)
{
	dt_state sState;
	Sytm sTm;
	char zZone[64];
	char zDate[64];
	ph7_value sVal;
	int nName;
	if( !DtIsInit(pThis) ){
		return;
	}
	DtLoad(pThis,&sState);
	nName = sState.nName;
	if( nName >= (int)sizeof(zZone) ){
		nName = (int)sizeof(zZone) - 1;
	}
	if( nName > 0 ){
		SyMemcpy(sState.zName,zZone,(sxu32)nName);
	}
	zZone[nName] = 0;
	DtFillSytm(sState.iTs,sState.iOff,zZone,&sTm);
	/* php's fixed shape here, not a format string: "Y-m-d H:i:s.uuuuuu". */
	SyBufferFormat(zDate,sizeof(zDate),"%04qd-%02d-%02d %02d:%02d:%02d.%06d",
		sTm.tm_year,sTm.tm_mon + 1,sTm.tm_mday,sTm.tm_hour,sTm.tm_min,sTm.tm_sec,
		sState.uSec);
	PH7_MemObjInitFromString(&(*pVm),&sVal,0);
	PH7_MemObjStringAppend(&sVal,zDate,(sxu32)SyStrlen(zDate));
	DtPresentPut(&(*pVm),pOut,"date",&sVal);
	PH7_MemObjRelease(&sVal);
	DtPresentZone(&(*pVm),pOut,zZone,nName,sState.iZoneKind);
}
/*
 * The hook itself: php's get_properties starts from the object's OWN table and
 * writes the struct's keys into it, so a subclass's properties come FIRST and a
 * subclass property named `date` keeps its position while taking the internal
 * value. PHL built the three keys alone, so every property a subclass declared was
 * missing from var_dump, var_export, the (array) cast and json_encode.
 */
static sxi32 DtPresentDateTime(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	SXUNUSED(bDebug); /* php shows the same three keys to both handlers */
	DtAddCommonProps(&(*pVm),pThis,pOut);
	DtDateTimeShape(&(*pVm),pThis,pOut);
	return SXRET_OK;
}
/*
 * DateInterval and DatePeriod present their OWN property table -- php's state for
 * these two is the visible properties themselves -- so the hook exists for one
 * reason: an object that was never constructed has no table at all there, and
 * showed ten (or seven) default fields here. The initialized case is the slot walk
 * the cast already fell back to, so nothing else about them changes.
 */
static sxi32 DtPresentProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	SXUNUSED(bDebug);
	if( DtIsInit(pThis) ){
		PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pOut->x.pOther);
	}else{
		/* No table there at all -- but a subclass's own properties are the
		 * object's, and php still shows those. */
		DtAddCommonProps(&(*pVm),pThis,pOut);
	}
	return SXRET_OK;
}
static void DtTimeZoneShape(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)
{
	const char *zName = 0;
	int nName = 0;
	if( !DtIsInit(pThis) ){
		return;
	}
	PH7_NativeAttrStr(pThis,DTZ_NAME,&zName,&nName);
	DtPresentZone(&(*pVm),pOut,zName ? zName : "",nName,
		DtZoneKindOf(pThis,DTZ_KIND,zName,nName));
}
static sxi32 DtPresentTimeZone(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	SXUNUSED(bDebug);
	DtAddCommonProps(&(*pVm),pThis,pOut);   /* see DtPresentDateTime */
	DtTimeZoneShape(&(*pVm),pThis,pOut);
	return SXRET_OK;
}
/*
 * ---------------------------------------------------------------------------
 * php's serialization pair for the three date classes whose state is HIDDEN.
 *
 * serialize() had been walking the engine slots, so a DateTime round-tripped as
 * `__dtTs`/`__dtOff`/`__dtName`/`__dtUs` and a payload php WROTE could not be read
 * back at all -- `unserialize('O:8:"DateTime":3:{s:4:"date";…}')` found none of the
 * names it wanted, silently kept the 1970 defaults and answered a valid object with
 * the wrong instant. php's answer is not a hidden-slot rule but a pair of methods:
 * __serialize() hands back the PRESENTED shape (date/timezone_type/timezone, the
 * same hash date_object_get_properties_for builds) and __unserialize() re-parses it,
 * so the payload is the class's public model rather than its storage.
 *
 * The four methods php declares are all here, because they are one contract:
 * __serialize/__unserialize is what serialize() uses, __wakeup reads a LEGACY
 * payload out of the object's own properties, and __set_state is what var_export's
 * `\DateTime::__set_state(array(…))` text evaluates to. All four fail with the same
 * plain `Error`, and php's sentence for it names the class.
 * ---------------------------------------------------------------------------
 */
/*
 * ---------------------------------------------------------------------------
 * How the date classes COMPARE (ph7_class::xCmp -- php's compare handlers).
 *
 * php compares a date object by what it MEANS, not by what it stores, and the
 * property walk PHL fell back to disagreed with every one of them: a DateTime
 * and a DateTimeImmutable of the same instant were unequal here because the
 * classes differ, two dates one second apart in different zones were unequal
 * because the zone NAME is a property, two `P1D` intervals were equal where php
 * refuses to compare intervals at all, and two DateTimeZones ordered by name
 * where php refuses to compare different KINDS of zone.
 *
 * All three handlers screen their partner by INSTANCE OF, not by class
 * identity: a subclass of DateTime still compares as an instant (and its extra
 * properties are invisible to the comparison, since php's date handler never
 * looks at properties), while anything that is not a date at all is php's
 * ZEND_UNCOMPARABLE -- the 1-from-both-sides the caller defaults to.
 * ---------------------------------------------------------------------------
 */
/*
 * DateTime / DateTimeImmutable: php's date_object_compare_date, which is
 * timelib_time_compare on the two INSTANTS -- the epoch second first, the
 * microseconds to break a tie. The zone is not part of it (php compares the
 * instant the two name, so 00:00 UTC equals 01:00+01:00), and neither is any
 * property, declared or dynamic.
 */
static void DtCmpDateTime(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)
{
	dt_state sL,sR;
	if( !DtIsA(&(*pVm),pThis,"DateTimeInterface")
	 || !DtIsA(&(*pVm),pCtx->pOther,"DateTimeInterface") ){
		return;   /* uncomparable, which is what the caller pre-loaded */
	}
	if( !DtIsInit(pThis) || !DtIsInit(pCtx->pOther) ){
		/* An unconstructed date has no instant to compare, and php refuses from
		 * EITHER side -- so `$fresh == $uninitialized` raises as well. The screen
		 * sits below the both-are-dates one on purpose: a date against something
		 * that is not one stays php's silent uncomparable. */
		pCtx->zThrowClass = "DateObjectError";
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
			"Trying to compare an incomplete DateTime or DateTimeImmutable object");
		return;
	}
	DtLoad(pThis,&sL);
	DtLoad(pCtx->pOther,&sR);
	if( sL.iTs != sR.iTs ){
		pCtx->iResult = sL.iTs < sR.iTs ? -1 : 1;
	}else if( sL.uSec != sR.uSec ){
		pCtx->iResult = sL.uSec < sR.uSec ? -1 : 1;
	}else{
		pCtx->iResult = 0;
	}
}
/*
 * DateInterval: php refuses. Two intervals carry no common unit -- a month is
 * not a fixed number of days -- so php's handler answers ZEND_UNCOMPARABLE
 * behind an E_WARNING for every pair, `P1D` against `P1D` included. The one
 * comparison that succeeds is `$i == $i`, and that never reaches a handler:
 * zend's identity shortcut answers it first (and PH7_ClassInstanceCmp's does
 * too, above this call).
 *
 * The warning is emitted from inside the comparator on purpose -- php emits it
 * from inside the handler, so a sort() over intervals warns once per COMPARISON
 * there as it does here.
 */
static void DtCmpInterval(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)
{
	if( !DtIsA(&(*pVm),pThis,"DateInterval")
	 || !DtIsA(&(*pVm),pCtx->pOther,"DateInterval") ){
		return;   /* an interval against something else: uncomparable, and silent */
	}
	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Cannot compare DateInterval objects");
	pCtx->iResult = 1;
}
/*
 * DateTimeZone: php compares two zones of the SAME kind and refuses two of
 * different kinds outright (a DateException raised from inside the comparison).
 * Same-kind zones answer 0 or php's uncomparable 1 and never an ordering, so
 * `+01:00 < +02:00` is false there: an OFFSET zone is compared by its offset
 * (`+0100` and `+01:00` are one zone), an abbreviation and an identifier by
 * their normalized names.
 */
static void DtCmpTimeZone(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)
{
	const char *zL = 0,*zR = 0;
	int nL = 0,nR = 0;
	int iKindL,iKindR;
	if( !DtIsA(&(*pVm),pThis,"DateTimeZone")
	 || !DtIsA(&(*pVm),pCtx->pOther,"DateTimeZone") ){
		return;
	}
	if( !DtIsInit(pThis) || !DtIsInit(pCtx->pOther) ){
		/* php's own sentence for the zone half, and its own class: a DateException
		 * carries the KIND mismatch below, an unconstructed operand a
		 * DateObjectError. */
		pCtx->zThrowClass = "DateObjectError";
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
			"Trying to compare uninitialized DateTimeZone objects");
		return;
	}
	PH7_NativeAttrStr(pThis,DTZ_NAME,&zL,&nL);
	PH7_NativeAttrStr(pCtx->pOther,DTZ_NAME,&zR,&nR);
	iKindL = DtZoneKindOf(pThis,DTZ_KIND,zL,nL);
	iKindR = DtZoneKindOf(pCtx->pOther,DTZ_KIND,zR,nR);
	if( iKindL != iKindR ){
		pCtx->zThrowClass = "DateException";
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
			"Cannot compare two different kinds of DateTimeZone objects");
		return;   /* the uncomparable 1 stands in until the refusal is raised */
	}
	if( iKindL == DT_ZONE_OFFSET ){
		pCtx->iResult = PH7_NativeAttrInt(pThis,DTZ_OFF)
		              == PH7_NativeAttrInt(pCtx->pOther,DTZ_OFF) ? 0 : 1;
		return;
	}
	pCtx->iResult = (nL == nR && (nL == 0 || SyMemcmp(zL,zR,(sxu32)nL) == 0)) ? 0 : 1;
}
/* Build a payload array: the class's presented shape, then its own visible slots. */
static int DtSerializePayload(ph7_context *pCtx,ph7_class_instance *pThis,int bZoneOnly,
	ph7_value *pOut)
{
	PH7_MemObjInit(pCtx->pVm,pOut);
	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){
		PH7_MemObjRelease(pOut);
		return -1;
	}
	/* The SHAPE first here, the object's own properties behind it: php's
	 * __serialize builds a fresh array from the struct and calls
	 * add_common_properties on the END of it, which is the opposite order from the
	 * presentation above (where the object's table is what the struct writes
	 * into). serialize() and var_dump therefore disagree about where a subclass's
	 * property sits, in both engines. */
	if( bZoneOnly ){
		DtTimeZoneShape(pCtx->pVm,pThis,pOut);
	}else{
		DtDateTimeShape(pCtx->pVm,pThis,pOut);
	}
	DtAddCommonProps(pCtx->pVm,pThis,pOut);
	return 0;
}
/* php's `Error: Invalid serialization data for <Class> object`, the one refusal all
 * four methods share. Named for the DECLARING class, not the receiver's. */
static int DtSerialError(ph7_context *pCtx,const char *zClass)
{
	return PH7_VmThrowException(pCtx,"Error",
		"Invalid serialization data for %s object",zClass);
}
/* The `array $data` parameter's own screen: the shared ZPP does not judge a scalar
 * against a bare `array` (a recorded gap), so each caller words php's TypeError. */
static int DtCheckDataArg(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zClass)
{
	char zBuf[64];
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){
		return 0;
	}
	PH7_VmThrowException(pCtx,"TypeError",
		"%s::__unserialize(): Argument #1 ($data) must be of type array, %s given",
		zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");
	return -1;
}
/*
 * php's php_date_timezone_initialize_from_hash(): `timezone_type` must be an int in
 * 1..3 and `timezone` a string, and then the NAME alone rebuilds the zone -- the tag
 * is validated but never trusted, which is why a payload tagged 1 whose name is
 * "UTC" restores a UTC zone rather than an offset one. Answers 0 on success.
 */
static int DtZoneRestore(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData)
{
	ph7_value *pType,*pName;
	const char *zTz,*zName;
	int nTz,nName,iKind = DT_ZONE_ID;
	sxi32 iOff = 0;
	sxi64 iType;
	char zBuf[16];
	pType = ph7_array_fetch(pData,"timezone_type",(int)sizeof("timezone_type")-1);
	if( pType == 0 || (pType->iFlags & MEMOBJ_INT) == 0 ){
		return -1;
	}
	iType = pType->x.iVal;
	if( iType < 1 || iType > 3 ){
		return -1;
	}
	pName = ph7_array_fetch(pData,"timezone",(int)sizeof("timezone")-1);
	if( pName == 0 || (pName->iFlags & MEMOBJ_STRING) == 0 ){
		return -1;
	}
	zTz = (const char *)SyBlobData(&pName->sBlob);
	nTz = (int)SyBlobLength(&pName->sBlob);
	if( DtZoneParse(zTz,nTz,&iOff,&zName,&nName,&iKind,zBuf,sizeof(zBuf)) != 0 ){
		return -1;
	}
	PH7_NativeSetAttrInt(&(*pVm),pThis,DTZ_OFF,iOff);
	PH7_NativeSetAttrStr(&(*pVm),pThis,DTZ_NAME,zName,nName);
	/* The payload's own `timezone_type` is NOT read back: php re-derives the kind
	 * from the name here too, which is what turns a serialized type-2 "UTC" into a
	 * type 3 on the way in. */
	PH7_NativeSetAttrInt(&(*pVm),pThis,DTZ_KIND,iKind);
	return 0;
}
/*
 * php's php_date_initialize_from_hash(): `date`, `timezone_type` and `timezone` must
 * all be present and well-typed, and the tag must be one php writes.
 *
 * php restores an OFFSET or ABBREVIATION payload by CONCATENATING the two and running
 * its ordinary parser over "<date> <timezone>", and an IDENTIFIER one by resolving the
 * name first. Resolving the name for all three is the same answer here and does not
 * lean on the parser: `date` is always php's own `x-m-d H:i:s.u`, which carries no
 * zone of its own, so nothing is left for the concatenated text to decide. It is also
 * the only spelling that works today -- PHL's parser accepts an offset only when it is
 * ATTACHED to the time ("…07+02:30", never "…07 +02:30") and accepts no trailing zone
 * NAME at all, so php's own round-trip string does not parse here (a scope-policy gap of its
 * own, recorded rather than worked around).
 *
 * Reading the NAME rather than the tag is also what php ends up doing: a payload
 * tagged 1 whose timezone is "UTC" restores a UTC zone in both engines.
 * Answers 0 on success.
 */
static int DtDateRestore(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pData)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pDate,*pType,*pName;
	const char *zDate,*zTz,*zZone,*zErr;
	int nDate,nTz,nZone,iPos,iKind = DT_ZONE_ID;
	sxi32 iOff = 0;
	sxi64 iType;
	dt_state sState;
	char zNameBuf[16],zZoneBuf[16],cAt;
	pDate = ph7_array_fetch(pData,"date",(int)sizeof("date")-1);
	if( pDate == 0 || (pDate->iFlags & MEMOBJ_STRING) == 0 ){
		return -1;
	}
	pType = ph7_array_fetch(pData,"timezone_type",(int)sizeof("timezone_type")-1);
	if( pType == 0 || (pType->iFlags & MEMOBJ_INT) == 0 ){
		return -1;
	}
	pName = ph7_array_fetch(pData,"timezone",(int)sizeof("timezone")-1);
	if( pName == 0 || (pName->iFlags & MEMOBJ_STRING) == 0 ){
		return -1;
	}
	zDate = (const char *)SyBlobData(&pDate->sBlob);
	nDate = (int)SyBlobLength(&pDate->sBlob);
	zTz   = (const char *)SyBlobData(&pName->sBlob);
	nTz   = (int)SyBlobLength(&pName->sBlob);
	iType = pType->x.iVal;
	if( iType < 1 || iType > 3 ){
		return -1;
	}
	if( DtZoneParse(zTz,nTz,&iOff,&zZone,&nZone,&iKind,zZoneBuf,sizeof(zZoneBuf)) != 0 ){
		return -1;
	}
	if( (iOff < 0 ? -iOff : iOff) / 3600 > 24 ){
		/* php reads the payload's zone back through its DATE-STRING grammar, whose
		 * offsets stop at hour 24 -- narrower than the zone constructor's 99. So a
		 * DateTime carrying `+25:00` serializes there and refuses to come back,
		 * while the DateTimeZone alone round-trips. Only the HOUR field is bounded:
		 * `+24:59` and `+24:00:01` are both fine. */
		return -1;
	}
	if( DtInitState(pCtx,zDate,nDate,iOff,zZone,nZone,iKind,&sState,zNameBuf,
		sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0 ){
		return -1;
	}
	DtStore(pVm,pThis,&sState);
	return 0;
}
/*
 * php's restore_custom_datetime_properties(): every payload key that is not part of
 * the internal shape becomes a property of the object. A REFERENCE is skipped, which
 * PHL cannot receive here (the pairs arrive already dereferenced).
 */
typedef struct dt_restore_ctx dt_restore_ctx;
struct dt_restore_ctx
{
	ph7_class_instance *pThis;
	int bZoneOnly;
};
static int DtRestoreWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	dt_restore_ctx *pRes = (dt_restore_ctx *)pUserData;
	const char *zKey;
	int nKey;
	if( !ph7_value_is_string(pKey) ){
		return PH7_OK;
	}
	zKey = ph7_value_to_string(pKey,&nKey);
	if( (nKey == 13 && SyMemcmp(zKey,"timezone_type",13) == 0)
	 || (nKey == 8  && SyMemcmp(zKey,"timezone",8) == 0)
	 || (!pRes->bZoneOnly && nKey == 4 && SyMemcmp(zKey,"date",4) == 0) ){
		return PH7_OK;
	}
	/* A name the class does not DECLARE is dropped, which is what the engine's own
	 * unserialize does with one: PHL has no dynamic properties, where php creates
	 * (and deprecates) them. */
	PH7_NativeSetProp(pRes->pThis->pVm,pRes->pThis,zKey,(sxu32)nKey,pVal);
	return PH7_OK;
}
static void DtRestoreCustomProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData,
	int bZoneOnly)
{
	dt_restore_ctx sRes;
	SXUNUSED(pVm);
	if( (pData->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return;
	}
	sRes.pThis = pThis;
	sRes.bZoneOnly = bZoneOnly;
	ph7_array_walk(pData,DtRestoreWalk,&sRes);
}
/* DateTimeZone::__serialize() / DateTime|DateTimeImmutable::__serialize() */
static int DtSerializeMagic(ph7_context *pCtx,int bZoneOnly)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_value sOut;
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( DtSerializePayload(pCtx,pThis,bZoneOnly,&sOut) != 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_result_value(pCtx,&sOut);
	PH7_MemObjRelease(&sOut);
	return PH7_OK;
}
static int vm_builtin_DateTimeZone_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DtSerializeMagic(pCtx,1);
}
static int vm_builtin_DateTime_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DtSerializeMagic(pCtx,0);
}
/* __unserialize(array $data): restore the state, then the subclass's own slots. */
static int DtUnserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg,int bZoneOnly,
	const char *zClass)
{
	/* Raw: this is a door that INITIALIZES -- unserialize() calls it on an object
	 * the engine built without a constructor, which is the whole point of it. */
	ph7_class_instance *pThis = DtThisRaw(pCtx);
	int rc;
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( DtCheckDataArg(pCtx,nArg,apArg,zClass) != 0 ){
		return PH7_EXCEPTION;
	}
	rc = bZoneOnly ? DtZoneRestore(pCtx->pVm,pThis,apArg[0])
	               : DtDateRestore(pCtx,pThis,apArg[0]);
	if( rc != 0 ){
		return DtSerialError(pCtx,zClass);
	}
	DtRestoreCustomProps(pCtx->pVm,pThis,apArg[0],bZoneOnly);
	DtSetInit(pCtx->pVm,pThis);
	return PH7_OK;
}
static int vm_builtin_DateTimeZone_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtUnserializeMagic(pCtx,nArg,apArg,1,"DateTimeZone");
}
static int vm_builtin_DateTime_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtUnserializeMagic(pCtx,nArg,apArg,0,"DateTime");
}
static int vm_builtin_DateTimeImmutable_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtUnserializeMagic(pCtx,nArg,apArg,0,"DateTimeImmutable");
}
/*
 * __wakeup(): the LEGACY payload, whose pairs the engine wrote into the object's own
 * properties before calling this. php reads Z_OBJPROP and restores from it, so an
 * object that has no such properties -- a plain `new DateTime` -- is exactly the
 * failure case, and php raises the same Error there.
 */
static int DtWakeupMagic(ph7_context *pCtx,int bZoneOnly,const char *zClass)
{
	/* Raw, for the reason __unserialize() is: php reads the object's own properties
	 * here and raises `Invalid serialization data` when they do not describe a
	 * date -- which is what an unconstructed object's empty set does. */
	ph7_class_instance *pThis = DtThisRaw(pCtx);
	ph7_value sProps;
	int rc;
	if( pThis == 0 ){
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm,&sProps);
	if( PH7_MemObjToHashmap(&sProps) != SXRET_OK ){
		PH7_MemObjRelease(&sProps);
		return PH7_ContextMemoryError(pCtx);
	}
	DtAddCommonProps(pCtx->pVm,pThis,&sProps);
	rc = bZoneOnly ? DtZoneRestore(pCtx->pVm,pThis,&sProps)
	               : DtDateRestore(pCtx,pThis,&sProps);
	PH7_MemObjRelease(&sProps);
	if( rc != 0 ){
		return DtSerialError(pCtx,zClass);
	}
	DtSetInit(pCtx->pVm,pThis);
	return PH7_OK;
}
static int vm_builtin_DateTimeZone_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DtWakeupMagic(pCtx,1,"DateTimeZone");
}
static int vm_builtin_DateTime_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DtWakeupMagic(pCtx,0,"DateTime");
}
static int vm_builtin_DateTimeImmutable_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DtWakeupMagic(pCtx,0,"DateTimeImmutable");
}
/*
 * __set_state(array $array): what var_export's `\DateTime::__set_state(array(…))`
 * text evaluates to. php instantiates the class the method is DECLARED on and not
 * the called one -- `MyDateTime::__set_state(…)` answers a plain DateTime there --
 * so this deliberately does not go through DtFactoryClass().
 */
static int DtSetStateMagic(ph7_context *pCtx,int nArg,ph7_value **apArg,int bZoneOnly,
	const char *zClass)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = DtClass(pVm,zClass);
	ph7_class_instance *pObj;
	int rc;
	if( pClass == 0 ){
		return PH7_OK;
	}
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s::__set_state(): Argument #1 ($array) must be of type array, %s given",
			zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");
	}
	pObj = DtNewInstance(pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	rc = bZoneOnly ? DtZoneRestore(pVm,pObj,apArg[0])
	               : DtDateRestore(pCtx,pObj,apArg[0]);
	if( rc != 0 ){
		PH7_ClassInstanceUnref(pObj);
		return DtSerialError(pCtx,zClass);
	}
	DtRestoreCustomProps(pVm,pObj,apArg[0],bZoneOnly);
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
static int vm_builtin_DateTimeZone_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtSetStateMagic(pCtx,nArg,apArg,1,"DateTimeZone");
}
static int vm_builtin_DateTime_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtSetStateMagic(pCtx,nArg,apArg,0,"DateTime");
}
static int vm_builtin_DateTimeImmutable_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtSetStateMagic(pCtx,nArg,apArg,0,"DateTimeImmutable");
}
/*
 * ---------------------------------------------------------------------------
 * php's serialization quartet for DateInterval and DatePeriod.
 *
 * Both classes declare __serialize/__unserialize/__wakeup/__set_state there and
 * neither declared any of them here, so `serialize()` walked the slots by luck
 * (the bytes matched, because for these two classes the state IS the php-visible
 * property table), `var_export()`'s `\DateInterval::__set_state(array(...))` text
 * evaluated to "Call to undefined method", and `serialize()` of an object that
 * was never constructed answered a payload where php raises.
 *
 * The two RESTORE rules are not the same rule, and both are php's:
 *
 *   DateInterval reads each field on its own and fills a MISSING one with -1 --
 *   timelib's "unset" marker, which is why `unserialize('O:12:"DateInterval":0:{}')`
 *   is an interval of -1 years. `invert`, `f` and `from_string` are the three
 *   exceptions, absent as 0, 0.0 and false. Nothing is refused.
 *
 *   DatePeriod refuses ANY payload that is not complete and well-typed: all seven
 *   keys, the three dates null or a DateTimeInterface, the interval a
 *   DateInterval, the count a real int and the two flags real bools -- one miss
 *   and it is php's `Invalid serialization data for DatePeriod object`.
 * ---------------------------------------------------------------------------
 */
/* __serialize(): the object's own visible slots, mangled the way php mangles a
 * non-public one -- which is what a subclass's private property serializes as. */
static int DtSerializeProps(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);   /* php raises on an unconstructed one */
	ph7_value sOut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm,&sOut);
	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){
		PH7_MemObjRelease(&sOut);
		return PH7_ContextMemoryError(pCtx);
	}
	/* The class's own shape first and the object's additions behind it, which is
	 * php's order here and the opposite of the presentation's (where the struct
	 * writes into the object's table). */
	DtAddNativeProps(pCtx->pVm,pThis,
		DtIsA(pCtx->pVm,pThis,"DateInterval") ? "DateInterval" : "DatePeriod",&sOut);
	DtAddCommonProps(pCtx->pVm,pThis,&sOut);
	ph7_result_value(pCtx,&sOut);
	PH7_MemObjRelease(&sOut);
	return PH7_OK;
}
/* One payload field, with the value php gives an absent one. A container is not a
 * number to php's reader either, so it counts as absent. */
static sxi64 DtIvRestoreInt(ph7_value *pData,const char *zKey,sxi64 iAbsent)
{
	ph7_value *pVal = ph7_array_fetch(pData,zKey,-1);
	if( pVal == 0 || (pVal->iFlags & (MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_BOOL|MEMOBJ_STRING)) == 0 ){
		return iAbsent;
	}
	return PH7_ValuePeekInt64(pVal);
}
/* php's date_interval_initialize_from_hash(), field by field. */
static void DtIvRestore(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData)
{
	static const char * const azMinusOne[] = { "y","m","d","h","i","s" };
	ph7_value *pVal;
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(azMinusOne) ; ++n ){
		PH7_NativeSetAttrInt(&(*pVm),pThis,azMinusOne[n],
			DtIvRestoreInt(pData,azMinusOne[n],-1));
	}
	PH7_NativeSetAttrInt(&(*pVm),pThis,"invert",DtIvRestoreInt(pData,"invert",0));
	/* `days` is php's one field with two TYPES -- a day count, or false when the
	 * interval was not measured between two dates -- so a bool payload stays a
	 * bool where every other field is narrowed to an int. */
	pVal = ph7_array_fetch(pData,"days",-1);
	if( pVal && (pVal->iFlags & MEMOBJ_BOOL) ){
		PH7_NativeSetAttrBool(&(*pVm),pThis,"days",pVal->x.iVal != 0);
	}else{
		PH7_NativeSetAttrInt(&(*pVm),pThis,"days",DtIvRestoreInt(pData,"days",-1));
	}
	pVal = ph7_array_fetch(pData,"f",-1);
	DtIvSetUsec(&(*pVm),pThis,
		pVal && (pVal->iFlags & (MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_BOOL|MEMOBJ_STRING))
			? DtIvUsecOfReal((double)PH7_ValuePeekReal(pVal)) : 0);
	/* php's LAZY interval, and what it keys on is the STRING rather than the
	 * flag beside it: a payload carrying `date_string` comes back as an interval
	 * built from that string -- the ten fields parsed out of it and hidden behind
	 * the two names php shows -- while one carrying `from_string` alone is an
	 * ordinary interval whose fields the reader above already filled. */
	pVal = ph7_array_fetch(pData,"date_string",-1);
	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){
		sxi64 aVal[DT_IV_USLOT + 1];
		int nIn,iPos = 0;
		char cAt = ' ';
		const char *zIn,*zReason = "";
		zIn = (const char *)SyBlobData(&pVal->sBlob);
		nIn = (int)SyBlobLength(&pVal->sBlob);
		if( DtIvParseRelative(zIn,nIn,aVal,&iPos,&cAt,&zReason) == 0 ){
			/* DtIvStore writes the six fields and the microseconds; `days` is
			 * php's own answer for an interval that was not measured between two
			 * dates. */
			DtIvStore(&(*pVm),pThis,aVal);
			PH7_NativeSetAttrBool(&(*pVm),pThis,"days",0);
		}
		DtIvFromString(&(*pVm),pThis,zIn,nIn);
		return;
	}
	/* ...and a payload without one is an ordinary interval whatever its
	 * `from_string` says: php answers false there even for the `b:1` a hand-made
	 * payload carries. */
	PH7_NativeSetAttrBool(&(*pVm),pThis,"from_string",0);
}
/*
 * php's date_period_initialize_from_hash(): the whole payload or nothing.
 * Answers 0 when it restored, -1 when the caller must raise.
 */
static int DpRestoreOne(ph7_vm *pVm,ph7_value *pData,const char *zKey,const char *zClass,
	int iFlags,ph7_value **ppOut)
{
	ph7_value *pVal = ph7_array_fetch(pData,zKey,-1);
	if( pVal == 0 ){
		return -1;
	}
	if( zClass ){
		if( (pVal->iFlags & MEMOBJ_NULL) == 0
		 && !DtValueIsA(&(*pVm),pVal,zClass) ){
			return -1;
		}
	}else if( (pVal->iFlags & iFlags) == 0 ){
		return -1;
	}
	*ppOut = pVal;
	return 0;
}
static int DpRestore(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData)
{
	ph7_value *pStart,*pCur,*pEnd,*pIv,*pRec,*pIncS,*pIncE;
	if( (pData->iFlags & MEMOBJ_HASHMAP) == 0
	 || DpRestoreOne(&(*pVm),pData,"start","DateTimeInterface",0,&pStart) != 0
	 || DpRestoreOne(&(*pVm),pData,"current","DateTimeInterface",0,&pCur) != 0
	 || DpRestoreOne(&(*pVm),pData,"end","DateTimeInterface",0,&pEnd) != 0
	 || DpRestoreOne(&(*pVm),pData,"interval","DateInterval",0,&pIv) != 0
	 || DpRestoreOne(&(*pVm),pData,"recurrences",0,MEMOBJ_INT,&pRec) != 0
	 || DpRestoreOne(&(*pVm),pData,"include_start_date",0,MEMOBJ_BOOL,&pIncS) != 0
	 || DpRestoreOne(&(*pVm),pData,"include_end_date",0,MEMOBJ_BOOL,&pIncE) != 0 ){
		return -1;
	}
	PH7_NativeSetProp(&(*pVm),pThis,"start",sizeof("start")-1,pStart);
	PH7_NativeSetProp(&(*pVm),pThis,"current",sizeof("current")-1,pCur);
	PH7_NativeSetProp(&(*pVm),pThis,"end",sizeof("end")-1,pEnd);
	PH7_NativeSetProp(&(*pVm),pThis,"interval",sizeof("interval")-1,pIv);
	PH7_NativeSetProp(&(*pVm),pThis,"recurrences",sizeof("recurrences")-1,pRec);
	PH7_NativeSetProp(&(*pVm),pThis,"include_start_date",sizeof("include_start_date")-1,pIncS);
	PH7_NativeSetProp(&(*pVm),pThis,"include_end_date",sizeof("include_end_date")-1,pIncE);
	return 0;
}
/* Every payload key that is not part of the class's own shape becomes a property,
 * the way php's restore_custom_* does (a name the class does not DECLARE is
 * dropped here, which is what PHL's own unserialize does with one). */
typedef struct dt_prop_restore dt_prop_restore;
struct dt_prop_restore
{
	ph7_class_instance *pThis;
	const char * const *azOwn;   /* the keys the class's own restore already read */
	sxu32 nOwn;
};
static int DtPropRestoreWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	dt_prop_restore *pRes = (dt_prop_restore *)pUserData;
	const char *zKey;
	int nKey;
	sxu32 n;
	if( !ph7_value_is_string(pKey) ){
		return PH7_OK;
	}
	zKey = ph7_value_to_string(pKey,&nKey);
	for( n = 0 ; n < pRes->nOwn ; ++n ){
		if( (int)SyStrlen(pRes->azOwn[n]) == nKey
		 && SyMemcmp(pRes->azOwn[n],zKey,(sxu32)nKey) == 0 ){
			return PH7_OK;
		}
	}
	PH7_NativeSetProp(pRes->pThis->pVm,pRes->pThis,zKey,(sxu32)nKey,pVal);
	return PH7_OK;
}
/* The shared body of __unserialize/__wakeup/__set_state for the two classes:
 * bPeriod picks the rule, pThis is the object being filled. */
static int DtPropsRestoreInto(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pData,
	int bPeriod,const char *zClass)
{
	static const char * const azIvOwn[] = {
		"y","m","d","h","i","s","f","invert","days","from_string","date_string"
	};
	static const char * const azDpOwn[] = {
		"start","current","end","interval","recurrences",
		"include_start_date","include_end_date"
	};
	dt_prop_restore sRes;
	if( bPeriod ){
		if( DpRestore(pCtx->pVm,pThis,pData) != 0 ){
			return DtSerialError(pCtx,zClass);
		}
	}else{
		DtIvRestore(pCtx->pVm,pThis,pData);
	}
	sRes.pThis = pThis;
	sRes.azOwn = bPeriod ? azDpOwn : azIvOwn;
	sRes.nOwn = bPeriod ? SX_ARRAYSIZE(azDpOwn) : SX_ARRAYSIZE(azIvOwn);
	ph7_array_walk(pData,DtPropRestoreWalk,&sRes);
	DtSetInit(pCtx->pVm,pThis);
	return PH7_OK;
}
static int DtUnserializeProps(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPeriod,
	const char *zClass)
{
	/* Raw: this is a door that INITIALIZES -- unserialize() calls it on an object
	 * the engine built without a constructor. */
	ph7_class_instance *pThis = DtThisRaw(pCtx);
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( DtCheckDataArg(pCtx,nArg,apArg,zClass) != 0 ){
		return PH7_EXCEPTION;
	}
	return DtPropsRestoreInto(pCtx,pThis,apArg[0],bPeriod,zClass);
}
static int vm_builtin_DateInterval_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtUnserializeProps(pCtx,nArg,apArg,0,"DateInterval");
}
static int vm_builtin_DatePeriod_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtUnserializeProps(pCtx,nArg,apArg,1,"DatePeriod");
}
/*
 * __wakeup(): the LEGACY payload, which the engine has already written into the
 * object's own properties -- so the restore rule reads them back off the object
 * itself. An interval whose payload said nothing becomes php's all -1 interval;
 * a period whose payload is incomplete is php's Error, `new DatePeriod` included.
 */
static int DtWakeupProps(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPeriod,
	const char *zClass)
{
	ph7_class_instance *pThis = DtThisRaw(pCtx);
	ph7_value sProps;
	int rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm,&sProps);
	if( PH7_MemObjToHashmap(&sProps) != SXRET_OK ){
		PH7_MemObjRelease(&sProps);
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)sProps.x.pOther);
	rc = DtPropsRestoreInto(pCtx,pThis,&sProps,bPeriod,zClass);
	PH7_MemObjRelease(&sProps);
	return rc;
}
static int vm_builtin_DateInterval_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtWakeupProps(pCtx,nArg,apArg,0,"DateInterval");
}
static int vm_builtin_DatePeriod_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtWakeupProps(pCtx,nArg,apArg,1,"DatePeriod");
}
/* __set_state(array $array): what var_export's text evaluates to. php builds the
 * class the method is DECLARED on, as it does for the date classes. */
static int DtSetStateProps(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPeriod,
	const char *zClass)
{
	ph7_class *pClass = DtClass(pCtx->pVm,zClass);
	ph7_class_instance *pObj;
	int rc;
	if( pClass == 0 ){
		return PH7_OK;
	}
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s::__set_state(): Argument #1 ($array) must be of type array, %s given",
			zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");
	}
	/* Not DtNewInstance(): the restore below is what initializes it, and only if
	 * it succeeds. */
	pObj = PH7_NewClassInstance(pCtx->pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	rc = DtPropsRestoreInto(pCtx,pObj,apArg[0],bPeriod,zClass);
	if( rc != PH7_OK || pCtx->nThrowRc != 0 ){
		PH7_ClassInstanceUnref(pObj);
		return rc;
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
static int vm_builtin_DateInterval_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtSetStateProps(pCtx,nArg,apArg,0,"DateInterval");
}
static int vm_builtin_DatePeriod_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DtSetStateProps(pCtx,nArg,apArg,1,"DatePeriod");
}
/* The four rows both date classes take. __serialize/__unserialize are php's only
 * NON-tentative internal returns in this family; __wakeup and __set_state carry the
 * `@`, and __set_state's return names the CONCRETE class php's stub writes. */
#define DT_NATIVE_SERIAL_METHODS(CLS) \
	{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_DateTime_serialize }, \
	{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void", \
	  vm_builtin_##CLS##_unserialize }, \
	{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_##CLS##_wakeup }, \
	{ "__set_state",   PH7_MOD_PUBLIC|PH7_MOD_STATIC, "array $array", "@" #CLS, \
	  vm_builtin_##CLS##_setState }
/* php's DateTimeInterface constants. */
#define DT_IFACE_CONST(NAME,VALUE) \
	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, VALUE, 0.0 }
/*
 * Install the whole date family from C: the exceptions and DateTimeInterface, then
 * DateTimeZone / DateTime / DateTimeImmutable, DateInterval and DatePeriod, then the
 * procedural aliases. The InternalIterator its getIterator() answers is not declared
 * here — it is shared native machinery (oo_native.c), reached through the vtable
 * DatePeriod's spec row names.
 *
 * Called from PH7_VmInit inside the bCompilingBuiltin window, after the Reflection
 * install (Exception must exist). IteratorAggregate is attached AFTER DatePeriod's
 * methods exist, for the abstract-stub reason above; DateTimeInterface declares no
 * method, so it can ride the spec table.
 */
PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)
{
	static const PH7_NativeConstDef aIfaceConst[] = {
		DT_IFACE_CONST("ATOM","Y-m-d\\TH:i:sP"),
		DT_IFACE_CONST("COOKIE","l, d-M-Y H:i:s T"),
		DT_IFACE_CONST("ISO8601","Y-m-d\\TH:i:sO"),
		DT_IFACE_CONST("ISO8601_EXPANDED","X-m-d\\TH:i:sP"),
		DT_IFACE_CONST("RFC822","D, d M y H:i:s O"),
		DT_IFACE_CONST("RFC850","l, d-M-y H:i:s T"),
		DT_IFACE_CONST("RFC1036","D, d M y H:i:s O"),
		DT_IFACE_CONST("RFC1123","D, d M Y H:i:s O"),
		DT_IFACE_CONST("RFC7231","D, d M Y H:i:s \\G\\M\\T"),
		DT_IFACE_CONST("RFC2822","D, d M Y H:i:s O"),
		DT_IFACE_CONST("RFC3339","Y-m-d\\TH:i:sP"),
		DT_IFACE_CONST("RFC3339_EXTENDED","Y-m-d\\TH:i:s.vP"),
		DT_IFACE_CONST("RSS","D, d M Y H:i:s O"),
		DT_IFACE_CONST("W3C","Y-m-d\\TH:i:sP"),
	};
	/*
	 * php's DateTimeInterface METHODS, which this engine did not declare at
	 * all -- so the nine it contracts for reported no `prototype` in the
	 * export (18 rows across DateTime and DateTimeImmutable), and the
	 * interface itself answered isAbstract() false for want of a member.
	 * Declared in php's own order, which is the order its export lists them.
	 *
	 * Safe on the spec table because PH7_InstallNativeClasses fills every
	 * class's METHODS before it wires any interface: PH7_ClassImplement's
	 * abstract stubbing then finds DateTime's own nine already there and
	 * skips them, which is the same reason DatePeriod's IteratorAggregate is
	 * attached by hand AFTER its methods (it is not in this table).
	 */
	static const PH7_NativeMethodDef aIfaceMethod[] = {
		{ "format",        PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "string $format", "@string", 0 },
		{ "getTimezone",   PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@DateTimeZone|false", 0 },
		{ "getOffset",     PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@int", 0 },
		{ "getTimestamp",  PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@int", 0 },
		{ "getMicrosecond",PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "int", 0 },
		{ "diff",          PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT,
		  "DateTimeInterface $targetObject, bool $absolute = false", "@DateInterval", 0 },
		{ "__wakeup",      PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@void", 0 },
		{ "__serialize",   PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "array", 0 },
		{ "__unserialize", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "array $data", "void", 0 },
	};
	static const PH7_NativePropDef aZoneProp[] = {
		{ DTZ_OFF,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },
		{ DTZ_NAME, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 }, 0 },
		{ DTZ_KIND, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_INT, DT_ZONE_ID, 0, 0.0 }, 0 },
		{ DT_INIT,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aZoneMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $timezone", "", vm_builtin_DateTimeZone_construct },
		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_DateTimeZone_getName },
		{ "getOffset",   PH7_MOD_PUBLIC, "DateTimeInterface $datetime", "@int",
		  vm_builtin_DateTimeZone_getOffset },
		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_DateTimeZone_serialize },
		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",
		  vm_builtin_DateTimeZone_unserialize },
		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DateTimeZone_wakeup },
		{ "__set_state",   PH7_MOD_PUBLIC|PH7_MOD_STATIC, "array $array", "@DateTimeZone",
		  vm_builtin_DateTimeZone_setState },
		{ "listIdentifiers", PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "int $timezoneGroup = 2047, ?string $countryCode = null", "@array",
		  vm_builtin_DateTimeZone_listIdentifiers },
		{ "getTransitions", PH7_MOD_PUBLIC,
		  "int $timestampBegin = 0, int $timestampEnd = 2147483647", "@array",
		  vm_builtin_DateTimeZone_getTransitions },
	};
	/* php's group bitmask. ALL and ALL_WITH_BC are the two the reader compares
	 * EXACTLY rather than masking (see DtZoneListResult). */
	static const PH7_NativeConstDef aZoneConst[] = {
		{ "AFRICA",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,    0, 0.0 },
		{ "AMERICA",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,    0, 0.0 },
		{ "ANTARCTICA",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,    0, 0.0 },
		{ "ARCTIC",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8,    0, 0.0 },
		{ "ASIA",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,   0, 0.0 },
		{ "ATLANTIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,   0, 0.0 },
		{ "AUSTRALIA",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,   0, 0.0 },
		{ "EUROPE",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128,  0, 0.0 },
		{ "INDIAN",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 256,  0, 0.0 },
		{ "PACIFIC",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 512,  0, 0.0 },
		{ "UTC",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1024, 0, 0.0 },
		{ "ALL",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DT_TZ_GROUP_ALL, 0, 0.0 },
		{ "ALL_WITH_BC", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DT_TZ_GROUP_ALL_W_BC, 0, 0.0 },
		{ "PER_COUNTRY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DT_TZ_GROUP_PER_COUNTRY, 0, 0.0 }
	};
	static const PH7_NativePropDef aDtProp[] = { DT_NATIVE_STATE_PROPS };
	static const PH7_NativeMethodDef aDtMethod[] = {
		DT_NATIVE_SHARED_METHODS("DateTime"),
		{ "createFromFormat",    PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTime|false",
		  vm_builtin_DateTime_createFromFormat },
		{ "createFromImmutable", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTimeImmutable $object", "@static",
		  vm_builtin_DateTime_copyOf },
		{ "createFromTimestamp", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "int|float $timestamp", "@static",
		  vm_builtin_DateTime_createFromTimestamp },
		{ "createFromInterface", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTime",
		  vm_builtin_DateTime_copyOf },
		DT_NATIVE_SERIAL_METHODS(DateTime),
	};
	static const PH7_NativeMethodDef aImmMethod[] = {
		DT_NATIVE_SHARED_METHODS("DateTimeImmutable"),
		{ "createFromFormat",    PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTimeImmutable|false",
		  vm_builtin_DateTimeImmutable_createFromFormat },
		{ "createFromMutable",   PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTime $object", "@static",
		  vm_builtin_DateTimeImmutable_copyOf },
		{ "createFromTimestamp", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "int|float $timestamp", "@static",
		  vm_builtin_DateTimeImmutable_createFromTimestamp },
		{ "createFromInterface", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTimeImmutable",
		  vm_builtin_DateTimeImmutable_copyOf },
		DT_NATIVE_SERIAL_METHODS(DateTimeImmutable),
	};
	static const PH7_NativePropDef aIvProp[] = {
		{ "y",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },
		{ "m",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },
		{ "d",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },
		{ "h",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },
		{ "i",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },
		{ "s",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },
		/* php's `f` is a FLOAT; the chunk's `= 0` made it an int. */
		{ "f",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_DOUBLE, 0, 0, 0.0 }, 0 },
		{ "invert",      PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },
		{ "days",        PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },
		{ "from_string", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },
		/* php's `date_string` is on an interval built from a STRING and on no
		 * other, so it is installed by the write that names it: an ordinary
		 * interval does not carry the name at all, and `isset()` says so. */
		{ "date_string", PH7_MOD_PUBLIC|PH7_MOD_ONDEMAND,
		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
		/* php's timelib_rel_time.us, the count `f` renders: see DtIvUsec. */
		{ DT_IV_US,      PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ DT_INIT,       PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aIvMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $duration", "",
		  vm_builtin_DateInterval_construct },
		{ "createFromDateString", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "string $datetime", "@DateInterval",
		  vm_builtin_DateInterval_createFromDateString },
		{ "format",      PH7_MOD_PUBLIC, "string $format", "@string", vm_builtin_DateInterval_format },
		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", DtSerializeProps },
		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",
		  vm_builtin_DateInterval_unserialize },
		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DateInterval_wakeup },
		{ "__set_state",   PH7_MOD_PUBLIC|PH7_MOD_STATIC, "array $array", "@DateInterval",
		  vm_builtin_DateInterval_setState },
	};
	/* php models all seven as VIRTUAL hooked properties, so it reports no default
	 * for any of them; PHL's are real slots and keep theirs, because a read before
	 * the first write must answer what php's getter answers rather than raise. The
	 * TYPE is what a spec row can state exactly — the virtual half is recorded. */
	static const PH7_NativePropDef aDpProp[] = {
		{ "start",              PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },
		{ "current",            PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },
		{ "end",                PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },
		{ "interval",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateInterval" },
		/* php FABRICATES these from its struct, so an object with no struct reads
		 * them as the zeroed one: 0 and false, not the 1 and true a constructed
		 * period ends up with. Every constructor path writes all three. */
		{ "recurrences",        PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, "int" },
		{ "include_start_date", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },
		{ "include_end_date",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },
		{ DT_INIT,              PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeConstDef aDpConst[] = {
		{ "EXCLUDE_START_DATE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },
		{ "INCLUDE_END_DATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },
	};
	static const PH7_NativeMethodDef aDpMethod[] = {
		/* php overloads this constructor three ways and rejects everything else with
		 * ONE message, so the signature stays unenforced and the body decides. */
		/* No signature ON PURPOSE, which is why Reflection reports no parameters
		 * for it. php declares four and enforces NEITHER end of the arity: the
		 * constructor has three shapes (start+interval+end, start+interval+count,
		 * and the ISO string), and both `new DatePeriod()` and a five-argument
		 * call reach the body and answer its own three-shape TypeError. A zSig
		 * here would enforce both bounds, so the choice is php's DIAGNOSTIC or
		 * php's parameter list, and the diagnostic wins. */
		{ "__construct",     PH7_MOD_PUBLIC, 0, "", vm_builtin_DatePeriod_construct },
		{ "createFromISO8601String", PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $specification, int $options = 0", "static",
		  vm_builtin_DatePeriod_createFromISO8601String },
		{ "getStartDate",    PH7_MOD_PUBLIC, "", "@DateTimeInterface",
		  vm_builtin_DatePeriod_getStartDate },
		{ "getEndDate",      PH7_MOD_PUBLIC, "", "@?DateTimeInterface",
		  vm_builtin_DatePeriod_getEndDate },
		{ "getDateInterval", PH7_MOD_PUBLIC, "", "@DateInterval",
		  vm_builtin_DatePeriod_getDateInterval },
		{ "getRecurrences",  PH7_MOD_PUBLIC, "", "@?int", vm_builtin_DatePeriod_getRecurrences },
		{ "getIterator",     PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_DatePeriod_getIterator },
		{ "__serialize",     PH7_MOD_PUBLIC, "", "array", DtSerializeProps },
		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "void",
		  vm_builtin_DatePeriod_unserialize },
		{ "__wakeup",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_DatePeriod_wakeup },
		{ "__set_state",     PH7_MOD_PUBLIC|PH7_MOD_STATIC, "array $array", "@DatePeriod",
		  vm_builtin_DatePeriod_setState },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		/* Exceptions first: the classes below throw them. */
		{ "DateException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateMalformedStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateInvalidTimeZoneException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateMalformedIntervalStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateMalformedPeriodStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateInvalidOperationException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		/* php's date tree has an ERROR half beside the exception one -- what a
		 * caller catches when an argument is out of RANGE (setMicrosecond) or the
		 * object was never constructed. All three were undefined here, so
		 * `catch (DateRangeError $e)` could not be spelled at all. */
		{ "DateError", "Error", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateRangeError", "DateError", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateObjectError", "DateError", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateTimeInterface", 0, 0, PH7_CLASS_INTERFACE,
		  aIfaceMethod, SX_ARRAYSIZE(aIfaceMethod),
		  aIfaceConst, SX_ARRAYSIZE(aIfaceConst), 0, 0, 0, 0, 0 },
		{ "DateTimeZone", 0, 0, 0,
		  aZoneMethod, SX_ARRAYSIZE(aZoneMethod), aZoneConst, SX_ARRAYSIZE(aZoneConst),
		  aZoneProp, SX_ARRAYSIZE(aZoneProp),
		  0, 0, DtPresentTimeZone },
		{ "DateTime", 0, "DateTimeInterface", 0,
		  aDtMethod, SX_ARRAYSIZE(aDtMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),
		  0, 0, DtPresentDateTime },
		{ "DateTimeImmutable", 0, "DateTimeInterface", 0,
		  aImmMethod, SX_ARRAYSIZE(aImmMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),
		  0, 0, DtPresentDateTime },
		{ "DateInterval", 0, 0, 0,
		  aIvMethod, SX_ARRAYSIZE(aIvMethod), 0, 0, aIvProp, SX_ARRAYSIZE(aIvProp),
		  0, 0, DtPresentProps },
		{ "DatePeriod", 0, 0, 0,
		  aDpMethod, SX_ARRAYSIZE(aDpMethod), aDpConst, SX_ARRAYSIZE(aDpConst),
		  aDpProp, SX_ARRAYSIZE(aDpProp), 0, &sDpIterVtab, DtPresentProps },
	};
	/* php's procedural aliases. Each is a function in its own right, not a forward,
	 * and each owes aBuiltinSig[] a row (vm_arg_check.c). */
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "strtotime",                    vm_builtin_strtotime },
		{ "date_create",                  vm_builtin_date_create },
		{ "date_create_immutable",        vm_builtin_date_create_immutable },
		{ "date_create_from_format",      vm_builtin_date_create_from_format },
		{ "date_create_immutable_from_format", vm_builtin_date_create_immutable_from_format },
		{ "date_format",                  vm_builtin_date_format },
		{ "date_modify",                  vm_builtin_date_modify },
		{ "date_add",                     vm_builtin_date_add },
		{ "date_sub",                     vm_builtin_date_sub },
		{ "date_diff",                    vm_builtin_date_diff },
		{ "date_timestamp_get",           vm_builtin_date_timestamp_get },
		{ "date_timestamp_set",           vm_builtin_date_timestamp_set },
		{ "date_timezone_get",            vm_builtin_date_timezone_get },
		{ "date_timezone_set",            vm_builtin_date_timezone_set },
		{ "date_offset_get",              vm_builtin_date_offset_get },
		{ "date_date_set",                vm_builtin_date_date_set },
		{ "date_time_set",                vm_builtin_date_time_set },
		{ "date_isodate_set",             vm_builtin_date_isodate_set },
		{ "date_interval_create_from_date_string", vm_builtin_date_interval_create_from_date_string },
		{ "date_interval_format",         vm_builtin_date_interval_format },
		{ "date_get_last_errors",         vm_builtin_date_get_last_errors },
		{ "date_parse",                   vm_builtin_date_parse },
		{ "date_parse_from_format",       vm_builtin_date_parse_from_format },
		{ "timezone_open",                vm_builtin_timezone_open },
		{ "timezone_name_get",            vm_builtin_timezone_name_get },
		{ "timezone_offset_get",          vm_builtin_timezone_offset_get },
		{ "timezone_identifiers_list",    vm_builtin_timezone_identifiers_list },
		{ "timezone_transitions_get",     vm_builtin_timezone_transitions_get },
	};
	sxu32 n;
	sxi32 rc;
	/* php's date.timezone default */
	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));
	pVm->nDefTz = sizeof("UTC") - 1;
	/* The error rows are allocated from the VM's own backend and released
	 * wholesale with it, so this is the only lifetime call they need. */
	SyBlobInit(&pVm->sDtLastErr.sErr,&pVm->sAllocator);
	DtLastErrClear(&(*pVm));
	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* php's write_property handler for DateInterval (ph7_class::xSet), assigned
	 * here for the reason the DOM's clone and dimension hooks are: the spec table
	 * carries no field for a hook. It also flags the class's properties, which is
	 * what makes `new` register their slots with the store filter. */
	rc = PH7_NativeClassInstallSetHook(&(*pVm),"DateInterval",DtIntervalSet);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* php's compare handlers (ph7_class::xCmp), assigned here for the same reason
	 * the write handler is. DatePeriod gets none: php has no handler for it, its
	 * real property table is EMPTY (the seven it shows are fabricated), and the
	 * ordinary walk over nothing is what makes any two of them equal -- which is
	 * what marking those seven virtual reproduces. */
	{
		static const struct {
			const char *zClass;
			void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *);
		} aCmp[] = {
			{ "DateTime",          DtCmpDateTime },
			{ "DateTimeImmutable", DtCmpDateTime },
			{ "DateInterval",      DtCmpInterval },
			{ "DateTimeZone",      DtCmpTimeZone },
		};
		for( n = 0 ; n < SX_ARRAYSIZE(aCmp) ; n++ ){
			rc = PH7_NativeClassInstallCmpHook(&(*pVm),aCmp[n].zClass,aCmp[n].xCmp);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
	}
	rc = PH7_NativeClassMarkVirtualProps(&(*pVm),"DatePeriod");
	if( rc != SXRET_OK ){
		return rc;
	}
	/* php 8.5 marks every IMMUTABLE mutator #[\NoDiscard]: these nine answer a NEW
	 * object and change nothing, so a caller who drops the answer wrote a
	 * statement that does nothing at all -- the single most common way to misuse
	 * DateTimeImmutable. The mutable DateTime twins are NOT marked (there the
	 * object really did change), and neither is any other internal member: this is
	 * php's whole internal NoDiscard set. The message is php's own wording, with
	 * the method named in it. */
	{
		/* Nine rows, nine static literals: PH7_NativeMethodSetNoDiscard borrows the
		 * argument record for the VM's lifetime. php's stub spells the message as a
		 * NAMED argument, and getArguments() shows the key. */
		static const PH7_NativeAttrArg aNdWhy[] = {
			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,
			  "as DateTimeImmutable::modify() does not modify the object itself", 0.0 } },
			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,
			  "as DateTimeImmutable::add() does not modify the object itself", 0.0 } },
			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,
			  "as DateTimeImmutable::sub() does not modify the object itself", 0.0 } },
			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,
			  "as DateTimeImmutable::setTimezone() does not modify the object itself", 0.0 } },
			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,
			  "as DateTimeImmutable::setTime() does not modify the object itself", 0.0 } },
			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,
			  "as DateTimeImmutable::setDate() does not modify the object itself", 0.0 } },
			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,
			  "as DateTimeImmutable::setISODate() does not modify the object itself", 0.0 } },
			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,
			  "as DateTimeImmutable::setTimestamp() does not modify the object itself", 0.0 } },
			{ "message", { 0, 0, PH7_NATIVE_VAL_STRING, 0,
			  "as DateTimeImmutable::setMicrosecond() does not modify the object itself", 0.0 } },
		};
		static const char *const azNdMethod[] = {
			"modify","add","sub","setTimezone","setTime","setDate","setISODate",
			"setTimestamp","setMicrosecond"
		};
		ph7_class *pImm = PH7_VmExtractClass(&(*pVm),"DateTimeImmutable",
			sizeof("DateTimeImmutable")-1,FALSE,0);
		for( n = 0 ; n < SX_ARRAYSIZE(azNdMethod) ; n++ ){
			rc = PH7_NativeMethodSetNoDiscard(&(*pVm),pImm,azNdMethod[n],&aNdWhy[n],1);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
	}
	/* php's state for these two IS their properties, and the table is written FROM
	 * the C struct its constructor allocates -- so an object nobody constructed has
	 * no such property at all. PHL declared them from `new`, so an unconstructed
	 * interval answered ten defaults to a read, ten to isset(), ten to
	 * get_object_vars() and ten to a property foreach, beside the empty shape the
	 * presentation hook was already showing. The two classes differ in what a read
	 * of a still-absent slot answers, which is php's split between its two
	 * handlers: DatePeriod reads its seven from the zeroed struct (null/0/false, in
	 * silence), DateInterval has no such fallback and its ten really are undefined
	 * until the constructor runs. */
	rc = PH7_NativeClassMarkLazyProps(&(*pVm),"DateInterval",0);
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = PH7_NativeClassMarkLazyProps(&(*pVm),"DatePeriod",1);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* php's write_property handler for DatePeriod refuses OUTRIGHT: the seven are
	 * a view of its struct and a script may only read them. PHL kept real slots a
	 * script could write, so `$p->recurrences = 99` and `$p->start = 5` landed and
	 * the period then iterated to a shape no constructor would have built --
	 * `unset($p->interval)` left one with no interval at all. */
	rc = PH7_NativeClassMarkNoWriteProps(&(*pVm),"DatePeriod");
	if( rc != SXRET_OK ){
		return rc;
	}
	/* IteratorAggregate declares a METHOD, so it is attached now that DatePeriod has
	 * its own: PH7_ClassImplement stubs a missing one as ABSTRACT, which would have
	 * made the class uninstantiable. */
	{
		ph7_class *pPeriod = DtClass(&(*pVm),"DatePeriod");
		ph7_class *pAggregate = DtClass(&(*pVm),"IteratorAggregate");
		if( pPeriod == 0 || pAggregate == 0 ){
			return SXERR_NOTFOUND;
		}
		rc = PH7_ClassImplement(pPeriod,pAggregate);
	}
	return rc;
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
