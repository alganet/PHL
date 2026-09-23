# src/ph7/builtin_date_parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2244/2604 lines (86.18%)

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
|   1414 |   29 | `PH7_PRIVATE sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|      2 |   30 | `{` |
|      - |   31 | `	sxi64 era;` |
|      - |   32 | `	unsigned yoe,doy,doe;` |
|   1416 |   33 | `	y -= (m <= 2);` |
|   1416 |   34 | `	era = (y >= 0 ? y : y - 399) / 400;` |
|   1416 |   35 | `	yoe = (unsigned)(y - era * 400);` |
|   1416 |   36 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|   1416 |   37 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|   1416 |   38 | `	return era * 146097 + (sxi64)doe - 719468;` |
|      2 |   39 | `}` |
|    644 |   40 | `PH7_PRIVATE void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|      1 |   41 | `{` |
|      - |   42 | `	sxi64 era;` |
|      - |   43 | `	unsigned doe,yoe,doy,mp;` |
|    645 |   44 | `	z += 719468;` |
|    645 |   45 | `	era = (z >= 0 ? z : z - 146096) / 146097;` |
|    645 |   46 | `	doe = (unsigned)(z - era * 146097);` |
|    645 |   47 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|    645 |   48 | `	*py = (sxi64)yoe + era * 400;` |
|    645 |   49 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|    645 |   50 | `	mp = (5 * doy + 2) / 153;` |
|    645 |   51 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|    645 |   52 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|    645 |   53 | `	if( *pm <= 2 ){` |
|    347 |   54 | `		*py += 1;` |
|    173 |   55 | `	}` |
|    645 |   56 | `}` |
|   1058 |   57 | `PH7_PRIVATE sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|      1 |   58 | `{` |
|   1059 |   59 | `	sxi64 q = a / b;` |
|   1059 |   60 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|      3 |   61 | `		q--;` |
|      1 |   62 | `	}` |
|   1059 |   63 | `	return q;` |
|      1 |   64 | `}` |
|      - |   65 | `/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z) */` |
|    286 |   66 | `static void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)` |
|      1 |   67 | `{` |
|    287 |   68 | `	sxi64 t = iTs + iOff;` |
|    287 |   69 | `	sxi64 days = DtFloorDiv(t,86400);` |
|    287 |   70 | `	sxi64 secs = t - days * 86400;` |
|      - |   71 | `	sxi64 y;` |
|      - |   72 | `	int mo,d;` |
|    287 |   73 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|    287 |   74 | `	pTm->tm_sec  = (int)(secs % 60);` |
|    287 |   75 | `	pTm->tm_min  = (int)((secs / 60) % 60);` |
|    287 |   76 | `	pTm->tm_hour = (int)(secs / 3600);` |
|    287 |   77 | `	pTm->tm_mday = d;` |
|    287 |   78 | `	pTm->tm_mon  = mo - 1;` |
|    287 |   79 | `	pTm->tm_year = (int)y;` |
|    287 |   80 | `	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */` |
|    287 |   81 | `	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));` |
|    287 |   82 | `	pTm->tm_isdst = 0;` |
|    287 |   83 | `	pTm->tm_zone = zZone;` |
|    287 |   84 | `	pTm->tm_gmtoff = (long)iOff;` |
|    287 |   85 | `}` |
|    368 |   86 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|      2 |   87 | `{` |
|    370 |   88 | `	return DtDaysFromCivil(y,mo,d) * 86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
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
|     48 |  111 | `static int DtReadFraction(const char **pz,const char *zEnd)` |
|      1 |  112 | `{` |
|     49 |  113 | `	const char *z = *pz;` |
|     49 |  114 | `	int us = 0,n = 0;` |
|     49 |  115 | `	z++; /* skip '.' */` |
|    279 |  116 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|    231 |  117 | `		if( n < 6 ){ us = us*10 + (z[0]-'0'); n++; }` |
|    231 |  118 | `		z++;` |
|      1 |  119 | `	}` |
|    107 |  120 | `	while( n < 6 ){ us *= 10; n++; }` |
|     49 |  121 | `	*pz = z;` |
|     49 |  122 | `	return us;` |
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
|    308 |  133 | `static int DtTimeSuffix(const char **pz,const char *zEnd,const char *zIn,` |
|      - |  134 | `	int *ph,int *pmi,int *ps,sxi32 *piOff,int *pbOffSet,int *pUs)` |
|      2 |  135 | `{` |
|    310 |  136 | `	const char *z = *pz;` |
|    308 |  137 | `	if( z < zEnd && (z[0]=='T' \|\| z[0]==' ') && zEnd-z >= 6` |
|    160 |  138 | `	 && SyisDigit(z[1]) && SyisDigit(z[2]) && z[3]==':' ){` |
|    155 |  139 | `		z++;` |
|    155 |  140 | `		*ph  = (z[0]-'0')*10 + (z[1]-'0');` |
|    155 |  141 | `		*pmi = (z[3]-'0')*10 + (z[4]-'0');` |
|      - |  142 | `		/* a 25+ hour kills php's whole time token: error at its start */` |
|    155 |  143 | `		if( *ph > 24 ){ return (int)(z - zIn) + 1; }` |
|      - |  144 | `		/* php lexes HH:M, then the minute's second digit starts a SECOND time` |
|      - |  145 | `		 * token: "Double time specification" (negative encoding) */` |
|    153 |  146 | `		if( *pmi > 59 ){ return -((int)(&z[4] - zIn) + 1); }` |
|    151 |  147 | `		z += 5;` |
|    151 |  148 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|    149 |  149 | `			*ps = (z[1]-'0')*10 + (z[2]-'0');` |
|    149 |  150 | `			if( *ps > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|    147 |  151 | `			z += 3;` |
|     73 |  152 | `		}` |
|    149 |  153 | `		if( z < zEnd && z[0]=='.' && zEnd-z >= 2 && SyisDigit(z[1]) ){ /* fractional seconds */` |
|     47 |  154 | `			*pUs = DtReadFraction(&z,zEnd);` |
|     23 |  155 | `		}` |
|    149 |  156 | `		if( z < zEnd && (z[0]=='Z' \|\| z[0]=='z') ){` |
|     13 |  157 | `			*piOff = 0; *pbOffSet = 2; z++;` |
|    143 |  158 | `		}else if( z < zEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
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
|     74 |  173 | `	}` |
|    304 |  174 | `	*pz = z;` |
|    304 |  175 | `	return 0;` |
|    156 |  176 | `}` |
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
|    282 |  272 | `static int DtMatchMonth(const char *z,const char *zEnd,int *pAdv)` |
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
|   5955 |  284 | `	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){` |
|   5735 |  285 | `		int n = aM[i].n;` |
|   5734 |  286 | `		if( zEnd - z >= n && SyStrnicmp(z,aM[i].z,(sxu32)n) == 0` |
|   2582 |  287 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|     63 |  288 | `			*pAdv = n;` |
|     63 |  289 | `			return aM[i].mo;` |
|      - |  290 | `		}` |
|   2837 |  291 | `	}` |
|    221 |  292 | `	return 0;` |
|    142 |  293 | `}` |
|      - |  294 | `/* Match a weekday name at z (full or 3-letter, case-insensitive, word boundary).` |
|      - |  295 | ` * Returns the day-of-week 0=Sunday..6=Saturday and sets *pAdv, or -1. */` |
|    182 |  296 | `static int DtMatchWeekday(const char *z,const char *zEnd,int *pAdv)` |
|      1 |  297 | `{` |
|      - |  298 | `	static const struct { const char *z; int n; int dow; } aW[] = {` |
|      - |  299 | `		{ "sunday",6,0 },{ "monday",6,1 },{ "tuesday",7,2 },{ "wednesday",9,3 },` |
|      - |  300 | `		{ "thursday",8,4 },{ "friday",6,5 },{ "saturday",8,6 },` |
|      - |  301 | `		{ "sun",3,0 },{ "mon",3,1 },{ "tue",3,2 },{ "wed",3,3 },{ "thu",3,4 },` |
|      - |  302 | `		{ "fri",3,5 },{ "sat",3,6 }` |
|      - |  303 | `	};` |
|      - |  304 | `	sxu32 i;` |
|   2259 |  305 | `	for( i = 0 ; i < SX_ARRAYSIZE(aW) ; ++i ){` |
|   2123 |  306 | `		int n = aW[i].n;` |
|   2122 |  307 | `		if( zEnd - z >= n && SyStrnicmp(z,aW[i].z,(sxu32)n) == 0` |
|    884 |  308 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|     47 |  309 | `			*pAdv = n;` |
|     47 |  310 | `			return aW[i].dow;` |
|      - |  311 | `		}` |
|   1039 |  312 | `	}` |
|    137 |  313 | `	return -1;` |
|     92 |  314 | `}` |
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
|    222 |  332 | `static int DtTryMonthDate(const char *z,const char *zEnd,const char **pzOut,` |
|      - |  333 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,sxi64 iBaseTs,int *pUs)` |
|      1 |  334 | `{` |
|    223 |  335 | `	int mo,d = 1,adv,haveDay = 0,haveYear = 0;` |
|    223 |  336 | `	sxi64 y = 0;` |
|    223 |  337 | `	int h = 0,mi = 0,s = 0,us = 0;` |
|    223 |  338 | `	sxi32 iOff = *pOff;` |
|      - |  339 | `	int rcT;` |
|      - |  340 | `#define MDSKIPWS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|    223 |  341 | `	if( (mo = DtMatchMonth(z,zEnd,&adv)) != 0 ){` |
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
|    208 |  357 | `	}else if( SyisDigit(z[0]) ){` |
|      - |  358 | `		/* Day MonthName [Year] */` |
|     49 |  359 | `		d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|     49 |  360 | `		if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|     49 |  361 | `		haveDay = 1;` |
|    107 |  362 | `		MDSKIPWS();` |
|     49 |  363 | `		if( (mo = DtMatchMonth(z,zEnd,&adv)) == 0 ){ return 0; }` |
|     21 |  364 | `		z += adv;` |
|     49 |  365 | `		MDSKIPWS();` |
|     11 |  366 | `	}else{` |
|    145 |  367 | `		return 0;` |
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
|    112 |  406 | `}` |
|      - |  407 | `/*` |
|      - |  408 | ` * Minimal php-datetime-string parser (slice 1): absolute forms` |
|      - |  409 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]",` |
|      - |  410 | ` * keywords today/midnight/noon/tomorrow/yesterday, and relative sequences` |
|      - |  411 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". Returns 0 on` |
|      - |  412 | ` * success (ts/off/bOffSet out), or the byte position of the first` |
|      - |  413 | ` * unparseable character +1 (for php's "at position N" message).` |
|      - |  414 | ` */` |
|    566 |  415 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,` |
|      - |  416 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs)` |
|      2 |  417 | `{` |
|    568 |  418 | `	const char *z = zIn, *zEnd = &zIn[nLen];` |
|    568 |  419 | `	sxi64 iTs = iBaseTs;` |
|    568 |  420 | `	sxi32 iOff = iBaseOff;` |
|    568 |  421 | `	int bOffSet = 0;` |
|    568 |  422 | `	int bAny = 0;` |
|      - |  423 | `	int iNumRc,iMonRc;` |
|    568 |  424 | `	int uSec = 0;` |
|    568 |  425 | `	*pUs = 0;` |
|      - |  426 | `#define DT_SKIP_WS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|      - |  427 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|      - |  428 | `	&& (zEnd-z == (nKw) \|\| !SyisAlpha(z[(nKw)])))` |
|    858 |  429 | `	DT_SKIP_WS();` |
|    568 |  430 | `	if( z >= zEnd ){` |
|      - |  431 | `		/* php: the empty string is "now" */` |
|      3 |  432 | `		*pTs = iTs;` |
|      3 |  433 | `		*pOff = iOff;` |
|      3 |  434 | `		*pbOffSet = bOffSet;` |
|      3 |  435 | `		return 0;` |
|      - |  436 | `	}` |
|      - |  437 | `	/* "@<seconds>" absolute epoch */` |
|    566 |  438 | `	if( z[0] == '@' ){` |
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
|    526 |  458 | `	if( zEnd-z >= 10 && SyisDigit(z[0]) && SyisDigit(z[1]) && SyisDigit(z[2])` |
|    260 |  459 | `	 && SyisDigit(z[3]) && z[4]=='-' ){` |
|    224 |  460 | `		sxi64 y = (z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0');` |
|    224 |  461 | `		int mo,d,h=0,mi=0,s=0;` |
|    224 |  462 | `		if( !SyisDigit(z[5])\|\|!SyisDigit(z[6])\|\|z[7] != '-'\|\|!SyisDigit(z[8])\|\|!SyisDigit(z[9]) ){` |
|    ! 0 |  463 | `			return (int)(z - zIn) + 1;` |
|      - |  464 | `		}` |
|    224 |  465 | `		mo = (z[5]-'0')*10 + (z[6]-'0');` |
|    224 |  466 | `		d  = (z[8]-'0')*10 + (z[9]-'0');` |
|      - |  467 | `		/* php's lexer dies on the SECOND digit of an out-of-range month/day` |
|      - |  468 | `		 * (either the two-digit pattern fails there, or a one-digit component` |
|      - |  469 | `		 * matched and the separator check fails there); "00" lexes fine and` |
|      - |  470 | `		 * normalizes (month 0 == December of the previous year). */` |
|    224 |  471 | `		if( mo > 12 ){ return (int)(&z[6] - zIn) + 1; }` |
|    218 |  472 | `		if( d > 31 ){ return (int)(&z[9] - zIn) + 1; }` |
|    214 |  473 | `		if( mo == 0 ){ mo = 12; y--; }` |
|    214 |  474 | `		z += 10;` |
|      - |  475 | `		{` |
|    214 |  476 | `			int rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,&bOffSet,&uSec);` |
|    214 |  477 | `			if( rcT != 0 ){ return rcT; }` |
|      - |  478 | `		}` |
|    208 |  479 | `		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    208 |  480 | `		bAny = 1;` |
|    409 |  481 | `	}else if( SyisDigit(z[0])` |
|    205 |  482 | `	 && (iNumRc = DtTryNumericDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,&uSec)) != 0 ){` |
|      - |  483 | `		/* DD-MM-YYYY / DD.MM.YYYY (day first), MM/DD/YYYY (slash, American), and` |
|      - |  484 | `		 * YYYY/MM/DD (slash, year first) — see DtTryNumericDate. Anything other than` |
|      - |  485 | `		 * 1 is an error code in DtParse's own convention (positive position / negative` |
|      - |  486 | `		 * "double time"); propagate it verbatim. */` |
|     55 |  487 | `		if( iNumRc != 1 ){ return iNumRc; }` |
|     47 |  488 | `		bAny = 1;` |
|    274 |  489 | `	}else if( (SyisAlpha(z[0]) \|\| SyisDigit(z[0]))` |
|    238 |  490 | `	 && (iMonRc = DtTryMonthDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,iBaseTs,&uSec)) != 0 ){` |
|      - |  491 | `		/* MonthName Day Year / Day MonthName Year, in any of php's spellings. As with` |
|      - |  492 | `		 * DtTryNumericDate, anything other than 1 is an error code to propagate. */` |
|     51 |  493 | `		if( iMonRc != 1 ){ return iMonRc; }` |
|     51 |  494 | `		bAny = 1;` |
|    226 |  495 | `	}else if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
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
|    193 |  515 | `	}else if( DT_LOWEQ("now",3) ){` |
|     19 |  516 | `		z += 3;` |
|     19 |  517 | `		bAny = 1;` |
|      9 |  518 | `	}` |
|      - |  519 | `	/* Relative / keyword sequence */` |
|    245 |  520 | `	for(;;){` |
|    806 |  521 | `		DT_SKIP_WS();` |
|    666 |  522 | `		if( z >= zEnd ){` |
|    472 |  523 | `			break;` |
|      - |  524 | `		}` |
|    195 |  525 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|      9 |  526 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|      9 |  527 | `			iTs = days*86400 - iOff;` |
|      9 |  528 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|      9 |  529 | `			bAny = 1;` |
|      9 |  530 | `			continue;` |
|      - |  531 | `		}` |
|    187 |  532 | `		if( DT_LOWEQ("noon",4) ){` |
|      3 |  533 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|      3 |  534 | `			iTs = days*86400 + 12*3600 - iOff;` |
|      3 |  535 | `			z += 4;` |
|      3 |  536 | `			bAny = 1;` |
|      3 |  537 | `			continue;` |
|      - |  538 | `		}` |
|    185 |  539 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|      3 |  540 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) + 1;` |
|      3 |  541 | `			iTs = days*86400 - iOff;` |
|      3 |  542 | `			z += 8;` |
|      3 |  543 | `			bAny = 1;` |
|      3 |  544 | `			continue;` |
|      - |  545 | `		}` |
|    183 |  546 | `		if( DT_LOWEQ("yesterday",9) ){` |
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
|    181 |  557 | `			const char *zSave = z;` |
|    181 |  558 | `			int dir = 0;         /* 0 = this-week occurrence, 1 = next, -1 = last */` |
|      - |  559 | `			int adv,dow;` |
|    217 |  560 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; DT_SKIP_WS(); }` |
|    160 |  561 | `			else if( DT_LOWEQ("previous",8) ){ dir = -1; z += 8; DT_SKIP_WS(); }` |
|    203 |  562 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; DT_SKIP_WS(); }` |
|    138 |  563 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; DT_SKIP_WS(); }` |
|    181 |  564 | `			dow = DtMatchWeekday(z,zEnd,&adv);` |
|    181 |  565 | `			if( dow >= 0 ){` |
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
|    135 |  583 | `			z = zSave; /* prefix did not introduce a weekday: rewind and try the rest */` |
|      - |  584 | `		}` |
|      - |  585 | `		/* "first\|last day of (this\|next\|last month \| MonthName [Year])": jump to the` |
|      - |  586 | `		 * first or last day of a target month. A this/next/last-month target keeps the` |
|      - |  587 | `		 * base time-of-day; an absolute MonthName [Year] target resets it to midnight` |
|      - |  588 | `		 * (php). */` |
|    135 |  589 | `		if( DT_LOWEQ("first",5) \|\| DT_LOWEQ("last",4) ){` |
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
|    127 |  641 | `			const char *zSave = z;` |
|    127 |  642 | `			int dir = 2; /* 2 = no prefix */` |
|    127 |  643 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; }` |
|     97 |  644 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; }` |
|     89 |  645 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; }` |
|    107 |  646 | `			if( dir != 2 ){` |
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
|     83 |  666 | `			z = zSave;` |
|      - |  667 | `		}` |
|      - |  668 | `		/* Trailing time-of-day in a relative sequence ("next thursday 15:00"): set` |
|      - |  669 | `		 * the clock on the current day. The leading absolute HH:MM branch handles a` |
|      - |  670 | `		 * time at the START; this handles one AFTER a date/relative token. */` |
|     82 |  671 | `		if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
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
|     71 |  689 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|     53 |  690 | `			int neg = 0;` |
|     53 |  691 | `			sxi64 v = 0;` |
|     53 |  692 | `			const char *zNumStart = z;` |
|     53 |  693 | `			if( z[0]=='+' \|\| z[0]=='-' ){` |
|     33 |  694 | `				neg = (z[0]=='-');` |
|     33 |  695 | `				z++;` |
|      - |  696 | `				/* php's lexer takes the sign as its own token, so whitespace may` |
|      - |  697 | `				 * follow it: "1 year + 3 months" is a relative sequence there and` |
|      - |  698 | `				 * was a parse FAILURE here. */` |
|     53 |  699 | `				DT_SKIP_WS();` |
|     16 |  700 | `			}` |
|     53 |  701 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zNumStart - zIn) + 1; }` |
|    123 |  702 | `			while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|     53 |  703 | `			if( neg ){ v = -v; }` |
|    129 |  704 | `			DT_SKIP_WS();` |
|     53 |  705 | `			if( DT_LOWEQ("seconds",7) )     { iTs += v;            z += 7; }` |
|     53 |  706 | `			else if( DT_LOWEQ("second",6) ) { iTs += v;            z += 6; }` |
|     53 |  707 | `			else if( DT_LOWEQ("secs",4) )   { iTs += v;            z += 4; }` |
|     53 |  708 | `			else if( DT_LOWEQ("sec",3) )    { iTs += v;            z += 3; }` |
|     53 |  709 | `			else if( DT_LOWEQ("minutes",7) ){ iTs += v*60;         z += 7; }` |
|     51 |  710 | `			else if( DT_LOWEQ("minute",6) ) { iTs += v*60;         z += 6; }` |
|     51 |  711 | `			else if( DT_LOWEQ("mins",4) )   { iTs += v*60;         z += 4; }` |
|     51 |  712 | `			else if( DT_LOWEQ("min",3) )    { iTs += v*60;         z += 3; }` |
|     51 |  713 | `			else if( DT_LOWEQ("hours",5) )  { iTs += v*3600;       z += 5; }` |
|     45 |  714 | `			else if( DT_LOWEQ("hour",4) )   { iTs += v*3600;       z += 4; }` |
|     45 |  715 | `			else if( DT_LOWEQ("days",4) )   { iTs += v*86400;      z += 4; }` |
|     41 |  716 | `			else if( DT_LOWEQ("day",3) )    { iTs += v*86400;      z += 3; }` |
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
|     47 |  728 | `			bAny = 1;` |
|     47 |  729 | `			continue;` |
|      - |  730 | `		}` |
|     19 |  731 | `		return (int)(z - zIn) + 1;` |
|    ! 0 |  732 | `	}` |
|    472 |  733 | `	if( !bAny ){` |
|    ! 0 |  734 | `		return 1;` |
|      - |  735 | `	}` |
|    472 |  736 | `	*pTs = iTs;` |
|    472 |  737 | `	*pOff = iOff;` |
|    472 |  738 | `	*pbOffSet = bOffSet;` |
|    472 |  739 | `	*pUs = uSec;` |
|    472 |  740 | `	return 0;` |
|      - |  741 | `#undef DT_SKIP_WS` |
|      - |  742 | `#undef DT_LOWEQ` |
|    284 |  743 | `}` |
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
|     70 |  767 | `static int DtDaysInMonth(sxi64 y,int m)` |
|      1 |  768 | `{` |
|      - |  769 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|     71 |  770 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|      5 |  771 | `		return 29;` |
|      - |  772 | `	}` |
|     67 |  773 | `	return aMonDays[(m - 1) % 12];` |
|     36 |  774 | `}` |
|      - |  775 | `/*` |
|      - |  776 | ` * php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|      - |  777 | ` * (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset.` |
|      - |  778 | ` */` |
|    116 |  779 | `static sxi64 DtCivilAdd(sxi64 iTs,sxi32 iOff,sxi64 y,sxi64 m,sxi64 d,` |
|      - |  780 | `	sxi64 h,sxi64 i,sxi64 s,int iSign)` |
|      1 |  781 | `{` |
|      - |  782 | `	sxi64 iLocal,iDays,iSecs,y0,moT,dayCount;` |
|      - |  783 | `	int mo0,d0;` |
|    117 |  784 | `	iSign = iSign < 0 ? -1 : 1;` |
|    117 |  785 | `	iLocal = iTs + iOff;` |
|    117 |  786 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    117 |  787 | `	iSecs  = iLocal - iDays*86400;` |
|    117 |  788 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    117 |  789 | `	y0 += iSign * y;` |
|    117 |  790 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    117 |  791 | `	y0 += DtFloorDiv(moT,12);` |
|    117 |  792 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    117 |  793 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    117 |  794 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    117 |  795 | `	return iLocal - iOff;` |
|      1 |  796 | `}` |
|      - |  797 | `/* One DateInterval's worth of fields, as diff() computes them. */` |
|      - |  798 | `typedef struct dt_diff dt_diff;` |
|      - |  799 | `struct dt_diff` |
|      - |  800 | `{` |
|      - |  801 | `	sxi64 y,m,d,h,i,s,nDays;` |
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
|     20 |  817 | `static void DtCivilDiff(sxi64 iTs1,sxi32 iOff,sxi64 iTs2,dt_diff *pOut)` |
|      1 |  818 | `{` |
|      - |  819 | `	sxi64 iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|      - |  820 | `	int moA,dA,moB,dB,bInvert;` |
|      - |  821 | `	sxi64 sA,sB,y,m,d,h,i,s;` |
|     21 |  822 | `	bInvert = iTs1 > iTs2;` |
|     21 |  823 | `	iA = bInvert ? iTs2 : iTs1;` |
|     21 |  824 | `	iB = bInvert ? iTs1 : iTs2;` |
|     21 |  825 | `	iLa = iA + iOff;` |
|     21 |  826 | `	iLb = iB + iOff;` |
|     21 |  827 | `	daysA = DtFloorDiv(iLa,86400);` |
|     21 |  828 | `	daysB = DtFloorDiv(iLb,86400);` |
|     21 |  829 | `	sA = iLa - daysA*86400;` |
|     21 |  830 | `	sB = iLb - daysB*86400;` |
|     21 |  831 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|     21 |  832 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|     21 |  833 | `	s = (sB % 60) - (sA % 60);` |
|     21 |  834 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|     21 |  835 | `	h = (sB / 3600) - (sA / 3600);` |
|     21 |  836 | `	d = dB - dA;` |
|     21 |  837 | `	m = moB - moA;` |
|     21 |  838 | `	y = yB - yA;` |
|     21 |  839 | `	if( s < 0 ){ s += 60; i--; }` |
|     21 |  840 | `	if( i < 0 ){ i += 60; h--; }` |
|     21 |  841 | `	if( h < 0 ){ h += 24; d--; }` |
|     21 |  842 | `	if( bInvert ){` |
|     13 |  843 | `		while( d < 0 ){` |
|      7 |  844 | `			d += DtDaysInMonth(yA,moA);` |
|      7 |  845 | `			m--;` |
|      7 |  846 | `			moA++;` |
|      7 |  847 | `			if( moA > 12 ){ moA = 1; yA++; }` |
|      1 |  848 | `		}` |
|      4 |  849 | `	}else{` |
|     23 |  850 | `		while( d < 0 ){` |
|      9 |  851 | `			moB--;` |
|      9 |  852 | `			if( moB < 1 ){ moB = 12; yB--; }` |
|      9 |  853 | `			d += DtDaysInMonth(yB,moB);` |
|      9 |  854 | `			m--;` |
|      1 |  855 | `		}` |
|      - |  856 | `	}` |
|     21 |  857 | `	if( m < 0 ){ m += 12; y--; }` |
|     21 |  858 | `	pOut->y = y;` |
|     21 |  859 | `	pOut->m = m;` |
|     21 |  860 | `	pOut->d = d;` |
|     21 |  861 | `	pOut->h = h;` |
|     21 |  862 | `	pOut->i = i;` |
|     21 |  863 | `	pOut->s = s;` |
|     21 |  864 | `	pOut->nDays = (iB - iA) / 86400;` |
|     21 |  865 | `	pOut->bInvert = bInvert;` |
|     21 |  866 | `}` |
|      - |  867 | `/*` |
|      - |  868 | ` * setISODate: jump to an ISO year/week/weekday, preserving the time of day.` |
|      - |  869 | ` */` |
|      8 |  870 | `static sxi64 DtIsoDate(sxi64 iTs,sxi32 iOff,sxi64 y,sxi64 w,sxi64 dow)` |
|      1 |  871 | `{` |
|      - |  872 | `	sxi64 iLocal,iTod,jan4,monday1,target;` |
|      - |  873 | `	int isoDow;` |
|      9 |  874 | `	iLocal = iTs + iOff;` |
|      9 |  875 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|      9 |  876 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|      9 |  877 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|      9 |  878 | `	monday1 = jan4 - (isoDow - 1);` |
|      9 |  879 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|      9 |  880 | `	return target*86400 + iTod - iOff;` |
|      1 |  881 | `}` |
|      - |  882 | `/* Consume nMin..nMax digits from *pz; returns count consumed (0 = failure) */` |
|    206 |  883 | `static int DtEatDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|      1 |  884 | `{` |
|    207 |  885 | `	const char *z = *pz;` |
|    207 |  886 | `	sxi64 v = 0;` |
|    207 |  887 | `	int n = 0;` |
|    733 |  888 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|    527 |  889 | `		v = v*10 + (z[0] - '0');` |
|    527 |  890 | `		z++;` |
|    527 |  891 | `		n++;` |
|      1 |  892 | `	}` |
|    207 |  893 | `	if( n < nMin ){` |
|      9 |  894 | `		return 0;` |
|      - |  895 | `	}` |
|    199 |  896 | `	*pz = z;` |
|    199 |  897 | `	*pVal = v;` |
|    199 |  898 | `	return n;` |
|    104 |  899 | `}` |
|      - |  900 | `/* timelib_get_nr's recovery: skip non-digits hunting for the field.` |
|      - |  901 | ` * Returns 1 = found+read, 0 = digits present but short, -1 = exhausted. */` |
|      8 |  902 | `static int DtHuntDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|      1 |  903 | `{` |
|      9 |  904 | `	const char *z = *pz;` |
|     45 |  905 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|      9 |  906 | `	*pz = z;` |
|      9 |  907 | `	if( z >= zEnd ){` |
|      9 |  908 | `		return -1;` |
|      - |  909 | `	}` |
|    ! 0 |  910 | `	return DtEatDigits(pz,zEnd,nMin,nMax,pVal) ? 1 : 0;` |
|      5 |  911 | `}` |
|      - |  912 | `/* Case-insensitive name-table lookup; returns 1-based index or 0 */` |
|     14 |  913 | `static int DtEatName(const char **pz,const char *zEnd,const char **azNames,int nNames)` |
|      1 |  914 | `{` |
|      - |  915 | `	int k;` |
|     23 |  916 | `	for( k = 0 ; k < nNames ; k++ ){` |
|     23 |  917 | `		int n = (int)SyStrlen(azNames[k]);` |
|     23 |  918 | `		if( zEnd - *pz >= n && SyStrnicmp(*pz,azNames[k],(sxu32)n) == 0 ){` |
|     15 |  919 | `			*pz += n;` |
|     15 |  920 | `			return k + 1;` |
|      - |  921 | `		}` |
|      5 |  922 | `	}` |
|    ! 0 |  923 | `	return 0;` |
|      8 |  924 | `}` |
|      - |  925 | `/*` |
|      - |  926 | ` * php's DateTime::createFromFormat engine.` |
|      - |  927 | ` *` |
|      - |  928 | `` * This was the `__dt_from_format()` thunk, whose answer had to survive a trip`` |
|      - |  929 | ` * through PHP: an ARRAY on success and a "COUNT\nPOS\tMESSAGE" string on failure,` |
|      - |  930 | ` * which the chunk then re-parsed. Both encodings are gone — the native methods call` |
|      - |  931 | ` * this directly and read the diagnostics as a struct. That is also why a parse that` |
|      - |  932 | ` * has BOTH errors and warnings can now report both: the failure encoding had no room` |
|      - |  933 | `` * for warnings, so php's `warning_count` was silently 0 whenever an error was present.`` |
|      - |  934 | ` *` |
|      - |  935 | ` * Returns 0 when the parse produced a time and non-zero when it did not; pOut->sDiag` |
|      - |  936 | ` * carries the warnings/errors either way (offKind: 0 none parsed, 1 numeric offset,` |
|      - |  937 | ` * 2 literal Z, 3 named identifier).` |
|      - |  938 | ` */` |
|      - |  939 | `/*` |
|      - |  940 | ` * Publish one scan's warnings and errors as the record getLastErrors() answers.` |
|      - |  941 | ` * The messages are static literals, so the record copies pointers, never bytes.` |
|      - |  942 | ` */` |
|     68 |  943 | `static void DtFfDiag(phl_dt_lasterr *pDiag,int nErr,int nErrKept,const int *aErrPos,` |
|      - |  944 | `	const char **azErr,int nWarn,const int *aWarnPos,const char **azWarn)` |
|      1 |  945 | `{` |
|      - |  946 | `	int k;` |
|     69 |  947 | `	pDiag->bSet = (sxu8)(nErr > 0 \|\| nWarn > 0);` |
|     69 |  948 | `	pDiag->nErr = nErr;` |
|     69 |  949 | `	pDiag->nErrKept = nErrKept;` |
|     89 |  950 | `	for( k = 0 ; k < nErrKept ; k++ ){` |
|     21 |  951 | `		pDiag->aErrPos[k] = aErrPos[k];` |
|     21 |  952 | `		pDiag->azErr[k] = azErr[k];` |
|     11 |  953 | `	}` |
|     69 |  954 | `	pDiag->nWarn = nWarn;` |
|     69 |  955 | `	pDiag->nWarnKept = nWarn;` |
|     75 |  956 | `	for( k = 0 ; k < nWarn ; k++ ){` |
|      7 |  957 | `		pDiag->aWarnPos[k] = aWarnPos[k];` |
|      7 |  958 | `		pDiag->azWarn[k] = azWarn[k];` |
|      4 |  959 | `	}` |
|     69 |  960 | `}` |
|      - |  961 | `typedef struct dt_ff_res dt_ff_res;` |
|      - |  962 | `struct dt_ff_res` |
|      - |  963 | `{` |
|      - |  964 | `	sxi64 iTs;` |
|      - |  965 | `	sxi32 iOff;` |
|      - |  966 | `	int iOffKind;` |
|      - |  967 | `	char zName[16];` |
|      - |  968 | `	int uSec;` |
|      - |  969 | `	int bHasUs;` |
|      - |  970 | `	phl_dt_lasterr sDiag;` |
|      - |  971 | `};` |
|     68 |  972 | `static int DtFromFormat(const char *zFmt,int nFmt,const char *zIn,int nIn,` |
|      - |  973 | `	sxi64 iNow,sxi32 iDefOff,dt_ff_res *pOut)` |
|      1 |  974 | `{` |
|      - |  975 | `	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};` |
|      - |  976 | `	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",` |
|      - |  977 | `		"thursday","friday","saturday"};` |
|      - |  978 | `	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",` |
|      - |  979 | `		"aug","sep","oct","nov","dec"};` |
|      - |  980 | `	static const char *azMonFull[] = {"january","february","march","april",` |
|      - |  981 | `		"may","june","july","august","september","october","november","december"};` |
|      - |  982 | `	const char *zEnd,*zInEnd,*z;` |
|      - |  983 | `	sxi64 v;` |
|      - |  984 | `	/* -1 == unset */` |
|     69 |  985 | `	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;` |
|     69 |  986 | `	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;` |
|     69 |  987 | `	int uSecFF = 0,bHasUs = 0;` |
|     69 |  988 | `	int iOffKind = 0;` |
|     69 |  989 | `	sxi32 iOffVal = 0;` |
|      - |  990 | `	char zName[16];` |
|     69 |  991 | `	const char *zErr = 0;` |
|      - |  992 | `	const char *aWarnMsg[PH7_DT_MAX_WARN];` |
|      - |  993 | `	int aWarnPos[PH7_DT_MAX_WARN];` |
|     69 |  994 | `	int nWarn = 0,bAborted = 0;` |
|      - |  995 | `	const char *aErrMsg[PH7_DT_MAX_ERR];` |
|      - |  996 | `	int aErrPos[PH7_DT_MAX_ERR];` |
|     69 |  997 | `	int nErr = 0,nErrKept = 0;` |
|     69 |  998 | `	SyZero(pOut,sizeof(*pOut));` |
|     69 |  999 | `	zEnd = &zFmt[nFmt];` |
|     69 | 1000 | `	zInEnd = &zIn[nIn];` |
|     69 | 1001 | `	z = zIn;` |
|     69 | 1002 | `	zName[0] = 0;` |
|      - | 1003 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|      - | 1004 | `	{ int _p = (iPos),_k,_f = -1; \` |
|      - | 1005 | `	  nErr++; \` |
|      - | 1006 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|      - | 1007 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|      - | 1008 | `	  else if( nErrKept < PH7_DT_MAX_ERR ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|    455 | 1009 | `	while( zFmt < zEnd ){` |
|    395 | 1010 | `		char c = zFmt[0];` |
|    395 | 1011 | `		zFmt++;` |
|    395 | 1012 | `		zErr = 0;` |
|    395 | 1013 | `		if( c == '!' ){` |
|     11 | 1014 | `			y = 1970; mo = 1; d = 1; h = 0; mi = 0; s = 0;` |
|     11 | 1015 | `			h12 = -1; iMeridiem = -1;` |
|     11 | 1016 | `			continue;` |
|      - | 1017 | `		}` |
|    385 | 1018 | `		if( c == '\|' ){ bPipe = 1; continue; }` |
|    381 | 1019 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|    379 | 1020 | `		if( z >= zInEnd ){` |
|      - | 1021 | `			/* timelib aborts the scan once input is exhausted */` |
|     17 | 1022 | `			DT_FF_LOGERR(nIn,"Not enough data available to satisfy format");` |
|      9 | 1023 | `			break;` |
|      - | 1024 | `		}` |
|    371 | 1025 | `		switch( c ){` |
|     24 | 1026 | `		case 'd': case 'j':` |
|     49 | 1027 | `			if( !DtEatDigits(&z,zInEnd,1,2,&d) ){` |
|    ! 0 | 1028 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit day could not be found");` |
|    ! 0 | 1029 | `				if( DtHuntDigits(&z,zInEnd,1,2,&d) < 0 ){` |
|    ! 0 | 1030 | `					DT_FF_LOGERR(nIn,"A two digit day could not be found");` |
|    ! 0 | 1031 | `				}` |
|    ! 0 | 1032 | `			}` |
|     49 | 1033 | `			break;` |
|      1 | 1034 | `		case 'D':` |
|      3 | 1035 | `			if( !DtEatName(&z,zInEnd,azDay3,7) ){` |
|    ! 0 | 1036 | `				zErr = "A textual day could not be found";` |
|    ! 0 | 1037 | `			}` |
|      3 | 1038 | `			break;` |
|      1 | 1039 | `		case 'l':` |
|      3 | 1040 | `			if( !DtEatName(&z,zInEnd,azDayFull,7) ){` |
|    ! 0 | 1041 | `				zErr = "A textual day could not be found";` |
|    ! 0 | 1042 | `			}` |
|      3 | 1043 | `			break;` |
|      1 | 1044 | `		case 'S':` |
|      - | 1045 | `			/* ordinal suffix: st nd rd th */` |
|      4 | 1046 | `			if( zInEnd-z >= 2 && ((z[0]=='s'&&z[1]=='t')\|\|(z[0]=='n'&&z[1]=='d')` |
|      2 | 1047 | `			 \|\|(z[0]=='r'&&z[1]=='d')\|\|(z[0]=='t'&&z[1]=='h')) ){` |
|      3 | 1048 | `				z += 2;` |
|      1 | 1049 | `			}` |
|      3 | 1050 | `			break;` |
|     22 | 1051 | `		case 'm': case 'n':` |
|     45 | 1052 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mo) ){` |
|    ! 0 | 1053 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit month could not be found");` |
|    ! 0 | 1054 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mo) < 0 ){` |
|    ! 0 | 1055 | `					DT_FF_LOGERR(nIn,"A two digit month could not be found");` |
|    ! 0 | 1056 | `				}` |
|    ! 0 | 1057 | `			}` |
|     45 | 1058 | `			break;` |
|      1 | 1059 | `		case 'M':{` |
|      3 | 1060 | `			int k = DtEatName(&z,zInEnd,azMon3,12);` |
|      3 | 1061 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|      3 | 1062 | `			break;` |
|      - | 1063 | `				 }` |
|      1 | 1064 | `		case 'F':{` |
|      3 | 1065 | `			int k = DtEatName(&z,zInEnd,azMonFull,12);` |
|      3 | 1066 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|      3 | 1067 | `			break;` |
|      - | 1068 | `				 }` |
|    ! 0 | 1069 | `		case 'y':` |
|    ! 0 | 1070 | `			if( DtEatDigits(&z,zInEnd,2,2,&y) ){` |
|    ! 0 | 1071 | `				y += (y <= 69) ? 2000 : 1900;` |
|    ! 0 | 1072 | `			}else{` |
|    ! 0 | 1073 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit year could not be found");` |
|    ! 0 | 1074 | `				if( DtHuntDigits(&z,zInEnd,2,2,&y) < 0 ){` |
|    ! 0 | 1075 | `					DT_FF_LOGERR(nIn,"A two digit year could not be found");` |
|    ! 0 | 1076 | `				}else if( y >= 0 ){` |
|    ! 0 | 1077 | `					y += (y <= 69) ? 2000 : 1900;` |
|    ! 0 | 1078 | `				}` |
|      - | 1079 | `			}` |
|    ! 0 | 1080 | `			break;` |
|     29 | 1081 | `		case 'Y':{` |
|     59 | 1082 | `			int neg = 0;` |
|     59 | 1083 | `			if( z < zInEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|     59 | 1084 | `			if( DtEatDigits(&z,zInEnd,1,4,&y) ){` |
|     51 | 1085 | `				if( neg ){ y = -y; }` |
|     26 | 1086 | `			}else{` |
|      9 | 1087 | `				DT_FF_LOGERR((int)(z - zIn),"A four digit year could not be found");` |
|      9 | 1088 | `				if( DtHuntDigits(&z,zInEnd,1,4,&y) < 0 ){` |
|     17 | 1089 | `					DT_FF_LOGERR(nIn,"A four digit year could not be found");` |
|      4 | 1090 | `				}` |
|      - | 1091 | `			}` |
|     59 | 1092 | `			break;` |
|      - | 1093 | `				 }` |
|      7 | 1094 | `		case 'H': case 'G':` |
|     15 | 1095 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h) ){` |
|    ! 0 | 1096 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|    ! 0 | 1097 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h) < 0 ){` |
|    ! 0 | 1098 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|    ! 0 | 1099 | `				}` |
|    ! 0 | 1100 | `			}` |
|     15 | 1101 | `			break;` |
|      2 | 1102 | `		case 'h': case 'g':` |
|      5 | 1103 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h12) ){` |
|    ! 0 | 1104 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|    ! 0 | 1105 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h12) < 0 ){` |
|    ! 0 | 1106 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|    ! 0 | 1107 | `				}` |
|    ! 0 | 1108 | `			}` |
|      5 | 1109 | `			break;` |
|      9 | 1110 | `		case 'i':` |
|     19 | 1111 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mi) ){` |
|    ! 0 | 1112 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit minute could not be found");` |
|    ! 0 | 1113 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mi) < 0 ){` |
|    ! 0 | 1114 | `					DT_FF_LOGERR(nIn,"A two digit minute could not be found");` |
|    ! 0 | 1115 | `				}` |
|    ! 0 | 1116 | `			}` |
|     19 | 1117 | `			break;` |
|      3 | 1118 | `		case 's':` |
|      7 | 1119 | `			if( !DtEatDigits(&z,zInEnd,1,2,&s) ){` |
|    ! 0 | 1120 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit second could not be found");` |
|    ! 0 | 1121 | `				if( DtHuntDigits(&z,zInEnd,1,2,&s) < 0 ){` |
|    ! 0 | 1122 | `					DT_FF_LOGERR(nIn,"A two digit second could not be found");` |
|    ! 0 | 1123 | `				}` |
|    ! 0 | 1124 | `			}` |
|      7 | 1125 | `			break;` |
|      1 | 1126 | `		case 'u':{` |
|      - | 1127 | `			/* Microseconds: the digits parsed are right-padded to 6 (".5" -> 500000). */` |
|      3 | 1128 | `			const char *zStart = z;` |
|      3 | 1129 | `			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){` |
|    ! 0 | 1130 | `				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");` |
|    ! 0 | 1131 | `				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){` |
|    ! 0 | 1132 | `					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");` |
|    ! 0 | 1133 | `				}else{` |
|    ! 0 | 1134 | `					zStart = z; /* HuntDigits repositioned; treat as freshly read */` |
|      - | 1135 | `				}` |
|    ! 0 | 1136 | `			}` |
|      - | 1137 | `			{` |
|      3 | 1138 | `				int nd = (int)(z - zStart);` |
|      3 | 1139 | `				while( nd > 0 && nd < 6 ){ v *= 10; nd++; }` |
|      3 | 1140 | `				uSecFF = (int)v; bHasUs = 1;` |
|      - | 1141 | `			}` |
|      3 | 1142 | `			break;` |
|      - | 1143 | `				 }` |
|    ! 0 | 1144 | `		case 'v':{` |
|    ! 0 | 1145 | `			const char *zStart = z;` |
|    ! 0 | 1146 | `			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){` |
|    ! 0 | 1147 | `				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");` |
|    ! 0 | 1148 | `				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){` |
|    ! 0 | 1149 | `					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");` |
|    ! 0 | 1150 | `				}else{` |
|    ! 0 | 1151 | `					zStart = z;` |
|      - | 1152 | `				}` |
|    ! 0 | 1153 | `			}` |
|      - | 1154 | `			{` |
|    ! 0 | 1155 | `				int nd = (int)(z - zStart);` |
|    ! 0 | 1156 | `				while( nd > 0 && nd < 3 ){ v *= 10; nd++; }` |
|    ! 0 | 1157 | `				uSecFF = (int)v * 1000; bHasUs = 1; /* ms -> us */` |
|      - | 1158 | `			}` |
|    ! 0 | 1159 | `			break;` |
|      - | 1160 | `				 }` |
|      2 | 1161 | `		case 'a': case 'A':{` |
|      - | 1162 | `			static const char *azMer[] = {"am","pm","a.m.","p.m."};` |
|      5 | 1163 | `			int k = DtEatName(&z,zInEnd,azMer,4);` |
|      5 | 1164 | `			if( k ){` |
|      5 | 1165 | `				iMeridiem = ((k - 1) & 1);` |
|      3 | 1166 | `			}else{` |
|    ! 0 | 1167 | `				zErr = "A meridian could not be found";` |
|      - | 1168 | `			}` |
|      5 | 1169 | `			break;` |
|      - | 1170 | `				 }` |
|      2 | 1171 | `		case 'U':{` |
|      5 | 1172 | `			int neg = 0;` |
|      5 | 1173 | `			if( z < zInEnd && z[0]=='-' ){ neg = 1; z++; }` |
|      5 | 1174 | `			if( DtEatDigits(&z,zInEnd,1,19,&uVal) ){` |
|      5 | 1175 | `				if( neg ){ uVal = -uVal; }` |
|      5 | 1176 | `				bHasU = 1;` |
|      3 | 1177 | `			}else{` |
|    ! 0 | 1178 | `				DT_FF_LOGERR((int)(z - zIn),"A unix timestamp could not be found");` |
|    ! 0 | 1179 | `				if( DtHuntDigits(&z,zInEnd,1,19,&uVal) < 0 ){` |
|    ! 0 | 1180 | `					DT_FF_LOGERR(nIn,"A unix timestamp could not be found");` |
|    ! 0 | 1181 | `				}else{` |
|    ! 0 | 1182 | `					if( neg ){ uVal = -uVal; }` |
|    ! 0 | 1183 | `					bHasU = 1;` |
|      - | 1184 | `				}` |
|      - | 1185 | `			}` |
|      5 | 1186 | `			break;` |
|      - | 1187 | `				 }` |
|      1 | 1188 | `		case 'e': case 'T':{` |
|      - | 1189 | `			static const char *azZone[] = {"UTC","GMT","Z"};` |
|      3 | 1190 | `			int k = DtEatName(&z,zInEnd,azZone,3);` |
|      3 | 1191 | `			if( k == 3 ){` |
|    ! 0 | 1192 | `				iOffKind = 2; iOffVal = 0;` |
|      3 | 1193 | `			}else if( k ){` |
|      3 | 1194 | `				iOffKind = 3; iOffVal = 0;` |
|      3 | 1195 | `				SyMemcpy(azZone[k-1],zName,4);` |
|      1 | 1196 | `			}else if( z < zInEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|    ! 0 | 1197 | `				goto parse_num_off;` |
|    ! 0 | 1198 | `			}else{` |
|    ! 0 | 1199 | `				zErr = "The timezone could not be found in the database";` |
|      - | 1200 | `			}` |
|      3 | 1201 | `			break;` |
|      2 | 1202 | `				 }` |
|      - | 1203 | `		case 'O': case 'P':` |
|      2 | 1204 | `parse_num_off:	{` |
|      5 | 1205 | `			int sign,oh,om = 0;` |
|      - | 1206 | `			sxi64 t;` |
|      5 | 1207 | `			if( z >= zInEnd \|\| (z[0] != '+' && z[0] != '-') ){` |
|    ! 0 | 1208 | `				zErr = "The timezone could not be found in the database";` |
|    ! 0 | 1209 | `				break;` |
|      - | 1210 | `			}` |
|      5 | 1211 | `			sign = (z[0]=='-') ? -1 : 1;` |
|      5 | 1212 | `			z++;` |
|      5 | 1213 | `			if( !DtEatDigits(&z,zInEnd,2,2,&t) ){` |
|    ! 0 | 1214 | `				zErr = "The timezone could not be found in the database";` |
|    ! 0 | 1215 | `				break;` |
|      - | 1216 | `			}` |
|      5 | 1217 | `			oh = (int)t;` |
|      5 | 1218 | `			if( z < zInEnd && z[0]==':' ){ z++; }` |
|      5 | 1219 | `			if( DtEatDigits(&z,zInEnd,2,2,&t) ){ om = (int)t; }` |
|      5 | 1220 | `			iOffKind = 1;` |
|      5 | 1221 | `			iOffVal = sign * (oh*3600 + om*60);` |
|      5 | 1222 | `			break;` |
|      - | 1223 | `				 }` |
|    ! 0 | 1224 | `		case '?':` |
|    ! 0 | 1225 | `			if( z < zInEnd ){ z++; }` |
|    ! 0 | 1226 | `			break;` |
|    ! 0 | 1227 | `		case '*':` |
|      - | 1228 | `			/* skip input until the next separator byte */` |
|    ! 0 | 1229 | `			while( z < zInEnd && !SyisDigit(z[0]) && z[0] != ';' && z[0] != ':'` |
|    ! 0 | 1230 | `			 && z[0] != '/' && z[0] != '.' && z[0] != ',' && z[0] != '-'` |
|    ! 0 | 1231 | `			 && z[0] != '(' && z[0] != ')' && z[0] != ' ' ){` |
|    ! 0 | 1232 | `				z++;` |
|    ! 0 | 1233 | `			}` |
|    ! 0 | 1234 | `			break;` |
|      1 | 1235 | `		case '#':` |
|      3 | 1236 | `			if( z < zInEnd && (z[0]==';'\|\|z[0]==':'\|\|z[0]=='/'\|\|z[0]=='.'` |
|    ! 0 | 1237 | `			 \|\|z[0]==','\|\|z[0]=='-'\|\|z[0]=='('\|\|z[0]==')') ){` |
|      3 | 1238 | `				z++;` |
|      2 | 1239 | `			}else{` |
|    ! 0 | 1240 | `				zErr = "The separation symbol could not be found";` |
|      - | 1241 | `			}` |
|      3 | 1242 | `			break;` |
|      1 | 1243 | `		case '\\':` |
|      3 | 1244 | `			if( zFmt < zEnd ){` |
|      3 | 1245 | `				if( z < zInEnd && z[0] == zFmt[0] ){` |
|      3 | 1246 | `					z++;` |
|      3 | 1247 | `					zFmt++;` |
|      2 | 1248 | `				}else{` |
|      - | 1249 | `					/* a literal mismatch aborts timelib's scan */` |
|    ! 0 | 1250 | `					DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|    ! 0 | 1251 | `					zFmt = zEnd;` |
|    ! 0 | 1252 | `					bAborted = 1;` |
|      - | 1253 | `				}` |
|      1 | 1254 | `			}` |
|      3 | 1255 | `			break;` |
|     57 | 1256 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|      - | 1257 | `		case '(' : case ')':` |
|    115 | 1258 | `			if( z < zInEnd && z[0] == c ){` |
|    115 | 1259 | `				z++;` |
|     58 | 1260 | `			}else{` |
|      - | 1261 | `				/* timelib logs BOTH messages (count +2, last-wins on the` |
|      - | 1262 | `				 * position), consumes the offending byte, and keeps going */` |
|    ! 0 | 1263 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|    ! 0 | 1264 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|    ! 0 | 1265 | `				z++;` |
|      - | 1266 | `			}` |
|    115 | 1267 | `			break;` |
|     16 | 1268 | `		case ' ':` |
|     33 | 1269 | `			if( z < zInEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|     33 | 1270 | `				z++;` |
|     17 | 1271 | `			}else{` |
|    ! 0 | 1272 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|    ! 0 | 1273 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|    ! 0 | 1274 | `				z++;` |
|      - | 1275 | `			}` |
|     33 | 1276 | `			break;` |
|      1 | 1277 | `		default:` |
|      - | 1278 | `			/* any other format byte must match the input verbatim; a mismatch` |
|      - | 1279 | `			 * aborts timelib's scan */` |
|      3 | 1280 | `			if( z < zInEnd && z[0] == c ){` |
|    ! 0 | 1281 | `				z++;` |
|    ! 0 | 1282 | `			}else{` |
|      3 | 1283 | `				DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|      3 | 1284 | `				zFmt = zEnd;` |
|      3 | 1285 | `				bAborted = 1;` |
|      - | 1286 | `			}` |
|      2 | 1287 | `			break;` |
|      - | 1288 | `		}` |
|    371 | 1289 | `		if( zErr ){` |
|      - | 1290 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|    ! 0 | 1291 | `			DT_FF_LOGERR((int)(z - zIn),zErr);` |
|    ! 0 | 1292 | `		}` |
|      1 | 1293 | `	}` |
|     69 | 1294 | `	if( z < zInEnd && !bAborted ){` |
|      5 | 1295 | `		if( bPlus ){` |
|      - | 1296 | `			/* '+' downgrades trailing data to a warning */` |
|      3 | 1297 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|      3 | 1298 | `			aWarnMsg[nWarn] = "Trailing data";` |
|      3 | 1299 | `			nWarn++;` |
|      2 | 1300 | `		}else{` |
|      3 | 1301 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|      - | 1302 | `		}` |
|      2 | 1303 | `	}` |
|     69 | 1304 | `	if( nErr > 0 ){` |
|      - | 1305 | `		/* The diagnostics are the caller's on this path too: php reports the` |
|      - | 1306 | `		 * warnings of a parse that ALSO failed, which the old string encoding had` |
|      - | 1307 | `		 * no room for. */` |
|     13 | 1308 | `		DtFfDiag(&pOut->sDiag,nErr,nErrKept,aErrPos,aErrMsg,nWarn,aWarnPos,aWarnMsg);` |
|     13 | 1309 | `		return -1;` |
|      - | 1310 | `	}` |
|     57 | 1311 | `	if( bPipe ){` |
|      5 | 1312 | `		if( y < 0 ){ y = 1970; }` |
|      5 | 1313 | `		if( mo < 0 ){ mo = 1; }` |
|      5 | 1314 | `		if( d < 0 ){ d = 1; }` |
|      5 | 1315 | `		if( h < 0 && h12 < 0 ){ h = 0; }` |
|      5 | 1316 | `		if( mi < 0 ){ mi = 0; }` |
|      5 | 1317 | `		if( s < 0 ){ s = 0; }` |
|      2 | 1318 | `	}` |
|      - | 1319 | `	{` |
|      - | 1320 | `		/* remaining unset fields come from "now" in the default offset */` |
|     57 | 1321 | `		sxi64 iLocal = iNow + iDefOff;` |
|     57 | 1322 | `		sxi64 days = DtFloorDiv(iLocal,86400);` |
|     57 | 1323 | `		sxi64 secs = iLocal - days*86400;` |
|      - | 1324 | `		sxi64 ny;` |
|      - | 1325 | `		int nmo,nd;` |
|     57 | 1326 | `		DtCivilFromDays(days,&ny,&nmo,&nd);` |
|     57 | 1327 | `		if( y < 0 ){ y = ny; }` |
|     57 | 1328 | `		if( mo < 0 ){ mo = nmo; }` |
|     57 | 1329 | `		if( d < 0 ){ d = nd; }` |
|     57 | 1330 | `		if( h12 >= 0 ){` |
|      5 | 1331 | `			h = (h12 % 12) + ((iMeridiem == 1) ? 12 : 0);` |
|      2 | 1332 | `		}` |
|      - | 1333 | `		/* php: parsing a time component zeroes the finer unset units */` |
|     57 | 1334 | `		if( h >= 0 ){` |
|     27 | 1335 | `			if( mi < 0 ){ mi = 0; }` |
|     27 | 1336 | `			if( s < 0 ){ s = 0; }` |
|     44 | 1337 | `		}else if( mi >= 0 ){` |
|      3 | 1338 | `			if( s < 0 ){ s = 0; }` |
|      1 | 1339 | `		}` |
|     57 | 1340 | `		if( h < 0 ){ h = secs / 3600; }` |
|     57 | 1341 | `		if( mi < 0 ){ mi = (secs / 60) % 60; }` |
|     57 | 1342 | `		if( s < 0 ){ s = secs % 60; }` |
|      - | 1343 | `	}` |
|      - | 1344 | `	/* php validates the RESOLVED fields and warns (parse still succeeds,` |
|      - | 1345 | `	 * values roll over via civil arithmetic) */` |
|     57 | 1346 | `	if( mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo) ){` |
|      3 | 1347 | `		if( nWarn < PH7_DT_MAX_WARN ){` |
|      3 | 1348 | `			aWarnPos[nWarn] = nIn;` |
|      3 | 1349 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|      3 | 1350 | `			nWarn++;` |
|      1 | 1351 | `		}` |
|      1 | 1352 | `	}` |
|     57 | 1353 | `	if( h > 24 \|\| mi > 59 \|\| s > 59 ){` |
|      3 | 1354 | `		if( nWarn < PH7_DT_MAX_WARN ){` |
|      3 | 1355 | `			aWarnPos[nWarn] = nIn;` |
|      3 | 1356 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|      3 | 1357 | `			nWarn++;` |
|      1 | 1358 | `		}` |
|      1 | 1359 | `	}` |
|      - | 1360 | `	{` |
|     57 | 1361 | `		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;` |
|     57 | 1362 | `		if( bHasU ){` |
|      5 | 1363 | `			pOut->iTs = uVal;` |
|      5 | 1364 | `			iUseOff = 0;` |
|      5 | 1365 | `			iOffKind = 1;` |
|      3 | 1366 | `		}else{` |
|     53 | 1367 | `			pOut->iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);` |
|      - | 1368 | `		}` |
|     57 | 1369 | `		pOut->iOff = iUseOff;` |
|     57 | 1370 | `		pOut->iOffKind = iOffKind;` |
|     57 | 1371 | `		SyMemcpy(zName,pOut->zName,sizeof(pOut->zName));` |
|     57 | 1372 | `		pOut->zName[sizeof(pOut->zName)-1] = 0;` |
|     57 | 1373 | `		pOut->uSec = uSecFF;` |
|     57 | 1374 | `		pOut->bHasUs = bHasUs;` |
|      - | 1375 | `	}` |
|     57 | 1376 | `	DtFfDiag(&pOut->sDiag,nErr,nErrKept,aErrPos,aErrMsg,nWarn,aWarnPos,aWarnMsg);` |
|     57 | 1377 | `	return 0;` |
|     35 | 1378 | `}` |
|      - | 1379 | `/*` |
|      - | 1380 | ` * ---------------------------------------------------------------------------` |
|      - | 1381 | ` * DateTimeZone, DateTime and DateTimeImmutable, declared from C.` |
|      - | 1382 | ` *` |
|      - | 1383 | `` * These three used to be embedded PHP over nine global `__dt_*` thunks, with a`` |
|      - | 1384 | `` * private `trait __DtCoreT` holding the state and the shared half of both date`` |
|      - | 1385 | ` * classes. Every operation therefore crossed C -> PHP -> C and marshalled its` |
|      - | 1386 | ` * answer through a throwaway PHP array. The bodies below call the same routines` |
|      - | 1387 | ` * directly; the thunks, the trait and their chunk classes are gone.` |
|      - | 1388 | ` *` |
|      - | 1389 | `` * The instance state is unchanged, so `clone`, `serialize` and `var_dump` see what`` |
|      - | 1390 | `` * they always saw (minus the `__DtCoreT` declaring-class name): four private slots`` |
|      - | 1391 | ` * on each date class, two on DateTimeZone. Native traits do not exist, so the` |
|      - | 1392 | `` * shared method table is simply installed on both classes -- which is what `use`` |
|      - | 1393 | `` * __DtCoreT` did anyway.`` |
|      - | 1394 | ` * ---------------------------------------------------------------------------` |
|      - | 1395 | ` */` |
|      - | 1396 | `#define DT_TS    "__dtTs"` |
|      - | 1397 | `#define DT_OFF   "__dtOff"` |
|      - | 1398 | `#define DT_NAME  "__dtName"` |
|      - | 1399 | `#define DT_US    "__dtUs"` |
|      - | 1400 | `#define DTZ_OFF  "__dtzOff"` |
|      - | 1401 | `#define DTZ_NAME "__dtzName"` |
|      - | 1402 | `/* One date object's state, as the bodies below pass it around. */` |
|      - | 1403 | `typedef struct dt_state dt_state;` |
|      - | 1404 | `struct dt_state` |
|      - | 1405 | `{` |
|      - | 1406 | `	sxi64 iTs;` |
|      - | 1407 | `	sxi32 iOff;` |
|      - | 1408 | `	int uSec;` |
|      - | 1409 | `	const char *zName;   /* borrowed from the instance's own slot */` |
|      - | 1410 | `	int nName;` |
|      - | 1411 | `};` |
|      - | 1412 | `/* php's name for a fixed offset: "+HH:MM" (and "+00:00" for zero, never "-00:00"). */` |
|     76 | 1413 | `static int DtOffName(char *zBuf,sxu32 nBuf,sxi32 iOff)` |
|      1 | 1414 | `{` |
|     77 | 1415 | `	sxi32 a = iOff < 0 ? -iOff : iOff;` |
|    115 | 1416 | `	return (int)SyBufferFormat(zBuf,nBuf,"%c%02d:%02d",` |
|     76 | 1417 | `		iOff < 0 ? '-' : '+',(int)(a / 3600),(int)((a % 3600) / 60));` |
|      1 | 1418 | `}` |
|    300 | 1419 | `static void DtLoad(ph7_class_instance *pObj,dt_state *pOut)` |
|      1 | 1420 | `{` |
|    301 | 1421 | `	pOut->iTs  = PH7_NativeAttrInt(pObj,DT_TS);` |
|    301 | 1422 | `	pOut->iOff = (sxi32)PH7_NativeAttrInt(pObj,DT_OFF);` |
|    301 | 1423 | `	pOut->uSec = (int)PH7_NativeAttrInt(pObj,DT_US);` |
|    301 | 1424 | `	PH7_NativeAttrStr(pObj,DT_NAME,&pOut->zName,&pOut->nName);` |
|    301 | 1425 | `}` |
|    310 | 1426 | `static void DtStore(ph7_vm *pVm,ph7_class_instance *pObj,const dt_state *pIn)` |
|      2 | 1427 | `{` |
|    312 | 1428 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_TS,pIn->iTs);` |
|    312 | 1429 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_OFF,pIn->iOff);` |
|    312 | 1430 | `	PH7_NativeSetAttrInt(pVm,pObj,DT_US,pIn->uSec);` |
|    312 | 1431 | `	PH7_NativeSetAttrStr(pVm,pObj,DT_NAME,pIn->zName,pIn->nName);` |
|    312 | 1432 | `}` |
|      - | 1433 | `/* The receiver of a native method, or NULL when the call has no object (which the` |
|      - | 1434 | ` * dispatcher only allows for a static one). */` |
|    950 | 1435 | `static ph7_class_instance * DtThis(ph7_context *pCtx)` |
|      2 | 1436 | `{` |
|    952 | 1437 | `	return PH7_ContextThis(pCtx);` |
|      2 | 1438 | `}` |
|   9558 | 1439 | `static ph7_class * DtClass(ph7_vm *pVm,const char *zName)` |
|      5 | 1440 | `{` |
|   9563 | 1441 | `	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);` |
|      5 | 1442 | `}` |
|      - | 1443 | `/* An immutable receiver mutates a COPY; a mutable one mutates itself. That is the` |
|      - | 1444 | ` * only difference between the two classes' method tables, so both share one body. */` |
|     54 | 1445 | `static int DtIsImmutable(ph7_vm *pVm,ph7_class_instance *pObj)` |
|      1 | 1446 | `{` |
|     55 | 1447 | `	ph7_class *pImm = DtClass(pVm,"DateTimeImmutable");` |
|     55 | 1448 | `	return pImm != 0 && PH7_VmInstanceOf(pObj->pClass,pImm);` |
|      1 | 1449 | `}` |
|      - | 1450 | `/*` |
|      - | 1451 | ` * The object a mutator writes: $this itself, or a clone for DateTimeImmutable.` |
|      - | 1452 | ` * Either way the caller returns it, so a mutable method answers the same object` |
|      - | 1453 | `` * php's does (`$d->modify(...) === $d`).`` |
|      - | 1454 | ` */` |
|     52 | 1455 | `static ph7_class_instance * DtMutTarget(ph7_context *pCtx,ph7_class_instance *pThis,int *pbCopy)` |
|      1 | 1456 | `{` |
|     53 | 1457 | `	if( DtIsImmutable(pCtx->pVm,pThis) ){` |
|     15 | 1458 | `		*pbCopy = 1;` |
|     15 | 1459 | `		return PH7_CloneClassInstance(pThis);` |
|      - | 1460 | `	}` |
|     39 | 1461 | `	*pbCopy = 0;` |
|     39 | 1462 | `	return pThis;` |
|     27 | 1463 | `}` |
|      - | 1464 | `/* Return a mutator's target the way php returns it: the clone (whose reference we` |
|      - | 1465 | ` * own) or the receiver itself (whose value the context already holds). */` |
|     52 | 1466 | `static void DtMutResult(ph7_context *pCtx,ph7_class_instance *pTarget,int bCopy)` |
|      1 | 1467 | `{` |
|     53 | 1468 | `	if( bCopy ){` |
|     15 | 1469 | `		PH7_NativeResultObject(pCtx,pTarget);` |
|      8 | 1470 | `	}else{` |
|     39 | 1471 | `		ph7_result_value(pCtx,PH7_ContextThisValue(pCtx));` |
|      - | 1472 | `	}` |
|     53 | 1473 | `}` |
|      - | 1474 | `/* Read a DateTimeZone argument's two slots. php's ext/date reads its own internal` |
|      - | 1475 | ` * timezone struct here, so an overridden getName()/getOffset() is ignored by both` |
|      - | 1476 | ` * engines. Answers 0 when the value is not a DateTimeZone at all. */` |
|    176 | 1477 | `static int DtZoneOf(ph7_value *pArg,sxi32 *piOff,const char **pzName,int *pnName)` |
|      1 | 1478 | `{` |
|      - | 1479 | `	ph7_class_instance *pObj;` |
|    177 | 1480 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 1481 | `		return 0;` |
|      - | 1482 | `	}` |
|    177 | 1483 | `	pObj = (ph7_class_instance *)pArg->x.pOther;` |
|    177 | 1484 | `	if( PH7_NativeAttr(pObj,DTZ_NAME) == 0 ){` |
|    ! 0 | 1485 | `		return 0;` |
|      - | 1486 | `	}` |
|    177 | 1487 | `	*piOff = (sxi32)PH7_NativeAttrInt(pObj,DTZ_OFF);` |
|    177 | 1488 | `	PH7_NativeAttrStr(pObj,DTZ_NAME,pzName,pnName);` |
|    177 | 1489 | `	return 1;` |
|     89 | 1490 | `}` |
|      - | 1491 | `/*` |
|      - | 1492 | ` * Record one parse's diagnostics as getLastErrors()'s answer.` |
|      - | 1493 | ` *` |
|      - | 1494 | ` * php resets the record on EVERY constructor and createFromFormat() call — a clean` |
|      - | 1495 | `` * parse answers `false` again — and a failing constructor publishes its reason as a`` |
|      - | 1496 | ` * one-entry error map before it throws.` |
|      - | 1497 | ` */` |
|   4912 | 1498 | `static void DtLastErrClear(ph7_vm *pVm)` |
|      5 | 1499 | `{` |
|   4917 | 1500 | `	SyZero(&pVm->sDtLastErr,sizeof(pVm->sDtLastErr));` |
|   4917 | 1501 | `}` |
|     30 | 1502 | `static void DtLastErrOne(ph7_vm *pVm,int iPos,const char *zMsg)` |
|      1 | 1503 | `{` |
|     31 | 1504 | `	DtLastErrClear(&(*pVm));` |
|     31 | 1505 | `	pVm->sDtLastErr.bSet = 1;` |
|     31 | 1506 | `	pVm->sDtLastErr.nErr = 1;` |
|     31 | 1507 | `	pVm->sDtLastErr.nErrKept = 1;` |
|     31 | 1508 | `	pVm->sDtLastErr.aErrPos[0] = iPos;` |
|     31 | 1509 | `	pVm->sDtLastErr.azErr[0] = zMsg;` |
|     31 | 1510 | `}` |
|      - | 1511 | `/*` |
|      - | 1512 | ` * Parse $datetime into a date object's state, php's constructor rules: an explicit` |
|      - | 1513 | ` * offset in the string wins over the $timezone argument, a literal "Z" keeps its` |
|      - | 1514 | ` * own name, and everything else takes the argument's (or the default) zone.` |
|      - | 1515 | ` * Returns 0 on success; on failure the caller throws with the reason and position` |
|      - | 1516 | ` * this reports.` |
|      - | 1517 | ` */` |
|    272 | 1518 | `static int DtInitState(ph7_context *pCtx,const char *zIn,int nIn,sxi32 iZoneOff,` |
|      - | 1519 | `	const char *zZoneName,int nZoneName,dt_state *pOut,char *zNameBuf,sxu32 nNameBuf,` |
|      - | 1520 | `	const char **pzErr,int *piPos,char *pcAt)` |
|      2 | 1521 | `{` |
|    274 | 1522 | `	sxi64 iTs = 0;` |
|    274 | 1523 | `	sxi32 iOff = 0;` |
|    274 | 1524 | `	int bOffSet = 0,uSec = 0,iErrPos;` |
|    274 | 1525 | `	iErrPos = DtParse(zIn,nIn,(sxi64)time(0),iZoneOff,&iTs,&iOff,&bOffSet,&uSec);` |
|    274 | 1526 | `	if( iErrPos != 0 ){` |
|     33 | 1527 | `		*pzErr = DtParseErr(zIn,nIn,iErrPos,piPos,pcAt);` |
|     33 | 1528 | `		return -1;` |
|      - | 1529 | `	}` |
|    242 | 1530 | `	pOut->iTs = iTs;` |
|    242 | 1531 | `	pOut->uSec = uSec;` |
|    242 | 1532 | `	if( bOffSet ){` |
|     41 | 1533 | `		pOut->iOff = iOff;` |
|     41 | 1534 | `		if( bOffSet == 2 ){` |
|      9 | 1535 | `			pOut->zName = "Z";` |
|      9 | 1536 | `			pOut->nName = 1;` |
|      5 | 1537 | `		}else{` |
|     33 | 1538 | `			pOut->nName = DtOffName(zNameBuf,nNameBuf,iOff);` |
|     33 | 1539 | `			pOut->zName = zNameBuf;` |
|      - | 1540 | `		}` |
|     21 | 1541 | `	}else{` |
|    202 | 1542 | `		pOut->iOff = iZoneOff;` |
|    202 | 1543 | `		pOut->zName = zZoneName;` |
|    202 | 1544 | `		pOut->nName = nZoneName;` |
|      - | 1545 | `	}` |
|    120 | 1546 | `	SXUNUSED(pCtx);` |
|    242 | 1547 | `	return 0;` |
|    138 | 1548 | `}` |
|      - | 1549 | `/*` |
|      - | 1550 | ` * The timezone spellings PHL understands with no tz database: UTC, GMT, Z and a` |
|      - | 1551 | ` * fixed [+-]HH:?MM offset. Shared by DateTimeZone::__construct(), which throws on` |
|      - | 1552 | ` * a miss, and timezone_open(), which warns and answers false.` |
|      - | 1553 | ` */` |
|    202 | 1554 | `static int DtZoneParse(const char *zTz,int nTz,sxi32 *piOff,const char **pzName,` |
|      - | 1555 | `	int *pnName,char *zBuf,sxu32 nBuf)` |
|      1 | 1556 | `{` |
|    203 | 1557 | `	if( nTz == 1 && zTz[0] == 'Z' ){` |
|    ! 0 | 1558 | `		*piOff = 0;` |
|    ! 0 | 1559 | `		*pzName = "Z";` |
|    ! 0 | 1560 | `		*pnName = 1;` |
|    ! 0 | 1561 | `		return 0;` |
|      - | 1562 | `	}` |
|    203 | 1563 | `	if( nTz == 3 && (SyStrnicmp(zTz,"UTC",3) == 0 \|\| SyStrnicmp(zTz,"GMT",3) == 0) ){` |
|      - | 1564 | `		/* php answers the canonical spelling, whatever case the caller used. */` |
|    163 | 1565 | `		*piOff = 0;` |
|    163 | 1566 | `		*pzName = (zTz[0] == 'u' \|\| zTz[0] == 'U') ? "UTC" : "GMT";` |
|    163 | 1567 | `		*pnName = 3;` |
|    163 | 1568 | `		return 0;` |
|      - | 1569 | `	}` |
|     40 | 1570 | `	if( (nTz == 6 \|\| nTz == 5) && (zTz[0] == '+' \|\| zTz[0] == '-')` |
|     36 | 1571 | `	 && SyisDigit(zTz[1]) && SyisDigit(zTz[2])` |
|     73 | 1572 | `	 && (nTz == 5 ? (SyisDigit(zTz[3]) && SyisDigit(zTz[4]))` |
|     36 | 1573 | `	              : (zTz[3] == ':' && SyisDigit(zTz[4]) && SyisDigit(zTz[5]))) ){` |
|     37 | 1574 | `		int h = (zTz[1] - '0') * 10 + (zTz[2] - '0');` |
|     19 | 1575 | `		int m = nTz == 5 ? (zTz[3] - '0') * 10 + (zTz[4] - '0')` |
|     36 | 1576 | `		                 : (zTz[4] - '0') * 10 + (zTz[5] - '0');` |
|     37 | 1577 | `		sxi32 iOff = h * 3600 + m * 60;` |
|     37 | 1578 | `		if( zTz[0] == '-' ){` |
|      5 | 1579 | `			iOff = -iOff;` |
|      2 | 1580 | `		}` |
|     37 | 1581 | `		*piOff = iOff;` |
|      - | 1582 | `		/* php normalizes the NAME through the offset, so "-00:00" is "+00:00". */` |
|     37 | 1583 | `		*pnName = DtOffName(zBuf,nBuf,iOff);` |
|     37 | 1584 | `		*pzName = zBuf;` |
|     37 | 1585 | `		return 0;` |
|      - | 1586 | `	}` |
|      5 | 1587 | `	return -1;` |
|    102 | 1588 | `}` |
|      - | 1589 | `/* DateTimeZone::__construct(string $timezone) */` |
|    170 | 1590 | `static int vm_builtin_DateTimeZone_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1591 | `{` |
|    171 | 1592 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1593 | `	const char *zTz,*zName;` |
|      - | 1594 | `	int nTz,nName;` |
|    171 | 1595 | `	sxi32 iOff = 0;` |
|      - | 1596 | `	char zBuf[16];` |
|    171 | 1597 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1598 | `		return PH7_OK;` |
|      - | 1599 | `	}` |
|    171 | 1600 | `	zTz = ph7_value_to_string(apArg[0],&nTz);` |
|    171 | 1601 | `	if( DtZoneParse(zTz,nTz,&iOff,&zName,&nName,zBuf,sizeof(zBuf)) != 0 ){` |
|      4 | 1602 | `		return PH7_VmThrowException(pCtx,"DateInvalidTimeZoneException",` |
|      1 | 1603 | `			"DateTimeZone::__construct(): Unknown or bad timezone (%.*s)",nTz,zTz);` |
|      - | 1604 | `	}` |
|    169 | 1605 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,DTZ_OFF,iOff);` |
|    169 | 1606 | `	PH7_NativeSetAttrStr(pCtx->pVm,pThis,DTZ_NAME,zName,nName);` |
|    169 | 1607 | `	return PH7_OK;` |
|     86 | 1608 | `}` |
|      - | 1609 | `/* DateTimeZone::getName() */` |
|     20 | 1610 | `static int vm_builtin_DateTimeZone_getName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1611 | `{` |
|     21 | 1612 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1613 | `	const char *zName;` |
|      - | 1614 | `	int nName;` |
|     10 | 1615 | `	SXUNUSED(nArg);` |
|     10 | 1616 | `	SXUNUSED(apArg);` |
|     21 | 1617 | `	if( pThis == 0 ){` |
|    ! 0 | 1618 | `		return PH7_OK;` |
|      - | 1619 | `	}` |
|     21 | 1620 | `	PH7_NativeAttrStr(pThis,DTZ_NAME,&zName,&nName);` |
|     21 | 1621 | `	ph7_result_string(pCtx,zName,nName);` |
|     21 | 1622 | `	return PH7_OK;` |
|     11 | 1623 | `}` |
|      - | 1624 | `/* DateTimeZone::getOffset(DateTimeInterface $datetime) */` |
|      2 | 1625 | `static int vm_builtin_DateTimeZone_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1626 | `{` |
|      3 | 1627 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      1 | 1628 | `	SXUNUSED(nArg);` |
|      1 | 1629 | `	SXUNUSED(apArg);` |
|      3 | 1630 | `	if( pThis == 0 ){` |
|    ! 0 | 1631 | `		return PH7_OK;` |
|      - | 1632 | `	}` |
|      - | 1633 | `	/* Fixed-offset zones only, so the instant does not change the answer. */` |
|      3 | 1634 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DTZ_OFF));` |
|      3 | 1635 | `	return PH7_OK;` |
|      2 | 1636 | `}` |
|      - | 1637 | `/* DateTime::__construct(string $datetime = 'now', ?DateTimeZone $timezone = null) */` |
|    214 | 1638 | `static int vm_builtin_DateTime_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1639 | `{` |
|    216 | 1640 | `	ph7_vm *pVm = pCtx->pVm;` |
|    216 | 1641 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|    216 | 1642 | `	const char *zIn = "now",*zZone;` |
|    216 | 1643 | `	int nIn = 3,nZone;` |
|    216 | 1644 | `	sxi32 iZoneOff = 0;` |
|      - | 1645 | `	dt_state sState;` |
|      - | 1646 | `	char zNameBuf[16];` |
|      - | 1647 | `	const char *zErr;` |
|      - | 1648 | `	int iPos;` |
|      - | 1649 | `	char cAt;` |
|    216 | 1650 | `	if( pThis == 0 ){` |
|    ! 0 | 1651 | `		return PH7_OK;` |
|      - | 1652 | `	}` |
|    216 | 1653 | `	zZone = pVm->zDefTz;` |
|    216 | 1654 | `	nZone = (int)pVm->nDefTz;` |
|    216 | 1655 | `	if( nArg > 0 ){` |
|    200 | 1656 | `		zIn = ph7_value_to_string(apArg[0],&nIn);` |
|     99 | 1657 | `	}` |
|    216 | 1658 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    131 | 1659 | `		DtZoneOf(apArg[1],&iZoneOff,&zZone,&nZone);` |
|     65 | 1660 | `	}` |
|    214 | 1661 | `	if( DtInitState(pCtx,zIn,nIn,iZoneOff,zZone,nZone,&sState,zNameBuf,sizeof(zNameBuf),` |
|    109 | 1662 | `		&zErr,&iPos,&cAt) != 0 ){` |
|      - | 1663 | `		/* php publishes the failure through getLastErrors() as well as throwing. */` |
|     27 | 1664 | `		DtLastErrOne(pVm,iPos,zErr);` |
|     40 | 1665 | `		return PH7_VmThrowException(pCtx,"DateMalformedStringException",` |
|      - | 1666 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|     13 | 1667 | `			nIn,zIn,iPos,cAt,zErr);` |
|      - | 1668 | `	}` |
|    190 | 1669 | `	DtLastErrClear(pVm);` |
|    190 | 1670 | `	DtStore(pVm,pThis,&sState);` |
|    190 | 1671 | `	return PH7_OK;` |
|    109 | 1672 | `}` |
|      - | 1673 | ``/* One date object's `format()`, shared with the date_format() alias. */`` |
|    236 | 1674 | `static void DtFormatOf(ph7_context *pCtx,ph7_class_instance *pObj,const char *zFmt,int nFmt)` |
|      1 | 1675 | `{` |
|      - | 1676 | `	dt_state sState;` |
|      - | 1677 | `	Sytm sTm;` |
|      - | 1678 | `	char zZone[64];` |
|      - | 1679 | `	int nName;` |
|    237 | 1680 | `	DtLoad(pObj,&sState);` |
|    237 | 1681 | `	nName = sState.nName;` |
|    237 | 1682 | `	if( nName >= (int)sizeof(zZone) ){` |
|    ! 0 | 1683 | `		nName = (int)sizeof(zZone) - 1;` |
|    ! 0 | 1684 | `	}` |
|    237 | 1685 | `	SyMemcpy(sState.zName,zZone,(sxu32)nName);` |
|    237 | 1686 | `	zZone[nName] = 0;` |
|    237 | 1687 | `	DtFillSytm(sState.iTs,sState.iOff,zZone,&sTm);` |
|    237 | 1688 | `	DateFormat(pCtx,zFmt,nFmt,&sTm,sState.uSec);` |
|    237 | 1689 | `}` |
|      - | 1690 | `/* DateTime::format(string $format) */` |
|    230 | 1691 | `static int vm_builtin_DateTime_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1692 | `{` |
|    231 | 1693 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1694 | `	const char *zFmt;` |
|      - | 1695 | `	int nFmt;` |
|    231 | 1696 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1697 | `		return PH7_OK;` |
|      - | 1698 | `	}` |
|    231 | 1699 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    231 | 1700 | `	DtFormatOf(pCtx,pThis,zFmt,nFmt);` |
|    231 | 1701 | `	return PH7_OK;` |
|    116 | 1702 | `}` |
|      - | 1703 | `/* DateTime::getTimestamp() / getMicrosecond() / getOffset() */` |
|     10 | 1704 | `static int vm_builtin_DateTime_getTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1705 | `{` |
|     11 | 1706 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      5 | 1707 | `	SXUNUSED(nArg);` |
|      5 | 1708 | `	SXUNUSED(apArg);` |
|     11 | 1709 | `	if( pThis ){` |
|     11 | 1710 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_TS));` |
|      5 | 1711 | `	}` |
|     11 | 1712 | `	return PH7_OK;` |
|      1 | 1713 | `}` |
|     12 | 1714 | `static int vm_builtin_DateTime_getMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1715 | `{` |
|     13 | 1716 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      6 | 1717 | `	SXUNUSED(nArg);` |
|      6 | 1718 | `	SXUNUSED(apArg);` |
|     13 | 1719 | `	if( pThis ){` |
|     13 | 1720 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_US));` |
|      6 | 1721 | `	}` |
|     13 | 1722 | `	return PH7_OK;` |
|      1 | 1723 | `}` |
|      4 | 1724 | `static int vm_builtin_DateTime_getOffset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1725 | `{` |
|      5 | 1726 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      2 | 1727 | `	SXUNUSED(nArg);` |
|      2 | 1728 | `	SXUNUSED(apArg);` |
|      5 | 1729 | `	if( pThis ){` |
|      5 | 1730 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,DT_OFF));` |
|      2 | 1731 | `	}` |
|      5 | 1732 | `	return PH7_OK;` |
|      1 | 1733 | `}` |
|      - | 1734 | `/* The zone object of a date, built from its stored name and offset, so an` |
|      - | 1735 | ` * identifier PHL stored but cannot re-parse still round-trips. Shared with the` |
|      - | 1736 | ` * date_timezone_get() alias. */` |
|     10 | 1737 | `static int DtTimezoneResult(ph7_context *pCtx,ph7_class_instance *pObj)` |
|      1 | 1738 | `{` |
|     11 | 1739 | `	ph7_vm *pVm = pCtx->pVm;` |
|     11 | 1740 | `	ph7_class *pZoneClass = DtClass(pVm,"DateTimeZone");` |
|      - | 1741 | `	ph7_class_instance *pZone;` |
|      - | 1742 | `	const char *zName;` |
|      - | 1743 | `	int nName;` |
|     11 | 1744 | `	if( pZoneClass == 0 ){` |
|    ! 0 | 1745 | `		return PH7_OK;` |
|      - | 1746 | `	}` |
|     11 | 1747 | `	pZone = PH7_NewClassInstance(pVm,pZoneClass);` |
|     11 | 1748 | `	if( pZone == 0 ){` |
|    ! 0 | 1749 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1750 | `	}` |
|     11 | 1751 | `	PH7_NativeAttrStr(pObj,DT_NAME,&zName,&nName);` |
|     11 | 1752 | `	PH7_NativeSetAttrInt(pVm,pZone,DTZ_OFF,PH7_NativeAttrInt(pObj,DT_OFF));` |
|     11 | 1753 | `	PH7_NativeSetAttrStr(pVm,pZone,DTZ_NAME,zName,nName);` |
|     11 | 1754 | `	PH7_NativeResultObject(pCtx,pZone);` |
|     11 | 1755 | `	return PH7_OK;` |
|      6 | 1756 | `}` |
|      - | 1757 | `/* DateTime::getTimezone() */` |
|      8 | 1758 | `static int vm_builtin_DateTime_getTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1759 | `{` |
|      9 | 1760 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      4 | 1761 | `	SXUNUSED(nArg);` |
|      4 | 1762 | `	SXUNUSED(apArg);` |
|      9 | 1763 | `	if( pThis == 0 ){` |
|    ! 0 | 1764 | `		return PH7_OK;` |
|      - | 1765 | `	}` |
|      9 | 1766 | `	return DtTimezoneResult(pCtx,pThis);` |
|      5 | 1767 | `}` |
|      - | 1768 | `/* The truth of a value, without converting the caller's copy of it. */` |
|      2 | 1769 | `static int DtValueTruth(ph7_vm *pVm,ph7_value *pVal)` |
|      1 | 1770 | `{` |
|      - | 1771 | `	ph7_value sTmp;` |
|      - | 1772 | `	int bRes;` |
|      3 | 1773 | `	PH7_MemObjInit(&(*pVm),&sTmp);` |
|      3 | 1774 | `	PH7_MemObjStore(pVal,&sTmp);` |
|      - | 1775 | `	/* PH7_MemObjToBool converts IN PLACE and returns a STATUS: the answer is in` |
|      - | 1776 | `	 * x.iVal (reading the return is a silent always-false). */` |
|      3 | 1777 | `	PH7_MemObjToBool(&sTmp);` |
|      3 | 1778 | `	bRes = sTmp.x.iVal != 0;` |
|      3 | 1779 | `	PH7_MemObjRelease(&sTmp);` |
|      3 | 1780 | `	return bRes;` |
|      1 | 1781 | `}` |
|      - | 1782 | `/* The DateInterval two dates differ by. Shared with the date_diff() alias. */` |
|     20 | 1783 | `static int DtDiffResult(ph7_context *pCtx,ph7_class_instance *pBase,` |
|      - | 1784 | `	ph7_class_instance *pTarget,int bAbsolute)` |
|      1 | 1785 | `{` |
|     21 | 1786 | `	ph7_vm *pVm = pCtx->pVm;` |
|     21 | 1787 | `	ph7_class *pIvClass = DtClass(pVm,"DateInterval");` |
|      - | 1788 | `	ph7_class_instance *pIv;` |
|      - | 1789 | `	dt_diff sDiff;` |
|     21 | 1790 | `	if( pIvClass == 0 ){` |
|    ! 0 | 1791 | `		return PH7_OK;` |
|      - | 1792 | `	}` |
|     31 | 1793 | `	DtCivilDiff(PH7_NativeAttrInt(pBase,DT_TS),(sxi32)PH7_NativeAttrInt(pBase,DT_OFF),` |
|     10 | 1794 | `		PH7_NativeAttrInt(pTarget,DT_TS),&sDiff);` |
|     21 | 1795 | `	pIv = PH7_NewClassInstance(pVm,pIvClass);` |
|     21 | 1796 | `	if( pIv == 0 ){` |
|    ! 0 | 1797 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1798 | `	}` |
|     21 | 1799 | `	PH7_NativeSetAttrInt(pVm,pIv,"y",sDiff.y);` |
|     21 | 1800 | `	PH7_NativeSetAttrInt(pVm,pIv,"m",sDiff.m);` |
|     21 | 1801 | `	PH7_NativeSetAttrInt(pVm,pIv,"d",sDiff.d);` |
|     21 | 1802 | `	PH7_NativeSetAttrInt(pVm,pIv,"h",sDiff.h);` |
|     21 | 1803 | `	PH7_NativeSetAttrInt(pVm,pIv,"i",sDiff.i);` |
|     21 | 1804 | `	PH7_NativeSetAttrInt(pVm,pIv,"s",sDiff.s);` |
|     21 | 1805 | `	PH7_NativeSetAttrInt(pVm,pIv,"days",sDiff.nDays);` |
|     21 | 1806 | `	PH7_NativeSetAttrInt(pVm,pIv,"invert",bAbsolute ? 0 : sDiff.bInvert);` |
|     21 | 1807 | `	PH7_NativeResultObject(pCtx,pIv);` |
|     21 | 1808 | `	return PH7_OK;` |
|     11 | 1809 | `}` |
|      - | 1810 | `/* DateTime::diff(DateTimeInterface $targetObject, bool $absolute = false) */` |
|     14 | 1811 | `static int vm_builtin_DateTime_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1812 | `{` |
|     15 | 1813 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1814 | `	ph7_class_instance *pTarget;` |
|     15 | 1815 | `	int bAbsolute = 0;` |
|     15 | 1816 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 1817 | `		return PH7_OK;` |
|      - | 1818 | `	}` |
|     15 | 1819 | `	pTarget = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     15 | 1820 | `	if( nArg > 1 ){` |
|      3 | 1821 | `		bAbsolute = DtValueTruth(pCtx->pVm,apArg[1]);` |
|      1 | 1822 | `	}` |
|     15 | 1823 | `	return DtDiffResult(pCtx,pThis,pTarget,bAbsolute);` |
|      8 | 1824 | `}` |
|      - | 1825 | `/* DateTime::modify(string $modifier) */` |
|     12 | 1826 | `static int vm_builtin_DateTime_modify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1827 | `{` |
|     13 | 1828 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 | 1829 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1830 | `	ph7_class_instance *pTarget;` |
|      - | 1831 | `	const char *zMod,*zErr;` |
|     13 | 1832 | `	int nMod,iPos,bCopy = 0,iErrPos;` |
|      - | 1833 | `	char cAt;` |
|     13 | 1834 | `	sxi64 iTs = 0;` |
|     13 | 1835 | `	sxi32 iOff = 0;` |
|     13 | 1836 | `	int bOffSet = 0,uSec = 0;` |
|     13 | 1837 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1838 | `		return PH7_OK;` |
|      - | 1839 | `	}` |
|     13 | 1840 | `	zMod = ph7_value_to_string(apArg[0],&nMod);` |
|     13 | 1841 | `	iErrPos = DtParse(zMod,nMod,PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),` |
|      - | 1842 | `		&iTs,&iOff,&bOffSet,&uSec);` |
|     13 | 1843 | `	if( iErrPos != 0 ){` |
|      3 | 1844 | `		int bImm = DtIsImmutable(pVm,pThis);` |
|      3 | 1845 | `		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);` |
|      4 | 1846 | `		return PH7_VmThrowException(pCtx,"DateMalformedStringException",` |
|      - | 1847 | `			"%s::modify(): Failed to parse time string (%.*s) at position %d (%c): %s",` |
|      1 | 1848 | `			bImm ? "DateTimeImmutable" : "DateTime",nMod,zMod,iPos,cAt,zErr);` |
|      - | 1849 | `	}` |
|     11 | 1850 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     11 | 1851 | `	PH7_NativeSetAttrInt(pVm,pTarget,DT_TS,iTs);` |
|     11 | 1852 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     11 | 1853 | `	return PH7_OK;` |
|      7 | 1854 | `}` |
|      - | 1855 | `/* DateTime::setTimestamp(int $timestamp) — php clears the microseconds with it */` |
|     10 | 1856 | `static int vm_builtin_DateTime_setTimestamp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1857 | `{` |
|     11 | 1858 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1859 | `	ph7_class_instance *pTarget;` |
|     11 | 1860 | `	int bCopy = 0;` |
|     11 | 1861 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1862 | `		return PH7_OK;` |
|      - | 1863 | `	}` |
|     11 | 1864 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     11 | 1865 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_TS,ph7_value_to_int64(apArg[0]));` |
|     11 | 1866 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_US,0);` |
|     11 | 1867 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|     11 | 1868 | `	return PH7_OK;` |
|      6 | 1869 | `}` |
|      - | 1870 | `/* DateTime::setMicrosecond(int $microsecond) */` |
|      4 | 1871 | `static int vm_builtin_DateTime_setMicrosecond(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1872 | `{` |
|      5 | 1873 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1874 | `	ph7_class_instance *pTarget;` |
|      5 | 1875 | `	int bCopy = 0;` |
|      5 | 1876 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1877 | `		return PH7_OK;` |
|      - | 1878 | `	}` |
|      5 | 1879 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|      5 | 1880 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_US,ph7_value_to_int64(apArg[0]));` |
|      5 | 1881 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|      5 | 1882 | `	return PH7_OK;` |
|      3 | 1883 | `}` |
|      - | 1884 | `/* DateTime::setTimezone(DateTimeZone $timezone) */` |
|    ! 0 | 1885 | `static int vm_builtin_DateTime_setTimezone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1886 | `{` |
|    ! 0 | 1887 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1888 | `	ph7_class_instance *pTarget;` |
|    ! 0 | 1889 | `	const char *zName = "UTC";` |
|    ! 0 | 1890 | `	int nName = 3,bCopy = 0;` |
|    ! 0 | 1891 | `	sxi32 iOff = 0;` |
|    ! 0 | 1892 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 1893 | `		return PH7_OK;` |
|      - | 1894 | `	}` |
|    ! 0 | 1895 | `	if( !DtZoneOf(apArg[0],&iOff,&zName,&nName) ){` |
|    ! 0 | 1896 | `		return PH7_OK;` |
|      - | 1897 | `	}` |
|    ! 0 | 1898 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|    ! 0 | 1899 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_OFF,iOff);` |
|    ! 0 | 1900 | `	PH7_NativeSetAttrStr(pCtx->pVm,pTarget,DT_NAME,zName,nName);` |
|    ! 0 | 1901 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|    ! 0 | 1902 | `	return PH7_OK;` |
|    ! 0 | 1903 | `}` |
|      - | 1904 | `/* Replace the DATE of an object, keeping its time of day (the offset it is` |
|      - | 1905 | ` * expressed in never changes). Shared with the date_date_set() alias. */` |
|      6 | 1906 | `static void DtSetDateOf(ph7_context *pCtx,ph7_class_instance *pObj,sxi64 y,int mo,int d)` |
|      1 | 1907 | `{` |
|      7 | 1908 | `	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);` |
|      7 | 1909 | `	sxi64 iDays = DtFloorDiv(iLocal,86400);` |
|      7 | 1910 | `	sxi64 iSecs = iLocal - iDays*86400;` |
|     10 | 1911 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,` |
|      9 | 1912 | `		DtMakeTs(y,mo,d,(int)(iSecs / 3600),(int)((iSecs / 60) % 60),(int)(iSecs % 60),` |
|      6 | 1913 | `			(sxi32)PH7_NativeAttrInt(pObj,DT_OFF)));` |
|      7 | 1914 | `}` |
|      - | 1915 | `/* Replace the TIME of day, keeping the date. Shared with date_time_set(). */` |
|      8 | 1916 | `static void DtSetTimeOf(ph7_context *pCtx,ph7_class_instance *pObj,int h,int mi,int s,sxi64 uSec)` |
|      1 | 1917 | `{` |
|      9 | 1918 | `	sxi64 iLocal = PH7_NativeAttrInt(pObj,DT_TS) + PH7_NativeAttrInt(pObj,DT_OFF);` |
|      9 | 1919 | `	sxi64 iDays = DtFloorDiv(iLocal,86400);` |
|      - | 1920 | `	sxi64 y;` |
|      - | 1921 | `	int mo,d;` |
|      9 | 1922 | `	DtCivilFromDays(iDays,&y,&mo,&d);` |
|      9 | 1923 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,DtMakeTs(y,mo,d,h,mi,s,(sxi32)PH7_NativeAttrInt(pObj,DT_OFF)));` |
|      9 | 1924 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,uSec);` |
|      9 | 1925 | `}` |
|      - | 1926 | `/* DateTime::setDate(int $year, int $month, int $day) — the time of day is kept */` |
|      6 | 1927 | `static int vm_builtin_DateTime_setDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1928 | `{` |
|      7 | 1929 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1930 | `	ph7_class_instance *pTarget;` |
|      7 | 1931 | `	int bCopy = 0;` |
|      7 | 1932 | `	if( pThis == 0 \|\| nArg < 3 ){` |
|    ! 0 | 1933 | `		return PH7_OK;` |
|      - | 1934 | `	}` |
|      7 | 1935 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     10 | 1936 | `	DtSetDateOf(pCtx,pTarget,ph7_value_to_int64(apArg[0]),ph7_value_to_int(apArg[1]),` |
|      6 | 1937 | `		ph7_value_to_int(apArg[2]));` |
|      7 | 1938 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|      7 | 1939 | `	return PH7_OK;` |
|      4 | 1940 | `}` |
|      - | 1941 | `/* DateTime::setTime(int $hour, int $minute, int $second = 0, int $microsecond = 0) */` |
|      8 | 1942 | `static int vm_builtin_DateTime_setTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1943 | `{` |
|      9 | 1944 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1945 | `	ph7_class_instance *pTarget;` |
|      9 | 1946 | `	int bCopy = 0;` |
|      9 | 1947 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 1948 | `		return PH7_OK;` |
|      - | 1949 | `	}` |
|      9 | 1950 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     18 | 1951 | `	DtSetTimeOf(pCtx,pTarget,ph7_value_to_int(apArg[0]),ph7_value_to_int(apArg[1]),` |
|      7 | 1952 | `		nArg > 2 ? ph7_value_to_int(apArg[2]) : 0,` |
|      6 | 1953 | `		nArg > 3 ? ph7_value_to_int64(apArg[3]) : 0);` |
|      9 | 1954 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|      9 | 1955 | `	return PH7_OK;` |
|      5 | 1956 | `}` |
|      - | 1957 | `/* DateTime::setISODate(int $year, int $week, int $dayOfWeek = 1) */` |
|      6 | 1958 | `static int vm_builtin_DateTime_setISODate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1959 | `{` |
|      7 | 1960 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1961 | `	ph7_class_instance *pTarget;` |
|      7 | 1962 | `	int bCopy = 0;` |
|      7 | 1963 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 1964 | `		return PH7_OK;` |
|      - | 1965 | `	}` |
|      7 | 1966 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     16 | 1967 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_TS,` |
|      9 | 1968 | `		DtIsoDate(PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),` |
|      6 | 1969 | `			ph7_value_to_int64(apArg[0]),ph7_value_to_int64(apArg[1]),` |
|      5 | 1970 | `			nArg > 2 ? ph7_value_to_int64(apArg[2]) : 1));` |
|      7 | 1971 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|      7 | 1972 | `	return PH7_OK;` |
|      4 | 1973 | `}` |
|      - | 1974 | `/* add()/sub(): one body, the sign is the difference (and a DateInterval carrying` |
|      - | 1975 | `` * `invert` flips it, exactly as the chunk's __dtAddTs did). */`` |
|      8 | 1976 | `static int DtAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)` |
|      1 | 1977 | `{` |
|      9 | 1978 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 1979 | `	ph7_class_instance *pTarget,*pIv;` |
|      9 | 1980 | `	int bCopy = 0;` |
|      9 | 1981 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 1982 | `		return PH7_OK;` |
|      - | 1983 | `	}` |
|      9 | 1984 | `	pIv = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      9 | 1985 | `	if( PH7_NativeAttrInt(pIv,"invert") ){` |
|    ! 0 | 1986 | `		iSign = -iSign;` |
|    ! 0 | 1987 | `	}` |
|      9 | 1988 | `	pTarget = DtMutTarget(pCtx,pThis,&bCopy);` |
|     17 | 1989 | `	PH7_NativeSetAttrInt(pCtx->pVm,pTarget,DT_TS,` |
|     12 | 1990 | `		DtCivilAdd(PH7_NativeAttrInt(pThis,DT_TS),(sxi32)PH7_NativeAttrInt(pThis,DT_OFF),` |
|      4 | 1991 | `			PH7_NativeAttrInt(pIv,"y"),PH7_NativeAttrInt(pIv,"m"),PH7_NativeAttrInt(pIv,"d"),` |
|      4 | 1992 | `			PH7_NativeAttrInt(pIv,"h"),PH7_NativeAttrInt(pIv,"i"),PH7_NativeAttrInt(pIv,"s"),iSign));` |
|      9 | 1993 | `	DtMutResult(pCtx,pTarget,bCopy);` |
|      9 | 1994 | `	return PH7_OK;` |
|      5 | 1995 | `}` |
|      4 | 1996 | `static int vm_builtin_DateTime_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1997 | `{` |
|      5 | 1998 | `	return DtAddSub(pCtx,nArg,apArg,1);` |
|      1 | 1999 | `}` |
|      4 | 2000 | `static int vm_builtin_DateTime_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2001 | `{` |
|      5 | 2002 | `	return DtAddSub(pCtx,nArg,apArg,-1);` |
|      1 | 2003 | `}` |
|      - | 2004 | ``/* DateTime::getLastErrors() — php's array, or `false` when the last parse was clean */`` |
|     24 | 2005 | `static int vm_builtin_DateTime_getLastErrors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2006 | `{` |
|     25 | 2007 | `	ph7_vm *pVm = pCtx->pVm;` |
|     25 | 2008 | `	const phl_dt_lasterr *pErr = &pVm->sDtLastErr;` |
|      - | 2009 | `	ph7_value *pArr,*pWarn,*pErrs,*pVal;` |
|      - | 2010 | `	int k;` |
|     12 | 2011 | `	SXUNUSED(nArg);` |
|     12 | 2012 | `	SXUNUSED(apArg);` |
|     25 | 2013 | `	if( !pErr->bSet ){` |
|      7 | 2014 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2015 | `		return PH7_OK;` |
|      - | 2016 | `	}` |
|     19 | 2017 | `	pArr = ph7_context_new_array(pCtx);` |
|     19 | 2018 | `	pWarn = ph7_context_new_array(pCtx);` |
|     19 | 2019 | `	pErrs = ph7_context_new_array(pCtx);` |
|     19 | 2020 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     19 | 2021 | `	if( pArr == 0 \|\| pWarn == 0 \|\| pErrs == 0 \|\| pVal == 0 ){` |
|    ! 0 | 2022 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2023 | `	}` |
|     25 | 2024 | `	for( k = 0 ; k < pErr->nWarnKept ; k++ ){` |
|      7 | 2025 | `		ph7_value_string(pVal,pErr->azWarn[k],-1);` |
|      7 | 2026 | `		ph7_array_add_intkey_elem(pWarn,pErr->aWarnPos[k],pVal);` |
|      7 | 2027 | `		ph7_value_reset_string_cursor(pVal);` |
|      4 | 2028 | `	}` |
|     35 | 2029 | `	for( k = 0 ; k < pErr->nErrKept ; k++ ){` |
|     17 | 2030 | `		ph7_value_string(pVal,pErr->azErr[k],-1);` |
|     17 | 2031 | `		ph7_array_add_intkey_elem(pErrs,pErr->aErrPos[k],pVal);` |
|     17 | 2032 | `		ph7_value_reset_string_cursor(pVal);` |
|      9 | 2033 | `	}` |
|     19 | 2034 | `	ph7_value_int(pVal,pErr->nWarn);` |
|     19 | 2035 | `	ph7_array_add_strkey_elem(pArr,"warning_count",pVal);` |
|     19 | 2036 | `	ph7_array_add_strkey_elem(pArr,"warnings",pWarn);` |
|     19 | 2037 | `	ph7_value_int(pVal,pErr->nErr);` |
|     19 | 2038 | `	ph7_array_add_strkey_elem(pArr,"error_count",pVal);` |
|     19 | 2039 | `	ph7_array_add_strkey_elem(pArr,"errors",pErrs);` |
|     19 | 2040 | `	ph7_result_value(pCtx,pArr);` |
|     19 | 2041 | `	return PH7_OK;` |
|     13 | 2042 | `}` |
|      - | 2043 | `/*` |
|      - | 2044 | ` * The class a static factory builds. php uses LATE STATIC BINDING here, so` |
|      - | 2045 | `` * `D::createFromFormat()` on a subclass answers a D — where the chunk hardcoded`` |
|      - | 2046 | ` * the literal class name and always answered a DateTime.` |
|      - | 2047 | ` */` |
|    102 | 2048 | `static ph7_class * DtFactoryClass(ph7_context *pCtx,const char *zFallback)` |
|      1 | 2049 | `{` |
|    103 | 2050 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|    103 | 2051 | `	return pClass ? pClass : DtClass(pCtx->pVm,zFallback);` |
|      1 | 2052 | `}` |
|      - | 2053 | `/* DateTime::createFromFormat(string $format, string $datetime, ?DateTimeZone $timezone = null) */` |
|     68 | 2054 | `static int DtCreateFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)` |
|      1 | 2055 | `{` |
|     69 | 2056 | `	ph7_vm *pVm = pCtx->pVm;` |
|     69 | 2057 | `	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);` |
|      - | 2058 | `	ph7_class_instance *pObj;` |
|      - | 2059 | `	dt_ff_res sRes;` |
|      - | 2060 | `	dt_state sState;` |
|      - | 2061 | `	char zNameBuf[16];` |
|      - | 2062 | `	const char *zZone;` |
|      - | 2063 | `	int nZone;` |
|     69 | 2064 | `	sxi32 iZoneOff = 0;` |
|      - | 2065 | `	const char *zFmt,*zIn;` |
|      - | 2066 | `	int nFmt,nIn;` |
|     69 | 2067 | `	if( pClass == 0 \|\| nArg < 2 ){` |
|    ! 0 | 2068 | `		return PH7_OK;` |
|      - | 2069 | `	}` |
|     69 | 2070 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|     69 | 2071 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|     69 | 2072 | `	zZone = pVm->zDefTz;` |
|     69 | 2073 | `	nZone = (int)pVm->nDefTz;` |
|     69 | 2074 | `	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     41 | 2075 | `		DtZoneOf(apArg[2],&iZoneOff,&zZone,&nZone);` |
|     20 | 2076 | `	}` |
|     69 | 2077 | `	if( DtFromFormat(zFmt,nFmt,zIn,nIn,(sxi64)time(0),iZoneOff,&sRes) != 0 ){` |
|     13 | 2078 | `		pVm->sDtLastErr = sRes.sDiag;` |
|     13 | 2079 | `		ph7_result_bool(pCtx,0);` |
|     13 | 2080 | `		return PH7_OK;` |
|      - | 2081 | `	}` |
|     57 | 2082 | `	pVm->sDtLastErr = sRes.sDiag;` |
|     57 | 2083 | `	sState.iTs = sRes.iTs;` |
|     57 | 2084 | `	sState.uSec = sRes.bHasUs ? sRes.uSec : 0;` |
|     57 | 2085 | `	switch( sRes.iOffKind ){` |
|     23 | 2086 | `		case 0:` |
|     47 | 2087 | `			sState.iOff = iZoneOff;` |
|     47 | 2088 | `			sState.zName = zZone;` |
|     47 | 2089 | `			sState.nName = nZone;` |
|     47 | 2090 | `			break;` |
|    ! 0 | 2091 | `		case 2:` |
|    ! 0 | 2092 | `			sState.iOff = 0;` |
|    ! 0 | 2093 | `			sState.zName = "Z";` |
|    ! 0 | 2094 | `			sState.nName = 1;` |
|    ! 0 | 2095 | `			break;` |
|      1 | 2096 | `		case 3:` |
|      3 | 2097 | `			sState.iOff = sRes.iOff;` |
|      3 | 2098 | `			sState.zName = sRes.zName;` |
|      3 | 2099 | `			sState.nName = (int)SyStrlen(sRes.zName);` |
|      3 | 2100 | `			break;` |
|      4 | 2101 | `		default:` |
|      9 | 2102 | `			sState.iOff = sRes.iOff;` |
|      9 | 2103 | `			sState.nName = DtOffName(zNameBuf,sizeof(zNameBuf),sRes.iOff);` |
|      9 | 2104 | `			sState.zName = zNameBuf;` |
|      8 | 2105 | `			break;` |
|      - | 2106 | `	}` |
|     57 | 2107 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     57 | 2108 | `	if( pObj == 0 ){` |
|    ! 0 | 2109 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2110 | `	}` |
|     57 | 2111 | `	DtStore(pVm,pObj,&sState);` |
|     57 | 2112 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     57 | 2113 | `	return PH7_OK;` |
|     35 | 2114 | `}` |
|     52 | 2115 | `static int vm_builtin_DateTime_createFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2116 | `{` |
|     53 | 2117 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTime");` |
|      1 | 2118 | `}` |
|      2 | 2119 | `static int vm_builtin_DateTimeImmutable_createFromFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2120 | `{` |
|      3 | 2121 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 2122 | `}` |
|      - | 2123 | `/* createFromImmutable()/createFromMutable()/createFromInterface(): one copy body */` |
|     14 | 2124 | `static int DtCopyOf(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFallback)` |
|      1 | 2125 | `{` |
|     15 | 2126 | `	ph7_vm *pVm = pCtx->pVm;` |
|     15 | 2127 | `	ph7_class *pClass = DtFactoryClass(pCtx,zFallback);` |
|      - | 2128 | `	ph7_class_instance *pSrc,*pObj;` |
|      - | 2129 | `	dt_state sState;` |
|     15 | 2130 | `	if( pClass == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 2131 | `		return PH7_OK;` |
|      - | 2132 | `	}` |
|     15 | 2133 | `	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     15 | 2134 | `	DtLoad(pSrc,&sState);` |
|     15 | 2135 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     15 | 2136 | `	if( pObj == 0 ){` |
|    ! 0 | 2137 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2138 | `	}` |
|     15 | 2139 | `	DtStore(pVm,pObj,&sState);` |
|     15 | 2140 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     15 | 2141 | `	return PH7_OK;` |
|      8 | 2142 | `}` |
|      6 | 2143 | `static int vm_builtin_DateTime_copyOf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2144 | `{` |
|      7 | 2145 | `	return DtCopyOf(pCtx,nArg,apArg,"DateTime");` |
|      1 | 2146 | `}` |
|      8 | 2147 | `static int vm_builtin_DateTimeImmutable_copyOf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2148 | `{` |
|      9 | 2149 | `	return DtCopyOf(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 2150 | `}` |
|      - | 2151 | `/*` |
|      - | 2152 | ` * int\|false strtotime(string $datetime, ?int $baseTimestamp = null)` |
|      - | 2153 | ` *` |
|      - | 2154 | ` * Rides the same DtParse the constructor uses, so its format coverage is identical.` |
|      - | 2155 | ` * php: the EMPTY string is false, but whitespace-only is 'now'; a parse failure is` |
|      - | 2156 | ` * false (never an exception), and the default timezone is offset 0 — exactly what` |
|      - | 2157 | ` * the constructor does for a null $timezone.` |
|      - | 2158 | ` */` |
|    268 | 2159 | `static int vm_builtin_strtotime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2160 | `{` |
|      - | 2161 | `	const char *zIn;` |
|      - | 2162 | `	int nIn;` |
|      - | 2163 | `	sxi64 iBase;` |
|    269 | 2164 | `	sxi64 iTs = 0;` |
|    269 | 2165 | `	sxi32 iOff = 0;` |
|    269 | 2166 | `	int bOffSet = 0,uSec = 0;` |
|    269 | 2167 | `	if( nArg < 1 ){` |
|    ! 0 | 2168 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2169 | `		return PH7_OK;` |
|      - | 2170 | `	}` |
|    269 | 2171 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    269 | 2172 | `	if( nIn < 1 ){` |
|      3 | 2173 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2174 | `		return PH7_OK;` |
|      - | 2175 | `	}` |
|    267 | 2176 | `	iBase = (nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0)` |
|    399 | 2177 | `		? ph7_value_to_int64(apArg[1]) : (sxi64)time(0);` |
|    267 | 2178 | `	if( DtParse(zIn,nIn,iBase,0,&iTs,&iOff,&bOffSet,&uSec) != 0 ){` |
|     23 | 2179 | `		ph7_result_bool(pCtx,0);` |
|     23 | 2180 | `		return PH7_OK;` |
|      - | 2181 | `	}` |
|    245 | 2182 | `	ph7_result_int64(pCtx,iTs);` |
|    245 | 2183 | `	return PH7_OK;` |
|    135 | 2184 | `}` |
|      - | 2185 | `/*` |
|      - | 2186 | ` * ---------------------------------------------------------------------------` |
|      - | 2187 | ` * DateInterval, DatePeriod and its iterator, declared from C.` |
|      - | 2188 | ` *` |
|      - | 2189 | ``  * The rest of the date chunk. DateInterval's two constructors were `preg_match` `` |
|      - | 2190 | `` * calls in PHP; DatePeriod's `getIterator()` was a PHP GENERATOR, which a C body`` |
|      - | 2191 | `` * cannot be -- so it answers a native `InternalIterator`, which is exactly the`` |
|      - | 2192 | ` * class php answers there.` |
|      - | 2193 | ` * ---------------------------------------------------------------------------` |
|      - | 2194 | ` */` |
|      - | 2195 | `/* php's unit words for DateInterval::createFromDateString(), longest first so a` |
|      - | 2196 | ` * prefix never wins over the word that contains it. */` |
|      - | 2197 | `typedef struct dt_unit dt_unit;` |
|      - | 2198 | `struct dt_unit` |
|      - | 2199 | `{` |
|      - | 2200 | `	const char *zName;` |
|      - | 2201 | `	int iField;   /* 0=y 1=m 2=d 3=h 4=i 5=s */` |
|      - | 2202 | `	int nMul;` |
|      - | 2203 | `};` |
|      - | 2204 | `static const dt_unit aDtUnit[] = {` |
|      - | 2205 | `	{ "seconds", 5, 1 }, { "second", 5, 1 }, { "secs", 5, 1 }, { "sec", 5, 1 },` |
|      - | 2206 | `	{ "minutes", 4, 1 }, { "minute", 4, 1 }, { "mins", 4, 1 }, { "min", 4, 1 },` |
|      - | 2207 | `	{ "hours", 3, 1 },   { "hour", 3, 1 },` |
|      - | 2208 | `	{ "fortnights", 2, 14 }, { "fortnight", 2, 14 },` |
|      - | 2209 | `	{ "weeks", 2, 7 },   { "week", 2, 7 },` |
|      - | 2210 | `	{ "days", 2, 1 },    { "day", 2, 1 },` |
|      - | 2211 | `	{ "months", 1, 1 },  { "month", 1, 1 },` |
|      - | 2212 | `	{ "years", 0, 1 },   { "year", 0, 1 },` |
|      - | 2213 | `};` |
|      - | 2214 | `static const char * const azDtIvField[] = { "y", "m", "d", "h", "i", "s" };` |
|      - | 2215 | `/* Read an unsigned run of digits; returns the count consumed. */` |
|    120 | 2216 | `static int DtIvDigits(const char *z,const char *zEnd,sxi64 *pVal)` |
|      1 | 2217 | `{` |
|    121 | 2218 | `	int n = 0;` |
|    121 | 2219 | `	sxi64 v = 0;` |
|    243 | 2220 | `	while( &z[n] < zEnd && SyisDigit(z[n]) ){` |
|    123 | 2221 | `		v = v*10 + (z[n] - '0');` |
|    123 | 2222 | `		n++;` |
|      1 | 2223 | `	}` |
|    121 | 2224 | `	*pVal = v;` |
|    121 | 2225 | `	return n;` |
|      1 | 2226 | `}` |
|      - | 2227 | `/*` |
|      - | 2228 | ` * php's ISO-8601 duration grammar: P[nY][nM][nW][nD][T[nH][nM][nS]], every field` |
|      - | 2229 | ` * an unsigned integer. A bare "P", a trailing "T" and a fractional second are all` |
|      - | 2230 | ` * rejected, as php rejects them.` |
|      - | 2231 | ` */` |
|     62 | 2232 | `static int DtIvParseIso(const char *zIn,int nIn,sxi64 *aOut)` |
|      1 | 2233 | `{` |
|     63 | 2234 | `	const char *z = zIn,*zEnd = &zIn[nIn];` |
|     63 | 2235 | `	int bTime = 0,bAny = 0;` |
|      - | 2236 | `	int k;` |
|    435 | 2237 | `	for( k = 0 ; k < 6 ; k++ ){` |
|    373 | 2238 | `		aOut[k] = 0;` |
|    187 | 2239 | `	}` |
|     63 | 2240 | `	if( nIn < 2 \|\| zIn[0] != 'P' \|\| zIn[nIn-1] == 'T' ){` |
|     11 | 2241 | `		return -1;` |
|      - | 2242 | `	}` |
|     53 | 2243 | `	z++;` |
|    143 | 2244 | `	while( z < zEnd ){` |
|      - | 2245 | `		sxi64 v;` |
|      - | 2246 | `		int n;` |
|    101 | 2247 | `		if( z[0] == 'T' ){` |
|     13 | 2248 | `			if( bTime ){` |
|    ! 0 | 2249 | `				return -1;` |
|      - | 2250 | `			}` |
|     13 | 2251 | `			bTime = 1;` |
|     13 | 2252 | `			z++;` |
|     13 | 2253 | `			continue;` |
|      - | 2254 | `		}` |
|     89 | 2255 | `		n = DtIvDigits(z,zEnd,&v);` |
|     89 | 2256 | `		if( n == 0 \|\| z + n >= zEnd ){` |
|      5 | 2257 | `			return -1;` |
|      - | 2258 | `		}` |
|     85 | 2259 | `		z += n;` |
|     85 | 2260 | `		switch( z[0] ){` |
|      9 | 2261 | `			case 'Y': if( bTime ){ return -1; } aOut[0] += v; break;` |
|      9 | 2262 | `			case 'W': if( bTime ){ return -1; } aOut[2] += v * 7; break;` |
|     25 | 2263 | `			case 'D': if( bTime ){ return -1; } aOut[2] += v; break;` |
|      9 | 2264 | `			case 'H': if( !bTime ){ return -1; } aOut[3] += v; break;` |
|      9 | 2265 | `			case 'S': if( !bTime ){ return -1; } aOut[5] += v; break;` |
|     11 | 2266 | `			case 'M':` |
|      - | 2267 | `				/* The one ambiguous designator: months before T, minutes after. */` |
|     23 | 2268 | `				if( bTime ){ aOut[4] += v; }else{ aOut[1] += v; }` |
|     23 | 2269 | `				break;` |
|      3 | 2270 | `			default:` |
|      7 | 2271 | `				return -1;` |
|      - | 2272 | `		}` |
|     79 | 2273 | `		z++;` |
|     79 | 2274 | `		bAny = 1;` |
|      1 | 2275 | `	}` |
|     43 | 2276 | `	return bAny ? 0 : -1;` |
|     32 | 2277 | `}` |
|      - | 2278 | `/*` |
|      - | 2279 | ` * php's relative-string interval. The string is VALIDATED by the same parser` |
|      - | 2280 | ` * strtotime() uses -- which is where php's "at position N (c): reason" wording` |
|      - | 2281 | ` * comes from -- and the number/unit pairs it understands are then summed. A` |
|      - | 2282 | ` * string the parser accepts but that names no unit ("next monday") is php's` |
|      - | 2283 | ` * all-zero interval, not an error.` |
|      - | 2284 | ` */` |
|     16 | 2285 | `static int DtIvParseRelative(const char *zIn,int nIn,sxi64 *aOut,int *piPos,` |
|      - | 2286 | `	char *pcAt,const char **pzReason)` |
|      1 | 2287 | `{` |
|     17 | 2288 | `	const char *z = zIn,*zEnd = &zIn[nIn];` |
|     17 | 2289 | `	sxi64 iTs = 0;` |
|     17 | 2290 | `	sxi32 iOff = 0;` |
|     17 | 2291 | `	int bOffSet = 0,uSec = 0,iErr;` |
|      - | 2292 | `	int k;` |
|    113 | 2293 | `	for( k = 0 ; k < 6 ; k++ ){` |
|     97 | 2294 | `		aOut[k] = 0;` |
|     49 | 2295 | `	}` |
|     17 | 2296 | `	if( nIn < 1 ){` |
|      3 | 2297 | `		*piPos = 0;` |
|      3 | 2298 | `		*pcAt = ' ';` |
|      3 | 2299 | `		*pzReason = "Empty string";` |
|      3 | 2300 | `		return -1;` |
|      - | 2301 | `	}` |
|     15 | 2302 | `	iErr = DtParse(zIn,nIn,0,0,&iTs,&iOff,&bOffSet,&uSec);` |
|     15 | 2303 | `	if( iErr != 0 ){` |
|    ! 0 | 2304 | `		*pzReason = DtParseErr(zIn,nIn,iErr,piPos,pcAt);` |
|    ! 0 | 2305 | `		return -1;` |
|      - | 2306 | `	}` |
|     37 | 2307 | `	while( z < zEnd ){` |
|      - | 2308 | `		sxi64 v;` |
|     23 | 2309 | `		int n,iSign = 1,iUnit;` |
|     52 | 2310 | `		while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t' \|\| z[0] == '\n' \|\| z[0] == '\r'` |
|     28 | 2311 | `		 \|\| z[0] == ',' \|\| z[0] == '+') ){` |
|     19 | 2312 | `			z++;` |
|      1 | 2313 | `		}` |
|     23 | 2314 | `		if( z < zEnd && z[0] == '-' ){` |
|    ! 0 | 2315 | `			iSign = -1;` |
|    ! 0 | 2316 | `			z++;` |
|    ! 0 | 2317 | `		}` |
|     23 | 2318 | `		n = DtIvDigits(z,zEnd,&v);` |
|     23 | 2319 | `		if( n == 0 ){` |
|      - | 2320 | `			/* Not a number: skip the token (php's parser already accepted the` |
|      - | 2321 | `			 * string, so this is a relative form with no interval field). */` |
|     25 | 2322 | `			while( z < zEnd && z[0] != ' ' && z[0] != ',' ){` |
|     21 | 2323 | `				z++;` |
|      1 | 2324 | `			}` |
|      5 | 2325 | `			continue;` |
|      - | 2326 | `		}` |
|     19 | 2327 | `		z += n;` |
|     46 | 2328 | `		while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|     19 | 2329 | `			z++;` |
|      1 | 2330 | `		}` |
|     19 | 2331 | `		iUnit = -1;` |
|    263 | 2332 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(aDtUnit) ; k++ ){` |
|    263 | 2333 | `			int nU = (int)SyStrlen(aDtUnit[k].zName);` |
|    262 | 2334 | `			if( zEnd - z >= nU && SyStrnicmp(z,aDtUnit[k].zName,(sxu32)nU) == 0` |
|    107 | 2335 | `			 && (zEnd - z == nU \|\| !(SyisAlphaNum(z[nU]) \|\| z[nU] == '_')) ){` |
|     19 | 2336 | `				iUnit = k;` |
|     19 | 2337 | `				z += nU;` |
|     19 | 2338 | `				break;` |
|      - | 2339 | `			}` |
|    123 | 2340 | `		}` |
|     19 | 2341 | `		if( iUnit < 0 ){` |
|    ! 0 | 2342 | `			continue;` |
|      - | 2343 | `		}` |
|     19 | 2344 | `		aOut[aDtUnit[iUnit].iField] += iSign * v * aDtUnit[iUnit].nMul;` |
|      1 | 2345 | `	}` |
|     15 | 2346 | `	return 0;` |
|      9 | 2347 | `}` |
|      - | 2348 | `/* Write the six relative fields onto a DateInterval instance. */` |
|     56 | 2349 | `static void DtIvStore(ph7_vm *pVm,ph7_class_instance *pObj,const sxi64 *aVal)` |
|      1 | 2350 | `{` |
|      - | 2351 | `	int k;` |
|    393 | 2352 | `	for( k = 0 ; k < 6 ; k++ ){` |
|    337 | 2353 | `		PH7_NativeSetAttrInt(pVm,pObj,azDtIvField[k],aVal[k]);` |
|    169 | 2354 | `	}` |
|     57 | 2355 | `}` |
|      - | 2356 | `/* DateInterval::__construct(string $duration) */` |
|     54 | 2357 | `static int vm_builtin_DateInterval_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2358 | `{` |
|     55 | 2359 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 2360 | `	const char *zDur;` |
|      - | 2361 | `	int nDur;` |
|      - | 2362 | `	sxi64 aVal[6];` |
|     55 | 2363 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 2364 | `		return PH7_OK;` |
|      - | 2365 | `	}` |
|     55 | 2366 | `	zDur = ph7_value_to_string(apArg[0],&nDur);` |
|     55 | 2367 | `	if( DtIvParseIso(zDur,nDur,aVal) != 0 ){` |
|     31 | 2368 | `		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",` |
|     10 | 2369 | `			"Unknown or bad format (%.*s)",nDur,zDur);` |
|      - | 2370 | `	}` |
|     35 | 2371 | `	DtIvStore(pCtx->pVm,pThis,aVal);` |
|     35 | 2372 | `	return PH7_OK;` |
|     28 | 2373 | `}` |
|      - | 2374 | `/*` |
|      - | 2375 | ` * DateInterval::createFromDateString(string $datetime). Shared with the` |
|      - | 2376 | ` * date_interval_create_from_date_string() alias, which WARNS and answers false` |
|      - | 2377 | ` * where the method throws.` |
|      - | 2378 | ` */` |
|     16 | 2379 | `static ph7_class_instance * DtIvFromDateString(ph7_context *pCtx,const char *zIn,int nIn,` |
|      - | 2380 | `	int *piPos,char *pcAt,const char **pzReason)` |
|      1 | 2381 | `{` |
|     17 | 2382 | `	ph7_vm *pVm = pCtx->pVm;` |
|     17 | 2383 | `	ph7_class *pClass = DtFactoryClass(pCtx,"DateInterval");` |
|      - | 2384 | `	ph7_class_instance *pObj;` |
|      - | 2385 | `	sxi64 aVal[6];` |
|      - | 2386 | `	ph7_value sVal;` |
|     17 | 2387 | `	if( pClass == 0 ){` |
|    ! 0 | 2388 | `		return 0;` |
|      - | 2389 | `	}` |
|     17 | 2390 | `	if( DtIvParseRelative(zIn,nIn,aVal,piPos,pcAt,pzReason) != 0 ){` |
|      3 | 2391 | `		return 0;` |
|      - | 2392 | `	}` |
|     15 | 2393 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     15 | 2394 | `	if( pObj == 0 ){` |
|    ! 0 | 2395 | `		return 0;` |
|      - | 2396 | `	}` |
|     15 | 2397 | `	DtIvStore(pVm,pObj,aVal);` |
|      - | 2398 | ``	/* php marks the interval as built from a string; the `date_string` property it`` |
|      - | 2399 | `	 * adds with it needs a dynamic property PHL has no equivalent of (§7.4). */` |
|     15 | 2400 | `	PH7_MemObjInitFromBool(pVm,&sVal,1);` |
|     15 | 2401 | `	PH7_NativeSetProp(pVm,pObj,"from_string",sizeof("from_string")-1,&sVal);` |
|     15 | 2402 | `	PH7_MemObjRelease(&sVal);` |
|     15 | 2403 | `	return pObj;` |
|      9 | 2404 | `}` |
|     10 | 2405 | `static int vm_builtin_DateInterval_createFromDateString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2406 | `{` |
|     11 | 2407 | `	const char *zIn,*zReason = "";` |
|     11 | 2408 | `	int nIn,iPos = 0;` |
|     11 | 2409 | `	char cAt = ' ';` |
|      - | 2410 | `	ph7_class_instance *pObj;` |
|     11 | 2411 | `	if( nArg < 1 ){` |
|    ! 0 | 2412 | `		return PH7_OK;` |
|      - | 2413 | `	}` |
|     11 | 2414 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|     11 | 2415 | `	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason);` |
|     11 | 2416 | `	if( pObj == 0 ){` |
|      4 | 2417 | `		return PH7_VmThrowException(pCtx,"DateMalformedIntervalStringException",` |
|      1 | 2418 | `			"Unknown or bad format (%.*s) at position %d (%c): %s",nIn,zIn,iPos,cAt,zReason);` |
|      - | 2419 | `	}` |
|      9 | 2420 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      9 | 2421 | `	return PH7_OK;` |
|      6 | 2422 | `}` |
|      - | 2423 | `/*` |
|      - | 2424 | ` * DateInterval::format(string $format) -- php's own %-token loop, including the` |
|      - | 2425 | `` * rule the chunk got wrong: an UNKNOWN token keeps its '%' (`%q` is "%q").`` |
|      - | 2426 | ` */` |
|     20 | 2427 | `static void DtIvFormat(ph7_context *pCtx,ph7_class_instance *pObj,const char *zFmt,int nFmt)` |
|      1 | 2428 | `{` |
|      - | 2429 | `	SyBlob sOut;` |
|      - | 2430 | `	ph7_value *pDays;` |
|      - | 2431 | `	int k;` |
|     21 | 2432 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    279 | 2433 | `	for( k = 0 ; k < nFmt ; k++ ){` |
|    261 | 2434 | `		char c = zFmt[k];` |
|      - | 2435 | `		char t;` |
|    261 | 2436 | `		if( c != '%' ){` |
|    137 | 2437 | `			SyBlobAppend(&sOut,&c,1);` |
|    137 | 2438 | `			continue;` |
|      - | 2439 | `		}` |
|    125 | 2440 | `		k++;` |
|    125 | 2441 | `		if( k >= nFmt ){` |
|      - | 2442 | `			/* php drops a trailing lone '%' rather than echoing it. */` |
|      3 | 2443 | `			break;` |
|      - | 2444 | `		}` |
|    123 | 2445 | `		t = zFmt[k];` |
|    123 | 2446 | `		switch( t ){` |
|      5 | 2447 | `			case 'Y': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"y")); break;` |
|     11 | 2448 | `			case 'y': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"y")); break;` |
|      5 | 2449 | `			case 'M': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"m")); break;` |
|     11 | 2450 | `			case 'm': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"m")); break;` |
|      5 | 2451 | `			case 'D': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"d")); break;` |
|     13 | 2452 | `			case 'd': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"d")); break;` |
|      5 | 2453 | `			case 'H': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"h")); break;` |
|      7 | 2454 | `			case 'h': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"h")); break;` |
|      5 | 2455 | `			case 'I': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"i")); break;` |
|      7 | 2456 | `			case 'i': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"i")); break;` |
|      5 | 2457 | `			case 'S': SyBlobFormat(&sOut,"%02d",(int)PH7_NativeAttrInt(pObj,"s")); break;` |
|      9 | 2458 | `			case 's': SyBlobFormat(&sOut,"%qd",PH7_NativeAttrInt(pObj,"s")); break;` |
|      2 | 2459 | `			case 'F': case 'f': {` |
|      5 | 2460 | `				ph7_value *pF = PH7_NativeAttr(pObj,"f");` |
|      5 | 2461 | `				double r = 0.0;` |
|      - | 2462 | `				sxi64 uS;` |
|      5 | 2463 | `				if( pF && (pF->iFlags & MEMOBJ_REAL) ){` |
|      5 | 2464 | `					r = pF->rVal;` |
|      2 | 2465 | `				}else if( pF && (pF->iFlags & MEMOBJ_INT) ){` |
|    ! 0 | 2466 | `					r = (double)pF->x.iVal;` |
|    ! 0 | 2467 | `				}` |
|      5 | 2468 | `				uS = (sxi64)(r * 1000000.0 + (r < 0 ? -0.5 : 0.5));` |
|      5 | 2469 | `				if( t == 'F' ){` |
|      3 | 2470 | `					SyBlobFormat(&sOut,"%06d",(int)uS);` |
|      2 | 2471 | `				}else{` |
|      3 | 2472 | `					SyBlobFormat(&sOut,"%qd",uS);` |
|      - | 2473 | `				}` |
|      5 | 2474 | `				break;` |
|      - | 2475 | `			}` |
|     13 | 2476 | `			case 'R': SyBlobAppend(&sOut,PH7_NativeAttrInt(pObj,"invert") ? "-" : "+",1); break;` |
|      5 | 2477 | `			case 'r': if( PH7_NativeAttrInt(pObj,"invert") ){ SyBlobAppend(&sOut,"-",1); } break;` |
|      9 | 2478 | `			case 'a':` |
|     19 | 2479 | `				pDays = PH7_NativeAttr(pObj,"days");` |
|     19 | 2480 | `				if( pDays && (pDays->iFlags & MEMOBJ_INT) ){` |
|     13 | 2481 | `					SyBlobFormat(&sOut,"%qd",pDays->x.iVal);` |
|      7 | 2482 | `				}else{` |
|      7 | 2483 | `					SyBlobAppend(&sOut,"(unknown)",sizeof("(unknown)")-1);` |
|      - | 2484 | `				}` |
|     19 | 2485 | `				break;` |
|      7 | 2486 | `			case '%': SyBlobAppend(&sOut,"%",1); break;` |
|      1 | 2487 | `			default:` |
|      - | 2488 | `				/* php keeps BOTH bytes of an unrecognised token. */` |
|      3 | 2489 | `				SyBlobAppend(&sOut,"%",1);` |
|      3 | 2490 | `				SyBlobAppend(&sOut,&t,1);` |
|      2 | 2491 | `				break;` |
|      - | 2492 | `		}` |
|     62 | 2493 | `	}` |
|     21 | 2494 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     21 | 2495 | `	SyBlobRelease(&sOut);` |
|     21 | 2496 | `}` |
|     16 | 2497 | `static int vm_builtin_DateInterval_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2498 | `{` |
|     17 | 2499 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 2500 | `	const char *zFmt;` |
|      - | 2501 | `	int nFmt;` |
|     17 | 2502 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 2503 | `		return PH7_OK;` |
|      - | 2504 | `	}` |
|     17 | 2505 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|     17 | 2506 | `	DtIvFormat(pCtx,pThis,zFmt,nFmt);` |
|     17 | 2507 | `	return PH7_OK;` |
|      9 | 2508 | `}` |
|      - | 2509 | `/* Is this value an instance of the named class? */` |
|     60 | 2510 | `static int DtValueIsA(ph7_vm *pVm,ph7_value *pVal,const char *zClass)` |
|      1 | 2511 | `{` |
|      - | 2512 | `	ph7_class *pClass;` |
|     61 | 2513 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      3 | 2514 | `		return 0;` |
|      - | 2515 | `	}` |
|     59 | 2516 | `	pClass = DtClass(&(*pVm),zClass);` |
|     59 | 2517 | `	return pClass != 0` |
|     58 | 2518 | `		&& PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass);` |
|     31 | 2519 | `}` |
|      - | 2520 | `/*` |
|      - | 2521 | ` * Write an object (or null) into a declared property of another object.` |
|      - | 2522 | ` *` |
|      - | 2523 | ` * The scratch value ALIASES the instance rather than owning it, and` |
|      - | 2524 | ` * PH7_MemObjStore takes the reference the slot keeps -- so releasing the scratch` |
|      - | 2525 | ` * afterwards would hand back the slot's own reference and free the object out from` |
|      - | 2526 | `` * under it (which is what it did: `foreach` over a DatePeriod crashed on the second`` |
|      - | 2527 | `` * element's `->format()`). The caller keeps owning whatever it passed in.`` |
|      - | 2528 | ` */` |
|      - | 2529 | `/*` |
|      - | 2530 | ` * DatePeriod::__construct($start, $interval, $end, $options)` |
|      - | 2531 | ` *` |
|      - | 2532 | ` * php overloads it three ways and rejects everything else with ONE message, which` |
|      - | 2533 | ` * is why the signature stays unenforced and the shapes are checked here.` |
|      - | 2534 | ` */` |
|     36 | 2535 | `static int DpConstructInto(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,ph7_value **apArg,` |
|      - | 2536 | `	const char *zIsoStartClass)` |
|      1 | 2537 | `{` |
|     37 | 2538 | `	ph7_vm *pVm = pCtx->pVm;` |
|     37 | 2539 | `	sxi64 iOptions = 0;` |
|      - | 2540 | `	static const char *zBadArgs =` |
|      - | 2541 | `		"DatePeriod::__construct() accepts (DateTimeInterface, DateInterval, int [, int]), "` |
|      - | 2542 | `		"or (DateTimeInterface, DateInterval, DateTime [, int]), or (string [, int]) as arguments";` |
|     37 | 2543 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 2544 | `		return PH7_VmThrowException(pCtx,"TypeError","%s",zBadArgs);` |
|      - | 2545 | `	}` |
|     37 | 2546 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|      - | 2547 | `		/* The ISO-8601 form: "R<n>/<start>/<duration>". php's second argument is` |
|      - | 2548 | `		 * then the OPTIONS bitmask, not an interval. */` |
|     11 | 2549 | `		const char *zSpec = (const char *)SyBlobData(&apArg[0]->sBlob);` |
|     11 | 2550 | `		int nSpec = (int)SyBlobLength(&apArg[0]->sBlob);` |
|      - | 2551 | `		const char *zStart,*zDur;` |
|      - | 2552 | `		int nStart,nDur,k;` |
|     11 | 2553 | `		sxi64 nRec = 0;` |
|      - | 2554 | `		sxi64 aIv[6];` |
|      - | 2555 | `		dt_state sState;` |
|      - | 2556 | `		char zNameBuf[16];` |
|      - | 2557 | `		const char *zErr;` |
|      - | 2558 | `		int iPos,nDigits;` |
|      - | 2559 | `		char cAt;` |
|      - | 2560 | `		ph7_class_instance *pStart,*pIv;` |
|     11 | 2561 | `		if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_INT) ){` |
|    ! 0 | 2562 | `			iOptions = apArg[1]->x.iVal;` |
|    ! 0 | 2563 | `		}` |
|     11 | 2564 | `		if( nSpec < 2 \|\| zSpec[0] != 'R' ){` |
|    ! 0 | 2565 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 2566 | `				"Unknown or bad format (%.*s)",nSpec,zSpec);` |
|      - | 2567 | `		}` |
|     11 | 2568 | `		nDigits = DtIvDigits(&zSpec[1],&zSpec[nSpec],&nRec);` |
|     11 | 2569 | `		k = 1 + nDigits;` |
|     11 | 2570 | `		if( nDigits == 0 \|\| k >= nSpec \|\| zSpec[k] != '/' ){` |
|    ! 0 | 2571 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 2572 | `				"Unknown or bad format (%.*s)",nSpec,zSpec);` |
|      - | 2573 | `		}` |
|     11 | 2574 | `		zStart = &zSpec[k+1];` |
|     11 | 2575 | `		nStart = 0;` |
|    179 | 2576 | `		while( &zStart[nStart] < &zSpec[nSpec] && zStart[nStart] != '/' ){` |
|    169 | 2577 | `			nStart++;` |
|      1 | 2578 | `		}` |
|     11 | 2579 | `		if( &zStart[nStart] >= &zSpec[nSpec] ){` |
|    ! 0 | 2580 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|    ! 0 | 2581 | `				"Unknown or bad format (%.*s)",nSpec,zSpec);` |
|      - | 2582 | `		}` |
|     11 | 2583 | `		zDur = &zStart[nStart+1];` |
|     11 | 2584 | `		nDur = (int)(&zSpec[nSpec] - zDur);` |
|     15 | 2585 | `		if( DtInitState(pCtx,zStart,nStart,0,pVm->zDefTz,(int)pVm->nDefTz,&sState,` |
|     10 | 2586 | `			zNameBuf,sizeof(zNameBuf),&zErr,&iPos,&cAt) != 0` |
|     10 | 2587 | `		 \|\| DtIvParseIso(zDur,nDur,aIv) != 0 ){` |
|      4 | 2588 | `			return PH7_VmThrowException(pCtx,"DateMalformedPeriodStringException",` |
|      1 | 2589 | `				"Unknown or bad format (%.*s)",nSpec,zSpec);` |
|      - | 2590 | `		}` |
|      - | 2591 | `		/* php's two ISO entry points disagree on the class they build, and both` |
|      - | 2592 | ``		 * answers are load-bearing: `new DatePeriod("R2/...")` yields DateTime`` |
|      - | 2593 | `		 * where DatePeriod::createFromISO8601String() yields DateTimeImmutable. */` |
|      9 | 2594 | `		pStart = PH7_NewClassInstance(pVm,DtClass(pVm,zIsoStartClass));` |
|      9 | 2595 | `		pIv = PH7_NewClassInstance(pVm,DtClass(pVm,"DateInterval"));` |
|      9 | 2596 | `		if( pStart == 0 \|\| pIv == 0 ){` |
|    ! 0 | 2597 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2598 | `		}` |
|      9 | 2599 | `		DtStore(pVm,pStart,&sState);` |
|      9 | 2600 | `		DtIvStore(pVm,pIv,aIv);` |
|      9 | 2601 | `		PH7_NativeSetAttrObj(pVm,pThis,"start",pStart);` |
|      9 | 2602 | `		PH7_NativeSetAttrObj(pVm,pThis,"interval",pIv);` |
|      9 | 2603 | `		PH7_ClassInstanceUnref(pStart);` |
|      9 | 2604 | `		PH7_ClassInstanceUnref(pIv);` |
|      9 | 2605 | `		PH7_NativeSetAttrInt(pVm,pThis,"recurrences",nRec + 1);` |
|      5 | 2606 | `	}else{` |
|      - | 2607 | `		ph7_class_instance *pStart,*pIv,*pEnd;` |
|     26 | 2608 | `		if( !DtValueIsA(pVm,apArg[0],"DateTimeInterface")` |
|     26 | 2609 | `		 \|\| nArg < 3` |
|     25 | 2610 | `		 \|\| !DtValueIsA(pVm,apArg[1],"DateInterval")` |
|     24 | 2611 | `		 \|\| ((apArg[2]->iFlags & MEMOBJ_INT) == 0` |
|     16 | 2612 | `		     && !DtValueIsA(pVm,apArg[2],"DateTimeInterface")) ){` |
|      5 | 2613 | `			return PH7_VmThrowException(pCtx,"TypeError","%s",zBadArgs);` |
|      - | 2614 | `		}` |
|     23 | 2615 | `		if( nArg > 3 ){` |
|      9 | 2616 | `			iOptions = ph7_value_to_int64(apArg[3]);` |
|      4 | 2617 | `		}` |
|     23 | 2618 | `		pStart = PH7_CloneClassInstance((ph7_class_instance *)apArg[0]->x.pOther);` |
|     23 | 2619 | `		pIv = (ph7_class_instance *)apArg[1]->x.pOther;` |
|     23 | 2620 | `		if( pStart == 0 ){` |
|    ! 0 | 2621 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2622 | `		}` |
|     23 | 2623 | `		PH7_NativeSetAttrObj(pVm,pThis,"start",pStart);` |
|     23 | 2624 | `		PH7_ClassInstanceUnref(pStart);` |
|     23 | 2625 | `		PH7_NativeSetAttrObj(pVm,pThis,"interval",pIv);` |
|     23 | 2626 | `		if( apArg[2]->iFlags & MEMOBJ_INT ){` |
|     13 | 2627 | `			PH7_NativeSetAttrInt(pVm,pThis,"recurrences",apArg[2]->x.iVal + 1);` |
|      7 | 2628 | `		}else{` |
|     11 | 2629 | `			pEnd = PH7_CloneClassInstance((ph7_class_instance *)apArg[2]->x.pOther);` |
|     11 | 2630 | `			if( pEnd == 0 ){` |
|    ! 0 | 2631 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2632 | `			}` |
|     11 | 2633 | `			PH7_NativeSetAttrObj(pVm,pThis,"end",pEnd);` |
|     11 | 2634 | `			PH7_ClassInstanceUnref(pEnd);` |
|      - | 2635 | `		}` |
|      - | 2636 | `	}` |
|     31 | 2637 | `	PH7_NativeSetAttrBool(pVm,pThis,"include_start_date",(iOptions & 1) == 0);` |
|     31 | 2638 | `	PH7_NativeSetAttrBool(pVm,pThis,"include_end_date",(iOptions & 2) != 0);` |
|     31 | 2639 | `	return PH7_OK;` |
|     19 | 2640 | `}` |
|     32 | 2641 | `static int vm_builtin_DatePeriod_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2642 | `{` |
|     33 | 2643 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|     33 | 2644 | `	if( pThis == 0 ){` |
|    ! 0 | 2645 | `		return PH7_OK;` |
|      - | 2646 | `	}` |
|     33 | 2647 | `	return DpConstructInto(pCtx,pThis,nArg,apArg,"DateTime");` |
|     17 | 2648 | `}` |
|      - | 2649 | `/* DatePeriod::createFromISO8601String(string $specification, int $options = 0) */` |
|      4 | 2650 | `static int vm_builtin_DatePeriod_createFromISO8601String(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2651 | `{` |
|      5 | 2652 | `	ph7_vm *pVm = pCtx->pVm;` |
|      5 | 2653 | `	ph7_class *pClass = DtFactoryClass(pCtx,"DatePeriod");` |
|      - | 2654 | `	ph7_class_instance *pObj;` |
|      - | 2655 | `	sxi32 rc;` |
|      5 | 2656 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 2657 | `		return PH7_OK;` |
|      - | 2658 | `	}` |
|      5 | 2659 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|      5 | 2660 | `	if( pObj == 0 ){` |
|    ! 0 | 2661 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2662 | `	}` |
|      - | 2663 | `	/* php's factory IS the constructor, with the same overloaded argument shape. */` |
|      5 | 2664 | `	rc = DpConstructInto(pCtx,pObj,nArg,apArg,"DateTimeImmutable");` |
|      5 | 2665 | `	if( rc != PH7_OK \|\| pCtx->nThrowRc != 0 ){` |
|    ! 0 | 2666 | `		PH7_ClassInstanceUnref(pObj);` |
|    ! 0 | 2667 | `		return rc;` |
|      - | 2668 | `	}` |
|      5 | 2669 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      5 | 2670 | `	return PH7_OK;` |
|      3 | 2671 | `}` |
|     10 | 2672 | `static int vm_builtin_DatePeriod_getStartDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2673 | `{` |
|     11 | 2674 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      5 | 2675 | `	SXUNUSED(nArg);` |
|      5 | 2676 | `	SXUNUSED(apArg);` |
|     11 | 2677 | `	if( pThis ){` |
|     11 | 2678 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"start");` |
|     11 | 2679 | `		if( pVal ){` |
|     11 | 2680 | `			ph7_result_value(pCtx,pVal);` |
|      5 | 2681 | `		}` |
|      5 | 2682 | `	}` |
|     11 | 2683 | `	return PH7_OK;` |
|      1 | 2684 | `}` |
|      4 | 2685 | `static int vm_builtin_DatePeriod_getEndDate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2686 | `{` |
|      5 | 2687 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      2 | 2688 | `	SXUNUSED(nArg);` |
|      2 | 2689 | `	SXUNUSED(apArg);` |
|      5 | 2690 | `	if( pThis ){` |
|      5 | 2691 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"end");` |
|      5 | 2692 | `		if( pVal ){` |
|      5 | 2693 | `			ph7_result_value(pCtx,pVal);` |
|      2 | 2694 | `		}` |
|      2 | 2695 | `	}` |
|      5 | 2696 | `	return PH7_OK;` |
|      1 | 2697 | `}` |
|      2 | 2698 | `static int vm_builtin_DatePeriod_getDateInterval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2699 | `{` |
|      3 | 2700 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      1 | 2701 | `	SXUNUSED(nArg);` |
|      1 | 2702 | `	SXUNUSED(apArg);` |
|      3 | 2703 | `	if( pThis ){` |
|      3 | 2704 | `		ph7_value *pVal = PH7_NativeAttr(pThis,"interval");` |
|      3 | 2705 | `		if( pVal ){` |
|      3 | 2706 | `			ph7_result_value(pCtx,pVal);` |
|      1 | 2707 | `		}` |
|      1 | 2708 | `	}` |
|      3 | 2709 | `	return PH7_OK;` |
|      1 | 2710 | `}` |
|      - | 2711 | `/*` |
|      - | 2712 | ` * DatePeriod::getRecurrences() -- php answers NULL for a period bounded by an END` |
|      - | 2713 | `` * DATE and the recurrence COUNT otherwise, which is `recurrences - 1` (php stores`` |
|      - | 2714 | ` * the count of dates, one more than the recurrences). No private slot needed.` |
|      - | 2715 | ` */` |
|      6 | 2716 | `static int vm_builtin_DatePeriod_getRecurrences(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2717 | `{` |
|      7 | 2718 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      3 | 2719 | `	SXUNUSED(nArg);` |
|      3 | 2720 | `	SXUNUSED(apArg);` |
|      7 | 2721 | `	if( pThis == 0 \|\| PH7_NativeAttrObj(pThis,"end") != 0 ){` |
|      3 | 2722 | `		ph7_result_null(pCtx);` |
|      3 | 2723 | `		return PH7_OK;` |
|      - | 2724 | `	}` |
|      5 | 2725 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,"recurrences") - 1);` |
|      5 | 2726 | `	return PH7_OK;` |
|      4 | 2727 | `}` |
|      - | 2728 | `/*` |
|      - | 2729 | ` * The period walk, expressed as the vtable an InternalIterator drives (oo_native.c).` |
|      - | 2730 | ` * It uses the shared cursor slots: SRC is the period, CUR the date the cursor sits` |
|      - | 2731 | ` * on, KEY the emitted position and POS the loop counter (which differs from KEY,` |
|      - | 2732 | ` * since an excluded start date is stepped over without emitting one).` |
|      - | 2733 | ` */` |
|      - | 2734 | `#define DP_IT_STEP PH7_NATIVE_IT_POS` |
|      - | 2735 | `/* One interval step from a date object: a NEW object, so a value already handed` |
|      - | 2736 | ` * to the caller is never mutated underneath it (php's iterator answers a fresh` |
|      - | 2737 | ` * object per position too). */` |
|    102 | 2738 | `static ph7_class_instance * DpAdvance(ph7_vm *pVm,ph7_class_instance *pCur,` |
|      - | 2739 | `	ph7_class_instance *pIv)` |
|      1 | 2740 | `{` |
|    103 | 2741 | `	ph7_class_instance *pNext = PH7_CloneClassInstance(pCur);` |
|    103 | 2742 | `	int iSign = PH7_NativeAttrInt(pIv,"invert") ? -1 : 1;` |
|    103 | 2743 | `	if( pNext == 0 ){` |
|    ! 0 | 2744 | `		return 0;` |
|      - | 2745 | `	}` |
|    205 | 2746 | `	PH7_NativeSetAttrInt(&(*pVm),pNext,DT_TS,` |
|    153 | 2747 | `		DtCivilAdd(PH7_NativeAttrInt(pCur,DT_TS),(sxi32)PH7_NativeAttrInt(pCur,DT_OFF),` |
|     51 | 2748 | `			PH7_NativeAttrInt(pIv,"y"),PH7_NativeAttrInt(pIv,"m"),PH7_NativeAttrInt(pIv,"d"),` |
|     51 | 2749 | `			PH7_NativeAttrInt(pIv,"h"),PH7_NativeAttrInt(pIv,"i"),PH7_NativeAttrInt(pIv,"s"),iSign));` |
|    103 | 2750 | `	return pNext;` |
|     52 | 2751 | `}` |
|      - | 2752 | `/*` |
|      - | 2753 | ` * Settle the cursor on the next date the period EMITS, mirroring the generator` |
|      - | 2754 | ` * this replaced: a start excluded by EXCLUDE_START_DATE is stepped over, an end` |
|      - | 2755 | ` * date stops the walk (inclusively under INCLUDE_END_DATE) and a recurrence count` |
|      - | 2756 | ` * bounds the number of steps instead.` |
|      - | 2757 | ` */` |
|    154 | 2758 | `static void DpSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 | 2759 | `{` |
|    155 | 2760 | `	ph7_class_instance *pPeriod = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|      - | 2761 | `	ph7_class_instance *pEnd,*pIv;` |
|      - | 2762 | `	int bInclStart,bInclEnd;` |
|      - | 2763 | `	sxi64 nTotal;` |
|    155 | 2764 | `	if( pPeriod == 0 ){` |
|    ! 0 | 2765 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 2766 | `		return;` |
|      - | 2767 | `	}` |
|    155 | 2768 | `	pEnd = PH7_NativeAttrObj(pPeriod,"end");` |
|    155 | 2769 | `	pIv = PH7_NativeAttrObj(pPeriod,"interval");` |
|    155 | 2770 | `	bInclStart = PH7_NativeAttrTruthy(pPeriod,"include_start_date");` |
|    155 | 2771 | `	bInclEnd = PH7_NativeAttrTruthy(pPeriod,"include_end_date");` |
|    155 | 2772 | `	nTotal = PH7_NativeAttrInt(pPeriod,"recurrences") + (bInclEnd ? 1 : 0);` |
|     85 | 2773 | `	for(;;){` |
|    163 | 2774 | `		ph7_class_instance *pCur = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_CUR);` |
|    163 | 2775 | `		sxi64 iStep = PH7_NativeAttrInt(pIt,DP_IT_STEP);` |
|      - | 2776 | `		ph7_class_instance *pNext;` |
|    163 | 2777 | `		if( pCur == 0 ){` |
|    ! 0 | 2778 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 2779 | `			return;` |
|      - | 2780 | `		}` |
|    163 | 2781 | `		if( pEnd != 0 ){` |
|     47 | 2782 | `			sxi64 iTs = PH7_NativeAttrInt(pCur,DT_TS);` |
|     47 | 2783 | `			sxi64 iEndTs = PH7_NativeAttrInt(pEnd,DT_TS);` |
|     47 | 2784 | `			if( bInclEnd ? (iTs > iEndTs) : (iTs >= iEndTs) ){` |
|      9 | 2785 | `				PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|      9 | 2786 | `				return;` |
|      1 | 2787 | `			}` |
|    136 | 2788 | `		}else if( iStep >= nTotal ){` |
|     21 | 2789 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|     21 | 2790 | `			return;` |
|      - | 2791 | `		}` |
|    135 | 2792 | `		if( iStep > 0 \|\| bInclStart ){` |
|    127 | 2793 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|    127 | 2794 | `			return;` |
|      - | 2795 | `		}` |
|      - | 2796 | `		/* The excluded start: step over it without emitting a key. */` |
|      9 | 2797 | `		if( pIv == 0 ){` |
|    ! 0 | 2798 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 2799 | `			return;` |
|      - | 2800 | `		}` |
|      9 | 2801 | `		pNext = DpAdvance(&(*pVm),pCur,pIv);` |
|      9 | 2802 | `		if( pNext == 0 ){` |
|    ! 0 | 2803 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 2804 | `			return;` |
|      - | 2805 | `		}` |
|      9 | 2806 | `		PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_CUR,pNext);` |
|      9 | 2807 | `		PH7_ClassInstanceUnref(pNext);` |
|      9 | 2808 | `		PH7_NativeSetAttrInt(&(*pVm),pIt,DP_IT_STEP,iStep + 1);` |
|      1 | 2809 | `	}` |
|     78 | 2810 | `}` |
|     60 | 2811 | `static void DpRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 | 2812 | `{` |
|      - | 2813 | `	ph7_class_instance *pPeriod,*pStart,*pCur;` |
|     61 | 2814 | `	pPeriod = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_SRC);` |
|     61 | 2815 | `	pStart = pPeriod ? PH7_NativeAttrObj(pPeriod,"start") : 0;` |
|     61 | 2816 | `	pCur = pStart ? PH7_CloneClassInstance(pStart) : 0;` |
|     61 | 2817 | `	if( pCur == 0 ){` |
|    ! 0 | 2818 | `		PH7_NativeSetAttrBool(pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 2819 | `		return;` |
|      - | 2820 | `	}` |
|     61 | 2821 | `	PH7_NativeSetAttrObj(pVm,pThis,PH7_NATIVE_IT_CUR,pCur);` |
|     61 | 2822 | `	PH7_ClassInstanceUnref(pCur);` |
|     61 | 2823 | `	PH7_NativeSetAttrInt(pVm,pThis,PH7_NATIVE_IT_KEY,0);` |
|     61 | 2824 | `	PH7_NativeSetAttrInt(pVm,pThis,DP_IT_STEP,0);` |
|     61 | 2825 | `	DpSettle(pVm,pThis);` |
|     31 | 2826 | `}` |
|     94 | 2827 | `static void DpNext(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 | 2828 | `{` |
|      - | 2829 | `	ph7_class_instance *pPeriod,*pIv,*pCur,*pNext;` |
|     95 | 2830 | `	pPeriod = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_SRC);` |
|     95 | 2831 | `	pIv = pPeriod ? PH7_NativeAttrObj(pPeriod,"interval") : 0;` |
|     95 | 2832 | `	pCur = PH7_NativeAttrObj(pThis,PH7_NATIVE_IT_CUR);` |
|     95 | 2833 | `	pNext = (pIv && pCur) ? DpAdvance(pVm,pCur,pIv) : 0;` |
|     95 | 2834 | `	if( pNext == 0 ){` |
|    ! 0 | 2835 | `		PH7_NativeSetAttrBool(pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|    ! 0 | 2836 | `		return;` |
|      - | 2837 | `	}` |
|     95 | 2838 | `	PH7_NativeSetAttrObj(pVm,pThis,PH7_NATIVE_IT_CUR,pNext);` |
|     95 | 2839 | `	PH7_ClassInstanceUnref(pNext);` |
|     95 | 2840 | `	PH7_NativeSetAttrInt(pVm,pThis,DP_IT_STEP,PH7_NativeAttrInt(pThis,DP_IT_STEP) + 1);` |
|     95 | 2841 | `	PH7_NativeSetAttrInt(pVm,pThis,PH7_NATIVE_IT_KEY,PH7_NativeAttrInt(pThis,PH7_NATIVE_IT_KEY) + 1);` |
|     95 | 2842 | `	DpSettle(pVm,pThis);` |
|     48 | 2843 | `}` |
|      - | 2844 | `static const PH7_NativeIterVtab sDpIterVtab = { DpRewind, DpNext };` |
|      - | 2845 | `/*` |
|      - | 2846 | ` * DatePeriod::getIterator(): Iterator` |
|      - | 2847 | ` *` |
|      - | 2848 | ` * This was a PHP GENERATOR, the one thing a C body cannot be. php answers an` |
|      - | 2849 | ` * InternalIterator here, so PHL answers the shared one (oo_native.c) driven by` |
|      - | 2850 | `` * the vtable above -- and stops diverging on `get_class($period->getIterator())`.`` |
|      - | 2851 | ` * A fresh one per call, as php's is.` |
|      - | 2852 | ` */` |
|     32 | 2853 | `static int vm_builtin_DatePeriod_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2854 | `{` |
|     33 | 2855 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 2856 | `	ph7_class_instance *pIt;` |
|     16 | 2857 | `	SXUNUSED(nArg);` |
|     16 | 2858 | `	SXUNUSED(apArg);` |
|     33 | 2859 | `	if( pThis == 0 ){` |
|    ! 0 | 2860 | `		return PH7_OK;` |
|      - | 2861 | `	}` |
|     33 | 2862 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|     33 | 2863 | `	if( pIt == 0 ){` |
|    ! 0 | 2864 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2865 | `	}` |
|     33 | 2866 | `	PH7_NativeResultObject(pCtx,pIt);` |
|     33 | 2867 | `	return PH7_OK;` |
|     17 | 2868 | `}` |
|      - | 2869 | `/*` |
|      - | 2870 | ` * ---------------------------------------------------------------------------` |
|      - | 2871 | ` * The procedural date API.` |
|      - | 2872 | ` *` |
|      - | 2873 | ` * php's aliases are functions in their own right, not forwards: they reach the` |
|      - | 2874 | ` * same implementation the methods do, so an overridden method in a subclass is` |
|      - | 2875 | ` * NOT what they call, and the ones that can fail WARN and answer false where the` |
|      - | 2876 | ` * method throws. Each owes aBuiltinSig[] a row (vm_arg_check.c).` |
|      - | 2877 | ` * ---------------------------------------------------------------------------` |
|      - | 2878 | ` */` |
|      - | 2879 | `/* The receiver argument of a procedural alias (already type-screened by its row). */` |
|     48 | 2880 | `static ph7_class_instance * DtArgObj(int nArg,ph7_value **apArg,int iArg)` |
|      1 | 2881 | `{` |
|     49 | 2882 | `	if( iArg >= nArg \|\| (apArg[iArg]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 2883 | `		return 0;` |
|      - | 2884 | `	}` |
|     49 | 2885 | `	return (ph7_class_instance *)apArg[iArg]->x.pOther;` |
|     25 | 2886 | `}` |
|      - | 2887 | `/* Answer the receiver itself, the way every mutating alias does. */` |
|      8 | 2888 | `static void DtResultArg(ph7_context *pCtx,ph7_value **apArg)` |
|      1 | 2889 | `{` |
|      9 | 2890 | `	ph7_result_value(pCtx,apArg[0]);` |
|      9 | 2891 | `}` |
|      - | 2892 | `/* date_create()/date_create_immutable(): php answers false on a parse failure and` |
|      - | 2893 | ` * says nothing -- the constructor's exception does not escape the alias. */` |
|     28 | 2894 | `static int DtProcCreate(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zClass)` |
|      1 | 2895 | `{` |
|     29 | 2896 | `	ph7_vm *pVm = pCtx->pVm;` |
|     29 | 2897 | `	ph7_class *pClass = DtClass(pVm,zClass);` |
|      - | 2898 | `	ph7_class_instance *pObj;` |
|     29 | 2899 | `	const char *zIn = "now",*zZone;` |
|     29 | 2900 | `	int nIn = 3,nZone,iPos;` |
|     29 | 2901 | `	sxi32 iZoneOff = 0;` |
|      - | 2902 | `	dt_state sState;` |
|      - | 2903 | `	char zNameBuf[16],cAt;` |
|      - | 2904 | `	const char *zErr;` |
|     29 | 2905 | `	if( pClass == 0 ){` |
|    ! 0 | 2906 | `		return PH7_OK;` |
|      - | 2907 | `	}` |
|     29 | 2908 | `	zZone = pVm->zDefTz;` |
|     29 | 2909 | `	nZone = (int)pVm->nDefTz;` |
|     29 | 2910 | `	if( nArg > 0 ){` |
|     29 | 2911 | `		zIn = ph7_value_to_string(apArg[0],&nIn);` |
|     14 | 2912 | `	}` |
|     29 | 2913 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|      7 | 2914 | `		DtZoneOf(apArg[1],&iZoneOff,&zZone,&nZone);` |
|      3 | 2915 | `	}` |
|     28 | 2916 | `	if( DtInitState(pCtx,zIn,nIn,iZoneOff,zZone,nZone,&sState,zNameBuf,sizeof(zNameBuf),` |
|     15 | 2917 | `		&zErr,&iPos,&cAt) != 0 ){` |
|      5 | 2918 | `		DtLastErrOne(pVm,iPos,zErr);` |
|      5 | 2919 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2920 | `		return PH7_OK;` |
|      - | 2921 | `	}` |
|     25 | 2922 | `	DtLastErrClear(pVm);` |
|     25 | 2923 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     25 | 2924 | `	if( pObj == 0 ){` |
|    ! 0 | 2925 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2926 | `	}` |
|     25 | 2927 | `	DtStore(pVm,pObj,&sState);` |
|     25 | 2928 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     25 | 2929 | `	return PH7_OK;` |
|     15 | 2930 | `}` |
|     24 | 2931 | `static int vm_builtin_date_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2932 | `{` |
|     25 | 2933 | `	return DtProcCreate(pCtx,nArg,apArg,"DateTime");` |
|      1 | 2934 | `}` |
|      4 | 2935 | `static int vm_builtin_date_create_immutable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2936 | `{` |
|      5 | 2937 | `	return DtProcCreate(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 2938 | `}` |
|     10 | 2939 | `static int vm_builtin_date_create_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2940 | `{` |
|     11 | 2941 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTime");` |
|      1 | 2942 | `}` |
|      4 | 2943 | `static int vm_builtin_date_create_immutable_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2944 | `{` |
|      5 | 2945 | `	return DtCreateFromFormat(pCtx,nArg,apArg,"DateTimeImmutable");` |
|      1 | 2946 | `}` |
|      6 | 2947 | `static int vm_builtin_date_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2948 | `{` |
|      7 | 2949 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      - | 2950 | `	const char *zFmt;` |
|      - | 2951 | `	int nFmt;` |
|      7 | 2952 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|    ! 0 | 2953 | `		return PH7_OK;` |
|      - | 2954 | `	}` |
|      7 | 2955 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|      7 | 2956 | `	DtFormatOf(pCtx,pObj,zFmt,nFmt);` |
|      7 | 2957 | `	return PH7_OK;` |
|      4 | 2958 | `}` |
|      - | 2959 | `/* date_modify(): php WARNS and answers false where DateTime::modify() throws. */` |
|    ! 0 | 2960 | `static int vm_builtin_date_modify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2961 | `{` |
|    ! 0 | 2962 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      - | 2963 | `	const char *zMod,*zErr;` |
|      - | 2964 | `	int nMod,iPos,iErrPos;` |
|      - | 2965 | `	char cAt;` |
|    ! 0 | 2966 | `	sxi64 iTs = 0;` |
|    ! 0 | 2967 | `	sxi32 iOff = 0;` |
|    ! 0 | 2968 | `	int bOffSet = 0,uSec = 0;` |
|    ! 0 | 2969 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|    ! 0 | 2970 | `		return PH7_OK;` |
|      - | 2971 | `	}` |
|    ! 0 | 2972 | `	zMod = ph7_value_to_string(apArg[1],&nMod);` |
|    ! 0 | 2973 | `	iErrPos = DtParse(zMod,nMod,PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),` |
|      - | 2974 | `		&iTs,&iOff,&bOffSet,&uSec);` |
|    ! 0 | 2975 | `	if( iErrPos != 0 ){` |
|    ! 0 | 2976 | `		zErr = DtParseErr(zMod,nMod,iErrPos,&iPos,&cAt);` |
|    ! 0 | 2977 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 2978 | `			"date_modify(): Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    ! 0 | 2979 | `			nMod,zMod,iPos,cAt,zErr);` |
|    ! 0 | 2980 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2981 | `		return PH7_OK;` |
|      - | 2982 | `	}` |
|    ! 0 | 2983 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,iTs);` |
|    ! 0 | 2984 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 2985 | `	return PH7_OK;` |
|    ! 0 | 2986 | `}` |
|      6 | 2987 | `static int DtProcAddSub(ph7_context *pCtx,int nArg,ph7_value **apArg,int iSign)` |
|      1 | 2988 | `{` |
|      7 | 2989 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      7 | 2990 | `	ph7_class_instance *pIv = DtArgObj(nArg,apArg,1);` |
|      7 | 2991 | `	if( pObj == 0 \|\| pIv == 0 ){` |
|    ! 0 | 2992 | `		return PH7_OK;` |
|      - | 2993 | `	}` |
|      7 | 2994 | `	if( PH7_NativeAttrInt(pIv,"invert") ){` |
|    ! 0 | 2995 | `		iSign = -iSign;` |
|    ! 0 | 2996 | `	}` |
|     13 | 2997 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,` |
|      9 | 2998 | `		DtCivilAdd(PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),` |
|      3 | 2999 | `			PH7_NativeAttrInt(pIv,"y"),PH7_NativeAttrInt(pIv,"m"),PH7_NativeAttrInt(pIv,"d"),` |
|      3 | 3000 | `			PH7_NativeAttrInt(pIv,"h"),PH7_NativeAttrInt(pIv,"i"),PH7_NativeAttrInt(pIv,"s"),iSign));` |
|      7 | 3001 | `	DtResultArg(pCtx,apArg);` |
|      7 | 3002 | `	return PH7_OK;` |
|      4 | 3003 | `}` |
|      4 | 3004 | `static int vm_builtin_date_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3005 | `{` |
|      5 | 3006 | `	return DtProcAddSub(pCtx,nArg,apArg,1);` |
|      1 | 3007 | `}` |
|      2 | 3008 | `static int vm_builtin_date_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3009 | `{` |
|      3 | 3010 | `	return DtProcAddSub(pCtx,nArg,apArg,-1);` |
|      1 | 3011 | `}` |
|      6 | 3012 | `static int vm_builtin_date_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3013 | `{` |
|      7 | 3014 | `	ph7_class_instance *pBase = DtArgObj(nArg,apArg,0);` |
|      7 | 3015 | `	ph7_class_instance *pTarget = DtArgObj(nArg,apArg,1);` |
|      7 | 3016 | `	int bAbsolute = 0;` |
|      7 | 3017 | `	if( pBase == 0 \|\| pTarget == 0 ){` |
|    ! 0 | 3018 | `		return PH7_OK;` |
|      - | 3019 | `	}` |
|      7 | 3020 | `	if( nArg > 2 ){` |
|    ! 0 | 3021 | `		bAbsolute = DtValueTruth(pCtx->pVm,apArg[2]);` |
|    ! 0 | 3022 | `	}` |
|      7 | 3023 | `	return DtDiffResult(pCtx,pBase,pTarget,bAbsolute);` |
|      4 | 3024 | `}` |
|      2 | 3025 | `static int vm_builtin_date_timestamp_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3026 | `{` |
|      3 | 3027 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      3 | 3028 | `	if( pObj ){` |
|      3 | 3029 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_TS));` |
|      1 | 3030 | `	}` |
|      3 | 3031 | `	return PH7_OK;` |
|      1 | 3032 | `}` |
|    ! 0 | 3033 | `static int vm_builtin_date_timestamp_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3034 | `{` |
|    ! 0 | 3035 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|    ! 0 | 3036 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|    ! 0 | 3037 | `		return PH7_OK;` |
|      - | 3038 | `	}` |
|    ! 0 | 3039 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,ph7_value_to_int64(apArg[1]));` |
|    ! 0 | 3040 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_US,0);` |
|    ! 0 | 3041 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 3042 | `	return PH7_OK;` |
|    ! 0 | 3043 | `}` |
|      2 | 3044 | `static int vm_builtin_date_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3045 | `{` |
|      3 | 3046 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      3 | 3047 | `	if( pObj == 0 ){` |
|    ! 0 | 3048 | `		return PH7_OK;` |
|      - | 3049 | `	}` |
|      3 | 3050 | `	return DtTimezoneResult(pCtx,pObj);` |
|      2 | 3051 | `}` |
|    ! 0 | 3052 | `static int vm_builtin_date_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3053 | `{` |
|    ! 0 | 3054 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|    ! 0 | 3055 | `	const char *zName = "UTC";` |
|    ! 0 | 3056 | `	int nName = 3;` |
|    ! 0 | 3057 | `	sxi32 iOff = 0;` |
|    ! 0 | 3058 | `	if( pObj == 0 \|\| nArg < 2 \|\| !DtZoneOf(apArg[1],&iOff,&zName,&nName) ){` |
|    ! 0 | 3059 | `		return PH7_OK;` |
|      - | 3060 | `	}` |
|    ! 0 | 3061 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_OFF,iOff);` |
|    ! 0 | 3062 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,DT_NAME,zName,nName);` |
|    ! 0 | 3063 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 3064 | `	return PH7_OK;` |
|    ! 0 | 3065 | `}` |
|      2 | 3066 | `static int vm_builtin_date_offset_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3067 | `{` |
|      3 | 3068 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      3 | 3069 | `	if( pObj ){` |
|      3 | 3070 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DT_OFF));` |
|      1 | 3071 | `	}` |
|      3 | 3072 | `	return PH7_OK;` |
|      1 | 3073 | `}` |
|    ! 0 | 3074 | `static int vm_builtin_date_date_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3075 | `{` |
|    ! 0 | 3076 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|    ! 0 | 3077 | `	if( pObj == 0 \|\| nArg < 4 ){` |
|    ! 0 | 3078 | `		return PH7_OK;` |
|      - | 3079 | `	}` |
|    ! 0 | 3080 | `	DtSetDateOf(pCtx,pObj,ph7_value_to_int64(apArg[1]),ph7_value_to_int(apArg[2]),` |
|    ! 0 | 3081 | `		ph7_value_to_int(apArg[3]));` |
|    ! 0 | 3082 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 3083 | `	return PH7_OK;` |
|    ! 0 | 3084 | `}` |
|    ! 0 | 3085 | `static int vm_builtin_date_time_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3086 | `{` |
|    ! 0 | 3087 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|    ! 0 | 3088 | `	if( pObj == 0 \|\| nArg < 3 ){` |
|    ! 0 | 3089 | `		return PH7_OK;` |
|      - | 3090 | `	}` |
|    ! 0 | 3091 | `	DtSetTimeOf(pCtx,pObj,ph7_value_to_int(apArg[1]),ph7_value_to_int(apArg[2]),` |
|    ! 0 | 3092 | `		nArg > 3 ? ph7_value_to_int(apArg[3]) : 0,` |
|    ! 0 | 3093 | `		nArg > 4 ? ph7_value_to_int64(apArg[4]) : 0);` |
|    ! 0 | 3094 | `	DtResultArg(pCtx,apArg);` |
|    ! 0 | 3095 | `	return PH7_OK;` |
|    ! 0 | 3096 | `}` |
|      2 | 3097 | `static int vm_builtin_date_isodate_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3098 | `{` |
|      3 | 3099 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      3 | 3100 | `	if( pObj == 0 \|\| nArg < 3 ){` |
|    ! 0 | 3101 | `		return PH7_OK;` |
|      - | 3102 | `	}` |
|      5 | 3103 | `	PH7_NativeSetAttrInt(pCtx->pVm,pObj,DT_TS,` |
|      3 | 3104 | `		DtIsoDate(PH7_NativeAttrInt(pObj,DT_TS),(sxi32)PH7_NativeAttrInt(pObj,DT_OFF),` |
|      2 | 3105 | `			ph7_value_to_int64(apArg[1]),ph7_value_to_int64(apArg[2]),` |
|      2 | 3106 | `			nArg > 3 ? ph7_value_to_int64(apArg[3]) : 1));` |
|      3 | 3107 | `	DtResultArg(pCtx,apArg);` |
|      3 | 3108 | `	return PH7_OK;` |
|      2 | 3109 | `}` |
|      - | 3110 | `/* date_interval_create_from_date_string(): warns and answers false where the` |
|      - | 3111 | ` * method throws. */` |
|      6 | 3112 | `static int vm_builtin_date_interval_create_from_date_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3113 | `{` |
|      7 | 3114 | `	const char *zIn,*zReason = "";` |
|      7 | 3115 | `	int nIn,iPos = 0;` |
|      7 | 3116 | `	char cAt = ' ';` |
|      - | 3117 | `	ph7_class_instance *pObj;` |
|      7 | 3118 | `	if( nArg < 1 ){` |
|    ! 0 | 3119 | `		return PH7_OK;` |
|      - | 3120 | `	}` |
|      7 | 3121 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|      7 | 3122 | `	pObj = DtIvFromDateString(pCtx,zIn,nIn,&iPos,&cAt,&zReason);` |
|      7 | 3123 | `	if( pObj == 0 ){` |
|    ! 0 | 3124 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 3125 | `			"date_interval_create_from_date_string(): Unknown or bad format (%.*s) "` |
|    ! 0 | 3126 | `			"at position %d (%c): %s",nIn,zIn,iPos,cAt,zReason);` |
|    ! 0 | 3127 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3128 | `		return PH7_OK;` |
|      - | 3129 | `	}` |
|      7 | 3130 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      7 | 3131 | `	return PH7_OK;` |
|      4 | 3132 | `}` |
|      4 | 3133 | `static int vm_builtin_date_interval_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3134 | `{` |
|      5 | 3135 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      - | 3136 | `	const char *zFmt;` |
|      - | 3137 | `	int nFmt;` |
|      5 | 3138 | `	if( pObj == 0 \|\| nArg < 2 ){` |
|    ! 0 | 3139 | `		return PH7_OK;` |
|      - | 3140 | `	}` |
|      5 | 3141 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|      5 | 3142 | `	DtIvFormat(pCtx,pObj,zFmt,nFmt);` |
|      5 | 3143 | `	return PH7_OK;` |
|      3 | 3144 | `}` |
|    ! 0 | 3145 | `static int vm_builtin_date_get_last_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3146 | `{` |
|    ! 0 | 3147 | `	return vm_builtin_DateTime_getLastErrors(pCtx,nArg,apArg);` |
|    ! 0 | 3148 | `}` |
|      - | 3149 | `/* timezone_open(): warns and answers false where the constructor throws. */` |
|      4 | 3150 | `static int vm_builtin_timezone_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3151 | `{` |
|      5 | 3152 | `	ph7_vm *pVm = pCtx->pVm;` |
|      5 | 3153 | `	ph7_class *pClass = DtClass(pVm,"DateTimeZone");` |
|      - | 3154 | `	ph7_class_instance *pObj;` |
|      - | 3155 | `	const char *zTz,*zName;` |
|      - | 3156 | `	int nTz,nName;` |
|      5 | 3157 | `	sxi32 iOff = 0;` |
|      - | 3158 | `	char zBuf[16];` |
|      5 | 3159 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3160 | `		return PH7_OK;` |
|      - | 3161 | `	}` |
|      5 | 3162 | `	zTz = ph7_value_to_string(apArg[0],&nTz);` |
|      5 | 3163 | `	if( DtZoneParse(zTz,nTz,&iOff,&zName,&nName,zBuf,sizeof(zBuf)) != 0 ){` |
|    ! 0 | 3164 | `		PH7_VmThrowWarningFmt(pVm,"timezone_open(): Unknown or bad timezone (%.*s)",nTz,zTz);` |
|    ! 0 | 3165 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3166 | `		return PH7_OK;` |
|      - | 3167 | `	}` |
|      5 | 3168 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|      5 | 3169 | `	if( pObj == 0 ){` |
|    ! 0 | 3170 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3171 | `	}` |
|      5 | 3172 | `	PH7_NativeSetAttrInt(pVm,pObj,DTZ_OFF,iOff);` |
|      5 | 3173 | `	PH7_NativeSetAttrStr(pVm,pObj,DTZ_NAME,zName,nName);` |
|      5 | 3174 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      5 | 3175 | `	return PH7_OK;` |
|      3 | 3176 | `}` |
|      4 | 3177 | `static int vm_builtin_timezone_name_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3178 | `{` |
|      5 | 3179 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      - | 3180 | `	const char *zName;` |
|      - | 3181 | `	int nName;` |
|      5 | 3182 | `	if( pObj == 0 ){` |
|    ! 0 | 3183 | `		return PH7_OK;` |
|      - | 3184 | `	}` |
|      5 | 3185 | `	PH7_NativeAttrStr(pObj,DTZ_NAME,&zName,&nName);` |
|      5 | 3186 | `	ph7_result_string(pCtx,zName,nName);` |
|      5 | 3187 | `	return PH7_OK;` |
|      3 | 3188 | `}` |
|      2 | 3189 | `static int vm_builtin_timezone_offset_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3190 | `{` |
|      3 | 3191 | `	ph7_class_instance *pObj = DtArgObj(nArg,apArg,0);` |
|      3 | 3192 | `	if( pObj ){` |
|      3 | 3193 | `		ph7_result_int64(pCtx,PH7_NativeAttrInt(pObj,DTZ_OFF));` |
|      1 | 3194 | `	}` |
|      3 | 3195 | `	return PH7_OK;` |
|      1 | 3196 | `}` |
|      - | 3197 | `/*` |
|      - | 3198 | ` * The four private slots a date object keeps its state in. Both classes declare` |
|      - | 3199 | `` * them: `trait __DtCoreT` had no native equivalent, and replaying the table is`` |
|      - | 3200 | `` * exactly what `use __DtCoreT` did.`` |
|      - | 3201 | ` */` |
|      - | 3202 | `#define DT_NATIVE_STATE_PROPS \` |
|      - | 3203 | `	{ DT_TS,   PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \` |
|      - | 3204 | `	{ DT_OFF,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }, \` |
|      - | 3205 | `	{ DT_NAME, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 }, 0 }, \` |
|      - | 3206 | `	{ DT_US,   PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 }` |
|      - | 3207 | `/*` |
|      - | 3208 | ` * The methods DateTime and DateTimeImmutable share -- the whole of the old trait` |
|      - | 3209 | ` * plus the mutators, whose one difference (write $this, or write a clone) the` |
|      - | 3210 | ` * bodies decide from the receiver's class. php's own signatures: they are the` |
|      - | 3211 | ` * single source of truth for arity, coercion and Reflection here, so the casts the` |
|      - | 3212 | `` * chunk wrote by hand (`(string)$format`, `(int)$timestamp`) are declared types now`` |
|      - | 3213 | ` * and the methods reject what php rejects.` |
|      - | 3214 | ` */` |
|      - | 3215 | `/* The methods DateTime and DateTimeImmutable share. CLS is the OWNING class name` |
|      - | 3216 | ` * as a string literal, because php's stubs write the concrete class rather than` |
|      - | 3217 | `` * `static` for the legacy mutators -- DateTime::add reports DateTime and`` |
|      - | 3218 | ` * DateTimeImmutable::add reports DateTimeImmutable. The two that php really does` |
|      - | 3219 | `` * declare `static` (setMicrosecond) and the two it declares for real rather than`` |
|      - | 3220 | ` * tentatively (getMicrosecond, and setMicrosecond again) are written as they are:` |
|      - | 3221 | `` * a leading `@` is php's @tentative-return-type, and nearly every method here has`` |
|      - | 3222 | ` * one. */` |
|      - | 3223 | `#define DT_NATIVE_SHARED_METHODS(CLS) \` |
|      - | 3224 | `	{ "__construct",     PH7_MOD_PUBLIC, "string $datetime = 'now', ?DateTimeZone $timezone = null", "", \` |
|      - | 3225 | `	  vm_builtin_DateTime_construct }, \` |
|      - | 3226 | `	{ "format",          PH7_MOD_PUBLIC, "string $format", "@string", vm_builtin_DateTime_format }, \` |
|      - | 3227 | `	{ "getTimestamp",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_DateTime_getTimestamp }, \` |
|      - | 3228 | `	{ "getMicrosecond",  PH7_MOD_PUBLIC, "", "int", vm_builtin_DateTime_getMicrosecond }, \` |
|      - | 3229 | `	{ "getOffset",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_DateTime_getOffset }, \` |
|      - | 3230 | `	{ "getTimezone",     PH7_MOD_PUBLIC, "", "@DateTimeZone\|false", vm_builtin_DateTime_getTimezone }, \` |
|      - | 3231 | `	{ "diff",            PH7_MOD_PUBLIC, "DateTimeInterface $targetObject, bool $absolute = false", \` |
|      - | 3232 | `	  "@DateInterval", vm_builtin_DateTime_diff }, \` |
|      - | 3233 | `	{ "modify",          PH7_MOD_PUBLIC, "string $modifier", "@" CLS, vm_builtin_DateTime_modify }, \` |
|      - | 3234 | `	{ "setTimestamp",    PH7_MOD_PUBLIC, "int $timestamp", "@" CLS, vm_builtin_DateTime_setTimestamp }, \` |
|      - | 3235 | `	{ "setMicrosecond",  PH7_MOD_PUBLIC, "int $microsecond", "static", vm_builtin_DateTime_setMicrosecond }, \` |
|      - | 3236 | `	{ "setTimezone",     PH7_MOD_PUBLIC, "DateTimeZone $timezone", "@" CLS, vm_builtin_DateTime_setTimezone }, \` |
|      - | 3237 | `	{ "setDate",         PH7_MOD_PUBLIC, "int $year, int $month, int $day", "@" CLS, \` |
|      - | 3238 | `	  vm_builtin_DateTime_setDate }, \` |
|      - | 3239 | `	{ "setTime",         PH7_MOD_PUBLIC, \` |
|      - | 3240 | `	  "int $hour, int $minute, int $second = 0, int $microsecond = 0", "@" CLS, \` |
|      - | 3241 | `	  vm_builtin_DateTime_setTime }, \` |
|      - | 3242 | `	{ "setISODate",      PH7_MOD_PUBLIC, "int $year, int $week, int $dayOfWeek = 1", "@" CLS, \` |
|      - | 3243 | `	  vm_builtin_DateTime_setISODate }, \` |
|      - | 3244 | `	{ "add",             PH7_MOD_PUBLIC, "DateInterval $interval", "@" CLS, vm_builtin_DateTime_add }, \` |
|      - | 3245 | `	{ "sub",             PH7_MOD_PUBLIC, "DateInterval $interval", "@" CLS, vm_builtin_DateTime_sub }, \` |
|      - | 3246 | `	{ "getLastErrors",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "@array\|false", \` |
|      - | 3247 | `	  vm_builtin_DateTime_getLastErrors }` |
|      - | 3248 | `/*` |
|      - | 3249 | ` * php's presentation for the date classes (ph7_class::xPresent).` |
|      - | 3250 | ` *` |
|      - | 3251 | ` * php keeps a timelib struct and SHOWS date/timezone_type/timezone; PHL keeps a` |
|      - | 3252 | ` * timestamp, an offset, a zone name and microseconds, all hidden. These build php's` |
|      - | 3253 | ` * shape out of that state, so var_dump/print_r, var_export and the (array) cast` |
|      - | 3254 | ` * agree with the oracle without changing what the C bodies read.` |
|      - | 3255 | ` *` |
|      - | 3256 | ` * timezone_type is php's own three-way tag: 1 = a fixed UTC OFFSET ("+02:00"),` |
|      - | 3257 | ` * 2 = an ABBREVIATION ("GMT", "Z"), 3 = an IDENTIFIER ("UTC", "Europe/Paris").` |
|      - | 3258 | ` * PHL accepts offsets, UTC, GMT and Z today; the identifier arm is written for the` |
|      - | 3259 | ` * whole rule so a tz database can only add names, never change the tagging.` |
|      - | 3260 | ` */` |
|     92 | 3261 | `static int DtZoneTypeOf(const char *zName,int nName)` |
|      1 | 3262 | `{` |
|     93 | 3263 | `	sxu32 nPos = 0;` |
|     93 | 3264 | `	if( nName > 0 && (zName[0] == '+' \|\| zName[0] == '-') ){` |
|     23 | 3265 | `		return 1;` |
|      - | 3266 | `	}` |
|     71 | 3267 | `	if( nName == 3 && SyStrnicmp(zName,"UTC",3) == 0 ){` |
|     63 | 3268 | `		return 3;` |
|      - | 3269 | `	}` |
|      9 | 3270 | `	if( nName > 0 && SyByteFind(zName,(sxu32)nName,'/',&nPos) == SXRET_OK ){` |
|    ! 0 | 3271 | `		return 3;` |
|      - | 3272 | `	}` |
|      9 | 3273 | `	return 2;` |
|     47 | 3274 | `}` |
|    234 | 3275 | `static void DtPresentPut(ph7_vm *pVm,ph7_value *pOut,const char *zKey,ph7_value *pVal)` |
|      1 | 3276 | `{` |
|      - | 3277 | `	ph7_value sKey;` |
|    235 | 3278 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    235 | 3279 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|    235 | 3280 | `	ph7_array_add_elem(pOut,&sKey,pVal);` |
|    235 | 3281 | `	PH7_MemObjRelease(&sKey);` |
|    235 | 3282 | `}` |
|     92 | 3283 | `static void DtPresentZone(ph7_vm *pVm,ph7_value *pOut,const char *zName,int nName)` |
|      1 | 3284 | `{` |
|      - | 3285 | `	ph7_value sVal;` |
|     93 | 3286 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,DtZoneTypeOf(zName,nName));` |
|     93 | 3287 | `	DtPresentPut(&(*pVm),pOut,"timezone_type",&sVal);` |
|     93 | 3288 | `	PH7_MemObjRelease(&sVal);` |
|     93 | 3289 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,0);` |
|     93 | 3290 | `	PH7_MemObjStringAppend(&sVal,zName,(sxu32)nName);` |
|     93 | 3291 | `	DtPresentPut(&(*pVm),pOut,"timezone",&sVal);` |
|     93 | 3292 | `	PH7_MemObjRelease(&sVal);` |
|     93 | 3293 | `}` |
|     50 | 3294 | `static sxi32 DtPresentDateTime(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      1 | 3295 | `{` |
|      - | 3296 | `	dt_state sState;` |
|      - | 3297 | `	Sytm sTm;` |
|      - | 3298 | `	char zZone[64];` |
|      - | 3299 | `	char zDate[64];` |
|      - | 3300 | `	ph7_value sVal;` |
|      - | 3301 | `	int nName;` |
|     25 | 3302 | `	SXUNUSED(bDebug); /* php shows the same three keys to both handlers */` |
|     51 | 3303 | `	DtLoad(pThis,&sState);` |
|     51 | 3304 | `	nName = sState.nName;` |
|     51 | 3305 | `	if( nName >= (int)sizeof(zZone) ){` |
|    ! 0 | 3306 | `		nName = (int)sizeof(zZone) - 1;` |
|    ! 0 | 3307 | `	}` |
|     51 | 3308 | `	if( nName > 0 ){` |
|     51 | 3309 | `		SyMemcpy(sState.zName,zZone,(sxu32)nName);` |
|     25 | 3310 | `	}` |
|     51 | 3311 | `	zZone[nName] = 0;` |
|     51 | 3312 | `	DtFillSytm(sState.iTs,sState.iOff,zZone,&sTm);` |
|      - | 3313 | `	/* php's fixed shape here, not a format string: "Y-m-d H:i:s.uuuuuu". */` |
|     76 | 3314 | `	SyBufferFormat(zDate,sizeof(zDate),"%04d-%02d-%02d %02d:%02d:%02d.%06d",` |
|     50 | 3315 | `		sTm.tm_year,sTm.tm_mon + 1,sTm.tm_mday,sTm.tm_hour,sTm.tm_min,sTm.tm_sec,` |
|     25 | 3316 | `		sState.uSec);` |
|     51 | 3317 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,0);` |
|     51 | 3318 | `	PH7_MemObjStringAppend(&sVal,zDate,(sxu32)SyStrlen(zDate));` |
|     51 | 3319 | `	DtPresentPut(&(*pVm),pOut,"date",&sVal);` |
|     51 | 3320 | `	PH7_MemObjRelease(&sVal);` |
|     51 | 3321 | `	DtPresentZone(&(*pVm),pOut,zZone,nName);` |
|     51 | 3322 | `	return SXRET_OK;` |
|      1 | 3323 | `}` |
|     42 | 3324 | `static sxi32 DtPresentTimeZone(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      1 | 3325 | `{` |
|     43 | 3326 | `	const char *zName = 0;` |
|     43 | 3327 | `	int nName = 0;` |
|     21 | 3328 | `	SXUNUSED(bDebug);` |
|     43 | 3329 | `	PH7_NativeAttrStr(pThis,DTZ_NAME,&zName,&nName);` |
|     43 | 3330 | `	DtPresentZone(&(*pVm),pOut,zName ? zName : "",nName);` |
|     43 | 3331 | `	return SXRET_OK;` |
|      1 | 3332 | `}` |
|      - | 3333 | `/*` |
|      - | 3334 | ` * ---------------------------------------------------------------------------` |
|      - | 3335 | ` * php's serialization pair for the three date classes whose state is HIDDEN.` |
|      - | 3336 | ` *` |
|      - | 3337 | ` * serialize() had been walking the engine slots, so a DateTime round-tripped as` |
|      - | 3338 | `` * `__dtTs`/`__dtOff`/`__dtName`/`__dtUs` and a payload php WROTE could not be read`` |
|      - | 3339 | `` * back at all -- `unserialize('O:8:"DateTime":3:{s:4:"date";…}')` found none of the`` |
|      - | 3340 | ` * names it wanted, silently kept the 1970 defaults and answered a valid object with` |
|      - | 3341 | ` * the wrong instant. php's answer is not a hidden-slot rule but a pair of methods:` |
|      - | 3342 | ` * __serialize() hands back the PRESENTED shape (date/timezone_type/timezone, the` |
|      - | 3343 | ` * same hash date_object_get_properties_for builds) and __unserialize() re-parses it,` |
|      - | 3344 | ` * so the payload is the class's public model rather than its storage.` |
|      - | 3345 | ` *` |
|      - | 3346 | ` * The four methods php declares are all here, because they are one contract:` |
|      - | 3347 | ` * __serialize/__unserialize is what serialize() uses, __wakeup reads a LEGACY` |
|      - | 3348 | ` * payload out of the object's own properties, and __set_state is what var_export's` |
|      - | 3349 | `` * `\DateTime::__set_state(array(…))` text evaluates to. All four fail with the same`` |
|      - | 3350 | `` * plain `Error`, and php's sentence for it names the class.`` |
|      - | 3351 | ` * ---------------------------------------------------------------------------` |
|      - | 3352 | ` */` |
|      - | 3353 | `/*` |
|      - | 3354 | ` * php's add_common_properties(): after the presented shape, the instance's own` |
|      - | 3355 | ` * php-visible slots -- a SUBCLASS's declared properties, which php serializes` |
|      - | 3356 | ` * alongside the internal state. A key the presented shape already wrote WINS` |
|      - | 3357 | ` * (zend_hash_add, not update), and a hidden engine slot is never a candidate:` |
|      - | 3358 | ` * this is the one walk in the date family that must skip them.` |
|      - | 3359 | ` */` |
|     28 | 3360 | `static void DtAddCommonProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|      1 | 3361 | `{` |
|      - | 3362 | `	SyHashEntry *pEntry;` |
|     29 | 3363 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    125 | 3364 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     97 | 3365 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     97 | 3366 | `		SyString *pName = &pVmAttr->pAttr->sName;` |
|      - | 3367 | `		ph7_value *pVal;` |
|      - | 3368 | `		ph7_value sKey;` |
|     97 | 3369 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|      - | 3370 | `			\|PH7_CLASS_ATTR_HIDDEN\|PH7_CLASS_ATTR_HOOK_VIRTUAL) ){` |
|     93 | 3371 | `			continue;` |
|      - | 3372 | `		}` |
|      5 | 3373 | `		if( ph7_array_fetch(pOut,pName->zString,(int)pName->nByte) != 0 ){` |
|    ! 0 | 3374 | `			continue;` |
|      - | 3375 | `		}` |
|      5 | 3376 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|      5 | 3377 | `		if( pVal == 0 ){` |
|    ! 0 | 3378 | `			continue;` |
|      - | 3379 | `		}` |
|      5 | 3380 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      5 | 3381 | `		PH7_MemObjStringAppend(&sKey,pName->zString,pName->nByte);` |
|      5 | 3382 | `		ph7_array_add_elem(pOut,&sKey,pVal);` |
|      5 | 3383 | `		PH7_MemObjRelease(&sKey);` |
|      1 | 3384 | `	}` |
|     29 | 3385 | `}` |
|      - | 3386 | `/* Build a payload array: the class's presented shape, then its own visible slots. */` |
|     26 | 3387 | `static int DtSerializePayload(ph7_context *pCtx,ph7_class_instance *pThis,int bZoneOnly,` |
|      - | 3388 | `	ph7_value *pOut)` |
|      1 | 3389 | `{` |
|     27 | 3390 | `	PH7_MemObjInit(pCtx->pVm,pOut);` |
|     27 | 3391 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|    ! 0 | 3392 | `		PH7_MemObjRelease(pOut);` |
|    ! 0 | 3393 | `		return -1;` |
|      - | 3394 | `	}` |
|     27 | 3395 | `	if( bZoneOnly ){` |
|     11 | 3396 | `		DtPresentTimeZone(pCtx->pVm,pThis,pOut,0);` |
|      6 | 3397 | `	}else{` |
|     17 | 3398 | `		DtPresentDateTime(pCtx->pVm,pThis,pOut,0);` |
|      - | 3399 | `	}` |
|     27 | 3400 | `	DtAddCommonProps(pCtx->pVm,pThis,pOut);` |
|     27 | 3401 | `	return 0;` |
|     14 | 3402 | `}` |
|      - | 3403 | ``/* php's `Error: Invalid serialization data for <Class> object`, the one refusal all`` |
|      - | 3404 | ` * four methods share. Named for the DECLARING class, not the receiver's. */` |
|     12 | 3405 | `static int DtSerialError(ph7_context *pCtx,const char *zClass)` |
|      1 | 3406 | `{` |
|     19 | 3407 | `	return PH7_VmThrowException(pCtx,"Error",` |
|      6 | 3408 | `		"Invalid serialization data for %s object",zClass);` |
|      1 | 3409 | `}` |
|      - | 3410 | ``/* The `array $data` parameter's own screen: the shared ZPP does not judge a scalar`` |
|      - | 3411 | `` * against a bare `array` (rule 18's §2 gap), so each caller words php's TypeError. */`` |
|     28 | 3412 | `static int DtCheckDataArg(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zClass)` |
|      1 | 3413 | `{` |
|      - | 3414 | `	char zBuf[64];` |
|     29 | 3415 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     29 | 3416 | `		return 0;` |
|      - | 3417 | `	}` |
|    ! 0 | 3418 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3419 | `		"%s::__unserialize(): Argument #1 ($data) must be of type array, %s given",` |
|    ! 0 | 3420 | `		zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|    ! 0 | 3421 | `	return -1;` |
|     15 | 3422 | `}` |
|      - | 3423 | `/*` |
|      - | 3424 | `` * php's php_date_timezone_initialize_from_hash(): `timezone_type` must be an int in`` |
|      - | 3425 | `` * 1..3 and `timezone` a string, and then the NAME alone rebuilds the zone -- the tag`` |
|      - | 3426 | ` * is validated but never trusted, which is why a payload tagged 1 whose name is` |
|      - | 3427 | ` * "UTC" restores a UTC zone rather than an offset one. Answers 0 on success.` |
|      - | 3428 | ` */` |
|     10 | 3429 | `static int DtZoneRestore(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 3430 | `{` |
|      - | 3431 | `	ph7_value *pType,*pName;` |
|      - | 3432 | `	const char *zTz,*zName;` |
|      - | 3433 | `	int nTz,nName;` |
|     11 | 3434 | `	sxi32 iOff = 0;` |
|      - | 3435 | `	sxi64 iType;` |
|      - | 3436 | `	char zBuf[16];` |
|     11 | 3437 | `	pType = ph7_array_fetch(pData,"timezone_type",(int)sizeof("timezone_type")-1);` |
|     11 | 3438 | `	if( pType == 0 \|\| (pType->iFlags & MEMOBJ_INT) == 0 ){` |
|      3 | 3439 | `		return -1;` |
|      - | 3440 | `	}` |
|      9 | 3441 | `	iType = pType->x.iVal;` |
|      9 | 3442 | `	if( iType < 1 \|\| iType > 3 ){` |
|      3 | 3443 | `		return -1;` |
|      - | 3444 | `	}` |
|      7 | 3445 | `	pName = ph7_array_fetch(pData,"timezone",(int)sizeof("timezone")-1);` |
|      7 | 3446 | `	if( pName == 0 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 3447 | `		return -1;` |
|      - | 3448 | `	}` |
|      7 | 3449 | `	zTz = (const char *)SyBlobData(&pName->sBlob);` |
|      7 | 3450 | `	nTz = (int)SyBlobLength(&pName->sBlob);` |
|      7 | 3451 | `	if( DtZoneParse(zTz,nTz,&iOff,&zName,&nName,zBuf,sizeof(zBuf)) != 0 ){` |
|    ! 0 | 3452 | `		return -1;` |
|      - | 3453 | `	}` |
|      7 | 3454 | `	PH7_NativeSetAttrInt(&(*pVm),pThis,DTZ_OFF,iOff);` |
|      7 | 3455 | `	PH7_NativeSetAttrStr(&(*pVm),pThis,DTZ_NAME,zName,nName);` |
|      7 | 3456 | `	return 0;` |
|      6 | 3457 | `}` |
|      - | 3458 | `/*` |
|      - | 3459 | `` * php's php_date_initialize_from_hash(): `date`, `timezone_type` and `timezone` must`` |
|      - | 3460 | ` * all be present and well-typed, and the tag must be one php writes.` |
|      - | 3461 | ` *` |
|      - | 3462 | ` * php restores an OFFSET or ABBREVIATION payload by CONCATENATING the two and running` |
|      - | 3463 | ` * its ordinary parser over "<date> <timezone>", and an IDENTIFIER one by resolving the` |
|      - | 3464 | ` * name first. Resolving the name for all three is the same answer here and does not` |
|      - | 3465 | `` * lean on the parser: `date` is always php's own `x-m-d H:i:s.u`, which carries no`` |
|      - | 3466 | ` * zone of its own, so nothing is left for the concatenated text to decide. It is also` |
|      - | 3467 | ` * the only spelling that works today -- PHL's parser accepts an offset only when it is` |
|      - | 3468 | ` * ATTACHED to the time ("…07+02:30", never "…07 +02:30") and accepts no trailing zone` |
|      - | 3469 | ` * NAME at all, so php's own round-trip string does not parse here (a §10 gap of its` |
|      - | 3470 | ` * own, recorded rather than worked around).` |
|      - | 3471 | ` *` |
|      - | 3472 | ` * Reading the NAME rather than the tag is also what php ends up doing: a payload` |
|      - | 3473 | ` * tagged 1 whose timezone is "UTC" restores a UTC zone in both engines.` |
|      - | 3474 | ` * Answers 0 on success.` |
|      - | 3475 | ` */` |
|     28 | 3476 | `static int DtDateRestore(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pData)` |
|      1 | 3477 | `{` |
|     29 | 3478 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 3479 | `	ph7_value *pDate,*pType,*pName;` |
|      - | 3480 | `	const char *zDate,*zTz,*zZone,*zErr;` |
|      - | 3481 | `	int nDate,nTz,nZone,iPos;` |
|     29 | 3482 | `	sxi32 iOff = 0;` |
|      - | 3483 | `	sxi64 iType;` |
|      - | 3484 | `	dt_state sState;` |
|      - | 3485 | `	char zNameBuf[16],zZoneBuf[16],cAt;` |
|     29 | 3486 | `	pDate = ph7_array_fetch(pData,"date",(int)sizeof("date")-1);` |
|     29 | 3487 | `	if( pDate == 0 \|\| (pDate->iFlags & MEMOBJ_STRING) == 0 ){` |
|      7 | 3488 | `		return -1;` |
|      - | 3489 | `	}` |
|     23 | 3490 | `	pType = ph7_array_fetch(pData,"timezone_type",(int)sizeof("timezone_type")-1);` |
|     23 | 3491 | `	if( pType == 0 \|\| (pType->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 3492 | `		return -1;` |
|      - | 3493 | `	}` |
|     23 | 3494 | `	pName = ph7_array_fetch(pData,"timezone",(int)sizeof("timezone")-1);` |
|     23 | 3495 | `	if( pName == 0 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 3496 | `		return -1;` |
|      - | 3497 | `	}` |
|     23 | 3498 | `	zDate = (const char *)SyBlobData(&pDate->sBlob);` |
|     23 | 3499 | `	nDate = (int)SyBlobLength(&pDate->sBlob);` |
|     23 | 3500 | `	zTz   = (const char *)SyBlobData(&pName->sBlob);` |
|     23 | 3501 | `	nTz   = (int)SyBlobLength(&pName->sBlob);` |
|     23 | 3502 | `	iType = pType->x.iVal;` |
|     23 | 3503 | `	if( iType < 1 \|\| iType > 3 ){` |
|    ! 0 | 3504 | `		return -1;` |
|      - | 3505 | `	}` |
|     23 | 3506 | `	if( DtZoneParse(zTz,nTz,&iOff,&zZone,&nZone,zZoneBuf,sizeof(zZoneBuf)) != 0 ){` |
|      3 | 3507 | `		return -1;` |
|      - | 3508 | `	}` |
|     20 | 3509 | `	if( DtInitState(pCtx,zDate,nDate,iOff,zZone,nZone,&sState,zNameBuf,sizeof(zNameBuf),` |
|     11 | 3510 | `		&zErr,&iPos,&cAt) != 0 ){` |
|    ! 0 | 3511 | `		return -1;` |
|      - | 3512 | `	}` |
|     21 | 3513 | `	DtStore(pVm,pThis,&sState);` |
|     21 | 3514 | `	return 0;` |
|     15 | 3515 | `}` |
|      - | 3516 | `/*` |
|      - | 3517 | ` * php's restore_custom_datetime_properties(): every payload key that is not part of` |
|      - | 3518 | ` * the internal shape becomes a property of the object. A REFERENCE is skipped, which` |
|      - | 3519 | ` * PHL cannot receive here (the pairs arrive already dereferenced).` |
|      - | 3520 | ` */` |
|      - | 3521 | `typedef struct dt_restore_ctx dt_restore_ctx;` |
|      - | 3522 | `struct dt_restore_ctx` |
|      - | 3523 | `{` |
|      - | 3524 | `	ph7_class_instance *pThis;` |
|      - | 3525 | `	int bZoneOnly;` |
|      - | 3526 | `};` |
|     52 | 3527 | `static int DtRestoreWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 | 3528 | `{` |
|     53 | 3529 | `	dt_restore_ctx *pRes = (dt_restore_ctx *)pUserData;` |
|      - | 3530 | `	const char *zKey;` |
|      - | 3531 | `	int nKey;` |
|     53 | 3532 | `	if( !ph7_value_is_string(pKey) ){` |
|    ! 0 | 3533 | `		return PH7_OK;` |
|      - | 3534 | `	}` |
|     53 | 3535 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     52 | 3536 | `	if( (nKey == 13 && SyMemcmp(zKey,"timezone_type",13) == 0)` |
|     39 | 3537 | `	 \|\| (nKey == 8  && SyMemcmp(zKey,"timezone",8) == 0)` |
|     33 | 3538 | `	 \|\| (!pRes->bZoneOnly && nKey == 4 && SyMemcmp(zKey,"date",4) == 0) ){` |
|     73 | 3539 | `		return PH7_OK;` |
|      - | 3540 | `	}` |
|      - | 3541 | `	/* A name the class does not DECLARE is dropped, which is what the engine's own` |
|      - | 3542 | `	 * unserialize does with one: PHL has no dynamic properties, where php creates` |
|      - | 3543 | `	 * (and deprecates) them. */` |
|     23 | 3544 | `	PH7_NativeSetProp(pRes->pThis->pVm,pRes->pThis,zKey,(sxu32)nKey,pVal);` |
|     23 | 3545 | `	return PH7_OK;` |
|     38 | 3546 | `}` |
|     26 | 3547 | `static void DtRestoreCustomProps(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pData,` |
|      - | 3548 | `	int bZoneOnly)` |
|      1 | 3549 | `{` |
|      - | 3550 | `	dt_restore_ctx sRes;` |
|     13 | 3551 | `	SXUNUSED(pVm);` |
|     27 | 3552 | `	if( (pData->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 3553 | `		return;` |
|      - | 3554 | `	}` |
|     27 | 3555 | `	sRes.pThis = pThis;` |
|     27 | 3556 | `	sRes.bZoneOnly = bZoneOnly;` |
|     27 | 3557 | `	ph7_array_walk(pData,DtRestoreWalk,&sRes);` |
|     14 | 3558 | `}` |
|      - | 3559 | `/* DateTimeZone::__serialize() / DateTime\|DateTimeImmutable::__serialize() */` |
|     26 | 3560 | `static int DtSerializeMagic(ph7_context *pCtx,int bZoneOnly)` |
|      1 | 3561 | `{` |
|     27 | 3562 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 3563 | `	ph7_value sOut;` |
|     27 | 3564 | `	if( pThis == 0 ){` |
|    ! 0 | 3565 | `		return PH7_OK;` |
|      - | 3566 | `	}` |
|     27 | 3567 | `	if( DtSerializePayload(pCtx,pThis,bZoneOnly,&sOut) != 0 ){` |
|    ! 0 | 3568 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3569 | `	}` |
|     27 | 3570 | `	ph7_result_value(pCtx,&sOut);` |
|     27 | 3571 | `	PH7_MemObjRelease(&sOut);` |
|     27 | 3572 | `	return PH7_OK;` |
|     14 | 3573 | `}` |
|     10 | 3574 | `static int vm_builtin_DateTimeZone_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3575 | `{` |
|      5 | 3576 | `	SXUNUSED(nArg);` |
|      5 | 3577 | `	SXUNUSED(apArg);` |
|     11 | 3578 | `	return DtSerializeMagic(pCtx,1);` |
|      1 | 3579 | `}` |
|     16 | 3580 | `static int vm_builtin_DateTime_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3581 | `{` |
|      8 | 3582 | `	SXUNUSED(nArg);` |
|      8 | 3583 | `	SXUNUSED(apArg);` |
|     17 | 3584 | `	return DtSerializeMagic(pCtx,0);` |
|      1 | 3585 | `}` |
|      - | 3586 | `/* __unserialize(array $data): restore the state, then the subclass's own slots. */` |
|     28 | 3587 | `static int DtUnserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg,int bZoneOnly,` |
|      - | 3588 | `	const char *zClass)` |
|      1 | 3589 | `{` |
|     29 | 3590 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 3591 | `	int rc;` |
|     29 | 3592 | `	if( pThis == 0 ){` |
|    ! 0 | 3593 | `		return PH7_OK;` |
|      - | 3594 | `	}` |
|     29 | 3595 | `	if( DtCheckDataArg(pCtx,nArg,apArg,zClass) != 0 ){` |
|    ! 0 | 3596 | `		return PH7_EXCEPTION;` |
|      - | 3597 | `	}` |
|     19 | 3598 | `	rc = bZoneOnly ? DtZoneRestore(pCtx->pVm,pThis,apArg[0])` |
|     24 | 3599 | `	               : DtDateRestore(pCtx,pThis,apArg[0]);` |
|     29 | 3600 | `	if( rc != 0 ){` |
|      9 | 3601 | `		return DtSerialError(pCtx,zClass);` |
|      - | 3602 | `	}` |
|     21 | 3603 | `	DtRestoreCustomProps(pCtx->pVm,pThis,apArg[0],bZoneOnly);` |
|     21 | 3604 | `	return PH7_OK;` |
|     15 | 3605 | `}` |
|      8 | 3606 | `static int vm_builtin_DateTimeZone_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3607 | `{` |
|      9 | 3608 | `	return DtUnserializeMagic(pCtx,nArg,apArg,1,"DateTimeZone");` |
|      1 | 3609 | `}` |
|     20 | 3610 | `static int vm_builtin_DateTime_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3611 | `{` |
|     21 | 3612 | `	return DtUnserializeMagic(pCtx,nArg,apArg,0,"DateTime");` |
|      1 | 3613 | `}` |
|    ! 0 | 3614 | `static int vm_builtin_DateTimeImmutable_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3615 | `{` |
|    ! 0 | 3616 | `	return DtUnserializeMagic(pCtx,nArg,apArg,0,"DateTimeImmutable");` |
|    ! 0 | 3617 | `}` |
|      - | 3618 | `/*` |
|      - | 3619 | ` * __wakeup(): the LEGACY payload, whose pairs the engine wrote into the object's own` |
|      - | 3620 | ` * properties before calling this. php reads Z_OBJPROP and restores from it, so an` |
|      - | 3621 | `` * object that has no such properties -- a plain `new DateTime` -- is exactly the`` |
|      - | 3622 | ` * failure case, and php raises the same Error there.` |
|      - | 3623 | ` */` |
|      2 | 3624 | `static int DtWakeupMagic(ph7_context *pCtx,int bZoneOnly,const char *zClass)` |
|      1 | 3625 | `{` |
|      3 | 3626 | `	ph7_class_instance *pThis = DtThis(pCtx);` |
|      - | 3627 | `	ph7_value sProps;` |
|      - | 3628 | `	int rc;` |
|      3 | 3629 | `	if( pThis == 0 ){` |
|    ! 0 | 3630 | `		return PH7_OK;` |
|      - | 3631 | `	}` |
|      3 | 3632 | `	PH7_MemObjInit(pCtx->pVm,&sProps);` |
|      3 | 3633 | `	if( PH7_MemObjToHashmap(&sProps) != SXRET_OK ){` |
|    ! 0 | 3634 | `		PH7_MemObjRelease(&sProps);` |
|    ! 0 | 3635 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3636 | `	}` |
|      3 | 3637 | `	DtAddCommonProps(pCtx->pVm,pThis,&sProps);` |
|      2 | 3638 | `	rc = bZoneOnly ? DtZoneRestore(pCtx->pVm,pThis,&sProps)` |
|      2 | 3639 | `	               : DtDateRestore(pCtx,pThis,&sProps);` |
|      3 | 3640 | `	PH7_MemObjRelease(&sProps);` |
|      3 | 3641 | `	if( rc != 0 ){` |
|      3 | 3642 | `		return DtSerialError(pCtx,zClass);` |
|      - | 3643 | `	}` |
|    ! 0 | 3644 | `	return PH7_OK;` |
|      2 | 3645 | `}` |
|    ! 0 | 3646 | `static int vm_builtin_DateTimeZone_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3647 | `{` |
|    ! 0 | 3648 | `	SXUNUSED(nArg);` |
|    ! 0 | 3649 | `	SXUNUSED(apArg);` |
|    ! 0 | 3650 | `	return DtWakeupMagic(pCtx,1,"DateTimeZone");` |
|    ! 0 | 3651 | `}` |
|      2 | 3652 | `static int vm_builtin_DateTime_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3653 | `{` |
|      1 | 3654 | `	SXUNUSED(nArg);` |
|      1 | 3655 | `	SXUNUSED(apArg);` |
|      3 | 3656 | `	return DtWakeupMagic(pCtx,0,"DateTime");` |
|      1 | 3657 | `}` |
|    ! 0 | 3658 | `static int vm_builtin_DateTimeImmutable_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3659 | `{` |
|    ! 0 | 3660 | `	SXUNUSED(nArg);` |
|    ! 0 | 3661 | `	SXUNUSED(apArg);` |
|    ! 0 | 3662 | `	return DtWakeupMagic(pCtx,0,"DateTimeImmutable");` |
|    ! 0 | 3663 | `}` |
|      - | 3664 | `/*` |
|      - | 3665 | ``  * __set_state(array $array): what var_export's `\DateTime::__set_state(array(…))` `` |
|      - | 3666 | ` * text evaluates to. php instantiates the class the method is DECLARED on and not` |
|      - | 3667 | `` * the called one -- `MyDateTime::__set_state(…)` answers a plain DateTime there --`` |
|      - | 3668 | ` * so this deliberately does not go through DtFactoryClass().` |
|      - | 3669 | ` */` |
|      8 | 3670 | `static int DtSetStateMagic(ph7_context *pCtx,int nArg,ph7_value **apArg,int bZoneOnly,` |
|      - | 3671 | `	const char *zClass)` |
|      1 | 3672 | `{` |
|      9 | 3673 | `	ph7_vm *pVm = pCtx->pVm;` |
|      9 | 3674 | `	ph7_class *pClass = DtClass(pVm,zClass);` |
|      - | 3675 | `	ph7_class_instance *pObj;` |
|      - | 3676 | `	int rc;` |
|      9 | 3677 | `	if( pClass == 0 ){` |
|    ! 0 | 3678 | `		return PH7_OK;` |
|      - | 3679 | `	}` |
|      9 | 3680 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - | 3681 | `		char zBuf[64];` |
|    ! 0 | 3682 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3683 | `			"%s::__set_state(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 | 3684 | `			zClass,nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|      - | 3685 | `	}` |
|      9 | 3686 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|      9 | 3687 | `	if( pObj == 0 ){` |
|    ! 0 | 3688 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3689 | `	}` |
|      6 | 3690 | `	rc = bZoneOnly ? DtZoneRestore(pVm,pObj,apArg[0])` |
|      7 | 3691 | `	               : DtDateRestore(pCtx,pObj,apArg[0]);` |
|      9 | 3692 | `	if( rc != 0 ){` |
|      3 | 3693 | `		PH7_ClassInstanceUnref(pObj);` |
|      3 | 3694 | `		return DtSerialError(pCtx,zClass);` |
|      - | 3695 | `	}` |
|      7 | 3696 | `	DtRestoreCustomProps(pVm,pObj,apArg[0],bZoneOnly);` |
|      7 | 3697 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      7 | 3698 | `	return PH7_OK;` |
|      5 | 3699 | `}` |
|      2 | 3700 | `static int vm_builtin_DateTimeZone_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3701 | `{` |
|      3 | 3702 | `	return DtSetStateMagic(pCtx,nArg,apArg,1,"DateTimeZone");` |
|      1 | 3703 | `}` |
|      6 | 3704 | `static int vm_builtin_DateTime_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3705 | `{` |
|      7 | 3706 | `	return DtSetStateMagic(pCtx,nArg,apArg,0,"DateTime");` |
|      1 | 3707 | `}` |
|    ! 0 | 3708 | `static int vm_builtin_DateTimeImmutable_setState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3709 | `{` |
|    ! 0 | 3710 | `	return DtSetStateMagic(pCtx,nArg,apArg,0,"DateTimeImmutable");` |
|    ! 0 | 3711 | `}` |
|      - | 3712 | `/* The four rows both date classes take. __serialize/__unserialize are php's only` |
|      - | 3713 | ` * NON-tentative internal returns in this family; __wakeup and __set_state carry the` |
|      - | 3714 | `` * `@`, and __set_state's return names the CONCRETE class php's stub writes. */`` |
|      - | 3715 | `#define DT_NATIVE_SERIAL_METHODS(CLS) \` |
|      - | 3716 | `	{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_DateTime_serialize }, \` |
|      - | 3717 | `	{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void", \` |
|      - | 3718 | `	  vm_builtin_##CLS##_unserialize }, \` |
|      - | 3719 | `	{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_##CLS##_wakeup }, \` |
|      - | 3720 | `	{ "__set_state",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@" #CLS, \` |
|      - | 3721 | `	  vm_builtin_##CLS##_setState }` |
|      - | 3722 | `/* php's DateTimeInterface constants, the whole of that interface's surface here` |
|      - | 3723 | ` * (its abstract METHODS are deliberately not declared: PH7_ClassImplement installs` |
|      - | 3724 | ` * a stub for every interface method an implementor lacks, so declaring them would` |
|      - | 3725 | ` * make every implementor abstract before its native methods are attached). */` |
|      - | 3726 | `#define DT_IFACE_CONST(NAME,VALUE) \` |
|      - | 3727 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, VALUE, 0.0 }` |
|      - | 3728 | `/*` |
|      - | 3729 | ` * Install the whole date family from C: the exceptions and DateTimeInterface, then` |
|      - | 3730 | ` * DateTimeZone / DateTime / DateTimeImmutable, DateInterval and DatePeriod, then the` |
|      - | 3731 | ` * procedural aliases. The InternalIterator its getIterator() answers is not declared` |
|      - | 3732 | ` * here — it is shared native machinery (oo_native.c), reached through the vtable` |
|      - | 3733 | ` * DatePeriod's spec row names.` |
|      - | 3734 | ` *` |
|      - | 3735 | ` * Called from PH7_VmInit inside the bCompilingBuiltin window, after the Reflection` |
|      - | 3736 | ` * install (Exception must exist). IteratorAggregate is attached AFTER DatePeriod's` |
|      - | 3737 | ` * methods exist, for the abstract-stub reason above; DateTimeInterface declares no` |
|      - | 3738 | ` * method, so it can ride the spec table.` |
|      - | 3739 | ` */` |
|   4670 | 3740 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
|      5 | 3741 | `{` |
|      - | 3742 | `	static const PH7_NativeConstDef aIfaceConst[] = {` |
|      - | 3743 | `		DT_IFACE_CONST("ATOM","Y-m-d\\TH:i:sP"),` |
|      - | 3744 | `		DT_IFACE_CONST("COOKIE","l, d-M-Y H:i:s T"),` |
|      - | 3745 | `		DT_IFACE_CONST("ISO8601","Y-m-d\\TH:i:sO"),` |
|      - | 3746 | `		DT_IFACE_CONST("ISO8601_EXPANDED","X-m-d\\TH:i:sP"),` |
|      - | 3747 | `		DT_IFACE_CONST("RFC822","D, d M y H:i:s O"),` |
|      - | 3748 | `		DT_IFACE_CONST("RFC850","l, d-M-y H:i:s T"),` |
|      - | 3749 | `		DT_IFACE_CONST("RFC1036","D, d M y H:i:s O"),` |
|      - | 3750 | `		DT_IFACE_CONST("RFC1123","D, d M Y H:i:s O"),` |
|      - | 3751 | `		DT_IFACE_CONST("RFC7231","D, d M Y H:i:s \\G\\M\\T"),` |
|      - | 3752 | `		DT_IFACE_CONST("RFC2822","D, d M Y H:i:s O"),` |
|      - | 3753 | `		DT_IFACE_CONST("RFC3339","Y-m-d\\TH:i:sP"),` |
|      - | 3754 | `		DT_IFACE_CONST("RFC3339_EXTENDED","Y-m-d\\TH:i:s.vP"),` |
|      - | 3755 | `		DT_IFACE_CONST("RSS","D, d M Y H:i:s O"),` |
|      - | 3756 | `		DT_IFACE_CONST("W3C","Y-m-d\\TH:i:sP"),` |
|      - | 3757 | `	};` |
|      - | 3758 | `	static const PH7_NativePropDef aZoneProp[] = {` |
|      - | 3759 | `		{ DTZ_OFF,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3760 | `		{ DTZ_NAME, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "UTC", 0.0 }, 0 },` |
|      - | 3761 | `	};` |
|      - | 3762 | `	static const PH7_NativeMethodDef aZoneMethod[] = {` |
|      - | 3763 | `		{ "__construct", PH7_MOD_PUBLIC, "string $timezone", "", vm_builtin_DateTimeZone_construct },` |
|      - | 3764 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_DateTimeZone_getName },` |
|      - | 3765 | `		{ "getOffset",   PH7_MOD_PUBLIC, "DateTimeInterface $datetime", "@int",` |
|      - | 3766 | `		  vm_builtin_DateTimeZone_getOffset },` |
|      - | 3767 | `		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_DateTimeZone_serialize },` |
|      - | 3768 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|      - | 3769 | `		  vm_builtin_DateTimeZone_unserialize },` |
|      - | 3770 | `		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DateTimeZone_wakeup },` |
|      - | 3771 | `		{ "__set_state",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array", "@DateTimeZone",` |
|      - | 3772 | `		  vm_builtin_DateTimeZone_setState },` |
|      - | 3773 | `	};` |
|      - | 3774 | `	static const PH7_NativePropDef aDtProp[] = { DT_NATIVE_STATE_PROPS };` |
|      - | 3775 | `	static const PH7_NativeMethodDef aDtMethod[] = {` |
|      - | 3776 | `		DT_NATIVE_SHARED_METHODS("DateTime"),` |
|      - | 3777 | `		{ "createFromFormat",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 3778 | `		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTime\|false",` |
|      - | 3779 | `		  vm_builtin_DateTime_createFromFormat },` |
|      - | 3780 | `		{ "createFromImmutable", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeImmutable $object", "@static",` |
|      - | 3781 | `		  vm_builtin_DateTime_copyOf },` |
|      - | 3782 | `		{ "createFromInterface", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTime",` |
|      - | 3783 | `		  vm_builtin_DateTime_copyOf },` |
|      - | 3784 | `		DT_NATIVE_SERIAL_METHODS(DateTime),` |
|      - | 3785 | `	};` |
|      - | 3786 | `	static const PH7_NativeMethodDef aImmMethod[] = {` |
|      - | 3787 | `		DT_NATIVE_SHARED_METHODS("DateTimeImmutable"),` |
|      - | 3788 | `		{ "createFromFormat",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 3789 | `		  "string $format, string $datetime, ?DateTimeZone $timezone = null", "@DateTimeImmutable\|false",` |
|      - | 3790 | `		  vm_builtin_DateTimeImmutable_createFromFormat },` |
|      - | 3791 | `		{ "createFromMutable",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTime $object", "@static",` |
|      - | 3792 | `		  vm_builtin_DateTimeImmutable_copyOf },` |
|      - | 3793 | `		{ "createFromInterface", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "DateTimeInterface $object", "DateTimeImmutable",` |
|      - | 3794 | `		  vm_builtin_DateTimeImmutable_copyOf },` |
|      - | 3795 | `		DT_NATIVE_SERIAL_METHODS(DateTimeImmutable),` |
|      - | 3796 | `	};` |
|      - | 3797 | `	static const PH7_NativePropDef aIvProp[] = {` |
|      - | 3798 | `		{ "y",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3799 | `		{ "m",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3800 | `		{ "d",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3801 | `		{ "h",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3802 | `		{ "i",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3803 | `		{ "s",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3804 | ``		/* php's `f` is a FLOAT; the chunk's `= 0` made it an int. */`` |
|      - | 3805 | `		{ "f",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_DOUBLE, 0, 0, 0.0 }, 0 },` |
|      - | 3806 | `		{ "invert",      PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 3807 | `		{ "days",        PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - | 3808 | `		{ "from_string", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - | 3809 | `	};` |
|      - | 3810 | `	static const PH7_NativeMethodDef aIvMethod[] = {` |
|      - | 3811 | `		{ "__construct", PH7_MOD_PUBLIC, "string $duration", "",` |
|      - | 3812 | `		  vm_builtin_DateInterval_construct },` |
|      - | 3813 | `		{ "createFromDateString", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $datetime", "@DateInterval",` |
|      - | 3814 | `		  vm_builtin_DateInterval_createFromDateString },` |
|      - | 3815 | `		{ "format",      PH7_MOD_PUBLIC, "string $format", "@string", vm_builtin_DateInterval_format },` |
|      - | 3816 | `	};` |
|      - | 3817 | `	/* php models all seven as VIRTUAL hooked properties, so it reports no default` |
|      - | 3818 | `	 * for any of them; PHL's are real slots and keep theirs, because a read before` |
|      - | 3819 | `	 * the first write must answer what php's getter answers rather than raise. The` |
|      - | 3820 | `	 * TYPE is what a spec row can state exactly — the virtual half is PLAN §7.4. */` |
|      - | 3821 | `	static const PH7_NativePropDef aDpProp[] = {` |
|      - | 3822 | `		{ "start",              PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 3823 | `		{ "current",            PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 3824 | `		{ "end",                PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateTimeInterface" },` |
|      - | 3825 | `		{ "interval",           PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?DateInterval" },` |
|      - | 3826 | `		{ "recurrences",        PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT,  1, 0, 0.0 }, "int" },` |
|      - | 3827 | `		{ "include_start_date", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, "bool" },` |
|      - | 3828 | `		{ "include_end_date",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|      - | 3829 | `	};` |
|      - | 3830 | `	static const PH7_NativeConstDef aDpConst[] = {` |
|      - | 3831 | `		{ "EXCLUDE_START_DATE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|      - | 3832 | `		{ "INCLUDE_END_DATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|      - | 3833 | `	};` |
|      - | 3834 | `	static const PH7_NativeMethodDef aDpMethod[] = {` |
|      - | 3835 | `		/* php overloads this constructor three ways and rejects everything else with` |
|      - | 3836 | `		 * ONE message, so the signature stays unenforced and the body decides. */` |
|      - | 3837 | `		{ "__construct",     PH7_MOD_PUBLIC, 0, "", vm_builtin_DatePeriod_construct },` |
|      - | 3838 | `		{ "createFromISO8601String", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|      - | 3839 | `		  "string $specification, int $options = 0", "static",` |
|      - | 3840 | `		  vm_builtin_DatePeriod_createFromISO8601String },` |
|      - | 3841 | `		{ "getStartDate",    PH7_MOD_PUBLIC, "", "@DateTimeInterface",` |
|      - | 3842 | `		  vm_builtin_DatePeriod_getStartDate },` |
|      - | 3843 | `		{ "getEndDate",      PH7_MOD_PUBLIC, "", "@?DateTimeInterface",` |
|      - | 3844 | `		  vm_builtin_DatePeriod_getEndDate },` |
|      - | 3845 | `		{ "getDateInterval", PH7_MOD_PUBLIC, "", "@DateInterval",` |
|      - | 3846 | `		  vm_builtin_DatePeriod_getDateInterval },` |
|      - | 3847 | `		{ "getRecurrences",  PH7_MOD_PUBLIC, "", "@?int", vm_builtin_DatePeriod_getRecurrences },` |
|      - | 3848 | `		{ "getIterator",     PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_DatePeriod_getIterator },` |
|      - | 3849 | `	};` |
|      - | 3850 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 3851 | `		/* Exceptions first: the classes below throw them. */` |
|      - | 3852 | `		{ "DateException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 3853 | `		{ "DateMalformedStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 3854 | `		{ "DateInvalidTimeZoneException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 3855 | `		{ "DateMalformedIntervalStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 3856 | `		{ "DateMalformedPeriodStringException", "DateException", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 3857 | `		{ "DateTimeInterface", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 3858 | `		  0, 0, aIfaceConst, SX_ARRAYSIZE(aIfaceConst), 0, 0, 0, 0, 0 },` |
|      - | 3859 | `		{ "DateTimeZone", 0, 0, 0,` |
|      - | 3860 | `		  aZoneMethod, SX_ARRAYSIZE(aZoneMethod), 0, 0, aZoneProp, SX_ARRAYSIZE(aZoneProp),` |
|      - | 3861 | `		  0, 0, DtPresentTimeZone },` |
|      - | 3862 | `		{ "DateTime", 0, "DateTimeInterface", 0,` |
|      - | 3863 | `		  aDtMethod, SX_ARRAYSIZE(aDtMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),` |
|      - | 3864 | `		  0, 0, DtPresentDateTime },` |
|      - | 3865 | `		{ "DateTimeImmutable", 0, "DateTimeInterface", 0,` |
|      - | 3866 | `		  aImmMethod, SX_ARRAYSIZE(aImmMethod), 0, 0, aDtProp, SX_ARRAYSIZE(aDtProp),` |
|      - | 3867 | `		  0, 0, DtPresentDateTime },` |
|      - | 3868 | `		{ "DateInterval", 0, 0, 0,` |
|      - | 3869 | `		  aIvMethod, SX_ARRAYSIZE(aIvMethod), 0, 0, aIvProp, SX_ARRAYSIZE(aIvProp), 0, 0, 0 },` |
|      - | 3870 | `		{ "DatePeriod", 0, 0, 0,` |
|      - | 3871 | `		  aDpMethod, SX_ARRAYSIZE(aDpMethod), aDpConst, SX_ARRAYSIZE(aDpConst),` |
|      - | 3872 | `		  aDpProp, SX_ARRAYSIZE(aDpProp), 0, &sDpIterVtab, 0 },` |
|      - | 3873 | `	};` |
|      - | 3874 | `	/* php's procedural aliases. Each is a function in its own right, not a forward,` |
|      - | 3875 | `	 * and each owes aBuiltinSig[] a row (vm_arg_check.c). */` |
|      - | 3876 | `	static const struct {` |
|      - | 3877 | `		const char *zName;` |
|      - | 3878 | `		ProchHostFunction xFunc;` |
|      - | 3879 | `	} aFunc[] = {` |
|      - | 3880 | `		{ "strtotime",                    vm_builtin_strtotime },` |
|      - | 3881 | `		{ "date_create",                  vm_builtin_date_create },` |
|      - | 3882 | `		{ "date_create_immutable",        vm_builtin_date_create_immutable },` |
|      - | 3883 | `		{ "date_create_from_format",      vm_builtin_date_create_from_format },` |
|      - | 3884 | `		{ "date_create_immutable_from_format", vm_builtin_date_create_immutable_from_format },` |
|      - | 3885 | `		{ "date_format",                  vm_builtin_date_format },` |
|      - | 3886 | `		{ "date_modify",                  vm_builtin_date_modify },` |
|      - | 3887 | `		{ "date_add",                     vm_builtin_date_add },` |
|      - | 3888 | `		{ "date_sub",                     vm_builtin_date_sub },` |
|      - | 3889 | `		{ "date_diff",                    vm_builtin_date_diff },` |
|      - | 3890 | `		{ "date_timestamp_get",           vm_builtin_date_timestamp_get },` |
|      - | 3891 | `		{ "date_timestamp_set",           vm_builtin_date_timestamp_set },` |
|      - | 3892 | `		{ "date_timezone_get",            vm_builtin_date_timezone_get },` |
|      - | 3893 | `		{ "date_timezone_set",            vm_builtin_date_timezone_set },` |
|      - | 3894 | `		{ "date_offset_get",              vm_builtin_date_offset_get },` |
|      - | 3895 | `		{ "date_date_set",                vm_builtin_date_date_set },` |
|      - | 3896 | `		{ "date_time_set",                vm_builtin_date_time_set },` |
|      - | 3897 | `		{ "date_isodate_set",             vm_builtin_date_isodate_set },` |
|      - | 3898 | `		{ "date_interval_create_from_date_string", vm_builtin_date_interval_create_from_date_string },` |
|      - | 3899 | `		{ "date_interval_format",         vm_builtin_date_interval_format },` |
|      - | 3900 | `		{ "date_get_last_errors",         vm_builtin_date_get_last_errors },` |
|      - | 3901 | `		{ "timezone_open",                vm_builtin_timezone_open },` |
|      - | 3902 | `		{ "timezone_name_get",            vm_builtin_timezone_name_get },` |
|      - | 3903 | `		{ "timezone_offset_get",          vm_builtin_timezone_offset_get },` |
|      - | 3904 | `	};` |
|      - | 3905 | `	sxu32 n;` |
|      - | 3906 | `	sxi32 rc;` |
|      - | 3907 | `	/* php's date.timezone default */` |
|   4675 | 3908 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|   4675 | 3909 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|   4675 | 3910 | `	DtLastErrClear(&(*pVm));` |
| 116755 | 3911 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 112085 | 3912 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  56045 | 3913 | `	}` |
|   4675 | 3914 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|   4675 | 3915 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 3916 | `		return rc;` |
|      - | 3917 | `	}` |
|      - | 3918 | `	/* IteratorAggregate declares a METHOD, so it is attached now that DatePeriod has` |
|      - | 3919 | `	 * its own: PH7_ClassImplement stubs a missing one as ABSTRACT, which would have` |
|      - | 3920 | `	 * made the class uninstantiable. */` |
|      - | 3921 | `	{` |
|   4675 | 3922 | `		ph7_class *pPeriod = DtClass(&(*pVm),"DatePeriod");` |
|   4675 | 3923 | `		ph7_class *pAggregate = DtClass(&(*pVm),"IteratorAggregate");` |
|   4675 | 3924 | `		if( pPeriod == 0 \|\| pAggregate == 0 ){` |
|    ! 0 | 3925 | `			return SXERR_NOTFOUND;` |
|      - | 3926 | `		}` |
|   4675 | 3927 | `		rc = PH7_ClassImplement(pPeriod,pAggregate);` |
|      - | 3928 | `	}` |
|   4675 | 3929 | `	return rc;` |
|   2340 | 3930 | `}` |
|      - | 3931 |  |
|      - | 3932 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 3933 |  |
|      - | 3934 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 3935 | `/* Tiny build: no DateTime family (builtin layer disabled) */` |
|      - | 3936 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){` |
|      - | 3937 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|      - | 3938 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|      - | 3939 | `	return SXRET_OK;` |
|      - | 3940 | `}` |
|      - | 3941 | `#endif` |
|      - | 3942 |  |
