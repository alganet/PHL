# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 886/1097 lines (80.77%)

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
|   718 |   56 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|     5 |   57 | `{` |
|     - |   58 | `	sxu32 i;` |
|  6659 |   59 | `	for( i = 0; i < nCacheUsed; i++ ){` |
|  6480 |   60 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|   543 |   61 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|   543 |   62 | `			if( pCaptureCount ){` |
|   505 |   63 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|   251 |   64 | `			}` |
|   543 |   65 | `			return aCache[i].pCode;` |
|     - |   66 | `		}` |
|  2971 |   67 | `	}` |
|   183 |   68 | `	return 0;` |
|   364 |   69 | `}` |
|     - |   70 |  |
|   148 |   71 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|     5 |   72 | `{` |
|     - |   73 | `	PcreCacheEntry *pEntry;` |
|     - |   74 | `	char *zCopy;` |
|     - |   75 | `	/* Allocate the pattern copy first, before touching the cache */` |
|   153 |   76 | `	zCopy = (char *)malloc(nLen + 1);` |
|   153 |   77 | `	if( zCopy == 0 ){` |
|     - |   78 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|   ! 0 |   79 | `		return;` |
|     - |   80 | `	}` |
|   153 |   81 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|   153 |   82 | `	zCopy[nLen] = 0;` |
|   153 |   83 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|    65 |   84 | `		pEntry = &aCache[nCacheUsed++];` |
|    35 |   85 | `	}else{` |
|     - |   86 | `		/* Evict LRU */` |
|    89 |   87 | `		sxu32 iMin = aCache[0].iLastUsed;` |
|    89 |   88 | `		sxu32 iMinIdx = 0;` |
|     - |   89 | `		sxu32 i;` |
|  1409 |   90 | `		for( i = 1; i < PCRE_CACHE_SIZE; i++ ){` |
|  1321 |   91 | `			if( aCache[i].iLastUsed < iMin ){` |
|   203 |   92 | `				iMin = aCache[i].iLastUsed;` |
|   203 |   93 | `				iMinIdx = i;` |
|   101 |   94 | `			}` |
|   661 |   95 | `		}` |
|    89 |   96 | `		pEntry = &aCache[iMinIdx];` |
|    89 |   97 | `		pcre2_code_free(pEntry->pCode);` |
|    89 |   98 | `		free(pEntry->zPattern);` |
|     - |   99 | `	}` |
|   153 |  100 | `	pEntry->zPattern = zCopy;` |
|   153 |  101 | `	pEntry->nLen = nLen;` |
|   153 |  102 | `	pEntry->pCode = pCode;` |
|   153 |  103 | `	pEntry->nCaptureCount = nCaptureCount;` |
|   153 |  104 | `	pEntry->iLastUsed = ++iCacheClock;` |
|    79 |  105 | `}` |
|     - |  106 |  |
|     - |  107 | `/* ===== Delimiter parser ===== */` |
|     - |  108 | `#define PCRE_PARSE_OK             0` |
|     - |  109 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|     - |  110 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */` |
|     - |  111 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|     - |  112 |  |
|   178 |  113 | `static sxi32 PcreParsePattern(` |
|     - |  114 | `	const char *zInput, int nInputLen,` |
|     - |  115 | `	const char **pPattern, int *pnPatternLen,` |
|     - |  116 | `	const char **pFlags, int *pnFlagLen,` |
|     - |  117 | `	char *pCloseDelim, int *pbPaired)` |
|     5 |  118 | `{` |
|   183 |  119 | `	const char *zEnd = &zInput[nInputLen];` |
|   183 |  120 | `	const char *z = zInput;` |
|     - |  121 | `	char cOpen, cClose;` |
|     - |  122 | `	const char *pStart;` |
|     - |  123 |  |
|     - |  124 | `	/* Delimiter details for a "no ending delimiter" diagnostic (php names it) */` |
|   183 |  125 | `	*pCloseDelim = 0;` |
|   183 |  126 | `	*pbPaired = 0;` |
|     - |  127 | `	/* Skip leading whitespace */` |
|   183 |  128 | `	while( z < zEnd && (unsigned char)*z <= 0x20 ){` |
|   ! 0 |  129 | `		z++;` |
|   ! 0 |  130 | `	}` |
|   183 |  131 | `	if( z >= zEnd ){` |
|   ! 0 |  132 | `		return PCRE_PARSE_EMPTY;` |
|     - |  133 | `	}` |
|   183 |  134 | `	cOpen = *z;` |
|     - |  135 | `	/* Must not be alphanumeric, backslash, or whitespace */` |
|   183 |  136 | `	if( SyisAlphaNum(cOpen) \|\| cOpen == '\\' \|\| (unsigned char)cOpen <= 0x20 ){` |
|     6 |  137 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|     - |  138 | `	}` |
|     - |  139 | `	/* Paired delimiters */` |
|   179 |  140 | `	switch( cOpen ){` |
|     5 |  141 | `		case '(': cClose = ')'; break;` |
|     3 |  142 | `		case '[': cClose = ']'; break;` |
|     3 |  143 | `		case '{': cClose = '}'; break;` |
|     3 |  144 | `		case '<': cClose = '>'; break;` |
|   169 |  145 | `		default:  cClose = cOpen; break;` |
|     - |  146 | `	}` |
|   179 |  147 | `	*pCloseDelim = cClose;` |
|   179 |  148 | `	*pbPaired = (cOpen != cClose);` |
|   179 |  149 | `	z++; /* Skip opening delimiter */` |
|   179 |  150 | `	pStart = z;` |
|     - |  151 | `	/* Scan for closing delimiter, respecting backslash escapes */` |
|  1411 |  152 | `	while( z < zEnd ){` |
|  1395 |  153 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|    96 |  154 | `			z += 2; /* Skip escaped char */` |
|    96 |  155 | `			continue;` |
|     - |  156 | `		}` |
|  1301 |  157 | `		if( *z == cClose ){` |
|   163 |  158 | `			break;` |
|     - |  159 | `		}` |
|  1143 |  160 | `		z++;` |
|     5 |  161 | `	}` |
|   179 |  162 | `	if( z >= zEnd ){` |
|    17 |  163 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|     - |  164 | `	}` |
|   163 |  165 | `	*pPattern = pStart;` |
|   163 |  166 | `	*pnPatternLen = (int)(z - pStart);` |
|   163 |  167 | `	z++; /* Skip closing delimiter */` |
|   163 |  168 | `	*pFlags = z;` |
|   163 |  169 | `	*pnFlagLen = (int)(zEnd - z);` |
|   163 |  170 | `	return PH7_OK;` |
|    94 |  171 | `}` |
|     - |  172 |  |
|     - |  173 | `/* ===== Flag mapper ===== */` |
|   158 |  174 | `static sxi32 PcreMapFlags(` |
|     - |  175 | `	const char *zFlags, int nFlagLen,` |
|     - |  176 | `	uint32_t *pCompileOpts)` |
|     5 |  177 | `{` |
|     - |  178 | `	int i;` |
|   163 |  179 | `	*pCompileOpts = 0;` |
|   187 |  180 | `	for( i = 0; i < nFlagLen; i++ ){` |
|    25 |  181 | `		switch( zFlags[i] ){` |
|    11 |  182 | `			case 'i': *pCompileOpts \|= PCRE2_CASELESS; break;` |
|     7 |  183 | `			case 'm': *pCompileOpts \|= PCRE2_MULTILINE; break;` |
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
|   163 |  195 | `	return PH7_OK;` |
|     5 |  196 | `}` |
|     - |  197 |  |
|     - |  198 | `/* ===== Compile helper =====` |
|     - |  199 | ` *` |
|     - |  200 | ` * PcreCompileQuiet is the whole of it; PcreCompile is that plus php's E_WARNING.` |
|     - |  201 | ` * The split exists because a caller may have to WORD the failure itself: php's` |
|     - |  202 | ` * SPL wraps the compile in zend_replace_error_handling(EH_THROW,` |
|     - |  203 | `` * InvalidArgumentException), so `new RegexIterator($it, 'nodelim')` raises an`` |
|     - |  204 | ` * exception carrying this exact text instead of warning. Nothing else may` |
|     - |  205 | ` * reproduce these four messages -- they are php's, verbatim, in one place.` |
|     - |  206 | ` */` |
|   718 |  207 | `static pcre2_code *PcreCompileQuiet(` |
|     - |  208 | `	ph7_vm *pVm,` |
|     - |  209 | `	const char *zFullPattern, int nLen,` |
|     - |  210 | `	sxu32 *pCaptureCount,` |
|     - |  211 | `	char *zErr, sxu32 nErr)` |
|     5 |  212 | `{` |
|     - |  213 | `	const char *zPat, *zFlags;` |
|     - |  214 | `	int nPatLen, nFlagLen;` |
|     - |  215 | `	uint32_t compileOpts;` |
|     - |  216 | `	pcre2_code *pCode;` |
|     - |  217 | `	PCRE2_SIZE erroffset;` |
|     - |  218 | `	int errcode;` |
|     - |  219 | `	sxu32 nCapture;` |
|     - |  220 | `	sxi32 parseRc;` |
|     - |  221 | `	char cDelim;` |
|     - |  222 | `	int bPaired;` |
|     - |  223 |  |
|   723 |  224 | `	if( nErr > 0 ){` |
|   723 |  225 | `		zErr[0] = 0;` |
|   359 |  226 | `	}` |
|     - |  227 | `	/* Check cache first */` |
|   723 |  228 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|   723 |  229 | `	if( pCode ){` |
|   543 |  230 | `		return pCode;` |
|     - |  231 | `	}` |
|     - |  232 | `	/* Parse delimiter */` |
|   183 |  233 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen,` |
|     - |  234 | `		&cDelim, &bPaired);` |
|   183 |  235 | `	if( parseRc != PCRE_PARSE_OK ){` |
|    22 |  236 | `		if( parseRc == PCRE_PARSE_EMPTY ){` |
|   ! 0 |  237 | `			SyBufferFormat(zErr, nErr, "Empty regular expression");` |
|    22 |  238 | `		}else if( parseRc == PCRE_PARSE_BAD_DELIMITER ){` |
|     6 |  239 | `			SyBufferFormat(zErr, nErr,` |
|     - |  240 | `				"Delimiter must not be alphanumeric, backslash, or NUL byte");` |
|     4 |  241 | `		}else{` |
|     - |  242 | `			/* php names the delimiter, and distinguishes paired delimiters */` |
|    25 |  243 | `			SyBufferFormat(zErr, nErr,` |
|    16 |  244 | `				bPaired ? "No ending matching delimiter '%c' found"` |
|     8 |  245 | `				        : "No ending delimiter '%c' found", cDelim);` |
|     - |  246 | `		}` |
|    22 |  247 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|    22 |  248 | `		return 0;` |
|     - |  249 | `	}` |
|     - |  250 | `	/* Map flags */` |
|   163 |  251 | `	PcreMapFlags(zFlags, nFlagLen, &compileOpts);` |
|     - |  252 | `	/* Compile */` |
|   163 |  253 | `	pCode = pcre2_compile(` |
|    79 |  254 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|    79 |  255 | `		compileOpts, &errcode, &erroffset, NULL);` |
|   163 |  256 | `	if( pCode == 0 ){` |
|     - |  257 | `		PCRE2_UCHAR errbuf[256];` |
|    11 |  258 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|    16 |  259 | `		SyBufferFormat(zErr, nErr,` |
|     5 |  260 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|    11 |  261 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|    11 |  262 | `		return 0;` |
|     - |  263 | `	}` |
|     - |  264 | `	/* Get capture count */` |
|   153 |  265 | `	nCapture = 0;` |
|   153 |  266 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|   153 |  267 | `	if( pCaptureCount ){` |
|   133 |  268 | `		*pCaptureCount = nCapture;` |
|    64 |  269 | `	}` |
|     - |  270 | `	/* Cache it */` |
|   153 |  271 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|   153 |  272 | `	pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   153 |  273 | `	return pCode;` |
|   364 |  274 | `}` |
|   658 |  275 | `static pcre2_code *PcreCompile(` |
|     - |  276 | `	ph7_context *pCtx,` |
|     - |  277 | `	const char *zFullPattern, int nLen,` |
|     - |  278 | `	sxu32 *pCaptureCount)` |
|     5 |  279 | `{` |
|     - |  280 | `	char zErr[288];` |
|   992 |  281 | `	pcre2_code *pCode = PcreCompileQuiet(pCtx->pVm, zFullPattern, nLen, pCaptureCount,` |
|   329 |  282 | `		zErr, sizeof(zErr));` |
|   663 |  283 | `	if( pCode == 0 && zErr[0] ){` |
|    30 |  284 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, zErr);` |
|    14 |  285 | `	}` |
|   663 |  286 | `	return pCode;` |
|     5 |  287 | `}` |
|     - |  288 | `/*` |
|     - |  289 | ` * Validate a pattern for a caller that raises its own diagnostic (php's SPL` |
|     - |  290 | ` * promotes the warning to an InvalidArgumentException). Answers TRUE when the` |
|     - |  291 | ` * pattern compiles; otherwise FALSE with php's text in zErr. The compiled code` |
|     - |  292 | ` * stays in the pattern cache, so a later match pays nothing for this.` |
|     - |  293 | ` */` |
|    60 |  294 | `PH7_PRIVATE int PH7_PcrePatternCheck(ph7_vm *pVm, const char *zPattern, int nLen,` |
|     - |  295 | `	char *zErr, sxu32 nErr)` |
|     1 |  296 | `{` |
|    61 |  297 | `	return PcreCompileQuiet(&(*pVm), zPattern, nLen, 0, zErr, nErr) != 0;` |
|     1 |  298 | `}` |
|     - |  299 |  |
|     - |  300 | `/* ===== Map PCRE2 match error to PHP error code ===== */` |
|   ! 0 |  301 | `static void PcreSetMatchError(ph7_vm *pVm, int rc)` |
|   ! 0 |  302 | `{` |
|   ! 0 |  303 | `	if( rc == PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  304 | `		pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   ! 0 |  305 | `	}else if( rc == PCRE2_ERROR_MATCHLIMIT ){` |
|   ! 0 |  306 | `		pVm->iPcreLastError = PHP_PREG_BACKTRACK_LIMIT_ERROR;` |
|   ! 0 |  307 | `	}else if( rc == PCRE2_ERROR_DEPTHLIMIT` |
|     - |  308 | `#ifdef PCRE2_ERROR_RECURSIONLIMIT` |
|   ! 0 |  309 | `		\|\| rc == PCRE2_ERROR_RECURSIONLIMIT` |
|     - |  310 | `#endif` |
|     - |  311 | `	){` |
|   ! 0 |  312 | `		pVm->iPcreLastError = PHP_PREG_RECURSION_LIMIT_ERROR;` |
|   ! 0 |  313 | `	}else if( rc == PCRE2_ERROR_BADUTFOFFSET ){` |
|   ! 0 |  314 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_OFFSET_ERROR;` |
|   ! 0 |  315 | `	}else if( rc == PCRE2_ERROR_UTF8_ERR1` |
|   ! 0 |  316 | `		\|\| rc == PCRE2_ERROR_UTF8_ERR2 ){` |
|   ! 0 |  317 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_ERROR;` |
|     - |  318 | `#ifdef PCRE2_ERROR_JIT_STACKLIMIT` |
|   ! 0 |  319 | `	}else if( rc == PCRE2_ERROR_JIT_STACKLIMIT ){` |
|   ! 0 |  320 | `		pVm->iPcreLastError = PHP_PREG_JIT_STACKLIMIT_ERROR;` |
|     - |  321 | `#endif` |
|   ! 0 |  322 | `	}else{` |
|   ! 0 |  323 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|     - |  324 | `	}` |
|   ! 0 |  325 | `}` |
|     - |  326 |  |
|     - |  327 | `/* ===== Helper: populate matches array from ovector ===== */` |
|   164 |  328 | `static void PcrePopulateMatches(` |
|     - |  329 | `	ph7_context *pCtx,` |
|     - |  330 | `	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */` |
|     - |  331 | `	const char *zSubject,` |
|     - |  332 | `	PCRE2_SIZE *ovector,` |
|     - |  333 | `	int nGroups,` |
|     - |  334 | `	pcre2_code *pCode,` |
|     - |  335 | `	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */` |
|     5 |  336 | `{` |
|   169 |  337 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   169 |  338 | `	ph7_value *pSub = 0;` |
|   169 |  339 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|   169 |  340 | `	PCRE2_SPTR nametable = 0;` |
|     - |  341 | `	int i;` |
|     - |  342 |  |
|   169 |  343 | `	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|    13 |  344 | `		pSub = ph7_context_new_array(pCtx);` |
|     5 |  345 | `	}` |
|     - |  346 | `	/* Read the name table up front so each group's named key can be emitted` |
|     - |  347 | `	 * INTERLEAVED with its numbered key, in group order — php stores` |
|     - |  348 | ``	 * `0, name, 1, value, 2` (named entry immediately before its number), not`` |
|     - |  349 | `	 * every number followed by every name. Code that iterates $matches or` |
|     - |  350 | `	 * var_dumps it (PHPUnit's annotation parser) depends on this order. */` |
|   169 |  351 | `	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|   169 |  352 | `	if( namecount > 0 ){` |
|     9 |  353 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     9 |  354 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|     4 |  355 | `	}` |
|   441 |  356 | `	for( i = 0; i < nGroups; i++ ){` |
|   277 |  357 | `		PCRE2_SIZE start = ovector[2 * i];` |
|   277 |  358 | `		PCRE2_SIZE end   = ovector[2 * i + 1];` |
|   277 |  359 | `		const char *zName = 0;` |
|     - |  360 | `		/* Does group i carry a (?<name>...) label? namecount is tiny in practice. */` |
|   277 |  361 | `		if( namecount > 0 ){` |
|     - |  362 | `			uint32_t k;` |
|    49 |  363 | `			for( k = 0; k < namecount; k++ ){` |
|    41 |  364 | `				PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|    41 |  365 | `				if( (((entry[0] << 8) \| entry[1])) == i ){` |
|    17 |  366 | `					zName = (const char *)(entry + 2);` |
|    17 |  367 | `					break;` |
|     - |  368 | `				}` |
|    13 |  369 | `			}` |
|    12 |  370 | `		}` |
|   277 |  371 | `		if( start == PCRE2_UNSET ){` |
|     5 |  372 | `			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|     5 |  373 | `				ph7_value_null(pVal);` |
|     3 |  374 | `			}else{` |
|   ! 0 |  375 | `				ph7_value_string(pVal, "", 0);` |
|     - |  376 | `			}` |
|     3 |  377 | `		}else{` |
|   273 |  378 | `			ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|     - |  379 | `		}` |
|   277 |  380 | `		if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|    17 |  381 | `			ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|    17 |  382 | `			ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|    17 |  383 | `			ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);` |
|    17 |  384 | `			ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|     - |  385 | `			/* php: the named key comes first, then the numbered key (same value). */` |
|    17 |  386 | `			if( zName ){` |
|   ! 0 |  387 | `				ph7_array_add_strkey_elem(pArray, zName, pSub);` |
|   ! 0 |  388 | `			}` |
|    17 |  389 | `			ph7_array_add_intkey_elem(pArray, i, pSub);` |
|    17 |  390 | `			ph7_context_release_value(pCtx, pOff);` |
|    17 |  391 | `			ph7_context_release_value(pCtx, pSub);` |
|    17 |  392 | `			pSub = ph7_context_new_array(pCtx);` |
|    10 |  393 | `		}else{` |
|   263 |  394 | `			if( zName ){` |
|    17 |  395 | `				ph7_array_add_strkey_elem(pArray, zName, pVal);` |
|     8 |  396 | `			}` |
|   263 |  397 | `			ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  398 | `		}` |
|   277 |  399 | `		ph7_value_reset_string_cursor(pVal);` |
|   141 |  400 | `	}` |
|   169 |  401 | `	ph7_context_release_value(pCtx, pVal);` |
|   169 |  402 | `	if( pSub ){` |
|    13 |  403 | `		ph7_context_release_value(pCtx, pSub);` |
|     5 |  404 | `	}` |
|   169 |  405 | `}` |
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
|   186 |  445 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  446 | `{` |
|     - |  447 | `	const char *zPattern, *zSubject;` |
|     - |  448 | `	int nPatLen, nSubLen;` |
|     - |  449 | `	pcre2_code *pCode;` |
|     - |  450 | `	pcre2_match_data *pMatchData;` |
|     - |  451 | `	PCRE2_SIZE *ovector;` |
|     - |  452 | `	sxu32 nCapture;` |
|   191 |  453 | `	PCRE2_SIZE startOffset = 0;` |
|   191 |  454 | `	int iFlags = 0;` |
|     - |  455 | `	int rc;` |
|     - |  456 |  |
|   191 |  457 | `	if( nArg < 2 ){` |
|   ! 0 |  458 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  459 | `			"preg_match() expects at least 2 parameters");` |
|   ! 0 |  460 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  461 | `		return PH7_OK;` |
|     - |  462 | `	}` |
|   191 |  463 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|   191 |  464 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|   191 |  465 | `	if( nArg >= 4 ){` |
|    42 |  466 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|    20 |  467 | `	}` |
|   191 |  468 | `	if( nArg >= 5 ){` |
|   ! 0 |  469 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  470 | `	}` |
|   191 |  471 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|   191 |  472 | `	if( pCode == 0 ){` |
|    15 |  473 | `		ph7_result_bool(pCtx, 0);` |
|    15 |  474 | `		return PH7_OK;` |
|     - |  475 | `	}` |
|     - |  476 | `	/* php validates $flags AFTER the pattern compiles (a bad pattern warns first).` |
|     - |  477 | `	 * php 8.5 only rejects flag bits BELOW PREG_OFFSET_CAPTURE (the low byte); any` |
|     - |  478 | `	 * higher bit is ignored. preg_match permits none of those low bits. */` |
|   177 |  479 | `	if( (iFlags & (PHP_PREG_OFFSET_CAPTURE - 1)) != 0 ){` |
|    11 |  480 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  481 | `			"preg_match(): Argument #4 ($flags) must be a PREG_* constant");` |
|     - |  482 | `	}` |
|   167 |  483 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|   167 |  484 | `	if( pMatchData == 0 ){` |
|   ! 0 |  485 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  486 | `		return PH7_OK;` |
|     - |  487 | `	}` |
|   248 |  488 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    81 |  489 | `		startOffset, 0, pMatchData, NULL);` |
|   167 |  490 | `	if( rc < 0 ){` |
|    51 |  491 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  492 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  493 | `		}` |
|     - |  494 | `		/* Populate empty matches if requested */` |
|    51 |  495 | `		if( nArg >= 3 ){` |
|    28 |  496 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    28 |  497 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pEmpty);` |
|    28 |  498 | `			ph7_context_release_value(pCtx, pEmpty);` |
|    13 |  499 | `		}` |
|    51 |  500 | `		pcre2_match_data_free(pMatchData);` |
|    51 |  501 | `		ph7_result_int(pCtx, 0);` |
|    51 |  502 | `		return PH7_OK;` |
|     - |  503 | `	}` |
|   119 |  504 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   119 |  505 | `	if( nArg >= 3 ){` |
|     - |  506 | `		/* Populate $matches */` |
|    77 |  507 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    77 |  508 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    77 |  509 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);` |
|     - |  510 | `		/* Write the array back to the caller's variable */` |
|    77 |  511 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pArray);` |
|    77 |  512 | `		ph7_context_release_value(pCtx, pArray);` |
|    36 |  513 | `	}` |
|   119 |  514 | `	pcre2_match_data_free(pMatchData);` |
|   119 |  515 | `	ph7_result_int(pCtx, 1);` |
|   119 |  516 | `	return PH7_OK;` |
|    98 |  517 | `}` |
|     - |  518 |  |
|     - |  519 | `/* ======================================================================` |
|     - |  520 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  521 | ` * ====================================================================== */` |
|    36 |  522 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |  523 | `{` |
|     - |  524 | `	const char *zPattern, *zSubject;` |
|     - |  525 | `	int nPatLen, nSubLen;` |
|     - |  526 | `	pcre2_code *pCode;` |
|     - |  527 | `	pcre2_match_data *pMatchData;` |
|     - |  528 | `	sxu32 nCapture;` |
|    38 |  529 | `	PCRE2_SIZE startOffset = 0;` |
|    38 |  530 | `	int iFlags = PHP_PREG_PATTERN_ORDER;` |
|    38 |  531 | `	int totalMatches = 0;` |
|     - |  532 | `	int rc;` |
|     - |  533 |  |
|    38 |  534 | `	if( nArg < 2 ){` |
|   ! 0 |  535 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  536 | `			"preg_match_all() expects at least 2 parameters");` |
|   ! 0 |  537 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  538 | `		return PH7_OK;` |
|     - |  539 | `	}` |
|    38 |  540 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    38 |  541 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    38 |  542 | `	if( nArg >= 4 ){` |
|    30 |  543 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|    14 |  544 | `	}` |
|    38 |  545 | `	if( nArg >= 5 ){` |
|   ! 0 |  546 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  547 | `	}` |
|    38 |  548 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    38 |  549 | `	if( pCode == 0 ){` |
|   ! 0 |  550 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  551 | `		return PH7_OK;` |
|     - |  552 | `	}` |
|     - |  553 | `	/* php validates $flags AFTER the pattern compiles (a bad pattern warns first).` |
|     - |  554 | `	 * php 8.5 rejects low-byte bits below PREG_OFFSET_CAPTURE EXCEPT the order flags,` |
|     - |  555 | `	 * and rejects PATTERN_ORDER+SET_ORDER together (mutually exclusive); higher bits` |
|     - |  556 | `	 * are ignored. Every case raises the same ValueError. */` |
|    36 |  557 | `	if( (iFlags & (PHP_PREG_OFFSET_CAPTURE - 1)` |
|    36 |  558 | `			& ~(PHP_PREG_PATTERN_ORDER\|PHP_PREG_SET_ORDER)) != 0` |
|    36 |  559 | `		\|\| ((iFlags & PHP_PREG_PATTERN_ORDER) && (iFlags & PHP_PREG_SET_ORDER)) ){` |
|     9 |  560 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  561 | `			"preg_match_all(): Argument #4 ($flags) must be a PREG_* constant");` |
|     - |  562 | `	}` |
|    30 |  563 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    30 |  564 | `	if( pMatchData == 0 ){` |
|   ! 0 |  565 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  566 | `		return PH7_OK;` |
|     - |  567 | `	}` |
|    30 |  568 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  569 | `	{` |
|    30 |  570 | `		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;` |
|     - |  571 |  |
|    30 |  572 | `		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){` |
|    22 |  573 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  574 | `				PCRE2_SIZE *ovector;` |
|    32 |  575 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    10 |  576 | `					startOffset, 0, pMatchData, NULL);` |
|    22 |  577 | `				if( rc < 0 ){` |
|    10 |  578 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|    10 |  579 | `					break;` |
|     - |  580 | `				}` |
|    14 |  581 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    14 |  582 | `				if( pOutArray ){` |
|    14 |  583 | `					ph7_value *pSet = ph7_context_new_array(pCtx);` |
|    14 |  584 | `					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, iFlags & ~0xFF);` |
|    14 |  585 | `					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);` |
|    14 |  586 | `					ph7_context_release_value(pCtx, pSet);` |
|     6 |  587 | `				}` |
|    14 |  588 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  589 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  590 | `				}else{` |
|    14 |  591 | `					startOffset = ovector[1];` |
|     - |  592 | `				}` |
|    14 |  593 | `				totalMatches++;` |
|     2 |  594 | `			}` |
|     6 |  595 | `		}else{` |
|     - |  596 | `			/* PREG_PATTERN_ORDER (default) */` |
|    22 |  597 | `			ph7_value **apGroupArrays = 0;` |
|    22 |  598 | `			sxu32 nGroups = nCapture + 1;` |
|     - |  599 | `			sxu32 g;` |
|    22 |  600 | `			if( pOutArray ){` |
|    32 |  601 | `				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,` |
|    10 |  602 | `					sizeof(ph7_value *) * nGroups, TRUE, FALSE);` |
|    22 |  603 | `				if( apGroupArrays ){` |
|    54 |  604 | `					for( g = 0; g < nGroups; g++ ){` |
|    34 |  605 | `						apGroupArrays[g] = ph7_context_new_array(pCtx);` |
|    18 |  606 | `					}` |
|    10 |  607 | `				}` |
|    10 |  608 | `			}` |
|    56 |  609 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  610 | `				PCRE2_SIZE *ovector;` |
|    83 |  611 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    27 |  612 | `					startOffset, 0, pMatchData, NULL);` |
|    56 |  613 | `				if( rc < 0 ){` |
|    22 |  614 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|    22 |  615 | `					break;` |
|     - |  616 | `				}` |
|    36 |  617 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    36 |  618 | `				if( apGroupArrays ){` |
|    36 |  619 | `					ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    36 |  620 | `					int nActual = rc;` |
|    94 |  621 | `					for( g = 0; g < nGroups; g++ ){` |
|    88 |  622 | `						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){` |
|    58 |  623 | `							PCRE2_SIZE s = ovector[2*g];` |
|    58 |  624 | `							PCRE2_SIZE e = ovector[2*g+1];` |
|    58 |  625 | `							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|     3 |  626 | `								ph7_value *pSub = ph7_context_new_array(pCtx);` |
|     3 |  627 | `								ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|     3 |  628 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|     3 |  629 | `								ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|     3 |  630 | `								ph7_value_int(pOff, (int)s);` |
|     3 |  631 | `								ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|     3 |  632 | `								ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|     3 |  633 | `								ph7_context_release_value(pCtx, pSub);` |
|     3 |  634 | `								ph7_context_release_value(pCtx, pOff);` |
|     2 |  635 | `							}else{` |
|    56 |  636 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|    56 |  637 | `								ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  638 | `							}` |
|    30 |  639 | `						}else{` |
|     3 |  640 | `							ph7_value_string(pVal, "", 0);` |
|     3 |  641 | `							ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  642 | `						}` |
|    60 |  643 | `						ph7_value_reset_string_cursor(pVal);` |
|    31 |  644 | `					}` |
|    36 |  645 | `					ph7_context_release_value(pCtx, pVal);` |
|    17 |  646 | `				}` |
|    36 |  647 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  648 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  649 | `				}else{` |
|    36 |  650 | `					startOffset = ovector[1];` |
|     - |  651 | `				}` |
|    36 |  652 | `				totalMatches++;` |
|     2 |  653 | `			}` |
|    22 |  654 | `			if( apGroupArrays ){` |
|     - |  655 | `				/* Attach the per-group match arrays. php's PREG_PATTERN_ORDER stores a` |
|     - |  656 | `				 * named group under BOTH its name and its number, interleaved` |
|     - |  657 | ``				 * (`0, name, 1, value, 2`) — the same value under each key. Read the`` |
|     - |  658 | `				 * name table so each numbered group can emit its named alias first. */` |
|    22 |  659 | `				uint32_t namecount = 0, nameentrysize = 0;` |
|    22 |  660 | `				PCRE2_SPTR nametable = 0;` |
|    22 |  661 | `				pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|    22 |  662 | `				if( namecount > 0 ){` |
|     3 |  663 | `					pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     3 |  664 | `					pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|     1 |  665 | `				}` |
|    54 |  666 | `				for( g = 0; g < nGroups; g++ ){` |
|    34 |  667 | `					const char *zName = 0;` |
|    34 |  668 | `					if( namecount > 0 ){` |
|     - |  669 | `						uint32_t k;` |
|    13 |  670 | `						for( k = 0; k < namecount; k++ ){` |
|    11 |  671 | `							PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|    11 |  672 | `							if( (uint32_t)(((entry[0] << 8) \| entry[1])) == g ){` |
|     5 |  673 | `								zName = (const char *)(entry + 2);` |
|     5 |  674 | `								break;` |
|     - |  675 | `							}` |
|     4 |  676 | `						}` |
|     3 |  677 | `					}` |
|    34 |  678 | `					if( zName ){` |
|     5 |  679 | `						ph7_array_add_strkey_elem(pOutArray, zName, apGroupArrays[g]);` |
|     2 |  680 | `					}` |
|    34 |  681 | `					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);` |
|    34 |  682 | `					ph7_context_release_value(pCtx, apGroupArrays[g]);` |
|    18 |  683 | `				}` |
|    22 |  684 | `				ph7_context_free_chunk(pCtx, apGroupArrays);` |
|    10 |  685 | `			}` |
|     - |  686 | `		}` |
|     - |  687 | `		/* Write output array to caller's variable */` |
|    30 |  688 | `		if( pOutArray && nArg >= 3 ){` |
|    30 |  689 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pOutArray);` |
|    30 |  690 | `			ph7_context_release_value(pCtx, pOutArray);` |
|    14 |  691 | `		}` |
|     - |  692 | `	}` |
|    30 |  693 | `	pcre2_match_data_free(pMatchData);` |
|    30 |  694 | `	ph7_result_int(pCtx, totalMatches);` |
|    30 |  695 | `	return PH7_OK;` |
|    20 |  696 | `}` |
|     - |  697 |  |
|     - |  698 | `/* ======================================================================` |
|     - |  699 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|     - |  700 | ` * ====================================================================== */` |
|    12 |  701 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |  702 | `{` |
|     - |  703 | `	const char *zPattern, *zSubject;` |
|     - |  704 | `	int nPatLen, nSubLen;` |
|     - |  705 | `	pcre2_code *pCode;` |
|     - |  706 | `	pcre2_match_data *pMatchData;` |
|     - |  707 | `	sxu32 nCapture;` |
|     - |  708 | `	ph7_value *pArray;` |
|     - |  709 | `	ph7_value *pVal;` |
|    14 |  710 | `	PCRE2_SIZE startOffset = 0, lastOffset = 0;` |
|    14 |  711 | `	int limit = -1;` |
|    14 |  712 | `	int iFlags = 0;` |
|    14 |  713 | `	int nPieces = 0;` |
|     - |  714 | `	int rc;` |
|     - |  715 |  |
|    14 |  716 | `	if( nArg < 2 ){` |
|   ! 0 |  717 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  718 | `			"preg_split() expects at least 2 parameters");` |
|   ! 0 |  719 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  720 | `		return PH7_OK;` |
|     - |  721 | `	}` |
|    14 |  722 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    14 |  723 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    14 |  724 | `	if( nArg >= 3 ){` |
|     9 |  725 | `		limit = ph7_value_to_int(apArg[2]);` |
|     4 |  726 | `	}` |
|    14 |  727 | `	if( nArg >= 4 ){` |
|     7 |  728 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     3 |  729 | `	}` |
|    14 |  730 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    14 |  731 | `	if( pCode == 0 ){` |
|     3 |  732 | `		ph7_result_bool(pCtx, 0);` |
|     3 |  733 | `		return PH7_OK;` |
|     - |  734 | `	}` |
|    11 |  735 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    11 |  736 | `	if( pMatchData == 0 ){` |
|   ! 0 |  737 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  738 | `		return PH7_OK;` |
|     - |  739 | `	}` |
|    11 |  740 | `	pArray = ph7_context_new_array(pCtx);` |
|    11 |  741 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    11 |  742 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  743 |  |
|    25 |  744 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|    25 |  745 | `		if( limit > 0 && nPieces >= limit - 1 ){` |
|     3 |  746 | `			break; /* Last piece gets the remainder */` |
|     - |  747 | `		}` |
|    34 |  748 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    11 |  749 | `			startOffset, 0, pMatchData, NULL);` |
|    23 |  750 | `		if( rc < 0 ){` |
|     9 |  751 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  752 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  753 | `			}` |
|     9 |  754 | `			break;` |
|     - |  755 | `		}` |
|     - |  756 | `		{` |
|    15 |  757 | `			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    15 |  758 | `			PCRE2_SIZE matchStart = ovector[0];` |
|    15 |  759 | `			PCRE2_SIZE matchEnd = ovector[1];` |
|    15 |  760 | `			int pieceLen = (int)(matchStart - lastOffset);` |
|     - |  761 |  |
|     - |  762 | `			/* Add the piece before the match */` |
|    15 |  763 | `			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| pieceLen > 0 ){` |
|    15 |  764 | `				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  765 | `					ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  766 | `					ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  767 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|   ! 0 |  768 | `					ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  769 | `					ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  770 | `					ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  771 | `					ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  772 | `					ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  773 | `					ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  774 | `				}else{` |
|    15 |  775 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|    15 |  776 | `					ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  777 | `				}` |
|    15 |  778 | `				ph7_value_reset_string_cursor(pVal);` |
|    15 |  779 | `				nPieces++;` |
|     7 |  780 | `			}` |
|     - |  781 | `			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */` |
|    15 |  782 | `			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){` |
|     - |  783 | `				int g;` |
|   ! 0 |  784 | `				for( g = 1; g < rc; g++ ){` |
|   ! 0 |  785 | `					PCRE2_SIZE gs = ovector[2*g];` |
|   ! 0 |  786 | `					PCRE2_SIZE ge = ovector[2*g+1];` |
|     - |  787 | `					int gLen;` |
|   ! 0 |  788 | `					if( gs == PCRE2_UNSET ) continue;` |
|   ! 0 |  789 | `					gLen = (int)(ge - gs);` |
|   ! 0 |  790 | `					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| gLen > 0 ){` |
|   ! 0 |  791 | `						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  792 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  793 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  794 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  795 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  796 | `							ph7_value_int(pOff, (int)gs);` |
|   ! 0 |  797 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  798 | `							ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  799 | `							ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  800 | `							ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  801 | `						}else{` |
|   ! 0 |  802 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  803 | `							ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  804 | `						}` |
|   ! 0 |  805 | `						ph7_value_reset_string_cursor(pVal);` |
|   ! 0 |  806 | `					}` |
|   ! 0 |  807 | `				}` |
|   ! 0 |  808 | `			}` |
|     - |  809 | `			/* Advance */` |
|    15 |  810 | `			lastOffset = matchEnd;` |
|    15 |  811 | `			if( matchEnd == matchStart ){` |
|   ! 0 |  812 | `				startOffset = matchEnd + 1;` |
|   ! 0 |  813 | `			}else{` |
|    15 |  814 | `				startOffset = matchEnd;` |
|     - |  815 | `			}` |
|     - |  816 | `		}` |
|     1 |  817 | `	}` |
|     - |  818 | `	/* Add trailing piece */` |
|     - |  819 | `	{` |
|    11 |  820 | `		int trailLen = nSubLen - (int)lastOffset;` |
|    11 |  821 | `		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| trailLen > 0 ){` |
|    11 |  822 | `			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  823 | `				ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  824 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  825 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|   ! 0 |  826 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  827 | `				ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  828 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  829 | `				ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  830 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  831 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  832 | `			}else{` |
|    11 |  833 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|    11 |  834 | `				ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  835 | `			}` |
|     5 |  836 | `		}` |
|     - |  837 | `	}` |
|    11 |  838 | `	ph7_context_release_value(pCtx, pVal);` |
|    11 |  839 | `	pcre2_match_data_free(pMatchData);` |
|    11 |  840 | `	ph7_result_value(pCtx, pArray);` |
|    11 |  841 | `	ph7_context_release_value(pCtx, pArray);` |
|    11 |  842 | `	return PH7_OK;` |
|     8 |  843 | `}` |
|     - |  844 |  |
|     - |  845 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|   250 |  846 | `static void PcreExpandBackrefs(` |
|     - |  847 | `	SyBlob *pOut,` |
|     - |  848 | `	const char *zRepl, int nReplLen,` |
|     - |  849 | `	const char *zSubject,` |
|     - |  850 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     2 |  851 | `{` |
|   252 |  852 | `	const char *zEnd = &zRepl[nReplLen];` |
|   252 |  853 | `	const char *z = zRepl;` |
|     - |  854 |  |
|   820 |  855 | `	while( z < zEnd ){` |
|   570 |  856 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   ! 0 |  857 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|   ! 0 |  858 | `				int g = z[1] - '0';` |
|   ! 0 |  859 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  860 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  861 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  862 | `				}` |
|   ! 0 |  863 | `				z += 2;` |
|   ! 0 |  864 | `				continue;` |
|     - |  865 | `			}` |
|   ! 0 |  866 | `			if( z[1] == '\\' ){` |
|   ! 0 |  867 | `				SyBlobAppend(pOut, "\\", 1);` |
|   ! 0 |  868 | `				z += 2;` |
|   ! 0 |  869 | `				continue;` |
|     - |  870 | `			}` |
|     - |  871 | `			/* Not a backreference — emit literally */` |
|   ! 0 |  872 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  873 | `			z++;` |
|   ! 0 |  874 | `			continue;` |
|     - |  875 | `		}` |
|   570 |  876 | `		if( *z == '$' && z + 1 < zEnd ){` |
|    89 |  877 | `			if( z[1] == '$' ){` |
|   ! 0 |  878 | `				SyBlobAppend(pOut, "$", 1);` |
|   ! 0 |  879 | `				z += 2;` |
|   ! 0 |  880 | `				continue;` |
|     - |  881 | `			}` |
|    89 |  882 | `			if( z[1] == '{' ){` |
|     - |  883 | `				/* ${N} form */` |
|    39 |  884 | `				const char *p = z + 2;` |
|    39 |  885 | `				int g = 0;` |
|    77 |  886 | `				while( p < zEnd && *p >= '0' && *p <= '9' ){` |
|    39 |  887 | `					g = g * 10 + (*p - '0');` |
|    39 |  888 | `					p++;` |
|     1 |  889 | `				}` |
|    39 |  890 | `				if( p < zEnd && *p == '}' ){` |
|    39 |  891 | `					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|    58 |  892 | `						SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|    38 |  893 | `							(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|    19 |  894 | `					}` |
|    39 |  895 | `					z = p + 1;` |
|    39 |  896 | `					continue;` |
|     - |  897 | `				}` |
|     - |  898 | `				/* Not a valid ${N} — emit literally */` |
|   ! 0 |  899 | `				SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  900 | `				z++;` |
|   ! 0 |  901 | `				continue;` |
|     - |  902 | `			}` |
|    51 |  903 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|     - |  904 | `				/* $N or $NN */` |
|    51 |  905 | `				int g = z[1] - '0';` |
|    51 |  906 | `				z += 2;` |
|     - |  907 | `				/* Check for second digit */` |
|    51 |  908 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  909 | `					int g2 = g * 10 + (*z - '0');` |
|   ! 0 |  910 | `					if( g2 < nGroups ){` |
|   ! 0 |  911 | `						g = g2;` |
|   ! 0 |  912 | `						z++;` |
|   ! 0 |  913 | `					}` |
|   ! 0 |  914 | `				}` |
|    51 |  915 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|    76 |  916 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|    50 |  917 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|    25 |  918 | `				}` |
|    51 |  919 | `				continue;` |
|     - |  920 | `			}` |
|     - |  921 | `			/* Not a backreference */` |
|   ! 0 |  922 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  923 | `			z++;` |
|   ! 0 |  924 | `			continue;` |
|     - |  925 | `		}` |
|   482 |  926 | `		SyBlobAppend(pOut, z, 1);` |
|   482 |  927 | `		z++;` |
|     2 |  928 | `	}` |
|   252 |  929 | `}` |
|     - |  930 |  |
|     - |  931 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|   344 |  932 | `static void PcreDoReplace(` |
|     - |  933 | `	ph7_context *pCtx,` |
|     - |  934 | `	pcre2_code *pCode,` |
|     - |  935 | `	const char *zSubject, int nSubLen,` |
|     - |  936 | `	const char *zRepl, int nReplLen,` |
|     - |  937 | `	int limit,` |
|     - |  938 | `	int *pCount,` |
|     - |  939 | `	SyBlob *pOut)` |
|     2 |  940 | `{` |
|     - |  941 | `	pcre2_match_data *pMatchData;` |
|   346 |  942 | `	PCRE2_SIZE startOffset = 0;` |
|   346 |  943 | `	int nReplacements = 0;` |
|     - |  944 | `	int rc;` |
|     - |  945 |  |
|   346 |  946 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|   346 |  947 | `	if( pMatchData == 0 ) return;` |
|     - |  948 |  |
|   596 |  949 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  950 | `		PCRE2_SIZE *ovector;` |
|   594 |  951 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|   890 |  952 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|   296 |  953 | `			startOffset, 0, pMatchData, NULL);` |
|   594 |  954 | `		if( rc < 0 ){` |
|   344 |  955 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  956 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  957 | `			}` |
|   344 |  958 | `			break;` |
|     - |  959 | `		}` |
|   252 |  960 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - |  961 | `		/* Copy text before match */` |
|   252 |  962 | `		if( ovector[0] > startOffset ){` |
|   191 |  963 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|    95 |  964 | `		}` |
|     - |  965 | `		/* Expand replacement */` |
|   252 |  966 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|   252 |  967 | `		nReplacements++;` |
|     - |  968 | `		/* Advance */` |
|   252 |  969 | `		if( ovector[1] == ovector[0] ){` |
|     - |  970 | `			/* Zero-width match: to make progress, emit the character AT THE MATCH` |
|     - |  971 | `			 * POSITION (ovector[0]) and step past it. The match can sit AHEAD of the` |
|     - |  972 | `			 * search start (a lookbehind/lookahead assertion, e.g. the camelCase` |
|     - |  973 | `			 * split /(?<=[[:lower:]])(?=[[:upper:]])/), so copying zSubject[startOffset]` |
|     - |  974 | `			 * grabbed the wrong byte ("fooBar" -> "foo far"). The text between` |
|     - |  975 | `			 * startOffset and ovector[0] was already copied above. */` |
|    23 |  976 | `			if( ovector[0] < (PCRE2_SIZE)nSubLen ){` |
|    21 |  977 | `				SyBlobAppend(pOut, &zSubject[ovector[0]], 1);` |
|    10 |  978 | `			}` |
|    23 |  979 | `			startOffset = ovector[0] + 1;` |
|    12 |  980 | `		}else{` |
|   230 |  981 | `			startOffset = ovector[1];` |
|     - |  982 | `		}` |
|     2 |  983 | `	}` |
|     - |  984 | `	/* Copy remainder */` |
|   346 |  985 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   268 |  986 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   133 |  987 | `	}` |
|   346 |  988 | `	if( pCount ){` |
|   346 |  989 | `		*pCount += nReplacements;` |
|   172 |  990 | `	}` |
|   346 |  991 | `	pcre2_match_data_free(pMatchData);` |
|   172 |  992 | `	SXUNUSED(pCtx);` |
|   174 |  993 | `}` |
|     - |  994 |  |
|     - |  995 | `/* ===== Helper: apply pattern(s)+replacement(s) to ONE subject string =====` |
|     - |  996 | ` * pPattern is a string or an array of patterns; pRepl is a string (used for` |
|     - |  997 | ` * every pattern) or, only when pPattern is an array, an array taken by ORDER` |
|     - |  998 | ` * (missing element -> ""). Array patterns are applied sequentially, each to the` |
|     - |  999 | ` * result of the previous (PHP semantics), ping-ponging two blobs. The final` |
|     - | 1000 | ` * text is appended to pOut. Returns SXRET_OK, or SXERR_ABORT on a bad pattern` |
|     - | 1001 | ` * (the caller then yields NULL, matching the scalar path). */` |
|   340 | 1002 | `static sxi32 PcreReplaceSubject(` |
|     - | 1003 | `	ph7_context *pCtx,` |
|     - | 1004 | `	ph7_value *pPattern,` |
|     - | 1005 | `	ph7_value *pRepl,` |
|     - | 1006 | `	const char *zSubject, int nSubLen,` |
|     - | 1007 | `	int limit,` |
|     - | 1008 | `	int *pCount,` |
|     - | 1009 | `	SyBlob *pOut)` |
|     3 | 1010 | `{` |
|     - | 1011 | `	sxu32 nCapture;` |
|   343 | 1012 | `	if( !ph7_value_is_array(pPattern) ){` |
|     - | 1013 | `		/* Single pattern + single replacement */` |
|     - | 1014 | `		const char *zPattern, *zRepl;` |
|     - | 1015 | `		int nPatLen, nReplLen;` |
|     - | 1016 | `		pcre2_code *pCode;` |
|   331 | 1017 | `		zPattern = ph7_value_to_string(pPattern, &nPatLen);` |
|   331 | 1018 | `		zRepl = ph7_value_to_string(pRepl, &nReplLen);` |
|   331 | 1019 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|   331 | 1020 | `		if( pCode == 0 ){` |
|     6 | 1021 | `			return SXERR_ABORT;` |
|     - | 1022 | `		}` |
|   326 | 1023 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, pCount, pOut);` |
|   326 | 1024 | `		return SXRET_OK;` |
|   ! 0 | 1025 | `	}else{` |
|     - | 1026 | `		/* Array of patterns: apply each in insertion order to the accumulating` |
|     - | 1027 | `		 * subject. Replacement is the parallel array element (by order) or the` |
|     - | 1028 | `		 * scalar replacement for every pattern. */` |
|    13 | 1029 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|    13 | 1030 | `		ph7_hashmap *pRepMap = ph7_value_is_array(pRepl) ? (ph7_hashmap *)pRepl->x.pOther : 0;` |
|    13 | 1031 | `		const char *zScalarRepl = 0;` |
|    13 | 1032 | `		int nScalarRepl = 0;` |
|     - | 1033 | `		ph7_hashmap_node *pPatNode, *pRepNode;` |
|     - | 1034 | `		ph7_value sPat, sRep;` |
|     - | 1035 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|     - | 1036 | `		sxu32 n;` |
|    13 | 1037 | `		sxi32 rc = SXRET_OK;` |
|    13 | 1038 | `		if( pRepMap == 0 ){` |
|     5 | 1039 | `			zScalarRepl = ph7_value_to_string(pRepl, &nScalarRepl);` |
|     2 | 1040 | `		}` |
|    13 | 1041 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|    13 | 1042 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|    13 | 1043 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|    13 | 1044 | `		pSrc = &sA; pDst = &sB;` |
|    13 | 1045 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|    13 | 1046 | `		PH7_MemObjInit(pCtx->pVm, &sRep);` |
|    13 | 1047 | `		pPatNode = pPatMap->pFirst;` |
|    13 | 1048 | `		pRepNode = pRepMap ? pRepMap->pFirst : 0;` |
|    13 | 1049 | `		n = pPatMap->nEntry;` |
|    33 | 1050 | `		while( n > 0 ){` |
|     - | 1051 | `			const char *zPattern, *zRepl;` |
|     - | 1052 | `			int nPatLen, nReplLen;` |
|     - | 1053 | `			pcre2_code *pCode;` |
|     - | 1054 | `			SyBlob *pSwap;` |
|    21 | 1055 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|    21 | 1056 | `			zPattern = ph7_value_to_string(&sPat, &nPatLen);` |
|    21 | 1057 | `			if( pRepMap ){` |
|    17 | 1058 | `				if( pRepNode ){` |
|    15 | 1059 | `					PH7_HashmapExtractNodeValue(pRepNode, &sRep, FALSE);` |
|    15 | 1060 | `					zRepl = ph7_value_to_string(&sRep, &nReplLen);` |
|     8 | 1061 | `				}else{` |
|     3 | 1062 | `					zRepl = ""; nReplLen = 0;` |
|     - | 1063 | `				}` |
|     9 | 1064 | `			}else{` |
|     5 | 1065 | `				zRepl = zScalarRepl; nReplLen = nScalarRepl;` |
|     - | 1066 | `			}` |
|    21 | 1067 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    21 | 1068 | `			if( pCode == 0 ){` |
|   ! 0 | 1069 | `				rc = SXERR_ABORT;` |
|   ! 0 | 1070 | `				PH7_MemObjRelease(&sPat);` |
|   ! 0 | 1071 | `				if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|   ! 0 | 1072 | `				break;` |
|     - | 1073 | `			}` |
|    21 | 1074 | `			SyBlobReset(pDst);` |
|    31 | 1075 | `			PcreDoReplace(pCtx, pCode,` |
|    20 | 1076 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|    10 | 1077 | `				zRepl, nReplLen, limit, pCount, pDst);` |
|     - | 1078 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|    21 | 1079 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|    21 | 1080 | `			PH7_MemObjRelease(&sPat);` |
|    21 | 1081 | `			if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|    21 | 1082 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|    21 | 1083 | `			if( pRepNode ){ pRepNode = pRepNode->pPrev; }` |
|    21 | 1084 | `			n--;` |
|     1 | 1085 | `		}` |
|    13 | 1086 | `		if( rc == SXRET_OK ){` |
|    13 | 1087 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|     6 | 1088 | `		}` |
|    13 | 1089 | `		SyBlobRelease(&sA);` |
|    13 | 1090 | `		SyBlobRelease(&sB);` |
|    13 | 1091 | `		return rc;` |
|     - | 1092 | `	}` |
|   173 | 1093 | `}` |
|     - | 1094 |  |
|     - | 1095 | `/* ======================================================================` |
|     - | 1096 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - | 1097 | ` * ====================================================================== */` |
|   324 | 1098 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1099 | `{` |
|   327 | 1100 | `	int limit = -1;` |
|   327 | 1101 | `	int count = 0;` |
|     - | 1102 |  |
|   327 | 1103 | `	if( nArg < 3 ){` |
|   ! 0 | 1104 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1105 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 | 1106 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1107 | `		return PH7_OK;` |
|     - | 1108 | `	}` |
|   327 | 1109 | `	if( nArg >= 4 ){` |
|    42 | 1110 | `		limit = ph7_value_to_int(apArg[3]);` |
|    20 | 1111 | `	}` |
|   327 | 1112 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1113 |  |
|     - | 1114 | `	/* A scalar pattern with an array replacement is a parameter mismatch (PHP` |
|     - | 1115 | `	 * throws a TypeError; PHL keeps preg_replace's warning-based arg-error style). */` |
|   327 | 1116 | `	if( !ph7_value_is_array(apArg[0]) && ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 1117 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1118 | `			"Parameter mismatch, pattern is a string while replacement is an array");` |
|   ! 0 | 1119 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1120 | `		return PH7_OK;` |
|     - | 1121 | `	}` |
|   487 | 1122 | `	if( ph7_value_is_array(apArg[2]) ){` |
|     - | 1123 | `		/* Array subject: return an array, each element replaced, keys preserved. */` |
|    15 | 1124 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|    15 | 1125 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|    15 | 1126 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     - | 1127 | `		ph7_value sKey, sVal;` |
|     - | 1128 | `		ph7_hashmap_node *pNode;` |
|     - | 1129 | `		sxu32 n;` |
|    15 | 1130 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|   ! 0 | 1131 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1132 | `			return PH7_OK;` |
|     - | 1133 | `		}` |
|    15 | 1134 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|    15 | 1135 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    15 | 1136 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|    15 | 1137 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|    45 | 1138 | `		while( n > 0 ){` |
|     - | 1139 | `			const char *zSubject;` |
|     - | 1140 | `			int nSubLen;` |
|     - | 1141 | `			SyBlob sOut;` |
|    31 | 1142 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|    31 | 1143 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|    31 | 1144 | `			zSubject = ph7_value_to_string(&sVal, &nSubLen);` |
|    31 | 1145 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    31 | 1146 | `			if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1147 | `				/* A bad pattern with an array subject yields an empty array (PHP);` |
|     - | 1148 | `				 * the failure hits the first element, so pResult is still empty. */` |
|   ! 0 | 1149 | `				SyBlobRelease(&sOut);` |
|   ! 0 | 1150 | `				PH7_MemObjRelease(&sKey);` |
|   ! 0 | 1151 | `				PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1152 | `				ph7_result_value(pCtx, pResult);` |
|   ! 0 | 1153 | `				goto set_count;` |
|     - | 1154 | `			}` |
|    31 | 1155 | `			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    31 | 1156 | `			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|    31 | 1157 | `			ph7_value_reset_string_cursor(pElem);` |
|    31 | 1158 | `			SyBlobRelease(&sOut);` |
|    31 | 1159 | `			PH7_MemObjRelease(&sKey);` |
|    31 | 1160 | `			PH7_MemObjRelease(&sVal);` |
|    31 | 1161 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|    31 | 1162 | `			n--;` |
|     1 | 1163 | `		}` |
|    15 | 1164 | `		ph7_result_value(pCtx, pResult);` |
|     8 | 1165 | `	}else{` |
|     - | 1166 | `		/* Scalar subject: one replaced string. */` |
|     - | 1167 | `		const char *zSubject;` |
|     - | 1168 | `		int nSubLen;` |
|     - | 1169 | `		SyBlob sOut;` |
|   313 | 1170 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|   313 | 1171 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|   313 | 1172 | `		if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1173 | `			/* Scalar subject: a bad pattern returns NULL (PHP). */` |
|     6 | 1174 | `			SyBlobRelease(&sOut);` |
|     6 | 1175 | `			ph7_result_null(pCtx);` |
|     6 | 1176 | `			goto set_count;` |
|     - | 1177 | `		}` |
|   308 | 1178 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|   308 | 1179 | `		SyBlobRelease(&sOut);` |
|     - | 1180 | `	}` |
|   162 | 1181 | `set_count:` |
|     - | 1182 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|     - | 1183 | `	 * (PHP always writes it: 0, or the count accumulated by earlier good patterns). */` |
|   327 | 1184 | `	if( nArg >= 5 ){` |
|     - | 1185 | `		ph7_value sCount;` |
|    42 | 1186 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|    42 | 1187 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|    42 | 1188 | `		PH7_MemObjRelease(&sCount);` |
|    20 | 1189 | `	}` |
|   327 | 1190 | `	return PH7_OK;` |
|   165 | 1191 | `}` |
|     - | 1192 |  |
|     - | 1193 | `/* ===== Helper: run the callback over ONE compiled pattern on ONE subject =====` |
|     - | 1194 | ` * The mirror of PcreDoReplace() for preg_replace_callback: the replacement text` |
|     - | 1195 | ` * comes from a user callback fed the match array (shaped by $flags) instead of` |
|     - | 1196 | ` * from a template. Appends the whole replaced subject to pOut and adds its own` |
|     - | 1197 | ` * replacement count to *pCount. Returns SXRET_OK, or PH7_EXCEPTION when the` |
|     - | 1198 | ` * callback threw — the caller then unwinds without producing a result. */` |
|    64 | 1199 | `static sxi32 PcreDoCallbackReplace(` |
|     - | 1200 | `	ph7_context *pCtx,` |
|     - | 1201 | `	pcre2_code *pCode,` |
|     - | 1202 | `	const char *zSubject, int nSubLen,` |
|     - | 1203 | `	ph7_value *pCallback,` |
|     - | 1204 | `	int limit,` |
|     - | 1205 | `	int iFlags,` |
|     - | 1206 | `	int *pCount,` |
|     - | 1207 | `	SyBlob *pOut)` |
|     3 | 1208 | `{` |
|     - | 1209 | `	pcre2_match_data *pMatchData;` |
|    67 | 1210 | `	PCRE2_SIZE startOffset = 0;` |
|    67 | 1211 | `	int nReplacements = 0;` |
|     - | 1212 | `	int rc;` |
|     - | 1213 |  |
|    67 | 1214 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    67 | 1215 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1216 | `		return SXRET_OK;` |
|     - | 1217 | `	}` |
|   145 | 1218 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1219 | `		PCRE2_SIZE *ovector;` |
|     - | 1220 | `		ph7_value *pMatchArr;` |
|     - | 1221 | `		ph7_value *apCbArg[1];` |
|     - | 1222 | `		ph7_value sResult;` |
|     - | 1223 | `		const char *zReplacement;` |
|     - | 1224 | `		int nReplLen;` |
|     - | 1225 |  |
|   174 | 1226 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|   210 | 1227 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    69 | 1228 | `			startOffset, 0, pMatchData, NULL);` |
|   141 | 1229 | `		if( rc < 0 ){` |
|    61 | 1230 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1231 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1232 | `			}` |
|    61 | 1233 | `			break;` |
|     - | 1234 | `		}` |
|    83 | 1235 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1236 | `		/* Copy text before match */` |
|    83 | 1237 | `		if( ovector[0] > startOffset ){` |
|    29 | 1238 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|    13 | 1239 | `		}` |
|     - | 1240 | `		/* Build matches array for callback */` |
|    83 | 1241 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    83 | 1242 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, iFlags);` |
|     - | 1243 | `		/* Call the callback */` |
|    83 | 1244 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    83 | 1245 | `		apCbArg[0] = pMatchArr;` |
|    83 | 1246 | `		if( PH7_VmCallUserFunction(pCtx->pVm, pCallback, 1, apCbArg, &sResult) == PH7_EXCEPTION ){` |
|     - | 1247 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|     3 | 1248 | `			PH7_MemObjRelease(&sResult);` |
|     3 | 1249 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|     3 | 1250 | `			pcre2_match_data_free(pMatchData);` |
|     3 | 1251 | `			*pCount += nReplacements;` |
|     3 | 1252 | `			return PH7_EXCEPTION;` |
|     - | 1253 | `		}` |
|     - | 1254 | `		/* Get replacement string from callback result */` |
|    81 | 1255 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    81 | 1256 | `		SyBlobAppend(pOut, zReplacement, (sxu32)nReplLen);` |
|    81 | 1257 | `		PH7_MemObjRelease(&sResult);` |
|    81 | 1258 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    81 | 1259 | `		nReplacements++;` |
|     - | 1260 | `		/* Advance */` |
|    81 | 1261 | `		if( ovector[1] == ovector[0] ){` |
|     - | 1262 | `			/* Zero-width match: emit the character AT THE MATCH POSITION and step` |
|     - | 1263 | `			 * past it. The match can sit AHEAD of the search start (a lookaround` |
|     - | 1264 | `			 * assertion, e.g. the camelCase split), so copying zSubject[startOffset]` |
|     - | 1265 | `			 * grabbed the wrong byte ("fooBar" -> "foo far") — the same fix` |
|     - | 1266 | `			 * PcreDoReplace() carries. */` |
|     5 | 1267 | `			if( ovector[0] < (PCRE2_SIZE)nSubLen ){` |
|     5 | 1268 | `				SyBlobAppend(pOut, &zSubject[ovector[0]], 1);` |
|     2 | 1269 | `			}` |
|     5 | 1270 | `			startOffset = ovector[0] + 1;` |
|     3 | 1271 | `		}else{` |
|    77 | 1272 | `			startOffset = ovector[1];` |
|     - | 1273 | `		}` |
|     3 | 1274 | `	}` |
|     - | 1275 | `	/* Copy remainder */` |
|    65 | 1276 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|    32 | 1277 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|    15 | 1278 | `	}` |
|    65 | 1279 | `	*pCount += nReplacements;` |
|    65 | 1280 | `	pcre2_match_data_free(pMatchData);` |
|    65 | 1281 | `	return SXRET_OK;` |
|    35 | 1282 | `}` |
|     - | 1283 |  |
|     - | 1284 | `/* ===== Helper: apply pattern(s)+callback to ONE subject string =====` |
|     - | 1285 | ` * The callback twin of PcreReplaceSubject(): pPattern is a string or an ARRAY of` |
|     - | 1286 | ` * patterns applied sequentially, each to the result of the previous (php` |
|     - | 1287 | ` * semantics), ping-ponging two blobs. Returns SXRET_OK, SXERR_ABORT on a bad` |
|     - | 1288 | ` * pattern (the caller then yields NULL / an empty array like the template path),` |
|     - | 1289 | ` * or PH7_EXCEPTION when the callback threw. */` |
|    64 | 1290 | `static sxi32 PcreCallbackReplaceSubject(` |
|     - | 1291 | `	ph7_context *pCtx,` |
|     - | 1292 | `	ph7_value *pPattern,` |
|     - | 1293 | `	ph7_value *pCallback,` |
|     - | 1294 | `	const char *zSubject, int nSubLen,` |
|     - | 1295 | `	int limit,` |
|     - | 1296 | `	int iFlags,` |
|     - | 1297 | `	int *pCount,` |
|     - | 1298 | `	SyBlob *pOut)` |
|     3 | 1299 | `{` |
|     - | 1300 | `	sxu32 nCapture;` |
|    67 | 1301 | `	if( !ph7_value_is_array(pPattern) ){` |
|     - | 1302 | `		const char *zPattern;` |
|     - | 1303 | `		int nPatLen;` |
|     - | 1304 | `		pcre2_code *pCode;` |
|    59 | 1305 | `		zPattern = ph7_value_to_string(pPattern, &nPatLen);` |
|    59 | 1306 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    59 | 1307 | `		if( pCode == 0 ){` |
|     7 | 1308 | `			return SXERR_ABORT;` |
|     - | 1309 | `		}` |
|    78 | 1310 | `		return PcreDoCallbackReplace(pCtx, pCode, zSubject, nSubLen, pCallback,` |
|    25 | 1311 | `			limit, iFlags, pCount, pOut);` |
|   ! 0 | 1312 | `	}else{` |
|     9 | 1313 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|     - | 1314 | `		ph7_hashmap_node *pPatNode;` |
|     - | 1315 | `		ph7_value sPat;` |
|     - | 1316 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|     - | 1317 | `		sxu32 n;` |
|     9 | 1318 | `		sxi32 rc = SXRET_OK;` |
|     9 | 1319 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|     9 | 1320 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|     9 | 1321 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|     9 | 1322 | `		pSrc = &sA; pDst = &sB;` |
|     9 | 1323 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|     9 | 1324 | `		pPatNode = pPatMap ? pPatMap->pFirst : 0;` |
|     9 | 1325 | `		n = pPatMap ? pPatMap->nEntry : 0;` |
|    23 | 1326 | `		while( n > 0 ){` |
|     - | 1327 | `			const char *zPattern;` |
|     - | 1328 | `			int nPatLen;` |
|     - | 1329 | `			pcre2_code *pCode;` |
|     - | 1330 | `			SyBlob *pSwap;` |
|    17 | 1331 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|    17 | 1332 | `			zPattern = ph7_value_to_string(&sPat, &nPatLen);` |
|    17 | 1333 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    17 | 1334 | `			if( pCode == 0 ){` |
|     3 | 1335 | `				rc = SXERR_ABORT;` |
|     3 | 1336 | `				PH7_MemObjRelease(&sPat);` |
|     3 | 1337 | `				break;` |
|     - | 1338 | `			}` |
|    15 | 1339 | `			SyBlobReset(pDst);` |
|    22 | 1340 | `			rc = PcreDoCallbackReplace(pCtx, pCode,` |
|    14 | 1341 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|     7 | 1342 | `				pCallback, limit, iFlags, pCount, pDst);` |
|     - | 1343 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|    15 | 1344 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|    15 | 1345 | `			PH7_MemObjRelease(&sPat);` |
|    15 | 1346 | `			if( rc == PH7_EXCEPTION ){` |
|   ! 0 | 1347 | `				break;` |
|     - | 1348 | `			}` |
|    15 | 1349 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|    15 | 1350 | `			n--;` |
|     1 | 1351 | `		}` |
|     9 | 1352 | `		if( rc == SXRET_OK ){` |
|     7 | 1353 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|     3 | 1354 | `		}` |
|     9 | 1355 | `		SyBlobRelease(&sA);` |
|     9 | 1356 | `		SyBlobRelease(&sB);` |
|     9 | 1357 | `		return rc;` |
|     - | 1358 | `	}` |
|    35 | 1359 | `}` |
|     - | 1360 |  |
|     - | 1361 | `/* ======================================================================` |
|     - | 1362 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count [, flags]]])` |
|     - | 1363 | ` * ====================================================================== */` |
|    52 | 1364 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1365 | `{` |
|    55 | 1366 | `	int limit = -1;` |
|    55 | 1367 | `	int iFlags = 0;` |
|    55 | 1368 | `	int count = 0;` |
|     - | 1369 | `	sxi32 rc;` |
|     - | 1370 |  |
|    55 | 1371 | `	if( nArg < 3 ){` |
|   ! 0 | 1372 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1373 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 | 1374 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1375 | `		return PH7_OK;` |
|     - | 1376 | `	}` |
|    55 | 1377 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 | 1378 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1379 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 | 1380 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1381 | `		return PH7_OK;` |
|     - | 1382 | `	}` |
|    55 | 1383 | `	if( nArg >= 4 ){` |
|    38 | 1384 | `		limit = ph7_value_to_int(apArg[3]);` |
|    18 | 1385 | `	}` |
|    55 | 1386 | `	if( nArg >= 6 ){` |
|     - | 1387 | `		/* $flags shapes the match array handed to the callback exactly as it` |
|     - | 1388 | `		 * shapes preg_match()'s &$matches (PREG_OFFSET_CAPTURE /` |
|     - | 1389 | `		 * PREG_UNMATCHED_AS_NULL). php validates nothing here, so neither do we. */` |
|    26 | 1390 | `		iFlags = ph7_value_to_int(apArg[5]);` |
|    12 | 1391 | `	}` |
|    55 | 1392 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1393 |  |
|    76 | 1394 | `	if( ph7_value_is_array(apArg[2]) ){` |
|     - | 1395 | `		/* Array subject: return an array, each element replaced, keys preserved` |
|     - | 1396 | `		 * (php; PHL used to stringify the whole array to "Array" and replace in` |
|     - | 1397 | `		 * THAT — a silent wrong answer). */` |
|    19 | 1398 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|    19 | 1399 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|    19 | 1400 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     - | 1401 | `		ph7_value sKey, sVal;` |
|     - | 1402 | `		ph7_hashmap_node *pNode;` |
|     - | 1403 | `		sxu32 n;` |
|    19 | 1404 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|   ! 0 | 1405 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1406 | `			return PH7_OK;` |
|     - | 1407 | `		}` |
|    19 | 1408 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|    19 | 1409 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    19 | 1410 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|    19 | 1411 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|    43 | 1412 | `		while( n > 0 ){` |
|     - | 1413 | `			const char *zSubject;` |
|     - | 1414 | `			int nSubLen;` |
|     - | 1415 | `			SyBlob sOut;` |
|    31 | 1416 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|    31 | 1417 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|    31 | 1418 | `			zSubject = ph7_value_to_string(&sVal, &nSubLen);` |
|    31 | 1419 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    46 | 1420 | `			rc = PcreCallbackReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen,` |
|    15 | 1421 | `				limit, iFlags, &count, &sOut);` |
|    31 | 1422 | `			if( rc != SXRET_OK ){` |
|     - | 1423 | `				/* A bad pattern with an array subject yields an empty array (php);` |
|     - | 1424 | `				 * the failure hits the first element, so pResult is still empty.` |
|     - | 1425 | `				 * A throwing callback unwinds with no result at all. */` |
|     7 | 1426 | `				SyBlobRelease(&sOut);` |
|     7 | 1427 | `				PH7_MemObjRelease(&sKey);` |
|     7 | 1428 | `				PH7_MemObjRelease(&sVal);` |
|     7 | 1429 | `				if( rc == PH7_EXCEPTION ){` |
|     3 | 1430 | `					return PH7_EXCEPTION;` |
|     - | 1431 | `				}` |
|     5 | 1432 | `				ph7_result_value(pCtx, pResult);` |
|     5 | 1433 | `				goto set_count;` |
|     - | 1434 | `			}` |
|    25 | 1435 | `			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    25 | 1436 | `			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|    25 | 1437 | `			ph7_value_reset_string_cursor(pElem);` |
|    25 | 1438 | `			SyBlobRelease(&sOut);` |
|    25 | 1439 | `			PH7_MemObjRelease(&sKey);` |
|    25 | 1440 | `			PH7_MemObjRelease(&sVal);` |
|    25 | 1441 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|    25 | 1442 | `			n--;` |
|     1 | 1443 | `		}` |
|    13 | 1444 | `		ph7_result_value(pCtx, pResult);` |
|     7 | 1445 | `	}else{` |
|     - | 1446 | `		/* Scalar subject: one replaced string. */` |
|     - | 1447 | `		const char *zSubject;` |
|     - | 1448 | `		int nSubLen;` |
|     - | 1449 | `		SyBlob sOut;` |
|    37 | 1450 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    37 | 1451 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    54 | 1452 | `		rc = PcreCallbackReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen,` |
|    17 | 1453 | `			limit, iFlags, &count, &sOut);` |
|    37 | 1454 | `		if( rc != SXRET_OK ){` |
|     5 | 1455 | `			SyBlobRelease(&sOut);` |
|     5 | 1456 | `			if( rc == PH7_EXCEPTION ){` |
|   ! 0 | 1457 | `				return PH7_EXCEPTION;` |
|     - | 1458 | `			}` |
|     - | 1459 | `			/* Scalar subject: a bad pattern returns NULL (php). */` |
|     5 | 1460 | `			ph7_result_null(pCtx);` |
|     5 | 1461 | `			goto set_count;` |
|     - | 1462 | `		}` |
|    33 | 1463 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    33 | 1464 | `		SyBlobRelease(&sOut);` |
|     - | 1465 | `	}` |
|    25 | 1466 | `set_count:` |
|     - | 1467 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|     - | 1468 | `	 * (php always writes it: 0, or the count accumulated by earlier good patterns). */` |
|    53 | 1469 | `	if( nArg >= 5 ){` |
|     - | 1470 | `		ph7_value sCount;` |
|    38 | 1471 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|    38 | 1472 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|    38 | 1473 | `		PH7_MemObjRelease(&sCount);` |
|    18 | 1474 | `	}` |
|    53 | 1475 | `	return PH7_OK;` |
|    29 | 1476 | `}` |
|     - | 1477 |  |
|     - | 1478 | `/* ======================================================================` |
|     - | 1479 | ` * preg_quote(str [, delimiter])` |
|     - | 1480 | ` * ====================================================================== */` |
|    26 | 1481 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1482 | `{` |
|    29 | 1483 | `	const char *zStr, *zDelim = 0;` |
|    29 | 1484 | `	int nLen, nDelimLen = 0;` |
|     - | 1485 | `	const char *z, *zEnd;` |
|     - | 1486 |  |
|    29 | 1487 | `	if( nArg < 1 ){` |
|   ! 0 | 1488 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1489 | `		return PH7_OK;` |
|     - | 1490 | `	}` |
|    29 | 1491 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|    29 | 1492 | `	if( nArg >= 2 ){` |
|    12 | 1493 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     5 | 1494 | `	}` |
|     - | 1495 | `	/* Type the result as a STRING up front: an empty subject quotes to the empty` |
|     - | 1496 | `	 * string, and the loop below would otherwise never touch the result at all,` |
|     - | 1497 | ``	 * leaving php's `string` return as NULL. */`` |
|    29 | 1498 | `	ph7_result_string(pCtx, "", 0);` |
|    29 | 1499 | `	z = zStr;` |
|    29 | 1500 | `	zEnd = &zStr[nLen];` |
|   651 | 1501 | `	while( z < zEnd ){` |
|   625 | 1502 | `		char c = *z;` |
|   625 | 1503 | `		if( c == '\0' ){` |
|     - | 1504 | `			/* php spells NUL as the three-digit escape "\000" (a backslash and a raw NUL` |
|     - | 1505 | `			 * byte, which is what this emitted, is not an escape at all: pcre reads the` |
|     - | 1506 | `			 * backslash as quoting the byte that FOLLOWS the NUL). */` |
|     9 | 1507 | `			ph7_result_string(pCtx, "\\000", 4);` |
|     9 | 1508 | `			z++;` |
|     9 | 1509 | `			continue;` |
|     - | 1510 | `		}` |
|   617 | 1511 | `		switch( c ){` |
|    30 | 1512 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1513 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1514 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1515 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1516 | `			case '#':` |
|    63 | 1517 | `				ph7_result_string(pCtx, "\\", 1);` |
|    63 | 1518 | `				break;` |
|   277 | 1519 | `			default:` |
|   557 | 1520 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     6 | 1521 | `					ph7_result_string(pCtx, "\\", 1);` |
|     2 | 1522 | `				}` |
|   554 | 1523 | `				break;` |
|     - | 1524 | `		}` |
|   617 | 1525 | `		ph7_result_string(pCtx, z, 1);` |
|   617 | 1526 | `		z++;` |
|     3 | 1527 | `	}` |
|    29 | 1528 | `	return PH7_OK;` |
|    16 | 1529 | `}` |
|     - | 1530 |  |
|     - | 1531 | `/* ======================================================================` |
|     - | 1532 | ` * preg_last_error()` |
|     - | 1533 | ` * ====================================================================== */` |
|   ! 0 | 1534 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1535 | `{` |
|   ! 0 | 1536 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1537 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1538 | `	return PH7_OK;` |
|   ! 0 | 1539 | `}` |
|     - | 1540 |  |
|     - | 1541 | `/* ======================================================================` |
|     - | 1542 | ` * preg_last_error_msg()` |
|     - | 1543 | ` * ====================================================================== */` |
|   ! 0 | 1544 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1545 | `{` |
|     - | 1546 | `	const char *zMsg;` |
|   ! 0 | 1547 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1548 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1549 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1550 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1551 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1552 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1553 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1554 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1555 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1556 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1557 | `	}` |
|   ! 0 | 1558 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1559 | `	return PH7_OK;` |
|   ! 0 | 1560 | `}` |
|     - | 1561 |  |
|     - | 1562 | `/*` |
|     - | 1563 | ` * The regex operation behind RegexIterator::accept(), in php's five REGIT modes.` |
|     - | 1564 | ` *` |
|     - | 1565 | ` * php reaches php_pcre_match_impl / php_pcre_split_impl / php_pcre_replace_impl` |
|     - | 1566 | ` * from spl_iterators.c rather than re-deriving any of it, and this is that door:` |
|     - | 1567 | ` * every mode is one of the builtins above, called with the arguments the PHP` |
|     - | 1568 | ` * spelling would have passed. The builtins answer through pCtx->pRet, which is` |
|     - | 1569 | ` * also accept()'s own return slot -- harmless because the caller writes its` |
|     - | 1570 | ` * boolean after this returns, and the reason pOut is a separate parameter.` |
|     - | 1571 | ` *` |
|     - | 1572 | ` * *pbOk is php's per-mode "matched" test: a positive match count, more than one` |
|     - | 1573 | ` * SPLIT piece, at least one REPLACE substitution. pOut receives the transformed` |
|     - | 1574 | ` * value for every mode but MATCH, which leaves the cached current() alone.` |
|     - | 1575 | ` */` |
|    82 | 1576 | `PH7_PRIVATE sxi32 PH7_PcreRegitApply(` |
|     - | 1577 | `	ph7_context *pCtx,` |
|     - | 1578 | `	int iMode,               /* PH7_REGIT_* */` |
|     - | 1579 | `	ph7_value *pPattern,` |
|     - | 1580 | `	ph7_value *pSubject,` |
|     - | 1581 | `	int iPregFlags,` |
|     - | 1582 | `	ph7_value *pRepl,        /* REPLACE only */` |
|     - | 1583 | `	ph7_value *pOut,         /* transformed value (modes other than MATCH) */` |
|     - | 1584 | `	int *pbOk` |
|     - | 1585 | `	)` |
|     1 | 1586 | `{` |
|     - | 1587 | `	ph7_value *apArg[5];` |
|     - | 1588 | `	ph7_value sFlags, sLimit, sCount;` |
|    83 | 1589 | `	sxi32 rc = PH7_OK;` |
|    83 | 1590 | `	*pbOk = 0;` |
|    83 | 1591 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sFlags,iPregFlags);` |
|    83 | 1592 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sLimit,-1);` |
|    83 | 1593 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sCount,0);` |
|     - | 1594 | `	/* A by-ref out-parameter with no caller slot behind it: PH7_VmStoreArgByRef` |
|     - | 1595 | `	 * writes through nIdx when it is not SXU32_HIGH, and a zeroed ph7_value's` |
|     - | 1596 | `	 * nIdx is 0 -- a REAL slot index, which would corrupt aMemObj[0]. */` |
|    83 | 1597 | `	sCount.nIdx = SXU32_HIGH;` |
|    83 | 1598 | `	if( pOut ){` |
|    83 | 1599 | `		pOut->nIdx = SXU32_HIGH;` |
|    41 | 1600 | `	}` |
|    83 | 1601 | `	apArg[0] = pPattern;` |
|    83 | 1602 | `	switch( iMode ){` |
|    18 | 1603 | `		case PH7_REGIT_MATCH:` |
|    37 | 1604 | `			apArg[1] = pSubject;` |
|    37 | 1605 | `			rc = PH7_builtin_preg_match(pCtx,2,apArg);` |
|    37 | 1606 | `			*pbOk = ph7_value_to_int(pCtx->pRet) > 0;` |
|    37 | 1607 | `			break;` |
|    12 | 1608 | `		case PH7_REGIT_GET_MATCH:` |
|     - | 1609 | `		case PH7_REGIT_ALL_MATCHES:` |
|    25 | 1610 | `			apArg[1] = pSubject;` |
|    25 | 1611 | `			apArg[2] = pOut;` |
|    25 | 1612 | `			apArg[3] = &sFlags;` |
|    25 | 1613 | `			rc = iMode == PH7_REGIT_GET_MATCH` |
|    20 | 1614 | `				? PH7_builtin_preg_match(pCtx,4,apArg)` |
|    14 | 1615 | `				: PH7_builtin_preg_match_all(pCtx,4,apArg);` |
|    25 | 1616 | `			*pbOk = ph7_value_to_int(pCtx->pRet) > 0;` |
|    25 | 1617 | `			break;` |
|     3 | 1618 | `		case PH7_REGIT_SPLIT:` |
|     7 | 1619 | `			apArg[1] = pSubject;` |
|     7 | 1620 | `			apArg[2] = &sLimit;` |
|     7 | 1621 | `			apArg[3] = &sFlags;` |
|     7 | 1622 | `			rc = PH7_builtin_preg_split(pCtx,4,apArg);` |
|     7 | 1623 | `			PH7_MemObjStore(pCtx->pRet,pOut);` |
|     7 | 1624 | `			if( pOut->iFlags & MEMOBJ_HASHMAP ){` |
|     7 | 1625 | `				*pbOk = ((ph7_hashmap *)pOut->x.pOther)->nEntry > 1;` |
|     3 | 1626 | `			}` |
|     7 | 1627 | `			break;` |
|     8 | 1628 | `		case PH7_REGIT_REPLACE:` |
|    17 | 1629 | `			apArg[1] = pRepl;` |
|    17 | 1630 | `			apArg[2] = pSubject;` |
|    17 | 1631 | `			apArg[3] = &sLimit;` |
|    17 | 1632 | `			apArg[4] = &sCount;` |
|    17 | 1633 | `			rc = PH7_builtin_preg_replace(pCtx,5,apArg);` |
|    17 | 1634 | `			PH7_MemObjStore(pCtx->pRet,pOut);` |
|    17 | 1635 | `			*pbOk = ph7_value_to_int(&sCount) > 0;` |
|    16 | 1636 | `			break;` |
|   ! 0 | 1637 | `		default:` |
|   ! 0 | 1638 | `			break;` |
|     - | 1639 | `	}` |
|    83 | 1640 | `	PH7_MemObjRelease(&sFlags);` |
|    83 | 1641 | `	PH7_MemObjRelease(&sLimit);` |
|    83 | 1642 | `	PH7_MemObjRelease(&sCount);` |
|    83 | 1643 | `	return rc;` |
|     1 | 1644 | `}` |
|     - | 1645 | `/* ===== Function registration table ===== */` |
|     - | 1646 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1647 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1648 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1649 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1650 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1651 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1652 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1653 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1654 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1655 | `};` |
|     - | 1656 |  |
|  4076 | 1657 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     5 | 1658 | `{` |
|     - | 1659 | `	sxu32 n;` |
| 36689 | 1660 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 32613 | 1661 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 16309 | 1662 | `	}` |
|  4081 | 1663 | `}` |
|     - | 1664 |  |
|     - | 1665 | `/* ===== Constant registration ===== */` |
|     - | 1666 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1667 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1668 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1669 | `	}` |
|     - | 1670 |  |
|    14 | 1671 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|    17 | 1672 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|    19 | 1673 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|    13 | 1674 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|   ! 0 | 1675 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|   ! 0 | 1676 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|   ! 0 | 1677 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|   ! 0 | 1678 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|   ! 0 | 1679 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|   ! 0 | 1680 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|   ! 0 | 1681 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|    13 | 1682 | `PCRE_CONST_INT(PREG_GREP_INVERT,         PHP_PREG_GREP_INVERT)` |
|   ! 0 | 1683 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|   ! 0 | 1684 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|   ! 0 | 1685 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1686 |  |
|  4076 | 1687 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     5 | 1688 | `{` |
|  4081 | 1689 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  4081 | 1690 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  4081 | 1691 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  4081 | 1692 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  4081 | 1693 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  4081 | 1694 | `	ph7_create_constant(&(*pVm), "PREG_GREP_INVERT",          PcreConst_PREG_GREP_INVERT, 0);` |
|  4081 | 1695 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  4081 | 1696 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  4081 | 1697 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  4081 | 1698 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  4081 | 1699 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  4081 | 1700 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  4081 | 1701 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  4081 | 1702 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  4081 | 1703 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  4081 | 1704 | `}` |
|     - | 1705 |  |
|     - | 1706 | `#else` |
|     - | 1707 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1708 | `typedef int vm_pcre_unused;` |
|     - | 1709 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1710 |  |
