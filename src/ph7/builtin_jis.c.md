# src/ph7/builtin_jis.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 58/63 lines (92.06%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` *` |
|    - |    5 | ` * The Japanese legacy character sets: JIS X 0208 and the two JIS X 0201 sets,` |
|    - |    6 | ` * both directions.` |
|    - |    7 | ` *` |
|    - |    8 | ` * This is the TABLE, not an encoding. ISO-2022-JP, EUC-JP and Shift_JIS are` |
|    - |    9 | ` * three ways of putting the same 94x94 JIS X 0208 code space on the wire --` |
|    - |   10 | ` * one escapes into it, one sets the high bit of both bytes, one folds two rows` |
|    - |   11 | ` * into one lead byte -- and each of those framings is its own converter arm.` |
|    - |   12 | ` * What they share is which cell means which character, and that is here, so a` |
|    - |   13 | ` * cell is answered once and the same way whichever framing asked.` |
|    - |   14 | ` *` |
|    - |   15 | `` * Guarded by PH7_ENABLE_JIS, which `full` and `coverage` set and `tiny` does`` |
|    - |   16 | ` * not: the payload is ~31 KB, and the tiny build ships the scope cut its iconv` |
|    - |   17 | ` * and mb_ already carry. With the flag off every door below is absent, and a` |
|    - |   18 | ` * caller that names one of the Japanese encodings gets the same "wrong` |
|    - |   19 | ` * encoding" answer it gets today.` |
|    - |   20 | ` *` |
|    - |   21 | ` * The table was CUT FROM php, cell by cell, and from BOTH of the faces php has` |
|    - |   22 | ` * for it -- iconv(), which is the C library's table, and mb_convert_encoding(),` |
|    - |   23 | ` * which is mbstring's own. They agree on all 6879 assigned cells, with no cell` |
|    - |   24 | ` * one answers and the other does not, which is why one table can serve both` |
|    - |   25 | ` * extensions here. It is also one-to-one, so the reverse direction is the` |
|    - |   26 | ` * forward table inverted rather than a second sweep with its own rules about` |
|    - |   27 | ` * which of several cells a code point should come back as. Both facts are` |
|    - |   28 | ` * re-checked every time the table is cut: build-aux/gen_jis.php refuses to` |
|    - |   29 | ` * write a header if either stops holding.` |
|    - |   30 | ` *` |
|    - |   31 | ` * The JIS X 0201 sets are not tables. The Roman set is ASCII with two cells` |
|    - |   32 | ` * moved -- 0x5C carries U+00A5 YEN SIGN and 0x7E carries U+203E OVERLINE,` |
|    - |   33 | ` * which is the whole reason a Japanese text file full of backslashes reads as` |
|    - |   34 | ` * currency -- and the katakana set is the arithmetic 0xA1..0xDF ->` |
|    - |   35 | ` * U+FF61..U+FF9F. Both are asserted against the oracle when the table is cut` |
|    - |   36 | ` * rather than emitted into it.` |
|    - |   37 | ` */` |
|    - |   38 | `#include "ph7int.h"` |
|    - |   39 |  |
|    - |   40 | `#if defined(PH7_ENABLE_JIS) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|    - |   41 |  |
|    - |   42 | `#include "builtin_jis.h"` |
|    - |   43 |  |
|    - |   44 | `/*` |
|    - |   45 | ` * The code point cell (iRow,iCell) of JIS X 0208 stands for, or 0 when the` |
|    - |   46 | ` * cell is unassigned. Both coordinates are the standard's own 0x21..0x7E --` |
|    - |   47 | ` * the ku-ten numbering plus 0x20 -- because that is what every framing hands` |
|    - |   48 | ` * over after it has stripped its own bits, and a caller that has a row outside` |
|    - |   49 | ` * the code space gets the unassigned answer rather than a range check of its` |
|    - |   50 | ` * own.` |
|    - |   51 | ` */` |
|  272 |   52 | `PH7_PRIVATE sxu32 PH7_JisX0208ToUni(int iRow,int iCell)` |
|    3 |   53 | `{` |
|    - |   54 | `	int k;` |
|  275 |   55 | `	if( iRow < 0x21 \|\| iRow > 0x7E \|\| iCell < 0x21 \|\| iCell > 0x7E ){` |
|    3 |   56 | `		return 0;` |
|    - |   57 | `	}` |
|  273 |   58 | `	k = (iRow - 0x21) * PH7_JIS_X0208_ROWS + (iCell - 0x21);` |
|  273 |   59 | `	return (sxu32)aJisX0208Uni[k];` |
|  139 |   60 | `}` |
|    - |   61 | `/*` |
|    - |   62 | ` * The cell that carries cp, or 0 when JIS X 0208 has no cell for it. A binary` |
|    - |   63 | ` * search over the cells ordered by code point: the table is one-to-one, so` |
|    - |   64 | ` * there is never a choice to make between two cells, and a hit is the answer` |
|    - |   65 | ` * rather than the first of several.` |
|    - |   66 | ` */` |
|  360 |   67 | `PH7_PRIVATE int PH7_JisX0208FromUni(sxu32 cp,int *piRow,int *piCell)` |
|    2 |   68 | `{` |
|  362 |   69 | `	int iLo = 0,iHi = PH7_JIS_X0208_COUNT - 1;` |
|  362 |   70 | `	if( cp > 0xFFFF ){` |
|    - |   71 | `		/* Every assigned cell is in the BMP, so nothing above it can hit and` |
|    - |   72 | `		 * the search need not widen to hold the comparison. */` |
|  ! 0 |   73 | `		return 0;` |
|    - |   74 | `	}` |
| 4570 |   75 | `	while( iLo <= iHi ){` |
| 4484 |   76 | `		int iMid = iLo + (iHi - iLo) / 2;` |
| 4484 |   77 | `		int k = (int)aJisX0208Rev[iMid];` |
| 4484 |   78 | `		sxu32 u = (sxu32)aJisX0208Uni[k];` |
| 4484 |   79 | `		if( u == cp ){` |
|  276 |   80 | `			*piRow  = k / PH7_JIS_X0208_ROWS + 0x21;` |
|  276 |   81 | `			*piCell = k % PH7_JIS_X0208_ROWS + 0x21;` |
|  276 |   82 | `			return 1;` |
|    - |   83 | `		}` |
| 4210 |   84 | `		if( u < cp ){` |
| 2030 |   85 | `			iLo = iMid + 1;` |
| 1016 |   86 | `		}else{` |
| 2182 |   87 | `			iHi = iMid - 1;` |
|    - |   88 | `		}` |
|    2 |   89 | `	}` |
|   88 |   90 | `	return 0;` |
|  182 |   91 | `}` |
|    - |   92 | `/*` |
|    - |   93 | ` * JIS X 0201's Roman set: ASCII with 0x5C and 0x7E carrying the yen sign and` |
|    - |   94 | ` * the overline instead of the backslash and the tilde. Answers 0 for a byte` |
|    - |   95 | ` * the set does not hold, which is every byte above 0x7F -- the katakana half` |
|    - |   96 | ` * is a separate set reached through a separate shift, never through this one.` |
|    - |   97 | ` */` |
|    8 |   98 | `PH7_PRIVATE sxu32 PH7_JisX0201RomanToUni(int c)` |
|    1 |   99 | `{` |
|    9 |  100 | `	if( c < 0 \|\| c > 0x7F ){` |
|  ! 0 |  101 | `		return 0;` |
|    - |  102 | `	}` |
|    9 |  103 | `	if( c == 0x5C ){` |
|    3 |  104 | `		return 0x00A5;` |
|    - |  105 | `	}` |
|    7 |  106 | `	if( c == 0x7E ){` |
|    3 |  107 | `		return 0x203E;` |
|    - |  108 | `	}` |
|    5 |  109 | `	return (sxu32)c;` |
|    5 |  110 | `}` |
|    - |  111 | `/* And back. 0x00A5 and 0x203E take the two moved cells; the backslash and the` |
|    - |  112 | ` * tilde have no cell at all in this set, which is what makes a round trip` |
|    - |  113 | ` * through it lossy in the direction people notice. */` |
|   96 |  114 | `PH7_PRIVATE int PH7_JisX0201RomanFromUni(sxu32 cp,int *piByte)` |
|    2 |  115 | `{` |
|   98 |  116 | `	if( cp == 0x00A5 ){` |
|   15 |  117 | `		*piByte = 0x5C;` |
|   15 |  118 | `		return 1;` |
|    - |  119 | `	}` |
|   84 |  120 | `	if( cp == 0x203E ){` |
|    3 |  121 | `		*piByte = 0x7E;` |
|    3 |  122 | `		return 1;` |
|    - |  123 | `	}` |
|   82 |  124 | `	if( cp <= 0x7F && cp != 0x5C && cp != 0x7E ){` |
|  ! 0 |  125 | `		*piByte = (int)cp;` |
|  ! 0 |  126 | `		return 1;` |
|    - |  127 | `	}` |
|   82 |  128 | `	return 0;` |
|   50 |  129 | `}` |
|    - |  130 | `/*` |
|    - |  131 | ` * JIS X 0201's katakana set: the halfwidth forms, contiguous in both, so the` |
|    - |  132 | ` * mapping is arithmetic in both directions. The byte numbering here is the` |
|    - |  133 | ` * 0xA1..0xDF one every framing uses, not the 0x21..0x5F the standard writes.` |
|    - |  134 | ` */` |
|   78 |  135 | `PH7_PRIVATE sxu32 PH7_JisX0201KanaToUni(int c)` |
|    2 |  136 | `{` |
|   80 |  137 | `	if( c < 0xA1 \|\| c > 0xDF ){` |
|  ! 0 |  138 | `		return 0;` |
|    - |  139 | `	}` |
|   80 |  140 | `	return (sxu32)(0xFF61 + (c - 0xA1));` |
|   41 |  141 | `}` |
|  240 |  142 | `PH7_PRIVATE int PH7_JisX0201KanaFromUni(sxu32 cp,int *piByte)` |
|    1 |  143 | `{` |
|  241 |  144 | `	if( cp < 0xFF61 \|\| cp > 0xFF9F ){` |
|  217 |  145 | `		return 0;` |
|    - |  146 | `	}` |
|   25 |  147 | `	*piByte = (int)(0xA1 + (cp - 0xFF61));` |
|   25 |  148 | `	return 1;` |
|  121 |  149 | `}` |
|    - |  150 |  |
|    - |  151 | `#endif /* PH7_ENABLE_JIS && !PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  152 |  |
