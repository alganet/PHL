# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 595/917 lines (64.89%)

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
|     - |  250 | `/* ===== Map PCRE2 match error to PHP error code ===== */` |
|   ! 0 |  251 | `static void PcreSetMatchError(ph7_vm *pVm, int rc)` |
|   ! 0 |  252 | `{` |
|   ! 0 |  253 | `	if( rc == PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  254 | `		pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   ! 0 |  255 | `	}else if( rc == PCRE2_ERROR_MATCHLIMIT ){` |
|   ! 0 |  256 | `		pVm->iPcreLastError = PHP_PREG_BACKTRACK_LIMIT_ERROR;` |
|   ! 0 |  257 | `	}else if( rc == PCRE2_ERROR_DEPTHLIMIT` |
|     - |  258 | `#ifdef PCRE2_ERROR_RECURSIONLIMIT` |
|   ! 0 |  259 | `		\|\| rc == PCRE2_ERROR_RECURSIONLIMIT` |
|     - |  260 | `#endif` |
|     - |  261 | `	){` |
|   ! 0 |  262 | `		pVm->iPcreLastError = PHP_PREG_RECURSION_LIMIT_ERROR;` |
|   ! 0 |  263 | `	}else if( rc == PCRE2_ERROR_BADUTFOFFSET ){` |
|   ! 0 |  264 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_OFFSET_ERROR;` |
|   ! 0 |  265 | `	}else if( rc == PCRE2_ERROR_UTF8_ERR1` |
|   ! 0 |  266 | `		\|\| rc == PCRE2_ERROR_UTF8_ERR2 ){` |
|   ! 0 |  267 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_ERROR;` |
|     - |  268 | `#ifdef PCRE2_ERROR_JIT_STACKLIMIT` |
|   ! 0 |  269 | `	}else if( rc == PCRE2_ERROR_JIT_STACKLIMIT ){` |
|   ! 0 |  270 | `		pVm->iPcreLastError = PHP_PREG_JIT_STACKLIMIT_ERROR;` |
|     - |  271 | `#endif` |
|   ! 0 |  272 | `	}else{` |
|   ! 0 |  273 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|     - |  274 | `	}` |
|   ! 0 |  275 | `}` |
|     - |  276 |  |
|     - |  277 | `/* ===== Helper: populate matches array from ovector ===== */` |
|    48 |  278 | `static void PcrePopulateMatches(` |
|     - |  279 | `	ph7_context *pCtx,` |
|     - |  280 | `	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */` |
|     - |  281 | `	const char *zSubject,` |
|     - |  282 | `	PCRE2_SIZE *ovector,` |
|     - |  283 | `	int nGroups,` |
|     - |  284 | `	pcre2_code *pCode,` |
|     - |  285 | `	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */` |
|     5 |  286 | `{` |
|    53 |  287 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    53 |  288 | `	ph7_value *pSub = 0;` |
|    53 |  289 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|    53 |  290 | `	PCRE2_SPTR nametable = 0;` |
|     - |  291 | `	int i;` |
|     - |  292 |  |
|    53 |  293 | `	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  294 | `		pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  295 | `	}` |
|   149 |  296 | `	for( i = 0; i < nGroups; i++ ){` |
|   101 |  297 | `		PCRE2_SIZE start = ovector[2 * i];` |
|   101 |  298 | `		PCRE2_SIZE end   = ovector[2 * i + 1];` |
|   101 |  299 | `		if( start == PCRE2_UNSET ){` |
|   ! 0 |  300 | `			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  301 | `				ph7_value_null(pVal);` |
|   ! 0 |  302 | `			}else{` |
|   ! 0 |  303 | `				ph7_value_string(pVal, "", 0);` |
|     - |  304 | `			}` |
|   ! 0 |  305 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  306 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  307 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  308 | `				ph7_value_int(pOff, -1);` |
|   ! 0 |  309 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  310 | `				ph7_array_add_intkey_elem(pArray, i, pSub);` |
|     - |  311 | `				/* Reset sub-array for reuse */` |
|   ! 0 |  312 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  313 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  314 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  315 | `			}else{` |
|   ! 0 |  316 | `				ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  317 | `			}` |
|   ! 0 |  318 | `		}else{` |
|   101 |  319 | `			ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|   101 |  320 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  321 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  322 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  323 | `				ph7_value_int(pOff, (int)start);` |
|   ! 0 |  324 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  325 | `				ph7_array_add_intkey_elem(pArray, i, pSub);` |
|   ! 0 |  326 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  327 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  328 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  329 | `			}else{` |
|   101 |  330 | `				ph7_array_add_intkey_elem(pArray, i, pVal);` |
|     - |  331 | `			}` |
|     - |  332 | `		}` |
|   101 |  333 | `		ph7_value_reset_string_cursor(pVal);` |
|    53 |  334 | `	}` |
|     - |  335 | `	/* Named groups */` |
|    53 |  336 | `	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|    53 |  337 | `	if( namecount > 0 ){` |
|     5 |  338 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     5 |  339 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|    13 |  340 | `		for( i = 0; (uint32_t)i < namecount; i++ ){` |
|     9 |  341 | `			PCRE2_SPTR entry = nametable + i * nameentrysize;` |
|     9 |  342 | `			int groupNum = (entry[0] << 8) \| entry[1];` |
|     9 |  343 | `			const char *zName = (const char *)(entry + 2);` |
|     - |  344 | `			PCRE2_SIZE start, end;` |
|     9 |  345 | `			if( groupNum >= nGroups ) continue;` |
|     9 |  346 | `			start = ovector[2 * groupNum];` |
|     9 |  347 | `			end   = ovector[2 * groupNum + 1];` |
|     9 |  348 | `			if( start == PCRE2_UNSET ){` |
|   ! 0 |  349 | `				if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|   ! 0 |  350 | `					ph7_value_null(pVal);` |
|   ! 0 |  351 | `				}else{` |
|   ! 0 |  352 | `					ph7_value_string(pVal, "", 0);` |
|     - |  353 | `				}` |
|   ! 0 |  354 | `			}else{` |
|     9 |  355 | `				ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|     - |  356 | `			}` |
|     9 |  357 | `			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  358 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  359 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  360 | `				ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);` |
|   ! 0 |  361 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  362 | `				ph7_array_add_strkey_elem(pArray, zName, pSub);` |
|   ! 0 |  363 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  364 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  365 | `				pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  366 | `			}else{` |
|     9 |  367 | `				ph7_array_add_strkey_elem(pArray, zName, pVal);` |
|     - |  368 | `			}` |
|     9 |  369 | `			ph7_value_reset_string_cursor(pVal);` |
|     5 |  370 | `		}` |
|     2 |  371 | `	}` |
|    53 |  372 | `	ph7_context_release_value(pCtx, pVal);` |
|    53 |  373 | `	if( pSub ){` |
|   ! 0 |  374 | `		ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  375 | `	}` |
|    53 |  376 | `}` |
|     - |  377 |  |
|     - |  378 | `/*` |
|     - |  379 | ` * Quiet whole-pattern match used by FILTER_VALIDATE_REGEXP: compile zPat (a full` |
|     - |  380 | ` * "/.../flags" pattern) and test it against zSub. On a successful attempt returns` |
|     - |  381 | ` * SXRET_OK with *pMatched set to 1 (match) or 0 (no match); returns SXERR_INVALID` |
|     - |  382 | ` * on a compile/match error (the caller treats that as a validation failure). The` |
|     - |  383 | ` * compiled code is owned by PcreCompile's cache, so it is not freed here.` |
|     - |  384 | ` */` |
|     4 |  385 | `PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,` |
|     - |  386 | `	const char *zSub,int nSub,int *pMatched)` |
|     1 |  387 | `{` |
|     - |  388 | `	pcre2_code *pCode;` |
|     - |  389 | `	pcre2_match_data *pMatchData;` |
|     - |  390 | `	sxu32 nCapture;` |
|     - |  391 | `	int rc;` |
|     5 |  392 | `	*pMatched = 0;` |
|     5 |  393 | `	pCode = PcreCompile(pCtx,zPat,nPat,&nCapture);` |
|     5 |  394 | `	if( pCode == 0 ){` |
|   ! 0 |  395 | `		return SXERR_INVALID;` |
|     - |  396 | `	}` |
|     5 |  397 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode,NULL);` |
|     5 |  398 | `	if( pMatchData == 0 ){` |
|   ! 0 |  399 | `		return SXERR_INVALID;` |
|     - |  400 | `	}` |
|     5 |  401 | `	rc = pcre2_match(pCode,(PCRE2_SPTR)zSub,(PCRE2_SIZE)nSub,0,0,pMatchData,NULL);` |
|     5 |  402 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  403 | `	if( rc < 0 ){` |
|     3 |  404 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  405 | `			PcreSetMatchError(pCtx->pVm,rc);` |
|   ! 0 |  406 | `			return SXERR_INVALID;` |
|     - |  407 | `		}` |
|     3 |  408 | `		return SXRET_OK; /* clean no-match */` |
|     - |  409 | `	}` |
|     3 |  410 | `	*pMatched = 1;` |
|     3 |  411 | `	return SXRET_OK;` |
|     3 |  412 | `}` |
|     - |  413 | `/* ======================================================================` |
|     - |  414 | ` * preg_match(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  415 | ` * ====================================================================== */` |
|    38 |  416 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  417 | `{` |
|     - |  418 | `	const char *zPattern, *zSubject;` |
|     - |  419 | `	int nPatLen, nSubLen;` |
|     - |  420 | `	pcre2_code *pCode;` |
|     - |  421 | `	pcre2_match_data *pMatchData;` |
|     - |  422 | `	PCRE2_SIZE *ovector;` |
|     - |  423 | `	sxu32 nCapture;` |
|    43 |  424 | `	PCRE2_SIZE startOffset = 0;` |
|    43 |  425 | `	int iFlags = 0;` |
|     - |  426 | `	int rc;` |
|     - |  427 |  |
|    43 |  428 | `	if( nArg < 2 ){` |
|   ! 0 |  429 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  430 | `			"preg_match() expects at least 2 parameters");` |
|   ! 0 |  431 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  432 | `		return PH7_OK;` |
|     - |  433 | `	}` |
|    43 |  434 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    43 |  435 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    43 |  436 | `	if( nArg >= 4 ){` |
|   ! 0 |  437 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  438 | `	}` |
|    43 |  439 | `	if( nArg >= 5 ){` |
|   ! 0 |  440 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  441 | `	}` |
|    43 |  442 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    43 |  443 | `	if( pCode == 0 ){` |
|   ! 0 |  444 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  445 | `		return PH7_OK;` |
|     - |  446 | `	}` |
|    43 |  447 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    43 |  448 | `	if( pMatchData == 0 ){` |
|   ! 0 |  449 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  450 | `		return PH7_OK;` |
|     - |  451 | `	}` |
|    62 |  452 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    19 |  453 | `		startOffset, 0, pMatchData, NULL);` |
|    43 |  454 | `	if( rc < 0 ){` |
|     5 |  455 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  456 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  457 | `		}` |
|     - |  458 | `		/* Populate empty matches if requested */` |
|     5 |  459 | `		if( nArg >= 3 ){` |
|     5 |  460 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|     5 |  461 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pEmpty);` |
|     5 |  462 | `			ph7_context_release_value(pCtx, pEmpty);` |
|     2 |  463 | `		}` |
|     5 |  464 | `		pcre2_match_data_free(pMatchData);` |
|     5 |  465 | `		ph7_result_int(pCtx, 0);` |
|     5 |  466 | `		return PH7_OK;` |
|     - |  467 | `	}` |
|    39 |  468 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    39 |  469 | `	if( nArg >= 3 ){` |
|     - |  470 | `		/* Populate $matches */` |
|    31 |  471 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    31 |  472 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    31 |  473 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);` |
|     - |  474 | `		/* Write the array back to the caller's variable */` |
|    31 |  475 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pArray);` |
|    31 |  476 | `		ph7_context_release_value(pCtx, pArray);` |
|    13 |  477 | `	}` |
|    39 |  478 | `	pcre2_match_data_free(pMatchData);` |
|    39 |  479 | `	ph7_result_int(pCtx, 1);` |
|    39 |  480 | `	return PH7_OK;` |
|    24 |  481 | `}` |
|     - |  482 |  |
|     - |  483 | `/* ======================================================================` |
|     - |  484 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|     - |  485 | ` * ====================================================================== */` |
|     8 |  486 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  487 | `{` |
|     - |  488 | `	const char *zPattern, *zSubject;` |
|     - |  489 | `	int nPatLen, nSubLen;` |
|     - |  490 | `	pcre2_code *pCode;` |
|     - |  491 | `	pcre2_match_data *pMatchData;` |
|     - |  492 | `	sxu32 nCapture;` |
|     9 |  493 | `	PCRE2_SIZE startOffset = 0;` |
|     9 |  494 | `	int iFlags = PHP_PREG_PATTERN_ORDER;` |
|     9 |  495 | `	int totalMatches = 0;` |
|     - |  496 | `	int rc;` |
|     - |  497 |  |
|     9 |  498 | `	if( nArg < 2 ){` |
|   ! 0 |  499 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  500 | `			"preg_match_all() expects at least 2 parameters");` |
|   ! 0 |  501 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  502 | `		return PH7_OK;` |
|     - |  503 | `	}` |
|     9 |  504 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     9 |  505 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     9 |  506 | `	if( nArg >= 4 ){` |
|     3 |  507 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     1 |  508 | `	}` |
|     9 |  509 | `	if( nArg >= 5 ){` |
|   ! 0 |  510 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|   ! 0 |  511 | `	}` |
|     9 |  512 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     9 |  513 | `	if( pCode == 0 ){` |
|   ! 0 |  514 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  515 | `		return PH7_OK;` |
|     - |  516 | `	}` |
|     9 |  517 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     9 |  518 | `	if( pMatchData == 0 ){` |
|   ! 0 |  519 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  520 | `		return PH7_OK;` |
|     - |  521 | `	}` |
|     9 |  522 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  523 | `	{` |
|     9 |  524 | `		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;` |
|     - |  525 |  |
|     9 |  526 | `		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){` |
|     7 |  527 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  528 | `				PCRE2_SIZE *ovector;` |
|    10 |  529 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     3 |  530 | `					startOffset, 0, pMatchData, NULL);` |
|     7 |  531 | `				if( rc < 0 ){` |
|     3 |  532 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     3 |  533 | `					break;` |
|     - |  534 | `				}` |
|     5 |  535 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     5 |  536 | `				if( pOutArray ){` |
|     5 |  537 | `					ph7_value *pSet = ph7_context_new_array(pCtx);` |
|     5 |  538 | `					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, iFlags & ~0xFF);` |
|     5 |  539 | `					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);` |
|     5 |  540 | `					ph7_context_release_value(pCtx, pSet);` |
|     2 |  541 | `				}` |
|     5 |  542 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  543 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  544 | `				}else{` |
|     5 |  545 | `					startOffset = ovector[1];` |
|     - |  546 | `				}` |
|     5 |  547 | `				totalMatches++;` |
|     1 |  548 | `			}` |
|     2 |  549 | `		}else{` |
|     - |  550 | `			/* PREG_PATTERN_ORDER (default) */` |
|     7 |  551 | `			ph7_value **apGroupArrays = 0;` |
|     7 |  552 | `			sxu32 nGroups = nCapture + 1;` |
|     - |  553 | `			sxu32 g;` |
|     7 |  554 | `			if( pOutArray ){` |
|    10 |  555 | `				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,` |
|     3 |  556 | `					sizeof(ph7_value *) * nGroups, TRUE, FALSE);` |
|     7 |  557 | `				if( apGroupArrays ){` |
|    17 |  558 | `					for( g = 0; g < nGroups; g++ ){` |
|    11 |  559 | `						apGroupArrays[g] = ph7_context_new_array(pCtx);` |
|     6 |  560 | `					}` |
|     3 |  561 | `				}` |
|     3 |  562 | `			}` |
|    23 |  563 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  564 | `				PCRE2_SIZE *ovector;` |
|    34 |  565 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    11 |  566 | `					startOffset, 0, pMatchData, NULL);` |
|    23 |  567 | `				if( rc < 0 ){` |
|     7 |  568 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     7 |  569 | `					break;` |
|     - |  570 | `				}` |
|    17 |  571 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    17 |  572 | `				if( apGroupArrays ){` |
|    17 |  573 | `					ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    17 |  574 | `					int nActual = rc;` |
|    41 |  575 | `					for( g = 0; g < nGroups; g++ ){` |
|    37 |  576 | `						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){` |
|    25 |  577 | `							PCRE2_SIZE s = ovector[2*g];` |
|    25 |  578 | `							PCRE2_SIZE e = ovector[2*g+1];` |
|    25 |  579 | `							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|   ! 0 |  580 | `								ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  581 | `								ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  582 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|   ! 0 |  583 | `								ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  584 | `								ph7_value_int(pOff, (int)s);` |
|   ! 0 |  585 | `								ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  586 | `								ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|   ! 0 |  587 | `								ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  588 | `								ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  589 | `							}else{` |
|    25 |  590 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|    25 |  591 | `								ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  592 | `							}` |
|    13 |  593 | `						}else{` |
|   ! 0 |  594 | `							ph7_value_string(pVal, "", 0);` |
|   ! 0 |  595 | `							ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|     - |  596 | `						}` |
|    25 |  597 | `						ph7_value_reset_string_cursor(pVal);` |
|    13 |  598 | `					}` |
|    17 |  599 | `					ph7_context_release_value(pCtx, pVal);` |
|     8 |  600 | `				}` |
|    17 |  601 | `				if( ovector[1] == ovector[0] ){` |
|   ! 0 |  602 | `					startOffset = ovector[0] + 1;` |
|   ! 0 |  603 | `				}else{` |
|    17 |  604 | `					startOffset = ovector[1];` |
|     - |  605 | `				}` |
|    17 |  606 | `				totalMatches++;` |
|     1 |  607 | `			}` |
|     7 |  608 | `			if( apGroupArrays ){` |
|    17 |  609 | `				for( g = 0; g < nGroups; g++ ){` |
|    11 |  610 | `					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);` |
|    11 |  611 | `					ph7_context_release_value(pCtx, apGroupArrays[g]);` |
|     6 |  612 | `				}` |
|     7 |  613 | `				ph7_context_free_chunk(pCtx, apGroupArrays);` |
|     3 |  614 | `			}` |
|     - |  615 | `		}` |
|     - |  616 | `		/* Write output array to caller's variable */` |
|     9 |  617 | `		if( pOutArray && nArg >= 3 ){` |
|     9 |  618 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pOutArray);` |
|     9 |  619 | `			ph7_context_release_value(pCtx, pOutArray);` |
|     4 |  620 | `		}` |
|     - |  621 | `	}` |
|     9 |  622 | `	pcre2_match_data_free(pMatchData);` |
|     9 |  623 | `	ph7_result_int(pCtx, totalMatches);` |
|     9 |  624 | `	return PH7_OK;` |
|     5 |  625 | `}` |
|     - |  626 |  |
|     - |  627 | `/* ======================================================================` |
|     - |  628 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|     - |  629 | ` * ====================================================================== */` |
|     4 |  630 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  631 | `{` |
|     - |  632 | `	const char *zPattern, *zSubject;` |
|     - |  633 | `	int nPatLen, nSubLen;` |
|     - |  634 | `	pcre2_code *pCode;` |
|     - |  635 | `	pcre2_match_data *pMatchData;` |
|     - |  636 | `	sxu32 nCapture;` |
|     - |  637 | `	ph7_value *pArray;` |
|     - |  638 | `	ph7_value *pVal;` |
|     5 |  639 | `	PCRE2_SIZE startOffset = 0, lastOffset = 0;` |
|     5 |  640 | `	int limit = -1;` |
|     5 |  641 | `	int iFlags = 0;` |
|     5 |  642 | `	int nPieces = 0;` |
|     - |  643 | `	int rc;` |
|     - |  644 |  |
|     5 |  645 | `	if( nArg < 2 ){` |
|   ! 0 |  646 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  647 | `			"preg_split() expects at least 2 parameters");` |
|   ! 0 |  648 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  649 | `		return PH7_OK;` |
|     - |  650 | `	}` |
|     5 |  651 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     5 |  652 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|     5 |  653 | `	if( nArg >= 3 ){` |
|     3 |  654 | `		limit = ph7_value_to_int(apArg[2]);` |
|     1 |  655 | `	}` |
|     5 |  656 | `	if( nArg >= 4 ){` |
|   ! 0 |  657 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|   ! 0 |  658 | `	}` |
|     5 |  659 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     5 |  660 | `	if( pCode == 0 ){` |
|   ! 0 |  661 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  662 | `		return PH7_OK;` |
|     - |  663 | `	}` |
|     5 |  664 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     5 |  665 | `	if( pMatchData == 0 ){` |
|   ! 0 |  666 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  667 | `		return PH7_OK;` |
|     - |  668 | `	}` |
|     5 |  669 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  670 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     5 |  671 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - |  672 |  |
|    13 |  673 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|    13 |  674 | `		if( limit > 0 && nPieces >= limit - 1 ){` |
|     3 |  675 | `			break; /* Last piece gets the remainder */` |
|     - |  676 | `		}` |
|    16 |  677 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     5 |  678 | `			startOffset, 0, pMatchData, NULL);` |
|    11 |  679 | `		if( rc < 0 ){` |
|     3 |  680 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  681 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  682 | `			}` |
|     3 |  683 | `			break;` |
|     - |  684 | `		}` |
|     - |  685 | `		{` |
|     9 |  686 | `			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     9 |  687 | `			PCRE2_SIZE matchStart = ovector[0];` |
|     9 |  688 | `			PCRE2_SIZE matchEnd = ovector[1];` |
|     9 |  689 | `			int pieceLen = (int)(matchStart - lastOffset);` |
|     - |  690 |  |
|     - |  691 | `			/* Add the piece before the match */` |
|     9 |  692 | `			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| pieceLen > 0 ){` |
|     9 |  693 | `				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  694 | `					ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  695 | `					ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  696 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|   ! 0 |  697 | `					ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  698 | `					ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  699 | `					ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  700 | `					ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  701 | `					ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  702 | `					ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  703 | `				}else{` |
|     9 |  704 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|     9 |  705 | `					ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  706 | `				}` |
|     9 |  707 | `				ph7_value_reset_string_cursor(pVal);` |
|     9 |  708 | `				nPieces++;` |
|     4 |  709 | `			}` |
|     - |  710 | `			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */` |
|     9 |  711 | `			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){` |
|     - |  712 | `				int g;` |
|   ! 0 |  713 | `				for( g = 1; g < rc; g++ ){` |
|   ! 0 |  714 | `					PCRE2_SIZE gs = ovector[2*g];` |
|   ! 0 |  715 | `					PCRE2_SIZE ge = ovector[2*g+1];` |
|     - |  716 | `					int gLen;` |
|   ! 0 |  717 | `					if( gs == PCRE2_UNSET ) continue;` |
|   ! 0 |  718 | `					gLen = (int)(ge - gs);` |
|   ! 0 |  719 | `					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| gLen > 0 ){` |
|   ! 0 |  720 | `						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  721 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  722 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  723 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  724 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  725 | `							ph7_value_int(pOff, (int)gs);` |
|   ! 0 |  726 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  727 | `							ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  728 | `							ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  729 | `							ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  730 | `						}else{` |
|   ! 0 |  731 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|   ! 0 |  732 | `							ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  733 | `						}` |
|   ! 0 |  734 | `						ph7_value_reset_string_cursor(pVal);` |
|   ! 0 |  735 | `					}` |
|   ! 0 |  736 | `				}` |
|   ! 0 |  737 | `			}` |
|     - |  738 | `			/* Advance */` |
|     9 |  739 | `			lastOffset = matchEnd;` |
|     9 |  740 | `			if( matchEnd == matchStart ){` |
|   ! 0 |  741 | `				startOffset = matchEnd + 1;` |
|   ! 0 |  742 | `			}else{` |
|     9 |  743 | `				startOffset = matchEnd;` |
|     - |  744 | `			}` |
|     - |  745 | `		}` |
|     1 |  746 | `	}` |
|     - |  747 | `	/* Add trailing piece */` |
|     - |  748 | `	{` |
|     5 |  749 | `		int trailLen = nSubLen - (int)lastOffset;` |
|     5 |  750 | `		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| trailLen > 0 ){` |
|     5 |  751 | `			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|   ! 0 |  752 | `				ph7_value *pSub = ph7_context_new_array(pCtx);` |
|   ! 0 |  753 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|   ! 0 |  754 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|   ! 0 |  755 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|   ! 0 |  756 | `				ph7_value_int(pOff, (int)lastOffset);` |
|   ! 0 |  757 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|   ! 0 |  758 | `				ph7_array_add_elem(pArray, 0, pSub);` |
|   ! 0 |  759 | `				ph7_context_release_value(pCtx, pSub);` |
|   ! 0 |  760 | `				ph7_context_release_value(pCtx, pOff);` |
|   ! 0 |  761 | `			}else{` |
|     5 |  762 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|     5 |  763 | `				ph7_array_add_elem(pArray, 0, pVal);` |
|     - |  764 | `			}` |
|     2 |  765 | `		}` |
|     - |  766 | `	}` |
|     5 |  767 | `	ph7_context_release_value(pCtx, pVal);` |
|     5 |  768 | `	pcre2_match_data_free(pMatchData);` |
|     5 |  769 | `	ph7_result_value(pCtx, pArray);` |
|     5 |  770 | `	ph7_context_release_value(pCtx, pArray);` |
|     5 |  771 | `	return PH7_OK;` |
|     3 |  772 | `}` |
|     - |  773 |  |
|     - |  774 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|    88 |  775 | `static void PcreExpandBackrefs(` |
|     - |  776 | `	SyBlob *pOut,` |
|     - |  777 | `	const char *zRepl, int nReplLen,` |
|     - |  778 | `	const char *zSubject,` |
|     - |  779 | `	PCRE2_SIZE *ovector, int nGroups)` |
|     1 |  780 | `{` |
|    89 |  781 | `	const char *zEnd = &zRepl[nReplLen];` |
|    89 |  782 | `	const char *z = zRepl;` |
|     - |  783 |  |
|   183 |  784 | `	while( z < zEnd ){` |
|    95 |  785 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   ! 0 |  786 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|   ! 0 |  787 | `				int g = z[1] - '0';` |
|   ! 0 |  788 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  789 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  790 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  791 | `				}` |
|   ! 0 |  792 | `				z += 2;` |
|   ! 0 |  793 | `				continue;` |
|     - |  794 | `			}` |
|   ! 0 |  795 | `			if( z[1] == '\\' ){` |
|   ! 0 |  796 | `				SyBlobAppend(pOut, "\\", 1);` |
|   ! 0 |  797 | `				z += 2;` |
|   ! 0 |  798 | `				continue;` |
|     - |  799 | `			}` |
|     - |  800 | `			/* Not a backreference — emit literally */` |
|   ! 0 |  801 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  802 | `			z++;` |
|   ! 0 |  803 | `			continue;` |
|     - |  804 | `		}` |
|    95 |  805 | `		if( *z == '$' && z + 1 < zEnd ){` |
|    13 |  806 | `			if( z[1] == '$' ){` |
|   ! 0 |  807 | `				SyBlobAppend(pOut, "$", 1);` |
|   ! 0 |  808 | `				z += 2;` |
|   ! 0 |  809 | `				continue;` |
|     - |  810 | `			}` |
|    13 |  811 | `			if( z[1] == '{' ){` |
|     - |  812 | `				/* ${N} form */` |
|   ! 0 |  813 | `				const char *p = z + 2;` |
|   ! 0 |  814 | `				int g = 0;` |
|   ! 0 |  815 | `				while( p < zEnd && *p >= '0' && *p <= '9' ){` |
|   ! 0 |  816 | `					g = g * 10 + (*p - '0');` |
|   ! 0 |  817 | `					p++;` |
|   ! 0 |  818 | `				}` |
|   ! 0 |  819 | `				if( p < zEnd && *p == '}' ){` |
|   ! 0 |  820 | `					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|   ! 0 |  821 | `						SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|   ! 0 |  822 | `							(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|   ! 0 |  823 | `					}` |
|   ! 0 |  824 | `					z = p + 1;` |
|   ! 0 |  825 | `					continue;` |
|     - |  826 | `				}` |
|     - |  827 | `				/* Not a valid ${N} — emit literally */` |
|   ! 0 |  828 | `				SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  829 | `				z++;` |
|   ! 0 |  830 | `				continue;` |
|     - |  831 | `			}` |
|    13 |  832 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|     - |  833 | `				/* $N or $NN */` |
|    13 |  834 | `				int g = z[1] - '0';` |
|    13 |  835 | `				z += 2;` |
|     - |  836 | `				/* Check for second digit */` |
|    13 |  837 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  838 | `					int g2 = g * 10 + (*z - '0');` |
|   ! 0 |  839 | `					if( g2 < nGroups ){` |
|   ! 0 |  840 | `						g = g2;` |
|   ! 0 |  841 | `						z++;` |
|   ! 0 |  842 | `					}` |
|   ! 0 |  843 | `				}` |
|    13 |  844 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|    19 |  845 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|    12 |  846 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|     6 |  847 | `				}` |
|    13 |  848 | `				continue;` |
|     - |  849 | `			}` |
|     - |  850 | `			/* Not a backreference */` |
|   ! 0 |  851 | `			SyBlobAppend(pOut, z, 1);` |
|   ! 0 |  852 | `			z++;` |
|   ! 0 |  853 | `			continue;` |
|     - |  854 | `		}` |
|    83 |  855 | `		SyBlobAppend(pOut, z, 1);` |
|    83 |  856 | `		z++;` |
|     1 |  857 | `	}` |
|    89 |  858 | `}` |
|     - |  859 |  |
|     - |  860 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|    60 |  861 | `static void PcreDoReplace(` |
|     - |  862 | `	ph7_context *pCtx,` |
|     - |  863 | `	pcre2_code *pCode,` |
|     - |  864 | `	const char *zSubject, int nSubLen,` |
|     - |  865 | `	const char *zRepl, int nReplLen,` |
|     - |  866 | `	int limit,` |
|     - |  867 | `	int *pCount,` |
|     - |  868 | `	SyBlob *pOut)` |
|     1 |  869 | `{` |
|     - |  870 | `	pcre2_match_data *pMatchData;` |
|    61 |  871 | `	PCRE2_SIZE startOffset = 0;` |
|    61 |  872 | `	int nReplacements = 0;` |
|     - |  873 | `	int rc;` |
|     - |  874 |  |
|    61 |  875 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    61 |  876 | `	if( pMatchData == 0 ) return;` |
|     - |  877 |  |
|   149 |  878 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - |  879 | `		PCRE2_SIZE *ovector;` |
|   149 |  880 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|   223 |  881 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    74 |  882 | `			startOffset, 0, pMatchData, NULL);` |
|   149 |  883 | `		if( rc < 0 ){` |
|    61 |  884 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 |  885 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 |  886 | `			}` |
|    61 |  887 | `			break;` |
|     - |  888 | `		}` |
|    89 |  889 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - |  890 | `		/* Copy text before match */` |
|    89 |  891 | `		if( ovector[0] > startOffset ){` |
|    61 |  892 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|    30 |  893 | `		}` |
|     - |  894 | `		/* Expand replacement */` |
|    89 |  895 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|    89 |  896 | `		nReplacements++;` |
|     - |  897 | `		/* Advance */` |
|    89 |  898 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 |  899 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 |  900 | `				SyBlobAppend(pOut, &zSubject[startOffset], 1);` |
|   ! 0 |  901 | `			}` |
|   ! 0 |  902 | `			startOffset = ovector[0] + 1;` |
|   ! 0 |  903 | `		}else{` |
|    89 |  904 | `			startOffset = ovector[1];` |
|     - |  905 | `		}` |
|     1 |  906 | `	}` |
|     - |  907 | `	/* Copy remainder */` |
|    61 |  908 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|    15 |  909 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|     7 |  910 | `	}` |
|    61 |  911 | `	if( pCount ){` |
|    61 |  912 | `		*pCount += nReplacements;` |
|    30 |  913 | `	}` |
|    61 |  914 | `	pcre2_match_data_free(pMatchData);` |
|    30 |  915 | `	SXUNUSED(pCtx);` |
|    31 |  916 | `}` |
|     - |  917 |  |
|     - |  918 | `/* ===== Helper: apply pattern(s)+replacement(s) to ONE subject string =====` |
|     - |  919 | ` * pPattern is a string or an array of patterns; pRepl is a string (used for` |
|     - |  920 | ` * every pattern) or, only when pPattern is an array, an array taken by ORDER` |
|     - |  921 | ` * (missing element -> ""). Array patterns are applied sequentially, each to the` |
|     - |  922 | ` * result of the previous (PHP semantics), ping-ponging two blobs. The final` |
|     - |  923 | ` * text is appended to pOut. Returns SXRET_OK, or SXERR_ABORT on a bad pattern` |
|     - |  924 | ` * (the caller then yields NULL, matching the scalar path). */` |
|    52 |  925 | `static sxi32 PcreReplaceSubject(` |
|     - |  926 | `	ph7_context *pCtx,` |
|     - |  927 | `	ph7_value *pPattern,` |
|     - |  928 | `	ph7_value *pRepl,` |
|     - |  929 | `	const char *zSubject, int nSubLen,` |
|     - |  930 | `	int limit,` |
|     - |  931 | `	int *pCount,` |
|     - |  932 | `	SyBlob *pOut)` |
|     1 |  933 | `{` |
|     - |  934 | `	sxu32 nCapture;` |
|    53 |  935 | `	if( !ph7_value_is_array(pPattern) ){` |
|     - |  936 | `		/* Single pattern + single replacement */` |
|     - |  937 | `		const char *zPattern, *zRepl;` |
|     - |  938 | `		int nPatLen, nReplLen;` |
|     - |  939 | `		pcre2_code *pCode;` |
|    41 |  940 | `		zPattern = ph7_value_to_string(pPattern, &nPatLen);` |
|    41 |  941 | `		zRepl = ph7_value_to_string(pRepl, &nReplLen);` |
|    41 |  942 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    41 |  943 | `		if( pCode == 0 ){` |
|   ! 0 |  944 | `			return SXERR_ABORT;` |
|     - |  945 | `		}` |
|    41 |  946 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, pCount, pOut);` |
|    41 |  947 | `		return SXRET_OK;` |
|   ! 0 |  948 | `	}else{` |
|     - |  949 | `		/* Array of patterns: apply each in insertion order to the accumulating` |
|     - |  950 | `		 * subject. Replacement is the parallel array element (by order) or the` |
|     - |  951 | `		 * scalar replacement for every pattern. */` |
|    13 |  952 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|    13 |  953 | `		ph7_hashmap *pRepMap = ph7_value_is_array(pRepl) ? (ph7_hashmap *)pRepl->x.pOther : 0;` |
|    13 |  954 | `		const char *zScalarRepl = 0;` |
|    13 |  955 | `		int nScalarRepl = 0;` |
|     - |  956 | `		ph7_hashmap_node *pPatNode, *pRepNode;` |
|     - |  957 | `		ph7_value sPat, sRep;` |
|     - |  958 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|     - |  959 | `		sxu32 n;` |
|    13 |  960 | `		sxi32 rc = SXRET_OK;` |
|    13 |  961 | `		if( pRepMap == 0 ){` |
|     5 |  962 | `			zScalarRepl = ph7_value_to_string(pRepl, &nScalarRepl);` |
|     2 |  963 | `		}` |
|    13 |  964 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|    13 |  965 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|    13 |  966 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|    13 |  967 | `		pSrc = &sA; pDst = &sB;` |
|    13 |  968 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|    13 |  969 | `		PH7_MemObjInit(pCtx->pVm, &sRep);` |
|    13 |  970 | `		pPatNode = pPatMap->pFirst;` |
|    13 |  971 | `		pRepNode = pRepMap ? pRepMap->pFirst : 0;` |
|    13 |  972 | `		n = pPatMap->nEntry;` |
|    33 |  973 | `		while( n > 0 ){` |
|     - |  974 | `			const char *zPattern, *zRepl;` |
|     - |  975 | `			int nPatLen, nReplLen;` |
|     - |  976 | `			pcre2_code *pCode;` |
|     - |  977 | `			SyBlob *pSwap;` |
|    21 |  978 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|    21 |  979 | `			zPattern = ph7_value_to_string(&sPat, &nPatLen);` |
|    21 |  980 | `			if( pRepMap ){` |
|    17 |  981 | `				if( pRepNode ){` |
|    15 |  982 | `					PH7_HashmapExtractNodeValue(pRepNode, &sRep, FALSE);` |
|    15 |  983 | `					zRepl = ph7_value_to_string(&sRep, &nReplLen);` |
|     8 |  984 | `				}else{` |
|     3 |  985 | `					zRepl = ""; nReplLen = 0;` |
|     - |  986 | `				}` |
|     9 |  987 | `			}else{` |
|     5 |  988 | `				zRepl = zScalarRepl; nReplLen = nScalarRepl;` |
|     - |  989 | `			}` |
|    21 |  990 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    21 |  991 | `			if( pCode == 0 ){` |
|   ! 0 |  992 | `				rc = SXERR_ABORT;` |
|   ! 0 |  993 | `				PH7_MemObjRelease(&sPat);` |
|   ! 0 |  994 | `				if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|   ! 0 |  995 | `				break;` |
|     - |  996 | `			}` |
|    21 |  997 | `			SyBlobReset(pDst);` |
|    31 |  998 | `			PcreDoReplace(pCtx, pCode,` |
|    20 |  999 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|    10 | 1000 | `				zRepl, nReplLen, limit, pCount, pDst);` |
|     - | 1001 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|    21 | 1002 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|    21 | 1003 | `			PH7_MemObjRelease(&sPat);` |
|    21 | 1004 | `			if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|    21 | 1005 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|    21 | 1006 | `			if( pRepNode ){ pRepNode = pRepNode->pPrev; }` |
|    21 | 1007 | `			n--;` |
|     1 | 1008 | `		}` |
|    13 | 1009 | `		if( rc == SXRET_OK ){` |
|    13 | 1010 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|     6 | 1011 | `		}` |
|    13 | 1012 | `		SyBlobRelease(&sA);` |
|    13 | 1013 | `		SyBlobRelease(&sB);` |
|    13 | 1014 | `		return rc;` |
|     - | 1015 | `	}` |
|    27 | 1016 | `}` |
|     - | 1017 |  |
|     - | 1018 | `/* ======================================================================` |
|     - | 1019 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|     - | 1020 | ` * ====================================================================== */` |
|    36 | 1021 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1022 | `{` |
|    37 | 1023 | `	int limit = -1;` |
|    37 | 1024 | `	int count = 0;` |
|     - | 1025 |  |
|    37 | 1026 | `	if( nArg < 3 ){` |
|   ! 0 | 1027 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1028 | `			"preg_replace() expects at least 3 parameters");` |
|   ! 0 | 1029 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1030 | `		return PH7_OK;` |
|     - | 1031 | `	}` |
|    37 | 1032 | `	if( nArg >= 4 ){` |
|     7 | 1033 | `		limit = ph7_value_to_int(apArg[3]);` |
|     3 | 1034 | `	}` |
|    37 | 1035 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1036 |  |
|     - | 1037 | `	/* A scalar pattern with an array replacement is a parameter mismatch (PHP` |
|     - | 1038 | `	 * throws a TypeError; PHL keeps preg_replace's warning-based arg-error style). */` |
|    37 | 1039 | `	if( !ph7_value_is_array(apArg[0]) && ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 1040 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1041 | `			"preg_replace(): Parameter mismatch, pattern is a string while replacement is an array");` |
|   ! 0 | 1042 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1043 | `		return PH7_OK;` |
|     - | 1044 | `	}` |
|    55 | 1045 | `	if( ph7_value_is_array(apArg[2]) ){` |
|     - | 1046 | `		/* Array subject: return an array, each element replaced, keys preserved. */` |
|    15 | 1047 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|    15 | 1048 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|    15 | 1049 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     - | 1050 | `		ph7_value sKey, sVal;` |
|     - | 1051 | `		ph7_hashmap_node *pNode;` |
|     - | 1052 | `		sxu32 n;` |
|    15 | 1053 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|   ! 0 | 1054 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1055 | `			return PH7_OK;` |
|     - | 1056 | `		}` |
|    15 | 1057 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|    15 | 1058 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    15 | 1059 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|    15 | 1060 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|    45 | 1061 | `		while( n > 0 ){` |
|     - | 1062 | `			const char *zSubject;` |
|     - | 1063 | `			int nSubLen;` |
|     - | 1064 | `			SyBlob sOut;` |
|    31 | 1065 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|    31 | 1066 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|    31 | 1067 | `			zSubject = ph7_value_to_string(&sVal, &nSubLen);` |
|    31 | 1068 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    31 | 1069 | `			if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1070 | `				/* A bad pattern with an array subject yields an empty array (PHP);` |
|     - | 1071 | `				 * the failure hits the first element, so pResult is still empty. */` |
|   ! 0 | 1072 | `				SyBlobRelease(&sOut);` |
|   ! 0 | 1073 | `				PH7_MemObjRelease(&sKey);` |
|   ! 0 | 1074 | `				PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1075 | `				ph7_result_value(pCtx, pResult);` |
|   ! 0 | 1076 | `				goto set_count;` |
|     - | 1077 | `			}` |
|    31 | 1078 | `			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    31 | 1079 | `			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|    31 | 1080 | `			ph7_value_reset_string_cursor(pElem);` |
|    31 | 1081 | `			SyBlobRelease(&sOut);` |
|    31 | 1082 | `			PH7_MemObjRelease(&sKey);` |
|    31 | 1083 | `			PH7_MemObjRelease(&sVal);` |
|    31 | 1084 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|    31 | 1085 | `			n--;` |
|     1 | 1086 | `		}` |
|    15 | 1087 | `		ph7_result_value(pCtx, pResult);` |
|     8 | 1088 | `	}else{` |
|     - | 1089 | `		/* Scalar subject: one replaced string. */` |
|     - | 1090 | `		const char *zSubject;` |
|     - | 1091 | `		int nSubLen;` |
|     - | 1092 | `		SyBlob sOut;` |
|    23 | 1093 | `		zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    23 | 1094 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    23 | 1095 | `		if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|     - | 1096 | `			/* Scalar subject: a bad pattern returns NULL (PHP). */` |
|   ! 0 | 1097 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1098 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1099 | `			goto set_count;` |
|     - | 1100 | `		}` |
|    23 | 1101 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    23 | 1102 | `		SyBlobRelease(&sOut);` |
|     - | 1103 | `	}` |
|    18 | 1104 | `set_count:` |
|     - | 1105 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|     - | 1106 | `	 * (PHP always writes it: 0, or the count accumulated by earlier good patterns). */` |
|    37 | 1107 | `	if( nArg >= 5 ){` |
|     - | 1108 | `		ph7_value sCount;` |
|     7 | 1109 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     7 | 1110 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|     7 | 1111 | `		PH7_MemObjRelease(&sCount);` |
|     3 | 1112 | `	}` |
|    37 | 1113 | `	return PH7_OK;` |
|    19 | 1114 | `}` |
|     - | 1115 |  |
|     - | 1116 | `/* ======================================================================` |
|     - | 1117 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count]])` |
|     - | 1118 | ` * ====================================================================== */` |
|     8 | 1119 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1120 | `{` |
|     - | 1121 | `	const char *zPattern, *zSubject;` |
|     - | 1122 | `	int nPatLen, nSubLen;` |
|     - | 1123 | `	pcre2_code *pCode;` |
|     - | 1124 | `	pcre2_match_data *pMatchData;` |
|     - | 1125 | `	sxu32 nCapture;` |
|     - | 1126 | `	SyBlob sOut;` |
|    10 | 1127 | `	PCRE2_SIZE startOffset = 0;` |
|    10 | 1128 | `	int limit = -1;` |
|    10 | 1129 | `	int count = 0;` |
|     - | 1130 | `	int rc;` |
|     - | 1131 |  |
|    10 | 1132 | `	if( nArg < 3 ){` |
|   ! 0 | 1133 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1134 | `			"preg_replace_callback() expects at least 3 parameters");` |
|   ! 0 | 1135 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1136 | `		return PH7_OK;` |
|     - | 1137 | `	}` |
|    10 | 1138 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    10 | 1139 | `	zSubject = ph7_value_to_string(apArg[2], &nSubLen);` |
|    10 | 1140 | `	if( nArg >= 4 ){` |
|     3 | 1141 | `		limit = ph7_value_to_int(apArg[3]);` |
|     1 | 1142 | `	}` |
|    10 | 1143 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|   ! 0 | 1144 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - | 1145 | `			"preg_replace_callback() expects parameter 2 to be a valid callback");` |
|   ! 0 | 1146 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1147 | `		return PH7_OK;` |
|     - | 1148 | `	}` |
|    10 | 1149 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    10 | 1150 | `	if( pCode == 0 ){` |
|   ! 0 | 1151 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1152 | `		return PH7_OK;` |
|     - | 1153 | `	}` |
|    10 | 1154 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    10 | 1155 | `	if( pMatchData == 0 ){` |
|   ! 0 | 1156 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1157 | `		return PH7_OK;` |
|     - | 1158 | `	}` |
|    10 | 1159 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    10 | 1160 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     - | 1161 |  |
|    28 | 1162 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|     - | 1163 | `		PCRE2_SIZE *ovector;` |
|     - | 1164 | `		ph7_value *pMatchArr;` |
|     - | 1165 | `		ph7_value *apCbArg[1];` |
|     - | 1166 | `		ph7_value sResult;` |
|     - | 1167 | `		const char *zReplacement;` |
|     - | 1168 | `		int nReplLen;` |
|     - | 1169 |  |
|    32 | 1170 | `		if( limit >= 0 && count >= limit ) break;` |
|    41 | 1171 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    13 | 1172 | `			startOffset, 0, pMatchData, NULL);` |
|    28 | 1173 | `		if( rc < 0 ){` |
|    10 | 1174 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|   ! 0 | 1175 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|   ! 0 | 1176 | `			}` |
|    10 | 1177 | `			break;` |
|     - | 1178 | `		}` |
|    20 | 1179 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|     - | 1180 | `		/* Copy text before match */` |
|    20 | 1181 | `		if( ovector[0] > startOffset ){` |
|    14 | 1182 | `			SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     6 | 1183 | `		}` |
|     - | 1184 | `		/* Build matches array for callback */` |
|    20 | 1185 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    20 | 1186 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, 0);` |
|     - | 1187 | `		/* Call the callback */` |
|    20 | 1188 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    20 | 1189 | `		apCbArg[0] = pMatchArr;` |
|    20 | 1190 | `		if( PH7_VmCallUserFunction(pCtx->pVm, apArg[1], 1, apCbArg, &sResult) == PH7_EXCEPTION ){` |
|     - | 1191 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|   ! 0 | 1192 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1193 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|   ! 0 | 1194 | `			SyBlobRelease(&sOut);` |
|   ! 0 | 1195 | `			pcre2_match_data_free(pMatchData);` |
|   ! 0 | 1196 | `			return PH7_EXCEPTION;` |
|     - | 1197 | `		}` |
|     - | 1198 | `		/* Get replacement string from callback result */` |
|    20 | 1199 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    20 | 1200 | `		SyBlobAppend(&sOut, zReplacement, (sxu32)nReplLen);` |
|    20 | 1201 | `		PH7_MemObjRelease(&sResult);` |
|    20 | 1202 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    20 | 1203 | `		count++;` |
|     - | 1204 | `		/* Advance */` |
|    20 | 1205 | `		if( ovector[1] == ovector[0] ){` |
|   ! 0 | 1206 | `			if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1207 | `				SyBlobAppend(&sOut, &zSubject[startOffset], 1);` |
|   ! 0 | 1208 | `			}` |
|   ! 0 | 1209 | `			startOffset = ovector[0] + 1;` |
|   ! 0 | 1210 | `		}else{` |
|    20 | 1211 | `			startOffset = ovector[1];` |
|     - | 1212 | `		}` |
|     2 | 1213 | `	}` |
|     - | 1214 | `	/* Copy remainder */` |
|    10 | 1215 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   ! 0 | 1216 | `		SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   ! 0 | 1217 | `	}` |
|    10 | 1218 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    10 | 1219 | `	SyBlobRelease(&sOut);` |
|    10 | 1220 | `	pcre2_match_data_free(pMatchData);` |
|     - | 1221 | `	/* Set &$count if provided */` |
|    10 | 1222 | `	if( nArg >= 5 ){` |
|     - | 1223 | `		ph7_value sCount;` |
|     3 | 1224 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     3 | 1225 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|     3 | 1226 | `		PH7_MemObjRelease(&sCount);` |
|     1 | 1227 | `	}` |
|    10 | 1228 | `	return PH7_OK;` |
|     6 | 1229 | `}` |
|     - | 1230 |  |
|     - | 1231 | `/* ======================================================================` |
|     - | 1232 | ` * preg_quote(str [, delimiter])` |
|     - | 1233 | ` * ====================================================================== */` |
|     6 | 1234 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1235 | `{` |
|     7 | 1236 | `	const char *zStr, *zDelim = 0;` |
|     7 | 1237 | `	int nLen, nDelimLen = 0;` |
|     - | 1238 | `	const char *z, *zEnd;` |
|     - | 1239 |  |
|     7 | 1240 | `	if( nArg < 1 ){` |
|   ! 0 | 1241 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1242 | `		return PH7_OK;` |
|     - | 1243 | `	}` |
|     7 | 1244 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|     7 | 1245 | `	if( nArg >= 2 ){` |
|     3 | 1246 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|     1 | 1247 | `	}` |
|     7 | 1248 | `	z = zStr;` |
|     7 | 1249 | `	zEnd = &zStr[nLen];` |
|    71 | 1250 | `	while( z < zEnd ){` |
|    65 | 1251 | `		char c = *z;` |
|    65 | 1252 | `		switch( c ){` |
|     4 | 1253 | `			case '.': case '\\': case '+': case '*': case '?':` |
|     - | 1254 | `			case '[': case '^': case ']': case '$': case '(':` |
|     - | 1255 | `			case ')': case '{': case '}': case '=': case '!':` |
|     - | 1256 | `			case '<': case '>': case '\|': case ':': case '-':` |
|     - | 1257 | `			case '#':` |
|     9 | 1258 | `				ph7_result_string(pCtx, "\\", 1);` |
|     9 | 1259 | `				break;` |
|    28 | 1260 | `			default:` |
|    57 | 1261 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|     3 | 1262 | `					ph7_result_string(pCtx, "\\", 1);` |
|     1 | 1263 | `				}` |
|    56 | 1264 | `				break;` |
|     - | 1265 | `		}` |
|    65 | 1266 | `		ph7_result_string(pCtx, z, 1);` |
|    65 | 1267 | `		z++;` |
|     1 | 1268 | `	}` |
|     7 | 1269 | `	return PH7_OK;` |
|     4 | 1270 | `}` |
|     - | 1271 |  |
|     - | 1272 | `/* ======================================================================` |
|     - | 1273 | ` * preg_last_error()` |
|     - | 1274 | ` * ====================================================================== */` |
|   ! 0 | 1275 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1276 | `{` |
|   ! 0 | 1277 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1278 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|   ! 0 | 1279 | `	return PH7_OK;` |
|   ! 0 | 1280 | `}` |
|     - | 1281 |  |
|     - | 1282 | `/* ======================================================================` |
|     - | 1283 | ` * preg_last_error_msg()` |
|     - | 1284 | ` * ====================================================================== */` |
|   ! 0 | 1285 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 1286 | `{` |
|     - | 1287 | `	const char *zMsg;` |
|   ! 0 | 1288 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 1289 | `	switch( pCtx->pVm->iPcreLastError ){` |
|   ! 0 | 1290 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|   ! 0 | 1291 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|   ! 0 | 1292 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|   ! 0 | 1293 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|   ! 0 | 1294 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|   ! 0 | 1295 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|   ! 0 | 1296 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|   ! 0 | 1297 | `		default: zMsg = "Unknown error"; break;` |
|     - | 1298 | `	}` |
|   ! 0 | 1299 | `	ph7_result_string(pCtx, zMsg, -1);` |
|   ! 0 | 1300 | `	return PH7_OK;` |
|   ! 0 | 1301 | `}` |
|     - | 1302 |  |
|     - | 1303 | `/* ===== Function registration table ===== */` |
|     - | 1304 | `static const ph7_builtin_func aPcreFunc[] = {` |
|     - | 1305 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|     - | 1306 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|     - | 1307 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|     - | 1308 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|     - | 1309 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|     - | 1310 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|     - | 1311 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|     - | 1312 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|     - | 1313 | `};` |
|     - | 1314 |  |
|  3496 | 1315 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|     5 | 1316 | `{` |
|     - | 1317 | `	sxu32 n;` |
| 31469 | 1318 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 27973 | 1319 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 13989 | 1320 | `	}` |
|  3501 | 1321 | `}` |
|     - | 1322 |  |
|     - | 1323 | `/* ===== Constant registration ===== */` |
|     - | 1324 | `#define PCRE_CONST_INT(name, val) \` |
|     - | 1325 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|     - | 1326 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|     - | 1327 | `	}` |
|     - | 1328 |  |
|   ! 0 | 1329 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|     3 | 1330 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|   ! 0 | 1331 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|   ! 0 | 1332 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|   ! 0 | 1333 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|   ! 0 | 1334 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|   ! 0 | 1335 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|   ! 0 | 1336 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|   ! 0 | 1337 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|   ! 0 | 1338 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|   ! 0 | 1339 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|   ! 0 | 1340 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|   ! 0 | 1341 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|   ! 0 | 1342 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|     - | 1343 |  |
|  3496 | 1344 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|     5 | 1345 | `{` |
|  3501 | 1346 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|  3501 | 1347 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|  3501 | 1348 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|  3501 | 1349 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|  3501 | 1350 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|  3501 | 1351 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|  3501 | 1352 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|  3501 | 1353 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|  3501 | 1354 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|  3501 | 1355 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|  3501 | 1356 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|  3501 | 1357 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|  3501 | 1358 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|  3501 | 1359 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|  3501 | 1360 | `}` |
|     - | 1361 |  |
|     - | 1362 | `#else` |
|     - | 1363 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|     - | 1364 | `typedef int vm_pcre_unused;` |
|     - | 1365 | `#endif /* PH7_ENABLE_PCRE */` |
|     - | 1366 |  |
