# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 807/1028 lines (78.50%)

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
|   438 |   56 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|     5 |   57 | `{` |
|     - |   58 | `	sxu32 i;` |
|  3311 |   59 | `	for( i = 0; i < nCacheUsed; i++ ){` |
|  3158 |   60 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|   290 |   61 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|   290 |   62 | `			if( pCaptureCount ){` |
|   290 |   63 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|   143 |   64 | `			}` |
|   290 |   65 | `			return aCache[i].pCode;` |
|     - |   66 | `		}` |
|  1438 |   67 | `	}` |
|   157 |   68 | `	return 0;` |
|   224 |   69 | `}` |
|     - |   70 |  |
|   124 |   71 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|     5 |   72 | `{` |
|     - |   73 | `	PcreCacheEntry *pEntry;` |
|     - |   74 | `	char *zCopy;` |
|     - |   75 | `	/* Allocate the pattern copy first, before touching the cache */` |
|   129 |   76 | `	zCopy = (char *)malloc(nLen + 1);` |
|   129 |   77 | `	if( zCopy == 0 ){` |
|     - |   78 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|   ! 0 |   79 | `		return;` |
|     - |   80 | `	}` |
|   129 |   81 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|   129 |   82 | `	zCopy[nLen] = 0;` |
|   129 |   83 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|    61 |   84 | `		pEntry = &aCache[nCacheUsed++];` |
|    33 |   85 | `	}else{` |
|     - |   86 | `		/* Evict LRU */` |
|    69 |   87 | `		sxu32 iMin = aCache[0].iLastUsed;` |
|    69 |   88 | `		sxu32 iMinIdx = 0;` |
|     - |   89 | `		sxu32 i;` |
|  1089 |   90 | `		for( i = 1; i < PCRE_CACHE_SIZE; i++ ){` |
|  1021 |   91 | `			if( aCache[i].iLastUsed < iMin ){` |
|   159 |   92 | `				iMin = aCache[i].iLastUsed;` |
|   159 |   93 | `				iMinIdx = i;` |
|    79 |   94 | `			}` |
|   511 |   95 | `		}` |
|    69 |   96 | `		pEntry = &aCache[iMinIdx];` |
|    69 |   97 | `		pcre2_code_free(pEntry->pCode);` |
|    69 |   98 | `		free(pEntry->zPattern);` |
|     - |   99 | `	}` |
|   129 |  100 | `	pEntry->zPattern = zCopy;` |
|   129 |  101 | `	pEntry->nLen = nLen;` |
|   129 |  102 | `	pEntry->pCode = pCode;` |
|   129 |  103 | `	pEntry->nCaptureCount = nCaptureCount;` |
|   129 |  104 | `	pEntry->iLastUsed = ++iCacheClock;` |
|    67 |  105 | `}` |
|     - |  106 |  |
|     - |  107 | `/* ===== Delimiter parser ===== */` |
|     - |  108 | `#define PCRE_PARSE_OK             0` |
|     - |  109 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|     - |  110 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */` |
|     - |  111 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|     - |  112 |  |
|   152 |  113 | `static sxi32 PcreParsePattern(` |
|     - |  114 | `	const char *zInput, int nInputLen,` |
|     - |  115 | `	const char **pPattern, int *pnPatternLen,` |
|     - |  116 | `	const char **pFlags, int *pnFlagLen,` |
|     - |  117 | `	char *pCloseDelim, int *pbPaired)` |
|     5 |  118 | `{` |
|   157 |  119 | `	const char *zEnd = &zInput[nInputLen];` |
|   157 |  120 | `	const char *z = zInput;` |
|     - |  121 | `	char cOpen, cClose;` |
|     - |  122 | `	const char *pStart;` |
|     - |  123 |  |
|     - |  124 | `	/* Delimiter details for a "no ending delimiter" diagnostic (php names it) */` |
|   157 |  125 | `	*pCloseDelim = 0;` |
|   157 |  126 | `	*pbPaired = 0;` |
|     - |  127 | `	/* Skip leading whitespace */` |
|   157 |  128 | `	while( z < zEnd && (unsigned char)*z <= 0x20 ){` |
|   ! 0 |  129 | `		z++;` |
|   ! 0 |  130 | `	}` |
|   157 |  131 | `	if( z >= zEnd ){` |
|   ! 0 |  132 | `		return PCRE_PARSE_EMPTY;` |
|     - |  133 | `	}` |
|   157 |  134 | `	cOpen = *z;` |
|     - |  135 | `	/* Must not be alphanumeric, backslash, or whitespace */` |
|   157 |  136 | `	if( SyisAlphaNum(cOpen) \|\| cOpen == '\\' \|\| (unsigned char)cOpen <= 0x20 ){` |
|     3 |  137 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|     - |  138 | `	}` |
|     - |  139 | `	/* Paired delimiters */` |
|   155 |  140 | `	switch( cOpen ){` |
|     5 |  141 | `		case '(': cClose = ')'; break;` |
|     3 |  142 | `		case '[': cClose = ']'; break;` |
|     3 |  143 | `		case '{': cClose = '}'; break;` |
|     3 |  144 | `		case '<': cClose = '>'; break;` |
|   145 |  145 | `		default:  cClose = cOpen; break;` |
|     - |  146 | `	}` |
|   155 |  147 | `	*pCloseDelim = cClose;` |
|   155 |  148 | `	*pbPaired = (cOpen != cClose);` |
|   155 |  149 | `	z++; /* Skip opening delimiter */` |
|   155 |  150 | `	pStart = z;` |
|     - |  151 | `	/* Scan for closing delimiter, respecting backslash escapes */` |
|  1701 |  152 | `	while( z < zEnd ){` |
|  1685 |  153 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   118 |  154 | `			z += 2; /* Skip escaped char */` |
|   118 |  155 | `			continue;` |
|     - |  156 | `		}` |
|  1569 |  157 | `		if( *z == cClose ){` |
|   139 |  158 | `			break;` |
|     - |  159 | `		}` |
|  1435 |  160 | `		z++;` |
|     5 |  161 | `	}` |
|   155 |  162 | `	if( z >= zEnd ){` |
|    17 |  163 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|     - |  164 | `	}` |
|   139 |  165 | `	*pPattern = pStart;` |
|   139 |  166 | `	*pnPatternLen = (int)(z - pStart);` |
|   139 |  167 | `	z++; /* Skip closing delimiter */` |
|   139 |  168 | `	*pFlags = z;` |
|   139 |  169 | `	*pnFlagLen = (int)(zEnd - z);` |
|   139 |  170 | `	return PH7_OK;` |
|    81 |  171 | `}` |
|     - |  172 |  |
|     - |  173 | `/* ===== Flag mapper ===== */` |
|   134 |  174 | `static sxi32 PcreMapFlags(` |
|     - |  175 | `	const char *zFlags, int nFlagLen,` |
|     - |  176 | `	uint32_t *pCompileOpts)` |
|     5 |  177 | `{` |
|     - |  178 | `	int i;` |
|   139 |  179 | `	*pCompileOpts = 0;` |
|   163 |  180 | `	for( i = 0; i < nFlagLen; i++ ){` |
|    25 |  181 | `		switch( zFlags[i] ){` |
|    13 |  182 | `			case 'i': *pCompileOpts \|= PCRE2_CASELESS; break;` |
|     5 |  183 | `			case 'm': *pCompileOpts \|= PCRE2_MULTILINE; break;` |
|     5 |  184 | `			case 's': *pCompileOpts \|= PCRE2_DOTALL; break;` |
|   ! 0 |  185 | `			case 'x': *pCompileOpts \|= PCRE2_EXTENDED; break;` |
|     5 |  186 | `			case 'u': *pCompileOpts \|= PCRE2_UTF \| PCRE2_UCP; break;` |
|   ! 0 |  187 | `			case 'A': *pCompileOpts \|= PCRE2_ANCHORED; break;` |
|   ! 0 |  188 | `			case 'D': *pCompileOpts \|= PCRE2_DOLLAR_ENDONLY; break;` |
|   ! 0 |  189 | `			case 'U': *pCompileOpts \|= PCRE2_UNGREEDY; break;` |
|   ! 0 |  190 | `			case 'J': *pCompileOpts \|= PCRE2_DUPNAMES; break;` |
|   ! 0 |  191 | `			case 'S': /* Study hint — no-op in PCRE2 */ break;` |
|   ! 0 |  192 | `			default: break;` |
|     - |  193 | `		}` |
|    13 |  194 | `	}` |
|   139 |  195 | `	return PH7_OK;` |
|     5 |  196 | `}` |
|     - |  197 |  |
|     - |  198 | `/* ===== Compile helper ===== */` |
|   438 |  199 | `static pcre2_code *PcreCompile(` |
|     - |  200 | `	ph7_context *pCtx,` |
|     - |  201 | `	const char *zFullPattern, int nLen,` |
|     - |  202 | `	sxu32 *pCaptureCount)` |
|     5 |  203 | `{` |
|     - |  204 | `	const char *zPat, *zFlags;` |
|     - |  205 | `	int nPatLen, nFlagLen;` |
|     - |  206 | `	uint32_t compileOpts;` |
|     - |  207 | `	pcre2_code *pCode;` |
|     - |  208 | `	PCRE2_SIZE erroffset;` |
|     - |  209 | `	int errcode;` |
|     - |  210 | `	sxu32 nCapture;` |
|     - |  211 | `	sxi32 parseRc;` |
|     - |  212 | `	char cDelim;` |
|     - |  213 | `	int bPaired;` |
|     - |  214 |  |
|     - |  215 | `	/* Check cache first */` |
|   443 |  216 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|   443 |  217 | `	if( pCode ){` |
|   290 |  218 | `		return pCode;` |
|     - |  219 | `	}` |
|     - |  220 | `	/* Parse delimiter */` |
|   157 |  221 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen,` |
|     - |  222 | `		&cDelim, &bPaired);` |
|   157 |  223 | `	if( parseRc != PCRE_PARSE_OK ){` |
|    19 |  224 | `		if( parseRc == PCRE_PARSE_EMPTY ){` |
|   ! 0 |  225 | `			ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty regular expression");` |
|    19 |  226 | `		}else if( parseRc == PCRE_PARSE_BAD_DELIMITER ){` |
|     3 |  227 | `			ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  228 | `				"Delimiter must not be alphanumeric, backslash, or NUL byte");` |
|     2 |  229 | `		}else{` |
|     - |  230 | `			/* php names the delimiter, and distinguishes paired delimiters */` |
|    25 |  231 | `			ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,` |
|    16 |  232 | `				bPaired ? "No ending matching delimiter '%c' found"` |
|     8 |  233 | `				        : "No ending delimiter '%c' found", cDelim);` |
|     - |  234 | `		}` |
|    19 |  235 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|    19 |  236 | `		return 0;` |
|     - |  237 | `	}` |
|     - |  238 | `	/* Map flags */` |
|   139 |  239 | `	PcreMapFlags(zFlags, nFlagLen, &compileOpts);` |
|     - |  240 | `	/* Compile */` |
|   139 |  241 | `	pCode = pcre2_compile(` |
|    67 |  242 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|    67 |  243 | `		compileOpts, &errcode, &erroffset, NULL);` |
|   139 |  244 | `	if( pCode == 0 ){` |
|     - |  245 | `		PCRE2_UCHAR errbuf[256];` |
|    11 |  246 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|    16 |  247 | `		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,` |
|     5 |  248 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|    11 |  249 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|    11 |  250 | `		return 0;` |
|     - |  251 | `	}` |
|     - |  252 | `	/* Get capture count */` |
|   129 |  253 | `	nCapture = 0;` |
|   129 |  254 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|   129 |  255 | `	if( pCaptureCount ){` |
|   129 |  256 | `		*pCaptureCount = nCapture;` |
|    62 |  257 | `	}` |
|     - |  258 | `	/* Cache it */` |
|   129 |  259 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|   129 |  260 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   129 |  261 | `	return pCode;` |
|   224 |  262 | `}` |
|     - |  263 |  |
|     - |  264 | `/* ===== Map PCRE2 match error to PHP error code ===== */` |
|   ! 0 |  265 | `static void PcreSetMatchError(ph7_vm *pVm, int rc)` |
|   ! 0 |  266 | `{` |
|   ! 0 |  267 | `	if( rc == PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  268 | `		pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   ! 0 |  269 | `	}else if( rc == PCRE2_ERROR_MATCHLIMIT ){` |
|   ! 0 |  270 | `		pVm->iPcreLastError = PHP_PREG_BACKTRACK_LIMIT_ERROR;` |
|   ! 0 |  271 | `	}else if( rc == PCRE2_ERROR_DEPTHLIMIT` |
|     - |  272 | `#ifdef PCRE2_ERROR_RECURSIONLIMIT` |
|   ! 0 |  273 | `		\|\| rc == PCRE2_ERROR_RECURSIONLIMIT` |
|     - |  274 | `#endif` |
|     - |  275 | `	){` |
|   ! 0 |  276 | `		pVm->iPcreLastError = PHP_PREG_RECURSION_LIMIT_ERROR;` |
|   ! 0 |  277 | `	}else if( rc == PCRE2_ERROR_BADUTFOFFSET ){` |
|   ! 0 |  278 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_OFFSET_ERROR;` |
|   ! 0 |  279 | `	}else if( rc == PCRE2_ERROR_UTF8_ERR1` |
|   ! 0 |  280 | `		\|\| rc == PCRE2_ERROR_UTF8_ERR2 ){` |
|   ! 0 |  281 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_ERROR;` |
|     - |  282 | `#ifdef PCRE2_ERROR_JIT_STACKLIMIT` |
|   ! 0 |  283 | `	}else if( rc == PCRE2_ERROR_JIT_STACKLIMIT ){` |
|   ! 0 |  284 | `		pVm->iPcreLastError = PHP_PREG_JIT_STACKLIMIT_ERROR;` |
|     - |  285 | `#endif` |
|   ! 0 |  286 | `	}else{` |
|   ! 0 |  287 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|     - |  288 | `	}` |
|   ! 0 |  289 | `}` |
|     - |  290 |  |
|     - |  291 | `/* ===== Helper: populate matches array from ovector ===== */` |
|   214 |  292 | `static void PcrePopulateMatches(` |
|     - |  293 | `	ph7_context *pCtx,` |
|     - |  294 | `	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */` |
|     - |  295 | `	const char *zSubject,` |
|     - |  296 | `	PCRE2_SIZE *ovector,` |
|     - |  297 | `	int nGroups,` |
|     - |  298 | `	pcre2_code *pCode,` |
|     - |  299 | `	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */` |
|     5 |  300 | `{` |
|   219 |  301 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   219 |  302 | `	ph7_value *pSub = 0;` |
|   219 |  303 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|   219 |  304 | `	PCRE2_SPTR nametable = 0;` |
|     - |  305 | `	int i;` |
|     - |  306 |  |
|   219 |  307 | `	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|    11 |  308 | `		pSub = ph7_context_new_array(pCtx);` |
|     4 |  309 | `	}` |
|     - |  310 | `	/* Read the name table up front so each group's named key can be emitted` |
|     - |  311 | `	 * INTERLEAVED with its numbered key, in group order — php stores` |
|     - |  312 | ``	 * `0, name, 1, value, 2` (named entry immediately before its number), not`` |
|     - |  313 | `	 * every number followed by every name. Code that iterates $matches or` |
|     - |  314 | `	 * var_dumps it (PHPUnit's annotation parser) depends on this order. */` |
|   219 |  315 | `	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|   219 |  316 | `	if( namecount > 0 ){` |
|     9 |  317 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     9 |  318 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|     4 |  319 | `	}` |
|   807 |  320 | `	for( i = 0; i < nGroups; i++ ){` |
|   593 |  321 | `		PCRE2_SIZE start = ovector[2 * i];` |
|   593 |  322 | `		PCRE2_SIZE end   = ovector[2 * i + 1];` |
|   593 |  323 | `		const char *zName = 0;` |
|     - |  324 | `		/* Does group i carry a (?<name>...) label? namecount is tiny in practice. */` |
|   593 |  325 | `		if( namecount > 0 ){` |
|     - |  326 | `			uint32_t k;` |
|    49 |  327 | `			for( k = 0; k < namecount; k++ ){` |
|    41 |  328 | `				PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|    41 |  329 | `				if( (((entry[0] << 8) \| entry[1])) == i ){` |
|    17 |  330 | `					zName = (const char *)(entry + 2);` |
|    17 |  331 | `					break;` |
|     - |  332 | `				}` |
|    13 |  333 | `			}` |
|    12 |  334 | `		}` |
|   593 |  335 | `		if( start == PCRE2_UNSET ){` |
|   135 |  336 | `			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|     5 |  337 | `				ph7_value_null(pVal);` |
|     3 |  338 | `			}else{` |
|   131 |  339 | `				ph7_value_string(pVal, "", 0);` |
|     - |  340 | `			}` |
|    68 |  341 | `		}else{` |
|   459 |  342 | `			ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|     - |  343 | `		}` |
|   593 |  344 | `		if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|    13 |  345 | `			ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|    13 |  346 | `			ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|    13 |  347 | `			ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);` |
|    13 |  348 | `			ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|     - |  349 | `			/* php: the named key comes first, then the numbered key (same value). */` |
|    13 |  350 | `			if( zName ){` |
|   ! 0 |  351 | `				ph7_array_add_strkey_elem(pArray, zName, pSub);` |
|   ! 0 |  352 | `			}` |
|    13 |  353 | `			ph7_array_add_intkey_elem(pArray, i, pSub);` |
|    13 |  354 | `			ph7_context_release_value(pCtx, pOff);` |
|    13 |  355 | `			ph7_context_release_value(pCtx, pSub);` |
|    13 |  356 | `			pSub = ph7_context_new_array(pCtx);` |
|     8 |  357 | `		}else{` |
|   583 |  358 | `			if( zName ){` |
|    17 |  359 | `				ph7_array_add_strkey_elem(pArray, zName, pVal);` |
|     8 |  360 | `			}` |
|   583 |  361 | `			ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  362 | `		}` |
|   593 |  363 | `		ph7_value_reset_string_cursor(pVal);` |
|   299 |  364 | `	}` |
|   219 |  365 | `	ph7_context_release_value(pCtx, pVal);` |
|   219 |  366 | `	if( pSub ){` |
|    11 |  367 | `		ph7_context_release_value(pCtx, pSub);` |
|     4 |  368 | `	}` |
|   219 |  369 | `}` |
|     - |  370 |  |
|     - |  371 | `/*` |
|     - |  372 | ` * Quiet whole-pattern match used by FILTER_VALIDATE_REGEXP: compile zPat (a full` |
|     - |  373 | ` * "/.../flags" pattern) and test it against zSub. On a successful attempt returns` |
|     - |  374 | ` * SXRET_OK with *pMatched set to 1 (match) or 0 (no match); returns SXERR_INVALID` |
|     - |  375 | ` * on a compile/match error (the caller treats that as a validation failure). The` |
|     - |  376 | ` * compiled code is owned by PcreCompile's cache, so it is not freed here.` |
|     - |  377 | ` */` |
|     4 |  378 | `PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,` |
|     - |  379 | `	const char *zSub,int nSub,int *pMatched)` |
|     1 |  380 | `{` |
|     - |  381 | `	pcre2_code *pCode;` |
|     - |  382 | `	pcre2_match_data *pMatchData;` |
|     - |  383 | `	sxu32 nCapture;` |
|     - |  384 | `	int rc;` |
|     5 |  385 | `	*pMatched = 0;` |
|     5 |  386 | `	pCode = PcreCompile(pCtx,zPat,nPat,&nCapture);` |
|     5 |  387 | `	if( pCode == 0 ){` |
|   ! 0 |  388 | `		return SXERR_INVALID;` |
|     - |  389 | `	}` |
|     5 |  390 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode,NULL);` |
|     5 |  391 | `	if( pMatchData == 0 ){` |
|   ! 0 |  392 | `		return SXERR_INVALID;` |
|     - |  393 | `	}` |
|     5 |  394 | `	rc = pcre2_match(pCode,(PCRE2_SPTR)zSub,(PCRE2_SIZE)nSub,0,0,pMatchData,NULL);` |
|     5 |  395 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  396 | `	if( rc < 0 ){` |
|     3 |  397 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  398 | `			PcreSetMatchError(pCtx->pVm,rc);` |
|   ! 0 |  399 | `			return SXERR_INVALID;` |
|     - |  400 | `		}` |
|     3 |  401 | `		return SXRET_OK; /* clean no-match */` |
|     - |  402 | `	}` |
|     3 |  403 | `	*pMatched = 1;` |
|     3 |  404 | `	return SXRET_OK;` |
|     3 |  405 | `}` |
|     - |  406 | `/* ======================================================================` |
|     - |  407 | ` * preg_match(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  408 | ` * ====================================================================== */` |
|   206 |  409 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  410 | `{` |
|     - |  411 | `	const char *zPattern, *zSubject;` |
|     - |  412 | `	int nPatLen, nSubLen;` |
|     - |  413 | `	pcre2_code *pCode;` |
|     - |  414 | `	pcre2_match_data *pMatchData;` |
|     - |  415 | `	PCRE2_SIZE *ovector;` |
|     - |  416 | `	sxu32 nCapture;` |
|   211 |  417 | `	PCRE2_SIZE startOffset = 0;` |
|   211 |  418 | `	int iFlags = 0;` |
|     - |  419 | `	int rc;` |
|     - |  420 |  |
|   211 |  421 | `	if( nArg < 2 ){` |
|   ! 0 |  422 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  423 | `			"preg_match() expects at least 2 parameters");` |
|   ! 0 |  424 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  425 | `		return PH7_OK;` |
|     - |  426 | `	}` |
|   211 |  427 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|   211 |  428 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|   211 |  429 | `	if( nArg >= 4 ){` |
|    28 |  430 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|    13 |  431 | `	}` |
|   211 |  432 | `	if( nArg >= 5 ){` |
|   ! 0 |  433 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  434 | `	}` |
|   211 |  435 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|   211 |  436 | `	if( pCode == 0 ){` |
|    15 |  437 | `		ph7_result_bool(pCtx, 0);` |
|    15 |  438 | `		return PH7_OK;` |
|     - |  439 | `	}` |
|     - |  440 | `	/* php validates $flags AFTER the pattern compiles (a bad pattern warns first).` |
|     - |  441 | `	 * php 8.5 only rejects flag bits BELOW PREG_OFFSET_CAPTURE (the low byte); any` |
|     - |  442 | `	 * higher bit is ignored. preg_match permits none of those low bits. */` |
|   197 |  443 | `	if( (iFlags & (PHP_PREG_OFFSET_CAPTURE - 1)) != 0 ){` |
|    11 |  444 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  445 | `			"preg_match(): Argument #4 ($flags) must be a PREG_* constant");` |
|     - |  446 | `	}` |
|   187 |  447 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|   187 |  448 | `	if( pMatchData == 0 ){` |
|   ! 0 |  449 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  450 | `		return PH7_OK;` |
|     - |  451 | `	}` |
|   278 |  452 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    91 |  453 | `		startOffset, 0, pMatchData, NULL);` |
|   187 |  454 | `	if( rc < 0 ){` |
|    32 |  455 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  456 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  457 | `		}` |
|     - |  458 | `		/* Populate empty matches if requested */` |
|    32 |  459 | `		if( nArg >= 3 ){` |
|    18 |  460 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    18 |  461 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pEmpty);` |
|    18 |  462 | `			ph7_context_release_value(pCtx, pEmpty);` |
|     8 |  463 | `		}` |
|    32 |  464 | `		pcre2_match_data_free(pMatchData);` |
|    32 |  465 | `		ph7_result_int(pCtx, 0);` |
|    32 |  466 | `		return PH7_OK;` |
|     - |  467 | `	}` |
|   157 |  468 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   157 |  469 | `	if( nArg >= 3 ){` |
|     - |  470 | `		/* Populate $matches */` |
|   129 |  471 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|   129 |  472 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|   129 |  473 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);` |
|     - |  474 | `		/* Write the array back to the caller's variable */` |
|   129 |  475 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pArray);` |
|   129 |  476 | `		ph7_context_release_value(pCtx, pArray);` |
|    62 |  477 | `	}` |
|   157 |  478 | `	pcre2_match_data_free(pMatchData);` |
|   157 |  479 | `	ph7_result_int(pCtx, 1);` |
|   157 |  480 | `	return PH7_OK;` |
|   108 |  481 | `}` |
|     - |  482 |  |
|     - |  483 | `/* ======================================================================` |
|     - |  484 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  485 | ` * ====================================================================== */` |
|    52 |  486 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |  487 | `{` |
|     - |  488 | `	const char *zPattern, *zSubject;` |
|     - |  489 | `	int nPatLen, nSubLen;` |
|     - |  490 | `	pcre2_code *pCode;` |
|     - |  491 | `	pcre2_match_data *pMatchData;` |
|     - |  492 | `	sxu32 nCapture;` |
|    54 |  493 | `	PCRE2_SIZE startOffset = 0;` |
|    54 |  494 | `	int iFlags = PHP_PREG_PATTERN_ORDER;` |
|    54 |  495 | `	int totalMatches = 0;` |
|     - |  496 | `	int rc;` |
|     - |  497 |  |
|    54 |  498 | `	if( nArg < 2 ){` |
|   ! 0 |  499 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  500 | `			"preg_match_all() expects at least 2 parameters");` |
|   ! 0 |  501 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  502 | `		return PH7_OK;` |
|     - |  503 | `	}` |
|    54 |  504 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    54 |  505 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    54 |  506 | `	if( nArg >= 4 ){` |
|    26 |  507 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|    12 |  508 | `	}` |
|    54 |  509 | `	if( nArg >= 5 ){` |
|   ! 0 |  510 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  511 | `	}` |
|    54 |  512 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    54 |  513 | `	if( pCode == 0 ){` |
|   ! 0 |  514 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  515 | `		return PH7_OK;` |
|     - |  516 | `	}` |
|     - |  517 | `	/* php validates $flags AFTER the pattern compiles (a bad pattern warns first).` |
|     - |  518 | `	 * php 8.5 rejects low-byte bits below PREG_OFFSET_CAPTURE EXCEPT the order flags,` |
|     - |  519 | `	 * and rejects PATTERN_ORDER+SET_ORDER together (mutually exclusive); higher bits` |
|     - |  520 | `	 * are ignored. Every case raises the same ValueError. */` |
|    52 |  521 | `	if( (iFlags & (PHP_PREG_OFFSET_CAPTURE - 1)` |
|    52 |  522 | `			& ~(PHP_PREG_PATTERN_ORDER\|PHP_PREG_SET_ORDER)) != 0` |
|    52 |  523 | `		\|\| ((iFlags & PHP_PREG_PATTERN_ORDER) && (iFlags & PHP_PREG_SET_ORDER)) ){` |
|     9 |  524 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  525 | `			"preg_match_all(): Argument #4 ($flags) must be a PREG_* constant");` |
|     - |  526 | `	}` |
|    46 |  527 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    46 |  528 | `	if( pMatchData == 0 ){` |
|   ! 0 |  529 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  530 | `		return PH7_OK;` |
|     - |  531 | `	}` |
|    46 |  532 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  533 | `	{` |
|    46 |  534 | `		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;` |
|     - |  535 |  |
|    46 |  536 | `		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){` |
|    22 |  537 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  538 | `				PCRE2_SIZE *ovector;` |
|    32 |  539 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    10 |  540 | `					startOffset, 0, pMatchData, NULL);` |
|    22 |  541 | `				if( rc < 0 ){` |
|    10 |  542 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|    10 |  543 | `					break;` |
|     - |  544 | `				}` |
|    14 |  545 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    14 |  546 | `				if( pOutArray ){` |
|    14 |  547 | `					ph7_value *pSet = ph7_context_new_array(pCtx);` |
|    14 |  548 | `					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, iFlags & ~0xFF);` |
|    14 |  549 | `					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);` |
|    14 |  550 | `					ph7_context_release_value(pCtx, pSet);` |
|     6 |  551 | `				}` |
|    14 |  552 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  553 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  554 | `				}else{` |
|    14 |  555 | `					startOffset = ovector[1];` |
|     - |  556 | `				}` |
|    14 |  557 | `				totalMatches++;` |
|     2 |  558 | `			}` |
|     6 |  559 | `		}else{` |
|     - |  560 | `			/* PREG_PATTERN_ORDER (default) */` |
|    38 |  561 | `			ph7_value **apGroupArrays = 0;` |
|    38 |  562 | `			sxu32 nGroups = nCapture + 1;` |
|     - |  563 | `			sxu32 g;` |
|    38 |  564 | `			if( pOutArray ){` |
|    56 |  565 | `				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,` |
|    18 |  566 | `					sizeof(ph7_value *) * nGroups, TRUE, FALSE);` |
|    38 |  567 | `				if( apGroupArrays ){` |
|   102 |  568 | `					for( g = 0; g < nGroups; g++ ){` |
|    66 |  569 | `						apGroupArrays[g] = ph7_context_new_array(pCtx);` |
|    34 |  570 | `					}` |
|    18 |  571 | `				}` |
|    18 |  572 | `			}` |
|    98 |  573 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  574 | `				PCRE2_SIZE *ovector;` |
|   146 |  575 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    48 |  576 | `					startOffset, 0, pMatchData, NULL);` |
|    98 |  577 | `				if( rc < 0 ){` |
|    38 |  578 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|    38 |  579 | `					break;` |
|     - |  580 | `				}` |
|    62 |  581 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    62 |  582 | `				if( apGroupArrays ){` |
|    62 |  583 | `					ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    62 |  584 | `					int nActual = rc;` |
|   172 |  585 | `					for( g = 0; g < nGroups; g++ ){` |
|   166 |  586 | `						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){` |
|   110 |  587 | `							PCRE2_SIZE s = ovector[2*g];` |
|   110 |  588 | `							PCRE2_SIZE e = ovector[2*g+1];` |
|   110 |  589 | `							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|     3 |  590 | `								ph7_value *pSub = ph7_context_new_array(pCtx);` |
|     3 |  591 | `								ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|     3 |  592 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|     3 |  593 | `								ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|     3 |  594 | `								ph7_value_int(pOff, (int)s);` |
|     3 |  595 | `								ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|     3 |  596 | `								ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|     3 |  597 | `								ph7_context_release_value(pCtx, pSub);` |
|     3 |  598 | `								ph7_context_release_value(pCtx, pOff);` |
|     2 |  599 | `							}else{` |
|   108 |  600 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|   108 |  601 | `								ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  602 | `							}` |
|    56 |  603 | `						}else{` |
|     3 |  604 | `							ph7_value_string(pVal, "", 0);` |
|     3 |  605 | `							ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  606 | `						}` |
|   112 |  607 | `						ph7_value_reset_string_cursor(pVal);` |
|    57 |  608 | `					}` |
|    62 |  609 | `					ph7_context_release_value(pCtx, pVal);` |
|    30 |  610 | `				}` |
|    62 |  611 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  612 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  613 | `				}else{` |
|    62 |  614 | `					startOffset = ovector[1];` |
|     - |  615 | `				}` |
|    62 |  616 | `				totalMatches++;` |
|     2 |  617 | `			}` |
|    38 |  618 | `			if( apGroupArrays ){` |
|     - |  619 | `				/* Attach the per-group match arrays. php's PREG_PATTERN_ORDER stores a` |
|     - |  620 | `				 * named group under BOTH its name and its number, interleaved` |
|     - |  621 | ``				 * (`0, name, 1, value, 2`) — the same value under each key. Read the`` |
|     - |  622 | `				 * name table so each numbered group can emit its named alias first. */` |
|    38 |  623 | `				uint32_t namecount = 0, nameentrysize = 0;` |
|    38 |  624 | `				PCRE2_SPTR nametable = 0;` |
|    38 |  625 | `				pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|    38 |  626 | `				if( namecount > 0 ){` |
|     3 |  627 | `					pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     3 |  628 | `					pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|     1 |  629 | `				}` |
|   102 |  630 | `				for( g = 0; g < nGroups; g++ ){` |
|    66 |  631 | `					const char *zName = 0;` |
|    66 |  632 | `					if( namecount > 0 ){` |
|     - |  633 | `						uint32_t k;` |
|    13 |  634 | `						for( k = 0; k < namecount; k++ ){` |
|    11 |  635 | `							PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|    11 |  636 | `							if( (uint32_t)(((entry[0] << 8) \| entry[1])) == g ){` |
|     5 |  637 | `								zName = (const char *)(entry + 2);` |
|     5 |  638 | `								break;` |
|     - |  639 | `							}` |
|     4 |  640 | `						}` |
|     3 |  641 | `					}` |
|    66 |  642 | `					if( zName ){` |
|     5 |  643 | `						ph7_array_add_strkey_elem(pOutArray, zName, apGroupArrays[g]);` |
|     2 |  644 | `					}` |
|    66 |  645 | `					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);` |
|    66 |  646 | `					ph7_context_release_value(pCtx, apGroupArrays[g]);` |
|    34 |  647 | `				}` |
|    38 |  648 | `				ph7_context_free_chunk(pCtx, apGroupArrays);` |
|    18 |  649 | `			}` |
|     - |  650 | `		}` |
|     - |  651 | `		/* Write output array to caller's variable */` |
|    46 |  652 | `		if( pOutArray && nArg >= 3 ){` |
|    46 |  653 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pOutArray);` |
|    46 |  654 | `			ph7_context_release_value(pCtx, pOutArray);` |
|    22 |  655 | `		}` |
|     - |  656 | `	}` |
|    46 |  657 | `	pcre2_match_data_free(pMatchData);` |
|    46 |  658 | `	ph7_result_int(pCtx, totalMatches);` |
|    46 |  659 | `	return PH7_OK;` |
|    28 |  660 | `}` |
|     - |  661 |  |
|     - |  662 | `/* ======================================================================` |
|     - |  663 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|     - |  664 | ` * ====================================================================== */` |
|     8 |  665 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |  666 | `{` |
|     - |  667 | `	const char *zPattern, *zSubject;` |
|     - |  668 | `	int nPatLen, nSubLen;` |
|     - |  669 | `	pcre2_code *pCode;` |
|     - |  670 | `	pcre2_match_data *pMatchData;` |
|     - |  671 | `	sxu32 nCapture;` |
|     - |  672 | `	ph7_value *pArray;` |
|     - |  673 | `	ph7_value *pVal;` |
|    10 |  674 | `	PCRE2_SIZE startOffset = 0, lastOffset = 0;` |
|    10 |  675 | `	int limit = -1;` |
|    10 |  676 | `	int iFlags = 0;` |
|    10 |  677 | `	int nPieces = 0;` |
|     - |  678 | `	int rc;` |
|     - |  679 |  |
|    10 |  680 | `	if( nArg < 2 ){` |
|   ! 0 |  681 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  682 | `			"preg_split() expects at least 2 parameters");` |
|   ! 0 |  683 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  684 | `		return PH7_OK;` |
|     - |  685 | `	}` |
|    10 |  686 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    10 |  687 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    10 |  688 | `	if( nArg >= 3 ){` |
|     5 |  689 | `		limit = ph7_value_to_int(apArg[2]);` |
|     2 |  690 | `	}` |
|    10 |  691 | `	if( nArg >= 4 ){` |
|     3 |  692 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     1 |  693 | `	}` |
|    10 |  694 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    10 |  695 | `	if( pCode == 0 ){` |
|     3 |  696 | `		ph7_result_bool(pCtx, 0);` |
|     3 |  697 | `		return PH7_OK;` |
|     - |  698 | `	}` |
|     7 |  699 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     7 |  700 | `	if( pMatchData == 0 ){` |
|   ! 0 |  701 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  702 | `		return PH7_OK;` |
|     - |  703 | `	}` |
|     7 |  704 | `	pArray = ph7_context_new_array(pCtx);` |
|     7 |  705 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     7 |  706 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  707 |  |
|    19 |  708 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|    19 |  709 | `		if( limit > 0 && nPieces >= limit - 1 ){` |
|     3 |  710 | `			break; /* Last piece gets the remainder */` |
|     - |  711 | `		}` |
|    25 |  712 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     8 |  713 | `			startOffset, 0, pMatchData, NULL);` |
|    17 |  714 | `		if( rc < 0 ){` |
|     5 |  715 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  716 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  717 | `			}` |
|     5 |  718 | `			break;` |
|     - |  719 | `		}` |
|     - |  720 | `		{` |
|    13 |  721 | `			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    13 |  722 | `			PCRE2_SIZE matchStart = ovector[0];` |
|    13 |  723 | `			PCRE2_SIZE matchEnd = ovector[1];` |
|    13 |  724 | `			int pieceLen = (int)(matchStart - lastOffset);` |
|     - |  725 |  |
|     - |  726 | `			/* Add the piece before the match */` |
|    13 |  727 | `			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| pieceLen > 0 ){` |
|    13 |  728 | `				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  729 | `					ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  730 | `					ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  731 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|   ! 0 |  732 | `					ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  733 | `					ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  734 | `					ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  735 | `					ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  736 | `					ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  737 | `					ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  738 | `				}else{` |
|    13 |  739 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|    13 |  740 | `					ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  741 | `				}` |
|    13 |  742 | `				ph7_value_reset_string_cursor(pVal);` |
|    13 |  743 | `				nPieces++;` |
|     6 |  744 | `			}` |
|     - |  745 | `			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */` |
|    13 |  746 | `			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){` |
|     - |  747 | `				int g;` |
|   ! 0 |  748 | `				for( g = 1; g < rc; g++ ){` |
|   ! 0 |  749 | `					PCRE2_SIZE gs = ovector[2*g];` |
|   ! 0 |  750 | `					PCRE2_SIZE ge = ovector[2*g+1];` |
|     - |  751 | `					int gLen;` |
|   ! 0 |  752 | `					if( gs == PCRE2_UNSET ) continue;` |
|   ! 0 |  753 | `					gLen = (int)(ge - gs);` |
|   ! 0 |  754 | `					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| gLen > 0 ){` |
|   ! 0 |  755 | `						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  756 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  757 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  758 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  759 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  760 | `							ph7_value_int(pOff, (int)gs);` |
|   ! 0 |  761 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  762 | `							ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  763 | `							ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  764 | `							ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  765 | `						}else{` |
|   ! 0 |  766 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  767 | `							ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  768 | `						}` |
|   ! 0 |  769 | `						ph7_value_reset_string_cursor(pVal);` |
|   ! 0 |  770 | `					}` |
|   ! 0 |  771 | `				}` |
|   ! 0 |  772 | `			}` |
|     - |  773 | `			/* Advance */` |
|    13 |  774 | `			lastOffset = matchEnd;` |
|    13 |  775 | `			if( matchEnd == matchStart ){` |
|   ! 0 |  776 | `				startOffset = matchEnd + 1;` |
|   ! 0 |  777 | `			}else{` |
|    13 |  778 | `				startOffset = matchEnd;` |
|     - |  779 | `			}` |
|     - |  780 | `		}` |
|     1 |  781 | `	}` |
|     - |  782 | `	/* Add trailing piece */` |
|     - |  783 | `	{` |
|     7 |  784 | `		int trailLen = nSubLen - (int)lastOffset;` |
|     7 |  785 | `		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| trailLen > 0 ){` |
|     7 |  786 | `			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  787 | `				ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  788 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  789 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|   ! 0 |  790 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  791 | `				ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  792 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  793 | `				ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  794 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  795 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  796 | `			}else{` |
|     7 |  797 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|     7 |  798 | `				ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  799 | `			}` |
|     3 |  800 | `		}` |
|     - |  801 | `	}` |
|     7 |  802 | `	ph7_context_release_value(pCtx, pVal);` |
|     7 |  803 | `	pcre2_match_data_free(pMatchData);` |
|     7 |  804 | `	ph7_result_value(pCtx, pArray);` |
|     7 |  805 | `	ph7_context_release_value(pCtx, pArray);` |
|     7 |  806 | `	return PH7_OK;` |
|     6 |  807 | `}` |
|     - |  808 |  |
|     - |  809 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|   134 |  810 | `static void PcreExpandBackrefs(` |
|     - |  811 | `	SyBlob *pOut,` |
|     - |  812 | `	const char *zRepl, int nReplLen,` |
|     - |  813 | `	const char *zSubject,` |
|     - |  814 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     2 |  815 | `{` |
|   136 |  816 | `	const char *zEnd = &zRepl[nReplLen];` |
|   136 |  817 | `	const char *z = zRepl;` |
|     - |  818 |  |
|   286 |  819 | `	while( z < zEnd ){` |
|   152 |  820 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   ! 0 |  821 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|   ! 0 |  822 | `				int g = z[1] - '0';` |
|   ! 0 |  823 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  824 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  825 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  826 | `				}` |
|   ! 0 |  827 | `				z += 2;` |
|   ! 0 |  828 | `				continue;` |
|     - |  829 | `			}` |
|   ! 0 |  830 | `			if( z[1] == '\\' ){` |
|   ! 0 |  831 | `				SyBlobAppend(pOut, "\\", 1);` |
|   ! 0 |  832 | `				z += 2;` |
|   ! 0 |  833 | `				continue;` |
|     - |  834 | `			}` |
|     - |  835 | `			/* Not a backreference — emit literally */` |
|   ! 0 |  836 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  837 | `			z++;` |
|   ! 0 |  838 | `			continue;` |
|     - |  839 | `		}` |
|   152 |  840 | `		if( *z == '$' && z + 1 < zEnd ){` |
|    13 |  841 | `			if( z[1] == '$' ){` |
|   ! 0 |  842 | `				SyBlobAppend(pOut, "$", 1);` |
|   ! 0 |  843 | `				z += 2;` |
|   ! 0 |  844 | `				continue;` |
|     - |  845 | `			}` |
|    13 |  846 | `			if( z[1] == '{' ){` |
|     - |  847 | `				/* ${N} form */` |
|   ! 0 |  848 | `				const char *p = z + 2;` |
|   ! 0 |  849 | `				int g = 0;` |
|   ! 0 |  850 | `				while( p < zEnd && *p >= '0' && *p <= '9' ){` |
|   ! 0 |  851 | `					g = g * 10 + (*p - '0');` |
|   ! 0 |  852 | `					p++;` |
|   ! 0 |  853 | `				}` |
|   ! 0 |  854 | `				if( p < zEnd && *p == '}' ){` |
|   ! 0 |  855 | `					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  856 | `						SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  857 | `							(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  858 | `					}` |
|   ! 0 |  859 | `					z = p + 1;` |
|   ! 0 |  860 | `					continue;` |
|     - |  861 | `				}` |
|     - |  862 | `				/* Not a valid ${N} — emit literally */` |
|   ! 0 |  863 | `				SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  864 | `				z++;` |
|   ! 0 |  865 | `				continue;` |
|     - |  866 | `			}` |
|    13 |  867 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|     - |  868 | `				/* $N or $NN */` |
|    13 |  869 | `				int g = z[1] - '0';` |
|    13 |  870 | `				z += 2;` |
|     - |  871 | `				/* Check for second digit */` |
|    13 |  872 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  873 | `					int g2 = g * 10 + (*z - '0');` |
|   ! 0 |  874 | `					if( g2 < nGroups ){` |
|   ! 0 |  875 | `						g = g2;` |
|   ! 0 |  876 | `						z++;` |
|   ! 0 |  877 | `					}` |
|   ! 0 |  878 | `				}` |
|    13 |  879 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|    19 |  880 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|    12 |  881 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|     6 |  882 | `				}` |
|    13 |  883 | `				continue;` |
|     - |  884 | `			}` |
|     - |  885 | `			/* Not a backreference */` |
|   ! 0 |  886 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  887 | `			z++;` |
|   ! 0 |  888 | `			continue;` |
|     - |  889 | `		}` |
|   140 |  890 | `		SyBlobAppend(pOut, z, 1);` |
|   140 |  891 | `		z++;` |
|     2 |  892 | `	}` |
|   136 |  893 | `}` |
|     - |  894 |  |
|     - |  895 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|    94 |  896 | `static void PcreDoReplace(` |
|     - |  897 | `	ph7_context *pCtx,` |
|     - |  898 | `	pcre2_code *pCode,` |
|     - |  899 | `	const char *zSubject, int nSubLen,` |
|     - |  900 | `	const char *zRepl, int nReplLen,` |
|     - |  901 | `	int limit,` |
|     - |  902 | `	int *pCount,` |
|     - |  903 | `	SyBlob *pOut)` |
|     2 |  904 | `{` |
|     - |  905 | `	pcre2_match_data *pMatchData;` |
|    96 |  906 | `	PCRE2_SIZE startOffset = 0;` |
|    96 |  907 | `	int nReplacements = 0;` |
|     - |  908 | `	int rc;` |
|     - |  909 |  |
|    96 |  910 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    96 |  911 | `	if( pMatchData == 0 ) return;` |
|     - |  912 |  |
|   230 |  913 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  914 | `		PCRE2_SIZE *ovector;` |
|   228 |  915 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|   341 |  916 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|   113 |  917 | `			startOffset, 0, pMatchData, NULL);` |
|   228 |  918 | `		if( rc < 0 ){` |
|    94 |  919 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  920 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  921 | `			}` |
|    94 |  922 | `			break;` |
|     - |  923 | `		}` |
|   136 |  924 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - |  925 | `		/* Copy text before match */` |
|   136 |  926 | `		if( ovector[0] > startOffset ){` |
|    81 |  927 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|    40 |  928 | `		}` |
|     - |  929 | `		/* Expand replacement */` |
|   136 |  930 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|   136 |  931 | `		nReplacements++;` |
|     - |  932 | `		/* Advance */` |
|   136 |  933 | `		if( ovector[1] == ovector[0] ){` |
|     - |  934 | `			/* Zero-width match: to make progress, emit the character AT THE MATCH` |
|     - |  935 | `			 * POSITION (ovector[0]) and step past it. The match can sit AHEAD of the` |
|     - |  936 | `			 * search start (a lookbehind/lookahead assertion, e.g. the camelCase` |
|     - |  937 | `			 * split /(?<=[[:lower:]])(?=[[:upper:]])/), so copying zSubject[startOffset]` |
|     - |  938 | `			 * grabbed the wrong byte ("fooBar" -> "foo far"). The text between` |
|     - |  939 | `			 * startOffset and ovector[0] was already copied above. */` |
|    23 |  940 | `			if( ovector[0] < (PCRE2_SIZE)nSubLen ){` |
|    21 |  941 | `				SyBlobAppend(pOut, &zSubject[ovector[0]], 1);` |
|    10 |  942 | `			}` |
|    23 |  943 | `			startOffset = ovector[0] + 1;` |
|    12 |  944 | `		}else{` |
|   114 |  945 | `			startOffset = ovector[1];` |
|     - |  946 | `		}` |
|     2 |  947 | `	}` |
|     - |  948 | `	/* Copy remainder */` |
|    96 |  949 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|    36 |  950 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|    17 |  951 | `	}` |
|    96 |  952 | `	if( pCount ){` |
|    96 |  953 | `		*pCount += nReplacements;` |
|    47 |  954 | `	}` |
|    96 |  955 | `	pcre2_match_data_free(pMatchData);` |
|    47 |  956 | `	SXUNUSED(pCtx);` |
|    49 |  957 | `}` |
|     - |  958 |  |
|     - |  959 | `/* ===== Helper: apply pattern(s)+replacement(s) to ONE subject string =====` |
|     - |  960 | ` * pPattern is a string or an array of patterns; pRepl is a string (used for` |
|     - |  961 | ` * every pattern) or, only when pPattern is an array, an array taken by ORDER` |
|     - |  962 | ` * (missing element -> ""). Array patterns are applied sequentially, each to the` |
|     - |  963 | ` * result of the previous (PHP semantics), ping-ponging two blobs. The final` |
|     - |  964 | ` * text is appended to pOut. Returns SXRET_OK, or SXERR_ABORT on a bad pattern` |
|     - |  965 | ` * (the caller then yields NULL, matching the scalar path). */` |
|    90 |  966 | `static sxi32 PcreReplaceSubject(` |
|     - |  967 | `	ph7_context *pCtx,` |
|     - |  968 | `	ph7_value *pPattern,` |
|     - |  969 | `	ph7_value *pRepl,` |
|     - |  970 | `	const char *zSubject, int nSubLen,` |
|     - |  971 | `	int limit,` |
|     - |  972 | `	int *pCount,` |
|     - |  973 | `	SyBlob *pOut)` |
|     3 |  974 | `{` |
|     - |  975 | `	sxu32 nCapture;` |
|    93 |  976 | `	if( !ph7_value_is_array(pPattern) ){` |
|     - |  977 | `		/* Single pattern + single replacement */` |
|     - |  978 | `		const char *zPattern, *zRepl;` |
|     - |  979 | `		int nPatLen, nReplLen;` |
|     - |  980 | `		pcre2_code *pCode;` |
|    81 |  981 | `		zPattern = ph7_value_to_string(pPattern, &nPatLen);` |
|    81 |  982 | `		zRepl = ph7_value_to_string(pRepl, &nReplLen);` |
|    81 |  983 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    81 |  984 | `		if( pCode == 0 ){` |
|     6 |  985 | `			return SXERR_ABORT;` |
|     - |  986 | `		}` |
|    76 |  987 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, pCount, pOut);` |
|    76 |  988 | `		return SXRET_OK;` |
|   ! 0 |  989 | `	}else{` |
|     - |  990 | `		/* Array of patterns: apply each in insertion order to the accumulating` |
|     - |  991 | `		 * subject. Replacement is the parallel array element (by order) or the` |
|     - |  992 | `		 * scalar replacement for every pattern. */` |
|    13 |  993 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|    13 |  994 | `		ph7_hashmap *pRepMap = ph7_value_is_array(pRepl) ? (ph7_hashmap *)pRepl->x.pOther : 0;` |
|    13 |  995 | `		const char *zScalarRepl = 0;` |
|    13 |  996 | `		int nScalarRepl = 0;` |
|     - |  997 | `		ph7_hashmap_node *pPatNode, *pRepNode;` |
|     - |  998 | `		ph7_value sPat, sRep;` |
|     - |  999 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|     - | 1000 | `		sxu32 n;` |
|    13 | 1001 | `		sxi32 rc = SXRET_OK;` |
|    13 | 1002 | `		if( pRepMap == 0 ){` |
|     5 | 1003 | `			zScalarRepl = ph7_value_to_string(pRepl, &nScalarRepl);` |
|     2 | 1004 | `		}` |
|    13 | 1005 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|    13 | 1006 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|    13 | 1007 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|    13 | 1008 | `		pSrc = &sA; pDst = &sB;` |
|    13 | 1009 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|    13 | 1010 | `		PH7_MemObjInit(pCtx->pVm, &sRep);` |
|    13 | 1011 | `		pPatNode = pPatMap->pFirst;` |
|    13 | 1012 | `		pRepNode = pRepMap ? pRepMap->pFirst : 0;` |
|    13 | 1013 | `		n = pPatMap->nEntry;` |
|    33 | 1014 | `		while( n > 0 ){` |
|     - | 1015 | `			const char *zPattern, *zRepl;` |
|     - | 1016 | `			int nPatLen, nReplLen;` |
|     - | 1017 | `			pcre2_code *pCode;` |
|     - | 1018 | `			SyBlob *pSwap;` |
|    21 | 1019 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|    21 | 1020 | `			zPattern = ph7_value_to_string(&sPat, &nPatLen);` |
|    21 | 1021 | `			if( pRepMap ){` |
|    17 | 1022 | `				if( pRepNode ){` |
|    15 | 1023 | `					PH7_HashmapExtractNodeValue(pRepNode, &sRep, FALSE);` |
|    15 | 1024 | `					zRepl = ph7_value_to_string(&sRep, &nReplLen);` |
|     8 | 1025 | `				}else{` |
|     3 | 1026 | `					zRepl = ""; nReplLen = 0;` |
|     - | 1027 | `				}` |
|     9 | 1028 | `			}else{` |
|     5 | 1029 | `				zRepl = zScalarRepl; nReplLen = nScalarRepl;` |
|     - | 1030 | `			}` |
|    21 | 1031 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    21 | 1032 | `			if( pCode == 0 ){` |
|   ! 0 | 1033 | `				rc = SXERR_ABORT;` |
|   ! 0 | 1034 | `				PH7_MemObjRelease(&sPat);` |
|   ! 0 | 1035 | `				if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|   ! 0 | 1036 | `				break;` |
|     - | 1037 | `			}` |
|    21 | 1038 | `			SyBlobReset(pDst);` |
|    31 | 1039 | `			PcreDoReplace(pCtx, pCode,` |
|    20 | 1040 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|    10 | 1041 | `				zRepl, nReplLen, limit, pCount, pDst);` |
|     - | 1042 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|    21 | 1043 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|    21 | 1044 | `			PH7_MemObjRelease(&sPat);` |
|    21 | 1045 | `			if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|    21 | 1046 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|    21 | 1047 | `			if( pRepNode ){ pRepNode = pRepNode->pPrev; }` |
|    21 | 1048 | `			n--;` |
|     1 | 1049 | `		}` |
|    13 | 1050 | `		if( rc == SXRET_OK ){` |
|    13 | 1051 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|     6 | 1052 | `		}` |
|    13 | 1053 | `		SyBlobRelease(&sA);` |
|    13 | 1054 | `		SyBlobRelease(&sB);` |
|    13 | 1055 | `		return rc;` |
|     - | 1056 | `	}` |
|    48 | 1057 | `}` |
|     - | 1058 |  |
|     - | 1059 | `/* ======================================================================` |
|     - | 1060 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - | 1061 | ` * ====================================================================== */` |
|    74 | 1062 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1063 | `{` |
|    77 | 1064 | `	int limit = -1;` |
|    77 | 1065 | `	int count = 0;` |
|     - | 1066 |  |
|    77 | 1067 | `	if( nArg < 3 ){` |
|   ! 0 | 1068 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1069 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 | 1070 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1071 | `		return PH7_OK;` |
|     - | 1072 | `	}` |
|    77 | 1073 | `	if( nArg >= 4 ){` |
|    32 | 1074 | `		limit = ph7_value_to_int(apArg[3]);` |
|    15 | 1075 | `	}` |
|    77 | 1076 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1077 |  |
|     - | 1078 | `	/* A scalar pattern with an array replacement is a parameter mismatch (PHP` |
|     - | 1079 | `	 * throws a TypeError; PHL keeps preg_replace's warning-based arg-error style). */` |
|    77 | 1080 | `	if( !ph7_value_is_array(apArg[0]) && ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 1081 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1082 | `			"Parameter mismatch, pattern is a string while replacement is an array");` |
|   ! 0 | 1083 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1084 | `		return PH7_OK;` |
|     - | 1085 | `	}` |
|   112 | 1086 | `	if( ph7_value_is_array(apArg[2]) ){` |
|     - | 1087 | `		/* Array subject: return an array, each element replaced, keys preserved. */` |
|    15 | 1088 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|    15 | 1089 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|    15 | 1090 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     - | 1091 | `		ph7_value sKey, sVal;` |
|     - | 1092 | `		ph7_hashmap_node *pNode;` |
|     - | 1093 | `		sxu32 n;` |
|    15 | 1094 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|   ! 0 | 1095 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1096 | `			return PH7_OK;` |
|     - | 1097 | `		}` |
|    15 | 1098 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|    15 | 1099 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    15 | 1100 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|    15 | 1101 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|    45 | 1102 | `		while( n > 0 ){` |
|     - | 1103 | `			const char *zSubject;` |
|     - | 1104 | `			int nSubLen;` |
|     - | 1105 | `			SyBlob sOut;` |
|    31 | 1106 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|    31 | 1107 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|    31 | 1108 | `			zSubject = ph7_value_to_string(&sVal, &nSubLen);` |
|    31 | 1109 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    31 | 1110 | `			if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1111 | `				/* A bad pattern with an array subject yields an empty array (PHP);` |
|     - | 1112 | `				 * the failure hits the first element, so pResult is still empty. */` |
|   ! 0 | 1113 | `				SyBlobRelease(&sOut);` |
|   ! 0 | 1114 | `				PH7_MemObjRelease(&sKey);` |
|   ! 0 | 1115 | `				PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1116 | `				ph7_result_value(pCtx, pResult);` |
|   ! 0 | 1117 | `				goto set_count;` |
|     - | 1118 | `			}` |
|    31 | 1119 | `			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    31 | 1120 | `			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|    31 | 1121 | `			ph7_value_reset_string_cursor(pElem);` |
|    31 | 1122 | `			SyBlobRelease(&sOut);` |
|    31 | 1123 | `			PH7_MemObjRelease(&sKey);` |
|    31 | 1124 | `			PH7_MemObjRelease(&sVal);` |
|    31 | 1125 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|    31 | 1126 | `			n--;` |
|     1 | 1127 | `		}` |
|    15 | 1128 | `		ph7_result_value(pCtx, pResult);` |
|     8 | 1129 | `	}else{` |
|     - | 1130 | `		/* Scalar subject: one replaced string. */` |
|     - | 1131 | `		const char *zSubject;` |
|     - | 1132 | `		int nSubLen;` |
|     - | 1133 | `		SyBlob sOut;` |
|    63 | 1134 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    63 | 1135 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    63 | 1136 | `		if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1137 | `			/* Scalar subject: a bad pattern returns NULL (PHP). */` |
|     6 | 1138 | `			SyBlobRelease(&sOut);` |
|     6 | 1139 | `			ph7_result_null(pCtx);` |
|     6 | 1140 | `			goto set_count;` |
|     - | 1141 | `		}` |
|    58 | 1142 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    58 | 1143 | `		SyBlobRelease(&sOut);` |
|     - | 1144 | `	}` |
|    37 | 1145 | `set_count:` |
|     - | 1146 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|     - | 1147 | `	 * (PHP always writes it: 0, or the count accumulated by earlier good patterns). */` |
|    77 | 1148 | `	if( nArg >= 5 ){` |
|     - | 1149 | `		ph7_value sCount;` |
|    32 | 1150 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|    32 | 1151 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|    32 | 1152 | `		PH7_MemObjRelease(&sCount);` |
|    15 | 1153 | `	}` |
|    77 | 1154 | `	return PH7_OK;` |
|    40 | 1155 | `}` |
|     - | 1156 |  |
|     - | 1157 | `/* ===== Helper: run the callback over ONE compiled pattern on ONE subject =====` |
|     - | 1158 | ` * The mirror of PcreDoReplace() for preg_replace_callback: the replacement text` |
|     - | 1159 | ` * comes from a user callback fed the match array (shaped by $flags) instead of` |
|     - | 1160 | ` * from a template. Appends the whole replaced subject to pOut and adds its own` |
|     - | 1161 | ` * replacement count to *pCount. Returns SXRET_OK, or PH7_EXCEPTION when the` |
|     - | 1162 | ` * callback threw — the caller then unwinds without producing a result. */` |
|    62 | 1163 | `static sxi32 PcreDoCallbackReplace(` |
|     - | 1164 | `	ph7_context *pCtx,` |
|     - | 1165 | `	pcre2_code *pCode,` |
|     - | 1166 | `	const char *zSubject, int nSubLen,` |
|     - | 1167 | `	ph7_value *pCallback,` |
|     - | 1168 | `	int limit,` |
|     - | 1169 | `	int iFlags,` |
|     - | 1170 | `	int *pCount,` |
|     - | 1171 | `	SyBlob *pOut)` |
|     3 | 1172 | `{` |
|     - | 1173 | `	pcre2_match_data *pMatchData;` |
|    65 | 1174 | `	PCRE2_SIZE startOffset = 0;` |
|    65 | 1175 | `	int nReplacements = 0;` |
|     - | 1176 | `	int rc;` |
|     - | 1177 |  |
|    65 | 1178 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    65 | 1179 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1180 | `		return SXRET_OK;` |
|     - | 1181 | `	}` |
|   141 | 1182 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1183 | `		PCRE2_SIZE *ovector;` |
|     - | 1184 | `		ph7_value *pMatchArr;` |
|     - | 1185 | `		ph7_value *apCbArg[1];` |
|     - | 1186 | `		ph7_value sResult;` |
|     - | 1187 | `		const char *zReplacement;` |
|     - | 1188 | `		int nReplLen;` |
|     - | 1189 |  |
|   169 | 1190 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|   204 | 1191 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    67 | 1192 | `			startOffset, 0, pMatchData, NULL);` |
|   137 | 1193 | `		if( rc < 0 ){` |
|    59 | 1194 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1195 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1196 | `			}` |
|    59 | 1197 | `			break;` |
|     - | 1198 | `		}` |
|    81 | 1199 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1200 | `		/* Copy text before match */` |
|    81 | 1201 | `		if( ovector[0] > startOffset ){` |
|    29 | 1202 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|    13 | 1203 | `		}` |
|     - | 1204 | `		/* Build matches array for callback */` |
|    81 | 1205 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    81 | 1206 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, iFlags);` |
|     - | 1207 | `		/* Call the callback */` |
|    81 | 1208 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    81 | 1209 | `		apCbArg[0] = pMatchArr;` |
|    81 | 1210 | `		if( PH7_VmCallUserFunction(pCtx->pVm, pCallback, 1, apCbArg, &sResult) == PH7_EXCEPTION ){` |
|     - | 1211 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|     3 | 1212 | `			PH7_MemObjRelease(&sResult);` |
|     3 | 1213 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|     3 | 1214 | `			pcre2_match_data_free(pMatchData);` |
|     3 | 1215 | `			*pCount += nReplacements;` |
|     3 | 1216 | `			return PH7_EXCEPTION;` |
|     - | 1217 | `		}` |
|     - | 1218 | `		/* Get replacement string from callback result */` |
|    79 | 1219 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    79 | 1220 | `		SyBlobAppend(pOut, zReplacement, (sxu32)nReplLen);` |
|    79 | 1221 | `		PH7_MemObjRelease(&sResult);` |
|    79 | 1222 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    79 | 1223 | `		nReplacements++;` |
|     - | 1224 | `		/* Advance */` |
|    79 | 1225 | `		if( ovector[1] == ovector[0] ){` |
|     - | 1226 | `			/* Zero-width match: emit the character AT THE MATCH POSITION and step` |
|     - | 1227 | `			 * past it. The match can sit AHEAD of the search start (a lookaround` |
|     - | 1228 | `			 * assertion, e.g. the camelCase split), so copying zSubject[startOffset]` |
|     - | 1229 | `			 * grabbed the wrong byte ("fooBar" -> "foo far") — the same fix` |
|     - | 1230 | `			 * PcreDoReplace() carries. */` |
|     5 | 1231 | `			if( ovector[0] < (PCRE2_SIZE)nSubLen ){` |
|     5 | 1232 | `				SyBlobAppend(pOut, &zSubject[ovector[0]], 1);` |
|     2 | 1233 | `			}` |
|     5 | 1234 | `			startOffset = ovector[0] + 1;` |
|     3 | 1235 | `		}else{` |
|    75 | 1236 | `			startOffset = ovector[1];` |
|     - | 1237 | `		}` |
|     3 | 1238 | `	}` |
|     - | 1239 | `	/* Copy remainder */` |
|    63 | 1240 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|    32 | 1241 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|    15 | 1242 | `	}` |
|    63 | 1243 | `	*pCount += nReplacements;` |
|    63 | 1244 | `	pcre2_match_data_free(pMatchData);` |
|    63 | 1245 | `	return SXRET_OK;` |
|    34 | 1246 | `}` |
|     - | 1247 |  |
|     - | 1248 | `/* ===== Helper: apply pattern(s)+callback to ONE subject string =====` |
|     - | 1249 | ` * The callback twin of PcreReplaceSubject(): pPattern is a string or an ARRAY of` |
|     - | 1250 | ` * patterns applied sequentially, each to the result of the previous (php` |
|     - | 1251 | ` * semantics), ping-ponging two blobs. Returns SXRET_OK, SXERR_ABORT on a bad` |
|     - | 1252 | ` * pattern (the caller then yields NULL / an empty array like the template path),` |
|     - | 1253 | ` * or PH7_EXCEPTION when the callback threw. */` |
|    62 | 1254 | `static sxi32 PcreCallbackReplaceSubject(` |
|     - | 1255 | `	ph7_context *pCtx,` |
|     - | 1256 | `	ph7_value *pPattern,` |
|     - | 1257 | `	ph7_value *pCallback,` |
|     - | 1258 | `	const char *zSubject, int nSubLen,` |
|     - | 1259 | `	int limit,` |
|     - | 1260 | `	int iFlags,` |
|     - | 1261 | `	int *pCount,` |
|     - | 1262 | `	SyBlob *pOut)` |
|     3 | 1263 | `{` |
|     - | 1264 | `	sxu32 nCapture;` |
|    65 | 1265 | `	if( !ph7_value_is_array(pPattern) ){` |
|     - | 1266 | `		const char *zPattern;` |
|     - | 1267 | `		int nPatLen;` |
|     - | 1268 | `		pcre2_code *pCode;` |
|    57 | 1269 | `		zPattern = ph7_value_to_string(pPattern, &nPatLen);` |
|    57 | 1270 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    57 | 1271 | `		if( pCode == 0 ){` |
|     7 | 1272 | `			return SXERR_ABORT;` |
|     - | 1273 | `		}` |
|    75 | 1274 | `		return PcreDoCallbackReplace(pCtx, pCode, zSubject, nSubLen, pCallback,` |
|    24 | 1275 | `			limit, iFlags, pCount, pOut);` |
|   ! 0 | 1276 | `	}else{` |
|     9 | 1277 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|     - | 1278 | `		ph7_hashmap_node *pPatNode;` |
|     - | 1279 | `		ph7_value sPat;` |
|     - | 1280 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|     - | 1281 | `		sxu32 n;` |
|     9 | 1282 | `		sxi32 rc = SXRET_OK;` |
|     9 | 1283 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|     9 | 1284 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|     9 | 1285 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|     9 | 1286 | `		pSrc = &sA; pDst = &sB;` |
|     9 | 1287 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|     9 | 1288 | `		pPatNode = pPatMap ? pPatMap->pFirst : 0;` |
|     9 | 1289 | `		n = pPatMap ? pPatMap->nEntry : 0;` |
|    23 | 1290 | `		while( n > 0 ){` |
|     - | 1291 | `			const char *zPattern;` |
|     - | 1292 | `			int nPatLen;` |
|     - | 1293 | `			pcre2_code *pCode;` |
|     - | 1294 | `			SyBlob *pSwap;` |
|    17 | 1295 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|    17 | 1296 | `			zPattern = ph7_value_to_string(&sPat, &nPatLen);` |
|    17 | 1297 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    17 | 1298 | `			if( pCode == 0 ){` |
|     3 | 1299 | `				rc = SXERR_ABORT;` |
|     3 | 1300 | `				PH7_MemObjRelease(&sPat);` |
|     3 | 1301 | `				break;` |
|     - | 1302 | `			}` |
|    15 | 1303 | `			SyBlobReset(pDst);` |
|    22 | 1304 | `			rc = PcreDoCallbackReplace(pCtx, pCode,` |
|    14 | 1305 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|     7 | 1306 | `				pCallback, limit, iFlags, pCount, pDst);` |
|     - | 1307 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|    15 | 1308 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|    15 | 1309 | `			PH7_MemObjRelease(&sPat);` |
|    15 | 1310 | `			if( rc == PH7_EXCEPTION ){` |
|   ! 0 | 1311 | `				break;` |
|     - | 1312 | `			}` |
|    15 | 1313 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|    15 | 1314 | `			n--;` |
|     1 | 1315 | `		}` |
|     9 | 1316 | `		if( rc == SXRET_OK ){` |
|     7 | 1317 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|     3 | 1318 | `		}` |
|     9 | 1319 | `		SyBlobRelease(&sA);` |
|     9 | 1320 | `		SyBlobRelease(&sB);` |
|     9 | 1321 | `		return rc;` |
|     - | 1322 | `	}` |
|    34 | 1323 | `}` |
|     - | 1324 |  |
|     - | 1325 | `/* ======================================================================` |
|     - | 1326 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count [, flags]]])` |
|     - | 1327 | ` * ====================================================================== */` |
|    50 | 1328 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1329 | `{` |
|    53 | 1330 | `	int limit = -1;` |
|    53 | 1331 | `	int iFlags = 0;` |
|    53 | 1332 | `	int count = 0;` |
|     - | 1333 | `	sxi32 rc;` |
|     - | 1334 |  |
|    53 | 1335 | `	if( nArg < 3 ){` |
|   ! 0 | 1336 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1337 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 | 1338 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1339 | `		return PH7_OK;` |
|     - | 1340 | `	}` |
|    53 | 1341 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 | 1342 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1343 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 | 1344 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1345 | `		return PH7_OK;` |
|     - | 1346 | `	}` |
|    53 | 1347 | `	if( nArg >= 4 ){` |
|    38 | 1348 | `		limit = ph7_value_to_int(apArg[3]);` |
|    18 | 1349 | `	}` |
|    53 | 1350 | `	if( nArg >= 6 ){` |
|     - | 1351 | `		/* $flags shapes the match array handed to the callback exactly as it` |
|     - | 1352 | `		 * shapes preg_match()'s &$matches (PREG_OFFSET_CAPTURE /` |
|     - | 1353 | `		 * PREG_UNMATCHED_AS_NULL). php validates nothing here, so neither do we. */` |
|    26 | 1354 | `		iFlags = ph7_value_to_int(apArg[5]);` |
|    12 | 1355 | `	}` |
|    53 | 1356 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1357 |  |
|    73 | 1358 | `	if( ph7_value_is_array(apArg[2]) ){` |
|     - | 1359 | `		/* Array subject: return an array, each element replaced, keys preserved` |
|     - | 1360 | `		 * (php; PHL used to stringify the whole array to "Array" and replace in` |
|     - | 1361 | `		 * THAT — a silent wrong answer). */` |
|    19 | 1362 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|    19 | 1363 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|    19 | 1364 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     - | 1365 | `		ph7_value sKey, sVal;` |
|     - | 1366 | `		ph7_hashmap_node *pNode;` |
|     - | 1367 | `		sxu32 n;` |
|    19 | 1368 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|   ! 0 | 1369 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1370 | `			return PH7_OK;` |
|     - | 1371 | `		}` |
|    19 | 1372 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|    19 | 1373 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    19 | 1374 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|    19 | 1375 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|    43 | 1376 | `		while( n > 0 ){` |
|     - | 1377 | `			const char *zSubject;` |
|     - | 1378 | `			int nSubLen;` |
|     - | 1379 | `			SyBlob sOut;` |
|    31 | 1380 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|    31 | 1381 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|    31 | 1382 | `			zSubject = ph7_value_to_string(&sVal, &nSubLen);` |
|    31 | 1383 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    46 | 1384 | `			rc = PcreCallbackReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen,` |
|    15 | 1385 | `				limit, iFlags, &count, &sOut);` |
|    31 | 1386 | `			if( rc != SXRET_OK ){` |
|     - | 1387 | `				/* A bad pattern with an array subject yields an empty array (php);` |
|     - | 1388 | `				 * the failure hits the first element, so pResult is still empty.` |
|     - | 1389 | `				 * A throwing callback unwinds with no result at all. */` |
|     7 | 1390 | `				SyBlobRelease(&sOut);` |
|     7 | 1391 | `				PH7_MemObjRelease(&sKey);` |
|     7 | 1392 | `				PH7_MemObjRelease(&sVal);` |
|     7 | 1393 | `				if( rc == PH7_EXCEPTION ){` |
|     3 | 1394 | `					return PH7_EXCEPTION;` |
|     - | 1395 | `				}` |
|     5 | 1396 | `				ph7_result_value(pCtx, pResult);` |
|     5 | 1397 | `				goto set_count;` |
|     - | 1398 | `			}` |
|    25 | 1399 | `			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    25 | 1400 | `			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|    25 | 1401 | `			ph7_value_reset_string_cursor(pElem);` |
|    25 | 1402 | `			SyBlobRelease(&sOut);` |
|    25 | 1403 | `			PH7_MemObjRelease(&sKey);` |
|    25 | 1404 | `			PH7_MemObjRelease(&sVal);` |
|    25 | 1405 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|    25 | 1406 | `			n--;` |
|     1 | 1407 | `		}` |
|    13 | 1408 | `		ph7_result_value(pCtx, pResult);` |
|     7 | 1409 | `	}else{` |
|     - | 1410 | `		/* Scalar subject: one replaced string. */` |
|     - | 1411 | `		const char *zSubject;` |
|     - | 1412 | `		int nSubLen;` |
|     - | 1413 | `		SyBlob sOut;` |
|    35 | 1414 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    35 | 1415 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    51 | 1416 | `		rc = PcreCallbackReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen,` |
|    16 | 1417 | `			limit, iFlags, &count, &sOut);` |
|    35 | 1418 | `		if( rc != SXRET_OK ){` |
|     5 | 1419 | `			SyBlobRelease(&sOut);` |
|     5 | 1420 | `			if( rc == PH7_EXCEPTION ){` |
|   ! 0 | 1421 | `				return PH7_EXCEPTION;` |
|     - | 1422 | `			}` |
|     - | 1423 | `			/* Scalar subject: a bad pattern returns NULL (php). */` |
|     5 | 1424 | `			ph7_result_null(pCtx);` |
|     5 | 1425 | `			goto set_count;` |
|     - | 1426 | `		}` |
|    31 | 1427 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    31 | 1428 | `		SyBlobRelease(&sOut);` |
|     - | 1429 | `	}` |
|    24 | 1430 | `set_count:` |
|     - | 1431 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|     - | 1432 | `	 * (php always writes it: 0, or the count accumulated by earlier good patterns). */` |
|    51 | 1433 | `	if( nArg >= 5 ){` |
|     - | 1434 | `		ph7_value sCount;` |
|    38 | 1435 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|    38 | 1436 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|    38 | 1437 | `		PH7_MemObjRelease(&sCount);` |
|    18 | 1438 | `	}` |
|    51 | 1439 | `	return PH7_OK;` |
|    28 | 1440 | `}` |
|     - | 1441 |  |
|     - | 1442 | `/* ======================================================================` |
|     - | 1443 | ` * preg_quote(str [, delimiter])` |
|     - | 1444 | ` * ====================================================================== */` |
|    26 | 1445 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1446 | `{` |
|    29 | 1447 | `	const char *zStr, *zDelim = 0;` |
|    29 | 1448 | `	int nLen, nDelimLen = 0;` |
|     - | 1449 | `	const char *z, *zEnd;` |
|     - | 1450 |  |
|    29 | 1451 | `	if( nArg < 1 ){` |
|   ! 0 | 1452 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1453 | `		return PH7_OK;` |
|     - | 1454 | `	}` |
|    29 | 1455 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|    29 | 1456 | `	if( nArg >= 2 ){` |
|    12 | 1457 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     5 | 1458 | `	}` |
|     - | 1459 | `	/* Type the result as a STRING up front: an empty subject quotes to the empty` |
|     - | 1460 | `	 * string, and the loop below would otherwise never touch the result at all,` |
|     - | 1461 | ``	 * leaving php's `string` return as NULL. */`` |
|    29 | 1462 | `	ph7_result_string(pCtx, "", 0);` |
|    29 | 1463 | `	z = zStr;` |
|    29 | 1464 | `	zEnd = &zStr[nLen];` |
|   651 | 1465 | `	while( z < zEnd ){` |
|   625 | 1466 | `		char c = *z;` |
|   625 | 1467 | `		if( c == '\0' ){` |
|     - | 1468 | `			/* php spells NUL as the three-digit escape "\000" (a backslash and a raw NUL` |
|     - | 1469 | `			 * byte, which is what this emitted, is not an escape at all: pcre reads the` |
|     - | 1470 | `			 * backslash as quoting the byte that FOLLOWS the NUL). */` |
|     9 | 1471 | `			ph7_result_string(pCtx, "\\000", 4);` |
|     9 | 1472 | `			z++;` |
|     9 | 1473 | `			continue;` |
|     - | 1474 | `		}` |
|   617 | 1475 | `		switch( c ){` |
|    30 | 1476 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1477 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1478 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1479 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1480 | `			case '#':` |
|    63 | 1481 | `				ph7_result_string(pCtx, "\\", 1);` |
|    63 | 1482 | `				break;` |
|   277 | 1483 | `			default:` |
|   557 | 1484 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     6 | 1485 | `					ph7_result_string(pCtx, "\\", 1);` |
|     2 | 1486 | `				}` |
|   554 | 1487 | `				break;` |
|     - | 1488 | `		}` |
|   617 | 1489 | `		ph7_result_string(pCtx, z, 1);` |
|   617 | 1490 | `		z++;` |
|     3 | 1491 | `	}` |
|    29 | 1492 | `	return PH7_OK;` |
|    16 | 1493 | `}` |
|     - | 1494 |  |
|     - | 1495 | `/* ======================================================================` |
|     - | 1496 | ` * preg_last_error()` |
|     - | 1497 | ` * ====================================================================== */` |
|   ! 0 | 1498 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1499 | `{` |
|   ! 0 | 1500 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1501 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1502 | `	return PH7_OK;` |
|   ! 0 | 1503 | `}` |
|     - | 1504 |  |
|     - | 1505 | `/* ======================================================================` |
|     - | 1506 | ` * preg_last_error_msg()` |
|     - | 1507 | ` * ====================================================================== */` |
|   ! 0 | 1508 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1509 | `{` |
|     - | 1510 | `	const char *zMsg;` |
|   ! 0 | 1511 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1512 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1513 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1514 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1515 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1516 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1517 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1518 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1519 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1520 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1521 | `	}` |
|   ! 0 | 1522 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1523 | `	return PH7_OK;` |
|   ! 0 | 1524 | `}` |
|     - | 1525 |  |
|     - | 1526 | `/* ===== Function registration table ===== */` |
|     - | 1527 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1528 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1529 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1530 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1531 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1532 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1533 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1534 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1535 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1536 | `};` |
|     - | 1537 |  |
|  3956 | 1538 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     5 | 1539 | `{` |
|     - | 1540 | `	sxu32 n;` |
| 35609 | 1541 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 31653 | 1542 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 15829 | 1543 | `	}` |
|  3961 | 1544 | `}` |
|     - | 1545 |  |
|     - | 1546 | `/* ===== Constant registration ===== */` |
|     - | 1547 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1548 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1549 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1550 | `	}` |
|     - | 1551 |  |
|    14 | 1552 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|    17 | 1553 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|    17 | 1554 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|    13 | 1555 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|   ! 0 | 1556 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|   ! 0 | 1557 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|   ! 0 | 1558 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|   ! 0 | 1559 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|   ! 0 | 1560 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|   ! 0 | 1561 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|   ! 0 | 1562 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|    13 | 1563 | `PCRE_CONST_INT(PREG_GREP_INVERT,         PHP_PREG_GREP_INVERT)` |
|   ! 0 | 1564 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|   ! 0 | 1565 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|   ! 0 | 1566 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1567 |  |
|  3956 | 1568 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     5 | 1569 | `{` |
|  3961 | 1570 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  3961 | 1571 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  3961 | 1572 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  3961 | 1573 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  3961 | 1574 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  3961 | 1575 | `	ph7_create_constant(&(*pVm), "PREG_GREP_INVERT",          PcreConst_PREG_GREP_INVERT, 0);` |
|  3961 | 1576 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  3961 | 1577 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  3961 | 1578 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  3961 | 1579 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  3961 | 1580 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  3961 | 1581 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  3961 | 1582 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  3961 | 1583 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  3961 | 1584 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  3961 | 1585 | `}` |
|     - | 1586 |  |
|     - | 1587 | `#else` |
|     - | 1588 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1589 | `typedef int vm_pcre_unused;` |
|     - | 1590 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1591 |  |
