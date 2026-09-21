# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 692/928 lines (74.57%)

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
|   366 |   56 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|     5 |   57 | `{` |
|     - |   58 | `	sxu32 i;` |
|  2639 |   59 | `	for( i = 0; i < nCacheUsed; i++ ){` |
|  2509 |   60 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|   241 |   61 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|   241 |   62 | `			if( pCaptureCount ){` |
|   241 |   63 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|   119 |   64 | `			}` |
|   241 |   65 | `			return aCache[i].pCode;` |
|     - |   66 | `		}` |
|  1137 |   67 | `	}` |
|   133 |   68 | `	return 0;` |
|   188 |   69 | `}` |
|     - |   70 |  |
|   110 |   71 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|     5 |   72 | `{` |
|     - |   73 | `	PcreCacheEntry *pEntry;` |
|     - |   74 | `	char *zCopy;` |
|     - |   75 | `	/* Allocate the pattern copy first, before touching the cache */` |
|   115 |   76 | `	zCopy = (char *)malloc(nLen + 1);` |
|   115 |   77 | `	if( zCopy == 0 ){` |
|     - |   78 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|   ! 0 |   79 | `		return;` |
|     - |   80 | `	}` |
|   115 |   81 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|   115 |   82 | `	zCopy[nLen] = 0;` |
|   115 |   83 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|    59 |   84 | `		pEntry = &aCache[nCacheUsed++];` |
|    32 |   85 | `	}else{` |
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
|   115 |  100 | `	pEntry->zPattern = zCopy;` |
|   115 |  101 | `	pEntry->nLen = nLen;` |
|   115 |  102 | `	pEntry->pCode = pCode;` |
|   115 |  103 | `	pEntry->nCaptureCount = nCaptureCount;` |
|   115 |  104 | `	pEntry->iLastUsed = ++iCacheClock;` |
|    60 |  105 | `}` |
|     - |  106 |  |
|     - |  107 | `/* ===== Delimiter parser ===== */` |
|     - |  108 | `#define PCRE_PARSE_OK             0` |
|     - |  109 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|     - |  110 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */` |
|     - |  111 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|     - |  112 |  |
|   128 |  113 | `static sxi32 PcreParsePattern(` |
|     - |  114 | `	const char *zInput, int nInputLen,` |
|     - |  115 | `	const char **pPattern, int *pnPatternLen,` |
|     - |  116 | `	const char **pFlags, int *pnFlagLen,` |
|     - |  117 | `	char *pCloseDelim, int *pbPaired)` |
|     5 |  118 | `{` |
|   133 |  119 | `	const char *zEnd = &zInput[nInputLen];` |
|   133 |  120 | `	const char *z = zInput;` |
|     - |  121 | `	char cOpen, cClose;` |
|     - |  122 | `	const char *pStart;` |
|     - |  123 |  |
|     - |  124 | `	/* Delimiter details for a "no ending delimiter" diagnostic (php names it) */` |
|   133 |  125 | `	*pCloseDelim = 0;` |
|   133 |  126 | `	*pbPaired = 0;` |
|     - |  127 | `	/* Skip leading whitespace */` |
|   133 |  128 | `	while( z < zEnd && (unsigned char)*z <= 0x20 ){` |
|   ! 0 |  129 | `		z++;` |
|   ! 0 |  130 | `	}` |
|   133 |  131 | `	if( z >= zEnd ){` |
|   ! 0 |  132 | `		return PCRE_PARSE_EMPTY;` |
|     - |  133 | `	}` |
|   133 |  134 | `	cOpen = *z;` |
|     - |  135 | `	/* Must not be alphanumeric, backslash, or whitespace */` |
|   133 |  136 | `	if( SyisAlphaNum(cOpen) \|\| cOpen == '\\' \|\| (unsigned char)cOpen <= 0x20 ){` |
|     3 |  137 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|     - |  138 | `	}` |
|     - |  139 | `	/* Paired delimiters */` |
|   131 |  140 | `	switch( cOpen ){` |
|     5 |  141 | `		case '(': cClose = ')'; break;` |
|     3 |  142 | `		case '[': cClose = ']'; break;` |
|     3 |  143 | `		case '{': cClose = '}'; break;` |
|     3 |  144 | `		case '<': cClose = '>'; break;` |
|   121 |  145 | `		default:  cClose = cOpen; break;` |
|     - |  146 | `	}` |
|   131 |  147 | `	*pCloseDelim = cClose;` |
|   131 |  148 | `	*pbPaired = (cOpen != cClose);` |
|   131 |  149 | `	z++; /* Skip opening delimiter */` |
|   131 |  150 | `	pStart = z;` |
|     - |  151 | `	/* Scan for closing delimiter, respecting backslash escapes */` |
|  1555 |  152 | `	while( z < zEnd ){` |
|  1539 |  153 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   113 |  154 | `			z += 2; /* Skip escaped char */` |
|   113 |  155 | `			continue;` |
|     - |  156 | `		}` |
|  1427 |  157 | `		if( *z == cClose ){` |
|   115 |  158 | `			break;` |
|     - |  159 | `		}` |
|  1317 |  160 | `		z++;` |
|     5 |  161 | `	}` |
|   131 |  162 | `	if( z >= zEnd ){` |
|    17 |  163 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|     - |  164 | `	}` |
|   115 |  165 | `	*pPattern = pStart;` |
|   115 |  166 | `	*pnPatternLen = (int)(z - pStart);` |
|   115 |  167 | `	z++; /* Skip closing delimiter */` |
|   115 |  168 | `	*pFlags = z;` |
|   115 |  169 | `	*pnFlagLen = (int)(zEnd - z);` |
|   115 |  170 | `	return PH7_OK;` |
|    69 |  171 | `}` |
|     - |  172 |  |
|     - |  173 | `/* ===== Flag mapper ===== */` |
|   110 |  174 | `static sxi32 PcreMapFlags(` |
|     - |  175 | `	const char *zFlags, int nFlagLen,` |
|     - |  176 | `	uint32_t *pCompileOpts)` |
|     5 |  177 | `{` |
|     - |  178 | `	int i;` |
|   115 |  179 | `	*pCompileOpts = 0;` |
|   139 |  180 | `	for( i = 0; i < nFlagLen; i++ ){` |
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
|   115 |  195 | `	return PH7_OK;` |
|     5 |  196 | `}` |
|     - |  197 |  |
|     - |  198 | `/* ===== Compile helper ===== */` |
|   366 |  199 | `static pcre2_code *PcreCompile(` |
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
|   371 |  216 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|   371 |  217 | `	if( pCode ){` |
|   241 |  218 | `		return pCode;` |
|     - |  219 | `	}` |
|     - |  220 | `	/* Parse delimiter */` |
|   133 |  221 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen,` |
|     - |  222 | `		&cDelim, &bPaired);` |
|   133 |  223 | `	if( parseRc != PCRE_PARSE_OK ){` |
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
|   115 |  239 | `	PcreMapFlags(zFlags, nFlagLen, &compileOpts);` |
|     - |  240 | `	/* Compile */` |
|   115 |  241 | `	pCode = pcre2_compile(` |
|    55 |  242 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|    55 |  243 | `		compileOpts, &errcode, &erroffset, NULL);` |
|   115 |  244 | `	if( pCode == 0 ){` |
|     - |  245 | `		PCRE2_UCHAR errbuf[256];` |
|   ! 0 |  246 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|   ! 0 |  247 | `		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,` |
|   ! 0 |  248 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|   ! 0 |  249 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|   ! 0 |  250 | `		return 0;` |
|     - |  251 | `	}` |
|     - |  252 | `	/* Get capture count */` |
|   115 |  253 | `	nCapture = 0;` |
|   115 |  254 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|   115 |  255 | `	if( pCaptureCount ){` |
|   115 |  256 | `		*pCaptureCount = nCapture;` |
|    55 |  257 | `	}` |
|     - |  258 | `	/* Cache it */` |
|   115 |  259 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|   115 |  260 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   115 |  261 | `	return pCode;` |
|   188 |  262 | `}` |
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
|   158 |  292 | `static void PcrePopulateMatches(` |
|     - |  293 | `	ph7_context *pCtx,` |
|     - |  294 | `	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */` |
|     - |  295 | `	const char *zSubject,` |
|     - |  296 | `	PCRE2_SIZE *ovector,` |
|     - |  297 | `	int nGroups,` |
|     - |  298 | `	pcre2_code *pCode,` |
|     - |  299 | `	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */` |
|     5 |  300 | `{` |
|   163 |  301 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   163 |  302 | `	ph7_value *pSub = 0;` |
|   163 |  303 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|   163 |  304 | `	PCRE2_SPTR nametable = 0;` |
|     - |  305 | `	int i;` |
|     - |  306 |  |
|   163 |  307 | `	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|     8 |  308 | `		pSub = ph7_context_new_array(pCtx);` |
|     3 |  309 | `	}` |
|     - |  310 | `	/* Read the name table up front so each group's named key can be emitted` |
|     - |  311 | `	 * INTERLEAVED with its numbered key, in group order — php stores` |
|     - |  312 | ``	 * `0, name, 1, value, 2` (named entry immediately before its number), not`` |
|     - |  313 | `	 * every number followed by every name. Code that iterates $matches or` |
|     - |  314 | `	 * var_dumps it (PHPUnit's annotation parser) depends on this order. */` |
|   163 |  315 | `	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|   163 |  316 | `	if( namecount > 0 ){` |
|     9 |  317 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     9 |  318 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|     4 |  319 | `	}` |
|   677 |  320 | `	for( i = 0; i < nGroups; i++ ){` |
|   519 |  321 | `		PCRE2_SIZE start = ovector[2 * i];` |
|   519 |  322 | `		PCRE2_SIZE end   = ovector[2 * i + 1];` |
|   519 |  323 | `		const char *zName = 0;` |
|     - |  324 | `		/* Does group i carry a (?<name>...) label? namecount is tiny in practice. */` |
|   519 |  325 | `		if( namecount > 0 ){` |
|     - |  326 | `			uint32_t k;` |
|    49 |  327 | `			for( k = 0; k < namecount; k++ ){` |
|    41 |  328 | `				PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|    41 |  329 | `				if( (((entry[0] << 8) \| entry[1])) == i ){` |
|    17 |  330 | `					zName = (const char *)(entry + 2);` |
|    17 |  331 | `					break;` |
|     - |  332 | `				}` |
|    13 |  333 | `			}` |
|    12 |  334 | `		}` |
|   519 |  335 | `		if( start == PCRE2_UNSET ){` |
|   131 |  336 | `			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  337 | `				ph7_value_null(pVal);` |
|   ! 0 |  338 | `			}else{` |
|   131 |  339 | `				ph7_value_string(pVal, "", 0);` |
|     - |  340 | `			}` |
|    66 |  341 | `		}else{` |
|   389 |  342 | `			ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|     - |  343 | `		}` |
|   519 |  344 | `		if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|     8 |  345 | `			ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|     8 |  346 | `			ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|     8 |  347 | `			ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);` |
|     8 |  348 | `			ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|     - |  349 | `			/* php: the named key comes first, then the numbered key (same value). */` |
|     8 |  350 | `			if( zName ){` |
|   ! 0 |  351 | `				ph7_array_add_strkey_elem(pArray, zName, pSub);` |
|   ! 0 |  352 | `			}` |
|     8 |  353 | `			ph7_array_add_intkey_elem(pArray, i, pSub);` |
|     8 |  354 | `			ph7_context_release_value(pCtx, pOff);` |
|     8 |  355 | `			ph7_context_release_value(pCtx, pSub);` |
|     8 |  356 | `			pSub = ph7_context_new_array(pCtx);` |
|     5 |  357 | `		}else{` |
|   513 |  358 | `			if( zName ){` |
|    17 |  359 | `				ph7_array_add_strkey_elem(pArray, zName, pVal);` |
|     8 |  360 | `			}` |
|   513 |  361 | `			ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  362 | `		}` |
|   519 |  363 | `		ph7_value_reset_string_cursor(pVal);` |
|   262 |  364 | `	}` |
|   163 |  365 | `	ph7_context_release_value(pCtx, pVal);` |
|   163 |  366 | `	if( pSub ){` |
|     8 |  367 | `		ph7_context_release_value(pCtx, pSub);` |
|     3 |  368 | `	}` |
|   163 |  369 | `}` |
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
|   204 |  409 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  410 | `{` |
|     - |  411 | `	const char *zPattern, *zSubject;` |
|     - |  412 | `	int nPatLen, nSubLen;` |
|     - |  413 | `	pcre2_code *pCode;` |
|     - |  414 | `	pcre2_match_data *pMatchData;` |
|     - |  415 | `	PCRE2_SIZE *ovector;` |
|     - |  416 | `	sxu32 nCapture;` |
|   209 |  417 | `	PCRE2_SIZE startOffset = 0;` |
|   209 |  418 | `	int iFlags = 0;` |
|     - |  419 | `	int rc;` |
|     - |  420 |  |
|   209 |  421 | `	if( nArg < 2 ){` |
|   ! 0 |  422 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  423 | `			"preg_match() expects at least 2 parameters");` |
|   ! 0 |  424 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  425 | `		return PH7_OK;` |
|     - |  426 | `	}` |
|   209 |  427 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|   209 |  428 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|   209 |  429 | `	if( nArg >= 4 ){` |
|    28 |  430 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|    13 |  431 | `	}` |
|   209 |  432 | `	if( nArg >= 5 ){` |
|   ! 0 |  433 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  434 | `	}` |
|   209 |  435 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|   209 |  436 | `	if( pCode == 0 ){` |
|    15 |  437 | `		ph7_result_bool(pCtx, 0);` |
|    15 |  438 | `		return PH7_OK;` |
|     - |  439 | `	}` |
|     - |  440 | `	/* php validates $flags AFTER the pattern compiles (a bad pattern warns first).` |
|     - |  441 | `	 * php 8.5 only rejects flag bits BELOW PREG_OFFSET_CAPTURE (the low byte); any` |
|     - |  442 | `	 * higher bit is ignored. preg_match permits none of those low bits. */` |
|   195 |  443 | `	if( (iFlags & (PHP_PREG_OFFSET_CAPTURE - 1)) != 0 ){` |
|    11 |  444 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  445 | `			"preg_match(): Argument #4 ($flags) must be a PREG_* constant");` |
|     - |  446 | `	}` |
|   185 |  447 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|   185 |  448 | `	if( pMatchData == 0 ){` |
|   ! 0 |  449 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  450 | `		return PH7_OK;` |
|     - |  451 | `	}` |
|   275 |  452 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    90 |  453 | `		startOffset, 0, pMatchData, NULL);` |
|   185 |  454 | `	if( rc < 0 ){` |
|    33 |  455 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  456 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  457 | `		}` |
|     - |  458 | `		/* Populate empty matches if requested */` |
|    33 |  459 | `		if( nArg >= 3 ){` |
|    18 |  460 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    18 |  461 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pEmpty);` |
|    18 |  462 | `			ph7_context_release_value(pCtx, pEmpty);` |
|     8 |  463 | `		}` |
|    33 |  464 | `		pcre2_match_data_free(pMatchData);` |
|    33 |  465 | `		ph7_result_int(pCtx, 0);` |
|    33 |  466 | `		return PH7_OK;` |
|     - |  467 | `	}` |
|   155 |  468 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   155 |  469 | `	if( nArg >= 3 ){` |
|     - |  470 | `		/* Populate $matches */` |
|   129 |  471 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|   129 |  472 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|   129 |  473 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);` |
|     - |  474 | `		/* Write the array back to the caller's variable */` |
|   129 |  475 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pArray);` |
|   129 |  476 | `		ph7_context_release_value(pCtx, pArray);` |
|    62 |  477 | `	}` |
|   155 |  478 | `	pcre2_match_data_free(pMatchData);` |
|   155 |  479 | `	ph7_result_int(pCtx, 1);` |
|   155 |  480 | `	return PH7_OK;` |
|   107 |  481 | `}` |
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
|   120 |  810 | `static void PcreExpandBackrefs(` |
|     - |  811 | `	SyBlob *pOut,` |
|     - |  812 | `	const char *zRepl, int nReplLen,` |
|     - |  813 | `	const char *zSubject,` |
|     - |  814 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     2 |  815 | `{` |
|   122 |  816 | `	const char *zEnd = &zRepl[nReplLen];` |
|   122 |  817 | `	const char *z = zRepl;` |
|     - |  818 |  |
|   258 |  819 | `	while( z < zEnd ){` |
|   138 |  820 | `		if( *z == '\\' && z + 1 < zEnd ){` |
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
|   138 |  840 | `		if( *z == '$' && z + 1 < zEnd ){` |
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
|   126 |  890 | `		SyBlobAppend(pOut, z, 1);` |
|   126 |  891 | `		z++;` |
|     2 |  892 | `	}` |
|   122 |  893 | `}` |
|     - |  894 |  |
|     - |  895 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|    84 |  896 | `static void PcreDoReplace(` |
|     - |  897 | `	ph7_context *pCtx,` |
|     - |  898 | `	pcre2_code *pCode,` |
|     - |  899 | `	const char *zSubject, int nSubLen,` |
|     - |  900 | `	const char *zRepl, int nReplLen,` |
|     - |  901 | `	int limit,` |
|     - |  902 | `	int *pCount,` |
|     - |  903 | `	SyBlob *pOut)` |
|     2 |  904 | `{` |
|     - |  905 | `	pcre2_match_data *pMatchData;` |
|    86 |  906 | `	PCRE2_SIZE startOffset = 0;` |
|    86 |  907 | `	int nReplacements = 0;` |
|     - |  908 | `	int rc;` |
|     - |  909 |  |
|    86 |  910 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    86 |  911 | `	if( pMatchData == 0 ) return;` |
|     - |  912 |  |
|   206 |  913 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  914 | `		PCRE2_SIZE *ovector;` |
|   204 |  915 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|   305 |  916 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|   101 |  917 | `			startOffset, 0, pMatchData, NULL);` |
|   204 |  918 | `		if( rc < 0 ){` |
|    84 |  919 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  920 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  921 | `			}` |
|    84 |  922 | `			break;` |
|     - |  923 | `		}` |
|   122 |  924 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - |  925 | `		/* Copy text before match */` |
|   122 |  926 | `		if( ovector[0] > startOffset ){` |
|    79 |  927 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|    39 |  928 | `		}` |
|     - |  929 | `		/* Expand replacement */` |
|   122 |  930 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|   122 |  931 | `		nReplacements++;` |
|     - |  932 | `		/* Advance */` |
|   122 |  933 | `		if( ovector[1] == ovector[0] ){` |
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
|   100 |  945 | `			startOffset = ovector[1];` |
|     - |  946 | `		}` |
|     2 |  947 | `	}` |
|     - |  948 | `	/* Copy remainder */` |
|    86 |  949 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|    32 |  950 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|    15 |  951 | `	}` |
|    86 |  952 | `	if( pCount ){` |
|    86 |  953 | `		*pCount += nReplacements;` |
|    42 |  954 | `	}` |
|    86 |  955 | `	pcre2_match_data_free(pMatchData);` |
|    42 |  956 | `	SXUNUSED(pCtx);` |
|    44 |  957 | `}` |
|     - |  958 |  |
|     - |  959 | `/* ===== Helper: apply pattern(s)+replacement(s) to ONE subject string =====` |
|     - |  960 | ` * pPattern is a string or an array of patterns; pRepl is a string (used for` |
|     - |  961 | ` * every pattern) or, only when pPattern is an array, an array taken by ORDER` |
|     - |  962 | ` * (missing element -> ""). Array patterns are applied sequentially, each to the` |
|     - |  963 | ` * result of the previous (PHP semantics), ping-ponging two blobs. The final` |
|     - |  964 | ` * text is appended to pOut. Returns SXRET_OK, or SXERR_ABORT on a bad pattern` |
|     - |  965 | ` * (the caller then yields NULL, matching the scalar path). */` |
|    78 |  966 | `static sxi32 PcreReplaceSubject(` |
|     - |  967 | `	ph7_context *pCtx,` |
|     - |  968 | `	ph7_value *pPattern,` |
|     - |  969 | `	ph7_value *pRepl,` |
|     - |  970 | `	const char *zSubject, int nSubLen,` |
|     - |  971 | `	int limit,` |
|     - |  972 | `	int *pCount,` |
|     - |  973 | `	SyBlob *pOut)` |
|     3 |  974 | `{` |
|     - |  975 | `	sxu32 nCapture;` |
|    81 |  976 | `	if( !ph7_value_is_array(pPattern) ){` |
|     - |  977 | `		/* Single pattern + single replacement */` |
|     - |  978 | `		const char *zPattern, *zRepl;` |
|     - |  979 | `		int nPatLen, nReplLen;` |
|     - |  980 | `		pcre2_code *pCode;` |
|    69 |  981 | `		zPattern = ph7_value_to_string(pPattern, &nPatLen);` |
|    69 |  982 | `		zRepl = ph7_value_to_string(pRepl, &nReplLen);` |
|    69 |  983 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    69 |  984 | `		if( pCode == 0 ){` |
|     3 |  985 | `			return SXERR_ABORT;` |
|     - |  986 | `		}` |
|    66 |  987 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, pCount, pOut);` |
|    66 |  988 | `		return SXRET_OK;` |
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
|    42 | 1057 | `}` |
|     - | 1058 |  |
|     - | 1059 | `/* ======================================================================` |
|     - | 1060 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - | 1061 | ` * ====================================================================== */` |
|    62 | 1062 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1063 | `{` |
|    65 | 1064 | `	int limit = -1;` |
|    65 | 1065 | `	int count = 0;` |
|     - | 1066 |  |
|    65 | 1067 | `	if( nArg < 3 ){` |
|   ! 0 | 1068 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1069 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 | 1070 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1071 | `		return PH7_OK;` |
|     - | 1072 | `	}` |
|    65 | 1073 | `	if( nArg >= 4 ){` |
|    20 | 1074 | `		limit = ph7_value_to_int(apArg[3]);` |
|     9 | 1075 | `	}` |
|    65 | 1076 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1077 |  |
|     - | 1078 | `	/* A scalar pattern with an array replacement is a parameter mismatch (PHP` |
|     - | 1079 | `	 * throws a TypeError; PHL keeps preg_replace's warning-based arg-error style). */` |
|    65 | 1080 | `	if( !ph7_value_is_array(apArg[0]) && ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 1081 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1082 | `			"Parameter mismatch, pattern is a string while replacement is an array");` |
|   ! 0 | 1083 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1084 | `		return PH7_OK;` |
|     - | 1085 | `	}` |
|    95 | 1086 | `	if( ph7_value_is_array(apArg[2]) ){` |
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
|    51 | 1134 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    51 | 1135 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    51 | 1136 | `		if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1137 | `			/* Scalar subject: a bad pattern returns NULL (PHP). */` |
|     3 | 1138 | `			SyBlobRelease(&sOut);` |
|     3 | 1139 | `			ph7_result_null(pCtx);` |
|     3 | 1140 | `			goto set_count;` |
|     - | 1141 | `		}` |
|    48 | 1142 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    48 | 1143 | `		SyBlobRelease(&sOut);` |
|     - | 1144 | `	}` |
|    31 | 1145 | `set_count:` |
|     - | 1146 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|     - | 1147 | `	 * (PHP always writes it: 0, or the count accumulated by earlier good patterns). */` |
|    65 | 1148 | `	if( nArg >= 5 ){` |
|     - | 1149 | `		ph7_value sCount;` |
|    20 | 1150 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|    20 | 1151 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|    20 | 1152 | `		PH7_MemObjRelease(&sCount);` |
|     9 | 1153 | `	}` |
|    65 | 1154 | `	return PH7_OK;` |
|    34 | 1155 | `}` |
|     - | 1156 |  |
|     - | 1157 | `/* ======================================================================` |
|     - | 1158 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count]])` |
|     - | 1159 | ` * ====================================================================== */` |
|    12 | 1160 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1161 | `{` |
|     - | 1162 | `	const char *zPattern, *zSubject;` |
|     - | 1163 | `	int nPatLen, nSubLen;` |
|     - | 1164 | `	pcre2_code *pCode;` |
|     - | 1165 | `	pcre2_match_data *pMatchData;` |
|     - | 1166 | `	sxu32 nCapture;` |
|     - | 1167 | `	SyBlob sOut;` |
|    15 | 1168 | `	PCRE2_SIZE startOffset = 0;` |
|    15 | 1169 | `	int limit = -1;` |
|    15 | 1170 | `	int count = 0;` |
|     - | 1171 | `	int rc;` |
|     - | 1172 |  |
|    15 | 1173 | `	if( nArg < 3 ){` |
|   ! 0 | 1174 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1175 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 | 1176 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1177 | `		return PH7_OK;` |
|     - | 1178 | `	}` |
|    15 | 1179 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    15 | 1180 | `	zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    15 | 1181 | `	if( nArg >= 4 ){` |
|     8 | 1182 | `		limit = ph7_value_to_int(apArg[3]);` |
|     3 | 1183 | `	}` |
|    15 | 1184 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 | 1185 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1186 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 | 1187 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1188 | `		return PH7_OK;` |
|     - | 1189 | `	}` |
|    15 | 1190 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    15 | 1191 | `	if( pCode == 0 ){` |
|   ! 0 | 1192 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1193 | `		return PH7_OK;` |
|     - | 1194 | `	}` |
|    15 | 1195 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    15 | 1196 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1197 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1198 | `		return PH7_OK;` |
|     - | 1199 | `	}` |
|    15 | 1200 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    15 | 1201 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1202 |  |
|    37 | 1203 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1204 | `		PCRE2_SIZE *ovector;` |
|     - | 1205 | `		ph7_value *pMatchArr;` |
|     - | 1206 | `		ph7_value *apCbArg[1];` |
|     - | 1207 | `		ph7_value sResult;` |
|     - | 1208 | `		const char *zReplacement;` |
|     - | 1209 | `		int nReplLen;` |
|     - | 1210 |  |
|    43 | 1211 | `		if( limit >= 0 && count >= limit ) break;` |
|    54 | 1212 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    17 | 1213 | `			startOffset, 0, pMatchData, NULL);` |
|    37 | 1214 | `		if( rc < 0 ){` |
|    15 | 1215 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1216 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1217 | `			}` |
|    15 | 1218 | `			break;` |
|     - | 1219 | `		}` |
|    25 | 1220 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1221 | `		/* Copy text before match */` |
|    25 | 1222 | `		if( ovector[0] > startOffset ){` |
|    17 | 1223 | `			SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     7 | 1224 | `		}` |
|     - | 1225 | `		/* Build matches array for callback */` |
|    25 | 1226 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    25 | 1227 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, 0);` |
|     - | 1228 | `		/* Call the callback */` |
|    25 | 1229 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    25 | 1230 | `		apCbArg[0] = pMatchArr;` |
|    25 | 1231 | `		if( PH7_VmCallUserFunction(pCtx->pVm, apArg[1], 1, apCbArg, &sResult) == PH7_EXCEPTION ){` |
|     - | 1232 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|   ! 0 | 1233 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1234 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|   ! 0 | 1235 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1236 | `			pcre2_match_data_free(pMatchData);` |
|   ! 0 | 1237 | `			return PH7_EXCEPTION;` |
|     - | 1238 | `		}` |
|     - | 1239 | `		/* Get replacement string from callback result */` |
|    25 | 1240 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    25 | 1241 | `		SyBlobAppend(&sOut, zReplacement, (sxu32)nReplLen);` |
|    25 | 1242 | `		PH7_MemObjRelease(&sResult);` |
|    25 | 1243 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    25 | 1244 | `		count++;` |
|     - | 1245 | `		/* Advance */` |
|    25 | 1246 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 | 1247 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1248 | `				SyBlobAppend(&sOut, &zSubject[startOffset], 1);` |
|   ! 0 | 1249 | `			}` |
|   ! 0 | 1250 | `			startOffset = ovector[0] + 1;` |
|   ! 0 | 1251 | `		}else{` |
|    25 | 1252 | `			startOffset = ovector[1];` |
|     - | 1253 | `		}` |
|     3 | 1254 | `	}` |
|     - | 1255 | `	/* Copy remainder */` |
|    15 | 1256 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|     5 | 1257 | `		SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|     2 | 1258 | `	}` |
|    15 | 1259 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    15 | 1260 | `	SyBlobRelease(&sOut);` |
|    15 | 1261 | `	pcre2_match_data_free(pMatchData);` |
|     - | 1262 | `	/* Set &$count if provided */` |
|    15 | 1263 | `	if( nArg >= 5 ){` |
|     - | 1264 | `		ph7_value sCount;` |
|     3 | 1265 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     3 | 1266 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|     3 | 1267 | `		PH7_MemObjRelease(&sCount);` |
|     1 | 1268 | `	}` |
|    15 | 1269 | `	return PH7_OK;` |
|     9 | 1270 | `}` |
|     - | 1271 |  |
|     - | 1272 | `/* ======================================================================` |
|     - | 1273 | ` * preg_quote(str [, delimiter])` |
|     - | 1274 | ` * ====================================================================== */` |
|     8 | 1275 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1276 | `{` |
|     9 | 1277 | `	const char *zStr, *zDelim = 0;` |
|     9 | 1278 | `	int nLen, nDelimLen = 0;` |
|     - | 1279 | `	const char *z, *zEnd;` |
|     - | 1280 |  |
|     9 | 1281 | `	if( nArg < 1 ){` |
|   ! 0 | 1282 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1283 | `		return PH7_OK;` |
|     - | 1284 | `	}` |
|     9 | 1285 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|     9 | 1286 | `	if( nArg >= 2 ){` |
|     5 | 1287 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     2 | 1288 | `	}` |
|     9 | 1289 | `	z = zStr;` |
|     9 | 1290 | `	zEnd = &zStr[nLen];` |
|    75 | 1291 | `	while( z < zEnd ){` |
|    67 | 1292 | `		char c = *z;` |
|    67 | 1293 | `		switch( c ){` |
|     5 | 1294 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1295 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1296 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1297 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1298 | `			case '#':` |
|    11 | 1299 | `				ph7_result_string(pCtx, "\\", 1);` |
|    11 | 1300 | `				break;` |
|    28 | 1301 | `			default:` |
|    57 | 1302 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     3 | 1303 | `					ph7_result_string(pCtx, "\\", 1);` |
|     1 | 1304 | `				}` |
|    56 | 1305 | `				break;` |
|     - | 1306 | `		}` |
|    67 | 1307 | `		ph7_result_string(pCtx, z, 1);` |
|    67 | 1308 | `		z++;` |
|     1 | 1309 | `	}` |
|     9 | 1310 | `	return PH7_OK;` |
|     5 | 1311 | `}` |
|     - | 1312 |  |
|     - | 1313 | `/* ======================================================================` |
|     - | 1314 | ` * preg_last_error()` |
|     - | 1315 | ` * ====================================================================== */` |
|   ! 0 | 1316 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1317 | `{` |
|   ! 0 | 1318 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1319 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1320 | `	return PH7_OK;` |
|   ! 0 | 1321 | `}` |
|     - | 1322 |  |
|     - | 1323 | `/* ======================================================================` |
|     - | 1324 | ` * preg_last_error_msg()` |
|     - | 1325 | ` * ====================================================================== */` |
|   ! 0 | 1326 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1327 | `{` |
|     - | 1328 | `	const char *zMsg;` |
|   ! 0 | 1329 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1330 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1331 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1332 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1333 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1334 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1335 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1336 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1337 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1338 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1339 | `	}` |
|   ! 0 | 1340 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1341 | `	return PH7_OK;` |
|   ! 0 | 1342 | `}` |
|     - | 1343 |  |
|     - | 1344 | `/* ===== Function registration table ===== */` |
|     - | 1345 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1346 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1347 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1348 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1349 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1350 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1351 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1352 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1353 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1354 | `};` |
|     - | 1355 |  |
|  3646 | 1356 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     5 | 1357 | `{` |
|     - | 1358 | `	sxu32 n;` |
| 32819 | 1359 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 29173 | 1360 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 14589 | 1361 | `	}` |
|  3651 | 1362 | `}` |
|     - | 1363 |  |
|     - | 1364 | `/* ===== Constant registration ===== */` |
|     - | 1365 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1366 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1367 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1368 | `	}` |
|     - | 1369 |  |
|    14 | 1370 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|    17 | 1371 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|    14 | 1372 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|     8 | 1373 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|   ! 0 | 1374 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|   ! 0 | 1375 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|   ! 0 | 1376 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|   ! 0 | 1377 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|   ! 0 | 1378 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|   ! 0 | 1379 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|   ! 0 | 1380 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|    13 | 1381 | `PCRE_CONST_INT(PREG_GREP_INVERT,         PHP_PREG_GREP_INVERT)` |
|   ! 0 | 1382 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|   ! 0 | 1383 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|   ! 0 | 1384 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1385 |  |
|  3646 | 1386 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     5 | 1387 | `{` |
|  3651 | 1388 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  3651 | 1389 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  3651 | 1390 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  3651 | 1391 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  3651 | 1392 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  3651 | 1393 | `	ph7_create_constant(&(*pVm), "PREG_GREP_INVERT",          PcreConst_PREG_GREP_INVERT, 0);` |
|  3651 | 1394 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  3651 | 1395 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  3651 | 1396 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  3651 | 1397 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  3651 | 1398 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  3651 | 1399 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  3651 | 1400 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  3651 | 1401 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  3651 | 1402 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  3651 | 1403 | `}` |
|     - | 1404 |  |
|     - | 1405 | `#else` |
|     - | 1406 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1407 | `typedef int vm_pcre_unused;` |
|     - | 1408 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1409 |  |
