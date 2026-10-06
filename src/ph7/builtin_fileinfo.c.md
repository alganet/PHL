# src/ph7/builtin_fileinfo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1195/1606 lines (74.41%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#include <time.h>   /* gzip's header carries a modification time */` |
|     - |    7 | `/*` |
|     - |    8 | ` * Section:` |
|     - |    9 | ` *    php's fileinfo extension: what a program asks a file's TYPE with.` |
|     - |   10 | ` * Status:` |
|     - |   11 | ` *    Stable.` |
|     - |   12 | ` *` |
|     - |   13 | ` * php's ext/fileinfo is a shell over libmagic plus the magic DATABASE php` |
|     - |   14 | ` * bundles with it, so its contract has two halves that behave very differently` |
|     - |   15 | ` * and are worth keeping apart.` |
|     - |   16 | ` *` |
|     - |   17 | ` * The API half -- the finfo class, the six functions, the eleven flags, which` |
|     - |   18 | ` * argument each door screens and how -- is php's own, small, and reproduced` |
|     - |   19 | ` * whole ("The php layer" below).` |
|     - |   20 | ` *` |
|     - |   21 | ` * The ANSWER half is a database, and a database cannot be "ported": php ships` |
|     - |   22 | ` * ~3MB of compiled magic covering thousands of formats, most of which no php` |
|     - |   23 | ` * program has ever asked about. So this is the ext/bcmath and ext/gettext` |
|     - |   24 | ` * precedent again -- PHL carries its OWN signature set, derived from php 8.5.9's` |
|     - |   25 | ` * answers rather than from libmagic's source, and everything outside it is` |
|     - |   26 | ` * answered the way libmagic answers a file it does not recognise. What that` |
|     - |   27 | ` * buys is the same thing deriving gettext bought: the extension is here on every` |
|     - |   28 | ` * platform, it links nothing, and its answers do not move when the box's` |
|     - |   29 | ` * /usr/share/misc/magic does.` |
|     - |   30 | ` *` |
|     - |   31 | ` * WHAT IS COVERED, and how exactly:` |
|     - |   32 | ` *` |
|     - |   33 | ` *   - The TEXT analysis is complete and exact. It is the whole of libmagic's` |
|     - |   34 | ` *     encoding.c and ascmagic.c: which byte values count as text, the encoding` |
|     - |   35 | ` *     ladder (ascii, UTF-8 with and without a BOM, BOM'd UTF-16/UTF-32,` |
|     - |   36 | ` *     ISO-8859, non-ISO extended ASCII, then binary), the charset name each` |
|     - |   37 | `` *     answers `FILEINFO_MIME_ENCODING` with, and the four annotations a`` |
|     - |   38 | ` *     description carries -- the longest line when it passes 300, the line` |
|     - |   39 | ` *     terminators unless they are LF alone, an ESC anywhere, a BS anywhere.` |
|     - |   40 | ` *     This is the path MOST real files take, and every byte of it matches.` |
|     - |   41 | ` *   - The MIME TYPE and MIME ENCODING faces are exact for every format in the` |
|     - |   42 | ``  *     table below -- which is what a php program consumes: `mime_content_type()` `` |
|     - |   43 | `` *     and `FILEINFO_MIME_TYPE` are the two faces an upload validator, a mail`` |
|     - |   44 | ` *     builder or a static-file server reads.` |
|     - |   45 | ` *   - The DESCRIPTION face (FILEINFO_NONE) is php's exactly where the` |
|     - |   46 | ` *     description is a fixed string or a shallow parse -- text, the shebang` |
|     - |   47 | ` *     scripts, the markup and data formats, PNG, GIF, gzip, bzip2, xz, zstd,` |
|     - |   48 | ` *     the zip family, ELF, Java class, wasm, the fonts, ar, PDF's version. For` |
|     - |   49 | ` *     the formats whose description ends in a deep parse of the container --` |
|     - |   50 | ` *     JPEG past its JFIF header, TIFF's directory, ICO's icon list, an MP3's` |
|     - |   51 | ` *     frame chain, Ogg's codec, WAV's audio parameters, ISO Media's brand` |
|     - |   52 | ` *     list, a PDF's page count, RTF's code page, TrueType's table names --` |
|     - |   53 | ` *     PHL answers the HEAD of php's own sentence and stops where the deep` |
|     - |   54 | ` *     parse would begin. The type and the encoding are still exact there.` |
|     - |   55 | ` *` |
|     - |   56 | `` * WHAT IS NOT: a format outside the table is `data` /`` |
|     - |   57 | `` * `application/octet-stream` if it is binary and plain text if it is text,`` |
|     - |   58 | ` * which is what php answers for anything ITS database does not carry either --` |
|     - |   59 | `` * the difference is only where the two tables end. `finfo_open()` with a`` |
|     - |   60 | ` * $magic_database argument is refused with php's own "Failed to load magic` |
|     - |   61 | ` * database" diagnostic: the argument names a file in libmagic's format, and` |
|     - |   62 | ` * this engine has no reader for one (see FinfoOpenCommon).` |
|     - |   63 | ` *` |
|     - |   64 | ` * THE PHP LAYER, all of it measured against php 8.5.9:` |
|     - |   65 | ` *` |
|     - |   66 | ` *   - finfo is an ordinary, non-final, subclassable class with no properties,` |
|     - |   67 | ``  *     no constants, no clone handler and no serialize handler -- so `clone` `` |
|     - |   68 | ` *     is "Trying to clone an uncloneable object" and serialize() is` |
|     - |   69 | ` *     "Serialization of 'finfo' is not allowed". Its one slot is HIDDEN and` |
|     - |   70 | ` *     holds the flags; an object built by newInstanceWithoutConstructor() has` |
|     - |   71 | `` *     nothing in it and every verb answers php's `Invalid finfo object` Error.`` |
|     - |   72 | ` *   - The flags argument of file()/buffer() OVERRIDES the object's for that` |
|     - |   73 | ` *     call only, and only when it is non-zero: FILEINFO_NONE means "keep` |
|     - |   74 | `` *     mine", which is why `(new finfo(FILEINFO_MIME_TYPE))->file($p, 0)` still`` |
|     - |   75 | ` *     answers a mime type. php restores the object's flags afterwards.` |
|     - |   76 | ` *   - An empty $filename is a ValueError, and it names ARGUMENT #2 -- with the` |
|     - |   77 | ` *     name of whatever argument #2 is in the door it was raised through, so` |
|     - |   78 | `` *     the method form says `Argument #2 ($flags)` and the function form says`` |
|     - |   79 | `` *     `Argument #2 ($filename)`. That is php's own arginfo lookup showing`` |
|     - |   80 | ` *     through a check written for the procedural signature, and it is` |
|     - |   81 | ` *     reproduced rather than corrected. A NUL byte is the ordinary path` |
|     - |   82 | ` *     refusal and names argument #1.` |
|     - |   83 | ` *   - file() consults the STAT of the path before opening it: a directory is` |
|     - |   84 | `` *     the string `directory` whatever the flags say, mime face included. What`` |
|     - |   85 | `` *     it opens is a php STREAM, so `php://`, `data://` and a userland wrapper`` |
|     - |   86 | ` *     all work, and a path that will not open is php's` |
|     - |   87 | `` *     `%s(%s): Failed to open stream: %s` warning and false.`` |
|     - |   88 | ` *   - mime_content_type() takes a path OR an open stream, and a stream is read` |
|     - |   89 | ` *     from its BEGINNING and left exactly where it was found.` |
|     - |   90 | ` *   - finfo_close() is deprecated in 8.5 and does nothing but answer true.` |
|     - |   91 | ` */` |
|     - |   92 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |   93 |  |
|     - |   94 | `/* php's FILEINFO_* -- libmagic's MAGIC_* values, which php re-exports as-is. */` |
|     - |   95 | `#define FINFO_NONE            0x0000000` |
|     - |   96 | `#define FINFO_SYMLINK         0x0000002` |
|     - |   97 | `#define FINFO_DEVICES         0x0000008` |
|     - |   98 | `#define FINFO_MIME_TYPE       0x0000010` |
|     - |   99 | `#define FINFO_CONTINUE        0x0000020` |
|     - |  100 | `#define FINFO_PRESERVE_ATIME  0x0000080` |
|     - |  101 | `#define FINFO_RAW             0x0000100` |
|     - |  102 | `#define FINFO_MIME_ENCODING   0x0000400` |
|     - |  103 | `#define FINFO_MIME            (FINFO_MIME_TYPE\|FINFO_MIME_ENCODING)` |
|     - |  104 | `#define FINFO_APPLE           0x0000800` |
|     - |  105 | `#define FINFO_EXTENSION       0x1000000` |
|     - |  106 |  |
|     - |  107 | `/* The hidden slot holding an instance's flags. NULL there means "never` |
|     - |  108 | `` * constructed", which is php's `Invalid finfo object`. */`` |
|     - |  109 | `#define FINFO_FLAGS_SLOT "__flags"` |
|     - |  110 |  |
|     - |  111 | `/* How much of a file the analysis looks at. libmagic's own default parameter` |
|     - |  112 | ` * (MAGIC_PARAM_BYTES_MAX) is 1MB and php does not change it, so a text file` |
|     - |  113 | ` * whose first megabyte is clean reads as text under both. */` |
|     - |  114 | `#define FINFO_READ_MAX 1048576` |
|     - |  115 |  |
|     - |  116 | `/*` |
|     - |  117 | ` * ---------------------------------------------------------------------------` |
|     - |  118 | ` * The TEXT analysis (libmagic's encoding.c + ascmagic.c)` |
|     - |  119 | ` * ---------------------------------------------------------------------------` |
|     - |  120 | `` * Every answer passes through here, not just the ones that end up as `ASCII`` |
|     - |  121 | `` * text`: the MIME ENCODING face is this and nothing else, so a PDF whose bytes`` |
|     - |  122 | `` * happen to be ASCII is `application/pdf; charset=us-ascii` while one carrying`` |
|     - |  123 | `` * a compressed stream is `charset=binary`.`` |
|     - |  124 | ` */` |
|     - |  125 | `#define FINFO_ENC_BINARY    0` |
|     - |  126 | `#define FINFO_ENC_ASCII     1` |
|     - |  127 | `#define FINFO_ENC_UTF8      2` |
|     - |  128 | `#define FINFO_ENC_UTF8_BOM  3` |
|     - |  129 | `#define FINFO_ENC_UTF16LE   4` |
|     - |  130 | `#define FINFO_ENC_UTF16BE   5` |
|     - |  131 | `#define FINFO_ENC_UTF32LE   6` |
|     - |  132 | `#define FINFO_ENC_UTF32BE   7` |
|     - |  133 | `#define FINFO_ENC_LATIN1    8` |
|     - |  134 | `#define FINFO_ENC_EXTENDED  9` |
|     - |  135 |  |
|     - |  136 | `typedef struct FinfoText FinfoText;` |
|     - |  137 | `struct FinfoText {` |
|     - |  138 | `	int iEnc;         /* FINFO_ENC_* */` |
|     - |  139 | `	int bCRLF;        /* saw a CR LF pair */` |
|     - |  140 | `	int bCR;          /* saw a CR that was not part of one */` |
|     - |  141 | `	int bLF;          /* saw an LF that was not part of one */` |
|     - |  142 | `	int bNEL;         /* saw U+0085, the Unicode NEL */` |
|     - |  143 | `	int bEscape;      /* saw ESC */` |
|     - |  144 | `	int bOverstrike;  /* saw BS */` |
|     - |  145 | `	sxu32 nLongest;   /* the longest line, terminator excluded */` |
|     - |  146 | `};` |
|     - |  147 |  |
|     - |  148 | `/*` |
|     - |  149 | ` * libmagic's text_chars[]: which byte values may appear in a text file. T is` |
|     - |  150 | ` * text in every encoding, I is text in ISO-8859 (a byte in the 0xA0..0xFF` |
|     - |  151 | ` * range), X is text only in the "non-ISO extended ASCII" reading, and 0 is` |
|     - |  152 | ` * never text -- which is what makes a NUL, a 0x7F DEL or a 0x1A anywhere turn` |
|     - |  153 | `` * the whole file into `data`.`` |
|     - |  154 | ` */` |
|     - |  155 | `#define FINFO_CH_NONE 0` |
|     - |  156 | `#define FINFO_CH_T    1` |
|     - |  157 | `#define FINFO_CH_I    2` |
|     - |  158 | `#define FINFO_CH_X    3` |
|     - |  159 | `static const sxu8 aFinfoTextChar[256] = {` |
|     - |  160 | `	/*         BEL BS  HT  LF  VT  FF  CR                */` |
|     - |  161 | `	0,0,0,0,0,0,0,1, 1,1,1,1,1,1,0,0,   /* 0x00 */` |
|     - |  162 | `	/*                     ESC                           */` |
|     - |  163 | `	0,0,0,0,0,0,0,0, 0,0,0,1,0,0,0,0,   /* 0x10 */` |
|     - |  164 | `	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,   /* 0x20 */` |
|     - |  165 | `	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,   /* 0x30 */` |
|     - |  166 | `	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,   /* 0x40 */` |
|     - |  167 | `	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,   /* 0x50 */` |
|     - |  168 | `	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,   /* 0x60 */` |
|     - |  169 | `	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,0,   /* 0x70 -- 0x7F DEL is not text */` |
|     - |  170 | `	3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3,   /* 0x80 */` |
|     - |  171 | `	3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3,   /* 0x90 */` |
|     - |  172 | `	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,   /* 0xA0 */` |
|     - |  173 | `	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,   /* 0xB0 */` |
|     - |  174 | `	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,   /* 0xC0 */` |
|     - |  175 | `	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,   /* 0xD0 */` |
|     - |  176 | `	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,   /* 0xE0 */` |
|     - |  177 | `	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2    /* 0xF0 */` |
|     - |  178 | `};` |
|     - |  179 |  |
|   658 |  180 | `static int FinfoLooksAscii(const unsigned char *z,sxu32 n)` |
|     3 |  181 | `{` |
|     - |  182 | `	sxu32 i;` |
| 20265 |  183 | `	for( i = 0 ; i < n ; ++i ){` |
| 19851 |  184 | `		if( aFinfoTextChar[z[i]] != FINFO_CH_T ){` |
|   246 |  185 | `			return 0;` |
|     - |  186 | `		}` |
|  9805 |  187 | `	}` |
|   417 |  188 | `	return 1;` |
|   332 |  189 | `}` |
|   202 |  190 | `static int FinfoLooksLatin1(const unsigned char *z,sxu32 n)` |
|     2 |  191 | `{` |
|     - |  192 | `	sxu32 i;` |
|   920 |  193 | `	for( i = 0 ; i < n ; ++i ){` |
|   914 |  194 | `		sxu8 c = aFinfoTextChar[z[i]];` |
|   914 |  195 | `		if( c != FINFO_CH_T && c != FINFO_CH_I ){` |
|   198 |  196 | `			return 0;` |
|     - |  197 | `		}` |
|   360 |  198 | `	}` |
|     7 |  199 | `	return 1;` |
|   103 |  200 | `}` |
|   196 |  201 | `static int FinfoLooksExtended(const unsigned char *z,sxu32 n)` |
|     2 |  202 | `{` |
|     - |  203 | `	sxu32 i;` |
|   986 |  204 | `	for( i = 0 ; i < n ; ++i ){` |
|   980 |  205 | `		if( aFinfoTextChar[z[i]] == FINFO_CH_NONE ){` |
|   192 |  206 | `			return 0;` |
|     - |  207 | `		}` |
|   396 |  208 | `	}` |
|     7 |  209 | `	return 1;` |
|   100 |  210 | `}` |
|     - |  211 | `/*` |
|     - |  212 | ` * A valid UTF-8 sequence, with libmagic's own screens: an overlong form, a` |
|     - |  213 | ` * surrogate, a code point past U+10FFFF and a truncated tail at the END of the` |
|     - |  214 | ` * buffer are all refusals (the last one included -- the buffer is what the` |
|     - |  215 | ` * caller has, and libmagic does not assume more is coming). Answers how many` |
|     - |  216 | ` * MULTIBYTE sequences were seen, so a pure-ASCII buffer scores 0 and loses to` |
|     - |  217 | ` * the ascii test that ran before it.` |
|     - |  218 | ` */` |
|   244 |  219 | `static int FinfoLooksUtf8(const unsigned char *z,sxu32 n,sxu32 *pnMulti)` |
|     2 |  220 | `{` |
|   246 |  221 | `	sxu32 i,nMulti = 0;` |
|   904 |  222 | `	for( i = 0 ; i < n ; ){` |
|   880 |  223 | `		unsigned int c = z[i];` |
|     - |  224 | `		int nFollow;` |
|     - |  225 | `		unsigned int uCode;` |
|   880 |  226 | `		if( c < 0x80 ){` |
|   782 |  227 | `			if( aFinfoTextChar[c] == FINFO_CH_NONE ){` |
|   136 |  228 | `				return 0;` |
|     - |  229 | `			}` |
|   648 |  230 | `			i++;` |
|   648 |  231 | `			continue;` |
|     - |  232 | `		}` |
|    99 |  233 | `		if( (c & 0xE0) == 0xC0 ){` |
|    19 |  234 | `			nFollow = 1; uCode = c & 0x1F;` |
|    90 |  235 | `		}else if( (c & 0xF0) == 0xE0 ){` |
|     7 |  236 | `			nFollow = 2; uCode = c & 0x0F;` |
|    78 |  237 | `		}else if( (c & 0xF8) == 0xF0 ){` |
|   ! 0 |  238 | `			nFollow = 3; uCode = c & 0x07;` |
|   ! 0 |  239 | `		}else{` |
|    75 |  240 | `			return 0;` |
|     - |  241 | `		}` |
|    25 |  242 | `		if( i + (sxu32)nFollow >= n ){` |
|   ! 0 |  243 | `			return 0;   /* a tail the buffer does not carry */` |
|     - |  244 | `		}` |
|     - |  245 | `		{` |
|     - |  246 | `			int k;` |
|    37 |  247 | `			for( k = 1 ; k <= nFollow ; ++k ){` |
|    25 |  248 | `				if( (z[i+(sxu32)k] & 0xC0) != 0x80 ){` |
|    13 |  249 | `					return 0;` |
|     - |  250 | `				}` |
|    13 |  251 | `				uCode = (uCode << 6) \| (unsigned int)(z[i+(sxu32)k] & 0x3F);` |
|     7 |  252 | `			}` |
|     - |  253 | `		}` |
|    12 |  254 | `		if( (nFollow == 1 && uCode < 0x80)` |
|    12 |  255 | `		 \|\| (nFollow == 2 && uCode < 0x800)` |
|    12 |  256 | `		 \|\| (nFollow == 3 && uCode < 0x10000)` |
|    12 |  257 | `		 \|\| uCode > 0x10FFFF` |
|    13 |  258 | `		 \|\| (uCode >= 0xD800 && uCode <= 0xDFFF) ){` |
|   ! 0 |  259 | `			return 0;` |
|     - |  260 | `		}` |
|    13 |  261 | `		i += (sxu32)nFollow + 1;` |
|    13 |  262 | `		nMulti++;` |
|     1 |  263 | `	}` |
|    25 |  264 | `	if( pnMulti ){` |
|    25 |  265 | `		*pnMulti = nMulti;` |
|    12 |  266 | `	}` |
|    25 |  267 | `	return 1;` |
|   124 |  268 | `}` |
|     - |  269 | `/*` |
|     - |  270 | `` * The line-shape half: what the annotations after `ASCII text` are made of.`` |
|     - |  271 | ` * Runs over CODE POINTS rather than bytes so a BOM'd UTF-16 file reports its` |
|     - |  272 | ` * own terminators -- which is the only reason the NEL (U+0085) counter is here` |
|     - |  273 | ` * at all, since libmagic only ever sees one in a wide encoding or as UTF-8.` |
|     - |  274 | ` */` |
| 19264 |  275 | `static void FinfoLineShape(FinfoText *pTx,unsigned int c,unsigned int cNext,int *pbSkip,sxu32 *pnLine)` |
|     3 |  276 | `{` |
| 19267 |  277 | `	if( c == 0x0D ){` |
|    43 |  278 | `		if( cNext == 0x0A ){` |
|    25 |  279 | `			pTx->bCRLF = 1;` |
|    25 |  280 | `			*pbSkip = 1;` |
|    13 |  281 | `		}else{` |
|    19 |  282 | `			pTx->bCR = 1;` |
|     - |  283 | `		}` |
|    43 |  284 | `		if( *pnLine > pTx->nLongest ){` |
|    25 |  285 | `			pTx->nLongest = *pnLine;` |
|    12 |  286 | `		}` |
|    43 |  287 | `		*pnLine = 0;` |
|    43 |  288 | `		return;` |
|     - |  289 | `	}` |
| 19225 |  290 | `	if( c == 0x0A ){` |
|   658 |  291 | `		pTx->bLF = 1;` |
|   658 |  292 | `		if( *pnLine > pTx->nLongest ){` |
|   370 |  293 | `			pTx->nLongest = *pnLine;` |
|   184 |  294 | `		}` |
|   658 |  295 | `		*pnLine = 0;` |
|   658 |  296 | `		return;` |
|     - |  297 | `	}` |
| 18569 |  298 | `	if( c == 0x85 ){` |
|     7 |  299 | `		pTx->bNEL = 1;` |
|     7 |  300 | `		if( *pnLine > pTx->nLongest ){` |
|     7 |  301 | `			pTx->nLongest = *pnLine;` |
|     3 |  302 | `		}` |
|     7 |  303 | `		*pnLine = 0;` |
|     7 |  304 | `		return;` |
|     - |  305 | `	}` |
| 18563 |  306 | `	if( c == 0x1B ){` |
|    13 |  307 | `		pTx->bEscape = 1;` |
| 18557 |  308 | `	}else if( c == 0x08 ){` |
|    13 |  309 | `		pTx->bOverstrike = 1;` |
|     6 |  310 | `	}` |
| 18563 |  311 | `	(*pnLine)++;` |
|  9635 |  312 | `}` |
|     - |  313 | `/*` |
|     - |  314 | ` * The whole text reading of a buffer: the encoding ladder first, then the line` |
|     - |  315 | ` * shape over whatever code points that encoding gives.` |
|     - |  316 | ` */` |
|   668 |  317 | `static void FinfoTextScan(const unsigned char *z,sxu32 n,FinfoText *pTx)` |
|     3 |  318 | `{` |
|   671 |  319 | `	sxu32 i,nLine = 0,nMulti = 0;` |
|   671 |  320 | `	int bWide = 0,bSwap = 0,nUnit = 0;` |
|   671 |  321 | `	SyZero(pTx,sizeof(*pTx));` |
|   671 |  322 | `	pTx->iEnc = FINFO_ENC_BINARY;` |
|   671 |  323 | `	if( n < 1 ){` |
|    12 |  324 | `		pTx->iEnc = FINFO_ENC_ASCII;` |
|   107 |  325 | `		return;` |
|     - |  326 | `	}` |
|   661 |  327 | `	if( FinfoLooksAscii(z,n) ){` |
|   417 |  328 | `		pTx->iEnc = FINFO_ENC_ASCII;` |
|   454 |  329 | `	}else if( n > 3 && z[0] == 0xEF && z[1] == 0xBB && z[2] == 0xBF` |
|    14 |  330 | `	       && FinfoLooksUtf8(z+3,n-3,&nMulti) ){` |
|    13 |  331 | `		pTx->iEnc = FINFO_ENC_UTF8_BOM;` |
|   240 |  332 | `	}else if( FinfoLooksUtf8(z,n,&nMulti) && nMulti > 0 ){` |
|    13 |  333 | `		pTx->iEnc = FINFO_ENC_UTF8;` |
|   228 |  334 | `	}else if( n >= 4 && z[0] == 0xFF && z[1] == 0xFE && z[2] == 0x00 && z[3] == 0x00 ){` |
|     7 |  335 | `		pTx->iEnc = FINFO_ENC_UTF32LE; bWide = 1; nUnit = 4; bSwap = 0;` |
|   219 |  336 | `	}else if( n >= 4 && z[0] == 0x00 && z[1] == 0x00 && z[2] == 0xFE && z[3] == 0xFF ){` |
|   ! 0 |  337 | `		pTx->iEnc = FINFO_ENC_UTF32BE; bWide = 1; nUnit = 4; bSwap = 1;` |
|   216 |  338 | `	}else if( n >= 2 && z[0] == 0xFF && z[1] == 0xFE ){` |
|     7 |  339 | `		pTx->iEnc = FINFO_ENC_UTF16LE; bWide = 1; nUnit = 2; bSwap = 0;` |
|   213 |  340 | `	}else if( n >= 2 && z[0] == 0xFE && z[1] == 0xFF ){` |
|     7 |  341 | `		pTx->iEnc = FINFO_ENC_UTF16BE; bWide = 1; nUnit = 2; bSwap = 1;` |
|   207 |  342 | `	}else if( FinfoLooksLatin1(z,n) ){` |
|     7 |  343 | `		pTx->iEnc = FINFO_ENC_LATIN1;` |
|   201 |  344 | `	}else if( FinfoLooksExtended(z,n) ){` |
|     7 |  345 | `		pTx->iEnc = FINFO_ENC_EXTENDED;` |
|     4 |  346 | `	}else{` |
|   192 |  347 | `		return;   /* binary: nothing else is measured */` |
|     - |  348 | `	}` |
|   471 |  349 | `	if( bWide ){` |
|     - |  350 | `		/* The BOM itself is not part of the text. A wide encoding's own` |
|     - |  351 | `		 * annotations are measured over its code points, which is why` |
|     - |  352 | `		 * "\xff\xfeh\0i\0" answers "with no line terminators" rather than` |
|     - |  353 | `		 * finding the NULs interesting. */` |
|    49 |  354 | `		for( i = (sxu32)nUnit ; i + (sxu32)nUnit <= n ; i += (sxu32)nUnit ){` |
|    31 |  355 | `			unsigned int c = 0,cNext = 0;` |
|    31 |  356 | `			int bSkip = 0,k;` |
|   103 |  357 | `			for( k = 0 ; k < nUnit ; ++k ){` |
|    73 |  358 | `				unsigned int b = z[i + (sxu32)(bSwap ? k : nUnit - 1 - k)];` |
|    73 |  359 | `				c = (c << 8) \| b;` |
|    37 |  360 | `			}` |
|    31 |  361 | `			if( i + (sxu32)(2*nUnit) <= n ){` |
|    37 |  362 | `				for( k = 0 ; k < nUnit ; ++k ){` |
|    25 |  363 | `					unsigned int b = z[i + (sxu32)nUnit + (sxu32)(bSwap ? k : nUnit - 1 - k)];` |
|    25 |  364 | `					cNext = (cNext << 8) \| b;` |
|    13 |  365 | `				}` |
|     6 |  366 | `			}` |
|    31 |  367 | `			FinfoLineShape(pTx,c,cNext,&bSkip,&nLine);` |
|    31 |  368 | `			if( bSkip ){` |
|   ! 0 |  369 | `				i += (sxu32)nUnit;` |
|   ! 0 |  370 | `			}` |
|    16 |  371 | `		}` |
|    10 |  372 | `	}else{` |
|   453 |  373 | `		sxu32 nStart = (pTx->iEnc == FINFO_ENC_UTF8_BOM) ? 3 : 0;` |
| 19699 |  374 | `		for( i = nStart ; i < n ; ++i ){` |
| 19249 |  375 | `			unsigned int c = z[i];` |
| 19249 |  376 | `			int bSkip = 0;` |
|     - |  377 | `			/* U+0085 reaches an 8-bit scan as the two bytes C2 85 in UTF-8, and` |
|     - |  378 | `			 * as the single byte 0x85 in a Latin-1 file -- which libmagic does` |
|     - |  379 | `			 * NOT count, since 0x85 is an ordinary high byte there. */` |
| 19246 |  380 | `			if( (pTx->iEnc == FINFO_ENC_UTF8 \|\| pTx->iEnc == FINFO_ENC_UTF8_BOM)` |
|  9689 |  381 | `			 && c == 0xC2 && i + 1 < n && z[i+1] == 0x85 ){` |
|     7 |  382 | `				FinfoLineShape(pTx,0x85,0,&bSkip,&nLine);` |
|     7 |  383 | `				i++;` |
|    13 |  384 | `				continue;` |
|     - |  385 | `			}` |
| 19243 |  386 | `			if( c >= 0x80 && (pTx->iEnc == FINFO_ENC_UTF8 \|\| pTx->iEnc == FINFO_ENC_UTF8_BOM) ){` |
|     - |  387 | `				/* A multibyte character is ONE column, and never a terminator. */` |
|    13 |  388 | `				if( (c & 0xC0) != 0x80 ){` |
|     7 |  389 | `					nLine++;` |
|     3 |  390 | `				}` |
|    13 |  391 | `				continue;` |
|     - |  392 | `			}` |
| 19231 |  393 | `			FinfoLineShape(pTx,c,(i + 1 < n) ? z[i+1] : 0,&bSkip,&nLine);` |
| 19231 |  394 | `			if( bSkip ){` |
|    25 |  395 | `				i++;` |
|    12 |  396 | `			}` |
|  9617 |  397 | `		}` |
|     - |  398 | `	}` |
|   471 |  399 | `	if( nLine > pTx->nLongest ){` |
|    95 |  400 | `		pTx->nLongest = nLine;` |
|    46 |  401 | `	}` |
|   337 |  402 | `}` |
|     - |  403 | `/* The charset name FILEINFO_MIME_ENCODING answers with. */` |
|   434 |  404 | `static const char * FinfoCharset(int iEnc)` |
|     2 |  405 | `{` |
|   436 |  406 | `	switch( iEnc ){` |
|   256 |  407 | `		case FINFO_ENC_ASCII:    return "us-ascii";` |
|     8 |  408 | `		case FINFO_ENC_UTF8:` |
|    17 |  409 | `		case FINFO_ENC_UTF8_BOM: return "utf-8";` |
|     5 |  410 | `		case FINFO_ENC_UTF16LE:  return "utf-16le";` |
|     5 |  411 | `		case FINFO_ENC_UTF16BE:  return "utf-16be";` |
|     5 |  412 | `		case FINFO_ENC_UTF32LE:  return "utf-32le";` |
|   ! 0 |  413 | `		case FINFO_ENC_UTF32BE:  return "utf-32be";` |
|     5 |  414 | `		case FINFO_ENC_LATIN1:   return "iso-8859-1";` |
|     5 |  415 | `		case FINFO_ENC_EXTENDED: return "unknown-8bit";` |
|   144 |  416 | `		default:                 break;` |
|     - |  417 | `	}` |
|   145 |  418 | `	return "binary";` |
|   219 |  419 | `}` |
|     - |  420 | `/* The head of a text description: the encoding's own name for itself. */` |
|   838 |  421 | `static const char * FinfoEncDesc(int iEnc)` |
|     3 |  422 | `{` |
|   841 |  423 | `	switch( iEnc ){` |
|   733 |  424 | `		case FINFO_ENC_ASCII:    return "ASCII text";` |
|    25 |  425 | `		case FINFO_ENC_UTF8:     return "Unicode text, UTF-8 text";` |
|    25 |  426 | `		case FINFO_ENC_UTF8_BOM: return "Unicode text, UTF-8 (with BOM) text";` |
|    13 |  427 | `		case FINFO_ENC_UTF16LE:  return "Unicode text, UTF-16, little-endian text";` |
|    13 |  428 | `		case FINFO_ENC_UTF16BE:  return "Unicode text, UTF-16, big-endian text";` |
|     - |  429 | ``		/* The two UCS-4 readings end there: libmagic prints no ` text` and no`` |
|     - |  430 | `		 * annotations after them. */` |
|    13 |  431 | `		case FINFO_ENC_UTF32LE:  return "Unicode text, UTF-32, little-endian";` |
|   ! 0 |  432 | `		case FINFO_ENC_UTF32BE:  return "Unicode text, UTF-32, big-endian";` |
|    13 |  433 | `		case FINFO_ENC_LATIN1:   return "ISO-8859 text";` |
|    13 |  434 | `		case FINFO_ENC_EXTENDED: return "Non-ISO extended-ASCII text";` |
|   ! 0 |  435 | `		default:                 break;` |
|     - |  436 | `	}` |
|   ! 0 |  437 | `	return "data";` |
|   422 |  438 | `}` |
|     - |  439 | `/*` |
|     - |  440 | ` * The annotations, in libmagic's order: the long line, then the terminators,` |
|     - |  441 | ` * then the escapes, then the overstriking. A file whose only terminator is LF` |
|     - |  442 | ` * says nothing at all -- that is the ordinary case and libmagic treats it as` |
|     - |  443 | ` * the default rather than as news.` |
|     - |  444 | ` */` |
|   416 |  445 | `static void FinfoTextDesc(const FinfoText *pTx,SyBlob *pOut)` |
|     3 |  446 | `{` |
|   419 |  447 | `	SyBlobAppend(pOut,FinfoEncDesc(pTx->iEnc),SyStrlen(FinfoEncDesc(pTx->iEnc)));` |
|   419 |  448 | `	if( pTx->iEnc == FINFO_ENC_UTF32LE \|\| pTx->iEnc == FINFO_ENC_UTF32BE ){` |
|     7 |  449 | `		return;` |
|     - |  450 | `	}` |
|   413 |  451 | `	if( pTx->nLongest > 300 ){` |
|    19 |  452 | `		SyBlobFormat(pOut,", with very long lines (%u)",pTx->nLongest);` |
|     9 |  453 | `	}` |
|   413 |  454 | `	if( !pTx->bCRLF && !pTx->bCR && !pTx->bLF && !pTx->bNEL ){` |
|    73 |  455 | `		SyBlobAppend(pOut,", with no line terminators",sizeof(", with no line terminators")-1);` |
|   377 |  456 | `	}else if( pTx->bCRLF \|\| pTx->bCR \|\| pTx->bNEL ){` |
|    37 |  457 | `		int bFirst = 1;` |
|    37 |  458 | `		SyBlobAppend(pOut,", with",sizeof(", with")-1);` |
|    37 |  459 | `		if( pTx->bCRLF ){` |
|    19 |  460 | `			SyBlobAppend(pOut," CRLF",5); bFirst = 0;` |
|     9 |  461 | `		}` |
|    37 |  462 | `		if( pTx->bCR ){` |
|    13 |  463 | `			SyBlobAppend(pOut,bFirst ? " CR" : ", CR",bFirst ? 3 : 4); bFirst = 0;` |
|     6 |  464 | `		}` |
|    37 |  465 | `		if( pTx->bLF ){` |
|    13 |  466 | `			SyBlobAppend(pOut,bFirst ? " LF" : ", LF",bFirst ? 3 : 4); bFirst = 0;` |
|     6 |  467 | `		}` |
|    37 |  468 | `		if( pTx->bNEL ){` |
|     7 |  469 | `			SyBlobAppend(pOut,bFirst ? " NEL" : ", NEL",bFirst ? 4 : 5);` |
|     3 |  470 | `		}` |
|    37 |  471 | `		SyBlobAppend(pOut," line terminators",sizeof(" line terminators")-1);` |
|    18 |  472 | `	}` |
|   413 |  473 | `	if( pTx->bEscape ){` |
|    13 |  474 | `		SyBlobAppend(pOut,", with escape sequences",sizeof(", with escape sequences")-1);` |
|     6 |  475 | `	}` |
|   413 |  476 | `	if( pTx->bOverstrike ){` |
|    13 |  477 | `		SyBlobAppend(pOut,", with overstriking",sizeof(", with overstriking")-1);` |
|     6 |  478 | `	}` |
|   211 |  479 | `}` |
|     - |  480 | `/*` |
|     - |  481 | ` * ---------------------------------------------------------------------------` |
|     - |  482 | ` * The signature set` |
|     - |  483 | ` * ---------------------------------------------------------------------------` |
|     - |  484 | ` * One answer, three faces. zMime and zExt are what the MIME TYPE and EXTENSION` |
|     - |  485 | ` * flags hand back; the description is built in sDesc, and bText asks for the` |
|     - |  486 | ``  * text analysis to be appended to it (which is how `HTML document, ASCII text` `` |
|     - |  487 | `` * and `PHP script, Unicode text, UTF-8 text` are made). bExec adds libmagic's`` |
|     - |  488 | `` * ` executable` after that, which only a #! line asks for.`` |
|     - |  489 | ` */` |
|     - |  490 | `typedef struct FinfoAnswer FinfoAnswer;` |
|     - |  491 | `struct FinfoAnswer {` |
|     - |  492 | `	SyBlob sDesc;` |
|     - |  493 | `	const char *zMime;` |
|     - |  494 | `	const char *zExt;` |
|     - |  495 | `	int bText;   /* 1: append ", " and the text analysis. 2: append it directly. 3:` |
|     - |  496 | `	              * append the ENCODING NAME alone, with none of the annotations` |
|     - |  497 | `	              * after it -- libmagic's CSV row is the only one that does, and` |
|     - |  498 | `	              * a CSV file with CRLF endings is where it shows. Otherwise 2 --` |
|     - |  499 | `	              * the row's own prefix already ends in the separator libmagic's` |
|     - |  500 | ``	              * entry carries (`CSV ` has a space where `PHP script, ` has a`` |
|     - |  501 | `	              * comma), which is why this is not a boolean. */` |
|     - |  502 | `	int bExec;` |
|     - |  503 | `};` |
|     - |  504 |  |
|    53 |  505 | `static sxu32 FinfoBe16(const unsigned char *z){ return ((sxu32)z[0]<<8)\|(sxu32)z[1]; }` |
|    80 |  506 | `static sxu32 FinfoLe16(const unsigned char *z){ return ((sxu32)z[1]<<8)\|(sxu32)z[0]; }` |
|    78 |  507 | `static sxu32 FinfoBe32(const unsigned char *z)` |
|     1 |  508 | `{` |
|    79 |  509 | `	return ((sxu32)z[0]<<24)\|((sxu32)z[1]<<16)\|((sxu32)z[2]<<8)\|(sxu32)z[3];` |
|     1 |  510 | `}` |
|    68 |  511 | `static sxu32 FinfoLe32(const unsigned char *z)` |
|     1 |  512 | `{` |
|    69 |  513 | `	return ((sxu32)z[3]<<24)\|((sxu32)z[2]<<16)\|((sxu32)z[1]<<8)\|(sxu32)z[0];` |
|     1 |  514 | `}` |
|     - |  515 | `/* Does the buffer carry this literal at this offset? */` |
| 20488 |  516 | `static int FinfoAt(const unsigned char *z,sxu32 n,sxu32 nOfft,const char *zLit,sxu32 nLit)` |
|     3 |  517 | `{` |
|     - |  518 | `	/* Subtraction rather than addition: some of these offsets are read OUT of` |
|     - |  519 | `	 * the file being examined (a PE header pointer, an EBML element), and` |
|     - |  520 | ``	 * `nOfft + nLit` wraps for a crafted one. */`` |
| 20491 |  521 | `	if( nOfft > n \|\| nLit > n - nOfft ){` |
|  2493 |  522 | `		return 0;` |
|     - |  523 | `	}` |
| 18001 |  524 | `	return SyMemcmp(z + nOfft,zLit,nLit) == 0;` |
| 10247 |  525 | `}` |
|     - |  526 | `#define FINFO_AT(z,n,o,lit) FinfoAt(z,n,(sxu32)(o),lit,(sxu32)(sizeof(lit)-1))` |
|     - |  527 | `/* Where this literal first appears within the first nLimit bytes, or -1. */` |
|   958 |  528 | `static sxi32 FinfoFind(const unsigned char *z,sxu32 n,sxu32 nLimit,const char *zLit,sxu32 nLit)` |
|     3 |  529 | `{` |
|     - |  530 | `	sxu32 i;` |
|   961 |  531 | `	if( nLimit > n ){` |
|   955 |  532 | `		nLimit = n;` |
|   476 |  533 | `	}` |
|   961 |  534 | `	if( nLit < 1 \|\| nLit > nLimit ){` |
|   423 |  535 | `		return -1;` |
|     - |  536 | `	}` |
| 51859 |  537 | `	for( i = 0 ; i + nLit <= nLimit ; ++i ){` |
| 51373 |  538 | `		if( z[i] == (unsigned char)zLit[0] && SyMemcmp(z + i,zLit,nLit) == 0 ){` |
|    53 |  539 | `			return (sxi32)i;` |
|     - |  540 | `		}` |
| 25662 |  541 | `	}` |
|   489 |  542 | `	return -1;` |
|   482 |  543 | `}` |
|     - |  544 | `#define FINFO_FIND(z,n,lim,lit) FinfoFind(z,n,(sxu32)(lim),lit,(sxu32)(sizeof(lit)-1))` |
|     - |  545 | `/* Case-insensitive (ASCII) compare of a literal at an offset. */` |
|  2318 |  546 | `static int FinfoAtNoCase(const unsigned char *z,sxu32 n,sxu32 nOfft,const char *zLit,sxu32 nLit)` |
|     3 |  547 | `{` |
|     - |  548 | `	sxu32 i;` |
|  2321 |  549 | `	if( nOfft > n \|\| nLit > n - nOfft ){` |
|   645 |  550 | `		return 0;` |
|     - |  551 | `	}` |
|  1851 |  552 | `	for( i = 0 ; i < nLit ; ++i ){` |
|  1825 |  553 | `		if( SyToLower(z[nOfft+i]) != SyToLower((unsigned char)zLit[i]) ){` |
|  1653 |  554 | `			return 0;` |
|     - |  555 | `		}` |
|    88 |  556 | `	}` |
|    28 |  557 | `	return 1;` |
|  1162 |  558 | `}` |
|     - |  559 | `#define FINFO_ATI(z,n,o,lit) FinfoAtNoCase(z,n,(sxu32)(o),lit,(sxu32)(sizeof(lit)-1))` |
|   630 |  560 | `static void FinfoSay(FinfoAnswer *pOut,const char *zText)` |
|     2 |  561 | `{` |
|   632 |  562 | `	SyBlobAppend(&pOut->sDesc,zText,SyStrlen(zText));` |
|   632 |  563 | `}` |
|     - |  564 | `/*` |
|     - |  565 | ` * ELF: what the header says about itself. php's answer stops at the version and` |
|     - |  566 | `` * the OS ABI -- the note-walking that gives file(1) its `dynamically linked,`` |
|     - |  567 | `` * interpreter ..., BuildID[sha1]=...` tail is not in php's bundled database, so`` |
|     - |  568 | ` * a whole ELF answer is these five fields and nothing more.` |
|     - |  569 | ` */` |
|   484 |  570 | `static int FinfoElf(const unsigned char *z,sxu32 n,ph7_int64 nFile,FinfoAnswer *pOut)` |
|     3 |  571 | `{` |
|     - |  572 | `	int bClass64,bLsb;` |
|     - |  573 | `	sxu32 nType,nMachine,nVersion,nOsabi;` |
|   487 |  574 | `	const char *zMachine = 0;` |
|   487 |  575 | `	int bInterp = 0;` |
|   487 |  576 | `	if( n < 20 \|\| !FINFO_AT(z,n,0,"\177ELF") ){` |
|   487 |  577 | `		return 0;` |
|     - |  578 | `	}` |
|   ! 0 |  579 | `	bClass64 = (z[4] == 2);` |
|   ! 0 |  580 | `	bLsb = (z[5] == 1);` |
|   ! 0 |  581 | `	nOsabi = z[7];` |
|   ! 0 |  582 | `	nType = bLsb ? FinfoLe16(z+16) : FinfoBe16(z+16);` |
|   ! 0 |  583 | `	nMachine = bLsb ? FinfoLe16(z+18) : FinfoBe16(z+18);` |
|   ! 0 |  584 | `	nVersion = (n >= 24) ? (bLsb ? FinfoLe32(z+20) : FinfoBe32(z+20)) : 1;` |
|   ! 0 |  585 | `	SyBlobFormat(&pOut->sDesc,"ELF %s-bit %sSB ",bClass64 ? "64" : "32",bLsb ? "L" : "M");` |
|     - |  586 | `	/* An ET_DYN with a PT_INTERP is what a modern toolchain builds an ordinary` |
|     - |  587 | ``	 * program as, and php calls THAT a `pie executable` -- a shared library,`` |
|     - |  588 | ``	 * which has the same object type and no interpreter, stays a `shared`` |
|     - |  589 | ``	 * object`. Walking the program headers is the only way to tell them apart,`` |
|     - |  590 | `	 * and php only does it for a FILE: the same bytes handed to buffer() are a` |
|     - |  591 | ``	 * `shared object` there, which is measured rather than assumed. */`` |
|   ! 0 |  592 | `	if( nType == 3 && n > 64 && nFile >= 0 ){` |
|     - |  593 | `		sxu32 nPhOff,nPhEntSize,nPhNum,i;` |
|   ! 0 |  594 | `		if( bClass64 ){` |
|   ! 0 |  595 | `			nPhOff = bLsb ? FinfoLe32(z+32) : FinfoBe32(z+36);` |
|   ! 0 |  596 | `			nPhEntSize = bLsb ? FinfoLe16(z+54) : FinfoBe16(z+54);` |
|   ! 0 |  597 | `			nPhNum = bLsb ? FinfoLe16(z+56) : FinfoBe16(z+56);` |
|   ! 0 |  598 | `		}else{` |
|   ! 0 |  599 | `			nPhOff = bLsb ? FinfoLe32(z+28) : FinfoBe32(z+28);` |
|   ! 0 |  600 | `			nPhEntSize = bLsb ? FinfoLe16(z+42) : FinfoBe16(z+42);` |
|   ! 0 |  601 | `			nPhNum = bLsb ? FinfoLe16(z+44) : FinfoBe16(z+44);` |
|     - |  602 | `		}` |
|   ! 0 |  603 | `		for( i = 0 ; i < nPhNum && nPhEntSize > 0 ; ++i ){` |
|   ! 0 |  604 | `			sxu32 nAt = nPhOff + i*nPhEntSize;` |
|     - |  605 | `			sxu32 nPType;` |
|   ! 0 |  606 | `			if( nAt + 4 > n ){` |
|   ! 0 |  607 | `				break;` |
|     - |  608 | `			}` |
|   ! 0 |  609 | `			nPType = bLsb ? FinfoLe32(z+nAt) : FinfoBe32(z+nAt);` |
|   ! 0 |  610 | `			if( nPType == 3 ){   /* PT_INTERP */` |
|   ! 0 |  611 | `				bInterp = 1;` |
|   ! 0 |  612 | `				break;` |
|     - |  613 | `			}` |
|   ! 0 |  614 | `		}` |
|   ! 0 |  615 | `	}` |
|   ! 0 |  616 | `	switch( nType ){` |
|   ! 0 |  617 | `		case 1: FinfoSay(pOut,"relocatable"); pOut->zMime = "application/x-object"; break;` |
|   ! 0 |  618 | `		case 2: FinfoSay(pOut,"executable"); pOut->zMime = "application/x-executable"; break;` |
|   ! 0 |  619 | `		case 3:` |
|   ! 0 |  620 | `			if( bInterp ){` |
|   ! 0 |  621 | `				FinfoSay(pOut,"pie executable");` |
|   ! 0 |  622 | `				pOut->zMime = "application/x-pie-executable";` |
|   ! 0 |  623 | `			}else{` |
|   ! 0 |  624 | `				FinfoSay(pOut,"shared object");` |
|   ! 0 |  625 | `				pOut->zMime = "application/x-sharedlib";` |
|     - |  626 | `			}` |
|   ! 0 |  627 | `			break;` |
|   ! 0 |  628 | `		case 4: FinfoSay(pOut,"core file"); pOut->zMime = "application/x-coredump"; break;` |
|   ! 0 |  629 | `		default: FinfoSay(pOut,"processor-specific"); pOut->zMime = "application/octet-stream"; break;` |
|     - |  630 | `	}` |
|   ! 0 |  631 | `	switch( nMachine ){` |
|   ! 0 |  632 | `		case 3:   zMachine = "Intel 80386"; break;` |
|   ! 0 |  633 | `		case 8:   zMachine = "MIPS"; break;` |
|   ! 0 |  634 | `		case 20:  zMachine = "PowerPC"; break;` |
|   ! 0 |  635 | `		case 21:  zMachine = "64-bit PowerPC"; break;` |
|   ! 0 |  636 | `		case 22:  zMachine = "IBM S/390"; break;` |
|   ! 0 |  637 | `		case 40:  zMachine = "ARM"; break;` |
|   ! 0 |  638 | `		case 42:  zMachine = "Renesas SH"; break;` |
|   ! 0 |  639 | `		case 43:  zMachine = "SPARC V9"; break;` |
|   ! 0 |  640 | `		case 50:  zMachine = "Intel IA-64"; break;` |
|   ! 0 |  641 | `		case 62:  zMachine = "x86-64"; break;` |
|   ! 0 |  642 | `		case 183: zMachine = "ARM aarch64"; break;` |
|   ! 0 |  643 | `		case 243: zMachine = "UCB RISC-V"; break;` |
|   ! 0 |  644 | `		default:  break;` |
|     - |  645 | `	}` |
|   ! 0 |  646 | `	if( zMachine ){` |
|   ! 0 |  647 | `		SyBlobFormat(&pOut->sDesc,", %s",zMachine);` |
|   ! 0 |  648 | `	}` |
|   ! 0 |  649 | `	SyBlobFormat(&pOut->sDesc,", version %u",nVersion);` |
|   ! 0 |  650 | `	switch( nOsabi ){` |
|   ! 0 |  651 | `		case 0:  FinfoSay(pOut," (SYSV)"); break;` |
|   ! 0 |  652 | `		case 1:  FinfoSay(pOut," (HP-UX)"); break;` |
|   ! 0 |  653 | `		case 2:  FinfoSay(pOut," (NetBSD)"); break;` |
|   ! 0 |  654 | `		case 3:  FinfoSay(pOut," (GNU/Linux)"); break;` |
|   ! 0 |  655 | `		case 6:  FinfoSay(pOut," (Solaris)"); break;` |
|   ! 0 |  656 | `		case 9:  FinfoSay(pOut," (FreeBSD)"); break;` |
|   ! 0 |  657 | `		case 12: FinfoSay(pOut," (OpenBSD)"); break;` |
|   ! 0 |  658 | `		default: break;` |
|     - |  659 | `	}` |
|   ! 0 |  660 | `	return 1;` |
|   245 |  661 | `}` |
|     - |  662 | `/*` |
|     - |  663 | ` * The zip container is four formats: an OpenDocument or an EPUB names itself in` |
|     - |  664 | `` * an uncompressed `mimetype` member the format REQUIRES to be first, a JAR`` |
|     - |  665 | ` * carries META-INF/ and an OOXML document [Content_Types].xml -- and everything` |
|     - |  666 | ` * else is a plain archive.` |
|     - |  667 | ` */` |
|     6 |  668 | `static int FinfoZip(const unsigned char *z,sxu32 n,FinfoAnswer *pOut)` |
|     1 |  669 | `{` |
|     - |  670 | `	sxu32 nNameLen,nExtraLen,nVer,nMethod;` |
|     - |  671 | `	const unsigned char *zName;` |
|     7 |  672 | `	if( FINFO_AT(z,n,0,"PK\005\006") ){` |
|     7 |  673 | `		FinfoSay(pOut,"Zip archive data (empty)");` |
|     7 |  674 | `		pOut->zMime = "application/zip";` |
|     7 |  675 | `		pOut->zExt = "zip/cbz";` |
|     7 |  676 | `		return 1;` |
|     - |  677 | `	}` |
|   ! 0 |  678 | `	if( n < 30 \|\| !FINFO_AT(z,n,0,"PK\003\004") ){` |
|   ! 0 |  679 | `		return 0;` |
|     - |  680 | `	}` |
|   ! 0 |  681 | `	nVer = FinfoLe16(z+4);` |
|   ! 0 |  682 | `	nMethod = FinfoLe16(z+8);` |
|   ! 0 |  683 | `	nNameLen = FinfoLe16(z+26);` |
|   ! 0 |  684 | `	nExtraLen = FinfoLe16(z+28);` |
|   ! 0 |  685 | `	zName = z + 30;` |
|   ! 0 |  686 | `	if( 30 + nNameLen <= n ){` |
|   ! 0 |  687 | `		if( nNameLen == 8 && SyMemcmp(zName,"mimetype",8) == 0 ){` |
|   ! 0 |  688 | `			sxu32 nAt = 30 + nNameLen + nExtraLen;` |
|     - |  689 | `			static const struct { const char *zType; const char *zDesc; const char *zExt; } aOdf[] = {` |
|     - |  690 | `				{ "application/vnd.oasis.opendocument.text",         "OpenDocument Text", "odt" },` |
|     - |  691 | `				{ "application/vnd.oasis.opendocument.spreadsheet",  "OpenDocument Spreadsheet", "ods" },` |
|     - |  692 | `				{ "application/vnd.oasis.opendocument.presentation", "OpenDocument Presentation", "odp" },` |
|     - |  693 | `				{ "application/vnd.oasis.opendocument.graphics",     "OpenDocument Drawing", "odg" },` |
|     - |  694 | `				{ "application/epub+zip",                            "EPUB document", "epub" },` |
|     - |  695 | `			};` |
|     - |  696 | `			sxu32 i;` |
|   ! 0 |  697 | `			for( i = 0 ; i < SX_ARRAYSIZE(aOdf) ; ++i ){` |
|   ! 0 |  698 | `				sxu32 nLen = (sxu32)SyStrlen(aOdf[i].zType);` |
|   ! 0 |  699 | `				if( nAt + nLen <= n && SyMemcmp(z + nAt,aOdf[i].zType,nLen) == 0 ){` |
|   ! 0 |  700 | `					FinfoSay(pOut,aOdf[i].zDesc);` |
|   ! 0 |  701 | `					pOut->zMime = aOdf[i].zType;` |
|   ! 0 |  702 | `					pOut->zExt = aOdf[i].zExt;` |
|   ! 0 |  703 | `					return 1;` |
|     - |  704 | `				}` |
|   ! 0 |  705 | `			}` |
|   ! 0 |  706 | `		}` |
|   ! 0 |  707 | `		if( nNameLen >= 9 && SyMemcmp(zName,"META-INF/",9) == 0 ){` |
|   ! 0 |  708 | `			FinfoSay(pOut,"Java archive data (JAR)");` |
|   ! 0 |  709 | `			pOut->zMime = "application/java-archive";` |
|   ! 0 |  710 | `			pOut->zExt = "jar";` |
|   ! 0 |  711 | `			return 1;` |
|     - |  712 | `		}` |
|   ! 0 |  713 | `		if( nNameLen == 19 && SyMemcmp(zName,"[Content_Types].xml",19) == 0 ){` |
|     - |  714 | `			static const struct { const char *zDir; const char *zDesc; const char *zMime; const char *zExt; } aOox[] = {` |
|     - |  715 | `				{ "word/",  "Microsoft Word 2007+",` |
|     - |  716 | `				  "application/vnd.openxmlformats-officedocument.wordprocessingml.document", "docx" },` |
|     - |  717 | `				{ "xl/",    "Microsoft Excel 2007+",` |
|     - |  718 | `				  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "xlsx" },` |
|     - |  719 | `				{ "ppt/",   "Microsoft PowerPoint 2007+",` |
|     - |  720 | `				  "application/vnd.openxmlformats-officedocument.presentationml.presentation", "pptx" },` |
|     - |  721 | `			};` |
|     - |  722 | `			sxu32 i;` |
|   ! 0 |  723 | `			for( i = 0 ; i < SX_ARRAYSIZE(aOox) ; ++i ){` |
|   ! 0 |  724 | `				if( FinfoFind(z,n,n,aOox[i].zDir,(sxu32)SyStrlen(aOox[i].zDir)) >= 0 ){` |
|   ! 0 |  725 | `					FinfoSay(pOut,aOox[i].zDesc);` |
|   ! 0 |  726 | `					pOut->zMime = aOox[i].zMime;` |
|   ! 0 |  727 | `					pOut->zExt = aOox[i].zExt;` |
|   ! 0 |  728 | `					return 1;` |
|     - |  729 | `				}` |
|   ! 0 |  730 | `			}` |
|   ! 0 |  731 | `		}` |
|   ! 0 |  732 | `	}` |
|   ! 0 |  733 | `	SyBlobFormat(&pOut->sDesc,"Zip archive data, at least v%u.%u to extract",` |
|   ! 0 |  734 | `		nVer / 10,nVer % 10);` |
|   ! 0 |  735 | `	switch( nMethod ){` |
|   ! 0 |  736 | `		case 0:  FinfoSay(pOut,", compression method=store"); break;` |
|   ! 0 |  737 | `		case 6:  FinfoSay(pOut,", compression method=implode"); break;` |
|   ! 0 |  738 | `		case 8:  FinfoSay(pOut,", compression method=deflate"); break;` |
|   ! 0 |  739 | `		case 9:  FinfoSay(pOut,", compression method=deflate64"); break;` |
|   ! 0 |  740 | `		case 12: FinfoSay(pOut,", compression method=bzip2"); break;` |
|   ! 0 |  741 | `		case 14: FinfoSay(pOut,", compression method=LZMA"); break;` |
|   ! 0 |  742 | `		case 93: FinfoSay(pOut,", compression method=Zstandard"); break;` |
|   ! 0 |  743 | `		case 95: FinfoSay(pOut,", compression method=XZ"); break;` |
|   ! 0 |  744 | `		case 98: FinfoSay(pOut,", compression method=PPMd"); break;` |
|   ! 0 |  745 | `		default: break;` |
|     - |  746 | `	}` |
|   ! 0 |  747 | `	pOut->zMime = "application/zip";` |
|   ! 0 |  748 | `	return 1;` |
|     4 |  749 | `}` |
|     - |  750 | `/*` |
|     - |  751 | ` * gzip carries a header full of optional fields, and php prints every one it` |
|     - |  752 | ` * finds: the original NAME, the modification time in LOCAL time, the deflate` |
|     - |  753 | ` * effort, the operating system that wrote it, and -- only when the whole FILE` |
|     - |  754 | ` * is at hand -- the last four bytes, which hold the uncompressed length mod` |
|     - |  755 | `` * 2^32. That last one is why `finfo::file()` and `finfo::buffer()` answer`` |
|     - |  756 | ` * differently for the same bytes.` |
|     - |  757 | ` */` |
|     6 |  758 | `static void FinfoCtime(SyBlob *pOut,sxu32 nEpoch)` |
|     1 |  759 | `{` |
|     - |  760 | `	static const char *azDay[] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };` |
|     - |  761 | `	static const char *azMon[] = { "Jan","Feb","Mar","Apr","May","Jun",` |
|     - |  762 | `	                               "Jul","Aug","Sep","Oct","Nov","Dec" };` |
|     7 |  763 | `	time_t t = (time_t)nEpoch;` |
|     - |  764 | `	struct tm sTm;` |
|     - |  765 | `	struct tm *pTm;` |
|     - |  766 | `	/* UTC, not the box's zone: php's entry for this field is a little-endian` |
|     - |  767 | ``	 * DATE (`ledate`), and libmagic renders those in GMT -- only the `l`-prefixed`` |
|     - |  768 | `	 * spelling is local. Measured, because the two differ by hours here. The` |
|     - |  769 | `	 * reentrant forms take their arguments in opposite orders, the same split` |
|     - |  770 | `	 * builtin_calendar.c carries. */` |
|     - |  771 | `#ifdef __WINNT__` |
|     1 |  772 | `	pTm = (gmtime_s(&sTm,&t) == 0) ? &sTm : 0;` |
|     - |  773 | `#else` |
|     6 |  774 | `	pTm = gmtime_r(&t,&sTm);` |
|     - |  775 | `#endif` |
|     7 |  776 | `	if( pTm == 0 ){` |
|   ! 0 |  777 | `		SyBlobFormat(pOut,"%u",nEpoch);` |
|   ! 0 |  778 | `		return;` |
|     - |  779 | `	}` |
|     7 |  780 | `	SyBlobFormat(pOut,"%s %s %s%u %s%u:%s%u:%s%u %u",` |
|     6 |  781 | `		azDay[pTm->tm_wday % 7],azMon[pTm->tm_mon % 12],` |
|     6 |  782 | `		pTm->tm_mday < 10 ? " " : "",(sxu32)pTm->tm_mday,` |
|     6 |  783 | `		pTm->tm_hour < 10 ? "0" : "",(sxu32)pTm->tm_hour,` |
|     6 |  784 | `		pTm->tm_min < 10 ? "0" : "",(sxu32)pTm->tm_min,` |
|     6 |  785 | `		pTm->tm_sec < 10 ? "0" : "",(sxu32)pTm->tm_sec,` |
|     6 |  786 | `		(sxu32)(pTm->tm_year + 1900));` |
|     4 |  787 | `}` |
|   652 |  788 | `static int FinfoGzip(const unsigned char *z,sxu32 n,ph7_int64 nFile,FinfoAnswer *pOut)` |
|     3 |  789 | `{` |
|     - |  790 | `	sxu32 nFlags,nMtime,nXfl,nOs;` |
|   655 |  791 | `	if( n < 10 \|\| z[0] != 0x1F \|\| z[1] != 0x8B ){` |
|   643 |  792 | `		return 0;` |
|     - |  793 | `	}` |
|    13 |  794 | `	if( z[2] != 8 ){` |
|   ! 0 |  795 | `		return 0;   /* only deflate has ever been used, and php names no other */` |
|     - |  796 | `	}` |
|    13 |  797 | `	nFlags = z[3];` |
|    13 |  798 | `	nMtime = FinfoLe32(z+4);` |
|    13 |  799 | `	nXfl = z[8];` |
|    13 |  800 | `	nOs = z[9];` |
|    13 |  801 | `	FinfoSay(pOut,"gzip compressed data");` |
|    13 |  802 | `	if( nFlags & 0x08 ){    /* FNAME: a NUL-terminated original name */` |
|     7 |  803 | `		sxu32 i = 10;` |
|     7 |  804 | `		if( nFlags & 0x04 ){   /* FEXTRA sits before it */` |
|   ! 0 |  805 | `			if( i + 2 <= n ){` |
|   ! 0 |  806 | `				i += 2 + FinfoLe16(z+i);` |
|   ! 0 |  807 | `			}` |
|   ! 0 |  808 | `		}` |
|     7 |  809 | `		if( i < n ){` |
|     7 |  810 | `			sxu32 nStart = i;` |
|    55 |  811 | `			while( i < n && z[i] != 0 ){` |
|    49 |  812 | `				i++;` |
|     1 |  813 | `			}` |
|     7 |  814 | `			if( i > nStart ){` |
|     7 |  815 | `				SyBlobFormat(&pOut->sDesc,", was \"%.*s\"",(int)(i - nStart),(const char *)(z + nStart));` |
|     3 |  816 | `			}` |
|     3 |  817 | `		}` |
|     3 |  818 | `	}` |
|    13 |  819 | `	if( nMtime > 0 ){` |
|     7 |  820 | `		FinfoSay(pOut,", last modified: ");` |
|     7 |  821 | `		FinfoCtime(&pOut->sDesc,nMtime);` |
|     3 |  822 | `	}` |
|    13 |  823 | `	if( nXfl == 2 ){` |
|     7 |  824 | `		FinfoSay(pOut,", max compression");` |
|    10 |  825 | `	}else if( nXfl == 4 ){` |
|   ! 0 |  826 | `		FinfoSay(pOut,", max speed");` |
|   ! 0 |  827 | `	}` |
|    13 |  828 | `	switch( nOs ){` |
|   ! 0 |  829 | `		case 0:  FinfoSay(pOut,", from FAT filesystem (MS-DOS, OS/2, NT)"); break;` |
|   ! 0 |  830 | `		case 1:  FinfoSay(pOut,", from Amiga"); break;` |
|   ! 0 |  831 | `		case 2:  FinfoSay(pOut,", from VMS"); break;` |
|    13 |  832 | `		case 3:  FinfoSay(pOut,", from Unix"); break;` |
|   ! 0 |  833 | `		case 4:  FinfoSay(pOut,", from VM/CMS"); break;` |
|   ! 0 |  834 | `		case 5:  FinfoSay(pOut,", from Atari"); break;` |
|   ! 0 |  835 | `		case 6:  FinfoSay(pOut,", from HPFS filesystem (OS/2, NT)"); break;` |
|   ! 0 |  836 | `		case 7:  FinfoSay(pOut,", from Macintosh"); break;` |
|   ! 0 |  837 | `		case 8:  FinfoSay(pOut,", from Z-System"); break;` |
|   ! 0 |  838 | `		case 9:  FinfoSay(pOut,", from CP/M"); break;` |
|   ! 0 |  839 | `		case 10: FinfoSay(pOut,", from TOPS/20"); break;` |
|   ! 0 |  840 | `		case 11: FinfoSay(pOut,", from NTFS filesystem (NT)"); break;` |
|   ! 0 |  841 | `		case 12: FinfoSay(pOut,", from QDOS"); break;` |
|   ! 0 |  842 | `		case 13: FinfoSay(pOut,", from Acorn RISCOS"); break;` |
|   ! 0 |  843 | `		default: break;` |
|     - |  844 | `	}` |
|     - |  845 | `	/* The trailer is only readable when the object being examined IS the whole` |
|     - |  846 | `	 * file -- which is the one thing buffer() cannot know. */` |
|    13 |  847 | `	if( nFile >= 18 && (ph7_int64)n >= nFile ){` |
|   ! 0 |  848 | `		SyBlobFormat(&pOut->sDesc,", original size modulo 2^32 %u",FinfoLe32(z + (sxu32)nFile - 4));` |
|     - |  849 | `		/* php's extension list hangs off that same branch of its entry, so the` |
|     - |  850 | ``		 * buffer door answers `???` for bytes the file door names. */`` |
|   ! 0 |  851 | `		pOut->zExt = "gz/tgz/tpz/ipk/vbox-extpack/svgz/blend/dia/gnucash/rdata/xoj";` |
|   ! 0 |  852 | `	}` |
|    13 |  853 | `	pOut->zMime = "application/gzip";` |
|    13 |  854 | `	return 1;` |
|   329 |  855 | `}` |
|     - |  856 | `/*` |
|     - |  857 | ` * An MPEG audio frame header, which php spells out in full: the version, the` |
|     - |  858 | ` * bitrate, the sample rate and the channel mode all live in four bytes, and` |
|     - |  859 | ` * every one of them is a table lookup. Shared by the bare frame and by the` |
|     - |  860 | `` * `contains:` clause an ID3 tag puts in front of one.`` |
|     - |  861 | ` */` |
|   532 |  862 | `static int FinfoMpegFrame(const unsigned char *z,sxu32 n,sxu32 nAt,const char *zLead,` |
|     - |  863 | `	FinfoAnswer *pOut)` |
|     3 |  864 | `{` |
|     - |  865 | `	static const sxu16 aRateV1[16] = { 0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0 };` |
|     - |  866 | `	static const sxu16 aRateV2[16] = { 0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0 };` |
|     - |  867 | `	static const char *azHzV1[4]   = { ", 44.1 kHz", ", 48 kHz", ", 32 kHz", 0 };` |
|     - |  868 | `	static const char *azHzV2[4]   = { ", 22.05 kHz", ", 24 kHz", ", 16 kHz", 0 };` |
|     - |  869 | `	static const char *azHzV25[4]  = { ", 11.025 kHz", ", 12 kHz", ", 8 kHz", 0 };` |
|     - |  870 | `	static const char *azMode[4]   = { ", Stereo", ", JntStereo", ", 2x Monaural", ", Monaural" };` |
|     - |  871 | `	sxu32 nVer,nRate,nHz,nMode;` |
|   535 |  872 | `	if( nAt + 4 > n \|\| z[nAt] != 0xFF \|\| (z[nAt+1] & 0xE0) != 0xE0 ){` |
|   517 |  873 | `		return 0;` |
|     - |  874 | `	}` |
|    19 |  875 | `	nVer = (z[nAt+1] >> 3) & 3;` |
|    19 |  876 | `	if( nVer == 1 \|\| (z[nAt+1] & 0x06) != 0x02 ){` |
|    13 |  877 | `		return 0;   /* the reserved version, or a layer that is not III */` |
|     - |  878 | `	}` |
|     7 |  879 | `	nRate = (z[nAt+2] >> 4) & 0x0F;` |
|     7 |  880 | `	nHz = (z[nAt+2] >> 2) & 3;` |
|     7 |  881 | `	nMode = (z[nAt+3] >> 6) & 3;` |
|     7 |  882 | `	if( nRate == 0 \|\| nRate == 15 \|\| nHz == 3 ){` |
|   ! 0 |  883 | `		return 0;` |
|     - |  884 | `	}` |
|     - |  885 | `	/* Nothing is written before every screen above has passed: the ID3 caller` |
|     - |  886 | ``	 * only earns its `, contains: ` when a frame really follows the tag. */`` |
|     7 |  887 | `	if( zLead ){` |
|   ! 0 |  888 | `		FinfoSay(pOut,zLead);` |
|   ! 0 |  889 | `	}` |
|     7 |  890 | `	FinfoSay(pOut,"MPEG ADTS, layer III");` |
|     7 |  891 | `	FinfoSay(pOut,nVer == 3 ? ", v1" : (nVer == 2 ? ", v2" : ", v2.5"));` |
|    13 |  892 | `	SyBlobFormat(&pOut->sDesc,", %u kbps",` |
|     3 |  893 | `		(sxu32)(nVer == 3 ? aRateV1[nRate] : aRateV2[nRate]));` |
|     7 |  894 | `	FinfoSay(pOut,nVer == 3 ? azHzV1[nHz] : (nVer == 2 ? azHzV2[nHz] : azHzV25[nHz]));` |
|     7 |  895 | `	FinfoSay(pOut,azMode[nMode]);` |
|     7 |  896 | `	return 1;` |
|   269 |  897 | `}` |
|     - |  898 | `/*` |
|     - |  899 | ` * Every other binary signature, in the order libmagic's strength ordering` |
|     - |  900 | ` * happens to try them: a longer, more specific magic before a shorter one that` |
|     - |  901 | ` * would also match.` |
|     - |  902 | ` */` |
|   640 |  903 | `static int FinfoBinary(const unsigned char *z,sxu32 n,ph7_int64 nFile,FinfoAnswer *pOut)` |
|     3 |  904 | `{` |
|   643 |  905 | `	if( n < 4 ){` |
|    19 |  906 | `		return 0;` |
|     - |  907 | `	}` |
|   625 |  908 | `	if( FINFO_AT(z,n,0,"\211PNG\r\n\032\n") ){` |
|    17 |  909 | `		FinfoSay(pOut,"PNG image data");` |
|    17 |  910 | `		if( n >= 33 && FINFO_AT(z,n,12,"IHDR") ){` |
|     - |  911 | `			static const char *azColor[] = { " grayscale", 0, "/color RGB", " colormap",` |
|     - |  912 | `			                                 " gray+alpha", 0, "/color RGBA" };` |
|    17 |  913 | `			sxu32 nColor = z[25];` |
|    17 |  914 | `			SyBlobFormat(&pOut->sDesc,", %u x %u, %u-bit",FinfoBe32(z+16),FinfoBe32(z+20),(sxu32)z[24]);` |
|    17 |  915 | `			if( nColor < SX_ARRAYSIZE(azColor) && azColor[nColor] ){` |
|    17 |  916 | `				FinfoSay(pOut,azColor[nColor]);` |
|     8 |  917 | `			}` |
|    17 |  918 | `			FinfoSay(pOut,z[28] ? ", interlaced" : ", non-interlaced");` |
|     8 |  919 | `		}` |
|    17 |  920 | `		pOut->zMime = "image/png";` |
|    17 |  921 | `		pOut->zExt = "png";` |
|    17 |  922 | `		return 1;` |
|     - |  923 | `	}` |
|   609 |  924 | `	if( FINFO_AT(z,n,0,"GIF87a") \|\| FINFO_AT(z,n,0,"GIF89a") ){` |
|    22 |  925 | `		SyBlobFormat(&pOut->sDesc,"GIF image data, version %.3s",(const char *)(z + 3));` |
|    22 |  926 | `		if( n >= 10 ){` |
|    22 |  927 | `			SyBlobFormat(&pOut->sDesc,", %u x %u",FinfoLe16(z+6),FinfoLe16(z+8));` |
|    10 |  928 | `		}` |
|    22 |  929 | `		pOut->zMime = "image/gif";` |
|    22 |  930 | `		pOut->zExt = "gif";` |
|    22 |  931 | `		return 1;` |
|     - |  932 | `	}` |
|   589 |  933 | `	if( z[0] == 0xFF && z[1] == 0xD8 && z[2] == 0xFF ){` |
|     5 |  934 | `		FinfoSay(pOut,"JPEG image data");` |
|     - |  935 | `		/* The JFIF APP0 sits at a fixed offset and its four fields are read` |
|     - |  936 | `		 * there; the comment and the frame are found by walking the segment` |
|     - |  937 | `		 * chain, which is what php's own entry does. */` |
|     5 |  938 | `		if( FINFO_AT(z,n,6,"JFIF\0") && n >= 18 ){` |
|     5 |  939 | `			SyBlobFormat(&pOut->sDesc,", JFIF standard %u.%s%u",(sxu32)z[11],` |
|     4 |  940 | `				z[12] < 10 ? "0" : "",(sxu32)z[12]);` |
|     5 |  941 | `			switch( z[13] ){` |
|     5 |  942 | `				case 0: FinfoSay(pOut,", aspect ratio"); break;` |
|   ! 0 |  943 | `				case 1: FinfoSay(pOut,", resolution (DPI)"); break;` |
|   ! 0 |  944 | `				case 2: FinfoSay(pOut,", resolution (DPCM)"); break;` |
|   ! 0 |  945 | `				default: break;` |
|     - |  946 | `			}` |
|     5 |  947 | `			SyBlobFormat(&pOut->sDesc,", density %ux%u",FinfoBe16(z+14),FinfoBe16(z+16));` |
|     5 |  948 | `			SyBlobFormat(&pOut->sDesc,", segment length %u",FinfoBe16(z+4));` |
|     2 |  949 | `		}` |
|     - |  950 | `		{` |
|     5 |  951 | `			sxu32 i = 2;` |
|     9 |  952 | `			while( i + 4 <= n && z[i] == 0xFF ){` |
|     9 |  953 | `				sxu32 nMark = z[i+1];` |
|     9 |  954 | `				sxu32 nLen = FinfoBe16(z+i+2);` |
|     9 |  955 | `				if( nMark == 0xD8 \|\| nMark == 0x01 \|\| (nMark >= 0xD0 && nMark <= 0xD7) ){` |
|   ! 0 |  956 | `					i += 2;` |
|   ! 0 |  957 | `					continue;` |
|     - |  958 | `				}` |
|     9 |  959 | `				if( nMark == 0xDA \|\| nMark == 0xD9 \|\| nLen < 2 ){` |
|   ! 0 |  960 | `					break;   /* the entropy-coded scan: nothing else is a segment */` |
|     - |  961 | `				}` |
|     9 |  962 | `				if( nMark == 0xFE && i + 4 < n ){` |
|   ! 0 |  963 | `					sxu32 nText = nLen - 2;` |
|   ! 0 |  964 | `					if( i + 4 + nText > n ){` |
|   ! 0 |  965 | `						nText = n - (i + 4);` |
|   ! 0 |  966 | `					}` |
|   ! 0 |  967 | `					while( nText > 0 && z[i+4+nText-1] <= ' ' ){` |
|   ! 0 |  968 | `						nText--;` |
|   ! 0 |  969 | `					}` |
|   ! 0 |  970 | `					SyBlobFormat(&pOut->sDesc,", comment: \"%.*s\"",(int)nText,` |
|   ! 0 |  971 | `						(const char *)(z + i + 4));` |
|   ! 0 |  972 | `				}` |
|     8 |  973 | `				if( (nMark >= 0xC0 && nMark <= 0xCF)` |
|     7 |  974 | `				 && nMark != 0xC4 && nMark != 0xC8 && nMark != 0xCC && i + 10 <= n ){` |
|     5 |  975 | `					switch( nMark ){` |
|     5 |  976 | `						case 0xC0: FinfoSay(pOut,", baseline"); break;` |
|   ! 0 |  977 | `						case 0xC1: FinfoSay(pOut,", extended sequential"); break;` |
|   ! 0 |  978 | `						case 0xC2: FinfoSay(pOut,", progressive"); break;` |
|   ! 0 |  979 | `						default:   FinfoSay(pOut,", lossless"); break;` |
|     - |  980 | `					}` |
|     5 |  981 | `					SyBlobFormat(&pOut->sDesc,", precision %u",(sxu32)z[i+4]);` |
|     5 |  982 | `					SyBlobFormat(&pOut->sDesc,", %ux%u",FinfoBe16(z+i+7),FinfoBe16(z+i+5));` |
|     5 |  983 | `					SyBlobFormat(&pOut->sDesc,", components %u",(sxu32)z[i+9]);` |
|     5 |  984 | `					break;` |
|     - |  985 | `				}` |
|     5 |  986 | `				i += 2 + nLen;` |
|     1 |  987 | `			}` |
|     - |  988 | `		}` |
|     5 |  989 | `		pOut->zMime = "image/jpeg";` |
|     5 |  990 | `		pOut->zExt = "jpeg/jpg/jpe/jfif";` |
|     5 |  991 | `		return 1;` |
|     - |  992 | `	}` |
|   585 |  993 | `	if( FINFO_AT(z,n,0,"%PDF-") ){` |
|     - |  994 | `		sxi32 nPages;` |
|     7 |  995 | `		if( n >= 8 ){` |
|     7 |  996 | `			SyBlobFormat(&pOut->sDesc,"PDF document, version %c.%c",z[5],z[7]);` |
|     4 |  997 | `		}else{` |
|   ! 0 |  998 | `			FinfoSay(pOut,"PDF document");` |
|     - |  999 | `		}` |
|     - | 1000 | ``		/* The page count is the `/Count` beside the document's `/Pages`, which`` |
|     - | 1001 | `		 * is what php's entry looks for and why a PDF that names neither says` |
|     - | 1002 | `		 * only its version. */` |
|     7 | 1003 | `		nPages = FINFO_FIND(z,n,1024,"/Pages");` |
|     7 | 1004 | `		if( nPages >= 0 ){` |
|     7 | 1005 | `			sxu32 nLimit = (sxu32)nPages + 70;` |
|     - | 1006 | `			sxi32 nCount;` |
|     7 | 1007 | `			if( nLimit > n ){` |
|     7 | 1008 | `				nLimit = n;` |
|     3 | 1009 | `			}` |
|     7 | 1010 | `			nCount = FinfoFind(z + nPages,nLimit - (sxu32)nPages,nLimit - (sxu32)nPages,` |
|     - | 1011 | `				"/Count",6);` |
|     7 | 1012 | `			if( nCount >= 0 ){` |
|     7 | 1013 | `				sxu32 i = (sxu32)nPages + (sxu32)nCount + 6;` |
|     7 | 1014 | `				sxu32 nVal = 0;` |
|     7 | 1015 | `				int bAny = 0;` |
|    13 | 1016 | `				while( i < n && z[i] == ' ' ){` |
|     7 | 1017 | `					i++;` |
|     1 | 1018 | `				}` |
|    13 | 1019 | `				while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|     7 | 1020 | `					nVal = nVal * 10 + (sxu32)(z[i] - '0');` |
|     7 | 1021 | `					bAny = 1;` |
|     7 | 1022 | `					i++;` |
|     1 | 1023 | `				}` |
|     7 | 1024 | `				if( bAny ){` |
|     7 | 1025 | `					SyBlobFormat(&pOut->sDesc,", %u page(s)",nVal);` |
|     3 | 1026 | `				}` |
|     3 | 1027 | `			}` |
|     3 | 1028 | `		}` |
|     7 | 1029 | `		pOut->zMime = "application/pdf";` |
|     7 | 1030 | `		pOut->zExt = "pdf";` |
|     7 | 1031 | `		return 1;` |
|     - | 1032 | `	}` |
|   579 | 1033 | `	if( FINFO_AT(z,n,0,"BM") && n >= 34 && FinfoLe32(z+14) == 40 ){` |
|     - | 1034 | `		sxu32 nXres,nYres;` |
|     7 | 1035 | `		SyBlobFormat(&pOut->sDesc,"PC bitmap, Windows 3.x format, %u x %u x %u",` |
|     2 | 1036 | `			FinfoLe32(z+18),FinfoLe32(z+22),FinfoLe16(z+28));` |
|     5 | 1037 | `		nXres = (n >= 46) ? FinfoLe32(z+38) : 0;` |
|     5 | 1038 | `		nYres = (n >= 46) ? FinfoLe32(z+42) : 0;` |
|     5 | 1039 | `		if( nXres \|\| nYres ){` |
|   ! 0 | 1040 | `			SyBlobFormat(&pOut->sDesc,", resolution %u x %u px/m",nXres,nYres);` |
|   ! 0 | 1041 | `		}` |
|     5 | 1042 | `		SyBlobFormat(&pOut->sDesc,", cbSize %u, bits offset %u",FinfoLe32(z+2),FinfoLe32(z+10));` |
|     5 | 1043 | `		pOut->zMime = "image/bmp";` |
|     5 | 1044 | `		pOut->zExt = "bmp";` |
|     5 | 1045 | `		return 1;` |
|     - | 1046 | `	}` |
|   575 | 1047 | `	if( (FINFO_AT(z,n,0,"II\052\000") \|\| FINFO_AT(z,n,0,"MM\000\052")) && n >= 8 ){` |
|     5 | 1048 | `		int bLe = (z[0] == 'I');` |
|     5 | 1049 | `		sxu32 nIfd = bLe ? FinfoLe32(z+4) : FinfoBe32(z+4);` |
|     5 | 1050 | `		SyBlobFormat(&pOut->sDesc,"TIFF image data, %s-endian",bLe ? "little" : "big");` |
|     5 | 1051 | `		if( nIfd <= n && n - nIfd >= 2 ){` |
|     9 | 1052 | `			SyBlobFormat(&pOut->sDesc,", direntries=%u",` |
|     2 | 1053 | `				bLe ? FinfoLe16(z+nIfd) : FinfoBe16(z+nIfd));` |
|     2 | 1054 | `		}` |
|     5 | 1055 | `		pOut->zMime = "image/tiff";` |
|     5 | 1056 | `		pOut->zExt = "tif/tiff";` |
|     5 | 1057 | `		return 1;` |
|     - | 1058 | `	}` |
|   571 | 1059 | `	if( FINFO_AT(z,n,0,"RIFF") && n >= 12 ){` |
|    13 | 1060 | `		FinfoSay(pOut,"RIFF (little-endian) data");` |
|    13 | 1061 | `		if( FINFO_AT(z,n,8,"WEBP") ){` |
|     7 | 1062 | `			FinfoSay(pOut,", Web/P image");` |
|     7 | 1063 | `			pOut->zMime = "image/webp";` |
|     7 | 1064 | `			pOut->zExt = "webp";` |
|    10 | 1065 | `		}else if( FINFO_AT(z,n,8,"WAVE") ){` |
|     7 | 1066 | `			FinfoSay(pOut,", WAVE audio");` |
|     7 | 1067 | `			if( FINFO_AT(z,n,12,"fmt ") && n >= 36 ){` |
|     7 | 1068 | `				sxu32 nFmt = FinfoLe16(z+20),nChan = FinfoLe16(z+22),nRate = FinfoLe32(z+24);` |
|     7 | 1069 | `				if( nFmt == 1 ){` |
|     7 | 1070 | `					FinfoSay(pOut,", Microsoft PCM");` |
|     7 | 1071 | `					if( n >= 36 ){` |
|     7 | 1072 | `						SyBlobFormat(&pOut->sDesc,", %u bit",FinfoLe16(z+34));` |
|     3 | 1073 | `					}` |
|     3 | 1074 | `				}` |
|     7 | 1075 | `				if( nChan == 1 ){` |
|   ! 0 | 1076 | `					FinfoSay(pOut,", mono");` |
|     7 | 1077 | `				}else if( nChan == 2 ){` |
|     7 | 1078 | `					FinfoSay(pOut,", stereo");` |
|     4 | 1079 | `				}else{` |
|   ! 0 | 1080 | `					SyBlobFormat(&pOut->sDesc,", %u channels",nChan);` |
|     - | 1081 | `				}` |
|     7 | 1082 | `				SyBlobFormat(&pOut->sDesc," %u Hz",nRate);` |
|     3 | 1083 | `			}` |
|     7 | 1084 | `			pOut->zMime = "audio/x-wav";` |
|     7 | 1085 | `			pOut->zExt = "wav/wave";` |
|     3 | 1086 | `		}else if( FINFO_AT(z,n,8,"AVI ") ){` |
|   ! 0 | 1087 | `			FinfoSay(pOut,", AVI");` |
|   ! 0 | 1088 | `			pOut->zMime = "video/x-msvideo";` |
|   ! 0 | 1089 | `			pOut->zExt = "avi/divx";` |
|   ! 0 | 1090 | `		}else{` |
|   ! 0 | 1091 | `			pOut->zMime = "application/octet-stream";` |
|     - | 1092 | `		}` |
|    13 | 1093 | `		return 1;` |
|     - | 1094 | `	}` |
|   559 | 1095 | `	if( FINFO_AT(z,n,4,"ftyp") && n >= 12 ){` |
|     - | 1096 | `		static const struct { const char *zBrand; const char *zDesc; const char *zMime; const char *zExt; } aFtyp[] = {` |
|     - | 1097 | `			{ "isom", "ISO Media, MP4 Base Media v1 [ISO 14496-12:2003]", "video/mp4", "mp4" },` |
|     - | 1098 | `			{ "mp41", "ISO Media, MP4 v1 [ISO 14496-1:ch13]", "video/mp4", 0 },` |
|     - | 1099 | `			{ "mp42", "ISO Media, MP4 v2 [ISO 14496-14]", "video/mp4", 0 },` |
|     - | 1100 | `			{ "M4A ", "ISO Media, Apple iTunes ALAC/AAC-LC (.M4A) Audio", "audio/x-m4a", 0 },` |
|     - | 1101 | `			{ "M4V ", "ISO Media, Apple iTunes Video (.M4V) Video", "video/x-m4v", 0 },` |
|     - | 1102 | `			{ "qt  ", "ISO Media, Apple QuickTime movie", "video/quicktime", 0 },` |
|     - | 1103 | `			{ "avif", "ISO Media, AVIF Image", "image/avif", 0 },` |
|     - | 1104 | `			{ "avis", "ISO Media, AVIF Image Sequence", "image/avif", 0 },` |
|     - | 1105 | `			{ "heic", "ISO Media, HEIF Image HEVC Main or Main Still Picture Profile", "image/heic", 0 },` |
|     - | 1106 | `			{ "heix", "ISO Media, HEIF Image HEVC Main 10 Profile", "image/heic", 0 },` |
|     - | 1107 | `			{ "mif1", "ISO Media, HEIF Image", "image/heif", 0 },` |
|     - | 1108 | `			{ "3gp4", "ISO Media, MPEG v4 system, 3GPP", "video/3gpp", 0 },` |
|     - | 1109 | `		};` |
|     - | 1110 | `		sxu32 i;` |
|    61 | 1111 | `		for( i = 0 ; i < SX_ARRAYSIZE(aFtyp) ; ++i ){` |
|    61 | 1112 | `			if( SyMemcmp(z + 8,aFtyp[i].zBrand,4) == 0 ){` |
|    13 | 1113 | `				FinfoSay(pOut,aFtyp[i].zDesc);` |
|    13 | 1114 | `				pOut->zMime = aFtyp[i].zMime;` |
|    13 | 1115 | `				pOut->zExt = aFtyp[i].zExt;` |
|    13 | 1116 | `				return 1;` |
|     - | 1117 | `			}` |
|    25 | 1118 | `		}` |
|   ! 0 | 1119 | `		FinfoSay(pOut,"ISO Media");` |
|   ! 0 | 1120 | `		pOut->zMime = "video/mp4";` |
|   ! 0 | 1121 | `		return 1;` |
|     - | 1122 | `	}` |
|   547 | 1123 | `	if( FINFO_AT(z,n,0,"\032\105\337\243") ){   /* EBML */` |
|     - | 1124 | `		/* The DocType ELEMENT decides which of the two this is, and php believes` |
|     - | 1125 | `` 		 * nothing without it: a buffer that merely carries the word `webm` `` |
|     - | 1126 | ``		 * somewhere is `data` under php too. The element is 0x4282, a`` |
|     - | 1127 | `		 * length byte with its size marker, then the name. */` |
|     5 | 1128 | `		sxi32 nAt = FINFO_FIND(z,n,64,"\102\202");` |
|     5 | 1129 | `		if( nAt >= 0 && (sxu32)nAt + 3 < n ){` |
|   ! 0 | 1130 | `			sxu32 nName = (sxu32)nAt + 3;` |
|   ! 0 | 1131 | `			if( FinfoAt(z,n,nName,"webm",4) ){` |
|   ! 0 | 1132 | `				FinfoSay(pOut,"WebM");` |
|   ! 0 | 1133 | `				pOut->zMime = "video/webm";` |
|   ! 0 | 1134 | `				return 1;` |
|     - | 1135 | `			}` |
|   ! 0 | 1136 | `			if( FinfoAt(z,n,nName,"matroska",8) ){` |
|   ! 0 | 1137 | `				FinfoSay(pOut,"Matroska data");` |
|   ! 0 | 1138 | `				pOut->zMime = "video/x-matroska";` |
|   ! 0 | 1139 | `				return 1;` |
|     - | 1140 | `			}` |
|   ! 0 | 1141 | `		}` |
|     5 | 1142 | `		return 0;` |
|     - | 1143 | `	}` |
|   543 | 1144 | `	if( FINFO_AT(z,n,0,"OggS") ){` |
|     5 | 1145 | `		FinfoSay(pOut,"Ogg data");` |
|     5 | 1146 | `		if( FINFO_FIND(z,n,64,"\001vorbis") >= 0 ){` |
|     5 | 1147 | `			FinfoSay(pOut,", Vorbis audio");` |
|     5 | 1148 | `			pOut->zMime = "audio/ogg";` |
|     2 | 1149 | `		}else if( FINFO_FIND(z,n,64,"OpusHead") >= 0 ){` |
|   ! 0 | 1150 | `			FinfoSay(pOut,", Opus audio");` |
|   ! 0 | 1151 | `			pOut->zMime = "audio/ogg";` |
|   ! 0 | 1152 | `		}else if( FINFO_FIND(z,n,64,"\200theora") >= 0 ){` |
|   ! 0 | 1153 | `			FinfoSay(pOut,", Theora video");` |
|   ! 0 | 1154 | `			pOut->zMime = "video/ogg";` |
|   ! 0 | 1155 | `		}else{` |
|   ! 0 | 1156 | `			pOut->zMime = "application/octet-stream";` |
|     - | 1157 | `		}` |
|     5 | 1158 | `		return 1;` |
|     - | 1159 | `	}` |
|   539 | 1160 | `	if( FINFO_AT(z,n,0,"fLaC") ){` |
|     5 | 1161 | `		FinfoSay(pOut,"FLAC audio bitstream data");` |
|     5 | 1162 | `		pOut->zMime = "audio/flac";` |
|     5 | 1163 | `		return 1;` |
|     - | 1164 | `	}` |
|   535 | 1165 | `	if( FINFO_AT(z,n,0,"ID3") && n >= 10 ){` |
|     - | 1166 | `		/* The tag says nothing about what follows it, and php's answer says so:` |
|     - | 1167 | ``		 * a bare ID3 header is `application/octet-stream` and only the MPEG`` |
|     - | 1168 | `		 * frame after the tag makes it audio. The length is syncsafe -- seven` |
|     - | 1169 | `		 * bits per byte. */` |
|   ! 0 | 1170 | `		sxu32 nTag = 10 + ((((sxu32)z[6] & 0x7F) << 21) \| (((sxu32)z[7] & 0x7F) << 14)` |
|   ! 0 | 1171 | `		                 \| (((sxu32)z[8] & 0x7F) << 7) \| ((sxu32)z[9] & 0x7F));` |
|   ! 0 | 1172 | `		SyBlobFormat(&pOut->sDesc,"Audio file with ID3 version 2.%u.%u",(sxu32)z[3],(sxu32)z[4]);` |
|   ! 0 | 1173 | `		pOut->zMime = FinfoMpegFrame(z,n,nTag,", contains: ",pOut)` |
|   ! 0 | 1174 | `			? "audio/mpeg" : "application/octet-stream";` |
|   ! 0 | 1175 | `		return 1;` |
|     - | 1176 | `	}` |
|     - | 1177 | `	/* A bare MPEG audio frame. The screen inside FinfoMpegFrame is narrow on` |
|     - | 1178 | `	 * purpose: anything looser reads a UTF-16 BOM as audio, which is exactly` |
|     - | 1179 | `	 * what it did before. */` |
|   535 | 1180 | `	if( FinfoMpegFrame(z,n,0,0,pOut) ){` |
|     7 | 1181 | `		pOut->zMime = "audio/mpeg";` |
|     7 | 1182 | `		return 1;` |
|     - | 1183 | `	}` |
|   529 | 1184 | `	if( FINFO_AT(z,n,0,"BZh") && z[3] >= '1' && z[3] <= '9' ){` |
|     7 | 1185 | `		SyBlobFormat(&pOut->sDesc,"bzip2 compressed data, block size = %c00k",z[3]);` |
|     7 | 1186 | `		pOut->zMime = "application/x-bzip2";` |
|     7 | 1187 | `		pOut->zExt = "bz2";` |
|     7 | 1188 | `		return 1;` |
|     - | 1189 | `	}` |
|   523 | 1190 | `	if( FINFO_AT(z,n,0,"\375" "7zXZ\000") ){` |
|     7 | 1191 | `		FinfoSay(pOut,"XZ compressed data");` |
|     7 | 1192 | `		if( n >= 8 ){` |
|     7 | 1193 | `			switch( z[7] & 0x0F ){` |
|   ! 0 | 1194 | `				case 0x00: FinfoSay(pOut,", checksum NONE"); break;` |
|   ! 0 | 1195 | `				case 0x01: FinfoSay(pOut,", checksum CRC32"); break;` |
|     7 | 1196 | `				case 0x04: FinfoSay(pOut,", checksum CRC64"); break;` |
|   ! 0 | 1197 | `				case 0x0A: FinfoSay(pOut,", checksum SHA-256"); break;` |
|   ! 0 | 1198 | `				default: break;` |
|     - | 1199 | `			}` |
|     3 | 1200 | `		}` |
|     7 | 1201 | `		pOut->zMime = "application/x-xz";` |
|     7 | 1202 | `		pOut->zExt = "xz";` |
|     7 | 1203 | `		return 1;` |
|     - | 1204 | `	}` |
|   517 | 1205 | `	if( z[0] == 0x28 && z[1] == 0xB5 && z[2] == 0x2F && z[3] == 0xFD ){` |
|     7 | 1206 | `		FinfoSay(pOut,"Zstandard compressed data (v0.8+)");` |
|     7 | 1207 | `		if( n >= 5 && (z[4] & 0x03) == 0 ){` |
|     7 | 1208 | `			FinfoSay(pOut,", Dictionary ID: None");` |
|     3 | 1209 | `		}` |
|     7 | 1210 | `		pOut->zMime = "application/zstd";` |
|     7 | 1211 | `		pOut->zExt = "zst";` |
|     7 | 1212 | `		return 1;` |
|     - | 1213 | `	}` |
|   511 | 1214 | `	if( FINFO_AT(z,n,0,"7z\274\257\047\034") && n >= 8 ){` |
|     7 | 1215 | `		SyBlobFormat(&pOut->sDesc,"7-zip archive data, version %u.%u",(sxu32)z[6],(sxu32)z[7]);` |
|     7 | 1216 | `		pOut->zMime = "application/x-7z-compressed";` |
|     7 | 1217 | `		pOut->zExt = "7z/cb7";` |
|     7 | 1218 | `		return 1;` |
|     - | 1219 | `	}` |
|   505 | 1220 | `	if( FINFO_AT(z,n,0,"Rar!\032\007") ){` |
|    13 | 1221 | `		FinfoSay(pOut,"RAR archive data");` |
|    13 | 1222 | `		if( n >= 8 && z[6] == 0x01 && z[7] == 0x00 ){` |
|     7 | 1223 | `			FinfoSay(pOut,", v5");` |
|     7 | 1224 | `			pOut->zExt = "rar";` |
|     4 | 1225 | `		}else{` |
|     7 | 1226 | `			pOut->zExt = "rar/cbr";` |
|     - | 1227 | `		}` |
|    13 | 1228 | `		pOut->zMime = "application/vnd.rar";` |
|    13 | 1229 | `		return 1;` |
|     - | 1230 | `	}` |
|   493 | 1231 | `	if( n >= 3 && z[0] == 0x1F && z[1] == 0x9D ){` |
|   ! 0 | 1232 | `		SyBlobFormat(&pOut->sDesc,"compress'd data %u bits",(sxu32)(z[2] & 0x1F));` |
|   ! 0 | 1233 | `		pOut->zMime = "application/x-compress";` |
|   ! 0 | 1234 | `		pOut->zExt = "Z";` |
|   ! 0 | 1235 | `		return 1;` |
|     - | 1236 | `	}` |
|   493 | 1237 | `	if( FINFO_AT(z,n,0,"\004\042M\030") ){` |
|   ! 0 | 1238 | `		FinfoSay(pOut,"LZ4 compressed data (v1.4+)");` |
|   ! 0 | 1239 | `		pOut->zMime = "application/x-lz4";` |
|   ! 0 | 1240 | `		pOut->zExt = "lz4";` |
|   ! 0 | 1241 | `		return 1;` |
|     - | 1242 | `	}` |
|   493 | 1243 | `	if( FINFO_AT(z,n,0,"PK") ){` |
|     7 | 1244 | `		return FinfoZip(z,n,pOut);` |
|     - | 1245 | `	}` |
|   487 | 1246 | `	if( FinfoElf(z,n,nFile,pOut) ){` |
|   ! 0 | 1247 | `		return 1;` |
|     - | 1248 | `	}` |
|   487 | 1249 | `	if( FINFO_AT(z,n,0,"\312\376\272\276") && n >= 8 ){` |
|     - | 1250 | `		{` |
|     7 | 1251 | `			sxu32 nMajor = FinfoBe16(z+6);` |
|    10 | 1252 | `			SyBlobFormat(&pOut->sDesc,"compiled Java class data, version %u.%u",` |
|     3 | 1253 | `				nMajor,FinfoBe16(z+4));` |
|     - | 1254 | `			/* The platform NAME the version belongs to, which php prints from 1.2` |
|     - | 1255 | `			 * on: 46..52 are the 1.x line and everything after is the plain` |
|     - | 1256 | `			 * release number. */` |
|     7 | 1257 | `			if( nMajor >= 46 && nMajor <= 52 ){` |
|     7 | 1258 | `				SyBlobFormat(&pOut->sDesc," (Java 1.%u)",nMajor - 44);` |
|     3 | 1259 | `			}else if( nMajor > 52 ){` |
|   ! 0 | 1260 | `				SyBlobFormat(&pOut->sDesc," (Java %u)",nMajor - 44);` |
|   ! 0 | 1261 | `			}` |
|     - | 1262 | `		}` |
|     7 | 1263 | `		pOut->zMime = "application/x-java-applet";` |
|     7 | 1264 | `		pOut->zExt = "class";` |
|     7 | 1265 | `		return 1;` |
|     - | 1266 | `	}` |
|   481 | 1267 | `	if( FINFO_AT(z,n,0,"\000asm") && n >= 8 ){` |
|    10 | 1268 | `		SyBlobFormat(&pOut->sDesc,"WebAssembly (wasm) binary module version 0x%u (MVP)",` |
|     3 | 1269 | `			FinfoLe32(z+4));` |
|     7 | 1270 | `		pOut->zMime = "application/wasm";` |
|     7 | 1271 | `		pOut->zExt = "wasm";` |
|     7 | 1272 | `		return 1;` |
|     - | 1273 | `	}` |
|   475 | 1274 | `	if( FINFO_AT(z,n,0,"MZ") ){` |
|   ! 0 | 1275 | `		sxu32 nPe = (n >= 64) ? FinfoLe32(z+60) : 0;` |
|   ! 0 | 1276 | `		if( nPe > 0 && FINFO_AT(z,n,nPe,"PE\0\0") && nPe <= n && n - nPe >= 26 ){` |
|   ! 0 | 1277 | `			sxu32 nMachine = FinfoLe16(z + nPe + 4);` |
|   ! 0 | 1278 | `			sxu32 nSect = FinfoLe16(z + nPe + 6);` |
|   ! 0 | 1279 | `			sxu32 nChar = FinfoLe16(z + nPe + 22);` |
|   ! 0 | 1280 | `			sxu32 nMagic = FinfoLe16(z + nPe + 24);` |
|   ! 0 | 1281 | `			SyBlobFormat(&pOut->sDesc,"PE32%s executable",nMagic == 0x20B ? "+" : "");` |
|   ! 0 | 1282 | `			if( n - nPe >= 76 ){` |
|   ! 0 | 1283 | `				SyBlobFormat(&pOut->sDesc," for MS Windows %u.%s%u",` |
|   ! 0 | 1284 | `					(sxu32)FinfoLe16(z + nPe + 72),` |
|   ! 0 | 1285 | `					FinfoLe16(z + nPe + 74) < 10 ? "0" : "",` |
|   ! 0 | 1286 | `					(sxu32)FinfoLe16(z + nPe + 74));` |
|   ! 0 | 1287 | `			}` |
|   ! 0 | 1288 | `			if( nChar & 0x2000 ){` |
|   ! 0 | 1289 | `				FinfoSay(pOut," (DLL)");` |
|   ! 0 | 1290 | `			}` |
|   ! 0 | 1291 | `			if( nMachine == 0x8664 ){` |
|   ! 0 | 1292 | `				FinfoSay(pOut,", x86-64");` |
|   ! 0 | 1293 | `			}else if( nMachine == 0x14C ){` |
|   ! 0 | 1294 | `				FinfoSay(pOut,", Intel i386");` |
|   ! 0 | 1295 | `			}else if( nMachine == 0xAA64 ){` |
|   ! 0 | 1296 | `				FinfoSay(pOut,", Aarch64");` |
|   ! 0 | 1297 | `			}` |
|   ! 0 | 1298 | `			SyBlobFormat(&pOut->sDesc,", %u sections",nSect);` |
|   ! 0 | 1299 | `			pOut->zMime = "application/vnd.microsoft.portable-executable";` |
|   ! 0 | 1300 | `			pOut->zExt = (nChar & 0x2000) ? "dll/cpl/tlb/ocx/acm/ax/ime" : "exe";` |
|   ! 0 | 1301 | `		}else{` |
|   ! 0 | 1302 | `			FinfoSay(pOut,"MS-DOS executable, MZ for MS-DOS");` |
|   ! 0 | 1303 | `			pOut->zMime = "application/x-dosexec";` |
|   ! 0 | 1304 | `			pOut->zExt = "exe/com/vlm/drv";` |
|     - | 1305 | `		}` |
|   ! 0 | 1306 | `		return 1;` |
|     - | 1307 | `	}` |
|   475 | 1308 | `	if( FINFO_AT(z,n,0,"!<arch>\n") ){` |
|     5 | 1309 | `		if( FINFO_AT(z,n,8,"debian-binary") ){` |
|   ! 0 | 1310 | `			FinfoSay(pOut,"Debian binary package");` |
|   ! 0 | 1311 | `			pOut->zMime = "application/vnd.debian.binary-package";` |
|   ! 0 | 1312 | `			pOut->zExt = "deb/udeb";` |
|   ! 0 | 1313 | `		}else{` |
|     5 | 1314 | `			FinfoSay(pOut,"current ar archive");` |
|     5 | 1315 | `			pOut->zMime = "application/x-archive";` |
|     5 | 1316 | `			pOut->zExt = "a/lib/ar";` |
|     - | 1317 | `		}` |
|     5 | 1318 | `		return 1;` |
|     - | 1319 | `	}` |
|   471 | 1320 | `	if( FINFO_AT(z,n,0,"SQLite format 3\000") ){` |
|     7 | 1321 | `		FinfoSay(pOut,"SQLite 3.x database");` |
|     7 | 1322 | `		if( n >= 100 ){` |
|     - | 1323 | `			static const char *azEnc[] = { "unknown 0", "UTF-8", "UTF-16le", "UTF-16be" };` |
|     7 | 1324 | `			sxu32 nEnc = FinfoBe32(z+56);` |
|    13 | 1325 | `			SyBlobFormat(&pOut->sDesc,` |
|     - | 1326 | `				", last written using SQLite version %u, file counter %u, database pages %u, "` |
|     - | 1327 | `				"cookie %u, schema %u, %s encoding, version-valid-for %u",` |
|     3 | 1328 | `				FinfoBe32(z+96),FinfoBe32(z+24),FinfoBe32(z+28),FinfoBe32(z+40),` |
|     6 | 1329 | `				FinfoBe32(z+44),azEnc[nEnc < 4 ? nEnc : 0],FinfoBe32(z+92));` |
|     3 | 1330 | `		}` |
|     7 | 1331 | `		pOut->zMime = "application/vnd.sqlite3";` |
|     7 | 1332 | `		pOut->zExt = "/sqlite/sqlite3/db/db3/dbe/sdb/help/ide/localstorage/sqlar/xowa/mbtiles";` |
|     7 | 1333 | `		return 1;` |
|     - | 1334 | `	}` |
|   465 | 1335 | `	if( FINFO_AT(z,n,257,"ustar") && n >= 512 ){` |
|     - | 1336 | `		/* php validates the header CHECKSUM before it believes any of this --` |
|     - | 1337 | `		 * the field is the octal sum of the 512-byte record with the field` |
|     - | 1338 | ``		 * itself read as spaces -- so a file that merely carries `ustar` at`` |
|     - | 1339 | `		 * offset 257 is not an archive. */` |
|   ! 0 | 1340 | `		sxu32 i,nSum = 0,nWant = 0;` |
|   ! 0 | 1341 | `		int bDigits = 0;` |
|   ! 0 | 1342 | `		for( i = 0 ; i < 512 ; ++i ){` |
|   ! 0 | 1343 | `			nSum += (i >= 148 && i < 156) ? (sxu32)' ' : (sxu32)z[i];` |
|   ! 0 | 1344 | `		}` |
|   ! 0 | 1345 | `		for( i = 148 ; i < 156 ; ++i ){` |
|   ! 0 | 1346 | `			if( z[i] >= '0' && z[i] <= '7' ){` |
|   ! 0 | 1347 | `				nWant = nWant * 8 + (sxu32)(z[i] - '0');` |
|   ! 0 | 1348 | `				bDigits = 1;` |
|   ! 0 | 1349 | `			}else if( z[i] == ' ' \|\| z[i] == 0 ){` |
|   ! 0 | 1350 | `				if( bDigits ){` |
|   ! 0 | 1351 | `					break;` |
|     - | 1352 | `				}` |
|   ! 0 | 1353 | `			}else{` |
|   ! 0 | 1354 | `				bDigits = 0;` |
|   ! 0 | 1355 | `				break;` |
|     - | 1356 | `			}` |
|   ! 0 | 1357 | `		}` |
|   ! 0 | 1358 | `		if( bDigits && nSum == nWant ){` |
|   ! 0 | 1359 | `			if( FINFO_AT(z,n,257,"ustar  \000") ){` |
|   ! 0 | 1360 | `				FinfoSay(pOut,"POSIX tar archive (GNU)");` |
|   ! 0 | 1361 | `			}else{` |
|   ! 0 | 1362 | `				FinfoSay(pOut,"POSIX tar archive");` |
|     - | 1363 | `			}` |
|   ! 0 | 1364 | `			pOut->zMime = "application/x-tar";` |
|   ! 0 | 1365 | `			pOut->zExt = "tar/gtar";` |
|   ! 0 | 1366 | `			return 1;` |
|     - | 1367 | `		}` |
|   ! 0 | 1368 | `	}` |
|   465 | 1369 | `	if( FINFO_AT(z,n,0,"OTTO") ){` |
|     5 | 1370 | `		FinfoSay(pOut,"OpenType font data");` |
|     5 | 1371 | `		pOut->zMime = "application/vnd.ms-opentype";` |
|     5 | 1372 | `		return 1;` |
|     - | 1373 | `	}` |
|   461 | 1374 | `	if( FINFO_AT(z,n,0,"\000\001\000\000\000") ){` |
|     5 | 1375 | `		FinfoSay(pOut,"TrueType Font data");` |
|     5 | 1376 | `		if( n >= 16 ){` |
|     5 | 1377 | `			SyBlobFormat(&pOut->sDesc,", %u tables, 1st \"%.4s\"",FinfoBe16(z+4),(const char *)(z+12));` |
|     2 | 1378 | `		}` |
|     5 | 1379 | `		pOut->zMime = "font/sfnt";` |
|     5 | 1380 | `		pOut->zExt = "ttf/tte";` |
|     5 | 1381 | `		return 1;` |
|     - | 1382 | `	}` |
|   457 | 1383 | `	if( FINFO_AT(z,n,0,"wOFF") && n >= 24 ){` |
|     5 | 1384 | `		FinfoSay(pOut,"Web Open Font Format");` |
|     5 | 1385 | `		FinfoSay(pOut,FINFO_AT(z,n,4,"OTTO") ? ", CFF" : ", TrueType");` |
|     7 | 1386 | `		SyBlobFormat(&pOut->sDesc,", length %u, version %u.%u",` |
|     2 | 1387 | `			FinfoBe32(z+8),FinfoBe16(z+20),FinfoBe16(z+22));` |
|     5 | 1388 | `		pOut->zMime = "font/woff";` |
|     5 | 1389 | `		return 1;` |
|     - | 1390 | `	}` |
|   453 | 1391 | `	if( FINFO_AT(z,n,0,"wOF2") && n >= 28 ){` |
|   ! 0 | 1392 | `		FinfoSay(pOut,"Web Open Font Format (Version 2)");` |
|   ! 0 | 1393 | `		FinfoSay(pOut,FINFO_AT(z,n,4,"OTTO") ? ", CFF" : ", TrueType");` |
|   ! 0 | 1394 | `		SyBlobFormat(&pOut->sDesc,", length %u, version %u.%u",` |
|   ! 0 | 1395 | `			FinfoBe32(z+8),FinfoBe16(z+24),FinfoBe16(z+26));` |
|   ! 0 | 1396 | `		pOut->zMime = "font/woff2";` |
|   ! 0 | 1397 | `		pOut->zExt = "woff2";` |
|   ! 0 | 1398 | `		return 1;` |
|     - | 1399 | `	}` |
|   450 | 1400 | `	if( FINFO_AT(z,n,0,"8BPS") && n >= 26 && FinfoBe16(z+4) == 1` |
|   ! 0 | 1401 | `	 && FinfoBe16(z+12) >= 1 && FinfoBe16(z+12) <= 56` |
|     3 | 1402 | `	 && FinfoBe32(z+14) > 0 && FinfoBe32(z+18) > 0 ){` |
|     - | 1403 | `		static const char *azMode[10] = { "Bitmap", "Grayscale", "Indexed", "RGB", "CMYK",` |
|     - | 1404 | `		                                  0, 0, "Multichannel", "Duotone", "Lab" };` |
|   ! 0 | 1405 | `		sxu32 nMode = FinfoBe16(z+24);` |
|   ! 0 | 1406 | `		SyBlobFormat(&pOut->sDesc,"Adobe Photoshop Image, %u x %u",` |
|   ! 0 | 1407 | `			FinfoBe32(z+14),FinfoBe32(z+18));` |
|   ! 0 | 1408 | `		if( nMode < SX_ARRAYSIZE(azMode) && azMode[nMode] ){` |
|   ! 0 | 1409 | `			SyBlobFormat(&pOut->sDesc,", %s",azMode[nMode]);` |
|   ! 0 | 1410 | `		}` |
|   ! 0 | 1411 | `		SyBlobFormat(&pOut->sDesc,", %ux %u-bit channels",FinfoBe16(z+12),FinfoBe16(z+22));` |
|   ! 0 | 1412 | `		pOut->zMime = "image/vnd.adobe.photoshop";` |
|   ! 0 | 1413 | `		pOut->zExt = "psd";` |
|   ! 0 | 1414 | `		return 1;` |
|     - | 1415 | `	}` |
|   453 | 1416 | `	if( FINFO_AT(z,n,0,"\320\317\021\340\241\261\032\341") && n >= 512 ){` |
|   ! 0 | 1417 | `		FinfoSay(pOut,"Composite Document File V2 Document");` |
|   ! 0 | 1418 | `		pOut->zMime = "application/x-ole-storage";` |
|   ! 0 | 1419 | `		return 1;` |
|     - | 1420 | `	}` |
|   450 | 1421 | `	if( FINFO_AT(z,n,0,"\000\000\001\000") && n >= 22 && FinfoLe16(z+4) > 0` |
|     7 | 1422 | `	 && FinfoLe32(z+18) <= n && FinfoLe32(z+14) <= n - FinfoLe32(z+18) ){` |
|     - | 1423 | `		/* php believes an icon directory only when the first image it points at` |
|     - | 1424 | ``		 * is really there, so a bare 22-byte header is `data` under both. */`` |
|     5 | 1425 | `		sxu32 nIcon = FinfoLe16(z+4),i,nShow;` |
|     5 | 1426 | `		SyBlobFormat(&pOut->sDesc,"MS Windows icon resource - %u icon%s",nIcon,nIcon == 1 ? "" : "s");` |
|     5 | 1427 | `		nShow = nIcon > 2 ? 2 : nIcon;` |
|     9 | 1428 | `		for( i = 0 ; i < nShow ; ++i ){` |
|     5 | 1429 | `			sxu32 nAt = 6 + i*16;` |
|     5 | 1430 | `			if( nAt + 16 > n ){` |
|   ! 0 | 1431 | `				break;` |
|     - | 1432 | `			}` |
|     5 | 1433 | `			SyBlobFormat(&pOut->sDesc,", %ux%u, %u bits/pixel",` |
|     4 | 1434 | `				z[nAt] ? (sxu32)z[nAt] : 256u,z[nAt+1] ? (sxu32)z[nAt+1] : 256u,` |
|     4 | 1435 | `				FinfoLe16(z+nAt+6));` |
|     3 | 1436 | `		}` |
|     5 | 1437 | `		pOut->zMime = "image/vnd.microsoft.icon";` |
|     5 | 1438 | `		pOut->zExt = "ico";` |
|     5 | 1439 | `		return 1;` |
|     - | 1440 | `	}` |
|   449 | 1441 | `	return 0;` |
|   323 | 1442 | `}` |
|     - | 1443 | `/*` |
|     - | 1444 | ` * ---------------------------------------------------------------------------` |
|     - | 1445 | ` * The TEXT formats` |
|     - | 1446 | ` * ---------------------------------------------------------------------------` |
|     - | 1447 | ` * These are the rows libmagic keeps in the text half of its database: they` |
|     - | 1448 | ` * match on characters rather than bytes, and their description is a PREFIX` |
|     - | 1449 | ` * that the text analysis finishes -- which is why a php file reads` |
|     - | 1450 | `` * `PHP script, ASCII text` and the same file saved as UTF-8 with a BOM reads`` |
|     - | 1451 | `` * `PHP script, Unicode text, UTF-8 (with BOM) text`.`` |
|     - | 1452 | ` */` |
|     - | 1453 | `/* A strict JSON reader: libmagic accepts an OBJECT or an ARRAY and nothing` |
|     - | 1454 | `` * else, so a bare `123` or `"x"` stays plain text. */`` |
|     - | 1455 | `static int FinfoJsonValue(const unsigned char *z,sxu32 n,sxu32 *pi,int nDepth);` |
|   420 | 1456 | `static void FinfoJsonSpace(const unsigned char *z,sxu32 n,sxu32 *pi)` |
|     3 | 1457 | `{` |
|   645 | 1458 | `	while( *pi < n && (z[*pi] == ' ' \|\| z[*pi] == '\t' \|\| z[*pi] == '\r' \|\| z[*pi] == '\n') ){` |
|    13 | 1459 | `		(*pi)++;` |
|     1 | 1460 | `	}` |
|   423 | 1461 | `}` |
|    18 | 1462 | `static int FinfoJsonString(const unsigned char *z,sxu32 n,sxu32 *pi)` |
|     1 | 1463 | `{` |
|    19 | 1464 | `	sxu32 i = *pi;` |
|    19 | 1465 | `	if( i >= n \|\| z[i] != '"' ){` |
|   ! 0 | 1466 | `		return 0;` |
|     - | 1467 | `	}` |
|    37 | 1468 | `	for( i++ ; i < n ; ++i ){` |
|    37 | 1469 | `		if( z[i] == '\\' ){` |
|   ! 0 | 1470 | `			i++;` |
|   ! 0 | 1471 | `			continue;` |
|     - | 1472 | `		}` |
|    37 | 1473 | `		if( z[i] == '"' ){` |
|    19 | 1474 | `			*pi = i + 1;` |
|    19 | 1475 | `			return 1;` |
|     - | 1476 | `		}` |
|    19 | 1477 | `		if( z[i] < 0x20 ){` |
|   ! 0 | 1478 | `			return 0;` |
|     - | 1479 | `		}` |
|    10 | 1480 | `	}` |
|   ! 0 | 1481 | `	return 0;` |
|    10 | 1482 | `}` |
|    36 | 1483 | `static int FinfoJsonNumber(const unsigned char *z,sxu32 n,sxu32 *pi)` |
|     1 | 1484 | `{` |
|    37 | 1485 | `	sxu32 i = *pi,nStart;` |
|    37 | 1486 | `	if( i < n && z[i] == '-' ){` |
|   ! 0 | 1487 | `		i++;` |
|   ! 0 | 1488 | `	}` |
|    37 | 1489 | `	nStart = i;` |
|    67 | 1490 | `	while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|    31 | 1491 | `		i++;` |
|     1 | 1492 | `	}` |
|    37 | 1493 | `	if( i == nStart ){` |
|     7 | 1494 | `		return 0;` |
|     - | 1495 | `	}` |
|    31 | 1496 | `	if( i < n && z[i] == '.' ){` |
|   ! 0 | 1497 | `		i++;` |
|   ! 0 | 1498 | `		nStart = i;` |
|   ! 0 | 1499 | `		while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|   ! 0 | 1500 | `			i++;` |
|   ! 0 | 1501 | `		}` |
|   ! 0 | 1502 | `		if( i == nStart ){` |
|   ! 0 | 1503 | `			return 0;` |
|     - | 1504 | `		}` |
|   ! 0 | 1505 | `	}` |
|    31 | 1506 | `	if( i < n && (z[i] == 'e' \|\| z[i] == 'E') ){` |
|   ! 0 | 1507 | `		i++;` |
|   ! 0 | 1508 | `		if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|   ! 0 | 1509 | `			i++;` |
|   ! 0 | 1510 | `		}` |
|   ! 0 | 1511 | `		nStart = i;` |
|   ! 0 | 1512 | `		while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|   ! 0 | 1513 | `			i++;` |
|   ! 0 | 1514 | `		}` |
|   ! 0 | 1515 | `		if( i == nStart ){` |
|   ! 0 | 1516 | `			return 0;` |
|     - | 1517 | `		}` |
|   ! 0 | 1518 | `	}` |
|    31 | 1519 | `	*pi = i;` |
|    31 | 1520 | `	return 1;` |
|    19 | 1521 | `}` |
|    72 | 1522 | `static int FinfoJsonValue(const unsigned char *z,sxu32 n,sxu32 *pi,int nDepth)` |
|     1 | 1523 | `{` |
|    73 | 1524 | `	if( nDepth > 128 ){` |
|   ! 0 | 1525 | `		return 0;` |
|     - | 1526 | `	}` |
|    73 | 1527 | `	FinfoJsonSpace(z,n,pi);` |
|    73 | 1528 | `	if( *pi >= n ){` |
|   ! 0 | 1529 | `		return 0;` |
|     - | 1530 | `	}` |
|    73 | 1531 | `	switch( z[*pi] ){` |
|    12 | 1532 | `		case '{': {` |
|    25 | 1533 | `			(*pi)++;` |
|    25 | 1534 | `			FinfoJsonSpace(z,n,pi);` |
|    25 | 1535 | `			if( *pi < n && z[*pi] == '}' ){` |
|     7 | 1536 | `				(*pi)++;` |
|     7 | 1537 | `				return 1;` |
|     - | 1538 | `			}` |
|     9 | 1539 | `			for(;;){` |
|    19 | 1540 | `				FinfoJsonSpace(z,n,pi);` |
|    19 | 1541 | `				if( !FinfoJsonString(z,n,pi) ){` |
|   ! 0 | 1542 | `					return 0;` |
|     - | 1543 | `				}` |
|    19 | 1544 | `				FinfoJsonSpace(z,n,pi);` |
|    19 | 1545 | `				if( *pi >= n \|\| z[*pi] != ':' ){` |
|   ! 0 | 1546 | `					return 0;` |
|     - | 1547 | `				}` |
|    19 | 1548 | `				(*pi)++;` |
|    19 | 1549 | `				if( !FinfoJsonValue(z,n,pi,nDepth+1) ){` |
|     7 | 1550 | `					return 0;` |
|     - | 1551 | `				}` |
|    13 | 1552 | `				FinfoJsonSpace(z,n,pi);` |
|    13 | 1553 | `				if( *pi < n && z[*pi] == ',' ){` |
|   ! 0 | 1554 | `					(*pi)++;` |
|   ! 0 | 1555 | `					continue;` |
|     - | 1556 | `				}` |
|    13 | 1557 | `				if( *pi < n && z[*pi] == '}' ){` |
|    13 | 1558 | `					(*pi)++;` |
|    13 | 1559 | `					return 1;` |
|     - | 1560 | `				}` |
|   ! 0 | 1561 | `				return 0;` |
|   ! 0 | 1562 | `			}` |
|     - | 1563 | `		}` |
|     6 | 1564 | `		case '[': {` |
|    13 | 1565 | `			(*pi)++;` |
|    13 | 1566 | `			FinfoJsonSpace(z,n,pi);` |
|    13 | 1567 | `			if( *pi < n && z[*pi] == ']' ){` |
|   ! 0 | 1568 | `				(*pi)++;` |
|   ! 0 | 1569 | `				return 1;` |
|     - | 1570 | `			}` |
|     6 | 1571 | `			for(;;){` |
|    37 | 1572 | `				if( !FinfoJsonValue(z,n,pi,nDepth+1) ){` |
|   ! 0 | 1573 | `					return 0;` |
|     - | 1574 | `				}` |
|    37 | 1575 | `				FinfoJsonSpace(z,n,pi);` |
|    37 | 1576 | `				if( *pi < n && z[*pi] == ',' ){` |
|    25 | 1577 | `					(*pi)++;` |
|    25 | 1578 | `					continue;` |
|     - | 1579 | `				}` |
|    13 | 1580 | `				if( *pi < n && z[*pi] == ']' ){` |
|    13 | 1581 | `					(*pi)++;` |
|    13 | 1582 | `					return 1;` |
|     - | 1583 | `				}` |
|   ! 0 | 1584 | `				return 0;` |
|   ! 0 | 1585 | `			}` |
|     - | 1586 | `		}` |
|   ! 0 | 1587 | `		case '"': return FinfoJsonString(z,n,pi);` |
|   ! 0 | 1588 | `		case 't': if( FinfoAt(z,n,*pi,"true",4) ){ *pi += 4; return 1; } return 0;` |
|   ! 0 | 1589 | `		case 'f': if( FinfoAt(z,n,*pi,"false",5) ){ *pi += 5; return 1; } return 0;` |
|   ! 0 | 1590 | `		case 'n': if( FinfoAt(z,n,*pi,"null",4) ){ *pi += 4; return 1; } return 0;` |
|    37 | 1591 | `		default:  return FinfoJsonNumber(z,n,pi);` |
|     - | 1592 | `	}` |
|    37 | 1593 | `}` |
|   216 | 1594 | `static int FinfoIsJson(const unsigned char *z,sxu32 n)` |
|     3 | 1595 | `{` |
|   219 | 1596 | `	sxu32 i = 0;` |
|   219 | 1597 | `	FinfoJsonSpace(z,n,&i);` |
|   219 | 1598 | `	if( i >= n \|\| (z[i] != '{' && z[i] != '[') ){` |
|   201 | 1599 | `		return 0;` |
|     - | 1600 | `	}` |
|    19 | 1601 | `	if( !FinfoJsonValue(z,n,&i,0) ){` |
|     7 | 1602 | `		return 0;` |
|     - | 1603 | `	}` |
|    13 | 1604 | `	FinfoJsonSpace(z,n,&i);` |
|    13 | 1605 | `	return i >= n;` |
|   111 | 1606 | `}` |
|     - | 1607 | `/*` |
|     - | 1608 | ` * libmagic's CSV reader, as its answers describe it: at least TWO complete` |
|     - | 1609 | ` * records, every one of them the same number of fields, at least three of` |
|     - | 1610 | ` * them, comma-separated, and the last record terminated -- a file whose final` |
|     - | 1611 | ` * line has no newline is not a CSV, which is measurable and reproduced.` |
|     - | 1612 | ` */` |
|   204 | 1613 | `static int FinfoIsCsv(const unsigned char *z,sxu32 n)` |
|     3 | 1614 | `{` |
|   207 | 1615 | `	sxu32 i = 0,nRec = 0,nField = 0,nWant = 0;` |
|   207 | 1616 | `	int bQuote = 0,bAny = 0;` |
|   207 | 1617 | `	if( n < 4 ){` |
|    13 | 1618 | `		return 0;` |
|     - | 1619 | `	}` |
|   195 | 1620 | `	nField = 1;` |
|  9537 | 1621 | `	for( i = 0 ; i < n ; ++i ){` |
|  9469 | 1622 | `		if( bQuote ){` |
|    25 | 1623 | `			if( z[i] == '"' ){` |
|    13 | 1624 | `				if( i + 1 < n && z[i+1] == '"' ){` |
|   ! 0 | 1625 | `					i++;` |
|   ! 0 | 1626 | `					continue;` |
|     - | 1627 | `				}` |
|    13 | 1628 | `				bQuote = 0;` |
|     6 | 1629 | `			}` |
|    25 | 1630 | `			continue;` |
|     - | 1631 | `		}` |
|  9445 | 1632 | `		if( z[i] == '"' ){` |
|    13 | 1633 | `			bQuote = 1;` |
|    13 | 1634 | `			bAny = 1;` |
|    13 | 1635 | `			continue;` |
|     - | 1636 | `		}` |
|  9433 | 1637 | `		if( z[i] == ',' ){` |
|    73 | 1638 | `			nField++;` |
|    73 | 1639 | `			continue;` |
|     - | 1640 | `		}` |
|  9361 | 1641 | `		if( z[i] == '\n' ){` |
|   150 | 1642 | `			if( nField < 3 ){` |
|   126 | 1643 | `				return 0;` |
|     - | 1644 | `			}` |
|    25 | 1645 | `			if( nRec == 0 ){` |
|    19 | 1646 | `				nWant = nField;` |
|    16 | 1647 | `			}else if( nField != nWant ){` |
|   ! 0 | 1648 | `				return 0;` |
|     - | 1649 | `			}` |
|    25 | 1650 | `			nRec++;` |
|    25 | 1651 | `			nField = 1;` |
|    25 | 1652 | `			bAny = 0;` |
|    25 | 1653 | `			continue;` |
|     - | 1654 | `		}` |
|  9213 | 1655 | `		if( z[i] == '\r' ){` |
|    31 | 1656 | `			continue;` |
|     - | 1657 | `		}` |
|  9183 | 1658 | `		bAny = 1;` |
|  4593 | 1659 | `	}` |
|    71 | 1660 | `	if( bQuote \|\| bAny \|\| nField != 1 ){` |
|    65 | 1661 | `		return 0;   /* an unterminated final record */` |
|     - | 1662 | `	}` |
|     7 | 1663 | `	return nRec >= 2;` |
|   105 | 1664 | `}` |
|     - | 1665 | `/*` |
|     - | 1666 | `` * The `#!` line. Which interpreters get a name of their own is the database's`` |
|     - | 1667 | ` * business rather than a rule, and the two lists are not the same: a direct` |
|     - | 1668 | `` * path knows sh/ksh/csh/awk/php and an `env` line does not, while both know`` |
|     - | 1669 | ` * bash, zsh, python, perl, ruby, node, lua and tclsh. Anything else is` |
|     - | 1670 | `` * libmagic's generic `a %s script`, whose %s is the whole first word for a`` |
|     - | 1671 | `` * direct path and just the interpreter word after an `env`.`` |
|     - | 1672 | ` */` |
|   452 | 1673 | `static int FinfoShebang(const unsigned char *z,sxu32 n,FinfoAnswer *pOut)` |
|     3 | 1674 | `{` |
|     - | 1675 | `	static const struct {` |
|     - | 1676 | `		const char *zName; const char *zDesc; const char *zEnvDesc; const char *zMime; int bEnv;` |
|     - | 1677 | `	} aInterp[] = {` |
|     - | 1678 | `		{ "sh",      "POSIX shell script",         0,             "text/x-shellscript",   0 },` |
|     - | 1679 | `		{ "bash",    "Bourne-Again shell script",  0,             "text/x-shellscript",   1 },` |
|     - | 1680 | `		{ "zsh",     "Paul Falstad's zsh script",  0,             "text/x-shellscript",   1 },` |
|     - | 1681 | `		{ "ksh",     "Korn shell script",          0,             "text/x-shellscript",   0 },` |
|     - | 1682 | `		{ "csh",     "C shell script",             0,             "text/x-shellscript",   0 },` |
|     - | 1683 | `		{ "tcsh",    "Tenex C shell script",       0,             "text/x-shellscript",   0 },` |
|     - | 1684 | `		{ "php",     "PHP script",                 0,             "text/x-php",           0 },` |
|     - | 1685 | `		{ "python",  "Python script",              0,             "text/x-script.python", 1 },` |
|     - | 1686 | `		{ "ruby",    "Ruby script",                0,             "text/x-ruby",          1 },` |
|     - | 1687 | `		{ "node",    "Node.js script executable, ",0,             "application/javascript", 1 },` |
|     - | 1688 | `		{ "awk",     "awk script",                 0,             "text/x-awk",           0 },` |
|     - | 1689 | `		{ "gawk",    "awk script",                 0,             "text/x-awk",           0 },` |
|     - | 1690 | `		{ "lua",     "Lua script",                 0,             "text/x-lua",           1 },` |
|     - | 1691 | `		/* tclsh is the one name the two doors describe differently, which is a` |
|     - | 1692 | `		 * fact about the database rather than a rule: a direct path reads` |
|     - | 1693 | ``		 * `Tcl/Tk script` and the env form `Tcl script`. */`` |
|     - | 1694 | `		{ "tclsh",   "Tcl/Tk script",              "Tcl script",  "text/x-tcl",           1 },` |
|     - | 1695 | `		{ "wish",    "Tcl/Tk script",              "Tcl script",  "text/x-tcl",           1 },` |
|     - | 1696 | `	};` |
|     - | 1697 | `	sxu32 i,nStart,nEnd,nWordStart,nWordEnd;` |
|   455 | 1698 | `	int bEnv = 0;` |
|     - | 1699 | `	const char *zWord;` |
|     - | 1700 | `	sxu32 nWord;` |
|   455 | 1701 | `	if( n < 3 \|\| z[0] != '#' \|\| z[1] != '!' ){` |
|   311 | 1702 | `		return 0;` |
|     - | 1703 | `	}` |
|   145 | 1704 | `	i = 2;` |
|   223 | 1705 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t') ){` |
|     7 | 1706 | `		i++;` |
|     1 | 1707 | `	}` |
|   145 | 1708 | `	nStart = i;` |
|  1753 | 1709 | `	while( i < n && z[i] != ' ' && z[i] != '\t' && z[i] != '\n' && z[i] != '\r' ){` |
|  1609 | 1710 | `		i++;` |
|     1 | 1711 | `	}` |
|   145 | 1712 | `	nEnd = i;` |
|   145 | 1713 | `	if( nEnd == nStart ){` |
|   ! 0 | 1714 | `		return 0;` |
|     - | 1715 | `	}` |
|     - | 1716 | ``	/* `#!/usr/bin/env foo` -- and libmagic takes the NEXT word whatever lies`` |
|     - | 1717 | ``	 * between, newline included, which is why `#!/usr/bin/env` alone answers`` |
|     - | 1718 | `	 * with the first word of the line under it. */` |
|   144 | 1719 | `	if( (nEnd - nStart >= 4 && SyMemcmp(z + nEnd - 4,"/env",4) == 0)` |
|   127 | 1720 | `	 \|\| (nEnd - nStart == 3 && SyMemcmp(z + nStart,"env",3) == 0) ){` |
|    91 | 1721 | `		while( i < n && (z[i] == ' ' \|\| z[i] == '\t' \|\| z[i] == '\n' \|\| z[i] == '\r') ){` |
|    37 | 1722 | `			i++;` |
|     1 | 1723 | `		}` |
|    37 | 1724 | `		nStart = i;` |
|   187 | 1725 | `		while( i < n && z[i] != ' ' && z[i] != '\t' && z[i] != '\n' && z[i] != '\r' ){` |
|   151 | 1726 | `			i++;` |
|     1 | 1727 | `		}` |
|    37 | 1728 | `		nEnd = i;` |
|    37 | 1729 | `		bEnv = 1;` |
|    37 | 1730 | `		if( nEnd == nStart ){` |
|   ! 0 | 1731 | `			return 0;` |
|     - | 1732 | `		}` |
|    18 | 1733 | `	}` |
|     - | 1734 | `	/* The BASENAME is what the table is keyed on, and a trailing version digit` |
|     - | 1735 | ``	 * is part of the name php recognises (`python3`, `php8`). */`` |
|   145 | 1736 | `	nWordStart = nStart;` |
|  1471 | 1737 | `	for( i = nStart ; i < nEnd ; ++i ){` |
|  1327 | 1738 | `		if( z[i] == '/' ){` |
|   277 | 1739 | `			nWordStart = i + 1;` |
|   138 | 1740 | `		}` |
|   664 | 1741 | `	}` |
|   145 | 1742 | `	nWordEnd = nEnd;` |
|   145 | 1743 | `	zWord = (const char *)(z + nWordStart);` |
|   145 | 1744 | `	nWord = nWordEnd - nWordStart;` |
|  1123 | 1745 | `	for( i = 0 ; i < SX_ARRAYSIZE(aInterp) ; ++i ){` |
|  1099 | 1746 | `		sxu32 nLen = (sxu32)SyStrlen(aInterp[i].zName);` |
|  1099 | 1747 | `		if( nWord < nLen \|\| SyMemcmp(zWord,aInterp[i].zName,nLen) != 0 ){` |
|   979 | 1748 | `			continue;` |
|     - | 1749 | `		}` |
|     - | 1750 | `` 		/* Only a VERSION may follow the name: `python3` is python, `phpunit` `` |
|     - | 1751 | `		 * is not php. */` |
|     - | 1752 | `		{` |
|     - | 1753 | `			sxu32 k;` |
|   121 | 1754 | `			int bOk = 1;` |
|   139 | 1755 | `			for( k = nLen ; k < nWord ; ++k ){` |
|    19 | 1756 | `				if( (zWord[k] < '0' \|\| zWord[k] > '9') && zWord[k] != '.' ){` |
|   ! 0 | 1757 | `					bOk = 0;` |
|   ! 0 | 1758 | `					break;` |
|     - | 1759 | `				}` |
|    10 | 1760 | `			}` |
|   121 | 1761 | `			if( !bOk ){` |
|   ! 0 | 1762 | `				continue;` |
|     - | 1763 | `			}` |
|     - | 1764 | `		}` |
|   121 | 1765 | `		if( bEnv && !aInterp[i].bEnv ){` |
|    19 | 1766 | `			break;   /* the env form does not know this one: fall to the generic row */` |
|     - | 1767 | `		}` |
|   103 | 1768 | `		FinfoSay(pOut,(bEnv && aInterp[i].zEnvDesc) ? aInterp[i].zEnvDesc : aInterp[i].zDesc);` |
|   103 | 1769 | `		pOut->zMime = aInterp[i].zMime;` |
|     - | 1770 | `		/* Node spells the whole thing itself: its row ends in the separator and` |
|     - | 1771 | ``		 * puts `executable` BEFORE the text rather than after it. */`` |
|   103 | 1772 | `		if( SyMemcmp(aInterp[i].zName,"node",4) == 0 ){` |
|     7 | 1773 | `			pOut->bText = 2;` |
|     4 | 1774 | `		}else{` |
|    97 | 1775 | `			pOut->bText = 1;` |
|    97 | 1776 | `			pOut->bExec = 1;` |
|     - | 1777 | `		}` |
|   103 | 1778 | `		return 1;` |
|   ! 0 | 1779 | `	}` |
|     - | 1780 | `	/* perl carries its whole sentence in the row -- no text analysis is` |
|     - | 1781 | `	 * appended to it at all, which no other interpreter does. */` |
|    43 | 1782 | `	if( nWord >= 4 && SyMemcmp(zWord,"perl",4) == 0 ){` |
|     7 | 1783 | `		FinfoSay(pOut,"Perl script text executable");` |
|     7 | 1784 | `		pOut->zMime = "text/x-perl";` |
|     7 | 1785 | `		return 1;` |
|     - | 1786 | `	}` |
|     - | 1787 | `	/* The generic row: the whole first word for a direct path, the interpreter` |
|     - | 1788 | ``	 * word alone after an `env`. */`` |
|    37 | 1789 | `	SyBlobFormat(&pOut->sDesc,"a %.*s script",(int)(nEnd - nStart),(const char *)(z + nStart));` |
|    37 | 1790 | `	pOut->zMime = "text/plain";` |
|    37 | 1791 | `	pOut->bText = 1;` |
|    37 | 1792 | `	pOut->bExec = 1;` |
|    37 | 1793 | `	return 1;` |
|   229 | 1794 | `}` |
|   452 | 1795 | `static int FinfoTextFormat(const unsigned char *z,sxu32 n,FinfoAnswer *pOut)` |
|     3 | 1796 | `{` |
|   455 | 1797 | `	sxu32 nWs = 0;` |
|   455 | 1798 | `	if( FinfoShebang(z,n,pOut) ){` |
|   145 | 1799 | `		return 1;` |
|     - | 1800 | `	}` |
|     - | 1801 | `	/* A UTF-8 BOM is not part of the text as far as these markers go: a php` |
|     - | 1802 | ``	 * file saved with one is still `PHP script`. */`` |
|   311 | 1803 | `	if( n > 3 && z[0] == 0xEF && z[1] == 0xBB && z[2] == 0xBF ){` |
|    13 | 1804 | `		z += 3;` |
|    13 | 1805 | `		n -= 3;` |
|     6 | 1806 | `	}` |
|   311 | 1807 | `	if( FINFO_FIND(z,n,4096,"<svg") >= 0 ){` |
|     7 | 1808 | `		FinfoSay(pOut,"SVG Scalable Vector Graphics image, ");` |
|     7 | 1809 | `		pOut->zMime = "image/svg+xml";` |
|     7 | 1810 | `		pOut->zExt = "svg";` |
|     7 | 1811 | `		pOut->bText = 2;` |
|     7 | 1812 | `		return 1;` |
|     - | 1813 | `	}` |
|   305 | 1814 | `	if( FINFO_AT(z,n,0,"<?xml") ){` |
|    19 | 1815 | `		sxi32 nVer = FINFO_FIND(z,n,64,"version=\"");` |
|    19 | 1816 | `		if( nVer >= 0 && (sxu32)nVer + 12 <= n ){` |
|    13 | 1817 | `			SyBlobFormat(&pOut->sDesc,"XML %.3s document, ",(const char *)(z + nVer + 9));` |
|     7 | 1818 | `		}else{` |
|     7 | 1819 | `			FinfoSay(pOut,"XML document, ");` |
|     - | 1820 | `		}` |
|    19 | 1821 | `		pOut->zMime = "text/xml";` |
|    19 | 1822 | `		pOut->bText = 2;` |
|    19 | 1823 | `		return 1;` |
|     - | 1824 | `	}` |
|   287 | 1825 | `	if( FINFO_ATI(z,n,0,"<?php") \|\| FINFO_AT(z,n,0,"<?\n") \|\| FINFO_AT(z,n,0,"<?\r") ){` |
|    22 | 1826 | `		FinfoSay(pOut,"PHP script, ");` |
|    22 | 1827 | `		pOut->zMime = "text/x-php";` |
|    22 | 1828 | `		pOut->bText = 2;` |
|    22 | 1829 | `		return 1;` |
|     - | 1830 | `	}` |
|   399 | 1831 | `	while( nWs < n && (z[nWs] == ' ' \|\| z[nWs] == '\t' \|\| z[nWs] == '\n' \|\| z[nWs] == '\r') ){` |
|   ! 0 | 1832 | `		nWs++;` |
|   ! 0 | 1833 | `	}` |
|   264 | 1834 | `	if( FINFO_ATI(z,n,nWs,"<!doctype") \|\| FINFO_ATI(z,n,nWs,"<html")` |
|   255 | 1835 | `	 \|\| FINFO_ATI(z,n,nWs,"<head") \|\| FINFO_ATI(z,n,nWs,"<title")` |
|   252 | 1836 | `	 \|\| FINFO_ATI(z,n,nWs,"<script") \|\| FINFO_ATI(z,n,nWs,"<style")` |
|   255 | 1837 | `	 \|\| FINFO_ATI(z,n,nWs,"<table") \|\| FINFO_ATI(z,n,nWs,"<a href=") ){` |
|    13 | 1838 | `		FinfoSay(pOut,"HTML document, ");` |
|    13 | 1839 | `		pOut->zMime = "text/html";` |
|    13 | 1840 | `		pOut->bText = 2;` |
|    13 | 1841 | `		return 1;` |
|     - | 1842 | `	}` |
|   255 | 1843 | `	if( FINFO_AT(z,n,0,"{\\rtf") && n >= 6 ){` |
|     7 | 1844 | `		SyBlobFormat(&pOut->sDesc,"Rich Text Format data, version %c",z[5]);` |
|     7 | 1845 | `		if( FINFO_FIND(z,n,64,"\\ansi") >= 0 ){` |
|     7 | 1846 | `			FinfoSay(pOut,", ANSI");` |
|     3 | 1847 | `		}else if( FINFO_FIND(z,n,64,"\\mac") >= 0 ){` |
|   ! 0 | 1848 | `			FinfoSay(pOut,", Apple Macintosh");` |
|   ! 0 | 1849 | `		}else if( FINFO_FIND(z,n,64,"\\pca") >= 0 ){` |
|   ! 0 | 1850 | `			FinfoSay(pOut,", IBM PS/2 codepage 850");` |
|   ! 0 | 1851 | `		}else if( FINFO_FIND(z,n,64,"\\pc") >= 0 ){` |
|   ! 0 | 1852 | `			FinfoSay(pOut,", IBM PC, code page 437");` |
|   ! 0 | 1853 | `		}` |
|     7 | 1854 | `		pOut->zMime = "text/rtf";` |
|     7 | 1855 | `		pOut->zExt = "rtf";` |
|     7 | 1856 | `		return 1;` |
|     - | 1857 | `	}` |
|   249 | 1858 | `	if( FINFO_AT(z,n,0,"%!") \|\| FINFO_AT(z,n,0,"\004%!") ){` |
|     7 | 1859 | `		FinfoSay(pOut,"PostScript document text");` |
|     7 | 1860 | `		if( FINFO_AT(z,n,0,"%!PS-Adobe-") && n >= 14 ){` |
|     7 | 1861 | `			SyBlobFormat(&pOut->sDesc," conforming DSC level %.3s",(const char *)(z + 11));` |
|     3 | 1862 | `		}` |
|     7 | 1863 | `		pOut->zMime = "application/postscript";` |
|     7 | 1864 | `		return 1;` |
|     - | 1865 | `	}` |
|   243 | 1866 | `	if( FINFO_AT(z,n,0,"--- ") && FINFO_FIND(z,n,4096,"\n+++ ") >= 0 ){` |
|     7 | 1867 | `		FinfoSay(pOut,"unified diff output, ");` |
|     7 | 1868 | `		pOut->zMime = "text/x-diff";` |
|     7 | 1869 | `		pOut->zExt = "diff/patch/dif/pch/rej";` |
|     7 | 1870 | `		pOut->bText = 2;` |
|     7 | 1871 | `		return 1;` |
|     - | 1872 | `	}` |
|   237 | 1873 | `	if( FINFO_AT(z,n,0,"*** ") && FINFO_FIND(z,n,4096,"\n--- ") >= 0 ){` |
|     7 | 1874 | `		FinfoSay(pOut,"context diff output, ");` |
|     7 | 1875 | `		pOut->zMime = "text/x-diff";` |
|     7 | 1876 | `		pOut->zExt = "diff/patch";` |
|     7 | 1877 | `		pOut->bText = 2;` |
|     7 | 1878 | `		return 1;` |
|     - | 1879 | `	}` |
|   231 | 1880 | `	if( FINFO_AT(z,n,0,"#EXTM3U") ){` |
|     7 | 1881 | `		FinfoSay(pOut,"M3U playlist, ");` |
|     7 | 1882 | `		pOut->zMime = "audio/x-mpegurl";` |
|     7 | 1883 | `		pOut->bText = 2;` |
|     7 | 1884 | `		return 1;` |
|     - | 1885 | `	}` |
|   222 | 1886 | `	if( FINFO_AT(z,n,0,"From:") \|\| FINFO_AT(z,n,0,"Return-Path:")` |
|   216 | 1887 | `	 \|\| FINFO_AT(z,n,0,"Received:") \|\| FINFO_AT(z,n,0,"Message-ID:")` |
|   219 | 1888 | `	 \|\| FINFO_AT(z,n,0,"Path:") ){` |
|     7 | 1889 | `		FinfoSay(pOut,"news or mail, ");` |
|     7 | 1890 | `		pOut->zMime = "message/rfc822";` |
|     7 | 1891 | `		pOut->bText = 2;` |
|     7 | 1892 | `		return 1;` |
|     - | 1893 | `	}` |
|   219 | 1894 | `	if( FinfoIsJson(z,n) ){` |
|    13 | 1895 | `		FinfoSay(pOut,"JSON text data");` |
|    13 | 1896 | `		pOut->zMime = "application/json";` |
|    13 | 1897 | `		return 1;` |
|     - | 1898 | `	}` |
|   207 | 1899 | `	if( FinfoIsCsv(z,n) ){` |
|     7 | 1900 | `		FinfoSay(pOut,"CSV ");` |
|     7 | 1901 | `		pOut->zMime = "text/csv";` |
|     7 | 1902 | `		pOut->bText = 3;` |
|     7 | 1903 | `		return 1;` |
|     - | 1904 | `	}` |
|     - | 1905 | `	/* The three JavaScript hints php's database actually fires on. Its` |
|     - | 1906 | ``	 * detection is a keyword lottery -- `function x(){}` is plain text under`` |
|     - | 1907 | `	 * php too -- so only the measured ones are here. */` |
|   198 | 1908 | `	if( FINFO_AT(z,n,nWs,"'use strict'") \|\| FINFO_AT(z,n,nWs,"\"use strict\"")` |
|   198 | 1909 | `	 \|\| FINFO_FIND(z,n,8192,"export default") >= 0` |
|   198 | 1910 | `	 \|\| FINFO_FIND(z,n,8192,"(typeof ") >= 0` |
|   201 | 1911 | `	 \|\| FINFO_FIND(z,n,8192,"jQuery(") >= 0 ){` |
|   ! 0 | 1912 | `		FinfoSay(pOut,"JavaScript source, ");` |
|   ! 0 | 1913 | `		pOut->zMime = "application/javascript";` |
|   ! 0 | 1914 | `		pOut->zExt = "js";` |
|   ! 0 | 1915 | `		pOut->bText = 2;` |
|   ! 0 | 1916 | `		return 1;` |
|     - | 1917 | `	}` |
|   201 | 1918 | `	return 0;` |
|   229 | 1919 | `}` |
|     - | 1920 | `/*` |
|     - | 1921 | ` * The whole reading of a buffer, in libmagic's own order: the two degenerate` |
|     - | 1922 | ` * sizes first, then the binary signatures, then the text ones, then the plain` |
|     - | 1923 | `` * text analysis -- and `data` for everything that is none of those.`` |
|     - | 1924 | ` */` |
|   668 | 1925 | `static void FinfoAnalyze(const unsigned char *z,sxu32 n,ph7_int64 nFile,` |
|     - | 1926 | `	FinfoAnswer *pOut,FinfoText *pTx)` |
|     3 | 1927 | `{` |
|   671 | 1928 | `	FinfoTextScan(z,n,pTx);` |
|   671 | 1929 | `	pOut->zMime = 0;` |
|   671 | 1930 | `	pOut->zExt = 0;` |
|   671 | 1931 | `	pOut->bText = 0;` |
|   671 | 1932 | `	pOut->bExec = 0;` |
|   671 | 1933 | `	if( n < 1 ){` |
|    12 | 1934 | `		pTx->iEnc = FINFO_ENC_BINARY;` |
|    12 | 1935 | `		FinfoSay(pOut,"empty");` |
|    12 | 1936 | `		pOut->zMime = "application/x-empty";` |
|    12 | 1937 | `		return;` |
|     - | 1938 | `	}` |
|   661 | 1939 | `	if( n < 2 ){` |
|     7 | 1940 | `		pTx->iEnc = FINFO_ENC_BINARY;` |
|     7 | 1941 | `		FinfoSay(pOut,"very short file (no magic)");` |
|     7 | 1942 | `		pOut->zMime = "application/octet-stream";` |
|     7 | 1943 | `		return;` |
|     - | 1944 | `	}` |
|   652 | 1945 | `	if( !FinfoGzip(z,n,nFile,pOut) && !FinfoBinary(z,n,nFile,pOut)` |
|   557 | 1946 | `	 && !(pTx->iEnc != FINFO_ENC_BINARY && FinfoTextFormat(z,n,pOut)) ){` |
|   217 | 1947 | `		if( pTx->iEnc == FINFO_ENC_BINARY ){` |
|    17 | 1948 | `			FinfoSay(pOut,"data");` |
|    17 | 1949 | `			pOut->zMime = "application/octet-stream";` |
|    17 | 1950 | `			return;` |
|     - | 1951 | `		}` |
|   201 | 1952 | `		pOut->bText = 2;` |
|   201 | 1953 | `		pOut->zMime = "text/plain";` |
|    99 | 1954 | `	}` |
|   639 | 1955 | `	if( pOut->bText ){` |
|   425 | 1956 | `		if( pOut->bText == 1 ){` |
|   133 | 1957 | `			SyBlobAppend(&pOut->sDesc,", ",2);` |
|    66 | 1958 | `		}` |
|   425 | 1959 | `		if( pOut->bText == 3 ){` |
|     7 | 1960 | `			FinfoSay(pOut,FinfoEncDesc(pTx->iEnc));` |
|     4 | 1961 | `		}else{` |
|   419 | 1962 | `			FinfoTextDesc(pTx,&pOut->sDesc);` |
|     - | 1963 | `		}` |
|   425 | 1964 | `		if( pOut->bExec ){` |
|   133 | 1965 | `			FinfoSay(pOut," executable");` |
|    66 | 1966 | `		}` |
|   211 | 1967 | `	}` |
|   639 | 1968 | `	if( pOut->zMime == 0 ){` |
|   ! 0 | 1969 | `		pOut->zMime = "application/octet-stream";` |
|   ! 0 | 1970 | `	}` |
|   337 | 1971 | `}` |
|     - | 1972 | `/*` |
|     - | 1973 | ` * Which of the three faces the flags ask for. libmagic checks them in this` |
|     - | 1974 | ` * order, and only one ever answers: EXTENSION, then the two MIME halves` |
|     - | 1975 | `` * (together they are `type; charset=encoding`), then the description.`` |
|     - | 1976 | ` */` |
|   668 | 1977 | `static void FinfoRender(const FinfoAnswer *pAns,const FinfoText *pTx,int iFlags,SyBlob *pOut)` |
|     3 | 1978 | `{` |
|   671 | 1979 | `	if( iFlags & FINFO_EXTENSION ){` |
|    10 | 1980 | `		SyBlobAppend(pOut,pAns->zExt ? pAns->zExt : "???",` |
|     6 | 1981 | `			(sxu32)SyStrlen(pAns->zExt ? pAns->zExt : "???"));` |
|     7 | 1982 | `		return;` |
|     - | 1983 | `	}` |
|   665 | 1984 | `	if( (iFlags & FINFO_MIME_TYPE) && (iFlags & FINFO_MIME_ENCODING) ){` |
|     7 | 1985 | `		SyBlobFormat(pOut,"%s; charset=%s",pAns->zMime,FinfoCharset(pTx->iEnc));` |
|     7 | 1986 | `		return;` |
|     - | 1987 | `	}` |
|   659 | 1988 | `	if( iFlags & FINFO_MIME_TYPE ){` |
|   235 | 1989 | `		SyBlobAppend(pOut,pAns->zMime,(sxu32)SyStrlen(pAns->zMime));` |
|   235 | 1990 | `		return;` |
|     - | 1991 | `	}` |
|   426 | 1992 | `	if( iFlags & FINFO_MIME_ENCODING ){` |
|   216 | 1993 | `		SyBlobAppend(pOut,FinfoCharset(pTx->iEnc),(sxu32)SyStrlen(FinfoCharset(pTx->iEnc)));` |
|   216 | 1994 | `		return;` |
|     - | 1995 | `	}` |
|   212 | 1996 | `	SyBlobAppend(pOut,SyBlobData(&pAns->sDesc),SyBlobLength(&pAns->sDesc));` |
|   337 | 1997 | `}` |
|     - | 1998 | `/*` |
|     - | 1999 | ` * ---------------------------------------------------------------------------` |
|     - | 2000 | ` * The php layer` |
|     - | 2001 | ` * ---------------------------------------------------------------------------` |
|     - | 2002 | ` */` |
|     - | 2003 | ``/* The flags an instance carries, or php's `Invalid finfo object` for one that`` |
|     - | 2004 | ` * was never constructed (ReflectionClass::newInstanceWithoutConstructor). */` |
|   690 | 2005 | `static int FinfoFlagsOf(ph7_context *pCtx,ph7_class_instance *pThis,int *piFlags)` |
|     3 | 2006 | `{` |
|     - | 2007 | `	SyString sAttr;` |
|     - | 2008 | `	ph7_value *pSlot;` |
|   693 | 2009 | `	if( pThis ){` |
|   693 | 2010 | `		SyStringInitFromBuf(&sAttr,FINFO_FLAGS_SLOT,sizeof(FINFO_FLAGS_SLOT)-1);` |
|   693 | 2011 | `		pSlot = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   693 | 2012 | `		if( pSlot && (pSlot->iFlags & MEMOBJ_INT) ){` |
|   689 | 2013 | `			*piFlags = (int)pSlot->x.iVal;` |
|   689 | 2014 | `			return 1;` |
|     - | 2015 | `		}` |
|     2 | 2016 | `	}` |
|     5 | 2017 | `	PH7_VmThrowException(pCtx,"Error","Invalid finfo object");` |
|     5 | 2018 | `	return 0;` |
|   348 | 2019 | `}` |
|    36 | 2020 | `static int FinfoSetFlagsOf(ph7_class_instance *pThis,int iFlags)` |
|     4 | 2021 | `{` |
|     - | 2022 | `	SyString sAttr;` |
|     - | 2023 | `	ph7_value *pSlot;` |
|    40 | 2024 | `	if( pThis == 0 ){` |
|   ! 0 | 2025 | `		return -1;` |
|     - | 2026 | `	}` |
|    40 | 2027 | `	SyStringInitFromBuf(&sAttr,FINFO_FLAGS_SLOT,sizeof(FINFO_FLAGS_SLOT)-1);` |
|    40 | 2028 | `	pSlot = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    40 | 2029 | `	if( pSlot == 0 ){` |
|   ! 0 | 2030 | `		return -1;` |
|     - | 2031 | `	}` |
|    40 | 2032 | `	PH7_MemObjRelease(pSlot);` |
|    40 | 2033 | `	pSlot->x.iVal = (ph7_int64)iFlags;` |
|    40 | 2034 | `	MemObjSetType(pSlot,MEMOBJ_INT);` |
|    40 | 2035 | `	return 0;` |
|    22 | 2036 | `}` |
|     - | 2037 | `/* The finfo an argument names, with php's TypeError for anything else. The` |
|     - | 2038 | ` * signature table has already screened the type, so a miss here can only be a` |
|     - | 2039 | ` * SUBCLASS instance with nothing in its slot. */` |
|    10 | 2040 | `static ph7_class_instance * FinfoArgObject(ph7_value *pArg)` |
|     2 | 2041 | `{` |
|    12 | 2042 | `	return (pArg->iFlags & MEMOBJ_OBJ) ? (ph7_class_instance *)pArg->x.pOther : 0;` |
|     2 | 2043 | `}` |
|     - | 2044 | `/* Analyse a buffer and answer the face the flags ask for. nFile is the size of` |
|     - | 2045 | ` * the whole object when the caller knows it (only a FILE does). */` |
|   668 | 2046 | `static void FinfoAnswerBuffer(ph7_context *pCtx,const unsigned char *z,sxu32 n,` |
|     - | 2047 | `	ph7_int64 nFile,int iFlags)` |
|     3 | 2048 | `{` |
|     - | 2049 | `	FinfoAnswer sAns;` |
|     - | 2050 | `	FinfoText sTx;` |
|     - | 2051 | `	SyBlob sOut;` |
|   671 | 2052 | `	SyBlobInit(&sAns.sDesc,&pCtx->pVm->sAllocator);` |
|   671 | 2053 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   671 | 2054 | `	FinfoAnalyze(z,n,nFile,&sAns,&sTx);` |
|   671 | 2055 | `	FinfoRender(&sAns,&sTx,iFlags,&sOut);` |
|   671 | 2056 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   671 | 2057 | `	SyBlobRelease(&sAns.sDesc);` |
|   671 | 2058 | `	SyBlobRelease(&sOut);` |
|   671 | 2059 | `}` |
|     - | 2060 | `/*` |
|     - | 2061 | ` * Is this path a directory? php asks before it opens anything, and answers the` |
|     - | 2062 | `` * bare string `directory` when the answer is yes -- through every face, mime`` |
|     - | 2063 | ` * included. A userland wrapper's url_stat() answers for the paths it owns.` |
|     - | 2064 | ` */` |
|    38 | 2065 | `static int FinfoPathIsDir(ph7_context *pCtx,const char *zPath)` |
|     1 | 2066 | `{` |
|    39 | 2067 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     - | 2068 | `	/* The OS's own stat and nothing else: a path a userland wrapper owns is not` |
|     - | 2069 | `	 * asked, which is measurable -- a wrapper whose url_stat() reports a` |
|     - | 2070 | `	 * DIRECTORY still has its bytes read and named under php. */` |
|    39 | 2071 | `	return (pVfs && pVfs->xIsdir) ? (pVfs->xIsdir(zPath) == PH7_OK) : 0;` |
|     1 | 2072 | `}` |
|     - | 2073 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - | 2074 | `/*` |
|     - | 2075 | ` * php reads the file through a php_stream, so every wrapper this engine` |
|     - | 2076 | ` * carries answers -- and so does the one a script registered itself. At most` |
|     - | 2077 | ` * FINFO_READ_MAX bytes, which is libmagic's own parameter.` |
|     - | 2078 | ` */` |
|    30 | 2079 | `static int FinfoReadPath(ph7_context *pCtx,const char *zPath,int nPath,SyBlob *pOut,` |
|     - | 2080 | `	ph7_int64 *pnFile)` |
|     1 | 2081 | `{` |
|     - | 2082 | `	const ph7_io_stream *pStream;` |
|     - | 2083 | `	void *pHandle;` |
|     - | 2084 | `	char zBuf[8192];` |
|    31 | 2085 | `	const char *zName = zPath;` |
|    31 | 2086 | `	ph7_int64 nTotal = 0;` |
|    31 | 2087 | `	*pnFile = -1;` |
|     - | 2088 | `	/* php resolves the wrapper once for its open_basedir check and once to open,` |
|     - | 2089 | `	 * so a scheme nothing implements is named TWICE before the open failure --` |
|     - | 2090 | `	 * which is this door's own diagnostic shape and nothing else's. */` |
|     - | 2091 | `	{` |
|    31 | 2092 | `		const char *zProbe = zPath;` |
|    31 | 2093 | `		if( PH7_VmGetStreamDevice(pCtx->pVm,&zProbe,nPath) == 0 ){` |
|     3 | 2094 | `			VfsThrowUnknownWrapperWarning(pCtx,zPath);` |
|     1 | 2095 | `		}` |
|     - | 2096 | `	}` |
|    31 | 2097 | `	pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zName,nPath);` |
|    31 | 2098 | `	if( pStream == 0 ){` |
|   ! 0 | 2099 | `		VfsThrowNoDeviceWarning(pCtx,zName,FALSE);` |
|   ! 0 | 2100 | `		return -1;` |
|     - | 2101 | `	}` |
|    46 | 2102 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zName,PH7_IO_OPEN_RDONLY,` |
|    15 | 2103 | `		FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|    31 | 2104 | `	if( pHandle == 0 ){` |
|     9 | 2105 | `		VfsThrowOpenWarning(pCtx,zName);` |
|     9 | 2106 | `		return -1;` |
|     - | 2107 | `	}` |
|    31 | 2108 | `	for(;;){` |
|     - | 2109 | `		ph7_int64 n;` |
|    43 | 2110 | `		ph7_int64 nChunk = (ph7_int64)sizeof(zBuf);` |
|    43 | 2111 | `		if( nTotal + nChunk > (ph7_int64)FINFO_READ_MAX ){` |
|   ! 0 | 2112 | `			nChunk = (ph7_int64)FINFO_READ_MAX - nTotal;` |
|   ! 0 | 2113 | `		}` |
|    43 | 2114 | `		if( nChunk < 1 ){` |
|   ! 0 | 2115 | `			break;` |
|     - | 2116 | `		}` |
|    43 | 2117 | `		n = pStream->xRead ? pStream->xRead(pHandle,zBuf,nChunk) : -1;` |
|    43 | 2118 | `		if( n < 1 ){` |
|    11 | 2119 | `			if( n == 0 \|\| nTotal > 0 ){` |
|     - | 2120 | `				/* A clean end, or a device that stopped answering after giving` |
|     - | 2121 | `				 * something: what was read IS the file as far as this goes. */` |
|    11 | 2122 | `			}` |
|    23 | 2123 | `			break;` |
|     - | 2124 | `		}` |
|    21 | 2125 | `		SyBlobAppend(pOut,zBuf,(sxu32)n);` |
|    21 | 2126 | `		nTotal += n;` |
|     1 | 2127 | `	}` |
|     - | 2128 | `	/* The whole size, when the stream can say -- gzip's trailer is the one` |
|     - | 2129 | `	 * answer that needs it, and a stream that cannot seek simply has none. */` |
|    23 | 2130 | `	if( pStream->xSeek && pStream->xTell && nTotal < (ph7_int64)FINFO_READ_MAX ){` |
|    23 | 2131 | `		*pnFile = nTotal;` |
|    11 | 2132 | `	}` |
|     - | 2133 | `	/* php asks the stream for a descriptor once it has the bytes, and a` |
|     - | 2134 | `	 * userland wrapper hears about it. */` |
|    23 | 2135 | `	PH7_StreamUserCast(pCtx,pStream,pHandle);` |
|    23 | 2136 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    23 | 2137 | `	return 0;` |
|    16 | 2138 | `}` |
|     - | 2139 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     - | 2140 | `/*` |
|     - | 2141 | ` * The body of finfo::file() and finfo_file(): the directory shortcut, then the` |
|     - | 2142 | ` * stream, then the analysis. iArgPos is which argument number the empty-name` |
|     - | 2143 | ` * ValueError names -- php's check is written for the procedural signature, so` |
|     - | 2144 | ` * it says #2 in both doors and picks up the METHOD's name for #2 in the method` |
|     - | 2145 | ` * one.` |
|     - | 2146 | ` */` |
|    44 | 2147 | `static int FinfoFileCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int iFlags,int nSkip,` |
|     - | 2148 | `	int iErrPos,const char *zErrName)` |
|     1 | 2149 | `{` |
|     - | 2150 | `	const char *zPath;` |
|     - | 2151 | `	int nPath;` |
|     - | 2152 | `	SyBlob sData;` |
|    45 | 2153 | `	ph7_int64 nFile = -1;` |
|    45 | 2154 | `	if( nArg > nSkip + 1 && ph7_value_is_int(apArg[nSkip+1]) ){` |
|     5 | 2155 | `		int iCall = (int)ph7_value_to_int(apArg[nSkip+1]);` |
|     5 | 2156 | `		if( iCall != 0 ){` |
|     3 | 2157 | `			iFlags = iCall;   /* this call only; the object keeps its own */` |
|     1 | 2158 | `		}` |
|     2 | 2159 | `	}` |
|    45 | 2160 | `	zPath = ph7_value_to_string(apArg[nSkip],&nPath);` |
|    45 | 2161 | `	if( nPath < 1 ){` |
|    10 | 2162 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     3 | 2163 | `			"%s(): Argument #%d ($%s) must not be empty",ph7_function_name(pCtx),` |
|     3 | 2164 | `			iErrPos,zErrName);` |
|     - | 2165 | `	}` |
|    39 | 2166 | `	if( FinfoPathIsDir(pCtx,zPath) ){` |
|     9 | 2167 | `		ph7_result_string(pCtx,"directory",sizeof("directory")-1);` |
|     9 | 2168 | `		return PH7_OK;` |
|     - | 2169 | `	}` |
|     - | 2170 | `#ifndef PH7_DISABLE_DISK_IO` |
|    31 | 2171 | `	SyBlobInit(&sData,&pCtx->pVm->sAllocator);` |
|    31 | 2172 | `	if( FinfoReadPath(pCtx,zPath,nPath,&sData,&nFile) != 0 ){` |
|     9 | 2173 | `		SyBlobRelease(&sData);` |
|     9 | 2174 | `		ph7_result_bool(pCtx,0);` |
|     9 | 2175 | `		return PH7_OK;` |
|     - | 2176 | `	}` |
|    34 | 2177 | `	FinfoAnswerBuffer(pCtx,(const unsigned char *)SyBlobData(&sData),` |
|    11 | 2178 | `		SyBlobLength(&sData),nFile,iFlags);` |
|    23 | 2179 | `	SyBlobRelease(&sData);` |
|     - | 2180 | `#else` |
|     - | 2181 | `	SXUNUSED(sData);` |
|     - | 2182 | `	SXUNUSED(nFile);` |
|     - | 2183 | `	ph7_result_bool(pCtx,0);` |
|     - | 2184 | `#endif` |
|    23 | 2185 | `	return PH7_OK;` |
|    23 | 2186 | `}` |
|     - | 2187 | `/*` |
|     - | 2188 | ` * php refuses a $magic_database it cannot load with a warning naming the door` |
|     - | 2189 | ` * and the path, and false -- or, from the constructor, an Exception carrying` |
|     - | 2190 | ` * that same sentence. PHL takes that path for EVERY non-empty database` |
|     - | 2191 | ` * argument: the file is in libmagic's compiled or source format and this` |
|     - | 2192 | ` * engine carries its own table instead of a reader for one.` |
|     - | 2193 | ` */` |
|   ! 0 | 2194 | `static int FinfoRefuseDatabase(ph7_context *pCtx,const char *zDb,int bCtor)` |
|   ! 0 | 2195 | `{` |
|   ! 0 | 2196 | `	if( bCtor ){` |
|   ! 0 | 2197 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|   ! 0 | 2198 | `			"%s(): Failed to load magic database at \"%s\"",ph7_function_name(pCtx),zDb);` |
|     - | 2199 | `	}` |
|   ! 0 | 2200 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|   ! 0 | 2201 | `		"Failed to load magic database at \"%s\"",zDb);` |
|   ! 0 | 2202 | `	return PH7_OK;` |
|   ! 0 | 2203 | `}` |
|     - | 2204 | `/* finfo::__construct(int $flags = FILEINFO_NONE, ?string $magic_database = null) */` |
|    22 | 2205 | `static int vm_builtin_finfo_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 2206 | `{` |
|    26 | 2207 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    26 | 2208 | `	int iFlags = FINFO_NONE;` |
|    26 | 2209 | `	if( nArg > 0 ){` |
|    22 | 2210 | `		iFlags = (int)ph7_value_to_int(apArg[0]);` |
|     9 | 2211 | `	}` |
|    26 | 2212 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     - | 2213 | `		int nDb;` |
|   ! 0 | 2214 | `		const char *zDb = ph7_value_to_string(apArg[1],&nDb);` |
|   ! 0 | 2215 | `		if( nDb > 0 ){` |
|   ! 0 | 2216 | `			return FinfoRefuseDatabase(pCtx,zDb,TRUE);` |
|     - | 2217 | `		}` |
|   ! 0 | 2218 | `	}` |
|    26 | 2219 | `	FinfoSetFlagsOf(pThis,iFlags);` |
|    26 | 2220 | `	return PH7_OK;` |
|    15 | 2221 | `}` |
|     - | 2222 | `/* finfo::file(string $filename, int $flags = FILEINFO_NONE, $context = null) */` |
|    34 | 2223 | `static int vm_builtin_finfo_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2224 | `{` |
|     - | 2225 | `	int iFlags;` |
|    36 | 2226 | `	if( !FinfoFlagsOf(pCtx,PH7_ContextThis(pCtx),&iFlags) ){` |
|     3 | 2227 | `		return PH7_OK;` |
|     - | 2228 | `	}` |
|     - | 2229 | `	/* php's check is written for the PROCEDURAL signature, so it names argument` |
|     - | 2230 | `	 * #2 -- and picks up whatever argument #2 is called in the door it was` |
|     - | 2231 | ``	 * raised through, which is `$flags` here and `$filename` there. */`` |
|    33 | 2232 | `	return FinfoFileCommon(pCtx,nArg,apArg,iFlags,0,2,"flags");` |
|    19 | 2233 | `}` |
|     - | 2234 | `/* finfo::buffer(string $string, int $flags = FILEINFO_NONE, $context = null) */` |
|   644 | 2235 | `static int vm_builtin_finfo_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 2236 | `{` |
|     - | 2237 | `	const char *zData;` |
|     - | 2238 | `	int nData,iFlags;` |
|   647 | 2239 | `	if( !FinfoFlagsOf(pCtx,PH7_ContextThis(pCtx),&iFlags) ){` |
|     3 | 2240 | `		return PH7_OK;` |
|     - | 2241 | `	}` |
|   645 | 2242 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|     3 | 2243 | `		int iCall = (int)ph7_value_to_int(apArg[1]);` |
|     3 | 2244 | `		if( iCall != 0 ){` |
|     3 | 2245 | `			iFlags = iCall;` |
|     1 | 2246 | `		}` |
|     1 | 2247 | `	}` |
|   645 | 2248 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|   645 | 2249 | `	FinfoAnswerBuffer(pCtx,(const unsigned char *)zData,(sxu32)(nData < 0 ? 0 : nData),-1,iFlags);` |
|   645 | 2250 | `	return PH7_OK;` |
|   325 | 2251 | `}` |
|     - | 2252 | `/* finfo::set_flags(int $flags): true */` |
|     2 | 2253 | `static int vm_builtin_finfo_set_flags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2254 | `{` |
|     - | 2255 | `	int iFlags;` |
|     3 | 2256 | `	if( !FinfoFlagsOf(pCtx,PH7_ContextThis(pCtx),&iFlags) ){` |
|   ! 0 | 2257 | `		return PH7_OK;` |
|     - | 2258 | `	}` |
|     1 | 2259 | `	SXUNUSED(nArg);` |
|     3 | 2260 | `	FinfoSetFlagsOf(PH7_ContextThis(pCtx),(int)ph7_value_to_int(apArg[0]));` |
|     3 | 2261 | `	ph7_result_bool(pCtx,1);` |
|     3 | 2262 | `	return PH7_OK;` |
|     2 | 2263 | `}` |
|     - | 2264 | `/* finfo\|false finfo_open(int $flags = FILEINFO_NONE, ?string $magic_database = null) */` |
|    10 | 2265 | `PH7_PRIVATE int PH7_builtin_finfo_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2266 | `{` |
|     - | 2267 | `	ph7_class_instance *pObj;` |
|     - | 2268 | `	ph7_class *pClass;` |
|    12 | 2269 | `	int iFlags = FINFO_NONE;` |
|    12 | 2270 | `	if( nArg > 0 ){` |
|     8 | 2271 | `		iFlags = (int)ph7_value_to_int(apArg[0]);` |
|     3 | 2272 | `	}` |
|    12 | 2273 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     - | 2274 | `		int nDb;` |
|     3 | 2275 | `		const char *zDb = ph7_value_to_string(apArg[1],&nDb);` |
|     3 | 2276 | `		if( nDb > 0 ){` |
|   ! 0 | 2277 | `			FinfoRefuseDatabase(pCtx,zDb,FALSE);` |
|   ! 0 | 2278 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 | 2279 | `			return PH7_OK;` |
|     - | 2280 | `		}` |
|     1 | 2281 | `	}` |
|    12 | 2282 | `	pClass = PH7_VmExtractClass(pCtx->pVm,"finfo",sizeof("finfo")-1,FALSE,0);` |
|    12 | 2283 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|    12 | 2284 | `	if( pObj == 0 ){` |
|   ! 0 | 2285 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2286 | `	}` |
|    12 | 2287 | `	FinfoSetFlagsOf(pObj,iFlags);` |
|    12 | 2288 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    12 | 2289 | `	return PH7_OK;` |
|     7 | 2290 | `}` |
|     - | 2291 | `/* true finfo_close(finfo $finfo) -- deprecated in 8.5 and does nothing. */` |
|     2 | 2292 | `PH7_PRIVATE int PH7_builtin_finfo_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2293 | `{` |
|     1 | 2294 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 | 2295 | `	ph7_result_bool(pCtx,1);` |
|     3 | 2296 | `	return PH7_OK;` |
|     1 | 2297 | `}` |
|     - | 2298 | `/* true finfo_set_flags(finfo $finfo,int $flags) */` |
|     2 | 2299 | `PH7_PRIVATE int PH7_builtin_finfo_set_flags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2300 | `{` |
|     3 | 2301 | `	ph7_class_instance *pThis = FinfoArgObject(apArg[0]);` |
|     - | 2302 | `	int iFlags;` |
|     1 | 2303 | `	SXUNUSED(nArg);` |
|     3 | 2304 | `	if( !FinfoFlagsOf(pCtx,pThis,&iFlags) ){` |
|   ! 0 | 2305 | `		return PH7_OK;` |
|     - | 2306 | `	}` |
|     3 | 2307 | `	FinfoSetFlagsOf(pThis,(int)ph7_value_to_int(apArg[1]));` |
|     3 | 2308 | `	ph7_result_bool(pCtx,1);` |
|     3 | 2309 | `	return PH7_OK;` |
|     2 | 2310 | `}` |
|     - | 2311 | `/* string\|false finfo_file(finfo $finfo,string $filename,int $flags = 0,$context = null) */` |
|     6 | 2312 | `PH7_PRIVATE int PH7_builtin_finfo_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2313 | `{` |
|     - | 2314 | `	int iFlags;` |
|     7 | 2315 | `	if( !FinfoFlagsOf(pCtx,FinfoArgObject(apArg[0]),&iFlags) ){` |
|   ! 0 | 2316 | `		return PH7_OK;` |
|     - | 2317 | `	}` |
|     7 | 2318 | `	return FinfoFileCommon(pCtx,nArg,apArg,iFlags,1,2,"filename");` |
|     4 | 2319 | `}` |
|     - | 2320 | `/* string\|false finfo_buffer(finfo $finfo,string $string,int $flags = 0,$context = null) */` |
|     2 | 2321 | `PH7_PRIVATE int PH7_builtin_finfo_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2322 | `{` |
|     - | 2323 | `	const char *zData;` |
|     - | 2324 | `	int nData,iFlags;` |
|     3 | 2325 | `	if( !FinfoFlagsOf(pCtx,FinfoArgObject(apArg[0]),&iFlags) ){` |
|   ! 0 | 2326 | `		return PH7_OK;` |
|     - | 2327 | `	}` |
|     3 | 2328 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|   ! 0 | 2329 | `		int iCall = (int)ph7_value_to_int(apArg[2]);` |
|   ! 0 | 2330 | `		if( iCall != 0 ){` |
|   ! 0 | 2331 | `			iFlags = iCall;` |
|   ! 0 | 2332 | `		}` |
|   ! 0 | 2333 | `	}` |
|     3 | 2334 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|     3 | 2335 | `	FinfoAnswerBuffer(pCtx,(const unsigned char *)zData,(sxu32)(nData < 0 ? 0 : nData),-1,iFlags);` |
|     3 | 2336 | `	return PH7_OK;` |
|     2 | 2337 | `}` |
|     - | 2338 | `/*` |
|     - | 2339 | ` * string\|false mime_content_type(resource\|string $filename)` |
|     - | 2340 | ` *` |
|     - | 2341 | ` * The one door that takes an OPEN stream as well as a path, and it reads such a` |
|     - | 2342 | ` * stream from the BEGINNING and puts it back exactly where it found it -- so a` |
|     - | 2343 | ` * script that has already read a header can still ask.` |
|     - | 2344 | ` */` |
|    12 | 2345 | `PH7_PRIVATE int PH7_builtin_mime_content_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2346 | `{` |
|     6 | 2347 | `	SXUNUSED(nArg);` |
|     - | 2348 | `	/* The parameter is declared untyped and screened by hand -- php's own` |
|     - | 2349 | `	 * arrangement, and the message says the union its body accepts. */` |
|    12 | 2350 | `	if( !ph7_value_is_resource(apArg[0]) && !ph7_value_is_string(apArg[0])` |
|     6 | 2351 | `	 && !PH7_ArgSatisfiesString(apArg[0]) ){` |
|     - | 2352 | `		char zGiven[64];` |
|     4 | 2353 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2354 | `			"mime_content_type(): Argument #1 ($filename) must be of type resource\|string, %s given",` |
|     1 | 2355 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - | 2356 | `	}` |
|    11 | 2357 | `	if( ph7_value_is_resource(apArg[0]) ){` |
|     - | 2358 | `#ifndef PH7_DISABLE_DISK_IO` |
|     5 | 2359 | `		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     - | 2360 | `		SyBlob sData;` |
|     - | 2361 | `		char zBuf[8192];` |
|     5 | 2362 | `		ph7_int64 nTotal = 0,nBack;` |
|     4 | 2363 | `		if( IO_PRIVATE_INVALID(pDev) \|\| pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC` |
|     3 | 2364 | `		 \|\| pDev->pStream == 0 \|\| pDev->pStream->xRead == 0 ){` |
|     3 | 2365 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2366 | `				"mime_content_type(): supplied resource is not a valid stream resource");` |
|     - | 2367 | `		}` |
|     3 | 2368 | `		nBack = PH7_StreamLogicalTell(pDev);` |
|     3 | 2369 | `		if( pDev->pStream->xSeek ){` |
|     3 | 2370 | `			pDev->pStream->xSeek(pDev->pHandle,0,0 /* SEEK_SET */);` |
|     1 | 2371 | `		}` |
|     3 | 2372 | `		SyBlobInit(&sData,&pCtx->pVm->sAllocator);` |
|     3 | 2373 | `		for(;;){` |
|     5 | 2374 | `			ph7_int64 nChunk = (ph7_int64)sizeof(zBuf);` |
|     - | 2375 | `			ph7_int64 n;` |
|     5 | 2376 | `			if( nTotal + nChunk > (ph7_int64)FINFO_READ_MAX ){` |
|   ! 0 | 2377 | `				nChunk = (ph7_int64)FINFO_READ_MAX - nTotal;` |
|   ! 0 | 2378 | `			}` |
|     5 | 2379 | `			if( nChunk < 1 ){` |
|   ! 0 | 2380 | `				break;` |
|     - | 2381 | `			}` |
|     5 | 2382 | `			n = pDev->pStream->xRead(pDev->pHandle,zBuf,nChunk);` |
|     5 | 2383 | `			if( n < 1 ){` |
|     3 | 2384 | `				break;` |
|     - | 2385 | `			}` |
|     3 | 2386 | `			SyBlobAppend(&sData,zBuf,(sxu32)n);` |
|     3 | 2387 | `			nTotal += n;` |
|     1 | 2388 | `		}` |
|     3 | 2389 | `		if( pDev->pStream->xSeek && nBack >= 0 ){` |
|     3 | 2390 | `			pDev->pStream->xSeek(pDev->pHandle,nBack,0 /* SEEK_SET */);` |
|     1 | 2391 | `		}` |
|     4 | 2392 | `		FinfoAnswerBuffer(pCtx,(const unsigned char *)SyBlobData(&sData),` |
|     1 | 2393 | `			SyBlobLength(&sData),nTotal,FINFO_MIME_TYPE);` |
|     3 | 2394 | `		SyBlobRelease(&sData);` |
|     - | 2395 | `#else` |
|     - | 2396 | `		ph7_result_bool(pCtx,0);` |
|     - | 2397 | `#endif` |
|     3 | 2398 | `		return PH7_OK;` |
|     - | 2399 | `	}` |
|     - | 2400 | `	/* The one door whose own signature puts the name first, so php's numbering` |
|     - | 2401 | `	 * and its arginfo agree here. */` |
|     7 | 2402 | `	return FinfoFileCommon(pCtx,nArg,apArg,FINFO_MIME_TYPE,0,1,"filename");` |
|     7 | 2403 | `}` |
|     - | 2404 | `/*` |
|     - | 2405 | ` * finfo: an ordinary class with one hidden slot. php declares no clone and no` |
|     - | 2406 | ` * serialize handler for it, which is what makes both refusals engine-level` |
|     - | 2407 | ` * rather than a body's.` |
|     - | 2408 | ` */` |
|  8445 | 2409 | `PH7_PRIVATE sxi32 PH7_VmInstallFileinfo(ph7_vm *pVm)` |
|     5 | 2410 | `{` |
|     - | 2411 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|     - | 2412 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - | 2413 | `		  "int $flags = FILEINFO_NONE, ?string $magic_database = null", 0,` |
|     - | 2414 | `		  vm_builtin_finfo_construct },` |
|     - | 2415 | `		{ "file", PH7_MOD_PUBLIC,` |
|     - | 2416 | `		  "string $filename, int $flags = FILEINFO_NONE, $context = null", "@string\|false",` |
|     - | 2417 | `		  vm_builtin_finfo_file },` |
|     - | 2418 | `		{ "buffer", PH7_MOD_PUBLIC,` |
|     - | 2419 | `		  "string $string, int $flags = FILEINFO_NONE, $context = null", "@string\|false",` |
|     - | 2420 | `		  vm_builtin_finfo_buffer },` |
|     - | 2421 | `		{ "set_flags", PH7_MOD_PUBLIC, "int $flags", "@true", vm_builtin_finfo_set_flags },` |
|     - | 2422 | `	};` |
|     - | 2423 | `	static const PH7_NativePropDef aProp[] = {` |
|     - | 2424 | `		{ FINFO_FLAGS_SLOT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 2425 | `		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2426 | `	};` |
|     - | 2427 | `	static const PH7_NativeClassSpec sSpec = {` |
|     - | 2428 | `		"finfo", 0, 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 2429 | `		aMethod, SX_ARRAYSIZE(aMethod), 0, 0,` |
|     - | 2430 | `		aProp, SX_ARRAYSIZE(aProp), 0, 0, 0` |
|     - | 2431 | `	};` |
|  8450 | 2432 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|     5 | 2433 | `}` |
|     - | 2434 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2435 |  |
