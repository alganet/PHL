# src/ph7/vm_pcre.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1733/1983 lines (87.39%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#ifdef PH7_ENABLE_PCRE` |
|      - |    6 | `#define PCRE2_CODE_UNIT_WIDTH 8` |
|      - |    7 | `#include <pcre2.h>` |
|      - |    8 | `#include <stdlib.h>` |
|      - |    9 | `#include "ph7int.h"` |
|      - |   10 |  |
|      - |   11 | `/*` |
|      - |   12 | ` * The last-error code lives in ph7_vm::iPcreLastError (per-VM).` |
|      - |   13 | ` *` |
|      - |   14 | ` * The compiled-regex cache below is shared across VMs.  pcre2_code objects` |
|      - |   15 | ` * are immutable after compilation and safe to read concurrently; only the` |
|      - |   16 | ` * insert/evict path mutates the cache, which is fine in PHL's current` |
|      - |   17 | ` * single-threaded-execution model.  If PHL ever runs VMs on parallel` |
|      - |   18 | ` * threads, the cache needs a mutex around PcreCache_Insert.` |
|      - |   19 | ` */` |
|      - |   20 |  |
|      - |   21 | `/* ===== PREG_* constant values (matching PHP) ===== */` |
|      - |   22 | `#define PHP_PREG_PATTERN_ORDER       1` |
|      - |   23 | `#define PHP_PREG_SET_ORDER           2` |
|      - |   24 | `#define PHP_PREG_OFFSET_CAPTURE      256` |
|      - |   25 | `#define PHP_PREG_UNMATCHED_AS_NULL   512` |
|      - |   26 |  |
|      - |   27 | `#define PHP_PREG_SPLIT_NO_EMPTY          1` |
|      - |   28 | `#define PHP_PREG_GREP_INVERT             1  /* preg_grep()'s only flag */` |
|      - |   29 | `#define PHP_PREG_SPLIT_DELIM_CAPTURE     2` |
|      - |   30 | `#define PHP_PREG_SPLIT_OFFSET_CAPTURE    4` |
|      - |   31 |  |
|      - |   32 | `#define PHP_PREG_NO_ERROR                0` |
|      - |   33 | `#define PHP_PREG_INTERNAL_ERROR          1` |
|      - |   34 | `#define PHP_PREG_BACKTRACK_LIMIT_ERROR   2` |
|      - |   35 | `#define PHP_PREG_RECURSION_LIMIT_ERROR   3` |
|      - |   36 | `#define PHP_PREG_BAD_UTF8_ERROR          4` |
|      - |   37 | `#define PHP_PREG_BAD_UTF8_OFFSET_ERROR   5` |
|      - |   38 | `#define PHP_PREG_JIT_STACKLIMIT_ERROR    6` |
|      - |   39 |  |
|      - |   40 | `/* ===== Compiled-regex cache ===== */` |
|      - |   41 | `#define PCRE_CACHE_SIZE 16` |
|      - |   42 |  |
|      - |   43 | `typedef struct PcreCacheEntry PcreCacheEntry;` |
|      - |   44 | `struct PcreCacheEntry {` |
|      - |   45 | `	char *zPattern;          /* Full PHP pattern string (heap copy) */` |
|      - |   46 | `	sxu32 nLen;` |
|      - |   47 | `	pcre2_code *pCode;` |
|      - |   48 | `	sxu32 nCaptureCount;` |
|      - |   49 | `	sxu32 iLastUsed;` |
|      - |   50 | `};` |
|      - |   51 |  |
|      - |   52 | `static PcreCacheEntry aCache[PCRE_CACHE_SIZE];` |
|      - |   53 | `static sxu32 nCacheUsed = 0;` |
|      - |   54 | `static sxu32 iCacheClock = 0;` |
|      - |   55 |  |
|  37833 |   56 | `static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)` |
|      5 |   57 | `{` |
|      - |   58 | `	sxu32 i;` |
| 445158 |   59 | `	for( i = 0; i < nCacheUsed; i++ ){` |
| 444299 |   60 | `		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){` |
|  36979 |   61 | `			aCache[i].iLastUsed = ++iCacheClock;` |
|  36979 |   62 | `			if( pCaptureCount ){` |
|  36927 |   63 | `				*pCaptureCount = aCache[i].nCaptureCount;` |
|  18461 |   64 | `			}` |
|  36979 |   65 | `			return aCache[i].pCode;` |
|      - |   66 | `		}` |
| 203665 |   67 | `	}` |
|    864 |   68 | `	return 0;` |
|  18921 |   69 | `}` |
|      - |   70 |  |
|    669 |   71 | `static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)` |
|      5 |   72 | `{` |
|      - |   73 | `	PcreCacheEntry *pEntry;` |
|      - |   74 | `	char *zCopy;` |
|      - |   75 | `	/* Allocate the pattern copy first, before touching the cache */` |
|    674 |   76 | `	zCopy = (char *)malloc(nLen + 1);` |
|    674 |   77 | `	if( zCopy == 0 ){` |
|      - |   78 | `		/* OOM — pCode is not cached; it leaks but remains usable by the caller */` |
|    ! 0 |   79 | `		return;` |
|      - |   80 | `	}` |
|    674 |   81 | `	SyMemcpy(zPattern, zCopy, nLen);` |
|    674 |   82 | `	zCopy[nLen] = 0;` |
|    674 |   83 | `	if( nCacheUsed < PCRE_CACHE_SIZE ){` |
|    346 |   84 | `		pEntry = &aCache[nCacheUsed++];` |
|    175 |   85 | `	}else{` |
|      - |   86 | `		/* Evict LRU */` |
|    329 |   87 | `		sxu32 iMin = aCache[0].iLastUsed;` |
|    329 |   88 | `		sxu32 iMinIdx = 0;` |
|      - |   89 | `		sxu32 i;` |
|   5249 |   90 | `		for( i = 1; i < PCRE_CACHE_SIZE; i++ ){` |
|   4921 |   91 | `			if( aCache[i].iLastUsed < iMin ){` |
|    543 |   92 | `				iMin = aCache[i].iLastUsed;` |
|    543 |   93 | `				iMinIdx = i;` |
|    271 |   94 | `			}` |
|   2461 |   95 | `		}` |
|    329 |   96 | `		pEntry = &aCache[iMinIdx];` |
|    329 |   97 | `		pcre2_code_free(pEntry->pCode);` |
|    329 |   98 | `		free(pEntry->zPattern);` |
|      - |   99 | `	}` |
|    674 |  100 | `	pEntry->zPattern = zCopy;` |
|    674 |  101 | `	pEntry->nLen = nLen;` |
|    674 |  102 | `	pEntry->pCode = pCode;` |
|    674 |  103 | `	pEntry->nCaptureCount = nCaptureCount;` |
|    674 |  104 | `	pEntry->iLastUsed = ++iCacheClock;` |
|    339 |  105 | `}` |
|      - |  106 |  |
|      - |  107 | `/* ===== Delimiter parser =====` |
|      - |  108 | ` *` |
|      - |  109 | ` * php reads the pattern STRING before it ever reaches PCRE2: leading` |
|      - |  110 | ` * whitespace, one delimiter byte, the body up to its close, then the` |
|      - |  111 | ` * modifier letters. Two rules in there are easy to get wrong and both` |
|      - |  112 | ` * mis-match SILENTLY rather than refusing:` |
|      - |  113 | ` *` |
|      - |  114 | ` *  - the four bracket delimiters scan for their MATCHING close, counting` |
|      - |  115 | `` *    nesting, so `{^a{2}$}` is the whole `^a{2}$` and not `^a{2` (which`` |
|      - |  116 | ` *    compiles fine as a literal and then never matches);` |
|      - |  117 | ` *  - php's leading-whitespace skip is isspace(), not "every byte <= 0x20",` |
|      - |  118 | ` *    so "\x01^a$\x01" is a pattern delimited by \x01 and a NUL delimiter is` |
|      - |  119 | ` *    refused rather than skipped over.` |
|      - |  120 | ` */` |
|      - |  121 | `#define PCRE_PARSE_OK             0` |
|      - |  122 | `#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */` |
|      - |  123 | `#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or NUL delimiter */` |
|      - |  124 | `#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */` |
|      - |  125 | `#define PCRE_PARSE_BAD_MODIFIER   4  /* A letter after the close php does not know */` |
|      - |  126 |  |
|    859 |  127 | `static sxi32 PcreParsePattern(` |
|      - |  128 | `	const char *zInput, int nInputLen,` |
|      - |  129 | `	const char **pPattern, int *pnPatternLen,` |
|      - |  130 | `	const char **pFlags, int *pnFlagLen,` |
|      - |  131 | `	char *pCloseDelim, int *pbPaired)` |
|      5 |  132 | `{` |
|    864 |  133 | `	const char *zEnd = &zInput[nInputLen];` |
|    864 |  134 | `	const char *z = zInput;` |
|      - |  135 | `	char cOpen, cClose;` |
|      - |  136 | `	const char *pStart;` |
|      - |  137 | `	int nDepth;` |
|      - |  138 |  |
|      - |  139 | `	/* Delimiter details for a "no ending delimiter" diagnostic (php names it) */` |
|    864 |  140 | `	*pCloseDelim = 0;` |
|    864 |  141 | `	*pbPaired = 0;` |
|      - |  142 | `	/* Skip leading whitespace -- php's isspace() set exactly */` |
|    880 |  143 | `	while( z < zEnd && SyisSpace((unsigned char)*z) ){` |
|     17 |  144 | `		z++;` |
|      1 |  145 | `	}` |
|    864 |  146 | `	if( z >= zEnd ){` |
|      5 |  147 | `		return PCRE_PARSE_EMPTY;` |
|      - |  148 | `	}` |
|    860 |  149 | `	cOpen = *z;` |
|      - |  150 | `	/* Must not be alphanumeric, backslash, or NUL. Any other control byte IS a` |
|      - |  151 | `	 * delimiter here -- only the six isspace() bytes were consumed above. */` |
|    860 |  152 | `	if( cOpen == 0 \|\| SyisAlphaNum((unsigned char)cOpen) \|\| cOpen == '\\' ){` |
|     14 |  153 | `		return PCRE_PARSE_BAD_DELIMITER;` |
|      - |  154 | `	}` |
|      - |  155 | `	/* Paired delimiters. A CLOSING bracket opens a pattern of its own and` |
|      - |  156 | ``	 * closes on itself -- `)^a$)` is php's, and it scans without nesting. */`` |
|    848 |  157 | `	switch( cOpen ){` |
|     18 |  158 | `		case '(': cClose = ')'; break;` |
|      8 |  159 | `		case '[': cClose = ']'; break;` |
|     32 |  160 | `		case '{': cClose = '}'; break;` |
|      6 |  161 | `		case '<': cClose = '>'; break;` |
|    792 |  162 | `		default:  cClose = cOpen; break;` |
|      - |  163 | `	}` |
|    848 |  164 | `	*pCloseDelim = cClose;` |
|    848 |  165 | `	*pbPaired = (cOpen != cClose);` |
|    848 |  166 | `	z++; /* Skip opening delimiter */` |
|    848 |  167 | `	pStart = z;` |
|      - |  168 | `	/* Scan for the MATCHING close, respecting backslash escapes: every unescaped` |
|      - |  169 | `	 * open raises the nesting level and every unescaped close lowers it. This is` |
|      - |  170 | `	 * a bracket count and nothing else -- a close inside a character class or a` |
|      - |  171 | ``	 * quantifier still counts, which is why `{[{}]}` is the pattern `[{}]`.`` |
|      - |  172 | `	 * An unpaired delimiter falls out of the same loop with no nesting to count:` |
|      - |  173 | `	 * its close IS its open, so the first one ends the body. */` |
|    848 |  174 | `	nDepth = 1;` |
|  22840 |  175 | `	while( z < zEnd ){` |
|  22772 |  176 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|   1680 |  177 | `			z += 2; /* Skip escaped char */` |
|   1680 |  178 | `			continue;` |
|      - |  179 | `		}` |
|  21097 |  180 | `		if( *z == cClose && --nDepth <= 0 ){` |
|    780 |  181 | `			break;` |
|      - |  182 | `		}` |
|  20322 |  183 | `		if( *z == cOpen ){` |
|     47 |  184 | `			nDepth++;` |
|     23 |  185 | `		}` |
|  20322 |  186 | `		z++;` |
|      5 |  187 | `	}` |
|    848 |  188 | `	if( z >= zEnd ){` |
|     70 |  189 | `		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */` |
|      - |  190 | `	}` |
|    780 |  191 | `	*pPattern = pStart;` |
|    780 |  192 | `	*pnPatternLen = (int)(z - pStart);` |
|    780 |  193 | `	z++; /* Skip closing delimiter */` |
|    780 |  194 | `	*pFlags = z;` |
|    780 |  195 | `	*pnFlagLen = (int)(zEnd - z);` |
|    780 |  196 | `	return PH7_OK;` |
|    434 |  197 | `}` |
|      - |  198 |  |
|      - |  199 | `/* ===== Flag mapper =====` |
|      - |  200 | ` *` |
|      - |  201 | ` * php SCREENS the modifiers and refuses the pattern on the first byte it does` |
|      - |  202 | `` * not know, before PCRE2 is asked to compile anything -- so `)(\d+))`, whose`` |
|      - |  203 | `` * body is `(\d+`, is "Unknown modifier ')'" and never a compile failure.`` |
|      - |  204 | ` * Space, LF and CR are ignored (a heredoc'd pattern keeps its newline); TAB,` |
|      - |  205 | ` * VT and FF are not.` |
|      - |  206 | ` */` |
|    775 |  207 | `static sxi32 PcreMapFlags(` |
|      - |  208 | `	const char *zFlags, int nFlagLen,` |
|      - |  209 | `	uint32_t *pCompileOpts, unsigned char *pBadFlag)` |
|      5 |  210 | `{` |
|      - |  211 | `	int i;` |
|    780 |  212 | `	*pCompileOpts = 0;` |
|    780 |  213 | `	*pBadFlag = 0;` |
|   1019 |  214 | `	for( i = 0; i < nFlagLen; i++ ){` |
|    332 |  215 | `		switch( zFlags[i] ){` |
|     40 |  216 | `			case 'i': *pCompileOpts \|= PCRE2_CASELESS; break;` |
|     49 |  217 | `			case 'm': *pCompileOpts \|= PCRE2_MULTILINE; break;` |
|    100 |  218 | `			case 's': *pCompileOpts \|= PCRE2_DOTALL; break;` |
|      3 |  219 | `			case 'x': *pCompileOpts \|= PCRE2_EXTENDED; break;` |
|     24 |  220 | `			case 'u': *pCompileOpts \|= PCRE2_UTF \| PCRE2_UCP; break;` |
|      3 |  221 | `			case 'A': *pCompileOpts \|= PCRE2_ANCHORED; break;` |
|     18 |  222 | `			case 'D': *pCompileOpts \|= PCRE2_DOLLAR_ENDONLY; break;` |
|      3 |  223 | `			case 'U': *pCompileOpts \|= PCRE2_UNGREEDY; break;` |
|      3 |  224 | `			case 'J': *pCompileOpts \|= PCRE2_DUPNAMES; break;` |
|      9 |  225 | `			case 'n': *pCompileOpts \|= PCRE2_NO_AUTO_CAPTURE; break;` |
|      3 |  226 | `			case 'S': /* Study hint — no-op in PCRE2 */ break;` |
|      3 |  227 | `			case 'X': /* PCRE1's "extra" strictness — no-op in PCRE2 */ break;` |
|      9 |  228 | `			case ' ': case '\n': case '\r': /* php ignores these three */ break;` |
|     44 |  229 | `			default:` |
|     89 |  230 | `				*pBadFlag = (unsigned char)zFlags[i];` |
|     89 |  231 | `				return PCRE_PARSE_BAD_MODIFIER;` |
|      - |  232 | `		}` |
|    124 |  233 | `	}` |
|    692 |  234 | `	return PH7_OK;` |
|    392 |  235 | `}` |
|      - |  236 |  |
|      - |  237 | `/* ===== Compile helper =====` |
|      - |  238 | ` *` |
|      - |  239 | ` * PcreCompileQuiet is the whole of it; PcreCompile is that plus php's E_WARNING.` |
|      - |  240 | ` * The split exists because a caller may have to WORD the failure itself: php's` |
|      - |  241 | ` * SPL wraps the compile in zend_replace_error_handling(EH_THROW,` |
|      - |  242 | `` * InvalidArgumentException), so `new RegexIterator($it, 'nodelim')` raises an`` |
|      - |  243 | ` * exception carrying this exact text instead of warning. Nothing else may` |
|      - |  244 | ` * reproduce these six messages -- they are php's, verbatim, in one place.` |
|      - |  245 | ` */` |
|  37833 |  246 | `static pcre2_code *PcreCompileQuiet(` |
|      - |  247 | `	ph7_vm *pVm,` |
|      - |  248 | `	const char *zFullPattern, int nLen,` |
|      - |  249 | `	sxu32 *pCaptureCount,` |
|      - |  250 | `	char *zErr, sxu32 nErr)` |
|      5 |  251 | `{` |
|      - |  252 | `	const char *zPat, *zFlags;` |
|      - |  253 | `	int nPatLen, nFlagLen;` |
|      - |  254 | `	uint32_t compileOpts;` |
|      - |  255 | `	pcre2_code *pCode;` |
|      - |  256 | `	PCRE2_SIZE erroffset;` |
|      - |  257 | `	int errcode;` |
|      - |  258 | `	sxu32 nCapture;` |
|      - |  259 | `	sxi32 parseRc;` |
|      - |  260 | `	char cDelim;` |
|      - |  261 | `	int bPaired;` |
|      - |  262 | `	unsigned char cBadFlag;` |
|      - |  263 |  |
|  37838 |  264 | `	if( nErr > 0 ){` |
|  37838 |  265 | `		zErr[0] = 0;` |
|  18916 |  266 | `	}` |
|      - |  267 | `	/* Check cache first */` |
|  37838 |  268 | `	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);` |
|  37838 |  269 | `	if( pCode ){` |
|  36979 |  270 | `		return pCode;` |
|      - |  271 | `	}` |
|      - |  272 | `	/* Parse delimiter */` |
|    864 |  273 | `	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen,` |
|      - |  274 | `		&cDelim, &bPaired);` |
|    864 |  275 | `	if( parseRc != PCRE_PARSE_OK ){` |
|     86 |  276 | `		if( parseRc == PCRE_PARSE_EMPTY ){` |
|      5 |  277 | `			SyBufferFormat(zErr, nErr, "Empty regular expression");` |
|     84 |  278 | `		}else if( parseRc == PCRE_PARSE_BAD_DELIMITER ){` |
|     14 |  279 | `			SyBufferFormat(zErr, nErr,` |
|      - |  280 | `				"Delimiter must not be alphanumeric, backslash, or NUL byte");` |
|      8 |  281 | `		}else{` |
|      - |  282 | `			/* php names the delimiter, and distinguishes paired delimiters */` |
|    104 |  283 | `			SyBufferFormat(zErr, nErr,` |
|     68 |  284 | `				bPaired ? "No ending matching delimiter '%c' found"` |
|     34 |  285 | `				        : "No ending delimiter '%c' found", cDelim);` |
|      - |  286 | `		}` |
|     86 |  287 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|     86 |  288 | `		return 0;` |
|      - |  289 | `	}` |
|      - |  290 | `	/* Screen the modifiers. php refuses here, BEFORE the compile, so a pattern` |
|      - |  291 | `	 * that is bad in both ways is reported as the modifier php read first. */` |
|    780 |  292 | `	if( PcreMapFlags(zFlags, nFlagLen, &compileOpts, &cBadFlag) != PCRE_PARSE_OK ){` |
|     89 |  293 | `		if( cBadFlag == 0 ){` |
|      5 |  294 | `			SyBufferFormat(zErr, nErr, "NUL byte is not a valid modifier");` |
|      3 |  295 | `		}else{` |
|     85 |  296 | `			SyBufferFormat(zErr, nErr, "Unknown modifier '%c'", (int)cBadFlag);` |
|      - |  297 | `		}` |
|     89 |  298 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|     89 |  299 | `		return 0;` |
|      - |  300 | `	}` |
|      - |  301 | `	/* Compile */` |
|    692 |  302 | `	pCode = pcre2_compile(` |
|    343 |  303 | `		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,` |
|    343 |  304 | `		compileOpts, &errcode, &erroffset, NULL);` |
|    692 |  305 | `	if( pCode == 0 ){` |
|      - |  306 | `		PCRE2_UCHAR errbuf[256];` |
|     20 |  307 | `		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));` |
|     29 |  308 | `		SyBufferFormat(zErr, nErr,` |
|      9 |  309 | `			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);` |
|     20 |  310 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|     20 |  311 | `		return 0;` |
|      - |  312 | `	}` |
|      - |  313 | `	/* Get capture count */` |
|    674 |  314 | `	nCapture = 0;` |
|    674 |  315 | `	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|    674 |  316 | `	if( pCaptureCount ){` |
|    640 |  317 | `		*pCaptureCount = nCapture;` |
|    317 |  318 | `	}` |
|      - |  319 | `	/* Cache it */` |
|    674 |  320 | `	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);` |
|    674 |  321 | `	pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    674 |  322 | `	return pCode;` |
|  18921 |  323 | `}` |
|  37741 |  324 | `static pcre2_code *PcreCompile(` |
|      - |  325 | `	ph7_context *pCtx,` |
|      - |  326 | `	const char *zFullPattern, int nLen,` |
|      - |  327 | `	sxu32 *pCaptureCount)` |
|      5 |  328 | `{` |
|      - |  329 | `	char zErr[288];` |
|  56616 |  330 | `	pcre2_code *pCode = PcreCompileQuiet(pCtx->pVm, zFullPattern, nLen, pCaptureCount,` |
|  18870 |  331 | `		zErr, sizeof(zErr));` |
|  37746 |  332 | `	if( pCode == 0 && zErr[0] ){` |
|    187 |  333 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, zErr);` |
|     92 |  334 | `	}` |
|  37746 |  335 | `	return pCode;` |
|      5 |  336 | `}` |
|      - |  337 | `/*` |
|      - |  338 | ` * Validate a pattern for a caller that raises its own diagnostic (php's SPL` |
|      - |  339 | ` * promotes the warning to an InvalidArgumentException). Answers TRUE when the` |
|      - |  340 | ` * pattern compiles; otherwise FALSE with php's text in zErr. The compiled code` |
|      - |  341 | ` * stays in the pattern cache, so a later match pays nothing for this.` |
|      - |  342 | ` */` |
|     92 |  343 | `PH7_PRIVATE int PH7_PcrePatternCheck(ph7_vm *pVm, const char *zPattern, int nLen,` |
|      - |  344 | `	char *zErr, sxu32 nErr)` |
|      2 |  345 | `{` |
|     94 |  346 | `	return PcreCompileQuiet(&(*pVm), zPattern, nLen, 0, zErr, nErr) != 0;` |
|      2 |  347 | `}` |
|      - |  348 |  |
|      - |  349 | `/* ===== Map PCRE2 match error to PHP error code ===== */` |
|    ! 0 |  350 | `static void PcreSetMatchError(ph7_vm *pVm, int rc)` |
|    ! 0 |  351 | `{` |
|    ! 0 |  352 | `	if( rc == PCRE2_ERROR_NOMATCH ){` |
|    ! 0 |  353 | `		pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    ! 0 |  354 | `	}else if( rc == PCRE2_ERROR_MATCHLIMIT ){` |
|    ! 0 |  355 | `		pVm->iPcreLastError = PHP_PREG_BACKTRACK_LIMIT_ERROR;` |
|    ! 0 |  356 | `	}else if( rc == PCRE2_ERROR_DEPTHLIMIT` |
|      - |  357 | `#ifdef PCRE2_ERROR_RECURSIONLIMIT` |
|    ! 0 |  358 | `		\|\| rc == PCRE2_ERROR_RECURSIONLIMIT` |
|      - |  359 | `#endif` |
|      - |  360 | `	){` |
|    ! 0 |  361 | `		pVm->iPcreLastError = PHP_PREG_RECURSION_LIMIT_ERROR;` |
|    ! 0 |  362 | `	}else if( rc == PCRE2_ERROR_BADUTFOFFSET ){` |
|    ! 0 |  363 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_OFFSET_ERROR;` |
|    ! 0 |  364 | `	}else if( rc == PCRE2_ERROR_UTF8_ERR1` |
|    ! 0 |  365 | `		\|\| rc == PCRE2_ERROR_UTF8_ERR2 ){` |
|    ! 0 |  366 | `		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_ERROR;` |
|      - |  367 | `#ifdef PCRE2_ERROR_JIT_STACKLIMIT` |
|    ! 0 |  368 | `	}else if( rc == PCRE2_ERROR_JIT_STACKLIMIT ){` |
|    ! 0 |  369 | `		pVm->iPcreLastError = PHP_PREG_JIT_STACKLIMIT_ERROR;` |
|      - |  370 | `#endif` |
|    ! 0 |  371 | `	}else{` |
|    ! 0 |  372 | `		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|      - |  373 | `	}` |
|    ! 0 |  374 | `}` |
|      - |  375 |  |
|      - |  376 | `/* ===== Helper: populate matches array from ovector ===== */` |
|      - |  377 | `/*` |
|      - |  378 | ` * Where the scan resumes after a ZERO-WIDTH match at nAt.` |
|      - |  379 | ` *` |
|      - |  380 | ` * php steps on by one CHARACTER, which under a UTF pattern is the whole` |
|      - |  381 | ` * multi-byte sequence: stepping one BYTE lands INSIDE it, and pcre2_match then` |
|      - |  382 | ` * refuses the offset (PCRE2_ERROR_BADUTFOFFSET) and the scan simply stops. So` |
|      - |  383 | `` * `preg_split('/(?<!^)(?!$)/u', 'éÄßご')` — the unicode str_split every library`` |
|      - |  384 | `` * writes, twig's `split` filter and its `random()` among them — answered TWO`` |
|      - |  385 | ` * pieces (the first character, then all the rest) instead of four, and the same` |
|      - |  386 | ` * one-byte step truncated preg_replace/preg_replace_callback/preg_match_all on` |
|      - |  387 | ` * any non-ASCII subject. A continuation byte is 10xxxxxx, so skipping them is` |
|      - |  388 | ` * the whole rule; without PCRE2_UTF the unit is the byte, as php's is.` |
|      - |  389 | ` */` |
|   1232 |  390 | `static PCRE2_SIZE PcreEmptyMatchNext(pcre2_code *pCode, const char *zSubject,` |
|      - |  391 | `	int nSubLen, PCRE2_SIZE nAt)` |
|      1 |  392 | `{` |
|   1233 |  393 | `	uint32_t nOpts = 0;` |
|   1233 |  394 | `	PCRE2_SIZE n = nAt + 1;` |
|   1233 |  395 | `	pcre2_pattern_info(pCode, PCRE2_INFO_ALLOPTIONS, &nOpts);` |
|   1233 |  396 | `	if( nOpts & PCRE2_UTF ){` |
|   1079 |  397 | `		while( n < (PCRE2_SIZE)nSubLen && (((unsigned char)zSubject[n]) & 0xC0) == 0x80 ){` |
|    357 |  398 | `			n++;` |
|      1 |  399 | `		}` |
|    361 |  400 | `	}` |
|   1233 |  401 | `	return n;` |
|      1 |  402 | `}` |
|      - |  403 | `/*` |
|      - |  404 | ` * php reports the NAME of the (*MARK)/(*:NAME) verb the successful match path last` |
|      - |  405 | ` * passed through, under the string key "MARK". The key exists only when the match` |
|      - |  406 | ` * actually reached a mark -- a pattern that HAS marks but matched down an unmarked` |
|      - |  407 | ` * branch has no MARK entry at all -- and it is a plain string even under` |
|      - |  408 | ` * PREG_OFFSET_CAPTURE, where every other entry is a [value, offset] pair.` |
|      - |  409 | ` *` |
|      - |  410 | ` * phpstan/phpdoc-parser's lexer is built on it: one alternation of ~50 marked` |
|      - |  411 | `` * branches, and `(int) $match['MARK']` is how it names the token it just read. With`` |
|      - |  412 | ` * no MARK key that read is an "Undefined array key" warning per token, which is why` |
|      - |  413 | ` * every slevomat sniff that parses a docblock died and phpcs reported an internal` |
|      - |  414 | ` * exception for each file.` |
|      - |  415 | ` */` |
|   1006 |  416 | `static void PcreAddMark(ph7_context *pCtx,ph7_value *pArray,pcre2_match_data *pMatchData)` |
|      5 |  417 | `{` |
|   1011 |  418 | `	PCRE2_SPTR zMark = pMatchData ? pcre2_get_mark(pMatchData) : 0;` |
|      - |  419 | `	ph7_value *pVal;` |
|   1011 |  420 | `	if( zMark == 0 ){` |
|   1011 |  421 | `		return;` |
|      - |  422 | `	}` |
|    ! 0 |  423 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    ! 0 |  424 | `	if( pVal == 0 ){` |
|    ! 0 |  425 | `		return;` |
|      - |  426 | `	}` |
|    ! 0 |  427 | `	ph7_value_string(pVal,(const char *)zMark,-1);` |
|    ! 0 |  428 | `	ph7_array_add_strkey_elem(pArray,"MARK",pVal);` |
|    ! 0 |  429 | `	ph7_context_release_value(pCtx,pVal);` |
|    508 |  430 | `}` |
|   1006 |  431 | `static void PcrePopulateMatches(` |
|      - |  432 | `	ph7_context *pCtx,` |
|      - |  433 | `	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */` |
|      - |  434 | `	const char *zSubject,` |
|      - |  435 | `	PCRE2_SIZE *ovector,` |
|      - |  436 | `	int nGroups,` |
|      - |  437 | `	pcre2_code *pCode,` |
|      - |  438 | `	pcre2_match_data *pMatchData, /* for the MARK entry; may be 0 */` |
|      - |  439 | `	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */` |
|      5 |  440 | `{` |
|   1011 |  441 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   1011 |  442 | `	ph7_value *pSub = 0;` |
|   1011 |  443 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|   1011 |  444 | `	PCRE2_SPTR nametable = 0;` |
|   1011 |  445 | `	int nMatched = nGroups;` |
|      - |  446 | `	int i;` |
|      - |  447 |  |
|   1011 |  448 | `	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|    257 |  449 | `		pSub = ph7_context_new_array(pCtx);` |
|    127 |  450 | `	}` |
|   1011 |  451 | `	if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|      - |  452 | `		/* pcre2 answers only as many pairs as the LAST group that participated, so` |
|      - |  453 | `		 * a pattern's trailing optional groups are simply absent -- which is php's` |
|      - |  454 | `		 * default shape too. PREG_UNMATCHED_AS_NULL is the flag that says "report` |
|      - |  455 | `		 * every group", so the tail is filled out to the pattern's own capture` |
|      - |  456 | `		 * count and each missing one answers NULL. Reading the flag only INSIDE the` |
|      - |  457 | ``		 * loop meant `preg_match('/(a)(x)?/','a',$m,PREG_UNMATCHED_AS_NULL)` still`` |
|      - |  458 | `		 * answered two entries where php answers three. */` |
|     39 |  459 | `		uint32_t nCapture = 0;` |
|     39 |  460 | `		pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);` |
|     39 |  461 | `		if( (int)nCapture + 1 > nGroups ){` |
|     23 |  462 | `			nGroups = (int)nCapture + 1;` |
|     11 |  463 | `		}` |
|     18 |  464 | `	}` |
|      - |  465 | `	/* Read the name table up front so each group's named key can be emitted` |
|      - |  466 | `	 * INTERLEAVED with its numbered key, in group order — php stores` |
|      - |  467 | ``	 * `0, name, 1, value, 2` (named entry immediately before its number), not`` |
|      - |  468 | `	 * every number followed by every name. Code that iterates $matches or` |
|      - |  469 | `	 * var_dumps it (PHPUnit's annotation parser) depends on this order. */` |
|   1011 |  470 | `	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|   1011 |  471 | `	if( namecount > 0 ){` |
|     29 |  472 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|     29 |  473 | `		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|     14 |  474 | `	}` |
|   2493 |  475 | `	for( i = 0; i < nGroups; i++ ){` |
|   1487 |  476 | `		PCRE2_SIZE start = i < nMatched ? ovector[2 * i]     : PCRE2_UNSET;` |
|   1487 |  477 | `		PCRE2_SIZE end   = i < nMatched ? ovector[2 * i + 1] : PCRE2_UNSET;` |
|   1487 |  478 | `		const char *zName = 0;` |
|      - |  479 | `		/* Does group i carry a (?<name>...) label? namecount is tiny in practice. */` |
|   1487 |  480 | `		if( namecount > 0 ){` |
|      - |  481 | `			uint32_t k;` |
|    149 |  482 | `			for( k = 0; k < namecount; k++ ){` |
|    119 |  483 | `				PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|    119 |  484 | `				if( (((entry[0] << 8) \| entry[1])) == i ){` |
|     49 |  485 | `					zName = (const char *)(entry + 2);` |
|     49 |  486 | `					break;` |
|      - |  487 | `				}` |
|     36 |  488 | `			}` |
|     39 |  489 | `		}` |
|   1487 |  490 | `		if( start == PCRE2_UNSET ){` |
|     43 |  491 | `			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|     39 |  492 | `				ph7_value_null(pVal);` |
|     20 |  493 | `			}else{` |
|      5 |  494 | `				ph7_value_string(pVal, "", 0);` |
|      - |  495 | `			}` |
|      - |  496 | ``			/* Duplicate group names -- `(?J)` -- give two numbered groups one key,`` |
|      - |  497 | `			 * and only one of them can have participated. php writes a name key` |
|      - |  498 | `			 * from a group that did NOT participate only when nothing is there` |
|      - |  499 | ``			 * yet, so `/(?J)(?<d>a)\|(?<d>b)/` on "a" keeps `d => "a"` instead of`` |
|      - |  500 | `			 * having the other alternative's NULL land on top of it. */` |
|     43 |  501 | `			if( zName && ph7_array_fetch(pArray, zName, -1) != 0 ){` |
|      9 |  502 | `				zName = 0;` |
|      4 |  503 | `			}` |
|     22 |  504 | `		}else{` |
|   1445 |  505 | `			ph7_value_string(pVal, &zSubject[start], (int)(end - start));` |
|      - |  506 | `		}` |
|   1487 |  507 | `		if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|    277 |  508 | `			ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|    277 |  509 | `			ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|    277 |  510 | `			ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);` |
|    277 |  511 | `			ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|      - |  512 | `			/* php: the named key comes first, then the numbered key (same value). */` |
|    277 |  513 | `			if( zName ){` |
|      3 |  514 | `				ph7_array_add_strkey_elem(pArray, zName, pSub);` |
|      1 |  515 | `			}` |
|    277 |  516 | `			ph7_array_add_intkey_elem(pArray, i, pSub);` |
|    277 |  517 | `			ph7_context_release_value(pCtx, pOff);` |
|    277 |  518 | `			ph7_context_release_value(pCtx, pSub);` |
|    277 |  519 | `			pSub = ph7_context_new_array(pCtx);` |
|    140 |  520 | `		}else{` |
|   1213 |  521 | `			if( zName ){` |
|     39 |  522 | `				ph7_array_add_strkey_elem(pArray, zName, pVal);` |
|     19 |  523 | `			}` |
|   1213 |  524 | `			ph7_array_add_intkey_elem(pArray, i, pVal);` |
|      - |  525 | `		}` |
|   1487 |  526 | `		ph7_value_reset_string_cursor(pVal);` |
|    746 |  527 | `	}` |
|   1011 |  528 | `	ph7_context_release_value(pCtx, pVal);` |
|   1011 |  529 | `	if( pSub ){` |
|    257 |  530 | `		ph7_context_release_value(pCtx, pSub);` |
|    127 |  531 | `	}` |
|      - |  532 | `	/* php appends it AFTER every numbered and named group. */` |
|   1011 |  533 | `	PcreAddMark(pCtx,pArray,pMatchData);` |
|   1011 |  534 | `}` |
|      - |  535 |  |
|      - |  536 | `/*` |
|      - |  537 | ` * Quiet whole-pattern match used by FILTER_VALIDATE_REGEXP: compile zPat (a full` |
|      - |  538 | ` * "/.../flags" pattern) and test it against zSub. On a successful attempt returns` |
|      - |  539 | ` * SXRET_OK with *pMatched set to 1 (match) or 0 (no match); returns SXERR_INVALID` |
|      - |  540 | ` * on a compile/match error (the caller treats that as a validation failure). The` |
|      - |  541 | ` * compiled code is owned by PcreCompile's cache, so it is not freed here.` |
|      - |  542 | ` */` |
|    208 |  543 | `PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,` |
|      - |  544 | `	const char *zSub,int nSub,int *pMatched)` |
|      4 |  545 | `{` |
|      - |  546 | `	pcre2_code *pCode;` |
|      - |  547 | `	pcre2_match_data *pMatchData;` |
|      - |  548 | `	sxu32 nCapture;` |
|      - |  549 | `	int rc;` |
|    212 |  550 | `	*pMatched = 0;` |
|    212 |  551 | `	pCode = PcreCompile(pCtx,zPat,nPat,&nCapture);` |
|    212 |  552 | `	if( pCode == 0 ){` |
|      3 |  553 | `		return SXERR_INVALID;` |
|      - |  554 | `	}` |
|    210 |  555 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode,NULL);` |
|    210 |  556 | `	if( pMatchData == 0 ){` |
|    ! 0 |  557 | `		return SXERR_INVALID;` |
|      - |  558 | `	}` |
|    210 |  559 | `	rc = pcre2_match(pCode,(PCRE2_SPTR)zSub,(PCRE2_SIZE)nSub,0,0,pMatchData,NULL);` |
|    210 |  560 | `	pcre2_match_data_free(pMatchData);` |
|    210 |  561 | `	if( rc < 0 ){` |
|    137 |  562 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|    ! 0 |  563 | `			PcreSetMatchError(pCtx->pVm,rc);` |
|    ! 0 |  564 | `			return SXERR_INVALID;` |
|      - |  565 | `		}` |
|    137 |  566 | `		return SXRET_OK; /* clean no-match */` |
|      - |  567 | `	}` |
|     75 |  568 | `	*pMatched = 1;` |
|     75 |  569 | `	return SXRET_OK;` |
|    108 |  570 | `}` |
|      - |  571 | `/* ======================================================================` |
|      - |  572 | ` * preg_match(pattern, subject [, &matches [, flags [, offset]]])` |
|      - |  573 | ` * ====================================================================== */` |
|  33289 |  574 | `static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  575 | `{` |
|      - |  576 | `	const char *zPattern, *zSubject;` |
|      - |  577 | `	int nPatLen, nSubLen;` |
|      - |  578 | `	pcre2_code *pCode;` |
|      - |  579 | `	pcre2_match_data *pMatchData;` |
|      - |  580 | `	PCRE2_SIZE *ovector;` |
|      - |  581 | `	sxu32 nCapture;` |
|  33294 |  582 | `	PCRE2_SIZE startOffset = 0;` |
|  33294 |  583 | `	int iFlags = 0;` |
|      - |  584 | `	int rc;` |
|      - |  585 |  |
|  33294 |  586 | `	if( nArg < 2 ){` |
|    ! 0 |  587 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|      - |  588 | `			"preg_match() expects at least 2 parameters");` |
|    ! 0 |  589 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  590 | `		return PH7_OK;` |
|      - |  591 | `	}` |
|  33294 |  592 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|  33294 |  593 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|  33294 |  594 | `	if( nArg >= 4 ){` |
|     68 |  595 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     33 |  596 | `	}` |
|  33294 |  597 | `	if( nArg >= 5 ){` |
|    ! 0 |  598 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|    ! 0 |  599 | `	}` |
|  33294 |  600 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|  33294 |  601 | `	if( pCode == 0 ){` |
|    129 |  602 | `		ph7_result_bool(pCtx, 0);` |
|    129 |  603 | `		return PH7_OK;` |
|      - |  604 | `	}` |
|      - |  605 | `	/* php validates $flags AFTER the pattern compiles (a bad pattern warns first).` |
|      - |  606 | `	 * php 8.5 only rejects flag bits BELOW PREG_OFFSET_CAPTURE (the low byte); any` |
|      - |  607 | `	 * higher bit is ignored. preg_match permits none of those low bits. */` |
|  33168 |  608 | `	if( (iFlags & (PHP_PREG_OFFSET_CAPTURE - 1)) != 0 ){` |
|     11 |  609 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  610 | `			"preg_match(): Argument #4 ($flags) must be a PREG_* constant");` |
|      - |  611 | `	}` |
|  33158 |  612 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|  33158 |  613 | `	if( pMatchData == 0 ){` |
|    ! 0 |  614 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  615 | `		return PH7_OK;` |
|      - |  616 | `	}` |
|  49734 |  617 | `	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|  16576 |  618 | `		startOffset, 0, pMatchData, NULL);` |
|  33158 |  619 | `	if( rc < 0 ){` |
|  32548 |  620 | `		if( rc != PCRE2_ERROR_NOMATCH ){` |
|    ! 0 |  621 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|    ! 0 |  622 | `		}` |
|      - |  623 | `		/* Populate empty matches if requested */` |
|  32548 |  624 | `		if( nArg >= 3 ){` |
|  32192 |  625 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|  32192 |  626 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pEmpty);` |
|  32192 |  627 | `			ph7_context_release_value(pCtx, pEmpty);` |
|  16093 |  628 | `		}` |
|  32548 |  629 | `		pcre2_match_data_free(pMatchData);` |
|  32548 |  630 | `		ph7_result_int(pCtx, 0);` |
|  32548 |  631 | `		return PH7_OK;` |
|      - |  632 | `	}` |
|    615 |  633 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|    615 |  634 | `	if( nArg >= 3 ){` |
|      - |  635 | `		/* Populate $matches */` |
|    359 |  636 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    359 |  637 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    359 |  638 | `		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, pMatchData, iFlags);` |
|      - |  639 | `		/* Write the array back to the caller's variable */` |
|    359 |  640 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pArray);` |
|    359 |  641 | `		ph7_context_release_value(pCtx, pArray);` |
|    177 |  642 | `	}` |
|    615 |  643 | `	pcre2_match_data_free(pMatchData);` |
|    615 |  644 | `	ph7_result_int(pCtx, 1);` |
|    615 |  645 | `	return PH7_OK;` |
|  16649 |  646 | `}` |
|      - |  647 |  |
|      - |  648 | `/* ======================================================================` |
|      - |  649 | ` * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])` |
|      - |  650 | ` * ====================================================================== */` |
|    252 |  651 | `static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  652 | `{` |
|      - |  653 | `	const char *zPattern, *zSubject;` |
|      - |  654 | `	int nPatLen, nSubLen;` |
|      - |  655 | `	pcre2_code *pCode;` |
|      - |  656 | `	pcre2_match_data *pMatchData;` |
|      - |  657 | `	sxu32 nCapture;` |
|    254 |  658 | `	PCRE2_SIZE startOffset = 0;` |
|    254 |  659 | `	int iFlags = PHP_PREG_PATTERN_ORDER;` |
|    254 |  660 | `	int totalMatches = 0;` |
|      - |  661 | `	int rc;` |
|      - |  662 |  |
|    254 |  663 | `	if( nArg < 2 ){` |
|    ! 0 |  664 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|      - |  665 | `			"preg_match_all() expects at least 2 parameters");` |
|    ! 0 |  666 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  667 | `		return PH7_OK;` |
|      - |  668 | `	}` |
|    254 |  669 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    254 |  670 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    254 |  671 | `	if( nArg >= 4 ){` |
|    236 |  672 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|    117 |  673 | `	}` |
|    254 |  674 | `	if( nArg >= 5 ){` |
|    ! 0 |  675 | `		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);` |
|    ! 0 |  676 | `	}` |
|    254 |  677 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    254 |  678 | `	if( pCode == 0 ){` |
|      - |  679 | `		/* php answers FALSE, not 0 -- the declared return type is int\|false and a` |
|      - |  680 | `		 * caller cannot tell a refused pattern from "no matches" otherwise. */` |
|      3 |  681 | `		ph7_result_bool(pCtx, 0);` |
|      3 |  682 | `		return PH7_OK;` |
|      - |  683 | `	}` |
|      - |  684 | `	/* php validates $flags AFTER the pattern compiles (a bad pattern warns first).` |
|      - |  685 | `	 * php 8.5 rejects low-byte bits below PREG_OFFSET_CAPTURE EXCEPT the order flags,` |
|      - |  686 | `	 * and rejects PATTERN_ORDER+SET_ORDER together (mutually exclusive); higher bits` |
|      - |  687 | `	 * are ignored. Every case raises the same ValueError. */` |
|    250 |  688 | `	if( (iFlags & (PHP_PREG_OFFSET_CAPTURE - 1)` |
|    250 |  689 | `			& ~(PHP_PREG_PATTERN_ORDER\|PHP_PREG_SET_ORDER)) != 0` |
|    250 |  690 | `		\|\| ((iFlags & PHP_PREG_PATTERN_ORDER) && (iFlags & PHP_PREG_SET_ORDER)) ){` |
|      9 |  691 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  692 | `			"preg_match_all(): Argument #4 ($flags) must be a PREG_* constant");` |
|      - |  693 | `	}` |
|    244 |  694 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    244 |  695 | `	if( pMatchData == 0 ){` |
|    ! 0 |  696 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  697 | `		return PH7_OK;` |
|      - |  698 | `	}` |
|    244 |  699 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|      - |  700 | `	{` |
|    244 |  701 | `		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;` |
|      - |  702 |  |
|    244 |  703 | `		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){` |
|    362 |  704 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|      - |  705 | `				PCRE2_SIZE *ovector;` |
|    479 |  706 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    159 |  707 | `					startOffset, 0, pMatchData, NULL);` |
|    320 |  708 | `				if( rc < 0 ){` |
|     68 |  709 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     68 |  710 | `					break;` |
|      - |  711 | `				}` |
|    254 |  712 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    254 |  713 | `				if( pOutArray ){` |
|    254 |  714 | `					ph7_value *pSet = ph7_context_new_array(pCtx);` |
|    254 |  715 | `					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, pMatchData, iFlags & ~0xFF);` |
|    254 |  716 | `					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);` |
|    254 |  717 | `					ph7_context_release_value(pCtx, pSet);` |
|    126 |  718 | `				}` |
|    254 |  719 | `				if( ovector[1] == ovector[0] ){` |
|    237 |  720 | `					startOffset = PcreEmptyMatchNext(pCode, zSubject, nSubLen, ovector[0]);` |
|    119 |  721 | `				}else{` |
|     18 |  722 | `					startOffset = ovector[1];` |
|      - |  723 | `				}` |
|    254 |  724 | `				totalMatches++;` |
|      2 |  725 | `			}` |
|     56 |  726 | `		}else{` |
|      - |  727 | `			/* PREG_PATTERN_ORDER (default) */` |
|    136 |  728 | `			ph7_value **apGroupArrays = 0;` |
|      - |  729 | `			/* The marks the successful paths passed through, in match order. php` |
|      - |  730 | `			 * appends them as one "MARK" array after the numbered groups -- and it` |
|      - |  731 | `			 * holds ONLY the matches that reached a mark, renumbered from 0, so it is` |
|      - |  732 | `			 * not aligned with the group arrays beside it. A run where nothing was` |
|      - |  733 | `			 * marked has no MARK key at all. */` |
|    136 |  734 | `			ph7_value *pMarkArray = 0;` |
|    136 |  735 | `			sxu32 nGroups = nCapture + 1;` |
|      - |  736 | `			sxu32 g;` |
|    136 |  737 | `			if( pOutArray ){` |
|    203 |  738 | `				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,` |
|     67 |  739 | `					sizeof(ph7_value *) * nGroups, TRUE, FALSE);` |
|    136 |  740 | `				if( apGroupArrays ){` |
|    304 |  741 | `					for( g = 0; g < nGroups; g++ ){` |
|    170 |  742 | `						apGroupArrays[g] = ph7_context_new_array(pCtx);` |
|     86 |  743 | `					}` |
|     67 |  744 | `				}` |
|     67 |  745 | `			}` |
|    494 |  746 | `			while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|      - |  747 | `				PCRE2_SIZE *ovector;` |
|    677 |  748 | `				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    225 |  749 | `					startOffset, 0, pMatchData, NULL);` |
|    452 |  750 | `				if( rc < 0 ){` |
|     94 |  751 | `					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);` |
|     94 |  752 | `					break;` |
|      - |  753 | `				}` |
|    360 |  754 | `				ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    360 |  755 | `				if( apGroupArrays ){` |
|    360 |  756 | `					PCRE2_SPTR zMark = pcre2_get_mark(pMatchData);` |
|    360 |  757 | `					ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    360 |  758 | `					int nActual = rc;` |
|    360 |  759 | `					if( zMark ){` |
|    ! 0 |  760 | `						if( pMarkArray == 0 ){` |
|    ! 0 |  761 | `							pMarkArray = ph7_context_new_array(pCtx);` |
|    ! 0 |  762 | `						}` |
|    ! 0 |  763 | `						if( pMarkArray ){` |
|    ! 0 |  764 | `							ph7_value_string(pVal,(const char *)zMark,-1);` |
|    ! 0 |  765 | `							ph7_array_add_elem(pMarkArray,0,pVal);` |
|    ! 0 |  766 | `							ph7_value_reset_string_cursor(pVal);` |
|    ! 0 |  767 | `						}` |
|    ! 0 |  768 | `					}` |
|    838 |  769 | `					for( g = 0; g < nGroups; g++ ){` |
|    711 |  770 | `						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){` |
|    464 |  771 | `							PCRE2_SIZE s = ovector[2*g];` |
|    464 |  772 | `							PCRE2_SIZE e = ovector[2*g+1];` |
|    464 |  773 | `							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|     20 |  774 | `								ph7_value *pSub = ph7_context_new_array(pCtx);` |
|     20 |  775 | `								ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|     20 |  776 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|     20 |  777 | `								ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|     20 |  778 | `								ph7_value_int(pOff, (int)s);` |
|     20 |  779 | `								ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|     20 |  780 | `								ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|     20 |  781 | `								ph7_context_release_value(pCtx, pSub);` |
|     20 |  782 | `								ph7_context_release_value(pCtx, pOff);` |
|     11 |  783 | `							}else{` |
|    446 |  784 | `								ph7_value_string(pVal, &zSubject[s], (int)(e - s));` |
|    446 |  785 | `								ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|      2 |  786 | `							}` |
|    248 |  787 | `						}else if( iFlags & PHP_PREG_OFFSET_CAPTURE ){` |
|      - |  788 | `							/* php reports an unmatched group as the pair ("", -1) --` |
|      - |  789 | `							 * or (NULL, -1) under PREG_UNMATCHED_AS_NULL. Both flags` |
|      - |  790 | `							 * were read only on the MATCHED arm, so an unmatched` |
|      - |  791 | `							 * group answered a bare "" whatever was asked for. */` |
|      9 |  792 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|      9 |  793 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|      9 |  794 | `							if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|      5 |  795 | `								ph7_value_null(pVal);` |
|      3 |  796 | `							}else{` |
|      5 |  797 | `								ph7_value_string(pVal, "", 0);` |
|      - |  798 | `							}` |
|      9 |  799 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|      9 |  800 | `							ph7_value_int(pOff, -1);` |
|      9 |  801 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|      9 |  802 | `							ph7_array_add_elem(apGroupArrays[g], 0, pSub);` |
|      9 |  803 | `							ph7_context_release_value(pCtx, pSub);` |
|      9 |  804 | `							ph7_context_release_value(pCtx, pOff);` |
|      5 |  805 | `						}else{` |
|      9 |  806 | `							if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){` |
|      7 |  807 | `								ph7_value_null(pVal);` |
|      4 |  808 | `							}else{` |
|      3 |  809 | `								ph7_value_string(pVal, "", 0);` |
|      - |  810 | `							}` |
|      9 |  811 | `							ph7_array_add_elem(apGroupArrays[g], 0, pVal);` |
|      - |  812 | `						}` |
|    480 |  813 | `						ph7_value_reset_string_cursor(pVal);` |
|    241 |  814 | `					}` |
|    360 |  815 | `					ph7_context_release_value(pCtx, pVal);` |
|    179 |  816 | `				}` |
|    360 |  817 | `				if( ovector[1] == ovector[0] ){` |
|    237 |  818 | `					startOffset = PcreEmptyMatchNext(pCode, zSubject, nSubLen, ovector[0]);` |
|    119 |  819 | `				}else{` |
|    124 |  820 | `					startOffset = ovector[1];` |
|      - |  821 | `				}` |
|    360 |  822 | `				totalMatches++;` |
|      2 |  823 | `			}` |
|    136 |  824 | `			if( apGroupArrays ){` |
|      - |  825 | `				/* Attach the per-group match arrays. php's PREG_PATTERN_ORDER stores a` |
|      - |  826 | `				 * named group under BOTH its name and its number, interleaved` |
|      - |  827 | ``				 * (`0, name, 1, value, 2`) — the same value under each key. Read the`` |
|      - |  828 | `				 * name table so each numbered group can emit its named alias first. */` |
|    136 |  829 | `				uint32_t namecount = 0, nameentrysize = 0;` |
|    136 |  830 | `				PCRE2_SPTR nametable = 0;` |
|    136 |  831 | `				pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);` |
|    136 |  832 | `				if( namecount > 0 ){` |
|      5 |  833 | `					pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);` |
|      5 |  834 | `					pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);` |
|      2 |  835 | `				}` |
|    304 |  836 | `				for( g = 0; g < nGroups; g++ ){` |
|    170 |  837 | `					const char *zName = 0;` |
|    170 |  838 | `					if( namecount > 0 ){` |
|      - |  839 | `						uint32_t k;` |
|     23 |  840 | `						for( k = 0; k < namecount; k++ ){` |
|     17 |  841 | `							PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|     17 |  842 | `							if( (uint32_t)(((entry[0] << 8) \| entry[1])) == g ){` |
|      7 |  843 | `								zName = (const char *)(entry + 2);` |
|      7 |  844 | `								break;` |
|      - |  845 | `							}` |
|      6 |  846 | `						}` |
|      6 |  847 | `					}` |
|    170 |  848 | `					if( zName ){` |
|      7 |  849 | `						ph7_array_add_strkey_elem(pOutArray, zName, apGroupArrays[g]);` |
|      3 |  850 | `					}` |
|    170 |  851 | `					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);` |
|    170 |  852 | `					ph7_context_release_value(pCtx, apGroupArrays[g]);` |
|     86 |  853 | `				}` |
|    136 |  854 | `				ph7_context_free_chunk(pCtx, apGroupArrays);` |
|    136 |  855 | `				if( pMarkArray ){` |
|    ! 0 |  856 | `					ph7_array_add_strkey_elem(pOutArray,"MARK",pMarkArray);` |
|    ! 0 |  857 | `					ph7_context_release_value(pCtx, pMarkArray);` |
|    ! 0 |  858 | `				}` |
|     67 |  859 | `			}` |
|      - |  860 | `		}` |
|      - |  861 | `		/* Write output array to caller's variable */` |
|    244 |  862 | `		if( pOutArray && nArg >= 3 ){` |
|    244 |  863 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pOutArray);` |
|    244 |  864 | `			ph7_context_release_value(pCtx, pOutArray);` |
|    121 |  865 | `		}` |
|      - |  866 | `	}` |
|    244 |  867 | `	pcre2_match_data_free(pMatchData);` |
|    244 |  868 | `	ph7_result_int(pCtx, totalMatches);` |
|    244 |  869 | `	return PH7_OK;` |
|    128 |  870 | `}` |
|      - |  871 |  |
|      - |  872 | `/* ======================================================================` |
|      - |  873 | ` * preg_split(pattern, subject [, limit [, flags]])` |
|      - |  874 | ` * ====================================================================== */` |
|    442 |  875 | `static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  876 | `{` |
|      - |  877 | `	const char *zPattern, *zSubject;` |
|      - |  878 | `	int nPatLen, nSubLen;` |
|      - |  879 | `	pcre2_code *pCode;` |
|      - |  880 | `	pcre2_match_data *pMatchData;` |
|      - |  881 | `	sxu32 nCapture;` |
|      - |  882 | `	ph7_value *pArray;` |
|      - |  883 | `	ph7_value *pVal;` |
|    444 |  884 | `	PCRE2_SIZE startOffset = 0, lastOffset = 0;` |
|    444 |  885 | `	int limit = -1;` |
|    444 |  886 | `	int iFlags = 0;` |
|    444 |  887 | `	int nPieces = 0;` |
|      - |  888 | `	int rc;` |
|      - |  889 |  |
|    444 |  890 | `	if( nArg < 2 ){` |
|    ! 0 |  891 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|      - |  892 | `			"preg_split() expects at least 2 parameters");` |
|    ! 0 |  893 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  894 | `		return PH7_OK;` |
|      - |  895 | `	}` |
|    444 |  896 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|    444 |  897 | `	zSubject = ph7_value_to_string(apArg[1], &nSubLen);` |
|    444 |  898 | `	if( nArg >= 3 ){` |
|     11 |  899 | `		limit = ph7_value_to_int(apArg[2]);` |
|      5 |  900 | `	}` |
|    444 |  901 | `	if( nArg >= 4 ){` |
|      9 |  902 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|      4 |  903 | `	}` |
|    444 |  904 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    444 |  905 | `	if( pCode == 0 ){` |
|      6 |  906 | `		ph7_result_bool(pCtx, 0);` |
|      6 |  907 | `		return PH7_OK;` |
|      - |  908 | `	}` |
|    439 |  909 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    439 |  910 | `	if( pMatchData == 0 ){` |
|    ! 0 |  911 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  912 | `		return PH7_OK;` |
|      - |  913 | `	}` |
|    439 |  914 | `	pArray = ph7_context_new_array(pCtx);` |
|    439 |  915 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    439 |  916 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|      - |  917 |  |
|   1177 |  918 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|   1133 |  919 | `		if( limit > 0 && nPieces >= limit - 1 ){` |
|      3 |  920 | `			break; /* Last piece gets the remainder */` |
|      - |  921 | `		}` |
|   1696 |  922 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    565 |  923 | `			startOffset, 0, pMatchData, NULL);` |
|   1131 |  924 | `		if( rc < 0 ){` |
|    393 |  925 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|    ! 0 |  926 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|    ! 0 |  927 | `			}` |
|    393 |  928 | `			break;` |
|      - |  929 | `		}` |
|      - |  930 | `		{` |
|    739 |  931 | `			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);` |
|    739 |  932 | `			PCRE2_SIZE matchStart = ovector[0];` |
|    739 |  933 | `			PCRE2_SIZE matchEnd = ovector[1];` |
|    739 |  934 | `			int pieceLen = (int)(matchStart - lastOffset);` |
|      - |  935 |  |
|      - |  936 | `			/* Add the piece before the match */` |
|    739 |  937 | `			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| pieceLen > 0 ){` |
|    737 |  938 | `				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|    ! 0 |  939 | `					ph7_value *pSub = ph7_context_new_array(pCtx);` |
|    ! 0 |  940 | `					ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|    ! 0 |  941 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|    ! 0 |  942 | `					ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|    ! 0 |  943 | `					ph7_value_int(pOff, (int)lastOffset);` |
|    ! 0 |  944 | `					ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|    ! 0 |  945 | `					ph7_array_add_elem(pArray, 0, pSub);` |
|    ! 0 |  946 | `					ph7_context_release_value(pCtx, pSub);` |
|    ! 0 |  947 | `					ph7_context_release_value(pCtx, pOff);` |
|    ! 0 |  948 | `				}else{` |
|    737 |  949 | `					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);` |
|    737 |  950 | `					ph7_array_add_elem(pArray, 0, pVal);` |
|      - |  951 | `				}` |
|    737 |  952 | `				ph7_value_reset_string_cursor(pVal);` |
|    737 |  953 | `				nPieces++;` |
|    368 |  954 | `			}` |
|      - |  955 | `			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */` |
|    739 |  956 | `			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){` |
|      - |  957 | `				int g;` |
|    ! 0 |  958 | `				for( g = 1; g < rc; g++ ){` |
|    ! 0 |  959 | `					PCRE2_SIZE gs = ovector[2*g];` |
|    ! 0 |  960 | `					PCRE2_SIZE ge = ovector[2*g+1];` |
|      - |  961 | `					int gLen;` |
|    ! 0 |  962 | `					if( gs == PCRE2_UNSET ) continue;` |
|    ! 0 |  963 | `					gLen = (int)(ge - gs);` |
|    ! 0 |  964 | `					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| gLen > 0 ){` |
|    ! 0 |  965 | `						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|    ! 0 |  966 | `							ph7_value *pSub = ph7_context_new_array(pCtx);` |
|    ! 0 |  967 | `							ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|    ! 0 |  968 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|    ! 0 |  969 | `							ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|    ! 0 |  970 | `							ph7_value_int(pOff, (int)gs);` |
|    ! 0 |  971 | `							ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|    ! 0 |  972 | `							ph7_array_add_elem(pArray, 0, pSub);` |
|    ! 0 |  973 | `							ph7_context_release_value(pCtx, pSub);` |
|    ! 0 |  974 | `							ph7_context_release_value(pCtx, pOff);` |
|    ! 0 |  975 | `						}else{` |
|    ! 0 |  976 | `							ph7_value_string(pVal, &zSubject[gs], gLen);` |
|    ! 0 |  977 | `							ph7_array_add_elem(pArray, 0, pVal);` |
|      - |  978 | `						}` |
|    ! 0 |  979 | `						ph7_value_reset_string_cursor(pVal);` |
|    ! 0 |  980 | `					}` |
|    ! 0 |  981 | `				}` |
|    ! 0 |  982 | `			}` |
|      - |  983 | `			/* Advance */` |
|    739 |  984 | `			lastOffset = matchEnd;` |
|    739 |  985 | `			if( matchEnd == matchStart ){` |
|    257 |  986 | `				startOffset = PcreEmptyMatchNext(pCode, zSubject, nSubLen, matchStart);` |
|    129 |  987 | `			}else{` |
|    483 |  988 | `				startOffset = matchEnd;` |
|      - |  989 | `			}` |
|      - |  990 | `		}` |
|      1 |  991 | `	}` |
|      - |  992 | `	/* Add trailing piece */` |
|      - |  993 | `	{` |
|    439 |  994 | `		int trailLen = nSubLen - (int)lastOffset;` |
|    439 |  995 | `		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) \|\| trailLen > 0 ){` |
|    437 |  996 | `			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){` |
|    ! 0 |  997 | `				ph7_value *pSub = ph7_context_new_array(pCtx);` |
|    ! 0 |  998 | `				ph7_value *pOff = ph7_context_new_scalar(pCtx);` |
|    ! 0 |  999 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|    ! 0 | 1000 | `				ph7_array_add_intkey_elem(pSub, 0, pVal);` |
|    ! 0 | 1001 | `				ph7_value_int(pOff, (int)lastOffset);` |
|    ! 0 | 1002 | `				ph7_array_add_intkey_elem(pSub, 1, pOff);` |
|    ! 0 | 1003 | `				ph7_array_add_elem(pArray, 0, pSub);` |
|    ! 0 | 1004 | `				ph7_context_release_value(pCtx, pSub);` |
|    ! 0 | 1005 | `				ph7_context_release_value(pCtx, pOff);` |
|    ! 0 | 1006 | `			}else{` |
|    437 | 1007 | `				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);` |
|    437 | 1008 | `				ph7_array_add_elem(pArray, 0, pVal);` |
|      - | 1009 | `			}` |
|    218 | 1010 | `		}` |
|      - | 1011 | `	}` |
|    439 | 1012 | `	ph7_context_release_value(pCtx, pVal);` |
|    439 | 1013 | `	pcre2_match_data_free(pMatchData);` |
|    439 | 1014 | `	ph7_result_value(pCtx, pArray);` |
|    439 | 1015 | `	ph7_context_release_value(pCtx, pArray);` |
|    439 | 1016 | `	return PH7_OK;` |
|    223 | 1017 | `}` |
|      - | 1018 |  |
|      - | 1019 | `/* ===== Helper: expand backreferences in replacement string ===== */` |
|   2626 | 1020 | `static void PcreExpandBackrefs(` |
|      - | 1021 | `	SyBlob *pOut,` |
|      - | 1022 | `	const char *zRepl, int nReplLen,` |
|      - | 1023 | `	const char *zSubject,` |
|      - | 1024 | `	PCRE2_SIZE *ovector, int nGroups)` |
|      5 | 1025 | `{` |
|   2631 | 1026 | `	const char *zEnd = &zRepl[nReplLen];` |
|   2631 | 1027 | `	const char *z = zRepl;` |
|      - | 1028 |  |
|  10903 | 1029 | `	while( z < zEnd ){` |
|   8277 | 1030 | `		if( *z == '\\' && z + 1 < zEnd ){` |
|    ! 0 | 1031 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|    ! 0 | 1032 | `				int g = z[1] - '0';` |
|    ! 0 | 1033 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|    ! 0 | 1034 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|    ! 0 | 1035 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|    ! 0 | 1036 | `				}` |
|    ! 0 | 1037 | `				z += 2;` |
|    ! 0 | 1038 | `				continue;` |
|      - | 1039 | `			}` |
|    ! 0 | 1040 | `			if( z[1] == '\\' ){` |
|    ! 0 | 1041 | `				SyBlobAppend(pOut, "\\", 1);` |
|    ! 0 | 1042 | `				z += 2;` |
|    ! 0 | 1043 | `				continue;` |
|      - | 1044 | `			}` |
|      - | 1045 | `			/* Not a backreference — emit literally */` |
|    ! 0 | 1046 | `			SyBlobAppend(pOut, z, 1);` |
|    ! 0 | 1047 | `			z++;` |
|    ! 0 | 1048 | `			continue;` |
|      - | 1049 | `		}` |
|   8277 | 1050 | `		if( *z == '$' && z + 1 < zEnd ){` |
|    312 | 1051 | `			if( z[1] == '$' ){` |
|    ! 0 | 1052 | `				SyBlobAppend(pOut, "$", 1);` |
|    ! 0 | 1053 | `				z += 2;` |
|    ! 0 | 1054 | `				continue;` |
|      - | 1055 | `			}` |
|    312 | 1056 | `			if( z[1] == '{' ){` |
|      - | 1057 | `				/* ${N} form */` |
|     39 | 1058 | `				const char *p = z + 2;` |
|     39 | 1059 | `				int g = 0;` |
|     77 | 1060 | `				while( p < zEnd && *p >= '0' && *p <= '9' ){` |
|     39 | 1061 | `					g = g * 10 + (*p - '0');` |
|     39 | 1062 | `					p++;` |
|      1 | 1063 | `				}` |
|     39 | 1064 | `				if( p < zEnd && *p == '}' ){` |
|     39 | 1065 | `					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|     58 | 1066 | `						SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|     38 | 1067 | `							(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|     19 | 1068 | `					}` |
|     39 | 1069 | `					z = p + 1;` |
|     39 | 1070 | `					continue;` |
|      - | 1071 | `				}` |
|      - | 1072 | `				/* Not a valid ${N} — emit literally */` |
|    ! 0 | 1073 | `				SyBlobAppend(pOut, z, 1);` |
|    ! 0 | 1074 | `				z++;` |
|    ! 0 | 1075 | `				continue;` |
|      - | 1076 | `			}` |
|    274 | 1077 | `			if( z[1] >= '0' && z[1] <= '9' ){` |
|      - | 1078 | `				/* $N or $NN */` |
|    274 | 1079 | `				int g = z[1] - '0';` |
|    274 | 1080 | `				z += 2;` |
|      - | 1081 | `				/* Check for second digit */` |
|    274 | 1082 | `				if( z < zEnd && *z >= '0' && *z <= '9' ){` |
|    ! 0 | 1083 | `					int g2 = g * 10 + (*z - '0');` |
|    ! 0 | 1084 | `					if( g2 < nGroups ){` |
|    ! 0 | 1085 | `						g = g2;` |
|    ! 0 | 1086 | `						z++;` |
|    ! 0 | 1087 | `					}` |
|    ! 0 | 1088 | `				}` |
|    274 | 1089 | `				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){` |
|    409 | 1090 | `					SyBlobAppend(pOut, &zSubject[ovector[2*g]],` |
|    270 | 1091 | `						(sxu32)(ovector[2*g+1] - ovector[2*g]));` |
|    135 | 1092 | `				}` |
|    274 | 1093 | `				continue;` |
|      - | 1094 | `			}` |
|      - | 1095 | `			/* Not a backreference */` |
|    ! 0 | 1096 | `			SyBlobAppend(pOut, z, 1);` |
|    ! 0 | 1097 | `			z++;` |
|    ! 0 | 1098 | `			continue;` |
|      - | 1099 | `		}` |
|   7969 | 1100 | `		SyBlobAppend(pOut, z, 1);` |
|   7969 | 1101 | `		z++;` |
|      5 | 1102 | `	}` |
|   2631 | 1103 | `}` |
|      - | 1104 |  |
|      - | 1105 | `/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */` |
|   3244 | 1106 | `static void PcreDoReplace(` |
|      - | 1107 | `	ph7_context *pCtx,` |
|      - | 1108 | `	pcre2_code *pCode,` |
|      - | 1109 | `	const char *zSubject, int nSubLen,` |
|      - | 1110 | `	const char *zRepl, int nReplLen,` |
|      - | 1111 | `	int limit,` |
|      - | 1112 | `	int *pCount,` |
|      - | 1113 | `	SyBlob *pOut)` |
|      5 | 1114 | `{` |
|      - | 1115 | `	pcre2_match_data *pMatchData;` |
|   3249 | 1116 | `	PCRE2_SIZE startOffset = 0;` |
|   3249 | 1117 | `	int nReplacements = 0;` |
|      - | 1118 | `	int rc;` |
|      - | 1119 |  |
|   3249 | 1120 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|   3249 | 1121 | `	if( pMatchData == 0 ) return;` |
|      - | 1122 |  |
|   5875 | 1123 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|      - | 1124 | `		PCRE2_SIZE *ovector;` |
|   5831 | 1125 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|   8732 | 1126 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|   2909 | 1127 | `			startOffset, 0, pMatchData, NULL);` |
|   5823 | 1128 | `		if( rc < 0 ){` |
|   3197 | 1129 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|    ! 0 | 1130 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|    ! 0 | 1131 | `			}` |
|   3197 | 1132 | `			break;` |
|      - | 1133 | `		}` |
|   2631 | 1134 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|      - | 1135 | `		/* Copy text before match */` |
|   2631 | 1136 | `		if( ovector[0] > startOffset ){` |
|   1883 | 1137 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|    939 | 1138 | `		}` |
|      - | 1139 | `		/* Expand replacement */` |
|   2631 | 1140 | `		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);` |
|   2631 | 1141 | `		nReplacements++;` |
|      - | 1142 | `		/* Advance */` |
|   2631 | 1143 | `		if( ovector[1] == ovector[0] ){` |
|      - | 1144 | `			/* Zero-width match: to make progress, emit the character AT THE MATCH` |
|      - | 1145 | `			 * POSITION (ovector[0]) and step past it. The match can sit AHEAD of the` |
|      - | 1146 | `			 * search start (a lookbehind/lookahead assertion, e.g. the camelCase` |
|      - | 1147 | `			 * split /(?<=[[:lower:]])(?=[[:upper:]])/), so copying zSubject[startOffset]` |
|      - | 1148 | `			 * grabbed the wrong byte ("fooBar" -> "foo far"). The text between` |
|      - | 1149 | `			 * startOffset and ovector[0] was already copied above. */` |
|    265 | 1150 | `			PCRE2_SIZE nNext = PcreEmptyMatchNext(pCode, zSubject, nSubLen, ovector[0]);` |
|    265 | 1151 | `			if( ovector[0] < (PCRE2_SIZE)nSubLen ){` |
|    221 | 1152 | `				SyBlobAppend(pOut, &zSubject[ovector[0]], (sxu32)(nNext - ovector[0]));` |
|    110 | 1153 | `			}` |
|    265 | 1154 | `			startOffset = nNext;` |
|    133 | 1155 | `		}else{` |
|   2367 | 1156 | `			startOffset = ovector[1];` |
|      - | 1157 | `		}` |
|      5 | 1158 | `	}` |
|      - | 1159 | `	/* Copy remainder */` |
|   3249 | 1160 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|   2241 | 1161 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|   1118 | 1162 | `	}` |
|   3249 | 1163 | `	if( pCount ){` |
|   3249 | 1164 | `		*pCount += nReplacements;` |
|   1622 | 1165 | `	}` |
|   3249 | 1166 | `	pcre2_match_data_free(pMatchData);` |
|   1622 | 1167 | `	SXUNUSED(pCtx);` |
|   1627 | 1168 | `}` |
|      - | 1169 |  |
|      - | 1170 | `/*` |
|      - | 1171 | ` * php's USER-VISIBLE string cast of a pcre PATTERN, REPLACEMENT or SUBJECT --` |
|      - | 1172 | ` * any of which preg_replace()/preg_replace_callback() may be handed as an ARRAY` |
|      - | 1173 | ` * whose elements are themselves anything at all. An element that is an array` |
|      - | 1174 | `` * warns `Array to string conversion` and reads as "Array"; one that is an`` |
|      - | 1175 | ` * object with no __toString() is php's catchable` |
|      - | 1176 | `` * `Object of class X could not be converted to string`, and the call then`` |
|      - | 1177 | ` * answers nothing. PHL used the SILENT embedder cast here, so the first said` |
|      - | 1178 | ` * nothing and the second rendered as the literal "Object" -- a string php never` |
|      - | 1179 | ` * produces, matched against the subject as if the program had written it.` |
|      - | 1180 | ` *` |
|      - | 1181 | ` * Answers 0 when the coercion threw; the status is on the call context and` |
|      - | 1182 | ` * OP_CALL lands it, so the caller has only to stop.` |
|      - | 1183 | ` */` |
|  10304 | 1184 | `static int PcreStrUV(ph7_context *pCtx, ph7_value *pVal, const char **pzOut, int *pnOut)` |
|      5 | 1185 | `{` |
|  10309 | 1186 | `	return PH7_ValueToStringUV(pCtx, pVal, pzOut, pnOut) == SXRET_OK;` |
|      5 | 1187 | `}` |
|      - | 1188 | `/* ===== Helper: apply pattern(s)+replacement(s) to ONE subject string =====` |
|      - | 1189 | ` * pPattern is a string or an array of patterns; pRepl is a string (used for` |
|      - | 1190 | ` * every pattern) or, only when pPattern is an array, an array taken by ORDER` |
|      - | 1191 | ` * (missing element -> ""). Array patterns are applied sequentially, each to the` |
|      - | 1192 | ` * result of the previous (PHP semantics), ping-ponging two blobs. The final` |
|      - | 1193 | ` * text is appended to pOut. Returns SXRET_OK, or SXERR_SYNTAX on a bad pattern` |
|      - | 1194 | ` * (the caller then yields NULL, matching the scalar path). */` |
|   3230 | 1195 | `static sxi32 PcreReplaceSubject(` |
|      - | 1196 | `	ph7_context *pCtx,` |
|      - | 1197 | `	ph7_value *pPattern,` |
|      - | 1198 | `	ph7_value *pRepl,` |
|      - | 1199 | `	const char *zSubject, int nSubLen,` |
|      - | 1200 | `	int limit,` |
|      - | 1201 | `	int *pCount,` |
|      - | 1202 | `	SyBlob *pOut)` |
|      5 | 1203 | `{` |
|      - | 1204 | `	sxu32 nCapture;` |
|   3235 | 1205 | `	if( !ph7_value_is_array(pPattern) ){` |
|      - | 1206 | `		/* Single pattern + single replacement */` |
|      - | 1207 | `		const char *zPattern, *zRepl;` |
|      - | 1208 | `		int nPatLen, nReplLen;` |
|      - | 1209 | `		pcre2_code *pCode;` |
|   3188 | 1210 | `		if( !PcreStrUV(pCtx, pPattern, &zPattern, &nPatLen)` |
|   3193 | 1211 | `		 \|\| !PcreStrUV(pCtx, pRepl, &zRepl, &nReplLen) ){` |
|    ! 0 | 1212 | `			return SXERR_SYNTAX;` |
|      - | 1213 | `		}` |
|   3193 | 1214 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|   3193 | 1215 | `		if( pCode == 0 ){` |
|     21 | 1216 | `			return SXERR_SYNTAX; /* NOT SXERR_ABORT: that is a real unwind status */` |
|      - | 1217 | `		}` |
|   3175 | 1218 | `		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, pCount, pOut);` |
|   3175 | 1219 | `		return SXRET_OK;` |
|    ! 0 | 1220 | `	}else{` |
|      - | 1221 | `		/* Array of patterns: apply each in insertion order to the accumulating` |
|      - | 1222 | `		 * subject. Replacement is the parallel array element (by order) or the` |
|      - | 1223 | `		 * scalar replacement for every pattern. */` |
|     44 | 1224 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|     44 | 1225 | `		ph7_hashmap *pRepMap = ph7_value_is_array(pRepl) ? (ph7_hashmap *)pRepl->x.pOther : 0;` |
|     44 | 1226 | `		const char *zScalarRepl = 0;` |
|     44 | 1227 | `		int nScalarRepl = 0;` |
|      - | 1228 | `		ph7_hashmap_node *pPatNode, *pRepNode;` |
|      - | 1229 | `		ph7_value sPat, sRep;` |
|      - | 1230 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|      - | 1231 | `		sxu32 n;` |
|     44 | 1232 | `		sxi32 rc = SXRET_OK;` |
|     44 | 1233 | `		if( pRepMap == 0 && !PcreStrUV(pCtx, pRepl, &zScalarRepl, &nScalarRepl) ){` |
|    ! 0 | 1234 | `			return SXERR_SYNTAX;` |
|      - | 1235 | `		}` |
|     44 | 1236 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|     44 | 1237 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|     44 | 1238 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|     44 | 1239 | `		pSrc = &sA; pDst = &sB;` |
|     44 | 1240 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|     44 | 1241 | `		PH7_MemObjInit(pCtx->pVm, &sRep);` |
|     44 | 1242 | `		pPatNode = pPatMap->pFirst;` |
|     44 | 1243 | `		pRepNode = pRepMap ? pRepMap->pFirst : 0;` |
|     44 | 1244 | `		n = pPatMap->nEntry;` |
|    118 | 1245 | `		while( n > 0 ){` |
|      - | 1246 | `			const char *zPattern, *zRepl;` |
|      - | 1247 | `			int nPatLen, nReplLen;` |
|      - | 1248 | `			pcre2_code *pCode;` |
|      - | 1249 | `			SyBlob *pSwap;` |
|     80 | 1250 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|     80 | 1251 | `			if( !PcreStrUV(pCtx, &sPat, &zPattern, &nPatLen) ){` |
|    ! 0 | 1252 | `				rc = SXERR_SYNTAX;` |
|    ! 0 | 1253 | `				PH7_MemObjRelease(&sPat);` |
|    ! 0 | 1254 | `				break;` |
|      - | 1255 | `			}` |
|     80 | 1256 | `			if( pRepMap ){` |
|     44 | 1257 | `				if( pRepNode ){` |
|     42 | 1258 | `					PH7_HashmapExtractNodeValue(pRepNode, &sRep, FALSE);` |
|     42 | 1259 | `					if( !PcreStrUV(pCtx, &sRep, &zRepl, &nReplLen) ){` |
|    ! 0 | 1260 | `						rc = SXERR_SYNTAX;` |
|    ! 0 | 1261 | `						PH7_MemObjRelease(&sPat);` |
|    ! 0 | 1262 | `						PH7_MemObjRelease(&sRep);` |
|    ! 0 | 1263 | `						break;` |
|      - | 1264 | `					}` |
|     22 | 1265 | `				}else{` |
|      3 | 1266 | `					zRepl = ""; nReplLen = 0;` |
|      - | 1267 | `				}` |
|     23 | 1268 | `			}else{` |
|     37 | 1269 | `				zRepl = zScalarRepl; nReplLen = nScalarRepl;` |
|      - | 1270 | `			}` |
|     80 | 1271 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     80 | 1272 | `			if( pCode == 0 ){` |
|      5 | 1273 | `				rc = SXERR_SYNTAX; /* NOT SXERR_ABORT: that is a real unwind status */` |
|      5 | 1274 | `				PH7_MemObjRelease(&sPat);` |
|      5 | 1275 | `				if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|      5 | 1276 | `				break;` |
|      - | 1277 | `			}` |
|     76 | 1278 | `			SyBlobReset(pDst);` |
|    113 | 1279 | `			PcreDoReplace(pCtx, pCode,` |
|     74 | 1280 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|     37 | 1281 | `				zRepl, nReplLen, limit, pCount, pDst);` |
|      - | 1282 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|     76 | 1283 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|     76 | 1284 | `			PH7_MemObjRelease(&sPat);` |
|     76 | 1285 | `			if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }` |
|     76 | 1286 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|     76 | 1287 | `			if( pRepNode ){ pRepNode = pRepNode->pPrev; }` |
|     76 | 1288 | `			n--;` |
|      2 | 1289 | `		}` |
|     44 | 1290 | `		if( rc == SXRET_OK ){` |
|     40 | 1291 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|     19 | 1292 | `		}` |
|     44 | 1293 | `		SyBlobRelease(&sA);` |
|     44 | 1294 | `		SyBlobRelease(&sB);` |
|     44 | 1295 | `		return rc;` |
|      - | 1296 | `	}` |
|   1620 | 1297 | `}` |
|      - | 1298 |  |
|      - | 1299 | `/* ======================================================================` |
|      - | 1300 | ` * preg_replace(pattern, replacement, subject [, limit [, &count]])` |
|      - | 1301 | ` * preg_filter(pattern, replacement, subject [, limit [, &count]])` |
|      - | 1302 | ` *` |
|      - | 1303 | ` * php gives the two ONE C body and a flag: preg_filter keeps only the subjects` |
|      - | 1304 | ` * that were actually changed (an array subject loses the untouched keys, a` |
|      - | 1305 | ` * scalar one answers NULL). Everything else -- the diagnostics, &$count, the` |
|      - | 1306 | ` * array shapes -- is the same code, so the two share it here too, and that is` |
|      - | 1307 | ` * also what makes preg_filter's refusals wear its OWN name: they are raised` |
|      - | 1308 | ` * through the calling context, which ph7_function_name() reads.` |
|      - | 1309 | ` * ====================================================================== */` |
|   3200 | 1310 | `static int PcreReplaceCommon(ph7_context *pCtx, int nArg, ph7_value **apArg, int bFilter)` |
|      5 | 1311 | `{` |
|   3205 | 1312 | `	int limit = -1;` |
|   3205 | 1313 | `	int count = 0;` |
|      - | 1314 |  |
|   3205 | 1315 | `	if( nArg < 3 ){` |
|      - | 1316 | `		/* Unreachable while aBuiltinSig[] enforces the arity php reports as an` |
|      - | 1317 | `		 * ArgumentCountError; kept for a direct C caller. */` |
|    ! 0 | 1318 | `		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,` |
|    ! 0 | 1319 | `			"%s() expects at least 3 parameters", ph7_function_name(pCtx));` |
|    ! 0 | 1320 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1321 | `		return PH7_OK;` |
|      - | 1322 | `	}` |
|   3205 | 1323 | `	if( nArg >= 4 ){` |
|     56 | 1324 | `		limit = ph7_value_to_int(apArg[3]);` |
|     27 | 1325 | `	}` |
|   3205 | 1326 | `	if( !ph7_value_is_array(apArg[0]) && ph7_value_is_array(apArg[1]) ){` |
|      - | 1327 | `		/* php 8 refuses the PAIR, as a catchable TypeError naming both positions --` |
|      - | 1328 | `		 * a string pattern cannot consume an array of replacements. PHL warned and` |
|      - | 1329 | `		 * answered NULL, php 5's shape, so a program written against php carried on` |
|      - | 1330 | `		 * past a call php stops it for. */` |
|     10 | 1331 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1332 | `			"%s(): Argument #1 ($pattern) must be of type array when argument #2 "` |
|      3 | 1333 | `			"($replacement) is an array, string given",ph7_function_name(pCtx));` |
|      - | 1334 | `	}` |
|      - | 1335 | `	/* Only now: preg_last_error() reports the last pattern that RAN, and an` |
|      - | 1336 | `	 * argument php refuses before that never clears it. A cached pattern skips` |
|      - | 1337 | `	 * the compile, which is why the clear cannot live there. */` |
|   3199 | 1338 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|   4790 | 1339 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      - | 1340 | `		/* Array subject: return an array, each element replaced, keys preserved. */` |
|     40 | 1341 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|     40 | 1342 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|     40 | 1343 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|      - | 1344 | `		ph7_value sKey, sVal;` |
|      - | 1345 | `		ph7_hashmap_node *pNode;` |
|      - | 1346 | `		sxu32 n;` |
|     40 | 1347 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|    ! 0 | 1348 | `			ph7_result_null(pCtx);` |
|    ! 0 | 1349 | `			return PH7_OK;` |
|      - | 1350 | `		}` |
|     40 | 1351 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|     40 | 1352 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     40 | 1353 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|     40 | 1354 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|    114 | 1355 | `		while( n > 0 ){` |
|      - | 1356 | `			const char *zSubject;` |
|      - | 1357 | `			int nSubLen;` |
|     78 | 1358 | `			int nBefore = count;` |
|      - | 1359 | `			SyBlob sOut;` |
|     78 | 1360 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|     78 | 1361 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|     78 | 1362 | `			if( !PcreStrUV(pCtx, &sVal, &zSubject, &nSubLen) ){` |
|      3 | 1363 | `				PH7_MemObjRelease(&sKey);` |
|      3 | 1364 | `				PH7_MemObjRelease(&sVal);` |
|      3 | 1365 | `				ph7_result_value(pCtx, pResult);` |
|      3 | 1366 | `				goto set_count;` |
|      - | 1367 | `			}` |
|     76 | 1368 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     76 | 1369 | `			if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|      - | 1370 | `				/* A refused pattern drops THIS subject and php moves to the next` |
|      - | 1371 | `				 * one, so a two-element array reports the refusal twice and` |
|      - | 1372 | `				 * answers the empty array. A coercion that THREW is the other` |
|      - | 1373 | `				 * kind of failure: it stops the call where php stops it. */` |
|     13 | 1374 | `				SyBlobRelease(&sOut);` |
|     13 | 1375 | `				PH7_MemObjRelease(&sKey);` |
|     13 | 1376 | `				PH7_MemObjRelease(&sVal);` |
|     13 | 1377 | `				if( pCtx->nThrowRc ){` |
|    ! 0 | 1378 | `					ph7_result_value(pCtx, pResult);` |
|    ! 0 | 1379 | `					goto set_count;` |
|      - | 1380 | `				}` |
|     13 | 1381 | `				pNode = pNode->pPrev;` |
|     13 | 1382 | `				n--;` |
|     13 | 1383 | `				continue;` |
|      - | 1384 | `			}` |
|      - | 1385 | `			/* preg_filter keeps a subject only when this one changed */` |
|     64 | 1386 | `			if( !bFilter \|\| count > nBefore ){` |
|     58 | 1387 | `				ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     58 | 1388 | `				ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|     58 | 1389 | `				ph7_value_reset_string_cursor(pElem);` |
|     28 | 1390 | `			}` |
|     64 | 1391 | `			SyBlobRelease(&sOut);` |
|     64 | 1392 | `			PH7_MemObjRelease(&sKey);` |
|     64 | 1393 | `			PH7_MemObjRelease(&sVal);` |
|     64 | 1394 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|     64 | 1395 | `			n--;` |
|      2 | 1396 | `		}` |
|     38 | 1397 | `		ph7_result_value(pCtx, pResult);` |
|     20 | 1398 | `	}else{` |
|      - | 1399 | `		/* Scalar subject: one replaced string. */` |
|      - | 1400 | `		const char *zSubject;` |
|      - | 1401 | `		int nSubLen;` |
|      - | 1402 | `		SyBlob sOut;` |
|   3161 | 1403 | `		if( !PcreStrUV(pCtx, apArg[2], &zSubject, &nSubLen) ){` |
|    ! 0 | 1404 | `			ph7_result_null(pCtx);` |
|    ! 0 | 1405 | `			goto set_count;` |
|      - | 1406 | `		}` |
|   3161 | 1407 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|   3161 | 1408 | `		if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){` |
|      - | 1409 | `			/* Scalar subject: a bad pattern returns NULL (PHP). */` |
|     13 | 1410 | `			SyBlobRelease(&sOut);` |
|     13 | 1411 | `			ph7_result_null(pCtx);` |
|     13 | 1412 | `			goto set_count;` |
|      - | 1413 | `		}` |
|   3151 | 1414 | `		if( bFilter && count == 0 ){` |
|      5 | 1415 | `			ph7_result_null(pCtx);` |
|      3 | 1416 | `		}else{` |
|   3147 | 1417 | `			ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      - | 1418 | `		}` |
|   3151 | 1419 | `		SyBlobRelease(&sOut);` |
|      - | 1420 | `	}` |
|   1597 | 1421 | `set_count:` |
|      - | 1422 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|      - | 1423 | `	 * (PHP always writes it: 0, or the count accumulated by earlier good patterns).` |
|      - | 1424 | `	 * A coercion that THREW is not one of those: php raises out of the call, so` |
|      - | 1425 | `	 * the caller's variable keeps whatever it held. */` |
|   3199 | 1426 | `	if( nArg >= 5 && pCtx->nThrowRc == 0 ){` |
|      - | 1427 | `		ph7_value sCount;` |
|     49 | 1428 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     49 | 1429 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|     49 | 1430 | `		PH7_MemObjRelease(&sCount);` |
|     24 | 1431 | `	}` |
|   3199 | 1432 | `	return PH7_OK;` |
|   1605 | 1433 | `}` |
|   3172 | 1434 | `static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 | 1435 | `{` |
|   3177 | 1436 | `	return PcreReplaceCommon(pCtx, nArg, apArg, 0);` |
|      5 | 1437 | `}` |
|     28 | 1438 | `static int PH7_builtin_preg_filter(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 1439 | `{` |
|     30 | 1440 | `	return PcreReplaceCommon(pCtx, nArg, apArg, 1);` |
|      2 | 1441 | `}` |
|      - | 1442 |  |
|      - | 1443 | `/* ===== Helper: run the callback over ONE compiled pattern on ONE subject =====` |
|      - | 1444 | ` * The mirror of PcreDoReplace() for preg_replace_callback: the replacement text` |
|      - | 1445 | ` * comes from a user callback fed the match array (shaped by $flags) instead of` |
|      - | 1446 | ` * from a template. Appends the whole replaced subject to pOut and adds its own` |
|      - | 1447 | ` * replacement count to *pCount. Returns SXRET_OK, or the dispatch status when` |
|      - | 1448 | ` * the callback did not return (PH7_CALLBACK_UNWOUND) — the caller then unwinds` |
|      - | 1449 | ` * without producing a result. */` |
|    242 | 1450 | `static sxi32 PcreDoCallbackReplace(` |
|      - | 1451 | `	ph7_context *pCtx,` |
|      - | 1452 | `	pcre2_code *pCode,` |
|      - | 1453 | `	const char *zSubject, int nSubLen,` |
|      - | 1454 | `	ph7_value *pCallback,` |
|      - | 1455 | `	int limit,` |
|      - | 1456 | `	int iFlags,` |
|      - | 1457 | `	int *pCount,` |
|      - | 1458 | `	SyBlob *pOut)` |
|      5 | 1459 | `{` |
|      - | 1460 | `	pcre2_match_data *pMatchData;` |
|    247 | 1461 | `	PCRE2_SIZE startOffset = 0;` |
|    247 | 1462 | `	int nReplacements = 0;` |
|      - | 1463 | `	int rc;` |
|      - | 1464 |  |
|    247 | 1465 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|    247 | 1466 | `	if( pMatchData == 0 ){` |
|    ! 0 | 1467 | `		return SXRET_OK;` |
|      - | 1468 | `	}` |
|    631 | 1469 | `	while( startOffset <= (PCRE2_SIZE)nSubLen ){` |
|      - | 1470 | `		PCRE2_SIZE *ovector;` |
|      - | 1471 | `		ph7_value *pMatchArr;` |
|      - | 1472 | `		ph7_value *apCbArg[1];` |
|      - | 1473 | `		ph7_value sResult;` |
|      - | 1474 | `		const char *zReplacement;` |
|      - | 1475 | `		int nReplLen;` |
|      - | 1476 | `		sxi32 rcCb;` |
|      - | 1477 |  |
|    679 | 1478 | `		if( limit >= 0 && nReplacements >= limit ) break;` |
|    875 | 1479 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|    290 | 1480 | `			startOffset, 0, pMatchData, NULL);` |
|    585 | 1481 | `		if( rc < 0 ){` |
|    185 | 1482 | `			if( rc != PCRE2_ERROR_NOMATCH ){` |
|    ! 0 | 1483 | `				PcreSetMatchError(pCtx->pVm, rc);` |
|    ! 0 | 1484 | `			}` |
|    185 | 1485 | `			break;` |
|      - | 1486 | `		}` |
|    405 | 1487 | `		ovector = pcre2_get_ovector_pointer(pMatchData);` |
|      - | 1488 | `		/* Copy text before match */` |
|    405 | 1489 | `		if( ovector[0] > startOffset ){` |
|     94 | 1490 | `			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));` |
|     45 | 1491 | `		}` |
|      - | 1492 | `		/* Build matches array for callback */` |
|    405 | 1493 | `		pMatchArr = ph7_context_new_array(pCtx);` |
|    405 | 1494 | `		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, pMatchData, iFlags);` |
|      - | 1495 | `		/* Call the callback */` |
|    405 | 1496 | `		PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    405 | 1497 | `		apCbArg[0] = pMatchArr;` |
|    405 | 1498 | `		rcCb = PH7_VmCallCallbackByValue(pCtx->pVm, pCallback, 1, apCbArg, &sResult, 0);` |
|    405 | 1499 | `		if( PH7_CALLBACK_UNWOUND(rcCb) ){` |
|      - | 1500 | `			/* The callback did not return: propagate so the dispatcher unwinds.` |
|      - | 1501 | `			 * An UNCAUGHT throw comes back as PH7_ABORT, and testing only` |
|      - | 1502 | `			 * PH7_EXCEPTION kept the scan going -- re-running the callback, and` |
|      - | 1503 | `			 * re-reporting the fatal, once per remaining match. */` |
|     19 | 1504 | `			PH7_MemObjRelease(&sResult);` |
|     19 | 1505 | `			ph7_context_release_value(pCtx, pMatchArr);` |
|     19 | 1506 | `			pcre2_match_data_free(pMatchData);` |
|     19 | 1507 | `			*pCount += nReplacements;` |
|     19 | 1508 | `			return rcCb;` |
|      - | 1509 | `		}` |
|      - | 1510 | `		/* Get replacement string from callback result */` |
|    389 | 1511 | `		zReplacement = ph7_value_to_string(&sResult, &nReplLen);` |
|    389 | 1512 | `		SyBlobAppend(pOut, zReplacement, (sxu32)nReplLen);` |
|    389 | 1513 | `		PH7_MemObjRelease(&sResult);` |
|    389 | 1514 | `		ph7_context_release_value(pCtx, pMatchArr);` |
|    389 | 1515 | `		nReplacements++;` |
|      - | 1516 | `		/* Advance */` |
|    389 | 1517 | `		if( ovector[1] == ovector[0] ){` |
|      - | 1518 | `			/* Zero-width match: emit the character AT THE MATCH POSITION and step` |
|      - | 1519 | `			 * past it. The match can sit AHEAD of the search start (a lookaround` |
|      - | 1520 | `			 * assertion, e.g. the camelCase split), so copying zSubject[startOffset]` |
|      - | 1521 | `			 * grabbed the wrong byte ("fooBar" -> "foo far") — the same fix` |
|      - | 1522 | `			 * PcreDoReplace() carries. */` |
|    241 | 1523 | `			PCRE2_SIZE nNext = PcreEmptyMatchNext(pCode, zSubject, nSubLen, ovector[0]);` |
|    241 | 1524 | `			if( ovector[0] < (PCRE2_SIZE)nSubLen ){` |
|    199 | 1525 | `				SyBlobAppend(pOut, &zSubject[ovector[0]], (sxu32)(nNext - ovector[0]));` |
|     99 | 1526 | `			}` |
|    241 | 1527 | `			startOffset = nNext;` |
|    121 | 1528 | `		}else{` |
|    149 | 1529 | `			startOffset = ovector[1];` |
|      - | 1530 | `		}` |
|      5 | 1531 | `	}` |
|      - | 1532 | `	/* Copy remainder */` |
|    231 | 1533 | `	if( startOffset < (PCRE2_SIZE)nSubLen ){` |
|     95 | 1534 | `		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));` |
|     46 | 1535 | `	}` |
|    231 | 1536 | `	*pCount += nReplacements;` |
|    231 | 1537 | `	pcre2_match_data_free(pMatchData);` |
|    231 | 1538 | `	return SXRET_OK;` |
|    126 | 1539 | `}` |
|      - | 1540 |  |
|      - | 1541 | `/* ===== Helper: apply pattern(s)+callback to ONE subject string =====` |
|      - | 1542 | ` * The callback twin of PcreReplaceSubject(): pPattern is a string or an ARRAY of` |
|      - | 1543 | ` * patterns applied sequentially, each to the result of the previous (php` |
|      - | 1544 | ` * semantics), ping-ponging two blobs. Returns SXRET_OK, SXERR_SYNTAX on a bad` |
|      - | 1545 | ` * pattern (the caller then yields NULL / an empty array like the template path),` |
|      - | 1546 | ` * or the dispatch status when the callback did not return. The bad-pattern` |
|      - | 1547 | ` * sentinel must NOT be SXERR_ABORT: that IS the status an exiting or uncaught` |
|      - | 1548 | ` * callback comes back with, and one code for both made a bad pattern kill the` |
|      - | 1549 | ` * script. */` |
|    256 | 1550 | `static sxi32 PcreCallbackReplaceSubject(` |
|      - | 1551 | `	ph7_context *pCtx,` |
|      - | 1552 | `	ph7_value *pPattern,` |
|      - | 1553 | `	ph7_value *pCallback,` |
|      - | 1554 | `	const char *zSubject, int nSubLen,` |
|      - | 1555 | `	int limit,` |
|      - | 1556 | `	int iFlags,` |
|      - | 1557 | `	int *pCount,` |
|      - | 1558 | `	SyBlob *pOut)` |
|      5 | 1559 | `{` |
|      - | 1560 | `	sxu32 nCapture;` |
|    261 | 1561 | `	if( !ph7_value_is_array(pPattern) ){` |
|      - | 1562 | `		const char *zPattern;` |
|      - | 1563 | `		int nPatLen;` |
|      - | 1564 | `		pcre2_code *pCode;` |
|    251 | 1565 | `		if( !PcreStrUV(pCtx, pPattern, &zPattern, &nPatLen) ){` |
|    ! 0 | 1566 | `			return SXERR_SYNTAX;` |
|      - | 1567 | `		}` |
|    251 | 1568 | `		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|    251 | 1569 | `		if( pCode == 0 ){` |
|     21 | 1570 | `			return SXERR_SYNTAX; /* bad pattern, NOT a callback unwind */` |
|      - | 1571 | `		}` |
|    344 | 1572 | `		return PcreDoCallbackReplace(pCtx, pCode, zSubject, nSubLen, pCallback,` |
|    113 | 1573 | `			limit, iFlags, pCount, pOut);` |
|    ! 0 | 1574 | `	}else{` |
|     12 | 1575 | `		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;` |
|      - | 1576 | `		ph7_hashmap_node *pPatNode;` |
|      - | 1577 | `		ph7_value sPat;` |
|      - | 1578 | `		SyBlob sA, sB, *pSrc, *pDst;` |
|      - | 1579 | `		sxu32 n;` |
|     12 | 1580 | `		sxi32 rc = SXRET_OK;` |
|     12 | 1581 | `		SyBlobInit(&sA, &pCtx->pVm->sAllocator);` |
|     12 | 1582 | `		SyBlobInit(&sB, &pCtx->pVm->sAllocator);` |
|     12 | 1583 | `		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */` |
|     12 | 1584 | `		pSrc = &sA; pDst = &sB;` |
|     12 | 1585 | `		PH7_MemObjInit(pCtx->pVm, &sPat);` |
|     12 | 1586 | `		pPatNode = pPatMap ? pPatMap->pFirst : 0;` |
|     12 | 1587 | `		n = pPatMap ? pPatMap->nEntry : 0;` |
|     26 | 1588 | `		while( n > 0 ){` |
|      - | 1589 | `			const char *zPattern;` |
|      - | 1590 | `			int nPatLen;` |
|      - | 1591 | `			pcre2_code *pCode;` |
|      - | 1592 | `			SyBlob *pSwap;` |
|     20 | 1593 | `			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);` |
|     20 | 1594 | `			if( !PcreStrUV(pCtx, &sPat, &zPattern, &nPatLen) ){` |
|    ! 0 | 1595 | `				rc = SXERR_SYNTAX;` |
|    ! 0 | 1596 | `				PH7_MemObjRelease(&sPat);` |
|    ! 0 | 1597 | `				break;` |
|      - | 1598 | `			}` |
|     20 | 1599 | `			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     20 | 1600 | `			if( pCode == 0 ){` |
|      3 | 1601 | `				rc = SXERR_SYNTAX; /* bad pattern, NOT a callback unwind */` |
|      3 | 1602 | `				PH7_MemObjRelease(&sPat);` |
|      3 | 1603 | `				break;` |
|      - | 1604 | `			}` |
|     18 | 1605 | `			SyBlobReset(pDst);` |
|     26 | 1606 | `			rc = PcreDoCallbackReplace(pCtx, pCode,` |
|     16 | 1607 | `				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),` |
|      8 | 1608 | `				pCallback, limit, iFlags, pCount, pDst);` |
|      - | 1609 | `			/* The freshly-produced text becomes the subject for the next pattern */` |
|     18 | 1610 | `			pSwap = pSrc; pSrc = pDst; pDst = pSwap;` |
|     18 | 1611 | `			PH7_MemObjRelease(&sPat);` |
|     18 | 1612 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      2 | 1613 | `				break;` |
|      - | 1614 | `			}` |
|     15 | 1615 | `			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */` |
|     15 | 1616 | `			n--;` |
|      1 | 1617 | `		}` |
|     12 | 1618 | `		if( rc == SXRET_OK ){` |
|      7 | 1619 | `			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));` |
|      3 | 1620 | `		}` |
|     12 | 1621 | `		SyBlobRelease(&sA);` |
|     12 | 1622 | `		SyBlobRelease(&sB);` |
|     12 | 1623 | `		return rc;` |
|      - | 1624 | `	}` |
|    133 | 1625 | `}` |
|      - | 1626 |  |
|      - | 1627 | `/* ======================================================================` |
|      - | 1628 | ` * preg_replace_callback(pattern, callback, subject [, limit [, &count [, flags]]])` |
|      - | 1629 | ` * ====================================================================== */` |
|    256 | 1630 | `static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 | 1631 | `{` |
|    261 | 1632 | `	int limit = -1;` |
|    261 | 1633 | `	int iFlags = 0;` |
|    261 | 1634 | `	int count = 0;` |
|      - | 1635 | `	sxi32 rc;` |
|      - | 1636 |  |
|    261 | 1637 | `	if( nArg < 3 ){` |
|    ! 0 | 1638 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|      - | 1639 | `			"preg_replace_callback() expects at least 3 parameters");` |
|    ! 0 | 1640 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1641 | `		return PH7_OK;` |
|      - | 1642 | `	}` |
|      - | 1643 | `	/* php screens $callback as an ARGUMENT and says exactly what is wrong with it` |
|      - | 1644 | `	 * ("function \"f\" not found or invalid function name", "no array or string` |
|      - | 1645 | `	 * given", "class \"x\" not found", "array callback must have exactly two` |
|      - | 1646 | `	 * members"). PHL answered one warning for all four and carried on with NULL,` |
|      - | 1647 | `	 * where php raises a catchable TypeError and never runs the call. */` |
|      - | 1648 | `	{` |
|    261 | 1649 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx, apArg[1], 2, "callback", 0);` |
|    261 | 1650 | `		if( rcCb != PH7_OK ){` |
|     15 | 1651 | `			return rcCb;` |
|      - | 1652 | `		}` |
|      - | 1653 | `	}` |
|    247 | 1654 | `	if( nArg >= 4 ){` |
|     66 | 1655 | `		limit = ph7_value_to_int(apArg[3]);` |
|     32 | 1656 | `	}` |
|    247 | 1657 | `	if( nArg >= 6 ){` |
|      - | 1658 | `		/* $flags shapes the match array handed to the callback exactly as it` |
|      - | 1659 | `		 * shapes preg_match()'s &$matches (PREG_OFFSET_CAPTURE /` |
|      - | 1660 | `		 * PREG_UNMATCHED_AS_NULL). php validates nothing here, so neither do we. */` |
|     52 | 1661 | `		iFlags = ph7_value_to_int(apArg[5]);` |
|     25 | 1662 | `	}` |
|    247 | 1663 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|      - | 1664 |  |
|    354 | 1665 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      - | 1666 | `		/* Array subject: return an array, each element replaced, keys preserved` |
|      - | 1667 | `		 * (php; PHL used to stringify the whole array to "Array" and replace in` |
|      - | 1668 | `		 * THAT — a silent wrong answer). */` |
|     28 | 1669 | `		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|     28 | 1670 | `		ph7_value *pResult = ph7_context_new_array(pCtx);` |
|     28 | 1671 | `		ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|      - | 1672 | `		ph7_value sKey, sVal;` |
|      - | 1673 | `		ph7_hashmap_node *pNode;` |
|      - | 1674 | `		sxu32 n;` |
|     28 | 1675 | `		if( pResult == 0 \|\| pElem == 0 ){` |
|    ! 0 | 1676 | `			ph7_result_null(pCtx);` |
|    ! 0 | 1677 | `			return PH7_OK;` |
|      - | 1678 | `		}` |
|     28 | 1679 | `		PH7_MemObjInit(pCtx->pVm, &sKey);` |
|     28 | 1680 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     28 | 1681 | `		pNode = pSubMap ? pSubMap->pFirst : 0;` |
|     28 | 1682 | `		n = pSubMap ? pSubMap->nEntry : 0;` |
|     64 | 1683 | `		while( n > 0 ){` |
|      - | 1684 | `			const char *zSubject;` |
|      - | 1685 | `			int nSubLen;` |
|      - | 1686 | `			SyBlob sOut;` |
|     42 | 1687 | `			PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|     42 | 1688 | `			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|     42 | 1689 | `			if( !PcreStrUV(pCtx, &sVal, &zSubject, &nSubLen) ){` |
|    ! 0 | 1690 | `				PH7_MemObjRelease(&sKey);` |
|    ! 0 | 1691 | `				PH7_MemObjRelease(&sVal);` |
|    ! 0 | 1692 | `				ph7_result_value(pCtx, pResult);` |
|    ! 0 | 1693 | `				goto set_count;` |
|      - | 1694 | `			}` |
|     42 | 1695 | `			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     62 | 1696 | `			rc = PcreCallbackReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen,` |
|     20 | 1697 | `				limit, iFlags, &count, &sOut);` |
|     42 | 1698 | `			if( rc != SXRET_OK ){` |
|      - | 1699 | `				/* A refused pattern drops THIS subject and php moves to the next,` |
|      - | 1700 | `				 * so the refusal is reported once per subject and the answer is` |
|      - | 1701 | `				 * the empty array. A throwing callback, and a coercion that` |
|      - | 1702 | `				 * threw, stop the call instead. */` |
|     16 | 1703 | `				SyBlobRelease(&sOut);` |
|     16 | 1704 | `				PH7_MemObjRelease(&sKey);` |
|     16 | 1705 | `				PH7_MemObjRelease(&sVal);` |
|     16 | 1706 | `				if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      6 | 1707 | `					pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|      6 | 1708 | `					return rc;` |
|      - | 1709 | `				}` |
|     11 | 1710 | `				if( pCtx->nThrowRc ){` |
|    ! 0 | 1711 | `					ph7_result_value(pCtx, pResult);` |
|    ! 0 | 1712 | `					goto set_count;` |
|      - | 1713 | `				}` |
|     11 | 1714 | `				pNode = pNode->pPrev;` |
|     11 | 1715 | `				n--;` |
|     11 | 1716 | `				continue;` |
|      - | 1717 | `			}` |
|     27 | 1718 | `			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     27 | 1719 | `			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */` |
|     27 | 1720 | `			ph7_value_reset_string_cursor(pElem);` |
|     27 | 1721 | `			SyBlobRelease(&sOut);` |
|     27 | 1722 | `			PH7_MemObjRelease(&sKey);` |
|     27 | 1723 | `			PH7_MemObjRelease(&sVal);` |
|     27 | 1724 | `			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|     27 | 1725 | `			n--;` |
|      1 | 1726 | `		}` |
|     23 | 1727 | `		ph7_result_value(pCtx, pResult);` |
|     12 | 1728 | `	}else{` |
|      - | 1729 | `		/* Scalar subject: one replaced string. */` |
|      - | 1730 | `		const char *zSubject;` |
|      - | 1731 | `		int nSubLen;` |
|      - | 1732 | `		SyBlob sOut;` |
|    221 | 1733 | `		if( !PcreStrUV(pCtx, apArg[2], &zSubject, &nSubLen) ){` |
|    ! 0 | 1734 | `			ph7_result_null(pCtx);` |
|    ! 0 | 1735 | `			goto set_count;` |
|      - | 1736 | `		}` |
|    221 | 1737 | `		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    329 | 1738 | `		rc = PcreCallbackReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen,` |
|    108 | 1739 | `			limit, iFlags, &count, &sOut);` |
|    221 | 1740 | `		if( rc != SXRET_OK ){` |
|     27 | 1741 | `			SyBlobRelease(&sOut);` |
|     27 | 1742 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      - | 1743 | `				/* php records the aborted run: preg_last_error() reads` |
|      - | 1744 | `				 * PREG_INTERNAL_ERROR after a callback that threw. */` |
|     15 | 1745 | `				pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;` |
|     15 | 1746 | `				return rc;` |
|      - | 1747 | `			}` |
|      - | 1748 | `			/* Scalar subject: a bad pattern returns NULL (php). */` |
|     13 | 1749 | `			ph7_result_null(pCtx);` |
|     13 | 1750 | `			goto set_count;` |
|      - | 1751 | `		}` |
|    197 | 1752 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    197 | 1753 | `		SyBlobRelease(&sOut);` |
|      - | 1754 | `	}` |
|    113 | 1755 | `set_count:` |
|      - | 1756 | `	/* Set &$count if provided — written on success AND on a bad-pattern failure` |
|      - | 1757 | `	 * (php always writes it: 0, or the count accumulated by earlier good patterns).` |
|      - | 1758 | `	 * A coercion that THREW is not one of those: php raises out of the call. */` |
|    231 | 1759 | `	if( nArg >= 5 && pCtx->nThrowRc == 0 ){` |
|      - | 1760 | `		ph7_value sCount;` |
|     64 | 1761 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);` |
|     64 | 1762 | `		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);` |
|     64 | 1763 | `		PH7_MemObjRelease(&sCount);` |
|     31 | 1764 | `	}` |
|    231 | 1765 | `	return PH7_OK;` |
|    133 | 1766 | `}` |
|      - | 1767 |  |
|      - | 1768 | `/* ======================================================================` |
|      - | 1769 | ` * preg_grep(pattern, array [, flags])` |
|      - | 1770 | ` *` |
|      - | 1771 | ` * php compiles the pattern ONCE, before it looks at the array at all: a refused` |
|      - | 1772 | ` * pattern is one diagnostic under preg_grep's own name and the answer FALSE,` |
|      - | 1773 | ` * even for an empty array. Written as embedded PHP over preg_match() this said` |
|      - | 1774 | `` * `preg_match():` once PER ELEMENT, answered an ARRAY (every element of it, with`` |
|      - | 1775 | ` * PREG_GREP_INVERT), and never spoke at all when the array was empty -- so a` |
|      - | 1776 | `` * caller testing `=== false` saw a successful filter that had matched nothing.`` |
|      - | 1777 | ` * ====================================================================== */` |
|     20 | 1778 | `static int PH7_builtin_preg_grep(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 1779 | `{` |
|      - | 1780 | `	const char *zPattern;` |
|      - | 1781 | `	int nPatLen;` |
|      - | 1782 | `	pcre2_code *pCode;` |
|      - | 1783 | `	pcre2_match_data *pMatchData;` |
|      - | 1784 | `	sxu32 nCapture;` |
|     22 | 1785 | `	int bInvert = 0;` |
|      - | 1786 | `	ph7_value *pResult;` |
|      - | 1787 | `	ph7_hashmap *pMap;` |
|      - | 1788 | `	ph7_hashmap_node *pNode;` |
|      - | 1789 | `	ph7_value sKey, sVal, sStr;` |
|      - | 1790 | `	sxu32 n;` |
|      - | 1791 |  |
|      - | 1792 | `	/* aBuiltinSig[] enforces php's arity before the call; this is the backstop` |
|      - | 1793 | `	 * that keeps a missing row from turning into an out-of-bounds apArg read. */` |
|     22 | 1794 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|    ! 0 | 1795 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 1796 | `		return PH7_OK;` |
|      - | 1797 | `	}` |
|     22 | 1798 | `	zPattern = ph7_value_to_string(apArg[0], &nPatLen);` |
|     22 | 1799 | `	if( nArg >= 3 ){` |
|      8 | 1800 | `		bInvert = (ph7_value_to_int(apArg[2]) & PHP_PREG_GREP_INVERT) != 0;` |
|      3 | 1801 | `	}` |
|     22 | 1802 | `	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);` |
|     22 | 1803 | `	if( pCode == 0 ){` |
|      7 | 1804 | `		ph7_result_bool(pCtx, 0);` |
|      7 | 1805 | `		return PH7_OK;` |
|      - | 1806 | `	}` |
|     16 | 1807 | `	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);` |
|     16 | 1808 | `	if( pMatchData == 0 ){` |
|    ! 0 | 1809 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 1810 | `		return PH7_OK;` |
|      - | 1811 | `	}` |
|     16 | 1812 | `	pResult = ph7_context_new_array(pCtx);` |
|     16 | 1813 | `	if( pResult == 0 ){` |
|    ! 0 | 1814 | `		pcre2_match_data_free(pMatchData);` |
|    ! 0 | 1815 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 1816 | `		return PH7_OK;` |
|      - | 1817 | `	}` |
|     16 | 1818 | `	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;` |
|     16 | 1819 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     16 | 1820 | `	PH7_MemObjInit(pCtx->pVm, &sKey);` |
|     16 | 1821 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     16 | 1822 | `	PH7_MemObjInit(pCtx->pVm, &sStr);` |
|     16 | 1823 | `	pNode = pMap ? pMap->pFirst : 0;` |
|     16 | 1824 | `	n = pMap ? pMap->nEntry : 0;` |
|     54 | 1825 | `	while( n > 0 ){` |
|      - | 1826 | `		const char *zSubject;` |
|      - | 1827 | `		int nSubLen, bKeep, rc;` |
|     40 | 1828 | `		PH7_HashmapExtractNodeKey(pNode, &sKey);` |
|     40 | 1829 | `		PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);` |
|      - | 1830 | `		/* The MATCH is made against the element's string form; what is KEPT is` |
|      - | 1831 | `		 * the element itself, so an int or a float comes back out as it went in.` |
|      - | 1832 | `		 * PcreStrUV coerces in place, hence the second slot. */` |
|     40 | 1833 | `		PH7_MemObjRelease(&sStr);` |
|     40 | 1834 | `		PH7_MemObjStore(&sVal, &sStr);` |
|     40 | 1835 | `		sStr.nIdx = SXU32_HIGH;` |
|     40 | 1836 | `		if( !PcreStrUV(pCtx, &sStr, &zSubject, &nSubLen) ){` |
|      - | 1837 | `			/* A coercion that threw stops the call where php stops it. */` |
|    ! 0 | 1838 | `			PH7_MemObjRelease(&sKey);` |
|    ! 0 | 1839 | `			PH7_MemObjRelease(&sVal);` |
|    ! 0 | 1840 | `			break;` |
|      - | 1841 | `		}` |
|     59 | 1842 | `		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,` |
|     19 | 1843 | `			0, 0, pMatchData, NULL);` |
|     40 | 1844 | `		if( rc < 0 && rc != PCRE2_ERROR_NOMATCH ){` |
|    ! 0 | 1845 | `			PcreSetMatchError(pCtx->pVm, rc);` |
|    ! 0 | 1846 | `		}` |
|     40 | 1847 | `		bKeep = (rc >= 0);` |
|     40 | 1848 | `		if( bInvert ){` |
|     10 | 1849 | `			bKeep = !bKeep;` |
|      4 | 1850 | `		}` |
|     40 | 1851 | `		if( bKeep ){` |
|     22 | 1852 | `			ph7_array_add_elem(pResult, &sKey, &sVal); /* copies key+value */` |
|     10 | 1853 | `		}` |
|     40 | 1854 | `		PH7_MemObjRelease(&sKey);` |
|     40 | 1855 | `		PH7_MemObjRelease(&sVal);` |
|     40 | 1856 | `		pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|     40 | 1857 | `		n--;` |
|      2 | 1858 | `	}` |
|     16 | 1859 | `	PH7_MemObjRelease(&sStr);` |
|     16 | 1860 | `	pcre2_match_data_free(pMatchData);` |
|     16 | 1861 | `	ph7_result_value(pCtx, pResult);` |
|     16 | 1862 | `	return PH7_OK;` |
|     12 | 1863 | `}` |
|      - | 1864 |  |
|      - | 1865 | `/* ======================================================================` |
|      - | 1866 | ` * preg_replace_callback_array(pattern, subject [, limit [, &count [, flags]]])` |
|      - | 1867 | ` *` |
|      - | 1868 | ` * One pass of preg_replace_callback() per entry, each fed the PREVIOUS entry's` |
|      - | 1869 | ` * output, and php validates each entry only when it reaches it -- so with` |
|      - | 1870 | ` * ['/a/' => good, '/b/' => 'nosuchfn'] the first replacement has already` |
|      - | 1871 | ` * happened when the TypeError for the second is raised. A refused pattern` |
|      - | 1872 | ` * answers NULL and leaves &$count untouched; an ARRAY subject is not that kind` |
|      - | 1873 | ` * of failure and degrades to the empty array with count 0.` |
|      - | 1874 | ` * ====================================================================== */` |
|     36 | 1875 | `static int PH7_builtin_preg_replace_callback_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 1876 | `{` |
|      - | 1877 | `	ph7_value sSubject, sPat, sCb, sLimit, sCount, sFlags;` |
|      - | 1878 | `	ph7_value *apDeleg[6];` |
|      - | 1879 | `	ph7_hashmap *pMap;` |
|      - | 1880 | `	ph7_hashmap_node *pNode;` |
|      - | 1881 | `	sxu32 n;` |
|     38 | 1882 | `	int total = 0;` |
|     38 | 1883 | `	int bFailed = 0;` |
|     38 | 1884 | `	sxi32 rc = PH7_OK;` |
|      - | 1885 |  |
|      - | 1886 | `	/* Same backstop as preg_grep's: the arity php reports is aBuiltinSig[]'s. */` |
|     38 | 1887 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 | 1888 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1889 | `		return PH7_OK;` |
|      - | 1890 | `	}` |
|     38 | 1891 | `	PH7_MemObjInit(pCtx->pVm, &sSubject);` |
|     38 | 1892 | `	PH7_MemObjInit(pCtx->pVm, &sPat);` |
|     38 | 1893 | `	PH7_MemObjInit(pCtx->pVm, &sCb);` |
|     38 | 1894 | `	PH7_MemObjInitFromInt(pCtx->pVm, &sLimit, nArg >= 3 ? ph7_value_to_int(apArg[2]) : -1);` |
|     38 | 1895 | `	PH7_MemObjInitFromInt(pCtx->pVm, &sCount, 0);` |
|     38 | 1896 | `	PH7_MemObjInitFromInt(pCtx->pVm, &sFlags, nArg >= 5 ? ph7_value_to_int(apArg[4]) : 0);` |
|      - | 1897 | `	/* &$count out-slot with no caller variable behind it: PH7_VmStoreArgByRef` |
|      - | 1898 | `	 * writes through nIdx when it is not SXU32_HIGH, and a zeroed ph7_value's` |
|      - | 1899 | `	 * nIdx is 0 -- a REAL slot index, which would corrupt aMemObj[0]. */` |
|     38 | 1900 | `	sCount.nIdx = SXU32_HIGH;` |
|     38 | 1901 | `	PH7_MemObjStore(apArg[1], &sSubject);` |
|     38 | 1902 | `	sSubject.nIdx = SXU32_HIGH;` |
|     38 | 1903 | `	apDeleg[0] = &sPat;` |
|     38 | 1904 | `	apDeleg[1] = &sCb;` |
|     38 | 1905 | `	apDeleg[2] = &sSubject;` |
|     38 | 1906 | `	apDeleg[3] = &sLimit;` |
|     38 | 1907 | `	apDeleg[4] = &sCount;` |
|     38 | 1908 | `	apDeleg[5] = &sFlags;` |
|     38 | 1909 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     38 | 1910 | `	pNode = pMap ? pMap->pFirst : 0;` |
|     38 | 1911 | `	n = pMap ? pMap->nEntry : 0;` |
|     72 | 1912 | `	while( n > 0 ){` |
|     50 | 1913 | `		PH7_HashmapExtractNodeKey(pNode, &sPat);` |
|     50 | 1914 | `		PH7_HashmapExtractNodeValue(pNode, &sCb, FALSE);` |
|     50 | 1915 | `		if( (sPat.iFlags & MEMOBJ_STRING) == 0 ){` |
|      - | 1916 | `			/* An INT key is not a pattern; php names the whole argument. */` |
|      3 | 1917 | `			rc = PH7_VmThrowException(pCtx, "TypeError",` |
|      - | 1918 | `				"preg_replace_callback_array(): Argument #1 ($pattern) must contain "` |
|      - | 1919 | `				"only string patterns as keys");` |
|      3 | 1920 | `			goto done;` |
|      - | 1921 | `		}` |
|     48 | 1922 | `		if( !ph7_value_is_callable(&sCb) ){` |
|      3 | 1923 | `			rc = PH7_VmThrowException(pCtx, "TypeError",` |
|      - | 1924 | `				"preg_replace_callback_array(): Argument #1 ($pattern) must contain "` |
|      - | 1925 | `				"only valid callbacks");` |
|      3 | 1926 | `			goto done;` |
|      - | 1927 | `		}` |
|      - | 1928 | `		/* pCtx->pRet ACCUMULATES -- ph7_result_string() appends to the return` |
|      - | 1929 | `		 * slot's blob -- so each delegated pass has to start from an empty one` |
|      - | 1930 | `		 * or the second entry's answer is glued onto the first's. */` |
|     46 | 1931 | `		ph7_result_null(pCtx);` |
|     46 | 1932 | `		rc = PH7_builtin_preg_replace_callback(pCtx, 6, apDeleg);` |
|     46 | 1933 | `		if( rc != PH7_OK ){` |
|      3 | 1934 | `			goto done;   /* the callback threw; it is already unwinding */` |
|      - | 1935 | `		}` |
|     44 | 1936 | `		if( ph7_value_is_null(pCtx->pRet) ){` |
|      - | 1937 | `			/* A refused pattern: NULL, and &$count keeps whatever it held. */` |
|      9 | 1938 | `			bFailed = 1;` |
|      9 | 1939 | `			goto done;` |
|      - | 1940 | `		}` |
|     36 | 1941 | `		total += ph7_value_to_int(&sCount);` |
|      - | 1942 | `		/* This entry's output is the next entry's subject */` |
|     36 | 1943 | `		PH7_MemObjStore(pCtx->pRet, &sSubject);` |
|     36 | 1944 | `		sSubject.nIdx = SXU32_HIGH;` |
|     36 | 1945 | `		PH7_MemObjRelease(&sPat);` |
|     36 | 1946 | `		PH7_MemObjRelease(&sCb);` |
|     36 | 1947 | `		pNode = pNode->pPrev; /* insertion-order walk (reverse link) */` |
|     36 | 1948 | `		n--;` |
|      2 | 1949 | `	}` |
|     11 | 1950 | `done:` |
|     38 | 1951 | `	ph7_result_null(pCtx);` |
|     38 | 1952 | `	if( !bFailed && rc == PH7_OK ){` |
|     24 | 1953 | `		ph7_result_value(pCtx, &sSubject);` |
|     24 | 1954 | `		if( nArg >= 4 ){` |
|      - | 1955 | `			ph7_value sTotal;` |
|     17 | 1956 | `			PH7_MemObjInitFromInt(pCtx->pVm, &sTotal, total);` |
|     17 | 1957 | `			PH7_VmStoreArgByRef(pCtx->pVm, apArg[3], &sTotal);` |
|     17 | 1958 | `			PH7_MemObjRelease(&sTotal);` |
|      8 | 1959 | `		}` |
|     11 | 1960 | `	}` |
|     38 | 1961 | `	PH7_MemObjRelease(&sSubject);` |
|     38 | 1962 | `	PH7_MemObjRelease(&sPat);` |
|     38 | 1963 | `	PH7_MemObjRelease(&sCb);` |
|     38 | 1964 | `	PH7_MemObjRelease(&sLimit);` |
|     38 | 1965 | `	PH7_MemObjRelease(&sCount);` |
|     38 | 1966 | `	PH7_MemObjRelease(&sFlags);` |
|     38 | 1967 | `	return rc;` |
|     20 | 1968 | `}` |
|      - | 1969 |  |
|      - | 1970 | `/* ======================================================================` |
|      - | 1971 | ` * preg_quote(str [, delimiter])` |
|      - | 1972 | ` * ====================================================================== */` |
|     32 | 1973 | `static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 1974 | `{` |
|     35 | 1975 | `	const char *zStr, *zDelim = 0;` |
|     35 | 1976 | `	int nLen, nDelimLen = 0;` |
|      - | 1977 | `	const char *z, *zEnd;` |
|      - | 1978 |  |
|     35 | 1979 | `	if( nArg < 1 ){` |
|    ! 0 | 1980 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1981 | `		return PH7_OK;` |
|      - | 1982 | `	}` |
|     35 | 1983 | `	zStr = ph7_value_to_string(apArg[0], &nLen);` |
|     35 | 1984 | `	if( nArg >= 2 ){` |
|     18 | 1985 | `		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);` |
|      8 | 1986 | `	}` |
|      - | 1987 | `	/* Type the result as a STRING up front: an empty subject quotes to the empty` |
|      - | 1988 | `	 * string, and the loop below would otherwise never touch the result at all,` |
|      - | 1989 | ``	 * leaving php's `string` return as NULL. */`` |
|     35 | 1990 | `	ph7_result_string(pCtx, "", 0);` |
|     35 | 1991 | `	z = zStr;` |
|     35 | 1992 | `	zEnd = &zStr[nLen];` |
|    822 | 1993 | `	while( z < zEnd ){` |
|    790 | 1994 | `		char c = *z;` |
|    790 | 1995 | `		if( c == '\0' ){` |
|      - | 1996 | `			/* php spells NUL as the three-digit escape "\000" (a backslash and a raw NUL` |
|      - | 1997 | `			 * byte, which is what this emitted, is not an escape at all: pcre reads the` |
|      - | 1998 | `			 * backslash as quoting the byte that FOLLOWS the NUL). */` |
|      9 | 1999 | `			ph7_result_string(pCtx, "\\000", 4);` |
|      9 | 2000 | `			z++;` |
|      9 | 2001 | `			continue;` |
|      - | 2002 | `		}` |
|    782 | 2003 | `		switch( c ){` |
|     33 | 2004 | `			case '.': case '\\': case '+': case '*': case '?':` |
|      - | 2005 | `			case '[': case '^': case ']': case '$': case '(':` |
|      - | 2006 | `			case ')': case '{': case '}': case '=': case '!':` |
|      - | 2007 | `			case '<': case '>': case '\|': case ':': case '-':` |
|      - | 2008 | `			case '#':` |
|     69 | 2009 | `				ph7_result_string(pCtx, "\\", 1);` |
|     69 | 2010 | `				break;` |
|    356 | 2011 | `			default:` |
|    716 | 2012 | `				if( nDelimLen > 0 && c == zDelim[0] ){` |
|      6 | 2013 | `					ph7_result_string(pCtx, "\\", 1);` |
|      2 | 2014 | `				}` |
|    713 | 2015 | `				break;` |
|      - | 2016 | `		}` |
|    782 | 2017 | `		ph7_result_string(pCtx, z, 1);` |
|    782 | 2018 | `		z++;` |
|      3 | 2019 | `	}` |
|     35 | 2020 | `	return PH7_OK;` |
|     19 | 2021 | `}` |
|      - | 2022 |  |
|      - | 2023 | `/* ======================================================================` |
|      - | 2024 | ` * preg_last_error()` |
|      - | 2025 | ` * ====================================================================== */` |
|    100 | 2026 | `static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2027 | `{` |
|     50 | 2028 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    101 | 2029 | `	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);` |
|    101 | 2030 | `	return PH7_OK;` |
|      1 | 2031 | `}` |
|      - | 2032 |  |
|      - | 2033 | `/* ======================================================================` |
|      - | 2034 | ` * preg_last_error_msg()` |
|      - | 2035 | ` * ====================================================================== */` |
|    100 | 2036 | `static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2037 | `{` |
|      - | 2038 | `	const char *zMsg;` |
|     50 | 2039 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    101 | 2040 | `	switch( pCtx->pVm->iPcreLastError ){` |
|     57 | 2041 | `		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;` |
|     45 | 2042 | `		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;` |
|    ! 0 | 2043 | `		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;` |
|    ! 0 | 2044 | `		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;` |
|    ! 0 | 2045 | `		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;` |
|    ! 0 | 2046 | `		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;` |
|    ! 0 | 2047 | `		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;` |
|    ! 0 | 2048 | `		default: zMsg = "Unknown error"; break;` |
|      - | 2049 | `	}` |
|    101 | 2050 | `	ph7_result_string(pCtx, zMsg, -1);` |
|    101 | 2051 | `	return PH7_OK;` |
|      1 | 2052 | `}` |
|      - | 2053 |  |
|      - | 2054 | `/*` |
|      - | 2055 | ` * The regex operation behind RegexIterator::accept(), in php's five REGIT modes.` |
|      - | 2056 | ` *` |
|      - | 2057 | ` * php reaches php_pcre_match_impl / php_pcre_split_impl / php_pcre_replace_impl` |
|      - | 2058 | ` * from spl_iterators.c rather than re-deriving any of it, and this is that door:` |
|      - | 2059 | ` * every mode is one of the builtins above, called with the arguments the PHP` |
|      - | 2060 | ` * spelling would have passed. The builtins answer through pCtx->pRet, which is` |
|      - | 2061 | ` * also accept()'s own return slot -- harmless because the caller writes its` |
|      - | 2062 | ` * boolean after this returns, and the reason pOut is a separate parameter.` |
|      - | 2063 | ` *` |
|      - | 2064 | ` * *pbOk is php's per-mode "matched" test: a positive match count, more than one` |
|      - | 2065 | ` * SPLIT piece, at least one REPLACE substitution. pOut receives the transformed` |
|      - | 2066 | ` * value for every mode but MATCH, which leaves the cached current() alone.` |
|      - | 2067 | ` */` |
|    100 | 2068 | `PH7_PRIVATE sxi32 PH7_PcreRegitApply(` |
|      - | 2069 | `	ph7_context *pCtx,` |
|      - | 2070 | `	int iMode,               /* PH7_REGIT_* */` |
|      - | 2071 | `	ph7_value *pPattern,` |
|      - | 2072 | `	ph7_value *pSubject,` |
|      - | 2073 | `	int iPregFlags,` |
|      - | 2074 | `	ph7_value *pRepl,        /* REPLACE only */` |
|      - | 2075 | `	ph7_value *pOut,         /* transformed value (modes other than MATCH) */` |
|      - | 2076 | `	int *pbOk` |
|      - | 2077 | `	)` |
|      2 | 2078 | `{` |
|      - | 2079 | `	ph7_value *apArg[5];` |
|      - | 2080 | `	ph7_value sFlags, sLimit, sCount;` |
|    102 | 2081 | `	sxi32 rc = PH7_OK;` |
|    102 | 2082 | `	*pbOk = 0;` |
|    102 | 2083 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sFlags,iPregFlags);` |
|    102 | 2084 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sLimit,-1);` |
|    102 | 2085 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sCount,0);` |
|      - | 2086 | `	/* A by-ref out-parameter with no caller slot behind it: PH7_VmStoreArgByRef` |
|      - | 2087 | `	 * writes through nIdx when it is not SXU32_HIGH, and a zeroed ph7_value's` |
|      - | 2088 | `	 * nIdx is 0 -- a REAL slot index, which would corrupt aMemObj[0]. */` |
|    102 | 2089 | `	sCount.nIdx = SXU32_HIGH;` |
|    102 | 2090 | `	if( pOut ){` |
|    102 | 2091 | `		pOut->nIdx = SXU32_HIGH;` |
|     50 | 2092 | `	}` |
|    102 | 2093 | `	apArg[0] = pPattern;` |
|    102 | 2094 | `	switch( iMode ){` |
|     27 | 2095 | `		case PH7_REGIT_MATCH:` |
|     56 | 2096 | `			apArg[1] = pSubject;` |
|     56 | 2097 | `			rc = PH7_builtin_preg_match(pCtx,2,apArg);` |
|     56 | 2098 | `			*pbOk = ph7_value_to_int(pCtx->pRet) > 0;` |
|     56 | 2099 | `			break;` |
|     12 | 2100 | `		case PH7_REGIT_GET_MATCH:` |
|      - | 2101 | `		case PH7_REGIT_ALL_MATCHES:` |
|     25 | 2102 | `			apArg[1] = pSubject;` |
|     25 | 2103 | `			apArg[2] = pOut;` |
|     25 | 2104 | `			apArg[3] = &sFlags;` |
|     25 | 2105 | `			rc = iMode == PH7_REGIT_GET_MATCH` |
|     20 | 2106 | `				? PH7_builtin_preg_match(pCtx,4,apArg)` |
|     14 | 2107 | `				: PH7_builtin_preg_match_all(pCtx,4,apArg);` |
|     25 | 2108 | `			*pbOk = ph7_value_to_int(pCtx->pRet) > 0;` |
|     25 | 2109 | `			break;` |
|      3 | 2110 | `		case PH7_REGIT_SPLIT:` |
|      7 | 2111 | `			apArg[1] = pSubject;` |
|      7 | 2112 | `			apArg[2] = &sLimit;` |
|      7 | 2113 | `			apArg[3] = &sFlags;` |
|      7 | 2114 | `			rc = PH7_builtin_preg_split(pCtx,4,apArg);` |
|      7 | 2115 | `			PH7_MemObjStore(pCtx->pRet,pOut);` |
|      7 | 2116 | `			if( pOut->iFlags & MEMOBJ_HASHMAP ){` |
|      7 | 2117 | `				*pbOk = ((ph7_hashmap *)pOut->x.pOther)->nEntry > 1;` |
|      3 | 2118 | `			}` |
|      7 | 2119 | `			break;` |
|      8 | 2120 | `		case PH7_REGIT_REPLACE:` |
|     17 | 2121 | `			apArg[1] = pRepl;` |
|     17 | 2122 | `			apArg[2] = pSubject;` |
|     17 | 2123 | `			apArg[3] = &sLimit;` |
|     17 | 2124 | `			apArg[4] = &sCount;` |
|     17 | 2125 | `			rc = PH7_builtin_preg_replace(pCtx,5,apArg);` |
|     17 | 2126 | `			PH7_MemObjStore(pCtx->pRet,pOut);` |
|     17 | 2127 | `			*pbOk = ph7_value_to_int(&sCount) > 0;` |
|     16 | 2128 | `			break;` |
|    ! 0 | 2129 | `		default:` |
|    ! 0 | 2130 | `			break;` |
|      - | 2131 | `	}` |
|    102 | 2132 | `	PH7_MemObjRelease(&sFlags);` |
|    102 | 2133 | `	PH7_MemObjRelease(&sLimit);` |
|    102 | 2134 | `	PH7_MemObjRelease(&sCount);` |
|    102 | 2135 | `	return rc;` |
|      2 | 2136 | `}` |
|      - | 2137 | `/* ===== Function registration table ===== */` |
|      - | 2138 | `/* ======================================================================` |
|      - | 2139 | ` * mbstring's regular expressions: mb_ereg*, mb_split, mb_regex_encoding` |
|      - | 2140 | ` * and mb_regex_set_options.` |
|      - | 2141 | ` *` |
|      - | 2142 | ` * php runs this family on Oniguruma, not on PCRE2, and the two disagree in` |
|      - | 2143 | ` * two places that matter and one that does not:` |
|      - | 2144 | ` *` |
|      - | 2145 | ` *  - Oniguruma's ONIG_OPTION_MULTILINE is PCRE2's DOTALL (dot matches a` |
|      - | 2146 | ` *    newline), and its ONIG_OPTION_SINGLELINE is the ABSENCE of PCRE2's` |
|      - | 2147 | `` *    MULTILINE (`^` is `\A` and `$` is `\Z`). mbstring's default option`` |
|      - | 2148 | ` *    string is "pr" -- both of those bits, ruby syntax -- so the family` |
|      - | 2149 | ` *    starts out with a dot that crosses lines and anchors that do not,` |
|      - | 2150 | ` *    which is the exact opposite of what the letters look like they say.` |
|      - | 2151 | `` *  - `l` (find-longest) and `n` (find-not-empty) have no PCRE2 spelling.`` |
|      - | 2152 | ` *    They are ACCEPTED and reported back by mb_regex_set_options(), and` |
|      - | 2153 | ` *    they do not change the match. So are the eight syntax letters: the` |
|      - | 2154 | ` *    grammar is PCRE2's whichever one is named.` |
|      - | 2155 | ` *  - a pattern that does not compile warns with PCRE2's message text where` |
|      - | 2156 | ` *    php prints Oniguruma's. The diagnostic's shape -- a warning naming the` |
|      - | 2157 | ` *    function, "mbregex compile err: ", and a false return -- is php's.` |
|      - | 2158 | ` *` |
|      - | 2159 | ` * The subject is matched as bytes; only UTF-8 is handed to PCRE2 as text` |
|      - | 2160 | `` * (with UCP, so `\w` covers the same letters Oniguruma's does). Every`` |
|      - | 2161 | ` * offset this family reports or takes is a byte offset, which is php's.` |
|      - | 2162 | ` * ====================================================================== */` |
|      - | 2163 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2164 |  |
|      - | 2165 | `#define MBRE_OPT_SET         0x0100  /* "a script has set these", so 0 reads as the default */` |
|      - | 2166 | `#define MBRE_OPT_IGNORECASE  0x0001` |
|      - | 2167 | `#define MBRE_OPT_EXTEND      0x0002` |
|      - | 2168 | `#define MBRE_OPT_MULTILINE   0x0004  /* onig: dot matches newline */` |
|      - | 2169 | `#define MBRE_OPT_SINGLELINE  0x0008  /* onig: ^ is \A and $ is \Z */` |
|      - | 2170 | `#define MBRE_OPT_LONGEST     0x0010  /* accepted, no PCRE2 spelling */` |
|      - | 2171 | `#define MBRE_OPT_NOTEMPTY    0x0020  /* accepted, no PCRE2 spelling */` |
|      - | 2172 |  |
|      - | 2173 | `#define MBRE_OPT_DEFAULT (MBRE_OPT_MULTILINE\|MBRE_OPT_SINGLELINE)` |
|      - | 2174 | `#define MBRE_SYNTAX_DEFAULT 'r'` |
|      - | 2175 |  |
|    186 | 2176 | `static sxu32 MbReOptOf(ph7_vm *pVm)` |
|      2 | 2177 | `{` |
|    188 | 2178 | `	return pVm->iMbReOpt ? (pVm->iMbReOpt & ~(sxu32)MBRE_OPT_SET) : (sxu32)MBRE_OPT_DEFAULT;` |
|      2 | 2179 | `}` |
|    236 | 2180 | `static int MbReSyntaxOf(ph7_vm *pVm)` |
|      2 | 2181 | `{` |
|    238 | 2182 | `	return pVm->iMbReSyntax ? (int)pVm->iMbReSyntax : MBRE_SYNTAX_DEFAULT;` |
|      2 | 2183 | `}` |
|      - | 2184 | `/* Read an option string. The option bits are REPLACED wholesale (an empty` |
|      - | 2185 | ` * string clears them all); the syntax letter is replaced only when one is` |
|      - | 2186 | ` * present, which is why mb_regex_set_options('i') answers "ir". */` |
|     54 | 2187 | `static int MbReParseOpt(const char *z,int n,sxu32 *pOpt,int *pSyntax,unsigned char *pBad)` |
|      2 | 2188 | `{` |
|     56 | 2189 | `	sxu32 opt = 0;` |
|      - | 2190 | `	int i;` |
|    160 | 2191 | `	for( i = 0 ; i < n ; ++i ){` |
|    108 | 2192 | `		switch( z[i] ){` |
|     18 | 2193 | `		case 'i': opt \|= MBRE_OPT_IGNORECASE; break;` |
|      5 | 2194 | `		case 'x': opt \|= MBRE_OPT_EXTEND; break;` |
|      7 | 2195 | `		case 'm': opt \|= MBRE_OPT_MULTILINE; break;` |
|      7 | 2196 | `		case 's': opt \|= MBRE_OPT_SINGLELINE; break;` |
|     29 | 2197 | `		case 'p': opt \|= MBRE_OPT_MULTILINE\|MBRE_OPT_SINGLELINE; break;` |
|      5 | 2198 | `		case 'l': opt \|= MBRE_OPT_LONGEST; break;` |
|      5 | 2199 | `		case 'n': opt \|= MBRE_OPT_NOTEMPTY; break;` |
|      - | 2200 | `		/* The syntax letters: java, gnu, grep, emacs, ruby, perl and the two` |
|      - | 2201 | `		 * POSIX grammars. PCRE2 answers all eight. */` |
|     18 | 2202 | `		case 'j': case 'u': case 'g': case 'c':` |
|      - | 2203 | `		case 'r': case 'z': case 'b': case 'd':` |
|     37 | 2204 | `			*pSyntax = (unsigned char)z[i];` |
|     37 | 2205 | `			break;` |
|      1 | 2206 | `		default:` |
|      3 | 2207 | `			*pBad = (unsigned char)z[i];` |
|      3 | 2208 | `			return -1;` |
|      - | 2209 | `		}` |
|     54 | 2210 | `	}` |
|     54 | 2211 | `	*pOpt = opt;` |
|     54 | 2212 | `	return 0;` |
|     29 | 2213 | `}` |
|      - | 2214 | `/* ...and write one back. php's order is i, x, the line pair, l, n, syntax --` |
|      - | 2215 | ` * and MULTILINE\|SINGLELINE together collapse to the single letter 'p'. */` |
|     74 | 2216 | `static int MbReOptString(sxu32 opt,int iSyntax,char *zBuf)` |
|      2 | 2217 | `{` |
|     76 | 2218 | `	int n = 0;` |
|     76 | 2219 | `	if( opt & MBRE_OPT_IGNORECASE ){ zBuf[n++] = 'i'; }` |
|     76 | 2220 | `	if( opt & MBRE_OPT_EXTEND ){ zBuf[n++] = 'x'; }` |
|     74 | 2221 | `	if( (opt & (MBRE_OPT_MULTILINE\|MBRE_OPT_SINGLELINE))` |
|     39 | 2222 | `		== (MBRE_OPT_MULTILINE\|MBRE_OPT_SINGLELINE) ){` |
|     50 | 2223 | `		zBuf[n++] = 'p';` |
|     26 | 2224 | `	}else{` |
|     27 | 2225 | `		if( opt & MBRE_OPT_MULTILINE ){ zBuf[n++] = 'm'; }` |
|     27 | 2226 | `		if( opt & MBRE_OPT_SINGLELINE ){ zBuf[n++] = 's'; }` |
|      - | 2227 | `	}` |
|     76 | 2228 | `	if( opt & MBRE_OPT_LONGEST ){ zBuf[n++] = 'l'; }` |
|     76 | 2229 | `	if( opt & MBRE_OPT_NOTEMPTY ){ zBuf[n++] = 'n'; }` |
|     76 | 2230 | `	zBuf[n++] = (char)iSyntax;` |
|     76 | 2231 | `	zBuf[n] = 0;` |
|     76 | 2232 | `	return n;` |
|      2 | 2233 | `}` |
|      - | 2234 | `/* The optional $options argument every matcher carries. It does NOT touch the` |
|      - | 2235 | ` * VM's own options -- mb_ereg_replace($p,$r,$s,'i') leaves` |
|      - | 2236 | ` * mb_regex_set_options() reading what it read before. */` |
|    112 | 2237 | `static int MbReOptArg(ph7_context *pCtx,ph7_value *pArg,sxu32 *pOpt,int *pSyntax)` |
|      2 | 2238 | `{` |
|      - | 2239 | `	const char *zOpt;` |
|      - | 2240 | `	int nOpt;` |
|    114 | 2241 | `	unsigned char cBad = 0;` |
|    114 | 2242 | `	*pOpt = MbReOptOf(pCtx->pVm);` |
|    114 | 2243 | `	*pSyntax = MbReSyntaxOf(pCtx->pVm);` |
|    114 | 2244 | `	if( pArg == 0 \|\| ph7_value_is_null(pArg) ){` |
|    110 | 2245 | `		return 0;` |
|      - | 2246 | `	}` |
|      6 | 2247 | `	zOpt = ph7_value_to_string(pArg,&nOpt);` |
|      6 | 2248 | `	if( MbReParseOpt(zOpt,nOpt,pOpt,pSyntax,&cBad) != 0 ){` |
|    ! 0 | 2249 | `		PH7_VmThrowException(pCtx,"ValueError","Option \"%c\" is not supported",(int)cBad);` |
|    ! 0 | 2250 | `		return -1;` |
|      - | 2251 | `	}` |
|      6 | 2252 | `	return 0;` |
|     58 | 2253 | `}` |
|      - | 2254 | `/* One compiled pattern is kept, because mb_ereg_search() walks a subject one` |
|      - | 2255 | ` * call at a time and would otherwise recompile per step. It is keyed on the` |
|      - | 2256 | ` * pattern bytes AND the options they were read under, so the same pattern` |
|      - | 2257 | ` * asked case-insensitively is a different entry. */` |
|      - | 2258 | `static struct {` |
|      - | 2259 | `	char *zKey;` |
|      - | 2260 | `	sxu32 nKey;` |
|      - | 2261 | `	pcre2_code *pCode;` |
|      - | 2262 | `	sxu32 nCapture;` |
|      - | 2263 | `} sMbReCache = { 0, 0, 0, 0 };` |
|      - | 2264 |  |
|    100 | 2265 | `static pcre2_code * MbReCompile(` |
|      - | 2266 | `	ph7_context *pCtx,` |
|      - | 2267 | `	const char *zPat,int nPat,` |
|      - | 2268 | `	sxu32 iOpt,` |
|      - | 2269 | `	const char *zFunc,` |
|      - | 2270 | `	sxu32 *pCapture)` |
|      2 | 2271 | `{` |
|    102 | 2272 | `	uint32_t compileOpts = 0;` |
|      - | 2273 | `	pcre2_code *pCode;` |
|      - | 2274 | `	PCRE2_SIZE erroffset;` |
|      - | 2275 | `	int errcode;` |
|      - | 2276 | `	sxu32 nCapture, nKey;` |
|      - | 2277 | `	char *zKey;` |
|    102 | 2278 | `	int bUtf8 = PH7_MbEncodingIsUtf8(pCtx->pVm->iMbReEnc);` |
|      - | 2279 |  |
|      - | 2280 | `	/* key = the pattern, then the options and the framing */` |
|    102 | 2281 | `	nKey = (sxu32)nPat + 3;` |
|    100 | 2282 | `	if( sMbReCache.zKey && sMbReCache.nKey == nKey` |
|     66 | 2283 | `		&& SyMemcmp(sMbReCache.zKey,zPat,(sxu32)nPat) == 0` |
|     30 | 2284 | `		&& sMbReCache.zKey[nPat] == (char)(iOpt & 0xFF)` |
|     23 | 2285 | `		&& sMbReCache.zKey[nPat+1] == (char)((iOpt >> 8) & 0xFF)` |
|     24 | 2286 | `		&& sMbReCache.zKey[nPat+2] == (char)bUtf8 ){` |
|     24 | 2287 | `		*pCapture = sMbReCache.nCapture;` |
|     24 | 2288 | `		return sMbReCache.pCode;` |
|      - | 2289 | `	}` |
|     80 | 2290 | `	if( bUtf8 ){` |
|      - | 2291 | `		/* UCP so that \w, \d and the POSIX classes cover what Oniguruma's do` |
|      - | 2292 | `		 * over UTF-8: mb_ereg('(\w+)','héllo') answers the whole word. */` |
|     80 | 2293 | `		compileOpts \|= PCRE2_UTF \| PCRE2_UCP;` |
|     39 | 2294 | `	}` |
|     80 | 2295 | `	if( iOpt & MBRE_OPT_IGNORECASE ){ compileOpts \|= PCRE2_CASELESS; }` |
|     80 | 2296 | `	if( iOpt & MBRE_OPT_EXTEND ){ compileOpts \|= PCRE2_EXTENDED; }` |
|     80 | 2297 | `	if( iOpt & MBRE_OPT_MULTILINE ){ compileOpts \|= PCRE2_DOTALL; }` |
|     80 | 2298 | `	if( (iOpt & MBRE_OPT_SINGLELINE) == 0 ){ compileOpts \|= PCRE2_MULTILINE; }` |
|     80 | 2299 | `	pCode = pcre2_compile((PCRE2_SPTR)zPat,(PCRE2_SIZE)nPat,compileOpts,` |
|      - | 2300 | `		&errcode,&erroffset,NULL);` |
|     80 | 2301 | `	if( pCode == 0 ){` |
|      - | 2302 | `		PCRE2_UCHAR errbuf[256];` |
|      5 | 2303 | `		pcre2_get_error_message(errcode,errbuf,sizeof(errbuf));` |
|      7 | 2304 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      2 | 2305 | `			"%s(): mbregex compile err: %s",zFunc,(const char *)errbuf);` |
|      5 | 2306 | `		return 0;` |
|      - | 2307 | `	}` |
|     76 | 2308 | `	nCapture = 0;` |
|     76 | 2309 | `	pcre2_pattern_info(pCode,PCRE2_INFO_CAPTURECOUNT,&nCapture);` |
|     76 | 2310 | `	zKey = (char *)malloc(nKey);` |
|     76 | 2311 | `	if( zKey == 0 ){` |
|      - | 2312 | `		/* No room to remember it; the caller still gets a usable pattern and the` |
|      - | 2313 | `		 * next call compiles again. */` |
|    ! 0 | 2314 | `		pcre2_code_free(pCode);` |
|    ! 0 | 2315 | `		return 0;` |
|      - | 2316 | `	}` |
|     76 | 2317 | `	SyMemcpy(zPat,zKey,(sxu32)nPat);` |
|     76 | 2318 | `	zKey[nPat]     = (char)(iOpt & 0xFF);` |
|     76 | 2319 | `	zKey[nPat + 1] = (char)((iOpt >> 8) & 0xFF);` |
|     76 | 2320 | `	zKey[nPat + 2] = (char)bUtf8;` |
|     76 | 2321 | `	if( sMbReCache.pCode ){` |
|     72 | 2322 | `		pcre2_code_free(sMbReCache.pCode);` |
|     72 | 2323 | `		free(sMbReCache.zKey);` |
|     35 | 2324 | `	}` |
|     76 | 2325 | `	sMbReCache.zKey = zKey;` |
|     76 | 2326 | `	sMbReCache.nKey = nKey;` |
|     76 | 2327 | `	sMbReCache.pCode = pCode;` |
|     76 | 2328 | `	sMbReCache.nCapture = nCapture;` |
|     76 | 2329 | `	*pCapture = nCapture;` |
|     76 | 2330 | `	return pCode;` |
|     52 | 2331 | `}` |
|      - | 2332 | ``/* An unmatched group is `false` here, not the empty string preg_match writes,`` |
|      - | 2333 | ` * and every group the pattern declares is present whether it participated or` |
|      - | 2334 | ` * not. Named groups are appended AFTER the numbered ones -- preg_match` |
|      - | 2335 | ` * interleaves them, mbstring does not. */` |
|     22 | 2336 | `static void MbRePopulate(` |
|      - | 2337 | `	ph7_context *pCtx,` |
|      - | 2338 | `	ph7_value *pArray,` |
|      - | 2339 | `	const char *zSub,` |
|      - | 2340 | `	const sxu32 *aOv,int nGroup,` |
|      - | 2341 | `	pcre2_code *pCode)` |
|      2 | 2342 | `{` |
|     24 | 2343 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|     24 | 2344 | `	uint32_t namecount = 0, nameentrysize = 0;` |
|     24 | 2345 | `	PCRE2_SPTR nametable = 0;` |
|      - | 2346 | `	int i;` |
|     64 | 2347 | `	for( i = 0 ; i < nGroup ; ++i ){` |
|     42 | 2348 | `		if( aOv[2*i] == SXU32_HIGH ){` |
|      5 | 2349 | `			ph7_value_bool(pVal,0);` |
|      3 | 2350 | `		}else{` |
|     38 | 2351 | `			ph7_value_string(pVal,&zSub[aOv[2*i]],(int)(aOv[2*i+1] - aOv[2*i]));` |
|      - | 2352 | `		}` |
|     42 | 2353 | `		ph7_array_add_intkey_elem(pArray,i,pVal);` |
|     42 | 2354 | `		ph7_value_reset_string_cursor(pVal);` |
|     22 | 2355 | `	}` |
|     24 | 2356 | `	pcre2_pattern_info(pCode,PCRE2_INFO_NAMECOUNT,&namecount);` |
|     24 | 2357 | `	if( namecount > 0 ){` |
|      - | 2358 | `		uint32_t k;` |
|      3 | 2359 | `		pcre2_pattern_info(pCode,PCRE2_INFO_NAMETABLE,&nametable);` |
|      3 | 2360 | `		pcre2_pattern_info(pCode,PCRE2_INFO_NAMEENTRYSIZE,&nameentrysize);` |
|      5 | 2361 | `		for( k = 0 ; k < namecount ; ++k ){` |
|      3 | 2362 | `			PCRE2_SPTR entry = nametable + k * nameentrysize;` |
|      3 | 2363 | `			int iNum = (entry[0] << 8) \| entry[1];` |
|      3 | 2364 | `			if( iNum >= nGroup ){` |
|    ! 0 | 2365 | `				continue;` |
|      - | 2366 | `			}` |
|      3 | 2367 | `			if( aOv[2*iNum] == SXU32_HIGH ){` |
|    ! 0 | 2368 | `				ph7_value_bool(pVal,0);` |
|    ! 0 | 2369 | `			}else{` |
|      4 | 2370 | `				ph7_value_string(pVal,&zSub[aOv[2*iNum]],` |
|      2 | 2371 | `					(int)(aOv[2*iNum+1] - aOv[2*iNum]));` |
|      - | 2372 | `			}` |
|      3 | 2373 | `			ph7_array_add_strkey_elem(pArray,(const char *)(entry + 2),pVal);` |
|      3 | 2374 | `			ph7_value_reset_string_cursor(pVal);` |
|      2 | 2375 | `		}` |
|      1 | 2376 | `	}` |
|     24 | 2377 | `	ph7_context_release_value(pCtx,pVal);` |
|     24 | 2378 | `}` |
|      - | 2379 | `/* Run one match and copy the offsets out as byte positions. Answers 1 on a` |
|      - | 2380 | ` * match, 0 on no match, -1 on a PCRE2 error. *pnGroup is the pattern's own` |
|      - | 2381 | ` * capture count plus one, so a trailing optional group that did not` |
|      - | 2382 | ` * participate still gets a slot -- php reports it as false. */` |
|    136 | 2383 | `static int MbReMatch(` |
|      - | 2384 | `	pcre2_code *pCode,sxu32 nCapture,` |
|      - | 2385 | `	const char *zSub,int nSub,sxu32 iStart,` |
|      - | 2386 | `	sxu32 *aOv,int *pnGroup)` |
|      2 | 2387 | `{` |
|      - | 2388 | `	pcre2_match_data *pData;` |
|      - | 2389 | `	PCRE2_SIZE *ov;` |
|      - | 2390 | `	int rc,i,nGroup;` |
|      - | 2391 |  |
|    138 | 2392 | `	pData = pcre2_match_data_create_from_pattern(pCode,NULL);` |
|    138 | 2393 | `	if( pData == 0 ){` |
|    ! 0 | 2394 | `		return -1;` |
|      - | 2395 | `	}` |
|    206 | 2396 | `	rc = pcre2_match(pCode,(PCRE2_SPTR)zSub,(PCRE2_SIZE)nSub,` |
|     68 | 2397 | `		(PCRE2_SIZE)iStart,0,pData,NULL);` |
|    138 | 2398 | `	if( rc < 0 ){` |
|     46 | 2399 | `		pcre2_match_data_free(pData);` |
|     46 | 2400 | `		return rc == PCRE2_ERROR_NOMATCH ? 0 : -1;` |
|      - | 2401 | `	}` |
|     94 | 2402 | `	ov = pcre2_get_ovector_pointer(pData);` |
|     94 | 2403 | `	nGroup = (int)nCapture + 1;` |
|    220 | 2404 | `	for( i = 0 ; i < nGroup ; ++i ){` |
|    128 | 2405 | `		if( i < rc && ov[2*i] != PCRE2_UNSET ){` |
|    124 | 2406 | `			aOv[2*i]     = (sxu32)ov[2*i];` |
|    124 | 2407 | `			aOv[2*i + 1] = (sxu32)ov[2*i + 1];` |
|     63 | 2408 | `		}else{` |
|      5 | 2409 | `			aOv[2*i] = aOv[2*i + 1] = SXU32_HIGH;` |
|      - | 2410 | `		}` |
|     65 | 2411 | `	}` |
|     94 | 2412 | `	*pnGroup = nGroup;` |
|     94 | 2413 | `	pcre2_match_data_free(pData);` |
|     94 | 2414 | `	return 1;` |
|     70 | 2415 | `}` |
|      - | 2416 | `/* mb_ereg / mb_eregi */` |
|     36 | 2417 | `static int MbEregCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCase)` |
|      2 | 2418 | `{` |
|      - | 2419 | `	const char *zPat,*zSub;` |
|     38 | 2420 | `	int nPat,nSub,nGroup = 0,rc;` |
|     38 | 2421 | `	sxu32 iOpt,nCapture = 0,*aOv;` |
|      - | 2422 | `	int iSyntax;` |
|      - | 2423 | `	pcre2_code *pCode;` |
|      - | 2424 |  |
|     38 | 2425 | `	zPat = ph7_value_to_string(apArg[0],&nPat);` |
|     38 | 2426 | `	if( nPat < 1 ){` |
|      4 | 2427 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2428 | `			"%s(): Argument #1 ($pattern) must not be empty",` |
|      1 | 2429 | `			bCase ? "mb_eregi" : "mb_ereg");` |
|      - | 2430 | `	}` |
|     36 | 2431 | `	zSub = ph7_value_to_string(apArg[1],&nSub);` |
|     36 | 2432 | `	if( MbReOptArg(pCtx,0,&iOpt,&iSyntax) != 0 ){` |
|    ! 0 | 2433 | `		return PH7_OK;` |
|      - | 2434 | `	}` |
|     36 | 2435 | `	if( bCase ){` |
|      3 | 2436 | `		iOpt \|= MBRE_OPT_IGNORECASE;` |
|      1 | 2437 | `	}` |
|     36 | 2438 | `	pCode = MbReCompile(pCtx,zPat,nPat,iOpt,bCase ? "mb_eregi" : "mb_ereg",&nCapture);` |
|     36 | 2439 | `	if( pCode == 0 ){` |
|      3 | 2440 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2441 | `		return PH7_OK;` |
|      - | 2442 | `	}` |
|     50 | 2443 | `	aOv = (sxu32 *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     32 | 2444 | `		(nCapture + 1) * 2 * sizeof(sxu32));` |
|     34 | 2445 | `	if( aOv == 0 ){` |
|    ! 0 | 2446 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2447 | `		return PH7_OK;` |
|      - | 2448 | `	}` |
|     34 | 2449 | `	rc = MbReMatch(pCode,nCapture,zSub,nSub,0,aOv,&nGroup);` |
|     34 | 2450 | `	if( nArg > 2 ){` |
|     15 | 2451 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|     15 | 2452 | `		if( rc > 0 ){` |
|     13 | 2453 | `			MbRePopulate(pCtx,pArray,zSub,aOv,nGroup,pCode);` |
|      6 | 2454 | `		}` |
|     15 | 2455 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pArray);` |
|     15 | 2456 | `		ph7_context_release_value(pCtx,pArray);` |
|      7 | 2457 | `	}` |
|     34 | 2458 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,aOv);` |
|     34 | 2459 | `	ph7_result_bool(pCtx,rc > 0);` |
|     34 | 2460 | `	return PH7_OK;` |
|     20 | 2461 | `}` |
|     34 | 2462 | `static int PH7_builtin_mb_ereg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2463 | `{` |
|     36 | 2464 | `	return MbEregCommon(pCtx,nArg,apArg,0);` |
|      2 | 2465 | `}` |
|      2 | 2466 | `static int PH7_builtin_mb_eregi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2467 | `{` |
|      3 | 2468 | `	return MbEregCommon(pCtx,nArg,apArg,1);` |
|      1 | 2469 | `}` |
|      - | 2470 | `/* mb_ereg_match: the pattern is anchored at the START of the subject, and only` |
|      - | 2471 | ` * there -- it does NOT have to reach the end. */` |
|      8 | 2472 | `static int PH7_builtin_mb_ereg_match(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2473 | `{` |
|      - | 2474 | `	const char *zPat,*zSub;` |
|      9 | 2475 | `	int nPat,nSub,iSyntax,nGroup = 0,rc;` |
|      9 | 2476 | `	sxu32 iOpt,nCapture = 0,*aOv;` |
|      - | 2477 | `	pcre2_code *pCode;` |
|      - | 2478 |  |
|      9 | 2479 | `	zPat = ph7_value_to_string(apArg[0],&nPat);` |
|      9 | 2480 | `	zSub = ph7_value_to_string(apArg[1],&nSub);` |
|      9 | 2481 | `	if( MbReOptArg(pCtx,nArg > 2 ? apArg[2] : 0,&iOpt,&iSyntax) != 0 ){` |
|    ! 0 | 2482 | `		return PH7_OK;` |
|      - | 2483 | `	}` |
|      9 | 2484 | `	pCode = MbReCompile(pCtx,zPat,nPat,iOpt,"mb_ereg_match",&nCapture);` |
|      9 | 2485 | `	if( pCode == 0 ){` |
|    ! 0 | 2486 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2487 | `		return PH7_OK;` |
|      - | 2488 | `	}` |
|     13 | 2489 | `	aOv = (sxu32 *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|      8 | 2490 | `		(nCapture + 1) * 2 * sizeof(sxu32));` |
|      9 | 2491 | `	if( aOv == 0 ){` |
|    ! 0 | 2492 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2493 | `		return PH7_OK;` |
|      - | 2494 | `	}` |
|      9 | 2495 | `	rc = MbReMatch(pCode,nCapture,zSub,nSub,0,aOv,&nGroup);` |
|      - | 2496 | `	/* Anchored: a match that did not begin at offset 0 is not one. */` |
|      9 | 2497 | `	ph7_result_bool(pCtx,rc > 0 && aOv[0] == 0);` |
|      9 | 2498 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,aOv);` |
|      9 | 2499 | `	return PH7_OK;` |
|      5 | 2500 | `}` |
|      - | 2501 | `/* Walk a subject one byte-character at a time -- used to step past a zero-width` |
|      - | 2502 | ` * match without splitting a UTF-8 sequence down the middle. */` |
|     14 | 2503 | `static sxu32 MbReStep(const char *z,int n,sxu32 i,int bUtf8)` |
|      1 | 2504 | `{` |
|     15 | 2505 | `	sxu32 k = 1;` |
|     15 | 2506 | `	if( bUtf8 && i < (sxu32)n ){` |
|     11 | 2507 | `		unsigned char c = (unsigned char)z[i];` |
|     11 | 2508 | `		if( c >= 0xF0 ){ k = 4; }` |
|     11 | 2509 | `		else if( c >= 0xE0 ){ k = 3; }` |
|     11 | 2510 | `		else if( c >= 0xC0 ){ k = 2; }` |
|      5 | 2511 | `	}` |
|     15 | 2512 | `	return i + k;` |
|      1 | 2513 | `}` |
|      - | 2514 | `/* mb_split. A zero-width match does not split -- an empty pattern answers the` |
|      - | 2515 | ` * whole subject as one piece. $limit caps the number of PIECES: the last one` |
|      - | 2516 | ` * holds everything that is left, and a limit below 1 is the whole subject. */` |
|     16 | 2517 | `static int PH7_builtin_mb_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2518 | `{` |
|      - | 2519 | `	const char *zPat,*zSub;` |
|     17 | 2520 | `	int nPat,nSub,iSyntax,nGroup = 0,nPiece = 0,iLimit = -1;` |
|     17 | 2521 | `	sxu32 iOpt,nCapture = 0,*aOv,iStart = 0,iFrom = 0;` |
|      - | 2522 | `	pcre2_code *pCode;` |
|      - | 2523 | `	ph7_value *pArray,*pVal;` |
|      - | 2524 | `	int bUtf8;` |
|      - | 2525 |  |
|     17 | 2526 | `	zPat = ph7_value_to_string(apArg[0],&nPat);` |
|     17 | 2527 | `	zSub = ph7_value_to_string(apArg[1],&nSub);` |
|     17 | 2528 | `	if( nArg > 2 ){` |
|      5 | 2529 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|      5 | 2530 | `		if( iLimit == 0 ){` |
|      - | 2531 | `			/* php reads a zero limit as one piece -- the whole subject. Only a` |
|      - | 2532 | `			 * NEGATIVE limit is "no limit". */` |
|      3 | 2533 | `			iLimit = 1;` |
|      1 | 2534 | `		}` |
|      2 | 2535 | `	}` |
|     17 | 2536 | `	if( MbReOptArg(pCtx,0,&iOpt,&iSyntax) != 0 ){` |
|    ! 0 | 2537 | `		return PH7_OK;` |
|      - | 2538 | `	}` |
|     17 | 2539 | `	pCode = MbReCompile(pCtx,zPat,nPat,iOpt,"mb_split",&nCapture);` |
|     17 | 2540 | `	if( pCode == 0 ){` |
|      3 | 2541 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2542 | `		return PH7_OK;` |
|      - | 2543 | `	}` |
|     22 | 2544 | `	aOv = (sxu32 *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     14 | 2545 | `		(nCapture + 1) * 2 * sizeof(sxu32));` |
|     15 | 2546 | `	if( aOv == 0 ){` |
|    ! 0 | 2547 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2548 | `		return PH7_OK;` |
|      - | 2549 | `	}` |
|     15 | 2550 | `	bUtf8 = PH7_MbEncodingIsUtf8(pCtx->pVm->iMbReEnc);` |
|     15 | 2551 | `	pArray = ph7_context_new_array(pCtx);` |
|     15 | 2552 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     37 | 2553 | `	while( iStart <= (sxu32)nSub ){` |
|     37 | 2554 | `		if( iLimit > 0 && nPiece + 1 >= iLimit ){` |
|      5 | 2555 | `			break;` |
|      - | 2556 | `		}` |
|     33 | 2557 | `		if( MbReMatch(pCode,nCapture,zSub,nSub,iStart,aOv,&nGroup) <= 0 ){` |
|      9 | 2558 | `			break;` |
|      - | 2559 | `		}` |
|     25 | 2560 | `		if( aOv[1] == aOv[0] ){` |
|      - | 2561 | `			/* Zero-width: no split here, step over one character and retry. */` |
|      9 | 2562 | `			iStart = MbReStep(zSub,nSub,aOv[0],bUtf8);` |
|      9 | 2563 | `			if( iStart > (sxu32)nSub ){` |
|      3 | 2564 | `				break;` |
|      - | 2565 | `			}` |
|      7 | 2566 | `			continue;` |
|      - | 2567 | `		}` |
|     17 | 2568 | `		ph7_value_string(pVal,&zSub[iFrom],(int)(aOv[0] - iFrom));` |
|     17 | 2569 | `		ph7_array_add_elem(pArray,0,pVal);` |
|     17 | 2570 | `		ph7_value_reset_string_cursor(pVal);` |
|     17 | 2571 | `		nPiece++;` |
|     17 | 2572 | `		iFrom = iStart = aOv[1];` |
|      1 | 2573 | `	}` |
|     15 | 2574 | `	ph7_value_string(pVal,&zSub[iFrom],nSub - (int)iFrom);` |
|     15 | 2575 | `	ph7_array_add_elem(pArray,0,pVal);` |
|     15 | 2576 | `	ph7_context_release_value(pCtx,pVal);` |
|     15 | 2577 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,aOv);` |
|     15 | 2578 | `	ph7_result_value(pCtx,pArray);` |
|     15 | 2579 | `	return PH7_OK;` |
|      9 | 2580 | `}` |
|      - | 2581 | ``/* The replacement string's own grammar: `\` followed by a digit names a group,`` |
|      - | 2582 | ` * and a group the pattern does not declare -- or any other byte after the` |
|      - | 2583 | ` * backslash -- is copied through with the backslash still attached. There is no` |
|      - | 2584 | `` * `$1` here and no way to escape a `\1`. */`` |
|     22 | 2585 | `static void MbReExpand(` |
|      - | 2586 | `	SyBlob *pOut,` |
|      - | 2587 | `	const char *zRepl,int nRepl,` |
|      - | 2588 | `	const char *zSub,const sxu32 *aOv,int nGroup)` |
|      1 | 2589 | `{` |
|      - | 2590 | `	int i;` |
|     75 | 2591 | `	for( i = 0 ; i < nRepl ; ++i ){` |
|     52 | 2592 | `		if( zRepl[i] == '\\' && i + 1 < nRepl` |
|     15 | 2593 | `			&& zRepl[i+1] >= '0' && zRepl[i+1] <= '9' ){` |
|      9 | 2594 | `			int iNum = zRepl[i+1] - '0';` |
|      9 | 2595 | `			if( iNum < nGroup ){` |
|      7 | 2596 | `				if( aOv[2*iNum] != SXU32_HIGH ){` |
|      7 | 2597 | `					SyBlobAppend(pOut,&zSub[aOv[2*iNum]],aOv[2*iNum+1] - aOv[2*iNum]);` |
|      3 | 2598 | `				}` |
|      7 | 2599 | `				i++;` |
|      7 | 2600 | `				continue;` |
|      - | 2601 | `			}` |
|      1 | 2602 | `		}` |
|     47 | 2603 | `		SyBlobAppend(pOut,&zRepl[i],1);` |
|     24 | 2604 | `	}` |
|     23 | 2605 | `}` |
|      - | 2606 | `/* mb_ereg_replace / mb_eregi_replace / mb_ereg_replace_callback */` |
|     24 | 2607 | `static int MbEregReplaceCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|      - | 2608 | `	int bCase,int bCallback)` |
|      1 | 2609 | `{` |
|     25 | 2610 | `	const char *zPat,*zSub,*zRepl = 0;` |
|     25 | 2611 | `	const char *zFunc = bCallback ? "mb_ereg_replace_callback"` |
|     22 | 2612 | `		: (bCase ? "mb_eregi_replace" : "mb_ereg_replace");` |
|     25 | 2613 | `	int nPat,nSub,nRepl = 0,iSyntax,nGroup = 0;` |
|     25 | 2614 | `	sxu32 iOpt,nCapture = 0,*aOv,iStart = 0,iFrom = 0;` |
|      - | 2615 | `	pcre2_code *pCode;` |
|      - | 2616 | `	SyBlob sOut;` |
|      - | 2617 | `	int bUtf8;` |
|     25 | 2618 | `	sxi32 rcCb = SXRET_OK;` |
|      - | 2619 |  |
|     25 | 2620 | `	zPat = ph7_value_to_string(apArg[0],&nPat);` |
|     25 | 2621 | `	if( !bCallback ){` |
|     21 | 2622 | `		zRepl = ph7_value_to_string(apArg[1],&nRepl);` |
|     10 | 2623 | `	}` |
|     25 | 2624 | `	zSub = ph7_value_to_string(apArg[2],&nSub);` |
|     25 | 2625 | `	if( MbReOptArg(pCtx,nArg > 3 ? apArg[3] : 0,&iOpt,&iSyntax) != 0 ){` |
|    ! 0 | 2626 | `		return PH7_OK;` |
|      - | 2627 | `	}` |
|     25 | 2628 | `	if( bCase ){` |
|      3 | 2629 | `		iOpt \|= MBRE_OPT_IGNORECASE;` |
|      1 | 2630 | `	}` |
|     25 | 2631 | `	pCode = MbReCompile(pCtx,zPat,nPat,iOpt,zFunc,&nCapture);` |
|     25 | 2632 | `	if( pCode == 0 ){` |
|    ! 0 | 2633 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2634 | `		return PH7_OK;` |
|      - | 2635 | `	}` |
|     37 | 2636 | `	aOv = (sxu32 *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     24 | 2637 | `		(nCapture + 1) * 2 * sizeof(sxu32));` |
|     25 | 2638 | `	if( aOv == 0 ){` |
|    ! 0 | 2639 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2640 | `		return PH7_OK;` |
|      - | 2641 | `	}` |
|     25 | 2642 | `	bUtf8 = PH7_MbEncodingIsUtf8(pCtx->pVm->iMbReEnc);` |
|     25 | 2643 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     49 | 2644 | `	while( iStart <= (sxu32)nSub ){` |
|     49 | 2645 | `		if( MbReMatch(pCode,nCapture,zSub,nSub,iStart,aOv,&nGroup) <= 0 ){` |
|     23 | 2646 | `			break;` |
|      - | 2647 | `		}` |
|     27 | 2648 | `		if( aOv[0] > iFrom ){` |
|     21 | 2649 | `			SyBlobAppend(&sOut,&zSub[iFrom],aOv[0] - iFrom);` |
|     10 | 2650 | `		}` |
|     27 | 2651 | `		if( bCallback ){` |
|      5 | 2652 | `			ph7_value *pMatchArr = ph7_context_new_array(pCtx);` |
|      - | 2653 | `			ph7_value *apCbArg[1];` |
|      - | 2654 | `			ph7_value sResult;` |
|      - | 2655 | `			const char *zCb;` |
|      - | 2656 | `			int nCb;` |
|      5 | 2657 | `			MbRePopulate(pCtx,pMatchArr,zSub,aOv,nGroup,pCode);` |
|      5 | 2658 | `			PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      5 | 2659 | `			apCbArg[0] = pMatchArr;` |
|      5 | 2660 | `			rcCb = PH7_VmCallCallbackByValue(pCtx->pVm,apArg[1],1,apCbArg,&sResult,0);` |
|      5 | 2661 | `			if( PH7_CALLBACK_UNWOUND(rcCb) ){` |
|    ! 0 | 2662 | `				PH7_MemObjRelease(&sResult);` |
|    ! 0 | 2663 | `				ph7_context_release_value(pCtx,pMatchArr);` |
|    ! 0 | 2664 | `				SyBlobRelease(&sOut);` |
|    ! 0 | 2665 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,aOv);` |
|    ! 0 | 2666 | `				return rcCb;` |
|      - | 2667 | `			}` |
|      5 | 2668 | `			zCb = ph7_value_to_string(&sResult,&nCb);` |
|      5 | 2669 | `			SyBlobAppend(&sOut,zCb,(sxu32)nCb);` |
|      5 | 2670 | `			PH7_MemObjRelease(&sResult);` |
|      5 | 2671 | `			ph7_context_release_value(pCtx,pMatchArr);` |
|      3 | 2672 | `		}else{` |
|     23 | 2673 | `			MbReExpand(&sOut,zRepl,nRepl,zSub,aOv,nGroup);` |
|      - | 2674 | `		}` |
|     27 | 2675 | `		iFrom = aOv[1];` |
|     27 | 2676 | `		if( aOv[1] == aOv[0] ){` |
|      - | 2677 | `			/* A zero-width match: carry the character it sat on across and step` |
|      - | 2678 | `			 * past it, or the scan never moves. */` |
|      7 | 2679 | `			sxu32 iNext = MbReStep(zSub,nSub,aOv[0],bUtf8);` |
|      7 | 2680 | `			if( iNext > (sxu32)nSub ){` |
|      3 | 2681 | `				iStart = iNext;` |
|      3 | 2682 | `				break;` |
|      - | 2683 | `			}` |
|      5 | 2684 | `			SyBlobAppend(&sOut,&zSub[aOv[0]],iNext - aOv[0]);` |
|      5 | 2685 | `			iFrom = iStart = iNext;` |
|      3 | 2686 | `		}else{` |
|     21 | 2687 | `			iStart = aOv[1];` |
|      - | 2688 | `		}` |
|      1 | 2689 | `	}` |
|     25 | 2690 | `	if( iFrom < (sxu32)nSub ){` |
|     23 | 2691 | `		SyBlobAppend(&sOut,&zSub[iFrom],(sxu32)nSub - iFrom);` |
|     11 | 2692 | `	}` |
|     25 | 2693 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     25 | 2694 | `	SyBlobRelease(&sOut);` |
|     25 | 2695 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,aOv);` |
|     25 | 2696 | `	return PH7_OK;` |
|     13 | 2697 | `}` |
|     18 | 2698 | `static int PH7_builtin_mb_ereg_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2699 | `{` |
|     19 | 2700 | `	return MbEregReplaceCommon(pCtx,nArg,apArg,0,0);` |
|      1 | 2701 | `}` |
|      2 | 2702 | `static int PH7_builtin_mb_eregi_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2703 | `{` |
|      3 | 2704 | `	return MbEregReplaceCommon(pCtx,nArg,apArg,1,0);` |
|      1 | 2705 | `}` |
|      4 | 2706 | `static int PH7_builtin_mb_ereg_replace_callback(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2707 | `{` |
|      5 | 2708 | `	return MbEregReplaceCommon(pCtx,nArg,apArg,0,1);` |
|      1 | 2709 | `}` |
|      - | 2710 | `/* ----- the stateful search ----------------------------------------------` |
|      - | 2711 | ` * mb_ereg_search_init() parks a subject and (optionally) a pattern; each of` |
|      - | 2712 | ` * mb_ereg_search(), _pos() and _regs() runs ONE match from the cursor and` |
|      - | 2713 | ` * leaves it just past what matched, while _getregs(), _getpos() and _setpos()` |
|      - | 2714 | ` * only read and write the state. A call with no subject parked is an Error,` |
|      - | 2715 | ` * and so is one with no pattern -- php checks the pattern first.` |
|      - | 2716 | ` * -------------------------------------------------------------------- */` |
|     22 | 2717 | `static void MbReSearchDropRegs(ph7_vm *pVm)` |
|      1 | 2718 | `{` |
|     23 | 2719 | `	if( pVm->aMbReOv ){` |
|     11 | 2720 | `		SyMemBackendFree(&pVm->sAllocator,pVm->aMbReOv);` |
|     11 | 2721 | `		pVm->aMbReOv = 0;` |
|      5 | 2722 | `	}` |
|     23 | 2723 | `	pVm->nMbReOv = 0;` |
|     23 | 2724 | `}` |
|     10 | 2725 | `static int MbReSearchSetPattern(ph7_context *pCtx,const char *zPat,int nPat,` |
|      - | 2726 | `	sxu32 iOpt,int iSyntax)` |
|      1 | 2727 | `{` |
|     11 | 2728 | `	ph7_vm *pVm = pCtx->pVm;` |
|     11 | 2729 | `	char *zCopy = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nPat + 1);` |
|     11 | 2730 | `	if( zCopy == 0 ){` |
|    ! 0 | 2731 | `		return -1;` |
|      - | 2732 | `	}` |
|     11 | 2733 | `	SyMemcpy(zPat,zCopy,(sxu32)nPat);` |
|     11 | 2734 | `	zCopy[nPat] = 0;` |
|     11 | 2735 | `	if( pVm->zMbRePat ){` |
|      9 | 2736 | `		SyMemBackendFree(&pVm->sAllocator,pVm->zMbRePat);` |
|      4 | 2737 | `	}` |
|     11 | 2738 | `	pVm->zMbRePat = zCopy;` |
|     11 | 2739 | `	pVm->nMbRePat = (sxu32)nPat;` |
|     11 | 2740 | `	pVm->iMbReOptCur = iOpt;` |
|     11 | 2741 | `	pVm->iMbReSynCur = (sxu8)iSyntax;` |
|     11 | 2742 | `	return 0;` |
|      6 | 2743 | `}` |
|      6 | 2744 | `static int PH7_builtin_mb_ereg_search_init(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2745 | `{` |
|      7 | 2746 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 2747 | `	const char *zStr;` |
|      - | 2748 | `	int nStr,iSyntax;` |
|      - | 2749 | `	sxu32 iOpt;` |
|      - | 2750 | `	char *zCopy;` |
|      - | 2751 |  |
|      7 | 2752 | `	zStr = ph7_value_to_string(apArg[0],&nStr);` |
|      7 | 2753 | `	if( MbReOptArg(pCtx,nArg > 2 ? apArg[2] : 0,&iOpt,&iSyntax) != 0 ){` |
|    ! 0 | 2754 | `		return PH7_OK;` |
|      - | 2755 | `	}` |
|      7 | 2756 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 2757 | `		const char *zPat;` |
|      - | 2758 | `		int nPat;` |
|      7 | 2759 | `		zPat = ph7_value_to_string(apArg[1],&nPat);` |
|      7 | 2760 | `		if( MbReSearchSetPattern(pCtx,zPat,nPat,iOpt,iSyntax) != 0 ){` |
|    ! 0 | 2761 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2762 | `			return PH7_OK;` |
|      - | 2763 | `		}` |
|      3 | 2764 | `	}` |
|      7 | 2765 | `	zCopy = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nStr + 1);` |
|      7 | 2766 | `	if( zCopy == 0 ){` |
|    ! 0 | 2767 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2768 | `		return PH7_OK;` |
|      - | 2769 | `	}` |
|      7 | 2770 | `	SyMemcpy(zStr,zCopy,(sxu32)nStr);` |
|      7 | 2771 | `	zCopy[nStr] = 0;` |
|      7 | 2772 | `	if( pVm->zMbReStr ){` |
|      5 | 2773 | `		SyMemBackendFree(&pVm->sAllocator,pVm->zMbReStr);` |
|      2 | 2774 | `	}` |
|      7 | 2775 | `	pVm->zMbReStr = zCopy;` |
|      7 | 2776 | `	pVm->nMbReStr = (sxu32)nStr;` |
|      7 | 2777 | `	pVm->iMbRePos = 0;` |
|      7 | 2778 | `	MbReSearchDropRegs(pVm);` |
|      7 | 2779 | `	ph7_result_bool(pCtx,1);` |
|      7 | 2780 | `	return PH7_OK;` |
|      4 | 2781 | `}` |
|      - | 2782 | `/* The one step behind mb_ereg_search(), _pos() and _regs(). Answers 1 on a` |
|      - | 2783 | ` * match (the state now holds its offsets), 0 on no match, and -1 when it` |
|      - | 2784 | ` * already threw. */` |
|     24 | 2785 | `static int MbReSearchStep(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|      - | 2786 | `	const char *zFunc,pcre2_code **ppCode)` |
|      1 | 2787 | `{` |
|     25 | 2788 | `	ph7_vm *pVm = pCtx->pVm;` |
|     25 | 2789 | `	int iSyntax,nGroup = 0,rc;` |
|     25 | 2790 | `	sxu32 iOpt,nCapture = 0,*aOv;` |
|      - | 2791 | `	pcre2_code *pCode;` |
|      - | 2792 |  |
|     25 | 2793 | `	if( MbReOptArg(pCtx,nArg > 1 ? apArg[1] : 0,&iOpt,&iSyntax) != 0 ){` |
|    ! 0 | 2794 | `		return -1;` |
|      - | 2795 | `	}` |
|     25 | 2796 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|      - | 2797 | `		const char *zPat;` |
|      - | 2798 | `		int nPat;` |
|      5 | 2799 | `		zPat = ph7_value_to_string(apArg[0],&nPat);` |
|      5 | 2800 | `		if( MbReSearchSetPattern(pCtx,zPat,nPat,iOpt,iSyntax) != 0 ){` |
|    ! 0 | 2801 | `			return -1;` |
|      - | 2802 | `		}` |
|      2 | 2803 | `	}` |
|     25 | 2804 | `	if( pVm->zMbRePat == 0 ){` |
|      7 | 2805 | `		PH7_VmThrowException(pCtx,"Error","No pattern was provided");` |
|      7 | 2806 | `		return -1;` |
|      - | 2807 | `	}` |
|     19 | 2808 | `	if( pVm->zMbReStr == 0 ){` |
|      3 | 2809 | `		PH7_VmThrowException(pCtx,"Error","No string was provided");` |
|      3 | 2810 | `		return -1;` |
|      - | 2811 | `	}` |
|     25 | 2812 | `	pCode = MbReCompile(pCtx,pVm->zMbRePat,(int)pVm->nMbRePat,pVm->iMbReOptCur,` |
|      8 | 2813 | `		zFunc,&nCapture);` |
|     17 | 2814 | `	if( pCode == 0 ){` |
|    ! 0 | 2815 | `		return 0;` |
|      - | 2816 | `	}` |
|     17 | 2817 | `	MbReSearchDropRegs(pVm);` |
|     17 | 2818 | `	if( pVm->iMbRePos > pVm->nMbReStr ){` |
|    ! 0 | 2819 | `		return 0;` |
|      - | 2820 | `	}` |
|     25 | 2821 | `	aOv = (sxu32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|     16 | 2822 | `		(nCapture + 1) * 2 * sizeof(sxu32));` |
|     17 | 2823 | `	if( aOv == 0 ){` |
|    ! 0 | 2824 | `		return 0;` |
|      - | 2825 | `	}` |
|     25 | 2826 | `	rc = MbReMatch(pCode,nCapture,pVm->zMbReStr,(int)pVm->nMbReStr,pVm->iMbRePos,` |
|      8 | 2827 | `		aOv,&nGroup);` |
|     17 | 2828 | `	if( rc <= 0 ){` |
|      5 | 2829 | `		SyMemBackendFree(&pVm->sAllocator,aOv);` |
|      5 | 2830 | `		return 0;` |
|      - | 2831 | `	}` |
|     13 | 2832 | `	pVm->aMbReOv = aOv;` |
|     13 | 2833 | `	pVm->nMbReOv = nGroup;` |
|     13 | 2834 | `	pVm->iMbRePos = aOv[1];` |
|     13 | 2835 | `	*ppCode = pCode;` |
|     13 | 2836 | `	return 1;` |
|     13 | 2837 | `}` |
|      8 | 2838 | `static int PH7_builtin_mb_ereg_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2839 | `{` |
|      9 | 2840 | `	pcre2_code *pCode = 0;` |
|      9 | 2841 | `	int rc = MbReSearchStep(pCtx,nArg,apArg,"mb_ereg_search",&pCode);` |
|      9 | 2842 | `	if( rc < 0 ){` |
|      5 | 2843 | `		return PH7_OK;` |
|      - | 2844 | `	}` |
|      5 | 2845 | `	ph7_result_bool(pCtx,rc > 0);` |
|      5 | 2846 | `	return PH7_OK;` |
|      5 | 2847 | `}` |
|      8 | 2848 | `static int PH7_builtin_mb_ereg_search_pos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2849 | `{` |
|      9 | 2850 | `	pcre2_code *pCode = 0;` |
|      - | 2851 | `	ph7_value *pArray,*pVal;` |
|      9 | 2852 | `	int rc = MbReSearchStep(pCtx,nArg,apArg,"mb_ereg_search_pos",&pCode);` |
|      9 | 2853 | `	if( rc < 0 ){` |
|      3 | 2854 | `		return PH7_OK;` |
|      - | 2855 | `	}` |
|      7 | 2856 | `	if( rc == 0 ){` |
|    ! 0 | 2857 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2858 | `		return PH7_OK;` |
|      - | 2859 | `	}` |
|      7 | 2860 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 2861 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 2862 | `	ph7_value_int(pVal,(int)pCtx->pVm->aMbReOv[0]);` |
|      7 | 2863 | `	ph7_array_add_intkey_elem(pArray,0,pVal);` |
|      7 | 2864 | `	ph7_value_int(pVal,(int)(pCtx->pVm->aMbReOv[1] - pCtx->pVm->aMbReOv[0]));` |
|      7 | 2865 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      7 | 2866 | `	ph7_context_release_value(pCtx,pVal);` |
|      7 | 2867 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 2868 | `	return PH7_OK;` |
|      5 | 2869 | `}` |
|      8 | 2870 | `static int PH7_builtin_mb_ereg_search_regs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2871 | `{` |
|      9 | 2872 | `	pcre2_code *pCode = 0;` |
|      - | 2873 | `	ph7_value *pArray;` |
|      9 | 2874 | `	int rc = MbReSearchStep(pCtx,nArg,apArg,"mb_ereg_search_regs",&pCode);` |
|      9 | 2875 | `	if( rc < 0 ){` |
|      3 | 2876 | `		return PH7_OK;` |
|      - | 2877 | `	}` |
|      7 | 2878 | `	if( rc == 0 ){` |
|      3 | 2879 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2880 | `		return PH7_OK;` |
|      - | 2881 | `	}` |
|      5 | 2882 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 2883 | `	MbRePopulate(pCtx,pArray,pCtx->pVm->zMbReStr,pCtx->pVm->aMbReOv,` |
|      4 | 2884 | `		pCtx->pVm->nMbReOv,pCode);` |
|      5 | 2885 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 2886 | `	return PH7_OK;` |
|      5 | 2887 | `}` |
|      6 | 2888 | `static int PH7_builtin_mb_ereg_search_getregs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2889 | `{` |
|      7 | 2890 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 2891 | `	ph7_value *pArray;` |
|      - | 2892 | `	pcre2_code *pCode;` |
|      7 | 2893 | `	sxu32 nCapture = 0;` |
|      3 | 2894 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      - | 2895 |  |
|      7 | 2896 | `	if( pVm->nMbReOv < 1 \|\| pVm->zMbReStr == 0 \|\| pVm->zMbRePat == 0 ){` |
|      5 | 2897 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2898 | `		return PH7_OK;` |
|      - | 2899 | `	}` |
|      3 | 2900 | `	pCode = MbReCompile(pCtx,pVm->zMbRePat,(int)pVm->nMbRePat,pVm->iMbReOptCur,` |
|      - | 2901 | `		"mb_ereg_search_getregs",&nCapture);` |
|      3 | 2902 | `	if( pCode == 0 ){` |
|    ! 0 | 2903 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2904 | `		return PH7_OK;` |
|      - | 2905 | `	}` |
|      3 | 2906 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 2907 | `	MbRePopulate(pCtx,pArray,pVm->zMbReStr,pVm->aMbReOv,pVm->nMbReOv,pCode);` |
|      3 | 2908 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 2909 | `	return PH7_OK;` |
|      4 | 2910 | `}` |
|     12 | 2911 | `static int PH7_builtin_mb_ereg_search_getpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2912 | `{` |
|      6 | 2913 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     13 | 2914 | `	ph7_result_int(pCtx,(int)pCtx->pVm->iMbRePos);` |
|     13 | 2915 | `	return PH7_OK;` |
|      1 | 2916 | `}` |
|      8 | 2917 | `static int PH7_builtin_mb_ereg_search_setpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2918 | `{` |
|      9 | 2919 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 2920 | `	sxi64 iOff;` |
|      4 | 2921 | `	SXUNUSED(nArg);` |
|      - | 2922 |  |
|      9 | 2923 | `	iOff = ph7_value_to_int64(apArg[0]);` |
|      - | 2924 | `	/* php measures the offset against the subject that is parked -- and with` |
|      - | 2925 | `	 * NONE parked there is nothing to measure against, so any non-negative` |
|      - | 2926 | `	 * offset is taken and the next mb_ereg_search_init() resets it anyway. */` |
|      9 | 2927 | `	if( iOff < 0 \|\| (pVm->zMbReStr != 0 && iOff > (sxi64)pVm->nMbReStr) ){` |
|      3 | 2928 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2929 | `			"mb_ereg_search_setpos(): Argument #1 ($offset) is out of range");` |
|      - | 2930 | `	}` |
|      7 | 2931 | `	pVm->iMbRePos = (sxu32)iOff;` |
|      7 | 2932 | `	ph7_result_bool(pCtx,1);` |
|      7 | 2933 | `	return PH7_OK;` |
|      5 | 2934 | `}` |
|      6 | 2935 | `static int PH7_builtin_mb_regex_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2936 | `{` |
|      8 | 2937 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 2938 | `	const char *zEnc;` |
|      - | 2939 | `	int nEnc,iName;` |
|      - | 2940 |  |
|      8 | 2941 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|      3 | 2942 | `		zEnc = PH7_MbEncodingCanonical(pVm->iMbReEnc);` |
|      3 | 2943 | `		ph7_result_string(pCtx,zEnc,-1);` |
|      3 | 2944 | `		return PH7_OK;` |
|      - | 2945 | `	}` |
|      6 | 2946 | `	zEnc = ph7_value_to_string(apArg[0],&nEnc);` |
|      6 | 2947 | `	iName = PH7_MbEncodingLookup(zEnc,nEnc);` |
|      6 | 2948 | `	if( iName < 0 ){` |
|      4 | 2949 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2950 | `			"mb_regex_encoding(): Argument #1 ($encoding) must be a valid encoding, "` |
|      1 | 2951 | `			"\"%.*s\" given",nEnc,zEnc);` |
|      - | 2952 | `	}` |
|      3 | 2953 | `	pVm->iMbReEnc = iName;` |
|      3 | 2954 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2955 | `	return PH7_OK;` |
|      5 | 2956 | `}` |
|     74 | 2957 | `static int PH7_builtin_mb_regex_set_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2958 | `{` |
|     76 | 2959 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 2960 | `	char zBuf[16];` |
|      - | 2961 | `	int nBuf;` |
|      - | 2962 | `	const char *zOpt;` |
|      - | 2963 | `	int nOpt,iSyntax;` |
|     76 | 2964 | `	sxu32 iOpt = 0;` |
|     76 | 2965 | `	unsigned char cBad = 0;` |
|      - | 2966 |  |
|      - | 2967 | `	/* The answer is always the option string as it stood BEFORE this call. */` |
|     76 | 2968 | `	nBuf = MbReOptString(MbReOptOf(pVm),MbReSyntaxOf(pVm),zBuf);` |
|     76 | 2969 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|     26 | 2970 | `		ph7_result_string(pCtx,zBuf,nBuf);` |
|     26 | 2971 | `		return PH7_OK;` |
|      - | 2972 | `	}` |
|     51 | 2973 | `	iSyntax = MbReSyntaxOf(pVm);` |
|     51 | 2974 | `	zOpt = ph7_value_to_string(apArg[0],&nOpt);` |
|     51 | 2975 | `	if( MbReParseOpt(zOpt,nOpt,&iOpt,&iSyntax,&cBad) != 0 ){` |
|      4 | 2976 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 2977 | `			"Option \"%c\" is not supported",(int)cBad);` |
|      - | 2978 | `	}` |
|     49 | 2979 | `	pVm->iMbReOpt = iOpt \| MBRE_OPT_SET;` |
|     49 | 2980 | `	pVm->iMbReSyntax = (sxu8)iSyntax;` |
|     49 | 2981 | `	ph7_result_string(pCtx,zBuf,nBuf);` |
|     49 | 2982 | `	return PH7_OK;` |
|     39 | 2983 | `}` |
|      - | 2984 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2985 |  |
|      - | 2986 | `static const ph7_builtin_func aPcreFunc[] = {` |
|      - | 2987 | `	{ "preg_match",              PH7_builtin_preg_match },` |
|      - | 2988 | `	{ "preg_match_all",          PH7_builtin_preg_match_all },` |
|      - | 2989 | `	{ "preg_replace",            PH7_builtin_preg_replace },` |
|      - | 2990 | `	{ "preg_filter",             PH7_builtin_preg_filter },` |
|      - | 2991 | `	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },` |
|      - | 2992 | `	{ "preg_replace_callback_array", PH7_builtin_preg_replace_callback_array },` |
|      - | 2993 | `	{ "preg_grep",               PH7_builtin_preg_grep },` |
|      - | 2994 | `	{ "preg_split",              PH7_builtin_preg_split },` |
|      - | 2995 | `	{ "preg_quote",              PH7_builtin_preg_quote },` |
|      - | 2996 | `	{ "preg_last_error",         PH7_builtin_preg_last_error },` |
|      - | 2997 | `	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },` |
|      - | 2998 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2999 | `	{ "mb_ereg",                 PH7_builtin_mb_ereg },` |
|      - | 3000 | `	{ "mb_eregi",                PH7_builtin_mb_eregi },` |
|      - | 3001 | `	{ "mb_ereg_match",           PH7_builtin_mb_ereg_match },` |
|      - | 3002 | `	{ "mb_ereg_replace",         PH7_builtin_mb_ereg_replace },` |
|      - | 3003 | `	{ "mb_eregi_replace",        PH7_builtin_mb_eregi_replace },` |
|      - | 3004 | `	{ "mb_ereg_replace_callback",PH7_builtin_mb_ereg_replace_callback },` |
|      - | 3005 | `	{ "mb_split",                PH7_builtin_mb_split },` |
|      - | 3006 | `	{ "mb_ereg_search_init",     PH7_builtin_mb_ereg_search_init },` |
|      - | 3007 | `	{ "mb_ereg_search",          PH7_builtin_mb_ereg_search },` |
|      - | 3008 | `	{ "mb_ereg_search_pos",      PH7_builtin_mb_ereg_search_pos },` |
|      - | 3009 | `	{ "mb_ereg_search_regs",     PH7_builtin_mb_ereg_search_regs },` |
|      - | 3010 | `	{ "mb_ereg_search_getregs",  PH7_builtin_mb_ereg_search_getregs },` |
|      - | 3011 | `	{ "mb_ereg_search_getpos",   PH7_builtin_mb_ereg_search_getpos },` |
|      - | 3012 | `	{ "mb_ereg_search_setpos",   PH7_builtin_mb_ereg_search_setpos },` |
|      - | 3013 | `	{ "mb_regex_encoding",       PH7_builtin_mb_regex_encoding },` |
|      - | 3014 | `	{ "mb_regex_set_options",    PH7_builtin_mb_regex_set_options },` |
|      - | 3015 | `#endif` |
|      - | 3016 | `};` |
|      - | 3017 |  |
|   8445 | 3018 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)` |
|      5 | 3019 | `{` |
|      - | 3020 | `	sxu32 n;` |
| 236465 | 3021 | `	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){` |
| 228020 | 3022 | `		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);` |
| 113864 | 3023 | `	}` |
|   8450 | 3024 | `}` |
|      - | 3025 |  |
|      - | 3026 | `/* ===== Constant registration ===== */` |
|      - | 3027 | `#define PCRE_CONST_INT(name, val) \` |
|      - | 3028 | `	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \` |
|      - | 3029 | `		SXUNUSED(pUnused); ph7_value_int(pVal, val); \` |
|      - | 3030 | `	}` |
|      - | 3031 |  |
|    203 | 3032 | `PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)` |
|    199 | 3033 | `PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)` |
|    213 | 3034 | `PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)` |
|    129 | 3035 | `PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)` |
|     87 | 3036 | `PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)` |
|     85 | 3037 | `PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)` |
|     85 | 3038 | `PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)` |
|     85 | 3039 | `PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)` |
|     85 | 3040 | `PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)` |
|     85 | 3041 | `PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)` |
|     85 | 3042 | `PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)` |
|     91 | 3043 | `PCRE_CONST_INT(PREG_GREP_INVERT,         PHP_PREG_GREP_INVERT)` |
|     85 | 3044 | `PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)` |
|     85 | 3045 | `PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)` |
|     85 | 3046 | `PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)` |
|      - | 3047 |  |
|   6985 | 3048 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)` |
|      5 | 3049 | `{` |
|   6990 | 3050 | `	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);` |
|   6990 | 3051 | `	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);` |
|   6990 | 3052 | `	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);` |
|   6990 | 3053 | `	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);` |
|   6990 | 3054 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);` |
|   6990 | 3055 | `	ph7_create_constant(&(*pVm), "PREG_GREP_INVERT",          PcreConst_PREG_GREP_INVERT, 0);` |
|   6990 | 3056 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);` |
|   6990 | 3057 | `	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);` |
|   6990 | 3058 | `	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);` |
|   6990 | 3059 | `	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);` |
|   6990 | 3060 | `	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);` |
|   6990 | 3061 | `	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);` |
|   6990 | 3062 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);` |
|   6990 | 3063 | `	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);` |
|   6990 | 3064 | `	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);` |
|   6990 | 3065 | `}` |
|      - | 3066 |  |
|      - | 3067 | `/*` |
|      - | 3068 | ` * What the PCRE2 this build is LINKED AGAINST says about itself. php publishes the` |
|      - | 3069 | ` * same four as constants and Composer reads PCRE_VERSION on startup (it records` |
|      - | 3070 | ` * the platform's pcre in the lock file's platform requirements), so a missing one` |
|      - | 3071 | ` * stops it before it can load a repository.` |
|      - | 3072 | ` *` |
|      - | 3073 | ` * Asked of the library rather than of its headers: the numbers differ per platform` |
|      - | 3074 | ` * and per build, and pinning a header's idea of them would report a version this` |
|      - | 3075 | ` * binary is not running.` |
|      - | 3076 | ` */` |
|    330 | 3077 | `PH7_PRIVATE void PH7_PcreVersionInfo(char *zBuf,int nBuf,int *pMajor,int *pMinor,int *pJit)` |
|      5 | 3078 | `{` |
|      - | 3079 | `	int n;` |
|    335 | 3080 | `	if( zBuf && nBuf > 0 ){` |
|    335 | 3081 | `		zBuf[0] = 0;` |
|    335 | 3082 | `		n = pcre2_config(PCRE2_CONFIG_VERSION,zBuf);` |
|    335 | 3083 | `		if( n < 0 \|\| n > nBuf ){` |
|    ! 0 | 3084 | `			zBuf[0] = 0;` |
|    ! 0 | 3085 | `		}` |
|    165 | 3086 | `	}` |
|    335 | 3087 | `	if( pMajor \|\| pMinor ){` |
|      - | 3088 | ``		/* The version string opens `MAJOR.MINOR ` -- php reads its own two numbers`` |
|      - | 3089 | `		 * the same way (its macros come from the same string). */` |
|    169 | 3090 | `		int iMaj = 0,iMin = 0;` |
|    169 | 3091 | `		const char *z = zBuf;` |
|    497 | 3092 | `		while( z && *z >= '0' && *z <= '9' ){ iMaj = iMaj*10 + (*z - '0'); z++; }` |
|    169 | 3093 | `		if( z && *z == '.' ){` |
|    169 | 3094 | `			z++;` |
|    497 | 3095 | `			while( *z >= '0' && *z <= '9' ){ iMin = iMin*10 + (*z - '0'); z++; }` |
|     82 | 3096 | `		}` |
|    169 | 3097 | `		if( pMajor ){ *pMajor = iMaj; }` |
|    169 | 3098 | `		if( pMinor ){ *pMinor = iMin; }` |
|     82 | 3099 | `	}` |
|    335 | 3100 | `	if( pJit ){` |
|     87 | 3101 | `		sxu32 nJit = 0;` |
|     87 | 3102 | `		*pJit = (pcre2_config(PCRE2_CONFIG_JIT,&nJit) == 0 && nJit != 0) ? 1 : 0;` |
|     41 | 3103 | `	}` |
|    335 | 3104 | `}` |
|      - | 3105 |  |
|      - | 3106 | `#else` |
|      - | 3107 | `/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */` |
|      - | 3108 | `typedef int vm_pcre_unused;` |
|      - | 3109 | `#endif /* PH7_ENABLE_PCRE */` |
|      - | 3110 |  |
