# src/sx/sxblowfish.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 168/170 lines (98.82%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 1997 Niels Provos <provos@physnet.uni-hamburg.de>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` *` |
|         - |    6 | ` * Blowfish + Eksblowfish (bcrypt) — the crypto core behind the PHP password_*` |
|         - |    7 | ` * builtins. The algorithm follows Provos & Mazieres, "A Future-Adaptable` |
|         - |    8 | ` * Password Scheme" (OpenBSD bcrypt). PHL emits the corrected "$2y" variant:` |
|         - |    9 | ` * key bytes are read as unsigned (no sign extension).` |
|         - |   10 | ` */` |
|         - |   11 | `#include "sxtypes.h"` |
|         - |   12 | `#include "sxmacros.h"` |
|         - |   13 | `#include "sxstr.h"` |
|         - |   14 | `#include "sxblowfish.h"` |
|         - |   15 |  |
|         - |   16 | `#define BLF_N    16              /* Number of Subkeys */` |
|         - |   17 |  |
|         - |   18 | `typedef struct {` |
|         - |   19 | `	sxu32 S[4][256];             /* S-Boxes */` |
|         - |   20 | `	sxu32 P[BLF_N + 2];          /* Subkeys */` |
|         - |   21 | `} blf_ctx;` |
|         - |   22 |  |
|         - |   23 | `/* The standard Blowfish initial subkeys/S-boxes: the fractional part of pi. */` |
|         - |   24 | `static const sxu32 ORIG_P[BLF_N + 2] = {` |
|         - |   25 |  |
|         - |   26 |  |
|         - |   27 |  |
|         - |   28 |  |
|         - |   29 |  |
|         - |   30 | `};` |
|         - |   31 |  |
|         - |   32 | `static const sxu32 ORIG_S[4][256] = {` |
|         - |   33 | `	{` |
|         - |   34 |  |
|         - |   35 |  |
|         - |   36 |  |
|         - |   37 |  |
|         - |   38 |  |
|         - |   39 |  |
|         - |   40 |  |
|         - |   41 |  |
|         - |   42 |  |
|         - |   43 |  |
|         - |   44 |  |
|         - |   45 |  |
|         - |   46 |  |
|         - |   47 |  |
|         - |   48 |  |
|         - |   49 |  |
|         - |   50 |  |
|         - |   51 |  |
|         - |   52 |  |
|         - |   53 |  |
|         - |   54 |  |
|         - |   55 |  |
|         - |   56 |  |
|         - |   57 |  |
|         - |   58 |  |
|         - |   59 |  |
|         - |   60 |  |
|         - |   61 |  |
|         - |   62 |  |
|         - |   63 |  |
|         - |   64 |  |
|         - |   65 |  |
|         - |   66 |  |
|         - |   67 |  |
|         - |   68 |  |
|         - |   69 |  |
|         - |   70 |  |
|         - |   71 |  |
|         - |   72 |  |
|         - |   73 |  |
|         - |   74 |  |
|         - |   75 |  |
|         - |   76 |  |
|         - |   77 |  |
|         - |   78 |  |
|         - |   79 |  |
|         - |   80 |  |
|         - |   81 |  |
|         - |   82 |  |
|         - |   83 |  |
|         - |   84 |  |
|         - |   85 |  |
|         - |   86 |  |
|         - |   87 |  |
|         - |   88 |  |
|         - |   89 |  |
|         - |   90 |  |
|         - |   91 |  |
|         - |   92 |  |
|         - |   93 |  |
|         - |   94 |  |
|         - |   95 |  |
|         - |   96 |  |
|         - |   97 |  |
|         - |   98 | `	}, {` |
|         - |   99 |  |
|         - |  100 |  |
|         - |  101 |  |
|         - |  102 |  |
|         - |  103 |  |
|         - |  104 |  |
|         - |  105 |  |
|         - |  106 |  |
|         - |  107 |  |
|         - |  108 |  |
|         - |  109 |  |
|         - |  110 |  |
|         - |  111 |  |
|         - |  112 |  |
|         - |  113 |  |
|         - |  114 |  |
|         - |  115 |  |
|         - |  116 |  |
|         - |  117 |  |
|         - |  118 |  |
|         - |  119 |  |
|         - |  120 |  |
|         - |  121 |  |
|         - |  122 |  |
|         - |  123 |  |
|         - |  124 |  |
|         - |  125 |  |
|         - |  126 |  |
|         - |  127 |  |
|         - |  128 |  |
|         - |  129 |  |
|         - |  130 |  |
|         - |  131 |  |
|         - |  132 |  |
|         - |  133 |  |
|         - |  134 |  |
|         - |  135 |  |
|         - |  136 |  |
|         - |  137 |  |
|         - |  138 |  |
|         - |  139 |  |
|         - |  140 |  |
|         - |  141 |  |
|         - |  142 |  |
|         - |  143 |  |
|         - |  144 |  |
|         - |  145 |  |
|         - |  146 |  |
|         - |  147 |  |
|         - |  148 |  |
|         - |  149 |  |
|         - |  150 |  |
|         - |  151 |  |
|         - |  152 |  |
|         - |  153 |  |
|         - |  154 |  |
|         - |  155 |  |
|         - |  156 |  |
|         - |  157 |  |
|         - |  158 |  |
|         - |  159 |  |
|         - |  160 |  |
|         - |  161 |  |
|         - |  162 |  |
|         - |  163 | `	}, {` |
|         - |  164 |  |
|         - |  165 |  |
|         - |  166 |  |
|         - |  167 |  |
|         - |  168 |  |
|         - |  169 |  |
|         - |  170 |  |
|         - |  171 |  |
|         - |  172 |  |
|         - |  173 |  |
|         - |  174 |  |
|         - |  175 |  |
|         - |  176 |  |
|         - |  177 |  |
|         - |  178 |  |
|         - |  179 |  |
|         - |  180 |  |
|         - |  181 |  |
|         - |  182 |  |
|         - |  183 |  |
|         - |  184 |  |
|         - |  185 |  |
|         - |  186 |  |
|         - |  187 |  |
|         - |  188 |  |
|         - |  189 |  |
|         - |  190 |  |
|         - |  191 |  |
|         - |  192 |  |
|         - |  193 |  |
|         - |  194 |  |
|         - |  195 |  |
|         - |  196 |  |
|         - |  197 |  |
|         - |  198 |  |
|         - |  199 |  |
|         - |  200 |  |
|         - |  201 |  |
|         - |  202 |  |
|         - |  203 |  |
|         - |  204 |  |
|         - |  205 |  |
|         - |  206 |  |
|         - |  207 |  |
|         - |  208 |  |
|         - |  209 |  |
|         - |  210 |  |
|         - |  211 |  |
|         - |  212 |  |
|         - |  213 |  |
|         - |  214 |  |
|         - |  215 |  |
|         - |  216 |  |
|         - |  217 |  |
|         - |  218 |  |
|         - |  219 |  |
|         - |  220 |  |
|         - |  221 |  |
|         - |  222 |  |
|         - |  223 |  |
|         - |  224 |  |
|         - |  225 |  |
|         - |  226 |  |
|         - |  227 |  |
|         - |  228 | `	}, {` |
|         - |  229 |  |
|         - |  230 |  |
|         - |  231 |  |
|         - |  232 |  |
|         - |  233 |  |
|         - |  234 |  |
|         - |  235 |  |
|         - |  236 |  |
|         - |  237 |  |
|         - |  238 |  |
|         - |  239 |  |
|         - |  240 |  |
|         - |  241 |  |
|         - |  242 |  |
|         - |  243 |  |
|         - |  244 |  |
|         - |  245 |  |
|         - |  246 |  |
|         - |  247 |  |
|         - |  248 |  |
|         - |  249 |  |
|         - |  250 |  |
|         - |  251 |  |
|         - |  252 |  |
|         - |  253 |  |
|         - |  254 |  |
|         - |  255 |  |
|         - |  256 |  |
|         - |  257 |  |
|         - |  258 |  |
|         - |  259 |  |
|         - |  260 |  |
|         - |  261 |  |
|         - |  262 |  |
|         - |  263 |  |
|         - |  264 |  |
|         - |  265 |  |
|         - |  266 |  |
|         - |  267 |  |
|         - |  268 |  |
|         - |  269 |  |
|         - |  270 |  |
|         - |  271 |  |
|         - |  272 |  |
|         - |  273 |  |
|         - |  274 |  |
|         - |  275 |  |
|         - |  276 |  |
|         - |  277 |  |
|         - |  278 |  |
|         - |  279 |  |
|         - |  280 |  |
|         - |  281 |  |
|         - |  282 |  |
|         - |  283 |  |
|         - |  284 |  |
|         - |  285 |  |
|         - |  286 |  |
|         - |  287 |  |
|         - |  288 |  |
|         - |  289 |  |
|         - |  290 |  |
|         - |  291 |  |
|         - |  292 |  |
|         - |  293 | `	}` |
|         - |  294 | `};` |
|         - |  295 |  |
|         - |  296 | `/* The Blowfish F function: 4-byte split, two S-box adds with one xor. */` |
| 214277345 |  297 | `static sxu32 Blowfish_F(blf_ctx *c,sxu32 x){` |
| 214277345 |  298 | `	sxu32 a = (x >> 24) & 0xff;` |
| 214277345 |  299 | `	sxu32 b = (x >> 16) & 0xff;` |
| 214277345 |  300 | `	sxu32 cc = (x >> 8) & 0xff;` |
| 214277345 |  301 | `	sxu32 d = x & 0xff;` |
| 214277345 |  302 | `	sxu32 y = c->S[0][a] + c->S[1][b];` |
| 214277345 |  303 | `	y = y ^ c->S[2][cc];` |
| 214277345 |  304 | `	y = y + c->S[3][d];` |
| 214277345 |  305 | `	return y;` |
|         1 |  306 |  |
|         - |  307 | `/* Encrypt one 64-bit block (xl\|\|xr) in place. */` |
|  13392335 |  308 | `static void Blowfish_encipher(blf_ctx *c,sxu32 *xl,sxu32 *xr){` |
|  13392335 |  309 | `	sxu32 Xl = *xl,Xr = *xr,temp;` |
|         - |  310 | `	int i;` |
| 227669679 |  311 | `	for( i = 0; i < BLF_N; i++ ){` |
| 214277345 |  312 | `		Xl = Xl ^ c->P[i];` |
| 214277345 |  313 | `		Xr = Blowfish_F(c,Xl) ^ Xr;` |
| 214277345 |  314 | `		temp = Xl; Xl = Xr; Xr = temp;   /* swap */` |
| 107138673 |  315 | `	}` |
|  13392335 |  316 | `	temp = Xl; Xl = Xr; Xr = temp;       /* undo the final swap */` |
|  13392335 |  317 | `	Xr = Xr ^ c->P[BLF_N];` |
|  13392335 |  318 | `	Xl = Xl ^ c->P[BLF_N + 1];` |
|  13392335 |  319 | `	*xl = Xl; *xr = Xr;` |
|  13392335 |  320 |  |
|         - |  321 | `/* Encrypt nBlocks consecutive 64-bit blocks (ECB). */` |
|      1921 |  322 | `static void Blowfish_encrypt(blf_ctx *c,sxu32 *data,int nBlocks){` |
|         - |  323 | `	int i;` |
|      7681 |  324 | `	for( i = 0; i < nBlocks; i++ ){` |
|      5761 |  325 | `		Blowfish_encipher(c,&data[i*2],&data[i*2+1]);` |
|      2881 |  326 | `	}` |
|      1921 |  327 |  |
|         - |  328 | `/* Read 4 bytes (big-endian) from data, cycling at databytes; advance *pCur.` |
|         - |  329 | ` * Bytes are unsigned (the corrected "$2y" behaviour — no sign extension). */` |
|    493933 |  330 | `static sxu32 Blowfish_stream2word(const unsigned char *data,sxu32 databytes,sxu32 *pCur){` |
|         - |  331 | `	int i;` |
|    493933 |  332 | `	sxu32 temp = 0,j = *pCur;` |
|   2469661 |  333 | `	for( i = 0; i < 4; i++,j++ ){` |
|   1975729 |  334 | `		if( j >= databytes ){ j = 0; }` |
|   1975729 |  335 | `		temp = (temp << 8) \| (sxu32)data[j];` |
|    987865 |  336 | `	}` |
|    493933 |  337 | `	*pCur = j;` |
|    493933 |  338 | `	return temp;` |
|         1 |  339 |  |
|        31 |  340 | `static void Blowfish_initstate(blf_ctx *c){` |
|        31 |  341 | `	SyMemcpy((const void *)ORIG_S,(void *)c->S,sizeof(c->S));` |
|        31 |  342 | `	SyMemcpy((const void *)ORIG_P,(void *)c->P,sizeof(c->P));` |
|        31 |  343 |  |
|         - |  344 | `/* Standard (unsalted) key expansion. */` |
|     25665 |  345 | `static void Blowfish_expand0state(blf_ctx *c,const unsigned char *key,sxu32 keybytes){` |
|         - |  346 | `	int i,k;` |
|         - |  347 | `	sxu32 j,datal,datar,temp;` |
|     25665 |  348 | `	j = 0;` |
|    487617 |  349 | `	for( i = 0; i < BLF_N + 2; i++ ){` |
|    461953 |  350 | `		temp = Blowfish_stream2word(key,keybytes,&j);` |
|    461953 |  351 | `		c->P[i] = c->P[i] ^ temp;` |
|    230977 |  352 | `	}` |
|     25665 |  353 | `	datal = datar = 0;` |
|    256641 |  354 | `	for( i = 0; i < BLF_N + 2; i += 2 ){` |
|    230977 |  355 | `		Blowfish_encipher(c,&datal,&datar);` |
|    230977 |  356 | `		c->P[i] = datal; c->P[i+1] = datar;` |
|    115489 |  357 | `	}` |
|    128321 |  358 | `	for( i = 0; i < 4; i++ ){` |
|  13242625 |  359 | `		for( k = 0; k < 256; k += 2 ){` |
|  13139969 |  360 | `			Blowfish_encipher(c,&datal,&datar);` |
|  13139969 |  361 | `			c->S[i][k] = datal; c->S[i][k+1] = datar;` |
|   6569985 |  362 | `		}` |
|     51329 |  363 | `	}` |
|     25665 |  364 |  |
|         - |  365 | `/* Salted "expensive" key expansion (the bcrypt ExpandKey). */` |
|        30 |  366 | `static void Blowfish_expandstate(blf_ctx *c,const unsigned char *data,sxu32 databytes,` |
|         1 |  367 | `	const unsigned char *key,sxu32 keybytes){` |
|         - |  368 | `	int i,k;` |
|         - |  369 | `	sxu32 j,datal,datar,temp;` |
|        31 |  370 | `	j = 0;` |
|       571 |  371 | `	for( i = 0; i < BLF_N + 2; i++ ){` |
|       541 |  372 | `		temp = Blowfish_stream2word(key,keybytes,&j);` |
|       541 |  373 | `		c->P[i] = c->P[i] ^ temp;` |
|       271 |  374 | `	}` |
|        31 |  375 | `	datal = datar = 0;` |
|        31 |  376 | `	j = 0;` |
|       301 |  377 | `	for( i = 0; i < BLF_N + 2; i += 2 ){` |
|       271 |  378 | `		datal ^= Blowfish_stream2word(data,databytes,&j);` |
|       271 |  379 | `		datar ^= Blowfish_stream2word(data,databytes,&j);` |
|       271 |  380 | `		Blowfish_encipher(c,&datal,&datar);` |
|       271 |  381 | `		c->P[i] = datal; c->P[i+1] = datar;` |
|       136 |  382 | `	}` |
|       151 |  383 | `	for( i = 0; i < 4; i++ ){` |
|     15481 |  384 | `		for( k = 0; k < 256; k += 2 ){` |
|     15361 |  385 | `			datal ^= Blowfish_stream2word(data,databytes,&j);` |
|     15361 |  386 | `			datar ^= Blowfish_stream2word(data,databytes,&j);` |
|     15361 |  387 | `			Blowfish_encipher(c,&datal,&datar);` |
|     15361 |  388 | `			c->S[i][k] = datal; c->S[i][k+1] = datar;` |
|      7681 |  389 | `		}` |
|        61 |  390 | `	}` |
|        31 |  391 |  |
|         - |  392 |  |
|         - |  393 | `/* bcrypt-base64 alphabet: '.','/', then A-Z, a-z, 0-9. */` |
|         - |  394 | `static const char zB64[] = "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";` |
|         - |  395 | `/* Map a bcrypt-base64 character to its 0..63 value, or 255 if invalid. */` |
|       397 |  396 | `static int BcryptB64Value(int c){` |
|       397 |  397 | `	const char *p = zB64;` |
|       397 |  398 | `	int i = 0;` |
|      9662 |  399 | `	for(; i < 64; i++ ){` |
|      9662 |  400 | `		if( p[i] == c ){ return i; }` |
|      4598 |  401 | `	}` |
|       ! 0 |  402 | `	return 255;` |
|       199 |  403 |  |
|         - |  404 | `/* Encode nIn bytes as bcrypt-base64 into zOut (no padding); returns char count. */` |
|        61 |  405 | `static int BcryptB64Encode(char *zOut,const unsigned char *pIn,sxu32 nIn){` |
|        61 |  406 | `	sxu32 i = 0;` |
|        61 |  407 | `	int n = 0;` |
|         - |  408 | `	unsigned int c1,c2;` |
|       421 |  409 | `	while( i < nIn ){` |
|       421 |  410 | `		c1 = pIn[i++];` |
|       421 |  411 | `		zOut[n++] = zB64[(c1 >> 2) & 0x3f];` |
|       421 |  412 | `		c1 = (c1 & 0x03) << 4;` |
|       421 |  413 | `		if( i >= nIn ){ zOut[n++] = zB64[c1 & 0x3f]; break; }` |
|       391 |  414 | `		c2 = pIn[i++];` |
|       391 |  415 | `		c1 \|= (c2 >> 4) & 0x0f;` |
|       391 |  416 | `		zOut[n++] = zB64[c1 & 0x3f];` |
|       391 |  417 | `		c1 = (c2 & 0x0f) << 2;` |
|       391 |  418 | `		if( i >= nIn ){ zOut[n++] = zB64[c1 & 0x3f]; break; }` |
|       361 |  419 | `		c2 = pIn[i++];` |
|       361 |  420 | `		c1 \|= (c2 >> 6) & 0x03;` |
|       361 |  421 | `		zOut[n++] = zB64[c1 & 0x3f];` |
|       361 |  422 | `		zOut[n++] = zB64[c2 & 0x3f];` |
|         1 |  423 | `	}` |
|        61 |  424 | `	return n;` |
|         1 |  425 |  |
|        19 |  426 | `PH7_PRIVATE sxi32 SyBcryptB64Decode(const char *zIn,sxu32 nIn,unsigned char *pOut,sxu32 nOut){` |
|        19 |  427 | `	sxu32 i = 0,o = 0;` |
|         - |  428 | `	int c1,c2,c3,c4;` |
|       109 |  429 | `	while( o < nOut ){` |
|       109 |  430 | `		if( i + 1 >= nIn ){ return SXERR_INVALID; }` |
|       109 |  431 | `		c1 = BcryptB64Value(zIn[i]); c2 = BcryptB64Value(zIn[i+1]);` |
|       109 |  432 | `		if( c1 == 255 \|\| c2 == 255 ){ return SXERR_INVALID; }` |
|       109 |  433 | `		pOut[o++] = (unsigned char)((c1 << 2) \| ((c2 & 0x30) >> 4));` |
|       109 |  434 | `		if( o >= nOut ){ break; }` |
|        91 |  435 | `		if( i + 2 >= nIn ){ return SXERR_INVALID; }` |
|        91 |  436 | `		c3 = BcryptB64Value(zIn[i+2]);` |
|        91 |  437 | `		if( c3 == 255 ){ return SXERR_INVALID; }` |
|        91 |  438 | `		pOut[o++] = (unsigned char)(((c2 & 0x0f) << 4) \| ((c3 & 0x3c) >> 2));` |
|        91 |  439 | `		if( o >= nOut ){ break; }` |
|        91 |  440 | `		if( i + 3 >= nIn ){ return SXERR_INVALID; }` |
|        91 |  441 | `		c4 = BcryptB64Value(zIn[i+3]);` |
|        91 |  442 | `		if( c4 == 255 ){ return SXERR_INVALID; }` |
|        91 |  443 | `		pOut[o++] = (unsigned char)(((c3 & 0x03) << 6) \| c4);` |
|        91 |  444 | `		i += 4;` |
|         1 |  445 | `	}` |
|        19 |  446 | `	return SXRET_OK;` |
|        10 |  447 |  |
|         - |  448 |  |
|        30 |  449 | `PH7_PRIVATE sxi32 SyBcryptHash(const unsigned char *pPwd,sxu32 nPwd,sxu32 nCost,` |
|         1 |  450 | `	const unsigned char aSalt[16],char zOut[60]){` |
|         - |  451 | `	blf_ctx state;` |
|         - |  452 | `	/* "OrpheanBeholderScryDoubt" = 24 bytes = 6 big-endian words. */` |
|         - |  453 | `	static const unsigned char zMagic[24] = {` |
|         - |  454 | `		'O','r','p','h','e','a','n','B','e','h','o','l','d','e','r',` |
|         - |  455 | `		'S','c','r','y','D','o','u','b','t'` |
|         - |  456 | `	};` |
|         - |  457 | `	sxu32 cdata[6];` |
|         - |  458 | `	unsigned char zCipher[24];` |
|         - |  459 | `	unsigned char zKey[73];` |
|         - |  460 | `	sxu32 keylen,j,rounds,k;` |
|         - |  461 | `	int i,n;` |
|        31 |  462 | `	if( nCost < 4 \|\| nCost > 31 ){` |
|       ! 0 |  463 | `		return SXERR_INVALID;` |
|         - |  464 | `	}` |
|         - |  465 | `	/* Key = password bytes + a trailing NUL, capped at 72 bytes total. The whole` |
|         - |  466 | `	 * buffer is zeroed first: only [0,keylen) is ever read (cyclically), but a` |
|         - |  467 | `	 * full init keeps -Wmaybe-uninitialized quiet when the loop is inlined. */` |
|        31 |  468 | `	SyZero(zKey,(sxu32)sizeof(zKey));` |
|        31 |  469 | `	keylen = nPwd + 1;` |
|        31 |  470 | `	if( keylen > 72 ){ keylen = 72; }` |
|       455 |  471 | `	for( j = 0; j < keylen; j++ ){` |
|       425 |  472 | `		zKey[j] = (j < nPwd) ? pPwd[j] : 0;` |
|       213 |  473 | `	}` |
|         - |  474 | `	/* EksBlowfishSetup */` |
|        31 |  475 | `	Blowfish_initstate(&state);` |
|        31 |  476 | `	Blowfish_expandstate(&state,aSalt,16,zKey,keylen);` |
|        31 |  477 | `	rounds = (sxu32)((sxu64)1 << nCost);` |
|     12863 |  478 | `	for( k = 0; k < rounds; k++ ){` |
|     12833 |  479 | `		Blowfish_expand0state(&state,zKey,keylen);` |
|     12833 |  480 | `		Blowfish_expand0state(&state,aSalt,16);` |
|      6417 |  481 | `	}` |
|         - |  482 | `	/* Encrypt the magic string 64 times (3 blocks each). */` |
|        31 |  483 | `	j = 0;` |
|       211 |  484 | `	for( i = 0; i < 6; i++ ){` |
|       181 |  485 | `		cdata[i] = Blowfish_stream2word(zMagic,24,&j);` |
|        91 |  486 | `	}` |
|      1951 |  487 | `	for( k = 0; k < 64; k++ ){` |
|      1921 |  488 | `		Blowfish_encrypt(&state,cdata,3);` |
|       961 |  489 | `	}` |
|       211 |  490 | `	for( i = 0; i < 6; i++ ){` |
|       181 |  491 | `		zCipher[i*4]   = (unsigned char)((cdata[i] >> 24) & 0xff);` |
|       181 |  492 | `		zCipher[i*4+1] = (unsigned char)((cdata[i] >> 16) & 0xff);` |
|       181 |  493 | `		zCipher[i*4+2] = (unsigned char)((cdata[i] >> 8) & 0xff);` |
|       181 |  494 | `		zCipher[i*4+3] = (unsigned char)(cdata[i] & 0xff);` |
|        91 |  495 | `	}` |
|         - |  496 | `	/* Assemble "$2y$CC$" + base64(salt,16)=22 + base64(cipher,23)=31 = 60. */` |
|        31 |  497 | `	n = 0;` |
|        31 |  498 | `	zOut[n++] = '$'; zOut[n++] = '2'; zOut[n++] = 'y'; zOut[n++] = '$';` |
|        31 |  499 | `	zOut[n++] = (char)('0' + (nCost / 10));` |
|        31 |  500 | `	zOut[n++] = (char)('0' + (nCost % 10));` |
|        31 |  501 | `	zOut[n++] = '$';` |
|        31 |  502 | `	n += BcryptB64Encode(&zOut[n],aSalt,16);          /* 22 chars → n = 29 */` |
|        31 |  503 | `	BcryptB64Encode(&zOut[n],zCipher,23);             /* 31 chars (drop the 24th byte) */` |
|        31 |  504 | `	return SXRET_OK;` |
|        16 |  505 |  |
|         - |  506 |  |
