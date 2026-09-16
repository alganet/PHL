# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 632/915 lines (69.07%)

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
|     - |   28 | `#define PHP_PREG_GREP_INVERT             1  /* preg_grep()'s only flag */` |
|     - |   29 | `#define PHP_PREG_SPLIT_DELIM_CAPTURE     2` |
|     - |   30 | `#define PHP_PREG_SPLIT_OFFSET_CAPTURE    4` |
|     - |   31 |  |
|     - |   32 | `#define PHP_PREG_NO_ERROR                0` |
|     - |   33 | `#define PHP_PREG_INTERNAL_ERROR          1` |
|     - |   34 | `#define PHP_PREG_BACKTRACK_LIMIT_ERROR   2` |
|     - |   35 | `#define PHP_PREG_RECURSION_LIMIT_ERROR   3` |
|     - |   36 | `#define PHP_PREG_BAD_UTF8_ERROR          4` |
|     - |   37 | `#define PHP_PREG_BAD_UTF8_OFFSET_ERROR   5` |
|     - |   38 | `#define PHP_PREG_JIT_STACKLIMIT_ERROR    6` |
|     - |   39 |  |
|     - |   40 | `/* ===== Compiled-regex cache ===== */` |
|     - |   41 | `#define PCRE_CACHE_SIZE 16` |
|     - |   42 |  |
|     - |   43 | `typedef struct PcreCacheEntry PcreCacheEntry;` |
|     - |   44 | `struct PcreCacheEntry {` |
|     - |   45 | `	char *zPattern;          /* Full PHP pattern string (heap copy) */` |
|     - |   46 | `	sxu32 nLen;` |
|     - |   47 | `	pcre2_code *pCode;` |
|     - |   48 | `	sxu32 nCaptureCount;` |
|     - |   49 | `	sxu32 iLastUsed;` |
|     - |   50 | `};` |
|     - |   51 |  |
|     - |   52 | `static PcreCacheEntry aCache[PCRE_CACHE_SIZE];` |
|     - |   53 | `static sxu32 nCacheUsed = 0;` |
|     - |   54 | `static sxu32 iCacheClock = 0;` |
|     - |   55 |  |
|   306 |   56 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|     5 |   57 | `{` |
|     - |   58 | `	sxu32 i;` |
|  2577 |   59 | `	for( i = 0; i < nCacheUsed; i++ ){` |
|  2473 |   60 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|   207 |   61 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|   207 |   62 | `			if( pCaptureCount ){` |
|   207 |   63 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|   102 |   64 | `			}` |
|   207 |   65 | `			return aCache[i].pCode;` |
|     - |   66 | `		}` |
|  1136 |   67 | `	}` |
|   107 |   68 | `	return 0;` |
|   158 |   69 | `}` |
|     - |   70 |  |
|   102 |   71 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|     5 |   72 | `{` |
|     - |   73 | `	PcreCacheEntry *pEntry;` |
|     - |   74 | `	char *zCopy;` |
|     - |   75 | `	/* Allocate the pattern copy first, before touching the cache */` |
|   107 |   76 | `	zCopy = (char *)malloc(nLen + 1);` |
|   107 |   77 | `	if( zCopy == 0 ){` |
|     - |   78 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|   ! 0 |   79 | `		return;` |
|     - |   80 | `	}` |
|   107 |   81 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|   107 |   82 | `	zCopy[nLen] = 0;` |
|   107 |   83 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|    51 |   84 | `		pEntry = &aCache[nCacheUsed++];` |
|    28 |   85 | `	}else{` |
|     - |   86 | `		/* Evict LRU */` |
|    57 |   87 | `		sxu32 iMin = aCache[0].iLastUsed;` |
|    57 |   88 | `		sxu32 iMinIdx = 0;` |
|     - |   89 | `		sxu32 i;` |
|   897 |   90 | `		for( i = 1; i < PCRE_CACHE_SIZE; i++ ){` |
|   841 |   91 | `			if( aCache[i].iLastUsed < iMin ){` |
|    89 |   92 | `				iMin = aCache[i].iLastUsed;` |
|    89 |   93 | `				iMinIdx = i;` |
|    44 |   94 | `			}` |
|   421 |   95 | `		}` |
|    57 |   96 | `		pEntry = &aCache[iMinIdx];` |
|    57 |   97 | `		pcre2_code_free(pEntry->pCode);` |
|    57 |   98 | `		free(pEntry->zPattern);` |
|     - |   99 | `	}` |
|   107 |  100 | `	pEntry->zPattern = zCopy;` |
|   107 |  101 | `	pEntry->nLen = nLen;` |
|   107 |  102 | `	pEntry->pCode = pCode;` |
|   107 |  103 | `	pEntry->nCaptureCount = nCaptureCount;` |
|   107 |  104 | `	pEntry->iLastUsed = ++iCacheClock;` |
|    56 |  105 | `}` |
|     - |  106 |  |
|     - |  107 | `/* ===== Delimiter parser ===== */` |
|     - |  108 | `#define PCRE_PARSE_OK             0` |
|     - |  109 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|     - |  110 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */` |
|     - |  111 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|     - |  112 |  |
|   102 |  113 | `static sxi32 PcreParsePattern(` |
|     - |  114 | `	const char *zInput, int nInputLen,` |
|     - |  115 | `	const char **pPattern, int *pnPatternLen,` |
|     - |  116 | `	const char **pFlags, int *pnFlagLen)` |
|     5 |  117 | `{` |
|   107 |  118 | `	const char *zEnd = &zInput[nInputLen];` |
|   107 |  119 | `	const char *z = zInput;` |
|     - |  120 | `	char cOpen, cClose;` |
|     - |  121 | `	const char *pStart;` |
|     - |  122 |  |
|     - |  123 | `	/* Skip leading whitespace */` |
|   107 |  124 | `	while( z < zEnd && (unsigned char)*z <= 0x20 ){` |
|   ! 0 |  125 | `		z++;` |
|   ! 0 |  126 | `	}` |
|   107 |  127 | `	if( z >= zEnd ){` |
|   ! 0 |  128 | `		return PCRE_PARSE_EMPTY;` |
|     - |  129 | `	}` |
|   107 |  130 | `	cOpen = *z;` |
|     - |  131 | `	/* Must not be alphanumeric, backslash, or whitespace */` |
|   107 |  132 | `	if( SyisAlphaNum(cOpen) \|\| cOpen == '\\' \|\| (unsigned char)cOpen <= 0x20 ){` |
|   ! 0 |  133 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|     - |  134 | `	}` |
|     - |  135 | `	/* Paired delimiters */` |
|   107 |  136 | `	switch( cOpen ){` |
|   ! 0 |  137 | `		case '(': cClose = ')'; break;` |
|   ! 0 |  138 | `		case '[': cClose = ']'; break;` |
|   ! 0 |  139 | `		case '{': cClose = '}'; break;` |
|   ! 0 |  140 | `		case '<': cClose = '>'; break;` |
|   107 |  141 | `		default:  cClose = cOpen; break;` |
|     - |  142 | `	}` |
|   107 |  143 | `	z++; /* Skip opening delimiter */` |
|   107 |  144 | `	pStart = z;` |
|     - |  145 | `	/* Scan for closing delimiter, respecting backslash escapes */` |
|  1503 |  146 | `	while( z < zEnd ){` |
|  1503 |  147 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   113 |  148 | `			z += 2; /* Skip escaped char */` |
|   113 |  149 | `			continue;` |
|     - |  150 | `		}` |
|  1391 |  151 | `		if( *z == cClose ){` |
|   107 |  152 | `			break;` |
|     - |  153 | `		}` |
|  1289 |  154 | `		z++;` |
|     5 |  155 | `	}` |
|   107 |  156 | `	if( z >= zEnd ){` |
|   ! 0 |  157 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|     - |  158 | `	}` |
|   107 |  159 | `	*pPattern = pStart;` |
|   107 |  160 | `	*pnPatternLen = (int)(z - pStart);` |
|   107 |  161 | `	z++; /* Skip closing delimiter */` |
|   107 |  162 | `	*pFlags = z;` |
|   107 |  163 | `	*pnFlagLen = (int)(zEnd - z);` |
|   107 |  164 | `	return PH7_OK;` |
|    56 |  165 | `}` |
|     - |  166 |  |
|     - |  167 | `/* ===== Flag mapper ===== */` |
|   102 |  168 | `static sxi32 PcreMapFlags(` |
|     - |  169 | `	const char *zFlags, int nFlagLen,` |
|     - |  170 | `	uint32_t *pCompileOpts)` |
|     5 |  171 | `{` |
|     - |  172 | `	int i;` |
|   107 |  173 | `	*pCompileOpts = 0;` |
|   131 |  174 | `	for( i = 0; i < nFlagLen; i++ ){` |
|    25 |  175 | `		switch( zFlags[i] ){` |
|    13 |  176 | `			case 'i': *pCompileOpts \|= PCRE2_CASELESS; break;` |
|     5 |  177 | `			case 'm': *pCompileOpts \|= PCRE2_MULTILINE; break;` |
|     5 |  178 | `			case 's': *pCompileOpts \|= PCRE2_DOTALL; break;` |
|   ! 0 |  179 | `			case 'x': *pCompileOpts \|= PCRE2_EXTENDED; break;` |
|     5 |  180 | `			case 'u': *pCompileOpts \|= PCRE2_UTF \| PCRE2_UCP; break;` |
|   ! 0 |  181 | `			case 'A': *pCompileOpts \|= PCRE2_ANCHORED; break;` |
|   ! 0 |  182 | `			case 'D': *pCompileOpts \|= PCRE2_DOLLAR_ENDONLY; break;` |
|   ! 0 |  183 | `			case 'U': *pCompileOpts \|= PCRE2_UNGREEDY; break;` |
|   ! 0 |  184 | `			case 'J': *pCompileOpts \|= PCRE2_DUPNAMES; break;` |
|   ! 0 |  185 | `			case 'S': /* Study hint — no-op in PCRE2 */ break;` |
|   ! 0 |  186 | `			default: break;` |
|     - |  187 | `		}` |
|    13 |  188 | `	}` |
|   107 |  189 | `	return PH7_OK;` |
|     5 |  190 | `}` |
|     - |  191 |  |
|     - |  192 | `/* ===== Compile helper ===== */` |
|   306 |  193 | `static pcre2_code *PcreCompile(` |
|     - |  194 | `	ph7_context *pCtx,` |
|     - |  195 | `	const char *zFullPattern, int nLen,` |
|     - |  196 | `	sxu32 *pCaptureCount)` |
|     5 |  197 | `{` |
|     - |  198 | `	const char *zPat, *zFlags;` |
|     - |  199 | `	int nPatLen, nFlagLen;` |
|     - |  200 | `	uint32_t compileOpts;` |
|     - |  201 | `	pcre2_code *pCode;` |
|     - |  202 | `	PCRE2_SIZE erroffset;` |
|     - |  203 | `	int errcode;` |
|     - |  204 | `	sxu32 nCapture;` |
|     - |  205 | `	sxi32 parseRc;` |
|     - |  206 |  |
|     - |  207 | `	/* Check cache first */` |
|   311 |  208 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|   311 |  209 | `	if( pCode ){` |
|   207 |  210 | `		return pCode;` |
|     - |  211 | `	}` |
|     - |  212 | `	/* Parse delimiter */` |
|   107 |  213 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen);` |
|   107 |  214 | `	if( parseRc != PCRE_PARSE_OK ){` |
|     - |  215 | `		const char *zMsg;` |
|   ! 0 |  216 | `		switch( parseRc ){` |
|   ! 0 |  217 | `			case PCRE_PARSE_EMPTY:         zMsg = "Empty regular expression"; break;` |
|   ! 0 |  218 | `			case PCRE_PARSE_BAD_DELIMITER: zMsg = "Delimiter must not be alphanumeric, backslash, or whitespace"; break;` |
|   ! 0 |  219 | `			default:                       zMsg = "No ending delimiter found"; break;` |
|     - |  220 | `		}` |
|   ! 0 |  221 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, zMsg);` |
|   ! 0 |  222 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|   ! 0 |  223 | `		return 0;` |
|     - |  224 | `	}` |
|     - |  225 | `	/* Map flags */` |
|   107 |  226 | `	PcreMapFlags(zFlags, nFlagLen, &compileOpts);` |
|     - |  227 | `	/* Compile */` |
|   107 |  228 | `	pCode = pcre2_compile(` |
|    51 |  229 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|    51 |  230 | `		compileOpts, &errcode, &erroffset, NULL);` |
|   107 |  231 | `	if( pCode == 0 ){` |
|     - |  232 | `		PCRE2_UCHAR errbuf[256];` |
|   ! 0 |  233 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|   ! 0 |  234 | `		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,` |
|   ! 0 |  235 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|   ! 0 |  236 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|   ! 0 |  237 | `		return 0;` |
|     - |  238 | `	}` |
|     - |  239 | `	/* Get capture count */` |
|   107 |  240 | `	nCapture = 0;` |
|   107 |  241 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|   107 |  242 | `	if( pCaptureCount ){` |
|   107 |  243 | `		*pCaptureCount = nCapture;` |
|    51 |  244 | `	}` |
|     - |  245 | `	/* Cache it */` |
|   107 |  246 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|   107 |  247 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   107 |  248 | `	return pCode;` |
|   158 |  249 | `}` |
|     - |  250 |  |
|     - |  251 | `/* ===== Map PCRE2 match error to PHP error code ===== */` |
|   ! 0 |  252 | `static void PcreSetMatchError(ph7_vm *pVm, int rc)` |
|   ! 0 |  253 | `{` |
|   ! 0 |  254 | `	if( rc == PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  255 | `		pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   ! 0 |  256 | `	}else if( rc == PCRE2_ERROR_MATCHLIMIT ){` |
|   ! 0 |  257 | `		pVm->iPcreLastError = PHP_PREG_BACKTRACK_LIMIT_ERROR;` |
|   ! 0 |  258 | `	}else if( rc == PCRE2_ERROR_DEPTHLIMIT` |
|     - |  259 | `#ifdef PCRE2_ERROR_RECURSIONLIMIT` |
|   ! 0 |  260 | `		\|\| rc == PCRE2_ERROR_RECURSIONLIMIT` |
|     - |  261 | `#endif` |
|     - |  262 | `	){` |
|   ! 0 |  263 | `		pVm->iPcreLastError = PHP_PREG_RECURSION_LIMIT_ERROR;` |
|   ! 0 |  264 | `	}else if( rc == PCRE2_ERROR_BADUTFOFFSET ){` |
|   ! 0 |  265 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_OFFSET_ERROR;` |
|   ! 0 |  266 | `	}else if( rc == PCRE2_ERROR_UTF8_ERR1` |
|   ! 0 |  267 | `		\|\| rc == PCRE2_ERROR_UTF8_ERR2 ){` |
|   ! 0 |  268 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_ERROR;` |
|     - |  269 | `#ifdef PCRE2_ERROR_JIT_STACKLIMIT` |
|   ! 0 |  270 | `	}else if( rc == PCRE2_ERROR_JIT_STACKLIMIT ){` |
|   ! 0 |  271 | `		pVm->iPcreLastError = PHP_PREG_JIT_STACKLIMIT_ERROR;` |
|     - |  272 | `#endif` |
|   ! 0 |  273 | `	}else{` |
|   ! 0 |  274 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|     - |  275 | `	}` |
|   ! 0 |  276 | `}` |
|     - |  277 |  |
|     - |  278 | `/* ===== Helper: populate matches array from ovector ===== */` |
|   144 |  279 | `static void PcrePopulateMatches(` |
|     - |  280 | `	ph7_context *pCtx,` |
|     - |  281 | `	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */` |
|     - |  282 | `	const char *zSubject,` |
|     - |  283 | `	PCRE2_SIZE *ovector,` |
|     - |  284 | `	int nGroups,` |
|     - |  285 | `	pcre2_code *pCode,` |
|     - |  286 | `	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */` |
|     5 |  287 | `{` |
|   149 |  288 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   149 |  289 | `	ph7_value *pSub = 0;` |
|   149 |  290 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|   149 |  291 | `	PCRE2_SPTR nametable = 0;` |
|     - |  292 | `	int i;` |
|     - |  293 |  |
|   149 |  294 | `	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  295 | `		pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  296 | `	}` |
|     - |  297 | `	/* Read the name table up front so each group's named key can be emitted` |
|     - |  298 | `	 * INTERLEAVED with its numbered key, in group order — php stores` |
|     - |  299 | ``	 * `0, name, 1, value, 2` (named entry immediately before its number), not`` |
|     - |  300 | `	 * every number followed by every name. Code that iterates $matches or` |
|     - |  301 | `	 * var_dumps it (PHPUnit's annotation parser) depends on this order. */` |
|   149 |  302 | `	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|   149 |  303 | `	if( namecount > 0 ){` |
|     9 |  304 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     9 |  305 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|     4 |  306 | `	}` |
|   647 |  307 | `	for( i = 0; i < nGroups; i++ ){` |
|   503 |  308 | `		PCRE2_SIZE start = ovector[2 * i];` |
|   503 |  309 | `		PCRE2_SIZE end   = ovector[2 * i + 1];` |
|   503 |  310 | `		const char *zName = 0;` |
|     - |  311 | `		/* Does group i carry a (?<name>...) label? namecount is tiny in practice. */` |
|   503 |  312 | `		if( namecount > 0 ){` |
|     - |  313 | `			uint32_t k;` |
|    49 |  314 | `			for( k = 0; k < namecount; k++ ){` |
|    41 |  315 | `				PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|    41 |  316 | `				if( (((entry[0] << 8) \| entry[1])) == i ){` |
|    17 |  317 | `					zName = (const char *)(entry + 2);` |
|    17 |  318 | `					break;` |
|     - |  319 | `				}` |
|    13 |  320 | `			}` |
|    12 |  321 | `		}` |
|   503 |  322 | `		if( start == PCRE2_UNSET ){` |
|   131 |  323 | `			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  324 | `				ph7_value_null(pVal);` |
|   ! 0 |  325 | `			}else{` |
|   131 |  326 | `				ph7_value_string(pVal, "", 0);` |
|     - |  327 | `			}` |
|    66 |  328 | `		}else{` |
|   373 |  329 | `			ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|     - |  330 | `		}` |
|   503 |  331 | `		if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  332 | `			ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  333 | `			ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  334 | `			ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);` |
|   ! 0 |  335 | `			ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|     - |  336 | `			/* php: the named key comes first, then the numbered key (same value). */` |
|   ! 0 |  337 | `			if( zName ){` |
|   ! 0 |  338 | `				ph7_array_add_strkey_elem(pArray, zName, pSub);` |
|   ! 0 |  339 | `			}` |
|   ! 0 |  340 | `			ph7_array_add_intkey_elem(pArray, i, pSub);` |
|   ! 0 |  341 | `			ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  342 | `			ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  343 | `			pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  344 | `		}else{` |
|   503 |  345 | `			if( zName ){` |
|    17 |  346 | `				ph7_array_add_strkey_elem(pArray, zName, pVal);` |
|     8 |  347 | `			}` |
|   503 |  348 | `			ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  349 | `		}` |
|   503 |  350 | `		ph7_value_reset_string_cursor(pVal);` |
|   254 |  351 | `	}` |
|   149 |  352 | `	ph7_context_release_value(pCtx, pVal);` |
|   149 |  353 | `	if( pSub ){` |
|   ! 0 |  354 | `		ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  355 | `	}` |
|   149 |  356 | `}` |
|     - |  357 |  |
|     - |  358 | `/*` |
|     - |  359 | ` * Quiet whole-pattern match used by FILTER_VALIDATE_REGEXP: compile zPat (a full` |
|     - |  360 | ` * "/.../flags" pattern) and test it against zSub. On a successful attempt returns` |
|     - |  361 | ` * SXRET_OK with *pMatched set to 1 (match) or 0 (no match); returns SXERR_INVALID` |
|     - |  362 | ` * on a compile/match error (the caller treats that as a validation failure). The` |
|     - |  363 | ` * compiled code is owned by PcreCompile's cache, so it is not freed here.` |
|     - |  364 | ` */` |
|     4 |  365 | `PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,` |
|     - |  366 | `	const char *zSub,int nSub,int *pMatched)` |
|     1 |  367 | `{` |
|     - |  368 | `	pcre2_code *pCode;` |
|     - |  369 | `	pcre2_match_data *pMatchData;` |
|     - |  370 | `	sxu32 nCapture;` |
|     - |  371 | `	int rc;` |
|     5 |  372 | `	*pMatched = 0;` |
|     5 |  373 | `	pCode = PcreCompile(pCtx,zPat,nPat,&nCapture);` |
|     5 |  374 | `	if( pCode == 0 ){` |
|   ! 0 |  375 | `		return SXERR_INVALID;` |
|     - |  376 | `	}` |
|     5 |  377 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode,NULL);` |
|     5 |  378 | `	if( pMatchData == 0 ){` |
|   ! 0 |  379 | `		return SXERR_INVALID;` |
|     - |  380 | `	}` |
|     5 |  381 | `	rc = pcre2_match(pCode,(PCRE2_SPTR)zSub,(PCRE2_SIZE)nSub,0,0,pMatchData,NULL);` |
|     5 |  382 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  383 | `	if( rc < 0 ){` |
|     3 |  384 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  385 | `			PcreSetMatchError(pCtx->pVm,rc);` |
|   ! 0 |  386 | `			return SXERR_INVALID;` |
|     - |  387 | `		}` |
|     3 |  388 | `		return SXRET_OK; /* clean no-match */` |
|     - |  389 | `	}` |
|     3 |  390 | `	*pMatched = 1;` |
|     3 |  391 | `	return SXRET_OK;` |
|     3 |  392 | `}` |
|     - |  393 | `/* ======================================================================` |
|     - |  394 | ` * preg_match(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  395 | ` * ====================================================================== */` |
|   168 |  396 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  397 | `{` |
|     - |  398 | `	const char *zPattern, *zSubject;` |
|     - |  399 | `	int nPatLen, nSubLen;` |
|     - |  400 | `	pcre2_code *pCode;` |
|     - |  401 | `	pcre2_match_data *pMatchData;` |
|     - |  402 | `	PCRE2_SIZE *ovector;` |
|     - |  403 | `	sxu32 nCapture;` |
|   173 |  404 | `	PCRE2_SIZE startOffset = 0;` |
|   173 |  405 | `	int iFlags = 0;` |
|     - |  406 | `	int rc;` |
|     - |  407 |  |
|   173 |  408 | `	if( nArg < 2 ){` |
|   ! 0 |  409 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  410 | `			"preg_match() expects at least 2 parameters");` |
|   ! 0 |  411 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  412 | `		return PH7_OK;` |
|     - |  413 | `	}` |
|   173 |  414 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|   173 |  415 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|   173 |  416 | `	if( nArg >= 4 ){` |
|     7 |  417 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     3 |  418 | `	}` |
|   173 |  419 | `	if( nArg >= 5 ){` |
|   ! 0 |  420 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  421 | `	}` |
|   173 |  422 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|   173 |  423 | `	if( pCode == 0 ){` |
|   ! 0 |  424 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  425 | `		return PH7_OK;` |
|     - |  426 | `	}` |
|   173 |  427 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|   173 |  428 | `	if( pMatchData == 0 ){` |
|   ! 0 |  429 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  430 | `		return PH7_OK;` |
|     - |  431 | `	}` |
|   257 |  432 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    84 |  433 | `		startOffset, 0, pMatchData, NULL);` |
|   173 |  434 | `	if( rc < 0 ){` |
|    30 |  435 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  436 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  437 | `		}` |
|     - |  438 | `		/* Populate empty matches if requested */` |
|    30 |  439 | `		if( nArg >= 3 ){` |
|    15 |  440 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    15 |  441 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pEmpty);` |
|    15 |  442 | `			ph7_context_release_value(pCtx, pEmpty);` |
|     7 |  443 | `		}` |
|    30 |  444 | `		pcre2_match_data_free(pMatchData);` |
|    30 |  445 | `		ph7_result_int(pCtx, 0);` |
|    30 |  446 | `		return PH7_OK;` |
|     - |  447 | `	}` |
|   145 |  448 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   145 |  449 | `	if( nArg >= 3 ){` |
|     - |  450 | `		/* Populate $matches */` |
|   119 |  451 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|   119 |  452 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|   119 |  453 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);` |
|     - |  454 | `		/* Write the array back to the caller's variable */` |
|   119 |  455 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pArray);` |
|   119 |  456 | `		ph7_context_release_value(pCtx, pArray);` |
|    57 |  457 | `	}` |
|   145 |  458 | `	pcre2_match_data_free(pMatchData);` |
|   145 |  459 | `	ph7_result_int(pCtx, 1);` |
|   145 |  460 | `	return PH7_OK;` |
|    89 |  461 | `}` |
|     - |  462 |  |
|     - |  463 | `/* ======================================================================` |
|     - |  464 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  465 | ` * ====================================================================== */` |
|    32 |  466 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  467 | `{` |
|     - |  468 | `	const char *zPattern, *zSubject;` |
|     - |  469 | `	int nPatLen, nSubLen;` |
|     - |  470 | `	pcre2_code *pCode;` |
|     - |  471 | `	pcre2_match_data *pMatchData;` |
|     - |  472 | `	sxu32 nCapture;` |
|    33 |  473 | `	PCRE2_SIZE startOffset = 0;` |
|    33 |  474 | `	int iFlags = PHP_PREG_PATTERN_ORDER;` |
|    33 |  475 | `	int totalMatches = 0;` |
|     - |  476 | `	int rc;` |
|     - |  477 |  |
|    33 |  478 | `	if( nArg < 2 ){` |
|   ! 0 |  479 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  480 | `			"preg_match_all() expects at least 2 parameters");` |
|   ! 0 |  481 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  482 | `		return PH7_OK;` |
|     - |  483 | `	}` |
|    33 |  484 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    33 |  485 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    33 |  486 | `	if( nArg >= 4 ){` |
|     5 |  487 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     2 |  488 | `	}` |
|    33 |  489 | `	if( nArg >= 5 ){` |
|   ! 0 |  490 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  491 | `	}` |
|    33 |  492 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    33 |  493 | `	if( pCode == 0 ){` |
|   ! 0 |  494 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  495 | `		return PH7_OK;` |
|     - |  496 | `	}` |
|    33 |  497 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    33 |  498 | `	if( pMatchData == 0 ){` |
|   ! 0 |  499 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  500 | `		return PH7_OK;` |
|     - |  501 | `	}` |
|    33 |  502 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  503 | `	{` |
|    33 |  504 | `		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;` |
|     - |  505 |  |
|    33 |  506 | `		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){` |
|    13 |  507 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  508 | `				PCRE2_SIZE *ovector;` |
|    19 |  509 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     6 |  510 | `					startOffset, 0, pMatchData, NULL);` |
|    13 |  511 | `				if( rc < 0 ){` |
|     5 |  512 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     5 |  513 | `					break;` |
|     - |  514 | `				}` |
|     9 |  515 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     9 |  516 | `				if( pOutArray ){` |
|     9 |  517 | `					ph7_value *pSet = ph7_context_new_array(pCtx);` |
|     9 |  518 | `					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, iFlags & ~0xFF);` |
|     9 |  519 | `					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);` |
|     9 |  520 | `					ph7_context_release_value(pCtx, pSet);` |
|     4 |  521 | `				}` |
|     9 |  522 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  523 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  524 | `				}else{` |
|     9 |  525 | `					startOffset = ovector[1];` |
|     - |  526 | `				}` |
|     9 |  527 | `				totalMatches++;` |
|     1 |  528 | `			}` |
|     3 |  529 | `		}else{` |
|     - |  530 | `			/* PREG_PATTERN_ORDER (default) */` |
|    29 |  531 | `			ph7_value **apGroupArrays = 0;` |
|    29 |  532 | `			sxu32 nGroups = nCapture + 1;` |
|     - |  533 | `			sxu32 g;` |
|    29 |  534 | `			if( pOutArray ){` |
|    43 |  535 | `				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,` |
|    14 |  536 | `					sizeof(ph7_value *) * nGroups, TRUE, FALSE);` |
|    29 |  537 | `				if( apGroupArrays ){` |
|    85 |  538 | `					for( g = 0; g < nGroups; g++ ){` |
|    57 |  539 | `						apGroupArrays[g] = ph7_context_new_array(pCtx);` |
|    29 |  540 | `					}` |
|    14 |  541 | `				}` |
|    14 |  542 | `			}` |
|    81 |  543 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  544 | `				PCRE2_SIZE *ovector;` |
|   121 |  545 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    40 |  546 | `					startOffset, 0, pMatchData, NULL);` |
|    81 |  547 | `				if( rc < 0 ){` |
|    29 |  548 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|    29 |  549 | `					break;` |
|     - |  550 | `				}` |
|    53 |  551 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    53 |  552 | `				if( apGroupArrays ){` |
|    53 |  553 | `					ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    53 |  554 | `					int nActual = rc;` |
|   155 |  555 | `					for( g = 0; g < nGroups; g++ ){` |
|   153 |  556 | `						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){` |
|   101 |  557 | `							PCRE2_SIZE s = ovector[2*g];` |
|   101 |  558 | `							PCRE2_SIZE e = ovector[2*g+1];` |
|   101 |  559 | `							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  560 | `								ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  561 | `								ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  562 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|   ! 0 |  563 | `								ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  564 | `								ph7_value_int(pOff, (int)s);` |
|   ! 0 |  565 | `								ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  566 | `								ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|   ! 0 |  567 | `								ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  568 | `								ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  569 | `							}else{` |
|   101 |  570 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|   101 |  571 | `								ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  572 | `							}` |
|    51 |  573 | `						}else{` |
|     3 |  574 | `							ph7_value_string(pVal, "", 0);` |
|     3 |  575 | `							ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  576 | `						}` |
|   103 |  577 | `						ph7_value_reset_string_cursor(pVal);` |
|    52 |  578 | `					}` |
|    53 |  579 | `					ph7_context_release_value(pCtx, pVal);` |
|    26 |  580 | `				}` |
|    53 |  581 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  582 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  583 | `				}else{` |
|    53 |  584 | `					startOffset = ovector[1];` |
|     - |  585 | `				}` |
|    53 |  586 | `				totalMatches++;` |
|     1 |  587 | `			}` |
|    29 |  588 | `			if( apGroupArrays ){` |
|     - |  589 | `				/* Attach the per-group match arrays. php's PREG_PATTERN_ORDER stores a` |
|     - |  590 | `				 * named group under BOTH its name and its number, interleaved` |
|     - |  591 | ``				 * (`0, name, 1, value, 2`) — the same value under each key. Read the`` |
|     - |  592 | `				 * name table so each numbered group can emit its named alias first. */` |
|    29 |  593 | `				uint32_t namecount = 0, nameentrysize = 0;` |
|    29 |  594 | `				PCRE2_SPTR nametable = 0;` |
|    29 |  595 | `				pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|    29 |  596 | `				if( namecount > 0 ){` |
|     3 |  597 | `					pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     3 |  598 | `					pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|     1 |  599 | `				}` |
|    85 |  600 | `				for( g = 0; g < nGroups; g++ ){` |
|    57 |  601 | `					const char *zName = 0;` |
|    57 |  602 | `					if( namecount > 0 ){` |
|     - |  603 | `						uint32_t k;` |
|    13 |  604 | `						for( k = 0; k < namecount; k++ ){` |
|    11 |  605 | `							PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|    11 |  606 | `							if( (uint32_t)(((entry[0] << 8) \| entry[1])) == g ){` |
|     5 |  607 | `								zName = (const char *)(entry + 2);` |
|     5 |  608 | `								break;` |
|     - |  609 | `							}` |
|     4 |  610 | `						}` |
|     3 |  611 | `					}` |
|    57 |  612 | `					if( zName ){` |
|     5 |  613 | `						ph7_array_add_strkey_elem(pOutArray, zName, apGroupArrays[g]);` |
|     2 |  614 | `					}` |
|    57 |  615 | `					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);` |
|    57 |  616 | `					ph7_context_release_value(pCtx, apGroupArrays[g]);` |
|    29 |  617 | `				}` |
|    29 |  618 | `				ph7_context_free_chunk(pCtx, apGroupArrays);` |
|    14 |  619 | `			}` |
|     - |  620 | `		}` |
|     - |  621 | `		/* Write output array to caller's variable */` |
|    33 |  622 | `		if( pOutArray && nArg >= 3 ){` |
|    33 |  623 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pOutArray);` |
|    33 |  624 | `			ph7_context_release_value(pCtx, pOutArray);` |
|    16 |  625 | `		}` |
|     - |  626 | `	}` |
|    33 |  627 | `	pcre2_match_data_free(pMatchData);` |
|    33 |  628 | `	ph7_result_int(pCtx, totalMatches);` |
|    33 |  629 | `	return PH7_OK;` |
|    17 |  630 | `}` |
|     - |  631 |  |
|     - |  632 | `/* ======================================================================` |
|     - |  633 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|     - |  634 | ` * ====================================================================== */` |
|     6 |  635 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  636 | `{` |
|     - |  637 | `	const char *zPattern, *zSubject;` |
|     - |  638 | `	int nPatLen, nSubLen;` |
|     - |  639 | `	pcre2_code *pCode;` |
|     - |  640 | `	pcre2_match_data *pMatchData;` |
|     - |  641 | `	sxu32 nCapture;` |
|     - |  642 | `	ph7_value *pArray;` |
|     - |  643 | `	ph7_value *pVal;` |
|     7 |  644 | `	PCRE2_SIZE startOffset = 0, lastOffset = 0;` |
|     7 |  645 | `	int limit = -1;` |
|     7 |  646 | `	int iFlags = 0;` |
|     7 |  647 | `	int nPieces = 0;` |
|     - |  648 | `	int rc;` |
|     - |  649 |  |
|     7 |  650 | `	if( nArg < 2 ){` |
|   ! 0 |  651 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  652 | `			"preg_split() expects at least 2 parameters");` |
|   ! 0 |  653 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  654 | `		return PH7_OK;` |
|     - |  655 | `	}` |
|     7 |  656 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     7 |  657 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     7 |  658 | `	if( nArg >= 3 ){` |
|     5 |  659 | `		limit = ph7_value_to_int(apArg[2]);` |
|     2 |  660 | `	}` |
|     7 |  661 | `	if( nArg >= 4 ){` |
|     3 |  662 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     1 |  663 | `	}` |
|     7 |  664 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     7 |  665 | `	if( pCode == 0 ){` |
|   ! 0 |  666 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  667 | `		return PH7_OK;` |
|     - |  668 | `	}` |
|     7 |  669 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     7 |  670 | `	if( pMatchData == 0 ){` |
|   ! 0 |  671 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  672 | `		return PH7_OK;` |
|     - |  673 | `	}` |
|     7 |  674 | `	pArray = ph7_context_new_array(pCtx);` |
|     7 |  675 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     7 |  676 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  677 |  |
|    19 |  678 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|    19 |  679 | `		if( limit > 0 && nPieces >= limit - 1 ){` |
|     3 |  680 | `			break; /* Last piece gets the remainder */` |
|     - |  681 | `		}` |
|    25 |  682 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     8 |  683 | `			startOffset, 0, pMatchData, NULL);` |
|    17 |  684 | `		if( rc < 0 ){` |
|     5 |  685 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  686 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  687 | `			}` |
|     5 |  688 | `			break;` |
|     - |  689 | `		}` |
|     - |  690 | `		{` |
|    13 |  691 | `			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    13 |  692 | `			PCRE2_SIZE matchStart = ovector[0];` |
|    13 |  693 | `			PCRE2_SIZE matchEnd = ovector[1];` |
|    13 |  694 | `			int pieceLen = (int)(matchStart - lastOffset);` |
|     - |  695 |  |
|     - |  696 | `			/* Add the piece before the match */` |
|    13 |  697 | `			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| pieceLen > 0 ){` |
|    13 |  698 | `				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  699 | `					ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  700 | `					ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  701 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|   ! 0 |  702 | `					ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  703 | `					ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  704 | `					ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  705 | `					ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  706 | `					ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  707 | `					ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  708 | `				}else{` |
|    13 |  709 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|    13 |  710 | `					ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  711 | `				}` |
|    13 |  712 | `				ph7_value_reset_string_cursor(pVal);` |
|    13 |  713 | `				nPieces++;` |
|     6 |  714 | `			}` |
|     - |  715 | `			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */` |
|    13 |  716 | `			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){` |
|     - |  717 | `				int g;` |
|   ! 0 |  718 | `				for( g = 1; g < rc; g++ ){` |
|   ! 0 |  719 | `					PCRE2_SIZE gs = ovector[2*g];` |
|   ! 0 |  720 | `					PCRE2_SIZE ge = ovector[2*g+1];` |
|     - |  721 | `					int gLen;` |
|   ! 0 |  722 | `					if( gs == PCRE2_UNSET ) continue;` |
|   ! 0 |  723 | `					gLen = (int)(ge - gs);` |
|   ! 0 |  724 | `					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| gLen > 0 ){` |
|   ! 0 |  725 | `						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  726 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  727 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  728 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  729 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  730 | `							ph7_value_int(pOff, (int)gs);` |
|   ! 0 |  731 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  732 | `							ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  733 | `							ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  734 | `							ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  735 | `						}else{` |
|   ! 0 |  736 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  737 | `							ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  738 | `						}` |
|   ! 0 |  739 | `						ph7_value_reset_string_cursor(pVal);` |
|   ! 0 |  740 | `					}` |
|   ! 0 |  741 | `				}` |
|   ! 0 |  742 | `			}` |
|     - |  743 | `			/* Advance */` |
|    13 |  744 | `			lastOffset = matchEnd;` |
|    13 |  745 | `			if( matchEnd == matchStart ){` |
|   ! 0 |  746 | `				startOffset = matchEnd + 1;` |
|   ! 0 |  747 | `			}else{` |
|    13 |  748 | `				startOffset = matchEnd;` |
|     - |  749 | `			}` |
|     - |  750 | `		}` |
|     1 |  751 | `	}` |
|     - |  752 | `	/* Add trailing piece */` |
|     - |  753 | `	{` |
|     7 |  754 | `		int trailLen = nSubLen - (int)lastOffset;` |
|     7 |  755 | `		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| trailLen > 0 ){` |
|     7 |  756 | `			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  757 | `				ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  758 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  759 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|   ! 0 |  760 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  761 | `				ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  762 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  763 | `				ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  764 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  765 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  766 | `			}else{` |
|     7 |  767 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|     7 |  768 | `				ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  769 | `			}` |
|     3 |  770 | `		}` |
|     - |  771 | `	}` |
|     7 |  772 | `	ph7_context_release_value(pCtx, pVal);` |
|     7 |  773 | `	pcre2_match_data_free(pMatchData);` |
|     7 |  774 | `	ph7_result_value(pCtx, pArray);` |
|     7 |  775 | `	ph7_context_release_value(pCtx, pArray);` |
|     7 |  776 | `	return PH7_OK;` |
|     4 |  777 | `}` |
|     - |  778 |  |
|     - |  779 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|   120 |  780 | `static void PcreExpandBackrefs(` |
|     - |  781 | `	SyBlob *pOut,` |
|     - |  782 | `	const char *zRepl, int nReplLen,` |
|     - |  783 | `	const char *zSubject,` |
|     - |  784 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     2 |  785 | `{` |
|   122 |  786 | `	const char *zEnd = &zRepl[nReplLen];` |
|   122 |  787 | `	const char *z = zRepl;` |
|     - |  788 |  |
|   258 |  789 | `	while( z < zEnd ){` |
|   138 |  790 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   ! 0 |  791 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|   ! 0 |  792 | `				int g = z[1] - '0';` |
|   ! 0 |  793 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  794 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  795 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  796 | `				}` |
|   ! 0 |  797 | `				z += 2;` |
|   ! 0 |  798 | `				continue;` |
|     - |  799 | `			}` |
|   ! 0 |  800 | `			if( z[1] == '\\' ){` |
|   ! 0 |  801 | `				SyBlobAppend(pOut, "\\", 1);` |
|   ! 0 |  802 | `				z += 2;` |
|   ! 0 |  803 | `				continue;` |
|     - |  804 | `			}` |
|     - |  805 | `			/* Not a backreference — emit literally */` |
|   ! 0 |  806 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  807 | `			z++;` |
|   ! 0 |  808 | `			continue;` |
|     - |  809 | `		}` |
|   138 |  810 | `		if( *z == '$' && z + 1 < zEnd ){` |
|    13 |  811 | `			if( z[1] == '$' ){` |
|   ! 0 |  812 | `				SyBlobAppend(pOut, "$", 1);` |
|   ! 0 |  813 | `				z += 2;` |
|   ! 0 |  814 | `				continue;` |
|     - |  815 | `			}` |
|    13 |  816 | `			if( z[1] == '{' ){` |
|     - |  817 | `				/* ${N} form */` |
|   ! 0 |  818 | `				const char *p = z + 2;` |
|   ! 0 |  819 | `				int g = 0;` |
|   ! 0 |  820 | `				while( p < zEnd && *p >= '0' && *p <= '9' ){` |
|   ! 0 |  821 | `					g = g * 10 + (*p - '0');` |
|   ! 0 |  822 | `					p++;` |
|   ! 0 |  823 | `				}` |
|   ! 0 |  824 | `				if( p < zEnd && *p == '}' ){` |
|   ! 0 |  825 | `					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  826 | `						SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  827 | `							(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  828 | `					}` |
|   ! 0 |  829 | `					z = p + 1;` |
|   ! 0 |  830 | `					continue;` |
|     - |  831 | `				}` |
|     - |  832 | `				/* Not a valid ${N} — emit literally */` |
|   ! 0 |  833 | `				SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  834 | `				z++;` |
|   ! 0 |  835 | `				continue;` |
|     - |  836 | `			}` |
|    13 |  837 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|     - |  838 | `				/* $N or $NN */` |
|    13 |  839 | `				int g = z[1] - '0';` |
|    13 |  840 | `				z += 2;` |
|     - |  841 | `				/* Check for second digit */` |
|    13 |  842 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  843 | `					int g2 = g * 10 + (*z - '0');` |
|   ! 0 |  844 | `					if( g2 < nGroups ){` |
|   ! 0 |  845 | `						g = g2;` |
|   ! 0 |  846 | `						z++;` |
|   ! 0 |  847 | `					}` |
|   ! 0 |  848 | `				}` |
|    13 |  849 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|    19 |  850 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|    12 |  851 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|     6 |  852 | `				}` |
|    13 |  853 | `				continue;` |
|     - |  854 | `			}` |
|     - |  855 | `			/* Not a backreference */` |
|   ! 0 |  856 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  857 | `			z++;` |
|   ! 0 |  858 | `			continue;` |
|     - |  859 | `		}` |
|   126 |  860 | `		SyBlobAppend(pOut, z, 1);` |
|   126 |  861 | `		z++;` |
|     2 |  862 | `	}` |
|   122 |  863 | `}` |
|     - |  864 |  |
|     - |  865 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|    84 |  866 | `static void PcreDoReplace(` |
|     - |  867 | `	ph7_context *pCtx,` |
|     - |  868 | `	pcre2_code *pCode,` |
|     - |  869 | `	const char *zSubject, int nSubLen,` |
|     - |  870 | `	const char *zRepl, int nReplLen,` |
|     - |  871 | `	int limit,` |
|     - |  872 | `	int *pCount,` |
|     - |  873 | `	SyBlob *pOut)` |
|     2 |  874 | `{` |
|     - |  875 | `	pcre2_match_data *pMatchData;` |
|    86 |  876 | `	PCRE2_SIZE startOffset = 0;` |
|    86 |  877 | `	int nReplacements = 0;` |
|     - |  878 | `	int rc;` |
|     - |  879 |  |
|    86 |  880 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    86 |  881 | `	if( pMatchData == 0 ) return;` |
|     - |  882 |  |
|   206 |  883 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  884 | `		PCRE2_SIZE *ovector;` |
|   204 |  885 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|   305 |  886 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|   101 |  887 | `			startOffset, 0, pMatchData, NULL);` |
|   204 |  888 | `		if( rc < 0 ){` |
|    84 |  889 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  890 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  891 | `			}` |
|    84 |  892 | `			break;` |
|     - |  893 | `		}` |
|   122 |  894 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - |  895 | `		/* Copy text before match */` |
|   122 |  896 | `		if( ovector[0] > startOffset ){` |
|    79 |  897 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|    39 |  898 | `		}` |
|     - |  899 | `		/* Expand replacement */` |
|   122 |  900 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|   122 |  901 | `		nReplacements++;` |
|     - |  902 | `		/* Advance */` |
|   122 |  903 | `		if( ovector[1] == ovector[0] ){` |
|     - |  904 | `			/* Zero-width match: to make progress, emit the character AT THE MATCH` |
|     - |  905 | `			 * POSITION (ovector[0]) and step past it. The match can sit AHEAD of the` |
|     - |  906 | `			 * search start (a lookbehind/lookahead assertion, e.g. the camelCase` |
|     - |  907 | `			 * split /(?<=[[:lower:]])(?=[[:upper:]])/), so copying zSubject[startOffset]` |
|     - |  908 | `			 * grabbed the wrong byte ("fooBar" -> "foo far"). The text between` |
|     - |  909 | `			 * startOffset and ovector[0] was already copied above. */` |
|    23 |  910 | `			if( ovector[0] < (PCRE2_SIZE)nSubLen ){` |
|    21 |  911 | `				SyBlobAppend(pOut, &zSubject[ovector[0]], 1);` |
|    10 |  912 | `			}` |
|    23 |  913 | `			startOffset = ovector[0] + 1;` |
|    12 |  914 | `		}else{` |
|   100 |  915 | `			startOffset = ovector[1];` |
|     - |  916 | `		}` |
|     2 |  917 | `	}` |
|     - |  918 | `	/* Copy remainder */` |
|    86 |  919 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|    32 |  920 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|    15 |  921 | `	}` |
|    86 |  922 | `	if( pCount ){` |
|    86 |  923 | `		*pCount += nReplacements;` |
|    42 |  924 | `	}` |
|    86 |  925 | `	pcre2_match_data_free(pMatchData);` |
|    42 |  926 | `	SXUNUSED(pCtx);` |
|    44 |  927 | `}` |
|     - |  928 |  |
|     - |  929 | `/* ===== Helper: apply pattern(s)+replacement(s) to ONE subject string =====` |
|     - |  930 | ` * pPattern is a string or an array of patterns; pRepl is a string (used for` |
|     - |  931 | ` * every pattern) or, only when pPattern is an array, an array taken by ORDER` |
|     - |  932 | ` * (missing element -> ""). Array patterns are applied sequentially, each to the` |
|     - |  933 | ` * result of the previous (PHP semantics), ping-ponging two blobs. The final` |
|     - |  934 | ` * text is appended to pOut. Returns SXRET_OK, or SXERR_ABORT on a bad pattern` |
|     - |  935 | ` * (the caller then yields NULL, matching the scalar path). */` |
|    76 |  936 | `static sxi32 PcreReplaceSubject(` |
|     - |  937 | `	ph7_context *pCtx,` |
|     - |  938 | `	ph7_value *pPattern,` |
|     - |  939 | `	ph7_value *pRepl,` |
|     - |  940 | `	const char *zSubject, int nSubLen,` |
|     - |  941 | `	int limit,` |
|     - |  942 | `	int *pCount,` |
|     - |  943 | `	SyBlob *pOut)` |
|     2 |  944 | `{` |
|     - |  945 | `	sxu32 nCapture;` |
|    78 |  946 | `	if( !ph7_value_is_array(pPattern) ){` |
|     - |  947 | `		/* Single pattern + single replacement */` |
|     - |  948 | `		const char *zPattern, *zRepl;` |
|     - |  949 | `		int nPatLen, nReplLen;` |
|     - |  950 | `		pcre2_code *pCode;` |
|    66 |  951 | `		zPattern = ph7_value_to_string(pPattern, &nPatLen);` |
|    66 |  952 | `		zRepl = ph7_value_to_string(pRepl, &nReplLen);` |
|    66 |  953 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    66 |  954 | `		if( pCode == 0 ){` |
|   ! 0 |  955 | `			return SXERR_ABORT;` |
|     - |  956 | `		}` |
|    66 |  957 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, pCount, pOut);` |
|    66 |  958 | `		return SXRET_OK;` |
|   ! 0 |  959 | `	}else{` |
|     - |  960 | `		/* Array of patterns: apply each in insertion order to the accumulating` |
|     - |  961 | `		 * subject. Replacement is the parallel array element (by order) or the` |
|     - |  962 | `		 * scalar replacement for every pattern. */` |
|    13 |  963 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|    13 |  964 | `		ph7_hashmap *pRepMap = ph7_value_is_array(pRepl) ? (ph7_hashmap *)pRepl->x.pOther : 0;` |
|    13 |  965 | `		const char *zScalarRepl = 0;` |
|    13 |  966 | `		int nScalarRepl = 0;` |
|     - |  967 | `		ph7_hashmap_node *pPatNode, *pRepNode;` |
|     - |  968 | `		ph7_value sPat, sRep;` |
|     - |  969 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|     - |  970 | `		sxu32 n;` |
|    13 |  971 | `		sxi32 rc = SXRET_OK;` |
|    13 |  972 | `		if( pRepMap == 0 ){` |
|     5 |  973 | `			zScalarRepl = ph7_value_to_string(pRepl, &nScalarRepl);` |
|     2 |  974 | `		}` |
|    13 |  975 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|    13 |  976 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|    13 |  977 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|    13 |  978 | `		pSrc = &sA; pDst = &sB;` |
|    13 |  979 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|    13 |  980 | `		PH7_MemObjInit(pCtx->pVm, &sRep);` |
|    13 |  981 | `		pPatNode = pPatMap->pFirst;` |
|    13 |  982 | `		pRepNode = pRepMap ? pRepMap->pFirst : 0;` |
|    13 |  983 | `		n = pPatMap->nEntry;` |
|    33 |  984 | `		while( n > 0 ){` |
|     - |  985 | `			const char *zPattern, *zRepl;` |
|     - |  986 | `			int nPatLen, nReplLen;` |
|     - |  987 | `			pcre2_code *pCode;` |
|     - |  988 | `			SyBlob *pSwap;` |
|    21 |  989 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|    21 |  990 | `			zPattern = ph7_value_to_string(&sPat, &nPatLen);` |
|    21 |  991 | `			if( pRepMap ){` |
|    17 |  992 | `				if( pRepNode ){` |
|    15 |  993 | `					PH7_HashmapExtractNodeValue(pRepNode, &sRep, FALSE);` |
|    15 |  994 | `					zRepl = ph7_value_to_string(&sRep, &nReplLen);` |
|     8 |  995 | `				}else{` |
|     3 |  996 | `					zRepl = ""; nReplLen = 0;` |
|     - |  997 | `				}` |
|     9 |  998 | `			}else{` |
|     5 |  999 | `				zRepl = zScalarRepl; nReplLen = nScalarRepl;` |
|     - | 1000 | `			}` |
|    21 | 1001 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    21 | 1002 | `			if( pCode == 0 ){` |
|   ! 0 | 1003 | `				rc = SXERR_ABORT;` |
|   ! 0 | 1004 | `				PH7_MemObjRelease(&sPat);` |
|   ! 0 | 1005 | `				if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|   ! 0 | 1006 | `				break;` |
|     - | 1007 | `			}` |
|    21 | 1008 | `			SyBlobReset(pDst);` |
|    31 | 1009 | `			PcreDoReplace(pCtx, pCode,` |
|    20 | 1010 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|    10 | 1011 | `				zRepl, nReplLen, limit, pCount, pDst);` |
|     - | 1012 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|    21 | 1013 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|    21 | 1014 | `			PH7_MemObjRelease(&sPat);` |
|    21 | 1015 | `			if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|    21 | 1016 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|    21 | 1017 | `			if( pRepNode ){ pRepNode = pRepNode->pPrev; }` |
|    21 | 1018 | `			n--;` |
|     1 | 1019 | `		}` |
|    13 | 1020 | `		if( rc == SXRET_OK ){` |
|    13 | 1021 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|     6 | 1022 | `		}` |
|    13 | 1023 | `		SyBlobRelease(&sA);` |
|    13 | 1024 | `		SyBlobRelease(&sB);` |
|    13 | 1025 | `		return rc;` |
|     - | 1026 | `	}` |
|    40 | 1027 | `}` |
|     - | 1028 |  |
|     - | 1029 | `/* ======================================================================` |
|     - | 1030 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - | 1031 | ` * ====================================================================== */` |
|    60 | 1032 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1033 | `{` |
|    62 | 1034 | `	int limit = -1;` |
|    62 | 1035 | `	int count = 0;` |
|     - | 1036 |  |
|    62 | 1037 | `	if( nArg < 3 ){` |
|   ! 0 | 1038 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1039 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 | 1040 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1041 | `		return PH7_OK;` |
|     - | 1042 | `	}` |
|    62 | 1043 | `	if( nArg >= 4 ){` |
|    20 | 1044 | `		limit = ph7_value_to_int(apArg[3]);` |
|     9 | 1045 | `	}` |
|    62 | 1046 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1047 |  |
|     - | 1048 | `	/* A scalar pattern with an array replacement is a parameter mismatch (PHP` |
|     - | 1049 | `	 * throws a TypeError; PHL keeps preg_replace's warning-based arg-error style). */` |
|    62 | 1050 | `	if( !ph7_value_is_array(apArg[0]) && ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 1051 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1052 | `			"preg_replace(): Parameter mismatch, pattern is a string while replacement is an array");` |
|   ! 0 | 1053 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1054 | `		return PH7_OK;` |
|     - | 1055 | `	}` |
|    92 | 1056 | `	if( ph7_value_is_array(apArg[2]) ){` |
|     - | 1057 | `		/* Array subject: return an array, each element replaced, keys preserved. */` |
|    15 | 1058 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|    15 | 1059 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|    15 | 1060 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     - | 1061 | `		ph7_value sKey, sVal;` |
|     - | 1062 | `		ph7_hashmap_node *pNode;` |
|     - | 1063 | `		sxu32 n;` |
|    15 | 1064 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|   ! 0 | 1065 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1066 | `			return PH7_OK;` |
|     - | 1067 | `		}` |
|    15 | 1068 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|    15 | 1069 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    15 | 1070 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|    15 | 1071 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|    45 | 1072 | `		while( n > 0 ){` |
|     - | 1073 | `			const char *zSubject;` |
|     - | 1074 | `			int nSubLen;` |
|     - | 1075 | `			SyBlob sOut;` |
|    31 | 1076 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|    31 | 1077 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|    31 | 1078 | `			zSubject = ph7_value_to_string(&sVal, &nSubLen);` |
|    31 | 1079 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    31 | 1080 | `			if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1081 | `				/* A bad pattern with an array subject yields an empty array (PHP);` |
|     - | 1082 | `				 * the failure hits the first element, so pResult is still empty. */` |
|   ! 0 | 1083 | `				SyBlobRelease(&sOut);` |
|   ! 0 | 1084 | `				PH7_MemObjRelease(&sKey);` |
|   ! 0 | 1085 | `				PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1086 | `				ph7_result_value(pCtx, pResult);` |
|   ! 0 | 1087 | `				goto set_count;` |
|     - | 1088 | `			}` |
|    31 | 1089 | `			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    31 | 1090 | `			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|    31 | 1091 | `			ph7_value_reset_string_cursor(pElem);` |
|    31 | 1092 | `			SyBlobRelease(&sOut);` |
|    31 | 1093 | `			PH7_MemObjRelease(&sKey);` |
|    31 | 1094 | `			PH7_MemObjRelease(&sVal);` |
|    31 | 1095 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|    31 | 1096 | `			n--;` |
|     1 | 1097 | `		}` |
|    15 | 1098 | `		ph7_result_value(pCtx, pResult);` |
|     8 | 1099 | `	}else{` |
|     - | 1100 | `		/* Scalar subject: one replaced string. */` |
|     - | 1101 | `		const char *zSubject;` |
|     - | 1102 | `		int nSubLen;` |
|     - | 1103 | `		SyBlob sOut;` |
|    48 | 1104 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    48 | 1105 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    48 | 1106 | `		if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1107 | `			/* Scalar subject: a bad pattern returns NULL (PHP). */` |
|   ! 0 | 1108 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1109 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1110 | `			goto set_count;` |
|     - | 1111 | `		}` |
|    48 | 1112 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    48 | 1113 | `		SyBlobRelease(&sOut);` |
|     - | 1114 | `	}` |
|    30 | 1115 | `set_count:` |
|     - | 1116 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|     - | 1117 | `	 * (PHP always writes it: 0, or the count accumulated by earlier good patterns). */` |
|    62 | 1118 | `	if( nArg >= 5 ){` |
|     - | 1119 | `		ph7_value sCount;` |
|    20 | 1120 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|    20 | 1121 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|    20 | 1122 | `		PH7_MemObjRelease(&sCount);` |
|     9 | 1123 | `	}` |
|    62 | 1124 | `	return PH7_OK;` |
|    32 | 1125 | `}` |
|     - | 1126 |  |
|     - | 1127 | `/* ======================================================================` |
|     - | 1128 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count]])` |
|     - | 1129 | ` * ====================================================================== */` |
|    12 | 1130 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1131 | `{` |
|     - | 1132 | `	const char *zPattern, *zSubject;` |
|     - | 1133 | `	int nPatLen, nSubLen;` |
|     - | 1134 | `	pcre2_code *pCode;` |
|     - | 1135 | `	pcre2_match_data *pMatchData;` |
|     - | 1136 | `	sxu32 nCapture;` |
|     - | 1137 | `	SyBlob sOut;` |
|    15 | 1138 | `	PCRE2_SIZE startOffset = 0;` |
|    15 | 1139 | `	int limit = -1;` |
|    15 | 1140 | `	int count = 0;` |
|     - | 1141 | `	int rc;` |
|     - | 1142 |  |
|    15 | 1143 | `	if( nArg < 3 ){` |
|   ! 0 | 1144 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1145 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 | 1146 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1147 | `		return PH7_OK;` |
|     - | 1148 | `	}` |
|    15 | 1149 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    15 | 1150 | `	zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    15 | 1151 | `	if( nArg >= 4 ){` |
|     8 | 1152 | `		limit = ph7_value_to_int(apArg[3]);` |
|     3 | 1153 | `	}` |
|    15 | 1154 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 | 1155 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1156 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 | 1157 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1158 | `		return PH7_OK;` |
|     - | 1159 | `	}` |
|    15 | 1160 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    15 | 1161 | `	if( pCode == 0 ){` |
|   ! 0 | 1162 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1163 | `		return PH7_OK;` |
|     - | 1164 | `	}` |
|    15 | 1165 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    15 | 1166 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1167 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1168 | `		return PH7_OK;` |
|     - | 1169 | `	}` |
|    15 | 1170 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    15 | 1171 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1172 |  |
|    37 | 1173 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1174 | `		PCRE2_SIZE *ovector;` |
|     - | 1175 | `		ph7_value *pMatchArr;` |
|     - | 1176 | `		ph7_value *apCbArg[1];` |
|     - | 1177 | `		ph7_value sResult;` |
|     - | 1178 | `		const char *zReplacement;` |
|     - | 1179 | `		int nReplLen;` |
|     - | 1180 |  |
|    43 | 1181 | `		if( limit >= 0 && count >= limit ) break;` |
|    54 | 1182 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    17 | 1183 | `			startOffset, 0, pMatchData, NULL);` |
|    37 | 1184 | `		if( rc < 0 ){` |
|    15 | 1185 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1186 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1187 | `			}` |
|    15 | 1188 | `			break;` |
|     - | 1189 | `		}` |
|    25 | 1190 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1191 | `		/* Copy text before match */` |
|    25 | 1192 | `		if( ovector[0] > startOffset ){` |
|    17 | 1193 | `			SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     7 | 1194 | `		}` |
|     - | 1195 | `		/* Build matches array for callback */` |
|    25 | 1196 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    25 | 1197 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, 0);` |
|     - | 1198 | `		/* Call the callback */` |
|    25 | 1199 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    25 | 1200 | `		apCbArg[0] = pMatchArr;` |
|    25 | 1201 | `		if( PH7_VmCallUserFunction(pCtx->pVm, apArg[1], 1, apCbArg, &sResult) == PH7_EXCEPTION ){` |
|     - | 1202 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|   ! 0 | 1203 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1204 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|   ! 0 | 1205 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1206 | `			pcre2_match_data_free(pMatchData);` |
|   ! 0 | 1207 | `			return PH7_EXCEPTION;` |
|     - | 1208 | `		}` |
|     - | 1209 | `		/* Get replacement string from callback result */` |
|    25 | 1210 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    25 | 1211 | `		SyBlobAppend(&sOut, zReplacement, (sxu32)nReplLen);` |
|    25 | 1212 | `		PH7_MemObjRelease(&sResult);` |
|    25 | 1213 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    25 | 1214 | `		count++;` |
|     - | 1215 | `		/* Advance */` |
|    25 | 1216 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 | 1217 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1218 | `				SyBlobAppend(&sOut, &zSubject[startOffset], 1);` |
|   ! 0 | 1219 | `			}` |
|   ! 0 | 1220 | `			startOffset = ovector[0] + 1;` |
|   ! 0 | 1221 | `		}else{` |
|    25 | 1222 | `			startOffset = ovector[1];` |
|     - | 1223 | `		}` |
|     3 | 1224 | `	}` |
|     - | 1225 | `	/* Copy remainder */` |
|    15 | 1226 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|     5 | 1227 | `		SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|     2 | 1228 | `	}` |
|    15 | 1229 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    15 | 1230 | `	SyBlobRelease(&sOut);` |
|    15 | 1231 | `	pcre2_match_data_free(pMatchData);` |
|     - | 1232 | `	/* Set &$count if provided */` |
|    15 | 1233 | `	if( nArg >= 5 ){` |
|     - | 1234 | `		ph7_value sCount;` |
|     3 | 1235 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     3 | 1236 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|     3 | 1237 | `		PH7_MemObjRelease(&sCount);` |
|     1 | 1238 | `	}` |
|    15 | 1239 | `	return PH7_OK;` |
|     9 | 1240 | `}` |
|     - | 1241 |  |
|     - | 1242 | `/* ======================================================================` |
|     - | 1243 | ` * preg_quote(str [, delimiter])` |
|     - | 1244 | ` * ====================================================================== */` |
|     8 | 1245 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1246 | `{` |
|     9 | 1247 | `	const char *zStr, *zDelim = 0;` |
|     9 | 1248 | `	int nLen, nDelimLen = 0;` |
|     - | 1249 | `	const char *z, *zEnd;` |
|     - | 1250 |  |
|     9 | 1251 | `	if( nArg < 1 ){` |
|   ! 0 | 1252 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1253 | `		return PH7_OK;` |
|     - | 1254 | `	}` |
|     9 | 1255 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|     9 | 1256 | `	if( nArg >= 2 ){` |
|     5 | 1257 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     2 | 1258 | `	}` |
|     9 | 1259 | `	z = zStr;` |
|     9 | 1260 | `	zEnd = &zStr[nLen];` |
|    75 | 1261 | `	while( z < zEnd ){` |
|    67 | 1262 | `		char c = *z;` |
|    67 | 1263 | `		switch( c ){` |
|     5 | 1264 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1265 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1266 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1267 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1268 | `			case '#':` |
|    11 | 1269 | `				ph7_result_string(pCtx, "\\", 1);` |
|    11 | 1270 | `				break;` |
|    28 | 1271 | `			default:` |
|    57 | 1272 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     3 | 1273 | `					ph7_result_string(pCtx, "\\", 1);` |
|     1 | 1274 | `				}` |
|    56 | 1275 | `				break;` |
|     - | 1276 | `		}` |
|    67 | 1277 | `		ph7_result_string(pCtx, z, 1);` |
|    67 | 1278 | `		z++;` |
|     1 | 1279 | `	}` |
|     9 | 1280 | `	return PH7_OK;` |
|     5 | 1281 | `}` |
|     - | 1282 |  |
|     - | 1283 | `/* ======================================================================` |
|     - | 1284 | ` * preg_last_error()` |
|     - | 1285 | ` * ====================================================================== */` |
|   ! 0 | 1286 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1287 | `{` |
|   ! 0 | 1288 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1289 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1290 | `	return PH7_OK;` |
|   ! 0 | 1291 | `}` |
|     - | 1292 |  |
|     - | 1293 | `/* ======================================================================` |
|     - | 1294 | ` * preg_last_error_msg()` |
|     - | 1295 | ` * ====================================================================== */` |
|   ! 0 | 1296 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1297 | `{` |
|     - | 1298 | `	const char *zMsg;` |
|   ! 0 | 1299 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1300 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1301 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1302 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1303 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1304 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1305 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1306 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1307 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1308 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1309 | `	}` |
|   ! 0 | 1310 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1311 | `	return PH7_OK;` |
|   ! 0 | 1312 | `}` |
|     - | 1313 |  |
|     - | 1314 | `/* ===== Function registration table ===== */` |
|     - | 1315 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1316 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1317 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1318 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1319 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1320 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1321 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1322 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1323 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1324 | `};` |
|     - | 1325 |  |
|  3406 | 1326 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     5 | 1327 | `{` |
|     - | 1328 | `	sxu32 n;` |
| 30659 | 1329 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 27253 | 1330 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 13629 | 1331 | `	}` |
|  3411 | 1332 | `}` |
|     - | 1333 |  |
|     - | 1334 | `/* ===== Constant registration ===== */` |
|     - | 1335 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1336 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1337 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1338 | `	}` |
|     - | 1339 |  |
|   ! 0 | 1340 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|     5 | 1341 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|   ! 0 | 1342 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|   ! 0 | 1343 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|   ! 0 | 1344 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|   ! 0 | 1345 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|   ! 0 | 1346 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|   ! 0 | 1347 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|   ! 0 | 1348 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|   ! 0 | 1349 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|   ! 0 | 1350 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|    13 | 1351 | `PCRE_CONST_INT(PREG_GREP_INVERT,         PHP_PREG_GREP_INVERT)` |
|   ! 0 | 1352 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|   ! 0 | 1353 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|   ! 0 | 1354 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1355 |  |
|  3406 | 1356 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     5 | 1357 | `{` |
|  3411 | 1358 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  3411 | 1359 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  3411 | 1360 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  3411 | 1361 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  3411 | 1362 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  3411 | 1363 | `	ph7_create_constant(&(*pVm), "PREG_GREP_INVERT",          PcreConst_PREG_GREP_INVERT, 0);` |
|  3411 | 1364 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  3411 | 1365 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  3411 | 1366 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  3411 | 1367 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  3411 | 1368 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  3411 | 1369 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  3411 | 1370 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  3411 | 1371 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  3411 | 1372 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  3411 | 1373 | `}` |
|     - | 1374 |  |
|     - | 1375 | `#else` |
|     - | 1376 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1377 | `typedef int vm_pcre_unused;` |
|     - | 1378 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1379 |  |
