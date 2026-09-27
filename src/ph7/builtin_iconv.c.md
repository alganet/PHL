# src/ph7/builtin_iconv.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1098/1197 lines (91.73%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `/*` |
|     - |    7 | ` * Section:` |
|     - |    8 | ` *    php's iconv extension: the character-set converter and the string,` |
|     - |    9 | ` *    encoding-setting and MIME halves built on it.` |
|     - |   10 | ` * Status:` |
|     - |   11 | ` *    Stable.` |
|     - |   12 | ` *` |
|     - |   13 | ` * php's iconv is a thin shell over the C library's iconv(3), so its contract is` |
|     - |   14 | ` * really glibc's -- and that is what is reproduced here, over PHL's own` |
|     - |   15 | ` * converter rather than the platform's, so a Windows build answers what a Linux` |
|     - |   16 | ` * one does. Three things had to be measured rather than read, because each is` |
|     - |   17 | ` * the library's behaviour and not php's:` |
|     - |   18 | ` *` |
|     - |   19 | ` *   The NAME grammar. Everything before the first '/' is the code-set name,` |
|     - |   20 | ` *   compared case-insensitively after every character outside` |
|     - |   21 | ``  *   [alnum] `_ - . , :` is DROPPED -- so `" ASCII "`, `"A SCII"` and `"ASCII\t"` `` |
|     - |   22 | ` *   all name ASCII. The segment between the first and second '/' must be empty;` |
|     - |   23 | ` *   what follows is a list of error-handler tokens split on '/' and ',', and` |
|     - |   24 | `` *   `TRANSLIT` anywhere in that list turns transliteration on. That is why`` |
|     - |   25 | `` *   `ASCII//TRANSLITX` is not transliteration and `ASCII//A/B/TRANSLIT` is.`` |
|     - |   26 | ` *` |
|     - |   27 | ` *   //IGNORE is mostly NOT the library's. The library has one -- it drops a` |
|     - |   28 | ` *   character the target cannot hold and carries on -- but it then reports the` |
|     - |   29 | ` *   whole call as an illegal sequence anyway, so on its own it only turns one` |
|     - |   30 | `` *   failure into another: `iconv("UTF-8","ASCII//IGNORE,X","a\u{4e2d}b")` is`` |
|     - |   31 | `` *   FALSE. What makes //IGNORE useful is php's own, in `_php_check_ignore()`,`` |
|     - |   32 | ` *   and that check is a case-SENSITIVE SUFFIX test on the TO charset only, for` |
|     - |   33 | `` *   `//IGNORE` or `//IGNORE//TRANSLIT`, with a length guard that makes the bare`` |
|     - |   34 | `` *   string `"//IGNORE"` fail it. So `ASCII//ignore` ignores nothing,`` |
|     - |   35 | `` *   `UTF-8//IGNORE` as the FROM charset ignores nothing, and what php's does is`` |
|     - |   36 | ` *   byte-granular: on an illegal sequence it advances the input by ONE BYTE and` |
|     - |   37 | ` *   converts again, and a failure with a single byte left is taken for success.` |
|     - |   38 | ` *` |
|     - |   39 | ` *   The UTF-8 accepted here is the ORIGINAL six-byte one, not Unicode's` |
|     - |   40 | `` *   four-byte cut: `\xFC\x84\x80\x80\x80\x80` (U+4000000) converts, while`` |
|     - |   41 | ` *   overlong forms, the surrogate range and the lead bytes C0/C1/FE/FF do not.` |
|     - |   42 | ` *   The difference between the two diagnostics is only WHERE the input ran out:` |
|     - |   43 | ` *   a truncated but so-far-valid sequence at the END of the string is` |
|     - |   44 | `` *   `Detected an incomplete multibyte character`, and everything else is`` |
|     - |   45 | `` *   `Detected an illegal character`.`` |
|     - |   46 | ` *` |
|     - |   47 | ` * The transliteration table is glibc's own, swept out of it code point by code` |
|     - |   48 | ` * point (3425 rows, 967 of them the EMPTY replacement that makes a combining` |
|     - |   49 | ` * mark disappear, plus 55 whose ISO-8859-1 answer differs from their ASCII` |
|     - |   50 | `` * one). A code point with no row is `?`. PHL models UTF-8, ISO-8859-1 and`` |
|     - |   51 | ` * US-ASCII only (§10's scope cut, the same one mb_ carries); a php-valid name` |
|     - |   52 | ` * outside those three gets php's own "Wrong encoding" warning, which is what` |
|     - |   53 | ` * php answers for a name the platform's iconv does not have either.` |
|     - |   54 | ` *` |
|     - |   55 | ` * Verified differentially against php 8.5 over 16000 randomized conversions` |
|     - |   56 | ` * (every encoding pair, every suffix spelling, well-formed and malformed` |
|     - |   57 | ` * input). The one recorded divergence is in §7.4: an IGNORE token that is NOT` |
|     - |   58 | `` * php's suffix (`ASCII//TRANSLIT,IGNORE`, `ASCII//ignore`) reaches the`` |
|     - |   59 | ` * library's error handler, whose skip-and-continue behaviour on MALFORMED input` |
|     - |   60 | ` * is neither documented nor stable across iconv implementations; PHL answers` |
|     - |   61 | ` * php's own suffix rule there, so those spellings fail where php sometimes` |
|     - |   62 | ` * recovers. Well-formed input agrees for every spelling.` |
|     - |   63 | ` */` |
|     - |   64 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |   65 |  |
|     - |   66 | `/* --- Encodings --------------------------------------------------------- */` |
|     - |   67 |  |
|     - |   68 | `#define ICV_UTF8    0` |
|     - |   69 | `#define ICV_LATIN1  1` |
|     - |   70 | `#define ICV_ASCII   2` |
|     - |   71 |  |
|     - |   72 | `/*` |
|     - |   73 | ` * php's own cap on an encoding NAME, checked before the name is looked at:` |
|     - |   74 | `` * ICONV_CSNMAXLEN is 64 and the test is `>=`, so a 64-character name is already`` |
|     - |   75 | ` * "exceeds the maximum allowed length of 64 characters".` |
|     - |   76 | ` */` |
|     - |   77 | `#define ICV_CSNMAXLEN 64` |
|     - |   78 |  |
|     - |   79 | `/*` |
|     - |   80 | ` * The names glibc registers for the three code sets PHL models, minus its` |
|     - |   81 | `` * numeric OSF/IBM aliases (`OSF00010020`, `IBM903`…), which are recorded as`` |
|     - |   82 | ` * refused rather than implemented. Compared after ICV normalisation, so the` |
|     - |   83 | ` * spelling here is the canonical upper-case one.` |
|     - |   84 | ` */` |
|     - |   85 | `static const struct IcvEncName {` |
|     - |   86 | `	const char *zName;` |
|     - |   87 | `	int iEnc;` |
|     - |   88 | `} aIcvEncName[] = {` |
|     - |   89 | `	{ "UTF-8",            ICV_UTF8   },` |
|     - |   90 | `	{ "UTF8",             ICV_UTF8   },` |
|     - |   91 | `	{ "ISO-IR-193",       ICV_UTF8   },` |
|     - |   92 | `	{ "ISO-8859-1",       ICV_LATIN1 },` |
|     - |   93 | `	{ "ISO_8859-1",       ICV_LATIN1 },` |
|     - |   94 | `	{ "ISO8859-1",        ICV_LATIN1 },` |
|     - |   95 | `	{ "ISO88591",         ICV_LATIN1 },` |
|     - |   96 | `	{ "ISO_8859-1:1987",  ICV_LATIN1 },` |
|     - |   97 | `	{ "ISO-IR-100",       ICV_LATIN1 },` |
|     - |   98 | `	{ "LATIN1",           ICV_LATIN1 },` |
|     - |   99 | `	{ "L1",               ICV_LATIN1 },` |
|     - |  100 | `	{ "CP819",            ICV_LATIN1 },` |
|     - |  101 | `	{ "IBM819",           ICV_LATIN1 },` |
|     - |  102 | `	{ "CSISOLATIN1",      ICV_LATIN1 },` |
|     - |  103 | `	{ "8859_1",           ICV_LATIN1 },` |
|     - |  104 | `	{ "ASCII",            ICV_ASCII  },` |
|     - |  105 | `	{ "US-ASCII",         ICV_ASCII  },` |
|     - |  106 | `	{ "US",               ICV_ASCII  },` |
|     - |  107 | `	{ "ANSI_X3.4-1968",   ICV_ASCII  },` |
|     - |  108 | `	{ "ANSI_X3.4-1986",   ICV_ASCII  },` |
|     - |  109 | `	{ "ANSI_X3.4",        ICV_ASCII  },` |
|     - |  110 | `	{ "ISO646-US",        ICV_ASCII  },` |
|     - |  111 | `	{ "ISO_646.IRV:1991", ICV_ASCII  },` |
|     - |  112 | `	{ "ISO-IR-6",         ICV_ASCII  },` |
|     - |  113 | `	{ "CP367",            ICV_ASCII  },` |
|     - |  114 | `	{ "IBM367",           ICV_ASCII  },` |
|     - |  115 | `	{ "CSASCII",          ICV_ASCII  }` |
|     - |  116 | `};` |
|     - |  117 |  |
|     - |  118 | `/* Is c one of the characters the name normaliser KEEPS? */` |
|  8288 |  119 | `static int IcvNameChar(int c)` |
|     3 |  120 | `{` |
|  8873 |  121 | `	return (c >= '0' && c <= '9') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= 'a' && c <= 'z')` |
| 12432 |  122 | `		\|\| c == '_' \|\| c == '-' \|\| c == '.' \|\| c == ',' \|\| c == ':';` |
|     3 |  123 | `}` |
|  8306 |  124 | `static int IcvUpper(int c)` |
|     3 |  125 | `{` |
|  8309 |  126 | `	return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;` |
|     3 |  127 | `}` |
|     - |  128 |  |
|     - |  129 | `/* What a parsed charset argument came to. */` |
|     - |  130 | `typedef struct icv_cs icv_cs;` |
|     - |  131 | `struct icv_cs {` |
|     - |  132 | `	int iEnc;        /* ICV_* id, or -1 when the name is not one PHL models */` |
|     - |  133 | `	int bTranslit;   /* a TRANSLIT token appeared in the error-handler list */` |
|     - |  134 | `	int bIgnore;     /* an IGNORE token did -- the LIBRARY's ignore, not php's */` |
|     - |  135 | `	/* The name as it was GIVEN, kept because the string family reports an` |
|     - |  136 | `	 * unknown one late -- at the point a conversion would have been opened --` |
|     - |  137 | `	 * and by then the argument that carried it may be gone. Bounded by the` |
|     - |  138 | `	 * length cap, which is checked before any of this. */` |
|     - |  139 | `	char zName[ICV_CSNMAXLEN];` |
|     - |  140 | `	int nName;` |
|     - |  141 | `};` |
|     - |  142 |  |
|     - |  143 | `/*` |
|     - |  144 | ` * How much of a charset argument the converter can SEE. php's ZPP hands the` |
|     - |  145 | ` * whole php string down, but iconv_open() takes a C string, so a name stops at` |
|     - |  146 | ` * its first NUL -- and so does the "Wrong encoding" message, which prints the` |
|     - |  147 | ` * same pointer with %s.` |
|     - |  148 | ` */` |
|  1614 |  149 | `static int IcvNameLen(const char *z,int n)` |
|     3 |  150 | `{` |
|     - |  151 | `	int i;` |
| 13887 |  152 | `	for( i = 0 ; i < n && z[i] != 0 ; ++i ){}` |
|  1617 |  153 | `	return i;` |
|     3 |  154 | `}` |
|     - |  155 | `/* Copy z[0..n-1] into zOut, keeping only the characters the normaliser keeps` |
|     - |  156 | ` * and upper-casing them. Answers the length written, capped at nOut. */` |
|  1406 |  157 | `static int IcvNormalize(char *zOut,int nOut,const char *z,int n)` |
|     3 |  158 | `{` |
|  1409 |  159 | `	int i,k = 0;` |
|  8557 |  160 | `	for( i = 0 ; i < n ; ++i ){` |
|  7151 |  161 | `		int c = (unsigned char)z[i];` |
|  7151 |  162 | `		if( IcvNameChar(c) && k < nOut ){` |
|  7137 |  163 | `			zOut[k++] = (char)IcvUpper(c);` |
|  3567 |  164 | `		}` |
|  3577 |  165 | `	}` |
|  1409 |  166 | `	return k;` |
|     3 |  167 | `}` |
|     - |  168 | `/*` |
|     - |  169 | ` * Parse one charset argument. The code-set name is z[0..] up to the first '/',` |
|     - |  170 | ` * normalised; the segment up to the second '/' must be empty; every remaining` |
|     - |  171 | `` * '/'- or ','-separated token is an error handler, and `TRANSLIT` is the one`` |
|     - |  172 | ` * that means anything here.` |
|     - |  173 | ` */` |
|  1260 |  174 | `static void IcvParseCharset(const char *z,int n,icv_cs *pCs)` |
|     3 |  175 | `{` |
|     - |  176 | `	char zBuf[ICV_CSNMAXLEN];` |
|     - |  177 | `	int iFirst,iSecond,nBuf,k;` |
|  1263 |  178 | `	pCs->iEnc = -1;` |
|  1263 |  179 | `	pCs->bTranslit = 0;` |
|  1263 |  180 | `	pCs->bIgnore = 0;` |
|  1263 |  181 | `	n = IcvNameLen(z,n);` |
|  1263 |  182 | `	pCs->nName = n < ICV_CSNMAXLEN ? n : ICV_CSNMAXLEN;` |
|  1263 |  183 | `	SyMemcpy(z,pCs->zName,(sxu32)pCs->nName);` |
|  8399 |  184 | `	for( iFirst = 0 ; iFirst < n && z[iFirst] != '/' ; ++iFirst ){}` |
|  1263 |  185 | `	nBuf = IcvNormalize(zBuf,(int)sizeof(zBuf),z,iFirst);` |
|  1263 |  186 | `	if( nBuf == 0 ){` |
|     - |  187 | `		/* php hands the name straight to iconv_open(), where an empty CODE SET` |
|     - |  188 | ``		 * means the LOCALE's charset -- which is why `"//IGNORE"` on its own`` |
|     - |  189 | `		 * names one. PHL has no locale and one charset it is written in, so the` |
|     - |  190 | `		 * empty name is UTF-8 here: what php answers on any UTF-8 locale,` |
|     - |  191 | `		 * deterministically rather than by environment. */` |
|     3 |  192 | `		pCs->iEnc = ICV_UTF8;` |
|     1 |  193 | `	}` |
|  6473 |  194 | `	for( k = 0 ; nBuf > 0 && k < (int)SX_ARRAYSIZE(aIcvEncName) ; ++k ){` |
|  6399 |  195 | `		const char *zCand = aIcvEncName[k].zName;` |
|  6399 |  196 | `		int nCand = (int)SyStrlen(zCand);` |
|  6399 |  197 | `		if( nCand == nBuf && SyMemcmp(zCand,zBuf,(sxu32)nCand) == 0 ){` |
|  1189 |  198 | `			pCs->iEnc = aIcvEncName[k].iEnc;` |
|  1189 |  199 | `			break;` |
|     - |  200 | `		}` |
|  2608 |  201 | `	}` |
|  1263 |  202 | `	if( iFirst >= n ){` |
|  1117 |  203 | `		return;` |
|     - |  204 | `	}` |
|     - |  205 | `	/* Between the first and second '/' is the segment that carries no handler,` |
|     - |  206 | ``	 * and it has to be empty: `ASCII/x/TRANSLIT` names nothing at all, which is`` |
|     - |  207 | ``	 * why php answers "Wrong encoding" for it while `ASCII/ /TRANSLIT` -- whose`` |
|     - |  208 | `	 * space the normaliser drops -- converts. */` |
|   159 |  209 | `	for( iSecond = iFirst + 1 ; iSecond < n && z[iSecond] != '/' ; ++iSecond ){}` |
|   147 |  210 | `	if( IcvNormalize(zBuf,(int)sizeof(zBuf),&z[iFirst+1],iSecond - iFirst - 1) > 0 ){` |
|     5 |  211 | `		pCs->iEnc = -1;` |
|     2 |  212 | `	}` |
|     - |  213 | `	/* Everything past it is a '/'- or ','-separated list of error handlers. */` |
|   147 |  214 | `	nBuf = 0;` |
|  1457 |  215 | `	for( k = iSecond + 1 ; k <= n ; ++k ){` |
|  1311 |  216 | `		int c = (k < n) ? (unsigned char)z[k] : '/';` |
|  1311 |  217 | `		if( c == '/' \|\| c == ',' ){` |
|   171 |  218 | `			if( nBuf == 8 && SyMemcmp(zBuf,"TRANSLIT",8) == 0 ){` |
|   109 |  219 | `				pCs->bTranslit = 1;` |
|    54 |  220 | `			}` |
|   171 |  221 | `			if( nBuf == 6 && SyMemcmp(zBuf,"IGNORE",6) == 0 ){` |
|    37 |  222 | `				pCs->bIgnore = 1;` |
|    18 |  223 | `			}` |
|   171 |  224 | `			nBuf = 0;` |
|   171 |  225 | `			continue;` |
|     - |  226 | `		}` |
|  1141 |  227 | `		if( IcvNameChar(c) ){` |
|  1141 |  228 | `			if( nBuf < (int)sizeof(zBuf) ){` |
|  1141 |  229 | `				zBuf[nBuf++] = (char)IcvUpper(c);` |
|   571 |  230 | `			}else{` |
|   ! 0 |  231 | `				nBuf = (int)sizeof(zBuf) + 1;` |
|     - |  232 | `			}` |
|   570 |  233 | `		}` |
|   571 |  234 | `	}` |
|   633 |  235 | `}` |
|     - |  236 |  |
|     - |  237 | `/*` |
|     - |  238 | `` * php's `_php_check_ignore()`: a case-SENSITIVE suffix test on the RAW name,`` |
|     - |  239 | ` * with the length guard that keeps the bare "//IGNORE" from matching.` |
|     - |  240 | ` */` |
|   282 |  241 | `static int IcvCheckIgnore(const char *z,int n)` |
|     1 |  242 | `{` |
|   283 |  243 | `	n = IcvNameLen(z,n);` |
|   283 |  244 | `	if( n >= 9 && SyMemcmp(&z[n-8],"//IGNORE",8) == 0 ){` |
|    29 |  245 | `		return 1;` |
|     - |  246 | `	}` |
|   255 |  247 | `	if( n >= 19 && SyMemcmp(&z[n-18],"//IGNORE//TRANSLIT",18) == 0 ){` |
|     3 |  248 | `		return 1;` |
|     - |  249 | `	}` |
|   253 |  250 | `	return 0;` |
|   142 |  251 | `}` |
|     - |  252 |  |
|     - |  253 | `/* --- Transliteration --------------------------------------------------- */` |
|     - |  254 |  |
|     - |  255 | `/*` |
|     - |  256 | ` * One transliteration row: the source code point and the offset/length of its` |
|     - |  257 | ` * replacement in the shared blob beside it. glibc's table gives an ORDERED list` |
|     - |  258 | ` * of candidate replacements and takes the first that fits the target, which for` |
|     - |  259 | ` * the two targets PHL can miss with comes to exactly two columns: the general` |
|     - |  260 | ` * (ASCII) answer here, and the 55 rows whose ISO-8859-1 answer is a different` |
|     - |  261 | ` * string.` |
|     - |  262 | ` */` |
|     - |  263 | `typedef struct icv_translit icv_translit;` |
|     - |  264 | `struct icv_translit {` |
|     - |  265 | `	sxu32 cp;` |
|     - |  266 | `	sxu16 iOfft;` |
|     - |  267 | `	sxu16 nByte;` |
|     - |  268 | `};` |
|     - |  269 | `#include "builtin_iconv_translit.h"` |
|     - |  270 |  |
|     - |  271 | `/* Binary-search a cp-sorted transliteration table. */` |
|   120 |  272 | `static const icv_translit * IcvTranslitFind(const icv_translit *aTab,int nTab,sxu32 cp)` |
|     1 |  273 | `{` |
|   121 |  274 | `	int lo = 0,hi = nTab - 1;` |
|  1271 |  275 | `	while( lo <= hi ){` |
|  1257 |  276 | `		int mid = lo + (hi - lo) / 2;` |
|  1257 |  277 | `		if( aTab[mid].cp == cp ){` |
|   107 |  278 | `			return &aTab[mid];` |
|     - |  279 | `		}` |
|  1151 |  280 | `		if( aTab[mid].cp < cp ){` |
|   525 |  281 | `			lo = mid + 1;` |
|   263 |  282 | `		}else{` |
|   627 |  283 | `			hi = mid - 1;` |
|     - |  284 | `		}` |
|     1 |  285 | `	}` |
|    15 |  286 | `	return 0;` |
|    61 |  287 | `}` |
|     - |  288 |  |
|     - |  289 | `/* --- The converter ----------------------------------------------------- */` |
|     - |  290 |  |
|     - |  291 | `/* php's php_iconv_err_t, in php's own order of preference. */` |
|     - |  292 | `#define ICV_OK            0` |
|     - |  293 | `#define ICV_ILLEGAL_CHAR  1   /* an incomplete sequence at the end of the input */` |
|     - |  294 | `#define ICV_ILLEGAL_SEQ   2   /* anything else the source encoding refuses */` |
|     - |  295 | `#define ICV_WRONG_CHARSET 3` |
|     - |  296 | `#define ICV_TOO_BIG       4` |
|     - |  297 | `#define ICV_MALFORMED     5` |
|     - |  298 | `#define ICV_OUT_BY_BOUNDS 6` |
|     - |  299 | `#define ICV_UNKNOWN_ERR   7` |
|     - |  300 | `#define ICV_TOO_LONG      8` |
|     - |  301 |  |
|     - |  302 | `/*` |
|     - |  303 | ` * Decode the character at z[0..n-1] under iEnc. Answers its byte length with` |
|     - |  304 | ` * *pCp set to the code point, or 0 with *pErr set to the diagnostic the` |
|     - |  305 | ` * sequence earns. The UTF-8 accepted is the original six-byte encoding, minus` |
|     - |  306 | ` * overlongs, the surrogate range and the C0/C1/FE/FF lead bytes -- glibc's set,` |
|     - |  307 | ` * not Unicode's.` |
|     - |  308 | ` */` |
|  6936 |  309 | `static int IcvDecode(const unsigned char *z,int n,int iEnc,sxu32 *pCp,int *pErr)` |
|     3 |  310 | `{` |
|     - |  311 | `	static const struct { unsigned char iLow,iHigh; int nSeq; sxu32 iMin; } aLead[] = {` |
|     - |  312 | `		{ 0xC2,0xDF,2,0x80       },` |
|     - |  313 | `		{ 0xE0,0xEF,3,0x800      },` |
|     - |  314 | `		{ 0xF0,0xF7,4,0x10000    },` |
|     - |  315 | `		{ 0xF8,0xFB,5,0x200000   },` |
|     - |  316 | `		{ 0xFC,0xFD,6,0x4000000  }` |
|     - |  317 | `	};` |
|  6939 |  318 | `	unsigned int c = z[0];` |
|     - |  319 | `	int i,k;` |
|  6939 |  320 | `	if( iEnc == ICV_LATIN1 ){` |
|   160 |  321 | `		*pCp = c;` |
|   160 |  322 | `		return 1;` |
|     - |  323 | `	}` |
|  6781 |  324 | `	if( iEnc == ICV_ASCII ){` |
|  1215 |  325 | `		if( c > 0x7F ){` |
|     8 |  326 | `			*pErr = ICV_ILLEGAL_SEQ;` |
|     8 |  327 | `			return 0;` |
|     - |  328 | `		}` |
|  1209 |  329 | `		*pCp = c;` |
|  1209 |  330 | `		return 1;` |
|     - |  331 | `	}` |
|  5568 |  332 | `	if( c < 0x80 ){` |
|  4816 |  333 | `		*pCp = c;` |
|  4816 |  334 | `		return 1;` |
|     - |  335 | `	}` |
|  1224 |  336 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(aLead) ; ++k ){` |
|     - |  337 | `		int nSeq;` |
|     - |  338 | `		sxu32 cp;` |
|  1172 |  339 | `		if( c < aLead[k].iLow \|\| c > aLead[k].iHigh ){` |
|   472 |  340 | `			continue;` |
|     - |  341 | `		}` |
|   702 |  342 | `		nSeq = aLead[k].nSeq;` |
|   702 |  343 | `		cp = c & (sxu32)(0x7F >> nSeq);` |
|  1528 |  344 | `		for( i = 1 ; i < nSeq ; ++i ){` |
|   862 |  345 | `			if( i >= n ){` |
|     - |  346 | `				/* Ran out mid-sequence with everything so far valid: php's` |
|     - |  347 | `				 * "incomplete multibyte character", not its "illegal" one. */` |
|    29 |  348 | `				*pErr = ICV_ILLEGAL_CHAR;` |
|    29 |  349 | `				return 0;` |
|     - |  350 | `			}` |
|   834 |  351 | `			if( (z[i] & 0xC0) != 0x80 ){` |
|     8 |  352 | `				*pErr = ICV_ILLEGAL_SEQ;` |
|     8 |  353 | `				return 0;` |
|     - |  354 | `			}` |
|   828 |  355 | `			cp = (cp << 6) \| (sxu32)(z[i] & 0x3F);` |
|   415 |  356 | `		}` |
|   668 |  357 | `		if( cp < aLead[k].iMin \|\| (cp >= 0xD800 && cp <= 0xDFFF) ){` |
|     9 |  358 | `			*pErr = ICV_ILLEGAL_SEQ;   /* overlong, or a lone surrogate */` |
|     9 |  359 | `			return 0;` |
|     - |  360 | `		}` |
|   660 |  361 | `		*pCp = cp;` |
|   660 |  362 | `		return nSeq;` |
|   ! 0 |  363 | `	}` |
|    53 |  364 | `	*pErr = ICV_ILLEGAL_SEQ;` |
|    53 |  365 | `	return 0;` |
|  3471 |  366 | `}` |
|     - |  367 |  |
|     - |  368 | `/* Write cp in iEnc when the encoding can hold it; 0 when it cannot. */` |
|  6050 |  369 | `static int IcvEncodeDirect(SyBlob *pOut,sxu32 cp,int iEnc)` |
|     2 |  370 | `{` |
|     - |  371 | `	unsigned char zEnc[6];` |
|  6052 |  372 | `	int n = 0;` |
|  6052 |  373 | `	if( iEnc == ICV_ASCII ){` |
|   653 |  374 | `		if( cp > 0x7F ){` |
|   137 |  375 | `			return 0;` |
|     1 |  376 | `		}` |
|  5658 |  377 | `	}else if( iEnc == ICV_LATIN1 ){` |
|   165 |  378 | `		if( cp > 0xFF ){` |
|    25 |  379 | `			return 0;` |
|     - |  380 | `		}` |
|    71 |  381 | `	}else{` |
|     - |  382 | `		/* The six-byte encoding, so every code point IcvDecode() produced can` |
|     - |  383 | `		 * be written back. */` |
|  5236 |  384 | `		if( cp < 0x80 ){` |
|  4856 |  385 | `			zEnc[n++] = (unsigned char)cp;` |
|  2808 |  386 | `		}else if( cp < 0x800 ){` |
|   371 |  387 | `			zEnc[n++] = (unsigned char)(0xC0 \| (cp >> 6));` |
|   371 |  388 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|   196 |  389 | `		}else if( cp < 0x10000 ){` |
|   ! 0 |  390 | `			zEnc[n++] = (unsigned char)(0xE0 \| (cp >> 12));` |
|   ! 0 |  391 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|   ! 0 |  392 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|    11 |  393 | `		}else if( cp < 0x200000 ){` |
|     7 |  394 | `			zEnc[n++] = (unsigned char)(0xF0 \| (cp >> 18));` |
|     7 |  395 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|     7 |  396 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|     7 |  397 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|     8 |  398 | `		}else if( cp < 0x4000000 ){` |
|     3 |  399 | `			zEnc[n++] = (unsigned char)(0xF8 \| (cp >> 24));` |
|     3 |  400 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 18) & 0x3F));` |
|     3 |  401 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|     3 |  402 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|     3 |  403 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|     2 |  404 | `		}else{` |
|     3 |  405 | `			zEnc[n++] = (unsigned char)(0xFC \| (cp >> 30));` |
|     3 |  406 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 24) & 0x3F));` |
|     3 |  407 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 18) & 0x3F));` |
|     3 |  408 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|     3 |  409 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|     3 |  410 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|     - |  411 | `		}` |
|  5236 |  412 | `		SyBlobAppend(pOut,zEnc,(sxu32)n);` |
|  5236 |  413 | `		return 1;` |
|     - |  414 | `	}` |
|   657 |  415 | `	zEnc[0] = (unsigned char)cp;` |
|   657 |  416 | `	SyBlobAppend(pOut,zEnc,1);` |
|   657 |  417 | `	return 1;` |
|  3027 |  418 | `}` |
|     - |  419 |  |
|     - |  420 | `/*` |
|     - |  421 | ` * Write cp in iEnc, transliterating when bTranslit is set. Answers 0 only when` |
|     - |  422 | ` * the encoding cannot hold cp and transliteration is off -- the '?' a` |
|     - |  423 | ` * transliterated miss produces is glibc's own answer, and it is a SUCCESS, as` |
|     - |  424 | ` * is the EMPTY replacement a combining mark transliterates to.` |
|     - |  425 | ` */` |
|  6050 |  426 | `static int IcvEncode(SyBlob *pOut,sxu32 cp,int iEnc,int bTranslit)` |
|     2 |  427 | `{` |
|     - |  428 | `	const icv_translit *pRow;` |
|  6052 |  429 | `	if( IcvEncodeDirect(pOut,cp,iEnc) ){` |
|  5892 |  430 | `		return 1;` |
|     - |  431 | `	}` |
|   161 |  432 | `	if( !bTranslit ){` |
|    45 |  433 | `		return 0;` |
|     - |  434 | `	}` |
|   116 |  435 | `	if( iEnc == ICV_LATIN1` |
|    66 |  436 | `	 && (pRow = IcvTranslitFind(aIcvTranslit1,(int)SX_ARRAYSIZE(aIcvTranslit1),cp)) != 0 ){` |
|    11 |  437 | `		SyBlobAppend(pOut,&zIcvTranslit1[pRow->iOfft],pRow->nByte);` |
|    11 |  438 | `		return 1;` |
|     - |  439 | `	}` |
|   107 |  440 | `	pRow = IcvTranslitFind(aIcvTranslit,(int)SX_ARRAYSIZE(aIcvTranslit),cp);` |
|   107 |  441 | `	if( pRow ){` |
|    97 |  442 | `		SyBlobAppend(pOut,&zIcvTranslit[pRow->iOfft],pRow->nByte);` |
|    97 |  443 | `		return 1;` |
|     - |  444 | `	}` |
|    11 |  445 | `	SyBlobAppend(pOut,"?",1);` |
|    11 |  446 | `	return 1;` |
|  3027 |  447 | `}` |
|     - |  448 |  |
|     - |  449 | `/*` |
|     - |  450 | ` * One iconv(3) CALL over zIn[*pi..nIn-1]: convert until the input runs out or` |
|     - |  451 | ` * the library would stop, leaving *pi where it stopped and answering the status` |
|     - |  452 | ` * php's loop then reads.` |
|     - |  453 | ` *` |
|     - |  454 | ` * There are TWO ignore mechanisms and they are not the same one. The LIBRARY's` |
|     - |  455 | `` * (`pTo->bIgnore`, an IGNORE token anywhere in the error-handler list) drops a`` |
|     - |  456 | ` * character the TARGET cannot hold and carries on, and reports the whole call` |
|     - |  457 | ` * as an illegal sequence AFTERWARDS -- so on its own it only ever turns one` |
|     - |  458 | `` * failure into another, which is why `iconv("UTF-8","ASCII//IGNORE,X",…)` is`` |
|     - |  459 | `` * still FALSE. php's own (`bIgnore` in IcvConvert()) is what makes //IGNORE`` |
|     - |  460 | ` * useful, and it lives one level up.` |
|     - |  461 | ` *` |
|     - |  462 | ` * *pbDropped is that afterwards, and it is STICKY across the calls php makes:` |
|     - |  463 | ` * once a character has been dropped because the target could not hold it, a` |
|     - |  464 | ` * TRUNCATED tail is reported as an illegal sequence rather than an incomplete` |
|     - |  465 | ` * character -- which is what lets` |
|     - |  466 | `` * `iconv("UTF-8","ISO-8859-1//IGNORE","\xE1\x88\xA2ab\xFD")` answer "ab"`` |
|     - |  467 | ` * while the same string without the unconvertible character in front of it is a` |
|     - |  468 | ` * hard failure.` |
|     - |  469 | ` */` |
|  1286 |  470 | `static int IcvConvertCall(SyBlob *pOut,const char *zIn,int nIn,int *pi,` |
|     - |  471 | `	int iFrom,const icv_cs *pTo,int *pbDropped)` |
|     2 |  472 | `{` |
|  1288 |  473 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|  1288 |  474 | `	int i = *pi,rc = ICV_OK;` |
|  3442 |  475 | `	while( i < nIn ){` |
|  2242 |  476 | `		sxu32 cp = 0;` |
|  2242 |  477 | `		int err = ICV_OK;` |
|  2242 |  478 | `		int nSeq = IcvDecode(&z[i],nIn - i,iFrom,&cp,&err);` |
|  2242 |  479 | `		if( nSeq == 0 ){` |
|    69 |  480 | `			rc = err;` |
|    78 |  481 | `			break;` |
|     - |  482 | `		}` |
|  2174 |  483 | `		if( IcvEncode(pOut,cp,pTo->iEnc,pTo->bTranslit) ){` |
|  2132 |  484 | `			i += nSeq;` |
|  2144 |  485 | `			continue;` |
|     - |  486 | `		}` |
|    43 |  487 | `		if( pTo->bIgnore ){` |
|    25 |  488 | `			i += nSeq;` |
|    25 |  489 | `			*pbDropped = 1;` |
|    25 |  490 | `			continue;` |
|     - |  491 | `		}` |
|    19 |  492 | `		rc = ICV_ILLEGAL_SEQ;` |
|    19 |  493 | `		break;` |
|   ! 0 |  494 | `	}` |
|  1288 |  495 | `	if( *pbDropped && (rc == ICV_OK \|\| rc == ICV_ILLEGAL_CHAR) ){` |
|    15 |  496 | `		rc = ICV_ILLEGAL_SEQ;` |
|     7 |  497 | `	}` |
|  1288 |  498 | `	*pi = i;` |
|  1288 |  499 | `	return rc;` |
|     2 |  500 | `}` |
|     - |  501 |  |
|     - |  502 | `/*` |
|     - |  503 | ` * The whole of php_iconv_string(): drive IcvConvertCall() the way php's loop` |
|     - |  504 | `` * drives iconv(3). php's own `//IGNORE` handling sits HERE and is byte-granular`` |
|     - |  505 | ` * -- on an illegal sequence it steps the input forward by one byte and converts` |
|     - |  506 | ` * again, and a failure with a single byte left is taken for a success.` |
|     - |  507 | ` */` |
|  1270 |  508 | `static int IcvConvert(SyBlob *pOut,const char *zIn,int nIn,int iFrom,const icv_cs *pTo,int bIgnore)` |
|     2 |  509 | `{` |
|  1272 |  510 | `	int i = 0,bDropped = 0;` |
|   643 |  511 | `	for(;;){` |
|  1288 |  512 | `		int rc = IcvConvertCall(pOut,zIn,nIn,&i,iFrom,pTo,&bDropped);` |
|  1288 |  513 | `		if( rc == ICV_OK ){` |
|  1192 |  514 | `			return ICV_OK;` |
|     - |  515 | `		}` |
|    97 |  516 | `		if( bIgnore && rc == ICV_ILLEGAL_SEQ ){` |
|    31 |  517 | `			if( nIn - i <= 1 ){` |
|    15 |  518 | `				return ICV_OK;` |
|     - |  519 | `			}` |
|    17 |  520 | `			i++;` |
|    17 |  521 | `			continue;` |
|     - |  522 | `		}` |
|    67 |  523 | `		return rc;` |
|   ! 0 |  524 | `	}` |
|   637 |  525 | `}` |
|     - |  526 |  |
|     - |  527 | `/*` |
|     - |  528 | ` * Convert from zIn[*pi..] until the OUTPUT would pass iRoom bytes, leaving *pi` |
|     - |  529 | ` * on a character boundary and *pnTook set to how many characters went. This is` |
|     - |  530 | ` * how the MIME encoder fills one encoded word: it has a byte budget for the` |
|     - |  531 | ` * line and has to stop on a whole character, which is exactly what iconv(3)` |
|     - |  532 | ` * does when its output buffer runs out.` |
|     - |  533 | ` */` |
|   222 |  534 | `static int IcvConvertBounded(SyBlob *pOut,const char *zIn,int nIn,int *pi,` |
|     - |  535 | `	int iFrom,const icv_cs *pTo,sxi64 iRoom,int *pnTook)` |
|     1 |  536 | `{` |
|   223 |  537 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   223 |  538 | `	int i = *pi,nTook = 0;` |
|  4019 |  539 | `	while( i < nIn ){` |
|  3881 |  540 | `		sxu32 cp = 0;` |
|  3881 |  541 | `		int err = ICV_OK;` |
|  3881 |  542 | `		sxu32 nBefore = SyBlobLength(pOut);` |
|  3881 |  543 | `		int nSeq = IcvDecode(&z[i],nIn - i,iFrom,&cp,&err);` |
|  3881 |  544 | `		if( nSeq == 0 ){` |
|     3 |  545 | `			*pi = i;` |
|     3 |  546 | `			*pnTook = nTook;` |
|     4 |  547 | `			return err;` |
|     - |  548 | `		}` |
|  3879 |  549 | `		if( !IcvEncode(pOut,cp,pTo->iEnc,pTo->bTranslit) ){` |
|     3 |  550 | `			*pi = i;` |
|     3 |  551 | `			*pnTook = nTook;` |
|     3 |  552 | `			return ICV_ILLEGAL_SEQ;` |
|     - |  553 | `		}` |
|  3877 |  554 | `		if( (sxi64)SyBlobLength(pOut) > iRoom ){` |
|     - |  555 | `			/* One character too many: give the bytes back and stop here. */` |
|    81 |  556 | `			SyBlobLength(pOut) = nBefore;` |
|    81 |  557 | `			break;` |
|     - |  558 | `		}` |
|  3797 |  559 | `		i += nSeq;` |
|  3797 |  560 | `		nTook++;` |
|     1 |  561 | `	}` |
|   219 |  562 | `	*pi = i;` |
|   219 |  563 | `	*pnTook = nTook;` |
|   219 |  564 | `	return ICV_OK;` |
|   112 |  565 | `}` |
|     - |  566 |  |
|     - |  567 | `/* --- Diagnostics ------------------------------------------------------- */` |
|     - |  568 |  |
|     - |  569 | `/*` |
|     - |  570 | `` * php's `_php_iconv_show_error()`. The context prefixes the function name, so`` |
|     - |  571 | ` * these are the message bodies only; the two decoder diagnostics are E_NOTICE` |
|     - |  572 | ` * and everything else E_WARNING, which is php's own split.` |
|     - |  573 | ` */` |
|   196 |  574 | `static void IcvShowError(ph7_context *pCtx,int err,const char *zTo,int nTo,` |
|     - |  575 | `	const char *zFrom,int nFrom)` |
|     3 |  576 | `{` |
|   199 |  577 | `	switch( err ){` |
|   ! 0 |  578 | `	case ICV_OK:` |
|   ! 0 |  579 | `		break;` |
|    30 |  580 | `	case ICV_WRONG_CHARSET:` |
|    92 |  581 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - |  582 | `			"Wrong encoding, conversion from \"%.*s\" to \"%.*s\" is not allowed",` |
|    30 |  583 | `			nFrom,zFrom,nTo,zTo);` |
|    62 |  584 | `		break;` |
|    12 |  585 | `	case ICV_ILLEGAL_CHAR:` |
|    25 |  586 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,` |
|     - |  587 | `			"Detected an incomplete multibyte character in input string");` |
|    25 |  588 | `		break;` |
|    33 |  589 | `	case ICV_ILLEGAL_SEQ:` |
|    68 |  590 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,` |
|     - |  591 | `			"Detected an illegal character in input string");` |
|    68 |  592 | `		break;` |
|     4 |  593 | `	case ICV_TOO_BIG:` |
|     9 |  594 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Buffer length exceeded");` |
|     9 |  595 | `		break;` |
|    16 |  596 | `	case ICV_MALFORMED:` |
|    33 |  597 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Malformed string");` |
|    33 |  598 | `		break;` |
|     3 |  599 | `	case ICV_UNKNOWN_ERR:` |
|     - |  600 | ``		/* php prints `errno` here, and nothing has SET it -- the number is`` |
|     - |  601 | `		 * whatever the last libc call in the process left behind (22 and 84` |
|     - |  602 | `		 * are what the same input produces on this box, in the same run). PHL` |
|     - |  603 | `		 * has no stale errno to leak, so it says 0; twin-paired. */` |
|     7 |  604 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Unknown error (0)");` |
|     6 |  605 | `		break;` |
|   ! 0 |  606 | `	default:` |
|   ! 0 |  607 | `		break;` |
|     - |  608 | `	}` |
|   199 |  609 | `}` |
|     - |  610 |  |
|     - |  611 | `/*` |
|     - |  612 | ` * Read one charset ARGUMENT: php's length cap first (it is checked before the` |
|     - |  613 | ` * name is looked at, so an over-long name never reaches the "Wrong encoding"` |
|     - |  614 | ` * message), then the grammar. Answers 0 after raising the length warning.` |
|     - |  615 | ` */` |
|  1008 |  616 | `static int IcvCharsetArg(ph7_context *pCtx,const char *z,int n,icv_cs *pCs)` |
|     3 |  617 | `{` |
|  1011 |  618 | `	if( n >= ICV_CSNMAXLEN ){` |
|    11 |  619 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - |  620 | `			"Encoding parameter exceeds the maximum allowed length of %d characters",` |
|     - |  621 | `			ICV_CSNMAXLEN);` |
|    11 |  622 | `		return 0;` |
|     - |  623 | `	}` |
|  1001 |  624 | `	IcvParseCharset(z,n,pCs);` |
|  1001 |  625 | `	return 1;` |
|   507 |  626 | `}` |
|     - |  627 |  |
|     - |  628 | `/* --- The functions ----------------------------------------------------- */` |
|     - |  629 |  |
|     - |  630 | `/* string\|false iconv(string $from_encoding, string $to_encoding, string $string) */` |
|   322 |  631 | `static int PH7_builtin_iconv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  632 | `{` |
|     - |  633 | `	const char *zFrom,*zTo,*zIn;` |
|     - |  634 | `	int nFrom,nTo,nIn,err;` |
|     - |  635 | `	icv_cs sFrom,sTo;` |
|     - |  636 | `	SyBlob sOut;` |
|   324 |  637 | `	if( nArg < 3 ){` |
|   ! 0 |  638 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  639 | `		return PH7_OK;` |
|     - |  640 | `	}` |
|   324 |  641 | `	zFrom = ph7_value_to_string(apArg[0],&nFrom);` |
|   324 |  642 | `	zTo = ph7_value_to_string(apArg[1],&nTo);` |
|   324 |  643 | `	zIn = ph7_value_to_string(apArg[2],&nIn);` |
|   324 |  644 | `	if( !IcvCharsetArg(pCtx,zFrom,nFrom,&sFrom) \|\| !IcvCharsetArg(pCtx,zTo,nTo,&sTo) ){` |
|     5 |  645 | `		ph7_result_bool(pCtx,0);` |
|     5 |  646 | `		return PH7_OK;` |
|     - |  647 | `	}` |
|   320 |  648 | `	if( sFrom.iEnc < 0 \|\| sTo.iEnc < 0 ){` |
|    56 |  649 | `		IcvShowError(pCtx,ICV_WRONG_CHARSET,` |
|    18 |  650 | `			zTo,IcvNameLen(zTo,nTo),zFrom,IcvNameLen(zFrom,nFrom));` |
|    38 |  651 | `		ph7_result_bool(pCtx,0);` |
|    38 |  652 | `		return PH7_OK;` |
|     - |  653 | `	}` |
|   283 |  654 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   283 |  655 | `	err = IcvConvert(&sOut,zIn,nIn,sFrom.iEnc,&sTo,IcvCheckIgnore(zTo,nTo));` |
|   283 |  656 | `	if( err != ICV_OK ){` |
|    63 |  657 | `		SyBlobRelease(&sOut);` |
|    63 |  658 | `		IcvShowError(pCtx,err,zTo,nTo,zFrom,nFrom);` |
|    63 |  659 | `		ph7_result_bool(pCtx,0);` |
|    63 |  660 | `		return PH7_OK;` |
|     - |  661 | `	}` |
|   221 |  662 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   221 |  663 | `	SyBlobRelease(&sOut);` |
|   221 |  664 | `	return PH7_OK;` |
|   163 |  665 | `}` |
|     - |  666 |  |
|     - |  667 |  |
|     - |  668 | `/* --- The string quartet ------------------------------------------------ */` |
|     - |  669 |  |
|     - |  670 | `/*` |
|     - |  671 | ` * php's own name for the wide intermediate every one of these four converts` |
|     - |  672 | ` * THROUGH, and the one that shows up in their "Wrong encoding" message: the` |
|     - |  673 | ` * conversion that fails is the one INTO it, so the message always reads` |
|     - |  674 | `` * `from "<the argument>" to "UCS-4LE"`.`` |
|     - |  675 | ` */` |
|     - |  676 | `#define ICV_SUPERSET "UCS-4LE"` |
|     - |  677 |  |
|     - |  678 | `/*` |
|     - |  679 | `` * The `?string $encoding = null` the string family shares. A missing or null`` |
|     - |  680 | ``  * argument is php's INTERNAL encoding, which is `iconv.internal_encoding` `` |
|     - |  681 | `` * falling back to `default_charset` -- and since §10 removes the deprecated`` |
|     - |  682 | `` * `iconv.*` directives, `default_charset` is the whole of it here.`` |
|     - |  683 | ` *` |
|     - |  684 | ` * Only php's LENGTH cap is a diagnostic at this point (answers 0 for it). An` |
|     - |  685 | ` * unknown NAME is not, because php does not learn of one until it opens a` |
|     - |  686 | `` * conversion -- which is why `iconv_strpos($h, "", 0, "NOPE")` is a silent`` |
|     - |  687 | ` * false while the same call with a needle warns. IcvEncReady() is that moment.` |
|     - |  688 | ` */` |
|   366 |  689 | `static int IcvStrEncArg(ph7_context *pCtx,ph7_value *pArg,icv_cs *pCs)` |
|     3 |  690 | `{` |
|     - |  691 | `	SyBlob sIni;` |
|     - |  692 | `	int rc;` |
|   369 |  693 | `	if( pArg != 0 && !ph7_value_is_null(pArg) ){` |
|     - |  694 | `		int nEnc;` |
|    44 |  695 | `		const char *zEnc = ph7_value_to_string(pArg,&nEnc);` |
|    44 |  696 | `		return IcvCharsetArg(pCtx,zEnc,nEnc,pCs);` |
|     - |  697 | `	}` |
|   327 |  698 | `	SyBlobInit(&sIni,&pCtx->pVm->sAllocator);` |
|   327 |  699 | `	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIni);` |
|   327 |  700 | `	rc = IcvCharsetArg(pCtx,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni),pCs);` |
|   327 |  701 | `	SyBlobRelease(&sIni);` |
|   327 |  702 | `	return rc;` |
|   186 |  703 | `}` |
|     - |  704 | `/*` |
|     - |  705 | ` * The "Wrong encoding" php raises where it would have opened the conversion.` |
|     - |  706 | ` * The conversion the string family opens is the one INTO the wide intermediate,` |
|     - |  707 | ` * so the message always names UCS-4LE as the target. Answers 0 after raising.` |
|     - |  708 | ` */` |
|   152 |  709 | `static int IcvEncReady(ph7_context *pCtx,const icv_cs *pCs)` |
|     2 |  710 | `{` |
|   154 |  711 | `	if( pCs->iEnc >= 0 ){` |
|   148 |  712 | `		return 1;` |
|     - |  713 | `	}` |
|    10 |  714 | `	IcvShowError(pCtx,ICV_WRONG_CHARSET,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,` |
|     6 |  715 | `		pCs->zName,pCs->nName);` |
|     7 |  716 | `	return 0;` |
|    78 |  717 | `}` |
|     - |  718 |  |
|     - |  719 | `/*` |
|     - |  720 | ` * A decoded string: one code point per character plus the byte offset each one` |
|     - |  721 | ` * starts at (nChar+1 entries, so the last is the buffer length). php works the` |
|     - |  722 | ` * same way -- it converts to UCS-4 and operates there -- and it is what lets a` |
|     - |  723 | ` * search answer in CHARACTERS while a slice is taken in BYTES.` |
|     - |  724 | ` */` |
|     - |  725 | `typedef struct icv_text icv_text;` |
|     - |  726 | `struct icv_text {` |
|     - |  727 | `	const char *zIn;` |
|     - |  728 | `	int nByte;` |
|     - |  729 | `	sxu32 *aCode;` |
|     - |  730 | `	int *aOfft;` |
|     - |  731 | `	int nChar;` |
|     - |  732 | `};` |
|     - |  733 | `#define ICV_TEXT_MAX 0x0FFFFFFF` |
|     - |  734 |  |
|     - |  735 | `/*` |
|     - |  736 | ` * Decode zIn under iEnc into pText. Answers PH7_OK, or PH7_OK with *pErr set to` |
|     - |  737 | ` * the diagnostic the input earns -- in which case pText is not usable but was` |
|     - |  738 | ` * still allocated, so IcvTextRelease() is safe either way.` |
|     - |  739 | ` */` |
|   200 |  740 | `static int IcvTextDecode(ph7_context *pCtx,icv_text *pText,const char *zIn,int nByte,` |
|     - |  741 | `	int iEnc,int *pErr)` |
|     2 |  742 | `{` |
|   202 |  743 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   202 |  744 | `	int i = 0,n = 0,nSlot;` |
|   202 |  745 | `	pText->zIn = zIn;` |
|   202 |  746 | `	pText->nByte = nByte;` |
|   202 |  747 | `	pText->nChar = 0;` |
|   202 |  748 | `	pText->aCode = 0;` |
|   202 |  749 | `	pText->aOfft = 0;` |
|   202 |  750 | `	*pErr = ICV_OK;` |
|   202 |  751 | `	if( nByte > ICV_TEXT_MAX ){` |
|   ! 0 |  752 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  753 | `	}` |
|   202 |  754 | `	nSlot = nByte + 1;` |
|   402 |  755 | `	pText->aCode = (sxu32 *)ph7_context_alloc_chunk(pCtx,` |
|   200 |  756 | `		(unsigned int)((sxu32)nSlot * (sizeof(sxu32) + sizeof(int))),FALSE,TRUE);` |
|   202 |  757 | `	if( pText->aCode == 0 ){` |
|   ! 0 |  758 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  759 | `	}` |
|   202 |  760 | `	pText->aOfft = (int *)&pText->aCode[nSlot];` |
|   988 |  761 | `	while( i < nByte ){` |
|   818 |  762 | `		sxu32 cp = 0;` |
|   818 |  763 | `		int nSeq = IcvDecode(&z[i],nByte - i,iEnc,&cp,pErr);` |
|   818 |  764 | `		if( nSeq == 0 ){` |
|     - |  765 | `			/* The good PREFIX is the answer here, not zero: php's own walk` |
|     - |  766 | `			 * stops at the bad character and everything before it has already` |
|     - |  767 | `			 * been converted, so nChar is how far a search got and the count` |
|     - |  768 | `			 * an out-of-bounds $offset is measured against. */` |
|    32 |  769 | `			pText->aOfft[n] = i;` |
|    32 |  770 | `			pText->nChar = n;` |
|    32 |  771 | `			return PH7_OK;` |
|     - |  772 | `		}` |
|   788 |  773 | `		pText->aCode[n] = cp;` |
|   788 |  774 | `		pText->aOfft[n] = i;` |
|   788 |  775 | `		i += nSeq;` |
|   788 |  776 | `		n++;` |
|     2 |  777 | `	}` |
|   172 |  778 | `	pText->aOfft[n] = nByte;` |
|   172 |  779 | `	pText->nChar = n;` |
|   172 |  780 | `	return PH7_OK;` |
|   102 |  781 | `}` |
|   200 |  782 | `static void IcvTextRelease(ph7_context *pCtx,icv_text *pText)` |
|     2 |  783 | `{` |
|   202 |  784 | `	if( pText->aCode ){` |
|   202 |  785 | `		ph7_context_free_chunk(pCtx,pText->aCode);` |
|   202 |  786 | `		pText->aCode = 0;` |
|   100 |  787 | `	}` |
|   202 |  788 | `}` |
|     - |  789 |  |
|     - |  790 | `/* int\|false iconv_strlen(string $string, ?string $encoding = null) */` |
|    38 |  791 | `static int PH7_builtin_iconv_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  792 | `{` |
|     - |  793 | `	const char *zIn;` |
|    40 |  794 | `	int nIn,err = ICV_OK;` |
|     - |  795 | `	icv_cs sCs;` |
|     - |  796 | `	icv_text sText;` |
|    38 |  797 | `	if( nArg < 1 \|\| !IcvStrEncArg(pCtx,nArg > 1 ? apArg[1] : 0,&sCs)` |
|    39 |  798 | `	 \|\| !IcvEncReady(pCtx,&sCs) ){` |
|     5 |  799 | `		ph7_result_bool(pCtx,0);` |
|     5 |  800 | `		return PH7_OK;` |
|     - |  801 | `	}` |
|    36 |  802 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    36 |  803 | `	if( IcvTextDecode(pCtx,&sText,zIn,nIn,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 |  804 | `		return PH7_OK;` |
|     - |  805 | `	}` |
|    36 |  806 | `	IcvTextRelease(pCtx,&sText);` |
|    36 |  807 | `	if( err != ICV_OK ){` |
|    12 |  808 | `		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|    12 |  809 | `		ph7_result_bool(pCtx,0);` |
|    12 |  810 | `		return PH7_OK;` |
|     - |  811 | `	}` |
|    26 |  812 | `	ph7_result_int(pCtx,sText.nChar);` |
|    26 |  813 | `	return PH7_OK;` |
|    21 |  814 | `}` |
|     - |  815 |  |
|     - |  816 | `/*` |
|     - |  817 | ` * string\|false iconv_substr(string $string, int $offset, ?int $length = null,` |
|     - |  818 | ` *                           ?string $encoding = null)` |
|     - |  819 | ` *` |
|     - |  820 | ` * php clamps in the order its own code does, and the order is visible: a null` |
|     - |  821 | ` * $length starts life as the string's BYTE count and is then clamped to the` |
|     - |  822 | ` * character count, so it can never reach past the end however the two differ.` |
|     - |  823 | ` */` |
|    52 |  824 | `static int PH7_builtin_iconv_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  825 | `{` |
|     - |  826 | `	const char *zIn;` |
|    54 |  827 | `	int nIn,err = ICV_OK,iStart,iStop;` |
|     - |  828 | `	sxi64 iOfft,iLen;` |
|     - |  829 | `	icv_cs sCs;` |
|     - |  830 | `	icv_text sText;` |
|    52 |  831 | `	if( nArg < 2 \|\| !IcvStrEncArg(pCtx,nArg > 3 ? apArg[3] : 0,&sCs)` |
|    54 |  832 | `	 \|\| !IcvEncReady(pCtx,&sCs) ){` |
|     3 |  833 | `		ph7_result_bool(pCtx,0);` |
|     3 |  834 | `		return PH7_OK;` |
|     - |  835 | `	}` |
|    52 |  836 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    52 |  837 | `	if( PH7_IntArgResolve(pCtx,apArg[1],"iconv_substr",2,"$offset","int",&iOfft) != PH7_OK ){` |
|   ! 0 |  838 | `		return PH7_OK;` |
|     - |  839 | `	}` |
|    52 |  840 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|    32 |  841 | `		if( PH7_IntArgResolve(pCtx,apArg[2],"iconv_substr",3,"$length","?int",&iLen) != PH7_OK ){` |
|   ! 0 |  842 | `			return PH7_OK;` |
|     - |  843 | `		}` |
|    17 |  844 | `	}else{` |
|    22 |  845 | `		iLen = nIn;` |
|     - |  846 | `	}` |
|    52 |  847 | `	if( IcvTextDecode(pCtx,&sText,zIn,nIn,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 |  848 | `		return PH7_OK;` |
|     - |  849 | `	}` |
|    52 |  850 | `	if( err != ICV_OK ){` |
|     5 |  851 | `		IcvTextRelease(pCtx,&sText);` |
|     5 |  852 | `		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|     5 |  853 | `		ph7_result_bool(pCtx,0);` |
|     5 |  854 | `		return PH7_OK;` |
|     - |  855 | `	}` |
|    48 |  856 | `	if( iOfft < 0 ){` |
|    11 |  857 | `		iOfft += sText.nChar;` |
|    11 |  858 | `		if( iOfft < 0 ){` |
|     7 |  859 | `			iOfft = 0;` |
|     4 |  860 | `		}` |
|    43 |  861 | `	}else if( iOfft > sText.nChar ){` |
|     5 |  862 | `		iOfft = sText.nChar;` |
|     2 |  863 | `	}` |
|    48 |  864 | `	if( iLen < 0 ){` |
|     9 |  865 | `		iLen += sText.nChar - iOfft;` |
|     9 |  866 | `		if( iLen < 0 ){` |
|     5 |  867 | `			iLen = 0;` |
|     3 |  868 | `		}` |
|    44 |  869 | `	}else if( iLen > sText.nChar ){` |
|    23 |  870 | `		iLen = sText.nChar;` |
|    11 |  871 | `	}` |
|    48 |  872 | `	if( iOfft + iLen > sText.nChar ){` |
|    16 |  873 | `		iLen = sText.nChar - iOfft;` |
|     7 |  874 | `	}` |
|    48 |  875 | `	iStart = sText.aOfft[iOfft];` |
|    48 |  876 | `	iStop = sText.aOfft[iOfft + iLen];` |
|    48 |  877 | `	ph7_result_string(pCtx,&zIn[iStart],iStop - iStart);` |
|    48 |  878 | `	IcvTextRelease(pCtx,&sText);` |
|    48 |  879 | `	return PH7_OK;` |
|    28 |  880 | `}` |
|     - |  881 |  |
|     - |  882 | `/*` |
|     - |  883 | ` * The search both position builtins run: the first match at or after iFrom, or` |
|     - |  884 | ` * the LAST one when bReverse is set. Answers the character index or -1.` |
|     - |  885 | ` */` |
|    54 |  886 | `static int IcvSearch(const icv_text *pH,const icv_text *pN,int iFrom,int bReverse)` |
|     2 |  887 | `{` |
|    56 |  888 | `	int i,iFound = -1;` |
|    56 |  889 | `	if( pN->nChar < 1 \|\| pN->nChar > pH->nChar ){` |
|    11 |  890 | `		return -1;` |
|     - |  891 | `	}` |
|   128 |  892 | `	for( i = iFrom ; i + pN->nChar <= pH->nChar ; ++i ){` |
|     - |  893 | `		int k;` |
|   180 |  894 | `		for( k = 0 ; k < pN->nChar && pH->aCode[i+k] == pN->aCode[k] ; ++k ){}` |
|   106 |  895 | `		if( k == pN->nChar ){` |
|    48 |  896 | `			if( !bReverse ){` |
|    24 |  897 | `				return i;` |
|     - |  898 | `			}` |
|    25 |  899 | `			iFound = i;` |
|    12 |  900 | `		}` |
|    43 |  901 | `	}` |
|    23 |  902 | `	return iFound;` |
|    29 |  903 | `}` |
|     - |  904 |  |
|     - |  905 | `/*` |
|     - |  906 | ` * int\|false iconv_strpos(string $haystack, string $needle, int $offset = 0,` |
|     - |  907 | ` *                        ?string $encoding = null)` |
|     - |  908 | ` * int\|false iconv_strrpos(string $haystack, string $needle,` |
|     - |  909 | ` *                         ?string $encoding = null)` |
|     - |  910 | ` *` |
|     - |  911 | ` * One body, because php's two differ only in four places: strrpos has no` |
|     - |  912 | ` * $offset at all, it tests the empty needle BEFORE the encoding is looked at` |
|     - |  913 | ` * (so an over-long name is silent there and warns in strpos), it keeps looking` |
|     - |  914 | ` * after a match instead of stopping at the first, and it has no out-of-bounds` |
|     - |  915 | ` * ValueError to raise.` |
|     - |  916 | ` *` |
|     - |  917 | ` * The ORDER below is php's and it is visible from the outside, because the two` |
|     - |  918 | ` * halves of the search report differently. The NEEDLE goes through a whole` |
|     - |  919 | ` * conversion, so an ill-formed one raises the same two diagnostics anything` |
|     - |  920 | ` * else does. The HAYSTACK is walked one character at a time into a buffer` |
|     - |  921 | ` * exactly one wide, and php's loop leaves that walk the moment a character` |
|     - |  922 | ` * fails to convert -- WITHOUT recording why. So an ill-formed haystack is` |
|     - |  923 | ` * silent: the search simply cannot see past the bad byte, and` |
|     - |  924 | `` * `iconv_strpos("ab\xFFcd","cd")` is FALSE with nothing said. What it does`` |
|     - |  925 | ` * decide is the count the out-of-bounds ValueError is measured against, which` |
|     - |  926 | ` * is why an $offset past the first bad byte raises where the same call on a` |
|     - |  927 | ` * clean string answers false.` |
|     - |  928 | ` */` |
|    74 |  929 | `static int IcvStrposBody(ph7_context *pCtx,int nArg,ph7_value **apArg,int bReverse)` |
|     2 |  930 | `{` |
|     - |  931 | `	const char *zH,*zN;` |
|    76 |  932 | `	int nH,nN,err = ICV_OK,iScanned,iFound;` |
|    76 |  933 | `	sxi64 iOfft = 0;` |
|     - |  934 | `	icv_cs sCs;` |
|     - |  935 | `	icv_text sH,sN;` |
|    76 |  936 | `	if( nArg < 2 ){` |
|   ! 0 |  937 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  938 | `		return PH7_OK;` |
|     - |  939 | `	}` |
|    76 |  940 | `	zH = ph7_value_to_string(apArg[0],&nH);` |
|    76 |  941 | `	zN = ph7_value_to_string(apArg[1],&nN);` |
|    76 |  942 | `	if( bReverse && nN < 1 ){` |
|     - |  943 | `		/* php's order: the empty needle answers false before the name is` |
|     - |  944 | `		 * measured, which is why an over-long $encoding is silent here. */` |
|     5 |  945 | `		ph7_result_bool(pCtx,0);` |
|     5 |  946 | `		return PH7_OK;` |
|     - |  947 | `	}` |
|    72 |  948 | `	if( !IcvStrEncArg(pCtx,nArg > (bReverse ? 2 : 3) ? apArg[bReverse ? 2 : 3] : 0,&sCs) ){` |
|     3 |  949 | `		ph7_result_bool(pCtx,0);` |
|     3 |  950 | `		return PH7_OK;` |
|     - |  951 | `	}` |
|    68 |  952 | `	if( !bReverse && nArg > 2` |
|    38 |  953 | `	 && PH7_IntArgResolve(pCtx,apArg[2],"iconv_strpos",3,"$offset","int",&iOfft) != PH7_OK ){` |
|     - |  954 | ``		/* $offset is php's plain `int`, so a null is the deprecation §10 turns`` |
|     - |  955 | `		 * into a TypeError -- and it has to be REFUSED here rather than skipped,` |
|     - |  956 | `		 * which is what treating a null argument as "not passed" would do. */` |
|   ! 0 |  957 | `		return PH7_OK;` |
|     - |  958 | `	}` |
|    70 |  959 | `	if( iOfft < 0 ){` |
|     - |  960 | `		/* A negative offset counts from the end, so THIS is the one path that` |
|     - |  961 | `		 * measures the haystack before the needle -- and the one place an` |
|     - |  962 | `		 * ill-formed haystack is heard about at all. */` |
|     5 |  963 | `		if( !IcvEncReady(pCtx,&sCs) ){` |
|   ! 0 |  964 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  965 | `			return PH7_OK;` |
|     - |  966 | `		}` |
|     5 |  967 | `		if( IcvTextDecode(pCtx,&sH,zH,nH,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 |  968 | `			return PH7_OK;` |
|     - |  969 | `		}` |
|     5 |  970 | `		IcvTextRelease(pCtx,&sH);` |
|     5 |  971 | `		if( err != ICV_OK ){` |
|   ! 0 |  972 | `			IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|   ! 0 |  973 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  974 | `			return PH7_OK;` |
|     - |  975 | `		}` |
|     5 |  976 | `		iOfft += sH.nChar;` |
|     5 |  977 | `		if( iOfft < 0 ){` |
|     3 |  978 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  979 | `				"iconv_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|     3 |  980 | `			return PH7_OK;` |
|     - |  981 | `		}` |
|     1 |  982 | `	}` |
|    68 |  983 | `	if( nN < 1 ){` |
|     7 |  984 | `		ph7_result_bool(pCtx,0);` |
|     7 |  985 | `		return PH7_OK;` |
|     - |  986 | `	}` |
|     - |  987 | `	/* php converts the NEEDLE whole before it scans, so an ill-formed needle is` |
|     - |  988 | `	 * the same two diagnostics as an ill-formed argument anywhere else. */` |
|    62 |  989 | `	if( !IcvEncReady(pCtx,&sCs) ){` |
|     3 |  990 | `		ph7_result_bool(pCtx,0);` |
|     3 |  991 | `		return PH7_OK;` |
|     - |  992 | `	}` |
|    60 |  993 | `	if( IcvTextDecode(pCtx,&sN,zN,nN,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 |  994 | `		return PH7_OK;` |
|     - |  995 | `	}` |
|    60 |  996 | `	if( err != ICV_OK ){` |
|     5 |  997 | `		IcvTextRelease(pCtx,&sN);` |
|     5 |  998 | `		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|     5 |  999 | `		ph7_result_bool(pCtx,0);` |
|     5 | 1000 | `		return PH7_OK;` |
|     - | 1001 | `	}` |
|    56 | 1002 | `	if( IcvTextDecode(pCtx,&sH,zH,nH,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 | 1003 | `		IcvTextRelease(pCtx,&sN);` |
|   ! 0 | 1004 | `		return PH7_OK;` |
|     - | 1005 | `	}` |
|    56 | 1006 | `	iScanned = sH.nChar;` |
|    56 | 1007 | `	iFound = IcvSearch(&sH,&sN,bReverse ? 0 : (int)iOfft,bReverse);` |
|    56 | 1008 | `	IcvTextRelease(pCtx,&sN);` |
|    56 | 1009 | `	IcvTextRelease(pCtx,&sH);` |
|    56 | 1010 | `	if( err != ICV_OK ){` |
|     - | 1011 | `		/* Whether the walk SAYS anything about the bad character depends on how` |
|     - | 1012 | `		 * far it got, because php hears about it from the call that converted` |
|     - | 1013 | `		 * the character BEFORE it. An error at index 0 is therefore silent, and` |
|     - | 1014 | `		 * so is one the walk never reached -- a full match ends the walk, so` |
|     - | 1015 | ``		 * `iconv_strpos(str_repeat("x",100)."\xFF","xx")` answers 0 with`` |
|     - | 1016 | ``		 * nothing said while `iconv_strpos("ab\xFF","ab")` answers FALSE with`` |
|     - | 1017 | `		 * the notice. */` |
|    13 | 1018 | `		int iStop = (!bReverse && iFound >= 0) ? iFound + sN.nChar - 1 : iScanned;` |
|    13 | 1019 | `		if( iScanned >= 1 && iStop >= iScanned - 1 ){` |
|     5 | 1020 | `			IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|     5 | 1021 | `			ph7_result_bool(pCtx,0);` |
|     5 | 1022 | `			return PH7_OK;` |
|     - | 1023 | `		}` |
|     4 | 1024 | `	}` |
|    52 | 1025 | `	if( !bReverse && iOfft > iScanned ){` |
|     5 | 1026 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1027 | `			"iconv_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|     5 | 1028 | `		return PH7_OK;` |
|     - | 1029 | `	}` |
|    48 | 1030 | `	if( iFound < 0 ){` |
|    15 | 1031 | `		ph7_result_bool(pCtx,0);` |
|     8 | 1032 | `	}else{` |
|    34 | 1033 | `		ph7_result_int(pCtx,iFound);` |
|     - | 1034 | `	}` |
|    48 | 1035 | `	return PH7_OK;` |
|    39 | 1036 | `}` |
|    50 | 1037 | `static int PH7_builtin_iconv_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1038 | `{` |
|    52 | 1039 | `	return IcvStrposBody(pCtx,nArg,apArg,0);` |
|     2 | 1040 | `}` |
|    24 | 1041 | `static int PH7_builtin_iconv_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1042 | `{` |
|    25 | 1043 | `	return IcvStrposBody(pCtx,nArg,apArg,1);` |
|     1 | 1044 | `}` |
|     - | 1045 |  |
|     - | 1046 | `/* --- The encoding settings --------------------------------------------- */` |
|     - | 1047 |  |
|     - | 1048 | `/*` |
|     - | 1049 | ` * array\|string\|false iconv_get_encoding(string $type = "all")` |
|     - | 1050 | ` *` |
|     - | 1051 | `` * php answers `iconv.input_encoding` / `output_encoding` / `internal_encoding`,`` |
|     - | 1052 | `` * each falling back to `default_charset`. §10 removes all three of those`` |
|     - | 1053 | ` * directives -- every one of them is deprecated, which is also why this` |
|     - | 1054 | ` * function's SETTER counterpart is not here at all (see the twin pair in` |
|     - | 1055 | `` * 002-integration) -- so `default_charset` is what all three answer, and`` |
|     - | 1056 | ` * moving it moves them together, exactly as php does when the iconv directives` |
|     - | 1057 | ` * are left unset. $type matches case-insensitively; anything else is FALSE,` |
|     - | 1058 | ` * with no diagnostic of any kind.` |
|     - | 1059 | ` */` |
|    36 | 1060 | `static int PH7_builtin_iconv_get_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1061 | `{` |
|     - | 1062 | `	static const char * const azType[] = { "input_encoding", "output_encoding", "internal_encoding" };` |
|     - | 1063 | `	SyBlob sIni;` |
|    38 | 1064 | `	const char *zType = "all";` |
|    38 | 1065 | `	int nType = 3,k;` |
|    38 | 1066 | `	if( nArg > 0 ){` |
|    34 | 1067 | `		zType = ph7_value_to_string(apArg[0],&nType);` |
|    16 | 1068 | `	}` |
|    38 | 1069 | `	SyBlobInit(&sIni,&pCtx->pVm->sAllocator);` |
|    38 | 1070 | `	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIni);` |
|    38 | 1071 | `	if( nType == 3 && SyStrnicmp(zType,"all",3) == 0 ){` |
|    11 | 1072 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    11 | 1073 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    11 | 1074 | `		if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 1075 | `			SyBlobRelease(&sIni);` |
|   ! 0 | 1076 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 1077 | `		}` |
|    11 | 1078 | `		ph7_value_string(pVal,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni));` |
|    41 | 1079 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(azType) ; ++k ){` |
|    31 | 1080 | `			ph7_array_add_strkey_elem(pArray,azType[k],pVal);` |
|    16 | 1081 | `		}` |
|    11 | 1082 | `		ph7_result_value(pCtx,pArray);` |
|    11 | 1083 | `		ph7_context_release_value(pCtx,pVal);` |
|    11 | 1084 | `		SyBlobRelease(&sIni);` |
|    11 | 1085 | `		return PH7_OK;` |
|     - | 1086 | `	}` |
|    80 | 1087 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azType) ; ++k ){` |
|    70 | 1088 | `		if( nType == (int)SyStrlen(azType[k]) && SyStrnicmp(zType,azType[k],(sxu32)nType) == 0 ){` |
|    18 | 1089 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni));` |
|    18 | 1090 | `			SyBlobRelease(&sIni);` |
|    18 | 1091 | `			return PH7_OK;` |
|     - | 1092 | `		}` |
|    28 | 1093 | `	}` |
|    11 | 1094 | `	SyBlobRelease(&sIni);` |
|    11 | 1095 | `	ph7_result_bool(pCtx,0);` |
|    11 | 1096 | `	return PH7_OK;` |
|    20 | 1097 | `}` |
|     - | 1098 |  |
|     - | 1099 | `/* --- MIME header words -------------------------------------------------- */` |
|     - | 1100 |  |
|     - | 1101 | `/*` |
|     - | 1102 | ` * Convert zIn into pOut, appending NOTHING unless the whole run converts. This` |
|     - | 1103 | ` * is php's _php_iconv_appendl(), which builds its own buffer and only hands it` |
|     - | 1104 | ` * over on success -- so a header run that fails part-way leaves no partial` |
|     - | 1105 | ` * bytes behind, which is visible whenever CONTINUE_ON_ERROR lets the scan go on` |
|     - | 1106 | ` * afterwards.` |
|     - | 1107 | ` */` |
|   988 | 1108 | `static int IcvAppendConv(ph7_context *pCtx,SyBlob *pOut,const char *zIn,int nIn,` |
|     - | 1109 | `	int iFrom,const icv_cs *pTo)` |
|     2 | 1110 | `{` |
|     - | 1111 | `	SyBlob sTmp;` |
|     - | 1112 | `	int rc;` |
|   990 | 1113 | `	SyBlobInit(&sTmp,&pCtx->pVm->sAllocator);` |
|   990 | 1114 | `	rc = IcvConvert(&sTmp,zIn,nIn,iFrom,pTo,0);` |
|   990 | 1115 | `	if( rc == ICV_OK ){` |
|   986 | 1116 | `		SyBlobAppend(pOut,SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|   492 | 1117 | `	}` |
|   990 | 1118 | `	SyBlobRelease(&sTmp);` |
|   990 | 1119 | `	return rc;` |
|     2 | 1120 | `}` |
|     - | 1121 |  |
|     - | 1122 |  |
|     - | 1123 | `/*` |
|     - | 1124 | ` * base64, in the two shapes RFC 2047 needs. The DECODER is php's` |
|     - | 1125 | ` * php_base64_decode() in its non-strict mode: every byte outside the alphabet` |
|     - | 1126 | ` * is skipped, and a partial final group contributes what its bits allow, so` |
|     - | 1127 | ` * "!!!" decodes to the empty string rather than failing.` |
|     - | 1128 | ` */` |
|     - | 1129 | `static const signed char aIcvB64[128] = {` |
|     - | 1130 | `	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,` |
|     - | 1131 | `	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,` |
|     - | 1132 | `	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,` |
|     - | 1133 | `	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,` |
|     - | 1134 | `	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,` |
|     - | 1135 | `	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,` |
|     - | 1136 | `	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,` |
|     - | 1137 | `	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1` |
|     - | 1138 | `};` |
|    50 | 1139 | `static void IcvBase64Encode(SyBlob *pOut,const unsigned char *z,int n)` |
|     1 | 1140 | `{` |
|     - | 1141 | `	static const char zAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";` |
|     - | 1142 | `	char zGrp[4];` |
|    51 | 1143 | `	int i = 0;` |
|   193 | 1144 | `	while( i + 2 < n ){` |
|   143 | 1145 | `		sxu32 v = ((sxu32)z[i] << 16) \| ((sxu32)z[i+1] << 8) \| z[i+2];` |
|   143 | 1146 | `		zGrp[0] = zAlpha[(v >> 18) & 0x3F]; zGrp[1] = zAlpha[(v >> 12) & 0x3F];` |
|   143 | 1147 | `		zGrp[2] = zAlpha[(v >> 6) & 0x3F];  zGrp[3] = zAlpha[v & 0x3F];` |
|   143 | 1148 | `		SyBlobAppend(pOut,zGrp,4);` |
|   143 | 1149 | `		i += 3;` |
|     1 | 1150 | `	}` |
|    51 | 1151 | `	if( i < n ){` |
|    45 | 1152 | `		sxu32 v = (sxu32)z[i] << 16;` |
|    45 | 1153 | `		int nRem = n - i;` |
|    45 | 1154 | `		if( nRem > 1 ){` |
|    35 | 1155 | `			v \|= (sxu32)z[i+1] << 8;` |
|    17 | 1156 | `		}` |
|    45 | 1157 | `		zGrp[0] = zAlpha[(v >> 18) & 0x3F];` |
|    45 | 1158 | `		zGrp[1] = zAlpha[(v >> 12) & 0x3F];` |
|    45 | 1159 | `		zGrp[2] = nRem > 1 ? zAlpha[(v >> 6) & 0x3F] : '=';` |
|    45 | 1160 | `		zGrp[3] = '=';` |
|    45 | 1161 | `		SyBlobAppend(pOut,zGrp,4);` |
|    22 | 1162 | `	}` |
|    51 | 1163 | `}` |
|     - | 1164 | `/* How many base64 characters n input bytes become. */` |
|   100 | 1165 | `static int IcvBase64Len(int n)` |
|     1 | 1166 | `{` |
|   101 | 1167 | `	return ((n + 2) / 3) * 4;` |
|     1 | 1168 | `}` |
|    48 | 1169 | `static void IcvBase64Decode(SyBlob *pOut,const char *z,int n)` |
|     1 | 1170 | `{` |
|    49 | 1171 | `	sxu32 v = 0;` |
|    49 | 1172 | `	int i,nBit = 0;` |
|   357 | 1173 | `	for( i = 0 ; i < n ; ++i ){` |
|   309 | 1174 | `		int c = (unsigned char)z[i];` |
|   309 | 1175 | `		int d = (c < 128) ? aIcvB64[c] : -1;` |
|   309 | 1176 | `		if( d < 0 ){` |
|     - | 1177 | `			/* php's non-strict decoder skips EVERYTHING outside the alphabet,` |
|     - | 1178 | ``			 * and that includes the padding: `=` is counted and stepped over,`` |
|     - | 1179 | `			 * not treated as the end, so ":!!!=ZZ" still decodes its "ZZ". */` |
|    57 | 1180 | `			continue;` |
|     - | 1181 | `		}` |
|   253 | 1182 | `		v = (v << 6) \| (sxu32)d;` |
|   253 | 1183 | `		nBit += 6;` |
|   253 | 1184 | `		if( nBit >= 8 ){` |
|   179 | 1185 | `			unsigned char b = (unsigned char)((v >> (nBit - 8)) & 0xFF);` |
|   179 | 1186 | `			SyBlobAppend(pOut,&b,1);` |
|   179 | 1187 | `			nBit -= 8;` |
|    89 | 1188 | `		}` |
|   127 | 1189 | `	}` |
|    49 | 1190 | `}` |
|     - | 1191 |  |
|     - | 1192 | `/*` |
|     - | 1193 | `` * quoted-printable, php's php_quot_print_decode() with `replace_us_by_ws` on --`` |
|     - | 1194 | `` * which is the `Q` of RFC 2047, where `_` stands for a space. Answers 0 for the`` |
|     - | 1195 | `` * strings php refuses (a `=` followed by one hex digit and a non-hex one, and a`` |
|     - | 1196 | ` * soft break that runs off the end); those are what make its caller fall back` |
|     - | 1197 | ` * to the raw encoded word. Stops at a NUL, as php's does.` |
|     - | 1198 | ` */` |
|    20 | 1199 | `static int IcvQPrintDecode(SyBlob *pOut,const char *z,int n,int bUnderscore)` |
|     2 | 1200 | `{` |
|    22 | 1201 | `	int i = 0;` |
|    56 | 1202 | `	while( i < n && z[i] != 0 ){` |
|    44 | 1203 | `		int c = (unsigned char)z[i];` |
|    44 | 1204 | `		if( c != '=' ){` |
|    23 | 1205 | `			unsigned char b = (unsigned char)((bUnderscore && c == '_') ? ' ' : c);` |
|    23 | 1206 | `			SyBlobAppend(pOut,&b,1);` |
|    23 | 1207 | `			i++;` |
|    23 | 1208 | `			continue;` |
|     - | 1209 | `		}` |
|    22 | 1210 | `		i++;` |
|    22 | 1211 | `		if( i >= n \|\| z[i] == 0 ){` |
|   ! 0 | 1212 | `			break;` |
|     - | 1213 | `		}` |
|    22 | 1214 | `		if( SyisHex(z[i]) ){` |
|    13 | 1215 | `			int hi = SyHexToint(z[i]);` |
|    13 | 1216 | `			if( i + 1 >= n \|\| !SyisHex(z[i+1]) ){` |
|   ! 0 | 1217 | `				return 0;` |
|     - | 1218 | `			}` |
|     - | 1219 | `			{` |
|    13 | 1220 | `				unsigned char b = (unsigned char)((hi << 4) \| SyHexToint(z[i+1]));` |
|    13 | 1221 | `				SyBlobAppend(pOut,&b,1);` |
|     - | 1222 | `			}` |
|    13 | 1223 | `			i += 2;` |
|    13 | 1224 | `			continue;` |
|     - | 1225 | `		}` |
|     - | 1226 | `		/* A soft line break: any run of spaces and tabs, then the newline. */` |
|     9 | 1227 | `		while( z[i] == ' ' \|\| z[i] == '\t' ){` |
|   ! 0 | 1228 | `			i++;` |
|   ! 0 | 1229 | `			if( i >= n \|\| z[i] == 0 ){` |
|   ! 0 | 1230 | `				return 0;` |
|     - | 1231 | `			}` |
|   ! 0 | 1232 | `		}` |
|     9 | 1233 | `		if( z[i] != '\r' && z[i] != '\n' ){` |
|     9 | 1234 | `			return 0;` |
|     - | 1235 | `		}` |
|   ! 0 | 1236 | `		if( z[i] == '\r' && i + 1 < n && z[i+1] == '\n' ){` |
|   ! 0 | 1237 | `			i++;` |
|   ! 0 | 1238 | `		}` |
|   ! 0 | 1239 | `		i++;` |
|   ! 0 | 1240 | `	}` |
|    13 | 1241 | `	return 1;` |
|    12 | 1242 | `}` |
|     - | 1243 |  |
|     - | 1244 | ``/* php's qp_table: 1 = the byte may be written as itself, 3 = it needs `=XX`. */`` |
|  4064 | 1245 | `static int IcvQPrintCost(int c)` |
|     1 | 1246 | `{` |
|  4065 | 1247 | `	if( c < 0x21 \|\| c > 0x7E \|\| c == '=' \|\| c == '?' \|\| c == '_' ){` |
|  1231 | 1248 | `		return 3;` |
|     - | 1249 | `	}` |
|  2835 | 1250 | `	return 1;` |
|  2033 | 1251 | `}` |
|     - | 1252 |  |
|     - | 1253 | `#define ICV_SCHEME_B64 0` |
|     - | 1254 | `#define ICV_SCHEME_QP  1` |
|     - | 1255 |  |
|     - | 1256 | `/*` |
|     - | 1257 | ` * php's _php_iconv_mime_encode(). The line budget is a single running counter` |
|     - | 1258 | ` * that every piece of the header subtracts from, and the two schemes spend it` |
|     - | 1259 | ` * differently: base64 works out how many INPUT bytes fit a line by arithmetic` |
|     - | 1260 | `` * (`(char_cnt - 2) / 4 * 3`, minus a four-byte reserve), while quoted-printable`` |
|     - | 1261 | ` * converts a candidate run, PRICES it through the table above, and shrinks the` |
|     - | 1262 | ` * run until the price fits -- which is why a value made of three-byte` |
|     - | 1263 | ` * characters wraps where it does. The counter is charged the field name's RAW` |
|     - | 1264 | ` * byte length even when the name is dropped for not being ASCII.` |
|     - | 1265 | ` */` |
|    64 | 1266 | `static int IcvMimeEncode(ph7_context *pCtx,SyBlob *pOut,` |
|     - | 1267 | `	const char *zName,int nName,const char *zVal,int nVal,` |
|     - | 1268 | `	sxi64 iMaxLine,const char *zLf,int nLf,int iScheme,` |
|     - | 1269 | `	const icv_cs *pTo,const char *zToName,int nToName,int iFrom)` |
|     1 | 1270 | `{` |
|     - | 1271 | `	SyBlob sChunk;` |
|     - | 1272 | `	sxi64 iBudget;` |
|    65 | 1273 | `	int i = 0,rc = ICV_OK;` |
|    65 | 1274 | `	if( (sxi64)nName + 2 >= iMaxLine \|\| (sxi64)nToName + 12 >= iMaxLine ){` |
|   ! 0 | 1275 | `		return ICV_TOO_BIG;` |
|     - | 1276 | `	}` |
|    65 | 1277 | `	iBudget = iMaxLine;` |
|    65 | 1278 | `	SyBlobInit(&sChunk,&pCtx->pVm->sAllocator);` |
|     - | 1279 | `	/* The field NAME goes out in ASCII, and php ignores whether that worked --` |
|     - | 1280 | `	 * so a name carrying a byte ASCII cannot hold contributes NOTHING (the` |
|     - | 1281 | `	 * append is all-or-nothing) while still costing its raw length. */` |
|     - | 1282 | `	{` |
|     - | 1283 | `		icv_cs sAscii;` |
|    65 | 1284 | `		sAscii.iEnc = ICV_ASCII;` |
|    65 | 1285 | `		sAscii.bTranslit = 0;` |
|    65 | 1286 | `		sAscii.bIgnore = 0;` |
|    65 | 1287 | `		sAscii.nName = 0;` |
|    65 | 1288 | `		(void)IcvAppendConv(pCtx,pOut,zName,nName,iFrom,&sAscii);` |
|     - | 1289 | `	}` |
|    65 | 1290 | `	iBudget -= nName;` |
|    65 | 1291 | `	SyBlobAppend(pOut,": ",2);` |
|    65 | 1292 | `	iBudget -= 2;` |
|    32 | 1293 | `	do{` |
|     - | 1294 | ``		/* `=?`, the charset, `?`, the scheme letter, `?` and the closing `?=`;`` |
|     - | 1295 | `		 * base64 needs one more character than quoted-printable can get away` |
|     - | 1296 | `		 * with, which is what makes the two minimums differ. */` |
|   103 | 1297 | `		sxi64 iMinWord = 7 + nToName + (iScheme == ICV_SCHEME_B64 ? 4 : 3);` |
|   103 | 1298 | `		if( iBudget < iMinWord + nLf + 1 ){` |
|    41 | 1299 | `			SyBlobAppend(pOut,zLf,(sxu32)nLf);` |
|    41 | 1300 | `			SyBlobAppend(pOut," ",1);` |
|    41 | 1301 | `			iBudget = iMaxLine - 1;` |
|    20 | 1302 | `		}` |
|   103 | 1303 | `		SyBlobAppend(pOut,"=?",2);` |
|   103 | 1304 | `		SyBlobAppend(pOut,zToName,(sxu32)nToName);` |
|   103 | 1305 | `		SyBlobAppend(pOut,"?",1);` |
|   103 | 1306 | `		SyBlobAppend(pOut,iScheme == ICV_SCHEME_B64 ? "B" : "Q",1);` |
|   103 | 1307 | `		SyBlobAppend(pOut,"?",1);` |
|   103 | 1308 | `		iBudget -= 2 + nToName + 3;` |
|   103 | 1309 | `		if( iScheme == ICV_SCHEME_B64 ){` |
|    55 | 1310 | `			sxi64 iRoom = (iBudget - 2) / 4 * 3 - 4;` |
|     - | 1311 | `			int nTook;` |
|    55 | 1312 | `			if( iRoom <= 0 ){` |
|   ! 0 | 1313 | `				rc = ICV_TOO_BIG;` |
|   ! 0 | 1314 | `				break;` |
|     - | 1315 | `			}` |
|    55 | 1316 | `			SyBlobReset(&sChunk);` |
|    55 | 1317 | `			rc = IcvConvertBounded(&sChunk,zVal,nVal,&i,iFrom,pTo,iRoom,&nTook);` |
|    55 | 1318 | `			if( rc != ICV_OK ){` |
|     5 | 1319 | `				break;` |
|     - | 1320 | `			}` |
|    51 | 1321 | `			if( nTook == 0 && i < nVal ){` |
|     - | 1322 | `				/* The line has room for a word but not for one CHARACTER of the` |
|     - | 1323 | `				 * value: php's iconv comes back E2BIG having consumed nothing,` |
|     - | 1324 | `				 * and refuses rather than emitting an empty word forever. */` |
|   ! 0 | 1325 | `				rc = ICV_TOO_BIG;` |
|   ! 0 | 1326 | `				break;` |
|     - | 1327 | `			}` |
|    51 | 1328 | `			if( IcvBase64Len((int)SyBlobLength(&sChunk)) > iBudget ){` |
|   ! 0 | 1329 | `				rc = ICV_UNKNOWN_ERR;` |
|   ! 0 | 1330 | `				break;` |
|     - | 1331 | `			}` |
|    76 | 1332 | `			IcvBase64Encode(pOut,(const unsigned char *)SyBlobData(&sChunk),` |
|    50 | 1333 | `				(int)SyBlobLength(&sChunk));` |
|    51 | 1334 | `			iBudget -= IcvBase64Len((int)SyBlobLength(&sChunk));` |
|    26 | 1335 | `		}else{` |
|    49 | 1336 | `			sxi64 iRoom = iBudget - 2;` |
|    49 | 1337 | `			int nTook = 0,k,iCost = 0;` |
|   144 | 1338 | `			for(;;){` |
|   169 | 1339 | `				int iStart = i;` |
|   169 | 1340 | `				if( iRoom <= 0 ){` |
|   ! 0 | 1341 | `					rc = ICV_UNKNOWN_ERR;` |
|   ! 0 | 1342 | `					break;` |
|     - | 1343 | `				}` |
|   169 | 1344 | `				SyBlobReset(&sChunk);` |
|   169 | 1345 | `				rc = IcvConvertBounded(&sChunk,zVal,nVal,&i,iFrom,pTo,iRoom,&nTook);` |
|   169 | 1346 | `				if( rc != ICV_OK ){` |
|   ! 0 | 1347 | `					break;` |
|     - | 1348 | `				}` |
|   169 | 1349 | `				if( nTook == 0 && i < nVal ){` |
|     - | 1350 | `					/* Not even one character fits: php's own E2BIG-with-no-progress` |
|     - | 1351 | `					 * refusal, which is what keeps this from emitting an empty` |
|     - | 1352 | `					 * encoded word for ever. */` |
|   ! 0 | 1353 | `					rc = ICV_UNKNOWN_ERR;` |
|   ! 0 | 1354 | `					break;` |
|     - | 1355 | `				}` |
|   169 | 1356 | `				iCost = 0;` |
|  3775 | 1357 | `				for( k = 0 ; k < (int)SyBlobLength(&sChunk) ; ++k ){` |
|  3607 | 1358 | `					iCost += IcvQPrintCost(((const unsigned char *)SyBlobData(&sChunk))[k]);` |
|  1804 | 1359 | `				}` |
|   169 | 1360 | `				if( iCost <= iBudget - 2 ){` |
|    49 | 1361 | `					break;` |
|     - | 1362 | `				}` |
|   121 | 1363 | `				iRoom -= ((iCost - (iBudget - 2)) + 2) / 3;` |
|   121 | 1364 | `				i = iStart;` |
|     1 | 1365 | `			}` |
|    49 | 1366 | `			if( rc != ICV_OK ){` |
|   ! 0 | 1367 | `				break;` |
|     - | 1368 | `			}` |
|   507 | 1369 | `			for( k = 0 ; k < (int)SyBlobLength(&sChunk) ; ++k ){` |
|   459 | 1370 | `				int c = ((const unsigned char *)SyBlobData(&sChunk))[k];` |
|   459 | 1371 | `				if( IcvQPrintCost(c) == 1 ){` |
|   333 | 1372 | `					char b = (char)c;` |
|   333 | 1373 | `					SyBlobAppend(pOut,&b,1);` |
|   333 | 1374 | `					iBudget--;` |
|   167 | 1375 | `				}else{` |
|     - | 1376 | `					static const char zHex[] = "0123456789ABCDEF";` |
|     - | 1377 | `					char zEsc[3];` |
|   127 | 1378 | `					zEsc[0] = '='; zEsc[1] = zHex[(c >> 4) & 0x0F]; zEsc[2] = zHex[c & 0x0F];` |
|   127 | 1379 | `					SyBlobAppend(pOut,zEsc,3);` |
|   127 | 1380 | `					iBudget -= 3;` |
|     - | 1381 | `				}` |
|   230 | 1382 | `			}` |
|     - | 1383 | `		}` |
|    99 | 1384 | `		SyBlobAppend(pOut,"?=",2);` |
|    99 | 1385 | `		iBudget -= 2;` |
|    99 | 1386 | `	}while( i < nVal );` |
|    65 | 1387 | `	SyBlobRelease(&sChunk);` |
|    65 | 1388 | `	return rc;` |
|    33 | 1389 | `}` |
|     - | 1390 |  |
|     - | 1391 | `/*` |
|     - | 1392 | ` * php's _php_iconv_mime_decode(), a thirteen-state scanner over the header` |
|     - | 1393 | ` * text. What makes it a scanner rather than a matcher is what it does with the` |
|     - | 1394 | ` * things RFC 2047 does not allow: an encoded word that turns out not to be one` |
|     - | 1395 | ` * is re-emitted as the RAW TEXT it was, from the '=' the scan started at, and` |
|     - | 1396 | ` * the whitespace BETWEEN two encoded words is dropped while whitespace between` |
|     - | 1397 | `` * a word and plain text is kept -- which is why `=?..?= =?..?=` joins and`` |
|     - | 1398 | `` * `=?..?= x` does not.`` |
|     - | 1399 | ` *` |
|     - | 1400 | ` * $mode carries two bits. STRICT (1) refuses the non-RFC forms php otherwise` |
|     - | 1401 | ` * accepts -- an encoded word not followed by whitespace, above all -- and` |
|     - | 1402 | ` * CONTINUE_ON_ERROR (2) turns every refusal into "emit the raw word and carry` |
|     - | 1403 | ` * on". *pNext is left at the byte the scan stopped on, which is how` |
|     - | 1404 | ` * iconv_mime_decode_headers() walks a whole header block one field at a time.` |
|     - | 1405 | ` */` |
|   232 | 1406 | `static int IcvMimeDecode(ph7_context *pCtx,SyBlob *pOut,const char *zIn,int nIn,` |
|     - | 1407 | `	const icv_cs *pTo,int *pNext,int *pbTouched,int iMode)` |
|     2 | 1408 | `{` |
|     - | 1409 | `	SyBlob sWord;` |
|   234 | 1410 | `	const char *z = zIn;` |
|   234 | 1411 | `	int i = 0,rc = ICV_OK;` |
|   234 | 1412 | `	int iState = 0,iScheme = ICV_SCHEME_B64,nLeft,bTouched = 0;` |
|   234 | 1413 | `	int iWord = -1,iSpaces = -1,iCsName = -1,nCsName = 0,iText = -1,nText = 0;` |
|     - | 1414 | `	icv_cs sWordCs;` |
|   234 | 1415 | `	int bStrict = (iMode & 1) != 0,bGoOn = (iMode & 2) != 0;` |
|   234 | 1416 | `	SyBlobInit(&sWord,&pCtx->pVm->sAllocator);` |
|   234 | 1417 | `	sWordCs.iEnc = -1;` |
|   234 | 1418 | `	sWordCs.bTranslit = 0;` |
|   234 | 1419 | `	sWordCs.bIgnore = 0;` |
|   234 | 1420 | `	sWordCs.nName = 0;` |
|     - | 1421 | `	/*` |
|     - | 1422 | `	 * Emit z[iFrom..iTo) as literal text. php converts it from US-ASCII into the` |
|     - | 1423 | `	 * output charset, so a byte over 0x7F cannot go -- and what happens then is` |
|     - | 1424 | `	 * NOT one rule but three, because php checks the conversion's answer at some` |
|     - | 1425 | `	 * of these sites and not others. HARD fails whatever $mode says; CHECKED is` |
|     - | 1426 | `	 * the one CONTINUE_ON_ERROR was written for; SILENT drops the byte and says` |
|     - | 1427 | `	 * nothing, which is why a stray high byte AFTER a complete encoded word` |
|     - | 1428 | `	 * disappears while the same byte inside one is a failure.` |
|     - | 1429 | `	 */` |
|     - | 1430 | `/* php's _php_iconv_appendl() is ALL OR NOTHING: it converts into a buffer of` |
|     - | 1431 | ` * its own and only hands that to the output when the whole run went, so a run` |
|     - | 1432 | ` * that fails part-way appends nothing at all. And with a NULL source it appends` |
|     - | 1433 | ``  * nothing and answers success, which is what an already-consumed `encoded_word` `` |
|     - | 1434 | ` * becomes -- load-bearing, because state 9 stays in state 9 and re-runs for` |
|     - | 1435 | ` * every character after a word it could not convert. */` |
|     - | 1436 | `#define ICV_RAW_AT(iFrom,iTo,eMode) do { \` |
|     - | 1437 | `		int rcRaw = (iFrom) < 0 ? ICV_OK \` |
|     - | 1438 | `			: IcvAppendConv(pCtx,pOut,&z[iFrom],(iTo) - (iFrom),ICV_ASCII,pTo); \` |
|     - | 1439 | `		bTouched = 1; \` |
|     - | 1440 | `		if( (eMode) != 2 ){ \` |
|     - | 1441 | ``			/* php's `err` is a RUNNING variable at the checked sites: a later \`` |
|     - | 1442 | `			 * successful append assigns SUCCESS over an earlier failure, so a \` |
|     - | 1443 | `			 * refusal recorded mid-scan can still be forgotten. The silent \` |
|     - | 1444 | `			 * sites never touch it at all. */ \` |
|     - | 1445 | `			rc = rcRaw; \` |
|     - | 1446 | `			if( rcRaw != ICV_OK && ((eMode) == 1 \|\| !bGoOn) ){ \` |
|     - | 1447 | `				goto done; \` |
|     - | 1448 | `			} \` |
|     - | 1449 | `			if( rcRaw != ICV_OK ){ rc = ICV_OK; } \` |
|     - | 1450 | `		} \` |
|     - | 1451 | `	} while(0)` |
|     - | 1452 | `#define ICV_RAW(iFrom,iTo)        ICV_RAW_AT(iFrom,iTo,0)` |
|     - | 1453 | `#define ICV_RAW_HARD(iFrom,iTo)   ICV_RAW_AT(iFrom,iTo,1)` |
|     - | 1454 | `#define ICV_RAW_SILENT(iFrom,iTo) ICV_RAW_AT(iFrom,iTo,2)` |
|     - | 1455 | `	/*` |
|     - | 1456 | `	 * php walks with a POSITION and a separate COUNT, and the two go out of step` |
|     - | 1457 | ``	 * on purpose: several states rewind the position by one (`--p1`) without`` |
|     - | 1458 | `	 * giving the count back, so the scan re-reads a byte and then stops one byte` |
|     - | 1459 | `	 * SHORT of the end. That is not an accident of style -- it is what makes` |
|     - | 1460 | ``	 * `iconv_mime_decode("UHLDvGZ1bmc=\r\n")` a "Malformed string": the final`` |
|     - | 1461 | `	 * "\n" is never reached, so the scanner ends mid-EOL instead of after one.` |
|     - | 1462 | `	 */` |
|   234 | 1463 | `	nLeft = nIn;` |
|  3188 | 1464 | `	while( nLeft > 0 ){` |
|  2978 | 1465 | `		int c = (unsigned char)z[i];` |
|  2978 | 1466 | `		int bEos = 0;` |
|  2978 | 1467 | `		switch( iState ){` |
|   393 | 1468 | `		case 0:   /* anything at all */` |
|   788 | 1469 | `			if( c == '\r' ){ iState = 7; break; }` |
|   736 | 1470 | `			if( c == '\n' ){ iState = 8; break; }` |
|   730 | 1471 | `			if( c == '=' ){ iWord = i; iState = 1; break; }` |
|   613 | 1472 | `			if( c == ' ' \|\| c == '\t' ){ iSpaces = i; iState = 11; break; }` |
|   529 | 1473 | `			ICV_RAW(i,i+1);` |
|   529 | 1474 | `			iWord = -1;` |
|   529 | 1475 | `			if( bStrict ){ iState = 12; }` |
|   529 | 1476 | `			break;` |
|    75 | 1477 | `		case 1:   /* after '=': expecting '?' */` |
|   152 | 1478 | `			if( c != '?' ){` |
|   ! 0 | 1479 | `				if( c == '\r' \|\| c == '\n' ){ i--; }` |
|   ! 0 | 1480 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1481 | `				iWord = -1;` |
|   ! 0 | 1482 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1483 | `				break;` |
|     - | 1484 | `			}` |
|   152 | 1485 | `			iCsName = i + 1;` |
|   152 | 1486 | `			iState = 2;` |
|   152 | 1487 | `			break;` |
|   405 | 1488 | `		case 2:   /* the charset name */` |
|   812 | 1489 | `			if( c == '\r' \|\| c == '\n' ){` |
|   ! 0 | 1490 | `				i--;` |
|   ! 0 | 1491 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1492 | `				iCsName = -1;` |
|   ! 0 | 1493 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1494 | `				break;` |
|     - | 1495 | `			}` |
|   812 | 1496 | `			if( c != '?' && c != '*' ){` |
|   686 | 1497 | `				break;` |
|     - | 1498 | `			}` |
|   128 | 1499 | `			nCsName = i - iCsName;` |
|   128 | 1500 | `			if( nCsName > ICV_CSNMAXLEN + 15 ){` |
|     - | 1501 | `				/* php's own 80-byte scratch buffer for the name. */` |
|   ! 0 | 1502 | `				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|   ! 0 | 1503 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1504 | `				iWord = -1;` |
|   ! 0 | 1505 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1506 | `				break;` |
|     - | 1507 | `			}` |
|   128 | 1508 | `			IcvParseCharset(&z[iCsName],nCsName,&sWordCs);` |
|   128 | 1509 | `			if( sWordCs.iEnc < 0 ){` |
|    23 | 1510 | `				if( !bGoOn ){` |
|    11 | 1511 | `					rc = ICV_WRONG_CHARSET;` |
|    11 | 1512 | `					goto done;` |
|     - | 1513 | `				}` |
|     - | 1514 | `				/* php skips to the end of the word and hands it over raw. */` |
|     - | 1515 | `				{` |
|    13 | 1516 | `					int nQ = 2;` |
|    97 | 1517 | `					while( nQ > 0 && nLeft > 1 ){` |
|    85 | 1518 | `						if( z[++i] == '?' ){ nQ--; }` |
|    85 | 1519 | `						nLeft--;` |
|     1 | 1520 | `					}` |
|    13 | 1521 | `					if( i + 1 < nIn && z[i+1] == '=' ){` |
|    13 | 1522 | `						i++;` |
|    13 | 1523 | `						if( nLeft > 1 ){ nLeft--; }` |
|     6 | 1524 | `					}` |
|     - | 1525 | `				}` |
|    13 | 1526 | `				ICV_RAW_HARD(iWord,i+1);` |
|    13 | 1527 | `				iState = 12;` |
|    13 | 1528 | `				break;` |
|     - | 1529 | `			}` |
|   106 | 1530 | `			iState = (c == '*') ? 10 : 3;` |
|   106 | 1531 | `			break;` |
|    48 | 1532 | `		case 3:   /* the scheme letter */` |
|    98 | 1533 | `			if( c == 'b' \|\| c == 'B' ){ iScheme = ICV_SCHEME_B64; iState = 4; break; }` |
|    30 | 1534 | `			if( c == 'q' \|\| c == 'Q' ){ iScheme = ICV_SCHEME_QP; iState = 4; break; }` |
|     9 | 1535 | `			if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|     5 | 1536 | `			ICV_RAW_HARD(iWord,i+1);` |
|     5 | 1537 | `			iWord = -1;` |
|     5 | 1538 | `			iState = bStrict ? 12 : 0;` |
|     5 | 1539 | `			break;` |
|    44 | 1540 | `		case 4:   /* expecting '?' */` |
|    90 | 1541 | `			if( c != '?' ){` |
|   ! 0 | 1542 | `				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|   ! 0 | 1543 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1544 | `				iWord = -1;` |
|   ! 0 | 1545 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1546 | `				break;` |
|     - | 1547 | `			}` |
|    90 | 1548 | `			iText = i + 1;` |
|    90 | 1549 | `			iState = 5;` |
|    90 | 1550 | `			break;` |
|   271 | 1551 | `		case 5:   /* the encoded text */` |
|   544 | 1552 | `			if( c == '?' ){` |
|    82 | 1553 | `				nText = i - iText;` |
|    82 | 1554 | `				iState = 6;` |
|    40 | 1555 | `			}` |
|   544 | 1556 | `			break;` |
|    36 | 1557 | `		case 6:   /* expecting the closing '=' */` |
|    74 | 1558 | `			if( c != '=' ){` |
|   ! 0 | 1559 | `				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|   ! 0 | 1560 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1561 | `				iWord = -1;` |
|   ! 0 | 1562 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1563 | `				break;` |
|     - | 1564 | `			}` |
|    74 | 1565 | `			iState = 9;` |
|    96 | 1566 | `			if( nLeft == 1 ){` |
|    46 | 1567 | `				bEos = 1;` |
|    24 | 1568 | `			}else{` |
|    29 | 1569 | `				break;` |
|     - | 1570 | `			}` |
|     - | 1571 | `			/* fall through -- the word ended with the string */` |
|     - | 1572 | `			/* FALLTHROUGH */` |
|     - | 1573 | `		case 9:   /* what follows a complete word */` |
|    74 | 1574 | `			if( !bEos && c != '\r' && c != '\n' && c != ' ' && c != '\t' && bStrict ){` |
|     5 | 1575 | `				ICV_RAW_HARD(iWord,i+1);` |
|     5 | 1576 | `				iState = 12;` |
|     5 | 1577 | `				break;` |
|     - | 1578 | `			}` |
|    70 | 1579 | `			SyBlobReset(&sWord);` |
|    70 | 1580 | `			if( iScheme == ICV_SCHEME_B64 ){` |
|    49 | 1581 | `				IcvBase64Decode(&sWord,&z[iText],nText);` |
|    46 | 1582 | `			}else if( !IcvQPrintDecode(&sWord,&z[iText],nText,1) ){` |
|     9 | 1583 | `				if( !bGoOn ){ rc = ICV_UNKNOWN_ERR; goto done; }` |
|     3 | 1584 | `				ICV_RAW_HARD(iWord,i+1);` |
|     3 | 1585 | `				iWord = -1;` |
|     3 | 1586 | `				iState = bStrict ? 12 : 0;` |
|     3 | 1587 | `				break;` |
|     - | 1588 | `			}` |
|     - | 1589 | `			{` |
|     - | 1590 | `				int rcW;` |
|    61 | 1591 | `				bTouched = 1;` |
|    91 | 1592 | `				rcW = IcvAppendConv(pCtx,pOut,(const char *)SyBlobData(&sWord),` |
|    60 | 1593 | `					(int)SyBlobLength(&sWord),sWordCs.iEnc,pTo);` |
|    61 | 1594 | `				if( rcW != ICV_OK ){` |
|     3 | 1595 | `					if( !bGoOn ){ rc = rcW; goto done; }` |
|     - | 1596 | `					/* php hands the raw word over and carries on. If THAT will` |
|     - | 1597 | ``					 * not convert either it keeps the failure in `err` and stays`` |
|     - | 1598 | `					 * in this state, so the next character re-runs the whole` |
|     - | 1599 | `					 * branch; when it DOES convert the state moves on below. */` |
|   ! 0 | 1600 | `				rc = iWord < 0 ? ICV_OK` |
|   ! 0 | 1601 | `						: IcvAppendConv(pCtx,pOut,&z[iWord],i - iWord,ICV_ASCII,pTo);` |
|   ! 0 | 1602 | `					bTouched = 1;` |
|   ! 0 | 1603 | `					iWord = -1;` |
|   ! 0 | 1604 | `					if( rc != ICV_OK ){` |
|   ! 0 | 1605 | `						break;` |
|     - | 1606 | `					}` |
|   ! 0 | 1607 | `				}` |
|     - | 1608 | `			}` |
|    59 | 1609 | `			if( bEos ){` |
|    35 | 1610 | `				iState = 0;` |
|    35 | 1611 | `				break;` |
|     - | 1612 | `			}` |
|    25 | 1613 | `			if( c == '\r' ){ iState = 7; break; }` |
|    15 | 1614 | `			if( c == '\n' ){ iState = 8; break; }` |
|    13 | 1615 | `			if( c == '=' ){ iWord = i; iState = 1; break; }` |
|    13 | 1616 | `			if( c == ' ' \|\| c == '\t' ){ iSpaces = i; iState = 11; break; }` |
|     7 | 1617 | `			bTouched = 1;` |
|     7 | 1618 | `			ICV_RAW_SILENT(i,i+1);` |
|     7 | 1619 | `			iState = 12;` |
|     7 | 1620 | `			break;` |
|    36 | 1621 | `		case 7:   /* after CR: expecting LF */` |
|    73 | 1622 | `			if( c == '\n' ){` |
|    71 | 1623 | `				iState = 8;` |
|    36 | 1624 | `			}else{` |
|     3 | 1625 | `				SyBlobAppend(pOut,"\r",1);` |
|     3 | 1626 | `				bTouched = 1;` |
|     3 | 1627 | `				ICV_RAW_SILENT(i,i+1);` |
|     3 | 1628 | `				iState = 0;` |
|     - | 1629 | `			}` |
|    73 | 1630 | `			break;` |
|    22 | 1631 | `		case 8:   /* after a newline: is the next line a continuation? */` |
|    45 | 1632 | `			if( c != ' ' && c != '\t' ){` |
|     - | 1633 | `				/* The field ended here. Rewinding the position and setting the` |
|     - | 1634 | `				 * count to one leaves *pNext pointing AT this character, so the` |
|     - | 1635 | `				 * caller's next field starts on it. */` |
|    39 | 1636 | `				nLeft = 1;` |
|    39 | 1637 | `				i--;` |
|    39 | 1638 | `				break;` |
|     - | 1639 | `			}` |
|     7 | 1640 | `			if( iWord < 0 ){` |
|     7 | 1641 | `				SyBlobAppend(pOut," ",1);` |
|     7 | 1642 | `				bTouched = 1;` |
|     3 | 1643 | `			}` |
|     7 | 1644 | `			iSpaces = -1;` |
|     7 | 1645 | `			iState = 11;` |
|     7 | 1646 | `			break;` |
|     3 | 1647 | `		case 10:  /* a language tag after the charset: dismissed */` |
|     7 | 1648 | `			if( c == '?' ){ iState = 3; }` |
|     7 | 1649 | `			break;` |
|    67 | 1650 | `		case 11:  /* a run of whitespace */` |
|   135 | 1651 | `			if( c == '\r' ){ iState = 7; break; }` |
|   135 | 1652 | `			if( c == '\n' ){ iState = 8; break; }` |
|   135 | 1653 | `			if( c == '=' ){` |
|     - | 1654 | `				/* Whitespace BETWEEN two encoded words disappears; whitespace` |
|     - | 1655 | `				 * that followed plain text does not. */` |
|    43 | 1656 | `				if( iSpaces >= 0 && iWord < 0 ){` |
|    39 | 1657 | `					bTouched = 1;` |
|    39 | 1658 | `					ICV_RAW_SILENT(iSpaces,i);` |
|    39 | 1659 | `					iSpaces = -1;` |
|    19 | 1660 | `				}` |
|    43 | 1661 | `				iWord = i;` |
|    43 | 1662 | `				iState = 1;` |
|    43 | 1663 | `				break;` |
|     - | 1664 | `			}` |
|    93 | 1665 | `			if( c == ' ' \|\| c == '\t' ){` |
|     4 | 1666 | `				break;` |
|     - | 1667 | `			}` |
|    87 | 1668 | `			if( iSpaces >= 0 ){` |
|    81 | 1669 | `				ICV_RAW_SILENT(iSpaces,i);` |
|    40 | 1670 | `			}` |
|    87 | 1671 | `			iSpaces = -1;` |
|    87 | 1672 | `			bTouched = 1;` |
|    87 | 1673 | `			ICV_RAW_SILENT(i,i+1);` |
|    87 | 1674 | `			iWord = -1;` |
|    87 | 1675 | `			iState = bStrict ? 12 : 0;` |
|    87 | 1676 | `			break;` |
|    74 | 1677 | `		case 12:  /* plain text */` |
|   149 | 1678 | `			if( c == '\r' ){ iState = 7; break; }` |
|   139 | 1679 | `			if( c == '\n' ){ iState = 8; break; }` |
|   139 | 1680 | `			if( c == ' ' \|\| c == '\t' ){ iSpaces = i; iState = 11; break; }` |
|   105 | 1681 | `			if( c == '=' && !bStrict ){ iWord = i; iState = 1; break; }` |
|   103 | 1682 | `			bTouched = 1;` |
|   103 | 1683 | `			ICV_RAW_SILENT(i,i+1);` |
|   102 | 1684 | `			break;` |
|     - | 1685 | `		}` |
|  2956 | 1686 | `		nLeft--;` |
|  2956 | 1687 | `		i++;` |
|     2 | 1688 | `	}` |
|   212 | 1689 | `	switch( iState ){` |
|    76 | 1690 | `	case 0: case 8: case 11: case 12:` |
|   154 | 1691 | `		break;` |
|    29 | 1692 | `	default:` |
|    59 | 1693 | `		if( bGoOn ){` |
|    31 | 1694 | `			if( iState == 1 ){` |
|     7 | 1695 | `				SyBlobAppend(pOut,"=",1);` |
|     3 | 1696 | `			}` |
|     - | 1697 | `			/* CONTINUE_ON_ERROR clears whatever the scan recorded on its way` |
|     - | 1698 | `			 * through, which is why a header full of bytes the output charset` |
|     - | 1699 | `			 * cannot hold comes back as the empty string rather than FALSE. */` |
|    31 | 1700 | `			rc = ICV_OK;` |
|    16 | 1701 | `		}else{` |
|    29 | 1702 | `			rc = ICV_MALFORMED;` |
|     - | 1703 | `		}` |
|    58 | 1704 | `		break;` |
|   105 | 1705 | `	}` |
|   116 | 1706 | `done:` |
|   234 | 1707 | `	SyBlobRelease(&sWord);` |
|   234 | 1708 | `	if( pNext ){` |
|    77 | 1709 | `		*pNext = i;` |
|    38 | 1710 | `	}` |
|   234 | 1711 | `	if( pbTouched ){` |
|    77 | 1712 | `		*pbTouched = bTouched;` |
|    38 | 1713 | `	}` |
|   234 | 1714 | `	return rc;` |
|     2 | 1715 | `}` |
|     - | 1716 | `#undef ICV_RAW_AT` |
|     - | 1717 | `#undef ICV_RAW` |
|     - | 1718 | `#undef ICV_RAW_HARD` |
|     - | 1719 | `#undef ICV_RAW_SILENT` |
|     - | 1720 |  |
|     - | 1721 | `/*` |
|     - | 1722 | ` * Read one option out of an OPTIONS array. php reads three of the five as` |
|     - | 1723 | `` * STRINGS ONLY -- a non-string `scheme`, `input-charset` or `output-charset` is`` |
|     - | 1724 | ` * not coerced, it is ignored -- so those need no conversion at all and the` |
|     - | 1725 | ` * array is never touched. The other two ARE coerced, and a coercion has to go` |
|     - | 1726 | `` * through a scratch copy: `ph7_value_to_xxx()` converts the value it is handed,`` |
|     - | 1727 | ` * which would rewrite the caller's own array (the defect §2 records four` |
|     - | 1728 | ` * builtins sharing).` |
|     - | 1729 | ` */` |
|   174 | 1730 | `static const char * IcvOptionRawStr(ph7_value *pOpt,const char *zKey,int *pnOut)` |
|     1 | 1731 | `{` |
|   175 | 1732 | `	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));` |
|   175 | 1733 | `	if( pV == 0 \|\| !ph7_value_is_string(pV) ){` |
|   121 | 1734 | `		return 0;` |
|     - | 1735 | `	}` |
|    55 | 1736 | `	return ph7_value_to_string(pV,pnOut);` |
|    88 | 1737 | `}` |
|    56 | 1738 | `static int IcvOptionInt(ph7_context *pCtx,ph7_value *pOpt,const char *zKey,sxi64 *pOut)` |
|     1 | 1739 | `{` |
|    57 | 1740 | `	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));` |
|    57 | 1741 | `	if( pV == 0 ){` |
|    41 | 1742 | `		return 0;` |
|     - | 1743 | `	}` |
|     8 | 1744 | `	SXUNUSED(pCtx);` |
|    17 | 1745 | `	*pOut = PH7_ValuePeekInt64(pV);` |
|    17 | 1746 | `	return 1;` |
|    29 | 1747 | `}` |
|     - | 1748 | `/* Copy one option into pOut as a string, coercing whatever it is. */` |
|    56 | 1749 | `static int IcvOptionCopyStr(ph7_context *pCtx,ph7_value *pOpt,const char *zKey,SyBlob *pOut)` |
|     1 | 1750 | `{` |
|    57 | 1751 | `	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));` |
|     - | 1752 | `	ph7_value sScratch,*pCopy;` |
|     - | 1753 | `	const char *z;` |
|    57 | 1754 | `	int n = 0;` |
|    57 | 1755 | `	if( pV == 0 ){` |
|    53 | 1756 | `		return 0;` |
|     - | 1757 | `	}` |
|     5 | 1758 | `	PH7_MemObjInit(pCtx->pVm,&sScratch);` |
|     5 | 1759 | `	pCopy = PH7_ValuePeek(pV,&sScratch);` |
|     5 | 1760 | `	z = ph7_value_to_string(pCopy,&n);` |
|     5 | 1761 | `	if( n > 0 ){` |
|     3 | 1762 | `		SyBlobAppend(pOut,z,(sxu32)n);` |
|     1 | 1763 | `	}` |
|     5 | 1764 | `	PH7_MemObjRelease(&sScratch);` |
|     5 | 1765 | `	return 1;` |
|    29 | 1766 | `}` |
|     - | 1767 |  |
|     - | 1768 | `/*` |
|     - | 1769 | ` * string\|false iconv_mime_encode(string $field_name, string $field_value,` |
|     - | 1770 | ` *                                array $options = [])` |
|     - | 1771 | ` *` |
|     - | 1772 | `` * The five options php reads, and the exact way it reads them: `scheme` is one`` |
|     - | 1773 | ` * LETTER (its first, case-insensitively, and anything but B/b/Q/q leaves the` |
|     - | 1774 | `` * default alone rather than failing), `input-charset` and `output-charset` are`` |
|     - | 1775 | `` * only honoured when they are non-empty STRINGS, `line-length` goes through an`` |
|     - | 1776 | `` * ordinary int cast, and `line-break-chars` is taken as a string whatever it is.`` |
|     - | 1777 | ` */` |
|    78 | 1778 | `static int PH7_builtin_iconv_mime_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1779 | `{` |
|    79 | 1780 | `	const char *zName,*zVal,*zLf = "\r\n",*zIn = 0,*zOut = 0;` |
|    79 | 1781 | `	int nName,nVal,nLf = 2,nIn = 0,nOut = 0,iScheme = ICV_SCHEME_B64,err;` |
|    79 | 1782 | `	sxi64 iMaxLine = 76;` |
|     - | 1783 | `	icv_cs sFrom,sTo;` |
|     - | 1784 | `	SyBlob sOut,sIniLf,sIniIn,sIniOut,sIniCharset;` |
|     - | 1785 | `	ph7_value *pOpt;` |
|    79 | 1786 | `	if( nArg < 2 ){` |
|   ! 0 | 1787 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1788 | `		return PH7_OK;` |
|     - | 1789 | `	}` |
|    79 | 1790 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    79 | 1791 | `	zVal = ph7_value_to_string(apArg[1],&nVal);` |
|    79 | 1792 | `	SyBlobInit(&sIniLf,&pCtx->pVm->sAllocator);` |
|    79 | 1793 | `	SyBlobInit(&sIniIn,&pCtx->pVm->sAllocator);` |
|    79 | 1794 | `	SyBlobInit(&sIniOut,&pCtx->pVm->sAllocator);` |
|    79 | 1795 | `	SyBlobInit(&sIniCharset,&pCtx->pVm->sAllocator);` |
|    79 | 1796 | `	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIniCharset);` |
|    79 | 1797 | `	zIn = (const char *)SyBlobData(&sIniCharset);` |
|    79 | 1798 | `	nIn = (int)SyBlobLength(&sIniCharset);` |
|    79 | 1799 | `	zOut = zIn;` |
|    79 | 1800 | `	nOut = nIn;` |
|    79 | 1801 | `	pOpt = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|    79 | 1802 | `	if( pOpt ){` |
|     - | 1803 | `		const char *zS;` |
|    59 | 1804 | `		int nS = 0;` |
|    59 | 1805 | `		if( (zS = IcvOptionRawStr(pOpt,"scheme",&nS)) != 0 && nS > 0 ){` |
|     - | 1806 | `			/* One LETTER, case-insensitively, and anything but B or Q leaves the` |
|     - | 1807 | `			 * default where it was rather than failing. */` |
|    33 | 1808 | `			int c = IcvUpper((unsigned char)zS[0]);` |
|    33 | 1809 | `			if( c == 'B' ){ iScheme = ICV_SCHEME_B64; }` |
|    27 | 1810 | `			else if( c == 'Q' ){ iScheme = ICV_SCHEME_QP; }` |
|    16 | 1811 | `		}` |
|    59 | 1812 | `		if( (zS = IcvOptionRawStr(pOpt,"input-charset",&nS)) != 0 ){` |
|     9 | 1813 | `			if( nS >= ICV_CSNMAXLEN ){` |
|   ! 0 | 1814 | `				err = ICV_TOO_LONG;` |
|   ! 0 | 1815 | `				goto fail;` |
|     - | 1816 | `			}` |
|     9 | 1817 | `			if( nS > 0 ){` |
|     9 | 1818 | `				SyBlobAppend(&sIniIn,zS,(sxu32)nS);` |
|     9 | 1819 | `				zIn = (const char *)SyBlobData(&sIniIn);` |
|     9 | 1820 | `				nIn = nS;` |
|     4 | 1821 | `			}` |
|     4 | 1822 | `		}` |
|    59 | 1823 | `		if( (zS = IcvOptionRawStr(pOpt,"output-charset",&nS)) != 0 ){` |
|    13 | 1824 | `			if( nS >= ICV_CSNMAXLEN ){` |
|     3 | 1825 | `				err = ICV_TOO_LONG;` |
|     3 | 1826 | `				goto fail;` |
|     - | 1827 | `			}` |
|    11 | 1828 | `			if( nS > 0 ){` |
|    11 | 1829 | `				SyBlobAppend(&sIniOut,zS,(sxu32)nS);` |
|    11 | 1830 | `				zOut = (const char *)SyBlobData(&sIniOut);` |
|    11 | 1831 | `				nOut = nS;` |
|     5 | 1832 | `			}` |
|     5 | 1833 | `		}` |
|    57 | 1834 | `		IcvOptionInt(pCtx,pOpt,"line-length",&iMaxLine);` |
|    57 | 1835 | `		if( IcvOptionCopyStr(pCtx,pOpt,"line-break-chars",&sIniLf) ){` |
|     5 | 1836 | `			zLf = (const char *)SyBlobData(&sIniLf);` |
|     5 | 1837 | `			nLf = (int)SyBlobLength(&sIniLf);` |
|     2 | 1838 | `		}` |
|    28 | 1839 | `	}` |
|     - | 1840 | `	/* php measures the LINE BUDGET before it opens a converter, so a header` |
|     - | 1841 | `	 * that cannot fit is refused whatever the charsets say. (php compares the` |
|     - | 1842 | ``	 * budget as a size_t, so a NEGATIVE `line-length` is a huge one there and it`` |
|     - | 1843 | `	 * dies in the allocator instead -- "Possible integer overflow in memory` |
|     - | 1844 | `	 * allocation"; PHL compares signed, so it lands on the same refusal every` |
|     - | 1845 | `	 * other impossible budget gets. §7.4.) */` |
|    77 | 1846 | `	if( (sxi64)nName + 2 >= iMaxLine \|\| (sxi64)nOut + 12 >= iMaxLine ){` |
|     9 | 1847 | `		err = ICV_TOO_BIG;` |
|     9 | 1848 | `		goto fail;` |
|     - | 1849 | `	}` |
|    69 | 1850 | `	IcvParseCharset(zIn,nIn,&sFrom);` |
|    69 | 1851 | `	IcvParseCharset(zOut,nOut,&sTo);` |
|    69 | 1852 | `	if( sFrom.iEnc < 0 \|\| sTo.iEnc < 0 ){` |
|     5 | 1853 | `		err = ICV_WRONG_CHARSET;` |
|     5 | 1854 | `		goto fail;` |
|     - | 1855 | `	}` |
|    65 | 1856 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    97 | 1857 | `	err = IcvMimeEncode(pCtx,&sOut,zName,nName,zVal,nVal,iMaxLine,zLf,nLf,` |
|    32 | 1858 | `		iScheme,&sTo,zOut,nOut,sFrom.iEnc);` |
|    65 | 1859 | `	if( err == ICV_OK ){` |
|    61 | 1860 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    30 | 1861 | `	}` |
|    65 | 1862 | `	SyBlobRelease(&sOut);` |
|    65 | 1863 | `	if( err != ICV_OK ){` |
|     5 | 1864 | `		goto fail;` |
|     - | 1865 | `	}` |
|    61 | 1866 | `	SyBlobRelease(&sIniLf); SyBlobRelease(&sIniIn);` |
|    61 | 1867 | `	SyBlobRelease(&sIniOut); SyBlobRelease(&sIniCharset);` |
|    61 | 1868 | `	return PH7_OK;` |
|     9 | 1869 | `fail:` |
|    19 | 1870 | `	if( err == ICV_TOO_LONG ){` |
|     3 | 1871 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 1872 | `			"Encoding parameter exceeds the maximum allowed length of %d characters",` |
|     - | 1873 | `			ICV_CSNMAXLEN);` |
|     2 | 1874 | `	}else{` |
|    17 | 1875 | `		IcvShowError(pCtx,err,zOut,nOut,zIn,nIn);` |
|     - | 1876 | `	}` |
|    19 | 1877 | `	ph7_result_bool(pCtx,0);` |
|    19 | 1878 | `	SyBlobRelease(&sIniLf); SyBlobRelease(&sIniIn);` |
|    19 | 1879 | `	SyBlobRelease(&sIniOut); SyBlobRelease(&sIniCharset);` |
|    19 | 1880 | `	return PH7_OK;` |
|    40 | 1881 | `}` |
|     - | 1882 |  |
|     - | 1883 | ``/* Resolve the `?string $encoding = null` the two decoders share: php's`` |
|     - | 1884 | ` * internal encoding, with only the length cap raised here. */` |
|   206 | 1885 | `static int IcvMimeDecodeArgs(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|     - | 1886 | `	icv_cs *pCs,sxi64 *pMode)` |
|     2 | 1887 | `{` |
|   208 | 1888 | `	*pMode = 0;` |
|   206 | 1889 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1])` |
|   206 | 1890 | `	 && PH7_IntArgResolve(pCtx,apArg[1],"iconv_mime_decode",2,"$mode","int",pMode) != PH7_OK ){` |
|   ! 0 | 1891 | `		return 0;` |
|     - | 1892 | `	}` |
|   208 | 1893 | `	if( !IcvStrEncArg(pCtx,nArg > 2 ? apArg[2] : 0,pCs) ){` |
|     3 | 1894 | `		return 0;` |
|     - | 1895 | `	}` |
|   206 | 1896 | `	if( pCs->iEnc < 0 ){` |
|     - | 1897 | ``		/* php names the source `"???"` here: the decoder learns the real one`` |
|     - | 1898 | `		 * word by word, so there is nothing to report but the target. */` |
|     5 | 1899 | `		IcvShowError(pCtx,ICV_WRONG_CHARSET,pCs->zName,pCs->nName,"???",3);` |
|     5 | 1900 | `		return 0;` |
|     - | 1901 | `	}` |
|   202 | 1902 | `	return 1;` |
|   105 | 1903 | `}` |
|     - | 1904 |  |
|     - | 1905 | `/* string\|false iconv_mime_decode(string $string, int $mode = 0,` |
|     - | 1906 | ` *                                ?string $encoding = null) */` |
|   160 | 1907 | `static int PH7_builtin_iconv_mime_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1908 | `{` |
|     - | 1909 | `	const char *zIn;` |
|     - | 1910 | `	int nIn,err;` |
|     - | 1911 | `	sxi64 iMode;` |
|     - | 1912 | `	icv_cs sCs;` |
|     - | 1913 | `	SyBlob sOut;` |
|   162 | 1914 | `	if( nArg < 1 \|\| !IcvMimeDecodeArgs(pCtx,nArg,apArg,&sCs,&iMode) ){` |
|     5 | 1915 | `		ph7_result_bool(pCtx,0);` |
|     5 | 1916 | `		return PH7_OK;` |
|     - | 1917 | `	}` |
|   158 | 1918 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|   158 | 1919 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   158 | 1920 | `	err = IcvMimeDecode(pCtx,&sOut,zIn,nIn,&sCs,0,0,(int)iMode);` |
|   158 | 1921 | `	if( err == ICV_OK ){` |
|   110 | 1922 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    56 | 1923 | `	}else{` |
|    50 | 1924 | `		IcvShowError(pCtx,err,sCs.zName,sCs.nName,"???",3);` |
|    50 | 1925 | `		ph7_result_bool(pCtx,0);` |
|     - | 1926 | `	}` |
|   158 | 1927 | `	SyBlobRelease(&sOut);` |
|   158 | 1928 | `	return PH7_OK;` |
|    82 | 1929 | `}` |
|     - | 1930 |  |
|     - | 1931 | `/*` |
|     - | 1932 | ` * array\|false iconv_mime_decode_headers(string $headers, int $mode = 0,` |
|     - | 1933 | ` *                                       ?string $encoding = null)` |
|     - | 1934 | ` *` |
|     - | 1935 | ` * Decode one field at a time, splitting each at its FIRST ':' -- a line with` |
|     - | 1936 | ` * none is dropped entirely, which is what ends the walk at the blank line that` |
|     - | 1937 | ` * separates headers from a body. A name seen twice becomes a LIST, and the` |
|     - | 1938 | ` * first value is kept as a plain string until the second arrives.` |
|     - | 1939 | ` */` |
|    46 | 1940 | `static int PH7_builtin_iconv_mime_decode_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1941 | `{` |
|     - | 1942 | `	const char *zIn;` |
|    47 | 1943 | `	int nIn,i = 0,err = ICV_OK;` |
|     - | 1944 | `	sxi64 iMode;` |
|     - | 1945 | `	icv_cs sCs;` |
|     - | 1946 | `	ph7_value *pArray,*pVal;` |
|    47 | 1947 | `	if( nArg < 1 \|\| !IcvMimeDecodeArgs(pCtx,nArg,apArg,&sCs,&iMode) ){` |
|     3 | 1948 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1949 | `		return PH7_OK;` |
|     - | 1950 | `	}` |
|    45 | 1951 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    45 | 1952 | `	pArray = ph7_context_new_array(pCtx);` |
|    45 | 1953 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    45 | 1954 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 1955 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1956 | `	}` |
|   117 | 1957 | `	while( i < nIn ){` |
|     - | 1958 | `		SyBlob sLine;` |
|     - | 1959 | `		const char *zLine;` |
|    77 | 1960 | `		int nLine,iColon,iNext = 0,k,bTouched = 0;` |
|    77 | 1961 | `		SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|    77 | 1962 | `		err = IcvMimeDecode(pCtx,&sLine,&zIn[i],nIn - i,&sCs,&iNext,&bTouched,(int)iMode);` |
|    77 | 1963 | `		if( err != ICV_OK ){` |
|     3 | 1964 | `			SyBlobRelease(&sLine);` |
|     4 | 1965 | `			break;` |
|     - | 1966 | `		}` |
|    75 | 1967 | `		nLine = (int)SyBlobLength(&sLine);` |
|    75 | 1968 | `		if( !bTouched ){` |
|     - | 1969 | `			/* php's buffer is still NULL here -- nothing was appended AT ALL,` |
|     - | 1970 | `			 * not even zero bytes -- and it leaves the loop. That is the blank` |
|     - | 1971 | `			 * line that ends a header block, so a body beyond it is never read;` |
|     - | 1972 | `			 * a field whose value is EMPTY did append and keeps the walk going. */` |
|     3 | 1973 | `			SyBlobRelease(&sLine);` |
|     3 | 1974 | `			break;` |
|     - | 1975 | `		}` |
|    73 | 1976 | `		zLine = (const char *)SyBlobData(&sLine);` |
|   333 | 1977 | `		for( iColon = 0 ; iColon < nLine && zLine[iColon] != ':' ; ++iColon ){}` |
|    73 | 1978 | `		if( iColon < nLine ){` |
|     - | 1979 | `			ph7_value *pSlot;` |
|    67 | 1980 | `			k = iColon + 1;` |
|   162 | 1981 | `			while( k < nLine && (zLine[k] == ' ' \|\| zLine[k] == '\t') ){ k++; }` |
|    67 | 1982 | `			ph7_value_string(pVal,&zLine[k],nLine - k);` |
|    67 | 1983 | `			pSlot = ph7_array_fetch(pArray,zLine,iColon);` |
|    67 | 1984 | `			if( pSlot == 0 ){` |
|    85 | 1985 | `				PH7_HashmapInsertRawKey((ph7_hashmap *)pArray->x.pOther,` |
|    28 | 1986 | `					zLine,(sxu32)iColon,pVal);` |
|    29 | 1987 | `			}else{` |
|     - | 1988 | `				/* A name seen twice becomes a LIST -- and the first value was` |
|     - | 1989 | `				 * stored as a plain string, so it has to be lifted into one` |
|     - | 1990 | `				 * now, which is php's own shape for a repeated header. */` |
|    11 | 1991 | `				if( !ph7_value_is_array(pSlot) ){` |
|     7 | 1992 | `					ph7_value *pList = ph7_context_new_array(pCtx);` |
|     7 | 1993 | `					if( pList == 0 ){` |
|   ! 0 | 1994 | `						SyBlobRelease(&sLine);` |
|   ! 0 | 1995 | `						ph7_context_release_value(pCtx,pVal);` |
|   ! 0 | 1996 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 1997 | `					}` |
|     7 | 1998 | `					ph7_array_add_strkey_elem(pList,0,pSlot);` |
|     7 | 1999 | `					ph7_array_add_strkey_elem(pList,0,pVal);` |
|    10 | 2000 | `					PH7_HashmapInsertRawKey((ph7_hashmap *)pArray->x.pOther,` |
|     3 | 2001 | `						zLine,(sxu32)iColon,pList);` |
|     7 | 2002 | `					ph7_context_release_value(pCtx,pList);` |
|     4 | 2003 | `				}else{` |
|     5 | 2004 | `					ph7_array_add_strkey_elem(pSlot,0,pVal);` |
|     - | 2005 | `				}` |
|     - | 2006 | `			}` |
|    67 | 2007 | `			ph7_value_reset_string_cursor(pVal);` |
|    33 | 2008 | `		}` |
|    73 | 2009 | `		SyBlobRelease(&sLine);` |
|    73 | 2010 | `		if( iNext <= 0 ){` |
|   ! 0 | 2011 | `			break;` |
|     - | 2012 | `		}` |
|    73 | 2013 | `		i += iNext;` |
|     1 | 2014 | `	}` |
|    45 | 2015 | `	ph7_context_release_value(pCtx,pVal);` |
|    45 | 2016 | `	if( err != ICV_OK ){` |
|     3 | 2017 | `		IcvShowError(pCtx,err,sCs.zName,sCs.nName,"???",3);` |
|     3 | 2018 | `		ph7_result_bool(pCtx,0);` |
|     2 | 2019 | `	}else{` |
|    43 | 2020 | `		ph7_result_value(pCtx,pArray);` |
|     - | 2021 | `	}` |
|    45 | 2022 | `	return PH7_OK;` |
|    24 | 2023 | `}` |
|     - | 2024 |  |
|   324 | 2025 | `PH7_PRIVATE int PH7_builtin_iconv_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv(pCtx,nArg,apArg); }` |
|    38 | 2026 | `PH7_PRIVATE int PH7_builtin_iconv_get_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_get_encoding(pCtx,nArg,apArg); }` |
|    79 | 2027 | `PH7_PRIVATE int PH7_builtin_iconv_mime_encode_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_encode(pCtx,nArg,apArg); }` |
|   162 | 2028 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_decode(pCtx,nArg,apArg); }` |
|    47 | 2029 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_headers_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_decode_headers(pCtx,nArg,apArg); }` |
|    40 | 2030 | `PH7_PRIVATE int PH7_builtin_iconv_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strlen(pCtx,nArg,apArg); }` |
|    54 | 2031 | `PH7_PRIVATE int PH7_builtin_iconv_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_substr(pCtx,nArg,apArg); }` |
|    52 | 2032 | `PH7_PRIVATE int PH7_builtin_iconv_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strpos(pCtx,nArg,apArg); }` |
|    25 | 2033 | `PH7_PRIVATE int PH7_builtin_iconv_strrpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strrpos(pCtx,nArg,apArg); }` |
|     - | 2034 |  |
|     - | 2035 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2036 |  |
