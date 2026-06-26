# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 525/838 lines (62.65%)

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
|    72 |   55 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|     5 |   56 |  |
|     - |   57 | `	sxu32 i;` |
|   703 |   58 | `	for( i = 0; i < nCacheUsed; i++ ){` |
|   658 |   59 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|    31 |   60 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|    31 |   61 | `			if( pCaptureCount ){` |
|    31 |   62 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|    15 |   63 | `			}` |
|    31 |   64 | `			return aCache[i].pCode;` |
|     - |   65 | `		}` |
|   315 |   66 | `	}` |
|    47 |   67 | `	return 0;` |
|    41 |   68 |  |
|     - |   69 |  |
|    42 |   70 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|     5 |   71 |  |
|     - |   72 | `	PcreCacheEntry *pEntry;` |
|     - |   73 | `	char *zCopy;` |
|     - |   74 | `	/* Allocate the pattern copy first, before touching the cache */` |
|    47 |   75 | `	zCopy = (char *)malloc(nLen + 1);` |
|    47 |   76 | `	if( zCopy == 0 ){` |
|     - |   77 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|   ! 0 |   78 | `		return;` |
|     - |   79 | `	}` |
|    47 |   80 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|    47 |   81 | `	zCopy[nLen] = 0;` |
|    47 |   82 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|    41 |   83 | `		pEntry = &aCache[nCacheUsed++];` |
|    23 |   84 | `	}else{` |
|     - |   85 | `		/* Evict LRU */` |
|     7 |   86 | `		sxu32 iMin = aCache[0].iLastUsed;` |
|     7 |   87 | `		sxu32 iMinIdx = 0;` |
|     - |   88 | `		sxu32 i;` |
|    97 |   89 | `		for( i = 1; i < PCRE_CACHE_SIZE; i++ ){` |
|    91 |   90 | `			if( aCache[i].iLastUsed < iMin ){` |
|     5 |   91 | `				iMin = aCache[i].iLastUsed;` |
|     5 |   92 | `				iMinIdx = i;` |
|     2 |   93 | `			}` |
|    46 |   94 | `		}` |
|     7 |   95 | `		pEntry = &aCache[iMinIdx];` |
|     7 |   96 | `		pcre2_code_free(pEntry->pCode);` |
|     7 |   97 | `		free(pEntry->zPattern);` |
|     - |   98 | `	}` |
|    47 |   99 | `	pEntry->zPattern = zCopy;` |
|    47 |  100 | `	pEntry->nLen = nLen;` |
|    47 |  101 | `	pEntry->pCode = pCode;` |
|    47 |  102 | `	pEntry->nCaptureCount = nCaptureCount;` |
|    47 |  103 | `	pEntry->iLastUsed = ++iCacheClock;` |
|    26 |  104 |  |
|     - |  105 |  |
|     - |  106 | `/* ===== Delimiter parser ===== */` |
|     - |  107 | `#define PCRE_PARSE_OK             0` |
|     - |  108 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|     - |  109 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */` |
|     - |  110 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|     - |  111 |  |
|    42 |  112 | `static sxi32 PcreParsePattern(` |
|     - |  113 | `	const char *zInput, int nInputLen,` |
|     - |  114 | `	const char **pPattern, int *pnPatternLen,` |
|     - |  115 | `	const char **pFlags, int *pnFlagLen)` |
|     5 |  116 |  |
|    47 |  117 | `	const char *zEnd = &zInput[nInputLen];` |
|    47 |  118 | `	const char *z = zInput;` |
|     - |  119 | `	char cOpen, cClose;` |
|     - |  120 | `	const char *pStart;` |
|     - |  121 |  |
|     - |  122 | `	/* Skip leading whitespace */` |
|    47 |  123 | `	while( z < zEnd && (unsigned char)*z <= 0x20 ){` |
|   ! 0 |  124 | `		z++;` |
|   ! 0 |  125 | `	}` |
|    47 |  126 | `	if( z >= zEnd ){` |
|   ! 0 |  127 | `		return PCRE_PARSE_EMPTY;` |
|     - |  128 | `	}` |
|    47 |  129 | `	cOpen = *z;` |
|     - |  130 | `	/* Must not be alphanumeric, backslash, or whitespace */` |
|    47 |  131 | `	if( SyisAlphaNum(cOpen) \|\| cOpen == '\\' \|\| (unsigned char)cOpen <= 0x20 ){` |
|   ! 0 |  132 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|     - |  133 | `	}` |
|     - |  134 | `	/* Paired delimiters */` |
|    47 |  135 | `	switch( cOpen ){` |
|   ! 0 |  136 | `		case '(': cClose = ')'; break;` |
|   ! 0 |  137 | `		case '[': cClose = ']'; break;` |
|   ! 0 |  138 | `		case '{': cClose = '}'; break;` |
|   ! 0 |  139 | `		case '<': cClose = '>'; break;` |
|    47 |  140 | `		default:  cClose = cOpen; break;` |
|     - |  141 | `	}` |
|    47 |  142 | `	z++; /* Skip opening delimiter */` |
|    47 |  143 | `	pStart = z;` |
|     - |  144 | `	/* Scan for closing delimiter, respecting backslash escapes */` |
|   391 |  145 | `	while( z < zEnd ){` |
|   391 |  146 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|    47 |  147 | `			z += 2; /* Skip escaped char */` |
|    47 |  148 | `			continue;` |
|     - |  149 | `		}` |
|   345 |  150 | `		if( *z == cClose ){` |
|    47 |  151 | `			break;` |
|     - |  152 | `		}` |
|   303 |  153 | `		z++;` |
|     5 |  154 | `	}` |
|    47 |  155 | `	if( z >= zEnd ){` |
|   ! 0 |  156 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|     - |  157 | `	}` |
|    47 |  158 | `	*pPattern = pStart;` |
|    47 |  159 | `	*pnPatternLen = (int)(z - pStart);` |
|    47 |  160 | `	z++; /* Skip closing delimiter */` |
|    47 |  161 | `	*pFlags = z;` |
|    47 |  162 | `	*pnFlagLen = (int)(zEnd - z);` |
|    47 |  163 | `	return PH7_OK;` |
|    26 |  164 |  |
|     - |  165 |  |
|     - |  166 | `/* ===== Flag mapper ===== */` |
|    42 |  167 | `static sxi32 PcreMapFlags(` |
|     - |  168 | `	const char *zFlags, int nFlagLen,` |
|     - |  169 | `	uint32_t *pCompileOpts)` |
|     5 |  170 |  |
|     - |  171 | `	int i;` |
|    47 |  172 | `	*pCompileOpts = 0;` |
|    59 |  173 | `	for( i = 0; i < nFlagLen; i++ ){` |
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
|    47 |  188 | `	return PH7_OK;` |
|     5 |  189 |  |
|     - |  190 |  |
|     - |  191 | `/* ===== Compile helper ===== */` |
|    72 |  192 | `static pcre2_code *PcreCompile(` |
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
|    77 |  207 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|    77 |  208 | `	if( pCode ){` |
|    31 |  209 | `		return pCode;` |
|     - |  210 | `	}` |
|     - |  211 | `	/* Parse delimiter */` |
|    47 |  212 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen);` |
|    47 |  213 | `	if( parseRc != PCRE_PARSE_OK ){` |
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
|    47 |  225 | `	PcreMapFlags(zFlags, nFlagLen, &compileOpts);` |
|     - |  226 | `	/* Compile */` |
|    47 |  227 | `	pCode = pcre2_compile(` |
|    21 |  228 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|    21 |  229 | `		compileOpts, &errcode, &erroffset, NULL);` |
|    47 |  230 | `	if( pCode == 0 ){` |
|     - |  231 | `		PCRE2_UCHAR errbuf[256];` |
|   ! 0 |  232 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|   ! 0 |  233 | `		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,` |
|   ! 0 |  234 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|   ! 0 |  235 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|   ! 0 |  236 | `		return 0;` |
|     - |  237 | `	}` |
|     - |  238 | `	/* Get capture count */` |
|    47 |  239 | `	nCapture = 0;` |
|    47 |  240 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|    47 |  241 | `	if( pCaptureCount ){` |
|    47 |  242 | `		*pCaptureCount = nCapture;` |
|    21 |  243 | `	}` |
|     - |  244 | `	/* Cache it */` |
|    47 |  245 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|    47 |  246 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    47 |  247 | `	return pCode;` |
|    41 |  248 |  |
|     - |  249 |  |
|     - |  250 | `/*` |
|     - |  251 | ` * Write a value back to the caller's variable through the stack slot's nIdx.` |
|     - |  252 | ` *` |
|     - |  253 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|     - |  254 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|     - |  255 | ` * bare undefined variable (e.g. preg_match($p,$s,$m) with $m never assigned),` |
|     - |  256 | ` * an array subscript (preg_match($p,$s,$a['k'])), and a declared/untyped` |
|     - |  257 | ` * property (preg_match($p,$s,$o->prop)) all arrive with a real nIdx and are` |
|     - |  258 | ` * written back here, matching PHP's reference semantics.` |
|     - |  259 | ` *` |
|     - |  260 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the value` |
|     - |  261 | ` * still lands in the local stack slot) when the argument cannot expose a stable` |
|     - |  262 | ` * memobj slot: a literal, a function-call result, a subscript of a non-lvalue` |
|     - |  263 | ` * parent (foo()['k']), or any variable in a call that also uses named or spread` |
|     - |  264 | ` * arguments (compile-time positions no longer map to the runtime arg slots, so` |
|     - |  265 | ` * the compiler conservatively does not vivify). An uninitialized typed property` |
|     - |  266 | ` * is also not wired (it throws before the write) -- see PLAN.md deferrals.` |
|     - |  267 | ` */` |
|    44 |  268 | `static void PcreStoreByRef(ph7_vm *pVm, ph7_value *pArg, ph7_value *pNewVal)` |
|     5 |  269 |  |
|    49 |  270 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|    47 |  271 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj, pArg->nIdx);` |
|    47 |  272 | `		if( pObj ){` |
|    47 |  273 | `			PH7_MemObjStore(pNewVal, pObj);` |
|    21 |  274 | `		}` |
|    21 |  275 | `	}` |
|    49 |  276 | `	PH7_MemObjStore(pNewVal, pArg);` |
|    49 |  277 |  |
|     - |  278 |  |
|     - |  279 | `/* ===== Map PCRE2 match error to PHP error code ===== */` |
|   ! 0 |  280 | `static void PcreSetMatchError(ph7_vm *pVm, int rc)` |
|   ! 0 |  281 |  |
|   ! 0 |  282 | `	if( rc == PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  283 | `		pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   ! 0 |  284 | `	}else if( rc == PCRE2_ERROR_MATCHLIMIT ){` |
|   ! 0 |  285 | `		pVm->iPcreLastError = PHP_PREG_BACKTRACK_LIMIT_ERROR;` |
|   ! 0 |  286 | `	}else if( rc == PCRE2_ERROR_DEPTHLIMIT` |
|     - |  287 | `#ifdef PCRE2_ERROR_RECURSIONLIMIT` |
|   ! 0 |  288 | `		\|\| rc == PCRE2_ERROR_RECURSIONLIMIT` |
|     - |  289 | `#endif` |
|     - |  290 | `	){` |
|   ! 0 |  291 | `		pVm->iPcreLastError = PHP_PREG_RECURSION_LIMIT_ERROR;` |
|   ! 0 |  292 | `	}else if( rc == PCRE2_ERROR_BADUTFOFFSET ){` |
|   ! 0 |  293 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_OFFSET_ERROR;` |
|   ! 0 |  294 | `	}else if( rc == PCRE2_ERROR_UTF8_ERR1` |
|   ! 0 |  295 | `		\|\| rc == PCRE2_ERROR_UTF8_ERR2 ){` |
|   ! 0 |  296 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_ERROR;` |
|     - |  297 | `#ifdef PCRE2_ERROR_JIT_STACKLIMIT` |
|   ! 0 |  298 | `	}else if( rc == PCRE2_ERROR_JIT_STACKLIMIT ){` |
|   ! 0 |  299 | `		pVm->iPcreLastError = PHP_PREG_JIT_STACKLIMIT_ERROR;` |
|     - |  300 | `#endif` |
|   ! 0 |  301 | `	}else{` |
|   ! 0 |  302 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|     - |  303 | `	}` |
|   ! 0 |  304 |  |
|     - |  305 |  |
|     - |  306 | `/* ===== Helper: populate matches array from ovector ===== */` |
|    48 |  307 | `static void PcrePopulateMatches(` |
|     - |  308 | `	ph7_context *pCtx,` |
|     - |  309 | `	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */` |
|     - |  310 | `	const char *zSubject,` |
|     - |  311 | `	PCRE2_SIZE *ovector,` |
|     - |  312 | `	int nGroups,` |
|     - |  313 | `	pcre2_code *pCode,` |
|     - |  314 | `	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */` |
|     5 |  315 |  |
|    53 |  316 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    53 |  317 | `	ph7_value *pSub = 0;` |
|    53 |  318 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|    53 |  319 | `	PCRE2_SPTR nametable = 0;` |
|     - |  320 | `	int i;` |
|     - |  321 |  |
|    53 |  322 | `	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  323 | `		pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  324 | `	}` |
|   149 |  325 | `	for( i = 0; i < nGroups; i++ ){` |
|   101 |  326 | `		PCRE2_SIZE start = ovector[2 * i];` |
|   101 |  327 | `		PCRE2_SIZE end   = ovector[2 * i + 1];` |
|   101 |  328 | `		if( start == PCRE2_UNSET ){` |
|   ! 0 |  329 | `			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  330 | `				ph7_value_null(pVal);` |
|   ! 0 |  331 | `			}else{` |
|   ! 0 |  332 | `				ph7_value_string(pVal, "", 0);` |
|     - |  333 | `			}` |
|   ! 0 |  334 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  335 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  336 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  337 | `				ph7_value_int(pOff, -1);` |
|   ! 0 |  338 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  339 | `				ph7_array_add_intkey_elem(pArray, i, pSub);` |
|     - |  340 | `				/* Reset sub-array for reuse */` |
|   ! 0 |  341 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  342 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  343 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  344 | `			}else{` |
|   ! 0 |  345 | `				ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  346 | `			}` |
|   ! 0 |  347 | `		}else{` |
|   101 |  348 | `			ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|   101 |  349 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  350 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  351 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  352 | `				ph7_value_int(pOff, (int)start);` |
|   ! 0 |  353 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  354 | `				ph7_array_add_intkey_elem(pArray, i, pSub);` |
|   ! 0 |  355 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  356 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  357 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  358 | `			}else{` |
|   101 |  359 | `				ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  360 | `			}` |
|     - |  361 | `		}` |
|   101 |  362 | `		ph7_value_reset_string_cursor(pVal);` |
|    53 |  363 | `	}` |
|     - |  364 | `	/* Named groups */` |
|    53 |  365 | `	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|    53 |  366 | `	if( namecount > 0 ){` |
|     5 |  367 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     5 |  368 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|    13 |  369 | `		for( i = 0; (uint32_t)i < namecount; i++ ){` |
|     9 |  370 | `			PCRE2_SPTR entry = nametable + i * nameentrysize;` |
|     9 |  371 | `			int groupNum = (entry[0] << 8) \| entry[1];` |
|     9 |  372 | `			const char *zName = (const char *)(entry + 2);` |
|     - |  373 | `			PCRE2_SIZE start, end;` |
|     9 |  374 | `			if( groupNum >= nGroups ) continue;` |
|     9 |  375 | `			start = ovector[2 * groupNum];` |
|     9 |  376 | `			end   = ovector[2 * groupNum + 1];` |
|     9 |  377 | `			if( start == PCRE2_UNSET ){` |
|   ! 0 |  378 | `				if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  379 | `					ph7_value_null(pVal);` |
|   ! 0 |  380 | `				}else{` |
|   ! 0 |  381 | `					ph7_value_string(pVal, "", 0);` |
|     - |  382 | `				}` |
|   ! 0 |  383 | `			}else{` |
|     9 |  384 | `				ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|     - |  385 | `			}` |
|     9 |  386 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  387 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  388 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  389 | `				ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);` |
|   ! 0 |  390 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  391 | `				ph7_array_add_strkey_elem(pArray, zName, pSub);` |
|   ! 0 |  392 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  393 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  394 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  395 | `			}else{` |
|     9 |  396 | `				ph7_array_add_strkey_elem(pArray, zName, pVal);` |
|     - |  397 | `			}` |
|     9 |  398 | `			ph7_value_reset_string_cursor(pVal);` |
|     5 |  399 | `		}` |
|     2 |  400 | `	}` |
|    53 |  401 | `	ph7_context_release_value(pCtx, pVal);` |
|    53 |  402 | `	if( pSub ){` |
|   ! 0 |  403 | `		ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  404 | `	}` |
|    53 |  405 |  |
|     - |  406 |  |
|     - |  407 | `/*` |
|     - |  408 | ` * Quiet whole-pattern match used by FILTER_VALIDATE_REGEXP: compile zPat (a full` |
|     - |  409 | ` * "/.../flags" pattern) and test it against zSub. On a successful attempt returns` |
|     - |  410 | ` * SXRET_OK with *pMatched set to 1 (match) or 0 (no match); returns SXERR_INVALID` |
|     - |  411 | ` * on a compile/match error (the caller treats that as a validation failure). The` |
|     - |  412 | ` * compiled code is owned by PcreCompile's cache, so it is not freed here.` |
|     - |  413 | ` */` |
|     4 |  414 | `PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,` |
|     - |  415 | `	const char *zSub,int nSub,int *pMatched)` |
|     1 |  416 |  |
|     - |  417 | `	pcre2_code *pCode;` |
|     - |  418 | `	pcre2_match_data *pMatchData;` |
|     - |  419 | `	sxu32 nCapture;` |
|     - |  420 | `	int rc;` |
|     5 |  421 | `	*pMatched = 0;` |
|     5 |  422 | `	pCode = PcreCompile(pCtx,zPat,nPat,&nCapture);` |
|     5 |  423 | `	if( pCode == 0 ){` |
|   ! 0 |  424 | `		return SXERR_INVALID;` |
|     - |  425 | `	}` |
|     5 |  426 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode,NULL);` |
|     5 |  427 | `	if( pMatchData == 0 ){` |
|   ! 0 |  428 | `		return SXERR_INVALID;` |
|     - |  429 | `	}` |
|     5 |  430 | `	rc = pcre2_match(pCode,(PCRE2_SPTR)zSub,(PCRE2_SIZE)nSub,0,0,pMatchData,NULL);` |
|     5 |  431 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  432 | `	if( rc < 0 ){` |
|     3 |  433 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  434 | `			PcreSetMatchError(pCtx->pVm,rc);` |
|   ! 0 |  435 | `			return SXERR_INVALID;` |
|     - |  436 | `		}` |
|     3 |  437 | `		return SXRET_OK; /* clean no-match */` |
|     - |  438 | `	}` |
|     3 |  439 | `	*pMatched = 1;` |
|     3 |  440 | `	return SXRET_OK;` |
|     3 |  441 |  |
|     - |  442 | `/* ======================================================================` |
|     - |  443 | ` * preg_match(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  444 | ` * ====================================================================== */` |
|    38 |  445 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  446 |  |
|     - |  447 | `	const char *zPattern, *zSubject;` |
|     - |  448 | `	int nPatLen, nSubLen;` |
|     - |  449 | `	pcre2_code *pCode;` |
|     - |  450 | `	pcre2_match_data *pMatchData;` |
|     - |  451 | `	PCRE2_SIZE *ovector;` |
|     - |  452 | `	sxu32 nCapture;` |
|    43 |  453 | `	PCRE2_SIZE startOffset = 0;` |
|    43 |  454 | `	int iFlags = 0;` |
|     - |  455 | `	int rc;` |
|     - |  456 |  |
|    43 |  457 | `	if( nArg < 2 ){` |
|   ! 0 |  458 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  459 | `			"preg_match() expects at least 2 parameters");` |
|   ! 0 |  460 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  461 | `		return PH7_OK;` |
|     - |  462 | `	}` |
|    43 |  463 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    43 |  464 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    43 |  465 | `	if( nArg >= 4 ){` |
|   ! 0 |  466 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  467 | `	}` |
|    43 |  468 | `	if( nArg >= 5 ){` |
|   ! 0 |  469 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  470 | `	}` |
|    43 |  471 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    43 |  472 | `	if( pCode == 0 ){` |
|   ! 0 |  473 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  474 | `		return PH7_OK;` |
|     - |  475 | `	}` |
|    43 |  476 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    43 |  477 | `	if( pMatchData == 0 ){` |
|   ! 0 |  478 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  479 | `		return PH7_OK;` |
|     - |  480 | `	}` |
|    62 |  481 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    19 |  482 | `		startOffset, 0, pMatchData, NULL);` |
|    43 |  483 | `	if( rc < 0 ){` |
|     5 |  484 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  485 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  486 | `		}` |
|     - |  487 | `		/* Populate empty matches if requested */` |
|     5 |  488 | `		if( nArg >= 3 ){` |
|     5 |  489 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|     5 |  490 | `			PcreStoreByRef(pCtx->pVm, apArg[2], pEmpty);` |
|     5 |  491 | `			ph7_context_release_value(pCtx, pEmpty);` |
|     2 |  492 | `		}` |
|     5 |  493 | `		pcre2_match_data_free(pMatchData);` |
|     5 |  494 | `		ph7_result_int(pCtx, 0);` |
|     5 |  495 | `		return PH7_OK;` |
|     - |  496 | `	}` |
|    39 |  497 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    39 |  498 | `	if( nArg >= 3 ){` |
|     - |  499 | `		/* Populate $matches */` |
|    31 |  500 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    31 |  501 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    31 |  502 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);` |
|     - |  503 | `		/* Write the array back to the caller's variable */` |
|    31 |  504 | `		PcreStoreByRef(pCtx->pVm, apArg[2], pArray);` |
|    31 |  505 | `		ph7_context_release_value(pCtx, pArray);` |
|    13 |  506 | `	}` |
|    39 |  507 | `	pcre2_match_data_free(pMatchData);` |
|    39 |  508 | `	ph7_result_int(pCtx, 1);` |
|    39 |  509 | `	return PH7_OK;` |
|    24 |  510 |  |
|     - |  511 |  |
|     - |  512 | `/* ======================================================================` |
|     - |  513 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  514 | ` * ====================================================================== */` |
|     8 |  515 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  516 |  |
|     - |  517 | `	const char *zPattern, *zSubject;` |
|     - |  518 | `	int nPatLen, nSubLen;` |
|     - |  519 | `	pcre2_code *pCode;` |
|     - |  520 | `	pcre2_match_data *pMatchData;` |
|     - |  521 | `	sxu32 nCapture;` |
|     9 |  522 | `	PCRE2_SIZE startOffset = 0;` |
|     9 |  523 | `	int iFlags = PHP_PREG_PATTERN_ORDER;` |
|     9 |  524 | `	int totalMatches = 0;` |
|     - |  525 | `	int rc;` |
|     - |  526 |  |
|     9 |  527 | `	if( nArg < 2 ){` |
|   ! 0 |  528 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  529 | `			"preg_match_all() expects at least 2 parameters");` |
|   ! 0 |  530 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  531 | `		return PH7_OK;` |
|     - |  532 | `	}` |
|     9 |  533 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     9 |  534 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     9 |  535 | `	if( nArg >= 4 ){` |
|     3 |  536 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     1 |  537 | `	}` |
|     9 |  538 | `	if( nArg >= 5 ){` |
|   ! 0 |  539 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  540 | `	}` |
|     9 |  541 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     9 |  542 | `	if( pCode == 0 ){` |
|   ! 0 |  543 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  544 | `		return PH7_OK;` |
|     - |  545 | `	}` |
|     9 |  546 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     9 |  547 | `	if( pMatchData == 0 ){` |
|   ! 0 |  548 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  549 | `		return PH7_OK;` |
|     - |  550 | `	}` |
|     9 |  551 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  552 | `	{` |
|     9 |  553 | `		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;` |
|     - |  554 |  |
|     9 |  555 | `		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){` |
|     7 |  556 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  557 | `				PCRE2_SIZE *ovector;` |
|    10 |  558 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     3 |  559 | `					startOffset, 0, pMatchData, NULL);` |
|     7 |  560 | `				if( rc < 0 ){` |
|     3 |  561 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     3 |  562 | `					break;` |
|     - |  563 | `				}` |
|     5 |  564 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     5 |  565 | `				if( pOutArray ){` |
|     5 |  566 | `					ph7_value *pSet = ph7_context_new_array(pCtx);` |
|     5 |  567 | `					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, iFlags & ~0xFF);` |
|     5 |  568 | `					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);` |
|     5 |  569 | `					ph7_context_release_value(pCtx, pSet);` |
|     2 |  570 | `				}` |
|     5 |  571 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  572 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  573 | `				}else{` |
|     5 |  574 | `					startOffset = ovector[1];` |
|     - |  575 | `				}` |
|     5 |  576 | `				totalMatches++;` |
|     1 |  577 | `			}` |
|     2 |  578 | `		}else{` |
|     - |  579 | `			/* PREG_PATTERN_ORDER (default) */` |
|     7 |  580 | `			ph7_value **apGroupArrays = 0;` |
|     7 |  581 | `			sxu32 nGroups = nCapture + 1;` |
|     - |  582 | `			sxu32 g;` |
|     7 |  583 | `			if( pOutArray ){` |
|    10 |  584 | `				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,` |
|     3 |  585 | `					sizeof(ph7_value *) * nGroups, TRUE, FALSE);` |
|     7 |  586 | `				if( apGroupArrays ){` |
|    17 |  587 | `					for( g = 0; g < nGroups; g++ ){` |
|    11 |  588 | `						apGroupArrays[g] = ph7_context_new_array(pCtx);` |
|     6 |  589 | `					}` |
|     3 |  590 | `				}` |
|     3 |  591 | `			}` |
|    23 |  592 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  593 | `				PCRE2_SIZE *ovector;` |
|    34 |  594 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    11 |  595 | `					startOffset, 0, pMatchData, NULL);` |
|    23 |  596 | `				if( rc < 0 ){` |
|     7 |  597 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     7 |  598 | `					break;` |
|     - |  599 | `				}` |
|    17 |  600 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    17 |  601 | `				if( apGroupArrays ){` |
|    17 |  602 | `					ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    17 |  603 | `					int nActual = rc;` |
|    41 |  604 | `					for( g = 0; g < nGroups; g++ ){` |
|    37 |  605 | `						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){` |
|    25 |  606 | `							PCRE2_SIZE s = ovector[2*g];` |
|    25 |  607 | `							PCRE2_SIZE e = ovector[2*g+1];` |
|    25 |  608 | `							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  609 | `								ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  610 | `								ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  611 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|   ! 0 |  612 | `								ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  613 | `								ph7_value_int(pOff, (int)s);` |
|   ! 0 |  614 | `								ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  615 | `								ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|   ! 0 |  616 | `								ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  617 | `								ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  618 | `							}else{` |
|    25 |  619 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|    25 |  620 | `								ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  621 | `							}` |
|    13 |  622 | `						}else{` |
|   ! 0 |  623 | `							ph7_value_string(pVal, "", 0);` |
|   ! 0 |  624 | `							ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  625 | `						}` |
|    25 |  626 | `						ph7_value_reset_string_cursor(pVal);` |
|    13 |  627 | `					}` |
|    17 |  628 | `					ph7_context_release_value(pCtx, pVal);` |
|     8 |  629 | `				}` |
|    17 |  630 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  631 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  632 | `				}else{` |
|    17 |  633 | `					startOffset = ovector[1];` |
|     - |  634 | `				}` |
|    17 |  635 | `				totalMatches++;` |
|     1 |  636 | `			}` |
|     7 |  637 | `			if( apGroupArrays ){` |
|    17 |  638 | `				for( g = 0; g < nGroups; g++ ){` |
|    11 |  639 | `					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);` |
|    11 |  640 | `					ph7_context_release_value(pCtx, apGroupArrays[g]);` |
|     6 |  641 | `				}` |
|     7 |  642 | `				ph7_context_free_chunk(pCtx, apGroupArrays);` |
|     3 |  643 | `			}` |
|     - |  644 | `		}` |
|     - |  645 | `		/* Write output array to caller's variable */` |
|     9 |  646 | `		if( pOutArray && nArg >= 3 ){` |
|     9 |  647 | `			PcreStoreByRef(pCtx->pVm, apArg[2], pOutArray);` |
|     9 |  648 | `			ph7_context_release_value(pCtx, pOutArray);` |
|     4 |  649 | `		}` |
|     - |  650 | `	}` |
|     9 |  651 | `	pcre2_match_data_free(pMatchData);` |
|     9 |  652 | `	ph7_result_int(pCtx, totalMatches);` |
|     9 |  653 | `	return PH7_OK;` |
|     5 |  654 |  |
|     - |  655 |  |
|     - |  656 | `/* ======================================================================` |
|     - |  657 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|     - |  658 | ` * ====================================================================== */` |
|     4 |  659 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  660 |  |
|     - |  661 | `	const char *zPattern, *zSubject;` |
|     - |  662 | `	int nPatLen, nSubLen;` |
|     - |  663 | `	pcre2_code *pCode;` |
|     - |  664 | `	pcre2_match_data *pMatchData;` |
|     - |  665 | `	sxu32 nCapture;` |
|     - |  666 | `	ph7_value *pArray;` |
|     - |  667 | `	ph7_value *pVal;` |
|     5 |  668 | `	PCRE2_SIZE startOffset = 0, lastOffset = 0;` |
|     5 |  669 | `	int limit = -1;` |
|     5 |  670 | `	int iFlags = 0;` |
|     5 |  671 | `	int nPieces = 0;` |
|     - |  672 | `	int rc;` |
|     - |  673 |  |
|     5 |  674 | `	if( nArg < 2 ){` |
|   ! 0 |  675 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  676 | `			"preg_split() expects at least 2 parameters");` |
|   ! 0 |  677 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  678 | `		return PH7_OK;` |
|     - |  679 | `	}` |
|     5 |  680 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     5 |  681 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     5 |  682 | `	if( nArg >= 3 ){` |
|     3 |  683 | `		limit = ph7_value_to_int(apArg[2]);` |
|     1 |  684 | `	}` |
|     5 |  685 | `	if( nArg >= 4 ){` |
|   ! 0 |  686 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  687 | `	}` |
|     5 |  688 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     5 |  689 | `	if( pCode == 0 ){` |
|   ! 0 |  690 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  691 | `		return PH7_OK;` |
|     - |  692 | `	}` |
|     5 |  693 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     5 |  694 | `	if( pMatchData == 0 ){` |
|   ! 0 |  695 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  696 | `		return PH7_OK;` |
|     - |  697 | `	}` |
|     5 |  698 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  699 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     5 |  700 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  701 |  |
|    13 |  702 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|    13 |  703 | `		if( limit > 0 && nPieces >= limit - 1 ){` |
|     3 |  704 | `			break; /* Last piece gets the remainder */` |
|     - |  705 | `		}` |
|    16 |  706 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     5 |  707 | `			startOffset, 0, pMatchData, NULL);` |
|    11 |  708 | `		if( rc < 0 ){` |
|     3 |  709 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  710 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  711 | `			}` |
|     3 |  712 | `			break;` |
|     - |  713 | `		}` |
|     - |  714 | `		{` |
|     9 |  715 | `			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     9 |  716 | `			PCRE2_SIZE matchStart = ovector[0];` |
|     9 |  717 | `			PCRE2_SIZE matchEnd = ovector[1];` |
|     9 |  718 | `			int pieceLen = (int)(matchStart - lastOffset);` |
|     - |  719 |  |
|     - |  720 | `			/* Add the piece before the match */` |
|     9 |  721 | `			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| pieceLen > 0 ){` |
|     9 |  722 | `				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  723 | `					ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  724 | `					ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  725 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|   ! 0 |  726 | `					ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  727 | `					ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  728 | `					ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  729 | `					ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  730 | `					ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  731 | `					ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  732 | `				}else{` |
|     9 |  733 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|     9 |  734 | `					ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  735 | `				}` |
|     9 |  736 | `				ph7_value_reset_string_cursor(pVal);` |
|     9 |  737 | `				nPieces++;` |
|     4 |  738 | `			}` |
|     - |  739 | `			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */` |
|     9 |  740 | `			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){` |
|     - |  741 | `				int g;` |
|   ! 0 |  742 | `				for( g = 1; g < rc; g++ ){` |
|   ! 0 |  743 | `					PCRE2_SIZE gs = ovector[2*g];` |
|   ! 0 |  744 | `					PCRE2_SIZE ge = ovector[2*g+1];` |
|     - |  745 | `					int gLen;` |
|   ! 0 |  746 | `					if( gs == PCRE2_UNSET ) continue;` |
|   ! 0 |  747 | `					gLen = (int)(ge - gs);` |
|   ! 0 |  748 | `					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| gLen > 0 ){` |
|   ! 0 |  749 | `						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  750 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  751 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  752 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  753 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  754 | `							ph7_value_int(pOff, (int)gs);` |
|   ! 0 |  755 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  756 | `							ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  757 | `							ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  758 | `							ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  759 | `						}else{` |
|   ! 0 |  760 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  761 | `							ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  762 | `						}` |
|   ! 0 |  763 | `						ph7_value_reset_string_cursor(pVal);` |
|   ! 0 |  764 | `					}` |
|   ! 0 |  765 | `				}` |
|   ! 0 |  766 | `			}` |
|     - |  767 | `			/* Advance */` |
|     9 |  768 | `			lastOffset = matchEnd;` |
|     9 |  769 | `			if( matchEnd == matchStart ){` |
|   ! 0 |  770 | `				startOffset = matchEnd + 1;` |
|   ! 0 |  771 | `			}else{` |
|     9 |  772 | `				startOffset = matchEnd;` |
|     - |  773 | `			}` |
|     - |  774 | `		}` |
|     1 |  775 | `	}` |
|     - |  776 | `	/* Add trailing piece */` |
|     - |  777 | `	{` |
|     5 |  778 | `		int trailLen = nSubLen - (int)lastOffset;` |
|     5 |  779 | `		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| trailLen > 0 ){` |
|     5 |  780 | `			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  781 | `				ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  782 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  783 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|   ! 0 |  784 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  785 | `				ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  786 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  787 | `				ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  788 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  789 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  790 | `			}else{` |
|     5 |  791 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|     5 |  792 | `				ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  793 | `			}` |
|     2 |  794 | `		}` |
|     - |  795 | `	}` |
|     5 |  796 | `	ph7_context_release_value(pCtx, pVal);` |
|     5 |  797 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  798 | `	ph7_result_value(pCtx, pArray);` |
|     5 |  799 | `	ph7_context_release_value(pCtx, pArray);` |
|     5 |  800 | `	return PH7_OK;` |
|     3 |  801 |  |
|     - |  802 |  |
|     - |  803 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|    20 |  804 | `static void PcreExpandBackrefs(` |
|     - |  805 | `	SyBlob *pOut,` |
|     - |  806 | `	const char *zRepl, int nReplLen,` |
|     - |  807 | `	const char *zSubject,` |
|     - |  808 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     1 |  809 |  |
|    21 |  810 | `	const char *zEnd = &zRepl[nReplLen];` |
|    21 |  811 | `	const char *z = zRepl;` |
|     - |  812 |  |
|    45 |  813 | `	while( z < zEnd ){` |
|    25 |  814 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   ! 0 |  815 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|   ! 0 |  816 | `				int g = z[1] - '0';` |
|   ! 0 |  817 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  818 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  819 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  820 | `				}` |
|   ! 0 |  821 | `				z += 2;` |
|   ! 0 |  822 | `				continue;` |
|     - |  823 | `			}` |
|   ! 0 |  824 | `			if( z[1] == '\\' ){` |
|   ! 0 |  825 | `				SyBlobAppend(pOut, "\\", 1);` |
|   ! 0 |  826 | `				z += 2;` |
|   ! 0 |  827 | `				continue;` |
|     - |  828 | `			}` |
|     - |  829 | `			/* Not a backreference — emit literally */` |
|   ! 0 |  830 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  831 | `			z++;` |
|   ! 0 |  832 | `			continue;` |
|     - |  833 | `		}` |
|    25 |  834 | `		if( *z == '$' && z + 1 < zEnd ){` |
|     5 |  835 | `			if( z[1] == '$' ){` |
|   ! 0 |  836 | `				SyBlobAppend(pOut, "$", 1);` |
|   ! 0 |  837 | `				z += 2;` |
|   ! 0 |  838 | `				continue;` |
|     - |  839 | `			}` |
|     5 |  840 | `			if( z[1] == '{' ){` |
|     - |  841 | `				/* ${N} form */` |
|   ! 0 |  842 | `				const char *p = z + 2;` |
|   ! 0 |  843 | `				int g = 0;` |
|   ! 0 |  844 | `				while( p < zEnd && *p >= '0' && *p <= '9' ){` |
|   ! 0 |  845 | `					g = g * 10 + (*p - '0');` |
|   ! 0 |  846 | `					p++;` |
|   ! 0 |  847 | `				}` |
|   ! 0 |  848 | `				if( p < zEnd && *p == '}' ){` |
|   ! 0 |  849 | `					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  850 | `						SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  851 | `							(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  852 | `					}` |
|   ! 0 |  853 | `					z = p + 1;` |
|   ! 0 |  854 | `					continue;` |
|     - |  855 | `				}` |
|     - |  856 | `				/* Not a valid ${N} — emit literally */` |
|   ! 0 |  857 | `				SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  858 | `				z++;` |
|   ! 0 |  859 | `				continue;` |
|     - |  860 | `			}` |
|     5 |  861 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|     - |  862 | `				/* $N or $NN */` |
|     5 |  863 | `				int g = z[1] - '0';` |
|     5 |  864 | `				z += 2;` |
|     - |  865 | `				/* Check for second digit */` |
|     5 |  866 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  867 | `					int g2 = g * 10 + (*z - '0');` |
|   ! 0 |  868 | `					if( g2 < nGroups ){` |
|   ! 0 |  869 | `						g = g2;` |
|   ! 0 |  870 | `						z++;` |
|   ! 0 |  871 | `					}` |
|   ! 0 |  872 | `				}` |
|     5 |  873 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|     7 |  874 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|     4 |  875 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|     2 |  876 | `				}` |
|     5 |  877 | `				continue;` |
|     - |  878 | `			}` |
|     - |  879 | `			/* Not a backreference */` |
|   ! 0 |  880 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  881 | `			z++;` |
|   ! 0 |  882 | `			continue;` |
|     - |  883 | `		}` |
|    21 |  884 | `		SyBlobAppend(pOut, z, 1);` |
|    21 |  885 | `		z++;` |
|     1 |  886 | `	}` |
|    21 |  887 |  |
|     - |  888 |  |
|     - |  889 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|    10 |  890 | `static void PcreDoReplace(` |
|     - |  891 | `	ph7_context *pCtx,` |
|     - |  892 | `	pcre2_code *pCode,` |
|     - |  893 | `	const char *zSubject, int nSubLen,` |
|     - |  894 | `	const char *zRepl, int nReplLen,` |
|     - |  895 | `	int limit,` |
|     - |  896 | `	int *pCount,` |
|     - |  897 | `	SyBlob *pOut)` |
|     1 |  898 |  |
|     - |  899 | `	pcre2_match_data *pMatchData;` |
|    11 |  900 | `	PCRE2_SIZE startOffset = 0;` |
|    11 |  901 | `	int nReplacements = 0;` |
|     - |  902 | `	int rc;` |
|     - |  903 |  |
|    11 |  904 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    11 |  905 | `	if( pMatchData == 0 ) return;` |
|     - |  906 |  |
|    31 |  907 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  908 | `		PCRE2_SIZE *ovector;` |
|    31 |  909 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|    46 |  910 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    15 |  911 | `			startOffset, 0, pMatchData, NULL);` |
|    31 |  912 | `		if( rc < 0 ){` |
|    11 |  913 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  914 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  915 | `			}` |
|    11 |  916 | `			break;` |
|     - |  917 | `		}` |
|    21 |  918 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - |  919 | `		/* Copy text before match */` |
|    21 |  920 | `		if( ovector[0] > startOffset ){` |
|    17 |  921 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     8 |  922 | `		}` |
|     - |  923 | `		/* Expand replacement */` |
|    21 |  924 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|    21 |  925 | `		nReplacements++;` |
|     - |  926 | `		/* Advance */` |
|    21 |  927 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 |  928 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 |  929 | `				SyBlobAppend(pOut, &zSubject[startOffset], 1);` |
|   ! 0 |  930 | `			}` |
|   ! 0 |  931 | `			startOffset = ovector[0] + 1;` |
|   ! 0 |  932 | `		}else{` |
|    21 |  933 | `			startOffset = ovector[1];` |
|     - |  934 | `		}` |
|     1 |  935 | `	}` |
|     - |  936 | `	/* Copy remainder */` |
|    11 |  937 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 |  938 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   ! 0 |  939 | `	}` |
|    11 |  940 | `	if( pCount ){` |
|    11 |  941 | `		*pCount += nReplacements;` |
|     5 |  942 | `	}` |
|    11 |  943 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  944 | `	SXUNUSED(pCtx);` |
|     6 |  945 |  |
|     - |  946 |  |
|     - |  947 | `/* ======================================================================` |
|     - |  948 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - |  949 | ` * ====================================================================== */` |
|    10 |  950 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  951 |  |
|    11 |  952 | `	int limit = -1;` |
|    11 |  953 | `	int count = 0;` |
|     - |  954 |  |
|    11 |  955 | `	if( nArg < 3 ){` |
|   ! 0 |  956 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  957 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 |  958 | `		ph7_result_null(pCtx);` |
|   ! 0 |  959 | `		return PH7_OK;` |
|     - |  960 | `	}` |
|    11 |  961 | `	if( nArg >= 4 ){` |
|     5 |  962 | `		limit = ph7_value_to_int(apArg[3]);` |
|     2 |  963 | `	}` |
|    11 |  964 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  965 |  |
|     - |  966 | `	/* Reject array subjects (not yet supported) */` |
|    11 |  967 | `	if( ph7_value_is_array(apArg[2]) ){` |
|   ! 0 |  968 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  969 | `			"preg_replace(): Array subjects are not yet supported");` |
|   ! 0 |  970 | `		ph7_result_null(pCtx);` |
|   ! 0 |  971 | `		return PH7_OK;` |
|     - |  972 | `	}` |
|    11 |  973 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     - |  974 | `		/* Single pattern + single replacement on a string subject */` |
|     - |  975 | `		const char *zPattern, *zRepl, *zSubject;` |
|     - |  976 | `		int nPatLen, nReplLen, nSubLen;` |
|     - |  977 | `		pcre2_code *pCode;` |
|     - |  978 | `		sxu32 nCapture;` |
|     - |  979 | `		SyBlob sOut;` |
|     - |  980 |  |
|    11 |  981 | `		zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    11 |  982 | `		zRepl = ph7_value_to_string(apArg[1], &nReplLen);` |
|    11 |  983 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|     - |  984 |  |
|    11 |  985 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    11 |  986 | `		if( pCode == 0 ){` |
|   ! 0 |  987 | `			ph7_result_null(pCtx);` |
|   ! 0 |  988 | `			return PH7_OK;` |
|     - |  989 | `		}` |
|    11 |  990 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    11 |  991 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, &count, &sOut);` |
|    11 |  992 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    11 |  993 | `		SyBlobRelease(&sOut);` |
|     6 |  994 | `	}else{` |
|     - |  995 | `		/* TODO: array of patterns — iterate pairs and apply sequentially */` |
|   ! 0 |  996 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  997 | `			"preg_replace() with array patterns is not yet supported");` |
|   ! 0 |  998 | `		ph7_result_null(pCtx);` |
|   ! 0 |  999 | `		return PH7_OK;` |
|     - | 1000 | `	}` |
|     - | 1001 | `	/* Set &$count if provided */` |
|    11 | 1002 | `	if( nArg >= 5 ){` |
|     - | 1003 | `		ph7_value sCount;` |
|     5 | 1004 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     5 | 1005 | `		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);` |
|     5 | 1006 | `		PH7_MemObjRelease(&sCount);` |
|     2 | 1007 | `	}` |
|    11 | 1008 | `	return PH7_OK;` |
|     6 | 1009 |  |
|     - | 1010 |  |
|     - | 1011 | `/* ======================================================================` |
|     - | 1012 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count]])` |
|     - | 1013 | ` * ====================================================================== */` |
|     8 | 1014 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1015 |  |
|     - | 1016 | `	const char *zPattern, *zSubject;` |
|     - | 1017 | `	int nPatLen, nSubLen;` |
|     - | 1018 | `	pcre2_code *pCode;` |
|     - | 1019 | `	pcre2_match_data *pMatchData;` |
|     - | 1020 | `	sxu32 nCapture;` |
|     - | 1021 | `	SyBlob sOut;` |
|    10 | 1022 | `	PCRE2_SIZE startOffset = 0;` |
|    10 | 1023 | `	int limit = -1;` |
|    10 | 1024 | `	int count = 0;` |
|     - | 1025 | `	int rc;` |
|     - | 1026 |  |
|    10 | 1027 | `	if( nArg < 3 ){` |
|   ! 0 | 1028 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1029 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 | 1030 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1031 | `		return PH7_OK;` |
|     - | 1032 | `	}` |
|    10 | 1033 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    10 | 1034 | `	zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    10 | 1035 | `	if( nArg >= 4 ){` |
|     3 | 1036 | `		limit = ph7_value_to_int(apArg[3]);` |
|     1 | 1037 | `	}` |
|    10 | 1038 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 | 1039 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1040 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 | 1041 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1042 | `		return PH7_OK;` |
|     - | 1043 | `	}` |
|    10 | 1044 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    10 | 1045 | `	if( pCode == 0 ){` |
|   ! 0 | 1046 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1047 | `		return PH7_OK;` |
|     - | 1048 | `	}` |
|    10 | 1049 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    10 | 1050 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1051 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1052 | `		return PH7_OK;` |
|     - | 1053 | `	}` |
|    10 | 1054 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    10 | 1055 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1056 |  |
|    28 | 1057 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1058 | `		PCRE2_SIZE *ovector;` |
|     - | 1059 | `		ph7_value *pMatchArr;` |
|     - | 1060 | `		ph7_value *apCbArg[1];` |
|     - | 1061 | `		ph7_value sResult;` |
|     - | 1062 | `		const char *zReplacement;` |
|     - | 1063 | `		int nReplLen;` |
|     - | 1064 |  |
|    32 | 1065 | `		if( limit >= 0 && count >= limit ) break;` |
|    41 | 1066 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    13 | 1067 | `			startOffset, 0, pMatchData, NULL);` |
|    28 | 1068 | `		if( rc < 0 ){` |
|    10 | 1069 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1070 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1071 | `			}` |
|    10 | 1072 | `			break;` |
|     - | 1073 | `		}` |
|    20 | 1074 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1075 | `		/* Copy text before match */` |
|    20 | 1076 | `		if( ovector[0] > startOffset ){` |
|    14 | 1077 | `			SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     6 | 1078 | `		}` |
|     - | 1079 | `		/* Build matches array for callback */` |
|    20 | 1080 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    20 | 1081 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, 0);` |
|     - | 1082 | `		/* Call the callback */` |
|    20 | 1083 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    20 | 1084 | `		apCbArg[0] = pMatchArr;` |
|    20 | 1085 | `		if( PH7_VmCallUserFunction(pCtx->pVm, apArg[1], 1, apCbArg, &sResult) == PH7_EXCEPTION ){` |
|     - | 1086 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|   ! 0 | 1087 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1088 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|   ! 0 | 1089 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1090 | `			pcre2_match_data_free(pMatchData);` |
|   ! 0 | 1091 | `			return PH7_EXCEPTION;` |
|     - | 1092 | `		}` |
|     - | 1093 | `		/* Get replacement string from callback result */` |
|    20 | 1094 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    20 | 1095 | `		SyBlobAppend(&sOut, zReplacement, (sxu32)nReplLen);` |
|    20 | 1096 | `		PH7_MemObjRelease(&sResult);` |
|    20 | 1097 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    20 | 1098 | `		count++;` |
|     - | 1099 | `		/* Advance */` |
|    20 | 1100 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 | 1101 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1102 | `				SyBlobAppend(&sOut, &zSubject[startOffset], 1);` |
|   ! 0 | 1103 | `			}` |
|   ! 0 | 1104 | `			startOffset = ovector[0] + 1;` |
|   ! 0 | 1105 | `		}else{` |
|    20 | 1106 | `			startOffset = ovector[1];` |
|     - | 1107 | `		}` |
|     2 | 1108 | `	}` |
|     - | 1109 | `	/* Copy remainder */` |
|    10 | 1110 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1111 | `		SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   ! 0 | 1112 | `	}` |
|    10 | 1113 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    10 | 1114 | `	SyBlobRelease(&sOut);` |
|    10 | 1115 | `	pcre2_match_data_free(pMatchData);` |
|     - | 1116 | `	/* Set &$count if provided */` |
|    10 | 1117 | `	if( nArg >= 5 ){` |
|     - | 1118 | `		ph7_value sCount;` |
|     3 | 1119 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     3 | 1120 | `		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);` |
|     3 | 1121 | `		PH7_MemObjRelease(&sCount);` |
|     1 | 1122 | `	}` |
|    10 | 1123 | `	return PH7_OK;` |
|     6 | 1124 |  |
|     - | 1125 |  |
|     - | 1126 | `/* ======================================================================` |
|     - | 1127 | ` * preg_quote(str [, delimiter])` |
|     - | 1128 | ` * ====================================================================== */` |
|     6 | 1129 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1130 |  |
|     7 | 1131 | `	const char *zStr, *zDelim = 0;` |
|     7 | 1132 | `	int nLen, nDelimLen = 0;` |
|     - | 1133 | `	const char *z, *zEnd;` |
|     - | 1134 |  |
|     7 | 1135 | `	if( nArg < 1 ){` |
|   ! 0 | 1136 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1137 | `		return PH7_OK;` |
|     - | 1138 | `	}` |
|     7 | 1139 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|     7 | 1140 | `	if( nArg >= 2 ){` |
|     3 | 1141 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     1 | 1142 | `	}` |
|     7 | 1143 | `	z = zStr;` |
|     7 | 1144 | `	zEnd = &zStr[nLen];` |
|    71 | 1145 | `	while( z < zEnd ){` |
|    65 | 1146 | `		char c = *z;` |
|    65 | 1147 | `		switch( c ){` |
|     4 | 1148 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1149 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1150 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1151 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1152 | `			case '#':` |
|     9 | 1153 | `				ph7_result_string(pCtx, "\\", 1);` |
|     9 | 1154 | `				break;` |
|    28 | 1155 | `			default:` |
|    57 | 1156 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     3 | 1157 | `					ph7_result_string(pCtx, "\\", 1);` |
|     1 | 1158 | `				}` |
|    56 | 1159 | `				break;` |
|     - | 1160 | `		}` |
|    65 | 1161 | `		ph7_result_string(pCtx, z, 1);` |
|    65 | 1162 | `		z++;` |
|     1 | 1163 | `	}` |
|     7 | 1164 | `	return PH7_OK;` |
|     4 | 1165 |  |
|     - | 1166 |  |
|     - | 1167 | `/* ======================================================================` |
|     - | 1168 | ` * preg_last_error()` |
|     - | 1169 | ` * ====================================================================== */` |
|   ! 0 | 1170 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1171 |  |
|   ! 0 | 1172 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1173 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1174 | `	return PH7_OK;` |
|   ! 0 | 1175 |  |
|     - | 1176 |  |
|     - | 1177 | `/* ======================================================================` |
|     - | 1178 | ` * preg_last_error_msg()` |
|     - | 1179 | ` * ====================================================================== */` |
|   ! 0 | 1180 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1181 |  |
|     - | 1182 | `	const char *zMsg;` |
|   ! 0 | 1183 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1184 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1185 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1186 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1187 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1188 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1189 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1190 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1191 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1192 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1193 | `	}` |
|   ! 0 | 1194 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1195 | `	return PH7_OK;` |
|   ! 0 | 1196 |  |
|     - | 1197 |  |
|     - | 1198 | `/* ===== Function registration table ===== */` |
|     - | 1199 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1200 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1201 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1202 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1203 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1204 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1205 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1206 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1207 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1208 | `};` |
|     - | 1209 |  |
|  3170 | 1210 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     5 | 1211 |  |
|     - | 1212 | `	sxu32 n;` |
| 28535 | 1213 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 25365 | 1214 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 12685 | 1215 | `	}` |
|  3175 | 1216 |  |
|     - | 1217 |  |
|     - | 1218 | `/* ===== Constant registration ===== */` |
|     - | 1219 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1220 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1221 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1222 | `	}` |
|     - | 1223 |  |
|   ! 0 | 1224 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|     3 | 1225 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|   ! 0 | 1226 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|   ! 0 | 1227 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|   ! 0 | 1228 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|   ! 0 | 1229 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|   ! 0 | 1230 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|   ! 0 | 1231 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|   ! 0 | 1232 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|   ! 0 | 1233 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|   ! 0 | 1234 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|   ! 0 | 1235 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|   ! 0 | 1236 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|   ! 0 | 1237 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1238 |  |
|  3170 | 1239 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     5 | 1240 |  |
|  3175 | 1241 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  3175 | 1242 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  3175 | 1243 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  3175 | 1244 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  3175 | 1245 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  3175 | 1246 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  3175 | 1247 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  3175 | 1248 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  3175 | 1249 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  3175 | 1250 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  3175 | 1251 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  3175 | 1252 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  3175 | 1253 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  3175 | 1254 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  3175 | 1255 |  |
|     - | 1256 |  |
|     - | 1257 | `#else` |
|     - | 1258 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1259 | `typedef int vm_pcre_unused;` |
|     - | 1260 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1261 |  |
