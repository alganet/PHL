# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 608/919 lines (66.16%)

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
|   286 |   56 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|     5 |   57 | `{` |
|     - |   58 | `	sxu32 i;` |
|  1871 |   59 | `	for( i = 0; i < nCacheUsed; i++ ){` |
|  1786 |   60 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|   205 |   61 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|   205 |   62 | `			if( pCaptureCount ){` |
|   205 |   63 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|   101 |   64 | `			}` |
|   205 |   65 | `			return aCache[i].pCode;` |
|     - |   66 | `		}` |
|   793 |   67 | `	}` |
|    89 |   68 | `	return 0;` |
|   148 |   69 | `}` |
|     - |   70 |  |
|    84 |   71 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|     5 |   72 | `{` |
|     - |   73 | `	PcreCacheEntry *pEntry;` |
|     - |   74 | `	char *zCopy;` |
|     - |   75 | `	/* Allocate the pattern copy first, before touching the cache */` |
|    89 |   76 | `	zCopy = (char *)malloc(nLen + 1);` |
|    89 |   77 | `	if( zCopy == 0 ){` |
|     - |   78 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|   ! 0 |   79 | `		return;` |
|     - |   80 | `	}` |
|    89 |   81 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|    89 |   82 | `	zCopy[nLen] = 0;` |
|    89 |   83 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|    51 |   84 | `		pEntry = &aCache[nCacheUsed++];` |
|    28 |   85 | `	}else{` |
|     - |   86 | `		/* Evict LRU */` |
|    39 |   87 | `		sxu32 iMin = aCache[0].iLastUsed;` |
|    39 |   88 | `		sxu32 iMinIdx = 0;` |
|     - |   89 | `		sxu32 i;` |
|   609 |   90 | `		for( i = 1; i < PCRE_CACHE_SIZE; i++ ){` |
|   571 |   91 | `			if( aCache[i].iLastUsed < iMin ){` |
|    77 |   92 | `				iMin = aCache[i].iLastUsed;` |
|    77 |   93 | `				iMinIdx = i;` |
|    38 |   94 | `			}` |
|   286 |   95 | `		}` |
|    39 |   96 | `		pEntry = &aCache[iMinIdx];` |
|    39 |   97 | `		pcre2_code_free(pEntry->pCode);` |
|    39 |   98 | `		free(pEntry->zPattern);` |
|     - |   99 | `	}` |
|    89 |  100 | `	pEntry->zPattern = zCopy;` |
|    89 |  101 | `	pEntry->nLen = nLen;` |
|    89 |  102 | `	pEntry->pCode = pCode;` |
|    89 |  103 | `	pEntry->nCaptureCount = nCaptureCount;` |
|    89 |  104 | `	pEntry->iLastUsed = ++iCacheClock;` |
|    47 |  105 | `}` |
|     - |  106 |  |
|     - |  107 | `/* ===== Delimiter parser ===== */` |
|     - |  108 | `#define PCRE_PARSE_OK             0` |
|     - |  109 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|     - |  110 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */` |
|     - |  111 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|     - |  112 |  |
|    84 |  113 | `static sxi32 PcreParsePattern(` |
|     - |  114 | `	const char *zInput, int nInputLen,` |
|     - |  115 | `	const char **pPattern, int *pnPatternLen,` |
|     - |  116 | `	const char **pFlags, int *pnFlagLen)` |
|     5 |  117 | `{` |
|    89 |  118 | `	const char *zEnd = &zInput[nInputLen];` |
|    89 |  119 | `	const char *z = zInput;` |
|     - |  120 | `	char cOpen, cClose;` |
|     - |  121 | `	const char *pStart;` |
|     - |  122 |  |
|     - |  123 | `	/* Skip leading whitespace */` |
|    89 |  124 | `	while( z < zEnd && (unsigned char)*z <= 0x20 ){` |
|   ! 0 |  125 | `		z++;` |
|   ! 0 |  126 | `	}` |
|    89 |  127 | `	if( z >= zEnd ){` |
|   ! 0 |  128 | `		return PCRE_PARSE_EMPTY;` |
|     - |  129 | `	}` |
|    89 |  130 | `	cOpen = *z;` |
|     - |  131 | `	/* Must not be alphanumeric, backslash, or whitespace */` |
|    89 |  132 | `	if( SyisAlphaNum(cOpen) \|\| cOpen == '\\' \|\| (unsigned char)cOpen <= 0x20 ){` |
|   ! 0 |  133 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|     - |  134 | `	}` |
|     - |  135 | `	/* Paired delimiters */` |
|    89 |  136 | `	switch( cOpen ){` |
|   ! 0 |  137 | `		case '(': cClose = ')'; break;` |
|   ! 0 |  138 | `		case '[': cClose = ']'; break;` |
|   ! 0 |  139 | `		case '{': cClose = '}'; break;` |
|   ! 0 |  140 | `		case '<': cClose = '>'; break;` |
|    89 |  141 | `		default:  cClose = cOpen; break;` |
|     - |  142 | `	}` |
|    89 |  143 | `	z++; /* Skip opening delimiter */` |
|    89 |  144 | `	pStart = z;` |
|     - |  145 | `	/* Scan for closing delimiter, respecting backslash escapes */` |
|  1079 |  146 | `	while( z < zEnd ){` |
|  1079 |  147 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|    99 |  148 | `			z += 2; /* Skip escaped char */` |
|    99 |  149 | `			continue;` |
|     - |  150 | `		}` |
|   981 |  151 | `		if( *z == cClose ){` |
|    89 |  152 | `			break;` |
|     - |  153 | `		}` |
|   897 |  154 | `		z++;` |
|     5 |  155 | `	}` |
|    89 |  156 | `	if( z >= zEnd ){` |
|   ! 0 |  157 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|     - |  158 | `	}` |
|    89 |  159 | `	*pPattern = pStart;` |
|    89 |  160 | `	*pnPatternLen = (int)(z - pStart);` |
|    89 |  161 | `	z++; /* Skip closing delimiter */` |
|    89 |  162 | `	*pFlags = z;` |
|    89 |  163 | `	*pnFlagLen = (int)(zEnd - z);` |
|    89 |  164 | `	return PH7_OK;` |
|    47 |  165 | `}` |
|     - |  166 |  |
|     - |  167 | `/* ===== Flag mapper ===== */` |
|    84 |  168 | `static sxi32 PcreMapFlags(` |
|     - |  169 | `	const char *zFlags, int nFlagLen,` |
|     - |  170 | `	uint32_t *pCompileOpts)` |
|     5 |  171 | `{` |
|     - |  172 | `	int i;` |
|    89 |  173 | `	*pCompileOpts = 0;` |
|   105 |  174 | `	for( i = 0; i < nFlagLen; i++ ){` |
|    17 |  175 | `		switch( zFlags[i] ){` |
|    13 |  176 | `			case 'i': *pCompileOpts \|= PCRE2_CASELESS; break;` |
|     3 |  177 | `			case 'm': *pCompileOpts \|= PCRE2_MULTILINE; break;` |
|     3 |  178 | `			case 's': *pCompileOpts \|= PCRE2_DOTALL; break;` |
|   ! 0 |  179 | `			case 'x': *pCompileOpts \|= PCRE2_EXTENDED; break;` |
|   ! 0 |  180 | `			case 'u': *pCompileOpts \|= PCRE2_UTF \| PCRE2_UCP; break;` |
|   ! 0 |  181 | `			case 'A': *pCompileOpts \|= PCRE2_ANCHORED; break;` |
|   ! 0 |  182 | `			case 'D': *pCompileOpts \|= PCRE2_DOLLAR_ENDONLY; break;` |
|   ! 0 |  183 | `			case 'U': *pCompileOpts \|= PCRE2_UNGREEDY; break;` |
|   ! 0 |  184 | `			case 'J': *pCompileOpts \|= PCRE2_DUPNAMES; break;` |
|   ! 0 |  185 | `			case 'S': /* Study hint — no-op in PCRE2 */ break;` |
|   ! 0 |  186 | `			default: break;` |
|     - |  187 | `		}` |
|     9 |  188 | `	}` |
|    89 |  189 | `	return PH7_OK;` |
|     5 |  190 | `}` |
|     - |  191 |  |
|     - |  192 | `/* ===== Compile helper ===== */` |
|   286 |  193 | `static pcre2_code *PcreCompile(` |
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
|   291 |  208 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|   291 |  209 | `	if( pCode ){` |
|   205 |  210 | `		return pCode;` |
|     - |  211 | `	}` |
|     - |  212 | `	/* Parse delimiter */` |
|    89 |  213 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen);` |
|    89 |  214 | `	if( parseRc != PCRE_PARSE_OK ){` |
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
|    89 |  226 | `	PcreMapFlags(zFlags, nFlagLen, &compileOpts);` |
|     - |  227 | `	/* Compile */` |
|    89 |  228 | `	pCode = pcre2_compile(` |
|    42 |  229 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|    42 |  230 | `		compileOpts, &errcode, &erroffset, NULL);` |
|    89 |  231 | `	if( pCode == 0 ){` |
|     - |  232 | `		PCRE2_UCHAR errbuf[256];` |
|   ! 0 |  233 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|   ! 0 |  234 | `		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,` |
|   ! 0 |  235 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|   ! 0 |  236 | `		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|   ! 0 |  237 | `		return 0;` |
|     - |  238 | `	}` |
|     - |  239 | `	/* Get capture count */` |
|    89 |  240 | `	nCapture = 0;` |
|    89 |  241 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|    89 |  242 | `	if( pCaptureCount ){` |
|    89 |  243 | `		*pCaptureCount = nCapture;` |
|    42 |  244 | `	}` |
|     - |  245 | `	/* Cache it */` |
|    89 |  246 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|    89 |  247 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    89 |  248 | `	return pCode;` |
|   148 |  249 | `}` |
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
|   140 |  279 | `static void PcrePopulateMatches(` |
|     - |  280 | `	ph7_context *pCtx,` |
|     - |  281 | `	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */` |
|     - |  282 | `	const char *zSubject,` |
|     - |  283 | `	PCRE2_SIZE *ovector,` |
|     - |  284 | `	int nGroups,` |
|     - |  285 | `	pcre2_code *pCode,` |
|     - |  286 | `	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */` |
|     5 |  287 | `{` |
|   145 |  288 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   145 |  289 | `	ph7_value *pSub = 0;` |
|   145 |  290 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|   145 |  291 | `	PCRE2_SPTR nametable = 0;` |
|     - |  292 | `	int i;` |
|     - |  293 |  |
|   145 |  294 | `	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  295 | `		pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  296 | `	}` |
|   631 |  297 | `	for( i = 0; i < nGroups; i++ ){` |
|   491 |  298 | `		PCRE2_SIZE start = ovector[2 * i];` |
|   491 |  299 | `		PCRE2_SIZE end   = ovector[2 * i + 1];` |
|   491 |  300 | `		if( start == PCRE2_UNSET ){` |
|   131 |  301 | `			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  302 | `				ph7_value_null(pVal);` |
|   ! 0 |  303 | `			}else{` |
|   131 |  304 | `				ph7_value_string(pVal, "", 0);` |
|     - |  305 | `			}` |
|   131 |  306 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  307 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  308 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  309 | `				ph7_value_int(pOff, -1);` |
|   ! 0 |  310 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  311 | `				ph7_array_add_intkey_elem(pArray, i, pSub);` |
|     - |  312 | `				/* Reset sub-array for reuse */` |
|   ! 0 |  313 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  314 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  315 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  316 | `			}else{` |
|   131 |  317 | `				ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  318 | `			}` |
|    66 |  319 | `		}else{` |
|   361 |  320 | `			ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|   361 |  321 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  322 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  323 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  324 | `				ph7_value_int(pOff, (int)start);` |
|   ! 0 |  325 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  326 | `				ph7_array_add_intkey_elem(pArray, i, pSub);` |
|   ! 0 |  327 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  328 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  329 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  330 | `			}else{` |
|   361 |  331 | `				ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  332 | `			}` |
|     - |  333 | `		}` |
|   491 |  334 | `		ph7_value_reset_string_cursor(pVal);` |
|   248 |  335 | `	}` |
|     - |  336 | `	/* Named groups */` |
|   145 |  337 | `	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|   145 |  338 | `	if( namecount > 0 ){` |
|     5 |  339 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     5 |  340 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|    13 |  341 | `		for( i = 0; (uint32_t)i < namecount; i++ ){` |
|     9 |  342 | `			PCRE2_SPTR entry = nametable + i * nameentrysize;` |
|     9 |  343 | `			int groupNum = (entry[0] << 8) \| entry[1];` |
|     9 |  344 | `			const char *zName = (const char *)(entry + 2);` |
|     - |  345 | `			PCRE2_SIZE start, end;` |
|     9 |  346 | `			if( groupNum >= nGroups ) continue;` |
|     9 |  347 | `			start = ovector[2 * groupNum];` |
|     9 |  348 | `			end   = ovector[2 * groupNum + 1];` |
|     9 |  349 | `			if( start == PCRE2_UNSET ){` |
|   ! 0 |  350 | `				if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  351 | `					ph7_value_null(pVal);` |
|   ! 0 |  352 | `				}else{` |
|   ! 0 |  353 | `					ph7_value_string(pVal, "", 0);` |
|     - |  354 | `				}` |
|   ! 0 |  355 | `			}else{` |
|     9 |  356 | `				ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|     - |  357 | `			}` |
|     9 |  358 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  359 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  360 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  361 | `				ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);` |
|   ! 0 |  362 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  363 | `				ph7_array_add_strkey_elem(pArray, zName, pSub);` |
|   ! 0 |  364 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  365 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  366 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  367 | `			}else{` |
|     9 |  368 | `				ph7_array_add_strkey_elem(pArray, zName, pVal);` |
|     - |  369 | `			}` |
|     9 |  370 | `			ph7_value_reset_string_cursor(pVal);` |
|     5 |  371 | `		}` |
|     2 |  372 | `	}` |
|   145 |  373 | `	ph7_context_release_value(pCtx, pVal);` |
|   145 |  374 | `	if( pSub ){` |
|   ! 0 |  375 | `		ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  376 | `	}` |
|   145 |  377 | `}` |
|     - |  378 |  |
|     - |  379 | `/*` |
|     - |  380 | ` * Quiet whole-pattern match used by FILTER_VALIDATE_REGEXP: compile zPat (a full` |
|     - |  381 | ` * "/.../flags" pattern) and test it against zSub. On a successful attempt returns` |
|     - |  382 | ` * SXRET_OK with *pMatched set to 1 (match) or 0 (no match); returns SXERR_INVALID` |
|     - |  383 | ` * on a compile/match error (the caller treats that as a validation failure). The` |
|     - |  384 | ` * compiled code is owned by PcreCompile's cache, so it is not freed here.` |
|     - |  385 | ` */` |
|     4 |  386 | `PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,` |
|     - |  387 | `	const char *zSub,int nSub,int *pMatched)` |
|     1 |  388 | `{` |
|     - |  389 | `	pcre2_code *pCode;` |
|     - |  390 | `	pcre2_match_data *pMatchData;` |
|     - |  391 | `	sxu32 nCapture;` |
|     - |  392 | `	int rc;` |
|     5 |  393 | `	*pMatched = 0;` |
|     5 |  394 | `	pCode = PcreCompile(pCtx,zPat,nPat,&nCapture);` |
|     5 |  395 | `	if( pCode == 0 ){` |
|   ! 0 |  396 | `		return SXERR_INVALID;` |
|     - |  397 | `	}` |
|     5 |  398 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode,NULL);` |
|     5 |  399 | `	if( pMatchData == 0 ){` |
|   ! 0 |  400 | `		return SXERR_INVALID;` |
|     - |  401 | `	}` |
|     5 |  402 | `	rc = pcre2_match(pCode,(PCRE2_SPTR)zSub,(PCRE2_SIZE)nSub,0,0,pMatchData,NULL);` |
|     5 |  403 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  404 | `	if( rc < 0 ){` |
|     3 |  405 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  406 | `			PcreSetMatchError(pCtx->pVm,rc);` |
|   ! 0 |  407 | `			return SXERR_INVALID;` |
|     - |  408 | `		}` |
|     3 |  409 | `		return SXRET_OK; /* clean no-match */` |
|     - |  410 | `	}` |
|     3 |  411 | `	*pMatched = 1;` |
|     3 |  412 | `	return SXRET_OK;` |
|     3 |  413 | `}` |
|     - |  414 | `/* ======================================================================` |
|     - |  415 | ` * preg_match(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  416 | ` * ====================================================================== */` |
|   162 |  417 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  418 | `{` |
|     - |  419 | `	const char *zPattern, *zSubject;` |
|     - |  420 | `	int nPatLen, nSubLen;` |
|     - |  421 | `	pcre2_code *pCode;` |
|     - |  422 | `	pcre2_match_data *pMatchData;` |
|     - |  423 | `	PCRE2_SIZE *ovector;` |
|     - |  424 | `	sxu32 nCapture;` |
|   167 |  425 | `	PCRE2_SIZE startOffset = 0;` |
|   167 |  426 | `	int iFlags = 0;` |
|     - |  427 | `	int rc;` |
|     - |  428 |  |
|   167 |  429 | `	if( nArg < 2 ){` |
|   ! 0 |  430 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  431 | `			"preg_match() expects at least 2 parameters");` |
|   ! 0 |  432 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  433 | `		return PH7_OK;` |
|     - |  434 | `	}` |
|   167 |  435 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|   167 |  436 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|   167 |  437 | `	if( nArg >= 4 ){` |
|     7 |  438 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     3 |  439 | `	}` |
|   167 |  440 | `	if( nArg >= 5 ){` |
|   ! 0 |  441 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  442 | `	}` |
|   167 |  443 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|   167 |  444 | `	if( pCode == 0 ){` |
|   ! 0 |  445 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  446 | `		return PH7_OK;` |
|     - |  447 | `	}` |
|   167 |  448 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|   167 |  449 | `	if( pMatchData == 0 ){` |
|   ! 0 |  450 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  451 | `		return PH7_OK;` |
|     - |  452 | `	}` |
|   248 |  453 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    81 |  454 | `		startOffset, 0, pMatchData, NULL);` |
|   167 |  455 | `	if( rc < 0 ){` |
|    28 |  456 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  457 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  458 | `		}` |
|     - |  459 | `		/* Populate empty matches if requested */` |
|    28 |  460 | `		if( nArg >= 3 ){` |
|    15 |  461 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    15 |  462 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pEmpty);` |
|    15 |  463 | `			ph7_context_release_value(pCtx, pEmpty);` |
|     7 |  464 | `		}` |
|    28 |  465 | `		pcre2_match_data_free(pMatchData);` |
|    28 |  466 | `		ph7_result_int(pCtx, 0);` |
|    28 |  467 | `		return PH7_OK;` |
|     - |  468 | `	}` |
|   141 |  469 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   141 |  470 | `	if( nArg >= 3 ){` |
|     - |  471 | `		/* Populate $matches */` |
|   119 |  472 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|   119 |  473 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|   119 |  474 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);` |
|     - |  475 | `		/* Write the array back to the caller's variable */` |
|   119 |  476 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pArray);` |
|   119 |  477 | `		ph7_context_release_value(pCtx, pArray);` |
|    57 |  478 | `	}` |
|   141 |  479 | `	pcre2_match_data_free(pMatchData);` |
|   141 |  480 | `	ph7_result_int(pCtx, 1);` |
|   141 |  481 | `	return PH7_OK;` |
|    86 |  482 | `}` |
|     - |  483 |  |
|     - |  484 | `/* ======================================================================` |
|     - |  485 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  486 | ` * ====================================================================== */` |
|    28 |  487 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  488 | `{` |
|     - |  489 | `	const char *zPattern, *zSubject;` |
|     - |  490 | `	int nPatLen, nSubLen;` |
|     - |  491 | `	pcre2_code *pCode;` |
|     - |  492 | `	pcre2_match_data *pMatchData;` |
|     - |  493 | `	sxu32 nCapture;` |
|    29 |  494 | `	PCRE2_SIZE startOffset = 0;` |
|    29 |  495 | `	int iFlags = PHP_PREG_PATTERN_ORDER;` |
|    29 |  496 | `	int totalMatches = 0;` |
|     - |  497 | `	int rc;` |
|     - |  498 |  |
|    29 |  499 | `	if( nArg < 2 ){` |
|   ! 0 |  500 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  501 | `			"preg_match_all() expects at least 2 parameters");` |
|   ! 0 |  502 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  503 | `		return PH7_OK;` |
|     - |  504 | `	}` |
|    29 |  505 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    29 |  506 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    29 |  507 | `	if( nArg >= 4 ){` |
|     3 |  508 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     1 |  509 | `	}` |
|    29 |  510 | `	if( nArg >= 5 ){` |
|   ! 0 |  511 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  512 | `	}` |
|    29 |  513 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    29 |  514 | `	if( pCode == 0 ){` |
|   ! 0 |  515 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  516 | `		return PH7_OK;` |
|     - |  517 | `	}` |
|    29 |  518 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    29 |  519 | `	if( pMatchData == 0 ){` |
|   ! 0 |  520 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  521 | `		return PH7_OK;` |
|     - |  522 | `	}` |
|    29 |  523 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  524 | `	{` |
|    29 |  525 | `		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;` |
|     - |  526 |  |
|    29 |  527 | `		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){` |
|     7 |  528 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  529 | `				PCRE2_SIZE *ovector;` |
|    10 |  530 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     3 |  531 | `					startOffset, 0, pMatchData, NULL);` |
|     7 |  532 | `				if( rc < 0 ){` |
|     3 |  533 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     3 |  534 | `					break;` |
|     - |  535 | `				}` |
|     5 |  536 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     5 |  537 | `				if( pOutArray ){` |
|     5 |  538 | `					ph7_value *pSet = ph7_context_new_array(pCtx);` |
|     5 |  539 | `					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, iFlags & ~0xFF);` |
|     5 |  540 | `					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);` |
|     5 |  541 | `					ph7_context_release_value(pCtx, pSet);` |
|     2 |  542 | `				}` |
|     5 |  543 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  544 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  545 | `				}else{` |
|     5 |  546 | `					startOffset = ovector[1];` |
|     - |  547 | `				}` |
|     5 |  548 | `				totalMatches++;` |
|     1 |  549 | `			}` |
|     2 |  550 | `		}else{` |
|     - |  551 | `			/* PREG_PATTERN_ORDER (default) */` |
|    27 |  552 | `			ph7_value **apGroupArrays = 0;` |
|    27 |  553 | `			sxu32 nGroups = nCapture + 1;` |
|     - |  554 | `			sxu32 g;` |
|    27 |  555 | `			if( pOutArray ){` |
|    40 |  556 | `				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,` |
|    13 |  557 | `					sizeof(ph7_value *) * nGroups, TRUE, FALSE);` |
|    27 |  558 | `				if( apGroupArrays ){` |
|    77 |  559 | `					for( g = 0; g < nGroups; g++ ){` |
|    51 |  560 | `						apGroupArrays[g] = ph7_context_new_array(pCtx);` |
|    26 |  561 | `					}` |
|    13 |  562 | `				}` |
|    13 |  563 | `			}` |
|    73 |  564 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  565 | `				PCRE2_SIZE *ovector;` |
|   109 |  566 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    36 |  567 | `					startOffset, 0, pMatchData, NULL);` |
|    73 |  568 | `				if( rc < 0 ){` |
|    27 |  569 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|    27 |  570 | `					break;` |
|     - |  571 | `				}` |
|    47 |  572 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    47 |  573 | `				if( apGroupArrays ){` |
|    47 |  574 | `					ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    47 |  575 | `					int nActual = rc;` |
|   131 |  576 | `					for( g = 0; g < nGroups; g++ ){` |
|   127 |  577 | `						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){` |
|    85 |  578 | `							PCRE2_SIZE s = ovector[2*g];` |
|    85 |  579 | `							PCRE2_SIZE e = ovector[2*g+1];` |
|    85 |  580 | `							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  581 | `								ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  582 | `								ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  583 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|   ! 0 |  584 | `								ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  585 | `								ph7_value_int(pOff, (int)s);` |
|   ! 0 |  586 | `								ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  587 | `								ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|   ! 0 |  588 | `								ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  589 | `								ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  590 | `							}else{` |
|    85 |  591 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|    85 |  592 | `								ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  593 | `							}` |
|    43 |  594 | `						}else{` |
|   ! 0 |  595 | `							ph7_value_string(pVal, "", 0);` |
|   ! 0 |  596 | `							ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  597 | `						}` |
|    85 |  598 | `						ph7_value_reset_string_cursor(pVal);` |
|    43 |  599 | `					}` |
|    47 |  600 | `					ph7_context_release_value(pCtx, pVal);` |
|    23 |  601 | `				}` |
|    47 |  602 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  603 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  604 | `				}else{` |
|    47 |  605 | `					startOffset = ovector[1];` |
|     - |  606 | `				}` |
|    47 |  607 | `				totalMatches++;` |
|     1 |  608 | `			}` |
|    27 |  609 | `			if( apGroupArrays ){` |
|    77 |  610 | `				for( g = 0; g < nGroups; g++ ){` |
|    51 |  611 | `					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);` |
|    51 |  612 | `					ph7_context_release_value(pCtx, apGroupArrays[g]);` |
|    26 |  613 | `				}` |
|    27 |  614 | `				ph7_context_free_chunk(pCtx, apGroupArrays);` |
|    13 |  615 | `			}` |
|     - |  616 | `		}` |
|     - |  617 | `		/* Write output array to caller's variable */` |
|    29 |  618 | `		if( pOutArray && nArg >= 3 ){` |
|    29 |  619 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pOutArray);` |
|    29 |  620 | `			ph7_context_release_value(pCtx, pOutArray);` |
|    14 |  621 | `		}` |
|     - |  622 | `	}` |
|    29 |  623 | `	pcre2_match_data_free(pMatchData);` |
|    29 |  624 | `	ph7_result_int(pCtx, totalMatches);` |
|    29 |  625 | `	return PH7_OK;` |
|    15 |  626 | `}` |
|     - |  627 |  |
|     - |  628 | `/* ======================================================================` |
|     - |  629 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|     - |  630 | ` * ====================================================================== */` |
|     6 |  631 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  632 | `{` |
|     - |  633 | `	const char *zPattern, *zSubject;` |
|     - |  634 | `	int nPatLen, nSubLen;` |
|     - |  635 | `	pcre2_code *pCode;` |
|     - |  636 | `	pcre2_match_data *pMatchData;` |
|     - |  637 | `	sxu32 nCapture;` |
|     - |  638 | `	ph7_value *pArray;` |
|     - |  639 | `	ph7_value *pVal;` |
|     7 |  640 | `	PCRE2_SIZE startOffset = 0, lastOffset = 0;` |
|     7 |  641 | `	int limit = -1;` |
|     7 |  642 | `	int iFlags = 0;` |
|     7 |  643 | `	int nPieces = 0;` |
|     - |  644 | `	int rc;` |
|     - |  645 |  |
|     7 |  646 | `	if( nArg < 2 ){` |
|   ! 0 |  647 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  648 | `			"preg_split() expects at least 2 parameters");` |
|   ! 0 |  649 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  650 | `		return PH7_OK;` |
|     - |  651 | `	}` |
|     7 |  652 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     7 |  653 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     7 |  654 | `	if( nArg >= 3 ){` |
|     5 |  655 | `		limit = ph7_value_to_int(apArg[2]);` |
|     2 |  656 | `	}` |
|     7 |  657 | `	if( nArg >= 4 ){` |
|     3 |  658 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     1 |  659 | `	}` |
|     7 |  660 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     7 |  661 | `	if( pCode == 0 ){` |
|   ! 0 |  662 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  663 | `		return PH7_OK;` |
|     - |  664 | `	}` |
|     7 |  665 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     7 |  666 | `	if( pMatchData == 0 ){` |
|   ! 0 |  667 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  668 | `		return PH7_OK;` |
|     - |  669 | `	}` |
|     7 |  670 | `	pArray = ph7_context_new_array(pCtx);` |
|     7 |  671 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     7 |  672 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  673 |  |
|    19 |  674 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|    19 |  675 | `		if( limit > 0 && nPieces >= limit - 1 ){` |
|     3 |  676 | `			break; /* Last piece gets the remainder */` |
|     - |  677 | `		}` |
|    25 |  678 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     8 |  679 | `			startOffset, 0, pMatchData, NULL);` |
|    17 |  680 | `		if( rc < 0 ){` |
|     5 |  681 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  682 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  683 | `			}` |
|     5 |  684 | `			break;` |
|     - |  685 | `		}` |
|     - |  686 | `		{` |
|    13 |  687 | `			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    13 |  688 | `			PCRE2_SIZE matchStart = ovector[0];` |
|    13 |  689 | `			PCRE2_SIZE matchEnd = ovector[1];` |
|    13 |  690 | `			int pieceLen = (int)(matchStart - lastOffset);` |
|     - |  691 |  |
|     - |  692 | `			/* Add the piece before the match */` |
|    13 |  693 | `			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| pieceLen > 0 ){` |
|    13 |  694 | `				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  695 | `					ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  696 | `					ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  697 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|   ! 0 |  698 | `					ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  699 | `					ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  700 | `					ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  701 | `					ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  702 | `					ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  703 | `					ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  704 | `				}else{` |
|    13 |  705 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|    13 |  706 | `					ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  707 | `				}` |
|    13 |  708 | `				ph7_value_reset_string_cursor(pVal);` |
|    13 |  709 | `				nPieces++;` |
|     6 |  710 | `			}` |
|     - |  711 | `			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */` |
|    13 |  712 | `			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){` |
|     - |  713 | `				int g;` |
|   ! 0 |  714 | `				for( g = 1; g < rc; g++ ){` |
|   ! 0 |  715 | `					PCRE2_SIZE gs = ovector[2*g];` |
|   ! 0 |  716 | `					PCRE2_SIZE ge = ovector[2*g+1];` |
|     - |  717 | `					int gLen;` |
|   ! 0 |  718 | `					if( gs == PCRE2_UNSET ) continue;` |
|   ! 0 |  719 | `					gLen = (int)(ge - gs);` |
|   ! 0 |  720 | `					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| gLen > 0 ){` |
|   ! 0 |  721 | `						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  722 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  723 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  724 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  725 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  726 | `							ph7_value_int(pOff, (int)gs);` |
|   ! 0 |  727 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  728 | `							ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  729 | `							ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  730 | `							ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  731 | `						}else{` |
|   ! 0 |  732 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  733 | `							ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  734 | `						}` |
|   ! 0 |  735 | `						ph7_value_reset_string_cursor(pVal);` |
|   ! 0 |  736 | `					}` |
|   ! 0 |  737 | `				}` |
|   ! 0 |  738 | `			}` |
|     - |  739 | `			/* Advance */` |
|    13 |  740 | `			lastOffset = matchEnd;` |
|    13 |  741 | `			if( matchEnd == matchStart ){` |
|   ! 0 |  742 | `				startOffset = matchEnd + 1;` |
|   ! 0 |  743 | `			}else{` |
|    13 |  744 | `				startOffset = matchEnd;` |
|     - |  745 | `			}` |
|     - |  746 | `		}` |
|     1 |  747 | `	}` |
|     - |  748 | `	/* Add trailing piece */` |
|     - |  749 | `	{` |
|     7 |  750 | `		int trailLen = nSubLen - (int)lastOffset;` |
|     7 |  751 | `		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| trailLen > 0 ){` |
|     7 |  752 | `			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  753 | `				ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  754 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  755 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|   ! 0 |  756 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  757 | `				ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  758 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  759 | `				ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  760 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  761 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  762 | `			}else{` |
|     7 |  763 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|     7 |  764 | `				ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  765 | `			}` |
|     3 |  766 | `		}` |
|     - |  767 | `	}` |
|     7 |  768 | `	ph7_context_release_value(pCtx, pVal);` |
|     7 |  769 | `	pcre2_match_data_free(pMatchData);` |
|     7 |  770 | `	ph7_result_value(pCtx, pArray);` |
|     7 |  771 | `	ph7_context_release_value(pCtx, pArray);` |
|     7 |  772 | `	return PH7_OK;` |
|     4 |  773 | `}` |
|     - |  774 |  |
|     - |  775 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|    98 |  776 | `static void PcreExpandBackrefs(` |
|     - |  777 | `	SyBlob *pOut,` |
|     - |  778 | `	const char *zRepl, int nReplLen,` |
|     - |  779 | `	const char *zSubject,` |
|     - |  780 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     2 |  781 | `{` |
|   100 |  782 | `	const char *zEnd = &zRepl[nReplLen];` |
|   100 |  783 | `	const char *z = zRepl;` |
|     - |  784 |  |
|   214 |  785 | `	while( z < zEnd ){` |
|   116 |  786 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   ! 0 |  787 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|   ! 0 |  788 | `				int g = z[1] - '0';` |
|   ! 0 |  789 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  790 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  791 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  792 | `				}` |
|   ! 0 |  793 | `				z += 2;` |
|   ! 0 |  794 | `				continue;` |
|     - |  795 | `			}` |
|   ! 0 |  796 | `			if( z[1] == '\\' ){` |
|   ! 0 |  797 | `				SyBlobAppend(pOut, "\\", 1);` |
|   ! 0 |  798 | `				z += 2;` |
|   ! 0 |  799 | `				continue;` |
|     - |  800 | `			}` |
|     - |  801 | `			/* Not a backreference — emit literally */` |
|   ! 0 |  802 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  803 | `			z++;` |
|   ! 0 |  804 | `			continue;` |
|     - |  805 | `		}` |
|   116 |  806 | `		if( *z == '$' && z + 1 < zEnd ){` |
|    13 |  807 | `			if( z[1] == '$' ){` |
|   ! 0 |  808 | `				SyBlobAppend(pOut, "$", 1);` |
|   ! 0 |  809 | `				z += 2;` |
|   ! 0 |  810 | `				continue;` |
|     - |  811 | `			}` |
|    13 |  812 | `			if( z[1] == '{' ){` |
|     - |  813 | `				/* ${N} form */` |
|   ! 0 |  814 | `				const char *p = z + 2;` |
|   ! 0 |  815 | `				int g = 0;` |
|   ! 0 |  816 | `				while( p < zEnd && *p >= '0' && *p <= '9' ){` |
|   ! 0 |  817 | `					g = g * 10 + (*p - '0');` |
|   ! 0 |  818 | `					p++;` |
|   ! 0 |  819 | `				}` |
|   ! 0 |  820 | `				if( p < zEnd && *p == '}' ){` |
|   ! 0 |  821 | `					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  822 | `						SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  823 | `							(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  824 | `					}` |
|   ! 0 |  825 | `					z = p + 1;` |
|   ! 0 |  826 | `					continue;` |
|     - |  827 | `				}` |
|     - |  828 | `				/* Not a valid ${N} — emit literally */` |
|   ! 0 |  829 | `				SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  830 | `				z++;` |
|   ! 0 |  831 | `				continue;` |
|     - |  832 | `			}` |
|    13 |  833 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|     - |  834 | `				/* $N or $NN */` |
|    13 |  835 | `				int g = z[1] - '0';` |
|    13 |  836 | `				z += 2;` |
|     - |  837 | `				/* Check for second digit */` |
|    13 |  838 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  839 | `					int g2 = g * 10 + (*z - '0');` |
|   ! 0 |  840 | `					if( g2 < nGroups ){` |
|   ! 0 |  841 | `						g = g2;` |
|   ! 0 |  842 | `						z++;` |
|   ! 0 |  843 | `					}` |
|   ! 0 |  844 | `				}` |
|    13 |  845 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|    19 |  846 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|    12 |  847 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|     6 |  848 | `				}` |
|    13 |  849 | `				continue;` |
|     - |  850 | `			}` |
|     - |  851 | `			/* Not a backreference */` |
|   ! 0 |  852 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  853 | `			z++;` |
|   ! 0 |  854 | `			continue;` |
|     - |  855 | `		}` |
|   104 |  856 | `		SyBlobAppend(pOut, z, 1);` |
|   104 |  857 | `		z++;` |
|     2 |  858 | `	}` |
|   100 |  859 | `}` |
|     - |  860 |  |
|     - |  861 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|    74 |  862 | `static void PcreDoReplace(` |
|     - |  863 | `	ph7_context *pCtx,` |
|     - |  864 | `	pcre2_code *pCode,` |
|     - |  865 | `	const char *zSubject, int nSubLen,` |
|     - |  866 | `	const char *zRepl, int nReplLen,` |
|     - |  867 | `	int limit,` |
|     - |  868 | `	int *pCount,` |
|     - |  869 | `	SyBlob *pOut)` |
|     2 |  870 | `{` |
|     - |  871 | `	pcre2_match_data *pMatchData;` |
|    76 |  872 | `	PCRE2_SIZE startOffset = 0;` |
|    76 |  873 | `	int nReplacements = 0;` |
|     - |  874 | `	int rc;` |
|     - |  875 |  |
|    76 |  876 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    76 |  877 | `	if( pMatchData == 0 ) return;` |
|     - |  878 |  |
|   174 |  879 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  880 | `		PCRE2_SIZE *ovector;` |
|   174 |  881 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|   260 |  882 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    86 |  883 | `			startOffset, 0, pMatchData, NULL);` |
|   174 |  884 | `		if( rc < 0 ){` |
|    76 |  885 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  886 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  887 | `			}` |
|    76 |  888 | `			break;` |
|     - |  889 | `		}` |
|   100 |  890 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - |  891 | `		/* Copy text before match */` |
|   100 |  892 | `		if( ovector[0] > startOffset ){` |
|    67 |  893 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|    33 |  894 | `		}` |
|     - |  895 | `		/* Expand replacement */` |
|   100 |  896 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|   100 |  897 | `		nReplacements++;` |
|     - |  898 | `		/* Advance */` |
|   100 |  899 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 |  900 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 |  901 | `				SyBlobAppend(pOut, &zSubject[startOffset], 1);` |
|   ! 0 |  902 | `			}` |
|   ! 0 |  903 | `			startOffset = ovector[0] + 1;` |
|   ! 0 |  904 | `		}else{` |
|   100 |  905 | `			startOffset = ovector[1];` |
|     - |  906 | `		}` |
|     2 |  907 | `	}` |
|     - |  908 | `	/* Copy remainder */` |
|    76 |  909 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|    26 |  910 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|    12 |  911 | `	}` |
|    76 |  912 | `	if( pCount ){` |
|    76 |  913 | `		*pCount += nReplacements;` |
|    37 |  914 | `	}` |
|    76 |  915 | `	pcre2_match_data_free(pMatchData);` |
|    37 |  916 | `	SXUNUSED(pCtx);` |
|    39 |  917 | `}` |
|     - |  918 |  |
|     - |  919 | `/* ===== Helper: apply pattern(s)+replacement(s) to ONE subject string =====` |
|     - |  920 | ` * pPattern is a string or an array of patterns; pRepl is a string (used for` |
|     - |  921 | ` * every pattern) or, only when pPattern is an array, an array taken by ORDER` |
|     - |  922 | ` * (missing element -> ""). Array patterns are applied sequentially, each to the` |
|     - |  923 | ` * result of the previous (PHP semantics), ping-ponging two blobs. The final` |
|     - |  924 | ` * text is appended to pOut. Returns SXRET_OK, or SXERR_ABORT on a bad pattern` |
|     - |  925 | ` * (the caller then yields NULL, matching the scalar path). */` |
|    66 |  926 | `static sxi32 PcreReplaceSubject(` |
|     - |  927 | `	ph7_context *pCtx,` |
|     - |  928 | `	ph7_value *pPattern,` |
|     - |  929 | `	ph7_value *pRepl,` |
|     - |  930 | `	const char *zSubject, int nSubLen,` |
|     - |  931 | `	int limit,` |
|     - |  932 | `	int *pCount,` |
|     - |  933 | `	SyBlob *pOut)` |
|     2 |  934 | `{` |
|     - |  935 | `	sxu32 nCapture;` |
|    68 |  936 | `	if( !ph7_value_is_array(pPattern) ){` |
|     - |  937 | `		/* Single pattern + single replacement */` |
|     - |  938 | `		const char *zPattern, *zRepl;` |
|     - |  939 | `		int nPatLen, nReplLen;` |
|     - |  940 | `		pcre2_code *pCode;` |
|    56 |  941 | `		zPattern = ph7_value_to_string(pPattern, &nPatLen);` |
|    56 |  942 | `		zRepl = ph7_value_to_string(pRepl, &nReplLen);` |
|    56 |  943 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    56 |  944 | `		if( pCode == 0 ){` |
|   ! 0 |  945 | `			return SXERR_ABORT;` |
|     - |  946 | `		}` |
|    56 |  947 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, pCount, pOut);` |
|    56 |  948 | `		return SXRET_OK;` |
|   ! 0 |  949 | `	}else{` |
|     - |  950 | `		/* Array of patterns: apply each in insertion order to the accumulating` |
|     - |  951 | `		 * subject. Replacement is the parallel array element (by order) or the` |
|     - |  952 | `		 * scalar replacement for every pattern. */` |
|    13 |  953 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|    13 |  954 | `		ph7_hashmap *pRepMap = ph7_value_is_array(pRepl) ? (ph7_hashmap *)pRepl->x.pOther : 0;` |
|    13 |  955 | `		const char *zScalarRepl = 0;` |
|    13 |  956 | `		int nScalarRepl = 0;` |
|     - |  957 | `		ph7_hashmap_node *pPatNode, *pRepNode;` |
|     - |  958 | `		ph7_value sPat, sRep;` |
|     - |  959 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|     - |  960 | `		sxu32 n;` |
|    13 |  961 | `		sxi32 rc = SXRET_OK;` |
|    13 |  962 | `		if( pRepMap == 0 ){` |
|     5 |  963 | `			zScalarRepl = ph7_value_to_string(pRepl, &nScalarRepl);` |
|     2 |  964 | `		}` |
|    13 |  965 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|    13 |  966 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|    13 |  967 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|    13 |  968 | `		pSrc = &sA; pDst = &sB;` |
|    13 |  969 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|    13 |  970 | `		PH7_MemObjInit(pCtx->pVm, &sRep);` |
|    13 |  971 | `		pPatNode = pPatMap->pFirst;` |
|    13 |  972 | `		pRepNode = pRepMap ? pRepMap->pFirst : 0;` |
|    13 |  973 | `		n = pPatMap->nEntry;` |
|    33 |  974 | `		while( n > 0 ){` |
|     - |  975 | `			const char *zPattern, *zRepl;` |
|     - |  976 | `			int nPatLen, nReplLen;` |
|     - |  977 | `			pcre2_code *pCode;` |
|     - |  978 | `			SyBlob *pSwap;` |
|    21 |  979 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|    21 |  980 | `			zPattern = ph7_value_to_string(&sPat, &nPatLen);` |
|    21 |  981 | `			if( pRepMap ){` |
|    17 |  982 | `				if( pRepNode ){` |
|    15 |  983 | `					PH7_HashmapExtractNodeValue(pRepNode, &sRep, FALSE);` |
|    15 |  984 | `					zRepl = ph7_value_to_string(&sRep, &nReplLen);` |
|     8 |  985 | `				}else{` |
|     3 |  986 | `					zRepl = ""; nReplLen = 0;` |
|     - |  987 | `				}` |
|     9 |  988 | `			}else{` |
|     5 |  989 | `				zRepl = zScalarRepl; nReplLen = nScalarRepl;` |
|     - |  990 | `			}` |
|    21 |  991 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    21 |  992 | `			if( pCode == 0 ){` |
|   ! 0 |  993 | `				rc = SXERR_ABORT;` |
|   ! 0 |  994 | `				PH7_MemObjRelease(&sPat);` |
|   ! 0 |  995 | `				if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|   ! 0 |  996 | `				break;` |
|     - |  997 | `			}` |
|    21 |  998 | `			SyBlobReset(pDst);` |
|    31 |  999 | `			PcreDoReplace(pCtx, pCode,` |
|    20 | 1000 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|    10 | 1001 | `				zRepl, nReplLen, limit, pCount, pDst);` |
|     - | 1002 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|    21 | 1003 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|    21 | 1004 | `			PH7_MemObjRelease(&sPat);` |
|    21 | 1005 | `			if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|    21 | 1006 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|    21 | 1007 | `			if( pRepNode ){ pRepNode = pRepNode->pPrev; }` |
|    21 | 1008 | `			n--;` |
|     1 | 1009 | `		}` |
|    13 | 1010 | `		if( rc == SXRET_OK ){` |
|    13 | 1011 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|     6 | 1012 | `		}` |
|    13 | 1013 | `		SyBlobRelease(&sA);` |
|    13 | 1014 | `		SyBlobRelease(&sB);` |
|    13 | 1015 | `		return rc;` |
|     - | 1016 | `	}` |
|    35 | 1017 | `}` |
|     - | 1018 |  |
|     - | 1019 | `/* ======================================================================` |
|     - | 1020 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - | 1021 | ` * ====================================================================== */` |
|    50 | 1022 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1023 | `{` |
|    52 | 1024 | `	int limit = -1;` |
|    52 | 1025 | `	int count = 0;` |
|     - | 1026 |  |
|    52 | 1027 | `	if( nArg < 3 ){` |
|   ! 0 | 1028 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1029 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 | 1030 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1031 | `		return PH7_OK;` |
|     - | 1032 | `	}` |
|    52 | 1033 | `	if( nArg >= 4 ){` |
|    20 | 1034 | `		limit = ph7_value_to_int(apArg[3]);` |
|     9 | 1035 | `	}` |
|    52 | 1036 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1037 |  |
|     - | 1038 | `	/* A scalar pattern with an array replacement is a parameter mismatch (PHP` |
|     - | 1039 | `	 * throws a TypeError; PHL keeps preg_replace's warning-based arg-error style). */` |
|    52 | 1040 | `	if( !ph7_value_is_array(apArg[0]) && ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 1041 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1042 | `			"preg_replace(): Parameter mismatch, pattern is a string while replacement is an array");` |
|   ! 0 | 1043 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1044 | `		return PH7_OK;` |
|     - | 1045 | `	}` |
|    77 | 1046 | `	if( ph7_value_is_array(apArg[2]) ){` |
|     - | 1047 | `		/* Array subject: return an array, each element replaced, keys preserved. */` |
|    15 | 1048 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|    15 | 1049 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|    15 | 1050 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     - | 1051 | `		ph7_value sKey, sVal;` |
|     - | 1052 | `		ph7_hashmap_node *pNode;` |
|     - | 1053 | `		sxu32 n;` |
|    15 | 1054 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|   ! 0 | 1055 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1056 | `			return PH7_OK;` |
|     - | 1057 | `		}` |
|    15 | 1058 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|    15 | 1059 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    15 | 1060 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|    15 | 1061 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|    45 | 1062 | `		while( n > 0 ){` |
|     - | 1063 | `			const char *zSubject;` |
|     - | 1064 | `			int nSubLen;` |
|     - | 1065 | `			SyBlob sOut;` |
|    31 | 1066 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|    31 | 1067 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|    31 | 1068 | `			zSubject = ph7_value_to_string(&sVal, &nSubLen);` |
|    31 | 1069 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    31 | 1070 | `			if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1071 | `				/* A bad pattern with an array subject yields an empty array (PHP);` |
|     - | 1072 | `				 * the failure hits the first element, so pResult is still empty. */` |
|   ! 0 | 1073 | `				SyBlobRelease(&sOut);` |
|   ! 0 | 1074 | `				PH7_MemObjRelease(&sKey);` |
|   ! 0 | 1075 | `				PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1076 | `				ph7_result_value(pCtx, pResult);` |
|   ! 0 | 1077 | `				goto set_count;` |
|     - | 1078 | `			}` |
|    31 | 1079 | `			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    31 | 1080 | `			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|    31 | 1081 | `			ph7_value_reset_string_cursor(pElem);` |
|    31 | 1082 | `			SyBlobRelease(&sOut);` |
|    31 | 1083 | `			PH7_MemObjRelease(&sKey);` |
|    31 | 1084 | `			PH7_MemObjRelease(&sVal);` |
|    31 | 1085 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|    31 | 1086 | `			n--;` |
|     1 | 1087 | `		}` |
|    15 | 1088 | `		ph7_result_value(pCtx, pResult);` |
|     8 | 1089 | `	}else{` |
|     - | 1090 | `		/* Scalar subject: one replaced string. */` |
|     - | 1091 | `		const char *zSubject;` |
|     - | 1092 | `		int nSubLen;` |
|     - | 1093 | `		SyBlob sOut;` |
|    38 | 1094 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    38 | 1095 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    38 | 1096 | `		if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1097 | `			/* Scalar subject: a bad pattern returns NULL (PHP). */` |
|   ! 0 | 1098 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1099 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1100 | `			goto set_count;` |
|     - | 1101 | `		}` |
|    38 | 1102 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    38 | 1103 | `		SyBlobRelease(&sOut);` |
|     - | 1104 | `	}` |
|    25 | 1105 | `set_count:` |
|     - | 1106 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|     - | 1107 | `	 * (PHP always writes it: 0, or the count accumulated by earlier good patterns). */` |
|    52 | 1108 | `	if( nArg >= 5 ){` |
|     - | 1109 | `		ph7_value sCount;` |
|    20 | 1110 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|    20 | 1111 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|    20 | 1112 | `		PH7_MemObjRelease(&sCount);` |
|     9 | 1113 | `	}` |
|    52 | 1114 | `	return PH7_OK;` |
|    27 | 1115 | `}` |
|     - | 1116 |  |
|     - | 1117 | `/* ======================================================================` |
|     - | 1118 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count]])` |
|     - | 1119 | ` * ====================================================================== */` |
|    12 | 1120 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1121 | `{` |
|     - | 1122 | `	const char *zPattern, *zSubject;` |
|     - | 1123 | `	int nPatLen, nSubLen;` |
|     - | 1124 | `	pcre2_code *pCode;` |
|     - | 1125 | `	pcre2_match_data *pMatchData;` |
|     - | 1126 | `	sxu32 nCapture;` |
|     - | 1127 | `	SyBlob sOut;` |
|    15 | 1128 | `	PCRE2_SIZE startOffset = 0;` |
|    15 | 1129 | `	int limit = -1;` |
|    15 | 1130 | `	int count = 0;` |
|     - | 1131 | `	int rc;` |
|     - | 1132 |  |
|    15 | 1133 | `	if( nArg < 3 ){` |
|   ! 0 | 1134 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1135 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 | 1136 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1137 | `		return PH7_OK;` |
|     - | 1138 | `	}` |
|    15 | 1139 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    15 | 1140 | `	zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    15 | 1141 | `	if( nArg >= 4 ){` |
|     8 | 1142 | `		limit = ph7_value_to_int(apArg[3]);` |
|     3 | 1143 | `	}` |
|    15 | 1144 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 | 1145 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1146 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 | 1147 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1148 | `		return PH7_OK;` |
|     - | 1149 | `	}` |
|    15 | 1150 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    15 | 1151 | `	if( pCode == 0 ){` |
|   ! 0 | 1152 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1153 | `		return PH7_OK;` |
|     - | 1154 | `	}` |
|    15 | 1155 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    15 | 1156 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1157 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1158 | `		return PH7_OK;` |
|     - | 1159 | `	}` |
|    15 | 1160 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    15 | 1161 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1162 |  |
|    37 | 1163 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1164 | `		PCRE2_SIZE *ovector;` |
|     - | 1165 | `		ph7_value *pMatchArr;` |
|     - | 1166 | `		ph7_value *apCbArg[1];` |
|     - | 1167 | `		ph7_value sResult;` |
|     - | 1168 | `		const char *zReplacement;` |
|     - | 1169 | `		int nReplLen;` |
|     - | 1170 |  |
|    43 | 1171 | `		if( limit >= 0 && count >= limit ) break;` |
|    54 | 1172 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    17 | 1173 | `			startOffset, 0, pMatchData, NULL);` |
|    37 | 1174 | `		if( rc < 0 ){` |
|    15 | 1175 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1176 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1177 | `			}` |
|    15 | 1178 | `			break;` |
|     - | 1179 | `		}` |
|    25 | 1180 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1181 | `		/* Copy text before match */` |
|    25 | 1182 | `		if( ovector[0] > startOffset ){` |
|    17 | 1183 | `			SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     7 | 1184 | `		}` |
|     - | 1185 | `		/* Build matches array for callback */` |
|    25 | 1186 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    25 | 1187 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, 0);` |
|     - | 1188 | `		/* Call the callback */` |
|    25 | 1189 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    25 | 1190 | `		apCbArg[0] = pMatchArr;` |
|    25 | 1191 | `		if( PH7_VmCallUserFunction(pCtx->pVm, apArg[1], 1, apCbArg, &sResult) == PH7_EXCEPTION ){` |
|     - | 1192 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|   ! 0 | 1193 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1194 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|   ! 0 | 1195 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1196 | `			pcre2_match_data_free(pMatchData);` |
|   ! 0 | 1197 | `			return PH7_EXCEPTION;` |
|     - | 1198 | `		}` |
|     - | 1199 | `		/* Get replacement string from callback result */` |
|    25 | 1200 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    25 | 1201 | `		SyBlobAppend(&sOut, zReplacement, (sxu32)nReplLen);` |
|    25 | 1202 | `		PH7_MemObjRelease(&sResult);` |
|    25 | 1203 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    25 | 1204 | `		count++;` |
|     - | 1205 | `		/* Advance */` |
|    25 | 1206 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 | 1207 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1208 | `				SyBlobAppend(&sOut, &zSubject[startOffset], 1);` |
|   ! 0 | 1209 | `			}` |
|   ! 0 | 1210 | `			startOffset = ovector[0] + 1;` |
|   ! 0 | 1211 | `		}else{` |
|    25 | 1212 | `			startOffset = ovector[1];` |
|     - | 1213 | `		}` |
|     3 | 1214 | `	}` |
|     - | 1215 | `	/* Copy remainder */` |
|    15 | 1216 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|     5 | 1217 | `		SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|     2 | 1218 | `	}` |
|    15 | 1219 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    15 | 1220 | `	SyBlobRelease(&sOut);` |
|    15 | 1221 | `	pcre2_match_data_free(pMatchData);` |
|     - | 1222 | `	/* Set &$count if provided */` |
|    15 | 1223 | `	if( nArg >= 5 ){` |
|     - | 1224 | `		ph7_value sCount;` |
|     3 | 1225 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     3 | 1226 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|     3 | 1227 | `		PH7_MemObjRelease(&sCount);` |
|     1 | 1228 | `	}` |
|    15 | 1229 | `	return PH7_OK;` |
|     9 | 1230 | `}` |
|     - | 1231 |  |
|     - | 1232 | `/* ======================================================================` |
|     - | 1233 | ` * preg_quote(str [, delimiter])` |
|     - | 1234 | ` * ====================================================================== */` |
|     6 | 1235 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1236 | `{` |
|     7 | 1237 | `	const char *zStr, *zDelim = 0;` |
|     7 | 1238 | `	int nLen, nDelimLen = 0;` |
|     - | 1239 | `	const char *z, *zEnd;` |
|     - | 1240 |  |
|     7 | 1241 | `	if( nArg < 1 ){` |
|   ! 0 | 1242 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1243 | `		return PH7_OK;` |
|     - | 1244 | `	}` |
|     7 | 1245 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|     7 | 1246 | `	if( nArg >= 2 ){` |
|     3 | 1247 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     1 | 1248 | `	}` |
|     7 | 1249 | `	z = zStr;` |
|     7 | 1250 | `	zEnd = &zStr[nLen];` |
|    71 | 1251 | `	while( z < zEnd ){` |
|    65 | 1252 | `		char c = *z;` |
|    65 | 1253 | `		switch( c ){` |
|     4 | 1254 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1255 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1256 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1257 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1258 | `			case '#':` |
|     9 | 1259 | `				ph7_result_string(pCtx, "\\", 1);` |
|     9 | 1260 | `				break;` |
|    28 | 1261 | `			default:` |
|    57 | 1262 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     3 | 1263 | `					ph7_result_string(pCtx, "\\", 1);` |
|     1 | 1264 | `				}` |
|    56 | 1265 | `				break;` |
|     - | 1266 | `		}` |
|    65 | 1267 | `		ph7_result_string(pCtx, z, 1);` |
|    65 | 1268 | `		z++;` |
|     1 | 1269 | `	}` |
|     7 | 1270 | `	return PH7_OK;` |
|     4 | 1271 | `}` |
|     - | 1272 |  |
|     - | 1273 | `/* ======================================================================` |
|     - | 1274 | ` * preg_last_error()` |
|     - | 1275 | ` * ====================================================================== */` |
|   ! 0 | 1276 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1277 | `{` |
|   ! 0 | 1278 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1279 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1280 | `	return PH7_OK;` |
|   ! 0 | 1281 | `}` |
|     - | 1282 |  |
|     - | 1283 | `/* ======================================================================` |
|     - | 1284 | ` * preg_last_error_msg()` |
|     - | 1285 | ` * ====================================================================== */` |
|   ! 0 | 1286 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1287 | `{` |
|     - | 1288 | `	const char *zMsg;` |
|   ! 0 | 1289 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1290 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1291 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1292 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1293 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1294 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1295 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1296 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1297 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1298 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1299 | `	}` |
|   ! 0 | 1300 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1301 | `	return PH7_OK;` |
|   ! 0 | 1302 | `}` |
|     - | 1303 |  |
|     - | 1304 | `/* ===== Function registration table ===== */` |
|     - | 1305 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1306 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1307 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1308 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1309 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1310 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1311 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1312 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1313 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1314 | `};` |
|     - | 1315 |  |
|  3364 | 1316 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     5 | 1317 | `{` |
|     - | 1318 | `	sxu32 n;` |
| 30281 | 1319 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 26917 | 1320 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 13461 | 1321 | `	}` |
|  3369 | 1322 | `}` |
|     - | 1323 |  |
|     - | 1324 | `/* ===== Constant registration ===== */` |
|     - | 1325 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1326 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1327 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1328 | `	}` |
|     - | 1329 |  |
|   ! 0 | 1330 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|     3 | 1331 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|   ! 0 | 1332 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|   ! 0 | 1333 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|   ! 0 | 1334 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|   ! 0 | 1335 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|   ! 0 | 1336 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|   ! 0 | 1337 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|   ! 0 | 1338 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|   ! 0 | 1339 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|   ! 0 | 1340 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|    13 | 1341 | `PCRE_CONST_INT(PREG_GREP_INVERT,         PHP_PREG_GREP_INVERT)` |
|   ! 0 | 1342 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|   ! 0 | 1343 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|   ! 0 | 1344 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1345 |  |
|  3364 | 1346 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     5 | 1347 | `{` |
|  3369 | 1348 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  3369 | 1349 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  3369 | 1350 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  3369 | 1351 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  3369 | 1352 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  3369 | 1353 | `	ph7_create_constant(&(*pVm), "PREG_GREP_INVERT",          PcreConst_PREG_GREP_INVERT, 0);` |
|  3369 | 1354 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  3369 | 1355 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  3369 | 1356 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  3369 | 1357 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  3369 | 1358 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  3369 | 1359 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  3369 | 1360 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  3369 | 1361 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  3369 | 1362 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  3369 | 1363 | `}` |
|     - | 1364 |  |
|     - | 1365 | `#else` |
|     - | 1366 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1367 | `typedef int vm_pcre_unused;` |
|     - | 1368 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1369 |  |
