# src/ph7/builtin_date_parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2323/2681 lines (86.65%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * The DateTime family: proleptic-Gregorian date math, the date/time` |
|      - |    9 | ` * string parser, the __dt_* host thunks, the embedded zDateTimeLib PHP` |
|      - |   10 | ` * chunk and PH7_VmInstallDateTime. The classic procedural date functions` |
|      - |   11 | ` * (date/gmdate/mktime/...) stay in builtin_date.c.` |
|      - |   12 | ` */` |
|      - |   13 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |   14 | `#include <time.h>` |
|      - |   15 | `/* ===========================================================================` |
|      - |   16 | ` * DateTime family (NEWPLAN band D slice 1): DateTimeInterface, DateTime,` |
|      - |   17 | ` * DateTimeImmutable, DateTimeZone (UTC + fixed offsets), date_create(),` |
|      - |   18 | ` * date_create_immutable(). Embedded-PHP chunk + C thunks, following the` |
|      - |   19 | ` * Reflection architecture (installed inside the bCompilingBuiltin window).` |
|      - |   20 | ` * Timezone SCOPE: UTC and fixed "+HH:MM" offsets only — no tz database` |
|      - |   21 | ` * (recorded §10 scope cut; named region zones throw like unknown zones).` |
|      - |   22 | ` * ======================================================================== */` |
|      - |   23 |  |
|      - |   24 | `/*` |
|      - |   25 | ` * Proleptic-Gregorian civil <-> day-count conversions (Howard Hinnant's` |
|      - |   26 | ` * algorithms): no time_t / libc dependence, correct far past 2038 and` |
|      - |   27 | ` * before 1970 on every platform. Day 0 == 1970-01-01.` |
|      - |   28 | ` */` |
|   1790 |   29 | `PH7_PRIVATE sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|      2 |   30 | `{` |
|      - |   31 | `	sxi64 era;` |
|      - |   32 | `	unsigned yoe,doy,doe;` |
|   1792 |   33 | `	y -= (m <= 2);` |
|   1792 |   34 | `	era = (y >= 0 ? y : y - 399) / 400;` |
|   1792 |   35 | `	yoe = (unsigned)(y - era * 400);` |
|   1792 |   36 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|   1792 |   37 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|   1792 |   38 | `	return era * 146097 + (sxi64)doe - 719468;` |
|      2 |   39 | `}` |
|    976 |   40 | `PH7_PRIVATE void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|      1 |   41 | `{` |
|      - |   42 | `	sxi64 era;` |
|      - |   43 | `	unsigned doe,yoe,doy,mp;` |
|    977 |   44 | `	z += 719468;` |
|    977 |   45 | `	era = (z >= 0 ? z : z - 146096) / 146097;` |
|    977 |   46 | `	doe = (unsigned)(z - era * 146097);` |
|    977 |   47 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|    977 |   48 | `	*py = (sxi64)yoe + era * 400;` |
|    977 |   49 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|    977 |   50 | `	mp = (5 * doy + 2) / 153;` |
|    977 |   51 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|    977 |   52 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|    977 |   53 | `	if( *pm <= 2 ){` |
|    639 |   54 | `		*py += 1;` |
|    319 |   55 | `	}` |
|    977 |   56 | `}` |
|   1782 |   57 | `PH7_PRIVATE sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|      1 |   58 | `{` |
|   1783 |   59 | `	sxi64 q = a / b;` |
|   1783 |   60 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|     29 |   61 | `		q--;` |
|     14 |   62 | `	}` |
|   1783 |   63 | `	return q;` |
|      1 |   64 | `}` |
|      - |   65 | `/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z) */` |
|    454 |   66 | `static void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)` |
|      1 |   67 | `{` |
|    455 |   68 | `	sxi64 t = iTs + iOff;` |
|    455 |   69 | `	sxi64 days = DtFloorDiv(t,86400);` |
|    455 |   70 | `	sxi64 secs = t - days * 86400;` |
|      - |   71 | `	sxi64 y;` |
|      - |   72 | `	int mo,d;` |
|    455 |   73 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|    455 |   74 | `	pTm->tm_sec  = (int)(secs % 60);` |
|    455 |   75 | `	pTm->tm_min  = (int)((secs / 60) % 60);` |
|    455 |   76 | `	pTm->tm_hour = (int)(secs / 3600);` |
|    455 |   77 | `	pTm->tm_mday = d;` |
|    455 |   78 | `	pTm->tm_mon  = mo - 1;` |
|    455 |   79 | `	pTm->tm_year = (int)y;` |
|    455 |   80 | `	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */` |
|    455 |   81 | `	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));` |
|    455 |   82 | `	pTm->tm_isdst = 0;` |
|    455 |   83 | `	pTm->tm_zone = zZone;` |
|    455 |   84 | `	pTm->tm_gmtoff = (long)iOff;` |
|    455 |   85 | `}` |
|    484 |   86 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|      2 |   87 | `{` |
|    486 |   88 | `	return DtDaysFromCivil(y,mo,d) * 86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|      2 |   89 | `}` |
|      - |   90 | `/* Month-arithmetic with php's overflow semantics (Jan 31 +1 month -> Mar 2/3):` |
|      - |   91 | ` * normalize the month, keep the day — the civil day-count formula is linear in` |
|      - |   92 | ` * d, so an out-of-range day simply lands in the following month. */` |
|     28 |   93 | `static sxi64 DtAddMonths(sxi64 iTs,sxi32 iOff,sxi64 nMonths)` |
|      1 |   94 | `{` |
|     29 |   95 | `	sxi64 t = iTs + iOff;` |
|     29 |   96 | `	sxi64 days = DtFloorDiv(t,86400);` |
|     29 |   97 | `	sxi64 secs = t - days * 86400;` |
|      - |   98 | `	sxi64 y;` |
|      - |   99 | `	int mo,d;` |
|      - |  100 | `	sxi64 m0;` |
|     29 |  101 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|     29 |  102 | `	m0 = (y * 12 + (mo - 1)) + nMonths;` |
|     29 |  103 | `	y  = DtFloorDiv(m0,12);` |
|     29 |  104 | `	mo = (int)(m0 - y * 12) + 1;` |
|     29 |  105 | `	return DtDaysFromCivil(y,mo,d) * 86400 + secs - iOff;` |
|      1 |  106 | `}` |
|      - |  107 | `/*` |
|      - |  108 | ` * Read a fractional-seconds part at z (which points at the '.'): up to 6 digits` |
|      - |  109 | ` * become microseconds (right-padded to 6, extra digits ignored). Advances *pz.` |
|      - |  110 | ` */` |
|    140 |  111 | `static int DtReadFraction(const char **pz,const char *zEnd)` |
|      1 |  112 | `{` |
|    141 |  113 | `	const char *z = *pz;` |
|    141 |  114 | `	int us = 0,n = 0;` |
|    141 |  115 | `	z++; /* skip '.' */` |
|    923 |  116 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|    783 |  117 | `		if( n < 6 ){ us = us*10 + (z[0]-'0'); n++; }` |
|    783 |  118 | `		z++;` |
|      1 |  119 | `	}` |
|    199 |  120 | `	while( n < 6 ){ us *= 10; n++; }` |
|    141 |  121 | `	*pz = z;` |
|    141 |  122 | `	return us;` |
|      1 |  123 | `}` |
|      - |  124 | `/*` |
|      - |  125 | ` * Parse an OPTIONAL time-of-day suffix after a date component:` |
|      - |  126 | ` * "[( \|T)]HH:MM[:SS][.frac][Z\|±hh[:mm]]". On entry *pz points just past the date;` |
|      - |  127 | ` * the h/mi/s outs must be pre-zeroed and the offset outs pre-seeded with the current` |
|      - |  128 | ` * offset; *pUs receives the microseconds from a fractional part (unchanged when` |
|      - |  129 | ` * absent). Advances *pz over whatever it consumes. Returns 0 on success (whether or` |
|      - |  130 | ` * not a time was present), or a 1-based error position into zIn (negative encodes` |
|      - |  131 | ` * php's "Double time specification"). Shared by every absolute-date branch.` |
|      - |  132 | ` */` |
|    424 |  133 | `static int DtTimeSuffix(const char **pz,const char *zEnd,const char *zIn,` |
|      - |  134 | `	int *ph,int *pmi,int *ps,sxi32 *piOff,int *pbOffSet,int *pUs)` |
|      2 |  135 | `{` |
|    426 |  136 | `	const char *z = *pz;` |
|    424 |  137 | `	if( z < zEnd && (z[0]=='T' \|\| z[0]==' ') && zEnd-z >= 6` |
|    258 |  138 | `	 && SyisDigit(z[1]) && SyisDigit(z[2]) && z[3]==':' ){` |
|    253 |  139 | `		z++;` |
|    253 |  140 | `		*ph  = (z[0]-'0')*10 + (z[1]-'0');` |
|    253 |  141 | `		*pmi = (z[3]-'0')*10 + (z[4]-'0');` |
|      - |  142 | `		/* a 25+ hour kills php's whole time token: error at its start */` |
|    253 |  143 | `		if( *ph > 24 ){ return (int)(z - zIn) + 1; }` |
|      - |  144 | `		/* php lexes HH:M, then the minute's second digit starts a SECOND time` |
|      - |  145 | `		 * token: "Double time specification" (negative encoding) */` |
|    251 |  146 | `		if( *pmi > 59 ){ return -((int)(&z[4] - zIn) + 1); }` |
|    249 |  147 | `		z += 5;` |
|    249 |  148 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|    247 |  149 | `			*ps = (z[1]-'0')*10 + (z[2]-'0');` |
|    247 |  150 | `			if( *ps > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|    245 |  151 | `			z += 3;` |
|    122 |  152 | `		}` |
|    247 |  153 | `		if( z < zEnd && z[0]=='.' && zEnd-z >= 2 && SyisDigit(z[1]) ){ /* fractional seconds */` |
|    139 |  154 | `			*pUs = DtReadFraction(&z,zEnd);` |
|     69 |  155 | `		}` |
|    247 |  156 | `		if( z < zEnd && (z[0]=='Z' \|\| z[0]=='z') ){` |
|     13 |  157 | `			*piOff = 0; *pbOffSet = 2; z++;` |
|    241 |  158 | `		}else if( z < zEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|      7 |  159 | `			int sign = (z[0]=='-') ? -1 : 1;` |
|      7 |  160 | `			int oh,om = 0;` |
|      7 |  161 | `			z++;` |
|      7 |  162 | `			if( zEnd-z < 2 \|\| !SyisDigit(z[0]) \|\| !SyisDigit(z[1]) ){ return (int)(z - zIn) + 1; }` |
|      7 |  163 | `			oh = (z[0]-'0')*10 + (z[1]-'0');` |
|      7 |  164 | `			z += 2;` |
|      7 |  165 | `			if( z < zEnd && z[0]==':' ){ z++; }` |
|      7 |  166 | `			if( zEnd-z >= 2 && SyisDigit(z[0]) && SyisDigit(z[1]) ){` |
|      7 |  167 | `				om = (z[0]-'0')*10 + (z[1]-'0');` |
|      7 |  168 | `				z += 2;` |
|      3 |  169 | `			}` |
|      7 |  170 | `			*piOff = sign * (oh*3600 + om*60);` |
|      7 |  171 | `			*pbOffSet = 1;` |
|      3 |  172 | `		}` |
|    123 |  173 | `	}` |
|    420 |  174 | `	*pz = z;` |
|    420 |  175 | `	return 0;` |
|    214 |  176 | `}` |
|      - |  177 | `/*` |
|      - |  178 | ` * Read one or two decimal digits at z (z<zEnd guaranteed by caller for the first).` |
|      - |  179 | ` * Returns the value; *pn = digits consumed (1 or 2).` |
|      - |  180 | ` */` |
|    130 |  181 | `static int DtRead1or2(const char *z,const char *zEnd,int *pn)` |
|      1 |  182 | `{` |
|    131 |  183 | `	int v = z[0]-'0';` |
|    131 |  184 | `	if( z+1 < zEnd && SyisDigit(z[1]) ){ v = v*10 + (z[1]-'0'); *pn = 2; }` |
|     23 |  185 | `	else { *pn = 1; }` |
|    131 |  186 | `	return v;` |
|      1 |  187 | `}` |
|      - |  188 | `/*` |
|      - |  189 | ` * Try to read a non-ISO numeric date at z: three integer components joined by ONE` |
|      - |  190 | ` * consistent separator, plus an optional time suffix. php's field order depends on` |
|      - |  191 | ` * the separator:` |
|      - |  192 | ` *   '/'      -> YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY` |
|      - |  193 | ` *   '-','.'  -> DD-MM-YYYY (day first); a 4-digit-first '.' date (YYYY.MM.DD) is` |
|      - |  194 | ` *               NOT a php format and is rejected. (ISO YYYY-MM-DD is matched by the` |
|      - |  195 | ` *               dedicated branch BEFORE this one, so a 4-digit-first '-' never` |
|      - |  196 | ` *               reaches here.)` |
|      - |  197 | ` * A 1-2 digit year maps php-style (00-69 -> 2000s, 70-99 -> 1900s). Returns 0 when` |
|      - |  198 | ` * the text is not such a date (caller falls through), 1 on success (the ts/off outs` |
|      - |  199 | ` * set and *pzOut advanced past the whole token), or an error code in DtParse's own` |
|      - |  200 | ` * convention (positive 1-based position into zIn, negative = "double time") when the` |
|      - |  201 | ` * shape matched but a component is out of range.` |
|      - |  202 | ` */` |
|    102 |  203 | `static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,` |
|      - |  204 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,int *pUs)` |
|      1 |  205 | `{` |
|      - |  206 | `	int a,b,c,na,nb,nc;` |
|      - |  207 | `	char sep;` |
|    103 |  208 | `	int y,mo,d,h = 0,mi = 0,s = 0,us = 0;` |
|    103 |  209 | `	sxi32 iOff = *pOff;` |
|      - |  210 | `	int rcT;` |
|      - |  211 | `	/* first field: 1-4 digits */` |
|    103 |  212 | `	if( !SyisDigit(z[0]) ){ return 0; }` |
|    103 |  213 | `	a = 0; na = 0;` |
|    315 |  214 | `	while( z < zEnd && SyisDigit(z[0]) && na < 4 ){ a = a*10 + (z[0]-'0'); z++; na++; }` |
|    103 |  215 | `	if( z >= zEnd \|\| (z[0] != '-' && z[0] != '/' && z[0] != '.') ){ return 0; }` |
|     57 |  216 | `	sep = z[0];` |
|     57 |  217 | `	z++;` |
|      - |  218 | `	/* second field: 1-2 digits */` |
|     57 |  219 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|     57 |  220 | `	b = DtRead1or2(z,zEnd,&nb); z += nb;` |
|     57 |  221 | `	if( z >= zEnd \|\| z[0] != sep ){ return 0; }` |
|     57 |  222 | `	z++;` |
|      - |  223 | `	/* third field: 1-4 digits */` |
|     57 |  224 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|     57 |  225 | `	c = 0; nc = 0;` |
|    239 |  226 | `	while( z < zEnd && SyisDigit(z[0]) && nc < 4 ){ c = c*10 + (z[0]-'0'); z++; nc++; }` |
|      - |  227 | `	/* map fields to Y/M/D; nyear tracks the year field's width for 2-digit mapping.` |
|      - |  228 | `	 * '/'  : YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY.` |
|      - |  229 | `	 * '-'/'.': a 4-digit LAST field is DD-MM-YYYY (day first); otherwise YY-MM-DD` |
|      - |  230 | `	 *          (year first) — php's width heuristic. (A 4-digit FIRST '-' field is` |
|      - |  231 | `	 *          ISO and never reaches here; a 4-digit-first '.' is not a php format.) */` |
|      - |  232 | `	{` |
|      - |  233 | `		int nyear;` |
|     57 |  234 | `		if( sep == '/' ){` |
|     27 |  235 | `			if( na == 4 ){ y = a; mo = b; d = c; nyear = na; }` |
|     19 |  236 | `			else{ mo = a; d = b; y = c; nyear = nc; }` |
|     44 |  237 | `		}else if( sep == '.' ){` |
|      - |  238 | `			/* php's dot date is DD.MM.YYYY only (a 4-digit year, day first). Other` |
|      - |  239 | `			 * widths are not a clean php format (php itself yields garbage there),` |
|      - |  240 | `			 * so don't claim the match — let the caller fail the parse. */` |
|      5 |  241 | `			if( na == 4 \|\| nc != 4 ){ return 0; }` |
|      3 |  242 | `			d = a; mo = b; y = c; nyear = nc;` |
|      2 |  243 | `		}else{ /* '-' : a 4-digit LAST field is DD-MM-YYYY, else YY-MM-DD */` |
|     27 |  244 | `			if( nc == 4 ){ d = a; mo = b; y = c; nyear = nc; }` |
|     11 |  245 | `			else{ y = a; mo = b; d = c; nyear = na; }` |
|      - |  246 | `		}` |
|     55 |  247 | `		if( nyear <= 2 ){` |
|     11 |  248 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|      3 |  249 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|      5 |  250 | `		}` |
|      - |  251 | `	}` |
|      - |  252 | `	/* php normalizes month 0 to December of the previous year (like the ISO branch)` |
|      - |  253 | `	 * but fails a month past 12; a day past 31 fails, while day 0 normalizes in` |
|      - |  254 | `	 * DtMakeTs. Errors point at the field end. */` |
|     55 |  255 | `	if( mo > 12 ){ return (int)(z - zIn) + 1; }` |
|     51 |  256 | `	if( mo == 0 ){ mo = 12; y--; }` |
|     51 |  257 | `	if( d > 31 ){ return (int)(z - zIn) + 1; }` |
|      - |  258 | `	/* optional time-of-day suffix, then commit */` |
|     47 |  259 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff,&us);` |
|     47 |  260 | `	if( rcT != 0 ){ return rcT; }` |
|     47 |  261 | `	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|     47 |  262 | `	*pOff = iOff;` |
|     47 |  263 | `	*pUs = us;` |
|     47 |  264 | `	*pzOut = z;` |
|     47 |  265 | `	return 1;` |
|     52 |  266 | `}` |
|      - |  267 | `/*` |
|      - |  268 | ` * Match a month name at z (full name or its distinct 3-letter abbreviation, plus` |
|      - |  269 | ` * "sept"), case-insensitively and only at a word boundary. Returns the month 1-12` |
|      - |  270 | ` * and sets *pAdv to the bytes consumed, or 0 when no month name is present.` |
|      - |  271 | ` */` |
|    284 |  272 | `static int DtMatchMonth(const char *z,const char *zEnd,int *pAdv)` |
|      1 |  273 | `{` |
|      - |  274 | `	static const struct { const char *z; int n; int mo; } aM[] = {` |
|      - |  275 | `		{ "january",7,1 },{ "february",8,2 },{ "march",5,3 },{ "april",5,4 },` |
|      - |  276 | `		{ "june",4,6 },{ "july",4,7 },{ "august",6,8 },{ "september",9,9 },` |
|      - |  277 | `		{ "sept",4,9 },{ "october",7,10 },{ "november",8,11 },{ "december",8,12 },` |
|      - |  278 | `		{ "may",3,5 },` |
|      - |  279 | `		{ "jan",3,1 },{ "feb",3,2 },{ "mar",3,3 },{ "apr",3,4 },{ "jun",3,6 },` |
|      - |  280 | `		{ "jul",3,7 },{ "aug",3,8 },{ "sep",3,9 },{ "oct",3,10 },{ "nov",3,11 },` |
|      - |  281 | `		{ "dec",3,12 }` |
|      - |  282 | `	};` |
|      - |  283 | `	sxu32 i;` |
|   6005 |  284 | `	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){` |
|   5783 |  285 | `		int n = aM[i].n;` |
|   5782 |  286 | `		if( zEnd - z >= n && SyStrnicmp(z,aM[i].z,(sxu32)n) == 0` |
|   2594 |  287 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|     63 |  288 | `			*pAdv = n;` |
|     63 |  289 | `			return aM[i].mo;` |
|      - |  290 | `		}` |
|   2861 |  291 | `	}` |
|    223 |  292 | `	return 0;` |
|    143 |  293 | `}` |
|      - |  294 | `/* Match a weekday name at z (full or 3-letter, case-insensitive, word boundary).` |
|      - |  295 | ` * Returns the day-of-week 0=Sunday..6=Saturday and sets *pAdv, or -1. */` |
|    184 |  296 | `static int DtMatchWeekday(const char *z,const char *zEnd,int *pAdv)` |
|      1 |  297 | `{` |
|      - |  298 | `	static const struct { const char *z; int n; int dow; } aW[] = {` |
|      - |  299 | `		{ "sunday",6,0 },{ "monday",6,1 },{ "tuesday",7,2 },{ "wednesday",9,3 },` |
|      - |  300 | `		{ "thursday",8,4 },{ "friday",6,5 },{ "saturday",8,6 },` |
|      - |  301 | `		{ "sun",3,0 },{ "mon",3,1 },{ "tue",3,2 },{ "wed",3,3 },{ "thu",3,4 },` |
|      - |  302 | `		{ "fri",3,5 },{ "sat",3,6 }` |
|      - |  303 | `	};` |
|      - |  304 | `	sxu32 i;` |
|   2289 |  305 | `	for( i = 0 ; i < SX_ARRAYSIZE(aW) ; ++i ){` |
|   2151 |  306 | `		int n = aW[i].n;` |
|   2150 |  307 | `		if( zEnd - z >= n && SyStrnicmp(z,aW[i].z,(sxu32)n) == 0` |
|    894 |  308 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|     47 |  309 | `			*pAdv = n;` |
|     47 |  310 | `			return aW[i].dow;` |
|      - |  311 | `		}` |
|   1053 |  312 | `	}` |
|    139 |  313 | `	return -1;` |
|     93 |  314 | `}` |
|      - |  315 | `/* True if z points at a two-letter English ordinal suffix (st/nd/rd/th). */` |
|     74 |  316 | `static int DtIsOrdinal(const char *z,const char *zEnd)` |
|      1 |  317 | `{` |
|     75 |  318 | `	if( zEnd - z < 2 ){ return 0; }` |
|    143 |  319 | `	return SyStrnicmp(z,"st",2) == 0 \|\| SyStrnicmp(z,"nd",2) == 0` |
|    107 |  320 | `		\|\| SyStrnicmp(z,"rd",2) == 0 \|\| SyStrnicmp(z,"th",2) == 0;` |
|     38 |  321 | `}` |
|      - |  322 | `/*` |
|      - |  323 | ` * Try to read a textual-month date at z, in either order:` |
|      - |  324 | ` *   MonthName [Day] [Year]   ("Jan 15 2020", "January", "January 2020")` |
|      - |  325 | ` *   Day MonthName [Year]     ("15 January 2020", "15th Jan")` |
|      - |  326 | ` * A missing day defaults to 1, a missing year to the base timestamp's year (php).` |
|      - |  327 | ` * Day may carry an ordinal suffix, fields may be comma-separated, month names are` |
|      - |  328 | ` * case-insensitive, and an optional time-of-day suffix + trailing UTC/GMT is` |
|      - |  329 | ` * consumed. Returns 0 (not a month date — caller falls through, *pzOut untouched),` |
|      - |  330 | ` * 1 on success, or a DtParse error code (out-of-range day).` |
|      - |  331 | ` */` |
|    224 |  332 | `static int DtTryMonthDate(const char *z,const char *zEnd,const char **pzOut,` |
|      - |  333 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,sxi64 iBaseTs,int *pUs)` |
|      1 |  334 | `{` |
|    225 |  335 | `	int mo,d = 1,adv,haveDay = 0,haveYear = 0;` |
|    225 |  336 | `	sxi64 y = 0;` |
|    225 |  337 | `	int h = 0,mi = 0,s = 0,us = 0;` |
|    225 |  338 | `	sxi32 iOff = *pOff;` |
|      - |  339 | `	int rcT;` |
|      - |  340 | `#define MDSKIPWS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|    225 |  341 | `	if( (mo = DtMatchMonth(z,zEnd,&adv)) != 0 ){` |
|      - |  342 | `		/* MonthName [Day] [Year]. A 4-digit number here is the YEAR, not the day` |
|      - |  343 | `		 * ("January 2020" is month+year, day defaults); a 1-2 digit number is the day. */` |
|     31 |  344 | `		z += adv;` |
|     76 |  345 | `		MDSKIPWS();` |
|     31 |  346 | `		if( z < zEnd && SyisDigit(z[0]) ){` |
|     31 |  347 | `			int nrun = 0;` |
|     31 |  348 | `			const char *zp = z;` |
|     95 |  349 | `			while( zp < zEnd && SyisDigit(zp[0]) && nrun < 4 ){ zp++; nrun++; }` |
|     31 |  350 | `			if( nrun < 4 ){` |
|     27 |  351 | `				d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|     27 |  352 | `				if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|     27 |  353 | `				haveDay = 1;` |
|     68 |  354 | `				MDSKIPWS();` |
|     13 |  355 | `			}` |
|     16 |  356 | `		}` |
|    210 |  357 | `	}else if( SyisDigit(z[0]) ){` |
|      - |  358 | `		/* Day MonthName [Year] */` |
|     49 |  359 | `		d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|     49 |  360 | `		if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|     49 |  361 | `		haveDay = 1;` |
|    107 |  362 | `		MDSKIPWS();` |
|     49 |  363 | `		if( (mo = DtMatchMonth(z,zEnd,&adv)) == 0 ){ return 0; }` |
|     21 |  364 | `		z += adv;` |
|     49 |  365 | `		MDSKIPWS();` |
|     11 |  366 | `	}else{` |
|    147 |  367 | `		return 0;` |
|      - |  368 | `	}` |
|      - |  369 | `	/* optional year */` |
|     51 |  370 | `	if( z < zEnd && SyisDigit(z[0]) ){` |
|     47 |  371 | `		int ny = 0;` |
|     47 |  372 | `		y = 0;` |
|    231 |  373 | `		while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ y = y*10 + (z[0]-'0'); z++; ny++; }` |
|     47 |  374 | `		if( ny <= 2 ){` |
|    ! 0 |  375 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|    ! 0 |  376 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|    ! 0 |  377 | `		}` |
|     47 |  378 | `		haveYear = 1;` |
|     23 |  379 | `	}` |
|      - |  380 | `	/* Default the unspecified fields from the base timestamp. php overlays: a` |
|      - |  381 | `	 * missing year takes the base year; a missing day is 1 when a year WAS given` |
|      - |  382 | `	 * ("January 2020" -> the 1st) but the base day when only the month was named` |
|      - |  383 | `	 * ("January" -> the base day). */` |
|      - |  384 | `	{` |
|      - |  385 | `		sxi64 by; int bm,bd;` |
|     51 |  386 | `		DtCivilFromDays(DtFloorDiv(iBaseTs + *pOff,86400),&by,&bm,&bd);` |
|     51 |  387 | `		if( !haveYear ){ y = by; }` |
|     51 |  388 | `		if( !haveDay ){ d = haveYear ? 1 : bd; }` |
|      - |  389 | `	}` |
|     51 |  390 | `	if( d > 31 ){ return (int)(z - zIn) + 1; }` |
|      - |  391 | `	/* optional time-of-day suffix */` |
|     51 |  392 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff,&us);` |
|     51 |  393 | `	if( rcT != 0 ){ return rcT; }` |
|      - |  394 | `	/* optional trailing UTC/GMT zone name (PHL's default zone is already UTC) */` |
|     55 |  395 | `	MDSKIPWS();` |
|     50 |  396 | `	if( (zEnd-z >= 3 && SyStrnicmp(z,"utc",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3])))` |
|     49 |  397 | `	 \|\| (zEnd-z >= 3 && SyStrnicmp(z,"gmt",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3]))) ){` |
|      3 |  398 | `		iOff = 0; z += 3;` |
|      1 |  399 | `	}` |
|     51 |  400 | `	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|     51 |  401 | `	*pOff = iOff;` |
|     51 |  402 | `	*pUs = us;` |
|     51 |  403 | `	*pzOut = z;` |
|     51 |  404 | `	return 1;` |
|      - |  405 | `#undef MDSKIPWS` |
|    113 |  406 | `}` |
|      - |  407 | `/*` |
|      - |  408 | ` * Minimal php-datetime-string parser (slice 1): absolute forms` |
|      - |  409 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]",` |
|      - |  410 | ` * keywords today/midnight/noon/tomorrow/yesterday, and relative sequences` |
|      - |  411 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". Returns 0 on` |
|      - |  412 | ` * success (ts/off/bOffSet out), or the byte position of the first` |
|      - |  413 | ` * unparseable character +1 (for php's "at position N" message).` |
|      - |  414 | ` */` |
|    686 |  415 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,` |
|      - |  416 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs)` |
|      2 |  417 | `{` |
|    688 |  418 | `	const char *z = zIn, *zEnd = &zIn[nLen];` |
|    688 |  419 | `	sxi64 iTs = iBaseTs;` |
|    688 |  420 | `	sxi32 iOff = iBaseOff;` |
|    688 |  421 | `	int bOffSet = 0;` |
|    688 |  422 | `	int bAny = 0;` |
|      - |  423 | `	int iNumRc,iMonRc;` |
|    688 |  424 | `	int uSec = 0;` |
|    688 |  425 | `	*pUs = 0;` |
|      - |  426 | `#define DT_SKIP_WS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|      - |  427 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|      - |  428 | `	&& (zEnd-z == (nKw) \|\| !SyisAlpha(z[(nKw)])))` |
|   1038 |  429 | `	DT_SKIP_WS();` |
|    688 |  430 | `	if( z >= zEnd ){` |
|      - |  431 | `		/* php: the empty string is "now" */` |
|      3 |  432 | `		*pTs = iTs;` |
|      3 |  433 | `		*pOff = iOff;` |
|      3 |  434 | `		*pbOffSet = bOffSet;` |
|      3 |  435 | `		return 0;` |
|      - |  436 | `	}` |
|      - |  437 | `	/* "@<seconds>" absolute epoch */` |
|    686 |  438 | `	if( z[0] == '@' ){` |
|     39 |  439 | `		int neg = 0;` |
|     39 |  440 | `		sxi64 v = 0;` |
|     39 |  441 | `		const char *zAt = z;` |
|     39 |  442 | `		z++;` |
|     39 |  443 | `		if( z < zEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|      - |  444 | `		/* php's lexer rejects the whole token: the error points at the '@' */` |
|     39 |  445 | `		if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zAt - zIn) + 1; }` |
|    141 |  446 | `		while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|      - |  447 | `		/* php accepts a fractional epoch ("@1600000000.5" -> .5s = 500000us) */` |
|     37 |  448 | `		if( z < zEnd && z[0]=='.' && zEnd-z >= 2 && SyisDigit(z[1]) ){` |
|      3 |  449 | `			*pUs = DtReadFraction(&z,zEnd);` |
|      1 |  450 | `		}` |
|     37 |  451 | `		*pTs = neg ? -v : v;` |
|     37 |  452 | `		*pOff = 0;` |
|     37 |  453 | `		*pbOffSet = 1;` |
|     37 |  454 | `		DT_SKIP_WS();` |
|     37 |  455 | `		return (z < zEnd) ? (int)(z - zIn) + 1 : 0;` |
|      - |  456 | `	}` |
|      - |  457 | `	/* Absolute date: YYYY-MM-DD[...] */` |
|    646 |  458 | `	if( zEnd-z >= 10 && SyisDigit(z[0]) && SyisDigit(z[1]) && SyisDigit(z[2])` |
|    376 |  459 | `	 && SyisDigit(z[3]) && z[4]=='-' ){` |
|    340 |  460 | `		sxi64 y = (z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0');` |
|    340 |  461 | `		int mo,d,h=0,mi=0,s=0;` |
|    340 |  462 | `		if( !SyisDigit(z[5])\|\|!SyisDigit(z[6])\|\|z[7] != '-'\|\|!SyisDigit(z[8])\|\|!SyisDigit(z[9]) ){` |
|    ! 0 |  463 | `			return (int)(z - zIn) + 1;` |
|      - |  464 | `		}` |
|    340 |  465 | `		mo = (z[5]-'0')*10 + (z[6]-'0');` |
|    340 |  466 | `		d  = (z[8]-'0')*10 + (z[9]-'0');` |
|      - |  467 | `		/* php's lexer dies on the SECOND digit of an out-of-range month/day` |
|      - |  468 | `		 * (either the two-digit pattern fails there, or a one-digit component` |
|      - |  469 | `		 * matched and the separator check fails there); "00" lexes fine and` |
|      - |  470 | `		 * normalizes (month 0 == December of the previous year). */` |
|    340 |  471 | `		if( mo > 12 ){ return (int)(&z[6] - zIn) + 1; }` |
|    334 |  472 | `		if( d > 31 ){ return (int)(&z[9] - zIn) + 1; }` |
|    330 |  473 | `		if( mo == 0 ){ mo = 12; y--; }` |
|    330 |  474 | `		z += 10;` |
|      - |  475 | `		{` |
|    330 |  476 | `			int rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,&bOffSet,&uSec);` |
|    330 |  477 | `			if( rcT != 0 ){ return rcT; }` |
|      - |  478 | `		}` |
|    324 |  479 | `		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    324 |  480 | `		bAny = 1;` |
|    471 |  481 | `	}else if( SyisDigit(z[0])` |
|    207 |  482 | `	 && (iNumRc = DtTryNumericDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,&uSec)) != 0 ){` |
|      - |  483 | `		/* DD-MM-YYYY / DD.MM.YYYY (day first), MM/DD/YYYY (slash, American), and` |
|      - |  484 | `		 * YYYY/MM/DD (slash, year first) — see DtTryNumericDate. Anything other than` |
|      - |  485 | `		 * 1 is an error code in DtParse's own convention (positive position / negative` |
|      - |  486 | `		 * "double time"); propagate it verbatim. */` |
|     55 |  487 | `		if( iNumRc != 1 ){ return iNumRc; }` |
|     47 |  488 | `		bAny = 1;` |
|    278 |  489 | `	}else if( (SyisAlpha(z[0]) \|\| SyisDigit(z[0]))` |
|    241 |  490 | `	 && (iMonRc = DtTryMonthDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,iBaseTs,&uSec)) != 0 ){` |
|      - |  491 | `		/* MonthName Day Year / Day MonthName Year, in any of php's spellings. As with` |
|      - |  492 | `		 * DtTryNumericDate, anything other than 1 is an error code to propagate. */` |
|     51 |  493 | `		if( iMonRc != 1 ){ return iMonRc; }` |
|     51 |  494 | `		bAny = 1;` |
|    230 |  495 | `	}else if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|     15 |  496 | `	 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|      - |  497 | `		/* Time-only: HH:MM[:SS] on the base date */` |
|     11 |  498 | `		sxi64 t = iTs + iOff;` |
|     11 |  499 | `		sxi64 days = DtFloorDiv(t,86400);` |
|     11 |  500 | `		int h  = (z[0]-'0')*10 + (z[1]-'0');` |
|     11 |  501 | `		int mi = (z[3]-'0')*10 + (z[4]-'0');` |
|     11 |  502 | `		int s = 0;` |
|      - |  503 | `		/* php: bad hour kills the token (error at its start); bad minute /` |
|      - |  504 | `		 * second dies on the component's second digit */` |
|     11 |  505 | `		if( h > 24 ){ return (int)(z - zIn) + 1; }` |
|      9 |  506 | `		if( mi > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|      7 |  507 | `		z += 5;` |
|      7 |  508 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|      5 |  509 | `			s = (z[1]-'0')*10 + (z[2]-'0');` |
|      5 |  510 | `			if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|      3 |  511 | `			z += 3;` |
|      1 |  512 | `		}` |
|      5 |  513 | `		iTs = days*86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|      5 |  514 | `		bAny = 1;` |
|    197 |  515 | `	}else if( DT_LOWEQ("now",3) ){` |
|     21 |  516 | `		z += 3;` |
|     21 |  517 | `		bAny = 1;` |
|     10 |  518 | `	}` |
|      - |  519 | `	/* Relative / keyword sequence */` |
|    305 |  520 | `	for(;;){` |
|    929 |  521 | `		DT_SKIP_WS();` |
|    788 |  522 | `		if( z >= zEnd ){` |
|    592 |  523 | `			break;` |
|      - |  524 | `		}` |
|    197 |  525 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|      9 |  526 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|      9 |  527 | `			iTs = days*86400 - iOff;` |
|      9 |  528 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|      9 |  529 | `			bAny = 1;` |
|      9 |  530 | `			continue;` |
|      - |  531 | `		}` |
|    189 |  532 | `		if( DT_LOWEQ("noon",4) ){` |
|      3 |  533 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|      3 |  534 | `			iTs = days*86400 + 12*3600 - iOff;` |
|      3 |  535 | `			z += 4;` |
|      3 |  536 | `			bAny = 1;` |
|      3 |  537 | `			continue;` |
|      - |  538 | `		}` |
|    187 |  539 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|      3 |  540 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) + 1;` |
|      3 |  541 | `			iTs = days*86400 - iOff;` |
|      3 |  542 | `			z += 8;` |
|      3 |  543 | `			bAny = 1;` |
|      3 |  544 | `			continue;` |
|      - |  545 | `		}` |
|    185 |  546 | `		if( DT_LOWEQ("yesterday",9) ){` |
|      3 |  547 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) - 1;` |
|      3 |  548 | `			iTs = days*86400 - iOff;` |
|      3 |  549 | `			z += 9;` |
|      3 |  550 | `			bAny = 1;` |
|      3 |  551 | `			continue;` |
|      - |  552 | `		}` |
|      - |  553 | `		/* Weekday navigation: "[next\|last\|previous\|this] <weekday>" moves to the` |
|      - |  554 | `		 * midnight of the target weekday. Bare/"this" = the this-week occurrence on` |
|      - |  555 | `		 * or after the base day; "next"/"last"/"previous" skip a matching base day. */` |
|      - |  556 | `		{` |
|    183 |  557 | `			const char *zSave = z;` |
|    183 |  558 | `			int dir = 0;         /* 0 = this-week occurrence, 1 = next, -1 = last */` |
|      - |  559 | `			int adv,dow;` |
|    219 |  560 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; DT_SKIP_WS(); }` |
|    162 |  561 | `			else if( DT_LOWEQ("previous",8) ){ dir = -1; z += 8; DT_SKIP_WS(); }` |
|    205 |  562 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; DT_SKIP_WS(); }` |
|    140 |  563 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; DT_SKIP_WS(); }` |
|    183 |  564 | `			dow = DtMatchWeekday(z,zEnd,&adv);` |
|    183 |  565 | `			if( dow >= 0 ){` |
|     47 |  566 | `				sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     47 |  567 | `				int bdow = (int)(((days + 4) % 7 + 7) % 7); /* 1970-01-01 was Thursday */` |
|      - |  568 | `				sxi64 delta;` |
|     47 |  569 | `				if( dir == 1 ){` |
|     15 |  570 | `					delta = ((dow - bdow) % 7 + 7) % 7;` |
|     15 |  571 | `					if( delta == 0 ){ delta = 7; }` |
|     40 |  572 | `				}else if( dir == -1 ){` |
|     13 |  573 | `					delta = -(((bdow - dow) % 7 + 7) % 7);` |
|     13 |  574 | `					if( delta == 0 ){ delta = -7; }` |
|      7 |  575 | `				}else{` |
|     21 |  576 | `					delta = ((dow - bdow) % 7 + 7) % 7;` |
|      - |  577 | `				}` |
|     47 |  578 | `				iTs = (days + delta)*86400 - iOff; /* midnight of the target day */` |
|     47 |  579 | `				z += adv;` |
|     47 |  580 | `				bAny = 1;` |
|     47 |  581 | `				continue;` |
|      - |  582 | `			}` |
|    137 |  583 | `			z = zSave; /* prefix did not introduce a weekday: rewind and try the rest */` |
|      - |  584 | `		}` |
|      - |  585 | `		/* "first\|last day of (this\|next\|last month \| MonthName [Year])": jump to the` |
|      - |  586 | `		 * first or last day of a target month. A this/next/last-month target keeps the` |
|      - |  587 | `		 * base time-of-day; an absolute MonthName [Year] target resets it to midnight` |
|      - |  588 | `		 * (php). */` |
|    137 |  589 | `		if( DT_LOWEQ("first",5) \|\| DT_LOWEQ("last",4) ){` |
|     39 |  590 | `			const char *zSave = z;` |
|     39 |  591 | `			int bFirst = (SyToLower((unsigned char)z[0]) == 'f');` |
|     39 |  592 | `			z += bFirst ? 5 : 4;` |
|     96 |  593 | `			DT_SKIP_WS();` |
|     39 |  594 | `			if( DT_LOWEQ("day",3) ){` |
|     31 |  595 | `				z += 3;` |
|     76 |  596 | `				DT_SKIP_WS();` |
|     31 |  597 | `				if( DT_LOWEQ("of",2) ){` |
|     31 |  598 | `					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);` |
|      - |  599 | `					sxi64 yy,tod;` |
|     31 |  600 | `					int mm,dd0,keepTime = 1,ok = 1;` |
|     31 |  601 | `					z += 2;` |
|     74 |  602 | `					DT_SKIP_WS();` |
|     31 |  603 | `					DtCivilFromDays(days0,&yy,&mm,&dd0);` |
|     31 |  604 | `					tod = (iTs + iOff) - days0*86400;` |
|     40 |  605 | `					if( DT_LOWEQ("this",4) ){ z += 4; DT_SKIP_WS();` |
|      7 |  606 | `						if( DT_LOWEQ("month",5) ){ z += 5; }else{ ok = 0; } }` |
|     34 |  607 | `					else if( DT_LOWEQ("next",4) ){ z += 4; DT_SKIP_WS();` |
|      7 |  608 | `						if( DT_LOWEQ("month",5) ){ z += 5; mm++; if(mm>12){ mm=1; yy++; } }else{ ok = 0; } }` |
|     25 |  609 | `					else if( DT_LOWEQ("last",4) ){ z += 4; DT_SKIP_WS();` |
|      5 |  610 | `						if( DT_LOWEQ("month",5) ){ z += 5; mm--; if(mm<1){ mm=12; yy--; } }else{ ok = 0; } }` |
|     15 |  611 | `					else if( z < zEnd ){` |
|      - |  612 | `						int mo,adv;` |
|     13 |  613 | `						mo = DtMatchMonth(z,zEnd,&adv);` |
|     13 |  614 | `						if( mo == 0 ){ return (int)(z - zIn) + 1; }` |
|     27 |  615 | `						z += adv; DT_SKIP_WS();` |
|     13 |  616 | `						mm = mo; keepTime = 0; tod = 0;` |
|     13 |  617 | `						if( z < zEnd && SyisDigit(z[0]) ){` |
|      9 |  618 | `							int ny = 0; sxi64 yv = 0;` |
|     41 |  619 | `							while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ yv = yv*10 + (z[0]-'0'); z++; ny++; }` |
|      9 |  620 | `							if( ny <= 2 ){ if( yv <= 69 ){ yv += 2000; } else if( yv <= 99 ){ yv += 1900; } }` |
|      9 |  621 | `							yy = yv;` |
|      4 |  622 | `						}` |
|      6 |  623 | `					}` |
|      - |  624 | `					/* else: "... day of" with nothing after — php defaults to this` |
|      - |  625 | `					 * month (mm/yy/tod stay the base, keepTime stays 1). */` |
|     31 |  626 | `					if( ok ){` |
|     31 |  627 | `						int dim = (int)(DtDaysFromCivil(yy,mm+1,1) - DtDaysFromCivil(yy,mm,1));` |
|     31 |  628 | `						int day = bFirst ? 1 : dim;` |
|     31 |  629 | `						iTs = DtDaysFromCivil(yy,mm,day)*86400 + (keepTime ? tod : 0) - iOff;` |
|     31 |  630 | `						bAny = 1;` |
|     31 |  631 | `						continue;` |
|      - |  632 | `					}` |
|    ! 0 |  633 | `				}` |
|    ! 0 |  634 | `			}` |
|      9 |  635 | `			z = zSave; /* not the "first\|last day of ..." shape: rewind */` |
|      4 |  636 | `		}` |
|      - |  637 | `		/* Standalone "this\|next\|last (month\|week)": month shifts by ±1 keeping the` |
|      - |  638 | `		 * day/time; week moves to the Monday of this/next/last ISO week keeping the` |
|      - |  639 | `		 * time-of-day (php: weeks start on Monday). */` |
|      - |  640 | `		{` |
|    129 |  641 | `			const char *zSave = z;` |
|    129 |  642 | `			int dir = 2; /* 2 = no prefix */` |
|    129 |  643 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; }` |
|     99 |  644 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; }` |
|     91 |  645 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; }` |
|    109 |  646 | `			if( dir != 2 ){` |
|     66 |  647 | `				DT_SKIP_WS();` |
|     27 |  648 | `				if( DT_LOWEQ("month",5) ){` |
|     13 |  649 | `					z += 5;` |
|     13 |  650 | `					iTs = DtAddMonths(iTs,iOff,dir);` |
|     13 |  651 | `					bAny = 1;` |
|     13 |  652 | `					continue;` |
|      - |  653 | `				}` |
|     15 |  654 | `				if( DT_LOWEQ("week",4) ){` |
|     13 |  655 | `					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);` |
|     13 |  656 | `					sxi64 tod = (iTs + iOff) - days0*86400;` |
|     13 |  657 | `					int bdow = (int)(((days0 + 4) % 7 + 7) % 7);` |
|     13 |  658 | `					sxi64 monday = days0 - ((bdow + 6) % 7); /* Monday of the base week */` |
|     13 |  659 | `					z += 4;` |
|     13 |  660 | `					monday += (sxi64)dir * 7;` |
|     13 |  661 | `					iTs = monday*86400 + tod - iOff;` |
|     13 |  662 | `					bAny = 1;` |
|     13 |  663 | `					continue;` |
|      - |  664 | `				}` |
|      1 |  665 | `			}` |
|     85 |  666 | `			z = zSave;` |
|      - |  667 | `		}` |
|      - |  668 | `		/* Trailing time-of-day in a relative sequence ("next thursday 15:00"): set` |
|      - |  669 | `		 * the clock on the current day. The leading absolute HH:MM branch handles a` |
|      - |  670 | `		 * time at the START; this handles one AFTER a date/relative token. */` |
|     84 |  671 | `		if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|     17 |  672 | `		 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|     13 |  673 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     13 |  674 | `			int hh = (z[0]-'0')*10 + (z[1]-'0');` |
|     13 |  675 | `			int mm = (z[3]-'0')*10 + (z[4]-'0');` |
|     13 |  676 | `			int ss = 0;` |
|     13 |  677 | `			if( hh > 24 ){ return (int)(z - zIn) + 1; }` |
|     13 |  678 | `			if( mm > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|     13 |  679 | `			z += 5;` |
|     13 |  680 | `			if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|      5 |  681 | `				ss = (z[1]-'0')*10 + (z[2]-'0');` |
|      5 |  682 | `				if( ss > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|      5 |  683 | `				z += 3;` |
|      2 |  684 | `			}` |
|     13 |  685 | `			iTs = days*86400 + (sxi64)hh*3600 + (sxi64)mm*60 + ss - iOff;` |
|     13 |  686 | `			bAny = 1;` |
|     13 |  687 | `			continue;` |
|      - |  688 | `		}` |
|     73 |  689 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|     55 |  690 | `			int neg = 0;` |
|     55 |  691 | `			sxi64 v = 0;` |
|     55 |  692 | `			const char *zNumStart = z;` |
|     55 |  693 | `			if( z[0]=='+' \|\| z[0]=='-' ){` |
|     35 |  694 | `				neg = (z[0]=='-');` |
|     35 |  695 | `				z++;` |
|      - |  696 | `				/* php's lexer takes the sign as its own token, so whitespace may` |
|      - |  697 | `				 * follow it: "1 year + 3 months" is a relative sequence there and` |
|      - |  698 | `				 * was a parse FAILURE here. */` |
|     56 |  699 | `				DT_SKIP_WS();` |
|     17 |  700 | `			}` |
|     55 |  701 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zNumStart - zIn) + 1; }` |
|    127 |  702 | `			while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|     55 |  703 | `			if( neg ){ v = -v; }` |
|    134 |  704 | `			DT_SKIP_WS();` |
|     55 |  705 | `			if( DT_LOWEQ("seconds",7) )     { iTs += v;            z += 7; }` |
|     55 |  706 | `			else if( DT_LOWEQ("second",6) ) { iTs += v;            z += 6; }` |
|     55 |  707 | `			else if( DT_LOWEQ("secs",4) )   { iTs += v;            z += 4; }` |
|     55 |  708 | `			else if( DT_LOWEQ("sec",3) )    { iTs += v;            z += 3; }` |
|     55 |  709 | `			else if( DT_LOWEQ("minutes",7) ){ iTs += v*60;         z += 7; }` |
|     53 |  710 | `			else if( DT_LOWEQ("minute",6) ) { iTs += v*60;         z += 6; }` |
|     53 |  711 | `			else if( DT_LOWEQ("mins",4) )   { iTs += v*60;         z += 4; }` |
|     53 |  712 | `			else if( DT_LOWEQ("min",3) )    { iTs += v*60;         z += 3; }` |
|     53 |  713 | `			else if( DT_LOWEQ("hours",5) )  { iTs += v*3600;       z += 5; }` |
|     47 |  714 | `			else if( DT_LOWEQ("hour",4) )   { iTs += v*3600;       z += 4; }` |
|     47 |  715 | `			else if( DT_LOWEQ("days",4) )   { iTs += v*86400;      z += 4; }` |
|     43 |  716 | `			else if( DT_LOWEQ("day",3) )    { iTs += v*86400;      z += 3; }` |
|     33 |  717 | `			else if( DT_LOWEQ("weeks",5) )  { iTs += v*7*86400;    z += 5; }` |
|     27 |  718 | `			else if( DT_LOWEQ("week",4) )   { iTs += v*7*86400;    z += 4; }` |
|     25 |  719 | `			else if( DT_LOWEQ("fortnights",10) ){ iTs += v*14*86400; z += 10; }` |
|     23 |  720 | `			else if( DT_LOWEQ("fortnight",9) )  { iTs += v*14*86400; z += 9; }` |
|     23 |  721 | `			else if( DT_LOWEQ("months",6) ) { iTs = DtAddMonths(iTs,iOff,v); z += 6; }` |
|     19 |  722 | `			else if( DT_LOWEQ("month",5) )  { iTs = DtAddMonths(iTs,iOff,v); z += 5; }` |
|     13 |  723 | `			else if( DT_LOWEQ("years",5) )  { iTs = DtAddMonths(iTs,iOff,v*12); z += 5; }` |
|     13 |  724 | `			else if( DT_LOWEQ("year",4) )   { iTs = DtAddMonths(iTs,iOff,v*12); z += 4; }` |
|      - |  725 | `			else{` |
|      7 |  726 | `				return (int)(z - zIn) + 1;` |
|      - |  727 | `			}` |
|     49 |  728 | `			bAny = 1;` |
|     49 |  729 | `			continue;` |
|      - |  730 | `		}` |
|     19 |  731 | `		return (int)(z - zIn) + 1;` |
|    ! 0 |  732 | `	}` |
|    592 |  733 | `	if( !bAny ){` |
|    ! 0 |  734 | `		return 1;` |
|      - |  735 | `	}` |
|    592 |  736 | `	*pTs = iTs;` |
|    592 |  737 | `	*pOff = iOff;` |
|    592 |  738 | `	*pbOffSet = bOffSet;` |
|    592 |  739 | `	*pUs = uSec;` |
|    592 |  740 | `	return 0;` |
|      - |  741 | `#undef DT_SKIP_WS` |
|      - |  742 | `#undef DT_LOWEQ` |
|    344 |  743 | `}` |
|      - |  744 | `/*` |
|      - |  745 | ` * php's parse-failure reason, from DtParse's error code.` |
|      - |  746 | ` *` |
|      - |  747 | ` * The reason and the offending byte used to be formatted straight into the message` |
|      - |  748 | `` * the `__dt_parse` thunk RETURNED as a string; the constructor also has to publish`` |
|      - |  749 | ` * them as getLastErrors()'s error map now, so the decision lives here.` |
|      - |  750 | ` */` |
|     34 |  751 | `static const char * DtParseErr(const char *zIn,int nLen,int iErrPos,int *piPos,char *pcAt)` |
|      1 |  752 | `{` |
|      - |  753 | `	/* Negative encoding: php's "Double time specification" reason */` |
|     35 |  754 | `	int bDouble = iErrPos < 0;` |
|     35 |  755 | `	int iPos = (bDouble ? -iErrPos : iErrPos) - 1;` |
|     35 |  756 | `	char cAt = (iPos < nLen) ? zIn[iPos] : ' ';` |
|     35 |  757 | `	*piPos = iPos;` |
|     35 |  758 | `	*pcAt = cAt;` |
|      - |  759 | `	/* php appends a reason: an alphabetic token is assumed to be a timezone` |
|      - |  760 | `	 * lookup miss, anything else an unexpected character. */` |
|     34 |  761 | `	return bDouble ? "Double time specification"` |
|     33 |  762 | `		: ((cAt >= 'a' && cAt <= 'z') \|\| (cAt >= 'A' && cAt <= 'Z'))` |
|      - |  763 | `			? "The timezone could not be found in the database"` |
|      - |  764 | `			: "Unexpected character";` |
|      1 |  765 | `}` |
|      - |  766 | `/* Days in a civil month (php's overflow rules use it during diff borrows) */` |
|     80 |  767 | `static int DtDaysInMonth(sxi64 y,int m)` |
|      1 |  768 | `{` |
|      - |  769 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|     81 |  770 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|      7 |  771 | `		return 29;` |
|      - |  772 | `	}` |
|     75 |  773 | `	return aMonDays[(m - 1) % 12];` |
|     41 |  774 | `}` |
|      - |  775 | `/*` |
|      - |  776 | ` * php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|      - |  777 | ` * (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset.` |
|      - |  778 | ` */` |
|    208 |  779 | `static sxi64 DtCivilAdd(sxi64 iTs,sxi32 iOff,sxi64 y,sxi64 m,sxi64 d,` |
|      - |  780 | `	sxi64 h,sxi64 i,sxi64 s,int iSign)` |
|      1 |  781 | `{` |
|      - |  782 | `	sxi64 iLocal,iDays,iSecs,y0,moT,dayCount;` |
|      - |  783 | `	int mo0,d0;` |
|    209 |  784 | `	iSign = iSign < 0 ? -1 : 1;` |
|    209 |  785 | `	iLocal = iTs + iOff;` |
|    209 |  786 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    209 |  787 | `	iSecs  = iLocal - iDays*86400;` |
|    209 |  788 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    209 |  789 | `	y0 += iSign * y;` |
|    209 |  790 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    209 |  791 | `	y0 += DtFloorDiv(moT,12);` |
|    209 |  792 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    209 |  793 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    209 |  794 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    209 |  795 | `	return iLocal - iOff;` |
|      1 |  796 | `}` |
|      - |  797 | `/* One DateInterval's worth of fields, as diff() computes them. */` |
|      - |  798 | `typedef struct dt_diff dt_diff;` |
|      - |  799 | `struct dt_diff` |
|      - |  800 | `{` |
|      - |  801 | `	sxi64 y,m,d,h,i,s,uSec,nDays;` |
|      - |  802 | `	int bInvert;` |
|      - |  803 | `};` |
|      - |  804 | `/*` |
|      - |  805 | ` * timelib's diff breakdown: field-wise deltas in the FIRST operand's offset, then` |
|      - |  806 | ` * borrow seconds->minutes->hours->days, then borrow whole months for the day.` |
|      - |  807 | ` *` |
|      - |  808 | ` * That last borrow is ASYMMETRIC in php, and PHL answered the symmetric result: a` |
|      - |  809 | ` * non-inverted diff walks months BACKWARD from the later date (which is why` |
|      - |  810 | ` * Jan 31 -> Mar 02 reports m=0 d=30, not "1 month"), while an inverted one borrows` |
|      - |  811 | ` * the month of the ORIGINAL first operand — the later date — walking forward. So` |
|      - |  812 | `` * `$later->diff($earlier)` is not `$earlier->diff($later)` with the sign flipped:`` |
|      - |  813 | ` * php answers y=1 m=1 d=2 where PHL answered y=1 m=0 d=30. One iteration always` |
|      - |  814 | ` * settles the inverted case: \|d\| < 31 and the borrowed month has at least 28 days,` |
|      - |  815 | ` * while a 28-day base month can only be reached from a day-of-month <= 29.` |
|      - |  816 | ` */` |
|     56 |  817 | `static void DtCivilDiff(sxi64 iTs1,int uSec1,sxi32 iOff,sxi64 iTs2,int uSec2,dt_diff *pOut)` |
|      1 |  818 | `{` |
|      - |  819 | `	sxi64 iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|      - |  820 | `	int moA,dA,moB,dB,bInvert,usA,usB;` |
|      - |  821 | `	sxi64 sA,sB,y,m,d,h,i,s,us;` |
|      - |  822 | ``	/* The MICROSECONDS are part of which date comes first -- `$a->diff($b)` on two`` |
|      - |  823 | `	 * dates inside the same second is an INVERTED interval when $a is the later of` |
|      - |  824 | `	 * them -- and their borrow is a whole second off the later date, so every field` |
|      - |  825 | `	 * below and the day COUNT are computed from the borrowed instant: a difference` |
|      - |  826 | `	 * of one microsecond less than a day is 23:59:59.999999 with days = 0, not a` |
|      - |  827 | `	 * day. */` |
|     57 |  828 | `	bInvert = iTs1 > iTs2 \|\| (iTs1 == iTs2 && uSec1 > uSec2);` |
|     57 |  829 | `	iA = bInvert ? iTs2 : iTs1;` |
|     57 |  830 | `	iB = bInvert ? iTs1 : iTs2;` |
|     57 |  831 | `	usA = bInvert ? uSec2 : uSec1;` |
|     57 |  832 | `	usB = bInvert ? uSec1 : uSec2;` |
|     57 |  833 | `	us = usB - usA;` |
|     57 |  834 | `	if( us < 0 ){` |
|     21 |  835 | `		us += 1000000;` |
|     21 |  836 | `		iB--;` |
|     10 |  837 | `	}` |
|     57 |  838 | `	iLa = iA + iOff;` |
|     57 |  839 | `	iLb = iB + iOff;` |
|     57 |  840 | `	daysA = DtFloorDiv(iLa,86400);` |
|     57 |  841 | `	daysB = DtFloorDiv(iLb,86400);` |
|     57 |  842 | `	sA = iLa - daysA*86400;` |
|     57 |  843 | `	sB = iLb - daysB*86400;` |
|     57 |  844 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|     57 |  845 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|     57 |  846 | `	s = (sB % 60) - (sA % 60);` |
|     57 |  847 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|     57 |  848 | `	h = (sB / 3600) - (sA / 3600);` |
|     57 |  849 | `	d = dB - dA;` |
|     57 |  850 | `	m = moB - moA;` |
|     57 |  851 | `	y = yB - yA;` |
|     57 |  852 | `	if( s < 0 ){ s += 60; i--; }` |
|     57 |  853 | `	if( i < 0 ){ i += 60; h--; }` |
|     57 |  854 | `	if( h < 0 ){ h += 24; d--; }` |
|     57 |  855 | `	if( bInvert ){` |
|     35 |  856 | `		while( d < 0 ){` |
|     11 |  857 | `			d += DtDaysInMonth(yA,moA);` |
|     11 |  858 | `			m--;` |
|     11 |  859 | `			moA++;` |
|     11 |  860 | `			if( moA > 12 ){ moA = 1; yA++; }` |
|      1 |  861 | `		}` |
|     13 |  862 | `	}else{` |
|     47 |  863 | `		while( d < 0 ){` |
|     15 |  864 | `			moB--;` |
|     15 |  865 | `			if( moB < 1 ){ moB = 12; yB--; }` |
|     15 |  866 | `			d += DtDaysInMonth(yB,moB);` |
|     15 |  867 | `			m--;` |
|      1 |  868 | `		}` |
|      - |  869 | `	}` |
|     57 |  870 | `	if( m < 0 ){ m += 12; y--; }` |
|     57 |  871 | `	pOut->y = y;` |
|     57 |  872 | `	pOut->m = m;` |
|     57 |  873 | `	pOut->d = d;` |
|     57 |  874 | `	pOut->h = h;` |
|     57 |  875 | `	pOut->i = i;` |
|     57 |  876 | `	pOut->s = s;` |
|     57 |  877 | `	pOut->uSec = us;` |
|     57 |  878 | `	pOut->nDays = (iB - iA) / 86400;` |
|     57 |  879 | `	pOut->bInvert = bInvert;` |
|     57 |  880 | `}` |
|      - |  881 | `/*` |
|      - |  882 | ` * setISODate: jump to an ISO year/week/weekday, preserving the time of day.` |
|      - |  883 | ` */` |
|      8 |  884 | `static sxi64 DtIsoDate(sxi64 iTs,sxi32 iOff,sxi64 y,sxi64 w,sxi64 dow)` |
|      1 |  885 | `{` |
|      - |  886 | `	sxi64 iLocal,iTod,jan4,monday1,target;` |
|      - |  887 | `	int isoDow;` |
|      9 |  888 | `	iLocal = iTs + iOff;` |
|      9 |  889 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|      9 |  890 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|      9 |  891 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|      9 |  892 | `	monday1 = jan4 - (isoDow - 1);` |
|      9 |  893 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|      9 |  894 | `	return target*86400 + iTod - iOff;` |
|      1 |  895 | `}` |
|      - |  896 | `/* Consume nMin..nMax digits from *pz; returns count consumed (0 = failure) */` |
|    206 |  897 | `static int DtEatDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|      1 |  898 | `{` |
|    207 |  899 | `	const char *z = *pz;` |
|    207 |  900 | `	sxi64 v = 0;` |
|    207 |  901 | `	int n = 0;` |
|    733 |  902 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|    527 |  903 | `		v = v*10 + (z[0] - '0');` |
|    527 |  904 | `		z++;` |
|    527 |  905 | `		n++;` |
|      1 |  906 | `	}` |
|    207 |  907 | `	if( n < nMin ){` |
|      9 |  908 | `		return 0;` |
|      - |  909 | `	}` |
|    199 |  910 | `	*pz = z;` |
|    199 |  911 | `	*pVal = v;` |
|    199 |  912 | `	return n;` |
|    104 |  913 | `}` |
|      - |  914 | `/* timelib_get_nr's recovery: skip non-digits hunting for the field.` |
|      - |  915 | ` * Returns 1 = found+read, 0 = digits present but short, -1 = exhausted. */` |
|      8 |  916 | `static int DtHuntDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|      1 |  917 | `{` |
|      9 |  918 | `	const char *z = *pz;` |
|     45 |  919 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|      9 |  920 | `	*pz = z;` |
|      9 |  921 | `	if( z >= zEnd ){` |
|      9 |  922 | `		return -1;` |
|      - |  923 | `	}` |
|    ! 0 |  924 | `	return DtEatDigits(pz,zEnd,nMin,nMax,pVal) ? 1 : 0;` |
|      5 |  925 | `}` |
|      - |  926 | `/* Case-insensitive name-table lookup; returns 1-based index or 0 */` |
|     14 |  927 | `static int DtEatName(const char **pz,const char *zEnd,const char **azNames,int nNames)` |
|      1 |  928 | `{` |
|      - |  929 | `	int k;` |
|     23 |  930 | `	for( k = 0 ; k < nNames ; k++ ){` |
|     23 |  931 | `		int n = (int)SyStrlen(azNames[k]);` |
|     23 |  932 | `		if( zEnd - *pz >= n && SyStrnicmp(*pz,azNames[k],(sxu32)n) == 0 ){` |
|     15 |  933 | `			*pz += n;` |
|     15 |  934 | `			return k + 1;` |
|      - |  935 | `		}` |
|      5 |  936 | `	}` |
|    ! 0 |  937 | `	return 0;` |
|      8 |  938 | `}` |
|      - |  939 | `/*` |
|      - |  940 | ` * php's DateTime::createFromFormat engine.` |
|      - |  941 | ` *` |
|      - |  942 | `` * This was the `__dt_from_format()` thunk, whose answer had to survive a trip`` |
|      - |  943 | ` * through PHP: an ARRAY on success and a "COUNT\nPOS\tMESSAGE" string on failure,` |
|      - |  944 | ` * which the chunk then re-parsed. Both encodings are gone — the native methods call` |
|      - |  945 | ` * this directly and read the diagnostics as a struct. That is also why a parse that` |
|      - |  946 | ` * has BOTH errors and warnings can now report both: the failure encoding had no room` |
|      - |  947 | `` * for warnings, so php's `warning_count` was silently 0 whenever an error was present.`` |
|      - |  948 | ` *` |
|      - |  949 | ` * Returns 0 when the parse produced a time and non-zero when it did not; pOut->sDiag` |
|      - |  950 | ` * carries the warnings/errors either way (offKind: 0 none parsed, 1 numeric offset,` |
|      - |  951 | ` * 2 literal Z, 3 named identifier).` |
|      - |  952 | ` */` |
|      - |  953 | `/*` |
|      - |  954 | ` * Publish one scan's warnings and errors as the record getLastErrors() answers.` |
|      - |  955 | ` * The messages are static literals, so the record copies pointers, never bytes.` |
|      - |  956 | ` */` |
|     68 |  957 | `static void DtFfDiag(phl_dt_lasterr *pDiag,int nErr,int nErrKept,const int *aErrPos,` |
|      - |  958 | `	const char **azErr,int nWarn,const int *aWarnPos,const char **azWarn)` |
|      1 |  959 | `{` |
|      - |  960 | `	int k;` |
|     69 |  961 | `	pDiag->bSet = (sxu8)(nErr > 0 \|\| nWarn > 0);` |
|     69 |  962 | `	pDiag->nErr = nErr;` |
|     69 |  963 | `	pDiag->nErrKept = nErrKept;` |
|     89 |  964 | `	for( k = 0 ; k < nErrKept ; k++ ){` |
|     21 |  965 | `		pDiag->aErrPos[k] = aErrPos[k];` |
|     21 |  966 | `		pDiag->azErr[k] = azErr[k];` |
|     11 |  967 | `	}` |
|     69 |  968 | `	pDiag->nWarn = nWarn;` |
|     69 |  969 | `	pDiag->nWarnKept = nWarn;` |
|     75 |  970 | `	for( k = 0 ; k < nWarn ; k++ ){` |
|      7 |  971 | `		pDiag->aWarnPos[k] = aWarnPos[k];` |
|      7 |  972 | `		pDiag->azWarn[k] = azWarn[k];` |
|      4 |  973 | `	}` |
|     69 |  974 | `}` |
|      - |  975 | `typedef struct dt_ff_res dt_ff_res;` |
|      - |  976 | `struct dt_ff_res` |
|      - |  977 | `{` |
|      - |  978 | `	sxi64 iTs;` |
|      - |  979 | `	sxi32 iOff;` |
|      - |  980 | `	int iOffKind;` |
|      - |  981 | `	char zName[16];` |
|      - |  982 | `	int uSec;` |
|      - |  983 | `	int bHasUs;` |
|      - |  984 | `	phl_dt_lasterr sDiag;` |
|      - |  985 | `};` |
|     68 |  986 | `static int DtFromFormat(const char *zFmt,int nFmt,const char *zIn,int nIn,` |
|      - |  987 | `	sxi64 iNow,sxi32 iDefOff,dt_ff_res *pOut)` |
|      1 |  988 | `{` |
|      - |  989 | `	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};` |
|      - |  990 | `	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",` |
|      - |  991 | `		"thursday","friday","saturday"};` |
|      - |  992 | `	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",` |
|      - |  993 | `		"aug","sep","oct","nov","dec"};` |
|      - |  994 | `	static const char *azMonFull[] = {"january","february","march","april",` |
|      - |  995 | `		"may","june","july","august","september","october","november","december"};` |
|      - |  996 | `	const char *zEnd,*zInEnd,*z;` |
|      - |  997 | `	sxi64 v;` |
|      - |  998 | `	/* -1 == unset */` |
|     69 |  999 | `	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;` |
|     69 | 1000 | `	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;` |
|     69 | 1001 | `	int uSecFF = 0,bHasUs = 0;` |
|     69 | 1002 | `	int iOffKind = 0;` |
|     69 | 1003 | `	sxi32 iOffVal = 0;` |
|      - | 1004 | `	char zName[16];` |
|     69 | 1005 | `	const char *zErr = 0;` |
|      - | 1006 | `	const char *aWarnMsg[PH7_DT_MAX_WARN];` |
|      - | 1007 | `	int aWarnPos[PH7_DT_MAX_WARN];` |
|     69 | 1008 | `	int nWarn = 0,bAborted = 0;` |
|      - | 1009 | `	const char *aErrMsg[PH7_DT_MAX_ERR];` |
|      - | 1010 | `	int aErrPos[PH7_DT_MAX_ERR];` |
|     69 | 1011 | `	int nErr = 0,nErrKept = 0;` |
|     69 | 1012 | `	SyZero(pOut,sizeof(*pOut));` |
|     69 | 1013 | `	zEnd = &zFmt[nFmt];` |
|     69 | 1014 | `	zInEnd = &zIn[nIn];` |
|     69 | 1015 | `	z = zIn;` |
|     69 | 1016 | `	zName[0] = 0;` |
|      - | 1017 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|      - | 1018 | `	{ int _p = (iPos),_k,_f = -1; \` |
|      - | 1019 | `	  nErr++; \` |
|      - | 1020 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|      - | 1021 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|      - | 1022 | `	  else if( nErrKept < PH7_DT_MAX_ERR ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|    455 | 1023 | `	while( zFmt < zEnd ){` |
|    395 | 1024 | `		char c = zFmt[0];` |
|    395 | 1025 | `		zFmt++;` |
|    395 | 1026 | `		zErr = 0;` |
|    395 | 1027 | `		if( c == '!' ){` |
|     11 | 1028 | `			y = 1970; mo = 1; d = 1; h = 0; mi = 0; s = 0;` |
|     11 | 1029 | `			h12 = -1; iMeridiem = -1;` |
|     11 | 1030 | `			continue;` |
|      - | 1031 | `		}` |
|    385 | 1032 | `		if( c == '\|' ){ bPipe = 1; continue; }` |
|    381 | 1033 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|    379 | 1034 | `		if( z >= zInEnd ){` |
|      - | 1035 | `			/* timelib aborts the scan once input is exhausted */` |
|     17 | 1036 | `			DT_FF_LOGERR(nIn,"Not enough data available to satisfy format");` |
|      9 | 1037 | `			break;` |
|      - | 1038 | `		}` |
|    371 | 1039 | `		switch( c ){` |
|     24 | 1040 | `		case 'd': case 'j':` |
|     49 | 1041 | `			if( !DtEatDigits(&z,zInEnd,1,2,&d) ){` |
|    ! 0 | 1042 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit day could not be found");` |
|    ! 0 | 1043 | `				if( DtHuntDigits(&z,zInEnd,1,2,&d) < 0 ){` |
|    ! 0 | 1044 | `					DT_FF_LOGERR(nIn,"A two digit day could not be found");` |
|    ! 0 | 1045 | `				}` |
|    ! 0 | 1046 | `			}` |
|     49 | 1047 | `			break;` |
|      1 | 1048 | `		case 'D':` |
|      3 | 1049 | `			if( !DtEatName(&z,zInEnd,azDay3,7) ){` |
|    ! 0 | 1050 | `				zErr = "A textual day could not be found";` |
|    ! 0 | 1051 | `			}` |
|      3 | 1052 | `			break;` |
|      1 | 1053 | `		case 'l':` |
|      3 | 1054 | `			if( !DtEatName(&z,zInEnd,azDayFull,7) ){` |
|    ! 0 | 1055 | `				zErr = "A textual day could not be found";` |
|    ! 0 | 1056 | `			}` |
|      3 | 1057 | `			break;` |
|      1 | 1058 | `		case 'S':` |
|      - | 1059 | `			/* ordinal suffix: st nd rd th */` |
|      4 | 1060 | `			if( zInEnd-z >= 2 && ((z[0]=='s'&&z[1]=='t')\|\|(z[0]=='n'&&z[1]=='d')` |
|      2 | 1061 | `			 \|\|(z[0]=='r'&&z[1]=='d')\|\|(z[0]=='t'&&z[1]=='h')) ){` |
|      3 | 1062 | `				z += 2;` |
|      1 | 1063 | `			}` |
|      3 | 1064 | `			break;` |
|     22 | 1065 | `		case 'm': case 'n':` |
|     45 | 1066 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mo) ){` |
|    ! 0 | 1067 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit month could not be found");` |
|    ! 0 | 1068 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mo) < 0 ){` |
|    ! 0 | 1069 | `					DT_FF_LOGERR(nIn,"A two digit month could not be found");` |
|    ! 0 | 1070 | `				}` |
|    ! 0 | 1071 | `			}` |
|     45 | 1072 | `			break;` |
|      1 | 1073 | `		case 'M':{` |
|      3 | 1074 | `			int k = DtEatName(&z,zInEnd,azMon3,12);` |
|      3 | 1075 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|      3 | 1076 | `			break;` |
|      - | 1077 | `				 }` |
|      1 | 1078 | `		case 'F':{` |
|      3 | 1079 | `			int k = DtEatName(&z,zInEnd,azMonFull,12);` |
|      3 | 1080 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|      3 | 1081 | `			break;` |
|      - | 1082 | `				 }` |
|    ! 0 | 1083 | `		case 'y':` |
|    ! 0 | 1084 | `			if( DtEatDigits(&z,zInEnd,2,2,&y) ){` |
|    ! 0 | 1085 | `				y += (y <= 69) ? 2000 : 1900;` |
|    ! 0 | 1086 | `			}else{` |
|    ! 0 | 1087 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit year could not be found");` |
|    ! 0 | 1088 | `				if( DtHuntDigits(&z,zInEnd,2,2,&y) < 0 ){` |
|    ! 0 | 1089 | `					DT_FF_LOGERR(nIn,"A two digit year could not be found");` |
|    ! 0 | 1090 | `				}else if( y >= 0 ){` |
|    ! 0 | 1091 | `					y += (y <= 69) ? 2000 : 1900;` |
|    ! 0 | 1092 | `				}` |
|      - | 1093 | `			}` |
|    ! 0 | 1094 | `			break;` |
|     29 | 1095 | `		case 'Y':{` |
|     59 | 1096 | `			int neg = 0;` |
|     59 | 1097 | `			if( z < zInEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|     59 | 1098 | `			if( DtEatDigits(&z,zInEnd,1,4,&y) ){` |
|     51 | 1099 | `				if( neg ){ y = -y; }` |
|     26 | 1100 | `			}else{` |
|      9 | 1101 | `				DT_FF_LOGERR((int)(z - zIn),"A four digit year could not be found");` |
|      9 | 1102 | `				if( DtHuntDigits(&z,zInEnd,1,4,&y) < 0 ){` |
|     17 | 1103 | `					DT_FF_LOGERR(nIn,"A four digit year could not be found");` |
|      4 | 1104 | `				}` |
|      - | 1105 | `			}` |
|     59 | 1106 | `			break;` |
|      - | 1107 | `				 }` |
|      7 | 1108 | `		case 'H': case 'G':` |
|     15 | 1109 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h) ){` |
|    ! 0 | 1110 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|    ! 0 | 1111 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h) < 0 ){` |
|    ! 0 | 1112 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|    ! 0 | 1113 | `				}` |
|    ! 0 | 1114 | `			}` |
|     15 | 1115 | `			break;` |
|      2 | 1116 | `		case 'h': case 'g':` |
|      5 | 1117 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h12) ){` |
|    ! 0 | 1118 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|    ! 0 | 1119 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h12) < 0 ){` |
|    ! 0 | 1120 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|    ! 0 | 1121 | `				}` |
|    ! 0 | 1122 | `			}` |
|      5 | 1123 | `			break;` |
|      9 | 1124 | `		case 'i':` |
|     19 | 1125 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mi) ){` |
|    ! 0 | 1126 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit minute could not be found");` |
|    ! 0 | 1127 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mi) < 0 ){` |
|    ! 0 | 1128 | `					DT_FF_LOGERR(nIn,"A two digit minute could not be found");` |
|    ! 0 | 1129 | `				}` |
|    ! 0 | 1130 | `			}` |
|     19 | 1131 | `			break;` |
|      3 | 1132 | `		case 's':` |
|      7 | 1133 | `			if( !DtEatDigits(&z,zInEnd,1,2,&s) ){` |
|    ! 0 | 1134 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit second could not be found");` |
|    ! 0 | 1135 | `				if( DtHuntDigits(&z,zInEnd,1,2,&s) < 0 ){` |
|    ! 0 | 1136 | `					DT_FF_LOGERR(nIn,"A two digit second could not be found");` |
|    ! 0 | 1137 | `				}` |
|    ! 0 | 1138 | `			}` |
|      7 | 1139 | `			break;` |
|      1 | 1140 | `		case 'u':{` |
|      - | 1141 | `			/* Microseconds: the digits parsed are right-padded to 6 (".5" -> 500000). */` |
|      3 | 1142 | `			const char *zStart = z;` |
|      3 | 1143 | `			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){` |
|    ! 0 | 1144 | `				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");` |
|    ! 0 | 1145 | `				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){` |
|    ! 0 | 1146 | `					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");` |
|    ! 0 | 1147 | `				}else{` |
|    ! 0 | 1148 | `					zStart = z; /* HuntDigits repositioned; treat as freshly read */` |
|      - | 1149 | `				}` |
|    ! 0 | 1150 | `			}` |
|      - | 1151 | `			{` |
|      3 | 1152 | `				int nd = (int)(z - zStart);` |
|      3 | 1153 | `				while( nd > 0 && nd < 6 ){ v *= 10; nd++; }` |
|      3 | 1154 | `				uSecFF = (int)v; bHasUs = 1;` |
|      - | 1155 | `			}` |
|      3 | 1156 | `			break;` |
|      - | 1157 | `				 }` |
|    ! 0 | 1158 | `		case 'v':{` |
|    ! 0 | 1159 | `			const char *zStart = z;` |
|    ! 0 | 1160 | `			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){` |
|    ! 0 | 1161 | `				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");` |
|    ! 0 | 1162 | `				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){` |
|    ! 0 | 1163 | `					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");` |
|    ! 0 | 1164 | `				}else{` |
|    ! 0 | 1165 | `					zStart = z;` |
|      - | 1166 | `				}` |
|    ! 0 | 1167 | `			}` |
|      - | 1168 | `			{` |
|    ! 0 | 1169 | `				int nd = (int)(z - zStart);` |
|    ! 0 | 1170 | `				while( nd > 0 && nd < 3 ){ v *= 10; nd++; }` |
|    ! 0 | 1171 | `				uSecFF = (int)v * 1000; bHasUs = 1; /* ms -> us */` |
|      - | 1172 | `			}` |
|    ! 0 | 1173 | `			break;` |
|      - | 1174 | `				 }` |
|      2 | 1175 | `		case 'a': case 'A':{` |
|      - | 1176 | `			static const char *azMer[] = {"am","pm","a.m.","p.m."};` |
|      5 | 1177 | `			int k = DtEatName(&z,zInEnd,azMer,4);` |
|      5 | 1178 | `			if( k ){` |
|      5 | 1179 | `				iMeridiem = ((k - 1) & 1);` |
|      3 | 1180 | `			}else{` |
|    ! 0 | 1181 | `				zErr = "A meridian could not be found";` |
|      - | 1182 | `			}` |
|      5 | 1183 | `			break;` |
|      - | 1184 | `				 }` |
|      2 | 1185 | `		case 'U':{` |
|      5 | 1186 | `			int neg = 0;` |
|      5 | 1187 | `			if( z < zInEnd && z[0]=='-' ){ neg = 1; z++; }` |
|      5 | 1188 | `			if( DtEatDigits(&z,zInEnd,1,19,&uVal) ){` |
|      5 | 1189 | `				if( neg ){ uVal = -uVal; }` |
|      5 | 1190 | `				bHasU = 1;` |
|      3 | 1191 | `			}else{` |
|    ! 0 | 1192 | `				DT_FF_LOGERR((int)(z - zIn),"A unix timestamp could not be found");` |
|    ! 0 | 1193 | `				if( DtHuntDigits(&z,zInEnd,1,19,&uVal) < 0 ){` |
|    ! 0 | 1194 | `					DT_FF_LOGERR(nIn,"A unix timestamp could not be found");` |
|    ! 0 | 1195 | `				}else{` |
|    ! 0 | 1196 | `					if( neg ){ uVal = -uVal; }` |
|    ! 0 | 1197 | `					bHasU = 1;` |
|      - | 1198 | `				}` |
|      - | 1199 | `			}` |
|      5 | 1200 | `			break;` |
|      - | 1201 | `				 }` |
|      1 | 1202 | `		case 'e': case 'T':{` |
|      - | 1203 | `			static const char *azZone[] = {"UTC","GMT","Z"};` |
|      3 | 1204 | `			int k = DtEatName(&z,zInEnd,azZone,3);` |
|      3 | 1205 | `			if( k == 3 ){` |
|    ! 0 | 1206 | `				iOffKind = 2; iOffVal = 0;` |
|      3 | 1207 | `			}else if( k ){` |
|      3 | 1208 | `				iOffKind = 3; iOffVal = 0;` |
|      3 | 1209 | `				SyMemcpy(azZone[k-1],zName,4);` |
|      1 | 1210 | `			}else if( z < zInEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|    ! 0 | 1211 | `				goto parse_num_off;` |
|    ! 0 | 1212 | `			}else{` |
|    ! 0 | 1213 | `				zErr = "The timezone could not be found in the database";` |
|      - | 1214 | `			}` |
|      3 | 1215 | `			break;` |
|      2 | 1216 | `				 }` |
|      - | 1217 | `		case 'O': case 'P':` |
|      2 | 1218 | `parse_num_off:	{` |
|      5 | 1219 | `			int sign,oh,om = 0;` |
|      - | 1220 | `			sxi64 t;` |
|      5 | 1221 | `			if( z >= zInEnd \|\| (z[0] != '+' && z[0] != '-') ){` |
|    ! 0 | 1222 | `				zErr = "The timezone could not be found in the database";` |
|    ! 0 | 1223 | `				break;` |
|      - | 1224 | `			}` |
|      5 | 1225 | `			sign = (z[0]=='-') ? -1 : 1;` |
|      5 | 1226 | `			z++;` |
|      5 | 1227 | `			if( !DtEatDigits(&z,zInEnd,2,2,&t) ){` |
|    ! 0 | 1228 | `				zErr = "The timezone could not be found in the database";` |
|    ! 0 | 1229 | `				break;` |
|      - | 1230 | `			}` |
|      5 | 1231 | `			oh = (int)t;` |
|      5 | 1232 | `			if( z < zInEnd && z[0]==':' ){ z++; }` |
|      5 | 1233 | `			if( DtEatDigits(&z,zInEnd,2,2,&t) ){ om = (int)t; }` |
|      5 | 1234 | `			iOffKind = 1;` |
|      5 | 1235 | `			iOffVal = sign * (oh*3600 + om*60);` |
|      5 | 1236 | `			break;` |
|      - | 1237 | `				 }` |
|    ! 0 | 1238 | `		case '?':` |
|    ! 0 | 1239 | `			if( z < zInEnd ){ z++; }` |
|    ! 0 | 1240 | `			break;` |
|    ! 0 | 1241 | `		case '*':` |
|      - | 1242 | `			/* skip input until the next separator byte */` |
|    ! 0 | 1243 | `			while( z < zInEnd && !SyisDigit(z[0]) && z[0] != ';' && z[0] != ':'` |
|    ! 0 | 1244 | `			 && z[0] != '/' && z[0] != '.' && z[0] != ',' && z[0] != '-'` |
|    ! 0 | 1245 | `			 && z[0] != '(' && z[0] != ')' && z[0] != ' ' ){` |
|    ! 0 | 1246 | `				z++;` |
|    ! 0 | 1247 | `			}` |
|    ! 0 | 1248 | `			break;` |
|      1 | 1249 | `		case '#':` |
|      3 | 1250 | `			if( z < zInEnd && (z[0]==';'\|\|z[0]==':'\|\|z[0]=='/'\|\|z[0]=='.'` |
|    ! 0 | 1251 | `			 \|\|z[0]==','\|\|z[0]=='-'\|\|z[0]=='('\|\|z[0]==')') ){` |
|      3 | 1252 | `				z++;` |
|      2 | 1253 | `			}else{` |
|    ! 0 | 1254 | `				zErr = "The separation symbol could not be found";` |
|      - | 1255 | `			}` |
|      3 | 1256 | `			break;` |
|      1 | 1257 | `		case '\\':` |
|      3 | 1258 | `			if( zFmt < zEnd ){` |
|      3 | 1259 | `				if( z < zInEnd && z[0] == zFmt[0] ){` |
|      3 | 1260 | `					z++;` |
|      3 | 1261 | `					zFmt++;` |
|      2 | 1262 | `				}else{` |
|      - | 1263 | `					/* a literal mismatch aborts timelib's scan */` |
|    ! 0 | 1264 | `					DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|    ! 0 | 1265 | `					zFmt = zEnd;` |
|    ! 0 | 1266 | `					bAborted = 1;` |
|      - | 1267 | `				}` |
|      1 | 1268 | `			}` |
|      3 | 1269 | `			break;` |
|     57 | 1270 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|      - | 1271 | `		case '(' : case ')':` |
|    115 | 1272 | `			if( z < zInEnd && z[0] == c ){` |
|    115 | 1273 | `				z++;` |
|     58 | 1274 | `			}else{` |
|      - | 1275 | `				/* timelib logs BOTH messages (count +2, last-wins on the` |
|      - | 1276 | `				 * position), consumes the offending byte, and keeps going */` |
|    ! 0 | 1277 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|    ! 0 | 1278 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|    ! 0 | 1279 | `				z++;` |
|      - | 1280 | `			}` |
|    115 | 1281 | `			break;` |
|     16 | 1282 | `		case ' ':` |
|     33 | 1283 | `			if( z < zInEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|     33 | 1284 | `				z++;` |
|     17 | 1285 | `			}else{` |
|    ! 0 | 1286 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|    ! 0 | 1287 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|    ! 0 | 1288 | `				z++;` |
|      - | 1289 | `			}` |
|     33 | 1290 | `			break;` |
|      1 | 1291 | `		default:` |
|      - | 1292 | `			/* any other format byte must match the input verbatim; a mismatch` |
|      - | 1293 | `			 * aborts timelib's scan */` |
|      3 | 1294 | `			if( z < zInEnd && z[0] == c ){` |
|    ! 0 | 1295 | `				z++;` |
|    ! 0 | 1296 | `			}else{` |
|      3 | 1297 | `				DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|      3 | 1298 | `				zFmt = zEnd;` |
|      3 | 1299 | `				bAborted = 1;` |
|      - | 1300 | `			}` |
|      2 | 1301 | `			break;` |
|      - | 1302 | `		}` |
|    371 | 1303 | `		if( zErr ){` |
|      - | 1304 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|    ! 0 | 1305 | `			DT_FF_LOGERR((int)(z - zIn),zErr);` |
|    ! 0 | 1306 | `		}` |
|      1 | 1307 | `	}` |
|     69 | 1308 | `	if( z < zInEnd && !bAborted ){` |
|      5 | 1309 | `		if( bPlus ){` |
|      - | 1310 | `			/* '+' downgrades trailing data to a warning */` |
|      3 | 1311 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|      3 | 1312 | `			aWarnMsg[nWarn] = "Trailing data";` |
|      3 | 1313 | `			nWarn++;` |
|      2 | 1314 | `		}else{` |
|      3 | 1315 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|      - | 1316 | `		}` |
|      2 | 1317 | `	}` |
|     69 | 1318 | `	if( nErr > 0 ){` |
|      - | 1319 | `		/* The diagnostics are the caller's on this path too: php reports the` |
|      - | 1320 | `		 * warnings of a parse that ALSO failed, which the old string encoding had` |
|      - | 1321 | `		 * no room for. */` |
|     13 | 1322 | `		DtFfDiag(&pOut->sDiag,nErr,nErrKept,aErrPos,aErrMsg,nWarn,aWarnPos,aWarnMsg);` |
|     13 | 1323 | `		return -1;` |
|      - | 1324 | `	}` |
|     57 | 1325 | `	if( bPipe ){` |
|      5 | 1326 | `		if( y < 0 ){ y = 1970; }` |
|      5 | 1327 | `		if( mo < 0 ){ mo = 1; }` |
|      5 | 1328 | `		if( d < 0 ){ d = 1; }` |
|      5 | 1329 | `		if( h < 0 && h12 < 0 ){ h = 0; }` |
|      5 | 1330 | `		if( mi < 0 ){ mi = 0; }` |
|      5 | 1331 | `		if( s < 0 ){ s = 0; }` |
|      2 | 1332 | `	}` |
|      - | 1333 | `	{` |
|      - | 1334 | `		/* remaining unset fields come from "now" in the default offset */` |
|     57 | 1335 | `		sxi64 iLocal = iNow + iDefOff;` |
|     57 | 1336 | `		sxi64 days = DtFloorDiv(iLocal,86400);` |
|     57 | 1337 | `		sxi64 secs = iLocal - days*86400;` |
|      - | 1338 | `		sxi64 ny;` |
|      - | 1339 | `		int nmo,nd;` |
|     57 | 1340 | `		DtCivilFromDays(days,&ny,&nmo,&nd);` |
|     57 | 1341 | `		if( y < 0 ){ y = ny; }` |
|     57 | 1342 | `		if( mo < 0 ){ mo = nmo; }` |
|     57 | 1343 | `		if( d < 0 ){ d = nd; }` |
|     57 | 1344 | `		if( h12 >= 0 ){` |
|      5 | 1345 | `			h = (h12 % 12) + ((iMeridiem == 1) ? 12 : 0);` |
|      2 | 1346 | `		}` |
|      - | 1347 | `		/* php: parsing a time component zeroes the finer unset units */` |
|     57 | 1348 | `		if( h >= 0 ){` |
|     27 | 1349 | `			if( mi < 0 ){ mi = 0; }` |
|     27 | 1350 | `			if( s < 0 ){ s = 0; }` |
|     44 | 1351 | `		}else if( mi >= 0 ){` |
|      3 | 1352 | `			if( s < 0 ){ s = 0; }` |
|      1 | 1353 | `		}` |
|     57 | 1354 | `		if( h < 0 ){ h = secs / 3600; }` |
|     57 | 1355 | `		if( mi < 0 ){ mi = (secs / 60) % 60; }` |
|     57 | 1356 | `		if( s < 0 ){ s = secs % 60; }` |
|      - | 1357 | `	}` |
|      - | 1358 | `	/* php validates the RESOLVED fields and warns (parse still succeeds,` |
|      - | 1359 | `	 * values roll over via civil arithmetic) */` |
|     57 | 1360 | `	if( mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo) ){` |
|      3 | 1361 | `		if( nWarn < PH7_DT_MAX_WARN ){` |
|      3 | 1362 | `			aWarnPos[nWarn] = nIn;` |
|      3 | 1363 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|      3 | 1364 | `			nWarn++;` |
|      1 | 1365 | `		}` |
|      1 | 1366 | `	}` |
|     57 | 1367 | `	if( h > 24 \|\| mi > 59 \|\| s > 59 ){` |
|      3 | 1368 | `		if( nWarn < PH7_DT_MAX_WARN ){` |
|      3 | 1369 | `			aWarnPos[nWarn] = nIn;` |
|      3 | 1370 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|      3 | 1371 | `			nWarn++;` |
|      1 | 1372 | `		}` |
|      1 | 1373 | `	}` |
|      - | 1374 | `	{` |
|     57 | 1375 | `		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;` |
|     57 | 1376 | `		if( bHasU ){` |
|      5 | 1377 | `			pOut->iTs = uVal;` |
|      5 | 1378 | `			iUseOff = 0;` |
|      5 | 1379 | `			iOffKind = 1;` |
|      3 | 1380 | `		}else{` |
|     53 | 1381 | `			pOut->iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);` |
|      - | 1382 | `		}` |
|     57 | 1383 | `		pOut->iOff = iUseOff;` |
|     57 | 1384 | `		pOut->iOffKind = iOffKind;` |
|     57 | 1385 | `		SyMemcpy(zName,pOut->zName,sizeof(pOut->zName));` |
|     57 | 1386 | `		pOut->zName[sizeof(pOut->zName)-1] = 0;` |
|     57 | 1387 | `		pOut->uSec = uSecFF;` |
|     57 | 1388 | `		pOut->bHasUs = bHasUs;` |
|      - | 1389 | `	}` |
|     57 | 1390 | `	DtFfDiag(&pOut->sDiag,nErr,nErrKept,aErrPos,aErrMsg,nWarn,aWarnPos,aWarnMsg);` |
|     57 | 1391 | `	return 0;` |
|     35 | 1392 | `}` |
|      - | 1393 | `/*` |
|      - | 1394 | ` * ---------------------------------------------------------------------------` |
|      - | 1395 | ` * DateTimeZone, DateTime and DateTimeImmutable, declared from C.` |
|      - | 1396 | ` *` |
|      - | 1397 | `` * These three used to be embedded PHP over nine global `__dt_*` thunks, with a`` |
|      - | 1398 | `` * private `trait __DtCoreT` holding the state and the shared half of both date`` |
|      - | 1399 | ` * classes. Every operation therefore crossed C -> PHP -> C and marshalled its` |
|      - | 1400 | ` * answer through a throwaway PHP array. The bodies below call the same routines` |
|      - | 1401 | ` * directly; the thunks, the trait and their chunk classes are gone.` |
|      - | 1402 | ` *` |
|      - | 1403 | `` * The instance state is unchanged, so `clone`, `serialize` and `var_dump` see what`` |
|      - | 1404 | `` * they always saw (minus the `__DtCoreT` declaring-class name): four private slots`` |
|      - | 1405 | ` * on each date class, two on DateTimeZone. Native traits do not exist, so the` |
|      - | 1406 | `` * shared method table is simply installed on both classes -- which is what `use`` |
|      - | 1407 | `` * __DtCoreT` did anyway.`` |
|      - | 1408 | ` * ---------------------------------------------------------------------------` |
|      - | 1409 | ` */` |
|      - | 1410 | `#define DT_TS    "__dtTs"` |
|      - | 1411 | `#define DT_OFF   "__dtOff"` |
|      - | 1412 | `#define DT_NAME  "__dtName"` |
|      - | 1413 | `#define DT_US    "__dtUs"` |
|      - | 1414 | `#define DTZ_OFF  "__dtzOff"` |
|      - | 1415 | `#define DTZ_NAME "__dtzName"` |
|      - | 1416 | `/*` |
|      - | 1417 | ` * ---------------------------------------------------------------------------` |
|      - | 1418 | ` * A DateInterval's MICROSECONDS.` |
|      - | 1419 | ` *` |
|      - | 1420 | ` * php stores them as an int64 COUNT (timelib_rel_time.us) and shows that count` |
|      - | 1421 | ` * divided by a million, so the float is a rendering and the integer is the` |
|      - | 1422 | `` * value: `$i->f = 0.1234567` reads back 0.123456 because the write truncated to`` |
|      - | 1423 | `` * 123456 microseconds, and `f` is what diff() fills, what add()/sub() move the`` |
|      - | 1424 | ` * clock by, and what format()'s %f prints.` |
|      - | 1425 | ` *` |
|      - | 1426 | `` * PHL's `f` is a real property slot a script reads directly, so the count lives`` |
|      - | 1427 | ` * beside it in a hidden one. The two are written together by every door that` |
|      - | 1428 | ` * owns the value (the write handler, diff, the constructors); a write that` |
|      - | 1429 | ` * arrives from somewhere else — unserialize's raw property store, or one of the` |
|      - | 1430 | `` * §7.4 shapes php answers with a temporary — leaves only `f` behind, so the`` |
|      - | 1431 | ` * count is trusted only while it still RENDERS to the float on show, and is` |
|      - | 1432 | ` * re-derived from the float when it does not.` |
|      - | 1433 | ` * ---------------------------------------------------------------------------` |
|      - | 1434 | ` */` |
|      - | 1435 | `#define DT_IV_US "__ivUs"` |
|      - | 1436 | ``/* php's conversion of the `f` property to its stored count, cast contract and`` |
|      - | 1437 | ` * all: it TRUNCATES toward zero, WRAPS what no int64 can hold, and answers 0 for` |
|      - | 1438 | ` * a NaN or an infinity. */` |
|    490 | 1439 | `static sxi64 DtIvUsecOfReal(double r)` |
|      2 | 1440 | `{` |
|    492 | 1441 | `	return PH7_RealToInt64(r * 1000000.0);` |
|      2 | 1442 | `}` |
|      - | 1443 | `/* The interval's microseconds. */` |
|    782 | 1444 | `static sxi64 DtIvUsec(ph7_class_instance *pIv)` |
|      2 | 1445 | `{` |
|    784 | 1446 | `	ph7_value *pF = PH7_NativeAttr(pIv,"f");` |
|    784 | 1447 | `	sxi64 us = PH7_NativeAttrInt(pIv,DT_IV_US);` |
|    784 | 1448 | `	double r = 0.0;` |
|    784 | 1449 | `	if( pF && (pF->iFlags & MEMOBJ_REAL) ){` |
|    784 | 1450 | `		r = (double)pF->rVal;` |
|    391 | 1451 | `	}else if( pF && (pF->iFlags & MEMOBJ_INT) ){` |
|    ! 0 | 1452 | `		r = (double)pF->x.iVal;` |
|    ! 0 | 1453 | `	}` |
|    784 | 1454 | `	if( (double)us / 1000000.0 == r ){` |
|    784 | 1455 | ``		return us;   /* the count `f` was rendered from: exact past 2^53, where the float is not */`` |
|      - | 1456 | `	}` |
|    ! 0 | 1457 | `	return DtIvUsecOfReal(r);` |
|    393 | 1458 | `}` |
|      - | 1459 | `/* Store a microsecond count and the float php shows for it -- the two halves of` |
|      - | 1460 | ` * the same value, written together by every door that owns it. */` |
|     56 | 1461 | `static void DtIvSetUsec(ph7_vm *pVm,ph7_class_instance *pIv,sxi64 us)` |
|      1 | 1462 | `{` |
|     57 | 1463 | `	PH7_NativeSetAttrInt(pVm,pIv,DT_IV_US,us);` |
|     57 | 1464 | `	PH7_NativeSetAttrReal(pVm,pIv,"f",(ph7_real)((double)us / 1000000.0));` |
|     57 | 1465 | `}` |
|      - | 1466 | `/* One date object's state, as the bodies below pass it around. */` |
|      - | 1467 | `typedef struct dt_state dt_state;` |
|      - | 1468 | `struct dt_state` |
|      - | 1469 | `{` |
|      - | 1470 | `	sxi64 iTs;` |
|      - | 1471 | `	sxi32 iOff;` |
|      - | 1472 | `	int uSec;` |
|      - | 1473 | `	const char *zName;   /* borrowed from the instance's own slot */` |
|      - | 1474 | `	int nName;` |
|      - | 1475 | `};` |
|      - | 1476 | `/* php's name for a fixed offset: "+HH:MM" (and "+00:00" for zero, never "-00:00"). */` |
|     76 | 1477 | `static int DtOffName(char *zBuf,sxu32 nBuf,sxi32 iOff)` |
|      1 | 1478 | `{` |
|     77 | 1479 | `	sxi32 a = iOff < 0 ? -iOff : iOff;` |
|    115 | 1480 | `	return (int)SyBufferFormat(zBuf,nBuf,"%c%02d:%02d",` |
|     76 | 1481 | `		iOff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|      1 | 1482 | `}` |
|    468 | 1483 | `static void DtLoad(ph7_class_instance *pObj,dt_state *pOut)` |
|      1 | 1484 | `{` |
|    469 | 1485 | `	pOut->iTs  = PH7_NativeAttrInt(pObj,DT_TS);` |
|    469 | 1486 | `	pOut->iOff = (sxi32)PH7_NativeAttrInt(pObj,DT_OFF);` |
|    469 | 1487 | `	pOut->uSec = (int)PH7_NativeAttrInt(pObj,DT_US);` |
|    469 | 1488 | `	PH7_NativeAttrStr(pObj,DT_NAME,&pOut->zName,&pOut->nName);` |
|    469 | 1489 | `}` |
|    428 | 1490 | `static void DtStore(ph7_vm *pVm,ph7_class_instance *pObj,const dt_state *pIn)` |
|      2 | 1491 | `{` |
|    430 | 1492 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_TS,pIn->iTs);` |
|    430 | 1493 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,pIn->iOff);` |
|    430 | 1494 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_US,pIn->uSec);` |
|    430 | 1495 | `	PH7_NativeSetAttrStr(pVm,pObj,DT_NAME,pIn->zName,pIn->nName);` |
|    430 | 1496 | `}` |
|      - | 1497 | `/* The receiver of a native method, or NULL when the call has no object (which the` |
|      - | 1498 | ` * dispatcher only allows for a static one). */` |
|   1976 | 1499 | `static ph7_class_instance * DtThis(ph7_context *pCtx)` |
|      3 | 1500 | `{` |
|   1979 | 1501 | `	return PH7_ContextThis(pCtx);` |
|      3 | 1502 | `}` |
|  10878 | 1503 | `static ph7_class * DtClass(ph7_vm *pVm,const char *zName)` |
|      5 | 1504 | `{` |
|  10883 | 1505 | `	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);` |
|      5 | 1506 | `}` |
|      - | 1507 | `/* An immutable receiver mutates a COPY; a mutable one mutates itself. That is the` |
|      - | 1508 | ` * only difference between the two classes' method tables, so both share one body. */` |
|    148 | 1509 | `static int DtIsImmutable(ph7_vm *pVm,ph7_class_instance *pObj)` |
|      1 | 1510 | `{` |
|    149 | 1511 | `	ph7_class *pImm = DtClass(pVm,"DateTimeImmutable");` |
|    149 | 1512 | `	return pImm != 0 && PH7_VmInstanceOf(pObj->pClass,pImm);` |
|      1 | 1513 | `}` |
|      - | 1514 | `/*` |
|      - | 1515 | ` * The object a mutator writes: $this itself, or a clone for DateTimeImmutable.` |
|      - | 1516 | ` * Either way the caller returns it, so a mutable method answers the same object` |
|      - | 1517 | `` * php's does (`$d->modify(...) === $d`).`` |
|      - | 1518 | ` */` |
|    120 | 1519 | `static ph7_class_instance * DtMutTarget(ph7_context *pCtx,ph7_class_instance *pThis,int *pbCopy)` |
|      1 | 1520 | `{` |
|    121 | 1521 | `	if( DtIsImmutable(pCtx->pVm,pThis) ){` |
|     35 | 1522 | `		*pbCopy = 1;` |
|     35 | 1523 | `		return PH7_CloneClassInstance(pThis);` |
|      - | 1524 | `	}` |
|     87 | 1525 | `	*pbCopy = 0;` |
|     87 | 1526 | `	return pThis;` |
|     61 | 1527 | `}` |
|      - | 1528 | `/* Return a mutator's target the way php returns it: the clone (whose reference we` |
|      - | 1529 | ` * own) or the receiver itself (whose value the context already holds). */` |
|    120 | 1530 | `static void DtMutResult(ph7_context *pCtx,ph7_class_instance *pTarget,int bCopy)` |
|      1 | 1531 | `{` |
|    121 | 1532 | `	if( bCopy ){` |
|     35 | 1533 | `		PH7_NativeResultObject(pCtx,pTarget);` |
|     18 | 1534 | `	}else{` |
|     87 | 1535 | `		ph7_result_value(pCtx,PH7_ContextThisValue(pCtx));` |
|      - | 1536 | `	}` |
|    121 | 1537 | `}` |
|      - | 1538 | `/* Read a DateTimeZone argument's two slots. php's ext/date reads its own internal` |
|      - | 1539 | ` * timezone struct here, so an overridden getName()/getOffset() is ignored by both` |
|      - | 1540 | ` * engines. Answers 0 when the value is not a DateTimeZone at all. */` |
|    176 | 1541 | `static int DtZoneOf(ph7_value *pArg,sxi32 *piOff,const char **pzName,int *pnName)` |
|      1 | 1542 | `{` |
|      - | 1543 | `	ph7_class_instance *pObj;` |
|    177 | 1544 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 1545 | `		return 0;` |
|      - | 1546 | `	}` |
|    177 | 1547 | `	pObj = (ph7_class_instance *)pArg->x.pOther;` |
|    177 | 1548 | `	if( PH7_NativeAttr(pObj,DTZ_NAME) == 0 ){` |
|    ! 0 | 1549 | `		return 0;` |
|      - | 1550 | `	}` |
|    177 | 1551 | `	*piOff = (sxi32)PH7_NativeAttrInt(pObj,DTZ_OFF);` |
|    177 | 1552 | `	PH7_NativeAttrStr(pObj,DTZ_NAME,pzName,pnName);` |
|    177 | 1553 | `	return 1;` |
|     89 | 1554 | `}` |
|      - | 1555 | `/*` |
|      - | 1556 | ` * Record one parse's diagnostics as getLastErrors()'s answer.` |
|      - | 1557 | ` *` |
|      - | 1558 | ` * php resets the record on EVERY constructor and createFromFormat() call — a clean` |
|      - | 1559 | `` * parse answers `false` again — and a failing constructor publishes its reason as a`` |
|      - | 1560 | ` * one-entry error map before it throws.` |
|      - | 1561 | ` */` |
|   5614 | 1562 | `static void DtLastErrClear(ph7_vm *pVm)` |
|      5 | 1563 | `{` |
|   5619 | 1564 | `	SyZero(&pVm->sDtLastErr,sizeof(pVm->sDtLastErr));` |
|   5619 | 1565 | `}` |
|     30 | 1566 | `static void DtLastErrOne(ph7_vm *pVm,int iPos,const char *zMsg)` |
|      1 | 1567 | `{` |
|     31 | 1568 | `	DtLastErrClear(&(*pVm));` |
|     31 | 1569 | `	pVm->sDtLastErr.bSet = 1;` |
|     31 | 1570 | `	pVm->sDtLastErr.nErr = 1;` |
|     31 | 1571 | `	pVm->sDtLastErr.nErrKept = 1;` |
|     31 | 1572 | `	pVm->sDtLastErr.aErrPos[0] = iPos;` |
|     31 | 1573 | `	pVm->sDtLastErr.azErr[0] = zMsg;` |
|     31 | 1574 | `}` |
|      - | 1575 | `/*` |
|      - | 1576 | ` * Parse $datetime into a date object's state, php's constructor rules: an explicit` |
|      - | 1577 | ` * offset in the string wins over the $timezone argument, a literal "Z" keeps its` |
|      - | 1578 | ` * own name, and everything else takes the argument's (or the default) zone.` |
|      - | 1579 | ` * Returns 0 on success; on failure the caller throws with the reason and position` |
|      - | 1580 | ` * this reports.` |
|      - | 1581 | ` */` |
|    390 | 1582 | `static int DtInitState(ph7_context *pCtx,const char *zIn,int nIn,sxi32 iZoneOff,` |
|      - | 1583 | `	const char *zZoneName,int nZoneName,dt_state *pOut,char *zNameBuf,sxu32 nNameBuf,` |
|      - | 1584 | `	const char **pzErr,int *piPos,char *pcAt)` |
|      2 | 1585 | `{` |
|    392 | 1586 | `	sxi64 iTs = 0;` |
|    392 | 1587 | `	sxi32 iOff = 0;` |
|    392 | 1588 | `	int bOffSet = 0,uSec = 0,iErrPos;` |
|    392 | 1589 | `	iErrPos = DtParse(zIn,nIn,(sxi64)time(0),iZoneOff,&iTs,&iOff,&bOffSet,&uSec);` |
|    392 | 1590 | `	if( iErrPos != 0 ){` |
|     33 | 1591 | `		*pzErr = DtParseErr(zIn,nIn,iErrPos,piPos,pcAt);` |
|     33 | 1592 | `		return -1;` |
|      - | 1593 | `	}` |
|    360 | 1594 | `	pOut->iTs = iTs;` |
|    360 | 1595 | `	pOut->uSec = uSec;` |
|    360 | 1596 | `	if( bOffSet ){` |
|     41 | 1597 | `		pOut->iOff = iOff;` |
|     41 | 1598 | `		if( bOffSet == 2 ){` |
|      9 | 1599 | `			pOut->zName = "Z";` |
|      9 | 1600 | `			pOut->nName = 1;` |
|      5 | 1601 | `		}else{` |
|     33 | 1602 | `			pOut->nName = DtOffName(zNameBuf,nNameBuf,iOff);` |
|     33 | 1603 | `			pOut->zName = zNameBuf;` |
|      - | 1604 | `		}` |
|     21 | 1605 | `	}else{` |
|    320 | 1606 | `		pOut->iOff = iZoneOff;` |
|    320 | 1607 | `		pOut->zName = zZoneName;` |
|    320 | 1608 | `		pOut->nName = nZoneName;` |
|      - | 1609 | `	}` |
|    179 | 1610 | `	SXUNUSED(pCtx);` |
|    360 | 1611 | `	return 0;` |
|    197 | 1612 | `}` |
|      - | 1613 | `/*` |
|      - | 1614 | ` * The timezone spellings PHL understands with no tz database: UTC, GMT, Z and a` |
|      - | 1615 | ` * fixed [+-]HH:?MM offset. Shared by DateTimeZone::__construct(), which throws on` |
|      - | 1616 | ` * a miss, and timezone_open(), which warns and answers false.` |
|      - | 1617 | ` */` |
|    202 | 1618 | `static int DtZoneParse(const char *zTz,int nTz,sxi32 *piOff,const char **pzName,` |
|      - | 1619 | `	int *pnName,char *zBuf,sxu32 nBuf)` |
|      1 | 1620 | `{` |
|    203 | 1621 | `	if( nTz == 1 && zTz[0] == 'Z' ){` |
|    ! 0 | 1622 | `		*piOff = 0;` |
|    ! 0 | 1623 | `		*pzName = "Z";` |
|    ! 0 | 1624 | `		*pnName = 1;` |
|    ! 0 | 1625 | `		return 0;` |
|      - | 1626 | `	}` |
|    203 | 1627 | `	if( nTz == 3 && (SyStrnicmp(zTz,"UTC",3) == 0 \|\| SyStrnicmp(zTz,"GMT",3) == 0) ){` |
|      - | 1628 | `		/* php answers the canonical spelling, whatever case the caller used. */` |
|    163 | 1629 | `		*piOff = 0;` |
|    163 | 1630 | `		*pzName = (zTz[0] == 'u' \|\| zTz[0] == 'U') ? "UTC" : "GMT";` |
|    163 | 1631 | `		*pnName = 3;` |
|    163 | 1632 | `		return 0;` |
|      - | 1633 | `	}` |
|     40 | 1634 | `	if( (nTz == 6 \|\| nTz == 5) && (zTz[0] == '+' \|\| zTz[0] == '-')` |
|     36 | 1635 | `	 && SyisDigit(zTz[1]) && SyisDigit(zTz[2])` |
|     73 | 1636 | `	 && (nTz == 5 ? (SyisDigit(zTz[3]) && SyisDigit(zTz[4]))` |
|     36 | 1637 | `	              : (zTz[3] == ':' && SyisDigit(zTz[4]) && SyisDigit(zTz[5]))) ){` |
|     37 | 1638 | `		int h = (zTz[1] - '0') * 10 + (zTz[2] - '0');` |
|     19 | 1639 | `		int m = nTz == 5 ? (zTz[3] - '0') * 10 + (zTz[4] - '0')` |
|     36 | 1640 | `		                 : (zTz[4] - '0') * 10 + (zTz[5] - '0');` |
|     37 | 1641 | `		sxi32 iOff = h * 3600 + m * 60;` |
|     37 | 1642 | `		if( zTz[0] == '-' ){` |
|      5 | 1643 | `			iOff = -iOff;` |
|      2 | 1644 | `		}` |
|     37 | 1645 | `		*piOff = iOff;` |
|      - | 1646 | `		/* php normalizes the NAME through the offset, so "-00:00" is "+00:00". */` |
|     37 | 1647 | `		*pnName = DtOffName(zBuf,nBuf,iOff);` |
|     37 | 1648 | `		*pzName = zBuf;` |
|     37 | 1649 | `		return 0;` |
|      - | 1650 | `	}` |
|      5 | 1651 | `	return -1;` |
|    102 | 1652 | `}` |
|      - | 1653 | `/* DateTimeZone::__construct(string $timezone) */` |
|    170 | 1654 | `static int vm_builtin_DateTimeZone_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1655 | `{` |
|    171 | 1656 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1657 | `	const char *zTz,*zName;` |
|      - | 1658 | `	int nTz,nName;` |
|    171 | 1659 | `	sxi32 iOff = 0;` |
|      - | 1660 | `	char zBuf[16];` |
|    171 | 1661 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1662 | `		return PH7_OK;` |
|      - | 1663 | `	}` |
|    171 | 1664 | `	zTz = ph7_value_to_string(apArg[0],&nTz);` |
|    171 | 1665 | `	if( DtZoneParse(zTz,nTz,&iOff,&zName,&nName,zBuf,sizeof(zBuf)) != 0 ){` |
|      4 | 1666 | `		return PH7_VmThrowException(pCtx,"DateInvalidTimeZoneException",` |
|      1 | 1667 | `			"DateTimeZone::__construct(): Unknown or bad timezone (%.*s)",nTz,zTz);` |
|      - | 1668 | `	}` |
|    169 | 1669 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,DTZ_OFF,iOff);` |
|    169 | 1670 | `	PH7_NativeSetAttrStr(pCtx->pVm,pThis,DTZ_NAME,zName,nName);` |
|    169 | 1671 | `	return PH7_OK;` |
|     86 | 1672 | `}` |
|      - | 1673 | `/* DateTimeZone::getName() */` |
|     20 | 1674 | `static int vm_builtin_DateTimeZone_getName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1675 | `{` |
|     21 | 1676 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1677 | `	const char *zName;` |
|      - | 1678 | `	int nName;` |
|     10 | 1679 | `	SXUNUSED(nArg);` |
|     10 | 1680 | `	SXUNUSED(apArg);` |
|     21 | 1681 | `	if( pThis == 0 ){` |
|    ! 0 | 1682 | `		return PH7_OK;` |
|      - | 1683 | `	}` |
|     21 | 1684 | `	PH7_NativeAttrStr(pThis,DTZ_NAME,&zName,&nName);` |
|     21 | 1685 | `	ph7_result_string(pCtx,zName,nName);` |
|     21 | 1686 | `	return PH7_OK;` |
|     11 | 1687 | `}` |
|      - | 1688 | `/* DateTimeZone::getOffset(DateTimeInterface $datetime) */` |
|      2 | 1689 | `static int vm_builtin_DateTimeZone_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1690 | `{` |
|      3 | 1691 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      1 | 1692 | `	SXUNUSED(nArg);` |
|      1 | 1693 | `	SXUNUSED(apArg);` |
|      3 | 1694 | `	if( pThis == 0 ){` |
|    ! 0 | 1695 | `		return PH7_OK;` |
|      - | 1696 | `	}` |
|      - | 1697 | `	/* Fixed-offset zones only, so the instant does not change the answer. */` |
|      3 | 1698 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DTZ_OFF));` |
|      3 | 1699 | `	return PH7_OK;` |
|      2 | 1700 | `}` |
|      - | 1701 | `/* DateTime::__construct(string $datetime = 'now', ?DateTimeZone $timezone = null) */` |
|    332 | 1702 | `static int vm_builtin_DateTime_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1703 | `{` |
|    334 | 1704 | `	ph7_vm *pVm = pCtx->pVm;` |
|    334 | 1705 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|    334 | 1706 | `	const char *zIn = "now",*zZone;` |
|    334 | 1707 | `	int nIn = 3,nZone;` |
|    334 | 1708 | `	sxi32 iZoneOff = 0;` |
|      - | 1709 | `	dt_state sState;` |
|      - | 1710 | `	char zNameBuf[16];` |
|      - | 1711 | `	const char *zErr;` |
|      - | 1712 | `	int iPos;` |
|      - | 1713 | `	char cAt;` |
|    334 | 1714 | `	if( pThis == 0 ){` |
|    ! 0 | 1715 | `		return PH7_OK;` |
|      - | 1716 | `	}` |
|    334 | 1717 | `	zZone = pVm->zDefTz;` |
|    334 | 1718 | `	nZone = (int)pVm->nDefTz;` |
|    334 | 1719 | `	if( nArg > 0 ){` |
|    316 | 1720 | `		zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    157 | 1721 | `	}` |
|    334 | 1722 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    131 | 1723 | `		DtZoneOf(apArg[1],&iZoneOff,&zZone,&nZone);` |
|     65 | 1724 | `	}` |
|    332 | 1725 | `	if( DtInitState(pCtx,zIn,nIn,iZoneOff,zZone,nZone,&sState,zNameBuf,sizeof(zNameBuf),` |
|    168 | 1726 | `		&zErr,&iPos,&cAt) != 0 ){` |
|      - | 1727 | `		/* php publishes the failure through getLastErrors() as well as throwing. */` |
|     27 | 1728 | `		DtLastErrOne(pVm,iPos,zErr);` |
|     40 | 1729 | `		return PH7_VmThrowException(pCtx,"DateMalformedStringException",` |
|      - | 1730 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|     13 | 1731 | `			nIn,zIn,iPos,cAt,zErr);` |
|      - | 1732 | `	}` |
|    308 | 1733 | `	DtLastErrClear(pVm);` |
|    308 | 1734 | `	DtStore(pVm,pThis,&sState);` |
|    308 | 1735 | `	return PH7_OK;` |
|    168 | 1736 | `}` |
|      - | 1737 | ``/* One date object's `format()`, shared with the date_format() alias. */`` |
|    404 | 1738 | `static void DtFormatOf(ph7_context *pCtx,ph7_class_instance *pObj,const char *zFmt,int nFmt)` |
|      1 | 1739 | `{` |
|      - | 1740 | `	dt_state sState;` |
|      - | 1741 | `	Sytm sTm;` |
|      - | 1742 | `	char zZone[64];` |
|      - | 1743 | `	int nName;` |
|    405 | 1744 | `	DtLoad(pObj,&sState);` |
|    405 | 1745 | `	nName = sState.nName;` |
|    405 | 1746 | `	if( nName >= (int)sizeof(zZone) ){` |
|    ! 0 | 1747 | `		nName = (int)sizeof(zZone) - 1;` |
|    ! 0 | 1748 | `	}` |
|    405 | 1749 | `	SyMemcpy(sState.zName,zZone,(sxu32)nName);` |
|    405 | 1750 | `	zZone[nName] = 0;` |
|    405 | 1751 | `	DtFillSytm(sState.iTs,sState.iOff,zZone,&sTm);` |
|    405 | 1752 | `	DateFormat(pCtx,zFmt,nFmt,&sTm,sState.uSec);` |
|    405 | 1753 | `}` |
|      - | 1754 | `/* DateTime::format(string $format) */` |
|    398 | 1755 | `static int vm_builtin_DateTime_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1756 | `{` |
|    399 | 1757 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1758 | `	const char *zFmt;` |
|      - | 1759 | `	int nFmt;` |
|    399 | 1760 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1761 | `		return PH7_OK;` |
|      - | 1762 | `	}` |
|    399 | 1763 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    399 | 1764 | `	DtFormatOf(pCtx,pThis,zFmt,nFmt);` |
|    399 | 1765 | `	return PH7_OK;` |
|    200 | 1766 | `}` |
|      - | 1767 | `/* DateTime::getTimestamp() / getMicrosecond() / getOffset() */` |
|     10 | 1768 | `static int vm_builtin_DateTime_getTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1769 | `{` |
|     11 | 1770 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      5 | 1771 | `	SXUNUSED(nArg);` |
|      5 | 1772 | `	SXUNUSED(apArg);` |
|     11 | 1773 | `	if( pThis ){` |
|     11 | 1774 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_TS));` |
|      5 | 1775 | `	}` |
|     11 | 1776 | `	return PH7_OK;` |
|      1 | 1777 | `}` |
|     12 | 1778 | `static int vm_builtin_DateTime_getMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1779 | `{` |
|     13 | 1780 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      6 | 1781 | `	SXUNUSED(nArg);` |
|      6 | 1782 | `	SXUNUSED(apArg);` |
|     13 | 1783 | `	if( pThis ){` |
|     13 | 1784 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_US));` |
|      6 | 1785 | `	}` |
|     13 | 1786 | `	return PH7_OK;` |
|      1 | 1787 | `}` |
|      4 | 1788 | `static int vm_builtin_DateTime_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1789 | `{` |
|      5 | 1790 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      2 | 1791 | `	SXUNUSED(nArg);` |
|      2 | 1792 | `	SXUNUSED(apArg);` |
|      5 | 1793 | `	if( pThis ){` |
|      5 | 1794 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_OFF));` |
|      2 | 1795 | `	}` |
|      5 | 1796 | `	return PH7_OK;` |
|      1 | 1797 | `}` |
|      - | 1798 | `/* The zone object of a date, built from its stored name and offset, so an` |
|      - | 1799 | ` * identifier PHL stored but cannot re-parse still round-trips. Shared with the` |
|      - | 1800 | ` * date_timezone_get() alias. */` |
|     10 | 1801 | `static int DtTimezoneResult(ph7_context *pCtx,ph7_class_instance *pObj)` |
|      1 | 1802 | `{` |
|     11 | 1803 | `	ph7_vm *pVm = pCtx->pVm;` |
|     11 | 1804 | `	ph7_class *pZoneClass = DtClass(pVm,"DateTimeZone");` |
|      - | 1805 | `	ph7_class_instance *pZone;` |
|      - | 1806 | `	const char *zName;` |
|      - | 1807 | `	int nName;` |
|     11 | 1808 | `	if( pZoneClass == 0 ){` |
|    ! 0 | 1809 | `		return PH7_OK;` |
|      - | 1810 | `	}` |
|     11 | 1811 | `	pZone = PH7_NewClassInstance(pVm,pZoneClass);` |
|     11 | 1812 | `	if( pZone == 0 ){` |
|    ! 0 | 1813 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1814 | `	}` |
|     11 | 1815 | `	PH7_NativeAttrStr(pObj,DT_NAME,&zName,&nName);` |
|     11 | 1816 | `	PH7_NativeSetAttrInt(pVm,pZone,DTZ_OFF,PH7_NativeAttrInt(pObj,DT_OFF));` |
|     11 | 1817 | `	PH7_NativeSetAttrStr(pVm,pZone,DTZ_NAME,zName,nName);` |
|     11 | 1818 | `	PH7_NativeResultObject(pCtx,pZone);` |
|     11 | 1819 | `	return PH7_OK;` |
|      6 | 1820 | `}` |
|      - | 1821 | `/* DateTime::getTimezone() */` |
|      8 | 1822 | `static int vm_builtin_DateTime_getTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1823 | `{` |
|      9 | 1824 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      4 | 1825 | `	SXUNUSED(nArg);` |
|      4 | 1826 | `	SXUNUSED(apArg);` |
|      9 | 1827 | `	if( pThis == 0 ){` |
|    ! 0 | 1828 | `		return PH7_OK;` |
|      - | 1829 | `	}` |
|      9 | 1830 | `	return DtTimezoneResult(pCtx,pThis);` |
|      5 | 1831 | `}` |
|      - | 1832 | `/* The truth of a value, without converting the caller's copy of it. */` |
|      4 | 1833 | `static int DtValueTruth(ph7_vm *pVm,ph7_value *pVal)` |
|      1 | 1834 | `{` |
|      - | 1835 | `	ph7_value sTmp;` |
|      - | 1836 | `	int bRes;` |
|      5 | 1837 | `	PH7_MemObjInit(&(*pVm),&sTmp);` |
|      5 | 1838 | `	PH7_MemObjStore(pVal,&sTmp);` |
|      - | 1839 | `	/* PH7_MemObjToBool converts IN PLACE and returns a STATUS: the answer is in` |
|      - | 1840 | `	 * x.iVal (reading the return is a silent always-false). */` |
|      5 | 1841 | `	PH7_MemObjToBool(&sTmp);` |
|      5 | 1842 | `	bRes = sTmp.x.iVal != 0;` |
|      5 | 1843 | `	PH7_MemObjRelease(&sTmp);` |
|      5 | 1844 | `	return bRes;` |
|      1 | 1845 | `}` |
|      - | 1846 | `/* The DateInterval two dates differ by. Shared with the date_diff() alias. */` |
|     56 | 1847 | `static int DtDiffResult(ph7_context *pCtx,ph7_class_instance *pBase,` |
|      - | 1848 | `	ph7_class_instance *pTarget,int bAbsolute)` |
|      1 | 1849 | `{` |
|     57 | 1850 | `	ph7_vm *pVm = pCtx->pVm;` |
|     57 | 1851 | `	ph7_class *pIvClass = DtClass(pVm,"DateInterval");` |
|      - | 1852 | `	ph7_class_instance *pIv;` |
|      - | 1853 | `	dt_diff sDiff;` |
|     57 | 1854 | `	if( pIvClass == 0 ){` |
|    ! 0 | 1855 | `		return PH7_OK;` |
|      - | 1856 | `	}` |
|    113 | 1857 | `	DtCivilDiff(PH7_NativeAttrInt(pBase,DT_TS),(int)PH7_NativeAttrInt(pBase,DT_US),` |
|     56 | 1858 | `		(sxi32)PH7_NativeAttrInt(pBase,DT_OFF),` |
|     56 | 1859 | `		PH7_NativeAttrInt(pTarget,DT_TS),(int)PH7_NativeAttrInt(pTarget,DT_US),&sDiff);` |
|     57 | 1860 | `	pIv = PH7_NewClassInstance(pVm,pIvClass);` |
|     57 | 1861 | `	if( pIv == 0 ){` |
|    ! 0 | 1862 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1863 | `	}` |
|     57 | 1864 | `	PH7_NativeSetAttrInt(pVm,pIv,"y",sDiff.y);` |
|     57 | 1865 | `	PH7_NativeSetAttrInt(pVm,pIv,"m",sDiff.m);` |
|     57 | 1866 | `	PH7_NativeSetAttrInt(pVm,pIv,"d",sDiff.d);` |
|     57 | 1867 | `	PH7_NativeSetAttrInt(pVm,pIv,"h",sDiff.h);` |
|     57 | 1868 | `	PH7_NativeSetAttrInt(pVm,pIv,"i",sDiff.i);` |
|     57 | 1869 | `	PH7_NativeSetAttrInt(pVm,pIv,"s",sDiff.s);` |
|     57 | 1870 | `	DtIvSetUsec(pVm,pIv,sDiff.uSec);` |
|     57 | 1871 | `	PH7_NativeSetAttrInt(pVm,pIv,"days",sDiff.nDays);` |
|     57 | 1872 | `	PH7_NativeSetAttrInt(pVm,pIv,"invert",bAbsolute ? 0 : sDiff.bInvert);` |
|     57 | 1873 | `	PH7_NativeResultObject(pCtx,pIv);` |
|     57 | 1874 | `	return PH7_OK;` |
|     29 | 1875 | `}` |
|      - | 1876 | `/* DateTime::diff(DateTimeInterface $targetObject, bool $absolute = false) */` |
|     50 | 1877 | `static int vm_builtin_DateTime_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1878 | `{` |
|     51 | 1879 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1880 | `	ph7_class_instance *pTarget;` |
|     51 | 1881 | `	int bAbsolute = 0;` |
|     51 | 1882 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 1883 | `		return PH7_OK;` |
|      - | 1884 | `	}` |
|     51 | 1885 | `	pTarget = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     51 | 1886 | `	if( nArg > 1 ){` |
|      5 | 1887 | `		bAbsolute = DtValueTruth(pCtx->pVm,apArg[1]);` |
|      2 | 1888 | `	}` |
|     51 | 1889 | `	return DtDiffResult(pCtx,pThis,pTarget,bAbsolute);` |
|     26 | 1890 | `}` |
|      - | 1891 | `/* DateTime::modify(string $modifier) */` |
|     12 | 1892 | `static int vm_builtin_DateTime_modify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1893 | `{` |
|     13 | 1894 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 | 1895 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1896 | `	ph7_class_instance *pTarget;` |
|      - | 1897 | `	const char *zMod,*zErr;` |
|     13 | 1898 | `	int nMod,iPos,bCopy = 0,iErrPos;` |
|      - | 1899 | `	char cAt;` |
|     13 | 1900 | `	sxi64 iTs = 0;` |
|     13 | 1901 | `	sxi32 iOff = 0;` |
|     13 | 1902 | `	int bOffSet = 0,uSec = 0;` |
|     13 | 1903 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1904 | `		return PH7_OK;` |
|      - | 1905 | `	}` |
|     13 | 1906 | `	zMod = ph7_value_to_string(apArg[0],&nMod);` |
|     13 | 1907 | `	iErrPos = DtParse(zMod,nMod,PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),` |
|      - | 1908 | `		&iTs,&iOff,&bOffSet,&uSec);` |
|     13 | 1909 | `	if( iErrPos != 0 ){` |
|      3 | 1910 | `		int bImm = DtIsImmutable(pVm,pThis);` |
|      3 | 1911 | `		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);` |
|      4 | 1912 | `		return PH7_VmThrowException(pCtx,"DateMalformedStringException",` |
|      - | 1913 | `			"%s::modify(): Failed to parse time string (%.*s) at position %d (%c): %s",` |
|      1 | 1914 | `			bImm ? "DateTimeImmutable" : "DateTime",nMod,zMod,iPos,cAt,zErr);` |
|      - | 1915 | `	}` |
|     11 | 1916 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     11 | 1917 | `	PH7_NativeSetAttrInt(pVm,pTarget,DT_TS,iTs);` |
|     11 | 1918 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     11 | 1919 | `	return PH7_OK;` |
|      7 | 1920 | `}` |
|      - | 1921 | `/* DateTime::setTimestamp(int $timestamp) — php clears the microseconds with it */` |
|     10 | 1922 | `static int vm_builtin_DateTime_setTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1923 | `{` |
|     11 | 1924 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1925 | `	ph7_class_instance *pTarget;` |
|     11 | 1926 | `	int bCopy = 0;` |
|     11 | 1927 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1928 | `		return PH7_OK;` |
|      - | 1929 | `	}` |
|     11 | 1930 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     11 | 1931 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_TS,ph7_value_to_int64(apArg[0]));` |
|     11 | 1932 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_US,0);` |
|     11 | 1933 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     11 | 1934 | `	return PH7_OK;` |
|      6 | 1935 | `}` |
|      - | 1936 | `/* DateTime::setMicrosecond(int $microsecond) */` |
|     42 | 1937 | `static int vm_builtin_DateTime_setMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1938 | `{` |
|     43 | 1939 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1940 | `	ph7_class_instance *pTarget;` |
|      - | 1941 | `	sxi64 iUs;` |
|     43 | 1942 | `	int bCopy = 0;` |
|     43 | 1943 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1944 | `		return PH7_OK;` |
|      - | 1945 | `	}` |
|     43 | 1946 | `	iUs = ph7_value_to_int64(apArg[0]);` |
|     43 | 1947 | `	if( iUs < 0 \|\| iUs > 999999 ){` |
|      - | 1948 | `		/* php's range refusal, and the reason a date's microseconds can be` |
|      - | 1949 | `		 * assumed to be a fraction of ONE second everywhere else: PHL stored` |
|      - | 1950 | ``		 * whatever int it was handed, so `setMicrosecond(1000000)` formatted as`` |
|      - | 1951 | ``		 * `00:00:00.1000000` and a negative one as `00:00:00.-00001` -- neither`` |
|      - | 1952 | `		 * of them a time. The message names the DECLARING class, so a subclass` |
|      - | 1953 | `		 * of DateTime still reports DateTime. */` |
|     40 | 1954 | `		return PH7_VmThrowException(pCtx,"DateRangeError",` |
|      - | 1955 | `			"%s::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, %qd given",` |
|     26 | 1956 | `			DtIsImmutable(pCtx->pVm,pThis) ? "DateTimeImmutable" : "DateTime",iUs);` |
|      - | 1957 | `	}` |
|     17 | 1958 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     17 | 1959 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_US,iUs);` |
|     17 | 1960 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     17 | 1961 | `	return PH7_OK;` |
|     22 | 1962 | `}` |
|      - | 1963 | `/* DateTime::setTimezone(DateTimeZone $timezone) */` |
|    ! 0 | 1964 | `static int vm_builtin_DateTime_setTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1965 | `{` |
|    ! 0 | 1966 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1967 | `	ph7_class_instance *pTarget;` |
|    ! 0 | 1968 | `	const char *zName = "UTC";` |
|    ! 0 | 1969 | `	int nName = 3,bCopy = 0;` |
|    ! 0 | 1970 | `	sxi32 iOff = 0;` |
|    ! 0 | 1971 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1972 | `		return PH7_OK;` |
|      - | 1973 | `	}` |
|    ! 0 | 1974 | `	if( !DtZoneOf(apArg[0],&iOff,&zName,&nName) ){` |
|    ! 0 | 1975 | `		return PH7_OK;` |
|      - | 1976 | `	}` |
|    ! 0 | 1977 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|    ! 0 | 1978 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_OFF,iOff);` |
|    ! 0 | 1979 | `	PH7_NativeSetAttrStr(pCtx->pVm,pTarget,DT_NAME,zName,nName);` |
|    ! 0 | 1980 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|    ! 0 | 1981 | `	return PH7_OK;` |
|    ! 0 | 1982 | `}` |
|      - | 1983 | `/* Replace the DATE of an object, keeping its time of day (the offset it is` |
|      - | 1984 | ` * expressed in never changes). Shared with the date_date_set() alias. */` |
|      6 | 1985 | `static void DtSetDateOf(ph7_context *pCtx,ph7_class_instance *pObj,sxi64 y,int mo,int d)` |
|      1 | 1986 | `{` |
|      7 | 1987 | `	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);` |
|      7 | 1988 | `	sxi64 iDays = DtFloorDiv(iLocal,86400);` |
|      7 | 1989 | `	sxi64 iSecs = iLocal - iDays*86400;` |
|     10 | 1990 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,` |
|      9 | 1991 | `		DtMakeTs(y,mo,d,(int)(iSecs / 3600),(int)((iSecs / 60) % 60),(int)(iSecs % 60),` |
|      6 | 1992 | `			(sxi32)PH7_NativeAttrInt(pObj,DT_OFF)));` |
|      7 | 1993 | `}` |
|      - | 1994 | `/* Replace the TIME of day, keeping the date. Shared with date_time_set(). */` |
|      8 | 1995 | `static void DtSetTimeOf(ph7_context *pCtx,ph7_class_instance *pObj,int h,int mi,int s,sxi64 uSec)` |
|      1 | 1996 | `{` |
|      9 | 1997 | `	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);` |
|      9 | 1998 | `	sxi64 iDays = DtFloorDiv(iLocal,86400);` |
|      - | 1999 | `	sxi64 y;` |
|      - | 2000 | `	int mo,d;` |
|      9 | 2001 | `	DtCivilFromDays(iDays,&y,&mo,&d);` |
|      9 | 2002 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,DtMakeTs(y,mo,d,h,mi,s,(sxi32)PH7_NativeAttrInt(pObj,DT_OFF)));` |
|      9 | 2003 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,uSec);` |
|      9 | 2004 | `}` |
|      - | 2005 | `/* DateTime::setDate(int $year, int $month, int $day) — the time of day is kept */` |
|      6 | 2006 | `static int vm_builtin_DateTime_setDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2007 | `{` |
|      7 | 2008 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 2009 | `	ph7_class_instance *pTarget;` |
|      7 | 2010 | `	int bCopy = 0;` |
|      7 | 2011 | `	if( pThis == 0 \|\| nArg < 3 ){` |
|    ! 0 | 2012 | `		return PH7_OK;` |
|      - | 2013 | `	}` |
|      7 | 2014 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     10 | 2015 | `	DtSetDateOf(pCtx,pTarget,ph7_value_to_int64(apArg[0]),ph7_value_to_int(apArg[1]),` |
|      6 | 2016 | `		ph7_value_to_int(apArg[2]));` |
|      7 | 2017 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|      7 | 2018 | `	return PH7_OK;` |
|      4 | 2019 | `}` |
|      - | 2020 | `/* DateTime::setTime(int $hour, int $minute, int $second = 0, int $microsecond = 0) */` |
|      8 | 2021 | `static int vm_builtin_DateTime_setTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2022 | `{` |
|      9 | 2023 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 2024 | `	ph7_class_instance *pTarget;` |
|      9 | 2025 | `	int bCopy = 0;` |
|      9 | 2026 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 2027 | `		return PH7_OK;` |
|      - | 2028 | `	}` |
|      9 | 2029 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     18 | 2030 | `	DtSetTimeOf(pCtx,pTarget,ph7_value_to_int(apArg[0]),ph7_value_to_int(apArg[1]),` |
|      7 | 2031 | `		nArg > 2 ? ph7_value_to_int(apArg[2]) : 0,` |
|      6 | 2032 | `		nArg > 3 ? ph7_value_to_int64(apArg[3]) : 0);` |
|      9 | 2033 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|      9 | 2034 | `	return PH7_OK;` |
|      5 | 2035 | `}` |
|      - | 2036 | `/* DateTime::setISODate(int $year, int $week, int $dayOfWeek = 1) */` |
|      6 | 2037 | `static int vm_builtin_DateTime_setISODate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2038 | `{` |
|      7 | 2039 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 2040 | `	ph7_class_instance *pTarget;` |
|      7 | 2041 | `	int bCopy = 0;` |
|      7 | 2042 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 2043 | `		return PH7_OK;` |
|      - | 2044 | `	}` |
|      7 | 2045 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     16 | 2046 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_TS,` |
|      9 | 2047 | `		DtIsoDate(PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),` |
|      6 | 2048 | `			ph7_value_to_int64(apArg[0]),ph7_value_to_int64(apArg[1]),` |
|      5 | 2049 | `			nArg > 2 ? ph7_value_to_int64(apArg[2]) : 1));` |
|      7 | 2050 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|      7 | 2051 | `	return PH7_OK;` |
|      4 | 2052 | `}` |
|      - | 2053 | `/*` |
|      - | 2054 | ` * One interval applied to a date -- the whole of what add(), sub(), their two` |
|      - | 2055 | ` * procedural aliases and the DatePeriod walk each did by hand.` |
|      - | 2056 | ` *` |
|      - | 2057 | `` * The MICROSECONDS are php's `f`, and php's `f` is a signed count of SECONDS'`` |
|      - | 2058 | ` * fractions that moves the clock like any other field: an interval carrying` |
|      - | 2059 | ` * f = 2.5 and nothing else moves it two and a half seconds, and its carry into` |
|      - | 2060 | ` * the second is ordinary floor division (so a sub() past the second borrows).` |
|      - | 2061 | `` * PHL ignored `f` at all four sites, which left `DatePeriod` over a sub-second`` |
|      - | 2062 | ` * interval standing STILL -- every step answering the start date.` |
|      - | 2063 | ` *` |
|      - | 2064 | `` * The arithmetic is done on unsigned intermediates: `f` is a whole int64 count`` |
|      - | 2065 | ` * of microseconds a script may write anything into, and the sum of two of them` |
|      - | 2066 | ` * is exactly the wrap php's own C arrives at rather than an overflow this build` |
|      - | 2067 | ` * would trap on.` |
|      - | 2068 | ` *` |
|      - | 2069 | `` * iSign is the FINAL direction, `invert` already folded in by whichever caller`` |
|      - | 2070 | ` * honours it -- add()/sub() and their aliases do, and the period walk does not` |
|      - | 2071 | ` * (php's own split, below).` |
|      - | 2072 | ` */` |
|    208 | 2073 | `static void DtApplyInterval(ph7_vm *pVm,ph7_class_instance *pSrc,ph7_class_instance *pDst,` |
|      - | 2074 | `	ph7_class_instance *pIv,int iSign)` |
|      1 | 2075 | `{` |
|      - | 2076 | `	sxi64 iUsIv,iUs,iCarry;` |
|    209 | 2077 | `	iUsIv = DtIvUsec(pIv);` |
|    209 | 2078 | `	if( iSign < 0 ){` |
|     33 | 2079 | `		iUsIv = (sxi64)((sxu64)0 - (sxu64)iUsIv);` |
|     16 | 2080 | `	}` |
|    209 | 2081 | `	iUs = (sxi64)((sxu64)PH7_NativeAttrInt(pSrc,DT_US) + (sxu64)iUsIv);` |
|    209 | 2082 | `	iCarry = DtFloorDiv(iUs,1000000);` |
|    313 | 2083 | `	PH7_NativeSetAttrInt(pVm,pDst,DT_TS,` |
|    312 | 2084 | `		(sxi64)((sxu64)DtCivilAdd(PH7_NativeAttrInt(pSrc,DT_TS),(sxi32)PH7_NativeAttrInt(pSrc,DT_OFF),` |
|    104 | 2085 | `			PH7_NativeAttrInt(pIv,"y"),PH7_NativeAttrInt(pIv,"m"),PH7_NativeAttrInt(pIv,"d"),` |
|    104 | 2086 | `			PH7_NativeAttrInt(pIv,"h"),PH7_NativeAttrInt(pIv,"i"),PH7_NativeAttrInt(pIv,"s"),iSign)` |
|    208 | 2087 | `			+ (sxu64)iCarry));` |
|    313 | 2088 | `	PH7_NativeSetAttrInt(pVm,pDst,DT_US,` |
|    208 | 2089 | `		(sxi64)((sxu64)iUs - (sxu64)iCarry * 1000000));` |
|    209 | 2090 | `}` |
|      - | 2091 | `/* add()/sub(): one body, the sign is the difference. */` |
|     64 | 2092 | `static int DtAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)` |
|      1 | 2093 | `{` |
|     65 | 2094 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 2095 | `	ph7_class_instance *pTarget,*pIv;` |
|     65 | 2096 | `	int bCopy = 0;` |
|     65 | 2097 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 2098 | `		return PH7_OK;` |
|      - | 2099 | `	}` |
|     65 | 2100 | `	pIv = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     65 | 2101 | `	if( PH7_NativeAttrInt(pIv,"invert") ){` |
|     17 | 2102 | `		iSign = -iSign;   /* an inverted interval subtracts from add() (php) */` |
|      8 | 2103 | `	}` |
|     65 | 2104 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     65 | 2105 | `	DtApplyInterval(pCtx->pVm,pThis,pTarget,pIv,iSign);` |
|     65 | 2106 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     65 | 2107 | `	return PH7_OK;` |
|     33 | 2108 | `}` |
|     40 | 2109 | `static int vm_builtin_DateTime_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2110 | `{` |
|     41 | 2111 | `	return DtAddSub(pCtx,nArg,apArg,1);` |
|      1 | 2112 | `}` |
|     24 | 2113 | `static int vm_builtin_DateTime_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2114 | `{` |
|     25 | 2115 | `	return DtAddSub(pCtx,nArg,apArg,-1);` |
|      1 | 2116 | `}` |
|      - | 2117 | ``/* DateTime::getLastErrors() — php's array, or `false` when the last parse was clean */`` |
|     24 | 2118 | `static int vm_builtin_DateTime_getLastErrors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2119 | `{` |
|     25 | 2120 | `	ph7_vm *pVm = pCtx->pVm;` |
|     25 | 2121 | `	const phl_dt_lasterr *pErr = &pVm->sDtLastErr;` |
|      - | 2122 | `	ph7_value *pArr,*pWarn,*pErrs,*pVal;` |
|      - | 2123 | `	int k;` |
|     12 | 2124 | `	SXUNUSED(nArg);` |
|     12 | 2125 | `	SXUNUSED(apArg);` |
|     25 | 2126 | `	if( !pErr->bSet ){` |
|      7 | 2127 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2128 | `		return PH7_OK;` |
|      - | 2129 | `	}` |
|     19 | 2130 | `	pArr = ph7_context_new_array(pCtx);` |
|     19 | 2131 | `	pWarn = ph7_context_new_array(pCtx);` |
|     19 | 2132 | `	pErrs = ph7_context_new_array(pCtx);` |
|     19 | 2133 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     19 | 2134 | `	if( pArr == 0 \|\| pWarn == 0 \|\| pErrs == 0 \|\| pVal == 0 ){` |
|    ! 0 | 2135 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2136 | `	}` |
|     25 | 2137 | `	for( k = 0 ; k < pErr->nWarnKept ; k++ ){` |
|      7 | 2138 | `		ph7_value_string(pVal,pErr->azWarn[k],-1);` |
|      7 | 2139 | `		ph7_array_add_intkey_elem(pWarn,pErr->aWarnPos[k],pVal);` |
|      7 | 2140 | `		ph7_value_reset_string_cursor(pVal);` |
|      4 | 2141 | `	}` |
|     35 | 2142 | `	for( k = 0 ; k < pErr->nErrKept ; k++ ){` |
|     17 | 2143 | `		ph7_value_string(pVal,pErr->azErr[k],-1);` |
|     17 | 2144 | `		ph7_array_add_intkey_elem(pErrs,pErr->aErrPos[k],pVal);` |
|     17 | 2145 | `		ph7_value_reset_string_cursor(pVal);` |
|      9 | 2146 | `	}` |
|     19 | 2147 | `	ph7_value_int(pVal,pErr->nWarn);` |
|     19 | 2148 | `	ph7_array_add_strkey_elem(pArr,"warning_count",pVal);` |
|     19 | 2149 | `	ph7_array_add_strkey_elem(pArr,"warnings",pWarn);` |
|     19 | 2150 | `	ph7_value_int(pVal,pErr->nErr);` |
|     19 | 2151 | `	ph7_array_add_strkey_elem(pArr,"error_count",pVal);` |
|     19 | 2152 | `	ph7_array_add_strkey_elem(pArr,"errors",pErrs);` |
|     19 | 2153 | `	ph7_result_value(pCtx,pArr);` |
|     19 | 2154 | `	return PH7_OK;` |
|     13 | 2155 | `}` |
|      - | 2156 | `/*` |
|      - | 2157 | ` * The class a static factory builds. php uses LATE STATIC BINDING here, so` |
|      - | 2158 | `` * `D::createFromFormat()` on a subclass answers a D — where the chunk hardcoded`` |
|      - | 2159 | ` * the literal class name and always answered a DateTime.` |
|      - | 2160 | ` */` |
|    104 | 2161 | `static ph7_class * DtFactoryClass(ph7_context *pCtx,const char *zFallback)` |
|      1 | 2162 | `{` |
|    105 | 2163 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|    105 | 2164 | `	return pClass ? pClass : DtClass(pCtx->pVm,zFallback);` |
|      1 | 2165 | `}` |
|      - | 2166 | `/* DateTime::createFromFormat(string $format, string $datetime, ?DateTimeZone $timezone = null) */` |
|     68 | 2167 | `static int DtCreateFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)` |
|      1 | 2168 | `{` |
|     69 | 2169 | `	ph7_vm *pVm = pCtx->pVm;` |
|     69 | 2170 | `	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);` |
|      - | 2171 | `	ph7_class_instance *pObj;` |
|      - | 2172 | `	dt_ff_res sRes;` |
|      - | 2173 | `	dt_state sState;` |
|      - | 2174 | `	char zNameBuf[16];` |
|      - | 2175 | `	const char *zZone;` |
|      - | 2176 | `	int nZone;` |
|     69 | 2177 | `	sxi32 iZoneOff = 0;` |
|      - | 2178 | `	const char *zFmt,*zIn;` |
|      - | 2179 | `	int nFmt,nIn;` |
|     69 | 2180 | `	if( pClass == 0 \|\| nArg < 2 ){` |
|    ! 0 | 2181 | `		return PH7_OK;` |
|      - | 2182 | `	}` |
|     69 | 2183 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|     69 | 2184 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|     69 | 2185 | `	zZone = pVm->zDefTz;` |
|     69 | 2186 | `	nZone = (int)pVm->nDefTz;` |
|     69 | 2187 | `	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     41 | 2188 | `		DtZoneOf(apArg[2],&iZoneOff,&zZone,&nZone);` |
|     20 | 2189 | `	}` |
|     69 | 2190 | `	if( DtFromFormat(zFmt,nFmt,zIn,nIn,(sxi64)time(0),iZoneOff,&sRes) != 0 ){` |
|     13 | 2191 | `		pVm->sDtLastErr = sRes.sDiag;` |
|     13 | 2192 | `		ph7_result_bool(pCtx,0);` |
|     13 | 2193 | `		return PH7_OK;` |
|      - | 2194 | `	}` |
|     57 | 2195 | `	pVm->sDtLastErr = sRes.sDiag;` |
|     57 | 2196 | `	sState.iTs = sRes.iTs;` |
|     57 | 2197 | `	sState.uSec = sRes.bHasUs ? sRes.uSec : 0;` |
|     57 | 2198 | `	switch( sRes.iOffKind ){` |
|     23 | 2199 | `		case 0:` |
|     47 | 2200 | `			sState.iOff = iZoneOff;` |
|     47 | 2201 | `			sState.zName = zZone;` |
|     47 | 2202 | `			sState.nName = nZone;` |
|     47 | 2203 | `			break;` |
|    ! 0 | 2204 | `		case 2:` |
|    ! 0 | 2205 | `			sState.iOff = 0;` |
|    ! 0 | 2206 | `			sState.zName = "Z";` |
|    ! 0 | 2207 | `			sState.nName = 1;` |
|    ! 0 | 2208 | `			break;` |
|      1 | 2209 | `		case 3:` |
|      3 | 2210 | `			sState.iOff = sRes.iOff;` |
|      3 | 2211 | `			sState.zName = sRes.zName;` |
|      3 | 2212 | `			sState.nName = (int)SyStrlen(sRes.zName);` |
|      3 | 2213 | `			break;` |
|      4 | 2214 | `		default:` |
|      9 | 2215 | `			sState.iOff = sRes.iOff;` |
|      9 | 2216 | `			sState.nName = DtOffName(zNameBuf,sizeof(zNameBuf),sRes.iOff);` |
|      9 | 2217 | `			sState.zName = zNameBuf;` |
|      8 | 2218 | `			break;` |
|      - | 2219 | `	}` |
|     57 | 2220 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     57 | 2221 | `	if( pObj == 0 ){` |
|    ! 0 | 2222 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2223 | `	}` |
|     57 | 2224 | `	DtStore(pVm,pObj,&sState);` |
|     57 | 2225 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     57 | 2226 | `	return PH7_OK;` |
|     35 | 2227 | `}` |
|     52 | 2228 | `static int vm_builtin_DateTime_createFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2229 | `{` |
|     53 | 2230 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTime");` |
|      1 | 2231 | `}` |
|      2 | 2232 | `static int vm_builtin_DateTimeImmutable_createFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2233 | `{` |
|      3 | 2234 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 2235 | `}` |
|      - | 2236 | `/* createFromImmutable()/createFromMutable()/createFromInterface(): one copy body */` |
|     14 | 2237 | `static int DtCopyOf(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)` |
|      1 | 2238 | `{` |
|     15 | 2239 | `	ph7_vm *pVm = pCtx->pVm;` |
|     15 | 2240 | `	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);` |
|      - | 2241 | `	ph7_class_instance *pSrc,*pObj;` |
|      - | 2242 | `	dt_state sState;` |
|     15 | 2243 | `	if( pClass == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 2244 | `		return PH7_OK;` |
|      - | 2245 | `	}` |
|     15 | 2246 | `	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     15 | 2247 | `	DtLoad(pSrc,&sState);` |
|     15 | 2248 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     15 | 2249 | `	if( pObj == 0 ){` |
|    ! 0 | 2250 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2251 | `	}` |
|     15 | 2252 | `	DtStore(pVm,pObj,&sState);` |
|     15 | 2253 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     15 | 2254 | `	return PH7_OK;` |
|      8 | 2255 | `}` |
|      6 | 2256 | `static int vm_builtin_DateTime_copyOf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2257 | `{` |
|      7 | 2258 | `	return DtCopyOf(pCtx,nArg,apArg,"DateTime");` |
|      1 | 2259 | `}` |
|      8 | 2260 | `static int vm_builtin_DateTimeImmutable_copyOf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2261 | `{` |
|      9 | 2262 | `	return DtCopyOf(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 2263 | `}` |
|      - | 2264 | `/*` |
|      - | 2265 | ` * int\|false strtotime(string $datetime, ?int $baseTimestamp = null)` |
|      - | 2266 | ` *` |
|      - | 2267 | ` * Rides the same DtParse the constructor uses, so its format coverage is identical.` |
|      - | 2268 | ` * php: the EMPTY string is false, but whitespace-only is 'now'; a parse failure is` |
|      - | 2269 | ` * false (never an exception), and the default timezone is offset 0 — exactly what` |
|      - | 2270 | ` * the constructor does for a null $timezone.` |
|      - | 2271 | ` */` |
|    268 | 2272 | `static int vm_builtin_strtotime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2273 | `{` |
|      - | 2274 | `	const char *zIn;` |
|      - | 2275 | `	int nIn;` |
|      - | 2276 | `	sxi64 iBase;` |
|    269 | 2277 | `	sxi64 iTs = 0;` |
|    269 | 2278 | `	sxi32 iOff = 0;` |
|    269 | 2279 | `	int bOffSet = 0,uSec = 0;` |
|    269 | 2280 | `	if( nArg < 1 ){` |
|    ! 0 | 2281 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2282 | `		return PH7_OK;` |
|      - | 2283 | `	}` |
|    269 | 2284 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    269 | 2285 | `	if( nIn < 1 ){` |
|      3 | 2286 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2287 | `		return PH7_OK;` |
|      - | 2288 | `	}` |
|    267 | 2289 | `	iBase = (nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0)` |
|    399 | 2290 | `		? ph7_value_to_int64(apArg[1]) : (sxi64)time(0);` |
|    267 | 2291 | `	if( DtParse(zIn,nIn,iBase,0,&iTs,&iOff,&bOffSet,&uSec) != 0 ){` |
|     23 | 2292 | `		ph7_result_bool(pCtx,0);` |
|     23 | 2293 | `		return PH7_OK;` |
|      - | 2294 | `	}` |
|    245 | 2295 | `	ph7_result_int64(pCtx,iTs);` |
|    245 | 2296 | `	return PH7_OK;` |
|    135 | 2297 | `}` |
|      - | 2298 | `/*` |
|      - | 2299 | ` * ---------------------------------------------------------------------------` |
|      - | 2300 | ` * DateInterval, DatePeriod and its iterator, declared from C.` |
|      - | 2301 | ` *` |
|      - | 2302 | ``  * The rest of the date chunk. DateInterval's two constructors were `preg_match` `` |
|      - | 2303 | `` * calls in PHP; DatePeriod's `getIterator()` was a PHP GENERATOR, which a C body`` |
|      - | 2304 | `` * cannot be -- so it answers a native `InternalIterator`, which is exactly the`` |
|      - | 2305 | ` * class php answers there.` |
|      - | 2306 | ` * ---------------------------------------------------------------------------` |
|      - | 2307 | ` */` |
|      - | 2308 | `/* php's unit words for DateInterval::createFromDateString(), longest first so a` |
|      - | 2309 | ` * prefix never wins over the word that contains it. */` |
|      - | 2310 | `typedef struct dt_unit dt_unit;` |
|      - | 2311 | `struct dt_unit` |
|      - | 2312 | `{` |
|      - | 2313 | `	const char *zName;` |
|      - | 2314 | `	int iField;   /* 0=y 1=m 2=d 3=h 4=i 5=s */` |
|      - | 2315 | `	int nMul;` |
|      - | 2316 | `};` |
|      - | 2317 | `static const dt_unit aDtUnit[] = {` |
|      - | 2318 | `	{ "seconds", 5, 1 }, { "second", 5, 1 }, { "secs", 5, 1 }, { "sec", 5, 1 },` |
|      - | 2319 | `	{ "minutes", 4, 1 }, { "minute", 4, 1 }, { "mins", 4, 1 }, { "min", 4, 1 },` |
|      - | 2320 | `	{ "hours", 3, 1 },   { "hour", 3, 1 },` |
|      - | 2321 | `	{ "fortnights", 2, 14 }, { "fortnight", 2, 14 },` |
|      - | 2322 | `	{ "weeks", 2, 7 },   { "week", 2, 7 },` |
|      - | 2323 | `	{ "days", 2, 1 },    { "day", 2, 1 },` |
|      - | 2324 | `	{ "months", 1, 1 },  { "month", 1, 1 },` |
|      - | 2325 | `	{ "years", 0, 1 },   { "year", 0, 1 },` |
|      - | 2326 | `};` |
|      - | 2327 | `static const char * const azDtIvField[] = { "y", "m", "d", "h", "i", "s" };` |
|      - | 2328 | `/* Read an unsigned run of digits; returns the count consumed. */` |
|    172 | 2329 | `static int DtIvDigits(const char *z,const char *zEnd,sxi64 *pVal)` |
|      2 | 2330 | `{` |
|    174 | 2331 | `	int n = 0;` |
|    174 | 2332 | `	sxi64 v = 0;` |
|    348 | 2333 | `	while( &z[n] < zEnd && SyisDigit(z[n]) ){` |
|    176 | 2334 | `		v = v*10 + (z[n] - '0');` |
|    176 | 2335 | `		n++;` |
|      2 | 2336 | `	}` |
|    174 | 2337 | `	*pVal = v;` |
|    174 | 2338 | `	return n;` |
|      2 | 2339 | `}` |
|      - | 2340 | `/*` |
|      - | 2341 | ` * php's ISO-8601 duration grammar: P[nY][nM][nW][nD][T[nH][nM][nS]], every field` |
|      - | 2342 | ` * an unsigned integer. A bare "P", a trailing "T" and a fractional second are all` |
|      - | 2343 | ` * rejected, as php rejects them.` |
|      - | 2344 | ` */` |
|    112 | 2345 | `static int DtIvParseIso(const char *zIn,int nIn,sxi64 *aOut)` |
|      2 | 2346 | `{` |
|    114 | 2347 | `	const char *z = zIn,*zEnd = &zIn[nIn];` |
|    114 | 2348 | `	int bTime = 0,bAny = 0;` |
|      - | 2349 | `	int k;` |
|    786 | 2350 | `	for( k = 0 ; k < 6 ; k++ ){` |
|    674 | 2351 | `		aOut[k] = 0;` |
|    338 | 2352 | `	}` |
|    114 | 2353 | `	if( nIn < 2 \|\| zIn[0] != 'P' \|\| zIn[nIn-1] == 'T' ){` |
|     11 | 2354 | `		return -1;` |
|      - | 2355 | `	}` |
|    104 | 2356 | `	z++;` |
|    290 | 2357 | `	while( z < zEnd ){` |
|      - | 2358 | `		sxi64 v;` |
|      - | 2359 | `		int n;` |
|    198 | 2360 | `		if( z[0] == 'T' ){` |
|     60 | 2361 | `			if( bTime ){` |
|    ! 0 | 2362 | `				return -1;` |
|      - | 2363 | `			}` |
|     60 | 2364 | `			bTime = 1;` |
|     60 | 2365 | `			z++;` |
|     60 | 2366 | `			continue;` |
|      - | 2367 | `		}` |
|    140 | 2368 | `		n = DtIvDigits(z,zEnd,&v);` |
|    140 | 2369 | `		if( n == 0 \|\| z + n >= zEnd ){` |
|      5 | 2370 | `			return -1;` |
|      - | 2371 | `		}` |
|    136 | 2372 | `		z += n;` |
|    136 | 2373 | `		switch( z[0] ){` |
|      9 | 2374 | `			case 'Y': if( bTime ){ return -1; } aOut[0] += v; break;` |
|      9 | 2375 | `			case 'W': if( bTime ){ return -1; } aOut[2] += v * 7; break;` |
|     27 | 2376 | `			case 'D': if( bTime ){ return -1; } aOut[2] += v; break;` |
|      9 | 2377 | `			case 'H': if( !bTime ){ return -1; } aOut[3] += v; break;` |
|     56 | 2378 | `			case 'S': if( !bTime ){ return -1; } aOut[5] += v; break;` |
|     12 | 2379 | `			case 'M':` |
|      - | 2380 | `				/* The one ambiguous designator: months before T, minutes after. */` |
|     25 | 2381 | `				if( bTime ){ aOut[4] += v; }else{ aOut[1] += v; }` |
|     25 | 2382 | `				break;` |
|      3 | 2383 | `			default:` |
|      7 | 2384 | `				return -1;` |
|      - | 2385 | `		}` |
|    130 | 2386 | `		z++;` |
|    130 | 2387 | `		bAny = 1;` |
|      2 | 2388 | `	}` |
|     94 | 2389 | `	return bAny ? 0 : -1;` |
|     58 | 2390 | `}` |
|      - | 2391 | `/*` |
|      - | 2392 | ` * php's relative-string interval. The string is VALIDATED by the same parser` |
|      - | 2393 | ` * strtotime() uses -- which is where php's "at position N (c): reason" wording` |
|      - | 2394 | ` * comes from -- and the number/unit pairs it understands are then summed. A` |
|      - | 2395 | ` * string the parser accepts but that names no unit ("next monday") is php's` |
|      - | 2396 | ` * all-zero interval, not an error.` |
|      - | 2397 | ` */` |
|     18 | 2398 | `static int DtIvParseRelative(const char *zIn,int nIn,sxi64 *aOut,int *piPos,` |
|      - | 2399 | `	char *pcAt,const char **pzReason)` |
|      1 | 2400 | `{` |
|     19 | 2401 | `	const char *z = zIn,*zEnd = &zIn[nIn];` |
|     19 | 2402 | `	sxi64 iTs = 0;` |
|     19 | 2403 | `	sxi32 iOff = 0;` |
|     19 | 2404 | `	int bOffSet = 0,uSec = 0,iErr;` |
|      - | 2405 | `	int k;` |
|    127 | 2406 | `	for( k = 0 ; k < 6 ; k++ ){` |
|    109 | 2407 | `		aOut[k] = 0;` |
|     55 | 2408 | `	}` |
|     19 | 2409 | `	if( nIn < 1 ){` |
|      3 | 2410 | `		*piPos = 0;` |
|      3 | 2411 | `		*pcAt = ' ';` |
|      3 | 2412 | `		*pzReason = "Empty string";` |
|      3 | 2413 | `		return -1;` |
|      - | 2414 | `	}` |
|     17 | 2415 | `	iErr = DtParse(zIn,nIn,0,0,&iTs,&iOff,&bOffSet,&uSec);` |
|     17 | 2416 | `	if( iErr != 0 ){` |
|    ! 0 | 2417 | `		*pzReason = DtParseErr(zIn,nIn,iErr,piPos,pcAt);` |
|    ! 0 | 2418 | `		return -1;` |
|      - | 2419 | `	}` |
|     41 | 2420 | `	while( z < zEnd ){` |
|      - | 2421 | `		sxi64 v;` |
|     25 | 2422 | `		int n,iSign = 1,iUnit;` |
|     55 | 2423 | `		while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t' \|\| z[0] == '\n' \|\| z[0] == '\r'` |
|     30 | 2424 | `		 \|\| z[0] == ',' \|\| z[0] == '+') ){` |
|     19 | 2425 | `			z++;` |
|      1 | 2426 | `		}` |
|     25 | 2427 | `		if( z < zEnd && z[0] == '-' ){` |
|      3 | 2428 | `			iSign = -1;` |
|      3 | 2429 | `			z++;` |
|      1 | 2430 | `		}` |
|     25 | 2431 | `		n = DtIvDigits(z,zEnd,&v);` |
|     25 | 2432 | `		if( n == 0 ){` |
|      - | 2433 | `			/* Not a number: skip the token (php's parser already accepted the` |
|      - | 2434 | `			 * string, so this is a relative form with no interval field). */` |
|     25 | 2435 | `			while( z < zEnd && z[0] != ' ' && z[0] != ',' ){` |
|     21 | 2436 | `				z++;` |
|      1 | 2437 | `			}` |
|      5 | 2438 | `			continue;` |
|      - | 2439 | `		}` |
|     21 | 2440 | `		z += n;` |
|     51 | 2441 | `		while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|     21 | 2442 | `			z++;` |
|      1 | 2443 | `		}` |
|     21 | 2444 | `		iUnit = -1;` |
|    295 | 2445 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(aDtUnit) ; k++ ){` |
|    295 | 2446 | `			int nU = (int)SyStrlen(aDtUnit[k].zName);` |
|    294 | 2447 | `			if( zEnd - z >= nU && SyStrnicmp(z,aDtUnit[k].zName,(sxu32)nU) == 0` |
|    111 | 2448 | `			 && (zEnd - z == nU \|\| !(SyisAlphaNum(z[nU]) \|\| z[nU] == '_')) ){` |
|     21 | 2449 | `				iUnit = k;` |
|     21 | 2450 | `				z += nU;` |
|     21 | 2451 | `				break;` |
|      - | 2452 | `			}` |
|    138 | 2453 | `		}` |
|     21 | 2454 | `		if( iUnit < 0 ){` |
|    ! 0 | 2455 | `			continue;` |
|      - | 2456 | `		}` |
|     21 | 2457 | `		aOut[aDtUnit[iUnit].iField] += iSign * v * aDtUnit[iUnit].nMul;` |
|      1 | 2458 | `	}` |
|     17 | 2459 | `	return 0;` |
|     10 | 2460 | `}` |
|      - | 2461 | `/* Write the six relative fields onto a DateInterval instance. */` |
|    108 | 2462 | `static void DtIvStore(ph7_vm *pVm,ph7_class_instance *pObj,const sxi64 *aVal)` |
|      2 | 2463 | `{` |
|      - | 2464 | `	int k;` |
|    758 | 2465 | `	for( k = 0 ; k < 6 ; k++ ){` |
|    650 | 2466 | `		PH7_NativeSetAttrInt(pVm,pObj,azDtIvField[k],aVal[k]);` |
|    326 | 2467 | `	}` |
|    110 | 2468 | `}` |
|      - | 2469 | `/*` |
|      - | 2470 | ` * php's date_interval_write_property: what a write to one of DateInterval's` |
|      - | 2471 | ` * properties CONVERTS to, since every one of them is a field of php's own C` |
|      - | 2472 | ` * struct rather than a slot a script's value lands in.` |
|      - | 2473 | ` *` |
|      - | 2474 | `` * The six relative fields and `invert` take php's int cast — a float truncates`` |
|      - | 2475 | ` * and warns where it wraps, a string reads its numeric prefix, an array is 1 —` |
|      - | 2476 | `` * with `invert` narrowed to the 32-bit `int` timelib declares it as (so`` |
|      - | 2477 | `` * `$i->invert = 3000000000` is -1294967296 in both engines). `f` is the`` |
|      - | 2478 | ` * microsecond count above, so its cast warning is raised HERE, on the SCALED` |
|      - | 2479 | ` * value, which is where php raises it.` |
|      - | 2480 | ` *` |
|      - | 2481 | `` * `days` and `from_string` are answered by php's read handler and refused by its`` |
|      - | 2482 | ` * write one: a script that assigns them creates a deprecated DYNAMIC property` |
|      - | 2483 | `` * that never reaches the interval (`$i->days = 5` leaves `$i->days` false`` |
|      - | 2484 | ` * there). §10 refuses a deprecation, and PHL refuses a dynamic property outright,` |
|      - | 2485 | `` * so the two meet at the Error PHL already raises for `$i->anythingElse = v`.`` |
|      - | 2486 | ` */` |
|    612 | 2487 | `static void DtIntervalSet(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)` |
|      2 | 2488 | `{` |
|    614 | 2489 | `	const char *zName = SyStringData(pCtx->pName);` |
|    614 | 2490 | `	sxu32 nName = SyStringLength(pCtx->pName);` |
|    614 | 2491 | `	ph7_value *pVal = pCtx->pValue;` |
|      - | 2492 | `	int bInvert;` |
|    614 | 2493 | `	if( nName == sizeof("days")-1 && SyMemcmp(zName,"days",nName) == 0 ){` |
|      3 | 2494 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 2495 | `			"Cannot create dynamic property DateInterval::$days");` |
|      3 | 2496 | `		pCtx->zThrowClass = "Error";` |
|      3 | 2497 | `		return;` |
|      - | 2498 | `	}` |
|    612 | 2499 | `	if( nName == sizeof("from_string")-1 && SyMemcmp(zName,"from_string",nName) == 0 ){` |
|      3 | 2500 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - | 2501 | `			"Cannot create dynamic property DateInterval::$from_string");` |
|      3 | 2502 | `		pCtx->zThrowClass = "Error";` |
|      3 | 2503 | `		return;` |
|      - | 2504 | `	}` |
|    610 | 2505 | `	if( nName == sizeof("f")-1 && zName[0] == 'f' ){` |
|    492 | 2506 | `		double r = (double)PH7_ValuePeekReal(pVal);` |
|      - | 2507 | `		sxi64 us;` |
|    492 | 2508 | `		PH7_RealWarnIntCast(pVm,r * 1000000.0);` |
|    492 | 2509 | `		us = DtIvUsecOfReal(r);` |
|    492 | 2510 | `		PH7_NativeSetAttrInt(pVm,pThis,DT_IV_US,us);` |
|    492 | 2511 | `		PH7_MemObjRelease(pVal);` |
|    492 | 2512 | `		PH7_MemObjInitFromReal(pVm,pVal,(ph7_real)((double)us / 1000000.0));` |
|    492 | 2513 | `		return;` |
|      - | 2514 | `	}` |
|    119 | 2515 | `	bInvert = nName == sizeof("invert")-1 && SyMemcmp(zName,"invert",nName) == 0;` |
|    119 | 2516 | `	if( !bInvert ){` |
|      - | 2517 | `		int k;` |
|    179 | 2518 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(azDtIvField) ; k++ ){` |
|    179 | 2519 | `			if( nName == 1 && zName[0] == azDtIvField[k][0] ){` |
|     79 | 2520 | `				break;` |
|      - | 2521 | `			}` |
|     51 | 2522 | `		}` |
|     79 | 2523 | `		if( k >= (int)SX_ARRAYSIZE(azDtIvField) ){` |
|    ! 0 | 2524 | `			return;   /* the hidden count slot: written from C, never through here */` |
|      - | 2525 | `		}` |
|     39 | 2526 | `	}` |
|      - | 2527 | `	{` |
|      - | 2528 | `		/* y/m/d/h/i/s and invert, all of them php's int cast. */` |
|      - | 2529 | `		sxi64 iVal;` |
|    119 | 2530 | `		PH7_MemObjWarnIntCast(pVal);` |
|    119 | 2531 | `		iVal = PH7_ValuePeekInt64(pVal);` |
|    119 | 2532 | `		if( bInvert ){` |
|     41 | 2533 | `			iVal = (sxi64)(sxi32)iVal;` |
|     20 | 2534 | `		}` |
|    119 | 2535 | `		PH7_MemObjRelease(pVal);` |
|    119 | 2536 | `		PH7_MemObjInitFromInt(pVm,pVal,iVal);` |
|      - | 2537 | `	}` |
|    308 | 2538 | `}` |
|      - | 2539 | `/* DateInterval::__construct(string $duration) */` |
|    104 | 2540 | `static int vm_builtin_DateInterval_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2541 | `{` |
|    106 | 2542 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 2543 | `	const char *zDur;` |
|      - | 2544 | `	int nDur;` |
|      - | 2545 | `	sxi64 aVal[6];` |
|    106 | 2546 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 2547 | `		return PH7_OK;` |
|      - | 2548 | `	}` |
|    106 | 2549 | `	zDur = ph7_value_to_string(apArg[0],&nDur);` |
|    106 | 2550 | `	if( DtIvParseIso(zDur,nDur,aVal) != 0 ){` |
|     31 | 2551 | `		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",` |
|     10 | 2552 | `			"Unknown or bad format (%.*s)",nDur,zDur);` |
|      - | 2553 | `	}` |
|     86 | 2554 | `	DtIvStore(pCtx->pVm,pThis,aVal);` |
|     86 | 2555 | `	return PH7_OK;` |
|     54 | 2556 | `}` |
|      - | 2557 | `/*` |
|      - | 2558 | ` * DateInterval::createFromDateString(string $datetime). Shared with the` |
|      - | 2559 | ` * date_interval_create_from_date_string() alias, which WARNS and answers false` |
|      - | 2560 | ` * where the method throws.` |
|      - | 2561 | ` */` |
|     18 | 2562 | `static ph7_class_instance * DtIvFromDateString(ph7_context *pCtx,const char *zIn,int nIn,` |
|      - | 2563 | `	int *piPos,char *pcAt,const char **pzReason)` |
|      1 | 2564 | `{` |
|     19 | 2565 | `	ph7_vm *pVm = pCtx->pVm;` |
|     19 | 2566 | `	ph7_class *pClass = DtFactoryClass(pCtx,"DateInterval");` |
|      - | 2567 | `	ph7_class_instance *pObj;` |
|      - | 2568 | `	sxi64 aVal[6];` |
|      - | 2569 | `	ph7_value sVal;` |
|     19 | 2570 | `	if( pClass == 0 ){` |
|    ! 0 | 2571 | `		return 0;` |
|      - | 2572 | `	}` |
|     19 | 2573 | `	if( DtIvParseRelative(zIn,nIn,aVal,piPos,pcAt,pzReason) != 0 ){` |
|      3 | 2574 | `		return 0;` |
|      - | 2575 | `	}` |
|     17 | 2576 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     17 | 2577 | `	if( pObj == 0 ){` |
|    ! 0 | 2578 | `		return 0;` |
|      - | 2579 | `	}` |
|     17 | 2580 | `	DtIvStore(pVm,pObj,aVal);` |
|      - | 2581 | ``	/* php marks the interval as built from a string; the `date_string` property it`` |
|      - | 2582 | `	 * adds with it needs a dynamic property PHL has no equivalent of (§7.4). */` |
|     17 | 2583 | `	PH7_MemObjInitFromBool(pVm,&sVal,1);` |
|     17 | 2584 | `	PH7_NativeSetProp(pVm,pObj,"from_string",sizeof("from_string")-1,&sVal);` |
|     17 | 2585 | `	PH7_MemObjRelease(&sVal);` |
|     17 | 2586 | `	return pObj;` |
|     10 | 2587 | `}` |
|     12 | 2588 | `static int vm_builtin_DateInterval_createFromDateString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2589 | `{` |
|     13 | 2590 | `	const char *zIn,*zReason = "";` |
|     13 | 2591 | `	int nIn,iPos = 0;` |
|     13 | 2592 | `	char cAt = ' ';` |
|      - | 2593 | `	ph7_class_instance *pObj;` |
|     13 | 2594 | `	if( nArg < 1 ){` |
|    ! 0 | 2595 | `		return PH7_OK;` |
|      - | 2596 | `	}` |
|     13 | 2597 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|     13 | 2598 | `	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason);` |
|     13 | 2599 | `	if( pObj == 0 ){` |
|      4 | 2600 | `		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",` |
|      1 | 2601 | `			"Unknown or bad format (%.*s) at position %d (%c): %s",nIn,zIn,iPos,cAt,zReason);` |
|      - | 2602 | `	}` |
|     11 | 2603 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     11 | 2604 | `	return PH7_OK;` |
|      7 | 2605 | `}` |
|      - | 2606 | `/*` |
|      - | 2607 | ` * DateInterval::format(string $format) -- php's own %-token loop, including the` |
|      - | 2608 | `` * rule the chunk got wrong: an UNKNOWN token keeps its '%' (`%q` is "%q").`` |
|      - | 2609 | ` */` |
|    560 | 2610 | `static void DtIvFormat(ph7_context *pCtx,ph7_class_instance *pObj,const char *zFmt,int nFmt)` |
|      2 | 2611 | `{` |
|      - | 2612 | `	SyBlob sOut;` |
|      - | 2613 | `	ph7_value *pDays;` |
|      - | 2614 | `	int k;` |
|    562 | 2615 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   1788 | 2616 | `	for( k = 0 ; k < nFmt ; k++ ){` |
|   1230 | 2617 | `		char c = zFmt[k];` |
|      - | 2618 | `		char t;` |
|   1230 | 2619 | `		if( c != '%' ){` |
|    351 | 2620 | `			SyBlobAppend(&sOut,&c,1);` |
|    351 | 2621 | `			continue;` |
|      - | 2622 | `		}` |
|    880 | 2623 | `		k++;` |
|    880 | 2624 | `		if( k >= nFmt ){` |
|      - | 2625 | `			/* php drops a trailing lone '%' rather than echoing it. */` |
|      3 | 2626 | `			break;` |
|      - | 2627 | `		}` |
|    878 | 2628 | `		t = zFmt[k];` |
|    878 | 2629 | `		switch( t ){` |
|      - | 2630 | ``			/* php prints five of the six through an `(int)` — a 32-bit NARROWING`` |
|      - | 2631 | `			 * of a property it stores as an int64 and hands back whole, so` |
|      - | 2632 | ``			 * `$i->y = 7960523868075137518` reads back in full and prints`` |
|      - | 2633 | `			 * 111352302. The SECONDS are the exception: php formats those with` |
|      - | 2634 | `			 * its long specifier, in both cases. */` |
|      7 | 2635 | `			case 'Y': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"y")); break;` |
|     13 | 2636 | `			case 'y': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"y")); break;` |
|      7 | 2637 | `			case 'M': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"m")); break;` |
|     13 | 2638 | `			case 'm': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"m")); break;` |
|      7 | 2639 | `			case 'D': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"d")); break;` |
|     15 | 2640 | `			case 'd': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"d")); break;` |
|      7 | 2641 | `			case 'H': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"h")); break;` |
|     41 | 2642 | `			case 'h': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"h")); break;` |
|      7 | 2643 | `			case 'I': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"i")); break;` |
|     41 | 2644 | `			case 'i': SyBlobFormat(&sOut,"%d",(int)PH7_NativeAttrInt(pObj,"i")); break;` |
|      7 | 2645 | `			case 'S': SyBlobFormat(&sOut,"%02qd",PH7_NativeAttrInt(pObj,"s")); break;` |
|     43 | 2646 | `			case 's': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"s")); break;` |
|    287 | 2647 | `			case 'F': case 'f': {` |
|      - | 2648 | `				/* php prints the STORED microsecond count, which is why` |
|      - | 2649 | ``				 * `$i->f = 0.1234567` prints 123456 rather than the 123457 a`` |
|      - | 2650 | `				 * rounding of the float would give, and why a count no double` |
|      - | 2651 | `				 * holds exactly still prints its own digits. The conversion the` |
|      - | 2652 | `				 * cast contract lives in — truncate toward zero, wrap what no` |
|      - | 2653 | `				 * int64 holds, 0 for a NaN or an infinity, and php's warning` |
|      - | 2654 | `				 * beside it — happens at the property WRITE, where php does it. */` |
|    576 | 2655 | `				sxi64 uS = DtIvUsec(pObj);` |
|    576 | 2656 | `				if( t == 'F' ){` |
|     85 | 2657 | `					SyBlobFormat(&sOut,"%06qd",uS);` |
|     43 | 2658 | `				}else{` |
|    492 | 2659 | `					SyBlobFormat(&sOut,"%qd",uS);` |
|      - | 2660 | `				}` |
|    576 | 2661 | `				break;` |
|      - | 2662 | `			}` |
|     45 | 2663 | `			case 'R': SyBlobAppend(&sOut,PH7_NativeAttrInt(pObj,"invert") ? "-" : "+",1); break;` |
|      5 | 2664 | `			case 'r': if( PH7_NativeAttrInt(pObj,"invert") ){ SyBlobAppend(&sOut,"-",1); } break;` |
|     25 | 2665 | `			case 'a':` |
|     51 | 2666 | `				pDays = PH7_NativeAttr(pObj,"days");` |
|     51 | 2667 | `				if( pDays && (pDays->iFlags & MEMOBJ_INT) ){` |
|     45 | 2668 | `					SyBlobFormat(&sOut,"%qd",pDays->x.iVal);` |
|     23 | 2669 | `				}else{` |
|      7 | 2670 | `					SyBlobAppend(&sOut,"(unknown)",sizeof("(unknown)")-1);` |
|      - | 2671 | `				}` |
|     51 | 2672 | `				break;` |
|      7 | 2673 | `			case '%': SyBlobAppend(&sOut,"%",1); break;` |
|      1 | 2674 | `			default:` |
|      - | 2675 | `				/* php keeps BOTH bytes of an unrecognised token. */` |
|      3 | 2676 | `				SyBlobAppend(&sOut,"%",1);` |
|      3 | 2677 | `				SyBlobAppend(&sOut,&t,1);` |
|      2 | 2678 | `				break;` |
|      - | 2679 | `		}` |
|    440 | 2680 | `	}` |
|    562 | 2681 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    562 | 2682 | `	SyBlobRelease(&sOut);` |
|    562 | 2683 | `}` |
|    556 | 2684 | `static int vm_builtin_DateInterval_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2685 | `{` |
|    558 | 2686 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 2687 | `	const char *zFmt;` |
|      - | 2688 | `	int nFmt;` |
|    558 | 2689 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 2690 | `		return PH7_OK;` |
|      - | 2691 | `	}` |
|    558 | 2692 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    558 | 2693 | `	DtIvFormat(pCtx,pThis,zFmt,nFmt);` |
|    558 | 2694 | `	return PH7_OK;` |
|    280 | 2695 | `}` |
|      - | 2696 | `/* Is this value an instance of the named class? */` |
|     82 | 2697 | `static int DtValueIsA(ph7_vm *pVm,ph7_value *pVal,const char *zClass)` |
|      1 | 2698 | `{` |
|      - | 2699 | `	ph7_class *pClass;` |
|     83 | 2700 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      3 | 2701 | `		return 0;` |
|      - | 2702 | `	}` |
|     81 | 2703 | `	pClass = DtClass(&(*pVm),zClass);` |
|     81 | 2704 | `	return pClass != 0` |
|     80 | 2705 | `		&& PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass);` |
|     42 | 2706 | `}` |
|      - | 2707 | `/*` |
|      - | 2708 | ` * Write an object (or null) into a declared property of another object.` |
|      - | 2709 | ` *` |
|      - | 2710 | ` * The scratch value ALIASES the instance rather than owning it, and` |
|      - | 2711 | ` * PH7_MemObjStore takes the reference the slot keeps -- so releasing the scratch` |
|      - | 2712 | ` * afterwards would hand back the slot's own reference and free the object out from` |
|      - | 2713 | `` * under it (which is what it did: `foreach` over a DatePeriod crashed on the second`` |
|      - | 2714 | `` * element's `->format()`). The caller keeps owning whatever it passed in.`` |
|      - | 2715 | ` */` |
|      - | 2716 | `/*` |
|      - | 2717 | ` * DatePeriod::__construct($start, $interval, $end, $options)` |
|      - | 2718 | ` *` |
|      - | 2719 | ` * php overloads it three ways and rejects everything else with ONE message, which` |
|      - | 2720 | ` * is why the signature stays unenforced and the shapes are checked here.` |
|      - | 2721 | ` */` |
|     46 | 2722 | `static int DpConstructInto(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,ph7_value **apArg,` |
|      - | 2723 | `	const char *zIsoStartClass)` |
|      1 | 2724 | `{` |
|     47 | 2725 | `	ph7_vm *pVm = pCtx->pVm;` |
|     47 | 2726 | `	sxi64 iOptions = 0;` |
|      - | 2727 | `	static const char *zBadArgs =` |
|      - | 2728 | `		"DatePeriod::__construct() accepts (DateTimeInterface, DateInterval, int [, int]), "` |
|      - | 2729 | `		"or (DateTimeInterface, DateInterval, DateTime [, int]), or (string [, int]) as arguments";` |
|     47 | 2730 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 2731 | `		return PH7_VmThrowException(pCtx,"TypeError","%s",zBadArgs);` |
|      - | 2732 | `	}` |
|     47 | 2733 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|      - | 2734 | `		/* The ISO-8601 form: "R<n>/<start>/<duration>". php's second argument is` |
|      - | 2735 | `		 * then the OPTIONS bitmask, not an interval. */` |
|     11 | 2736 | `		const char *zSpec = (const char *)SyBlobData(&apArg[0]->sBlob);` |
|     11 | 2737 | `		int nSpec = (int)SyBlobLength(&apArg[0]->sBlob);` |
|      - | 2738 | `		const char *zStart,*zDur;` |
|      - | 2739 | `		int nStart,nDur,k;` |
|     11 | 2740 | `		sxi64 nRec = 0;` |
|      - | 2741 | `		sxi64 aIv[6];` |
|      - | 2742 | `		dt_state sState;` |
|      - | 2743 | `		char zNameBuf[16];` |
|      - | 2744 | `		const char *zErr;` |
|      - | 2745 | `		int iPos,nDigits;` |
|      - | 2746 | `		char cAt;` |
|      - | 2747 | `		ph7_class_instance *pStart,*pIv;` |
|     11 | 2748 | `		if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_INT) ){` |
|    ! 0 | 2749 | `			iOptions = apArg[1]->x.iVal;` |
|    ! 0 | 2750 | `		}` |
|     11 | 2751 | `		if( nSpec < 2 \|\| zSpec[0] != 'R' ){` |
|    ! 0 | 2752 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 2753 | `				"Unknown or bad format (%.*s)",nSpec,zSpec);` |
|      - | 2754 | `		}` |
|     11 | 2755 | `		nDigits = DtIvDigits(&zSpec[1],&zSpec[nSpec],&nRec);` |
|     11 | 2756 | `		k = 1 + nDigits;` |
|     11 | 2757 | `		if( nDigits == 0 \|\| k >= nSpec \|\| zSpec[k] != '/' ){` |
|    ! 0 | 2758 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 2759 | `				"Unknown or bad format (%.*s)",nSpec,zSpec);` |
|      - | 2760 | `		}` |
|     11 | 2761 | `		zStart = &zSpec[k+1];` |
|     11 | 2762 | `		nStart = 0;` |
|    179 | 2763 | `		while( &zStart[nStart] < &zSpec[nSpec] && zStart[nStart] != '/' ){` |
|    169 | 2764 | `			nStart++;` |
|      1 | 2765 | `		}` |
|     11 | 2766 | `		if( &zStart[nStart] >= &zSpec[nSpec] ){` |
|    ! 0 | 2767 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 2768 | `				"Unknown or bad format (%.*s)",nSpec,zSpec);` |
|      - | 2769 | `		}` |
|     11 | 2770 | `		zDur = &zStart[nStart+1];` |
|     11 | 2771 | `		nDur = (int)(&zSpec[nSpec] - zDur);` |
|     15 | 2772 | `		if( DtInitState(pCtx,zStart,nStart,0,pVm->zDefTz,(int)pVm->nDefTz,&sState,` |
|     10 | 2773 | `			zNameBuf,sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0` |
|     10 | 2774 | `		 \|\| DtIvParseIso(zDur,nDur,aIv) != 0 ){` |
|      4 | 2775 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|      1 | 2776 | `				"Unknown or bad format (%.*s)",nSpec,zSpec);` |
|      - | 2777 | `		}` |
|      - | 2778 | `		/* php's two ISO entry points disagree on the class they build, and both` |
|      - | 2779 | ``		 * answers are load-bearing: `new DatePeriod("R2/...")` yields DateTime`` |
|      - | 2780 | `		 * where DatePeriod::createFromISO8601String() yields DateTimeImmutable. */` |
|      9 | 2781 | `		pStart = PH7_NewClassInstance(pVm,DtClass(pVm,zIsoStartClass));` |
|      9 | 2782 | `		pIv = PH7_NewClassInstance(pVm,DtClass(pVm,"DateInterval"));` |
|      9 | 2783 | `		if( pStart == 0 \|\| pIv == 0 ){` |
|    ! 0 | 2784 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2785 | `		}` |
|      9 | 2786 | `		DtStore(pVm,pStart,&sState);` |
|      9 | 2787 | `		DtIvStore(pVm,pIv,aIv);` |
|      9 | 2788 | `		PH7_NativeSetAttrObj(pVm,pThis,"start",pStart);` |
|      9 | 2789 | `		PH7_NativeSetAttrObj(pVm,pThis,"interval",pIv);` |
|      9 | 2790 | `		PH7_ClassInstanceUnref(pStart);` |
|      9 | 2791 | `		PH7_ClassInstanceUnref(pIv);` |
|      9 | 2792 | `		PH7_NativeSetAttrInt(pVm,pThis,"recurrences",nRec + 1);` |
|      5 | 2793 | `	}else{` |
|      - | 2794 | `		ph7_class_instance *pStart,*pIv,*pEnd;` |
|     36 | 2795 | `		if( !DtValueIsA(pVm,apArg[0],"DateTimeInterface")` |
|     36 | 2796 | `		 \|\| nArg < 3` |
|     35 | 2797 | `		 \|\| !DtValueIsA(pVm,apArg[1],"DateInterval")` |
|     34 | 2798 | `		 \|\| ((apArg[2]->iFlags & MEMOBJ_INT) == 0` |
|     22 | 2799 | `		     && !DtValueIsA(pVm,apArg[2],"DateTimeInterface")) ){` |
|      5 | 2800 | `			return PH7_VmThrowException(pCtx,"TypeError","%s",zBadArgs);` |
|      - | 2801 | `		}` |
|     33 | 2802 | `		if( nArg > 3 ){` |
|      9 | 2803 | `			iOptions = ph7_value_to_int64(apArg[3]);` |
|      4 | 2804 | `		}` |
|     33 | 2805 | `		pStart = PH7_CloneClassInstance((ph7_class_instance *)apArg[0]->x.pOther);` |
|     33 | 2806 | `		pIv = (ph7_class_instance *)apArg[1]->x.pOther;` |
|     33 | 2807 | `		if( pStart == 0 ){` |
|    ! 0 | 2808 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2809 | `		}` |
|     33 | 2810 | `		PH7_NativeSetAttrObj(pVm,pThis,"start",pStart);` |
|     33 | 2811 | `		PH7_ClassInstanceUnref(pStart);` |
|     33 | 2812 | `		PH7_NativeSetAttrObj(pVm,pThis,"interval",pIv);` |
|     33 | 2813 | `		if( apArg[2]->iFlags & MEMOBJ_INT ){` |
|     21 | 2814 | `			PH7_NativeSetAttrInt(pVm,pThis,"recurrences",apArg[2]->x.iVal + 1);` |
|     11 | 2815 | `		}else{` |
|     13 | 2816 | `			pEnd = PH7_CloneClassInstance((ph7_class_instance *)apArg[2]->x.pOther);` |
|     13 | 2817 | `			if( pEnd == 0 ){` |
|    ! 0 | 2818 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2819 | `			}` |
|     13 | 2820 | `			PH7_NativeSetAttrObj(pVm,pThis,"end",pEnd);` |
|     13 | 2821 | `			PH7_ClassInstanceUnref(pEnd);` |
|      - | 2822 | `		}` |
|      - | 2823 | `	}` |
|     41 | 2824 | `	PH7_NativeSetAttrBool(pVm,pThis,"include_start_date",(iOptions & 1) == 0);` |
|     41 | 2825 | `	PH7_NativeSetAttrBool(pVm,pThis,"include_end_date",(iOptions & 2) != 0);` |
|     41 | 2826 | `	return PH7_OK;` |
|     24 | 2827 | `}` |
|     42 | 2828 | `static int vm_builtin_DatePeriod_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2829 | `{` |
|     43 | 2830 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|     43 | 2831 | `	if( pThis == 0 ){` |
|    ! 0 | 2832 | `		return PH7_OK;` |
|      - | 2833 | `	}` |
|     43 | 2834 | `	return DpConstructInto(pCtx,pThis,nArg,apArg,"DateTime");` |
|     22 | 2835 | `}` |
|      - | 2836 | `/* DatePeriod::createFromISO8601String(string $specification, int $options = 0) */` |
|      4 | 2837 | `static int vm_builtin_DatePeriod_createFromISO8601String(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2838 | `{` |
|      5 | 2839 | `	ph7_vm *pVm = pCtx->pVm;` |
|      5 | 2840 | `	ph7_class *pClass = DtFactoryClass(pCtx,"DatePeriod");` |
|      - | 2841 | `	ph7_class_instance *pObj;` |
|      - | 2842 | `	sxi32 rc;` |
|      5 | 2843 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 2844 | `		return PH7_OK;` |
|      - | 2845 | `	}` |
|      5 | 2846 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|      5 | 2847 | `	if( pObj == 0 ){` |
|    ! 0 | 2848 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2849 | `	}` |
|      - | 2850 | `	/* php's factory IS the constructor, with the same overloaded argument shape. */` |
|      5 | 2851 | `	rc = DpConstructInto(pCtx,pObj,nArg,apArg,"DateTimeImmutable");` |
|      5 | 2852 | `	if( rc != PH7_OK \|\| pCtx->nThrowRc != 0 ){` |
|    ! 0 | 2853 | `		PH7_ClassInstanceUnref(pObj);` |
|    ! 0 | 2854 | `		return rc;` |
|      - | 2855 | `	}` |
|      5 | 2856 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      5 | 2857 | `	return PH7_OK;` |
|      3 | 2858 | `}` |
|     10 | 2859 | `static int vm_builtin_DatePeriod_getStartDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2860 | `{` |
|     11 | 2861 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      5 | 2862 | `	SXUNUSED(nArg);` |
|      5 | 2863 | `	SXUNUSED(apArg);` |
|     11 | 2864 | `	if( pThis ){` |
|     11 | 2865 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"start");` |
|     11 | 2866 | `		if( pVal ){` |
|     11 | 2867 | `			ph7_result_value(pCtx,pVal);` |
|      5 | 2868 | `		}` |
|      5 | 2869 | `	}` |
|     11 | 2870 | `	return PH7_OK;` |
|      1 | 2871 | `}` |
|      4 | 2872 | `static int vm_builtin_DatePeriod_getEndDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2873 | `{` |
|      5 | 2874 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      2 | 2875 | `	SXUNUSED(nArg);` |
|      2 | 2876 | `	SXUNUSED(apArg);` |
|      5 | 2877 | `	if( pThis ){` |
|      5 | 2878 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"end");` |
|      5 | 2879 | `		if( pVal ){` |
|      5 | 2880 | `			ph7_result_value(pCtx,pVal);` |
|      2 | 2881 | `		}` |
|      2 | 2882 | `	}` |
|      5 | 2883 | `	return PH7_OK;` |
|      1 | 2884 | `}` |
|      2 | 2885 | `static int vm_builtin_DatePeriod_getDateInterval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2886 | `{` |
|      3 | 2887 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      1 | 2888 | `	SXUNUSED(nArg);` |
|      1 | 2889 | `	SXUNUSED(apArg);` |
|      3 | 2890 | `	if( pThis ){` |
|      3 | 2891 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"interval");` |
|      3 | 2892 | `		if( pVal ){` |
|      3 | 2893 | `			ph7_result_value(pCtx,pVal);` |
|      1 | 2894 | `		}` |
|      1 | 2895 | `	}` |
|      3 | 2896 | `	return PH7_OK;` |
|      1 | 2897 | `}` |
|      - | 2898 | `/*` |
|      - | 2899 | ` * DatePeriod::getRecurrences() -- php answers NULL for a period bounded by an END` |
|      - | 2900 | `` * DATE and the recurrence COUNT otherwise, which is `recurrences - 1` (php stores`` |
|      - | 2901 | ` * the count of dates, one more than the recurrences). No private slot needed.` |
|      - | 2902 | ` */` |
|      6 | 2903 | `static int vm_builtin_DatePeriod_getRecurrences(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2904 | `{` |
|      7 | 2905 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      3 | 2906 | `	SXUNUSED(nArg);` |
|      3 | 2907 | `	SXUNUSED(apArg);` |
|      7 | 2908 | `	if( pThis == 0 \|\| PH7_NativeAttrObj(pThis,"end") != 0 ){` |
|      3 | 2909 | `		ph7_result_null(pCtx);` |
|      3 | 2910 | `		return PH7_OK;` |
|      - | 2911 | `	}` |
|      5 | 2912 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,"recurrences") - 1);` |
|      5 | 2913 | `	return PH7_OK;` |
|      4 | 2914 | `}` |
|      - | 2915 | `/*` |
|      - | 2916 | ` * The period walk, expressed as the vtable an InternalIterator drives (oo_native.c).` |
|      - | 2917 | ` * It uses the shared cursor slots: SRC is the period, CUR the date the cursor sits` |
|      - | 2918 | ` * on, KEY the emitted position and POS the loop counter (which differs from KEY,` |
|      - | 2919 | ` * since an excluded start date is stepped over without emitting one).` |
|      - | 2920 | ` */` |
|      - | 2921 | `#define DP_IT_STEP PH7_NATIVE_IT_POS` |
|      - | 2922 | `/*` |
|      - | 2923 | ` * One interval step from a date object: a NEW object, so a value already handed` |
|      - | 2924 | ` * to the caller is never mutated underneath it (php's iterator answers a fresh` |
|      - | 2925 | ` * object per position too).` |
|      - | 2926 | ` *` |
|      - | 2927 | `` * The step always ADDS, whatever the interval's `invert` says -- php's period`` |
|      - | 2928 | ` * walk reads the fields and not the flag, so a period built on an interval a` |
|      - | 2929 | `` * diff() answered (or on `$iv->invert = 1`) still runs FORWARD, while a`` |
|      - | 2930 | `` * negative FIELD (`createFromDateString('-1 day')` leaves d = -1 and invert 0)`` |
|      - | 2931 | ` * really does step backward. PHL honoured the flag, so such a period walked the` |
|      - | 2932 | ` * wrong way -- and with an END date rather than a recurrence count it walked` |
|      - | 2933 | ` * away from that end, stopped by nothing.` |
|      - | 2934 | ` */` |
|    134 | 2935 | `static ph7_class_instance * DpAdvance(ph7_vm *pVm,ph7_class_instance *pCur,` |
|      - | 2936 | `	ph7_class_instance *pIv)` |
|      1 | 2937 | `{` |
|    135 | 2938 | `	ph7_class_instance *pNext = PH7_CloneClassInstance(pCur);` |
|    135 | 2939 | `	if( pNext == 0 ){` |
|    ! 0 | 2940 | `		return 0;` |
|      - | 2941 | `	}` |
|    135 | 2942 | `	DtApplyInterval(&(*pVm),pCur,pNext,pIv,1);` |
|    135 | 2943 | `	return pNext;` |
|     68 | 2944 | `}` |
|      - | 2945 | `/*` |
|      - | 2946 | ` * Settle the cursor on the next date the period EMITS, mirroring the generator` |
|      - | 2947 | ` * this replaced: a start excluded by EXCLUDE_START_DATE is stepped over, an end` |
|      - | 2948 | ` * date stops the walk (inclusively under INCLUDE_END_DATE) and a recurrence count` |
|      - | 2949 | ` * bounds the number of steps instead.` |
|      - | 2950 | ` */` |
|    206 | 2951 | `static void DpSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 | 2952 | `{` |
|    207 | 2953 | `	ph7_class_instance *pPeriod = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|      - | 2954 | `	ph7_class_instance *pEnd,*pIv;` |
|      - | 2955 | `	int bInclStart,bInclEnd;` |
|      - | 2956 | `	sxi64 nTotal;` |
|    207 | 2957 | `	if( pPeriod == 0 ){` |
|    ! 0 | 2958 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 2959 | `		return;` |
|      - | 2960 | `	}` |
|    207 | 2961 | `	pEnd = PH7_NativeAttrObj(pPeriod,"end");` |
|    207 | 2962 | `	pIv = PH7_NativeAttrObj(pPeriod,"interval");` |
|    207 | 2963 | `	bInclStart = PH7_NativeAttrTruthy(pPeriod,"include_start_date");` |
|    207 | 2964 | `	bInclEnd = PH7_NativeAttrTruthy(pPeriod,"include_end_date");` |
|    207 | 2965 | `	nTotal = PH7_NativeAttrInt(pPeriod,"recurrences") + (bInclEnd ? 1 : 0);` |
|    111 | 2966 | `	for(;;){` |
|    215 | 2967 | `		ph7_class_instance *pCur = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_CUR);` |
|    215 | 2968 | `		sxi64 iStep = PH7_NativeAttrInt(pIt,DP_IT_STEP);` |
|      - | 2969 | `		ph7_class_instance *pNext;` |
|    215 | 2970 | `		if( pCur == 0 ){` |
|    ! 0 | 2971 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 2972 | `			return;` |
|      - | 2973 | `		}` |
|    215 | 2974 | `		if( pEnd != 0 ){` |
|     57 | 2975 | `			sxi64 iTs = PH7_NativeAttrInt(pCur,DT_TS);` |
|     57 | 2976 | `			sxi64 iEndTs = PH7_NativeAttrInt(pEnd,DT_TS);` |
|     57 | 2977 | `			if( bInclEnd ? (iTs > iEndTs) : (iTs >= iEndTs) ){` |
|     11 | 2978 | `				PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|     11 | 2979 | `				return;` |
|      1 | 2980 | `			}` |
|    182 | 2981 | `		}else if( iStep >= nTotal ){` |
|     29 | 2982 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|     29 | 2983 | `			return;` |
|      - | 2984 | `		}` |
|    177 | 2985 | `		if( iStep > 0 \|\| bInclStart ){` |
|    169 | 2986 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|    169 | 2987 | `			return;` |
|      - | 2988 | `		}` |
|      - | 2989 | `		/* The excluded start: step over it without emitting a key. */` |
|      9 | 2990 | `		if( pIv == 0 ){` |
|    ! 0 | 2991 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 2992 | `			return;` |
|      - | 2993 | `		}` |
|      9 | 2994 | `		pNext = DpAdvance(&(*pVm),pCur,pIv);` |
|      9 | 2995 | `		if( pNext == 0 ){` |
|    ! 0 | 2996 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 2997 | `			return;` |
|      - | 2998 | `		}` |
|      9 | 2999 | `		PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_CUR,pNext);` |
|      9 | 3000 | `		PH7_ClassInstanceUnref(pNext);` |
|      9 | 3001 | `		PH7_NativeSetAttrInt(&(*pVm),pIt,DP_IT_STEP,iStep + 1);` |
|      1 | 3002 | `	}` |
|    104 | 3003 | `}` |
|     80 | 3004 | `static void DpRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 | 3005 | `{` |
|      - | 3006 | `	ph7_class_instance *pPeriod,*pStart,*pCur;` |
|     81 | 3007 | `	pPeriod = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_SRC);` |
|     81 | 3008 | `	pStart = pPeriod ? PH7_NativeAttrObj(pPeriod,"start") : 0;` |
|     81 | 3009 | `	pCur = pStart ? PH7_CloneClassInstance(pStart) : 0;` |
|     81 | 3010 | `	if( pCur == 0 ){` |
|    ! 0 | 3011 | `		PH7_NativeSetAttrBool(pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 3012 | `		return;` |
|      - | 3013 | `	}` |
|     81 | 3014 | `	PH7_NativeSetAttrObj(pVm,pThis,PH7_NATIVE_IT_CUR,pCur);` |
|     81 | 3015 | `	PH7_ClassInstanceUnref(pCur);` |
|     81 | 3016 | `	PH7_NativeSetAttrInt(pVm,pThis,PH7_NATIVE_IT_KEY,0);` |
|     81 | 3017 | `	PH7_NativeSetAttrInt(pVm,pThis,DP_IT_STEP,0);` |
|     81 | 3018 | `	DpSettle(pVm,pThis);` |
|     41 | 3019 | `}` |
|    126 | 3020 | `static void DpNext(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 | 3021 | `{` |
|      - | 3022 | `	ph7_class_instance *pPeriod,*pIv,*pCur,*pNext;` |
|    127 | 3023 | `	pPeriod = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_SRC);` |
|    127 | 3024 | `	pIv = pPeriod ? PH7_NativeAttrObj(pPeriod,"interval") : 0;` |
|    127 | 3025 | `	pCur = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_CUR);` |
|    127 | 3026 | `	pNext = (pIv && pCur) ? DpAdvance(pVm,pCur,pIv) : 0;` |
|    127 | 3027 | `	if( pNext == 0 ){` |
|    ! 0 | 3028 | `		PH7_NativeSetAttrBool(pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 3029 | `		return;` |
|      - | 3030 | `	}` |
|    127 | 3031 | `	PH7_NativeSetAttrObj(pVm,pThis,PH7_NATIVE_IT_CUR,pNext);` |
|    127 | 3032 | `	PH7_ClassInstanceUnref(pNext);` |
|    127 | 3033 | `	PH7_NativeSetAttrInt(pVm,pThis,DP_IT_STEP,PH7_NativeAttrInt(pThis,DP_IT_STEP) + 1);` |
|    127 | 3034 | `	PH7_NativeSetAttrInt(pVm,pThis,PH7_NATIVE_IT_KEY,PH7_NativeAttrInt(pThis,PH7_NATIVE_IT_KEY) + 1);` |
|    127 | 3035 | `	DpSettle(pVm,pThis);` |
|     64 | 3036 | `}` |
|      - | 3037 | `static const PH7_NativeIterVtab sDpIterVtab = { DpRewind, DpNext };` |
|      - | 3038 | `/*` |
|      - | 3039 | ` * DatePeriod::getIterator(): Iterator` |
|      - | 3040 | ` *` |
|      - | 3041 | ` * This was a PHP GENERATOR, the one thing a C body cannot be. php answers an` |
|      - | 3042 | ` * InternalIterator here, so PHL answers the shared one (oo_native.c) driven by` |
|      - | 3043 | `` * the vtable above -- and stops diverging on `get_class($period->getIterator())`.`` |
|      - | 3044 | ` * A fresh one per call, as php's is.` |
|      - | 3045 | ` */` |
|     42 | 3046 | `static int vm_builtin_DatePeriod_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3047 | `{` |
|     43 | 3048 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 3049 | `	ph7_class_instance *pIt;` |
|     21 | 3050 | `	SXUNUSED(nArg);` |
|     21 | 3051 | `	SXUNUSED(apArg);` |
|     43 | 3052 | `	if( pThis == 0 ){` |
|    ! 0 | 3053 | `		return PH7_OK;` |
|      - | 3054 | `	}` |
|     43 | 3055 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|     43 | 3056 | `	if( pIt == 0 ){` |
|    ! 0 | 3057 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3058 | `	}` |
|     43 | 3059 | `	PH7_NativeResultObject(pCtx,pIt);` |
|     43 | 3060 | `	return PH7_OK;` |
|     22 | 3061 | `}` |
|      - | 3062 | `/*` |
|      - | 3063 | ` * ---------------------------------------------------------------------------` |
|      - | 3064 | ` * The procedural date API.` |
|      - | 3065 | ` *` |
|      - | 3066 | ` * php's aliases are functions in their own right, not forwards: they reach the` |
|      - | 3067 | ` * same implementation the methods do, so an overridden method in a subclass is` |
|      - | 3068 | ` * NOT what they call, and the ones that can fail WARN and answer false where the` |
|      - | 3069 | ` * method throws. Each owes aBuiltinSig[] a row (vm_arg_check.c).` |
|      - | 3070 | ` * ---------------------------------------------------------------------------` |
|      - | 3071 | ` */` |
|      - | 3072 | `/* The receiver argument of a procedural alias (already type-screened by its row). */` |
|     56 | 3073 | `static ph7_class_instance * DtArgObj(int nArg,ph7_value **apArg,int iArg)` |
|      1 | 3074 | `{` |
|     57 | 3075 | `	if( iArg >= nArg \|\| (apArg[iArg]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 3076 | `		return 0;` |
|      - | 3077 | `	}` |
|     57 | 3078 | `	return (ph7_class_instance *)apArg[iArg]->x.pOther;` |
|     29 | 3079 | `}` |
|      - | 3080 | `/* Answer the receiver itself, the way every mutating alias does. */` |
|     12 | 3081 | `static void DtResultArg(ph7_context *pCtx,ph7_value **apArg)` |
|      1 | 3082 | `{` |
|     13 | 3083 | `	ph7_result_value(pCtx,apArg[0]);` |
|     13 | 3084 | `}` |
|      - | 3085 | `/* date_create()/date_create_immutable(): php answers false on a parse failure and` |
|      - | 3086 | ` * says nothing -- the constructor's exception does not escape the alias. */` |
|     28 | 3087 | `static int DtProcCreate(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zClass)` |
|      1 | 3088 | `{` |
|     29 | 3089 | `	ph7_vm *pVm = pCtx->pVm;` |
|     29 | 3090 | `	ph7_class *pClass = DtClass(pVm,zClass);` |
|      - | 3091 | `	ph7_class_instance *pObj;` |
|     29 | 3092 | `	const char *zIn = "now",*zZone;` |
|     29 | 3093 | `	int nIn = 3,nZone,iPos;` |
|     29 | 3094 | `	sxi32 iZoneOff = 0;` |
|      - | 3095 | `	dt_state sState;` |
|      - | 3096 | `	char zNameBuf[16],cAt;` |
|      - | 3097 | `	const char *zErr;` |
|     29 | 3098 | `	if( pClass == 0 ){` |
|    ! 0 | 3099 | `		return PH7_OK;` |
|      - | 3100 | `	}` |
|     29 | 3101 | `	zZone = pVm->zDefTz;` |
|     29 | 3102 | `	nZone = (int)pVm->nDefTz;` |
|     29 | 3103 | `	if( nArg > 0 ){` |
|     29 | 3104 | `		zIn = ph7_value_to_string(apArg[0],&nIn);` |
|     14 | 3105 | `	}` |
|     29 | 3106 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|      7 | 3107 | `		DtZoneOf(apArg[1],&iZoneOff,&zZone,&nZone);` |
|      3 | 3108 | `	}` |
|     28 | 3109 | `	if( DtInitState(pCtx,zIn,nIn,iZoneOff,zZone,nZone,&sState,zNameBuf,sizeof(zNameBuf),` |
|     15 | 3110 | `		&zErr,&iPos,&cAt) != 0 ){` |
|      5 | 3111 | `		DtLastErrOne(pVm,iPos,zErr);` |
|      5 | 3112 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3113 | `		return PH7_OK;` |
|      - | 3114 | `	}` |
|     25 | 3115 | `	DtLastErrClear(pVm);` |
|     25 | 3116 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     25 | 3117 | `	if( pObj == 0 ){` |
|    ! 0 | 3118 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3119 | `	}` |
|     25 | 3120 | `	DtStore(pVm,pObj,&sState);` |
|     25 | 3121 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     25 | 3122 | `	return PH7_OK;` |
|     15 | 3123 | `}` |
|     24 | 3124 | `static int vm_builtin_date_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3125 | `{` |
|     25 | 3126 | `	return DtProcCreate(pCtx,nArg,apArg,"DateTime");` |
|      1 | 3127 | `}` |
|      4 | 3128 | `static int vm_builtin_date_create_immutable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3129 | `{` |
|      5 | 3130 | `	return DtProcCreate(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 3131 | `}` |
|     10 | 3132 | `static int vm_builtin_date_create_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3133 | `{` |
|     11 | 3134 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTime");` |
|      1 | 3135 | `}` |
|      4 | 3136 | `static int vm_builtin_date_create_immutable_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3137 | `{` |
|      5 | 3138 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 3139 | `}` |
|      6 | 3140 | `static int vm_builtin_date_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3141 | `{` |
|      7 | 3142 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      - | 3143 | `	const char *zFmt;` |
|      - | 3144 | `	int nFmt;` |
|      7 | 3145 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|    ! 0 | 3146 | `		return PH7_OK;` |
|      - | 3147 | `	}` |
|      7 | 3148 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|      7 | 3149 | `	DtFormatOf(pCtx,pObj,zFmt,nFmt);` |
|      7 | 3150 | `	return PH7_OK;` |
|      4 | 3151 | `}` |
|      - | 3152 | `/* date_modify(): php WARNS and answers false where DateTime::modify() throws. */` |
|    ! 0 | 3153 | `static int vm_builtin_date_modify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3154 | `{` |
|    ! 0 | 3155 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      - | 3156 | `	const char *zMod,*zErr;` |
|      - | 3157 | `	int nMod,iPos,iErrPos;` |
|      - | 3158 | `	char cAt;` |
|    ! 0 | 3159 | `	sxi64 iTs = 0;` |
|    ! 0 | 3160 | `	sxi32 iOff = 0;` |
|    ! 0 | 3161 | `	int bOffSet = 0,uSec = 0;` |
|    ! 0 | 3162 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|    ! 0 | 3163 | `		return PH7_OK;` |
|      - | 3164 | `	}` |
|    ! 0 | 3165 | `	zMod = ph7_value_to_string(apArg[1],&nMod);` |
|    ! 0 | 3166 | `	iErrPos = DtParse(zMod,nMod,PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),` |
|      - | 3167 | `		&iTs,&iOff,&bOffSet,&uSec);` |
|    ! 0 | 3168 | `	if( iErrPos != 0 ){` |
|    ! 0 | 3169 | `		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);` |
|    ! 0 | 3170 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 3171 | `			"date_modify(): Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    ! 0 | 3172 | `			nMod,zMod,iPos,cAt,zErr);` |
|    ! 0 | 3173 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3174 | `		return PH7_OK;` |
|      - | 3175 | `	}` |
|    ! 0 | 3176 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,iTs);` |
|    ! 0 | 3177 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 3178 | `	return PH7_OK;` |
|    ! 0 | 3179 | `}` |
|     10 | 3180 | `static int DtProcAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)` |
|      1 | 3181 | `{` |
|     11 | 3182 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|     11 | 3183 | `	ph7_class_instance *pIv = DtArgObj(nArg,apArg,1);` |
|     11 | 3184 | `	if( pObj == 0 \|\| pIv == 0 ){` |
|    ! 0 | 3185 | `		return PH7_OK;` |
|      - | 3186 | `	}` |
|     11 | 3187 | `	if( PH7_NativeAttrInt(pIv,"invert") ){` |
|    ! 0 | 3188 | `		iSign = -iSign;` |
|    ! 0 | 3189 | `	}` |
|     11 | 3190 | `	DtApplyInterval(pCtx->pVm,pObj,pObj,pIv,iSign);` |
|     11 | 3191 | `	DtResultArg(pCtx,apArg);` |
|     11 | 3192 | `	return PH7_OK;` |
|      6 | 3193 | `}` |
|      6 | 3194 | `static int vm_builtin_date_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3195 | `{` |
|      7 | 3196 | `	return DtProcAddSub(pCtx,nArg,apArg,1);` |
|      1 | 3197 | `}` |
|      4 | 3198 | `static int vm_builtin_date_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3199 | `{` |
|      5 | 3200 | `	return DtProcAddSub(pCtx,nArg,apArg,-1);` |
|      1 | 3201 | `}` |
|      6 | 3202 | `static int vm_builtin_date_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3203 | `{` |
|      7 | 3204 | `	ph7_class_instance *pBase = DtArgObj(nArg,apArg,0);` |
|      7 | 3205 | `	ph7_class_instance *pTarget = DtArgObj(nArg,apArg,1);` |
|      7 | 3206 | `	int bAbsolute = 0;` |
|      7 | 3207 | `	if( pBase == 0 \|\| pTarget == 0 ){` |
|    ! 0 | 3208 | `		return PH7_OK;` |
|      - | 3209 | `	}` |
|      7 | 3210 | `	if( nArg > 2 ){` |
|    ! 0 | 3211 | `		bAbsolute = DtValueTruth(pCtx->pVm,apArg[2]);` |
|    ! 0 | 3212 | `	}` |
|      7 | 3213 | `	return DtDiffResult(pCtx,pBase,pTarget,bAbsolute);` |
|      4 | 3214 | `}` |
|      2 | 3215 | `static int vm_builtin_date_timestamp_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3216 | `{` |
|      3 | 3217 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      3 | 3218 | `	if( pObj ){` |
|      3 | 3219 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_TS));` |
|      1 | 3220 | `	}` |
|      3 | 3221 | `	return PH7_OK;` |
|      1 | 3222 | `}` |
|    ! 0 | 3223 | `static int vm_builtin_date_timestamp_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3224 | `{` |
|    ! 0 | 3225 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|    ! 0 | 3226 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|    ! 0 | 3227 | `		return PH7_OK;` |
|      - | 3228 | `	}` |
|    ! 0 | 3229 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,ph7_value_to_int64(apArg[1]));` |
|    ! 0 | 3230 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,0);` |
|    ! 0 | 3231 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 3232 | `	return PH7_OK;` |
|    ! 0 | 3233 | `}` |
|      2 | 3234 | `static int vm_builtin_date_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3235 | `{` |
|      3 | 3236 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      3 | 3237 | `	if( pObj == 0 ){` |
|    ! 0 | 3238 | `		return PH7_OK;` |
|      - | 3239 | `	}` |
|      3 | 3240 | `	return DtTimezoneResult(pCtx,pObj);` |
|      2 | 3241 | `}` |
|    ! 0 | 3242 | `static int vm_builtin_date_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3243 | `{` |
|    ! 0 | 3244 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|    ! 0 | 3245 | `	const char *zName = "UTC";` |
|    ! 0 | 3246 | `	int nName = 3;` |
|    ! 0 | 3247 | `	sxi32 iOff = 0;` |
|    ! 0 | 3248 | `	if( pObj == 0 \|\| nArg < 2 \|\| !DtZoneOf(apArg[1],&iOff,&zName,&nName) ){` |
|    ! 0 | 3249 | `		return PH7_OK;` |
|      - | 3250 | `	}` |
|    ! 0 | 3251 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_OFF,iOff);` |
|    ! 0 | 3252 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,DT_NAME,zName,nName);` |
|    ! 0 | 3253 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 3254 | `	return PH7_OK;` |
|    ! 0 | 3255 | `}` |
|      2 | 3256 | `static int vm_builtin_date_offset_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3257 | `{` |
|      3 | 3258 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      3 | 3259 | `	if( pObj ){` |
|      3 | 3260 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_OFF));` |
|      1 | 3261 | `	}` |
|      3 | 3262 | `	return PH7_OK;` |
|      1 | 3263 | `}` |
|    ! 0 | 3264 | `static int vm_builtin_date_date_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3265 | `{` |
|    ! 0 | 3266 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|    ! 0 | 3267 | `	if( pObj == 0 \|\| nArg < 4 ){` |
|    ! 0 | 3268 | `		return PH7_OK;` |
|      - | 3269 | `	}` |
|    ! 0 | 3270 | `	DtSetDateOf(pCtx,pObj,ph7_value_to_int64(apArg[1]),ph7_value_to_int(apArg[2]),` |
|    ! 0 | 3271 | `		ph7_value_to_int(apArg[3]));` |
|    ! 0 | 3272 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 3273 | `	return PH7_OK;` |
|    ! 0 | 3274 | `}` |
|    ! 0 | 3275 | `static int vm_builtin_date_time_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3276 | `{` |
|    ! 0 | 3277 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|    ! 0 | 3278 | `	if( pObj == 0 \|\| nArg < 3 ){` |
|    ! 0 | 3279 | `		return PH7_OK;` |
|      - | 3280 | `	}` |
|    ! 0 | 3281 | `	DtSetTimeOf(pCtx,pObj,ph7_value_to_int(apArg[1]),ph7_value_to_int(apArg[2]),` |
|    ! 0 | 3282 | `		nArg > 3 ? ph7_value_to_int(apArg[3]) : 0,` |
|    ! 0 | 3283 | `		nArg > 4 ? ph7_value_to_int64(apArg[4]) : 0);` |
|    ! 0 | 3284 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 3285 | `	return PH7_OK;` |
|    ! 0 | 3286 | `}` |
|      2 | 3287 | `static int vm_builtin_date_isodate_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3288 | `{` |
|      3 | 3289 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      3 | 3290 | `	if( pObj == 0 \|\| nArg < 3 ){` |
|    ! 0 | 3291 | `		return PH7_OK;` |
|      - | 3292 | `	}` |
|      5 | 3293 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,` |
|      3 | 3294 | `		DtIsoDate(PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),` |
|      2 | 3295 | `			ph7_value_to_int64(apArg[1]),ph7_value_to_int64(apArg[2]),` |
|      2 | 3296 | `			nArg > 3 ? ph7_value_to_int64(apArg[3]) : 1));` |
|      3 | 3297 | `	DtResultArg(pCtx,apArg);` |
|      3 | 3298 | `	return PH7_OK;` |
|      2 | 3299 | `}` |
|      - | 3300 | `/* date_interval_create_from_date_string(): warns and answers false where the` |
|      - | 3301 | ` * method throws. */` |
|      6 | 3302 | `static int vm_builtin_date_interval_create_from_date_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3303 | `{` |
|      7 | 3304 | `	const char *zIn,*zReason = "";` |
|      7 | 3305 | `	int nIn,iPos = 0;` |
|      7 | 3306 | `	char cAt = ' ';` |
|      - | 3307 | `	ph7_class_instance *pObj;` |
|      7 | 3308 | `	if( nArg < 1 ){` |
|    ! 0 | 3309 | `		return PH7_OK;` |
|      - | 3310 | `	}` |
|      7 | 3311 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|      7 | 3312 | `	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason);` |
|      7 | 3313 | `	if( pObj == 0 ){` |
|    ! 0 | 3314 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 3315 | `			"date_interval_create_from_date_string(): Unknown or bad format (%.*s) "` |
|    ! 0 | 3316 | `			"at position %d (%c): %s",nIn,zIn,iPos,cAt,zReason);` |
|    ! 0 | 3317 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3318 | `		return PH7_OK;` |
|      - | 3319 | `	}` |
|      7 | 3320 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      7 | 3321 | `	return PH7_OK;` |
|      4 | 3322 | `}` |
|      4 | 3323 | `static int vm_builtin_date_interval_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3324 | `{` |
|      5 | 3325 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      - | 3326 | `	const char *zFmt;` |
|      - | 3327 | `	int nFmt;` |
|      5 | 3328 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|    ! 0 | 3329 | `		return PH7_OK;` |
|      - | 3330 | `	}` |
|      5 | 3331 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|      5 | 3332 | `	DtIvFormat(pCtx,pObj,zFmt,nFmt);` |
|      5 | 3333 | `	return PH7_OK;` |
|      3 | 3334 | `}` |
|    ! 0 | 3335 | `static int vm_builtin_date_get_last_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3336 | `{` |
|    ! 0 | 3337 | `	return vm_builtin_DateTime_getLastErrors(pCtx,nArg,apArg);` |
|    ! 0 | 3338 | `}` |
|      - | 3339 | `/* timezone_open(): warns and answers false where the constructor throws. */` |
|      4 | 3340 | `static int vm_builtin_timezone_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3341 | `{` |
|      5 | 3342 | `	ph7_vm *pVm = pCtx->pVm;` |
|      5 | 3343 | `	ph7_class *pClass = DtClass(pVm,"DateTimeZone");` |
|      - | 3344 | `	ph7_class_instance *pObj;` |
|      - | 3345 | `	const char *zTz,*zName;` |
|      - | 3346 | `	int nTz,nName;` |
|      5 | 3347 | `	sxi32 iOff = 0;` |
|      - | 3348 | `	char zBuf[16];` |
|      5 | 3349 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3350 | `		return PH7_OK;` |
|      - | 3351 | `	}` |
|      5 | 3352 | `	zTz = ph7_value_to_string(apArg[0],&nTz);` |
|      5 | 3353 | `	if( DtZoneParse(zTz,nTz,&iOff,&zName,&nName,zBuf,sizeof(zBuf)) != 0 ){` |
|    ! 0 | 3354 | `		PH7_VmThrowWarningFmt(pVm,"timezone_open(): Unknown or bad timezone (%.*s)",nTz,zTz);` |
|    ! 0 | 3355 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3356 | `		return PH7_OK;` |
|      - | 3357 | `	}` |
|      5 | 3358 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|      5 | 3359 | `	if( pObj == 0 ){` |
|    ! 0 | 3360 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3361 | `	}` |
|      5 | 3362 | `	PH7_NativeSetAttrInt(pVm,pObj,DTZ_OFF,iOff);` |
|      5 | 3363 | `	PH7_NativeSetAttrStr(pVm,pObj,DTZ_NAME,zName,nName);` |
|      5 | 3364 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      5 | 3365 | `	return PH7_OK;` |
|      3 | 3366 | `}` |
|      4 | 3367 | `static int vm_builtin_timezone_name_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3368 | `{` |
|      5 | 3369 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      - | 3370 | `	const char *zName;` |
|      - | 3371 | `	int nName;` |
|      5 | 3372 | `	if( pObj == 0 ){` |
|    ! 0 | 3373 | `		return PH7_OK;` |
|      - | 3374 | `	}` |
|      5 | 3375 | `	PH7_NativeAttrStr(pObj,DTZ_NAME,&zName,&nName);` |
|      5 | 3376 | `	ph7_result_string(pCtx,zName,nName);` |
|      5 | 3377 | `	return PH7_OK;` |
|      3 | 3378 | `}` |
|      2 | 3379 | `static int vm_builtin_timezone_offset_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3380 | `{` |
|      3 | 3381 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      3 | 3382 | `	if( pObj ){` |
|      3 | 3383 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DTZ_OFF));` |
|      1 | 3384 | `	}` |
|      3 | 3385 | `	return PH7_OK;` |
|      1 | 3386 | `}` |
|      - | 3387 | `/*` |
|      - | 3388 | ` * The four private slots a date object keeps its state in. Both classes declare` |
|      - | 3389 | `` * them: `trait __DtCoreT` had no native equivalent, and replaying the table is`` |
|      - | 3390 | `` * exactly what `use __DtCoreT` did.`` |
|      - | 3391 | ` */` |
|      - | 3392 | `#define DT_NATIVE_STATE_PROPS \` |
|      - | 3393 | `	{ DT_TS,   PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \` |
|      - | 3394 | `	{ DT_OFF,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \` |
|      - | 3395 | `	{ DT_NAME, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 }, 0 }, \` |
|      - | 3396 | `	{ DT_US,   PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }` |
|      - | 3397 | `/*` |
|      - | 3398 | ` * The methods DateTime and DateTimeImmutable share -- the whole of the old trait` |
|      - | 3399 | ` * plus the mutators, whose one difference (write $this, or write a clone) the` |
|      - | 3400 | ` * bodies decide from the receiver's class. php's own signatures: they are the` |
|      - | 3401 | ` * single source of truth for arity, coercion and Reflection here, so the casts the` |
|      - | 3402 | `` * chunk wrote by hand (`(string)$format`, `(int)$timestamp`) are declared types now`` |
|      - | 3403 | ` * and the methods reject what php rejects.` |
|      - | 3404 | ` */` |
|      - | 3405 | `/* The methods DateTime and DateTimeImmutable share. CLS is the OWNING class name` |
|      - | 3406 | ` * as a string literal, because php's stubs write the concrete class rather than` |
|      - | 3407 | `` * `static` for the legacy mutators -- DateTime::add reports DateTime and`` |
|      - | 3408 | ` * DateTimeImmutable::add reports DateTimeImmutable. The two that php really does` |
|      - | 3409 | `` * declare `static` (setMicrosecond) and the two it declares for real rather than`` |
|      - | 3410 | ` * tentatively (getMicrosecond, and setMicrosecond again) are written as they are:` |
|      - | 3411 | `` * a leading `@` is php's @tentative-return-type, and nearly every method here has`` |
|      - | 3412 | ` * one. */` |
|      - | 3413 | `#define DT_NATIVE_SHARED_METHODS(CLS) \` |
|      - | 3414 | `	{ "__construct",     PH7_MOD_PUBLIC, "string $datetime = 'now', ?DateTimeZone $timezone = null", "", \` |
|      - | 3415 | `	  vm_builtin_DateTime_construct }, \` |
|      - | 3416 | `	{ "format",          PH7_MOD_PUBLIC, "string $format", "@string", vm_builtin_DateTime_format }, \` |
|      - | 3417 | `	{ "getTimestamp",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_DateTime_getTimestamp }, \` |
|      - | 3418 | `	{ "getMicrosecond",  PH7_MOD_PUBLIC, "", "int", vm_builtin_DateTime_getMicrosecond }, \` |
|      - | 3419 | `	{ "getOffset",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_DateTime_getOffset }, \` |
|      - | 3420 | `	{ "getTimezone",     PH7_MOD_PUBLIC, "", "@DateTimeZone\|false", vm_builtin_DateTime_getTimezone }, \` |
|      - | 3421 | `	{ "diff",            PH7_MOD_PUBLIC, "DateTimeInterface $targetObject, bool $absolute = false", \` |
|      - | 3422 | `	  "@DateInterval", vm_builtin_DateTime_diff }, \` |
|      - | 3423 | `	{ "modify",          PH7_MOD_PUBLIC, "string $modifier", "@" CLS, vm_builtin_DateTime_modify }, \` |
|      - | 3424 | `	{ "setTimestamp",    PH7_MOD_PUBLIC, "int $timestamp", "@" CLS, vm_builtin_DateTime_setTimestamp }, \` |
|      - | 3425 | `	{ "setMicrosecond",  PH7_MOD_PUBLIC, "int $microsecond", "static", vm_builtin_DateTime_setMicrosecond }, \` |
|      - | 3426 | `	{ "setTimezone",     PH7_MOD_PUBLIC, "DateTimeZone $timezone", "@" CLS, vm_builtin_DateTime_setTimezone }, \` |
|      - | 3427 | `	{ "setDate",         PH7_MOD_PUBLIC, "int $year, int $month, int $day", "@" CLS, \` |
|      - | 3428 | `	  vm_builtin_DateTime_setDate }, \` |
|      - | 3429 | `	{ "setTime",         PH7_MOD_PUBLIC, \` |
|      - | 3430 | `	  "int $hour, int $minute, int $second = 0, int $microsecond = 0", "@" CLS, \` |
|      - | 3431 | `	  vm_builtin_DateTime_setTime }, \` |
|      - | 3432 | `	{ "setISODate",      PH7_MOD_PUBLIC, "int $year, int $week, int $dayOfWeek = 1", "@" CLS, \` |
|      - | 3433 | `	  vm_builtin_DateTime_setISODate }, \` |
|      - | 3434 | `	{ "add",             PH7_MOD_PUBLIC, "DateInterval $interval", "@" CLS, vm_builtin_DateTime_add }, \` |
|      - | 3435 | `	{ "sub",             PH7_MOD_PUBLIC, "DateInterval $interval", "@" CLS, vm_builtin_DateTime_sub }, \` |
|      - | 3436 | `	{ "getLastErrors",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "@array\|false", \` |
|      - | 3437 | `	  vm_builtin_DateTime_getLastErrors }` |
|      - | 3438 | `/*` |
|      - | 3439 | ` * php's presentation for the date classes (ph7_class::xPresent).` |
|      - | 3440 | ` *` |
|      - | 3441 | ` * php keeps a timelib struct and SHOWS date/timezone_type/timezone; PHL keeps a` |
|      - | 3442 | ` * timestamp, an offset, a zone name and microseconds, all hidden. These build php's` |
|      - | 3443 | ` * shape out of that state, so var_dump/print_r, var_export and the (array) cast` |
|      - | 3444 | ` * agree with the oracle without changing what the C bodies read.` |
|      - | 3445 | ` *` |
|      - | 3446 | ` * timezone_type is php's own three-way tag: 1 = a fixed UTC OFFSET ("+02:00"),` |
|      - | 3447 | ` * 2 = an ABBREVIATION ("GMT", "Z"), 3 = an IDENTIFIER ("UTC", "Europe/Paris").` |
|      - | 3448 | ` * PHL accepts offsets, UTC, GMT and Z today; the identifier arm is written for the` |
|      - | 3449 | ` * whole rule so a tz database can only add names, never change the tagging.` |
|      - | 3450 | ` */` |
|     92 | 3451 | `static int DtZoneTypeOf(const char *zName,int nName)` |
|      1 | 3452 | `{` |
|     93 | 3453 | `	sxu32 nPos = 0;` |
|     93 | 3454 | `	if( nName > 0 && (zName[0] == '+' \|\| zName[0] == '-') ){` |
|     23 | 3455 | `		return 1;` |
|      - | 3456 | `	}` |
|     71 | 3457 | `	if( nName == 3 && SyStrnicmp(zName,"UTC",3) == 0 ){` |
|     63 | 3458 | `		return 3;` |
|      - | 3459 | `	}` |
|      9 | 3460 | `	if( nName > 0 && SyByteFind(zName,(sxu32)nName,'/',&nPos) == SXRET_OK ){` |
|    ! 0 | 3461 | `		return 3;` |
|      - | 3462 | `	}` |
|      9 | 3463 | `	return 2;` |
|     47 | 3464 | `}` |
|    234 | 3465 | `static void DtPresentPut(ph7_vm *pVm,ph7_value *pOut,const char *zKey,ph7_value *pVal)` |
|      1 | 3466 | `{` |
|      - | 3467 | `	ph7_value sKey;` |
|    235 | 3468 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    235 | 3469 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|    235 | 3470 | `	ph7_array_add_elem(pOut,&sKey,pVal);` |
|    235 | 3471 | `	PH7_MemObjRelease(&sKey);` |
|    235 | 3472 | `}` |
|     92 | 3473 | `static void DtPresentZone(ph7_vm *pVm,ph7_value *pOut,const char *zName,int nName)` |
|      1 | 3474 | `{` |
|      - | 3475 | `	ph7_value sVal;` |
|     93 | 3476 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,DtZoneTypeOf(zName,nName));` |
|     93 | 3477 | `	DtPresentPut(&(*pVm),pOut,"timezone_type",&sVal);` |
|     93 | 3478 | `	PH7_MemObjRelease(&sVal);` |
|     93 | 3479 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,0);` |
|     93 | 3480 | `	PH7_MemObjStringAppend(&sVal,zName,(sxu32)nName);` |
|     93 | 3481 | `	DtPresentPut(&(*pVm),pOut,"timezone",&sVal);` |
|     93 | 3482 | `	PH7_MemObjRelease(&sVal);` |
|     93 | 3483 | `}` |
|     50 | 3484 | `static sxi32 DtPresentDateTime(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      1 | 3485 | `{` |
|      - | 3486 | `	dt_state sState;` |
|      - | 3487 | `	Sytm sTm;` |
|      - | 3488 | `	char zZone[64];` |
|      - | 3489 | `	char zDate[64];` |
|      - | 3490 | `	ph7_value sVal;` |
|      - | 3491 | `	int nName;` |
|     25 | 3492 | `	SXUNUSED(bDebug); /* php shows the same three keys to both handlers */` |
|     51 | 3493 | `	DtLoad(pThis,&sState);` |
|     51 | 3494 | `	nName = sState.nName;` |
|     51 | 3495 | `	if( nName >= (int)sizeof(zZone) ){` |
|    ! 0 | 3496 | `		nName = (int)sizeof(zZone) - 1;` |
|    ! 0 | 3497 | `	}` |
|     51 | 3498 | `	if( nName > 0 ){` |
|     51 | 3499 | `		SyMemcpy(sState.zName,zZone,(sxu32)nName);` |
|     25 | 3500 | `	}` |
|     51 | 3501 | `	zZone[nName] = 0;` |
|     51 | 3502 | `	DtFillSytm(sState.iTs,sState.iOff,zZone,&sTm);` |
|      - | 3503 | `	/* php's fixed shape here, not a format string: "Y-m-d H:i:s.uuuuuu". */` |
|     76 | 3504 | `	SyBufferFormat(zDate,sizeof(zDate),"%04d-%02d-%02d %02d:%02d:%02d.%06d",` |
|     50 | 3505 | `		sTm.tm_year,sTm.tm_mon + 1,sTm.tm_mday,sTm.tm_hour,sTm.tm_min,sTm.tm_sec,` |
|     25 | 3506 | `		sState.uSec);` |
|     51 | 3507 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,0);` |
|     51 | 3508 | `	PH7_MemObjStringAppend(&sVal,zDate,(sxu32)SyStrlen(zDate));` |
|     51 | 3509 | `	DtPresentPut(&(*pVm),pOut,"date",&sVal);` |
|     51 | 3510 | `	PH7_MemObjRelease(&sVal);` |
|     51 | 3511 | `	DtPresentZone(&(*pVm),pOut,zZone,nName);` |
|     51 | 3512 | `	return SXRET_OK;` |
|      1 | 3513 | `}` |
|     42 | 3514 | `static sxi32 DtPresentTimeZone(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      1 | 3515 | `{` |
|     43 | 3516 | `	const char *zName = 0;` |
|     43 | 3517 | `	int nName = 0;` |
|     21 | 3518 | `	SXUNUSED(bDebug);` |
|     43 | 3519 | `	PH7_NativeAttrStr(pThis,DTZ_NAME,&zName,&nName);` |
|     43 | 3520 | `	DtPresentZone(&(*pVm),pOut,zName ? zName : "",nName);` |
|     43 | 3521 | `	return SXRET_OK;` |
|      1 | 3522 | `}` |
|      - | 3523 | `/*` |
|      - | 3524 | ` * ---------------------------------------------------------------------------` |
|      - | 3525 | ` * php's serialization pair for the three date classes whose state is HIDDEN.` |
|      - | 3526 | ` *` |
|      - | 3527 | ` * serialize() had been walking the engine slots, so a DateTime round-tripped as` |
|      - | 3528 | `` * `__dtTs`/`__dtOff`/`__dtName`/`__dtUs` and a payload php WROTE could not be read`` |
|      - | 3529 | `` * back at all -- `unserialize('O:8:"DateTime":3:{s:4:"date";…}')` found none of the`` |
|      - | 3530 | ` * names it wanted, silently kept the 1970 defaults and answered a valid object with` |
|      - | 3531 | ` * the wrong instant. php's answer is not a hidden-slot rule but a pair of methods:` |
|      - | 3532 | ` * __serialize() hands back the PRESENTED shape (date/timezone_type/timezone, the` |
|      - | 3533 | ` * same hash date_object_get_properties_for builds) and __unserialize() re-parses it,` |
|      - | 3534 | ` * so the payload is the class's public model rather than its storage.` |
|      - | 3535 | ` *` |
|      - | 3536 | ` * The four methods php declares are all here, because they are one contract:` |
|      - | 3537 | ` * __serialize/__unserialize is what serialize() uses, __wakeup reads a LEGACY` |
|      - | 3538 | ` * payload out of the object's own properties, and __set_state is what var_export's` |
|      - | 3539 | `` * `\DateTime::__set_state(array(…))` text evaluates to. All four fail with the same`` |
|      - | 3540 | `` * plain `Error`, and php's sentence for it names the class.`` |
|      - | 3541 | ` * ---------------------------------------------------------------------------` |
|      - | 3542 | ` */` |
|      - | 3543 | `/*` |
|      - | 3544 | ` * php's add_common_properties(): after the presented shape, the instance's own` |
|      - | 3545 | ` * php-visible slots -- a SUBCLASS's declared properties, which php serializes` |
|      - | 3546 | ` * alongside the internal state. A key the presented shape already wrote WINS` |
|      - | 3547 | ` * (zend_hash_add, not update), and a hidden engine slot is never a candidate:` |
|      - | 3548 | ` * this is the one walk in the date family that must skip them.` |
|      - | 3549 | ` */` |
|     28 | 3550 | `static void DtAddCommonProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|      1 | 3551 | `{` |
|      - | 3552 | `	SyHashEntry *pEntry;` |
|     29 | 3553 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    125 | 3554 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     97 | 3555 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     97 | 3556 | `		SyString *pName = &pVmAttr->pAttr->sName;` |
|      - | 3557 | `		ph7_value *pVal;` |
|      - | 3558 | `		ph7_value sKey;` |
|     97 | 3559 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|      - | 3560 | `			\|PH7_CLASS_ATTR_HIDDEN\|PH7_CLASS_ATTR_HOOK_VIRTUAL) ){` |
|     93 | 3561 | `			continue;` |
|      - | 3562 | `		}` |
|      5 | 3563 | `		if( ph7_array_fetch(pOut,pName->zString,(int)pName->nByte) != 0 ){` |
|    ! 0 | 3564 | `			continue;` |
|      - | 3565 | `		}` |
|      5 | 3566 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|      5 | 3567 | `		if( pVal == 0 ){` |
|    ! 0 | 3568 | `			continue;` |
|      - | 3569 | `		}` |
|      5 | 3570 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      5 | 3571 | `		PH7_MemObjStringAppend(&sKey,pName->zString,pName->nByte);` |
|      5 | 3572 | `		ph7_array_add_elem(pOut,&sKey,pVal);` |
|      5 | 3573 | `		PH7_MemObjRelease(&sKey);` |
|      1 | 3574 | `	}` |
|     29 | 3575 | `}` |
|      - | 3576 | `/* Build a payload array: the class's presented shape, then its own visible slots. */` |
|     26 | 3577 | `static int DtSerializePayload(ph7_context *pCtx,ph7_class_instance *pThis,int bZoneOnly,` |
|      - | 3578 | `	ph7_value *pOut)` |
|      1 | 3579 | `{` |
|     27 | 3580 | `	PH7_MemObjInit(pCtx->pVm,pOut);` |
|     27 | 3581 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|    ! 0 | 3582 | `		PH7_MemObjRelease(pOut);` |
|    ! 0 | 3583 | `		return -1;` |
|      - | 3584 | `	}` |
|     27 | 3585 | `	if( bZoneOnly ){` |
|     11 | 3586 | `		DtPresentTimeZone(pCtx->pVm,pThis,pOut,0);` |
|      6 | 3587 | `	}else{` |
|     17 | 3588 | `		DtPresentDateTime(pCtx->pVm,pThis,pOut,0);` |
|      - | 3589 | `	}` |
|     27 | 3590 | `	DtAddCommonProps(pCtx->pVm,pThis,pOut);` |
|     27 | 3591 | `	return 0;` |
|     14 | 3592 | `}` |
|      - | 3593 | ``/* php's `Error: Invalid serialization data for <Class> object`, the one refusal all`` |
|      - | 3594 | ` * four methods share. Named for the DECLARING class, not the receiver's. */` |
|     12 | 3595 | `static int DtSerialError(ph7_context *pCtx,const char *zClass)` |
|      1 | 3596 | `{` |
|     19 | 3597 | `	return PH7_VmThrowException(pCtx,"Error",` |
|      6 | 3598 | `		"Invalid serialization data for %s object",zClass);` |
|      1 | 3599 | `}` |
|      - | 3600 | ``/* The `array $data` parameter's own screen: the shared ZPP does not judge a scalar`` |
|      - | 3601 | `` * against a bare `array` (rule 18's §2 gap), so each caller words php's TypeError. */`` |
|     28 | 3602 | `static int DtCheckDataArg(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zClass)` |
|      1 | 3603 | `{` |
|      - | 3604 | `	char zBuf[64];` |
|     29 | 3605 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     29 | 3606 | `		return 0;` |
|      - | 3607 | `	}` |
|    ! 0 | 3608 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3609 | `		"%s::__unserialize(): Argument #1 ($data) must be of type array, %s given",` |
|    ! 0 | 3610 | `		zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|    ! 0 | 3611 | `	return -1;` |
|     15 | 3612 | `}` |
|      - | 3613 | `/*` |
|      - | 3614 | `` * php's php_date_timezone_initialize_from_hash(): `timezone_type` must be an int in`` |
|      - | 3615 | `` * 1..3 and `timezone` a string, and then the NAME alone rebuilds the zone -- the tag`` |
|      - | 3616 | ` * is validated but never trusted, which is why a payload tagged 1 whose name is` |
|      - | 3617 | ` * "UTC" restores a UTC zone rather than an offset one. Answers 0 on success.` |
|      - | 3618 | ` */` |
|     10 | 3619 | `static int DtZoneRestore(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 3620 | `{` |
|      - | 3621 | `	ph7_value *pType,*pName;` |
|      - | 3622 | `	const char *zTz,*zName;` |
|      - | 3623 | `	int nTz,nName;` |
|     11 | 3624 | `	sxi32 iOff = 0;` |
|      - | 3625 | `	sxi64 iType;` |
|      - | 3626 | `	char zBuf[16];` |
|     11 | 3627 | `	pType = ph7_array_fetch(pData,"timezone_type",(int)sizeof("timezone_type")-1);` |
|     11 | 3628 | `	if( pType == 0 \|\| (pType->iFlags & MEMOBJ_INT) == 0 ){` |
|      3 | 3629 | `		return -1;` |
|      - | 3630 | `	}` |
|      9 | 3631 | `	iType = pType->x.iVal;` |
|      9 | 3632 | `	if( iType < 1 \|\| iType > 3 ){` |
|      3 | 3633 | `		return -1;` |
|      - | 3634 | `	}` |
|      7 | 3635 | `	pName = ph7_array_fetch(pData,"timezone",(int)sizeof("timezone")-1);` |
|      7 | 3636 | `	if( pName == 0 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 3637 | `		return -1;` |
|      - | 3638 | `	}` |
|      7 | 3639 | `	zTz = (const char *)SyBlobData(&pName->sBlob);` |
|      7 | 3640 | `	nTz = (int)SyBlobLength(&pName->sBlob);` |
|      7 | 3641 | `	if( DtZoneParse(zTz,nTz,&iOff,&zName,&nName,zBuf,sizeof(zBuf)) != 0 ){` |
|    ! 0 | 3642 | `		return -1;` |
|      - | 3643 | `	}` |
|      7 | 3644 | `	PH7_NativeSetAttrInt(&(*pVm),pThis,DTZ_OFF,iOff);` |
|      7 | 3645 | `	PH7_NativeSetAttrStr(&(*pVm),pThis,DTZ_NAME,zName,nName);` |
|      7 | 3646 | `	return 0;` |
|      6 | 3647 | `}` |
|      - | 3648 | `/*` |
|      - | 3649 | `` * php's php_date_initialize_from_hash(): `date`, `timezone_type` and `timezone` must`` |
|      - | 3650 | ` * all be present and well-typed, and the tag must be one php writes.` |
|      - | 3651 | ` *` |
|      - | 3652 | ` * php restores an OFFSET or ABBREVIATION payload by CONCATENATING the two and running` |
|      - | 3653 | ` * its ordinary parser over "<date> <timezone>", and an IDENTIFIER one by resolving the` |
|      - | 3654 | ` * name first. Resolving the name for all three is the same answer here and does not` |
|      - | 3655 | `` * lean on the parser: `date` is always php's own `x-m-d H:i:s.u`, which carries no`` |
|      - | 3656 | ` * zone of its own, so nothing is left for the concatenated text to decide. It is also` |
|      - | 3657 | ` * the only spelling that works today -- PHL's parser accepts an offset only when it is` |
|      - | 3658 | ` * ATTACHED to the time ("…07+02:30", never "…07 +02:30") and accepts no trailing zone` |
|      - | 3659 | ` * NAME at all, so php's own round-trip string does not parse here (a §10 gap of its` |
|      - | 3660 | ` * own, recorded rather than worked around).` |
|      - | 3661 | ` *` |
|      - | 3662 | ` * Reading the NAME rather than the tag is also what php ends up doing: a payload` |
|      - | 3663 | ` * tagged 1 whose timezone is "UTC" restores a UTC zone in both engines.` |
|      - | 3664 | ` * Answers 0 on success.` |
|      - | 3665 | ` */` |
|     28 | 3666 | `static int DtDateRestore(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 3667 | `{` |
|     29 | 3668 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 3669 | `	ph7_value *pDate,*pType,*pName;` |
|      - | 3670 | `	const char *zDate,*zTz,*zZone,*zErr;` |
|      - | 3671 | `	int nDate,nTz,nZone,iPos;` |
|     29 | 3672 | `	sxi32 iOff = 0;` |
|      - | 3673 | `	sxi64 iType;` |
|      - | 3674 | `	dt_state sState;` |
|      - | 3675 | `	char zNameBuf[16],zZoneBuf[16],cAt;` |
|     29 | 3676 | `	pDate = ph7_array_fetch(pData,"date",(int)sizeof("date")-1);` |
|     29 | 3677 | `	if( pDate == 0 \|\| (pDate->iFlags & MEMOBJ_STRING) == 0 ){` |
|      7 | 3678 | `		return -1;` |
|      - | 3679 | `	}` |
|     23 | 3680 | `	pType = ph7_array_fetch(pData,"timezone_type",(int)sizeof("timezone_type")-1);` |
|     23 | 3681 | `	if( pType == 0 \|\| (pType->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 3682 | `		return -1;` |
|      - | 3683 | `	}` |
|     23 | 3684 | `	pName = ph7_array_fetch(pData,"timezone",(int)sizeof("timezone")-1);` |
|     23 | 3685 | `	if( pName == 0 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 3686 | `		return -1;` |
|      - | 3687 | `	}` |
|     23 | 3688 | `	zDate = (const char *)SyBlobData(&pDate->sBlob);` |
|     23 | 3689 | `	nDate = (int)SyBlobLength(&pDate->sBlob);` |
|     23 | 3690 | `	zTz   = (const char *)SyBlobData(&pName->sBlob);` |
|     23 | 3691 | `	nTz   = (int)SyBlobLength(&pName->sBlob);` |
|     23 | 3692 | `	iType = pType->x.iVal;` |
|     23 | 3693 | `	if( iType < 1 \|\| iType > 3 ){` |
|    ! 0 | 3694 | `		return -1;` |
|      - | 3695 | `	}` |
|     23 | 3696 | `	if( DtZoneParse(zTz,nTz,&iOff,&zZone,&nZone,zZoneBuf,sizeof(zZoneBuf)) != 0 ){` |
|      3 | 3697 | `		return -1;` |
|      - | 3698 | `	}` |
|     20 | 3699 | `	if( DtInitState(pCtx,zDate,nDate,iOff,zZone,nZone,&sState,zNameBuf,sizeof(zNameBuf),` |
|     11 | 3700 | `		&zErr,&iPos,&cAt) != 0 ){` |
|    ! 0 | 3701 | `		return -1;` |
|      - | 3702 | `	}` |
|     21 | 3703 | `	DtStore(pVm,pThis,&sState);` |
|     21 | 3704 | `	return 0;` |
|     15 | 3705 | `}` |
|      - | 3706 | `/*` |
|      - | 3707 | ` * php's restore_custom_datetime_properties(): every payload key that is not part of` |
|      - | 3708 | ` * the internal shape becomes a property of the object. A REFERENCE is skipped, which` |
|      - | 3709 | ` * PHL cannot receive here (the pairs arrive already dereferenced).` |
|      - | 3710 | ` */` |
|      - | 3711 | `typedef struct dt_restore_ctx dt_restore_ctx;` |
|      - | 3712 | `struct dt_restore_ctx` |
|      - | 3713 | `{` |
|      - | 3714 | `	ph7_class_instance *pThis;` |
|      - | 3715 | `	int bZoneOnly;` |
|      - | 3716 | `};` |
|     52 | 3717 | `static int DtRestoreWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 | 3718 | `{` |
|     53 | 3719 | `	dt_restore_ctx *pRes = (dt_restore_ctx *)pUserData;` |
|      - | 3720 | `	const char *zKey;` |
|      - | 3721 | `	int nKey;` |
|     53 | 3722 | `	if( !ph7_value_is_string(pKey) ){` |
|    ! 0 | 3723 | `		return PH7_OK;` |
|      - | 3724 | `	}` |
|     53 | 3725 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     52 | 3726 | `	if( (nKey == 13 && SyMemcmp(zKey,"timezone_type",13) == 0)` |
|     39 | 3727 | `	 \|\| (nKey == 8  && SyMemcmp(zKey,"timezone",8) == 0)` |
|     33 | 3728 | `	 \|\| (!pRes->bZoneOnly && nKey == 4 && SyMemcmp(zKey,"date",4) == 0) ){` |
|     73 | 3729 | `		return PH7_OK;` |
|      - | 3730 | `	}` |
|      - | 3731 | `	/* A name the class does not DECLARE is dropped, which is what the engine's own` |
|      - | 3732 | `	 * unserialize does with one: PHL has no dynamic properties, where php creates` |
|      - | 3733 | `	 * (and deprecates) them. */` |
|     23 | 3734 | `	PH7_NativeSetProp(pRes->pThis->pVm,pRes->pThis,zKey,(sxu32)nKey,pVal);` |
|     23 | 3735 | `	return PH7_OK;` |
|     38 | 3736 | `}` |
|     26 | 3737 | `static void DtRestoreCustomProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData,` |
|      - | 3738 | `	int bZoneOnly)` |
|      1 | 3739 | `{` |
|      - | 3740 | `	dt_restore_ctx sRes;` |
|     13 | 3741 | `	SXUNUSED(pVm);` |
|     27 | 3742 | `	if( (pData->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 3743 | `		return;` |
|      - | 3744 | `	}` |
|     27 | 3745 | `	sRes.pThis = pThis;` |
|     27 | 3746 | `	sRes.bZoneOnly = bZoneOnly;` |
|     27 | 3747 | `	ph7_array_walk(pData,DtRestoreWalk,&sRes);` |
|     14 | 3748 | `}` |
|      - | 3749 | `/* DateTimeZone::__serialize() / DateTime\|DateTimeImmutable::__serialize() */` |
|     26 | 3750 | `static int DtSerializeMagic(ph7_context *pCtx,int bZoneOnly)` |
|      1 | 3751 | `{` |
|     27 | 3752 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 3753 | `	ph7_value sOut;` |
|     27 | 3754 | `	if( pThis == 0 ){` |
|    ! 0 | 3755 | `		return PH7_OK;` |
|      - | 3756 | `	}` |
|     27 | 3757 | `	if( DtSerializePayload(pCtx,pThis,bZoneOnly,&sOut) != 0 ){` |
|    ! 0 | 3758 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3759 | `	}` |
|     27 | 3760 | `	ph7_result_value(pCtx,&sOut);` |
|     27 | 3761 | `	PH7_MemObjRelease(&sOut);` |
|     27 | 3762 | `	return PH7_OK;` |
|     14 | 3763 | `}` |
|     10 | 3764 | `static int vm_builtin_DateTimeZone_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3765 | `{` |
|      5 | 3766 | `	SXUNUSED(nArg);` |
|      5 | 3767 | `	SXUNUSED(apArg);` |
|     11 | 3768 | `	return DtSerializeMagic(pCtx,1);` |
|      1 | 3769 | `}` |
|     16 | 3770 | `static int vm_builtin_DateTime_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3771 | `{` |
|      8 | 3772 | `	SXUNUSED(nArg);` |
|      8 | 3773 | `	SXUNUSED(apArg);` |
|     17 | 3774 | `	return DtSerializeMagic(pCtx,0);` |
|      1 | 3775 | `}` |
|      - | 3776 | `/* __unserialize(array $data): restore the state, then the subclass's own slots. */` |
|     28 | 3777 | `static int DtUnserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg,int bZoneOnly,` |
|      - | 3778 | `	const char *zClass)` |
|      1 | 3779 | `{` |
|     29 | 3780 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 3781 | `	int rc;` |
|     29 | 3782 | `	if( pThis == 0 ){` |
|    ! 0 | 3783 | `		return PH7_OK;` |
|      - | 3784 | `	}` |
|     29 | 3785 | `	if( DtCheckDataArg(pCtx,nArg,apArg,zClass) != 0 ){` |
|    ! 0 | 3786 | `		return PH7_EXCEPTION;` |
|      - | 3787 | `	}` |
|     19 | 3788 | `	rc = bZoneOnly ? DtZoneRestore(pCtx->pVm,pThis,apArg[0])` |
|     24 | 3789 | `	               : DtDateRestore(pCtx,pThis,apArg[0]);` |
|     29 | 3790 | `	if( rc != 0 ){` |
|      9 | 3791 | `		return DtSerialError(pCtx,zClass);` |
|      - | 3792 | `	}` |
|     21 | 3793 | `	DtRestoreCustomProps(pCtx->pVm,pThis,apArg[0],bZoneOnly);` |
|     21 | 3794 | `	return PH7_OK;` |
|     15 | 3795 | `}` |
|      8 | 3796 | `static int vm_builtin_DateTimeZone_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3797 | `{` |
|      9 | 3798 | `	return DtUnserializeMagic(pCtx,nArg,apArg,1,"DateTimeZone");` |
|      1 | 3799 | `}` |
|     20 | 3800 | `static int vm_builtin_DateTime_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3801 | `{` |
|     21 | 3802 | `	return DtUnserializeMagic(pCtx,nArg,apArg,0,"DateTime");` |
|      1 | 3803 | `}` |
|    ! 0 | 3804 | `static int vm_builtin_DateTimeImmutable_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3805 | `{` |
|    ! 0 | 3806 | `	return DtUnserializeMagic(pCtx,nArg,apArg,0,"DateTimeImmutable");` |
|    ! 0 | 3807 | `}` |
|      - | 3808 | `/*` |
|      - | 3809 | ` * __wakeup(): the LEGACY payload, whose pairs the engine wrote into the object's own` |
|      - | 3810 | ` * properties before calling this. php reads Z_OBJPROP and restores from it, so an` |
|      - | 3811 | `` * object that has no such properties -- a plain `new DateTime` -- is exactly the`` |
|      - | 3812 | ` * failure case, and php raises the same Error there.` |
|      - | 3813 | ` */` |
|      2 | 3814 | `static int DtWakeupMagic(ph7_context *pCtx,int bZoneOnly,const char *zClass)` |
|      1 | 3815 | `{` |
|      3 | 3816 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 3817 | `	ph7_value sProps;` |
|      - | 3818 | `	int rc;` |
|      3 | 3819 | `	if( pThis == 0 ){` |
|    ! 0 | 3820 | `		return PH7_OK;` |
|      - | 3821 | `	}` |
|      3 | 3822 | `	PH7_MemObjInit(pCtx->pVm,&sProps);` |
|      3 | 3823 | `	if( PH7_MemObjToHashmap(&sProps) != SXRET_OK ){` |
|    ! 0 | 3824 | `		PH7_MemObjRelease(&sProps);` |
|    ! 0 | 3825 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3826 | `	}` |
|      3 | 3827 | `	DtAddCommonProps(pCtx->pVm,pThis,&sProps);` |
|      2 | 3828 | `	rc = bZoneOnly ? DtZoneRestore(pCtx->pVm,pThis,&sProps)` |
|      2 | 3829 | `	               : DtDateRestore(pCtx,pThis,&sProps);` |
|      3 | 3830 | `	PH7_MemObjRelease(&sProps);` |
|      3 | 3831 | `	if( rc != 0 ){` |
|      3 | 3832 | `		return DtSerialError(pCtx,zClass);` |
|      - | 3833 | `	}` |
|    ! 0 | 3834 | `	return PH7_OK;` |
|      2 | 3835 | `}` |
|    ! 0 | 3836 | `static int vm_builtin_DateTimeZone_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3837 | `{` |
|    ! 0 | 3838 | `	SXUNUSED(nArg);` |
|    ! 0 | 3839 | `	SXUNUSED(apArg);` |
|    ! 0 | 3840 | `	return DtWakeupMagic(pCtx,1,"DateTimeZone");` |
|    ! 0 | 3841 | `}` |
|      2 | 3842 | `static int vm_builtin_DateTime_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3843 | `{` |
|      1 | 3844 | `	SXUNUSED(nArg);` |
|      1 | 3845 | `	SXUNUSED(apArg);` |
|      3 | 3846 | `	return DtWakeupMagic(pCtx,0,"DateTime");` |
|      1 | 3847 | `}` |
|    ! 0 | 3848 | `static int vm_builtin_DateTimeImmutable_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3849 | `{` |
|    ! 0 | 3850 | `	SXUNUSED(nArg);` |
|    ! 0 | 3851 | `	SXUNUSED(apArg);` |
|    ! 0 | 3852 | `	return DtWakeupMagic(pCtx,0,"DateTimeImmutable");` |
|    ! 0 | 3853 | `}` |
|      - | 3854 | `/*` |
|      - | 3855 | ``  * __set_state(array $array): what var_export's `\DateTime::__set_state(array(…))` `` |
|      - | 3856 | ` * text evaluates to. php instantiates the class the method is DECLARED on and not` |
|      - | 3857 | `` * the called one -- `MyDateTime::__set_state(…)` answers a plain DateTime there --`` |
|      - | 3858 | ` * so this deliberately does not go through DtFactoryClass().` |
|      - | 3859 | ` */` |
|      8 | 3860 | `static int DtSetStateMagic(ph7_context *pCtx,int nArg,ph7_value **apArg,int bZoneOnly,` |
|      - | 3861 | `	const char *zClass)` |
|      1 | 3862 | `{` |
|      9 | 3863 | `	ph7_vm *pVm = pCtx->pVm;` |
|      9 | 3864 | `	ph7_class *pClass = DtClass(pVm,zClass);` |
|      - | 3865 | `	ph7_class_instance *pObj;` |
|      - | 3866 | `	int rc;` |
|      9 | 3867 | `	if( pClass == 0 ){` |
|    ! 0 | 3868 | `		return PH7_OK;` |
|      - | 3869 | `	}` |
|      9 | 3870 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - | 3871 | `		char zBuf[64];` |
|    ! 0 | 3872 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3873 | `			"%s::__set_state(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 | 3874 | `			zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|      - | 3875 | `	}` |
|      9 | 3876 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|      9 | 3877 | `	if( pObj == 0 ){` |
|    ! 0 | 3878 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3879 | `	}` |
|      6 | 3880 | `	rc = bZoneOnly ? DtZoneRestore(pVm,pObj,apArg[0])` |
|      7 | 3881 | `	               : DtDateRestore(pCtx,pObj,apArg[0]);` |
|      9 | 3882 | `	if( rc != 0 ){` |
|      3 | 3883 | `		PH7_ClassInstanceUnref(pObj);` |
|      3 | 3884 | `		return DtSerialError(pCtx,zClass);` |
|      - | 3885 | `	}` |
|      7 | 3886 | `	DtRestoreCustomProps(pVm,pObj,apArg[0],bZoneOnly);` |
|      7 | 3887 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      7 | 3888 | `	return PH7_OK;` |
|      5 | 3889 | `}` |
|      2 | 3890 | `static int vm_builtin_DateTimeZone_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3891 | `{` |
|      3 | 3892 | `	return DtSetStateMagic(pCtx,nArg,apArg,1,"DateTimeZone");` |
|      1 | 3893 | `}` |
|      6 | 3894 | `static int vm_builtin_DateTime_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3895 | `{` |
|      7 | 3896 | `	return DtSetStateMagic(pCtx,nArg,apArg,0,"DateTime");` |
|      1 | 3897 | `}` |
|    ! 0 | 3898 | `static int vm_builtin_DateTimeImmutable_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3899 | `{` |
|    ! 0 | 3900 | `	return DtSetStateMagic(pCtx,nArg,apArg,0,"DateTimeImmutable");` |
|    ! 0 | 3901 | `}` |
|      - | 3902 | `/* The four rows both date classes take. __serialize/__unserialize are php's only` |
|      - | 3903 | ` * NON-tentative internal returns in this family; __wakeup and __set_state carry the` |
|      - | 3904 | `` * `@`, and __set_state's return names the CONCRETE class php's stub writes. */`` |
|      - | 3905 | `#define DT_NATIVE_SERIAL_METHODS(CLS) \` |
|      - | 3906 | `	{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_DateTime_serialize }, \` |
|      - | 3907 | `	{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void", \` |
|      - | 3908 | `	  vm_builtin_##CLS##_unserialize }, \` |
|      - | 3909 | `	{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_##CLS##_wakeup }, \` |
|      - | 3910 | `	{ "__set_state",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@" #CLS, \` |
|      - | 3911 | `	  vm_builtin_##CLS##_setState }` |
|      - | 3912 | `/* php's DateTimeInterface constants, the whole of that interface's surface here` |
|      - | 3913 | ` * (its abstract METHODS are deliberately not declared: PH7_ClassImplement installs` |
|      - | 3914 | ` * a stub for every interface method an implementor lacks, so declaring them would` |
|      - | 3915 | ` * make every implementor abstract before its native methods are attached). */` |
|      - | 3916 | `#define DT_IFACE_CONST(NAME,VALUE) \` |
|      - | 3917 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, VALUE, 0.0 }` |
|      - | 3918 | `/*` |
|      - | 3919 | ` * Install the whole date family from C: the exceptions and DateTimeInterface, then` |
|      - | 3920 | ` * DateTimeZone / DateTime / DateTimeImmutable, DateInterval and DatePeriod, then the` |
|      - | 3921 | ` * procedural aliases. The InternalIterator its getIterator() answers is not declared` |
|      - | 3922 | ` * here — it is shared native machinery (oo_native.c), reached through the vtable` |
|      - | 3923 | ` * DatePeriod's spec row names.` |
|      - | 3924 | ` *` |
|      - | 3925 | ` * Called from PH7_VmInit inside the bCompilingBuiltin window, after the Reflection` |
|      - | 3926 | ` * install (Exception must exist). IteratorAggregate is attached AFTER DatePeriod's` |
|      - | 3927 | ` * methods exist, for the abstract-stub reason above; DateTimeInterface declares no` |
|      - | 3928 | ` * method, so it can ride the spec table.` |
|      - | 3929 | ` */` |
|   5254 | 3930 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
|      5 | 3931 | `{` |
|      - | 3932 | `	static const PH7_NativeConstDef aIfaceConst[] = {` |
|      - | 3933 | `		DT_IFACE_CONST("ATOM","Y-m-d\\TH:i:sP"),` |
|      - | 3934 | `		DT_IFACE_CONST("COOKIE","l, d-M-Y H:i:s T"),` |
|      - | 3935 | `		DT_IFACE_CONST("ISO8601","Y-m-d\\TH:i:sO"),` |
|      - | 3936 | `		DT_IFACE_CONST("ISO8601_EXPANDED","X-m-d\\TH:i:sP"),` |
|      - | 3937 | `		DT_IFACE_CONST("RFC822","D, d M y H:i:s O"),` |
|      - | 3938 | `		DT_IFACE_CONST("RFC850","l, d-M-y H:i:s T"),` |
|      - | 3939 | `		DT_IFACE_CONST("RFC1036","D, d M y H:i:s O"),` |
|      - | 3940 | `		DT_IFACE_CONST("RFC1123","D, d M Y H:i:s O"),` |
|      - | 3941 | `		DT_IFACE_CONST("RFC7231","D, d M Y H:i:s \\G\\M\\T"),` |
|      - | 3942 | `		DT_IFACE_CONST("RFC2822","D, d M Y H:i:s O"),` |
|      - | 3943 | `		DT_IFACE_CONST("RFC3339","Y-m-d\\TH:i:sP"),` |
|      - | 3944 | `		DT_IFACE_CONST("RFC3339_EXTENDED","Y-m-d\\TH:i:s.vP"),` |
|      - | 3945 | `		DT_IFACE_CONST("RSS","D, d M Y H:i:s O"),` |
|      - | 3946 | `		DT_IFACE_CONST("W3C","Y-m-d\\TH:i:sP"),` |
|      - | 3947 | `	};` |
|      - | 3948 | `	static const PH7_NativePropDef aZoneProp[] = {` |
|      - | 3949 | `		{ DTZ_OFF,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3950 | `		{ DTZ_NAME, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 }, 0 },` |
|      - | 3951 | `	};` |
|      - | 3952 | `	static const PH7_NativeMethodDef aZoneMethod[] = {` |
|      - | 3953 | `		{ "__construct", PH7_MOD_PUBLIC, "string $timezone", "", vm_builtin_DateTimeZone_construct },` |
|      - | 3954 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_DateTimeZone_getName },` |
|      - | 3955 | `		{ "getOffset",   PH7_MOD_PUBLIC, "DateTimeInterface $datetime", "@int",` |
|      - | 3956 | `		  vm_builtin_DateTimeZone_getOffset },` |
|      - | 3957 | `		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_DateTimeZone_serialize },` |
|      - | 3958 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|      - | 3959 | `		  vm_builtin_DateTimeZone_unserialize },` |
|      - | 3960 | `		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DateTimeZone_wakeup },` |
|      - | 3961 | `		{ "__set_state",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@DateTimeZone",` |
|      - | 3962 | `		  vm_builtin_DateTimeZone_setState },` |
|      - | 3963 | `	};` |
|      - | 3964 | `	static const PH7_NativePropDef aDtProp[] = { DT_NATIVE_STATE_PROPS };` |
|      - | 3965 | `	static const PH7_NativeMethodDef aDtMethod[] = {` |
|      - | 3966 | `		DT_NATIVE_SHARED_METHODS("DateTime"),` |
|      - | 3967 | `		{ "createFromFormat",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 3968 | `		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTime\|false",` |
|      - | 3969 | `		  vm_builtin_DateTime_createFromFormat },` |
|      - | 3970 | `		{ "createFromImmutable", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeImmutable $object", "@static",` |
|      - | 3971 | `		  vm_builtin_DateTime_copyOf },` |
|      - | 3972 | `		{ "createFromInterface", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTime",` |
|      - | 3973 | `		  vm_builtin_DateTime_copyOf },` |
|      - | 3974 | `		DT_NATIVE_SERIAL_METHODS(DateTime),` |
|      - | 3975 | `	};` |
|      - | 3976 | `	static const PH7_NativeMethodDef aImmMethod[] = {` |
|      - | 3977 | `		DT_NATIVE_SHARED_METHODS("DateTimeImmutable"),` |
|      - | 3978 | `		{ "createFromFormat",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 3979 | `		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTimeImmutable\|false",` |
|      - | 3980 | `		  vm_builtin_DateTimeImmutable_createFromFormat },` |
|      - | 3981 | `		{ "createFromMutable",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTime $object", "@static",` |
|      - | 3982 | `		  vm_builtin_DateTimeImmutable_copyOf },` |
|      - | 3983 | `		{ "createFromInterface", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTimeImmutable",` |
|      - | 3984 | `		  vm_builtin_DateTimeImmutable_copyOf },` |
|      - | 3985 | `		DT_NATIVE_SERIAL_METHODS(DateTimeImmutable),` |
|      - | 3986 | `	};` |
|      - | 3987 | `	static const PH7_NativePropDef aIvProp[] = {` |
|      - | 3988 | `		{ "y",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3989 | `		{ "m",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3990 | `		{ "d",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3991 | `		{ "h",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3992 | `		{ "i",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3993 | `		{ "s",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3994 | ``		/* php's `f` is a FLOAT; the chunk's `= 0` made it an int. */`` |
|      - | 3995 | `		{ "f",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_DOUBLE, 0, 0, 0.0 }, 0 },` |
|      - | 3996 | `		{ "invert",      PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3997 | `		{ "days",        PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - | 3998 | `		{ "from_string", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - | 3999 | ``		/* php's timelib_rel_time.us, the count `f` renders: see DtIvUsec. */`` |
|      - | 4000 | `		{ DT_IV_US,      PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|      - | 4001 | `	};` |
|      - | 4002 | `	static const PH7_NativeMethodDef aIvMethod[] = {` |
|      - | 4003 | `		{ "__construct", PH7_MOD_PUBLIC, "string $duration", "",` |
|      - | 4004 | `		  vm_builtin_DateInterval_construct },` |
|      - | 4005 | `		{ "createFromDateString", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $datetime", "@DateInterval",` |
|      - | 4006 | `		  vm_builtin_DateInterval_createFromDateString },` |
|      - | 4007 | `		{ "format",      PH7_MOD_PUBLIC, "string $format", "@string", vm_builtin_DateInterval_format },` |
|      - | 4008 | `	};` |
|      - | 4009 | `	/* php models all seven as VIRTUAL hooked properties, so it reports no default` |
|      - | 4010 | `	 * for any of them; PHL's are real slots and keep theirs, because a read before` |
|      - | 4011 | `	 * the first write must answer what php's getter answers rather than raise. The` |
|      - | 4012 | `	 * TYPE is what a spec row can state exactly — the virtual half is PLAN §7.4. */` |
|      - | 4013 | `	static const PH7_NativePropDef aDpProp[] = {` |
|      - | 4014 | `		{ "start",              PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 4015 | `		{ "current",            PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 4016 | `		{ "end",                PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 4017 | `		{ "interval",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateInterval" },` |
|      - | 4018 | `		{ "recurrences",        PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,  1, 0, 0.0 }, "int" },` |
|      - | 4019 | `		{ "include_start_date", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, "bool" },` |
|      - | 4020 | `		{ "include_end_date",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|      - | 4021 | `	};` |
|      - | 4022 | `	static const PH7_NativeConstDef aDpConst[] = {` |
|      - | 4023 | `		{ "EXCLUDE_START_DATE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|      - | 4024 | `		{ "INCLUDE_END_DATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|      - | 4025 | `	};` |
|      - | 4026 | `	static const PH7_NativeMethodDef aDpMethod[] = {` |
|      - | 4027 | `		/* php overloads this constructor three ways and rejects everything else with` |
|      - | 4028 | `		 * ONE message, so the signature stays unenforced and the body decides. */` |
|      - | 4029 | `		{ "__construct",     PH7_MOD_PUBLIC, 0, "", vm_builtin_DatePeriod_construct },` |
|      - | 4030 | `		{ "createFromISO8601String", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 4031 | `		  "string $specification, int $options = 0", "static",` |
|      - | 4032 | `		  vm_builtin_DatePeriod_createFromISO8601String },` |
|      - | 4033 | `		{ "getStartDate",    PH7_MOD_PUBLIC, "", "@DateTimeInterface",` |
|      - | 4034 | `		  vm_builtin_DatePeriod_getStartDate },` |
|      - | 4035 | `		{ "getEndDate",      PH7_MOD_PUBLIC, "", "@?DateTimeInterface",` |
|      - | 4036 | `		  vm_builtin_DatePeriod_getEndDate },` |
|      - | 4037 | `		{ "getDateInterval", PH7_MOD_PUBLIC, "", "@DateInterval",` |
|      - | 4038 | `		  vm_builtin_DatePeriod_getDateInterval },` |
|      - | 4039 | `		{ "getRecurrences",  PH7_MOD_PUBLIC, "", "@?int", vm_builtin_DatePeriod_getRecurrences },` |
|      - | 4040 | `		{ "getIterator",     PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_DatePeriod_getIterator },` |
|      - | 4041 | `	};` |
|      - | 4042 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 4043 | `		/* Exceptions first: the classes below throw them. */` |
|      - | 4044 | `		{ "DateException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4045 | `		{ "DateMalformedStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4046 | `		{ "DateInvalidTimeZoneException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4047 | `		{ "DateMalformedIntervalStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4048 | `		{ "DateMalformedPeriodStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4049 | `		{ "DateInvalidOperationException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4050 | `		/* php's date tree has an ERROR half beside the exception one -- what a` |
|      - | 4051 | `		 * caller catches when an argument is out of RANGE (setMicrosecond) or the` |
|      - | 4052 | `		 * object was never constructed. All three were undefined here, so` |
|      - | 4053 | ``		 * `catch (DateRangeError $e)` could not be spelled at all. */`` |
|      - | 4054 | `		{ "DateError", "Error", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4055 | `		{ "DateRangeError", "DateError", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4056 | `		{ "DateObjectError", "DateError", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4057 | `		{ "DateTimeInterface", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 4058 | `		  0, 0, aIfaceConst, SX_ARRAYSIZE(aIfaceConst), 0, 0, 0, 0, 0 },` |
|      - | 4059 | `		{ "DateTimeZone", 0, 0, 0,` |
|      - | 4060 | `		  aZoneMethod, SX_ARRAYSIZE(aZoneMethod), 0, 0, aZoneProp, SX_ARRAYSIZE(aZoneProp),` |
|      - | 4061 | `		  0, 0, DtPresentTimeZone },` |
|      - | 4062 | `		{ "DateTime", 0, "DateTimeInterface", 0,` |
|      - | 4063 | `		  aDtMethod, SX_ARRAYSIZE(aDtMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),` |
|      - | 4064 | `		  0, 0, DtPresentDateTime },` |
|      - | 4065 | `		{ "DateTimeImmutable", 0, "DateTimeInterface", 0,` |
|      - | 4066 | `		  aImmMethod, SX_ARRAYSIZE(aImmMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),` |
|      - | 4067 | `		  0, 0, DtPresentDateTime },` |
|      - | 4068 | `		{ "DateInterval", 0, 0, 0,` |
|      - | 4069 | `		  aIvMethod, SX_ARRAYSIZE(aIvMethod), 0, 0, aIvProp, SX_ARRAYSIZE(aIvProp), 0, 0, 0 },` |
|      - | 4070 | `		{ "DatePeriod", 0, 0, 0,` |
|      - | 4071 | `		  aDpMethod, SX_ARRAYSIZE(aDpMethod), aDpConst, SX_ARRAYSIZE(aDpConst),` |
|      - | 4072 | `		  aDpProp, SX_ARRAYSIZE(aDpProp), 0, &sDpIterVtab, 0 },` |
|      - | 4073 | `	};` |
|      - | 4074 | `	/* php's procedural aliases. Each is a function in its own right, not a forward,` |
|      - | 4075 | `	 * and each owes aBuiltinSig[] a row (vm_arg_check.c). */` |
|      - | 4076 | `	static const struct {` |
|      - | 4077 | `		const char *zName;` |
|      - | 4078 | `		ProchHostFunction xFunc;` |
|      - | 4079 | `	} aFunc[] = {` |
|      - | 4080 | `		{ "strtotime",                    vm_builtin_strtotime },` |
|      - | 4081 | `		{ "date_create",                  vm_builtin_date_create },` |
|      - | 4082 | `		{ "date_create_immutable",        vm_builtin_date_create_immutable },` |
|      - | 4083 | `		{ "date_create_from_format",      vm_builtin_date_create_from_format },` |
|      - | 4084 | `		{ "date_create_immutable_from_format", vm_builtin_date_create_immutable_from_format },` |
|      - | 4085 | `		{ "date_format",                  vm_builtin_date_format },` |
|      - | 4086 | `		{ "date_modify",                  vm_builtin_date_modify },` |
|      - | 4087 | `		{ "date_add",                     vm_builtin_date_add },` |
|      - | 4088 | `		{ "date_sub",                     vm_builtin_date_sub },` |
|      - | 4089 | `		{ "date_diff",                    vm_builtin_date_diff },` |
|      - | 4090 | `		{ "date_timestamp_get",           vm_builtin_date_timestamp_get },` |
|      - | 4091 | `		{ "date_timestamp_set",           vm_builtin_date_timestamp_set },` |
|      - | 4092 | `		{ "date_timezone_get",            vm_builtin_date_timezone_get },` |
|      - | 4093 | `		{ "date_timezone_set",            vm_builtin_date_timezone_set },` |
|      - | 4094 | `		{ "date_offset_get",              vm_builtin_date_offset_get },` |
|      - | 4095 | `		{ "date_date_set",                vm_builtin_date_date_set },` |
|      - | 4096 | `		{ "date_time_set",                vm_builtin_date_time_set },` |
|      - | 4097 | `		{ "date_isodate_set",             vm_builtin_date_isodate_set },` |
|      - | 4098 | `		{ "date_interval_create_from_date_string", vm_builtin_date_interval_create_from_date_string },` |
|      - | 4099 | `		{ "date_interval_format",         vm_builtin_date_interval_format },` |
|      - | 4100 | `		{ "date_get_last_errors",         vm_builtin_date_get_last_errors },` |
|      - | 4101 | `		{ "timezone_open",                vm_builtin_timezone_open },` |
|      - | 4102 | `		{ "timezone_name_get",            vm_builtin_timezone_name_get },` |
|      - | 4103 | `		{ "timezone_offset_get",          vm_builtin_timezone_offset_get },` |
|      - | 4104 | `	};` |
|      - | 4105 | `	sxu32 n;` |
|      - | 4106 | `	sxi32 rc;` |
|      - | 4107 | `	/* php's date.timezone default */` |
|   5259 | 4108 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|   5259 | 4109 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|   5259 | 4110 | `	DtLastErrClear(&(*pVm));` |
| 131355 | 4111 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 126101 | 4112 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  63053 | 4113 | `	}` |
|   5259 | 4114 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|   5259 | 4115 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 4116 | `		return rc;` |
|      - | 4117 | `	}` |
|      - | 4118 | `	/* php's write_property handler for DateInterval (ph7_class::xSet), assigned` |
|      - | 4119 | `	 * here for the reason the DOM's clone and dimension hooks are: the spec table` |
|      - | 4120 | `	 * carries no field for a hook. It also flags the class's properties, which is` |
|      - | 4121 | ``	 * what makes `new` register their slots with the store filter. */`` |
|   5259 | 4122 | `	rc = PH7_NativeClassInstallSetHook(&(*pVm),"DateInterval",DtIntervalSet);` |
|   5259 | 4123 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 4124 | `		return rc;` |
|      - | 4125 | `	}` |
|      - | 4126 | `	/* IteratorAggregate declares a METHOD, so it is attached now that DatePeriod has` |
|      - | 4127 | `	 * its own: PH7_ClassImplement stubs a missing one as ABSTRACT, which would have` |
|      - | 4128 | `	 * made the class uninstantiable. */` |
|      - | 4129 | `	{` |
|   5259 | 4130 | `		ph7_class *pPeriod = DtClass(&(*pVm),"DatePeriod");` |
|   5259 | 4131 | `		ph7_class *pAggregate = DtClass(&(*pVm),"IteratorAggregate");` |
|   5259 | 4132 | `		if( pPeriod == 0 \|\| pAggregate == 0 ){` |
|    ! 0 | 4133 | `			return SXERR_NOTFOUND;` |
|      - | 4134 | `		}` |
|   5259 | 4135 | `		rc = PH7_ClassImplement(pPeriod,pAggregate);` |
|      - | 4136 | `	}` |
|   5259 | 4137 | `	return rc;` |
|   2632 | 4138 | `}` |
|      - | 4139 |  |
|      - | 4140 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 4141 |  |
|      - | 4142 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 4143 | `/* Tiny build: no DateTime family (builtin layer disabled) */` |
|      - | 4144 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){` |
|      - | 4145 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|      - | 4146 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|      - | 4147 | `	return SXRET_OK;` |
|      - | 4148 | `}` |
|      - | 4149 | `#endif` |
|      - | 4150 |  |
