/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXMACROS_H__
#define __SXMACROS_H__

#include "sxtypes.h"
#include <string.h>

/* SyString manipulation macros */
#define SyStringData(RAW)   ((RAW)->zString)
#define SyStringLength(RAW) ((RAW)->nByte)
#define SyStringInitFromBuf(RAW,ZBUF,NLEN){\
	(RAW)->zString  = (const char *)ZBUF;\
	(RAW)->nByte    = (sxu32)(NLEN);\
}
#define SyStringUpdatePtr(RAW,NBYTES){\
	if( NBYTES > (RAW)->nByte ){\
		(RAW)->nByte = 0;\
	}else{\
		(RAW)->zString += NBYTES;\
		(RAW)->nByte -= NBYTES;\
	}\
}
#define SyStringDupPtr(RAW1,RAW2)\
	(RAW1)->zString = (RAW2)->zString;\
	(RAW1)->nByte = (RAW2)->nByte;

#define SyStringTrimLeadingChar(RAW,CHAR)\
	while((RAW)->nByte > 0 && (RAW)->zString[0] == CHAR ){\
			(RAW)->zString++;\
			(RAW)->nByte--;\
	}
#define SyStringTrimTrailingChar(RAW,CHAR)\
	while((RAW)->nByte > 0 && (RAW)->zString[(RAW)->nByte - 1] == CHAR){\
		(RAW)->nByte--;\
	}
#define SyStringCmp(RAW1,RAW2,xCMP)\
	(((RAW1)->nByte == (RAW2)->nByte) ? xCMP((RAW1)->zString,(RAW2)->zString,(RAW2)->nByte) : (sxi32)((RAW1)->nByte - (RAW2)->nByte))

#define SyStringCmp2(RAW1,RAW2,xCMP)\
	(((RAW1)->nByte >= (RAW2)->nByte) ? xCMP((RAW1)->zString,(RAW2)->zString,(RAW2)->nByte) : (sxi32)((RAW2)->nByte - (RAW1)->nByte))

#define SyStringCharCmp(RAW,CHAR) \
	(((RAW)->nByte == sizeof(char)) ? ((RAW)->zString[0] == CHAR ? 0 : CHAR - (RAW)->zString[0]) : ((RAW)->zString[0] == CHAR ? 0 : (RAW)->nByte - sizeof(char)))

/* Linked list macros */
#define MACRO_LIST_PUSH(Head,Item)\
	Item->pNext = Head;\
	Head = Item;
#define MACRO_LD_PUSH(Head,Item)\
	if( Head == 0 ){\
		Head = Item;\
	}else{\
		Item->pNext = Head;\
		Head->pPrev = Item;\
		Head = Item;\
	}
#define MACRO_LD_REMOVE(Head,Item)\
	if( Head == Item ){\
		Head = Head->pNext;\
	}\
	if( Item->pPrev ){ Item->pPrev->pNext = Item->pNext;}\
	if( Item->pNext ){ Item->pNext->pPrev = Item->pPrev;}

/* Comparison, byte swap, byte copy macros */
/*
 * The two byte-at-a-time primitives the whole engine is built on, over the C
 * library's own. These ran a hand-unrolled four-byte loop, which is what a 2011
 * compiler could be trusted with and is now several times slower than one call
 * to a libc routine that reads a vector register at a time -- and they are the
 * bottom of everything: a profile of the ecosystem gate's phpcs step counted
 * 201M SyMemcmp, 191M SyMemcpy and 52M SyZero in a nine-second run.
 *
 * SX_MACRO_FAST_CMP keeps its exact answer, which is NOT memcmp's: php's
 * strcmp() reports the DIFFERENCE of the first bytes that differ
 * (`strcmp("a","z")` is -25, under php 8.5 too), where memcmp only promises the
 * sign of it. So memcmp decides equality at vector speed and the byte scan runs
 * only over the equal prefix of an UNEQUAL pair, which is short by definition.
 */
#define SX_MACRO_FAST_CMP(X1,X2,SIZE,RC){\
	const unsigned char *r1 = (const unsigned char *)(X1);\
	const unsigned char *r2 = (const unsigned char *)(X2);\
	sxu32 LEN = (sxu32)(SIZE);\
	RC = LEN ? memcmp(r1,r2,(size_t)LEN) : 0;\
	if( RC != 0 ){\
		while( r1[0] == r2[0] ){ r1++; r2++; }\
		RC = (sxi32)r1[0] - (sxi32)r2[0];\
	}\
}
/* memMOVE, not memcpy: the byte loop this replaces copied forward, so a caller
 * whose ranges overlap downwards was well-defined before and stays so. */
#define SX_MACRO_FAST_MEMCPY(SRC,DST,SIZ){\
	sxu32 xLen = (sxu32)(SIZ);\
	if( xLen ){ memmove((void *)(DST),(const void *)(SRC),(size_t)xLen); }\
}
#define SX_MACRO_BYTE_SWAP(X,Y,Z){\
	register unsigned char *s = (unsigned char *)X;\
	register unsigned char *d = (unsigned char *)Y;\
	sxu32   ZLong = Z;  \
	sxi32 c; \
	for(;;){\
	  if(!ZLong){ break; } c = s[0] ; s[0] = d[0]; d[0] = (unsigned char)c; s++; d++; --ZLong;\
	  if(!ZLong){ break; } c = s[0] ; s[0] = d[0]; d[0] = (unsigned char)c; s++; d++; --ZLong;\
	  if(!ZLong){ break; } c = s[0] ; s[0] = d[0]; d[0] = (unsigned char)c; s++; d++; --ZLong;\
	  if(!ZLong){ break; } c = s[0] ; s[0] = d[0]; d[0] = (unsigned char)c; s++; d++; --ZLong;\
	}\
}

/* SPDX-SnippetBegin */
/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */
/* SPDX-License-Identifier: blessing */
/*
** Notes on UTF-8 (According to SQLite3 authors):
**
**   Byte-0    Byte-1    Byte-2    Byte-3    Value
**  0xxxxxxx                                 00000000 00000000 0xxxxxxx
**  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx
**  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx
**  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx
**
*/
/*
** Assuming zIn points to the first byte of a UTF-8 character,
** advance zIn to point to the first byte of the next UTF-8 character.
*/
#define SX_JMP_UTF8(zIn,zEnd)\
	while(zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){ zIn++; }
#define SX_WRITE_UTF8(zOut, c) {                       \
  if( c<0x00080 ){                                     \
    *zOut++ = (sxu8)(c&0xFF);                          \
  }else if( c<0x00800 ){                               \
    *zOut++ = 0xC0 + (sxu8)((c>>6)&0x1F);              \
    *zOut++ = 0x80 + (sxu8)(c & 0x3F);                 \
  }else if( c<0x10000 ){                               \
    *zOut++ = 0xE0 + (sxu8)((c>>12)&0x0F);             \
    *zOut++ = 0x80 + (sxu8)((c>>6) & 0x3F);            \
    *zOut++ = 0x80 + (sxu8)(c & 0x3F);                 \
  }else{                                               \
    *zOut++ = 0xF0 + (sxu8)((c>>18) & 0x07);           \
    *zOut++ = 0x80 + (sxu8)((c>>12) & 0x3F);           \
    *zOut++ = 0x80 + (sxu8)((c>>6) & 0x3F);            \
    *zOut++ = 0x80 + (sxu8)(c & 0x3F);                 \
  }                                                    \
}
/* SPDX-SnippetEnd */

/* Rely on the standard ctype */
#include <ctype.h>
/* ...except for CASE FOLDING, which php defines as ASCII-only and
 * locale-INDEPENDENT everywhere it folds: name resolution (the class / method /
 * function / use-import tables fold through SyStrHash and SyStrnicmp) and the
 * string builtins alike (strtolower, strtoupper, ucfirst, lcfirst, ucwords,
 * strcasecmp/strncasecmp, stristr/str_ireplace, natcasesort, SORT_FLAG_CASE,
 * soundex — all ASCII-only in php 8).  libc tolower()/toupper() follow LC_CTYPE,
 * so in an embedder that calls setlocale(LC_CTYPE,"tr_TR.UTF-8") the dotless-i
 * rule would leak in: STRLEN would stop resolving to strlen and strtolower("I")
 * would answer something other than "i".  Plain ASCII arithmetic keeps the fold
 * the same in every locale (and drops a libc call per byte on the name-table
 * hash/compare path).  The ctype PREDICATES below stay libc's: they classify,
 * they do not fold. */
#define SyToUpper(c) (((c) >= 'a' && (c) <= 'z') ? ((c) - ('a' - 'A')) : (c))
#define SyToLower(c) (((c) >= 'A' && (c) <= 'Z') ? ((c) + ('a' - 'A')) : (c))
#define SyisUpper(c) isupper(c)
#define SyisLower(c) islower(c)
#define SyisSpace(c) isspace(c)
#define SyisBlank(c) isspace(c)
#define SyisAlpha(c) isalpha(c)
#define SyisDigit(c) isdigit(c)
#define SyisHex(c)   isxdigit(c)
#define SyisPrint(c) isprint(c)
#define SyisPunct(c) ispunct(c)
#define SyisSpec(c)  iscntrl(c)
#define SyisCtrl(c)  iscntrl(c)
#define SyisAscii(c) isascii(c)
#define SyisAlphaNum(c) isalnum(c)
#define SyisGraph(c)     isgraph(c)
#define SyDigToHex(c)    "0123456789ABCDEF"[c & 0x0F]
#define SyDigToInt(c)     ((c < 0xc0 && SyisDigit(c))? (c - '0') : 0 )
/* The `< 0xc0` guard these used to carry kept libc's locale fold off UTF-8 lead
 * bytes; an ASCII-only fold never touches them, so the guard is redundant. */
#define SyCharToUpper(c)  SyToUpper(c)
#define SyCharToLower(c)  SyToLower(c)

/* Remove white space/NUL byte from a raw string */
#define SyStringLeftTrim(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[0] < 0xc0 && SyisSpace((RAW)->zString[0])){\
		(RAW)->nByte--;\
		(RAW)->zString++;\
	}
#define SyStringLeftTrimSafe(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[0] < 0xc0 && ((RAW)->zString[0] == 0 || SyisSpace((RAW)->zString[0]))){\
		(RAW)->nByte--;\
		(RAW)->zString++;\
	}
#define SyStringRightTrim(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[(RAW)->nByte - 1] < 0xc0  && SyisSpace((RAW)->zString[(RAW)->nByte - 1])){\
		(RAW)->nByte--;\
	}
#define SyStringRightTrimSafe(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[(RAW)->nByte - 1] < 0xc0  && \
	(( RAW)->zString[(RAW)->nByte - 1] == 0 || SyisSpace((RAW)->zString[(RAW)->nByte - 1]))){\
		(RAW)->nByte--;\
	}

#define SyStringFullTrim(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[0] < 0xc0  && SyisSpace((RAW)->zString[0])){\
		(RAW)->nByte--;\
		(RAW)->zString++;\
	}\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[(RAW)->nByte - 1] < 0xc0  && SyisSpace((RAW)->zString[(RAW)->nByte - 1])){\
		(RAW)->nByte--;\
	}
#define SyStringFullTrimSafe(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[0] < 0xc0  && \
          ( (RAW)->zString[0] == 0 || SyisSpace((RAW)->zString[0]))){\
		(RAW)->nByte--;\
		(RAW)->zString++;\
	}\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[(RAW)->nByte - 1] < 0xc0  && \
                   ( (RAW)->zString[(RAW)->nByte - 1] == 0 || SyisSpace((RAW)->zString[(RAW)->nByte - 1]))){\
		(RAW)->nByte--;\
	}

#endif /* __SXMACROS_H__ */
