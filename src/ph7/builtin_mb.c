/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * mb_* multibyte string functions, UTF-8 only (NEWPLAN band D; the recorded
 * §10 scope cut — php's full encoding zoo is out). Codepoint semantics match
 * php 8.5 byte-for-byte for UTF-8 input; case mapping is algorithmic over
 * ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic (full Unicode tables
 * recorded as a residual — unmapped codepoints pass through unchanged).
 */

/* --- UTF-8 primitives ------------------------------------------------- */

/* Byte length of the ill-formed run at z[0..n-1]: the maximal prefix php's
 * decoder consumes before giving up — the lead byte plus every continuation
 * byte that is still in range for it. mbstring reports that whole prefix as ONE
 * character and substitutes ONE '?' for it, so "\xe0\xa0" (a truncated 3-byte
 * sequence) is one character, while "\xff\xfe" is two: neither byte can lead. */
static sxu32 MbUtf8BadLen(const unsigned char *z,sxu32 n)
{
	sxu32 c = z[0],need,iLow,iHigh,i;
	if( c >= 0xC2 && c <= 0xDF ){
		need = 2; iLow = 0x80; iHigh = 0xBF;
	}else if( c >= 0xE0 && c <= 0xEF ){
		need = 3; iLow = (c == 0xE0) ? 0xA0 : 0x80; iHigh = (c == 0xED) ? 0x9F : 0xBF;
	}else if( c >= 0xF0 && c <= 0xF4 ){
		need = 4; iLow = (c == 0xF0) ? 0x90 : 0x80; iHigh = (c == 0xF4) ? 0x8F : 0xBF;
	}else{
		return 1; /* 80..C1 or F5..FF: cannot lead anything */
	}
	for( i = 1 ; i < need && i < n ; ++i ){
		sxu32 lo = (i == 1) ? iLow : 0x80;
		sxu32 hi = (i == 1) ? iHigh : 0xBF;
		if( z[i] < lo || z[i] > hi ){
			break;
		}
	}
	return i;
}
/* Decode the character at z (n bytes available); *pLen = the bytes it occupies.
 * Returns the codepoint, or -1 when the sequence is ILL-FORMED — in which case
 * *pLen is the run above, which php's mbstring counts as one character and
 * re-encodes as '?'.
 *
 * This used to be byte-transparent: an undecodable byte came back AS ITSELF
 * with length 1, so mb_strtolower("\xff\xfe") answered the two bytes
 * re-encoded as UTF-8 (\xc3\xbf\xc3\xbe) — latin-1 semantics php does not have,
 * and characters PHL invented — where php answers "??". The over-long,
 * surrogate and past-U+10FFFF forms were accepted as well; validation is
 * PH7_Utf8ReadStrict's job now (the same reader json_encode uses). */
static sxi32 MbUtf8Decode(const unsigned char *z,sxu32 n,sxu32 *pLen)
{
	sxi32 iCp = PH7_Utf8ReadStrict(z,n,pLen);
	if( iCp < 0 ){
		*pLen = MbUtf8BadLen(z,n);
	}
	return iCp;
}
/* Encode cp into z (up to 4 bytes); returns the byte count */
static sxu32 MbUtf8Encode(sxu32 cp,unsigned char *z)
{
	if( cp < 0x80 ){
		z[0] = (unsigned char)cp;
		return 1;
	}
	if( cp < 0x800 ){
		z[0] = (unsigned char)(0xC0 | (cp >> 6));
		z[1] = (unsigned char)(0x80 | (cp & 0x3F));
		return 2;
	}
	if( cp < 0x10000 ){
		z[0] = (unsigned char)(0xE0 | (cp >> 12));
		z[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
		z[2] = (unsigned char)(0x80 | (cp & 0x3F));
		return 3;
	}
	z[0] = (unsigned char)(0xF0 | (cp >> 18));
	z[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
	z[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
	z[3] = (unsigned char)(0x80 | (cp & 0x3F));
	return 4;
}
/* Byte offset of codepoint index iCp (clamped to the buffer end) */
static sxu32 MbUtf8Skip(const char *zIn,sxu32 nByte,sxu32 iCp)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nLen;
	while( i < nByte && iCp > 0 ){
		MbUtf8Decode(&z[i],nByte - i,&nLen);
		i += nLen;
		iCp--;
	}
	return i;
}

/* --- Case mapping (Unicode's own tables) ------------------------------ */

/*
 * php's mb_* case conversion is Unicode's, and these tables are php 8.5's own
 * answers for every code point, re-derived rather than reasoned about: a sweep
 * of all 1114112 of them through mb_convert_case() produced the mappings, run
 * -length encoded here (the paired blocks — U+0100..U+0177 and friends, where an
 * even code point upper-cases and the odd one after it lowers — are the reason a
 * row carries a STEP).
 *
 * What they replace is five hand-written ranges covering ASCII, Latin-1, Latin
 * Ext-A, Greek and Cyrillic, which left 1387 upper-case and 1299 lower-case
 * mappings unmade: mb_strtoupper() answered its argument unchanged for every
 * Latin Ext-B, Armenian, Georgian, Cherokee, Greek Extended, full-width and
 * Deseret letter, and got U+0131 (dotless i) wrong outright.
 *
 * Three tables, because Unicode has three mappings and title is not upper:
 * U+00DF upper-cases to "SS" and title-cases to "Ss", and the Latin digraphs
 * (U+01C4..U+01CC and friends) have a third form of their own. A mapping that
 * produces MORE than one character (php's full mapping — ﬁ upper-cases to FI)
 * lives in the *Full tables beside them.
 */
typedef struct mb_case_range mb_case_range;
struct mb_case_range {
	sxu32 iFirst;   /* first code point of the run */
	sxu32 iLast;    /* last one */
	sxu8  iStep;    /* 1 = every code point in the run, 2 = every other one */
	sxi32 iDelta;   /* what to add to reach the mapped code point */
};
typedef struct mb_case_full mb_case_full;
struct mb_case_full {
	sxu32 iCp;
	sxu32 aTo[3];   /* the replacement, NUL-terminated when shorter than 3 */
};
/* Simple UPPER-case mapping: [iFirst,iLast] stepping iStep, plus iDelta */
static const mb_case_range aMbUpper[] = {
	{0x61,0x7A,1,-32}, {0xB5,0xB5,1,743}, {0xE0,0xF6,1,-32}, {0xF8,0xFE,1,-32},
	{0xFF,0xFF,1,121}, {0x101,0x12F,2,-1}, {0x131,0x131,1,-232}, {0x133,0x137,2,-1},
	{0x13A,0x148,2,-1}, {0x14B,0x177,2,-1}, {0x17A,0x17E,2,-1}, {0x17F,0x17F,1,-300},
	{0x180,0x180,1,195}, {0x183,0x185,2,-1}, {0x188,0x188,1,-1}, {0x18C,0x18C,1,-1},
	{0x192,0x192,1,-1}, {0x195,0x195,1,97}, {0x199,0x199,1,-1}, {0x19A,0x19A,1,163},
	{0x19B,0x19B,1,42561}, {0x19E,0x19E,1,130}, {0x1A1,0x1A5,2,-1}, {0x1A8,0x1A8,1,-1},
	{0x1AD,0x1AD,1,-1}, {0x1B0,0x1B0,1,-1}, {0x1B4,0x1B6,2,-1}, {0x1B9,0x1B9,1,-1},
	{0x1BD,0x1BD,1,-1}, {0x1BF,0x1BF,1,56}, {0x1C5,0x1C5,1,-1}, {0x1C6,0x1C6,1,-2},
	{0x1C8,0x1C8,1,-1}, {0x1C9,0x1C9,1,-2}, {0x1CB,0x1CB,1,-1}, {0x1CC,0x1CC,1,-2},
	{0x1CE,0x1DC,2,-1}, {0x1DD,0x1DD,1,-79}, {0x1DF,0x1EF,2,-1}, {0x1F2,0x1F2,1,-1},
	{0x1F3,0x1F3,1,-2}, {0x1F5,0x1F5,1,-1}, {0x1F9,0x21F,2,-1}, {0x223,0x233,2,-1},
	{0x23C,0x23C,1,-1}, {0x23F,0x240,1,10815}, {0x242,0x242,1,-1}, {0x247,0x24F,2,-1},
	{0x250,0x250,1,10783}, {0x251,0x251,1,10780}, {0x252,0x252,1,10782}, {0x253,0x253,1,-210},
	{0x254,0x254,1,-206}, {0x256,0x257,1,-205}, {0x259,0x259,1,-202}, {0x25B,0x25B,1,-203},
	{0x25C,0x25C,1,42319}, {0x260,0x260,1,-205}, {0x261,0x261,1,42315}, {0x263,0x263,1,-207},
	{0x264,0x264,1,42343}, {0x265,0x265,1,42280}, {0x266,0x266,1,42308}, {0x268,0x268,1,-209},
	{0x269,0x269,1,-211}, {0x26A,0x26A,1,42308}, {0x26B,0x26B,1,10743}, {0x26C,0x26C,1,42305},
	{0x26F,0x26F,1,-211}, {0x271,0x271,1,10749}, {0x272,0x272,1,-213}, {0x275,0x275,1,-214},
	{0x27D,0x27D,1,10727}, {0x280,0x280,1,-218}, {0x282,0x282,1,42307}, {0x283,0x283,1,-218},
	{0x287,0x287,1,42282}, {0x288,0x288,1,-218}, {0x289,0x289,1,-69}, {0x28A,0x28B,1,-217},
	{0x28C,0x28C,1,-71}, {0x292,0x292,1,-219}, {0x29D,0x29D,1,42261}, {0x29E,0x29E,1,42258},
	{0x345,0x345,1,84}, {0x371,0x373,2,-1}, {0x377,0x377,1,-1}, {0x37B,0x37D,1,130},
	{0x3AC,0x3AC,1,-38}, {0x3AD,0x3AF,1,-37}, {0x3B1,0x3C1,1,-32}, {0x3C2,0x3C2,1,-31},
	{0x3C3,0x3CB,1,-32}, {0x3CC,0x3CC,1,-64}, {0x3CD,0x3CE,1,-63}, {0x3D0,0x3D0,1,-62},
	{0x3D1,0x3D1,1,-57}, {0x3D5,0x3D5,1,-47}, {0x3D6,0x3D6,1,-54}, {0x3D7,0x3D7,1,-8},
	{0x3D9,0x3EF,2,-1}, {0x3F0,0x3F0,1,-86}, {0x3F1,0x3F1,1,-80}, {0x3F2,0x3F2,1,7},
	{0x3F3,0x3F3,1,-116}, {0x3F5,0x3F5,1,-96}, {0x3F8,0x3F8,1,-1}, {0x3FB,0x3FB,1,-1},
	{0x430,0x44F,1,-32}, {0x450,0x45F,1,-80}, {0x461,0x481,2,-1}, {0x48B,0x4BF,2,-1},
	{0x4C2,0x4CE,2,-1}, {0x4CF,0x4CF,1,-15}, {0x4D1,0x52F,2,-1}, {0x561,0x586,1,-48},
	{0x10D0,0x10FA,1,3008}, {0x10FD,0x10FF,1,3008}, {0x13F8,0x13FD,1,-8}, {0x1C80,0x1C80,1,-6254},
	{0x1C81,0x1C81,1,-6253}, {0x1C82,0x1C82,1,-6244}, {0x1C83,0x1C84,1,-6242}, {0x1C85,0x1C85,1,-6243},
	{0x1C86,0x1C86,1,-6236}, {0x1C87,0x1C87,1,-6181}, {0x1C88,0x1C88,1,35266}, {0x1C8A,0x1C8A,1,-1},
	{0x1D79,0x1D79,1,35332}, {0x1D7D,0x1D7D,1,3814}, {0x1D8E,0x1D8E,1,35384}, {0x1E01,0x1E95,2,-1},
	{0x1E9B,0x1E9B,1,-59}, {0x1EA1,0x1EFF,2,-1}, {0x1F00,0x1F07,1,8}, {0x1F10,0x1F15,1,8},
	{0x1F20,0x1F27,1,8}, {0x1F30,0x1F37,1,8}, {0x1F40,0x1F45,1,8}, {0x1F51,0x1F57,2,8},
	{0x1F60,0x1F67,1,8}, {0x1F70,0x1F71,1,74}, {0x1F72,0x1F75,1,86}, {0x1F76,0x1F77,1,100},
	{0x1F78,0x1F79,1,128}, {0x1F7A,0x1F7B,1,112}, {0x1F7C,0x1F7D,1,126}, {0x1FB0,0x1FB1,1,8},
	{0x1FBE,0x1FBE,1,-7205}, {0x1FD0,0x1FD1,1,8}, {0x1FE0,0x1FE1,1,8}, {0x1FE5,0x1FE5,1,7},
	{0x214E,0x214E,1,-28}, {0x2170,0x217F,1,-16}, {0x2184,0x2184,1,-1}, {0x24D0,0x24E9,1,-26},
	{0x2C30,0x2C5F,1,-48}, {0x2C61,0x2C61,1,-1}, {0x2C65,0x2C65,1,-10795}, {0x2C66,0x2C66,1,-10792},
	{0x2C68,0x2C6C,2,-1}, {0x2C73,0x2C73,1,-1}, {0x2C76,0x2C76,1,-1}, {0x2C81,0x2CE3,2,-1},
	{0x2CEC,0x2CEE,2,-1}, {0x2CF3,0x2CF3,1,-1}, {0x2D00,0x2D25,1,-7264}, {0x2D27,0x2D27,1,-7264},
	{0x2D2D,0x2D2D,1,-7264}, {0xA641,0xA66D,2,-1}, {0xA681,0xA69B,2,-1}, {0xA723,0xA72F,2,-1},
	{0xA733,0xA76F,2,-1}, {0xA77A,0xA77C,2,-1}, {0xA77F,0xA787,2,-1}, {0xA78C,0xA78C,1,-1},
	{0xA791,0xA793,2,-1}, {0xA794,0xA794,1,48}, {0xA797,0xA7A9,2,-1}, {0xA7B5,0xA7C3,2,-1},
	{0xA7C8,0xA7CA,2,-1}, {0xA7CD,0xA7DB,2,-1}, {0xA7F6,0xA7F6,1,-1}, {0xAB53,0xAB53,1,-928},
	{0xAB70,0xABBF,1,-38864}, {0xFF41,0xFF5A,1,-32}, {0x10428,0x1044F,1,-40}, {0x104D8,0x104FB,1,-40},
	{0x10597,0x105A1,1,-39}, {0x105A3,0x105B1,1,-39}, {0x105B3,0x105B9,1,-39}, {0x105BB,0x105BC,1,-39},
	{0x10CC0,0x10CF2,1,-64}, {0x10D70,0x10D85,1,-32}, {0x118C0,0x118DF,1,-32}, {0x16E60,0x16E7F,1,-32},
	{0x16EBB,0x16ED3,1,-27}, {0x1E922,0x1E943,1,-34},
};
/* Upper-casing that produces more than one character (php's full mapping) */
static const mb_case_full aMbUpperFull[] = {
	{0xDF,{0x53,0x53,0x0}},
	{0x149,{0x2BC,0x4E,0x0}},
	{0x1F0,{0x4A,0x30C,0x0}},
	{0x390,{0x399,0x308,0x301}},
	{0x3B0,{0x3A5,0x308,0x301}},
	{0x587,{0x535,0x552,0x0}},
	{0x1E96,{0x48,0x331,0x0}},
	{0x1E97,{0x54,0x308,0x0}},
	{0x1E98,{0x57,0x30A,0x0}},
	{0x1E99,{0x59,0x30A,0x0}},
	{0x1E9A,{0x41,0x2BE,0x0}},
	{0x1F50,{0x3A5,0x313,0x0}},
	{0x1F52,{0x3A5,0x313,0x300}},
	{0x1F54,{0x3A5,0x313,0x301}},
	{0x1F56,{0x3A5,0x313,0x342}},
	{0x1F80,{0x1F08,0x399,0x0}},
	{0x1F81,{0x1F09,0x399,0x0}},
	{0x1F82,{0x1F0A,0x399,0x0}},
	{0x1F83,{0x1F0B,0x399,0x0}},
	{0x1F84,{0x1F0C,0x399,0x0}},
	{0x1F85,{0x1F0D,0x399,0x0}},
	{0x1F86,{0x1F0E,0x399,0x0}},
	{0x1F87,{0x1F0F,0x399,0x0}},
	{0x1F88,{0x1F08,0x399,0x0}},
	{0x1F89,{0x1F09,0x399,0x0}},
	{0x1F8A,{0x1F0A,0x399,0x0}},
	{0x1F8B,{0x1F0B,0x399,0x0}},
	{0x1F8C,{0x1F0C,0x399,0x0}},
	{0x1F8D,{0x1F0D,0x399,0x0}},
	{0x1F8E,{0x1F0E,0x399,0x0}},
	{0x1F8F,{0x1F0F,0x399,0x0}},
	{0x1F90,{0x1F28,0x399,0x0}},
	{0x1F91,{0x1F29,0x399,0x0}},
	{0x1F92,{0x1F2A,0x399,0x0}},
	{0x1F93,{0x1F2B,0x399,0x0}},
	{0x1F94,{0x1F2C,0x399,0x0}},
	{0x1F95,{0x1F2D,0x399,0x0}},
	{0x1F96,{0x1F2E,0x399,0x0}},
	{0x1F97,{0x1F2F,0x399,0x0}},
	{0x1F98,{0x1F28,0x399,0x0}},
	{0x1F99,{0x1F29,0x399,0x0}},
	{0x1F9A,{0x1F2A,0x399,0x0}},
	{0x1F9B,{0x1F2B,0x399,0x0}},
	{0x1F9C,{0x1F2C,0x399,0x0}},
	{0x1F9D,{0x1F2D,0x399,0x0}},
	{0x1F9E,{0x1F2E,0x399,0x0}},
	{0x1F9F,{0x1F2F,0x399,0x0}},
	{0x1FA0,{0x1F68,0x399,0x0}},
	{0x1FA1,{0x1F69,0x399,0x0}},
	{0x1FA2,{0x1F6A,0x399,0x0}},
	{0x1FA3,{0x1F6B,0x399,0x0}},
	{0x1FA4,{0x1F6C,0x399,0x0}},
	{0x1FA5,{0x1F6D,0x399,0x0}},
	{0x1FA6,{0x1F6E,0x399,0x0}},
	{0x1FA7,{0x1F6F,0x399,0x0}},
	{0x1FA8,{0x1F68,0x399,0x0}},
	{0x1FA9,{0x1F69,0x399,0x0}},
	{0x1FAA,{0x1F6A,0x399,0x0}},
	{0x1FAB,{0x1F6B,0x399,0x0}},
	{0x1FAC,{0x1F6C,0x399,0x0}},
	{0x1FAD,{0x1F6D,0x399,0x0}},
	{0x1FAE,{0x1F6E,0x399,0x0}},
	{0x1FAF,{0x1F6F,0x399,0x0}},
	{0x1FB2,{0x1FBA,0x399,0x0}},
	{0x1FB3,{0x391,0x399,0x0}},
	{0x1FB4,{0x386,0x399,0x0}},
	{0x1FB6,{0x391,0x342,0x0}},
	{0x1FB7,{0x391,0x342,0x399}},
	{0x1FBC,{0x391,0x399,0x0}},
	{0x1FC2,{0x1FCA,0x399,0x0}},
	{0x1FC3,{0x397,0x399,0x0}},
	{0x1FC4,{0x389,0x399,0x0}},
	{0x1FC6,{0x397,0x342,0x0}},
	{0x1FC7,{0x397,0x342,0x399}},
	{0x1FCC,{0x397,0x399,0x0}},
	{0x1FD2,{0x399,0x308,0x300}},
	{0x1FD3,{0x399,0x308,0x301}},
	{0x1FD6,{0x399,0x342,0x0}},
	{0x1FD7,{0x399,0x308,0x342}},
	{0x1FE2,{0x3A5,0x308,0x300}},
	{0x1FE3,{0x3A5,0x308,0x301}},
	{0x1FE4,{0x3A1,0x313,0x0}},
	{0x1FE6,{0x3A5,0x342,0x0}},
	{0x1FE7,{0x3A5,0x308,0x342}},
	{0x1FF2,{0x1FFA,0x399,0x0}},
	{0x1FF3,{0x3A9,0x399,0x0}},
	{0x1FF4,{0x38F,0x399,0x0}},
	{0x1FF6,{0x3A9,0x342,0x0}},
	{0x1FF7,{0x3A9,0x342,0x399}},
	{0x1FFC,{0x3A9,0x399,0x0}},
	{0xFB00,{0x46,0x46,0x0}},
	{0xFB01,{0x46,0x49,0x0}},
	{0xFB02,{0x46,0x4C,0x0}},
	{0xFB03,{0x46,0x46,0x49}},
	{0xFB04,{0x46,0x46,0x4C}},
	{0xFB05,{0x53,0x54,0x0}},
	{0xFB06,{0x53,0x54,0x0}},
	{0xFB13,{0x544,0x546,0x0}},
	{0xFB14,{0x544,0x535,0x0}},
	{0xFB15,{0x544,0x53B,0x0}},
	{0xFB16,{0x54E,0x546,0x0}},
	{0xFB17,{0x544,0x53D,0x0}},
};
/* Simple LOWER-case mapping */
static const mb_case_range aMbLower[] = {
	{0x41,0x5A,1,32}, {0xC0,0xD6,1,32}, {0xD8,0xDE,1,32}, {0x100,0x12E,2,1},
	{0x132,0x136,2,1}, {0x139,0x147,2,1}, {0x14A,0x176,2,1}, {0x178,0x178,1,-121},
	{0x179,0x17D,2,1}, {0x181,0x181,1,210}, {0x182,0x184,2,1}, {0x186,0x186,1,206},
	{0x187,0x187,1,1}, {0x189,0x18A,1,205}, {0x18B,0x18B,1,1}, {0x18E,0x18E,1,79},
	{0x18F,0x18F,1,202}, {0x190,0x190,1,203}, {0x191,0x191,1,1}, {0x193,0x193,1,205},
	{0x194,0x194,1,207}, {0x196,0x196,1,211}, {0x197,0x197,1,209}, {0x198,0x198,1,1},
	{0x19C,0x19C,1,211}, {0x19D,0x19D,1,213}, {0x19F,0x19F,1,214}, {0x1A0,0x1A4,2,1},
	{0x1A6,0x1A6,1,218}, {0x1A7,0x1A7,1,1}, {0x1A9,0x1A9,1,218}, {0x1AC,0x1AC,1,1},
	{0x1AE,0x1AE,1,218}, {0x1AF,0x1AF,1,1}, {0x1B1,0x1B2,1,217}, {0x1B3,0x1B5,2,1},
	{0x1B7,0x1B7,1,219}, {0x1B8,0x1B8,1,1}, {0x1BC,0x1BC,1,1}, {0x1C4,0x1C4,1,2},
	{0x1C5,0x1C5,1,1}, {0x1C7,0x1C7,1,2}, {0x1C8,0x1C8,1,1}, {0x1CA,0x1CA,1,2},
	{0x1CB,0x1DB,2,1}, {0x1DE,0x1EE,2,1}, {0x1F1,0x1F1,1,2}, {0x1F2,0x1F4,2,1},
	{0x1F6,0x1F6,1,-97}, {0x1F7,0x1F7,1,-56}, {0x1F8,0x21E,2,1}, {0x220,0x220,1,-130},
	{0x222,0x232,2,1}, {0x23A,0x23A,1,10795}, {0x23B,0x23B,1,1}, {0x23D,0x23D,1,-163},
	{0x23E,0x23E,1,10792}, {0x241,0x241,1,1}, {0x243,0x243,1,-195}, {0x244,0x244,1,69},
	{0x245,0x245,1,71}, {0x246,0x24E,2,1}, {0x370,0x372,2,1}, {0x376,0x376,1,1},
	{0x37F,0x37F,1,116}, {0x386,0x386,1,38}, {0x388,0x38A,1,37}, {0x38C,0x38C,1,64},
	{0x38E,0x38F,1,63}, {0x391,0x3A1,1,32}, {0x3A3,0x3AB,1,32}, {0x3CF,0x3CF,1,8},
	{0x3D8,0x3EE,2,1}, {0x3F4,0x3F4,1,-60}, {0x3F7,0x3F7,1,1}, {0x3F9,0x3F9,1,-7},
	{0x3FA,0x3FA,1,1}, {0x3FD,0x3FF,1,-130}, {0x400,0x40F,1,80}, {0x410,0x42F,1,32},
	{0x460,0x480,2,1}, {0x48A,0x4BE,2,1}, {0x4C0,0x4C0,1,15}, {0x4C1,0x4CD,2,1},
	{0x4D0,0x52E,2,1}, {0x531,0x556,1,48}, {0x10A0,0x10C5,1,7264}, {0x10C7,0x10C7,1,7264},
	{0x10CD,0x10CD,1,7264}, {0x13A0,0x13EF,1,38864}, {0x13F0,0x13F5,1,8}, {0x1C89,0x1C89,1,1},
	{0x1C90,0x1CBA,1,-3008}, {0x1CBD,0x1CBF,1,-3008}, {0x1E00,0x1E94,2,1}, {0x1E9E,0x1E9E,1,-7615},
	{0x1EA0,0x1EFE,2,1}, {0x1F08,0x1F0F,1,-8}, {0x1F18,0x1F1D,1,-8}, {0x1F28,0x1F2F,1,-8},
	{0x1F38,0x1F3F,1,-8}, {0x1F48,0x1F4D,1,-8}, {0x1F59,0x1F5F,2,-8}, {0x1F68,0x1F6F,1,-8},
	{0x1F88,0x1F8F,1,-8}, {0x1F98,0x1F9F,1,-8}, {0x1FA8,0x1FAF,1,-8}, {0x1FB8,0x1FB9,1,-8},
	{0x1FBA,0x1FBB,1,-74}, {0x1FBC,0x1FBC,1,-9}, {0x1FC8,0x1FCB,1,-86}, {0x1FCC,0x1FCC,1,-9},
	{0x1FD8,0x1FD9,1,-8}, {0x1FDA,0x1FDB,1,-100}, {0x1FE8,0x1FE9,1,-8}, {0x1FEA,0x1FEB,1,-112},
	{0x1FEC,0x1FEC,1,-7}, {0x1FF8,0x1FF9,1,-128}, {0x1FFA,0x1FFB,1,-126}, {0x1FFC,0x1FFC,1,-9},
	{0x2126,0x2126,1,-7517}, {0x212A,0x212A,1,-8383}, {0x212B,0x212B,1,-8262}, {0x2132,0x2132,1,28},
	{0x2160,0x216F,1,16}, {0x2183,0x2183,1,1}, {0x24B6,0x24CF,1,26}, {0x2C00,0x2C2F,1,48},
	{0x2C60,0x2C60,1,1}, {0x2C62,0x2C62,1,-10743}, {0x2C63,0x2C63,1,-3814}, {0x2C64,0x2C64,1,-10727},
	{0x2C67,0x2C6B,2,1}, {0x2C6D,0x2C6D,1,-10780}, {0x2C6E,0x2C6E,1,-10749}, {0x2C6F,0x2C6F,1,-10783},
	{0x2C70,0x2C70,1,-10782}, {0x2C72,0x2C72,1,1}, {0x2C75,0x2C75,1,1}, {0x2C7E,0x2C7F,1,-10815},
	{0x2C80,0x2CE2,2,1}, {0x2CEB,0x2CED,2,1}, {0x2CF2,0x2CF2,1,1}, {0xA640,0xA66C,2,1},
	{0xA680,0xA69A,2,1}, {0xA722,0xA72E,2,1}, {0xA732,0xA76E,2,1}, {0xA779,0xA77B,2,1},
	{0xA77D,0xA77D,1,-35332}, {0xA77E,0xA786,2,1}, {0xA78B,0xA78B,1,1}, {0xA78D,0xA78D,1,-42280},
	{0xA790,0xA792,2,1}, {0xA796,0xA7A8,2,1}, {0xA7AA,0xA7AA,1,-42308}, {0xA7AB,0xA7AB,1,-42319},
	{0xA7AC,0xA7AC,1,-42315}, {0xA7AD,0xA7AD,1,-42305}, {0xA7AE,0xA7AE,1,-42308}, {0xA7B0,0xA7B0,1,-42258},
	{0xA7B1,0xA7B1,1,-42282}, {0xA7B2,0xA7B2,1,-42261}, {0xA7B3,0xA7B3,1,928}, {0xA7B4,0xA7C2,2,1},
	{0xA7C4,0xA7C4,1,-48}, {0xA7C5,0xA7C5,1,-42307}, {0xA7C6,0xA7C6,1,-35384}, {0xA7C7,0xA7C9,2,1},
	{0xA7CB,0xA7CB,1,-42343}, {0xA7CC,0xA7DA,2,1}, {0xA7DC,0xA7DC,1,-42561}, {0xA7F5,0xA7F5,1,1},
	{0xFF21,0xFF3A,1,32}, {0x10400,0x10427,1,40}, {0x104B0,0x104D3,1,40}, {0x10570,0x1057A,1,39},
	{0x1057C,0x1058A,1,39}, {0x1058C,0x10592,1,39}, {0x10594,0x10595,1,39}, {0x10C80,0x10CB2,1,64},
	{0x10D50,0x10D65,1,32}, {0x118A0,0x118BF,1,32}, {0x16E40,0x16E5F,1,32}, {0x16EA0,0x16EB8,1,27},
	{0x1E900,0x1E921,1,34},
};
/* Lower-casing that produces more than one character */
static const mb_case_full aMbLowerFull[] = {
	{0x130,{0x69,0x307,0x0}},
};
/* Simple TITLE-case mapping (upper-case, except where Unicode has a third form) */
static const mb_case_range aMbTitle[] = {
	{0x61,0x7A,1,-32}, {0xB5,0xB5,1,743}, {0xE0,0xF6,1,-32}, {0xF8,0xFE,1,-32},
	{0xFF,0xFF,1,121}, {0x101,0x12F,2,-1}, {0x131,0x131,1,-232}, {0x133,0x137,2,-1},
	{0x13A,0x148,2,-1}, {0x14B,0x177,2,-1}, {0x17A,0x17E,2,-1}, {0x17F,0x17F,1,-300},
	{0x180,0x180,1,195}, {0x183,0x185,2,-1}, {0x188,0x188,1,-1}, {0x18C,0x18C,1,-1},
	{0x192,0x192,1,-1}, {0x195,0x195,1,97}, {0x199,0x199,1,-1}, {0x19A,0x19A,1,163},
	{0x19B,0x19B,1,42561}, {0x19E,0x19E,1,130}, {0x1A1,0x1A5,2,-1}, {0x1A8,0x1A8,1,-1},
	{0x1AD,0x1AD,1,-1}, {0x1B0,0x1B0,1,-1}, {0x1B4,0x1B6,2,-1}, {0x1B9,0x1B9,1,-1},
	{0x1BD,0x1BD,1,-1}, {0x1BF,0x1BF,1,56}, {0x1C4,0x1C4,1,1}, {0x1C6,0x1C6,1,-1},
	{0x1C7,0x1C7,1,1}, {0x1C9,0x1C9,1,-1}, {0x1CA,0x1CA,1,1}, {0x1CC,0x1DC,2,-1},
	{0x1DD,0x1DD,1,-79}, {0x1DF,0x1EF,2,-1}, {0x1F1,0x1F1,1,1}, {0x1F3,0x1F5,2,-1},
	{0x1F9,0x21F,2,-1}, {0x223,0x233,2,-1}, {0x23C,0x23C,1,-1}, {0x23F,0x240,1,10815},
	{0x242,0x242,1,-1}, {0x247,0x24F,2,-1}, {0x250,0x250,1,10783}, {0x251,0x251,1,10780},
	{0x252,0x252,1,10782}, {0x253,0x253,1,-210}, {0x254,0x254,1,-206}, {0x256,0x257,1,-205},
	{0x259,0x259,1,-202}, {0x25B,0x25B,1,-203}, {0x25C,0x25C,1,42319}, {0x260,0x260,1,-205},
	{0x261,0x261,1,42315}, {0x263,0x263,1,-207}, {0x264,0x264,1,42343}, {0x265,0x265,1,42280},
	{0x266,0x266,1,42308}, {0x268,0x268,1,-209}, {0x269,0x269,1,-211}, {0x26A,0x26A,1,42308},
	{0x26B,0x26B,1,10743}, {0x26C,0x26C,1,42305}, {0x26F,0x26F,1,-211}, {0x271,0x271,1,10749},
	{0x272,0x272,1,-213}, {0x275,0x275,1,-214}, {0x27D,0x27D,1,10727}, {0x280,0x280,1,-218},
	{0x282,0x282,1,42307}, {0x283,0x283,1,-218}, {0x287,0x287,1,42282}, {0x288,0x288,1,-218},
	{0x289,0x289,1,-69}, {0x28A,0x28B,1,-217}, {0x28C,0x28C,1,-71}, {0x292,0x292,1,-219},
	{0x29D,0x29D,1,42261}, {0x29E,0x29E,1,42258}, {0x345,0x345,1,84}, {0x371,0x373,2,-1},
	{0x377,0x377,1,-1}, {0x37B,0x37D,1,130}, {0x3AC,0x3AC,1,-38}, {0x3AD,0x3AF,1,-37},
	{0x3B1,0x3C1,1,-32}, {0x3C2,0x3C2,1,-31}, {0x3C3,0x3CB,1,-32}, {0x3CC,0x3CC,1,-64},
	{0x3CD,0x3CE,1,-63}, {0x3D0,0x3D0,1,-62}, {0x3D1,0x3D1,1,-57}, {0x3D5,0x3D5,1,-47},
	{0x3D6,0x3D6,1,-54}, {0x3D7,0x3D7,1,-8}, {0x3D9,0x3EF,2,-1}, {0x3F0,0x3F0,1,-86},
	{0x3F1,0x3F1,1,-80}, {0x3F2,0x3F2,1,7}, {0x3F3,0x3F3,1,-116}, {0x3F5,0x3F5,1,-96},
	{0x3F8,0x3F8,1,-1}, {0x3FB,0x3FB,1,-1}, {0x430,0x44F,1,-32}, {0x450,0x45F,1,-80},
	{0x461,0x481,2,-1}, {0x48B,0x4BF,2,-1}, {0x4C2,0x4CE,2,-1}, {0x4CF,0x4CF,1,-15},
	{0x4D1,0x52F,2,-1}, {0x561,0x586,1,-48}, {0x13F8,0x13FD,1,-8}, {0x1C80,0x1C80,1,-6254},
	{0x1C81,0x1C81,1,-6253}, {0x1C82,0x1C82,1,-6244}, {0x1C83,0x1C84,1,-6242}, {0x1C85,0x1C85,1,-6243},
	{0x1C86,0x1C86,1,-6236}, {0x1C87,0x1C87,1,-6181}, {0x1C88,0x1C88,1,35266}, {0x1C8A,0x1C8A,1,-1},
	{0x1D79,0x1D79,1,35332}, {0x1D7D,0x1D7D,1,3814}, {0x1D8E,0x1D8E,1,35384}, {0x1E01,0x1E95,2,-1},
	{0x1E9B,0x1E9B,1,-59}, {0x1EA1,0x1EFF,2,-1}, {0x1F00,0x1F07,1,8}, {0x1F10,0x1F15,1,8},
	{0x1F20,0x1F27,1,8}, {0x1F30,0x1F37,1,8}, {0x1F40,0x1F45,1,8}, {0x1F51,0x1F57,2,8},
	{0x1F60,0x1F67,1,8}, {0x1F70,0x1F71,1,74}, {0x1F72,0x1F75,1,86}, {0x1F76,0x1F77,1,100},
	{0x1F78,0x1F79,1,128}, {0x1F7A,0x1F7B,1,112}, {0x1F7C,0x1F7D,1,126}, {0x1F80,0x1F87,1,8},
	{0x1F90,0x1F97,1,8}, {0x1FA0,0x1FA7,1,8}, {0x1FB0,0x1FB1,1,8}, {0x1FB3,0x1FB3,1,9},
	{0x1FBE,0x1FBE,1,-7205}, {0x1FC3,0x1FC3,1,9}, {0x1FD0,0x1FD1,1,8}, {0x1FE0,0x1FE1,1,8},
	{0x1FE5,0x1FE5,1,7}, {0x1FF3,0x1FF3,1,9}, {0x214E,0x214E,1,-28}, {0x2170,0x217F,1,-16},
	{0x2184,0x2184,1,-1}, {0x24D0,0x24E9,1,-26}, {0x2C30,0x2C5F,1,-48}, {0x2C61,0x2C61,1,-1},
	{0x2C65,0x2C65,1,-10795}, {0x2C66,0x2C66,1,-10792}, {0x2C68,0x2C6C,2,-1}, {0x2C73,0x2C73,1,-1},
	{0x2C76,0x2C76,1,-1}, {0x2C81,0x2CE3,2,-1}, {0x2CEC,0x2CEE,2,-1}, {0x2CF3,0x2CF3,1,-1},
	{0x2D00,0x2D25,1,-7264}, {0x2D27,0x2D27,1,-7264}, {0x2D2D,0x2D2D,1,-7264}, {0xA641,0xA66D,2,-1},
	{0xA681,0xA69B,2,-1}, {0xA723,0xA72F,2,-1}, {0xA733,0xA76F,2,-1}, {0xA77A,0xA77C,2,-1},
	{0xA77F,0xA787,2,-1}, {0xA78C,0xA78C,1,-1}, {0xA791,0xA793,2,-1}, {0xA794,0xA794,1,48},
	{0xA797,0xA7A9,2,-1}, {0xA7B5,0xA7C3,2,-1}, {0xA7C8,0xA7CA,2,-1}, {0xA7CD,0xA7DB,2,-1},
	{0xA7F6,0xA7F6,1,-1}, {0xAB53,0xAB53,1,-928}, {0xAB70,0xABBF,1,-38864}, {0xFF41,0xFF5A,1,-32},
	{0x10428,0x1044F,1,-40}, {0x104D8,0x104FB,1,-40}, {0x10597,0x105A1,1,-39}, {0x105A3,0x105B1,1,-39},
	{0x105B3,0x105B9,1,-39}, {0x105BB,0x105BC,1,-39}, {0x10CC0,0x10CF2,1,-64}, {0x10D70,0x10D85,1,-32},
	{0x118C0,0x118DF,1,-32}, {0x16E60,0x16E7F,1,-32}, {0x16EBB,0x16ED3,1,-27}, {0x1E922,0x1E943,1,-34},
};
/* Title-casing that produces more than one character */
static const mb_case_full aMbTitleFull[] = {
	{0xDF,{0x53,0x73,0x0}},
	{0x149,{0x2BC,0x4E,0x0}},
	{0x1F0,{0x4A,0x30C,0x0}},
	{0x390,{0x399,0x308,0x301}},
	{0x3B0,{0x3A5,0x308,0x301}},
	{0x587,{0x535,0x582,0x0}},
	{0x1E96,{0x48,0x331,0x0}},
	{0x1E97,{0x54,0x308,0x0}},
	{0x1E98,{0x57,0x30A,0x0}},
	{0x1E99,{0x59,0x30A,0x0}},
	{0x1E9A,{0x41,0x2BE,0x0}},
	{0x1F50,{0x3A5,0x313,0x0}},
	{0x1F52,{0x3A5,0x313,0x300}},
	{0x1F54,{0x3A5,0x313,0x301}},
	{0x1F56,{0x3A5,0x313,0x342}},
	{0x1FB2,{0x1FBA,0x345,0x0}},
	{0x1FB4,{0x386,0x345,0x0}},
	{0x1FB6,{0x391,0x342,0x0}},
	{0x1FB7,{0x391,0x342,0x345}},
	{0x1FC2,{0x1FCA,0x345,0x0}},
	{0x1FC4,{0x389,0x345,0x0}},
	{0x1FC6,{0x397,0x342,0x0}},
	{0x1FC7,{0x397,0x342,0x345}},
	{0x1FD2,{0x399,0x308,0x300}},
	{0x1FD3,{0x399,0x308,0x301}},
	{0x1FD6,{0x399,0x342,0x0}},
	{0x1FD7,{0x399,0x308,0x342}},
	{0x1FE2,{0x3A5,0x308,0x300}},
	{0x1FE3,{0x3A5,0x308,0x301}},
	{0x1FE4,{0x3A1,0x313,0x0}},
	{0x1FE6,{0x3A5,0x342,0x0}},
	{0x1FE7,{0x3A5,0x308,0x342}},
	{0x1FF2,{0x1FFA,0x345,0x0}},
	{0x1FF4,{0x38F,0x345,0x0}},
	{0x1FF6,{0x3A9,0x342,0x0}},
	{0x1FF7,{0x3A9,0x342,0x345}},
	{0xFB00,{0x46,0x66,0x0}},
	{0xFB01,{0x46,0x69,0x0}},
	{0xFB02,{0x46,0x6C,0x0}},
	{0xFB03,{0x46,0x66,0x69}},
	{0xFB04,{0x46,0x66,0x6C}},
	{0xFB05,{0x53,0x74,0x0}},
	{0xFB06,{0x53,0x74,0x0}},
	{0xFB13,{0x544,0x576,0x0}},
	{0xFB14,{0x544,0x565,0x0}},
	{0xFB15,{0x544,0x56B,0x0}},
	{0xFB16,{0x54E,0x576,0x0}},
	{0xFB17,{0x544,0x56D,0x0}},
};
/* Unicode's Cased property: a character that CARRIES case */
static const sxu32 aMbCased[][2] = {
	{0x41,0x5A}, {0x61,0x7A}, {0xAA,0xAA}, {0xB5,0xB5}, {0xBA,0xBA},
	{0xC0,0xD6}, {0xD8,0xF6}, {0xF8,0x1BA}, {0x1BC,0x1BF}, {0x1C4,0x293},
	{0x296,0x2AF}, {0x370,0x373}, {0x376,0x377}, {0x37B,0x37D}, {0x37F,0x37F},
	{0x386,0x386}, {0x388,0x38A}, {0x38C,0x38C}, {0x38E,0x3A1}, {0x3A3,0x3F5},
	{0x3F7,0x481}, {0x48A,0x52F}, {0x531,0x556}, {0x560,0x588}, {0x10A0,0x10C5},
	{0x10C7,0x10C7}, {0x10CD,0x10CD}, {0x10D0,0x10FA}, {0x10FD,0x10FF}, {0x13A0,0x13F5},
	{0x13F8,0x13FD}, {0x1C80,0x1C8A}, {0x1C90,0x1CBA}, {0x1CBD,0x1CBF}, {0x1D00,0x1D2B},
	{0x1D6B,0x1D77}, {0x1D79,0x1D9A}, {0x1E00,0x1F15}, {0x1F18,0x1F1D}, {0x1F20,0x1F45},
	{0x1F48,0x1F4D}, {0x1F50,0x1F57}, {0x1F59,0x1F59}, {0x1F5B,0x1F5B}, {0x1F5D,0x1F5D},
	{0x1F5F,0x1F7D}, {0x1F80,0x1FB4}, {0x1FB6,0x1FBC}, {0x1FBE,0x1FBE}, {0x1FC2,0x1FC4},
	{0x1FC6,0x1FCC}, {0x1FD0,0x1FD3}, {0x1FD6,0x1FDB}, {0x1FE0,0x1FEC}, {0x1FF2,0x1FF4},
	{0x1FF6,0x1FFC}, {0x2102,0x2102}, {0x2107,0x2107}, {0x210A,0x2113}, {0x2115,0x2115},
	{0x2119,0x211D}, {0x2124,0x2124}, {0x2126,0x2126}, {0x2128,0x2128}, {0x212A,0x212D},
	{0x212F,0x2134}, {0x2139,0x2139}, {0x213C,0x213F}, {0x2145,0x2149}, {0x214E,0x214E},
	{0x2160,0x217F}, {0x2183,0x2184}, {0x24B6,0x24E9}, {0x2C00,0x2C7B}, {0x2C7E,0x2CE4},
	{0x2CEB,0x2CEE}, {0x2CF2,0x2CF3}, {0x2D00,0x2D25}, {0x2D27,0x2D27}, {0x2D2D,0x2D2D},
	{0xA640,0xA66D}, {0xA680,0xA69B}, {0xA722,0xA76F}, {0xA771,0xA787}, {0xA78B,0xA78E},
	{0xA790,0xA7DC}, {0xA7F5,0xA7F6}, {0xA7FA,0xA7FA}, {0xAB30,0xAB5A}, {0xAB60,0xAB68},
	{0xAB70,0xABBF}, {0xFB00,0xFB06}, {0xFB13,0xFB17}, {0xFF21,0xFF3A}, {0xFF41,0xFF5A},
	{0x10400,0x1044F}, {0x104B0,0x104D3}, {0x104D8,0x104FB}, {0x10570,0x1057A}, {0x1057C,0x1058A},
	{0x1058C,0x10592}, {0x10594,0x10595}, {0x10597,0x105A1}, {0x105A3,0x105B1}, {0x105B3,0x105B9},
	{0x105BB,0x105BC}, {0x10C80,0x10CB2}, {0x10CC0,0x10CF2}, {0x10D50,0x10D65}, {0x10D70,0x10D85},
	{0x118A0,0x118DF}, {0x16E40,0x16E7F}, {0x16EA0,0x16EB8}, {0x16EBB,0x16ED3}, {0x1D400,0x1D454},
	{0x1D456,0x1D49C}, {0x1D49E,0x1D49F}, {0x1D4A2,0x1D4A2}, {0x1D4A5,0x1D4A6}, {0x1D4A9,0x1D4AC},
	{0x1D4AE,0x1D4B9}, {0x1D4BB,0x1D4BB}, {0x1D4BD,0x1D4C3}, {0x1D4C5,0x1D505}, {0x1D507,0x1D50A},
	{0x1D50D,0x1D514}, {0x1D516,0x1D51C}, {0x1D51E,0x1D539}, {0x1D53B,0x1D53E}, {0x1D540,0x1D544},
	{0x1D546,0x1D546}, {0x1D54A,0x1D550}, {0x1D552,0x1D6A5}, {0x1D6A8,0x1D6C0}, {0x1D6C2,0x1D6DA},
	{0x1D6DC,0x1D6FA}, {0x1D6FC,0x1D714}, {0x1D716,0x1D734}, {0x1D736,0x1D74E}, {0x1D750,0x1D76E},
	{0x1D770,0x1D788}, {0x1D78A,0x1D7A8}, {0x1D7AA,0x1D7C2}, {0x1D7C4,0x1D7CB}, {0x1DF00,0x1DF09},
	{0x1DF0B,0x1DF1E}, {0x1DF25,0x1DF2A}, {0x1E900,0x1E943}, {0x1F130,0x1F149}, {0x1F150,0x1F169},
	{0x1F170,0x1F189},
};
/* Unicode's Case_Ignorable property: apostrophes, combining marks, modifier letters -- they leave a title-case word boundary exactly as they found it */
static const sxu32 aMbIgnorable[][2] = {
	{0x27,0x27}, {0x2E,0x2E}, {0x3A,0x3A}, {0x5E,0x5E}, {0x60,0x60},
	{0xA8,0xA8}, {0xAD,0xAD}, {0xAF,0xAF}, {0xB4,0xB4}, {0xB7,0xB8},
	{0x2B0,0x36F}, {0x374,0x375}, {0x37A,0x37A}, {0x384,0x385}, {0x387,0x387},
	{0x483,0x489}, {0x559,0x559}, {0x55F,0x55F}, {0x591,0x5BD}, {0x5BF,0x5BF},
	{0x5C1,0x5C2}, {0x5C4,0x5C5}, {0x5C7,0x5C7}, {0x5F4,0x5F4}, {0x600,0x605},
	{0x610,0x61A}, {0x61C,0x61C}, {0x640,0x640}, {0x64B,0x65F}, {0x670,0x670},
	{0x6D6,0x6DD}, {0x6DF,0x6E8}, {0x6EA,0x6ED}, {0x70F,0x70F}, {0x711,0x711},
	{0x730,0x74A}, {0x7A6,0x7B0}, {0x7EB,0x7F5}, {0x7FA,0x7FA}, {0x7FD,0x7FD},
	{0x816,0x82D}, {0x859,0x85B}, {0x888,0x888}, {0x890,0x891}, {0x897,0x89F},
	{0x8C9,0x902}, {0x93A,0x93A}, {0x93C,0x93C}, {0x941,0x948}, {0x94D,0x94D},
	{0x951,0x957}, {0x962,0x963}, {0x971,0x971}, {0x981,0x981}, {0x9BC,0x9BC},
	{0x9C1,0x9C4}, {0x9CD,0x9CD}, {0x9E2,0x9E3}, {0x9FE,0x9FE}, {0xA01,0xA02},
	{0xA3C,0xA3C}, {0xA41,0xA42}, {0xA47,0xA48}, {0xA4B,0xA4D}, {0xA51,0xA51},
	{0xA70,0xA71}, {0xA75,0xA75}, {0xA81,0xA82}, {0xABC,0xABC}, {0xAC1,0xAC5},
	{0xAC7,0xAC8}, {0xACD,0xACD}, {0xAE2,0xAE3}, {0xAFA,0xAFF}, {0xB01,0xB01},
	{0xB3C,0xB3C}, {0xB3F,0xB3F}, {0xB41,0xB44}, {0xB4D,0xB4D}, {0xB55,0xB56},
	{0xB62,0xB63}, {0xB82,0xB82}, {0xBC0,0xBC0}, {0xBCD,0xBCD}, {0xC00,0xC00},
	{0xC04,0xC04}, {0xC3C,0xC3C}, {0xC3E,0xC40}, {0xC46,0xC48}, {0xC4A,0xC4D},
	{0xC55,0xC56}, {0xC62,0xC63}, {0xC81,0xC81}, {0xCBC,0xCBC}, {0xCBF,0xCBF},
	{0xCC6,0xCC6}, {0xCCC,0xCCD}, {0xCE2,0xCE3}, {0xD00,0xD01}, {0xD3B,0xD3C},
	{0xD41,0xD44}, {0xD4D,0xD4D}, {0xD62,0xD63}, {0xD81,0xD81}, {0xDCA,0xDCA},
	{0xDD2,0xDD4}, {0xDD6,0xDD6}, {0xE31,0xE31}, {0xE34,0xE3A}, {0xE46,0xE4E},
	{0xEB1,0xEB1}, {0xEB4,0xEBC}, {0xEC6,0xEC6}, {0xEC8,0xECE}, {0xF18,0xF19},
	{0xF35,0xF35}, {0xF37,0xF37}, {0xF39,0xF39}, {0xF71,0xF7E}, {0xF80,0xF84},
	{0xF86,0xF87}, {0xF8D,0xF97}, {0xF99,0xFBC}, {0xFC6,0xFC6}, {0x102D,0x1030},
	{0x1032,0x1037}, {0x1039,0x103A}, {0x103D,0x103E}, {0x1058,0x1059}, {0x105E,0x1060},
	{0x1071,0x1074}, {0x1082,0x1082}, {0x1085,0x1086}, {0x108D,0x108D}, {0x109D,0x109D},
	{0x10FC,0x10FC}, {0x135D,0x135F}, {0x1712,0x1714}, {0x1732,0x1733}, {0x1752,0x1753},
	{0x1772,0x1773}, {0x17B4,0x17B5}, {0x17B7,0x17BD}, {0x17C6,0x17C6}, {0x17C9,0x17D3},
	{0x17D7,0x17D7}, {0x17DD,0x17DD}, {0x180B,0x180F}, {0x1843,0x1843}, {0x1885,0x1886},
	{0x18A9,0x18A9}, {0x1920,0x1922}, {0x1927,0x1928}, {0x1932,0x1932}, {0x1939,0x193B},
	{0x1A17,0x1A18}, {0x1A1B,0x1A1B}, {0x1A56,0x1A56}, {0x1A58,0x1A5E}, {0x1A60,0x1A60},
	{0x1A62,0x1A62}, {0x1A65,0x1A6C}, {0x1A73,0x1A7C}, {0x1A7F,0x1A7F}, {0x1AA7,0x1AA7},
	{0x1AB0,0x1ADD}, {0x1AE0,0x1AEB}, {0x1B00,0x1B03}, {0x1B34,0x1B34}, {0x1B36,0x1B3A},
	{0x1B3C,0x1B3C}, {0x1B42,0x1B42}, {0x1B6B,0x1B73}, {0x1B80,0x1B81}, {0x1BA2,0x1BA5},
	{0x1BA8,0x1BA9}, {0x1BAB,0x1BAD}, {0x1BE6,0x1BE6}, {0x1BE8,0x1BE9}, {0x1BED,0x1BED},
	{0x1BEF,0x1BF1}, {0x1C2C,0x1C33}, {0x1C36,0x1C37}, {0x1C78,0x1C7D}, {0x1CD0,0x1CD2},
	{0x1CD4,0x1CE0}, {0x1CE2,0x1CE8}, {0x1CED,0x1CED}, {0x1CF4,0x1CF4}, {0x1CF8,0x1CF9},
	{0x1D2C,0x1D6A}, {0x1D78,0x1D78}, {0x1D9B,0x1DFF}, {0x1FBD,0x1FBD}, {0x1FBF,0x1FC1},
	{0x1FCD,0x1FCF}, {0x1FDD,0x1FDF}, {0x1FED,0x1FEF}, {0x1FFD,0x1FFE}, {0x200B,0x200F},
	{0x2018,0x2019}, {0x2024,0x2024}, {0x2027,0x2027}, {0x202A,0x202E}, {0x2060,0x2064},
	{0x2066,0x206F}, {0x2071,0x2071}, {0x207F,0x207F}, {0x2090,0x209C}, {0x20D0,0x20F0},
	{0x2C7C,0x2C7D}, {0x2CEF,0x2CF1}, {0x2D6F,0x2D6F}, {0x2D7F,0x2D7F}, {0x2DE0,0x2DFF},
	{0x2E2F,0x2E2F}, {0x3005,0x3005}, {0x302A,0x302D}, {0x3031,0x3035}, {0x303B,0x303B},
	{0x3099,0x309E}, {0x30FC,0x30FE}, {0xA015,0xA015}, {0xA4F8,0xA4FD}, {0xA60C,0xA60C},
	{0xA66F,0xA672}, {0xA674,0xA67D}, {0xA67F,0xA67F}, {0xA69C,0xA69F}, {0xA6F0,0xA6F1},
	{0xA700,0xA721}, {0xA770,0xA770}, {0xA788,0xA78A}, {0xA7F1,0xA7F4}, {0xA7F8,0xA7F9},
	{0xA802,0xA802}, {0xA806,0xA806}, {0xA80B,0xA80B}, {0xA825,0xA826}, {0xA82C,0xA82C},
	{0xA8C4,0xA8C5}, {0xA8E0,0xA8F1}, {0xA8FF,0xA8FF}, {0xA926,0xA92D}, {0xA947,0xA951},
	{0xA980,0xA982}, {0xA9B3,0xA9B3}, {0xA9B6,0xA9B9}, {0xA9BC,0xA9BD}, {0xA9CF,0xA9CF},
	{0xA9E5,0xA9E6}, {0xAA29,0xAA2E}, {0xAA31,0xAA32}, {0xAA35,0xAA36}, {0xAA43,0xAA43},
	{0xAA4C,0xAA4C}, {0xAA70,0xAA70}, {0xAA7C,0xAA7C}, {0xAAB0,0xAAB0}, {0xAAB2,0xAAB4},
	{0xAAB7,0xAAB8}, {0xAABE,0xAABF}, {0xAAC1,0xAAC1}, {0xAADD,0xAADD}, {0xAAEC,0xAAED},
	{0xAAF3,0xAAF4}, {0xAAF6,0xAAF6}, {0xAB5B,0xAB5F}, {0xAB69,0xAB6B}, {0xABE5,0xABE5},
	{0xABE8,0xABE8}, {0xABED,0xABED}, {0xFB1E,0xFB1E}, {0xFBB2,0xFBC2}, {0xFE00,0xFE0F},
	{0xFE13,0xFE13}, {0xFE20,0xFE2F}, {0xFE52,0xFE52}, {0xFE55,0xFE55}, {0xFEFF,0xFEFF},
	{0xFF07,0xFF07}, {0xFF0E,0xFF0E}, {0xFF1A,0xFF1A}, {0xFF3E,0xFF3E}, {0xFF40,0xFF40},
	{0xFF70,0xFF70}, {0xFF9E,0xFF9F}, {0xFFE3,0xFFE3}, {0xFFF9,0xFFFB}, {0x101FD,0x101FD},
	{0x102E0,0x102E0}, {0x10376,0x1037A}, {0x10780,0x10785}, {0x10787,0x107B0}, {0x107B2,0x107BA},
	{0x10A01,0x10A03}, {0x10A05,0x10A06}, {0x10A0C,0x10A0F}, {0x10A38,0x10A3A}, {0x10A3F,0x10A3F},
	{0x10AE5,0x10AE6}, {0x10D24,0x10D27}, {0x10D4E,0x10D4E}, {0x10D69,0x10D6D}, {0x10D6F,0x10D6F},
	{0x10EAB,0x10EAC}, {0x10EC5,0x10EC5}, {0x10EFA,0x10EFF}, {0x10F46,0x10F50}, {0x10F82,0x10F85},
	{0x11001,0x11001}, {0x11038,0x11046}, {0x11070,0x11070}, {0x11073,0x11074}, {0x1107F,0x11081},
	{0x110B3,0x110B6}, {0x110B9,0x110BA}, {0x110BD,0x110BD}, {0x110C2,0x110C2}, {0x110CD,0x110CD},
	{0x11100,0x11102}, {0x11127,0x1112B}, {0x1112D,0x11134}, {0x11173,0x11173}, {0x11180,0x11181},
	{0x111B6,0x111BE}, {0x111C9,0x111CC}, {0x111CF,0x111CF}, {0x1122F,0x11231}, {0x11234,0x11234},
	{0x11236,0x11237}, {0x1123E,0x1123E}, {0x11241,0x11241}, {0x112DF,0x112DF}, {0x112E3,0x112EA},
	{0x11300,0x11301}, {0x1133B,0x1133C}, {0x11340,0x11340}, {0x11366,0x1136C}, {0x11370,0x11374},
	{0x113BB,0x113C0}, {0x113CE,0x113CE}, {0x113D0,0x113D0}, {0x113D2,0x113D2}, {0x113E1,0x113E2},
	{0x11438,0x1143F}, {0x11442,0x11444}, {0x11446,0x11446}, {0x1145E,0x1145E}, {0x114B3,0x114B8},
	{0x114BA,0x114BA}, {0x114BF,0x114C0}, {0x114C2,0x114C3}, {0x115B2,0x115B5}, {0x115BC,0x115BD},
	{0x115BF,0x115C0}, {0x115DC,0x115DD}, {0x11633,0x1163A}, {0x1163D,0x1163D}, {0x1163F,0x11640},
	{0x116AB,0x116AB}, {0x116AD,0x116AD}, {0x116B0,0x116B5}, {0x116B7,0x116B7}, {0x1171D,0x1171D},
	{0x1171F,0x1171F}, {0x11722,0x11725}, {0x11727,0x1172B}, {0x1182F,0x11837}, {0x11839,0x1183A},
	{0x1193B,0x1193C}, {0x1193E,0x1193E}, {0x11943,0x11943}, {0x119D4,0x119D7}, {0x119DA,0x119DB},
	{0x119E0,0x119E0}, {0x11A01,0x11A0A}, {0x11A33,0x11A38}, {0x11A3B,0x11A3E}, {0x11A47,0x11A47},
	{0x11A51,0x11A56}, {0x11A59,0x11A5B}, {0x11A8A,0x11A96}, {0x11A98,0x11A99}, {0x11B60,0x11B60},
	{0x11B62,0x11B64}, {0x11B66,0x11B66}, {0x11C30,0x11C36}, {0x11C38,0x11C3D}, {0x11C3F,0x11C3F},
	{0x11C92,0x11CA7}, {0x11CAA,0x11CB0}, {0x11CB2,0x11CB3}, {0x11CB5,0x11CB6}, {0x11D31,0x11D36},
	{0x11D3A,0x11D3A}, {0x11D3C,0x11D3D}, {0x11D3F,0x11D45}, {0x11D47,0x11D47}, {0x11D90,0x11D91},
	{0x11D95,0x11D95}, {0x11D97,0x11D97}, {0x11DD9,0x11DD9}, {0x11EF3,0x11EF4}, {0x11F00,0x11F01},
	{0x11F36,0x11F3A}, {0x11F40,0x11F40}, {0x11F42,0x11F42}, {0x11F5A,0x11F5A}, {0x13430,0x13440},
	{0x13447,0x13455}, {0x1611E,0x16129}, {0x1612D,0x1612F}, {0x16AF0,0x16AF4}, {0x16B30,0x16B36},
	{0x16B40,0x16B43}, {0x16D40,0x16D42}, {0x16D6B,0x16D6C}, {0x16F4F,0x16F4F}, {0x16F8F,0x16F9F},
	{0x16FE0,0x16FE1}, {0x16FE3,0x16FE4}, {0x16FF2,0x16FF3}, {0x1AFF0,0x1AFF3}, {0x1AFF5,0x1AFFB},
	{0x1AFFD,0x1AFFE}, {0x1BC9D,0x1BC9E}, {0x1BCA0,0x1BCA3}, {0x1CF00,0x1CF2D}, {0x1CF30,0x1CF46},
	{0x1D167,0x1D169}, {0x1D173,0x1D182}, {0x1D185,0x1D18B}, {0x1D1AA,0x1D1AD}, {0x1D242,0x1D244},
	{0x1DA00,0x1DA36}, {0x1DA3B,0x1DA6C}, {0x1DA75,0x1DA75}, {0x1DA84,0x1DA84}, {0x1DA9B,0x1DA9F},
	{0x1DAA1,0x1DAAF}, {0x1E000,0x1E006}, {0x1E008,0x1E018}, {0x1E01B,0x1E021}, {0x1E023,0x1E024},
	{0x1E026,0x1E02A}, {0x1E030,0x1E06D}, {0x1E08F,0x1E08F}, {0x1E130,0x1E13D}, {0x1E2AE,0x1E2AE},
	{0x1E2EC,0x1E2EF}, {0x1E4EB,0x1E4EF}, {0x1E5EE,0x1E5EF}, {0x1E6E3,0x1E6E3}, {0x1E6E6,0x1E6E6},
	{0x1E6EE,0x1E6EF}, {0x1E6F5,0x1E6F5}, {0x1E6FF,0x1E6FF}, {0x1E8D0,0x1E8D6}, {0x1E944,0x1E94B},
	{0x1F3FB,0x1F3FF}, {0xE0001,0xE0001}, {0xE0020,0xE007F}, {0xE0100,0xE01EF},
};

/* Binary search a case table for the row covering cp, honouring its step. */
static const mb_case_range * MbCaseFind(const mb_case_range *aTab,sxu32 nTab,sxu32 cp)
{
	sxu32 iLo = 0,iHi = nTab;
	while( iLo < iHi ){
		sxu32 iMid = iLo + (iHi - iLo) / 2;
		if( cp < aTab[iMid].iFirst ){
			iHi = iMid;
		}else if( cp > aTab[iMid].iLast ){
			iLo = iMid + 1;
		}else{
			return ((cp - aTab[iMid].iFirst) % aTab[iMid].iStep) == 0 ? &aTab[iMid] : 0;
		}
	}
	return 0;
}
static sxu32 MbMapSimple(const mb_case_range *aTab,sxu32 nTab,sxu32 cp)
{
	const mb_case_range *pRow = MbCaseFind(aTab,nTab,cp);
	return pRow ? (sxu32)((sxi64)cp + pRow->iDelta) : cp;
}
/* The multi-character mapping for cp, or 0 when it has none. */
static const sxu32 * MbMapFull(const mb_case_full *aTab,sxu32 nTab,sxu32 cp)
{
	sxu32 iLo = 0,iHi = nTab;
	while( iLo < iHi ){
		sxu32 iMid = iLo + (iHi - iLo) / 2;
		if( cp < aTab[iMid].iCp ){
			iHi = iMid;
		}else if( cp > aTab[iMid].iCp ){
			iLo = iMid + 1;
		}else{
			return aTab[iMid].aTo;
		}
	}
	return 0;
}
static int MbInPairs(const sxu32 (*aTab)[2],sxu32 nTab,sxu32 cp)
{
	sxu32 iLo = 0,iHi = nTab;
	while( iLo < iHi ){
		sxu32 iMid = iLo + (iHi - iLo) / 2;
		if( cp < aTab[iMid][0] ){
			iHi = iMid;
		}else if( cp > aTab[iMid][1] ){
			iLo = iMid + 1;
		}else{
			return 1;
		}
	}
	return 0;
}
static sxu32 MbToLower(sxu32 c){ return MbMapSimple(aMbLower,SX_ARRAYSIZE(aMbLower),c); }
static sxu32 MbToUpper(sxu32 c){ return MbMapSimple(aMbUpper,SX_ARRAYSIZE(aMbUpper),c); }
static sxu32 MbToTitle(sxu32 c){ return MbMapSimple(aMbTitle,SX_ARRAYSIZE(aMbTitle),c); }
/* Does this character CARRY case? It is what a title-case word boundary is made
 * of: a word starts at a cased character whose predecessor was neither cased nor
 * case-ignorable, which is why "a1b" titles to "A1B" (a digit carries no case
 * and ends the word) while "o'neil" titles to "O'neil" (an apostrophe is
 * ignorable and the word runs on through it). */
static int MbIsCased(sxu32 c){ return MbInPairs(aMbCased,SX_ARRAYSIZE(aMbCased),c); }
static int MbIsCaseIgnorable(sxu32 c){ return MbInPairs(aMbIgnorable,SX_ARRAYSIZE(aMbIgnorable),c); }
/* The code a case-INSENSITIVE comparison sees: php folds through the simple
 * upper mapping and back down, so ς, σ and Σ are one character and the Kelvin
 * sign is a plain k — but ı is NOT an i (its upper case is I, whose lower case
 * is i, and php still keeps the two apart), and a character whose case mapping
 * expands (ß) folds to itself because a simple mapping is all this asks for. */
static sxu32 MbFoldCode(sxu32 c)
{
	if( c == 0x0131 ){
		return c;   /* U+0131 LATIN SMALL LETTER DOTLESS I */
	}
	return MbToLower(MbToUpper(c));
}

/* --- Encodings and the character walk ---------------------------------- */

/*
 * The three encodings PHL models (the §10 scope cut — php's full encoding zoo
 * is out; a php-VALID name PHL does not model, e.g. SJIS, raises the same
 * ValueError php uses for a truly invalid name). They differ in exactly two
 * ways, and both matter to every function here: how many BYTES a character
 * takes, and which byte sequences are characters at all.
 *
 *   UTF-8    1..4 bytes per character; an ill-formed run is php's error
 *            character, which mbstring counts as one and re-encodes as '?'.
 *   LATIN1   one byte per character, and its VALUE is the code point — which
 *            is what makes 8bit/binary/ISO-8859-1 one encoding here: php's
 *            answers for the three are byte-identical on every function below.
 *   ASCII    one byte per character; a byte over 0x7F is an error character.
 */
#define MB_ENC_UTF8    0
#define MB_ENC_LATIN1  1
#define MB_ENC_ASCII   2
/* The code an error character carries. Not a code point (php's own marker is
 * not one either), so it compares equal to another error character and to
 * nothing else — a literal '?' in the haystack is NOT a match for an
 * undecodable needle, which is what folding through '?' used to make it. */
#define MB_BAD_CODE  0xFFFFFFFFu

/* Resolve an encoding name to an MB_ENC_* id, or -1 when it is outside PHL's
 * modelled set. Surrounding ASCII whitespace is trimmed (php accepts " UTF-8"). */
static int MbConvEncId(const char *z,int n)
{
	while( n > 0 && (z[0]==' '||z[0]=='\t'||z[0]=='\n'||z[0]=='\r') ){ z++; n--; }
	while( n > 0 && (z[n-1]==' '||z[n-1]=='\t'||z[n-1]=='\n'||z[n-1]=='\r') ){ n--; }
	if( (n==5 && SyStrnicmp(z,"UTF-8",5)==0) || (n==4 && SyStrnicmp(z,"UTF8",4)==0) ){
		return MB_ENC_UTF8;
	}
	if( (n==10 && SyStrnicmp(z,"ISO-8859-1",10)==0) || (n==9 && SyStrnicmp(z,"ISO8859-1",9)==0)
	 || (n==6 && SyStrnicmp(z,"latin1",6)==0) || (n==4 && SyStrnicmp(z,"8bit",4)==0)
	 || (n==6 && SyStrnicmp(z,"binary",6)==0) ){
		return MB_ENC_LATIN1;
	}
	if( (n==5 && SyStrnicmp(z,"ASCII",5)==0) || (n==8 && SyStrnicmp(z,"US-ASCII",8)==0) ){
		return MB_ENC_ASCII;
	}
	return -1;
}
/* Validate the optional $encoding argument: an MB_ENC_* id, or -1 after raising
 * php's ValueError. A missing/null argument is php's internal encoding, which
 * PHL fixes at UTF-8. */
static int MbEncodingArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNo)
{
	const char *zEnc;
	int nEnc,iEnc;
	if( pArg == 0 || ph7_value_is_null(pArg) ){
		return MB_ENC_UTF8;
	}
	zEnc = ph7_value_to_string(pArg,&nEnc);
	iEnc = MbConvEncId(zEnc,nEnc);
	if( iEnc < 0 ){
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($encoding) must be a valid encoding, \"%.*s\" given",
			zFunc,iArgNo,nEnc,zEnc);
		return -1;
	}
	return iEnc;
}
/* Decode the character at z[0..n-1] under iEnc: its code (a code point, or
 * MB_BAD_CODE for an error character) with *pLen set to the bytes it spans. */
static sxu32 MbNextCode(const unsigned char *z,sxu32 n,int iEnc,sxu32 *pLen)
{
	sxi32 iCp;
	if( iEnc != MB_ENC_UTF8 ){
		*pLen = 1;
		return (iEnc == MB_ENC_ASCII && z[0] > 0x7F) ? MB_BAD_CODE : (sxu32)z[0];
	}
	iCp = MbUtf8Decode(z,n,pLen);
	return iCp < 0 ? MB_BAD_CODE : (sxu32)iCp;
}
/* Character count of zIn[0..nByte-1] under iEnc */
static sxu32 MbStrlen(const char *zIn,sxu32 nByte,int iEnc)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nCp = 0,nLen;
	if( iEnc != MB_ENC_UTF8 ){
		return nByte;   /* one byte per character */
	}
	while( i < nByte ){
		MbUtf8Decode(&z[i],nByte - i,&nLen);
		i += nLen;
		nCp++;
	}
	return nCp;
}
/* Byte offset of character index iCp (clamped to the buffer end) */
static sxu32 MbSkip(const char *zIn,sxu32 nByte,sxu32 iCp,int iEnc)
{
	if( iEnc != MB_ENC_UTF8 ){
		return iCp < nByte ? iCp : nByte;
	}
	return MbUtf8Skip(zIn,nByte,iCp);
}
/*
 * A decoded string: one code per character plus the byte offset each one starts
 * at (nChar+1 entries, so the last is the buffer length). php works the same
 * way — mbstring converts to a wchar buffer and operates there — and it is what
 * lets a search answer in CHARACTERS while slicing in BYTES. The cost is two
 * words per input byte, which is why the buffer is capped well inside what the
 * allocator's byte count can express.
 */
#define MB_TEXT_MAX  0x0FFFFFFFu
typedef struct mb_text mb_text;
struct mb_text {
	const char *zIn;   /* the source buffer (not owned) */
	sxu32 nByte;
	sxu32 *aCode;      /* nChar codes */
	sxu32 *aOfft;      /* nChar+1 byte offsets */
	sxu32 nChar;
};
/* Decode zIn under iEnc into pText, lower-casing every code when bFold is set
 * (which is what makes an error character equal to any other one: they share
 * MB_BAD_CODE, and folding is the only mode php compares them loosely in). */
static int MbTextDecode(ph7_context *pCtx,mb_text *pText,const char *zIn,sxu32 nByte,
	int iEnc,int bFold)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,n = 0,nLen,nSlot;
	pText->zIn = zIn;
	pText->nByte = nByte;
	pText->nChar = 0;
	if( nByte > MB_TEXT_MAX ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* One slot per byte is the upper bound on the character count */
	nSlot = nByte + 1;
	pText->aCode = (sxu32 *)ph7_context_alloc_chunk(pCtx,
		(unsigned int)(nSlot * 2 * sizeof(sxu32)),FALSE,TRUE);
	if( pText->aCode == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pText->aOfft = &pText->aCode[nSlot];
	while( i < nByte ){
		sxu32 cp = MbNextCode(&z[i],nByte - i,iEnc,&nLen);
		if( bFold && cp != MB_BAD_CODE ){
			cp = MbFoldCode(cp);
		}
		pText->aCode[n] = cp;
		pText->aOfft[n] = i;
		i += nLen;
		n++;
	}
	pText->aOfft[n] = nByte;
	pText->nChar = n;
	return PH7_OK;
}
/* Does pN occur in pH starting at character i? bExactBad compares the raw bytes
 * of an error character rather than taking every one for equal, which is php's
 * rule for a case-SENSITIVE UTF-8 search: mb_strpos("a\xffb","\xfe") is false
 * there, where the same pair matches under mb_stripos (and under ASCII, whose
 * error character carries nothing to tell apart). */
static int MbTextMatchAt(const mb_text *pH,sxu32 i,const mb_text *pN,int bExactBad)
{
	sxu32 k;
	for( k = 0 ; k < pN->nChar ; ++k ){
		if( pH->aCode[i+k] != pN->aCode[k] ){
			return 0;
		}
		if( bExactBad && pH->aCode[i+k] == MB_BAD_CODE ){
			sxu32 nH = pH->aOfft[i+k+1] - pH->aOfft[i+k];
			sxu32 nN = pN->aOfft[k+1] - pN->aOfft[k];
			if( nH != nN || SyMemcmp(&pH->zIn[pH->aOfft[i+k]],&pN->zIn[pN->aOfft[k]],nH) != 0 ){
				return 0;
			}
		}
	}
	return 1;
}

/* --- The functions ----------------------------------------------------- */

/* int mb_strlen(string $string, ?string $encoding = null) */
static int PH7_builtin_mb_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,iEnc;
	if( nArg < 1 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_strlen",2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	ph7_result_int64(pCtx,(ph7_int64)MbStrlen(zIn,(sxu32)nByte,iEnc));
	return PH7_OK;
}
/* Set the call result to zIn[0..nByte-1] with every ill-formed run replaced by
 * '?', php's substitution character. A well-formed buffer copies verbatim. */
static void MbBlobSubstituted(SyBlob *pOut,const char *zIn,sxu32 nByte)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nLen;
	while( i < nByte ){
		if( MbUtf8Decode(&z[i],nByte - i,&nLen) < 0 ){
			SyBlobAppend(pOut,"?",1);
		}else{
			SyBlobAppend(pOut,&z[i],nLen);
		}
		i += nLen;
	}
}
static void MbResultSubstituted(ph7_context *pCtx,const char *zIn,sxu32 nByte)
{
	const unsigned char *z = (const unsigned char *)zIn;
	SyBlob sOut;
	sxu32 i = 0,nLen;
	/* Well-formed is the overwhelmingly common case: check first and hand back
	 * the buffer as it stands rather than rebuilding it. */
	while( i < nByte && MbUtf8Decode(&z[i],nByte - i,&nLen) >= 0 ){
		i += nLen;
	}
	if( i >= nByte ){
		ph7_result_string(pCtx,zIn,(int)nByte);
		return;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	MbBlobSubstituted(&sOut,zIn,nByte);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
}
/* string mb_substr(string $string, int $start, ?int $length = null,
 *                  ?string $encoding = null) */
static int PH7_builtin_mb_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,iEnc;
	sxi64 iStart,iLen;
	sxu32 nCp,iOfft,iEnd;
	int bLenSet = 0;
	if( nArg < 2 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,"mb_substr",4);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	iStart = ph7_value_to_int64(apArg[1]);
	iLen = 0;
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		iLen = ph7_value_to_int64(apArg[2]);
		bLenSet = 1;
	}
	nCp = MbStrlen(zIn,(sxu32)nByte,iEnc);
	if( iStart < 0 ){
		iStart = (sxi64)nCp + iStart;
		if( iStart < 0 ){ iStart = 0; }
	}
	if( iStart >= (sxi64)nCp ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( !bLenSet ){
		iLen = (sxi64)nCp - iStart;
	}else if( iLen < 0 ){
		iLen = ((sxi64)nCp - iStart) + iLen;
		if( iLen < 0 ){ iLen = 0; }
	}
	if( iStart + iLen > (sxi64)nCp ){
		iLen = (sxi64)nCp - iStart;
	}
	if( iEnc != MB_ENC_UTF8 ){
		/* One byte per character: the slice is the byte range, handed back as it
		 * stands (php substitutes nothing here — mb_substr("\xff",0,1,"ASCII")
		 * is the raw byte, error character or not). */
		ph7_result_string(pCtx,&zIn[iStart],(int)iLen);
		return PH7_OK;
	}
	iOfft = MbUtf8Skip(zIn,(sxu32)nByte,(sxu32)iStart);
	iEnd  = iOfft + MbUtf8Skip(&zIn[iOfft],(sxu32)nByte - iOfft,(sxu32)iLen);
	/* php decodes and re-encodes the slice rather than copying its bytes, so an
	 * undecodable run inside it comes out as '?' — mb_substr("ab\xffcd",2,1) is
	 * "?", not the raw \xff PHL used to hand back. */
	MbResultSubstituted(pCtx,&zIn[iOfft],iEnd - iOfft);
	return PH7_OK;
}
/* Append one code in iEnc's encoding. A code the target cannot hold — U+0178,
 * which is what upper-casing 0xFF produces, in a one-byte encoding — is php's
 * substitute character, the same '?' an error character gets. */
static void MbAppendCode(SyBlob *pOut,sxu32 cp,int iEnc)
{
	unsigned char zEnc[4];
	if( iEnc == MB_ENC_UTF8 ){
		SyBlobAppend(pOut,zEnc,MbUtf8Encode(cp,zEnc));
		return;
	}
	zEnc[0] = (unsigned char)((cp <= (iEnc == MB_ENC_ASCII ? 0x7Fu : 0xFFu)) ? cp : '?');
	SyBlobAppend(pOut,zEnc,1);
}
/* Is there a cased character after byte i, reading past the case-ignorable ones?
 * Unicode's Final_Sigma condition asks that of both sides of a Σ. */
static int MbCasedFollows(const unsigned char *z,sxu32 i,sxu32 nByte,int iEnc)
{
	while( i < nByte ){
		sxu32 nLen,cp = MbNextCode(&z[i],nByte - i,iEnc,&nLen);
		i += nLen;
		if( cp == MB_BAD_CODE ){
			return 0;   /* an error character carries no case */
		}
		if( MbIsCaseIgnorable(cp) ){
			continue;
		}
		return MbIsCased(cp);
	}
	return 0;
}
/* Case-map one character into pOut. iMode 0 = lower, 1 = upper, 2 = title;
 * bFinalSigma picks ς over σ for a Σ that ends a word. */
static void MbMapOne(SyBlob *pOut,sxu32 cp,int iMode,int iEnc,int bFinalSigma)
{
	const sxu32 *aFull;
	if( bFinalSigma && cp == 0x03A3 ){
		MbAppendCode(pOut,0x03C2,iEnc);
		return;
	}
	aFull = (iMode == 1) ? MbMapFull(aMbUpperFull,SX_ARRAYSIZE(aMbUpperFull),cp)
		: ((iMode == 0) ? MbMapFull(aMbLowerFull,SX_ARRAYSIZE(aMbLowerFull),cp)
		: MbMapFull(aMbTitleFull,SX_ARRAYSIZE(aMbTitleFull),cp));
	if( aFull ){
		/* php's full mapping: ß upper-cases to SS and title-cases to Ss */
		int k;
		for( k = 0 ; k < 3 && aFull[k] ; ++k ){
			MbAppendCode(pOut,aFull[k],iEnc);
		}
		return;
	}
	MbAppendCode(pOut,(iMode == 1) ? MbToUpper(cp) : ((iMode == 0) ? MbToLower(cp) : MbToTitle(cp)),iEnc);
}
/*
 * Shared case transform: iMode 0 = lower, 1 = upper, 2 = title.
 *
 * TITLE mode is Unicode's, which is not "upper-case after a non-letter": a word
 * starts at a CASED character whose predecessor was neither cased nor
 * case-ignorable. That is what makes `a1b` title to `A1B` (a digit carries no
 * case, so it ends the word), `o'neil` to `O'neil` (an apostrophe is ignorable
 * and the word runs through it), and `a日b` to `A日B` (an uncased letter ends the
 * word just as a digit does). Uncased characters pass through untouched.
 *
 * LOWER mode carries Unicode's Final_Sigma condition, which needs both sides:
 * a Σ lowers to ς only when a cased character precedes it and none follows —
 * `ΑΣ` is `ας` but a lone `Σ` is `σ`, and looking only forward (which is what
 * this used to do) got the lone one wrong.
 */
static int MbCaseTransform(ph7_context *pCtx,const char *zIn,sxu32 nByte,int iMode,int iEnc)
{
	SyBlob sOut;
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nLen,cp;
	int bWordStart = 1,bPrevCased = 0;
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	while( i < nByte ){
		cp = MbNextCode(&z[i],nByte - i,iEnc,&nLen);
		i += nLen;
		if( cp == MB_BAD_CODE ){
			/* php substitutes '?' for an error character and leaves the word
			 * boundary exactly as it found it, the way a case-ignorable
			 * character does: "\xffab" titles to "?Ab" (the run did not open the
			 * word, 'a' still does) and "a\xffb" to "A?b" ('b' is still
			 * mid-word). Reading FORWARD it is not ignorable — it ends the scan
			 * for a following cased character, so `ΑΣ\xffΑ` still lowers its
			 * sigma to the final form. */
			SyBlobAppend(&sOut,"?",1);
			continue;
		}
		/* Every character is mapped — an uncased one simply has no mapping to
		 * apply, and a case-ignorable one that HAS a mapping still takes it
		 * (U+0345 title-cases to iota); what ignorable means is that the word
		 * boundary is left exactly as it was found. */
		MbMapOne(&sOut,cp,(iMode == 2) ? (bWordStart ? 2 : 0) : iMode,iEnc,
			(iMode != 1) && bPrevCased && !MbCasedFollows(z,i,nByte,iEnc));
		if( !MbIsCaseIgnorable(cp) ){
			bPrevCased = MbIsCased(cp);
			bWordStart = !bPrevCased;
		}
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* string mb_ucfirst/mb_lcfirst(string $string, ?string $encoding = null) — php 8.4 */
static int PH7_builtin_mb_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zFunc;
	int nByte,iEnc;
	sxu32 cp,nLen;
	SyBlob sOut;
	if( nArg < 1 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zFunc = ph7_function_name(pCtx);
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,zFunc,2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	cp = MbNextCode((const unsigned char *)zIn,(sxu32)nByte,iEnc,&nLen);
	if( cp == MB_BAD_CODE && iEnc == MB_ENC_UTF8 ){
		/* An error character carries no case, so nothing changes and php hands
		 * back the string it was given, bytes and all. (Under ASCII the same
		 * character is not one php can write, so it substitutes — that is php's
		 * own difference between the two, not a shortcut here.) */
		ph7_result_string(pCtx,zIn,nByte);
		return PH7_OK;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	if( cp == MB_BAD_CODE ){
		SyBlobAppend(&sOut,"?",1);
	}else{
		/* mb_Ucfirst TITLE-cases (php: 'ß' becomes 'Ss', where upper-casing it is
		 * 'SS'); mb_lcfirst lowers, and a leading Σ is never a FINAL sigma
		 * because nothing precedes it. */
		MbMapOne(&sOut,cp,zFunc[3] == 'u' ? 2 : 0,iEnc,0);
	}
	if( SyBlobLength(&sOut) == nLen
	 && SyMemcmp(SyBlobData(&sOut),zIn,nLen) == 0 ){
		/* The first character had nothing to change, so php returns the ORIGINAL
		 * string — which is what keeps an ill-formed run further along from being
		 * substituted by a call that did no work. */
		SyBlobRelease(&sOut);
		ph7_result_string(pCtx,zIn,nByte);
		return PH7_OK;
	}
	if( iEnc == MB_ENC_UTF8 ){
		/* The rest rides through the same decode/re-encode a slice takes, so an
		 * ill-formed run in it becomes '?' — mb_ucfirst("a\xffb") is "A?b". */
		SyBlob sTail;
		SyBlobInit(&sTail,&pCtx->pVm->sAllocator);
		MbBlobSubstituted(&sTail,&zIn[nLen],(sxu32)nByte - nLen);
		SyBlobAppend(&sOut,SyBlobData(&sTail),SyBlobLength(&sTail));
		SyBlobRelease(&sTail);
	}else{
		SyBlobAppend(&sOut,&zIn[nLen],(sxu32)nByte - nLen);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* string mb_strtolower/mb_strtoupper(string $string, ?string $encoding) */
static int PH7_builtin_mb_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zFunc;
	int nByte,iEnc;
	if( nArg < 1 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zFunc = ph7_function_name(pCtx);
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,zFunc,2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	return MbCaseTransform(pCtx,zIn,(sxu32)nByte,
		zFunc[sizeof("mb_strto")-1] == 'u' ? 1 : 0,iEnc); /* mb_strtoUpper */
}
/* string mb_convert_case(string $string, int $mode, ?string $encoding) */
static int PH7_builtin_mb_convert_case(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,iMode,iEnc;
	if( nArg < 2 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_convert_case",3);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	iMode = ph7_value_to_int(apArg[1]);
	if( iMode < 0 || iMode > 2 ){
		/* php has FOLD/SIMPLE variants 3-7; PHL's recorded scope is 0-2 */
		return PH7_VmThrowException(pCtx,"ValueError",
			"mb_convert_case(): Argument #2 ($mode) must be one of the MB_CASE_* constants");
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	/* php: MB_CASE_UPPER=0, MB_CASE_LOWER=1, MB_CASE_TITLE=2 */
	return MbCaseTransform(pCtx,zIn,(sxu32)nByte,
		iMode == 0 ? 1 : (iMode == 1 ? 0 : 2),iEnc);
}
/* Shared search core: returns the character index of a match, or -1.
 *
 * iOfftCp is the LOWEST character index a match may start at; iMaxCp the highest,
 * or -1 for no upper bound. The pair is php's asymmetric strrpos rule (a negative
 * $offset is an upper bound counted back from the end, a non-negative one a lower
 * bound) — the same split StrRSearchWindow() applies to the 8-bit family. */
static sxi64 MbTextSearch(const mb_text *pH,const mb_text *pN,
	sxi64 iOfftCp,sxi64 iMaxCp,int bExactBad,int bReverse)
{
	sxi64 iFound = -1;
	sxu32 i,iFrom;
	if( pN->nChar == 0 || pN->nChar > pH->nChar ){
		return -1;
	}
	iFrom = (sxu32)(iOfftCp > 0 ? iOfftCp : 0);
	for( i = iFrom ; i + pN->nChar <= pH->nChar ; ++i ){
		if( iMaxCp >= 0 && (sxi64)i > iMaxCp ){
			break; /* past the window's upper bound; nothing later qualifies */
		}
		if( MbTextMatchAt(pH,i,pN,bExactBad) ){
			iFound = (sxi64)i;
			if( !bReverse ){
				break;
			}
			/* keep scanning for the last hit */
		}
	}
	return iFound;
}
/* mb_strpos / mb_stripos / mb_strrpos / mb_strripos */
static int PH7_builtin_mb_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zH,*zN,*zFunc;
	mb_text sH,sN;
	int nH,nN,iEnc,rc;
	sxi64 iOfft = 0,iMax = -1,iPos,nCp;
	int bFold,bRev;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFunc = ph7_function_name(pCtx);
	/* "mb_str" + "pos" / "ipos" / "rpos" / "ripos" */
	bRev  = zFunc[sizeof("mb_str")-1] == 'r';
	bFold = zFunc[sizeof("mb_str")-1+(bRev?1:0)] == 'i';
	iEnc = MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,zFunc,4);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zH = ph7_value_to_string(apArg[0],&nH);
	zN = ph7_value_to_string(apArg[1],&nN);
	nCp = (sxi64)MbStrlen(zH,(sxu32)nH,iEnc);
	if( nArg > 2 ){
		iOfft = ph7_value_to_int64(apArg[2]);
		/* php requires -strlen <= $offset <= strlen, in CODE POINTS here, and
		 * raises rather than answering "not found" — the same rule the 8-bit
		 * family got, which these three were left out of: mb_strpos("abc","c",7)
		 * answered false, and false is what a genuine miss answers too. */
		if( iOfft < 0 ? (iOfft < -nCp) : (iOfft > nCp) ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",
				zFunc);
		}
		if( bRev ){
			/* A negative offset is an UPPER bound on where the match may START,
			 * not a start position: mb_strrpos("áéíóú","í",-1) is 2 in php, where
			 * counting it forward answered false — and -5 is php's false, where
			 * counting it forward answered 2. */
			if( iOfft < 0 ){
				iMax = nCp + iOfft;
				iOfft = 0;
			}
		}else if( iOfft < 0 ){
			iOfft = nCp + iOfft;
		}
	}
	if( nN == 0 ){
		/* php 8 matches an EMPTY needle at the offset itself (and, searching
		 * backwards, at the last position the window allows) — `mb_strpos("abc","")`
		 * is 0 and `mb_strrpos("abc","")` is 3. These three answered false, which is
		 * also what a genuine miss answers; the 8-bit family already had the rule. */
		iPos = bRev ? (iMax >= 0 ? iMax : nCp) : iOfft;
		if( iPos > nCp ){
			iPos = nCp;
		}
		if( iPos < iOfft ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ph7_result_int64(pCtx,iPos);
		return PH7_OK;
	}
	SyZero(&sH,sizeof(sH));
	SyZero(&sN,sizeof(sN));
	rc = MbTextDecode(pCtx,&sH,zH,(sxu32)nH,iEnc,bFold);
	if( rc == PH7_OK ){
		rc = MbTextDecode(pCtx,&sN,zN,(sxu32)nN,iEnc,bFold);
	}
	if( rc != PH7_OK ){
		return rc;   /* the decode already raised; do not raise a second time */
	}
	iPos = MbTextSearch(&sH,&sN,iOfft,iMax,iEnc == MB_ENC_UTF8 && !bFold,bRev);
	if( iPos < 0 ){
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_int64(pCtx,iPos);
	}
	return PH7_OK;
}
/* Hand back pText's characters [iFrom,iTo) as the call result: the byte range
 * they occupy, decoded and re-encoded under UTF-8 (so an ill-formed run inside
 * it becomes '?', the rule every UTF-8 slice here follows) and copied verbatim
 * under a one-byte encoding, where a slice is a byte range and nothing else. */
static void MbResultSlice(ph7_context *pCtx,const mb_text *pText,sxu32 iFrom,sxu32 iTo,int iEnc)
{
	sxu32 iOfft = pText->aOfft[iFrom],iEnd = pText->aOfft[iTo];
	if( iEnc != MB_ENC_UTF8 ){
		ph7_result_string(pCtx,&pText->zIn[iOfft],(int)(iEnd - iOfft));
		return;
	}
	MbResultSubstituted(pCtx,&pText->zIn[iOfft],iEnd - iOfft);
}
/*
 * string|false mb_strstr / mb_stristr / mb_strrchr / mb_strrichr(
 *     string $haystack, string $needle, bool $before_needle = false,
 *     ?string $encoding = null)
 *
 * The four are one routine over two flags, as php has them: fold the case or
 * not, take the FIRST match or the LAST. php's mb_strrchr is not the 8-bit
 * strrchr — it searches for the whole needle, not for its first character —
 * and an EMPTY needle matches at the position the direction starts from, so
 * mb_strstr("abc","") is "abc" and mb_strrchr("abc","") is "".
 */
static int PH7_builtin_mb_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zH,*zN,*zFunc;
	mb_text sH,sN;
	int nH,nN,iEnc,rc,bBefore = 0,bFold,bRev;
	sxi64 iPos;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFunc = ph7_function_name(pCtx);
	/* "mb_str" + "str" / "istr" / "rchr" / "richr" */
	bRev  = zFunc[sizeof("mb_str")-1] == 'r';
	bFold = zFunc[sizeof("mb_str")-1+(bRev?1:0)] == 'i';
	iEnc = MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,zFunc,4);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	if( nArg > 2 ){
		bBefore = ph7_value_to_bool(apArg[2]);
	}
	zH = ph7_value_to_string(apArg[0],&nH);
	zN = ph7_value_to_string(apArg[1],&nN);
	SyZero(&sH,sizeof(sH));
	SyZero(&sN,sizeof(sN));
	rc = MbTextDecode(pCtx,&sH,zH,(sxu32)nH,iEnc,bFold);
	if( rc == PH7_OK ){
		rc = MbTextDecode(pCtx,&sN,zN,(sxu32)nN,iEnc,bFold);
	}
	if( rc != PH7_OK ){
		return rc;
	}
	if( sN.nChar == 0 ){
		iPos = bRev ? (sxi64)sH.nChar : 0;
	}else{
		iPos = MbTextSearch(&sH,&sN,0,-1,iEnc == MB_ENC_UTF8 && !bFold,bRev);
	}
	if( iPos < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( bBefore ){
		MbResultSlice(pCtx,&sH,0,(sxu32)iPos,iEnc);
	}else{
		MbResultSlice(pCtx,&sH,(sxu32)iPos,sH.nChar,iEnc);
	}
	return PH7_OK;
}
/*
 * int mb_substr_count(string $haystack, string $needle, ?string $encoding = null)
 *
 * Non-overlapping, like the 8-bit substr_count: a match consumes its own
 * characters, so "aaa" contains ONE "aa". An empty needle is php's ValueError
 * rather than an infinite answer. Error characters compare equal to each other
 * here whatever the encoding — this is the one search php does not tell two
 * ill-formed runs apart in.
 */
static int PH7_builtin_mb_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zH,*zN;
	mb_text sH,sN;
	int nH,nN,iEnc,rc;
	sxu32 i;
	ph7_int64 nCount = 0;
	if( nArg < 2 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_substr_count",3);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zH = ph7_value_to_string(apArg[0],&nH);
	zN = ph7_value_to_string(apArg[1],&nN);
	if( nN < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"mb_substr_count(): Argument #2 ($needle) must not be empty");
	}
	SyZero(&sH,sizeof(sH));
	SyZero(&sN,sizeof(sN));
	rc = MbTextDecode(pCtx,&sH,zH,(sxu32)nH,iEnc,0);
	if( rc == PH7_OK ){
		rc = MbTextDecode(pCtx,&sN,zN,(sxu32)nN,iEnc,0);
	}
	if( rc != PH7_OK ){
		return rc;
	}
	for( i = 0 ; sN.nChar > 0 && i + sN.nChar <= sH.nChar ; ){
		if( MbTextMatchAt(&sH,i,&sN,0) ){
			nCount++;
			i += sN.nChar;
		}else{
			i++;
		}
	}
	ph7_result_int64(pCtx,nCount);
	return PH7_OK;
}
/* array mb_str_split(string $string, int $length = 1, ?string $encoding) */
static int PH7_builtin_mb_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,iEnc;
	sxi64 iChunk = 1;
	ph7_value *pArr,*pV;
	sxu32 i;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_str_split",3);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	if( nArg > 1 ){
		iChunk = ph7_value_to_int64(apArg[1]);
		if( iChunk < 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"mb_str_split(): Argument #2 ($length) must be greater than 0");
		}
		if( iChunk >= 0x40000000 ){
			/* php's own ceiling, and the reason it is not just "clamp to the
			 * string": the chunk count is what it allocates for. Without it the
			 * count was TRUNCATED into 32 bits here, so a $length of 2^32+1
			 * split into single characters. */
			return PH7_VmThrowException(pCtx,"ValueError",
				"mb_str_split(): Argument #2 ($length) is too large");
		}
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	pArr = ph7_context_new_array(pCtx);
	pV = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pV == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	for( i = 0 ; i < (sxu32)nByte ; ){
		sxu32 iEnd = i + MbSkip(&zIn[i],(sxu32)nByte - i,(sxu32)iChunk,iEnc);
		ph7_value_string(pV,&zIn[i],(int)(iEnd - i));
		ph7_array_add_elem(pArr,0,pV);
		ph7_value_reset_string_cursor(pV);
		i = iEnd;
	}
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/* --- mb_trim / mb_ltrim / mb_rtrim (php 8.4) --------------------------- */

#define MB_TRIM_LEFT  1
#define MB_TRIM_RIGHT 2

/*
 * The character set a trim walks against. php builds a hash of code points;
 * this splits it in two so the common case costs nothing: a 256-bit map for
 * everything below U+0100 (which is where a hand-written trim set almost
 * always lives) and a linear array for the rest, whose length is the number of
 * DISTINCT high code points in $characters. php's own fast path is a linear
 * scan of up to four, so the shape is not a departure.
 */
typedef struct mb_trim_set mb_trim_set;
struct mb_trim_set {
	unsigned char aLow[32];   /* bitmap of U+0000 .. U+00FF */
	sxu32 *aHigh;             /* the rest, in encounter order */
	sxu32 nHigh;
	sxu32 nAlloc;
};
static int MbTrimSetAdd(ph7_context *pCtx,mb_trim_set *pSet,sxu32 cp)
{
	sxu32 i;
	if( cp < 256 ){
		pSet->aLow[cp >> 3] |= (unsigned char)(1 << (cp & 7));
		return PH7_OK;
	}
	for( i = 0 ; i < pSet->nHigh ; ++i ){
		if( pSet->aHigh[i] == cp ){
			return PH7_OK;
		}
	}
	if( pSet->nHigh >= pSet->nAlloc ){
		sxu32 nNew = pSet->nAlloc ? pSet->nAlloc * 2 : 16;
		sxu32 *aNew = (sxu32 *)ph7_context_alloc_chunk(pCtx,
			(unsigned int)(nNew * sizeof(sxu32)),FALSE,TRUE);
		if( aNew == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		if( pSet->nHigh > 0 ){
			SyMemcpy(pSet->aHigh,aNew,pSet->nHigh * (sxu32)sizeof(sxu32));
		}
		pSet->aHigh = aNew;
		pSet->nAlloc = nNew;
	}
	pSet->aHigh[pSet->nHigh++] = cp;
	return PH7_OK;
}
static int MbTrimSetHas(const mb_trim_set *pSet,sxu32 cp)
{
	sxu32 i;
	if( cp < 256 ){
		return (pSet->aLow[cp >> 3] & (1 << (cp & 7))) != 0;
	}
	for( i = 0 ; i < pSet->nHigh ; ++i ){
		if( pSet->aHigh[i] == cp ){
			return 1;
		}
	}
	return 0;
}
/*
 * string mb_trim(string $string, ?string $characters = null, ?string $encoding = null)
 * string mb_ltrim(...) / string mb_rtrim(...)
 *  Strip whole CHARACTERS -- there is no `a..z` range syntax here, unlike
 *  trim() -- from one or both ends, defaulting to php's Unicode whitespace set.
 *  Where a chunk implementation compared the encoded bytes, this decodes: an
 *  ill-formed run is ONE character that compares equal to every other
 *  ill-formed run, which is what makes mb_trim("\xff\xfeab\xff", "\xff")
 *  answer "ab" rather than leaving the bytes it could not read in place.
 */
static int PH7_builtin_mb_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	/* php's trim_default_chars[], in its own order (mb_trim_default_chars()) */
	static const sxu32 aDefault[] = {
		0x20, 0x0C, 0x0A, 0x0D, 0x09, 0x0B, 0x00, 0xA0, 0x1680,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007,
		0x2008, 0x2009, 0x200A, 0x2028, 0x2029, 0x202F, 0x205F, 0x3000,
		0x85, 0x180E
	};
	const char *zFunc = ph7_function_name(pCtx);
	const char *zIn;
	mb_trim_set sSet;
	int nByte,iEnc,iMode;
	sxu32 i,iLeft,iRight,nLen;
	if( nArg < 1 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* One body, three names: "mb_|l|trim" and "mb_|r|trim" against "mb_|t|rim". */
	iMode = (zFunc[3] == 'l') ? MB_TRIM_LEFT
		: ((zFunc[3] == 'r') ? MB_TRIM_RIGHT : (MB_TRIM_LEFT|MB_TRIM_RIGHT));
	iEnc = MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,zFunc,3);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	SyZero(&sSet,sizeof(sSet));
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		const char *zWhat = ph7_value_to_string(apArg[1],&nByte);
		for( i = 0 ; i < (sxu32)nByte ; i += nLen ){
			/* Every error character decodes to the same member, php's error
			 * marker, so one bad byte in $characters strips them all. */
			sxu32 cp = MbNextCode((const unsigned char *)&zWhat[i],(sxu32)nByte - i,iEnc,&nLen);
			if( MbTrimSetAdd(pCtx,&sSet,cp) != PH7_OK ){
				return PH7_ContextMemoryError(pCtx);
			}
		}
	}else{
		for( i = 0 ; i < SX_ARRAYSIZE(aDefault) ; ++i ){
			if( MbTrimSetAdd(pCtx,&sSet,aDefault[i]) != PH7_OK ){
				return PH7_ContextMemoryError(pCtx);
			}
		}
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	iLeft = 0;
	iRight = (sxu32)nByte;
	if( iMode & MB_TRIM_LEFT ){
		while( iLeft < iRight ){
			sxu32 cp = MbNextCode((const unsigned char *)&zIn[iLeft],iRight - iLeft,iEnc,&nLen);
			if( !MbTrimSetHas(&sSet,cp) ){
				break;
			}
			iLeft += nLen;
		}
	}
	if( iMode & MB_TRIM_RIGHT ){
		/* UTF-8 has no backwards reader here, so re-walk from the left edge and
		 * keep the offset where the CURRENT run of trim characters began; the
		 * last one still open when the walk ends is the trailing run. */
		sxu32 iRun = iRight;
		int bInRun = 0;
		for( i = iLeft ; i < iRight ; i += nLen ){
			sxu32 cp = MbNextCode((const unsigned char *)&zIn[i],iRight - i,iEnc,&nLen);
			if( MbTrimSetHas(&sSet,cp) ){
				if( !bInRun ){
					iRun = i;
					bInRun = 1;
				}
			}else{
				bInRun = 0;
			}
		}
		if( bInRun ){
			iRight = iRun;
		}
	}
	if( iEnc != MB_ENC_UTF8 || (iLeft == 0 && iRight == (sxu32)nByte) ){
		/* php hands the ORIGINAL string back when it trimmed nothing
		 * (trim_each_wchar()'s zend_string_copy), so an ill-formed run survives
		 * a no-op trim and is substituted only when something was sliced off. */
		ph7_result_string(pCtx,&zIn[iLeft],(int)(iRight - iLeft));
		return PH7_OK;
	}
	/* What it does keep is decoded and re-encoded, so an ill-formed run left in
	 * the middle comes back as '?' -- the same rule mb_substr() follows. */
	MbResultSubstituted(pCtx,&zIn[iLeft],iRight - iLeft);
	return PH7_OK;
}
/* string|bool mb_internal_encoding(?string $encoding = null) */
static int PH7_builtin_mb_internal_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 || ph7_value_is_null(apArg[0]) ){
		ph7_result_string(pCtx,"UTF-8",sizeof("UTF-8")-1);
		return PH7_OK;
	}
	if( MbEncodingArg(pCtx,apArg[0],"mb_internal_encoding",1) < 0 ){
		return PH7_OK;
	}
	/* Only the UTF-8 family is accepted, and it is already the default */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* Is every character of zIn[0..nByte-1] a character of iEnc? LATIN1 answers yes
 * to any buffer at all — every byte is a code point there, which is what makes
 * mb_check_encoding("\xff","8bit") true where the ASCII and UTF-8 answers are
 * false. */
static int MbBufferIsValid(const char *zIn,sxu32 nByte,int iEnc)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nLen;
	if( iEnc == MB_ENC_LATIN1 ){
		return 1;
	}
	while( i < nByte ){
		if( MbNextCode(&z[i],nByte - i,iEnc,&nLen) == MB_BAD_CODE ){
			return 0;
		}
		i += nLen;
	}
	return 1;
}
/* ph7_array_walk() callback: fold one element's validity into *pbOk. */
static int MbCheckWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	int *paState = (int *)pUserData;   /* [0] = ok so far, [1] = the encoding */
	const char *zIn;
	int nByte;
	SXUNUSED(pKey);
	if( ph7_value_is_array(pData) ){
		return ph7_array_walk(pData,MbCheckWalker,pUserData);
	}
	zIn = ph7_value_to_string(pData,&nByte);
	if( !MbBufferIsValid(zIn,(sxu32)nByte,paState[1]) ){
		paState[0] = 0;
		return SXERR_ABORT;
	}
	return PH7_OK;
}
/* bool mb_check_encoding(array|string|null $value = null, ?string $encoding = null) */
static int PH7_builtin_mb_check_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,iEnc;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_check_encoding",2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	if( ph7_value_is_array(apArg[0]) ){
		/* php checks EVERY element (the declared type is array|string|null);
		 * stringifying the array checked the six bytes of the word "Array". */
		int aState[2];
		aState[0] = 1;
		aState[1] = iEnc;
		ph7_array_walk(apArg[0],MbCheckWalker,aState);
		ph7_result_bool(pCtx,aState[0]);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	ph7_result_bool(pCtx,MbBufferIsValid(zIn,(sxu32)nByte,iEnc));
	return PH7_OK;
}
/* php's East Asian wide/fullwidth set: two columns, everything else one. */
static int MbCodeWidth(sxu32 cp)
{
	if( (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF)
	 || (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF)
	 || (cp >= 0xFE30 && cp <= 0xFE4F) || (cp >= 0xFF00 && cp <= 0xFF60)
	 || (cp >= 0xFFE0 && cp <= 0xFFE6) || cp >= 0x20000 ){
		return 2;
	}
	return 1;
}
/* int mb_strwidth(string $string, ?string $encoding = null) */
static int PH7_builtin_mb_strwidth(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *z;
	const char *zIn;
	int nByte,iEnc;
	sxu32 i = 0,nLen,cp;
	ph7_int64 nWidth = 0;
	if( nArg < 1 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_strwidth",2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	z = (const unsigned char *)zIn;
	while( i < (sxu32)nByte ){
		cp = MbNextCode(&z[i],(sxu32)nByte - i,iEnc,&nLen);
		i += nLen;
		/* An error character stands in for '?': one column */
		if( cp == MB_BAD_CODE ){
			cp = (sxu32)'?';
		}
		nWidth += MbCodeWidth(cp);
	}
	ph7_result_int64(pCtx,nWidth);
	return PH7_OK;
}

/* string|false mb_chr(int $codepoint, ?string $encoding = null) */
static int PH7_builtin_mb_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 cp;
	unsigned char zOut[4];
	sxu32 n;
	int iEnc;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_chr",2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	cp = ph7_value_to_int64(apArg[0]);
	/* php rejects negatives, code points past U+10FFFF and the UTF-16
	 * surrogate range with FALSE — and, in a one-byte encoding, everything the
	 * encoding cannot hold: mb_chr(233,"ASCII") is false where mb_chr(233,"8bit")
	 * is the single byte 0xE9. */
	if( cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)
	 || (iEnc == MB_ENC_LATIN1 && cp > 0xFF) || (iEnc == MB_ENC_ASCII && cp > 0x7F) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( iEnc != MB_ENC_UTF8 ){
		zOut[0] = (unsigned char)cp;
		ph7_result_string(pCtx,(const char *)zOut,1);
		return PH7_OK;
	}
	n = MbUtf8Encode((sxu32)cp,zOut);
	ph7_result_string(pCtx,(const char *)zOut,(int)n);
	return PH7_OK;
}
/* int|false mb_ord(string $string, ?string $encoding = null) */
static int PH7_builtin_mb_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	const unsigned char *z;
	int nByte,iEnc;
	sxu32 cp,nLen;
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_ord",2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		/* php throws on an empty string rather than returning FALSE */
		return PH7_VmThrowException(pCtx,"ValueError",
			"mb_ord(): Argument #1 ($string) must not be empty");
	}
	z = (const unsigned char *)zIn;
	/* An error character answers FALSE — a malformed UTF-8 first character
	 * (invalid lead / truncated / bad continuation / over-long / surrogate), or a
	 * byte over 0x7F under ASCII. Under a one-byte encoding whose code point IS
	 * the byte there is no such thing, so mb_ord("\xff","8bit") is 255. */
	cp = MbNextCode(z,(sxu32)nByte,iEnc,&nLen);
	if( cp == MB_BAD_CODE ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,(sxi64)cp);
	return PH7_OK;
}
/*
 * mb_detect_encoding(string $string, array|string|null $encodings = null,
 *                    bool $strict = false): string|false
 *
 * PHL's detectable set is ASCII, UTF-8 and ISO-8859-1 (the §10 scope cut).
 * php's default detect order is exactly ASCII,UTF-8, so the null/default path
 * is byte-identical. A candidate encoding php supports but PHL does not (e.g.
 * SJIS) raises the same ValueError php uses for a truly invalid name — a
 * recorded scope divergence, not silent.
 *
 * Two numbers decide it, and both are php's: a candidate that cannot decode the
 * input at all loses to one that can, and among those that can, the one that
 * reads the FEWEST characters wins — which is why "\xc3\xa1" is UTF-8 (one
 * character) rather than the ISO-8859-1 listed ahead of it (two), while a pure
 * ASCII string, where every candidate reads the same count, is the first one
 * named. In strict mode a candidate with any error at all answers false.
 * `8bit`/`binary` are php's non-detectable pair: named, counted, never chosen.
 */
#define MB_DETECT_NEVER  3   /* 8bit / binary: a candidate php never selects */
static int MbAsciiErrors(const unsigned char *z,int n)
{
	int i,e = 0;
	for( i = 0 ; i < n ; i++ ){
		if( z[i] >= 0x80 ){ e++; }
	}
	return e;
}
static int MbUtf8Errors(const unsigned char *z,int n)
{
	sxu32 i = 0,nLen;
	int e = 0;
	while( i < (sxu32)n ){
		if( MbUtf8Decode(&z[i],(sxu32)n - i,&nLen) < 0 ){ e++; i++; }
		else{ i += nLen; }
	}
	return e;
}
/* Map an encoding name to PHL's detectable set: 0 = ASCII, 1 = UTF-8,
 * 2 = ISO-8859-1, MB_DETECT_NEVER = 8bit/binary, -1 = out of scope. Surrounding
 * ASCII whitespace is trimmed (php accepts "ASCII, UTF-8"). */
static int MbDetectEncId(const char *z,int n)
{
	while( n > 0 && (z[0]==' '||z[0]=='\t'||z[0]=='\n'||z[0]=='\r') ){ z++; n--; }
	while( n > 0 && (z[n-1]==' '||z[n-1]=='\t'||z[n-1]=='\n'||z[n-1]=='\r') ){ n--; }
	if( (n == 5 && SyStrnicmp(z,"ASCII",5) == 0)
	 || (n == 8 && SyStrnicmp(z,"US-ASCII",8) == 0) ){
		return 0;
	}
	if( (n == 5 && SyStrnicmp(z,"UTF-8",5) == 0)
	 || (n == 4 && SyStrnicmp(z,"UTF8",4) == 0) ){
		return 1;
	}
	if( (n == 10 && SyStrnicmp(z,"ISO-8859-1",10) == 0)
	 || (n == 9 && SyStrnicmp(z,"ISO8859-1",9) == 0)
	 || (n == 6 && SyStrnicmp(z,"latin1",6) == 0) ){
		return 2;
	}
	if( (n == 4 && SyStrnicmp(z,"8bit",4) == 0)
	 || (n == 6 && SyStrnicmp(z,"binary",6) == 0) ){
		return MB_DETECT_NEVER;
	}
	return -1;
}
/* Per-detection running state, shared by the array walker and the string path. */
typedef struct mb_detect_state mb_detect_state;
struct mb_detect_state {
	ph7_context *pCtx;
	int aErr[3];      /* precomputed [ASCII], [UTF-8], [ISO-8859-1] error counts */
	int aChar[3];     /* and the character count each one reads */
	int iBestEnc;     /* winning encoding id, -1 until the first that can win */
	int iBestErr;     /* its error count */
	int iBestChar;    /* and its character count */
	int nSeen;        /* candidates considered (0 -> "must specify at least one") */
	int bError;       /* an out-of-scope name threw -> abort */
	int rc;           /* the throw's propagation code (PH7_ABORT/PH7_EXCEPTION) */
};
/* Fold one candidate encoding name into the running best. Returns SXERR_ABORT
 * (and throws) when the name is outside PHL's detectable scope. */
static int MbDetectConsider(mb_detect_state *pState,const char *zName,int nName)
{
	int enc = MbDetectEncId(zName,nName);
	if( enc < 0 ){
		pState->bError = 1;
		pState->rc = PH7_VmThrowException(pState->pCtx,"ValueError",
			"mb_detect_encoding(): Argument #2 ($encodings) contains invalid encoding \"%.*s\"",
			nName,zName);
		return SXERR_ABORT;
	}
	pState->nSeen++;
	if( enc == MB_DETECT_NEVER ){
		return PH7_OK;
	}
	if( pState->iBestEnc < 0
	 || pState->aErr[enc] < pState->iBestErr
	 || (pState->aErr[enc] == pState->iBestErr && pState->aChar[enc] < pState->iBestChar) ){
		pState->iBestEnc = enc;
		pState->iBestErr = pState->aErr[enc];
		pState->iBestChar = pState->aChar[enc];
	}
	return PH7_OK;
}
/* ph7_array_walk() callback over the $encodings array. */
static int MbDetectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	mb_detect_state *pState = (mb_detect_state *)pUserData;
	const char *zName;
	int nName;
	SXUNUSED(pKey);
	zName = ph7_value_to_string(pData,&nName);
	return MbDetectConsider(pState,zName,nName);
}
static int PH7_builtin_mb_detect_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,bStrict = 0;
	mb_detect_state sState;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	if( nArg > 2 ){ bStrict = ph7_value_to_bool(apArg[2]); }
	sState.pCtx = pCtx;
	sState.aErr[0] = MbAsciiErrors((const unsigned char *)zIn,nByte);
	sState.aErr[1] = MbUtf8Errors((const unsigned char *)zIn,nByte);
	sState.aErr[2] = 0;   /* every byte is a character of ISO-8859-1 */
	sState.aChar[0] = nByte;
	sState.aChar[1] = (int)MbStrlen(zIn,(sxu32)nByte,MB_ENC_UTF8);
	sState.aChar[2] = nByte;
	sState.iBestEnc = -1;
	sState.iBestErr = 0;
	sState.iBestChar = 0;
	sState.nSeen = 0;
	sState.bError = 0;
	sState.rc = PH7_OK;
	if( nArg < 2 || ph7_value_is_null(apArg[1]) ){
		/* php's default detect order is exactly ASCII, then UTF-8 */
		MbDetectConsider(&sState,"ASCII",5);
		MbDetectConsider(&sState,"UTF-8",5);
	}else if( ph7_value_is_array(apArg[1]) ){
		if( ph7_array_count(apArg[1]) == 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"mb_detect_encoding(): Argument #2 ($encodings) must specify at least one encoding");
		}
		ph7_array_walk(apArg[1],MbDetectWalker,&sState);
		if( sState.bError ){ return sState.rc; }
	}else{
		/* comma-separated list, e.g. "ASCII, UTF-8" */
		const char *z2;
		int n2,i,iStart = 0;
		z2 = ph7_value_to_string(apArg[1],&n2);
		for( i = 0 ; i <= n2 ; i++ ){
			if( i == n2 || z2[i] == ',' ){
				const char *zTok = &z2[iStart];
				int nTok = i - iStart,t = nTok;
				/* ignore an empty / all-whitespace token */
				while( t > 0 && (zTok[0]==' '||zTok[0]=='\t'||zTok[0]=='\n'||zTok[0]=='\r') ){ zTok++; t--; }
				if( t > 0 && MbDetectConsider(&sState,&z2[iStart],nTok) == SXERR_ABORT ){
					return sState.rc;
				}
				iStart = i + 1;
			}
		}
	}
	if( sState.nSeen == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"mb_detect_encoding(): Argument #2 ($encodings) must specify at least one encoding");
	}
	if( sState.iBestEnc < 0 || (bStrict && sState.iBestErr > 0) ){
		/* nothing but 8bit/binary was named, or the best candidate still had an
		 * error and the caller asked for a strict answer */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( sState.iBestEnc == 2 ){
		ph7_result_string(pCtx,"ISO-8859-1",sizeof("ISO-8859-1")-1);
	}else{
		ph7_result_string(pCtx,sState.iBestEnc == 0 ? "ASCII" : "UTF-8",5);
	}
	return PH7_OK;
}
/*
 * mb_convert_encoding(array|string $string, string $to_encoding,
 *                     array|string|null $from_encoding = null): array|string
 *
 * PHL's encoding scope is UTF-8, the byte encodings (8bit/binary/ASCII) and
 * ISO-8859-1 (the §10 scope cut — php's full encoding zoo is out; a php-valid
 * name PHL does not model, e.g. SJIS, raises the same ValueError php uses for a
 * truly invalid name). ISO-8859-1 is carried because it is the documented
 * replacement path for the removed utf8_encode()/utf8_decode() builtins:
 * mb_convert_encoding($s,'UTF-8','ISO-8859-1') and its inverse. Conversion is
 * codepoint-exact for the modelled encodings; a source byte or codepoint that
 * cannot be represented in the target maps to '?' (0x3F), php's default
 * substitute character.
 */
/* Transcode one byte buffer from idFrom to idTo, appending to pOut. Input that
 * cannot be represented in the target substitutes '?' (0x3F), php's default. */
static void MbConvertBuffer(SyBlob *pOut,const char *zIn,sxu32 nByte,int idFrom,int idTo)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nLen,cp;
	unsigned char zEnc[4];
	while( i < nByte ){
		if( idFrom == MB_ENC_UTF8 ){
			sxi32 iCp = MbUtf8Decode(&z[i],nByte - i,&nLen);
			cp = (iCp < 0) ? (sxu32)'?' : (sxu32)iCp; /* invalid sequence */
			i += nLen;
		}else{
			cp = z[i];
			i++;
			if( idFrom == MB_ENC_ASCII && cp > 0x7F ){ cp = '?'; }
		}
		if( idTo == MB_ENC_UTF8 ){
			SyBlobAppend(pOut,zEnc,MbUtf8Encode(cp,zEnc));
		}else{
			sxu32 iMax = (idTo == MB_ENC_ASCII) ? 0x7F : 0xFF;
			zEnc[0] = (unsigned char)((cp <= iMax) ? cp : '?');
			SyBlobAppend(pOut,zEnc,1);
		}
	}
}
/* Build a converted copy of pIn as a fresh context value: a string is
 * transcoded; an array is rebuilt element by element (keys preserved, nested
 * arrays recursed) to match php's array form. Returns 0 on allocation failure. */
static ph7_value * MbConvertNew(ph7_context *pCtx,ph7_value *pIn,int idFrom,int idTo)
{
	if( ph7_value_is_array(pIn) ){
		ph7_hashmap *pMap = (ph7_hashmap *)pIn->x.pOther;
		ph7_hashmap_node *pEntry = pMap->pFirst;
		ph7_value *pArr = ph7_context_new_array(pCtx);
		ph7_value sKey;
		sxu32 n;
		if( pArr == 0 ){
			return 0;
		}
		PH7_MemObjInit(pCtx->pVm,&sKey);
		for( n = 0 ; n < pMap->nEntry ; n++ ){
			ph7_value *pData = HashmapExtractNodeValue(pEntry);
			if( pData ){
				ph7_value *pConv = MbConvertNew(pCtx,pData,idFrom,idTo);
				if( pConv ){
					PH7_HashmapExtractNodeKey(pEntry,&sKey);
					ph7_array_add_elem(pArr,&sKey,pConv);
					PH7_MemObjRelease(&sKey);
					ph7_context_release_value(pCtx,pConv);
				}
			}
			pEntry = pEntry->pPrev; /* forward walk (reverse link) */
		}
		return pArr;
	}else{
		SyBlob sOut;
		const char *zIn;
		int nByte;
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		if( pVal == 0 ){
			return 0;
		}
		zIn = ph7_value_to_string(pIn,&nByte);
		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
		MbConvertBuffer(&sOut,zIn,(sxu32)nByte,idFrom,idTo);
		ph7_value_string(pVal,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
		SyBlobRelease(&sOut);
		return pVal;
	}
}
static int PH7_builtin_mb_convert_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zTo,*zFrom;
	int nTo,nFrom,idTo,idFrom;
	ph7_value *pResult;
	if( nArg < 2 ){
		/* the arity guard fires first; stay defensive */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zTo = ph7_value_to_string(apArg[1],&nTo);
	idTo = MbConvEncId(zTo,nTo);
	if( idTo < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"mb_convert_encoding(): Argument #2 ($to_encoding) must be a valid encoding, \"%.*s\" given",
			nTo,zTo);
	}
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		/* php also accepts an array / comma list here for source detection;
		 * PHL's modelled set makes detection trivial, so a single name is taken
		 * (a list falls out of scope and hits the same loud ValueError). */
		zFrom = ph7_value_to_string(apArg[2],&nFrom);
		idFrom = MbConvEncId(zFrom,nFrom);
		if( idFrom < 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"mb_convert_encoding(): Argument #3 ($from_encoding) contains invalid encoding \"%.*s\"",
				nFrom,zFrom);
		}
	}else{
		/* php falls back to the internal encoding, which PHL fixes at UTF-8 */
		idFrom = MB_ENC_UTF8;
	}
	pResult = MbConvertNew(pCtx,apArg[0],idFrom,idTo);
	if( pResult == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_result_value(pCtx,pResult);
	return PH7_OK;
}
/*
 * Install the mb_* functions (called from PH7_RegisterBuiltInFunction's
 * table in builtin.c via these PH7_PRIVATE symbols).
 */
PH7_PRIVATE int PH7_builtin_mb_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strlen(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_substr(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strtolower(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_convert_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_convert_case(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_ucfirst_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_ucfirst(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strpos(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_strstr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strstr(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_substr_count_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_substr_count(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_str_split_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_str_split(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_trim_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_trim(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_internal_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_internal_encoding(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_check_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_check_encoding(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_strwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strwidth(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_chr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_chr(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_ord_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_ord(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_detect_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_detect_encoding(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_convert_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_convert_encoding(pCtx,nArg,apArg); }

#endif /* PH7_DISABLE_BUILTIN_FUNC */
