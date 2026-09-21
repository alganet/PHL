# src/ph7/vm_builtin_getopt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 209/268 lines (77.99%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `/*` |
|    - |    8 | ` * getopt() walks the REAL $argv array, element by element, exactly like php's` |
|    - |    9 | ` * php_getopt(). The previous implementation scanned a flattened "arg1 arg2 …"` |
|    - |   10 | ` * string (pVm->sArgv) with no notion of element boundaries, which made it` |
|    - |   11 | ` * structurally unable to stop at the first non-option operand: every` |
|    - |   12 | ` * whitespace-separated word that followed an option was swallowed as another of` |
|    - |   13 | `` * that option's values (`--bee=2 rest1 rest2` answered `bee => [2, rest1,`` |
|    - |   14 | `` * rest2]` where php answers `bee => "2"` and reports index 3 as the rest). It`` |
|    - |   15 | `` * also had no clustering (`-abval`), no `--` terminator, and no `&$rest_index`.`` |
|    - |   16 | ` */` |
|    - |   17 |  |
|    - |   18 | `/* One declared option, from the short-option string or the long-option array. */` |
|    - |   19 | `#define GETOPT_ARG_NONE     0  /* "x"   — never takes a value */` |
|    - |   20 | `#define GETOPT_ARG_REQUIRED 1  /* "x:"  — takes the next word if not attached */` |
|    - |   21 | `#define GETOPT_ARG_OPTIONAL 2  /* "x::" — attached values only */` |
|    - |   22 | `typedef struct getopt_spec getopt_spec;` |
|    - |   23 | `struct getopt_spec` |
|    - |   24 | `{` |
|    - |   25 | `	sxu32 nNameOfft;  /* Offset of the option name inside the name pool */` |
|    - |   26 | `	sxu32 nNameLen;   /* Its length (1 for a short option) */` |
|    - |   27 | `	int iArg;         /* GETOPT_ARG_* */` |
|    - |   28 | `	int bLong;        /* TRUE for a --long option */` |
|    - |   29 | `};` |
|    - |   30 | `/* One $argv element, copied into the pool (the walker's values are transient). */` |
|    - |   31 | `typedef struct getopt_word getopt_word;` |
|    - |   32 | `struct getopt_word` |
|    - |   33 | `{` |
|    - |   34 | `	sxu32 nOfft;` |
|    - |   35 | `	sxu32 nLen;` |
|    - |   36 | `};` |
|    - |   37 | `/* Collector state shared by the two ph7_array_walk() callbacks below. */` |
|    - |   38 | `typedef struct getopt_collect getopt_collect;` |
|    - |   39 | `struct getopt_collect` |
|    - |   40 | `{` |
|    - |   41 | `	SyBlob *pPool;   /* Byte pool the offsets point into */` |
|    - |   42 | `	SySet *pOut;     /* SySet of getopt_word / getopt_spec */` |
|    - |   43 | `	sxi32 rc;        /* SXRET_OK or SXERR_MEM */` |
|    - |   44 | `};` |
|    - |   45 | `/*` |
|    - |   46 | ` * Append zIn[0..nLen) to the pool and return its offset. The pool can be` |
|    - |   47 | ` * reallocated, so nothing may hold a raw pointer into it across an append —` |
|    - |   48 | ` * every reference is an offset resolved through GetoptPoolAt().` |
|    - |   49 | ` */` |
|   96 |   50 | `static sxi32 GetoptPoolPut(SyBlob *pPool,const char *zIn,sxu32 nLen,sxu32 *pnOfft)` |
|    4 |   51 | `{` |
|  100 |   52 | `	*pnOfft = SyBlobLength(pPool);` |
|  100 |   53 | `	if( nLen < 1 ){` |
|  ! 0 |   54 | `		return SXRET_OK;` |
|    - |   55 | `	}` |
|  100 |   56 | `	return SyBlobAppend(pPool,(const void *)zIn,nLen);` |
|   52 |   57 | `}` |
|  100 |   58 | `static const char * GetoptPoolAt(SyBlob *pPool,sxu32 nOfft)` |
|    4 |   59 | `{` |
|  104 |   60 | `	return &((const char *)SyBlobData(pPool))[nOfft];` |
|    4 |   61 | `}` |
|    - |   62 | `/* ph7_array_walk() callback: copy one $argv element into the word list. */` |
|   66 |   63 | `static int GetoptArgvWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|    4 |   64 | `{` |
|   70 |   65 | `	getopt_collect *pCol = (getopt_collect *)pUserData;` |
|    - |   66 | `	getopt_word sWord;` |
|    - |   67 | `	const char *zIn;` |
|    - |   68 | `	int nIn;` |
|   70 |   69 | `	zIn = ph7_value_to_string(pData,&nIn);` |
|   70 |   70 | `	if( nIn < 0 ){` |
|  ! 0 |   71 | `		nIn = 0;` |
|  ! 0 |   72 | `	}` |
|   70 |   73 | `	if( GetoptPoolPut(pCol->pPool,zIn,(sxu32)nIn,&sWord.nOfft) != SXRET_OK ){` |
|  ! 0 |   74 | `		pCol->rc = SXERR_MEM;` |
|  ! 0 |   75 | `		return SXERR_ABORT;` |
|    - |   76 | `	}` |
|   70 |   77 | `	sWord.nLen = (sxu32)nIn;` |
|   70 |   78 | `	if( SySetPut(pCol->pOut,(const void *)&sWord) != SXRET_OK ){` |
|  ! 0 |   79 | `		pCol->rc = SXERR_MEM;` |
|  ! 0 |   80 | `		return SXERR_ABORT;` |
|    - |   81 | `	}` |
|   33 |   82 | `	SXUNUSED(pKey);` |
|   70 |   83 | `	return PH7_OK;` |
|   37 |   84 | `}` |
|    - |   85 | `/*` |
|    - |   86 | ` * ph7_array_walk() callback: turn one $long_options element into a spec. php` |
|    - |   87 | ` * ignores a non-string element, and reads a trailing ":"/"::" exactly like the` |
|    - |   88 | ` * short-option string.` |
|    - |   89 | ` */` |
|    8 |   90 | `static int GetoptLongWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|    2 |   91 | `{` |
|   10 |   92 | `	getopt_collect *pCol = (getopt_collect *)pUserData;` |
|    - |   93 | `	getopt_spec sSpec;` |
|    - |   94 | `	const char *zIn;` |
|    - |   95 | `	int nIn;` |
|   10 |   96 | `	if( !ph7_value_is_string(pData) ){` |
|  ! 0 |   97 | `		SXUNUSED(pKey);` |
|  ! 0 |   98 | `		return PH7_OK;` |
|    - |   99 | `	}` |
|   10 |  100 | `	zIn = ph7_value_to_string(pData,&nIn);` |
|   10 |  101 | `	if( nIn < 1 ){` |
|  ! 0 |  102 | `		return PH7_OK;` |
|    - |  103 | `	}` |
|   10 |  104 | `	sSpec.iArg = GETOPT_ARG_NONE;` |
|   10 |  105 | `	if( nIn > 1 && zIn[nIn-1] == ':' ){` |
|    5 |  106 | `		sSpec.iArg = GETOPT_ARG_REQUIRED;` |
|    5 |  107 | `		nIn--;` |
|    5 |  108 | `		if( nIn > 1 && zIn[nIn-1] == ':' ){` |
|    3 |  109 | `			sSpec.iArg = GETOPT_ARG_OPTIONAL;` |
|    3 |  110 | `			nIn--;` |
|    1 |  111 | `		}` |
|    2 |  112 | `	}` |
|   10 |  113 | `	if( nIn < 1 ){` |
|  ! 0 |  114 | `		return PH7_OK;` |
|    - |  115 | `	}` |
|   10 |  116 | `	if( GetoptPoolPut(pCol->pPool,zIn,(sxu32)nIn,&sSpec.nNameOfft) != SXRET_OK ){` |
|  ! 0 |  117 | `		pCol->rc = SXERR_MEM;` |
|  ! 0 |  118 | `		return SXERR_ABORT;` |
|    - |  119 | `	}` |
|   10 |  120 | `	sSpec.nNameLen = (sxu32)nIn;` |
|   10 |  121 | `	sSpec.bLong = 1;` |
|   10 |  122 | `	if( SySetPut(pCol->pOut,(const void *)&sSpec) != SXRET_OK ){` |
|  ! 0 |  123 | `		pCol->rc = SXERR_MEM;` |
|  ! 0 |  124 | `		return SXERR_ABORT;` |
|    - |  125 | `	}` |
|   10 |  126 | `	return PH7_OK;` |
|    6 |  127 | `}` |
|    - |  128 | `/* Find a declared option by name; bLong selects the -x / --xx namespace. */` |
|   32 |  129 | `static getopt_spec * GetoptFindSpec(SySet *pSpecs,SyBlob *pPool,const char *zName,sxu32 nName,int bLong)` |
|    4 |  130 | `{` |
|   36 |  131 | `	getopt_spec *aSpec = (getopt_spec *)SySetBasePtr(pSpecs);` |
|   36 |  132 | `	sxu32 n = SySetUsed(pSpecs);` |
|    - |  133 | `	sxu32 i;` |
|   84 |  134 | `	for( i = 0 ; i < n ; ++i ){` |
|   74 |  135 | `		if( aSpec[i].bLong != bLong \|\| aSpec[i].nNameLen != nName ){` |
|   29 |  136 | `			continue;` |
|    - |  137 | `		}` |
|   47 |  138 | `		if( SyMemcmp(GetoptPoolAt(pPool,aSpec[i].nNameOfft),zName,nName) == 0 ){` |
|   25 |  139 | `			return &aSpec[i];` |
|    - |  140 | `		}` |
|   13 |  141 | `	}` |
|   13 |  142 | `	return 0;` |
|   20 |  143 | `}` |
|    - |  144 | `/*` |
|    - |  145 | ` * Record one parsed option. php answers FALSE for a valueless option, the value` |
|    - |  146 | ` * string otherwise, and — when the SAME option appears more than once — an array` |
|    - |  147 | `` * of every occurrence in order (`-a -a` gives [false,false]).`` |
|    - |  148 | ` */` |
|   20 |  149 | `static sxi32 GetoptAddResult(` |
|    - |  150 | `	ph7_context *pCtx,` |
|    - |  151 | `	ph7_value *pArray,` |
|    - |  152 | `	const char *zName,sxu32 nName,` |
|    - |  153 | `	const char *zVal,sxu32 nVal,int bHasVal` |
|    - |  154 | `	)` |
|    3 |  155 | `{` |
|    - |  156 | `	ph7_value *pKey,*pVal,*pOld;` |
|   23 |  157 | `	sxi32 rc = SXRET_OK;` |
|   23 |  158 | `	pKey = ph7_context_new_scalar(pCtx);` |
|   23 |  159 | `	pVal = ph7_context_new_scalar(pCtx);` |
|   23 |  160 | `	if( pKey == 0 \|\| pVal == 0 ){` |
|  ! 0 |  161 | `		return SXERR_MEM;` |
|    - |  162 | `	}` |
|   23 |  163 | `	ph7_value_string(pKey,zName,(int)nName);` |
|   23 |  164 | `	if( bHasVal ){` |
|    7 |  165 | `		ph7_value_string(pVal,zVal,(int)nVal);` |
|    4 |  166 | `	}else{` |
|   17 |  167 | `		ph7_value_bool(pVal,0);` |
|    - |  168 | `	}` |
|   23 |  169 | `	pOld = ph7_array_fetch(pArray,zName,(int)nName);` |
|   23 |  170 | `	if( pOld == 0 ){` |
|   21 |  171 | `		rc = ph7_array_add_elem(pArray,pKey,pVal);` |
|   12 |  172 | `	}else if( ph7_value_is_array(pOld) ){` |
|    - |  173 | `		/* Third and later occurrence: append to the existing list. */` |
|  ! 0 |  174 | `		rc = ph7_array_add_elem(pOld,0,pVal);` |
|  ! 0 |  175 | `	}else{` |
|    - |  176 | `		/* Second occurrence: promote the scalar to php's list form. */` |
|    3 |  177 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|    3 |  178 | `		if( pList == 0 ){` |
|  ! 0 |  179 | `			rc = SXERR_MEM;` |
|  ! 0 |  180 | `		}else{` |
|    3 |  181 | `			rc = ph7_array_add_elem(pList,0,pOld);` |
|    3 |  182 | `			if( rc == SXRET_OK ){` |
|    3 |  183 | `				rc = ph7_array_add_elem(pList,0,pVal);` |
|    1 |  184 | `			}` |
|    3 |  185 | `			if( rc == SXRET_OK ){` |
|    3 |  186 | `				rc = ph7_array_add_elem(pArray,pKey,pList);` |
|    1 |  187 | `			}` |
|    3 |  188 | `			ph7_context_release_value(pCtx,pList);` |
|    - |  189 | `		}` |
|    - |  190 | `	}` |
|   23 |  191 | `	ph7_context_release_value(pCtx,pKey);` |
|   23 |  192 | `	ph7_context_release_value(pCtx,pVal);` |
|   23 |  193 | `	return rc;` |
|   13 |  194 | `}` |
|    - |  195 | `/*` |
|    - |  196 | ` * Write getopt()'s optional by-reference &$rest_index out-param: the $argv index` |
|    - |  197 | ` * of the first argument that is NOT an option (php's optind).` |
|    - |  198 | ` */` |
|   12 |  199 | `static void GetoptStoreRestIndex(ph7_context *pCtx,int nArg,ph7_value **apArg,sxi64 nRest)` |
|    4 |  200 | `{` |
|    - |  201 | `	ph7_value sVal;` |
|   16 |  202 | `	if( nArg < 3 ){` |
|    3 |  203 | `		return;` |
|    - |  204 | `	}` |
|   13 |  205 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sVal,nRest);` |
|   13 |  206 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],&sVal);` |
|   13 |  207 | `	PH7_MemObjRelease(&sVal);` |
|   10 |  208 | `}` |
|    - |  209 | `/*` |
|    - |  210 | ` * array\|false getopt(string $short_options[,array $long_options[,int &$rest_index]])` |
|    - |  211 | ` *   Gets options from the command line argument list.` |
|    - |  212 | ` * Parameters` |
|    - |  213 | ` *  $short_options` |
|    - |  214 | ` *   Each character is an option; a following ":" means it takes a value and` |
|    - |  215 | ` *   "::" that the value is optional (attached forms only).` |
|    - |  216 | ` *  $long_options` |
|    - |  217 | ` *   Option names for the "--name" form, with the same ":"/"::" suffixes.` |
|    - |  218 | ` *  &$rest_index` |
|    - |  219 | ` *   Set to the $argv index where option parsing stopped.` |
|    - |  220 | ` * Return` |
|    - |  221 | ` *  An array of option => value pairs, or FALSE on failure.` |
|    - |  222 | ` *` |
|    - |  223 | ` * Parsing follows php: it walks $argv from index 1 and STOPS at the first` |
|    - |  224 | ` * element that is not an option (including a bare "-" and the empty string),` |
|    - |  225 | ` * consuming a "--" terminator. An unknown option is skipped, and a required` |
|    - |  226 | ` * value that is nowhere to be found drops its option instead of inventing one.` |
|    - |  227 | ` */` |
|   12 |  228 | `PH7_PRIVATE int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  229 | `{` |
|    - |  230 | `	getopt_collect sCol;` |
|    - |  231 | `	getopt_word *aWord;` |
|    - |  232 | `	ph7_value *pArgv,*pArray;` |
|    - |  233 | `	SySet sSpecs,sWords;` |
|    - |  234 | `	SyBlob sPool;` |
|    - |  235 | `	const char *zIn;` |
|    - |  236 | `	sxu32 nWord,iWord;` |
|    - |  237 | `	int nByte;` |
|    - |  238 | `	int i;` |
|   16 |  239 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  240 | `		/* Missing/Invalid arguments,return FALSE */` |
|  ! 0 |  241 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");` |
|  ! 0 |  242 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  243 | `		return PH7_OK;` |
|    - |  244 | `	}` |
|   16 |  245 | `	SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|   16 |  246 | `	SySetInit(&sSpecs,&pCtx->pVm->sAllocator,sizeof(getopt_spec));` |
|   16 |  247 | `	SySetInit(&sWords,&pCtx->pVm->sAllocator,sizeof(getopt_word));` |
|   16 |  248 | `	SyZero(&sCol,sizeof(sCol));` |
|   16 |  249 | `	sCol.pPool = &sPool;` |
|    - |  250 | `	/* Declared short options: an alphanumeric character, optionally ':'/'::'. */` |
|   16 |  251 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   38 |  252 | `	for( i = 0 ; i < nByte ; ++i ){` |
|    - |  253 | `		getopt_spec sSpec;` |
|   26 |  254 | `		char c = zIn[i];` |
|   26 |  255 | `		if( !SyisAlphaNum((unsigned char)c) ){` |
|  ! 0 |  256 | `			continue;` |
|    - |  257 | `		}` |
|   26 |  258 | `		sSpec.iArg = GETOPT_ARG_NONE;` |
|   26 |  259 | `		if( i + 1 < nByte && zIn[i+1] == ':' ){` |
|   16 |  260 | `			sSpec.iArg = GETOPT_ARG_REQUIRED;` |
|   16 |  261 | `			i++;` |
|   16 |  262 | `			if( i + 1 < nByte && zIn[i+1] == ':' ){` |
|    3 |  263 | `				sSpec.iArg = GETOPT_ARG_OPTIONAL;` |
|    3 |  264 | `				i++;` |
|    1 |  265 | `			}` |
|    6 |  266 | `		}` |
|   26 |  267 | `		if( GetoptPoolPut(&sPool,&c,sizeof(char),&sSpec.nNameOfft) != SXRET_OK ){` |
|  ! 0 |  268 | `			sCol.rc = SXERR_MEM;` |
|  ! 0 |  269 | `			break;` |
|    - |  270 | `		}` |
|   26 |  271 | `		sSpec.nNameLen = 1;` |
|   26 |  272 | `		sSpec.bLong = 0;` |
|   26 |  273 | `		if( SySetPut(&sSpecs,(const void *)&sSpec) != SXRET_OK ){` |
|  ! 0 |  274 | `			sCol.rc = SXERR_MEM;` |
|  ! 0 |  275 | `			break;` |
|    - |  276 | `		}` |
|   15 |  277 | `	}` |
|    - |  278 | `	/* Declared long options. */` |
|   16 |  279 | `	if( sCol.rc == SXRET_OK && nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|   13 |  280 | `		sCol.pOut = &sSpecs;` |
|   13 |  281 | `		ph7_array_walk(apArg[1],GetoptLongWalker,&sCol);` |
|    5 |  282 | `	}` |
|    - |  283 | `	/* $argv, copied out element-wise. */` |
|   16 |  284 | `	pArgv = PH7_VmExtractSuper(pCtx->pVm,"argv",sizeof("argv")-1);` |
|   16 |  285 | `	if( sCol.rc == SXRET_OK && pArgv && ph7_value_is_array(pArgv) ){` |
|   16 |  286 | `		sCol.pOut = &sWords;` |
|   16 |  287 | `		ph7_array_walk(pArgv,GetoptArgvWalker,&sCol);` |
|    6 |  288 | `	}` |
|   16 |  289 | `	pArray = ph7_context_new_array(pCtx);` |
|   16 |  290 | `	if( pArray == 0 \|\| sCol.rc != SXRET_OK ){` |
|  ! 0 |  291 | `		SyBlobRelease(&sPool);` |
|  ! 0 |  292 | `		SySetRelease(&sSpecs);` |
|  ! 0 |  293 | `		SySetRelease(&sWords);` |
|  ! 0 |  294 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  295 | `	}` |
|   16 |  296 | `	aWord = (getopt_word *)SySetBasePtr(&sWords);` |
|   16 |  297 | `	nWord = SySetUsed(&sWords);` |
|    - |  298 | `	/* php starts at $argv[1]: $argv[0] is the script name. */` |
|   16 |  299 | `	iWord = 1;` |
|   44 |  300 | `	while( iWord < nWord ){` |
|   40 |  301 | `		const char *zArg = GetoptPoolAt(&sPool,aWord[iWord].nOfft);` |
|   40 |  302 | `		sxu32 nArgLen = aWord[iWord].nLen;` |
|   40 |  303 | `		if( nArgLen < 2 \|\| zArg[0] != '-' ){` |
|    - |  304 | `			/* Not an option (this covers "" and a bare "-"): stop here. */` |
|    4 |  305 | `			break;` |
|    - |  306 | `		}` |
|   36 |  307 | `		if( zArg[1] == '-' ){` |
|    - |  308 | `			const char *zName,*zEq;` |
|    - |  309 | `			sxu32 nName,j;` |
|    - |  310 | `			getopt_spec *pSpec;` |
|   15 |  311 | `			if( nArgLen == 2 ){` |
|    - |  312 | `				/* "--" terminates the options and is consumed. */` |
|    5 |  313 | `				iWord++;` |
|    5 |  314 | `				break;` |
|    - |  315 | `			}` |
|   10 |  316 | `			zName = &zArg[2];` |
|   10 |  317 | `			nName = nArgLen - 2;` |
|   10 |  318 | `			zEq = 0;` |
|   60 |  319 | `			for( j = 0 ; j < nName ; ++j ){` |
|   56 |  320 | `				if( zName[j] == '=' ){` |
|    5 |  321 | `					zEq = &zName[j];` |
|    5 |  322 | `					nName = j;` |
|    5 |  323 | `					break;` |
|    - |  324 | `				}` |
|   27 |  325 | `			}` |
|   10 |  326 | `			pSpec = GetoptFindSpec(&sSpecs,&sPool,zName,nName,1);` |
|   10 |  327 | `			if( pSpec == 0 ){` |
|    - |  328 | `				/* Unknown long option: skipped, parsing continues. */` |
|    3 |  329 | `				iWord++;` |
|    3 |  330 | `				continue;` |
|    - |  331 | `			}` |
|    7 |  332 | `			if( zEq ){` |
|    - |  333 | `				/* "--opt=" with nothing after the '=' drops the option entirely,` |
|    - |  334 | `				 * whatever its arity — probed against php 8.5.8. A value that is` |
|    - |  335 | `				 * itself an '=' loses that one leading character ("--opt==" is the` |
|    - |  336 | `				 * empty value), the same rule the attached short form uses. */` |
|    5 |  337 | `				const char *zVal = zEq + 1;` |
|    5 |  338 | `				sxu32 nVal = (sxu32)((&zArg[nArgLen]) - zVal);` |
|    5 |  339 | `				if( nVal < 1 ){` |
|  ! 0 |  340 | `					iWord++;` |
|  ! 0 |  341 | `					continue;` |
|    - |  342 | `				}` |
|    5 |  343 | `				if( zVal[0] == '=' ){` |
|  ! 0 |  344 | `					zVal++;` |
|  ! 0 |  345 | `					nVal--;` |
|  ! 0 |  346 | `				}` |
|    5 |  347 | `				if( pSpec->iArg == GETOPT_ARG_NONE ){` |
|    - |  348 | `					/* php ignores a value handed to a valueless option. */` |
|  ! 0 |  349 | `					sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),` |
|  ! 0 |  350 | `						pSpec->nNameLen,0,0,0);` |
|  ! 0 |  351 | `				}else{` |
|    7 |  352 | `					sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),` |
|    2 |  353 | `						pSpec->nNameLen,zVal,nVal,1);` |
|    1 |  354 | `				}` |
|    5 |  355 | `			}else if( pSpec->iArg == GETOPT_ARG_REQUIRED ){` |
|    - |  356 | `				/* The value is the NEXT element, whatever it looks like. With no` |
|    - |  357 | `				 * next element the option is dropped, as php does. */` |
|  ! 0 |  358 | `				if( iWord + 1 < nWord ){` |
|  ! 0 |  359 | `					iWord++;` |
|  ! 0 |  360 | `					sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),` |
|  ! 0 |  361 | `						pSpec->nNameLen,GetoptPoolAt(&sPool,aWord[iWord].nOfft),aWord[iWord].nLen,1);` |
|  ! 0 |  362 | `				}` |
|  ! 0 |  363 | `			}else{` |
|    - |  364 | `				/* No value, or an OPTIONAL one that was not attached. */` |
|    4 |  365 | `				sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),` |
|    1 |  366 | `					pSpec->nNameLen,0,0,0);` |
|    - |  367 | `			}` |
|    7 |  368 | `			if( sCol.rc != SXRET_OK ){` |
|  ! 0 |  369 | `				break;` |
|    - |  370 | `			}` |
|    7 |  371 | `			iWord++;` |
|    7 |  372 | `			continue;` |
|    - |  373 | `		}` |
|    - |  374 | `		/* Short-option cluster: "-abval" is -a then -b with the value "val". */` |
|    - |  375 | `		{` |
|   23 |  376 | `			sxu32 j = 1;` |
|   43 |  377 | `			while( j < nArgLen ){` |
|   27 |  378 | `				getopt_spec *pSpec = GetoptFindSpec(&sSpecs,&sPool,&zArg[j],1,0);` |
|    - |  379 | `				const char *zVal;` |
|    - |  380 | `				sxu32 nVal;` |
|   27 |  381 | `				if( pSpec == 0 ){` |
|    - |  382 | `					/* Unknown option character: skipped, the cluster continues. */` |
|   10 |  383 | `					j++;` |
|   10 |  384 | `					continue;` |
|    - |  385 | `				}` |
|   19 |  386 | `				if( pSpec->iArg == GETOPT_ARG_NONE ){` |
|   18 |  387 | `					sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),` |
|    5 |  388 | `						pSpec->nNameLen,0,0,0);` |
|   13 |  389 | `					if( sCol.rc != SXRET_OK ){` |
|  ! 0 |  390 | `						break;` |
|    - |  391 | `					}` |
|   13 |  392 | `					j++;` |
|   13 |  393 | `					continue;` |
|    - |  394 | `				}` |
|    - |  395 | `				/* Everything left in this element is the attached value; a single` |
|    - |  396 | `				 * leading '=' is a separator, not part of it ("-b=v" is "v"). */` |
|    8 |  397 | `				zVal = &zArg[j+1];` |
|    8 |  398 | `				nVal = nArgLen - (j+1);` |
|    8 |  399 | `				if( nVal > 0 ){` |
|    3 |  400 | `					if( zVal[0] == '=' ){` |
|  ! 0 |  401 | `						zVal++;` |
|  ! 0 |  402 | `						nVal--;` |
|  ! 0 |  403 | `					}` |
|    4 |  404 | `					sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),` |
|    1 |  405 | `						pSpec->nNameLen,zVal,nVal,1);` |
|    3 |  406 | `					j = nArgLen;` |
|    3 |  407 | `					break;` |
|    - |  408 | `				}` |
|    6 |  409 | `				if( pSpec->iArg == GETOPT_ARG_REQUIRED ){` |
|    - |  410 | `					/* Take the next element; with none left the option is dropped. */` |
|    3 |  411 | `					if( iWord + 1 < nWord ){` |
|  ! 0 |  412 | `						iWord++;` |
|  ! 0 |  413 | `						sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),` |
|  ! 0 |  414 | `							pSpec->nNameLen,GetoptPoolAt(&sPool,aWord[iWord].nOfft),` |
|  ! 0 |  415 | `							aWord[iWord].nLen,1);` |
|  ! 0 |  416 | `					}` |
|    3 |  417 | `					j = nArgLen;` |
|    3 |  418 | `					break;` |
|    - |  419 | `				}` |
|    - |  420 | `				/* Optional value, nothing attached: FALSE, and the next element` |
|    - |  421 | `				 * stays an operand. */` |
|    4 |  422 | `				sCol.rc = GetoptAddResult(pCtx,pArray,GetoptPoolAt(&sPool,pSpec->nNameOfft),` |
|    1 |  423 | `					pSpec->nNameLen,0,0,0);` |
|    3 |  424 | `				if( sCol.rc != SXRET_OK ){` |
|  ! 0 |  425 | `					break;` |
|    - |  426 | `				}` |
|    3 |  427 | `				j++;` |
|    1 |  428 | `			}` |
|   23 |  429 | `			if( sCol.rc != SXRET_OK ){` |
|  ! 0 |  430 | `				break;` |
|    - |  431 | `			}` |
|    - |  432 | `		}` |
|   23 |  433 | `		iWord++;` |
|    3 |  434 | `	}` |
|   16 |  435 | `	SyBlobRelease(&sPool);` |
|   16 |  436 | `	SySetRelease(&sSpecs);` |
|   16 |  437 | `	SySetRelease(&sWords);` |
|   16 |  438 | `	if( sCol.rc != SXRET_OK ){` |
|  ! 0 |  439 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  440 | `	}` |
|   16 |  441 | `	GetoptStoreRestIndex(pCtx,nArg,apArg,(sxi64)iWord);` |
|   16 |  442 | `	ph7_result_value(pCtx,pArray);` |
|   16 |  443 | `	return PH7_OK;` |
|   10 |  444 | `}` |
|    - |  445 |  |
