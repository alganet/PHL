# src/ph7/builtin_date_parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 948/1079 lines (87.86%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `/*` |
|     - |    8 | ` * The DateTime family: proleptic-Gregorian date math, the date/time` |
|     - |    9 | ` * string parser, the __dt_* host thunks, the embedded zDateTimeLib PHP` |
|     - |   10 | ` * chunk and PH7_VmInstallDateTime. The classic procedural date functions` |
|     - |   11 | ` * (date/gmdate/mktime/...) stay in builtin_date.c.` |
|     - |   12 | ` */` |
|     - |   13 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |   14 | `#include <time.h>` |
|     - |   15 | `/* ===========================================================================` |
|     - |   16 | ` * DateTime family (NEWPLAN band D slice 1): DateTimeInterface, DateTime,` |
|     - |   17 | ` * DateTimeImmutable, DateTimeZone (UTC + fixed offsets), date_create(),` |
|     - |   18 | ` * date_create_immutable(). Embedded-PHP chunk + C thunks, following the` |
|     - |   19 | ` * Reflection architecture (installed inside the bCompilingBuiltin window).` |
|     - |   20 | ` * Timezone SCOPE: UTC and fixed "+HH:MM" offsets only — no tz database` |
|     - |   21 | ` * (recorded §10 scope cut; named region zones throw like unknown zones).` |
|     - |   22 | ` * ======================================================================== */` |
|     - |   23 |  |
|     - |   24 | `/*` |
|     - |   25 | ` * Proleptic-Gregorian civil <-> day-count conversions (Howard Hinnant's` |
|     - |   26 | ` * algorithms): no time_t / libc dependence, correct far past 2038 and` |
|     - |   27 | ` * before 1970 on every platform. Day 0 == 1970-01-01.` |
|     - |   28 | ` */` |
|   950 |   29 | `PH7_PRIVATE sxi64 DtDaysFromCivil(sxi64 y,int m,int d)` |
|     1 |   30 | `{` |
|     - |   31 | `	sxi64 era;` |
|     - |   32 | `	unsigned yoe,doy,doe;` |
|   951 |   33 | `	y -= (m <= 2);` |
|   951 |   34 | `	era = (y >= 0 ? y : y - 399) / 400;` |
|   951 |   35 | `	yoe = (unsigned)(y - era * 400);` |
|   951 |   36 | `	doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);` |
|   951 |   37 | `	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;` |
|   951 |   38 | `	return era * 146097 + (sxi64)doe - 719468;` |
|     1 |   39 | `}` |
|   430 |   40 | `PH7_PRIVATE void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd)` |
|     1 |   41 | `{` |
|     - |   42 | `	sxi64 era;` |
|     - |   43 | `	unsigned doe,yoe,doy,mp;` |
|   431 |   44 | `	z += 719468;` |
|   431 |   45 | `	era = (z >= 0 ? z : z - 146096) / 146097;` |
|   431 |   46 | `	doe = (unsigned)(z - era * 146097);` |
|   431 |   47 | `	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;` |
|   431 |   48 | `	*py = (sxi64)yoe + era * 400;` |
|   431 |   49 | `	doy = doe - (365 * yoe + yoe/4 - yoe/100);` |
|   431 |   50 | `	mp = (5 * doy + 2) / 153;` |
|   431 |   51 | `	*pd = (int)(doy - (153 * mp + 2) / 5 + 1);` |
|   431 |   52 | `	*pm = (int)(mp < 10 ? mp + 3 : mp - 9);` |
|   431 |   53 | `	if( *pm <= 2 ){` |
|   195 |   54 | `		*py += 1;` |
|    97 |   55 | `	}` |
|   431 |   56 | `}` |
|   722 |   57 | `PH7_PRIVATE sxi64 DtFloorDiv(sxi64 a,sxi64 b)` |
|     1 |   58 | `{` |
|   723 |   59 | `	sxi64 q = a / b;` |
|   723 |   60 | `	if( (a % b) != 0 && ((a < 0) != (b < 0)) ){` |
|     3 |   61 | `		q--;` |
|     1 |   62 | `	}` |
|   723 |   63 | `	return q;` |
|     1 |   64 | `}` |
|     - |   65 | `/* Timestamp + offset -> Sytm (with zone metadata for DateFormat's T/e/O/P/Z) */` |
|   182 |   66 | `static void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm)` |
|     1 |   67 | `{` |
|   183 |   68 | `	sxi64 t = iTs + iOff;` |
|   183 |   69 | `	sxi64 days = DtFloorDiv(t,86400);` |
|   183 |   70 | `	sxi64 secs = t - days * 86400;` |
|     - |   71 | `	sxi64 y;` |
|     - |   72 | `	int mo,d;` |
|   183 |   73 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|   183 |   74 | `	pTm->tm_sec  = (int)(secs % 60);` |
|   183 |   75 | `	pTm->tm_min  = (int)((secs / 60) % 60);` |
|   183 |   76 | `	pTm->tm_hour = (int)(secs / 3600);` |
|   183 |   77 | `	pTm->tm_mday = d;` |
|   183 |   78 | `	pTm->tm_mon  = mo - 1;` |
|   183 |   79 | `	pTm->tm_year = (int)y;` |
|   183 |   80 | `	pTm->tm_wday = (int)(((days % 7) + 11) % 7); /* day 0 = Thursday(4) */` |
|   183 |   81 | `	pTm->tm_yday = (int)(days - DtDaysFromCivil(y,1,1));` |
|   183 |   82 | `	pTm->tm_isdst = 0;` |
|   183 |   83 | `	pTm->tm_zone = zZone;` |
|   183 |   84 | `	pTm->tm_gmtoff = (long)iOff;` |
|   183 |   85 | `}` |
|   244 |   86 | `static sxi64 DtMakeTs(sxi64 y,int mo,int d,int h,int mi,int s,sxi32 iOff)` |
|     1 |   87 | `{` |
|   245 |   88 | `	return DtDaysFromCivil(y,mo,d) * 86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     1 |   89 | `}` |
|     - |   90 | `/* Month-arithmetic with php's overflow semantics (Jan 31 +1 month -> Mar 2/3):` |
|     - |   91 | ` * normalize the month, keep the day — the civil day-count formula is linear in` |
|     - |   92 | ` * d, so an out-of-range day simply lands in the following month. */` |
|    22 |   93 | `static sxi64 DtAddMonths(sxi64 iTs,sxi32 iOff,sxi64 nMonths)` |
|     1 |   94 | `{` |
|    23 |   95 | `	sxi64 t = iTs + iOff;` |
|    23 |   96 | `	sxi64 days = DtFloorDiv(t,86400);` |
|    23 |   97 | `	sxi64 secs = t - days * 86400;` |
|     - |   98 | `	sxi64 y;` |
|     - |   99 | `	int mo,d;` |
|     - |  100 | `	sxi64 m0;` |
|    23 |  101 | `	DtCivilFromDays(days,&y,&mo,&d);` |
|    23 |  102 | `	m0 = (y * 12 + (mo - 1)) + nMonths;` |
|    23 |  103 | `	y  = DtFloorDiv(m0,12);` |
|    23 |  104 | `	mo = (int)(m0 - y * 12) + 1;` |
|    23 |  105 | `	return DtDaysFromCivil(y,mo,d) * 86400 + secs - iOff;` |
|     1 |  106 | `}` |
|     - |  107 | `/*` |
|     - |  108 | ` * Read a fractional-seconds part at z (which points at the '.'): up to 6 digits` |
|     - |  109 | ` * become microseconds (right-padded to 6, extra digits ignored). Advances *pz.` |
|     - |  110 | ` */` |
|    24 |  111 | `static int DtReadFraction(const char **pz,const char *zEnd)` |
|     1 |  112 | `{` |
|    25 |  113 | `	const char *z = *pz;` |
|    25 |  114 | `	int us = 0,n = 0;` |
|    25 |  115 | `	z++; /* skip '.' */` |
|   111 |  116 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|    87 |  117 | `		if( n < 6 ){ us = us*10 + (z[0]-'0'); n++; }` |
|    87 |  118 | `		z++;` |
|     1 |  119 | `	}` |
|    83 |  120 | `	while( n < 6 ){ us *= 10; n++; }` |
|    25 |  121 | `	*pz = z;` |
|    25 |  122 | `	return us;` |
|     1 |  123 | `}` |
|     - |  124 | `/*` |
|     - |  125 | ` * Parse an OPTIONAL time-of-day suffix after a date component:` |
|     - |  126 | ` * "[( \|T)]HH:MM[:SS][.frac][Z\|±hh[:mm]]". On entry *pz points just past the date;` |
|     - |  127 | ` * the h/mi/s outs must be pre-zeroed and the offset outs pre-seeded with the current` |
|     - |  128 | ` * offset; *pUs receives the microseconds from a fractional part (unchanged when` |
|     - |  129 | ` * absent). Advances *pz over whatever it consumes. Returns 0 on success (whether or` |
|     - |  130 | ` * not a time was present), or a 1-based error position into zIn (negative encodes` |
|     - |  131 | ` * php's "Double time specification"). Shared by every absolute-date branch.` |
|     - |  132 | ` */` |
|   196 |  133 | `static int DtTimeSuffix(const char **pz,const char *zEnd,const char *zIn,` |
|     - |  134 | `	int *ph,int *pmi,int *ps,sxi32 *piOff,int *pbOffSet,int *pUs)` |
|     1 |  135 | `{` |
|   197 |  136 | `	const char *z = *pz;` |
|   196 |  137 | `	if( z < zEnd && (z[0]=='T' \|\| z[0]==' ') && zEnd-z >= 6` |
|    82 |  138 | `	 && SyisDigit(z[1]) && SyisDigit(z[2]) && z[3]==':' ){` |
|    79 |  139 | `		z++;` |
|    79 |  140 | `		*ph  = (z[0]-'0')*10 + (z[1]-'0');` |
|    79 |  141 | `		*pmi = (z[3]-'0')*10 + (z[4]-'0');` |
|     - |  142 | `		/* a 25+ hour kills php's whole time token: error at its start */` |
|    79 |  143 | `		if( *ph > 24 ){ return (int)(z - zIn) + 1; }` |
|     - |  144 | `		/* php lexes HH:M, then the minute's second digit starts a SECOND time` |
|     - |  145 | `		 * token: "Double time specification" (negative encoding) */` |
|    77 |  146 | `		if( *pmi > 59 ){ return -((int)(&z[4] - zIn) + 1); }` |
|    75 |  147 | `		z += 5;` |
|    75 |  148 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|    73 |  149 | `			*ps = (z[1]-'0')*10 + (z[2]-'0');` |
|    73 |  150 | `			if( *ps > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|    71 |  151 | `			z += 3;` |
|    35 |  152 | `		}` |
|    73 |  153 | `		if( z < zEnd && z[0]=='.' && zEnd-z >= 2 && SyisDigit(z[1]) ){ /* fractional seconds */` |
|    23 |  154 | `			*pUs = DtReadFraction(&z,zEnd);` |
|    11 |  155 | `		}` |
|    73 |  156 | `		if( z < zEnd && (z[0]=='Z' \|\| z[0]=='z') ){` |
|     7 |  157 | `			*piOff = 0; *pbOffSet = 2; z++;` |
|    70 |  158 | `		}else if( z < zEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|     7 |  159 | `			int sign = (z[0]=='-') ? -1 : 1;` |
|     7 |  160 | `			int oh,om = 0;` |
|     7 |  161 | `			z++;` |
|     7 |  162 | `			if( zEnd-z < 2 \|\| !SyisDigit(z[0]) \|\| !SyisDigit(z[1]) ){ return (int)(z - zIn) + 1; }` |
|     7 |  163 | `			oh = (z[0]-'0')*10 + (z[1]-'0');` |
|     7 |  164 | `			z += 2;` |
|     7 |  165 | `			if( z < zEnd && z[0]==':' ){ z++; }` |
|     7 |  166 | `			if( zEnd-z >= 2 && SyisDigit(z[0]) && SyisDigit(z[1]) ){` |
|     7 |  167 | `				om = (z[0]-'0')*10 + (z[1]-'0');` |
|     7 |  168 | `				z += 2;` |
|     3 |  169 | `			}` |
|     7 |  170 | `			*piOff = sign * (oh*3600 + om*60);` |
|     7 |  171 | `			*pbOffSet = 1;` |
|     3 |  172 | `		}` |
|    36 |  173 | `	}` |
|   191 |  174 | `	*pz = z;` |
|   191 |  175 | `	return 0;` |
|    99 |  176 | `}` |
|     - |  177 | `/*` |
|     - |  178 | ` * Read one or two decimal digits at z (z<zEnd guaranteed by caller for the first).` |
|     - |  179 | ` * Returns the value; *pn = digits consumed (1 or 2).` |
|     - |  180 | ` */` |
|   118 |  181 | `static int DtRead1or2(const char *z,const char *zEnd,int *pn)` |
|     1 |  182 | `{` |
|   119 |  183 | `	int v = z[0]-'0';` |
|   119 |  184 | `	if( z+1 < zEnd && SyisDigit(z[1]) ){ v = v*10 + (z[1]-'0'); *pn = 2; }` |
|    13 |  185 | `	else { *pn = 1; }` |
|   119 |  186 | `	return v;` |
|     1 |  187 | `}` |
|     - |  188 | `/*` |
|     - |  189 | ` * Try to read a non-ISO numeric date at z: three integer components joined by ONE` |
|     - |  190 | ` * consistent separator, plus an optional time suffix. php's field order depends on` |
|     - |  191 | ` * the separator:` |
|     - |  192 | ` *   '/'      -> YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY` |
|     - |  193 | ` *   '-','.'  -> DD-MM-YYYY (day first); a 4-digit-first '.' date (YYYY.MM.DD) is` |
|     - |  194 | ` *               NOT a php format and is rejected. (ISO YYYY-MM-DD is matched by the` |
|     - |  195 | ` *               dedicated branch BEFORE this one, so a 4-digit-first '-' never` |
|     - |  196 | ` *               reaches here.)` |
|     - |  197 | ` * A 1-2 digit year maps php-style (00-69 -> 2000s, 70-99 -> 1900s). Returns 0 when` |
|     - |  198 | ` * the text is not such a date (caller falls through), 1 on success (the ts/off outs` |
|     - |  199 | ` * set and *pzOut advanced past the whole token), or an error code in DtParse's own` |
|     - |  200 | ` * convention (positive 1-based position into zIn, negative = "double time") when the` |
|     - |  201 | ` * shape matched but a component is out of range.` |
|     - |  202 | ` */` |
|    90 |  203 | `static int DtTryNumericDate(const char *z,const char *zEnd,const char **pzOut,` |
|     - |  204 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,int *pUs)` |
|     1 |  205 | `{` |
|     - |  206 | `	int a,b,c,na,nb,nc;` |
|     - |  207 | `	char sep;` |
|    91 |  208 | `	int y,mo,d,h = 0,mi = 0,s = 0,us = 0;` |
|    91 |  209 | `	sxi32 iOff = *pOff;` |
|     - |  210 | `	int rcT;` |
|     - |  211 | `	/* first field: 1-4 digits */` |
|    91 |  212 | `	if( !SyisDigit(z[0]) ){ return 0; }` |
|    91 |  213 | `	a = 0; na = 0;` |
|   289 |  214 | `	while( z < zEnd && SyisDigit(z[0]) && na < 4 ){ a = a*10 + (z[0]-'0'); z++; na++; }` |
|    91 |  215 | `	if( z >= zEnd \|\| (z[0] != '-' && z[0] != '/' && z[0] != '.') ){ return 0; }` |
|    57 |  216 | `	sep = z[0];` |
|    57 |  217 | `	z++;` |
|     - |  218 | `	/* second field: 1-2 digits */` |
|    57 |  219 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    57 |  220 | `	b = DtRead1or2(z,zEnd,&nb); z += nb;` |
|    57 |  221 | `	if( z >= zEnd \|\| z[0] != sep ){ return 0; }` |
|    57 |  222 | `	z++;` |
|     - |  223 | `	/* third field: 1-4 digits */` |
|    57 |  224 | `	if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return 0; }` |
|    57 |  225 | `	c = 0; nc = 0;` |
|   239 |  226 | `	while( z < zEnd && SyisDigit(z[0]) && nc < 4 ){ c = c*10 + (z[0]-'0'); z++; nc++; }` |
|     - |  227 | `	/* map fields to Y/M/D; nyear tracks the year field's width for 2-digit mapping.` |
|     - |  228 | `	 * '/'  : YYYY/MM/DD when the first field is 4 digits, else MM/DD/YYYY.` |
|     - |  229 | `	 * '-'/'.': a 4-digit LAST field is DD-MM-YYYY (day first); otherwise YY-MM-DD` |
|     - |  230 | `	 *          (year first) — php's width heuristic. (A 4-digit FIRST '-' field is` |
|     - |  231 | `	 *          ISO and never reaches here; a 4-digit-first '.' is not a php format.) */` |
|     - |  232 | `	{` |
|     - |  233 | `		int nyear;` |
|    57 |  234 | `		if( sep == '/' ){` |
|    27 |  235 | `			if( na == 4 ){ y = a; mo = b; d = c; nyear = na; }` |
|    19 |  236 | `			else{ mo = a; d = b; y = c; nyear = nc; }` |
|    44 |  237 | `		}else if( sep == '.' ){` |
|     - |  238 | `			/* php's dot date is DD.MM.YYYY only (a 4-digit year, day first). Other` |
|     - |  239 | `			 * widths are not a clean php format (php itself yields garbage there),` |
|     - |  240 | `			 * so don't claim the match — let the caller fail the parse. */` |
|     5 |  241 | `			if( na == 4 \|\| nc != 4 ){ return 0; }` |
|     3 |  242 | `			d = a; mo = b; y = c; nyear = nc;` |
|     2 |  243 | `		}else{ /* '-' : a 4-digit LAST field is DD-MM-YYYY, else YY-MM-DD */` |
|    27 |  244 | `			if( nc == 4 ){ d = a; mo = b; y = c; nyear = nc; }` |
|    11 |  245 | `			else{ y = a; mo = b; d = c; nyear = na; }` |
|     - |  246 | `		}` |
|    55 |  247 | `		if( nyear <= 2 ){` |
|    11 |  248 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|     3 |  249 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|     5 |  250 | `		}` |
|     - |  251 | `	}` |
|     - |  252 | `	/* php normalizes month 0 to December of the previous year (like the ISO branch)` |
|     - |  253 | `	 * but fails a month past 12; a day past 31 fails, while day 0 normalizes in` |
|     - |  254 | `	 * DtMakeTs. Errors point at the field end. */` |
|    55 |  255 | `	if( mo > 12 ){ return (int)(z - zIn) + 1; }` |
|    51 |  256 | `	if( mo == 0 ){ mo = 12; y--; }` |
|    51 |  257 | `	if( d > 31 ){ return (int)(z - zIn) + 1; }` |
|     - |  258 | `	/* optional time-of-day suffix, then commit */` |
|    47 |  259 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff,&us);` |
|    47 |  260 | `	if( rcT != 0 ){ return rcT; }` |
|    47 |  261 | `	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    47 |  262 | `	*pOff = iOff;` |
|    47 |  263 | `	*pUs = us;` |
|    47 |  264 | `	*pzOut = z;` |
|    47 |  265 | `	return 1;` |
|    46 |  266 | `}` |
|     - |  267 | `/*` |
|     - |  268 | ` * Match a month name at z (full name or its distinct 3-letter abbreviation, plus` |
|     - |  269 | ` * "sept"), case-insensitively and only at a word boundary. Returns the month 1-12` |
|     - |  270 | ` * and sets *pAdv to the bytes consumed, or 0 when no month name is present.` |
|     - |  271 | ` */` |
|   236 |  272 | `static int DtMatchMonth(const char *z,const char *zEnd,int *pAdv)` |
|     1 |  273 | `{` |
|     - |  274 | `	static const struct { const char *z; int n; int mo; } aM[] = {` |
|     - |  275 | `		{ "january",7,1 },{ "february",8,2 },{ "march",5,3 },{ "april",5,4 },` |
|     - |  276 | `		{ "june",4,6 },{ "july",4,7 },{ "august",6,8 },{ "september",9,9 },` |
|     - |  277 | `		{ "sept",4,9 },{ "october",7,10 },{ "november",8,11 },{ "december",8,12 },` |
|     - |  278 | `		{ "may",3,5 },` |
|     - |  279 | `		{ "jan",3,1 },{ "feb",3,2 },{ "mar",3,3 },{ "apr",3,4 },{ "jun",3,6 },` |
|     - |  280 | `		{ "jul",3,7 },{ "aug",3,8 },{ "sep",3,9 },{ "oct",3,10 },{ "nov",3,11 },` |
|     - |  281 | `		{ "dec",3,12 }` |
|     - |  282 | `	};` |
|     - |  283 | `	sxu32 i;` |
|  4805 |  284 | `	for( i = 0 ; i < SX_ARRAYSIZE(aM) ; ++i ){` |
|  4631 |  285 | `		int n = aM[i].n;` |
|  4630 |  286 | `		if( zEnd - z >= n && SyStrnicmp(z,aM[i].z,(sxu32)n) == 0` |
|  2186 |  287 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|    63 |  288 | `			*pAdv = n;` |
|    63 |  289 | `			return aM[i].mo;` |
|     - |  290 | `		}` |
|  2285 |  291 | `	}` |
|   175 |  292 | `	return 0;` |
|   119 |  293 | `}` |
|     - |  294 | `/* Match a weekday name at z (full or 3-letter, case-insensitive, word boundary).` |
|     - |  295 | ` * Returns the day-of-week 0=Sunday..6=Saturday and sets *pAdv, or -1. */` |
|   154 |  296 | `static int DtMatchWeekday(const char *z,const char *zEnd,int *pAdv)` |
|     1 |  297 | `{` |
|     - |  298 | `	static const struct { const char *z; int n; int dow; } aW[] = {` |
|     - |  299 | `		{ "sunday",6,0 },{ "monday",6,1 },{ "tuesday",7,2 },{ "wednesday",9,3 },` |
|     - |  300 | `		{ "thursday",8,4 },{ "friday",6,5 },{ "saturday",8,6 },` |
|     - |  301 | `		{ "sun",3,0 },{ "mon",3,1 },{ "tue",3,2 },{ "wed",3,3 },{ "thu",3,4 },` |
|     - |  302 | `		{ "fri",3,5 },{ "sat",3,6 }` |
|     - |  303 | `	};` |
|     - |  304 | `	sxu32 i;` |
|  1865 |  305 | `	for( i = 0 ; i < SX_ARRAYSIZE(aW) ; ++i ){` |
|  1755 |  306 | `		int n = aW[i].n;` |
|  1754 |  307 | `		if( zEnd - z >= n && SyStrnicmp(z,aW[i].z,(sxu32)n) == 0` |
|   737 |  308 | `		 && (zEnd - z == n \|\| !SyisAlpha(z[n])) ){` |
|    45 |  309 | `			*pAdv = n;` |
|    45 |  310 | `			return aW[i].dow;` |
|     - |  311 | `		}` |
|   856 |  312 | `	}` |
|   111 |  313 | `	return -1;` |
|    78 |  314 | `}` |
|     - |  315 | `/* True if z points at a two-letter English ordinal suffix (st/nd/rd/th). */` |
|    62 |  316 | `static int DtIsOrdinal(const char *z,const char *zEnd)` |
|     1 |  317 | `{` |
|    63 |  318 | `	if( zEnd - z < 2 ){ return 0; }` |
|   119 |  319 | `	return SyStrnicmp(z,"st",2) == 0 \|\| SyStrnicmp(z,"nd",2) == 0` |
|    89 |  320 | `		\|\| SyStrnicmp(z,"rd",2) == 0 \|\| SyStrnicmp(z,"th",2) == 0;` |
|    32 |  321 | `}` |
|     - |  322 | `/*` |
|     - |  323 | ` * Try to read a textual-month date at z, in either order:` |
|     - |  324 | ` *   MonthName [Day] [Year]   ("Jan 15 2020", "January", "January 2020")` |
|     - |  325 | ` *   Day MonthName [Year]     ("15 January 2020", "15th Jan")` |
|     - |  326 | ` * A missing day defaults to 1, a missing year to the base timestamp's year (php).` |
|     - |  327 | ` * Day may carry an ordinal suffix, fields may be comma-separated, month names are` |
|     - |  328 | ` * case-insensitive, and an optional time-of-day suffix + trailing UTC/GMT is` |
|     - |  329 | ` * consumed. Returns 0 (not a month date — caller falls through, *pzOut untouched),` |
|     - |  330 | ` * 1 on success, or a DtParse error code (out-of-range day).` |
|     - |  331 | ` */` |
|   188 |  332 | `static int DtTryMonthDate(const char *z,const char *zEnd,const char **pzOut,` |
|     - |  333 | `	sxi64 *pTs,sxi32 *pOff,int *pbOff,const char *zIn,sxi64 iBaseTs,int *pUs)` |
|     1 |  334 | `{` |
|   189 |  335 | `	int mo,d = 1,adv,haveDay = 0,haveYear = 0;` |
|   189 |  336 | `	sxi64 y = 0;` |
|   189 |  337 | `	int h = 0,mi = 0,s = 0,us = 0;` |
|   189 |  338 | `	sxi32 iOff = *pOff;` |
|     - |  339 | `	int rcT;` |
|     - |  340 | `#define MDSKIPWS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|   189 |  341 | `	if( (mo = DtMatchMonth(z,zEnd,&adv)) != 0 ){` |
|     - |  342 | `		/* MonthName [Day] [Year]. A 4-digit number here is the YEAR, not the day` |
|     - |  343 | `		 * ("January 2020" is month+year, day defaults); a 1-2 digit number is the day. */` |
|    31 |  344 | `		z += adv;` |
|    76 |  345 | `		MDSKIPWS();` |
|    31 |  346 | `		if( z < zEnd && SyisDigit(z[0]) ){` |
|    31 |  347 | `			int nrun = 0;` |
|    31 |  348 | `			const char *zp = z;` |
|    95 |  349 | `			while( zp < zEnd && SyisDigit(zp[0]) && nrun < 4 ){ zp++; nrun++; }` |
|    31 |  350 | `			if( nrun < 4 ){` |
|    27 |  351 | `				d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|    27 |  352 | `				if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|    27 |  353 | `				haveDay = 1;` |
|    68 |  354 | `				MDSKIPWS();` |
|    13 |  355 | `			}` |
|    16 |  356 | `		}` |
|   174 |  357 | `	}else if( SyisDigit(z[0]) ){` |
|     - |  358 | `		/* Day MonthName [Year] */` |
|    37 |  359 | `		d = DtRead1or2(z,zEnd,&adv); z += adv;` |
|    37 |  360 | `		if( DtIsOrdinal(z,zEnd) ){ z += 2; }` |
|    37 |  361 | `		haveDay = 1;` |
|    77 |  362 | `		MDSKIPWS();` |
|    37 |  363 | `		if( (mo = DtMatchMonth(z,zEnd,&adv)) == 0 ){ return 0; }` |
|    21 |  364 | `		z += adv;` |
|    49 |  365 | `		MDSKIPWS();` |
|    11 |  366 | `	}else{` |
|   123 |  367 | `		return 0;` |
|     - |  368 | `	}` |
|     - |  369 | `	/* optional year */` |
|    51 |  370 | `	if( z < zEnd && SyisDigit(z[0]) ){` |
|    47 |  371 | `		int ny = 0;` |
|    47 |  372 | `		y = 0;` |
|   231 |  373 | `		while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ y = y*10 + (z[0]-'0'); z++; ny++; }` |
|    47 |  374 | `		if( ny <= 2 ){` |
|   ! 0 |  375 | `			if( y >= 0 && y <= 69 ){ y += 2000; }` |
|   ! 0 |  376 | `			else if( y >= 70 && y <= 99 ){ y += 1900; }` |
|   ! 0 |  377 | `		}` |
|    47 |  378 | `		haveYear = 1;` |
|    23 |  379 | `	}` |
|     - |  380 | `	/* Default the unspecified fields from the base timestamp. php overlays: a` |
|     - |  381 | `	 * missing year takes the base year; a missing day is 1 when a year WAS given` |
|     - |  382 | `	 * ("January 2020" -> the 1st) but the base day when only the month was named` |
|     - |  383 | `	 * ("January" -> the base day). */` |
|     - |  384 | `	{` |
|     - |  385 | `		sxi64 by; int bm,bd;` |
|    51 |  386 | `		DtCivilFromDays(DtFloorDiv(iBaseTs + *pOff,86400),&by,&bm,&bd);` |
|    51 |  387 | `		if( !haveYear ){ y = by; }` |
|    51 |  388 | `		if( !haveDay ){ d = haveYear ? 1 : bd; }` |
|     - |  389 | `	}` |
|    51 |  390 | `	if( d > 31 ){ return (int)(z - zIn) + 1; }` |
|     - |  391 | `	/* optional time-of-day suffix */` |
|    51 |  392 | `	rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,pbOff,&us);` |
|    51 |  393 | `	if( rcT != 0 ){ return rcT; }` |
|     - |  394 | `	/* optional trailing UTC/GMT zone name (PHL's default zone is already UTC) */` |
|    55 |  395 | `	MDSKIPWS();` |
|    50 |  396 | `	if( (zEnd-z >= 3 && SyStrnicmp(z,"utc",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3])))` |
|    49 |  397 | `	 \|\| (zEnd-z >= 3 && SyStrnicmp(z,"gmt",3) == 0 && (zEnd-z==3 \|\| !SyisAlpha(z[3]))) ){` |
|     3 |  398 | `		iOff = 0; z += 3;` |
|     1 |  399 | `	}` |
|    51 |  400 | `	*pTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    51 |  401 | `	*pOff = iOff;` |
|    51 |  402 | `	*pUs = us;` |
|    51 |  403 | `	*pzOut = z;` |
|    51 |  404 | `	return 1;` |
|     - |  405 | `#undef MDSKIPWS` |
|    95 |  406 | `}` |
|     - |  407 | `/*` |
|     - |  408 | ` * Minimal php-datetime-string parser (slice 1): absolute forms` |
|     - |  409 | ` * "now" \| "@<ts>" \| "YYYY-MM-DD[( \|T)HH:MM[:SS]][Z\|±HH[:MM]]" \| "HH:MM[:SS]",` |
|     - |  410 | ` * keywords today/midnight/noon/tomorrow/yesterday, and relative sequences` |
|     - |  411 | ` * "[+\|-]N (sec\|min\|hour\|day\|week\|fortnight\|month\|year)[s]". Returns 0 on` |
|     - |  412 | ` * success (ts/off/bOffSet out), or the byte position of the first` |
|     - |  413 | ` * unparseable character +1 (for php's "at position N" message).` |
|     - |  414 | ` */` |
|   458 |  415 | `static int DtParse(const char *zIn,int nLen,sxi64 iBaseTs,sxi32 iBaseOff,` |
|     - |  416 | `	sxi64 *pTs,sxi32 *pOff,int *pbOffSet,int *pUs)` |
|     1 |  417 | `{` |
|   459 |  418 | `	const char *z = zIn, *zEnd = &zIn[nLen];` |
|   459 |  419 | `	sxi64 iTs = iBaseTs;` |
|   459 |  420 | `	sxi32 iOff = iBaseOff;` |
|   459 |  421 | `	int bOffSet = 0;` |
|   459 |  422 | `	int bAny = 0;` |
|     - |  423 | `	int iNumRc,iMonRc;` |
|   459 |  424 | `	int uSec = 0;` |
|   459 |  425 | `	*pUs = 0;` |
|     - |  426 | `#define DT_SKIP_WS() while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]==',') ){ z++; }` |
|     - |  427 | `#define DT_LOWEQ(zKw,nKw) (zEnd-z >= (nKw) && SyStrnicmp(z,zKw,nKw) == 0 \` |
|     - |  428 | `	&& (zEnd-z == (nKw) \|\| !SyisAlpha(z[(nKw)])))` |
|   694 |  429 | `	DT_SKIP_WS();` |
|   459 |  430 | `	if( z >= zEnd ){` |
|     - |  431 | `		/* php: the empty string is "now" */` |
|     3 |  432 | `		*pTs = iTs;` |
|     3 |  433 | `		*pOff = iOff;` |
|     3 |  434 | `		*pbOffSet = bOffSet;` |
|     3 |  435 | `		return 0;` |
|     - |  436 | `	}` |
|     - |  437 | `	/* "@<seconds>" absolute epoch */` |
|   457 |  438 | `	if( z[0] == '@' ){` |
|    81 |  439 | `		int neg = 0;` |
|    81 |  440 | `		sxi64 v = 0;` |
|    81 |  441 | `		const char *zAt = z;` |
|    81 |  442 | `		z++;` |
|    81 |  443 | `		if( z < zEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|     - |  444 | `		/* php's lexer rejects the whole token: the error points at the '@' */` |
|    81 |  445 | `		if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zAt - zIn) + 1; }` |
|   225 |  446 | `		while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|     - |  447 | `		/* php accepts a fractional epoch ("@1600000000.5" -> .5s = 500000us) */` |
|    79 |  448 | `		if( z < zEnd && z[0]=='.' && zEnd-z >= 2 && SyisDigit(z[1]) ){` |
|     3 |  449 | `			*pUs = DtReadFraction(&z,zEnd);` |
|     1 |  450 | `		}` |
|    79 |  451 | `		*pTs = neg ? -v : v;` |
|    79 |  452 | `		*pOff = 0;` |
|    79 |  453 | `		*pbOffSet = 1;` |
|    79 |  454 | `		DT_SKIP_WS();` |
|    79 |  455 | `		return (z < zEnd) ? (int)(z - zIn) + 1 : 0;` |
|     - |  456 | `	}` |
|     - |  457 | `	/* Absolute date: YYYY-MM-DD[...] */` |
|   376 |  458 | `	if( zEnd-z >= 10 && SyisDigit(z[0]) && SyisDigit(z[1]) && SyisDigit(z[2])` |
|   147 |  459 | `	 && SyisDigit(z[3]) && z[4]=='-' ){` |
|   111 |  460 | `		sxi64 y = (z[0]-'0')*1000 + (z[1]-'0')*100 + (z[2]-'0')*10 + (z[3]-'0');` |
|   111 |  461 | `		int mo,d,h=0,mi=0,s=0;` |
|   111 |  462 | `		if( !SyisDigit(z[5])\|\|!SyisDigit(z[6])\|\|z[7] != '-'\|\|!SyisDigit(z[8])\|\|!SyisDigit(z[9]) ){` |
|   ! 0 |  463 | `			return (int)(z - zIn) + 1;` |
|     - |  464 | `		}` |
|   111 |  465 | `		mo = (z[5]-'0')*10 + (z[6]-'0');` |
|   111 |  466 | `		d  = (z[8]-'0')*10 + (z[9]-'0');` |
|     - |  467 | `		/* php's lexer dies on the SECOND digit of an out-of-range month/day` |
|     - |  468 | `		 * (either the two-digit pattern fails there, or a one-digit component` |
|     - |  469 | `		 * matched and the separator check fails there); "00" lexes fine and` |
|     - |  470 | `		 * normalizes (month 0 == December of the previous year). */` |
|   111 |  471 | `		if( mo > 12 ){ return (int)(&z[6] - zIn) + 1; }` |
|   105 |  472 | `		if( d > 31 ){ return (int)(&z[9] - zIn) + 1; }` |
|   101 |  473 | `		if( mo == 0 ){ mo = 12; y--; }` |
|   101 |  474 | `		z += 10;` |
|     - |  475 | `		{` |
|   101 |  476 | `			int rcT = DtTimeSuffix(&z,zEnd,zIn,&h,&mi,&s,&iOff,&bOffSet,&uSec);` |
|   101 |  477 | `			if( rcT != 0 ){ return rcT; }` |
|     - |  478 | `		}` |
|    95 |  479 | `		iTs = DtMakeTs(y,mo,d,h,mi,s,iOff);` |
|    95 |  480 | `		bAny = 1;` |
|   314 |  481 | `	}else if( SyisDigit(z[0])` |
|   179 |  482 | `	 && (iNumRc = DtTryNumericDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,&uSec)) != 0 ){` |
|     - |  483 | `		/* DD-MM-YYYY / DD.MM.YYYY (day first), MM/DD/YYYY (slash, American), and` |
|     - |  484 | `		 * YYYY/MM/DD (slash, year first) — see DtTryNumericDate. Anything other than` |
|     - |  485 | `		 * 1 is an error code in DtParse's own convention (positive position / negative` |
|     - |  486 | `		 * "double time"); propagate it verbatim. */` |
|    55 |  487 | `		if( iNumRc != 1 ){ return iNumRc; }` |
|    47 |  488 | `		bAny = 1;` |
|   236 |  489 | `	}else if( (SyisAlpha(z[0]) \|\| SyisDigit(z[0]))` |
|   201 |  490 | `	 && (iMonRc = DtTryMonthDate(z,zEnd,&z,&iTs,&iOff,&bOffSet,zIn,iBaseTs,&uSec)) != 0 ){` |
|     - |  491 | `		/* MonthName Day Year / Day MonthName Year, in any of php's spellings. As with` |
|     - |  492 | `		 * DtTryNumericDate, anything other than 1 is an error code to propagate. */` |
|    51 |  493 | `		if( iMonRc != 1 ){ return iMonRc; }` |
|    51 |  494 | `		bAny = 1;` |
|   188 |  495 | `	}else if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|    14 |  496 | `	 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|     - |  497 | `		/* Time-only: HH:MM[:SS] on the base date */` |
|    11 |  498 | `		sxi64 t = iTs + iOff;` |
|    11 |  499 | `		sxi64 days = DtFloorDiv(t,86400);` |
|    11 |  500 | `		int h  = (z[0]-'0')*10 + (z[1]-'0');` |
|    11 |  501 | `		int mi = (z[3]-'0')*10 + (z[4]-'0');` |
|    11 |  502 | `		int s = 0;` |
|     - |  503 | `		/* php: bad hour kills the token (error at its start); bad minute /` |
|     - |  504 | `		 * second dies on the component's second digit */` |
|    11 |  505 | `		if( h > 24 ){ return (int)(z - zIn) + 1; }` |
|     9 |  506 | `		if( mi > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|     7 |  507 | `		z += 5;` |
|     7 |  508 | `		if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     5 |  509 | `			s = (z[1]-'0')*10 + (z[2]-'0');` |
|     5 |  510 | `			if( s > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|     3 |  511 | `			z += 3;` |
|     1 |  512 | `		}` |
|     5 |  513 | `		iTs = days*86400 + (sxi64)h*3600 + (sxi64)mi*60 + s - iOff;` |
|     5 |  514 | `		bAny = 1;` |
|   155 |  515 | `	}else if( DT_LOWEQ("now",3) ){` |
|     3 |  516 | `		z += 3;` |
|     3 |  517 | `		bAny = 1;` |
|     1 |  518 | `	}` |
|     - |  519 | `	/* Relative / keyword sequence */` |
|   173 |  520 | `	for(;;){` |
|   608 |  521 | `		DT_SKIP_WS();` |
|   497 |  522 | `		if( z >= zEnd ){` |
|   329 |  523 | `			break;` |
|     - |  524 | `		}` |
|   169 |  525 | `		if( DT_LOWEQ("today",5) \|\| DT_LOWEQ("midnight",8) ){` |
|     9 |  526 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     9 |  527 | `			iTs = days*86400 - iOff;` |
|     9 |  528 | `			z += (SyToLower(z[0])=='t') ? 5 : 8;` |
|     9 |  529 | `			bAny = 1;` |
|     9 |  530 | `			continue;` |
|     - |  531 | `		}` |
|   161 |  532 | `		if( DT_LOWEQ("noon",4) ){` |
|     3 |  533 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|     3 |  534 | `			iTs = days*86400 + 12*3600 - iOff;` |
|     3 |  535 | `			z += 4;` |
|     3 |  536 | `			bAny = 1;` |
|     3 |  537 | `			continue;` |
|     - |  538 | `		}` |
|   159 |  539 | `		if( DT_LOWEQ("tomorrow",8) ){` |
|     3 |  540 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) + 1;` |
|     3 |  541 | `			iTs = days*86400 - iOff;` |
|     3 |  542 | `			z += 8;` |
|     3 |  543 | `			bAny = 1;` |
|     3 |  544 | `			continue;` |
|     - |  545 | `		}` |
|   157 |  546 | `		if( DT_LOWEQ("yesterday",9) ){` |
|     3 |  547 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400) - 1;` |
|     3 |  548 | `			iTs = days*86400 - iOff;` |
|     3 |  549 | `			z += 9;` |
|     3 |  550 | `			bAny = 1;` |
|     3 |  551 | `			continue;` |
|     - |  552 | `		}` |
|     - |  553 | `		/* Weekday navigation: "[next\|last\|previous\|this] <weekday>" moves to the` |
|     - |  554 | `		 * midnight of the target weekday. Bare/"this" = the this-week occurrence on` |
|     - |  555 | `		 * or after the base day; "next"/"last"/"previous" skip a matching base day. */` |
|     - |  556 | `		{` |
|   155 |  557 | `			const char *zSave = z;` |
|   155 |  558 | `			int dir = 0;         /* 0 = this-week occurrence, 1 = next, -1 = last */` |
|     - |  559 | `			int adv,dow;` |
|   188 |  560 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; DT_SKIP_WS(); }` |
|   136 |  561 | `			else if( DT_LOWEQ("previous",8) ){ dir = -1; z += 8; DT_SKIP_WS(); }` |
|   179 |  562 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; DT_SKIP_WS(); }` |
|   114 |  563 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; DT_SKIP_WS(); }` |
|   155 |  564 | `			dow = DtMatchWeekday(z,zEnd,&adv);` |
|   155 |  565 | `			if( dow >= 0 ){` |
|    45 |  566 | `				sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|    45 |  567 | `				int bdow = (int)(((days + 4) % 7 + 7) % 7); /* 1970-01-01 was Thursday */` |
|     - |  568 | `				sxi64 delta;` |
|    45 |  569 | `				if( dir == 1 ){` |
|    13 |  570 | `					delta = ((dow - bdow) % 7 + 7) % 7;` |
|    13 |  571 | `					if( delta == 0 ){ delta = 7; }` |
|    39 |  572 | `				}else if( dir == -1 ){` |
|    13 |  573 | `					delta = -(((bdow - dow) % 7 + 7) % 7);` |
|    13 |  574 | `					if( delta == 0 ){ delta = -7; }` |
|     7 |  575 | `				}else{` |
|    21 |  576 | `					delta = ((dow - bdow) % 7 + 7) % 7;` |
|     - |  577 | `				}` |
|    45 |  578 | `				iTs = (days + delta)*86400 - iOff; /* midnight of the target day */` |
|    45 |  579 | `				z += adv;` |
|    45 |  580 | `				bAny = 1;` |
|    45 |  581 | `				continue;` |
|     - |  582 | `			}` |
|   111 |  583 | `			z = zSave; /* prefix did not introduce a weekday: rewind and try the rest */` |
|     - |  584 | `		}` |
|     - |  585 | `		/* "first\|last day of (this\|next\|last month \| MonthName [Year])": jump to the` |
|     - |  586 | `		 * first or last day of a target month. A this/next/last-month target keeps the` |
|     - |  587 | `		 * base time-of-day; an absolute MonthName [Year] target resets it to midnight` |
|     - |  588 | `		 * (php). */` |
|   111 |  589 | `		if( DT_LOWEQ("first",5) \|\| DT_LOWEQ("last",4) ){` |
|    39 |  590 | `			const char *zSave = z;` |
|    39 |  591 | `			int bFirst = (SyToLower((unsigned char)z[0]) == 'f');` |
|    39 |  592 | `			z += bFirst ? 5 : 4;` |
|    96 |  593 | `			DT_SKIP_WS();` |
|    39 |  594 | `			if( DT_LOWEQ("day",3) ){` |
|    31 |  595 | `				z += 3;` |
|    76 |  596 | `				DT_SKIP_WS();` |
|    31 |  597 | `				if( DT_LOWEQ("of",2) ){` |
|    31 |  598 | `					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);` |
|     - |  599 | `					sxi64 yy,tod;` |
|    31 |  600 | `					int mm,dd0,keepTime = 1,ok = 1;` |
|    31 |  601 | `					z += 2;` |
|    74 |  602 | `					DT_SKIP_WS();` |
|    31 |  603 | `					DtCivilFromDays(days0,&yy,&mm,&dd0);` |
|    31 |  604 | `					tod = (iTs + iOff) - days0*86400;` |
|    40 |  605 | `					if( DT_LOWEQ("this",4) ){ z += 4; DT_SKIP_WS();` |
|     7 |  606 | `						if( DT_LOWEQ("month",5) ){ z += 5; }else{ ok = 0; } }` |
|    34 |  607 | `					else if( DT_LOWEQ("next",4) ){ z += 4; DT_SKIP_WS();` |
|     7 |  608 | `						if( DT_LOWEQ("month",5) ){ z += 5; mm++; if(mm>12){ mm=1; yy++; } }else{ ok = 0; } }` |
|    25 |  609 | `					else if( DT_LOWEQ("last",4) ){ z += 4; DT_SKIP_WS();` |
|     5 |  610 | `						if( DT_LOWEQ("month",5) ){ z += 5; mm--; if(mm<1){ mm=12; yy--; } }else{ ok = 0; } }` |
|    15 |  611 | `					else if( z < zEnd ){` |
|     - |  612 | `						int mo,adv;` |
|    13 |  613 | `						mo = DtMatchMonth(z,zEnd,&adv);` |
|    13 |  614 | `						if( mo == 0 ){ return (int)(z - zIn) + 1; }` |
|    27 |  615 | `						z += adv; DT_SKIP_WS();` |
|    13 |  616 | `						mm = mo; keepTime = 0; tod = 0;` |
|    13 |  617 | `						if( z < zEnd && SyisDigit(z[0]) ){` |
|     9 |  618 | `							int ny = 0; sxi64 yv = 0;` |
|    41 |  619 | `							while( z < zEnd && SyisDigit(z[0]) && ny < 4 ){ yv = yv*10 + (z[0]-'0'); z++; ny++; }` |
|     9 |  620 | `							if( ny <= 2 ){ if( yv <= 69 ){ yv += 2000; } else if( yv <= 99 ){ yv += 1900; } }` |
|     9 |  621 | `							yy = yv;` |
|     4 |  622 | `						}` |
|     6 |  623 | `					}` |
|     - |  624 | `					/* else: "... day of" with nothing after — php defaults to this` |
|     - |  625 | `					 * month (mm/yy/tod stay the base, keepTime stays 1). */` |
|    31 |  626 | `					if( ok ){` |
|    31 |  627 | `						int dim = (int)(DtDaysFromCivil(yy,mm+1,1) - DtDaysFromCivil(yy,mm,1));` |
|    31 |  628 | `						int day = bFirst ? 1 : dim;` |
|    31 |  629 | `						iTs = DtDaysFromCivil(yy,mm,day)*86400 + (keepTime ? tod : 0) - iOff;` |
|    31 |  630 | `						bAny = 1;` |
|    31 |  631 | `						continue;` |
|     - |  632 | `					}` |
|   ! 0 |  633 | `				}` |
|   ! 0 |  634 | `			}` |
|     9 |  635 | `			z = zSave; /* not the "first\|last day of ..." shape: rewind */` |
|     4 |  636 | `		}` |
|     - |  637 | `		/* Standalone "this\|next\|last (month\|week)": month shifts by ±1 keeping the` |
|     - |  638 | `		 * day/time; week moves to the Monday of this/next/last ISO week keeping the` |
|     - |  639 | `		 * time-of-day (php: weeks start on Monday). */` |
|     - |  640 | `		{` |
|   103 |  641 | `			const char *zSave = z;` |
|   103 |  642 | `			int dir = 2; /* 2 = no prefix */` |
|   103 |  643 | `			if( DT_LOWEQ("next",4) ){ dir = 1; z += 4; }` |
|    71 |  644 | `			else if( DT_LOWEQ("last",4) ){ dir = -1; z += 4; }` |
|    63 |  645 | `			else if( DT_LOWEQ("this",4) ){ dir = 0; z += 4; }` |
|    81 |  646 | `			if( dir != 2 ){` |
|    66 |  647 | `				DT_SKIP_WS();` |
|    27 |  648 | `				if( DT_LOWEQ("month",5) ){` |
|    13 |  649 | `					z += 5;` |
|    13 |  650 | `					iTs = DtAddMonths(iTs,iOff,dir);` |
|    13 |  651 | `					bAny = 1;` |
|    13 |  652 | `					continue;` |
|     - |  653 | `				}` |
|    15 |  654 | `				if( DT_LOWEQ("week",4) ){` |
|    13 |  655 | `					sxi64 days0 = DtFloorDiv(iTs + iOff,86400);` |
|    13 |  656 | `					sxi64 tod = (iTs + iOff) - days0*86400;` |
|    13 |  657 | `					int bdow = (int)(((days0 + 4) % 7 + 7) % 7);` |
|    13 |  658 | `					sxi64 monday = days0 - ((bdow + 6) % 7); /* Monday of the base week */` |
|    13 |  659 | `					z += 4;` |
|    13 |  660 | `					monday += (sxi64)dir * 7;` |
|    13 |  661 | `					iTs = monday*86400 + tod - iOff;` |
|    13 |  662 | `					bAny = 1;` |
|    13 |  663 | `					continue;` |
|     - |  664 | `				}` |
|     1 |  665 | `			}` |
|    57 |  666 | `			z = zSave;` |
|     - |  667 | `		}` |
|     - |  668 | `		/* Trailing time-of-day in a relative sequence ("next thursday 15:00"): set` |
|     - |  669 | `		 * the clock on the current day. The leading absolute HH:MM branch handles a` |
|     - |  670 | `		 * time at the START; this handles one AFTER a date/relative token. */` |
|    56 |  671 | `		if( zEnd-z >= 5 && SyisDigit(z[0]) && SyisDigit(z[1]) && z[2]==':'` |
|    16 |  672 | `		 && SyisDigit(z[3]) && SyisDigit(z[4]) ){` |
|    13 |  673 | `			sxi64 days = DtFloorDiv(iTs + iOff,86400);` |
|    13 |  674 | `			int hh = (z[0]-'0')*10 + (z[1]-'0');` |
|    13 |  675 | `			int mm = (z[3]-'0')*10 + (z[4]-'0');` |
|    13 |  676 | `			int ss = 0;` |
|    13 |  677 | `			if( hh > 24 ){ return (int)(z - zIn) + 1; }` |
|    13 |  678 | `			if( mm > 59 ){ return (int)(&z[4] - zIn) + 1; }` |
|    13 |  679 | `			z += 5;` |
|    13 |  680 | `			if( z < zEnd && z[0]==':' && zEnd-z >= 3 && SyisDigit(z[1]) && SyisDigit(z[2]) ){` |
|     5 |  681 | `				ss = (z[1]-'0')*10 + (z[2]-'0');` |
|     5 |  682 | `				if( ss > 59 ){ return (int)(&z[2] - zIn) + 1; }` |
|     5 |  683 | `				z += 3;` |
|     2 |  684 | `			}` |
|    13 |  685 | `			iTs = days*86400 + (sxi64)hh*3600 + (sxi64)mm*60 + ss - iOff;` |
|    13 |  686 | `			bAny = 1;` |
|    13 |  687 | `			continue;` |
|     - |  688 | `		}` |
|    45 |  689 | `		if( SyisDigit(z[0]) \|\| z[0]=='+' \|\| z[0]=='-' ){` |
|    33 |  690 | `			int neg = 0;` |
|    33 |  691 | `			sxi64 v = 0;` |
|    33 |  692 | `			const char *zNumStart = z;` |
|    33 |  693 | `			if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; }` |
|    33 |  694 | `			if( z >= zEnd \|\| !SyisDigit(z[0]) ){ return (int)(zNumStart - zIn) + 1; }` |
|    81 |  695 | `			while( z < zEnd && SyisDigit(z[0]) ){ v = v*10 + (z[0]-'0'); z++; }` |
|    33 |  696 | `			if( neg ){ v = -v; }` |
|    79 |  697 | `			DT_SKIP_WS();` |
|    33 |  698 | `			if( DT_LOWEQ("seconds",7) )     { iTs += v;            z += 7; }` |
|    33 |  699 | `			else if( DT_LOWEQ("second",6) ) { iTs += v;            z += 6; }` |
|    33 |  700 | `			else if( DT_LOWEQ("secs",4) )   { iTs += v;            z += 4; }` |
|    33 |  701 | `			else if( DT_LOWEQ("sec",3) )    { iTs += v;            z += 3; }` |
|    33 |  702 | `			else if( DT_LOWEQ("minutes",7) ){ iTs += v*60;         z += 7; }` |
|    31 |  703 | `			else if( DT_LOWEQ("minute",6) ) { iTs += v*60;         z += 6; }` |
|    31 |  704 | `			else if( DT_LOWEQ("mins",4) )   { iTs += v*60;         z += 4; }` |
|    31 |  705 | `			else if( DT_LOWEQ("min",3) )    { iTs += v*60;         z += 3; }` |
|    31 |  706 | `			else if( DT_LOWEQ("hours",5) )  { iTs += v*3600;       z += 5; }` |
|    29 |  707 | `			else if( DT_LOWEQ("hour",4) )   { iTs += v*3600;       z += 4; }` |
|    29 |  708 | `			else if( DT_LOWEQ("days",4) )   { iTs += v*86400;      z += 4; }` |
|    29 |  709 | `			else if( DT_LOWEQ("day",3) )    { iTs += v*86400;      z += 3; }` |
|    23 |  710 | `			else if( DT_LOWEQ("weeks",5) )  { iTs += v*7*86400;    z += 5; }` |
|    21 |  711 | `			else if( DT_LOWEQ("week",4) )   { iTs += v*7*86400;    z += 4; }` |
|    19 |  712 | `			else if( DT_LOWEQ("fortnights",10) ){ iTs += v*14*86400; z += 10; }` |
|    17 |  713 | `			else if( DT_LOWEQ("fortnight",9) )  { iTs += v*14*86400; z += 9; }` |
|    17 |  714 | `			else if( DT_LOWEQ("months",6) ) { iTs = DtAddMonths(iTs,iOff,v); z += 6; }` |
|    15 |  715 | `			else if( DT_LOWEQ("month",5) )  { iTs = DtAddMonths(iTs,iOff,v); z += 5; }` |
|     9 |  716 | `			else if( DT_LOWEQ("years",5) )  { iTs = DtAddMonths(iTs,iOff,v*12); z += 5; }` |
|     9 |  717 | `			else if( DT_LOWEQ("year",4) )   { iTs = DtAddMonths(iTs,iOff,v*12); z += 4; }` |
|     - |  718 | `			else{` |
|     7 |  719 | `				return (int)(z - zIn) + 1;` |
|     - |  720 | `			}` |
|    27 |  721 | `			bAny = 1;` |
|    27 |  722 | `			continue;` |
|     - |  723 | `		}` |
|    13 |  724 | `		return (int)(z - zIn) + 1;` |
|   ! 0 |  725 | `	}` |
|   329 |  726 | `	if( !bAny ){` |
|   ! 0 |  727 | `		return 1;` |
|     - |  728 | `	}` |
|   329 |  729 | `	*pTs = iTs;` |
|   329 |  730 | `	*pOff = iOff;` |
|   329 |  731 | `	*pbOffSet = bOffSet;` |
|   329 |  732 | `	*pUs = uSec;` |
|   329 |  733 | `	return 0;` |
|     - |  734 | `#undef DT_SKIP_WS` |
|     - |  735 | `#undef DT_LOWEQ` |
|   230 |  736 | `}` |
|     - |  737 | `/* int __dt_now() */` |
|   240 |  738 | `static int vm_builtin_dt_now(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  739 | `{` |
|   120 |  740 | `	SXUNUSED(nArg);` |
|   120 |  741 | `	SXUNUSED(apArg);` |
|   241 |  742 | `	ph7_result_int64(pCtx,(ph7_int64)time(0));` |
|   241 |  743 | `	return PH7_OK;` |
|     1 |  744 | `}` |
|     - |  745 | `/* mixed __dt_parse(string $s, int $baseTs, int $baseOff)` |
|     - |  746 | ` *   -> [ts, off, offWasExplicit] on success; php's error MESSAGE string on` |
|     - |  747 | ` *      failure (the chunk wraps it in DateMalformedStringException). */` |
|   458 |  748 | `static int vm_builtin_dt_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  749 | `{` |
|     - |  750 | `	const char *zIn;` |
|     - |  751 | `	int nLen;` |
|     - |  752 | `	sxi64 iBaseTs;` |
|     - |  753 | `	sxi32 iBaseOff;` |
|   459 |  754 | `	sxi64 iTs = 0;` |
|   459 |  755 | `	sxi32 iOff = 0;` |
|   459 |  756 | `	int bOffSet = 0;` |
|   459 |  757 | `	int uSec = 0;` |
|     - |  758 | `	int iErrPos;` |
|   459 |  759 | `	if( nArg < 3 ){` |
|   ! 0 |  760 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  761 | `		return PH7_OK;` |
|     - |  762 | `	}` |
|   459 |  763 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   459 |  764 | `	iBaseTs  = ph7_value_to_int64(apArg[1]);` |
|   459 |  765 | `	iBaseOff = (sxi32)ph7_value_to_int64(apArg[2]);` |
|   459 |  766 | `	iErrPos = DtParse(zIn,nLen,iBaseTs,iBaseOff,&iTs,&iOff,&bOffSet,&uSec);` |
|   459 |  767 | `	if( iErrPos != 0 ){` |
|     - |  768 | `		/* Negative encoding: php's "Double time specification" reason */` |
|    51 |  769 | `		int bDouble = iErrPos < 0;` |
|    51 |  770 | `		int iPos = (bDouble ? -iErrPos : iErrPos) - 1;` |
|    51 |  771 | `		char cAt = (iPos < nLen) ? zIn[iPos] : ' ';` |
|     - |  772 | `		/* php appends a reason: an alphabetic token is assumed to be a timezone` |
|     - |  773 | `		 * lookup miss, anything else an unexpected character. */` |
|   100 |  774 | `		ph7_result_string_format(pCtx,` |
|     - |  775 | `			"Failed to parse time string (%.*s) at position %d (%c): %s",` |
|    25 |  776 | `			nLen,zIn,iPos,cAt,` |
|    49 |  777 | `			bDouble ? "Double time specification"` |
|    48 |  778 | `			: ((cAt >= 'a' && cAt <= 'z') \|\| (cAt >= 'A' && cAt <= 'Z'))` |
|     - |  779 | `				? "The timezone could not be found in the database"` |
|    48 |  780 | `				: "Unexpected character");` |
|    51 |  781 | `		return PH7_OK;` |
|     - |  782 | `	}` |
|     - |  783 | `	{` |
|   409 |  784 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|   409 |  785 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|   409 |  786 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 |  787 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |  788 | `		}` |
|   409 |  789 | `		ph7_value_int64(pV,iTs);` |
|   409 |  790 | `		ph7_array_add_elem(pArr,0,pV);` |
|   409 |  791 | `		ph7_value_int64(pV,iOff);` |
|   409 |  792 | `		ph7_array_add_elem(pArr,0,pV);` |
|     - |  793 | `		/* int, not bool: 0 = no explicit offset, 1 = numeric offset/@epoch,` |
|     - |  794 | `		 * 2 = literal "Z" (php keeps the distinction in the zone name) */` |
|   409 |  795 | `		ph7_value_int64(pV,bOffSet);` |
|   409 |  796 | `		ph7_array_add_elem(pArr,0,pV);` |
|     - |  797 | `		/* [3] = microseconds parsed from a fractional-seconds part (0 when absent) */` |
|   409 |  798 | `		ph7_value_int64(pV,uSec);` |
|   409 |  799 | `		ph7_array_add_elem(pArr,0,pV);` |
|   409 |  800 | `		ph7_result_value(pCtx,pArr);` |
|     - |  801 | `	}` |
|   409 |  802 | `	return PH7_OK;` |
|   230 |  803 | `}` |
|     - |  804 | `/* string __dt_default_tz(void) — the date_default_timezone_set() identifier */` |
|   240 |  805 | `static int vm_builtin_dt_default_tz(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  806 | `{` |
|   120 |  807 | `	SXUNUSED(nArg);` |
|   120 |  808 | `	SXUNUSED(apArg);` |
|   241 |  809 | `	ph7_result_string(pCtx,pCtx->pVm->zDefTz,(int)pCtx->pVm->nDefTz);` |
|   241 |  810 | `	return PH7_OK;` |
|     1 |  811 | `}` |
|     - |  812 | `/* string __dt_format(int $ts, int $off, string $tzname, string $format, int $us = 0) */` |
|   182 |  813 | `static int vm_builtin_dt_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  814 | `{` |
|     - |  815 | `	Sytm sTm;` |
|     - |  816 | `	sxi64 iTs;` |
|     - |  817 | `	sxi32 iOff;` |
|     - |  818 | `	const char *zName,*zFmt;` |
|   183 |  819 | `	int nName,nFmt,uSec = 0;` |
|     - |  820 | `	char zZone[64];` |
|   183 |  821 | `	if( nArg < 4 ){` |
|   ! 0 |  822 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  823 | `		return PH7_OK;` |
|     - |  824 | `	}` |
|   183 |  825 | `	iTs  = ph7_value_to_int64(apArg[0]);` |
|   183 |  826 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|   183 |  827 | `	zName = ph7_value_to_string(apArg[2],&nName);` |
|   183 |  828 | `	zFmt  = ph7_value_to_string(apArg[3],&nFmt);` |
|   183 |  829 | `	if( nArg > 4 ){ uSec = ph7_value_to_int(apArg[4]); }` |
|   183 |  830 | `	if( nName >= (int)sizeof(zZone) ){ nName = (int)sizeof(zZone) - 1; }` |
|   183 |  831 | `	SyMemcpy(zName,zZone,(sxu32)nName);` |
|   183 |  832 | `	zZone[nName] = 0;` |
|   183 |  833 | `	DtFillSytm(iTs,iOff,zZone,&sTm);` |
|   183 |  834 | `	DateFormat(pCtx,zFmt,nFmt,&sTm,uSec);` |
|   183 |  835 | `	return PH7_OK;` |
|    92 |  836 | `}` |
|     - |  837 | `/* int __dt_make(int y, int mo, int d, int h, int i, int s, int off) */` |
|     8 |  838 | `static int vm_builtin_dt_make(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  839 | `{` |
|     - |  840 | `	sxi64 y;` |
|     - |  841 | `	int mo,d,h,mi,s;` |
|     - |  842 | `	sxi32 iOff;` |
|     9 |  843 | `	if( nArg < 7 ){` |
|   ! 0 |  844 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  845 | `		return PH7_OK;` |
|     - |  846 | `	}` |
|     9 |  847 | `	y   = ph7_value_to_int64(apArg[0]);` |
|     9 |  848 | `	mo  = ph7_value_to_int(apArg[1]);` |
|     9 |  849 | `	d   = ph7_value_to_int(apArg[2]);` |
|     9 |  850 | `	h   = ph7_value_to_int(apArg[3]);` |
|     9 |  851 | `	mi  = ph7_value_to_int(apArg[4]);` |
|     9 |  852 | `	s   = ph7_value_to_int(apArg[5]);` |
|     9 |  853 | `	iOff = (sxi32)ph7_value_to_int64(apArg[6]);` |
|     9 |  854 | `	ph7_result_int64(pCtx,DtMakeTs(y,mo,d,h,mi,s,iOff));` |
|     9 |  855 | `	return PH7_OK;` |
|     5 |  856 | `}` |
|     - |  857 | `/* Days in a civil month (php's overflow rules use it during diff borrows) */` |
|    62 |  858 | `static int DtDaysInMonth(sxi64 y,int m)` |
|     1 |  859 | `{` |
|     - |  860 | `	static const int aMonDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};` |
|    63 |  861 | `	if( m == 2 && ((y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0) ){` |
|     9 |  862 | `		return 29;` |
|     - |  863 | `	}` |
|    55 |  864 | `	return aMonDays[(m - 1) % 12];` |
|    32 |  865 | `}` |
|     - |  866 | `/* int __dt_civil_add(int ts, int off, int y, int m, int d, int h, int i,` |
|     - |  867 | ` *                    int s, int sign)` |
|     - |  868 | ` *   php's DateTime::add/sub: month arithmetic with linear day/time overflow` |
|     - |  869 | ` *   (Jan 31 + P1M == Mar 02), all in the instant's own fixed offset. */` |
|    54 |  870 | `static int vm_builtin_dt_civil_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  871 | `{` |
|     - |  872 | `	sxi64 iTs,iLocal,iDays,iSecs,y0,moT,dayCount;` |
|     - |  873 | `	sxi32 iOff;` |
|     - |  874 | `	int mo0,d0,iSign;` |
|     - |  875 | `	sxi64 y,m,d,h,i,s;` |
|    55 |  876 | `	if( nArg < 9 ){` |
|   ! 0 |  877 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  878 | `		return PH7_OK;` |
|     - |  879 | `	}` |
|    55 |  880 | `	iTs   = ph7_value_to_int64(apArg[0]);` |
|    55 |  881 | `	iOff  = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    55 |  882 | `	y     = ph7_value_to_int64(apArg[2]);` |
|    55 |  883 | `	m     = ph7_value_to_int64(apArg[3]);` |
|    55 |  884 | `	d     = ph7_value_to_int64(apArg[4]);` |
|    55 |  885 | `	h     = ph7_value_to_int64(apArg[5]);` |
|    55 |  886 | `	i     = ph7_value_to_int64(apArg[6]);` |
|    55 |  887 | `	s     = ph7_value_to_int64(apArg[7]);` |
|    55 |  888 | `	iSign = ph7_value_to_int(apArg[8]) < 0 ? -1 : 1;` |
|    55 |  889 | `	iLocal = iTs + iOff;` |
|    55 |  890 | `	iDays  = DtFloorDiv(iLocal,86400);` |
|    55 |  891 | `	iSecs  = iLocal - iDays*86400;` |
|    55 |  892 | `	DtCivilFromDays(iDays,&y0,&mo0,&d0);` |
|    55 |  893 | `	y0 += iSign * y;` |
|    55 |  894 | `	moT = (sxi64)(mo0 - 1) + iSign * m;` |
|    55 |  895 | `	y0 += DtFloorDiv(moT,12);` |
|    55 |  896 | `	moT -= DtFloorDiv(moT,12) * 12;` |
|    55 |  897 | `	dayCount = DtDaysFromCivil(y0,(int)moT + 1,1) + (d0 - 1) + iSign * d;` |
|    55 |  898 | `	iLocal = dayCount*86400 + iSecs + iSign * (h*3600 + i*60 + s);` |
|    55 |  899 | `	ph7_result_int64(pCtx,iLocal - iOff);` |
|    55 |  900 | `	return PH7_OK;` |
|    28 |  901 | `}` |
|     - |  902 | `/* array __dt_civil_diff(int ts1, int off1, int ts2)` |
|     - |  903 | ` *   -> [y,m,d,h,i,s,days,invert]: timelib's breakdown — field-wise deltas in` |
|     - |  904 | ` *   the FIRST operand's offset, then borrow seconds→minutes→hours→days, then` |
|     - |  905 | ` *   the day borrow walks whole months backward from the later date (that walk` |
|     - |  906 | ` *   is why Jan 31 → Mar 02 reports m=0 d=30, not "1 month"). */` |
|    14 |  907 | `static int vm_builtin_dt_civil_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  908 | `{` |
|     - |  909 | `	sxi64 iTs1,iTs2,iA,iB,iLa,iLb,daysA,daysB,yA,yB;` |
|     - |  910 | `	sxi32 iOff;` |
|     - |  911 | `	int moA,dA,moB,dB,bInvert;` |
|     - |  912 | `	sxi64 sA,sB,y,m,d,h,i,s;` |
|     - |  913 | `	ph7_value *pArr,*pV;` |
|    15 |  914 | `	if( nArg < 3 ){` |
|   ! 0 |  915 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  916 | `		return PH7_OK;` |
|     - |  917 | `	}` |
|    15 |  918 | `	iTs1 = ph7_value_to_int64(apArg[0]);` |
|    15 |  919 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|    15 |  920 | `	iTs2 = ph7_value_to_int64(apArg[2]);` |
|    15 |  921 | `	bInvert = iTs1 > iTs2;` |
|    15 |  922 | `	iA = bInvert ? iTs2 : iTs1;` |
|    15 |  923 | `	iB = bInvert ? iTs1 : iTs2;` |
|    15 |  924 | `	iLa = iA + iOff;` |
|    15 |  925 | `	iLb = iB + iOff;` |
|    15 |  926 | `	daysA = DtFloorDiv(iLa,86400);` |
|    15 |  927 | `	daysB = DtFloorDiv(iLb,86400);` |
|    15 |  928 | `	sA = iLa - daysA*86400;` |
|    15 |  929 | `	sB = iLb - daysB*86400;` |
|    15 |  930 | `	DtCivilFromDays(daysA,&yA,&moA,&dA);` |
|    15 |  931 | `	DtCivilFromDays(daysB,&yB,&moB,&dB);` |
|    15 |  932 | `	s = (sB % 60) - (sA % 60);` |
|    15 |  933 | `	i = ((sB / 60) % 60) - ((sA / 60) % 60);` |
|    15 |  934 | `	h = (sB / 3600) - (sA / 3600);` |
|    15 |  935 | `	d = dB - dA;` |
|    15 |  936 | `	m = moB - moA;` |
|    15 |  937 | `	y = yB - yA;` |
|    15 |  938 | `	if( s < 0 ){ s += 60; i--; }` |
|    15 |  939 | `	if( i < 0 ){ i += 60; h--; }` |
|    15 |  940 | `	if( h < 0 ){ h += 24; d--; }` |
|    27 |  941 | `	while( d < 0 ){` |
|    13 |  942 | `		moB--;` |
|    13 |  943 | `		if( moB < 1 ){ moB = 12; yB--; }` |
|    13 |  944 | `		d += DtDaysInMonth(yB,moB);` |
|    13 |  945 | `		m--;` |
|     1 |  946 | `	}` |
|    15 |  947 | `	if( m < 0 ){ m += 12; y--; }` |
|    15 |  948 | `	pArr = ph7_context_new_array(pCtx);` |
|    15 |  949 | `	pV = ph7_context_new_scalar(pCtx);` |
|    15 |  950 | `	if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 |  951 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  952 | `	}` |
|    15 |  953 | `	ph7_value_int64(pV,y);  ph7_array_add_elem(pArr,0,pV);` |
|    15 |  954 | `	ph7_value_int64(pV,m);  ph7_array_add_elem(pArr,0,pV);` |
|    15 |  955 | `	ph7_value_int64(pV,d);  ph7_array_add_elem(pArr,0,pV);` |
|    15 |  956 | `	ph7_value_int64(pV,h);  ph7_array_add_elem(pArr,0,pV);` |
|    15 |  957 | `	ph7_value_int64(pV,i);  ph7_array_add_elem(pArr,0,pV);` |
|    15 |  958 | `	ph7_value_int64(pV,s);  ph7_array_add_elem(pArr,0,pV);` |
|    15 |  959 | `	ph7_value_int64(pV,(iB - iA) / 86400); ph7_array_add_elem(pArr,0,pV);` |
|    15 |  960 | `	ph7_value_int64(pV,bInvert); ph7_array_add_elem(pArr,0,pV);` |
|    15 |  961 | `	ph7_result_value(pCtx,pArr);` |
|    15 |  962 | `	return PH7_OK;` |
|     8 |  963 | `}` |
|     - |  964 | `/* int __dt_isodate(int ts, int off, int y, int w, int dow)` |
|     - |  965 | ` *   setISODate: jump to ISO year/week/weekday, preserving the time of day. */` |
|     8 |  966 | `static int vm_builtin_dt_isodate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  967 | `{` |
|     - |  968 | `	sxi64 iTs,iLocal,iTod,jan4,monday1,target,y;` |
|     - |  969 | `	sxi32 iOff;` |
|     - |  970 | `	sxi64 w,dow;` |
|     - |  971 | `	int isoDow;` |
|     9 |  972 | `	if( nArg < 5 ){` |
|   ! 0 |  973 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  974 | `		return PH7_OK;` |
|     - |  975 | `	}` |
|     9 |  976 | `	iTs = ph7_value_to_int64(apArg[0]);` |
|     9 |  977 | `	iOff = (sxi32)ph7_value_to_int64(apArg[1]);` |
|     9 |  978 | `	y   = ph7_value_to_int64(apArg[2]);` |
|     9 |  979 | `	w   = ph7_value_to_int64(apArg[3]);` |
|     9 |  980 | `	dow = ph7_value_to_int64(apArg[4]);` |
|     9 |  981 | `	iLocal = iTs + iOff;` |
|     9 |  982 | `	iTod = iLocal - DtFloorDiv(iLocal,86400)*86400;` |
|     9 |  983 | `	jan4 = DtDaysFromCivil(y,1,4);` |
|     9 |  984 | `	isoDow = (int)(((jan4 + 3) % 7 + 7) % 7) + 1;` |
|     9 |  985 | `	monday1 = jan4 - (isoDow - 1);` |
|     9 |  986 | `	target = monday1 + (w - 1)*7 + (dow - 1);` |
|     9 |  987 | `	ph7_result_int64(pCtx,target*86400 + iTod - iOff);` |
|     9 |  988 | `	return PH7_OK;` |
|     5 |  989 | `}` |
|     - |  990 | `/* Consume nMin..nMax digits from *pz; returns count consumed (0 = failure) */` |
|   184 |  991 | `static int DtEatDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 |  992 | `{` |
|   185 |  993 | `	const char *z = *pz;` |
|   185 |  994 | `	sxi64 v = 0;` |
|   185 |  995 | `	int n = 0;` |
|   663 |  996 | `	while( z < zEnd && n < nMax && SyisDigit(z[0]) ){` |
|   479 |  997 | `		v = v*10 + (z[0] - '0');` |
|   479 |  998 | `		z++;` |
|   479 |  999 | `		n++;` |
|     1 | 1000 | `	}` |
|   185 | 1001 | `	if( n < nMin ){` |
|     5 | 1002 | `		return 0;` |
|     - | 1003 | `	}` |
|   181 | 1004 | `	*pz = z;` |
|   181 | 1005 | `	*pVal = v;` |
|   181 | 1006 | `	return n;` |
|    93 | 1007 | `}` |
|     - | 1008 | `/* timelib_get_nr's recovery: skip non-digits hunting for the field.` |
|     - | 1009 | ` * Returns 1 = found+read, 0 = digits present but short, -1 = exhausted. */` |
|     4 | 1010 | `static int DtHuntDigits(const char **pz,const char *zEnd,int nMin,int nMax,sxi64 *pVal)` |
|     1 | 1011 | `{` |
|     5 | 1012 | `	const char *z = *pz;` |
|    29 | 1013 | `	while( z < zEnd && !SyisDigit(z[0]) ){ z++; }` |
|     5 | 1014 | `	*pz = z;` |
|     5 | 1015 | `	if( z >= zEnd ){` |
|     5 | 1016 | `		return -1;` |
|     - | 1017 | `	}` |
|   ! 0 | 1018 | `	return DtEatDigits(pz,zEnd,nMin,nMax,pVal) ? 1 : 0;` |
|     3 | 1019 | `}` |
|     - | 1020 | `/* Case-insensitive name-table lookup; returns 1-based index or 0 */` |
|    14 | 1021 | `static int DtEatName(const char **pz,const char *zEnd,const char **azNames,int nNames)` |
|     1 | 1022 | `{` |
|     - | 1023 | `	int k;` |
|    23 | 1024 | `	for( k = 0 ; k < nNames ; k++ ){` |
|    23 | 1025 | `		int n = (int)SyStrlen(azNames[k]);` |
|    23 | 1026 | `		if( zEnd - *pz >= n && SyStrnicmp(*pz,azNames[k],(sxu32)n) == 0 ){` |
|    15 | 1027 | `			*pz += n;` |
|    15 | 1028 | `			return k + 1;` |
|     - | 1029 | `		}` |
|     5 | 1030 | `	}` |
|   ! 0 | 1031 | `	return 0;` |
|     8 | 1032 | `}` |
|     - | 1033 | `/* mixed __dt_from_format(string fmt, string input, int nowTs, int defOff)` |
|     - | 1034 | ` *   php's DateTime::createFromFormat engine. Success: [ts, off, offKind, name]` |
|     - | 1035 | ` *   where offKind 0=none-parsed, 1=numeric offset, 2=literal Z, 3=named id.` |
|     - | 1036 | ` *   Failure: "POS\tMESSAGE" (timelib's message strings; PHL reports the FIRST` |
|     - | 1037 | ` *   error where php may accumulate several — recorded). A trailing-data` |
|     - | 1038 | ` *   warning rides as [4]=pos, [5]=msg on the success array. */` |
|    58 | 1039 | `static int vm_builtin_dt_from_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1040 | `{` |
|     - | 1041 | `	static const char *azDay3[] = {"sun","mon","tue","wed","thu","fri","sat"};` |
|     - | 1042 | `	static const char *azDayFull[] = {"sunday","monday","tuesday","wednesday",` |
|     - | 1043 | `		"thursday","friday","saturday"};` |
|     - | 1044 | `	static const char *azMon3[] = {"jan","feb","mar","apr","may","jun","jul",` |
|     - | 1045 | `		"aug","sep","oct","nov","dec"};` |
|     - | 1046 | `	static const char *azMonFull[] = {"january","february","march","april",` |
|     - | 1047 | `		"may","june","july","august","september","october","november","december"};` |
|     - | 1048 | `	const char *zFmt,*zIn,*zEnd,*zInEnd,*z;` |
|     - | 1049 | `	int nFmt,nIn;` |
|     - | 1050 | `	sxi64 iNow,v;` |
|     - | 1051 | `	sxi32 iDefOff;` |
|     - | 1052 | `	/* -1 == unset */` |
|    59 | 1053 | `	sxi64 y = -1,mo = -1,d = -1,h = -1,mi = -1,s = -1,h12 = -1,uVal = 0;` |
|    59 | 1054 | `	int iMeridiem = -1,bHasU = 0,bPipe = 0,bPlus = 0;` |
|    59 | 1055 | `	int uSecFF = 0,bHasUs = 0;` |
|    59 | 1056 | `	int iOffKind = 0;` |
|    59 | 1057 | `	sxi32 iOffVal = 0;` |
|     - | 1058 | `	char zName[16];` |
|    59 | 1059 | `	const char *zErr = 0;` |
|     - | 1060 | `	const char *aWarnMsg[3];` |
|     - | 1061 | `	int aWarnPos[3];` |
|    59 | 1062 | `	int nWarn = 0,bAborted = 0;` |
|     - | 1063 | `	const char *aErrMsg[8];` |
|     - | 1064 | `	int aErrPos[8];` |
|    59 | 1065 | `	int nErr = 0,nErrKept = 0;` |
|    59 | 1066 | `	if( nArg < 4 ){` |
|   ! 0 | 1067 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1068 | `		return PH7_OK;` |
|     - | 1069 | `	}` |
|    59 | 1070 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|    59 | 1071 | `	zIn  = ph7_value_to_string(apArg[1],&nIn);` |
|    59 | 1072 | `	iNow = ph7_value_to_int64(apArg[2]);` |
|    59 | 1073 | `	iDefOff = (sxi32)ph7_value_to_int64(apArg[3]);` |
|    59 | 1074 | `	zEnd = &zFmt[nFmt];` |
|    59 | 1075 | `	zInEnd = &zIn[nIn];` |
|    59 | 1076 | `	z = zIn;` |
|    59 | 1077 | `	zName[0] = 0;` |
|     - | 1078 | `#define DT_FF_LOGERR(iPos,zMsg) \` |
|     - | 1079 | `	{ int _p = (iPos),_k,_f = -1; \` |
|     - | 1080 | `	  nErr++; \` |
|     - | 1081 | `	  for( _k = 0 ; _k < nErrKept ; _k++ ){ if( aErrPos[_k] == _p ){ _f = _k; break; } } \` |
|     - | 1082 | `	  if( _f >= 0 ){ aErrMsg[_f] = (zMsg); } \` |
|     - | 1083 | `	  else if( nErrKept < 8 ){ aErrPos[nErrKept] = _p; aErrMsg[nErrKept] = (zMsg); nErrKept++; } }` |
|   411 | 1084 | `	while( zFmt < zEnd ){` |
|   357 | 1085 | `		char c = zFmt[0];` |
|   357 | 1086 | `		zFmt++;` |
|   357 | 1087 | `		zErr = 0;` |
|   357 | 1088 | `		if( c == '!' ){` |
|    11 | 1089 | `			y = 1970; mo = 1; d = 1; h = 0; mi = 0; s = 0;` |
|    11 | 1090 | `			h12 = -1; iMeridiem = -1;` |
|    11 | 1091 | `			continue;` |
|     - | 1092 | `		}` |
|   347 | 1093 | `		if( c == '\|' ){ bPipe = 1; continue; }` |
|   343 | 1094 | `		if( c == '+' ){ bPlus = 1; continue; }` |
|   341 | 1095 | `		if( z >= zInEnd ){` |
|     - | 1096 | `			/* timelib aborts the scan once input is exhausted */` |
|     9 | 1097 | `			DT_FF_LOGERR(nIn,"Not enough data available to satisfy format");` |
|     5 | 1098 | `			break;` |
|     - | 1099 | `		}` |
|   337 | 1100 | `		switch( c ){` |
|    21 | 1101 | `		case 'd': case 'j':` |
|    43 | 1102 | `			if( !DtEatDigits(&z,zInEnd,1,2,&d) ){` |
|   ! 0 | 1103 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit day could not be found");` |
|   ! 0 | 1104 | `				if( DtHuntDigits(&z,zInEnd,1,2,&d) < 0 ){` |
|   ! 0 | 1105 | `					DT_FF_LOGERR(nIn,"A two digit day could not be found");` |
|   ! 0 | 1106 | `				}` |
|   ! 0 | 1107 | `			}` |
|    43 | 1108 | `			break;` |
|     1 | 1109 | `		case 'D':` |
|     3 | 1110 | `			if( !DtEatName(&z,zInEnd,azDay3,7) ){` |
|   ! 0 | 1111 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 1112 | `			}` |
|     3 | 1113 | `			break;` |
|     1 | 1114 | `		case 'l':` |
|     3 | 1115 | `			if( !DtEatName(&z,zInEnd,azDayFull,7) ){` |
|   ! 0 | 1116 | `				zErr = "A textual day could not be found";` |
|   ! 0 | 1117 | `			}` |
|     3 | 1118 | `			break;` |
|     1 | 1119 | `		case 'S':` |
|     - | 1120 | `			/* ordinal suffix: st nd rd th */` |
|     4 | 1121 | `			if( zInEnd-z >= 2 && ((z[0]=='s'&&z[1]=='t')\|\|(z[0]=='n'&&z[1]=='d')` |
|     2 | 1122 | `			 \|\|(z[0]=='r'&&z[1]=='d')\|\|(z[0]=='t'&&z[1]=='h')) ){` |
|     3 | 1123 | `				z += 2;` |
|     1 | 1124 | `			}` |
|     3 | 1125 | `			break;` |
|    19 | 1126 | `		case 'm': case 'n':` |
|    39 | 1127 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mo) ){` |
|   ! 0 | 1128 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit month could not be found");` |
|   ! 0 | 1129 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mo) < 0 ){` |
|   ! 0 | 1130 | `					DT_FF_LOGERR(nIn,"A two digit month could not be found");` |
|   ! 0 | 1131 | `				}` |
|   ! 0 | 1132 | `			}` |
|    39 | 1133 | `			break;` |
|     1 | 1134 | `		case 'M':{` |
|     3 | 1135 | `			int k = DtEatName(&z,zInEnd,azMon3,12);` |
|     3 | 1136 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 1137 | `			break;` |
|     - | 1138 | `				 }` |
|     1 | 1139 | `		case 'F':{` |
|     3 | 1140 | `			int k = DtEatName(&z,zInEnd,azMonFull,12);` |
|     3 | 1141 | `			if( k ){ mo = k; }else{ zErr = "A textual month could not be found"; }` |
|     3 | 1142 | `			break;` |
|     - | 1143 | `				 }` |
|   ! 0 | 1144 | `		case 'y':` |
|   ! 0 | 1145 | `			if( DtEatDigits(&z,zInEnd,2,2,&y) ){` |
|   ! 0 | 1146 | `				y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 1147 | `			}else{` |
|   ! 0 | 1148 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit year could not be found");` |
|   ! 0 | 1149 | `				if( DtHuntDigits(&z,zInEnd,2,2,&y) < 0 ){` |
|   ! 0 | 1150 | `					DT_FF_LOGERR(nIn,"A two digit year could not be found");` |
|   ! 0 | 1151 | `				}else if( y >= 0 ){` |
|   ! 0 | 1152 | `					y += (y <= 69) ? 2000 : 1900;` |
|   ! 0 | 1153 | `				}` |
|     - | 1154 | `			}` |
|   ! 0 | 1155 | `			break;` |
|    24 | 1156 | `		case 'Y':{` |
|    49 | 1157 | `			int neg = 0;` |
|    49 | 1158 | `			if( z < zInEnd && (z[0]=='-'\|\|z[0]=='+') ){ neg = (z[0]=='-'); z++; }` |
|    49 | 1159 | `			if( DtEatDigits(&z,zInEnd,1,4,&y) ){` |
|    45 | 1160 | `				if( neg ){ y = -y; }` |
|    23 | 1161 | `			}else{` |
|     5 | 1162 | `				DT_FF_LOGERR((int)(z - zIn),"A four digit year could not be found");` |
|     5 | 1163 | `				if( DtHuntDigits(&z,zInEnd,1,4,&y) < 0 ){` |
|     9 | 1164 | `					DT_FF_LOGERR(nIn,"A four digit year could not be found");` |
|     2 | 1165 | `				}` |
|     - | 1166 | `			}` |
|    49 | 1167 | `			break;` |
|     - | 1168 | `				 }` |
|     7 | 1169 | `		case 'H': case 'G':` |
|    15 | 1170 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h) ){` |
|   ! 0 | 1171 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 1172 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h) < 0 ){` |
|   ! 0 | 1173 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 1174 | `				}` |
|   ! 0 | 1175 | `			}` |
|    15 | 1176 | `			break;` |
|     2 | 1177 | `		case 'h': case 'g':` |
|     5 | 1178 | `			if( !DtEatDigits(&z,zInEnd,1,2,&h12) ){` |
|   ! 0 | 1179 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit hour could not be found");` |
|   ! 0 | 1180 | `				if( DtHuntDigits(&z,zInEnd,1,2,&h12) < 0 ){` |
|   ! 0 | 1181 | `					DT_FF_LOGERR(nIn,"A two digit hour could not be found");` |
|   ! 0 | 1182 | `				}` |
|   ! 0 | 1183 | `			}` |
|     5 | 1184 | `			break;` |
|     9 | 1185 | `		case 'i':` |
|    19 | 1186 | `			if( !DtEatDigits(&z,zInEnd,1,2,&mi) ){` |
|   ! 0 | 1187 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit minute could not be found");` |
|   ! 0 | 1188 | `				if( DtHuntDigits(&z,zInEnd,1,2,&mi) < 0 ){` |
|   ! 0 | 1189 | `					DT_FF_LOGERR(nIn,"A two digit minute could not be found");` |
|   ! 0 | 1190 | `				}` |
|   ! 0 | 1191 | `			}` |
|    19 | 1192 | `			break;` |
|     3 | 1193 | `		case 's':` |
|     7 | 1194 | `			if( !DtEatDigits(&z,zInEnd,1,2,&s) ){` |
|   ! 0 | 1195 | `				DT_FF_LOGERR((int)(z - zIn),"A two digit second could not be found");` |
|   ! 0 | 1196 | `				if( DtHuntDigits(&z,zInEnd,1,2,&s) < 0 ){` |
|   ! 0 | 1197 | `					DT_FF_LOGERR(nIn,"A two digit second could not be found");` |
|   ! 0 | 1198 | `				}` |
|   ! 0 | 1199 | `			}` |
|     7 | 1200 | `			break;` |
|     1 | 1201 | `		case 'u':{` |
|     - | 1202 | `			/* Microseconds: the digits parsed are right-padded to 6 (".5" -> 500000). */` |
|     3 | 1203 | `			const char *zStart = z;` |
|     3 | 1204 | `			if( !DtEatDigits(&z,zInEnd,1,6,&v) ){` |
|   ! 0 | 1205 | `				DT_FF_LOGERR((int)(z - zIn),"A six digit microsecond could not be found");` |
|   ! 0 | 1206 | `				if( DtHuntDigits(&z,zInEnd,1,6,&v) < 0 ){` |
|   ! 0 | 1207 | `					DT_FF_LOGERR(nIn,"A six digit microsecond could not be found");` |
|   ! 0 | 1208 | `				}else{` |
|   ! 0 | 1209 | `					zStart = z; /* HuntDigits repositioned; treat as freshly read */` |
|     - | 1210 | `				}` |
|   ! 0 | 1211 | `			}` |
|     - | 1212 | `			{` |
|     3 | 1213 | `				int nd = (int)(z - zStart);` |
|     3 | 1214 | `				while( nd > 0 && nd < 6 ){ v *= 10; nd++; }` |
|     3 | 1215 | `				uSecFF = (int)v; bHasUs = 1;` |
|     - | 1216 | `			}` |
|     3 | 1217 | `			break;` |
|     - | 1218 | `				 }` |
|   ! 0 | 1219 | `		case 'v':{` |
|   ! 0 | 1220 | `			const char *zStart = z;` |
|   ! 0 | 1221 | `			if( !DtEatDigits(&z,zInEnd,1,3,&v) ){` |
|   ! 0 | 1222 | `				DT_FF_LOGERR((int)(z - zIn),"A three digit millisecond could not be found");` |
|   ! 0 | 1223 | `				if( DtHuntDigits(&z,zInEnd,1,3,&v) < 0 ){` |
|   ! 0 | 1224 | `					DT_FF_LOGERR(nIn,"A three digit millisecond could not be found");` |
|   ! 0 | 1225 | `				}else{` |
|   ! 0 | 1226 | `					zStart = z;` |
|     - | 1227 | `				}` |
|   ! 0 | 1228 | `			}` |
|     - | 1229 | `			{` |
|   ! 0 | 1230 | `				int nd = (int)(z - zStart);` |
|   ! 0 | 1231 | `				while( nd > 0 && nd < 3 ){ v *= 10; nd++; }` |
|   ! 0 | 1232 | `				uSecFF = (int)v * 1000; bHasUs = 1; /* ms -> us */` |
|     - | 1233 | `			}` |
|   ! 0 | 1234 | `			break;` |
|     - | 1235 | `				 }` |
|     2 | 1236 | `		case 'a': case 'A':{` |
|     - | 1237 | `			static const char *azMer[] = {"am","pm","a.m.","p.m."};` |
|     5 | 1238 | `			int k = DtEatName(&z,zInEnd,azMer,4);` |
|     5 | 1239 | `			if( k ){` |
|     5 | 1240 | `				iMeridiem = ((k - 1) & 1);` |
|     3 | 1241 | `			}else{` |
|   ! 0 | 1242 | `				zErr = "A meridian could not be found";` |
|     - | 1243 | `			}` |
|     5 | 1244 | `			break;` |
|     - | 1245 | `				 }` |
|     2 | 1246 | `		case 'U':{` |
|     5 | 1247 | `			int neg = 0;` |
|     5 | 1248 | `			if( z < zInEnd && z[0]=='-' ){ neg = 1; z++; }` |
|     5 | 1249 | `			if( DtEatDigits(&z,zInEnd,1,19,&uVal) ){` |
|     5 | 1250 | `				if( neg ){ uVal = -uVal; }` |
|     5 | 1251 | `				bHasU = 1;` |
|     3 | 1252 | `			}else{` |
|   ! 0 | 1253 | `				DT_FF_LOGERR((int)(z - zIn),"A unix timestamp could not be found");` |
|   ! 0 | 1254 | `				if( DtHuntDigits(&z,zInEnd,1,19,&uVal) < 0 ){` |
|   ! 0 | 1255 | `					DT_FF_LOGERR(nIn,"A unix timestamp could not be found");` |
|   ! 0 | 1256 | `				}else{` |
|   ! 0 | 1257 | `					if( neg ){ uVal = -uVal; }` |
|   ! 0 | 1258 | `					bHasU = 1;` |
|     - | 1259 | `				}` |
|     - | 1260 | `			}` |
|     5 | 1261 | `			break;` |
|     - | 1262 | `				 }` |
|     1 | 1263 | `		case 'e': case 'T':{` |
|     - | 1264 | `			static const char *azZone[] = {"UTC","GMT","Z"};` |
|     3 | 1265 | `			int k = DtEatName(&z,zInEnd,azZone,3);` |
|     3 | 1266 | `			if( k == 3 ){` |
|   ! 0 | 1267 | `				iOffKind = 2; iOffVal = 0;` |
|     3 | 1268 | `			}else if( k ){` |
|     3 | 1269 | `				iOffKind = 3; iOffVal = 0;` |
|     3 | 1270 | `				SyMemcpy(azZone[k-1],zName,4);` |
|     1 | 1271 | `			}else if( z < zInEnd && (z[0]=='+' \|\| z[0]=='-') ){` |
|   ! 0 | 1272 | `				goto parse_num_off;` |
|   ! 0 | 1273 | `			}else{` |
|   ! 0 | 1274 | `				zErr = "The timezone could not be found in the database";` |
|     - | 1275 | `			}` |
|     3 | 1276 | `			break;` |
|     2 | 1277 | `				 }` |
|     - | 1278 | `		case 'O': case 'P':` |
|     2 | 1279 | `parse_num_off:	{` |
|     5 | 1280 | `			int sign,oh,om = 0;` |
|     - | 1281 | `			sxi64 t;` |
|     5 | 1282 | `			if( z >= zInEnd \|\| (z[0] != '+' && z[0] != '-') ){` |
|   ! 0 | 1283 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 1284 | `				break;` |
|     - | 1285 | `			}` |
|     5 | 1286 | `			sign = (z[0]=='-') ? -1 : 1;` |
|     5 | 1287 | `			z++;` |
|     5 | 1288 | `			if( !DtEatDigits(&z,zInEnd,2,2,&t) ){` |
|   ! 0 | 1289 | `				zErr = "The timezone could not be found in the database";` |
|   ! 0 | 1290 | `				break;` |
|     - | 1291 | `			}` |
|     5 | 1292 | `			oh = (int)t;` |
|     5 | 1293 | `			if( z < zInEnd && z[0]==':' ){ z++; }` |
|     5 | 1294 | `			if( DtEatDigits(&z,zInEnd,2,2,&t) ){ om = (int)t; }` |
|     5 | 1295 | `			iOffKind = 1;` |
|     5 | 1296 | `			iOffVal = sign * (oh*3600 + om*60);` |
|     5 | 1297 | `			break;` |
|     - | 1298 | `				 }` |
|   ! 0 | 1299 | `		case '?':` |
|   ! 0 | 1300 | `			if( z < zInEnd ){ z++; }` |
|   ! 0 | 1301 | `			break;` |
|   ! 0 | 1302 | `		case '*':` |
|     - | 1303 | `			/* skip input until the next separator byte */` |
|   ! 0 | 1304 | `			while( z < zInEnd && !SyisDigit(z[0]) && z[0] != ';' && z[0] != ':'` |
|   ! 0 | 1305 | `			 && z[0] != '/' && z[0] != '.' && z[0] != ',' && z[0] != '-'` |
|   ! 0 | 1306 | `			 && z[0] != '(' && z[0] != ')' && z[0] != ' ' ){` |
|   ! 0 | 1307 | `				z++;` |
|   ! 0 | 1308 | `			}` |
|   ! 0 | 1309 | `			break;` |
|     1 | 1310 | `		case '#':` |
|     3 | 1311 | `			if( z < zInEnd && (z[0]==';'\|\|z[0]==':'\|\|z[0]=='/'\|\|z[0]=='.'` |
|   ! 0 | 1312 | `			 \|\|z[0]==','\|\|z[0]=='-'\|\|z[0]=='('\|\|z[0]==')') ){` |
|     3 | 1313 | `				z++;` |
|     2 | 1314 | `			}else{` |
|   ! 0 | 1315 | `				zErr = "The separation symbol could not be found";` |
|     - | 1316 | `			}` |
|     3 | 1317 | `			break;` |
|     1 | 1318 | `		case '\\':` |
|     3 | 1319 | `			if( zFmt < zEnd ){` |
|     3 | 1320 | `				if( z < zInEnd && z[0] == zFmt[0] ){` |
|     3 | 1321 | `					z++;` |
|     3 | 1322 | `					zFmt++;` |
|     2 | 1323 | `				}else{` |
|     - | 1324 | `					/* a literal mismatch aborts timelib's scan */` |
|   ! 0 | 1325 | `					DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|   ! 0 | 1326 | `					zFmt = zEnd;` |
|   ! 0 | 1327 | `					bAborted = 1;` |
|     - | 1328 | `				}` |
|     1 | 1329 | `			}` |
|     3 | 1330 | `			break;` |
|    51 | 1331 | `		case ';': case ':': case '/': case '.': case ',': case '-':` |
|     - | 1332 | `		case '(' : case ')':` |
|   103 | 1333 | `			if( z < zInEnd && z[0] == c ){` |
|   103 | 1334 | `				z++;` |
|    52 | 1335 | `			}else{` |
|     - | 1336 | `				/* timelib logs BOTH messages (count +2, last-wins on the` |
|     - | 1337 | `				 * position), consumes the offending byte, and keeps going */` |
|   ! 0 | 1338 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 1339 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 1340 | `				z++;` |
|     - | 1341 | `			}` |
|   103 | 1342 | `			break;` |
|    16 | 1343 | `		case ' ':` |
|    33 | 1344 | `			if( z < zInEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|    33 | 1345 | `				z++;` |
|    17 | 1346 | `			}else{` |
|   ! 0 | 1347 | `				DT_FF_LOGERR((int)(z - zIn),"The separation symbol could not be found");` |
|   ! 0 | 1348 | `				DT_FF_LOGERR((int)(z - zIn),"Unexpected data found.");` |
|   ! 0 | 1349 | `				z++;` |
|     - | 1350 | `			}` |
|    33 | 1351 | `			break;` |
|     1 | 1352 | `		default:` |
|     - | 1353 | `			/* any other format byte must match the input verbatim; a mismatch` |
|     - | 1354 | `			 * aborts timelib's scan */` |
|     3 | 1355 | `			if( z < zInEnd && z[0] == c ){` |
|   ! 0 | 1356 | `				z++;` |
|   ! 0 | 1357 | `			}else{` |
|     3 | 1358 | `				DT_FF_LOGERR((int)(z - zIn),"The format separator does not match");` |
|     3 | 1359 | `				zFmt = zEnd;` |
|     3 | 1360 | `				bAborted = 1;` |
|     - | 1361 | `			}` |
|     2 | 1362 | `			break;` |
|     - | 1363 | `		}` |
|   337 | 1364 | `		if( zErr ){` |
|     - | 1365 | `			/* name/zone/separator mismatch: log and keep scanning (timelib) */` |
|   ! 0 | 1366 | `			DT_FF_LOGERR((int)(z - zIn),zErr);` |
|   ! 0 | 1367 | `		}` |
|     1 | 1368 | `	}` |
|    59 | 1369 | `	if( z < zInEnd && !bAborted ){` |
|     5 | 1370 | `		if( bPlus ){` |
|     - | 1371 | `			/* '+' downgrades trailing data to a warning */` |
|     3 | 1372 | `			aWarnPos[nWarn] = (int)(z - zIn);` |
|     3 | 1373 | `			aWarnMsg[nWarn] = "Trailing data";` |
|     3 | 1374 | `			nWarn++;` |
|     2 | 1375 | `		}else{` |
|     3 | 1376 | `			DT_FF_LOGERR((int)(z - zIn),"Trailing data");` |
|     - | 1377 | `		}` |
|     2 | 1378 | `	}` |
|    59 | 1379 | `	if( nErr > 0 ){` |
|     - | 1380 | `		SyBlob sOut;` |
|     - | 1381 | `		int k;` |
|     9 | 1382 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     9 | 1383 | `		SyBlobFormat(&sOut,"%d",nErr);` |
|    21 | 1384 | `		for( k = 0 ; k < nErrKept ; k++ ){` |
|    13 | 1385 | `			SyBlobFormat(&sOut,"\n%d\t%s",aErrPos[k],aErrMsg[k]);` |
|     7 | 1386 | `		}` |
|     9 | 1387 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     9 | 1388 | `		SyBlobRelease(&sOut);` |
|     9 | 1389 | `		return PH7_OK;` |
|     - | 1390 | `	}` |
|    51 | 1391 | `	if( bPipe ){` |
|     5 | 1392 | `		if( y < 0 ){ y = 1970; }` |
|     5 | 1393 | `		if( mo < 0 ){ mo = 1; }` |
|     5 | 1394 | `		if( d < 0 ){ d = 1; }` |
|     5 | 1395 | `		if( h < 0 && h12 < 0 ){ h = 0; }` |
|     5 | 1396 | `		if( mi < 0 ){ mi = 0; }` |
|     5 | 1397 | `		if( s < 0 ){ s = 0; }` |
|     2 | 1398 | `	}` |
|     - | 1399 | `	{` |
|     - | 1400 | `		/* remaining unset fields come from "now" in the default offset */` |
|    51 | 1401 | `		sxi64 iLocal = iNow + iDefOff;` |
|    51 | 1402 | `		sxi64 days = DtFloorDiv(iLocal,86400);` |
|    51 | 1403 | `		sxi64 secs = iLocal - days*86400;` |
|     - | 1404 | `		sxi64 ny;` |
|     - | 1405 | `		int nmo,nd;` |
|    51 | 1406 | `		DtCivilFromDays(days,&ny,&nmo,&nd);` |
|    51 | 1407 | `		if( y < 0 ){ y = ny; }` |
|    51 | 1408 | `		if( mo < 0 ){ mo = nmo; }` |
|    51 | 1409 | `		if( d < 0 ){ d = nd; }` |
|    51 | 1410 | `		if( h12 >= 0 ){` |
|     5 | 1411 | `			h = (h12 % 12) + ((iMeridiem == 1) ? 12 : 0);` |
|     2 | 1412 | `		}` |
|     - | 1413 | `		/* php: parsing a time component zeroes the finer unset units */` |
|    51 | 1414 | `		if( h >= 0 ){` |
|    27 | 1415 | `			if( mi < 0 ){ mi = 0; }` |
|    27 | 1416 | `			if( s < 0 ){ s = 0; }` |
|    38 | 1417 | `		}else if( mi >= 0 ){` |
|     3 | 1418 | `			if( s < 0 ){ s = 0; }` |
|     1 | 1419 | `		}` |
|    51 | 1420 | `		if( h < 0 ){ h = secs / 3600; }` |
|    51 | 1421 | `		if( mi < 0 ){ mi = (secs / 60) % 60; }` |
|    51 | 1422 | `		if( s < 0 ){ s = secs % 60; }` |
|     - | 1423 | `	}` |
|     - | 1424 | `	/* php validates the RESOLVED fields and warns (parse still succeeds,` |
|     - | 1425 | `	 * values roll over via civil arithmetic) */` |
|    51 | 1426 | `	if( mo < 1 \|\| mo > 12 \|\| d < 1 \|\| d > DtDaysInMonth(y,(int)mo) ){` |
|     3 | 1427 | `		if( nWarn < 3 ){` |
|     3 | 1428 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 1429 | `			aWarnMsg[nWarn] = "The parsed date was invalid";` |
|     3 | 1430 | `			nWarn++;` |
|     1 | 1431 | `		}` |
|     1 | 1432 | `	}` |
|    51 | 1433 | `	if( h > 24 \|\| mi > 59 \|\| s > 59 ){` |
|     3 | 1434 | `		if( nWarn < 3 ){` |
|     3 | 1435 | `			aWarnPos[nWarn] = nIn;` |
|     3 | 1436 | `			aWarnMsg[nWarn] = "The parsed time was invalid";` |
|     3 | 1437 | `			nWarn++;` |
|     1 | 1438 | `		}` |
|     1 | 1439 | `	}` |
|     - | 1440 | `	{` |
|    51 | 1441 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    51 | 1442 | `		ph7_value *pV = ph7_context_new_scalar(pCtx);` |
|     - | 1443 | `		sxi64 iTs;` |
|    51 | 1444 | `		sxi32 iUseOff = (iOffKind != 0) ? iOffVal : iDefOff;` |
|    51 | 1445 | `		if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 | 1446 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 1447 | `		}` |
|    51 | 1448 | `		if( bHasU ){` |
|     5 | 1449 | `			iTs = uVal;` |
|     5 | 1450 | `			iUseOff = 0;` |
|     5 | 1451 | `			iOffKind = 1;` |
|     3 | 1452 | `		}else{` |
|    47 | 1453 | `			iTs = DtMakeTs(y,(int)mo,(int)d,(int)h,(int)mi,(int)s,iUseOff);` |
|     - | 1454 | `		}` |
|    51 | 1455 | `		ph7_value_int64(pV,iTs);           ph7_array_add_elem(pArr,0,pV);` |
|    51 | 1456 | `		ph7_value_int64(pV,iUseOff);       ph7_array_add_elem(pArr,0,pV);` |
|    51 | 1457 | `		ph7_value_int64(pV,iOffKind);      ph7_array_add_elem(pArr,0,pV);` |
|    51 | 1458 | `		ph7_value_string(pV,zName,-1);     ph7_array_add_elem(pArr,0,pV);` |
|     - | 1459 | `		/* microseconds from a u/v token ride an associative key so they never` |
|     - | 1460 | `		 * collide with the numeric [4+] trailing-warning pairs */` |
|    51 | 1461 | `		if( bHasUs ){ ph7_value_int64(pV,uSecFF); ph7_array_add_strkey_elem(pArr,"us",pV); }` |
|     - | 1462 | `		{` |
|     - | 1463 | `			int k;` |
|    57 | 1464 | `			for( k = 0 ; k < nWarn ; k++ ){` |
|     7 | 1465 | `				ph7_value_int64(pV,aWarnPos[k]);` |
|     7 | 1466 | `				ph7_array_add_elem(pArr,0,pV);` |
|     7 | 1467 | `				ph7_value_string(pV,aWarnMsg[k],-1);` |
|     7 | 1468 | `				ph7_array_add_elem(pArr,0,pV);` |
|     4 | 1469 | `			}` |
|     - | 1470 | `		}` |
|    51 | 1471 | `		ph7_result_value(pCtx,pArr);` |
|     - | 1472 | `	}` |
|    51 | 1473 | `	return PH7_OK;` |
|    30 | 1474 | `}` |
|     - | 1475 | `/*` |
|     - | 1476 | ` * The embedded DateTime library. Timezone scope: UTC + fixed offsets.` |
|     - | 1477 | ` */` |
|     - | 1478 | `static const char zDateTimeLib[] =` |
|     - | 1479 | `"class DateException extends Exception {}"` |
|     - | 1480 | `"class DateMalformedStringException extends DateException {}"` |
|     - | 1481 | `"class DateInvalidTimeZoneException extends DateException {}"` |
|     - | 1482 | `"class DateMalformedIntervalStringException extends DateException {}"` |
|     - | 1483 | `"class DateMalformedPeriodStringException extends DateException {}"` |
|     - | 1484 | `"interface DateTimeInterface {"` |
|     - | 1485 | `" const ATOM = 'Y-m-d\\TH:i:sP';"` |
|     - | 1486 | `" const COOKIE = 'l, d-M-Y H:i:s T';"` |
|     - | 1487 | `" const ISO8601 = 'Y-m-d\\TH:i:sO';"` |
|     - | 1488 | `" const ISO8601_EXPANDED = 'X-m-d\\TH:i:sP';"` |
|     - | 1489 | `" const RFC822 = 'D, d M y H:i:s O';"` |
|     - | 1490 | `" const RFC850 = 'l, d-M-y H:i:s T';"` |
|     - | 1491 | `" const RFC1036 = 'D, d M y H:i:s O';"` |
|     - | 1492 | `" const RFC1123 = 'D, d M Y H:i:s O';"` |
|     - | 1493 | `" const RFC7231 = 'D, d M Y H:i:s \\G\\M\\T';"` |
|     - | 1494 | `" const RFC2822 = 'D, d M Y H:i:s O';"` |
|     - | 1495 | `" const RFC3339 = 'Y-m-d\\TH:i:sP';"` |
|     - | 1496 | `" const RFC3339_EXTENDED = 'Y-m-d\\TH:i:s.vP';"` |
|     - | 1497 | `" const RSS = 'D, d M Y H:i:s O';"` |
|     - | 1498 | `" const W3C = 'Y-m-d\\TH:i:sP';"` |
|     - | 1499 | `"}"` |
|     - | 1500 | `"class DateTimeZone {"` |
|     - | 1501 | `" private $__dtzOff = 0;"` |
|     - | 1502 | `" private $__dtzName = 'UTC';"` |
|     - | 1503 | `" public function __construct($timezone = 'UTC'){"` |
|     - | 1504 | `"  $tz = (string)$timezone;"` |
|     - | 1505 | `"  if( strcasecmp($tz, 'UTC') === 0 ){"` |
|     - | 1506 | `"   $this->__dtzOff = 0; $this->__dtzName = 'UTC';"` |
|     - | 1507 | `"   return;"` |
|     - | 1508 | `"  }"` |
|     - | 1509 | `"  if( $tz === 'Z' ){"` |
|     - | 1510 | `"   $this->__dtzOff = 0; $this->__dtzName = 'Z';"` |
|     - | 1511 | `"   return;"` |
|     - | 1512 | `"  }"` |
|     - | 1513 | `"  if( strcasecmp($tz, 'GMT') === 0 ){"` |
|     - | 1514 | `"   $this->__dtzOff = 0; $this->__dtzName = 'GMT';"` |
|     - | 1515 | `"   return;"` |
|     - | 1516 | `"  }"` |
|     - | 1517 | `"  $m = null;"` |
|     - | 1518 | `"  if( preg_match('/^([+-])(\\d{2}):?(\\d{2})$/', $tz, $m) ){"` |
|     - | 1519 | `"   $off = ((int)$m[2]) * 3600 + ((int)$m[3]) * 60;"` |
|     - | 1520 | `"   if( $m[1] === '-' ){ $off = -$off; }"` |
|     - | 1521 | `"   $this->__dtzOff = $off;"` |
|     - | 1522 | `"   $this->__dtzName = $m[1] . $m[2] . ':' . $m[3];"` |
|     - | 1523 | `"   return;"` |
|     - | 1524 | `"  }"` |
|     - | 1525 | `"  throw new DateInvalidTimeZoneException("` |
|     - | 1526 | `"   'DateTimeZone::__construct(): Unknown or bad timezone (' . $tz . ')');"` |
|     - | 1527 | `" }"` |
|     - | 1528 | `" public function getName(){ return $this->__dtzName; }"` |
|     - | 1529 | `" public function getOffset($datetime = null){ return $this->__dtzOff; }"` |
|     - | 1530 | `"}"` |
|     - | 1531 | `"trait __DtCoreT {"` |
|     - | 1532 | `" private $__dtTs = 0;"` |
|     - | 1533 | `" private $__dtOff = 0;"` |
|     - | 1534 | `" private $__dtName = 'UTC';"` |
|     - | 1535 | `" private $__dtUs = 0;"` |
|     - | 1536 | `" private function __dtInit($datetime, $timezone){"` |
|     - | 1537 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 1538 | `"  if( $timezone !== null ){"` |
|     - | 1539 | `"   $off = $timezone->getOffset($this);"` |
|     - | 1540 | `"   $name = $timezone->getName();"` |
|     - | 1541 | `"  }"` |
|     - | 1542 | `"  $r = __dt_parse((string)$datetime, __dt_now(), $off);"` |
|     - | 1543 | `"  if( is_string($r) ){ throw new DateMalformedStringException($r); }"` |
|     - | 1544 | `"  $this->__dtTs = $r[0];"` |
|     - | 1545 | `"  $this->__dtUs = $r[3];"` |
|     - | 1546 | `"  if( $r[2] ){"` |
|     - | 1547 | `"   $this->__dtOff = $r[1];"` |
|     - | 1548 | `"   $this->__dtName = $r[2] === 2 ? 'Z' : $this->__dtOffName($r[1]);"` |
|     - | 1549 | `"  }else{"` |
|     - | 1550 | `"   $this->__dtOff = $off;"` |
|     - | 1551 | `"   $this->__dtName = $name;"` |
|     - | 1552 | `"  }"` |
|     - | 1553 | `" }"` |
|     - | 1554 | `" private function __dtOffName($off){"` |
|     - | 1555 | `"  $s = $off < 0 ? '-' : '+';"` |
|     - | 1556 | `"  $a = $off < 0 ? -$off : $off;"` |
|     - | 1557 | `"  return $s . sprintf('%02d:%02d', intdiv($a, 3600), intdiv($a % 3600, 60));"` |
|     - | 1558 | `" }"` |
|     - | 1559 | `" public function format($format){ return __dt_format($this->__dtTs, $this->__dtOff, $this->__dtName, (string)$format, $this->__dtUs); }"` |
|     - | 1560 | `" public function getTimestamp(){ return $this->__dtTs; }"` |
|     - | 1561 | `" public function getMicrosecond(){ return $this->__dtUs; }"` |
|     - | 1562 | `" public function getOffset(){ return $this->__dtOff; }"` |
|     - | 1563 | `" public function getTimezone(){ return new DateTimeZone($this->__dtName); }"` |
|     - | 1564 | `" public function diff($targetObject, $absolute = false){"` |
|     - | 1565 | `"  $r = __dt_civil_diff($this->__dtTs, $this->__dtOff, $targetObject->getTimestamp());"` |
|     - | 1566 | `"  $iv = new DateInterval('P0D');"` |
|     - | 1567 | `"  $iv->y = $r[0]; $iv->m = $r[1]; $iv->d = $r[2];"` |
|     - | 1568 | `"  $iv->h = $r[3]; $iv->i = $r[4]; $iv->s = $r[5];"` |
|     - | 1569 | `"  $iv->days = $r[6];"` |
|     - | 1570 | `"  $iv->invert = $absolute ? 0 : $r[7];"` |
|     - | 1571 | `"  return $iv;"` |
|     - | 1572 | `" }"` |
|     - | 1573 | `" private function __dtAddTs($interval, $sign){"` |
|     - | 1574 | `"  if( $interval->invert ){ $sign = -$sign; }"` |
|     - | 1575 | `"  return __dt_civil_add($this->__dtTs, $this->__dtOff, $interval->y, $interval->m,"` |
|     - | 1576 | `"   $interval->d, $interval->h, $interval->i, $interval->s, $sign);"` |
|     - | 1577 | `" }"` |
|     - | 1578 | `" private static function __dtFromFormat($format, $datetime, $timezone, $class){"` |
|     - | 1579 | `"  $off = 0; $name = __dt_default_tz();"` |
|     - | 1580 | `"  if( $timezone !== null ){"` |
|     - | 1581 | `"   $off = $timezone->getOffset(null);"` |
|     - | 1582 | `"   $name = $timezone->getName();"` |
|     - | 1583 | `"  }"` |
|     - | 1584 | `"  $r = __dt_from_format((string)$format, (string)$datetime, __dt_now(), $off);"` |
|     - | 1585 | `"  if( is_string($r) ){"` |
|     - | 1586 | `"   $lines = explode(\"\\n\", $r);"` |
|     - | 1587 | `"   $errs = [];"` |
|     - | 1588 | `"   $nl = count($lines);"` |
|     - | 1589 | `"   for( $k = 1; $k < $nl; $k++ ){"` |
|     - | 1590 | `"    $p = strpos($lines[$k], \"\\t\");"` |
|     - | 1591 | `"    $errs[(int)substr($lines[$k], 0, $p)] = substr($lines[$k], $p + 1);"` |
|     - | 1592 | `"   }"` |
|     - | 1593 | `"   DateTime::$__dtLastErr = ['warning_count' => 0, 'warnings' => [],"` |
|     - | 1594 | `"    'error_count' => (int)$lines[0], 'errors' => $errs];"` |
|     - | 1595 | `"   return false;"` |
|     - | 1596 | `"  }"` |
|     - | 1597 | `"  if( isset($r[4]) ){"` |
|     - | 1598 | `"   $warns = [];"` |
|     - | 1599 | `"   $wc = 0;"` |
|     - | 1600 | `"   for( $k = 4; isset($r[$k]); $k += 2 ){"` |
|     - | 1601 | `"    $warns[$r[$k]] = $r[$k + 1];"` |
|     - | 1602 | `"    $wc++;"` |
|     - | 1603 | `"   }"` |
|     - | 1604 | `"   DateTime::$__dtLastErr = ['warning_count' => $wc, 'warnings' => $warns,"` |
|     - | 1605 | `"    'error_count' => 0, 'errors' => []];"` |
|     - | 1606 | `"  }else{"` |
|     - | 1607 | `"   DateTime::$__dtLastErr = false;"` |
|     - | 1608 | `"  }"` |
|     - | 1609 | `"  $obj = new $class('@0');"` |
|     - | 1610 | `"  $obj->__dtTs = $r[0];"` |
|     - | 1611 | `"  $obj->__dtUs = $r['us'] ?? 0;"` |
|     - | 1612 | `"  if( $r[2] === 0 ){ $obj->__dtOff = $off; $obj->__dtName = $name; }"` |
|     - | 1613 | `"  elseif( $r[2] === 2 ){ $obj->__dtOff = 0; $obj->__dtName = 'Z'; }"` |
|     - | 1614 | `"  elseif( $r[2] === 3 ){ $obj->__dtOff = $r[1]; $obj->__dtName = $r[3]; }"` |
|     - | 1615 | `"  else { $obj->__dtOff = $r[1]; $obj->__dtName = $obj->__dtOffName($r[1]); }"` |
|     - | 1616 | `"  return $obj;"` |
|     - | 1617 | `" }"` |
|     - | 1618 | `" private static function __dtCopyOf($object, $class){"` |
|     - | 1619 | `"  $d = new $class('@0');"` |
|     - | 1620 | `"  $d->__dtTs = $object->getTimestamp();"` |
|     - | 1621 | `"  $d->__dtUs = $object->getMicrosecond();"` |
|     - | 1622 | `"  $d->__dtOff = $object->getOffset();"` |
|     - | 1623 | `"  $d->__dtName = $object->getTimezone()->getName();"` |
|     - | 1624 | `"  return $d;"` |
|     - | 1625 | `" }"` |
|     - | 1626 | `"}"` |
|     - | 1627 | `"class DateTime implements DateTimeInterface {"` |
|     - | 1628 | `" use __DtCoreT;"` |
|     - | 1629 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 1630 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 1631 | `" }"` |
|     - | 1632 | `" public function modify($modifier){"` |
|     - | 1633 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 1634 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTime::modify(): ' . $r); }"` |
|     - | 1635 | `"  $this->__dtTs = $r[0];"` |
|     - | 1636 | `"  return $this;"` |
|     - | 1637 | `" }"` |
|     - | 1638 | `" public function setTimestamp($timestamp){ $this->__dtTs = (int)$timestamp; $this->__dtUs = 0; return $this; }"` |
|     - | 1639 | `" public function setMicrosecond($microsecond){ $this->__dtUs = (int)$microsecond; return $this; }"` |
|     - | 1640 | `" public function setTimezone($timezone){"` |
|     - | 1641 | `"  $this->__dtOff = $timezone->getOffset($this);"` |
|     - | 1642 | `"  $this->__dtName = $timezone->getName();"` |
|     - | 1643 | `"  return $this;"` |
|     - | 1644 | `" }"` |
|     - | 1645 | `" public function setDate($year, $month, $day){"` |
|     - | 1646 | `"  $this->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 1647 | `"  return $this;"` |
|     - | 1648 | `" }"` |
|     - | 1649 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 1650 | `"  $this->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 1651 | `"  $this->__dtUs = (int)$microsecond;"` |
|     - | 1652 | `"  return $this;"` |
|     - | 1653 | `" }"` |
|     - | 1654 | `" public function add($interval){ $this->__dtTs = $this->__dtAddTs($interval, 1); return $this; }"` |
|     - | 1655 | `" public function sub($interval){ $this->__dtTs = $this->__dtAddTs($interval, -1); return $this; }"` |
|     - | 1656 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 1657 | `"  $this->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 1658 | `"  return $this;"` |
|     - | 1659 | `" }"` |
|     - | 1660 | `" public static $__dtLastErr = false;"` |
|     - | 1661 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 1662 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 1663 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTime');"` |
|     - | 1664 | `" }"` |
|     - | 1665 | `" public static function createFromImmutable($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 1666 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTime'); }"` |
|     - | 1667 | `"}"` |
|     - | 1668 | `"class DateTimeImmutable implements DateTimeInterface {"` |
|     - | 1669 | `" use __DtCoreT;"` |
|     - | 1670 | `" public function __construct($datetime = 'now', $timezone = null){"` |
|     - | 1671 | `"  $this->__dtInit($datetime, $timezone);"` |
|     - | 1672 | `" }"` |
|     - | 1673 | `" public function modify($modifier){"` |
|     - | 1674 | `"  $r = __dt_parse((string)$modifier, $this->__dtTs, $this->__dtOff);"` |
|     - | 1675 | `"  if( is_string($r) ){ throw new DateMalformedStringException('DateTimeImmutable::modify(): ' . $r); }"` |
|     - | 1676 | `"  $c = clone $this;"` |
|     - | 1677 | `"  $c->__dtTs = $r[0];"` |
|     - | 1678 | `"  return $c;"` |
|     - | 1679 | `" }"` |
|     - | 1680 | `" public function setTimestamp($timestamp){ $c = clone $this; $c->__dtTs = (int)$timestamp; $c->__dtUs = 0; return $c; }"` |
|     - | 1681 | `" public function setMicrosecond($microsecond){ $c = clone $this; $c->__dtUs = (int)$microsecond; return $c; }"` |
|     - | 1682 | `" public function setTimezone($timezone){"` |
|     - | 1683 | `"  $c = clone $this;"` |
|     - | 1684 | `"  $c->__dtOff = $timezone->getOffset($this);"` |
|     - | 1685 | `"  $c->__dtName = $timezone->getName();"` |
|     - | 1686 | `"  return $c;"` |
|     - | 1687 | `" }"` |
|     - | 1688 | `" public function setDate($year, $month, $day){"` |
|     - | 1689 | `"  $c = clone $this;"` |
|     - | 1690 | `"  $c->__dtTs = __dt_make($year, $month, $day, (int)$this->format('G'), (int)$this->format('i'), (int)$this->format('s'), $this->__dtOff);"` |
|     - | 1691 | `"  return $c;"` |
|     - | 1692 | `" }"` |
|     - | 1693 | `" public function setTime($hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 1694 | `"  $c = clone $this;"` |
|     - | 1695 | `"  $c->__dtTs = __dt_make((int)$this->format('Y'), (int)$this->format('n'), (int)$this->format('j'), $hour, $minute, $second, $this->__dtOff);"` |
|     - | 1696 | `"  $c->__dtUs = (int)$microsecond;"` |
|     - | 1697 | `"  return $c;"` |
|     - | 1698 | `" }"` |
|     - | 1699 | `" public function add($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, 1); return $c; }"` |
|     - | 1700 | `" public function sub($interval){ $c = clone $this; $c->__dtTs = $this->__dtAddTs($interval, -1); return $c; }"` |
|     - | 1701 | `" public function setISODate($year, $week, $dayOfWeek = 1){"` |
|     - | 1702 | `"  $c = clone $this;"` |
|     - | 1703 | `"  $c->__dtTs = __dt_isodate($this->__dtTs, $this->__dtOff, $year, $week, $dayOfWeek);"` |
|     - | 1704 | `"  return $c;"` |
|     - | 1705 | `" }"` |
|     - | 1706 | `" public static function getLastErrors(){ return DateTime::$__dtLastErr; }"` |
|     - | 1707 | `" public static function createFromFormat($format, $datetime, $timezone = null){"` |
|     - | 1708 | `"  return self::__dtFromFormat($format, $datetime, $timezone, 'DateTimeImmutable');"` |
|     - | 1709 | `" }"` |
|     - | 1710 | `" public static function createFromMutable($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 1711 | `" public static function createFromInterface($object){ return self::__dtCopyOf($object, 'DateTimeImmutable'); }"` |
|     - | 1712 | `"}"` |
|     - | 1713 | `"function date_create($datetime = 'now', $timezone = null){"` |
|     - | 1714 | `" try { return new DateTime($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 1715 | `"}"` |
|     - | 1716 | `"function date_create_immutable($datetime = 'now', $timezone = null){"` |
|     - | 1717 | `" try { return new DateTimeImmutable($datetime, $timezone); } catch (Exception $e) { return false; }"` |
|     - | 1718 | `"}"` |
|     - | 1719 | `/* Procedural aliases of the createFromFormat statics: same (format, datetime,` |
|     - | 1720 | ` * ?timezone) order, returning false on failure like php. */` |
|     - | 1721 | `"function date_create_from_format($format, $datetime, $timezone = null){"` |
|     - | 1722 | `" return DateTime::createFromFormat($format, $datetime, $timezone);"` |
|     - | 1723 | `"}"` |
|     - | 1724 | `"function date_create_immutable_from_format($format, $datetime, $timezone = null){"` |
|     - | 1725 | `" return DateTimeImmutable::createFromFormat($format, $datetime, $timezone);"` |
|     - | 1726 | `"}"` |
|     - | 1727 | `"class DateInterval {"` |
|     - | 1728 | `" public $y = 0;"` |
|     - | 1729 | `" public $m = 0;"` |
|     - | 1730 | `" public $d = 0;"` |
|     - | 1731 | `" public $h = 0;"` |
|     - | 1732 | `" public $i = 0;"` |
|     - | 1733 | `" public $s = 0;"` |
|     - | 1734 | `" public $f = 0;"` |
|     - | 1735 | `" public $invert = 0;"` |
|     - | 1736 | `" public $days = false;"` |
|     - | 1737 | `" public $from_string = false;"` |
|     - | 1738 | `" public function __construct($duration = 'P0D'){"` |
|     - | 1739 | `"  $dur = (string)$duration;"` |
|     - | 1740 | `"  $mm = null;"` |
|     - | 1741 | `"  if( strlen($dur) < 2 \|\| substr($dur, -1) === 'T'"` |
|     - | 1742 | `"   \|\| !preg_match('/^P(?:(\\d+)Y)?(?:(\\d+)M)?(?:(\\d+)W)?(?:(\\d+)D)?(?:T(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?)?$/', $dur, $mm) ){"` |
|     - | 1743 | `"   throw new DateMalformedIntervalStringException('Unknown or bad format (' . $dur . ')');"` |
|     - | 1744 | `"  }"` |
|     - | 1745 | `"  $this->y = (int)($mm[1] ?? 0);"` |
|     - | 1746 | `"  $this->m = (int)($mm[2] ?? 0);"` |
|     - | 1747 | `"  $this->d = (int)($mm[4] ?? 0) + 7 * (int)($mm[3] ?? 0);"` |
|     - | 1748 | `"  $this->h = (int)($mm[5] ?? 0);"` |
|     - | 1749 | `"  $this->i = (int)($mm[6] ?? 0);"` |
|     - | 1750 | `"  $this->s = (int)($mm[7] ?? 0);"` |
|     - | 1751 | `" }"` |
|     - | 1752 | `" public static function createFromDateString($datetime){"` |
|     - | 1753 | `"  $s = trim((string)$datetime);"` |
|     - | 1754 | `"  $iv = new DateInterval('P0D');"` |
|     - | 1755 | `"  $rest = $s;"` |
|     - | 1756 | `"  $any = false;"` |
|     - | 1757 | `"  while( $rest !== '' ){"` |
|     - | 1758 | `"   $mm = null;"` |
|     - | 1759 | `"   if( !preg_match('/^[\\s,+]*([+-]?\\d+)\\s*(sec\|secs\|second\|seconds\|min\|mins\|minute\|minutes\|hour\|hours\|day\|days\|week\|weeks\|fortnight\|fortnights\|month\|months\|year\|years)\\b/i', $rest, $mm) ){"` |
|     - | 1760 | `"    throw new DateMalformedIntervalStringException("` |
|     - | 1761 | `"     'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 1762 | `"   }"` |
|     - | 1763 | `"   $n = (int)$mm[1];"` |
|     - | 1764 | `"   $u = strtolower($mm[2]);"` |
|     - | 1765 | `"   if( $u === 'sec' \|\| $u === 'secs' \|\| $u === 'second' \|\| $u === 'seconds' ){ $iv->s += $n; }"` |
|     - | 1766 | `"   elseif( $u === 'min' \|\| $u === 'mins' \|\| $u === 'minute' \|\| $u === 'minutes' ){ $iv->i += $n; }"` |
|     - | 1767 | `"   elseif( $u === 'hour' \|\| $u === 'hours' ){ $iv->h += $n; }"` |
|     - | 1768 | `"   elseif( $u === 'day' \|\| $u === 'days' ){ $iv->d += $n; }"` |
|     - | 1769 | `"   elseif( $u === 'week' \|\| $u === 'weeks' ){ $iv->d += 7 * $n; }"` |
|     - | 1770 | `"   elseif( $u === 'fortnight' \|\| $u === 'fortnights' ){ $iv->d += 14 * $n; }"` |
|     - | 1771 | `"   elseif( $u === 'month' \|\| $u === 'months' ){ $iv->m += $n; }"` |
|     - | 1772 | `"   else { $iv->y += $n; }"` |
|     - | 1773 | `"   $any = true;"` |
|     - | 1774 | `"   $rest = ltrim(substr($rest, strlen($mm[0])));"` |
|     - | 1775 | `"  }"` |
|     - | 1776 | `"  if( !$any ){"` |
|     - | 1777 | `"   throw new DateMalformedIntervalStringException("` |
|     - | 1778 | `"    'DateInterval::createFromDateString(): Unknown or bad format (' . $s . ')');"` |
|     - | 1779 | `"  }"` |
|     - | 1780 | `"  return $iv;"` |
|     - | 1781 | `" }"` |
|     - | 1782 | `" public function format($format){"` |
|     - | 1783 | `"  $f = (string)$format;"` |
|     - | 1784 | `"  $out = '';"` |
|     - | 1785 | `"  $n = strlen($f);"` |
|     - | 1786 | `"  for( $k = 0; $k < $n; $k++ ){"` |
|     - | 1787 | `"   $c = $f[$k];"` |
|     - | 1788 | `"   if( $c !== '%' ){ $out .= $c; continue; }"` |
|     - | 1789 | `"   $k++;"` |
|     - | 1790 | `"   if( $k >= $n ){ $out .= '%'; break; }"` |
|     - | 1791 | `"   $t = $f[$k];"` |
|     - | 1792 | `"   if( $t === 'Y' ){ $out .= sprintf('%02d', $this->y); }"` |
|     - | 1793 | `"   elseif( $t === 'y' ){ $out .= $this->y; }"` |
|     - | 1794 | `"   elseif( $t === 'M' ){ $out .= sprintf('%02d', $this->m); }"` |
|     - | 1795 | `"   elseif( $t === 'm' ){ $out .= $this->m; }"` |
|     - | 1796 | `"   elseif( $t === 'D' ){ $out .= sprintf('%02d', $this->d); }"` |
|     - | 1797 | `"   elseif( $t === 'd' ){ $out .= $this->d; }"` |
|     - | 1798 | `"   elseif( $t === 'H' ){ $out .= sprintf('%02d', $this->h); }"` |
|     - | 1799 | `"   elseif( $t === 'h' ){ $out .= $this->h; }"` |
|     - | 1800 | `"   elseif( $t === 'I' ){ $out .= sprintf('%02d', $this->i); }"` |
|     - | 1801 | `"   elseif( $t === 'i' ){ $out .= $this->i; }"` |
|     - | 1802 | `"   elseif( $t === 'S' ){ $out .= sprintf('%02d', $this->s); }"` |
|     - | 1803 | `"   elseif( $t === 's' ){ $out .= $this->s; }"` |
|     - | 1804 | `"   elseif( $t === 'F' ){ $out .= sprintf('%06d', (int)round($this->f * 1000000)); }"` |
|     - | 1805 | `"   elseif( $t === 'f' ){ $out .= (int)round($this->f * 1000000); }"` |
|     - | 1806 | `"   elseif( $t === 'R' ){ $out .= $this->invert ? '-' : '+'; }"` |
|     - | 1807 | `"   elseif( $t === 'r' ){ $out .= $this->invert ? '-' : ''; }"` |
|     - | 1808 | `"   elseif( $t === 'a' ){ $out .= $this->days === false ? '(unknown)' : $this->days; }"` |
|     - | 1809 | `"   elseif( $t === '%' ){ $out .= '%'; }"` |
|     - | 1810 | `"   else { $out .= $t; }"` |
|     - | 1811 | `"  }"` |
|     - | 1812 | `"  return $out;"` |
|     - | 1813 | `" }"` |
|     - | 1814 | `"}"` |
|     - | 1815 | `"class DatePeriod implements IteratorAggregate {"` |
|     - | 1816 | `" const EXCLUDE_START_DATE = 1;"` |
|     - | 1817 | `" const INCLUDE_END_DATE = 2;"` |
|     - | 1818 | `" public $start = null;"` |
|     - | 1819 | `" public $current = null;"` |
|     - | 1820 | `" public $end = null;"` |
|     - | 1821 | `" public $interval = null;"` |
|     - | 1822 | `" public $recurrences = 1;"` |
|     - | 1823 | `" public $include_start_date = true;"` |
|     - | 1824 | `" public $include_end_date = false;"` |
|     - | 1825 | `" private $__dpN = null;"` |
|     - | 1826 | `" public function __construct($start, $interval = null, $end = null, $options = 0){"` |
|     - | 1827 | `"  if( is_string($start) ){"` |
|     - | 1828 | `"   $mm = null;"` |
|     - | 1829 | `"   if( !preg_match('/^R(\\d+)\\/(.+)\\/(P.+)$/', $start, $mm) ){"` |
|     - | 1830 | `"    throw new DateMalformedPeriodStringException("` |
|     - | 1831 | `"     'DatePeriod::__construct(): Unknown or bad format (' . $start . ')');"` |
|     - | 1832 | `"   }"` |
|     - | 1833 | `"   $options = is_int($interval) ? $interval : 0;"` |
|     - | 1834 | `"   $this->start = new DateTimeImmutable($mm[2]);"` |
|     - | 1835 | `"   $this->interval = new DateInterval($mm[3]);"` |
|     - | 1836 | `"   $this->__dpN = (int)$mm[1];"` |
|     - | 1837 | `"   $this->recurrences = $this->__dpN + 1;"` |
|     - | 1838 | `"  }else{"` |
|     - | 1839 | `"   $this->start = clone $start;"` |
|     - | 1840 | `"   $this->interval = $interval;"` |
|     - | 1841 | `"   if( is_int($end) ){"` |
|     - | 1842 | `"    $this->__dpN = $end;"` |
|     - | 1843 | `"    $this->recurrences = $end + 1;"` |
|     - | 1844 | `"   }else{"` |
|     - | 1845 | `"    $this->end = $end === null ? null : (clone $end);"` |
|     - | 1846 | `"   }"` |
|     - | 1847 | `"  }"` |
|     - | 1848 | `"  $this->include_start_date = !((int)$options & 1);"` |
|     - | 1849 | `"  $this->include_end_date = ((int)$options & 2) !== 0;"` |
|     - | 1850 | `" }"` |
|     - | 1851 | `" public static function createFromISO8601String($specification, $options = 0){"` |
|     - | 1852 | `"  return new DatePeriod((string)$specification, (int)$options);"` |
|     - | 1853 | `" }"` |
|     - | 1854 | `" public function getStartDate(){ return $this->start; }"` |
|     - | 1855 | `" public function getEndDate(){ return $this->end; }"` |
|     - | 1856 | `" public function getDateInterval(){ return $this->interval; }"` |
|     - | 1857 | `" public function getRecurrences(){ return $this->__dpN; }"` |
|     - | 1858 | `" public function getIterator(): Generator {"` |
|     - | 1859 | `"  $cur = $this->start;"` |
|     - | 1860 | `"  $iv = $this->interval;"` |
|     - | 1861 | `"  $k = 0;"` |
|     - | 1862 | `"  if( $this->end !== null ){"` |
|     - | 1863 | `"   $endTs = $this->end->getTimestamp();"` |
|     - | 1864 | `"   $first = true;"` |
|     - | 1865 | `"   while( true ){"` |
|     - | 1866 | `"    $ts = $cur->getTimestamp();"` |
|     - | 1867 | `"    if( $this->include_end_date ? ($ts > $endTs) : ($ts >= $endTs) ){ break; }"` |
|     - | 1868 | `"    if( !$first \|\| $this->include_start_date ){"` |
|     - | 1869 | `"     yield $k => (clone $cur);"` |
|     - | 1870 | `"     $k++;"` |
|     - | 1871 | `"    }"` |
|     - | 1872 | `"    $first = false;"` |
|     - | 1873 | `"    $next = clone $cur;"` |
|     - | 1874 | `"    $cur = $next->add($iv);"` |
|     - | 1875 | `"   }"` |
|     - | 1876 | `"   return;"` |
|     - | 1877 | `"  }"` |
|     - | 1878 | `"  $total = $this->__dpN + 1 + ($this->include_end_date ? 1 : 0);"` |
|     - | 1879 | `"  for( $j = 0; $j < $total; $j++ ){"` |
|     - | 1880 | `"   if( $j > 0 \|\| $this->include_start_date ){"` |
|     - | 1881 | `"    yield $k => (clone $cur);"` |
|     - | 1882 | `"    $k++;"` |
|     - | 1883 | `"   }"` |
|     - | 1884 | `"   $next = clone $cur;"` |
|     - | 1885 | `"   $cur = $next->add($iv);"` |
|     - | 1886 | `"  }"` |
|     - | 1887 | `" }"` |
|     - | 1888 | `"}"` |
|     - | 1889 | `"function date_format($object, $format){ return $object->format($format); }"` |
|     - | 1890 | `"function date_modify($object, $modifier){"` |
|     - | 1891 | `" try { return $object->modify($modifier); } catch (Exception $e) { return false; }"` |
|     - | 1892 | `"}"` |
|     - | 1893 | `"function date_add($object, $interval){ return $object->add($interval); }"` |
|     - | 1894 | `"function date_sub($object, $interval){ return $object->sub($interval); }"` |
|     - | 1895 | `"function date_diff($baseObject, $targetObject, $absolute = false){"` |
|     - | 1896 | `" return $baseObject->diff($targetObject, $absolute);"` |
|     - | 1897 | `"}"` |
|     - | 1898 | `"function date_timestamp_get($object){ return $object->getTimestamp(); }"` |
|     - | 1899 | `"function date_timestamp_set($object, $timestamp){ return $object->setTimestamp($timestamp); }"` |
|     - | 1900 | `"function date_timezone_get($object){ return $object->getTimezone(); }"` |
|     - | 1901 | `"function date_timezone_set($object, $timezone){ return $object->setTimezone($timezone); }"` |
|     - | 1902 | `"function date_offset_get($object){ return $object->getOffset(); }"` |
|     - | 1903 | `"function date_date_set($object, $year, $month, $day){ return $object->setDate($year, $month, $day); }"` |
|     - | 1904 | `"function date_time_set($object, $hour, $minute, $second = 0, $microsecond = 0){"` |
|     - | 1905 | `" return $object->setTime($hour, $minute, $second, $microsecond);"` |
|     - | 1906 | `"}"` |
|     - | 1907 | `"function date_isodate_set($object, $year, $week, $dayOfWeek = 1){"` |
|     - | 1908 | `" return $object->setISODate($year, $week, $dayOfWeek);"` |
|     - | 1909 | `"}"` |
|     - | 1910 | `"function date_interval_create_from_date_string($datetime){"` |
|     - | 1911 | `" return DateInterval::createFromDateString($datetime);"` |
|     - | 1912 | `"}"` |
|     - | 1913 | `"function date_interval_format($object, $format){ return $object->format($format); }"` |
|     - | 1914 | `"function date_get_last_errors(){ return DateTime::getLastErrors(); }"` |
|     - | 1915 | `"function timezone_open($timezone){"` |
|     - | 1916 | `" try { return new DateTimeZone($timezone); } catch (Exception $e) { return false; }"` |
|     - | 1917 | `"}"` |
|     - | 1918 | `"function timezone_name_get($object){ return $object->getName(); }"` |
|     - | 1919 | `"function timezone_offset_get($object, $datetime){ return $object->getOffset($datetime); }"` |
|     - | 1920 | `/* int\|false strtotime(string $datetime, ?int $baseTimestamp = null). Rides the` |
|     - | 1921 | ` * same DtParse the DateTime constructor uses, so its format coverage is identical.` |
|     - | 1922 | ` * php: the EMPTY string is false, but whitespace-only is 'now'; a parse failure is` |
|     - | 1923 | ` * false (never an exception). The default timezone is treated as offset 0, exactly` |
|     - | 1924 | ` * as the DateTime constructor does for a null $timezone. */` |
|     - | 1925 | `"function strtotime($datetime, $baseTimestamp = null){"` |
|     - | 1926 | `" $s = (string)$datetime;"` |
|     - | 1927 | `" if( $s === '' ){ return false; }"` |
|     - | 1928 | `" $base = $baseTimestamp === null ? __dt_now() : (int)$baseTimestamp;"` |
|     - | 1929 | `" $r = __dt_parse($s, $base, 0);"` |
|     - | 1930 | `" return is_string($r) ? false : $r[0];"` |
|     - | 1931 | `"}"` |
|     - | 1932 | `;` |
|     - | 1933 | `/*` |
|     - | 1934 | ` * Install the DateTime family: thunks first, then the chunk. Called from` |
|     - | 1935 | ` * PH7_VmInit inside the bCompilingBuiltin window, after the Reflection` |
|     - | 1936 | ` * install (Exception must exist).` |
|     - | 1937 | ` */` |
|  3878 | 1938 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm)` |
|     5 | 1939 | `{` |
|     - | 1940 | `	static const struct {` |
|     - | 1941 | `		const char *zName;` |
|     - | 1942 | `		ProchHostFunction xFunc;` |
|     - | 1943 | `	} aFunc[] = {` |
|     - | 1944 | `		{ "__dt_now",    vm_builtin_dt_now },` |
|     - | 1945 | `		{ "__dt_default_tz", vm_builtin_dt_default_tz },` |
|     - | 1946 | `		{ "__dt_civil_add",  vm_builtin_dt_civil_add },` |
|     - | 1947 | `		{ "__dt_civil_diff", vm_builtin_dt_civil_diff },` |
|     - | 1948 | `		{ "__dt_isodate",    vm_builtin_dt_isodate },` |
|     - | 1949 | `		{ "__dt_from_format", vm_builtin_dt_from_format },` |
|     - | 1950 | `		{ "__dt_parse",  vm_builtin_dt_parse },` |
|     - | 1951 | `		{ "__dt_format", vm_builtin_dt_format },` |
|     - | 1952 | `		{ "__dt_make",   vm_builtin_dt_make },` |
|     - | 1953 | `	};` |
|     - | 1954 | `	sxu32 n;` |
|     - | 1955 | `	/* php's date.timezone default */` |
|  3883 | 1956 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|  3883 | 1957 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
| 38785 | 1958 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 34907 | 1959 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 17456 | 1960 | `	}` |
|  3883 | 1961 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zDateTimeLib,sizeof(zDateTimeLib)-1);` |
|     5 | 1962 | `}` |
|     - | 1963 |  |
|     - | 1964 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 1965 |  |
|     - | 1966 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 1967 | `/* Tiny build: no DateTime family (builtin layer disabled) */` |
|     - | 1968 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm){` |
|     - | 1969 | `	SyMemcpy("UTC",pVm->zDefTz,sizeof("UTC"));` |
|     - | 1970 | `	pVm->nDefTz = sizeof("UTC") - 1;` |
|     - | 1971 | `	return SXRET_OK;` |
|     - | 1972 | `}` |
|     - | 1973 | `#endif` |
|     - | 1974 |  |
