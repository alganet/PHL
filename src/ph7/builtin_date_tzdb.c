/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The IANA timezone database: a TZif reader over the embedded payload.
 *
 * Guarded by PH7_ENABLE_TZDB, which `full` and `coverage` set and `tiny` does
 * not -- the payload is ~296 KB, which is most of a tiny build. With the flag
 * off every door below is absent and the date family keeps the fixed-offset
 * answers it has always had: UTC, GMT, Z, php's military letters and the whole
 * ±HH:MM:SS grammar. Nothing here is reachable from a `#ifdef`-free caller;
 * builtin_date_parse.c asks PH7_TzFind() first and falls through when it is
 * not compiled in.
 *
 * WHAT A TZif FILE SAYS (RFC 8536). After a 44-byte header the version-2 block
 * carries, in order: `timecnt` transition instants as big-endian signed 64-bit
 * seconds; `timecnt` one-byte indices into the type table; `typecnt` six-byte
 * ttinfo records (a 32-bit UT offset, an is-DST byte, an index into the
 * abbreviation blob); `charcnt` bytes of NUL-separated abbreviations; the leap
 * table; the standard/wall and UT/local indicator arrays; and finally a
 * newline-wrapped POSIX TZ string.
 *
 * THREE RANGES, and each is a different rule -- this is the whole reader:
 *
 *   BEFORE the first transition   the first ttinfo whose is-DST byte is clear,
 *                                 or ttinfo 0 when every one of them is set.
 *                                 That is where a zone's LMT lives, which is
 *                                 why `America/New_York` in 1800 is -04:56.
 *   BETWEEN two transitions       the type the transition named. A binary
 *                                 search, because a zone carries up to 366 of
 *                                 them and every date formats through here.
 *   AFTER the last transition     the FOOTER's rule, evaluated for that year.
 *                                 The files stop at 2037 -- New York's last
 *                                 explicit transition is 2037-11-01 -- and
 *                                 everything past it is `EST5EDT,M3.2.0,M11.1.0`
 *                                 read as a rule. A build that skipped this
 *                                 would answer standard time for every date
 *                                 after 2037 and look right in every test that
 *                                 does not cross one.
 *
 * The two directions are NOT symmetric. Reading an offset off an INSTANT is a
 * lookup; turning a WALL CLOCK into an instant is a search, because an hour
 * that DST skips names no instant and an hour it repeats names two. php's rule
 * for both is in PH7_TzLocalToUtc().
 */
#include "ph7int.h"

#if defined(PH7_ENABLE_TZDB) && !defined(PH7_DISABLE_BUILTIN_FUNC)

#include "builtin_date_tzdb.h"

/* --- Reading the payload ------------------------------------------------- */

/* The header is fixed-width big-endian: magic(4) version(1) reserved(15) and
 * then six 32-bit counts. Everything else is measured off those six. */
#define TZ_HDR_SIZE 44

static sxu32 TzU32(const unsigned char *z)
{
	return ((sxu32)z[0] << 24) | ((sxu32)z[1] << 16) | ((sxu32)z[2] << 8) | (sxu32)z[3];
}
static sxi32 TzI32(const unsigned char *z)
{
	/* Spelled through the unsigned width and then narrowed, so the sign comes
	 * from the cast rather than from a shift of a negative value. */
	return (sxi32)TzU32(z);
}
static sxi64 TzI64(const unsigned char *z)
{
	sxu64 v = 0;
	int i;
	for( i = 0 ; i < 8 ; ++i ){
		v = (v << 8) | (sxu64)z[i];
	}
	return (sxi64)v;
}

/* One zone's block, laid out. Every pointer is into aTzPayload and outlives any
 * caller: the table is static const. */
typedef struct tz_block tz_block;
struct tz_block
{
	const unsigned char *aTime;   /* timecnt × 8, big-endian seconds */
	const unsigned char *aIdx;    /* timecnt × 1, indices into aType */
	const unsigned char *aType;   /* typecnt × 6 */
	const char *zChar;            /* charcnt bytes of NUL-separated names */
	sxu32 nTime;
	sxu32 nType;
	sxu32 nChar;
	const char *zPosix;           /* the footer rule, or 0 */
	int nPosix;
};

/*
 * Lay out zone iZone. Answers 0 when the block is malformed -- a payload this
 * engine generated cannot be, but the reader is written so that a truncated or
 * hand-edited one refuses rather than walks off the end.
 */
static int TzBlock(int iZone,tz_block *pOut)
{
	const unsigned char *z,*zEnd;
	sxu32 nIsUt,nIsStd,nLeap,nNeed;
	if( iZone < 0 || iZone >= PH7_TZDB_ZONE_COUNT ){
		return 0;
	}
	z = &aTzPayload[aTzZone[iZone].iOfst];
	zEnd = z + aTzZone[iZone].nByte;
	if( aTzZone[iZone].nByte < TZ_HDR_SIZE ){
		return 0;
	}
	nIsUt  = TzU32(z + 20);
	nIsStd = TzU32(z + 24);
	nLeap  = TzU32(z + 28);
	pOut->nTime = TzU32(z + 32);
	pOut->nType = TzU32(z + 36);
	pOut->nChar = TzU32(z + 40);
	if( pOut->nType == 0 ){
		return 0;
	}
	/* Sized before anything is read: the counts are attacker-shaped data in the
	 * general case, and 9 × timecnt can overflow 32 bits on its own. */
	if( pOut->nTime > 0x00FFFFFF || pOut->nType > 0x00FFFF || pOut->nChar > 0x00FFFF
	 || nLeap > 0x00FFFF || nIsStd > 0x00FFFF || nIsUt > 0x00FFFF ){
		return 0;
	}
	nNeed = TZ_HDR_SIZE + pOut->nTime * 9 + pOut->nType * 6 + pOut->nChar
	      + nLeap * 12 + nIsStd + nIsUt;
	if( nNeed > aTzZone[iZone].nByte ){
		return 0;
	}
	pOut->aTime = z + TZ_HDR_SIZE;
	pOut->aIdx  = pOut->aTime + pOut->nTime * 8;
	pOut->aType = pOut->aIdx + pOut->nTime;
	pOut->zChar = (const char *)(pOut->aType + pOut->nType * 6);
	/* The footer is `\n` RULE `\n` at the very end of the block. An empty rule
	 * (the two newlines adjacent) is how a zone with no future transitions --
	 * `UTC`, `Etc/GMT+5` -- says so. */
	pOut->zPosix = 0;
	pOut->nPosix = 0;
	z = (const unsigned char *)pOut->zChar + pOut->nChar + nLeap * 12 + nIsStd + nIsUt;
	if( z < zEnd && z[0] == '\n' ){
		const unsigned char *zStop = z + 1;
		while( zStop < zEnd && zStop[0] != '\n' ){
			zStop++;
		}
		if( zStop > z + 1 ){
			pOut->zPosix = (const char *)(z + 1);
			pOut->nPosix = (int)(zStop - (z + 1));
		}
	}
	return 1;
}

/* The abbreviation ttinfo iType names, bounded by the blob it indexes into. */
static void TzAbbr(const tz_block *pB,sxu32 iType,const char **pzAbbr,int *pnAbbr)
{
	sxu32 iChar = pB->aType[iType * 6 + 5];
	sxu32 n = 0;
	if( iChar >= pB->nChar ){
		*pzAbbr = "";
		*pnAbbr = 0;
		return;
	}
	while( iChar + n < pB->nChar && pB->zChar[iChar + n] != 0 ){
		n++;
	}
	*pzAbbr = pB->zChar + iChar;
	*pnAbbr = (int)n;
}

/* The type a moment BEFORE the first transition takes: RFC 8536's rule, which
 * is also what every zone's LMT row wants. */
static sxu32 TzFirstType(const tz_block *pB)
{
	sxu32 i;
	for( i = 0 ; i < pB->nType ; ++i ){
		if( pB->aType[i * 6 + 4] == 0 ){
			return i;
		}
	}
	return 0;
}

/* --- The POSIX footer ----------------------------------------------------- *
 *
 * `EST5EDT,M3.2.0,M11.1.0` -- a standard name and offset, an optional DST name
 * and offset, and the two dates the year switches on. The offset's SIGN is
 * inverted against everything else here: POSIX writes the seconds to ADD to
 * local time to reach UT, so `EST5` is -5 hours.
 */
typedef struct tz_posix tz_posix;
struct tz_posix
{
	const char *zStd;  int nStd;  sxi32 iStd;
	const char *zDst;  int nDst;  sxi32 iDst;
	int bHasDst;
	/* Each rule is one of: J n (a 1-based day, never counting 29 Feb),
	 * n (a 0-based day that does), or M m.w.d. */
	int eStartKind,iStartM,iStartW,iStartD; sxi32 iStartSec;
	int eEndKind,iEndM,iEndW,iEndD;         sxi32 iEndSec;
};
#define TZ_RULE_J 0
#define TZ_RULE_N 1
#define TZ_RULE_M 2

/* A name is either a run of letters or a `<...>` quoted run (which is how a
 * numeric abbreviation like `<+07>` is spelled). */
static int TzPosixName(const char **pz,const char *zEnd,const char **pzName,int *pnName)
{
	const char *z = *pz;
	if( z < zEnd && z[0] == '<' ){
		const char *zStart = ++z;
		while( z < zEnd && z[0] != '>' ){
			z++;
		}
		if( z >= zEnd ){
			return 0;
		}
		*pzName = zStart;
		*pnName = (int)(z - zStart);
		*pz = z + 1;
		return 1;
	}
	{
		const char *zStart = z;
		while( z < zEnd && ((z[0] >= 'A' && z[0] <= 'Z') || (z[0] >= 'a' && z[0] <= 'z')) ){
			z++;
		}
		if( z - zStart < 3 ){
			return 0;
		}
		*pzName = zStart;
		*pnName = (int)(z - zStart);
		*pz = z;
		return 1;
	}
}

/* A plain unsigned decimal run, at most six digits. */
static int TzPosixNum(const char **pz,const char *zEnd,sxi32 *piOut)
{
	const char *z = *pz;
	sxi32 v = 0;
	int nDigit = 0;
	while( z < zEnd && z[0] >= '0' && z[0] <= '9' ){
		v = v * 10 + (z[0] - '0');
		z++;
		if( ++nDigit > 6 ){
			return 0;
		}
	}
	if( nDigit == 0 ){
		return 0;
	}
	*piOut = v;
	*pz = z;
	return 1;
}

/*
 * `[+-]hh[:mm[:ss]]`, answered as SIGNED SECONDS OF THE CLOCK -- the reading as
 * written, sign and all. The POSIX inversion (a zone's offset field counts
 * seconds to ADD to reach UT, so `EST5` is five hours WEST) belongs to the two
 * callers that read a zone offset, not here: the `/time` field of a switch rule
 * is an ordinary clock reading and must not be flipped.
 */
static int TzPosixClock(const char **pz,const char *zEnd,sxi32 *piOut)
{
	const char *z = *pz;
	int iSign = 1,nPart = 0;
	sxi32 aPart[3];
	aPart[0] = aPart[1] = aPart[2] = 0;
	if( z < zEnd && (z[0] == '+' || z[0] == '-') ){
		iSign = z[0] == '-' ? -1 : 1;
		z++;
	}
	for(;;){
		if( !TzPosixNum(&z,zEnd,&aPart[nPart]) ){
			return 0;
		}
		nPart++;
		if( nPart == 3 || z >= zEnd || z[0] != ':' ){
			break;
		}
		z++;
	}
	*piOut = iSign * (aPart[0] * 3600 + aPart[1] * 60 + aPart[2]);
	*pz = z;
	return 1;
}

/* A zone offset: the clock reading with POSIX's sign inverted, so what comes
 * back is the seconds EAST of UT that the rest of this file speaks. */
static int TzPosixOffset(const char **pz,const char *zEnd,sxi32 *piOut)
{
	sxi32 v;
	if( !TzPosixClock(pz,zEnd,&v) ){
		return 0;
	}
	*piOut = -v;
	return 1;
}

/* `,Mm.w.d[/time]`, `,Jn[/time]` or `,n[/time]`. */
static int TzPosixRule(const char **pz,const char *zEnd,int *peKind,int *piM,int *piW,
	int *piD,sxi32 *piSec)
{
	const char *z = *pz;
	sxi32 iSec = 2 * 3600;   /* POSIX's default switch time is 02:00 local */
	if( z >= zEnd ){
		return 0;
	}
	if( z[0] == 'M' ){
		sxi32 a,b,c;
		z++;
		if( !TzPosixNum(&z,zEnd,&a) || z >= zEnd || z[0] != '.' ){
			return 0;
		}
		z++;
		if( !TzPosixNum(&z,zEnd,&b) || z >= zEnd || z[0] != '.' ){
			return 0;
		}
		z++;
		if( !TzPosixNum(&z,zEnd,&c) ){
			return 0;
		}
		*peKind = TZ_RULE_M;
		*piM = (int)a;
		*piW = (int)b;
		*piD = (int)c;
		if( *piM < 1 || *piM > 12 || *piW < 1 || *piW > 5 || *piD < 0 || *piD > 6 ){
			return 0;
		}
	}else{
		int bJ = 0;
		sxi32 n;
		if( z[0] == 'J' ){
			bJ = 1;
			z++;
		}
		if( !TzPosixNum(&z,zEnd,&n) ){
			return 0;
		}
		*peKind = bJ ? TZ_RULE_J : TZ_RULE_N;
		*piM = *piW = 0;
		*piD = (int)n;
	}
	if( z < zEnd && z[0] == '/' ){
		z++;
		if( !TzPosixClock(&z,zEnd,&iSec) ){
			return 0;
		}
	}
	*piSec = iSec;
	*pz = z;
	return 1;
}

static int TzPosixParse(const char *zIn,int nIn,tz_posix *pOut)
{
	const char *z = zIn,*zEnd = zIn + nIn;
	SyZero(pOut,sizeof(*pOut));
	if( !TzPosixName(&z,zEnd,&pOut->zStd,&pOut->nStd) ){
		return 0;
	}
	if( !TzPosixOffset(&z,zEnd,&pOut->iStd) ){
		return 0;
	}
	if( z >= zEnd ){
		return 1;   /* a zone with no DST at all: `<+07>-7` */
	}
	if( !TzPosixName(&z,zEnd,&pOut->zDst,&pOut->nDst) ){
		return 0;
	}
	pOut->bHasDst = 1;
	/* The DST offset may be left out, and then it is one hour east of standard. */
	if( z < zEnd && z[0] != ',' ){
		if( !TzPosixOffset(&z,zEnd,&pOut->iDst) ){
			return 0;
		}
	}else{
		pOut->iDst = pOut->iStd + 3600;
	}
	if( z >= zEnd || z[0] != ',' ){
		return 0;
	}
	z++;
	if( !TzPosixRule(&z,zEnd,&pOut->eStartKind,&pOut->iStartM,&pOut->iStartW,
			&pOut->iStartD,&pOut->iStartSec) ){
		return 0;
	}
	if( z >= zEnd || z[0] != ',' ){
		return 0;
	}
	z++;
	if( !TzPosixRule(&z,zEnd,&pOut->eEndKind,&pOut->iEndM,&pOut->iEndW,
			&pOut->iEndD,&pOut->iEndSec) ){
		return 0;
	}
	return z == zEnd;
}

static int TzIsLeap(sxi64 y)
{
	return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

/*
 * The LOCAL-time instant a rule names inside year y, as seconds since the
 * epoch with the rule's own clock reading folded in. The caller converts it to
 * UT with whichever offset was in force before the switch.
 */
static sxi64 TzRuleInstant(int eKind,int iM,int iW,int iD,sxi32 iSec,sxi64 y)
{
	static const int aLen[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
	sxi64 iDay;
	if( eKind == TZ_RULE_M ){
		int nLen = aLen[iM - 1] + (iM == 2 && TzIsLeap(y) ? 1 : 0);
		sxi64 iFirst = DtDaysFromCivil(y,iM,1);
		/* 1970-01-01 was a Thursday, so day 0 is weekday 4 with Sunday at 0. */
		int iWd = (int)(((iFirst % 7) + 7 + 4) % 7);
		int iShift = ((iD - iWd) + 7) % 7;
		int iMday = 1 + iShift + (iW - 1) * 7;
		while( iMday > nLen ){
			iMday -= 7;   /* week 5 means "the last one", however many there are */
		}
		iDay = DtDaysFromCivil(y,iM,iMday);
	}else if( eKind == TZ_RULE_J ){
		/* J counts 1..365 and never counts 29 February, so from March on it
		 * lands a day later in a leap year than the plain count would. */
		int n = iD;
		if( n >= 60 && TzIsLeap(y) ){
			n++;
		}
		iDay = DtDaysFromCivil(y,1,1) + (n - 1);
	}else{
		iDay = DtDaysFromCivil(y,1,1) + iD;
	}
	return iDay * (sxi64)86400 + iSec;
}

/*
 * Answer the footer's rule for instant iTs. Both switch instants are LOCAL
 * readings, so each is converted with the offset in force just BEFORE it --
 * standard time before the spring switch, DST before the autumn one -- which is
 * what makes a southern-hemisphere zone (whose DST spans the new year) come out
 * right.
 */
static void TzPosixAt(const tz_posix *pP,sxi64 iTs,sxi32 *piOff,int *pbDst,
	const char **pzAbbr,int *pnAbbr)
{
	sxi64 y;
	int m,d;
	sxi64 iStart,iEnd;
	int bDst;
	if( !pP->bHasDst ){
		*piOff = pP->iStd;
		*pbDst = 0;
		*pzAbbr = pP->zStd;
		*pnAbbr = pP->nStd;
		return;
	}
	DtCivilFromDays(DtFloorDiv(iTs + pP->iStd,86400),&y,&m,&d);
	iStart = TzRuleInstant(pP->eStartKind,pP->iStartM,pP->iStartW,pP->iStartD,
		pP->iStartSec,y) - pP->iStd;
	iEnd = TzRuleInstant(pP->eEndKind,pP->iEndM,pP->iEndW,pP->iEndD,
		pP->iEndSec,y) - pP->iDst;
	if( iStart <= iEnd ){
		bDst = (iTs >= iStart && iTs < iEnd);
	}else{
		/* Southern hemisphere: DST runs from the start date to the END DATE OF
		 * THE NEXT YEAR, so a moment is standard time only in the gap between. */
		bDst = !(iTs >= iEnd && iTs < iStart);
	}
	*piOff = bDst ? pP->iDst : pP->iStd;
	*pbDst = bDst;
	*pzAbbr = bDst ? pP->zDst : pP->zStd;
	*pnAbbr = bDst ? pP->nDst : pP->nStd;
}

/* --- The public doors ----------------------------------------------------- */

/*
 * Resolve an identifier, folding case: php looks the name up case-insensitively
 * and then stores the CALLER's spelling, so `new DateTimeZone('europe/paris')`
 * is named `europe/paris` and still knows about Paris. Answers a row index or
 * -1.
 *
 * aTzFold is the row order sorted on the FOLDED name, which is what lets this
 * be a binary search over a table the generator sorted byte-wise for
 * `timezone_identifiers_list()`.
 */
PH7_PRIVATE int PH7_TzFind(const char *zName,int nName)
{
	int iLo = 0,iHi = PH7_TZDB_ZONE_COUNT - 1;
	if( zName == 0 || nName < 1 ){
		return -1;
	}
	while( iLo <= iHi ){
		int iMid = iLo + (iHi - iLo) / 2;
		const PH7_TzZoneRow *pRow = &aTzZone[aTzFold[iMid]];
		int n = nName < pRow->nName ? nName : (int)pRow->nName;
		int c = SyStrnicmp(zName,pRow->zName,(sxu32)n);
		if( c == 0 ){
			c = nName - (int)pRow->nName;
		}
		if( c == 0 ){
			return (int)aTzFold[iMid];
		}
		if( c < 0 ){
			iHi = iMid - 1;
		}else{
			iLo = iMid + 1;
		}
	}
	return -1;
}

PH7_PRIVATE int PH7_TzCount(void)
{
	return PH7_TZDB_ZONE_COUNT;
}

/*
 * php's ABBREVIATION table, which is asked BEFORE the database and is the whole
 * reason `new DateTimeZone('CET')` is a fixed +01:00 that never observes
 * daylight time while the zone FILE of that name switches twice a year. Ten
 * names sit in both tables and the abbreviation wins every one.
 *
 * An abbreviation is a FIXED offset -- the instant never enters -- carrying a
 * daylight flag that only `I` reads, and it answers the table's canonical
 * UPPER-CASE spelling rather than the caller's, which is the visible difference
 * from an identifier. Answers 0 when the name is not one.
 *
 * The table's names are letters only, so byte order over the upper-case
 * spellings is the same order case-folding produces and the search needs no
 * second index.
 */
PH7_PRIVATE int PH7_TzAbbrFind(const char *zName,int nName,sxi32 *piOff,int *pbDst,
	const char **pzCanon,int *pnCanon)
{
	int iLo = 0,iHi = PH7_TZDB_ABBR_COUNT - 1;
	if( zName == 0 || nName < 1 ){
		return 0;
	}
	while( iLo <= iHi ){
		int iMid = iLo + (iHi - iLo) / 2;
		const PH7_TzAbbrRow *pRow = &aTzAbbr[iMid];
		int n = nName < pRow->nName ? nName : (int)pRow->nName;
		int c = SyStrnicmp(zName,pRow->zName,(sxu32)n);
		if( c == 0 ){
			c = nName - (int)pRow->nName;
		}
		if( c == 0 ){
			*piOff = pRow->iOff;
			*pbDst = (int)pRow->bDst;
			*pzCanon = pRow->zName;
			*pnCanon = (int)pRow->nName;
			return 1;
		}
		if( c < 0 ){
			iHi = iMid - 1;
		}else{
			iLo = iMid + 1;
		}
	}
	return 0;
}

PH7_PRIVATE const char * PH7_TzName(int iZone,int *pnName,int *pbBackward)
{
	if( iZone < 0 || iZone >= PH7_TZDB_ZONE_COUNT ){
		return 0;
	}
	if( pnName ){
		*pnName = (int)aTzZone[iZone].nName;
	}
	if( pbBackward ){
		*pbBackward = (int)aTzZone[iZone].bBackward;
	}
	return aTzZone[iZone].zName;
}

/*
 * The offset, the is-DST flag and the abbreviation zone iZone is on at iTs.
 * The abbreviation points either into the payload or into the footer text, both
 * of which are static, so a caller may hold it.
 */
PH7_PRIVATE int PH7_TzOffsetAt(int iZone,sxi64 iTs,sxi32 *piOff,int *pbDst,
	const char **pzAbbr,int *pnAbbr)
{
	tz_block sB;
	sxu32 iType;
	sxi32 iOff = 0;
	int bDst = 0,nAbbr = 0;
	const char *zAbbr = "";
	if( !TzBlock(iZone,&sB) ){
		return 0;
	}
	if( sB.nTime == 0 || iTs < TzI64(sB.aTime) ){
		iType = TzFirstType(&sB);
	}else if( iTs >= TzI64(sB.aTime + (sB.nTime - 1) * 8) && sB.zPosix ){
		tz_posix sP;
		if( TzPosixParse(sB.zPosix,sB.nPosix,&sP) ){
			TzPosixAt(&sP,iTs,piOff,pbDst,pzAbbr,pnAbbr);
			return 1;
		}
		iType = sB.aIdx[sB.nTime - 1];
	}else{
		/* The last transition at or before iTs. */
		sxu32 iLo = 0,iHi = sB.nTime - 1;
		while( iLo < iHi ){
			sxu32 iMid = iLo + (iHi - iLo + 1) / 2;
			if( TzI64(sB.aTime + iMid * 8) <= iTs ){
				iLo = iMid;
			}else{
				iHi = iMid - 1;
			}
		}
		iType = sB.aIdx[iLo];
	}
	if( iType >= sB.nType ){
		iType = 0;
	}
	iOff = TzI32(sB.aType + iType * 6);
	bDst = sB.aType[iType * 6 + 4] != 0;
	TzAbbr(&sB,iType,&zAbbr,&nAbbr);
	*piOff = iOff;
	*pbDst = bDst;
	*pzAbbr = zAbbr;
	*pnAbbr = nAbbr;
	return 1;
}

/*
 * The instant a WALL CLOCK reading names. iLocal is the reading expressed as if
 * it were UT, which is the shape every date parser here already produces.
 *
 * Two guesses settle it for every unambiguous reading: read the offset at the
 * reading itself, subtract it, then read the offset at THAT instant and use it.
 * The two disagree only across a switch, and there php's rule is:
 *
 *   an hour DST SKIPPED   -- no instant reads that way -- takes the offset in
 *                            force BEFORE the gap, which pushes the answer
 *                            forward into DST (02:30 on a spring-forward day
 *                            is 03:30 local).
 *   an hour DST REPEATED  -- two instants read that way -- takes the FIRST,
 *                            the one still on the pre-switch offset.
 *
 * Both fall out of preferring the offset that was in force before: the fixed
 * point is searched from the earlier side.
 */
PH7_PRIVATE int PH7_TzLocalToUtc(int iZone,sxi64 iLocal,sxi64 *piTs,sxi32 *piOff)
{
	sxi32 iOff1 = 0,iOff2 = 0,iOff3 = 0;
	int bDst;
	const char *zAbbr;
	int nAbbr;
	if( !PH7_TzOffsetAt(iZone,iLocal,&iOff1,&bDst,&zAbbr,&nAbbr) ){
		return 0;
	}
	if( !PH7_TzOffsetAt(iZone,iLocal - iOff1,&iOff2,&bDst,&zAbbr,&nAbbr) ){
		return 0;
	}
	if( iOff2 == iOff1 ){
		*piTs = iLocal - iOff1;
		*piOff = iOff1;
		return 1;
	}
	/* The two disagree, so the reading sits at or near a switch. Try the second
	 * guess's own instant: when it is stable, that is the answer. An hour DST
	 * REPEATED lands here and takes the FIRST of its two instants, because the
	 * first guess already read the pre-switch offset. */
	if( !PH7_TzOffsetAt(iZone,iLocal - iOff2,&iOff3,&bDst,&zAbbr,&nAbbr) ){
		return 0;
	}
	if( iOff3 == iOff2 ){
		*piTs = iLocal - iOff2;
		*piOff = iOff2;
		return 1;
	}
	/* Neither is a fixed point: the reading names no instant at all, because
	 * DST skipped that hour. php reads it with the offset in force BEFORE the
	 * gap, which lands the answer just past the switch -- 02:30 on a
	 * spring-forward morning in New York is 03:30 EDT.
	 *
	 * "Before the gap" is the SMALLER of the two offsets, not the first guess:
	 * a gap is always a forward jump, so the offset ahead of it is the lower
	 * one, and which of the two guesses found it depends on the sign of the
	 * zone's offset. Reading it off guess one is right in New York and wrong in
	 * Paris, where the local-as-if-UTC instant lands on the far side of the
	 * switch instead of the near one.
	 *
	 * The offset REPORTED is the one at the instant it landed on, not the one it
	 * was read with, which is why every arm here re-reads instead of answering
	 * its guess. */
	*piTs = iLocal - (iOff1 < iOff2 ? iOff1 : iOff2);
	PH7_TzOffsetAt(iZone,*piTs,piOff,&bDst,&zAbbr,&nAbbr);
	return 1;
}

#endif /* PH7_ENABLE_TZDB && !PH7_DISABLE_BUILTIN_FUNC */
