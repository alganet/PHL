/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_PCRE
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdlib.h>
#include "ph7int.h"

/*
 * The last-error code lives in ph7_vm::iPcreLastError (per-VM).
 *
 * The compiled-regex cache below is shared across VMs.  pcre2_code objects
 * are immutable after compilation and safe to read concurrently; only the
 * insert/evict path mutates the cache, which is fine in PHL's current
 * single-threaded-execution model.  If PHL ever runs VMs on parallel
 * threads, the cache needs a mutex around PcreCache_Insert.
 */

/* ===== PREG_* constant values (matching PHP) ===== */
#define PHP_PREG_PATTERN_ORDER       1
#define PHP_PREG_SET_ORDER           2
#define PHP_PREG_OFFSET_CAPTURE      256
#define PHP_PREG_UNMATCHED_AS_NULL   512

#define PHP_PREG_SPLIT_NO_EMPTY          1
#define PHP_PREG_GREP_INVERT             1  /* preg_grep()'s only flag */
#define PHP_PREG_SPLIT_DELIM_CAPTURE     2
#define PHP_PREG_SPLIT_OFFSET_CAPTURE    4

#define PHP_PREG_NO_ERROR                0
#define PHP_PREG_INTERNAL_ERROR          1
#define PHP_PREG_BACKTRACK_LIMIT_ERROR   2
#define PHP_PREG_RECURSION_LIMIT_ERROR   3
#define PHP_PREG_BAD_UTF8_ERROR          4
#define PHP_PREG_BAD_UTF8_OFFSET_ERROR   5
#define PHP_PREG_JIT_STACKLIMIT_ERROR    6

/* ===== Compiled-regex cache ===== */
#define PCRE_CACHE_SIZE 16

typedef struct PcreCacheEntry PcreCacheEntry;
struct PcreCacheEntry {
	char *zPattern;          /* Full PHP pattern string (heap copy) */
	sxu32 nLen;
	pcre2_code *pCode;
	sxu32 nCaptureCount;
	sxu32 iLastUsed;
};

static PcreCacheEntry aCache[PCRE_CACHE_SIZE];
static sxu32 nCacheUsed = 0;
static sxu32 iCacheClock = 0;

static pcre2_code *PcreCache_Find(const char *zPattern, sxu32 nLen, sxu32 *pCaptureCount)
{
	sxu32 i;
	for( i = 0; i < nCacheUsed; i++ ){
		if( aCache[i].nLen == nLen && SyMemcmp(aCache[i].zPattern, zPattern, nLen) == 0 ){
			aCache[i].iLastUsed = ++iCacheClock;
			if( pCaptureCount ){
				*pCaptureCount = aCache[i].nCaptureCount;
			}
			return aCache[i].pCode;
		}
	}
	return 0;
}

static void PcreCache_Insert(const char *zPattern, sxu32 nLen, pcre2_code *pCode, sxu32 nCaptureCount)
{
	PcreCacheEntry *pEntry;
	char *zCopy;
	/* Allocate the pattern copy first, before touching the cache */
	zCopy = (char *)malloc(nLen + 1);
	if( zCopy == 0 ){
		/* OOM — pCode is not cached; it leaks but remains usable by the caller */
		return;
	}
	SyMemcpy(zPattern, zCopy, nLen);
	zCopy[nLen] = 0;
	if( nCacheUsed < PCRE_CACHE_SIZE ){
		pEntry = &aCache[nCacheUsed++];
	}else{
		/* Evict LRU */
		sxu32 iMin = aCache[0].iLastUsed;
		sxu32 iMinIdx = 0;
		sxu32 i;
		for( i = 1; i < PCRE_CACHE_SIZE; i++ ){
			if( aCache[i].iLastUsed < iMin ){
				iMin = aCache[i].iLastUsed;
				iMinIdx = i;
			}
		}
		pEntry = &aCache[iMinIdx];
		pcre2_code_free(pEntry->pCode);
		free(pEntry->zPattern);
	}
	pEntry->zPattern = zCopy;
	pEntry->nLen = nLen;
	pEntry->pCode = pCode;
	pEntry->nCaptureCount = nCaptureCount;
	pEntry->iLastUsed = ++iCacheClock;
}

/* ===== Delimiter parser =====
 *
 * php reads the pattern STRING before it ever reaches PCRE2: leading
 * whitespace, one delimiter byte, the body up to its close, then the
 * modifier letters. Two rules in there are easy to get wrong and both
 * mis-match SILENTLY rather than refusing:
 *
 *  - the four bracket delimiters scan for their MATCHING close, counting
 *    nesting, so `{^a{2}$}` is the whole `^a{2}$` and not `^a{2` (which
 *    compiles fine as a literal and then never matches);
 *  - php's leading-whitespace skip is isspace(), not "every byte <= 0x20",
 *    so "\x01^a$\x01" is a pattern delimited by \x01 and a NUL delimiter is
 *    refused rather than skipped over.
 */
#define PCRE_PARSE_OK             0
#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */
#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or NUL delimiter */
#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */
#define PCRE_PARSE_BAD_MODIFIER   4  /* A letter after the close php does not know */

static sxi32 PcreParsePattern(
	const char *zInput, int nInputLen,
	const char **pPattern, int *pnPatternLen,
	const char **pFlags, int *pnFlagLen,
	char *pCloseDelim, int *pbPaired)
{
	const char *zEnd = &zInput[nInputLen];
	const char *z = zInput;
	char cOpen, cClose;
	const char *pStart;
	int nDepth;

	/* Delimiter details for a "no ending delimiter" diagnostic (php names it) */
	*pCloseDelim = 0;
	*pbPaired = 0;
	/* Skip leading whitespace -- php's isspace() set exactly */
	while( z < zEnd && SyisSpace((unsigned char)*z) ){
		z++;
	}
	if( z >= zEnd ){
		return PCRE_PARSE_EMPTY;
	}
	cOpen = *z;
	/* Must not be alphanumeric, backslash, or NUL. Any other control byte IS a
	 * delimiter here -- only the six isspace() bytes were consumed above. */
	if( cOpen == 0 || SyisAlphaNum((unsigned char)cOpen) || cOpen == '\\' ){
		return PCRE_PARSE_BAD_DELIMITER;
	}
	/* Paired delimiters. A CLOSING bracket opens a pattern of its own and
	 * closes on itself -- `)^a$)` is php's, and it scans without nesting. */
	switch( cOpen ){
		case '(': cClose = ')'; break;
		case '[': cClose = ']'; break;
		case '{': cClose = '}'; break;
		case '<': cClose = '>'; break;
		default:  cClose = cOpen; break;
	}
	*pCloseDelim = cClose;
	*pbPaired = (cOpen != cClose);
	z++; /* Skip opening delimiter */
	pStart = z;
	/* Scan for the MATCHING close, respecting backslash escapes: every unescaped
	 * open raises the nesting level and every unescaped close lowers it. This is
	 * a bracket count and nothing else -- a close inside a character class or a
	 * quantifier still counts, which is why `{[{}]}` is the pattern `[{}]`.
	 * An unpaired delimiter falls out of the same loop with no nesting to count:
	 * its close IS its open, so the first one ends the body. */
	nDepth = 1;
	while( z < zEnd ){
		if( *z == '\\' && z + 1 < zEnd ){
			z += 2; /* Skip escaped char */
			continue;
		}
		if( *z == cClose && --nDepth <= 0 ){
			break;
		}
		if( *z == cOpen ){
			nDepth++;
		}
		z++;
	}
	if( z >= zEnd ){
		return PCRE_PARSE_NO_ENDING; /* No closing delimiter */
	}
	*pPattern = pStart;
	*pnPatternLen = (int)(z - pStart);
	z++; /* Skip closing delimiter */
	*pFlags = z;
	*pnFlagLen = (int)(zEnd - z);
	return PH7_OK;
}

/* ===== Flag mapper =====
 *
 * php SCREENS the modifiers and refuses the pattern on the first byte it does
 * not know, before PCRE2 is asked to compile anything -- so `)(\d+))`, whose
 * body is `(\d+`, is "Unknown modifier ')'" and never a compile failure.
 * Space, LF and CR are ignored (a heredoc'd pattern keeps its newline); TAB,
 * VT and FF are not.
 */
static sxi32 PcreMapFlags(
	const char *zFlags, int nFlagLen,
	uint32_t *pCompileOpts, unsigned char *pBadFlag)
{
	int i;
	*pCompileOpts = 0;
	*pBadFlag = 0;
	for( i = 0; i < nFlagLen; i++ ){
		switch( zFlags[i] ){
			case 'i': *pCompileOpts |= PCRE2_CASELESS; break;
			case 'm': *pCompileOpts |= PCRE2_MULTILINE; break;
			case 's': *pCompileOpts |= PCRE2_DOTALL; break;
			case 'x': *pCompileOpts |= PCRE2_EXTENDED; break;
			case 'u': *pCompileOpts |= PCRE2_UTF | PCRE2_UCP; break;
			case 'A': *pCompileOpts |= PCRE2_ANCHORED; break;
			case 'D': *pCompileOpts |= PCRE2_DOLLAR_ENDONLY; break;
			case 'U': *pCompileOpts |= PCRE2_UNGREEDY; break;
			case 'J': *pCompileOpts |= PCRE2_DUPNAMES; break;
			case 'n': *pCompileOpts |= PCRE2_NO_AUTO_CAPTURE; break;
			case 'S': /* Study hint — no-op in PCRE2 */ break;
			case 'X': /* PCRE1's "extra" strictness — no-op in PCRE2 */ break;
			case ' ': case '\n': case '\r': /* php ignores these three */ break;
			default:
				*pBadFlag = (unsigned char)zFlags[i];
				return PCRE_PARSE_BAD_MODIFIER;
		}
	}
	return PH7_OK;
}

/* ===== Compile helper =====
 *
 * PcreCompileQuiet is the whole of it; PcreCompile is that plus php's E_WARNING.
 * The split exists because a caller may have to WORD the failure itself: php's
 * SPL wraps the compile in zend_replace_error_handling(EH_THROW,
 * InvalidArgumentException), so `new RegexIterator($it, 'nodelim')` raises an
 * exception carrying this exact text instead of warning. Nothing else may
 * reproduce these six messages -- they are php's, verbatim, in one place.
 */
static pcre2_code *PcreCompileQuiet(
	ph7_vm *pVm,
	const char *zFullPattern, int nLen,
	sxu32 *pCaptureCount,
	char *zErr, sxu32 nErr)
{
	const char *zPat, *zFlags;
	int nPatLen, nFlagLen;
	uint32_t compileOpts;
	pcre2_code *pCode;
	PCRE2_SIZE erroffset;
	int errcode;
	sxu32 nCapture;
	sxi32 parseRc;
	char cDelim;
	int bPaired;
	unsigned char cBadFlag;

	if( nErr > 0 ){
		zErr[0] = 0;
	}
	/* Check cache first */
	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);
	if( pCode ){
		return pCode;
	}
	/* Parse delimiter */
	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen,
		&cDelim, &bPaired);
	if( parseRc != PCRE_PARSE_OK ){
		if( parseRc == PCRE_PARSE_EMPTY ){
			SyBufferFormat(zErr, nErr, "Empty regular expression");
		}else if( parseRc == PCRE_PARSE_BAD_DELIMITER ){
			SyBufferFormat(zErr, nErr,
				"Delimiter must not be alphanumeric, backslash, or NUL byte");
		}else{
			/* php names the delimiter, and distinguishes paired delimiters */
			SyBufferFormat(zErr, nErr,
				bPaired ? "No ending matching delimiter '%c' found"
				        : "No ending delimiter '%c' found", cDelim);
		}
		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;
		return 0;
	}
	/* Screen the modifiers. php refuses here, BEFORE the compile, so a pattern
	 * that is bad in both ways is reported as the modifier php read first. */
	if( PcreMapFlags(zFlags, nFlagLen, &compileOpts, &cBadFlag) != PCRE_PARSE_OK ){
		if( cBadFlag == 0 ){
			SyBufferFormat(zErr, nErr, "NUL byte is not a valid modifier");
		}else{
			SyBufferFormat(zErr, nErr, "Unknown modifier '%c'", (int)cBadFlag);
		}
		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;
		return 0;
	}
	/* Compile */
	pCode = pcre2_compile(
		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,
		compileOpts, &errcode, &erroffset, NULL);
	if( pCode == 0 ){
		PCRE2_UCHAR errbuf[256];
		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));
		SyBufferFormat(zErr, nErr,
			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);
		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;
		return 0;
	}
	/* Get capture count */
	nCapture = 0;
	pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);
	if( pCaptureCount ){
		*pCaptureCount = nCapture;
	}
	/* Cache it */
	PcreCache_Insert(zFullPattern, (sxu32)nLen, pCode, nCapture);
	pVm->iPcreLastError = PHP_PREG_NO_ERROR;
	return pCode;
}
static pcre2_code *PcreCompile(
	ph7_context *pCtx,
	const char *zFullPattern, int nLen,
	sxu32 *pCaptureCount)
{
	char zErr[288];
	pcre2_code *pCode = PcreCompileQuiet(pCtx->pVm, zFullPattern, nLen, pCaptureCount,
		zErr, sizeof(zErr));
	if( pCode == 0 && zErr[0] ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, zErr);
	}
	return pCode;
}
/*
 * Validate a pattern for a caller that raises its own diagnostic (php's SPL
 * promotes the warning to an InvalidArgumentException). Answers TRUE when the
 * pattern compiles; otherwise FALSE with php's text in zErr. The compiled code
 * stays in the pattern cache, so a later match pays nothing for this.
 */
PH7_PRIVATE int PH7_PcrePatternCheck(ph7_vm *pVm, const char *zPattern, int nLen,
	char *zErr, sxu32 nErr)
{
	return PcreCompileQuiet(&(*pVm), zPattern, nLen, 0, zErr, nErr) != 0;
}

/* ===== Map PCRE2 match error to PHP error code ===== */
static void PcreSetMatchError(ph7_vm *pVm, int rc)
{
	if( rc == PCRE2_ERROR_NOMATCH ){
		pVm->iPcreLastError = PHP_PREG_NO_ERROR;
	}else if( rc == PCRE2_ERROR_MATCHLIMIT ){
		pVm->iPcreLastError = PHP_PREG_BACKTRACK_LIMIT_ERROR;
	}else if( rc == PCRE2_ERROR_DEPTHLIMIT
#ifdef PCRE2_ERROR_RECURSIONLIMIT
		|| rc == PCRE2_ERROR_RECURSIONLIMIT
#endif
	){
		pVm->iPcreLastError = PHP_PREG_RECURSION_LIMIT_ERROR;
	}else if( rc == PCRE2_ERROR_BADUTFOFFSET ){
		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_OFFSET_ERROR;
	}else if( rc == PCRE2_ERROR_UTF8_ERR1
		|| rc == PCRE2_ERROR_UTF8_ERR2 ){
		pVm->iPcreLastError = PHP_PREG_BAD_UTF8_ERROR;
#ifdef PCRE2_ERROR_JIT_STACKLIMIT
	}else if( rc == PCRE2_ERROR_JIT_STACKLIMIT ){
		pVm->iPcreLastError = PHP_PREG_JIT_STACKLIMIT_ERROR;
#endif
	}else{
		pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;
	}
}

/* ===== Helper: populate matches array from ovector ===== */
/*
 * Where the scan resumes after a ZERO-WIDTH match at nAt.
 *
 * php steps on by one CHARACTER, which under a UTF pattern is the whole
 * multi-byte sequence: stepping one BYTE lands INSIDE it, and pcre2_match then
 * refuses the offset (PCRE2_ERROR_BADUTFOFFSET) and the scan simply stops. So
 * `preg_split('/(?<!^)(?!$)/u', 'éÄßご')` — the unicode str_split every library
 * writes, twig's `split` filter and its `random()` among them — answered TWO
 * pieces (the first character, then all the rest) instead of four, and the same
 * one-byte step truncated preg_replace/preg_replace_callback/preg_match_all on
 * any non-ASCII subject. A continuation byte is 10xxxxxx, so skipping them is
 * the whole rule; without PCRE2_UTF the unit is the byte, as php's is.
 */
static PCRE2_SIZE PcreEmptyMatchNext(pcre2_code *pCode, const char *zSubject,
	int nSubLen, PCRE2_SIZE nAt)
{
	uint32_t nOpts = 0;
	PCRE2_SIZE n = nAt + 1;
	pcre2_pattern_info(pCode, PCRE2_INFO_ALLOPTIONS, &nOpts);
	if( nOpts & PCRE2_UTF ){
		while( n < (PCRE2_SIZE)nSubLen && (((unsigned char)zSubject[n]) & 0xC0) == 0x80 ){
			n++;
		}
	}
	return n;
}
/*
 * php reports the NAME of the (*MARK)/(*:NAME) verb the successful match path last
 * passed through, under the string key "MARK". The key exists only when the match
 * actually reached a mark -- a pattern that HAS marks but matched down an unmarked
 * branch has no MARK entry at all -- and it is a plain string even under
 * PREG_OFFSET_CAPTURE, where every other entry is a [value, offset] pair.
 *
 * phpstan/phpdoc-parser's lexer is built on it: one alternation of ~50 marked
 * branches, and `(int) $match['MARK']` is how it names the token it just read. With
 * no MARK key that read is an "Undefined array key" warning per token, which is why
 * every slevomat sniff that parses a docblock died and phpcs reported an internal
 * exception for each file.
 */
static void PcreAddMark(ph7_context *pCtx,ph7_value *pArray,pcre2_match_data *pMatchData)
{
	PCRE2_SPTR zMark = pMatchData ? pcre2_get_mark(pMatchData) : 0;
	ph7_value *pVal;
	if( zMark == 0 ){
		return;
	}
	pVal = ph7_context_new_scalar(pCtx);
	if( pVal == 0 ){
		return;
	}
	ph7_value_string(pVal,(const char *)zMark,-1);
	ph7_array_add_strkey_elem(pArray,"MARK",pVal);
	ph7_context_release_value(pCtx,pVal);
}
static void PcrePopulateMatches(
	ph7_context *pCtx,
	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */
	const char *zSubject,
	PCRE2_SIZE *ovector,
	int nGroups,
	pcre2_code *pCode,
	pcre2_match_data *pMatchData, /* for the MARK entry; may be 0 */
	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */
{
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	ph7_value *pSub = 0;
	uint32_t namecount = 0, nameentrysize = 0;
	PCRE2_SPTR nametable = 0;
	int nMatched = nGroups;
	int i;

	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){
		pSub = ph7_context_new_array(pCtx);
	}
	if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){
		/* pcre2 answers only as many pairs as the LAST group that participated, so
		 * a pattern's trailing optional groups are simply absent -- which is php's
		 * default shape too. PREG_UNMATCHED_AS_NULL is the flag that says "report
		 * every group", so the tail is filled out to the pattern's own capture
		 * count and each missing one answers NULL. Reading the flag only INSIDE the
		 * loop meant `preg_match('/(a)(x)?/','a',$m,PREG_UNMATCHED_AS_NULL)` still
		 * answered two entries where php answers three. */
		uint32_t nCapture = 0;
		pcre2_pattern_info(pCode, PCRE2_INFO_CAPTURECOUNT, &nCapture);
		if( (int)nCapture + 1 > nGroups ){
			nGroups = (int)nCapture + 1;
		}
	}
	/* Read the name table up front so each group's named key can be emitted
	 * INTERLEAVED with its numbered key, in group order — php stores
	 * `0, name, 1, value, 2` (named entry immediately before its number), not
	 * every number followed by every name. Code that iterates $matches or
	 * var_dumps it (PHPUnit's annotation parser) depends on this order. */
	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);
	if( namecount > 0 ){
		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);
		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);
	}
	for( i = 0; i < nGroups; i++ ){
		PCRE2_SIZE start = i < nMatched ? ovector[2 * i]     : PCRE2_UNSET;
		PCRE2_SIZE end   = i < nMatched ? ovector[2 * i + 1] : PCRE2_UNSET;
		const char *zName = 0;
		/* Does group i carry a (?<name>...) label? namecount is tiny in practice. */
		if( namecount > 0 ){
			uint32_t k;
			for( k = 0; k < namecount; k++ ){
				PCRE2_SPTR entry = nametable + k * nameentrysize;
				if( (((entry[0] << 8) | entry[1])) == i ){
					zName = (const char *)(entry + 2);
					break;
				}
			}
		}
		if( start == PCRE2_UNSET ){
			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){
				ph7_value_null(pVal);
			}else{
				ph7_value_string(pVal, "", 0);
			}
			/* Duplicate group names -- `(?J)` -- give two numbered groups one key,
			 * and only one of them can have participated. php writes a name key
			 * from a group that did NOT participate only when nothing is there
			 * yet, so `/(?J)(?<d>a)|(?<d>b)/` on "a" keeps `d => "a"` instead of
			 * having the other alternative's NULL land on top of it. */
			if( zName && ph7_array_fetch(pArray, zName, -1) != 0 ){
				zName = 0;
			}
		}else{
			ph7_value_string(pVal, &zSubject[start], (int)(end - start));
		}
		if( iFlags & PHP_PREG_OFFSET_CAPTURE ){
			ph7_value *pOff = ph7_context_new_scalar(pCtx);
			ph7_array_add_intkey_elem(pSub, 0, pVal);
			ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);
			ph7_array_add_intkey_elem(pSub, 1, pOff);
			/* php: the named key comes first, then the numbered key (same value). */
			if( zName ){
				ph7_array_add_strkey_elem(pArray, zName, pSub);
			}
			ph7_array_add_intkey_elem(pArray, i, pSub);
			ph7_context_release_value(pCtx, pOff);
			ph7_context_release_value(pCtx, pSub);
			pSub = ph7_context_new_array(pCtx);
		}else{
			if( zName ){
				ph7_array_add_strkey_elem(pArray, zName, pVal);
			}
			ph7_array_add_intkey_elem(pArray, i, pVal);
		}
		ph7_value_reset_string_cursor(pVal);
	}
	ph7_context_release_value(pCtx, pVal);
	if( pSub ){
		ph7_context_release_value(pCtx, pSub);
	}
	/* php appends it AFTER every numbered and named group. */
	PcreAddMark(pCtx,pArray,pMatchData);
}

/*
 * Quiet whole-pattern match used by FILTER_VALIDATE_REGEXP: compile zPat (a full
 * "/.../flags" pattern) and test it against zSub. On a successful attempt returns
 * SXRET_OK with *pMatched set to 1 (match) or 0 (no match); returns SXERR_INVALID
 * on a compile/match error (the caller treats that as a validation failure). The
 * compiled code is owned by PcreCompile's cache, so it is not freed here.
 */
PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,
	const char *zSub,int nSub,int *pMatched)
{
	pcre2_code *pCode;
	pcre2_match_data *pMatchData;
	sxu32 nCapture;
	int rc;
	*pMatched = 0;
	pCode = PcreCompile(pCtx,zPat,nPat,&nCapture);
	if( pCode == 0 ){
		return SXERR_INVALID;
	}
	pMatchData = pcre2_match_data_create_from_pattern(pCode,NULL);
	if( pMatchData == 0 ){
		return SXERR_INVALID;
	}
	rc = pcre2_match(pCode,(PCRE2_SPTR)zSub,(PCRE2_SIZE)nSub,0,0,pMatchData,NULL);
	pcre2_match_data_free(pMatchData);
	if( rc < 0 ){
		if( rc != PCRE2_ERROR_NOMATCH ){
			PcreSetMatchError(pCtx->pVm,rc);
			return SXERR_INVALID;
		}
		return SXRET_OK; /* clean no-match */
	}
	*pMatched = 1;
	return SXRET_OK;
}
/* ======================================================================
 * preg_match(pattern, subject [, &matches [, flags [, offset]]])
 * ====================================================================== */
static int PH7_builtin_preg_match(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zPattern, *zSubject;
	int nPatLen, nSubLen;
	pcre2_code *pCode;
	pcre2_match_data *pMatchData;
	PCRE2_SIZE *ovector;
	sxu32 nCapture;
	PCRE2_SIZE startOffset = 0;
	int iFlags = 0;
	int rc;

	if( nArg < 2 ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
			"preg_match() expects at least 2 parameters");
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zPattern = ph7_value_to_string(apArg[0], &nPatLen);
	zSubject = ph7_value_to_string(apArg[1], &nSubLen);
	if( nArg >= 4 ){
		iFlags = ph7_value_to_int(apArg[3]);
	}
	if( nArg >= 5 ){
		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);
	}
	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);
	if( pCode == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	/* php validates $flags AFTER the pattern compiles (a bad pattern warns first).
	 * php 8.5 only rejects flag bits BELOW PREG_OFFSET_CAPTURE (the low byte); any
	 * higher bit is ignored. preg_match permits none of those low bits. */
	if( (iFlags & (PHP_PREG_OFFSET_CAPTURE - 1)) != 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"preg_match(): Argument #4 ($flags) must be a PREG_* constant");
	}
	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);
	if( pMatchData == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,
		startOffset, 0, pMatchData, NULL);
	if( rc < 0 ){
		if( rc != PCRE2_ERROR_NOMATCH ){
			PcreSetMatchError(pCtx->pVm, rc);
		}
		/* Populate empty matches if requested */
		if( nArg >= 3 ){
			ph7_value *pEmpty = ph7_context_new_array(pCtx);
			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pEmpty);
			ph7_context_release_value(pCtx, pEmpty);
		}
		pcre2_match_data_free(pMatchData);
		ph7_result_int(pCtx, 0);
		return PH7_OK;
	}
	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;
	if( nArg >= 3 ){
		/* Populate $matches */
		ph7_value *pArray = ph7_context_new_array(pCtx);
		ovector = pcre2_get_ovector_pointer(pMatchData);
		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, pMatchData, iFlags);
		/* Write the array back to the caller's variable */
		PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pArray);
		ph7_context_release_value(pCtx, pArray);
	}
	pcre2_match_data_free(pMatchData);
	ph7_result_int(pCtx, 1);
	return PH7_OK;
}

/* ======================================================================
 * preg_match_all(pattern, subject [, &matches [, flags [, offset]]])
 * ====================================================================== */
static int PH7_builtin_preg_match_all(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zPattern, *zSubject;
	int nPatLen, nSubLen;
	pcre2_code *pCode;
	pcre2_match_data *pMatchData;
	sxu32 nCapture;
	PCRE2_SIZE startOffset = 0;
	int iFlags = PHP_PREG_PATTERN_ORDER;
	int totalMatches = 0;
	int rc;

	if( nArg < 2 ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
			"preg_match_all() expects at least 2 parameters");
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zPattern = ph7_value_to_string(apArg[0], &nPatLen);
	zSubject = ph7_value_to_string(apArg[1], &nSubLen);
	if( nArg >= 4 ){
		iFlags = ph7_value_to_int(apArg[3]);
	}
	if( nArg >= 5 ){
		startOffset = (PCRE2_SIZE)ph7_value_to_int(apArg[4]);
	}
	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);
	if( pCode == 0 ){
		/* php answers FALSE, not 0 -- the declared return type is int|false and a
		 * caller cannot tell a refused pattern from "no matches" otherwise. */
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	/* php validates $flags AFTER the pattern compiles (a bad pattern warns first).
	 * php 8.5 rejects low-byte bits below PREG_OFFSET_CAPTURE EXCEPT the order flags,
	 * and rejects PATTERN_ORDER+SET_ORDER together (mutually exclusive); higher bits
	 * are ignored. Every case raises the same ValueError. */
	if( (iFlags & (PHP_PREG_OFFSET_CAPTURE - 1)
			& ~(PHP_PREG_PATTERN_ORDER|PHP_PREG_SET_ORDER)) != 0
		|| ((iFlags & PHP_PREG_PATTERN_ORDER) && (iFlags & PHP_PREG_SET_ORDER)) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"preg_match_all(): Argument #4 ($flags) must be a PREG_* constant");
	}
	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);
	if( pMatchData == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;
	{
		ph7_value *pOutArray = (nArg >= 3) ? ph7_context_new_array(pCtx) : 0;

		if( (iFlags & 0xFF) == PHP_PREG_SET_ORDER ){
			while( startOffset <= (PCRE2_SIZE)nSubLen ){
				PCRE2_SIZE *ovector;
				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,
					startOffset, 0, pMatchData, NULL);
				if( rc < 0 ){
					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);
					break;
				}
				ovector = pcre2_get_ovector_pointer(pMatchData);
				if( pOutArray ){
					ph7_value *pSet = ph7_context_new_array(pCtx);
					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, pMatchData, iFlags & ~0xFF);
					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);
					ph7_context_release_value(pCtx, pSet);
				}
				if( ovector[1] == ovector[0] ){
					startOffset = PcreEmptyMatchNext(pCode, zSubject, nSubLen, ovector[0]);
				}else{
					startOffset = ovector[1];
				}
				totalMatches++;
			}
		}else{
			/* PREG_PATTERN_ORDER (default) */
			ph7_value **apGroupArrays = 0;
			/* The marks the successful paths passed through, in match order. php
			 * appends them as one "MARK" array after the numbered groups -- and it
			 * holds ONLY the matches that reached a mark, renumbered from 0, so it is
			 * not aligned with the group arrays beside it. A run where nothing was
			 * marked has no MARK key at all. */
			ph7_value *pMarkArray = 0;
			sxu32 nGroups = nCapture + 1;
			sxu32 g;
			if( pOutArray ){
				apGroupArrays = (ph7_value **)ph7_context_alloc_chunk(pCtx,
					sizeof(ph7_value *) * nGroups, TRUE, FALSE);
				if( apGroupArrays ){
					for( g = 0; g < nGroups; g++ ){
						apGroupArrays[g] = ph7_context_new_array(pCtx);
					}
				}
			}
			while( startOffset <= (PCRE2_SIZE)nSubLen ){
				PCRE2_SIZE *ovector;
				rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,
					startOffset, 0, pMatchData, NULL);
				if( rc < 0 ){
					if( rc != PCRE2_ERROR_NOMATCH ) PcreSetMatchError(pCtx->pVm, rc);
					break;
				}
				ovector = pcre2_get_ovector_pointer(pMatchData);
				if( apGroupArrays ){
					PCRE2_SPTR zMark = pcre2_get_mark(pMatchData);
					ph7_value *pVal = ph7_context_new_scalar(pCtx);
					int nActual = rc;
					if( zMark ){
						if( pMarkArray == 0 ){
							pMarkArray = ph7_context_new_array(pCtx);
						}
						if( pMarkArray ){
							ph7_value_string(pVal,(const char *)zMark,-1);
							ph7_array_add_elem(pMarkArray,0,pVal);
							ph7_value_reset_string_cursor(pVal);
						}
					}
					for( g = 0; g < nGroups; g++ ){
						if( (int)g < nActual && ovector[2*g] != PCRE2_UNSET ){
							PCRE2_SIZE s = ovector[2*g];
							PCRE2_SIZE e = ovector[2*g+1];
							if( iFlags & PHP_PREG_OFFSET_CAPTURE ){
								ph7_value *pSub = ph7_context_new_array(pCtx);
								ph7_value *pOff = ph7_context_new_scalar(pCtx);
								ph7_value_string(pVal, &zSubject[s], (int)(e - s));
								ph7_array_add_intkey_elem(pSub, 0, pVal);
								ph7_value_int(pOff, (int)s);
								ph7_array_add_intkey_elem(pSub, 1, pOff);
								ph7_array_add_elem(apGroupArrays[g], 0, pSub);
								ph7_context_release_value(pCtx, pSub);
								ph7_context_release_value(pCtx, pOff);
							}else{
								ph7_value_string(pVal, &zSubject[s], (int)(e - s));
								ph7_array_add_elem(apGroupArrays[g], 0, pVal);
							}
						}else if( iFlags & PHP_PREG_OFFSET_CAPTURE ){
							/* php reports an unmatched group as the pair ("", -1) --
							 * or (NULL, -1) under PREG_UNMATCHED_AS_NULL. Both flags
							 * were read only on the MATCHED arm, so an unmatched
							 * group answered a bare "" whatever was asked for. */
							ph7_value *pSub = ph7_context_new_array(pCtx);
							ph7_value *pOff = ph7_context_new_scalar(pCtx);
							if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){
								ph7_value_null(pVal);
							}else{
								ph7_value_string(pVal, "", 0);
							}
							ph7_array_add_intkey_elem(pSub, 0, pVal);
							ph7_value_int(pOff, -1);
							ph7_array_add_intkey_elem(pSub, 1, pOff);
							ph7_array_add_elem(apGroupArrays[g], 0, pSub);
							ph7_context_release_value(pCtx, pSub);
							ph7_context_release_value(pCtx, pOff);
						}else{
							if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){
								ph7_value_null(pVal);
							}else{
								ph7_value_string(pVal, "", 0);
							}
							ph7_array_add_elem(apGroupArrays[g], 0, pVal);
						}
						ph7_value_reset_string_cursor(pVal);
					}
					ph7_context_release_value(pCtx, pVal);
				}
				if( ovector[1] == ovector[0] ){
					startOffset = PcreEmptyMatchNext(pCode, zSubject, nSubLen, ovector[0]);
				}else{
					startOffset = ovector[1];
				}
				totalMatches++;
			}
			if( apGroupArrays ){
				/* Attach the per-group match arrays. php's PREG_PATTERN_ORDER stores a
				 * named group under BOTH its name and its number, interleaved
				 * (`0, name, 1, value, 2`) — the same value under each key. Read the
				 * name table so each numbered group can emit its named alias first. */
				uint32_t namecount = 0, nameentrysize = 0;
				PCRE2_SPTR nametable = 0;
				pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);
				if( namecount > 0 ){
					pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);
					pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);
				}
				for( g = 0; g < nGroups; g++ ){
					const char *zName = 0;
					if( namecount > 0 ){
						uint32_t k;
						for( k = 0; k < namecount; k++ ){
							PCRE2_SPTR entry = nametable + k * nameentrysize;
							if( (uint32_t)(((entry[0] << 8) | entry[1])) == g ){
								zName = (const char *)(entry + 2);
								break;
							}
						}
					}
					if( zName ){
						ph7_array_add_strkey_elem(pOutArray, zName, apGroupArrays[g]);
					}
					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);
					ph7_context_release_value(pCtx, apGroupArrays[g]);
				}
				ph7_context_free_chunk(pCtx, apGroupArrays);
				if( pMarkArray ){
					ph7_array_add_strkey_elem(pOutArray,"MARK",pMarkArray);
					ph7_context_release_value(pCtx, pMarkArray);
				}
			}
		}
		/* Write output array to caller's variable */
		if( pOutArray && nArg >= 3 ){
			PH7_VmStoreArgByRef(pCtx->pVm, apArg[2], pOutArray);
			ph7_context_release_value(pCtx, pOutArray);
		}
	}
	pcre2_match_data_free(pMatchData);
	ph7_result_int(pCtx, totalMatches);
	return PH7_OK;
}

/* ======================================================================
 * preg_split(pattern, subject [, limit [, flags]])
 * ====================================================================== */
static int PH7_builtin_preg_split(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zPattern, *zSubject;
	int nPatLen, nSubLen;
	pcre2_code *pCode;
	pcre2_match_data *pMatchData;
	sxu32 nCapture;
	ph7_value *pArray;
	ph7_value *pVal;
	PCRE2_SIZE startOffset = 0, lastOffset = 0;
	int limit = -1;
	int iFlags = 0;
	int nPieces = 0;
	int rc;

	if( nArg < 2 ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
			"preg_split() expects at least 2 parameters");
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zPattern = ph7_value_to_string(apArg[0], &nPatLen);
	zSubject = ph7_value_to_string(apArg[1], &nSubLen);
	if( nArg >= 3 ){
		limit = ph7_value_to_int(apArg[2]);
	}
	if( nArg >= 4 ){
		iFlags = ph7_value_to_int(apArg[3]);
	}
	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);
	if( pCode == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);
	if( pMatchData == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;

	while( startOffset <= (PCRE2_SIZE)nSubLen ){
		if( limit > 0 && nPieces >= limit - 1 ){
			break; /* Last piece gets the remainder */
		}
		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,
			startOffset, 0, pMatchData, NULL);
		if( rc < 0 ){
			if( rc != PCRE2_ERROR_NOMATCH ){
				PcreSetMatchError(pCtx->pVm, rc);
			}
			break;
		}
		{
			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pMatchData);
			PCRE2_SIZE matchStart = ovector[0];
			PCRE2_SIZE matchEnd = ovector[1];
			int pieceLen = (int)(matchStart - lastOffset);

			/* Add the piece before the match */
			if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) || pieceLen > 0 ){
				if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){
					ph7_value *pSub = ph7_context_new_array(pCtx);
					ph7_value *pOff = ph7_context_new_scalar(pCtx);
					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);
					ph7_array_add_intkey_elem(pSub, 0, pVal);
					ph7_value_int(pOff, (int)lastOffset);
					ph7_array_add_intkey_elem(pSub, 1, pOff);
					ph7_array_add_elem(pArray, 0, pSub);
					ph7_context_release_value(pCtx, pSub);
					ph7_context_release_value(pCtx, pOff);
				}else{
					ph7_value_string(pVal, &zSubject[lastOffset], pieceLen);
					ph7_array_add_elem(pArray, 0, pVal);
				}
				ph7_value_reset_string_cursor(pVal);
				nPieces++;
			}
			/* Add captured delimiters if PREG_SPLIT_DELIM_CAPTURE */
			if( iFlags & PHP_PREG_SPLIT_DELIM_CAPTURE ){
				int g;
				for( g = 1; g < rc; g++ ){
					PCRE2_SIZE gs = ovector[2*g];
					PCRE2_SIZE ge = ovector[2*g+1];
					int gLen;
					if( gs == PCRE2_UNSET ) continue;
					gLen = (int)(ge - gs);
					if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) || gLen > 0 ){
						if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){
							ph7_value *pSub = ph7_context_new_array(pCtx);
							ph7_value *pOff = ph7_context_new_scalar(pCtx);
							ph7_value_string(pVal, &zSubject[gs], gLen);
							ph7_array_add_intkey_elem(pSub, 0, pVal);
							ph7_value_int(pOff, (int)gs);
							ph7_array_add_intkey_elem(pSub, 1, pOff);
							ph7_array_add_elem(pArray, 0, pSub);
							ph7_context_release_value(pCtx, pSub);
							ph7_context_release_value(pCtx, pOff);
						}else{
							ph7_value_string(pVal, &zSubject[gs], gLen);
							ph7_array_add_elem(pArray, 0, pVal);
						}
						ph7_value_reset_string_cursor(pVal);
					}
				}
			}
			/* Advance */
			lastOffset = matchEnd;
			if( matchEnd == matchStart ){
				startOffset = PcreEmptyMatchNext(pCode, zSubject, nSubLen, matchStart);
			}else{
				startOffset = matchEnd;
			}
		}
	}
	/* Add trailing piece */
	{
		int trailLen = nSubLen - (int)lastOffset;
		if( !(iFlags & PHP_PREG_SPLIT_NO_EMPTY) || trailLen > 0 ){
			if( iFlags & PHP_PREG_SPLIT_OFFSET_CAPTURE ){
				ph7_value *pSub = ph7_context_new_array(pCtx);
				ph7_value *pOff = ph7_context_new_scalar(pCtx);
				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);
				ph7_array_add_intkey_elem(pSub, 0, pVal);
				ph7_value_int(pOff, (int)lastOffset);
				ph7_array_add_intkey_elem(pSub, 1, pOff);
				ph7_array_add_elem(pArray, 0, pSub);
				ph7_context_release_value(pCtx, pSub);
				ph7_context_release_value(pCtx, pOff);
			}else{
				ph7_value_string(pVal, &zSubject[lastOffset], trailLen);
				ph7_array_add_elem(pArray, 0, pVal);
			}
		}
	}
	ph7_context_release_value(pCtx, pVal);
	pcre2_match_data_free(pMatchData);
	ph7_result_value(pCtx, pArray);
	ph7_context_release_value(pCtx, pArray);
	return PH7_OK;
}

/* ===== Helper: expand backreferences in replacement string ===== */
static void PcreExpandBackrefs(
	SyBlob *pOut,
	const char *zRepl, int nReplLen,
	const char *zSubject,
	PCRE2_SIZE *ovector, int nGroups)
{
	const char *zEnd = &zRepl[nReplLen];
	const char *z = zRepl;

	while( z < zEnd ){
		if( *z == '\\' && z + 1 < zEnd ){
			if( z[1] >= '0' && z[1] <= '9' ){
				int g = z[1] - '0';
				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){
					SyBlobAppend(pOut, &zSubject[ovector[2*g]],
						(sxu32)(ovector[2*g+1] - ovector[2*g]));
				}
				z += 2;
				continue;
			}
			if( z[1] == '\\' ){
				SyBlobAppend(pOut, "\\", 1);
				z += 2;
				continue;
			}
			/* Not a backreference — emit literally */
			SyBlobAppend(pOut, z, 1);
			z++;
			continue;
		}
		if( *z == '$' && z + 1 < zEnd ){
			if( z[1] == '$' ){
				SyBlobAppend(pOut, "$", 1);
				z += 2;
				continue;
			}
			if( z[1] == '{' ){
				/* ${N} form */
				const char *p = z + 2;
				int g = 0;
				while( p < zEnd && *p >= '0' && *p <= '9' ){
					g = g * 10 + (*p - '0');
					p++;
				}
				if( p < zEnd && *p == '}' ){
					if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){
						SyBlobAppend(pOut, &zSubject[ovector[2*g]],
							(sxu32)(ovector[2*g+1] - ovector[2*g]));
					}
					z = p + 1;
					continue;
				}
				/* Not a valid ${N} — emit literally */
				SyBlobAppend(pOut, z, 1);
				z++;
				continue;
			}
			if( z[1] >= '0' && z[1] <= '9' ){
				/* $N or $NN */
				int g = z[1] - '0';
				z += 2;
				/* Check for second digit */
				if( z < zEnd && *z >= '0' && *z <= '9' ){
					int g2 = g * 10 + (*z - '0');
					if( g2 < nGroups ){
						g = g2;
						z++;
					}
				}
				if( g < nGroups && ovector[2*g] != PCRE2_UNSET ){
					SyBlobAppend(pOut, &zSubject[ovector[2*g]],
						(sxu32)(ovector[2*g+1] - ovector[2*g]));
				}
				continue;
			}
			/* Not a backreference */
			SyBlobAppend(pOut, z, 1);
			z++;
			continue;
		}
		SyBlobAppend(pOut, z, 1);
		z++;
	}
}

/* ===== Helper: do replacement for a single pattern+replacement on a single subject ===== */
static void PcreDoReplace(
	ph7_context *pCtx,
	pcre2_code *pCode,
	const char *zSubject, int nSubLen,
	const char *zRepl, int nReplLen,
	int limit,
	int *pCount,
	SyBlob *pOut)
{
	pcre2_match_data *pMatchData;
	PCRE2_SIZE startOffset = 0;
	int nReplacements = 0;
	int rc;

	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);
	if( pMatchData == 0 ) return;

	while( startOffset <= (PCRE2_SIZE)nSubLen ){
		PCRE2_SIZE *ovector;
		if( limit >= 0 && nReplacements >= limit ) break;
		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,
			startOffset, 0, pMatchData, NULL);
		if( rc < 0 ){
			if( rc != PCRE2_ERROR_NOMATCH ){
				PcreSetMatchError(pCtx->pVm, rc);
			}
			break;
		}
		ovector = pcre2_get_ovector_pointer(pMatchData);
		/* Copy text before match */
		if( ovector[0] > startOffset ){
			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));
		}
		/* Expand replacement */
		PcreExpandBackrefs(pOut, zRepl, nReplLen, zSubject, ovector, rc);
		nReplacements++;
		/* Advance */
		if( ovector[1] == ovector[0] ){
			/* Zero-width match: to make progress, emit the character AT THE MATCH
			 * POSITION (ovector[0]) and step past it. The match can sit AHEAD of the
			 * search start (a lookbehind/lookahead assertion, e.g. the camelCase
			 * split /(?<=[[:lower:]])(?=[[:upper:]])/), so copying zSubject[startOffset]
			 * grabbed the wrong byte ("fooBar" -> "foo far"). The text between
			 * startOffset and ovector[0] was already copied above. */
			PCRE2_SIZE nNext = PcreEmptyMatchNext(pCode, zSubject, nSubLen, ovector[0]);
			if( ovector[0] < (PCRE2_SIZE)nSubLen ){
				SyBlobAppend(pOut, &zSubject[ovector[0]], (sxu32)(nNext - ovector[0]));
			}
			startOffset = nNext;
		}else{
			startOffset = ovector[1];
		}
	}
	/* Copy remainder */
	if( startOffset < (PCRE2_SIZE)nSubLen ){
		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));
	}
	if( pCount ){
		*pCount += nReplacements;
	}
	pcre2_match_data_free(pMatchData);
	SXUNUSED(pCtx);
}

/*
 * php's USER-VISIBLE string cast of a pcre PATTERN, REPLACEMENT or SUBJECT --
 * any of which preg_replace()/preg_replace_callback() may be handed as an ARRAY
 * whose elements are themselves anything at all. An element that is an array
 * warns `Array to string conversion` and reads as "Array"; one that is an
 * object with no __toString() is php's catchable
 * `Object of class X could not be converted to string`, and the call then
 * answers nothing. PHL used the SILENT embedder cast here, so the first said
 * nothing and the second rendered as the literal "Object" -- a string php never
 * produces, matched against the subject as if the program had written it.
 *
 * Answers 0 when the coercion threw; the status is on the call context and
 * OP_CALL lands it, so the caller has only to stop.
 */
static int PcreStrUV(ph7_context *pCtx, ph7_value *pVal, const char **pzOut, int *pnOut)
{
	return PH7_ValueToStringUV(pCtx, pVal, pzOut, pnOut) == SXRET_OK;
}
/* ===== Helper: apply pattern(s)+replacement(s) to ONE subject string =====
 * pPattern is a string or an array of patterns; pRepl is a string (used for
 * every pattern) or, only when pPattern is an array, an array taken by ORDER
 * (missing element -> ""). Array patterns are applied sequentially, each to the
 * result of the previous (PHP semantics), ping-ponging two blobs. The final
 * text is appended to pOut. Returns SXRET_OK, or SXERR_SYNTAX on a bad pattern
 * (the caller then yields NULL, matching the scalar path). */
static sxi32 PcreReplaceSubject(
	ph7_context *pCtx,
	ph7_value *pPattern,
	ph7_value *pRepl,
	const char *zSubject, int nSubLen,
	int limit,
	int *pCount,
	SyBlob *pOut)
{
	sxu32 nCapture;
	if( !ph7_value_is_array(pPattern) ){
		/* Single pattern + single replacement */
		const char *zPattern, *zRepl;
		int nPatLen, nReplLen;
		pcre2_code *pCode;
		if( !PcreStrUV(pCtx, pPattern, &zPattern, &nPatLen)
		 || !PcreStrUV(pCtx, pRepl, &zRepl, &nReplLen) ){
			return SXERR_SYNTAX;
		}
		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);
		if( pCode == 0 ){
			return SXERR_SYNTAX; /* NOT SXERR_ABORT: that is a real unwind status */
		}
		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, pCount, pOut);
		return SXRET_OK;
	}else{
		/* Array of patterns: apply each in insertion order to the accumulating
		 * subject. Replacement is the parallel array element (by order) or the
		 * scalar replacement for every pattern. */
		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;
		ph7_hashmap *pRepMap = ph7_value_is_array(pRepl) ? (ph7_hashmap *)pRepl->x.pOther : 0;
		const char *zScalarRepl = 0;
		int nScalarRepl = 0;
		ph7_hashmap_node *pPatNode, *pRepNode;
		ph7_value sPat, sRep;
		SyBlob sA, sB, *pSrc, *pDst;
		sxu32 n;
		sxi32 rc = SXRET_OK;
		if( pRepMap == 0 && !PcreStrUV(pCtx, pRepl, &zScalarRepl, &nScalarRepl) ){
			return SXERR_SYNTAX;
		}
		SyBlobInit(&sA, &pCtx->pVm->sAllocator);
		SyBlobInit(&sB, &pCtx->pVm->sAllocator);
		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */
		pSrc = &sA; pDst = &sB;
		PH7_MemObjInit(pCtx->pVm, &sPat);
		PH7_MemObjInit(pCtx->pVm, &sRep);
		pPatNode = pPatMap->pFirst;
		pRepNode = pRepMap ? pRepMap->pFirst : 0;
		n = pPatMap->nEntry;
		while( n > 0 ){
			const char *zPattern, *zRepl;
			int nPatLen, nReplLen;
			pcre2_code *pCode;
			SyBlob *pSwap;
			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);
			if( !PcreStrUV(pCtx, &sPat, &zPattern, &nPatLen) ){
				rc = SXERR_SYNTAX;
				PH7_MemObjRelease(&sPat);
				break;
			}
			if( pRepMap ){
				if( pRepNode ){
					PH7_HashmapExtractNodeValue(pRepNode, &sRep, FALSE);
					if( !PcreStrUV(pCtx, &sRep, &zRepl, &nReplLen) ){
						rc = SXERR_SYNTAX;
						PH7_MemObjRelease(&sPat);
						PH7_MemObjRelease(&sRep);
						break;
					}
				}else{
					zRepl = ""; nReplLen = 0;
				}
			}else{
				zRepl = zScalarRepl; nReplLen = nScalarRepl;
			}
			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);
			if( pCode == 0 ){
				rc = SXERR_SYNTAX; /* NOT SXERR_ABORT: that is a real unwind status */
				PH7_MemObjRelease(&sPat);
				if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }
				break;
			}
			SyBlobReset(pDst);
			PcreDoReplace(pCtx, pCode,
				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),
				zRepl, nReplLen, limit, pCount, pDst);
			/* The freshly-produced text becomes the subject for the next pattern */
			pSwap = pSrc; pSrc = pDst; pDst = pSwap;
			PH7_MemObjRelease(&sPat);
			if( pRepMap && pRepNode ){ PH7_MemObjRelease(&sRep); }
			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */
			if( pRepNode ){ pRepNode = pRepNode->pPrev; }
			n--;
		}
		if( rc == SXRET_OK ){
			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));
		}
		SyBlobRelease(&sA);
		SyBlobRelease(&sB);
		return rc;
	}
}

/* ======================================================================
 * preg_replace(pattern, replacement, subject [, limit [, &count]])
 * preg_filter(pattern, replacement, subject [, limit [, &count]])
 *
 * php gives the two ONE C body and a flag: preg_filter keeps only the subjects
 * that were actually changed (an array subject loses the untouched keys, a
 * scalar one answers NULL). Everything else -- the diagnostics, &$count, the
 * array shapes -- is the same code, so the two share it here too, and that is
 * also what makes preg_filter's refusals wear its OWN name: they are raised
 * through the calling context, which ph7_function_name() reads.
 * ====================================================================== */
static int PcreReplaceCommon(ph7_context *pCtx, int nArg, ph7_value **apArg, int bFilter)
{
	int limit = -1;
	int count = 0;

	if( nArg < 3 ){
		/* Unreachable while aBuiltinSig[] enforces the arity php reports as an
		 * ArgumentCountError; kept for a direct C caller. */
		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,
			"%s() expects at least 3 parameters", ph7_function_name(pCtx));
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg >= 4 ){
		limit = ph7_value_to_int(apArg[3]);
	}
	if( !ph7_value_is_array(apArg[0]) && ph7_value_is_array(apArg[1]) ){
		/* php 8 refuses the PAIR, as a catchable TypeError naming both positions --
		 * a string pattern cannot consume an array of replacements. PHL warned and
		 * answered NULL, php 5's shape, so a program written against php carried on
		 * past a call php stops it for. */
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #1 ($pattern) must be of type array when argument #2 "
			"($replacement) is an array, string given",ph7_function_name(pCtx));
	}
	/* Only now: preg_last_error() reports the last pattern that RAN, and an
	 * argument php refuses before that never clears it. A cached pattern skips
	 * the compile, which is why the clear cannot live there. */
	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;
	if( ph7_value_is_array(apArg[2]) ){
		/* Array subject: return an array, each element replaced, keys preserved. */
		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;
		ph7_value *pResult = ph7_context_new_array(pCtx);
		ph7_value *pElem = ph7_context_new_scalar(pCtx);
		ph7_value sKey, sVal;
		ph7_hashmap_node *pNode;
		sxu32 n;
		if( pResult == 0 || pElem == 0 ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		PH7_MemObjInit(pCtx->pVm, &sKey);
		PH7_MemObjInit(pCtx->pVm, &sVal);
		pNode = pSubMap ? pSubMap->pFirst : 0;
		n = pSubMap ? pSubMap->nEntry : 0;
		while( n > 0 ){
			const char *zSubject;
			int nSubLen;
			int nBefore = count;
			SyBlob sOut;
			PH7_HashmapExtractNodeKey(pNode, &sKey);
			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);
			if( !PcreStrUV(pCtx, &sVal, &zSubject, &nSubLen) ){
				PH7_MemObjRelease(&sKey);
				PH7_MemObjRelease(&sVal);
				ph7_result_value(pCtx, pResult);
				goto set_count;
			}
			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
			if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){
				/* A refused pattern drops THIS subject and php moves to the next
				 * one, so a two-element array reports the refusal twice and
				 * answers the empty array. A coercion that THREW is the other
				 * kind of failure: it stops the call where php stops it. */
				SyBlobRelease(&sOut);
				PH7_MemObjRelease(&sKey);
				PH7_MemObjRelease(&sVal);
				if( pCtx->nThrowRc ){
					ph7_result_value(pCtx, pResult);
					goto set_count;
				}
				pNode = pNode->pPrev;
				n--;
				continue;
			}
			/* preg_filter keeps a subject only when this one changed */
			if( !bFilter || count > nBefore ){
				ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
				ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */
				ph7_value_reset_string_cursor(pElem);
			}
			SyBlobRelease(&sOut);
			PH7_MemObjRelease(&sKey);
			PH7_MemObjRelease(&sVal);
			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */
			n--;
		}
		ph7_result_value(pCtx, pResult);
	}else{
		/* Scalar subject: one replaced string. */
		const char *zSubject;
		int nSubLen;
		SyBlob sOut;
		if( !PcreStrUV(pCtx, apArg[2], &zSubject, &nSubLen) ){
			ph7_result_null(pCtx);
			goto set_count;
		}
		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
		if( PcreReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen, limit, &count, &sOut) != SXRET_OK ){
			/* Scalar subject: a bad pattern returns NULL (PHP). */
			SyBlobRelease(&sOut);
			ph7_result_null(pCtx);
			goto set_count;
		}
		if( bFilter && count == 0 ){
			ph7_result_null(pCtx);
		}else{
			ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
		}
		SyBlobRelease(&sOut);
	}
set_count:
	/* Set &$count if provided — written on success AND on a bad-pattern failure
	 * (PHP always writes it: 0, or the count accumulated by earlier good patterns).
	 * A coercion that THREW is not one of those: php raises out of the call, so
	 * the caller's variable keeps whatever it held. */
	if( nArg >= 5 && pCtx->nThrowRc == 0 ){
		ph7_value sCount;
		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);
		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);
		PH7_MemObjRelease(&sCount);
	}
	return PH7_OK;
}
static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	return PcreReplaceCommon(pCtx, nArg, apArg, 0);
}
static int PH7_builtin_preg_filter(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	return PcreReplaceCommon(pCtx, nArg, apArg, 1);
}

/* ===== Helper: run the callback over ONE compiled pattern on ONE subject =====
 * The mirror of PcreDoReplace() for preg_replace_callback: the replacement text
 * comes from a user callback fed the match array (shaped by $flags) instead of
 * from a template. Appends the whole replaced subject to pOut and adds its own
 * replacement count to *pCount. Returns SXRET_OK, or the dispatch status when
 * the callback did not return (PH7_CALLBACK_UNWOUND) — the caller then unwinds
 * without producing a result. */
static sxi32 PcreDoCallbackReplace(
	ph7_context *pCtx,
	pcre2_code *pCode,
	const char *zSubject, int nSubLen,
	ph7_value *pCallback,
	int limit,
	int iFlags,
	int *pCount,
	SyBlob *pOut)
{
	pcre2_match_data *pMatchData;
	PCRE2_SIZE startOffset = 0;
	int nReplacements = 0;
	int rc;

	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);
	if( pMatchData == 0 ){
		return SXRET_OK;
	}
	while( startOffset <= (PCRE2_SIZE)nSubLen ){
		PCRE2_SIZE *ovector;
		ph7_value *pMatchArr;
		ph7_value *apCbArg[1];
		ph7_value sResult;
		const char *zReplacement;
		int nReplLen;
		sxi32 rcCb;

		if( limit >= 0 && nReplacements >= limit ) break;
		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,
			startOffset, 0, pMatchData, NULL);
		if( rc < 0 ){
			if( rc != PCRE2_ERROR_NOMATCH ){
				PcreSetMatchError(pCtx->pVm, rc);
			}
			break;
		}
		ovector = pcre2_get_ovector_pointer(pMatchData);
		/* Copy text before match */
		if( ovector[0] > startOffset ){
			SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));
		}
		/* Build matches array for callback */
		pMatchArr = ph7_context_new_array(pCtx);
		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, pMatchData, iFlags);
		/* Call the callback */
		PH7_MemObjInit(pCtx->pVm, &sResult);
		apCbArg[0] = pMatchArr;
		rcCb = PH7_VmCallCallbackByValue(pCtx->pVm, pCallback, 1, apCbArg, &sResult, 0);
		if( PH7_CALLBACK_UNWOUND(rcCb) ){
			/* The callback did not return: propagate so the dispatcher unwinds.
			 * An UNCAUGHT throw comes back as PH7_ABORT, and testing only
			 * PH7_EXCEPTION kept the scan going -- re-running the callback, and
			 * re-reporting the fatal, once per remaining match. */
			PH7_MemObjRelease(&sResult);
			ph7_context_release_value(pCtx, pMatchArr);
			pcre2_match_data_free(pMatchData);
			*pCount += nReplacements;
			return rcCb;
		}
		/* Get replacement string from callback result */
		zReplacement = ph7_value_to_string(&sResult, &nReplLen);
		SyBlobAppend(pOut, zReplacement, (sxu32)nReplLen);
		PH7_MemObjRelease(&sResult);
		ph7_context_release_value(pCtx, pMatchArr);
		nReplacements++;
		/* Advance */
		if( ovector[1] == ovector[0] ){
			/* Zero-width match: emit the character AT THE MATCH POSITION and step
			 * past it. The match can sit AHEAD of the search start (a lookaround
			 * assertion, e.g. the camelCase split), so copying zSubject[startOffset]
			 * grabbed the wrong byte ("fooBar" -> "foo far") — the same fix
			 * PcreDoReplace() carries. */
			PCRE2_SIZE nNext = PcreEmptyMatchNext(pCode, zSubject, nSubLen, ovector[0]);
			if( ovector[0] < (PCRE2_SIZE)nSubLen ){
				SyBlobAppend(pOut, &zSubject[ovector[0]], (sxu32)(nNext - ovector[0]));
			}
			startOffset = nNext;
		}else{
			startOffset = ovector[1];
		}
	}
	/* Copy remainder */
	if( startOffset < (PCRE2_SIZE)nSubLen ){
		SyBlobAppend(pOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));
	}
	*pCount += nReplacements;
	pcre2_match_data_free(pMatchData);
	return SXRET_OK;
}

/* ===== Helper: apply pattern(s)+callback to ONE subject string =====
 * The callback twin of PcreReplaceSubject(): pPattern is a string or an ARRAY of
 * patterns applied sequentially, each to the result of the previous (php
 * semantics), ping-ponging two blobs. Returns SXRET_OK, SXERR_SYNTAX on a bad
 * pattern (the caller then yields NULL / an empty array like the template path),
 * or the dispatch status when the callback did not return. The bad-pattern
 * sentinel must NOT be SXERR_ABORT: that IS the status an exiting or uncaught
 * callback comes back with, and one code for both made a bad pattern kill the
 * script. */
static sxi32 PcreCallbackReplaceSubject(
	ph7_context *pCtx,
	ph7_value *pPattern,
	ph7_value *pCallback,
	const char *zSubject, int nSubLen,
	int limit,
	int iFlags,
	int *pCount,
	SyBlob *pOut)
{
	sxu32 nCapture;
	if( !ph7_value_is_array(pPattern) ){
		const char *zPattern;
		int nPatLen;
		pcre2_code *pCode;
		if( !PcreStrUV(pCtx, pPattern, &zPattern, &nPatLen) ){
			return SXERR_SYNTAX;
		}
		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);
		if( pCode == 0 ){
			return SXERR_SYNTAX; /* bad pattern, NOT a callback unwind */
		}
		return PcreDoCallbackReplace(pCtx, pCode, zSubject, nSubLen, pCallback,
			limit, iFlags, pCount, pOut);
	}else{
		ph7_hashmap *pPatMap = (ph7_hashmap *)pPattern->x.pOther;
		ph7_hashmap_node *pPatNode;
		ph7_value sPat;
		SyBlob sA, sB, *pSrc, *pDst;
		sxu32 n;
		sxi32 rc = SXRET_OK;
		SyBlobInit(&sA, &pCtx->pVm->sAllocator);
		SyBlobInit(&sB, &pCtx->pVm->sAllocator);
		SyBlobAppend(&sA, zSubject, (sxu32)nSubLen); /* seed with the subject */
		pSrc = &sA; pDst = &sB;
		PH7_MemObjInit(pCtx->pVm, &sPat);
		pPatNode = pPatMap ? pPatMap->pFirst : 0;
		n = pPatMap ? pPatMap->nEntry : 0;
		while( n > 0 ){
			const char *zPattern;
			int nPatLen;
			pcre2_code *pCode;
			SyBlob *pSwap;
			PH7_HashmapExtractNodeValue(pPatNode, &sPat, FALSE);
			if( !PcreStrUV(pCtx, &sPat, &zPattern, &nPatLen) ){
				rc = SXERR_SYNTAX;
				PH7_MemObjRelease(&sPat);
				break;
			}
			pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);
			if( pCode == 0 ){
				rc = SXERR_SYNTAX; /* bad pattern, NOT a callback unwind */
				PH7_MemObjRelease(&sPat);
				break;
			}
			SyBlobReset(pDst);
			rc = PcreDoCallbackReplace(pCtx, pCode,
				(const char *)SyBlobData(pSrc), (int)SyBlobLength(pSrc),
				pCallback, limit, iFlags, pCount, pDst);
			/* The freshly-produced text becomes the subject for the next pattern */
			pSwap = pSrc; pSrc = pDst; pDst = pSwap;
			PH7_MemObjRelease(&sPat);
			if( PH7_CALLBACK_UNWOUND(rc) ){
				break;
			}
			pPatNode = pPatNode->pPrev; /* insertion-order walk (reverse link) */
			n--;
		}
		if( rc == SXRET_OK ){
			SyBlobAppend(pOut, SyBlobData(pSrc), SyBlobLength(pSrc));
		}
		SyBlobRelease(&sA);
		SyBlobRelease(&sB);
		return rc;
	}
}

/* ======================================================================
 * preg_replace_callback(pattern, callback, subject [, limit [, &count [, flags]]])
 * ====================================================================== */
static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	int limit = -1;
	int iFlags = 0;
	int count = 0;
	sxi32 rc;

	if( nArg < 3 ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
			"preg_replace_callback() expects at least 3 parameters");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* php screens $callback as an ARGUMENT and says exactly what is wrong with it
	 * ("function \"f\" not found or invalid function name", "no array or string
	 * given", "class \"x\" not found", "array callback must have exactly two
	 * members"). PHL answered one warning for all four and carried on with NULL,
	 * where php raises a catchable TypeError and never runs the call. */
	{
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx, apArg[1], 2, "callback", 0);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	if( nArg >= 4 ){
		limit = ph7_value_to_int(apArg[3]);
	}
	if( nArg >= 6 ){
		/* $flags shapes the match array handed to the callback exactly as it
		 * shapes preg_match()'s &$matches (PREG_OFFSET_CAPTURE /
		 * PREG_UNMATCHED_AS_NULL). php validates nothing here, so neither do we. */
		iFlags = ph7_value_to_int(apArg[5]);
	}
	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;

	if( ph7_value_is_array(apArg[2]) ){
		/* Array subject: return an array, each element replaced, keys preserved
		 * (php; PHL used to stringify the whole array to "Array" and replace in
		 * THAT — a silent wrong answer). */
		ph7_hashmap *pSubMap = (ph7_hashmap *)apArg[2]->x.pOther;
		ph7_value *pResult = ph7_context_new_array(pCtx);
		ph7_value *pElem = ph7_context_new_scalar(pCtx);
		ph7_value sKey, sVal;
		ph7_hashmap_node *pNode;
		sxu32 n;
		if( pResult == 0 || pElem == 0 ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		PH7_MemObjInit(pCtx->pVm, &sKey);
		PH7_MemObjInit(pCtx->pVm, &sVal);
		pNode = pSubMap ? pSubMap->pFirst : 0;
		n = pSubMap ? pSubMap->nEntry : 0;
		while( n > 0 ){
			const char *zSubject;
			int nSubLen;
			SyBlob sOut;
			PH7_HashmapExtractNodeKey(pNode, &sKey);
			PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);
			if( !PcreStrUV(pCtx, &sVal, &zSubject, &nSubLen) ){
				PH7_MemObjRelease(&sKey);
				PH7_MemObjRelease(&sVal);
				ph7_result_value(pCtx, pResult);
				goto set_count;
			}
			SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
			rc = PcreCallbackReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen,
				limit, iFlags, &count, &sOut);
			if( rc != SXRET_OK ){
				/* A refused pattern drops THIS subject and php moves to the next,
				 * so the refusal is reported once per subject and the answer is
				 * the empty array. A throwing callback, and a coercion that
				 * threw, stop the call instead. */
				SyBlobRelease(&sOut);
				PH7_MemObjRelease(&sKey);
				PH7_MemObjRelease(&sVal);
				if( PH7_CALLBACK_UNWOUND(rc) ){
					pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;
					return rc;
				}
				if( pCtx->nThrowRc ){
					ph7_result_value(pCtx, pResult);
					goto set_count;
				}
				pNode = pNode->pPrev;
				n--;
				continue;
			}
			ph7_value_string(pElem, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
			ph7_array_add_elem(pResult, &sKey, pElem); /* copies key+value */
			ph7_value_reset_string_cursor(pElem);
			SyBlobRelease(&sOut);
			PH7_MemObjRelease(&sKey);
			PH7_MemObjRelease(&sVal);
			pNode = pNode->pPrev; /* insertion-order walk (reverse link) */
			n--;
		}
		ph7_result_value(pCtx, pResult);
	}else{
		/* Scalar subject: one replaced string. */
		const char *zSubject;
		int nSubLen;
		SyBlob sOut;
		if( !PcreStrUV(pCtx, apArg[2], &zSubject, &nSubLen) ){
			ph7_result_null(pCtx);
			goto set_count;
		}
		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
		rc = PcreCallbackReplaceSubject(pCtx, apArg[0], apArg[1], zSubject, nSubLen,
			limit, iFlags, &count, &sOut);
		if( rc != SXRET_OK ){
			SyBlobRelease(&sOut);
			if( PH7_CALLBACK_UNWOUND(rc) ){
				/* php records the aborted run: preg_last_error() reads
				 * PREG_INTERNAL_ERROR after a callback that threw. */
				pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;
				return rc;
			}
			/* Scalar subject: a bad pattern returns NULL (php). */
			ph7_result_null(pCtx);
			goto set_count;
		}
		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
		SyBlobRelease(&sOut);
	}
set_count:
	/* Set &$count if provided — written on success AND on a bad-pattern failure
	 * (php always writes it: 0, or the count accumulated by earlier good patterns).
	 * A coercion that THREW is not one of those: php raises out of the call. */
	if( nArg >= 5 && pCtx->nThrowRc == 0 ){
		ph7_value sCount;
		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);
		PH7_VmStoreArgByRef(pCtx->pVm, apArg[4], &sCount);
		PH7_MemObjRelease(&sCount);
	}
	return PH7_OK;
}

/* ======================================================================
 * preg_grep(pattern, array [, flags])
 *
 * php compiles the pattern ONCE, before it looks at the array at all: a refused
 * pattern is one diagnostic under preg_grep's own name and the answer FALSE,
 * even for an empty array. Written as embedded PHP over preg_match() this said
 * `preg_match():` once PER ELEMENT, answered an ARRAY (every element of it, with
 * PREG_GREP_INVERT), and never spoke at all when the array was empty -- so a
 * caller testing `=== false` saw a successful filter that had matched nothing.
 * ====================================================================== */
static int PH7_builtin_preg_grep(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zPattern;
	int nPatLen;
	pcre2_code *pCode;
	pcre2_match_data *pMatchData;
	sxu32 nCapture;
	int bInvert = 0;
	ph7_value *pResult;
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode;
	ph7_value sKey, sVal, sStr;
	sxu32 n;

	/* aBuiltinSig[] enforces php's arity before the call; this is the backstop
	 * that keeps a missing row from turning into an out-of-bounds apArg read. */
	if( nArg < 2 || !ph7_value_is_array(apArg[1]) ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zPattern = ph7_value_to_string(apArg[0], &nPatLen);
	if( nArg >= 3 ){
		bInvert = (ph7_value_to_int(apArg[2]) & PHP_PREG_GREP_INVERT) != 0;
	}
	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);
	if( pCode == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);
	if( pMatchData == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pResult = ph7_context_new_array(pCtx);
	if( pResult == 0 ){
		pcre2_match_data_free(pMatchData);
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	PH7_MemObjInit(pCtx->pVm, &sKey);
	PH7_MemObjInit(pCtx->pVm, &sVal);
	PH7_MemObjInit(pCtx->pVm, &sStr);
	pNode = pMap ? pMap->pFirst : 0;
	n = pMap ? pMap->nEntry : 0;
	while( n > 0 ){
		const char *zSubject;
		int nSubLen, bKeep, rc;
		PH7_HashmapExtractNodeKey(pNode, &sKey);
		PH7_HashmapExtractNodeValue(pNode, &sVal, FALSE);
		/* The MATCH is made against the element's string form; what is KEPT is
		 * the element itself, so an int or a float comes back out as it went in.
		 * PcreStrUV coerces in place, hence the second slot. */
		PH7_MemObjRelease(&sStr);
		PH7_MemObjStore(&sVal, &sStr);
		sStr.nIdx = SXU32_HIGH;
		if( !PcreStrUV(pCtx, &sStr, &zSubject, &nSubLen) ){
			/* A coercion that threw stops the call where php stops it. */
			PH7_MemObjRelease(&sKey);
			PH7_MemObjRelease(&sVal);
			break;
		}
		rc = pcre2_match(pCode, (PCRE2_SPTR)zSubject, (PCRE2_SIZE)nSubLen,
			0, 0, pMatchData, NULL);
		if( rc < 0 && rc != PCRE2_ERROR_NOMATCH ){
			PcreSetMatchError(pCtx->pVm, rc);
		}
		bKeep = (rc >= 0);
		if( bInvert ){
			bKeep = !bKeep;
		}
		if( bKeep ){
			ph7_array_add_elem(pResult, &sKey, &sVal); /* copies key+value */
		}
		PH7_MemObjRelease(&sKey);
		PH7_MemObjRelease(&sVal);
		pNode = pNode->pPrev; /* insertion-order walk (reverse link) */
		n--;
	}
	PH7_MemObjRelease(&sStr);
	pcre2_match_data_free(pMatchData);
	ph7_result_value(pCtx, pResult);
	return PH7_OK;
}

/* ======================================================================
 * preg_replace_callback_array(pattern, subject [, limit [, &count [, flags]]])
 *
 * One pass of preg_replace_callback() per entry, each fed the PREVIOUS entry's
 * output, and php validates each entry only when it reaches it -- so with
 * ['/a/' => good, '/b/' => 'nosuchfn'] the first replacement has already
 * happened when the TypeError for the second is raised. A refused pattern
 * answers NULL and leaves &$count untouched; an ARRAY subject is not that kind
 * of failure and degrades to the empty array with count 0.
 * ====================================================================== */
static int PH7_builtin_preg_replace_callback_array(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_value sSubject, sPat, sCb, sLimit, sCount, sFlags;
	ph7_value *apDeleg[6];
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode;
	sxu32 n;
	int total = 0;
	int bFailed = 0;
	sxi32 rc = PH7_OK;

	/* Same backstop as preg_grep's: the arity php reports is aBuiltinSig[]'s. */
	if( nArg < 2 || !ph7_value_is_array(apArg[0]) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm, &sSubject);
	PH7_MemObjInit(pCtx->pVm, &sPat);
	PH7_MemObjInit(pCtx->pVm, &sCb);
	PH7_MemObjInitFromInt(pCtx->pVm, &sLimit, nArg >= 3 ? ph7_value_to_int(apArg[2]) : -1);
	PH7_MemObjInitFromInt(pCtx->pVm, &sCount, 0);
	PH7_MemObjInitFromInt(pCtx->pVm, &sFlags, nArg >= 5 ? ph7_value_to_int(apArg[4]) : 0);
	/* &$count out-slot with no caller variable behind it: PH7_VmStoreArgByRef
	 * writes through nIdx when it is not SXU32_HIGH, and a zeroed ph7_value's
	 * nIdx is 0 -- a REAL slot index, which would corrupt aMemObj[0]. */
	sCount.nIdx = SXU32_HIGH;
	PH7_MemObjStore(apArg[1], &sSubject);
	sSubject.nIdx = SXU32_HIGH;
	apDeleg[0] = &sPat;
	apDeleg[1] = &sCb;
	apDeleg[2] = &sSubject;
	apDeleg[3] = &sLimit;
	apDeleg[4] = &sCount;
	apDeleg[5] = &sFlags;
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	pNode = pMap ? pMap->pFirst : 0;
	n = pMap ? pMap->nEntry : 0;
	while( n > 0 ){
		PH7_HashmapExtractNodeKey(pNode, &sPat);
		PH7_HashmapExtractNodeValue(pNode, &sCb, FALSE);
		if( (sPat.iFlags & MEMOBJ_STRING) == 0 ){
			/* An INT key is not a pattern; php names the whole argument. */
			rc = PH7_VmThrowException(pCtx, "TypeError",
				"preg_replace_callback_array(): Argument #1 ($pattern) must contain "
				"only string patterns as keys");
			goto done;
		}
		if( !ph7_value_is_callable(&sCb) ){
			rc = PH7_VmThrowException(pCtx, "TypeError",
				"preg_replace_callback_array(): Argument #1 ($pattern) must contain "
				"only valid callbacks");
			goto done;
		}
		/* pCtx->pRet ACCUMULATES -- ph7_result_string() appends to the return
		 * slot's blob -- so each delegated pass has to start from an empty one
		 * or the second entry's answer is glued onto the first's. */
		ph7_result_null(pCtx);
		rc = PH7_builtin_preg_replace_callback(pCtx, 6, apDeleg);
		if( rc != PH7_OK ){
			goto done;   /* the callback threw; it is already unwinding */
		}
		if( ph7_value_is_null(pCtx->pRet) ){
			/* A refused pattern: NULL, and &$count keeps whatever it held. */
			bFailed = 1;
			goto done;
		}
		total += ph7_value_to_int(&sCount);
		/* This entry's output is the next entry's subject */
		PH7_MemObjStore(pCtx->pRet, &sSubject);
		sSubject.nIdx = SXU32_HIGH;
		PH7_MemObjRelease(&sPat);
		PH7_MemObjRelease(&sCb);
		pNode = pNode->pPrev; /* insertion-order walk (reverse link) */
		n--;
	}
done:
	ph7_result_null(pCtx);
	if( !bFailed && rc == PH7_OK ){
		ph7_result_value(pCtx, &sSubject);
		if( nArg >= 4 ){
			ph7_value sTotal;
			PH7_MemObjInitFromInt(pCtx->pVm, &sTotal, total);
			PH7_VmStoreArgByRef(pCtx->pVm, apArg[3], &sTotal);
			PH7_MemObjRelease(&sTotal);
		}
	}
	PH7_MemObjRelease(&sSubject);
	PH7_MemObjRelease(&sPat);
	PH7_MemObjRelease(&sCb);
	PH7_MemObjRelease(&sLimit);
	PH7_MemObjRelease(&sCount);
	PH7_MemObjRelease(&sFlags);
	return rc;
}

/* ======================================================================
 * preg_quote(str [, delimiter])
 * ====================================================================== */
static int PH7_builtin_preg_quote(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zStr, *zDelim = 0;
	int nLen, nDelimLen = 0;
	const char *z, *zEnd;

	if( nArg < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zStr = ph7_value_to_string(apArg[0], &nLen);
	if( nArg >= 2 ){
		zDelim = ph7_value_to_string(apArg[1], &nDelimLen);
	}
	/* Type the result as a STRING up front: an empty subject quotes to the empty
	 * string, and the loop below would otherwise never touch the result at all,
	 * leaving php's `string` return as NULL. */
	ph7_result_string(pCtx, "", 0);
	z = zStr;
	zEnd = &zStr[nLen];
	while( z < zEnd ){
		char c = *z;
		if( c == '\0' ){
			/* php spells NUL as the three-digit escape "\000" (a backslash and a raw NUL
			 * byte, which is what this emitted, is not an escape at all: pcre reads the
			 * backslash as quoting the byte that FOLLOWS the NUL). */
			ph7_result_string(pCtx, "\\000", 4);
			z++;
			continue;
		}
		switch( c ){
			case '.': case '\\': case '+': case '*': case '?':
			case '[': case '^': case ']': case '$': case '(':
			case ')': case '{': case '}': case '=': case '!':
			case '<': case '>': case '|': case ':': case '-':
			case '#':
				ph7_result_string(pCtx, "\\", 1);
				break;
			default:
				if( nDelimLen > 0 && c == zDelim[0] ){
					ph7_result_string(pCtx, "\\", 1);
				}
				break;
		}
		ph7_result_string(pCtx, z, 1);
		z++;
	}
	return PH7_OK;
}

/* ======================================================================
 * preg_last_error()
 * ====================================================================== */
static int PH7_builtin_preg_last_error(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int(pCtx, pCtx->pVm->iPcreLastError);
	return PH7_OK;
}

/* ======================================================================
 * preg_last_error_msg()
 * ====================================================================== */
static int PH7_builtin_preg_last_error_msg(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zMsg;
	SXUNUSED(nArg); SXUNUSED(apArg);
	switch( pCtx->pVm->iPcreLastError ){
		case PHP_PREG_NO_ERROR:               zMsg = "No error"; break;
		case PHP_PREG_INTERNAL_ERROR:         zMsg = "Internal error"; break;
		case PHP_PREG_BACKTRACK_LIMIT_ERROR:  zMsg = "Backtrack limit exhausted"; break;
		case PHP_PREG_RECURSION_LIMIT_ERROR:  zMsg = "Recursion limit exhausted"; break;
		case PHP_PREG_BAD_UTF8_ERROR:         zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded"; break;
		case PHP_PREG_BAD_UTF8_OFFSET_ERROR:  zMsg = "The offset did not correspond to the beginning of a valid UTF-8 code point"; break;
		case PHP_PREG_JIT_STACKLIMIT_ERROR:   zMsg = "JIT stack limit exhausted"; break;
		default: zMsg = "Unknown error"; break;
	}
	ph7_result_string(pCtx, zMsg, -1);
	return PH7_OK;
}

/*
 * The regex operation behind RegexIterator::accept(), in php's five REGIT modes.
 *
 * php reaches php_pcre_match_impl / php_pcre_split_impl / php_pcre_replace_impl
 * from spl_iterators.c rather than re-deriving any of it, and this is that door:
 * every mode is one of the builtins above, called with the arguments the PHP
 * spelling would have passed. The builtins answer through pCtx->pRet, which is
 * also accept()'s own return slot -- harmless because the caller writes its
 * boolean after this returns, and the reason pOut is a separate parameter.
 *
 * *pbOk is php's per-mode "matched" test: a positive match count, more than one
 * SPLIT piece, at least one REPLACE substitution. pOut receives the transformed
 * value for every mode but MATCH, which leaves the cached current() alone.
 */
PH7_PRIVATE sxi32 PH7_PcreRegitApply(
	ph7_context *pCtx,
	int iMode,               /* PH7_REGIT_* */
	ph7_value *pPattern,
	ph7_value *pSubject,
	int iPregFlags,
	ph7_value *pRepl,        /* REPLACE only */
	ph7_value *pOut,         /* transformed value (modes other than MATCH) */
	int *pbOk
	)
{
	ph7_value *apArg[5];
	ph7_value sFlags, sLimit, sCount;
	sxi32 rc = PH7_OK;
	*pbOk = 0;
	PH7_MemObjInitFromInt(pCtx->pVm,&sFlags,iPregFlags);
	PH7_MemObjInitFromInt(pCtx->pVm,&sLimit,-1);
	PH7_MemObjInitFromInt(pCtx->pVm,&sCount,0);
	/* A by-ref out-parameter with no caller slot behind it: PH7_VmStoreArgByRef
	 * writes through nIdx when it is not SXU32_HIGH, and a zeroed ph7_value's
	 * nIdx is 0 -- a REAL slot index, which would corrupt aMemObj[0]. */
	sCount.nIdx = SXU32_HIGH;
	if( pOut ){
		pOut->nIdx = SXU32_HIGH;
	}
	apArg[0] = pPattern;
	switch( iMode ){
		case PH7_REGIT_MATCH:
			apArg[1] = pSubject;
			rc = PH7_builtin_preg_match(pCtx,2,apArg);
			*pbOk = ph7_value_to_int(pCtx->pRet) > 0;
			break;
		case PH7_REGIT_GET_MATCH:
		case PH7_REGIT_ALL_MATCHES:
			apArg[1] = pSubject;
			apArg[2] = pOut;
			apArg[3] = &sFlags;
			rc = iMode == PH7_REGIT_GET_MATCH
				? PH7_builtin_preg_match(pCtx,4,apArg)
				: PH7_builtin_preg_match_all(pCtx,4,apArg);
			*pbOk = ph7_value_to_int(pCtx->pRet) > 0;
			break;
		case PH7_REGIT_SPLIT:
			apArg[1] = pSubject;
			apArg[2] = &sLimit;
			apArg[3] = &sFlags;
			rc = PH7_builtin_preg_split(pCtx,4,apArg);
			PH7_MemObjStore(pCtx->pRet,pOut);
			if( pOut->iFlags & MEMOBJ_HASHMAP ){
				*pbOk = ((ph7_hashmap *)pOut->x.pOther)->nEntry > 1;
			}
			break;
		case PH7_REGIT_REPLACE:
			apArg[1] = pRepl;
			apArg[2] = pSubject;
			apArg[3] = &sLimit;
			apArg[4] = &sCount;
			rc = PH7_builtin_preg_replace(pCtx,5,apArg);
			PH7_MemObjStore(pCtx->pRet,pOut);
			*pbOk = ph7_value_to_int(&sCount) > 0;
			break;
		default:
			break;
	}
	PH7_MemObjRelease(&sFlags);
	PH7_MemObjRelease(&sLimit);
	PH7_MemObjRelease(&sCount);
	return rc;
}
/* ===== Function registration table ===== */
/* ======================================================================
 * mbstring's regular expressions: mb_ereg*, mb_split, mb_regex_encoding
 * and mb_regex_set_options.
 *
 * php runs this family on Oniguruma, not on PCRE2, and the two disagree in
 * two places that matter and one that does not:
 *
 *  - Oniguruma's ONIG_OPTION_MULTILINE is PCRE2's DOTALL (dot matches a
 *    newline), and its ONIG_OPTION_SINGLELINE is the ABSENCE of PCRE2's
 *    MULTILINE (`^` is `\A` and `$` is `\Z`). mbstring's default option
 *    string is "pr" -- both of those bits, ruby syntax -- so the family
 *    starts out with a dot that crosses lines and anchors that do not,
 *    which is the exact opposite of what the letters look like they say.
 *  - `l` (find-longest) and `n` (find-not-empty) have no PCRE2 spelling.
 *    They are ACCEPTED and reported back by mb_regex_set_options(), and
 *    they do not change the match. So are the eight syntax letters: the
 *    grammar is PCRE2's whichever one is named.
 *  - a pattern that does not compile warns with PCRE2's message text where
 *    php prints Oniguruma's. The diagnostic's shape -- a warning naming the
 *    function, "mbregex compile err: ", and a false return -- is php's.
 *
 * The subject is matched as bytes; only UTF-8 is handed to PCRE2 as text
 * (with UCP, so `\w` covers the same letters Oniguruma's does). Every
 * offset this family reports or takes is a byte offset, which is php's.
 * ====================================================================== */
#ifndef PH7_DISABLE_BUILTIN_FUNC

#define MBRE_OPT_SET         0x0100  /* "a script has set these", so 0 reads as the default */
#define MBRE_OPT_IGNORECASE  0x0001
#define MBRE_OPT_EXTEND      0x0002
#define MBRE_OPT_MULTILINE   0x0004  /* onig: dot matches newline */
#define MBRE_OPT_SINGLELINE  0x0008  /* onig: ^ is \A and $ is \Z */
#define MBRE_OPT_LONGEST     0x0010  /* accepted, no PCRE2 spelling */
#define MBRE_OPT_NOTEMPTY    0x0020  /* accepted, no PCRE2 spelling */

#define MBRE_OPT_DEFAULT (MBRE_OPT_MULTILINE|MBRE_OPT_SINGLELINE)
#define MBRE_SYNTAX_DEFAULT 'r'

static sxu32 MbReOptOf(ph7_vm *pVm)
{
	return pVm->iMbReOpt ? (pVm->iMbReOpt & ~(sxu32)MBRE_OPT_SET) : (sxu32)MBRE_OPT_DEFAULT;
}
static int MbReSyntaxOf(ph7_vm *pVm)
{
	return pVm->iMbReSyntax ? (int)pVm->iMbReSyntax : MBRE_SYNTAX_DEFAULT;
}
/* Read an option string. The option bits are REPLACED wholesale (an empty
 * string clears them all); the syntax letter is replaced only when one is
 * present, which is why mb_regex_set_options('i') answers "ir". */
static int MbReParseOpt(const char *z,int n,sxu32 *pOpt,int *pSyntax,unsigned char *pBad)
{
	sxu32 opt = 0;
	int i;
	for( i = 0 ; i < n ; ++i ){
		switch( z[i] ){
		case 'i': opt |= MBRE_OPT_IGNORECASE; break;
		case 'x': opt |= MBRE_OPT_EXTEND; break;
		case 'm': opt |= MBRE_OPT_MULTILINE; break;
		case 's': opt |= MBRE_OPT_SINGLELINE; break;
		case 'p': opt |= MBRE_OPT_MULTILINE|MBRE_OPT_SINGLELINE; break;
		case 'l': opt |= MBRE_OPT_LONGEST; break;
		case 'n': opt |= MBRE_OPT_NOTEMPTY; break;
		/* The syntax letters: java, gnu, grep, emacs, ruby, perl and the two
		 * POSIX grammars. PCRE2 answers all eight. */
		case 'j': case 'u': case 'g': case 'c':
		case 'r': case 'z': case 'b': case 'd':
			*pSyntax = (unsigned char)z[i];
			break;
		default:
			*pBad = (unsigned char)z[i];
			return -1;
		}
	}
	*pOpt = opt;
	return 0;
}
/* ...and write one back. php's order is i, x, the line pair, l, n, syntax --
 * and MULTILINE|SINGLELINE together collapse to the single letter 'p'. */
static int MbReOptString(sxu32 opt,int iSyntax,char *zBuf)
{
	int n = 0;
	if( opt & MBRE_OPT_IGNORECASE ){ zBuf[n++] = 'i'; }
	if( opt & MBRE_OPT_EXTEND ){ zBuf[n++] = 'x'; }
	if( (opt & (MBRE_OPT_MULTILINE|MBRE_OPT_SINGLELINE))
		== (MBRE_OPT_MULTILINE|MBRE_OPT_SINGLELINE) ){
		zBuf[n++] = 'p';
	}else{
		if( opt & MBRE_OPT_MULTILINE ){ zBuf[n++] = 'm'; }
		if( opt & MBRE_OPT_SINGLELINE ){ zBuf[n++] = 's'; }
	}
	if( opt & MBRE_OPT_LONGEST ){ zBuf[n++] = 'l'; }
	if( opt & MBRE_OPT_NOTEMPTY ){ zBuf[n++] = 'n'; }
	zBuf[n++] = (char)iSyntax;
	zBuf[n] = 0;
	return n;
}
/* The optional $options argument every matcher carries. It does NOT touch the
 * VM's own options -- mb_ereg_replace($p,$r,$s,'i') leaves
 * mb_regex_set_options() reading what it read before. */
static int MbReOptArg(ph7_context *pCtx,ph7_value *pArg,sxu32 *pOpt,int *pSyntax)
{
	const char *zOpt;
	int nOpt;
	unsigned char cBad = 0;
	*pOpt = MbReOptOf(pCtx->pVm);
	*pSyntax = MbReSyntaxOf(pCtx->pVm);
	if( pArg == 0 || ph7_value_is_null(pArg) ){
		return 0;
	}
	zOpt = ph7_value_to_string(pArg,&nOpt);
	if( MbReParseOpt(zOpt,nOpt,pOpt,pSyntax,&cBad) != 0 ){
		PH7_VmThrowException(pCtx,"ValueError","Option \"%c\" is not supported",(int)cBad);
		return -1;
	}
	return 0;
}
/* One compiled pattern is kept, because mb_ereg_search() walks a subject one
 * call at a time and would otherwise recompile per step. It is keyed on the
 * pattern bytes AND the options they were read under, so the same pattern
 * asked case-insensitively is a different entry. */
static struct {
	char *zKey;
	sxu32 nKey;
	pcre2_code *pCode;
	sxu32 nCapture;
} sMbReCache = { 0, 0, 0, 0 };

static pcre2_code * MbReCompile(
	ph7_context *pCtx,
	const char *zPat,int nPat,
	sxu32 iOpt,
	const char *zFunc,
	sxu32 *pCapture)
{
	uint32_t compileOpts = 0;
	pcre2_code *pCode;
	PCRE2_SIZE erroffset;
	int errcode;
	sxu32 nCapture, nKey;
	char *zKey;
	int bUtf8 = PH7_MbEncodingIsUtf8(pCtx->pVm->iMbReEnc);

	/* key = the pattern, then the options and the framing */
	nKey = (sxu32)nPat + 3;
	if( sMbReCache.zKey && sMbReCache.nKey == nKey
		&& SyMemcmp(sMbReCache.zKey,zPat,(sxu32)nPat) == 0
		&& sMbReCache.zKey[nPat] == (char)(iOpt & 0xFF)
		&& sMbReCache.zKey[nPat+1] == (char)((iOpt >> 8) & 0xFF)
		&& sMbReCache.zKey[nPat+2] == (char)bUtf8 ){
		*pCapture = sMbReCache.nCapture;
		return sMbReCache.pCode;
	}
	if( bUtf8 ){
		/* UCP so that \w, \d and the POSIX classes cover what Oniguruma's do
		 * over UTF-8: mb_ereg('(\w+)','héllo') answers the whole word. */
		compileOpts |= PCRE2_UTF | PCRE2_UCP;
	}
	if( iOpt & MBRE_OPT_IGNORECASE ){ compileOpts |= PCRE2_CASELESS; }
	if( iOpt & MBRE_OPT_EXTEND ){ compileOpts |= PCRE2_EXTENDED; }
	if( iOpt & MBRE_OPT_MULTILINE ){ compileOpts |= PCRE2_DOTALL; }
	if( (iOpt & MBRE_OPT_SINGLELINE) == 0 ){ compileOpts |= PCRE2_MULTILINE; }
	pCode = pcre2_compile((PCRE2_SPTR)zPat,(PCRE2_SIZE)nPat,compileOpts,
		&errcode,&erroffset,NULL);
	if( pCode == 0 ){
		PCRE2_UCHAR errbuf[256];
		pcre2_get_error_message(errcode,errbuf,sizeof(errbuf));
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"%s(): mbregex compile err: %s",zFunc,(const char *)errbuf);
		return 0;
	}
	nCapture = 0;
	pcre2_pattern_info(pCode,PCRE2_INFO_CAPTURECOUNT,&nCapture);
	zKey = (char *)malloc(nKey);
	if( zKey == 0 ){
		/* No room to remember it; the caller still gets a usable pattern and the
		 * next call compiles again. */
		pcre2_code_free(pCode);
		return 0;
	}
	SyMemcpy(zPat,zKey,(sxu32)nPat);
	zKey[nPat]     = (char)(iOpt & 0xFF);
	zKey[nPat + 1] = (char)((iOpt >> 8) & 0xFF);
	zKey[nPat + 2] = (char)bUtf8;
	if( sMbReCache.pCode ){
		pcre2_code_free(sMbReCache.pCode);
		free(sMbReCache.zKey);
	}
	sMbReCache.zKey = zKey;
	sMbReCache.nKey = nKey;
	sMbReCache.pCode = pCode;
	sMbReCache.nCapture = nCapture;
	*pCapture = nCapture;
	return pCode;
}
/* An unmatched group is `false` here, not the empty string preg_match writes,
 * and every group the pattern declares is present whether it participated or
 * not. Named groups are appended AFTER the numbered ones -- preg_match
 * interleaves them, mbstring does not. */
static void MbRePopulate(
	ph7_context *pCtx,
	ph7_value *pArray,
	const char *zSub,
	const sxu32 *aOv,int nGroup,
	pcre2_code *pCode)
{
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	uint32_t namecount = 0, nameentrysize = 0;
	PCRE2_SPTR nametable = 0;
	int i;
	for( i = 0 ; i < nGroup ; ++i ){
		if( aOv[2*i] == SXU32_HIGH ){
			ph7_value_bool(pVal,0);
		}else{
			ph7_value_string(pVal,&zSub[aOv[2*i]],(int)(aOv[2*i+1] - aOv[2*i]));
		}
		ph7_array_add_intkey_elem(pArray,i,pVal);
		ph7_value_reset_string_cursor(pVal);
	}
	pcre2_pattern_info(pCode,PCRE2_INFO_NAMECOUNT,&namecount);
	if( namecount > 0 ){
		uint32_t k;
		pcre2_pattern_info(pCode,PCRE2_INFO_NAMETABLE,&nametable);
		pcre2_pattern_info(pCode,PCRE2_INFO_NAMEENTRYSIZE,&nameentrysize);
		for( k = 0 ; k < namecount ; ++k ){
			PCRE2_SPTR entry = nametable + k * nameentrysize;
			int iNum = (entry[0] << 8) | entry[1];
			if( iNum >= nGroup ){
				continue;
			}
			if( aOv[2*iNum] == SXU32_HIGH ){
				ph7_value_bool(pVal,0);
			}else{
				ph7_value_string(pVal,&zSub[aOv[2*iNum]],
					(int)(aOv[2*iNum+1] - aOv[2*iNum]));
			}
			ph7_array_add_strkey_elem(pArray,(const char *)(entry + 2),pVal);
			ph7_value_reset_string_cursor(pVal);
		}
	}
	ph7_context_release_value(pCtx,pVal);
}
/* Run one match and copy the offsets out as byte positions. Answers 1 on a
 * match, 0 on no match, -1 on a PCRE2 error. *pnGroup is the pattern's own
 * capture count plus one, so a trailing optional group that did not
 * participate still gets a slot -- php reports it as false. */
static int MbReMatch(
	pcre2_code *pCode,sxu32 nCapture,
	const char *zSub,int nSub,sxu32 iStart,
	sxu32 *aOv,int *pnGroup)
{
	pcre2_match_data *pData;
	PCRE2_SIZE *ov;
	int rc,i,nGroup;

	pData = pcre2_match_data_create_from_pattern(pCode,NULL);
	if( pData == 0 ){
		return -1;
	}
	rc = pcre2_match(pCode,(PCRE2_SPTR)zSub,(PCRE2_SIZE)nSub,
		(PCRE2_SIZE)iStart,0,pData,NULL);
	if( rc < 0 ){
		pcre2_match_data_free(pData);
		return rc == PCRE2_ERROR_NOMATCH ? 0 : -1;
	}
	ov = pcre2_get_ovector_pointer(pData);
	nGroup = (int)nCapture + 1;
	for( i = 0 ; i < nGroup ; ++i ){
		if( i < rc && ov[2*i] != PCRE2_UNSET ){
			aOv[2*i]     = (sxu32)ov[2*i];
			aOv[2*i + 1] = (sxu32)ov[2*i + 1];
		}else{
			aOv[2*i] = aOv[2*i + 1] = SXU32_HIGH;
		}
	}
	*pnGroup = nGroup;
	pcre2_match_data_free(pData);
	return 1;
}
/* mb_ereg / mb_eregi */
static int MbEregCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCase)
{
	const char *zPat,*zSub;
	int nPat,nSub,nGroup = 0,rc;
	sxu32 iOpt,nCapture = 0,*aOv;
	int iSyntax;
	pcre2_code *pCode;

	zPat = ph7_value_to_string(apArg[0],&nPat);
	if( nPat < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($pattern) must not be empty",
			bCase ? "mb_eregi" : "mb_ereg");
	}
	zSub = ph7_value_to_string(apArg[1],&nSub);
	if( MbReOptArg(pCtx,0,&iOpt,&iSyntax) != 0 ){
		return PH7_OK;
	}
	if( bCase ){
		iOpt |= MBRE_OPT_IGNORECASE;
	}
	pCode = MbReCompile(pCtx,zPat,nPat,iOpt,bCase ? "mb_eregi" : "mb_ereg",&nCapture);
	if( pCode == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	aOv = (sxu32 *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
		(nCapture + 1) * 2 * sizeof(sxu32));
	if( aOv == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	rc = MbReMatch(pCode,nCapture,zSub,nSub,0,aOv,&nGroup);
	if( nArg > 2 ){
		ph7_value *pArray = ph7_context_new_array(pCtx);
		if( rc > 0 ){
			MbRePopulate(pCtx,pArray,zSub,aOv,nGroup,pCode);
		}
		PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pArray);
		ph7_context_release_value(pCtx,pArray);
	}
	SyMemBackendFree(&pCtx->pVm->sAllocator,aOv);
	ph7_result_bool(pCtx,rc > 0);
	return PH7_OK;
}
static int PH7_builtin_mb_ereg(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return MbEregCommon(pCtx,nArg,apArg,0);
}
static int PH7_builtin_mb_eregi(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return MbEregCommon(pCtx,nArg,apArg,1);
}
/* mb_ereg_match: the pattern is anchored at the START of the subject, and only
 * there -- it does NOT have to reach the end. */
static int PH7_builtin_mb_ereg_match(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPat,*zSub;
	int nPat,nSub,iSyntax,nGroup = 0,rc;
	sxu32 iOpt,nCapture = 0,*aOv;
	pcre2_code *pCode;

	zPat = ph7_value_to_string(apArg[0],&nPat);
	zSub = ph7_value_to_string(apArg[1],&nSub);
	if( MbReOptArg(pCtx,nArg > 2 ? apArg[2] : 0,&iOpt,&iSyntax) != 0 ){
		return PH7_OK;
	}
	pCode = MbReCompile(pCtx,zPat,nPat,iOpt,"mb_ereg_match",&nCapture);
	if( pCode == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	aOv = (sxu32 *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
		(nCapture + 1) * 2 * sizeof(sxu32));
	if( aOv == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	rc = MbReMatch(pCode,nCapture,zSub,nSub,0,aOv,&nGroup);
	/* Anchored: a match that did not begin at offset 0 is not one. */
	ph7_result_bool(pCtx,rc > 0 && aOv[0] == 0);
	SyMemBackendFree(&pCtx->pVm->sAllocator,aOv);
	return PH7_OK;
}
/* Walk a subject one byte-character at a time -- used to step past a zero-width
 * match without splitting a UTF-8 sequence down the middle. */
static sxu32 MbReStep(const char *z,int n,sxu32 i,int bUtf8)
{
	sxu32 k = 1;
	if( bUtf8 && i < (sxu32)n ){
		unsigned char c = (unsigned char)z[i];
		if( c >= 0xF0 ){ k = 4; }
		else if( c >= 0xE0 ){ k = 3; }
		else if( c >= 0xC0 ){ k = 2; }
	}
	return i + k;
}
/* mb_split. A zero-width match does not split -- an empty pattern answers the
 * whole subject as one piece. $limit caps the number of PIECES: the last one
 * holds everything that is left, and a limit below 1 is the whole subject. */
static int PH7_builtin_mb_split(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPat,*zSub;
	int nPat,nSub,iSyntax,nGroup = 0,nPiece = 0,iLimit = -1;
	sxu32 iOpt,nCapture = 0,*aOv,iStart = 0,iFrom = 0;
	pcre2_code *pCode;
	ph7_value *pArray,*pVal;
	int bUtf8;

	zPat = ph7_value_to_string(apArg[0],&nPat);
	zSub = ph7_value_to_string(apArg[1],&nSub);
	if( nArg > 2 ){
		iLimit = ph7_value_to_int(apArg[2]);
		if( iLimit == 0 ){
			/* php reads a zero limit as one piece -- the whole subject. Only a
			 * NEGATIVE limit is "no limit". */
			iLimit = 1;
		}
	}
	if( MbReOptArg(pCtx,0,&iOpt,&iSyntax) != 0 ){
		return PH7_OK;
	}
	pCode = MbReCompile(pCtx,zPat,nPat,iOpt,"mb_split",&nCapture);
	if( pCode == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	aOv = (sxu32 *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
		(nCapture + 1) * 2 * sizeof(sxu32));
	if( aOv == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	bUtf8 = PH7_MbEncodingIsUtf8(pCtx->pVm->iMbReEnc);
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	while( iStart <= (sxu32)nSub ){
		if( iLimit > 0 && nPiece + 1 >= iLimit ){
			break;
		}
		if( MbReMatch(pCode,nCapture,zSub,nSub,iStart,aOv,&nGroup) <= 0 ){
			break;
		}
		if( aOv[1] == aOv[0] ){
			/* Zero-width: no split here, step over one character and retry. */
			iStart = MbReStep(zSub,nSub,aOv[0],bUtf8);
			if( iStart > (sxu32)nSub ){
				break;
			}
			continue;
		}
		ph7_value_string(pVal,&zSub[iFrom],(int)(aOv[0] - iFrom));
		ph7_array_add_elem(pArray,0,pVal);
		ph7_value_reset_string_cursor(pVal);
		nPiece++;
		iFrom = iStart = aOv[1];
	}
	ph7_value_string(pVal,&zSub[iFrom],nSub - (int)iFrom);
	ph7_array_add_elem(pArray,0,pVal);
	ph7_context_release_value(pCtx,pVal);
	SyMemBackendFree(&pCtx->pVm->sAllocator,aOv);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* The replacement string's own grammar: `\` followed by a digit names a group,
 * and a group the pattern does not declare -- or any other byte after the
 * backslash -- is copied through with the backslash still attached. There is no
 * `$1` here and no way to escape a `\1`. */
static void MbReExpand(
	SyBlob *pOut,
	const char *zRepl,int nRepl,
	const char *zSub,const sxu32 *aOv,int nGroup)
{
	int i;
	for( i = 0 ; i < nRepl ; ++i ){
		if( zRepl[i] == '\\' && i + 1 < nRepl
			&& zRepl[i+1] >= '0' && zRepl[i+1] <= '9' ){
			int iNum = zRepl[i+1] - '0';
			if( iNum < nGroup ){
				if( aOv[2*iNum] != SXU32_HIGH ){
					SyBlobAppend(pOut,&zSub[aOv[2*iNum]],aOv[2*iNum+1] - aOv[2*iNum]);
				}
				i++;
				continue;
			}
		}
		SyBlobAppend(pOut,&zRepl[i],1);
	}
}
/* mb_ereg_replace / mb_eregi_replace / mb_ereg_replace_callback */
static int MbEregReplaceCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,
	int bCase,int bCallback)
{
	const char *zPat,*zSub,*zRepl = 0;
	const char *zFunc = bCallback ? "mb_ereg_replace_callback"
		: (bCase ? "mb_eregi_replace" : "mb_ereg_replace");
	int nPat,nSub,nRepl = 0,iSyntax,nGroup = 0;
	sxu32 iOpt,nCapture = 0,*aOv,iStart = 0,iFrom = 0;
	pcre2_code *pCode;
	SyBlob sOut;
	int bUtf8;
	sxi32 rcCb = SXRET_OK;

	zPat = ph7_value_to_string(apArg[0],&nPat);
	if( !bCallback ){
		zRepl = ph7_value_to_string(apArg[1],&nRepl);
	}
	zSub = ph7_value_to_string(apArg[2],&nSub);
	if( MbReOptArg(pCtx,nArg > 3 ? apArg[3] : 0,&iOpt,&iSyntax) != 0 ){
		return PH7_OK;
	}
	if( bCase ){
		iOpt |= MBRE_OPT_IGNORECASE;
	}
	pCode = MbReCompile(pCtx,zPat,nPat,iOpt,zFunc,&nCapture);
	if( pCode == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	aOv = (sxu32 *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
		(nCapture + 1) * 2 * sizeof(sxu32));
	if( aOv == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	bUtf8 = PH7_MbEncodingIsUtf8(pCtx->pVm->iMbReEnc);
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	while( iStart <= (sxu32)nSub ){
		if( MbReMatch(pCode,nCapture,zSub,nSub,iStart,aOv,&nGroup) <= 0 ){
			break;
		}
		if( aOv[0] > iFrom ){
			SyBlobAppend(&sOut,&zSub[iFrom],aOv[0] - iFrom);
		}
		if( bCallback ){
			ph7_value *pMatchArr = ph7_context_new_array(pCtx);
			ph7_value *apCbArg[1];
			ph7_value sResult;
			const char *zCb;
			int nCb;
			MbRePopulate(pCtx,pMatchArr,zSub,aOv,nGroup,pCode);
			PH7_MemObjInit(pCtx->pVm,&sResult);
			apCbArg[0] = pMatchArr;
			rcCb = PH7_VmCallCallbackByValue(pCtx->pVm,apArg[1],1,apCbArg,&sResult,0);
			if( PH7_CALLBACK_UNWOUND(rcCb) ){
				PH7_MemObjRelease(&sResult);
				ph7_context_release_value(pCtx,pMatchArr);
				SyBlobRelease(&sOut);
				SyMemBackendFree(&pCtx->pVm->sAllocator,aOv);
				return rcCb;
			}
			zCb = ph7_value_to_string(&sResult,&nCb);
			SyBlobAppend(&sOut,zCb,(sxu32)nCb);
			PH7_MemObjRelease(&sResult);
			ph7_context_release_value(pCtx,pMatchArr);
		}else{
			MbReExpand(&sOut,zRepl,nRepl,zSub,aOv,nGroup);
		}
		iFrom = aOv[1];
		if( aOv[1] == aOv[0] ){
			/* A zero-width match: carry the character it sat on across and step
			 * past it, or the scan never moves. */
			sxu32 iNext = MbReStep(zSub,nSub,aOv[0],bUtf8);
			if( iNext > (sxu32)nSub ){
				iStart = iNext;
				break;
			}
			SyBlobAppend(&sOut,&zSub[aOv[0]],iNext - aOv[0]);
			iFrom = iStart = iNext;
		}else{
			iStart = aOv[1];
		}
	}
	if( iFrom < (sxu32)nSub ){
		SyBlobAppend(&sOut,&zSub[iFrom],(sxu32)nSub - iFrom);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	SyMemBackendFree(&pCtx->pVm->sAllocator,aOv);
	return PH7_OK;
}
static int PH7_builtin_mb_ereg_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return MbEregReplaceCommon(pCtx,nArg,apArg,0,0);
}
static int PH7_builtin_mb_eregi_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return MbEregReplaceCommon(pCtx,nArg,apArg,1,0);
}
static int PH7_builtin_mb_ereg_replace_callback(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return MbEregReplaceCommon(pCtx,nArg,apArg,0,1);
}
/* ----- the stateful search ----------------------------------------------
 * mb_ereg_search_init() parks a subject and (optionally) a pattern; each of
 * mb_ereg_search(), _pos() and _regs() runs ONE match from the cursor and
 * leaves it just past what matched, while _getregs(), _getpos() and _setpos()
 * only read and write the state. A call with no subject parked is an Error,
 * and so is one with no pattern -- php checks the pattern first.
 * -------------------------------------------------------------------- */
static void MbReSearchDropRegs(ph7_vm *pVm)
{
	if( pVm->aMbReOv ){
		SyMemBackendFree(&pVm->sAllocator,pVm->aMbReOv);
		pVm->aMbReOv = 0;
	}
	pVm->nMbReOv = 0;
}
static int MbReSearchSetPattern(ph7_context *pCtx,const char *zPat,int nPat,
	sxu32 iOpt,int iSyntax)
{
	ph7_vm *pVm = pCtx->pVm;
	char *zCopy = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nPat + 1);
	if( zCopy == 0 ){
		return -1;
	}
	SyMemcpy(zPat,zCopy,(sxu32)nPat);
	zCopy[nPat] = 0;
	if( pVm->zMbRePat ){
		SyMemBackendFree(&pVm->sAllocator,pVm->zMbRePat);
	}
	pVm->zMbRePat = zCopy;
	pVm->nMbRePat = (sxu32)nPat;
	pVm->iMbReOptCur = iOpt;
	pVm->iMbReSynCur = (sxu8)iSyntax;
	return 0;
}
static int PH7_builtin_mb_ereg_search_init(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zStr;
	int nStr,iSyntax;
	sxu32 iOpt;
	char *zCopy;

	zStr = ph7_value_to_string(apArg[0],&nStr);
	if( MbReOptArg(pCtx,nArg > 2 ? apArg[2] : 0,&iOpt,&iSyntax) != 0 ){
		return PH7_OK;
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		const char *zPat;
		int nPat;
		zPat = ph7_value_to_string(apArg[1],&nPat);
		if( MbReSearchSetPattern(pCtx,zPat,nPat,iOpt,iSyntax) != 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	zCopy = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nStr + 1);
	if( zCopy == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyMemcpy(zStr,zCopy,(sxu32)nStr);
	zCopy[nStr] = 0;
	if( pVm->zMbReStr ){
		SyMemBackendFree(&pVm->sAllocator,pVm->zMbReStr);
	}
	pVm->zMbReStr = zCopy;
	pVm->nMbReStr = (sxu32)nStr;
	pVm->iMbRePos = 0;
	MbReSearchDropRegs(pVm);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* The one step behind mb_ereg_search(), _pos() and _regs(). Answers 1 on a
 * match (the state now holds its offsets), 0 on no match, and -1 when it
 * already threw. */
static int MbReSearchStep(ph7_context *pCtx,int nArg,ph7_value **apArg,
	const char *zFunc,pcre2_code **ppCode)
{
	ph7_vm *pVm = pCtx->pVm;
	int iSyntax,nGroup = 0,rc;
	sxu32 iOpt,nCapture = 0,*aOv;
	pcre2_code *pCode;

	if( MbReOptArg(pCtx,nArg > 1 ? apArg[1] : 0,&iOpt,&iSyntax) != 0 ){
		return -1;
	}
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		const char *zPat;
		int nPat;
		zPat = ph7_value_to_string(apArg[0],&nPat);
		if( MbReSearchSetPattern(pCtx,zPat,nPat,iOpt,iSyntax) != 0 ){
			return -1;
		}
	}
	if( pVm->zMbRePat == 0 ){
		PH7_VmThrowException(pCtx,"Error","No pattern was provided");
		return -1;
	}
	if( pVm->zMbReStr == 0 ){
		PH7_VmThrowException(pCtx,"Error","No string was provided");
		return -1;
	}
	pCode = MbReCompile(pCtx,pVm->zMbRePat,(int)pVm->nMbRePat,pVm->iMbReOptCur,
		zFunc,&nCapture);
	if( pCode == 0 ){
		return 0;
	}
	MbReSearchDropRegs(pVm);
	if( pVm->iMbRePos > pVm->nMbReStr ){
		return 0;
	}
	aOv = (sxu32 *)SyMemBackendAlloc(&pVm->sAllocator,
		(nCapture + 1) * 2 * sizeof(sxu32));
	if( aOv == 0 ){
		return 0;
	}
	rc = MbReMatch(pCode,nCapture,pVm->zMbReStr,(int)pVm->nMbReStr,pVm->iMbRePos,
		aOv,&nGroup);
	if( rc <= 0 ){
		SyMemBackendFree(&pVm->sAllocator,aOv);
		return 0;
	}
	pVm->aMbReOv = aOv;
	pVm->nMbReOv = nGroup;
	pVm->iMbRePos = aOv[1];
	*ppCode = pCode;
	return 1;
}
static int PH7_builtin_mb_ereg_search(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	pcre2_code *pCode = 0;
	int rc = MbReSearchStep(pCtx,nArg,apArg,"mb_ereg_search",&pCode);
	if( rc < 0 ){
		return PH7_OK;
	}
	ph7_result_bool(pCtx,rc > 0);
	return PH7_OK;
}
static int PH7_builtin_mb_ereg_search_pos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	pcre2_code *pCode = 0;
	ph7_value *pArray,*pVal;
	int rc = MbReSearchStep(pCtx,nArg,apArg,"mb_ereg_search_pos",&pCode);
	if( rc < 0 ){
		return PH7_OK;
	}
	if( rc == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	ph7_value_int(pVal,(int)pCtx->pVm->aMbReOv[0]);
	ph7_array_add_intkey_elem(pArray,0,pVal);
	ph7_value_int(pVal,(int)(pCtx->pVm->aMbReOv[1] - pCtx->pVm->aMbReOv[0]));
	ph7_array_add_intkey_elem(pArray,1,pVal);
	ph7_context_release_value(pCtx,pVal);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
static int PH7_builtin_mb_ereg_search_regs(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	pcre2_code *pCode = 0;
	ph7_value *pArray;
	int rc = MbReSearchStep(pCtx,nArg,apArg,"mb_ereg_search_regs",&pCode);
	if( rc < 0 ){
		return PH7_OK;
	}
	if( rc == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	MbRePopulate(pCtx,pArray,pCtx->pVm->zMbReStr,pCtx->pVm->aMbReOv,
		pCtx->pVm->nMbReOv,pCode);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
static int PH7_builtin_mb_ereg_search_getregs(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pArray;
	pcre2_code *pCode;
	sxu32 nCapture = 0;
	SXUNUSED(nArg); SXUNUSED(apArg);

	if( pVm->nMbReOv < 1 || pVm->zMbReStr == 0 || pVm->zMbRePat == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pCode = MbReCompile(pCtx,pVm->zMbRePat,(int)pVm->nMbRePat,pVm->iMbReOptCur,
		"mb_ereg_search_getregs",&nCapture);
	if( pCode == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	MbRePopulate(pCtx,pArray,pVm->zMbReStr,pVm->aMbReOv,pVm->nMbReOv,pCode);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
static int PH7_builtin_mb_ereg_search_getpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int(pCtx,(int)pCtx->pVm->iMbRePos);
	return PH7_OK;
}
static int PH7_builtin_mb_ereg_search_setpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxi64 iOff;
	SXUNUSED(nArg);

	iOff = ph7_value_to_int64(apArg[0]);
	/* php measures the offset against the subject that is parked -- and with
	 * NONE parked there is nothing to measure against, so any non-negative
	 * offset is taken and the next mb_ereg_search_init() resets it anyway. */
	if( iOff < 0 || (pVm->zMbReStr != 0 && iOff > (sxi64)pVm->nMbReStr) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"mb_ereg_search_setpos(): Argument #1 ($offset) is out of range");
	}
	pVm->iMbRePos = (sxu32)iOff;
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int PH7_builtin_mb_regex_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zEnc;
	int nEnc,iName;

	if( nArg < 1 || ph7_value_is_null(apArg[0]) ){
		zEnc = PH7_MbEncodingCanonical(pVm->iMbReEnc);
		ph7_result_string(pCtx,zEnc,-1);
		return PH7_OK;
	}
	zEnc = ph7_value_to_string(apArg[0],&nEnc);
	iName = PH7_MbEncodingLookup(zEnc,nEnc);
	if( iName < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"mb_regex_encoding(): Argument #1 ($encoding) must be a valid encoding, "
			"\"%.*s\" given",nEnc,zEnc);
	}
	pVm->iMbReEnc = iName;
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int PH7_builtin_mb_regex_set_options(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	char zBuf[16];
	int nBuf;
	const char *zOpt;
	int nOpt,iSyntax;
	sxu32 iOpt = 0;
	unsigned char cBad = 0;

	/* The answer is always the option string as it stood BEFORE this call. */
	nBuf = MbReOptString(MbReOptOf(pVm),MbReSyntaxOf(pVm),zBuf);
	if( nArg < 1 || ph7_value_is_null(apArg[0]) ){
		ph7_result_string(pCtx,zBuf,nBuf);
		return PH7_OK;
	}
	iSyntax = MbReSyntaxOf(pVm);
	zOpt = ph7_value_to_string(apArg[0],&nOpt);
	if( MbReParseOpt(zOpt,nOpt,&iOpt,&iSyntax,&cBad) != 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Option \"%c\" is not supported",(int)cBad);
	}
	pVm->iMbReOpt = iOpt | MBRE_OPT_SET;
	pVm->iMbReSyntax = (sxu8)iSyntax;
	ph7_result_string(pCtx,zBuf,nBuf);
	return PH7_OK;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */

static const ph7_builtin_func aPcreFunc[] = {
	{ "preg_match",              PH7_builtin_preg_match },
	{ "preg_match_all",          PH7_builtin_preg_match_all },
	{ "preg_replace",            PH7_builtin_preg_replace },
	{ "preg_filter",             PH7_builtin_preg_filter },
	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },
	{ "preg_replace_callback_array", PH7_builtin_preg_replace_callback_array },
	{ "preg_grep",               PH7_builtin_preg_grep },
	{ "preg_split",              PH7_builtin_preg_split },
	{ "preg_quote",              PH7_builtin_preg_quote },
	{ "preg_last_error",         PH7_builtin_preg_last_error },
	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },
#ifndef PH7_DISABLE_BUILTIN_FUNC
	{ "mb_ereg",                 PH7_builtin_mb_ereg },
	{ "mb_eregi",                PH7_builtin_mb_eregi },
	{ "mb_ereg_match",           PH7_builtin_mb_ereg_match },
	{ "mb_ereg_replace",         PH7_builtin_mb_ereg_replace },
	{ "mb_eregi_replace",        PH7_builtin_mb_eregi_replace },
	{ "mb_ereg_replace_callback",PH7_builtin_mb_ereg_replace_callback },
	{ "mb_split",                PH7_builtin_mb_split },
	{ "mb_ereg_search_init",     PH7_builtin_mb_ereg_search_init },
	{ "mb_ereg_search",          PH7_builtin_mb_ereg_search },
	{ "mb_ereg_search_pos",      PH7_builtin_mb_ereg_search_pos },
	{ "mb_ereg_search_regs",     PH7_builtin_mb_ereg_search_regs },
	{ "mb_ereg_search_getregs",  PH7_builtin_mb_ereg_search_getregs },
	{ "mb_ereg_search_getpos",   PH7_builtin_mb_ereg_search_getpos },
	{ "mb_ereg_search_setpos",   PH7_builtin_mb_ereg_search_setpos },
	{ "mb_regex_encoding",       PH7_builtin_mb_regex_encoding },
	{ "mb_regex_set_options",    PH7_builtin_mb_regex_set_options },
#endif
};

PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0; n < SX_ARRAYSIZE(aPcreFunc); n++ ){
		ph7_create_function(&(*pVm), aPcreFunc[n].zName, aPcreFunc[n].xFunc, 0);
	}
}

/* ===== Constant registration ===== */
#define PCRE_CONST_INT(name, val) \
	static void PcreConst_##name(ph7_value *pVal, void *pUnused){ \
		SXUNUSED(pUnused); ph7_value_int(pVal, val); \
	}

PCRE_CONST_INT(PREG_PATTERN_ORDER,       PHP_PREG_PATTERN_ORDER)
PCRE_CONST_INT(PREG_SET_ORDER,           PHP_PREG_SET_ORDER)
PCRE_CONST_INT(PREG_OFFSET_CAPTURE,      PHP_PREG_OFFSET_CAPTURE)
PCRE_CONST_INT(PREG_UNMATCHED_AS_NULL,   PHP_PREG_UNMATCHED_AS_NULL)
PCRE_CONST_INT(PREG_SPLIT_NO_EMPTY,      PHP_PREG_SPLIT_NO_EMPTY)
PCRE_CONST_INT(PREG_SPLIT_DELIM_CAPTURE, PHP_PREG_SPLIT_DELIM_CAPTURE)
PCRE_CONST_INT(PREG_SPLIT_OFFSET_CAPTURE,PHP_PREG_SPLIT_OFFSET_CAPTURE)
PCRE_CONST_INT(PREG_NO_ERROR,            PHP_PREG_NO_ERROR)
PCRE_CONST_INT(PREG_INTERNAL_ERROR,      PHP_PREG_INTERNAL_ERROR)
PCRE_CONST_INT(PREG_BACKTRACK_LIMIT_ERROR,PHP_PREG_BACKTRACK_LIMIT_ERROR)
PCRE_CONST_INT(PREG_RECURSION_LIMIT_ERROR,PHP_PREG_RECURSION_LIMIT_ERROR)
PCRE_CONST_INT(PREG_GREP_INVERT,         PHP_PREG_GREP_INVERT)
PCRE_CONST_INT(PREG_BAD_UTF8_ERROR,      PHP_PREG_BAD_UTF8_ERROR)
PCRE_CONST_INT(PREG_BAD_UTF8_OFFSET_ERROR,PHP_PREG_BAD_UTF8_OFFSET_ERROR)
PCRE_CONST_INT(PREG_JIT_STACKLIMIT_ERROR,PHP_PREG_JIT_STACKLIMIT_ERROR)

PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm)
{
	ph7_create_constant(&(*pVm), "PREG_PATTERN_ORDER",        PcreConst_PREG_PATTERN_ORDER, 0);
	ph7_create_constant(&(*pVm), "PREG_SET_ORDER",            PcreConst_PREG_SET_ORDER, 0);
	ph7_create_constant(&(*pVm), "PREG_OFFSET_CAPTURE",       PcreConst_PREG_OFFSET_CAPTURE, 0);
	ph7_create_constant(&(*pVm), "PREG_UNMATCHED_AS_NULL",    PcreConst_PREG_UNMATCHED_AS_NULL, 0);
	ph7_create_constant(&(*pVm), "PREG_SPLIT_NO_EMPTY",       PcreConst_PREG_SPLIT_NO_EMPTY, 0);
	ph7_create_constant(&(*pVm), "PREG_GREP_INVERT",          PcreConst_PREG_GREP_INVERT, 0);
	ph7_create_constant(&(*pVm), "PREG_SPLIT_DELIM_CAPTURE",  PcreConst_PREG_SPLIT_DELIM_CAPTURE, 0);
	ph7_create_constant(&(*pVm), "PREG_SPLIT_OFFSET_CAPTURE", PcreConst_PREG_SPLIT_OFFSET_CAPTURE, 0);
	ph7_create_constant(&(*pVm), "PREG_NO_ERROR",             PcreConst_PREG_NO_ERROR, 0);
	ph7_create_constant(&(*pVm), "PREG_INTERNAL_ERROR",       PcreConst_PREG_INTERNAL_ERROR, 0);
	ph7_create_constant(&(*pVm), "PREG_BACKTRACK_LIMIT_ERROR", PcreConst_PREG_BACKTRACK_LIMIT_ERROR, 0);
	ph7_create_constant(&(*pVm), "PREG_RECURSION_LIMIT_ERROR", PcreConst_PREG_RECURSION_LIMIT_ERROR, 0);
	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_ERROR",       PcreConst_PREG_BAD_UTF8_ERROR, 0);
	ph7_create_constant(&(*pVm), "PREG_BAD_UTF8_OFFSET_ERROR",PcreConst_PREG_BAD_UTF8_OFFSET_ERROR, 0);
	ph7_create_constant(&(*pVm), "PREG_JIT_STACKLIMIT_ERROR", PcreConst_PREG_JIT_STACKLIMIT_ERROR, 0);
}

/*
 * What the PCRE2 this build is LINKED AGAINST says about itself. php publishes the
 * same four as constants and Composer reads PCRE_VERSION on startup (it records
 * the platform's pcre in the lock file's platform requirements), so a missing one
 * stops it before it can load a repository.
 *
 * Asked of the library rather than of its headers: the numbers differ per platform
 * and per build, and pinning a header's idea of them would report a version this
 * binary is not running.
 */
PH7_PRIVATE void PH7_PcreVersionInfo(char *zBuf,int nBuf,int *pMajor,int *pMinor,int *pJit)
{
	int n;
	if( zBuf && nBuf > 0 ){
		zBuf[0] = 0;
		n = pcre2_config(PCRE2_CONFIG_VERSION,zBuf);
		if( n < 0 || n > nBuf ){
			zBuf[0] = 0;
		}
	}
	if( pMajor || pMinor ){
		/* The version string opens `MAJOR.MINOR ` -- php reads its own two numbers
		 * the same way (its macros come from the same string). */
		int iMaj = 0,iMin = 0;
		const char *z = zBuf;
		while( z && *z >= '0' && *z <= '9' ){ iMaj = iMaj*10 + (*z - '0'); z++; }
		if( z && *z == '.' ){
			z++;
			while( *z >= '0' && *z <= '9' ){ iMin = iMin*10 + (*z - '0'); z++; }
		}
		if( pMajor ){ *pMajor = iMaj; }
		if( pMinor ){ *pMinor = iMin; }
	}
	if( pJit ){
		sxu32 nJit = 0;
		*pJit = (pcre2_config(PCRE2_CONFIG_JIT,&nJit) == 0 && nJit != 0) ? 1 : 0;
	}
}

#else
/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */
typedef int vm_pcre_unused;
#endif /* PH7_ENABLE_PCRE */
