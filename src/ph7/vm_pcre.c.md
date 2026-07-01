# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 605/927 lines (65.26%)

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
|   122 |   55 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|     5 |   56 | `{` |
|     - |   57 | `	sxu32 i;` |
|  1259 |   58 | `	for( i = 0; i < nCacheUsed; i++ ){` |
|  1202 |   59 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|    69 |   60 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|    69 |   61 | `			if( pCaptureCount ){` |
|    69 |   62 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|    34 |   63 | `			}` |
|    69 |   64 | `			return aCache[i].pCode;` |
|     - |   65 | `		}` |
|   568 |   66 | `	}` |
|    59 |   67 | `	return 0;` |
|    66 |   68 | `}` |
|     - |   69 |  |
|    54 |   70 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|     5 |   71 | `{` |
|     - |   72 | `	PcreCacheEntry *pEntry;` |
|     - |   73 | `	char *zCopy;` |
|     - |   74 | `	/* Allocate the pattern copy first, before touching the cache */` |
|    59 |   75 | `	zCopy = (char *)malloc(nLen + 1);` |
|    59 |   76 | `	if( zCopy == 0 ){` |
|     - |   77 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|   ! 0 |   78 | `		return;` |
|     - |   79 | `	}` |
|    59 |   80 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|    59 |   81 | `	zCopy[nLen] = 0;` |
|    59 |   82 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|    41 |   83 | `		pEntry = &aCache[nCacheUsed++];` |
|    23 |   84 | `	}else{` |
|     - |   85 | `		/* Evict LRU */` |
|    19 |   86 | `		sxu32 iMin = aCache[0].iLastUsed;` |
|    19 |   87 | `		sxu32 iMinIdx = 0;` |
|     - |   88 | `		sxu32 i;` |
|   289 |   89 | `		for( i = 1; i < PCRE_CACHE_SIZE; i++ ){` |
|   271 |   90 | `			if( aCache[i].iLastUsed < iMin ){` |
|    49 |   91 | `				iMin = aCache[i].iLastUsed;` |
|    49 |   92 | `				iMinIdx = i;` |
|    24 |   93 | `			}` |
|   136 |   94 | `		}` |
|    19 |   95 | `		pEntry = &aCache[iMinIdx];` |
|    19 |   96 | `		pcre2_code_free(pEntry->pCode);` |
|    19 |   97 | `		free(pEntry->zPattern);` |
|     - |   98 | `	}` |
|    59 |   99 | `	pEntry->zPattern = zCopy;` |
|    59 |  100 | `	pEntry->nLen = nLen;` |
|    59 |  101 | `	pEntry->pCode = pCode;` |
|    59 |  102 | `	pEntry->nCaptureCount = nCaptureCount;` |
|    59 |  103 | `	pEntry->iLastUsed = ++iCacheClock;` |
|    32 |  104 | `}` |
|     - |  105 |  |
|     - |  106 | `/* ===== Delimiter parser ===== */` |
|     - |  107 | `#define PCRE_PARSE_OK             0` |
|     - |  108 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|     - |  109 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */` |
|     - |  110 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|     - |  111 |  |
|    54 |  112 | `static sxi32 PcreParsePattern(` |
|     - |  113 | `	const char *zInput, int nInputLen,` |
|     - |  114 | `	const char **pPattern, int *pnPatternLen,` |
|     - |  115 | `	const char **pFlags, int *pnFlagLen)` |
|     5 |  116 | `{` |
|    59 |  117 | `	const char *zEnd = &zInput[nInputLen];` |
|    59 |  118 | `	const char *z = zInput;` |
|     - |  119 | `	char cOpen, cClose;` |
|     - |  120 | `	const char *pStart;` |
|     - |  121 |  |
|     - |  122 | `	/* Skip leading whitespace */` |
|    59 |  123 | `	while( z < zEnd && (unsigned char)*z <= 0x20 ){` |
|   ! 0 |  124 | `		z++;` |
|   ! 0 |  125 | `	}` |
|    59 |  126 | `	if( z >= zEnd ){` |
|   ! 0 |  127 | `		return PCRE_PARSE_EMPTY;` |
|     - |  128 | `	}` |
|    59 |  129 | `	cOpen = *z;` |
|     - |  130 | `	/* Must not be alphanumeric, backslash, or whitespace */` |
|    59 |  131 | `	if( SyisAlphaNum(cOpen) \|\| cOpen == '\\' \|\| (unsigned char)cOpen <= 0x20 ){` |
|   ! 0 |  132 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|     - |  133 | `	}` |
|     - |  134 | `	/* Paired delimiters */` |
|    59 |  135 | `	switch( cOpen ){` |
|   ! 0 |  136 | `		case '(': cClose = ')'; break;` |
|   ! 0 |  137 | `		case '[': cClose = ']'; break;` |
|   ! 0 |  138 | `		case '{': cClose = '}'; break;` |
|   ! 0 |  139 | `		case '<': cClose = '>'; break;` |
|    59 |  140 | `		default:  cClose = cOpen; break;` |
|     - |  141 | `	}` |
|    59 |  142 | `	z++; /* Skip opening delimiter */` |
|    59 |  143 | `	pStart = z;` |
|     - |  144 | `	/* Scan for closing delimiter, respecting backslash escapes */` |
|   425 |  145 | `	while( z < zEnd ){` |
|   425 |  146 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|    51 |  147 | `			z += 2; /* Skip escaped char */` |
|    51 |  148 | `			continue;` |
|     - |  149 | `		}` |
|   375 |  150 | `		if( *z == cClose ){` |
|    59 |  151 | `			break;` |
|     - |  152 | `		}` |
|   321 |  153 | `		z++;` |
|     5 |  154 | `	}` |
|    59 |  155 | `	if( z >= zEnd ){` |
|   ! 0 |  156 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|     - |  157 | `	}` |
|    59 |  158 | `	*pPattern = pStart;` |
|    59 |  159 | `	*pnPatternLen = (int)(z - pStart);` |
|    59 |  160 | `	z++; /* Skip closing delimiter */` |
|    59 |  161 | `	*pFlags = z;` |
|    59 |  162 | `	*pnFlagLen = (int)(zEnd - z);` |
|    59 |  163 | `	return PH7_OK;` |
|    32 |  164 | `}` |
|     - |  165 |  |
|     - |  166 | `/* ===== Flag mapper ===== */` |
|    54 |  167 | `static sxi32 PcreMapFlags(` |
|     - |  168 | `	const char *zFlags, int nFlagLen,` |
|     - |  169 | `	uint32_t *pCompileOpts)` |
|     5 |  170 | `{` |
|     - |  171 | `	int i;` |
|    59 |  172 | `	*pCompileOpts = 0;` |
|    73 |  173 | `	for( i = 0; i < nFlagLen; i++ ){` |
|    15 |  174 | `		switch( zFlags[i] ){` |
|    11 |  175 | `			case 'i': *pCompileOpts \|= PCRE2_CASELESS; break;` |
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
|     8 |  187 | `	}` |
|    59 |  188 | `	return PH7_OK;` |
|     5 |  189 | `}` |
|     - |  190 |  |
|     - |  191 | `/* ===== Compile helper ===== */` |
|   122 |  192 | `static pcre2_code *PcreCompile(` |
|     - |  193 | `	ph7_context *pCtx,` |
|     - |  194 | `	const char *zFullPattern, int nLen,` |
|     - |  195 | `	sxu32 *pCaptureCount)` |
|     5 |  196 | `{` |
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
|   127 |  207 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|   127 |  208 | `	if( pCode ){` |
|    69 |  209 | `		return pCode;` |
|     - |  210 | `	}` |
|     - |  211 | `	/* Parse delimiter */` |
|    59 |  212 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen);` |
|    59 |  213 | `	if( parseRc != PCRE_PARSE_OK ){` |
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
|    59 |  225 | `	PcreMapFlags(zFlags, nFlagLen, &compileOpts);` |
|     - |  226 | `	/* Compile */` |
|    59 |  227 | `	pCode = pcre2_compile(` |
|    27 |  228 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|    27 |  229 | `		compileOpts, &errcode, &erroffset, NULL);` |
|    59 |  230 | `	if( pCode == 0 ){` |
|     - |  231 | `		PCRE2_UCHAR errbuf[256];` |
|   ! 0 |  232 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|   ! 0 |  233 | `		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,` |
|   ! 0 |  234 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|   ! 0 |  235 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|   ! 0 |  236 | `		return 0;` |
|     - |  237 | `	}` |
|     - |  238 | `	/* Get capture count */` |
|    59 |  239 | `	nCapture = 0;` |
|    59 |  240 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|    59 |  241 | `	if( pCaptureCount ){` |
|    59 |  242 | `		*pCaptureCount = nCapture;` |
|    27 |  243 | `	}` |
|     - |  244 | `	/* Cache it */` |
|    59 |  245 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|    59 |  246 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    59 |  247 | `	return pCode;` |
|    66 |  248 | `}` |
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
|    46 |  268 | `static void PcreStoreByRef(ph7_vm *pVm, ph7_value *pArg, ph7_value *pNewVal)` |
|     5 |  269 | `{` |
|    51 |  270 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|    49 |  271 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj, pArg->nIdx);` |
|    49 |  272 | `		if( pObj ){` |
|    49 |  273 | `			PH7_MemObjStore(pNewVal, pObj);` |
|    22 |  274 | `		}` |
|    22 |  275 | `	}` |
|    51 |  276 | `	PH7_MemObjStore(pNewVal, pArg);` |
|    51 |  277 | `}` |
|     - |  278 |  |
|     - |  279 | `/* ===== Map PCRE2 match error to PHP error code ===== */` |
|   ! 0 |  280 | `static void PcreSetMatchError(ph7_vm *pVm, int rc)` |
|   ! 0 |  281 | `{` |
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
|   ! 0 |  304 | `}` |
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
|     5 |  315 | `{` |
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
|    53 |  405 | `}` |
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
|     1 |  416 | `{` |
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
|     3 |  441 | `}` |
|     - |  442 | `/* ======================================================================` |
|     - |  443 | ` * preg_match(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  444 | ` * ====================================================================== */` |
|    38 |  445 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  446 | `{` |
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
|    24 |  510 | `}` |
|     - |  511 |  |
|     - |  512 | `/* ======================================================================` |
|     - |  513 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  514 | ` * ====================================================================== */` |
|     8 |  515 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  516 | `{` |
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
|     5 |  654 | `}` |
|     - |  655 |  |
|     - |  656 | `/* ======================================================================` |
|     - |  657 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|     - |  658 | ` * ====================================================================== */` |
|     4 |  659 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  660 | `{` |
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
|     3 |  801 | `}` |
|     - |  802 |  |
|     - |  803 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|    88 |  804 | `static void PcreExpandBackrefs(` |
|     - |  805 | `	SyBlob *pOut,` |
|     - |  806 | `	const char *zRepl, int nReplLen,` |
|     - |  807 | `	const char *zSubject,` |
|     - |  808 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     1 |  809 | `{` |
|    89 |  810 | `	const char *zEnd = &zRepl[nReplLen];` |
|    89 |  811 | `	const char *z = zRepl;` |
|     - |  812 |  |
|   183 |  813 | `	while( z < zEnd ){` |
|    95 |  814 | `		if( *z == '\\' && z + 1 < zEnd ){` |
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
|    95 |  834 | `		if( *z == '$' && z + 1 < zEnd ){` |
|    13 |  835 | `			if( z[1] == '$' ){` |
|   ! 0 |  836 | `				SyBlobAppend(pOut, "$", 1);` |
|   ! 0 |  837 | `				z += 2;` |
|   ! 0 |  838 | `				continue;` |
|     - |  839 | `			}` |
|    13 |  840 | `			if( z[1] == '{' ){` |
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
|    13 |  861 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|     - |  862 | `				/* $N or $NN */` |
|    13 |  863 | `				int g = z[1] - '0';` |
|    13 |  864 | `				z += 2;` |
|     - |  865 | `				/* Check for second digit */` |
|    13 |  866 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  867 | `					int g2 = g * 10 + (*z - '0');` |
|   ! 0 |  868 | `					if( g2 < nGroups ){` |
|   ! 0 |  869 | `						g = g2;` |
|   ! 0 |  870 | `						z++;` |
|   ! 0 |  871 | `					}` |
|   ! 0 |  872 | `				}` |
|    13 |  873 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|    19 |  874 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|    12 |  875 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|     6 |  876 | `				}` |
|    13 |  877 | `				continue;` |
|     - |  878 | `			}` |
|     - |  879 | `			/* Not a backreference */` |
|   ! 0 |  880 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  881 | `			z++;` |
|   ! 0 |  882 | `			continue;` |
|     - |  883 | `		}` |
|    83 |  884 | `		SyBlobAppend(pOut, z, 1);` |
|    83 |  885 | `		z++;` |
|     1 |  886 | `	}` |
|    89 |  887 | `}` |
|     - |  888 |  |
|     - |  889 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|    60 |  890 | `static void PcreDoReplace(` |
|     - |  891 | `	ph7_context *pCtx,` |
|     - |  892 | `	pcre2_code *pCode,` |
|     - |  893 | `	const char *zSubject, int nSubLen,` |
|     - |  894 | `	const char *zRepl, int nReplLen,` |
|     - |  895 | `	int limit,` |
|     - |  896 | `	int *pCount,` |
|     - |  897 | `	SyBlob *pOut)` |
|     1 |  898 | `{` |
|     - |  899 | `	pcre2_match_data *pMatchData;` |
|    61 |  900 | `	PCRE2_SIZE startOffset = 0;` |
|    61 |  901 | `	int nReplacements = 0;` |
|     - |  902 | `	int rc;` |
|     - |  903 |  |
|    61 |  904 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    61 |  905 | `	if( pMatchData == 0 ) return;` |
|     - |  906 |  |
|   149 |  907 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  908 | `		PCRE2_SIZE *ovector;` |
|   149 |  909 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|   223 |  910 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    74 |  911 | `			startOffset, 0, pMatchData, NULL);` |
|   149 |  912 | `		if( rc < 0 ){` |
|    61 |  913 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  914 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  915 | `			}` |
|    61 |  916 | `			break;` |
|     - |  917 | `		}` |
|    89 |  918 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - |  919 | `		/* Copy text before match */` |
|    89 |  920 | `		if( ovector[0] > startOffset ){` |
|    61 |  921 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|    30 |  922 | `		}` |
|     - |  923 | `		/* Expand replacement */` |
|    89 |  924 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|    89 |  925 | `		nReplacements++;` |
|     - |  926 | `		/* Advance */` |
|    89 |  927 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 |  928 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 |  929 | `				SyBlobAppend(pOut, &zSubject[startOffset], 1);` |
|   ! 0 |  930 | `			}` |
|   ! 0 |  931 | `			startOffset = ovector[0] + 1;` |
|   ! 0 |  932 | `		}else{` |
|    89 |  933 | `			startOffset = ovector[1];` |
|     - |  934 | `		}` |
|     1 |  935 | `	}` |
|     - |  936 | `	/* Copy remainder */` |
|    61 |  937 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|    15 |  938 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|     7 |  939 | `	}` |
|    61 |  940 | `	if( pCount ){` |
|    61 |  941 | `		*pCount += nReplacements;` |
|    30 |  942 | `	}` |
|    61 |  943 | `	pcre2_match_data_free(pMatchData);` |
|    30 |  944 | `	SXUNUSED(pCtx);` |
|    31 |  945 | `}` |
|     - |  946 |  |
|     - |  947 | `/* ===== Helper: apply pattern(s)+replacement(s) to ONE subject string =====` |
|     - |  948 | ` * pPattern is a string or an array of patterns; pRepl is a string (used for` |
|     - |  949 | ` * every pattern) or, only when pPattern is an array, an array taken by ORDER` |
|     - |  950 | ` * (missing element -> ""). Array patterns are applied sequentially, each to the` |
|     - |  951 | ` * result of the previous (PHP semantics), ping-ponging two blobs. The final` |
|     - |  952 | ` * text is appended to pOut. Returns SXRET_OK, or SXERR_ABORT on a bad pattern` |
|     - |  953 | ` * (the caller then yields NULL, matching the scalar path). */` |
|    52 |  954 | `static sxi32 PcreReplaceSubject(` |
|     - |  955 | `	ph7_context *pCtx,` |
|     - |  956 | `	ph7_value *pPattern,` |
|     - |  957 | `	ph7_value *pRepl,` |
|     - |  958 | `	const char *zSubject, int nSubLen,` |
|     - |  959 | `	int limit,` |
|     - |  960 | `	int *pCount,` |
|     - |  961 | `	SyBlob *pOut)` |
|     1 |  962 | `{` |
|     - |  963 | `	sxu32 nCapture;` |
|    53 |  964 | `	if( !ph7_value_is_array(pPattern) ){` |
|     - |  965 | `		/* Single pattern + single replacement */` |
|     - |  966 | `		const char *zPattern, *zRepl;` |
|     - |  967 | `		int nPatLen, nReplLen;` |
|     - |  968 | `		pcre2_code *pCode;` |
|    41 |  969 | `		zPattern = ph7_value_to_string(pPattern, &nPatLen);` |
|    41 |  970 | `		zRepl = ph7_value_to_string(pRepl, &nReplLen);` |
|    41 |  971 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    41 |  972 | `		if( pCode == 0 ){` |
|   ! 0 |  973 | `			return SXERR_ABORT;` |
|     - |  974 | `		}` |
|    41 |  975 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, pCount, pOut);` |
|    41 |  976 | `		return SXRET_OK;` |
|   ! 0 |  977 | `	}else{` |
|     - |  978 | `		/* Array of patterns: apply each in insertion order to the accumulating` |
|     - |  979 | `		 * subject. Replacement is the parallel array element (by order) or the` |
|     - |  980 | `		 * scalar replacement for every pattern. */` |
|    13 |  981 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|    13 |  982 | `		ph7_hashmap *pRepMap = ph7_value_is_array(pRepl) ? (ph7_hashmap *)pRepl->x.pOther : 0;` |
|    13 |  983 | `		const char *zScalarRepl = 0;` |
|    13 |  984 | `		int nScalarRepl = 0;` |
|     - |  985 | `		ph7_hashmap_node *pPatNode, *pRepNode;` |
|     - |  986 | `		ph7_value sPat, sRep;` |
|     - |  987 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|     - |  988 | `		sxu32 n;` |
|    13 |  989 | `		sxi32 rc = SXRET_OK;` |
|    13 |  990 | `		if( pRepMap == 0 ){` |
|     5 |  991 | `			zScalarRepl = ph7_value_to_string(pRepl, &nScalarRepl);` |
|     2 |  992 | `		}` |
|    13 |  993 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|    13 |  994 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|    13 |  995 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|    13 |  996 | `		pSrc = &sA; pDst = &sB;` |
|    13 |  997 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|    13 |  998 | `		PH7_MemObjInit(pCtx->pVm, &sRep);` |
|    13 |  999 | `		pPatNode = pPatMap->pFirst;` |
|    13 | 1000 | `		pRepNode = pRepMap ? pRepMap->pFirst : 0;` |
|    13 | 1001 | `		n = pPatMap->nEntry;` |
|    33 | 1002 | `		while( n > 0 ){` |
|     - | 1003 | `			const char *zPattern, *zRepl;` |
|     - | 1004 | `			int nPatLen, nReplLen;` |
|     - | 1005 | `			pcre2_code *pCode;` |
|     - | 1006 | `			SyBlob *pSwap;` |
|    21 | 1007 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|    21 | 1008 | `			zPattern = ph7_value_to_string(&sPat, &nPatLen);` |
|    21 | 1009 | `			if( pRepMap ){` |
|    17 | 1010 | `				if( pRepNode ){` |
|    15 | 1011 | `					PH7_HashmapExtractNodeValue(pRepNode, &sRep, FALSE);` |
|    15 | 1012 | `					zRepl = ph7_value_to_string(&sRep, &nReplLen);` |
|     8 | 1013 | `				}else{` |
|     3 | 1014 | `					zRepl = ""; nReplLen = 0;` |
|     - | 1015 | `				}` |
|     9 | 1016 | `			}else{` |
|     5 | 1017 | `				zRepl = zScalarRepl; nReplLen = nScalarRepl;` |
|     - | 1018 | `			}` |
|    21 | 1019 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    21 | 1020 | `			if( pCode == 0 ){` |
|   ! 0 | 1021 | `				rc = SXERR_ABORT;` |
|   ! 0 | 1022 | `				PH7_MemObjRelease(&sPat);` |
|   ! 0 | 1023 | `				if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|   ! 0 | 1024 | `				break;` |
|     - | 1025 | `			}` |
|    21 | 1026 | `			SyBlobReset(pDst);` |
|    31 | 1027 | `			PcreDoReplace(pCtx, pCode,` |
|    20 | 1028 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|    10 | 1029 | `				zRepl, nReplLen, limit, pCount, pDst);` |
|     - | 1030 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|    21 | 1031 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|    21 | 1032 | `			PH7_MemObjRelease(&sPat);` |
|    21 | 1033 | `			if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|    21 | 1034 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|    21 | 1035 | `			if( pRepNode ){ pRepNode = pRepNode->pPrev; }` |
|    21 | 1036 | `			n--;` |
|     1 | 1037 | `		}` |
|    13 | 1038 | `		if( rc == SXRET_OK ){` |
|    13 | 1039 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|     6 | 1040 | `		}` |
|    13 | 1041 | `		SyBlobRelease(&sA);` |
|    13 | 1042 | `		SyBlobRelease(&sB);` |
|    13 | 1043 | `		return rc;` |
|     - | 1044 | `	}` |
|    27 | 1045 | `}` |
|     - | 1046 |  |
|     - | 1047 | `/* ======================================================================` |
|     - | 1048 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - | 1049 | ` * ====================================================================== */` |
|    36 | 1050 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1051 | `{` |
|    37 | 1052 | `	int limit = -1;` |
|    37 | 1053 | `	int count = 0;` |
|     - | 1054 |  |
|    37 | 1055 | `	if( nArg < 3 ){` |
|   ! 0 | 1056 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1057 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 | 1058 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1059 | `		return PH7_OK;` |
|     - | 1060 | `	}` |
|    37 | 1061 | `	if( nArg >= 4 ){` |
|     7 | 1062 | `		limit = ph7_value_to_int(apArg[3]);` |
|     3 | 1063 | `	}` |
|    37 | 1064 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1065 |  |
|     - | 1066 | `	/* A scalar pattern with an array replacement is a parameter mismatch (PHP` |
|     - | 1067 | `	 * throws a TypeError; PHL keeps preg_replace's warning-based arg-error style). */` |
|    37 | 1068 | `	if( !ph7_value_is_array(apArg[0]) && ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 1069 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1070 | `			"preg_replace(): Parameter mismatch, pattern is a string while replacement is an array");` |
|   ! 0 | 1071 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1072 | `		return PH7_OK;` |
|     - | 1073 | `	}` |
|    55 | 1074 | `	if( ph7_value_is_array(apArg[2]) ){` |
|     - | 1075 | `		/* Array subject: return an array, each element replaced, keys preserved. */` |
|    15 | 1076 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|    15 | 1077 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|    15 | 1078 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     - | 1079 | `		ph7_value sKey, sVal;` |
|     - | 1080 | `		ph7_hashmap_node *pNode;` |
|     - | 1081 | `		sxu32 n;` |
|    15 | 1082 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|   ! 0 | 1083 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1084 | `			return PH7_OK;` |
|     - | 1085 | `		}` |
|    15 | 1086 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|    15 | 1087 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    15 | 1088 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|    15 | 1089 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|    45 | 1090 | `		while( n > 0 ){` |
|     - | 1091 | `			const char *zSubject;` |
|     - | 1092 | `			int nSubLen;` |
|     - | 1093 | `			SyBlob sOut;` |
|    31 | 1094 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|    31 | 1095 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|    31 | 1096 | `			zSubject = ph7_value_to_string(&sVal, &nSubLen);` |
|    31 | 1097 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    31 | 1098 | `			if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1099 | `				/* A bad pattern with an array subject yields an empty array (PHP);` |
|     - | 1100 | `				 * the failure hits the first element, so pResult is still empty. */` |
|   ! 0 | 1101 | `				SyBlobRelease(&sOut);` |
|   ! 0 | 1102 | `				PH7_MemObjRelease(&sKey);` |
|   ! 0 | 1103 | `				PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1104 | `				ph7_result_value(pCtx, pResult);` |
|   ! 0 | 1105 | `				goto set_count;` |
|     - | 1106 | `			}` |
|    31 | 1107 | `			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    31 | 1108 | `			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|    31 | 1109 | `			ph7_value_reset_string_cursor(pElem);` |
|    31 | 1110 | `			SyBlobRelease(&sOut);` |
|    31 | 1111 | `			PH7_MemObjRelease(&sKey);` |
|    31 | 1112 | `			PH7_MemObjRelease(&sVal);` |
|    31 | 1113 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|    31 | 1114 | `			n--;` |
|     1 | 1115 | `		}` |
|    15 | 1116 | `		ph7_result_value(pCtx, pResult);` |
|     8 | 1117 | `	}else{` |
|     - | 1118 | `		/* Scalar subject: one replaced string. */` |
|     - | 1119 | `		const char *zSubject;` |
|     - | 1120 | `		int nSubLen;` |
|     - | 1121 | `		SyBlob sOut;` |
|    23 | 1122 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    23 | 1123 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    23 | 1124 | `		if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1125 | `			/* Scalar subject: a bad pattern returns NULL (PHP). */` |
|   ! 0 | 1126 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1127 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1128 | `			goto set_count;` |
|     - | 1129 | `		}` |
|    23 | 1130 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    23 | 1131 | `		SyBlobRelease(&sOut);` |
|     - | 1132 | `	}` |
|    18 | 1133 | `set_count:` |
|     - | 1134 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|     - | 1135 | `	 * (PHP always writes it: 0, or the count accumulated by earlier good patterns). */` |
|    37 | 1136 | `	if( nArg >= 5 ){` |
|     - | 1137 | `		ph7_value sCount;` |
|     7 | 1138 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     7 | 1139 | `		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);` |
|     7 | 1140 | `		PH7_MemObjRelease(&sCount);` |
|     3 | 1141 | `	}` |
|    37 | 1142 | `	return PH7_OK;` |
|    19 | 1143 | `}` |
|     - | 1144 |  |
|     - | 1145 | `/* ======================================================================` |
|     - | 1146 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count]])` |
|     - | 1147 | ` * ====================================================================== */` |
|     8 | 1148 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1149 | `{` |
|     - | 1150 | `	const char *zPattern, *zSubject;` |
|     - | 1151 | `	int nPatLen, nSubLen;` |
|     - | 1152 | `	pcre2_code *pCode;` |
|     - | 1153 | `	pcre2_match_data *pMatchData;` |
|     - | 1154 | `	sxu32 nCapture;` |
|     - | 1155 | `	SyBlob sOut;` |
|    10 | 1156 | `	PCRE2_SIZE startOffset = 0;` |
|    10 | 1157 | `	int limit = -1;` |
|    10 | 1158 | `	int count = 0;` |
|     - | 1159 | `	int rc;` |
|     - | 1160 |  |
|    10 | 1161 | `	if( nArg < 3 ){` |
|   ! 0 | 1162 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1163 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 | 1164 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1165 | `		return PH7_OK;` |
|     - | 1166 | `	}` |
|    10 | 1167 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    10 | 1168 | `	zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    10 | 1169 | `	if( nArg >= 4 ){` |
|     3 | 1170 | `		limit = ph7_value_to_int(apArg[3]);` |
|     1 | 1171 | `	}` |
|    10 | 1172 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 | 1173 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1174 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 | 1175 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1176 | `		return PH7_OK;` |
|     - | 1177 | `	}` |
|    10 | 1178 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    10 | 1179 | `	if( pCode == 0 ){` |
|   ! 0 | 1180 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1181 | `		return PH7_OK;` |
|     - | 1182 | `	}` |
|    10 | 1183 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    10 | 1184 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1185 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1186 | `		return PH7_OK;` |
|     - | 1187 | `	}` |
|    10 | 1188 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    10 | 1189 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1190 |  |
|    28 | 1191 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1192 | `		PCRE2_SIZE *ovector;` |
|     - | 1193 | `		ph7_value *pMatchArr;` |
|     - | 1194 | `		ph7_value *apCbArg[1];` |
|     - | 1195 | `		ph7_value sResult;` |
|     - | 1196 | `		const char *zReplacement;` |
|     - | 1197 | `		int nReplLen;` |
|     - | 1198 |  |
|    32 | 1199 | `		if( limit >= 0 && count >= limit ) break;` |
|    41 | 1200 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    13 | 1201 | `			startOffset, 0, pMatchData, NULL);` |
|    28 | 1202 | `		if( rc < 0 ){` |
|    10 | 1203 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1204 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1205 | `			}` |
|    10 | 1206 | `			break;` |
|     - | 1207 | `		}` |
|    20 | 1208 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1209 | `		/* Copy text before match */` |
|    20 | 1210 | `		if( ovector[0] > startOffset ){` |
|    14 | 1211 | `			SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     6 | 1212 | `		}` |
|     - | 1213 | `		/* Build matches array for callback */` |
|    20 | 1214 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    20 | 1215 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, 0);` |
|     - | 1216 | `		/* Call the callback */` |
|    20 | 1217 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    20 | 1218 | `		apCbArg[0] = pMatchArr;` |
|    20 | 1219 | `		if( PH7_VmCallUserFunction(pCtx->pVm, apArg[1], 1, apCbArg, &sResult) == PH7_EXCEPTION ){` |
|     - | 1220 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|   ! 0 | 1221 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1222 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|   ! 0 | 1223 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1224 | `			pcre2_match_data_free(pMatchData);` |
|   ! 0 | 1225 | `			return PH7_EXCEPTION;` |
|     - | 1226 | `		}` |
|     - | 1227 | `		/* Get replacement string from callback result */` |
|    20 | 1228 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    20 | 1229 | `		SyBlobAppend(&sOut, zReplacement, (sxu32)nReplLen);` |
|    20 | 1230 | `		PH7_MemObjRelease(&sResult);` |
|    20 | 1231 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    20 | 1232 | `		count++;` |
|     - | 1233 | `		/* Advance */` |
|    20 | 1234 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 | 1235 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1236 | `				SyBlobAppend(&sOut, &zSubject[startOffset], 1);` |
|   ! 0 | 1237 | `			}` |
|   ! 0 | 1238 | `			startOffset = ovector[0] + 1;` |
|   ! 0 | 1239 | `		}else{` |
|    20 | 1240 | `			startOffset = ovector[1];` |
|     - | 1241 | `		}` |
|     2 | 1242 | `	}` |
|     - | 1243 | `	/* Copy remainder */` |
|    10 | 1244 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1245 | `		SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   ! 0 | 1246 | `	}` |
|    10 | 1247 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    10 | 1248 | `	SyBlobRelease(&sOut);` |
|    10 | 1249 | `	pcre2_match_data_free(pMatchData);` |
|     - | 1250 | `	/* Set &$count if provided */` |
|    10 | 1251 | `	if( nArg >= 5 ){` |
|     - | 1252 | `		ph7_value sCount;` |
|     3 | 1253 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     3 | 1254 | `		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);` |
|     3 | 1255 | `		PH7_MemObjRelease(&sCount);` |
|     1 | 1256 | `	}` |
|    10 | 1257 | `	return PH7_OK;` |
|     6 | 1258 | `}` |
|     - | 1259 |  |
|     - | 1260 | `/* ======================================================================` |
|     - | 1261 | ` * preg_quote(str [, delimiter])` |
|     - | 1262 | ` * ====================================================================== */` |
|     6 | 1263 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1264 | `{` |
|     7 | 1265 | `	const char *zStr, *zDelim = 0;` |
|     7 | 1266 | `	int nLen, nDelimLen = 0;` |
|     - | 1267 | `	const char *z, *zEnd;` |
|     - | 1268 |  |
|     7 | 1269 | `	if( nArg < 1 ){` |
|   ! 0 | 1270 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1271 | `		return PH7_OK;` |
|     - | 1272 | `	}` |
|     7 | 1273 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|     7 | 1274 | `	if( nArg >= 2 ){` |
|     3 | 1275 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     1 | 1276 | `	}` |
|     7 | 1277 | `	z = zStr;` |
|     7 | 1278 | `	zEnd = &zStr[nLen];` |
|    71 | 1279 | `	while( z < zEnd ){` |
|    65 | 1280 | `		char c = *z;` |
|    65 | 1281 | `		switch( c ){` |
|     4 | 1282 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1283 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1284 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1285 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1286 | `			case '#':` |
|     9 | 1287 | `				ph7_result_string(pCtx, "\\", 1);` |
|     9 | 1288 | `				break;` |
|    28 | 1289 | `			default:` |
|    57 | 1290 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     3 | 1291 | `					ph7_result_string(pCtx, "\\", 1);` |
|     1 | 1292 | `				}` |
|    56 | 1293 | `				break;` |
|     - | 1294 | `		}` |
|    65 | 1295 | `		ph7_result_string(pCtx, z, 1);` |
|    65 | 1296 | `		z++;` |
|     1 | 1297 | `	}` |
|     7 | 1298 | `	return PH7_OK;` |
|     4 | 1299 | `}` |
|     - | 1300 |  |
|     - | 1301 | `/* ======================================================================` |
|     - | 1302 | ` * preg_last_error()` |
|     - | 1303 | ` * ====================================================================== */` |
|   ! 0 | 1304 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1305 | `{` |
|   ! 0 | 1306 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1307 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1308 | `	return PH7_OK;` |
|   ! 0 | 1309 | `}` |
|     - | 1310 |  |
|     - | 1311 | `/* ======================================================================` |
|     - | 1312 | ` * preg_last_error_msg()` |
|     - | 1313 | ` * ====================================================================== */` |
|   ! 0 | 1314 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1315 | `{` |
|     - | 1316 | `	const char *zMsg;` |
|   ! 0 | 1317 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1318 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1319 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1320 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1321 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1322 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1323 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1324 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1325 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1326 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1327 | `	}` |
|   ! 0 | 1328 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1329 | `	return PH7_OK;` |
|   ! 0 | 1330 | `}` |
|     - | 1331 |  |
|     - | 1332 | `/* ===== Function registration table ===== */` |
|     - | 1333 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1334 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1335 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1336 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1337 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1338 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1339 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1340 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1341 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1342 | `};` |
|     - | 1343 |  |
|  3280 | 1344 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     5 | 1345 | `{` |
|     - | 1346 | `	sxu32 n;` |
| 29525 | 1347 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 26245 | 1348 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 13125 | 1349 | `	}` |
|  3285 | 1350 | `}` |
|     - | 1351 |  |
|     - | 1352 | `/* ===== Constant registration ===== */` |
|     - | 1353 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1354 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1355 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1356 | `	}` |
|     - | 1357 |  |
|   ! 0 | 1358 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|     3 | 1359 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|   ! 0 | 1360 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|   ! 0 | 1361 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|   ! 0 | 1362 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|   ! 0 | 1363 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|   ! 0 | 1364 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|   ! 0 | 1365 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|   ! 0 | 1366 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|   ! 0 | 1367 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|   ! 0 | 1368 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|   ! 0 | 1369 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|   ! 0 | 1370 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|   ! 0 | 1371 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1372 |  |
|  3280 | 1373 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     5 | 1374 | `{` |
|  3285 | 1375 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  3285 | 1376 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  3285 | 1377 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  3285 | 1378 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  3285 | 1379 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  3285 | 1380 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  3285 | 1381 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  3285 | 1382 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  3285 | 1383 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  3285 | 1384 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  3285 | 1385 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  3285 | 1386 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  3285 | 1387 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  3285 | 1388 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  3285 | 1389 | `}` |
|     - | 1390 |  |
|     - | 1391 | `#else` |
|     - | 1392 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1393 | `typedef int vm_pcre_unused;` |
|     - | 1394 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1395 |  |
