# src/ph7/builtin_mb.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 354/390 lines (90.77%)

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
|    - |   17 | `/* Decode the codepoint at z (n bytes available); *pLen = sequence length.` |
|    - |   18 | ` * Invalid lead bytes decode as themselves with length 1 (byte-transparent,` |
|    - |   19 | ` * so malformed input degrades instead of exploding). */` |
|  766 |   20 | `static sxu32 MbUtf8Decode(const unsigned char *z,sxu32 n,sxu32 *pLen)` |
|    1 |   21 | `{` |
|  767 |   22 | `	sxu32 c = z[0];` |
|  767 |   23 | `	if( c < 0x80 ){` |
|  477 |   24 | `		*pLen = 1;` |
|  477 |   25 | `		return c;` |
|    - |   26 | `	}` |
|  291 |   27 | `	if( (c & 0xE0) == 0xC0 && n >= 2 && (z[1] & 0xC0) == 0x80 ){` |
|  205 |   28 | `		*pLen = 2;` |
|  205 |   29 | `		return ((c & 0x1F) << 6) \| (z[1] & 0x3F);` |
|    - |   30 | `	}` |
|   87 |   31 | `	if( (c & 0xF0) == 0xE0 && n >= 3 && (z[1] & 0xC0) == 0x80 && (z[2] & 0xC0) == 0x80 ){` |
|   67 |   32 | `		*pLen = 3;` |
|   67 |   33 | `		return ((c & 0x0F) << 12) \| ((z[1] & 0x3F) << 6) \| (z[2] & 0x3F);` |
|    - |   34 | `	}` |
|   20 |   35 | `	if( (c & 0xF8) == 0xF0 && n >= 4 && (z[1] & 0xC0) == 0x80 && (z[2] & 0xC0) == 0x80` |
|   19 |   36 | `	 && (z[3] & 0xC0) == 0x80 ){` |
|   19 |   37 | `		*pLen = 4;` |
|   19 |   38 | `		return ((c & 0x07) << 18) \| ((z[1] & 0x3F) << 12) \| ((z[2] & 0x3F) << 6) \| (z[3] & 0x3F);` |
|    - |   39 | `	}` |
|    3 |   40 | `	*pLen = 1;` |
|    3 |   41 | `	return c;` |
|  384 |   42 | `}` |
|    - |   43 | `/* Encode cp into z (up to 4 bytes); returns the byte count */` |
|  186 |   44 | `static sxu32 MbUtf8Encode(sxu32 cp,unsigned char *z)` |
|    1 |   45 | `{` |
|  187 |   46 | `	if( cp < 0x80 ){` |
|  101 |   47 | `		z[0] = (unsigned char)cp;` |
|  101 |   48 | `		return 1;` |
|    - |   49 | `	}` |
|   87 |   50 | `	if( cp < 0x800 ){` |
|   79 |   51 | `		z[0] = (unsigned char)(0xC0 \| (cp >> 6));` |
|   79 |   52 | `		z[1] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|   79 |   53 | `		return 2;` |
|    - |   54 | `	}` |
|    9 |   55 | `	if( cp < 0x10000 ){` |
|    7 |   56 | `		z[0] = (unsigned char)(0xE0 \| (cp >> 12));` |
|    7 |   57 | `		z[1] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|    7 |   58 | `		z[2] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|    7 |   59 | `		return 3;` |
|    - |   60 | `	}` |
|    3 |   61 | `	z[0] = (unsigned char)(0xF0 \| (cp >> 18));` |
|    3 |   62 | `	z[1] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|    3 |   63 | `	z[2] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|    3 |   64 | `	z[3] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|    3 |   65 | `	return 4;` |
|   94 |   66 | `}` |
|    - |   67 | `/* Codepoint count of a UTF-8 buffer */` |
|   26 |   68 | `static sxu32 MbUtf8Strlen(const char *zIn,sxu32 nByte)` |
|    1 |   69 | `{` |
|   27 |   70 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   27 |   71 | `	sxu32 i = 0,nCp = 0,nLen;` |
|  313 |   72 | `	while( i < nByte ){` |
|  287 |   73 | `		MbUtf8Decode(&z[i],nByte - i,&nLen);` |
|  287 |   74 | `		i += nLen;` |
|  287 |   75 | `		nCp++;` |
|    1 |   76 | `	}` |
|   27 |   77 | `	return nCp;` |
|    1 |   78 | `}` |
|    - |   79 | `/* Byte offset of codepoint index iCp (clamped to the buffer end) */` |
|   52 |   80 | `static sxu32 MbUtf8Skip(const char *zIn,sxu32 nByte,sxu32 iCp)` |
|    1 |   81 | `{` |
|   53 |   82 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   53 |   83 | `	sxu32 i = 0,nLen;` |
|  211 |   84 | `	while( i < nByte && iCp > 0 ){` |
|  159 |   85 | `		MbUtf8Decode(&z[i],nByte - i,&nLen);` |
|  159 |   86 | `		i += nLen;` |
|  159 |   87 | `		iCp--;` |
|    1 |   88 | `	}` |
|   53 |   89 | `	return i;` |
|    1 |   90 | `}` |
|    - |   91 |  |
|    - |   92 | `/* --- Case mapping (ASCII, Latin-1, Latin Ext-A, Greek, Cyrillic) ------ */` |
|    - |   93 |  |
|  118 |   94 | `static sxu32 MbToLower(sxu32 c)` |
|    1 |   95 | `{` |
|  119 |   96 | `	if( c < 0x80 ){ return (c >= 'A' && c <= 'Z') ? c + 0x20 : c; }` |
|   55 |   97 | `	if( c >= 0x00C0 && c <= 0x00DE && c != 0x00D7 ){ return c + 0x20; }` |
|   49 |   98 | `	if( c >= 0x0100 && c <= 0x0137 ){ return c \| 1; }` |
|   45 |   99 | `	if( c >= 0x0139 && c <= 0x0148 ){ return ((c - 1) \| 1) + 1; }` |
|   41 |  100 | `	if( c >= 0x014A && c <= 0x0177 ){ return c \| 1; }` |
|   41 |  101 | `	if( c == 0x0178 ){ return 0x00FF; }` |
|   41 |  102 | `	if( c >= 0x0179 && c <= 0x017E ){ return ((c - 1) \| 1) + 1; }` |
|   37 |  103 | `	if( c >= 0x0391 && c <= 0x03A9 && c != 0x03A2 ){ return c + 0x20; }` |
|   31 |  104 | `	if( c >= 0x0410 && c <= 0x042F ){ return c + 0x20; }` |
|   19 |  105 | `	if( c >= 0x0400 && c <= 0x040F ){ return c + 0x50; }` |
|   19 |  106 | `	return c;` |
|   60 |  107 | `}` |
|   62 |  108 | `static sxu32 MbToUpper(sxu32 c)` |
|    1 |  109 | `{` |
|   63 |  110 | `	if( c < 0x80 ){ return (c >= 'a' && c <= 'z') ? c - 0x20 : c; }` |
|   31 |  111 | `	if( c >= 0x00E0 && c <= 0x00FE && c != 0x00F7 ){ return c - 0x20; }` |
|   27 |  112 | `	if( c == 0x00FF ){ return 0x0178; }` |
|   27 |  113 | `	if( c >= 0x0100 && c <= 0x0137 ){ return c & ~(sxu32)1; }` |
|   25 |  114 | `	if( c >= 0x0139 && c <= 0x0148 ){ return ((c - 1) & ~(sxu32)1) + 1; }` |
|   23 |  115 | `	if( c >= 0x014A && c <= 0x0177 ){ return c & ~(sxu32)1; }` |
|   23 |  116 | `	if( c >= 0x0179 && c <= 0x017E ){ return ((c - 1) & ~(sxu32)1) + 1; }` |
|   21 |  117 | `	if( c == 0x017F ){ return 'S'; } /* long s */` |
|   21 |  118 | `	if( c >= 0x03B1 && c <= 0x03C9 && c != 0x03C2 ){ return c - 0x20; }` |
|   13 |  119 | `	if( c == 0x03C2 ){ return 0x03A3; } /* final sigma */` |
|   13 |  120 | `	if( c >= 0x0430 && c <= 0x044F ){ return c - 0x20; }` |
|  ! 0 |  121 | `	if( c >= 0x0450 && c <= 0x045F ){ return c - 0x50; }` |
|  ! 0 |  122 | `	return c;` |
|   32 |  123 | `}` |
|    - |  124 | `/* A codepoint counts as a letter for title-case word boundaries */` |
|   30 |  125 | `static int MbIsAlnum(sxu32 c)` |
|    1 |  126 | `{` |
|   31 |  127 | `	if( c < 0x80 ){` |
|   27 |  128 | `		return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= '0' && c <= '9');` |
|    - |  129 | `	}` |
|    - |  130 | `	/* non-ASCII letters: anything the case mapper knows, plus CJK & co —` |
|    - |  131 | `	 * treat every non-ASCII codepoint as a word character (php's word` |
|    - |  132 | `	 * boundary for TITLE mode is whitespace/punct, all ASCII) */` |
|    5 |  133 | `	return 1;` |
|   16 |  134 | `}` |
|    - |  135 |  |
|    - |  136 | `/* --- Shared argument handling ----------------------------------------- */` |
|    - |  137 |  |
|    - |  138 | `/* Validate the optional $encoding argument: UTF-8 aliases → 0, "8bit"-style` |
|    - |  139 | ` * byte encodings → 1, anything else raises php's ValueError and returns -1.` |
|    - |  140 | ` * (php accepts dozens of encodings; PHL's recorded scope is UTF-8 — a` |
|    - |  141 | ` * php-VALID encoding like SJIS gets the same loud ValueError.) */` |
|   66 |  142 | `static int MbEncodingArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNo)` |
|    1 |  143 | `{` |
|    - |  144 | `	const char *zEnc;` |
|    - |  145 | `	int nEnc;` |
|   67 |  146 | `	if( pArg == 0 \|\| ph7_value_is_null(pArg) ){` |
|   57 |  147 | `		return 0;` |
|    - |  148 | `	}` |
|   11 |  149 | `	zEnc = ph7_value_to_string(pArg,&nEnc);` |
|   10 |  150 | `	if( (nEnc == 5 && SyStrnicmp(zEnc,"UTF-8",5) == 0)` |
|    8 |  151 | `	 \|\| (nEnc == 4 && SyStrnicmp(zEnc,"UTF8",4) == 0) ){` |
|    7 |  152 | `		return 0;` |
|    - |  153 | `	}` |
|    4 |  154 | `	if( (nEnc == 4 && SyStrnicmp(zEnc,"8bit",4) == 0)` |
|    3 |  155 | `	 \|\| (nEnc == 5 && SyStrnicmp(zEnc,"ASCII",5) == 0)` |
|    3 |  156 | `	 \|\| (nEnc == 6 && SyStrnicmp(zEnc,"binary",6) == 0) ){` |
|    3 |  157 | `		return 1;` |
|    - |  158 | `	}` |
|    4 |  159 | `	PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  160 | `		"%s(): Argument #%d ($encoding) must be a valid encoding, \"%.*s\" given",` |
|    1 |  161 | `		zFunc,iArgNo,nEnc,zEnc);` |
|    3 |  162 | `	return -1;` |
|   34 |  163 | `}` |
|    - |  164 |  |
|    - |  165 | `/* --- The functions ----------------------------------------------------- */` |
|    - |  166 |  |
|    - |  167 | `/* int mb_strlen(string $string, ?string $encoding = null) */` |
|    4 |  168 | `static int PH7_builtin_mb_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  169 | `{` |
|    - |  170 | `	const char *zIn;` |
|    - |  171 | `	int nByte,iEnc;` |
|    5 |  172 | `	if( nArg < 1 ){` |
|  ! 0 |  173 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  174 | `		return PH7_OK;` |
|    - |  175 | `	}` |
|    5 |  176 | `	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_strlen",2);` |
|    5 |  177 | `	if( iEnc < 0 ){` |
|  ! 0 |  178 | `		return PH7_OK;` |
|    - |  179 | `	}` |
|    5 |  180 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    6 |  181 | `	ph7_result_int64(pCtx,iEnc == 1 ? (ph7_int64)nByte` |
|    2 |  182 | `		: (ph7_int64)MbUtf8Strlen(zIn,(sxu32)nByte));` |
|    5 |  183 | `	return PH7_OK;` |
|    3 |  184 | `}` |
|    - |  185 | `/* string mb_substr(string $string, int $start, ?int $length = null,` |
|    - |  186 | ` *                  ?string $encoding = null) */` |
|   12 |  187 | `static int PH7_builtin_mb_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  188 | `{` |
|    - |  189 | `	const char *zIn;` |
|    - |  190 | `	int nByte,iEnc;` |
|    - |  191 | `	sxi64 iStart,iLen;` |
|    - |  192 | `	sxu32 nCp,iOfft,iEnd;` |
|   13 |  193 | `	int bLenSet = 0;` |
|   13 |  194 | `	if( nArg < 2 ){` |
|  ! 0 |  195 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  196 | `		return PH7_OK;` |
|    - |  197 | `	}` |
|   13 |  198 | `	iEnc = MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,"mb_substr",4);` |
|   13 |  199 | `	if( iEnc < 0 ){` |
|  ! 0 |  200 | `		return PH7_OK;` |
|    - |  201 | `	}` |
|   13 |  202 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   13 |  203 | `	iStart = ph7_value_to_int64(apArg[1]);` |
|   13 |  204 | `	iLen = 0;` |
|   13 |  205 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|    7 |  206 | `		iLen = ph7_value_to_int64(apArg[2]);` |
|    7 |  207 | `		bLenSet = 1;` |
|    3 |  208 | `	}` |
|   13 |  209 | `	nCp = (iEnc == 1) ? (sxu32)nByte : MbUtf8Strlen(zIn,(sxu32)nByte);` |
|   13 |  210 | `	if( iStart < 0 ){` |
|    3 |  211 | `		iStart = (sxi64)nCp + iStart;` |
|    3 |  212 | `		if( iStart < 0 ){ iStart = 0; }` |
|    1 |  213 | `	}` |
|   13 |  214 | `	if( iStart >= (sxi64)nCp ){` |
|    3 |  215 | `		ph7_result_string(pCtx,"",0);` |
|    3 |  216 | `		return PH7_OK;` |
|    - |  217 | `	}` |
|   11 |  218 | `	if( !bLenSet ){` |
|    5 |  219 | `		iLen = (sxi64)nCp - iStart;` |
|    9 |  220 | `	}else if( iLen < 0 ){` |
|    3 |  221 | `		iLen = ((sxi64)nCp - iStart) + iLen;` |
|    3 |  222 | `		if( iLen < 0 ){ iLen = 0; }` |
|    1 |  223 | `	}` |
|   11 |  224 | `	if( iStart + iLen > (sxi64)nCp ){` |
|    3 |  225 | `		iLen = (sxi64)nCp - iStart;` |
|    1 |  226 | `	}` |
|   11 |  227 | `	if( iEnc == 1 ){` |
|  ! 0 |  228 | `		ph7_result_string(pCtx,&zIn[iStart],(int)iLen);` |
|  ! 0 |  229 | `		return PH7_OK;` |
|    - |  230 | `	}` |
|   11 |  231 | `	iOfft = MbUtf8Skip(zIn,(sxu32)nByte,(sxu32)iStart);` |
|   11 |  232 | `	iEnd  = iOfft + MbUtf8Skip(&zIn[iOfft],(sxu32)nByte - iOfft,(sxu32)iLen);` |
|   11 |  233 | `	ph7_result_string(pCtx,&zIn[iOfft],(int)(iEnd - iOfft));` |
|   11 |  234 | `	return PH7_OK;` |
|    7 |  235 | `}` |
|    - |  236 | `/* Shared case transform: iMode 0 = lower, 1 = upper, 2 = title */` |
|   20 |  237 | `static int MbCaseTransform(ph7_context *pCtx,const char *zIn,sxu32 nByte,int iMode)` |
|    1 |  238 | `{` |
|    - |  239 | `	SyBlob sOut;` |
|   21 |  240 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   21 |  241 | `	sxu32 i = 0,nLen,cp,mapped;` |
|    - |  242 | `	unsigned char zEnc[4];` |
|   21 |  243 | `	int bWordStart = 1;` |
|   21 |  244 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  155 |  245 | `	while( i < nByte ){` |
|  135 |  246 | `		cp = MbUtf8Decode(&z[i],nByte - i,&nLen);` |
|  135 |  247 | `		i += nLen;` |
|  135 |  248 | `		if( iMode == 1 ){` |
|   59 |  249 | `			if( cp == 0x00DF ){ /* php: mb_strtoupper('ß') === 'SS' */` |
|    3 |  250 | `				SyBlobAppend(&sOut,"SS",2);` |
|    3 |  251 | `				continue;` |
|    - |  252 | `			}` |
|   57 |  253 | `			mapped = MbToUpper(cp);` |
|  105 |  254 | `		}else if( iMode == 0 ){` |
|   47 |  255 | `			if( cp == 0x03A3 ){` |
|    - |  256 | `				/* Greek capital sigma: final position lowers to ς, else σ */` |
|    3 |  257 | `				sxu32 nPeek,cpNext = 0;` |
|    3 |  258 | `				if( i < nByte ){` |
|  ! 0 |  259 | `					cpNext = MbUtf8Decode(&z[i],nByte - i,&nPeek);` |
|  ! 0 |  260 | `				}` |
|    3 |  261 | `				mapped = (i >= nByte \|\| !MbIsAlnum(cpNext) ) ? 0x03C2 : 0x03C3;` |
|    2 |  262 | `			}else{` |
|   45 |  263 | `				mapped = MbToLower(cp);` |
|    - |  264 | `			}` |
|   24 |  265 | `		}else{` |
|   31 |  266 | `			if( MbIsAlnum(cp) ){` |
|   27 |  267 | `				mapped = bWordStart ? MbToUpper(cp) : MbToLower(cp);` |
|   27 |  268 | `				bWordStart = 0;` |
|   14 |  269 | `			}else{` |
|    5 |  270 | `				mapped = cp;` |
|    5 |  271 | `				bWordStart = 1;` |
|    - |  272 | `			}` |
|    - |  273 | `		}` |
|  133 |  274 | `		SyBlobAppend(&sOut,zEnc,MbUtf8Encode(mapped,zEnc));` |
|    1 |  275 | `	}` |
|   21 |  276 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   21 |  277 | `	SyBlobRelease(&sOut);` |
|   21 |  278 | `	return PH7_OK;` |
|    1 |  279 | `}` |
|    - |  280 | `/* string mb_strtolower/mb_strtoupper(string $string, ?string $encoding) */` |
|   14 |  281 | `static int PH7_builtin_mb_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  282 | `{` |
|    - |  283 | `	const char *zIn,*zFunc;` |
|    - |  284 | `	int nByte;` |
|   15 |  285 | `	if( nArg < 1 ){` |
|  ! 0 |  286 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  287 | `		return PH7_OK;` |
|    - |  288 | `	}` |
|   15 |  289 | `	zFunc = ph7_function_name(pCtx);` |
|   15 |  290 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,zFunc,2) < 0 ){` |
|  ! 0 |  291 | `		return PH7_OK;` |
|    - |  292 | `	}` |
|   15 |  293 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   22 |  294 | `	return MbCaseTransform(pCtx,zIn,(sxu32)nByte,` |
|   14 |  295 | `		zFunc[sizeof("mb_strto")-1] == 'u' ? 1 : 0); /* mb_strtoUpper */` |
|    8 |  296 | `}` |
|    - |  297 | `/* string mb_convert_case(string $string, int $mode, ?string $encoding) */` |
|    6 |  298 | `static int PH7_builtin_mb_convert_case(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  299 | `{` |
|    - |  300 | `	const char *zIn;` |
|    - |  301 | `	int nByte,iMode;` |
|    7 |  302 | `	if( nArg < 2 ){` |
|  ! 0 |  303 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  304 | `		return PH7_OK;` |
|    - |  305 | `	}` |
|    7 |  306 | `	if( MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_convert_case",3) < 0 ){` |
|  ! 0 |  307 | `		return PH7_OK;` |
|    - |  308 | `	}` |
|    7 |  309 | `	iMode = ph7_value_to_int(apArg[1]);` |
|    7 |  310 | `	if( iMode < 0 \|\| iMode > 2 ){` |
|    - |  311 | `		/* php has FOLD/SIMPLE variants 3-7; PHL's recorded scope is 0-2 */` |
|  ! 0 |  312 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  313 | `			"mb_convert_case(): Argument #2 ($mode) must be one of the MB_CASE_* constants");` |
|    - |  314 | `	}` |
|    7 |  315 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    - |  316 | `	/* php: MB_CASE_UPPER=0, MB_CASE_LOWER=1, MB_CASE_TITLE=2 */` |
|   12 |  317 | `	return MbCaseTransform(pCtx,zIn,(sxu32)nByte,` |
|    5 |  318 | `		iMode == 0 ? 1 : (iMode == 1 ? 0 : 2));` |
|    4 |  319 | `}` |
|    - |  320 | `/* Shared search core: returns the codepoint index or -1 */` |
|   12 |  321 | `static sxi64 MbSearch(const char *zH,sxu32 nH,const char *zN,sxu32 nN,` |
|    - |  322 | `	sxi64 iOfftCp,int bCaseFold,int bReverse,ph7_context *pCtx)` |
|    1 |  323 | `{` |
|    - |  324 | `	sxu32 iByte,i;` |
|    - |  325 | `	SyBlob sFh,sFn;` |
|   13 |  326 | `	const char *zHay = zH,*zNee = zN;` |
|   13 |  327 | `	sxi64 iFound = -1;` |
|   13 |  328 | `	if( nN == 0 \|\| nN > nH ){` |
|  ! 0 |  329 | `		return -1;` |
|    - |  330 | `	}` |
|   13 |  331 | `	if( bCaseFold ){` |
|    - |  332 | `		/* fold both through the case mapper */` |
|    - |  333 | `		const unsigned char *z;` |
|    - |  334 | `		sxu32 k,nLen,cp;` |
|    - |  335 | `		unsigned char zEnc[4];` |
|    3 |  336 | `		SyBlobInit(&sFh,&pCtx->pVm->sAllocator);` |
|    3 |  337 | `		SyBlobInit(&sFn,&pCtx->pVm->sAllocator);` |
|    3 |  338 | `		z = (const unsigned char *)zH;` |
|   47 |  339 | `		for( k = 0 ; k < nH ; ){` |
|   45 |  340 | `			cp = MbUtf8Decode(&z[k],nH - k,&nLen);` |
|   45 |  341 | `			k += nLen;` |
|   45 |  342 | `			SyBlobAppend(&sFh,zEnc,MbUtf8Encode(MbToLower(cp),zEnc));` |
|    1 |  343 | `		}` |
|    3 |  344 | `		z = (const unsigned char *)zN;` |
|   13 |  345 | `		for( k = 0 ; k < nN ; ){` |
|   11 |  346 | `			cp = MbUtf8Decode(&z[k],nN - k,&nLen);` |
|   11 |  347 | `			k += nLen;` |
|   11 |  348 | `			SyBlobAppend(&sFn,zEnc,MbUtf8Encode(MbToLower(cp),zEnc));` |
|    1 |  349 | `		}` |
|    3 |  350 | `		zHay = (const char *)SyBlobData(&sFh);` |
|    3 |  351 | `		nH = SyBlobLength(&sFh);` |
|    3 |  352 | `		zNee = (const char *)SyBlobData(&sFn);` |
|    3 |  353 | `		nN = SyBlobLength(&sFn);` |
|    1 |  354 | `	}` |
|   13 |  355 | `	iByte = MbUtf8Skip(zHay,nH,(sxu32)(iOfftCp > 0 ? iOfftCp : 0));` |
|  109 |  356 | `	for( i = iByte ; i + nN <= nH ; ){` |
|  105 |  357 | `		if( SyMemcmp(&zHay[i],zNee,nN) == 0 ){` |
|   13 |  358 | `			iFound = (sxi64)MbUtf8Strlen(zHay,i);` |
|   13 |  359 | `			if( !bReverse ){` |
|    9 |  360 | `				break;` |
|    - |  361 | `			}` |
|    - |  362 | `			/* keep scanning for the last hit */` |
|    2 |  363 | `		}` |
|    - |  364 | `		{` |
|    - |  365 | `			sxu32 nStep;` |
|   97 |  366 | `			MbUtf8Decode((const unsigned char *)&zHay[i],nH - i,&nStep);` |
|   97 |  367 | `			i += nStep;` |
|    - |  368 | `		}` |
|    1 |  369 | `	}` |
|   13 |  370 | `	if( bCaseFold ){` |
|    3 |  371 | `		SyBlobRelease(&sFh);` |
|    3 |  372 | `		SyBlobRelease(&sFn);` |
|    1 |  373 | `	}` |
|   13 |  374 | `	return iFound;` |
|    7 |  375 | `}` |
|    - |  376 | `/* mb_strpos / mb_stripos / mb_strrpos */` |
|   12 |  377 | `static int PH7_builtin_mb_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  378 | `{` |
|    - |  379 | `	const char *zH,*zN,*zFunc;` |
|    - |  380 | `	int nH,nN;` |
|   13 |  381 | `	sxi64 iOfft = 0,iPos;` |
|    - |  382 | `	int bFold,bRev;` |
|   13 |  383 | `	if( nArg < 2 ){` |
|  ! 0 |  384 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  385 | `		return PH7_OK;` |
|    - |  386 | `	}` |
|   13 |  387 | `	zFunc = ph7_function_name(pCtx);` |
|   13 |  388 | `	bFold = zFunc[sizeof("mb_str")-1] == 'i';   /* mb_strIpos */` |
|   13 |  389 | `	bRev  = zFunc[sizeof("mb_str")-1] == 'r';   /* mb_strRpos */` |
|   13 |  390 | `	if( MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,zFunc,4) < 0 ){` |
|  ! 0 |  391 | `		return PH7_OK;` |
|    - |  392 | `	}` |
|   13 |  393 | `	zH = ph7_value_to_string(apArg[0],&nH);` |
|   13 |  394 | `	zN = ph7_value_to_string(apArg[1],&nN);` |
|   13 |  395 | `	if( nArg > 2 ){` |
|    3 |  396 | `		iOfft = ph7_value_to_int64(apArg[2]);` |
|    3 |  397 | `		if( iOfft < 0 ){` |
|  ! 0 |  398 | `			iOfft = (sxi64)MbUtf8Strlen(zH,(sxu32)nH) + iOfft;` |
|  ! 0 |  399 | `			if( iOfft < 0 ){ iOfft = 0; }` |
|  ! 0 |  400 | `		}` |
|    1 |  401 | `	}` |
|   13 |  402 | `	iPos = MbSearch(zH,(sxu32)nH,zN,(sxu32)nN,iOfft,bFold,bRev,pCtx);` |
|   13 |  403 | `	if( iPos < 0 ){` |
|    3 |  404 | `		ph7_result_bool(pCtx,0);` |
|    2 |  405 | `	}else{` |
|   11 |  406 | `		ph7_result_int64(pCtx,iPos);` |
|    - |  407 | `	}` |
|   13 |  408 | `	return PH7_OK;` |
|    7 |  409 | `}` |
|    - |  410 | `/* array mb_str_split(string $string, int $length = 1, ?string $encoding) */` |
|    8 |  411 | `static int PH7_builtin_mb_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  412 | `{` |
|    - |  413 | `	const char *zIn;` |
|    - |  414 | `	int nByte;` |
|    9 |  415 | `	sxi64 iChunk = 1;` |
|    - |  416 | `	ph7_value *pArr,*pV;` |
|    - |  417 | `	sxu32 i;` |
|    9 |  418 | `	if( nArg < 1 ){` |
|  ! 0 |  419 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  420 | `		return PH7_OK;` |
|    - |  421 | `	}` |
|    9 |  422 | `	if( MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_str_split",3) < 0 ){` |
|  ! 0 |  423 | `		return PH7_OK;` |
|    - |  424 | `	}` |
|    9 |  425 | `	if( nArg > 1 ){` |
|    7 |  426 | `		iChunk = ph7_value_to_int64(apArg[1]);` |
|    7 |  427 | `		if( iChunk < 1 ){` |
|    3 |  428 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  429 | `				"mb_str_split(): Argument #2 ($length) must be greater than 0");` |
|    - |  430 | `		}` |
|    2 |  431 | `	}` |
|    7 |  432 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    7 |  433 | `	pArr = ph7_context_new_array(pCtx);` |
|    7 |  434 | `	pV = ph7_context_new_scalar(pCtx);` |
|    7 |  435 | `	if( pArr == 0 \|\| pV == 0 ){` |
|  ! 0 |  436 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  437 | `	}` |
|   27 |  438 | `	for( i = 0 ; i < (sxu32)nByte ; ){` |
|   21 |  439 | `		sxu32 iEnd = i + MbUtf8Skip(&zIn[i],(sxu32)nByte - i,(sxu32)iChunk);` |
|   21 |  440 | `		ph7_value_string(pV,&zIn[i],(int)(iEnd - i));` |
|   21 |  441 | `		ph7_array_add_elem(pArr,0,pV);` |
|   21 |  442 | `		ph7_value_reset_string_cursor(pV);` |
|   21 |  443 | `		i = iEnd;` |
|    1 |  444 | `	}` |
|    7 |  445 | `	ph7_result_value(pCtx,pArr);` |
|    7 |  446 | `	return PH7_OK;` |
|    5 |  447 | `}` |
|    - |  448 | `/* string\|bool mb_internal_encoding(?string $encoding = null) */` |
|    6 |  449 | `static int PH7_builtin_mb_internal_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  450 | `{` |
|    7 |  451 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|    3 |  452 | `		ph7_result_string(pCtx,"UTF-8",sizeof("UTF-8")-1);` |
|    3 |  453 | `		return PH7_OK;` |
|    - |  454 | `	}` |
|    5 |  455 | `	if( MbEncodingArg(pCtx,apArg[0],"mb_internal_encoding",1) < 0 ){` |
|    3 |  456 | `		return PH7_OK;` |
|    - |  457 | `	}` |
|    - |  458 | `	/* Only the UTF-8 family is accepted, and it is already the default */` |
|    3 |  459 | `	ph7_result_bool(pCtx,1);` |
|    3 |  460 | `	return PH7_OK;` |
|    4 |  461 | `}` |
|    - |  462 | `/* bool mb_check_encoding(string $value, ?string $encoding = null) */` |
|    4 |  463 | `static int PH7_builtin_mb_check_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  464 | `{` |
|    - |  465 | `	const unsigned char *z;` |
|    - |  466 | `	const char *zIn;` |
|    - |  467 | `	int nByte;` |
|    5 |  468 | `	sxu32 i = 0,nLen,cp;` |
|    5 |  469 | `	if( nArg < 1 ){` |
|  ! 0 |  470 | `		ph7_result_bool(pCtx,1);` |
|  ! 0 |  471 | `		return PH7_OK;` |
|    - |  472 | `	}` |
|    5 |  473 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_check_encoding",2) < 0 ){` |
|  ! 0 |  474 | `		return PH7_OK;` |
|    - |  475 | `	}` |
|    5 |  476 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    5 |  477 | `	z = (const unsigned char *)zIn;` |
|   29 |  478 | `	while( i < (sxu32)nByte ){` |
|   27 |  479 | `		cp = MbUtf8Decode(&z[i],(sxu32)nByte - i,&nLen);` |
|   27 |  480 | `		if( nLen == 1 && cp >= 0x80 ){` |
|    - |  481 | `			/* a lead/continuation byte that failed to decode */` |
|    3 |  482 | `			ph7_result_bool(pCtx,0);` |
|    3 |  483 | `			return PH7_OK;` |
|    - |  484 | `		}` |
|   25 |  485 | `		i += nLen;` |
|    1 |  486 | `	}` |
|    3 |  487 | `	ph7_result_bool(pCtx,1);` |
|    3 |  488 | `	return PH7_OK;` |
|    3 |  489 | `}` |
|    - |  490 | `/* int mb_strwidth(string $string, ?string $encoding = null) */` |
|    2 |  491 | `static int PH7_builtin_mb_strwidth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  492 | `{` |
|    - |  493 | `	const unsigned char *z;` |
|    - |  494 | `	const char *zIn;` |
|    - |  495 | `	int nByte;` |
|    3 |  496 | `	sxu32 i = 0,nLen,cp;` |
|    3 |  497 | `	ph7_int64 nWidth = 0;` |
|    3 |  498 | `	if( nArg < 1 ){` |
|  ! 0 |  499 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  500 | `		return PH7_OK;` |
|    - |  501 | `	}` |
|    3 |  502 | `	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_strwidth",2) < 0 ){` |
|  ! 0 |  503 | `		return PH7_OK;` |
|    - |  504 | `	}` |
|    3 |  505 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    3 |  506 | `	z = (const unsigned char *)zIn;` |
|   15 |  507 | `	while( i < (sxu32)nByte ){` |
|   13 |  508 | `		cp = MbUtf8Decode(&z[i],(sxu32)nByte - i,&nLen);` |
|   13 |  509 | `		i += nLen;` |
|    - |  510 | `		/* php's East Asian wide/fullwidth set */` |
|   12 |  511 | `		if( (cp >= 0x1100 && cp <= 0x115F) \|\| (cp >= 0x2E80 && cp <= 0xA4CF)` |
|    9 |  512 | `		 \|\| (cp >= 0xAC00 && cp <= 0xD7A3) \|\| (cp >= 0xF900 && cp <= 0xFAFF)` |
|    6 |  513 | `		 \|\| (cp >= 0xFE30 && cp <= 0xFE4F) \|\| (cp >= 0xFF00 && cp <= 0xFF60)` |
|    7 |  514 | `		 \|\| (cp >= 0xFFE0 && cp <= 0xFFE6) \|\| cp >= 0x20000 ){` |
|    7 |  515 | `			nWidth += 2;` |
|    4 |  516 | `		}else{` |
|    7 |  517 | `			nWidth += 1;` |
|    - |  518 | `		}` |
|    1 |  519 | `	}` |
|    3 |  520 | `	ph7_result_int64(pCtx,nWidth);` |
|    3 |  521 | `	return PH7_OK;` |
|    2 |  522 | `}` |
|    - |  523 |  |
|    - |  524 | `/*` |
|    - |  525 | ` * Install the mb_* functions (called from PH7_RegisterBuiltInFunction's` |
|    - |  526 | ` * table in builtin.c via these PH7_PRIVATE symbols).` |
|    - |  527 | ` */` |
|    5 |  528 | `PH7_PRIVATE int PH7_builtin_mb_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strlen(pCtx,nArg,apArg); }` |
|   13 |  529 | `PH7_PRIVATE int PH7_builtin_mb_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_substr(pCtx,nArg,apArg); }` |
|   15 |  530 | `PH7_PRIVATE int PH7_builtin_mb_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strtolower(pCtx,nArg,apArg); }` |
|    7 |  531 | `PH7_PRIVATE int PH7_builtin_mb_convert_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_convert_case(pCtx,nArg,apArg); }` |
|   13 |  532 | `PH7_PRIVATE int PH7_builtin_mb_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strpos(pCtx,nArg,apArg); }` |
|    9 |  533 | `PH7_PRIVATE int PH7_builtin_mb_str_split_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_str_split(pCtx,nArg,apArg); }` |
|    7 |  534 | `PH7_PRIVATE int PH7_builtin_mb_internal_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_internal_encoding(pCtx,nArg,apArg); }` |
|    5 |  535 | `PH7_PRIVATE int PH7_builtin_mb_check_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_check_encoding(pCtx,nArg,apArg); }` |
|    3 |  536 | `PH7_PRIVATE int PH7_builtin_mb_strwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strwidth(pCtx,nArg,apArg); }` |
|    - |  537 |  |
|    - |  538 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  539 |  |
