# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 493/819 lines (60.20%)

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
|    46 |   55 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|     5 |   56 |  |
|     - |   57 | `	sxu32 i;` |
|   347 |   58 | `	for( i = 0; i < nCacheUsed; i++ ){` |
|   310 |   59 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|    13 |   60 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|    13 |   61 | `			if( pCaptureCount ){` |
|    13 |   62 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|     6 |   63 | `			}` |
|    13 |   64 | `			return aCache[i].pCode;` |
|     - |   65 | `		}` |
|   150 |   66 | `	}` |
|    39 |   67 | `	return 0;` |
|    28 |   68 |  |
|     - |   69 |  |
|    34 |   70 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|     5 |   71 |  |
|     - |   72 | `	PcreCacheEntry *pEntry;` |
|     - |   73 | `	char *zCopy;` |
|     - |   74 | `	/* Allocate the pattern copy first, before touching the cache */` |
|    39 |   75 | `	zCopy = (char *)malloc(nLen + 1);` |
|    39 |   76 | `	if( zCopy == 0 ){` |
|     - |   77 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|   ! 0 |   78 | `		return;` |
|     - |   79 | `	}` |
|    39 |   80 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|    39 |   81 | `	zCopy[nLen] = 0;` |
|    39 |   82 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|    39 |   83 | `		pEntry = &aCache[nCacheUsed++];` |
|    22 |   84 | `	}else{` |
|     - |   85 | `		/* Evict LRU */` |
|   ! 0 |   86 | `		sxu32 iMin = aCache[0].iLastUsed;` |
|   ! 0 |   87 | `		sxu32 iMinIdx = 0;` |
|     - |   88 | `		sxu32 i;` |
|   ! 0 |   89 | `		for( i = 1; i < PCRE_CACHE_SIZE; i++ ){` |
|   ! 0 |   90 | `			if( aCache[i].iLastUsed < iMin ){` |
|   ! 0 |   91 | `				iMin = aCache[i].iLastUsed;` |
|   ! 0 |   92 | `				iMinIdx = i;` |
|   ! 0 |   93 | `			}` |
|   ! 0 |   94 | `		}` |
|   ! 0 |   95 | `		pEntry = &aCache[iMinIdx];` |
|   ! 0 |   96 | `		pcre2_code_free(pEntry->pCode);` |
|   ! 0 |   97 | `		free(pEntry->zPattern);` |
|     - |   98 | `	}` |
|    39 |   99 | `	pEntry->zPattern = zCopy;` |
|    39 |  100 | `	pEntry->nLen = nLen;` |
|    39 |  101 | `	pEntry->pCode = pCode;` |
|    39 |  102 | `	pEntry->nCaptureCount = nCaptureCount;` |
|    39 |  103 | `	pEntry->iLastUsed = ++iCacheClock;` |
|    22 |  104 |  |
|     - |  105 |  |
|     - |  106 | `/* ===== Delimiter parser ===== */` |
|     - |  107 | `#define PCRE_PARSE_OK             0` |
|     - |  108 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|     - |  109 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */` |
|     - |  110 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|     - |  111 |  |
|    34 |  112 | `static sxi32 PcreParsePattern(` |
|     - |  113 | `	const char *zInput, int nInputLen,` |
|     - |  114 | `	const char **pPattern, int *pnPatternLen,` |
|     - |  115 | `	const char **pFlags, int *pnFlagLen)` |
|     5 |  116 |  |
|    39 |  117 | `	const char *zEnd = &zInput[nInputLen];` |
|    39 |  118 | `	const char *z = zInput;` |
|     - |  119 | `	char cOpen, cClose;` |
|     - |  120 | `	const char *pStart;` |
|     - |  121 |  |
|     - |  122 | `	/* Skip leading whitespace */` |
|    39 |  123 | `	while( z < zEnd && (unsigned char)*z <= 0x20 ){` |
|   ! 0 |  124 | `		z++;` |
|   ! 0 |  125 | `	}` |
|    39 |  126 | `	if( z >= zEnd ){` |
|   ! 0 |  127 | `		return PCRE_PARSE_EMPTY;` |
|     - |  128 | `	}` |
|    39 |  129 | `	cOpen = *z;` |
|     - |  130 | `	/* Must not be alphanumeric, backslash, or whitespace */` |
|    39 |  131 | `	if( SyisAlphaNum(cOpen) \|\| cOpen == '\\' \|\| (unsigned char)cOpen <= 0x20 ){` |
|   ! 0 |  132 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|     - |  133 | `	}` |
|     - |  134 | `	/* Paired delimiters */` |
|    39 |  135 | `	switch( cOpen ){` |
|   ! 0 |  136 | `		case '(': cClose = ')'; break;` |
|   ! 0 |  137 | `		case '[': cClose = ']'; break;` |
|   ! 0 |  138 | `		case '{': cClose = '}'; break;` |
|   ! 0 |  139 | `		case '<': cClose = '>'; break;` |
|    39 |  140 | `		default:  cClose = cOpen; break;` |
|     - |  141 | `	}` |
|    39 |  142 | `	z++; /* Skip opening delimiter */` |
|    39 |  143 | `	pStart = z;` |
|     - |  144 | `	/* Scan for closing delimiter, respecting backslash escapes */` |
|   313 |  145 | `	while( z < zEnd ){` |
|   313 |  146 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|    41 |  147 | `			z += 2; /* Skip escaped char */` |
|    41 |  148 | `			continue;` |
|     - |  149 | `		}` |
|   273 |  150 | `		if( *z == cClose ){` |
|    39 |  151 | `			break;` |
|     - |  152 | `		}` |
|   239 |  153 | `		z++;` |
|     5 |  154 | `	}` |
|    39 |  155 | `	if( z >= zEnd ){` |
|   ! 0 |  156 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|     - |  157 | `	}` |
|    39 |  158 | `	*pPattern = pStart;` |
|    39 |  159 | `	*pnPatternLen = (int)(z - pStart);` |
|    39 |  160 | `	z++; /* Skip closing delimiter */` |
|    39 |  161 | `	*pFlags = z;` |
|    39 |  162 | `	*pnFlagLen = (int)(zEnd - z);` |
|    39 |  163 | `	return PH7_OK;` |
|    22 |  164 |  |
|     - |  165 |  |
|     - |  166 | `/* ===== Flag mapper ===== */` |
|    34 |  167 | `static sxi32 PcreMapFlags(` |
|     - |  168 | `	const char *zFlags, int nFlagLen,` |
|     - |  169 | `	uint32_t *pCompileOpts)` |
|     5 |  170 |  |
|     - |  171 | `	int i;` |
|    39 |  172 | `	*pCompileOpts = 0;` |
|    51 |  173 | `	for( i = 0; i < nFlagLen; i++ ){` |
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
|    39 |  188 | `	return PH7_OK;` |
|     5 |  189 |  |
|     - |  190 |  |
|     - |  191 | `/* ===== Compile helper ===== */` |
|    46 |  192 | `static pcre2_code *PcreCompile(` |
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
|    51 |  207 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|    51 |  208 | `	if( pCode ){` |
|    13 |  209 | `		return pCode;` |
|     - |  210 | `	}` |
|     - |  211 | `	/* Parse delimiter */` |
|    39 |  212 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen);` |
|    39 |  213 | `	if( parseRc != PCRE_PARSE_OK ){` |
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
|    39 |  225 | `	PcreMapFlags(zFlags, nFlagLen, &compileOpts);` |
|     - |  226 | `	/* Compile */` |
|    39 |  227 | `	pCode = pcre2_compile(` |
|    17 |  228 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|    17 |  229 | `		compileOpts, &errcode, &erroffset, NULL);` |
|    39 |  230 | `	if( pCode == 0 ){` |
|     - |  231 | `		PCRE2_UCHAR errbuf[256];` |
|   ! 0 |  232 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|   ! 0 |  233 | `		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,` |
|   ! 0 |  234 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|   ! 0 |  235 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|   ! 0 |  236 | `		return 0;` |
|     - |  237 | `	}` |
|     - |  238 | `	/* Get capture count */` |
|    39 |  239 | `	nCapture = 0;` |
|    39 |  240 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|    39 |  241 | `	if( pCaptureCount ){` |
|    39 |  242 | `		*pCaptureCount = nCapture;` |
|    17 |  243 | `	}` |
|     - |  244 | `	/* Cache it */` |
|    39 |  245 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|    39 |  246 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    39 |  247 | `	return pCode;` |
|    28 |  248 |  |
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
|     - |  405 | `/* ======================================================================` |
|     - |  406 | ` * preg_match(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  407 | ` * ====================================================================== */` |
|    22 |  408 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  409 |  |
|     - |  410 | `	const char *zPattern, *zSubject;` |
|     - |  411 | `	int nPatLen, nSubLen;` |
|     - |  412 | `	pcre2_code *pCode;` |
|     - |  413 | `	pcre2_match_data *pMatchData;` |
|     - |  414 | `	PCRE2_SIZE *ovector;` |
|     - |  415 | `	sxu32 nCapture;` |
|    27 |  416 | `	PCRE2_SIZE startOffset = 0;` |
|    27 |  417 | `	int iFlags = 0;` |
|     - |  418 | `	int rc;` |
|     - |  419 |  |
|    27 |  420 | `	if( nArg < 2 ){` |
|   ! 0 |  421 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  422 | `			"preg_match() expects at least 2 parameters");` |
|   ! 0 |  423 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  424 | `		return PH7_OK;` |
|     - |  425 | `	}` |
|    27 |  426 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    27 |  427 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    27 |  428 | `	if( nArg >= 4 ){` |
|   ! 0 |  429 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  430 | `	}` |
|    27 |  431 | `	if( nArg >= 5 ){` |
|   ! 0 |  432 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  433 | `	}` |
|    27 |  434 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    27 |  435 | `	if( pCode == 0 ){` |
|   ! 0 |  436 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  437 | `		return PH7_OK;` |
|     - |  438 | `	}` |
|    27 |  439 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    27 |  440 | `	if( pMatchData == 0 ){` |
|   ! 0 |  441 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  442 | `		return PH7_OK;` |
|     - |  443 | `	}` |
|    38 |  444 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    11 |  445 | `		startOffset, 0, pMatchData, NULL);` |
|    27 |  446 | `	if( rc < 0 ){` |
|     5 |  447 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  448 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  449 | `		}` |
|     - |  450 | `		/* Populate empty matches if requested */` |
|     5 |  451 | `		if( nArg >= 3 ){` |
|     5 |  452 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|     5 |  453 | `			PcreStoreByRef(pCtx->pVm, apArg[2], pEmpty);` |
|     5 |  454 | `			ph7_context_release_value(pCtx, pEmpty);` |
|     2 |  455 | `		}` |
|     5 |  456 | `		pcre2_match_data_free(pMatchData);` |
|     5 |  457 | `		ph7_result_int(pCtx, 0);` |
|     5 |  458 | `		return PH7_OK;` |
|     - |  459 | `	}` |
|    23 |  460 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    23 |  461 | `	if( nArg >= 3 ){` |
|     - |  462 | `		/* Populate $matches */` |
|    15 |  463 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    15 |  464 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    15 |  465 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);` |
|     - |  466 | `		/* Write the array back to the caller's variable */` |
|    15 |  467 | `		PcreStoreByRef(pCtx->pVm, apArg[2], pArray);` |
|    15 |  468 | `		ph7_context_release_value(pCtx, pArray);` |
|     5 |  469 | `	}` |
|    23 |  470 | `	pcre2_match_data_free(pMatchData);` |
|    23 |  471 | `	ph7_result_int(pCtx, 1);` |
|    23 |  472 | `	return PH7_OK;` |
|    16 |  473 |  |
|     - |  474 |  |
|     - |  475 | `/* ======================================================================` |
|     - |  476 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  477 | ` * ====================================================================== */` |
|     6 |  478 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  479 |  |
|     - |  480 | `	const char *zPattern, *zSubject;` |
|     - |  481 | `	int nPatLen, nSubLen;` |
|     - |  482 | `	pcre2_code *pCode;` |
|     - |  483 | `	pcre2_match_data *pMatchData;` |
|     - |  484 | `	sxu32 nCapture;` |
|     7 |  485 | `	PCRE2_SIZE startOffset = 0;` |
|     7 |  486 | `	int iFlags = PHP_PREG_PATTERN_ORDER;` |
|     7 |  487 | `	int totalMatches = 0;` |
|     - |  488 | `	int rc;` |
|     - |  489 |  |
|     7 |  490 | `	if( nArg < 2 ){` |
|   ! 0 |  491 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  492 | `			"preg_match_all() expects at least 2 parameters");` |
|   ! 0 |  493 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  494 | `		return PH7_OK;` |
|     - |  495 | `	}` |
|     7 |  496 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     7 |  497 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     7 |  498 | `	if( nArg >= 4 ){` |
|     3 |  499 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     1 |  500 | `	}` |
|     7 |  501 | `	if( nArg >= 5 ){` |
|   ! 0 |  502 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  503 | `	}` |
|     7 |  504 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     7 |  505 | `	if( pCode == 0 ){` |
|   ! 0 |  506 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  507 | `		return PH7_OK;` |
|     - |  508 | `	}` |
|     7 |  509 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     7 |  510 | `	if( pMatchData == 0 ){` |
|   ! 0 |  511 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  512 | `		return PH7_OK;` |
|     - |  513 | `	}` |
|     7 |  514 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  515 | `	{` |
|     7 |  516 | `		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;` |
|     - |  517 |  |
|     7 |  518 | `		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){` |
|     7 |  519 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  520 | `				PCRE2_SIZE *ovector;` |
|    10 |  521 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     3 |  522 | `					startOffset, 0, pMatchData, NULL);` |
|     7 |  523 | `				if( rc < 0 ){` |
|     3 |  524 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     3 |  525 | `					break;` |
|     - |  526 | `				}` |
|     5 |  527 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     5 |  528 | `				if( pOutArray ){` |
|     5 |  529 | `					ph7_value *pSet = ph7_context_new_array(pCtx);` |
|     5 |  530 | `					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, iFlags & ~0xFF);` |
|     5 |  531 | `					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);` |
|     5 |  532 | `					ph7_context_release_value(pCtx, pSet);` |
|     2 |  533 | `				}` |
|     5 |  534 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  535 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  536 | `				}else{` |
|     5 |  537 | `					startOffset = ovector[1];` |
|     - |  538 | `				}` |
|     5 |  539 | `				totalMatches++;` |
|     1 |  540 | `			}` |
|     2 |  541 | `		}else{` |
|     - |  542 | `			/* PREG_PATTERN_ORDER (default) */` |
|     5 |  543 | `			ph7_value **apGroupArrays = 0;` |
|     5 |  544 | `			sxu32 nGroups = nCapture + 1;` |
|     - |  545 | `			sxu32 g;` |
|     5 |  546 | `			if( pOutArray ){` |
|     7 |  547 | `				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,` |
|     2 |  548 | `					sizeof(ph7_value *) * nGroups, TRUE, FALSE);` |
|     5 |  549 | `				if( apGroupArrays ){` |
|    13 |  550 | `					for( g = 0; g < nGroups; g++ ){` |
|     9 |  551 | `						apGroupArrays[g] = ph7_context_new_array(pCtx);` |
|     5 |  552 | `					}` |
|     2 |  553 | `				}` |
|     2 |  554 | `			}` |
|    15 |  555 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  556 | `				PCRE2_SIZE *ovector;` |
|    22 |  557 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     7 |  558 | `					startOffset, 0, pMatchData, NULL);` |
|    15 |  559 | `				if( rc < 0 ){` |
|     5 |  560 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     5 |  561 | `					break;` |
|     - |  562 | `				}` |
|    11 |  563 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    11 |  564 | `				if( apGroupArrays ){` |
|    11 |  565 | `					ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    11 |  566 | `					int nActual = rc;` |
|    29 |  567 | `					for( g = 0; g < nGroups; g++ ){` |
|    28 |  568 | `						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){` |
|    19 |  569 | `							PCRE2_SIZE s = ovector[2*g];` |
|    19 |  570 | `							PCRE2_SIZE e = ovector[2*g+1];` |
|    19 |  571 | `							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  572 | `								ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  573 | `								ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  574 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|   ! 0 |  575 | `								ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  576 | `								ph7_value_int(pOff, (int)s);` |
|   ! 0 |  577 | `								ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  578 | `								ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|   ! 0 |  579 | `								ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  580 | `								ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  581 | `							}else{` |
|    19 |  582 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|    19 |  583 | `								ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  584 | `							}` |
|    10 |  585 | `						}else{` |
|   ! 0 |  586 | `							ph7_value_string(pVal, "", 0);` |
|   ! 0 |  587 | `							ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  588 | `						}` |
|    19 |  589 | `						ph7_value_reset_string_cursor(pVal);` |
|    10 |  590 | `					}` |
|    11 |  591 | `					ph7_context_release_value(pCtx, pVal);` |
|     5 |  592 | `				}` |
|    11 |  593 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  594 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  595 | `				}else{` |
|    11 |  596 | `					startOffset = ovector[1];` |
|     - |  597 | `				}` |
|    11 |  598 | `				totalMatches++;` |
|     1 |  599 | `			}` |
|     5 |  600 | `			if( apGroupArrays ){` |
|    13 |  601 | `				for( g = 0; g < nGroups; g++ ){` |
|     9 |  602 | `					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);` |
|     9 |  603 | `					ph7_context_release_value(pCtx, apGroupArrays[g]);` |
|     5 |  604 | `				}` |
|     5 |  605 | `				ph7_context_free_chunk(pCtx, apGroupArrays);` |
|     2 |  606 | `			}` |
|     - |  607 | `		}` |
|     - |  608 | `		/* Write output array to caller's variable */` |
|     7 |  609 | `		if( pOutArray && nArg >= 3 ){` |
|     7 |  610 | `			PcreStoreByRef(pCtx->pVm, apArg[2], pOutArray);` |
|     7 |  611 | `			ph7_context_release_value(pCtx, pOutArray);` |
|     3 |  612 | `		}` |
|     - |  613 | `	}` |
|     7 |  614 | `	pcre2_match_data_free(pMatchData);` |
|     7 |  615 | `	ph7_result_int(pCtx, totalMatches);` |
|     7 |  616 | `	return PH7_OK;` |
|     4 |  617 |  |
|     - |  618 |  |
|     - |  619 | `/* ======================================================================` |
|     - |  620 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|     - |  621 | ` * ====================================================================== */` |
|     4 |  622 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  623 |  |
|     - |  624 | `	const char *zPattern, *zSubject;` |
|     - |  625 | `	int nPatLen, nSubLen;` |
|     - |  626 | `	pcre2_code *pCode;` |
|     - |  627 | `	pcre2_match_data *pMatchData;` |
|     - |  628 | `	sxu32 nCapture;` |
|     - |  629 | `	ph7_value *pArray;` |
|     - |  630 | `	ph7_value *pVal;` |
|     5 |  631 | `	PCRE2_SIZE startOffset = 0, lastOffset = 0;` |
|     5 |  632 | `	int limit = -1;` |
|     5 |  633 | `	int iFlags = 0;` |
|     5 |  634 | `	int nPieces = 0;` |
|     - |  635 | `	int rc;` |
|     - |  636 |  |
|     5 |  637 | `	if( nArg < 2 ){` |
|   ! 0 |  638 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  639 | `			"preg_split() expects at least 2 parameters");` |
|   ! 0 |  640 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  641 | `		return PH7_OK;` |
|     - |  642 | `	}` |
|     5 |  643 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     5 |  644 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     5 |  645 | `	if( nArg >= 3 ){` |
|     3 |  646 | `		limit = ph7_value_to_int(apArg[2]);` |
|     1 |  647 | `	}` |
|     5 |  648 | `	if( nArg >= 4 ){` |
|   ! 0 |  649 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  650 | `	}` |
|     5 |  651 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     5 |  652 | `	if( pCode == 0 ){` |
|   ! 0 |  653 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  654 | `		return PH7_OK;` |
|     - |  655 | `	}` |
|     5 |  656 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     5 |  657 | `	if( pMatchData == 0 ){` |
|   ! 0 |  658 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  659 | `		return PH7_OK;` |
|     - |  660 | `	}` |
|     5 |  661 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  662 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     5 |  663 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  664 |  |
|    13 |  665 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|    13 |  666 | `		if( limit > 0 && nPieces >= limit - 1 ){` |
|     3 |  667 | `			break; /* Last piece gets the remainder */` |
|     - |  668 | `		}` |
|    16 |  669 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     5 |  670 | `			startOffset, 0, pMatchData, NULL);` |
|    11 |  671 | `		if( rc < 0 ){` |
|     3 |  672 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  673 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  674 | `			}` |
|     3 |  675 | `			break;` |
|     - |  676 | `		}` |
|     - |  677 | `		{` |
|     9 |  678 | `			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     9 |  679 | `			PCRE2_SIZE matchStart = ovector[0];` |
|     9 |  680 | `			PCRE2_SIZE matchEnd = ovector[1];` |
|     9 |  681 | `			int pieceLen = (int)(matchStart - lastOffset);` |
|     - |  682 |  |
|     - |  683 | `			/* Add the piece before the match */` |
|     9 |  684 | `			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| pieceLen > 0 ){` |
|     9 |  685 | `				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  686 | `					ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  687 | `					ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  688 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|   ! 0 |  689 | `					ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  690 | `					ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  691 | `					ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  692 | `					ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  693 | `					ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  694 | `					ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  695 | `				}else{` |
|     9 |  696 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|     9 |  697 | `					ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  698 | `				}` |
|     9 |  699 | `				ph7_value_reset_string_cursor(pVal);` |
|     9 |  700 | `				nPieces++;` |
|     4 |  701 | `			}` |
|     - |  702 | `			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */` |
|     9 |  703 | `			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){` |
|     - |  704 | `				int g;` |
|   ! 0 |  705 | `				for( g = 1; g < rc; g++ ){` |
|   ! 0 |  706 | `					PCRE2_SIZE gs = ovector[2*g];` |
|   ! 0 |  707 | `					PCRE2_SIZE ge = ovector[2*g+1];` |
|     - |  708 | `					int gLen;` |
|   ! 0 |  709 | `					if( gs == PCRE2_UNSET ) continue;` |
|   ! 0 |  710 | `					gLen = (int)(ge - gs);` |
|   ! 0 |  711 | `					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| gLen > 0 ){` |
|   ! 0 |  712 | `						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  713 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  714 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  715 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  716 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  717 | `							ph7_value_int(pOff, (int)gs);` |
|   ! 0 |  718 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  719 | `							ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  720 | `							ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  721 | `							ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  722 | `						}else{` |
|   ! 0 |  723 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  724 | `							ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  725 | `						}` |
|   ! 0 |  726 | `						ph7_value_reset_string_cursor(pVal);` |
|   ! 0 |  727 | `					}` |
|   ! 0 |  728 | `				}` |
|   ! 0 |  729 | `			}` |
|     - |  730 | `			/* Advance */` |
|     9 |  731 | `			lastOffset = matchEnd;` |
|     9 |  732 | `			if( matchEnd == matchStart ){` |
|   ! 0 |  733 | `				startOffset = matchEnd + 1;` |
|   ! 0 |  734 | `			}else{` |
|     9 |  735 | `				startOffset = matchEnd;` |
|     - |  736 | `			}` |
|     - |  737 | `		}` |
|     1 |  738 | `	}` |
|     - |  739 | `	/* Add trailing piece */` |
|     - |  740 | `	{` |
|     5 |  741 | `		int trailLen = nSubLen - (int)lastOffset;` |
|     5 |  742 | `		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| trailLen > 0 ){` |
|     5 |  743 | `			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  744 | `				ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  745 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  746 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|   ! 0 |  747 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  748 | `				ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  749 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  750 | `				ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  751 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  752 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  753 | `			}else{` |
|     5 |  754 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|     5 |  755 | `				ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  756 | `			}` |
|     2 |  757 | `		}` |
|     - |  758 | `	}` |
|     5 |  759 | `	ph7_context_release_value(pCtx, pVal);` |
|     5 |  760 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  761 | `	ph7_result_value(pCtx, pArray);` |
|     5 |  762 | `	ph7_context_release_value(pCtx, pArray);` |
|     5 |  763 | `	return PH7_OK;` |
|     3 |  764 |  |
|     - |  765 |  |
|     - |  766 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|    14 |  767 | `static void PcreExpandBackrefs(` |
|     - |  768 | `	SyBlob *pOut,` |
|     - |  769 | `	const char *zRepl, int nReplLen,` |
|     - |  770 | `	const char *zSubject,` |
|     - |  771 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     1 |  772 |  |
|    15 |  773 | `	const char *zEnd = &zRepl[nReplLen];` |
|    15 |  774 | `	const char *z = zRepl;` |
|     - |  775 |  |
|    33 |  776 | `	while( z < zEnd ){` |
|    19 |  777 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   ! 0 |  778 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|   ! 0 |  779 | `				int g = z[1] - '0';` |
|   ! 0 |  780 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  781 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  782 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  783 | `				}` |
|   ! 0 |  784 | `				z += 2;` |
|   ! 0 |  785 | `				continue;` |
|     - |  786 | `			}` |
|   ! 0 |  787 | `			if( z[1] == '\\' ){` |
|   ! 0 |  788 | `				SyBlobAppend(pOut, "\\", 1);` |
|   ! 0 |  789 | `				z += 2;` |
|   ! 0 |  790 | `				continue;` |
|     - |  791 | `			}` |
|     - |  792 | `			/* Not a backreference — emit literally */` |
|   ! 0 |  793 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  794 | `			z++;` |
|   ! 0 |  795 | `			continue;` |
|     - |  796 | `		}` |
|    19 |  797 | `		if( *z == '$' && z + 1 < zEnd ){` |
|     5 |  798 | `			if( z[1] == '$' ){` |
|   ! 0 |  799 | `				SyBlobAppend(pOut, "$", 1);` |
|   ! 0 |  800 | `				z += 2;` |
|   ! 0 |  801 | `				continue;` |
|     - |  802 | `			}` |
|     5 |  803 | `			if( z[1] == '{' ){` |
|     - |  804 | `				/* ${N} form */` |
|   ! 0 |  805 | `				const char *p = z + 2;` |
|   ! 0 |  806 | `				int g = 0;` |
|   ! 0 |  807 | `				while( p < zEnd && *p >= '0' && *p <= '9' ){` |
|   ! 0 |  808 | `					g = g * 10 + (*p - '0');` |
|   ! 0 |  809 | `					p++;` |
|   ! 0 |  810 | `				}` |
|   ! 0 |  811 | `				if( p < zEnd && *p == '}' ){` |
|   ! 0 |  812 | `					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  813 | `						SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  814 | `							(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  815 | `					}` |
|   ! 0 |  816 | `					z = p + 1;` |
|   ! 0 |  817 | `					continue;` |
|     - |  818 | `				}` |
|     - |  819 | `				/* Not a valid ${N} — emit literally */` |
|   ! 0 |  820 | `				SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  821 | `				z++;` |
|   ! 0 |  822 | `				continue;` |
|     - |  823 | `			}` |
|     5 |  824 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|     - |  825 | `				/* $N or $NN */` |
|     5 |  826 | `				int g = z[1] - '0';` |
|     5 |  827 | `				z += 2;` |
|     - |  828 | `				/* Check for second digit */` |
|     5 |  829 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  830 | `					int g2 = g * 10 + (*z - '0');` |
|   ! 0 |  831 | `					if( g2 < nGroups ){` |
|   ! 0 |  832 | `						g = g2;` |
|   ! 0 |  833 | `						z++;` |
|   ! 0 |  834 | `					}` |
|   ! 0 |  835 | `				}` |
|     5 |  836 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|     7 |  837 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|     4 |  838 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|     2 |  839 | `				}` |
|     5 |  840 | `				continue;` |
|     - |  841 | `			}` |
|     - |  842 | `			/* Not a backreference */` |
|   ! 0 |  843 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  844 | `			z++;` |
|   ! 0 |  845 | `			continue;` |
|     - |  846 | `		}` |
|    15 |  847 | `		SyBlobAppend(pOut, z, 1);` |
|    15 |  848 | `		z++;` |
|     1 |  849 | `	}` |
|    15 |  850 |  |
|     - |  851 |  |
|     - |  852 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|     8 |  853 | `static void PcreDoReplace(` |
|     - |  854 | `	ph7_context *pCtx,` |
|     - |  855 | `	pcre2_code *pCode,` |
|     - |  856 | `	const char *zSubject, int nSubLen,` |
|     - |  857 | `	const char *zRepl, int nReplLen,` |
|     - |  858 | `	int limit,` |
|     - |  859 | `	int *pCount,` |
|     - |  860 | `	SyBlob *pOut)` |
|     1 |  861 |  |
|     - |  862 | `	pcre2_match_data *pMatchData;` |
|     9 |  863 | `	PCRE2_SIZE startOffset = 0;` |
|     9 |  864 | `	int nReplacements = 0;` |
|     - |  865 | `	int rc;` |
|     - |  866 |  |
|     9 |  867 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     9 |  868 | `	if( pMatchData == 0 ) return;` |
|     - |  869 |  |
|    23 |  870 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  871 | `		PCRE2_SIZE *ovector;` |
|    23 |  872 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|    34 |  873 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    11 |  874 | `			startOffset, 0, pMatchData, NULL);` |
|    23 |  875 | `		if( rc < 0 ){` |
|     9 |  876 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  877 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  878 | `			}` |
|     9 |  879 | `			break;` |
|     - |  880 | `		}` |
|    15 |  881 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - |  882 | `		/* Copy text before match */` |
|    15 |  883 | `		if( ovector[0] > startOffset ){` |
|    13 |  884 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     6 |  885 | `		}` |
|     - |  886 | `		/* Expand replacement */` |
|    15 |  887 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|    15 |  888 | `		nReplacements++;` |
|     - |  889 | `		/* Advance */` |
|    15 |  890 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 |  891 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 |  892 | `				SyBlobAppend(pOut, &zSubject[startOffset], 1);` |
|   ! 0 |  893 | `			}` |
|   ! 0 |  894 | `			startOffset = ovector[0] + 1;` |
|   ! 0 |  895 | `		}else{` |
|    15 |  896 | `			startOffset = ovector[1];` |
|     - |  897 | `		}` |
|     1 |  898 | `	}` |
|     - |  899 | `	/* Copy remainder */` |
|     9 |  900 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 |  901 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   ! 0 |  902 | `	}` |
|     9 |  903 | `	if( pCount ){` |
|     9 |  904 | `		*pCount += nReplacements;` |
|     4 |  905 | `	}` |
|     9 |  906 | `	pcre2_match_data_free(pMatchData);` |
|     4 |  907 | `	SXUNUSED(pCtx);` |
|     5 |  908 |  |
|     - |  909 |  |
|     - |  910 | `/* ======================================================================` |
|     - |  911 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - |  912 | ` * ====================================================================== */` |
|     8 |  913 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  914 |  |
|     9 |  915 | `	int limit = -1;` |
|     9 |  916 | `	int count = 0;` |
|     - |  917 |  |
|     9 |  918 | `	if( nArg < 3 ){` |
|   ! 0 |  919 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  920 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 |  921 | `		ph7_result_null(pCtx);` |
|   ! 0 |  922 | `		return PH7_OK;` |
|     - |  923 | `	}` |
|     9 |  924 | `	if( nArg >= 4 ){` |
|     3 |  925 | `		limit = ph7_value_to_int(apArg[3]);` |
|     1 |  926 | `	}` |
|     9 |  927 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  928 |  |
|     - |  929 | `	/* Reject array subjects (not yet supported) */` |
|     9 |  930 | `	if( ph7_value_is_array(apArg[2]) ){` |
|   ! 0 |  931 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  932 | `			"preg_replace(): Array subjects are not yet supported");` |
|   ! 0 |  933 | `		ph7_result_null(pCtx);` |
|   ! 0 |  934 | `		return PH7_OK;` |
|     - |  935 | `	}` |
|     9 |  936 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     - |  937 | `		/* Single pattern + single replacement on a string subject */` |
|     - |  938 | `		const char *zPattern, *zRepl, *zSubject;` |
|     - |  939 | `		int nPatLen, nReplLen, nSubLen;` |
|     - |  940 | `		pcre2_code *pCode;` |
|     - |  941 | `		sxu32 nCapture;` |
|     - |  942 | `		SyBlob sOut;` |
|     - |  943 |  |
|     9 |  944 | `		zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     9 |  945 | `		zRepl = ph7_value_to_string(apArg[1], &nReplLen);` |
|     9 |  946 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|     - |  947 |  |
|     9 |  948 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     9 |  949 | `		if( pCode == 0 ){` |
|   ! 0 |  950 | `			ph7_result_null(pCtx);` |
|   ! 0 |  951 | `			return PH7_OK;` |
|     - |  952 | `		}` |
|     9 |  953 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     9 |  954 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, &count, &sOut);` |
|     9 |  955 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     9 |  956 | `		SyBlobRelease(&sOut);` |
|     5 |  957 | `	}else{` |
|     - |  958 | `		/* TODO: array of patterns — iterate pairs and apply sequentially */` |
|   ! 0 |  959 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  960 | `			"preg_replace() with array patterns is not yet supported");` |
|   ! 0 |  961 | `		ph7_result_null(pCtx);` |
|   ! 0 |  962 | `		return PH7_OK;` |
|     - |  963 | `	}` |
|     - |  964 | `	/* Set &$count if provided */` |
|     9 |  965 | `	if( nArg >= 5 ){` |
|     - |  966 | `		ph7_value sCount;` |
|     3 |  967 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     3 |  968 | `		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);` |
|     3 |  969 | `		PH7_MemObjRelease(&sCount);` |
|     1 |  970 | `	}` |
|     9 |  971 | `	return PH7_OK;` |
|     5 |  972 |  |
|     - |  973 |  |
|     - |  974 | `/* ======================================================================` |
|     - |  975 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count]])` |
|     - |  976 | ` * ====================================================================== */` |
|     6 |  977 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |  978 |  |
|     - |  979 | `	const char *zPattern, *zSubject;` |
|     - |  980 | `	int nPatLen, nSubLen;` |
|     - |  981 | `	pcre2_code *pCode;` |
|     - |  982 | `	pcre2_match_data *pMatchData;` |
|     - |  983 | `	sxu32 nCapture;` |
|     - |  984 | `	SyBlob sOut;` |
|     8 |  985 | `	PCRE2_SIZE startOffset = 0;` |
|     8 |  986 | `	int limit = -1;` |
|     8 |  987 | `	int count = 0;` |
|     - |  988 | `	int rc;` |
|     - |  989 |  |
|     8 |  990 | `	if( nArg < 3 ){` |
|   ! 0 |  991 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  992 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 |  993 | `		ph7_result_null(pCtx);` |
|   ! 0 |  994 | `		return PH7_OK;` |
|     - |  995 | `	}` |
|     8 |  996 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     8 |  997 | `	zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|     8 |  998 | `	if( nArg >= 4 ){` |
|   ! 0 |  999 | `		limit = ph7_value_to_int(apArg[3]);` |
|   ! 0 | 1000 | `	}` |
|     8 | 1001 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 | 1002 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1003 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 | 1004 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1005 | `		return PH7_OK;` |
|     - | 1006 | `	}` |
|     8 | 1007 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     8 | 1008 | `	if( pCode == 0 ){` |
|   ! 0 | 1009 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1010 | `		return PH7_OK;` |
|     - | 1011 | `	}` |
|     8 | 1012 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     8 | 1013 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1014 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1015 | `		return PH7_OK;` |
|     - | 1016 | `	}` |
|     8 | 1017 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     8 | 1018 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1019 |  |
|    22 | 1020 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1021 | `		PCRE2_SIZE *ovector;` |
|     - | 1022 | `		ph7_value *pMatchArr;` |
|     - | 1023 | `		ph7_value *apCbArg[1];` |
|     - | 1024 | `		ph7_value sResult;` |
|     - | 1025 | `		const char *zReplacement;` |
|     - | 1026 | `		int nReplLen;` |
|     - | 1027 |  |
|    25 | 1028 | `		if( limit >= 0 && count >= limit ) break;` |
|    32 | 1029 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    10 | 1030 | `			startOffset, 0, pMatchData, NULL);` |
|    22 | 1031 | `		if( rc < 0 ){` |
|     8 | 1032 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1033 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1034 | `			}` |
|     8 | 1035 | `			break;` |
|     - | 1036 | `		}` |
|    16 | 1037 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1038 | `		/* Copy text before match */` |
|    16 | 1039 | `		if( ovector[0] > startOffset ){` |
|    12 | 1040 | `			SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     5 | 1041 | `		}` |
|     - | 1042 | `		/* Build matches array for callback */` |
|    16 | 1043 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    16 | 1044 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, 0);` |
|     - | 1045 | `		/* Call the callback */` |
|    16 | 1046 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    16 | 1047 | `		apCbArg[0] = pMatchArr;` |
|    16 | 1048 | `		if( PH7_VmCallUserFunction(pCtx->pVm, apArg[1], 1, apCbArg, &sResult) == PH7_EXCEPTION ){` |
|     - | 1049 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|   ! 0 | 1050 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1051 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|   ! 0 | 1052 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1053 | `			pcre2_match_data_free(pMatchData);` |
|   ! 0 | 1054 | `			return PH7_EXCEPTION;` |
|     - | 1055 | `		}` |
|     - | 1056 | `		/* Get replacement string from callback result */` |
|    16 | 1057 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    16 | 1058 | `		SyBlobAppend(&sOut, zReplacement, (sxu32)nReplLen);` |
|    16 | 1059 | `		PH7_MemObjRelease(&sResult);` |
|    16 | 1060 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    16 | 1061 | `		count++;` |
|     - | 1062 | `		/* Advance */` |
|    16 | 1063 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 | 1064 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1065 | `				SyBlobAppend(&sOut, &zSubject[startOffset], 1);` |
|   ! 0 | 1066 | `			}` |
|   ! 0 | 1067 | `			startOffset = ovector[0] + 1;` |
|   ! 0 | 1068 | `		}else{` |
|    16 | 1069 | `			startOffset = ovector[1];` |
|     - | 1070 | `		}` |
|     2 | 1071 | `	}` |
|     - | 1072 | `	/* Copy remainder */` |
|     8 | 1073 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1074 | `		SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   ! 0 | 1075 | `	}` |
|     8 | 1076 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     8 | 1077 | `	SyBlobRelease(&sOut);` |
|     8 | 1078 | `	pcre2_match_data_free(pMatchData);` |
|     - | 1079 | `	/* Set &$count if provided */` |
|     8 | 1080 | `	if( nArg >= 5 ){` |
|     - | 1081 | `		ph7_value sCount;` |
|   ! 0 | 1082 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|   ! 0 | 1083 | `		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);` |
|   ! 0 | 1084 | `		PH7_MemObjRelease(&sCount);` |
|   ! 0 | 1085 | `	}` |
|     8 | 1086 | `	return PH7_OK;` |
|     5 | 1087 |  |
|     - | 1088 |  |
|     - | 1089 | `/* ======================================================================` |
|     - | 1090 | ` * preg_quote(str [, delimiter])` |
|     - | 1091 | ` * ====================================================================== */` |
|     6 | 1092 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1093 |  |
|     7 | 1094 | `	const char *zStr, *zDelim = 0;` |
|     7 | 1095 | `	int nLen, nDelimLen = 0;` |
|     - | 1096 | `	const char *z, *zEnd;` |
|     - | 1097 |  |
|     7 | 1098 | `	if( nArg < 1 ){` |
|   ! 0 | 1099 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1100 | `		return PH7_OK;` |
|     - | 1101 | `	}` |
|     7 | 1102 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|     7 | 1103 | `	if( nArg >= 2 ){` |
|     3 | 1104 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     1 | 1105 | `	}` |
|     7 | 1106 | `	z = zStr;` |
|     7 | 1107 | `	zEnd = &zStr[nLen];` |
|    71 | 1108 | `	while( z < zEnd ){` |
|    65 | 1109 | `		char c = *z;` |
|    65 | 1110 | `		switch( c ){` |
|     4 | 1111 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1112 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1113 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1114 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1115 | `			case '#':` |
|     9 | 1116 | `				ph7_result_string(pCtx, "\\", 1);` |
|     9 | 1117 | `				break;` |
|    28 | 1118 | `			default:` |
|    57 | 1119 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     3 | 1120 | `					ph7_result_string(pCtx, "\\", 1);` |
|     1 | 1121 | `				}` |
|    56 | 1122 | `				break;` |
|     - | 1123 | `		}` |
|    65 | 1124 | `		ph7_result_string(pCtx, z, 1);` |
|    65 | 1125 | `		z++;` |
|     1 | 1126 | `	}` |
|     7 | 1127 | `	return PH7_OK;` |
|     4 | 1128 |  |
|     - | 1129 |  |
|     - | 1130 | `/* ======================================================================` |
|     - | 1131 | ` * preg_last_error()` |
|     - | 1132 | ` * ====================================================================== */` |
|   ! 0 | 1133 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1134 |  |
|   ! 0 | 1135 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1136 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1137 | `	return PH7_OK;` |
|   ! 0 | 1138 |  |
|     - | 1139 |  |
|     - | 1140 | `/* ======================================================================` |
|     - | 1141 | ` * preg_last_error_msg()` |
|     - | 1142 | ` * ====================================================================== */` |
|   ! 0 | 1143 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1144 |  |
|     - | 1145 | `	const char *zMsg;` |
|   ! 0 | 1146 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1147 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1148 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1149 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1150 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1151 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1152 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1153 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1154 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1155 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1156 | `	}` |
|   ! 0 | 1157 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1158 | `	return PH7_OK;` |
|   ! 0 | 1159 |  |
|     - | 1160 |  |
|     - | 1161 | `/* ===== Function registration table ===== */` |
|     - | 1162 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1163 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1164 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1165 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1166 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1167 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1168 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1169 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1170 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1171 | `};` |
|     - | 1172 |  |
|  2956 | 1173 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     5 | 1174 |  |
|     - | 1175 | `	sxu32 n;` |
| 26609 | 1176 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 23653 | 1177 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 11829 | 1178 | `	}` |
|  2961 | 1179 |  |
|     - | 1180 |  |
|     - | 1181 | `/* ===== Constant registration ===== */` |
|     - | 1182 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1183 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1184 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1185 | `	}` |
|     - | 1186 |  |
|   ! 0 | 1187 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|     3 | 1188 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|   ! 0 | 1189 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|   ! 0 | 1190 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|   ! 0 | 1191 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|   ! 0 | 1192 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|   ! 0 | 1193 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|   ! 0 | 1194 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|   ! 0 | 1195 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|   ! 0 | 1196 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|   ! 0 | 1197 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|   ! 0 | 1198 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|   ! 0 | 1199 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|   ! 0 | 1200 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1201 |  |
|  2956 | 1202 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     5 | 1203 |  |
|  2961 | 1204 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  2961 | 1205 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  2961 | 1206 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  2961 | 1207 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  2961 | 1208 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  2961 | 1209 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  2961 | 1210 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  2961 | 1211 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  2961 | 1212 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  2961 | 1213 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  2961 | 1214 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  2961 | 1215 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  2961 | 1216 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  2961 | 1217 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  2961 | 1218 |  |
|     - | 1219 |  |
|     - | 1220 | `#else` |
|     - | 1221 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1222 | `typedef int vm_pcre_unused;` |
|     - | 1223 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1224 |  |
