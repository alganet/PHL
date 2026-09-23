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
			if( z[0]=='+' || z[0]=='-' ){
				neg = (z[0]=='-');
				z++;
				/* php's lexer takes the sign as its own token, so whitespace may
				 * follow it: "1 year + 3 months" is a relative sequence there and
				 * was a parse FAILURE here. */
				DT_SKIP_WS();
			}
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
/* php's name for a fixed offset: "+HH:MM" (and "+00:00" for zero, never "-00:00"). */
static int DtOffName(char *zBuf,sxu32 nBuf,sxi32 iOff)
{
	sxi32 a = iOff < 0 ? -iOff : iOff;
	return (int)SyBufferFormat(zBuf,nBuf,"%c%02d:%02d",
		iOff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));
}
static void DtLoad(ph7_class_instance *pObj,dt_state *pOut)
{
	pOut->iTs  = PH7_NativeAttrInt(pObj,DT_TS);
	pOut->iOff = (sxi32)PH7_NativeAttrInt(pObj,DT_OFF);
	pOut->uSec = (int)PH7_NativeAttrInt(pObj,DT_US);
	PH7_NativeAttrStr(pObj,DT_NAME,&pOut->zName,&pOut->nName);
}
static void DtStore(ph7_vm *pVm,ph7_class_instance *pObj,const dt_state *pIn)
{
	PH7_NativeSetAttrInt(pVm,pObj,DT_TS,pIn->iTs);
	PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,pIn->iOff);
	PH7_NativeSetAttrInt(pVm,pObj,DT_US,pIn->uSec);
	PH7_NativeSetAttrStr(pVm,pObj,DT_NAME,pIn->zName,pIn->nName);
}
/* The receiver of a native method, or NULL when the call has no object (which the
 * dispatcher only allows for a static one). */
static ph7_class_instance * DtThis(ph7_context *pCtx)
{
	return PH7_ContextThis(pCtx);
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
		PH7_NativeResultObject(pCtx,pTarget);
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
	if( PH7_NativeAttr(pObj,DTZ_NAME) == 0 ){
		return 0;
	}
	*piOff = (sxi32)PH7_NativeAttrInt(pObj,DTZ_OFF);
	PH7_NativeAttrStr(pObj,DTZ_NAME,pzName,pnName);
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
/*
 * The timezone spellings PHL understands with no tz database: UTC, GMT, Z and a
 * fixed [+-]HH:?MM offset. Shared by DateTimeZone::__construct(), which throws on
 * a miss, and timezone_open(), which warns and answers false.
 */
static int DtZoneParse(const char *zTz,int nTz,sxi32 *piOff,const char **pzName,
	int *pnName,char *zBuf,sxu32 nBuf)
{
	if( nTz == 1 && zTz[0] == 'Z' ){
		*piOff = 0;
		*pzName = "Z";
		*pnName = 1;
		return 0;
	}
	if( nTz == 3 && (SyStrnicmp(zTz,"UTC",3) == 0 || SyStrnicmp(zTz,"GMT",3) == 0) ){
		/* php answers the canonical spelling, whatever case the caller used. */
		*piOff = 0;
		*pzName = (zTz[0] == 'u' || zTz[0] == 'U') ? "UTC" : "GMT";
		*pnName = 3;
		return 0;
	}
	if( (nTz == 6 || nTz == 5) && (zTz[0] == '+' || zTz[0] == '-')
	 && SyisDigit(zTz[1]) && SyisDigit(zTz[2])
	 && (nTz == 5 ? (SyisDigit(zTz[3]) && SyisDigit(zTz[4]))
	              : (zTz[3] == ':' && SyisDigit(zTz[4]) && SyisDigit(zTz[5]))) ){
		int h = (zTz[1] - '0') * 10 + (zTz[2] - '0');
		int m = nTz == 5 ? (zTz[3] - '0') * 10 + (zTz[4] - '0')
		                 : (zTz[4] - '0') * 10 + (zTz[5] - '0');
		sxi32 iOff = h * 3600 + m * 60;
		if( zTz[0] == '-' ){
			iOff = -iOff;
		}
		*piOff = iOff;
		/* php normalizes the NAME through the offset, so "-00:00" is "+00:00". */
		*pnName = DtOffName(zBuf,nBuf,iOff);
		*pzName = zBuf;
		return 0;
	}
	return -1;
}
/* DateTimeZone::__construct(string $timezone) */
static int vm_builtin_DateTimeZone_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	const char *zTz,*zName;
	int nTz,nName;
	sxi32 iOff = 0;
	char zBuf[16];
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zTz = ph7_value_to_string(apArg[0],&nTz);
	if( DtZoneParse(zTz,nTz,&iOff,&zName,&nName,zBuf,sizeof(zBuf)) != 0 ){
		return PH7_VmThrowException(pCtx,"DateInvalidTimeZoneException",
			"DateTimeZone::__construct(): Unknown or bad timezone (%.*s)",nTz,zTz);
	}
	PH7_NativeSetAttrInt(pCtx->pVm,pThis,DTZ_OFF,iOff);
	PH7_NativeSetAttrStr(pCtx->pVm,pThis,DTZ_NAME,zName,nName);
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
	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DTZ_OFF));
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
	pZone = PH7_NewClassInstance(pVm,pZoneClass);
	if( pZone == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeAttrStr(pObj,DT_NAME,&zName,&nName);
	PH7_NativeSetAttrInt(pVm,pZone,DTZ_OFF,PH7_NativeAttrInt(pObj,DT_OFF));
	PH7_NativeSetAttrStr(pVm,pZone,DTZ_NAME,zName,nName);
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
	DtCivilDiff(PH7_NativeAttrInt(pBase,DT_TS),(sxi32)PH7_NativeAttrInt(pBase,DT_OFF),
		PH7_NativeAttrInt(pTarget,DT_TS),&sDiff);
	pIv = PH7_NewClassInstance(pVm,pIvClass);
	if( pIv == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeSetAttrInt(pVm,pIv,"y",sDiff.y);
	PH7_NativeSetAttrInt(pVm,pIv,"m",sDiff.m);
	PH7_NativeSetAttrInt(pVm,pIv,"d",sDiff.d);
	PH7_NativeSetAttrInt(pVm,pIv,"h",sDiff.h);
	PH7_NativeSetAttrInt(pVm,pIv,"i",sDiff.i);
	PH7_NativeSetAttrInt(pVm,pIv,"s",sDiff.s);
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
	if( pThis == 0 || nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
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
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zMod = ph7_value_to_string(apArg[0],&nMod);
	iErrPos = DtParse(zMod,nMod,PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),
		&iTs,&iOff,&bOffSet,&uSec);
	if( iErrPos != 0 ){
		int bImm = DtIsImmutable(pVm,pThis);
		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);
		return PH7_VmThrowException(pCtx,"DateMalformedStringException",
			"%s::modify(): Failed to parse time string (%.*s) at position %d (%c): %s",
			bImm ? "DateTimeImmutable" : "DateTime",nMod,zMod,iPos,cAt,zErr);
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	PH7_NativeSetAttrInt(pVm,pTarget,DT_TS,iTs);
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
	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_US,ph7_value_to_int64(apArg[0]));
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
	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_OFF,iOff);
	PH7_NativeSetAttrStr(pCtx->pVm,pTarget,DT_NAME,zName,nName);
	DtMutResult(pCtx,pTarget,bCopy);
	return PH7_OK;
}
/* Replace the DATE of an object, keeping its time of day (the offset it is
 * expressed in never changes). Shared with the date_date_set() alias. */
static void DtSetDateOf(ph7_context *pCtx,ph7_class_instance *pObj,sxi64 y,int mo,int d)
{
	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);
	sxi64 iDays = DtFloorDiv(iLocal,86400);
	sxi64 iSecs = iLocal - iDays*86400;
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,
		DtMakeTs(y,mo,d,(int)(iSecs / 3600),(int)((iSecs / 60) % 60),(int)(iSecs % 60),
			(sxi32)PH7_NativeAttrInt(pObj,DT_OFF)));
}
/* Replace the TIME of day, keeping the date. Shared with date_time_set(). */
static void DtSetTimeOf(ph7_context *pCtx,ph7_class_instance *pObj,int h,int mi,int s,sxi64 uSec)
{
	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);
	sxi64 iDays = DtFloorDiv(iLocal,86400);
	sxi64 y;
	int mo,d;
	DtCivilFromDays(iDays,&y,&mo,&d);
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,DtMakeTs(y,mo,d,h,mi,s,(sxi32)PH7_NativeAttrInt(pObj,DT_OFF)));
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
	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_TS,
		DtIsoDate(PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),
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
	if( PH7_NativeAttrInt(pIv,"invert") ){
		iSign = -iSign;
	}
	pTarget = DtMutTarget(pCtx,pThis,&bCopy);
	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_TS,
		DtCivilAdd(PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),
			PH7_NativeAttrInt(pIv,"y"),PH7_NativeAttrInt(pIv,"m"),PH7_NativeAttrInt(pIv,"d"),
			PH7_NativeAttrInt(pIv,"h"),PH7_NativeAttrInt(pIv,"i"),PH7_NativeAttrInt(pIv,"s"),iSign));
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
	PH7_NativeResultObject(pCtx,pObj);
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
 * ---------------------------------------------------------------------------
 * DateInterval, DatePeriod and its iterator, declared from C.
 *
 * The rest of the date chunk. DateInterval's two constructors were `preg_match`
 * calls in PHP; DatePeriod's `getIterator()` was a PHP GENERATOR, which a C body
 * cannot be -- so it answers a native `InternalIterator`, which is exactly the
 * class php answers there.
 * ---------------------------------------------------------------------------
 */
/* php's unit words for DateInterval::createFromDateString(), longest first so a
 * prefix never wins over the word that contains it. */
typedef struct dt_unit dt_unit;
struct dt_unit
{
	const char *zName;
	int iField;   /* 0=y 1=m 2=d 3=h 4=i 5=s */
	int nMul;
};
static const dt_unit aDtUnit[] = {
	{ "seconds", 5, 1 }, { "second", 5, 1 }, { "secs", 5, 1 }, { "sec", 5, 1 },
	{ "minutes", 4, 1 }, { "minute", 4, 1 }, { "mins", 4, 1 }, { "min", 4, 1 },
	{ "hours", 3, 1 },   { "hour", 3, 1 },
	{ "fortnights", 2, 14 }, { "fortnight", 2, 14 },
	{ "weeks", 2, 7 },   { "week", 2, 7 },
	{ "days", 2, 1 },    { "day", 2, 1 },
	{ "months", 1, 1 },  { "month", 1, 1 },
	{ "years", 0, 1 },   { "year", 0, 1 },
};
static const char * const azDtIvField[] = { "y", "m", "d", "h", "i", "s" };
/* Read an unsigned run of digits; returns the count consumed. */
static int DtIvDigits(const char *z,const char *zEnd,sxi64 *pVal)
{
	int n = 0;
	sxi64 v = 0;
	while( &z[n] < zEnd && SyisDigit(z[n]) ){
		v = v*10 + (z[n] - '0');
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
	for( k = 0 ; k < 6 ; k++ ){
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
		if( n == 0 || z + n >= zEnd ){
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
	const char *z = zIn,*zEnd = &zIn[nIn];
	sxi64 iTs = 0;
	sxi32 iOff = 0;
	int bOffSet = 0,uSec = 0,iErr;
	int k;
	for( k = 0 ; k < 6 ; k++ ){
		aOut[k] = 0;
	}
	if( nIn < 1 ){
		*piPos = 0;
		*pcAt = ' ';
		*pzReason = "Empty string";
		return -1;
	}
	iErr = DtParse(zIn,nIn,0,0,&iTs,&iOff,&bOffSet,&uSec);
	if( iErr != 0 ){
		*pzReason = DtParseErr(zIn,nIn,iErr,piPos,pcAt);
		return -1;
	}
	while( z < zEnd ){
		sxi64 v;
		int n,iSign = 1,iUnit;
		while( z < zEnd && (z[0] == ' ' || z[0] == '\t' || z[0] == '\n' || z[0] == '\r'
		 || z[0] == ',' || z[0] == '+') ){
			z++;
		}
		if( z < zEnd && z[0] == '-' ){
			iSign = -1;
			z++;
		}
		n = DtIvDigits(z,zEnd,&v);
		if( n == 0 ){
			/* Not a number: skip the token (php's parser already accepted the
			 * string, so this is a relative form with no interval field). */
			while( z < zEnd && z[0] != ' ' && z[0] != ',' ){
				z++;
			}
			continue;
		}
		z += n;
		while( z < zEnd && (z[0] == ' ' || z[0] == '\t') ){
			z++;
		}
		iUnit = -1;
		for( k = 0 ; k < (int)SX_ARRAYSIZE(aDtUnit) ; k++ ){
			int nU = (int)SyStrlen(aDtUnit[k].zName);
			if( zEnd - z >= nU && SyStrnicmp(z,aDtUnit[k].zName,(sxu32)nU) == 0
			 && (zEnd - z == nU || !(SyisAlphaNum(z[nU]) || z[nU] == '_')) ){
				iUnit = k;
				z += nU;
				break;
			}
		}
		if( iUnit < 0 ){
			continue;
		}
		aOut[aDtUnit[iUnit].iField] += iSign * v * aDtUnit[iUnit].nMul;
	}
	return 0;
}
/* Write the six relative fields onto a DateInterval instance. */
static void DtIvStore(ph7_vm *pVm,ph7_class_instance *pObj,const sxi64 *aVal)
{
	int k;
	for( k = 0 ; k < 6 ; k++ ){
		PH7_NativeSetAttrInt(pVm,pObj,azDtIvField[k],aVal[k]);
	}
}
/* DateInterval::__construct(string $duration) */
static int vm_builtin_DateInterval_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	const char *zDur;
	int nDur;
	sxi64 aVal[6];
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zDur = ph7_value_to_string(apArg[0],&nDur);
	if( DtIvParseIso(zDur,nDur,aVal) != 0 ){
		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",
			"Unknown or bad format (%.*s)",nDur,zDur);
	}
	DtIvStore(pCtx->pVm,pThis,aVal);
	return PH7_OK;
}
/*
 * DateInterval::createFromDateString(string $datetime). Shared with the
 * date_interval_create_from_date_string() alias, which WARNS and answers false
 * where the method throws.
 */
static ph7_class_instance * DtIvFromDateString(ph7_context *pCtx,const char *zIn,int nIn,
	int *piPos,char *pcAt,const char **pzReason)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = DtFactoryClass(pCtx,"DateInterval");
	ph7_class_instance *pObj;
	sxi64 aVal[6];
	ph7_value sVal;
	if( pClass == 0 ){
		return 0;
	}
	if( DtIvParseRelative(zIn,nIn,aVal,piPos,pcAt,pzReason) != 0 ){
		return 0;
	}
	pObj = PH7_NewClassInstance(pVm,pClass);
	if( pObj == 0 ){
		return 0;
	}
	DtIvStore(pVm,pObj,aVal);
	/* php marks the interval as built from a string; the `date_string` property it
	 * adds with it needs a dynamic property PHL has no equivalent of (§7.4). */
	PH7_MemObjInitFromBool(pVm,&sVal,1);
	PH7_NativeSetProp(pVm,pObj,"from_string",sizeof("from_string")-1,&sVal);
	PH7_MemObjRelease(&sVal);
	return pObj;
}
static int vm_builtin_DateInterval_createFromDateString(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zReason = "";
	int nIn,iPos = 0;
	char cAt = ' ';
	ph7_class_instance *pObj;
	if( nArg < 1 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason);
	if( pObj == 0 ){
		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",
			"Unknown or bad format (%.*s) at position %d (%c): %s",nIn,zIn,iPos,cAt,zReason);
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
			case 'Y': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"y")); break;
			case 'y': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"y")); break;
			case 'M': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"m")); break;
			case 'm': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"m")); break;
			case 'D': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"d")); break;
			case 'd': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"d")); break;
			case 'H': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"h")); break;
			case 'h': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"h")); break;
			case 'I': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"i")); break;
			case 'i': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"i")); break;
			case 'S': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"s")); break;
			case 's': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"s")); break;
			case 'F': case 'f': {
				ph7_value *pF = PH7_NativeAttr(pObj,"f");
				double r = 0.0;
				sxi64 uS;
				if( pF && (pF->iFlags & MEMOBJ_REAL) ){
					r = pF->rVal;
				}else if( pF && (pF->iFlags & MEMOBJ_INT) ){
					r = (double)pF->x.iVal;
				}
				uS = (sxi64)(r * 1000000.0 + (r < 0 ? -0.5 : 0.5));
				if( t == 'F' ){
					SyBlobFormat(&sOut,"%06d",(int)uS);
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
static int DpConstructInto(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,ph7_value **apArg,
	const char *zIsoStartClass)
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
		sxi64 aIv[6];
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
				"Unknown or bad format (%.*s)",nSpec,zSpec);
		}
		nDigits = DtIvDigits(&zSpec[1],&zSpec[nSpec],&nRec);
		k = 1 + nDigits;
		if( nDigits == 0 || k >= nSpec || zSpec[k] != '/' ){
			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",
				"Unknown or bad format (%.*s)",nSpec,zSpec);
		}
		zStart = &zSpec[k+1];
		nStart = 0;
		while( &zStart[nStart] < &zSpec[nSpec] && zStart[nStart] != '/' ){
			nStart++;
		}
		if( &zStart[nStart] >= &zSpec[nSpec] ){
			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",
				"Unknown or bad format (%.*s)",nSpec,zSpec);
		}
		zDur = &zStart[nStart+1];
		nDur = (int)(&zSpec[nSpec] - zDur);
		if( DtInitState(pCtx,zStart,nStart,0,pVm->zDefTz,(int)pVm->nDefTz,&sState,
			zNameBuf,sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0
		 || DtIvParseIso(zDur,nDur,aIv) != 0 ){
			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",
				"Unknown or bad format (%.*s)",nSpec,zSpec);
		}
		/* php's two ISO entry points disagree on the class they build, and both
		 * answers are load-bearing: `new DatePeriod("R2/...")` yields DateTime
		 * where DatePeriod::createFromISO8601String() yields DateTimeImmutable. */
		pStart = PH7_NewClassInstance(pVm,DtClass(pVm,zIsoStartClass));
		pIv = PH7_NewClassInstance(pVm,DtClass(pVm,"DateInterval"));
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
		if( nArg > 3 ){
			iOptions = ph7_value_to_int64(apArg[3]);
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
		}
	}
	PH7_NativeSetAttrBool(pVm,pThis,"include_start_date",(iOptions & 1) == 0);
	PH7_NativeSetAttrBool(pVm,pThis,"include_end_date",(iOptions & 2) != 0);
	return PH7_OK;
}
static int vm_builtin_DatePeriod_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
	if( pThis == 0 ){
		return PH7_OK;
	}
	return DpConstructInto(pCtx,pThis,nArg,apArg,"DateTime");
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
	pObj = PH7_NewClassInstance(pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* php's factory IS the constructor, with the same overloaded argument shape. */
	rc = DpConstructInto(pCtx,pObj,nArg,apArg,"DateTimeImmutable");
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
static int vm_builtin_DatePeriod_getEndDate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = DtThis(pCtx);
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
	ph7_class_instance *pThis = DtThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || PH7_NativeAttrObj(pThis,"end") != 0 ){
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
/* One interval step from a date object: a NEW object, so a value already handed
 * to the caller is never mutated underneath it (php's iterator answers a fresh
 * object per position too). */
static ph7_class_instance * DpAdvance(ph7_vm *pVm,ph7_class_instance *pCur,
	ph7_class_instance *pIv)
{
	ph7_class_instance *pNext = PH7_CloneClassInstance(pCur);
	int iSign = PH7_NativeAttrInt(pIv,"invert") ? -1 : 1;
	if( pNext == 0 ){
		return 0;
	}
	PH7_NativeSetAttrInt(&(*pVm),pNext,DT_TS,
		DtCivilAdd(PH7_NativeAttrInt(pCur,DT_TS),(sxi32)PH7_NativeAttrInt(pCur,DT_OFF),
			PH7_NativeAttrInt(pIv,"y"),PH7_NativeAttrInt(pIv,"m"),PH7_NativeAttrInt(pIv,"d"),
			PH7_NativeAttrInt(pIv,"h"),PH7_NativeAttrInt(pIv,"i"),PH7_NativeAttrInt(pIv,"s"),iSign));
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
static const PH7_NativeIterVtab sDpIterVtab = { DpRewind, DpNext };
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
	ph7_class_instance *pThis = DtThis(pCtx);
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
/* The receiver argument of a procedural alias (already type-screened by its row). */
static ph7_class_instance * DtArgObj(int nArg,ph7_value **apArg,int iArg)
{
	if( iArg >= nArg || (apArg[iArg]->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	return (ph7_class_instance *)apArg[iArg]->x.pOther;
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
	int nIn = 3,nZone,iPos;
	sxi32 iZoneOff = 0;
	dt_state sState;
	char zNameBuf[16],cAt;
	const char *zErr;
	if( pClass == 0 ){
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
		DtLastErrOne(pVm,iPos,zErr);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	DtLastErrClear(pVm);
	pObj = PH7_NewClassInstance(pVm,pClass);
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
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
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
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
	const char *zMod,*zErr;
	int nMod,iPos,iErrPos;
	char cAt;
	sxi64 iTs = 0;
	sxi32 iOff = 0;
	int bOffSet = 0,uSec = 0;
	if( pObj == 0 || nArg < 2 ){
		return PH7_OK;
	}
	zMod = ph7_value_to_string(apArg[1],&nMod);
	iErrPos = DtParse(zMod,nMod,PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),
		&iTs,&iOff,&bOffSet,&uSec);
	if( iErrPos != 0 ){
		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);
		PH7_VmThrowWarningFmt(pCtx->pVm,
			"date_modify(): Failed to parse time string (%.*s) at position %d (%c): %s",
			nMod,zMod,iPos,cAt,zErr);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,iTs);
	DtResultArg(pCtx,apArg);
	return PH7_OK;
}
static int DtProcAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)
{
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
	ph7_class_instance *pIv = DtArgObj(nArg,apArg,1);
	if( pObj == 0 || pIv == 0 ){
		return PH7_OK;
	}
	if( PH7_NativeAttrInt(pIv,"invert") ){
		iSign = -iSign;
	}
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,
		DtCivilAdd(PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),
			PH7_NativeAttrInt(pIv,"y"),PH7_NativeAttrInt(pIv,"m"),PH7_NativeAttrInt(pIv,"d"),
			PH7_NativeAttrInt(pIv,"h"),PH7_NativeAttrInt(pIv,"i"),PH7_NativeAttrInt(pIv,"s"),iSign));
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
	ph7_class_instance *pBase = DtArgObj(nArg,apArg,0);
	ph7_class_instance *pTarget = DtArgObj(nArg,apArg,1);
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
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
	if( pObj ){
		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_TS));
	}
	return PH7_OK;
}
static int vm_builtin_date_timestamp_set(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
	if( pObj == 0 || nArg < 2 ){
		return PH7_OK;
	}
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,ph7_value_to_int64(apArg[1]));
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,0);
	DtResultArg(pCtx,apArg);
	return PH7_OK;
}
static int vm_builtin_date_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
	if( pObj == 0 ){
		return PH7_OK;
	}
	return DtTimezoneResult(pCtx,pObj);
}
static int vm_builtin_date_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
	const char *zName = "UTC";
	int nName = 3;
	sxi32 iOff = 0;
	if( pObj == 0 || nArg < 2 || !DtZoneOf(apArg[1],&iOff,&zName,&nName) ){
		return PH7_OK;
	}
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_OFF,iOff);
	PH7_NativeSetAttrStr(pCtx->pVm,pObj,DT_NAME,zName,nName);
	DtResultArg(pCtx,apArg);
	return PH7_OK;
}
static int vm_builtin_date_offset_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
	if( pObj ){
		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_OFF));
	}
	return PH7_OK;
}
static int vm_builtin_date_date_set(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
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
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
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
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
	if( pObj == 0 || nArg < 3 ){
		return PH7_OK;
	}
	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,
		DtIsoDate(PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),
			ph7_value_to_int64(apArg[1]),ph7_value_to_int64(apArg[2]),
			nArg > 3 ? ph7_value_to_int64(apArg[3]) : 1));
	DtResultArg(pCtx,apArg);
	return PH7_OK;
}
/* date_interval_create_from_date_string(): warns and answers false where the
 * method throws. */
static int vm_builtin_date_interval_create_from_date_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zReason = "";
	int nIn,iPos = 0;
	char cAt = ' ';
	ph7_class_instance *pObj;
	if( nArg < 1 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason);
	if( pObj == 0 ){
		PH7_VmThrowWarningFmt(pCtx->pVm,
			"date_interval_create_from_date_string(): Unknown or bad format (%.*s) "
			"at position %d (%c): %s",nIn,zIn,iPos,cAt,zReason);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
static int vm_builtin_date_interval_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
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
	int nTz,nName;
	sxi32 iOff = 0;
	char zBuf[16];
	if( pClass == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zTz = ph7_value_to_string(apArg[0],&nTz);
	if( DtZoneParse(zTz,nTz,&iOff,&zName,&nName,zBuf,sizeof(zBuf)) != 0 ){
		PH7_VmThrowWarningFmt(pVm,"timezone_open(): Unknown or bad timezone (%.*s)",nTz,zTz);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pObj = PH7_NewClassInstance(pVm,pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeSetAttrInt(pVm,pObj,DTZ_OFF,iOff);
	PH7_NativeSetAttrStr(pVm,pObj,DTZ_NAME,zName,nName);
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
static int vm_builtin_timezone_name_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
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
	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);
	if( pObj ){
		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DTZ_OFF));
	}
	return PH7_OK;
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
	{ DT_US,   PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }
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
static int DtZoneTypeOf(const char *zName,int nName)
{
	sxu32 nPos = 0;
	if( nName > 0 && (zName[0] == '+' || zName[0] == '-') ){
		return 1;
	}
	if( nName == 3 && SyStrnicmp(zName,"UTC",3) == 0 ){
		return 3;
	}
	if( nName > 0 && SyByteFind(zName,(sxu32)nName,'/',&nPos) == SXRET_OK ){
		return 3;
	}
	return 2;
}
static void DtPresentPut(ph7_vm *pVm,ph7_value *pOut,const char *zKey,ph7_value *pVal)
{
	ph7_value sKey;
	PH7_MemObjInitFromString(&(*pVm),&sKey,0);
	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));
	ph7_array_add_elem(pOut,&sKey,pVal);
	PH7_MemObjRelease(&sKey);
}
static void DtPresentZone(ph7_vm *pVm,ph7_value *pOut,const char *zName,int nName)
{
	ph7_value sVal;
	PH7_MemObjInitFromInt(&(*pVm),&sVal,DtZoneTypeOf(zName,nName));
	DtPresentPut(&(*pVm),pOut,"timezone_type",&sVal);
	PH7_MemObjRelease(&sVal);
	PH7_MemObjInitFromString(&(*pVm),&sVal,0);
	PH7_MemObjStringAppend(&sVal,zName,(sxu32)nName);
	DtPresentPut(&(*pVm),pOut,"timezone",&sVal);
	PH7_MemObjRelease(&sVal);
}
static sxi32 DtPresentDateTime(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	dt_state sState;
	Sytm sTm;
	char zZone[64];
	char zDate[64];
	ph7_value sVal;
	int nName;
	SXUNUSED(bDebug); /* php shows the same three keys to both handlers */
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
	SyBufferFormat(zDate,sizeof(zDate),"%04d-%02d-%02d %02d:%02d:%02d.%06d",
		sTm.tm_year,sTm.tm_mon + 1,sTm.tm_mday,sTm.tm_hour,sTm.tm_min,sTm.tm_sec,
		sState.uSec);
	PH7_MemObjInitFromString(&(*pVm),&sVal,0);
	PH7_MemObjStringAppend(&sVal,zDate,(sxu32)SyStrlen(zDate));
	DtPresentPut(&(*pVm),pOut,"date",&sVal);
	PH7_MemObjRelease(&sVal);
	DtPresentZone(&(*pVm),pOut,zZone,nName);
	return SXRET_OK;
}
static sxi32 DtPresentTimeZone(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	const char *zName = 0;
	int nName = 0;
	SXUNUSED(bDebug);
	PH7_NativeAttrStr(pThis,DTZ_NAME,&zName,&nName);
	DtPresentZone(&(*pVm),pOut,zName ? zName : "",nName);
	return SXRET_OK;
}
/* php's DateTimeInterface constants, the whole of that interface's surface here
 * (its abstract METHODS are deliberately not declared: PH7_ClassImplement installs
 * a stub for every interface method an implementor lacks, so declaring them would
 * make every implementor abstract before its native methods are attached). */
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
	static const PH7_NativePropDef aZoneProp[] = {
		{ DTZ_OFF,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },
		{ DTZ_NAME, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aZoneMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $timezone", "", vm_builtin_DateTimeZone_construct },
		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_DateTimeZone_getName },
		{ "getOffset",   PH7_MOD_PUBLIC, "DateTimeInterface $datetime", "@int",
		  vm_builtin_DateTimeZone_getOffset },
	};
	static const PH7_NativePropDef aDtProp[] = { DT_NATIVE_STATE_PROPS };
	static const PH7_NativeMethodDef aDtMethod[] = {
		DT_NATIVE_SHARED_METHODS("DateTime"),
		{ "createFromFormat",    PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTime|false",
		  vm_builtin_DateTime_createFromFormat },
		{ "createFromImmutable", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTimeImmutable $object", "@static",
		  vm_builtin_DateTime_copyOf },
		{ "createFromInterface", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTime",
		  vm_builtin_DateTime_copyOf },
	};
	static const PH7_NativeMethodDef aImmMethod[] = {
		DT_NATIVE_SHARED_METHODS("DateTimeImmutable"),
		{ "createFromFormat",    PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTimeImmutable|false",
		  vm_builtin_DateTimeImmutable_createFromFormat },
		{ "createFromMutable",   PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTime $object", "@static",
		  vm_builtin_DateTimeImmutable_copyOf },
		{ "createFromInterface", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTimeImmutable",
		  vm_builtin_DateTimeImmutable_copyOf },
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
	};
	static const PH7_NativeMethodDef aIvMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $duration", "",
		  vm_builtin_DateInterval_construct },
		{ "createFromDateString", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "string $datetime", "@DateInterval",
		  vm_builtin_DateInterval_createFromDateString },
		{ "format",      PH7_MOD_PUBLIC, "string $format", "@string", vm_builtin_DateInterval_format },
	};
	/* php models all seven as VIRTUAL hooked properties, so it reports no default
	 * for any of them; PHL's are real slots and keep theirs, because a read before
	 * the first write must answer what php's getter answers rather than raise. The
	 * TYPE is what a spec row can state exactly — the virtual half is PLAN §7.4. */
	static const PH7_NativePropDef aDpProp[] = {
		{ "start",              PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },
		{ "current",            PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },
		{ "end",                PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },
		{ "interval",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateInterval" },
		{ "recurrences",        PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,  1, 0, 0.0 }, "int" },
		{ "include_start_date", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, "bool" },
		{ "include_end_date",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },
	};
	static const PH7_NativeConstDef aDpConst[] = {
		{ "EXCLUDE_START_DATE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },
		{ "INCLUDE_END_DATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },
	};
	static const PH7_NativeMethodDef aDpMethod[] = {
		/* php overloads this constructor three ways and rejects everything else with
		 * ONE message, so the signature stays unenforced and the body decides. */
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
	};
	static const PH7_NativeClassSpec aSpec[] = {
		/* Exceptions first: the classes below throw them. */
		{ "DateException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateMalformedStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateInvalidTimeZoneException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateMalformedIntervalStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateMalformedPeriodStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DateTimeInterface", 0, 0, PH7_CLASS_INTERFACE,
		  0, 0, aIfaceConst, SX_ARRAYSIZE(aIfaceConst), 0, 0, 0, 0, 0 },
		{ "DateTimeZone", 0, 0, 0,
		  aZoneMethod, SX_ARRAYSIZE(aZoneMethod), 0, 0, aZoneProp, SX_ARRAYSIZE(aZoneProp),
		  0, 0, DtPresentTimeZone },
		{ "DateTime", 0, "DateTimeInterface", 0,
		  aDtMethod, SX_ARRAYSIZE(aDtMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),
		  0, 0, DtPresentDateTime },
		{ "DateTimeImmutable", 0, "DateTimeInterface", 0,
		  aImmMethod, SX_ARRAYSIZE(aImmMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),
		  0, 0, DtPresentDateTime },
		{ "DateInterval", 0, 0, 0,
		  aIvMethod, SX_ARRAYSIZE(aIvMethod), 0, 0, aIvProp, SX_ARRAYSIZE(aIvProp), 0, 0, 0 },
		{ "DatePeriod", 0, 0, 0,
		  aDpMethod, SX_ARRAYSIZE(aDpMethod), aDpConst, SX_ARRAYSIZE(aDpConst),
		  aDpProp, SX_ARRAYSIZE(aDpProp), 0, &sDpIterVtab, 0 },
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
		{ "timezone_open",                vm_builtin_timezone_open },
		{ "timezone_name_get",            vm_builtin_timezone_name_get },
		{ "timezone_offset_get",          vm_builtin_timezone_offset_get },
	};
	sxu32 n;
	sxi32 rc;
	/* php's date.timezone default */
	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));
	pVm->nDefTz = sizeof("UTC") - 1;
	DtLastErrClear(&(*pVm));
	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
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
