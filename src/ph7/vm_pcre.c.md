# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 487/814 lines (59.83%)

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
|    28 |   55 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|     1 |   56 |  |
|     - |   57 | `	sxu32 i;` |
|   171 |   58 | `	for( i = 0; i < nCacheUsed; i++ ){` |
|   149 |   59 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|     7 |   60 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|     7 |   61 | `			if( pCaptureCount ){` |
|     7 |   62 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|     3 |   63 | `			}` |
|     7 |   64 | `			return aCache[i].pCode;` |
|     - |   65 | `		}` |
|    72 |   66 | `	}` |
|    23 |   67 | `	return 0;` |
|    15 |   68 |  |
|     - |   69 |  |
|    22 |   70 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|     1 |   71 |  |
|     - |   72 | `	PcreCacheEntry *pEntry;` |
|     - |   73 | `	char *zCopy;` |
|     - |   74 | `	/* Allocate the pattern copy first, before touching the cache */` |
|    23 |   75 | `	zCopy = (char *)malloc(nLen + 1);` |
|    23 |   76 | `	if( zCopy == 0 ){` |
|     - |   77 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|   ! 0 |   78 | `		return;` |
|     - |   79 | `	}` |
|    23 |   80 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|    23 |   81 | `	zCopy[nLen] = 0;` |
|    23 |   82 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|    23 |   83 | `		pEntry = &aCache[nCacheUsed++];` |
|    12 |   84 | `	}else{` |
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
|    23 |   99 | `	pEntry->zPattern = zCopy;` |
|    23 |  100 | `	pEntry->nLen = nLen;` |
|    23 |  101 | `	pEntry->pCode = pCode;` |
|    23 |  102 | `	pEntry->nCaptureCount = nCaptureCount;` |
|    23 |  103 | `	pEntry->iLastUsed = ++iCacheClock;` |
|    12 |  104 |  |
|     - |  105 |  |
|     - |  106 | `/* ===== Delimiter parser ===== */` |
|     - |  107 | `#define PCRE_PARSE_OK             0` |
|     - |  108 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|     - |  109 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */` |
|     - |  110 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|     - |  111 |  |
|    22 |  112 | `static sxi32 PcreParsePattern(` |
|     - |  113 | `	const char *zInput, int nInputLen,` |
|     - |  114 | `	const char **pPattern, int *pnPatternLen,` |
|     - |  115 | `	const char **pFlags, int *pnFlagLen)` |
|     1 |  116 |  |
|    23 |  117 | `	const char *zEnd = &zInput[nInputLen];` |
|    23 |  118 | `	const char *z = zInput;` |
|     - |  119 | `	char cOpen, cClose;` |
|     - |  120 | `	const char *pStart;` |
|     - |  121 |  |
|     - |  122 | `	/* Skip leading whitespace */` |
|    23 |  123 | `	while( z < zEnd && (unsigned char)*z <= 0x20 ){` |
|   ! 0 |  124 | `		z++;` |
|   ! 0 |  125 | `	}` |
|    23 |  126 | `	if( z >= zEnd ){` |
|   ! 0 |  127 | `		return PCRE_PARSE_EMPTY;` |
|     - |  128 | `	}` |
|    23 |  129 | `	cOpen = *z;` |
|     - |  130 | `	/* Must not be alphanumeric, backslash, or whitespace */` |
|    23 |  131 | `	if( SyisAlphaNum(cOpen) \|\| cOpen == '\\' \|\| (unsigned char)cOpen <= 0x20 ){` |
|   ! 0 |  132 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|     - |  133 | `	}` |
|     - |  134 | `	/* Paired delimiters */` |
|    23 |  135 | `	switch( cOpen ){` |
|   ! 0 |  136 | `		case '(': cClose = ')'; break;` |
|   ! 0 |  137 | `		case '[': cClose = ']'; break;` |
|   ! 0 |  138 | `		case '{': cClose = '}'; break;` |
|   ! 0 |  139 | `		case '<': cClose = '>'; break;` |
|    23 |  140 | `		default:  cClose = cOpen; break;` |
|     - |  141 | `	}` |
|    23 |  142 | `	z++; /* Skip opening delimiter */` |
|    23 |  143 | `	pStart = z;` |
|     - |  144 | `	/* Scan for closing delimiter, respecting backslash escapes */` |
|   189 |  145 | `	while( z < zEnd ){` |
|   189 |  146 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|    23 |  147 | `			z += 2; /* Skip escaped char */` |
|    23 |  148 | `			continue;` |
|     - |  149 | `		}` |
|   167 |  150 | `		if( *z == cClose ){` |
|    23 |  151 | `			break;` |
|     - |  152 | `		}` |
|   145 |  153 | `		z++;` |
|     1 |  154 | `	}` |
|    23 |  155 | `	if( z >= zEnd ){` |
|   ! 0 |  156 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|     - |  157 | `	}` |
|    23 |  158 | `	*pPattern = pStart;` |
|    23 |  159 | `	*pnPatternLen = (int)(z - pStart);` |
|    23 |  160 | `	z++; /* Skip closing delimiter */` |
|    23 |  161 | `	*pFlags = z;` |
|    23 |  162 | `	*pnFlagLen = (int)(zEnd - z);` |
|    23 |  163 | `	return PH7_OK;` |
|    12 |  164 |  |
|     - |  165 |  |
|     - |  166 | `/* ===== Flag mapper ===== */` |
|    22 |  167 | `static sxi32 PcreMapFlags(` |
|     - |  168 | `	const char *zFlags, int nFlagLen,` |
|     - |  169 | `	uint32_t *pCompileOpts)` |
|     1 |  170 |  |
|     - |  171 | `	int i;` |
|    23 |  172 | `	*pCompileOpts = 0;` |
|    35 |  173 | `	for( i = 0; i < nFlagLen; i++ ){` |
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
|    23 |  188 | `	return PH7_OK;` |
|     1 |  189 |  |
|     - |  190 |  |
|     - |  191 | `/* ===== Compile helper ===== */` |
|    28 |  192 | `static pcre2_code *PcreCompile(` |
|     - |  193 | `	ph7_context *pCtx,` |
|     - |  194 | `	const char *zFullPattern, int nLen,` |
|     - |  195 | `	sxu32 *pCaptureCount)` |
|     1 |  196 |  |
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
|    29 |  207 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|    29 |  208 | `	if( pCode ){` |
|     7 |  209 | `		return pCode;` |
|     - |  210 | `	}` |
|     - |  211 | `	/* Parse delimiter */` |
|    23 |  212 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen);` |
|    23 |  213 | `	if( parseRc != PCRE_PARSE_OK ){` |
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
|    23 |  225 | `	PcreMapFlags(zFlags, nFlagLen, &compileOpts);` |
|     - |  226 | `	/* Compile */` |
|    23 |  227 | `	pCode = pcre2_compile(` |
|    11 |  228 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|    11 |  229 | `		compileOpts, &errcode, &erroffset, NULL);` |
|    23 |  230 | `	if( pCode == 0 ){` |
|     - |  231 | `		PCRE2_UCHAR errbuf[256];` |
|   ! 0 |  232 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|   ! 0 |  233 | `		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,` |
|   ! 0 |  234 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|   ! 0 |  235 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|   ! 0 |  236 | `		return 0;` |
|     - |  237 | `	}` |
|     - |  238 | `	/* Get capture count */` |
|    23 |  239 | `	nCapture = 0;` |
|    23 |  240 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|    23 |  241 | `	if( pCaptureCount ){` |
|    23 |  242 | `		*pCaptureCount = nCapture;` |
|    11 |  243 | `	}` |
|     - |  244 | `	/* Cache it */` |
|    23 |  245 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|    23 |  246 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    23 |  247 | `	return pCode;` |
|    15 |  248 |  |
|     - |  249 |  |
|     - |  250 | `/*` |
|     - |  251 | ` * Write a value back to the caller's variable through the stack slot's nIdx.` |
|     - |  252 | ` *` |
|     - |  253 | ` * PHL limitation: if the caller passes an undefined variable (e.g. bare $m` |
|     - |  254 | ` * without prior assignment), nIdx is SXU32_HIGH and the write-back is a` |
|     - |  255 | ` * no-op — the caller's variable stays null.  Workaround: initialize the` |
|     - |  256 | ` * variable before the call ($m = null;).  PHP does not have this limitation` |
|     - |  257 | ` * because it creates the variable via the & reference mechanism.` |
|     - |  258 | ` */` |
|    10 |  259 | `static void PcreStoreByRef(ph7_vm *pVm, ph7_value *pArg, ph7_value *pNewVal)` |
|     1 |  260 |  |
|    11 |  261 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|    11 |  262 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj, pArg->nIdx);` |
|    11 |  263 | `		if( pObj ){` |
|    11 |  264 | `			PH7_MemObjStore(pNewVal, pObj);` |
|     5 |  265 | `		}` |
|     5 |  266 | `	}` |
|    11 |  267 | `	PH7_MemObjStore(pNewVal, pArg);` |
|    11 |  268 |  |
|     - |  269 |  |
|     - |  270 | `/* ===== Map PCRE2 match error to PHP error code ===== */` |
|   ! 0 |  271 | `static void PcreSetMatchError(ph7_vm *pVm, int rc)` |
|   ! 0 |  272 |  |
|   ! 0 |  273 | `	if( rc == PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  274 | `		pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   ! 0 |  275 | `	}else if( rc == PCRE2_ERROR_MATCHLIMIT ){` |
|   ! 0 |  276 | `		pVm->iPcreLastError = PHP_PREG_BACKTRACK_LIMIT_ERROR;` |
|   ! 0 |  277 | `	}else if( rc == PCRE2_ERROR_DEPTHLIMIT` |
|     - |  278 | `#ifdef PCRE2_ERROR_RECURSIONLIMIT` |
|   ! 0 |  279 | `		\|\| rc == PCRE2_ERROR_RECURSIONLIMIT` |
|     - |  280 | `#endif` |
|     - |  281 | `	){` |
|   ! 0 |  282 | `		pVm->iPcreLastError = PHP_PREG_RECURSION_LIMIT_ERROR;` |
|   ! 0 |  283 | `	}else if( rc == PCRE2_ERROR_BADUTFOFFSET ){` |
|   ! 0 |  284 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_OFFSET_ERROR;` |
|   ! 0 |  285 | `	}else if( rc == PCRE2_ERROR_UTF8_ERR1` |
|   ! 0 |  286 | `		\|\| rc == PCRE2_ERROR_UTF8_ERR2 ){` |
|   ! 0 |  287 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_ERROR;` |
|     - |  288 | `#ifdef PCRE2_ERROR_JIT_STACKLIMIT` |
|   ! 0 |  289 | `	}else if( rc == PCRE2_ERROR_JIT_STACKLIMIT ){` |
|   ! 0 |  290 | `		pVm->iPcreLastError = PHP_PREG_JIT_STACKLIMIT_ERROR;` |
|     - |  291 | `#endif` |
|   ! 0 |  292 | `	}else{` |
|   ! 0 |  293 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|     - |  294 | `	}` |
|   ! 0 |  295 |  |
|     - |  296 |  |
|     - |  297 | `/* ===== Helper: populate matches array from ovector ===== */` |
|    12 |  298 | `static void PcrePopulateMatches(` |
|     - |  299 | `	ph7_context *pCtx,` |
|     - |  300 | `	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */` |
|     - |  301 | `	const char *zSubject,` |
|     - |  302 | `	PCRE2_SIZE *ovector,` |
|     - |  303 | `	int nGroups,` |
|     - |  304 | `	pcre2_code *pCode,` |
|     - |  305 | `	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */` |
|     1 |  306 |  |
|    13 |  307 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    13 |  308 | `	ph7_value *pSub = 0;` |
|    13 |  309 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|    13 |  310 | `	PCRE2_SPTR nametable = 0;` |
|     - |  311 | `	int i;` |
|     - |  312 |  |
|    13 |  313 | `	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  314 | `		pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  315 | `	}` |
|    41 |  316 | `	for( i = 0; i < nGroups; i++ ){` |
|    29 |  317 | `		PCRE2_SIZE start = ovector[2 * i];` |
|    29 |  318 | `		PCRE2_SIZE end   = ovector[2 * i + 1];` |
|    29 |  319 | `		if( start == PCRE2_UNSET ){` |
|   ! 0 |  320 | `			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  321 | `				ph7_value_null(pVal);` |
|   ! 0 |  322 | `			}else{` |
|   ! 0 |  323 | `				ph7_value_string(pVal, "", 0);` |
|     - |  324 | `			}` |
|   ! 0 |  325 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  326 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  327 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  328 | `				ph7_value_int(pOff, -1);` |
|   ! 0 |  329 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  330 | `				ph7_array_add_intkey_elem(pArray, i, pSub);` |
|     - |  331 | `				/* Reset sub-array for reuse */` |
|   ! 0 |  332 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  333 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  334 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  335 | `			}else{` |
|   ! 0 |  336 | `				ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  337 | `			}` |
|   ! 0 |  338 | `		}else{` |
|    29 |  339 | `			ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|    29 |  340 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  341 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  342 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  343 | `				ph7_value_int(pOff, (int)start);` |
|   ! 0 |  344 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  345 | `				ph7_array_add_intkey_elem(pArray, i, pSub);` |
|   ! 0 |  346 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  347 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  348 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  349 | `			}else{` |
|    29 |  350 | `				ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  351 | `			}` |
|     - |  352 | `		}` |
|    29 |  353 | `		ph7_value_reset_string_cursor(pVal);` |
|    15 |  354 | `	}` |
|     - |  355 | `	/* Named groups */` |
|    13 |  356 | `	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|    13 |  357 | `	if( namecount > 0 ){` |
|     3 |  358 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     3 |  359 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|     7 |  360 | `		for( i = 0; (uint32_t)i < namecount; i++ ){` |
|     5 |  361 | `			PCRE2_SPTR entry = nametable + i * nameentrysize;` |
|     5 |  362 | `			int groupNum = (entry[0] << 8) \| entry[1];` |
|     5 |  363 | `			const char *zName = (const char *)(entry + 2);` |
|     - |  364 | `			PCRE2_SIZE start, end;` |
|     5 |  365 | `			if( groupNum >= nGroups ) continue;` |
|     5 |  366 | `			start = ovector[2 * groupNum];` |
|     5 |  367 | `			end   = ovector[2 * groupNum + 1];` |
|     5 |  368 | `			if( start == PCRE2_UNSET ){` |
|   ! 0 |  369 | `				if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  370 | `					ph7_value_null(pVal);` |
|   ! 0 |  371 | `				}else{` |
|   ! 0 |  372 | `					ph7_value_string(pVal, "", 0);` |
|     - |  373 | `				}` |
|   ! 0 |  374 | `			}else{` |
|     5 |  375 | `				ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|     - |  376 | `			}` |
|     5 |  377 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  378 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  379 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  380 | `				ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);` |
|   ! 0 |  381 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  382 | `				ph7_array_add_strkey_elem(pArray, zName, pSub);` |
|   ! 0 |  383 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  384 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  385 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  386 | `			}else{` |
|     5 |  387 | `				ph7_array_add_strkey_elem(pArray, zName, pVal);` |
|     - |  388 | `			}` |
|     5 |  389 | `			ph7_value_reset_string_cursor(pVal);` |
|     3 |  390 | `		}` |
|     1 |  391 | `	}` |
|    13 |  392 | `	ph7_context_release_value(pCtx, pVal);` |
|    13 |  393 | `	if( pSub ){` |
|   ! 0 |  394 | `		ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  395 | `	}` |
|    13 |  396 |  |
|     - |  397 |  |
|     - |  398 | `/* ======================================================================` |
|     - |  399 | ` * preg_match(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  400 | ` * ====================================================================== */` |
|    12 |  401 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  402 |  |
|     - |  403 | `	const char *zPattern, *zSubject;` |
|     - |  404 | `	int nPatLen, nSubLen;` |
|     - |  405 | `	pcre2_code *pCode;` |
|     - |  406 | `	pcre2_match_data *pMatchData;` |
|     - |  407 | `	PCRE2_SIZE *ovector;` |
|     - |  408 | `	sxu32 nCapture;` |
|    13 |  409 | `	PCRE2_SIZE startOffset = 0;` |
|    13 |  410 | `	int iFlags = 0;` |
|     - |  411 | `	int rc;` |
|     - |  412 |  |
|    13 |  413 | `	if( nArg < 2 ){` |
|   ! 0 |  414 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  415 | `			"preg_match() expects at least 2 parameters");` |
|   ! 0 |  416 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  417 | `		return PH7_OK;` |
|     - |  418 | `	}` |
|    13 |  419 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    13 |  420 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    13 |  421 | `	if( nArg >= 4 ){` |
|   ! 0 |  422 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  423 | `	}` |
|    13 |  424 | `	if( nArg >= 5 ){` |
|   ! 0 |  425 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  426 | `	}` |
|    13 |  427 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    13 |  428 | `	if( pCode == 0 ){` |
|   ! 0 |  429 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  430 | `		return PH7_OK;` |
|     - |  431 | `	}` |
|    13 |  432 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    13 |  433 | `	if( pMatchData == 0 ){` |
|   ! 0 |  434 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  435 | `		return PH7_OK;` |
|     - |  436 | `	}` |
|    19 |  437 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     6 |  438 | `		startOffset, 0, pMatchData, NULL);` |
|    13 |  439 | `	if( rc < 0 ){` |
|     3 |  440 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  441 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  442 | `		}` |
|     - |  443 | `		/* Populate empty matches if requested */` |
|     3 |  444 | `		if( nArg >= 3 ){` |
|     3 |  445 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|     3 |  446 | `			PcreStoreByRef(pCtx->pVm, apArg[2], pEmpty);` |
|     3 |  447 | `			ph7_context_release_value(pCtx, pEmpty);` |
|     1 |  448 | `		}` |
|     3 |  449 | `		pcre2_match_data_free(pMatchData);` |
|     3 |  450 | `		ph7_result_int(pCtx, 0);` |
|     3 |  451 | `		return PH7_OK;` |
|     - |  452 | `	}` |
|    11 |  453 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    11 |  454 | `	if( nArg >= 3 ){` |
|     - |  455 | `		/* Populate $matches */` |
|     5 |  456 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|     5 |  457 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     5 |  458 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);` |
|     - |  459 | `		/* Write the array back to the caller's variable */` |
|     5 |  460 | `		PcreStoreByRef(pCtx->pVm, apArg[2], pArray);` |
|     5 |  461 | `		ph7_context_release_value(pCtx, pArray);` |
|     2 |  462 | `	}` |
|    11 |  463 | `	pcre2_match_data_free(pMatchData);` |
|    11 |  464 | `	ph7_result_int(pCtx, 1);` |
|    11 |  465 | `	return PH7_OK;` |
|     7 |  466 |  |
|     - |  467 |  |
|     - |  468 | `/* ======================================================================` |
|     - |  469 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  470 | ` * ====================================================================== */` |
|     4 |  471 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  472 |  |
|     - |  473 | `	const char *zPattern, *zSubject;` |
|     - |  474 | `	int nPatLen, nSubLen;` |
|     - |  475 | `	pcre2_code *pCode;` |
|     - |  476 | `	pcre2_match_data *pMatchData;` |
|     - |  477 | `	sxu32 nCapture;` |
|     5 |  478 | `	PCRE2_SIZE startOffset = 0;` |
|     5 |  479 | `	int iFlags = PHP_PREG_PATTERN_ORDER;` |
|     5 |  480 | `	int totalMatches = 0;` |
|     - |  481 | `	int rc;` |
|     - |  482 |  |
|     5 |  483 | `	if( nArg < 2 ){` |
|   ! 0 |  484 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  485 | `			"preg_match_all() expects at least 2 parameters");` |
|   ! 0 |  486 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  487 | `		return PH7_OK;` |
|     - |  488 | `	}` |
|     5 |  489 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     5 |  490 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     5 |  491 | `	if( nArg >= 4 ){` |
|     3 |  492 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     1 |  493 | `	}` |
|     5 |  494 | `	if( nArg >= 5 ){` |
|   ! 0 |  495 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  496 | `	}` |
|     5 |  497 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     5 |  498 | `	if( pCode == 0 ){` |
|   ! 0 |  499 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  500 | `		return PH7_OK;` |
|     - |  501 | `	}` |
|     5 |  502 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     5 |  503 | `	if( pMatchData == 0 ){` |
|   ! 0 |  504 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  505 | `		return PH7_OK;` |
|     - |  506 | `	}` |
|     5 |  507 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  508 | `	{` |
|     5 |  509 | `		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;` |
|     - |  510 |  |
|     5 |  511 | `		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){` |
|     7 |  512 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  513 | `				PCRE2_SIZE *ovector;` |
|    10 |  514 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     3 |  515 | `					startOffset, 0, pMatchData, NULL);` |
|     7 |  516 | `				if( rc < 0 ){` |
|     3 |  517 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     3 |  518 | `					break;` |
|     - |  519 | `				}` |
|     5 |  520 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     5 |  521 | `				if( pOutArray ){` |
|     5 |  522 | `					ph7_value *pSet = ph7_context_new_array(pCtx);` |
|     5 |  523 | `					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, iFlags & ~0xFF);` |
|     5 |  524 | `					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);` |
|     5 |  525 | `					ph7_context_release_value(pCtx, pSet);` |
|     2 |  526 | `				}` |
|     5 |  527 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  528 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  529 | `				}else{` |
|     5 |  530 | `					startOffset = ovector[1];` |
|     - |  531 | `				}` |
|     5 |  532 | `				totalMatches++;` |
|     1 |  533 | `			}` |
|     2 |  534 | `		}else{` |
|     - |  535 | `			/* PREG_PATTERN_ORDER (default) */` |
|     3 |  536 | `			ph7_value **apGroupArrays = 0;` |
|     3 |  537 | `			sxu32 nGroups = nCapture + 1;` |
|     - |  538 | `			sxu32 g;` |
|     3 |  539 | `			if( pOutArray ){` |
|     4 |  540 | `				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,` |
|     1 |  541 | `					sizeof(ph7_value *) * nGroups, TRUE, FALSE);` |
|     3 |  542 | `				if( apGroupArrays ){` |
|     9 |  543 | `					for( g = 0; g < nGroups; g++ ){` |
|     7 |  544 | `						apGroupArrays[g] = ph7_context_new_array(pCtx);` |
|     4 |  545 | `					}` |
|     1 |  546 | `				}` |
|     1 |  547 | `			}` |
|     7 |  548 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  549 | `				PCRE2_SIZE *ovector;` |
|    10 |  550 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     3 |  551 | `					startOffset, 0, pMatchData, NULL);` |
|     7 |  552 | `				if( rc < 0 ){` |
|     3 |  553 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     3 |  554 | `					break;` |
|     - |  555 | `				}` |
|     5 |  556 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     5 |  557 | `				if( apGroupArrays ){` |
|     5 |  558 | `					ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|     5 |  559 | `					int nActual = rc;` |
|    17 |  560 | `					for( g = 0; g < nGroups; g++ ){` |
|    19 |  561 | `						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){` |
|    13 |  562 | `							PCRE2_SIZE s = ovector[2*g];` |
|    13 |  563 | `							PCRE2_SIZE e = ovector[2*g+1];` |
|    13 |  564 | `							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  565 | `								ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  566 | `								ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  567 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|   ! 0 |  568 | `								ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  569 | `								ph7_value_int(pOff, (int)s);` |
|   ! 0 |  570 | `								ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  571 | `								ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|   ! 0 |  572 | `								ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  573 | `								ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  574 | `							}else{` |
|    13 |  575 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|    13 |  576 | `								ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  577 | `							}` |
|     7 |  578 | `						}else{` |
|   ! 0 |  579 | `							ph7_value_string(pVal, "", 0);` |
|   ! 0 |  580 | `							ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  581 | `						}` |
|    13 |  582 | `						ph7_value_reset_string_cursor(pVal);` |
|     7 |  583 | `					}` |
|     5 |  584 | `					ph7_context_release_value(pCtx, pVal);` |
|     2 |  585 | `				}` |
|     5 |  586 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  587 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  588 | `				}else{` |
|     5 |  589 | `					startOffset = ovector[1];` |
|     - |  590 | `				}` |
|     5 |  591 | `				totalMatches++;` |
|     1 |  592 | `			}` |
|     3 |  593 | `			if( apGroupArrays ){` |
|     9 |  594 | `				for( g = 0; g < nGroups; g++ ){` |
|     7 |  595 | `					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);` |
|     7 |  596 | `					ph7_context_release_value(pCtx, apGroupArrays[g]);` |
|     4 |  597 | `				}` |
|     3 |  598 | `				ph7_context_free_chunk(pCtx, apGroupArrays);` |
|     1 |  599 | `			}` |
|     - |  600 | `		}` |
|     - |  601 | `		/* Write output array to caller's variable */` |
|     5 |  602 | `		if( pOutArray && nArg >= 3 ){` |
|     5 |  603 | `			PcreStoreByRef(pCtx->pVm, apArg[2], pOutArray);` |
|     5 |  604 | `			ph7_context_release_value(pCtx, pOutArray);` |
|     2 |  605 | `		}` |
|     - |  606 | `	}` |
|     5 |  607 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  608 | `	ph7_result_int(pCtx, totalMatches);` |
|     5 |  609 | `	return PH7_OK;` |
|     3 |  610 |  |
|     - |  611 |  |
|     - |  612 | `/* ======================================================================` |
|     - |  613 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|     - |  614 | ` * ====================================================================== */` |
|     4 |  615 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  616 |  |
|     - |  617 | `	const char *zPattern, *zSubject;` |
|     - |  618 | `	int nPatLen, nSubLen;` |
|     - |  619 | `	pcre2_code *pCode;` |
|     - |  620 | `	pcre2_match_data *pMatchData;` |
|     - |  621 | `	sxu32 nCapture;` |
|     - |  622 | `	ph7_value *pArray;` |
|     - |  623 | `	ph7_value *pVal;` |
|     5 |  624 | `	PCRE2_SIZE startOffset = 0, lastOffset = 0;` |
|     5 |  625 | `	int limit = -1;` |
|     5 |  626 | `	int iFlags = 0;` |
|     5 |  627 | `	int nPieces = 0;` |
|     - |  628 | `	int rc;` |
|     - |  629 |  |
|     5 |  630 | `	if( nArg < 2 ){` |
|   ! 0 |  631 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  632 | `			"preg_split() expects at least 2 parameters");` |
|   ! 0 |  633 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  634 | `		return PH7_OK;` |
|     - |  635 | `	}` |
|     5 |  636 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     5 |  637 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     5 |  638 | `	if( nArg >= 3 ){` |
|     3 |  639 | `		limit = ph7_value_to_int(apArg[2]);` |
|     1 |  640 | `	}` |
|     5 |  641 | `	if( nArg >= 4 ){` |
|   ! 0 |  642 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  643 | `	}` |
|     5 |  644 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     5 |  645 | `	if( pCode == 0 ){` |
|   ! 0 |  646 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  647 | `		return PH7_OK;` |
|     - |  648 | `	}` |
|     5 |  649 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     5 |  650 | `	if( pMatchData == 0 ){` |
|   ! 0 |  651 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  652 | `		return PH7_OK;` |
|     - |  653 | `	}` |
|     5 |  654 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  655 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     5 |  656 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  657 |  |
|    13 |  658 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|    13 |  659 | `		if( limit > 0 && nPieces >= limit - 1 ){` |
|     3 |  660 | `			break; /* Last piece gets the remainder */` |
|     - |  661 | `		}` |
|    16 |  662 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     5 |  663 | `			startOffset, 0, pMatchData, NULL);` |
|    11 |  664 | `		if( rc < 0 ){` |
|     3 |  665 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  666 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  667 | `			}` |
|     3 |  668 | `			break;` |
|     - |  669 | `		}` |
|     - |  670 | `		{` |
|     9 |  671 | `			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     9 |  672 | `			PCRE2_SIZE matchStart = ovector[0];` |
|     9 |  673 | `			PCRE2_SIZE matchEnd = ovector[1];` |
|     9 |  674 | `			int pieceLen = (int)(matchStart - lastOffset);` |
|     - |  675 |  |
|     - |  676 | `			/* Add the piece before the match */` |
|     9 |  677 | `			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| pieceLen > 0 ){` |
|     9 |  678 | `				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  679 | `					ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  680 | `					ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  681 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|   ! 0 |  682 | `					ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  683 | `					ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  684 | `					ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  685 | `					ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  686 | `					ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  687 | `					ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  688 | `				}else{` |
|     9 |  689 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|     9 |  690 | `					ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  691 | `				}` |
|     9 |  692 | `				ph7_value_reset_string_cursor(pVal);` |
|     9 |  693 | `				nPieces++;` |
|     4 |  694 | `			}` |
|     - |  695 | `			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */` |
|     9 |  696 | `			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){` |
|     - |  697 | `				int g;` |
|   ! 0 |  698 | `				for( g = 1; g < rc; g++ ){` |
|   ! 0 |  699 | `					PCRE2_SIZE gs = ovector[2*g];` |
|   ! 0 |  700 | `					PCRE2_SIZE ge = ovector[2*g+1];` |
|     - |  701 | `					int gLen;` |
|   ! 0 |  702 | `					if( gs == PCRE2_UNSET ) continue;` |
|   ! 0 |  703 | `					gLen = (int)(ge - gs);` |
|   ! 0 |  704 | `					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| gLen > 0 ){` |
|   ! 0 |  705 | `						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  706 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  707 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  708 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  709 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  710 | `							ph7_value_int(pOff, (int)gs);` |
|   ! 0 |  711 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  712 | `							ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  713 | `							ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  714 | `							ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  715 | `						}else{` |
|   ! 0 |  716 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  717 | `							ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  718 | `						}` |
|   ! 0 |  719 | `						ph7_value_reset_string_cursor(pVal);` |
|   ! 0 |  720 | `					}` |
|   ! 0 |  721 | `				}` |
|   ! 0 |  722 | `			}` |
|     - |  723 | `			/* Advance */` |
|     9 |  724 | `			lastOffset = matchEnd;` |
|     9 |  725 | `			if( matchEnd == matchStart ){` |
|   ! 0 |  726 | `				startOffset = matchEnd + 1;` |
|   ! 0 |  727 | `			}else{` |
|     9 |  728 | `				startOffset = matchEnd;` |
|     - |  729 | `			}` |
|     - |  730 | `		}` |
|     1 |  731 | `	}` |
|     - |  732 | `	/* Add trailing piece */` |
|     - |  733 | `	{` |
|     5 |  734 | `		int trailLen = nSubLen - (int)lastOffset;` |
|     5 |  735 | `		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| trailLen > 0 ){` |
|     5 |  736 | `			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  737 | `				ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  738 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  739 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|   ! 0 |  740 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  741 | `				ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  742 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  743 | `				ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  744 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  745 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  746 | `			}else{` |
|     5 |  747 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|     5 |  748 | `				ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  749 | `			}` |
|     2 |  750 | `		}` |
|     - |  751 | `	}` |
|     5 |  752 | `	ph7_context_release_value(pCtx, pVal);` |
|     5 |  753 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  754 | `	ph7_result_value(pCtx, pArray);` |
|     5 |  755 | `	ph7_context_release_value(pCtx, pArray);` |
|     5 |  756 | `	return PH7_OK;` |
|     3 |  757 |  |
|     - |  758 |  |
|     - |  759 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|     8 |  760 | `static void PcreExpandBackrefs(` |
|     - |  761 | `	SyBlob *pOut,` |
|     - |  762 | `	const char *zRepl, int nReplLen,` |
|     - |  763 | `	const char *zSubject,` |
|     - |  764 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     1 |  765 |  |
|     9 |  766 | `	const char *zEnd = &zRepl[nReplLen];` |
|     9 |  767 | `	const char *z = zRepl;` |
|     - |  768 |  |
|    21 |  769 | `	while( z < zEnd ){` |
|    13 |  770 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   ! 0 |  771 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|   ! 0 |  772 | `				int g = z[1] - '0';` |
|   ! 0 |  773 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  774 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  775 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  776 | `				}` |
|   ! 0 |  777 | `				z += 2;` |
|   ! 0 |  778 | `				continue;` |
|     - |  779 | `			}` |
|   ! 0 |  780 | `			if( z[1] == '\\' ){` |
|   ! 0 |  781 | `				SyBlobAppend(pOut, "\\", 1);` |
|   ! 0 |  782 | `				z += 2;` |
|   ! 0 |  783 | `				continue;` |
|     - |  784 | `			}` |
|     - |  785 | `			/* Not a backreference — emit literally */` |
|   ! 0 |  786 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  787 | `			z++;` |
|   ! 0 |  788 | `			continue;` |
|     - |  789 | `		}` |
|    13 |  790 | `		if( *z == '$' && z + 1 < zEnd ){` |
|     5 |  791 | `			if( z[1] == '$' ){` |
|   ! 0 |  792 | `				SyBlobAppend(pOut, "$", 1);` |
|   ! 0 |  793 | `				z += 2;` |
|   ! 0 |  794 | `				continue;` |
|     - |  795 | `			}` |
|     5 |  796 | `			if( z[1] == '{' ){` |
|     - |  797 | `				/* ${N} form */` |
|   ! 0 |  798 | `				const char *p = z + 2;` |
|   ! 0 |  799 | `				int g = 0;` |
|   ! 0 |  800 | `				while( p < zEnd && *p >= '0' && *p <= '9' ){` |
|   ! 0 |  801 | `					g = g * 10 + (*p - '0');` |
|   ! 0 |  802 | `					p++;` |
|   ! 0 |  803 | `				}` |
|   ! 0 |  804 | `				if( p < zEnd && *p == '}' ){` |
|   ! 0 |  805 | `					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  806 | `						SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  807 | `							(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  808 | `					}` |
|   ! 0 |  809 | `					z = p + 1;` |
|   ! 0 |  810 | `					continue;` |
|     - |  811 | `				}` |
|     - |  812 | `				/* Not a valid ${N} — emit literally */` |
|   ! 0 |  813 | `				SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  814 | `				z++;` |
|   ! 0 |  815 | `				continue;` |
|     - |  816 | `			}` |
|     5 |  817 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|     - |  818 | `				/* $N or $NN */` |
|     5 |  819 | `				int g = z[1] - '0';` |
|     5 |  820 | `				z += 2;` |
|     - |  821 | `				/* Check for second digit */` |
|     5 |  822 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  823 | `					int g2 = g * 10 + (*z - '0');` |
|   ! 0 |  824 | `					if( g2 < nGroups ){` |
|   ! 0 |  825 | `						g = g2;` |
|   ! 0 |  826 | `						z++;` |
|   ! 0 |  827 | `					}` |
|   ! 0 |  828 | `				}` |
|     5 |  829 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|     7 |  830 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|     4 |  831 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|     2 |  832 | `				}` |
|     5 |  833 | `				continue;` |
|     - |  834 | `			}` |
|     - |  835 | `			/* Not a backreference */` |
|   ! 0 |  836 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  837 | `			z++;` |
|   ! 0 |  838 | `			continue;` |
|     - |  839 | `		}` |
|     9 |  840 | `		SyBlobAppend(pOut, z, 1);` |
|     9 |  841 | `		z++;` |
|     1 |  842 | `	}` |
|     9 |  843 |  |
|     - |  844 |  |
|     - |  845 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|     6 |  846 | `static void PcreDoReplace(` |
|     - |  847 | `	ph7_context *pCtx,` |
|     - |  848 | `	pcre2_code *pCode,` |
|     - |  849 | `	const char *zSubject, int nSubLen,` |
|     - |  850 | `	const char *zRepl, int nReplLen,` |
|     - |  851 | `	int limit,` |
|     - |  852 | `	int *pCount,` |
|     - |  853 | `	SyBlob *pOut)` |
|     1 |  854 |  |
|     - |  855 | `	pcre2_match_data *pMatchData;` |
|     7 |  856 | `	PCRE2_SIZE startOffset = 0;` |
|     7 |  857 | `	int nReplacements = 0;` |
|     - |  858 | `	int rc;` |
|     - |  859 |  |
|     7 |  860 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     7 |  861 | `	if( pMatchData == 0 ) return;` |
|     - |  862 |  |
|    15 |  863 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  864 | `		PCRE2_SIZE *ovector;` |
|    15 |  865 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|    22 |  866 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     7 |  867 | `			startOffset, 0, pMatchData, NULL);` |
|    15 |  868 | `		if( rc < 0 ){` |
|     7 |  869 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  870 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  871 | `			}` |
|     7 |  872 | `			break;` |
|     - |  873 | `		}` |
|     9 |  874 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - |  875 | `		/* Copy text before match */` |
|     9 |  876 | `		if( ovector[0] > startOffset ){` |
|     7 |  877 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     3 |  878 | `		}` |
|     - |  879 | `		/* Expand replacement */` |
|     9 |  880 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|     9 |  881 | `		nReplacements++;` |
|     - |  882 | `		/* Advance */` |
|     9 |  883 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 |  884 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 |  885 | `				SyBlobAppend(pOut, &zSubject[startOffset], 1);` |
|   ! 0 |  886 | `			}` |
|   ! 0 |  887 | `			startOffset = ovector[0] + 1;` |
|   ! 0 |  888 | `		}else{` |
|     9 |  889 | `			startOffset = ovector[1];` |
|     - |  890 | `		}` |
|     1 |  891 | `	}` |
|     - |  892 | `	/* Copy remainder */` |
|     7 |  893 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 |  894 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   ! 0 |  895 | `	}` |
|     7 |  896 | `	if( pCount ){` |
|     7 |  897 | `		*pCount += nReplacements;` |
|     3 |  898 | `	}` |
|     7 |  899 | `	pcre2_match_data_free(pMatchData);` |
|     3 |  900 | `	SXUNUSED(pCtx);` |
|     4 |  901 |  |
|     - |  902 |  |
|     - |  903 | `/* ======================================================================` |
|     - |  904 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - |  905 | ` * ====================================================================== */` |
|     6 |  906 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  907 |  |
|     7 |  908 | `	int limit = -1;` |
|     7 |  909 | `	int count = 0;` |
|     - |  910 |  |
|     7 |  911 | `	if( nArg < 3 ){` |
|   ! 0 |  912 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  913 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 |  914 | `		ph7_result_null(pCtx);` |
|   ! 0 |  915 | `		return PH7_OK;` |
|     - |  916 | `	}` |
|     7 |  917 | `	if( nArg >= 4 ){` |
|   ! 0 |  918 | `		limit = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  919 | `	}` |
|     7 |  920 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  921 |  |
|     - |  922 | `	/* Reject array subjects (not yet supported) */` |
|     7 |  923 | `	if( ph7_value_is_array(apArg[2]) ){` |
|   ! 0 |  924 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  925 | `			"preg_replace(): Array subjects are not yet supported");` |
|   ! 0 |  926 | `		ph7_result_null(pCtx);` |
|   ! 0 |  927 | `		return PH7_OK;` |
|     - |  928 | `	}` |
|     7 |  929 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     - |  930 | `		/* Single pattern + single replacement on a string subject */` |
|     - |  931 | `		const char *zPattern, *zRepl, *zSubject;` |
|     - |  932 | `		int nPatLen, nReplLen, nSubLen;` |
|     - |  933 | `		pcre2_code *pCode;` |
|     - |  934 | `		sxu32 nCapture;` |
|     - |  935 | `		SyBlob sOut;` |
|     - |  936 |  |
|     7 |  937 | `		zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     7 |  938 | `		zRepl = ph7_value_to_string(apArg[1], &nReplLen);` |
|     7 |  939 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|     - |  940 |  |
|     7 |  941 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     7 |  942 | `		if( pCode == 0 ){` |
|   ! 0 |  943 | `			ph7_result_null(pCtx);` |
|   ! 0 |  944 | `			return PH7_OK;` |
|     - |  945 | `		}` |
|     7 |  946 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     7 |  947 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, &count, &sOut);` |
|     7 |  948 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     7 |  949 | `		SyBlobRelease(&sOut);` |
|     4 |  950 | `	}else{` |
|     - |  951 | `		/* TODO: array of patterns — iterate pairs and apply sequentially */` |
|   ! 0 |  952 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  953 | `			"preg_replace() with array patterns is not yet supported");` |
|   ! 0 |  954 | `		ph7_result_null(pCtx);` |
|   ! 0 |  955 | `		return PH7_OK;` |
|     - |  956 | `	}` |
|     - |  957 | `	/* Set &$count if provided */` |
|     7 |  958 | `	if( nArg >= 5 ){` |
|     - |  959 | `		ph7_value sCount;` |
|   ! 0 |  960 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|   ! 0 |  961 | `		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);` |
|   ! 0 |  962 | `		PH7_MemObjRelease(&sCount);` |
|   ! 0 |  963 | `	}` |
|     7 |  964 | `	return PH7_OK;` |
|     4 |  965 |  |
|     - |  966 |  |
|     - |  967 | `/* ======================================================================` |
|     - |  968 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count]])` |
|     - |  969 | ` * ====================================================================== */` |
|     2 |  970 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  971 |  |
|     - |  972 | `	const char *zPattern, *zSubject;` |
|     - |  973 | `	int nPatLen, nSubLen;` |
|     - |  974 | `	pcre2_code *pCode;` |
|     - |  975 | `	pcre2_match_data *pMatchData;` |
|     - |  976 | `	sxu32 nCapture;` |
|     - |  977 | `	SyBlob sOut;` |
|     3 |  978 | `	PCRE2_SIZE startOffset = 0;` |
|     3 |  979 | `	int limit = -1;` |
|     3 |  980 | `	int count = 0;` |
|     - |  981 | `	int rc;` |
|     - |  982 |  |
|     3 |  983 | `	if( nArg < 3 ){` |
|   ! 0 |  984 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  985 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 |  986 | `		ph7_result_null(pCtx);` |
|   ! 0 |  987 | `		return PH7_OK;` |
|     - |  988 | `	}` |
|     3 |  989 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     3 |  990 | `	zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|     3 |  991 | `	if( nArg >= 4 ){` |
|   ! 0 |  992 | `		limit = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  993 | `	}` |
|     3 |  994 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 |  995 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  996 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 |  997 | `		ph7_result_null(pCtx);` |
|   ! 0 |  998 | `		return PH7_OK;` |
|     - |  999 | `	}` |
|     3 | 1000 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     3 | 1001 | `	if( pCode == 0 ){` |
|   ! 0 | 1002 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1003 | `		return PH7_OK;` |
|     - | 1004 | `	}` |
|     3 | 1005 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     3 | 1006 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1007 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1008 | `		return PH7_OK;` |
|     - | 1009 | `	}` |
|     3 | 1010 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     3 | 1011 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1012 |  |
|     7 | 1013 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1014 | `		PCRE2_SIZE *ovector;` |
|     - | 1015 | `		ph7_value *pMatchArr;` |
|     - | 1016 | `		ph7_value *apCbArg[1];` |
|     - | 1017 | `		ph7_value sResult;` |
|     - | 1018 | `		const char *zReplacement;` |
|     - | 1019 | `		int nReplLen;` |
|     - | 1020 |  |
|     8 | 1021 | `		if( limit >= 0 && count >= limit ) break;` |
|    10 | 1022 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     3 | 1023 | `			startOffset, 0, pMatchData, NULL);` |
|     7 | 1024 | `		if( rc < 0 ){` |
|     3 | 1025 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1026 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1027 | `			}` |
|     3 | 1028 | `			break;` |
|     - | 1029 | `		}` |
|     5 | 1030 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1031 | `		/* Copy text before match */` |
|     5 | 1032 | `		if( ovector[0] > startOffset ){` |
|     3 | 1033 | `			SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     1 | 1034 | `		}` |
|     - | 1035 | `		/* Build matches array for callback */` |
|     5 | 1036 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|     5 | 1037 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, 0);` |
|     - | 1038 | `		/* Call the callback */` |
|     5 | 1039 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|     5 | 1040 | `		apCbArg[0] = pMatchArr;` |
|     5 | 1041 | `		PH7_VmCallUserFunction(pCtx->pVm, apArg[1], 1, apCbArg, &sResult);` |
|     - | 1042 | `		/* Get replacement string from callback result */` |
|     5 | 1043 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|     5 | 1044 | `		SyBlobAppend(&sOut, zReplacement, (sxu32)nReplLen);` |
|     5 | 1045 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1046 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|     5 | 1047 | `		count++;` |
|     - | 1048 | `		/* Advance */` |
|     5 | 1049 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 | 1050 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1051 | `				SyBlobAppend(&sOut, &zSubject[startOffset], 1);` |
|   ! 0 | 1052 | `			}` |
|   ! 0 | 1053 | `			startOffset = ovector[0] + 1;` |
|   ! 0 | 1054 | `		}else{` |
|     5 | 1055 | `			startOffset = ovector[1];` |
|     - | 1056 | `		}` |
|     1 | 1057 | `	}` |
|     - | 1058 | `	/* Copy remainder */` |
|     3 | 1059 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1060 | `		SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   ! 0 | 1061 | `	}` |
|     3 | 1062 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     3 | 1063 | `	SyBlobRelease(&sOut);` |
|     3 | 1064 | `	pcre2_match_data_free(pMatchData);` |
|     - | 1065 | `	/* Set &$count if provided */` |
|     3 | 1066 | `	if( nArg >= 5 ){` |
|     - | 1067 | `		ph7_value sCount;` |
|   ! 0 | 1068 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|   ! 0 | 1069 | `		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);` |
|   ! 0 | 1070 | `		PH7_MemObjRelease(&sCount);` |
|   ! 0 | 1071 | `	}` |
|     3 | 1072 | `	return PH7_OK;` |
|     2 | 1073 |  |
|     - | 1074 |  |
|     - | 1075 | `/* ======================================================================` |
|     - | 1076 | ` * preg_quote(str [, delimiter])` |
|     - | 1077 | ` * ====================================================================== */` |
|     6 | 1078 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1079 |  |
|     7 | 1080 | `	const char *zStr, *zDelim = 0;` |
|     7 | 1081 | `	int nLen, nDelimLen = 0;` |
|     - | 1082 | `	const char *z, *zEnd;` |
|     - | 1083 |  |
|     7 | 1084 | `	if( nArg < 1 ){` |
|   ! 0 | 1085 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1086 | `		return PH7_OK;` |
|     - | 1087 | `	}` |
|     7 | 1088 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|     7 | 1089 | `	if( nArg >= 2 ){` |
|     3 | 1090 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     1 | 1091 | `	}` |
|     7 | 1092 | `	z = zStr;` |
|     7 | 1093 | `	zEnd = &zStr[nLen];` |
|    71 | 1094 | `	while( z < zEnd ){` |
|    65 | 1095 | `		char c = *z;` |
|    65 | 1096 | `		switch( c ){` |
|     4 | 1097 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1098 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1099 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1100 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1101 | `			case '#':` |
|     9 | 1102 | `				ph7_result_string(pCtx, "\\", 1);` |
|     9 | 1103 | `				break;` |
|    28 | 1104 | `			default:` |
|    57 | 1105 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     3 | 1106 | `					ph7_result_string(pCtx, "\\", 1);` |
|     1 | 1107 | `				}` |
|    56 | 1108 | `				break;` |
|     - | 1109 | `		}` |
|    65 | 1110 | `		ph7_result_string(pCtx, z, 1);` |
|    65 | 1111 | `		z++;` |
|     1 | 1112 | `	}` |
|     7 | 1113 | `	return PH7_OK;` |
|     4 | 1114 |  |
|     - | 1115 |  |
|     - | 1116 | `/* ======================================================================` |
|     - | 1117 | ` * preg_last_error()` |
|     - | 1118 | ` * ====================================================================== */` |
|   ! 0 | 1119 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1120 |  |
|   ! 0 | 1121 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1122 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1123 | `	return PH7_OK;` |
|   ! 0 | 1124 |  |
|     - | 1125 |  |
|     - | 1126 | `/* ======================================================================` |
|     - | 1127 | ` * preg_last_error_msg()` |
|     - | 1128 | ` * ====================================================================== */` |
|   ! 0 | 1129 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1130 |  |
|     - | 1131 | `	const char *zMsg;` |
|   ! 0 | 1132 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1133 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1134 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1135 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1136 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1137 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1138 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1139 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1140 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1141 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1142 | `	}` |
|   ! 0 | 1143 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1144 | `	return PH7_OK;` |
|   ! 0 | 1145 |  |
|     - | 1146 |  |
|     - | 1147 | `/* ===== Function registration table ===== */` |
|     - | 1148 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1149 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1150 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1151 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1152 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1153 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1154 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1155 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1156 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1157 | `};` |
|     - | 1158 |  |
|  2554 | 1159 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     2 | 1160 |  |
|     - | 1161 | `	sxu32 n;` |
| 22988 | 1162 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 20434 | 1163 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 10218 | 1164 | `	}` |
|  2556 | 1165 |  |
|     - | 1166 |  |
|     - | 1167 | `/* ===== Constant registration ===== */` |
|     - | 1168 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1169 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1170 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1171 | `	}` |
|     - | 1172 |  |
|   ! 0 | 1173 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|     3 | 1174 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|   ! 0 | 1175 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|   ! 0 | 1176 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|   ! 0 | 1177 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|   ! 0 | 1178 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|   ! 0 | 1179 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|   ! 0 | 1180 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|   ! 0 | 1181 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|   ! 0 | 1182 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|   ! 0 | 1183 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|   ! 0 | 1184 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|   ! 0 | 1185 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|   ! 0 | 1186 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1187 |  |
|  2554 | 1188 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     2 | 1189 |  |
|  2556 | 1190 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  2556 | 1191 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  2556 | 1192 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  2556 | 1193 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  2556 | 1194 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  2556 | 1195 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  2556 | 1196 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  2556 | 1197 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  2556 | 1198 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  2556 | 1199 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  2556 | 1200 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  2556 | 1201 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  2556 | 1202 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  2556 | 1203 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  2556 | 1204 |  |
|     - | 1205 |  |
|     - | 1206 | `#else` |
|     - | 1207 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1208 | `typedef int vm_pcre_unused;` |
|     - | 1209 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1210 |  |
