/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The Japanese legacy character sets: JIS X 0208 and the two JIS X 0201 sets,
 * both directions.
 *
 * This is the TABLE, not an encoding. ISO-2022-JP, EUC-JP and Shift_JIS are
 * three ways of putting the same 94x94 JIS X 0208 code space on the wire --
 * one escapes into it, one sets the high bit of both bytes, one folds two rows
 * into one lead byte -- and each of those framings is its own converter arm.
 * What they share is which cell means which character, and that is here, so a
 * cell is answered once and the same way whichever framing asked.
 *
 * Guarded by PH7_ENABLE_JIS, which `full` and `coverage` set and `tiny` does
 * not: the payload is ~31 KB, and the tiny build ships the scope cut its iconv
 * and mb_ already carry. With the flag off every door below is absent, and a
 * caller that names one of the Japanese encodings gets the same "wrong
 * encoding" answer it gets today.
 *
 * The table was CUT FROM php, cell by cell, and from BOTH of the faces php has
 * for it -- iconv(), which is the C library's table, and mb_convert_encoding(),
 * which is mbstring's own. They agree on all 6879 assigned cells, with no cell
 * one answers and the other does not, which is why one table can serve both
 * extensions here. It is also one-to-one, so the reverse direction is the
 * forward table inverted rather than a second sweep with its own rules about
 * which of several cells a code point should come back as. Both facts are
 * re-checked every time the table is cut: build-aux/gen_jis.php refuses to
 * write a header if either stops holding.
 *
 * The JIS X 0201 sets are not tables. The Roman set is ASCII with two cells
 * moved -- 0x5C carries U+00A5 YEN SIGN and 0x7E carries U+203E OVERLINE,
 * which is the whole reason a Japanese text file full of backslashes reads as
 * currency -- and the katakana set is the arithmetic 0xA1..0xDF ->
 * U+FF61..U+FF9F. Both are asserted against the oracle when the table is cut
 * rather than emitted into it.
 */
#include "ph7int.h"

#if defined(PH7_ENABLE_JIS) && !defined(PH7_DISABLE_BUILTIN_FUNC)

#include "builtin_jis.h"

/*
 * The code point cell (iRow,iCell) of JIS X 0208 stands for, or 0 when the
 * cell is unassigned. Both coordinates are the standard's own 0x21..0x7E --
 * the ku-ten numbering plus 0x20 -- because that is what every framing hands
 * over after it has stripped its own bits, and a caller that has a row outside
 * the code space gets the unassigned answer rather than a range check of its
 * own.
 */
PH7_PRIVATE sxu32 PH7_JisX0208ToUni(int iRow,int iCell)
{
	int k;
	if( iRow < 0x21 || iRow > 0x7E || iCell < 0x21 || iCell > 0x7E ){
		return 0;
	}
	k = (iRow - 0x21) * PH7_JIS_X0208_ROWS + (iCell - 0x21);
	return (sxu32)aJisX0208Uni[k];
}
/*
 * The cell that carries cp, or 0 when JIS X 0208 has no cell for it. A binary
 * search over the cells ordered by code point: the table is one-to-one, so
 * there is never a choice to make between two cells, and a hit is the answer
 * rather than the first of several.
 */
PH7_PRIVATE int PH7_JisX0208FromUni(sxu32 cp,int *piRow,int *piCell)
{
	int iLo = 0,iHi = PH7_JIS_X0208_COUNT - 1;
	if( cp > 0xFFFF ){
		/* Every assigned cell is in the BMP, so nothing above it can hit and
		 * the search need not widen to hold the comparison. */
		return 0;
	}
	while( iLo <= iHi ){
		int iMid = iLo + (iHi - iLo) / 2;
		int k = (int)aJisX0208Rev[iMid];
		sxu32 u = (sxu32)aJisX0208Uni[k];
		if( u == cp ){
			*piRow  = k / PH7_JIS_X0208_ROWS + 0x21;
			*piCell = k % PH7_JIS_X0208_ROWS + 0x21;
			return 1;
		}
		if( u < cp ){
			iLo = iMid + 1;
		}else{
			iHi = iMid - 1;
		}
	}
	return 0;
}
/*
 * JIS X 0201's Roman set: ASCII with 0x5C and 0x7E carrying the yen sign and
 * the overline instead of the backslash and the tilde. Answers 0 for a byte
 * the set does not hold, which is every byte above 0x7F -- the katakana half
 * is a separate set reached through a separate shift, never through this one.
 */
PH7_PRIVATE sxu32 PH7_JisX0201RomanToUni(int c)
{
	if( c < 0 || c > 0x7F ){
		return 0;
	}
	if( c == 0x5C ){
		return 0x00A5;
	}
	if( c == 0x7E ){
		return 0x203E;
	}
	return (sxu32)c;
}
/* And back. 0x00A5 and 0x203E take the two moved cells; the backslash and the
 * tilde have no cell at all in this set, which is what makes a round trip
 * through it lossy in the direction people notice. */
PH7_PRIVATE int PH7_JisX0201RomanFromUni(sxu32 cp,int *piByte)
{
	if( cp == 0x00A5 ){
		*piByte = 0x5C;
		return 1;
	}
	if( cp == 0x203E ){
		*piByte = 0x7E;
		return 1;
	}
	if( cp <= 0x7F && cp != 0x5C && cp != 0x7E ){
		*piByte = (int)cp;
		return 1;
	}
	return 0;
}
/*
 * JIS X 0201's katakana set: the halfwidth forms, contiguous in both, so the
 * mapping is arithmetic in both directions. The byte numbering here is the
 * 0xA1..0xDF one every framing uses, not the 0x21..0x5F the standard writes.
 */
PH7_PRIVATE sxu32 PH7_JisX0201KanaToUni(int c)
{
	if( c < 0xA1 || c > 0xDF ){
		return 0;
	}
	return (sxu32)(0xFF61 + (c - 0xA1));
}
PH7_PRIVATE int PH7_JisX0201KanaFromUni(sxu32 cp,int *piByte)
{
	if( cp < 0xFF61 || cp > 0xFF9F ){
		return 0;
	}
	*piByte = (int)(0xA1 + (cp - 0xFF61));
	return 1;
}

#endif /* PH7_ENABLE_JIS && !PH7_DISABLE_BUILTIN_FUNC */
