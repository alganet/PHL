# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 516/838 lines (61.58%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#ifdef PH7_ENABLE_PCRE` |
|     - |    6 | `#define PCRE2_CODE_UNIT_WIDTH 8` |
|     - |    7 | `#include <pcre2.h>` |
|     - |    8 | `#include <stdlib.h>` |
|     - |    9 | `#include "ph7int.h"` |
|     - |   10 |  |
|     - |   11 | `/*` |
|     - |   12 | ` * The last-error code lives in ph7_vm::iPcreLastError (per-VM).` |
|     - |   13 | ` *` |
|     - |   14 | ` * The compiled-regex cache below is shared across VMs.  pcre2_code objects` |
|     - |   15 | ` * are immutable after compilation and safe to read concurrently; only the` |
|     - |   16 | ` * insert/evict path mutates the cache, which is fine in PHL's current` |
|     - |   17 | ` * single-threaded-execution model.  If PHL ever runs VMs on parallel` |
|     - |   18 | ` * threads, the cache needs a mutex around PcreCache_Insert.` |
|     - |   19 | ` */` |
|     - |   20 |  |
|     - |   21 | `/* ===== PREG_* constant values (matching PHP) ===== */` |
|     - |   22 | `#define PHP_PREG_PATTERN_ORDER       1` |
|     - |   23 | `#define PHP_PREG_SET_ORDER           2` |
|     - |   24 | `#define PHP_PREG_OFFSET_CAPTURE      256` |
|     - |   25 | `#define PHP_PREG_UNMATCHED_AS_NULL   512` |
|     - |   26 |  |
|     - |   27 | `#define PHP_PREG_SPLIT_NO_EMPTY          1` |
|     - |   28 | `#define PHP_PREG_SPLIT_DELIM_CAPTURE     2` |
|     - |   29 | `#define PHP_PREG_SPLIT_OFFSET_CAPTURE    4` |
|     - |   30 |  |
|     - |   31 | `#define PHP_PREG_NO_ERROR                0` |
|     - |   32 | `#define PHP_PREG_INTERNAL_ERROR          1` |
|     - |   33 | `#define PHP_PREG_BACKTRACK_LIMIT_ERROR   2` |
|     - |   34 | `#define PHP_PREG_RECURSION_LIMIT_ERROR   3` |
|     - |   35 | `#define PHP_PREG_BAD_UTF8_ERROR          4` |
|     - |   36 | `#define PHP_PREG_BAD_UTF8_OFFSET_ERROR   5` |
|     - |   37 | `#define PHP_PREG_JIT_STACKLIMIT_ERROR    6` |
|     - |   38 |  |
|     - |   39 | `/* ===== Compiled-regex cache ===== */` |
|     - |   40 | `#define PCRE_CACHE_SIZE 16` |
|     - |   41 |  |
|     - |   42 | `typedef struct PcreCacheEntry PcreCacheEntry;` |
|     - |   43 | `struct PcreCacheEntry {` |
|     - |   44 | `	char *zPattern;          /* Full PHP pattern string (heap copy) */` |
|     - |   45 | `	sxu32 nLen;` |
|     - |   46 | `	pcre2_code *pCode;` |
|     - |   47 | `	sxu32 nCaptureCount;` |
|     - |   48 | `	sxu32 iLastUsed;` |
|     - |   49 | `};` |
|     - |   50 |  |
|     - |   51 | `static PcreCacheEntry aCache[PCRE_CACHE_SIZE];` |
|     - |   52 | `static sxu32 nCacheUsed = 0;` |
|     - |   53 | `static sxu32 iCacheClock = 0;` |
|     - |   54 |  |
|    50 |   55 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|     5 |   56 |  |
|     - |   57 | `	sxu32 i;` |
|   405 |   58 | `	for( i = 0; i < nCacheUsed; i++ ){` |
|   364 |   59 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|    13 |   60 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|    13 |   61 | `			if( pCaptureCount ){` |
|    13 |   62 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|     6 |   63 | `			}` |
|    13 |   64 | `			return aCache[i].pCode;` |
|     - |   65 | `		}` |
|   177 |   66 | `	}` |
|    43 |   67 | `	return 0;` |
|    30 |   68 |  |
|     - |   69 |  |
|    38 |   70 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|     5 |   71 |  |
|     - |   72 | `	PcreCacheEntry *pEntry;` |
|     - |   73 | `	char *zCopy;` |
|     - |   74 | `	/* Allocate the pattern copy first, before touching the cache */` |
|    43 |   75 | `	zCopy = (char *)malloc(nLen + 1);` |
|    43 |   76 | `	if( zCopy == 0 ){` |
|     - |   77 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|   ! 0 |   78 | `		return;` |
|     - |   79 | `	}` |
|    43 |   80 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|    43 |   81 | `	zCopy[nLen] = 0;` |
|    43 |   82 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|    41 |   83 | `		pEntry = &aCache[nCacheUsed++];` |
|    23 |   84 | `	}else{` |
|     - |   85 | `		/* Evict LRU */` |
|     3 |   86 | `		sxu32 iMin = aCache[0].iLastUsed;` |
|     3 |   87 | `		sxu32 iMinIdx = 0;` |
|     - |   88 | `		sxu32 i;` |
|    33 |   89 | `		for( i = 1; i < PCRE_CACHE_SIZE; i++ ){` |
|    31 |   90 | `			if( aCache[i].iLastUsed < iMin ){` |
|   ! 0 |   91 | `				iMin = aCache[i].iLastUsed;` |
|   ! 0 |   92 | `				iMinIdx = i;` |
|   ! 0 |   93 | `			}` |
|    16 |   94 | `		}` |
|     3 |   95 | `		pEntry = &aCache[iMinIdx];` |
|     3 |   96 | `		pcre2_code_free(pEntry->pCode);` |
|     3 |   97 | `		free(pEntry->zPattern);` |
|     - |   98 | `	}` |
|    43 |   99 | `	pEntry->zPattern = zCopy;` |
|    43 |  100 | `	pEntry->nLen = nLen;` |
|    43 |  101 | `	pEntry->pCode = pCode;` |
|    43 |  102 | `	pEntry->nCaptureCount = nCaptureCount;` |
|    43 |  103 | `	pEntry->iLastUsed = ++iCacheClock;` |
|    24 |  104 |  |
|     - |  105 |  |
|     - |  106 | `/* ===== Delimiter parser ===== */` |
|     - |  107 | `#define PCRE_PARSE_OK             0` |
|     - |  108 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|     - |  109 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */` |
|     - |  110 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|     - |  111 |  |
|    38 |  112 | `static sxi32 PcreParsePattern(` |
|     - |  113 | `	const char *zInput, int nInputLen,` |
|     - |  114 | `	const char **pPattern, int *pnPatternLen,` |
|     - |  115 | `	const char **pFlags, int *pnFlagLen)` |
|     5 |  116 |  |
|    43 |  117 | `	const char *zEnd = &zInput[nInputLen];` |
|    43 |  118 | `	const char *z = zInput;` |
|     - |  119 | `	char cOpen, cClose;` |
|     - |  120 | `	const char *pStart;` |
|     - |  121 |  |
|     - |  122 | `	/* Skip leading whitespace */` |
|    43 |  123 | `	while( z < zEnd && (unsigned char)*z <= 0x20 ){` |
|   ! 0 |  124 | `		z++;` |
|   ! 0 |  125 | `	}` |
|    43 |  126 | `	if( z >= zEnd ){` |
|   ! 0 |  127 | `		return PCRE_PARSE_EMPTY;` |
|     - |  128 | `	}` |
|    43 |  129 | `	cOpen = *z;` |
|     - |  130 | `	/* Must not be alphanumeric, backslash, or whitespace */` |
|    43 |  131 | `	if( SyisAlphaNum(cOpen) \|\| cOpen == '\\' \|\| (unsigned char)cOpen <= 0x20 ){` |
|   ! 0 |  132 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|     - |  133 | `	}` |
|     - |  134 | `	/* Paired delimiters */` |
|    43 |  135 | `	switch( cOpen ){` |
|   ! 0 |  136 | `		case '(': cClose = ')'; break;` |
|   ! 0 |  137 | `		case '[': cClose = ']'; break;` |
|   ! 0 |  138 | `		case '{': cClose = '}'; break;` |
|   ! 0 |  139 | `		case '<': cClose = '>'; break;` |
|    43 |  140 | `		default:  cClose = cOpen; break;` |
|     - |  141 | `	}` |
|    43 |  142 | `	z++; /* Skip opening delimiter */` |
|    43 |  143 | `	pStart = z;` |
|     - |  144 | `	/* Scan for closing delimiter, respecting backslash escapes */` |
|   361 |  145 | `	while( z < zEnd ){` |
|   361 |  146 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|    41 |  147 | `			z += 2; /* Skip escaped char */` |
|    41 |  148 | `			continue;` |
|     - |  149 | `		}` |
|   321 |  150 | `		if( *z == cClose ){` |
|    43 |  151 | `			break;` |
|     - |  152 | `		}` |
|   283 |  153 | `		z++;` |
|     5 |  154 | `	}` |
|    43 |  155 | `	if( z >= zEnd ){` |
|   ! 0 |  156 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|     - |  157 | `	}` |
|    43 |  158 | `	*pPattern = pStart;` |
|    43 |  159 | `	*pnPatternLen = (int)(z - pStart);` |
|    43 |  160 | `	z++; /* Skip closing delimiter */` |
|    43 |  161 | `	*pFlags = z;` |
|    43 |  162 | `	*pnFlagLen = (int)(zEnd - z);` |
|    43 |  163 | `	return PH7_OK;` |
|    24 |  164 |  |
|     - |  165 |  |
|     - |  166 | `/* ===== Flag mapper ===== */` |
|    38 |  167 | `static sxi32 PcreMapFlags(` |
|     - |  168 | `	const char *zFlags, int nFlagLen,` |
|     - |  169 | `	uint32_t *pCompileOpts)` |
|     5 |  170 |  |
|     - |  171 | `	int i;` |
|    43 |  172 | `	*pCompileOpts = 0;` |
|    55 |  173 | `	for( i = 0; i < nFlagLen; i++ ){` |
|    13 |  174 | `		switch( zFlags[i] ){` |
|     9 |  175 | `			case 'i': *pCompileOpts \|= PCRE2_CASELESS; break;` |
|     3 |  176 | `			case 'm': *pCompileOpts \|= PCRE2_MULTILINE; break;` |
|     3 |  177 | `			case 's': *pCompileOpts \|= PCRE2_DOTALL; break;` |
|   ! 0 |  178 | `			case 'x': *pCompileOpts \|= PCRE2_EXTENDED; break;` |
|   ! 0 |  179 | `			case 'u': *pCompileOpts \|= PCRE2_UTF \| PCRE2_UCP; break;` |
|   ! 0 |  180 | `			case 'A': *pCompileOpts \|= PCRE2_ANCHORED; break;` |
|   ! 0 |  181 | `			case 'D': *pCompileOpts \|= PCRE2_DOLLAR_ENDONLY; break;` |
|   ! 0 |  182 | `			case 'U': *pCompileOpts \|= PCRE2_UNGREEDY; break;` |
|   ! 0 |  183 | `			case 'J': *pCompileOpts \|= PCRE2_DUPNAMES; break;` |
|   ! 0 |  184 | `			case 'S': /* Study hint — no-op in PCRE2 */ break;` |
|   ! 0 |  185 | `			default: break;` |
|     - |  186 | `		}` |
|     7 |  187 | `	}` |
|    43 |  188 | `	return PH7_OK;` |
|     5 |  189 |  |
|     - |  190 |  |
|     - |  191 | `/* ===== Compile helper ===== */` |
|    50 |  192 | `static pcre2_code *PcreCompile(` |
|     - |  193 | `	ph7_context *pCtx,` |
|     - |  194 | `	const char *zFullPattern, int nLen,` |
|     - |  195 | `	sxu32 *pCaptureCount)` |
|     5 |  196 |  |
|     - |  197 | `	const char *zPat, *zFlags;` |
|     - |  198 | `	int nPatLen, nFlagLen;` |
|     - |  199 | `	uint32_t compileOpts;` |
|     - |  200 | `	pcre2_code *pCode;` |
|     - |  201 | `	PCRE2_SIZE erroffset;` |
|     - |  202 | `	int errcode;` |
|     - |  203 | `	sxu32 nCapture;` |
|     - |  204 | `	sxi32 parseRc;` |
|     - |  205 |  |
|     - |  206 | `	/* Check cache first */` |
|    55 |  207 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|    55 |  208 | `	if( pCode ){` |
|    13 |  209 | `		return pCode;` |
|     - |  210 | `	}` |
|     - |  211 | `	/* Parse delimiter */` |
|    43 |  212 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen);` |
|    43 |  213 | `	if( parseRc != PCRE_PARSE_OK ){` |
|     - |  214 | `		const char *zMsg;` |
|   ! 0 |  215 | `		switch( parseRc ){` |
|   ! 0 |  216 | `			case PCRE_PARSE_EMPTY:         zMsg = "Empty regular expression"; break;` |
|   ! 0 |  217 | `			case PCRE_PARSE_BAD_DELIMITER: zMsg = "Delimiter must not be alphanumeric, backslash, or whitespace"; break;` |
|   ! 0 |  218 | `			default:                       zMsg = "No ending delimiter found"; break;` |
|     - |  219 | `		}` |
|   ! 0 |  220 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, zMsg);` |
|   ! 0 |  221 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|   ! 0 |  222 | `		return 0;` |
|     - |  223 | `	}` |
|     - |  224 | `	/* Map flags */` |
|    43 |  225 | `	PcreMapFlags(zFlags, nFlagLen, &compileOpts);` |
|     - |  226 | `	/* Compile */` |
|    43 |  227 | `	pCode = pcre2_compile(` |
|    19 |  228 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|    19 |  229 | `		compileOpts, &errcode, &erroffset, NULL);` |
|    43 |  230 | `	if( pCode == 0 ){` |
|     - |  231 | `		PCRE2_UCHAR errbuf[256];` |
|   ! 0 |  232 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|   ! 0 |  233 | `		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,` |
|   ! 0 |  234 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|   ! 0 |  235 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|   ! 0 |  236 | `		return 0;` |
|     - |  237 | `	}` |
|     - |  238 | `	/* Get capture count */` |
|    43 |  239 | `	nCapture = 0;` |
|    43 |  240 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|    43 |  241 | `	if( pCaptureCount ){` |
|    43 |  242 | `		*pCaptureCount = nCapture;` |
|    19 |  243 | `	}` |
|     - |  244 | `	/* Cache it */` |
|    43 |  245 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|    43 |  246 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    43 |  247 | `	return pCode;` |
|    30 |  248 |  |
|     - |  249 |  |
|     - |  250 | `/*` |
|     - |  251 | ` * Write a value back to the caller's variable through the stack slot's nIdx.` |
|     - |  252 | ` *` |
|     - |  253 | ` * For a plain positional variable argument the call compiler auto-vivifies` |
|     - |  254 | ` * known by-reference out-params (see GenStateByRefBuiltinMask in compile.c),` |
|     - |  255 | ` * so even a bare undefined variable (e.g. preg_match($p,$s,$m) with $m never` |
|     - |  256 | ` * assigned) arrives with a real nIdx and is written back here, matching PHP's` |
|     - |  257 | ` * reference semantics.` |
|     - |  258 | ` *` |
|     - |  259 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the value` |
|     - |  260 | ` * still lands in the local stack slot) when the argument is not a plain lvalue` |
|     - |  261 | ` * variable: a literal, a function-call result, an array/property subscript` |
|     - |  262 | ` * (element vivification is not wired), or any variable in a call that also uses` |
|     - |  263 | ` * named or spread arguments (compile-time positions no longer map to the` |
|     - |  264 | ` * runtime arg slots, so the compiler conservatively does not vivify).` |
|     - |  265 | ` */` |
|    22 |  266 | `static void PcreStoreByRef(ph7_vm *pVm, ph7_value *pArg, ph7_value *pNewVal)` |
|     5 |  267 |  |
|    27 |  268 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|    27 |  269 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj, pArg->nIdx);` |
|    27 |  270 | `		if( pObj ){` |
|    27 |  271 | `			PH7_MemObjStore(pNewVal, pObj);` |
|    11 |  272 | `		}` |
|    11 |  273 | `	}` |
|    27 |  274 | `	PH7_MemObjStore(pNewVal, pArg);` |
|    27 |  275 |  |
|     - |  276 |  |
|     - |  277 | `/* ===== Map PCRE2 match error to PHP error code ===== */` |
|   ! 0 |  278 | `static void PcreSetMatchError(ph7_vm *pVm, int rc)` |
|   ! 0 |  279 |  |
|   ! 0 |  280 | `	if( rc == PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  281 | `		pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   ! 0 |  282 | `	}else if( rc == PCRE2_ERROR_MATCHLIMIT ){` |
|   ! 0 |  283 | `		pVm->iPcreLastError = PHP_PREG_BACKTRACK_LIMIT_ERROR;` |
|   ! 0 |  284 | `	}else if( rc == PCRE2_ERROR_DEPTHLIMIT` |
|     - |  285 | `#ifdef PCRE2_ERROR_RECURSIONLIMIT` |
|   ! 0 |  286 | `		\|\| rc == PCRE2_ERROR_RECURSIONLIMIT` |
|     - |  287 | `#endif` |
|     - |  288 | `	){` |
|   ! 0 |  289 | `		pVm->iPcreLastError = PHP_PREG_RECURSION_LIMIT_ERROR;` |
|   ! 0 |  290 | `	}else if( rc == PCRE2_ERROR_BADUTFOFFSET ){` |
|   ! 0 |  291 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_OFFSET_ERROR;` |
|   ! 0 |  292 | `	}else if( rc == PCRE2_ERROR_UTF8_ERR1` |
|   ! 0 |  293 | `		\|\| rc == PCRE2_ERROR_UTF8_ERR2 ){` |
|   ! 0 |  294 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_ERROR;` |
|     - |  295 | `#ifdef PCRE2_ERROR_JIT_STACKLIMIT` |
|   ! 0 |  296 | `	}else if( rc == PCRE2_ERROR_JIT_STACKLIMIT ){` |
|   ! 0 |  297 | `		pVm->iPcreLastError = PHP_PREG_JIT_STACKLIMIT_ERROR;` |
|     - |  298 | `#endif` |
|   ! 0 |  299 | `	}else{` |
|   ! 0 |  300 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|     - |  301 | `	}` |
|   ! 0 |  302 |  |
|     - |  303 |  |
|     - |  304 | `/* ===== Helper: populate matches array from ovector ===== */` |
|    28 |  305 | `static void PcrePopulateMatches(` |
|     - |  306 | `	ph7_context *pCtx,` |
|     - |  307 | `	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */` |
|     - |  308 | `	const char *zSubject,` |
|     - |  309 | `	PCRE2_SIZE *ovector,` |
|     - |  310 | `	int nGroups,` |
|     - |  311 | `	pcre2_code *pCode,` |
|     - |  312 | `	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */` |
|     5 |  313 |  |
|    33 |  314 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    33 |  315 | `	ph7_value *pSub = 0;` |
|    33 |  316 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|    33 |  317 | `	PCRE2_SPTR nametable = 0;` |
|     - |  318 | `	int i;` |
|     - |  319 |  |
|    33 |  320 | `	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  321 | `		pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  322 | `	}` |
|    87 |  323 | `	for( i = 0; i < nGroups; i++ ){` |
|    59 |  324 | `		PCRE2_SIZE start = ovector[2 * i];` |
|    59 |  325 | `		PCRE2_SIZE end   = ovector[2 * i + 1];` |
|    59 |  326 | `		if( start == PCRE2_UNSET ){` |
|   ! 0 |  327 | `			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  328 | `				ph7_value_null(pVal);` |
|   ! 0 |  329 | `			}else{` |
|   ! 0 |  330 | `				ph7_value_string(pVal, "", 0);` |
|     - |  331 | `			}` |
|   ! 0 |  332 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  333 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  334 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  335 | `				ph7_value_int(pOff, -1);` |
|   ! 0 |  336 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  337 | `				ph7_array_add_intkey_elem(pArray, i, pSub);` |
|     - |  338 | `				/* Reset sub-array for reuse */` |
|   ! 0 |  339 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  340 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  341 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  342 | `			}else{` |
|   ! 0 |  343 | `				ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  344 | `			}` |
|   ! 0 |  345 | `		}else{` |
|    59 |  346 | `			ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|    59 |  347 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  348 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  349 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  350 | `				ph7_value_int(pOff, (int)start);` |
|   ! 0 |  351 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  352 | `				ph7_array_add_intkey_elem(pArray, i, pSub);` |
|   ! 0 |  353 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  354 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  355 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  356 | `			}else{` |
|    59 |  357 | `				ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  358 | `			}` |
|     - |  359 | `		}` |
|    59 |  360 | `		ph7_value_reset_string_cursor(pVal);` |
|    32 |  361 | `	}` |
|     - |  362 | `	/* Named groups */` |
|    33 |  363 | `	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|    33 |  364 | `	if( namecount > 0 ){` |
|     5 |  365 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     5 |  366 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|    13 |  367 | `		for( i = 0; (uint32_t)i < namecount; i++ ){` |
|     9 |  368 | `			PCRE2_SPTR entry = nametable + i * nameentrysize;` |
|     9 |  369 | `			int groupNum = (entry[0] << 8) \| entry[1];` |
|     9 |  370 | `			const char *zName = (const char *)(entry + 2);` |
|     - |  371 | `			PCRE2_SIZE start, end;` |
|     9 |  372 | `			if( groupNum >= nGroups ) continue;` |
|     9 |  373 | `			start = ovector[2 * groupNum];` |
|     9 |  374 | `			end   = ovector[2 * groupNum + 1];` |
|     9 |  375 | `			if( start == PCRE2_UNSET ){` |
|   ! 0 |  376 | `				if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  377 | `					ph7_value_null(pVal);` |
|   ! 0 |  378 | `				}else{` |
|   ! 0 |  379 | `					ph7_value_string(pVal, "", 0);` |
|     - |  380 | `				}` |
|   ! 0 |  381 | `			}else{` |
|     9 |  382 | `				ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|     - |  383 | `			}` |
|     9 |  384 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  385 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  386 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  387 | `				ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);` |
|   ! 0 |  388 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  389 | `				ph7_array_add_strkey_elem(pArray, zName, pSub);` |
|   ! 0 |  390 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  391 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  392 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  393 | `			}else{` |
|     9 |  394 | `				ph7_array_add_strkey_elem(pArray, zName, pVal);` |
|     - |  395 | `			}` |
|     9 |  396 | `			ph7_value_reset_string_cursor(pVal);` |
|     5 |  397 | `		}` |
|     2 |  398 | `	}` |
|    33 |  399 | `	ph7_context_release_value(pCtx, pVal);` |
|    33 |  400 | `	if( pSub ){` |
|   ! 0 |  401 | `		ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  402 | `	}` |
|    33 |  403 |  |
|     - |  404 |  |
|     - |  405 | `/*` |
|     - |  406 | ` * Quiet whole-pattern match used by FILTER_VALIDATE_REGEXP: compile zPat (a full` |
|     - |  407 | ` * "/.../flags" pattern) and test it against zSub. On a successful attempt returns` |
|     - |  408 | ` * SXRET_OK with *pMatched set to 1 (match) or 0 (no match); returns SXERR_INVALID` |
|     - |  409 | ` * on a compile/match error (the caller treats that as a validation failure). The` |
|     - |  410 | ` * compiled code is owned by PcreCompile's cache, so it is not freed here.` |
|     - |  411 | ` */` |
|     4 |  412 | `PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,` |
|     - |  413 | `	const char *zSub,int nSub,int *pMatched)` |
|     1 |  414 |  |
|     - |  415 | `	pcre2_code *pCode;` |
|     - |  416 | `	pcre2_match_data *pMatchData;` |
|     - |  417 | `	sxu32 nCapture;` |
|     - |  418 | `	int rc;` |
|     5 |  419 | `	*pMatched = 0;` |
|     5 |  420 | `	pCode = PcreCompile(pCtx,zPat,nPat,&nCapture);` |
|     5 |  421 | `	if( pCode == 0 ){` |
|   ! 0 |  422 | `		return SXERR_INVALID;` |
|     - |  423 | `	}` |
|     5 |  424 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode,NULL);` |
|     5 |  425 | `	if( pMatchData == 0 ){` |
|   ! 0 |  426 | `		return SXERR_INVALID;` |
|     - |  427 | `	}` |
|     5 |  428 | `	rc = pcre2_match(pCode,(PCRE2_SPTR)zSub,(PCRE2_SIZE)nSub,0,0,pMatchData,NULL);` |
|     5 |  429 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  430 | `	if( rc < 0 ){` |
|     3 |  431 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  432 | `			PcreSetMatchError(pCtx->pVm,rc);` |
|   ! 0 |  433 | `			return SXERR_INVALID;` |
|     - |  434 | `		}` |
|     3 |  435 | `		return SXRET_OK; /* clean no-match */` |
|     - |  436 | `	}` |
|     3 |  437 | `	*pMatched = 1;` |
|     3 |  438 | `	return SXRET_OK;` |
|     3 |  439 |  |
|     - |  440 | `/* ======================================================================` |
|     - |  441 | ` * preg_match(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  442 | ` * ====================================================================== */` |
|    22 |  443 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  444 |  |
|     - |  445 | `	const char *zPattern, *zSubject;` |
|     - |  446 | `	int nPatLen, nSubLen;` |
|     - |  447 | `	pcre2_code *pCode;` |
|     - |  448 | `	pcre2_match_data *pMatchData;` |
|     - |  449 | `	PCRE2_SIZE *ovector;` |
|     - |  450 | `	sxu32 nCapture;` |
|    27 |  451 | `	PCRE2_SIZE startOffset = 0;` |
|    27 |  452 | `	int iFlags = 0;` |
|     - |  453 | `	int rc;` |
|     - |  454 |  |
|    27 |  455 | `	if( nArg < 2 ){` |
|   ! 0 |  456 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  457 | `			"preg_match() expects at least 2 parameters");` |
|   ! 0 |  458 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  459 | `		return PH7_OK;` |
|     - |  460 | `	}` |
|    27 |  461 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    27 |  462 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    27 |  463 | `	if( nArg >= 4 ){` |
|   ! 0 |  464 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  465 | `	}` |
|    27 |  466 | `	if( nArg >= 5 ){` |
|   ! 0 |  467 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  468 | `	}` |
|    27 |  469 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    27 |  470 | `	if( pCode == 0 ){` |
|   ! 0 |  471 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  472 | `		return PH7_OK;` |
|     - |  473 | `	}` |
|    27 |  474 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    27 |  475 | `	if( pMatchData == 0 ){` |
|   ! 0 |  476 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  477 | `		return PH7_OK;` |
|     - |  478 | `	}` |
|    38 |  479 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    11 |  480 | `		startOffset, 0, pMatchData, NULL);` |
|    27 |  481 | `	if( rc < 0 ){` |
|     5 |  482 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  483 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  484 | `		}` |
|     - |  485 | `		/* Populate empty matches if requested */` |
|     5 |  486 | `		if( nArg >= 3 ){` |
|     5 |  487 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|     5 |  488 | `			PcreStoreByRef(pCtx->pVm, apArg[2], pEmpty);` |
|     5 |  489 | `			ph7_context_release_value(pCtx, pEmpty);` |
|     2 |  490 | `		}` |
|     5 |  491 | `		pcre2_match_data_free(pMatchData);` |
|     5 |  492 | `		ph7_result_int(pCtx, 0);` |
|     5 |  493 | `		return PH7_OK;` |
|     - |  494 | `	}` |
|    23 |  495 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    23 |  496 | `	if( nArg >= 3 ){` |
|     - |  497 | `		/* Populate $matches */` |
|    15 |  498 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    15 |  499 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    15 |  500 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);` |
|     - |  501 | `		/* Write the array back to the caller's variable */` |
|    15 |  502 | `		PcreStoreByRef(pCtx->pVm, apArg[2], pArray);` |
|    15 |  503 | `		ph7_context_release_value(pCtx, pArray);` |
|     5 |  504 | `	}` |
|    23 |  505 | `	pcre2_match_data_free(pMatchData);` |
|    23 |  506 | `	ph7_result_int(pCtx, 1);` |
|    23 |  507 | `	return PH7_OK;` |
|    16 |  508 |  |
|     - |  509 |  |
|     - |  510 | `/* ======================================================================` |
|     - |  511 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  512 | ` * ====================================================================== */` |
|     6 |  513 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  514 |  |
|     - |  515 | `	const char *zPattern, *zSubject;` |
|     - |  516 | `	int nPatLen, nSubLen;` |
|     - |  517 | `	pcre2_code *pCode;` |
|     - |  518 | `	pcre2_match_data *pMatchData;` |
|     - |  519 | `	sxu32 nCapture;` |
|     7 |  520 | `	PCRE2_SIZE startOffset = 0;` |
|     7 |  521 | `	int iFlags = PHP_PREG_PATTERN_ORDER;` |
|     7 |  522 | `	int totalMatches = 0;` |
|     - |  523 | `	int rc;` |
|     - |  524 |  |
|     7 |  525 | `	if( nArg < 2 ){` |
|   ! 0 |  526 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  527 | `			"preg_match_all() expects at least 2 parameters");` |
|   ! 0 |  528 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  529 | `		return PH7_OK;` |
|     - |  530 | `	}` |
|     7 |  531 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     7 |  532 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     7 |  533 | `	if( nArg >= 4 ){` |
|     3 |  534 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     1 |  535 | `	}` |
|     7 |  536 | `	if( nArg >= 5 ){` |
|   ! 0 |  537 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  538 | `	}` |
|     7 |  539 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     7 |  540 | `	if( pCode == 0 ){` |
|   ! 0 |  541 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  542 | `		return PH7_OK;` |
|     - |  543 | `	}` |
|     7 |  544 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     7 |  545 | `	if( pMatchData == 0 ){` |
|   ! 0 |  546 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  547 | `		return PH7_OK;` |
|     - |  548 | `	}` |
|     7 |  549 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  550 | `	{` |
|     7 |  551 | `		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;` |
|     - |  552 |  |
|     7 |  553 | `		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){` |
|     7 |  554 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  555 | `				PCRE2_SIZE *ovector;` |
|    10 |  556 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     3 |  557 | `					startOffset, 0, pMatchData, NULL);` |
|     7 |  558 | `				if( rc < 0 ){` |
|     3 |  559 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     3 |  560 | `					break;` |
|     - |  561 | `				}` |
|     5 |  562 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     5 |  563 | `				if( pOutArray ){` |
|     5 |  564 | `					ph7_value *pSet = ph7_context_new_array(pCtx);` |
|     5 |  565 | `					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, iFlags & ~0xFF);` |
|     5 |  566 | `					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);` |
|     5 |  567 | `					ph7_context_release_value(pCtx, pSet);` |
|     2 |  568 | `				}` |
|     5 |  569 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  570 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  571 | `				}else{` |
|     5 |  572 | `					startOffset = ovector[1];` |
|     - |  573 | `				}` |
|     5 |  574 | `				totalMatches++;` |
|     1 |  575 | `			}` |
|     2 |  576 | `		}else{` |
|     - |  577 | `			/* PREG_PATTERN_ORDER (default) */` |
|     5 |  578 | `			ph7_value **apGroupArrays = 0;` |
|     5 |  579 | `			sxu32 nGroups = nCapture + 1;` |
|     - |  580 | `			sxu32 g;` |
|     5 |  581 | `			if( pOutArray ){` |
|     7 |  582 | `				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,` |
|     2 |  583 | `					sizeof(ph7_value *) * nGroups, TRUE, FALSE);` |
|     5 |  584 | `				if( apGroupArrays ){` |
|    13 |  585 | `					for( g = 0; g < nGroups; g++ ){` |
|     9 |  586 | `						apGroupArrays[g] = ph7_context_new_array(pCtx);` |
|     5 |  587 | `					}` |
|     2 |  588 | `				}` |
|     2 |  589 | `			}` |
|    15 |  590 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  591 | `				PCRE2_SIZE *ovector;` |
|    22 |  592 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     7 |  593 | `					startOffset, 0, pMatchData, NULL);` |
|    15 |  594 | `				if( rc < 0 ){` |
|     5 |  595 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     5 |  596 | `					break;` |
|     - |  597 | `				}` |
|    11 |  598 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    11 |  599 | `				if( apGroupArrays ){` |
|    11 |  600 | `					ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    11 |  601 | `					int nActual = rc;` |
|    29 |  602 | `					for( g = 0; g < nGroups; g++ ){` |
|    28 |  603 | `						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){` |
|    19 |  604 | `							PCRE2_SIZE s = ovector[2*g];` |
|    19 |  605 | `							PCRE2_SIZE e = ovector[2*g+1];` |
|    19 |  606 | `							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  607 | `								ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  608 | `								ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  609 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|   ! 0 |  610 | `								ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  611 | `								ph7_value_int(pOff, (int)s);` |
|   ! 0 |  612 | `								ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  613 | `								ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|   ! 0 |  614 | `								ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  615 | `								ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  616 | `							}else{` |
|    19 |  617 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|    19 |  618 | `								ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  619 | `							}` |
|    10 |  620 | `						}else{` |
|   ! 0 |  621 | `							ph7_value_string(pVal, "", 0);` |
|   ! 0 |  622 | `							ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  623 | `						}` |
|    19 |  624 | `						ph7_value_reset_string_cursor(pVal);` |
|    10 |  625 | `					}` |
|    11 |  626 | `					ph7_context_release_value(pCtx, pVal);` |
|     5 |  627 | `				}` |
|    11 |  628 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  629 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  630 | `				}else{` |
|    11 |  631 | `					startOffset = ovector[1];` |
|     - |  632 | `				}` |
|    11 |  633 | `				totalMatches++;` |
|     1 |  634 | `			}` |
|     5 |  635 | `			if( apGroupArrays ){` |
|    13 |  636 | `				for( g = 0; g < nGroups; g++ ){` |
|     9 |  637 | `					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);` |
|     9 |  638 | `					ph7_context_release_value(pCtx, apGroupArrays[g]);` |
|     5 |  639 | `				}` |
|     5 |  640 | `				ph7_context_free_chunk(pCtx, apGroupArrays);` |
|     2 |  641 | `			}` |
|     - |  642 | `		}` |
|     - |  643 | `		/* Write output array to caller's variable */` |
|     7 |  644 | `		if( pOutArray && nArg >= 3 ){` |
|     7 |  645 | `			PcreStoreByRef(pCtx->pVm, apArg[2], pOutArray);` |
|     7 |  646 | `			ph7_context_release_value(pCtx, pOutArray);` |
|     3 |  647 | `		}` |
|     - |  648 | `	}` |
|     7 |  649 | `	pcre2_match_data_free(pMatchData);` |
|     7 |  650 | `	ph7_result_int(pCtx, totalMatches);` |
|     7 |  651 | `	return PH7_OK;` |
|     4 |  652 |  |
|     - |  653 |  |
|     - |  654 | `/* ======================================================================` |
|     - |  655 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|     - |  656 | ` * ====================================================================== */` |
|     4 |  657 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  658 |  |
|     - |  659 | `	const char *zPattern, *zSubject;` |
|     - |  660 | `	int nPatLen, nSubLen;` |
|     - |  661 | `	pcre2_code *pCode;` |
|     - |  662 | `	pcre2_match_data *pMatchData;` |
|     - |  663 | `	sxu32 nCapture;` |
|     - |  664 | `	ph7_value *pArray;` |
|     - |  665 | `	ph7_value *pVal;` |
|     5 |  666 | `	PCRE2_SIZE startOffset = 0, lastOffset = 0;` |
|     5 |  667 | `	int limit = -1;` |
|     5 |  668 | `	int iFlags = 0;` |
|     5 |  669 | `	int nPieces = 0;` |
|     - |  670 | `	int rc;` |
|     - |  671 |  |
|     5 |  672 | `	if( nArg < 2 ){` |
|   ! 0 |  673 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  674 | `			"preg_split() expects at least 2 parameters");` |
|   ! 0 |  675 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  676 | `		return PH7_OK;` |
|     - |  677 | `	}` |
|     5 |  678 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     5 |  679 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     5 |  680 | `	if( nArg >= 3 ){` |
|     3 |  681 | `		limit = ph7_value_to_int(apArg[2]);` |
|     1 |  682 | `	}` |
|     5 |  683 | `	if( nArg >= 4 ){` |
|   ! 0 |  684 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  685 | `	}` |
|     5 |  686 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     5 |  687 | `	if( pCode == 0 ){` |
|   ! 0 |  688 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  689 | `		return PH7_OK;` |
|     - |  690 | `	}` |
|     5 |  691 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     5 |  692 | `	if( pMatchData == 0 ){` |
|   ! 0 |  693 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  694 | `		return PH7_OK;` |
|     - |  695 | `	}` |
|     5 |  696 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  697 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     5 |  698 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  699 |  |
|    13 |  700 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|    13 |  701 | `		if( limit > 0 && nPieces >= limit - 1 ){` |
|     3 |  702 | `			break; /* Last piece gets the remainder */` |
|     - |  703 | `		}` |
|    16 |  704 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     5 |  705 | `			startOffset, 0, pMatchData, NULL);` |
|    11 |  706 | `		if( rc < 0 ){` |
|     3 |  707 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  708 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  709 | `			}` |
|     3 |  710 | `			break;` |
|     - |  711 | `		}` |
|     - |  712 | `		{` |
|     9 |  713 | `			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     9 |  714 | `			PCRE2_SIZE matchStart = ovector[0];` |
|     9 |  715 | `			PCRE2_SIZE matchEnd = ovector[1];` |
|     9 |  716 | `			int pieceLen = (int)(matchStart - lastOffset);` |
|     - |  717 |  |
|     - |  718 | `			/* Add the piece before the match */` |
|     9 |  719 | `			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| pieceLen > 0 ){` |
|     9 |  720 | `				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  721 | `					ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  722 | `					ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  723 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|   ! 0 |  724 | `					ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  725 | `					ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  726 | `					ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  727 | `					ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  728 | `					ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  729 | `					ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  730 | `				}else{` |
|     9 |  731 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|     9 |  732 | `					ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  733 | `				}` |
|     9 |  734 | `				ph7_value_reset_string_cursor(pVal);` |
|     9 |  735 | `				nPieces++;` |
|     4 |  736 | `			}` |
|     - |  737 | `			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */` |
|     9 |  738 | `			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){` |
|     - |  739 | `				int g;` |
|   ! 0 |  740 | `				for( g = 1; g < rc; g++ ){` |
|   ! 0 |  741 | `					PCRE2_SIZE gs = ovector[2*g];` |
|   ! 0 |  742 | `					PCRE2_SIZE ge = ovector[2*g+1];` |
|     - |  743 | `					int gLen;` |
|   ! 0 |  744 | `					if( gs == PCRE2_UNSET ) continue;` |
|   ! 0 |  745 | `					gLen = (int)(ge - gs);` |
|   ! 0 |  746 | `					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| gLen > 0 ){` |
|   ! 0 |  747 | `						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  748 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  749 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  750 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  751 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  752 | `							ph7_value_int(pOff, (int)gs);` |
|   ! 0 |  753 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  754 | `							ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  755 | `							ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  756 | `							ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  757 | `						}else{` |
|   ! 0 |  758 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  759 | `							ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  760 | `						}` |
|   ! 0 |  761 | `						ph7_value_reset_string_cursor(pVal);` |
|   ! 0 |  762 | `					}` |
|   ! 0 |  763 | `				}` |
|   ! 0 |  764 | `			}` |
|     - |  765 | `			/* Advance */` |
|     9 |  766 | `			lastOffset = matchEnd;` |
|     9 |  767 | `			if( matchEnd == matchStart ){` |
|   ! 0 |  768 | `				startOffset = matchEnd + 1;` |
|   ! 0 |  769 | `			}else{` |
|     9 |  770 | `				startOffset = matchEnd;` |
|     - |  771 | `			}` |
|     - |  772 | `		}` |
|     1 |  773 | `	}` |
|     - |  774 | `	/* Add trailing piece */` |
|     - |  775 | `	{` |
|     5 |  776 | `		int trailLen = nSubLen - (int)lastOffset;` |
|     5 |  777 | `		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| trailLen > 0 ){` |
|     5 |  778 | `			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  779 | `				ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  780 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  781 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|   ! 0 |  782 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  783 | `				ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  784 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  785 | `				ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  786 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  787 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  788 | `			}else{` |
|     5 |  789 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|     5 |  790 | `				ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  791 | `			}` |
|     2 |  792 | `		}` |
|     - |  793 | `	}` |
|     5 |  794 | `	ph7_context_release_value(pCtx, pVal);` |
|     5 |  795 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  796 | `	ph7_result_value(pCtx, pArray);` |
|     5 |  797 | `	ph7_context_release_value(pCtx, pArray);` |
|     5 |  798 | `	return PH7_OK;` |
|     3 |  799 |  |
|     - |  800 |  |
|     - |  801 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|    14 |  802 | `static void PcreExpandBackrefs(` |
|     - |  803 | `	SyBlob *pOut,` |
|     - |  804 | `	const char *zRepl, int nReplLen,` |
|     - |  805 | `	const char *zSubject,` |
|     - |  806 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     1 |  807 |  |
|    15 |  808 | `	const char *zEnd = &zRepl[nReplLen];` |
|    15 |  809 | `	const char *z = zRepl;` |
|     - |  810 |  |
|    33 |  811 | `	while( z < zEnd ){` |
|    19 |  812 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   ! 0 |  813 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|   ! 0 |  814 | `				int g = z[1] - '0';` |
|   ! 0 |  815 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  816 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  817 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  818 | `				}` |
|   ! 0 |  819 | `				z += 2;` |
|   ! 0 |  820 | `				continue;` |
|     - |  821 | `			}` |
|   ! 0 |  822 | `			if( z[1] == '\\' ){` |
|   ! 0 |  823 | `				SyBlobAppend(pOut, "\\", 1);` |
|   ! 0 |  824 | `				z += 2;` |
|   ! 0 |  825 | `				continue;` |
|     - |  826 | `			}` |
|     - |  827 | `			/* Not a backreference — emit literally */` |
|   ! 0 |  828 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  829 | `			z++;` |
|   ! 0 |  830 | `			continue;` |
|     - |  831 | `		}` |
|    19 |  832 | `		if( *z == '$' && z + 1 < zEnd ){` |
|     5 |  833 | `			if( z[1] == '$' ){` |
|   ! 0 |  834 | `				SyBlobAppend(pOut, "$", 1);` |
|   ! 0 |  835 | `				z += 2;` |
|   ! 0 |  836 | `				continue;` |
|     - |  837 | `			}` |
|     5 |  838 | `			if( z[1] == '{' ){` |
|     - |  839 | `				/* ${N} form */` |
|   ! 0 |  840 | `				const char *p = z + 2;` |
|   ! 0 |  841 | `				int g = 0;` |
|   ! 0 |  842 | `				while( p < zEnd && *p >= '0' && *p <= '9' ){` |
|   ! 0 |  843 | `					g = g * 10 + (*p - '0');` |
|   ! 0 |  844 | `					p++;` |
|   ! 0 |  845 | `				}` |
|   ! 0 |  846 | `				if( p < zEnd && *p == '}' ){` |
|   ! 0 |  847 | `					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  848 | `						SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  849 | `							(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  850 | `					}` |
|   ! 0 |  851 | `					z = p + 1;` |
|   ! 0 |  852 | `					continue;` |
|     - |  853 | `				}` |
|     - |  854 | `				/* Not a valid ${N} — emit literally */` |
|   ! 0 |  855 | `				SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  856 | `				z++;` |
|   ! 0 |  857 | `				continue;` |
|     - |  858 | `			}` |
|     5 |  859 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|     - |  860 | `				/* $N or $NN */` |
|     5 |  861 | `				int g = z[1] - '0';` |
|     5 |  862 | `				z += 2;` |
|     - |  863 | `				/* Check for second digit */` |
|     5 |  864 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  865 | `					int g2 = g * 10 + (*z - '0');` |
|   ! 0 |  866 | `					if( g2 < nGroups ){` |
|   ! 0 |  867 | `						g = g2;` |
|   ! 0 |  868 | `						z++;` |
|   ! 0 |  869 | `					}` |
|   ! 0 |  870 | `				}` |
|     5 |  871 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|     7 |  872 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|     4 |  873 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|     2 |  874 | `				}` |
|     5 |  875 | `				continue;` |
|     - |  876 | `			}` |
|     - |  877 | `			/* Not a backreference */` |
|   ! 0 |  878 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  879 | `			z++;` |
|   ! 0 |  880 | `			continue;` |
|     - |  881 | `		}` |
|    15 |  882 | `		SyBlobAppend(pOut, z, 1);` |
|    15 |  883 | `		z++;` |
|     1 |  884 | `	}` |
|    15 |  885 |  |
|     - |  886 |  |
|     - |  887 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|     8 |  888 | `static void PcreDoReplace(` |
|     - |  889 | `	ph7_context *pCtx,` |
|     - |  890 | `	pcre2_code *pCode,` |
|     - |  891 | `	const char *zSubject, int nSubLen,` |
|     - |  892 | `	const char *zRepl, int nReplLen,` |
|     - |  893 | `	int limit,` |
|     - |  894 | `	int *pCount,` |
|     - |  895 | `	SyBlob *pOut)` |
|     1 |  896 |  |
|     - |  897 | `	pcre2_match_data *pMatchData;` |
|     9 |  898 | `	PCRE2_SIZE startOffset = 0;` |
|     9 |  899 | `	int nReplacements = 0;` |
|     - |  900 | `	int rc;` |
|     - |  901 |  |
|     9 |  902 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     9 |  903 | `	if( pMatchData == 0 ) return;` |
|     - |  904 |  |
|    23 |  905 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  906 | `		PCRE2_SIZE *ovector;` |
|    23 |  907 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|    34 |  908 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    11 |  909 | `			startOffset, 0, pMatchData, NULL);` |
|    23 |  910 | `		if( rc < 0 ){` |
|     9 |  911 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  912 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  913 | `			}` |
|     9 |  914 | `			break;` |
|     - |  915 | `		}` |
|    15 |  916 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - |  917 | `		/* Copy text before match */` |
|    15 |  918 | `		if( ovector[0] > startOffset ){` |
|    13 |  919 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     6 |  920 | `		}` |
|     - |  921 | `		/* Expand replacement */` |
|    15 |  922 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|    15 |  923 | `		nReplacements++;` |
|     - |  924 | `		/* Advance */` |
|    15 |  925 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 |  926 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 |  927 | `				SyBlobAppend(pOut, &zSubject[startOffset], 1);` |
|   ! 0 |  928 | `			}` |
|   ! 0 |  929 | `			startOffset = ovector[0] + 1;` |
|   ! 0 |  930 | `		}else{` |
|    15 |  931 | `			startOffset = ovector[1];` |
|     - |  932 | `		}` |
|     1 |  933 | `	}` |
|     - |  934 | `	/* Copy remainder */` |
|     9 |  935 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 |  936 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   ! 0 |  937 | `	}` |
|     9 |  938 | `	if( pCount ){` |
|     9 |  939 | `		*pCount += nReplacements;` |
|     4 |  940 | `	}` |
|     9 |  941 | `	pcre2_match_data_free(pMatchData);` |
|     4 |  942 | `	SXUNUSED(pCtx);` |
|     5 |  943 |  |
|     - |  944 |  |
|     - |  945 | `/* ======================================================================` |
|     - |  946 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - |  947 | ` * ====================================================================== */` |
|     8 |  948 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  949 |  |
|     9 |  950 | `	int limit = -1;` |
|     9 |  951 | `	int count = 0;` |
|     - |  952 |  |
|     9 |  953 | `	if( nArg < 3 ){` |
|   ! 0 |  954 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  955 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 |  956 | `		ph7_result_null(pCtx);` |
|   ! 0 |  957 | `		return PH7_OK;` |
|     - |  958 | `	}` |
|     9 |  959 | `	if( nArg >= 4 ){` |
|     3 |  960 | `		limit = ph7_value_to_int(apArg[3]);` |
|     1 |  961 | `	}` |
|     9 |  962 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  963 |  |
|     - |  964 | `	/* Reject array subjects (not yet supported) */` |
|     9 |  965 | `	if( ph7_value_is_array(apArg[2]) ){` |
|   ! 0 |  966 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  967 | `			"preg_replace(): Array subjects are not yet supported");` |
|   ! 0 |  968 | `		ph7_result_null(pCtx);` |
|   ! 0 |  969 | `		return PH7_OK;` |
|     - |  970 | `	}` |
|     9 |  971 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     - |  972 | `		/* Single pattern + single replacement on a string subject */` |
|     - |  973 | `		const char *zPattern, *zRepl, *zSubject;` |
|     - |  974 | `		int nPatLen, nReplLen, nSubLen;` |
|     - |  975 | `		pcre2_code *pCode;` |
|     - |  976 | `		sxu32 nCapture;` |
|     - |  977 | `		SyBlob sOut;` |
|     - |  978 |  |
|     9 |  979 | `		zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     9 |  980 | `		zRepl = ph7_value_to_string(apArg[1], &nReplLen);` |
|     9 |  981 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|     - |  982 |  |
|     9 |  983 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     9 |  984 | `		if( pCode == 0 ){` |
|   ! 0 |  985 | `			ph7_result_null(pCtx);` |
|   ! 0 |  986 | `			return PH7_OK;` |
|     - |  987 | `		}` |
|     9 |  988 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     9 |  989 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, &count, &sOut);` |
|     9 |  990 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     9 |  991 | `		SyBlobRelease(&sOut);` |
|     5 |  992 | `	}else{` |
|     - |  993 | `		/* TODO: array of patterns — iterate pairs and apply sequentially */` |
|   ! 0 |  994 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  995 | `			"preg_replace() with array patterns is not yet supported");` |
|   ! 0 |  996 | `		ph7_result_null(pCtx);` |
|   ! 0 |  997 | `		return PH7_OK;` |
|     - |  998 | `	}` |
|     - |  999 | `	/* Set &$count if provided */` |
|     9 | 1000 | `	if( nArg >= 5 ){` |
|     - | 1001 | `		ph7_value sCount;` |
|     3 | 1002 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     3 | 1003 | `		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);` |
|     3 | 1004 | `		PH7_MemObjRelease(&sCount);` |
|     1 | 1005 | `	}` |
|     9 | 1006 | `	return PH7_OK;` |
|     5 | 1007 |  |
|     - | 1008 |  |
|     - | 1009 | `/* ======================================================================` |
|     - | 1010 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count]])` |
|     - | 1011 | ` * ====================================================================== */` |
|     6 | 1012 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1013 |  |
|     - | 1014 | `	const char *zPattern, *zSubject;` |
|     - | 1015 | `	int nPatLen, nSubLen;` |
|     - | 1016 | `	pcre2_code *pCode;` |
|     - | 1017 | `	pcre2_match_data *pMatchData;` |
|     - | 1018 | `	sxu32 nCapture;` |
|     - | 1019 | `	SyBlob sOut;` |
|     8 | 1020 | `	PCRE2_SIZE startOffset = 0;` |
|     8 | 1021 | `	int limit = -1;` |
|     8 | 1022 | `	int count = 0;` |
|     - | 1023 | `	int rc;` |
|     - | 1024 |  |
|     8 | 1025 | `	if( nArg < 3 ){` |
|   ! 0 | 1026 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1027 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 | 1028 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1029 | `		return PH7_OK;` |
|     - | 1030 | `	}` |
|     8 | 1031 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     8 | 1032 | `	zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|     8 | 1033 | `	if( nArg >= 4 ){` |
|   ! 0 | 1034 | `		limit = ph7_value_to_int(apArg[3]);` |
|   ! 0 | 1035 | `	}` |
|     8 | 1036 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 | 1037 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1038 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 | 1039 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1040 | `		return PH7_OK;` |
|     - | 1041 | `	}` |
|     8 | 1042 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     8 | 1043 | `	if( pCode == 0 ){` |
|   ! 0 | 1044 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1045 | `		return PH7_OK;` |
|     - | 1046 | `	}` |
|     8 | 1047 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     8 | 1048 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1049 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1050 | `		return PH7_OK;` |
|     - | 1051 | `	}` |
|     8 | 1052 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     8 | 1053 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1054 |  |
|    22 | 1055 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1056 | `		PCRE2_SIZE *ovector;` |
|     - | 1057 | `		ph7_value *pMatchArr;` |
|     - | 1058 | `		ph7_value *apCbArg[1];` |
|     - | 1059 | `		ph7_value sResult;` |
|     - | 1060 | `		const char *zReplacement;` |
|     - | 1061 | `		int nReplLen;` |
|     - | 1062 |  |
|    25 | 1063 | `		if( limit >= 0 && count >= limit ) break;` |
|    32 | 1064 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    10 | 1065 | `			startOffset, 0, pMatchData, NULL);` |
|    22 | 1066 | `		if( rc < 0 ){` |
|     8 | 1067 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1068 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1069 | `			}` |
|     8 | 1070 | `			break;` |
|     - | 1071 | `		}` |
|    16 | 1072 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1073 | `		/* Copy text before match */` |
|    16 | 1074 | `		if( ovector[0] > startOffset ){` |
|    12 | 1075 | `			SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     5 | 1076 | `		}` |
|     - | 1077 | `		/* Build matches array for callback */` |
|    16 | 1078 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    16 | 1079 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, 0);` |
|     - | 1080 | `		/* Call the callback */` |
|    16 | 1081 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    16 | 1082 | `		apCbArg[0] = pMatchArr;` |
|    16 | 1083 | `		if( PH7_VmCallUserFunction(pCtx->pVm, apArg[1], 1, apCbArg, &sResult) == PH7_EXCEPTION ){` |
|     - | 1084 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|   ! 0 | 1085 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1086 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|   ! 0 | 1087 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1088 | `			pcre2_match_data_free(pMatchData);` |
|   ! 0 | 1089 | `			return PH7_EXCEPTION;` |
|     - | 1090 | `		}` |
|     - | 1091 | `		/* Get replacement string from callback result */` |
|    16 | 1092 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    16 | 1093 | `		SyBlobAppend(&sOut, zReplacement, (sxu32)nReplLen);` |
|    16 | 1094 | `		PH7_MemObjRelease(&sResult);` |
|    16 | 1095 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    16 | 1096 | `		count++;` |
|     - | 1097 | `		/* Advance */` |
|    16 | 1098 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 | 1099 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1100 | `				SyBlobAppend(&sOut, &zSubject[startOffset], 1);` |
|   ! 0 | 1101 | `			}` |
|   ! 0 | 1102 | `			startOffset = ovector[0] + 1;` |
|   ! 0 | 1103 | `		}else{` |
|    16 | 1104 | `			startOffset = ovector[1];` |
|     - | 1105 | `		}` |
|     2 | 1106 | `	}` |
|     - | 1107 | `	/* Copy remainder */` |
|     8 | 1108 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1109 | `		SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   ! 0 | 1110 | `	}` |
|     8 | 1111 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     8 | 1112 | `	SyBlobRelease(&sOut);` |
|     8 | 1113 | `	pcre2_match_data_free(pMatchData);` |
|     - | 1114 | `	/* Set &$count if provided */` |
|     8 | 1115 | `	if( nArg >= 5 ){` |
|     - | 1116 | `		ph7_value sCount;` |
|   ! 0 | 1117 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|   ! 0 | 1118 | `		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);` |
|   ! 0 | 1119 | `		PH7_MemObjRelease(&sCount);` |
|   ! 0 | 1120 | `	}` |
|     8 | 1121 | `	return PH7_OK;` |
|     5 | 1122 |  |
|     - | 1123 |  |
|     - | 1124 | `/* ======================================================================` |
|     - | 1125 | ` * preg_quote(str [, delimiter])` |
|     - | 1126 | ` * ====================================================================== */` |
|     6 | 1127 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1128 |  |
|     7 | 1129 | `	const char *zStr, *zDelim = 0;` |
|     7 | 1130 | `	int nLen, nDelimLen = 0;` |
|     - | 1131 | `	const char *z, *zEnd;` |
|     - | 1132 |  |
|     7 | 1133 | `	if( nArg < 1 ){` |
|   ! 0 | 1134 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1135 | `		return PH7_OK;` |
|     - | 1136 | `	}` |
|     7 | 1137 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|     7 | 1138 | `	if( nArg >= 2 ){` |
|     3 | 1139 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     1 | 1140 | `	}` |
|     7 | 1141 | `	z = zStr;` |
|     7 | 1142 | `	zEnd = &zStr[nLen];` |
|    71 | 1143 | `	while( z < zEnd ){` |
|    65 | 1144 | `		char c = *z;` |
|    65 | 1145 | `		switch( c ){` |
|     4 | 1146 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1147 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1148 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1149 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1150 | `			case '#':` |
|     9 | 1151 | `				ph7_result_string(pCtx, "\\", 1);` |
|     9 | 1152 | `				break;` |
|    28 | 1153 | `			default:` |
|    57 | 1154 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     3 | 1155 | `					ph7_result_string(pCtx, "\\", 1);` |
|     1 | 1156 | `				}` |
|    56 | 1157 | `				break;` |
|     - | 1158 | `		}` |
|    65 | 1159 | `		ph7_result_string(pCtx, z, 1);` |
|    65 | 1160 | `		z++;` |
|     1 | 1161 | `	}` |
|     7 | 1162 | `	return PH7_OK;` |
|     4 | 1163 |  |
|     - | 1164 |  |
|     - | 1165 | `/* ======================================================================` |
|     - | 1166 | ` * preg_last_error()` |
|     - | 1167 | ` * ====================================================================== */` |
|   ! 0 | 1168 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1169 |  |
|   ! 0 | 1170 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1171 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1172 | `	return PH7_OK;` |
|   ! 0 | 1173 |  |
|     - | 1174 |  |
|     - | 1175 | `/* ======================================================================` |
|     - | 1176 | ` * preg_last_error_msg()` |
|     - | 1177 | ` * ====================================================================== */` |
|   ! 0 | 1178 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1179 |  |
|     - | 1180 | `	const char *zMsg;` |
|   ! 0 | 1181 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1182 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1183 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1184 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1185 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1186 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1187 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1188 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1189 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1190 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1191 | `	}` |
|   ! 0 | 1192 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1193 | `	return PH7_OK;` |
|   ! 0 | 1194 |  |
|     - | 1195 |  |
|     - | 1196 | `/* ===== Function registration table ===== */` |
|     - | 1197 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1198 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1199 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1200 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1201 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1202 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1203 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1204 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1205 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1206 | `};` |
|     - | 1207 |  |
|  3050 | 1208 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     5 | 1209 |  |
|     - | 1210 | `	sxu32 n;` |
| 27455 | 1211 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 24405 | 1212 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 12205 | 1213 | `	}` |
|  3055 | 1214 |  |
|     - | 1215 |  |
|     - | 1216 | `/* ===== Constant registration ===== */` |
|     - | 1217 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1218 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1219 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1220 | `	}` |
|     - | 1221 |  |
|   ! 0 | 1222 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|     3 | 1223 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|   ! 0 | 1224 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|   ! 0 | 1225 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|   ! 0 | 1226 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|   ! 0 | 1227 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|   ! 0 | 1228 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|   ! 0 | 1229 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|   ! 0 | 1230 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|   ! 0 | 1231 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|   ! 0 | 1232 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|   ! 0 | 1233 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|   ! 0 | 1234 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|   ! 0 | 1235 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1236 |  |
|  3050 | 1237 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     5 | 1238 |  |
|  3055 | 1239 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  3055 | 1240 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  3055 | 1241 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  3055 | 1242 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  3055 | 1243 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  3055 | 1244 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  3055 | 1245 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  3055 | 1246 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  3055 | 1247 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  3055 | 1248 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  3055 | 1249 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  3055 | 1250 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  3055 | 1251 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  3055 | 1252 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  3055 | 1253 |  |
|     - | 1254 |  |
|     - | 1255 | `#else` |
|     - | 1256 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1257 | `typedef int vm_pcre_unused;` |
|     - | 1258 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1259 |  |
