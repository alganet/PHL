# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 931/1126 lines (82.68%)

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
|  1244 |   56 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|     5 |   57 | `{` |
|     - |   58 | `	sxu32 i;` |
|  7305 |   59 | `	for( i = 0; i < nCacheUsed; i++ ){` |
|  7053 |   60 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|   997 |   61 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|   997 |   62 | `			if( pCaptureCount ){` |
|   959 |   63 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|   477 |   64 | `			}` |
|   997 |   65 | `			return aCache[i].pCode;` |
|     - |   66 | `		}` |
|  3033 |   67 | `	}` |
|   257 |   68 | `	return 0;` |
|   627 |   69 | `}` |
|     - |   70 |  |
|   222 |   71 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|     5 |   72 | `{` |
|     - |   73 | `	PcreCacheEntry *pEntry;` |
|     - |   74 | `	char *zCopy;` |
|     - |   75 | `	/* Allocate the pattern copy first, before touching the cache */` |
|   227 |   76 | `	zCopy = (char *)malloc(nLen + 1);` |
|   227 |   77 | `	if( zCopy == 0 ){` |
|     - |   78 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|   ! 0 |   79 | `		return;` |
|     - |   80 | `	}` |
|   227 |   81 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|   227 |   82 | `	zCopy[nLen] = 0;` |
|   227 |   83 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|   111 |   84 | `		pEntry = &aCache[nCacheUsed++];` |
|    58 |   85 | `	}else{` |
|     - |   86 | `		/* Evict LRU */` |
|   117 |   87 | `		sxu32 iMin = aCache[0].iLastUsed;` |
|   117 |   88 | `		sxu32 iMinIdx = 0;` |
|     - |   89 | `		sxu32 i;` |
|  1857 |   90 | `		for( i = 1; i < PCRE_CACHE_SIZE; i++ ){` |
|  1741 |   91 | `			if( aCache[i].iLastUsed < iMin ){` |
|   231 |   92 | `				iMin = aCache[i].iLastUsed;` |
|   231 |   93 | `				iMinIdx = i;` |
|   115 |   94 | `			}` |
|   871 |   95 | `		}` |
|   117 |   96 | `		pEntry = &aCache[iMinIdx];` |
|   117 |   97 | `		pcre2_code_free(pEntry->pCode);` |
|   117 |   98 | `		free(pEntry->zPattern);` |
|     - |   99 | `	}` |
|   227 |  100 | `	pEntry->zPattern = zCopy;` |
|   227 |  101 | `	pEntry->nLen = nLen;` |
|   227 |  102 | `	pEntry->pCode = pCode;` |
|   227 |  103 | `	pEntry->nCaptureCount = nCaptureCount;` |
|   227 |  104 | `	pEntry->iLastUsed = ++iCacheClock;` |
|   116 |  105 | `}` |
|     - |  106 |  |
|     - |  107 | `/* ===== Delimiter parser ===== */` |
|     - |  108 | `#define PCRE_PARSE_OK             0` |
|     - |  109 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|     - |  110 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */` |
|     - |  111 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|     - |  112 |  |
|   252 |  113 | `static sxi32 PcreParsePattern(` |
|     - |  114 | `	const char *zInput, int nInputLen,` |
|     - |  115 | `	const char **pPattern, int *pnPatternLen,` |
|     - |  116 | `	const char **pFlags, int *pnFlagLen,` |
|     - |  117 | `	char *pCloseDelim, int *pbPaired)` |
|     5 |  118 | `{` |
|   257 |  119 | `	const char *zEnd = &zInput[nInputLen];` |
|   257 |  120 | `	const char *z = zInput;` |
|     - |  121 | `	char cOpen, cClose;` |
|     - |  122 | `	const char *pStart;` |
|     - |  123 |  |
|     - |  124 | `	/* Delimiter details for a "no ending delimiter" diagnostic (php names it) */` |
|   257 |  125 | `	*pCloseDelim = 0;` |
|   257 |  126 | `	*pbPaired = 0;` |
|     - |  127 | `	/* Skip leading whitespace */` |
|   257 |  128 | `	while( z < zEnd && (unsigned char)*z <= 0x20 ){` |
|   ! 0 |  129 | `		z++;` |
|   ! 0 |  130 | `	}` |
|   257 |  131 | `	if( z >= zEnd ){` |
|   ! 0 |  132 | `		return PCRE_PARSE_EMPTY;` |
|     - |  133 | `	}` |
|   257 |  134 | `	cOpen = *z;` |
|     - |  135 | `	/* Must not be alphanumeric, backslash, or whitespace */` |
|   257 |  136 | `	if( SyisAlphaNum(cOpen) \|\| cOpen == '\\' \|\| (unsigned char)cOpen <= 0x20 ){` |
|     6 |  137 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|     - |  138 | `	}` |
|     - |  139 | `	/* Paired delimiters */` |
|   253 |  140 | `	switch( cOpen ){` |
|     5 |  141 | `		case '(': cClose = ')'; break;` |
|     3 |  142 | `		case '[': cClose = ']'; break;` |
|     3 |  143 | `		case '{': cClose = '}'; break;` |
|     3 |  144 | `		case '<': cClose = '>'; break;` |
|   243 |  145 | `		default:  cClose = cOpen; break;` |
|     - |  146 | `	}` |
|   253 |  147 | `	*pCloseDelim = cClose;` |
|   253 |  148 | `	*pbPaired = (cOpen != cClose);` |
|   253 |  149 | `	z++; /* Skip opening delimiter */` |
|   253 |  150 | `	pStart = z;` |
|     - |  151 | `	/* Scan for closing delimiter, respecting backslash escapes */` |
| 14411 |  152 | `	while( z < zEnd ){` |
| 14395 |  153 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|  1133 |  154 | `			z += 2; /* Skip escaped char */` |
|  1133 |  155 | `			continue;` |
|     - |  156 | `		}` |
| 13267 |  157 | `		if( *z == cClose ){` |
|   237 |  158 | `			break;` |
|     - |  159 | `		}` |
| 13035 |  160 | `		z++;` |
|     5 |  161 | `	}` |
|   253 |  162 | `	if( z >= zEnd ){` |
|    17 |  163 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|     - |  164 | `	}` |
|   237 |  165 | `	*pPattern = pStart;` |
|   237 |  166 | `	*pnPatternLen = (int)(z - pStart);` |
|   237 |  167 | `	z++; /* Skip closing delimiter */` |
|   237 |  168 | `	*pFlags = z;` |
|   237 |  169 | `	*pnFlagLen = (int)(zEnd - z);` |
|   237 |  170 | `	return PH7_OK;` |
|   131 |  171 | `}` |
|     - |  172 |  |
|     - |  173 | `/* ===== Flag mapper ===== */` |
|   232 |  174 | `static sxi32 PcreMapFlags(` |
|     - |  175 | `	const char *zFlags, int nFlagLen,` |
|     - |  176 | `	uint32_t *pCompileOpts)` |
|     5 |  177 | `{` |
|     - |  178 | `	int i;` |
|   237 |  179 | `	*pCompileOpts = 0;` |
|   291 |  180 | `	for( i = 0; i < nFlagLen; i++ ){` |
|    58 |  181 | `		switch( zFlags[i] ){` |
|    30 |  182 | `			case 'i': *pCompileOpts \|= PCRE2_CASELESS; break;` |
|     7 |  183 | `			case 'm': *pCompileOpts \|= PCRE2_MULTILINE; break;` |
|     5 |  184 | `			case 's': *pCompileOpts \|= PCRE2_DOTALL; break;` |
|   ! 0 |  185 | `			case 'x': *pCompileOpts \|= PCRE2_EXTENDED; break;` |
|     8 |  186 | `			case 'u': *pCompileOpts \|= PCRE2_UTF \| PCRE2_UCP; break;` |
|   ! 0 |  187 | `			case 'A': *pCompileOpts \|= PCRE2_ANCHORED; break;` |
|    16 |  188 | `			case 'D': *pCompileOpts \|= PCRE2_DOLLAR_ENDONLY; break;` |
|   ! 0 |  189 | `			case 'U': *pCompileOpts \|= PCRE2_UNGREEDY; break;` |
|   ! 0 |  190 | `			case 'J': *pCompileOpts \|= PCRE2_DUPNAMES; break;` |
|   ! 0 |  191 | `			case 'S': /* Study hint — no-op in PCRE2 */ break;` |
|   ! 0 |  192 | `			default: break;` |
|     - |  193 | `		}` |
|    31 |  194 | `	}` |
|   237 |  195 | `	return PH7_OK;` |
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
|  1244 |  207 | `static pcre2_code *PcreCompileQuiet(` |
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
|  1249 |  224 | `	if( nErr > 0 ){` |
|  1249 |  225 | `		zErr[0] = 0;` |
|   622 |  226 | `	}` |
|     - |  227 | `	/* Check cache first */` |
|  1249 |  228 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|  1249 |  229 | `	if( pCode ){` |
|   997 |  230 | `		return pCode;` |
|     - |  231 | `	}` |
|     - |  232 | `	/* Parse delimiter */` |
|   257 |  233 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen,` |
|     - |  234 | `		&cDelim, &bPaired);` |
|   257 |  235 | `	if( parseRc != PCRE_PARSE_OK ){` |
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
|   237 |  251 | `	PcreMapFlags(zFlags, nFlagLen, &compileOpts);` |
|     - |  252 | `	/* Compile */` |
|   237 |  253 | `	pCode = pcre2_compile(` |
|   116 |  254 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|   116 |  255 | `		compileOpts, &errcode, &erroffset, NULL);` |
|   237 |  256 | `	if( pCode == 0 ){` |
|     - |  257 | `		PCRE2_UCHAR errbuf[256];` |
|    11 |  258 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|    16 |  259 | `		SyBufferFormat(zErr, nErr,` |
|     5 |  260 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|    11 |  261 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|    11 |  262 | `		return 0;` |
|     - |  263 | `	}` |
|     - |  264 | `	/* Get capture count */` |
|   227 |  265 | `	nCapture = 0;` |
|   227 |  266 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|   227 |  267 | `	if( pCaptureCount ){` |
|   207 |  268 | `		*pCaptureCount = nCapture;` |
|   101 |  269 | `	}` |
|     - |  270 | `	/* Cache it */` |
|   227 |  271 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|   227 |  272 | `	pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   227 |  273 | `	return pCode;` |
|   627 |  274 | `}` |
|  1184 |  275 | `static pcre2_code *PcreCompile(` |
|     - |  276 | `	ph7_context *pCtx,` |
|     - |  277 | `	const char *zFullPattern, int nLen,` |
|     - |  278 | `	sxu32 *pCaptureCount)` |
|     5 |  279 | `{` |
|     - |  280 | `	char zErr[288];` |
|  1781 |  281 | `	pcre2_code *pCode = PcreCompileQuiet(pCtx->pVm, zFullPattern, nLen, pCaptureCount,` |
|   592 |  282 | `		zErr, sizeof(zErr));` |
|  1189 |  283 | `	if( pCode == 0 && zErr[0] ){` |
|    30 |  284 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, zErr);` |
|    14 |  285 | `	}` |
|  1189 |  286 | `	return pCode;` |
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
|   206 |  328 | `static void PcrePopulateMatches(` |
|     - |  329 | `	ph7_context *pCtx,` |
|     - |  330 | `	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */` |
|     - |  331 | `	const char *zSubject,` |
|     - |  332 | `	PCRE2_SIZE *ovector,` |
|     - |  333 | `	int nGroups,` |
|     - |  334 | `	pcre2_code *pCode,` |
|     - |  335 | `	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */` |
|     5 |  336 | `{` |
|   211 |  337 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   211 |  338 | `	ph7_value *pSub = 0;` |
|   211 |  339 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|   211 |  340 | `	PCRE2_SPTR nametable = 0;` |
|   211 |  341 | `	int nMatched = nGroups;` |
|     - |  342 | `	int i;` |
|     - |  343 |  |
|   211 |  344 | `	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|    19 |  345 | `		pSub = ph7_context_new_array(pCtx);` |
|     8 |  346 | `	}` |
|   211 |  347 | `	if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|     - |  348 | `		/* pcre2 answers only as many pairs as the LAST group that participated, so` |
|     - |  349 | `		 * a pattern's trailing optional groups are simply absent -- which is php's` |
|     - |  350 | `		 * default shape too. PREG_UNMATCHED_AS_NULL is the flag that says "report` |
|     - |  351 | `		 * every group", so the tail is filled out to the pattern's own capture` |
|     - |  352 | `		 * count and each missing one answers NULL. Reading the flag only INSIDE the` |
|     - |  353 | ``		 * loop meant `preg_match('/(a)(x)?/','a',$m,PREG_UNMATCHED_AS_NULL)` still`` |
|     - |  354 | `		 * answered two entries where php answers three. */` |
|    39 |  355 | `		uint32_t nCapture = 0;` |
|    39 |  356 | `		pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|    39 |  357 | `		if( (int)nCapture + 1 > nGroups ){` |
|    23 |  358 | `			nGroups = (int)nCapture + 1;` |
|    11 |  359 | `		}` |
|    18 |  360 | `	}` |
|     - |  361 | `	/* Read the name table up front so each group's named key can be emitted` |
|     - |  362 | `	 * INTERLEAVED with its numbered key, in group order — php stores` |
|     - |  363 | ``	 * `0, name, 1, value, 2` (named entry immediately before its number), not`` |
|     - |  364 | `	 * every number followed by every name. Code that iterates $matches or` |
|     - |  365 | `	 * var_dumps it (PHPUnit's annotation parser) depends on this order. */` |
|   211 |  366 | `	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|   211 |  367 | `	if( namecount > 0 ){` |
|    23 |  368 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|    23 |  369 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|    11 |  370 | `	}` |
|   601 |  371 | `	for( i = 0; i < nGroups; i++ ){` |
|   395 |  372 | `		PCRE2_SIZE start = i < nMatched ? ovector[2 * i]     : PCRE2_UNSET;` |
|   395 |  373 | `		PCRE2_SIZE end   = i < nMatched ? ovector[2 * i + 1] : PCRE2_UNSET;` |
|   395 |  374 | `		const char *zName = 0;` |
|     - |  375 | `		/* Does group i carry a (?<name>...) label? namecount is tiny in practice. */` |
|   395 |  376 | `		if( namecount > 0 ){` |
|     - |  377 | `			uint32_t k;` |
|   131 |  378 | `			for( k = 0; k < namecount; k++ ){` |
|   107 |  379 | `				PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|   107 |  380 | `				if( (((entry[0] << 8) \| entry[1])) == i ){` |
|    43 |  381 | `					zName = (const char *)(entry + 2);` |
|    43 |  382 | `					break;` |
|     - |  383 | `				}` |
|    33 |  384 | `			}` |
|    33 |  385 | `		}` |
|   395 |  386 | `		if( start == PCRE2_UNSET ){` |
|    43 |  387 | `			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|    39 |  388 | `				ph7_value_null(pVal);` |
|    20 |  389 | `			}else{` |
|     5 |  390 | `				ph7_value_string(pVal, "", 0);` |
|     - |  391 | `			}` |
|     - |  392 | ``			/* Duplicate group names -- `(?J)` -- give two numbered groups one key,`` |
|     - |  393 | `			 * and only one of them can have participated. php writes a name key` |
|     - |  394 | `			 * from a group that did NOT participate only when nothing is there` |
|     - |  395 | ``			 * yet, so `/(?J)(?<d>a)\|(?<d>b)/` on "a" keeps `d => "a"` instead of`` |
|     - |  396 | `			 * having the other alternative's NULL land on top of it. */` |
|    43 |  397 | `			if( zName && ph7_array_fetch(pArray, zName, -1) != 0 ){` |
|     9 |  398 | `				zName = 0;` |
|     4 |  399 | `			}` |
|    22 |  400 | `		}else{` |
|   353 |  401 | `			ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|     - |  402 | `		}` |
|   395 |  403 | `		if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|    37 |  404 | `			ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|    37 |  405 | `			ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|    37 |  406 | `			ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);` |
|    37 |  407 | `			ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|     - |  408 | `			/* php: the named key comes first, then the numbered key (same value). */` |
|    37 |  409 | `			if( zName ){` |
|     3 |  410 | `				ph7_array_add_strkey_elem(pArray, zName, pSub);` |
|     1 |  411 | `			}` |
|    37 |  412 | `			ph7_array_add_intkey_elem(pArray, i, pSub);` |
|    37 |  413 | `			ph7_context_release_value(pCtx, pOff);` |
|    37 |  414 | `			ph7_context_release_value(pCtx, pSub);` |
|    37 |  415 | `			pSub = ph7_context_new_array(pCtx);` |
|    20 |  416 | `		}else{` |
|   361 |  417 | `			if( zName ){` |
|    33 |  418 | `				ph7_array_add_strkey_elem(pArray, zName, pVal);` |
|    16 |  419 | `			}` |
|   361 |  420 | `			ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  421 | `		}` |
|   395 |  422 | `		ph7_value_reset_string_cursor(pVal);` |
|   200 |  423 | `	}` |
|   211 |  424 | `	ph7_context_release_value(pCtx, pVal);` |
|   211 |  425 | `	if( pSub ){` |
|    19 |  426 | `		ph7_context_release_value(pCtx, pSub);` |
|     8 |  427 | `	}` |
|   211 |  428 | `}` |
|     - |  429 |  |
|     - |  430 | `/*` |
|     - |  431 | ` * Quiet whole-pattern match used by FILTER_VALIDATE_REGEXP: compile zPat (a full` |
|     - |  432 | ` * "/.../flags" pattern) and test it against zSub. On a successful attempt returns` |
|     - |  433 | ` * SXRET_OK with *pMatched set to 1 (match) or 0 (no match); returns SXERR_INVALID` |
|     - |  434 | ` * on a compile/match error (the caller treats that as a validation failure). The` |
|     - |  435 | ` * compiled code is owned by PcreCompile's cache, so it is not freed here.` |
|     - |  436 | ` */` |
|   204 |  437 | `PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,` |
|     - |  438 | `	const char *zSub,int nSub,int *pMatched)` |
|     4 |  439 | `{` |
|     - |  440 | `	pcre2_code *pCode;` |
|     - |  441 | `	pcre2_match_data *pMatchData;` |
|     - |  442 | `	sxu32 nCapture;` |
|     - |  443 | `	int rc;` |
|   208 |  444 | `	*pMatched = 0;` |
|   208 |  445 | `	pCode = PcreCompile(pCtx,zPat,nPat,&nCapture);` |
|   208 |  446 | `	if( pCode == 0 ){` |
|   ! 0 |  447 | `		return SXERR_INVALID;` |
|     - |  448 | `	}` |
|   208 |  449 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode,NULL);` |
|   208 |  450 | `	if( pMatchData == 0 ){` |
|   ! 0 |  451 | `		return SXERR_INVALID;` |
|     - |  452 | `	}` |
|   208 |  453 | `	rc = pcre2_match(pCode,(PCRE2_SPTR)zSub,(PCRE2_SIZE)nSub,0,0,pMatchData,NULL);` |
|   208 |  454 | `	pcre2_match_data_free(pMatchData);` |
|   208 |  455 | `	if( rc < 0 ){` |
|   137 |  456 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  457 | `			PcreSetMatchError(pCtx->pVm,rc);` |
|   ! 0 |  458 | `			return SXERR_INVALID;` |
|     - |  459 | `		}` |
|   137 |  460 | `		return SXRET_OK; /* clean no-match */` |
|     - |  461 | `	}` |
|    73 |  462 | `	*pMatched = 1;` |
|    73 |  463 | `	return SXRET_OK;` |
|   106 |  464 | `}` |
|     - |  465 | `/* ======================================================================` |
|     - |  466 | ` * preg_match(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  467 | ` * ====================================================================== */` |
|   358 |  468 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  469 | `{` |
|     - |  470 | `	const char *zPattern, *zSubject;` |
|     - |  471 | `	int nPatLen, nSubLen;` |
|     - |  472 | `	pcre2_code *pCode;` |
|     - |  473 | `	pcre2_match_data *pMatchData;` |
|     - |  474 | `	PCRE2_SIZE *ovector;` |
|     - |  475 | `	sxu32 nCapture;` |
|   363 |  476 | `	PCRE2_SIZE startOffset = 0;` |
|   363 |  477 | `	int iFlags = 0;` |
|     - |  478 | `	int rc;` |
|     - |  479 |  |
|   363 |  480 | `	if( nArg < 2 ){` |
|   ! 0 |  481 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  482 | `			"preg_match() expects at least 2 parameters");` |
|   ! 0 |  483 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  484 | `		return PH7_OK;` |
|     - |  485 | `	}` |
|   363 |  486 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|   363 |  487 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|   363 |  488 | `	if( nArg >= 4 ){` |
|    68 |  489 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|    33 |  490 | `	}` |
|   363 |  491 | `	if( nArg >= 5 ){` |
|   ! 0 |  492 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  493 | `	}` |
|   363 |  494 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|   363 |  495 | `	if( pCode == 0 ){` |
|    15 |  496 | `		ph7_result_bool(pCtx, 0);` |
|    15 |  497 | `		return PH7_OK;` |
|     - |  498 | `	}` |
|     - |  499 | `	/* php validates $flags AFTER the pattern compiles (a bad pattern warns first).` |
|     - |  500 | `	 * php 8.5 only rejects flag bits BELOW PREG_OFFSET_CAPTURE (the low byte); any` |
|     - |  501 | `	 * higher bit is ignored. preg_match permits none of those low bits. */` |
|   349 |  502 | `	if( (iFlags & (PHP_PREG_OFFSET_CAPTURE - 1)) != 0 ){` |
|    11 |  503 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  504 | `			"preg_match(): Argument #4 ($flags) must be a PREG_* constant");` |
|     - |  505 | `	}` |
|   339 |  506 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|   339 |  507 | `	if( pMatchData == 0 ){` |
|   ! 0 |  508 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  509 | `		return PH7_OK;` |
|     - |  510 | `	}` |
|   506 |  511 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|   167 |  512 | `		startOffset, 0, pMatchData, NULL);` |
|   339 |  513 | `	if( rc < 0 ){` |
|   146 |  514 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  515 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  516 | `		}` |
|     - |  517 | `		/* Populate empty matches if requested */` |
|   146 |  518 | `		if( nArg >= 3 ){` |
|    30 |  519 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    30 |  520 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pEmpty);` |
|    30 |  521 | `			ph7_context_release_value(pCtx, pEmpty);` |
|    14 |  522 | `		}` |
|   146 |  523 | `		pcre2_match_data_free(pMatchData);` |
|   146 |  524 | `		ph7_result_int(pCtx, 0);` |
|   146 |  525 | `		return PH7_OK;` |
|     - |  526 | `	}` |
|   195 |  527 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   195 |  528 | `	if( nArg >= 3 ){` |
|     - |  529 | `		/* Populate $matches */` |
|   105 |  530 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|   105 |  531 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|   105 |  532 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);` |
|     - |  533 | `		/* Write the array back to the caller's variable */` |
|   105 |  534 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pArray);` |
|   105 |  535 | `		ph7_context_release_value(pCtx, pArray);` |
|    50 |  536 | `	}` |
|   195 |  537 | `	pcre2_match_data_free(pMatchData);` |
|   195 |  538 | `	ph7_result_int(pCtx, 1);` |
|   195 |  539 | `	return PH7_OK;` |
|   184 |  540 | `}` |
|     - |  541 |  |
|     - |  542 | `/* ======================================================================` |
|     - |  543 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  544 | ` * ====================================================================== */` |
|    46 |  545 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |  546 | `{` |
|     - |  547 | `	const char *zPattern, *zSubject;` |
|     - |  548 | `	int nPatLen, nSubLen;` |
|     - |  549 | `	pcre2_code *pCode;` |
|     - |  550 | `	pcre2_match_data *pMatchData;` |
|     - |  551 | `	sxu32 nCapture;` |
|    48 |  552 | `	PCRE2_SIZE startOffset = 0;` |
|    48 |  553 | `	int iFlags = PHP_PREG_PATTERN_ORDER;` |
|    48 |  554 | `	int totalMatches = 0;` |
|     - |  555 | `	int rc;` |
|     - |  556 |  |
|    48 |  557 | `	if( nArg < 2 ){` |
|   ! 0 |  558 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  559 | `			"preg_match_all() expects at least 2 parameters");` |
|   ! 0 |  560 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  561 | `		return PH7_OK;` |
|     - |  562 | `	}` |
|    48 |  563 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    48 |  564 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    48 |  565 | `	if( nArg >= 4 ){` |
|    40 |  566 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|    19 |  567 | `	}` |
|    48 |  568 | `	if( nArg >= 5 ){` |
|   ! 0 |  569 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  570 | `	}` |
|    48 |  571 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    48 |  572 | `	if( pCode == 0 ){` |
|   ! 0 |  573 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  574 | `		return PH7_OK;` |
|     - |  575 | `	}` |
|     - |  576 | `	/* php validates $flags AFTER the pattern compiles (a bad pattern warns first).` |
|     - |  577 | `	 * php 8.5 rejects low-byte bits below PREG_OFFSET_CAPTURE EXCEPT the order flags,` |
|     - |  578 | `	 * and rejects PATTERN_ORDER+SET_ORDER together (mutually exclusive); higher bits` |
|     - |  579 | `	 * are ignored. Every case raises the same ValueError. */` |
|    46 |  580 | `	if( (iFlags & (PHP_PREG_OFFSET_CAPTURE - 1)` |
|    46 |  581 | `			& ~(PHP_PREG_PATTERN_ORDER\|PHP_PREG_SET_ORDER)) != 0` |
|    46 |  582 | `		\|\| ((iFlags & PHP_PREG_PATTERN_ORDER) && (iFlags & PHP_PREG_SET_ORDER)) ){` |
|     9 |  583 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  584 | `			"preg_match_all(): Argument #4 ($flags) must be a PREG_* constant");` |
|     - |  585 | `	}` |
|    40 |  586 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    40 |  587 | `	if( pMatchData == 0 ){` |
|   ! 0 |  588 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  589 | `		return PH7_OK;` |
|     - |  590 | `	}` |
|    40 |  591 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  592 | `	{` |
|    40 |  593 | `		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;` |
|     - |  594 |  |
|    40 |  595 | `		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){` |
|    28 |  596 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  597 | `				PCRE2_SIZE *ovector;` |
|    41 |  598 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    13 |  599 | `					startOffset, 0, pMatchData, NULL);` |
|    28 |  600 | `				if( rc < 0 ){` |
|    12 |  601 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|    12 |  602 | `					break;` |
|     - |  603 | `				}` |
|    18 |  604 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    18 |  605 | `				if( pOutArray ){` |
|    18 |  606 | `					ph7_value *pSet = ph7_context_new_array(pCtx);` |
|    18 |  607 | `					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, iFlags & ~0xFF);` |
|    18 |  608 | `					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);` |
|    18 |  609 | `					ph7_context_release_value(pCtx, pSet);` |
|     8 |  610 | `				}` |
|    18 |  611 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  612 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  613 | `				}else{` |
|    18 |  614 | `					startOffset = ovector[1];` |
|     - |  615 | `				}` |
|    18 |  616 | `				totalMatches++;` |
|     2 |  617 | `			}` |
|     7 |  618 | `		}else{` |
|     - |  619 | `			/* PREG_PATTERN_ORDER (default) */` |
|    30 |  620 | `			ph7_value **apGroupArrays = 0;` |
|    30 |  621 | `			sxu32 nGroups = nCapture + 1;` |
|     - |  622 | `			sxu32 g;` |
|    30 |  623 | `			if( pOutArray ){` |
|    44 |  624 | `				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,` |
|    14 |  625 | `					sizeof(ph7_value *) * nGroups, TRUE, FALSE);` |
|    30 |  626 | `				if( apGroupArrays ){` |
|    86 |  627 | `					for( g = 0; g < nGroups; g++ ){` |
|    58 |  628 | `						apGroupArrays[g] = ph7_context_new_array(pCtx);` |
|    30 |  629 | `					}` |
|    14 |  630 | `				}` |
|    14 |  631 | `			}` |
|    78 |  632 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  633 | `				PCRE2_SIZE *ovector;` |
|   116 |  634 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    38 |  635 | `					startOffset, 0, pMatchData, NULL);` |
|    78 |  636 | `				if( rc < 0 ){` |
|    30 |  637 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|    30 |  638 | `					break;` |
|     - |  639 | `				}` |
|    50 |  640 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    50 |  641 | `				if( apGroupArrays ){` |
|    50 |  642 | `					ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    50 |  643 | `					int nActual = rc;` |
|   150 |  644 | `					for( g = 0; g < nGroups; g++ ){` |
|   144 |  645 | `						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){` |
|    86 |  646 | `							PCRE2_SIZE s = ovector[2*g];` |
|    86 |  647 | `							PCRE2_SIZE e = ovector[2*g+1];` |
|    86 |  648 | `							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|    20 |  649 | `								ph7_value *pSub = ph7_context_new_array(pCtx);` |
|    20 |  650 | `								ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|    20 |  651 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|    20 |  652 | `								ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|    20 |  653 | `								ph7_value_int(pOff, (int)s);` |
|    20 |  654 | `								ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|    20 |  655 | `								ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|    20 |  656 | `								ph7_context_release_value(pCtx, pSub);` |
|    20 |  657 | `								ph7_context_release_value(pCtx, pOff);` |
|    11 |  658 | `							}else{` |
|    68 |  659 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|    68 |  660 | `								ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     2 |  661 | `							}` |
|    59 |  662 | `						}else if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|     - |  663 | `							/* php reports an unmatched group as the pair ("", -1) --` |
|     - |  664 | `							 * or (NULL, -1) under PREG_UNMATCHED_AS_NULL. Both flags` |
|     - |  665 | `							 * were read only on the MATCHED arm, so an unmatched` |
|     - |  666 | `							 * group answered a bare "" whatever was asked for. */` |
|     9 |  667 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|     9 |  668 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|     9 |  669 | `							if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|     5 |  670 | `								ph7_value_null(pVal);` |
|     3 |  671 | `							}else{` |
|     5 |  672 | `								ph7_value_string(pVal, "", 0);` |
|     - |  673 | `							}` |
|     9 |  674 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|     9 |  675 | `							ph7_value_int(pOff, -1);` |
|     9 |  676 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|     9 |  677 | `							ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|     9 |  678 | `							ph7_context_release_value(pCtx, pSub);` |
|     9 |  679 | `							ph7_context_release_value(pCtx, pOff);` |
|     5 |  680 | `						}else{` |
|     9 |  681 | `							if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|     7 |  682 | `								ph7_value_null(pVal);` |
|     4 |  683 | `							}else{` |
|     3 |  684 | `								ph7_value_string(pVal, "", 0);` |
|     - |  685 | `							}` |
|     9 |  686 | `							ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  687 | `						}` |
|   102 |  688 | `						ph7_value_reset_string_cursor(pVal);` |
|    52 |  689 | `					}` |
|    50 |  690 | `					ph7_context_release_value(pCtx, pVal);` |
|    24 |  691 | `				}` |
|    50 |  692 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  693 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  694 | `				}else{` |
|    50 |  695 | `					startOffset = ovector[1];` |
|     - |  696 | `				}` |
|    50 |  697 | `				totalMatches++;` |
|     2 |  698 | `			}` |
|    30 |  699 | `			if( apGroupArrays ){` |
|     - |  700 | `				/* Attach the per-group match arrays. php's PREG_PATTERN_ORDER stores a` |
|     - |  701 | `				 * named group under BOTH its name and its number, interleaved` |
|     - |  702 | ``				 * (`0, name, 1, value, 2`) — the same value under each key. Read the`` |
|     - |  703 | `				 * name table so each numbered group can emit its named alias first. */` |
|    30 |  704 | `				uint32_t namecount = 0, nameentrysize = 0;` |
|    30 |  705 | `				PCRE2_SPTR nametable = 0;` |
|    30 |  706 | `				pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|    30 |  707 | `				if( namecount > 0 ){` |
|     5 |  708 | `					pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     5 |  709 | `					pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|     2 |  710 | `				}` |
|    86 |  711 | `				for( g = 0; g < nGroups; g++ ){` |
|    58 |  712 | `					const char *zName = 0;` |
|    58 |  713 | `					if( namecount > 0 ){` |
|     - |  714 | `						uint32_t k;` |
|    23 |  715 | `						for( k = 0; k < namecount; k++ ){` |
|    17 |  716 | `							PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|    17 |  717 | `							if( (uint32_t)(((entry[0] << 8) \| entry[1])) == g ){` |
|     7 |  718 | `								zName = (const char *)(entry + 2);` |
|     7 |  719 | `								break;` |
|     - |  720 | `							}` |
|     6 |  721 | `						}` |
|     6 |  722 | `					}` |
|    58 |  723 | `					if( zName ){` |
|     7 |  724 | `						ph7_array_add_strkey_elem(pOutArray, zName, apGroupArrays[g]);` |
|     3 |  725 | `					}` |
|    58 |  726 | `					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);` |
|    58 |  727 | `					ph7_context_release_value(pCtx, apGroupArrays[g]);` |
|    30 |  728 | `				}` |
|    30 |  729 | `				ph7_context_free_chunk(pCtx, apGroupArrays);` |
|    14 |  730 | `			}` |
|     - |  731 | `		}` |
|     - |  732 | `		/* Write output array to caller's variable */` |
|    40 |  733 | `		if( pOutArray && nArg >= 3 ){` |
|    40 |  734 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pOutArray);` |
|    40 |  735 | `			ph7_context_release_value(pCtx, pOutArray);` |
|    19 |  736 | `		}` |
|     - |  737 | `	}` |
|    40 |  738 | `	pcre2_match_data_free(pMatchData);` |
|    40 |  739 | `	ph7_result_int(pCtx, totalMatches);` |
|    40 |  740 | `	return PH7_OK;` |
|    25 |  741 | `}` |
|     - |  742 |  |
|     - |  743 | `/* ======================================================================` |
|     - |  744 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|     - |  745 | ` * ====================================================================== */` |
|    12 |  746 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |  747 | `{` |
|     - |  748 | `	const char *zPattern, *zSubject;` |
|     - |  749 | `	int nPatLen, nSubLen;` |
|     - |  750 | `	pcre2_code *pCode;` |
|     - |  751 | `	pcre2_match_data *pMatchData;` |
|     - |  752 | `	sxu32 nCapture;` |
|     - |  753 | `	ph7_value *pArray;` |
|     - |  754 | `	ph7_value *pVal;` |
|    14 |  755 | `	PCRE2_SIZE startOffset = 0, lastOffset = 0;` |
|    14 |  756 | `	int limit = -1;` |
|    14 |  757 | `	int iFlags = 0;` |
|    14 |  758 | `	int nPieces = 0;` |
|     - |  759 | `	int rc;` |
|     - |  760 |  |
|    14 |  761 | `	if( nArg < 2 ){` |
|   ! 0 |  762 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  763 | `			"preg_split() expects at least 2 parameters");` |
|   ! 0 |  764 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  765 | `		return PH7_OK;` |
|     - |  766 | `	}` |
|    14 |  767 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    14 |  768 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    14 |  769 | `	if( nArg >= 3 ){` |
|     9 |  770 | `		limit = ph7_value_to_int(apArg[2]);` |
|     4 |  771 | `	}` |
|    14 |  772 | `	if( nArg >= 4 ){` |
|     7 |  773 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     3 |  774 | `	}` |
|    14 |  775 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    14 |  776 | `	if( pCode == 0 ){` |
|     3 |  777 | `		ph7_result_bool(pCtx, 0);` |
|     3 |  778 | `		return PH7_OK;` |
|     - |  779 | `	}` |
|    11 |  780 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    11 |  781 | `	if( pMatchData == 0 ){` |
|   ! 0 |  782 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  783 | `		return PH7_OK;` |
|     - |  784 | `	}` |
|    11 |  785 | `	pArray = ph7_context_new_array(pCtx);` |
|    11 |  786 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    11 |  787 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  788 |  |
|    25 |  789 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|    25 |  790 | `		if( limit > 0 && nPieces >= limit - 1 ){` |
|     3 |  791 | `			break; /* Last piece gets the remainder */` |
|     - |  792 | `		}` |
|    34 |  793 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    11 |  794 | `			startOffset, 0, pMatchData, NULL);` |
|    23 |  795 | `		if( rc < 0 ){` |
|     9 |  796 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  797 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  798 | `			}` |
|     9 |  799 | `			break;` |
|     - |  800 | `		}` |
|     - |  801 | `		{` |
|    15 |  802 | `			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    15 |  803 | `			PCRE2_SIZE matchStart = ovector[0];` |
|    15 |  804 | `			PCRE2_SIZE matchEnd = ovector[1];` |
|    15 |  805 | `			int pieceLen = (int)(matchStart - lastOffset);` |
|     - |  806 |  |
|     - |  807 | `			/* Add the piece before the match */` |
|    15 |  808 | `			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| pieceLen > 0 ){` |
|    15 |  809 | `				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  810 | `					ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  811 | `					ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  812 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|   ! 0 |  813 | `					ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  814 | `					ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  815 | `					ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  816 | `					ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  817 | `					ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  818 | `					ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  819 | `				}else{` |
|    15 |  820 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|    15 |  821 | `					ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  822 | `				}` |
|    15 |  823 | `				ph7_value_reset_string_cursor(pVal);` |
|    15 |  824 | `				nPieces++;` |
|     7 |  825 | `			}` |
|     - |  826 | `			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */` |
|    15 |  827 | `			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){` |
|     - |  828 | `				int g;` |
|   ! 0 |  829 | `				for( g = 1; g < rc; g++ ){` |
|   ! 0 |  830 | `					PCRE2_SIZE gs = ovector[2*g];` |
|   ! 0 |  831 | `					PCRE2_SIZE ge = ovector[2*g+1];` |
|     - |  832 | `					int gLen;` |
|   ! 0 |  833 | `					if( gs == PCRE2_UNSET ) continue;` |
|   ! 0 |  834 | `					gLen = (int)(ge - gs);` |
|   ! 0 |  835 | `					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| gLen > 0 ){` |
|   ! 0 |  836 | `						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  837 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  838 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  839 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  840 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  841 | `							ph7_value_int(pOff, (int)gs);` |
|   ! 0 |  842 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  843 | `							ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  844 | `							ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  845 | `							ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  846 | `						}else{` |
|   ! 0 |  847 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  848 | `							ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  849 | `						}` |
|   ! 0 |  850 | `						ph7_value_reset_string_cursor(pVal);` |
|   ! 0 |  851 | `					}` |
|   ! 0 |  852 | `				}` |
|   ! 0 |  853 | `			}` |
|     - |  854 | `			/* Advance */` |
|    15 |  855 | `			lastOffset = matchEnd;` |
|    15 |  856 | `			if( matchEnd == matchStart ){` |
|   ! 0 |  857 | `				startOffset = matchEnd + 1;` |
|   ! 0 |  858 | `			}else{` |
|    15 |  859 | `				startOffset = matchEnd;` |
|     - |  860 | `			}` |
|     - |  861 | `		}` |
|     1 |  862 | `	}` |
|     - |  863 | `	/* Add trailing piece */` |
|     - |  864 | `	{` |
|    11 |  865 | `		int trailLen = nSubLen - (int)lastOffset;` |
|    11 |  866 | `		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| trailLen > 0 ){` |
|    11 |  867 | `			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  868 | `				ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  869 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  870 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|   ! 0 |  871 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  872 | `				ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  873 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  874 | `				ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  875 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  876 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  877 | `			}else{` |
|    11 |  878 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|    11 |  879 | `				ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  880 | `			}` |
|     5 |  881 | `		}` |
|     - |  882 | `	}` |
|    11 |  883 | `	ph7_context_release_value(pCtx, pVal);` |
|    11 |  884 | `	pcre2_match_data_free(pMatchData);` |
|    11 |  885 | `	ph7_result_value(pCtx, pArray);` |
|    11 |  886 | `	ph7_context_release_value(pCtx, pArray);` |
|    11 |  887 | `	return PH7_OK;` |
|     8 |  888 | `}` |
|     - |  889 |  |
|     - |  890 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|   310 |  891 | `static void PcreExpandBackrefs(` |
|     - |  892 | `	SyBlob *pOut,` |
|     - |  893 | `	const char *zRepl, int nReplLen,` |
|     - |  894 | `	const char *zSubject,` |
|     - |  895 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     4 |  896 | `{` |
|   314 |  897 | `	const char *zEnd = &zRepl[nReplLen];` |
|   314 |  898 | `	const char *z = zRepl;` |
|     - |  899 |  |
|  1560 |  900 | `	while( z < zEnd ){` |
|  1250 |  901 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   ! 0 |  902 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|   ! 0 |  903 | `				int g = z[1] - '0';` |
|   ! 0 |  904 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  905 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  906 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  907 | `				}` |
|   ! 0 |  908 | `				z += 2;` |
|   ! 0 |  909 | `				continue;` |
|     - |  910 | `			}` |
|   ! 0 |  911 | `			if( z[1] == '\\' ){` |
|   ! 0 |  912 | `				SyBlobAppend(pOut, "\\", 1);` |
|   ! 0 |  913 | `				z += 2;` |
|   ! 0 |  914 | `				continue;` |
|     - |  915 | `			}` |
|     - |  916 | `			/* Not a backreference — emit literally */` |
|   ! 0 |  917 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  918 | `			z++;` |
|   ! 0 |  919 | `			continue;` |
|     - |  920 | `		}` |
|  1250 |  921 | `		if( *z == '$' && z + 1 < zEnd ){` |
|    92 |  922 | `			if( z[1] == '$' ){` |
|   ! 0 |  923 | `				SyBlobAppend(pOut, "$", 1);` |
|   ! 0 |  924 | `				z += 2;` |
|   ! 0 |  925 | `				continue;` |
|     - |  926 | `			}` |
|    92 |  927 | `			if( z[1] == '{' ){` |
|     - |  928 | `				/* ${N} form */` |
|    39 |  929 | `				const char *p = z + 2;` |
|    39 |  930 | `				int g = 0;` |
|    77 |  931 | `				while( p < zEnd && *p >= '0' && *p <= '9' ){` |
|    39 |  932 | `					g = g * 10 + (*p - '0');` |
|    39 |  933 | `					p++;` |
|     1 |  934 | `				}` |
|    39 |  935 | `				if( p < zEnd && *p == '}' ){` |
|    39 |  936 | `					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|    58 |  937 | `						SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|    38 |  938 | `							(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|    19 |  939 | `					}` |
|    39 |  940 | `					z = p + 1;` |
|    39 |  941 | `					continue;` |
|     - |  942 | `				}` |
|     - |  943 | `				/* Not a valid ${N} — emit literally */` |
|   ! 0 |  944 | `				SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  945 | `				z++;` |
|   ! 0 |  946 | `				continue;` |
|     - |  947 | `			}` |
|    54 |  948 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|     - |  949 | `				/* $N or $NN */` |
|    54 |  950 | `				int g = z[1] - '0';` |
|    54 |  951 | `				z += 2;` |
|     - |  952 | `				/* Check for second digit */` |
|    54 |  953 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  954 | `					int g2 = g * 10 + (*z - '0');` |
|   ! 0 |  955 | `					if( g2 < nGroups ){` |
|   ! 0 |  956 | `						g = g2;` |
|   ! 0 |  957 | `						z++;` |
|   ! 0 |  958 | `					}` |
|   ! 0 |  959 | `				}` |
|    54 |  960 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|    80 |  961 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|    52 |  962 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|    26 |  963 | `				}` |
|    54 |  964 | `				continue;` |
|     - |  965 | `			}` |
|     - |  966 | `			/* Not a backreference */` |
|   ! 0 |  967 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  968 | `			z++;` |
|   ! 0 |  969 | `			continue;` |
|     - |  970 | `		}` |
|  1160 |  971 | `		SyBlobAppend(pOut, z, 1);` |
|  1160 |  972 | `		z++;` |
|     4 |  973 | `	}` |
|   314 |  974 | `}` |
|     - |  975 |  |
|     - |  976 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|   478 |  977 | `static void PcreDoReplace(` |
|     - |  978 | `	ph7_context *pCtx,` |
|     - |  979 | `	pcre2_code *pCode,` |
|     - |  980 | `	const char *zSubject, int nSubLen,` |
|     - |  981 | `	const char *zRepl, int nReplLen,` |
|     - |  982 | `	int limit,` |
|     - |  983 | `	int *pCount,` |
|     - |  984 | `	SyBlob *pOut)` |
|     5 |  985 | `{` |
|     - |  986 | `	pcre2_match_data *pMatchData;` |
|   483 |  987 | `	PCRE2_SIZE startOffset = 0;` |
|   483 |  988 | `	int nReplacements = 0;` |
|     - |  989 | `	int rc;` |
|     - |  990 |  |
|   483 |  991 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|   483 |  992 | `	if( pMatchData == 0 ) return;` |
|     - |  993 |  |
|   793 |  994 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  995 | `		PCRE2_SIZE *ovector;` |
|   791 |  996 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|  1184 |  997 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|   393 |  998 | `			startOffset, 0, pMatchData, NULL);` |
|   791 |  999 | `		if( rc < 0 ){` |
|   481 | 1000 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1001 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1002 | `			}` |
|   481 | 1003 | `			break;` |
|     - | 1004 | `		}` |
|   314 | 1005 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1006 | `		/* Copy text before match */` |
|   314 | 1007 | `		if( ovector[0] > startOffset ){` |
|   212 | 1008 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|   105 | 1009 | `		}` |
|     - | 1010 | `		/* Expand replacement */` |
|   314 | 1011 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|   314 | 1012 | `		nReplacements++;` |
|     - | 1013 | `		/* Advance */` |
|   314 | 1014 | `		if( ovector[1] == ovector[0] ){` |
|     - | 1015 | `			/* Zero-width match: to make progress, emit the character AT THE MATCH` |
|     - | 1016 | `			 * POSITION (ovector[0]) and step past it. The match can sit AHEAD of the` |
|     - | 1017 | `			 * search start (a lookbehind/lookahead assertion, e.g. the camelCase` |
|     - | 1018 | `			 * split /(?<=[[:lower:]])(?=[[:upper:]])/), so copying zSubject[startOffset]` |
|     - | 1019 | `			 * grabbed the wrong byte ("fooBar" -> "foo far"). The text between` |
|     - | 1020 | `			 * startOffset and ovector[0] was already copied above. */` |
|    23 | 1021 | `			if( ovector[0] < (PCRE2_SIZE)nSubLen ){` |
|    21 | 1022 | `				SyBlobAppend(pOut, &zSubject[ovector[0]], 1);` |
|    10 | 1023 | `			}` |
|    23 | 1024 | `			startOffset = ovector[0] + 1;` |
|    12 | 1025 | `		}else{` |
|   292 | 1026 | `			startOffset = ovector[1];` |
|     - | 1027 | `		}` |
|     4 | 1028 | `	}` |
|     - | 1029 | `	/* Copy remainder */` |
|   483 | 1030 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   387 | 1031 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   191 | 1032 | `	}` |
|   483 | 1033 | `	if( pCount ){` |
|   483 | 1034 | `		*pCount += nReplacements;` |
|   239 | 1035 | `	}` |
|   483 | 1036 | `	pcre2_match_data_free(pMatchData);` |
|   239 | 1037 | `	SXUNUSED(pCtx);` |
|   244 | 1038 | `}` |
|     - | 1039 |  |
|     - | 1040 | `/* ===== Helper: apply pattern(s)+replacement(s) to ONE subject string =====` |
|     - | 1041 | ` * pPattern is a string or an array of patterns; pRepl is a string (used for` |
|     - | 1042 | ` * every pattern) or, only when pPattern is an array, an array taken by ORDER` |
|     - | 1043 | ` * (missing element -> ""). Array patterns are applied sequentially, each to the` |
|     - | 1044 | ` * result of the previous (PHP semantics), ping-ponging two blobs. The final` |
|     - | 1045 | ` * text is appended to pOut. Returns SXRET_OK, or SXERR_SYNTAX on a bad pattern` |
|     - | 1046 | ` * (the caller then yields NULL, matching the scalar path). */` |
|   468 | 1047 | `static sxi32 PcreReplaceSubject(` |
|     - | 1048 | `	ph7_context *pCtx,` |
|     - | 1049 | `	ph7_value *pPattern,` |
|     - | 1050 | `	ph7_value *pRepl,` |
|     - | 1051 | `	const char *zSubject, int nSubLen,` |
|     - | 1052 | `	int limit,` |
|     - | 1053 | `	int *pCount,` |
|     - | 1054 | `	SyBlob *pOut)` |
|     5 | 1055 | `{` |
|     - | 1056 | `	sxu32 nCapture;` |
|   473 | 1057 | `	if( !ph7_value_is_array(pPattern) ){` |
|     - | 1058 | `		/* Single pattern + single replacement */` |
|     - | 1059 | `		const char *zPattern, *zRepl;` |
|     - | 1060 | `		int nPatLen, nReplLen;` |
|     - | 1061 | `		pcre2_code *pCode;` |
|   455 | 1062 | `		zPattern = ph7_value_to_string(pPattern, &nPatLen);` |
|   455 | 1063 | `		zRepl = ph7_value_to_string(pRepl, &nReplLen);` |
|   455 | 1064 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|   455 | 1065 | `		if( pCode == 0 ){` |
|     6 | 1066 | `			return SXERR_SYNTAX; /* NOT SXERR_ABORT: that is a real unwind status */` |
|     - | 1067 | `		}` |
|   450 | 1068 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, pCount, pOut);` |
|   450 | 1069 | `		return SXRET_OK;` |
|   ! 0 | 1070 | `	}else{` |
|     - | 1071 | `		/* Array of patterns: apply each in insertion order to the accumulating` |
|     - | 1072 | `		 * subject. Replacement is the parallel array element (by order) or the` |
|     - | 1073 | `		 * scalar replacement for every pattern. */` |
|    20 | 1074 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|    20 | 1075 | `		ph7_hashmap *pRepMap = ph7_value_is_array(pRepl) ? (ph7_hashmap *)pRepl->x.pOther : 0;` |
|    20 | 1076 | `		const char *zScalarRepl = 0;` |
|    20 | 1077 | `		int nScalarRepl = 0;` |
|     - | 1078 | `		ph7_hashmap_node *pPatNode, *pRepNode;` |
|     - | 1079 | `		ph7_value sPat, sRep;` |
|     - | 1080 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|     - | 1081 | `		sxu32 n;` |
|    20 | 1082 | `		sxi32 rc = SXRET_OK;` |
|    20 | 1083 | `		if( pRepMap == 0 ){` |
|     5 | 1084 | `			zScalarRepl = ph7_value_to_string(pRepl, &nScalarRepl);` |
|     2 | 1085 | `		}` |
|    20 | 1086 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|    20 | 1087 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|    20 | 1088 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|    20 | 1089 | `		pSrc = &sA; pDst = &sB;` |
|    20 | 1090 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|    20 | 1091 | `		PH7_MemObjInit(pCtx->pVm, &sRep);` |
|    20 | 1092 | `		pPatNode = pPatMap->pFirst;` |
|    20 | 1093 | `		pRepNode = pRepMap ? pRepMap->pFirst : 0;` |
|    20 | 1094 | `		n = pPatMap->nEntry;` |
|    52 | 1095 | `		while( n > 0 ){` |
|     - | 1096 | `			const char *zPattern, *zRepl;` |
|     - | 1097 | `			int nPatLen, nReplLen;` |
|     - | 1098 | `			pcre2_code *pCode;` |
|     - | 1099 | `			SyBlob *pSwap;` |
|    34 | 1100 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|    34 | 1101 | `			zPattern = ph7_value_to_string(&sPat, &nPatLen);` |
|    34 | 1102 | `			if( pRepMap ){` |
|    30 | 1103 | `				if( pRepNode ){` |
|    28 | 1104 | `					PH7_HashmapExtractNodeValue(pRepNode, &sRep, FALSE);` |
|    28 | 1105 | `					zRepl = ph7_value_to_string(&sRep, &nReplLen);` |
|    15 | 1106 | `				}else{` |
|     3 | 1107 | `					zRepl = ""; nReplLen = 0;` |
|     - | 1108 | `				}` |
|    16 | 1109 | `			}else{` |
|     5 | 1110 | `				zRepl = zScalarRepl; nReplLen = nScalarRepl;` |
|     - | 1111 | `			}` |
|    34 | 1112 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    34 | 1113 | `			if( pCode == 0 ){` |
|   ! 0 | 1114 | `				rc = SXERR_SYNTAX; /* NOT SXERR_ABORT: that is a real unwind status */` |
|   ! 0 | 1115 | `				PH7_MemObjRelease(&sPat);` |
|   ! 0 | 1116 | `				if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|   ! 0 | 1117 | `				break;` |
|     - | 1118 | `			}` |
|    34 | 1119 | `			SyBlobReset(pDst);` |
|    50 | 1120 | `			PcreDoReplace(pCtx, pCode,` |
|    32 | 1121 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|    16 | 1122 | `				zRepl, nReplLen, limit, pCount, pDst);` |
|     - | 1123 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|    34 | 1124 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|    34 | 1125 | `			PH7_MemObjRelease(&sPat);` |
|    34 | 1126 | `			if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|    34 | 1127 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|    34 | 1128 | `			if( pRepNode ){ pRepNode = pRepNode->pPrev; }` |
|    34 | 1129 | `			n--;` |
|     2 | 1130 | `		}` |
|    20 | 1131 | `		if( rc == SXRET_OK ){` |
|    20 | 1132 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|     9 | 1133 | `		}` |
|    20 | 1134 | `		SyBlobRelease(&sA);` |
|    20 | 1135 | `		SyBlobRelease(&sB);` |
|    20 | 1136 | `		return rc;` |
|     - | 1137 | `	}` |
|   239 | 1138 | `}` |
|     - | 1139 |  |
|     - | 1140 | `/* ======================================================================` |
|     - | 1141 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - | 1142 | ` * ====================================================================== */` |
|   452 | 1143 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 | 1144 | `{` |
|   457 | 1145 | `	int limit = -1;` |
|   457 | 1146 | `	int count = 0;` |
|     - | 1147 |  |
|   457 | 1148 | `	if( nArg < 3 ){` |
|   ! 0 | 1149 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1150 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 | 1151 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1152 | `		return PH7_OK;` |
|     - | 1153 | `	}` |
|   457 | 1154 | `	if( nArg >= 4 ){` |
|    42 | 1155 | `		limit = ph7_value_to_int(apArg[3]);` |
|    20 | 1156 | `	}` |
|   457 | 1157 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1158 |  |
|     - | 1159 | `	/* A scalar pattern with an array replacement is a parameter mismatch (PHP` |
|     - | 1160 | `	 * throws a TypeError; PHL keeps preg_replace's warning-based arg-error style). */` |
|   457 | 1161 | `	if( !ph7_value_is_array(apArg[0]) && ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 1162 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1163 | `			"Parameter mismatch, pattern is a string while replacement is an array");` |
|   ! 0 | 1164 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1165 | `		return PH7_OK;` |
|     - | 1166 | `	}` |
|   681 | 1167 | `	if( ph7_value_is_array(apArg[2]) ){` |
|     - | 1168 | `		/* Array subject: return an array, each element replaced, keys preserved. */` |
|    15 | 1169 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|    15 | 1170 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|    15 | 1171 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     - | 1172 | `		ph7_value sKey, sVal;` |
|     - | 1173 | `		ph7_hashmap_node *pNode;` |
|     - | 1174 | `		sxu32 n;` |
|    15 | 1175 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|   ! 0 | 1176 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1177 | `			return PH7_OK;` |
|     - | 1178 | `		}` |
|    15 | 1179 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|    15 | 1180 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    15 | 1181 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|    15 | 1182 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|    45 | 1183 | `		while( n > 0 ){` |
|     - | 1184 | `			const char *zSubject;` |
|     - | 1185 | `			int nSubLen;` |
|     - | 1186 | `			SyBlob sOut;` |
|    31 | 1187 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|    31 | 1188 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|    31 | 1189 | `			zSubject = ph7_value_to_string(&sVal, &nSubLen);` |
|    31 | 1190 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    31 | 1191 | `			if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1192 | `				/* A bad pattern with an array subject yields an empty array (PHP);` |
|     - | 1193 | `				 * the failure hits the first element, so pResult is still empty. */` |
|   ! 0 | 1194 | `				SyBlobRelease(&sOut);` |
|   ! 0 | 1195 | `				PH7_MemObjRelease(&sKey);` |
|   ! 0 | 1196 | `				PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1197 | `				ph7_result_value(pCtx, pResult);` |
|   ! 0 | 1198 | `				goto set_count;` |
|     - | 1199 | `			}` |
|    31 | 1200 | `			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    31 | 1201 | `			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|    31 | 1202 | `			ph7_value_reset_string_cursor(pElem);` |
|    31 | 1203 | `			SyBlobRelease(&sOut);` |
|    31 | 1204 | `			PH7_MemObjRelease(&sKey);` |
|    31 | 1205 | `			PH7_MemObjRelease(&sVal);` |
|    31 | 1206 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|    31 | 1207 | `			n--;` |
|     1 | 1208 | `		}` |
|    15 | 1209 | `		ph7_result_value(pCtx, pResult);` |
|     8 | 1210 | `	}else{` |
|     - | 1211 | `		/* Scalar subject: one replaced string. */` |
|     - | 1212 | `		const char *zSubject;` |
|     - | 1213 | `		int nSubLen;` |
|     - | 1214 | `		SyBlob sOut;` |
|   443 | 1215 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|   443 | 1216 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|   443 | 1217 | `		if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1218 | `			/* Scalar subject: a bad pattern returns NULL (PHP). */` |
|     6 | 1219 | `			SyBlobRelease(&sOut);` |
|     6 | 1220 | `			ph7_result_null(pCtx);` |
|     6 | 1221 | `			goto set_count;` |
|     - | 1222 | `		}` |
|   439 | 1223 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|   439 | 1224 | `		SyBlobRelease(&sOut);` |
|     - | 1225 | `	}` |
|   226 | 1226 | `set_count:` |
|     - | 1227 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|     - | 1228 | `	 * (PHP always writes it: 0, or the count accumulated by earlier good patterns). */` |
|   457 | 1229 | `	if( nArg >= 5 ){` |
|     - | 1230 | `		ph7_value sCount;` |
|    42 | 1231 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|    42 | 1232 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|    42 | 1233 | `		PH7_MemObjRelease(&sCount);` |
|    20 | 1234 | `	}` |
|   457 | 1235 | `	return PH7_OK;` |
|   231 | 1236 | `}` |
|     - | 1237 |  |
|     - | 1238 | `/* ===== Helper: run the callback over ONE compiled pattern on ONE subject =====` |
|     - | 1239 | ` * The mirror of PcreDoReplace() for preg_replace_callback: the replacement text` |
|     - | 1240 | ` * comes from a user callback fed the match array (shaped by $flags) instead of` |
|     - | 1241 | ` * from a template. Appends the whole replaced subject to pOut and adds its own` |
|     - | 1242 | ` * replacement count to *pCount. Returns SXRET_OK, or the dispatch status when` |
|     - | 1243 | ` * the callback did not return (PH7_CALLBACK_UNWOUND) — the caller then unwinds` |
|     - | 1244 | ` * without producing a result. */` |
|    74 | 1245 | `static sxi32 PcreDoCallbackReplace(` |
|     - | 1246 | `	ph7_context *pCtx,` |
|     - | 1247 | `	pcre2_code *pCode,` |
|     - | 1248 | `	const char *zSubject, int nSubLen,` |
|     - | 1249 | `	ph7_value *pCallback,` |
|     - | 1250 | `	int limit,` |
|     - | 1251 | `	int iFlags,` |
|     - | 1252 | `	int *pCount,` |
|     - | 1253 | `	SyBlob *pOut)` |
|     3 | 1254 | `{` |
|     - | 1255 | `	pcre2_match_data *pMatchData;` |
|    77 | 1256 | `	PCRE2_SIZE startOffset = 0;` |
|    77 | 1257 | `	int nReplacements = 0;` |
|     - | 1258 | `	int rc;` |
|     - | 1259 |  |
|    77 | 1260 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    77 | 1261 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1262 | `		return SXRET_OK;` |
|     - | 1263 | `	}` |
|   159 | 1264 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1265 | `		PCRE2_SIZE *ovector;` |
|     - | 1266 | `		ph7_value *pMatchArr;` |
|     - | 1267 | `		ph7_value *apCbArg[1];` |
|     - | 1268 | `		ph7_value sResult;` |
|     - | 1269 | `		const char *zReplacement;` |
|     - | 1270 | `		int nReplLen;` |
|     - | 1271 | `		sxi32 rcCb;` |
|     - | 1272 |  |
|   190 | 1273 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|   231 | 1274 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    76 | 1275 | `			startOffset, 0, pMatchData, NULL);` |
|   155 | 1276 | `		if( rc < 0 ){` |
|    65 | 1277 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1278 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1279 | `			}` |
|    65 | 1280 | `			break;` |
|     - | 1281 | `		}` |
|    93 | 1282 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1283 | `		/* Copy text before match */` |
|    93 | 1284 | `		if( ovector[0] > startOffset ){` |
|    29 | 1285 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|    13 | 1286 | `		}` |
|     - | 1287 | `		/* Build matches array for callback */` |
|    93 | 1288 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    93 | 1289 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, iFlags);` |
|     - | 1290 | `		/* Call the callback */` |
|    93 | 1291 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    93 | 1292 | `		apCbArg[0] = pMatchArr;` |
|    93 | 1293 | `		rcCb = PH7_VmCallCallbackByValue(pCtx->pVm, pCallback, 1, apCbArg, &sResult, 0);` |
|    93 | 1294 | `		if( PH7_CALLBACK_UNWOUND(rcCb) ){` |
|     - | 1295 | `			/* The callback did not return: propagate so the dispatcher unwinds.` |
|     - | 1296 | `			 * An UNCAUGHT throw comes back as PH7_ABORT, and testing only` |
|     - | 1297 | `			 * PH7_EXCEPTION kept the scan going -- re-running the callback, and` |
|     - | 1298 | `			 * re-reporting the fatal, once per remaining match. */` |
|    10 | 1299 | `			PH7_MemObjRelease(&sResult);` |
|    10 | 1300 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|    10 | 1301 | `			pcre2_match_data_free(pMatchData);` |
|    10 | 1302 | `			*pCount += nReplacements;` |
|    10 | 1303 | `			return rcCb;` |
|     - | 1304 | `		}` |
|     - | 1305 | `		/* Get replacement string from callback result */` |
|    85 | 1306 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    85 | 1307 | `		SyBlobAppend(pOut, zReplacement, (sxu32)nReplLen);` |
|    85 | 1308 | `		PH7_MemObjRelease(&sResult);` |
|    85 | 1309 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    85 | 1310 | `		nReplacements++;` |
|     - | 1311 | `		/* Advance */` |
|    85 | 1312 | `		if( ovector[1] == ovector[0] ){` |
|     - | 1313 | `			/* Zero-width match: emit the character AT THE MATCH POSITION and step` |
|     - | 1314 | `			 * past it. The match can sit AHEAD of the search start (a lookaround` |
|     - | 1315 | `			 * assertion, e.g. the camelCase split), so copying zSubject[startOffset]` |
|     - | 1316 | `			 * grabbed the wrong byte ("fooBar" -> "foo far") — the same fix` |
|     - | 1317 | `			 * PcreDoReplace() carries. */` |
|     5 | 1318 | `			if( ovector[0] < (PCRE2_SIZE)nSubLen ){` |
|     5 | 1319 | `				SyBlobAppend(pOut, &zSubject[ovector[0]], 1);` |
|     2 | 1320 | `			}` |
|     5 | 1321 | `			startOffset = ovector[0] + 1;` |
|     3 | 1322 | `		}else{` |
|    81 | 1323 | `			startOffset = ovector[1];` |
|     - | 1324 | `		}` |
|     3 | 1325 | `	}` |
|     - | 1326 | `	/* Copy remainder */` |
|    69 | 1327 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|    32 | 1328 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|    15 | 1329 | `	}` |
|    69 | 1330 | `	*pCount += nReplacements;` |
|    69 | 1331 | `	pcre2_match_data_free(pMatchData);` |
|    69 | 1332 | `	return SXRET_OK;` |
|    40 | 1333 | `}` |
|     - | 1334 |  |
|     - | 1335 | `/* ===== Helper: apply pattern(s)+callback to ONE subject string =====` |
|     - | 1336 | ` * The callback twin of PcreReplaceSubject(): pPattern is a string or an ARRAY of` |
|     - | 1337 | ` * patterns applied sequentially, each to the result of the previous (php` |
|     - | 1338 | ` * semantics), ping-ponging two blobs. Returns SXRET_OK, SXERR_SYNTAX on a bad` |
|     - | 1339 | ` * pattern (the caller then yields NULL / an empty array like the template path),` |
|     - | 1340 | ` * or the dispatch status when the callback did not return. The bad-pattern` |
|     - | 1341 | ` * sentinel must NOT be SXERR_ABORT: that IS the status an exiting or uncaught` |
|     - | 1342 | ` * callback comes back with, and one code for both made a bad pattern kill the` |
|     - | 1343 | ` * script. */` |
|    74 | 1344 | `static sxi32 PcreCallbackReplaceSubject(` |
|     - | 1345 | `	ph7_context *pCtx,` |
|     - | 1346 | `	ph7_value *pPattern,` |
|     - | 1347 | `	ph7_value *pCallback,` |
|     - | 1348 | `	const char *zSubject, int nSubLen,` |
|     - | 1349 | `	int limit,` |
|     - | 1350 | `	int iFlags,` |
|     - | 1351 | `	int *pCount,` |
|     - | 1352 | `	SyBlob *pOut)` |
|     3 | 1353 | `{` |
|     - | 1354 | `	sxu32 nCapture;` |
|    77 | 1355 | `	if( !ph7_value_is_array(pPattern) ){` |
|     - | 1356 | `		const char *zPattern;` |
|     - | 1357 | `		int nPatLen;` |
|     - | 1358 | `		pcre2_code *pCode;` |
|    67 | 1359 | `		zPattern = ph7_value_to_string(pPattern, &nPatLen);` |
|    67 | 1360 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    67 | 1361 | `		if( pCode == 0 ){` |
|     7 | 1362 | `			return SXERR_SYNTAX; /* bad pattern, NOT a callback unwind */` |
|     - | 1363 | `		}` |
|    90 | 1364 | `		return PcreDoCallbackReplace(pCtx, pCode, zSubject, nSubLen, pCallback,` |
|    29 | 1365 | `			limit, iFlags, pCount, pOut);` |
|   ! 0 | 1366 | `	}else{` |
|    12 | 1367 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|     - | 1368 | `		ph7_hashmap_node *pPatNode;` |
|     - | 1369 | `		ph7_value sPat;` |
|     - | 1370 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|     - | 1371 | `		sxu32 n;` |
|    12 | 1372 | `		sxi32 rc = SXRET_OK;` |
|    12 | 1373 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|    12 | 1374 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|    12 | 1375 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|    12 | 1376 | `		pSrc = &sA; pDst = &sB;` |
|    12 | 1377 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|    12 | 1378 | `		pPatNode = pPatMap ? pPatMap->pFirst : 0;` |
|    12 | 1379 | `		n = pPatMap ? pPatMap->nEntry : 0;` |
|    26 | 1380 | `		while( n > 0 ){` |
|     - | 1381 | `			const char *zPattern;` |
|     - | 1382 | `			int nPatLen;` |
|     - | 1383 | `			pcre2_code *pCode;` |
|     - | 1384 | `			SyBlob *pSwap;` |
|    20 | 1385 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|    20 | 1386 | `			zPattern = ph7_value_to_string(&sPat, &nPatLen);` |
|    20 | 1387 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    20 | 1388 | `			if( pCode == 0 ){` |
|     3 | 1389 | `				rc = SXERR_SYNTAX; /* bad pattern, NOT a callback unwind */` |
|     3 | 1390 | `				PH7_MemObjRelease(&sPat);` |
|     4 | 1391 | `				break;` |
|     - | 1392 | `			}` |
|    18 | 1393 | `			SyBlobReset(pDst);` |
|    26 | 1394 | `			rc = PcreDoCallbackReplace(pCtx, pCode,` |
|    16 | 1395 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|     8 | 1396 | `				pCallback, limit, iFlags, pCount, pDst);` |
|     - | 1397 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|    18 | 1398 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|    18 | 1399 | `			PH7_MemObjRelease(&sPat);` |
|    18 | 1400 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|     2 | 1401 | `				break;` |
|     - | 1402 | `			}` |
|    15 | 1403 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|    15 | 1404 | `			n--;` |
|     1 | 1405 | `		}` |
|    12 | 1406 | `		if( rc == SXRET_OK ){` |
|     7 | 1407 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|     3 | 1408 | `		}` |
|    12 | 1409 | `		SyBlobRelease(&sA);` |
|    12 | 1410 | `		SyBlobRelease(&sB);` |
|    12 | 1411 | `		return rc;` |
|     - | 1412 | `	}` |
|    40 | 1413 | `}` |
|     - | 1414 |  |
|     - | 1415 | `/* ======================================================================` |
|     - | 1416 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count [, flags]]])` |
|     - | 1417 | ` * ====================================================================== */` |
|    62 | 1418 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1419 | `{` |
|    65 | 1420 | `	int limit = -1;` |
|    65 | 1421 | `	int iFlags = 0;` |
|    65 | 1422 | `	int count = 0;` |
|     - | 1423 | `	sxi32 rc;` |
|     - | 1424 |  |
|    65 | 1425 | `	if( nArg < 3 ){` |
|   ! 0 | 1426 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1427 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 | 1428 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1429 | `		return PH7_OK;` |
|     - | 1430 | `	}` |
|    65 | 1431 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 | 1432 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1433 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 | 1434 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1435 | `		return PH7_OK;` |
|     - | 1436 | `	}` |
|    65 | 1437 | `	if( nArg >= 4 ){` |
|    40 | 1438 | `		limit = ph7_value_to_int(apArg[3]);` |
|    19 | 1439 | `	}` |
|    65 | 1440 | `	if( nArg >= 6 ){` |
|     - | 1441 | `		/* $flags shapes the match array handed to the callback exactly as it` |
|     - | 1442 | `		 * shapes preg_match()'s &$matches (PREG_OFFSET_CAPTURE /` |
|     - | 1443 | `		 * PREG_UNMATCHED_AS_NULL). php validates nothing here, so neither do we. */` |
|    28 | 1444 | `		iFlags = ph7_value_to_int(apArg[5]);` |
|    13 | 1445 | `	}` |
|    65 | 1446 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1447 |  |
|    88 | 1448 | `	if( ph7_value_is_array(apArg[2]) ){` |
|     - | 1449 | `		/* Array subject: return an array, each element replaced, keys preserved` |
|     - | 1450 | `		 * (php; PHL used to stringify the whole array to "Array" and replace in` |
|     - | 1451 | `		 * THAT — a silent wrong answer). */` |
|    22 | 1452 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|    22 | 1453 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|    22 | 1454 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     - | 1455 | `		ph7_value sKey, sVal;` |
|     - | 1456 | `		ph7_hashmap_node *pNode;` |
|     - | 1457 | `		sxu32 n;` |
|    22 | 1458 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|   ! 0 | 1459 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1460 | `			return PH7_OK;` |
|     - | 1461 | `		}` |
|    22 | 1462 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|    22 | 1463 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    22 | 1464 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|    22 | 1465 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|    46 | 1466 | `		while( n > 0 ){` |
|     - | 1467 | `			const char *zSubject;` |
|     - | 1468 | `			int nSubLen;` |
|     - | 1469 | `			SyBlob sOut;` |
|    34 | 1470 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|    34 | 1471 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|    34 | 1472 | `			zSubject = ph7_value_to_string(&sVal, &nSubLen);` |
|    34 | 1473 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    50 | 1474 | `			rc = PcreCallbackReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen,` |
|    16 | 1475 | `				limit, iFlags, &count, &sOut);` |
|    34 | 1476 | `			if( rc != SXRET_OK ){` |
|     - | 1477 | `				/* A bad pattern with an array subject yields an empty array (php);` |
|     - | 1478 | `				 * the failure hits the first element, so pResult is still empty.` |
|     - | 1479 | `				 * A throwing callback unwinds with no result at all. */` |
|    10 | 1480 | `				SyBlobRelease(&sOut);` |
|    10 | 1481 | `				PH7_MemObjRelease(&sKey);` |
|    10 | 1482 | `				PH7_MemObjRelease(&sVal);` |
|    10 | 1483 | `				if( PH7_CALLBACK_UNWOUND(rc) ){` |
|     6 | 1484 | `					return rc;` |
|     - | 1485 | `				}` |
|     5 | 1486 | `				ph7_result_value(pCtx, pResult);` |
|     5 | 1487 | `				goto set_count;` |
|     - | 1488 | `			}` |
|    25 | 1489 | `			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    25 | 1490 | `			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|    25 | 1491 | `			ph7_value_reset_string_cursor(pElem);` |
|    25 | 1492 | `			SyBlobRelease(&sOut);` |
|    25 | 1493 | `			PH7_MemObjRelease(&sKey);` |
|    25 | 1494 | `			PH7_MemObjRelease(&sVal);` |
|    25 | 1495 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|    25 | 1496 | `			n--;` |
|     1 | 1497 | `		}` |
|    13 | 1498 | `		ph7_result_value(pCtx, pResult);` |
|     7 | 1499 | `	}else{` |
|     - | 1500 | `		/* Scalar subject: one replaced string. */` |
|     - | 1501 | `		const char *zSubject;` |
|     - | 1502 | `		int nSubLen;` |
|     - | 1503 | `		SyBlob sOut;` |
|    45 | 1504 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    45 | 1505 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    66 | 1506 | `		rc = PcreCallbackReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen,` |
|    21 | 1507 | `			limit, iFlags, &count, &sOut);` |
|    45 | 1508 | `		if( rc != SXRET_OK ){` |
|    10 | 1509 | `			SyBlobRelease(&sOut);` |
|    10 | 1510 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|     5 | 1511 | `				return rc;` |
|     - | 1512 | `			}` |
|     - | 1513 | `			/* Scalar subject: a bad pattern returns NULL (php). */` |
|     5 | 1514 | `			ph7_result_null(pCtx);` |
|     5 | 1515 | `			goto set_count;` |
|     - | 1516 | `		}` |
|    37 | 1517 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    37 | 1518 | `		SyBlobRelease(&sOut);` |
|     - | 1519 | `	}` |
|    27 | 1520 | `set_count:` |
|     - | 1521 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|     - | 1522 | `	 * (php always writes it: 0, or the count accumulated by earlier good patterns). */` |
|    57 | 1523 | `	if( nArg >= 5 ){` |
|     - | 1524 | `		ph7_value sCount;` |
|    40 | 1525 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|    40 | 1526 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|    40 | 1527 | `		PH7_MemObjRelease(&sCount);` |
|    19 | 1528 | `	}` |
|    57 | 1529 | `	return PH7_OK;` |
|    34 | 1530 | `}` |
|     - | 1531 |  |
|     - | 1532 | `/* ======================================================================` |
|     - | 1533 | ` * preg_quote(str [, delimiter])` |
|     - | 1534 | ` * ====================================================================== */` |
|    26 | 1535 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1536 | `{` |
|    29 | 1537 | `	const char *zStr, *zDelim = 0;` |
|    29 | 1538 | `	int nLen, nDelimLen = 0;` |
|     - | 1539 | `	const char *z, *zEnd;` |
|     - | 1540 |  |
|    29 | 1541 | `	if( nArg < 1 ){` |
|   ! 0 | 1542 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1543 | `		return PH7_OK;` |
|     - | 1544 | `	}` |
|    29 | 1545 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|    29 | 1546 | `	if( nArg >= 2 ){` |
|    12 | 1547 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     5 | 1548 | `	}` |
|     - | 1549 | `	/* Type the result as a STRING up front: an empty subject quotes to the empty` |
|     - | 1550 | `	 * string, and the loop below would otherwise never touch the result at all,` |
|     - | 1551 | ``	 * leaving php's `string` return as NULL. */`` |
|    29 | 1552 | `	ph7_result_string(pCtx, "", 0);` |
|    29 | 1553 | `	z = zStr;` |
|    29 | 1554 | `	zEnd = &zStr[nLen];` |
|   651 | 1555 | `	while( z < zEnd ){` |
|   625 | 1556 | `		char c = *z;` |
|   625 | 1557 | `		if( c == '\0' ){` |
|     - | 1558 | `			/* php spells NUL as the three-digit escape "\000" (a backslash and a raw NUL` |
|     - | 1559 | `			 * byte, which is what this emitted, is not an escape at all: pcre reads the` |
|     - | 1560 | `			 * backslash as quoting the byte that FOLLOWS the NUL). */` |
|     9 | 1561 | `			ph7_result_string(pCtx, "\\000", 4);` |
|     9 | 1562 | `			z++;` |
|     9 | 1563 | `			continue;` |
|     - | 1564 | `		}` |
|   617 | 1565 | `		switch( c ){` |
|    30 | 1566 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1567 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1568 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1569 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1570 | `			case '#':` |
|    63 | 1571 | `				ph7_result_string(pCtx, "\\", 1);` |
|    63 | 1572 | `				break;` |
|   277 | 1573 | `			default:` |
|   557 | 1574 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     6 | 1575 | `					ph7_result_string(pCtx, "\\", 1);` |
|     2 | 1576 | `				}` |
|   554 | 1577 | `				break;` |
|     - | 1578 | `		}` |
|   617 | 1579 | `		ph7_result_string(pCtx, z, 1);` |
|   617 | 1580 | `		z++;` |
|     3 | 1581 | `	}` |
|    29 | 1582 | `	return PH7_OK;` |
|    16 | 1583 | `}` |
|     - | 1584 |  |
|     - | 1585 | `/* ======================================================================` |
|     - | 1586 | ` * preg_last_error()` |
|     - | 1587 | ` * ====================================================================== */` |
|   ! 0 | 1588 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1589 | `{` |
|   ! 0 | 1590 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1591 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1592 | `	return PH7_OK;` |
|   ! 0 | 1593 | `}` |
|     - | 1594 |  |
|     - | 1595 | `/* ======================================================================` |
|     - | 1596 | ` * preg_last_error_msg()` |
|     - | 1597 | ` * ====================================================================== */` |
|   ! 0 | 1598 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1599 | `{` |
|     - | 1600 | `	const char *zMsg;` |
|   ! 0 | 1601 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1602 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1603 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1604 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1605 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1606 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1607 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1608 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1609 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1610 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1611 | `	}` |
|   ! 0 | 1612 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1613 | `	return PH7_OK;` |
|   ! 0 | 1614 | `}` |
|     - | 1615 |  |
|     - | 1616 | `/*` |
|     - | 1617 | ` * The regex operation behind RegexIterator::accept(), in php's five REGIT modes.` |
|     - | 1618 | ` *` |
|     - | 1619 | ` * php reaches php_pcre_match_impl / php_pcre_split_impl / php_pcre_replace_impl` |
|     - | 1620 | ` * from spl_iterators.c rather than re-deriving any of it, and this is that door:` |
|     - | 1621 | ` * every mode is one of the builtins above, called with the arguments the PHP` |
|     - | 1622 | ` * spelling would have passed. The builtins answer through pCtx->pRet, which is` |
|     - | 1623 | ` * also accept()'s own return slot -- harmless because the caller writes its` |
|     - | 1624 | ` * boolean after this returns, and the reason pOut is a separate parameter.` |
|     - | 1625 | ` *` |
|     - | 1626 | ` * *pbOk is php's per-mode "matched" test: a positive match count, more than one` |
|     - | 1627 | ` * SPLIT piece, at least one REPLACE substitution. pOut receives the transformed` |
|     - | 1628 | ` * value for every mode but MATCH, which leaves the cached current() alone.` |
|     - | 1629 | ` */` |
|    82 | 1630 | `PH7_PRIVATE sxi32 PH7_PcreRegitApply(` |
|     - | 1631 | `	ph7_context *pCtx,` |
|     - | 1632 | `	int iMode,               /* PH7_REGIT_* */` |
|     - | 1633 | `	ph7_value *pPattern,` |
|     - | 1634 | `	ph7_value *pSubject,` |
|     - | 1635 | `	int iPregFlags,` |
|     - | 1636 | `	ph7_value *pRepl,        /* REPLACE only */` |
|     - | 1637 | `	ph7_value *pOut,         /* transformed value (modes other than MATCH) */` |
|     - | 1638 | `	int *pbOk` |
|     - | 1639 | `	)` |
|     1 | 1640 | `{` |
|     - | 1641 | `	ph7_value *apArg[5];` |
|     - | 1642 | `	ph7_value sFlags, sLimit, sCount;` |
|    83 | 1643 | `	sxi32 rc = PH7_OK;` |
|    83 | 1644 | `	*pbOk = 0;` |
|    83 | 1645 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sFlags,iPregFlags);` |
|    83 | 1646 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sLimit,-1);` |
|    83 | 1647 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sCount,0);` |
|     - | 1648 | `	/* A by-ref out-parameter with no caller slot behind it: PH7_VmStoreArgByRef` |
|     - | 1649 | `	 * writes through nIdx when it is not SXU32_HIGH, and a zeroed ph7_value's` |
|     - | 1650 | `	 * nIdx is 0 -- a REAL slot index, which would corrupt aMemObj[0]. */` |
|    83 | 1651 | `	sCount.nIdx = SXU32_HIGH;` |
|    83 | 1652 | `	if( pOut ){` |
|    83 | 1653 | `		pOut->nIdx = SXU32_HIGH;` |
|    41 | 1654 | `	}` |
|    83 | 1655 | `	apArg[0] = pPattern;` |
|    83 | 1656 | `	switch( iMode ){` |
|    18 | 1657 | `		case PH7_REGIT_MATCH:` |
|    37 | 1658 | `			apArg[1] = pSubject;` |
|    37 | 1659 | `			rc = PH7_builtin_preg_match(pCtx,2,apArg);` |
|    37 | 1660 | `			*pbOk = ph7_value_to_int(pCtx->pRet) > 0;` |
|    37 | 1661 | `			break;` |
|    12 | 1662 | `		case PH7_REGIT_GET_MATCH:` |
|     - | 1663 | `		case PH7_REGIT_ALL_MATCHES:` |
|    25 | 1664 | `			apArg[1] = pSubject;` |
|    25 | 1665 | `			apArg[2] = pOut;` |
|    25 | 1666 | `			apArg[3] = &sFlags;` |
|    25 | 1667 | `			rc = iMode == PH7_REGIT_GET_MATCH` |
|    20 | 1668 | `				? PH7_builtin_preg_match(pCtx,4,apArg)` |
|    14 | 1669 | `				: PH7_builtin_preg_match_all(pCtx,4,apArg);` |
|    25 | 1670 | `			*pbOk = ph7_value_to_int(pCtx->pRet) > 0;` |
|    25 | 1671 | `			break;` |
|     3 | 1672 | `		case PH7_REGIT_SPLIT:` |
|     7 | 1673 | `			apArg[1] = pSubject;` |
|     7 | 1674 | `			apArg[2] = &sLimit;` |
|     7 | 1675 | `			apArg[3] = &sFlags;` |
|     7 | 1676 | `			rc = PH7_builtin_preg_split(pCtx,4,apArg);` |
|     7 | 1677 | `			PH7_MemObjStore(pCtx->pRet,pOut);` |
|     7 | 1678 | `			if( pOut->iFlags & MEMOBJ_HASHMAP ){` |
|     7 | 1679 | `				*pbOk = ((ph7_hashmap *)pOut->x.pOther)->nEntry > 1;` |
|     3 | 1680 | `			}` |
|     7 | 1681 | `			break;` |
|     8 | 1682 | `		case PH7_REGIT_REPLACE:` |
|    17 | 1683 | `			apArg[1] = pRepl;` |
|    17 | 1684 | `			apArg[2] = pSubject;` |
|    17 | 1685 | `			apArg[3] = &sLimit;` |
|    17 | 1686 | `			apArg[4] = &sCount;` |
|    17 | 1687 | `			rc = PH7_builtin_preg_replace(pCtx,5,apArg);` |
|    17 | 1688 | `			PH7_MemObjStore(pCtx->pRet,pOut);` |
|    17 | 1689 | `			*pbOk = ph7_value_to_int(&sCount) > 0;` |
|    16 | 1690 | `			break;` |
|   ! 0 | 1691 | `		default:` |
|   ! 0 | 1692 | `			break;` |
|     - | 1693 | `	}` |
|    83 | 1694 | `	PH7_MemObjRelease(&sFlags);` |
|    83 | 1695 | `	PH7_MemObjRelease(&sLimit);` |
|    83 | 1696 | `	PH7_MemObjRelease(&sCount);` |
|    83 | 1697 | `	return rc;` |
|     1 | 1698 | `}` |
|     - | 1699 | `/* ===== Function registration table ===== */` |
|     - | 1700 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1701 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1702 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1703 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1704 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1705 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1706 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1707 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1708 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1709 | `};` |
|     - | 1710 |  |
|  4552 | 1711 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     5 | 1712 | `{` |
|     - | 1713 | `	sxu32 n;` |
| 40973 | 1714 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 36421 | 1715 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 18213 | 1716 | `	}` |
|  4557 | 1717 | `}` |
|     - | 1718 |  |
|     - | 1719 | `/* ===== Constant registration ===== */` |
|     - | 1720 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1721 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1722 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1723 | `	}` |
|     - | 1724 |  |
|    86 | 1725 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|    82 | 1726 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|    92 | 1727 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|   110 | 1728 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|    65 | 1729 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|    65 | 1730 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|    65 | 1731 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|    65 | 1732 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|    65 | 1733 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|    65 | 1734 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|    65 | 1735 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|    78 | 1736 | `PCRE_CONST_INT(PREG_GREP_INVERT,         PHP_PREG_GREP_INVERT)` |
|    65 | 1737 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|    65 | 1738 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|    65 | 1739 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1740 |  |
|  4552 | 1741 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     5 | 1742 | `{` |
|  4557 | 1743 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  4557 | 1744 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  4557 | 1745 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  4557 | 1746 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  4557 | 1747 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  4557 | 1748 | `	ph7_create_constant(&(*pVm), "PREG_GREP_INVERT",          PcreConst_PREG_GREP_INVERT, 0);` |
|  4557 | 1749 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  4557 | 1750 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  4557 | 1751 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  4557 | 1752 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  4557 | 1753 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  4557 | 1754 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  4557 | 1755 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  4557 | 1756 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  4557 | 1757 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  4557 | 1758 | `}` |
|     - | 1759 |  |
|     - | 1760 | `#else` |
|     - | 1761 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1762 | `typedef int vm_pcre_unused;` |
|     - | 1763 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1764 |  |
