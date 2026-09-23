/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * The DateTime family: proleptic-Gregorian date math, the date/time
 * string parser, the __dt_* host thunks, the embedded zDateTimeLib PHP
 * chunk and PH7_VmInstallDateTime. The classic procedural date functions
 * (date/gmdate/mktime/...) stay in builtin_date.c.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
#include <time.h>
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
PH7_PRIVATE sxi64 DtDaysFromCivil(sxi64 y,int m,int d)
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
PH7_PRIVATE void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)
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
PH7_PRIVATE sxi64 DtFloorDiv(sxi64 a,sxi64 b)
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
 * Parse an OPTIONAL time-of-day suffix after a date component:
 * "[( |T)]HH:MM[:SS][.frac][Z|±hh[:mm]]". On entry *pz points just past the date;
 * the h/mi/s outs must be pre-zeroed and the offset outs pre-seeded with the current
 * offset; *pUs receives the microseconds from a fractional part (unchanged when
 * absent). Advances *pz over whatever it consumes. Returns 0 on success (whether or
 * not a time was present), or a 1-based error position into zIn (negative encodes
 * php's "Double time specification"). Shared by every absolute-date branch.
 */
static int DtTimeSuffix(const char **pz,const char *zEnd,const char *zIn,
	int *ph,int *pmi,int *ps,sxi32 *piOff,int *pbOffSet,int *pUs)
{
	const char *z = *pz;
	if( z < zEnd && (z[0]=='T' || z[0]==' ') && zEnd-z >= 6
	 && SyisDigit(z[1]) && SyisDigit(z[2]) && z[3]==':' ){
		z++;
		*ph  = (z[0]-'0')*10 + (z[1]-'0');
		*pmi = (z[3]-'0')*10 + (z[4]-'0');
		/* a 25+ hour kills php's whole time token: error at its start */
		if( *ph > 24 ){ return (int)(z - zIn) + 1; }
		/* php lexes HH:M, then the minute's second digit starts a SECOND time
		 * token: "Double time specification" (negative encoding) */
		if( *pmi > 59 ){ return -((int)(&z[4] - zIn) + 1); }
		z += 5;
		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){
			*ps = (z[1]-'0')*10 + (z[2]-'0');
			if( *ps > 59 ){ return (int)(&z[2] - zIn) + 1; }
			z += 3;
		}
		if( z < zEnd && z[0]=='.' && zEnd-z >= 2 && SyisDigit(z[1]) ){ /* fractional seconds */
			*pUs = DtReadFraction(&z,zEnd);
		}
		if( z < zEnd && (z[0]=='Z' || z[0]=='z') ){
			*piOff = 0; *pbOffSet = 2; z++;
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
			*piOff = sign * (oh*3600 + om*60);
			*pbOffSet = 1;
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
 * Try to read a non-ISO numeric date at z: three integer components joined by ONE
 * consistent separator, plus an optional time suffix. php's field order depends on
 * the separator:
 *   '/'      -> YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY
 *   '-','.'  -> DD-MM-YYYY (day first); a 4-digit-first '.' date (YYYY.MM.DD) is
 *               NOT a php format and is rejected. (ISO YYYY-MM-DD is matched by the
 *               dedicated branch BEFORE this one, so a 4-digit-first '-' never
 *               reaches here.)
 * A 1-2 digit year maps php-style (00-69 -> 2000s, 70-99 -> 1900s). Returns 0 when
 * the text is not such a date (caller falls through), 1 on success (the ts/off outs
 * set and *pzOut advanced past the whole token), or an error code in DtParse's own
 * convention (positive 1-based position into zIn, negative = "double time") when the
 * shape matched but a component is out of range.
 */
static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,
	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,int *pUs)
{
	int a,b,c,na,nb,nc;
	char sep;
	int y,mo,d,h = 0,mi = 0,s = 0,us = 0;
	sxi32 iOff = *pOff;
	int rcT;
	/* first field: 1-4 digits */
	if( !SyisDigit(z[0]) ){ return 0; }
	a = 0; na = 0;
	while( z < zEnd && SyisDigit(z[0]) && na < 4 ){ a = a*10 + (z[0]-'0'); z++; na++; }
	if( z >= zEnd || (z[0] != '-' && z[0] != '/' && z[0] != '.') ){ return 0; }
	sep = z[0];
	z++;
	/* second field: 1-2 digits */
	if( z >= zEnd || !SyisDigit(z[0]) ){ return 0; }
	b = DtRead1or2(z,zEnd,&nb); z += nb;
	if( z >= zEnd || z[0] != sep ){ return 0; }
	z++;
	/* third field: 1-4 digits */
	if( z >= zEnd || !SyisDigit(z[0]) ){ return 0; }
	c = 0; nc = 0;
	while( z < zEnd && SyisDigit(z[0]) && nc < 4 ){ c = c*10 + (z[0]-'0'); z++; nc++; }
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
			/* php's dot date is DD.MM.YYYY only (a 4-digit year, day first). Other
			 * widths are not a clean php format (php itself yields garbage there),
			 * so don't claim the match — let the caller fail the parse. */
			if( na == 4 || nc != 4 ){ return 0; }
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
	/* php normalizes month 0 to December of the previous year (like the ISO branch)
	 * but fails a month past 12; a day past 31 fails, while day 0 normalizes in
	 * DtMakeTs. Errors point at the field end. */
	if( mo > 12 ){ return (int)(z - zIn) + 1; }
	if( mo == 0 ){ mo = 12; y--; }
	if( d > 31 ){ return (int)(z - zIn) + 1; }
	/* optional time-of-day suffix, then commit */
	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff,&us);
	if( rcT != 0 ){ return rcT; }
	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);
	*pOff = iOff;
	*pUs = us;
	*pzOut = z;
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
		 && (zEnd - z == n || !SyisAlpha(z[n])) ){
			*pAdv = n;
			return aM[i].mo;
		}
	}
	return 0;
}
/* Match a weekday name at z (full or 3-letter, case-insensitive, word boundary).
 * Returns the day-of-week 0=Sunday..6=Saturday and sets *pAdv, or -1. */
static int DtMatchWeekday(const char *z,const char *zEnd,int *pAdv)
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
		if( zEnd - z >= n && SyStrnicmp(z,aW[i].z,(sxu32)n) == 0
		 && (zEnd - z == n || !SyisAlpha(z[n])) ){
			*pAdv = n;
			return aW[i].dow;
		}
	}
	return -1;
}
/* True if z points at a two-letter English ordinal suffix (st/nd/rd/th). */
static int DtIsOrdinal(const char *z,const char *zEnd)
{
	if( zEnd - z < 2 ){ return 0; }
	return SyStrnicmp(z,"st",2) == 0 || SyStrnicmp(z,"nd",2) == 0
		|| SyStrnicmp(z,"rd",2) == 0 || SyStrnicmp(z,"th",2) == 0;
}
/*
 * Try to read a textual-month date at z, in either order:
 *   MonthName [Day] [Year]   ("Jan 15 2020", "January", "January 2020")
 *   Day MonthName [Year]     ("15 January 2020", "15th Jan")
 * A missing day defaults to 1, a missing year to the base timestamp's year (php).
 * Day may carry an ordinal suffix, fields may be comma-separated, month names are
 * case-insensitive, and an optional time-of-day suffix + trailing UTC/GMT is
 * consumed. Returns 0 (not a month date — caller falls through, *pzOut untouched),
 * 1 on success, or a DtParse error code (out-of-range day).
 */
static int DtTryMonthDate(const char *z,const char *zEnd,const char **pzOut,
	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,sxi64 iBaseTs,int *pUs)
{
	int mo,d = 1,adv,haveDay = 0,haveYear = 0;
	sxi64 y = 0;
	int h = 0,mi = 0,s = 0,us = 0;
	sxi32 iOff = *pOff;
	int rcT;
#define MDSKIPWS() while( z < zEnd && (z[0]==' '||z[0]=='\t'||z[0]==',') ){ z++; }
	if( (mo = DtMatchMonth(z,zEnd,&adv)) != 0 ){
		/* MonthName [Day] [Year]. A 4-digit number here is the YEAR, not the day
		 * ("January 2020" is month+year, day defaults); a 1-2 digit number is the day. */
		z += adv;
		MDSKIPWS();
		if( z < zEnd && SyisDigit(z[0]) ){
			int nrun = 0;
			const char *zp = z;
			while( zp < zEnd && SyisDigit(zp[0]) && nrun < 4 ){ zp++; nrun++; }
			if( nrun < 4 ){
				d = DtRead1or2(z,zEnd,&adv); z += adv;
				if( DtIsOrdinal(z,zEnd) ){ z += 2; }
				haveDay = 1;
				MDSKIPWS();
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
	/* optional year */
	if( z < zEnd && SyisDigit(z[0]) ){
		int ny = 0;
		y = 0;
		while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ y = y*10 + (z[0]-'0'); z++; ny++; }
		if( ny <= 2 ){
			if( y >= 0 && y <= 69 ){ y += 2000; }
			else if( y >= 70 && y <= 99 ){ y += 1900; }
		}
		haveYear = 1;
	}
	/* Default the unspecified fields from the base timestamp. php overlays: a
	 * missing year takes the base year; a missing day is 1 when a year WAS given
	 * ("January 2020" -> the 1st) but the base day when only the month was named
	 * ("January" -> the base day). */
	{
		sxi64 by; int bm,bd;
		DtCivilFromDays(DtFloorDiv(iBaseTs + *pOff,86400),&by,&bm,&bd);
		if( !haveYear ){ y = by; }
		if( !haveDay ){ d = haveYear ? 1 : bd; }
	}
	if( d > 31 ){ return (int)(z - zIn) + 1; }
	/* optional time-of-day suffix */
	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff,&us);
	if( rcT != 0 ){ return rcT; }
	/* optional trailing UTC/GMT zone name (PHL's default zone is already UTC) */
	MDSKIPWS();
	if( (zEnd-z >= 3 && SyStrnicmp(z,"utc",3) == 0 && (zEnd-z==3 || !SyisAlpha(z[3])))
	 || (zEnd-z >= 3 && SyStrnicmp(z,"gmt",3) == 0 && (zEnd-z==3 || !SyisAlpha(z[3]))) ){
		iOff = 0; z += 3;
	}
	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);
	*pOff = iOff;
	*pUs = us;
	*pzOut = z;
	return 1;
#undef MDSKIPWS
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
	sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs)
{
	const char *z = zIn, *zEnd = &zIn[nLen];
	sxi64 iTs = iBaseTs;
	sxi32 iOff = iBaseOff;
	int bOffSet = 0;
	int bAny = 0;
	int iNumRc,iMonRc;
	int uSec = 0;
	*pUs = 0;
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
		/* php accepts a fractional epoch ("@1600000000.5" -> .5s = 500000us) */
		if( z < zEnd && z[0]=='.' && zEnd-z >= 2 && SyisDigit(z[1]) ){
			*pUs = DtReadFraction(&z,zEnd);
		}
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
		{
			int rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,&bOffSet,&uSec);
			if( rcT != 0 ){ return rcT; }
		}
		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);
		bAny = 1;
	}else if( SyisDigit(z[0])
	 && (iNumRc = DtTryNumericDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,&uSec)) != 0 ){
		/* DD-MM-YYYY / DD.MM.YYYY (day first), MM/DD/YYYY (slash, American), and
		 * YYYY/MM/DD (slash, year first) — see DtTryNumericDate. Anything other than
		 * 1 is an error code in DtParse's own convention (positive position / negative
		 * "double time"); propagate it verbatim. */
		if( iNumRc != 1 ){ return iNumRc; }
		bAny = 1;
	}else if( (SyisAlpha(z[0]) || SyisDigit(z[0]))
	 && (iMonRc = DtTryMonthDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,iBaseTs,&uSec)) != 0 ){
		/* MonthName Day Year / Day MonthName Year, in any of php's spellings. As with
		 * DtTryNumericDate, anything other than 1 is an error code to propagate. */
		if( iMonRc != 1 ){ return iMonRc; }
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
		/* Weekday navigation: "[next|last|previous|this] <weekday>" moves to the
		 * midnight of the target weekday. Bare/"this" = the this-week occurrence on
		 * or after the base day; "next"/"last"/"previous" skip a matching base day. */
		{
			const char *zSave = z;
			int dir = 0;         /* 0 = this-week occurrence, 1 = next, -1 = last */
			int adv,dow;
			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; DT_SKIP_WS(); }
			else if( DT_LOWEQ("previous",8) ){ dir = -1; z += 8; DT_SKIP_WS(); }
			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; DT_SKIP_WS(); }
			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; DT_SKIP_WS(); }
			dow = DtMatchWeekday(z,zEnd,&adv);
			if( dow >= 0 ){
				sxi64 days = DtFloorDiv(iTs + iOff,86400);
				int bdow = (int)(((days + 4) % 7 + 7) % 7); /* 1970-01-01 was Thursday */
				sxi64 delta;
				if( dir == 1 ){
					delta = ((dow - bdow) % 7 + 7) % 7;
					if( delta == 0 ){ delta = 7; }
				}else if( dir == -1 ){
					delta = -(((bdow - dow) % 7 + 7) % 7);
					if( delta == 0 ){ delta = -7; }
				}else{
					delta = ((dow - bdow) % 7 + 7) % 7;
				}
				iTs = (days + delta)*86400 - iOff; /* midnight of the target day */
				z += adv;
				bAny = 1;
				continue;
			}
			z = zSave; /* prefix did not introduce a weekday: rewind and try the rest */
		}
		/* "first|last day of (this|next|last month | MonthName [Year])": jump to the
		 * first or last day of a target month. A this/next/last-month target keeps the
		 * base time-of-day; an absolute MonthName [Year] target resets it to midnight
		 * (php). */
		if( DT_LOWEQ("first",5) || DT_LOWEQ("last",4) ){
			const char *zSave = z;
			int bFirst = (SyToLower((unsigned char)z[0]) == 'f');
			z += bFirst ? 5 : 4;
			DT_SKIP_WS();
			if( DT_LOWEQ("day",3) ){
				z += 3;
				DT_SKIP_WS();
				if( DT_LOWEQ("of",2) ){
					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);
					sxi64 yy,tod;
					int mm,dd0,keepTime = 1,ok = 1;
					z += 2;
					DT_SKIP_WS();
					DtCivilFromDays(days0,&yy,&mm,&dd0);
					tod = (iTs + iOff) - days0*86400;
					if( DT_LOWEQ("this",4) ){ z += 4; DT_SKIP_WS();
						if( DT_LOWEQ("month",5) ){ z += 5; }else{ ok = 0; } }
					else if( DT_LOWEQ("next",4) ){ z += 4; DT_SKIP_WS();
						if( DT_LOWEQ("month",5) ){ z += 5; mm++; if(mm>12){ mm=1; yy++; } }else{ ok = 0; } }
					else if( DT_LOWEQ("last",4) ){ z += 4; DT_SKIP_WS();
						if( DT_LOWEQ("month",5) ){ z += 5; mm--; if(mm<1){ mm=12; yy--; } }else{ ok = 0; } }
					else if( z < zEnd ){
						int mo,adv;
						mo = DtMatchMonth(z,zEnd,&adv);
						if( mo == 0 ){ return (int)(z - zIn) + 1; }
						z += adv; DT_SKIP_WS();
						mm = mo; keepTime = 0; tod = 0;
						if( z < zEnd && SyisDigit(z[0]) ){
							int ny = 0; sxi64 yv = 0;
							while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ yv = yv*10 + (z[0]-'0'); z++; ny++; }
							if( ny <= 2 ){ if( yv <= 69 ){ yv += 2000; } else if( yv <= 99 ){ yv += 1900; } }
							yy = yv;
						}
					}
					/* else: "... day of" with nothing after — php defaults to this
					 * month (mm/yy/tod stay the base, keepTime stays 1). */
					if( ok ){
						int dim = (int)(DtDaysFromCivil(yy,mm+1,1) - DtDaysFromCivil(yy,mm,1));
						int day = bFirst ? 1 : dim;
						iTs = DtDaysFromCivil(yy,mm,day)*86400 + (keepTime ? tod : 0) - iOff;
						bAny = 1;
						continue;
					}
				}
			}
			z = zSave; /* not the "first|last day of ..." shape: rewind */
		}
		/* Standalone "this|next|last (month|week)": month shifts by ±1 keeping the
		 * day/time; week moves to the Monday of this/next/last ISO week keeping the
		 * time-of-day (php: weeks start on Monday). */
		{
			const char *zSave = z;
			int dir = 2; /* 2 = no prefix */
			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; }
			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; }
			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; }
			if( dir != 2 ){
				DT_SKIP_WS();
				if( DT_LOWEQ("month",5) ){
					z += 5;
					iTs = DtAddMonths(iTs,iOff,dir);
					bAny = 1;
					continue;
				}
				if( DT_LOWEQ("week",4) ){
					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);
					sxi64 tod = (iTs + iOff) - days0*86400;
					int bdow = (int)(((days0 + 4) % 7 + 7) % 7);
					sxi64 monday = days0 - ((bdow + 6) % 7); /* Monday of the base week */
					z += 4;
					monday += (sxi64)dir * 7;
					iTs = monday*86400 + tod - iOff;
					bAny = 1;
					continue;
				}
			}
			z = zSave;
		}
		/* Trailing time-of-day in a relative sequence ("next thursday 15:00"): set
		 * the clock on the current day. The leading absolute HH:MM branch handles a
		 * time at the START; this handles one AFTER a date/relative token. */
		if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'
		 && SyisDigit(z[3]) && SyisDigit(z[4]) ){
			sxi64 days = DtFloorDiv(iTs + iOff,86400);
			int hh = (z[0]-'0')*10 + (z[1]-'0');
			int mm = (z[3]-'0')*10 + (z[4]-'0');
			int ss = 0;
			if( hh > 24 ){ return (int)(z - zIn) + 1; }
			if( mm > 59 ){ return (int)(&z[4] - zIn) + 1; }
			z += 5;
			if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){
				ss = (z[1]-'0')*10 + (z[2]-'0');
				if( ss > 59 ){ return (int)(&z[2] - zIn) + 1; }
				z += 3;
			}
			iTs = days*86400 + (sxi64)hh*3600 + (sxi64)mm*60 + ss - iOff;
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
	*pUs = uSec;
	return 0;
#undef DT_SKIP_WS
#undef DT_LOWEQ
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
	/* Negative encoding: php's "Double time specification" reason */
	int bDouble = iErrPos < 0;
	int iPos = (bDouble ? -iErrPos : iErrPos) - 1;
	char cAt = (iPos < nLen) ? zIn[iPos] : ' ';
	*piPos = iPos;
	*pcAt = cAt;
	/* php appends a reason: an alphabetic token is assumed to be a timezone
	 * lookup miss, anything else an unexpected character. */
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
	sxi64 y,m,d,h,i,s,nDays;
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
static void DtCivilDiff(sxi64 iTs1,sxi32 iOff,sxi64 iTs2,dt_diff *pOut)
{
	sxi64 iA,iB,iLa,iLb,daysA,daysB,yA,yB;
	int moA,dA,moB,dB,bInvert;
	sxi64 sA,sB,y,m,d,h,i,s;
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
	pOut->nDays = (iB - iA) / 86400;
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
static void DtFfDiag(phl_dt_lasterr *pDiag,int nErr,int nErrKept,const int *aErrPos,
	const char **azErr,int nWarn,const int *aWarnPos,const char **azWarn)
{
	int k;
	pDiag->bSet = (sxu8)(nErr > 0 || nWarn > 0);
	pDiag->nErr = nErr;
	pDiag->nErrKept = nErrKept;
	for( k = 0 ; k < nErrKept ; k++ ){
		pDiag->aErrPos[k] = aErrPos[k];
		pDiag->azErr[k] = azErr[k];
	}
	pDiag->nWarn = nWarn;
	pDiag->nWarnKept = nWarn;
	for( k = 0 ; k < nWarn ; k++ ){
		pDiag->aWarnPos[k] = aWarnPos[k];
		pDiag->azWarn[k] = azWarn[k];
	}
}
typedef struct dt_ff_res dt_ff_res;
struct dt_ff_res
{
	sxi64 iTs;
	sxi32 iOff;
	int iOffKind;
	char zName[16];
	int uSec;
	int bHasUs;
	phl_dt_lasterr sDiag;
};
static int DtFromFormat(const char *zFmt,int nFmt,const char *zIn,int nIn,
	sxi64 iNow,sxi32 iDefOff,dt_ff_res *pOut)
{
	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};
	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",
		"thursday","friday","saturday"};
	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",
		"aug","sep","oct","nov","dec"};
	static const char *azMonFull[] = {"january","february","march","april",
		"may","june","july","august","september","october","november","december"};
	const char *zEnd,*zInEnd,*z;
	sxi64 v;
	/* -1 == unset */
	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;
	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;
	int uSecFF = 0,bHasUs = 0;
	int iOffKind = 0;
	sxi32 iOffVal = 0;
	char zName[16];
	const char *zErr = 0;
	const char *aWarnMsg[PH7_DT_MAX_WARN];
	int aWarnPos[PH7_DT_MAX_WARN];
	int nWarn = 0,bAborted = 0;
	const char *aErrMsg[PH7_DT_MAX_ERR];
	int aErrPos[PH7_DT_MAX_ERR];
	int nErr = 0,nErrKept = 0;
	SyZero(pOut,sizeof(*pOut));
	zEnd = &zFmt[nFmt];
	zInEnd = &zIn[nIn];
	z = zIn;
	zName[0] = 0;
#define DT_FF_LOGERR(iPos,zMsg) \
	{ int _p = (iPos),_k,_f = -1; \
	  nErr++; \
	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \
	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \
	  else if( nErrKept < PH7_DT_MAX_ERR ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }
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
		case 'u':{
			/* Microseconds: the digits parsed are right-padded to 6 (".5" -> 500000). */
			const char *zStart = z;
			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){
				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");
				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){
					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");
				}else{
					zStart = z; /* HuntDigits repositioned; treat as freshly read */
				}
			}
			{
				int nd = (int)(z - zStart);
				while( nd > 0 && nd < 6 ){ v *= 10; nd++; }
				uSecFF = (int)v; bHasUs = 1;
			}
			break;
				 }
		case 'v':{
			const char *zStart = z;
			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){
				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");
				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){
					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");
				}else{
					zStart = z;
				}
			}
			{
				int nd = (int)(z - zStart);
				while( nd > 0 && nd < 3 ){ v *= 10; nd++; }
				uSecFF = (int)v * 1000; bHasUs = 1; /* ms -> us */
			}
			break;
				 }
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
		/* The diagnostics are the caller's on this path too: php reports the
		 * warnings of a parse that ALSO failed, which the old string encoding had
		 * no room for. */
		DtFfDiag(&pOut->sDiag,nErr,nErrKept,aErrPos,aErrMsg,nWarn,aWarnPos,aWarnMsg);
		return -1;
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
		if( nWarn < PH7_DT_MAX_WARN ){
			aWarnPos[nWarn] = nIn;
			aWarnMsg[nWarn] = "The parsed date was invalid";
			nWarn++;
		}
	}
	if( h > 24 || mi > 59 || s > 59 ){
		if( nWarn < PH7_DT_MAX_WARN ){
			aWarnPos[nWarn] = nIn;
			aWarnMsg[nWarn] = "The parsed time was invalid";
			nWarn++;
		}
	}
	{
		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;
		if( bHasU ){
			pOut->iTs = uVal;
			iUseOff = 0;
			iOffKind = 1;
		}else{
			pOut->iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);
		}
		pOut->iOff = iUseOff;
		pOut->iOffKind = iOffKind;
		SyMemcpy(zName,pOut->zName,sizeof(pOut->zName));
		pOut->zName[sizeof(pOut->zName)-1] = 0;
		pOut->uSec = uSecFF;
		pOut->bHasUs = bHasUs;
	}
	DtFfDiag(&pOut->sDiag,nErr,nErrKept,aErrPos,aErrMsg,nWarn,aWarnPos,aWarnMsg);
	return 0;
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
/* One date object's state, as the bodies below pass it around. */
typedef struct dt_state dt_state;
struct dt_state
{
	sxi64 iTs;
	sxi32 iOff;
	int uSec;
	const char *zName;   /* borrowed from the instance's own slot */
	int nName;
};
/* Fetch a declared instance slot by name (never a static/constant one). */
static ph7_value * DtAttr(ph7_class_instance *pObj,const char *zName)
{
	SyString sName;
	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));
	return PH7_ClassInstanceFetchAttr(pObj,&sName);
}
/* Read an int slot without converting it: these slots only ever hold integers,
 * and ph7_value_to_int64() would convert the attribute IN PLACE. */
static sxi64 DtAttrInt(ph7_class_instance *pObj,const char *zName)
{
	ph7_value *pVal = DtAttr(pObj,zName);
	if( pVal == 0 ){
		return 0;
	}
	if( pVal->iFlags & MEMOBJ_INT ){
		return pVal->x.iVal;
	}
	return 0;
}
static void DtAttrStr(ph7_class_instance *pObj,const char *zName,const char **pzOut,int *pnOut)
{
	ph7_value *pVal = DtAttr(pObj,zName);
	*pzOut = "";
	*pnOut = 0;
	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){
		*pzOut = (const char *)SyBlobData(&pVal->sBlob);
		*pnOut = (int)SyBlobLength(&pVal->sBlob);
	}
}
static void DtSetInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal)
{
	ph7_value *pSlot = DtAttr(pObj,zName);
	ph7_value sVal;
	if( pSlot == 0 ){
		return;
	}
	PH7_MemObjInitFromInt(&(*pVm),&sVal,iVal);
	PH7_MemObjStore(&sVal,pSlot);
	PH7_MemObjRelease(&sVal);
}
static void DtSetStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,const char *zVal,int nVal)
{
	ph7_value *pSlot = DtAttr(pObj,zName);
	ph7_value sVal;
	SyString sStr;
	if( pSlot == 0 ){
		return;
	}
	SyStringInitFromBuf(&sStr,zVal,nVal);
	PH7_MemObjInitFromString(&(*pVm),&sVal,&sStr);
	PH7_MemObjStore(&sVal,pSlot);
	PH7_MemObjRelease(&sVal);
}
/* php's name for a fixed offset: "+HH:MM" (and "+00:00" for zero, never "-00:00"). */
static int DtOffName(char *zBuf,sxu32 nBuf,sxi32 iOff)
{
	sxi32 a = iOff < 0 ? -iOff : iOff;
	return (int)SyBufferFormat(zBuf,nBuf,"%c%02d:%02d",
		iOff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));
}
static void DtLoad(ph7_class_instance *pObj,dt_state *pOut)
{
	pOut->iTs  = DtAttrInt(pObj,DT_TS);
	pOut->iOff = (sxi32)DtAttrInt(pObj,DT_OFF);
	pOut->uSec = (int)DtAttrInt(pObj,DT_US);
	DtAttrStr(pObj,DT_NAME,&pOut->zName,&pOut->nName);
}
static void DtStore(ph7_vm *pVm,ph7_class_instance *pObj,const dt_state *pIn)
{
	DtSetInt(pVm,pObj,DT_TS,pIn->iTs);
	DtSetInt(pVm,pObj,DT_OFF,pIn->iOff);
	DtSetInt(pVm,pObj,DT_US,pIn->uSec);
	DtSetStr(pVm,pObj,DT_NAME,pIn->zName,pIn->nName);
}
/* The receiver of a native method, or NULL when the call has no object (which the
 * dispatcher only allows for a static one). */
static ph7_class_instance * DtThis(ph7_context *pCtx)
{
	return PH7_ContextThis(pCtx);
}
/* Hand an instance back as the call's result, dropping the reference
 * PH7_NewClassInstance/PH7_CloneClassInstance handed us. */
static void DtResultObject(ph7_context *pCtx,ph7_class_instance *pObj)
{
	ph7_value sRes;
	PH7_MemObjInit(pCtx->pVm,&sRes);
	sRes.x.pOther = pObj;
	sRes.iFlags = MEMOBJ_OBJ;
	ph7_result_value(pCtx,&sRes);   /* takes its own reference */
	PH7_ClassInstanceUnref(pObj);
}
static ph7_class * DtClass(ph7_vm *pVm,const char *zName)
{
	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);
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
		DtResultObject(pCtx,pTarget);
	}else{
		ph7_result_value(pCtx,PH7_ContextThisValue(pCtx));
	}
}
/* Read a DateTimeZone argument's two slots. php's ext/date reads its own internal
 * timezone struct here, so an overridden getName()/getOffset() is ignored by both
 * engines. Answers 0 when the value is not a DateTimeZone at all. */
static int DtZoneOf(ph7_value *pArg,sxi32 *piOff,const char **pzName,int *pnName)
{
	ph7_class_instance *pObj;
	if( pArg == 0 || (pArg->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pObj = (ph7_class_instance *)pArg->x.pOther;
	if( DtAttr(pObj,DTZ_NAME) == 0 ){
		return 0;
	}
	*piOff = (sxi32)DtAttrInt(pObj,DTZ_OFF);
	DtAttrStr(pObj,DTZ_NAME,pzName,pnName);
	return 1;
}
/*
 * Record one parse's diagnostics as getLastErrors()'s answer.
 *
 * php resets the record on EVERY constructor and createFromFormat() call — a clean
 * parse answers `false` again — and a failing constructor publishes its reason as a
 * one-entry error map before it throws.
 */
static void DtLastErrClear(ph7_vm *pVm)
{
	SyZero(&pVm->sDtLastErr,sizeof(pVm->sDtLastErr));
}
static void DtLastErrOne(ph7_vm *pVm,int iPos,const char *zMsg)
{
	DtLastErrClear(&(*pVm));
	pVm->sDtLastErr.bSet = 1;
	pVm->sDtLastErr.nErr = 1;
	pVm->sDtLastErr.nErrKept = 1;
	pVm->sDtLastErr.aErrPos[0] = iPos;
	pVm->sDtLastErr.azErr[0] = zMsg;
}
/*
 * Parse $datetime into a date object's state, php's constructor rules: an explicit
 * offset in the string wins over the $timezone argument, a literal "Z" keeps its
 * own name, and everything else takes the argument's (or the default) zone.
 * Returns 0 on success; on failure the caller throws with the reason and position
 * this reports.
 */
static int DtInitState(ph7_context *pCtx,const char *zIn,int nIn,sxi32 iZoneOff,
	const char *zZoneName,int nZoneName,dt_state *pOut,char *zNameBuf,sxu32 nNameBuf,
	const char **pzErr,int *piPos,char *pcAt)
{
	sxi64 iTs = 0;
	sxi32 iOff = 0;
	int bOffSet = 0,uSec = 0,iErrPos;
	iErrPos = DtParse(zIn,nIn,(sxi64)time(0),iZoneOff,&iTs,&iOff,&bOffSet,&uSec);
	if( iErrPos != 0 ){
		*pzErr = DtParseErr(zIn,nIn,iErrPos,piPos,pcAt);
		return -1;
	}
	pOut->iTs = iTs;
	pOut->uSec = uSec;
	if( bOffSet ){
		pOut->iOff = iOff;
		if( bOffSet == 2 ){
			pOut->zName = "Z";
			pOut->nName = 1;
		}else{
			pOut->nName = DtOffName(zNameBuf,nNameBuf,iOff);
			pOut->zName = zNameBuf;
		}
	}else{
		pOut->iOff = iZoneOff;
		pOut->zName = zZoneName;
		pOut->nName = nZoneName;
	}
	SXUNUSED(pCtx);
	return 0;
}
/* DateTimeZone::__construct(string $timezone) */
static int vm_builtin_DateTimeZone_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	const char *zTz;
	int nTz,iOff = 0;
	char zName[16];
	int nName;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zTz = ph7_value_to_string(apArg[0],&nTz);
	if( (nTz == 3 && SyStrnicmp(zTz,"UTC",3) == 0) || (nTz == 3 && SyStrnicmp(zTz,"GMT",3) == 0)
	 || (nTz == 1 && zTz[0] == 'Z') ){
		/* The three names PHL has no database for but does understand. php keeps
		 * the spelling the caller used for Z and folds the other two to upper. */
		DtSetInt(pCtx->pVm,pThis,DTZ_OFF,0);
		DtSetStr(pCtx->pVm,pThis,DTZ_NAME,nTz == 1 ? "Z" : (zTz[0] == 'u' || zTz[0] == 'U') ? "UTC" : "GMT",nTz);
		return PH7_OK;
	}
	/* [+-]HH:?MM, php's only other accepted spelling without a tz database. */
	if( (nTz == 6 || nTz == 5) && (zTz[0] == '+' || zTz[0] == '-')
	 && SyisDigit(zTz[1]) && SyisDigit(zTz[2])
	 && (nTz == 5 ? (SyisDigit(zTz[3]) && SyisDigit(zTz[4]))
	              : (zTz[3] == ':' && SyisDigit(zTz[4]) && SyisDigit(zTz[5]))) ){
		int h = (zTz[1] - '0') * 10 + (zTz[2] - '0');
		int m = nTz == 5 ? (zTz[3] - '0') * 10 + (zTz[4] - '0')
		                 : (zTz[4] - '0') * 10 + (zTz[5] - '0');
		iOff = h * 3600 + m * 60;
		if( zTz[0] == '-' ){
			iOff = -iOff;
		}
		nName = DtOffName(zName,sizeof(zName),iOff);
		DtSetInt(pCtx->pVm,pThis,DTZ_OFF,iOff);
		DtSetStr(pCtx->pVm,pThis,DTZ_NAME,zName,nName);
		return PH7_OK;
	}
	return PH7_VmThrowException(pCtx,"DateInvalidTimeZoneException",
		"DateTimeZone::__construct(): Unknown or bad timezone (%.*s)",nTz,zTz);
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
	DtAttrStr(pThis,DTZ_NAME,&zName,&nName);
	ph7_result_string(pCtx,zName,nName);
	return PH7_OK;
}
/* DateTimeZone::getOffset(DateTimeInterface $datetime) */
static int vm_builtin_DateTimeZone_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	/* Fixed-offset zones only, so the instant does not change the answer. */
	ph7_result_int64(pCtx,DtAttrInt(pThis,DTZ_OFF));
	return PH7_OK;
}
/* DateTime::__construct(string $datetime = 'now', ?DateTimeZone $timezone = null) */
static int vm_builtin_DateTime_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = DtThis(pCtx);
	const char *zIn = "now",*zZone;
	int nIn = 3,nZone;
	sxi32 iZoneOff = 0;
	dt_state sState;
	char zNameBuf[16];
	const char *zErr;
	int iPos;
	char cAt;
	if( pThis == 0 ){
		return PH7_OK;
	}
	zZone = pVm->zDefTz;
	nZone = (int)pVm->nDefTz;
	if( nArg > 0 ){
		zIn = ph7_value_to_string(apArg[0],&nIn);
	}
	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){
		DtZoneOf(apArg[1],&iZoneOff,&zZone,&nZone);
	}
	if( DtInitState(pCtx,zIn,nIn,iZoneOff,zZone,nZone,&sState,zNameBuf,sizeof(zNameBuf),
		&zErr,&iPos,&cAt) != 0 ){
		/* php publishes the failure through getLastErrors() as well as throwing. */
		DtLastErrOne(pVm,iPos,zErr);
		return PH7_VmThrowException(pCtx,"DateMalformedStringException",
			"Failed to parse time string (%.*s) at position %d (%c): %s",
			nIn,zIn,iPos,cAt,zErr);
	}
	DtLastErrClear(pVm);
	DtStore(pVm,pThis,&sState);
	return PH7_OK;
}
/* DateTime::format(string $format) */
static int vm_builtin_DateTime_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	dt_state sState;
	Sytm sTm;
	char zZone[64];
	const char *zFmt;
	int nFmt,nName;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	DtLoad(pThis,&sState);
	zFmt = ph7_value_to_string(apArg[0],&nFmt);
	nName = sState.nName;
	if( nName >= (int)sizeof(zZone) ){
		nName = (int)sizeof(zZone) - 1;
	}
	SyMemcpy(sState.zName,zZone,(sxu32)nName);
	zZone[nName] = 0;
	DtFillSytm(sState.iTs,sState.iOff,zZone,&sTm);
	DateFormat(pCtx,zFmt,nFmt,&sTm,sState.uSec);
	return PH7_OK;
}
/* DateTime::getTimestamp() / getMicrosecond() / getOffset() */
static int vm_builtin_DateTime_getTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		ph7_result_int64(pCtx,DtAttrInt(pThis,DT_TS));
	}
	return PH7_OK;
}
static int vm_builtin_DateTime_getMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		ph7_result_int64(pCtx,DtAttrInt(pThis,DT_US));
	}
	return PH7_OK;
}
static int vm_builtin_DateTime_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		ph7_result_int64(pCtx,DtAttrInt(pThis,DT_OFF));
	}
	return PH7_OK;
}
/* DateTime::getTimezone() — the zone is built from the stored name and offset, so
 * an identifier PHL stored but cannot re-parse still round-trips. */
static int vm_builtin_DateTime_getTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class *pZoneClass = DtClass(pVm,"DateTimeZone");
	ph7_class_instance *pZone;
	const char *zName;
	int nName;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || pZoneClass == 0 ){
		return PH7_OK;
	}
	pZone = PH7_NewClassInstance(pVm,pZoneClass);
	if( pZone == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	DtAttrStr(pThis,DT_NAME,&zName,&nName);
	DtSetInt(pVm,pZone,DTZ_OFF,DtAttrInt(pThis,DT_OFF));
	DtSetStr(pVm,pZone,DTZ_NAME,zName,nName);
	DtResultObject(pCtx,pZone);
	return PH7_OK;
}
/* DateTime::diff(DateTimeInterface $targetObject, bool $absolute = false) */
static int vm_builtin_DateTime_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class *pIvClass = DtClass(pVm,"DateInterval");
	ph7_class_instance *pTarget,*pIv;
	dt_diff sDiff;
	int bAbsolute = 0;
	if( pThis == 0 || pIvClass == 0 || nArg < 1 ){
		return PH7_OK;
	}
	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_OK;
	}
	pTarget = (ph7_class_instance *)apArg[0]->x.pOther;
	if( nArg > 1 ){
		ph7_value sTmp;
		PH7_MemObjInit(pVm,&sTmp);
		PH7_MemObjStore(apArg[1],&sTmp);
		PH7_MemObjToBool(&sTmp);
		bAbsolute = sTmp.x.iVal != 0;
		PH7_MemObjRelease(&sTmp);
	}
	DtCivilDiff(DtAttrInt(pThis,DT_TS),(sxi32)DtAttrInt(pThis,DT_OFF),
		DtAttrInt(pTarget,DT_TS),&sDiff);
	pIv = PH7_NewClassInstance(pVm,pIvClass);
	if( pIv == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	DtSetInt(pVm,pIv,"y",sDiff.y);
	DtSetInt(pVm,pIv,"m",sDiff.m);
	DtSetInt(pVm,pIv,"d",sDiff.d);
	DtSetInt(pVm,pIv,"h",sDiff.h);
	DtSetInt(pVm,pIv,"i",sDiff.i);
	DtSetInt(pVm,pIv,"s",sDiff.s);
	DtSetInt(pVm,pIv,"days",sDiff.nDays);
	DtSetInt(pVm,pIv,"invert",bAbsolute ? 0 : sDiff.bInvert);
	DtResultObject(pCtx,pIv);
	return PH7_OK;
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
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zMod = ph7_value_to_string(apArg[0],&nMod);
	iErrPos = DtParse(zMod,nMod,DtAttrInt(pThis,DT_TS),(sxi32)DtAttrInt(pThis,DT_OFF),
		&iTs,&iOff,&bOffSet,&uSec);
	if( iErrPos != 0 ){
		int bImm = DtIsImmutable(pVm,pThis);
		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);
		return PH7_VmThrowException(pCtx,"DateMalformedStringException",
			"%s::modify(): Failed to parse time string (%.*s) at position %d (%c): %s",
			bImm ? "DateTimeImmutable" : "DateTime",nMod,zMod,iPos,cAt,zErr);
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	DtSetInt(pVm,pTarget,DT_TS,iTs);
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
	DtSetInt(pCtx->pVm,pTarget,DT_TS,ph7_value_to_int64(apArg[0]));
	DtSetInt(pCtx->pVm,pTarget,DT_US,0);
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* DateTime::setMicrosecond(int $microsecond) */
static int vm_builtin_DateTime_setMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	int bCopy = 0;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	DtSetInt(pCtx->pVm,pTarget,DT_US,ph7_value_to_int64(apArg[0]));
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* DateTime::setTimezone(DateTimeZone $timezone) */
static int vm_builtin_DateTime_setTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	const char *zName = "UTC";
	int nName = 3,bCopy = 0;
	sxi32 iOff = 0;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	if( !DtZoneOf(apArg[0],&iOff,&zName,&nName) ){
		return PH7_OK;
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	DtSetInt(pCtx->pVm,pTarget,DT_OFF,iOff);
	DtSetStr(pCtx->pVm,pTarget,DT_NAME,zName,nName);
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* DateTime::setDate(int $year, int $month, int $day) — the time of day is kept */
static int vm_builtin_DateTime_setDate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	dt_state sState;
	sxi64 iLocal,iDays,iSecs;
	int bCopy = 0;
	if( pThis == 0 || nArg < 3 ){
		return PH7_OK;
	}
	DtLoad(pThis,&sState);
	iLocal = sState.iTs + sState.iOff;
	iDays = DtFloorDiv(iLocal,86400);
	iSecs = iLocal - iDays*86400;
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	DtSetInt(pCtx->pVm,pTarget,DT_TS,
		DtMakeTs(ph7_value_to_int64(apArg[0]),ph7_value_to_int(apArg[1]),
			ph7_value_to_int(apArg[2]),(int)(iSecs / 3600),(int)((iSecs / 60) % 60),
			(int)(iSecs % 60),sState.iOff));
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* DateTime::setTime(int $hour, int $minute, int $second = 0, int $microsecond = 0) */
static int vm_builtin_DateTime_setTime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget;
	dt_state sState;
	sxi64 iLocal,iDays,y;
	int mo,d,bCopy = 0;
	if( pThis == 0 || nArg < 2 ){
		return PH7_OK;
	}
	DtLoad(pThis,&sState);
	iLocal = sState.iTs + sState.iOff;
	iDays = DtFloorDiv(iLocal,86400);
	DtCivilFromDays(iDays,&y,&mo,&d);
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	DtSetInt(pCtx->pVm,pTarget,DT_TS,
		DtMakeTs(y,mo,d,ph7_value_to_int(apArg[0]),ph7_value_to_int(apArg[1]),
			nArg > 2 ? ph7_value_to_int(apArg[2]) : 0,sState.iOff));
	DtSetInt(pCtx->pVm,pTarget,DT_US,nArg > 3 ? ph7_value_to_int64(apArg[3]) : 0);
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
	DtSetInt(pCtx->pVm,pTarget,DT_TS,
		DtIsoDate(DtAttrInt(pThis,DT_TS),(sxi32)DtAttrInt(pThis,DT_OFF),
			ph7_value_to_int64(apArg[0]),ph7_value_to_int64(apArg[1]),
			nArg > 2 ? ph7_value_to_int64(apArg[2]) : 1));
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* add()/sub(): one body, the sign is the difference (and a DateInterval carrying
 * `invert` flips it, exactly as the chunk's __dtAddTs did). */
static int DtAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	ph7_class_instance *pTarget,*pIv;
	int bCopy = 0;
	if( pThis == 0 || nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_OK;
	}
	pIv = (ph7_class_instance *)apArg[0]->x.pOther;
	if( DtAttrInt(pIv,"invert") ){
		iSign = -iSign;
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	DtSetInt(pCtx->pVm,pTarget,DT_TS,
		DtCivilAdd(DtAttrInt(pThis,DT_TS),(sxi32)DtAttrInt(pThis,DT_OFF),
			DtAttrInt(pIv,"y"),DtAttrInt(pIv,"m"),DtAttrInt(pIv,"d"),
			DtAttrInt(pIv,"h"),DtAttrInt(pIv,"i"),DtAttrInt(pIv,"s"),iSign));
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
	const phl_dt_lasterr *pErr = &pVm->sDtLastErr;
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
	for( k = 0 ; k < pErr->nErrKept ; k++ ){
		ph7_value_string(pVal,pErr->azErr[k],-1);
		ph7_array_add_intkey_elem(pErrs,pErr->aErrPos[k],pVal);
		ph7_value_reset_string_cursor(pVal);
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
	char zNameBuf[16];
	const char *zZone;
	int nZone;
	sxi32 iZoneOff = 0;
	const char *zFmt,*zIn;
	int nFmt,nIn;
	if( pClass == 0 || nArg < 2 ){
		return PH7_OK;
	}
	zFmt = ph7_value_to_string(apArg[0],&nFmt);
	zIn  = ph7_value_to_string(apArg[1],&nIn);
	zZone = pVm->zDefTz;
	nZone = (int)pVm->nDefTz;
	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){
		DtZoneOf(apArg[2],&iZoneOff,&zZone,&nZone);
	}
	if( DtFromFormat(zFmt,nFmt,zIn,nIn,(sxi64)time(0),iZoneOff,&sRes) != 0 ){
		pVm->sDtLastErr = sRes.sDiag;
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pVm->sDtLastErr = sRes.sDiag;
	sState.iTs = sRes.iTs;
	sState.uSec = sRes.bHasUs ? sRes.uSec : 0;
	switch( sRes.iOffKind ){
		case 0:
			sState.iOff = iZoneOff;
			sState.zName = zZone;
			sState.nName = nZone;
			break;
		case 2:
			sState.iOff = 0;
			sState.zName = "Z";
			sState.nName = 1;
			break;
		case 3:
			sState.iOff = sRes.iOff;
			sState.zName = sRes.zName;
			sState.nName = (int)SyStrlen(sRes.zName);
			break;
		default:
			sState.iOff = sRes.iOff;
			sState.nName = DtOffName(zNameBuf,sizeof(zNameBuf),sRes.iOff);
			sState.zName = zNameBuf;
			break;
	}
	pObj = PH7_NewClassInstance(pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	DtStore(pVm,pObj,&sState);
	DtResultObject(pCtx,pObj);
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
	if( pClass == 0 || nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_OK;
	}
	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;
	DtLoad(pSrc,&sState);
	pObj = PH7_NewClassInstance(pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	DtStore(pVm,pObj,&sState);
	DtResultObject(pCtx,pObj);
	return PH7_OK;
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
	sxi32 iOff = 0;
	int bOffSet = 0,uSec = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	if( nIn < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iBase = (nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0)
		? ph7_value_to_int64(apArg[1]) : (sxi64)time(0);
	if( DtParse(zIn,nIn,iBase,0,&iTs,&iOff,&bOffSet,&uSec) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,iTs);
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
"function date_create($datetime = 'now', $timezone = null){"
" try { return new DateTime($datetime, $timezone); } catch (Exception $e) { return false; }"
"}"
"function date_create_immutable($datetime = 'now', $timezone = null){"
" try { return new DateTimeImmutable($datetime, $timezone); } catch (Exception $e) { return false; }"
"}"
/* Procedural aliases of the createFromFormat statics: same (format, datetime,
 * ?timezone) order, returning false on failure like php. */
"function date_create_from_format($format, $datetime, $timezone = null){"
" return DateTime::createFromFormat($format, $datetime, $timezone);"
"}"
"function date_create_immutable_from_format($format, $datetime, $timezone = null){"
" return DateTimeImmutable::createFromFormat($format, $datetime, $timezone);"
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
 * The four private slots a date object keeps its state in. Both classes declare
 * them: `trait __DtCoreT` had no native equivalent, and replaying the table is
 * exactly what `use __DtCoreT` did.
 */
#define DT_NATIVE_STATE_PROPS \
	{ DT_TS,   PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 } }, \
	{ DT_OFF,  PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 } }, \
	{ DT_NAME, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 } }, \
	{ DT_US,   PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 } }
/*
 * The methods DateTime and DateTimeImmutable share -- the whole of the old trait
 * plus the mutators, whose one difference (write $this, or write a clone) the
 * bodies decide from the receiver's class. php's own signatures: they are the
 * single source of truth for arity, coercion and Reflection here, so the casts the
 * chunk wrote by hand (`(string)$format`, `(int)$timestamp`) are declared types now
 * and the methods reject what php rejects.
 */
#define DT_NATIVE_SHARED_METHODS \
	{ "__construct",     PH7_MOD_PUBLIC, "string $datetime = 'now', ?DateTimeZone $timezone = null", "", \
	  vm_builtin_DateTime_construct }, \
	{ "format",          PH7_MOD_PUBLIC, "string $format", "string", vm_builtin_DateTime_format }, \
	{ "getTimestamp",    PH7_MOD_PUBLIC, "", "int", vm_builtin_DateTime_getTimestamp }, \
	{ "getMicrosecond",  PH7_MOD_PUBLIC, "", "int", vm_builtin_DateTime_getMicrosecond }, \
	{ "getOffset",       PH7_MOD_PUBLIC, "", "int", vm_builtin_DateTime_getOffset }, \
	{ "getTimezone",     PH7_MOD_PUBLIC, "", "DateTimeZone", vm_builtin_DateTime_getTimezone }, \
	{ "diff",            PH7_MOD_PUBLIC, "DateTimeInterface $targetObject, bool $absolute = false", \
	  "DateInterval", vm_builtin_DateTime_diff }, \
	{ "modify",          PH7_MOD_PUBLIC, "string $modifier", "static", vm_builtin_DateTime_modify }, \
	{ "setTimestamp",    PH7_MOD_PUBLIC, "int $timestamp", "static", vm_builtin_DateTime_setTimestamp }, \
	{ "setMicrosecond",  PH7_MOD_PUBLIC, "int $microsecond", "static", vm_builtin_DateTime_setMicrosecond }, \
	{ "setTimezone",     PH7_MOD_PUBLIC, "DateTimeZone $timezone", "static", vm_builtin_DateTime_setTimezone }, \
	{ "setDate",         PH7_MOD_PUBLIC, "int $year, int $month, int $day", "static", \
	  vm_builtin_DateTime_setDate }, \
	{ "setTime",         PH7_MOD_PUBLIC, \
	  "int $hour, int $minute, int $second = 0, int $microsecond = 0", "static", \
	  vm_builtin_DateTime_setTime }, \
	{ "setISODate",      PH7_MOD_PUBLIC, "int $year, int $week, int $dayOfWeek = 1", "static", \
	  vm_builtin_DateTime_setISODate }, \
	{ "add",             PH7_MOD_PUBLIC, "DateInterval $interval", "static", vm_builtin_DateTime_add }, \
	{ "sub",             PH7_MOD_PUBLIC, "DateInterval $interval", "static", vm_builtin_DateTime_sub }, \
	{ "getLastErrors",   PH7_MOD_PUBLIC|PH7_MOD_STATIC, "", "array|false", \
	  vm_builtin_DateTime_getLastErrors }
/*
 * Install the DateTime family: the chunk first (its exceptions, DateTimeInterface
 * and DateInterval are what the native classes throw, implement and build), then
 * DateTimeZone / DateTime / DateTimeImmutable, which are C.
 *
 * Called from PH7_VmInit inside the bCompilingBuiltin window, after the Reflection
 * install (Exception must exist). `implements DateTimeInterface` rides the spec
 * because that interface declares CONSTANTS only -- PH7_ClassImplement's abstract-
 * stub rule needs interface methods to bite.
 */
PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)
{
	static const PH7_NativePropDef aZoneProp[] = {
		{ DTZ_OFF,  PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 } },
		{ DTZ_NAME, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 } },
	};
	static const PH7_NativeMethodDef aZoneMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $timezone", "", vm_builtin_DateTimeZone_construct },
		{ "getName",     PH7_MOD_PUBLIC, "", "string", vm_builtin_DateTimeZone_getName },
		{ "getOffset",   PH7_MOD_PUBLIC, "DateTimeInterface $datetime", "int",
		  vm_builtin_DateTimeZone_getOffset },
	};
	static const PH7_NativePropDef aDtProp[] = { DT_NATIVE_STATE_PROPS };
	static const PH7_NativeMethodDef aDtMethod[] = {
		DT_NATIVE_SHARED_METHODS,
		{ "createFromFormat",    PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "static|false",
		  vm_builtin_DateTime_createFromFormat },
		{ "createFromImmutable", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTimeImmutable $object", "static",
		  vm_builtin_DateTime_copyOf },
		{ "createFromInterface", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTimeInterface $object", "static",
		  vm_builtin_DateTime_copyOf },
	};
	static const PH7_NativeMethodDef aImmMethod[] = {
		DT_NATIVE_SHARED_METHODS,
		{ "createFromFormat",    PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "static|false",
		  vm_builtin_DateTimeImmutable_createFromFormat },
		{ "createFromMutable",   PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTime $object", "static",
		  vm_builtin_DateTimeImmutable_copyOf },
		{ "createFromInterface", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTimeInterface $object", "static",
		  vm_builtin_DateTimeImmutable_copyOf },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "DateTimeZone", 0, 0, 0,
		  aZoneMethod, SX_ARRAYSIZE(aZoneMethod), 0, 0, aZoneProp, SX_ARRAYSIZE(aZoneProp) },
		{ "DateTime", 0, "DateTimeInterface", 0,
		  aDtMethod, SX_ARRAYSIZE(aDtMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp) },
		{ "DateTimeImmutable", 0, "DateTimeInterface", 0,
		  aImmMethod, SX_ARRAYSIZE(aImmMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp) },
	};
	sxi32 rc;
	/* php's date.timezone default */
	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));
	pVm->nDefTz = sizeof("UTC") - 1;
	DtLastErrClear(&(*pVm));
	/* strtotime() was a prelude function over two of the retired thunks; as a C
	 * builtin it owes aBuiltinSig[] a row (vm_arg_check.c) for its parameters. */
	ph7_create_function(&(*pVm),"strtotime",vm_builtin_strtotime,0);
	rc = PH7_VmEvalBuiltinChunk(&(*pVm),zDateTimeLib,sizeof(zDateTimeLib)-1);
	if( rc != SXRET_OK ){
		return rc;
	}
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
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
