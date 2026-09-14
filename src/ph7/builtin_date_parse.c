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
	int uSec = 0;
	int iErrPos;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nLen);
	iBaseTs  = ph7_value_to_int64(apArg[1]);
	iBaseOff = (sxi32)ph7_value_to_int64(apArg[2]);
	iErrPos = DtParse(zIn,nLen,iBaseTs,iBaseOff,&iTs,&iOff,&bOffSet,&uSec);
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
		/* [3] = microseconds parsed from a fractional-seconds part (0 when absent) */
		ph7_value_int64(pV,uSec);
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
/* string __dt_format(int $ts, int $off, string $tzname, string $format, int $us = 0) */
static int vm_builtin_dt_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	Sytm sTm;
	sxi64 iTs;
	sxi32 iOff;
	const char *zName,*zFmt;
	int nName,nFmt,uSec = 0;
	char zZone[64];
	if( nArg < 4 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iTs  = ph7_value_to_int64(apArg[0]);
	iOff = (sxi32)ph7_value_to_int64(apArg[1]);
	zName = ph7_value_to_string(apArg[2],&nName);
	zFmt  = ph7_value_to_string(apArg[3],&nFmt);
	if( nArg > 4 ){ uSec = ph7_value_to_int(apArg[4]); }
	if( nName >= (int)sizeof(zZone) ){ nName = (int)sizeof(zZone) - 1; }
	SyMemcpy(zName,zZone,(sxu32)nName);
	zZone[nName] = 0;
	DtFillSytm(iTs,iOff,zZone,&sTm);
	DateFormat(pCtx,zFmt,nFmt,&sTm,uSec);
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
	int uSecFF = 0,bHasUs = 0;
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
		/* microseconds from a u/v token ride an associative key so they never
		 * collide with the numeric [4+] trailing-warning pairs */
		if( bHasUs ){ ph7_value_int64(pV,uSecFF); ph7_array_add_strkey_elem(pArr,"us",pV); }
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
" private $__dtUs = 0;"
" private function __dtInit($datetime, $timezone){"
"  $off = 0; $name = __dt_default_tz();"
"  if( $timezone !== null ){"
"   $off = $timezone->getOffset($this);"
"   $name = $timezone->getName();"
"  }"
"  $r = __dt_parse((string)$datetime, __dt_now(), $off);"
"  if( is_string($r) ){ throw new DateMalformedStringException($r); }"
"  $this->__dtTs = $r[0];"
"  $this->__dtUs = $r[3];"
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
" public function format($format){ return __dt_format($this->__dtTs, $this->__dtOff, $this->__dtName, (string)$format, $this->__dtUs); }"
" public function getTimestamp(){ return $this->__dtTs; }"
" public function getMicrosecond(){ return $this->__dtUs; }"
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
"  $obj->__dtUs = $r['us'] ?? 0;"
"  if( $r[2] === 0 ){ $obj->__dtOff = $off; $obj->__dtName = $name; }"
"  elseif( $r[2] === 2 ){ $obj->__dtOff = 0; $obj->__dtName = 'Z'; }"
"  elseif( $r[2] === 3 ){ $obj->__dtOff = $r[1]; $obj->__dtName = $r[3]; }"
"  else { $obj->__dtOff = $r[1]; $obj->__dtName = $obj->__dtOffName($r[1]); }"
"  return $obj;"
" }"
" private static function __dtCopyOf($object, $class){"
"  $d = new $class('@0');"
"  $d->__dtTs = $object->getTimestamp();"
"  $d->__dtUs = $object->getMicrosecond();"
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
" public function setTimestamp($timestamp){ $this->__dtTs = (int)$timestamp; $this->__dtUs = 0; return $this; }"
" public function setMicrosecond($microsecond){ $this->__dtUs = (int)$microsecond; return $this; }"
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
"  $this->__dtUs = (int)$microsecond;"
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
" public function setTimestamp($timestamp){ $c = clone $this; $c->__dtTs = (int)$timestamp; $c->__dtUs = 0; return $c; }"
" public function setMicrosecond($microsecond){ $c = clone $this; $c->__dtUs = (int)$microsecond; return $c; }"
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
"  $c->__dtUs = (int)$microsecond;"
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
/* int|false strtotime(string $datetime, ?int $baseTimestamp = null). Rides the
 * same DtParse the DateTime constructor uses, so its format coverage is identical.
 * php: the EMPTY string is false, but whitespace-only is 'now'; a parse failure is
 * false (never an exception). The default timezone is treated as offset 0, exactly
 * as the DateTime constructor does for a null $timezone. */
"function strtotime($datetime, $baseTimestamp = null){"
" $s = (string)$datetime;"
" if( $s === '' ){ return false; }"
" $base = $baseTimestamp === null ? __dt_now() : (int)$baseTimestamp;"
" $r = __dt_parse($s, $base, 0);"
" return is_string($r) ? false : $r[0];"
"}"
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
