# src/ph7/builtin_date_tzdb.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 544/635 lines (85.67%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` *` |
|      - |    5 | ` * The IANA timezone database: a TZif reader over the embedded payload.` |
|      - |    6 | ` *` |
|      - |    7 | `` * Guarded by PH7_ENABLE_TZDB, which `full` and `coverage` set and `tiny` does`` |
|      - |    8 | ` * not -- the payload is ~296 KB, which is most of a tiny build. With the flag` |
|      - |    9 | ` * off every door below is absent and the date family keeps the fixed-offset` |
|      - |   10 | ` * answers it has always had: UTC, GMT, Z, php's military letters and the whole` |
|      - |   11 | `` * ±HH:MM:SS grammar. Nothing here is reachable from a `#ifdef`-free caller;`` |
|      - |   12 | ` * builtin_date_parse.c asks PH7_TzFind() first and falls through when it is` |
|      - |   13 | ` * not compiled in.` |
|      - |   14 | ` *` |
|      - |   15 | ` * WHAT A TZif FILE SAYS (RFC 8536). After a 44-byte header the version-2 block` |
|      - |   16 | `` * carries, in order: `timecnt` transition instants as big-endian signed 64-bit`` |
|      - |   17 | `` * seconds; `timecnt` one-byte indices into the type table; `typecnt` six-byte`` |
|      - |   18 | ` * ttinfo records (a 32-bit UT offset, an is-DST byte, an index into the` |
|      - |   19 | `` * abbreviation blob); `charcnt` bytes of NUL-separated abbreviations; the leap`` |
|      - |   20 | ` * table; the standard/wall and UT/local indicator arrays; and finally a` |
|      - |   21 | ` * newline-wrapped POSIX TZ string.` |
|      - |   22 | ` *` |
|      - |   23 | ` * THREE RANGES, and each is a different rule -- this is the whole reader:` |
|      - |   24 | ` *` |
|      - |   25 | ` *   BEFORE the first transition   the first ttinfo whose is-DST byte is clear,` |
|      - |   26 | ` *                                 or ttinfo 0 when every one of them is set.` |
|      - |   27 | ` *                                 That is where a zone's LMT lives, which is` |
|      - |   28 | `` *                                 why `America/New_York` in 1800 is -04:56.`` |
|      - |   29 | ` *   BETWEEN two transitions       the type the transition named. A binary` |
|      - |   30 | ` *                                 search, because a zone carries up to 366 of` |
|      - |   31 | ` *                                 them and every date formats through here.` |
|      - |   32 | ` *   AFTER the last transition     the FOOTER's rule, evaluated for that year.` |
|      - |   33 | ` *                                 The files stop at 2037 -- New York's last` |
|      - |   34 | ` *                                 explicit transition is 2037-11-01 -- and` |
|      - |   35 | ``  *                                 everything past it is `EST5EDT,M3.2.0,M11.1.0` `` |
|      - |   36 | ` *                                 read as a rule. A build that skipped this` |
|      - |   37 | ` *                                 would answer standard time for every date` |
|      - |   38 | ` *                                 after 2037 and look right in every test that` |
|      - |   39 | ` *                                 does not cross one.` |
|      - |   40 | ` *` |
|      - |   41 | ` * The two directions are NOT symmetric. Reading an offset off an INSTANT is a` |
|      - |   42 | ` * lookup; turning a WALL CLOCK into an instant is a search, because an hour` |
|      - |   43 | ` * that DST skips names no instant and an hour it repeats names two. php's rule` |
|      - |   44 | ` * for both is in PH7_TzLocalToUtc().` |
|      - |   45 | ` */` |
|      - |   46 | `#include "ph7int.h"` |
|      - |   47 |  |
|      - |   48 | `#if defined(PH7_ENABLE_TZDB) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|      - |   49 |  |
|      - |   50 | `#include "builtin_date_tzdb.h"` |
|      - |   51 |  |
|      - |   52 | `/* --- Reading the payload ------------------------------------------------- */` |
|      - |   53 |  |
|      - |   54 | `/* The header is fixed-width big-endian: magic(4) version(1) reserved(15) and` |
|      - |   55 | ` * then six 32-bit counts. Everything else is measured off those six. */` |
|      - |   56 | `#define TZ_HDR_SIZE 44` |
|      - |   57 |  |
|  46138 |   58 | `static sxu32 TzU32(const unsigned char *z)` |
|      4 |   59 | `{` |
|  46142 |   60 | `	return ((sxu32)z[0] << 24) \| ((sxu32)z[1] << 16) \| ((sxu32)z[2] << 8) \| (sxu32)z[3];` |
|      4 |   61 | `}` |
|   6358 |   62 | `static sxi32 TzI32(const unsigned char *z)` |
|      4 |   63 | `{` |
|      - |   64 | `	/* Spelled through the unsigned width and then narrowed, so the sign comes` |
|      - |   65 | `	 * from the cast rather than from a shift of a negative value. */` |
|   6362 |   66 | `	return (sxi32)TzU32(z);` |
|      4 |   67 | `}` |
|  21032 |   68 | `static sxi64 TzI64(const unsigned char *z)` |
|      4 |   69 | `{` |
|  21036 |   70 | `	sxu64 v = 0;` |
|      - |   71 | `	int i;` |
| 189292 |   72 | `	for( i = 0 ; i < 8 ; ++i ){` |
| 168260 |   73 | `		v = (v << 8) \| (sxu64)z[i];` |
|  84132 |   74 | `	}` |
|  21036 |   75 | `	return (sxi64)v;` |
|      4 |   76 | `}` |
|      - |   77 |  |
|      - |   78 | `/* One zone's block, laid out. Every pointer is into aTzPayload and outlives any` |
|      - |   79 | ` * caller: the table is static const. */` |
|      - |   80 | `typedef struct tz_block tz_block;` |
|      - |   81 | `struct tz_block` |
|      - |   82 | `{` |
|      - |   83 | `	const unsigned char *aTime;   /* timecnt × 8, big-endian seconds */` |
|      - |   84 | `	const unsigned char *aIdx;    /* timecnt × 1, indices into aType */` |
|      - |   85 | `	const unsigned char *aType;   /* typecnt × 6 */` |
|      - |   86 | `	const char *zChar;            /* charcnt bytes of NUL-separated names */` |
|      - |   87 | `	sxu32 nTime;` |
|      - |   88 | `	sxu32 nType;` |
|      - |   89 | `	sxu32 nChar;` |
|      - |   90 | `	const char *zPosix;           /* the footer rule, or 0 */` |
|      - |   91 | `	int nPosix;` |
|      - |   92 | `};` |
|      - |   93 |  |
|      - |   94 | `/*` |
|      - |   95 | ` * Lay out zone iZone. Answers 0 when the block is malformed -- a payload this` |
|      - |   96 | ` * engine generated cannot be, but the reader is written so that a truncated or` |
|      - |   97 | ` * hand-edited one refuses rather than walks off the end.` |
|      - |   98 | ` */` |
|   6630 |   99 | `static int TzBlock(int iZone,tz_block *pOut)` |
|      4 |  100 | `{` |
|      - |  101 | `	const unsigned char *z,*zEnd;` |
|      - |  102 | `	sxu32 nIsUt,nIsStd,nLeap,nNeed;` |
|   6634 |  103 | `	if( iZone < 0 \|\| iZone >= PH7_TZDB_ZONE_COUNT ){` |
|    ! 0 |  104 | `		return 0;` |
|      - |  105 | `	}` |
|   6634 |  106 | `	z = &aTzPayload[aTzZone[iZone].iOfst];` |
|   6634 |  107 | `	zEnd = z + aTzZone[iZone].nByte;` |
|   6634 |  108 | `	if( aTzZone[iZone].nByte < TZ_HDR_SIZE ){` |
|    ! 0 |  109 | `		return 0;` |
|      - |  110 | `	}` |
|   6634 |  111 | `	nIsUt  = TzU32(z + 20);` |
|   6634 |  112 | `	nIsStd = TzU32(z + 24);` |
|   6634 |  113 | `	nLeap  = TzU32(z + 28);` |
|   6634 |  114 | `	pOut->nTime = TzU32(z + 32);` |
|   6634 |  115 | `	pOut->nType = TzU32(z + 36);` |
|   6634 |  116 | `	pOut->nChar = TzU32(z + 40);` |
|   6634 |  117 | `	if( pOut->nType == 0 ){` |
|    ! 0 |  118 | `		return 0;` |
|      - |  119 | `	}` |
|      - |  120 | `	/* Sized before anything is read: the counts are attacker-shaped data in the` |
|      - |  121 | `	 * general case, and 9 × timecnt can overflow 32 bits on its own. */` |
|   6630 |  122 | `	if( pOut->nTime > 0x00FFFFFF \|\| pOut->nType > 0x00FFFF \|\| pOut->nChar > 0x00FFFF` |
|   6634 |  123 | `	 \|\| nLeap > 0x00FFFF \|\| nIsStd > 0x00FFFF \|\| nIsUt > 0x00FFFF ){` |
|    ! 0 |  124 | `		return 0;` |
|      - |  125 | `	}` |
|   9949 |  126 | `	nNeed = TZ_HDR_SIZE + pOut->nTime * 9 + pOut->nType * 6 + pOut->nChar` |
|   6630 |  127 | `	      + nLeap * 12 + nIsStd + nIsUt;` |
|   6634 |  128 | `	if( nNeed > aTzZone[iZone].nByte ){` |
|    ! 0 |  129 | `		return 0;` |
|      - |  130 | `	}` |
|   6634 |  131 | `	pOut->aTime = z + TZ_HDR_SIZE;` |
|   6634 |  132 | `	pOut->aIdx  = pOut->aTime + pOut->nTime * 8;` |
|   6634 |  133 | `	pOut->aType = pOut->aIdx + pOut->nTime;` |
|   6634 |  134 | `	pOut->zChar = (const char *)(pOut->aType + pOut->nType * 6);` |
|      - |  135 | ``	/* The footer is `\n` RULE `\n` at the very end of the block. An empty rule`` |
|      - |  136 | `	 * (the two newlines adjacent) is how a zone with no future transitions --` |
|      - |  137 | ``	 * `UTC`, `Etc/GMT+5` -- says so. */`` |
|   6634 |  138 | `	pOut->zPosix = 0;` |
|   6634 |  139 | `	pOut->nPosix = 0;` |
|   6634 |  140 | `	z = (const unsigned char *)pOut->zChar + pOut->nChar + nLeap * 12 + nIsStd + nIsUt;` |
|   6634 |  141 | `	if( z < zEnd && z[0] == '\n' ){` |
|   6634 |  142 | `		const unsigned char *zStop = z + 1;` |
| 158316 |  143 | `		while( zStop < zEnd && zStop[0] != '\n' ){` |
| 151686 |  144 | `			zStop++;` |
|      4 |  145 | `		}` |
|   6634 |  146 | `		if( zStop > z + 1 ){` |
|   6634 |  147 | `			pOut->zPosix = (const char *)(z + 1);` |
|   6634 |  148 | `			pOut->nPosix = (int)(zStop - (z + 1));` |
|   3315 |  149 | `		}` |
|   3315 |  150 | `	}` |
|   6634 |  151 | `	return 1;` |
|   3319 |  152 | `}` |
|      - |  153 |  |
|      - |  154 | `/* The abbreviation ttinfo iType names, bounded by the blob it indexes into. */` |
|   6358 |  155 | `static void TzAbbr(const tz_block *pB,sxu32 iType,const char **pzAbbr,int *pnAbbr)` |
|      4 |  156 | `{` |
|   6362 |  157 | `	sxu32 iChar = pB->aType[iType * 6 + 5];` |
|   6362 |  158 | `	sxu32 n = 0;` |
|   6362 |  159 | `	if( iChar >= pB->nChar ){` |
|    ! 0 |  160 | `		*pzAbbr = "";` |
|    ! 0 |  161 | `		*pnAbbr = 0;` |
|    ! 0 |  162 | `		return;` |
|      - |  163 | `	}` |
|  26810 |  164 | `	while( iChar + n < pB->nChar && pB->zChar[iChar + n] != 0 ){` |
|  20452 |  165 | `		n++;` |
|      4 |  166 | `	}` |
|   6362 |  167 | `	*pzAbbr = pB->zChar + iChar;` |
|   6362 |  168 | `	*pnAbbr = (int)n;` |
|   3183 |  169 | `}` |
|      - |  170 |  |
|      - |  171 | `/* The type a moment BEFORE the first transition takes: RFC 8536's rule, which` |
|      - |  172 | ` * is also what every zone's LMT row wants. */` |
|     84 |  173 | `static sxu32 TzFirstType(const tz_block *pB)` |
|      3 |  174 | `{` |
|      - |  175 | `	sxu32 i;` |
|     87 |  176 | `	for( i = 0 ; i < pB->nType ; ++i ){` |
|     87 |  177 | `		if( pB->aType[i * 6 + 4] == 0 ){` |
|     87 |  178 | `			return i;` |
|      - |  179 | `		}` |
|    ! 0 |  180 | `	}` |
|    ! 0 |  181 | `	return 0;` |
|     45 |  182 | `}` |
|      - |  183 |  |
|      - |  184 | `/* --- The POSIX footer ----------------------------------------------------- *` |
|      - |  185 | ` *` |
|      - |  186 | `` * `EST5EDT,M3.2.0,M11.1.0` -- a standard name and offset, an optional DST name`` |
|      - |  187 | ` * and offset, and the two dates the year switches on. The offset's SIGN is` |
|      - |  188 | ` * inverted against everything else here: POSIX writes the seconds to ADD to` |
|      - |  189 | `` * local time to reach UT, so `EST5` is -5 hours.`` |
|      - |  190 | ` */` |
|      - |  191 | `typedef struct tz_posix tz_posix;` |
|      - |  192 | `struct tz_posix` |
|      - |  193 | `{` |
|      - |  194 | `	const char *zStd;  int nStd;  sxi32 iStd;` |
|      - |  195 | `	const char *zDst;  int nDst;  sxi32 iDst;` |
|      - |  196 | `	int bHasDst;` |
|      - |  197 | `	/* Each rule is one of: J n (a 1-based day, never counting 29 Feb),` |
|      - |  198 | `	 * n (a 0-based day that does), or M m.w.d. */` |
|      - |  199 | `	int eStartKind,iStartM,iStartW,iStartD; sxi32 iStartSec;` |
|      - |  200 | `	int eEndKind,iEndM,iEndW,iEndD;         sxi32 iEndSec;` |
|      - |  201 | `};` |
|      - |  202 | `#define TZ_RULE_J 0` |
|      - |  203 | `#define TZ_RULE_N 1` |
|      - |  204 | `#define TZ_RULE_M 2` |
|      - |  205 |  |
|      - |  206 | ``/* A name is either a run of letters or a `<...>` quoted run (which is how a`` |
|      - |  207 | `` * numeric abbreviation like `<+07>` is spelled). */`` |
|    412 |  208 | `static int TzPosixName(const char **pz,const char *zEnd,const char **pzName,int *pnName)` |
|      3 |  209 | `{` |
|    415 |  210 | `	const char *z = *pz;` |
|    415 |  211 | `	if( z < zEnd && z[0] == '<' ){` |
|     25 |  212 | `		const char *zStart = ++z;` |
|     97 |  213 | `		while( z < zEnd && z[0] != '>' ){` |
|     73 |  214 | `			z++;` |
|      1 |  215 | `		}` |
|     25 |  216 | `		if( z >= zEnd ){` |
|    ! 0 |  217 | `			return 0;` |
|      - |  218 | `		}` |
|     25 |  219 | `		*pzName = zStart;` |
|     25 |  220 | `		*pnName = (int)(z - zStart);` |
|     25 |  221 | `		*pz = z + 1;` |
|     25 |  222 | `		return 1;` |
|      - |  223 | `	}` |
|      - |  224 | `	{` |
|    391 |  225 | `		const char *zStart = z;` |
|   1899 |  226 | `		while( z < zEnd && ((z[0] >= 'A' && z[0] <= 'Z') \|\| (z[0] >= 'a' && z[0] <= 'z')) ){` |
|   1317 |  227 | `			z++;` |
|      3 |  228 | `		}` |
|    391 |  229 | `		if( z - zStart < 3 ){` |
|    ! 0 |  230 | `			return 0;` |
|      - |  231 | `		}` |
|    391 |  232 | `		*pzName = zStart;` |
|    391 |  233 | `		*pnName = (int)(z - zStart);` |
|    391 |  234 | `		*pz = z;` |
|    391 |  235 | `		return 1;` |
|      - |  236 | `	}` |
|    209 |  237 | `}` |
|      - |  238 |  |
|      - |  239 | `/* A plain unsigned decimal run, at most six digits. */` |
|   1354 |  240 | `static int TzPosixNum(const char **pz,const char *zEnd,sxi32 *piOut)` |
|      3 |  241 | `{` |
|   1357 |  242 | `	const char *z = *pz;` |
|   1357 |  243 | `	sxi32 v = 0;` |
|   1357 |  244 | `	int nDigit = 0;` |
|   2927 |  245 | `	while( z < zEnd && z[0] >= '0' && z[0] <= '9' ){` |
|   1573 |  246 | `		v = v * 10 + (z[0] - '0');` |
|   1573 |  247 | `		z++;` |
|   1573 |  248 | `		if( ++nDigit > 6 ){` |
|    ! 0 |  249 | `			return 0;` |
|      - |  250 | `		}` |
|      3 |  251 | `	}` |
|   1357 |  252 | `	if( nDigit == 0 ){` |
|    ! 0 |  253 | `		return 0;` |
|      - |  254 | `	}` |
|   1357 |  255 | `	*piOut = v;` |
|   1357 |  256 | `	*pz = z;` |
|   1357 |  257 | `	return 1;` |
|    680 |  258 | `}` |
|      - |  259 |  |
|      - |  260 | `/*` |
|      - |  261 | `` * `[+-]hh[:mm[:ss]]`, answered as SIGNED SECONDS OF THE CLOCK -- the reading as`` |
|      - |  262 | ` * written, sign and all. The POSIX inversion (a zone's offset field counts` |
|      - |  263 | `` * seconds to ADD to reach UT, so `EST5` is five hours WEST) belongs to the two`` |
|      - |  264 | `` * callers that read a zone offset, not here: the `/time` field of a switch rule`` |
|      - |  265 | ` * is an ordinary clock reading and must not be flipped.` |
|      - |  266 | ` */` |
|    346 |  267 | `static int TzPosixClock(const char **pz,const char *zEnd,sxi32 *piOut)` |
|      3 |  268 | `{` |
|    349 |  269 | `	const char *z = *pz;` |
|    349 |  270 | `	int iSign = 1,nPart = 0;` |
|      - |  271 | `	sxi32 aPart[3];` |
|    349 |  272 | `	aPart[0] = aPart[1] = aPart[2] = 0;` |
|    349 |  273 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|    151 |  274 | `		iSign = z[0] == '-' ? -1 : 1;` |
|    151 |  275 | `		z++;` |
|     74 |  276 | `	}` |
|    173 |  277 | `	for(;;){` |
|    349 |  278 | `		if( !TzPosixNum(&z,zEnd,&aPart[nPart]) ){` |
|    ! 0 |  279 | `			return 0;` |
|      - |  280 | `		}` |
|    349 |  281 | `		nPart++;` |
|    349 |  282 | `		if( nPart == 3 \|\| z >= zEnd \|\| z[0] != ':' ){` |
|    176 |  283 | `			break;` |
|      - |  284 | `		}` |
|    ! 0 |  285 | `		z++;` |
|    ! 0 |  286 | `	}` |
|    349 |  287 | `	*piOut = iSign * (aPart[0] * 3600 + aPart[1] * 60 + aPart[2]);` |
|    349 |  288 | `	*pz = z;` |
|    349 |  289 | `	return 1;` |
|    176 |  290 | `}` |
|      - |  291 |  |
|      - |  292 | `/* A zone offset: the clock reading with POSIX's sign inverted, so what comes` |
|      - |  293 | ` * back is the seconds EAST of UT that the rest of this file speaks. */` |
|    244 |  294 | `static int TzPosixOffset(const char **pz,const char *zEnd,sxi32 *piOut)` |
|      3 |  295 | `{` |
|      - |  296 | `	sxi32 v;` |
|    247 |  297 | `	if( !TzPosixClock(pz,zEnd,&v) ){` |
|    ! 0 |  298 | `		return 0;` |
|      - |  299 | `	}` |
|    247 |  300 | `	*piOut = -v;` |
|    247 |  301 | `	return 1;` |
|    125 |  302 | `}` |
|      - |  303 |  |
|      - |  304 | ``/* `,Mm.w.d[/time]`, `,Jn[/time]` or `,n[/time]`. */`` |
|    336 |  305 | `static int TzPosixRule(const char **pz,const char *zEnd,int *peKind,int *piM,int *piW,` |
|      - |  306 | `	int *piD,sxi32 *piSec)` |
|      3 |  307 | `{` |
|    339 |  308 | `	const char *z = *pz;` |
|    339 |  309 | `	sxi32 iSec = 2 * 3600;   /* POSIX's default switch time is 02:00 local */` |
|    339 |  310 | `	if( z >= zEnd ){` |
|    ! 0 |  311 | `		return 0;` |
|      - |  312 | `	}` |
|    339 |  313 | `	if( z[0] == 'M' ){` |
|      - |  314 | `		sxi32 a,b,c;` |
|    339 |  315 | `		z++;` |
|    339 |  316 | `		if( !TzPosixNum(&z,zEnd,&a) \|\| z >= zEnd \|\| z[0] != '.' ){` |
|    ! 0 |  317 | `			return 0;` |
|      - |  318 | `		}` |
|    339 |  319 | `		z++;` |
|    339 |  320 | `		if( !TzPosixNum(&z,zEnd,&b) \|\| z >= zEnd \|\| z[0] != '.' ){` |
|    ! 0 |  321 | `			return 0;` |
|      - |  322 | `		}` |
|    339 |  323 | `		z++;` |
|    339 |  324 | `		if( !TzPosixNum(&z,zEnd,&c) ){` |
|    ! 0 |  325 | `			return 0;` |
|      - |  326 | `		}` |
|    339 |  327 | `		*peKind = TZ_RULE_M;` |
|    339 |  328 | `		*piM = (int)a;` |
|    339 |  329 | `		*piW = (int)b;` |
|    339 |  330 | `		*piD = (int)c;` |
|    339 |  331 | `		if( *piM < 1 \|\| *piM > 12 \|\| *piW < 1 \|\| *piW > 5 \|\| *piD < 0 \|\| *piD > 6 ){` |
|    ! 0 |  332 | `			return 0;` |
|      - |  333 | `		}` |
|    171 |  334 | `	}else{` |
|    ! 0 |  335 | `		int bJ = 0;` |
|      - |  336 | `		sxi32 n;` |
|    ! 0 |  337 | `		if( z[0] == 'J' ){` |
|    ! 0 |  338 | `			bJ = 1;` |
|    ! 0 |  339 | `			z++;` |
|    ! 0 |  340 | `		}` |
|    ! 0 |  341 | `		if( !TzPosixNum(&z,zEnd,&n) ){` |
|    ! 0 |  342 | `			return 0;` |
|      - |  343 | `		}` |
|    ! 0 |  344 | `		*peKind = bJ ? TZ_RULE_J : TZ_RULE_N;` |
|    ! 0 |  345 | `		*piM = *piW = 0;` |
|    ! 0 |  346 | `		*piD = (int)n;` |
|      - |  347 | `	}` |
|    339 |  348 | `	if( z < zEnd && z[0] == '/' ){` |
|    105 |  349 | `		z++;` |
|    105 |  350 | `		if( !TzPosixClock(&z,zEnd,&iSec) ){` |
|    ! 0 |  351 | `			return 0;` |
|      - |  352 | `		}` |
|     51 |  353 | `	}` |
|    339 |  354 | `	*piSec = iSec;` |
|    339 |  355 | `	*pz = z;` |
|    339 |  356 | `	return 1;` |
|    171 |  357 | `}` |
|      - |  358 |  |
|    244 |  359 | `static int TzPosixParse(const char *zIn,int nIn,tz_posix *pOut)` |
|      3 |  360 | `{` |
|    247 |  361 | `	const char *z = zIn,*zEnd = zIn + nIn;` |
|    247 |  362 | `	SyZero(pOut,sizeof(*pOut));` |
|    247 |  363 | `	if( !TzPosixName(&z,zEnd,&pOut->zStd,&pOut->nStd) ){` |
|    ! 0 |  364 | `		return 0;` |
|      - |  365 | `	}` |
|    247 |  366 | `	if( !TzPosixOffset(&z,zEnd,&pOut->iStd) ){` |
|    ! 0 |  367 | `		return 0;` |
|      - |  368 | `	}` |
|    247 |  369 | `	if( z >= zEnd ){` |
|     78 |  370 | ``		return 1;   /* a zone with no DST at all: `<+07>-7` */`` |
|      - |  371 | `	}` |
|    171 |  372 | `	if( !TzPosixName(&z,zEnd,&pOut->zDst,&pOut->nDst) ){` |
|    ! 0 |  373 | `		return 0;` |
|      - |  374 | `	}` |
|    171 |  375 | `	pOut->bHasDst = 1;` |
|      - |  376 | `	/* The DST offset may be left out, and then it is one hour east of standard. */` |
|    171 |  377 | `	if( z < zEnd && z[0] != ',' ){` |
|    ! 0 |  378 | `		if( !TzPosixOffset(&z,zEnd,&pOut->iDst) ){` |
|    ! 0 |  379 | `			return 0;` |
|      - |  380 | `		}` |
|    ! 0 |  381 | `	}else{` |
|    171 |  382 | `		pOut->iDst = pOut->iStd + 3600;` |
|      - |  383 | `	}` |
|    171 |  384 | `	if( z >= zEnd \|\| z[0] != ',' ){` |
|    ! 0 |  385 | `		return 0;` |
|      - |  386 | `	}` |
|    171 |  387 | `	z++;` |
|    255 |  388 | `	if( !TzPosixRule(&z,zEnd,&pOut->eStartKind,&pOut->iStartM,&pOut->iStartW,` |
|     84 |  389 | `			&pOut->iStartD,&pOut->iStartSec) ){` |
|    ! 0 |  390 | `		return 0;` |
|      - |  391 | `	}` |
|    171 |  392 | `	if( z >= zEnd \|\| z[0] != ',' ){` |
|    ! 0 |  393 | `		return 0;` |
|      - |  394 | `	}` |
|    171 |  395 | `	z++;` |
|    255 |  396 | `	if( !TzPosixRule(&z,zEnd,&pOut->eEndKind,&pOut->iEndM,&pOut->iEndW,` |
|     84 |  397 | `			&pOut->iEndD,&pOut->iEndSec) ){` |
|    ! 0 |  398 | `		return 0;` |
|      - |  399 | `	}` |
|    171 |  400 | `	return z == zEnd;` |
|    125 |  401 | `}` |
|      - |  402 |  |
|    ! 0 |  403 | `static int TzIsLeap(sxi64 y)` |
|    ! 0 |  404 | `{` |
|    ! 0 |  405 | `	return (y % 4 == 0 && y % 100 != 0) \|\| y % 400 == 0;` |
|    ! 0 |  406 | `}` |
|      - |  407 |  |
|      - |  408 | `/*` |
|      - |  409 | ` * The LOCAL-time instant a rule names inside year y, as seconds since the` |
|      - |  410 | ` * epoch with the rule's own clock reading folded in. The caller converts it to` |
|      - |  411 | ` * UT with whichever offset was in force before the switch.` |
|      - |  412 | ` */` |
|    452 |  413 | `static sxi64 TzRuleInstant(int eKind,int iM,int iW,int iD,sxi32 iSec,sxi64 y)` |
|      3 |  414 | `{` |
|      - |  415 | `	static const int aLen[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };` |
|      - |  416 | `	sxi64 iDay;` |
|    455 |  417 | `	if( eKind == TZ_RULE_M ){` |
|    455 |  418 | `		int nLen = aLen[iM - 1] + (iM == 2 && TzIsLeap(y) ? 1 : 0);` |
|    455 |  419 | `		sxi64 iFirst = DtDaysFromCivil(y,iM,1);` |
|      - |  420 | `		/* 1970-01-01 was a Thursday, so day 0 is weekday 4 with Sunday at 0. */` |
|    455 |  421 | `		int iWd = (int)(((iFirst % 7) + 7 + 4) % 7);` |
|    455 |  422 | `		int iShift = ((iD - iWd) + 7) % 7;` |
|    455 |  423 | `		int iMday = 1 + iShift + (iW - 1) * 7;` |
|    517 |  424 | `		while( iMday > nLen ){` |
|     65 |  425 | `			iMday -= 7;   /* week 5 means "the last one", however many there are */` |
|      3 |  426 | `		}` |
|    455 |  427 | `		iDay = DtDaysFromCivil(y,iM,iMday);` |
|    226 |  428 | `	}else if( eKind == TZ_RULE_J ){` |
|      - |  429 | `		/* J counts 1..365 and never counts 29 February, so from March on it` |
|      - |  430 | `		 * lands a day later in a leap year than the plain count would. */` |
|    ! 0 |  431 | `		int n = iD;` |
|    ! 0 |  432 | `		if( n >= 60 && TzIsLeap(y) ){` |
|    ! 0 |  433 | `			n++;` |
|    ! 0 |  434 | `		}` |
|    ! 0 |  435 | `		iDay = DtDaysFromCivil(y,1,1) + (n - 1);` |
|    ! 0 |  436 | `	}else{` |
|    ! 0 |  437 | `		iDay = DtDaysFromCivil(y,1,1) + iD;` |
|      - |  438 | `	}` |
|    455 |  439 | `	return iDay * (sxi64)86400 + iSec;` |
|      3 |  440 | `}` |
|      - |  441 |  |
|      - |  442 | `/*` |
|      - |  443 | ` * Answer the footer's rule for instant iTs. Both switch instants are LOCAL` |
|      - |  444 | ` * readings, so each is converted with the offset in force just BEFORE it --` |
|      - |  445 | ` * standard time before the spring switch, DST before the autumn one -- which is` |
|      - |  446 | ` * what makes a southern-hemisphere zone (whose DST spans the new year) come out` |
|      - |  447 | ` * right.` |
|      - |  448 | ` */` |
|    240 |  449 | `static void TzPosixAt(const tz_posix *pP,sxi64 iTs,sxi32 *piOff,int *pbDst,` |
|      - |  450 | `	const char **pzAbbr,int *pnAbbr)` |
|      3 |  451 | `{` |
|      - |  452 | `	sxi64 y;` |
|      - |  453 | `	int m,d;` |
|      - |  454 | `	sxi64 iStart,iEnd;` |
|      - |  455 | `	int bDst;` |
|    243 |  456 | `	if( !pP->bHasDst ){` |
|     74 |  457 | `		*piOff = pP->iStd;` |
|     74 |  458 | `		*pbDst = 0;` |
|     74 |  459 | `		*pzAbbr = pP->zStd;` |
|     74 |  460 | `		*pnAbbr = pP->nStd;` |
|     74 |  461 | `		return;` |
|      - |  462 | `	}` |
|    171 |  463 | `	DtCivilFromDays(DtFloorDiv(iTs + pP->iStd,86400),&y,&m,&d);` |
|    423 |  464 | `	iStart = TzRuleInstant(pP->eStartKind,pP->iStartM,pP->iStartW,pP->iStartD,` |
|    252 |  465 | `		pP->iStartSec,y) - pP->iStd;` |
|    423 |  466 | `	iEnd = TzRuleInstant(pP->eEndKind,pP->iEndM,pP->iEndW,pP->iEndD,` |
|    252 |  467 | `		pP->iEndSec,y) - pP->iDst;` |
|    171 |  468 | `	if( iStart <= iEnd ){` |
|    123 |  469 | `		bDst = (iTs >= iStart && iTs < iEnd);` |
|     63 |  470 | `	}else{` |
|      - |  471 | `		/* Southern hemisphere: DST runs from the start date to the END DATE OF` |
|      - |  472 | `		 * THE NEXT YEAR, so a moment is standard time only in the gap between. */` |
|     51 |  473 | `		bDst = !(iTs >= iEnd && iTs < iStart);` |
|      - |  474 | `	}` |
|    171 |  475 | `	*piOff = bDst ? pP->iDst : pP->iStd;` |
|    171 |  476 | `	*pbDst = bDst;` |
|    171 |  477 | `	*pzAbbr = bDst ? pP->zDst : pP->zStd;` |
|    171 |  478 | `	*pnAbbr = bDst ? pP->nDst : pP->nStd;` |
|    123 |  479 | `}` |
|      - |  480 |  |
|      - |  481 | `/* --- The public doors ----------------------------------------------------- */` |
|      - |  482 |  |
|      - |  483 | `/*` |
|      - |  484 | ` * Resolve an identifier, folding case: php looks the name up case-insensitively` |
|      - |  485 | ``  * and then stores the CALLER's spelling, so `new DateTimeZone('europe/paris')` `` |
|      - |  486 | `` * is named `europe/paris` and still knows about Paris. Answers a row index or`` |
|      - |  487 | ` * -1.` |
|      - |  488 | ` *` |
|      - |  489 | ` * aTzFold is the row order sorted on the FOLDED name, which is what lets this` |
|      - |  490 | ` * be a binary search over a table the generator sorted byte-wise for` |
|      - |  491 | `` * `timezone_identifiers_list()`.`` |
|      - |  492 | ` */` |
|   2750 |  493 | `PH7_PRIVATE int PH7_TzFind(const char *zName,int nName)` |
|      5 |  494 | `{` |
|   2755 |  495 | `	int iLo = 0,iHi = PH7_TZDB_ZONE_COUNT - 1;` |
|   2755 |  496 | `	if( zName == 0 \|\| nName < 1 ){` |
|      5 |  497 | `		return -1;` |
|      - |  498 | `	}` |
|  24367 |  499 | `	while( iLo <= iHi ){` |
|  23931 |  500 | `		int iMid = iLo + (iHi - iLo) / 2;` |
|  23931 |  501 | `		const PH7_TzZoneRow *pRow = &aTzZone[aTzFold[iMid]];` |
|  23931 |  502 | `		int n = nName < pRow->nName ? nName : (int)pRow->nName;` |
|  23931 |  503 | `		int c = SyStrnicmp(zName,pRow->zName,(sxu32)n);` |
|  23931 |  504 | `		if( c == 0 ){` |
|   2511 |  505 | `			c = nName - (int)pRow->nName;` |
|   1253 |  506 | `		}` |
|  23931 |  507 | `		if( c == 0 ){` |
|   2315 |  508 | `			return (int)aTzFold[iMid];` |
|      - |  509 | `		}` |
|  21621 |  510 | `		if( c < 0 ){` |
|   9847 |  511 | `			iHi = iMid - 1;` |
|   4926 |  512 | `		}else{` |
|  11779 |  513 | `			iLo = iMid + 1;` |
|      - |  514 | `		}` |
|      5 |  515 | `	}` |
|    440 |  516 | `	return -1;` |
|   1380 |  517 | `}` |
|      - |  518 |  |
|     82 |  519 | `PH7_PRIVATE int PH7_TzCount(void)` |
|      1 |  520 | `{` |
|     83 |  521 | `	return PH7_TZDB_ZONE_COUNT;` |
|      1 |  522 | `}` |
|      - |  523 |  |
|      - |  524 | `/*` |
|      - |  525 | ` * The zone at position i of the order listIdentifiers() prints, which is` |
|      - |  526 | ``  * case-INSENSITIVE -- `CET` sits between `Canada/Yukon` and `Chile/Continental` `` |
|      - |  527 | `` * and `localtime` among the `L` names, not after every upper-case one. Asked of`` |
|      - |  528 | ` * a bundled-timelib php rather than derived: the two lists coincide for the 419` |
|      - |  529 | `` * canonical `Continent/City` names, and only ALL_WITH_BC, which carries the`` |
|      - |  530 | ` * odd-cased backward links, can tell the two orders apart.` |
|      - |  531 | ` *` |
|      - |  532 | ` * aTzFold is that order already -- it is what PH7_TzFind() binary-searches.` |
|      - |  533 | ` */` |
|  49118 |  534 | `PH7_PRIVATE int PH7_TzAt(int i)` |
|      1 |  535 | `{` |
|  49119 |  536 | `	if( i < 0 \|\| i >= PH7_TZDB_ZONE_COUNT ){` |
|    ! 0 |  537 | `		return -1;` |
|      - |  538 | `	}` |
|  49119 |  539 | `	return (int)aTzFold[i];` |
|  24560 |  540 | `}` |
|      - |  541 |  |
|      - |  542 | `/*` |
|      - |  543 | ` * php's ABBREVIATION table, which is asked BEFORE the database and is the whole` |
|      - |  544 | `` * reason `new DateTimeZone('CET')` is a fixed +01:00 that never observes`` |
|      - |  545 | ` * daylight time while the zone FILE of that name switches twice a year. Ten` |
|      - |  546 | ` * names sit in both tables and the abbreviation wins every one.` |
|      - |  547 | ` *` |
|      - |  548 | ` * An abbreviation is a FIXED offset -- the instant never enters -- carrying a` |
|      - |  549 | `` * daylight flag that only `I` reads, and it answers the table's canonical`` |
|      - |  550 | ` * UPPER-CASE spelling rather than the caller's, which is the visible difference` |
|      - |  551 | ` * from an identifier. Answers 0 when the name is not one.` |
|      - |  552 | ` *` |
|      - |  553 | ` * The table's names are letters only, so byte order over the upper-case` |
|      - |  554 | ` * spellings is the same order case-folding produces and the search needs no` |
|      - |  555 | ` * second index.` |
|      - |  556 | ` */` |
|   2332 |  557 | `PH7_PRIVATE int PH7_TzAbbrFind(const char *zName,int nName,sxi32 *piOff,int *pbDst,` |
|      - |  558 | `	const char **pzCanon,int *pnCanon)` |
|      4 |  559 | `{` |
|   2336 |  560 | `	int iLo = 0,iHi = PH7_TZDB_ABBR_COUNT - 1;` |
|   2336 |  561 | `	if( zName == 0 \|\| nName < 1 ){` |
|      5 |  562 | `		return 0;` |
|      - |  563 | `	}` |
|  17590 |  564 | `	while( iLo <= iHi ){` |
|  16176 |  565 | `		int iMid = iLo + (iHi - iLo) / 2;` |
|  16176 |  566 | `		const PH7_TzAbbrRow *pRow = &aTzAbbr[iMid];` |
|  16176 |  567 | `		int n = nName < pRow->nName ? nName : (int)pRow->nName;` |
|  16176 |  568 | `		int c = SyStrnicmp(zName,pRow->zName,(sxu32)n);` |
|  16176 |  569 | `		if( c == 0 ){` |
|   1804 |  570 | `			c = nName - (int)pRow->nName;` |
|    900 |  571 | `		}` |
|  16176 |  572 | `		if( c == 0 ){` |
|    918 |  573 | `			*piOff = pRow->iOff;` |
|    918 |  574 | `			*pbDst = (int)pRow->bDst;` |
|    918 |  575 | `			*pzCanon = pRow->zName;` |
|    918 |  576 | `			*pnCanon = (int)pRow->nName;` |
|    918 |  577 | `			return 1;` |
|      - |  578 | `		}` |
|  15262 |  579 | `		if( c < 0 ){` |
|   7898 |  580 | `			iHi = iMid - 1;` |
|   3951 |  581 | `		}else{` |
|   7368 |  582 | `			iLo = iMid + 1;` |
|      - |  583 | `		}` |
|      4 |  584 | `	}` |
|   1418 |  585 | `	return 0;` |
|   1170 |  586 | `}` |
|      - |  587 |  |
|      - |  588 | `/*` |
|      - |  589 | `` * The OTHER half of the abbreviation table: not what `CET` resolves to, but`` |
|      - |  590 | `` * everything the database ever wrote `CET` FOR. php prints it as`` |
|      - |  591 | ` * listAbbreviations() -- 1127 (daylight, offset, zone) triples over the 144` |
|      - |  592 | ` * names -- and searches it as timezone_name_from_abbr().` |
|      - |  593 | ` *` |
|      - |  594 | ` * The triples of one name are contiguous and in php's own order, so the FIRST` |
|      - |  595 | ` * of a group is that abbreviation's fixed offset: the two tables agree on a` |
|      - |  596 | `` * bare `CET` by construction, and the rest of the group is reachable only by`` |
|      - |  597 | ` * naming an offset. 25 triples name no zone at all and print null.` |
|      - |  598 | ` *` |
|      - |  599 | ` * The listing walks aTzAbbrOrder rather than aTzAbbr, because php prints the` |
|      - |  600 | ` * flat table's order and stores it sorted.` |
|      - |  601 | ` */` |
|      4 |  602 | `PH7_PRIVATE int PH7_TzAbbrCount(void)` |
|      1 |  603 | `{` |
|      5 |  604 | `	return PH7_TZDB_ABBR_COUNT;` |
|      1 |  605 | `}` |
|    576 |  606 | `PH7_PRIVATE const char * PH7_TzAbbrAt(int i,int *pnName,int *pnRow)` |
|      1 |  607 | `{` |
|      - |  608 | `	const PH7_TzAbbrRow *pRow;` |
|    577 |  609 | `	if( i < 0 \|\| i >= PH7_TZDB_ABBR_COUNT ){` |
|    ! 0 |  610 | `		return 0;` |
|      - |  611 | `	}` |
|    577 |  612 | `	pRow = &aTzAbbr[aTzAbbrOrder[i]];` |
|    577 |  613 | `	if( pnName ){` |
|    577 |  614 | `		*pnName = (int)pRow->nName;` |
|    288 |  615 | `	}` |
|    577 |  616 | `	if( pnRow ){` |
|    577 |  617 | `		*pnRow = (int)pRow->nZone;` |
|    288 |  618 | `	}` |
|    577 |  619 | `	return pRow->zName;` |
|    289 |  620 | `}` |
|      - |  621 | `/* Triple j of listing position i. *piZone is -1 for the ones php prints null` |
|      - |  622 | ` * for, and an index into the zone table otherwise. */` |
|   4508 |  623 | `PH7_PRIVATE int PH7_TzAbbrRowAt(int i,int j,sxi32 *piOff,int *pbDst,int *piZone)` |
|      1 |  624 | `{` |
|      - |  625 | `	const PH7_TzAbbrRow *pRow;` |
|      - |  626 | `	const PH7_TzAbbrZoneRow *pZone;` |
|   4509 |  627 | `	if( i < 0 \|\| i >= PH7_TZDB_ABBR_COUNT ){` |
|    ! 0 |  628 | `		return 0;` |
|      - |  629 | `	}` |
|   4509 |  630 | `	pRow = &aTzAbbr[aTzAbbrOrder[i]];` |
|   4509 |  631 | `	if( j < 0 \|\| j >= (int)pRow->nZone ){` |
|    ! 0 |  632 | `		return 0;` |
|      - |  633 | `	}` |
|   4509 |  634 | `	pZone = &aTzAbbrZone[pRow->iZone + j];` |
|   4509 |  635 | `	*piOff = pZone->iOff;` |
|   4509 |  636 | `	*pbDst = (int)pZone->bDst;` |
|   4509 |  637 | `	*piZone = pZone->iZone == PH7_TZ_NOZONE ? -1 : (int)pZone->iZone;` |
|   4509 |  638 | `	return 1;` |
|   2255 |  639 | `}` |
|      - |  640 |  |
|      - |  641 | `/*` |
|      - |  642 | ` * timezone_name_from_abbr(): the zone one abbreviation stands for, which is` |
|      - |  643 | ` * FOUR rules in a fixed order and not the single lookup it reads like.` |
|      - |  644 | ` *` |
|      - |  645 | `` *   1. `utc` and `gmt`, either case, are answered `UTC` before any table is`` |
|      - |  646 | `` *      consulted -- and the offset argument is ignored, so `gmt` at +02:00 is`` |
|      - |  647 | `` *      still `UTC`.`` |
|      - |  648 | ` *   2. The name's own triples. An offset of -1 means the caller named NO` |
|      - |  649 | ` *      offset, so the first triple wins; php spells "unspecified" as that` |
|      - |  650 | ` *      value and cannot tell it from a real -1 second, and neither can this.` |
|      - |  651 | ` *   3. The name matched but no triple carried that offset: the first triple` |
|      - |  652 | ` *      wins anyway.` |
|      - |  653 | ` *   4. The name matched NOTHING: a second table keyed on (offset, daylight)` |
|      - |  654 | ` *      alone decides, which is the only rule the daylight argument reaches.` |
|      - |  655 | ` *` |
|      - |  656 | ` * A triple that names no zone answers false at rule 2 or 3 rather than falling` |
|      - |  657 | ` * through to rule 4 -- the search SUCCEEDED, it just has no name to give.` |
|      - |  658 | ` */` |
|     58 |  659 | `PH7_PRIVATE const char * PH7_TzAbbrZoneFind(const char *zName,int nName,sxi64 iOff,` |
|      - |  660 | `	sxi64 iDst,int *pnZone)` |
|      1 |  661 | `{` |
|     59 |  662 | `	const PH7_TzAbbrRow *pRow = 0;` |
|     59 |  663 | `	int iLo = 0,iHi = PH7_TZDB_ABBR_COUNT - 1,j;` |
|     59 |  664 | `	if( zName == 0 ){` |
|    ! 0 |  665 | `		return 0;` |
|      - |  666 | `	}` |
|     59 |  667 | `	if( nName == 3 && (SyStrnicmp(zName,"UTC",3) == 0 \|\| SyStrnicmp(zName,"GMT",3) == 0) ){` |
|     17 |  668 | `		*pnZone = 3;` |
|     17 |  669 | `		return "UTC";` |
|      - |  670 | `	}` |
|    343 |  671 | `	while( iLo <= iHi ){` |
|    319 |  672 | `		int iMid = iLo + (iHi - iLo) / 2;` |
|    319 |  673 | `		const PH7_TzAbbrRow *p = &aTzAbbr[iMid];` |
|    319 |  674 | `		int n = nName < p->nName ? nName : (int)p->nName;` |
|    319 |  675 | `		int c = nName < 1 ? -1 : SyStrnicmp(zName,p->zName,(sxu32)n);` |
|    319 |  676 | `		if( c == 0 ){` |
|     47 |  677 | `			c = nName - (int)p->nName;` |
|     23 |  678 | `		}` |
|    319 |  679 | `		if( c == 0 ){` |
|     19 |  680 | `			pRow = p;` |
|     19 |  681 | `			break;` |
|      - |  682 | `		}` |
|    301 |  683 | `		if( c < 0 ){` |
|     73 |  684 | `			iHi = iMid - 1;` |
|     37 |  685 | `		}else{` |
|    229 |  686 | `			iLo = iMid + 1;` |
|      - |  687 | `		}` |
|      1 |  688 | `	}` |
|     43 |  689 | `	if( pRow ){` |
|     19 |  690 | `		const PH7_TzAbbrZoneRow *pHit = &aTzAbbrZone[pRow->iZone];` |
|     19 |  691 | `		if( iOff != -1 ){` |
|    115 |  692 | `			for( j = 0 ; j < (int)pRow->nZone ; ++j ){` |
|    113 |  693 | `				if( (sxi64)aTzAbbrZone[pRow->iZone + j].iOff == iOff ){` |
|      7 |  694 | `					pHit = &aTzAbbrZone[pRow->iZone + j];` |
|      7 |  695 | `					break;` |
|      - |  696 | `				}` |
|     54 |  697 | `			}` |
|      4 |  698 | `		}` |
|     19 |  699 | `		if( pHit->iZone == PH7_TZ_NOZONE ){` |
|      5 |  700 | `			return 0;` |
|      - |  701 | `		}` |
|     15 |  702 | `		return PH7_TzName((int)pHit->iZone,pnZone,0);` |
|      - |  703 | `	}` |
|    699 |  704 | `	for( j = 0 ; j < PH7_TZDB_ABBR_FALLBACK_COUNT ; ++j ){` |
|    689 |  705 | `		if( (sxi64)aTzAbbrFallback[j].iOff == iOff && (sxi64)aTzAbbrFallback[j].bDst == iDst ){` |
|     15 |  706 | `			return PH7_TzName((int)aTzAbbrFallback[j].iZone,pnZone,0);` |
|      - |  707 | `		}` |
|    338 |  708 | `	}` |
|     11 |  709 | `	return 0;` |
|     30 |  710 | `}` |
|      - |  711 |  |
|  53554 |  712 | `PH7_PRIVATE const char * PH7_TzName(int iZone,int *pnName,int *pbBackward)` |
|      2 |  713 | `{` |
|  53556 |  714 | `	if( iZone < 0 \|\| iZone >= PH7_TZDB_ZONE_COUNT ){` |
|    ! 0 |  715 | `		return 0;` |
|      - |  716 | `	}` |
|  53556 |  717 | `	if( pnName ){` |
|  53556 |  718 | `		*pnName = (int)aTzZone[iZone].nName;` |
|  26777 |  719 | `	}` |
|  53556 |  720 | `	if( pbBackward ){` |
|  49119 |  721 | `		*pbBackward = (int)aTzZone[iZone].bBackward;` |
|  24559 |  722 | `	}` |
|  53556 |  723 | `	return aTzZone[iZone].zName;` |
|  26779 |  724 | `}` |
|      - |  725 |  |
|      - |  726 | `/*` |
|      - |  727 | ` * The GROUP bit listIdentifiers() sorts a zone into. It is the name's own` |
|      - |  728 | `` * continent prefix and nothing else -- so `EST5EDT`, `W-SU` and `Factory` are`` |
|      - |  729 | ` * in no group at all and appear only under ALL_WITH_BC -- with one special` |
|      - |  730 | `` * case, `UTC`, which php gives a group of its own.`` |
|      - |  731 | ` *` |
|      - |  732 | ` * Derived rather than stored: the prefix is already in the name, and a table` |
|      - |  733 | ` * would be one more thing the generator could get out of step with.` |
|      - |  734 | ` */` |
|  20950 |  735 | `PH7_PRIVATE int PH7_TzGroup(int iZone)` |
|      1 |  736 | `{` |
|      - |  737 | `	static const struct { const char *zPrefix; int nPrefix; int iBit; } aGroup[] = {` |
|      - |  738 | `		{ "Africa/",     7, 0x0001 },` |
|      - |  739 | `		{ "America/",    8, 0x0002 },` |
|      - |  740 | `		{ "Antarctica/",11, 0x0004 },` |
|      - |  741 | `		{ "Arctic/",     7, 0x0008 },` |
|      - |  742 | `		{ "Asia/",       5, 0x0010 },` |
|      - |  743 | `		{ "Atlantic/",   9, 0x0020 },` |
|      - |  744 | `		{ "Australia/", 10, 0x0040 },` |
|      - |  745 | `		{ "Europe/",     7, 0x0080 },` |
|      - |  746 | `		{ "Indian/",     7, 0x0100 },` |
|      - |  747 | `		{ "Pacific/",    8, 0x0200 }` |
|      - |  748 | `	};` |
|      - |  749 | `	sxu32 i;` |
|      - |  750 | `	const PH7_TzZoneRow *pRow;` |
|  20951 |  751 | `	if( iZone < 0 \|\| iZone >= PH7_TZDB_ZONE_COUNT ){` |
|    ! 0 |  752 | `		return 0;` |
|      - |  753 | `	}` |
|  20951 |  754 | `	pRow = &aTzZone[iZone];` |
|  20951 |  755 | `	if( pRow->nName == 3 && SyMemcmp(pRow->zName,"UTC",3) == 0 ){` |
|     51 |  756 | `		return 0x0400;` |
|      - |  757 | `	}` |
|  93351 |  758 | `	for( i = 0 ; i < SX_ARRAYSIZE(aGroup) ; ++i ){` |
|  93350 |  759 | `		if( (int)pRow->nName > aGroup[i].nPrefix` |
|  92401 |  760 | `		 && SyMemcmp(pRow->zName,aGroup[i].zPrefix,(sxu32)aGroup[i].nPrefix) == 0 ){` |
|  20901 |  761 | `			return aGroup[i].iBit;` |
|      - |  762 | `		}` |
|  36226 |  763 | `	}` |
|    ! 0 |  764 | `	return 0;` |
|  10476 |  765 | `}` |
|      - |  766 |  |
|      - |  767 | ``/* Its ISO 3166-1 country, two bytes, `??` when it has none. */`` |
|  19192 |  768 | `PH7_PRIVATE const char * PH7_TzCountry(int iZone)` |
|      2 |  769 | `{` |
|  19194 |  770 | `	if( iZone < 0 \|\| iZone >= PH7_TZDB_ZONE_COUNT ){` |
|    ! 0 |  771 | `		return "??";` |
|      - |  772 | `	}` |
|  19194 |  773 | `	return aTzZone[iZone].zCc;` |
|   9598 |  774 | `}` |
|      - |  775 |  |
|      - |  776 | `/*` |
|      - |  777 | `` * The rest of getLocation(): the point tzdata's `zone.tab` puts the zone at and`` |
|      - |  778 | ` * the note beside it. A zone the tab does not list reads 0/0 with the comment` |
|      - |  779 | `` * `?` -- a literal question mark, not the empty string a LISTED zone with no`` |
|      - |  780 | ` * note carries, and the two are distinguishable from PHP.` |
|      - |  781 | ` */` |
|     24 |  782 | `PH7_PRIVATE void PH7_TzLocation(int iZone,double *prLat,double *prLong,` |
|      - |  783 | `	const char **pzComment,int *pnComment)` |
|      1 |  784 | `{` |
|      - |  785 | `	const PH7_TzZoneRow *pRow;` |
|     25 |  786 | `	if( iZone < 0 \|\| iZone >= PH7_TZDB_ZONE_COUNT ){` |
|    ! 0 |  787 | `		*prLat = 0.0;` |
|    ! 0 |  788 | `		*prLong = 0.0;` |
|    ! 0 |  789 | `		*pzComment = "?";` |
|    ! 0 |  790 | `		*pnComment = 1;` |
|    ! 0 |  791 | `		return;` |
|      - |  792 | `	}` |
|     25 |  793 | `	pRow = &aTzZone[iZone];` |
|     25 |  794 | `	*prLat = pRow->rLat;` |
|     25 |  795 | `	*prLong = pRow->rLong;` |
|     25 |  796 | `	*pzComment = pRow->zComment;` |
|     25 |  797 | `	*pnComment = (int)pRow->nComment;` |
|     13 |  798 | `}` |
|      - |  799 |  |
|      - |  800 | `/*` |
|      - |  801 | `` * What `timezone_version_get()` answers -- timelib's spelling of the IANA`` |
|      - |  802 | ` * release this table was cut from, not the release string itself.` |
|      - |  803 | ` */` |
|      2 |  804 | `PH7_PRIVATE const char * PH7_TzVersion(void)` |
|      1 |  805 | `{` |
|      3 |  806 | `	return PH7_TZDB_PHP_VERSION;` |
|      1 |  807 | `}` |
|      - |  808 |  |
|      - |  809 | `/*` |
|      - |  810 | ` * The offset, the is-DST flag and the abbreviation zone iZone is on at iTs.` |
|      - |  811 | ` * The abbreviation points either into the payload or into the footer text, both` |
|      - |  812 | ` * of which are static, so a caller may hold it.` |
|      - |  813 | ` */` |
|   1944 |  814 | `PH7_PRIVATE int PH7_TzOffsetAt(int iZone,sxi64 iTs,sxi32 *piOff,int *pbDst,` |
|      - |  815 | `	const char **pzAbbr,int *pnAbbr)` |
|      4 |  816 | `{` |
|      - |  817 | `	tz_block sB;` |
|      - |  818 | `	sxu32 iType;` |
|   1948 |  819 | `	sxi32 iOff = 0;` |
|   1948 |  820 | `	int bDst = 0,nAbbr = 0;` |
|   1948 |  821 | `	const char *zAbbr = "";` |
|   1948 |  822 | `	if( !TzBlock(iZone,&sB) ){` |
|    ! 0 |  823 | `		return 0;` |
|      - |  824 | `	}` |
|   1948 |  825 | `	if( sB.nTime == 0 \|\| iTs < TzI64(sB.aTime) ){` |
|     87 |  826 | `		iType = TzFirstType(&sB);` |
|   1906 |  827 | `	}else if( iTs >= TzI64(sB.aTime + (sB.nTime - 1) * 8) && sB.zPosix ){` |
|      - |  828 | `		tz_posix sP;` |
|    209 |  829 | `		if( TzPosixParse(sB.zPosix,sB.nPosix,&sP) ){` |
|    209 |  830 | `			TzPosixAt(&sP,iTs,piOff,pbDst,pzAbbr,pnAbbr);` |
|    209 |  831 | `			return 1;` |
|      - |  832 | `		}` |
|    ! 0 |  833 | `		iType = sB.aIdx[sB.nTime - 1];` |
|    ! 0 |  834 | `	}else{` |
|      - |  835 | `		/* The last transition at or before iTs. */` |
|   1658 |  836 | `		sxu32 iLo = 0,iHi = sB.nTime - 1;` |
|  14310 |  837 | `		while( iLo < iHi ){` |
|  12656 |  838 | `			sxu32 iMid = iLo + (iHi - iLo + 1) / 2;` |
|  12656 |  839 | `			if( TzI64(sB.aTime + iMid * 8) <= iTs ){` |
|   7278 |  840 | `				iLo = iMid;` |
|   3641 |  841 | `			}else{` |
|   5382 |  842 | `				iHi = iMid - 1;` |
|      - |  843 | `			}` |
|      4 |  844 | `		}` |
|   1658 |  845 | `		iType = sB.aIdx[iLo];` |
|      - |  846 | `	}` |
|   1742 |  847 | `	if( iType >= sB.nType ){` |
|    ! 0 |  848 | `		iType = 0;` |
|    ! 0 |  849 | `	}` |
|   1742 |  850 | `	iOff = TzI32(sB.aType + iType * 6);` |
|   1742 |  851 | `	bDst = sB.aType[iType * 6 + 4] != 0;` |
|   1742 |  852 | `	TzAbbr(&sB,iType,&zAbbr,&nAbbr);` |
|   1742 |  853 | `	*piOff = iOff;` |
|   1742 |  854 | `	*pbDst = bDst;` |
|   1742 |  855 | `	*pzAbbr = zAbbr;` |
|   1742 |  856 | `	*pnAbbr = nAbbr;` |
|   1742 |  857 | `	return 1;` |
|    976 |  858 | `}` |
|      - |  859 |  |
|      - |  860 | `/* --- Transitions, one row at a time --------------------------------------- *` |
|      - |  861 | ` *` |
|      - |  862 | ` * getTransitions() wants the SWITCHES themselves rather than the offset at an` |
|      - |  863 | ` * instant, and its range may run past the last one a file carries, so it reads` |
|      - |  864 | ` * through two doors: the transition table for the explicit rows, and the` |
|      - |  865 | ` * footer rule for everything after them.` |
|      - |  866 | ` */` |
|      - |  867 |  |
|      - |  868 | `/* How many explicit transitions zone iZone's block carries. */` |
|     28 |  869 | `PH7_PRIVATE int PH7_TzTransCount(int iZone)` |
|      1 |  870 | `{` |
|      - |  871 | `	tz_block sB;` |
|     29 |  872 | `	if( !TzBlock(iZone,&sB) ){` |
|    ! 0 |  873 | `		return 0;` |
|      - |  874 | `	}` |
|     29 |  875 | `	return (int)sB.nTime;` |
|     15 |  876 | `}` |
|      - |  877 |  |
|      - |  878 | `/* Row i of that table, 0-based, in the file's own order. */` |
|   4620 |  879 | `PH7_PRIVATE int PH7_TzTransAt(int iZone,int i,sxi64 *piTs,sxi32 *piOff,int *pbDst,` |
|      - |  880 | `	const char **pzAbbr,int *pnAbbr)` |
|      1 |  881 | `{` |
|      - |  882 | `	tz_block sB;` |
|      - |  883 | `	sxu32 iType;` |
|   4621 |  884 | `	if( !TzBlock(iZone,&sB) \|\| i < 0 \|\| (sxu32)i >= sB.nTime ){` |
|    ! 0 |  885 | `		return 0;` |
|      - |  886 | `	}` |
|   4621 |  887 | `	iType = sB.aIdx[i];` |
|   4621 |  888 | `	if( iType >= sB.nType ){` |
|    ! 0 |  889 | `		iType = 0;` |
|    ! 0 |  890 | `	}` |
|   4621 |  891 | `	*piTs  = TzI64(sB.aTime + (sxu32)i * 8);` |
|   4621 |  892 | `	*piOff = TzI32(sB.aType + iType * 6);` |
|   4621 |  893 | `	*pbDst = sB.aType[iType * 6 + 4] != 0;` |
|   4621 |  894 | `	TzAbbr(&sB,iType,pzAbbr,pnAbbr);` |
|   4621 |  895 | `	return 1;` |
|   2311 |  896 | `}` |
|      - |  897 |  |
|      - |  898 | `/*` |
|      - |  899 | ` * The first switch the FOOTER names strictly after iTs. The files stop in 2037,` |
|      - |  900 | ` * so a range running past them is answered from the rule instead: each year` |
|      - |  901 | ` * names two instants, and a caller walks this door until one lands beyond the` |
|      - |  902 | ` * end of what it was asked for.` |
|      - |  903 | ` *` |
|      - |  904 | `` * A zone whose footer carries no DST part -- `Asia/Tokyo` -- never switches`` |
|      - |  905 | ` * again and says so with 0, which is also what a malformed or absent footer` |
|      - |  906 | ` * answers. Both are the caller's signal to stop.` |
|      - |  907 | ` */` |
|     38 |  908 | `PH7_PRIVATE int PH7_TzTransNextPosix(int iZone,sxi64 iTs,sxi64 *piTs,sxi32 *piOff,` |
|      - |  909 | `	int *pbDst,const char **pzAbbr,int *pnAbbr)` |
|      1 |  910 | `{` |
|      - |  911 | `	tz_block sB;` |
|      - |  912 | `	tz_posix sP;` |
|     39 |  913 | `	sxi64 y,iBest = 0;` |
|     39 |  914 | `	int m,d,i,bFound = 0;` |
|     38 |  915 | `	if( !TzBlock(iZone,&sB) \|\| sB.zPosix == 0` |
|     39 |  916 | `	 \|\| !TzPosixParse(sB.zPosix,sB.nPosix,&sP) \|\| !sP.bHasDst ){` |
|      5 |  917 | `		return 0;` |
|      - |  918 | `	}` |
|      - |  919 | `	/* Bounded before any arithmetic: the year is multiplied back up to seconds` |
|      - |  920 | `	 * below, and a caller walking towards PHP_INT_MAX would otherwise overflow` |
|      - |  921 | `	 * every term. Refusing is the same "no more switches" the caller already` |
|      - |  922 | `	 * handles. */` |
|     35 |  923 | `	if( iTs > (sxi64)0x0FFFFFFFFFFFFFFF \|\| iTs < -(sxi64)0x0FFFFFFFFFFFFFFF ){` |
|    ! 0 |  924 | `		return 0;` |
|      - |  925 | `	}` |
|     35 |  926 | `	DtCivilFromDays(DtFloorDiv(iTs + sP.iStd,86400),&y,&m,&d);` |
|     35 |  927 | `	if( y < -9999999 \|\| y > 9999999 ){` |
|    ! 0 |  928 | `		return 0;` |
|      - |  929 | `	}` |
|      - |  930 | `	/* Both instants are LOCAL readings converted with the offset in force just` |
|      - |  931 | `	 * before each -- the pairing TzPosixAt() makes -- and the year iTs falls in` |
|      - |  932 | `	 * can name one that is already behind it, so this takes the smallest of the` |
|      - |  933 | `	 * two that is genuinely ahead and rolls into the next year when neither is.` |
|      - |  934 | `	 * A southern-hemisphere rule, whose two are in the other order, needs no` |
|      - |  935 | `	 * special case under a plain minimum. */` |
|     93 |  936 | `	for( i = 0 ; i < 3 && !bFound ; ++i ){` |
|      - |  937 | `		sxi64 aCand[2];` |
|      - |  938 | `		int j;` |
|    117 |  939 | `		aCand[0] = TzRuleInstant(sP.eStartKind,sP.iStartM,sP.iStartW,sP.iStartD,` |
|     87 |  940 | `			sP.iStartSec,y + i) - sP.iStd;` |
|    117 |  941 | `		aCand[1] = TzRuleInstant(sP.eEndKind,sP.iEndM,sP.iEndW,sP.iEndD,` |
|     87 |  942 | `			sP.iEndSec,y + i) - sP.iDst;` |
|    175 |  943 | `		for( j = 0 ; j < 2 ; ++j ){` |
|    117 |  944 | `			if( aCand[j] > iTs && (!bFound \|\| aCand[j] < iBest) ){` |
|     39 |  945 | `				iBest = aCand[j];` |
|     39 |  946 | `				bFound = 1;` |
|     19 |  947 | `			}` |
|     59 |  948 | `		}` |
|     30 |  949 | `	}` |
|     35 |  950 | `	if( !bFound ){` |
|    ! 0 |  951 | `		return 0;` |
|      - |  952 | `	}` |
|     35 |  953 | `	TzPosixAt(&sP,iBest,piOff,pbDst,pzAbbr,pnAbbr);` |
|     35 |  954 | `	*piTs = iBest;` |
|     35 |  955 | `	return 1;` |
|     20 |  956 | `}` |
|      - |  957 |  |
|      - |  958 | `/*` |
|      - |  959 | ` * The instant a WALL CLOCK reading names. iLocal is the reading expressed as if` |
|      - |  960 | ` * it were UT, which is the shape every date parser here already produces.` |
|      - |  961 | ` *` |
|      - |  962 | ` * Two guesses settle it for every unambiguous reading: read the offset at the` |
|      - |  963 | ` * reading itself, subtract it, then read the offset at THAT instant and use it.` |
|      - |  964 | ` * The two disagree only across a switch, and there php's rule is:` |
|      - |  965 | ` *` |
|      - |  966 | ` *   an hour DST SKIPPED   -- no instant reads that way -- takes the offset in` |
|      - |  967 | ` *                            force BEFORE the gap, which pushes the answer` |
|      - |  968 | ` *                            forward into DST (02:30 on a spring-forward day` |
|      - |  969 | ` *                            is 03:30 local).` |
|      - |  970 | ` *   an hour DST REPEATED  -- two instants read that way -- takes the FIRST,` |
|      - |  971 | ` *                            the one still on the pre-switch offset.` |
|      - |  972 | ` *` |
|      - |  973 | ` * Both fall out of preferring the offset that was in force before: the fixed` |
|      - |  974 | ` * point is searched from the earlier side.` |
|      - |  975 | ` */` |
|    348 |  976 | `PH7_PRIVATE int PH7_TzLocalToUtc(int iZone,sxi64 iLocal,sxi64 *piTs,sxi32 *piOff)` |
|      3 |  977 | `{` |
|    351 |  978 | `	sxi32 iOff1 = 0,iOff2 = 0,iOff3 = 0;` |
|      - |  979 | `	int bDst;` |
|      - |  980 | `	const char *zAbbr;` |
|      - |  981 | `	int nAbbr;` |
|    351 |  982 | `	if( !PH7_TzOffsetAt(iZone,iLocal,&iOff1,&bDst,&zAbbr,&nAbbr) ){` |
|    ! 0 |  983 | `		return 0;` |
|      - |  984 | `	}` |
|    351 |  985 | `	if( !PH7_TzOffsetAt(iZone,iLocal - iOff1,&iOff2,&bDst,&zAbbr,&nAbbr) ){` |
|    ! 0 |  986 | `		return 0;` |
|      - |  987 | `	}` |
|    351 |  988 | `	if( iOff2 == iOff1 ){` |
|    334 |  989 | `		*piTs = iLocal - iOff1;` |
|    334 |  990 | `		*piOff = iOff1;` |
|    334 |  991 | `		return 1;` |
|      - |  992 | `	}` |
|      - |  993 | `	/* The two disagree, so the reading sits at or near a switch. Try the second` |
|      - |  994 | `	 * guess's own instant: when it is stable, that is the answer. An hour DST` |
|      - |  995 | `	 * REPEATED lands here and takes the FIRST of its two instants, because the` |
|      - |  996 | `	 * first guess already read the pre-switch offset. */` |
|     18 |  997 | `	if( !PH7_TzOffsetAt(iZone,iLocal - iOff2,&iOff3,&bDst,&zAbbr,&nAbbr) ){` |
|    ! 0 |  998 | `		return 0;` |
|      - |  999 | `	}` |
|     18 | 1000 | `	if( iOff3 == iOff2 ){` |
|      7 | 1001 | `		*piTs = iLocal - iOff2;` |
|      7 | 1002 | `		*piOff = iOff2;` |
|      7 | 1003 | `		return 1;` |
|      - | 1004 | `	}` |
|      - | 1005 | `	/* Neither is a fixed point: the reading names no instant at all, because` |
|      - | 1006 | `	 * DST skipped that hour. php reads it with the offset in force BEFORE the` |
|      - | 1007 | `	 * gap, which lands the answer just past the switch -- 02:30 on a` |
|      - | 1008 | `	 * spring-forward morning in New York is 03:30 EDT.` |
|      - | 1009 | `	 *` |
|      - | 1010 | `	 * "Before the gap" is the SMALLER of the two offsets, not the first guess:` |
|      - | 1011 | `	 * a gap is always a forward jump, so the offset ahead of it is the lower` |
|      - | 1012 | `	 * one, and which of the two guesses found it depends on the sign of the` |
|      - | 1013 | `	 * zone's offset. Reading it off guess one is right in New York and wrong in` |
|      - | 1014 | `	 * Paris, where the local-as-if-UTC instant lands on the far side of the` |
|      - | 1015 | `	 * switch instead of the near one.` |
|      - | 1016 | `	 *` |
|      - | 1017 | `	 * The offset REPORTED is the one at the instant it landed on, not the one it` |
|      - | 1018 | `	 * was read with, which is why every arm here re-reads instead of answering` |
|      - | 1019 | `	 * its guess. */` |
|     12 | 1020 | `	*piTs = iLocal - (iOff1 < iOff2 ? iOff1 : iOff2);` |
|     12 | 1021 | `	PH7_TzOffsetAt(iZone,*piTs,piOff,&bDst,&zAbbr,&nAbbr);` |
|     12 | 1022 | `	return 1;` |
|    177 | 1023 | `}` |
|      - | 1024 |  |
|      - | 1025 | `/*` |
|      - | 1026 | ` * mktime()'s reading of the same thing, and it is NOT the same answer.` |
|      - | 1027 | ` *` |
|      - | 1028 | ` * An hour daylight saving repeated names two instants, and PH7_TzLocalToUtc()` |
|      - | 1029 | ` * settles it the way php's parser does. php's mktime() settles it differently:` |
|      - | 1030 | ` * it seeds its fields from the CURRENT moment -- offset and is-DST flag and all` |
|      - | 1031 | ` * -- and that seed survives into the answer. So mktime() on an ambiguous hour` |
|      - | 1032 | ` * picks by what the zone is doing TODAY, which makes its answer depend on the` |
|      - | 1033 | ` * date the script RUNS. That is php's, not this engine's invention, and it is` |
|      - | 1034 | ` * the reason no .phpt can pin it; it is swept against the oracle instead.` |
|      - | 1035 | ` *` |
|      - | 1036 | ` * The seed is read in TWO steps, and both are needed -- 170030 readings taken` |
|      - | 1037 | ` * from around every transition since 2000 say so:` |
|      - | 1038 | ` *` |
|      - | 1039 | `` *   The DAYLIGHT FLAG first. `America/Dawson` keeps standard time all year now,`` |
|      - | 1040 | ` *   so mktime(1,30,0,11,7,2010) there is 1289122200 -- the standard-time` |
|      - | 1041 | ` *   candidate -- where strtotime() of the same reading is 1289118600. London,` |
|      - | 1042 | ` *   on daylight time today, takes the other one, and would swap in December.` |
|      - | 1043 | ` *` |
|      - | 1044 | ` *   The OFFSET when the flags TIE, which is the case for a switch that changes` |
|      - | 1045 | ` *   the offset without changing the flag. Two zones in the whole database do` |
|      - | 1046 | `` *   it: `America/Argentina/San_Luis` went -02:00 to -03:00 in January 2008 with`` |
|      - | 1047 | `` *   both sides marked daylight, and `Asia/Famagusta` went +03:00 to +02:00 in`` |
|      - | 1048 | ` *   October 2017 with neither. The daylight flag cannot tell those apart and` |
|      - | 1049 | ` *   the seed offset can.` |
|      - | 1050 | ` *` |
|      - | 1051 | ` * A reading no switch touches has one candidate and ignores all of this; an` |
|      - | 1052 | ` * hour a switch SKIPPED has none, and falls back to PH7_TzLocalToUtc(), which` |
|      - | 1053 | ` * is why mktime() and the parser agree on every gap.` |
|      - | 1054 | ` */` |
|      - | 1055 | `/*` |
|      - | 1056 | ` * The same reading, resolved php's OTHER way -- the FIRST of the two instants a` |
|      - | 1057 | ` * repeated hour names, which is what a zone NAMED INSIDE A DATE STRING gets.` |
|      - | 1058 | ` *` |
|      - | 1059 | ` * php has two answers for one ambiguous reading and they are not the same. A` |
|      - | 1060 | ` * zone handed to the constructor as an argument, or standing as the default,` |
|      - | 1061 | `` * takes the SECOND instant (`new DateTime('2026-10-25 02:30:00', new`` |
|      - | 1062 | `` * DateTimeZone('Europe/Paris'))` is CET); the identifier the STRING spells takes`` |
|      - | 1063 | `` * the first (`new DateTime('2026-10-25 02:30:00 Europe/Paris')` is CEST). Swept`` |
|      - | 1064 | ` * over 30 fall-back switches in eleven zones, the string door was the first` |
|      - | 1065 | ` * instant every time.` |
|      - | 1066 | ` *` |
|      - | 1067 | ` * "First" is the LARGER of the two offsets, not the earlier of the two guesses:` |
|      - | 1068 | ` * a fall-back is a backward jump, so the offset ahead of it is the higher one.` |
|      - | 1069 | ` * Reading it off the first guess is right in New York and wrong in Paris -- the` |
|      - | 1070 | ` * local-as-if-UTC instant lands on the far side of the switch for a zone east of` |
|      - | 1071 | ` * Greenwich and on the near side for one west of it -- which is the same` |
|      - | 1072 | ` * asymmetry PH7_TzLocalToUtc's gap arm has to spell out.` |
|      - | 1073 | ` *` |
|      - | 1074 | ` * Candidates come from probing a day either side, as the seeded door does: a` |
|      - | 1075 | ` * switch is at most a few hours wide, so that reaches every offset a reading can` |
|      - | 1076 | ` * be read with. An offset counts only if it is a FIXED POINT -- the instant it` |
|      - | 1077 | ` * produces must itself be on that offset -- so a SKIPPED hour has none at all,` |
|      - | 1078 | ` * and falls through to the gap rule.` |
|      - | 1079 | ` */` |
|     42 | 1080 | `PH7_PRIVATE int PH7_TzLocalToUtcFirst(int iZone,sxi64 iLocal,sxi64 *piTs,sxi32 *piOff)` |
|      1 | 1081 | `{` |
|      - | 1082 | `	static const sxi64 aProbe[3] = { 0, -90000, 90000 };` |
|      - | 1083 | `	sxi32 aOff[3];` |
|     43 | 1084 | `	int nOff = 0,i,j,bDst,nAbbr,bAny = 0;` |
|      - | 1085 | `	const char *zAbbr;` |
|     43 | 1086 | `	sxi32 iSeed = 0,iBestOff = 0;` |
|     43 | 1087 | `	sxi64 iBest = 0;` |
|     43 | 1088 | `	if( !PH7_TzOffsetAt(iZone,iLocal,&iSeed,&bDst,&zAbbr,&nAbbr) ){` |
|    ! 0 | 1089 | `		return 0;` |
|      - | 1090 | `	}` |
|    169 | 1091 | `	for( i = 0 ; i < 3 ; ++i ){` |
|    127 | 1092 | `		sxi32 iOff = iSeed;` |
|    127 | 1093 | `		if( !PH7_TzOffsetAt(iZone,iLocal - iSeed + aProbe[i],&iOff,&bDst,&zAbbr,&nAbbr) ){` |
|    ! 0 | 1094 | `			continue;` |
|      - | 1095 | `		}` |
|    141 | 1096 | `		for( j = 0 ; j < nOff ; ++j ){` |
|     85 | 1097 | `			if( aOff[j] == iOff ){` |
|     71 | 1098 | `				break;` |
|      - | 1099 | `			}` |
|      8 | 1100 | `		}` |
|    127 | 1101 | `		if( j == nOff ){` |
|     57 | 1102 | `			aOff[nOff++] = iOff;` |
|     28 | 1103 | `		}` |
|     64 | 1104 | `	}` |
|     99 | 1105 | `	for( i = 0 ; i < nOff ; ++i ){` |
|     57 | 1106 | `		sxi64 iTs = iLocal - aOff[i];` |
|     57 | 1107 | `		sxi32 iAt = aOff[i];` |
|     57 | 1108 | `		if( !PH7_TzOffsetAt(iZone,iTs,&iAt,&bDst,&zAbbr,&nAbbr) \|\| iAt != aOff[i] ){` |
|      9 | 1109 | `			continue;` |
|      - | 1110 | `		}` |
|     49 | 1111 | `		if( !bAny \|\| iAt > iBestOff ){` |
|     47 | 1112 | `			iBest = iTs;` |
|     47 | 1113 | `			iBestOff = iAt;` |
|     47 | 1114 | `			bAny = 1;` |
|     23 | 1115 | `		}` |
|     25 | 1116 | `	}` |
|     43 | 1117 | `	if( bAny ){` |
|     39 | 1118 | `		*piTs = iBest;` |
|     39 | 1119 | `		*piOff = iBestOff;` |
|     39 | 1120 | `		return 1;` |
|      - | 1121 | `	}` |
|      5 | 1122 | `	return PH7_TzLocalToUtc(iZone,iLocal,piTs,piOff);` |
|     22 | 1123 | `}` |
|     16 | 1124 | `PH7_PRIVATE int PH7_TzLocalToUtcSeed(int iZone,sxi64 iLocal,sxi32 iOffNow,int bDstNow,` |
|      - | 1125 | `	sxi64 *piTs,sxi32 *piOff)` |
|      1 | 1126 | `{` |
|      - | 1127 | `	/* The offsets in force around the reading. A switch is at most a few hours` |
|      - | 1128 | `	 * wide, so an offset a day either side of the first guess is the whole set` |
|      - | 1129 | `	 * a candidate can be drawn from. */` |
|      - | 1130 | `	static const sxi64 aProbe[3] = { 0, -90000, 90000 };` |
|      - | 1131 | `	sxi32 aOff[3];` |
|     17 | 1132 | `	int nOff = 0,i,j;` |
|     17 | 1133 | `	sxi64 iBest = 0;` |
|     17 | 1134 | `	sxi32 iBestOff = 0;` |
|     17 | 1135 | `	int iBestRank = -1;` |
|     17 | 1136 | `	sxi32 iSeed = 0;` |
|      - | 1137 | `	int bDst,nAbbr;` |
|      - | 1138 | `	const char *zAbbr;` |
|     17 | 1139 | `	if( !PH7_TzOffsetAt(iZone,iLocal,&iSeed,&bDst,&zAbbr,&nAbbr) ){` |
|    ! 0 | 1140 | `		return 0;` |
|      - | 1141 | `	}` |
|     65 | 1142 | `	for( i = 0 ; i < 3 ; ++i ){` |
|     49 | 1143 | `		sxi32 iOff = iSeed;` |
|     49 | 1144 | `		if( !PH7_TzOffsetAt(iZone,iLocal - iSeed + aProbe[i],&iOff,&bDst,&zAbbr,&nAbbr) ){` |
|    ! 0 | 1145 | `			continue;` |
|      - | 1146 | `		}` |
|     49 | 1147 | `		for( j = 0 ; j < nOff ; ++j ){` |
|     33 | 1148 | `			if( aOff[j] == iOff ){` |
|     33 | 1149 | `				break;` |
|      - | 1150 | `			}` |
|    ! 0 | 1151 | `		}` |
|     49 | 1152 | `		if( j == nOff ){` |
|     17 | 1153 | `			aOff[nOff++] = iOff;` |
|      8 | 1154 | `		}` |
|     25 | 1155 | `	}` |
|      - | 1156 | `	/* A candidate is an offset the reading actually READS as: the instant it` |
|      - | 1157 | `	 * produces must be on that same offset. Ambiguity is two of them; the seed` |
|      - | 1158 | `	 * ranks them, 2 for a daylight-flag match and 1 for an offset match. */` |
|     33 | 1159 | `	for( i = 0 ; i < nOff ; ++i ){` |
|     17 | 1160 | `		sxi64 iTs = iLocal - aOff[i];` |
|     17 | 1161 | `		sxi32 iAt = aOff[i];` |
|      - | 1162 | `		int iRank;` |
|     17 | 1163 | `		if( !PH7_TzOffsetAt(iZone,iTs,&iAt,&bDst,&zAbbr,&nAbbr) \|\| iAt != aOff[i] ){` |
|    ! 0 | 1164 | `			continue;` |
|      - | 1165 | `		}` |
|     17 | 1166 | `		iRank = (bDst != 0) == (bDstNow != 0) ? 2 : (iAt == iOffNow ? 1 : 0);` |
|     17 | 1167 | `		if( iRank > iBestRank ){` |
|     17 | 1168 | `			iBest = iTs;` |
|     17 | 1169 | `			iBestOff = iAt;` |
|     17 | 1170 | `			iBestRank = iRank;` |
|      8 | 1171 | `		}` |
|      9 | 1172 | `	}` |
|     17 | 1173 | `	if( iBestRank >= 0 ){` |
|     17 | 1174 | `		*piTs = iBest;` |
|     17 | 1175 | `		*piOff = iBestOff;` |
|     17 | 1176 | `		return 1;` |
|      - | 1177 | `	}` |
|    ! 0 | 1178 | `	return PH7_TzLocalToUtc(iZone,iLocal,piTs,piOff);` |
|      9 | 1179 | `}` |
|      - | 1180 |  |
|      - | 1181 | `#endif /* PH7_ENABLE_TZDB && !PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1182 |  |
