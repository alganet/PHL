/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * getopt() walks the REAL $argv array, element by element, exactly like php's
 * php_getopt(). The previous implementation scanned a flattened "arg1 arg2 …"
 * string (pVm->sArgv) with no notion of element boundaries, which made it
 * structurally unable to stop at the first non-option operand: every
 * whitespace-separated word that followed an option was swallowed as another of
 * that option's values (`--bee=2 rest1 rest2` answered `bee => [2, rest1,
 * rest2]` where php answers `bee => "2"` and reports index 3 as the rest). It
 * also had no clustering (`-abval`), no `--` terminator, and no `&$rest_index`.
 */

/* One declared option, from the short-option string or the long-option array. */
#define GETOPT_ARG_NONE     0  /* "x"   — never takes a value */
#define GETOPT_ARG_REQUIRED 1  /* "x:"  — takes the next word if not attached */
#define GETOPT_ARG_OPTIONAL 2  /* "x::" — attached values only */
typedef struct getopt_spec getopt_spec;
struct getopt_spec
{
	sxu32 nNameOfft;  /* Offset of the option name inside the name pool */
	sxu32 nNameLen;   /* Its length (1 for a short option) */
	int iArg;         /* GETOPT_ARG_* */
	int bLong;        /* TRUE for a --long option */
};
/* One $argv element, copied into the pool (the walker's values are transient). */
typedef struct getopt_word getopt_word;
struct getopt_word
{
	sxu32 nOfft;
	sxu32 nLen;
};
/* Collector state shared by the two ph7_array_walk() callbacks below. */
typedef struct getopt_collect getopt_collect;
struct getopt_collect
{
	SyBlob *pPool;   /* Byte pool the offsets point into */
	SySet *pOut;     /* SySet of getopt_word / getopt_spec */
	sxi32 rc;        /* SXRET_OK or SXERR_MEM */
};
/*
 * Append zIn[0..nLen) to the pool and return its offset. The pool can be
 * reallocated, so nothing may hold a raw pointer into it across an append —
 * every reference is an offset resolved through GetoptPoolAt().
 */
static sxi32 GetoptPoolPut(SyBlob *pPool,const char *zIn,sxu32 nLen,sxu32 *pnOfft)
{
	*pnOfft = SyBlobLength(pPool);
	if( nLen < 1 ){
		return SXRET_OK;
	}
	return SyBlobAppend(pPool,(const void *)zIn,nLen);
}
static const char * GetoptPoolAt(SyBlob *pPool,sxu32 nOfft)
{
	return &((const char *)SyBlobData(pPool))[nOfft];
}
/* ph7_array_walk() callback: copy one $argv element into the word list. */
static int GetoptArgvWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	getopt_collect *pCol = (getopt_collect *)pUserData;
	getopt_word sWord;
	const char *zIn;
	int nIn;
	zIn = ph7_value_to_string(pData,&nIn);
	if( nIn < 0 ){
		nIn = 0;
	}
	if( GetoptPoolPut(pCol->pPool,zIn,(sxu32)nIn,&sWord.nOfft) != SXRET_OK ){
		pCol->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	sWord.nLen = (sxu32)nIn;
	if( SySetPut(pCol->pOut,(const void *)&sWord) != SXRET_OK ){
		pCol->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	SXUNUSED(pKey);
	return PH7_OK;
}
/*
 * ph7_array_walk() callback: turn one $long_options element into a spec. php
 * ignores a non-string element, and reads a trailing ":"/"::" exactly like the
 * short-option string.
 */
static int GetoptLongWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	getopt_collect *pCol = (getopt_collect *)pUserData;
	getopt_spec sSpec;
	const char *zIn;
	int nIn;
	if( !ph7_value_is_string(pData) ){
		SXUNUSED(pKey);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(pData,&nIn);
	if( nIn < 1 ){
		return PH7_OK;
	}
	sSpec.iArg = GETOPT_ARG_NONE;
	if( nIn > 1 && zIn[nIn-1] == ':' ){
		sSpec.iArg = GETOPT_ARG_REQUIRED;
		nIn--;
		if( nIn > 1 && zIn[nIn-1] == ':' ){
			sSpec.iArg = GETOPT_ARG_OPTIONAL;
			nIn--;
		}
	}
	if( nIn < 1 ){
		return PH7_OK;
	}
	if( GetoptPoolPut(pCol->pPool,zIn,(sxu32)nIn,&sSpec.nNameOfft) != SXRET_OK ){
		pCol->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	sSpec.nNameLen = (sxu32)nIn;
	sSpec.bLong = 1;
	if( SySetPut(pCol->pOut,(const void *)&sSpec) != SXRET_OK ){
		pCol->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	return PH7_OK;
}
/* Find a declared option by name; bLong selects the -x / --xx namespace. */
static getopt_spec * GetoptFindSpec(SySet *pSpecs,SyBlob *pPool,const char *zName,sxu32 nName,int bLong)
{
	getopt_spec *aSpec = (getopt_spec *)SySetBasePtr(pSpecs);
	sxu32 n = SySetUsed(pSpecs);
	sxu32 i;
	for( i = 0 ; i < n ; ++i ){
		if( aSpec[i].bLong != bLong || aSpec[i].nNameLen != nName ){
			continue;
		}
		if( SyMemcmp(GetoptPoolAt(pPool,aSpec[i].nNameOfft),zName,nName) == 0 ){
			return &aSpec[i];
		}
	}
	return 0;
}
/*
 * Record one parsed option. php answers FALSE for a valueless option, the value
 * string otherwise, and — when the SAME option appears more than once — an array
 * of every occurrence in order (`-a -a` gives [false,false]).
 */
static sxi32 GetoptAddResult(
	ph7_context *pCtx,
	ph7_value *pArray,
	const char *zName,sxu32 nName,
	const char *zVal,sxu32 nVal,int bHasVal
	)
{
	ph7_value *pKey,*pVal,*pOld;
	sxi32 rc = SXRET_OK;
	pKey = ph7_context_new_scalar(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pKey == 0 || pVal == 0 ){
		return SXERR_MEM;
	}
	ph7_value_string(pKey,zName,(int)nName);
	if( bHasVal ){
		ph7_value_string(pVal,zVal,(int)nVal);
	}else{
		ph7_value_bool(pVal,0);
	}
	pOld = ph7_array_fetch(pArray,zName,(int)nName);
	if( pOld == 0 ){
		rc = ph7_array_add_elem(pArray,pKey,pVal);
	}else if( ph7_value_is_array(pOld) ){
		/* Third and later occurrence: append to the existing list. */
		rc = ph7_array_add_elem(pOld,0,pVal);
	}else{
		/* Second occurrence: promote the scalar to php's list form. */
		ph7_value *pList = ph7_context_new_array(pCtx);
		if( pList == 0 ){
			rc = SXERR_MEM;
		}else{
			rc = ph7_array_add_elem(pList,0,pOld);
			if( rc == SXRET_OK ){
				rc = ph7_array_add_elem(pList,0,pVal);
			}
			if( rc == SXRET_OK ){
				rc = ph7_array_add_elem(pArray,pKey,pList);
			}
			ph7_context_release_value(pCtx,pList);
		}
	}
	ph7_context_release_value(pCtx,pKey);
	ph7_context_release_value(pCtx,pVal);
	return rc;
}
/*
 * Write getopt()'s optional by-reference &$rest_index out-param: the $argv index
 * of the first argument that is NOT an option (php's optind).
 */
static void GetoptStoreRestIndex(ph7_context *pCtx,int nArg,ph7_value **apArg,sxi64 nRest)
{
	ph7_value sVal;
	if( nArg < 3 ){
		return;
	}
	PH7_MemObjInitFromInt(pCtx->pVm,&sVal,nRest);
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],&sVal);
	PH7_MemObjRelease(&sVal);
}
/*
 * array|false getopt(string $short_options[,array $long_options[,int &$rest_index]])
 *   Gets options from the command line argument list.
 * Parameters
 *  $short_options
 *   Each character is an option; a following ":" means it takes a value and
 *   "::" that the value is optional (attached forms only).
 *  $long_options
 *   Option names for the "--name" form, with the same ":"/"::" suffixes.
 *  &$rest_index
 *   Set to the $argv index where option parsing stopped.
 * Return
 *  An array of option => value pairs, or FALSE on failure.
 *
 * Parsing follows php: it walks $argv from index 1 and STOPS at the first
 * element that is not an option (including a bare "-" and the empty string),
 * consuming a "--" terminator. An unknown option is skipped, and a required
 * value that is nowhere to be found drops its option instead of inventing one.
 */
PH7_PRIVATE int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	getopt_collect sCol;
	getopt_word *aWord;
	ph7_value *pArgv,*pArray;
	SySet sSpecs,sWords;
	SyBlob sPool;
	const char *zIn;
	sxu32 nWord,iWord;
	int nByte;
	int i;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sPool,&pCtx->pVm->sAllocator);
	SySetInit(&sSpecs,&pCtx->pVm->sAllocator,sizeof(getopt_spec));
	SySetInit(&sWords,&pCtx->pVm->sAllocator,sizeof(getopt_word));
	SyZero(&sCol,sizeof(sCol));
	sCol.pPool = &sPool;
	/* Declared short options: an alphanumeric character, optionally ':'/'::'. */
	zIn = ph7_value_to_string(apArg[0],&nByte);
	for( i = 0 ; i < nByte ; ++i ){
		getopt_spec sSpec;
		char c = zIn[i];
		if( !SyisAlphaNum((unsigned char)c) ){
			continue;
		}
		sSpec.iArg = GETOPT_ARG_NONE;
		if( i + 1 < nByte && zIn[i+1] == ':' ){
			sSpec.iArg = GETOPT_ARG_REQUIRED;
			i++;
			if( i + 1 < nByte && zIn[i+1] == ':' ){
				sSpec.iArg = GETOPT_ARG_OPTIONAL;
				i++;
			}
		}
		if( GetoptPoolPut(&sPool,&c,sizeof(char),&sSpec.nNameOfft) != SXRET_OK ){
			sCol.rc = SXERR_MEM;
			break;
		}
		sSpec.nNameLen = 1;
		sSpec.bLong = 0;
		if( SySetPut(&sSpecs,(const void *)&sSpec) != SXRET_OK ){
			sCol.rc = SXERR_MEM;
			break;
		}
	}
	/* Declared long options. */
	if( sCol.rc == SXRET_OK && nArg > 1 && ph7_value_is_array(apArg[1]) ){
		sCol.pOut = &sSpecs;
		ph7_array_walk(apArg[1],GetoptLongWalker,&sCol);
	}
	/* $argv, copied out element-wise. */
	pArgv = PH7_VmExtractSuper(pCtx->pVm,"argv",sizeof("argv")-1);
	if( sCol.rc == SXRET_OK && pArgv && ph7_value_is_array(pArgv) ){
		sCol.pOut = &sWords;
		ph7_array_walk(pArgv,GetoptArgvWalker,&sCol);
	}
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 || sCol.rc != SXRET_OK ){
		SyBlobRelease(&sPool);
		SySetRelease(&sSpecs);
		SySetRelease(&sWords);
		return PH7_ContextMemoryError(pCtx);
	}
	aWord = (getopt_word *)SySetBasePtr(&sWords);
	nWord = SySetUsed(&sWords);
	/* php starts at $argv[1]: $argv[0] is the script name. */
	iWord = 1;
	while( iWord < nWord ){
		const char *zArg = GetoptPoolAt(&sPool,aWord[iWord].nOfft);
		sxu32 nArgLen = aWord[iWord].nLen;
		if( nArgLen < 2 || zArg[0] != '-' ){
			/* Not an option (this covers "" and a bare "-"): stop here. */
			break;
		}
		if( zArg[1] == '-' ){
			const char *zName,*zEq;
			sxu32 nName,j;
			getopt_spec *pSpec;
			if( nArgLen == 2 ){
				/* "--" terminates the options and is consumed. */
				iWord++;
				break;
			}
			zName = &zArg[2];
			nName = nArgLen - 2;
			zEq = 0;
			for( j = 0 ; j < nName ; ++j ){
				if( zName[j] == '=' ){
					zEq = &zName[j];
					nName = j;
					break;
				}
			}
			pSpec = GetoptFindSpec(&sSpecs,&sPool,zName,nName,1);
			if( pSpec == 0 ){
				/* Unknown long option: skipped, parsing continues. */
				iWord++;
				continue;
			}
			if( zEq ){
				/* "--opt=" with nothing after the '=' drops the option entirely,
				 * whatever its arity — probed against php 8.5.8. A value that is
				 * itself an '=' loses that one leading character ("--opt==" is the
				 * empty value), the same rule the attached short form uses. */
				const char *zVal = zEq + 1;
				sxu32 nVal = (sxu32)((&zArg[nArgLen]) - zVal);
				if( nVal < 1 ){
					iWord++;
					continue;
				}
				if( zVal[0] == '=' ){
					zVal++;
					nVal--;
				}
				if( pSpec->iArg == GETOPT_ARG_NONE ){
					/* php ignores a value handed to a valueless option. */
					sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),
						pSpec->nNameLen,0,0,0);
				}else{
					sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),
						pSpec->nNameLen,zVal,nVal,1);
				}
			}else if( pSpec->iArg == GETOPT_ARG_REQUIRED ){
				/* The value is the NEXT element, whatever it looks like. With no
				 * next element the option is dropped, as php does. */
				if( iWord + 1 < nWord ){
					iWord++;
					sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),
						pSpec->nNameLen,GetoptPoolAt(&sPool,aWord[iWord].nOfft),aWord[iWord].nLen,1);
				}
			}else{
				/* No value, or an OPTIONAL one that was not attached. */
				sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),
					pSpec->nNameLen,0,0,0);
			}
			if( sCol.rc != SXRET_OK ){
				break;
			}
			iWord++;
			continue;
		}
		/* Short-option cluster: "-abval" is -a then -b with the value "val". */
		{
			sxu32 j = 1;
			while( j < nArgLen ){
				getopt_spec *pSpec = GetoptFindSpec(&sSpecs,&sPool,&zArg[j],1,0);
				const char *zVal;
				sxu32 nVal;
				if( pSpec == 0 ){
					/* Unknown option character: skipped, the cluster continues. */
					j++;
					continue;
				}
				if( pSpec->iArg == GETOPT_ARG_NONE ){
					sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),
						pSpec->nNameLen,0,0,0);
					if( sCol.rc != SXRET_OK ){
						break;
					}
					j++;
					continue;
				}
				/* Everything left in this element is the attached value; a single
				 * leading '=' is a separator, not part of it ("-b=v" is "v"). */
				zVal = &zArg[j+1];
				nVal = nArgLen - (j+1);
				if( nVal > 0 ){
					if( zVal[0] == '=' ){
						zVal++;
						nVal--;
					}
					sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),
						pSpec->nNameLen,zVal,nVal,1);
					j = nArgLen;
					break;
				}
				if( pSpec->iArg == GETOPT_ARG_REQUIRED ){
					/* Take the next element; with none left the option is dropped. */
					if( iWord + 1 < nWord ){
						iWord++;
						sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),
							pSpec->nNameLen,GetoptPoolAt(&sPool,aWord[iWord].nOfft),
							aWord[iWord].nLen,1);
					}
					j = nArgLen;
					break;
				}
				/* Optional value, nothing attached: FALSE, and the next element
				 * stays an operand. */
				sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),
					pSpec->nNameLen,0,0,0);
				if( sCol.rc != SXRET_OK ){
					break;
				}
				j++;
			}
			if( sCol.rc != SXRET_OK ){
				break;
			}
		}
		iWord++;
	}
	SyBlobRelease(&sPool);
	SySetRelease(&sSpecs);
	SySetRelease(&sWords);
	if( sCol.rc != SXRET_OK ){
		return PH7_ContextMemoryError(pCtx);
	}
	GetoptStoreRestIndex(pCtx,nArg,apArg,(sxi64)iWord);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
