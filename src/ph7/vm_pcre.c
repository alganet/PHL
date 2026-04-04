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

/* ===== Delimiter parser ===== */
#define PCRE_PARSE_OK             0
#define PCRE_PARSE_EMPTY          1  /* Empty pattern string */
#define PCRE_PARSE_BAD_DELIMITER  2  /* Alphanumeric, backslash, or whitespace delimiter */
#define PCRE_PARSE_NO_ENDING      3  /* No closing delimiter found */

static sxi32 PcreParsePattern(
	const char *zInput, int nInputLen,
	const char **pPattern, int *pnPatternLen,
	const char **pFlags, int *pnFlagLen)
{
	const char *zEnd = &zInput[nInputLen];
	const char *z = zInput;
	char cOpen, cClose;
	const char *pStart;

	/* Skip leading whitespace */
	while( z < zEnd && (unsigned char)*z <= 0x20 ){
		z++;
	}
	if( z >= zEnd ){
		return PCRE_PARSE_EMPTY;
	}
	cOpen = *z;
	/* Must not be alphanumeric, backslash, or whitespace */
	if( SyisAlphaNum(cOpen) || cOpen == '\\' || (unsigned char)cOpen <= 0x20 ){
		return PCRE_PARSE_BAD_DELIMITER;
	}
	/* Paired delimiters */
	switch( cOpen ){
		case '(': cClose = ')'; break;
		case '[': cClose = ']'; break;
		case '{': cClose = '}'; break;
		case '<': cClose = '>'; break;
		default:  cClose = cOpen; break;
	}
	z++; /* Skip opening delimiter */
	pStart = z;
	/* Scan for closing delimiter, respecting backslash escapes */
	while( z < zEnd ){
		if( *z == '\\' && z + 1 < zEnd ){
			z += 2; /* Skip escaped char */
			continue;
		}
		if( *z == cClose ){
			break;
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

/* ===== Flag mapper ===== */
static sxi32 PcreMapFlags(
	const char *zFlags, int nFlagLen,
	uint32_t *pCompileOpts)
{
	int i;
	*pCompileOpts = 0;
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
			case 'S': /* Study hint — no-op in PCRE2 */ break;
			default: break;
		}
	}
	return PH7_OK;
}

/* ===== Compile helper ===== */
static pcre2_code *PcreCompile(
	ph7_context *pCtx,
	const char *zFullPattern, int nLen,
	sxu32 *pCaptureCount)
{
	const char *zPat, *zFlags;
	int nPatLen, nFlagLen;
	uint32_t compileOpts;
	pcre2_code *pCode;
	PCRE2_SIZE erroffset;
	int errcode;
	sxu32 nCapture;
	sxi32 parseRc;

	/* Check cache first */
	pCode = PcreCache_Find(zFullPattern, (sxu32)nLen, pCaptureCount);
	if( pCode ){
		return pCode;
	}
	/* Parse delimiter */
	parseRc = PcreParsePattern(zFullPattern, nLen, &zPat, &nPatLen, &zFlags, &nFlagLen);
	if( parseRc != PCRE_PARSE_OK ){
		const char *zMsg;
		switch( parseRc ){
			case PCRE_PARSE_EMPTY:         zMsg = "Empty regular expression"; break;
			case PCRE_PARSE_BAD_DELIMITER: zMsg = "Delimiter must not be alphanumeric, backslash, or whitespace"; break;
			default:                       zMsg = "No ending delimiter found"; break;
		}
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, zMsg);
		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;
		return 0;
	}
	/* Map flags */
	PcreMapFlags(zFlags, nFlagLen, &compileOpts);
	/* Compile */
	pCode = pcre2_compile(
		(PCRE2_SPTR)zPat, (PCRE2_SIZE)nPatLen,
		compileOpts, &errcode, &erroffset, NULL);
	if( pCode == 0 ){
		PCRE2_UCHAR errbuf[256];
		pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));
		ph7_context_throw_error_format(pCtx, PH7_CTX_WARNING,
			"Compilation failed: %s at offset %d", (const char *)errbuf, (int)erroffset);
		pCtx->pVm->iPcreLastError = PHP_PREG_INTERNAL_ERROR;
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
	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;
	return pCode;
}

/*
 * Write a value back to the caller's variable through the stack slot's nIdx.
 *
 * PHL limitation: if the caller passes an undefined variable (e.g. bare $m
 * without prior assignment), nIdx is SXU32_HIGH and the write-back is a
 * no-op — the caller's variable stays null.  Workaround: initialize the
 * variable before the call ($m = null;).  PHP does not have this limitation
 * because it creates the variable via the & reference mechanism.
 */
static void PcreStoreByRef(ph7_vm *pVm, ph7_value *pArg, ph7_value *pNewVal)
{
	if( pArg->nIdx != SXU32_HIGH ){
		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj, pArg->nIdx);
		if( pObj ){
			PH7_MemObjStore(pNewVal, pObj);
		}
	}
	PH7_MemObjStore(pNewVal, pArg);
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
static void PcrePopulateMatches(
	ph7_context *pCtx,
	ph7_value *pArray,          /* Target array (apArg[2] or sub-array) */
	const char *zSubject,
	PCRE2_SIZE *ovector,
	int nGroups,
	pcre2_code *pCode,
	int iFlags)                 /* PREG_OFFSET_CAPTURE etc. */
{
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	ph7_value *pSub = 0;
	uint32_t namecount = 0, nameentrysize = 0;
	PCRE2_SPTR nametable = 0;
	int i;

	if( iFlags & PHP_PREG_OFFSET_CAPTURE ){
		pSub = ph7_context_new_array(pCtx);
	}
	for( i = 0; i < nGroups; i++ ){
		PCRE2_SIZE start = ovector[2 * i];
		PCRE2_SIZE end   = ovector[2 * i + 1];
		if( start == PCRE2_UNSET ){
			if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){
				ph7_value_null(pVal);
			}else{
				ph7_value_string(pVal, "", 0);
			}
			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){
				ph7_value *pOff = ph7_context_new_scalar(pCtx);
				ph7_array_add_intkey_elem(pSub, 0, pVal);
				ph7_value_int(pOff, -1);
				ph7_array_add_intkey_elem(pSub, 1, pOff);
				ph7_array_add_intkey_elem(pArray, i, pSub);
				/* Reset sub-array for reuse */
				ph7_context_release_value(pCtx, pOff);
				ph7_context_release_value(pCtx, pSub);
				pSub = ph7_context_new_array(pCtx);
			}else{
				ph7_array_add_intkey_elem(pArray, i, pVal);
			}
		}else{
			ph7_value_string(pVal, &zSubject[start], (int)(end - start));
			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){
				ph7_value *pOff = ph7_context_new_scalar(pCtx);
				ph7_array_add_intkey_elem(pSub, 0, pVal);
				ph7_value_int(pOff, (int)start);
				ph7_array_add_intkey_elem(pSub, 1, pOff);
				ph7_array_add_intkey_elem(pArray, i, pSub);
				ph7_context_release_value(pCtx, pOff);
				ph7_context_release_value(pCtx, pSub);
				pSub = ph7_context_new_array(pCtx);
			}else{
				ph7_array_add_intkey_elem(pArray, i, pVal);
			}
		}
		ph7_value_reset_string_cursor(pVal);
	}
	/* Named groups */
	pcre2_pattern_info(pCode, PCRE2_INFO_NAMECOUNT, &namecount);
	if( namecount > 0 ){
		pcre2_pattern_info(pCode, PCRE2_INFO_NAMETABLE, &nametable);
		pcre2_pattern_info(pCode, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize);
		for( i = 0; (uint32_t)i < namecount; i++ ){
			PCRE2_SPTR entry = nametable + i * nameentrysize;
			int groupNum = (entry[0] << 8) | entry[1];
			const char *zName = (const char *)(entry + 2);
			PCRE2_SIZE start, end;
			if( groupNum >= nGroups ) continue;
			start = ovector[2 * groupNum];
			end   = ovector[2 * groupNum + 1];
			if( start == PCRE2_UNSET ){
				if( iFlags & PHP_PREG_UNMATCHED_AS_NULL ){
					ph7_value_null(pVal);
				}else{
					ph7_value_string(pVal, "", 0);
				}
			}else{
				ph7_value_string(pVal, &zSubject[start], (int)(end - start));
			}
			if( iFlags & PHP_PREG_OFFSET_CAPTURE ){
				ph7_value *pOff = ph7_context_new_scalar(pCtx);
				ph7_array_add_intkey_elem(pSub, 0, pVal);
				ph7_value_int(pOff, start == PCRE2_UNSET ? -1 : (int)start);
				ph7_array_add_intkey_elem(pSub, 1, pOff);
				ph7_array_add_strkey_elem(pArray, zName, pSub);
				ph7_context_release_value(pCtx, pOff);
				ph7_context_release_value(pCtx, pSub);
				pSub = ph7_context_new_array(pCtx);
			}else{
				ph7_array_add_strkey_elem(pArray, zName, pVal);
			}
			ph7_value_reset_string_cursor(pVal);
		}
	}
	ph7_context_release_value(pCtx, pVal);
	if( pSub ){
		ph7_context_release_value(pCtx, pSub);
	}
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
			PcreStoreByRef(pCtx->pVm, apArg[2], pEmpty);
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
		PcrePopulateMatches(pCtx, pArray, zSubject, ovector, rc, pCode, iFlags);
		/* Write the array back to the caller's variable */
		PcreStoreByRef(pCtx->pVm, apArg[2], pArray);
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
		ph7_result_int(pCtx, 0);
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
		ph7_result_int(pCtx, 0);
		return PH7_OK;
	}
	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);
	if( pMatchData == 0 ){
		ph7_result_int(pCtx, 0);
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
					PcrePopulateMatches(pCtx, pSet, zSubject, ovector, rc, pCode, iFlags & ~0xFF);
					ph7_array_add_intkey_elem(pOutArray, totalMatches, pSet);
					ph7_context_release_value(pCtx, pSet);
				}
				if( ovector[1] == ovector[0] ){
					startOffset = ovector[0] + 1;
				}else{
					startOffset = ovector[1];
				}
				totalMatches++;
			}
		}else{
			/* PREG_PATTERN_ORDER (default) */
			ph7_value **apGroupArrays = 0;
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
					ph7_value *pVal = ph7_context_new_scalar(pCtx);
					int nActual = rc;
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
						}else{
							ph7_value_string(pVal, "", 0);
							ph7_array_add_elem(apGroupArrays[g], 0, pVal);
						}
						ph7_value_reset_string_cursor(pVal);
					}
					ph7_context_release_value(pCtx, pVal);
				}
				if( ovector[1] == ovector[0] ){
					startOffset = ovector[0] + 1;
				}else{
					startOffset = ovector[1];
				}
				totalMatches++;
			}
			if( apGroupArrays ){
				for( g = 0; g < nGroups; g++ ){
					ph7_array_add_intkey_elem(pOutArray, (int)g, apGroupArrays[g]);
					ph7_context_release_value(pCtx, apGroupArrays[g]);
				}
				ph7_context_free_chunk(pCtx, apGroupArrays);
			}
		}
		/* Write output array to caller's variable */
		if( pOutArray && nArg >= 3 ){
			PcreStoreByRef(pCtx->pVm, apArg[2], pOutArray);
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
				startOffset = matchEnd + 1;
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
			if( startOffset < (PCRE2_SIZE)nSubLen ){
				SyBlobAppend(pOut, &zSubject[startOffset], 1);
			}
			startOffset = ovector[0] + 1;
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

/* ======================================================================
 * preg_replace(pattern, replacement, subject [, limit [, &count]])
 * ====================================================================== */
static int PH7_builtin_preg_replace(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	int limit = -1;
	int count = 0;

	if( nArg < 3 ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
			"preg_replace() expects at least 3 parameters");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg >= 4 ){
		limit = ph7_value_to_int(apArg[3]);
	}
	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;

	/* Reject array subjects (not yet supported) */
	if( ph7_value_is_array(apArg[2]) ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
			"preg_replace(): Array subjects are not yet supported");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( !ph7_value_is_array(apArg[0]) ){
		/* Single pattern + single replacement on a string subject */
		const char *zPattern, *zRepl, *zSubject;
		int nPatLen, nReplLen, nSubLen;
		pcre2_code *pCode;
		sxu32 nCapture;
		SyBlob sOut;

		zPattern = ph7_value_to_string(apArg[0], &nPatLen);
		zRepl = ph7_value_to_string(apArg[1], &nReplLen);
		zSubject = ph7_value_to_string(apArg[2], &nSubLen);

		pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);
		if( pCode == 0 ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
		PcreDoReplace(pCtx, pCode, zSubject, nSubLen, zRepl, nReplLen, limit, &count, &sOut);
		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
		SyBlobRelease(&sOut);
	}else{
		/* TODO: array of patterns — iterate pairs and apply sequentially */
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
			"preg_replace() with array patterns is not yet supported");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Set &$count if provided */
	if( nArg >= 5 ){
		ph7_value sCount;
		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);
		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);
		PH7_MemObjRelease(&sCount);
	}
	return PH7_OK;
}

/* ======================================================================
 * preg_replace_callback(pattern, callback, subject [, limit [, &count]])
 * ====================================================================== */
static int PH7_builtin_preg_replace_callback(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zPattern, *zSubject;
	int nPatLen, nSubLen;
	pcre2_code *pCode;
	pcre2_match_data *pMatchData;
	sxu32 nCapture;
	SyBlob sOut;
	PCRE2_SIZE startOffset = 0;
	int limit = -1;
	int count = 0;
	int rc;

	if( nArg < 3 ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
			"preg_replace_callback() expects at least 3 parameters");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zPattern = ph7_value_to_string(apArg[0], &nPatLen);
	zSubject = ph7_value_to_string(apArg[2], &nSubLen);
	if( nArg >= 4 ){
		limit = ph7_value_to_int(apArg[3]);
	}
	if( !ph7_value_is_callable(apArg[1]) ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
			"preg_replace_callback() expects parameter 2 to be a valid callback");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pCode = PcreCompile(pCtx, zPattern, nPatLen, &nCapture);
	if( pCode == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pMatchData = pcre2_match_data_create_from_pattern(pCode, NULL);
	if( pMatchData == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
	pCtx->pVm->iPcreLastError = PHP_PREG_NO_ERROR;

	while( startOffset <= (PCRE2_SIZE)nSubLen ){
		PCRE2_SIZE *ovector;
		ph7_value *pMatchArr;
		ph7_value *apCbArg[1];
		ph7_value sResult;
		const char *zReplacement;
		int nReplLen;

		if( limit >= 0 && count >= limit ) break;
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
			SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(ovector[0] - startOffset));
		}
		/* Build matches array for callback */
		pMatchArr = ph7_context_new_array(pCtx);
		PcrePopulateMatches(pCtx, pMatchArr, zSubject, ovector, rc, pCode, 0);
		/* Call the callback */
		PH7_MemObjInit(pCtx->pVm, &sResult);
		apCbArg[0] = pMatchArr;
		PH7_VmCallUserFunction(pCtx->pVm, apArg[1], 1, apCbArg, &sResult);
		/* Get replacement string from callback result */
		zReplacement = ph7_value_to_string(&sResult, &nReplLen);
		SyBlobAppend(&sOut, zReplacement, (sxu32)nReplLen);
		PH7_MemObjRelease(&sResult);
		ph7_context_release_value(pCtx, pMatchArr);
		count++;
		/* Advance */
		if( ovector[1] == ovector[0] ){
			if( startOffset < (PCRE2_SIZE)nSubLen ){
				SyBlobAppend(&sOut, &zSubject[startOffset], 1);
			}
			startOffset = ovector[0] + 1;
		}else{
			startOffset = ovector[1];
		}
	}
	/* Copy remainder */
	if( startOffset < (PCRE2_SIZE)nSubLen ){
		SyBlobAppend(&sOut, &zSubject[startOffset], (sxu32)(nSubLen - startOffset));
	}
	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	pcre2_match_data_free(pMatchData);
	/* Set &$count if provided */
	if( nArg >= 5 ){
		ph7_value sCount;
		PH7_MemObjInitFromInt(pCtx->pVm, &sCount, count);
		PcreStoreByRef(pCtx->pVm, apArg[4], &sCount);
		PH7_MemObjRelease(&sCount);
	}
	return PH7_OK;
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
	z = zStr;
	zEnd = &zStr[nLen];
	while( z < zEnd ){
		char c = *z;
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

/* ===== Function registration table ===== */
static const ph7_builtin_func aPcreFunc[] = {
	{ "preg_match",              PH7_builtin_preg_match },
	{ "preg_match_all",          PH7_builtin_preg_match_all },
	{ "preg_replace",            PH7_builtin_preg_replace },
	{ "preg_replace_callback",   PH7_builtin_preg_replace_callback },
	{ "preg_split",              PH7_builtin_preg_split },
	{ "preg_quote",              PH7_builtin_preg_quote },
	{ "preg_last_error",         PH7_builtin_preg_last_error },
	{ "preg_last_error_msg",     PH7_builtin_preg_last_error_msg },
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

#else
/* Ensure non-empty translation unit when PCRE is disabled (MSVC C4206) */
typedef int vm_pcre_unused;
#endif /* PH7_ENABLE_PCRE */
