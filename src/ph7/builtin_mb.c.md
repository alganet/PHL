# src/ph7/builtin_mb.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 724/776 lines (93.30%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |    7 | `/*` |
|     - |    8 | ` * mb_* multibyte string functions, UTF-8 only (NEWPLAN band D; the recorded` |
|     - |    9 | ` * §10 scope cut — php's full encoding zoo is out). Codepoint semantics match` |
|     - |   10 | ` * php 8.5 byte-for-byte for UTF-8 input; case mapping is algorithmic over` |
|     - |   11 | ` * ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic (full Unicode tables` |
|     - |   12 | ` * recorded as a residual — unmapped codepoints pass through unchanged).` |
|     - |   13 | ` */` |
|     - |   14 |  |
|     - |   15 | `/* --- UTF-8 primitives ------------------------------------------------- */` |
|     - |   16 |  |
|     - |   17 | `/* Byte length of the ill-formed run at z[0..n-1]: the maximal prefix php's` |
|     - |   18 | ` * decoder consumes before giving up — the lead byte plus every continuation` |
|     - |   19 | ` * byte that is still in range for it. mbstring reports that whole prefix as ONE` |
|     - |   20 | ` * character and substitutes ONE '?' for it, so "\xe0\xa0" (a truncated 3-byte` |
|     - |   21 | ` * sequence) is one character, while "\xff\xfe" is two: neither byte can lead. */` |
|   452 |   22 | `static sxu32 MbUtf8BadLen(const unsigned char *z,sxu32 n)` |
|     1 |   23 | `{` |
|   453 |   24 | `	sxu32 c = z[0],need,iLow,iHigh,i;` |
|   453 |   25 | `	if( c >= 0xC2 && c <= 0xDF ){` |
|    57 |   26 | `		need = 2; iLow = 0x80; iHigh = 0xBF;` |
|   425 |   27 | `	}else if( c >= 0xE0 && c <= 0xEF ){` |
|    71 |   28 | `		need = 3; iLow = (c == 0xE0) ? 0xA0 : 0x80; iHigh = (c == 0xED) ? 0x9F : 0xBF;` |
|   362 |   29 | `	}else if( c >= 0xF0 && c <= 0xF4 ){` |
|    25 |   30 | `		need = 4; iLow = (c == 0xF0) ? 0x90 : 0x80; iHigh = (c == 0xF4) ? 0x8F : 0xBF;` |
|    13 |   31 | `	}else{` |
|   303 |   32 | `		return 1; /* 80..C1 or F5..FF: cannot lead anything */` |
|     - |   33 | `	}` |
|   233 |   34 | `	for( i = 1 ; i < need && i < n ; ++i ){` |
|   179 |   35 | `		sxu32 lo = (i == 1) ? iLow : 0x80;` |
|   179 |   36 | `		sxu32 hi = (i == 1) ? iHigh : 0xBF;` |
|   179 |   37 | `		if( z[i] < lo \|\| z[i] > hi ){` |
|    49 |   38 | `			break;` |
|     - |   39 | `		}` |
|    42 |   40 | `	}` |
|   151 |   41 | `	return i;` |
|   227 |   42 | `}` |
|     - |   43 | `/* Decode the character at z (n bytes available); *pLen = the bytes it occupies.` |
|     - |   44 | ` * Returns the codepoint, or -1 when the sequence is ILL-FORMED — in which case` |
|     - |   45 | ` * *pLen is the run above, which php's mbstring counts as one character and` |
|     - |   46 | ` * re-encodes as '?'.` |
|     - |   47 | ` *` |
|     - |   48 | ` * This used to be byte-transparent: an undecodable byte came back AS ITSELF` |
|     - |   49 | ` * with length 1, so mb_strtolower("\xff\xfe") answered the two bytes` |
|     - |   50 | ` * re-encoded as UTF-8 (\xc3\xbf\xc3\xbe) — latin-1 semantics php does not have,` |
|     - |   51 | ` * and characters PHL invented — where php answers "??". The over-long,` |
|     - |   52 | ` * surrogate and past-U+10FFFF forms were accepted as well; validation is` |
|     - |   53 | ` * PH7_Utf8ReadStrict's job now (the same reader json_encode uses). */` |
|  3368 |   54 | `static sxi32 MbUtf8Decode(const unsigned char *z,sxu32 n,sxu32 *pLen)` |
|     1 |   55 | `{` |
|  3369 |   56 | `	sxi32 iCp = PH7_Utf8ReadStrict(z,n,pLen);` |
|  3369 |   57 | `	if( iCp < 0 ){` |
|   453 |   58 | `		*pLen = MbUtf8BadLen(z,n);` |
|   226 |   59 | `	}` |
|  3369 |   60 | `	return iCp;` |
|     1 |   61 | `}` |
|     - |   62 | `/* Encode cp into z (up to 4 bytes); returns the byte count */` |
|   368 |   63 | `static sxu32 MbUtf8Encode(sxu32 cp,unsigned char *z)` |
|     1 |   64 | `{` |
|   369 |   65 | `	if( cp < 0x80 ){` |
|   201 |   66 | `		z[0] = (unsigned char)cp;` |
|   201 |   67 | `		return 1;` |
|     - |   68 | `	}` |
|   169 |   69 | `	if( cp < 0x800 ){` |
|   155 |   70 | `		z[0] = (unsigned char)(0xC0 \| (cp >> 6));` |
|   155 |   71 | `		z[1] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|   155 |   72 | `		return 2;` |
|     - |   73 | `	}` |
|    15 |   74 | `	if( cp < 0x10000 ){` |
|     9 |   75 | `		z[0] = (unsigned char)(0xE0 \| (cp >> 12));` |
|     9 |   76 | `		z[1] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|     9 |   77 | `		z[2] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|     9 |   78 | `		return 3;` |
|     - |   79 | `	}` |
|     7 |   80 | `	z[0] = (unsigned char)(0xF0 \| (cp >> 18));` |
|     7 |   81 | `	z[1] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|     7 |   82 | `	z[2] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|     7 |   83 | `	z[3] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|     7 |   84 | `	return 4;` |
|   185 |   85 | `}` |
|     - |   86 | `/* Codepoint count of a UTF-8 buffer */` |
|   226 |   87 | `static sxu32 MbUtf8Strlen(const char *zIn,sxu32 nByte)` |
|     1 |   88 | `{` |
|   227 |   89 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   227 |   90 | `	sxu32 i = 0,nCp = 0,nLen;` |
|  1327 |   91 | `	while( i < nByte ){` |
|  1101 |   92 | `		MbUtf8Decode(&z[i],nByte - i,&nLen);` |
|  1101 |   93 | `		i += nLen;` |
|  1101 |   94 | `		nCp++;` |
|     1 |   95 | `	}` |
|   227 |   96 | `	return nCp;` |
|     1 |   97 | `}` |
|     - |   98 | `/* Byte offset of codepoint index iCp (clamped to the buffer end) */` |
|   250 |   99 | `static sxu32 MbUtf8Skip(const char *zIn,sxu32 nByte,sxu32 iCp)` |
|     1 |  100 | `{` |
|   251 |  101 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   251 |  102 | `	sxu32 i = 0,nLen;` |
|   663 |  103 | `	while( i < nByte && iCp > 0 ){` |
|   413 |  104 | `		MbUtf8Decode(&z[i],nByte - i,&nLen);` |
|   413 |  105 | `		i += nLen;` |
|   413 |  106 | `		iCp--;` |
|     1 |  107 | `	}` |
|   251 |  108 | `	return i;` |
|     1 |  109 | `}` |
|     - |  110 |  |
|     - |  111 | `/* --- Case mapping (ASCII, Latin-1, Latin Ext-A, Greek, Cyrillic) ------ */` |
|     - |  112 |  |
|   202 |  113 | `static sxu32 MbToLower(sxu32 c)` |
|     1 |  114 | `{` |
|   203 |  115 | `	if( c < 0x80 ){ return (c >= 'A' && c <= 'Z') ? c + 0x20 : c; }` |
|   107 |  116 | `	if( c >= 0x00C0 && c <= 0x00DE && c != 0x00D7 ){ return c + 0x20; }` |
|    99 |  117 | `	if( c >= 0x0100 && c <= 0x0137 ){ return c \| 1; }` |
|    95 |  118 | `	if( c >= 0x0139 && c <= 0x0148 ){ return ((c - 1) \| 1) + 1; }` |
|    91 |  119 | `	if( c >= 0x014A && c <= 0x0177 ){ return c \| 1; }` |
|    91 |  120 | `	if( c == 0x0178 ){ return 0x00FF; }` |
|    91 |  121 | `	if( c >= 0x0179 && c <= 0x017E ){ return ((c - 1) \| 1) + 1; }` |
|    87 |  122 | `	if( c >= 0x0391 && c <= 0x03A9 && c != 0x03A2 ){ return c + 0x20; }` |
|    79 |  123 | `	if( c >= 0x0410 && c <= 0x042F ){ return c + 0x20; }` |
|    67 |  124 | `	if( c >= 0x0400 && c <= 0x040F ){ return c + 0x50; }` |
|    67 |  125 | `	return c;` |
|   102 |  126 | `}` |
|   120 |  127 | `static sxu32 MbToUpper(sxu32 c)` |
|     1 |  128 | `{` |
|   121 |  129 | `	if( c < 0x80 ){ return (c >= 'a' && c <= 'z') ? c - 0x20 : c; }` |
|    41 |  130 | `	if( c >= 0x00E0 && c <= 0x00FE && c != 0x00F7 ){ return c - 0x20; }` |
|    31 |  131 | `	if( c == 0x00FF ){ return 0x0178; }` |
|    31 |  132 | `	if( c >= 0x0100 && c <= 0x0137 ){ return c & ~(sxu32)1; }` |
|    29 |  133 | `	if( c >= 0x0139 && c <= 0x0148 ){ return ((c - 1) & ~(sxu32)1) + 1; }` |
|    27 |  134 | `	if( c >= 0x014A && c <= 0x0177 ){ return c & ~(sxu32)1; }` |
|    27 |  135 | `	if( c >= 0x0179 && c <= 0x017E ){ return ((c - 1) & ~(sxu32)1) + 1; }` |
|    25 |  136 | `	if( c == 0x017F ){ return 'S'; } /* long s */` |
|    25 |  137 | `	if( c >= 0x03B1 && c <= 0x03C9 && c != 0x03C2 ){ return c - 0x20; }` |
|    15 |  138 | `	if( c == 0x03C2 ){ return 0x03A3; } /* final sigma */` |
|    15 |  139 | `	if( c >= 0x0430 && c <= 0x044F ){ return c - 0x20; }` |
|     3 |  140 | `	if( c >= 0x0450 && c <= 0x045F ){ return c - 0x50; }` |
|     3 |  141 | `	return c;` |
|    61 |  142 | `}` |
|     - |  143 | `/* A codepoint counts as a letter for title-case word boundaries */` |
|    52 |  144 | `static int MbIsAlnum(sxu32 c)` |
|     1 |  145 | `{` |
|    53 |  146 | `	if( c < 0x80 ){` |
|    49 |  147 | `		return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= '0' && c <= '9');` |
|     - |  148 | `	}` |
|     - |  149 | `	/* non-ASCII letters: anything the case mapper knows, plus CJK & co —` |
|     - |  150 | `	 * treat every non-ASCII codepoint as a word character (php's word` |
|     - |  151 | `	 * boundary for TITLE mode is whitespace/punct, all ASCII) */` |
|     5 |  152 | `	return 1;` |
|    27 |  153 | `}` |
|     - |  154 |  |
|     - |  155 | `/* --- Shared argument handling ----------------------------------------- */` |
|     - |  156 |  |
|     - |  157 | `/* Validate the optional $encoding argument: UTF-8 aliases → 0, "8bit"-style` |
|     - |  158 | ` * byte encodings → 1, anything else raises php's ValueError and returns -1.` |
|     - |  159 | ` * (php accepts dozens of encodings; PHL's recorded scope is UTF-8 — a` |
|     - |  160 | ` * php-VALID encoding like SJIS gets the same loud ValueError.) */` |
|  4564 |  161 | `static int MbEncodingArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNo)` |
|     2 |  162 | `{` |
|     - |  163 | `	const char *zEnc;` |
|     - |  164 | `	int nEnc;` |
|  4566 |  165 | `	if( pArg == 0 \|\| ph7_value_is_null(pArg) ){` |
|   517 |  166 | `		return 0;` |
|     - |  167 | `	}` |
|  4050 |  168 | `	zEnc = ph7_value_to_string(pArg,&nEnc);` |
|  4048 |  169 | `	if( (nEnc == 5 && SyStrnicmp(zEnc,"UTF-8",5) == 0)` |
|  4042 |  170 | `	 \|\| (nEnc == 4 && SyStrnicmp(zEnc,"UTF8",4) == 0) ){` |
|    19 |  171 | `		return 0;` |
|     - |  172 | `	}` |
|  4030 |  173 | `	if( (nEnc == 4 && SyStrnicmp(zEnc,"8bit",4) == 0)` |
|  4028 |  174 | `	 \|\| (nEnc == 5 && SyStrnicmp(zEnc,"ASCII",5) == 0)` |
|  4026 |  175 | `	 \|\| (nEnc == 6 && SyStrnicmp(zEnc,"binary",6) == 0) ){` |
|     7 |  176 | `		return 1;` |
|     - |  177 | `	}` |
|  6038 |  178 | `	PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  179 | `		"%s(): Argument #%d ($encoding) must be a valid encoding, \"%.*s\" given",` |
|  2012 |  180 | `		zFunc,iArgNo,nEnc,zEnc);` |
|  4026 |  181 | `	return -1;` |
|  2284 |  182 | `}` |
|     - |  183 |  |
|     - |  184 | `/* --- The functions ----------------------------------------------------- */` |
|     - |  185 |  |
|     - |  186 | `/* int mb_strlen(string $string, ?string $encoding = null) */` |
|  4060 |  187 | `static int PH7_builtin_mb_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  188 | `{` |
|     - |  189 | `	const char *zIn;` |
|     - |  190 | `	int nByte,iEnc;` |
|  4062 |  191 | `	if( nArg < 1 ){` |
|   ! 0 |  192 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  193 | `		return PH7_OK;` |
|     - |  194 | `	}` |
|  4062 |  195 | `	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_strlen",2);` |
|  4062 |  196 | `	if( iEnc < 0 ){` |
|  4018 |  197 | `		return PH7_OK;` |
|     - |  198 | `	}` |
|    45 |  199 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    66 |  200 | `	ph7_result_int64(pCtx,iEnc == 1 ? (ph7_int64)nByte` |
|    42 |  201 | `		: (ph7_int64)MbUtf8Strlen(zIn,(sxu32)nByte));` |
|    45 |  202 | `	return PH7_OK;` |
|  2032 |  203 | `}` |
|     - |  204 | `/* Set the call result to zIn[0..nByte-1] with every ill-formed run replaced by` |
|     - |  205 | ` * '?', php's substitution character. A well-formed buffer copies verbatim. */` |
|   174 |  206 | `static void MbResultSubstituted(ph7_context *pCtx,const char *zIn,sxu32 nByte)` |
|     1 |  207 | `{` |
|   175 |  208 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|     - |  209 | `	SyBlob sOut;` |
|   175 |  210 | `	sxu32 i = 0,nLen;` |
|     - |  211 | `	/* Well-formed is the overwhelmingly common case: check first and hand back` |
|     - |  212 | `	 * the buffer as it stands rather than rebuilding it. */` |
|   495 |  213 | `	while( i < nByte && MbUtf8Decode(&z[i],nByte - i,&nLen) >= 0 ){` |
|   321 |  214 | `		i += nLen;` |
|     1 |  215 | `	}` |
|   175 |  216 | `	if( i >= nByte ){` |
|   145 |  217 | `		ph7_result_string(pCtx,zIn,(int)nByte);` |
|   145 |  218 | `		return;` |
|     - |  219 | `	}` |
|    31 |  220 | `	i = 0;` |
|    31 |  221 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   103 |  222 | `	while( i < nByte ){` |
|    73 |  223 | `		if( MbUtf8Decode(&z[i],nByte - i,&nLen) < 0 ){` |
|    49 |  224 | `			SyBlobAppend(&sOut,"?",1);` |
|    25 |  225 | `		}else{` |
|    25 |  226 | `			SyBlobAppend(&sOut,&z[i],nLen);` |
|     - |  227 | `		}` |
|    73 |  228 | `		i += nLen;` |
|     1 |  229 | `	}` |
|    31 |  230 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    31 |  231 | `	SyBlobRelease(&sOut);` |
|    88 |  232 | `}` |
|     - |  233 | `/* string mb_substr(string $string, int $start, ?int $length = null,` |
|     - |  234 | ` *                  ?string $encoding = null) */` |
|    88 |  235 | `static int PH7_builtin_mb_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  236 | `{` |
|     - |  237 | `	const char *zIn;` |
|     - |  238 | `	int nByte,iEnc;` |
|     - |  239 | `	sxi64 iStart,iLen;` |
|     - |  240 | `	sxu32 nCp,iOfft,iEnd;` |
|    89 |  241 | `	int bLenSet = 0;` |
|    89 |  242 | `	if( nArg < 2 ){` |
|   ! 0 |  243 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  244 | `		return PH7_OK;` |
|     - |  245 | `	}` |
|    89 |  246 | `	iEnc = MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,"mb_substr",4);` |
|    89 |  247 | `	if( iEnc < 0 ){` |
|     3 |  248 | `		return PH7_OK;` |
|     - |  249 | `	}` |
|    87 |  250 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    87 |  251 | `	iStart = ph7_value_to_int64(apArg[1]);` |
|    87 |  252 | `	iLen = 0;` |
|    87 |  253 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|    57 |  254 | `		iLen = ph7_value_to_int64(apArg[2]);` |
|    57 |  255 | `		bLenSet = 1;` |
|    28 |  256 | `	}` |
|    87 |  257 | `	nCp = (iEnc == 1) ? (sxu32)nByte : MbUtf8Strlen(zIn,(sxu32)nByte);` |
|    87 |  258 | `	if( iStart < 0 ){` |
|     3 |  259 | `		iStart = (sxi64)nCp + iStart;` |
|     3 |  260 | `		if( iStart < 0 ){ iStart = 0; }` |
|     1 |  261 | `	}` |
|    87 |  262 | `	if( iStart >= (sxi64)nCp ){` |
|     7 |  263 | `		ph7_result_string(pCtx,"",0);` |
|     7 |  264 | `		return PH7_OK;` |
|     - |  265 | `	}` |
|    81 |  266 | `	if( !bLenSet ){` |
|    25 |  267 | `		iLen = (sxi64)nCp - iStart;` |
|    69 |  268 | `	}else if( iLen < 0 ){` |
|     3 |  269 | `		iLen = ((sxi64)nCp - iStart) + iLen;` |
|     3 |  270 | `		if( iLen < 0 ){ iLen = 0; }` |
|     1 |  271 | `	}` |
|    81 |  272 | `	if( iStart + iLen > (sxi64)nCp ){` |
|    27 |  273 | `		iLen = (sxi64)nCp - iStart;` |
|    13 |  274 | `	}` |
|    81 |  275 | `	if( iEnc == 1 ){` |
|   ! 0 |  276 | `		ph7_result_string(pCtx,&zIn[iStart],(int)iLen);` |
|   ! 0 |  277 | `		return PH7_OK;` |
|     - |  278 | `	}` |
|    81 |  279 | `	iOfft = MbUtf8Skip(zIn,(sxu32)nByte,(sxu32)iStart);` |
|    81 |  280 | `	iEnd  = iOfft + MbUtf8Skip(&zIn[iOfft],(sxu32)nByte - iOfft,(sxu32)iLen);` |
|     - |  281 | `	/* php decodes and re-encodes the slice rather than copying its bytes, so an` |
|     - |  282 | `	 * undecodable run inside it comes out as '?' — mb_substr("ab\xffcd",2,1) is` |
|     - |  283 | `	 * "?", not the raw \xff PHL used to hand back. */` |
|    81 |  284 | `	MbResultSubstituted(pCtx,&zIn[iOfft],iEnd - iOfft);` |
|    81 |  285 | `	return PH7_OK;` |
|    45 |  286 | `}` |
|     - |  287 | `/* Shared case transform: iMode 0 = lower, 1 = upper, 2 = title */` |
|   122 |  288 | `static int MbCaseTransform(ph7_context *pCtx,const char *zIn,sxu32 nByte,int iMode)` |
|     1 |  289 | `{` |
|     - |  290 | `	SyBlob sOut;` |
|   123 |  291 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   123 |  292 | `	sxu32 i = 0,nLen,cp,mapped;` |
|     - |  293 | `	unsigned char zEnc[4];` |
|   123 |  294 | `	int bWordStart = 1;` |
|   123 |  295 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   477 |  296 | `	while( i < nByte ){` |
|   355 |  297 | `		sxi32 iCp = MbUtf8Decode(&z[i],nByte - i,&nLen);` |
|   355 |  298 | `		i += nLen;` |
|   355 |  299 | `		if( iCp < 0 ){` |
|     - |  300 | `			/* php substitutes '?' for an undecodable run. For TITLE mode the run` |
|     - |  301 | `			 * is neither a word character nor a separator — it leaves the` |
|     - |  302 | `			 * word-start flag exactly as it found it, so "\xffab" titles to` |
|     - |  303 | `			 * "?Ab" (the run did not open a word, 'a' still does) while` |
|     - |  304 | `			 * "a\xffb" titles to "A?b" ('b' is still mid-word). */` |
|   125 |  305 | `			SyBlobAppend(&sOut,"?",1);` |
|   125 |  306 | `			continue;` |
|     - |  307 | `		}` |
|   231 |  308 | `		cp = (sxu32)iCp;` |
|   231 |  309 | `		if( iMode == 1 ){` |
|   109 |  310 | `			if( cp == 0x00DF ){ /* php: mb_strtoupper('ß') === 'SS' */` |
|     3 |  311 | `				SyBlobAppend(&sOut,"SS",2);` |
|     3 |  312 | `				continue;` |
|     - |  313 | `			}` |
|   107 |  314 | `			mapped = MbToUpper(cp);` |
|   176 |  315 | `		}else if( iMode == 0 ){` |
|    71 |  316 | `			if( cp == 0x03A3 ){` |
|     - |  317 | `				/* Greek capital sigma: final position lowers to ς, else σ */` |
|     - |  318 | `				sxu32 nPeek;` |
|     3 |  319 | `				sxi32 iNext = -1;` |
|     3 |  320 | `				if( i < nByte ){` |
|   ! 0 |  321 | `					iNext = MbUtf8Decode(&z[i],nByte - i,&nPeek);` |
|   ! 0 |  322 | `				}` |
|     3 |  323 | `				mapped = (iNext < 0 \|\| !MbIsAlnum((sxu32)iNext) ) ? 0x03C2 : 0x03C3;` |
|     2 |  324 | `			}else{` |
|    69 |  325 | `				mapped = MbToLower(cp);` |
|     - |  326 | `			}` |
|    36 |  327 | `		}else{` |
|    53 |  328 | `			if( MbIsAlnum(cp) ){` |
|    47 |  329 | `				mapped = bWordStart ? MbToUpper(cp) : MbToLower(cp);` |
|    47 |  330 | `				bWordStart = 0;` |
|    24 |  331 | `			}else{` |
|     7 |  332 | `				mapped = cp;` |
|     7 |  333 | `				bWordStart = 1;` |
|     - |  334 | `			}` |
|     - |  335 | `		}` |
|   229 |  336 | `		SyBlobAppend(&sOut,zEnc,MbUtf8Encode(mapped,zEnc));` |
|     1 |  337 | `	}` |
|   123 |  338 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   123 |  339 | `	SyBlobRelease(&sOut);` |
|   123 |  340 | `	return PH7_OK;` |
|     1 |  341 | `}` |
|     - |  342 | `/* string mb_strtolower/mb_strtoupper(string $string, ?string $encoding) */` |
|    88 |  343 | `static int PH7_builtin_mb_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  344 | `{` |
|     - |  345 | `	const char *zIn,*zFunc;` |
|     - |  346 | `	int nByte;` |
|    89 |  347 | `	if( nArg < 1 ){` |
|   ! 0 |  348 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  349 | `		return PH7_OK;` |
|     - |  350 | `	}` |
|    89 |  351 | `	zFunc = ph7_function_name(pCtx);` |
|    89 |  352 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,zFunc,2) < 0 ){` |
|   ! 0 |  353 | `		return PH7_OK;` |
|     - |  354 | `	}` |
|    89 |  355 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   133 |  356 | `	return MbCaseTransform(pCtx,zIn,(sxu32)nByte,` |
|    88 |  357 | `		zFunc[sizeof("mb_strto")-1] == 'u' ? 1 : 0); /* mb_strtoUpper */` |
|    45 |  358 | `}` |
|     - |  359 | `/* string mb_convert_case(string $string, int $mode, ?string $encoding) */` |
|    34 |  360 | `static int PH7_builtin_mb_convert_case(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  361 | `{` |
|     - |  362 | `	const char *zIn;` |
|     - |  363 | `	int nByte,iMode;` |
|    35 |  364 | `	if( nArg < 2 ){` |
|   ! 0 |  365 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  366 | `		return PH7_OK;` |
|     - |  367 | `	}` |
|    35 |  368 | `	if( MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_convert_case",3) < 0 ){` |
|   ! 0 |  369 | `		return PH7_OK;` |
|     - |  370 | `	}` |
|    35 |  371 | `	iMode = ph7_value_to_int(apArg[1]);` |
|    35 |  372 | `	if( iMode < 0 \|\| iMode > 2 ){` |
|     - |  373 | `		/* php has FOLD/SIMPLE variants 3-7; PHL's recorded scope is 0-2 */` |
|   ! 0 |  374 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  375 | `			"mb_convert_case(): Argument #2 ($mode) must be one of the MB_CASE_* constants");` |
|     - |  376 | `	}` |
|    35 |  377 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|     - |  378 | `	/* php: MB_CASE_UPPER=0, MB_CASE_LOWER=1, MB_CASE_TITLE=2 */` |
|    68 |  379 | `	return MbCaseTransform(pCtx,zIn,(sxu32)nByte,` |
|    33 |  380 | `		iMode == 0 ? 1 : (iMode == 1 ? 0 : 2));` |
|    18 |  381 | `}` |
|     - |  382 | `/* Shared search core: returns the codepoint index or -1.` |
|     - |  383 | ` *` |
|     - |  384 | ` * iOfftCp is the LOWEST codepoint index a match may start at; iMaxCp the highest,` |
|     - |  385 | ` * or -1 for no upper bound. The pair is php's asymmetric strrpos rule (a negative` |
|     - |  386 | ` * $offset is an upper bound counted back from the end, a non-negative one a lower` |
|     - |  387 | ` * bound) — the same split StrRSearchWindow() applies to the 8-bit family. */` |
|    36 |  388 | `static sxi64 MbSearch(const char *zH,sxu32 nH,const char *zN,sxu32 nN,` |
|     - |  389 | `	sxi64 iOfftCp,sxi64 iMaxCp,int bCaseFold,int bReverse,ph7_context *pCtx)` |
|     1 |  390 | `{` |
|     - |  391 | `	sxu32 iByte,i;` |
|     - |  392 | `	SyBlob sFh,sFn;` |
|    37 |  393 | `	const char *zHay = zH,*zNee = zN;` |
|    37 |  394 | `	sxi64 iFound = -1;` |
|    37 |  395 | `	if( nN == 0 \|\| nN > nH ){` |
|   ! 0 |  396 | `		return -1;` |
|     - |  397 | `	}` |
|    37 |  398 | `	if( bCaseFold ){` |
|     - |  399 | `		/* fold both through the case mapper */` |
|     - |  400 | `		const unsigned char *z;` |
|     - |  401 | `		sxu32 k,nLen;` |
|     - |  402 | `		sxi32 iCp;` |
|     - |  403 | `		unsigned char zEnc[4];` |
|    11 |  404 | `		SyBlobInit(&sFh,&pCtx->pVm->sAllocator);` |
|    11 |  405 | `		SyBlobInit(&sFn,&pCtx->pVm->sAllocator);` |
|    11 |  406 | `		z = (const unsigned char *)zH;` |
|    95 |  407 | `		for( k = 0 ; k < nH ; ){` |
|    85 |  408 | `			iCp = MbUtf8Decode(&z[k],nH - k,&nLen);` |
|    85 |  409 | `			k += nLen;` |
|    85 |  410 | `			if( iCp < 0 ){ SyBlobAppend(&sFh,"?",1); continue; }` |
|    85 |  411 | `			SyBlobAppend(&sFh,zEnc,MbUtf8Encode(MbToLower((sxu32)iCp),zEnc));` |
|     1 |  412 | `		}` |
|    11 |  413 | `		z = (const unsigned char *)zN;` |
|    29 |  414 | `		for( k = 0 ; k < nN ; ){` |
|    19 |  415 | `			iCp = MbUtf8Decode(&z[k],nN - k,&nLen);` |
|    19 |  416 | `			k += nLen;` |
|    19 |  417 | `			if( iCp < 0 ){ SyBlobAppend(&sFn,"?",1); continue; }` |
|    19 |  418 | `			SyBlobAppend(&sFn,zEnc,MbUtf8Encode(MbToLower((sxu32)iCp),zEnc));` |
|     1 |  419 | `		}` |
|    11 |  420 | `		zHay = (const char *)SyBlobData(&sFh);` |
|    11 |  421 | `		nH = SyBlobLength(&sFh);` |
|    11 |  422 | `		zNee = (const char *)SyBlobData(&sFn);` |
|    11 |  423 | `		nN = SyBlobLength(&sFn);` |
|     5 |  424 | `	}` |
|    37 |  425 | `	iByte = MbUtf8Skip(zHay,nH,(sxu32)(iOfftCp > 0 ? iOfftCp : 0));` |
|   177 |  426 | `	for( i = iByte ; i + nN <= nH ; ){` |
|   159 |  427 | `		if( SyMemcmp(&zHay[i],zNee,nN) == 0 ){` |
|    27 |  428 | `			sxi64 iAt = (sxi64)MbUtf8Strlen(zHay,i);` |
|    27 |  429 | `			if( iMaxCp >= 0 && iAt > iMaxCp ){` |
|     3 |  430 | `				break; /* past the window's upper bound; nothing later qualifies */` |
|     - |  431 | `			}` |
|    25 |  432 | `			iFound = iAt;` |
|    25 |  433 | `			if( !bReverse ){` |
|    17 |  434 | `				break;` |
|     - |  435 | `			}` |
|     - |  436 | `			/* keep scanning for the last hit */` |
|     4 |  437 | `		}` |
|     - |  438 | `		{` |
|     - |  439 | `			sxu32 nStep;` |
|   141 |  440 | `			MbUtf8Decode((const unsigned char *)&zHay[i],nH - i,&nStep);` |
|   141 |  441 | `			i += nStep;` |
|     - |  442 | `		}` |
|     1 |  443 | `	}` |
|    37 |  444 | `	if( bCaseFold ){` |
|    11 |  445 | `		SyBlobRelease(&sFh);` |
|    11 |  446 | `		SyBlobRelease(&sFn);` |
|     5 |  447 | `	}` |
|    37 |  448 | `	return iFound;` |
|    19 |  449 | `}` |
|     - |  450 | `/* mb_strpos / mb_stripos / mb_strrpos */` |
|    84 |  451 | `static int PH7_builtin_mb_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  452 | `{` |
|     - |  453 | `	const char *zH,*zN,*zFunc;` |
|     - |  454 | `	int nH,nN;` |
|    85 |  455 | `	sxi64 iOfft = 0,iMax = -1,iPos,nCp;` |
|     - |  456 | `	int bFold,bRev;` |
|    85 |  457 | `	if( nArg < 2 ){` |
|   ! 0 |  458 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  459 | `		return PH7_OK;` |
|     - |  460 | `	}` |
|    85 |  461 | `	zFunc = ph7_function_name(pCtx);` |
|    85 |  462 | `	bFold = zFunc[sizeof("mb_str")-1] == 'i';   /* mb_strIpos */` |
|    85 |  463 | `	bRev  = zFunc[sizeof("mb_str")-1] == 'r';   /* mb_strRpos */` |
|    85 |  464 | `	if( MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,zFunc,4) < 0 ){` |
|   ! 0 |  465 | `		return PH7_OK;` |
|     - |  466 | `	}` |
|    85 |  467 | `	zH = ph7_value_to_string(apArg[0],&nH);` |
|    85 |  468 | `	zN = ph7_value_to_string(apArg[1],&nN);` |
|    85 |  469 | `	nCp = (sxi64)MbUtf8Strlen(zH,(sxu32)nH);` |
|    85 |  470 | `	if( nArg > 2 ){` |
|    69 |  471 | `		iOfft = ph7_value_to_int64(apArg[2]);` |
|     - |  472 | `		/* php requires -strlen <= $offset <= strlen, in CODE POINTS here, and` |
|     - |  473 | `		 * raises rather than answering "not found" — the same rule the 8-bit` |
|     - |  474 | `		 * family got, which these three were left out of: mb_strpos("abc","c",7)` |
|     - |  475 | `		 * answered false, and false is what a genuine miss answers too. */` |
|    69 |  476 | `		if( iOfft < 0 ? (iOfft < -nCp) : (iOfft > nCp) ){` |
|    43 |  477 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  478 | `				"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",` |
|    18 |  479 | `				zFunc);` |
|     - |  480 | `		}` |
|    45 |  481 | `		if( bRev ){` |
|     - |  482 | `			/* A negative offset is an UPPER bound on where the match may START,` |
|     - |  483 | `			 * not a start position: mb_strrpos("áéíóú","í",-1) is 2 in php, where` |
|     - |  484 | `			 * counting it forward answered false — and -5 is php's false, where` |
|     - |  485 | `			 * counting it forward answered 2. */` |
|    15 |  486 | `			if( iOfft < 0 ){` |
|     7 |  487 | `				iMax = nCp + iOfft;` |
|     7 |  488 | `				iOfft = 0;` |
|     4 |  489 | `			}` |
|    38 |  490 | `		}else if( iOfft < 0 ){` |
|    13 |  491 | `			iOfft = nCp + iOfft;` |
|     6 |  492 | `		}` |
|    22 |  493 | `	}` |
|    61 |  494 | `	if( nN == 0 ){` |
|     - |  495 | `		/* php 8 matches an EMPTY needle at the offset itself (and, searching` |
|     - |  496 | `` 		 * backwards, at the last position the window allows) — `mb_strpos("abc","")` `` |
|     - |  497 | ``		 * is 0 and `mb_strrpos("abc","")` is 3. These three answered false, which is`` |
|     - |  498 | `		 * also what a genuine miss answers; the 8-bit family already had the rule. */` |
|    25 |  499 | `		iPos = bRev ? (iMax >= 0 ? iMax : nCp) : iOfft;` |
|    25 |  500 | `		if( iPos > nCp ){` |
|   ! 0 |  501 | `			iPos = nCp;` |
|   ! 0 |  502 | `		}` |
|    25 |  503 | `		if( iPos < iOfft ){` |
|   ! 0 |  504 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  505 | `			return PH7_OK;` |
|     - |  506 | `		}` |
|    25 |  507 | `		ph7_result_int64(pCtx,iPos);` |
|    25 |  508 | `		return PH7_OK;` |
|     - |  509 | `	}` |
|    37 |  510 | `	iPos = MbSearch(zH,(sxu32)nH,zN,(sxu32)nN,iOfft,iMax,bFold,bRev,pCtx);` |
|    37 |  511 | `	if( iPos < 0 ){` |
|    15 |  512 | `		ph7_result_bool(pCtx,0);` |
|     8 |  513 | `	}else{` |
|    23 |  514 | `		ph7_result_int64(pCtx,iPos);` |
|     - |  515 | `	}` |
|    37 |  516 | `	return PH7_OK;` |
|    37 |  517 | `}` |
|     - |  518 | `/* array mb_str_split(string $string, int $length = 1, ?string $encoding) */` |
|    26 |  519 | `static int PH7_builtin_mb_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  520 | `{` |
|     - |  521 | `	const char *zIn;` |
|     - |  522 | `	int nByte;` |
|    27 |  523 | `	sxi64 iChunk = 1;` |
|     - |  524 | `	ph7_value *pArr,*pV;` |
|     - |  525 | `	sxu32 i;` |
|    27 |  526 | `	if( nArg < 1 ){` |
|   ! 0 |  527 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  528 | `		return PH7_OK;` |
|     - |  529 | `	}` |
|    27 |  530 | `	if( MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_str_split",3) < 0 ){` |
|     3 |  531 | `		return PH7_OK;` |
|     - |  532 | `	}` |
|    25 |  533 | `	if( nArg > 1 ){` |
|     7 |  534 | `		iChunk = ph7_value_to_int64(apArg[1]);` |
|     7 |  535 | `		if( iChunk < 1 ){` |
|     3 |  536 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  537 | `				"mb_str_split(): Argument #2 ($length) must be greater than 0");` |
|     - |  538 | `		}` |
|     2 |  539 | `	}` |
|    23 |  540 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    23 |  541 | `	pArr = ph7_context_new_array(pCtx);` |
|    23 |  542 | `	pV = ph7_context_new_scalar(pCtx);` |
|    23 |  543 | `	if( pArr == 0 \|\| pV == 0 ){` |
|   ! 0 |  544 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  545 | `	}` |
|    77 |  546 | `	for( i = 0 ; i < (sxu32)nByte ; ){` |
|    55 |  547 | `		sxu32 iEnd = i + MbUtf8Skip(&zIn[i],(sxu32)nByte - i,(sxu32)iChunk);` |
|    55 |  548 | `		ph7_value_string(pV,&zIn[i],(int)(iEnd - i));` |
|    55 |  549 | `		ph7_array_add_elem(pArr,0,pV);` |
|    55 |  550 | `		ph7_value_reset_string_cursor(pV);` |
|    55 |  551 | `		i = iEnd;` |
|     1 |  552 | `	}` |
|    23 |  553 | `	ph7_result_value(pCtx,pArr);` |
|    23 |  554 | `	return PH7_OK;` |
|    14 |  555 | `}` |
|     - |  556 | `/* --- mb_trim / mb_ltrim / mb_rtrim (php 8.4) --------------------------- */` |
|     - |  557 |  |
|     - |  558 | `#define MB_TRIM_LEFT  1` |
|     - |  559 | `#define MB_TRIM_RIGHT 2` |
|     - |  560 |  |
|     - |  561 | `/*` |
|     - |  562 | ` * The character set a trim walks against. php builds a hash of code points;` |
|     - |  563 | ` * this splits it in two so the common case costs nothing: a 256-bit map for` |
|     - |  564 | ` * everything below U+0100 (which is where a hand-written trim set almost` |
|     - |  565 | ` * always lives) and a linear array for the rest, whose length is the number of` |
|     - |  566 | ` * DISTINCT high code points in $characters. php's own fast path is a linear` |
|     - |  567 | ` * scan of up to four, so the shape is not a departure.` |
|     - |  568 | ` */` |
|     - |  569 | `typedef struct mb_trim_set mb_trim_set;` |
|     - |  570 | `struct mb_trim_set {` |
|     - |  571 | `	unsigned char aLow[32];   /* bitmap of U+0000 .. U+00FF */` |
|     - |  572 | `	sxu32 *aHigh;             /* the rest, in encounter order */` |
|     - |  573 | `	sxu32 nHigh;` |
|     - |  574 | `	sxu32 nAlloc;` |
|     - |  575 | `};` |
|  2116 |  576 | `static int MbTrimSetAdd(ph7_context *pCtx,mb_trim_set *pSet,sxu32 cp)` |
|     1 |  577 | `{` |
|     - |  578 | `	sxu32 i;` |
|  2117 |  579 | `	if( cp < 256 ){` |
|   729 |  580 | `		pSet->aLow[cp >> 3] \|= (unsigned char)(1 << (cp & 7));` |
|   729 |  581 | `		return PH7_OK;` |
|     - |  582 | `	}` |
| 13017 |  583 | `	for( i = 0 ; i < pSet->nHigh ; ++i ){` |
| 11629 |  584 | `		if( pSet->aHigh[i] == cp ){` |
|   ! 0 |  585 | `			return PH7_OK;` |
|     - |  586 | `		}` |
|  5815 |  587 | `	}` |
|  1389 |  588 | `	if( pSet->nHigh >= pSet->nAlloc ){` |
|   173 |  589 | `		sxu32 nNew = pSet->nAlloc ? pSet->nAlloc * 2 : 16;` |
|   259 |  590 | `		sxu32 *aNew = (sxu32 *)ph7_context_alloc_chunk(pCtx,` |
|    86 |  591 | `			(unsigned int)(nNew * sizeof(sxu32)),FALSE,TRUE);` |
|   173 |  592 | `		if( aNew == 0 ){` |
|   ! 0 |  593 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |  594 | `		}` |
|   173 |  595 | `		if( pSet->nHigh > 0 ){` |
|    77 |  596 | `			SyMemcpy(pSet->aHigh,aNew,pSet->nHigh * (sxu32)sizeof(sxu32));` |
|    38 |  597 | `		}` |
|   173 |  598 | `		pSet->aHigh = aNew;` |
|   173 |  599 | `		pSet->nAlloc = nNew;` |
|    86 |  600 | `	}` |
|  1389 |  601 | `	pSet->aHigh[pSet->nHigh++] = cp;` |
|  1389 |  602 | `	return PH7_OK;` |
|  1059 |  603 | `}` |
|   556 |  604 | `static int MbTrimSetHas(const mb_trim_set *pSet,sxu32 cp)` |
|     1 |  605 | `{` |
|     - |  606 | `	sxu32 i;` |
|   557 |  607 | `	if( cp < 256 ){` |
|   451 |  608 | `		return (pSet->aLow[cp >> 3] & (1 << (cp & 7))) != 0;` |
|     - |  609 | `	}` |
|  1193 |  610 | `	for( i = 0 ; i < pSet->nHigh ; ++i ){` |
|  1157 |  611 | `		if( pSet->aHigh[i] == cp ){` |
|    71 |  612 | `			return 1;` |
|     - |  613 | `		}` |
|   544 |  614 | `	}` |
|    37 |  615 | `	return 0;` |
|   279 |  616 | `}` |
|     - |  617 | `/*` |
|     - |  618 | ` * string mb_trim(string $string, ?string $characters = null, ?string $encoding = null)` |
|     - |  619 | ` * string mb_ltrim(...) / string mb_rtrim(...)` |
|     - |  620 | `` *  Strip whole CHARACTERS -- there is no `a..z` range syntax here, unlike`` |
|     - |  621 | ` *  trim() -- from one or both ends, defaulting to php's Unicode whitespace set.` |
|     - |  622 | ` *  Where a chunk implementation compared the encoded bytes, this decodes: an` |
|     - |  623 | ` *  ill-formed run is ONE character that compares equal to every other` |
|     - |  624 | ` *  ill-formed run, which is what makes mb_trim("\xff\xfeab\xff", "\xff")` |
|     - |  625 | ` *  answer "ab" rather than leaving the bytes it could not read in place.` |
|     - |  626 | ` */` |
|   132 |  627 | `static int PH7_builtin_mb_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  628 | `{` |
|     - |  629 | `	/* php's trim_default_chars[], in its own order (mb_trim_default_chars()) */` |
|     - |  630 | `	static const sxu32 aDefault[] = {` |
|     - |  631 | `		0x20, 0x0C, 0x0A, 0x0D, 0x09, 0x0B, 0x00, 0xA0, 0x1680,` |
|     - |  632 | `		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007,` |
|     - |  633 | `		0x2008, 0x2009, 0x200A, 0x2028, 0x2029, 0x202F, 0x205F, 0x3000,` |
|     - |  634 | `		0x85, 0x180E` |
|     - |  635 | `	};` |
|   133 |  636 | `	const char *zFunc = ph7_function_name(pCtx);` |
|     - |  637 | `	const char *zIn;` |
|     - |  638 | `	mb_trim_set sSet;` |
|     - |  639 | `	int nByte,iEnc,iMode;` |
|     - |  640 | `	sxu32 i,iLeft,iRight,nLen;` |
|   133 |  641 | `	if( nArg < 1 ){` |
|   ! 0 |  642 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  643 | `		return PH7_OK;` |
|     - |  644 | `	}` |
|     - |  645 | `	/* One body, three names: "mb_\|l\|trim" and "mb_\|r\|trim" against "mb_\|t\|rim". */` |
|   199 |  646 | `	iMode = (zFunc[3] == 'l') ? MB_TRIM_LEFT` |
|   115 |  647 | `		: ((zFunc[3] == 'r') ? MB_TRIM_RIGHT : (MB_TRIM_LEFT\|MB_TRIM_RIGHT));` |
|   133 |  648 | `	iEnc = MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,zFunc,3);` |
|   133 |  649 | `	if( iEnc < 0 ){` |
|     3 |  650 | `		return PH7_OK;` |
|     - |  651 | `	}` |
|   131 |  652 | `	SyZero(&sSet,sizeof(sSet));` |
|   158 |  653 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|    55 |  654 | `		const char *zWhat = ph7_value_to_string(apArg[1],&nByte);` |
|   119 |  655 | `		for( i = 0 ; i < (sxu32)nByte ; i += nLen ){` |
|    65 |  656 | `			sxi32 cp = MbUtf8Decode((const unsigned char *)&zWhat[i],(sxu32)nByte - i,&nLen);` |
|    65 |  657 | `			if( iEnc == 1 ){` |
|     5 |  658 | `				cp = (unsigned char)zWhat[i];` |
|     5 |  659 | `				nLen = 1;` |
|     2 |  660 | `			}` |
|     - |  661 | `			/* Every ill-formed run decodes to the same member, php's error` |
|     - |  662 | `			 * marker, so one bad byte in $characters strips them all. */` |
|    65 |  663 | `			if( MbTrimSetAdd(pCtx,&sSet,cp < 0 ? 0xFFFFFFFF : (sxu32)cp) != PH7_OK ){` |
|   ! 0 |  664 | `				return PH7_ContextMemoryError(pCtx);` |
|     - |  665 | `			}` |
|    33 |  666 | `		}` |
|    28 |  667 | `	}else{` |
|  2129 |  668 | `		for( i = 0 ; i < SX_ARRAYSIZE(aDefault) ; ++i ){` |
|  2053 |  669 | `			if( MbTrimSetAdd(pCtx,&sSet,aDefault[i]) != PH7_OK ){` |
|   ! 0 |  670 | `				return PH7_ContextMemoryError(pCtx);` |
|     - |  671 | `			}` |
|  1027 |  672 | `		}` |
|     - |  673 | `	}` |
|   131 |  674 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   131 |  675 | `	iLeft = 0;` |
|   131 |  676 | `	iRight = (sxu32)nByte;` |
|   131 |  677 | `	if( iMode & MB_TRIM_LEFT ){` |
|   243 |  678 | `		while( iLeft < iRight ){` |
|   225 |  679 | `			sxi32 cp = MbUtf8Decode((const unsigned char *)&zIn[iLeft],iRight - iLeft,&nLen);` |
|   225 |  680 | `			if( iEnc == 1 ){` |
|     5 |  681 | `				cp = (unsigned char)zIn[iLeft];` |
|     5 |  682 | `				nLen = 1;` |
|     2 |  683 | `			}` |
|   225 |  684 | `			if( !MbTrimSetHas(&sSet,cp < 0 ? 0xFFFFFFFF : (sxu32)cp) ){` |
|    77 |  685 | `				break;` |
|     - |  686 | `			}` |
|   149 |  687 | `			iLeft += nLen;` |
|     1 |  688 | `		}` |
|    47 |  689 | `	}` |
|   131 |  690 | `	if( iMode & MB_TRIM_RIGHT ){` |
|     - |  691 | `		/* UTF-8 has no backwards reader here, so re-walk from the left edge and` |
|     - |  692 | `		 * keep the offset where the CURRENT run of trim characters began; the` |
|     - |  693 | `		 * last one still open when the walk ends is the trailing run. */` |
|    97 |  694 | `		sxu32 iRun = iRight;` |
|    97 |  695 | `		int bInRun = 0;` |
|   429 |  696 | `		for( i = iLeft ; i < iRight ; i += nLen ){` |
|   333 |  697 | `			sxi32 cp = MbUtf8Decode((const unsigned char *)&zIn[i],iRight - i,&nLen);` |
|   333 |  698 | `			if( iEnc == 1 ){` |
|     9 |  699 | `				cp = (unsigned char)zIn[i];` |
|     9 |  700 | `				nLen = 1;` |
|     4 |  701 | `			}` |
|   333 |  702 | `			if( MbTrimSetHas(&sSet,cp < 0 ? 0xFFFFFFFF : (sxu32)cp) ){` |
|   149 |  703 | `				if( !bInRun ){` |
|    87 |  704 | `					iRun = i;` |
|    87 |  705 | `					bInRun = 1;` |
|    43 |  706 | `				}` |
|    75 |  707 | `			}else{` |
|   185 |  708 | `				bInRun = 0;` |
|     - |  709 | `			}` |
|   167 |  710 | `		}` |
|    97 |  711 | `		if( bInRun ){` |
|    65 |  712 | `			iRight = iRun;` |
|    32 |  713 | `		}` |
|    48 |  714 | `	}` |
|   131 |  715 | `	if( iEnc == 1 \|\| (iLeft == 0 && iRight == (sxu32)nByte) ){` |
|     - |  716 | `		/* php hands the ORIGINAL string back when it trimmed nothing` |
|     - |  717 | `		 * (trim_each_wchar()'s zend_string_copy), so an ill-formed run survives` |
|     - |  718 | `		 * a no-op trim and is substituted only when something was sliced off. */` |
|    37 |  719 | `		ph7_result_string(pCtx,&zIn[iLeft],(int)(iRight - iLeft));` |
|    37 |  720 | `		return PH7_OK;` |
|     - |  721 | `	}` |
|     - |  722 | `	/* What it does keep is decoded and re-encoded, so an ill-formed run left in` |
|     - |  723 | `	 * the middle comes back as '?' -- the same rule mb_substr() follows. */` |
|    95 |  724 | `	MbResultSubstituted(pCtx,&zIn[iLeft],iRight - iLeft);` |
|    95 |  725 | `	return PH7_OK;` |
|    67 |  726 | `}` |
|     - |  727 | `/* string\|bool mb_internal_encoding(?string $encoding = null) */` |
|     6 |  728 | `static int PH7_builtin_mb_internal_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  729 | `{` |
|     7 |  730 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|     3 |  731 | `		ph7_result_string(pCtx,"UTF-8",sizeof("UTF-8")-1);` |
|     3 |  732 | `		return PH7_OK;` |
|     - |  733 | `	}` |
|     5 |  734 | `	if( MbEncodingArg(pCtx,apArg[0],"mb_internal_encoding",1) < 0 ){` |
|     3 |  735 | `		return PH7_OK;` |
|     - |  736 | `	}` |
|     - |  737 | `	/* Only the UTF-8 family is accepted, and it is already the default */` |
|     3 |  738 | `	ph7_result_bool(pCtx,1);` |
|     3 |  739 | `	return PH7_OK;` |
|     4 |  740 | `}` |
|     - |  741 | `/* bool mb_check_encoding(string $value, ?string $encoding = null) */` |
|    12 |  742 | `static int PH7_builtin_mb_check_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  743 | `{` |
|     - |  744 | `	const unsigned char *z;` |
|     - |  745 | `	const char *zIn;` |
|     - |  746 | `	int nByte;` |
|    13 |  747 | `	sxu32 i = 0,nLen;` |
|    13 |  748 | `	if( nArg < 1 ){` |
|   ! 0 |  749 | `		ph7_result_bool(pCtx,1);` |
|   ! 0 |  750 | `		return PH7_OK;` |
|     - |  751 | `	}` |
|    13 |  752 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_check_encoding",2) < 0 ){` |
|   ! 0 |  753 | `		return PH7_OK;` |
|     - |  754 | `	}` |
|    13 |  755 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    13 |  756 | `	z = (const unsigned char *)zIn;` |
|    39 |  757 | `	while( i < (sxu32)nByte ){` |
|    35 |  758 | `		if( MbUtf8Decode(&z[i],(sxu32)nByte - i,&nLen) < 0 ){` |
|     9 |  759 | `			ph7_result_bool(pCtx,0);` |
|     9 |  760 | `			return PH7_OK;` |
|     - |  761 | `		}` |
|    27 |  762 | `		i += nLen;` |
|     1 |  763 | `	}` |
|     5 |  764 | `	ph7_result_bool(pCtx,1);` |
|     5 |  765 | `	return PH7_OK;` |
|     7 |  766 | `}` |
|     - |  767 | `/* int mb_strwidth(string $string, ?string $encoding = null) */` |
|     2 |  768 | `static int PH7_builtin_mb_strwidth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  769 | `{` |
|     - |  770 | `	const unsigned char *z;` |
|     - |  771 | `	const char *zIn;` |
|     - |  772 | `	int nByte;` |
|     - |  773 | `	sxi32 iCp;` |
|     3 |  774 | `	sxu32 i = 0,nLen,cp;` |
|     3 |  775 | `	ph7_int64 nWidth = 0;` |
|     3 |  776 | `	if( nArg < 1 ){` |
|   ! 0 |  777 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  778 | `		return PH7_OK;` |
|     - |  779 | `	}` |
|     3 |  780 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_strwidth",2) < 0 ){` |
|   ! 0 |  781 | `		return PH7_OK;` |
|     - |  782 | `	}` |
|     3 |  783 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|     3 |  784 | `	z = (const unsigned char *)zIn;` |
|    15 |  785 | `	while( i < (sxu32)nByte ){` |
|    13 |  786 | `		iCp = MbUtf8Decode(&z[i],(sxu32)nByte - i,&nLen);` |
|    13 |  787 | `		i += nLen;` |
|     - |  788 | `		/* An ill-formed run stands in for '?': one column */` |
|    13 |  789 | `		cp = (iCp < 0) ? (sxu32)'?' : (sxu32)iCp;` |
|     - |  790 | `		/* php's East Asian wide/fullwidth set */` |
|    12 |  791 | `		if( (cp >= 0x1100 && cp <= 0x115F) \|\| (cp >= 0x2E80 && cp <= 0xA4CF)` |
|     9 |  792 | `		 \|\| (cp >= 0xAC00 && cp <= 0xD7A3) \|\| (cp >= 0xF900 && cp <= 0xFAFF)` |
|     6 |  793 | `		 \|\| (cp >= 0xFE30 && cp <= 0xFE4F) \|\| (cp >= 0xFF00 && cp <= 0xFF60)` |
|     7 |  794 | `		 \|\| (cp >= 0xFFE0 && cp <= 0xFFE6) \|\| cp >= 0x20000 ){` |
|     7 |  795 | `			nWidth += 2;` |
|     4 |  796 | `		}else{` |
|     7 |  797 | `			nWidth += 1;` |
|     - |  798 | `		}` |
|     1 |  799 | `	}` |
|     3 |  800 | `	ph7_result_int64(pCtx,nWidth);` |
|     3 |  801 | `	return PH7_OK;` |
|     2 |  802 | `}` |
|     - |  803 |  |
|     - |  804 | `/* string\|false mb_chr(int $codepoint, ?string $encoding = null) */` |
|    18 |  805 | `static int PH7_builtin_mb_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  806 | `{` |
|     - |  807 | `	sxi64 cp;` |
|     - |  808 | `	unsigned char zOut[4];` |
|     - |  809 | `	sxu32 n;` |
|    19 |  810 | `	if( nArg < 1 ){` |
|   ! 0 |  811 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  812 | `		return PH7_OK;` |
|     - |  813 | `	}` |
|    19 |  814 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_chr",2) < 0 ){` |
|   ! 0 |  815 | `		return PH7_OK;` |
|     - |  816 | `	}` |
|    19 |  817 | `	cp = ph7_value_to_int64(apArg[0]);` |
|     - |  818 | `	/* php rejects negatives, code points past U+10FFFF and the UTF-16` |
|     - |  819 | `	 * surrogate range with FALSE. */` |
|    19 |  820 | `	if( cp < 0 \|\| cp > 0x10FFFF \|\| (cp >= 0xD800 && cp <= 0xDFFF) ){` |
|     7 |  821 | `		ph7_result_bool(pCtx,0);` |
|     7 |  822 | `		return PH7_OK;` |
|     - |  823 | `	}` |
|    13 |  824 | `	n = MbUtf8Encode((sxu32)cp,zOut);` |
|    13 |  825 | `	ph7_result_string(pCtx,(const char *)zOut,(int)n);` |
|    13 |  826 | `	return PH7_OK;` |
|    10 |  827 | `}` |
|     - |  828 | `/* int\|false mb_ord(string $string, ?string $encoding = null) */` |
|    28 |  829 | `static int PH7_builtin_mb_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  830 | `{` |
|     - |  831 | `	const char *zIn;` |
|     - |  832 | `	const unsigned char *z;` |
|     - |  833 | `	int nByte;` |
|     - |  834 | `	sxi32 iCp;` |
|     - |  835 | `	sxu32 nLen;` |
|    29 |  836 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_ord",2) < 0 ){` |
|   ! 0 |  837 | `		return PH7_OK;` |
|     - |  838 | `	}` |
|    29 |  839 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    29 |  840 | `	if( nByte < 1 ){` |
|     - |  841 | `		/* php throws on an empty string rather than returning FALSE */` |
|     3 |  842 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  843 | `			"mb_ord(): Argument #1 ($string) must not be empty");` |
|     - |  844 | `	}` |
|    27 |  845 | `	z = (const unsigned char *)zIn;` |
|     - |  846 | `	/* Reject a malformed first character (invalid lead / truncated / bad` |
|     - |  847 | `	 * continuation / over-long / surrogate) the way php does */` |
|    27 |  848 | `	iCp = MbUtf8Decode(z,(sxu32)nByte,&nLen);` |
|    27 |  849 | `	if( iCp < 0 ){` |
|    13 |  850 | `		ph7_result_bool(pCtx,0);` |
|    13 |  851 | `		return PH7_OK;` |
|     - |  852 | `	}` |
|    15 |  853 | `	ph7_result_int64(pCtx,(sxi64)iCp);` |
|    15 |  854 | `	return PH7_OK;` |
|    15 |  855 | `}` |
|     - |  856 | `/*` |
|     - |  857 | ` * mb_detect_encoding(string $string, array\|string\|null $encodings = null,` |
|     - |  858 | ` *                    bool $strict = false): string\|false` |
|     - |  859 | ` *` |
|     - |  860 | ` * PHL's encoding scope is ASCII and UTF-8 (NEWPLAN §10 scope cut). php's` |
|     - |  861 | ` * default detect order is exactly ASCII,UTF-8, so the null/default path is` |
|     - |  862 | ` * byte-identical. A candidate encoding php supports but PHL does not (e.g.` |
|     - |  863 | ` * SJIS) raises the same ValueError php uses for a truly invalid name — a` |
|     - |  864 | ` * recorded scope divergence, not silent.` |
|     - |  865 | ` *` |
|     - |  866 | ` * Detection scores each candidate by its count of undecodable bytes and keeps` |
|     - |  867 | ` * the smallest, earliest-in-order on a tie. In strict mode a non-zero best` |
|     - |  868 | ` * score means "no candidate fully matched" -> false. This reproduces php's` |
|     - |  869 | ` * ASCII/UTF-8 outcomes for every probed case, strict and non-strict alike.` |
|     - |  870 | ` */` |
|    28 |  871 | `static int MbAsciiErrors(const unsigned char *z,int n)` |
|     1 |  872 | `{` |
|    29 |  873 | `	int i,e = 0;` |
|   151 |  874 | `	for( i = 0 ; i < n ; i++ ){` |
|   123 |  875 | `		if( z[i] >= 0x80 ){ e++; }` |
|    62 |  876 | `	}` |
|    29 |  877 | `	return e;` |
|     1 |  878 | `}` |
|    28 |  879 | `static int MbUtf8Errors(const unsigned char *z,int n)` |
|     1 |  880 | `{` |
|    29 |  881 | `	sxu32 i = 0,nLen;` |
|    29 |  882 | `	int e = 0;` |
|   139 |  883 | `	while( i < (sxu32)n ){` |
|   111 |  884 | `		if( MbUtf8Decode(&z[i],(sxu32)n - i,&nLen) < 0 ){ e++; i++; }` |
|    99 |  885 | `		else{ i += nLen; }` |
|     1 |  886 | `	}` |
|    29 |  887 | `	return e;` |
|     1 |  888 | `}` |
|     - |  889 | `/* Map an encoding name to PHL's supported set: 0 = ASCII, 1 = UTF-8, -1 = out` |
|     - |  890 | ` * of scope. Surrounding ASCII whitespace is trimmed (php accepts "ASCII, UTF-8"). */` |
|    48 |  891 | `static int MbDetectEncId(const char *z,int n)` |
|     1 |  892 | `{` |
|    75 |  893 | `	while( n > 0 && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]=='\n'\|\|z[0]=='\r') ){ z++; n--; }` |
|    73 |  894 | `	while( n > 0 && (z[n-1]==' '\|\|z[n-1]=='\t'\|\|z[n-1]=='\n'\|\|z[n-1]=='\r') ){ n--; }` |
|    48 |  895 | `	if( (n == 5 && SyStrnicmp(z,"ASCII",5) == 0)` |
|    37 |  896 | `	 \|\| (n == 8 && SyStrnicmp(z,"US-ASCII",8) == 0) ){` |
|    25 |  897 | `		return 0;` |
|     - |  898 | `	}` |
|    24 |  899 | `	if( (n == 5 && SyStrnicmp(z,"UTF-8",5) == 0)` |
|    15 |  900 | `	 \|\| (n == 4 && SyStrnicmp(z,"UTF8",4) == 0) ){` |
|    23 |  901 | `		return 1;` |
|     - |  902 | `	}` |
|     3 |  903 | `	return -1;` |
|    25 |  904 | `}` |
|     - |  905 | `/* Per-detection running state, shared by the array walker and the string path. */` |
|     - |  906 | `typedef struct mb_detect_state mb_detect_state;` |
|     - |  907 | `struct mb_detect_state {` |
|     - |  908 | `	ph7_context *pCtx;` |
|     - |  909 | `	int aErr[2];      /* precomputed [ASCII], [UTF-8] error counts */` |
|     - |  910 | `	int iBestEnc;     /* winning encoding id, -1 until the first candidate */` |
|     - |  911 | `	int iBestErr;     /* its error count */` |
|     - |  912 | `	int nSeen;        /* candidates considered (0 -> "must specify at least one") */` |
|     - |  913 | `	int bError;       /* an out-of-scope name threw -> abort */` |
|     - |  914 | `	int rc;           /* the throw's propagation code (PH7_ABORT/PH7_EXCEPTION) */` |
|     - |  915 | `};` |
|     - |  916 | `/* Fold one candidate encoding name into the running best. Returns SXERR_ABORT` |
|     - |  917 | ` * (and throws) when the name is outside PHL's ASCII/UTF-8 scope. */` |
|    48 |  918 | `static int MbDetectConsider(mb_detect_state *pState,const char *zName,int nName)` |
|     1 |  919 | `{` |
|    49 |  920 | `	int enc = MbDetectEncId(zName,nName);` |
|    49 |  921 | `	if( enc < 0 ){` |
|     3 |  922 | `		pState->bError = 1;` |
|     4 |  923 | `		pState->rc = PH7_VmThrowException(pState->pCtx,"ValueError",` |
|     - |  924 | `			"mb_detect_encoding(): Argument #2 ($encodings) contains invalid encoding \"%.*s\"",` |
|     1 |  925 | `			nName,zName);` |
|     3 |  926 | `		return SXERR_ABORT;` |
|     - |  927 | `	}` |
|    47 |  928 | `	pState->nSeen++;` |
|    47 |  929 | `	if( pState->iBestEnc < 0 \|\| pState->aErr[enc] < pState->iBestErr ){` |
|    37 |  930 | `		pState->iBestEnc = enc;` |
|    37 |  931 | `		pState->iBestErr = pState->aErr[enc];` |
|    18 |  932 | `	}` |
|    47 |  933 | `	return PH7_OK;` |
|    25 |  934 | `}` |
|     - |  935 | `/* ph7_array_walk() callback over the $encodings array. */` |
|    12 |  936 | `static int MbDetectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|     1 |  937 | `{` |
|    13 |  938 | `	mb_detect_state *pState = (mb_detect_state *)pUserData;` |
|     - |  939 | `	const char *zName;` |
|     - |  940 | `	int nName;` |
|     6 |  941 | `	SXUNUSED(pKey);` |
|    13 |  942 | `	zName = ph7_value_to_string(pData,&nName);` |
|    13 |  943 | `	return MbDetectConsider(pState,zName,nName);` |
|     1 |  944 | `}` |
|    28 |  945 | `static int PH7_builtin_mb_detect_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  946 | `{` |
|     - |  947 | `	const char *zIn;` |
|    29 |  948 | `	int nByte,bStrict = 0;` |
|     - |  949 | `	mb_detect_state sState;` |
|    29 |  950 | `	if( nArg < 1 ){` |
|   ! 0 |  951 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  952 | `		return PH7_OK;` |
|     - |  953 | `	}` |
|    29 |  954 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    29 |  955 | `	if( nArg > 2 ){ bStrict = ph7_value_to_bool(apArg[2]); }` |
|    29 |  956 | `	sState.pCtx = pCtx;` |
|    29 |  957 | `	sState.aErr[0] = MbAsciiErrors((const unsigned char *)zIn,nByte);` |
|    29 |  958 | `	sState.aErr[1] = MbUtf8Errors((const unsigned char *)zIn,nByte);` |
|    29 |  959 | `	sState.iBestEnc = -1;` |
|    29 |  960 | `	sState.iBestErr = 0;` |
|    29 |  961 | `	sState.nSeen = 0;` |
|    29 |  962 | `	sState.bError = 0;` |
|    29 |  963 | `	sState.rc = PH7_OK;` |
|    29 |  964 | `	if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|     - |  965 | `		/* php's default detect order is exactly ASCII, then UTF-8 */` |
|    15 |  966 | `		MbDetectConsider(&sState,"ASCII",5);` |
|    15 |  967 | `		MbDetectConsider(&sState,"UTF-8",5);` |
|    22 |  968 | `	}else if( ph7_value_is_array(apArg[1]) ){` |
|     9 |  969 | `		if( ph7_array_count(apArg[1]) == 0 ){` |
|     3 |  970 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  971 | `				"mb_detect_encoding(): Argument #2 ($encodings) must specify at least one encoding");` |
|     - |  972 | `		}` |
|     7 |  973 | `		ph7_array_walk(apArg[1],MbDetectWalker,&sState);` |
|     7 |  974 | `		if( sState.bError ){ return sState.rc; }` |
|     3 |  975 | `	}else{` |
|     - |  976 | `		/* comma-separated list, e.g. "ASCII, UTF-8" */` |
|     - |  977 | `		const char *z2;` |
|     7 |  978 | `		int n2,i,iStart = 0;` |
|     7 |  979 | `		z2 = ph7_value_to_string(apArg[1],&n2);` |
|    55 |  980 | `		for( i = 0 ; i <= n2 ; i++ ){` |
|    49 |  981 | `			if( i == n2 \|\| z2[i] == ',' ){` |
|     9 |  982 | `				const char *zTok = &z2[iStart];` |
|     9 |  983 | `				int nTok = i - iStart,t = nTok;` |
|     - |  984 | `				/* ignore an empty / all-whitespace token */` |
|    15 |  985 | `				while( t > 0 && (zTok[0]==' '\|\|zTok[0]=='\t'\|\|zTok[0]=='\n'\|\|zTok[0]=='\r') ){ zTok++; t--; }` |
|     9 |  986 | `				if( t > 0 && MbDetectConsider(&sState,&z2[iStart],nTok) == SXERR_ABORT ){` |
|   ! 0 |  987 | `					return sState.rc;` |
|     - |  988 | `				}` |
|     9 |  989 | `				iStart = i + 1;` |
|     4 |  990 | `			}` |
|    25 |  991 | `		}` |
|     - |  992 | `	}` |
|    25 |  993 | `	if( sState.nSeen == 0 ){` |
|   ! 0 |  994 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  995 | `			"mb_detect_encoding(): Argument #2 ($encodings) must specify at least one encoding");` |
|     - |  996 | `	}` |
|    25 |  997 | `	if( bStrict && sState.iBestErr > 0 ){` |
|     5 |  998 | `		ph7_result_bool(pCtx,0);` |
|     5 |  999 | `		return PH7_OK;` |
|     - | 1000 | `	}` |
|    21 | 1001 | `	ph7_result_string(pCtx,sState.iBestEnc == 0 ? "ASCII" : "UTF-8",5);` |
|    21 | 1002 | `	return PH7_OK;` |
|    15 | 1003 | `}` |
|     - | 1004 | `/*` |
|     - | 1005 | ` * mb_convert_encoding(array\|string $string, string $to_encoding,` |
|     - | 1006 | ` *                     array\|string\|null $from_encoding = null): array\|string` |
|     - | 1007 | ` *` |
|     - | 1008 | ` * PHL's encoding scope is UTF-8, the byte encodings (8bit/binary/ASCII) and` |
|     - | 1009 | ` * ISO-8859-1 (the §10 scope cut — php's full encoding zoo is out; a php-valid` |
|     - | 1010 | ` * name PHL does not model, e.g. SJIS, raises the same ValueError php uses for a` |
|     - | 1011 | ` * truly invalid name). ISO-8859-1 is carried because it is the documented` |
|     - | 1012 | ` * replacement path for the removed utf8_encode()/utf8_decode() builtins:` |
|     - | 1013 | ` * mb_convert_encoding($s,'UTF-8','ISO-8859-1') and its inverse. Conversion is` |
|     - | 1014 | ` * codepoint-exact for the modelled encodings; a source byte or codepoint that` |
|     - | 1015 | ` * cannot be represented in the target maps to '?' (0x3F), php's default` |
|     - | 1016 | ` * substitute character.` |
|     - | 1017 | ` */` |
|     - | 1018 | `#define MB_ENC_UTF8    0` |
|     - | 1019 | `#define MB_ENC_LATIN1  1   /* ISO-8859-1 / 8bit / binary: byte == codepoint 0..255 */` |
|     - | 1020 | `#define MB_ENC_ASCII   2   /* 7-bit: a byte / codepoint > 0x7F substitutes */` |
|     - | 1021 | `/* Resolve an encoding name to an MB_ENC_* id, or -1 when it is outside PHL's` |
|     - | 1022 | ` * modelled set. Surrounding ASCII whitespace is trimmed (php accepts " UTF-8"). */` |
|    52 | 1023 | `static int MbConvEncId(const char *z,int n)` |
|     1 | 1024 | `{` |
|    77 | 1025 | `	while( n > 0 && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]=='\n'\|\|z[0]=='\r') ){ z++; n--; }` |
|    77 | 1026 | `	while( n > 0 && (z[n-1]==' '\|\|z[n-1]=='\t'\|\|z[n-1]=='\n'\|\|z[n-1]=='\r') ){ n--; }` |
|    45 | 1027 | `	if( (n==5 && SyStrnicmp(z,"UTF-8",5)==0) \|\| (n==4 && SyStrnicmp(z,"UTF8",4)==0) ){` |
|    25 | 1028 | `		return MB_ENC_UTF8;` |
|     - | 1029 | `	}` |
|    20 | 1030 | `	if( (n==10 && SyStrnicmp(z,"ISO-8859-1",10)==0) \|\| (n==9 && SyStrnicmp(z,"ISO8859-1",9)==0)` |
|    10 | 1031 | `	 \|\| (n==6 && SyStrnicmp(z,"latin1",6)==0) \|\| (n==4 && SyStrnicmp(z,"8bit",4)==0)` |
|     9 | 1032 | `	 \|\| (n==6 && SyStrnicmp(z,"binary",6)==0) ){` |
|    17 | 1033 | `		return MB_ENC_LATIN1;` |
|     - | 1034 | `	}` |
|     9 | 1035 | `	if( (n==5 && SyStrnicmp(z,"ASCII",5)==0) \|\| (n==8 && SyStrnicmp(z,"US-ASCII",8)==0) ){` |
|     5 | 1036 | `		return MB_ENC_ASCII;` |
|     - | 1037 | `	}` |
|     5 | 1038 | `	return -1;` |
|    25 | 1039 | `}` |
|     - | 1040 | `/* Transcode one byte buffer from idFrom to idTo, appending to pOut. Input that` |
|     - | 1041 | ` * cannot be represented in the target substitutes '?' (0x3F), php's default. */` |
|    24 | 1042 | `static void MbConvertBuffer(SyBlob *pOut,const char *zIn,sxu32 nByte,int idFrom,int idTo)` |
|     1 | 1043 | `{` |
|    25 | 1044 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|    25 | 1045 | `	sxu32 i = 0,nLen,cp;` |
|     - | 1046 | `	unsigned char zEnc[4];` |
|    79 | 1047 | `	while( i < nByte ){` |
|    55 | 1048 | `		if( idFrom == MB_ENC_UTF8 ){` |
|    37 | 1049 | `			sxi32 iCp = MbUtf8Decode(&z[i],nByte - i,&nLen);` |
|    37 | 1050 | `			cp = (iCp < 0) ? (sxu32)'?' : (sxu32)iCp; /* invalid sequence */` |
|    37 | 1051 | `			i += nLen;` |
|    19 | 1052 | `		}else{` |
|    19 | 1053 | `			cp = z[i];` |
|    19 | 1054 | `			i++;` |
|    19 | 1055 | `			if( idFrom == MB_ENC_ASCII && cp > 0x7F ){ cp = '?'; }` |
|     - | 1056 | `		}` |
|    55 | 1057 | `		if( idTo == MB_ENC_UTF8 ){` |
|    27 | 1058 | `			SyBlobAppend(pOut,zEnc,MbUtf8Encode(cp,zEnc));` |
|    14 | 1059 | `		}else{` |
|    29 | 1060 | `			sxu32 iMax = (idTo == MB_ENC_ASCII) ? 0x7F : 0xFF;` |
|    29 | 1061 | `			zEnc[0] = (unsigned char)((cp <= iMax) ? cp : '?');` |
|    29 | 1062 | `			SyBlobAppend(pOut,zEnc,1);` |
|     - | 1063 | `		}` |
|     1 | 1064 | `	}` |
|    25 | 1065 | `}` |
|     - | 1066 | `/* Build a converted copy of pIn as a fresh context value: a string is` |
|     - | 1067 | ` * transcoded; an array is rebuilt element by element (keys preserved, nested` |
|     - | 1068 | ` * arrays recursed) to match php's array form. Returns 0 on allocation failure. */` |
|    28 | 1069 | `static ph7_value * MbConvertNew(ph7_context *pCtx,ph7_value *pIn,int idFrom,int idTo)` |
|     1 | 1070 | `{` |
|    29 | 1071 | `	if( ph7_value_is_array(pIn) ){` |
|     5 | 1072 | `		ph7_hashmap *pMap = (ph7_hashmap *)pIn->x.pOther;` |
|     5 | 1073 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|     5 | 1074 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|     - | 1075 | `		ph7_value sKey;` |
|     - | 1076 | `		sxu32 n;` |
|     5 | 1077 | `		if( pArr == 0 ){` |
|   ! 0 | 1078 | `			return 0;` |
|     - | 1079 | `		}` |
|     5 | 1080 | `		PH7_MemObjInit(pCtx->pVm,&sKey);` |
|    11 | 1081 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     7 | 1082 | `			ph7_value *pData = HashmapExtractNodeValue(pEntry);` |
|     7 | 1083 | `			if( pData ){` |
|     7 | 1084 | `				ph7_value *pConv = MbConvertNew(pCtx,pData,idFrom,idTo);` |
|     7 | 1085 | `				if( pConv ){` |
|     7 | 1086 | `					PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     7 | 1087 | `					ph7_array_add_elem(pArr,&sKey,pConv);` |
|     7 | 1088 | `					PH7_MemObjRelease(&sKey);` |
|     7 | 1089 | `					ph7_context_release_value(pCtx,pConv);` |
|     3 | 1090 | `				}` |
|     3 | 1091 | `			}` |
|     7 | 1092 | `			pEntry = pEntry->pPrev; /* forward walk (reverse link) */` |
|     4 | 1093 | `		}` |
|     5 | 1094 | `		return pArr;` |
|   ! 0 | 1095 | `	}else{` |
|     - | 1096 | `		SyBlob sOut;` |
|     - | 1097 | `		const char *zIn;` |
|     - | 1098 | `		int nByte;` |
|    25 | 1099 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    25 | 1100 | `		if( pVal == 0 ){` |
|   ! 0 | 1101 | `			return 0;` |
|     - | 1102 | `		}` |
|    25 | 1103 | `		zIn = ph7_value_to_string(pIn,&nByte);` |
|    25 | 1104 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    25 | 1105 | `		MbConvertBuffer(&sOut,zIn,(sxu32)nByte,idFrom,idTo);` |
|    25 | 1106 | `		ph7_value_string(pVal,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    25 | 1107 | `		SyBlobRelease(&sOut);` |
|    25 | 1108 | `		return pVal;` |
|     - | 1109 | `	}` |
|    15 | 1110 | `}` |
|    26 | 1111 | `static int PH7_builtin_mb_convert_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1112 | `{` |
|     - | 1113 | `	const char *zTo,*zFrom;` |
|     - | 1114 | `	int nTo,nFrom,idTo,idFrom;` |
|     - | 1115 | `	ph7_value *pResult;` |
|    27 | 1116 | `	if( nArg < 2 ){` |
|     - | 1117 | `		/* the arity guard fires first; stay defensive */` |
|   ! 0 | 1118 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1119 | `		return PH7_OK;` |
|     - | 1120 | `	}` |
|    27 | 1121 | `	zTo = ph7_value_to_string(apArg[1],&nTo);` |
|    27 | 1122 | `	idTo = MbConvEncId(zTo,nTo);` |
|    27 | 1123 | `	if( idTo < 0 ){` |
|     4 | 1124 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1125 | `			"mb_convert_encoding(): Argument #2 ($to_encoding) must be a valid encoding, \"%.*s\" given",` |
|     1 | 1126 | `			nTo,zTo);` |
|     - | 1127 | `	}` |
|    25 | 1128 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     - | 1129 | `		/* php also accepts an array / comma list here for source detection;` |
|     - | 1130 | `		 * PHL's modelled set makes detection trivial, so a single name is taken` |
|     - | 1131 | `		 * (a list falls out of scope and hits the same loud ValueError). */` |
|    23 | 1132 | `		zFrom = ph7_value_to_string(apArg[2],&nFrom);` |
|    23 | 1133 | `		idFrom = MbConvEncId(zFrom,nFrom);` |
|    23 | 1134 | `		if( idFrom < 0 ){` |
|     4 | 1135 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1136 | `				"mb_convert_encoding(): Argument #3 ($from_encoding) contains invalid encoding \"%.*s\"",` |
|     1 | 1137 | `				nFrom,zFrom);` |
|     - | 1138 | `		}` |
|    11 | 1139 | `	}else{` |
|     - | 1140 | `		/* php falls back to the internal encoding, which PHL fixes at UTF-8 */` |
|     3 | 1141 | `		idFrom = MB_ENC_UTF8;` |
|     - | 1142 | `	}` |
|    23 | 1143 | `	pResult = MbConvertNew(pCtx,apArg[0],idFrom,idTo);` |
|    23 | 1144 | `	if( pResult == 0 ){` |
|   ! 0 | 1145 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1146 | `	}` |
|    23 | 1147 | `	ph7_result_value(pCtx,pResult);` |
|    23 | 1148 | `	return PH7_OK;` |
|    14 | 1149 | `}` |
|     - | 1150 | `/*` |
|     - | 1151 | ` * Install the mb_* functions (called from PH7_RegisterBuiltInFunction's` |
|     - | 1152 | ` * table in builtin.c via these PH7_PRIVATE symbols).` |
|     - | 1153 | ` */` |
|  4062 | 1154 | `PH7_PRIVATE int PH7_builtin_mb_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strlen(pCtx,nArg,apArg); }` |
|    89 | 1155 | `PH7_PRIVATE int PH7_builtin_mb_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_substr(pCtx,nArg,apArg); }` |
|    89 | 1156 | `PH7_PRIVATE int PH7_builtin_mb_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strtolower(pCtx,nArg,apArg); }` |
|    35 | 1157 | `PH7_PRIVATE int PH7_builtin_mb_convert_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_convert_case(pCtx,nArg,apArg); }` |
|    73 | 1158 | `PH7_PRIVATE int PH7_builtin_mb_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strpos(pCtx,nArg,apArg); }` |
|    27 | 1159 | `PH7_PRIVATE int PH7_builtin_mb_str_split_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_str_split(pCtx,nArg,apArg); }` |
|   133 | 1160 | `PH7_PRIVATE int PH7_builtin_mb_trim_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_trim(pCtx,nArg,apArg); }` |
|     7 | 1161 | `PH7_PRIVATE int PH7_builtin_mb_internal_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_internal_encoding(pCtx,nArg,apArg); }` |
|    13 | 1162 | `PH7_PRIVATE int PH7_builtin_mb_check_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_check_encoding(pCtx,nArg,apArg); }` |
|     3 | 1163 | `PH7_PRIVATE int PH7_builtin_mb_strwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strwidth(pCtx,nArg,apArg); }` |
|    19 | 1164 | `PH7_PRIVATE int PH7_builtin_mb_chr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_chr(pCtx,nArg,apArg); }` |
|    29 | 1165 | `PH7_PRIVATE int PH7_builtin_mb_ord_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_ord(pCtx,nArg,apArg); }` |
|    29 | 1166 | `PH7_PRIVATE int PH7_builtin_mb_detect_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_detect_encoding(pCtx,nArg,apArg); }` |
|    27 | 1167 | `PH7_PRIVATE int PH7_builtin_mb_convert_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_convert_encoding(pCtx,nArg,apArg); }` |
|     - | 1168 |  |
|     - | 1169 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 1170 |  |
