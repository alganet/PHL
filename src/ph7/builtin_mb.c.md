# src/ph7/builtin_mb.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 605/650 lines (93.08%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |    7 | `/*` |
|    - |    8 | ` * mb_* multibyte string functions, UTF-8 only (NEWPLAN band D; the recorded` |
|    - |    9 | ` * §10 scope cut — php's full encoding zoo is out). Codepoint semantics match` |
|    - |   10 | ` * php 8.5 byte-for-byte for UTF-8 input; case mapping is algorithmic over` |
|    - |   11 | ` * ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic (full Unicode tables` |
|    - |   12 | ` * recorded as a residual — unmapped codepoints pass through unchanged).` |
|    - |   13 | ` */` |
|    - |   14 |  |
|    - |   15 | `/* --- UTF-8 primitives ------------------------------------------------- */` |
|    - |   16 |  |
|    - |   17 | `/* Byte length of the ill-formed run at z[0..n-1]: the maximal prefix php's` |
|    - |   18 | ` * decoder consumes before giving up — the lead byte plus every continuation` |
|    - |   19 | ` * byte that is still in range for it. mbstring reports that whole prefix as ONE` |
|    - |   20 | ` * character and substitutes ONE '?' for it, so "\xe0\xa0" (a truncated 3-byte` |
|    - |   21 | ` * sequence) is one character, while "\xff\xfe" is two: neither byte can lead. */` |
|  380 |   22 | `static sxu32 MbUtf8BadLen(const unsigned char *z,sxu32 n)` |
|    1 |   23 | `{` |
|  381 |   24 | `	sxu32 c = z[0],need,iLow,iHigh,i;` |
|  381 |   25 | `	if( c >= 0xC2 && c <= 0xDF ){` |
|   39 |   26 | `		need = 2; iLow = 0x80; iHigh = 0xBF;` |
|  362 |   27 | `	}else if( c >= 0xE0 && c <= 0xEF ){` |
|   71 |   28 | `		need = 3; iLow = (c == 0xE0) ? 0xA0 : 0x80; iHigh = (c == 0xED) ? 0x9F : 0xBF;` |
|  308 |   29 | `	}else if( c >= 0xF0 && c <= 0xF4 ){` |
|   25 |   30 | `		need = 4; iLow = (c == 0xF0) ? 0x90 : 0x80; iHigh = (c == 0xF4) ? 0x8F : 0xBF;` |
|   13 |   31 | `	}else{` |
|  249 |   32 | `		return 1; /* 80..C1 or F5..FF: cannot lead anything */` |
|    - |   33 | `	}` |
|  215 |   34 | `	for( i = 1 ; i < need && i < n ; ++i ){` |
|  161 |   35 | `		sxu32 lo = (i == 1) ? iLow : 0x80;` |
|  161 |   36 | `		sxu32 hi = (i == 1) ? iHigh : 0xBF;` |
|  161 |   37 | `		if( z[i] < lo \|\| z[i] > hi ){` |
|   40 |   38 | `			break;` |
|    - |   39 | `		}` |
|   42 |   40 | `	}` |
|  133 |   41 | `	return i;` |
|  191 |   42 | `}` |
|    - |   43 | `/* Decode the character at z (n bytes available); *pLen = the bytes it occupies.` |
|    - |   44 | ` * Returns the codepoint, or -1 when the sequence is ILL-FORMED — in which case` |
|    - |   45 | ` * *pLen is the run above, which php's mbstring counts as one character and` |
|    - |   46 | ` * re-encodes as '?'.` |
|    - |   47 | ` *` |
|    - |   48 | ` * This used to be byte-transparent: an undecodable byte came back AS ITSELF` |
|    - |   49 | ` * with length 1, so mb_strtolower("\xff\xfe") answered the two bytes` |
|    - |   50 | ` * re-encoded as UTF-8 (\xc3\xbf\xc3\xbe) — latin-1 semantics php does not have,` |
|    - |   51 | ` * and characters PHL invented — where php answers "??". The over-long,` |
|    - |   52 | ` * surrogate and past-U+10FFFF forms were accepted as well; validation is` |
|    - |   53 | ` * PH7_Utf8ReadStrict's job now (the same reader json_encode uses). */` |
| 2088 |   54 | `static sxi32 MbUtf8Decode(const unsigned char *z,sxu32 n,sxu32 *pLen)` |
|    1 |   55 | `{` |
| 2089 |   56 | `	sxi32 iCp = PH7_Utf8ReadStrict(z,n,pLen);` |
| 2089 |   57 | `	if( iCp < 0 ){` |
|  381 |   58 | `		*pLen = MbUtf8BadLen(z,n);` |
|  190 |   59 | `	}` |
| 2089 |   60 | `	return iCp;` |
|    1 |   61 | `}` |
|    - |   62 | `/* Encode cp into z (up to 4 bytes); returns the byte count */` |
|  374 |   63 | `static sxu32 MbUtf8Encode(sxu32 cp,unsigned char *z)` |
|    1 |   64 | `{` |
|  375 |   65 | `	if( cp < 0x80 ){` |
|  215 |   66 | `		z[0] = (unsigned char)cp;` |
|  215 |   67 | `		return 1;` |
|    - |   68 | `	}` |
|  161 |   69 | `	if( cp < 0x800 ){` |
|  111 |   70 | `		z[0] = (unsigned char)(0xC0 \| (cp >> 6));` |
|  111 |   71 | `		z[1] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|  111 |   72 | `		return 2;` |
|    - |   73 | `	}` |
|   51 |   74 | `	if( cp < 0x10000 ){` |
|   45 |   75 | `		z[0] = (unsigned char)(0xE0 \| (cp >> 12));` |
|   45 |   76 | `		z[1] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|   45 |   77 | `		z[2] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|   45 |   78 | `		return 3;` |
|    - |   79 | `	}` |
|    7 |   80 | `	z[0] = (unsigned char)(0xF0 \| (cp >> 18));` |
|    7 |   81 | `	z[1] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|    7 |   82 | `	z[2] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|    7 |   83 | `	z[3] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|    7 |   84 | `	return 4;` |
|  188 |   85 | `}` |
|    - |   86 | `/* Codepoint count of a UTF-8 buffer */` |
|  140 |   87 | `static sxu32 MbUtf8Strlen(const char *zIn,sxu32 nByte)` |
|    1 |   88 | `{` |
|  141 |   89 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|  141 |   90 | `	sxu32 i = 0,nCp = 0,nLen;` |
|  763 |   91 | `	while( i < nByte ){` |
|  623 |   92 | `		MbUtf8Decode(&z[i],nByte - i,&nLen);` |
|  623 |   93 | `		i += nLen;` |
|  623 |   94 | `		nCp++;` |
|    1 |   95 | `	}` |
|  141 |   96 | `	return nCp;` |
|    1 |   97 | `}` |
|    - |   98 | `/* Byte offset of codepoint index iCp (clamped to the buffer end) */` |
|  386 |   99 | `static sxu32 MbUtf8Skip(const char *zIn,sxu32 nByte,sxu32 iCp)` |
|    1 |  100 | `{` |
|  387 |  101 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|  387 |  102 | `	sxu32 i = 0,nLen;` |
|  913 |  103 | `	while( i < nByte && iCp > 0 ){` |
|  527 |  104 | `		MbUtf8Decode(&z[i],nByte - i,&nLen);` |
|  527 |  105 | `		i += nLen;` |
|  527 |  106 | `		iCp--;` |
|    1 |  107 | `	}` |
|  387 |  108 | `	return i;` |
|    1 |  109 | `}` |
|    - |  110 |  |
|    - |  111 | `/* --- Case mapping (ASCII, Latin-1, Latin Ext-A, Greek, Cyrillic) ------ */` |
|    - |  112 |  |
|  154 |  113 | `static sxu32 MbToLower(sxu32 c)` |
|    1 |  114 | `{` |
|  155 |  115 | `	if( c < 0x80 ){ return (c >= 'A' && c <= 'Z') ? c + 0x20 : c; }` |
|   59 |  116 | `	if( c >= 0x00C0 && c <= 0x00DE && c != 0x00D7 ){ return c + 0x20; }` |
|   51 |  117 | `	if( c >= 0x0100 && c <= 0x0137 ){ return c \| 1; }` |
|   47 |  118 | `	if( c >= 0x0139 && c <= 0x0148 ){ return ((c - 1) \| 1) + 1; }` |
|   43 |  119 | `	if( c >= 0x014A && c <= 0x0177 ){ return c \| 1; }` |
|   43 |  120 | `	if( c == 0x0178 ){ return 0x00FF; }` |
|   43 |  121 | `	if( c >= 0x0179 && c <= 0x017E ){ return ((c - 1) \| 1) + 1; }` |
|   39 |  122 | `	if( c >= 0x0391 && c <= 0x03A9 && c != 0x03A2 ){ return c + 0x20; }` |
|   31 |  123 | `	if( c >= 0x0410 && c <= 0x042F ){ return c + 0x20; }` |
|   19 |  124 | `	if( c >= 0x0400 && c <= 0x040F ){ return c + 0x50; }` |
|   19 |  125 | `	return c;` |
|   78 |  126 | `}` |
|  120 |  127 | `static sxu32 MbToUpper(sxu32 c)` |
|    1 |  128 | `{` |
|  121 |  129 | `	if( c < 0x80 ){ return (c >= 'a' && c <= 'z') ? c - 0x20 : c; }` |
|   41 |  130 | `	if( c >= 0x00E0 && c <= 0x00FE && c != 0x00F7 ){ return c - 0x20; }` |
|   31 |  131 | `	if( c == 0x00FF ){ return 0x0178; }` |
|   31 |  132 | `	if( c >= 0x0100 && c <= 0x0137 ){ return c & ~(sxu32)1; }` |
|   29 |  133 | `	if( c >= 0x0139 && c <= 0x0148 ){ return ((c - 1) & ~(sxu32)1) + 1; }` |
|   27 |  134 | `	if( c >= 0x014A && c <= 0x0177 ){ return c & ~(sxu32)1; }` |
|   27 |  135 | `	if( c >= 0x0179 && c <= 0x017E ){ return ((c - 1) & ~(sxu32)1) + 1; }` |
|   25 |  136 | `	if( c == 0x017F ){ return 'S'; } /* long s */` |
|   25 |  137 | `	if( c >= 0x03B1 && c <= 0x03C9 && c != 0x03C2 ){ return c - 0x20; }` |
|   15 |  138 | `	if( c == 0x03C2 ){ return 0x03A3; } /* final sigma */` |
|   15 |  139 | `	if( c >= 0x0430 && c <= 0x044F ){ return c - 0x20; }` |
|    3 |  140 | `	if( c >= 0x0450 && c <= 0x045F ){ return c - 0x50; }` |
|    3 |  141 | `	return c;` |
|   61 |  142 | `}` |
|    - |  143 | `/* A codepoint counts as a letter for title-case word boundaries */` |
|   52 |  144 | `static int MbIsAlnum(sxu32 c)` |
|    1 |  145 | `{` |
|   53 |  146 | `	if( c < 0x80 ){` |
|   49 |  147 | `		return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= '0' && c <= '9');` |
|    - |  148 | `	}` |
|    - |  149 | `	/* non-ASCII letters: anything the case mapper knows, plus CJK & co —` |
|    - |  150 | `	 * treat every non-ASCII codepoint as a word character (php's word` |
|    - |  151 | `	 * boundary for TITLE mode is whitespace/punct, all ASCII) */` |
|    5 |  152 | `	return 1;` |
|   27 |  153 | `}` |
|    - |  154 |  |
|    - |  155 | `/* --- Shared argument handling ----------------------------------------- */` |
|    - |  156 |  |
|    - |  157 | `/* Validate the optional $encoding argument: UTF-8 aliases → 0, "8bit"-style` |
|    - |  158 | ` * byte encodings → 1, anything else raises php's ValueError and returns -1.` |
|    - |  159 | ` * (php accepts dozens of encodings; PHL's recorded scope is UTF-8 — a` |
|    - |  160 | ` * php-VALID encoding like SJIS gets the same loud ValueError.) */` |
| 4456 |  161 | `static int MbEncodingArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNo)` |
|    2 |  162 | `{` |
|    - |  163 | `	const char *zEnc;` |
|    - |  164 | `	int nEnc;` |
| 4458 |  165 | `	if( pArg == 0 \|\| ph7_value_is_null(pArg) ){` |
|  419 |  166 | `		return 0;` |
|    - |  167 | `	}` |
| 4040 |  168 | `	zEnc = ph7_value_to_string(pArg,&nEnc);` |
| 4038 |  169 | `	if( (nEnc == 5 && SyStrnicmp(zEnc,"UTF-8",5) == 0)` |
| 4033 |  170 | `	 \|\| (nEnc == 4 && SyStrnicmp(zEnc,"UTF8",4) == 0) ){` |
|   15 |  171 | `		return 0;` |
|    - |  172 | `	}` |
| 4024 |  173 | `	if( (nEnc == 4 && SyStrnicmp(zEnc,"8bit",4) == 0)` |
| 4023 |  174 | `	 \|\| (nEnc == 5 && SyStrnicmp(zEnc,"ASCII",5) == 0)` |
| 4024 |  175 | `	 \|\| (nEnc == 6 && SyStrnicmp(zEnc,"binary",6) == 0) ){` |
|    3 |  176 | `		return 1;` |
|    - |  177 | `	}` |
| 6035 |  178 | `	PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  179 | `		"%s(): Argument #%d ($encoding) must be a valid encoding, \"%.*s\" given",` |
| 2011 |  180 | `		zFunc,iArgNo,nEnc,zEnc);` |
| 4024 |  181 | `	return -1;` |
| 2230 |  182 | `}` |
|    - |  183 |  |
|    - |  184 | `/* --- The functions ----------------------------------------------------- */` |
|    - |  185 |  |
|    - |  186 | `/* int mb_strlen(string $string, ?string $encoding = null) */` |
| 4060 |  187 | `static int PH7_builtin_mb_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  188 | `{` |
|    - |  189 | `	const char *zIn;` |
|    - |  190 | `	int nByte,iEnc;` |
| 4062 |  191 | `	if( nArg < 1 ){` |
|  ! 0 |  192 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  193 | `		return PH7_OK;` |
|    - |  194 | `	}` |
| 4062 |  195 | `	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_strlen",2);` |
| 4062 |  196 | `	if( iEnc < 0 ){` |
| 4018 |  197 | `		return PH7_OK;` |
|    - |  198 | `	}` |
|   45 |  199 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   66 |  200 | `	ph7_result_int64(pCtx,iEnc == 1 ? (ph7_int64)nByte` |
|   42 |  201 | `		: (ph7_int64)MbUtf8Strlen(zIn,(sxu32)nByte));` |
|   45 |  202 | `	return PH7_OK;` |
| 2032 |  203 | `}` |
|    - |  204 | `/* Set the call result to zIn[0..nByte-1] with every ill-formed run replaced by` |
|    - |  205 | ` * '?', php's substitution character. A well-formed buffer copies verbatim. */` |
|   80 |  206 | `static void MbResultSubstituted(ph7_context *pCtx,const char *zIn,sxu32 nByte)` |
|    1 |  207 | `{` |
|   81 |  208 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|    - |  209 | `	SyBlob sOut;` |
|   81 |  210 | `	sxu32 i = 0,nLen;` |
|    - |  211 | `	/* Well-formed is the overwhelmingly common case: check first and hand back` |
|    - |  212 | `	 * the buffer as it stands rather than rebuilding it. */` |
|  221 |  213 | `	while( i < nByte && MbUtf8Decode(&z[i],nByte - i,&nLen) >= 0 ){` |
|  141 |  214 | `		i += nLen;` |
|    1 |  215 | `	}` |
|   81 |  216 | `	if( i >= nByte ){` |
|   57 |  217 | `		ph7_result_string(pCtx,zIn,(int)nByte);` |
|   57 |  218 | `		return;` |
|    - |  219 | `	}` |
|   25 |  220 | `	i = 0;` |
|   25 |  221 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   79 |  222 | `	while( i < nByte ){` |
|   55 |  223 | `		if( MbUtf8Decode(&z[i],nByte - i,&nLen) < 0 ){` |
|   41 |  224 | `			SyBlobAppend(&sOut,"?",1);` |
|   21 |  225 | `		}else{` |
|   15 |  226 | `			SyBlobAppend(&sOut,&z[i],nLen);` |
|    - |  227 | `		}` |
|   55 |  228 | `		i += nLen;` |
|    1 |  229 | `	}` |
|   25 |  230 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   25 |  231 | `	SyBlobRelease(&sOut);` |
|   41 |  232 | `}` |
|    - |  233 | `/* string mb_substr(string $string, int $start, ?int $length = null,` |
|    - |  234 | ` *                  ?string $encoding = null) */` |
|   88 |  235 | `static int PH7_builtin_mb_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  236 | `{` |
|    - |  237 | `	const char *zIn;` |
|    - |  238 | `	int nByte,iEnc;` |
|    - |  239 | `	sxi64 iStart,iLen;` |
|    - |  240 | `	sxu32 nCp,iOfft,iEnd;` |
|   89 |  241 | `	int bLenSet = 0;` |
|   89 |  242 | `	if( nArg < 2 ){` |
|  ! 0 |  243 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  244 | `		return PH7_OK;` |
|    - |  245 | `	}` |
|   89 |  246 | `	iEnc = MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,"mb_substr",4);` |
|   89 |  247 | `	if( iEnc < 0 ){` |
|    3 |  248 | `		return PH7_OK;` |
|    - |  249 | `	}` |
|   87 |  250 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   87 |  251 | `	iStart = ph7_value_to_int64(apArg[1]);` |
|   87 |  252 | `	iLen = 0;` |
|   87 |  253 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|   57 |  254 | `		iLen = ph7_value_to_int64(apArg[2]);` |
|   57 |  255 | `		bLenSet = 1;` |
|   28 |  256 | `	}` |
|   87 |  257 | `	nCp = (iEnc == 1) ? (sxu32)nByte : MbUtf8Strlen(zIn,(sxu32)nByte);` |
|   87 |  258 | `	if( iStart < 0 ){` |
|    3 |  259 | `		iStart = (sxi64)nCp + iStart;` |
|    3 |  260 | `		if( iStart < 0 ){ iStart = 0; }` |
|    1 |  261 | `	}` |
|   87 |  262 | `	if( iStart >= (sxi64)nCp ){` |
|    7 |  263 | `		ph7_result_string(pCtx,"",0);` |
|    7 |  264 | `		return PH7_OK;` |
|    - |  265 | `	}` |
|   81 |  266 | `	if( !bLenSet ){` |
|   25 |  267 | `		iLen = (sxi64)nCp - iStart;` |
|   69 |  268 | `	}else if( iLen < 0 ){` |
|    3 |  269 | `		iLen = ((sxi64)nCp - iStart) + iLen;` |
|    3 |  270 | `		if( iLen < 0 ){ iLen = 0; }` |
|    1 |  271 | `	}` |
|   81 |  272 | `	if( iStart + iLen > (sxi64)nCp ){` |
|   27 |  273 | `		iLen = (sxi64)nCp - iStart;` |
|   13 |  274 | `	}` |
|   81 |  275 | `	if( iEnc == 1 ){` |
|  ! 0 |  276 | `		ph7_result_string(pCtx,&zIn[iStart],(int)iLen);` |
|  ! 0 |  277 | `		return PH7_OK;` |
|    - |  278 | `	}` |
|   81 |  279 | `	iOfft = MbUtf8Skip(zIn,(sxu32)nByte,(sxu32)iStart);` |
|   81 |  280 | `	iEnd  = iOfft + MbUtf8Skip(&zIn[iOfft],(sxu32)nByte - iOfft,(sxu32)iLen);` |
|    - |  281 | `	/* php decodes and re-encodes the slice rather than copying its bytes, so an` |
|    - |  282 | `	 * undecodable run inside it comes out as '?' — mb_substr("ab\xffcd",2,1) is` |
|    - |  283 | `	 * "?", not the raw \xff PHL used to hand back. */` |
|   81 |  284 | `	MbResultSubstituted(pCtx,&zIn[iOfft],iEnd - iOfft);` |
|   81 |  285 | `	return PH7_OK;` |
|   45 |  286 | `}` |
|    - |  287 | `/* Shared case transform: iMode 0 = lower, 1 = upper, 2 = title */` |
|  122 |  288 | `static int MbCaseTransform(ph7_context *pCtx,const char *zIn,sxu32 nByte,int iMode)` |
|    1 |  289 | `{` |
|    - |  290 | `	SyBlob sOut;` |
|  123 |  291 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|  123 |  292 | `	sxu32 i = 0,nLen,cp,mapped;` |
|    - |  293 | `	unsigned char zEnc[4];` |
|  123 |  294 | `	int bWordStart = 1;` |
|  123 |  295 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  477 |  296 | `	while( i < nByte ){` |
|  355 |  297 | `		sxi32 iCp = MbUtf8Decode(&z[i],nByte - i,&nLen);` |
|  355 |  298 | `		i += nLen;` |
|  355 |  299 | `		if( iCp < 0 ){` |
|    - |  300 | `			/* php substitutes '?' for an undecodable run. For TITLE mode the run` |
|    - |  301 | `			 * is neither a word character nor a separator — it leaves the` |
|    - |  302 | `			 * word-start flag exactly as it found it, so "\xffab" titles to` |
|    - |  303 | `			 * "?Ab" (the run did not open a word, 'a' still does) while` |
|    - |  304 | `			 * "a\xffb" titles to "A?b" ('b' is still mid-word). */` |
|  125 |  305 | `			SyBlobAppend(&sOut,"?",1);` |
|  125 |  306 | `			continue;` |
|    - |  307 | `		}` |
|  231 |  308 | `		cp = (sxu32)iCp;` |
|  231 |  309 | `		if( iMode == 1 ){` |
|  109 |  310 | `			if( cp == 0x00DF ){ /* php: mb_strtoupper('ß') === 'SS' */` |
|    3 |  311 | `				SyBlobAppend(&sOut,"SS",2);` |
|    3 |  312 | `				continue;` |
|    - |  313 | `			}` |
|  107 |  314 | `			mapped = MbToUpper(cp);` |
|  176 |  315 | `		}else if( iMode == 0 ){` |
|   71 |  316 | `			if( cp == 0x03A3 ){` |
|    - |  317 | `				/* Greek capital sigma: final position lowers to ς, else σ */` |
|    - |  318 | `				sxu32 nPeek;` |
|    3 |  319 | `				sxi32 iNext = -1;` |
|    3 |  320 | `				if( i < nByte ){` |
|  ! 0 |  321 | `					iNext = MbUtf8Decode(&z[i],nByte - i,&nPeek);` |
|  ! 0 |  322 | `				}` |
|    3 |  323 | `				mapped = (iNext < 0 \|\| !MbIsAlnum((sxu32)iNext) ) ? 0x03C2 : 0x03C3;` |
|    2 |  324 | `			}else{` |
|   69 |  325 | `				mapped = MbToLower(cp);` |
|    - |  326 | `			}` |
|   36 |  327 | `		}else{` |
|   53 |  328 | `			if( MbIsAlnum(cp) ){` |
|   47 |  329 | `				mapped = bWordStart ? MbToUpper(cp) : MbToLower(cp);` |
|   47 |  330 | `				bWordStart = 0;` |
|   24 |  331 | `			}else{` |
|    7 |  332 | `				mapped = cp;` |
|    7 |  333 | `				bWordStart = 1;` |
|    - |  334 | `			}` |
|    - |  335 | `		}` |
|  229 |  336 | `		SyBlobAppend(&sOut,zEnc,MbUtf8Encode(mapped,zEnc));` |
|    1 |  337 | `	}` |
|  123 |  338 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  123 |  339 | `	SyBlobRelease(&sOut);` |
|  123 |  340 | `	return PH7_OK;` |
|    1 |  341 | `}` |
|    - |  342 | `/* string mb_strtolower/mb_strtoupper(string $string, ?string $encoding) */` |
|   88 |  343 | `static int PH7_builtin_mb_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  344 | `{` |
|    - |  345 | `	const char *zIn,*zFunc;` |
|    - |  346 | `	int nByte;` |
|   89 |  347 | `	if( nArg < 1 ){` |
|  ! 0 |  348 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  349 | `		return PH7_OK;` |
|    - |  350 | `	}` |
|   89 |  351 | `	zFunc = ph7_function_name(pCtx);` |
|   89 |  352 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,zFunc,2) < 0 ){` |
|  ! 0 |  353 | `		return PH7_OK;` |
|    - |  354 | `	}` |
|   89 |  355 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  133 |  356 | `	return MbCaseTransform(pCtx,zIn,(sxu32)nByte,` |
|   88 |  357 | `		zFunc[sizeof("mb_strto")-1] == 'u' ? 1 : 0); /* mb_strtoUpper */` |
|   45 |  358 | `}` |
|    - |  359 | `/* string mb_convert_case(string $string, int $mode, ?string $encoding) */` |
|   34 |  360 | `static int PH7_builtin_mb_convert_case(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  361 | `{` |
|    - |  362 | `	const char *zIn;` |
|    - |  363 | `	int nByte,iMode;` |
|   35 |  364 | `	if( nArg < 2 ){` |
|  ! 0 |  365 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  366 | `		return PH7_OK;` |
|    - |  367 | `	}` |
|   35 |  368 | `	if( MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_convert_case",3) < 0 ){` |
|  ! 0 |  369 | `		return PH7_OK;` |
|    - |  370 | `	}` |
|   35 |  371 | `	iMode = ph7_value_to_int(apArg[1]);` |
|   35 |  372 | `	if( iMode < 0 \|\| iMode > 2 ){` |
|    - |  373 | `		/* php has FOLD/SIMPLE variants 3-7; PHL's recorded scope is 0-2 */` |
|  ! 0 |  374 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  375 | `			"mb_convert_case(): Argument #2 ($mode) must be one of the MB_CASE_* constants");` |
|    - |  376 | `	}` |
|   35 |  377 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    - |  378 | `	/* php: MB_CASE_UPPER=0, MB_CASE_LOWER=1, MB_CASE_TITLE=2 */` |
|   68 |  379 | `	return MbCaseTransform(pCtx,zIn,(sxu32)nByte,` |
|   33 |  380 | `		iMode == 0 ? 1 : (iMode == 1 ? 0 : 2));` |
|   18 |  381 | `}` |
|    - |  382 | `/* Shared search core: returns the codepoint index or -1 */` |
|   12 |  383 | `static sxi64 MbSearch(const char *zH,sxu32 nH,const char *zN,sxu32 nN,` |
|    - |  384 | `	sxi64 iOfftCp,int bCaseFold,int bReverse,ph7_context *pCtx)` |
|    1 |  385 | `{` |
|    - |  386 | `	sxu32 iByte,i;` |
|    - |  387 | `	SyBlob sFh,sFn;` |
|   13 |  388 | `	const char *zHay = zH,*zNee = zN;` |
|   13 |  389 | `	sxi64 iFound = -1;` |
|   13 |  390 | `	if( nN == 0 \|\| nN > nH ){` |
|  ! 0 |  391 | `		return -1;` |
|    - |  392 | `	}` |
|   13 |  393 | `	if( bCaseFold ){` |
|    - |  394 | `		/* fold both through the case mapper */` |
|    - |  395 | `		const unsigned char *z;` |
|    - |  396 | `		sxu32 k,nLen;` |
|    - |  397 | `		sxi32 iCp;` |
|    - |  398 | `		unsigned char zEnc[4];` |
|    3 |  399 | `		SyBlobInit(&sFh,&pCtx->pVm->sAllocator);` |
|    3 |  400 | `		SyBlobInit(&sFn,&pCtx->pVm->sAllocator);` |
|    3 |  401 | `		z = (const unsigned char *)zH;` |
|   47 |  402 | `		for( k = 0 ; k < nH ; ){` |
|   45 |  403 | `			iCp = MbUtf8Decode(&z[k],nH - k,&nLen);` |
|   45 |  404 | `			k += nLen;` |
|   45 |  405 | `			if( iCp < 0 ){ SyBlobAppend(&sFh,"?",1); continue; }` |
|   45 |  406 | `			SyBlobAppend(&sFh,zEnc,MbUtf8Encode(MbToLower((sxu32)iCp),zEnc));` |
|    1 |  407 | `		}` |
|    3 |  408 | `		z = (const unsigned char *)zN;` |
|   13 |  409 | `		for( k = 0 ; k < nN ; ){` |
|   11 |  410 | `			iCp = MbUtf8Decode(&z[k],nN - k,&nLen);` |
|   11 |  411 | `			k += nLen;` |
|   11 |  412 | `			if( iCp < 0 ){ SyBlobAppend(&sFn,"?",1); continue; }` |
|   11 |  413 | `			SyBlobAppend(&sFn,zEnc,MbUtf8Encode(MbToLower((sxu32)iCp),zEnc));` |
|    1 |  414 | `		}` |
|    3 |  415 | `		zHay = (const char *)SyBlobData(&sFh);` |
|    3 |  416 | `		nH = SyBlobLength(&sFh);` |
|    3 |  417 | `		zNee = (const char *)SyBlobData(&sFn);` |
|    3 |  418 | `		nN = SyBlobLength(&sFn);` |
|    1 |  419 | `	}` |
|   13 |  420 | `	iByte = MbUtf8Skip(zHay,nH,(sxu32)(iOfftCp > 0 ? iOfftCp : 0));` |
|  109 |  421 | `	for( i = iByte ; i + nN <= nH ; ){` |
|  105 |  422 | `		if( SyMemcmp(&zHay[i],zNee,nN) == 0 ){` |
|   13 |  423 | `			iFound = (sxi64)MbUtf8Strlen(zHay,i);` |
|   13 |  424 | `			if( !bReverse ){` |
|    9 |  425 | `				break;` |
|    - |  426 | `			}` |
|    - |  427 | `			/* keep scanning for the last hit */` |
|    2 |  428 | `		}` |
|    - |  429 | `		{` |
|    - |  430 | `			sxu32 nStep;` |
|   97 |  431 | `			MbUtf8Decode((const unsigned char *)&zHay[i],nH - i,&nStep);` |
|   97 |  432 | `			i += nStep;` |
|    - |  433 | `		}` |
|    1 |  434 | `	}` |
|   13 |  435 | `	if( bCaseFold ){` |
|    3 |  436 | `		SyBlobRelease(&sFh);` |
|    3 |  437 | `		SyBlobRelease(&sFn);` |
|    1 |  438 | `	}` |
|   13 |  439 | `	return iFound;` |
|    7 |  440 | `}` |
|    - |  441 | `/* mb_strpos / mb_stripos / mb_strrpos */` |
|   12 |  442 | `static int PH7_builtin_mb_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  443 | `{` |
|    - |  444 | `	const char *zH,*zN,*zFunc;` |
|    - |  445 | `	int nH,nN;` |
|   13 |  446 | `	sxi64 iOfft = 0,iPos;` |
|    - |  447 | `	int bFold,bRev;` |
|   13 |  448 | `	if( nArg < 2 ){` |
|  ! 0 |  449 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  450 | `		return PH7_OK;` |
|    - |  451 | `	}` |
|   13 |  452 | `	zFunc = ph7_function_name(pCtx);` |
|   13 |  453 | `	bFold = zFunc[sizeof("mb_str")-1] == 'i';   /* mb_strIpos */` |
|   13 |  454 | `	bRev  = zFunc[sizeof("mb_str")-1] == 'r';   /* mb_strRpos */` |
|   13 |  455 | `	if( MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,zFunc,4) < 0 ){` |
|  ! 0 |  456 | `		return PH7_OK;` |
|    - |  457 | `	}` |
|   13 |  458 | `	zH = ph7_value_to_string(apArg[0],&nH);` |
|   13 |  459 | `	zN = ph7_value_to_string(apArg[1],&nN);` |
|   13 |  460 | `	if( nArg > 2 ){` |
|    3 |  461 | `		iOfft = ph7_value_to_int64(apArg[2]);` |
|    3 |  462 | `		if( iOfft < 0 ){` |
|  ! 0 |  463 | `			iOfft = (sxi64)MbUtf8Strlen(zH,(sxu32)nH) + iOfft;` |
|  ! 0 |  464 | `			if( iOfft < 0 ){ iOfft = 0; }` |
|  ! 0 |  465 | `		}` |
|    1 |  466 | `	}` |
|   13 |  467 | `	iPos = MbSearch(zH,(sxu32)nH,zN,(sxu32)nN,iOfft,bFold,bRev,pCtx);` |
|   13 |  468 | `	if( iPos < 0 ){` |
|    3 |  469 | `		ph7_result_bool(pCtx,0);` |
|    2 |  470 | `	}else{` |
|   11 |  471 | `		ph7_result_int64(pCtx,iPos);` |
|    - |  472 | `	}` |
|   13 |  473 | `	return PH7_OK;` |
|    7 |  474 | `}` |
|    - |  475 | `/* array mb_str_split(string $string, int $length = 1, ?string $encoding) */` |
|   56 |  476 | `static int PH7_builtin_mb_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  477 | `{` |
|    - |  478 | `	const char *zIn;` |
|    - |  479 | `	int nByte;` |
|   57 |  480 | `	sxi64 iChunk = 1;` |
|    - |  481 | `	ph7_value *pArr,*pV;` |
|    - |  482 | `	sxu32 i;` |
|   57 |  483 | `	if( nArg < 1 ){` |
|  ! 0 |  484 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  485 | `		return PH7_OK;` |
|    - |  486 | `	}` |
|   57 |  487 | `	if( MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_str_split",3) < 0 ){` |
|    3 |  488 | `		return PH7_OK;` |
|    - |  489 | `	}` |
|   55 |  490 | `	if( nArg > 1 ){` |
|    7 |  491 | `		iChunk = ph7_value_to_int64(apArg[1]);` |
|    7 |  492 | `		if( iChunk < 1 ){` |
|    3 |  493 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  494 | `				"mb_str_split(): Argument #2 ($length) must be greater than 0");` |
|    - |  495 | `		}` |
|    2 |  496 | `	}` |
|   53 |  497 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   53 |  498 | `	pArr = ph7_context_new_array(pCtx);` |
|   53 |  499 | `	pV = ph7_context_new_scalar(pCtx);` |
|   53 |  500 | `	if( pArr == 0 \|\| pV == 0 ){` |
|  ! 0 |  501 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  502 | `	}` |
|  267 |  503 | `	for( i = 0 ; i < (sxu32)nByte ; ){` |
|  215 |  504 | `		sxu32 iEnd = i + MbUtf8Skip(&zIn[i],(sxu32)nByte - i,(sxu32)iChunk);` |
|  215 |  505 | `		ph7_value_string(pV,&zIn[i],(int)(iEnd - i));` |
|  215 |  506 | `		ph7_array_add_elem(pArr,0,pV);` |
|  215 |  507 | `		ph7_value_reset_string_cursor(pV);` |
|  215 |  508 | `		i = iEnd;` |
|    1 |  509 | `	}` |
|   53 |  510 | `	ph7_result_value(pCtx,pArr);` |
|   53 |  511 | `	return PH7_OK;` |
|   29 |  512 | `}` |
|    - |  513 | `/* string\|bool mb_internal_encoding(?string $encoding = null) */` |
|    6 |  514 | `static int PH7_builtin_mb_internal_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  515 | `{` |
|    7 |  516 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|    3 |  517 | `		ph7_result_string(pCtx,"UTF-8",sizeof("UTF-8")-1);` |
|    3 |  518 | `		return PH7_OK;` |
|    - |  519 | `	}` |
|    5 |  520 | `	if( MbEncodingArg(pCtx,apArg[0],"mb_internal_encoding",1) < 0 ){` |
|    3 |  521 | `		return PH7_OK;` |
|    - |  522 | `	}` |
|    - |  523 | `	/* Only the UTF-8 family is accepted, and it is already the default */` |
|    3 |  524 | `	ph7_result_bool(pCtx,1);` |
|    3 |  525 | `	return PH7_OK;` |
|    4 |  526 | `}` |
|    - |  527 | `/* bool mb_check_encoding(string $value, ?string $encoding = null) */` |
|   12 |  528 | `static int PH7_builtin_mb_check_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  529 | `{` |
|    - |  530 | `	const unsigned char *z;` |
|    - |  531 | `	const char *zIn;` |
|    - |  532 | `	int nByte;` |
|   13 |  533 | `	sxu32 i = 0,nLen;` |
|   13 |  534 | `	if( nArg < 1 ){` |
|  ! 0 |  535 | `		ph7_result_bool(pCtx,1);` |
|  ! 0 |  536 | `		return PH7_OK;` |
|    - |  537 | `	}` |
|   13 |  538 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_check_encoding",2) < 0 ){` |
|  ! 0 |  539 | `		return PH7_OK;` |
|    - |  540 | `	}` |
|   13 |  541 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   13 |  542 | `	z = (const unsigned char *)zIn;` |
|   39 |  543 | `	while( i < (sxu32)nByte ){` |
|   35 |  544 | `		if( MbUtf8Decode(&z[i],(sxu32)nByte - i,&nLen) < 0 ){` |
|    9 |  545 | `			ph7_result_bool(pCtx,0);` |
|    9 |  546 | `			return PH7_OK;` |
|    - |  547 | `		}` |
|   27 |  548 | `		i += nLen;` |
|    1 |  549 | `	}` |
|    5 |  550 | `	ph7_result_bool(pCtx,1);` |
|    5 |  551 | `	return PH7_OK;` |
|    7 |  552 | `}` |
|    - |  553 | `/* int mb_strwidth(string $string, ?string $encoding = null) */` |
|    2 |  554 | `static int PH7_builtin_mb_strwidth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  555 | `{` |
|    - |  556 | `	const unsigned char *z;` |
|    - |  557 | `	const char *zIn;` |
|    - |  558 | `	int nByte;` |
|    - |  559 | `	sxi32 iCp;` |
|    3 |  560 | `	sxu32 i = 0,nLen,cp;` |
|    3 |  561 | `	ph7_int64 nWidth = 0;` |
|    3 |  562 | `	if( nArg < 1 ){` |
|  ! 0 |  563 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  564 | `		return PH7_OK;` |
|    - |  565 | `	}` |
|    3 |  566 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_strwidth",2) < 0 ){` |
|  ! 0 |  567 | `		return PH7_OK;` |
|    - |  568 | `	}` |
|    3 |  569 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    3 |  570 | `	z = (const unsigned char *)zIn;` |
|   15 |  571 | `	while( i < (sxu32)nByte ){` |
|   13 |  572 | `		iCp = MbUtf8Decode(&z[i],(sxu32)nByte - i,&nLen);` |
|   13 |  573 | `		i += nLen;` |
|    - |  574 | `		/* An ill-formed run stands in for '?': one column */` |
|   13 |  575 | `		cp = (iCp < 0) ? (sxu32)'?' : (sxu32)iCp;` |
|    - |  576 | `		/* php's East Asian wide/fullwidth set */` |
|   12 |  577 | `		if( (cp >= 0x1100 && cp <= 0x115F) \|\| (cp >= 0x2E80 && cp <= 0xA4CF)` |
|    9 |  578 | `		 \|\| (cp >= 0xAC00 && cp <= 0xD7A3) \|\| (cp >= 0xF900 && cp <= 0xFAFF)` |
|    6 |  579 | `		 \|\| (cp >= 0xFE30 && cp <= 0xFE4F) \|\| (cp >= 0xFF00 && cp <= 0xFF60)` |
|    7 |  580 | `		 \|\| (cp >= 0xFFE0 && cp <= 0xFFE6) \|\| cp >= 0x20000 ){` |
|    7 |  581 | `			nWidth += 2;` |
|    4 |  582 | `		}else{` |
|    7 |  583 | `			nWidth += 1;` |
|    - |  584 | `		}` |
|    1 |  585 | `	}` |
|    3 |  586 | `	ph7_result_int64(pCtx,nWidth);` |
|    3 |  587 | `	return PH7_OK;` |
|    2 |  588 | `}` |
|    - |  589 |  |
|    - |  590 | `/* string\|false mb_chr(int $codepoint, ?string $encoding = null) */` |
|   72 |  591 | `static int PH7_builtin_mb_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  592 | `{` |
|    - |  593 | `	sxi64 cp;` |
|    - |  594 | `	unsigned char zOut[4];` |
|    - |  595 | `	sxu32 n;` |
|   73 |  596 | `	if( nArg < 1 ){` |
|  ! 0 |  597 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  598 | `		return PH7_OK;` |
|    - |  599 | `	}` |
|   73 |  600 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_chr",2) < 0 ){` |
|  ! 0 |  601 | `		return PH7_OK;` |
|    - |  602 | `	}` |
|   73 |  603 | `	cp = ph7_value_to_int64(apArg[0]);` |
|    - |  604 | `	/* php rejects negatives, code points past U+10FFFF and the UTF-16` |
|    - |  605 | `	 * surrogate range with FALSE. */` |
|   73 |  606 | `	if( cp < 0 \|\| cp > 0x10FFFF \|\| (cp >= 0xD800 && cp <= 0xDFFF) ){` |
|    7 |  607 | `		ph7_result_bool(pCtx,0);` |
|    7 |  608 | `		return PH7_OK;` |
|    - |  609 | `	}` |
|   67 |  610 | `	n = MbUtf8Encode((sxu32)cp,zOut);` |
|   67 |  611 | `	ph7_result_string(pCtx,(const char *)zOut,(int)n);` |
|   67 |  612 | `	return PH7_OK;` |
|   37 |  613 | `}` |
|    - |  614 | `/* int\|false mb_ord(string $string, ?string $encoding = null) */` |
|   28 |  615 | `static int PH7_builtin_mb_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  616 | `{` |
|    - |  617 | `	const char *zIn;` |
|    - |  618 | `	const unsigned char *z;` |
|    - |  619 | `	int nByte;` |
|    - |  620 | `	sxi32 iCp;` |
|    - |  621 | `	sxu32 nLen;` |
|   29 |  622 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_ord",2) < 0 ){` |
|  ! 0 |  623 | `		return PH7_OK;` |
|    - |  624 | `	}` |
|   29 |  625 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   29 |  626 | `	if( nByte < 1 ){` |
|    - |  627 | `		/* php throws on an empty string rather than returning FALSE */` |
|    3 |  628 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  629 | `			"mb_ord(): Argument #1 ($string) must not be empty");` |
|    - |  630 | `	}` |
|   27 |  631 | `	z = (const unsigned char *)zIn;` |
|    - |  632 | `	/* Reject a malformed first character (invalid lead / truncated / bad` |
|    - |  633 | `	 * continuation / over-long / surrogate) the way php does */` |
|   27 |  634 | `	iCp = MbUtf8Decode(z,(sxu32)nByte,&nLen);` |
|   27 |  635 | `	if( iCp < 0 ){` |
|   13 |  636 | `		ph7_result_bool(pCtx,0);` |
|   13 |  637 | `		return PH7_OK;` |
|    - |  638 | `	}` |
|   15 |  639 | `	ph7_result_int64(pCtx,(sxi64)iCp);` |
|   15 |  640 | `	return PH7_OK;` |
|   15 |  641 | `}` |
|    - |  642 | `/*` |
|    - |  643 | ` * mb_detect_encoding(string $string, array\|string\|null $encodings = null,` |
|    - |  644 | ` *                    bool $strict = false): string\|false` |
|    - |  645 | ` *` |
|    - |  646 | ` * PHL's encoding scope is ASCII and UTF-8 (NEWPLAN §10 scope cut). php's` |
|    - |  647 | ` * default detect order is exactly ASCII,UTF-8, so the null/default path is` |
|    - |  648 | ` * byte-identical. A candidate encoding php supports but PHL does not (e.g.` |
|    - |  649 | ` * SJIS) raises the same ValueError php uses for a truly invalid name — a` |
|    - |  650 | ` * recorded scope divergence, not silent.` |
|    - |  651 | ` *` |
|    - |  652 | ` * Detection scores each candidate by its count of undecodable bytes and keeps` |
|    - |  653 | ` * the smallest, earliest-in-order on a tie. In strict mode a non-zero best` |
|    - |  654 | ` * score means "no candidate fully matched" -> false. This reproduces php's` |
|    - |  655 | ` * ASCII/UTF-8 outcomes for every probed case, strict and non-strict alike.` |
|    - |  656 | ` */` |
|   28 |  657 | `static int MbAsciiErrors(const unsigned char *z,int n)` |
|    1 |  658 | `{` |
|   29 |  659 | `	int i,e = 0;` |
|  151 |  660 | `	for( i = 0 ; i < n ; i++ ){` |
|  123 |  661 | `		if( z[i] >= 0x80 ){ e++; }` |
|   62 |  662 | `	}` |
|   29 |  663 | `	return e;` |
|    1 |  664 | `}` |
|   28 |  665 | `static int MbUtf8Errors(const unsigned char *z,int n)` |
|    1 |  666 | `{` |
|   29 |  667 | `	sxu32 i = 0,nLen;` |
|   29 |  668 | `	int e = 0;` |
|  139 |  669 | `	while( i < (sxu32)n ){` |
|  111 |  670 | `		if( MbUtf8Decode(&z[i],(sxu32)n - i,&nLen) < 0 ){ e++; i++; }` |
|   99 |  671 | `		else{ i += nLen; }` |
|    1 |  672 | `	}` |
|   29 |  673 | `	return e;` |
|    1 |  674 | `}` |
|    - |  675 | `/* Map an encoding name to PHL's supported set: 0 = ASCII, 1 = UTF-8, -1 = out` |
|    - |  676 | ` * of scope. Surrounding ASCII whitespace is trimmed (php accepts "ASCII, UTF-8"). */` |
|   48 |  677 | `static int MbDetectEncId(const char *z,int n)` |
|    1 |  678 | `{` |
|   75 |  679 | `	while( n > 0 && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]=='\n'\|\|z[0]=='\r') ){ z++; n--; }` |
|   73 |  680 | `	while( n > 0 && (z[n-1]==' '\|\|z[n-1]=='\t'\|\|z[n-1]=='\n'\|\|z[n-1]=='\r') ){ n--; }` |
|   48 |  681 | `	if( (n == 5 && SyStrnicmp(z,"ASCII",5) == 0)` |
|   37 |  682 | `	 \|\| (n == 8 && SyStrnicmp(z,"US-ASCII",8) == 0) ){` |
|   25 |  683 | `		return 0;` |
|    - |  684 | `	}` |
|   24 |  685 | `	if( (n == 5 && SyStrnicmp(z,"UTF-8",5) == 0)` |
|   15 |  686 | `	 \|\| (n == 4 && SyStrnicmp(z,"UTF8",4) == 0) ){` |
|   23 |  687 | `		return 1;` |
|    - |  688 | `	}` |
|    3 |  689 | `	return -1;` |
|   25 |  690 | `}` |
|    - |  691 | `/* Per-detection running state, shared by the array walker and the string path. */` |
|    - |  692 | `typedef struct mb_detect_state mb_detect_state;` |
|    - |  693 | `struct mb_detect_state {` |
|    - |  694 | `	ph7_context *pCtx;` |
|    - |  695 | `	int aErr[2];      /* precomputed [ASCII], [UTF-8] error counts */` |
|    - |  696 | `	int iBestEnc;     /* winning encoding id, -1 until the first candidate */` |
|    - |  697 | `	int iBestErr;     /* its error count */` |
|    - |  698 | `	int nSeen;        /* candidates considered (0 -> "must specify at least one") */` |
|    - |  699 | `	int bError;       /* an out-of-scope name threw -> abort */` |
|    - |  700 | `	int rc;           /* the throw's propagation code (PH7_ABORT/PH7_EXCEPTION) */` |
|    - |  701 | `};` |
|    - |  702 | `/* Fold one candidate encoding name into the running best. Returns SXERR_ABORT` |
|    - |  703 | ` * (and throws) when the name is outside PHL's ASCII/UTF-8 scope. */` |
|   48 |  704 | `static int MbDetectConsider(mb_detect_state *pState,const char *zName,int nName)` |
|    1 |  705 | `{` |
|   49 |  706 | `	int enc = MbDetectEncId(zName,nName);` |
|   49 |  707 | `	if( enc < 0 ){` |
|    3 |  708 | `		pState->bError = 1;` |
|    4 |  709 | `		pState->rc = PH7_VmThrowException(pState->pCtx,"ValueError",` |
|    - |  710 | `			"mb_detect_encoding(): Argument #2 ($encodings) contains invalid encoding \"%.*s\"",` |
|    1 |  711 | `			nName,zName);` |
|    3 |  712 | `		return SXERR_ABORT;` |
|    - |  713 | `	}` |
|   47 |  714 | `	pState->nSeen++;` |
|   47 |  715 | `	if( pState->iBestEnc < 0 \|\| pState->aErr[enc] < pState->iBestErr ){` |
|   37 |  716 | `		pState->iBestEnc = enc;` |
|   37 |  717 | `		pState->iBestErr = pState->aErr[enc];` |
|   18 |  718 | `	}` |
|   47 |  719 | `	return PH7_OK;` |
|   25 |  720 | `}` |
|    - |  721 | `/* ph7_array_walk() callback over the $encodings array. */` |
|   12 |  722 | `static int MbDetectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|    1 |  723 | `{` |
|   13 |  724 | `	mb_detect_state *pState = (mb_detect_state *)pUserData;` |
|    - |  725 | `	const char *zName;` |
|    - |  726 | `	int nName;` |
|    6 |  727 | `	SXUNUSED(pKey);` |
|   13 |  728 | `	zName = ph7_value_to_string(pData,&nName);` |
|   13 |  729 | `	return MbDetectConsider(pState,zName,nName);` |
|    1 |  730 | `}` |
|   28 |  731 | `static int PH7_builtin_mb_detect_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  732 | `{` |
|    - |  733 | `	const char *zIn;` |
|   29 |  734 | `	int nByte,bStrict = 0;` |
|    - |  735 | `	mb_detect_state sState;` |
|   29 |  736 | `	if( nArg < 1 ){` |
|  ! 0 |  737 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  738 | `		return PH7_OK;` |
|    - |  739 | `	}` |
|   29 |  740 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   29 |  741 | `	if( nArg > 2 ){ bStrict = ph7_value_to_bool(apArg[2]); }` |
|   29 |  742 | `	sState.pCtx = pCtx;` |
|   29 |  743 | `	sState.aErr[0] = MbAsciiErrors((const unsigned char *)zIn,nByte);` |
|   29 |  744 | `	sState.aErr[1] = MbUtf8Errors((const unsigned char *)zIn,nByte);` |
|   29 |  745 | `	sState.iBestEnc = -1;` |
|   29 |  746 | `	sState.iBestErr = 0;` |
|   29 |  747 | `	sState.nSeen = 0;` |
|   29 |  748 | `	sState.bError = 0;` |
|   29 |  749 | `	sState.rc = PH7_OK;` |
|   29 |  750 | `	if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|    - |  751 | `		/* php's default detect order is exactly ASCII, then UTF-8 */` |
|   15 |  752 | `		MbDetectConsider(&sState,"ASCII",5);` |
|   15 |  753 | `		MbDetectConsider(&sState,"UTF-8",5);` |
|   22 |  754 | `	}else if( ph7_value_is_array(apArg[1]) ){` |
|    9 |  755 | `		if( ph7_array_count(apArg[1]) == 0 ){` |
|    3 |  756 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  757 | `				"mb_detect_encoding(): Argument #2 ($encodings) must specify at least one encoding");` |
|    - |  758 | `		}` |
|    7 |  759 | `		ph7_array_walk(apArg[1],MbDetectWalker,&sState);` |
|    7 |  760 | `		if( sState.bError ){ return sState.rc; }` |
|    3 |  761 | `	}else{` |
|    - |  762 | `		/* comma-separated list, e.g. "ASCII, UTF-8" */` |
|    - |  763 | `		const char *z2;` |
|    7 |  764 | `		int n2,i,iStart = 0;` |
|    7 |  765 | `		z2 = ph7_value_to_string(apArg[1],&n2);` |
|   55 |  766 | `		for( i = 0 ; i <= n2 ; i++ ){` |
|   49 |  767 | `			if( i == n2 \|\| z2[i] == ',' ){` |
|    9 |  768 | `				const char *zTok = &z2[iStart];` |
|    9 |  769 | `				int nTok = i - iStart,t = nTok;` |
|    - |  770 | `				/* ignore an empty / all-whitespace token */` |
|   15 |  771 | `				while( t > 0 && (zTok[0]==' '\|\|zTok[0]=='\t'\|\|zTok[0]=='\n'\|\|zTok[0]=='\r') ){ zTok++; t--; }` |
|    9 |  772 | `				if( t > 0 && MbDetectConsider(&sState,&z2[iStart],nTok) == SXERR_ABORT ){` |
|  ! 0 |  773 | `					return sState.rc;` |
|    - |  774 | `				}` |
|    9 |  775 | `				iStart = i + 1;` |
|    4 |  776 | `			}` |
|   25 |  777 | `		}` |
|    - |  778 | `	}` |
|   25 |  779 | `	if( sState.nSeen == 0 ){` |
|  ! 0 |  780 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  781 | `			"mb_detect_encoding(): Argument #2 ($encodings) must specify at least one encoding");` |
|    - |  782 | `	}` |
|   25 |  783 | `	if( bStrict && sState.iBestErr > 0 ){` |
|    5 |  784 | `		ph7_result_bool(pCtx,0);` |
|    5 |  785 | `		return PH7_OK;` |
|    - |  786 | `	}` |
|   21 |  787 | `	ph7_result_string(pCtx,sState.iBestEnc == 0 ? "ASCII" : "UTF-8",5);` |
|   21 |  788 | `	return PH7_OK;` |
|   15 |  789 | `}` |
|    - |  790 | `/*` |
|    - |  791 | ` * mb_convert_encoding(array\|string $string, string $to_encoding,` |
|    - |  792 | ` *                     array\|string\|null $from_encoding = null): array\|string` |
|    - |  793 | ` *` |
|    - |  794 | ` * PHL's encoding scope is UTF-8, the byte encodings (8bit/binary/ASCII) and` |
|    - |  795 | ` * ISO-8859-1 (the §10 scope cut — php's full encoding zoo is out; a php-valid` |
|    - |  796 | ` * name PHL does not model, e.g. SJIS, raises the same ValueError php uses for a` |
|    - |  797 | ` * truly invalid name). ISO-8859-1 is carried because it is the documented` |
|    - |  798 | ` * replacement path for the removed utf8_encode()/utf8_decode() builtins:` |
|    - |  799 | ` * mb_convert_encoding($s,'UTF-8','ISO-8859-1') and its inverse. Conversion is` |
|    - |  800 | ` * codepoint-exact for the modelled encodings; a source byte or codepoint that` |
|    - |  801 | ` * cannot be represented in the target maps to '?' (0x3F), php's default` |
|    - |  802 | ` * substitute character.` |
|    - |  803 | ` */` |
|    - |  804 | `#define MB_ENC_UTF8    0` |
|    - |  805 | `#define MB_ENC_LATIN1  1   /* ISO-8859-1 / 8bit / binary: byte == codepoint 0..255 */` |
|    - |  806 | `#define MB_ENC_ASCII   2   /* 7-bit: a byte / codepoint > 0x7F substitutes */` |
|    - |  807 | `/* Resolve an encoding name to an MB_ENC_* id, or -1 when it is outside PHL's` |
|    - |  808 | ` * modelled set. Surrounding ASCII whitespace is trimmed (php accepts " UTF-8"). */` |
|   52 |  809 | `static int MbConvEncId(const char *z,int n)` |
|    1 |  810 | `{` |
|   77 |  811 | `	while( n > 0 && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]=='\n'\|\|z[0]=='\r') ){ z++; n--; }` |
|   77 |  812 | `	while( n > 0 && (z[n-1]==' '\|\|z[n-1]=='\t'\|\|z[n-1]=='\n'\|\|z[n-1]=='\r') ){ n--; }` |
|   45 |  813 | `	if( (n==5 && SyStrnicmp(z,"UTF-8",5)==0) \|\| (n==4 && SyStrnicmp(z,"UTF8",4)==0) ){` |
|   25 |  814 | `		return MB_ENC_UTF8;` |
|    - |  815 | `	}` |
|   20 |  816 | `	if( (n==10 && SyStrnicmp(z,"ISO-8859-1",10)==0) \|\| (n==9 && SyStrnicmp(z,"ISO8859-1",9)==0)` |
|   10 |  817 | `	 \|\| (n==6 && SyStrnicmp(z,"latin1",6)==0) \|\| (n==4 && SyStrnicmp(z,"8bit",4)==0)` |
|    9 |  818 | `	 \|\| (n==6 && SyStrnicmp(z,"binary",6)==0) ){` |
|   17 |  819 | `		return MB_ENC_LATIN1;` |
|    - |  820 | `	}` |
|    9 |  821 | `	if( (n==5 && SyStrnicmp(z,"ASCII",5)==0) \|\| (n==8 && SyStrnicmp(z,"US-ASCII",8)==0) ){` |
|    5 |  822 | `		return MB_ENC_ASCII;` |
|    - |  823 | `	}` |
|    5 |  824 | `	return -1;` |
|   25 |  825 | `}` |
|    - |  826 | `/* Transcode one byte buffer from idFrom to idTo, appending to pOut. Input that` |
|    - |  827 | ` * cannot be represented in the target substitutes '?' (0x3F), php's default. */` |
|   24 |  828 | `static void MbConvertBuffer(SyBlob *pOut,const char *zIn,sxu32 nByte,int idFrom,int idTo)` |
|    1 |  829 | `{` |
|   25 |  830 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   25 |  831 | `	sxu32 i = 0,nLen,cp;` |
|    - |  832 | `	unsigned char zEnc[4];` |
|   79 |  833 | `	while( i < nByte ){` |
|   55 |  834 | `		if( idFrom == MB_ENC_UTF8 ){` |
|   37 |  835 | `			sxi32 iCp = MbUtf8Decode(&z[i],nByte - i,&nLen);` |
|   37 |  836 | `			cp = (iCp < 0) ? (sxu32)'?' : (sxu32)iCp; /* invalid sequence */` |
|   37 |  837 | `			i += nLen;` |
|   19 |  838 | `		}else{` |
|   19 |  839 | `			cp = z[i];` |
|   19 |  840 | `			i++;` |
|   19 |  841 | `			if( idFrom == MB_ENC_ASCII && cp > 0x7F ){ cp = '?'; }` |
|    - |  842 | `		}` |
|   55 |  843 | `		if( idTo == MB_ENC_UTF8 ){` |
|   27 |  844 | `			SyBlobAppend(pOut,zEnc,MbUtf8Encode(cp,zEnc));` |
|   14 |  845 | `		}else{` |
|   29 |  846 | `			sxu32 iMax = (idTo == MB_ENC_ASCII) ? 0x7F : 0xFF;` |
|   29 |  847 | `			zEnc[0] = (unsigned char)((cp <= iMax) ? cp : '?');` |
|   29 |  848 | `			SyBlobAppend(pOut,zEnc,1);` |
|    - |  849 | `		}` |
|    1 |  850 | `	}` |
|   25 |  851 | `}` |
|    - |  852 | `/* Build a converted copy of pIn as a fresh context value: a string is` |
|    - |  853 | ` * transcoded; an array is rebuilt element by element (keys preserved, nested` |
|    - |  854 | ` * arrays recursed) to match php's array form. Returns 0 on allocation failure. */` |
|   28 |  855 | `static ph7_value * MbConvertNew(ph7_context *pCtx,ph7_value *pIn,int idFrom,int idTo)` |
|    1 |  856 | `{` |
|   29 |  857 | `	if( ph7_value_is_array(pIn) ){` |
|    5 |  858 | `		ph7_hashmap *pMap = (ph7_hashmap *)pIn->x.pOther;` |
|    5 |  859 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|    5 |  860 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    - |  861 | `		ph7_value sKey;` |
|    - |  862 | `		sxu32 n;` |
|    5 |  863 | `		if( pArr == 0 ){` |
|  ! 0 |  864 | `			return 0;` |
|    - |  865 | `		}` |
|    5 |  866 | `		PH7_MemObjInit(pCtx->pVm,&sKey);` |
|   11 |  867 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    7 |  868 | `			ph7_value *pData = HashmapExtractNodeValue(pEntry);` |
|    7 |  869 | `			if( pData ){` |
|    7 |  870 | `				ph7_value *pConv = MbConvertNew(pCtx,pData,idFrom,idTo);` |
|    7 |  871 | `				if( pConv ){` |
|    7 |  872 | `					PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    7 |  873 | `					ph7_array_add_elem(pArr,&sKey,pConv);` |
|    7 |  874 | `					PH7_MemObjRelease(&sKey);` |
|    7 |  875 | `					ph7_context_release_value(pCtx,pConv);` |
|    3 |  876 | `				}` |
|    3 |  877 | `			}` |
|    7 |  878 | `			pEntry = pEntry->pPrev; /* forward walk (reverse link) */` |
|    4 |  879 | `		}` |
|    5 |  880 | `		return pArr;` |
|  ! 0 |  881 | `	}else{` |
|    - |  882 | `		SyBlob sOut;` |
|    - |  883 | `		const char *zIn;` |
|    - |  884 | `		int nByte;` |
|   25 |  885 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   25 |  886 | `		if( pVal == 0 ){` |
|  ! 0 |  887 | `			return 0;` |
|    - |  888 | `		}` |
|   25 |  889 | `		zIn = ph7_value_to_string(pIn,&nByte);` |
|   25 |  890 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   25 |  891 | `		MbConvertBuffer(&sOut,zIn,(sxu32)nByte,idFrom,idTo);` |
|   25 |  892 | `		ph7_value_string(pVal,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   25 |  893 | `		SyBlobRelease(&sOut);` |
|   25 |  894 | `		return pVal;` |
|    - |  895 | `	}` |
|   15 |  896 | `}` |
|   26 |  897 | `static int PH7_builtin_mb_convert_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  898 | `{` |
|    - |  899 | `	const char *zTo,*zFrom;` |
|    - |  900 | `	int nTo,nFrom,idTo,idFrom;` |
|    - |  901 | `	ph7_value *pResult;` |
|   27 |  902 | `	if( nArg < 2 ){` |
|    - |  903 | `		/* the arity guard fires first; stay defensive */` |
|  ! 0 |  904 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  905 | `		return PH7_OK;` |
|    - |  906 | `	}` |
|   27 |  907 | `	zTo = ph7_value_to_string(apArg[1],&nTo);` |
|   27 |  908 | `	idTo = MbConvEncId(zTo,nTo);` |
|   27 |  909 | `	if( idTo < 0 ){` |
|    4 |  910 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  911 | `			"mb_convert_encoding(): Argument #2 ($to_encoding) must be a valid encoding, \"%.*s\" given",` |
|    1 |  912 | `			nTo,zTo);` |
|    - |  913 | `	}` |
|   25 |  914 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|    - |  915 | `		/* php also accepts an array / comma list here for source detection;` |
|    - |  916 | `		 * PHL's modelled set makes detection trivial, so a single name is taken` |
|    - |  917 | `		 * (a list falls out of scope and hits the same loud ValueError). */` |
|   23 |  918 | `		zFrom = ph7_value_to_string(apArg[2],&nFrom);` |
|   23 |  919 | `		idFrom = MbConvEncId(zFrom,nFrom);` |
|   23 |  920 | `		if( idFrom < 0 ){` |
|    4 |  921 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  922 | `				"mb_convert_encoding(): Argument #3 ($from_encoding) contains invalid encoding \"%.*s\"",` |
|    1 |  923 | `				nFrom,zFrom);` |
|    - |  924 | `		}` |
|   11 |  925 | `	}else{` |
|    - |  926 | `		/* php falls back to the internal encoding, which PHL fixes at UTF-8 */` |
|    3 |  927 | `		idFrom = MB_ENC_UTF8;` |
|    - |  928 | `	}` |
|   23 |  929 | `	pResult = MbConvertNew(pCtx,apArg[0],idFrom,idTo);` |
|   23 |  930 | `	if( pResult == 0 ){` |
|  ! 0 |  931 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  932 | `	}` |
|   23 |  933 | `	ph7_result_value(pCtx,pResult);` |
|   23 |  934 | `	return PH7_OK;` |
|   14 |  935 | `}` |
|    - |  936 | `/*` |
|    - |  937 | ` * Install the mb_* functions (called from PH7_RegisterBuiltInFunction's` |
|    - |  938 | ` * table in builtin.c via these PH7_PRIVATE symbols).` |
|    - |  939 | ` */` |
| 4062 |  940 | `PH7_PRIVATE int PH7_builtin_mb_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strlen(pCtx,nArg,apArg); }` |
|   89 |  941 | `PH7_PRIVATE int PH7_builtin_mb_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_substr(pCtx,nArg,apArg); }` |
|   89 |  942 | `PH7_PRIVATE int PH7_builtin_mb_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strtolower(pCtx,nArg,apArg); }` |
|   35 |  943 | `PH7_PRIVATE int PH7_builtin_mb_convert_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_convert_case(pCtx,nArg,apArg); }` |
|   13 |  944 | `PH7_PRIVATE int PH7_builtin_mb_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strpos(pCtx,nArg,apArg); }` |
|   57 |  945 | `PH7_PRIVATE int PH7_builtin_mb_str_split_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_str_split(pCtx,nArg,apArg); }` |
|    7 |  946 | `PH7_PRIVATE int PH7_builtin_mb_internal_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_internal_encoding(pCtx,nArg,apArg); }` |
|   13 |  947 | `PH7_PRIVATE int PH7_builtin_mb_check_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_check_encoding(pCtx,nArg,apArg); }` |
|    3 |  948 | `PH7_PRIVATE int PH7_builtin_mb_strwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strwidth(pCtx,nArg,apArg); }` |
|   73 |  949 | `PH7_PRIVATE int PH7_builtin_mb_chr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_chr(pCtx,nArg,apArg); }` |
|   29 |  950 | `PH7_PRIVATE int PH7_builtin_mb_ord_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_ord(pCtx,nArg,apArg); }` |
|   29 |  951 | `PH7_PRIVATE int PH7_builtin_mb_detect_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_detect_encoding(pCtx,nArg,apArg); }` |
|   27 |  952 | `PH7_PRIVATE int PH7_builtin_mb_convert_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_convert_encoding(pCtx,nArg,apArg); }` |
|    - |  953 |  |
|    - |  954 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  955 |  |
