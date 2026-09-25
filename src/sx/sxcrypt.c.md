# src/sx/sxcrypt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 507/525 lines (96.57%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 1994 David Burren` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` *` |
|         - |    6 | ` * Unix crypt(3) — the engine behind the PHP crypt() builtin and` |
|         - |    7 | ` * password_verify()'s non-bcrypt fallback. Six schemes, matching what PHP's` |
|         - |    8 | ` * own crypt answers: traditional DES, BSDI extended DES ("_"), MD5-crypt` |
|         - |    9 | ` * ("$1$"), bcrypt ("$2a/b/x/y$", via sxblowfish), SHA-256-crypt ("$5$") and` |
|         - |   10 | ` * SHA-512-crypt ("$6$").` |
|         - |   11 | ` *` |
|         - |   12 | ` * The DES core is derived from FreeSec (David Burren's original DES and` |
|         - |   13 | ` * crypt(3) implementation for NetBSD, BSD-3-Clause): the FIPS 46-3 base` |
|         - |   14 | ` * tables below are the standard's own constants, and the OR-mask lookup` |
|         - |   15 | ` * tables FreeSec precomputes are generated here once at runtime. MD5-crypt` |
|         - |   16 | ` * follows Poul-Henning Kamp's algorithm; SHA-crypt follows Ulrich Drepper's` |
|         - |   17 | ` * public specification. Every scheme's parsing quirks (which salt bytes are` |
|         - |   18 | ` * legal, when "rounds=" is a spec and when it is a salt, what a malformed` |
|         - |   19 | ` * setting answers) follow PHP's crypt, oracle-verified — a malformed setting` |
|         - |   20 | ` * is the "*0" failure token, never an error.` |
|         - |   21 | ` */` |
|         - |   22 | `#include "sxtypes.h"` |
|         - |   23 | `#include "sxmacros.h"` |
|         - |   24 | `#include "sxstr.h"` |
|         - |   25 | `#include "sxdigest.h"` |
|         - |   26 | `#include "sxblowfish.h"` |
|         - |   27 | `#include "sxcrypt.h"` |
|         - |   28 |  |
|         - |   29 | `/* The MD5/SHA cores this file leans on live behind the same guard in` |
|         - |   30 | ` * sxhash.c; without them there is no crypt() to register either. */` |
|         - |   31 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|         - |   32 |  |
|         - |   33 | `/* The crypt base64 alphabet ('.', '/', digits, upper, lower) — NOT the bcrypt` |
|         - |   34 | ` * one, which starts at '.' but orders the classes differently. */` |
|         - |   35 | `static const char zCryptA64[] =` |
|         - |   36 | `	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";` |
|         - |   37 | `/* Map an ascii64 character to its 0..63 value, or -1 when outside the set. */` |
|       138 |   38 | `static int CryptA64Value(int c)` |
|         1 |   39 | `{` |
|       139 |   40 | `	if( c > 'z' ){ return -1; }` |
|       139 |   41 | `	if( c >= 'a' ){ return c - 'a' + 38; }` |
|        67 |   42 | `	if( c > 'Z' ){ return -1; }` |
|        67 |   43 | `	if( c >= 'A' ){ return c - 'A' + 12; }` |
|        67 |   44 | `	if( c > '9' ){ return -1; }` |
|        65 |   45 | `	if( c >= '.' ){ return c - '.'; }` |
|        11 |   46 | `	return -1;` |
|        70 |   47 | `}` |
|         - |   48 | `/* Emit n ascii64 characters from the low 6*n bits of w, least significant` |
|         - |   49 | ` * 6-bit group first (the crypt output convention for MD5/SHA schemes). */` |
|       558 |   50 | `static char *CryptA64Emit(char *zOut,sxu32 w,int n)` |
|         1 |   51 | `{` |
|      2717 |   52 | `	while( n-- > 0 ){` |
|      2159 |   53 | `		*zOut++ = zCryptA64[w & 0x3f];` |
|      2159 |   54 | `		w >>= 6;` |
|         1 |   55 | `	}` |
|       559 |   56 | `	return zOut;` |
|         1 |   57 | `}` |
|         - |   58 | `/*` |
|         - |   59 | ` * ---------------------------------------------------------------------------` |
|         - |   60 | ` * DES core (FreeSec structure). The base tables are FIPS 46-3's constants;` |
|         - |   61 | ` * DesInit() expands them into the OR-mask lookup tables the cipher uses, the` |
|         - |   62 | ` * same precomputation FreeSec ships. Generated once, lazily: concurrent first` |
|         - |   63 | ` * calls race only on writing identical values (the expansion is a pure` |
|         - |   64 | ` * function of the constants), which is benign — the flag is set last.` |
|         - |   65 | ` * ---------------------------------------------------------------------------` |
|         - |   66 | ` */` |
|         - |   67 | `static const sxu8 aDesIP[64] = {` |
|         - |   68 | `	58, 50, 42, 34, 26, 18, 10,  2, 60, 52, 44, 36, 28, 20, 12,  4,` |
|         - |   69 | `	62, 54, 46, 38, 30, 22, 14,  6, 64, 56, 48, 40, 32, 24, 16,  8,` |
|         - |   70 | `	57, 49, 41, 33, 25, 17,  9,  1, 59, 51, 43, 35, 27, 19, 11,  3,` |
|         - |   71 | `	61, 53, 45, 37, 29, 21, 13,  5, 63, 55, 47, 39, 31, 23, 15,  7` |
|         - |   72 | `};` |
|         - |   73 | `static const sxu8 aDesKeyPerm[56] = {` |
|         - |   74 | `	57, 49, 41, 33, 25, 17,  9,  1, 58, 50, 42, 34, 26, 18,` |
|         - |   75 | `	10,  2, 59, 51, 43, 35, 27, 19, 11,  3, 60, 52, 44, 36,` |
|         - |   76 | `	63, 55, 47, 39, 31, 23, 15,  7, 62, 54, 46, 38, 30, 22,` |
|         - |   77 | `	14,  6, 61, 53, 45, 37, 29, 21, 13,  5, 28, 20, 12,  4` |
|         - |   78 | `};` |
|         - |   79 | `static const sxu8 aDesCompPerm[48] = {` |
|         - |   80 | `	14, 17, 11, 24,  1,  5,  3, 28, 15,  6, 21, 10,` |
|         - |   81 | `	23, 19, 12,  4, 26,  8, 16,  7, 27, 20, 13,  2,` |
|         - |   82 | `	41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,` |
|         - |   83 | `	44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32` |
|         - |   84 | `};` |
|         - |   85 | `static const sxu8 aDesSbox[8][64] = {` |
|         - |   86 | `	{` |
|         - |   87 | `		14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,` |
|         - |   88 | `		 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8,` |
|         - |   89 | `		 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,` |
|         - |   90 | `		15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13` |
|         - |   91 | `	},` |
|         - |   92 | `	{` |
|         - |   93 | `		15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,` |
|         - |   94 | `		 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5,` |
|         - |   95 | `		 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,` |
|         - |   96 | `		13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9` |
|         - |   97 | `	},` |
|         - |   98 | `	{` |
|         - |   99 | `		10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,` |
|         - |  100 | `		13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1,` |
|         - |  101 | `		13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,` |
|         - |  102 | `		 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12` |
|         - |  103 | `	},` |
|         - |  104 | `	{` |
|         - |  105 | `		 7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,` |
|         - |  106 | `		13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9,` |
|         - |  107 | `		10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,` |
|         - |  108 | `		 3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14` |
|         - |  109 | `	},` |
|         - |  110 | `	{` |
|         - |  111 | `		 2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,` |
|         - |  112 | `		14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6,` |
|         - |  113 | `		 4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,` |
|         - |  114 | `		11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3` |
|         - |  115 | `	},` |
|         - |  116 | `	{` |
|         - |  117 | `		12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,` |
|         - |  118 | `		10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8,` |
|         - |  119 | `		 9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,` |
|         - |  120 | `		 4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13` |
|         - |  121 | `	},` |
|         - |  122 | `	{` |
|         - |  123 | `		 4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,` |
|         - |  124 | `		13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6,` |
|         - |  125 | `		 1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,` |
|         - |  126 | `		 6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12` |
|         - |  127 | `	},` |
|         - |  128 | `	{` |
|         - |  129 | `		13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,` |
|         - |  130 | `		 1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2,` |
|         - |  131 | `		 7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,` |
|         - |  132 | `		 2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11` |
|         - |  133 | `	}` |
|         - |  134 | `};` |
|         - |  135 | `static const sxu8 aDesPbox[32] = {` |
|         - |  136 | `	16,  7, 20, 21, 29, 12, 28, 17,  1, 15, 23, 26,  5, 18, 31, 10,` |
|         - |  137 | `	 2,  8, 24, 14, 32, 27,  3,  9, 19, 13, 30,  6, 22, 11,  4, 25` |
|         - |  138 | `};` |
|         - |  139 | `/* The generated lookup tables (~70 KB, .bss). */` |
|         - |  140 | `static struct DesTables {` |
|         - |  141 | `	volatile int isInit;` |
|         - |  142 | `	sxu8 mSbox[4][4096];` |
|         - |  143 | `	sxu32 ipMaskL[8][256], ipMaskR[8][256];` |
|         - |  144 | `	sxu32 fpMaskL[8][256], fpMaskR[8][256];` |
|         - |  145 | `	sxu32 keyPermMaskL[8][128], keyPermMaskR[8][128];` |
|         - |  146 | `	sxu32 compMaskL[8][128], compMaskR[8][128];` |
|         - |  147 | `	sxu32 psbox[4][256];` |
|         - |  148 | `} sDes;` |
|         - |  149 |  |
|         - |  150 | `typedef struct SyDesCtx SyDesCtx;` |
|         - |  151 | `struct SyDesCtx {` |
|         - |  152 | `	sxu32 aKeyL[16];` |
|         - |  153 | `	sxu32 aKeyR[16];` |
|         - |  154 | `	sxu32 nSaltBits;` |
|         - |  155 | `};` |
|         - |  156 |  |
|       120 |  157 | `static void DesInit(void)` |
|         1 |  158 | `{` |
|         - |  159 | `	sxu8 aUSbox[8][64];` |
|         - |  160 | `	sxu8 aInvKeyPerm[64], aInvCompPerm[56];` |
|         - |  161 | `	sxu8 aInitPerm[64], aFinalPerm[64], aUnPbox[32];` |
|         - |  162 | `	int i, j, b, k, inbit, obit;` |
|       121 |  163 | `	if( sDes.isInit ){` |
|       119 |  164 | `		return;` |
|         - |  165 | `	}` |
|         - |  166 | `	/* Invert the S-boxes, reordering the input bits. */` |
|        19 |  167 | `	for( i = 0; i < 8; i++ ){` |
|      1041 |  168 | `		for( j = 0; j < 64; j++ ){` |
|      1025 |  169 | `			b = (j & 0x20) \| ((j & 1) << 4) \| ((j >> 1) & 0xf);` |
|      1025 |  170 | `			aUSbox[i][j] = aDesSbox[i][b];` |
|       513 |  171 | `		}` |
|         9 |  172 | `	}` |
|         - |  173 | `	/* Convert the inverted S-boxes into 4 arrays handling 12 input bits each. */` |
|        11 |  174 | `	for( b = 0; b < 4; b++ ){` |
|       521 |  175 | `		for( i = 0; i < 64; i++ ){` |
|     33281 |  176 | `			for( j = 0; j < 64; j++ ){` |
|     32769 |  177 | `				sDes.mSbox[b][(i << 6) \| j] =` |
|     32768 |  178 | `					(sxu8)((aUSbox[b*2][i] << 4) \| aUSbox[b*2+1][j]);` |
|     16385 |  179 | `			}` |
|       257 |  180 | `		}` |
|         5 |  181 | `	}` |
|         - |  182 | `	/* Initial/final permutations, and the inverted key permutations. */` |
|       131 |  183 | `	for( i = 0; i < 64; i++ ){` |
|       129 |  184 | `		aFinalPerm[i] = (sxu8)(aDesIP[i] - 1);` |
|       129 |  185 | `		aInitPerm[aFinalPerm[i]] = (sxu8)i;` |
|       129 |  186 | `		aInvKeyPerm[i] = 255;` |
|        65 |  187 | `	}` |
|       115 |  188 | `	for( i = 0; i < 56; i++ ){` |
|       113 |  189 | `		aInvKeyPerm[aDesKeyPerm[i] - 1] = (sxu8)i;` |
|       113 |  190 | `		aInvCompPerm[i] = 255;` |
|        57 |  191 | `	}` |
|        99 |  192 | `	for( i = 0; i < 48; i++ ){` |
|        97 |  193 | `		aInvCompPerm[aDesCompPerm[i] - 1] = (sxu8)i;` |
|        49 |  194 | `	}` |
|         - |  195 | `	/* OR-mask arrays for the initial/final and key permutations. */` |
|        19 |  196 | `	for( k = 0; k < 8; k++ ){` |
|      4113 |  197 | `		for( i = 0; i < 256; i++ ){` |
|      4097 |  198 | `			sxu32 il = 0, ir = 0, fl = 0, fr = 0;` |
|     36865 |  199 | `			for( j = 0; j < 8; j++ ){` |
|     32769 |  200 | `				inbit = 8*k + j;` |
|     32769 |  201 | `				if( i & (0x80 >> j) ){` |
|     16385 |  202 | `					obit = aInitPerm[inbit];` |
|     16385 |  203 | `					if( obit < 32 ){ il \|= (sxu32)0x80000000 >> obit; }` |
|      8193 |  204 | `					else{ ir \|= (sxu32)0x80000000 >> (obit - 32); }` |
|     16385 |  205 | `					obit = aFinalPerm[inbit];` |
|     16385 |  206 | `					if( obit < 32 ){ fl \|= (sxu32)0x80000000 >> obit; }` |
|      8193 |  207 | `					else{ fr \|= (sxu32)0x80000000 >> (obit - 32); }` |
|      8192 |  208 | `				}` |
|     16385 |  209 | `			}` |
|      4097 |  210 | `			sDes.ipMaskL[k][i] = il; sDes.ipMaskR[k][i] = ir;` |
|      4097 |  211 | `			sDes.fpMaskL[k][i] = fl; sDes.fpMaskR[k][i] = fr;` |
|      2049 |  212 | `		}` |
|      2065 |  213 | `		for( i = 0; i < 128; i++ ){` |
|      2049 |  214 | `			sxu32 kl = 0, kr = 0, cl = 0, cr = 0;` |
|     16385 |  215 | `			for( j = 0; j < 7; j++ ){` |
|     14337 |  216 | `				inbit = 8*k + j;` |
|     14337 |  217 | `				if( i & (0x80 >> (j + 1)) ){` |
|      7169 |  218 | `					obit = aInvKeyPerm[inbit];` |
|      7169 |  219 | `					if( obit != 255 ){` |
|         - |  220 | `						/* 28-bit halves sit in the top 28 bits of a word. */` |
|      7169 |  221 | `						if( obit < 28 ){ kl \|= (sxu32)0x08000000 >> obit; }` |
|      3585 |  222 | `						else{ kr \|= (sxu32)0x08000000 >> (obit - 28); }` |
|      3584 |  223 | `					}` |
|      3584 |  224 | `				}` |
|     14337 |  225 | `				inbit = 7*k + j;` |
|     14337 |  226 | `				if( i & (0x80 >> (j + 1)) ){` |
|      7169 |  227 | `					obit = aInvCompPerm[inbit];` |
|      7169 |  228 | `					if( obit != 255 ){` |
|         - |  229 | `						/* 24-bit halves sit in the top 24 bits of a word. */` |
|      6145 |  230 | `						if( obit < 24 ){ cl \|= (sxu32)0x00800000 >> obit; }` |
|      3073 |  231 | `						else{ cr \|= (sxu32)0x00800000 >> (obit - 24); }` |
|      3072 |  232 | `					}` |
|      3584 |  233 | `				}` |
|      7169 |  234 | `			}` |
|      2049 |  235 | `			sDes.keyPermMaskL[k][i] = kl; sDes.keyPermMaskR[k][i] = kr;` |
|      2049 |  236 | `			sDes.compMaskL[k][i] = cl; sDes.compMaskR[k][i] = cr;` |
|      1025 |  237 | `		}` |
|         9 |  238 | `	}` |
|         - |  239 | `	/* Invert the P-box, folded into the S-box output masks. */` |
|        67 |  240 | `	for( i = 0; i < 32; i++ ){` |
|        65 |  241 | `		aUnPbox[aDesPbox[i] - 1] = (sxu8)i;` |
|        33 |  242 | `	}` |
|        11 |  243 | `	for( b = 0; b < 4; b++ ){` |
|      2057 |  244 | `		for( i = 0; i < 256; i++ ){` |
|      2049 |  245 | `			sxu32 p = 0;` |
|     18433 |  246 | `			for( j = 0; j < 8; j++ ){` |
|     16385 |  247 | `				if( i & (0x80 >> j) ){` |
|      8193 |  248 | `					p \|= (sxu32)0x80000000 >> aUnPbox[8*b + j];` |
|      4096 |  249 | `				}` |
|      8193 |  250 | `			}` |
|      2049 |  251 | `			sDes.psbox[b][i] = p;` |
|      1025 |  252 | `		}` |
|         5 |  253 | `	}` |
|         3 |  254 | `	sDes.isInit = 1;` |
|        61 |  255 | `}` |
|        26 |  256 | `static void DesSetKey(SyDesCtx *pCtx,const sxu8 aKey[8])` |
|         1 |  257 | `{` |
|         - |  258 | `	static const sxu8 aKeyShift[16] = { 1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1 };` |
|         - |  259 | `	sxu32 rawkey0, rawkey1, k0, k1, t0, t1;` |
|         - |  260 | `	int nShift, iRound, i;` |
|        40 |  261 | `	rawkey0 = ((sxu32)aKey[0] << 24) \| ((sxu32)aKey[1] << 16)` |
|        26 |  262 | `		\| ((sxu32)aKey[2] << 8) \| (sxu32)aKey[3];` |
|        40 |  263 | `	rawkey1 = ((sxu32)aKey[4] << 24) \| ((sxu32)aKey[5] << 16)` |
|        26 |  264 | `		\| ((sxu32)aKey[6] << 8) \| (sxu32)aKey[7];` |
|        27 |  265 | `	k0 = k1 = 0;` |
|       131 |  266 | `	for( i = 0; i < 4; i++ ){` |
|       157 |  267 | `		k0 \|= sDes.keyPermMaskL[i][(rawkey0 >> (25 - 8*i)) & 0x7f]` |
|       104 |  268 | `			\| sDes.keyPermMaskL[i+4][(rawkey1 >> (25 - 8*i)) & 0x7f];` |
|       157 |  269 | `		k1 \|= sDes.keyPermMaskR[i][(rawkey0 >> (25 - 8*i)) & 0x7f]` |
|       104 |  270 | `			\| sDes.keyPermMaskR[i+4][(rawkey1 >> (25 - 8*i)) & 0x7f];` |
|        53 |  271 | `	}` |
|        27 |  272 | `	nShift = 0;` |
|       443 |  273 | `	for( iRound = 0; iRound < 16; iRound++ ){` |
|       417 |  274 | `		nShift += aKeyShift[iRound];` |
|       417 |  275 | `		t0 = (k0 << nShift) \| (k0 >> (28 - nShift));` |
|       417 |  276 | `		t1 = (k1 << nShift) \| (k1 >> (28 - nShift));` |
|       417 |  277 | `		pCtx->aKeyL[iRound] = pCtx->aKeyR[iRound] = 0;` |
|      2081 |  278 | `		for( i = 0; i < 4; i++ ){` |
|      2497 |  279 | `			pCtx->aKeyL[iRound] \|= sDes.compMaskL[i][(t0 >> (21 - 7*i)) & 0x7f]` |
|      1664 |  280 | `				\| sDes.compMaskL[i+4][(t1 >> (21 - 7*i)) & 0x7f];` |
|      2497 |  281 | `			pCtx->aKeyR[iRound] \|= sDes.compMaskR[i][(t0 >> (21 - 7*i)) & 0x7f]` |
|      1664 |  282 | `				\| sDes.compMaskR[i+4][(t1 >> (21 - 7*i)) & 0x7f];` |
|       833 |  283 | `		}` |
|       209 |  284 | `	}` |
|        27 |  285 | `}` |
|         - |  286 | `/* The salt selectively swaps E-expansion bits: bit i of the 24-bit salt set` |
|         - |  287 | ` * means expansion bits i of the two halves trade places. */` |
|        34 |  288 | `static void DesSetSalt(SyDesCtx *pCtx,sxu32 nSalt)` |
|         1 |  289 | `{` |
|        35 |  290 | `	sxu32 obit = 0x800000, saltbit = 1, saltbits = 0;` |
|         - |  291 | `	int i;` |
|       851 |  292 | `	for( i = 0; i < 24; i++ ){` |
|       817 |  293 | `		if( nSalt & saltbit ){` |
|       249 |  294 | `			saltbits \|= obit;` |
|       124 |  295 | `		}` |
|       817 |  296 | `		saltbit <<= 1;` |
|       817 |  297 | `		obit >>= 1;` |
|       409 |  298 | `	}` |
|        35 |  299 | `	pCtx->nSaltBits = saltbits;` |
|        35 |  300 | `}` |
|         - |  301 | `/* Encrypt the 64-bit block in[] nCount times (count 0 behaves as 1, the` |
|         - |  302 | ` * FreeSec rule ext-DES's "0000" rounds field relies on). */` |
|        26 |  303 | `static void DesCryptBlock(SyDesCtx *pCtx,sxu8 aOut[8],const sxu8 aIn[8],sxu32 nCount)` |
|         1 |  304 | `{` |
|         - |  305 | `	sxu32 l_in, r_in, l_out, r_out, l, r, f, r48l, r48r;` |
|        27 |  306 | `	sxu32 saltbits = pCtx->nSaltBits;` |
|         - |  307 | `	int iRound, i;` |
|        27 |  308 | `	if( nCount == 0 ){` |
|       ! 0 |  309 | `		nCount = 1;` |
|       ! 0 |  310 | `	}` |
|        40 |  311 | `	l_in = ((sxu32)aIn[0] << 24) \| ((sxu32)aIn[1] << 16)` |
|        26 |  312 | `		\| ((sxu32)aIn[2] << 8) \| (sxu32)aIn[3];` |
|        40 |  313 | `	r_in = ((sxu32)aIn[4] << 24) \| ((sxu32)aIn[5] << 16)` |
|        26 |  314 | `		\| ((sxu32)aIn[6] << 8) \| (sxu32)aIn[7];` |
|        27 |  315 | `	l = r = 0;` |
|       131 |  316 | `	for( i = 0; i < 4; i++ ){` |
|       157 |  317 | `		l \|= sDes.ipMaskL[i][(l_in >> (24 - 8*i)) & 0xff]` |
|       104 |  318 | `			\| sDes.ipMaskL[i+4][(r_in >> (24 - 8*i)) & 0xff];` |
|       157 |  319 | `		r \|= sDes.ipMaskR[i][(l_in >> (24 - 8*i)) & 0xff]` |
|       104 |  320 | `			\| sDes.ipMaskR[i+4][(r_in >> (24 - 8*i)) & 0xff];` |
|        53 |  321 | `	}` |
|        27 |  322 | `	f = 0;` |
|        13 |  323 | `	do {` |
| 108660091 |  324 | `		for( iRound = 0; iRound < 16; iRound++ ){` |
|         - |  325 | `			/* E-expansion of r into two 24-bit halves. */` |
| 153402481 |  326 | `			r48l = ((r & 0x00000001) << 23)` |
| 102268320 |  327 | `				\| ((r & 0xf8000000) >>  9)` |
| 102268320 |  328 | `				\| ((r & 0x1f800000) >> 11)` |
| 102268320 |  329 | `				\| ((r & 0x01f80000) >> 13)` |
| 102268320 |  330 | `				\| ((r & 0x001f8000) >> 15);` |
| 153402481 |  331 | `			r48r = ((r & 0x0001f800) <<  7)` |
| 102268320 |  332 | `				\| ((r & 0x00001f80) <<  5)` |
| 102268320 |  333 | `				\| ((r & 0x000001f8) <<  3)` |
| 102268320 |  334 | `				\| ((r & 0x0000001f) <<  1)` |
| 102268320 |  335 | `				\| ((r & 0x80000000) >> 31);` |
|         - |  336 | `			/* Apply salt and the round key. */` |
| 102268321 |  337 | `			f = (r48l ^ r48r) & saltbits;` |
| 102268321 |  338 | `			r48l ^= f ^ pCtx->aKeyL[iRound];` |
| 102268321 |  339 | `			r48r ^= f ^ pCtx->aKeyR[iRound];` |
|         - |  340 | `			/* S-box lookups fused with the P-box permutation. */` |
| 153402481 |  341 | `			f = sDes.psbox[0][sDes.mSbox[0][r48l >> 12]]` |
| 102268320 |  342 | `				\| sDes.psbox[1][sDes.mSbox[1][r48l & 0xfff]]` |
| 102268320 |  343 | `				\| sDes.psbox[2][sDes.mSbox[2][r48r >> 12]]` |
| 102268320 |  344 | `				\| sDes.psbox[3][sDes.mSbox[3][r48r & 0xfff]];` |
| 102268321 |  345 | `			f ^= l;` |
| 102268321 |  346 | `			l = r;` |
| 102268321 |  347 | `			r = f;` |
|  51134161 |  348 | `		}` |
|   6391771 |  349 | `		r = l;` |
|   6391771 |  350 | `		l = f;` |
|   6391771 |  351 | `	} while( --nCount > 0 );` |
|        27 |  352 | `	l_out = r_out = 0;` |
|       131 |  353 | `	for( i = 0; i < 4; i++ ){` |
|       157 |  354 | `		l_out \|= sDes.fpMaskL[i][(l >> (24 - 8*i)) & 0xff]` |
|       104 |  355 | `			\| sDes.fpMaskL[i+4][(r >> (24 - 8*i)) & 0xff];` |
|       157 |  356 | `		r_out \|= sDes.fpMaskR[i][(l >> (24 - 8*i)) & 0xff]` |
|       104 |  357 | `			\| sDes.fpMaskR[i+4][(r >> (24 - 8*i)) & 0xff];` |
|        53 |  358 | `	}` |
|        27 |  359 | `	aOut[0] = (sxu8)(l_out >> 24); aOut[1] = (sxu8)(l_out >> 16);` |
|        27 |  360 | `	aOut[2] = (sxu8)(l_out >> 8);  aOut[3] = (sxu8)l_out;` |
|        27 |  361 | `	aOut[4] = (sxu8)(r_out >> 24); aOut[5] = (sxu8)(r_out >> 16);` |
|        27 |  362 | `	aOut[6] = (sxu8)(r_out >> 8);  aOut[7] = (sxu8)r_out;` |
|        27 |  363 | `}` |
|         - |  364 | `/* Encrypt a zero block nCount times and append the 11-character encoding of` |
|         - |  365 | ` * the 64-bit result (crypt's big-endian 6-bit packing, MSB first). */` |
|        26 |  366 | `static char *DesGenHash(SyDesCtx *pCtx,sxu32 nCount,char *zOut)` |
|         1 |  367 | `{` |
|         - |  368 | `	sxu8 aCipher[8], aZero[8];` |
|        27 |  369 | `	const sxu8 *p = aCipher, *pEnd = aCipher + 8;` |
|         - |  370 | `	unsigned int c1, c2;` |
|        27 |  371 | `	SyZero(aZero,(sxu32)sizeof(aZero));` |
|        27 |  372 | `	DesCryptBlock(pCtx,aCipher,aZero,nCount);` |
|        13 |  373 | `	do {` |
|        79 |  374 | `		c1 = *p++;` |
|        79 |  375 | `		*zOut++ = zCryptA64[c1 >> 2];` |
|        79 |  376 | `		c1 = (c1 & 0x03) << 4;` |
|        79 |  377 | `		if( p >= pEnd ){` |
|       ! 0 |  378 | `			*zOut++ = zCryptA64[c1];` |
|       ! 0 |  379 | `			break;` |
|         - |  380 | `		}` |
|        79 |  381 | `		c2 = *p++;` |
|        79 |  382 | `		c1 \|= c2 >> 4;` |
|        79 |  383 | `		*zOut++ = zCryptA64[c1];` |
|        79 |  384 | `		c1 = (c2 & 0x0f) << 2;` |
|        79 |  385 | `		if( p >= pEnd ){` |
|        27 |  386 | `			*zOut++ = zCryptA64[c1];` |
|        27 |  387 | `			break;` |
|         - |  388 | `		}` |
|        53 |  389 | `		c2 = *p++;` |
|        53 |  390 | `		c1 \|= c2 >> 6;` |
|        53 |  391 | `		*zOut++ = zCryptA64[c1];` |
|        53 |  392 | `		*zOut++ = zCryptA64[c2 & 0x3f];` |
|        53 |  393 | `	} while( p < pEnd );` |
|        27 |  394 | `	return zOut;` |
|         1 |  395 | `}` |
|         - |  396 | `/* Traditional DES: 2 salt characters, password truncated at 8. Answers the` |
|         - |  397 | ` * output length, or 0 on a malformed salt. */` |
|        32 |  398 | `static sxu32 CryptDes(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,` |
|         - |  399 | `	char *zOut)` |
|         1 |  400 | `{` |
|         - |  401 | `	SyDesCtx sCtx;` |
|         - |  402 | `	sxu8 aKey[8];` |
|         - |  403 | `	sxu32 i, nSalt;` |
|         - |  404 | `	int v0, v1;` |
|        33 |  405 | `	if( nSetting < 2 ){` |
|         7 |  406 | `		return 0;` |
|         - |  407 | `	}` |
|        27 |  408 | `	v0 = CryptA64Value((unsigned char)zSetting[0]);` |
|        27 |  409 | `	v1 = CryptA64Value((unsigned char)zSetting[1]);` |
|        27 |  410 | `	if( v0 < 0 \|\| v1 < 0 ){` |
|         9 |  411 | `		return 0;` |
|         - |  412 | `	}` |
|        19 |  413 | `	nSalt = (sxu32)v0 \| ((sxu32)v1 << 6);` |
|         - |  414 | `	/* The canonical salt is re-encoded, not copied. */` |
|        19 |  415 | `	zOut[0] = zCryptA64[nSalt & 0x3f];` |
|        19 |  416 | `	zOut[1] = zCryptA64[(nSalt >> 6) & 0x3f];` |
|       163 |  417 | `	for( i = 0; i < 8; i++ ){` |
|         - |  418 | `		/* Widen unsigned before the shift: a high-bit password byte is a` |
|         - |  419 | `		 * NEGATIVE char, and shifting that left is UB. */` |
|       145 |  420 | `		aKey[i] = (sxu8)((sxu32)(sxu8)(i < nPwd ? zPwd[i] : 0) << 1);` |
|        73 |  421 | `	}` |
|        19 |  422 | `	DesSetKey(&sCtx,aKey);` |
|        19 |  423 | `	DesSetSalt(&sCtx,nSalt);` |
|        19 |  424 | `	DesGenHash(&sCtx,25,&zOut[2]);` |
|        19 |  425 | `	return 13;` |
|        17 |  426 | `}` |
|         - |  427 | `/* BSDI extended DES: "_", 4 count characters, 4 salt characters (each 6 bits,` |
|         - |  428 | ` * least significant first), unlimited password folded 8 bytes at a time. */` |
|        16 |  429 | `static sxu32 CryptExtDes(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,` |
|         - |  430 | `	char *zOut)` |
|         1 |  431 | `{` |
|         - |  432 | `	SyDesCtx sCtx;` |
|         - |  433 | `	sxu8 aKey[8], aPrev[8];` |
|        17 |  434 | `	sxu32 nCount = 0, nSalt = 0, iPwd, i;` |
|         - |  435 | `	int v;` |
|        17 |  436 | `	if( nSetting < 9 ){` |
|         5 |  437 | `		return 0;` |
|         - |  438 | `	}` |
|        61 |  439 | `	for( i = 1; i < 5; i++ ){` |
|        49 |  440 | `		v = CryptA64Value((unsigned char)zSetting[i]);` |
|        49 |  441 | `		if( v < 0 ){` |
|       ! 0 |  442 | `			return 0;` |
|         - |  443 | `		}` |
|        49 |  444 | `		nCount \|= (sxu32)v << ((i - 1) * 6);` |
|        25 |  445 | `	}` |
|        13 |  446 | `	if( nCount == 0 ){` |
|         - |  447 | `		/* php refuses a zero iteration count outright ("*0"), where FreeSec` |
|         - |  448 | `		 * quietly computes with 1 — oracle-verified. */` |
|         3 |  449 | `		return 0;` |
|         - |  450 | `	}` |
|        47 |  451 | `	for( i = 5; i < 9; i++ ){` |
|        39 |  452 | `		v = CryptA64Value((unsigned char)zSetting[i]);` |
|        39 |  453 | `		if( v < 0 ){` |
|         3 |  454 | `			return 0;` |
|         - |  455 | `		}` |
|        37 |  456 | `		nSalt \|= (sxu32)v << ((i - 5) * 6);` |
|        19 |  457 | `	}` |
|         9 |  458 | `	SyMemcpy(zSetting,zOut,9);` |
|         - |  459 | `	/* Fold the password into one DES key, Merkle-Damgård style: each 8-byte` |
|         - |  460 | `	 * chunk is shifted as usual, XORed with the previous round's output (IV` |
|         - |  461 | `	 * zero), set as the key, and encrypted once with a zero salt. */` |
|         9 |  462 | `	DesSetSalt(&sCtx,0);` |
|         9 |  463 | `	SyZero(aPrev,(sxu32)sizeof(aPrev));` |
|         9 |  464 | `	iPwd = 0;` |
|         4 |  465 | `	for(;;){` |
|        73 |  466 | `		for( i = 0; i < 8; i++ ){` |
|         - |  467 | `			/* Unsigned-widened shift, as in CryptDes. */` |
|        65 |  468 | `			aKey[i] = (sxu8)(aPrev[i] ^ (sxu8)((sxu32)(sxu8)(iPwd < nPwd ? zPwd[iPwd] : 0) << 1));` |
|        65 |  469 | `			if( iPwd < nPwd ){ iPwd++; }` |
|        33 |  470 | `		}` |
|         9 |  471 | `		DesSetKey(&sCtx,aKey);` |
|         9 |  472 | `		if( iPwd >= nPwd ){` |
|         9 |  473 | `			break;` |
|         - |  474 | `		}` |
|       ! 0 |  475 | `		DesCryptBlock(&sCtx,aPrev,aKey,1);` |
|       ! 0 |  476 | `	}` |
|         9 |  477 | `	DesSetSalt(&sCtx,nSalt);` |
|         9 |  478 | `	DesGenHash(&sCtx,nCount,&zOut[9]);` |
|         9 |  479 | `	return 20;` |
|         9 |  480 | `}` |
|         - |  481 | `/*` |
|         - |  482 | ` * ---------------------------------------------------------------------------` |
|         - |  483 | ` * MD5-crypt ("$1$"): Poul-Henning Kamp's construction — salt is up to 8 bytes` |
|         - |  484 | ` * ending at '$' (any byte values), 1000 fixed rounds.` |
|         - |  485 | ` * ---------------------------------------------------------------------------` |
|         - |  486 | ` */` |
|        16 |  487 | `static sxu32 CryptMd5(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,` |
|         - |  488 | `	char *zOut)` |
|         1 |  489 | `{` |
|         - |  490 | `	MD5Context sCtx;` |
|         - |  491 | `	sxu8 aDigest[16];` |
|        17 |  492 | `	const char *zSalt = &zSetting[3];` |
|        17 |  493 | `	sxu32 nSalt = nSetting - 3, nCnt;` |
|         - |  494 | `	char *zCur;` |
|         - |  495 | `	sxu32 i;` |
|        17 |  496 | `	if( nSalt > 8 ){ nSalt = 8; }` |
|       125 |  497 | `	for( i = 0; i < nSalt; i++ ){` |
|       111 |  498 | `		if( zSalt[i] == '$' ){` |
|         3 |  499 | `			nSalt = i;` |
|         3 |  500 | `			break;` |
|         - |  501 | `		}` |
|        55 |  502 | `	}` |
|         - |  503 | `	/* The alternate sum: MD5(password + salt + password). */` |
|        17 |  504 | `	MD5Init(&sCtx);` |
|        17 |  505 | `	MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);` |
|        17 |  506 | `	MD5Update(&sCtx,(const unsigned char *)zSalt,nSalt);` |
|        17 |  507 | `	MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);` |
|        17 |  508 | `	MD5Final(aDigest,&sCtx);` |
|         - |  509 | `	/* The intermediate sum: password, the "$1$" magic, the salt, then one` |
|         - |  510 | `	 * byte of the alternate sum per password byte, then PHK's bit walk. */` |
|        17 |  511 | `	MD5Init(&sCtx);` |
|        17 |  512 | `	MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);` |
|        17 |  513 | `	MD5Update(&sCtx,(const unsigned char *)"$1$",3);` |
|        17 |  514 | `	MD5Update(&sCtx,(const unsigned char *)zSalt,nSalt);` |
|        17 |  515 | `	for( nCnt = nPwd; nCnt > 16; nCnt -= 16 ){` |
|       ! 0 |  516 | `		MD5Update(&sCtx,aDigest,16);` |
|       ! 0 |  517 | `	}` |
|        17 |  518 | `	MD5Update(&sCtx,aDigest,nCnt);` |
|        17 |  519 | `	aDigest[0] = 0;` |
|        55 |  520 | `	for( nCnt = nPwd; nCnt > 0; nCnt >>= 1 ){` |
|        39 |  521 | `		if( nCnt & 1 ){` |
|        25 |  522 | `			MD5Update(&sCtx,aDigest,1);` |
|        13 |  523 | `		}else{` |
|        15 |  524 | `			MD5Update(&sCtx,(const unsigned char *)zPwd,1);` |
|         - |  525 | `		}` |
|        20 |  526 | `	}` |
|        17 |  527 | `	MD5Final(aDigest,&sCtx);` |
|         - |  528 | `	/* The 1000-round stretching loop. */` |
|     16017 |  529 | `	for( nCnt = 0; nCnt < 1000; nCnt++ ){` |
|     16001 |  530 | `		MD5Init(&sCtx);` |
|     16001 |  531 | `		if( nCnt & 1 ){` |
|      8001 |  532 | `			MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);` |
|      4001 |  533 | `		}else{` |
|      8001 |  534 | `			MD5Update(&sCtx,aDigest,16);` |
|         - |  535 | `		}` |
|     16001 |  536 | `		if( nCnt % 3 ){` |
|     10657 |  537 | `			MD5Update(&sCtx,(const unsigned char *)zSalt,nSalt);` |
|      5328 |  538 | `		}` |
|     16001 |  539 | `		if( nCnt % 7 ){` |
|     13713 |  540 | `			MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);` |
|      6856 |  541 | `		}` |
|     16001 |  542 | `		if( nCnt & 1 ){` |
|      8001 |  543 | `			MD5Update(&sCtx,aDigest,16);` |
|      4001 |  544 | `		}else{` |
|      8001 |  545 | `			MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);` |
|         - |  546 | `		}` |
|     16001 |  547 | `		MD5Final(aDigest,&sCtx);` |
|      8001 |  548 | `	}` |
|        17 |  549 | `	zCur = zOut;` |
|        17 |  550 | `	SyMemcpy("$1$",zCur,3); zCur += 3;` |
|        17 |  551 | `	SyMemcpy(zSalt,zCur,nSalt); zCur += nSalt;` |
|        17 |  552 | `	*zCur++ = '$';` |
|        17 |  553 | `	zCur = CryptA64Emit(zCur,((sxu32)aDigest[0] << 16) \| ((sxu32)aDigest[6] << 8) \| aDigest[12],4);` |
|        17 |  554 | `	zCur = CryptA64Emit(zCur,((sxu32)aDigest[1] << 16) \| ((sxu32)aDigest[7] << 8) \| aDigest[13],4);` |
|        17 |  555 | `	zCur = CryptA64Emit(zCur,((sxu32)aDigest[2] << 16) \| ((sxu32)aDigest[8] << 8) \| aDigest[14],4);` |
|        17 |  556 | `	zCur = CryptA64Emit(zCur,((sxu32)aDigest[3] << 16) \| ((sxu32)aDigest[9] << 8) \| aDigest[15],4);` |
|        17 |  557 | `	zCur = CryptA64Emit(zCur,((sxu32)aDigest[4] << 16) \| ((sxu32)aDigest[10] << 8) \| aDigest[5],4);` |
|        17 |  558 | `	zCur = CryptA64Emit(zCur,aDigest[11],2);` |
|        17 |  559 | `	return (sxu32)(zCur - zOut);` |
|         1 |  560 | `}` |
|         - |  561 | `/*` |
|         - |  562 | ` * ---------------------------------------------------------------------------` |
|         - |  563 | ` * SHA-crypt ("$5$" / "$6$"): Ulrich Drepper's construction. One body serves` |
|         - |  564 | ` * both digests through a small context switch.` |
|         - |  565 | ` * ---------------------------------------------------------------------------` |
|         - |  566 | ` */` |
|         - |  567 | `typedef struct ShaCryptCtx ShaCryptCtx;` |
|         - |  568 | `struct ShaCryptCtx {` |
|         - |  569 | `	int bIs512;` |
|         - |  570 | `	union {` |
|         - |  571 | `		SHA256Context s256;` |
|         - |  572 | `		SHA512Context s512;` |
|         - |  573 | `	} u;` |
|         - |  574 | `};` |
|     92120 |  575 | `static void ShaCtxInit(ShaCryptCtx *p)` |
|         1 |  576 | `{` |
|     92121 |  577 | `	if( p->bIs512 ){ SHA512Init(&p->u.s512); }else{ SHA256Init(&p->u.s256); }` |
|     92121 |  578 | `}` |
|    327268 |  579 | `static void ShaCtxUpdate(ShaCryptCtx *p,const void *pData,sxu32 nLen)` |
|         1 |  580 | `{` |
|    327269 |  581 | `	if( p->bIs512 ){` |
|    135779 |  582 | `		SHA512Update(&p->u.s512,(const unsigned char *)pData,nLen);` |
|     67890 |  583 | `	}else{` |
|    191491 |  584 | `		SHA256Update(&p->u.s256,(const unsigned char *)pData,nLen);` |
|         - |  585 | `	}` |
|    327269 |  586 | `}` |
|     92120 |  587 | `static void ShaCtxFinal(ShaCryptCtx *p,sxu8 *pDigest)` |
|         1 |  588 | `{` |
|     92121 |  589 | `	if( p->bIs512 ){ SHA512Final(&p->u.s512,pDigest); }else{ SHA256Final(&p->u.s256,pDigest); }` |
|     92121 |  590 | `}` |
|         - |  591 | `/* Update with pBlock repeated cyclically to nLen total bytes — the standard's` |
|         - |  592 | ` * P and S sequences without materialising a password-sized buffer. */` |
|    232188 |  593 | `static void ShaCtxUpdateRecycled(ShaCryptCtx *p,const sxu8 *pBlock,sxu32 nBlock,sxu32 nLen)` |
|         1 |  594 | `{` |
|    232189 |  595 | `	while( nLen > nBlock ){` |
|       ! 0 |  596 | `		ShaCtxUpdate(p,pBlock,nBlock);` |
|       ! 0 |  597 | `		nLen -= nBlock;` |
|       ! 0 |  598 | `	}` |
|    232189 |  599 | `	ShaCtxUpdate(p,pBlock,nLen);` |
|    232189 |  600 | `}` |
|        34 |  601 | `static sxu32 CryptSha(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,` |
|         - |  602 | `	int bIs512,char *zOut)` |
|         1 |  603 | `{` |
|         - |  604 | `	ShaCryptCtx sCtx;` |
|         - |  605 | `	sxu8 aResult[64], aPDigest[64], aSDigest[64];` |
|        35 |  606 | `	const char *zSalt = &zSetting[3];` |
|        35 |  607 | `	sxu32 nRest = nSetting - 3;` |
|        35 |  608 | `	sxu32 nRounds = 5000, nSalt, nDigest, nCnt;` |
|        35 |  609 | `	int bRoundsCustom = 0;` |
|         - |  610 | `	char *zCur;` |
|         - |  611 | `	sxu32 i;` |
|         - |  612 | `	/* "rounds=N$" is a spec only when the digit run ends at a '$'; anything` |
|         - |  613 | `	 * else (including "rounds=1000x") is an ordinary salt. The accepted range` |
|         - |  614 | `	 * is [1000, 999999999], refused — not clamped — outside it. */` |
|        35 |  615 | `	if( nRest >= 7 && SyMemcmp(zSalt,"rounds=",7) == 0 ){` |
|        23 |  616 | `		sxu64 nVal = 0;` |
|        23 |  617 | `		i = 7;` |
|       115 |  618 | `		while( i < nRest && zSalt[i] >= '0' && zSalt[i] <= '9' ){` |
|        93 |  619 | `			if( nVal < (sxu64)10000000000ULL ){` |
|        93 |  620 | `				nVal = nVal * 10 + (sxu64)(zSalt[i] - '0');` |
|        46 |  621 | `			}` |
|        93 |  622 | `			i++;` |
|         1 |  623 | `		}` |
|        23 |  624 | `		if( i < nRest && zSalt[i] == '$' ){` |
|        21 |  625 | `			if( nVal < 1000 \|\| nVal > 999999999 ){` |
|         7 |  626 | `				return 0;` |
|         - |  627 | `			}` |
|        15 |  628 | `			nRounds = (sxu32)nVal;` |
|        15 |  629 | `			bRoundsCustom = 1;` |
|        15 |  630 | `			zSalt += i + 1;` |
|        15 |  631 | `			nRest -= i + 1;` |
|         7 |  632 | `		}` |
|         8 |  633 | `	}` |
|        29 |  634 | `	nSalt = nRest;` |
|       191 |  635 | `	for( i = 0; i < nRest; i++ ){` |
|       187 |  636 | `		if( zSalt[i] == '$' ){` |
|        25 |  637 | `			nSalt = i;` |
|        25 |  638 | `			break;` |
|         - |  639 | `		}` |
|        82 |  640 | `	}` |
|        29 |  641 | `	if( nSalt > 16 ){ nSalt = 16; }` |
|        29 |  642 | `	nDigest = bIs512 ? 64 : 32;` |
|        29 |  643 | `	sCtx.bIs512 = bIs512;` |
|         - |  644 | `	/* Digest B: password + salt + password. */` |
|        29 |  645 | `	ShaCtxInit(&sCtx);` |
|        29 |  646 | `	ShaCtxUpdate(&sCtx,zPwd,nPwd);` |
|        29 |  647 | `	ShaCtxUpdate(&sCtx,zSalt,nSalt);` |
|        29 |  648 | `	ShaCtxUpdate(&sCtx,zPwd,nPwd);` |
|        29 |  649 | `	ShaCtxFinal(&sCtx,aResult);` |
|         - |  650 | `	/* Digest A: password + salt + B repeated to the password's length, then` |
|         - |  651 | `	 * for each bit of the length: B when set, the password when clear. */` |
|        29 |  652 | `	ShaCtxInit(&sCtx);` |
|        29 |  653 | `	ShaCtxUpdate(&sCtx,zPwd,nPwd);` |
|        29 |  654 | `	ShaCtxUpdate(&sCtx,zSalt,nSalt);` |
|        29 |  655 | `	for( nCnt = nPwd; nCnt > nDigest; nCnt -= nDigest ){` |
|       ! 0 |  656 | `		ShaCtxUpdate(&sCtx,aResult,nDigest);` |
|       ! 0 |  657 | `	}` |
|        29 |  658 | `	ShaCtxUpdate(&sCtx,aResult,nCnt);` |
|        99 |  659 | `	for( nCnt = nPwd; nCnt > 0; nCnt >>= 1 ){` |
|        71 |  660 | `		if( nCnt & 1 ){` |
|        41 |  661 | `			ShaCtxUpdate(&sCtx,aResult,nDigest);` |
|        21 |  662 | `		}else{` |
|        31 |  663 | `			ShaCtxUpdate(&sCtx,zPwd,nPwd);` |
|         - |  664 | `		}` |
|        36 |  665 | `	}` |
|        29 |  666 | `	ShaCtxFinal(&sCtx,aResult);` |
|         - |  667 | `	/* Digest P: the password repeated once per password byte. */` |
|        29 |  668 | `	ShaCtxInit(&sCtx);` |
|       155 |  669 | `	for( nCnt = 0; nCnt < nPwd; nCnt++ ){` |
|       127 |  670 | `		ShaCtxUpdate(&sCtx,zPwd,nPwd);` |
|        64 |  671 | `	}` |
|        29 |  672 | `	ShaCtxFinal(&sCtx,aPDigest);` |
|         - |  673 | `	/* Digest S: the salt repeated 16 + A[0] times. */` |
|        29 |  674 | `	ShaCtxInit(&sCtx);` |
|      2737 |  675 | `	for( nCnt = 0; nCnt < 16u + aResult[0]; nCnt++ ){` |
|      2709 |  676 | `		ShaCtxUpdate(&sCtx,zSalt,nSalt);` |
|      1355 |  677 | `	}` |
|        29 |  678 | `	ShaCtxFinal(&sCtx,aSDigest);` |
|         - |  679 | `	/* The rounds loop, P standing in for the password and S for the salt. */` |
|     92037 |  680 | `	for( nCnt = 0; nCnt < nRounds; nCnt++ ){` |
|     92009 |  681 | `		ShaCtxInit(&sCtx);` |
|     92009 |  682 | `		if( nCnt & 1 ){` |
|     46001 |  683 | `			ShaCtxUpdateRecycled(&sCtx,aPDigest,nDigest,nPwd);` |
|     23001 |  684 | `		}else{` |
|     46009 |  685 | `			ShaCtxUpdate(&sCtx,aResult,nDigest);` |
|         - |  686 | `		}` |
|     92009 |  687 | `		if( nCnt % 3 ){` |
|     61329 |  688 | `			ShaCtxUpdateRecycled(&sCtx,aSDigest,nDigest,nSalt);` |
|     30664 |  689 | `		}` |
|     92009 |  690 | `		if( nCnt % 7 ){` |
|     78853 |  691 | `			ShaCtxUpdateRecycled(&sCtx,aPDigest,nDigest,nPwd);` |
|     39426 |  692 | `		}` |
|     92009 |  693 | `		if( nCnt & 1 ){` |
|     46001 |  694 | `			ShaCtxUpdate(&sCtx,aResult,nDigest);` |
|     23001 |  695 | `		}else{` |
|     46009 |  696 | `			ShaCtxUpdateRecycled(&sCtx,aPDigest,nDigest,nPwd);` |
|         - |  697 | `		}` |
|     92009 |  698 | `		ShaCtxFinal(&sCtx,aResult);` |
|     46005 |  699 | `	}` |
|        29 |  700 | `	zCur = zOut;` |
|        29 |  701 | `	*zCur++ = '$'; *zCur++ = bIs512 ? '6' : '5'; *zCur++ = '$';` |
|        29 |  702 | `	if( bRoundsCustom ){` |
|         - |  703 | `		char zNum[12];` |
|        15 |  704 | `		int nNum = 0;` |
|        15 |  705 | `		sxu32 nR = nRounds;` |
|         7 |  706 | `		do {` |
|        57 |  707 | `			zNum[nNum++] = (char)('0' + (nR % 10));` |
|        57 |  708 | `			nR /= 10;` |
|        57 |  709 | `		} while( nR > 0 );` |
|        15 |  710 | `		SyMemcpy("rounds=",zCur,7); zCur += 7;` |
|        71 |  711 | `		while( nNum > 0 ){` |
|        57 |  712 | `			*zCur++ = zNum[--nNum];` |
|         1 |  713 | `		}` |
|        15 |  714 | `		*zCur++ = '$';` |
|         7 |  715 | `	}` |
|        29 |  716 | `	SyMemcpy(zSalt,zCur,nSalt); zCur += nSalt;` |
|        29 |  717 | `	*zCur++ = '$';` |
|        29 |  718 | `	if( bIs512 ){` |
|         - |  719 | `		static const sxu8 aOrd[21][3] = {` |
|         - |  720 | `			{ 0,21,42},{22,43, 1},{44, 2,23},{ 3,24,45},{25,46, 4},{47, 5,26},` |
|         - |  721 | `			{ 6,27,48},{28,49, 7},{50, 8,29},{ 9,30,51},{31,52,10},{53,11,32},` |
|         - |  722 | `			{12,33,54},{34,55,13},{56,14,35},{15,36,57},{37,58,16},{59,17,38},` |
|         - |  723 | `			{18,39,60},{40,61,19},{62,20,41}` |
|         - |  724 | `		};` |
|       309 |  725 | `		for( i = 0; i < 21; i++ ){` |
|       442 |  726 | `			zCur = CryptA64Emit(zCur,((sxu32)aResult[aOrd[i][0]] << 16)` |
|       294 |  727 | `				\| ((sxu32)aResult[aOrd[i][1]] << 8) \| aResult[aOrd[i][2]],4);` |
|       148 |  728 | `		}` |
|        15 |  729 | `		zCur = CryptA64Emit(zCur,aResult[63],2);` |
|         8 |  730 | `	}else{` |
|         - |  731 | `		static const sxu8 aOrd[10][3] = {` |
|         - |  732 | `			{ 0,10,20},{21, 1,11},{12,22, 2},{ 3,13,23},{24, 4,14},` |
|         - |  733 | `			{15,25, 5},{ 6,16,26},{27, 7,17},{18,28, 8},{ 9,19,29}` |
|         - |  734 | `		};` |
|       155 |  735 | `		for( i = 0; i < 10; i++ ){` |
|       211 |  736 | `			zCur = CryptA64Emit(zCur,((sxu32)aResult[aOrd[i][0]] << 16)` |
|       140 |  737 | `				\| ((sxu32)aResult[aOrd[i][1]] << 8) \| aResult[aOrd[i][2]],4);` |
|        71 |  738 | `		}` |
|        15 |  739 | `		zCur = CryptA64Emit(zCur,((sxu32)aResult[31] << 8) \| aResult[30],3);` |
|         - |  740 | `	}` |
|        29 |  741 | `	return (sxu32)(zCur - zOut);` |
|        18 |  742 | `}` |
|         - |  743 | `/*` |
|         - |  744 | ` * ---------------------------------------------------------------------------` |
|         - |  745 | ` * bcrypt ("$2a/b/x/y$NN$" + 22 salt characters; extra characters ignored).` |
|         - |  746 | ` * ---------------------------------------------------------------------------` |
|         - |  747 | ` */` |
|        22 |  748 | `static sxu32 CryptBlowfish(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,` |
|         - |  749 | `	char *zOut)` |
|         1 |  750 | `{` |
|         - |  751 | `	sxu8 aSalt[16];` |
|         - |  752 | `	sxu32 nCost;` |
|         - |  753 | `	int cMinor;` |
|        23 |  754 | `	if( nSetting < 29 \|\| zSetting[3] != '$' \|\| zSetting[6] != '$' ){` |
|         5 |  755 | `		return 0;` |
|         - |  756 | `	}` |
|        19 |  757 | `	cMinor = (unsigned char)zSetting[2];` |
|        19 |  758 | `	if( cMinor != 'a' && cMinor != 'b' && cMinor != 'x' && cMinor != 'y' ){` |
|       ! 0 |  759 | `		return 0;` |
|         - |  760 | `	}` |
|        19 |  761 | `	if( zSetting[4] < '0' \|\| zSetting[4] > '9' \|\| zSetting[5] < '0' \|\| zSetting[5] > '9' ){` |
|       ! 0 |  762 | `		return 0;` |
|         - |  763 | `	}` |
|        19 |  764 | `	nCost = (sxu32)(zSetting[4] - '0') * 10 + (sxu32)(zSetting[5] - '0');` |
|        19 |  765 | `	if( nCost < 4 \|\| nCost > 31 ){` |
|         7 |  766 | `		return 0;` |
|         - |  767 | `	}` |
|        13 |  768 | `	if( SyBcryptB64Decode(&zSetting[7],22,aSalt,(sxu32)sizeof(aSalt)) != SXRET_OK ){` |
|       ! 0 |  769 | `		return 0;` |
|         - |  770 | `	}` |
|         - |  771 | `	/* password_hash()'s 72-byte key cap lives inside SyBcryptHashEx. */` |
|        13 |  772 | `	if( SyBcryptHashEx((const unsigned char *)zPwd,nPwd,nCost,aSalt,cMinor,zOut) != SXRET_OK ){` |
|       ! 0 |  773 | `		return 0;` |
|         - |  774 | `	}` |
|        13 |  775 | `	return 60;` |
|        12 |  776 | `}` |
|         - |  777 | `/*` |
|         - |  778 | ` * The dispatcher. php semantics throughout: an unrecognised or malformed` |
|         - |  779 | ` * setting answers the "*0" failure token ("*1" when the setting itself` |
|         - |  780 | ` * begins with "*0"), and both inputs end at their first NUL byte.` |
|         - |  781 | ` */` |
|       120 |  782 | `PH7_PRIVATE sxi32 SyCrypt(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,` |
|         - |  783 | `	char *zOut,sxu32 *pnOut)` |
|         1 |  784 | `{` |
|       121 |  785 | `	sxu32 n = 0;` |
|         - |  786 | `	sxu32 i;` |
|         - |  787 | `	/* php hands its C layer NUL-terminated strings: a NUL ends the value. */` |
|       519 |  788 | `	for( i = 0; i < nPwd; i++ ){` |
|       401 |  789 | `		if( zPwd[i] == 0 ){ nPwd = i; break; }` |
|       200 |  790 | `	}` |
|      2623 |  791 | `	for( i = 0; i < nSetting; i++ ){` |
|      2503 |  792 | `		if( zSetting[i] == 0 ){ nSetting = i; break; }` |
|      1252 |  793 | `	}` |
|       121 |  794 | `	DesInit();` |
|       121 |  795 | `	if( nSetting >= 3 && zSetting[0] == '$' && zSetting[1] == '1' && zSetting[2] == '$' ){` |
|        17 |  796 | `		n = CryptMd5(zPwd,nPwd,zSetting,nSetting,zOut);` |
|       113 |  797 | `	}else if( nSetting >= 2 && zSetting[0] == '$' && zSetting[1] == '2' ){` |
|        23 |  798 | `		n = CryptBlowfish(zPwd,nPwd,zSetting,nSetting,zOut);` |
|        94 |  799 | `	}else if( nSetting >= 3 && zSetting[0] == '$' && zSetting[1] == '5' && zSetting[2] == '$' ){` |
|        21 |  800 | `		n = CryptSha(zPwd,nPwd,zSetting,nSetting,0,zOut);` |
|        73 |  801 | `	}else if( nSetting >= 3 && zSetting[0] == '$' && zSetting[1] == '6' && zSetting[2] == '$' ){` |
|        15 |  802 | `		n = CryptSha(zPwd,nPwd,zSetting,nSetting,1,zOut);` |
|        56 |  803 | `	}else if( nSetting >= 1 && zSetting[0] == '_' ){` |
|        17 |  804 | `		n = CryptExtDes(zPwd,nPwd,zSetting,nSetting,zOut);` |
|         9 |  805 | `	}else{` |
|        33 |  806 | `		n = CryptDes(zPwd,nPwd,zSetting,nSetting,zOut);` |
|         - |  807 | `	}` |
|       121 |  808 | `	if( n == 0 ){` |
|         - |  809 | `		/* The failure token: "*0", or "*1" when the setting begins "*0". */` |
|        39 |  810 | `		zOut[0] = '*';` |
|        39 |  811 | `		zOut[1] = (nSetting >= 2 && zSetting[0] == '*' && zSetting[1] == '0') ? '1' : '0';` |
|        39 |  812 | `		n = 2;` |
|        19 |  813 | `	}` |
|       121 |  814 | `	*pnOut = n;` |
|       121 |  815 | `	return SXRET_OK;` |
|         1 |  816 | `}` |
|         - |  817 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - |  818 |  |
