# src/ph7/builtin_pack.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 541/583 lines (92.80%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#include <string.h>  /* memset (field padding) */` |
|    - |    7 | `/*` |
|    - |    8 | ` * Section:` |
|    - |    9 | ` *    Binary strings: pack() and unpack().` |
|    - |   10 | ` * Status:` |
|    - |   11 | ` *    Stable.` |
|    - |   12 | ` *` |
|    - |   13 | ` * The format string is a little language php inherited from Perl, and both` |
|    - |   14 | ` * builtins read the same alphabet of 24 codes. A code is one letter, optionally` |
|    - |   15 | `` * followed by a REPEATER -- a decimal count, or `*` for "as many as there are".`` |
|    - |   16 | `` * What the repeater counts is the code's own business: `N4` is four 32-bit`` |
|    - |   17 | `` * words, `a4` is ONE four-byte string field, `x4` is four NUL bytes, and `@4` is`` |
|    - |   18 | ` * an absolute position rather than a count at all.` |
|    - |   19 | ` *` |
|    - |   20 | ` * Modelled on php 8.5's ext/standard/pack.c, whose behaviour is the contract` |
|    - |   21 | ` * here -- including the parts a re-derivation gets wrong: which codes consume an` |
|    - |   22 | ` * argument, that a field wider than its value is PADDED rather than refused, and` |
|    - |   23 | ` * that a diagnostic is raised where php raises it (the preprocessing pass finds` |
|    - |   24 | ` * an unknown code before a single byte is produced, while a short hex string is` |
|    - |   25 | ` * only noticed while the bytes are being laid down).` |
|    - |   26 | ` */` |
|    - |   27 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |   28 | `#define PH7_NEED_BUILTIN_REG 1` |
|    - |   29 | `#endif` |
|    - |   30 | `#ifdef PH7_NEED_BUILTIN_REG` |
|    - |   31 | `/*` |
|    - |   32 | `` * php sizes every count in this family with a C `int` and refuses an output`` |
|    - |   33 | ` * position that would overflow one, so the whole engine is bounded by INT_MAX` |
|    - |   34 | ` * and so is this.` |
|    - |   35 | ` */` |
|    - |   36 | `#define PACK_INT_MAX 2147483647` |
|    - |   37 | `/*` |
|    - |   38 | `` * Byte order of a fixed-width field. MACHINE is what `s`, `S`, `i`, `I`, `l`,`` |
|    - |   39 | `` * `L`, `q`, `Q`, `f` and `d` mean -- the host's own order, which is what makes`` |
|    - |   40 | ` * them the codes to avoid in a file format meant to travel.` |
|    - |   41 | ` */` |
|    - |   42 | `#define PACK_MACHINE 0` |
|    - |   43 | `#define PACK_LITTLE  1` |
|    - |   44 | `#define PACK_BIG     2` |
|    - |   45 | `/*` |
|    - |   46 | ` * Is the host little-endian? Read off the storage of a known value rather than` |
|    - |   47 | ` * taken from a build-time macro, so a port whose makefile gets its endianness` |
|    - |   48 | ` * wrong still packs correctly.` |
|    - |   49 | ` */` |
|  108 |   50 | `static int PackHostIsLittle(void)` |
|    1 |   51 | `{` |
|    - |   52 | `	static const sxu32 uProbe = 1;` |
|  109 |   53 | `	return ((const unsigned char *)&uProbe)[0] == 1;` |
|    1 |   54 | `}` |
|    - |   55 | `/*` |
|    - |   56 | ` * Lay the low nSize bytes of uVal into zOut in the requested order. Written` |
|    - |   57 | ` * with shifts rather than as a memcpy of a native type, so the answer is the` |
|    - |   58 | ` * same on either host: a BIG field is most-significant byte first whatever the` |
|    - |   59 | ` * host does, a LITTLE field least-significant first, and a MACHINE field is` |
|    - |   60 | ` * whichever of the two the host uses -- which is what php's byte-map tables say.` |
|    - |   61 | ` */` |
|  180 |   62 | `static void PackPutInt(char *zOut,sxu64 uVal,int nSize,int iOrder)` |
|    1 |   63 | `{` |
|  301 |   64 | `	int bLittle = (iOrder == PACK_LITTLE) \|\|` |
|  120 |   65 | `		(iOrder == PACK_MACHINE && PackHostIsLittle());` |
|    - |   66 | `	int i;` |
|  827 |   67 | `	for( i = 0 ; i < nSize ; ++i ){` |
|  647 |   68 | `		int nShift = bLittle ? i : (nSize - 1 - i);` |
|  647 |   69 | `		zOut[i] = (char)((uVal >> (8 * nShift)) & 0xFF);` |
|  324 |   70 | `	}` |
|  181 |   71 | `}` |
|    - |   72 | `/*` |
|    - |   73 | ` * Read nSize bytes back out of zIn as an unsigned value in the given order --` |
|    - |   74 | ` * PackPutInt's inverse, and the only reader unpack() uses. Nothing wider than a` |
|    - |   75 | ` * byte is ever dereferenced, so an unaligned field (which is the ordinary case:` |
|    - |   76 | `` * `unpack('Ca/Nb', ...)` puts the 32-bit one at offset 1) is not a misaligned`` |
|    - |   77 | ` * load.` |
|    - |   78 | ` */` |
|   52 |   79 | `static sxu64 PackGetInt(const char *zIn,int nSize,int iOrder)` |
|    1 |   80 | `{` |
|   85 |   81 | `	int bLittle = (iOrder == PACK_LITTLE) \|\|` |
|   32 |   82 | `		(iOrder == PACK_MACHINE && PackHostIsLittle());` |
|   53 |   83 | `	sxu64 uVal = 0;` |
|    - |   84 | `	int i;` |
|  301 |   85 | `	for( i = 0 ; i < nSize ; ++i ){` |
|  249 |   86 | `		int nShift = bLittle ? i : (nSize - 1 - i);` |
|  249 |   87 | `		uVal \|= ((sxu64)(unsigned char)zIn[i]) << (8 * nShift);` |
|  125 |   88 | `	}` |
|   53 |   89 | `	return uVal;` |
|    1 |   90 | `}` |
|    - |   91 | `/*` |
|    - |   92 | ` * A float and a double travel as their IEEE-754 bit pattern, so all four are the` |
|    - |   93 | `` * integer routines above applied to the value's own storage. `f`/`d` are the`` |
|    - |   94 | `` * host's order; `g`/`e` little-endian, `G`/`E` big.`` |
|    - |   95 | ` */` |
|   16 |   96 | `static void PackPutFloat(char *zOut,float fVal,int iOrder)` |
|    1 |   97 | `{` |
|   17 |   98 | `	sxu32 uBits = 0;` |
|   17 |   99 | `	SyMemcpy((const void *)&fVal,(void *)&uBits,(sxu32)sizeof(uBits));` |
|   17 |  100 | `	PackPutInt(zOut,(sxu64)uBits,(int)sizeof(uBits),iOrder);` |
|   17 |  101 | `}` |
|   20 |  102 | `static void PackPutDouble(char *zOut,double dVal,int iOrder)` |
|    1 |  103 | `{` |
|   21 |  104 | `	sxu64 uBits = 0;` |
|   21 |  105 | `	SyMemcpy((const void *)&dVal,(void *)&uBits,(sxu32)sizeof(uBits));` |
|   21 |  106 | `	PackPutInt(zOut,uBits,(int)sizeof(uBits),iOrder);` |
|   21 |  107 | `}` |
|    8 |  108 | `static float PackGetFloat(const char *zIn,int iOrder)` |
|    1 |  109 | `{` |
|    9 |  110 | `	sxu32 uBits = (sxu32)PackGetInt(zIn,(int)sizeof(uBits),iOrder);` |
|    9 |  111 | `	float fVal = 0.0f;` |
|    9 |  112 | `	SyMemcpy((const void *)&uBits,(void *)&fVal,(sxu32)sizeof(fVal));` |
|    9 |  113 | `	return fVal;` |
|    1 |  114 | `}` |
|    6 |  115 | `static double PackGetDouble(const char *zIn,int iOrder)` |
|    1 |  116 | `{` |
|    7 |  117 | `	sxu64 uBits = PackGetInt(zIn,(int)sizeof(uBits),iOrder);` |
|    7 |  118 | `	double dVal = 0.0;` |
|    7 |  119 | `	SyMemcpy((const void *)&uBits,(void *)&dVal,(sxu32)sizeof(dVal));` |
|    7 |  120 | `	return dVal;` |
|    1 |  121 | `}` |
|    - |  122 | `/*` |
|    - |  123 | ` * Width in bytes of one repetition of a fixed-width code, and the order it is` |
|    - |  124 | ` * written in. Answers 0 for a code with no fixed width of its own` |
|    - |  125 | `` * (`a`/`A`/`Z`/`h`/`H`/`x`/`X`/`@`), which every caller screens for first.`` |
|    - |  126 | ` *` |
|    - |  127 | `` * `i`/`I` are php's one platform-sized code (`sizeof(int)`), 4 on every target`` |
|    - |  128 | ` * PHL builds for but asked rather than assumed.` |
|    - |  129 | ` */` |
|  826 |  130 | `static int PackFixedWidth(int code,int *piOrder)` |
|    2 |  131 | `{` |
|  828 |  132 | `	int nSize = 0, iOrder = PACK_MACHINE;` |
|  828 |  133 | `	switch( code ){` |
|  280 |  134 | `		case 'c': case 'C': nSize = 1; break;` |
|   29 |  135 | `		case 's': case 'S': nSize = 2; break;` |
|   51 |  136 | `		case 'n': nSize = 2; iOrder = PACK_BIG;    break;` |
|   35 |  137 | `		case 'v': nSize = 2; iOrder = PACK_LITTLE; break;` |
|   21 |  138 | `		case 'i': case 'I': nSize = (int)sizeof(int); break;` |
|   21 |  139 | `		case 'l': case 'L': nSize = 4; break;` |
|  130 |  140 | `		case 'N': nSize = 4; iOrder = PACK_BIG;    break;` |
|   19 |  141 | `		case 'V': nSize = 4; iOrder = PACK_LITTLE; break;` |
|   21 |  142 | `		case 'q': case 'Q': nSize = 8; break;` |
|   35 |  143 | `		case 'J': nSize = 8; iOrder = PACK_BIG;    break;` |
|   27 |  144 | `		case 'P': nSize = 8; iOrder = PACK_LITTLE; break;` |
|   29 |  145 | `		case 'f': nSize = (int)sizeof(float); break;` |
|   19 |  146 | `		case 'g': nSize = (int)sizeof(float);  iOrder = PACK_LITTLE; break;` |
|   27 |  147 | `		case 'G': nSize = (int)sizeof(float);  iOrder = PACK_BIG;    break;` |
|   19 |  148 | `		case 'd': nSize = (int)sizeof(double); break;` |
|   19 |  149 | `		case 'e': nSize = (int)sizeof(double); iOrder = PACK_LITTLE; break;` |
|   51 |  150 | `		case 'E': nSize = (int)sizeof(double); iOrder = PACK_BIG;    break;` |
|   12 |  151 | `		default: break;` |
|    - |  152 | `	}` |
|  828 |  153 | `	if( piOrder ){` |
|  281 |  154 | `		*piOrder = iOrder;` |
|  140 |  155 | `	}` |
|  828 |  156 | `	return nSize;` |
|    2 |  157 | `}` |
|    - |  158 | `/* Does this code take its value as a STRING rather than as a number? */` |
|  862 |  159 | `static int PackCodeTakesString(int code)` |
|    2 |  160 | `{` |
| 1117 |  161 | `	return code == 'a' \|\| code == 'A' \|\| code == 'Z' \|\|` |
| 1225 |  162 | `		code == 'h' \|\| code == 'H';` |
|    2 |  163 | `}` |
|    - |  164 | `/* Is this code one of the three that produce bytes out of no argument at all? */` |
|  752 |  165 | `static int PackCodeTakesNoArg(int code)` |
|    2 |  166 | `{` |
|  754 |  167 | `	return code == 'x' \|\| code == 'X' \|\| code == '@';` |
|    2 |  168 | `}` |
|    - |  169 | `/*` |
|    - |  170 | ``  * Read the repeater that follows a format code: a run of decimal digits, `*` `` |
|    - |  171 | ` * (answered as -1), or nothing at all (answered as 1). *pzCur is advanced past` |
|    - |  172 | ` * whatever was consumed.` |
|    - |  173 | ` *` |
|    - |  174 | ` * php parses the digits with atoi(), so past INT_MAX the answer is the C` |
|    - |  175 | ` * library's: glibc truncates the parsed long into an int, and` |
|    - |  176 | `` * `pack('a99999999999','xy')` really does allocate the 1215752191 bytes that`` |
|    - |  177 | `` * wrap lands on while `pack('x2147483648')` wraps NEGATIVE and is read as `*`.`` |
|    - |  178 | ` * PHL refuses the whole class instead, using the message php raises for the same` |
|    - |  179 | ` * overflow one step later. A count no int can hold is not a count.` |
|    - |  180 | ` */` |
|  542 |  181 | `static sxi32 PackReadRepeat(const char **pzCur,const char *zEnd,sxi64 *piRepeat)` |
|    2 |  182 | `{` |
|  544 |  183 | `	const char *zCur = *pzCur;` |
|  544 |  184 | `	sxi64 iVal = 0;` |
|  544 |  185 | `	*piRepeat = 1;` |
|  544 |  186 | `	if( zCur >= zEnd ){` |
|  169 |  187 | `		return SXRET_OK;` |
|    - |  188 | `	}` |
|  376 |  189 | `	if( zCur[0] == '*' ){` |
|   81 |  190 | `		*pzCur = zCur + 1;` |
|   81 |  191 | `		*piRepeat = -1;` |
|   81 |  192 | `		return SXRET_OK;` |
|    - |  193 | `	}` |
|  296 |  194 | `	if( zCur[0] < '0' \|\| zCur[0] > '9' ){` |
|  107 |  195 | `		return SXRET_OK;` |
|    - |  196 | `	}` |
|  682 |  197 | `	while( zCur < zEnd && zCur[0] >= '0' && zCur[0] <= '9' ){` |
|  494 |  198 | `		if( iVal <= PACK_INT_MAX ){` |
|  462 |  199 | `			iVal = iVal * 10 + (zCur[0] - '0');` |
|  230 |  200 | `		}` |
|  494 |  201 | `		zCur++;` |
|    2 |  202 | `	}` |
|  190 |  203 | `	*pzCur = zCur;` |
|  190 |  204 | `	*piRepeat = iVal;` |
|  190 |  205 | `	return iVal > PACK_INT_MAX ? SXERR_OVERFLOW : SXRET_OK;` |
|  273 |  206 | `}` |
|    - |  207 | `/*` |
|    - |  208 | `` * One preprocessed format entry: the code and its repeater, with `*` already`` |
|    - |  209 | ` * resolved into the count it stands for. php builds the same two arrays for the` |
|    - |  210 | ` * same reason -- the whole format has to be validated, and the output size` |
|    - |  211 | ` * known, before a single byte is written.` |
|    - |  212 | ` */` |
|    - |  213 | `struct PackEntry {` |
|    - |  214 | `	int code;` |
|    - |  215 | `	sxi64 iRepeat;` |
|    - |  216 | `};` |
|    - |  217 | `/*` |
|    - |  218 | `` * The string one `a`/`A`/`Z`/`h`/`H` entry consumes, converted ONCE. php`` |
|    - |  219 | ` * converts the argument in place in its own argument stack, so an array` |
|    - |  220 | `` * argument warns `Array to string conversion` once however many passes read it;`` |
|    - |  221 | ``  * PHL must not write to the caller's value (a spread `pack('a*', ...$rows)` `` |
|    - |  222 | ` * hands the array's own element slots straight in), so the conversion is kept` |
|    - |  223 | ` * here instead.` |
|    - |  224 | ` */` |
|    - |  225 | `struct PackStrArg {` |
|    - |  226 | `	ph7_value sVal;     /* The scratch copy that owns the bytes below */` |
|    - |  227 | `	const char *zStr;` |
|    - |  228 | `	int nStr;` |
|    - |  229 | `	int bDone;          /* Converted already (the format may read it twice) */` |
|    - |  230 | `	int bNotStringable; /* php's catchable Error, predicted rather than raised */` |
|    - |  231 | `};` |
|    - |  232 | `/*` |
|    - |  233 | ` * Convert value argument iIdx to a string once and answer it, emitting php's` |
|    - |  234 | ` * user-visible coercion diagnostics the first time.` |
|    - |  235 | ` *` |
|    - |  236 | ` * Three outcomes: SXRET_OK, SXERR_ABORT for a value that cannot become a string` |
|    - |  237 | ` * at all (an object with no __toString(), php's catchable Error -- PREDICTED` |
|    - |  238 | ` * here rather than raised, so the caller decides when it may interrupt the` |
|    - |  239 | ` * builtin), and any other error status for a __toString() that itself threw,` |
|    - |  240 | ` * which cannot be predicted and stops the call where it happened.` |
|    - |  241 | ` */` |
|  140 |  242 | `static sxi32 PackStringArg(` |
|    - |  243 | `	ph7_context *pCtx,` |
|    - |  244 | `	ph7_value **apArg,` |
|    - |  245 | `	struct PackStrArg *aStr,` |
|    - |  246 | `	int iIdx,` |
|    - |  247 | `	const char **pzStr,` |
|    - |  248 | `	int *pnStr` |
|    2 |  249 | `){` |
|  142 |  250 | `	struct PackStrArg *pSlot = &aStr[iIdx];` |
|  142 |  251 | `	sxi32 rc = SXRET_OK;` |
|  142 |  252 | `	if( !pSlot->bDone ){` |
|  108 |  253 | `		pSlot->bDone = 1;` |
|  108 |  254 | `		pSlot->zStr = "";` |
|  108 |  255 | `		pSlot->nStr = 0;` |
|  108 |  256 | `		PH7_MemObjInit(pCtx->pVm,&pSlot->sVal);` |
|  108 |  257 | `		PH7_MemObjLoad(apArg[iIdx + 1],&pSlot->sVal);` |
|  108 |  258 | `		if( PH7_MemObjIsNotStringable(&pSlot->sVal) ){` |
|    7 |  259 | `			pSlot->bNotStringable = 1;` |
|    4 |  260 | `		}else{` |
|  102 |  261 | `			rc = PH7_ValueToStringUV(pCtx,&pSlot->sVal,&pSlot->zStr,&pSlot->nStr);` |
|  102 |  262 | `			if( rc != SXRET_OK ){` |
|  ! 0 |  263 | `				pSlot->zStr = "";` |
|  ! 0 |  264 | `				pSlot->nStr = 0;` |
|  ! 0 |  265 | `			}` |
|    - |  266 | `		}` |
|   53 |  267 | `	}` |
|  142 |  268 | `	*pzStr = pSlot->zStr;` |
|  142 |  269 | `	*pnStr = pSlot->nStr;` |
|  142 |  270 | `	return pSlot->bNotStringable ? SXERR_ABORT : rc;` |
|    2 |  271 | `}` |
|    - |  272 | `/*` |
|    - |  273 | ` * string pack(string $format, mixed ...$values)` |
|    - |  274 | ` *  Pack the given values into a binary string according to $format.` |
|    - |  275 | ` *` |
|    - |  276 | ` * Three passes, php's own: preprocess the format (which resolves every count,` |
|    - |  277 | ` * consumes every argument and raises every ValueError), measure the output, then` |
|    - |  278 | `` * write it. Splitting them is what lets `X` and `@` move the cursor backwards`` |
|    - |  279 | ` * over bytes that have already been produced.` |
|    - |  280 | ` */` |
|  312 |  281 | `PH7_PRIVATE int PH7_builtin_pack(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  282 | `{` |
|    - |  283 | `	const char *zFmt,*zCur,*zEnd;` |
|  314 |  284 | `	struct PackEntry *aEntry = 0;` |
|  314 |  285 | `	struct PackStrArg *aStr = 0;` |
|  314 |  286 | `	int nEntry = 0, nFmt = 0, nValue = 0, iValue = 0, i;` |
|  314 |  287 | `	sxi64 iPos = 0, iSize = 0;` |
|  314 |  288 | `	char *zOut = 0;` |
|  314 |  289 | `	sxi32 rc = PH7_OK;` |
|  314 |  290 | `	int iThrowArg = 0;  /* 1-based apArg index of the first unconvertible value */` |
|  314 |  291 | `	if( nArg < 1 ){` |
|    - |  292 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|  ! 0 |  293 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  294 | `		return PH7_OK;` |
|    - |  295 | `	}` |
|  314 |  296 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|  314 |  297 | `	nValue = nArg - 1;` |
|  314 |  298 | `	if( (sxu32)nFmt > (sxu32)(PACK_INT_MAX / (int)sizeof(struct PackEntry)) ){` |
|    - |  299 | `		/* One entry per format byte, so a format long enough to overflow the` |
|    - |  300 | `		 * entry array's own size is refused rather than under-allocated. */` |
|  ! 0 |  301 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  302 | `	}` |
|  314 |  303 | `	if( nFmt > 0 ){` |
|  470 |  304 | `		aEntry = (struct PackEntry *)ph7_context_alloc_chunk(pCtx,` |
|  312 |  305 | `			(unsigned int)((sxu32)nFmt * sizeof(struct PackEntry)),TRUE,TRUE);` |
|  314 |  306 | `		if( aEntry == 0 ){` |
|  ! 0 |  307 | `			return PH7_ContextMemoryError(pCtx);` |
|    - |  308 | `		}` |
|  156 |  309 | `	}` |
|  314 |  310 | `	if( nValue > 0 ){` |
|  407 |  311 | `		aStr = (struct PackStrArg *)ph7_context_alloc_chunk(pCtx,` |
|  270 |  312 | `			(unsigned int)((sxu32)nValue * sizeof(struct PackStrArg)),TRUE,TRUE);` |
|  272 |  313 | `		if( aStr == 0 ){` |
|  ! 0 |  314 | `			return PH7_ContextMemoryError(pCtx);` |
|    - |  315 | `		}` |
|  135 |  316 | `	}` |
|    - |  317 | `	/* Pass 1: read the format, resolve every repeater, consume every argument. */` |
|  314 |  318 | `	zCur = zFmt;` |
|  314 |  319 | `	zEnd = &zFmt[nFmt];` |
|  668 |  320 | `	while( zCur < zEnd ){` |
|  390 |  321 | `		int code = (unsigned char)zCur[0];` |
|    - |  322 | `		sxi64 iRepeat;` |
|    - |  323 | `		int bOverflow;` |
|  390 |  324 | `		zCur++;` |
|  390 |  325 | `		bOverflow = PackReadRepeat(&zCur,zEnd,&iRepeat) != SXRET_OK;` |
|  388 |  326 | `		if( !PackCodeTakesNoArg(code) && !PackCodeTakesString(code)` |
|  251 |  327 | `		 && PackFixedWidth(code,0) < 1 ){` |
|    - |  328 | `			/* Whether the code EXISTS is decided before anything its repeater` |
|    - |  329 | `			 * says, which is php's order: a format is validated left to right and` |
|    - |  330 | `			 * an unknown letter is the first thing wrong with it. */` |
|   13 |  331 | `			rc = PH7_VmThrowException(pCtx,"ValueError",` |
|    4 |  332 | `				"Type %c: unknown format code",code);` |
|   22 |  333 | `			goto Done;` |
|    - |  334 | `		}` |
|  382 |  335 | `		if( bOverflow ){` |
|   25 |  336 | `			rc = PH7_VmThrowException(pCtx,"ValueError",` |
|    8 |  337 | `				"Type %c: integer overflow in format string",code);` |
|   17 |  338 | `			goto Done;` |
|    - |  339 | `		}` |
|  366 |  340 | `		if( PackCodeTakesNoArg(code) ){` |
|   75 |  341 | `			if( iRepeat < 0 ){` |
|   13 |  342 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    4 |  343 | `					"Type %c: '*' ignored",code);` |
|    9 |  344 | `				iRepeat = 1;` |
|    5 |  345 | `			}` |
|  329 |  346 | `		}else if( PackCodeTakesString(code) ){` |
|  112 |  347 | `			if( iValue >= nValue ){` |
|    7 |  348 | `				rc = PH7_VmThrowException(pCtx,"ValueError",` |
|    2 |  349 | `					"Type %c: not enough arguments",code);` |
|    5 |  350 | `				goto Done;` |
|    - |  351 | `			}` |
|  108 |  352 | `			if( iRepeat < 0 ){` |
|    - |  353 | ``				/* `*` is the argument's own length, so the argument has to become a`` |
|    - |  354 | `				 * string HERE -- and one that cannot stops the call where php's` |
|    - |  355 | `				 * try_convert_to_string() stops it, ahead of the unused-argument` |
|    - |  356 | `				 * warning below rather than after it. */` |
|    - |  357 | `				const char *zStr; int nStr;` |
|   37 |  358 | `				sxi32 rcStr = PackStringArg(pCtx,apArg,aStr,iValue,&zStr,&nStr);` |
|   37 |  359 | `				if( rcStr == SXERR_ABORT ){` |
|    3 |  360 | `					rc = PH7_MemObjToStringUV(apArg[iValue + 1]);` |
|    3 |  361 | `					goto Done;` |
|   35 |  362 | `				}else if( rcStr != SXRET_OK ){` |
|  ! 0 |  363 | `					rc = rcStr;` |
|  ! 0 |  364 | `					goto Done;` |
|    - |  365 | `				}` |
|    - |  366 | ``				/* Z is always NUL-terminated, so `Z*` is one byte longer than the`` |
|    - |  367 | `				 * string it was given: pack('Z*','aa') is "aa\0". */` |
|   35 |  368 | `				iRepeat = (sxi64)nStr + (code == 'Z' ? 1 : 0);` |
|   17 |  369 | `			}` |
|  106 |  370 | `			iValue++;` |
|  233 |  371 | `		}else if( PackFixedWidth(code,0) > 0 ){` |
|    - |  372 | `			sxi64 iNext;` |
|  181 |  373 | `			if( iRepeat < 0 ){` |
|    5 |  374 | `				iRepeat = nValue - iValue;` |
|    2 |  375 | `			}` |
|  181 |  376 | `			iNext = (sxi64)iValue + iRepeat;` |
|  181 |  377 | `			if( iNext > nValue ){` |
|    7 |  378 | `				rc = PH7_VmThrowException(pCtx,"ValueError",` |
|    2 |  379 | `					"Type %c: too few arguments",code);` |
|    5 |  380 | `				goto Done;` |
|    - |  381 | `			}` |
|  177 |  382 | `			iValue = (int)iNext;` |
|   88 |  383 | `		}` |
|  356 |  384 | `		aEntry[nEntry].code = code;` |
|  356 |  385 | `		aEntry[nEntry].iRepeat = iRepeat;` |
|  356 |  386 | `		nEntry++;` |
|    2 |  387 | `	}` |
|  280 |  388 | `	if( iValue < nValue ){` |
|   10 |  389 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    3 |  390 | `			"%d arguments unused",nValue - iValue);` |
|    3 |  391 | `	}` |
|    - |  392 | `	/* Pass 2: measure. The answer is the HIGHEST position the cursor reaches,` |
|    - |  393 | ``	 * which is not the last one: `@8X4` ends at 4 having produced 8. */`` |
|  624 |  394 | `	for( i = 0 ; i < nEntry ; ++i ){` |
|  352 |  395 | `		int code = aEntry[i].code;` |
|  352 |  396 | `		sxi64 iRepeat = aEntry[i].iRepeat;` |
|  352 |  397 | `		if( code == 'X' ){` |
|   23 |  398 | `			iPos -= iRepeat;` |
|   23 |  399 | `			if( iPos < 0 ){` |
|   10 |  400 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    3 |  401 | `					"Type %c: outside of string",code);` |
|    7 |  402 | `				iPos = 0;` |
|    4 |  403 | `			}` |
|  341 |  404 | `		}else if( code == '@' ){` |
|   29 |  405 | `			iPos = iRepeat;` |
|   15 |  406 | `		}else{` |
|  302 |  407 | `			sxi64 nUnit = 1, nCount = iRepeat;` |
|  302 |  408 | `			if( code == 'h' \|\| code == 'H' ){` |
|    - |  409 | `				/* Two hex digits to the byte, an odd count rounding up. */` |
|   35 |  410 | `				nCount = iRepeat / 2 + (iRepeat % 2);` |
|  285 |  411 | `			}else if( !PackCodeTakesString(code) && code != 'x' ){` |
|  175 |  412 | `				nUnit = PackFixedWidth(code,0);` |
|   87 |  413 | `			}` |
|  302 |  414 | `			if( (PACK_INT_MAX - iPos) / nUnit < nCount ){` |
|   10 |  415 | `				rc = PH7_VmThrowException(pCtx,"ValueError",` |
|    3 |  416 | `					"Type %c: integer overflow in format string",code);` |
|    7 |  417 | `				goto Done;` |
|    - |  418 | `			}` |
|  296 |  419 | `			iPos += nCount * nUnit;` |
|    - |  420 | `		}` |
|  346 |  421 | `		if( iSize < iPos ){` |
|  280 |  422 | `			iSize = iPos;` |
|  139 |  423 | `		}` |
|  174 |  424 | `	}` |
|  274 |  425 | `	if( iSize > 0 ){` |
|  248 |  426 | `		zOut = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)iSize,TRUE,TRUE);` |
|  248 |  427 | `		if( zOut == 0 ){` |
|  ! 0 |  428 | `			rc = PH7_ContextMemoryError(pCtx);` |
|  ! 0 |  429 | `			goto Done;` |
|    - |  430 | `		}` |
|  123 |  431 | `	}` |
|    - |  432 | `	/* Pass 3: write. */` |
|  274 |  433 | `	iPos = 0;` |
|  274 |  434 | `	iValue = 0;` |
|  612 |  435 | `	for( i = 0 ; i < nEntry ; ++i ){` |
|  340 |  436 | `		int code = aEntry[i].code;` |
|  340 |  437 | `		sxi64 iRepeat = aEntry[i].iRepeat;` |
|  340 |  438 | `		int iOrder = PACK_MACHINE, nWidth;` |
|    - |  439 | `		sxi64 n;` |
|  340 |  440 | `		switch( code ){` |
|   35 |  441 | `			case 'a': case 'A': case 'Z': {` |
|    - |  442 | `				/* One field of exactly iRepeat bytes: the value truncated to fit,` |
|    - |  443 | ``				 * or padded out to the width. `A` pads with spaces, the other two`` |
|    - |  444 | ``				 * with NULs, and `Z` keeps its last byte for the terminator. */`` |
|   72 |  445 | `				sxi64 iRoom = (code == 'Z') ? (iRepeat > 0 ? iRepeat - 1 : 0) : iRepeat;` |
|    - |  446 | `				const char *zStr; int nStr;` |
|   72 |  447 | `				sxi32 rcStr = PackStringArg(pCtx,apArg,aStr,iValue,&zStr,&nStr);` |
|   72 |  448 | `				if( rcStr == SXERR_ABORT ){` |
|    - |  449 | `					/* php raises the Error here and lets pack() run to the end, so` |
|    - |  450 | `					 * PHL cannot raise it here -- an enclosing catch would run` |
|    - |  451 | `					 * before the rest of the format. Remember it for the exit. */` |
|    5 |  452 | `					if( iThrowArg == 0 ){` |
|    5 |  453 | `						iThrowArg = iValue + 1;` |
|    3 |  454 | `					}` |
|   70 |  455 | `				}else if( rcStr != SXRET_OK ){` |
|  ! 0 |  456 | `					rc = rcStr;` |
|  ! 0 |  457 | `					goto Done;` |
|    - |  458 | `				}` |
|   72 |  459 | `				if( iRoom > nStr ){` |
|   24 |  460 | `					iRoom = nStr;` |
|   11 |  461 | `				}` |
|   72 |  462 | `				if( iRepeat > 0 ){` |
|   62 |  463 | `					memset(&zOut[iPos],(code == 'A') ? ' ' : '\0',(size_t)iRepeat);` |
|   30 |  464 | `				}` |
|   72 |  465 | `				if( iRoom > 0 ){` |
|   52 |  466 | `					SyMemcpy((const void *)zStr,(void *)&zOut[iPos],(sxu32)iRoom);` |
|   25 |  467 | `				}` |
|   72 |  468 | `				iPos += iRepeat;` |
|   72 |  469 | `				iValue++;` |
|   72 |  470 | `				break;` |
|    - |  471 | `			}` |
|   17 |  472 | `			case 'h': case 'H': {` |
|    - |  473 | ``				/* One nibble per input character: `h` fills the low nibble of a`` |
|    - |  474 | ``				 * byte first, `H` the high one. */`` |
|   35 |  475 | `				int nShift = (code == 'h') ? 0 : 4;` |
|   35 |  476 | `				int bFirst = 1;` |
|    - |  477 | `				const char *zStr; int nStr;` |
|   35 |  478 | `				sxi32 rcStr = PackStringArg(pCtx,apArg,aStr,iValue,&zStr,&nStr);` |
|   35 |  479 | `				if( rcStr == SXERR_ABORT ){` |
|  ! 0 |  480 | `					if( iThrowArg == 0 ){` |
|  ! 0 |  481 | `						iThrowArg = iValue + 1;` |
|  ! 0 |  482 | `					}` |
|   35 |  483 | `				}else if( rcStr != SXRET_OK ){` |
|  ! 0 |  484 | `					rc = rcStr;` |
|  ! 0 |  485 | `					goto Done;` |
|    - |  486 | `				}` |
|   35 |  487 | `				if( iRepeat > nStr ){` |
|    7 |  488 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    2 |  489 | `						"Type %c: not enough characters in string",code);` |
|    5 |  490 | `					iRepeat = nStr;` |
|    2 |  491 | `				}` |
|   35 |  492 | `				iPos--;` |
|  127 |  493 | `				for( n = 0 ; n < iRepeat ; ++n ){` |
|   93 |  494 | `					int c = (unsigned char)zStr[n];` |
|    - |  495 | `					int digit;` |
|   93 |  496 | `					if( c >= '0' && c <= '9' ){` |
|   39 |  497 | `						digit = c - '0';` |
|   74 |  498 | `					}else if( c >= 'A' && c <= 'F' ){` |
|   13 |  499 | `						digit = c - ('A' - 10);` |
|   49 |  500 | `					}else if( c >= 'a' && c <= 'f' ){` |
|   37 |  501 | `						digit = c - ('a' - 10);` |
|   19 |  502 | `					}else{` |
|   10 |  503 | `						ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    3 |  504 | `							"Type %c: illegal hex digit %c",code,c);` |
|    7 |  505 | `						digit = 0;` |
|    - |  506 | `					}` |
|   93 |  507 | `					if( bFirst ){` |
|    - |  508 | `						/* A byte starts here, so the nibble is written into a` |
|    - |  509 | `						 * cleared byte rather than merged into an old one. */` |
|   55 |  510 | `						zOut[++iPos] = 0;` |
|   55 |  511 | `						bFirst = 0;` |
|   28 |  512 | `					}else{` |
|   39 |  513 | `						bFirst = 1;` |
|    - |  514 | `					}` |
|   93 |  515 | `					zOut[iPos] = (char)((unsigned char)zOut[iPos] \| (digit << nShift));` |
|   93 |  516 | `					nShift = (nShift + 4) & 7;` |
|   47 |  517 | `				}` |
|   35 |  518 | `				iPos++;` |
|   35 |  519 | `				iValue++;` |
|   35 |  520 | `				break;` |
|    - |  521 | `			}` |
|    7 |  522 | `			case 'x':` |
|   15 |  523 | `				if( iRepeat > 0 ){` |
|   13 |  524 | `					memset(&zOut[iPos],'\0',(size_t)iRepeat);` |
|    6 |  525 | `				}` |
|   15 |  526 | `				iPos += iRepeat;` |
|   15 |  527 | `				break;` |
|   11 |  528 | `			case 'X':` |
|   23 |  529 | `				iPos -= iRepeat;` |
|   23 |  530 | `				if( iPos < 0 ){` |
|    - |  531 | `					/* Already reported by the measuring pass. */` |
|    7 |  532 | `					iPos = 0;` |
|    3 |  533 | `				}` |
|   23 |  534 | `				break;` |
|   12 |  535 | `			case '@':` |
|   25 |  536 | `				if( iRepeat > iPos ){` |
|   13 |  537 | `					memset(&zOut[iPos],'\0',(size_t)(iRepeat - iPos));` |
|    6 |  538 | `				}` |
|   25 |  539 | `				iPos = iRepeat;` |
|   25 |  540 | `				break;` |
|   87 |  541 | `			default:` |
|    - |  542 | `				/* The numeric codes read their argument through` |
|    - |  543 | `				 * ph7_value_to_int64 / ph7_value_to_double, which coerce IN PLACE` |
|    - |  544 | `				 * -- which is safe only because each value is consumed by exactly` |
|    - |  545 | `				 * one entry and a builtin's arguments are copies (a spread of an` |
|    - |  546 | `				 * array, and a reference inside one, both leave the caller's` |
|    - |  547 | `				 * elements alone; verified against the oracle). The string codes` |
|    - |  548 | `				 * cannot take that route: they are read twice, once to measure and` |
|    - |  549 | `				 * once to write, and a second conversion would emit a second` |
|    - |  550 | ``				 * `Array to string conversion`. */`` |
|  175 |  551 | `				nWidth = PackFixedWidth(code,&iOrder);` |
|  355 |  552 | `				for( n = 0 ; n < iRepeat ; ++n ){` |
|  181 |  553 | `					ph7_value *pVal = apArg[iValue + 1];` |
|  181 |  554 | `					if( code == 'f' \|\| code == 'g' \|\| code == 'G' ){` |
|   17 |  555 | `						PackPutFloat(&zOut[iPos],(float)ph7_value_to_double(pVal),iOrder);` |
|  173 |  556 | `					}else if( code == 'd' \|\| code == 'e' \|\| code == 'E' ){` |
|   21 |  557 | `						PackPutDouble(&zOut[iPos],ph7_value_to_double(pVal),iOrder);` |
|   11 |  558 | `					}else{` |
|  145 |  559 | `						PackPutInt(&zOut[iPos],(sxu64)ph7_value_to_int64(pVal),nWidth,iOrder);` |
|    - |  560 | `					}` |
|  181 |  561 | `					iPos += nWidth;` |
|  181 |  562 | `					iValue++;` |
|   91 |  563 | `				}` |
|  174 |  564 | `				break;` |
|    - |  565 | `		}` |
|  171 |  566 | `	}` |
|  274 |  567 | `	ph7_result_string(pCtx,iPos > 0 ? zOut : "",(int)iPos);` |
|  276 |  568 | `	if( iThrowArg ){` |
|    - |  569 | `		/* The format ran to completion and the result is in place; raise php's` |
|    - |  570 | `		 * Error now. The unwind discards that result, exactly as php's does. */` |
|    5 |  571 | `		rc = PH7_MemObjToStringUV(apArg[iThrowArg]);` |
|    2 |  572 | `	}` |
|  134 |  573 | `Done:` |
|  632 |  574 | `	for( i = 0 ; i < nValue ; ++i ){` |
|  320 |  575 | `		if( aStr[i].bDone ){` |
|  108 |  576 | `			PH7_MemObjRelease(&aStr[i].sVal);` |
|   53 |  577 | `		}` |
|  161 |  578 | `	}` |
|  314 |  579 | `	return rc;` |
|  158 |  580 | `}` |
|    - |  581 | `/*` |
|    - |  582 | ` * php truncates an unpack() element NAME at 200 bytes. The name is whatever` |
|    - |  583 | ` * follows the repeater up to the next '/', so it is the format's own text and` |
|    - |  584 | ` * nothing bounds it otherwise.` |
|    - |  585 | ` */` |
|    - |  586 | `#define UNPACK_MAX_NAME 200` |
|    - |  587 | `/*` |
|    - |  588 | ` * Store one unpacked value under the name this entry gives it. php's three` |
|    - |  589 | ` * naming rules, in its own order: an entry with NO name is keyed by its` |
|    - |  590 | `` * 1-based POSITION within that entry (`unpack('C*', $s)` answers 1,2,3...), a`` |
|    - |  591 | ` * single repetition takes the name as written, and a repeated one has the` |
|    - |  592 | `` * 1-based index appended (`unpack('C3n', $s)` answers n1, n2, n3).`` |
|    - |  593 | ` *` |
|    - |  594 | ` * The key goes in as a php key, so a name that spells an integer becomes an` |
|    - |  595 | `` * integer key exactly as `zend_symtable_update` makes it one.`` |
|    - |  596 | ` */` |
|  130 |  597 | `static sxi32 UnpackStore(` |
|    - |  598 | `	ph7_context *pCtx,` |
|    - |  599 | `	ph7_value *pArray,` |
|    - |  600 | `	ph7_value *pKey,` |
|    - |  601 | `	ph7_value *pVal,` |
|    - |  602 | `	const char *zName,` |
|    - |  603 | `	int nName,` |
|    - |  604 | `	sxi64 iIndex,` |
|    - |  605 | `	int bIndexed` |
|    1 |  606 | `){` |
|  131 |  607 | `	if( nName < 1 ){` |
|   15 |  608 | `		ph7_value_int64(pKey,iIndex);` |
|  124 |  609 | `	}else if( !bIndexed ){` |
|  101 |  610 | `		ph7_value_string(pKey,zName,nName);` |
|   51 |  611 | `	}else{` |
|    - |  612 | `		char zNum[32];` |
|    - |  613 | `		int nNum;` |
|   17 |  614 | `		ph7_value_string(pKey,zName,nName);` |
|   17 |  615 | `		nNum = SyBufferFormat(zNum,sizeof(zNum),"%qd",iIndex);` |
|   17 |  616 | `		ph7_value_string(pKey,zNum,nNum);` |
|    - |  617 | `	}` |
|  131 |  618 | `	if( ph7_array_add_elem(pArray,pKey,pVal) != SXRET_OK ){` |
|  ! 0 |  619 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  620 | `	}` |
|  131 |  621 | `	ph7_value_reset_string_cursor(pKey);` |
|  131 |  622 | `	return PH7_OK;` |
|   66 |  623 | `}` |
|    - |  624 | `/*` |
|    - |  625 | ` * array\|false unpack(string $format, string $string, int $offset = 0)` |
|    - |  626 | ` *  Read a binary string back into an array according to $format.` |
|    - |  627 | ` *` |
|    - |  628 | ` * The mirror of pack(), with two differences that are php's and not` |
|    - |  629 | ` * symmetries: the format is a '/'-separated list of NAMED entries rather than a` |
|    - |  630 | ` * bare run of codes, and a repeater means something slightly different for the` |
|    - |  631 | `` * string codes -- `a5` is one FIVE-BYTE field here as it is there, but `C3` is`` |
|    - |  632 | ` * three separate elements rather than three bytes of one.` |
|    - |  633 | ` *` |
|    - |  634 | ` * Answers FALSE (after a warning) for input that runs out mid-field, which is` |
|    - |  635 | ` * why the whole array is discarded rather than returned half-filled.` |
|    - |  636 | ` */` |
|  152 |  637 | `PH7_PRIVATE int PH7_builtin_unpack(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  638 | `{` |
|    - |  639 | `	const char *zFmt,*zCur,*zFmtEnd,*zIn;` |
|    - |  640 | `	ph7_value *pArray,*pVal,*pKey;` |
|  153 |  641 | `	int nFmt = 0, nIn = 0;` |
|  153 |  642 | `	sxi64 iOffset = 0, iPos = 0;` |
|  153 |  643 | `	sxi32 rc = PH7_OK;` |
|  153 |  644 | `	if( nArg < 2 ){` |
|    - |  645 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|  ! 0 |  646 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  647 | `		return PH7_OK;` |
|    - |  648 | `	}` |
|  153 |  649 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|  153 |  650 | `	zIn = ph7_value_to_string(apArg[1],&nIn);` |
|  153 |  651 | `	if( nArg > 2 ){` |
|  113 |  652 | `		iOffset = ph7_value_to_int64(apArg[2]);` |
|   56 |  653 | `	}` |
|  153 |  654 | `	if( iOffset < 0 \|\| iOffset > nIn ){` |
|    - |  655 | ``		/* php names argument #2 as `$data` here and `$string` in the signature;`` |
|    - |  656 | `		 * the message is its own, verbatim. */` |
|    5 |  657 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  658 | `			"unpack(): Argument #3 ($offset) must be contained in argument #2 ($data)");` |
|    - |  659 | `	}` |
|  149 |  660 | `	zIn += iOffset;` |
|  149 |  661 | `	nIn -= (int)iOffset;` |
|  149 |  662 | `	pArray = ph7_context_new_array(pCtx);` |
|  149 |  663 | `	pVal = ph7_context_new_scalar(pCtx);` |
|  149 |  664 | `	pKey = ph7_context_new_scalar(pCtx);` |
|  149 |  665 | `	if( pArray == 0 \|\| pVal == 0 \|\| pKey == 0 ){` |
|  ! 0 |  666 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  667 | `	}` |
|  149 |  668 | `	zCur = zFmt;` |
|  149 |  669 | `	zFmtEnd = &zFmt[nFmt];` |
|  285 |  670 | `	while( zCur < zFmtEnd ){` |
|  155 |  671 | `		int code = (unsigned char)zCur[0];` |
|    - |  672 | `		const char *zName;` |
|  155 |  673 | `		int nName, iOrder = PACK_MACHINE;` |
|  155 |  674 | `		sxi64 iRepeat, iDeclared, iSize = 0, i;` |
|  155 |  675 | `		zCur++;` |
|  155 |  676 | `		if( PackReadRepeat(&zCur,zFmtEnd,&iRepeat) != SXRET_OK ){` |
|    - |  677 | `			/* Unlike pack()'s atoi, php range-checks this one and answers with a` |
|    - |  678 | `			 * warning and FALSE rather than a wrapped count. */` |
|    7 |  679 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    2 |  680 | `				"Type %c: integer overflow",code);` |
|    5 |  681 | `			ph7_result_bool(pCtx,0);` |
|   12 |  682 | `			return PH7_OK;` |
|    - |  683 | `		}` |
|    - |  684 | `		/* The NAME is everything up to the next '/' -- including spaces, digits` |
|    - |  685 | `		 * and anything else, since only the separator ends it. */` |
|  151 |  686 | `		zName = zCur;` |
|  309 |  687 | `		while( zCur < zFmtEnd && zCur[0] != '/' ){` |
|  159 |  688 | `			zCur++;` |
|    1 |  689 | `		}` |
|  151 |  690 | `		nName = (int)(zCur - zName);` |
|  151 |  691 | `		if( nName > UNPACK_MAX_NAME ){` |
|  ! 0 |  692 | `			nName = UNPACK_MAX_NAME;` |
|  ! 0 |  693 | `		}` |
|  151 |  694 | `		iDeclared = iRepeat;` |
|  151 |  695 | `		switch( code ){` |
|    2 |  696 | `			case 'X':` |
|    - |  697 | `				/* Consumes nothing and moves the cursor BACK one byte per` |
|    - |  698 | `				 * repetition; the negative size is what does the moving. */` |
|    5 |  699 | `				iSize = -1;` |
|    5 |  700 | `				if( iRepeat < 0 ){` |
|    4 |  701 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    1 |  702 | `						"Type %c: '*' ignored",code);` |
|    3 |  703 | `					iRepeat = 1;` |
|    1 |  704 | `				}` |
|    5 |  705 | `				break;` |
|    3 |  706 | `			case '@':` |
|    7 |  707 | `				iSize = 0;` |
|    7 |  708 | `				break;` |
|   10 |  709 | `			case 'a': case 'A': case 'Z':` |
|    - |  710 | `				/* One field of that many bytes, not that many fields. */` |
|   21 |  711 | `				iSize = iRepeat;` |
|   21 |  712 | `				iRepeat = 1;` |
|   21 |  713 | `				break;` |
|    6 |  714 | `			case 'h': case 'H':` |
|    - |  715 | `				/* The repeater counts NIBBLES, so the field is half as many` |
|    - |  716 | `				 * bytes, rounded up. */` |
|   13 |  717 | `				iSize = iRepeat > 0 ? (iRepeat + 1) / 2 : iRepeat;` |
|   13 |  718 | `				iRepeat = 1;` |
|   13 |  719 | `				break;` |
|    1 |  720 | `			case 'x':` |
|    3 |  721 | `				iSize = 1;` |
|    3 |  722 | `				break;` |
|   53 |  723 | `			default:` |
|  107 |  724 | `				iSize = PackFixedWidth(code,&iOrder);` |
|  107 |  725 | `				if( iSize < 1 ){` |
|    7 |  726 | `					rc = PH7_VmThrowException(pCtx,"ValueError",` |
|    2 |  727 | `						"Invalid format type %c",code);` |
|    5 |  728 | `					return rc;` |
|    - |  729 | `				}` |
|  102 |  730 | `				break;` |
|    - |  731 | `		}` |
|  291 |  732 | `		for( i = 0 ; i != iRepeat ; ++i ){` |
|  167 |  733 | `			int bIndexed = (iRepeat != 1);` |
|    - |  734 | `			/* The value slot is reused across repetitions and a string goes in by` |
|    - |  735 | `			 * APPEND, so the previous element has to be let go of first. */` |
|  167 |  736 | `			ph7_value_reset_string_cursor(pVal);` |
|  167 |  737 | `			if( iSize > 0 && (sxi64)PACK_INT_MAX - iSize + 1 < iPos ){` |
|  ! 0 |  738 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|  ! 0 |  739 | `					"Type %c: integer overflow",code);` |
|  ! 0 |  740 | `				ph7_result_bool(pCtx,0);` |
|  ! 0 |  741 | `				return PH7_OK;` |
|    - |  742 | `			}` |
|  167 |  743 | `			if( iPos + iSize > nIn ){` |
|   23 |  744 | `				if( iRepeat < 0 ){` |
|    - |  745 | ``					/* A `*` run simply stops when the input does. */`` |
|   13 |  746 | `					break;` |
|    - |  747 | `				}` |
|   16 |  748 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  749 | `					"Type %c: not enough input values, need %d values but only "` |
|    - |  750 | `					"%qd %s provided",` |
|   10 |  751 | `					code,(int)iSize,(sxi64)(nIn - iPos),` |
|   10 |  752 | `					(nIn - iPos) == 1 ? "was" : "were");` |
|   11 |  753 | `				ph7_result_bool(pCtx,0);` |
|   11 |  754 | `				return PH7_OK;` |
|    - |  755 | `			}` |
|  145 |  756 | `			switch( code ){` |
|    8 |  757 | `				case 'a': case 'A': case 'Z': {` |
|    - |  758 | `					/* All three read the same run of bytes and differ only in` |
|    - |  759 | ``					 * what they keep: `a` everything, `A` minus its trailing`` |
|    - |  760 | ``					 * whitespace and NULs, `Z` up to the first NUL. */`` |
|   17 |  761 | `					sxi64 iLen = nIn - iPos;` |
|   17 |  762 | `					if( iSize >= 0 && iLen > iSize ){` |
|    7 |  763 | `						iLen = iSize;` |
|    3 |  764 | `					}` |
|   17 |  765 | `					iSize = iLen;` |
|   17 |  766 | `					if( code == 'A' ){` |
|   15 |  767 | `						while( --iLen >= 0 ){` |
|   15 |  768 | `							int c = (unsigned char)zIn[iPos + iLen];` |
|   14 |  769 | `							if( c != '\0' && c != ' ' && c != '\t'` |
|    8 |  770 | `							 && c != '\r' && c != '\n' ){` |
|    3 |  771 | `								break;` |
|    - |  772 | `							}` |
|    1 |  773 | `						}` |
|    3 |  774 | `						iLen++;` |
|   16 |  775 | `					}else if( code == 'Z' ){` |
|    - |  776 | `						sxi64 s;` |
|   13 |  777 | `						for( s = 0 ; s < iLen ; ++s ){` |
|   13 |  778 | `							if( zIn[iPos + s] == '\0' ){` |
|    5 |  779 | `								break;` |
|    - |  780 | `							}` |
|    5 |  781 | `						}` |
|    5 |  782 | `						iLen = s;` |
|    2 |  783 | `					}` |
|   17 |  784 | `					ph7_value_string(pVal,&zIn[iPos],(int)iLen);` |
|   17 |  785 | `					break;` |
|    - |  786 | `				}` |
|    6 |  787 | `				case 'h': case 'H': {` |
|    - |  788 | `					/* One hex digit per nibble, in the order that code fills` |
|    - |  789 | `					 * them. An ODD declared count drops the last digit. */` |
|   13 |  790 | `					sxi64 iLen = ((sxi64)nIn - iPos) * 2;` |
|   13 |  791 | `					int nShift = (code == 'h') ? 0 : 4;` |
|   13 |  792 | `					int bFirst = 1;` |
|   13 |  793 | `					sxi64 iIn = 0, iOut;` |
|    - |  794 | `					char *zHex;` |
|   13 |  795 | `					if( iSize > PACK_INT_MAX / 2 ){` |
|    - |  796 | `						/* Asked HERE, where php asks it: a repeater this large has` |
|    - |  797 | `						 * already failed the input-length check above unless the` |
|    - |  798 | `						 * input is enormous. */` |
|  ! 0 |  799 | `						return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  800 | `							"unpack(): Argument #1 ($format) repeater must be less than "` |
|    - |  801 | `							"or equal to %d",PACK_INT_MAX / 2);` |
|    - |  802 | `					}` |
|   13 |  803 | `					if( iSize >= 0 && iLen > iSize * 2 ){` |
|    5 |  804 | `						iLen = iSize * 2;` |
|    2 |  805 | `					}` |
|   13 |  806 | `					if( iLen > 0 && iDeclared > 0 ){` |
|    7 |  807 | `						iLen -= iDeclared % 2;` |
|    3 |  808 | `					}` |
|   19 |  809 | `					zHex = (char *)ph7_context_alloc_chunk(pCtx,` |
|    6 |  810 | `						(unsigned int)(iLen > 0 ? iLen : 1),FALSE,TRUE);` |
|   13 |  811 | `					if( zHex == 0 ){` |
|  ! 0 |  812 | `						return PH7_ContextMemoryError(pCtx);` |
|    - |  813 | `					}` |
|   43 |  814 | `					for( iOut = 0 ; iOut < iLen ; ++iOut ){` |
|   31 |  815 | `						int c = (((unsigned char)zIn[iPos + iIn]) >> nShift) & 0x0F;` |
|   31 |  816 | `						zHex[iOut] = (char)(c < 10 ? c + '0' : c + ('a' - 10));` |
|   31 |  817 | `						nShift = (nShift + 4) & 7;` |
|   31 |  818 | `						if( bFirst ){` |
|   19 |  819 | `							bFirst = 0;` |
|   10 |  820 | `						}else{` |
|   13 |  821 | `							iIn++;` |
|   13 |  822 | `							bFirst = 1;` |
|    - |  823 | `						}` |
|   16 |  824 | `					}` |
|   13 |  825 | `					ph7_value_string(pVal,zHex,(int)(iLen > 0 ? iLen : 0));` |
|   13 |  826 | `					break;` |
|    - |  827 | `				}` |
|   25 |  828 | `				case 'c': case 'C': {` |
|   51 |  829 | `					int c = (unsigned char)zIn[iPos];` |
|   51 |  830 | `					ph7_value_int64(pVal,code == 'c' ? (sxi64)(signed char)c : (sxi64)c);` |
|   51 |  831 | `					break;` |
|    - |  832 | `				}` |
|    2 |  833 | `				case 'x':` |
|    - |  834 | `					/* Skipped, not stored. */` |
|    5 |  835 | `					goto NoOutput;` |
|    2 |  836 | `				case 'X':` |
|    5 |  837 | `					if( iPos < iSize ){` |
|  ! 0 |  838 | `						iPos = -iSize;` |
|  ! 0 |  839 | `						i = iRepeat - 1;` |
|  ! 0 |  840 | `						if( iRepeat >= 0 ){` |
|  ! 0 |  841 | `							ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|  ! 0 |  842 | `								"Type %c: outside of string",code);` |
|  ! 0 |  843 | `						}` |
|  ! 0 |  844 | `					}` |
|    5 |  845 | `					goto NoOutput;` |
|    3 |  846 | `				case '@':` |
|    7 |  847 | `					if( iRepeat <= nIn ){` |
|    5 |  848 | `						iPos = iRepeat;` |
|    3 |  849 | `					}else{` |
|    4 |  850 | `						ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    1 |  851 | `							"Type %c: outside of string",code);` |
|    - |  852 | `					}` |
|    7 |  853 | `					i = iRepeat - 1;   /* php: one @ per entry, whatever it said */` |
|    7 |  854 | `					goto NoOutput;` |
|   26 |  855 | `				default: {` |
|   53 |  856 | `					int nWidth = (int)iSize;` |
|   53 |  857 | `					if( code == 'f' \|\| code == 'g' \|\| code == 'G' ){` |
|    9 |  858 | `						ph7_value_double(pVal,(double)PackGetFloat(&zIn[iPos],iOrder));` |
|   49 |  859 | `					}else if( code == 'd' \|\| code == 'e' \|\| code == 'E' ){` |
|    7 |  860 | `						ph7_value_double(pVal,PackGetDouble(&zIn[iPos],iOrder));` |
|    4 |  861 | `					}else{` |
|   39 |  862 | `						sxu64 uRaw = PackGetInt(&zIn[iPos],nWidth,iOrder);` |
|    - |  863 | `						sxi64 iOut;` |
|    - |  864 | `						/* Only the three SIGNED codes sign-extend; every other` |
|    - |  865 | `						 * width answers the unsigned value it read, which is why` |
|    - |  866 | ``						 * `Q`/`J`/`P` of 0xFF... is -1 and `N` of the same four`` |
|    - |  867 | `						 * bytes is 4294967295. */` |
|   39 |  868 | `						if( code == 's' ){` |
|    3 |  869 | `							iOut = (sxi64)(sxi16)(sxu16)uRaw;` |
|   38 |  870 | `						}else if( code == 'i' ){` |
|    3 |  871 | `							iOut = (sxi64)(int)(unsigned int)uRaw;` |
|   36 |  872 | `						}else if( code == 'l' ){` |
|    3 |  873 | `							iOut = (sxi64)(sxi32)(sxu32)uRaw;` |
|    2 |  874 | `						}else{` |
|   33 |  875 | `							iOut = (sxi64)uRaw;` |
|    - |  876 | `						}` |
|   39 |  877 | `						ph7_value_int64(pVal,iOut);` |
|    - |  878 | `					}` |
|   52 |  879 | `					break;` |
|    - |  880 | `				}` |
|    - |  881 | `			}` |
|  131 |  882 | `			rc = UnpackStore(pCtx,pArray,pKey,pVal,zName,nName,i + 1,bIndexed);` |
|  131 |  883 | `			if( rc != PH7_OK ){` |
|  ! 0 |  884 | `				return rc;` |
|    - |  885 | `			}` |
|   65 |  886 | `NoOutput:` |
|  145 |  887 | `			iPos += iSize;` |
|  145 |  888 | `			if( iPos < 0 ){` |
|    9 |  889 | `				if( iSize != -1 ){` |
|    - |  890 | ``					/* An `X` says so through its own branch above; this is the`` |
|    - |  891 | `					 * cursor landing before the start any other way. */` |
|  ! 0 |  892 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|  ! 0 |  893 | `						"Type %c: outside of string",code);` |
|  ! 0 |  894 | `				}` |
|    9 |  895 | `				iPos = 0;` |
|    4 |  896 | `			}` |
|   73 |  897 | `		}` |
|  137 |  898 | `		if( zCur < zFmtEnd ){` |
|    9 |  899 | `			zCur++;   /* step over the '/' separator */` |
|    4 |  900 | `		}` |
|    1 |  901 | `	}` |
|  131 |  902 | `	ph7_result_value(pCtx,pArray);` |
|  131 |  903 | `	return PH7_OK;` |
|   77 |  904 | `}` |
|    - |  905 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|    - |  906 |  |
