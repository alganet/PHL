# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7033/8958 lines (78.51%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits |  Line | Source |
| -------: | ----: | :--- |
|        - |     1 | `/**` |
|        - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |     5 | ` */` |
|        - |     6 | `#include "ph7int.h"` |
|        - |     7 | `#include <stddef.h>` |
|        - |     8 | `#include <stdlib.h>` |
|        - |     9 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |    10 | `#include <math.h>` |
|        - |    11 | `#endif` |
|        - |    12 | `/* Signed 64-bit multiplication with overflow detection. GCC/Clang expose` |
|        - |    13 | ` * __builtin_mul_overflow; MSVC does not, so we fall back to a portable` |
|        - |    14 | ` * UB-free bound-check implementation. Sets *pR to the wrapped product and` |
|        - |    15 | ` * returns non-zero on overflow. The fallback never divides a potentially-` |
|        - |    16 | ` * overflowed intermediate: all divisions are of compile-time constants` |
|        - |    17 | ` * (LARGEST_INT64/SMALLEST_INT64) by a factor already proven not to be -1,` |
|        - |    18 | ` * and the product itself is computed via unsigned multiplication to avoid` |
|        - |    19 | ` * signed-overflow UB. */` |
|        - |    20 | `#if defined(__GNUC__) \|\| defined(__clang__)` |
|        - |    21 | `#define VmMulOverflow64(a,b,pR) __builtin_mul_overflow((a),(b),(pR))` |
|        - |    22 | `#else` |
|        - |    23 | `static int VmMulOverflow64(sxi64 a, sxi64 b, sxi64 *pR)` |
|        1 |    24 |  |
|        1 |    25 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|        - |    26 | `	/* Factors of 0 or ±1 never overflow; handle up front so the divisions` |
|        - |    27 | `	 * below are guaranteed safe (no SMALLEST_INT64 / -1, no /0). */` |
|        1 |    28 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|        1 |    29 | `		return 0;` |
|        - |    30 | `	}` |
|        1 |    31 | `	if( a == -1 ){` |
|      ! 0 |    32 | `		return b == SMALLEST_INT64;` |
|        - |    33 | `	}` |
|        1 |    34 | `	if( b == -1 ){` |
|      ! 0 |    35 | `		return a == SMALLEST_INT64;` |
|        - |    36 | `	}` |
|        - |    37 | `	/* \|a\|,\|b\| >= 2 and neither is -1.  Bound check against the MAX/MIN` |
|        - |    38 | `	 * thresholds.  No division by -1 is possible here, and the quotients` |
|        - |    39 | `	 * of compile-time constants by {a,b} always fit in sxi64. */` |
|        1 |    40 | `	if( a > 0 ){` |
|        1 |    41 | `		if( b > 0 ){` |
|        1 |    42 | `			return a > LARGEST_INT64 / b;` |
|      ! 0 |    43 | `		}else{` |
|      ! 0 |    44 | `			return b < SMALLEST_INT64 / a;` |
|        - |    45 | `		}` |
|      ! 0 |    46 | `	}else{` |
|        1 |    47 | `		if( b > 0 ){` |
|        1 |    48 | `			return a < SMALLEST_INT64 / b;` |
|      ! 0 |    49 | `		}else{` |
|        1 |    50 | `			return b < LARGEST_INT64 / a;` |
|        - |    51 | `		}` |
|        - |    52 | `	}` |
|        1 |    53 |  |
|        - |    54 | `#endif` |
|        - |    55 | `/*` |
|        - |    56 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|        - |    57 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|        - |    58 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|        - |    59 | ` * statements.` |
|        - |    60 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|        - |    61 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|        - |    62 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|        - |    63 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|        - |    64 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|        - |    65 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|        - |    66 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|        - |    67 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|        - |    68 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|        - |    69 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|        - |    70 | ` * and so on.` |
|        - |    71 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|        - |    72 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|        - |    73 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|        - |    74 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|        - |    75 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|        - |    76 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|        - |    77 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|        - |    78 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|        - |    79 | ` */` |
|        - |    80 | `/* VmFrame struct and VM_FRAME_* defines moved to ph7int.h */` |
|        - |    81 | `/*` |
|        - |    82 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|        - |    83 | ` * memory object index is stored in an instance of the following structure and put` |
|        - |    84 | ` * in the free object table so that it can be reused again without allocating` |
|        - |    85 | ` * a new memory object.` |
|        - |    86 | ` */` |
|        - |    87 | `typedef struct VmSlot VmSlot;` |
|        - |    88 | `struct VmSlot` |
|        - |    89 |  |
|        - |    90 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|        - |    91 | `	void *pUserData; /* Upper-layer private data */` |
|        - |    92 | `};` |
|        - |    93 | `/*` |
|        - |    94 | ` * An entry in the reference table is represented by an instance of the` |
|        - |    95 | ` * follwoing table.` |
|        - |    96 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - |    97 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - |    98 | ` * the reference implementation is consistent,solid and it's` |
|        - |    99 | ` * behavior resemble the C++ reference mechanism.` |
|        - |   100 | ` * Refer to the official for more information on this powerful` |
|        - |   101 | ` * extension.` |
|        - |   102 | ` */` |
|        - |   103 | `struct VmRefObj` |
|        - |   104 |  |
|        - |   105 | `	SySet aReference;  /* Table of references to this memory object */` |
|        - |   106 | `	SySet aArrEntries; /* Foreign hashmap entries [i.e: array(&$a) ] */` |
|        - |   107 | `	sxu32 nIdx;        /* Referenced object index */` |
|        - |   108 | `	sxi32 iFlags;      /* Configuration flags */` |
|        - |   109 | `	VmRefObj *pNextCollide,*pPrevCollide; /* Collision link */` |
|        - |   110 | `	VmRefObj *pNext,*pPrev;               /* List of all referenced objects */` |
|        - |   111 | `};` |
|        - |   112 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|        - |   113 | `/* VmObEntry struct moved to ph7int.h */` |
|        - |   114 | `/*` |
|        - |   115 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |   116 | ` * is stored in an instance of the following structure.` |
|        - |   117 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |   118 | ` */` |
|        - |   119 | `typedef struct VmShutdownCB VmShutdownCB;` |
|        - |   120 | `struct VmShutdownCB` |
|        - |   121 |  |
|        - |   122 | `	ph7_value sCallback; /* Shutdown callback */` |
|        - |   123 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|        - |   124 | `	int nArg;             /* Total number of given arguments */` |
|        - |   125 | `};` |
|        - |   126 | `/*` |
|        - |   127 | ` * Each installed autoload callback (registered using [spl_autoload_register()] )` |
|        - |   128 | ` * is stored in an instance of the following structure.` |
|        - |   129 | ` * Refer to the implementation of [spl_autoload_register()] for more information.` |
|        - |   130 | ` */` |
|        - |   131 | `typedef struct VmAutoloadCB VmAutoloadCB;` |
|        - |   132 | `struct VmAutoloadCB` |
|        - |   133 |  |
|        - |   134 | `	ph7_value sCallback; /* Autoload callback (string or [obj,method] array) */` |
|        - |   135 | `};` |
|        - |   136 |  |
|        - |   137 | `/*` |
|        - |   138 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |   139 | ` */` |
|   953227 |   140 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        5 |   141 |  |
|   953232 |   142 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   143 | `		return TRUE;` |
|        - |   144 | `	}` |
|   953198 |   145 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   146 | `		return TRUE;` |
|        - |   147 | `	}` |
|   953188 |   148 | `	return FALSE;` |
|   476661 |   149 |  |
|        - |   150 | `/*` |
|        - |   151 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   152 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   153 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   154 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   155 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   156 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   157 | ` * still go through the existing numeric coercion.` |
|        - |   158 | ` */` |
|   347289 |   159 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        5 |   160 |  |
|        - |   161 | `	SyString sStr;` |
|   347294 |   162 | `	sxu8 bReal = FALSE;` |
|   347294 |   163 | `	const char *zTail = 0;` |
|        - |   164 | `	const char *zEnd;` |
|   347294 |   165 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   347224 |   166 | `		return FALSE;` |
|        - |   167 | `	}` |
|       71 |   168 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       71 |   169 | `	if( sStr.nByte == 0 ){` |
|        5 |   170 | `		return TRUE;` |
|        - |   171 | `	}` |
|       67 |   172 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|       47 |   173 | `		return TRUE;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|        - |   176 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|        - |   177 | `	 * still takes the Perl path. */` |
|       21 |   178 | `	zEnd = sStr.zString + sStr.nByte;` |
|       25 |   179 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|        5 |   180 | `		zTail++;` |
|        1 |   181 | `	}` |
|       21 |   182 | `	return zTail < zEnd;` |
|   173692 |   183 |  |
|        - |   184 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   185 | `/* Constant expander used by define(); used below to recognise user-defined` |
|        - |   186 | ` * (vs. host/built-in) constants so their owned value object can be freed when` |
|        - |   187 | ` * a define() overwrites them. */` |
|        - |   188 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData);` |
|        - |   189 | `/*` |
|        - |   190 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   191 | ` * it can be expanded from the target PHP program.` |
|        - |   192 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   193 | ` * simple and work as follows:` |
|        - |   194 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   195 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   196 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   197 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   198 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   199 | ` * (Windows,Linux,...) and so on.` |
|        - |   200 | ` * Please refer to the official documentation for additional information.` |
|        - |   201 | ` */` |
|   672852 |   202 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   203 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   204 | `	const SyString *pName,  /* Constant name */` |
|        - |   205 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   206 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   207 | `	)` |
|        5 |   208 |  |
|        - |   209 | `	ph7_constant *pCons;` |
|        - |   210 | `	SyHashEntry *pEntry;` |
|        - |   211 | `	char *zDupName;` |
|        - |   212 | `	sxi32 rc;` |
|   672857 |   213 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   672857 |   214 | `	if( pEntry ){` |
|        - |   215 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   216 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |   217 | `		/* A user-defined (define()) constant owns a heap ph7_value as its` |
|        - |   218 | `		 * pUserData; free it before overwriting so repeated define()s — e.g.` |
|        - |   219 | `		 * the same script re-run on a reused VM — don't leak the old value. */` |
|        4 |   220 | `		if( pCons->xExpand == VmExpandUserConstant && pCons->pUserData` |
|        4 |   221 | `		 && pCons->pUserData != pUserData ){` |
|        3 |   222 | `			PH7_MemObjRelease((ph7_value *)pCons->pUserData);` |
|        3 |   223 | `			SyMemBackendPoolFree(&pVm->sAllocator,pCons->pUserData);` |
|        1 |   224 | `		}` |
|        6 |   225 | `		pCons->xExpand = xExpand;` |
|        6 |   226 | `		pCons->pUserData = pUserData;` |
|        6 |   227 | `		return SXRET_OK;` |
|        - |   228 | `	}` |
|        - |   229 | `	/* Allocate a new constant instance */` |
|   672853 |   230 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   672853 |   231 | `	if( pCons == 0 ){` |
|      ! 0 |   232 | `		return 0;` |
|        - |   233 | `	}` |
|        - |   234 | `	/* Duplicate constant name */` |
|   672853 |   235 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   672853 |   236 | `	if( zDupName == 0 ){` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   238 | `		return 0;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* Install the constant */` |
|   672853 |   241 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   672853 |   242 | `	pCons->xExpand = xExpand;` |
|   672853 |   243 | `	pCons->pUserData = pUserData;` |
|   672853 |   244 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   672853 |   245 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   246 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   247 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   248 | `		return rc;` |
|        - |   249 | `	}` |
|        - |   250 | `	/* All done,constant can be invoked from PHP code */` |
|   672853 |   251 | `	return SXRET_OK;` |
|   336431 |   252 |  |
|        - |   253 | `/*` |
|        - |   254 | ` * Allocate a new foreign function instance.` |
|        - |   255 | ` * This function return SXRET_OK on success. Any other` |
|        - |   256 | ` * return value indicates failure.` |
|        - |   257 | ` * Please refer to the official documentation for an introduction to` |
|        - |   258 | ` * the foreign function mechanism.` |
|        - |   259 | ` */` |
|  1491424 |   260 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   261 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   262 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   263 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   264 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   265 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   266 | `	)` |
|        5 |   267 |  |
|        - |   268 | `	ph7_user_func *pFunc;` |
|        - |   269 | `	char *zDup;` |
|        - |   270 | `	/* Allocate a new user function */` |
|  1491429 |   271 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1491429 |   272 | `	if( pFunc == 0 ){` |
|      ! 0 |   273 | `		return SXERR_MEM;` |
|        - |   274 | `	}` |
|        - |   275 | `	/* Duplicate function name */` |
|  1491429 |   276 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1491429 |   277 | `	if( zDup == 0 ){` |
|      ! 0 |   278 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   279 | `		return SXERR_MEM;` |
|        - |   280 | `	}` |
|        - |   281 | `	/* Zero the structure */` |
|  1491429 |   282 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   283 | `	/* Initialize structure fields */` |
|  1491429 |   284 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1491429 |   285 | `	pFunc->pVm   = pVm;` |
|  1491429 |   286 | `	pFunc->xFunc = xFunc;` |
|  1491429 |   287 | `	pFunc->pUserData = pUserData;` |
|  1491429 |   288 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   289 | `	/* Write a pointer to the new function */` |
|  1491429 |   290 | `	*ppOut = pFunc;` |
|  1491429 |   291 | `	return SXRET_OK;` |
|   745717 |   292 |  |
|        - |   293 | `/*` |
|        - |   294 | ` * Install a foreign function and it's associated callback so that` |
|        - |   295 | ` * it can be invoked from the target PHP code.` |
|        - |   296 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   297 | ` * return value indicates failure.` |
|        - |   298 | ` * Please refer to the official documentation for an introduction to` |
|        - |   299 | ` * the foreign function mechanism.` |
|        - |   300 | ` */` |
|  1494388 |   301 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   302 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   303 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   304 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   305 | `	void *pUserData           /* Foreign function private data */` |
|        - |   306 | `	)` |
|        5 |   307 |  |
|        - |   308 | `	ph7_user_func *pFunc;` |
|        - |   309 | `	SyHashEntry *pEntry;` |
|        - |   310 | `	sxi32 rc;` |
|        - |   311 | `	/* Overwrite any previously registered function with the same name */` |
|  1494393 |   312 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1494393 |   313 | `	if( pEntry ){` |
|     2969 |   314 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2969 |   315 | `		pFunc->pUserData = pUserData;` |
|     2969 |   316 | `		pFunc->xFunc = xFunc;` |
|     2969 |   317 | `		SySetReset(&pFunc->aAux);` |
|     2969 |   318 | `		return SXRET_OK;` |
|        - |   319 | `	}` |
|        - |   320 | `	/* Create a new user function */` |
|  1491429 |   321 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1491429 |   322 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   323 | `		return rc;` |
|        - |   324 | `	}` |
|        - |   325 | `	/* Install the function in the corresponding hashtable */` |
|  1491429 |   326 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1491429 |   327 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   328 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   329 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   330 | `		return rc;` |
|        - |   331 | `	}` |
|        - |   332 | `	/* User function successfully installed */` |
|  1491429 |   333 | `	return SXRET_OK;` |
|   747199 |   334 |  |
|        - |   335 | `/*` |
|        - |   336 | ` * Initialize a VM function.` |
|        - |   337 | ` */` |
|   292852 |   338 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   339 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   340 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   341 | `	const char *zName,  /* Function name */` |
|        - |   342 | `	sxu32 nByte,        /* zName length */` |
|        - |   343 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   344 | `	void *pUserData     /* Function private data */` |
|        - |   345 | `	)` |
|        5 |   346 |  |
|        - |   347 | `	/* Zero the structure */` |
|   292857 |   348 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   349 | `	/* Initialize structure fields */` |
|        - |   350 | `	/* Arguments container */` |
|   292857 |   351 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   352 | `	/* Static variable container */` |
|   292857 |   353 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   354 | `	/* Bytecode container */` |
|   292857 |   355 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   356 | `    /* Preallocate some instruction slots */` |
|   292857 |   357 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   358 | `	/* Closure environment */` |
|   292857 |   359 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   360 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   292857 |   361 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   292857 |   362 | `	pFunc->iFlags = iFlags;` |
|   292857 |   363 | `	pFunc->pUserData = pUserData;` |
|        - |   364 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   365 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   292857 |   366 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   292857 |   367 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   292857 |   368 | `	return SXRET_OK;` |
|        5 |   369 |  |
|        - |   370 | `/*` |
|        - |   371 | ` * Namespace-aware function lookup.` |
|        - |   372 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   373 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   374 | ` */` |
|        - |   375 | `/*` |
|        - |   376 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   377 | ` */` |
|  1530556 |   378 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   379 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   380 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   381 | `	SyString *pName     /* Function name */` |
|        - |   382 | `	)` |
|        5 |   383 |  |
|        - |   384 | `	SyHashEntry *pEntry;` |
|        - |   385 | `	sxi32 rc;` |
|  1530561 |   386 | `	if( pName == 0 ){` |
|        - |   387 | `		/* Use the built-in name */` |
|    44137 |   388 | `		pName = &pFunc->sName;` |
|    22066 |   389 | `	}` |
|        - |   390 | `	/* Check for duplicates (functions with the same name) first */` |
|  1530561 |   391 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  1530561 |   392 | `	if( pEntry ){` |
|  1323901 |   393 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  1323901 |   394 | `		if( pLink != pFunc ){` |
|        - |   395 | `			/* Link */` |
|      188 |   396 | `			pFunc->pNextName = pLink;` |
|      188 |   397 | `			pEntry->pUserData = pFunc;` |
|       93 |   398 | `		}` |
|  1323901 |   399 | `		return SXRET_OK;` |
|        - |   400 | `	}` |
|        - |   401 | `	/* First time seen */` |
|   206665 |   402 | `	pFunc->pNextName = 0;` |
|   206665 |   403 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   206665 |   404 | `	return rc;` |
|   765283 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   408 | ` */` |
|   126660 |   409 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   410 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   411 | `	ph7_class *pClass /* Target Class */` |
|        - |   412 | `	)` |
|        5 |   413 |  |
|   126665 |   414 | `	SyString *pName = &pClass->sName;` |
|        - |   415 | `	SyHashEntry *pEntry;` |
|        - |   416 | `	sxi32 rc;` |
|        - |   417 | `	/* Check for duplicates */` |
|   126665 |   418 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   126665 |   419 | `	if( pEntry ){` |
|       31 |   420 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   421 | `		/* Link entry with the same name */` |
|       31 |   422 | `		pClass->pNextName = pLink;` |
|       31 |   423 | `		pEntry->pUserData = pClass;` |
|       31 |   424 | `		return SXRET_OK;` |
|        - |   425 | `	}` |
|   126635 |   426 | `	pClass->pNextName = 0;` |
|        - |   427 | `	/* Perform a simple hashtable insertion */` |
|   126635 |   428 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   126635 |   429 | `	return rc;` |
|    63335 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Instruction builder interface.` |
|        - |   433 | ` */` |
|  4476624 |   434 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   435 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   436 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   437 | `	sxi32 iP1,    /* First operand */` |
|        - |   438 | `	sxu32 iP2,    /* Second operand */` |
|        - |   439 | `	void *p3,     /* Third operand */` |
|        - |   440 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   441 | `	)` |
|        5 |   442 |  |
|        - |   443 | `	VmInstr sInstr;` |
|        - |   444 | `	sxi32 rc;` |
|        - |   445 | `	/* Fill the VM instruction */` |
|  4476629 |   446 | `	sInstr.iOp = (sxu8)iOp;` |
|  4476629 |   447 | `	sInstr.iP1 = iP1;` |
|  4476629 |   448 | `	sInstr.iP2 = iP2;` |
|  4476629 |   449 | `	sInstr.p3  = p3;` |
|  4476629 |   450 | `	if( pIndex ){` |
|        - |   451 | `		/* Instruction index in the bytecode array */` |
|   243237 |   452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   121616 |   453 | `	}` |
|        - |   454 | `	/* Finally,record the instruction */` |
|  4476629 |   455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4476629 |   456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   458 | `		/* Fall throw */` |
|      ! 0 |   459 | `	}` |
|  4476629 |   460 | `	return rc;` |
|        5 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Swap the current bytecode container with the given one.` |
|        - |   464 | ` */` |
|   581804 |   465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        5 |   466 |  |
|   581809 |   467 | `	if( pContainer == 0 ){` |
|        - |   468 | `		/* Point to the default container */` |
|      ! 0 |   469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   470 | `	}else{` |
|        - |   471 | `		/* Change container */` |
|   581809 |   472 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   473 | `	}` |
|   581809 |   474 | `	return SXRET_OK;` |
|        5 |   475 |  |
|        - |   476 | `/*` |
|        - |   477 | ` * Return the current bytecode container.` |
|        - |   478 | ` */` |
|   290902 |   479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        5 |   480 |  |
|   290907 |   481 | `	return pVm->pByteContainer;` |
|        5 |   482 |  |
|        - |   483 | `/*` |
|        - |   484 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   485 | ` */` |
|   239852 |   486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        5 |   487 |  |
|        - |   488 | `	VmInstr *pInstr;` |
|   239857 |   489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   239857 |   490 | `	return pInstr;` |
|        5 |   491 |  |
|        - |   492 | `/*` |
|        - |   493 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   494 | ` */` |
|  1345668 |   495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        5 |   496 |  |
|  1345673 |   497 | `	return SySetUsed(pVm->pByteContainer);` |
|        5 |   498 |  |
|        - |   499 | `/*` |
|        - |   500 | ` * Pop the last VM instruction.` |
|        - |   501 | ` */` |
|   221796 |   502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        5 |   503 |  |
|   221801 |   504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        5 |   505 |  |
|        - |   506 | `/*` |
|        - |   507 | ` * Peek the last VM instruction.` |
|        - |   508 | ` */` |
|   882458 |   509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        5 |   510 |  |
|   882463 |   511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        5 |   512 |  |
|    35304 |   513 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        5 |   514 |  |
|        - |   515 | `	VmInstr *aInstr;` |
|        - |   516 | `	sxu32 n;` |
|    35309 |   517 | `	n = SySetUsed(pVm->pByteContainer);` |
|    35309 |   518 | `	if( n < 2 ){` |
|      ! 0 |   519 | `		return 0;` |
|        - |   520 | `	}` |
|    35309 |   521 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    35309 |   522 | `	return &aInstr[n - 2];` |
|    17657 |   523 |  |
|        - |   524 | `/*` |
|        - |   525 | ` * Allocate a new virtual machine frame.` |
|        - |   526 | ` */` |
|    24280 |   527 | `static VmFrame * VmNewFrame(` |
|        - |   528 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   529 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   531 | `	)` |
|        5 |   532 |  |
|        - |   533 | `	VmFrame *pFrame;` |
|        - |   534 | `	/* Allocate a new vm frame */` |
|    24285 |   535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    24285 |   536 | `	if( pFrame == 0 ){` |
|      ! 0 |   537 | `		return 0;` |
|        - |   538 | `	}` |
|        - |   539 | `	/* Zero the structure */` |
|    24285 |   540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   541 | `	/* Initialize frame fields */` |
|    24285 |   542 | `	pFrame->pUserData = pUserData;` |
|    24285 |   543 | `	pFrame->pThis = pThis;` |
|    24285 |   544 | `	pFrame->pVm = pVm;` |
|    24285 |   545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    24285 |   546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    24285 |   547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    24285 |   548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    24285 |   549 | `	return pFrame;` |
|    12145 |   550 |  |
|        - |   551 | `/* Forward declaration */` |
|        - |   552 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   553 | `/*` |
|        - |   554 | ` * Enter a VM frame.` |
|        - |   555 | ` */` |
|    24208 |   556 | `static sxi32 VmEnterFrame(` |
|        - |   557 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   558 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   559 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   560 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   561 | `	)` |
|        5 |   562 |  |
|        - |   563 | `	VmFrame *pFrame;` |
|        - |   564 | `	/* Allocate a new frame */` |
|    24213 |   565 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    24213 |   566 | `	if( pFrame == 0 ){` |
|      ! 0 |   567 | `		return SXERR_MEM;` |
|        - |   568 | `	}` |
|        - |   569 | `	/* Link to the list of active VM frame */` |
|    24213 |   570 | `	pFrame->pParent = pVm->pFrame;` |
|    24213 |   571 | `	pVm->pFrame = pFrame;` |
|    24213 |   572 | `	if( ppFrame ){` |
|        - |   573 | `		/* Write a pointer to the new VM frame */` |
|    20903 |   574 | `		*ppFrame = pFrame;` |
|    10449 |   575 | `	}` |
|    24213 |   576 | `	return SXRET_OK;` |
|    12109 |   577 |  |
|        - |   578 | `/*` |
|        - |   579 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   580 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   581 | ` * information.` |
|        - |   582 | ` */` |
|       70 |   583 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        5 |   584 |  |
|        - |   585 | `	VmFrame *pTarget,*pFrame;` |
|       75 |   586 | `	SyHashEntry *pEntry = 0;` |
|        - |   587 | `	sxi32 rc;` |
|        - |   588 | `	/* Point to the upper frame */` |
|       75 |   589 | `	pFrame = pVm->pFrame;` |
|       75 |   590 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       75 |   591 | `	pTarget = pFrame;` |
|       75 |   592 | `	pFrame = pTarget->pParent;` |
|       75 |   593 | `	while( pFrame ){` |
|       75 |   594 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   595 | `			/* Query the current frame */` |
|       75 |   596 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       75 |   597 | `			if( pEntry ){` |
|        - |   598 | `				/* Variable found */` |
|       75 |   599 | `				break;` |
|        - |   600 | `			}` |
|      ! 0 |   601 | `		}` |
|        - |   602 | `		/* Point to the upper frame */` |
|      ! 0 |   603 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   604 | `	}` |
|       75 |   605 | `	if( pEntry == 0 ){` |
|        - |   606 | `		/* Inexistant variable */` |
|      ! 0 |   607 | `		return SXERR_NOTFOUND;` |
|        - |   608 | `	}` |
|        - |   609 | `	/* Link to the current frame */` |
|       75 |   610 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       75 |   611 | `	if( rc == SXRET_OK ){` |
|        - |   612 | `		sxu32 nIdx;` |
|       75 |   613 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       75 |   614 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       35 |   615 | `	}` |
|       75 |   616 | `	return rc;` |
|       40 |   617 |  |
|        - |   618 | `/*` |
|        - |   619 | ` * Leave the top-most active frame.` |
|        - |   620 | ` */` |
|    20894 |   621 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        5 |   622 |  |
|    20899 |   623 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    20899 |   624 | `	if( pCurFrame ){` |
|        - |   625 | `		/* Unlink from the list of active VM frame */` |
|    20899 |   626 | `		pVm->pFrame = pCurFrame->pParent;` |
|    20899 |   627 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   628 | `			VmSlot  *aSlot;` |
|        - |   629 | `			sxu32 n;` |
|        - |   630 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    20027 |   631 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   131407 |   632 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   633 | `				/* Unset the local variable */` |
|   111385 |   634 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    55695 |   635 | `			}` |
|        - |   636 | `			/* Remove local reference */` |
|    20027 |   637 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   131481 |   638 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   111459 |   639 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    55732 |   640 | `			}` |
|    10011 |   641 | `		}` |
|        - |   642 | `		/* Release internal containers */` |
|    20899 |   643 | `		SyHashRelease(&pCurFrame->hVar);` |
|    20899 |   644 | `		SySetRelease(&pCurFrame->sArg);` |
|    20899 |   645 | `		SySetRelease(&pCurFrame->sLocal);` |
|    20899 |   646 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   647 | `		/* Release the whole structure */` |
|    20899 |   648 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|    10447 |   649 | `	}` |
|    20899 |   650 |  |
|        - |   651 | `/*` |
|        - |   652 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   653 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   654 | ` * should be skipped when looking for the real execution context.` |
|        - |   655 | ` */` |
|  7332311 |   656 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        5 |   657 |  |
|  7337148 |   658 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     4837 |   659 | `		pFrame = pFrame->pParent;` |
|        5 |   660 | `	}` |
|  7332316 |   661 | `	return pFrame;` |
|        5 |   662 |  |
|        - |   663 | `/*` |
|        - |   664 | ` * After a callee invoked from an OP_CALL site (object __invoke, an array` |
|        - |   665 | ` * callable, or a NEW constructor) returns PH7_EXCEPTION, decide how the current` |
|        - |   666 | ` * frame unwinds. The catch body, if any, already ran in-place inside` |
|        - |   667 | ` * VmThrowException. Returns TRUE and sets *pResumePc to the post-try resume` |
|        - |   668 | ` * point when THIS frame's own try caught the exception; returns FALSE to signal` |
|        - |   669 | ` * the caller to propagate (goto Exception) so the exception unwinds through` |
|        - |   670 | ` * intermediate frames that have no local handler.` |
|        - |   671 | ` */` |
|       12 |   672 | `static int VmCalleeExceptionResume(ph7_vm *pVm,sxi32 *pResumePc)` |
|        1 |   673 |  |
|       13 |   674 | `	VmFrame *pFrame = pVm->pFrame;` |
|       12 |   675 | `	if( (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0` |
|       11 |   676 | `	 && !(pFrame->iFlags & VM_FRAME_THROW) ){` |
|       11 |   677 | `		*pResumePc = (sxi32)pFrame->iExceptionJump - 1;` |
|       11 |   678 | `		return TRUE;` |
|        - |   679 | `	}` |
|        3 |   680 | `	return FALSE;` |
|        7 |   681 |  |
|        - |   682 | `/*` |
|        - |   683 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|        - |   684 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|        - |   685 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|        - |   686 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|        - |   687 | ` * nested try/finally inside a catch body). Each finally runs with` |
|        - |   688 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value via` |
|        - |   689 | ` * pVm->sCatchReturn. Returns SXERR_ABORT if a finally aborted, SXRET_OK otherwise.` |
|        - |   690 | ` */` |
|    49544 |   691 | `static sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|        5 |   692 |  |
|        - |   693 | `	sxu32 nUsed;` |
|    49559 |   694 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|       12 |   695 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       12 |   696 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|       12 |   697 | `		(void)SySetPop(&pVm->aException);` |
|       12 |   698 | `		pExc->pFrame = 0;` |
|       12 |   699 | `		VmLeaveFrame(&(*pVm));` |
|       12 |   700 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|       12 |   701 | `			pExc->iFinallyDone = 1;` |
|       12 |   702 | `			if( VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE) == SXERR_ABORT ){` |
|      ! 0 |   703 | `				return SXERR_ABORT;` |
|        - |   704 | `			}` |
|        5 |   705 | `		}` |
|        2 |   706 | `	}` |
|    49549 |   707 | `	return SXRET_OK;` |
|    24777 |   708 |  |
|        - |   709 | `/*` |
|        - |   710 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|        - |   711 | ` * deferred pVm->sCatchReturn into the enclosing function's result, clear the` |
|        - |   712 | ` * signal, and tear down any try frames left open above pEntryFrame whose` |
|        - |   713 | ` * OP_POP_EXCEPTION the return bypassed (bounded by pEntryFrame so a tangled` |
|        - |   714 | ` * exception-in-finally chain can't over-leave). Only the real function body` |
|        - |   715 | ` * (bReturnPropagates=FALSE) calls this; a nested mini-program leaves the signal` |
|        - |   716 | ` * set so it propagates outward to its own enclosing function.` |
|        - |   717 | ` */` |
|       32 |   718 | `static void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|        2 |   719 |  |
|       34 |   720 | `	if( pResult ){` |
|       34 |   721 | `		PH7_MemObjStore(&pVm->sCatchReturn,pResult);` |
|       16 |   722 | `	}` |
|       34 |   723 | `	pVm->bReturnRequested = 0;` |
|       34 |   724 | `	PH7_MemObjRelease(&pVm->sCatchReturn);` |
|       36 |   725 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|        3 |   726 | `		VmLeaveFrame(&(*pVm));` |
|        1 |   727 | `	}` |
|       34 |   728 |  |
|        - |   729 | `/*` |
|        - |   730 | ` * Compare two functions signature and return the comparison result.` |
|        - |   731 | ` */` |
|      836 |   732 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   733 |  |
|      837 |   734 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   735 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   736 | `	const char *zSin = pSecond->zString;` |
|      837 |   737 | `	const char *zFin = pFirst->zString;` |
|      837 |   738 | `	const char *zPtr = zFin;` |
|      421 |   739 | `	for(;;){` |
|      843 |   740 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   741 | `			break;` |
|        - |   742 | `		}` |
|       19 |   743 | `		if( zFin[0] != zSin[0] ){` |
|        - |   744 | `			/* mismatch */` |
|       13 |   745 | `			break;` |
|        - |   746 | `		}` |
|        7 |   747 | `		zFin++;` |
|        7 |   748 | `		zSin++;` |
|        1 |   749 | `	}` |
|      837 |   750 | `	return (int)(zFin-zPtr);` |
|        1 |   751 |  |
|        - |   752 | `/*` |
|        - |   753 | ` * Select the appropriate VM function for the current call context.` |
|        - |   754 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   755 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   756 | ` * Refer to the official documentation for more information.` |
|        - |   757 | ` */` |
|      138 |   758 | `static ph7_vm_func * VmOverload(` |
|        - |   759 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   760 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   761 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   762 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   763 | `	)` |
|        2 |   764 |  |
|        - |   765 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   766 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   767 | `	ph7_vm_func *pLink;` |
|        - |   768 | `	SyString sArgSig;` |
|        - |   769 | `	SyBlob sSig;` |
|        - |   770 |  |
|      140 |   771 | `	pLink = pList;` |
|      140 |   772 | `	i = 0;` |
|        - |   773 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   774 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   775 | `		if( pLink == 0 ){` |
|       78 |   776 | `			break;` |
|        - |   777 | `		}` |
|      948 |   778 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   779 | `			/* Candidate for overloading */` |
|      902 |   780 | `			apSet[i++] = pLink;` |
|      450 |   781 | `		}` |
|        - |   782 | `		/* Point to the next entry */` |
|      948 |   783 | `		pLink = pLink->pNextName;` |
|        2 |   784 | `	}` |
|      140 |   785 | `	if( i < 1 ){` |
|        - |   786 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   787 | `		return pList;` |
|        - |   788 | `	}` |
|      140 |   789 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   790 | `		/* Return the only candidate */` |
|       32 |   791 | `		return apSet[0];` |
|        - |   792 | `	}` |
|        - |   793 | `	/* Calculate function signature */` |
|      109 |   794 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   795 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   796 | `		int c = 'n'; /* null */` |
|      259 |   797 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   798 | `			/* Hashmap */` |
|       45 |   799 | `			c = 'h';` |
|      237 |   800 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   801 | `			/* bool */` |
|      ! 0 |   802 | `			c = 'b';` |
|      215 |   803 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   804 | `			/* int */` |
|        7 |   805 | `			c = 'i';` |
|      212 |   806 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   807 | `			/* String */` |
|      107 |   808 | `			c = 's';` |
|      156 |   809 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   810 | `			/* Float */` |
|      ! 0 |   811 | `			c = 'f';` |
|      103 |   812 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   813 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   814 | `			int marker = 'o';` |
|        3 |   815 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   816 | `			SyString *pName = &pClass->sName;` |
|        3 |   817 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   818 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   819 | `			c = -1;` |
|        1 |   820 | `		}` |
|      259 |   821 | `		if( c > 0 ){` |
|      257 |   822 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   823 | `		}` |
|      130 |   824 | `	}` |
|      109 |   825 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   826 | `	iTarget = 0;` |
|      109 |   827 | `	iMax = -1;` |
|        - |   828 | `	/* Select the appropriate function */` |
|      945 |   829 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   830 | `		/* Compare the two signatures */` |
|      837 |   831 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   832 | `		if( iCur > iMax ){` |
|      113 |   833 | `			iMax = iCur;` |
|      113 |   834 | `			iTarget = j;` |
|       56 |   835 | `		}` |
|      419 |   836 | `	}` |
|      109 |   837 | `	SyBlobRelease(&sSig);` |
|        - |   838 | `	/* Appropriate function for the current call context */` |
|      109 |   839 | `	return apSet[iTarget];` |
|       71 |   840 |  |
|        - |   841 | `/* Forward declaration */` |
|        - |   842 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   843 | `static sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);` |
|        - |   844 | `/*` |
|        - |   845 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   846 | ` * it can be instanciated from the executed PHP script.` |
|        - |   847 | ` */` |
|        - |   848 | `/*` |
|        - |   849 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|        - |   850 | ` * This is the per-execution part of mounting a class: every static/const` |
|        - |   851 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|        - |   852 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|        - |   853 | ` * properties register their enforcement slot. It is factored out of` |
|        - |   854 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|        - |   855 | ` * reuse without re-installing the (compile-time) methods.` |
|        - |   856 | ` */` |
|   371120 |   857 | `static sxi32 VmMountUserClassAttrs(` |
|        - |   858 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   859 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - |   860 | `	)` |
|        5 |   861 |  |
|        - |   862 | `	ph7_class_attr *pAttr;` |
|        - |   863 | `	SyHashEntry *pEntry;` |
|        - |   864 | `	/* Reset the loop cursor */` |
|   371125 |   865 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   866 | `	/* Process only static and constant attribute */` |
|  1472049 |   867 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   868 | `		/* Extract the current attribute */` |
|   915373 |   869 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   915373 |   870 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   871 | `			ph7_value *pMemObj;` |
|        - |   872 | `			/* Reserve a memory object for this constant/static attribute */` |
|     4027 |   873 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     4027 |   874 | `			if( pMemObj == 0 ){` |
|      ! 0 |   875 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   876 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   877 | `					&pClass->sName,&pAttr->sName` |
|        - |   878 | `					);` |
|      ! 0 |   879 | `				return SXERR_MEM;` |
|        - |   880 | `			}` |
|     4027 |   881 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   882 | `				/* Initialize attribute default value (any complex expression) */` |
|     4023 |   883 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj,FALSE);` |
|        - |   884 | `				/* Typed class constant (PHP 8.3): enforce the computed value` |
|        - |   885 | `				 * against the declared type. A mismatch is a non-catchable` |
|        - |   886 | `				 * fatal, raised here at definition time (matching PHP). */` |
|     6027 |   887 | `				if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|     2014 |   888 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|     1087 |   889 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj);` |
|     1087 |   890 | `					if( rcType != SXRET_OK ){` |
|        6 |   891 | `						return rcType;` |
|        - |   892 | `					}` |
|      540 |   893 | `				}` |
|     2007 |   894 | `			}` |
|        - |   895 | `			/* Record attribute index */` |
|     4022 |   896 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   897 | `			/* Install static attribute in the reference table */` |
|     4022 |   898 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   899 | `			/* If this is a typed static property, register the slot so the` |
|        - |   900 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   901 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   902 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|        - |   903 | `			 * Typed *constants* are excluded — they are immutable and were` |
|        - |   904 | `			 * already enforced above, so they need no store-time slot. */` |
|     4018 |   905 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     2560 |   906 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       17 |   907 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       17 |   908 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   909 | `					return SXERR_MEM;` |
|        - |   910 | `				}` |
|       17 |   911 | `				pVmAttrS->pAttr = pAttr;` |
|       17 |   912 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       17 |   913 | `				pVmAttrS->iState = 0;` |
|       17 |   914 | `				pVmAttrS->pOwner = pClass;` |
|        - |   915 | `				/* Static typed property with no default starts uninitialized` |
|        - |   916 | `				 * (constants are already excluded by the enclosing condition). */` |
|       17 |   917 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        6 |   918 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   919 | `				}` |
|       17 |   920 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   921 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   922 | `					return SXERR_MEM;` |
|        - |   923 | `				}` |
|        7 |   924 | `			}` |
|     2009 |   925 | `		}` |
|        5 |   926 | `	}` |
|   371121 |   927 | `	return SXRET_OK;` |
|   185565 |   928 |  |
|   370888 |   929 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   930 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   931 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   932 | `	)` |
|        5 |   933 |  |
|        - |   934 | `	ph7_class_method *pMeth;` |
|        - |   935 | `	SyHashEntry *pEntry;` |
|        - |   936 | `	sxi32 rc;` |
|        - |   937 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   370893 |   938 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   370893 |   939 | `	if( rc != SXRET_OK ){` |
|        6 |   940 | `		return rc;` |
|        - |   941 | `	}` |
|        - |   942 | `	/* Install class methods */` |
|   370889 |   943 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   944 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   945 | `		 */` |
|   200783 |   946 | `		return SXRET_OK;` |
|        - |   947 | `	}` |
|        - |   948 | `	/* Create constructor alias if not yet done */` |
|   170111 |   949 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   950 | `		/* User constructor with the same base class name */` |
|     7013 |   951 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     7013 |   952 | `		if( pEntry ){` |
|      ! 0 |   953 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   954 | `			/* Create the alias */` |
|      ! 0 |   955 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   956 | `		}` |
|     3504 |   957 | `	}` |
|        - |   958 | `	/* Install the methods now */` |
|   170111 |   959 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  1741596 |   960 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  1486437 |   961 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  1486437 |   962 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  1486429 |   963 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  1486429 |   964 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   965 | `				return rc;` |
|        - |   966 | `			}` |
|   743212 |   967 | `		}` |
|        5 |   968 | `	}` |
|        - |   969 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   170111 |   970 | `	pClass->bMounted = TRUE;` |
|   170111 |   971 | `	return SXRET_OK;` |
|   185449 |   972 |  |
|        - |   973 | `/*` |
|        - |   974 | ` * Allocate a private frame for attributes of the given` |
|        - |   975 | ` * class instance (Object in the PHP jargon).` |
|        - |   976 | ` */` |
|     2296 |   977 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   978 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   979 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   980 | `	)` |
|        5 |   981 |  |
|     2301 |   982 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   983 | `	ph7_class_attr *pAttr;` |
|        - |   984 | `	SyHashEntry *pEntry;` |
|        - |   985 | `	sxi32 rc;` |
|        - |   986 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2301 |   987 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     9635 |   988 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   989 | `		VmClassAttr *pVmAttr;` |
|        - |   990 | `		/* Extract the current attribute */` |
|     7339 |   991 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7339 |   992 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     7339 |   993 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   994 | `			return SXERR_MEM;` |
|        - |   995 | `		}` |
|     7339 |   996 | `		pVmAttr->pAttr = pAttr;` |
|     7339 |   997 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   998 | `			ph7_value *pMemObj;` |
|        - |   999 | `			/* Reserve a memory object for this attribute */` |
|     7313 |  1000 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     7313 |  1001 | `			if( pMemObj == 0 ){` |
|      ! 0 |  1002 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  1003 | `				return SXERR_MEM;` |
|        - |  1004 | `			}` |
|     7313 |  1005 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     7313 |  1006 | `			pVmAttr->iState = 0;` |
|     7313 |  1007 | `			pVmAttr->pOwner = pClass;` |
|     7313 |  1008 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |  1009 | `				/* Initialize attribute default value (any complex expression) */` |
|     2497 |  1010 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj,FALSE);` |
|     6067 |  1011 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |  1012 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |  1013 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|      127 |  1014 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       61 |  1015 | `			}` |
|     7313 |  1016 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     7313 |  1017 | `			if( rc != SXRET_OK ){` |
|        - |  1018 | `				VmSlot sSlot;` |
|        - |  1019 | `				/* Restore memory object */` |
|      ! 0 |  1020 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |  1021 | `				sSlot.pUserData = 0;` |
|      ! 0 |  1022 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |  1023 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  1024 | `				return SXERR_MEM;` |
|        - |  1025 | `			}` |
|        - |  1026 | `			/* Install attribute in the reference table */` |
|     7313 |  1027 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |  1028 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |  1029 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |  1030 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     7313 |  1031 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      237 |  1032 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      237 |  1033 | `				if( rc != SXRET_OK ){` |
|        - |  1034 | `					VmSlot sSlot;` |
|      ! 0 |  1035 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |  1036 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |  1037 | `					sSlot.pUserData = 0;` |
|      ! 0 |  1038 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |  1039 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  1040 | `					return SXERR_MEM;` |
|        - |  1041 | `				}` |
|      116 |  1042 | `			}` |
|     3659 |  1043 | `		}else{` |
|        - |  1044 | `			/* Install static/constant attribute */` |
|       29 |  1045 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       29 |  1046 | `			pVmAttr->iState = 0;` |
|       29 |  1047 | `			pVmAttr->pOwner = pClass;` |
|       29 |  1048 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       29 |  1049 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  1050 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  1051 | `				return SXERR_MEM;` |
|        - |  1052 | `			}` |
|        - |  1053 | `		}` |
|        5 |  1054 | `	}` |
|     2301 |  1055 | `	return SXRET_OK;` |
|     1153 |  1056 |  |
|        - |  1057 | `/* Forward declaration */` |
|        - |  1058 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |  1059 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |  1060 | `/*` |
|        - |  1061 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |  1062 | ` */` |
|        - |  1063 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |  1064 | `/*` |
|        - |  1065 | ` * Reserve a constant memory object.` |
|        - |  1066 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1067 | ` */` |
|   479238 |  1068 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 |  1069 |  |
|        - |  1070 | `	ph7_value *pObj;` |
|        - |  1071 | `	sxi32 rc;` |
|   479243 |  1072 | `	if( pIndex ){` |
|        - |  1073 | `		/* Object index in the object table */` |
|   469331 |  1074 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   234663 |  1075 | `	}` |
|        - |  1076 | `	/* Reserve a slot for the new object */` |
|   479243 |  1077 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   479243 |  1078 | `	if( rc != SXRET_OK ){` |
|        - |  1079 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1080 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1081 | `		 */` |
|      ! 0 |  1082 | `		return 0;` |
|        - |  1083 | `	}` |
|   479243 |  1084 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   479243 |  1085 | `	return pObj;` |
|   239624 |  1086 |  |
|        - |  1087 | `/*` |
|        - |  1088 | ` * Reserve a memory object.` |
|        - |  1089 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1090 | ` */` |
|  2175274 |  1091 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 |  1092 |  |
|        - |  1093 | `	ph7_value *pObj;` |
|        - |  1094 | `	sxi32 rc;` |
|  2175279 |  1095 | `	if( pIndex ){` |
|        - |  1096 | `		/* Object index in the object table */` |
|  2175279 |  1097 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1087637 |  1098 | `	}` |
|        - |  1099 | `	/* Reserve a slot for the new object */` |
|  2175279 |  1100 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2175279 |  1101 | `	if( rc != SXRET_OK ){` |
|        - |  1102 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1103 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1104 | `		 */` |
|      ! 0 |  1105 | `		return 0;` |
|        - |  1106 | `	}` |
|  2175279 |  1107 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2175279 |  1108 | `	return pObj;` |
|  1087642 |  1109 |  |
|        - |  1110 | `/* Forward declaration */` |
|        - |  1111 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |  1112 | `/* Forward declarations for Fiber C functions */` |
|        - |  1113 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1114 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1115 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1116 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1117 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1118 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1119 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1120 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1121 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1122 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1123 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |  1124 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |  1125 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1126 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  1127 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |  1128 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1129 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |  1130 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |  1131 | `static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1132 | `	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);` |
|        - |  1133 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);` |
|        - |  1134 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |  1135 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1136 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |  1137 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1138 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1139 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1140 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1141 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1142 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1143 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1144 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1145 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1146 | `/*` |
|        - |  1147 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  1148 | ` * directly as foreign functions.` |
|        - |  1149 | ` */` |
|        - |  1150 | `#define PH7_BUILTIN_LIB \` |
|        - |  1151 | `	"interface Throwable {"\` |
|        - |  1152 | `	"public function getMessage();"\` |
|        - |  1153 | `	"public function getCode();"\` |
|        - |  1154 | `	"public function getFile();"\` |
|        - |  1155 | `	"public function getLine();"\` |
|        - |  1156 | `	"public function getTrace();"\` |
|        - |  1157 | `	"public function getTraceAsString();"\` |
|        - |  1158 | `	"public function getPrevious();"\` |
|        - |  1159 | `	"public function __toString();"\` |
|        - |  1160 | `	"}"\` |
|        - |  1161 | `	"interface Traversable {}"\` |
|        - |  1162 | `	"interface ArrayAccess {"\` |
|        - |  1163 | `	"public function offsetExists($offset);"\` |
|        - |  1164 | `	"public function offsetGet($offset);"\` |
|        - |  1165 | `	"public function offsetSet($offset, $value);"\` |
|        - |  1166 | `	"public function offsetUnset($offset);"\` |
|        - |  1167 | `	"}"\` |
|        - |  1168 | `	"interface Countable {"\` |
|        - |  1169 | `	"public function count();"\` |
|        - |  1170 | `	"}"\` |
|        - |  1171 | `	"interface Stringable {"\` |
|        - |  1172 | `	"public function __toString();"\` |
|        - |  1173 | `	"}"\` |
|        - |  1174 | `	"interface JsonSerializable {"\` |
|        - |  1175 | `	"public function jsonSerialize();"\` |
|        - |  1176 | `	"}"\` |
|        - |  1177 | `	"interface UnitEnum {"\` |
|        - |  1178 | `	"public static function cases();"\` |
|        - |  1179 | `	"}"\` |
|        - |  1180 | `	"interface BackedEnum extends UnitEnum {"\` |
|        - |  1181 | `	"public static function from($value);"\` |
|        - |  1182 | `	"public static function tryFrom($value);"\` |
|        - |  1183 | `	"}"\` |
|        - |  1184 | `	"class Exception implements Throwable { "\` |
|        - |  1185 | `    "protected $message = '';"\` |
|        - |  1186 | `    "protected $code = 0;"\` |
|        - |  1187 | `    "protected $file;"\` |
|        - |  1188 | `    "protected $line;"\` |
|        - |  1189 | `    "protected $trace;"\` |
|        - |  1190 | `    "protected $previous;"\` |
|        - |  1191 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1192 | `	"   if( isset($message) ){"\` |
|        - |  1193 | `	"	  $this->message = $message;"\` |
|        - |  1194 | `	"   }"\` |
|        - |  1195 | `	"   $this->code = $code;"\` |
|        - |  1196 | `	"   $this->file = __FILE__;"\` |
|        - |  1197 | `	"   $this->line = __LINE__;"\` |
|        - |  1198 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1199 | `	"   if( isset($previous) ){"\` |
|        - |  1200 | `	"     $this->previous = $previous;"\` |
|        - |  1201 | `	"   }"\` |
|        - |  1202 | `	"}"\` |
|        - |  1203 | `	"public function getMessage(){"\` |
|        - |  1204 | `	"   return $this->message;"\` |
|        - |  1205 | `	"}"\` |
|        - |  1206 | `	" public function getCode(){"\` |
|        - |  1207 | `	"  return $this->code;"\` |
|        - |  1208 | `	"}"\` |
|        - |  1209 | `	"public function getFile(){"\` |
|        - |  1210 | `	"  return $this->file;"\` |
|        - |  1211 | `	"}"\` |
|        - |  1212 | `	"public function getLine(){"\` |
|        - |  1213 | `	"  return $this->line;"\` |
|        - |  1214 | `	"}"\` |
|        - |  1215 | `	"public function getTrace(){"\` |
|        - |  1216 | `	"   return $this->trace;"\` |
|        - |  1217 | `	"}"\` |
|        - |  1218 | `	"public function getTraceAsString(){"\` |
|        - |  1219 | `	"  return debug_string_backtrace();"\` |
|        - |  1220 | `	"}"\` |
|        - |  1221 | `	"public function getPrevious(){"\` |
|        - |  1222 | `	"    return $this->previous;"\` |
|        - |  1223 | `	"}"\` |
|        - |  1224 | `	"public function __toString(){"\` |
|        - |  1225 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1226 | `    "}"\` |
|        - |  1227 | `	"}"\` |
|        - |  1228 | `	"class Error implements Throwable { "\` |
|        - |  1229 | `    "protected $message = '';"\` |
|        - |  1230 | `    "protected $code = 0;"\` |
|        - |  1231 | `    "protected $file;"\` |
|        - |  1232 | `    "protected $line;"\` |
|        - |  1233 | `    "protected $trace;"\` |
|        - |  1234 | `    "protected $previous;"\` |
|        - |  1235 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1236 | `	"   if( isset($message) ){"\` |
|        - |  1237 | `	"	  $this->message = $message;"\` |
|        - |  1238 | `	"   }"\` |
|        - |  1239 | `	"   $this->code = $code;"\` |
|        - |  1240 | `	"   $this->file = __FILE__;"\` |
|        - |  1241 | `	"   $this->line = __LINE__;"\` |
|        - |  1242 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1243 | `	"   if( isset($previous) ){"\` |
|        - |  1244 | `	"     $this->previous = $previous;"\` |
|        - |  1245 | `	"   }"\` |
|        - |  1246 | `	"}"\` |
|        - |  1247 | `	"public function getMessage(){"\` |
|        - |  1248 | `	"   return $this->message;"\` |
|        - |  1249 | `	"}"\` |
|        - |  1250 | `	"public function getCode(){"\` |
|        - |  1251 | `	"  return $this->code;"\` |
|        - |  1252 | `	"}"\` |
|        - |  1253 | `	"public function getFile(){"\` |
|        - |  1254 | `	"  return $this->file;"\` |
|        - |  1255 | `	"}"\` |
|        - |  1256 | `	"public function getLine(){"\` |
|        - |  1257 | `	"  return $this->line;"\` |
|        - |  1258 | `	"}"\` |
|        - |  1259 | `	"public function getTrace(){"\` |
|        - |  1260 | `	"   return $this->trace;"\` |
|        - |  1261 | `	"}"\` |
|        - |  1262 | `	"public function getTraceAsString(){"\` |
|        - |  1263 | `	"  return debug_string_backtrace();"\` |
|        - |  1264 | `	"}"\` |
|        - |  1265 | `	"public function getPrevious(){"\` |
|        - |  1266 | `	"    return $this->previous;"\` |
|        - |  1267 | `	"}"\` |
|        - |  1268 | `	"public function __toString(){"\` |
|        - |  1269 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1270 | `	"}"\` |
|        - |  1271 | `	"}"\` |
|        - |  1272 | `	"class TypeError extends Error { }"\` |
|        - |  1273 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1274 | `	"class ValueError extends Error { }"\` |
|        - |  1275 | `	"class FiberError extends Error { }"\` |
|        - |  1276 | `	"class AssertionError extends Error { }"\` |
|        - |  1277 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1278 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1279 | `	"class ErrorException extends Exception { "\` |
|        - |  1280 | `	"protected $severity;"\` |
|        - |  1281 | `	"public function __construct(string $message = null,"\` |
|        - |  1282 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1283 | `	"   if( isset($message) ){"\` |
|        - |  1284 | `	"	  $this->message = $message;"\` |
|        - |  1285 | `	"   }"\` |
|        - |  1286 | `	"   $this->severity = $severity;"\` |
|        - |  1287 | `	"   $this->code = $code;"\` |
|        - |  1288 | `	"   $this->file = $filename;"\` |
|        - |  1289 | `	"   $this->line = $lineno;"\` |
|        - |  1290 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1291 | `	"   if( isset($previous) ){"\` |
|        - |  1292 | `	"     $this->previous = $previous;"\` |
|        - |  1293 | `	"   }"\` |
|        - |  1294 | `	"}"\` |
|        - |  1295 | `	"public function getSeverity(){"\` |
|        - |  1296 | `	"   return $this->severity;"\` |
|        - |  1297 | `    "}"\` |
|        - |  1298 | `	"}"\` |
|        - |  1299 | `	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\` |
|        - |  1300 | `	"class LogicException extends Exception { }"\` |
|        - |  1301 | `	"class RuntimeException extends Exception { }"\` |
|        - |  1302 | `	"class BadFunctionCallException extends LogicException { }"\` |
|        - |  1303 | `	"class BadMethodCallException extends BadFunctionCallException { }"\` |
|        - |  1304 | `	"class DomainException extends LogicException { }"\` |
|        - |  1305 | `	"class InvalidArgumentException extends LogicException { }"\` |
|        - |  1306 | `	"class LengthException extends LogicException { }"\` |
|        - |  1307 | `	"class OutOfRangeException extends LogicException { }"\` |
|        - |  1308 | `	"class OutOfBoundsException extends RuntimeException { }"\` |
|        - |  1309 | `	"class OverflowException extends RuntimeException { }"\` |
|        - |  1310 | `	"class RangeException extends RuntimeException { }"\` |
|        - |  1311 | `	"class UnderflowException extends RuntimeException { }"\` |
|        - |  1312 | `	"class UnexpectedValueException extends RuntimeException { }"\` |
|        - |  1313 | `	"interface Iterator extends Traversable {"\` |
|        - |  1314 | `	"public function current();"\` |
|        - |  1315 | `	"public function key();"\` |
|        - |  1316 | `	"public function next();"\` |
|        - |  1317 | `	"public function rewind();"\` |
|        - |  1318 | `	"public function valid();"\` |
|        - |  1319 | `	"}"\` |
|        - |  1320 | `	"interface IteratorAggregate extends Traversable {"\` |
|        - |  1321 | `	"public function getIterator();"\` |
|        - |  1322 | `	"}"\` |
|        - |  1323 | `	"interface Serializable {"\` |
|        - |  1324 | `	"public function serialize();"\` |
|        - |  1325 | `	"public function unserialize(string $serialized);"\` |
|        - |  1326 | `	"}"\` |
|        - |  1327 | `	"/* Directory releated IO */"\` |
|        - |  1328 | `	"class Directory {"\` |
|        - |  1329 | `	"public $handle = null;"\` |
|        - |  1330 | `	"public $path  = null;"\` |
|        - |  1331 | `	"public function __construct(string $path)"\` |
|        - |  1332 | `	"{"\` |
|        - |  1333 | `	"   $this->handle = opendir($path);"\` |
|        - |  1334 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1335 | `	"      $this->path = $path;"\` |
|        - |  1336 | `	"   }"\` |
|        - |  1337 | `	"}"\` |
|        - |  1338 | `	"public function __destruct()"\` |
|        - |  1339 | `	"{"\` |
|        - |  1340 | `	"  if( $this->handle != null ){"\` |
|        - |  1341 | `	"       closedir($this->handle);"\` |
|        - |  1342 | `	"  }"\` |
|        - |  1343 | `	"}"\` |
|        - |  1344 | `	"public function read()"\` |
|        - |  1345 | `	"{"\` |
|        - |  1346 | `	"    return readdir($this->handle);"\` |
|        - |  1347 | `	"}"\` |
|        - |  1348 | `	"public function rewind()"\` |
|        - |  1349 | `	"{"\` |
|        - |  1350 | `	"    rewinddir($this->handle);"\` |
|        - |  1351 | `	"}"\` |
|        - |  1352 | `	"public function close()"\` |
|        - |  1353 | `	"{"\` |
|        - |  1354 | `	"    closedir($this->handle);"\` |
|        - |  1355 | `	"    $this->handle = null;"\` |
|        - |  1356 | `	"}"\` |
|        - |  1357 | `	"}"\` |
|        - |  1358 | `	"class Fiber {"\` |
|        - |  1359 | `	"  private $__ctx;"\` |
|        - |  1360 | `	"  private $__callable;"\` |
|        - |  1361 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1362 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1363 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1364 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1365 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1366 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1367 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1368 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1369 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1370 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1371 | `	"}"\` |
|        - |  1372 | `	"class Generator implements Iterator {"\` |
|        - |  1373 | `	"  private $__ctx;"\` |
|        - |  1374 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1375 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1376 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1377 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1378 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1379 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1380 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1381 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1382 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1383 | `	"}"\` |
|        - |  1384 | `	"class stdClass{"\` |
|        - |  1385 | `	"  public $value;"\` |
|        - |  1386 | `	" /* Magic methods */"\` |
|        - |  1387 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1388 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1389 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1390 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1391 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1392 | `	"}"\` |
|        - |  1393 | `	"function dir(string $path){"\` |
|        - |  1394 | `	"   return new Directory($path);"\` |
|        - |  1395 | `	"}"\` |
|        - |  1396 | `	"function Dir(string $path){"\` |
|        - |  1397 | `	"   return new Directory($path);"\` |
|        - |  1398 | `	"}"\` |
|        - |  1399 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1400 | `    "{"\` |
|        - |  1401 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1402 | `	"  $aDir = array();"\` |
|        - |  1403 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1404 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1405 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1406 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1407 | `	"   }"\` |
|        - |  1408 | `	"  closedir($pHandle);"\` |
|        - |  1409 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1410 | `	"      rsort($aDir);"\` |
|        - |  1411 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1412 | `	"      sort($aDir);"\` |
|        - |  1413 | `	"  }"\` |
|        - |  1414 | `	"  return $aDir;"\` |
|        - |  1415 | `	"}"\` |
|        - |  1416 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1417 | `	"/* Open the target directory */"\` |
|        - |  1418 | `	"$zDir = dirname($pattern);"\` |
|        - |  1419 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1420 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1421 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1422 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1423 | `	"	return FALSE;"\` |
|        - |  1424 | `	"}"\` |
|        - |  1425 | `	"$pattern = basename($pattern);"\` |
|        - |  1426 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1427 | `	"/* Loop throw available entries */"\` |
|        - |  1428 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1429 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1430 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1431 | `	"	if( $rc ){"\` |
|        - |  1432 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1433 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1434 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1435 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1436 | `	"		  }"\` |
|        - |  1437 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1438 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1439 | `	"		 continue;"\` |
|        - |  1440 | `	"	   }"\` |
|        - |  1441 | `	"	   /* Add the entry */"\` |
|        - |  1442 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1443 | `	"	}"\` |
|        - |  1444 | `	" }"\` |
|        - |  1445 | `	"/* Close the handle */"\` |
|        - |  1446 | `	"closedir($pHandle);"\` |
|        - |  1447 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1448 | `	"  /* Sort the array */"\` |
|        - |  1449 | `	"  sort($pArray);"\` |
|        - |  1450 | `	"}"\` |
|        - |  1451 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1452 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1453 | `	"  $pArray[] = $pattern;"\` |
|        - |  1454 | `	"}"\` |
|        - |  1455 | `	"/* Return the created array */"\` |
|        - |  1456 | `	"return $pArray;"\` |
|        - |  1457 | `   "}"\` |
|        - |  1458 | `   "/* Creates a temporary file */"\` |
|        - |  1459 | `   "function tmpfile(){"\` |
|        - |  1460 | `   "  /* Extract the temp directory */"\` |
|        - |  1461 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1462 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1463 | `   "    /* Use the current dir */"\` |
|        - |  1464 | `   "    $zTempDir = '.';"\` |
|        - |  1465 | `   "  }"\` |
|        - |  1466 | `   "  /* Create the file */"\` |
|        - |  1467 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1468 | `   "  return $pHandle;"\` |
|        - |  1469 | `   "}"\` |
|        - |  1470 | `   "/* Creates a temporary filename */"\` |
|        - |  1471 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1472 | `   "{"\` |
|        - |  1473 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1474 | `   "}"\` |
|        - |  1475 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1476 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1477 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1478 | `   "/* Copy arguments */"\` |
|        - |  1479 | `   "$nArgs = func_num_args();"\` |
|        - |  1480 | `   "$pNew = array();"\` |
|        - |  1481 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1482 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1483 | `    "}"\` |
|        - |  1484 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1485 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1486 | `	"/* Erase */"\` |
|        - |  1487 | `	"array_erase($pArray);"\` |
|        - |  1488 | `	"/* Unshift */"\` |
|        - |  1489 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1490 | `	"return sizeof($pArray);"\` |
|        - |  1491 | `    "}"\` |
|        - |  1492 | `	"function array_merge_recursive(){"\` |
|        - |  1493 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1494 | `    "$arrays = func_get_args();"\` |
|        - |  1495 | `    "$narrays = count($arrays);"\` |
|        - |  1496 | `    "$ret = array();"\` |
|        - |  1497 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1498 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1499 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1500 | `	 " }"\` |
|        - |  1501 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1502 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1503 | `     "  if( $keyIsInt ) {"\` |
|        - |  1504 | `     "   $ret[] = $value;"\` |
|        - |  1505 | `     "  } else {"\` |
|        - |  1506 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1507 | `     "    $cur = $ret[$key];"\` |
|        - |  1508 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1509 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1510 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1511 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1512 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1513 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1514 | `     "    } else {"\` |
|        - |  1515 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1516 | `     "    }"\` |
|        - |  1517 | `     "   } else {"\` |
|        - |  1518 | `     "    $ret[$key] = $value;"\` |
|        - |  1519 | `     "   }"\` |
|        - |  1520 | `     "  }"\` |
|        - |  1521 | `     " }"\` |
|        - |  1522 | `	 " }"\` |
|        - |  1523 | `	 " return $ret;"\` |
|        - |  1524 | `    "}"\` |
|        - |  1525 | `	"function max(){"\` |
|        - |  1526 | `    "  $pArgs = func_get_args();"\` |
|        - |  1527 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1528 | `	"  return null;"\` |
|        - |  1529 | `    " }"\` |
|        - |  1530 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1531 | `    " $pArg = $pArgs[0];"\` |
|        - |  1532 | `	" if( !is_array($pArg) ){"\` |
|        - |  1533 | `	"   return $pArg; "\` |
|        - |  1534 | `	" }"\` |
|        - |  1535 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1536 | `	"   return null;"\` |
|        - |  1537 | `	" }"\` |
|        - |  1538 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1539 | `	" reset($pArg);"\` |
|        - |  1540 | `	" $max = current($pArg);"\` |
|        - |  1541 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1542 | `	"   if( $val > $max ){"\` |
|        - |  1543 | `	"     $max = $val;"\` |
|        - |  1544 | `    " }"\` |
|        - |  1545 | `	" }"\` |
|        - |  1546 | `	" return $max;"\` |
|        - |  1547 | `    " }"\` |
|        - |  1548 | `    " $max = $pArgs[0];"\` |
|        - |  1549 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1550 | `    " $val = $pArgs[$i];"\` |
|        - |  1551 | `	"if( $val > $max ){"\` |
|        - |  1552 | `	" $max = $val;"\` |
|        - |  1553 | `	"}"\` |
|        - |  1554 | `    " }"\` |
|        - |  1555 | `	" return $max;"\` |
|        - |  1556 | `    "}"\` |
|        - |  1557 | `	"function min(){"\` |
|        - |  1558 | `    "  $pArgs = func_get_args();"\` |
|        - |  1559 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1560 | `	"  return null;"\` |
|        - |  1561 | `    " }"\` |
|        - |  1562 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1563 | `    " $pArg = $pArgs[0];"\` |
|        - |  1564 | `	" if( !is_array($pArg) ){"\` |
|        - |  1565 | `	"   return $pArg; "\` |
|        - |  1566 | `	" }"\` |
|        - |  1567 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1568 | `	"   return null;"\` |
|        - |  1569 | `	" }"\` |
|        - |  1570 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1571 | `	" reset($pArg);"\` |
|        - |  1572 | `	" $min = current($pArg);"\` |
|        - |  1573 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1574 | `	"   if( $val < $min ){"\` |
|        - |  1575 | `	"     $min = $val;"\` |
|        - |  1576 | `    " }"\` |
|        - |  1577 | `	" }"\` |
|        - |  1578 | `	" return $min;"\` |
|        - |  1579 | `    " }"\` |
|        - |  1580 | `    " $min = $pArgs[0];"\` |
|        - |  1581 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1582 | `    " $val = $pArgs[$i];"\` |
|        - |  1583 | `	"if( $val < $min ){"\` |
|        - |  1584 | `	" $min = $val;"\` |
|        - |  1585 | `	" }"\` |
|        - |  1586 | `    " }"\` |
|        - |  1587 | `	" return $min;"\` |
|        - |  1588 | `	"}"\` |
|        - |  1589 | `	"function fileowner(string $file){"\` |
|        - |  1590 | `    " $a = stat($file);"\` |
|        - |  1591 | `	" if( !is_array($a) ){"\` |
|        - |  1592 | `	"	return false;"\` |
|        - |  1593 | `	" }"\` |
|        - |  1594 | `	" return $a['uid'];"\` |
|        - |  1595 | `    "}"\` |
|        - |  1596 | `    "function filegroup(string $file){"\` |
|        - |  1597 | `	" $a = stat($file);"\` |
|        - |  1598 | `	" if( !is_array($a) ){"\` |
|        - |  1599 | `	"	return false;"\` |
|        - |  1600 | `	" }"\` |
|        - |  1601 | `	" return $a['gid'];"\` |
|        - |  1602 | `    "}"\` |
|        - |  1603 | `	 "function fileinode(string $file){"\` |
|        - |  1604 | `	" $a = stat($file);"\` |
|        - |  1605 | `	" if( !is_array($a) ){"\` |
|        - |  1606 | `	"	return false;"\` |
|        - |  1607 | `	" }"\` |
|        - |  1608 | `	" return $a['ino'];"\` |
|        - |  1609 | `    "}"` |
|        - |  1610 |  |
|        - |  1611 | `/*` |
|        - |  1612 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1613 | ` * start compiling the target PHP program.` |
|        - |  1614 | ` */` |
|     3304 |  1615 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1616 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1617 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1618 | `	 )` |
|        5 |  1619 |  |
|        - |  1620 | `	SyString sBuiltin;` |
|        - |  1621 | `	ph7_value *pObj;` |
|        - |  1622 | `	sxi32 rc;` |
|        - |  1623 | `	/* Zero the structure */` |
|     3309 |  1624 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1625 | `	/* Initialize VM fields */` |
|     3309 |  1626 | `	pVm->pEngine = &(*pEngine);` |
|     3309 |  1627 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1628 | `	/* Instructions containers */` |
|     3309 |  1629 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3309 |  1630 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3309 |  1631 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1632 | `	/* Object containers */` |
|     3309 |  1633 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3309 |  1634 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1635 | `	/* Virtual machine internal containers */` |
|     3309 |  1636 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3309 |  1637 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3309 |  1638 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3309 |  1639 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3309 |  1640 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3309 |  1641 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3309 |  1642 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3309 |  1643 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3309 |  1644 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3309 |  1645 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3309 |  1646 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3309 |  1647 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3309 |  1648 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3309 |  1649 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3309 |  1650 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3309 |  1651 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3309 |  1652 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3309 |  1653 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3309 |  1654 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3309 |  1655 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3309 |  1656 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3309 |  1657 | `	pVm->pPendingException = 0;` |
|        - |  1658 | `	/* Configuration containers */` |
|     3309 |  1659 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3309 |  1660 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3309 |  1661 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3309 |  1662 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3309 |  1663 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3309 |  1664 | `	pVm->iResponseStatus = 200;` |
|     3309 |  1665 | `	pVm->bHeadersSent = 0;` |
|     3309 |  1666 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1667 | `	/* Error callbacks containers */` |
|     3309 |  1668 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3309 |  1669 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3309 |  1670 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3309 |  1671 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3309 |  1672 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1673 | `	/* Set a default recursion limit */` |
|        - |  1674 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3309 |  1675 | `	pVm->nMaxDepth = 32;` |
|        - |  1676 | `#else` |
|        - |  1677 | `	pVm->nMaxDepth = 16;` |
|        - |  1678 | `#endif` |
|        - |  1679 | `	/* Default assertion flags */` |
|     3309 |  1680 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1681 | `	/* JSON return status */` |
|     3309 |  1682 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1683 | `	/* PRNG context */` |
|     3309 |  1684 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1685 | `	/* Install the null constant */` |
|     3309 |  1686 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3309 |  1687 | `	if( pObj == 0 ){` |
|      ! 0 |  1688 | `		rc = SXERR_MEM;` |
|      ! 0 |  1689 | `		goto Err;` |
|        - |  1690 | `	}` |
|     3309 |  1691 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1692 | `	/* Install the boolean TRUE constant */` |
|     3309 |  1693 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3309 |  1694 | `	if( pObj == 0 ){` |
|      ! 0 |  1695 | `		rc = SXERR_MEM;` |
|      ! 0 |  1696 | `		goto Err;` |
|        - |  1697 | `	}` |
|     3309 |  1698 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1699 | `	/* Install the boolean FALSE constant */` |
|     3309 |  1700 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3309 |  1701 | `	if( pObj == 0 ){` |
|      ! 0 |  1702 | `		rc = SXERR_MEM;` |
|      ! 0 |  1703 | `		goto Err;` |
|        - |  1704 | `	}` |
|     3309 |  1705 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1706 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1707 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1708 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3309 |  1709 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3309 |  1710 | `	if( pObj == 0 ){` |
|      ! 0 |  1711 | `		rc = SXERR_MEM;` |
|      ! 0 |  1712 | `		goto Err;` |
|        - |  1713 | `	}` |
|     3309 |  1714 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1715 | `	/* Create the global frame */` |
|     3309 |  1716 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3309 |  1717 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1718 | `		goto Err;` |
|        - |  1719 | `	}` |
|        - |  1720 | `	/* Initialize the code generator */` |
|     3309 |  1721 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3309 |  1722 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1723 | `		goto Err;` |
|        - |  1724 | `	}` |
|        - |  1725 | `	/* VM correctly initialized,set the magic number */` |
|     3309 |  1726 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3309 |  1727 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1728 | `	/* Compile the built-in library */` |
|     3309 |  1729 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1730 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3309 |  1731 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1732 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3309 |  1733 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3309 |  1734 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3309 |  1735 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3309 |  1736 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|     3309 |  1737 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|        - |  1738 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3309 |  1739 | `	pVm->pCoalesceObj = 0;` |
|     3309 |  1740 | `	pVm->bCoalesceArmed = 0;` |
|     3309 |  1741 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  1742 | `	/* Register Fiber internal C functions */` |
|     3309 |  1743 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3309 |  1744 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3309 |  1745 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3309 |  1746 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3309 |  1747 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3309 |  1748 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3309 |  1749 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3309 |  1750 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3309 |  1751 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3309 |  1752 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1753 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3309 |  1754 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3309 |  1755 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3309 |  1756 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3309 |  1757 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3309 |  1758 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3309 |  1759 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3309 |  1760 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3309 |  1761 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3309 |  1762 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3309 |  1763 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1764 | `	/* Reset the code generator */` |
|     3309 |  1765 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3309 |  1766 | `	return SXRET_OK;` |
|      ! 0 |  1767 | `Err:` |
|      ! 0 |  1768 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1769 | `	return rc;` |
|     1657 |  1770 |  |
|        - |  1771 | `/*` |
|        - |  1772 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1773 | ` * routine which store the output in an internal blob.` |
|        - |  1774 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1775 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1776 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1777 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1778 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1779 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1780 | ` * to finish executing and extracting the output.` |
|        - |  1781 | ` */` |
|       58 |  1782 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1783 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1784 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1785 | `	void *pUserData     /* User private data */` |
|        - |  1786 | `	)` |
|      ! 0 |  1787 |  |
|        - |  1788 | `	 sxi32 rc;` |
|        - |  1789 | `	 /* Store the output in an internal BLOB */` |
|       58 |  1790 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       58 |  1791 | `	 return rc;` |
|      ! 0 |  1792 |  |
|        - |  1793 | `/*` |
|        - |  1794 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1795 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1796 | ` */` |
|    21546 |  1797 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        5 |  1798 |  |
|    21551 |  1799 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    21551 |  1800 | `	if( xCons != VmObConsumer ){` |
|     8551 |  1801 | `		pVm->nOutputLen += nLen;` |
|     8551 |  1802 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1077 |  1803 | `			pVm->bHeadersSent = 1;` |
|      536 |  1804 | `		}` |
|     4273 |  1805 | `	}` |
|    21551 |  1806 |  |
|        - |  1807 | `#define VM_STACK_GUARD 16` |
|        - |  1808 | `/*` |
|        - |  1809 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1810 | ` * our compiled PHP program.` |
|        - |  1811 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1812 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1813 | ` */` |
|    50396 |  1814 | `static ph7_value * VmNewOperandStack(` |
|        - |  1815 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1816 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1817 | `	)` |
|        5 |  1818 |  |
|        - |  1819 | `	ph7_value *pStack;` |
|        - |  1820 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1821 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1822 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1823 | `  ** on the maximum stack depth required.` |
|        - |  1824 | `  **` |
|        - |  1825 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1826 | `  */` |
|    50401 |  1827 | `	nInstr += VM_STACK_GUARD;` |
|    50401 |  1828 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    50401 |  1829 | `	if( pStack == 0 ){` |
|      ! 0 |  1830 | `		return 0;` |
|        - |  1831 | `	}` |
|        - |  1832 | `	/* Initialize the operand stack */` |
|  3323179 |  1833 | `	while( nInstr > 0 ){` |
|  3272783 |  1834 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  3272783 |  1835 | `		--nInstr;` |
|        5 |  1836 | `	}` |
|        - |  1837 | `	/* Ready for bytecode execution */` |
|    50401 |  1838 | `	return pStack;` |
|    25203 |  1839 |  |
|        - |  1840 | `/* Forward declaration */` |
|        - |  1841 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1842 | `/*` |
|        - |  1843 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1844 | ` * This routine gets called by the PH7 engine after` |
|        - |  1845 | ` * successful compilation of the target PHP program.` |
|        - |  1846 | ` */` |
|     2964 |  1847 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1848 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1849 | `	)` |
|        5 |  1850 |  |
|        - |  1851 | `	SyHashEntry *pEntry;` |
|        - |  1852 | `	sxi32 rc;` |
|     2969 |  1853 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1854 | `		/* Initialize your VM first */` |
|      ! 0 |  1855 | `		return SXERR_CORRUPT;` |
|        - |  1856 | `	}` |
|        - |  1857 | `	/* Mark the VM ready for byte-code execution */` |
|     2969 |  1858 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1859 | `	/* Release the code generator now we have compiled our program, but keep its` |
|        - |  1860 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|        - |  1861 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|        - |  1862 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|     2969 |  1863 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|        - |  1864 | `	/* Emit the DONE instruction */` |
|     2969 |  1865 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2969 |  1866 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1867 | `		return SXERR_MEM;` |
|        - |  1868 | `	}` |
|        - |  1869 | `	/* Script return value */` |
|     2969 |  1870 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1871 | `	/* Pending return value from a catch/finally block (see VmThrowException) */` |
|     2969 |  1872 | `	PH7_MemObjInit(&(*pVm),&pVm->sCatchReturn);` |
|     2969 |  1873 | `	pVm->bReturnRequested = 0;` |
|        - |  1874 | `	/* Allocate a new operand stack */` |
|     2969 |  1875 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2969 |  1876 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1877 | `		return SXERR_MEM;` |
|        - |  1878 | `	}` |
|        - |  1879 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1880 | `	 * private data. */` |
|     2969 |  1881 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2969 |  1882 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1883 | `	/* Allocate the reference table */` |
|     2969 |  1884 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2969 |  1885 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2969 |  1886 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1887 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1888 | `		return SXERR_MEM;` |
|        - |  1889 | `	}` |
|        - |  1890 | `	/* Zero the reference table */` |
|     2969 |  1891 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1892 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2969 |  1893 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2969 |  1894 | `	if( rc != SXRET_OK ){` |
|        - |  1895 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1896 | `		return rc;` |
|        - |  1897 | `	}` |
|        - |  1898 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|        - |  1899 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|        - |  1900 | `	 * every object/variable created during execution) is per-exec state that` |
|        - |  1901 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|        - |  1902 | `	 * below it is compile-time/init state that survives a reset. */` |
|     2969 |  1903 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|        - |  1904 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2969 |  1905 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2969 |  1906 | `	if( rc != SXRET_OK ){` |
|        - |  1907 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1908 | `		return rc;` |
|        - |  1909 | `	}` |
|        - |  1910 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2969 |  1911 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1912 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2969 |  1913 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1914 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2969 |  1915 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1916 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1917 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2969 |  1918 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2969 |  1919 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1920 | `#endif` |
|        - |  1921 | `	/* Initialize and install static and constants class attributes.` |
|        - |  1922 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|        - |  1923 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|        - |  1924 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|        - |  1925 | `	 * that function in sync when changing what is reserved here. */` |
|     2969 |  1926 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   115895 |  1927 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   112933 |  1928 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   112933 |  1929 | `		if( rc != SXRET_OK ){` |
|        3 |  1930 | `			return rc;` |
|        - |  1931 | `		}` |
|        5 |  1932 | `	}` |
|        - |  1933 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2967 |  1934 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1935 | `	/* VM is ready for bytecode execution */` |
|     2967 |  1936 | `	return SXRET_OK;` |
|     1487 |  1937 |  |
|        - |  1938 | `/*` |
|        - |  1939 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|        - |  1940 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|        - |  1941 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|        - |  1942 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|        - |  1943 | ` * a dangling node pointer in some other object's reference record.` |
|        - |  1944 | ` */` |
|        6 |  1945 | `static void VmResetRefTable(ph7_vm *pVm)` |
|      ! 0 |  1946 |  |
|        - |  1947 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|        - |  1948 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|        - |  1949 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|      204 |  1950 | `	while( pVm->pRefList ){` |
|      198 |  1951 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|      ! 0 |  1952 | `	}` |
|        6 |  1953 |  |
|        - |  1954 | `/*` |
|        - |  1955 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|        - |  1956 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|        - |  1957 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|        - |  1958 | ` */` |
|       48 |  1959 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|      ! 0 |  1960 |  |
|       48 |  1961 | `	PH7_MemObjRelease(pObj);` |
|       48 |  1962 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|       48 |  1963 |  |
|        - |  1964 | `/*` |
|        - |  1965 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|        - |  1966 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|        - |  1967 | ` * of statics).` |
|        - |  1968 | ` */` |
|      380 |  1969 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|      ! 0 |  1970 |  |
|      380 |  1971 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|        - |  1972 | `	sxu32 k;` |
|      384 |  1973 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|        4 |  1974 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|        2 |  1975 | `	}` |
|      380 |  1976 |  |
|        - |  1977 | `/*` |
|        - |  1978 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|        - |  1979 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|        - |  1980 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|        - |  1981 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|        - |  1982 | ` *    captured environment values, its name buffer and its structure (the` |
|        - |  1983 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|        - |  1984 | ` *    freed). Its template-shared static sentinels are reset too.` |
|        - |  1985 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|        - |  1986 | ` *    has its static sentinels reset.` |
|        - |  1987 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|        - |  1988 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|        - |  1989 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|        - |  1990 | ` */` |
|        6 |  1991 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|      ! 0 |  1992 |  |
|        - |  1993 | `	SyHashEntry *pEntry;` |
|        6 |  1994 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|      386 |  1995 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|      380 |  1996 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      380 |  1997 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - |  1998 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|        - |  1999 | `			 * release its captured-by-value environment, then free the entry,` |
|        - |  2000 | `			 * name buffer and structure. */` |
|        4 |  2001 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        4 |  2002 | `			const char *zName = SyStringData(&pFunc->sName);` |
|        - |  2003 | `			sxu32 k;` |
|        4 |  2004 | `			VmResetFuncStatics(pFunc);` |
|        8 |  2005 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|        4 |  2006 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|        2 |  2007 | `			}` |
|        4 |  2008 | `			SySetRelease(&pFunc->aClosureEnv);` |
|        - |  2009 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|        4 |  2010 | `			SyHashDeleteEntry2(pEntry);` |
|        4 |  2011 | `			if( zName ){` |
|        4 |  2012 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|        2 |  2013 | `			}` |
|        4 |  2014 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|        4 |  2015 | `			continue;` |
|        - |  2016 | `		}` |
|        - |  2017 | `		/* Named function: reset statics for every overload sharing this name. */` |
|      752 |  2018 | `		while( pFunc ){` |
|      376 |  2019 | `			VmResetFuncStatics(pFunc);` |
|      376 |  2020 | `			pFunc = pFunc->pNextName;` |
|      ! 0 |  2021 | `		}` |
|      ! 0 |  2022 | `	}` |
|        6 |  2023 | `	pVm->closure_cnt = 0;` |
|        6 |  2024 |  |
|        - |  2025 | `/*` |
|        - |  2026 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|        - |  2027 | ` * are already gone (each object's destructor removed its own during the object` |
|        - |  2028 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|        - |  2029 | ` * the class re-mount registers fresh ones.` |
|        - |  2030 | ` */` |
|        6 |  2031 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|      ! 0 |  2032 |  |
|        - |  2033 | `	SyHashEntry *pEntry;` |
|        - |  2034 | `	/* Common case: no class static typed properties — table already empty. */` |
|        6 |  2035 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        2 |  2036 | `		return;` |
|        - |  2037 | `	}` |
|        - |  2038 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|        - |  2039 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|        4 |  2040 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|       10 |  2041 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|        4 |  2042 | `		if( pEntry->pUserData ){` |
|        4 |  2043 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|        2 |  2044 | `		}` |
|      ! 0 |  2045 | `	}` |
|        4 |  2046 | `	SyHashRelease(&pVm->hTypedSlot);` |
|        4 |  2047 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|        3 |  2048 |  |
|        - |  2049 | `/*` |
|        - |  2050 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|        - |  2051 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|        - |  2052 | ` *` |
|        - |  2053 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|        - |  2054 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|        - |  2055 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|        - |  2056 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|        - |  2057 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|        - |  2058 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|        - |  2059 | ` *` |
|        - |  2060 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|        - |  2061 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|        - |  2062 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|        - |  2063 | ` * exception/error-handler state, the reference table and every object/array` |
|        - |  2064 | ` * reserved during the run.` |
|        - |  2065 | ` *` |
|        - |  2066 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|        - |  2067 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|        - |  2068 | ` * global-scope destructors never fired.` |
|        - |  2069 | ` */` |
|        6 |  2070 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  2071 |  |
|        - |  2072 | `	sxu32 nWater,n;` |
|        6 |  2073 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  2074 | `		return SXERR_CORRUPT;` |
|        - |  2075 | `	}` |
|        6 |  2076 | `	nWater = pVm->nSuperBaseline;` |
|        - |  2077 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|        - |  2078 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        6 |  2079 | `	pVm->pGlobal = 0;` |
|        - |  2080 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|        - |  2081 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|        - |  2082 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|        - |  2083 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        6 |  2084 | `	pVm->bInReset = 1;` |
|        - |  2085 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        6 |  2086 | `	VmResetRefTable(&(*pVm));` |
|        - |  2087 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|        - |  2088 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|        - |  2089 | `	 * function/class registrations and intentionally persist across reuse (a` |
|        - |  2090 | `	 * re-run define() overwrites the value in place). */` |
|        6 |  2091 | `	VmResetFunctionState(&(*pVm));` |
|        - |  2092 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|        - |  2093 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|      218 |  2094 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|      212 |  2095 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      212 |  2096 | `		if( pObj ){` |
|      212 |  2097 | `			PH7_MemObjRelease(pObj);` |
|      106 |  2098 | `		}` |
|      106 |  2099 | `	}` |
|        - |  2100 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|        - |  2101 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        6 |  2102 | `	VmResetTypedSlots(&(*pVm));` |
|        - |  2103 | `	/* (5) Unwind any active frames back to none. */` |
|       12 |  2104 | `	while( pVm->pFrame ){` |
|        6 |  2105 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 |  2106 | `	}` |
|        - |  2107 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        6 |  2108 | `	pVm->bInReset = 0;` |
|        - |  2109 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|        - |  2110 | `	 * slots (their indices no longer exist). */` |
|        6 |  2111 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        6 |  2112 | `	SySetReset(&pVm->aFreeObj);` |
|        - |  2113 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        6 |  2114 | `	SyHashRelease(&pVm->hSuper);` |
|        6 |  2115 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        - |  2116 | `	/* (8) Drain remaining per-exec containers. */` |
|        6 |  2117 | `	SySetReset(&pVm->aSelf);` |
|        - |  2118 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|        - |  2119 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|        - |  2120 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        6 |  2121 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|      ! 0 |  2122 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|      ! 0 |  2123 | `		if( pCB ){` |
|        - |  2124 | `			int iArg;` |
|      ! 0 |  2125 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2126 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|      ! 0 |  2127 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|      ! 0 |  2128 | `			}` |
|      ! 0 |  2129 | `		}` |
|      ! 0 |  2130 | `	}` |
|        6 |  2131 | `	SySetReset(&pVm->aShutdown);` |
|        6 |  2132 | `	SySetReset(&pVm->aException);` |
|        6 |  2133 | `	pVm->pPendingException = 0;` |
|        6 |  2134 | `	pVm->nExceptDepth = 0;` |
|        - |  2135 | `	/* spl_autoload_register() callbacks are per request */` |
|        6 |  2136 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|      ! 0 |  2137 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      ! 0 |  2138 | `		if( pCB ){` |
|      ! 0 |  2139 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2140 | `		}` |
|      ! 0 |  2141 | `	}` |
|        6 |  2142 | `	SySetReset(&pVm->aAutoload);` |
|        - |  2143 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|        - |  2144 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        6 |  2145 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|      ! 0 |  2146 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|      ! 0 |  2147 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      ! 0 |  2148 | `	}` |
|        - |  2149 | `	/* Output buffers */` |
|        6 |  2150 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|      ! 0 |  2151 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|      ! 0 |  2152 | `		if( pOb ){` |
|      ! 0 |  2153 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|      ! 0 |  2154 | `			SyBlobRelease(&pOb->sOB);` |
|      ! 0 |  2155 | `		}` |
|      ! 0 |  2156 | `	}` |
|        6 |  2157 | `	SySetReset(&pVm->aOB);` |
|        6 |  2158 | `	pVm->nObDepth = 0;` |
|        - |  2159 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|        - |  2160 | `	{` |
|        6 |  2161 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        6 |  2162 | `		if( rc == SXRET_OK ){` |
|        6 |  2163 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|        3 |  2164 | `		}` |
|        6 |  2165 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  2166 | `			return rc;` |
|        - |  2167 | `		}` |
|        - |  2168 | `	}` |
|        - |  2169 | `	/* (10) Re-mount the static/const attribute slots of every class. */` |
|        - |  2170 | `	{` |
|        - |  2171 | `		SyHashEntry *pEntry;` |
|        6 |  2172 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      238 |  2173 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      232 |  2174 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      232 |  2175 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2176 | `				return rc;` |
|        - |  2177 | `			}` |
|      ! 0 |  2178 | `		}` |
|        - |  2179 | `	}` |
|        - |  2180 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        6 |  2181 | `	SyBlobReset(&pVm->sConsumer);` |
|        6 |  2182 | `	pVm->nOutputLen = 0;` |
|        6 |  2183 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        6 |  2184 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        6 |  2185 | `	pVm->iResponseStatus = 200;` |
|        6 |  2186 | `	pVm->bHeadersSent = 0;` |
|        6 |  2187 | `	pVm->bHttpContext = 0;` |
|        6 |  2188 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|        6 |  2189 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|        6 |  2190 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|        6 |  2191 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|        6 |  2192 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|        6 |  2193 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  2194 | `#ifdef PH7_ENABLE_PCRE` |
|        6 |  2195 | `	pVm->iPcreLastError = 0;` |
|        - |  2196 | `#endif` |
|        6 |  2197 | `	pVm->iCmpCallbackExc = 0;` |
|        6 |  2198 | `	pVm->bReturnRequested = 0;` |
|        6 |  2199 | `	VmReinitMemObj(&(*pVm),&pVm->sCatchReturn);` |
|        6 |  2200 | `	pVm->bHaltRequested = 0;` |
|        6 |  2201 | `	pVm->iExitStatus = 0;` |
|        6 |  2202 | `	pVm->iSpreadExtra = 0;` |
|        6 |  2203 | `	pVm->nRecursionDepth = 0;` |
|        6 |  2204 | `	pVm->pActiveCtx = 0;` |
|        6 |  2205 | `	pVm->pCoalesceObj = 0;` |
|        6 |  2206 | `	pVm->bCoalesceArmed = 0;` |
|        6 |  2207 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|        - |  2208 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        6 |  2209 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  2210 | `	/* Set the ready flag */` |
|        6 |  2211 | `	pVm->nMagic = PH7_VM_RUN;` |
|        6 |  2212 | `	return SXRET_OK;` |
|        3 |  2213 |  |
|        - |  2214 | `/*` |
|        - |  2215 | ` * Release a Virtual Machine.` |
|        - |  2216 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  2217 | ` */` |
|     2962 |  2218 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        5 |  2219 |  |
|        - |  2220 | `	/* Set the stale magic number */` |
|     2967 |  2221 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  2222 | `	/* Release the private memory subsystem */` |
|     2967 |  2223 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2967 |  2224 | `	return SXRET_OK;` |
|        5 |  2225 |  |
|        - |  2226 | `/*` |
|        - |  2227 | ` * Initialize a foreign function call context.` |
|        - |  2228 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  2229 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  2230 | ` * functions.` |
|        - |  2231 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  2232 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  2233 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  2234 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  2235 | ` */` |
|   718821 |  2236 | `static sxi32 VmInitCallContext(` |
|        - |  2237 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  2238 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  2239 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  2240 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  2241 | `	sxi32 iFlags          /* Control flags */` |
|        - |  2242 | `	)` |
|        5 |  2243 |  |
|   718826 |  2244 | `	pOut->pFunc = pFunc;` |
|   718826 |  2245 | `	pOut->pVm   = pVm;` |
|   718826 |  2246 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   718826 |  2247 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  2248 | `	/* Assume a null return value */` |
|   718826 |  2249 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   718826 |  2250 | `	pOut->pRet = pRet;` |
|   718826 |  2251 | `	pOut->iFlags = iFlags;` |
|   718826 |  2252 | `	return SXRET_OK;` |
|        5 |  2253 |  |
|        - |  2254 | `/*` |
|        - |  2255 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  2256 | ` * left behind.` |
|        - |  2257 | ` */` |
|   718821 |  2258 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        5 |  2259 |  |
|        - |  2260 | `	sxu32 n;` |
|   718826 |  2261 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8979 |  2262 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    26315 |  2263 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    17341 |  2264 | `			if( apObj[n] == 0 ){` |
|        - |  2265 | `				/* Already released */` |
|      387 |  2266 | `				continue;` |
|        - |  2267 | `			}` |
|    16959 |  2268 | `			PH7_MemObjRelease(apObj[n]);` |
|    16959 |  2269 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     8482 |  2270 | `		}` |
|     8979 |  2271 | `		SySetRelease(&pCtx->sVar);` |
|     4487 |  2272 | `	}` |
|   718826 |  2273 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  2274 | `		ph7_aux_data *aAux;` |
|        - |  2275 | `		void *pChunk;` |
|        - |  2276 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  2277 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  2278 | `		 */` |
|        9 |  2279 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  2280 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  2281 | `			pChunk = aAux[n].pAuxData;` |
|        - |  2282 | `			/* Release the chunk */` |
|       25 |  2283 | `			if( pChunk ){` |
|       25 |  2284 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  2285 | `			}` |
|       13 |  2286 | `		}` |
|        9 |  2287 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  2288 | `	}` |
|   718826 |  2289 |  |
|        - |  2290 | `/*` |
|        - |  2291 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  2292 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  2293 | ` */` |
|      382 |  2294 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  2295 | `	ph7_context *pCtx, /* Call context */` |
|        - |  2296 | `	ph7_value *pValue  /* Release this value */` |
|        - |  2297 | `	)` |
|        5 |  2298 |  |
|      387 |  2299 | `	if( pValue == 0 ){` |
|        - |  2300 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  2301 | `		return;` |
|        - |  2302 | `	}` |
|      387 |  2303 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      387 |  2304 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  2305 | `		sxu32 n;` |
|     1285 |  2306 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1285 |  2307 | `			if( apObj[n] == pValue ){` |
|      387 |  2308 | `				PH7_MemObjRelease(pValue);` |
|      387 |  2309 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  2310 | `				/* Mark as released */` |
|      387 |  2311 | `				apObj[n] = 0;` |
|      387 |  2312 | `				break;` |
|        - |  2313 | `			}` |
|      454 |  2314 | `		}` |
|      191 |  2315 | `	}` |
|      196 |  2316 |  |
|        - |  2317 | `/*` |
|        - |  2318 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  2319 | ` */` |
|  4092859 |  2320 | `static void VmPopOperand(` |
|        - |  2321 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  2322 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  2323 | `	)` |
|        5 |  2324 |  |
|  4092864 |  2325 | `	ph7_value *pTos = *ppTos;` |
|  8718271 |  2326 | `	while( nPop > 0 ){` |
|  4625412 |  2327 | `		PH7_MemObjRelease(pTos);` |
|  4625412 |  2328 | `		pTos--;` |
|  4625412 |  2329 | `		nPop--;` |
|        5 |  2330 | `	}` |
|        - |  2331 | `	/* Top of the stack */` |
|  4092864 |  2332 | `	*ppTos = pTos;` |
|  4092864 |  2333 |  |
|        - |  2334 | `/*` |
|        - |  2335 | ` * Reserve a memory object.` |
|        - |  2336 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  2337 | ` */` |
|  3272338 |  2338 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        5 |  2339 |  |
|  3272343 |  2340 | `	ph7_value *pObj = 0;` |
|        - |  2341 | `	VmSlot *pSlot;` |
|        - |  2342 | `	sxu32 nIdx;` |
|        - |  2343 | `	/* Check for a free slot */` |
|  3272343 |  2344 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3272343 |  2345 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3272343 |  2346 | `	if( pSlot ){` |
|  1097077 |  2347 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1097077 |  2348 | `		nIdx = pSlot->nIdx;` |
|   548536 |  2349 | `	}` |
|  3272343 |  2350 | `	if( pObj == 0 ){` |
|        - |  2351 | `		/* Reserve a new memory object */` |
|  2175271 |  2352 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2175271 |  2353 | `		if( pObj == 0 ){` |
|      ! 0 |  2354 | `			return 0;` |
|        - |  2355 | `		}` |
|  1087633 |  2356 | `	}` |
|        - |  2357 | `	/* Set a null default value */` |
|  3272343 |  2358 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3272343 |  2359 | `	pObj->nIdx = nIdx;` |
|  3272343 |  2360 | `	return pObj;` |
|  1636174 |  2361 |  |
|        - |  2362 | `/*` |
|        - |  2363 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  2364 | ` */` |
|    39882 |  2365 | `static sxi32 VmHashmapRefInsert(` |
|        - |  2366 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  2367 | `	const char *zKey,  /* Entry key */` |
|        - |  2368 | `	sxu32 nByte,       /* Key length */` |
|        - |  2369 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  2370 | `	)` |
|        5 |  2371 |  |
|        - |  2372 | `	ph7_value sKey;` |
|        - |  2373 | `	sxi32 rc;` |
|    39887 |  2374 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    39887 |  2375 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  2376 | `	/* Perform the insertion */` |
|    39887 |  2377 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    39887 |  2378 | `	PH7_MemObjRelease(&sKey);` |
|    39887 |  2379 | `	return rc;` |
|        5 |  2380 |  |
|        - |  2381 | `/*` |
|        - |  2382 | ` * Extract a variable value from the top active VM frame.` |
|        - |  2383 | ` * Return a pointer to the variable value on success.` |
|        - |  2384 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  2385 | ` */` |
|  3806999 |  2386 | `static ph7_value * VmExtractMemObj(` |
|        - |  2387 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  2388 | `	const SyString *pName, /* Variable name */` |
|        - |  2389 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  2390 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  2391 | `	)` |
|        5 |  2392 |  |
|  3807004 |  2393 | `	int bNullify = FALSE;` |
|        - |  2394 | `	SyHashEntry *pEntry;` |
|        - |  2395 | `	VmFrame *pFrame;` |
|        - |  2396 | `	ph7_value *pObj;` |
|        - |  2397 | `	sxu32 nIdx;` |
|        - |  2398 | `	sxi32 rc;` |
|        - |  2399 | `	/* Point to the top active frame */` |
|  3807004 |  2400 | `	pFrame = pVm->pFrame;` |
|  3807004 |  2401 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  2402 | `	/* Perform the lookup */` |
|  3807004 |  2403 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2404 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2405 | `		pName = &sAnnon;` |
|        - |  2406 | `		/* Always nullify the object */` |
|      ! 0 |  2407 | `		bNullify = TRUE;` |
|      ! 0 |  2408 | `		bDup = FALSE;` |
|      ! 0 |  2409 | `	}` |
|        - |  2410 | `	/* Check the superglobals table first */` |
|  3807004 |  2411 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3807004 |  2412 | `	if( pEntry == 0 ){` |
|        - |  2413 | `		/* Query the top active frame */` |
|  3806938 |  2414 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3806938 |  2415 | `		if( pEntry == 0 ){` |
|   119645 |  2416 | `			char *zName = (char *)pName->zString;` |
|        - |  2417 | `			VmSlot sLocal;` |
|   119645 |  2418 | `			if( !bCreate ){` |
|        - |  2419 | `				/* Do not create the variable,return NULL instead */` |
|     1007 |  2420 | `				return 0;` |
|        - |  2421 | `			}` |
|        - |  2422 | `			/* No such variable,automatically create a new one and install` |
|        - |  2423 | `			 * it in the current frame.` |
|        - |  2424 | `			 */` |
|   118643 |  2425 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   118643 |  2426 | `			if( pObj == 0 ){` |
|      ! 0 |  2427 | `				return 0;` |
|        - |  2428 | `			}` |
|   118643 |  2429 | `			nIdx = pObj->nIdx;` |
|   118643 |  2430 | `			if( bDup ){` |
|        - |  2431 | `				/* Duplicate name */` |
|      232 |  2432 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      232 |  2433 | `				if( zName == 0 ){` |
|      ! 0 |  2434 | `					return 0;` |
|        - |  2435 | `				}` |
|      114 |  2436 | `			}` |
|        - |  2437 | `			/* Link to the top active VM frame */` |
|   118643 |  2438 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   118643 |  2439 | `			if( rc != SXRET_OK ){` |
|        - |  2440 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2441 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2442 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2443 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2444 | `				return 0;` |
|        - |  2445 | `			}` |
|   118643 |  2446 | `			if( pFrame->pParent != 0 ){` |
|        - |  2447 | `				/* Local variable */` |
|   111435 |  2448 | `				sLocal.nIdx = nIdx;` |
|   111435 |  2449 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    55720 |  2450 | `			}else{` |
|        - |  2451 | `				/* Register in the $GLOBALS array */` |
|     7213 |  2452 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2453 | `			}` |
|        - |  2454 | `			/* Install in the reference table */` |
|   118643 |  2455 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2456 | `			/* Save object index */` |
|   118643 |  2457 | `			pObj->nIdx = nIdx;` |
|    59324 |  2458 | `		}else{` |
|        - |  2459 | `			/* Extract variable contents */` |
|  3687298 |  2460 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3687298 |  2461 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3687298 |  2462 | `			if( bNullify && pObj ){` |
|      ! 0 |  2463 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2464 | `			}` |
|        - |  2465 | `		}` |
|  1903183 |  2466 | `	}else{` |
|        - |  2467 | `		/* Superglobal */` |
|       71 |  2468 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       71 |  2469 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2470 | `	}` |
|  3806002 |  2471 | `	return pObj;` |
|  1903717 |  2472 |  |
|        - |  2473 | `/*` |
|        - |  2474 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2475 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2476 | ` */` |
|    21086 |  2477 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2478 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2479 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2480 | `	sxu32 nByte        /* zName length */` |
|        - |  2481 | `	)` |
|        5 |  2482 |  |
|        - |  2483 | `	SyHashEntry *pEntry;` |
|        - |  2484 | `	ph7_value *pValue;` |
|        - |  2485 | `	sxu32 nIdx;` |
|        - |  2486 | `	/* Query the superglobal table */` |
|    21091 |  2487 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    21091 |  2488 | `	if( pEntry == 0 ){` |
|        - |  2489 | `		/* No such entry */` |
|      ! 0 |  2490 | `		return 0;` |
|        - |  2491 | `	}` |
|        - |  2492 | `	/* Extract the superglobal index in the global object pool */` |
|    21091 |  2493 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2494 | `	/* Extract the variable value  */` |
|    21091 |  2495 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    21091 |  2496 | `	return pValue;` |
|    10548 |  2497 |  |
|        - |  2498 | `/*` |
|        - |  2499 | ` * Perform a raw hashmap insertion.` |
|        - |  2500 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2501 | ` */` |
|    21132 |  2502 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2503 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2504 | `	const char *zKey,   /* Entry key */` |
|        - |  2505 | `	int nKeylen,        /* zKey length*/` |
|        - |  2506 | `	const char *zData,  /* Entry data */` |
|        - |  2507 | `	int nLen            /* zData length */` |
|        - |  2508 | `	)` |
|        5 |  2509 |  |
|        - |  2510 | `	ph7_value sKey,sValue;` |
|        - |  2511 | `	sxi32 rc;` |
|    21137 |  2512 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    21137 |  2513 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|    21137 |  2514 | `	if( zKey ){` |
|    18167 |  2515 | `		if( nKeylen < 0 ){` |
|    18079 |  2516 | `			nKeylen = (int)SyStrlen(zKey);` |
|     9037 |  2517 | `		}` |
|    18167 |  2518 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     9081 |  2519 | `	}` |
|    21137 |  2520 | `	if( zData ){` |
|    21137 |  2521 | `		if( nLen < 0 ){` |
|        - |  2522 | `			/* Compute length automatically */` |
|    11989 |  2523 | `			nLen = (int)SyStrlen(zData);` |
|     5992 |  2524 | `		}` |
|    21137 |  2525 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|    10566 |  2526 | `	}` |
|        - |  2527 | `	/* Perform the insertion */` |
|    21137 |  2528 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|    21137 |  2529 | `	PH7_MemObjRelease(&sKey);` |
|    21137 |  2530 | `	PH7_MemObjRelease(&sValue);` |
|    21137 |  2531 | `	return rc;` |
|        5 |  2532 |  |
|        - |  2533 | `/*` |
|        - |  2534 | ` * Configure a working virtual machine instance.` |
|        - |  2535 | ` *` |
|        - |  2536 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2537 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2538 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2539 | ` * The second argument to this function is an integer configuration option` |
|        - |  2540 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2541 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2542 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2543 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2544 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2545 | ` */` |
|    68582 |  2546 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2547 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2548 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2549 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2550 | `	)` |
|        5 |  2551 |  |
|    68587 |  2552 | `	sxi32 rc = SXRET_OK;` |
|    68587 |  2553 | `	switch(nOp){` |
|     1471 |  2554 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2947 |  2555 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2947 |  2556 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2557 | `		/* VM output consumer callback */` |
|        - |  2558 | `#ifdef UNTRUST` |
|        - |  2559 | `		if( xConsumer == 0 ){` |
|        - |  2560 | `			rc = SXERR_CORRUPT;` |
|        - |  2561 | `			break;` |
|        - |  2562 | `		}` |
|        - |  2563 | `#endif` |
|        - |  2564 | `		/* Install the output consumer */` |
|     2947 |  2565 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2947 |  2566 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2947 |  2567 | `		break;` |
|        - |  2568 | `							   }` |
|     1481 |  2569 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2570 | `		/* Import path */` |
|        - |  2571 | `		  const char *zPath;` |
|        - |  2572 | `		  SyString sPath;` |
|     2967 |  2573 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2574 | `#if defined(UNTRUST)` |
|        - |  2575 | `		  if( zPath == 0 ){` |
|        - |  2576 | `			  rc = SXERR_EMPTY;` |
|        - |  2577 | `			  break;` |
|        - |  2578 | `		  }` |
|        - |  2579 | `#endif` |
|     2967 |  2580 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2581 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2582 | `#ifdef __WINNT__` |
|        5 |  2583 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2584 | `#endif` |
|     5929 |  2585 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2586 | `		  /* Remove leading and trailing white spaces */` |
|     2967 |  2587 | `		  SyStringFullTrim(&sPath);` |
|     2967 |  2588 | `		  if( sPath.nByte > 0 ){` |
|        - |  2589 | `			  /* Store the path in the corresponding conatiner */` |
|     2967 |  2590 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1481 |  2591 | `		  }` |
|     2967 |  2592 | `		  break;` |
|        - |  2593 | `									 }` |
|     1483 |  2594 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2595 | `		/* Run-Time Error report */` |
|     2971 |  2596 | `		pVm->bErrReport = 1;` |
|     2971 |  2597 | `		break;` |
|      ! 0 |  2598 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2599 | `		/* Recursion depth */` |
|      ! 0 |  2600 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2601 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2602 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2603 | `		}` |
|      ! 0 |  2604 | `		break;` |
|        - |  2605 | `									   }` |
|      ! 0 |  2606 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2607 | `		/* VM output length in bytes */` |
|      ! 0 |  2608 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2609 | `#ifdef UNTRUST` |
|        - |  2610 | `		if( pOut == 0 ){` |
|        - |  2611 | `			rc = SXERR_CORRUPT;` |
|        - |  2612 | `			break;` |
|        - |  2613 | `		}` |
|        - |  2614 | `#endif` |
|      ! 0 |  2615 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2616 | `		break;` |
|        - |  2617 | `							   }` |
|        - |  2618 |  |
|    16321 |  2619 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2620 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2621 | `		/* Create a new superglobal/global variable */` |
|    32647 |  2622 | `		const char *zName = va_arg(ap,const char *);` |
|    32647 |  2623 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2624 | `		SyHashEntry *pEntry;` |
|        - |  2625 | `		ph7_value *pObj;` |
|        - |  2626 | `		sxu32 nByte;` |
|        - |  2627 | `		sxu32 nIdx;` |
|        - |  2628 | `#ifdef UNTRUST` |
|        - |  2629 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2630 | `			rc = SXERR_CORRUPT;` |
|        - |  2631 | `			break;` |
|        - |  2632 | `		}` |
|        - |  2633 | `#endif` |
|    32647 |  2634 | `		nByte = SyStrlen(zName);` |
|    32647 |  2635 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2636 | `			/* Check if the superglobal is already installed */` |
|    29705 |  2637 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    14855 |  2638 | `		}else{` |
|        - |  2639 | `			/* Query the top active VM frame */` |
|     2947 |  2640 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2641 | `		}` |
|    32647 |  2642 | `		if( pEntry ){` |
|        - |  2643 | `			/* Variable already installed */` |
|      ! 0 |  2644 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2645 | `			/* Extract contents */` |
|      ! 0 |  2646 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2647 | `			if( pObj ){` |
|        - |  2648 | `				/* Overwrite old contents */` |
|      ! 0 |  2649 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2650 | `			}` |
|      ! 0 |  2651 | `		}else{` |
|        - |  2652 | `			/* Install a new variable */` |
|    32647 |  2653 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    32647 |  2654 | `			if( pObj == 0 ){` |
|      ! 0 |  2655 | `				rc = SXERR_MEM;` |
|      ! 0 |  2656 | `				break;` |
|        - |  2657 | `			}` |
|    32647 |  2658 | `			nIdx = pObj->nIdx;` |
|        - |  2659 | `			/* Copy value */` |
|    32647 |  2660 | `			PH7_MemObjStore(pValue,pObj);` |
|    32647 |  2661 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2662 | `				/* Install the superglobal */` |
|    29705 |  2663 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    14855 |  2664 | `			}else{` |
|        - |  2665 | `				/* Install in the current frame */` |
|     2947 |  2666 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2667 | `			}` |
|    32647 |  2668 | `			if( rc == SXRET_OK ){` |
|        - |  2669 | `				SyHashEntry *pRef;` |
|    32647 |  2670 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    29705 |  2671 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    14855 |  2672 | `				}else{` |
|     2947 |  2673 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2674 | `				}` |
|        - |  2675 | `				/* Install in the reference table */` |
|    32647 |  2676 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    32647 |  2677 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2678 | `					/* Register in the $GLOBALS array */` |
|    32647 |  2679 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    16321 |  2680 | `				}` |
|    16321 |  2681 | `			}` |
|        - |  2682 | `		}` |
|    32647 |  2683 | `		break;` |
|        - |  2684 | `									}` |
|     9037 |  2685 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2686 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2687 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2688 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2689 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2690 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2691 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|    18079 |  2692 | `		const char *zKey   = va_arg(ap,const char *);` |
|    18079 |  2693 | `		const char *zValue = va_arg(ap,const char *);` |
|    18079 |  2694 | `		int nLen = va_arg(ap,int);` |
|        - |  2695 | `		ph7_hashmap *pMap;` |
|        - |  2696 | `		ph7_value *pValue;` |
|    18079 |  2697 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2698 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2699 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|    18078 |  2700 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2701 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2702 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|    18077 |  2703 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2704 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2705 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|    18077 |  2706 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2707 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2708 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|    18077 |  2709 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2710 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2711 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|    18077 |  2712 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2713 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2714 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2715 | `		}else{` |
|        - |  2716 | `			/* Extract the $_SERVER superglobal */` |
|    18077 |  2717 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2718 | `		}` |
|    18079 |  2719 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2720 | `			/* No such entry */` |
|      ! 0 |  2721 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2722 | `			break;` |
|        - |  2723 | `		}` |
|        - |  2724 | `		/* Point to the hashmap */` |
|    18079 |  2725 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2726 | `		/* Perform the insertion */` |
|    18079 |  2727 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|    18079 |  2728 | `		break;` |
|        - |  2729 | `								   }` |
|     1486 |  2730 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2731 | `		/* Script arguments */` |
|     2977 |  2732 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2733 | `		ph7_hashmap *pMap;` |
|        - |  2734 | `		ph7_value *pValue;` |
|        - |  2735 | `		sxu32 n;` |
|     2977 |  2736 | `		if( SX_EMPTY_STR(zValue) ){` |
|        2 |  2737 | `			rc = SXERR_EMPTY;` |
|        2 |  2738 | `			break;` |
|        - |  2739 | `		}` |
|        - |  2740 | `		/* Extract the $argv array */` |
|     2975 |  2741 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|     2975 |  2742 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2743 | `			/* No such entry */` |
|      ! 0 |  2744 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2745 | `			break;` |
|        - |  2746 | `		}` |
|        - |  2747 | `		/* Point to the hashmap */` |
|     2975 |  2748 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2749 | `		/* Perform the insertion */` |
|     2975 |  2750 | `		n = (sxu32)SyStrlen(zValue);` |
|     2975 |  2751 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|     2975 |  2752 | `		if( rc == SXRET_OK ){` |
|     2975 |  2753 | `			if( pMap->nEntry > 1 ){` |
|        - |  2754 | `				/* Append space separator first */` |
|       33 |  2755 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|       14 |  2756 | `			}` |
|     2975 |  2757 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|     1485 |  2758 | `		}` |
|     2975 |  2759 | `		break;` |
|        - |  2760 | `								  }` |
|      ! 0 |  2761 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2762 | `		/* error_log() consumer */` |
|      ! 0 |  2763 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2764 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2765 | `		break;` |
|        - |  2766 | `										}` |
|      ! 0 |  2767 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2768 | `		/* Script return value */` |
|      ! 0 |  2769 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2770 | `#ifdef UNTRUST` |
|        - |  2771 | `		if( ppValue == 0 ){` |
|        - |  2772 | `			rc = SXERR_CORRUPT;` |
|        - |  2773 | `			break;` |
|        - |  2774 | `		}` |
|        - |  2775 | `#endif` |
|      ! 0 |  2776 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2777 | `		break;` |
|        - |  2778 | `								   }` |
|     2964 |  2779 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2780 | `		/* Register an IO stream device */` |
|     5933 |  2781 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2782 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8892 |  2783 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5933 |  2784 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2785 | `				/* Invalid stream */` |
|      ! 0 |  2786 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2787 | `				break;` |
|        - |  2788 | `		}` |
|     5933 |  2789 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2790 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2969 |  2791 | `			pVm->pDefStream = pStream;` |
|     1482 |  2792 | `		}` |
|        - |  2793 | `		/* Insert in the appropriate container */` |
|     5933 |  2794 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5933 |  2795 | `		break;` |
|        - |  2796 | `								  }` |
|       12 |  2797 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2798 | `		/* Point to the VM internal output consumer buffer */` |
|       24 |  2799 | `		const void **ppOut = va_arg(ap,const void **);` |
|       24 |  2800 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2801 | `#ifdef UNTRUST` |
|        - |  2802 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2803 | `			rc = SXERR_CORRUPT;` |
|        - |  2804 | `			break;` |
|        - |  2805 | `		}` |
|        - |  2806 | `#endif` |
|       24 |  2807 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       24 |  2808 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       24 |  2809 | `		break;` |
|        - |  2810 | `									   }` |
|       12 |  2811 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2812 | `		/* Raw HTTP request*/` |
|       24 |  2813 | `		const char *zRequest = va_arg(ap,const char *);` |
|       24 |  2814 | `		int nByte = va_arg(ap,int);` |
|       24 |  2815 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2816 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2817 | `			break;` |
|        - |  2818 | `		}` |
|       24 |  2819 | `		if( nByte < 0 ){` |
|        - |  2820 | `			/* Compute length automatically */` |
|      ! 0 |  2821 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2822 | `		}` |
|        - |  2823 | `		/* Process the request */` |
|       24 |  2824 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2825 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       24 |  2826 | `		if( rc == SXRET_OK ){` |
|       24 |  2827 | `			pVm->bHttpContext = 1;` |
|       12 |  2828 | `		}` |
|       24 |  2829 | `		break;` |
|        - |  2830 | `									}` |
|       12 |  2831 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2832 | `		/* Extract HTTP response status code */` |
|       24 |  2833 | `		int *pStatus = va_arg(ap, int *);` |
|       24 |  2834 | `		if( pStatus ){` |
|       24 |  2835 | `			*pStatus = pVm->iResponseStatus;` |
|       12 |  2836 | `		}` |
|       24 |  2837 | `		break;` |
|        - |  2838 | `										}` |
|       12 |  2839 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2840 | `		/* Iterate response headers via callback */` |
|        - |  2841 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       24 |  2842 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       24 |  2843 | `		void *pUserData = va_arg(ap, void *);` |
|       24 |  2844 | `		if( xCallback ){` |
|       24 |  2845 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       24 |  2846 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       36 |  2847 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2848 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2849 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2850 | `							   pUserData);` |
|       12 |  2851 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2852 | `					break;` |
|        - |  2853 | `				}` |
|        6 |  2854 | `			}` |
|       12 |  2855 | `		}` |
|       24 |  2856 | `		break;` |
|        - |  2857 | `										 }` |
|      ! 0 |  2858 | `	default:` |
|        - |  2859 | `		/* Unknown configuration option */` |
|      ! 0 |  2860 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2861 | `		break;` |
|        - |  2862 | `	}` |
|    68587 |  2863 | `	return rc;` |
|        5 |  2864 |  |
|        - |  2865 | `/* Forward declaration */` |
|        - |  2866 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2867 | `/*` |
|        - |  2868 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2869 | ` * format.` |
|        - |  2870 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2871 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2872 | ` * (STDOUT).` |
|        - |  2873 | ` */` |
|        2 |  2874 | `static sxi32 VmByteCodeDump(` |
|        - |  2875 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2876 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2877 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2878 | `	)` |
|        1 |  2879 |  |
|        - |  2880 | `	static const char zDump[] = {` |
|        - |  2881 | `		"====================================================\n"` |
|        - |  2882 | `		"PH7 VM Dump\n"` |
|        - |  2883 | `		"====================================================\n"` |
|        - |  2884 | `	};` |
|        - |  2885 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2886 | `	sxi32 rc = SXRET_OK;` |
|        - |  2887 | `	sxu32 n;` |
|        - |  2888 | `	/* Point to the PH7 instructions */` |
|        3 |  2889 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2890 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2891 | `	n = 0;` |
|        3 |  2892 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2893 | `	/* Dump instructions */` |
|        7 |  2894 | `	for(;;){` |
|       15 |  2895 | `		if( pInstr >= pEnd ){` |
|        - |  2896 | `			/* No more instructions */` |
|        3 |  2897 | `			break;` |
|        - |  2898 | `		}` |
|        - |  2899 | `		/* Format and call the consumer callback */` |
|       19 |  2900 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2901 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2902 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2903 | `		if( rc != SXRET_OK ){` |
|        - |  2904 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2905 | `			return rc;` |
|        - |  2906 | `		}` |
|       13 |  2907 | `		++n;` |
|       13 |  2908 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2909 | `	}` |
|        3 |  2910 | `	return rc;` |
|        2 |  2911 |  |
|        - |  2912 | `/* Forward declaration */` |
|        - |  2913 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2914 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2915 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2916 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2917 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2918 | `/*` |
|        - |  2919 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2920 | ` * consumer callback.` |
|        - |  2921 | ` */` |
|      632 |  2922 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        4 |  2923 |  |
|      636 |  2924 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      636 |  2925 | `	sxi32 rc = SXRET_OK;` |
|        - |  2926 | `	/* Append a new line */` |
|        - |  2927 | `#ifdef __WINNT__` |
|        4 |  2928 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2929 | `#else` |
|      632 |  2930 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2931 | `#endif` |
|        - |  2932 | `	/* Invoke the output consumer callback */` |
|      636 |  2933 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      636 |  2934 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      636 |  2935 | `	return rc;` |
|        4 |  2936 |  |
|        - |  2937 | `/*` |
|        - |  2938 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2939 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2940 | ` * information.` |
|        - |  2941 | ` */` |
|      154 |  2942 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        5 |  2943 |  |
|      159 |  2944 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2945 | `		ph7_value apArg[4];` |
|        - |  2946 | `		ph7_value *apArgPtr[4];` |
|        - |  2947 | `		ph7_value sResult;` |
|        - |  2948 | `		SyString sErr;` |
|        - |  2949 | `		/* Prepare arguments */` |
|       76 |  2950 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2951 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2952 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2953 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2954 | `		if( pFile ){` |
|       76 |  2955 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2956 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2957 | `		}else{` |
|      ! 0 |  2958 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2959 | `		}` |
|       76 |  2960 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2961 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2962 | `		/* Set up pointer array */` |
|       76 |  2963 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2964 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2965 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2966 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2967 | `		/* Call the handler */` |
|       76 |  2968 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2969 | `		/* Check return value */` |
|       76 |  2970 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2971 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2972 | `		}` |
|        - |  2973 | `		/* Release */` |
|       76 |  2974 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2975 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2976 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2977 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2978 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2979 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2980 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2981 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2982 | `	}` |
|        - |  2983 | `	/* No handler, always call error handler */` |
|       84 |  2984 | `	return TRUE;` |
|       82 |  2985 |  |
|      110 |  2986 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2987 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2988 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2989 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2990 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2991 | `	)` |
|        5 |  2992 |  |
|      115 |  2993 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2994 | `	SyString *pFile;` |
|        - |  2995 | `	char *zErr;` |
|      115 |  2996 | `	sxi32 rc = SXRET_OK;` |
|      115 |  2997 | `	if( !pVm->bErrReport ){` |
|        - |  2998 | `		/* Don't bother reporting errors */` |
|        3 |  2999 | `		return SXRET_OK;` |
|        - |  3000 | `	}` |
|        - |  3001 | `	/* Reset the working buffer */` |
|      113 |  3002 | `	SyBlobReset(pWorker);` |
|        - |  3003 | `	/* Peek the processed file if available */` |
|      113 |  3004 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      113 |  3005 | `	if( pFile ){` |
|        - |  3006 | `		/* Append file name */` |
|      113 |  3007 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      113 |  3008 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  3009 | `	}` |
|        - |  3010 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  3011 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  3012 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  3013 | `	 * E_DEPRECATED). */` |
|      113 |  3014 | `	zErr = "Error:  ";` |
|      113 |  3015 | `	switch(iErr){` |
|       19 |  3016 | `	case PH7_CTX_WARNING:` |
|       41 |  3017 | `		zErr = "Warning:  ";` |
|       41 |  3018 | `		break;` |
|        6 |  3019 | `	case PH7_CTX_NOTICE:` |
|       15 |  3020 | `		zErr = "Notice:  ";` |
|       12 |  3021 | `		break;` |
|       29 |  3022 | `	default:` |
|        - |  3023 | `		/* keep iErr unchanged */` |
|       58 |  3024 | `		break;` |
|        - |  3025 | `	}` |
|      113 |  3026 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      113 |  3027 | `	if( pFuncName ){` |
|        - |  3028 | `		/* Append function name first */` |
|       25 |  3029 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       25 |  3030 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  3031 | `	}` |
|      113 |  3032 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  3033 | `	/* Check for user error handler.  compute length of C string */` |
|      113 |  3034 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       52 |  3035 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  3036 | `	}` |
|      113 |  3037 | `	return rc;` |
|       60 |  3038 |  |
|        - |  3039 | `/*` |
|        - |  3040 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  3041 | ` *` |
|        - |  3042 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  3043 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  3044 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  3045 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  3046 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  3047 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  3048 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  3049 | ` * calling it from a VM op.` |
|        - |  3050 | ` */` |
|      ! 0 |  3051 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  3052 |  |
|      ! 0 |  3053 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  3054 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  3055 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  3056 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  3057 | `	return PH7_ABORT;` |
|      ! 0 |  3058 |  |
|        - |  3059 | `/*` |
|        - |  3060 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  3061 | ` */` |
|      ! 0 |  3062 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  3063 |  |
|      ! 0 |  3064 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  3065 |  |
|        - |  3066 | `/*` |
|        - |  3067 | ` * Single source of truth for the call-recursion cap policy. Each recursion` |
|        - |  3068 | ` * entry point (OP_CALL, eval/include, fibers/generators) tests this before` |
|        - |  3069 | ` * descending another native C frame; the control flow on a hit differs per` |
|        - |  3070 | ` * site, but the rule itself lives here.` |
|        - |  3071 | ` */` |
|    33450 |  3072 | `static int VmRecursionExceeded(ph7_vm *pVm)` |
|        5 |  3073 |  |
|    33455 |  3074 | `	return pVm->nRecursionDepth > pVm->nMaxDepth;` |
|        5 |  3075 |  |
|        - |  3076 | `/*` |
|        - |  3077 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|        - |  3078 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|        - |  3079 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|        - |  3080 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|        - |  3081 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|        - |  3082 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|        - |  3083 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|        - |  3084 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|        - |  3085 | ` * register_shutdown_function() callbacks. Used by every recursion path —` |
|        - |  3086 | ` * OP_CALL, eval()/include/require (VmEvalChunk) and fibers/generators` |
|        - |  3087 | ` * (VmStartCtx/VmResumeCtx).` |
|        - |  3088 | ` *` |
|        - |  3089 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|        - |  3090 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|        - |  3091 | ` * re-enter and loop.` |
|        - |  3092 | ` */` |
|        6 |  3093 | `static sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|        2 |  3094 |  |
|        8 |  3095 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  3096 | `		return PH7_ABORT;` |
|        - |  3097 | `	}` |
|        8 |  3098 | `	pVm->iExitStatus = 255;` |
|        8 |  3099 | `	pVm->bHaltRequested = 1;` |
|        8 |  3100 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|        8 |  3101 | `	return PH7_ABORT;` |
|        5 |  3102 |  |
|        - |  3103 | `/*` |
|        - |  3104 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3105 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3106 | ` * information.` |
|        - |  3107 | ` */` |
|       46 |  3108 | `static sxi32 VmThrowErrorAp(` |
|        - |  3109 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3110 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  3111 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  3112 | `	const char *zFormat, /* Format message */` |
|        - |  3113 | `	va_list ap           /* Variable list of arguments */` |
|        - |  3114 | `	)` |
|        5 |  3115 |  |
|       51 |  3116 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  3117 | `	SyBlob sMsg;` |
|        - |  3118 | `	SyString *pFile;` |
|        - |  3119 | `	char *zErr;` |
|       51 |  3120 | `	sxi32 rc = SXRET_OK;` |
|       51 |  3121 | `	if( !pVm->bErrReport ){` |
|        - |  3122 | `		/* Don't bother reporting errors */` |
|      ! 0 |  3123 | `		return SXRET_OK;` |
|        - |  3124 | `	}` |
|        - |  3125 | `	/* Reset the working buffer */` |
|       51 |  3126 | `	SyBlobReset(pWorker);` |
|        - |  3127 | `	/* Peek the processed file if available */` |
|       51 |  3128 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       51 |  3129 | `	if( pFile ){` |
|        - |  3130 | `		/* Append file name */` |
|       51 |  3131 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       51 |  3132 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       23 |  3133 | `	}` |
|        - |  3134 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  3135 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  3136 | `	 * the correct errno value. */` |
|       51 |  3137 | `	zErr = "Error:  ";` |
|       51 |  3138 | `	switch(iErr){` |
|        4 |  3139 | `	case PH7_CTX_WARNING:` |
|       11 |  3140 | `		zErr = "Warning:  ";` |
|       11 |  3141 | `		break;` |
|        3 |  3142 | `	case PH7_CTX_NOTICE:` |
|        8 |  3143 | `		zErr = "Notice:  ";` |
|        6 |  3144 | `		break;` |
|       16 |  3145 | `	default:` |
|        - |  3146 | `		/* do not change iErr */` |
|       32 |  3147 | `		break;` |
|        - |  3148 | `	}` |
|       51 |  3149 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       51 |  3150 | `	if( pFuncName ){` |
|        - |  3151 | `		/* Append function name first */` |
|       28 |  3152 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       28 |  3153 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  3154 | `	}` |
|        - |  3155 | `	/* Format the raw message */` |
|       51 |  3156 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       51 |  3157 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  3158 | `	/* Check if a user error handler is installed */` |
|       51 |  3159 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  3160 | `		/* No handler or handler returned TRUE, normal processing */` |
|       36 |  3161 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       36 |  3162 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       16 |  3163 | `	}` |
|       51 |  3164 | `	SyBlobRelease(&sMsg);` |
|       51 |  3165 | `	return rc;` |
|       28 |  3166 |  |
|        - |  3167 | `/*` |
|        - |  3168 | ``  * Return the class currently active on the self-stack (the innermost `self` `` |
|        - |  3169 | ` * scope), or NULL when executing outside any class context.` |
|        - |  3170 | ` */` |
|       86 |  3171 | `static ph7_class * VmCurrentSelf(ph7_vm *pVm)` |
|        5 |  3172 |  |
|       91 |  3173 | `	if( SySetUsed(&pVm->aSelf) > 0 ){` |
|       53 |  3174 | `		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|       53 |  3175 | `		return apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        - |  3176 | `	}` |
|       43 |  3177 | `	return 0;` |
|       48 |  3178 |  |
|        - |  3179 | `/*` |
|        - |  3180 | ` * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it` |
|        - |  3181 | ` * with the message held in *pMsg, and throw it from the current frame. Consumes` |
|        - |  3182 | ` * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the` |
|        - |  3183 | ` * class is unavailable or the engine is aborting. Shared scaffolding for the` |
|        - |  3184 | ` * typed-property / uninitialized-property / readonly error throwers.` |
|        - |  3185 | ` */` |
|       70 |  3186 | `static sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)` |
|        5 |  3187 |  |
|        - |  3188 | `	ph7_class *pErrClass;` |
|        - |  3189 | `	ph7_class_instance *pThis;` |
|        - |  3190 | `	ph7_class_method *pCons;` |
|        - |  3191 | `	VmFrame *pFrame;` |
|        - |  3192 | `	sxi32 rc;` |
|       75 |  3193 | `	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);` |
|       75 |  3194 | `	if( pErrClass == 0 ){` |
|      ! 0 |  3195 | `		SyBlobRelease(pMsg);` |
|      ! 0 |  3196 | `		return PH7_ABORT;` |
|        - |  3197 | `	}` |
|       75 |  3198 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|       75 |  3199 | `	if( pThis == 0 ){` |
|      ! 0 |  3200 | `		SyBlobRelease(pMsg);` |
|      ! 0 |  3201 | `		return PH7_ABORT;` |
|        - |  3202 | `	}` |
|       75 |  3203 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|       75 |  3204 | `	if( pCons ){` |
|        - |  3205 | `		ph7_value sArg;` |
|        - |  3206 | `		ph7_value *apArg[1];` |
|        - |  3207 | `		SyString sMsgStr;` |
|       75 |  3208 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|       75 |  3209 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       75 |  3210 | `		apArg[0] = &sArg;` |
|       75 |  3211 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       75 |  3212 | `		PH7_MemObjRelease(&sArg);` |
|       35 |  3213 | `	}` |
|       75 |  3214 | `	SyBlobRelease(pMsg);` |
|       75 |  3215 | `	pFrame = pVm->pFrame;` |
|       75 |  3216 | `	if( pFrame ){` |
|       75 |  3217 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       75 |  3218 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       35 |  3219 | `	}` |
|       75 |  3220 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       75 |  3221 | `	PH7_ClassInstanceUnref(pThis);` |
|       75 |  3222 | `	if( rc == SXERR_ABORT ){` |
|       18 |  3223 | `		return PH7_ABORT;` |
|        - |  3224 | `	}` |
|       61 |  3225 | `	return PH7_EXCEPTION;` |
|       40 |  3226 |  |
|        - |  3227 | `/*` |
|        - |  3228 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  3229 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  3230 | ` * possible.` |
|        - |  3231 | ` */` |
|       44 |  3232 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        5 |  3233 |  |
|       49 |  3234 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|       49 |  3235 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|        - |  3236 | `	SyBlob sMsg;` |
|       49 |  3237 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3238 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  3239 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|       49 |  3240 | `	if( pOwner ){` |
|       49 |  3241 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       22 |  3242 | `			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       27 |  3243 | `	}else{` |
|      ! 0 |  3244 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  3245 | `			zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  3246 | `	}` |
|       49 |  3247 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        5 |  3248 |  |
|        - |  3249 | `/*` |
|        - |  3250 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  3251 | ` */` |
|        6 |  3252 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        2 |  3253 |  |
|        8 |  3254 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        8 |  3255 | `	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        - |  3256 | `	SyBlob sMsg;` |
|        8 |  3257 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        8 |  3258 | `	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        3 |  3259 | `		zKind,&pOwner->sName,&pAttr->sName);` |
|        8 |  3260 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        2 |  3261 |  |
|        - |  3262 | `/*` |
|        - |  3263 | ` * Throw the PHP-compatible Error raised on an illegal write to a readonly` |
|        - |  3264 | ` * property (PHP 8.1). bModify TRUE → a write to an already-initialized property` |
|        - |  3265 | ` * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from` |
|        - |  3266 | ` * a scope that cannot satisfy the readonly set-scope ("Cannot modify` |
|        - |  3267 | ` * protected(set) readonly property C::$x from {global scope\|scope X}").` |
|        - |  3268 | ` */` |
|       20 |  3269 | `static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)` |
|        4 |  3270 |  |
|       24 |  3271 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - |  3272 | `	SyBlob sMsg;` |
|       24 |  3273 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       24 |  3274 | `	if( bModify ){` |
|       22 |  3275 | `		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|       13 |  3276 | `	}else{` |
|        3 |  3277 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        3 |  3278 | `		if( pActive ){` |
|      ! 0 |  3279 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",` |
|      ! 0 |  3280 | `				&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|      ! 0 |  3281 | `		}else{` |
|        3 |  3282 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",` |
|        1 |  3283 | `				&pOwner->sName,&pAttr->sName);` |
|        - |  3284 | `		}` |
|        - |  3285 | `	}` |
|       24 |  3286 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        4 |  3287 |  |
|        - |  3288 | `/*` |
|        - |  3289 | `` * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment`` |
|        - |  3290 | ` * and decrement opcodes mutate the per-instance slot directly, bypassing` |
|        - |  3291 | ` * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A` |
|        - |  3292 | `` * readonly property reached by `++`/`--` is necessarily already initialized (an`` |
|        - |  3293 | ` * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always` |
|        - |  3294 | ` * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the` |
|        - |  3295 | ` * PH7_EXCEPTION/PH7_ABORT produced by the throw.` |
|        - |  3296 | ` */` |
|   347301 |  3297 | `static sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  3298 |  |
|        - |  3299 | `	SyHashEntry *pSlot;` |
|        - |  3300 | `	VmClassAttr *pVmAttr;` |
|   347306 |  3301 | `	if( nIdx == SXU32_HIGH \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|   331186 |  3302 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|        - |  3303 | `	}` |
|    16123 |  3304 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    16123 |  3305 | `	if( pSlot == 0 ){` |
|    16095 |  3306 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3307 | `	}` |
|       29 |  3308 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|       29 |  3309 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|       12 |  3310 | `		return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pVmAttr->pAttr,1);` |
|        - |  3311 | `	}` |
|       17 |  3312 | `	return SXRET_OK;` |
|   173698 |  3313 |  |
|        - |  3314 | `/*` |
|        - |  3315 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  3316 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  3317 | ` * For class types, instanceof is verified.` |
|        - |  3318 | ` *` |
|        - |  3319 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  3320 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  3321 | ` */` |
|        - |  3322 | `/*` |
|        - |  3323 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  3324 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  3325 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  3326 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  3327 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  3328 | ` */` |
|       24 |  3329 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        4 |  3330 |  |
|        - |  3331 | `	const char *z, *zEnd, *zTail;` |
|        - |  3332 | `	sxu32 n;` |
|        - |  3333 | `	sxu8 bReal;` |
|        - |  3334 | `	sxi32 rc;` |
|       28 |  3335 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3336 | `		return 0;` |
|        - |  3337 | `	}` |
|       28 |  3338 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       28 |  3339 | `	n = SyBlobLength(&pValue->sBlob);` |
|       28 |  3340 | `	zEnd = z + n;` |
|       28 |  3341 | `	if( n == 0 ){` |
|      ! 0 |  3342 | `		return 0;` |
|        - |  3343 | `	}` |
|       28 |  3344 | `	zTail = 0;` |
|       28 |  3345 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       28 |  3346 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|       10 |  3347 | `		return 0;` |
|        - |  3348 | `	}` |
|        - |  3349 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       19 |  3350 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  3351 | `		zTail++;` |
|      ! 0 |  3352 | `	}` |
|       19 |  3353 | `	return zTail == zEnd ? 1 : 0;` |
|       16 |  3354 |  |
|        - |  3355 |  |
|        - |  3356 | `/*` |
|        - |  3357 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  3358 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  3359 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  3360 | ` *   0 if it's not strictly numeric.` |
|        - |  3361 | ` */` |
|       16 |  3362 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  3363 |  |
|        - |  3364 | `	const char *z, *zEnd, *zTail;` |
|        - |  3365 | `	sxu32 n;` |
|       18 |  3366 | `	sxu8 bReal = 0;` |
|        - |  3367 | `	sxi32 rc;` |
|       18 |  3368 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3369 | `		return 0;` |
|        - |  3370 | `	}` |
|       18 |  3371 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  3372 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  3373 | `	zEnd = z + n;` |
|       18 |  3374 | `	if( n == 0 ) return 0;` |
|       18 |  3375 | `	zTail = 0;` |
|       18 |  3376 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  3377 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  3378 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  3379 | `	if( zTail != zEnd ) return 0;` |
|       15 |  3380 | `	return bReal ? 2 : 1;` |
|       10 |  3381 |  |
|        - |  3382 |  |
|        - |  3383 | `/*` |
|        - |  3384 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|        - |  3385 | `` * PH7 parses `true`/`false`/`iterable`/`mixed` as class-name atoms (they are not`` |
|        - |  3386 | ` * scalar keywords), so without this every enforcement site — return, parameter,` |
|        - |  3387 | ` * property, union alternative — would have to string-match the name itself.` |
|        - |  3388 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|        - |  3389 | ` * to extend when another literal/pseudo type is added.` |
|        - |  3390 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|        - |  3391 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|        - |  3392 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|        - |  3393 | ` */` |
|      160 |  3394 | `static int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|        4 |  3395 |  |
|      164 |  3396 | `	const char *z = pClass->zString;` |
|      164 |  3397 | `	sxu32 n = pClass->nByte;` |
|      164 |  3398 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|       51 |  3399 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|        - |  3400 | `	}` |
|      114 |  3401 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       15 |  3402 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|        - |  3403 | `	}` |
|      100 |  3404 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|        3 |  3405 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|        - |  3406 | `	}` |
|       98 |  3407 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        - |  3408 | `		/* iterable === array \| Traversable */` |
|       17 |  3409 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  3410 | `			return 1;` |
|        - |  3411 | `		}` |
|       11 |  3412 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|        5 |  3413 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        5 |  3414 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|        5 |  3415 | `				return 1;` |
|        - |  3416 | `			}` |
|      ! 0 |  3417 | `		}` |
|        7 |  3418 | `		return 0;` |
|        - |  3419 | `	}` |
|       82 |  3420 | `	return -1;` |
|       84 |  3421 |  |
|        - |  3422 | `/*` |
|        - |  3423 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  3424 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  3425 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  3426 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  3427 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  3428 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  3429 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  3430 | ` * throw.` |
|        - |  3431 | ` *` |
|        - |  3432 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  3433 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  3434 | ` */` |
|      106 |  3435 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        5 |  3436 |  |
|        - |  3437 | `	sxu32 i;` |
|        - |  3438 | `	ph7_type_alt *aAlts;` |
|        - |  3439 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  3440 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      111 |  3441 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       15 |  3442 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  3443 | `	}` |
|       98 |  3444 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|        - |  3445 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|        - |  3446 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|        - |  3447 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|        - |  3448 | ``	 * `iterable\|Foo`). */`` |
|      282 |  3449 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      186 |  3450 | `		if( aAlts[i].nType == SXU32_HIGH` |
|      111 |  3451 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|        3 |  3452 | `			return SXRET_OK;` |
|        - |  3453 | `		}` |
|       96 |  3454 | `	}` |
|       96 |  3455 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       96 |  3456 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      280 |  3457 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      188 |  3458 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      162 |  3459 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      162 |  3460 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      162 |  3461 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       82 |  3462 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       54 |  3463 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  3464 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       96 |  3465 | `	}` |
|        - |  3466 | `	/* Object handling */` |
|       96 |  3467 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       19 |  3468 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       19 |  3469 | `		if( bHasClassAlt ){` |
|       15 |  3470 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       15 |  3471 | `			ph7_class *pSelfNow = VmCurrentSelf(pVm);` |
|       27 |  3472 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  3473 | `				ph7_class *pExpected;` |
|        - |  3474 | `				SyString *pCN;` |
|       23 |  3475 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       23 |  3476 | `				pCN = &aAlts[i].sClass;` |
|       23 |  3477 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  3478 | `					pExpected = pSelfNow;` |
|       23 |  3479 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  3480 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3481 | `				}else{` |
|       23 |  3482 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  3483 | `				}` |
|       23 |  3484 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  3485 | `					return SXRET_OK;` |
|        - |  3486 | `				}` |
|        9 |  3487 | `			}` |
|        2 |  3488 | `		}` |
|       10 |  3489 | `		return SXERR_INVALID;` |
|        - |  3490 | `	}` |
|        - |  3491 | `	/* Array handling */` |
|       79 |  3492 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        8 |  3493 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  3494 | `	}` |
|        - |  3495 | `	/* Scalar handling — exact match first */` |
|       72 |  3496 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       30 |  3497 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  3498 | `	}` |
|       44 |  3499 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  3500 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  3501 | `	}` |
|       40 |  3502 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       40 |  3503 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  3504 | `	}` |
|       18 |  3505 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3506 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  3507 | `	}` |
|       18 |  3508 | `	if( bStrict ){` |
|        - |  3509 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  3510 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  3511 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  3512 | `			return SXRET_OK;` |
|        - |  3513 | `		}` |
|      ! 0 |  3514 | `		return SXERR_INVALID;` |
|        - |  3515 | `	}` |
|        - |  3516 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  3517 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  3518 | `	 * to match PHP's union RFC. */` |
|        - |  3519 | `	{` |
|       18 |  3520 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  3521 | `		if( bHasInt ){` |
|        - |  3522 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  3523 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  3524 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3525 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3526 | `				return SXRET_OK;` |
|        - |  3527 | `			}` |
|       18 |  3528 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3529 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  3530 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  3531 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3532 | `					return SXRET_OK;` |
|        - |  3533 | `				}` |
|      ! 0 |  3534 | `			}` |
|       18 |  3535 | `			if( kind == 1 ){` |
|        9 |  3536 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  3537 | `				return SXRET_OK;` |
|        - |  3538 | `			}` |
|        4 |  3539 | `		}` |
|       10 |  3540 | `		if( bHasFloat ){` |
|       10 |  3541 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  3542 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  3543 | `				return SXRET_OK;` |
|        - |  3544 | `			}` |
|       10 |  3545 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  3546 | `				PH7_MemObjToReal(pValue);` |
|        7 |  3547 | `				return SXRET_OK;` |
|        - |  3548 | `			}` |
|        1 |  3549 | `		}` |
|        3 |  3550 | `		if( bHasString ){` |
|      ! 0 |  3551 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3552 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3553 | `				return SXRET_OK;` |
|        - |  3554 | `			}` |
|      ! 0 |  3555 | `		}` |
|        3 |  3556 | `		if( bHasBool ){` |
|      ! 0 |  3557 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3558 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3559 | `				return SXRET_OK;` |
|        - |  3560 | `			}` |
|      ! 0 |  3561 | `		}` |
|        - |  3562 | `	}` |
|        3 |  3563 | `	return SXERR_INVALID;` |
|       58 |  3564 |  |
|        - |  3565 |  |
|        - |  3566 | `/*` |
|        - |  3567 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3568 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3569 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3570 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3571 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3572 | ` */` |
|       38 |  3573 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        3 |  3574 |  |
|        - |  3575 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|        - |  3576 | `	 * null value satisfies it (and a null value matches via the flag test` |
|        - |  3577 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|        - |  3578 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|        - |  3579 | `	 * silently swallow any argument. */` |
|       41 |  3580 | `	if( nType == MEMOBJ_NULL ){` |
|        3 |  3581 | `		return SXERR_INVALID;` |
|        - |  3582 | `	}` |
|       39 |  3583 | `	if( bStrict ){` |
|        - |  3584 | `		/* Only int -> float widening is allowed implicitly. */` |
|       13 |  3585 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3586 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3587 | `			return SXRET_OK;` |
|        - |  3588 | `		}` |
|       11 |  3589 | `		return SXERR_INVALID;` |
|        - |  3590 | `	}` |
|        - |  3591 | `	{` |
|       28 |  3592 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3593 | `		if( xCast ) xCast(pVal);` |
|        - |  3594 | `	}` |
|       28 |  3595 | `	return SXRET_OK;` |
|       22 |  3596 |  |
|        - |  3597 |  |
|        - |  3598 | `/*` |
|        - |  3599 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3600 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3601 | ` *` |
|        - |  3602 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3603 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3604 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3605 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3606 | ` */` |
|       12 |  3607 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        4 |  3608 |  |
|       16 |  3609 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|       16 |  3610 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|       16 |  3611 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       16 |  3612 | `		if( pDeclared->zString && nCopy > 0 ){` |
|       16 |  3613 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        6 |  3614 | `		}` |
|       16 |  3615 | `		zBuf[nCopy] = 0;` |
|       16 |  3616 | `		return zBuf;` |
|        - |  3617 | `	}` |
|      ! 0 |  3618 | `	switch( nType ){` |
|      ! 0 |  3619 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3620 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3621 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3622 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3623 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3624 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3625 | `		default:             return "scalar";` |
|        - |  3626 | `	}` |
|       10 |  3627 |  |
|        - |  3628 |  |
|        - |  3629 | `/*` |
|        - |  3630 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3631 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3632 | ` */` |
|       18 |  3633 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        4 |  3634 |  |
|       22 |  3635 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       31 |  3636 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3637 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       22 |  3638 | `	return zBuf;` |
|        4 |  3639 |  |
|        - |  3640 |  |
|     7014 |  3641 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        5 |  3642 |  |
|        - |  3643 | `	SyHashEntry *pSlot;` |
|        - |  3644 | `	VmClassAttr *pVmAttr;` |
|        - |  3645 | `	ph7_class_attr *pAttr;` |
|     7019 |  3646 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|     7019 |  3647 | `	if( pSlot == 0 ){` |
|     6741 |  3648 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3649 | `	}` |
|      283 |  3650 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      283 |  3651 | `	pAttr = pVmAttr->pAttr;` |
|      283 |  3652 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3653 | `		return SXRET_OK;` |
|        - |  3654 | `	}` |
|        - |  3655 | `	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly` |
|        - |  3656 | `	 * property may be written exactly once and only from within the declaring` |
|        - |  3657 | `	 * class scope (its set-scope is protected). */` |
|      283 |  3658 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - |  3659 | `		/* A readonly property is always typed and default-less, so it starts` |
|        - |  3660 | `		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*` |
|        - |  3661 | `		 * write below — making it the write-once latch (a type-rejected write` |
|        - |  3662 | `		 * leaves it set, so a later valid initialization still works). */` |
|       55 |  3663 | `		if( (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|        - |  3664 | `			/* Already initialized: any further write is forbidden, any scope. */` |
|       11 |  3665 | `			return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,1);` |
|        - |  3666 | `		}` |
|        - |  3667 | `		{` |
|        - |  3668 | `			/* First write must come from within the declaring class scope` |
|        - |  3669 | `			 * (readonly's set-scope is protected — a subclass may initialize). */` |
|       47 |  3670 | `			ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       47 |  3671 | `			ph7_class *pActive = VmCurrentSelf(pVm);` |
|       47 |  3672 | `			if( pActive == 0 \|\| pDecl == 0 \|\| !PH7_VmInstanceOf(pActive,pDecl) ){` |
|        3 |  3673 | `				return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,0);` |
|        - |  3674 | `			}` |
|        - |  3675 | `		}` |
|       20 |  3676 | `	}` |
|        - |  3677 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3678 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3679 | `	 * matching PHP's documented behavior. */` |
|      273 |  3680 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       25 |  3681 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3682 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3683 |  |
|       18 |  3684 | `		if( rc == SXRET_OK ){` |
|        9 |  3685 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3686 | `			return SXRET_OK;` |
|        - |  3687 | `		}` |
|        9 |  3688 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3689 | `			char zBuf[128];` |
|        4 |  3690 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3691 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3692 | `		}` |
|        6 |  3693 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3694 | `	}` |
|        - |  3695 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|        - |  3696 | `	 * includes null). */` |
|      259 |  3697 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       17 |  3698 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|       13 |  3699 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        2 |  3700 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|       16 |  3701 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       16 |  3702 | `			return SXRET_OK;` |
|        - |  3703 | `		}` |
|        3 |  3704 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3705 | `	}` |
|        - |  3706 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|        - |  3707 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|        - |  3708 | `	 * type error. */` |
|      243 |  3709 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 |  3710 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3711 | `	}` |
|        - |  3712 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3713 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3714 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      243 |  3715 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3716 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3717 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3718 | `			return SXRET_OK;` |
|        - |  3719 | `		}` |
|        7 |  3720 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3721 | `	}` |
|        - |  3722 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|        - |  3723 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|        - |  3724 | `	 * handled by the nullable check above). Checked by value before the generic` |
|        - |  3725 | `	 * class-instanceof branch, which would resolve no such class and then` |
|        - |  3726 | `	 * wrongly accept any object / reject arrays. */` |
|      233 |  3727 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       39 |  3728 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|       39 |  3729 | `		if( rcPseudo == 1 ){` |
|       11 |  3730 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       11 |  3731 | `			return SXRET_OK;` |
|        - |  3732 | `		}` |
|       29 |  3733 | `		if( rcPseudo == 0 ){` |
|        3 |  3734 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3735 | `		}` |
|        - |  3736 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|       12 |  3737 | `	}` |
|      221 |  3738 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3739 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3740 | `		 * currently active on the self-stack. */` |
|       27 |  3741 | `		ph7_class *pExpected = 0;` |
|       27 |  3742 | `		SyString *pClassName = &pAttr->sClass;` |
|       27 |  3743 | `		ph7_class *pSelfNow = VmCurrentSelf(pVm);` |
|       27 |  3744 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3745 | `			pExpected = pSelfNow;` |
|       25 |  3746 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3747 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3748 | `		}else{` |
|       23 |  3749 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3750 | `		}` |
|       27 |  3751 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3752 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3753 | `		}` |
|       27 |  3754 | `		if( pExpected ){` |
|       23 |  3755 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       23 |  3756 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3757 | `				char zBuf[128];` |
|        8 |  3758 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3759 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3760 | `			}` |
|        8 |  3761 | `		}` |
|       23 |  3762 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       23 |  3763 | `		return SXRET_OK;` |
|        - |  3764 | `	}` |
|        - |  3765 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3766 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      197 |  3767 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3768 | `		char zBuf[128];` |
|       12 |  3769 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3770 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3771 | `	}` |
|      191 |  3772 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       33 |  3773 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       33 |  3774 | `		if( xCast ){` |
|        - |  3775 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       33 |  3776 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3777 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3778 | `			}` |
|       31 |  3779 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        6 |  3780 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3781 | `			}` |
|        - |  3782 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3783 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3784 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       32 |  3785 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       21 |  3786 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       25 |  3787 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|       16 |  3788 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3789 | `			}` |
|       12 |  3790 | `			xCast(pValue);` |
|        5 |  3791 | `		}` |
|        5 |  3792 | `	}` |
|      173 |  3793 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      173 |  3794 | `	return SXRET_OK;` |
|     3512 |  3795 |  |
|        - |  3796 | `/*` |
|        - |  3797 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|        - |  3798 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|        - |  3799 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|        - |  3800 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|        - |  3801 | ` */` |
|        4 |  3802 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        2 |  3803 |  |
|        6 |  3804 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - |  3805 | `	char zBuf[128];` |
|        - |  3806 | `	const char *zGiven;` |
|        6 |  3807 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3808 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3809 | `	}else{` |
|        6 |  3810 | `		zGiven = ph7_type_name(pValue);` |
|        - |  3811 | `	}` |
|        - |  3812 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|        - |  3813 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|        - |  3814 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|        - |  3815 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|        - |  3816 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|        - |  3817 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|        - |  3818 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|        6 |  3819 | `	if( pVm->sCodeGen.xErr ){` |
|        4 |  3820 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|        - |  3821 | `			"Cannot use %s as value for class constant %z::%z of type %z",` |
|        1 |  3822 | `			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|        2 |  3823 | `	}else{` |
|        4 |  3824 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3825 | `			"Cannot use %s as value for class constant %z::%z of type %z",` |
|        1 |  3826 | `			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  3827 | `	}` |
|        6 |  3828 | `	pVm->iExitStatus = 255;` |
|        6 |  3829 | `	pVm->bHaltRequested = 1;` |
|        6 |  3830 | `	return SXERR_ABORT;` |
|        2 |  3831 |  |
|        - |  3832 | `/*` |
|        - |  3833 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|        - |  3834 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|        - |  3835 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|        - |  3836 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|        - |  3837 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|        - |  3838 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|        - |  3839 | ` */` |
|     1084 |  3840 | `static sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        3 |  3841 |  |
|     1087 |  3842 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|        - |  3843 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|     1087 |  3844 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 |  3845 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|        3 |  3846 | `			return SXRET_OK;` |
|        - |  3847 | `		}` |
|      ! 0 |  3848 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|      ! 0 |  3849 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|      ! 0 |  3850 | `			return SXRET_OK;` |
|        - |  3851 | `		}` |
|      ! 0 |  3852 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3853 | `	}` |
|        - |  3854 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|     1085 |  3855 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|        5 |  3856 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */) == SXRET_OK ){` |
|        5 |  3857 | `			return SXRET_OK;` |
|        - |  3858 | `		}` |
|      ! 0 |  3859 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3860 | `	}` |
|        - |  3861 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|     1081 |  3862 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 |  3863 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3864 | `	}` |
|        - |  3865 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|     1081 |  3866 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 |  3867 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3868 | `			return SXRET_OK;` |
|        - |  3869 | `		}` |
|      ! 0 |  3870 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3871 | `	}` |
|        - |  3872 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|        - |  3873 | `	 * a real class/interface verified by instanceof. */` |
|     1081 |  3874 | `	if( pAttr->nType == SXU32_HIGH ){` |
|      ! 0 |  3875 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|      ! 0 |  3876 | `		if( rcPseudo == 1 ){` |
|      ! 0 |  3877 | `			return SXRET_OK;` |
|        - |  3878 | `		}` |
|      ! 0 |  3879 | `		if( rcPseudo == 0 ){` |
|      ! 0 |  3880 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3881 | `		}` |
|        - |  3882 | `		/* rcPseudo == -1: a real class/interface type. */` |
|      ! 0 |  3883 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3884 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3885 | `		}` |
|        - |  3886 | `		{` |
|      ! 0 |  3887 | `			SyString *pCN = &pAttr->sClass;` |
|        - |  3888 | `			ph7_class *pExpected;` |
|      ! 0 |  3889 | `			if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  3890 | `				pExpected = pClass;` |
|      ! 0 |  3891 | `			}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  3892 | `				pExpected = pClass->pBase;` |
|      ! 0 |  3893 | `			}else{` |
|      ! 0 |  3894 | `				pExpected = PH7_VmExtractClass(&(*pVm),pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  3895 | `			}` |
|      ! 0 |  3896 | `			if( pExpected ){` |
|      ! 0 |  3897 | `				ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      ! 0 |  3898 | `				if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3899 | `					return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3900 | `				}` |
|      ! 0 |  3901 | `			}` |
|        - |  3902 | `		}` |
|      ! 0 |  3903 | `		return SXRET_OK;` |
|        - |  3904 | `	}` |
|        - |  3905 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|        - |  3906 | `	 * implicit widening. Everything else is a type error.` |
|        - |  3907 | `	 *` |
|        - |  3908 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|        - |  3909 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT (a `1.0` literal, and — because `/` always`` |
|        - |  3910 | ``	 * yields a real — an evenly-dividing `4/2`), so such a value satisfies a`` |
|        - |  3911 | ``	 * `: int` constant here. PHP accepts `const int X = 4/2` (its `/` yields a`` |
|        - |  3912 | ``	 * genuine int) but rejects `const int X = 1.0`; PHL cannot tell the two`` |
|        - |  3913 | ``	 * apart by flag, so it accepts both rather than rejecting the valid `4/2`.`` |
|        - |  3914 | ``	 * A fractional real (`1.5`, MEMOBJ_REAL only) carries no MEMOBJ_INT and is`` |
|        - |  3915 | `	 * correctly rejected. Tightening this needs PHL's float-identity/division` |
|        - |  3916 | `	 * model, which is out of scope here. */` |
|     1081 |  3917 | `	if( pValue->iFlags & pAttr->nType ){` |
|     1073 |  3918 | `		return SXRET_OK;` |
|        - |  3919 | `	}` |
|        9 |  3920 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|        3 |  3921 | `		PH7_MemObjToReal(pValue);` |
|        3 |  3922 | `		return SXRET_OK;` |
|        - |  3923 | `	}` |
|        6 |  3924 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      545 |  3925 |  |
|        - |  3926 |  |
|        - |  3927 | `/*` |
|        - |  3928 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3929 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3930 | ` * information.` |
|        - |  3931 | ` * ------------------------------------` |
|        - |  3932 | ` * Simple boring wrapper function.` |
|        - |  3933 | ` * ------------------------------------` |
|        - |  3934 | ` */` |
|       22 |  3935 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        4 |  3936 |  |
|        - |  3937 | `	va_list ap;` |
|        - |  3938 | `	sxi32 rc;` |
|       26 |  3939 | `	va_start(ap,zFormat);` |
|       26 |  3940 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       26 |  3941 | `	va_end(ap);` |
|       26 |  3942 | `	return rc;` |
|        4 |  3943 |  |
|        - |  3944 | `/*` |
|        - |  3945 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3946 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3947 | ` */` |
|       42 |  3948 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        5 |  3949 |  |
|        - |  3950 | `	ph7_class *pClass;` |
|        - |  3951 | `	ph7_class_instance *pThis;` |
|        - |  3952 | `	ph7_class_method *pCons;` |
|        - |  3953 | `	ph7_value sArg;` |
|        - |  3954 | `	ph7_value *apArg[1];` |
|        - |  3955 | `	SyBlob sMsg;` |
|        - |  3956 | `	SyString sMsgStr;` |
|        - |  3957 | `	VmFrame *pFrame;` |
|        - |  3958 | `	sxi32 rc;` |
|       47 |  3959 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       47 |  3960 | `	if( pClass == 0 ){` |
|      ! 0 |  3961 | `		return PH7_ABORT;` |
|        - |  3962 | `	}` |
|       47 |  3963 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       47 |  3964 | `	if( pThis == 0 ){` |
|      ! 0 |  3965 | `		return PH7_ABORT;` |
|        - |  3966 | `	}` |
|       47 |  3967 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       47 |  3968 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       21 |  3969 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       47 |  3970 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       47 |  3971 | `	if( pCons ){` |
|       47 |  3972 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       47 |  3973 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       47 |  3974 | `		apArg[0] = &sArg;` |
|       47 |  3975 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       47 |  3976 | `		PH7_MemObjRelease(&sArg);` |
|       21 |  3977 | `	}` |
|       47 |  3978 | `	SyBlobRelease(&sMsg);` |
|       47 |  3979 | `	pFrame = pVm->pFrame;` |
|       47 |  3980 | `	if( pFrame ){` |
|       47 |  3981 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       47 |  3982 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       21 |  3983 | `	}` |
|       47 |  3984 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       47 |  3985 | `	PH7_ClassInstanceUnref(pThis);` |
|       47 |  3986 | `	if( rc == SXERR_ABORT ){` |
|        6 |  3987 | `		return PH7_ABORT;` |
|        - |  3988 | `	}` |
|       43 |  3989 | `	return PH7_EXCEPTION;` |
|       26 |  3990 |  |
|        - |  3991 | `/*` |
|        - |  3992 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3993 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3994 | ` */` |
|       12 |  3995 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        4 |  3996 |  |
|        - |  3997 | `	ph7_class *pClass;` |
|        - |  3998 | `	ph7_class_instance *pThis;` |
|        - |  3999 | `	ph7_class_method *pCons;` |
|        - |  4000 | `	ph7_value sArg;` |
|        - |  4001 | `	ph7_value *apArg[1];` |
|        - |  4002 | `	SyBlob sMsg;` |
|        - |  4003 | `	SyString sMsgStr;` |
|        - |  4004 | `	VmFrame *pFrame;` |
|        - |  4005 | `	sxi32 rc;` |
|       16 |  4006 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       16 |  4007 | `	if( pClass == 0 ){` |
|      ! 0 |  4008 | `		return PH7_ABORT;` |
|        - |  4009 | `	}` |
|       16 |  4010 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       16 |  4011 | `	if( pThis == 0 ){` |
|      ! 0 |  4012 | `		return PH7_ABORT;` |
|        - |  4013 | `	}` |
|       16 |  4014 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       16 |  4015 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        6 |  4016 | `		pFuncName,zExpected,zGiven);` |
|       16 |  4017 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       16 |  4018 | `	if( pCons ){` |
|       16 |  4019 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       16 |  4020 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       16 |  4021 | `		apArg[0] = &sArg;` |
|       16 |  4022 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       16 |  4023 | `		PH7_MemObjRelease(&sArg);` |
|        6 |  4024 | `	}` |
|       16 |  4025 | `	SyBlobRelease(&sMsg);` |
|       16 |  4026 | `	pFrame = pVm->pFrame;` |
|       16 |  4027 | `	if( pFrame ){` |
|       16 |  4028 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       16 |  4029 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 |  4030 | `	}` |
|       16 |  4031 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       16 |  4032 | `	PH7_ClassInstanceUnref(pThis);` |
|       16 |  4033 | `	if( rc == SXERR_ABORT ){` |
|        9 |  4034 | `		return PH7_ABORT;` |
|        - |  4035 | `	}` |
|        7 |  4036 | `	return PH7_EXCEPTION;` |
|       10 |  4037 |  |
|        - |  4038 | `/*` |
|        - |  4039 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  4040 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  4041 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  4042 | ` */` |
|       28 |  4043 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        3 |  4044 |  |
|       31 |  4045 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       10 |  4046 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  4047 | `	}` |
|       23 |  4048 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        9 |  4049 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        9 |  4050 | `		if( pThis && pThis->pClass ){` |
|        9 |  4051 | `			SyString *pName = &pThis->pClass->sName;` |
|        9 |  4052 | `			sxu32 n = pName->nByte;` |
|        9 |  4053 | `			if( n >= nBuf ){` |
|      ! 0 |  4054 | `				n = nBuf - 1;` |
|      ! 0 |  4055 | `			}` |
|        9 |  4056 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        9 |  4057 | `			zBuf[n] = 0;` |
|        9 |  4058 | `			return zBuf;` |
|        - |  4059 | `		}` |
|      ! 0 |  4060 | `		return "object";` |
|        - |  4061 | `	}` |
|       16 |  4062 | `	return ph7_type_name(pVal);` |
|       17 |  4063 |  |
|        - |  4064 | `/*` |
|        - |  4065 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  4066 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  4067 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  4068 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  4069 | ` */` |
|       18 |  4070 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        3 |  4071 |  |
|        - |  4072 | `	ph7_class *pClass;` |
|        - |  4073 | `	ph7_class_instance *pThis;` |
|        - |  4074 | `	ph7_class_method *pCons;` |
|        - |  4075 | `	ph7_value sArg;` |
|        - |  4076 | `	ph7_value *apArg[1];` |
|        - |  4077 | `	SyBlob sMsg;` |
|        - |  4078 | `	SyString sMsgStr;` |
|        - |  4079 | `	VmFrame *pFrame;` |
|        - |  4080 | `	sxi32 rc;` |
|       21 |  4081 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  4082 | `	char zNameBuf[64];` |
|       21 |  4083 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       21 |  4084 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       21 |  4085 | `	if( pClass == 0 ){` |
|      ! 0 |  4086 | `		return PH7_ABORT;` |
|        - |  4087 | `	}` |
|       21 |  4088 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       21 |  4089 | `	if( pThis == 0 ){` |
|      ! 0 |  4090 | `		return PH7_ABORT;` |
|        - |  4091 | `	}` |
|       21 |  4092 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       21 |  4093 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       21 |  4094 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       21 |  4095 | `	if( pCons ){` |
|       21 |  4096 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       21 |  4097 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       21 |  4098 | `		apArg[0] = &sArg;` |
|       21 |  4099 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       21 |  4100 | `		PH7_MemObjRelease(&sArg);` |
|        9 |  4101 | `	}` |
|       21 |  4102 | `	SyBlobRelease(&sMsg);` |
|       21 |  4103 | `	pFrame = pVm->pFrame;` |
|       21 |  4104 | `	if( pFrame ){` |
|       21 |  4105 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       21 |  4106 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        9 |  4107 | `	}` |
|       21 |  4108 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       21 |  4109 | `	PH7_ClassInstanceUnref(pThis);` |
|       21 |  4110 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4111 | `		return PH7_ABORT;` |
|        - |  4112 | `	}` |
|       21 |  4113 | `	return PH7_EXCEPTION;` |
|       12 |  4114 |  |
|        - |  4115 | `/*` |
|        - |  4116 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  4117 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  4118 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  4119 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  4120 | ` */` |
|        - |  4121 | `/*` |
|        - |  4122 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  4123 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  4124 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  4125 | ` */` |
|       34 |  4126 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        5 |  4127 |  |
|        - |  4128 | `	sxu32 nCopy;` |
|       39 |  4129 | `	if( nBuf == 0 ) return "";` |
|       39 |  4130 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  4131 | `		zBuf[0] = 0;` |
|      ! 0 |  4132 | `		return zBuf;` |
|        - |  4133 | `	}` |
|       39 |  4134 | `	nCopy = SyStringLength(pStr);` |
|       39 |  4135 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       39 |  4136 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       39 |  4137 | `	zBuf[nCopy] = 0;` |
|       39 |  4138 | `	return zBuf;` |
|       22 |  4139 |  |
|        - |  4140 |  |
|      480 |  4141 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        5 |  4142 |  |
|      485 |  4143 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  4144 | `	const char *zGiven;` |
|        - |  4145 | `	char zBuf[128];` |
|        - |  4146 | `	char zTypeBuf[128];` |
|        - |  4147 | `	/* Untyped function: no enforcement. */` |
|      485 |  4148 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  4149 | `		return SXRET_OK;` |
|        - |  4150 | `	}` |
|        - |  4151 | `	/* void return type: the function must not produce a value. */` |
|      485 |  4152 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      160 |  4153 | `		if( pValue == 0 ){` |
|      158 |  4154 | `			return SXRET_OK;` |
|        - |  4155 | `		}` |
|        - |  4156 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  4157 | `		 * still counts as "returned a value" here. */` |
|        3 |  4158 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  4159 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  4160 | `	}` |
|        - |  4161 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  4162 | ``	 * returns null. For a typed non-nullable return (including `mixed`, which`` |
|        - |  4163 | `	 * requires an explicit returned value), that's a TypeError. */` |
|      329 |  4164 | `	if( pValue == 0 ){` |
|      ! 0 |  4165 | `		const char *zExpected = "value";` |
|      ! 0 |  4166 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  4167 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  4168 | `		}` |
|      ! 0 |  4169 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  4170 | `	}` |
|        - |  4171 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|        - |  4172 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|        - |  4173 | `	 * matching how every other typed return reports a missing value.) */` |
|      329 |  4174 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|        5 |  4175 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 |  4176 | `			return SXRET_OK;` |
|        - |  4177 | `		}` |
|        4 |  4178 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"null",` |
|        1 |  4179 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  4180 | `	}` |
|        - |  4181 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|        - |  4182 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|        - |  4183 | `	 * Check by value before the real-class instanceof branch below. */` |
|      325 |  4184 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|       64 |  4185 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|       64 |  4186 | `		if( rcPseudo == 1 ){` |
|       53 |  4187 | `			return SXRET_OK;` |
|        - |  4188 | `		}` |
|       12 |  4189 | `		if( rcPseudo == 0 ){` |
|        9 |  4190 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        4 |  4191 | `				VmSyStringToCStr(&pFunc->sReturnClass,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 |  4192 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  4193 | `		}` |
|        - |  4194 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|        3 |  4195 | `	}` |
|        - |  4196 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  4197 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  4198 | `	 * bNullable=0 here. */` |
|      269 |  4199 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  4200 | `		sxi32 rcU;` |
|      ! 0 |  4201 | `		int bNullable = 0;` |
|      ! 0 |  4202 | `		const char *zExpected = "union";` |
|        - |  4203 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  4204 | `		{` |
|        - |  4205 | `			sxu32 i;` |
|      ! 0 |  4206 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  4207 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  4208 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  4209 | `			}` |
|        - |  4210 | `		}` |
|      ! 0 |  4211 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  4212 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  4213 | `			return SXRET_OK;` |
|        - |  4214 | `		}` |
|      ! 0 |  4215 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4216 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4217 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  4218 | `			zGiven = "null";` |
|      ! 0 |  4219 | `		}else{` |
|      ! 0 |  4220 | `			zGiven = ph7_type_name(pValue);` |
|        - |  4221 | `		}` |
|      ! 0 |  4222 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  4223 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  4224 | `		}` |
|      ! 0 |  4225 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4226 | `	}` |
|        - |  4227 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  4228 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  4229 | `	 * it into the TypeError message. */` |
|      269 |  4230 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        8 |  4231 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  4232 | `		const char *zExpected;` |
|        - |  4233 | `		ph7_class *pExpected;` |
|        8 |  4234 | `		ph7_class *pSelfNow = VmCurrentSelf(pVm);` |
|        8 |  4235 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  4236 | `			pExpected = pSelfNow;` |
|        6 |  4237 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  4238 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  4239 | `		}else{` |
|        5 |  4240 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  4241 | `		}` |
|        8 |  4242 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        8 |  4243 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  4244 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  4245 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4246 | `		}` |
|        8 |  4247 | `		if( pExpected ){` |
|        6 |  4248 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  4249 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  4250 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4251 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4252 | `			}` |
|        2 |  4253 | `		}` |
|        8 |  4254 | `		return SXRET_OK;` |
|        - |  4255 | `	}` |
|        - |  4256 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  4257 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  4258 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  4259 | `	 * via the type-text leading '?'. */` |
|      263 |  4260 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  4261 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  4262 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  4263 | `			return SXRET_OK;` |
|        - |  4264 | `		}` |
|      ! 0 |  4265 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4266 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  4267 | `			"null");` |
|        - |  4268 | `	}` |
|        - |  4269 | `	/* Exact match? Done. */` |
|      257 |  4270 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      251 |  4271 | `		return SXRET_OK;` |
|        - |  4272 | `	}` |
|        - |  4273 | `	/* Object->scalar is never compatible. */` |
|        9 |  4274 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4275 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4276 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4277 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  4278 | `			zGiven);` |
|        - |  4279 | `	}` |
|        - |  4280 | `	/* Array <-> scalar is never compatible. */` |
|        9 |  4281 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  4282 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4283 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  4284 | `			ph7_type_name(pValue));` |
|        - |  4285 | `	}` |
|        - |  4286 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  4287 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  4288 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  4289 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  4290 | `	if( !bStrict` |
|        5 |  4291 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  4292 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        7 |  4293 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  4294 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  4295 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  4296 | `			"string");` |
|        - |  4297 | `	}` |
|        6 |  4298 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  4299 | `		return SXRET_OK;` |
|        - |  4300 | `	}` |
|        4 |  4301 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  4302 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  4303 | `		ph7_type_name(pValue));` |
|      245 |  4304 |  |
|        - |  4305 | `/*` |
|        - |  4306 | ` * Report a fatal named-argument error.` |
|        - |  4307 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  4308 | ` */` |
|        6 |  4309 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        2 |  4310 |  |
|        8 |  4311 | `	const char *zFunc = 0;` |
|        8 |  4312 | `	int nFunc = 0;` |
|        8 |  4313 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        8 |  4314 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        2 |  4315 |  |
|        - |  4316 | `/*` |
|        - |  4317 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  4318 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  4319 | ` * information.` |
|        - |  4320 | ` * ------------------------------------` |
|        - |  4321 | ` * Simple boring wrapper function.` |
|        - |  4322 | ` * ------------------------------------` |
|        - |  4323 | ` */` |
|       24 |  4324 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        4 |  4325 |  |
|        - |  4326 | `	sxi32 rc;` |
|       28 |  4327 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       28 |  4328 | `	return rc;` |
|        4 |  4329 |  |
|        - |  4330 | `/*` |
|        - |  4331 | ` * Resolve function context from the current frame.` |
|        - |  4332 | ` */` |
|     1064 |  4333 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        4 |  4334 |  |
|        - |  4335 | `	VmFrame *pFrame;` |
|        - |  4336 | `	ph7_vm_func *pFunc;` |
|     1068 |  4337 | `	*pzFuncName = 0;` |
|     1068 |  4338 | `	*pnFuncLen = 0;` |
|     1068 |  4339 | `	pFrame = pVm->pFrame;` |
|     1068 |  4340 | `	if( pFrame == 0 ){` |
|      ! 0 |  4341 | `		return;` |
|        - |  4342 | `	}` |
|     1068 |  4343 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1068 |  4344 | `	if( pFrame->pParent == 0 ){` |
|     1038 |  4345 | `		return;` |
|        - |  4346 | `	}` |
|       34 |  4347 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       34 |  4348 | `	if( pFunc == 0 ){` |
|      ! 0 |  4349 | `		return;` |
|        - |  4350 | `	}` |
|       34 |  4351 | `	*pzFuncName = pFunc->sName.zString;` |
|       34 |  4352 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      536 |  4353 |  |
|        - |  4354 | `/*` |
|        - |  4355 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  4356 | ` */` |
|      550 |  4357 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        4 |  4358 |  |
|        - |  4359 | `	SyBlob sOut;` |
|        - |  4360 | `	SyString *pFile;` |
|      554 |  4361 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  4362 | `		return PH7_OK;` |
|        - |  4363 | `	}` |
|      554 |  4364 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  4365 | `		zClass = "Exception";` |
|      ! 0 |  4366 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  4367 | `	}` |
|      554 |  4368 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      526 |  4369 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      261 |  4370 | `	}` |
|      554 |  4371 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      554 |  4372 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      554 |  4373 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      554 |  4374 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      554 |  4375 | `	if( zMsg && nMsg > 0 ){` |
|      554 |  4376 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      554 |  4377 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      275 |  4378 | `	}` |
|      554 |  4379 | `	if( pFile ){` |
|      554 |  4380 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      554 |  4381 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      554 |  4382 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      275 |  4383 | `	}` |
|      554 |  4384 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      554 |  4385 | `	if( pFile ){` |
|      554 |  4386 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      554 |  4387 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      554 |  4388 | `		if( zFuncName && nFuncLen > 0 ){` |
|       34 |  4389 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       19 |  4390 | `		}else{` |
|      524 |  4391 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        4 |  4392 | `		}` |
|      275 |  4393 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  4394 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  4395 | `	}else{` |
|      ! 0 |  4396 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  4397 | `	}` |
|      554 |  4398 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      554 |  4399 | `	if( pFile ){` |
|      554 |  4400 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      554 |  4401 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      554 |  4402 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      554 |  4403 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      275 |  4404 | `	}` |
|      554 |  4405 | `	VmCallErrorHandler(pVm,&sOut);` |
|      554 |  4406 | `	SyBlobRelease(&sOut);` |
|      554 |  4407 | `	return PH7_ABORT;` |
|      279 |  4408 |  |
|        - |  4409 | `/*` |
|        - |  4410 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  4411 | ` *` |
|        - |  4412 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  4413 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  4414 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  4415 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  4416 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  4417 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  4418 | ` */` |
|      974 |  4419 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        5 |  4420 |  |
|      979 |  4421 | `	if( pVm->bCoalesceArmed ){` |
|        8 |  4422 | `		if( pVm->pCoalesceObj ){` |
|        8 |  4423 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  4424 | `		}` |
|        8 |  4425 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        8 |  4426 | `		pVm->pCoalesceObj = 0;` |
|        8 |  4427 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  4428 | `	}` |
|      979 |  4429 |  |
|        - |  4430 | `/*` |
|        - |  4431 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  4432 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  4433 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  4434 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  4435 | ` *` |
|        - |  4436 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  4437 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  4438 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  4439 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  4440 | ` */` |
|        4 |  4441 | `static sxi32 VmThrowFromVm(` |
|        - |  4442 | `	ph7_vm *pVm,` |
|        - |  4443 | `	const char *zClass,` |
|        - |  4444 | `	const char *zMsg,` |
|        - |  4445 | `	sxu32 nMsg` |
|        2 |  4446 | `){` |
|        - |  4447 | `	ph7_class *pClass;` |
|        - |  4448 | `	ph7_class_instance *pThis;` |
|        - |  4449 | `	ph7_class_method *pCons;` |
|        - |  4450 | `	VmFrame *pFrame;` |
|        - |  4451 | `	sxi32 rc;` |
|        6 |  4452 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        6 |  4453 | `	if( pClass == 0 ){` |
|      ! 0 |  4454 | `		return SXERR_ABORT;` |
|        - |  4455 | `	}` |
|        6 |  4456 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        6 |  4457 | `	if( pThis == 0 ){` |
|      ! 0 |  4458 | `		return SXERR_ABORT;` |
|        - |  4459 | `	}` |
|        6 |  4460 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        6 |  4461 | `	if( pCons ){` |
|        - |  4462 | `		ph7_value sArg;` |
|        - |  4463 | `		ph7_value *apArg[1];` |
|        - |  4464 | `		SyString sMsgStr;` |
|        6 |  4465 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        6 |  4466 | `		PH7_MemObjInit(pVm,&sArg);` |
|        6 |  4467 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        6 |  4468 | `		apArg[0] = &sArg;` |
|        6 |  4469 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        6 |  4470 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  4471 | `	}` |
|        6 |  4472 | `	pFrame = pVm->pFrame;` |
|        6 |  4473 | `	if( pFrame ){` |
|        6 |  4474 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        6 |  4475 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  4476 | `	}` |
|        6 |  4477 | `	rc = VmThrowException(pVm,pThis);` |
|        6 |  4478 | `	PH7_ClassInstanceUnref(pThis);` |
|        6 |  4479 | `	return rc;` |
|        4 |  4480 |  |
|        - |  4481 | `/*` |
|        - |  4482 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  4483 | ` */` |
|      586 |  4484 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        5 |  4485 |  |
|        - |  4486 | `	ph7_vm *pVm;` |
|        - |  4487 | `	ph7_class *pClass;` |
|        - |  4488 | `	ph7_class_instance *pThis;` |
|        - |  4489 | `	ph7_class_method *pCons;` |
|        - |  4490 | `	ph7_value sArg;` |
|        - |  4491 | `	ph7_value *apArg[1];` |
|        - |  4492 | `	SyBlob sMsg;` |
|        - |  4493 | `	SyString sMsgStr;` |
|        - |  4494 | `	VmFrame *pFrame;` |
|        - |  4495 | `	va_list ap;` |
|        - |  4496 | `	sxi32 rc;` |
|        - |  4497 |  |
|      591 |  4498 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4499 | `		return PH7_ABORT;` |
|        - |  4500 | `	}` |
|      591 |  4501 | `	pVm = pCtx->pVm;` |
|      591 |  4502 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4503 | `		zClass = "Error";` |
|      ! 0 |  4504 | `	}` |
|      591 |  4505 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      591 |  4506 | `	if( pClass == 0 ){` |
|      ! 0 |  4507 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4508 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  4509 | `			zClass` |
|        - |  4510 | `			);` |
|        - |  4511 | `	}` |
|      591 |  4512 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      591 |  4513 | `	if( pThis == 0 ){` |
|      ! 0 |  4514 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4515 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  4516 | `			);` |
|        - |  4517 | `	}` |
|        - |  4518 |  |
|      591 |  4519 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      591 |  4520 | `	va_start(ap,zFormat);` |
|      591 |  4521 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      591 |  4522 | `	va_end(ap);` |
|        - |  4523 |  |
|      591 |  4524 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      591 |  4525 | `	if( pCons ){` |
|      591 |  4526 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      591 |  4527 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      591 |  4528 | `		apArg[0] = &sArg;` |
|      591 |  4529 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      591 |  4530 | `		PH7_MemObjRelease(&sArg);` |
|      293 |  4531 | `	}` |
|      591 |  4532 | `	SyBlobRelease(&sMsg);` |
|        - |  4533 |  |
|      591 |  4534 | `	pFrame = pVm->pFrame;` |
|      591 |  4535 | `	if( pFrame ){` |
|      591 |  4536 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      591 |  4537 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      293 |  4538 | `	}` |
|      591 |  4539 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      591 |  4540 | `	PH7_ClassInstanceUnref(pThis);` |
|      591 |  4541 | `	if( rc == SXERR_ABORT ){` |
|      506 |  4542 | `		return PH7_ABORT;` |
|        - |  4543 | `	}` |
|       88 |  4544 | `	return PH7_EXCEPTION;` |
|      298 |  4545 |  |
|        - |  4546 | `/*` |
|        - |  4547 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  4548 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  4549 | ` */` |
|      ! 0 |  4550 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  4551 |  |
|        - |  4552 | `	ph7_vm *pVm;` |
|        - |  4553 | `	SyBlob sMsg;` |
|      ! 0 |  4554 | `	const char *zFuncName = 0;` |
|      ! 0 |  4555 | `	int nFuncLen = 0;` |
|        - |  4556 | `	va_list ap;` |
|        - |  4557 | `	sxi32 rc;` |
|        - |  4558 |  |
|      ! 0 |  4559 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4560 | `		return PH7_OK;` |
|        - |  4561 | `	}` |
|      ! 0 |  4562 | `	pVm = pCtx->pVm;` |
|      ! 0 |  4563 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4564 | `		zClass = "Error";` |
|      ! 0 |  4565 | `	}` |
|        - |  4566 |  |
|      ! 0 |  4567 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  4568 |  |
|      ! 0 |  4569 | `	va_start(ap,zFormat);` |
|      ! 0 |  4570 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  4571 | `	va_end(ap);` |
|        - |  4572 |  |
|      ! 0 |  4573 | `	if( pCtx->pFunc ){` |
|      ! 0 |  4574 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  4575 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  4576 | `	}` |
|      ! 0 |  4577 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  4578 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  4579 | `	}` |
|      ! 0 |  4580 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  4581 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  4582 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  4583 | `	return rc;` |
|      ! 0 |  4584 |  |
|        - |  4585 | `/*` |
|        - |  4586 | ` * Save the execution state of a fiber/generator context.` |
|        - |  4587 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  4588 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  4589 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  4590 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  4591 | ` * when VmByteCodeExec returns.` |
|        - |  4592 | ` */` |
|      200 |  4593 | `static sxi32 VmSuspendCtx(` |
|        - |  4594 | `	ph7_vm *pVm,` |
|        - |  4595 | `	ph7_exec_ctx *pCtx,` |
|        - |  4596 | `	sxi32 pc,` |
|        - |  4597 | `	sxi32 nTos` |
|        - |  4598 | `	)` |
|        5 |  4599 |  |
|      100 |  4600 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      205 |  4601 | `	pCtx->pc = pc;` |
|      205 |  4602 | `	pCtx->nTos = nTos;` |
|      205 |  4603 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      205 |  4604 | `	return PH7_SUSPEND;` |
|        5 |  4605 |  |
|        - |  4606 | `/*` |
|        - |  4607 | ` * Resolve named-argument mapping.` |
|        - |  4608 | ` *` |
|        - |  4609 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  4610 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  4611 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  4612 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  4613 | ` * every formal parameter that received a value.` |
|        - |  4614 | ` *` |
|        - |  4615 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  4616 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  4617 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  4618 | ` */` |
|       98 |  4619 | `static sxi32 VmResolveNamedArgs(` |
|        - |  4620 | `	ph7_vm *pVm,` |
|        - |  4621 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  4622 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  4623 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  4624 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  4625 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  4626 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  4627 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  4628 |  |
|        3 |  4629 |  |
|      101 |  4630 | `	sxi32 posIdx = 0;` |
|        - |  4631 | `	sxu32 i;` |
|        - |  4632 | `	char zErrMsg[256];` |
|      101 |  4633 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      297 |  4634 | `	for( i = 0; i < nActual; i++ ){` |
|      199 |  4635 | `		aSlot[i] = -2;` |
|      101 |  4636 | `	}` |
|      291 |  4637 | `	for( i = 0; i < nActual; i++ ){` |
|      287 |  4638 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  4639 | `			/* Named argument — find formal by name */` |
|      185 |  4640 | `			int found = 0;` |
|        - |  4641 | `			sxu32 k;` |
|      305 |  4642 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  4643 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      282 |  4644 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  4645 | `						pMap->aNames[i].zString,` |
|      402 |  4646 | `						pMap->aNames[i].nByte) == 0 ){` |
|      173 |  4647 | `					if( aUsed[k] ){` |
|        8 |  4648 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4649 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  4650 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        6 |  4651 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        6 |  4652 | `						return PH7_ABORT;` |
|        - |  4653 | `					}` |
|      168 |  4654 | `					aSlot[i] = (sxi32)k;` |
|      168 |  4655 | `					aUsed[k] = 1;` |
|      168 |  4656 | `					found = 1;` |
|      168 |  4657 | `					break;` |
|        - |  4658 | `				}` |
|       62 |  4659 | `			}` |
|      181 |  4660 | `			if( !found ){` |
|       14 |  4661 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  4662 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  4663 | `				}else{` |
|        4 |  4664 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4665 | `						"Unknown named parameter $%.*s",` |
|        2 |  4666 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  4667 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  4668 | `					return PH7_ABORT;` |
|        - |  4669 | `				}` |
|        5 |  4670 | `			}` |
|       90 |  4671 | `		}else{` |
|        - |  4672 | `			/* Positional argument */` |
|       16 |  4673 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  4674 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  4675 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4676 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  4677 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  4678 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  4679 | `					return PH7_ABORT;` |
|        - |  4680 | `				}` |
|       16 |  4681 | `				aSlot[i] = posIdx;` |
|       16 |  4682 | `				aUsed[posIdx] = 1;` |
|        7 |  4683 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  4684 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  4685 | `			}` |
|       16 |  4686 | `			posIdx++;` |
|        - |  4687 | `		}` |
|       98 |  4688 | `	}` |
|       93 |  4689 | `	return SXRET_OK;` |
|       52 |  4690 |  |
|        - |  4691 | `/*` |
|        - |  4692 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|        - |  4693 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|        - |  4694 | ` */` |
|       42 |  4695 | `static int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|        5 |  4696 |  |
|       47 |  4697 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|       34 |  4698 | `		return 0;` |
|        - |  4699 | `	}` |
|       15 |  4700 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       26 |  4701 |  |
|        - |  4702 | `/*` |
|        - |  4703 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|        - |  4704 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|        - |  4705 | ` * preserved (later wins), integer keys are renumbered.` |
|        - |  4706 | ` */` |
|       10 |  4707 | `static sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 |  4708 |  |
|       11 |  4709 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|        5 |  4710 | `	(void)pVm;` |
|       11 |  4711 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|       11 |  4712 | `	return SXRET_OK;` |
|        1 |  4713 |  |
|        - |  4714 | `/*` |
|        - |  4715 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|        - |  4716 | ` * collect values positionally (keys ignored) into a temp array.` |
|        - |  4717 | ` */` |
|        6 |  4718 | `static sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 |  4719 |  |
|        3 |  4720 | `	(void)pVm; (void)pKey;` |
|        7 |  4721 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|        7 |  4722 | `	return SXRET_OK;` |
|        1 |  4723 |  |
|        - |  4724 | `/*` |
|        - |  4725 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  4726 | ` *` |
|        - |  4727 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  4728 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  4729 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  4730 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  4731 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  4732 | ` * then the program execution is halted.` |
|        - |  4733 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  4734 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  4735 | ` * or to reset the VM to it's initial state.` |
|        - |  4736 | ` */` |
|    50544 |  4737 | `static sxi32 VmByteCodeExec(` |
|        - |  4738 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  4739 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  4740 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  4741 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  4742 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  4743 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  4744 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  4745 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  4746 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  4747 | `	int bReturnPropagates /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value to pVm->sCatchReturn for the enclosing try handler to return. */` |
|        - |  4748 | `	)` |
|        5 |  4749 |  |
|        - |  4750 | `	VmInstr *pInstr;` |
|        - |  4751 | `	ph7_value *pTos;` |
|        - |  4752 | `	SySet aArg;` |
|        - |  4753 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  4754 | `	VmFrame *pEntryFrame;  /* Active frame at entry (for return-unwind frame teardown) */` |
|        - |  4755 | `	sxi32 pc;` |
|        - |  4756 | `	sxi32 rc;` |
|        - |  4757 | `	/* Argument container */` |
|    50549 |  4758 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    50549 |  4759 | `	if( nTos < 0 ){` |
|    46805 |  4760 | `		pTos = &pStack[-1];` |
|    23405 |  4761 | `	}else{` |
|     3749 |  4762 | `		pTos = &pStack[nTos];` |
|        - |  4763 | `	}` |
|    50549 |  4764 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    50549 |  4765 | `	pEntryFrame = pVm->pFrame;` |
|    50549 |  4766 | `	pc = nPc;` |
|        - |  4767 | `/*` |
|        - |  4768 | ` * Route an enforcement helper's return code from inside the main switch:` |
|        - |  4769 | ` * proceed on SXRET_OK, abort on PH7_ABORT, and on PH7_EXCEPTION jump to the` |
|        - |  4770 | ` * enclosing catch block (if any) or unwind out of the VM loop.` |
|        - |  4771 | ` */` |
|        - |  4772 | `#define PH7_DISPATCH_ENFORCE_RC(rcVar) \` |
|        - |  4773 | `	if( (rcVar) == PH7_ABORT ){ goto Abort; } \` |
|        - |  4774 | `	if( (rcVar) == PH7_EXCEPTION ){ \` |
|        - |  4775 | `		VmFrame *_pFrmE = pVm->pFrame; \` |
|        - |  4776 | `		if( _pFrmE && (_pFrmE->iFlags & VM_FRAME_EXCEPTION) && _pFrmE->iExceptionJump > 0 ){ \` |
|        - |  4777 | `			pc = _pFrmE->iExceptionJump - 1; \` |
|        - |  4778 | `			break; \` |
|        - |  4779 | `		} \` |
|        - |  4780 | `		goto Exception; \` |
|        - |  4781 | `	}` |
|        - |  4782 | `/*` |
|        - |  4783 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  4784 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  4785 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  4786 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  4787 | ` */` |
|        - |  4788 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  4789 | `	{ \` |
|        - |  4790 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  4791 | `		PH7_DISPATCH_ENFORCE_RC(_rcT) \` |
|        - |  4792 | `	}` |
|        - |  4793 | `/*` |
|        - |  4794 | `` * Readonly enforcement helper for the in-place mutation opcodes (`++`/`--`),`` |
|        - |  4795 | ` * which bypass the typed-store path. Throws "Cannot modify readonly property"` |
|        - |  4796 | ` * when the lvalue slot is a readonly property, otherwise proceeds. Must be used` |
|        - |  4797 | ` * inside a case of the main switch.` |
|        - |  4798 | ` */` |
|        - |  4799 | `#define PH7_ENFORCE_READONLY_MUTATE(nIdxArg) \` |
|        - |  4800 | `	{ \` |
|        - |  4801 | `		sxi32 _rcR = VmCheckReadonlyMutate(&(*pVm),(nIdxArg)); \` |
|        - |  4802 | `		PH7_DISPATCH_ENFORCE_RC(_rcR) \` |
|        - |  4803 | `	}` |
|        - |  4804 | `	/* Execute as much as we can */` |
|  6117831 |  4805 | `	for(;;){` |
|        - |  4806 | `		/* Fetch the instruction to execute */` |
| 12234307 |  4807 | `		pInstr = &aInstr[pc];` |
| 12234307 |  4808 | `		rc = SXRET_OK;` |
|        - |  4809 | `/*` |
|        - |  4810 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4811 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4812 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4813 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4814 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4815 | ` */` |
| 12234307 |  4816 | `		switch(pInstr->iOp){` |
|        - |  4817 | `/*` |
|        - |  4818 | ` * DONE: P1 * *` |
|        - |  4819 | ` *` |
|        - |  4820 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4821 | ` * and return immediately.` |
|        - |  4822 | ` */` |
|    24764 |  4823 | `case PH7_OP_DONE:` |
|    49533 |  4824 | `	if( pInstr->iP2 && bReturnPropagates ){` |
|        - |  4825 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - |  4826 | `		 * to pVm->sCatchReturn; the enclosing try's OP_THROW / OP_POP_EXCEPTION` |
|        - |  4827 | `		 * handler materializes it into the function's result and returns. Drain` |
|        - |  4828 | `		 * any finally opened within this body first (nested try/finally inside` |
|        - |  4829 | `		 * the catch), which may itself override sCatchReturn. */` |
|       36 |  4830 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|       34 |  4831 | `			PH7_MemObjStore(pTos,&pVm->sCatchReturn);` |
|       34 |  4832 | `			VmPopOperand(&pTos,1);` |
|       18 |  4833 | `		}else{` |
|        3 |  4834 | ``			PH7_MemObjRelease(&pVm->sCatchReturn); /* bare `return;` -> null */`` |
|        - |  4835 | `		}` |
|       36 |  4836 | `		pVm->bReturnRequested = 1;` |
|       36 |  4837 | `		rc = VmDrainFinally(&(*pVm),nExceptionBase);` |
|       36 |  4838 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4839 | `			goto Abort;` |
|        - |  4840 | `		}` |
|       36 |  4841 | `		goto Done;` |
|        - |  4842 | `	}` |
|        - |  4843 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4844 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4845 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4846 | `	 * callback trampolines, and the main script. */` |
|    49494 |  4847 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0` |
|      491 |  4848 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  4849 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  4850 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  4851 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  4852 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  4853 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  4854 | `		 * exception. */` |
|      485 |  4855 | `		ph7_value *pRetVal = 0;` |
|      485 |  4856 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      331 |  4857 | `			pRetVal = pTos;` |
|      163 |  4858 | `		}` |
|      485 |  4859 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      485 |  4860 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      479 |  4861 | `		if( rc == PH7_EXCEPTION ){` |
|        7 |  4862 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|        7 |  4863 | `				PH7_MemObjRelease(pTos);` |
|        7 |  4864 | `				pTos--;` |
|        3 |  4865 | `			}` |
|        7 |  4866 | `			goto Exception;` |
|        - |  4867 | `		}` |
|        - |  4868 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4869 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4870 | `		 * defensively we clear the pointer after a successful check). */` |
|      473 |  4871 | `		pEnforceRetFunc = 0;` |
|      234 |  4872 | `	}` |
|    49487 |  4873 | `	if( pInstr->iP1 ){` |
|        - |  4874 | `#ifdef UNTRUST` |
|        - |  4875 | `		if( pTos < pStack ){` |
|        - |  4876 | `			goto Abort;` |
|        - |  4877 | `		}` |
|        - |  4878 | `#endif` |
|    31355 |  4879 | `		if( pLastRef ){` |
|    17617 |  4880 | `			*pLastRef = pTos->nIdx;` |
|     8806 |  4881 | `		}` |
|    31355 |  4882 | `		if( pResult ){` |
|        - |  4883 | `			/* Execution result */` |
|    29619 |  4884 | `			PH7_MemObjStore(pTos,pResult);` |
|    14807 |  4885 | `		}` |
|    31355 |  4886 | `		VmPopOperand(&pTos,1);` |
|    33812 |  4887 | `	}else if( pLastRef ){` |
|        - |  4888 | `		/* Nothing referenced */` |
|     2165 |  4889 | `		*pLastRef = SXU32_HIGH;` |
|     1080 |  4890 | `	}` |
|        - |  4891 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4892 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4893 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4894 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4895 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4896 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4897 | `	 * block can override it (the finally writes pVm->sCatchReturn, materialized` |
|        - |  4898 | `	 * below).` |
|        - |  4899 | `	 */` |
|    49487 |  4900 | `	rc = VmDrainFinally(&(*pVm),nExceptionBase);` |
|    49487 |  4901 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4902 | `		goto Abort;` |
|        - |  4903 | `	}` |
|    49487 |  4904 | `	if( pVm->bReturnRequested && !bReturnPropagates ){` |
|        - |  4905 | `		/* A drained finally issued a 'return' that overrides this one. */` |
|        8 |  4906 | `		VmMaterializeCatchReturn(&(*pVm),pResult,pEntryFrame);` |
|        3 |  4907 | `	}` |
|    49487 |  4908 | `	goto Done;` |
|        - |  4909 | `/*` |
|        - |  4910 | ` * HALT: P1 * *` |
|        - |  4911 | ` *` |
|        - |  4912 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4913 | ` * and abort immediately.` |
|        - |  4914 | ` */` |
|        7 |  4915 | `case PH7_OP_HALT:` |
|       18 |  4916 | `	if( pInstr->iP1 ){` |
|        - |  4917 | `#ifdef UNTRUST` |
|        - |  4918 | `		if( pTos < pStack ){` |
|        - |  4919 | `			goto Abort;` |
|        - |  4920 | `		}` |
|        - |  4921 | `#endif` |
|       18 |  4922 | `		if( pLastRef ){` |
|        3 |  4923 | `			*pLastRef = pTos->nIdx;` |
|        1 |  4924 | `		}` |
|       18 |  4925 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       13 |  4926 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4927 | `				/* Output the exit message */` |
|       18 |  4928 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        5 |  4929 | `					pVm->sVmConsumer.pUserData);` |
|       13 |  4930 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        8 |  4931 | `			}` |
|       11 |  4932 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4933 | `			/* Record exit status */` |
|        6 |  4934 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4935 | `		}` |
|       18 |  4936 | `		VmPopOperand(&pTos,1);` |
|        7 |  4937 | `	}else if( pLastRef ){` |
|        - |  4938 | `		/* Nothing referenced */` |
|      ! 0 |  4939 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4940 | `	}` |
|        - |  4941 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  4942 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  4943 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  4944 | `	 */` |
|       18 |  4945 | `	pVm->bHaltRequested = 1;` |
|       18 |  4946 | `	goto Abort;` |
|        - |  4947 | `/*` |
|        - |  4948 | ` * JMP: * P2 *` |
|        - |  4949 | ` *` |
|        - |  4950 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4951 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4952 | ` */` |
|   259493 |  4953 | `case PH7_OP_JMP:` |
|   519076 |  4954 | `	pc = pInstr->iP2 - 1;` |
|   519076 |  4955 | `	break;` |
|        - |  4956 | `/*` |
|        - |  4957 | ` * JZ: P1 P2 *` |
|        - |  4958 | ` *` |
|        - |  4959 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4960 | ` * entry in the stack if P1 is zero.` |
|        - |  4961 | ` */` |
|   618605 |  4962 | `case PH7_OP_JZ:` |
|        - |  4963 | `#ifdef UNTRUST` |
|        - |  4964 | `	if( pTos < pStack ){` |
|        - |  4965 | `		goto Abort;` |
|        - |  4966 | `	}` |
|        - |  4967 | `#endif` |
|        - |  4968 | `	/* Get a boolean value */` |
|  1237385 |  4969 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      175 |  4970 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4971 | `	}` |
|  1237385 |  4972 | `	if( !pTos->x.iVal ){` |
|        - |  4973 | `		/* Take the jump */` |
|   639679 |  4974 | `		pc = pInstr->iP2 - 1;` |
|   319837 |  4975 | `	}` |
|  1237385 |  4976 | `	if( !pInstr->iP1 ){` |
|   982236 |  4977 | `		VmPopOperand(&pTos,1);` |
|   491158 |  4978 | `	}` |
|  1237385 |  4979 | `	break;` |
|        - |  4980 | `/*` |
|        - |  4981 | ` * JNZ: P1 P2 *` |
|        - |  4982 | ` *` |
|        - |  4983 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4984 | ` * entry in the stack if P1 is zero.` |
|        - |  4985 | ` */` |
|    64320 |  4986 | `case PH7_OP_JNZ:` |
|        - |  4987 | `#ifdef UNTRUST` |
|        - |  4988 | `	if( pTos < pStack ){` |
|        - |  4989 | `		goto Abort;` |
|        - |  4990 | `	}` |
|        - |  4991 | `#endif` |
|        - |  4992 | `	/* Get a boolean value */` |
|   128645 |  4993 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4994 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4995 | `	}` |
|   128645 |  4996 | `	if( pTos->x.iVal ){` |
|        - |  4997 | `		/* Take the jump */` |
|     5783 |  4998 | `		pc = pInstr->iP2 - 1;` |
|     2889 |  4999 | `	}` |
|   128645 |  5000 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  5001 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5002 | `	}` |
|   128645 |  5003 | `	break;` |
|        - |  5004 | `/*` |
|        - |  5005 | ` * NOOP: * * *` |
|        - |  5006 | ` *` |
|        - |  5007 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  5008 | ` * destination.` |
|        - |  5009 | ` */` |
|      ! 0 |  5010 | `case PH7_OP_NOOP:` |
|      ! 0 |  5011 | `	break;` |
|        - |  5012 | `/*` |
|        - |  5013 | ` * POP: P1 * *` |
|        - |  5014 | ` *` |
|        - |  5015 | ` * Pop P1 elements from the operand stack.` |
|        - |  5016 | ` */` |
|   478522 |  5017 | `case PH7_OP_POP: {` |
|   957134 |  5018 | `	sxi32 n = pInstr->iP1;` |
|   957134 |  5019 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  5020 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       53 |  5021 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  5022 | `	}` |
|   957134 |  5023 | `	VmPopOperand(&pTos,n);` |
|   957134 |  5024 | `	break;` |
|        - |  5025 | `				 }` |
|        - |  5026 | `/*` |
|        - |  5027 | ` * DUP: * * *` |
|        - |  5028 | ` *` |
|        - |  5029 | ` * Duplicate the top of the stack.` |
|        - |  5030 | ` */` |
|       41 |  5031 | `case PH7_OP_DUP:` |
|        - |  5032 | `#ifdef UNTRUST` |
|        - |  5033 | `	if( pTos < pStack ){` |
|        - |  5034 | `		goto Abort;` |
|        - |  5035 | `	}` |
|        - |  5036 | `#endif` |
|       84 |  5037 | `	pTos++;` |
|       84 |  5038 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  5039 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  5040 | `	break;` |
|        - |  5041 | `/*` |
|        - |  5042 | ` * NSSWITCH: * * P3` |
|        - |  5043 | ` *` |
|        - |  5044 | ` * Switch the active namespace at runtime.` |
|        - |  5045 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  5046 | ` */` |
|     8106 |  5047 | `case PH7_OP_NSSWITCH:` |
|    16217 |  5048 | `	SyBlobReset(&pVm->sNamespace);` |
|    16217 |  5049 | `	if( pInstr->p3 ){` |
|      103 |  5050 | `		const char *zNs = (const char *)pInstr->p3;` |
|      103 |  5051 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       49 |  5052 | `	}` |
|        - |  5053 | `	/* Clear namespace-scoped use-const imports */` |
|    16217 |  5054 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    16217 |  5055 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    16217 |  5056 | `	break;` |
|        - |  5057 | `/* OP_USECONST P1 * P3` |
|        - |  5058 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  5059 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  5060 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  5061 | ` */` |
|        7 |  5062 | `case PH7_OP_USECONST: {` |
|       16 |  5063 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  5064 | `	if( azPair ){` |
|       16 |  5065 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  5066 | `	}` |
|       16 |  5067 | `	break;` |
|        - |  5068 | `				}` |
|        - |  5069 | `/*` |
|        - |  5070 | ` * CVT_INT: * * *` |
|        - |  5071 | ` *` |
|        - |  5072 | ` * Force the top of the stack to be an integer.` |
|        - |  5073 | ` */` |
|       80 |  5074 | `case PH7_OP_CVT_INT:` |
|        - |  5075 | `#ifdef UNTRUST` |
|        - |  5076 | `	if( pTos < pStack ){` |
|        - |  5077 | `		goto Abort;` |
|        - |  5078 | `	}` |
|        - |  5079 | `#endif` |
|      165 |  5080 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      115 |  5081 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  5082 | `	}` |
|        - |  5083 | `	/* Invalidate any prior representation */` |
|      165 |  5084 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      165 |  5085 | `	break;` |
|        - |  5086 | `/*` |
|        - |  5087 | ` * CVT_REAL: * * *` |
|        - |  5088 | ` *` |
|        - |  5089 | ` * Force the top of the stack to be a real.` |
|        - |  5090 | ` */` |
|        7 |  5091 | `case PH7_OP_CVT_REAL:` |
|        - |  5092 | `#ifdef UNTRUST` |
|        - |  5093 | `	if( pTos < pStack ){` |
|        - |  5094 | `		goto Abort;` |
|        - |  5095 | `	}` |
|        - |  5096 | `#endif` |
|       15 |  5097 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 |  5098 | `		PH7_MemObjToReal(pTos);` |
|        5 |  5099 | `	}` |
|        - |  5100 | `	/* Invalidate any prior representation */` |
|       15 |  5101 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       15 |  5102 | `	break;` |
|        - |  5103 | `/*` |
|        - |  5104 | ` * CVT_STR: * * *` |
|        - |  5105 | ` *` |
|        - |  5106 | ` * Force the top of the stack to be a string.` |
|        - |  5107 | ` */` |
|      163 |  5108 | `case PH7_OP_CVT_STR:` |
|        - |  5109 | `#ifdef UNTRUST` |
|        - |  5110 | `	if( pTos < pStack ){` |
|        - |  5111 | `		goto Abort;` |
|        - |  5112 | `	}` |
|        - |  5113 | `#endif` |
|      331 |  5114 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      311 |  5115 | `		PH7_MemObjToString(pTos);` |
|      153 |  5116 | `	}` |
|      331 |  5117 | `	break;` |
|        - |  5118 | `/*` |
|        - |  5119 | ` * CVT_BOOL: * * *` |
|        - |  5120 | ` *` |
|        - |  5121 | ` * Force the top of the stack to be a boolean.` |
|        - |  5122 | ` */` |
|        5 |  5123 | `case PH7_OP_CVT_BOOL:` |
|        - |  5124 | `#ifdef UNTRUST` |
|        - |  5125 | `	if( pTos < pStack ){` |
|        - |  5126 | `		goto Abort;` |
|        - |  5127 | `	}` |
|        - |  5128 | `#endif` |
|       11 |  5129 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  5130 | `		PH7_MemObjToBool(pTos);` |
|        3 |  5131 | `	}` |
|       11 |  5132 | `	break;` |
|        - |  5133 | `/*` |
|        - |  5134 | ` * CVT_NULL: * * *` |
|        - |  5135 | ` *` |
|        - |  5136 | ` * Nullify the top of the stack.` |
|        - |  5137 | ` */` |
|        3 |  5138 | `case PH7_OP_CVT_NULL:` |
|        - |  5139 | `#ifdef UNTRUST` |
|        - |  5140 | `	if( pTos < pStack ){` |
|        - |  5141 | `		goto Abort;` |
|        - |  5142 | `	}` |
|        - |  5143 | `#endif` |
|        7 |  5144 | `	PH7_MemObjRelease(pTos);` |
|        7 |  5145 | `	break;` |
|        - |  5146 | `/*` |
|        - |  5147 | ` * CVT_NUMC: * * *` |
|        - |  5148 | ` *` |
|        - |  5149 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  5150 | ` */` |
|      ! 0 |  5151 | `case PH7_OP_CVT_NUMC:` |
|        - |  5152 | `#ifdef UNTRUST` |
|        - |  5153 | `	if( pTos < pStack ){` |
|        - |  5154 | `		goto Abort;` |
|        - |  5155 | `	}` |
|        - |  5156 | `#endif` |
|        - |  5157 | `	/* Force a numeric cast */` |
|      ! 0 |  5158 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  5159 | `	break;` |
|        - |  5160 | `/*` |
|        - |  5161 | ` * CVT_ARRAY: * * *` |
|        - |  5162 | ` *` |
|        - |  5163 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  5164 | ` */` |
|       10 |  5165 | `case PH7_OP_CVT_ARRAY:` |
|        - |  5166 | `#ifdef UNTRUST` |
|        - |  5167 | `	if( pTos < pStack ){` |
|        - |  5168 | `		goto Abort;` |
|        - |  5169 | `	}` |
|        - |  5170 | `#endif` |
|        - |  5171 | `	/* Force a hashmap cast */` |
|       21 |  5172 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  5173 | `	if( rc != SXRET_OK ){` |
|        - |  5174 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  5175 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  5176 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  5177 | `	}` |
|       21 |  5178 | `	break;` |
|        - |  5179 | `/*` |
|        - |  5180 | ` * CVT_OBJ: * * *` |
|        - |  5181 | ` *` |
|        - |  5182 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  5183 | ` */` |
|        8 |  5184 | `case PH7_OP_CVT_OBJ:` |
|        - |  5185 | `#ifdef UNTRUST` |
|        - |  5186 | `	if( pTos < pStack ){` |
|        - |  5187 | `		goto Abort;` |
|        - |  5188 | `	}` |
|        - |  5189 | `#endif` |
|       17 |  5190 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  5191 | `		/* Force a 'stdClass()' cast */` |
|       17 |  5192 | `		PH7_MemObjToObject(pTos);` |
|        8 |  5193 | `	}` |
|       17 |  5194 | `	break;` |
|        - |  5195 | `/*` |
|        - |  5196 | ` * ERR_CTRL * * *` |
|        - |  5197 | ` *` |
|        - |  5198 | ` * Error control operator.` |
|        - |  5199 | ` */` |
|    16581 |  5200 | `case PH7_OP_ERR_CTRL:` |
|        - |  5201 | `	/*` |
|        - |  5202 | `	 * TICKET 1433-038:` |
|        - |  5203 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  5204 | `	 * use the public API,to control error output.` |
|        - |  5205 | `	 */` |
|    33162 |  5206 | `	break;` |
|        - |  5207 | `/*` |
|        - |  5208 | ` * IS_A * * *` |
|        - |  5209 | ` *` |
|        - |  5210 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  5211 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  5212 | ` * holding a class name or an object).` |
|        - |  5213 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  5214 | ` */` |
|       77 |  5215 | `case PH7_OP_IS_A:{` |
|      159 |  5216 | `	ph7_value *pNos = &pTos[-1];` |
|      159 |  5217 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  5218 | `#ifdef UNTRUST` |
|        - |  5219 | `	if( pNos < pStack ){` |
|        - |  5220 | `		goto Abort;` |
|        - |  5221 | `	}` |
|        - |  5222 | `#endif` |
|      159 |  5223 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      157 |  5224 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      157 |  5225 | `		ph7_class *pClass = 0;` |
|        - |  5226 | `		/* Extract the target class */` |
|      157 |  5227 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5228 | `			/* Instance already loaded */` |
|      ! 0 |  5229 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      157 |  5230 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      157 |  5231 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      157 |  5232 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  5233 | `			/* Handle self/static/parent keywords */` |
|      157 |  5234 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        6 |  5235 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      155 |  5236 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  5237 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      154 |  5238 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        6 |  5239 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        6 |  5240 | `				if( pSelf && pSelf->pBase ){` |
|        6 |  5241 | `					pClass = pSelf->pBase;` |
|        2 |  5242 | `				}` |
|        4 |  5243 | `			}else{` |
|      147 |  5244 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5245 | `			}` |
|       76 |  5246 | `		}` |
|      157 |  5247 | `		if( pClass ){` |
|        - |  5248 | `			/* Perform the query */` |
|      157 |  5249 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       76 |  5250 | `		}` |
|       76 |  5251 | `	}` |
|        - |  5252 | `	/* Push result */` |
|      159 |  5253 | `	VmPopOperand(&pTos,1);` |
|      159 |  5254 | `	PH7_MemObjRelease(pTos);` |
|      159 |  5255 | `	pTos->x.iVal = iRes;` |
|      159 |  5256 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      159 |  5257 | `	break;` |
|        - |  5258 | `				 }` |
|        - |  5259 |  |
|        - |  5260 | `/*` |
|        - |  5261 | ` * LOADC P1 P2 *` |
|        - |  5262 | ` *` |
|        - |  5263 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  5264 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  5265 | ` */` |
|  1050689 |  5266 | `case PH7_OP_LOADC: {` |
|        - |  5267 | `	ph7_value *pObj;` |
|        - |  5268 | `	/* Reserve a room */` |
|  2101468 |  5269 | `	pTos++;` |
|  3141985 |  5270 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2101468 |  5271 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  5272 | `			SyHashEntry *pEntry;` |
|        - |  5273 | `			/* Check use const imports first — imports take precedence */` |
|        - |  5274 | `			{` |
|        - |  5275 | `				SyHashEntry *pConstImport;` |
|    30782 |  5276 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    20518 |  5277 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    20523 |  5278 | `				if( pConstImport ){` |
|       11 |  5279 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  5280 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  5281 | `					if( pEntry ){` |
|       11 |  5282 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  5283 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  5284 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  5285 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  5286 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  5287 | `						break;` |
|        - |  5288 | `					}` |
|        - |  5289 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  5290 | `				}` |
|        - |  5291 | `			}` |
|        - |  5292 | `			/* Candidate for expansion via user defined callbacks */` |
|    20513 |  5293 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    20513 |  5294 | `			if( pEntry ){` |
|    20507 |  5295 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  5296 | `				/* Set a NULL default value */` |
|    20507 |  5297 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    20507 |  5298 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  5299 | `				/* Invoke the callback and deal with the expanded value */` |
|    20507 |  5300 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  5301 | `				/* Mark as constant */` |
|    20507 |  5302 | `				pTos->nIdx = SXU32_HIGH;` |
|    20507 |  5303 | `				break;` |
|        - |  5304 | `			}` |
|        - |  5305 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  5306 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  5307 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  5308 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  5309 | `			{` |
|        9 |  5310 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        9 |  5311 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  5312 | `				sxu32 j;` |
|        9 |  5313 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       25 |  5314 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  5315 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  5316 | `				}` |
|        9 |  5317 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  5318 | `					/* Try current_namespace\name */` |
|      ! 0 |  5319 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  5320 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  5321 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  5322 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  5323 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  5324 | `					if( pEntry ){` |
|      ! 0 |  5325 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  5326 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5327 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  5328 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  5329 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5330 | `						break;` |
|        - |  5331 | `					}` |
|        - |  5332 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  5333 | `				}` |
|        9 |  5334 | `				if( isQualified ){` |
|        - |  5335 | `					/* Qualified name: must be a real constant. */` |
|        3 |  5336 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  5337 | `					SyBlob sErr;` |
|        3 |  5338 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  5339 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  5340 | `					if( pErrFile ){` |
|        3 |  5341 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  5342 | `					}` |
|        3 |  5343 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  5344 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  5345 | `					SyBlobRelease(&sErr);` |
|        3 |  5346 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  5347 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  5348 | `					goto LoadC_Done;` |
|        - |  5349 | `				}` |
|        - |  5350 | `			}` |
|        2 |  5351 | `		}` |
|  2080954 |  5352 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1040522 |  5353 | `	}else{` |
|        - |  5354 | `		/* Set a NULL value */` |
|      ! 0 |  5355 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5356 | `	}` |
|  1040433 |  5357 | `LoadC_Done:` |
|        - |  5358 | `	/* Mark as constant */` |
|  2080956 |  5359 | `	pTos->nIdx = SXU32_HIGH;` |
|  2080956 |  5360 | `	break;` |
|        - |  5361 | `				  }` |
|        - |  5362 | `/*` |
|        - |  5363 | ` * LOAD: P1 * P3` |
|        - |  5364 | ` *` |
|        - |  5365 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  5366 | ` * from the P3 operand.` |
|        - |  5367 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  5368 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  5369 | ` */` |
|  1635415 |  5370 | `case PH7_OP_LOAD:{` |
|        - |  5371 | `	ph7_value *pObj;` |
|        - |  5372 | `	SyString sName;` |
|  3271260 |  5373 | `	if( pInstr->p3 == 0 ){` |
|        - |  5374 | `		/* Take the variable name from the top of the stack */` |
|        - |  5375 | `#ifdef UNTRUST` |
|        - |  5376 | `		if( pTos < pStack ){` |
|        - |  5377 | `			goto Abort;` |
|        - |  5378 | `		}` |
|        - |  5379 | `#endif` |
|        - |  5380 | `		/* Force a string cast */` |
|       19 |  5381 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5382 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5383 | `		}` |
|       19 |  5384 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  5385 | `	}else{` |
|  3271242 |  5386 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5387 | `		/* Reserve a room for the target object */` |
|  3271242 |  5388 | `		pTos++;` |
|        - |  5389 | `	}` |
|        - |  5390 | `	/* Extract the requested memory object */` |
|  3271260 |  5391 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3271260 |  5392 | `	if( pObj == 0 ){` |
|      879 |  5393 | `		if( pInstr->iP1 ){` |
|        - |  5394 | `			/* Variable not found,load NULL */` |
|      879 |  5395 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5396 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5397 | `			}else{` |
|      879 |  5398 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5399 | `			}` |
|      879 |  5400 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1635857 |  5401 | `			break;` |
|      ! 0 |  5402 | `		}else{` |
|        - |  5403 | `			/* Fatal error */` |
|      ! 0 |  5404 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5405 | `			goto Abort;` |
|        - |  5406 | `		}` |
|        - |  5407 | `	}` |
|        - |  5408 | `	/* Load variable contents */` |
|  3270386 |  5409 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3270386 |  5410 | `	pTos->nIdx = pObj->nIdx;` |
|  3270386 |  5411 | `	break;` |
|        - |  5412 | `				   }` |
|        - |  5413 | `/*` |
|        - |  5414 | ` * LOAD_MAP P1 * *` |
|        - |  5415 | ` *` |
|        - |  5416 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  5417 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  5418 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  5419 | ` */` |
|    23518 |  5420 | `case PH7_OP_LOAD_MAP: {` |
|        - |  5421 | `	ph7_hashmap *pMap;` |
|        - |  5422 | `	/* Allocate a new hashmap instance */` |
|    47041 |  5423 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    47041 |  5424 | `	if( pMap == 0 ){` |
|      ! 0 |  5425 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5426 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  5427 | `		goto Abort;` |
|        - |  5428 | `	}` |
|    47041 |  5429 | `	if( pInstr->iP1 > 0 ){` |
|     2839 |  5430 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2839 |  5431 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  5432 | `		/* Perform the insertion */` |
|     8661 |  5433 | `		while( pEntry < pTos ){` |
|     5845 |  5434 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  5435 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  5436 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  5437 | `				 * renumbered. Same routine that backs array_merge. */` |
|       77 |  5438 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       53 |  5439 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       53 |  5440 | `					if( rcMerge != SXRET_OK ){` |
|        - |  5441 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  5442 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  5443 | `						 * map dangling. */` |
|      ! 0 |  5444 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5445 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  5446 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  5447 | `						break;` |
|        1 |  5448 | `					}` |
|       51 |  5449 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|        - |  5450 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|        - |  5451 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|        5 |  5452 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|        5 |  5453 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 |  5454 | `						rcSpread = rcW;` |
|      ! 0 |  5455 | `						break;` |
|        - |  5456 | `					}` |
|        3 |  5457 | `				}else{` |
|        - |  5458 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       21 |  5459 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       21 |  5460 | `					break;` |
|        1 |  5461 | `				}` |
|     5799 |  5462 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  5463 | `				/* Insertion by reference */` |
|      151 |  5464 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  5465 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  5466 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  5467 | `					);` |
|       51 |  5468 | `			}else{` |
|        - |  5469 | `				/* Standard insertion */` |
|     8504 |  5470 | `				PH7_HashmapInsert(pMap,` |
|     5666 |  5471 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2833 |  5472 | `					&pEntry[1]` |
|        - |  5473 | `				);` |
|        - |  5474 | `			}` |
|        - |  5475 | `			/* Next pair on the stack */` |
|     5827 |  5476 | `			pEntry += 2;` |
|        5 |  5477 | `		}` |
|        - |  5478 | `		/* Pop P1 elements */` |
|     2839 |  5479 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2839 |  5480 | `		if( rcSpread != SXRET_OK ){` |
|        - |  5481 | `			/* Discard the partially-built map and propagate the exception. */` |
|       21 |  5482 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       21 |  5483 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  5484 | `				goto Abort;` |
|        - |  5485 | `			}` |
|        - |  5486 | `			{` |
|       21 |  5487 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       21 |  5488 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        6 |  5489 | `					pc = pFrm2->iExceptionJump - 1;` |
|        6 |  5490 | `					break;` |
|        - |  5491 | `				}` |
|        - |  5492 | `			}` |
|       15 |  5493 | `			goto Exception;` |
|        - |  5494 | `		}` |
|     1408 |  5495 | `	}` |
|        - |  5496 | `	/* Push the hashmap */` |
|    47023 |  5497 | `	pTos++;` |
|    47023 |  5498 | `	pTos->nIdx = SXU32_HIGH;` |
|    47023 |  5499 | `	pTos->x.pOther = pMap;` |
|    47023 |  5500 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    47023 |  5501 | `	break;` |
|        - |  5502 | `					  }` |
|        - |  5503 | `/*` |
|        - |  5504 | ` * LOAD_LIST: P1 * *` |
|        - |  5505 | ` *` |
|        - |  5506 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  5507 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  5508 | ` * Caveats:` |
|        - |  5509 | ` *  This implementation support only a single nesting level.` |
|        - |  5510 | ` */` |
|       48 |  5511 | `case PH7_OP_LOAD_LIST: {` |
|        - |  5512 | `	ph7_value *pEntry;` |
|       98 |  5513 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  5514 | `		/* Empty list,break immediately */` |
|      ! 0 |  5515 | `		break;` |
|        - |  5516 | `	}` |
|       98 |  5517 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  5518 | `#ifdef UNTRUST` |
|        - |  5519 | `	if( &pEntry[-1] < pStack ){` |
|        - |  5520 | `		goto Abort;` |
|        - |  5521 | `	}` |
|        - |  5522 | `#endif` |
|       98 |  5523 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  5524 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  5525 | `		ph7_hashmap_node *pNode;` |
|        - |  5526 | `		ph7_value sKey,*pObj;` |
|        - |  5527 | `		/* Start Copying */` |
|       91 |  5528 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  5529 | `		while( pEntry <= pTos ){` |
|      193 |  5530 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  5531 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  5532 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  5533 | `					if( rc == SXRET_OK ){` |
|        - |  5534 | `						/* Store node value */` |
|      165 |  5535 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  5536 | `					}else{` |
|        - |  5537 | `						/* Undefined array key */` |
|        - |  5538 | `						char zMsg[128];` |
|      ! 0 |  5539 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  5540 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5541 | `						PH7_MemObjRelease(pObj);` |
|        - |  5542 | `					}` |
|       82 |  5543 | `				}` |
|       82 |  5544 | `			}` |
|      193 |  5545 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  5546 | `			pEntry++;` |
|        1 |  5547 | `		}` |
|       46 |  5548 | `	}else{` |
|        - |  5549 | `		/* Source is not an array */` |
|        - |  5550 | `		ph7_value *pObj;` |
|       18 |  5551 | `		while( pEntry <= pTos ){` |
|       12 |  5552 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  5553 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  5554 | `					PH7_MemObjRelease(pObj);` |
|        5 |  5555 | `				}` |
|        5 |  5556 | `			}` |
|       12 |  5557 | `			pEntry++;` |
|        2 |  5558 | `		}` |
|        8 |  5559 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  5560 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  5561 | `			const char *zType = "unknown";` |
|        3 |  5562 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  5563 | `			char zMsg[256];` |
|        3 |  5564 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  5565 | `				zType = "string";` |
|        1 |  5566 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  5567 | `				zType = "int";` |
|      ! 0 |  5568 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5569 | `				zType = "float";` |
|      ! 0 |  5570 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  5571 | `				zType = "object";` |
|      ! 0 |  5572 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  5573 | `				zType = "resource";` |
|      ! 0 |  5574 | `			}` |
|        3 |  5575 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  5576 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  5577 | `		}` |
|        - |  5578 | `	}` |
|       98 |  5579 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  5580 | `	break;` |
|        - |  5581 | `					   }` |
|        - |  5582 | `/*` |
|        - |  5583 | ` * LOAD_IDX: P1 P2 *` |
|        - |  5584 | ` *` |
|        - |  5585 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  5586 | ` * from the stack.` |
|        - |  5587 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  5588 | ` * instead.` |
|        - |  5589 | ` */` |
|   261201 |  5590 | `case PH7_OP_LOAD_IDX: {` |
|   522492 |  5591 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   522492 |  5592 | `	ph7_hashmap *pMap = 0;` |
|        - |  5593 | `	ph7_value *pIdx;` |
|   522492 |  5594 | `	pIdx = 0;` |
|   522492 |  5595 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  5596 | `		if( !pInstr->iP2){` |
|        - |  5597 | `			/* No available index,load NULL */` |
|      ! 0 |  5598 | `			if( pTos >= pStack ){` |
|      ! 0 |  5599 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5600 | `			}else{` |
|        - |  5601 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  5602 | `				pTos++;` |
|      ! 0 |  5603 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5604 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5605 | `			}` |
|        - |  5606 | `			/* Emit a notice */` |
|      ! 0 |  5607 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  5608 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  5609 | `			break;` |
|        - |  5610 | `		}` |
|      ! 0 |  5611 | `	}else{` |
|   522492 |  5612 | `		pIdx = pTos;` |
|   522492 |  5613 | `		pTos--;` |
|        - |  5614 | `	}` |
|   522492 |  5615 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  5616 | `		/* String access */` |
|   405154 |  5617 | `		if( pIdx ){` |
|        - |  5618 | `			sxu32 nOfft;` |
|   405154 |  5619 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  5620 | `				/* Force an int cast */` |
|      ! 0 |  5621 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5622 | `			}` |
|   405154 |  5623 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   405154 |  5624 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  5625 | `				/* Invalid offset,load null */` |
|      ! 0 |  5626 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5627 | `			}else{` |
|   405154 |  5628 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   405154 |  5629 | `				int c = zData[nOfft];` |
|   405154 |  5630 | `				PH7_MemObjRelease(pTos);` |
|   405154 |  5631 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   405154 |  5632 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  5633 | `			}` |
|   202622 |  5634 | `		}else{` |
|        - |  5635 | `			/* No available index,load NULL */` |
|      ! 0 |  5636 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5637 | `		}` |
|   405154 |  5638 | `		break;` |
|        - |  5639 | `	}` |
|   117343 |  5640 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5641 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  5642 | `		 * iP2 codes:` |
|        - |  5643 | `		 *   0 = read       → offsetGet` |
|        - |  5644 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  5645 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  5646 | `		 *   4 = isset()    → offsetExists` |
|        - |  5647 | `		 *   5 = unset()    → offsetUnset` |
|        - |  5648 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      129 |  5649 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      129 |  5650 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      129 |  5651 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5652 | `			ph7_class_method *pMeth;` |
|        - |  5653 | `			ph7_value sResult;` |
|        - |  5654 | `			ph7_value *apArg[1];` |
|      127 |  5655 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  5656 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  5657 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  5658 | `					"Cannot use [] for reading");` |
|      ! 0 |  5659 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5660 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5661 | `				break;` |
|        - |  5662 | `			}` |
|      127 |  5663 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      127 |  5664 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  5665 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       54 |  5666 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5667 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       54 |  5668 | `				apArg[0] = pIdx;` |
|       54 |  5669 | `				if( pMeth ){` |
|       54 |  5670 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       29 |  5671 | `				}` |
|      102 |  5672 | `			}else if( pInstr->iP2 == 5 ){` |
|       11 |  5673 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5674 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|       11 |  5675 | `				apArg[0] = pIdx;` |
|       11 |  5676 | `				if( pMeth ){` |
|       11 |  5677 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  5678 | `				}` |
|        7 |  5679 | `			}else{` |
|       69 |  5680 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5681 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       69 |  5682 | `				apArg[0] = pIdx;` |
|       69 |  5683 | `				if( pMeth ){` |
|       69 |  5684 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  5685 | `				}` |
|        - |  5686 | `			}` |
|      127 |  5687 | `			if( pInstr->iP2 == 4 ){` |
|        - |  5688 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  5689 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  5690 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       36 |  5691 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       36 |  5692 | `				PH7_MemObjRelease(pTos);` |
|       36 |  5693 | `				pTos->nIdx = SXU32_HIGH;` |
|       36 |  5694 | `				if( bExists ){` |
|       19 |  5695 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       19 |  5696 | `					pTos->x.iVal = 1;` |
|       11 |  5697 | `				}else{` |
|       20 |  5698 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        4 |  5699 | `				}` |
|      111 |  5700 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  5701 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  5702 | `				 * vm_builtin_unset is a harmless no-op. */` |
|       11 |  5703 | `				PH7_MemObjRelease(pTos);` |
|       11 |  5704 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  5705 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       91 |  5706 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  5707 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  5708 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  5709 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  5710 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  5711 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  5712 | `				PH7_MemObjRelease(pTos);` |
|       11 |  5713 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  5714 | `				if( !bExists ){` |
|        3 |  5715 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  5716 | `				}else{` |
|        9 |  5717 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5718 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  5719 | `					ph7_value sValue;` |
|        9 |  5720 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  5721 | `					apArg[0] = pIdx;` |
|        9 |  5722 | `					if( pGet ){` |
|        9 |  5723 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  5724 | `					}` |
|        9 |  5725 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  5726 | `					PH7_MemObjRelease(&sValue);` |
|        - |  5727 | `				}` |
|       11 |  5728 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  5729 | `				break; /* skip the duplicate sResult release below */` |
|       77 |  5730 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  5731 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  5732 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  5733 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  5734 | `				 *     and push NULL.` |
|        - |  5735 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|       10 |  5736 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       10 |  5737 | `				int bShouldArm = !bExists;` |
|        - |  5738 | `				ph7_value sValue;` |
|       10 |  5739 | `				PH7_MemObjRelease(&sResult);` |
|        - |  5740 | `				/* Reset any prior arming defensively */` |
|       10 |  5741 | `				VmCoalesceDisarm(pVm);` |
|       10 |  5742 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|       10 |  5743 | `				if( bExists ){` |
|        5 |  5744 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5745 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  5746 | `					apArg[0] = pIdx;` |
|        5 |  5747 | `					if( pGet ){` |
|        5 |  5748 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  5749 | `					}` |
|        5 |  5750 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  5751 | `						bShouldArm = 1;` |
|        1 |  5752 | `					}` |
|        2 |  5753 | `				}` |
|       10 |  5754 | `				PH7_MemObjRelease(pTos);` |
|       10 |  5755 | `				pTos->nIdx = SXU32_HIGH;` |
|       10 |  5756 | `				if( bShouldArm ){` |
|        - |  5757 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  5758 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  5759 | `					 * intervening expression evaluation. */` |
|        8 |  5760 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        8 |  5761 | `					if( pIdx ){` |
|        8 |  5762 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  5763 | `					}` |
|        8 |  5764 | `					pVm->pCoalesceObj = pInst;` |
|        8 |  5765 | `					pInst->iRef++;` |
|        8 |  5766 | `					pVm->bCoalesceArmed = 1;` |
|        5 |  5767 | `				}else{` |
|        3 |  5768 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  5769 | `				}` |
|       10 |  5770 | `				PH7_MemObjRelease(&sValue);` |
|       10 |  5771 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       10 |  5772 | `				break;` |
|      ! 0 |  5773 | `			}else{` |
|        - |  5774 | `				/* offsetGet: replace pTos with the returned value. */` |
|       69 |  5775 | `				PH7_MemObjRelease(pTos);` |
|       69 |  5776 | `				PH7_MemObjStore(&sResult,pTos);` |
|       69 |  5777 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5778 | `			}` |
|      109 |  5779 | `			PH7_MemObjRelease(&sResult);` |
|      109 |  5780 | `			if( pIdx ){` |
|      109 |  5781 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  5782 | `			}` |
|      109 |  5783 | `			break;` |
|        - |  5784 | `		}` |
|        - |  5785 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  5786 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  5787 | `		if( pInst ){` |
|        - |  5788 | `			char zMsg[256];` |
|        3 |  5789 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  5790 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5791 | `				"Cannot use object of type %.*s as array",` |
|        2 |  5792 | `				(int)pName->nByte,pName->zString);` |
|        3 |  5793 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5794 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  5795 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5796 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5797 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5798 | `			break;` |
|        - |  5799 | `		}` |
|      ! 0 |  5800 | `	}` |
|   117219 |  5801 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  5802 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5803 | `			ph7_value *pObj;` |
|        3 |  5804 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5805 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  5806 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  5807 | `			}` |
|        1 |  5808 | `		}` |
|        1 |  5809 | `	}` |
|   117219 |  5810 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   117219 |  5811 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   117219 |  5812 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  5813 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  5814 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  5815 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5816 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5817 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5818 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      898 |  5819 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      447 |  5820 | `		}` |
|        - |  5821 | `		/* Point to the hashmap */` |
|   117219 |  5822 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   117219 |  5823 | `		if( pIdx ){` |
|        - |  5824 | `			/* Load the desired entry */` |
|   117219 |  5825 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    58607 |  5826 | `		}` |
|   117219 |  5827 | `		if( pInstr->iP2 == 3 ){` |
|        - |  5828 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  5829 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  5830 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  5831 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  5832 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5833 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5834 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5835 | `			 * correct for the outermost write. */` |
|       19 |  5836 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5837 | `			if( !needWrite && pNode ){` |
|       13 |  5838 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5839 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5840 | `					needWrite = 1;` |
|        3 |  5841 | `				}` |
|        6 |  5842 | `			}` |
|       19 |  5843 | `			if( needWrite ){` |
|       13 |  5844 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5845 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5846 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5847 | `					 * into the new map's storage. */` |
|        7 |  5848 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5849 | `					if( pIdx ){` |
|        7 |  5850 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5851 | `					}` |
|        3 |  5852 | `				}` |
|        6 |  5853 | `			}` |
|        9 |  5854 | `		}` |
|   117219 |  5855 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5856 | `			/* Create a new empty entry */` |
|      273 |  5857 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5858 | `			if( rc == SXRET_OK ){` |
|        - |  5859 | `				/* Point to the last inserted entry */` |
|      273 |  5860 | `				pNode = pMap->pLast;` |
|      136 |  5861 | `			}` |
|      136 |  5862 | `		}` |
|    58607 |  5863 | `	}` |
|   117219 |  5864 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5865 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5866 | `		char zMsg[128];` |
|      ! 0 |  5867 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5868 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5869 | `		}` |
|      ! 0 |  5870 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5871 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5872 | `	}` |
|   117219 |  5873 | `	if( pIdx ){` |
|   117219 |  5874 | `		PH7_MemObjRelease(pIdx);` |
|    58607 |  5875 | `	}` |
|   117219 |  5876 | `	if( rc == SXRET_OK ){` |
|        - |  5877 | `		/* Load entry contents */` |
|    51781 |  5878 | `		if( pMap->iRef < 2 ){` |
|        - |  5879 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5880 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5881 | `			 */` |
|       28 |  5882 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5883 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5884 | `		}else{` |
|    51755 |  5885 | `			pTos->nIdx = pNode->nValIdx;` |
|    51755 |  5886 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    51755 |  5887 | `			PH7_HashmapUnref(pMap);` |
|        - |  5888 | `		}` |
|    25893 |  5889 | `	}else{` |
|        - |  5890 | `		/* No such entry,load NULL */` |
|    65443 |  5891 | `		PH7_MemObjRelease(pTos);` |
|    65443 |  5892 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5893 | `	}` |
|   117219 |  5894 | `	break;` |
|        - |  5895 | `					  }` |
|        - |  5896 | `/*` |
|        - |  5897 | ` * LOAD_CLOSURE * * P3` |
|        - |  5898 | ` *` |
|        - |  5899 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5900 | ` * name in the stack.` |
|        - |  5901 | ` */` |
|       64 |  5902 | `case PH7_OP_LOAD_CLOSURE:{` |
|      130 |  5903 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      130 |  5904 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5905 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5906 | `		ph7_vm_func *pClosure;` |
|        - |  5907 | `		char *zName;` |
|        - |  5908 | `		sxu32 mLen;` |
|        - |  5909 | `		sxu32 n;` |
|        - |  5910 | `		/* Create a new VM function */` |
|      130 |  5911 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5912 | `		/* Generate an unique closure name */` |
|      130 |  5913 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|      130 |  5914 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5915 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5916 | `			goto Abort;` |
|        - |  5917 | `		}` |
|      130 |  5918 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      130 |  5919 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5920 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5921 | `		}` |
|        - |  5922 | `		/* Zero the stucture */` |
|      130 |  5923 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5924 | `		/* Perform a structure assignment on read-only items */` |
|      130 |  5925 | `		pClosure->aArgs = pFunc->aArgs;` |
|      130 |  5926 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|      130 |  5927 | `		pClosure->aStatic = pFunc->aStatic;` |
|      130 |  5928 | `		pClosure->iFlags = pFunc->iFlags;` |
|      130 |  5929 | `		pClosure->pUserData = pFunc->pUserData;` |
|      130 |  5930 | `		pClosure->sSignature = pFunc->sSignature;` |
|      130 |  5931 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|      130 |  5932 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|      130 |  5933 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|      130 |  5934 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|      130 |  5935 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5936 | `		/* Register the closure */` |
|      130 |  5937 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5938 | `		/* Set up closure environment */` |
|      130 |  5939 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|      130 |  5940 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      324 |  5941 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5942 | `			ph7_value *pValue;` |
|      196 |  5943 | `			pEnv = &aEnv[n];` |
|      196 |  5944 | `			sEnv.sName  = pEnv->sName;` |
|      196 |  5945 | `			sEnv.iFlags = pEnv->iFlags;` |
|      196 |  5946 | `			sEnv.nIdx = SXU32_HIGH;` |
|      196 |  5947 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      196 |  5948 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5949 | `				/* Pass by reference */` |
|      ! 0 |  5950 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5951 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5952 | `					);` |
|      ! 0 |  5953 | `			}` |
|        - |  5954 | `			/* Standard pass by value */` |
|      196 |  5955 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      196 |  5956 | `			if( pValue ){` |
|        - |  5957 | `				/* Copy imported value */` |
|       72 |  5958 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5959 | `			}` |
|        - |  5960 | `			/* Insert the imported variable */` |
|      196 |  5961 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       99 |  5962 | `		}` |
|        - |  5963 | `		/* Finally,load the closure name on the stack */` |
|      130 |  5964 | `		pTos++;` |
|      130 |  5965 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       64 |  5966 | `	}` |
|      130 |  5967 | `	break;` |
|        - |  5968 | `						 }` |
|        - |  5969 | `/*` |
|        - |  5970 | ` * STORE * P2 P3` |
|        - |  5971 | ` *` |
|        - |  5972 | ` * Perform a store (Assignment) operation.` |
|        - |  5973 | ` */` |
|   151845 |  5974 | `case PH7_OP_STORE: {` |
|        - |  5975 | `	ph7_value *pObj;` |
|        - |  5976 | `	SyString sName;` |
|        - |  5977 | `#ifdef UNTRUST` |
|        - |  5978 | `	if( pTos < pStack ){` |
|        - |  5979 | `		goto Abort;` |
|        - |  5980 | `	}` |
|        - |  5981 | `#endif` |
|   303695 |  5982 | `	if( pInstr->iP2 ){` |
|        - |  5983 | `		sxu32 nIdx;` |
|        - |  5984 | `		sxi32 rcT;` |
|        - |  5985 | `		/* Member store operation */` |
|     5871 |  5986 | `		nIdx = pTos->nIdx;` |
|     5871 |  5987 | `		VmPopOperand(&pTos,1);` |
|     5871 |  5988 | `		if( nIdx == SXU32_HIGH ){` |
|        6 |  5989 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5990 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        6 |  5991 | `			pTos->nIdx = SXU32_HIGH;` |
|        4 |  5992 | `		}else{` |
|        - |  5993 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5994 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5867 |  5995 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5867 |  5996 | `			if( rcT == PH7_ABORT ){` |
|       13 |  5997 | `				goto Abort;` |
|        - |  5998 | `			}` |
|     5857 |  5999 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  6000 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  6001 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  6002 | `				 * propagate out of the VM loop. */` |
|       45 |  6003 | `				VmPopOperand(&pTos,1);` |
|        - |  6004 | `				{` |
|       45 |  6005 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       45 |  6006 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       45 |  6007 | `						pc = pFrm2->iExceptionJump - 1;` |
|   151865 |  6008 | `						break;` |
|        - |  6009 | `					}` |
|        - |  6010 | `				}` |
|      ! 0 |  6011 | `				goto Exception;` |
|        - |  6012 | `			}` |
|        - |  6013 | `			/* Point to the desired memory object */` |
|     5817 |  6014 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5817 |  6015 | `			if( pObj ){` |
|        - |  6016 | `				/* Perform the store operation */` |
|     5817 |  6017 | `				PH7_MemObjStore(pTos,pObj);` |
|     2906 |  6018 | `			}` |
|        - |  6019 | `		}` |
|     5821 |  6020 | `		break;` |
|   297829 |  6021 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  6022 | `		/* Take the variable name from the next on the stack */` |
|        7 |  6023 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6024 | `			/* Force a string cast */` |
|      ! 0 |  6025 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6026 | `		}` |
|        7 |  6027 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  6028 | `		pTos--;` |
|        - |  6029 | `#ifdef UNTRUST` |
|        - |  6030 | `		if( pTos < pStack  ){` |
|        - |  6031 | `			goto Abort;` |
|        - |  6032 | `		}` |
|        - |  6033 | `#endif` |
|        4 |  6034 | `	}else{` |
|   297823 |  6035 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6036 | `	}` |
|        - |  6037 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   297829 |  6038 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   297829 |  6039 | `	if( pObj == 0 ){` |
|      ! 0 |  6040 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6041 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  6042 | `		goto Abort;` |
|        - |  6043 | `	}` |
|   297829 |  6044 | `	if( !pInstr->p3 ){` |
|        7 |  6045 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  6046 | `	}` |
|        - |  6047 | `	/* Perform the store operation */` |
|   297829 |  6048 | `	PH7_MemObjStore(pTos,pObj);` |
|   297829 |  6049 | `	break;` |
|        - |  6050 | `				   }` |
|        - |  6051 | `/*` |
|        - |  6052 | ` * STORE_IDX:   P1 * P3` |
|        - |  6053 | ` * STORE_IDX_R: P1 * P3` |
|        - |  6054 | ` *` |
|        - |  6055 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  6056 | ` */` |
|    99507 |  6057 | `case PH7_OP_STORE_IDX:` |
|        - |  6058 | `case PH7_OP_STORE_IDX_REF: {` |
|   199019 |  6059 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  6060 | `	ph7_value *pKey;` |
|        - |  6061 | `	sxu32 nIdx;` |
|   199019 |  6062 | `	if( pInstr->iP1 ){` |
|        - |  6063 | `		/* Key is next on stack */` |
|    64277 |  6064 | `		pKey = pTos;` |
|    64277 |  6065 | `		pTos--;` |
|    32141 |  6066 | `	}else{` |
|   134747 |  6067 | `		pKey = 0;` |
|        - |  6068 | `	}` |
|   199019 |  6069 | `	nIdx = pTos->nIdx;` |
|        - |  6070 | `	{` |
|        - |  6071 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  6072 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  6073 | `		 * the backing variable slot at nIdx. */` |
|   199019 |  6074 | `		ph7_class_instance *pInst = 0;` |
|   199019 |  6075 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       35 |  6076 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   199003 |  6077 | `		}else if( nIdx != SXU32_HIGH ){` |
|   198987 |  6078 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   198987 |  6079 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  6080 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  6081 | `			}` |
|    99491 |  6082 | `		}` |
|   199019 |  6083 | `		if( pInst ){` |
|       35 |  6084 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       35 |  6085 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  6086 | `				ph7_class_method *pMeth;` |
|        - |  6087 | `				ph7_value sNullKey;` |
|        - |  6088 | `				ph7_value *apArg[2];` |
|       33 |  6089 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  6090 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6091 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  6092 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  6093 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  6094 | `					break;` |
|        - |  6095 | `				}` |
|       33 |  6096 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  6097 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  6098 | `				/* Pop container; pTos now points to the value */` |
|       33 |  6099 | `				VmPopOperand(&pTos,1);` |
|       33 |  6100 | `				if( pKey == 0 ){` |
|        7 |  6101 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  6102 | `					apArg[0] = &sNullKey;` |
|        4 |  6103 | `				}else{` |
|       27 |  6104 | `					apArg[0] = pKey;` |
|        - |  6105 | `				}` |
|       33 |  6106 | `				apArg[1] = pTos;` |
|       33 |  6107 | `				if( pMeth ){` |
|       33 |  6108 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  6109 | `				}` |
|       33 |  6110 | `				if( pKey ){` |
|       27 |  6111 | `					PH7_MemObjRelease(pKey);` |
|       15 |  6112 | `				}else{` |
|        7 |  6113 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  6114 | `				}` |
|        - |  6115 | `				/* Pop the value */` |
|       33 |  6116 | `				VmPopOperand(&pTos,1);` |
|       33 |  6117 | `				break;` |
|        - |  6118 | `			}` |
|        - |  6119 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  6120 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  6121 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  6122 | `			 * a few lines below). Match PHP. */` |
|        - |  6123 | `			{` |
|        - |  6124 | `				char zMsg[256];` |
|        3 |  6125 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  6126 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  6127 | `					"Cannot use object of type %.*s as array",` |
|        2 |  6128 | `					(int)pName->nByte,pName->zString);` |
|        3 |  6129 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  6130 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  6131 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  6132 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  6133 | `				break;` |
|        - |  6134 | `			}` |
|        - |  6135 | `		}` |
|        - |  6136 | `	}` |
|   198987 |  6137 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6138 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  6139 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  6140 | `		 * checking true sharing count, then re-add after separation. */` |
|   198935 |  6141 | `		if( nIdx != SXU32_HIGH ){` |
|   198935 |  6142 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   298400 |  6143 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   198935 |  6144 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6145 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  6146 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  6147 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  6148 | `				 * refcounts if the backing array was already separated. */` |
|   198935 |  6149 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   198935 |  6150 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   198935 |  6151 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   198935 |  6152 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   198935 |  6153 | `					pTos->x.pOther = pMap;` |
|    99470 |  6154 | `				}else{` |
|        - |  6155 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  6156 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  6157 | `					pMap = pCur;` |
|        - |  6158 | `				}` |
|    99470 |  6159 | `			}else{` |
|      ! 0 |  6160 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6161 | `			}` |
|    99470 |  6162 | `		}else{` |
|      ! 0 |  6163 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6164 | `		}` |
|   198935 |  6165 | `		if( pMap->iRef < 2 ){` |
|        - |  6166 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  6167 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  6168 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  6169 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  6170 | `			pMap->iRef = 2;` |
|      ! 0 |  6171 | `		}` |
|    99470 |  6172 | `	}else{` |
|        - |  6173 | `		ph7_value *pObj;` |
|       53 |  6174 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  6175 | `		if( pObj == 0 ){` |
|      ! 0 |  6176 | `			if( pKey ){` |
|      ! 0 |  6177 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  6178 | `			}` |
|      ! 0 |  6179 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6180 | `			break;` |
|        - |  6181 | `		}` |
|        - |  6182 | `		/* Phase#1: Load the array */` |
|       53 |  6183 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  6184 | `			VmPopOperand(&pTos,1);` |
|       53 |  6185 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  6186 | `				/* Force a string cast */` |
|      ! 0 |  6187 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  6188 | `			}` |
|       53 |  6189 | `			if( pKey == 0 ){` |
|        - |  6190 | `				/* Append string */` |
|        3 |  6191 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  6192 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  6193 | `				}` |
|        2 |  6194 | `			}else{` |
|        - |  6195 | `				sxu32 nOfft;` |
|       51 |  6196 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  6197 | `					/* Force an int cast */` |
|       51 |  6198 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  6199 | `				}` |
|       51 |  6200 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  6201 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  6202 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  6203 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  6204 | `					zData[nOfft] = zBlob[0];` |
|       26 |  6205 | `				}else{` |
|      ! 0 |  6206 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  6207 | `						/* Perform an append operation */` |
|      ! 0 |  6208 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  6209 | `					}` |
|        - |  6210 | `				}` |
|        - |  6211 | `			}` |
|       53 |  6212 | `			if( pKey ){` |
|       51 |  6213 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  6214 | `			}` |
|       53 |  6215 | `			break;` |
|      ! 0 |  6216 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  6217 | `			/* Force a hashmap cast  */` |
|      ! 0 |  6218 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  6219 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6220 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  6221 | `				goto Abort;` |
|        - |  6222 | `			}` |
|      ! 0 |  6223 | `		}` |
|        - |  6224 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  6225 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  6226 | `	}` |
|   198935 |  6227 | `	VmPopOperand(&pTos,1);` |
|        - |  6228 | `	/* Phase#2: Perform the insertion */` |
|   198935 |  6229 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  6230 | `		/* Insertion by reference */` |
|       15 |  6231 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  6232 | `	}else{` |
|   198921 |  6233 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  6234 | `	}` |
|   198935 |  6235 | `	if( pKey ){` |
|    64201 |  6236 | `		PH7_MemObjRelease(pKey);` |
|    32098 |  6237 | `	}` |
|   198935 |  6238 | `	break;` |
|        - |  6239 | `					   }` |
|        - |  6240 | `/*` |
|        - |  6241 | ` * INCR: P1 * *` |
|        - |  6242 | ` *` |
|        - |  6243 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  6244 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  6245 | ` * the stack and increment after that.` |
|        - |  6246 | ` */` |
|   173591 |  6247 | `case PH7_OP_INCR:` |
|        - |  6248 | `#ifdef UNTRUST` |
|        - |  6249 | `	if( pTos < pStack ){` |
|        - |  6250 | `		goto Abort;` |
|        - |  6251 | `	}` |
|        - |  6252 | `#endif` |
|        - |  6253 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|        - |  6254 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - |  6255 | `	 * — which otherwise skips object/array/resource operands. */` |
|   347272 |  6256 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|   347268 |  6257 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   347268 |  6258 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  6259 | `			ph7_value *pObj;` |
|   347268 |  6260 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   347268 |  6261 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  6262 | `					/* Perl-style string increment.` |
|        - |  6263 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  6264 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  6265 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  6266 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  6267 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  6268 | `					}` |
|       49 |  6269 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  6270 | `					if( pInstr->iP1 ){` |
|        - |  6271 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  6272 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  6273 | `					}` |
|       25 |  6274 | `				}else{` |
|        - |  6275 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  6276 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  6277 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  6278 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  6279 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  6280 | `					 * so its old-value view survives the coercion. */` |
|   347220 |  6281 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  6282 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  6283 | `					}` |
|        - |  6284 | `					/* Force a numeric cast on the variable */` |
|   347220 |  6285 | `					PH7_MemObjToNumeric(pObj);` |
|   347220 |  6286 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  6287 | `						pObj->rVal++;` |
|        - |  6288 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  6289 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  6290 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  6291 | `						 * integer-valued real. */` |
|        9 |  6292 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  6293 | `					}else{` |
|   347212 |  6294 | `						pObj->x.iVal++;` |
|        - |  6295 | `					}` |
|   347220 |  6296 | `					if( pInstr->iP1 ){` |
|        - |  6297 | `						/* Pre-increment: result is the new value. */` |
|       83 |  6298 | `						PH7_MemObjStore(pObj,pTos);` |
|       41 |  6299 | `					}` |
|        - |  6300 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  6301 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  6302 | `				}` |
|   173674 |  6303 | `			}` |
|   173679 |  6304 | `		}else{` |
|      ! 0 |  6305 | `			if( pInstr->iP1 ){` |
|      ! 0 |  6306 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  6307 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  6308 | `				}else{` |
|        - |  6309 | `					/* Force a numeric cast */` |
|      ! 0 |  6310 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  6311 | `					/* Pre-increment */` |
|      ! 0 |  6312 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6313 | `						pTos->rVal++;` |
|        - |  6314 | `						/* Try to get an integer representation */` |
|      ! 0 |  6315 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  6316 | `					}else{` |
|      ! 0 |  6317 | `						pTos->x.iVal++;` |
|      ! 0 |  6318 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  6319 | `					}` |
|        - |  6320 | `				}` |
|      ! 0 |  6321 | `			}` |
|        - |  6322 | `		}` |
|   173674 |  6323 | `	}` |
|   347268 |  6324 | `	break;` |
|        - |  6325 | `/*` |
|        - |  6326 | ` * DECR: P1 * *` |
|        - |  6327 | ` *` |
|        - |  6328 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  6329 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  6330 | ` * and decrement after that.` |
|        - |  6331 | ` */` |
|       17 |  6332 | `case PH7_OP_DECR:` |
|        - |  6333 | `#ifdef UNTRUST` |
|        - |  6334 | `	if( pTos < pStack ){` |
|        - |  6335 | `		goto Abort;` |
|        - |  6336 | `	}` |
|        - |  6337 | `#endif` |
|        - |  6338 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|        - |  6339 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - |  6340 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|        - |  6341 | `	 * property currently holding null). */` |
|       37 |  6342 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - |  6343 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  6344 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  6345 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  6346 | `			ph7_value *pObj;` |
|       27 |  6347 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  6348 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  6349 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  6350 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  6351 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  6352 | `					if( pInstr->iP1 ){` |
|        - |  6353 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  6354 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  6355 | `					}` |
|        - |  6356 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  6357 | `				}else{` |
|        - |  6358 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  6359 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  6360 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  6361 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  6362 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  6363 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  6364 | `					}` |
|       21 |  6365 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  6366 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  6367 | `						pObj->rVal--;` |
|        - |  6368 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  6369 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  6370 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  6371 | `						 * integer-valued real. */` |
|        9 |  6372 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  6373 | `					}else{` |
|       13 |  6374 | `						pObj->x.iVal--;` |
|        - |  6375 | `					}` |
|       21 |  6376 | `					if( pInstr->iP1 ){` |
|        - |  6377 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  6378 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  6379 | `					}` |
|        - |  6380 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  6381 | `				}` |
|       13 |  6382 | `			}` |
|       14 |  6383 | `		}else{` |
|      ! 0 |  6384 | `			if( pInstr->iP1 ){` |
|      ! 0 |  6385 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  6386 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  6387 | `				}else{` |
|        - |  6388 | `					/* Force a numeric cast */` |
|      ! 0 |  6389 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  6390 | `					/* Pre-decrement */` |
|      ! 0 |  6391 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6392 | `						pTos->rVal--;` |
|        - |  6393 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  6394 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  6395 | `					}else{` |
|      ! 0 |  6396 | `						pTos->x.iVal--;` |
|      ! 0 |  6397 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  6398 | `					}` |
|        - |  6399 | `				}` |
|      ! 0 |  6400 | `			}` |
|        - |  6401 | `		}` |
|       13 |  6402 | `	}` |
|       29 |  6403 | `	break;` |
|        - |  6404 | `/*` |
|        - |  6405 | ` * UMINUS: * * *` |
|        - |  6406 | ` *` |
|        - |  6407 | ` * Perform a unary minus operation.` |
|        - |  6408 | ` */` |
|    30670 |  6409 | `case PH7_OP_UMINUS:` |
|        - |  6410 | `#ifdef UNTRUST` |
|        - |  6411 | `	if( pTos < pStack ){` |
|        - |  6412 | `		goto Abort;` |
|        - |  6413 | `	}` |
|        - |  6414 | `#endif` |
|        - |  6415 | `	/* Force a numeric (integer,real or both) cast */` |
|    61345 |  6416 | `	PH7_MemObjToNumeric(pTos);` |
|    61345 |  6417 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  6418 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  6419 | `	}` |
|    61345 |  6420 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    61315 |  6421 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    30655 |  6422 | `	}` |
|    61345 |  6423 | `	break;` |
|        - |  6424 | `/*` |
|        - |  6425 | ` * UPLUS: * * *` |
|        - |  6426 | ` *` |
|        - |  6427 | ` * Perform a unary plus operation.` |
|        - |  6428 | ` */` |
|       18 |  6429 | `case PH7_OP_UPLUS:` |
|        - |  6430 | `#ifdef UNTRUST` |
|        - |  6431 | `	if( pTos < pStack ){` |
|        - |  6432 | `		goto Abort;` |
|        - |  6433 | `	}` |
|        - |  6434 | `#endif` |
|        - |  6435 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  6436 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  6437 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6438 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  6439 | `	}` |
|       37 |  6440 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  6441 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  6442 | `	}` |
|       37 |  6443 | `	break;` |
|        - |  6444 | `/*` |
|        - |  6445 | ` * OP_LNOT: * * *` |
|        - |  6446 | ` *` |
|        - |  6447 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  6448 | ` * with its complement.` |
|        - |  6449 | ` */` |
|    45681 |  6450 | `case PH7_OP_LNOT:` |
|        - |  6451 | `#ifdef UNTRUST` |
|        - |  6452 | `	if( pTos < pStack ){` |
|        - |  6453 | `		goto Abort;` |
|        - |  6454 | `	}` |
|        - |  6455 | `#endif` |
|        - |  6456 | `	/* Force a boolean cast */` |
|    91452 |  6457 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       27 |  6458 | `		PH7_MemObjToBool(pTos);` |
|       11 |  6459 | `	}` |
|    91452 |  6460 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    91452 |  6461 | `	break;` |
|        - |  6462 | `/*` |
|        - |  6463 | ` * OP_BITNOT: * * *` |
|        - |  6464 | ` *` |
|        - |  6465 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  6466 | ` * with its ones-complement.` |
|        - |  6467 | ` */` |
|       14 |  6468 | `case PH7_OP_BITNOT:` |
|        - |  6469 | `#ifdef UNTRUST` |
|        - |  6470 | `	if( pTos < pStack ){` |
|        - |  6471 | `		goto Abort;` |
|        - |  6472 | `	}` |
|        - |  6473 | `#endif` |
|        - |  6474 | `	/* Force an integer cast */` |
|       33 |  6475 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6476 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6477 | `	}` |
|       33 |  6478 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       33 |  6479 | `	break;` |
|        - |  6480 | `/* OP_MUL * * *` |
|        - |  6481 | ` * OP_MUL_STORE * * *` |
|        - |  6482 | ` *` |
|        - |  6483 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  6484 | ` * and push the result back onto the stack.` |
|        - |  6485 | ` */` |
|     1297 |  6486 | `case PH7_OP_MUL:` |
|        - |  6487 | `case PH7_OP_MUL_STORE: {` |
|     2598 |  6488 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6489 | `	/* Force the operand to be numeric */` |
|        - |  6490 | `#ifdef UNTRUST` |
|        - |  6491 | `	if( pNos < pStack ){` |
|        - |  6492 | `		goto Abort;` |
|        - |  6493 | `	}` |
|        - |  6494 | `#endif` |
|     2598 |  6495 | `	PH7_MemObjToNumeric(pTos);` |
|     2598 |  6496 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6497 | `	/* Perform the requested operation */` |
|     2598 |  6498 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6499 | `		/* Floating point arithemic */` |
|        - |  6500 | `		ph7_real a,b,r;` |
|       21 |  6501 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  6502 | `			PH7_MemObjToReal(pTos);` |
|        4 |  6503 | `		}` |
|       21 |  6504 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  6505 | `			PH7_MemObjToReal(pNos);` |
|        3 |  6506 | `		}` |
|       21 |  6507 | `		a = pNos->rVal;` |
|       21 |  6508 | `		b = pTos->rVal;` |
|       21 |  6509 | `		r = a * b;` |
|        - |  6510 | `		/* Push the result */` |
|       21 |  6511 | `		pNos->rVal = r;` |
|       21 |  6512 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6513 | `		/* Try to get an integer representation */` |
|       21 |  6514 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  6515 | `	}else{` |
|        - |  6516 | `		/* Integer arithmetic */` |
|        - |  6517 | `		sxi64 a,b,r;` |
|     2578 |  6518 | `		a = pNos->x.iVal;` |
|     2578 |  6519 | `		b = pTos->x.iVal;` |
|     2578 |  6520 | `		r = a * b;` |
|        - |  6521 | `		/* Push the result */` |
|     2578 |  6522 | `		pNos->x.iVal = r;` |
|     2578 |  6523 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6524 | `	}` |
|     2598 |  6525 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  6526 | `		ph7_value *pObj;` |
|       32 |  6527 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6528 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  6529 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  6530 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  6531 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  6532 | `		}` |
|       15 |  6533 | `	}` |
|     2598 |  6534 | `	VmPopOperand(&pTos,1);` |
|     2598 |  6535 | `	break;` |
|        - |  6536 | `				 }` |
|        - |  6537 | `/* OP_POW * * *` |
|        - |  6538 | ` * OP_POW_STORE * * *` |
|        - |  6539 | ` *` |
|        - |  6540 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  6541 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  6542 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  6543 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  6544 | ` */` |
|       67 |  6545 | `case PH7_OP_POW:` |
|        - |  6546 | `case PH7_OP_POW_STORE: {` |
|      135 |  6547 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  6548 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  6549 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  6550 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  6551 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  6552 | `	 */` |
|      135 |  6553 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  6554 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  6555 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  6556 | `	int bBothInt;` |
|      135 |  6557 | `	int usedInt = 0;` |
|        - |  6558 | `	ph7_real a, b, r;` |
|        - |  6559 | `#endif` |
|      135 |  6560 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  6561 | `#ifdef UNTRUST` |
|        - |  6562 | `	if( pNos < pStack ){` |
|        - |  6563 | `		goto Abort;` |
|        - |  6564 | `	}` |
|        - |  6565 | `#endif` |
|      135 |  6566 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  6567 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6568 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  6569 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  6570 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  6571 | `	if( bBothInt ){` |
|      123 |  6572 | `		base_i = pBase->x.iVal;` |
|      123 |  6573 | `		exp_i  = pExp->x.iVal;` |
|       61 |  6574 | `	}` |
|      135 |  6575 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  6576 | `		PH7_MemObjToReal(pBase);` |
|       62 |  6577 | `	}` |
|      135 |  6578 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  6579 | `		PH7_MemObjToReal(pExp);` |
|       66 |  6580 | `	}` |
|      135 |  6581 | `	a = pBase->rVal;` |
|      135 |  6582 | `	b = pExp->rVal;` |
|      135 |  6583 | `	r = pow(a, b);` |
|        - |  6584 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  6585 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  6586 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  6587 | `	 * representable as double but not as signed int64. */` |
|      135 |  6588 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  6589 | `		sxi64 result_i = 1;` |
|      117 |  6590 | `		sxi64 cur_base = base_i;` |
|      117 |  6591 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  6592 | `		int overflow = 0;` |
|      401 |  6593 | `		while( cur_exp > 0 ){` |
|      289 |  6594 | `			if( cur_exp & 1 ){` |
|      189 |  6595 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  6596 | `					overflow = 1;` |
|        3 |  6597 | `					break;` |
|        - |  6598 | `				}` |
|       93 |  6599 | `			}` |
|      287 |  6600 | `			cur_exp >>= 1;` |
|      287 |  6601 | `			if( cur_exp > 0 ){` |
|      181 |  6602 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  6603 | `					overflow = 1;` |
|        3 |  6604 | `					break;` |
|        - |  6605 | `				}` |
|       89 |  6606 | `			}` |
|        1 |  6607 | `		}` |
|      117 |  6608 | `		if( !overflow ){` |
|      113 |  6609 | `			pNos->x.iVal = result_i;` |
|      113 |  6610 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  6611 | `			usedInt = 1;` |
|       56 |  6612 | `		}` |
|       58 |  6613 | `	}` |
|      135 |  6614 | `	if( !usedInt ){` |
|       23 |  6615 | `		pNos->rVal = r;` |
|       23 |  6616 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  6617 | `	}` |
|        - |  6618 | `#else` |
|        - |  6619 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  6620 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  6621 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  6622 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  6623 | `	 * represented. */` |
|        - |  6624 | `	base_i = pBase->x.iVal;` |
|        - |  6625 | `	exp_i  = pExp->x.iVal;` |
|        - |  6626 | `	{` |
|        - |  6627 | `		sxi64 result_i = 1;` |
|        - |  6628 | `		sxi64 cur_base = base_i;` |
|        - |  6629 | `		sxi64 cur_exp  = exp_i;` |
|        - |  6630 | `		if( cur_exp < 0 ){` |
|        - |  6631 | `			result_i = 0;` |
|        - |  6632 | `		}else{` |
|        - |  6633 | `			while( cur_exp > 0 ){` |
|        - |  6634 | `				if( cur_exp & 1 ){` |
|        - |  6635 | `					result_i *= cur_base;` |
|        - |  6636 | `				}` |
|        - |  6637 | `				cur_exp >>= 1;` |
|        - |  6638 | `				if( cur_exp > 0 ){` |
|        - |  6639 | `					cur_base *= cur_base;` |
|        - |  6640 | `				}` |
|        - |  6641 | `			}` |
|        - |  6642 | `		}` |
|        - |  6643 | `		pNos->x.iVal = result_i;` |
|        - |  6644 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  6645 | `	}` |
|        - |  6646 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  6647 | `	if( bStore ){` |
|        - |  6648 | `		ph7_value *pObj;` |
|       23 |  6649 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6650 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  6651 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  6652 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  6653 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  6654 | `		}` |
|       11 |  6655 | `	}` |
|      135 |  6656 | `	VmPopOperand(&pTos,1);` |
|      135 |  6657 | `	break;` |
|        - |  6658 | `				 }` |
|        - |  6659 | `/* OP_ADD * * *` |
|        - |  6660 | ` *` |
|        - |  6661 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6662 | ` * and push the result back onto the stack.` |
|        - |  6663 | ` */` |
|      539 |  6664 | `case PH7_OP_ADD:{` |
|     1083 |  6665 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6666 | `#ifdef UNTRUST` |
|        - |  6667 | `	if( pNos < pStack ){` |
|        - |  6668 | `		goto Abort;` |
|        - |  6669 | `	}` |
|        - |  6670 | `#endif` |
|        - |  6671 | `	/* Perform the addition */` |
|     1083 |  6672 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1083 |  6673 | `	VmPopOperand(&pTos,1);` |
|     1083 |  6674 | `	break;` |
|        - |  6675 | `				}` |
|        - |  6676 | `/*` |
|        - |  6677 | ` * OP_ADD_STORE * * *` |
|        - |  6678 | ` *` |
|        - |  6679 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6680 | ` * and push the result back onto the stack.` |
|        - |  6681 | ` */` |
|      502 |  6682 | `case PH7_OP_ADD_STORE:{` |
|     1009 |  6683 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6684 | `	ph7_value *pObj;` |
|        - |  6685 | `	sxu32 nIdx;` |
|        - |  6686 | `#ifdef UNTRUST` |
|        - |  6687 | `	if( pNos < pStack ){` |
|        - |  6688 | `		goto Abort;` |
|        - |  6689 | `	}` |
|        - |  6690 | `#endif` |
|        - |  6691 | `	/* Perform the addition */` |
|     1009 |  6692 | `	nIdx = pTos->nIdx;` |
|     1009 |  6693 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  6694 | `	/* Peform the store operation */` |
|     1009 |  6695 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6696 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1009 |  6697 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1009 |  6698 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1009 |  6699 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  6700 | `	}` |
|        - |  6701 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1009 |  6702 | `	PH7_MemObjStore(pTos,pNos);` |
|     1009 |  6703 | `	VmPopOperand(&pTos,1);` |
|     1009 |  6704 | `	break;` |
|        - |  6705 | `				}` |
|        - |  6706 | `/* OP_SUB * * *` |
|        - |  6707 | ` *` |
|        - |  6708 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6709 | ` * first (what was next on the stack) from the second (the` |
|        - |  6710 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6711 | ` */` |
|      352 |  6712 | `case PH7_OP_SUB: {` |
|      709 |  6713 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6714 | `#ifdef UNTRUST` |
|        - |  6715 | `	if( pNos < pStack ){` |
|        - |  6716 | `		goto Abort;` |
|        - |  6717 | `	}` |
|        - |  6718 | `#endif` |
|      709 |  6719 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6720 | `		/* Floating point arithemic */` |
|        - |  6721 | `		ph7_real a,b,r;` |
|      103 |  6722 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6723 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6724 | `		}` |
|      103 |  6725 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6726 | `			PH7_MemObjToReal(pNos);` |
|        2 |  6727 | `		}` |
|      103 |  6728 | `		a = pNos->rVal;` |
|      103 |  6729 | `		b = pTos->rVal;` |
|      103 |  6730 | `		r = a - b;` |
|        - |  6731 | `		/* Push the result */` |
|      103 |  6732 | `		pNos->rVal = r;` |
|      103 |  6733 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6734 | `		/* Try to get an integer representation */` |
|      103 |  6735 | `		PH7_MemObjTryInteger(pNos);` |
|       52 |  6736 | `	}else{` |
|        - |  6737 | `		/* Integer arithmetic */` |
|        - |  6738 | `		sxi64 a,b,r;` |
|      607 |  6739 | `		a = pNos->x.iVal;` |
|      607 |  6740 | `		b = pTos->x.iVal;` |
|      607 |  6741 | `		r = a - b;` |
|        - |  6742 | `		/* Push the result */` |
|      607 |  6743 | `		pNos->x.iVal = r;` |
|      607 |  6744 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6745 | `	}` |
|      709 |  6746 | `	VmPopOperand(&pTos,1);` |
|      709 |  6747 | `	break;` |
|        - |  6748 | `				 }` |
|        - |  6749 | `/* OP_SUB_STORE * * *` |
|        - |  6750 | ` *` |
|        - |  6751 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6752 | ` * first (what was next on the stack) from the second (the` |
|        - |  6753 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6754 | ` */` |
|        4 |  6755 | `case PH7_OP_SUB_STORE: {` |
|       10 |  6756 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6757 | `	ph7_value *pObj;` |
|        - |  6758 | `#ifdef UNTRUST` |
|        - |  6759 | `	if( pNos < pStack ){` |
|        - |  6760 | `		goto Abort;` |
|        - |  6761 | `	}` |
|        - |  6762 | `#endif` |
|       10 |  6763 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6764 | `		/* Floating point arithemic */` |
|        - |  6765 | `		ph7_real a,b,r;` |
|      ! 0 |  6766 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6767 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6768 | `		}` |
|      ! 0 |  6769 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6770 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  6771 | `		}` |
|      ! 0 |  6772 | `		a = pTos->rVal;` |
|      ! 0 |  6773 | `		b = pNos->rVal;` |
|      ! 0 |  6774 | `		r = a - b;` |
|        - |  6775 | `		/* Push the result */` |
|      ! 0 |  6776 | `		pNos->rVal = r;` |
|      ! 0 |  6777 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6778 | `		/* Try to get an integer representation */` |
|      ! 0 |  6779 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  6780 | `	}else{` |
|        - |  6781 | `		/* Integer arithmetic */` |
|        - |  6782 | `		sxi64 a,b,r;` |
|       10 |  6783 | `		a = pTos->x.iVal;` |
|       10 |  6784 | `		b = pNos->x.iVal;` |
|       10 |  6785 | `		r = a - b;` |
|        - |  6786 | `		/* Push the result */` |
|       10 |  6787 | `		pNos->x.iVal = r;` |
|       10 |  6788 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6789 | `	}` |
|       10 |  6790 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6791 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  6792 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  6793 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  6794 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  6795 | `	}` |
|       10 |  6796 | `	VmPopOperand(&pTos,1);` |
|       10 |  6797 | `	break;` |
|        - |  6798 | `				 }` |
|        - |  6799 |  |
|        - |  6800 | `/*` |
|        - |  6801 | ` * OP_MOD * * *` |
|        - |  6802 | ` *` |
|        - |  6803 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6804 | ` * first (what was next on the stack) from the second (the` |
|        - |  6805 | ` * top of the stack) and push the remainder after division` |
|        - |  6806 | ` * onto the stack.` |
|        - |  6807 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6808 | ` */` |
|      310 |  6809 | `case PH7_OP_MOD:{` |
|      625 |  6810 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6811 | `	sxi64 a,b,r;` |
|        - |  6812 | `#ifdef UNTRUST` |
|        - |  6813 | `	if( pNos < pStack ){` |
|        - |  6814 | `		goto Abort;` |
|        - |  6815 | `	}` |
|        - |  6816 | `#endif` |
|        - |  6817 | `	/* Force the operands to be integer */` |
|      625 |  6818 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6819 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6820 | `	}` |
|      625 |  6821 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  6822 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  6823 | `	}` |
|        - |  6824 | `	/* Perform the requested operation */` |
|      625 |  6825 | `	a = pNos->x.iVal;` |
|      625 |  6826 | `	b = pTos->x.iVal;` |
|      625 |  6827 | `	if( b == 0 ){` |
|        3 |  6828 | `		r = 0;` |
|        3 |  6829 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6830 | `		/* goto Abort; */` |
|        2 |  6831 | `	}else{` |
|      623 |  6832 | `		r = a%b;` |
|        - |  6833 | `	}` |
|        - |  6834 | `	/* Push the result */` |
|      625 |  6835 | `	pNos->x.iVal = r;` |
|      625 |  6836 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      625 |  6837 | `	VmPopOperand(&pTos,1);` |
|      625 |  6838 | `	break;` |
|        - |  6839 | `				}` |
|        - |  6840 | `/*` |
|        - |  6841 | ` * OP_MOD_STORE * * *` |
|        - |  6842 | ` *` |
|        - |  6843 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6844 | ` * first (what was next on the stack) from the second (the` |
|        - |  6845 | ` * top of the stack) and push the remainder after division` |
|        - |  6846 | ` * onto the stack.` |
|        - |  6847 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6848 | ` */` |
|        1 |  6849 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6850 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6851 | `	ph7_value *pObj;` |
|        - |  6852 | `	sxi64 a,b,r;` |
|        - |  6853 | `#ifdef UNTRUST` |
|        - |  6854 | `	if( pNos < pStack ){` |
|        - |  6855 | `		goto Abort;` |
|        - |  6856 | `	}` |
|        - |  6857 | `#endif` |
|        - |  6858 | `	/* Force the operands to be integer */` |
|        3 |  6859 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6860 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6861 | `	}` |
|        3 |  6862 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6863 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6864 | `	}` |
|        - |  6865 | `	/* Perform the requested operation */` |
|        3 |  6866 | `	a = pTos->x.iVal;` |
|        3 |  6867 | `	b = pNos->x.iVal;` |
|        3 |  6868 | `	if( b == 0 ){` |
|      ! 0 |  6869 | `		r = 0;` |
|      ! 0 |  6870 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6871 | `		/* goto Abort; */` |
|      ! 0 |  6872 | `	}else{` |
|        3 |  6873 | `		r = a%b;` |
|        - |  6874 | `	}` |
|        - |  6875 | `	/* Push the result */` |
|        3 |  6876 | `	pNos->x.iVal = r;` |
|        3 |  6877 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6878 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6879 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6880 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6881 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6882 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6883 | `	}` |
|        3 |  6884 | `	VmPopOperand(&pTos,1);` |
|        3 |  6885 | `	break;` |
|        - |  6886 | `				}` |
|        - |  6887 | `/*` |
|        - |  6888 | ` * OP_DIV * * *` |
|        - |  6889 | ` *` |
|        - |  6890 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6891 | ` * first (what was next on the stack) from the second (the` |
|        - |  6892 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6893 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6894 | ` */` |
|       33 |  6895 | `case PH7_OP_DIV:{` |
|       68 |  6896 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6897 | `	ph7_real a,b,r;` |
|        - |  6898 | `#ifdef UNTRUST` |
|        - |  6899 | `	if( pNos < pStack ){` |
|        - |  6900 | `		goto Abort;` |
|        - |  6901 | `	}` |
|        - |  6902 | `#endif` |
|        - |  6903 | `	/* Force the operands to be real */` |
|       68 |  6904 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6905 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6906 | `	}` |
|       68 |  6907 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6908 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6909 | `	}` |
|        - |  6910 | `	/* Perform the requested operation */` |
|       68 |  6911 | `	a = pNos->rVal;` |
|       68 |  6912 | `	b = pTos->rVal;` |
|       68 |  6913 | `	if( b == 0 ){` |
|        - |  6914 | `		/* Division by zero */` |
|        3 |  6915 | `		pNos->rVal = 0;` |
|        3 |  6916 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6917 | `		/* goto Abort; */` |
|        2 |  6918 | `	}else{` |
|       65 |  6919 | `		r = a/b;` |
|        - |  6920 | `		/* Push the result */` |
|       65 |  6921 | `		pNos->rVal = r;` |
|       65 |  6922 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6923 | `		/* Try to get an integer representation */` |
|       65 |  6924 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6925 | `	}` |
|       68 |  6926 | `	VmPopOperand(&pTos,1);` |
|       68 |  6927 | `	break;` |
|        - |  6928 | `				}` |
|        - |  6929 | `/*` |
|        - |  6930 | ` * OP_DIV_STORE * * *` |
|        - |  6931 | ` *` |
|        - |  6932 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6933 | ` * first (what was next on the stack) from the second (the` |
|        - |  6934 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6935 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6936 | ` */` |
|        2 |  6937 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6938 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6939 | `	ph7_value *pObj;` |
|        - |  6940 | `	ph7_real a,b,r;` |
|        - |  6941 | `#ifdef UNTRUST` |
|        - |  6942 | `	if( pNos < pStack ){` |
|        - |  6943 | `		goto Abort;` |
|        - |  6944 | `	}` |
|        - |  6945 | `#endif` |
|        - |  6946 | `	/* Force the operands to be real */` |
|        5 |  6947 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6948 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6949 | `	}` |
|        5 |  6950 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6951 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6952 | `	}` |
|        - |  6953 | `	/* Perform the requested operation */` |
|        5 |  6954 | `	a = pTos->rVal;` |
|        5 |  6955 | `	b = pNos->rVal;` |
|        5 |  6956 | `	if( b == 0 ){` |
|        - |  6957 | `		/* Division by zero */` |
|      ! 0 |  6958 | `		r = 0;` |
|      ! 0 |  6959 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6960 | `		/* goto Abort; */` |
|      ! 0 |  6961 | `	}else{` |
|        5 |  6962 | `		r = a/b;` |
|        - |  6963 | `		/* Push the result */` |
|        5 |  6964 | `		pNos->rVal = r;` |
|        5 |  6965 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6966 | `		/* Try to get an integer representation */` |
|        5 |  6967 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6968 | `	}` |
|        5 |  6969 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6970 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6971 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6972 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6973 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6974 | `	}` |
|        5 |  6975 | `	VmPopOperand(&pTos,1);` |
|        5 |  6976 | `	break;` |
|        - |  6977 | `				}` |
|        - |  6978 | `/* OP_BAND * * *` |
|        - |  6979 | ` *` |
|        - |  6980 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6981 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6982 | ` * two elements.` |
|        - |  6983 | `*/` |
|        - |  6984 | `/* OP_BOR * * *` |
|        - |  6985 | ` *` |
|        - |  6986 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6987 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6988 | ` * two elements.` |
|        - |  6989 | ` */` |
|        - |  6990 | `/* OP_BXOR * * *` |
|        - |  6991 | ` *` |
|        - |  6992 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6993 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6994 | ` * two elements.` |
|        - |  6995 | ` */` |
|       43 |  6996 | `case PH7_OP_BAND:` |
|        - |  6997 | `case PH7_OP_BOR:` |
|        - |  6998 | `case PH7_OP_BXOR:{` |
|       91 |  6999 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7000 | `	sxi64 a,b,r;` |
|        - |  7001 | `#ifdef UNTRUST` |
|        - |  7002 | `	if( pNos < pStack ){` |
|        - |  7003 | `		goto Abort;` |
|        - |  7004 | `	}` |
|        - |  7005 | `#endif` |
|        - |  7006 | `	/* Force the operands to be integer */` |
|       91 |  7007 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7008 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  7009 | `	}` |
|       91 |  7010 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7011 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  7012 | `	}` |
|        - |  7013 | `	/* Perform the requested operation */` |
|       91 |  7014 | `	a = pNos->x.iVal;` |
|       91 |  7015 | `	b = pTos->x.iVal;` |
|       91 |  7016 | `	switch(pInstr->iOp){` |
|        7 |  7017 | `	case PH7_OP_BOR_STORE:` |
|       15 |  7018 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  7019 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  7020 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       29 |  7021 | `	case PH7_OP_BAND_STORE:` |
|       29 |  7022 | `	case PH7_OP_BAND:` |
|       63 |  7023 | `	default:          r = a&b; break;` |
|        - |  7024 | `	}` |
|        - |  7025 | `	/* Push the result */` |
|       91 |  7026 | `	pNos->x.iVal = r;` |
|       91 |  7027 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       91 |  7028 | `	VmPopOperand(&pTos,1);` |
|       91 |  7029 | `	break;` |
|        - |  7030 | `				 }` |
|        - |  7031 | `/* OP_BAND_STORE * * *` |
|        - |  7032 | ` *` |
|        - |  7033 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  7034 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  7035 | ` * two elements.` |
|        - |  7036 | `*/` |
|        - |  7037 | `/* OP_BOR_STORE * * *` |
|        - |  7038 | ` *` |
|        - |  7039 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  7040 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  7041 | ` * two elements.` |
|        - |  7042 | ` */` |
|        - |  7043 | `/* OP_BXOR_STORE * * *` |
|        - |  7044 | ` *` |
|        - |  7045 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  7046 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  7047 | ` * two elements.` |
|        - |  7048 | ` */` |
|       10 |  7049 | `case PH7_OP_BAND_STORE:` |
|        - |  7050 | `case PH7_OP_BOR_STORE:` |
|        - |  7051 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  7052 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7053 | `	ph7_value *pObj;` |
|        - |  7054 | `	sxi64 a,b,r;` |
|        - |  7055 | `#ifdef UNTRUST` |
|        - |  7056 | `	if( pNos < pStack ){` |
|        - |  7057 | `		goto Abort;` |
|        - |  7058 | `	}` |
|        - |  7059 | `#endif` |
|        - |  7060 | `	/* Force the operands to be integer */` |
|       21 |  7061 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7062 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  7063 | `	}` |
|       21 |  7064 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7065 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  7066 | `	}` |
|        - |  7067 | `	/* Perform the requested operation */` |
|       21 |  7068 | `	a = pTos->x.iVal;` |
|       21 |  7069 | `	b = pNos->x.iVal;` |
|       21 |  7070 | `	switch(pInstr->iOp){` |
|        3 |  7071 | `	case PH7_OP_BOR_STORE:` |
|        7 |  7072 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  7073 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  7074 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  7075 | `	case PH7_OP_BAND_STORE:` |
|        3 |  7076 | `	case PH7_OP_BAND:` |
|        7 |  7077 | `	default:          r = a&b; break;` |
|        - |  7078 | `	}` |
|        - |  7079 | `	/* Push the result */` |
|       21 |  7080 | `	pNos->x.iVal = r;` |
|       21 |  7081 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  7082 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7083 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  7084 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  7085 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  7086 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  7087 | `	}` |
|       21 |  7088 | `	VmPopOperand(&pTos,1);` |
|       21 |  7089 | `	break;` |
|        - |  7090 | `				 }` |
|        - |  7091 | `/* OP_SHL * * *` |
|        - |  7092 | ` *` |
|        - |  7093 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  7094 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  7095 | ` * left by N bits where N is the top element on the stack.` |
|        - |  7096 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  7097 | ` */` |
|        - |  7098 | `/* OP_SHR * * *` |
|        - |  7099 | ` *` |
|        - |  7100 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  7101 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  7102 | ` * right by N bits where N is the top element on the stack.` |
|        - |  7103 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  7104 | ` */` |
|       12 |  7105 | `case PH7_OP_SHL:` |
|        - |  7106 | `case PH7_OP_SHR: {` |
|       25 |  7107 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7108 | `	sxi64 a,r;` |
|        - |  7109 | `	sxi32 b;` |
|        - |  7110 | `#ifdef UNTRUST` |
|        - |  7111 | `	if( pNos < pStack ){` |
|        - |  7112 | `		goto Abort;` |
|        - |  7113 | `	}` |
|        - |  7114 | `#endif` |
|        - |  7115 | `	/* Force the operands to be integer */` |
|       25 |  7116 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7117 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  7118 | `	}` |
|       25 |  7119 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7120 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  7121 | `	}` |
|        - |  7122 | `	/* Perform the requested operation */` |
|       25 |  7123 | `	a = pNos->x.iVal;` |
|       25 |  7124 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  7125 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  7126 | `		r = a << b;` |
|        8 |  7127 | `	}else{` |
|       11 |  7128 | `		r = a >> b;` |
|        - |  7129 | `	}` |
|        - |  7130 | `	/* Push the result */` |
|       25 |  7131 | `	pNos->x.iVal = r;` |
|       25 |  7132 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  7133 | `	VmPopOperand(&pTos,1);` |
|       25 |  7134 | `	break;` |
|        - |  7135 | `				 }` |
|        - |  7136 | `/*  OP_SHL_STORE * * *` |
|        - |  7137 | ` *` |
|        - |  7138 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  7139 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  7140 | ` * left by N bits where N is the top element on the stack.` |
|        - |  7141 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  7142 | ` */` |
|        - |  7143 | `/* OP_SHR_STORE * * *` |
|        - |  7144 | ` *` |
|        - |  7145 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  7146 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  7147 | ` * right by N bits where N is the top element on the stack.` |
|        - |  7148 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  7149 | ` */` |
|        9 |  7150 | `case PH7_OP_SHL_STORE:` |
|        - |  7151 | `case PH7_OP_SHR_STORE: {` |
|       19 |  7152 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7153 | `	ph7_value *pObj;` |
|        - |  7154 | `	sxi64 a,r;` |
|        - |  7155 | `	sxi32 b;` |
|        - |  7156 | `#ifdef UNTRUST` |
|        - |  7157 | `	if( pNos < pStack ){` |
|        - |  7158 | `		goto Abort;` |
|        - |  7159 | `	}` |
|        - |  7160 | `#endif` |
|        - |  7161 | `	/* Force the operands to be integer */` |
|       19 |  7162 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7163 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  7164 | `	}` |
|       19 |  7165 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7166 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  7167 | `	}` |
|        - |  7168 | `	/* Perform the requested operation */` |
|       19 |  7169 | `	a = pTos->x.iVal;` |
|       19 |  7170 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  7171 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  7172 | `		r = a << b;` |
|        5 |  7173 | `	}else{` |
|       11 |  7174 | `		r = a >> b;` |
|        - |  7175 | `	}` |
|        - |  7176 | `	/* Push the result */` |
|       19 |  7177 | `	pNos->x.iVal = r;` |
|       19 |  7178 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  7179 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7180 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  7181 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  7182 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  7183 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  7184 | `	}` |
|       19 |  7185 | `	VmPopOperand(&pTos,1);` |
|       19 |  7186 | `	break;` |
|        - |  7187 | `				 }` |
|        - |  7188 | `/* CAT:  P1 * *` |
|        - |  7189 | ` *` |
|        - |  7190 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  7191 | ` * back.` |
|        - |  7192 | ` */` |
|    73498 |  7193 | `case PH7_OP_CAT:{` |
|        - |  7194 | `	ph7_value *pNos,*pCur;` |
|   147001 |  7195 | `	if( pInstr->iP1 < 1 ){` |
|   119503 |  7196 | `		pNos = &pTos[-1];` |
|    59754 |  7197 | `	}else{` |
|    27503 |  7198 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  7199 | `	}` |
|        - |  7200 | `#ifdef UNTRUST` |
|        - |  7201 | `	if( pNos < pStack ){` |
|        - |  7202 | `		goto Abort;` |
|        - |  7203 | `	}` |
|        - |  7204 | `#endif` |
|        - |  7205 | `	/* Force a string cast */` |
|   147001 |  7206 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1685 |  7207 | `		PH7_MemObjToString(pNos);` |
|      840 |  7208 | `	}` |
|   147001 |  7209 | `	pCur = &pNos[1];` |
|   296755 |  7210 | `	while( pCur <= pTos ){` |
|   149759 |  7211 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50995 |  7212 | `			PH7_MemObjToString(pCur);` |
|    25495 |  7213 | `		}` |
|        - |  7214 | `		/* Perform the concatenation */` |
|   149759 |  7215 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   149715 |  7216 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - |  7217 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 |  7218 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  7219 | `				goto Abort;` |
|        - |  7220 | `			}` |
|    74855 |  7221 | `		}` |
|   149759 |  7222 | `		SyBlobRelease(&pCur->sBlob);` |
|   149759 |  7223 | `		pCur++;` |
|        5 |  7224 | `	}` |
|   147001 |  7225 | `	pTos = pNos;` |
|   147001 |  7226 | `	break;` |
|        - |  7227 | `				}` |
|        - |  7228 | `/*  CAT_STORE: * * *` |
|        - |  7229 | ` *` |
|        - |  7230 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  7231 | ` * back.` |
|        - |  7232 | ` */` |
|     4368 |  7233 | `case PH7_OP_CAT_STORE:{` |
|     8741 |  7234 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7235 | `	ph7_value *pObj;` |
|        - |  7236 | `	sxu32 nIdx;` |
|        - |  7237 | `#ifdef UNTRUST` |
|        - |  7238 | `	if( pNos < pStack ){` |
|        - |  7239 | `		goto Abort;` |
|        - |  7240 | `	}` |
|        - |  7241 | `#endif` |
|        - |  7242 | `	/* The right operand must be a string to append it */` |
|     8741 |  7243 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7244 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7245 | `	}` |
|     8741 |  7246 | `	nIdx = pTos->nIdx;` |
|        - |  7247 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - |  7248 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - |  7249 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - |  7250 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - |  7251 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - |  7252 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - |  7253 | `	 * the source we copy from — references share the slot index, so one check` |
|        - |  7254 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - |  7255 | `	 * must run before any mutation (left to the slow path).` |
|        - |  7256 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - |  7257 | `	 * and remains O(n^2) by design. */` |
|     8739 |  7258 | `	if( nIdx != SXU32_HIGH` |
|     8736 |  7259 | `	 && nIdx != pNos->nIdx` |
|     8732 |  7260 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|     8733 |  7261 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|     4367 |  7262 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|     8727 |  7263 | `		if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7264 | `			/* e.g. $x = 5; $x .= "a";  ->  "5a" */` |
|        3 |  7265 | `			PH7_MemObjToString(pObj);` |
|        1 |  7266 | `		}` |
|     8727 |  7267 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8725 |  7268 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  7269 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - |  7270 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 |  7271 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  7272 | `				goto Abort;` |
|        - |  7273 | `			}` |
|     4360 |  7274 | `		}` |
|        - |  7275 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - |  7276 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - |  7277 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - |  7278 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - |  7279 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - |  7280 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - |  7281 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - |  7282 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - |  7283 | `		 * the same slot is appended to again later in the statement` |
|        - |  7284 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - |  7285 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - |  7286 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|     8727 |  7287 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 |  7288 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 |  7289 | `		}` |
|     8727 |  7290 | `		pNos->nIdx = SXU32_HIGH;` |
|     8727 |  7291 | `		VmPopOperand(&pTos,1);` |
|     8734 |  7292 | `		break;` |
|        - |  7293 | `	}` |
|        - |  7294 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|       16 |  7295 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7296 | `		/* Force a string cast */` |
|        6 |  7297 | `		PH7_MemObjToString(pTos);` |
|        2 |  7298 | `	}` |
|        - |  7299 | `	/* Perform the concatenation (Reverse order) */` |
|       16 |  7300 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 |  7301 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  7302 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - |  7303 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 |  7304 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  7305 | `			goto Abort;` |
|        - |  7306 | `		}` |
|        7 |  7307 | `	}` |
|        - |  7308 | `	/* Perform the store operation */` |
|       16 |  7309 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7310 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 |  7311 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       16 |  7312 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 |  7313 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 |  7314 | `	}` |
|       11 |  7315 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 |  7316 | `	VmPopOperand(&pTos,1);` |
|       11 |  7317 | `	break;` |
|        - |  7318 | `				}` |
|        - |  7319 | `/* OP_AND: * * *` |
|        - |  7320 | ` *` |
|        - |  7321 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  7322 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7323 | ` * stack.` |
|        - |  7324 | ` */` |
|        - |  7325 | `/* OP_OR: * * *` |
|        - |  7326 | ` *` |
|        - |  7327 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  7328 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7329 | ` * stack.` |
|        - |  7330 | ` */` |
|   112072 |  7331 | `case PH7_OP_LAND:` |
|        - |  7332 | `case PH7_OP_LOR: {` |
|   224234 |  7333 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7334 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  7335 | `#ifdef UNTRUST` |
|        - |  7336 | `	if( pNos < pStack ){` |
|        - |  7337 | `		goto Abort;` |
|        - |  7338 | `	}` |
|        - |  7339 | `#endif` |
|        - |  7340 | `	/* Force a boolean cast */` |
|   224234 |  7341 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  7342 | `		PH7_MemObjToBool(pTos);` |
|        1 |  7343 | `	}` |
|   224234 |  7344 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7345 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7346 | `	}` |
|   224234 |  7347 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   224234 |  7348 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   224234 |  7349 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  7350 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   101374 |  7351 | `		v1 = and_logic[v1*3+v2];` |
|    50732 |  7352 | `	}else{` |
|        - |  7353 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   122865 |  7354 | `		v1 = or_logic[v1*3+v2];` |
|        - |  7355 | `	}` |
|   224234 |  7356 | `	if( v1 == 2 ){` |
|      ! 0 |  7357 | `		v1 = 1;` |
|      ! 0 |  7358 | `	}` |
|   224234 |  7359 | `	VmPopOperand(&pTos,1);` |
|   224234 |  7360 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   224234 |  7361 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   224234 |  7362 | `	break;` |
|        - |  7363 | `				 }` |
|        - |  7364 | `/*` |
|        - |  7365 | ` * OP_NULLC: * * *` |
|        - |  7366 | ` * Null coalescing operator '??'.` |
|        - |  7367 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  7368 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  7369 | ` */` |
|        - |  7370 | `/*` |
|        - |  7371 | ` * OP_NULLC: * P2 *` |
|        - |  7372 | ` * Short-circuit null coalescing '??'.` |
|        - |  7373 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  7374 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  7375 | ` */` |
|       99 |  7376 | `case PH7_OP_NULLC: {` |
|        - |  7377 | `#ifdef UNTRUST` |
|        - |  7378 | `	if( pTos < pStack ){` |
|        - |  7379 | `		goto Abort;` |
|        - |  7380 | `	}` |
|        - |  7381 | `#endif` |
|      203 |  7382 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7383 | `		/* Left is not null — keep it and skip the RHS */` |
|      123 |  7384 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       64 |  7385 | `	}else{` |
|        - |  7386 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       85 |  7387 | `		VmPopOperand(&pTos, 1);` |
|        - |  7388 | `	}` |
|      203 |  7389 | `	break;` |
|        - |  7390 |  |
|        - |  7391 | `/*` |
|        - |  7392 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  7393 | ` * Null coalescing assignment short-circuit.` |
|        - |  7394 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  7395 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  7396 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  7397 | ` */` |
|       28 |  7398 | `case PH7_OP_NULLC_JMP: {` |
|        - |  7399 | `#ifdef UNTRUST` |
|        - |  7400 | `	if( pTos < pStack ){` |
|        - |  7401 | `		goto Abort;` |
|        - |  7402 | `	}` |
|        - |  7403 | `#endif` |
|       59 |  7404 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  7405 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  7406 | `	}` |
|       59 |  7407 | `	break;` |
|        - |  7408 |  |
|        - |  7409 | `/*` |
|        - |  7410 | ` * OP_NULLC_STORE: * * *` |
|        - |  7411 | ` * Null coalescing assignment store.` |
|        - |  7412 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  7413 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  7414 | ` * expression result.` |
|        - |  7415 | ` */` |
|        - |  7416 | `/*` |
|        - |  7417 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  7418 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  7419 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  7420 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  7421 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  7422 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  7423 | ` */` |
|       51 |  7424 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  7425 | `#ifdef UNTRUST` |
|        - |  7426 | `	if( pTos < pStack ){` |
|        - |  7427 | `		goto Abort;` |
|        - |  7428 | `	}` |
|        - |  7429 | `#endif` |
|      105 |  7430 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  7431 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  7432 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  7433 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  7434 | `	}` |
|      105 |  7435 | `	break;` |
|        - |  7436 |  |
|       17 |  7437 | `case PH7_OP_NULLC_STORE: {` |
|       37 |  7438 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7439 | `	ph7_value *pObj;` |
|        - |  7440 | `	sxu32 nIdx;` |
|        - |  7441 | `#ifdef UNTRUST` |
|        - |  7442 | `	if( pNos < pStack ){` |
|        - |  7443 | `		goto Abort;` |
|        - |  7444 | `	}` |
|        - |  7445 | `#endif` |
|        - |  7446 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  7447 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  7448 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       37 |  7449 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  7450 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  7451 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  7452 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  7453 | `		ph7_value *apArg[2];` |
|        5 |  7454 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  7455 | `		apArg[1] = pTos;` |
|        5 |  7456 | `		if( pSet ){` |
|        5 |  7457 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  7458 | `		}` |
|        - |  7459 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  7460 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  7461 | `		VmPopOperand(&pTos,1);` |
|        - |  7462 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  7463 | `		VmCoalesceDisarm(pVm);` |
|        5 |  7464 | `		break;` |
|        - |  7465 | `	}` |
|       32 |  7466 | `	nIdx = pNos->nIdx;` |
|       32 |  7467 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  7468 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7469 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  7470 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  7471 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  7472 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  7473 | `	}` |
|       32 |  7474 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  7475 | `	VmPopOperand(&pTos,1);` |
|       32 |  7476 | `	break;` |
|        - |  7477 |  |
|        - |  7478 | `/*` |
|        - |  7479 | ` * OP_SPREAD: * * *` |
|        - |  7480 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  7481 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  7482 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  7483 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  7484 | ` */` |
|       10 |  7485 | `case PH7_OP_SPREAD: {` |
|        - |  7486 | `#ifdef UNTRUST` |
|        - |  7487 | `	if( pTos < pStack ){` |
|        - |  7488 | `		goto Abort;` |
|        - |  7489 | `	}` |
|        - |  7490 | `#endif` |
|        - |  7491 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - |  7492 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - |  7493 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - |  7494 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - |  7495 | `	 * be freed immediately. */` |
|       23 |  7496 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 |  7497 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - |  7498 | `		sxi32 rcW;` |
|        - |  7499 | `		sxu32 nEnt;` |
|        3 |  7500 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 |  7501 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 |  7502 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 |  7503 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 |  7504 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 |  7505 | `			goto Exception;` |
|        - |  7506 | `		}` |
|        3 |  7507 | `		nEnt = pTmpMap->nEntry;` |
|        3 |  7508 | `		if( nEnt == 0 ){` |
|      ! 0 |  7509 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7510 | `			pVm->iSpreadExtra--;` |
|        3 |  7511 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEnt - 1) >= VM_STACK_GUARD ){` |
|      ! 0 |  7512 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  7513 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)", VM_STACK_GUARD);` |
|      ! 0 |  7514 | `		}else{` |
|        3 |  7515 | `			ph7_hashmap_node *pNodeT = pTmpMap->pFirst;` |
|        - |  7516 | `			ph7_value *pElemT;` |
|        - |  7517 | `			sxu32 iT;` |
|        3 |  7518 | `			pElemT = (ph7_value *)SySetAt(&pVm->aMemObj, pNodeT->nValIdx);` |
|        3 |  7519 | `			if( pElemT ){ PH7_MemObjStore(pElemT, pTos); }else{ PH7_MemObjRelease(pTos); }` |
|        3 |  7520 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  7521 | `			pNodeT = pNodeT->pPrev;` |
|        7 |  7522 | `			for( iT = 1; iT < nEnt; iT++ ){` |
|        5 |  7523 | `				pTos++;` |
|        5 |  7524 | `				PH7_MemObjInit(pVm, pTos);` |
|        5 |  7525 | `				pTos->nIdx = SXU32_HIGH;` |
|        5 |  7526 | `				pElemT = (ph7_value *)SySetAt(&pVm->aMemObj, pNodeT->nValIdx);` |
|        5 |  7527 | `				if( pElemT ){ PH7_MemObjStore(pElemT, pTos); }` |
|        5 |  7528 | `				pNodeT = pNodeT->pPrev;` |
|        3 |  7529 | `			}` |
|        3 |  7530 | `			pVm->iSpreadExtra += (sxi32)(nEnt - 1);` |
|        - |  7531 | `		}` |
|        3 |  7532 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 |  7533 | `		break;` |
|        - |  7534 | `	}` |
|       21 |  7535 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       21 |  7536 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       21 |  7537 | `		sxu32 nEntry = pMap->nEntry;` |
|       21 |  7538 | `		if( nEntry == 0 ){` |
|        - |  7539 | `			/* Empty array — remove from stack */` |
|        3 |  7540 | `			VmPopOperand(&pTos, 1);` |
|        3 |  7541 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       20 |  7542 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  7543 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  7544 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  7545 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  7546 | `				VM_STACK_GUARD);` |
|      ! 0 |  7547 | `		}else{` |
|        - |  7548 | `			ph7_hashmap_node *pNode2;` |
|        - |  7549 | `			ph7_value *pElem;` |
|        - |  7550 | `			sxu32 i;` |
|        - |  7551 | `			/* Overwrite TOS with first element */` |
|       19 |  7552 | `			pNode2 = pMap->pFirst;` |
|       19 |  7553 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       19 |  7554 | `			PH7_MemObjRelease(pTos);` |
|       19 |  7555 | `			if( pElem ){` |
|       19 |  7556 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  7557 | `			}` |
|       19 |  7558 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7559 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  7560 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       19 |  7561 | `			pNode2 = pNode2->pPrev;` |
|        - |  7562 | `			/* Push remaining elements */` |
|       45 |  7563 | `			for( i = 1; i < nEntry; i++ ){` |
|       29 |  7564 | `				pTos++;` |
|       29 |  7565 | `				PH7_MemObjInit(pVm, pTos);` |
|       29 |  7566 | `				pTos->nIdx = SXU32_HIGH;` |
|       29 |  7567 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       29 |  7568 | `				if( pElem ){` |
|       29 |  7569 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  7570 | `				}` |
|       29 |  7571 | `				pNode2 = pNode2->pPrev;` |
|       16 |  7572 | `			}` |
|       19 |  7573 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  7574 | `		}` |
|        9 |  7575 | `	}` |
|        - |  7576 | `	/* else: not an array — leave as-is (single arg) */` |
|       21 |  7577 | `	break;` |
|        - |  7578 |  |
|        - |  7579 | `/*` |
|        - |  7580 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  7581 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  7582 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  7583 | ` */` |
|       37 |  7584 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  7585 | `#ifdef UNTRUST` |
|        - |  7586 | `	if( pTos < pStack ){` |
|        - |  7587 | `		goto Abort;` |
|        - |  7588 | `	}` |
|        - |  7589 | `#endif` |
|       77 |  7590 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       77 |  7591 | `	break;` |
|        - |  7592 |  |
|        - |  7593 | `/* OP_LXOR: * * *` |
|        - |  7594 | ` *` |
|        - |  7595 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  7596 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7597 | ` * stack.` |
|        - |  7598 | ` * According to the PHP language reference manual:` |
|        - |  7599 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  7600 | ` *  TRUE,but not both.` |
|        - |  7601 | ` */` |
|        5 |  7602 | `case PH7_OP_LXOR:{` |
|       11 |  7603 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  7604 | `	sxi32 v = 0;` |
|        - |  7605 | `#ifdef UNTRUST` |
|        - |  7606 | `	if( pNos < pStack ){` |
|        - |  7607 | `		goto Abort;` |
|        - |  7608 | `	}` |
|        - |  7609 | `#endif` |
|        - |  7610 | `	/* Force a boolean cast */` |
|       11 |  7611 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7612 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  7613 | `	}` |
|       11 |  7614 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7615 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7616 | `	}` |
|       11 |  7617 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  7618 | `		v = 1;` |
|        3 |  7619 | `	}` |
|       11 |  7620 | `	VmPopOperand(&pTos,1);` |
|       11 |  7621 | `	pTos->x.iVal = v;` |
|       11 |  7622 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  7623 | `	break;` |
|        - |  7624 | `				 }` |
|        - |  7625 | `/* OP_EQ P1 P2 P3` |
|        - |  7626 | ` *` |
|        - |  7627 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  7628 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7629 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7630 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7631 | ` */` |
|        - |  7632 | `/* OP_NEQ P1 P2 P3` |
|        - |  7633 | ` *` |
|        - |  7634 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  7635 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7636 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7637 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7638 | ` */` |
|     4726 |  7639 | `case PH7_OP_EQ:` |
|        - |  7640 | `case PH7_OP_NEQ: {` |
|     9457 |  7641 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7642 | `	/* Perform the comparison and act accordingly */` |
|        - |  7643 | `#ifdef UNTRUST` |
|        - |  7644 | `	if( pNos < pStack ){` |
|        - |  7645 | `		goto Abort;` |
|        - |  7646 | `	}` |
|        - |  7647 | `#endif` |
|     9457 |  7648 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9457 |  7649 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  7650 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9448 |  7651 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9413 |  7652 | `		rc = rc == 0;` |
|     4709 |  7653 | `	}else{` |
|       31 |  7654 | `		rc = rc != 0;` |
|        - |  7655 | `	}` |
|     9457 |  7656 | `	VmPopOperand(&pTos,1);` |
|     9457 |  7657 | `	if( !pInstr->iP2 ){` |
|        - |  7658 | `		/* Push comparison result without taking the jump */` |
|     9457 |  7659 | `		PH7_MemObjRelease(pTos);` |
|     9457 |  7660 | `		pTos->x.iVal = rc;` |
|        - |  7661 | `		/* Invalidate any prior representation */` |
|     9457 |  7662 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4731 |  7663 | `	}else{` |
|      ! 0 |  7664 | `		if( rc ){` |
|        - |  7665 | `			/* Jump to the desired location */` |
|      ! 0 |  7666 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7667 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7668 | `		}` |
|        - |  7669 | `	}` |
|     9457 |  7670 | `	break;` |
|        - |  7671 | `				 }` |
|        - |  7672 | `/* OP_TEQ P1 P2 *` |
|        - |  7673 | ` *` |
|        - |  7674 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  7675 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7676 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7677 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7678 | ` */` |
|   168144 |  7679 | `case PH7_OP_TEQ: {` |
|   336293 |  7680 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7681 | `	/* Perform the comparison and act accordingly */` |
|        - |  7682 | `#ifdef UNTRUST` |
|        - |  7683 | `	if( pNos < pStack ){` |
|        - |  7684 | `		goto Abort;` |
|        - |  7685 | `	}` |
|        - |  7686 | `#endif` |
|   336293 |  7687 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   336293 |  7688 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7689 | `		rc = 0;` |
|        2 |  7690 | `	}else{` |
|   336291 |  7691 | `		rc = rc == 0;` |
|        - |  7692 | `	}` |
|   336293 |  7693 | `	VmPopOperand(&pTos,1);` |
|   336293 |  7694 | `	if( !pInstr->iP2 ){` |
|        - |  7695 | `		/* Push comparison result without taking the jump */` |
|   336293 |  7696 | `		PH7_MemObjRelease(pTos);` |
|   336293 |  7697 | `		pTos->x.iVal = rc;` |
|        - |  7698 | `		/* Invalidate any prior representation */` |
|   336293 |  7699 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   168149 |  7700 | `	}else{` |
|      ! 0 |  7701 | `		if( rc ){` |
|        - |  7702 | `			/* Jump to the desired location */` |
|      ! 0 |  7703 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7704 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7705 | `		}` |
|        - |  7706 | `	}` |
|   336293 |  7707 | `	break;` |
|        - |  7708 | `				 }` |
|        - |  7709 | `/* OP_TNE P1 P2 *` |
|        - |  7710 | ` *` |
|        - |  7711 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  7712 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  7713 | ` * instruction.` |
|        - |  7714 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7715 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7716 | ` *` |
|        - |  7717 | ` */` |
|   129452 |  7718 | `case PH7_OP_TNE: {` |
|   258909 |  7719 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7720 | `	/* Perform the comparison and act accordingly */` |
|        - |  7721 | `#ifdef UNTRUST` |
|        - |  7722 | `	if( pNos < pStack ){` |
|        - |  7723 | `		goto Abort;` |
|        - |  7724 | `	}` |
|        - |  7725 | `#endif` |
|   258909 |  7726 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   258909 |  7727 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7728 | `		rc = 1;` |
|        2 |  7729 | `	}else{` |
|   258907 |  7730 | `		rc = rc != 0;` |
|        - |  7731 | `	}` |
|   258909 |  7732 | `	VmPopOperand(&pTos,1);` |
|   258909 |  7733 | `	if( !pInstr->iP2 ){` |
|        - |  7734 | `		/* Push comparison result without taking the jump */` |
|   258909 |  7735 | `		PH7_MemObjRelease(pTos);` |
|   258909 |  7736 | `		pTos->x.iVal = rc;` |
|        - |  7737 | `		/* Invalidate any prior representation */` |
|   258909 |  7738 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   129457 |  7739 | `	}else{` |
|      ! 0 |  7740 | `		if( rc ){` |
|        - |  7741 | `			/* Jump to the desired location */` |
|      ! 0 |  7742 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7743 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7744 | `		}` |
|        - |  7745 | `	}` |
|   258909 |  7746 | `	break;` |
|        - |  7747 | `				 }` |
|        - |  7748 | `/* OP_LT P1 P2 P3` |
|        - |  7749 | ` *` |
|        - |  7750 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7751 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7752 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7753 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7754 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7755 | ` *` |
|        - |  7756 | ` */` |
|        - |  7757 | `/* OP_LE P1 P2 P3` |
|        - |  7758 | ` *` |
|        - |  7759 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7760 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7761 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7762 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7763 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7764 | ` *` |
|        - |  7765 | ` */` |
|   115740 |  7766 | `case PH7_OP_LT:` |
|        - |  7767 | `case PH7_OP_LE: {` |
|   231570 |  7768 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7769 | `	/* Perform the comparison and act accordingly */` |
|        - |  7770 | `#ifdef UNTRUST` |
|        - |  7771 | `	if( pNos < pStack ){` |
|        - |  7772 | `		goto Abort;` |
|        - |  7773 | `	}` |
|        - |  7774 | `#endif` |
|   231570 |  7775 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   231570 |  7776 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7777 | `		rc = 0;` |
|   231566 |  7778 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1931 |  7779 | `		rc = rc < 1;` |
|      968 |  7780 | `	}else{` |
|   229636 |  7781 | `		rc = rc < 0;` |
|        - |  7782 | `	}` |
|   231570 |  7783 | `	VmPopOperand(&pTos,1);` |
|   231570 |  7784 | `	if( !pInstr->iP2 ){` |
|        - |  7785 | `		/* Push comparison result without taking the jump */` |
|   231570 |  7786 | `		PH7_MemObjRelease(pTos);` |
|   231570 |  7787 | `		pTos->x.iVal = rc;` |
|        - |  7788 | `		/* Invalidate any prior representation */` |
|   231570 |  7789 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   115830 |  7790 | `	}else{` |
|      ! 0 |  7791 | `		if( rc ){` |
|        - |  7792 | `			/* Jump to the desired location */` |
|      ! 0 |  7793 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7794 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7795 | `		}` |
|        - |  7796 | `	}` |
|   231570 |  7797 | `	break;` |
|        - |  7798 | `				}` |
|        - |  7799 | `/* OP_GT P1 P2 P3` |
|        - |  7800 | ` *` |
|        - |  7801 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7802 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7803 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7804 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7805 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7806 | ` *` |
|        - |  7807 | ` */` |
|        - |  7808 | `/* OP_GE P1 P2 P3` |
|        - |  7809 | ` *` |
|        - |  7810 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7811 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7812 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7813 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7814 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7815 | ` *` |
|        - |  7816 | ` */` |
|    58484 |  7817 | `case PH7_OP_GT:` |
|        - |  7818 | `case PH7_OP_GE: {` |
|   116973 |  7819 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7820 | `	/* Perform the comparison and act accordingly */` |
|        - |  7821 | `#ifdef UNTRUST` |
|        - |  7822 | `	if( pNos < pStack ){` |
|        - |  7823 | `		goto Abort;` |
|        - |  7824 | `	}` |
|        - |  7825 | `#endif` |
|   116973 |  7826 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   116973 |  7827 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7828 | `		rc = 0;` |
|   116969 |  7829 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   116533 |  7830 | `		rc = rc >= 0;` |
|    58269 |  7831 | `	}else{` |
|      437 |  7832 | `		rc = rc > 0;` |
|        - |  7833 | `	}` |
|   116973 |  7834 | `	VmPopOperand(&pTos,1);` |
|   116973 |  7835 | `	if( !pInstr->iP2 ){` |
|        - |  7836 | `		/* Push comparison result without taking the jump */` |
|   116973 |  7837 | `		PH7_MemObjRelease(pTos);` |
|   116973 |  7838 | `		pTos->x.iVal = rc;` |
|        - |  7839 | `		/* Invalidate any prior representation */` |
|   116973 |  7840 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    58489 |  7841 | `	}else{` |
|      ! 0 |  7842 | `		if( rc ){` |
|        - |  7843 | `			/* Jump to the desired location */` |
|      ! 0 |  7844 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7845 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7846 | `		}` |
|        - |  7847 | `	}` |
|   116973 |  7848 | `	break;` |
|        - |  7849 | `				}` |
|        - |  7850 | `/* OP_SPACESHIP * * *` |
|        - |  7851 | ` *` |
|        - |  7852 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  7853 | ` *   -1 if left < right` |
|        - |  7854 | ` *    0 if left == right` |
|        - |  7855 | ` *    1 if left > right` |
|        - |  7856 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  7857 | ` */` |
|       25 |  7858 | `case PH7_OP_SPACESHIP: {` |
|       51 |  7859 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7860 | `#ifdef UNTRUST` |
|        - |  7861 | `	if( pNos < pStack ){` |
|        - |  7862 | `		goto Abort;` |
|        - |  7863 | `	}` |
|        - |  7864 | `#endif` |
|       51 |  7865 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  7866 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  7867 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  7868 | `		rc = 1;` |
|        4 |  7869 | `	}else{` |
|        - |  7870 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  7871 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  7872 | `	}` |
|       51 |  7873 | `	VmPopOperand(&pTos,1);` |
|       51 |  7874 | `	PH7_MemObjRelease(pTos);` |
|       51 |  7875 | `	pTos->x.iVal = rc;` |
|       51 |  7876 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  7877 | `	break;` |
|        - |  7878 | `				}` |
|        - |  7879 | `/* OP_SEQ P1 P2 *` |
|        - |  7880 | ` * Strict string comparison.` |
|        - |  7881 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  7882 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7883 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7884 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7885 | ` * use PH7_OP_EQ.` |
|        - |  7886 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7887 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7888 | ` */` |
|        - |  7889 | `/* OP_SNE P1 P2 *` |
|        - |  7890 | ` * Strict string comparison.` |
|        - |  7891 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  7892 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7893 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7894 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7895 | ` * use PH7_OP_EQ.` |
|        - |  7896 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7897 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7898 | ` */` |
|       18 |  7899 | `case PH7_OP_SEQ:` |
|        - |  7900 | `case PH7_OP_SNE: {` |
|       38 |  7901 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7902 | `	SyString s1,s2;` |
|        - |  7903 | `	/* Perform the comparison and act accordingly */` |
|        - |  7904 | `#ifdef UNTRUST` |
|        - |  7905 | `	if( pNos < pStack ){` |
|        - |  7906 | `		goto Abort;` |
|        - |  7907 | `	}` |
|        - |  7908 | `#endif` |
|        - |  7909 | `	/* Force a string cast */` |
|       38 |  7910 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  7911 | `		PH7_MemObjToString(pTos);` |
|        2 |  7912 | `	}` |
|       38 |  7913 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7914 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7915 | `	}` |
|       38 |  7916 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  7917 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  7918 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  7919 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  7920 | `		rc = rc != 0;` |
|      ! 0 |  7921 | `	}else{` |
|       38 |  7922 | `		rc = rc == 0;` |
|        - |  7923 | `	}` |
|       38 |  7924 | `	VmPopOperand(&pTos,1);` |
|       38 |  7925 | `	if( !pInstr->iP2 ){` |
|        - |  7926 | `		/* Push comparison result without taking the jump */` |
|       38 |  7927 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7928 | `		pTos->x.iVal = rc;` |
|        - |  7929 | `		/* Invalidate any prior representation */` |
|       38 |  7930 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7931 | `	}else{` |
|      ! 0 |  7932 | `		if( rc ){` |
|        - |  7933 | `			/* Jump to the desired location */` |
|      ! 0 |  7934 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7935 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7936 | `		}` |
|        - |  7937 | `	}` |
|       38 |  7938 | `	break;` |
|        - |  7939 | `				 }` |
|        - |  7940 | `/*` |
|        - |  7941 | ` * OP_LOAD_REF * * *` |
|        - |  7942 | ` * Push the index of a referenced object on the stack.` |
|        - |  7943 | ` */` |
|       60 |  7944 | `case PH7_OP_LOAD_REF: {` |
|        - |  7945 | `	sxu32 nIdx;` |
|        - |  7946 | `#ifdef UNTRUST` |
|        - |  7947 | `	if( pTos < pStack ){` |
|        - |  7948 | `		goto Abort;` |
|        - |  7949 | `	}` |
|        - |  7950 | `#endif` |
|        - |  7951 | `	/* Extract memory object index */` |
|      121 |  7952 | `	nIdx = pTos->nIdx;` |
|      121 |  7953 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7954 | `		/* Nullify the object */` |
|      101 |  7955 | `		PH7_MemObjRelease(pTos);` |
|        - |  7956 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7957 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7958 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7959 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7960 | `	}` |
|      121 |  7961 | `	break;` |
|        - |  7962 | `					  }` |
|        - |  7963 | `/*` |
|        - |  7964 | ` * OP_STORE_REF * * P3` |
|        - |  7965 | ` * Perform an assignment operation by reference.` |
|        - |  7966 | ` */` |
|       18 |  7967 | ` case PH7_OP_STORE_REF: {` |
|       38 |  7968 | `	 SyString sName = { 0 , 0 };` |
|        - |  7969 | `	 VmFrame *pFrameLocal;` |
|        - |  7970 | `	SyHashEntry *pEntry;` |
|        - |  7971 | `	sxu32 nIdx;` |
|        - |  7972 | `#ifdef UNTRUST` |
|        - |  7973 | `	if( pTos < pStack ){` |
|        - |  7974 | `		goto Abort;` |
|        - |  7975 | `	}` |
|        - |  7976 | `#endif` |
|       38 |  7977 | `	if( pInstr->p3 == 0 ){` |
|        - |  7978 | `		char *zName;` |
|        - |  7979 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7980 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7981 | `			/* Force a string cast */` |
|      ! 0 |  7982 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7983 | `		}` |
|      ! 0 |  7984 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7985 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7986 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7987 | `			if( zName ){` |
|      ! 0 |  7988 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7989 | `			}` |
|      ! 0 |  7990 | `		}` |
|      ! 0 |  7991 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7992 | `		pTos--;` |
|      ! 0 |  7993 | `	}else{` |
|       38 |  7994 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7995 | `	}` |
|       38 |  7996 | `	nIdx = pTos->nIdx;` |
|       38 |  7997 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7998 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7999 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8000 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  8001 | `		}else{` |
|        - |  8002 | `			ph7_value *pObj;` |
|        - |  8003 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  8004 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  8005 | `			if( pObj == 0 ){` |
|      ! 0 |  8006 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8007 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  8008 | `				goto Abort;` |
|        - |  8009 | `			}` |
|        - |  8010 | `			/* Perform the store operation */` |
|      ! 0 |  8011 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  8012 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  8013 | `		}` |
|       38 |  8014 | `	}else if( sName.nByte > 0){` |
|       38 |  8015 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  8016 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  8017 | `		}else{` |
|       38 |  8018 | `			pFrameLocal = pVm->pFrame;` |
|       38 |  8019 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  8020 | `			/* Query the local frame */` |
|       38 |  8021 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       38 |  8022 | `			if( pEntry ){` |
|      ! 0 |  8023 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  8024 | `			}else{` |
|       38 |  8025 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       38 |  8026 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  8027 | `					/* Insert in the $GLOBALS array */` |
|       34 |  8028 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       16 |  8029 | `				}` |
|       38 |  8030 | `				if( rc == SXRET_OK ){` |
|       38 |  8031 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       18 |  8032 | `				}` |
|        - |  8033 | `			}` |
|        - |  8034 | `		}` |
|       18 |  8035 | `	}` |
|       38 |  8036 | `	break;` |
|        - |  8037 | `				 }` |
|        - |  8038 | `/*` |
|        - |  8039 | ` * OP_UPLINK P1 * *` |
|        - |  8040 | ` * Link a variable to the top active VM frame.` |
|        - |  8041 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  8042 | ` */` |
|       30 |  8043 | `case PH7_OP_UPLINK: {` |
|       65 |  8044 | `	if( pVm->pFrame->pParent ){` |
|       65 |  8045 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  8046 | `		SyString sName;` |
|        - |  8047 | `		/* Perform the link */` |
|      135 |  8048 | `		while( pLink <= pTos ){` |
|       75 |  8049 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  8050 | `				/* Force a string cast */` |
|      ! 0 |  8051 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  8052 | `			}` |
|       75 |  8053 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       75 |  8054 | `			if( sName.nByte > 0 ){` |
|       75 |  8055 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  8056 | `			}` |
|       75 |  8057 | `			pLink++;` |
|        5 |  8058 | `		}` |
|       30 |  8059 | `	}` |
|       65 |  8060 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       65 |  8061 | `	break;` |
|        - |  8062 | `					}` |
|        - |  8063 | `/*` |
|        - |  8064 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  8065 | ` * Push an exception in the corresponding container so that` |
|        - |  8066 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  8067 | ` */` |
|      227 |  8068 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      459 |  8069 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  8070 | `	VmFrame *pFrameLocal;` |
|        - |  8071 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      459 |  8072 | `	pException->iFinallyDone = 0;` |
|      459 |  8073 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  8074 | `	/* Create the exception frame */` |
|      459 |  8075 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      459 |  8076 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8077 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  8078 | `		goto Abort;` |
|        - |  8079 | `	}` |
|        - |  8080 | `	/* Mark the special frame */` |
|      459 |  8081 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      459 |  8082 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  8083 | `	/* Point to the frame that trigger the exception */` |
|      459 |  8084 | `	pFrameLocal = pFrameLocal->pParent;` |
|      459 |  8085 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      459 |  8086 | `	pException->pFrame = pFrameLocal;` |
|      459 |  8087 | `	break;` |
|        - |  8088 | `							}` |
|        - |  8089 | `/*` |
|        - |  8090 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  8091 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  8092 | ` */` |
|      221 |  8093 | `case PH7_OP_POP_EXCEPTION: {` |
|      447 |  8094 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      447 |  8095 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8096 | `		ph7_exception **apException;` |
|        - |  8097 | `		/* Pop the loaded exception */` |
|       36 |  8098 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       36 |  8099 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       32 |  8100 | `			(void)SySetPop(&pVm->aException);` |
|       15 |  8101 | `		}` |
|       17 |  8102 | `	}` |
|      447 |  8103 | `	pException->pFrame = 0;` |
|        - |  8104 | `	/* Leave the exception frame */` |
|      447 |  8105 | `	VmLeaveFrame(&(*pVm));` |
|        - |  8106 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      447 |  8107 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  8108 | `		sxi32 rcFinally;` |
|       22 |  8109 | `		pException->iFinallyDone = 1;` |
|       22 |  8110 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0,TRUE);` |
|       22 |  8111 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  8112 | `			goto Abort;` |
|        - |  8113 | `		}` |
|       10 |  8114 | `	}` |
|      447 |  8115 | `	if( pVm->bReturnRequested ){` |
|        - |  8116 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - |  8117 | `		 * function. Drain outer finally blocks first, then — only in the real` |
|        - |  8118 | `		 * function body — materialize; inside a mini-program propagate outward. */` |
|       29 |  8119 | `		rc = VmDrainFinally(&(*pVm),nExceptionBase);` |
|       29 |  8120 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8121 | `			goto Abort;` |
|        - |  8122 | `		}` |
|       29 |  8123 | `		if( !bReturnPropagates ){` |
|       27 |  8124 | `			VmMaterializeCatchReturn(&(*pVm),pResult,pEntryFrame);` |
|       13 |  8125 | `		}` |
|       29 |  8126 | `		goto Done;` |
|        - |  8127 | `	}` |
|      419 |  8128 | `	break;` |
|        - |  8129 | `							}` |
|        - |  8130 |  |
|        - |  8131 | `/*` |
|        - |  8132 | ` * OP_THROW * P2 *` |
|        - |  8133 | ` * Throw an user exception.` |
|        - |  8134 | ` */` |
|      104 |  8135 | `case PH7_OP_THROW: {` |
|      213 |  8136 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      213 |  8137 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  8138 | `#ifdef UNTRUST` |
|        - |  8139 | `	if( pTos < pStack ){` |
|        - |  8140 | `		goto Abort;` |
|        - |  8141 | `	}` |
|        - |  8142 | `#endif` |
|      213 |  8143 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  8144 | `	/* Tell the upper layer that an exception was thrown */` |
|      213 |  8145 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      213 |  8146 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      213 |  8147 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8148 | `		ph7_class *pThrowable;` |
|        - |  8149 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      213 |  8150 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      214 |  8151 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  8152 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  8153 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  8154 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  8155 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  8156 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  8157 | `			if( pErrorClass ){` |
|        3 |  8158 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  8159 | `			}` |
|        3 |  8160 | `			if( pErrInst ){` |
|        - |  8161 | `				ph7_class_method *pCons;` |
|        3 |  8162 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  8163 | `				if( pCons ){` |
|        - |  8164 | `					ph7_value sArg;` |
|        - |  8165 | `					ph7_value *apArg[1];` |
|        - |  8166 | `					SyString sMsgStr;` |
|        - |  8167 | `					static const char zErrMsg[] =` |
|        - |  8168 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  8169 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  8170 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  8171 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  8172 | `					apArg[0] = &sArg;` |
|        3 |  8173 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  8174 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  8175 | `				}` |
|        3 |  8176 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  8177 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  8178 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8179 | `					goto Abort;` |
|        - |  8180 | `				}` |
|        2 |  8181 | `			}else{` |
|        - |  8182 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  8183 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  8184 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8185 | `					goto Abort;` |
|        - |  8186 | `				}` |
|        - |  8187 | `			}` |
|        2 |  8188 | `		}else{` |
|        - |  8189 | `			/* Throw the exception */` |
|      211 |  8190 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      211 |  8191 | `			if( rc == SXERR_ABORT ){` |
|        - |  8192 | `				/* Abort processing immediately */` |
|       14 |  8193 | `				goto Abort;` |
|        - |  8194 | `			}` |
|        - |  8195 | `		}` |
|      103 |  8196 | `	}else{` |
|        - |  8197 | `		/* Expecting a class instance */` |
|      ! 0 |  8198 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  8199 | `		if( rc == SXERR_ABORT ){` |
|        - |  8200 | `			/* Abort processing immediately */` |
|      ! 0 |  8201 | `			goto Abort;` |
|        - |  8202 | `		}` |
|        - |  8203 | `	}` |
|        - |  8204 | `	/* Pop the top entry */` |
|      202 |  8205 | `	VmPopOperand(&pTos,1);` |
|        - |  8206 | `	/* Perform an unconditional jump to the try's OP_POP_EXCEPTION landing pad,` |
|        - |  8207 | `	 * which tears down the try frame, runs finally, and (when a catch/finally` |
|        - |  8208 | ``	 * issued a `return`) consumes pVm->bReturnRequested. Routing the return`` |
|        - |  8209 | `	 * through OP_POP_EXCEPTION keeps the frame stack balanced. */` |
|      202 |  8210 | `	pc = nJump - 1;` |
|      202 |  8211 | `	break;` |
|        - |  8212 | `				   }` |
|        - |  8213 | `/*` |
|        - |  8214 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  8215 | ` * Prepare a foreach step.` |
|        - |  8216 | ` */` |
|     6382 |  8217 | `case PH7_OP_FOREACH_INIT: {` |
|    12769 |  8218 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  8219 | `	void *pName;` |
|        - |  8220 | `#ifdef UNTRUST` |
|        - |  8221 | `	if( pTos < pStack ){` |
|        - |  8222 | `		goto Abort;` |
|        - |  8223 | `	}` |
|        - |  8224 | `#endif` |
|    12769 |  8225 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  8226 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  8227 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  8228 | `			/* Force a string cast */` |
|      ! 0 |  8229 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  8230 | `		}` |
|        - |  8231 | `		/* Duplicate name */` |
|      ! 0 |  8232 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  8233 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8234 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8235 | `		}` |
|      ! 0 |  8236 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  8237 | `	}` |
|    12769 |  8238 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  8239 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  8240 | `			/* Force a string cast */` |
|      ! 0 |  8241 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  8242 | `		}` |
|        - |  8243 | `		/* Duplicate name */` |
|      ! 0 |  8244 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  8245 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8246 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8247 | `		}` |
|      ! 0 |  8248 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  8249 | `	}` |
|        - |  8250 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12769 |  8251 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  8252 | `		/* Jump out of the loop */` |
|      ! 0 |  8253 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8254 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  8255 | `		}` |
|      ! 0 |  8256 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  8257 | `	}else{` |
|        - |  8258 | `		ph7_foreach_step *pStep;` |
|    12769 |  8259 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12769 |  8260 | `		if( pStep == 0 ){` |
|      ! 0 |  8261 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  8262 | `			/* Jump out of the loop */` |
|      ! 0 |  8263 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  8264 | `		}else{` |
|        - |  8265 | `			/* Zero the structure */` |
|    12769 |  8266 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  8267 | `			/* Prepare the step */` |
|    12769 |  8268 | `			pStep->iFlags = pInfo->iFlags;` |
|    12769 |  8269 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8270 | `				ph7_hashmap *pMap;` |
|        - |  8271 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  8272 | `				 * source array so mutations don't affect other sharers. */` |
|    12735 |  8273 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  8274 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  8275 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  8276 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  8277 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  8278 | `						 * variable still points at the same hashmap as` |
|        - |  8279 | `						 * the stack value. */` |
|        9 |  8280 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  8281 | `							pCur->iRef--;` |
|        - |  8282 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|        - |  8283 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|        - |  8284 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|        9 |  8285 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  8286 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  8287 | `						}` |
|        4 |  8288 | `					}` |
|        4 |  8289 | `				}` |
|    12735 |  8290 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  8291 | `				/* Reset the internal loop cursor */` |
|    12735 |  8292 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  8293 | `				/* Mark the step */` |
|    12735 |  8294 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12735 |  8295 | `				pStep->xIter.pMap = pMap;` |
|    12735 |  8296 | `				pMap->iRef++;` |
|     6370 |  8297 | `			}else{` |
|       39 |  8298 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8299 | `				ph7_class *pIteratorClass;` |
|        - |  8300 | `				/* Check if the object implements Iterator */` |
|       39 |  8301 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       50 |  8302 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  8303 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  8304 | `					ph7_class_method *pRewind;` |
|       26 |  8305 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       26 |  8306 | `					pStep->xIter.pThis = pThis;` |
|       26 |  8307 | `					pThis->iRef++;` |
|       26 |  8308 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       26 |  8309 | `					if( pRewind ){` |
|       26 |  8310 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  8311 | `					}` |
|       15 |  8312 | `				}else{` |
|        - |  8313 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  8314 | `					ph7_class *pIterAggClass;` |
|       14 |  8315 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  8316 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  8317 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  8318 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  8319 | `						ph7_class_method *pGetIter;` |
|        3 |  8320 | `						int iterAggOk = 0;` |
|        3 |  8321 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  8322 | `						if( pGetIter ){` |
|        - |  8323 | `							ph7_value sResult;` |
|        3 |  8324 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  8325 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  8326 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  8327 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  8328 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  8329 | `									ph7_class_method *pRewind;` |
|        3 |  8330 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  8331 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  8332 | `									pIterObj->iRef++;` |
|        - |  8333 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  8334 | `									pStep->pOwner = pThis;` |
|        3 |  8335 | `									pThis->iRef++;` |
|        3 |  8336 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  8337 | `									if( pRewind ){` |
|        3 |  8338 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  8339 | `									}` |
|        3 |  8340 | `									iterAggOk = 1;` |
|        1 |  8341 | `								}` |
|        1 |  8342 | `							}` |
|        3 |  8343 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  8344 | `						}` |
|        3 |  8345 | `						if( !iterAggOk ){` |
|        - |  8346 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  8347 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8348 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  8349 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  8350 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  8351 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  8352 | `						}` |
|        2 |  8353 | `					}else{` |
|        - |  8354 | `						/* Plain object iteration via hAttr */` |
|       12 |  8355 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  8356 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  8357 | `						pStep->xIter.pThis = pThis;` |
|       12 |  8358 | `						pThis->iRef++;` |
|        - |  8359 | `					}` |
|        - |  8360 | `				}` |
|        - |  8361 | `			}` |
|        - |  8362 | `		}` |
|    12769 |  8363 | `		if( pStep ){` |
|    12769 |  8364 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  8365 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  8366 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  8367 | `				/* Jump out of the loop */` |
|      ! 0 |  8368 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  8369 | `			}` |
|     6382 |  8370 | `		}` |
|        - |  8371 | `	}` |
|    12769 |  8372 | `	VmPopOperand(&pTos,1);` |
|    12769 |  8373 | `	break;` |
|        - |  8374 | `						  }` |
|        - |  8375 | `/*` |
|        - |  8376 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  8377 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  8378 | ` */` |
|   104952 |  8379 | `case PH7_OP_FOREACH_STEP: {` |
|   209909 |  8380 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  8381 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  8382 | `	ph7_value *pValue;` |
|        - |  8383 | `	VmFrame *pFrameLocal;` |
|        - |  8384 | `	/* Peek the last step */` |
|   209909 |  8385 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   209909 |  8386 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   209909 |  8387 | `	pFrameLocal = pVm->pFrame;` |
|   209909 |  8388 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   209909 |  8389 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   209775 |  8390 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  8391 | `		ph7_hashmap_node *pNode;` |
|        - |  8392 | `		/* Extract the current node value */` |
|   209775 |  8393 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   209775 |  8394 | `		if( pNode == 0 ){` |
|        - |  8395 | `			/* No more entry to process */` |
|    12733 |  8396 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12733 |  8397 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8398 | `				/* Break the reference with the last element */` |
|        7 |  8399 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  8400 | `			}` |
|        - |  8401 | `			/* Automatically reset the loop cursor */` |
|    12733 |  8402 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  8403 | `			/* Cleanup the mess left behind */` |
|    12733 |  8404 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12733 |  8405 | `			SySetPop(&pInfo->aStep);` |
|    12733 |  8406 | `			PH7_HashmapUnref(pMap);` |
|     6369 |  8407 | `		}else{` |
|   197047 |  8408 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      531 |  8409 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      531 |  8410 | `				if( pKey ){` |
|      531 |  8411 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  8412 | `				}` |
|      263 |  8413 | `			}` |
|   197047 |  8414 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8415 | `				SyHashEntry *pEntry;` |
|        - |  8416 | `				/* Pass by reference */` |
|       23 |  8417 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  8418 | `				if( pEntry ){` |
|       21 |  8419 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  8420 | `				}else{` |
|        4 |  8421 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  8422 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  8423 | `				}` |
|       12 |  8424 | `			}else{` |
|        - |  8425 | `				/* Make a copy of the entry value */` |
|   197025 |  8426 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   197025 |  8427 | `				if( pValue ){` |
|   197025 |  8428 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    98510 |  8429 | `				}` |
|        - |  8430 | `			}` |
|        5 |  8431 | `		}` |
|   105024 |  8432 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  8433 | `		/* Iterator-based iteration.` |
|        - |  8434 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  8435 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  8436 | `		 */` |
|      109 |  8437 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  8438 | `		ph7_class_method *pMethod;` |
|        - |  8439 | `		ph7_value sResult;` |
|      109 |  8440 | `		int isValid = 0;` |
|        - |  8441 | `		/* Call next() to advance — but skip on the first iteration */` |
|      109 |  8442 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       29 |  8443 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       17 |  8444 | `		}else{` |
|       85 |  8445 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       85 |  8446 | `			if( pMethod ){` |
|       85 |  8447 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  8448 | `			}` |
|        - |  8449 | `		}` |
|        - |  8450 | `		/* Call valid() */` |
|      109 |  8451 | `		PH7_MemObjInit(pVm,&sResult);` |
|      109 |  8452 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      109 |  8453 | `		if( pMethod ){` |
|      109 |  8454 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      109 |  8455 | `			PH7_MemObjToBool(&sResult);` |
|      109 |  8456 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  8457 | `		}` |
|      109 |  8458 | `		PH7_MemObjRelease(&sResult);` |
|      109 |  8459 | `		if( !isValid ){` |
|        - |  8460 | `			/* Iterator exhausted */` |
|       27 |  8461 | `			pc = pInstr->iP2 - 1;` |
|        - |  8462 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       27 |  8463 | `			if( pStep->pOwner ){` |
|        3 |  8464 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  8465 | `			}` |
|       27 |  8466 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       27 |  8467 | `			SySetPop(&pInfo->aStep);` |
|       27 |  8468 | `			PH7_ClassInstanceUnref(pThis);` |
|       16 |  8469 | `		}else{` |
|        - |  8470 | `			/* Call current() to get value */` |
|       87 |  8471 | `			PH7_MemObjInit(pVm,&sResult);` |
|       87 |  8472 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       87 |  8473 | `			if( pMethod ){` |
|       87 |  8474 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  8475 | `			}` |
|       87 |  8476 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       87 |  8477 | `			if( pValue ){` |
|       87 |  8478 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  8479 | `			}` |
|       87 |  8480 | `			PH7_MemObjRelease(&sResult);` |
|        - |  8481 | `			/* Call key() if needed */` |
|       87 |  8482 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  8483 | `				ph7_value sKey;` |
|       37 |  8484 | `				PH7_MemObjInit(pVm,&sKey);` |
|       37 |  8485 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       37 |  8486 | `				if( pMethod ){` |
|       37 |  8487 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  8488 | `				}` |
|       37 |  8489 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       37 |  8490 | `				if( pValue ){` |
|       37 |  8491 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  8492 | `				}` |
|       37 |  8493 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  8494 | `			}` |
|        - |  8495 | `		}` |
|       57 |  8496 | `	}else{` |
|       32 |  8497 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  8498 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  8499 | `		SyHashEntry *pEntry;` |
|        - |  8500 | `		/* Point to the next attribute */` |
|       36 |  8501 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  8502 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  8503 | `			/* Check access permission */` |
|       38 |  8504 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  8505 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  8506 | `					break; /* Access is granted */` |
|        - |  8507 | `			}` |
|        1 |  8508 | `		}` |
|       32 |  8509 | `		if( pEntry == 0 ){` |
|        - |  8510 | `			/* Clean up the mess left behind */` |
|       12 |  8511 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  8512 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8513 | `				/* Break the reference with the last element */` |
|        3 |  8514 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  8515 | `			}` |
|       12 |  8516 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  8517 | `			SySetPop(&pInfo->aStep);` |
|       12 |  8518 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  8519 | `		}else{` |
|       22 |  8520 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  8521 | `			ph7_value *pAttrValue;` |
|       22 |  8522 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  8523 | `				/* Fill with the current attribute name */` |
|       22 |  8524 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  8525 | `				if( pKey ){` |
|       22 |  8526 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  8527 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  8528 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  8529 | `				}` |
|       10 |  8530 | `			}` |
|        - |  8531 | `			/* Extract attribute value */` |
|       22 |  8532 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  8533 | `			if( pAttrValue ){` |
|       22 |  8534 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8535 | `					/* Pass by reference */` |
|        3 |  8536 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  8537 | `					if( pEntry ){` |
|        3 |  8538 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  8539 | `					}else{` |
|      ! 0 |  8540 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  8541 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  8542 | `					}` |
|        2 |  8543 | `				}else{` |
|        - |  8544 | `					/* Make a copy of the attribute value */` |
|       20 |  8545 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  8546 | `					if( pValue ){` |
|       20 |  8547 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  8548 | `					}` |
|        - |  8549 | `				}` |
|       10 |  8550 | `			}` |
|        - |  8551 | `		}` |
|        - |  8552 | `	}` |
|   209909 |  8553 | `	break;` |
|        - |  8554 | `						  }` |
|        - |  8555 | `/*` |
|        - |  8556 | ` * OP_MEMBER P1 P2` |
|        - |  8557 | ` * Load class attribute/method on the stack.` |
|        - |  8558 | ` */` |
|     4473 |  8559 | `case PH7_OP_MEMBER: {` |
|        - |  8560 | `	ph7_class_instance *pThis;` |
|        - |  8561 | `	ph7_value *pNos;` |
|        - |  8562 | `	SyString sName;` |
|     8951 |  8563 | `	if( !pInstr->iP1 ){` |
|     8669 |  8564 | `		pNos = &pTos[-1];` |
|        - |  8565 | `#ifdef UNTRUST` |
|        - |  8566 | `		if( pNos < pStack ){` |
|        - |  8567 | `			goto Abort;` |
|        - |  8568 | `		}` |
|        - |  8569 | `#endif` |
|     8669 |  8570 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8571 | `			ph7_class *pClass;` |
|        - |  8572 | `			/* Class already instantiated */` |
|     8667 |  8573 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  8574 | `			/* Point to the instantiated class */` |
|     8667 |  8575 | `			pClass = pThis->pClass;` |
|        - |  8576 | `			/* Extract attribute name first */` |
|     8667 |  8577 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     8667 |  8578 | `			if( pInstr->iP2 ){` |
|        - |  8579 | `				/* Method call */` |
|      825 |  8580 | `				ph7_class_method *pMeth = 0;` |
|      825 |  8581 | `				if( sName.nByte > 0 ){` |
|        - |  8582 | `					/* Extract the target method */` |
|      825 |  8583 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      410 |  8584 | `				}` |
|      825 |  8585 | `				if( pMeth == 0 ){` |
|      ! 0 |  8586 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  8587 | `						&pClass->sName,&sName` |
|        - |  8588 | `						);` |
|        - |  8589 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  8590 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  8591 | `					/* Pop the method name from the stack */` |
|      ! 0 |  8592 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8593 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  8594 | `				}else{` |
|        - |  8595 | `					/* Push method name on the stack */` |
|      825 |  8596 | `					PH7_MemObjRelease(pTos);` |
|      825 |  8597 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      825 |  8598 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8599 | `				}` |
|      825 |  8600 | `				pTos->nIdx = SXU32_HIGH;` |
|      415 |  8601 | `			}else{` |
|        - |  8602 | `				/* Attribute access */` |
|     7847 |  8603 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  8604 | `				SyHashEntry *pEntry;` |
|        - |  8605 | `				/* Extract the target attribute */` |
|     7847 |  8606 | `				if( sName.nByte > 0 ){` |
|     7847 |  8607 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     7847 |  8608 | `					if( pEntry ){` |
|        - |  8609 | `						/* Point to the attribute value */` |
|     7845 |  8610 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3920 |  8611 | `					}` |
|     3921 |  8612 | `				}` |
|     7847 |  8613 | `				if( pObjAttr == 0 ){` |
|        - |  8614 | `					/* No such attribute,load null */` |
|        4 |  8615 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  8616 | `						&pClass->sName,&sName);` |
|        - |  8617 | `					/* Call the __get magic method if available */` |
|        3 |  8618 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  8619 | `				}` |
|     7847 |  8620 | `				VmPopOperand(&pTos,1);` |
|        - |  8621 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  8622 | `				 * This is due to the following case:` |
|        - |  8623 | `				 *     (new TestClass())->foo;` |
|        - |  8624 | `				 */` |
|     7847 |  8625 | `				pThis->iRef++;` |
|     7847 |  8626 | `				PH7_MemObjRelease(pTos);` |
|     7847 |  8627 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     7847 |  8628 | `				if( pObjAttr ){` |
|     7845 |  8629 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  8630 | `					/* Check attribute access */` |
|     7845 |  8631 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  8632 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  8633 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  8634 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  8635 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  8636 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     7840 |  8637 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3988 |  8638 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|      131 |  8639 | `							VmInstr *pNext = pInstr + 1;` |
|      131 |  8640 | `							int bIsLhs = 0;` |
|      131 |  8641 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|      127 |  8642 | `								bIsLhs = 1;` |
|       61 |  8643 | `							}` |
|      131 |  8644 | `							if( !bIsLhs ){` |
|        6 |  8645 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        6 |  8646 | `								PH7_ClassInstanceUnref(pThis);` |
|        6 |  8647 | `								if( rcU == PH7_ABORT ){` |
|        5 |  8648 | `									goto Abort;` |
|        - |  8649 | `								}` |
|        - |  8650 | `								{` |
|        3 |  8651 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8652 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8653 | `										pc = pFrm2->iExceptionJump - 1;` |
|     4472 |  8654 | `										break;` |
|        - |  8655 | `									}` |
|        - |  8656 | `								}` |
|      ! 0 |  8657 | `								goto Exception;` |
|        - |  8658 | `							}` |
|       61 |  8659 | `						}` |
|        - |  8660 | `						/* Load attribute */` |
|     7841 |  8661 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     7841 |  8662 | `						if( pValue ){` |
|     7841 |  8663 | `							if( pThis->iRef < 2 ){` |
|        - |  8664 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  8665 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  8666 | `								 */` |
|       11 |  8667 | `								PH7_MemObjStore(pValue,pTos);` |
|        6 |  8668 | `							}else{` |
|        - |  8669 | `								/* Simple load */` |
|     7831 |  8670 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  8671 | `							}` |
|     7841 |  8672 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     7839 |  8673 | `								if( pThis->iRef > 1 ){` |
|        - |  8674 | `									/* Load attribute index */` |
|     7829 |  8675 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3912 |  8676 | `								}` |
|     3917 |  8677 | `							}` |
|     3918 |  8678 | `						}` |
|     3923 |  8679 | `					}else{` |
|        - |  8680 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  8681 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  8682 | `						char zMsg[256];` |
|      ! 0 |  8683 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8684 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8685 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8686 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  8687 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8688 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8689 | `						goto Abort;` |
|        - |  8690 | `					}` |
|     3918 |  8691 | `				}` |
|        - |  8692 | `				/* Safely unreference the object */` |
|     7843 |  8693 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  8694 | `			}` |
|     4334 |  8695 | `		}else{` |
|        3 |  8696 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  8697 | `			VmPopOperand(&pTos,1);` |
|        3 |  8698 | `			PH7_MemObjRelease(pTos);` |
|        3 |  8699 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  8700 | `		}` |
|     4335 |  8701 | `	}else{` |
|        - |  8702 | `		/* Static member access using class name */` |
|      287 |  8703 | `		pNos = pTos;` |
|      287 |  8704 | `		pThis = 0;` |
|      287 |  8705 | `		if( !pInstr->p3 ){` |
|      237 |  8706 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      237 |  8707 | `			pNos--;` |
|        - |  8708 | `#ifdef UNTRUST` |
|        - |  8709 | `			if( pNos < pStack ){` |
|        - |  8710 | `				goto Abort;` |
|        - |  8711 | `			}` |
|        - |  8712 | `#endif` |
|      121 |  8713 | `		}else{` |
|        - |  8714 | `			/* Attribute name already computed */` |
|       54 |  8715 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  8716 | `		}` |
|      287 |  8717 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      287 |  8718 | `			ph7_class *pClass = 0;` |
|      287 |  8719 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8720 | `				/* Class already instantiated */` |
|        5 |  8721 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  8722 | `				pClass = pThis->pClass;` |
|        5 |  8723 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  8724 | `			}else{` |
|        - |  8725 | `				/* Try to extract the target class */` |
|      283 |  8726 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      283 |  8727 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      283 |  8728 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  8729 | `					/* Handle self/static/parent keywords */` |
|      283 |  8730 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       65 |  8731 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       65 |  8732 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  8733 | `							/* In a trait method, self:: resolves to the using class */` |
|       14 |  8734 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       11 |  8735 | `						}` |
|      253 |  8736 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       29 |  8737 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      223 |  8738 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       29 |  8739 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       29 |  8740 | `						if( pSelf && pSelf->pBase ){` |
|       29 |  8741 | `							pClass = pSelf->pBase;` |
|       13 |  8742 | `						}` |
|       16 |  8743 | `					}else{` |
|      171 |  8744 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  8745 | `					}` |
|      139 |  8746 | `				}` |
|        - |  8747 | `			}` |
|      287 |  8748 | `			if( pClass == 0 ){` |
|        - |  8749 | `				/* Undefined class */` |
|      ! 0 |  8750 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  8751 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  8752 | `					);` |
|      ! 0 |  8753 | `				if( !pInstr->p3 ){` |
|      ! 0 |  8754 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8755 | `				}` |
|      ! 0 |  8756 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  8757 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  8758 | `			}else{` |
|      287 |  8759 | `				if( pInstr->iP2 ){` |
|        - |  8760 | `					/* Method call */` |
|       89 |  8761 | `					ph7_class_method *pMeth = 0;` |
|       89 |  8762 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  8763 | `						/* Extract the target method */` |
|       89 |  8764 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  8765 | `					}` |
|       89 |  8766 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  8767 | `						if( pMeth ){` |
|      ! 0 |  8768 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  8769 | `								&pClass->sName,&sName` |
|        - |  8770 | `								);` |
|      ! 0 |  8771 | `						}else{` |
|      ! 0 |  8772 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8773 | `								&pClass->sName,&sName` |
|        - |  8774 | `								);` |
|        - |  8775 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  8776 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  8777 | `						}` |
|        - |  8778 | `						/* Pop the method name from the stack */` |
|      ! 0 |  8779 | `						if( !pInstr->p3 ){` |
|      ! 0 |  8780 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  8781 | `						}` |
|      ! 0 |  8782 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  8783 | `					}else{` |
|        - |  8784 | `						/* Push method name on the stack */` |
|       89 |  8785 | `						PH7_MemObjRelease(pTos);` |
|       89 |  8786 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       89 |  8787 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8788 | `					}` |
|       89 |  8789 | `					pTos->nIdx = SXU32_HIGH;` |
|       47 |  8790 | `				}else{` |
|        - |  8791 | `					/* Attribute access */` |
|      203 |  8792 | `					ph7_class_attr *pAttr = 0;` |
|        - |  8793 | `					/* Check for special ::class pseudo-constant */` |
|      249 |  8794 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  8795 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  8796 | `						/* ::class returns the fully qualified class name */` |
|        - |  8797 | `						/* Pop the attribute name from the stack */` |
|       62 |  8798 | `						if( !pInstr->p3 ){` |
|       62 |  8799 | `							VmPopOperand(&pTos,1);` |
|       29 |  8800 | `						}` |
|       62 |  8801 | `						PH7_MemObjRelease(pTos);` |
|        - |  8802 | `						/* Load the class name */` |
|       62 |  8803 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       62 |  8804 | `						pTos->nIdx = SXU32_HIGH;` |
|       33 |  8805 | `					}else{` |
|        - |  8806 | `						/* Extract the target attribute */` |
|      144 |  8807 | `						if( sName.nByte > 0 ){` |
|      144 |  8808 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       70 |  8809 | `						}` |
|      144 |  8810 | `						if( pAttr == 0 ){` |
|        - |  8811 | `							/* No such attribute,load null */` |
|      ! 0 |  8812 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8813 | `								&pClass->sName,&sName);` |
|        - |  8814 | `							/* Call the __get magic method if available */` |
|      ! 0 |  8815 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  8816 | `						}` |
|        - |  8817 | `						/* Pop the attribute name from the stack */` |
|      144 |  8818 | `						if( !pInstr->p3 ){` |
|       92 |  8819 | `							VmPopOperand(&pTos,1);` |
|       45 |  8820 | `						}` |
|      144 |  8821 | `						PH7_MemObjRelease(pTos);` |
|      144 |  8822 | `						pTos->nIdx = SXU32_HIGH;` |
|      144 |  8823 | `						if( pAttr ){` |
|      144 |  8824 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  8825 | `								/* Access to a non static attribute */` |
|      ! 0 |  8826 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8827 | `									&pClass->sName,&pAttr->sName` |
|        - |  8828 | `									);` |
|      ! 0 |  8829 | `							}else{` |
|        - |  8830 | `								ph7_value *pValue;` |
|        - |  8831 | `								/* Check if the access to the attribute is allowed */` |
|      144 |  8832 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  8833 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  8834 | `									 * Same LHS-of-store peek as the instance path. */` |
|      136 |  8835 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|      105 |  8836 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       60 |  8837 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       38 |  8838 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       41 |  8839 | `										if( pS ){` |
|       41 |  8840 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       41 |  8841 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  8842 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  8843 | `												int bIsLhs = 0;` |
|        8 |  8844 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  8845 | `													bIsLhs = 1;` |
|        2 |  8846 | `												}` |
|        8 |  8847 | `												if( !bIsLhs ){` |
|        3 |  8848 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  8849 | `													if( pThis ){` |
|      ! 0 |  8850 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8851 | `													}` |
|        3 |  8852 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  8853 | `														goto Abort;` |
|        - |  8854 | `													}` |
|        - |  8855 | `													{` |
|        3 |  8856 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8857 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8858 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  8859 | `															break;` |
|        - |  8860 | `														}` |
|        - |  8861 | `													}` |
|      ! 0 |  8862 | `													goto Exception;` |
|        - |  8863 | `												}` |
|        2 |  8864 | `											}` |
|       18 |  8865 | `										}` |
|       18 |  8866 | `									}` |
|        - |  8867 | `									/* Load the desired attribute */` |
|      138 |  8868 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|      138 |  8869 | `									if( pValue ){` |
|      138 |  8870 | `										PH7_MemObjLoad(pValue,pTos);` |
|      138 |  8871 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  8872 | `											/* Load index number */` |
|       52 |  8873 | `											pTos->nIdx = pAttr->nIdx;` |
|       24 |  8874 | `										}` |
|       67 |  8875 | `									}` |
|       71 |  8876 | `								}else{` |
|        - |  8877 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  8878 | `									char zMsg[256];` |
|        5 |  8879 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  8880 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  8881 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  8882 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  8883 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  8884 | `									}else{` |
|      ! 0 |  8885 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8886 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8887 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  8888 | `									}` |
|        5 |  8889 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  8890 | `									goto Abort;` |
|        - |  8891 | `								}` |
|        - |  8892 | `							}` |
|       67 |  8893 | `						}` |
|        - |  8894 | `					}` |
|        - |  8895 | `				}` |
|      281 |  8896 | `				if( pThis ){` |
|        - |  8897 | `					/* Safely unreference the object */` |
|        5 |  8898 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  8899 | `				}` |
|        - |  8900 | `			}` |
|      143 |  8901 | `		}else{` |
|        - |  8902 | `			/* Pop operands */` |
|      ! 0 |  8903 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  8904 | `			if( !pInstr->p3 ){` |
|      ! 0 |  8905 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  8906 | `			}` |
|      ! 0 |  8907 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8908 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  8909 | `		}` |
|        - |  8910 | `	}` |
|     8941 |  8911 | `	break;` |
|        - |  8912 | `					}` |
|        - |  8913 | `/*` |
|        - |  8914 | ` * OP_NEW P1 * * *` |
|        - |  8915 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  8916 | ` */` |
|      718 |  8917 | `case PH7_OP_NEW: {` |
|     1441 |  8918 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1441 |  8919 | `	ph7_class *pClass = 0;` |
|        - |  8920 | `	ph7_class_instance *pNew;` |
|     1441 |  8921 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  8922 | `		/* Try to extract the desired class */` |
|     2159 |  8923 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1436 |  8924 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      718 |  8925 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8926 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8927 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8928 | `	}` |
|     1441 |  8929 | `	if( pClass == 0 ){` |
|        - |  8930 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8931 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8932 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8933 | `			);` |
|        - |  8934 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8935 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8936 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8937 | `			/* Pop given arguments */` |
|      ! 0 |  8938 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8939 | `		}` |
|      ! 0 |  8940 | `		goto Abort;` |
|      ! 0 |  8941 | `	}else{` |
|        - |  8942 | `		ph7_class_method *pCons;` |
|        - |  8943 | `		/* Create a new class instance */` |
|     1441 |  8944 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1441 |  8945 | `		if( pNew == 0 ){` |
|      ! 0 |  8946 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8947 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8948 | `				&pClass->sName` |
|        - |  8949 | `			);` |
|      ! 0 |  8950 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8951 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8952 | `				/* Pop given arguments */` |
|      ! 0 |  8953 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8954 | `			}` |
|      ! 0 |  8955 | `			break;` |
|        - |  8956 | `		}` |
|        - |  8957 | `		/* Check if a constructor is available */` |
|     1441 |  8958 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1441 |  8959 | `		if( pCons == 0 ){` |
|      961 |  8960 | `			SyString *pName = &pClass->sName;` |
|        - |  8961 | `			/* Check for a constructor with the same base class name */` |
|      961 |  8962 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      478 |  8963 | `		}` |
|     1441 |  8964 | `		if( pCons ){` |
|        - |  8965 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8966 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8967 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8968 | `			 * (including variadic string-key packing). */` |
|      485 |  8969 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8970 | `			sxi32 rcCons;` |
|      485 |  8971 | `			SySetReset(&aArg);` |
|      939 |  8972 | `			while( pArg < pTos ){` |
|      459 |  8973 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      459 |  8974 | `				pArg++;` |
|        5 |  8975 | `			}` |
|      485 |  8976 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8977 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8978 | `				sxu32 n;` |
|      139 |  8979 | `				n = SySetUsed(&aArg);` |
|        - |  8980 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8981 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8982 | `				 * after resolution). */` |
|      255 |  8983 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      121 |  8984 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      121 |  8985 | `					if( pFuncArg ){` |
|      121 |  8986 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        8 |  8987 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8988 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8989 | `						}` |
|       58 |  8990 | `					}` |
|      121 |  8991 | `					n++;` |
|        5 |  8992 | `				}` |
|       67 |  8993 | `			}` |
|      485 |  8994 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8995 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      485 |  8996 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8997 | `				pNew->iRef = 1;` |
|      ! 0 |  8998 | `			}` |
|      485 |  8999 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  9000 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  9001 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  9002 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  9003 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  9004 | `				sxi32 iResumePc;` |
|        5 |  9005 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  9006 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  9007 | `					goto Abort;` |
|        - |  9008 | `				}` |
|        5 |  9009 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  9010 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  9011 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  9012 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  9013 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  9014 | `					}` |
|        5 |  9015 | `					PH7_MemObjRelease(pTos);` |
|        5 |  9016 | `					pc = iResumePc;` |
|        5 |  9017 | `					break;` |
|        - |  9018 | `				}` |
|      ! 0 |  9019 | `				goto Exception;` |
|        - |  9020 | `			}` |
|      238 |  9021 | `		}` |
|     1437 |  9022 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9023 | `			/* Pop given arguments */` |
|      379 |  9024 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      187 |  9025 | `		}` |
|     1437 |  9026 | `		PH7_MemObjRelease(pTos);` |
|     1437 |  9027 | `		pTos->x.pOther = pNew;` |
|     1437 |  9028 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  9029 | `	}` |
|     1437 |  9030 | `	break;` |
|        - |  9031 | `				 }` |
|        - |  9032 | `/*` |
|        - |  9033 | ` * OP_CLONE * * *` |
|        - |  9034 | ` * Perfome a clone operation.` |
|        - |  9035 | ` */` |
|       27 |  9036 | `case PH7_OP_CLONE: {` |
|        - |  9037 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  9038 | `#ifdef UNTRUST` |
|        - |  9039 | `	if( pTos < pStack ){` |
|        - |  9040 | `		goto Abort;` |
|        - |  9041 | `	}` |
|        - |  9042 | `#endif` |
|        - |  9043 | `	/* Make sure we are dealing with a class instance */` |
|       58 |  9044 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  9045 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  9046 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  9047 | `		PH7_MemObjRelease(pTos);` |
|        5 |  9048 | `		break;` |
|        - |  9049 | `	}` |
|        - |  9050 | `	/* Point to the source */` |
|       54 |  9051 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  9052 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       54 |  9053 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  9054 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9055 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  9056 | `			&pSrc->pClass->sName);` |
|      ! 0 |  9057 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  9058 | `		break;` |
|        - |  9059 | `	}` |
|        - |  9060 | `	/* Perform the clone operation */` |
|       54 |  9061 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       54 |  9062 | `	PH7_MemObjRelease(pTos);` |
|       54 |  9063 | `	if( pClone == 0 ){` |
|      ! 0 |  9064 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  9065 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  9066 | `	}else{` |
|        - |  9067 | `		/* Load the cloned object */` |
|       54 |  9068 | `		pTos->x.pOther = pClone;` |
|       54 |  9069 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  9070 | `	}` |
|       54 |  9071 | `	break;` |
|        - |  9072 | `				   }` |
|        - |  9073 | `/*` |
|        - |  9074 | ` * OP_SWITCH * * P3` |
|        - |  9075 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  9076 | ` */` |
|       26 |  9077 | `case PH7_OP_SWITCH: {` |
|       57 |  9078 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  9079 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  9080 | `	ph7_value sValue,sCaseValue;` |
|        - |  9081 | `	sxu32 n,nEntry;` |
|        - |  9082 | `#ifdef UNTRUST` |
|        - |  9083 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  9084 | `		goto Abort;` |
|        - |  9085 | `	}` |
|        - |  9086 | `#endif` |
|        - |  9087 | `	/* Point to the case table  */` |
|       57 |  9088 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       57 |  9089 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  9090 | `	/* Select the appropriate case block to execute */` |
|       57 |  9091 | `	PH7_MemObjInit(pVm,&sValue);` |
|       57 |  9092 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      135 |  9093 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      133 |  9094 | `		pCase = &aCase[n];` |
|      133 |  9095 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  9096 | `		/* Execute the case expression first */` |
|      133 |  9097 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);` |
|        - |  9098 | `		/* Compare the two expression */` |
|      133 |  9099 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      133 |  9100 | `		PH7_MemObjRelease(&sValue);` |
|      133 |  9101 | `		PH7_MemObjRelease(&sCaseValue);` |
|      133 |  9102 | `		if( rc == 0 ){` |
|        - |  9103 | `			/* Value match,jump to this block */` |
|       55 |  9104 | `			pc = pCase->nStart - 1;` |
|       55 |  9105 | `			break;` |
|        - |  9106 | `		}` |
|       44 |  9107 | `	}` |
|       57 |  9108 | `	VmPopOperand(&pTos,1);` |
|       57 |  9109 | `	if( n >= nEntry ){` |
|        - |  9110 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  9111 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  9112 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  9113 | `		}else{` |
|        - |  9114 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  9115 | `			pc = pSwitch->nOut - 1;` |
|        - |  9116 | `		}` |
|        1 |  9117 | `	}` |
|       57 |  9118 | `	break;` |
|        - |  9119 | `					}` |
|        - |  9120 | `/*` |
|        - |  9121 | ` * OP_MATCH * * P3` |
|        - |  9122 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  9123 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  9124 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  9125 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  9126 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  9127 | ` */` |
|       54 |  9128 | `case PH7_OP_MATCH: {` |
|      111 |  9129 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      111 |  9130 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  9131 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  9132 | `	sxu32 i,j,nArm,nCond;` |
|      111 |  9133 | `	int matched = 0;` |
|        - |  9134 | `#ifdef UNTRUST` |
|        - |  9135 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  9136 | `		goto Abort;` |
|        - |  9137 | `	}` |
|        - |  9138 | `#endif` |
|      111 |  9139 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      111 |  9140 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      111 |  9141 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      111 |  9142 | `	PH7_MemObjInit(pVm,&sCond);` |
|      111 |  9143 | `	PH7_MemObjInit(pVm,&sResult);` |
|      111 |  9144 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      349 |  9145 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  9146 | `		pArm = &aArm[i];` |
|      240 |  9147 | `		if( pArm->bDefault ){` |
|       13 |  9148 | `			pDefault = pArm;` |
|       13 |  9149 | `			continue;` |
|        - |  9150 | `		}` |
|      228 |  9151 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  9152 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  9153 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  9154 | `			if( pCondBc == 0 ){` |
|      ! 0 |  9155 | `				continue;` |
|        - |  9156 | `			}` |
|      260 |  9157 | `			VmLocalExec(pVm,pCondBc,&sCond,FALSE);` |
|      260 |  9158 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  9159 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  9160 | `			if( rc == 0 ){` |
|       93 |  9161 | `				VmLocalExec(pVm,&pArm->aResult,&sResult,FALSE);` |
|       93 |  9162 | `				matched = 1;` |
|       93 |  9163 | `				break;` |
|        - |  9164 | `			}` |
|       85 |  9165 | `		}` |
|      115 |  9166 | `	}` |
|      111 |  9167 | `	if( !matched && pDefault ){` |
|       13 |  9168 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult,FALSE);` |
|       13 |  9169 | `		matched = 1;` |
|        6 |  9170 | `	}` |
|      111 |  9171 | `	if( !matched ){` |
|        6 |  9172 | `		const char *zType = "unknown";` |
|        - |  9173 | `		char zMsg[128];` |
|        - |  9174 | `		sxu32 nMsg;` |
|        6 |  9175 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  9176 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  9177 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        6 |  9178 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  9179 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  9180 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  9181 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  9182 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  9183 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  9184 | `		default: break;` |
|        - |  9185 | `		}` |
|        8 |  9186 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  9187 | `			"Unhandled match case of type %s",zType);` |
|        8 |  9188 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  9189 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        6 |  9190 | `		PH7_MemObjRelease(&sSubject);` |
|        6 |  9191 | `		PH7_MemObjRelease(&sResult);` |
|        6 |  9192 | `		goto Abort;` |
|        - |  9193 | `	}` |
|      105 |  9194 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  9195 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  9196 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  9197 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  9198 | `	break;` |
|        - |  9199 | `					}` |
|        - |  9200 | `/*` |
|        - |  9201 | ` * OP_YIELD P1 P2 *` |
|        - |  9202 | ` *  Yield a value from a generator function.` |
|        - |  9203 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  9204 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  9205 | ` */` |
|       62 |  9206 | `case PH7_OP_YIELD: {` |
|        - |  9207 | `	ph7_generator *pGen;` |
|      129 |  9208 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  9209 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  9210 | `		goto Abort;` |
|        - |  9211 | `	}` |
|      129 |  9212 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|      129 |  9213 | `	if( pInstr->iP2 ){` |
|        - |  9214 | `		/* yield $key => $value: value on top, key below */` |
|        - |  9215 | `#ifdef UNTRUST` |
|        - |  9216 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  9217 | `#endif` |
|       20 |  9218 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       20 |  9219 | `		VmPopOperand(&pTos, 1);` |
|       20 |  9220 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       20 |  9221 | `		VmPopOperand(&pTos, 1);` |
|        - |  9222 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       20 |  9223 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  9224 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  9225 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  9226 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  9227 | `			}` |
|        2 |  9228 | `		}` |
|      120 |  9229 | `	}else if( pInstr->iP1 ){` |
|        - |  9230 | `		/* yield $value */` |
|        - |  9231 | `#ifdef UNTRUST` |
|        - |  9232 | `		if( pTos < pStack ) goto Abort;` |
|        - |  9233 | `#endif` |
|      111 |  9234 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|      111 |  9235 | `		VmPopOperand(&pTos, 1);` |
|        - |  9236 | `		/* Auto-increment key */` |
|      111 |  9237 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      111 |  9238 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      111 |  9239 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       58 |  9240 | `	}else{` |
|        - |  9241 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  9242 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  9243 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  9244 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  9245 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  9246 | `	}` |
|        - |  9247 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|      129 |  9248 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|      129 |  9249 | `	goto Suspend;` |
|        - |  9250 |  |
|        - |  9251 | `/*` |
|        - |  9252 | ` * OP_CALL P1 * *` |
|        - |  9253 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  9254 | ` *  function on the stack.` |
|        - |  9255 | ` */` |
|   369454 |  9256 | `case PH7_OP_CALL: {` |
|   738998 |  9257 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  9258 | `	ph7_value *pArg;` |
|   738998 |  9259 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   738998 |  9260 | `	pArg = &pTos[-nCallArgs];` |
|        - |  9261 | `	SyHashEntry *pEntry;` |
|        - |  9262 | `	SyString sName;` |
|        - |  9263 | `	/* Extract function name */` |
|   738998 |  9264 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  9265 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  9266 | `			ph7_value sResult;` |
|        - |  9267 | `			sxi32 rcArr;` |
|        3 |  9268 | `			SySetReset(&aArg);` |
|        3 |  9269 | `			while( pArg < pTos ){` |
|      ! 0 |  9270 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  9271 | `				pArg++;` |
|      ! 0 |  9272 | `			}` |
|        3 |  9273 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9274 | `			/* May be a class instance and it's static method */` |
|        3 |  9275 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  9276 | `			SySetReset(&aArg);` |
|        - |  9277 | `			/* Pop given arguments */` |
|        3 |  9278 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9279 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9280 | `			}` |
|        3 |  9281 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  9282 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9283 | `				goto Abort;` |
|        - |  9284 | `			}` |
|        3 |  9285 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  9286 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  9287 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  9288 | `				sxi32 iResumePc;` |
|        3 |  9289 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  9290 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  9291 | `					PH7_MemObjRelease(pTos);` |
|        3 |  9292 | `					pc = iResumePc;` |
|        3 |  9293 | `					break;` |
|        - |  9294 | `				}` |
|      ! 0 |  9295 | `				goto Exception;` |
|        - |  9296 | `			}` |
|        - |  9297 | `			/* Copy result */` |
|      ! 0 |  9298 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  9299 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  9300 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  9301 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  9302 | `			ph7_value sResult;` |
|        - |  9303 | `			sxi32 rcInv;` |
|       84 |  9304 | `			SySetReset(&aArg);` |
|      200 |  9305 | `			while( pArg < pTos ){` |
|      118 |  9306 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  9307 | `				pArg++;` |
|        2 |  9308 | `			}` |
|       84 |  9309 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  9310 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  9311 | `				(int)SySetUsed(&aArg),` |
|       82 |  9312 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  9313 | `				&sResult,` |
|       82 |  9314 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  9315 | `			SySetReset(&aArg);` |
|       84 |  9316 | `			if( nCallArgs > 0 ){` |
|       76 |  9317 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  9318 | `			}` |
|       84 |  9319 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  9320 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  9321 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  9322 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  9323 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  9324 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  9325 | `				pThis->iRef++;` |
|       13 |  9326 | `				PH7_MemObjRelease(pTos);` |
|       13 |  9327 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  9328 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  9329 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9330 | `					goto Abort;` |
|        - |  9331 | `				}` |
|        - |  9332 | `				{` |
|       13 |  9333 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  9334 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  9335 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  9336 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  9337 | `						break;` |
|        - |  9338 | `					}` |
|        - |  9339 | `				}` |
|      ! 0 |  9340 | `				goto Exception;` |
|        - |  9341 | `			}` |
|       72 |  9342 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  9343 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9344 | `				goto Abort;` |
|        - |  9345 | `			}` |
|       72 |  9346 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  9347 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  9348 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  9349 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  9350 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  9351 | `				sxi32 iResumePc;` |
|        7 |  9352 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  9353 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  9354 | `					PH7_MemObjRelease(pTos);` |
|        5 |  9355 | `					pc = iResumePc;` |
|        5 |  9356 | `					break;` |
|        - |  9357 | `				}` |
|        3 |  9358 | `				goto Exception;` |
|        - |  9359 | `			}` |
|       66 |  9360 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  9361 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  9362 | `		}else{` |
|        - |  9363 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  9364 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  9365 | `			/* Pop given arguments */` |
|      ! 0 |  9366 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9367 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9368 | `			}` |
|        - |  9369 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9370 | `			PH7_MemObjRelease(pTos);` |
|        - |  9371 | `		}` |
|       66 |  9372 | `		break;` |
|        - |  9373 | `	}` |
|   738914 |  9374 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  9375 | `	/* Check for a compiled function first.` |
|        - |  9376 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  9377 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   738914 |  9378 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9379 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  9380 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  9381 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  9382 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  9383 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  9384 | `	{` |
|   738914 |  9385 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   738914 |  9386 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  9387 | `		const char *zFunc;` |
|        - |  9388 | `		const char *zEnd;` |
|        - |  9389 | `		const char *z;` |
|        - |  9390 | `		SyString sGlobal;` |
|       24 |  9391 | `		zFunc = sName.zString;` |
|       24 |  9392 | `		zEnd  = zFunc + sName.nByte;` |
|       24 |  9393 | `		z = zEnd;` |
|        - |  9394 | `		/* Find last namespace separator */` |
|      196 |  9395 | `		while( z > zFunc ){` |
|      196 |  9396 | `			if( z[-1] == '\\' ){` |
|       24 |  9397 | `				break;` |
|        - |  9398 | `			}` |
|      176 |  9399 | `			z--;` |
|        4 |  9400 | `		}` |
|       24 |  9401 | `		if( z > zFunc && z < zEnd ){` |
|        - |  9402 | `			/* Retry lookup using the unqualified/global function name */` |
|       24 |  9403 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       24 |  9404 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  9405 | `		}` |
|       10 |  9406 | `	}` |
|        - |  9407 | `	} /* end VmCallArgMap namespace scope */` |
|   738914 |  9408 | `	if( pEntry ){` |
|        - |  9409 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  9410 | `		ph7_class_instance *pThis;` |
|        - |  9411 | `		ph7_value *pFrameStack;` |
|        - |  9412 | `		ph7_vm_func *pVmFunc;` |
|        - |  9413 | `		ph7_class *pSelf;` |
|        - |  9414 | `		VmFrame *pFrame;` |
|        - |  9415 | `		ph7_value *pObj;` |
|        - |  9416 | `		VmSlot sArg;` |
|        - |  9417 | `		sxu32 n;` |
|        - |  9418 | `		/* initialize fields */` |
|    20089 |  9419 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    20089 |  9420 | `		pThis = 0;` |
|    20089 |  9421 | `		pSelf = 0;` |
|    20089 |  9422 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  9423 | `			ph7_class_method *pMeth;` |
|        - |  9424 | `			/* Class method call */` |
|     3881 |  9425 | `			ph7_value *pTarget = &pTos[-1];` |
|     3881 |  9426 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  9427 | `				/* Extract the 'this' pointer */` |
|     3881 |  9428 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  9429 | `					/* Instance already loaded */` |
|     3791 |  9430 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3791 |  9431 | `					pThis->iRef++;` |
|     3791 |  9432 | `					pSelf = pThis->pClass;` |
|     1893 |  9433 | `				}` |
|     3881 |  9434 | `				if( pSelf == 0 ){` |
|       95 |  9435 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  9436 | `						/* "Late Static Binding" class name */` |
|      131 |  9437 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  9438 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  9439 | `					}` |
|       95 |  9440 | `					if( pSelf == 0 ){` |
|       21 |  9441 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  9442 | `					}` |
|       45 |  9443 | `				}` |
|     3881 |  9444 | `				if( pThis == 0  ){` |
|       95 |  9445 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       95 |  9446 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       95 |  9447 | `					if( pFrameLocal->pParent ){` |
|        - |  9448 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       69 |  9449 | `						pThis = pFrameLocal->pThis;` |
|       69 |  9450 | `						if( pThis ){` |
|       21 |  9451 | `							pThis->iRef++;` |
|       10 |  9452 | `						}` |
|       32 |  9453 | `					}` |
|       45 |  9454 | `				}` |
|     3881 |  9455 | `				VmPopOperand(&pTos,1);` |
|     3881 |  9456 | `				PH7_MemObjRelease(pTos);` |
|        - |  9457 | `				/* Synchronize pointers */` |
|     3881 |  9458 | `				pArg = &pTos[-nCallArgs];` |
|        - |  9459 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  9460 | `				 * user have already computed the random generated unique class method name` |
|        - |  9461 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  9462 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  9463 | `				 */` |
|     3881 |  9464 | `				while( pArg < pStack ){` |
|      ! 0 |  9465 | `					pArg++;` |
|      ! 0 |  9466 | `				}` |
|     3881 |  9467 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  9468 | `					/* Check if the call is allowed */` |
|     3881 |  9469 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3881 |  9470 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  9471 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  9472 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  9473 | `							char zMsg[256];` |
|      ! 0 |  9474 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  9475 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  9476 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  9477 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  9478 | `							/* Pop given arguments */` |
|      ! 0 |  9479 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  9480 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9481 | `							}` |
|      ! 0 |  9482 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  9483 | `							goto Abort;` |
|        - |  9484 | `						}` |
|        6 |  9485 | `					}` |
|     1938 |  9486 | `				}` |
|     1938 |  9487 | `			}` |
|     1938 |  9488 | `		}` |
|        - |  9489 | `		/* Check The recursion limit. Hitting it raises a clean, non-catchable` |
|        - |  9490 | `		 * fatal (was: silently set NULL and continue) and halts. The check is` |
|        - |  9491 | `		 * before VmEnterFrame/the recursive VmByteCodeExec below, so a` |
|        - |  9492 | `		 * correctly-set cap also keeps deep recursion off the native stack. */` |
|    20089 |  9493 | `		if( VmRecursionExceeded(pVm) ){` |
|        - |  9494 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - |  9495 | `			 * which walks the whole operand stack — don't release them here. */` |
|        6 |  9496 | `			VmRecursionFatal(&(*pVm));` |
|        6 |  9497 | `			goto Abort;` |
|        - |  9498 | `		}` |
|    20085 |  9499 | `		if( pVmFunc->pNextName ){` |
|        - |  9500 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  9501 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  9502 | `		}` |
|    20085 |  9503 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  9504 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  9505 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  9506 | `			ph7_generator *pGenerator;` |
|        - |  9507 | `			ph7_class_instance *pGenObj;` |
|        - |  9508 | `			ph7_value *pCtxAttr;` |
|        - |  9509 | `			SyString sAttrName;` |
|        - |  9510 | `			ph7_value **apCallArgs;` |
|        - |  9511 | `			int nGenArgs, iArg;` |
|        - |  9512 | `			/* Collect arguments from the operand stack */` |
|       53 |  9513 | `			nGenArgs = (int)(pTos - pArg);` |
|       53 |  9514 | `			apCallArgs = 0;` |
|       53 |  9515 | `			if( nGenArgs > 0 ){` |
|       14 |  9516 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9517 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  9518 | `				if( apCallArgs == 0 ){` |
|        - |  9519 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  9520 | `					nGenArgs = 0;` |
|      ! 0 |  9521 | `				}else{` |
|       10 |  9522 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  9523 | `					int didReorder = 0;` |
|       10 |  9524 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  9525 | `						/* Named-argument reordering for generator */` |
|        5 |  9526 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  9527 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  9528 | `						sxu32 nNV = nF;` |
|        5 |  9529 | `						sxi32 iVIdx = -1;` |
|        - |  9530 | `						sxi32 *aGSlot;` |
|        - |  9531 | `						sxu8 *aGUsed;` |
|        - |  9532 | `						sxu32 gi;` |
|       13 |  9533 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  9534 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  9535 | `						}` |
|        7 |  9536 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9537 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  9538 | `						if( aGSlot ){` |
|        5 |  9539 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  9540 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  9541 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  9542 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9543 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  9544 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9545 | `								goto Abort;` |
|        - |  9546 | `							}` |
|        - |  9547 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  9548 | `							 * append overflow (variadic / positional beyond` |
|        - |  9549 | `							 * formals) so downstream sees every argument. */` |
|        - |  9550 | `							{` |
|        5 |  9551 | `								int nOut = 0;` |
|       13 |  9552 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  9553 | `									sxu32 gj;` |
|       13 |  9554 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  9555 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  9556 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  9557 | `											break;` |
|        - |  9558 | `										}` |
|        3 |  9559 | `									}` |
|        5 |  9560 | `								}` |
|       13 |  9561 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  9562 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  9563 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  9564 | `									}` |
|        5 |  9565 | `								}` |
|        5 |  9566 | `								nGenArgs = nOut;` |
|        - |  9567 | `							}` |
|        5 |  9568 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  9569 | `							didReorder = 1;` |
|        2 |  9570 | `						}` |
|        - |  9571 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  9572 | `						 * positional fill below — preserves arg order rather` |
|        - |  9573 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  9574 | `					}` |
|       10 |  9575 | `					if( !didReorder ){` |
|       12 |  9576 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  9577 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  9578 | `						}` |
|        2 |  9579 | `					}` |
|        - |  9580 | `				}` |
|        4 |  9581 | `			}` |
|        - |  9582 | `			/* Create execution context and generator wrapper */` |
|       53 |  9583 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       53 |  9584 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  9585 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9586 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9587 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9588 | `				break;` |
|        - |  9589 | `			}` |
|       53 |  9590 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       53 |  9591 | `			if( pGenerator == 0 ){` |
|      ! 0 |  9592 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  9593 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9594 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9595 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9596 | `				break;` |
|        - |  9597 | `			}` |
|        - |  9598 | `			/* Set up the frame with arguments, closure env, $this */` |
|       53 |  9599 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       53 |  9600 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       53 |  9601 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       53 |  9602 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       53 |  9603 | `			pExecCtx->pFrame->pParent = 0;` |
|       53 |  9604 | `			if( apCallArgs ){` |
|       10 |  9605 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  9606 | `			}` |
|       53 |  9607 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9608 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9609 | `				if( pThis ){` |
|      ! 0 |  9610 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9611 | `				}` |
|      ! 0 |  9612 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9613 | `					goto Abort;` |
|        - |  9614 | `				}` |
|      ! 0 |  9615 | `				break;` |
|        - |  9616 | `			}` |
|        - |  9617 | `			/* Create Generator class instance */` |
|       53 |  9618 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       53 |  9619 | `			if( pGenObj == 0 ){` |
|      ! 0 |  9620 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9621 | `				break;` |
|        - |  9622 | `			}` |
|        - |  9623 | `			/* Store generator in __ctx attribute */` |
|       53 |  9624 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       53 |  9625 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       53 |  9626 | `			if( pCtxAttr ){` |
|       53 |  9627 | `				pCtxAttr->x.pOther = pGenerator;` |
|       53 |  9628 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       24 |  9629 | `			}` |
|        - |  9630 | `			/* Pop args and function name, push Generator object */` |
|       53 |  9631 | `			PH7_MemObjRelease(pTos);` |
|       53 |  9632 | `			pTos = &pTos[-nCallArgs];` |
|       53 |  9633 | `			pTos->x.pOther = pGenObj;` |
|       53 |  9634 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       53 |  9635 | `			pGenObj->iRef++;` |
|       53 |  9636 | `			if( pThis ){` |
|      ! 0 |  9637 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9638 | `			}` |
|       53 |  9639 | `			break;` |
|        - |  9640 | `		}` |
|        - |  9641 | `		/* Extract the formal argument set */` |
|    20037 |  9642 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  9643 | `		/* Create a new VM frame  */` |
|    20037 |  9644 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    20037 |  9645 | `		if( rc != SXRET_OK ){` |
|        - |  9646 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9647 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9648 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9649 | `				&pVmFunc->sName);` |
|        - |  9650 | `			/* Pop given arguments */` |
|      ! 0 |  9651 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9652 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9653 | `			}` |
|        - |  9654 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9655 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  9656 | `			break;` |
|        - |  9657 | `		}` |
|    20037 |  9658 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  9659 | `			/* Install the '$this' variable */` |
|        - |  9660 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3809 |  9661 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3809 |  9662 | `			if( pObj ){` |
|        - |  9663 | `				/* Reflect the change */` |
|     3809 |  9664 | `				pObj->x.pOther = pThis;` |
|     3809 |  9665 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1902 |  9666 | `			}` |
|     1902 |  9667 | `		}` |
|    20037 |  9668 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  9669 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  9670 | `			/* Install static variables */` |
|       13 |  9671 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       25 |  9672 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|       13 |  9673 | `				pStatic = &aStatic[n];` |
|       13 |  9674 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  9675 | `					/* Initialize the static variables */` |
|        9 |  9676 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|        9 |  9677 | `					if( pObj ){` |
|        - |  9678 | `						/* Assume a NULL initialization value */` |
|        9 |  9679 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|        9 |  9680 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  9681 | `							/* Evaluate initialization expression (Any complex expression) */` |
|        9 |  9682 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|        4 |  9683 | `						}` |
|        9 |  9684 | `						pObj->nIdx = pStatic->nIdx;` |
|        5 |  9685 | `					}else{` |
|      ! 0 |  9686 | `						continue;` |
|        - |  9687 | `					}` |
|        4 |  9688 | `				}` |
|        - |  9689 | `				/* Install in the current frame */` |
|       19 |  9690 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|       12 |  9691 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|        7 |  9692 | `			}` |
|        6 |  9693 | `		}` |
|        - |  9694 | `		/* Push arguments in the local frame */` |
|        - |  9695 | `		{` |
|    20037 |  9696 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  9697 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  9698 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    20037 |  9699 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    20037 |  9700 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  9701 | `			/* ============================================================` |
|        - |  9702 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  9703 | `			 *` |
|        - |  9704 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  9705 | `			 * or position, then install them in the frame.` |
|        - |  9706 | `			 * ============================================================ */` |
|       97 |  9707 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       97 |  9708 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       97 |  9709 | `			sxi32 iVariadicIdx = -1;` |
|        - |  9710 | `			sxu32 nNonVariadic;` |
|        - |  9711 | `			sxi32 *aSlot;` |
|        - |  9712 | `			sxu8  *aUsed;` |
|        - |  9713 | `			sxu32 i;` |
|        - |  9714 | `			/* Find variadic parameter index */` |
|      293 |  9715 | `			for( i = 0; i < nFormal; i++ ){` |
|      207 |  9716 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  9717 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  9718 | `					break;` |
|        - |  9719 | `				}` |
|      101 |  9720 | `			}` |
|       97 |  9721 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  9722 | `			/* Allocate mapping arrays */` |
|      144 |  9723 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  9724 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       97 |  9725 | `			if( aSlot == 0 ){` |
|      ! 0 |  9726 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  9727 | `				goto Abort;` |
|        - |  9728 | `			}` |
|       97 |  9729 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  9730 | `			/* Resolve named arguments to formal parameters */` |
|      144 |  9731 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  9732 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       97 |  9733 | `			if( rc == PH7_ABORT ){` |
|        8 |  9734 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 |  9735 | `				goto Abort;` |
|        - |  9736 | `			}` |
|        - |  9737 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  9738 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  9739 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  9740 | `				sxi32 iSrc = -1;` |
|      309 |  9741 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  9742 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  9743 | `						iSrc = (sxi32)i;` |
|      169 |  9744 | `						break;` |
|        - |  9745 | `					}` |
|       62 |  9746 | `				}` |
|      187 |  9747 | `				if( iSrc >= 0 ){` |
|        - |  9748 | `					/* Argument was provided — install with type checking */` |
|      169 |  9749 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  9750 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  9751 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  9752 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  9753 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal,FALSE);` |
|      ! 0 |  9754 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9755 | `					}` |
|        - |  9756 | `					/* Type checking: union types */` |
|      169 |  9757 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  9758 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  9759 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  9760 | `							bCallIsStrict);` |
|       13 |  9761 | `						if( rcU != SXRET_OK ){` |
|        - |  9762 | `							const char *zGiven;` |
|      ! 0 |  9763 | `							const char *zExpected = "union";` |
|        - |  9764 | `							char zBuf[128];` |
|        - |  9765 | `							char zTypeBuf[128];` |
|      ! 0 |  9766 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9767 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  9768 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9769 | `								zGiven = "null";` |
|      ! 0 |  9770 | `							}else{` |
|      ! 0 |  9771 | `								zGiven = ph7_type_name(pVal);` |
|        - |  9772 | `							}` |
|      ! 0 |  9773 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  9774 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  9775 | `							}` |
|      ! 0 |  9776 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9777 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  9778 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9779 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9780 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  9781 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9782 | `							pFrameStack = 0;` |
|      ! 0 |  9783 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  9784 | `							goto SkipFuncBody;` |
|        - |  9785 | `						}` |
|      171 |  9786 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  9787 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  9788 | `						/* Scalar/class type checking */` |
|       17 |  9789 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  9790 | `							SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9791 | `							ph7_class *pClass;` |
|      ! 0 |  9792 | `							int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|      ! 0 |  9793 | `							if( rcPseudo == 0 ){` |
|        - |  9794 | `								/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - |  9795 | `								char zTypeBuf[128],zGivenBuf[128];` |
|      ! 0 |  9796 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9797 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9798 | `									VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  9799 | `									VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      ! 0 |  9800 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9801 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9802 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9803 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9804 | `								pFrameStack = 0;` |
|      ! 0 |  9805 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9806 | `								goto SkipFuncBody;` |
|        - |  9807 | `							}` |
|        - |  9808 | `							/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class */` |
|      ! 0 |  9809 | `							pClass = (rcPseudo == 1) ? 0 : PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  9810 | `							if( pClass ){` |
|      ! 0 |  9811 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9812 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9813 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9814 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9815 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9816 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9817 | `									}` |
|      ! 0 |  9818 | `								}else{` |
|      ! 0 |  9819 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9820 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  9821 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9822 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9823 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9824 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9825 | `									}` |
|        - |  9826 | `								}` |
|      ! 0 |  9827 | `							}` |
|       17 |  9828 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  9829 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  9830 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9831 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  9832 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9833 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9834 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9835 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9836 | `								pFrameStack = 0;` |
|      ! 0 |  9837 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9838 | `								goto SkipFuncBody;` |
|        7 |  9839 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9840 | `								char zTypeBuf[128];` |
|      ! 0 |  9841 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9842 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9843 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9844 | `									ph7_type_name(pVal));` |
|      ! 0 |  9845 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9846 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9847 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9848 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9849 | `								pFrameStack = 0;` |
|      ! 0 |  9850 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9851 | `								goto SkipFuncBody;` |
|        - |  9852 | `							}` |
|        3 |  9853 | `						}` |
|        8 |  9854 | `					}` |
|        - |  9855 | `					/* Install: by reference or by value */` |
|      169 |  9856 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  9857 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9858 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  9859 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9860 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9861 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9862 | `							}` |
|      ! 0 |  9863 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9864 | `						}else{` |
|        7 |  9865 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  9866 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  9867 | `							if( pRefEntry == 0 ){` |
|        7 |  9868 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  9869 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  9870 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  9871 | `								sArg.pUserData = 0;` |
|        5 |  9872 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  9873 | `							}` |
|        5 |  9874 | `							pObj = 0;` |
|        - |  9875 | `						}` |
|        3 |  9876 | `					}else{` |
|      165 |  9877 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9878 | `					}` |
|      169 |  9879 | `					if( pObj ){` |
|      165 |  9880 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  9881 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  9882 | `						sArg.pUserData = 0;` |
|      165 |  9883 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  9884 | `					}` |
|       85 |  9885 | `				}else{` |
|        - |  9886 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  9887 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9888 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  9889 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  9890 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  9891 | `						if( pObj ){` |
|       19 |  9892 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|       19 |  9893 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  9894 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  9895 | `							sArg.pUserData = 0;` |
|       19 |  9896 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9897 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  9898 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9899 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9900 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  9901 | `							}` |
|        9 |  9902 | `						}` |
|        9 |  9903 | `					}` |
|        - |  9904 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  9905 | `				}` |
|       94 |  9906 | `			}` |
|        - |  9907 | `			/* Handle variadic parameter */` |
|       89 |  9908 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  9909 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  9910 | `				if( pObj ){` |
|        9 |  9911 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9912 | `					{` |
|        9 |  9913 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  9914 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  9915 | `							if( aSlot[i] == -1 ){` |
|       16 |  9916 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  9917 | `									/* Named variadic entry: insert with string key */` |
|        - |  9918 | `									ph7_value sKey;` |
|       11 |  9919 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  9920 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  9921 | `										pCallMap3->aNames[i].zString,` |
|       10 |  9922 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  9923 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  9924 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  9925 | `								}else{` |
|        - |  9926 | `									/* Positional variadic entry */` |
|      ! 0 |  9927 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  9928 | `								}` |
|        5 |  9929 | `							}` |
|       12 |  9930 | `						}` |
|        - |  9931 | `					}` |
|        9 |  9932 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  9933 | `					sArg.pUserData = 0;` |
|        9 |  9934 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  9935 | `				}` |
|        5 |  9936 | `			}else{` |
|        - |  9937 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  9938 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  9939 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9940 | `				 * the positional-only path's behavior. */` |
|       81 |  9941 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9942 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9943 | `					if( aSlot[i] == -2 ){` |
|        - |  9944 | `						char zAnonBuf[32];` |
|        - |  9945 | `						SyString sAnonName;` |
|      ! 0 |  9946 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9947 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9948 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9949 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9950 | `						if( pObj ){` |
|      ! 0 |  9951 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9952 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9953 | `							sArg.pUserData = 0;` |
|      ! 0 |  9954 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9955 | `						}` |
|      ! 0 |  9956 | `						nAnon++;` |
|      ! 0 |  9957 | `					}` |
|       79 |  9958 | `				}` |
|        - |  9959 | `			}` |
|        - |  9960 | `			/* Release all stack arguments */` |
|      267 |  9961 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9962 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9963 | `			}` |
|       89 |  9964 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9965 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9966 | `			n = nFormal;` |
|       45 |  9967 | `		}else{` |
|        - |  9968 | `		/* ============================================================` |
|        - |  9969 | `		 * Positional-only matching path (original)` |
|        - |  9970 | `		 * ============================================================ */` |
|    19943 |  9971 | `		n = 0;` |
|    52287 |  9972 | `		while( pArg < pTos ){` |
|    32429 |  9973 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9974 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       45 |  9975 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       45 |  9976 | `				if( pObj ){` |
|        - |  9977 | `					/* Initialize as empty array */` |
|       45 |  9978 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9979 | `					{` |
|       45 |  9980 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      161 |  9981 | `						while( pArg < pTos ){` |
|        - |  9982 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9983 | `							 *` |
|        - |  9984 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9985 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9986 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9987 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9988 | `							 * fixing both wants a separate counter for elements` |
|        - |  9989 | `							 * already packed into the variadic array. */` |
|      123 |  9990 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9991 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9992 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9993 | `									bCallIsStrict);` |
|       16 |  9994 | `								if( rcU != SXRET_OK ){` |
|        - |  9995 | `									const char *zGiven;` |
|        3 |  9996 | `									const char *zExpected = "union";` |
|        - |  9997 | `									char zBuf[128];` |
|        - |  9998 | `									char zTypeBuf[128];` |
|        3 |  9999 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10000 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 | 10001 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 10002 | `										zGiven = "null";` |
|      ! 0 | 10003 | `									}else{` |
|        3 | 10004 | `										zGiven = ph7_type_name(pArg);` |
|        - | 10005 | `									}` |
|        3 | 10006 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 | 10007 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 | 10008 | `									}` |
|        4 | 10009 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 | 10010 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 | 10011 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 10012 | `										goto Abort;` |
|        - | 10013 | `									}` |
|        3 | 10014 | `									PH7_MemObjRelease(pTos);` |
|        3 | 10015 | `									pTos = &pTos[-nCallArgs];` |
|        3 | 10016 | `									pFrameStack = 0;` |
|        3 | 10017 | `									rc = PH7_EXCEPTION;` |
|        3 | 10018 | `									goto SkipFuncBody;` |
|        - | 10019 | `								}` |
|       14 | 10020 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 | 10021 | `								pArg++;` |
|       14 | 10022 | `								continue;` |
|        - | 10023 | `							}` |
|        - | 10024 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - | 10025 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      120 | 10026 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 | 10027 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       44 | 10028 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 | 10029 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - | 10030 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 | 10031 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 | 10032 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 | 10033 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 10034 | `										goto Abort;` |
|        - | 10035 | `									}` |
|        - | 10036 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 | 10037 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 | 10038 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 | 10039 | `									pFrameStack = 0;` |
|      ! 0 | 10040 | `									rc = PH7_EXCEPTION;` |
|      ! 0 | 10041 | `									goto SkipFuncBody;` |
|       13 | 10042 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 10043 | `									char zTypeBuf[128];` |
|      ! 0 | 10044 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 | 10045 | `										&aFormalArg[n].sName,` |
|      ! 0 | 10046 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 | 10047 | `										ph7_type_name(pArg));` |
|      ! 0 | 10048 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 10049 | `										goto Abort;` |
|        - | 10050 | `									}` |
|      ! 0 | 10051 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 | 10052 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 | 10053 | `									pFrameStack = 0;` |
|      ! 0 | 10054 | `									rc = PH7_EXCEPTION;` |
|      ! 0 | 10055 | `									goto SkipFuncBody;` |
|        - | 10056 | `								}` |
|        6 | 10057 | `							}` |
|      109 | 10058 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      109 | 10059 | `							pArg++;` |
|        5 | 10060 | `						}` |
|        - | 10061 | `					}` |
|       43 | 10062 | `					sArg.nIdx = pObj->nIdx;` |
|       43 | 10063 | `					sArg.pUserData = 0;` |
|       43 | 10064 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       19 | 10065 | `				}` |
|       43 | 10066 | `				break; /* All remaining args consumed */` |
|        - | 10067 | `			}` |
|    32389 | 10068 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    32168 | 10069 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       44 | 10070 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - | 10071 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 | 10072 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg,FALSE);` |
|      ! 0 | 10073 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 10074 | `						goto Abort;` |
|        - | 10075 | `					}` |
|      ! 0 | 10076 | `				}` |
|        - | 10077 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    32173 | 10078 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       98 | 10079 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       62 | 10080 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       31 | 10081 | `						bCallIsStrict);` |
|       67 | 10082 | `					if( rcU != SXRET_OK ){` |
|        - | 10083 | `						const char *zGiven;` |
|       22 | 10084 | `						const char *zExpected = "union";` |
|        - | 10085 | `						char zBuf[128];` |
|        - | 10086 | `						char zTypeBuf[128];` |
|       22 | 10087 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        8 | 10088 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       18 | 10089 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|       10 | 10090 | `							zGiven = "null";` |
|        6 | 10091 | `						}else{` |
|        6 | 10092 | `							zGiven = ph7_type_name(pArg);` |
|        - | 10093 | `						}` |
|       22 | 10094 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       22 | 10095 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 | 10096 | `						}` |
|       31 | 10097 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 | 10098 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       22 | 10099 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 10100 | `							goto Abort;` |
|        - | 10101 | `						}` |
|       22 | 10102 | `						PH7_MemObjRelease(pTos);` |
|       22 | 10103 | `						pTos = &pTos[-nCallArgs];` |
|       22 | 10104 | `						pFrameStack = 0;` |
|       22 | 10105 | `						rc = PH7_EXCEPTION;` |
|       22 | 10106 | `						goto SkipFuncBody;` |
|        - | 10107 | `					}` |
|       23 | 10108 | `				}else` |
|        - | 10109 | `				/* Make sure the given arguments are of the correct type.` |
|        - | 10110 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    32136 | 10111 | `				if( aFormalArg[n].nType > 0` |
|    16810 | 10112 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1479 | 10113 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - | 10114 | `						/* Argument must be a class instance [i.e: object] */` |
|       37 | 10115 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - | 10116 | `						ph7_class *pClass;` |
|       37 | 10117 | `						int rcPseudo = VmCheckPseudoType(&(*pVm),pArg,pName);` |
|       37 | 10118 | `						if( rcPseudo == 0 ){` |
|        - | 10119 | `							/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - | 10120 | `							char zTypeBuf[128],zGivenBuf[128];` |
|        7 | 10121 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 | 10122 | `								&aFormalArg[n].sName,` |
|        2 | 10123 | `								VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 | 10124 | `								VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf)));` |
|        5 | 10125 | `							if( rc == PH7_ABORT ) goto Abort;` |
|        5 | 10126 | `							PH7_MemObjRelease(pTos);` |
|        5 | 10127 | `							pTos = &pTos[-nCallArgs];` |
|        5 | 10128 | `							pFrameStack = 0;` |
|        5 | 10129 | `							rc = PH7_EXCEPTION;` |
|        5 | 10130 | `							goto SkipFuncBody;` |
|        - | 10131 | `						}` |
|        - | 10132 | `						/* Try to extract the desired class (rcPseudo==1 accepts; -1 real class) */` |
|       33 | 10133 | `						pClass = (rcPseudo == 1) ? 0 : PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       33 | 10134 | `						if( pClass ){` |
|       23 | 10135 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10136 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 | 10137 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 10138 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 | 10139 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 | 10140 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 | 10141 | `								}` |
|      ! 0 | 10142 | `							}else{` |
|        - | 10143 | `								/* reuse pThis declared in outer scope */` |
|       23 | 10144 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - | 10145 | `								/* Make sure the object is an instance of the given class */` |
|       23 | 10146 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 | 10147 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 10148 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 | 10149 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 | 10150 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 | 10151 | `								}` |
|        - | 10152 | `							}` |
|       13 | 10153 | `						}` |
|     1460 | 10154 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       30 | 10155 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - | 10156 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 | 10157 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 | 10158 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 | 10159 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 10160 | `								goto Abort;` |
|        - | 10161 | `							}` |
|        - | 10162 | `							/* Skip function body, route through normal cleanup */` |
|       11 | 10163 | `							PH7_MemObjRelease(pTos);` |
|       11 | 10164 | `							pTos = &pTos[-nCallArgs];` |
|       11 | 10165 | `							pFrameStack = 0;` |
|       11 | 10166 | `							rc = PH7_EXCEPTION;` |
|       11 | 10167 | `							goto SkipFuncBody;` |
|       19 | 10168 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 10169 | `							char zTypeBuf[128];` |
|       15 | 10170 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        8 | 10171 | `								&aFormalArg[n].sName,` |
|        8 | 10172 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        4 | 10173 | `								ph7_type_name(pArg));` |
|       11 | 10174 | `							if( rc == PH7_ABORT ){` |
|        6 | 10175 | `								goto Abort;` |
|        - | 10176 | `							}` |
|        5 | 10177 | `							PH7_MemObjRelease(pTos);` |
|        5 | 10178 | `							pTos = &pTos[-nCallArgs];` |
|        5 | 10179 | `							pFrameStack = 0;` |
|        5 | 10180 | `							rc = PH7_EXCEPTION;` |
|        5 | 10181 | `							goto SkipFuncBody;` |
|        - | 10182 | `						}` |
|        4 | 10183 | `					}` |
|      726 | 10184 | `				}` |
|    32133 | 10185 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 10186 | `					/* Pass by reference */` |
|       59 | 10187 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - | 10188 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 | 10189 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 | 10190 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 10191 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 | 10192 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 | 10193 | `						}` |
|        - | 10194 | `						/* Switch to pass by value */` |
|      ! 0 | 10195 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 10196 | `					}else{` |
|        - | 10197 | `						SyHashEntry *pRefEntry;` |
|        - | 10198 | `						/* Install the referenced variable in the private function frame */` |
|       59 | 10199 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       59 | 10200 | `						if( pRefEntry == 0 ){` |
|       87 | 10201 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 | 10202 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       59 | 10203 | `							sArg.nIdx = pArg->nIdx;` |
|       59 | 10204 | `							sArg.pUserData = 0;` |
|       59 | 10205 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 | 10206 | `						}` |
|       59 | 10207 | `						pObj = 0;` |
|        - | 10208 | `					}` |
|       31 | 10209 | `				}else{` |
|        - | 10210 | `					/* Pass by value,make a copy of the given argument */` |
|    32077 | 10211 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 10212 | `				}` |
|    16069 | 10213 | `			}else{` |
|        - | 10214 | `				char zName[32];` |
|        - | 10215 | `				SyString sArgName;` |
|        - | 10216 | `				/* Set a dummy name */` |
|      220 | 10217 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      220 | 10218 | `				sArgName.zString = zName;` |
|        - | 10219 | `				/* Annonymous argument */` |
|      220 | 10220 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 10221 | `			}` |
|    32349 | 10222 | `			if( pObj ){` |
|    32293 | 10223 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 10224 | `				/* Insert argument index  */` |
|    32293 | 10225 | `				sArg.nIdx = pObj->nIdx;` |
|    32293 | 10226 | `				sArg.pUserData = 0;` |
|    32293 | 10227 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    16144 | 10228 | `			}` |
|    32349 | 10229 | `			PH7_MemObjRelease(pArg);` |
|    32349 | 10230 | `			pArg++;` |
|    32349 | 10231 | `			++n;` |
|        5 | 10232 | `		}` |
|        - | 10233 | `		} /* end named vs positional branch */` |
|        - | 10234 | `		/* Set up closure environment */` |
|    19989 | 10235 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10236 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 10237 | `			ph7_value *pValue;` |
|        - | 10238 | `			sxu32 iEnv;` |
|      184 | 10239 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      434 | 10240 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      252 | 10241 | `				pEnv = &aEnv[iEnv];` |
|      252 | 10242 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 10243 | `					/* Do not install null value */` |
|      178 | 10244 | `					continue;` |
|        - | 10245 | `				}` |
|       76 | 10246 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 | 10247 | `				if( pValue == 0 ){` |
|      ! 0 | 10248 | `					continue;` |
|        - | 10249 | `				}` |
|        - | 10250 | `				/* Invalidate any prior representation */` |
|       76 | 10251 | `				PH7_MemObjRelease(pValue);` |
|        - | 10252 | `				/* Duplicate bound variable value */` |
|       76 | 10253 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 | 10254 | `			}` |
|       91 | 10255 | `		}` |
|        - | 10256 | `		/* Process default values for remaining formal parameters */` |
|    23125 | 10257 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     3189 | 10258 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 10259 | `				/* Variadic parameter with no extra args — create empty array */` |
|       53 | 10260 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       53 | 10261 | `				if( pObj ){` |
|       53 | 10262 | `					PH7_MemObjToHashmap(pObj);` |
|       53 | 10263 | `					sArg.nIdx = pObj->nIdx;` |
|       53 | 10264 | `					sArg.pUserData = 0;` |
|       53 | 10265 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       24 | 10266 | `				}` |
|       53 | 10267 | `				n++;` |
|       53 | 10268 | `				break; /* Variadic is always last */` |
|        - | 10269 | `			}` |
|     3141 | 10270 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     3135 | 10271 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     3135 | 10272 | `				if( pObj ){` |
|        - | 10273 | `					/* Evaluate the default value and extract it's result */` |
|     3135 | 10274 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|     3135 | 10275 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 10276 | `						goto Abort;` |
|        - | 10277 | `					}` |
|        - | 10278 | `					/* Insert argument index */` |
|     3135 | 10279 | `					sArg.nIdx = pObj->nIdx;` |
|     3135 | 10280 | `					sArg.pUserData = 0;` |
|     3135 | 10281 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 10282 | `					/* Make sure the default argument is of the correct type */` |
|     3130 | 10283 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     2003 | 10284 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 | 10285 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 10286 | `						/* Cast to the desired type */` |
|        3 | 10287 | `						xCast(pObj);` |
|        1 | 10288 | `					}` |
|     1565 | 10289 | `				}` |
|     1565 | 10290 | `			}` |
|     3141 | 10291 | `			++n;` |
|        5 | 10292 | `		}` |
|        - | 10293 | `		} /* end VmCallArgMap scope */` |
|        - | 10294 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 10295 | `		 * does not return anything.` |
|        - | 10296 | `		 */` |
|    19989 | 10297 | `		PH7_MemObjRelease(pTos);` |
|    19989 | 10298 | `		pTos = &pTos[-nCallArgs];` |
|        - | 10299 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    19989 | 10300 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    19989 | 10301 | `		if( pFrameStack == 0 ){` |
|        - | 10302 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 10303 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 10304 | `				&pVmFunc->sName);` |
|      ! 0 | 10305 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 10306 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 10307 | `			}` |
|      ! 0 | 10308 | `			break;` |
|        - | 10309 | `		}` |
|     9992 | 10310 | `SkipFuncBody:` |
|    20027 | 10311 | `		if( pSelf ){` |
|        - | 10312 | `			/* Push class name */` |
|     3879 | 10313 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1937 | 10314 | `		}` |
|        - | 10315 | `		/* Increment nesting level */` |
|    20027 | 10316 | `		pVm->nRecursionDepth++;` |
|    20027 | 10317 | `		if( rc != PH7_EXCEPTION ){` |
|        - | 10318 | `			/* Execute function body */` |
|    29981 | 10319 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    19984 | 10320 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0, FALSE);` |
|     9992 | 10321 | `		}` |
|        - | 10322 | `		/* Decrement nesting level */` |
|    20027 | 10323 | `		pVm->nRecursionDepth--;` |
|    20027 | 10324 | `		if( pSelf ){` |
|        - | 10325 | `			/* Pop class name */` |
|     3879 | 10326 | `			(void)SySetPop(&pVm->aSelf);` |
|     1937 | 10327 | `		}` |
|        - | 10328 | `		/* Cleanup the mess left behind */` |
|    20027 | 10329 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - | 10330 | `			/* Return by reference,reflect that */` |
|        9 | 10331 | `			if( n != SXU32_HIGH ){` |
|        9 | 10332 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - | 10333 | `				sxu32 i;` |
|        - | 10334 | `				/* Make sure the referenced object is not a local variable */` |
|       13 | 10335 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 | 10336 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 | 10337 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 | 10338 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 | 10339 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - | 10340 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 | 10341 | `								&pVmFunc->sName);` |
|      ! 0 | 10342 | `						}` |
|      ! 0 | 10343 | `						n = SXU32_HIGH;` |
|      ! 0 | 10344 | `						break;` |
|        - | 10345 | `					}` |
|        3 | 10346 | `				}` |
|        5 | 10347 | `			}else{` |
|      ! 0 | 10348 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 | 10349 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - | 10350 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 | 10351 | `						&pVmFunc->sName);` |
|      ! 0 | 10352 | `				}` |
|        - | 10353 | `			}` |
|        9 | 10354 | `			pTos->nIdx = n;` |
|        4 | 10355 | `		}` |
|        - | 10356 | `		/* Cleanup the mess left behind */` |
|    20027 | 10357 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - | 10358 | `			/* An exception was throw in this frame */` |
|      121 | 10359 | `			pFrame = pFrame->pParent;` |
|      121 | 10360 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - | 10361 | `				/* Pop the resutlt */` |
|       77 | 10362 | `				VmPopOperand(&pTos,1);` |
|        - | 10363 | `				/* Jump to this destination */` |
|       77 | 10364 | `				pc = pFrame->iExceptionJump - 1;` |
|       77 | 10365 | `				rc = PH7_OK;` |
|       41 | 10366 | `			}else{` |
|       45 | 10367 | `				if( pFrame->pParent ){` |
|       43 | 10368 | `					rc = PH7_EXCEPTION;` |
|       22 | 10369 | `				}else{` |
|        - | 10370 | `					/* Continue normal execution */` |
|        3 | 10371 | `					rc = PH7_OK;` |
|        - | 10372 | `				}` |
|        - | 10373 | `			}` |
|       58 | 10374 | `		}` |
|        - | 10375 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    20027 | 10376 | `		if( pFrameStack ){` |
|    19989 | 10377 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9992 | 10378 | `		}` |
|        - | 10379 | `		/* Leave the frame */` |
|    20027 | 10380 | `		VmLeaveFrame(&(*pVm));` |
|    20027 | 10381 | `		if( rc == PH7_ABORT ){` |
|        - | 10382 | `			/* Abort processing immeditaley */` |
|      126 | 10383 | `			goto Abort;` |
|    19905 | 10384 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 10385 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - | 10386 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - | 10387 | `			 * overwriting the state saved by the inner level.` |
|        - | 10388 | `			 * pTos points to the result slot (not yet written).` |
|        - | 10389 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       43 | 10390 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       43 | 10391 | `			goto Suspend;` |
|    19867 | 10392 | `		}else if( rc == PH7_EXCEPTION ){` |
|       43 | 10393 | `			goto Exception;` |
|        - | 10394 | `		}` |
|     9915 | 10395 | `	}else{` |
|        - | 10396 | `		ph7_user_func *pFunc;` |
|        - | 10397 | `		ph7_context sCtx;` |
|        - | 10398 | `		ph7_value sRet;` |
|        - | 10399 | `		/* Look for an installed foreign function.` |
|        - | 10400 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 10401 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 10402 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 10403 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   718830 | 10404 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 10405 | `		{` |
|   718830 | 10406 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   718830 | 10407 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 10408 | `			/* Compiler-qualified: try short name as global fallback */` |
|       24 | 10409 | `			const char *zShort = sName.zString;` |
|        - | 10410 | `			sxu32 i;` |
|      336 | 10411 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      316 | 10412 | `				if( sName.zString[i] == '\\' ){` |
|       30 | 10413 | `					zShort = &sName.zString[i + 1];` |
|       13 | 10414 | `				}` |
|      160 | 10415 | `			}` |
|       24 | 10416 | `			if( zShort != sName.zString ){` |
|       24 | 10417 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       24 | 10418 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 | 10419 | `			}` |
|       10 | 10420 | `		}` |
|        - | 10421 | `		} /* end VmCallArgMap namespace scope */` |
|   718830 | 10422 | `		if( pEntry == 0 ){` |
|        - | 10423 | `			/* Call to undefined function */` |
|        6 | 10424 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - | 10425 | `			/* Pop given arguments */` |
|        6 | 10426 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 | 10427 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 | 10428 | `			}` |
|        - | 10429 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        6 | 10430 | `			PH7_MemObjRelease(pTos);` |
|       61 | 10431 | `			break;` |
|        - | 10432 | `		}` |
|   718826 | 10433 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 10434 | `		/* Start collecting function arguments */` |
|   718826 | 10435 | `		SySetReset(&aArg);` |
|  1938805 | 10436 | `		while( pArg < pTos ){` |
|  1219984 | 10437 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1219984 | 10438 | `			pArg++;` |
|        5 | 10439 | `		}` |
|        - | 10440 | `		/* Assume a null return value */` |
|   718826 | 10441 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 10442 | `		/* Init the call context */` |
|   718826 | 10443 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 10444 | `		/* Call the foreign function */` |
|   718826 | 10445 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - | 10446 | `		/* Release the call context */` |
|   718826 | 10447 | `		VmReleaseCallContext(&sCtx);` |
|   718826 | 10448 | `		if( rc == PH7_ABORT ){` |
|        - | 10449 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 10450 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 10451 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      548 | 10452 | `			PH7_MemObjRelease(&sRet);` |
|      548 | 10453 | `			goto Abort;` |
|   718282 | 10454 | `		}else if( rc == PH7_EXCEPTION ){` |
|      118 | 10455 | `			VmFrame *pFrm = pVm->pFrame;` |
|      118 | 10456 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      118 | 10457 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - | 10458 | `				/* Exception was NOT caught, propagate */` |
|        6 | 10459 | `				goto Exception;` |
|        - | 10460 | `			}` |
|        - | 10461 | `			/* Exception was caught: pop args and the result slot */` |
|      113 | 10462 | `			PH7_MemObjRelease(&sRet);` |
|      113 | 10463 | `			if( pInstr->iP1 > 0 ){` |
|       97 | 10464 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       47 | 10465 | `			}` |
|        - | 10466 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      113 | 10467 | `			VmPopOperand(&pTos,1);` |
|        - | 10468 | `			/* Jump past the try/catch block via the exception frame */` |
|      113 | 10469 | `			pFrm = pVm->pFrame;` |
|      113 | 10470 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      113 | 10471 | `				pc = pFrm->iExceptionJump - 1;` |
|       55 | 10472 | `			}` |
|      113 | 10473 | `			break;` |
|        - | 10474 | `		}` |
|   718168 | 10475 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 10476 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 10477 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 10478 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 10479 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 10480 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 10481 | `			 * body), the user-function path above will handle re-saving. */` |
|       43 | 10482 | `			PH7_MemObjRelease(&sRet);` |
|       43 | 10483 | `			if( pInstr->iP1 > 0 ){` |
|       43 | 10484 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 | 10485 | `			}` |
|        - | 10486 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 10487 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       43 | 10488 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       43 | 10489 | `			goto Suspend;` |
|        - | 10490 | `		}` |
|   718130 | 10491 | `		if( pInstr->iP1 > 0 ){` |
|        - | 10492 | `			/* Pop function name and arguments */` |
|   695514 | 10493 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   347797 | 10494 | `		}` |
|        - | 10495 | `		/* Save foreign function return value */` |
|   718130 | 10496 | `		PH7_MemObjStore(&sRet,pTos);` |
|   718130 | 10497 | `		PH7_MemObjRelease(&sRet);` |
|        - | 10498 | `	}` |
|   737950 | 10499 | `	break;` |
|        - | 10500 | `				  }` |
|        - | 10501 | `/*` |
|        - | 10502 | ` * OP_CONSUME: P1 * *` |
|        - | 10503 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 10504 | ` */` |
|    16631 | 10505 | `case PH7_OP_CONSUME: {` |
|    33267 | 10506 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    33267 | 10507 | `	ph7_value *pCur,*pOut = pTos;` |
|        - | 10508 |  |
|    33267 | 10509 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    33267 | 10510 | `	pCur = pOut;` |
|        - | 10511 | `	/* Start the consume process  */` |
|    66571 | 10512 | `	while( pOut <= pTos ){` |
|        - | 10513 | `		/* Force a string cast */` |
|    33309 | 10514 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1157 | 10515 | `			PH7_MemObjToString(pOut);` |
|      576 | 10516 | `		}` |
|    33309 | 10517 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - | 10518 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - | 10519 | `			/* Invoke the output consumer callback */` |
|    20535 | 10520 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    20535 | 10521 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    20535 | 10522 | `			SyBlobRelease(&pOut->sBlob);` |
|    20535 | 10523 | `			if( rc == SXERR_ABORT ){` |
|        - | 10524 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 | 10525 | `				goto Abort;` |
|        - | 10526 | `			}` |
|    10265 | 10527 | `		}` |
|    33309 | 10528 | `		pOut++;` |
|        5 | 10529 | `	}` |
|    33267 | 10530 | `	pTos = &pCur[-1];` |
|    33262 | 10531 | `	break;` |
|        - | 10532 | `					 }` |
|        - | 10533 |  |
|        - | 10534 | `		} /* Switch() */` |
| 12183763 | 10535 | `		pc++; /* Next instruction in the stream */` |
|        5 | 10536 | `	} /* For(;;) */` |
|    24772 | 10537 | `Done:` |
|    49549 | 10538 | `	SySetRelease(&aArg);` |
|    49549 | 10539 | `	return SXRET_OK;` |
|      100 | 10540 | `Suspend:` |
|      205 | 10541 | `	SySetRelease(&aArg);` |
|      205 | 10542 | `	return PH7_SUSPEND;` |
|      366 | 10543 | `Abort:` |
|      736 | 10544 | `	SySetRelease(&aArg);` |
|     2286 | 10545 | `	while( pTos >= pStack ){` |
|     1554 | 10546 | `		PH7_MemObjRelease(pTos);` |
|     1554 | 10547 | `		pTos--;` |
|        4 | 10548 | `	}` |
|      736 | 10549 | `	return PH7_ABORT;` |
|       34 | 10550 | `Exception:` |
|       71 | 10551 | `	SySetRelease(&aArg);` |
|      127 | 10552 | `	while( pTos >= pStack ){` |
|       59 | 10553 | `		PH7_MemObjRelease(pTos);` |
|       59 | 10554 | `		pTos--;` |
|        3 | 10555 | `	}` |
|       71 | 10556 | `	return PH7_EXCEPTION;` |
|    25277 | 10557 |  |
|        - | 10558 | `/*` |
|        - | 10559 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 10560 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10561 | ` * See block-comment on that function for additional information.` |
|        - | 10562 | ` */` |
|    23782 | 10563 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|        5 | 10564 |  |
|        - | 10565 | `	ph7_value *pStack;` |
|        - | 10566 | `	sxi32 rc;` |
|        - | 10567 | `	/* Allocate a new operand stack */` |
|    23787 | 10568 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    23787 | 10569 | `	if( pStack == 0 ){` |
|      ! 0 | 10570 | `		return SXERR_MEM;` |
|        - | 10571 | `	}` |
|        - | 10572 | `	/* Execute the program */` |
|    23787 | 10573 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates);` |
|        - | 10574 | `	/* Free the operand stack */` |
|    23787 | 10575 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 10576 | `	/* Execution result */` |
|    23787 | 10577 | `	return rc;` |
|    11896 | 10578 |  |
|        - | 10579 | `/*` |
|        - | 10580 | ` * Invoke any installed shutdown callbacks.` |
|        - | 10581 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 10582 | ` * or more calls to [register_shutdown_function()].` |
|        - | 10583 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 10584 | ` * execution ends.` |
|        - | 10585 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 10586 | ` * additional information.` |
|        - | 10587 | ` */` |
|     2966 | 10588 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        5 | 10589 |  |
|        - | 10590 | `	VmShutdownCB *pEntry;` |
|        - | 10591 | `	ph7_value *apArg[10];` |
|        - | 10592 | `	sxu32 n,nEntry;` |
|        - | 10593 | `	int i;` |
|        - | 10594 | `	/* Point to the stack of registered callbacks */` |
|     2971 | 10595 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    32631 | 10596 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    29665 | 10597 | `		apArg[i] = 0;` |
|    14835 | 10598 | `	}` |
|        - | 10599 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 10600 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 10601 | `	 * callbacks, mirroring PHP.` |
|        - | 10602 | `	 */` |
|     2971 | 10603 | `	pVm->bHaltRequested = 0;` |
|     2983 | 10604 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       17 | 10605 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       17 | 10606 | `		if( pEntry ){` |
|        - | 10607 | `			/* Prepare callback arguments if any */` |
|       17 | 10608 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 10609 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 10610 | `					break;` |
|        - | 10611 | `				}` |
|      ! 0 | 10612 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 10613 | `			}` |
|        - | 10614 | `			/* Invoke the callback */` |
|       17 | 10615 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 10616 | `			/*` |
|        - | 10617 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 10618 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 10619 | `			 */` |
|       17 | 10620 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       17 | 10621 | `			if( pEntry ){` |
|       17 | 10622 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       17 | 10623 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 10624 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 10625 | `				}` |
|        6 | 10626 | `			}` |
|       17 | 10627 | `			if( pVm->bHaltRequested ){` |
|        - | 10628 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 10629 | `				break;` |
|        - | 10630 | `			}` |
|        6 | 10631 | `		}` |
|       11 | 10632 | `	}` |
|     2971 | 10633 | `	SySetReset(&pVm->aShutdown);` |
|     2971 | 10634 |  |
|        - | 10635 | `/*` |
|        - | 10636 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 10637 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10638 | ` * See block-comment on that function for additional information.` |
|        - | 10639 | ` */` |
|     2966 | 10640 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        5 | 10641 |  |
|        - | 10642 | `	/* Make sure we are ready to execute this program */` |
|     2971 | 10643 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 10644 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 10645 | `	}` |
|        - | 10646 | `	/* Set the execution magic number  */` |
|     2971 | 10647 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 10648 | `	/* Execute the program */` |
|     2971 | 10649 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE);` |
|        - | 10650 | `	/* Invoke any shutdown callbacks */` |
|     2971 | 10651 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 10652 | `	/*` |
|        - | 10653 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 10654 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 10655 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 10656 | `	 */` |
|     2971 | 10657 | `	return SXRET_OK;` |
|     1488 | 10658 |  |
|        - | 10659 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 10660 | `/*` |
|        - | 10661 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - | 10662 | ` * The context is in CREATED state and ready to be started.` |
|        - | 10663 | ` */` |
|       72 | 10664 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        5 | 10665 |  |
|        - | 10666 | `	ph7_exec_ctx *pCtx;` |
|        - | 10667 | `	ph7_value *pStack;` |
|        - | 10668 | `	VmFrame *pFrame;` |
|       77 | 10669 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       77 | 10670 | `	if( pCtx == 0 ){` |
|      ! 0 | 10671 | `		return 0;` |
|        - | 10672 | `	}` |
|       77 | 10673 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       77 | 10674 | `	pCtx->pVm = pVm;` |
|       77 | 10675 | `	pCtx->pFunc = pFunc;` |
|       77 | 10676 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       77 | 10677 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       77 | 10678 | `	pCtx->pc = 0;` |
|       77 | 10679 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       77 | 10680 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - | 10681 | `	/* Allocate a private operand stack */` |
|       77 | 10682 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       77 | 10683 | `	if( pStack == 0 ){` |
|      ! 0 | 10684 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10685 | `		return 0;` |
|        - | 10686 | `	}` |
|       77 | 10687 | `	pCtx->pStack = pStack;` |
|        - | 10688 | `	/* Create a detached frame for the fiber */` |
|       77 | 10689 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       77 | 10690 | `	if( pFrame == 0 ){` |
|      ! 0 | 10691 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 | 10692 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10693 | `		return 0;` |
|        - | 10694 | `	}` |
|       77 | 10695 | `	pCtx->pFrame = pFrame;` |
|       77 | 10696 | `	return pCtx;` |
|       41 | 10697 |  |
|        - | 10698 | `/*` |
|        - | 10699 | ` * Start executing a fiber context for the first time.` |
|        - | 10700 | ` */` |
|       68 | 10701 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        5 | 10702 |  |
|        - | 10703 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10704 | `	sxi32 rc;` |
|       73 | 10705 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10706 | `		return SXERR_INVALID;` |
|        - | 10707 | `	}` |
|        - | 10708 | `	/* Bound fiber/generator nesting under the same cap (each start adds a C` |
|        - | 10709 | `	 * frame); reject before mutating VM state so the abort is clean. */` |
|       73 | 10710 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10711 | `		return VmRecursionFatal(pVm);` |
|        - | 10712 | `	}` |
|        - | 10713 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       73 | 10714 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       73 | 10715 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10716 | `	/* Save and set the active context */` |
|       73 | 10717 | `	pOldCtx = pVm->pActiveCtx;` |
|       73 | 10718 | `	pVm->pActiveCtx = pCtx;` |
|       73 | 10719 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       73 | 10720 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       73 | 10721 | `	pVm->nRecursionDepth++;` |
|        - | 10722 | `	/* Execute from the beginning */` |
|       73 | 10723 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       34 | 10724 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       68 | 10725 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0, FALSE);` |
|       73 | 10726 | `	pVm->nRecursionDepth--;` |
|        - | 10727 | `	/* Restore the previous context */` |
|       73 | 10728 | `	pVm->pActiveCtx = pOldCtx;` |
|       73 | 10729 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10730 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       69 | 10731 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       69 | 10732 | `		pCtx->pFrame->pParent = 0;` |
|       69 | 10733 | `		if( pResult ){` |
|       27 | 10734 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 | 10735 | `		}` |
|       69 | 10736 | `		return SXRET_OK;` |
|        - | 10737 | `	}` |
|        - | 10738 | `	/* Detach frame */` |
|        6 | 10739 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        6 | 10740 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        6 | 10741 | `		pCtx->pFrame->pParent = 0;` |
|        2 | 10742 | `	}` |
|        6 | 10743 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10744 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10745 | `		return PH7_ABORT;` |
|        - | 10746 | `	}` |
|        6 | 10747 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10748 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10749 | `		return PH7_EXCEPTION;` |
|        - | 10750 | `	}` |
|        - | 10751 | `	/* Normal completion */` |
|        6 | 10752 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        6 | 10753 | `	if( pResult ){` |
|        3 | 10754 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 | 10755 | `	}` |
|        6 | 10756 | `	return SXRET_OK;` |
|       39 | 10757 |  |
|        - | 10758 | `/*` |
|        - | 10759 | ` * Resume a suspended fiber context.` |
|        - | 10760 | ` */` |
|      150 | 10761 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        5 | 10762 |  |
|        - | 10763 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10764 | `	sxi32 rc;` |
|      155 | 10765 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 | 10766 | `		return SXERR_INVALID;` |
|        - | 10767 | `	}` |
|        - | 10768 | `	/* Bound fiber/generator nesting under the same cap; reject before mutating` |
|        - | 10769 | `	 * VM state so the abort is clean. */` |
|      155 | 10770 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10771 | `		return VmRecursionFatal(pVm);` |
|        - | 10772 | `	}` |
|        - | 10773 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - | 10774 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - | 10775 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      155 | 10776 | `	if( pResumeValue ){` |
|       43 | 10777 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       24 | 10778 | `	}else{` |
|      117 | 10779 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - | 10780 | `	}` |
|      155 | 10781 | `	pCtx->nTos++;` |
|        - | 10782 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      155 | 10783 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      155 | 10784 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10785 | `	/* Save and set the active context */` |
|      155 | 10786 | `	pOldCtx = pVm->pActiveCtx;` |
|      155 | 10787 | `	pVm->pActiveCtx = pCtx;` |
|      155 | 10788 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      155 | 10789 | `	pVm->nRecursionDepth++;` |
|        - | 10790 | `	/* Resume execution from saved PC */` |
|      155 | 10791 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       75 | 10792 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|      150 | 10793 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0, FALSE);` |
|      155 | 10794 | `	pVm->nRecursionDepth--;` |
|        - | 10795 | `	/* Restore the previous context */` |
|      155 | 10796 | `	pVm->pActiveCtx = pOldCtx;` |
|      155 | 10797 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10798 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|      103 | 10799 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|      103 | 10800 | `		pCtx->pFrame->pParent = 0;` |
|      103 | 10801 | `		if( pResult ){` |
|       20 | 10802 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 | 10803 | `		}` |
|      103 | 10804 | `		return SXRET_OK;` |
|        - | 10805 | `	}` |
|        - | 10806 | `	/* Detach frame */` |
|       57 | 10807 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       57 | 10808 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       57 | 10809 | `		pCtx->pFrame->pParent = 0;` |
|       26 | 10810 | `	}` |
|       57 | 10811 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10812 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10813 | `		return PH7_ABORT;` |
|        - | 10814 | `	}` |
|       57 | 10815 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10816 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10817 | `		return PH7_EXCEPTION;` |
|        - | 10818 | `	}` |
|        - | 10819 | `	/* Normal completion */` |
|       57 | 10820 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       57 | 10821 | `	if( pResult ){` |
|       23 | 10822 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 | 10823 | `	}` |
|       57 | 10824 | `	return SXRET_OK;` |
|       80 | 10825 |  |
|        - | 10826 | `/*` |
|        - | 10827 | ` * Release an execution context and all its resources.` |
|        - | 10828 | ` */` |
|        4 | 10829 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 | 10830 |  |
|        5 | 10831 | `	if( pCtx == 0 ){` |
|      ! 0 | 10832 | `		return;` |
|        - | 10833 | `	}` |
|        5 | 10834 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - | 10835 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 | 10836 | `		return;` |
|        - | 10837 | `	}` |
|        5 | 10838 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - | 10839 | `	/* Release values */` |
|        5 | 10840 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 | 10841 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - | 10842 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 | 10843 | `	if( pCtx->pFrame ){` |
|        - | 10844 | `		VmSlot *aSlot;` |
|        - | 10845 | `		sxu32 n;` |
|        - | 10846 | `		/* Free local variables */` |
|        5 | 10847 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 | 10848 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 | 10849 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 | 10850 | `		}` |
|        - | 10851 | `		/* Remove local references */` |
|        5 | 10852 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 | 10853 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 | 10854 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 | 10855 | `		}` |
|        5 | 10856 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 | 10857 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 | 10858 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 | 10859 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 | 10860 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 | 10861 | `		pCtx->pFrame = 0;` |
|        2 | 10862 | `	}` |
|        - | 10863 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - | 10864 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - | 10865 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 | 10866 | `	if( pCtx->pStack ){` |
|        5 | 10867 | `		if( pCtx->nTos >= 0 ){` |
|        5 | 10868 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 | 10869 | `			while( pTos >= pCtx->pStack ){` |
|        5 | 10870 | `				PH7_MemObjRelease(pTos);` |
|        5 | 10871 | `				pTos--;` |
|        1 | 10872 | `			}` |
|        2 | 10873 | `		}` |
|        5 | 10874 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 | 10875 | `		pCtx->pStack = 0;` |
|        2 | 10876 | `	}` |
|        - | 10877 | `	/* Free the context itself */` |
|        5 | 10878 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 | 10879 |  |
|        - | 10880 | `/*` |
|        - | 10881 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - | 10882 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - | 10883 | ` */` |
|       90 | 10884 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        5 | 10885 |  |
|        - | 10886 | `	ph7_class_instance *pThis;` |
|        - | 10887 | `	SyString sAttr;` |
|        - | 10888 | `	ph7_value *pAttr;` |
|       95 | 10889 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10890 | `		return 0;` |
|        - | 10891 | `	}` |
|       95 | 10892 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       95 | 10893 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 | 10894 | `		return 0;` |
|        - | 10895 | `	}` |
|       95 | 10896 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       95 | 10897 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       95 | 10898 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       35 | 10899 | `		return 0;` |
|        - | 10900 | `	}` |
|       65 | 10901 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       50 | 10902 |  |
|        - | 10903 | `/*` |
|        - | 10904 | ` * Fiber::suspend($value = null) — static method.` |
|        - | 10905 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - | 10906 | ` */` |
|       38 | 10907 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 10908 |  |
|       43 | 10909 | `	ph7_vm *pVm = pCtx->pVm;` |
|       43 | 10910 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 | 10911 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10912 | `			"Cannot suspend outside of a fiber");` |
|        - | 10913 | `	}` |
|       43 | 10914 | `	if( nArg > 0 ){` |
|       43 | 10915 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       24 | 10916 | `	}else{` |
|      ! 0 | 10917 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - | 10918 | `	}` |
|       43 | 10919 | `	return PH7_SUSPEND;` |
|       24 | 10920 |  |
|        - | 10921 | `/*` |
|        - | 10922 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - | 10923 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - | 10924 | ` * and closure-environment binding happen with the correct argument context.` |
|        - | 10925 | ` */` |
|       24 | 10926 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 10927 |  |
|        - | 10928 | `	ph7_class_instance *pThis;` |
|        - | 10929 | `	ph7_value *pAttr;` |
|        - | 10930 | `	SyString sAttrName;` |
|       29 | 10931 | `	if( nArg < 2 ){` |
|      ! 0 | 10932 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10933 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 10934 | `	}` |
|       29 | 10935 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10936 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10937 | `			"Fiber::__construct(): invalid $this");` |
|        - | 10938 | `	}` |
|       29 | 10939 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       29 | 10940 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 10941 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10942 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 10943 | `	}` |
|        - | 10944 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       29 | 10945 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10946 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10947 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - | 10948 | `	}` |
|        - | 10949 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       29 | 10950 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       29 | 10951 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       29 | 10952 | `	if( pAttr ){` |
|       29 | 10953 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 | 10954 | `	}` |
|       29 | 10955 | `	return PH7_OK;` |
|       17 | 10956 |  |
|        - | 10957 | `/*` |
|        - | 10958 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 10959 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 10960 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 10961 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 10962 | ` */` |
|       24 | 10963 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 10964 | `	ph7_class_instance **ppThis)` |
|        5 | 10965 |  |
|       29 | 10966 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10967 | `	ph7_value *pCallable;` |
|        - | 10968 | `	SyString sAttrName;` |
|       29 | 10969 | `	*ppThis = 0;` |
|       29 | 10970 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       29 | 10971 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       29 | 10972 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10973 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10974 | `		return 0;` |
|        - | 10975 | `	}` |
|       29 | 10976 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10977 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10978 | `		SyString sName;` |
|        - | 10979 | `		SyHashEntry *pEntry;` |
|        - | 10980 | `		ph7_vm_func *pFunc;` |
|       29 | 10981 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       29 | 10982 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       29 | 10983 | `		if( pEntry == 0 ){` |
|      ! 0 | 10984 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10985 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10986 | `			return 0;` |
|        - | 10987 | `		}` |
|       29 | 10988 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       29 | 10989 | `		return pFunc;` |
|      ! 0 | 10990 | `	}else{` |
|        - | 10991 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10992 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10993 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10994 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10995 | `		if( pMethod == 0 ){` |
|      ! 0 | 10996 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10997 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10998 | `			return 0;` |
|        - | 10999 | `		}` |
|      ! 0 | 11000 | `		*ppThis = pClosure;` |
|      ! 0 | 11001 | `		return &pMethod->sFunc;` |
|        - | 11002 | `	}` |
|       17 | 11003 |  |
|        - | 11004 | `/*` |
|        - | 11005 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 11006 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 11007 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 11008 | ` */` |
|       72 | 11009 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 11010 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        5 | 11011 |  |
|       77 | 11012 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 11013 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 11014 | `	sxu32 nFormal, n;` |
|        - | 11015 | `	VmSlot sSlot;` |
|        - | 11016 | `	sxi32 rc;` |
|        - | 11017 | `	/* Install $this for closure/method callables */` |
|       77 | 11018 | `	if( pClosureThis ){` |
|        - | 11019 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 11020 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 11021 | `		if( pObj ){` |
|      ! 0 | 11022 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 11023 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 11024 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 11025 | `		}` |
|      ! 0 | 11026 | `	}` |
|        - | 11027 | `	/* Install static variables */` |
|       77 | 11028 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 11029 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 11030 | `		ph7_value *pVal;` |
|      ! 0 | 11031 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 11032 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 11033 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 11034 | `			if( pVal ){` |
|      ! 0 | 11035 | `				sSlot.pUserData = 0;` |
|      ! 0 | 11036 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 11037 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 11038 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 11039 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 11040 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);` |
|      ! 0 | 11041 | `				}` |
|      ! 0 | 11042 | `			}` |
|      ! 0 | 11043 | `		}` |
|      ! 0 | 11044 | `	}` |
|        - | 11045 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       77 | 11046 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       77 | 11047 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       95 | 11048 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 11049 | `		ph7_value *pObj;` |
|       20 | 11050 | `		if( n < (sxu32)nArg ){` |
|        - | 11051 | `			/* Argument provided — install with type casting */` |
|       20 | 11052 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 11053 | `			if( pObj ){` |
|       20 | 11054 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 11055 | `				/* Type casting */` |
|       20 | 11056 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 11057 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 11058 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 11059 | `						if( xCast ){` |
|      ! 0 | 11060 | `							xCast(pObj);` |
|      ! 0 | 11061 | `						}` |
|      ! 0 | 11062 | `					}` |
|      ! 0 | 11063 | `				}` |
|       20 | 11064 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 11065 | `				sSlot.pUserData = 0;` |
|       20 | 11066 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 11067 | `			}` |
|        9 | 11068 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 11069 | `			/* Default value */` |
|      ! 0 | 11070 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 11071 | `			if( pObj ){` |
|      ! 0 | 11072 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);` |
|      ! 0 | 11073 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11074 | `					return rc;` |
|        - | 11075 | `				}` |
|      ! 0 | 11076 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 11077 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 11078 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 11079 | `						if( xCast ){` |
|      ! 0 | 11080 | `							xCast(pObj);` |
|      ! 0 | 11081 | `						}` |
|      ! 0 | 11082 | `					}` |
|      ! 0 | 11083 | `				}` |
|      ! 0 | 11084 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 11085 | `				sSlot.pUserData = 0;` |
|      ! 0 | 11086 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 11087 | `			}` |
|      ! 0 | 11088 | `		}` |
|       11 | 11089 | `	}` |
|        - | 11090 | `	/* Install closure environment (captured variables) */` |
|       77 | 11091 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 11092 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 11093 | `		ph7_value *pValue;` |
|        - | 11094 | `		sxu32 iEnv;` |
|        3 | 11095 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 11096 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 11097 | `			pEnv = &aEnv[iEnv];` |
|        7 | 11098 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 11099 | `				continue;` |
|        - | 11100 | `			}` |
|        5 | 11101 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 11102 | `			if( pValue == 0 ){` |
|      ! 0 | 11103 | `				continue;` |
|        - | 11104 | `			}` |
|        5 | 11105 | `			PH7_MemObjRelease(pValue);` |
|        5 | 11106 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 11107 | `		}` |
|        1 | 11108 | `	}` |
|       77 | 11109 | `	return SXRET_OK;` |
|       41 | 11110 |  |
|        - | 11111 | `/*` |
|        - | 11112 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 11113 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 11114 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 11115 | ` */` |
|       26 | 11116 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11117 |  |
|       31 | 11118 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11119 | `	ph7_class_instance *pThis;` |
|        - | 11120 | `	ph7_class_instance *pClosureThis;` |
|        - | 11121 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 11122 | `	ph7_vm_func *pFunc;` |
|        - | 11123 | `	ph7_value sResult;` |
|        - | 11124 | `	ph7_value *pCtxAttr;` |
|        - | 11125 | `	SyString sAttrName;` |
|        - | 11126 | `	sxi32 rc;` |
|       31 | 11127 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11128 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 11129 | `	}` |
|       31 | 11130 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11131 | `	/* Check if already started (has a __ctx) */` |
|       31 | 11132 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       31 | 11133 | `	if( pExecCtx != 0 ){` |
|        3 | 11134 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11135 | `			"Cannot start a fiber that has already been started");` |
|        - | 11136 | `	}` |
|        - | 11137 | `	/* Resolve callable */` |
|       29 | 11138 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       29 | 11139 | `	if( pFunc == 0 ){` |
|      ! 0 | 11140 | `		return PH7_EXCEPTION;` |
|        - | 11141 | `	}` |
|        - | 11142 | `	/* Create execution context now that we know the function */` |
|       29 | 11143 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       29 | 11144 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 11145 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11146 | `			"Fiber::start(): out of memory");` |
|        - | 11147 | `	}` |
|        - | 11148 | `	/* Store context in $this->__ctx */` |
|       29 | 11149 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       29 | 11150 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       29 | 11151 | `	if( pCtxAttr ){` |
|       29 | 11152 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       29 | 11153 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 11154 | `	}` |
|        - | 11155 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 11156 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 11157 | `	 * into the fiber's frame, not the caller's. */` |
|       29 | 11158 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       29 | 11159 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 11160 | `	/* Unpack the args array and install into the frame */` |
|        - | 11161 | `	{` |
|       29 | 11162 | `		ph7_value **apValues = 0;` |
|       29 | 11163 | `		ph7_value *aStore = 0;` |
|       29 | 11164 | `		int nActual = 0;` |
|       29 | 11165 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       29 | 11166 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 11167 | `			ph7_hashmap_node *pNode;` |
|       29 | 11168 | `			sxu32 nCount = pMap->nEntry;` |
|       29 | 11169 | `			if( nCount > 0 ){` |
|        3 | 11170 | `				sxu32 idx = 0;` |
|        4 | 11171 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 11172 | `					nCount * sizeof(ph7_value *));` |
|        4 | 11173 | `				aStore = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 11174 | `					nCount * sizeof(ph7_value));` |
|        3 | 11175 | `				if( apValues && aStore ){` |
|        3 | 11176 | `					pNode = pMap->pFirst;` |
|        7 | 11177 | `					while( pNode && idx < nCount ){` |
|        - | 11178 | `						/* Snapshot each source into stable storage: VmFiberSetupFrame reserves` |
|        - | 11179 | `						 * memory objects (VmExtractMemObj) before reading the args, which can` |
|        - | 11180 | `						 * reallocate (move) pVm->aMemObj and dangle a raw pool pointer. A` |
|        - | 11181 | `						 * shallow copy is a safe source — the referent and the heap-resident` |
|        - | 11182 | `						 * blob data survive the move (same sSafeVal idiom the hashmap inserters` |
|        - | 11183 | `						 * use); it owns nothing independently, so it needs no release. */` |
|        5 | 11184 | `						ph7_value *pSrc = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 11185 | `						if( pSrc ){` |
|        5 | 11186 | `							aStore[idx] = *pSrc;` |
|        3 | 11187 | `						}else{` |
|      ! 0 | 11188 | `							PH7_MemObjInit(pVm, &aStore[idx]);` |
|        - | 11189 | `						}` |
|        5 | 11190 | `						apValues[idx] = &aStore[idx];` |
|        5 | 11191 | `						idx++;` |
|        5 | 11192 | `						pNode = pNode->pPrev;` |
|        1 | 11193 | `					}` |
|        3 | 11194 | `					nActual = (int)idx;` |
|        1 | 11195 | `				}` |
|        1 | 11196 | `			}` |
|       12 | 11197 | `		}` |
|       29 | 11198 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       29 | 11199 | `		if( aStore ){` |
|        3 | 11200 | `			SyMemBackendFree(&pVm->sAllocator, aStore);` |
|        1 | 11201 | `		}` |
|       29 | 11202 | `		if( apValues ){` |
|        3 | 11203 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 11204 | `		}` |
|        - | 11205 | `	}` |
|        - | 11206 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       29 | 11207 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       29 | 11208 | `	pExecCtx->pFrame->pParent = 0;` |
|       29 | 11209 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11210 | `		return PH7_ABORT;` |
|        - | 11211 | `	}` |
|       29 | 11212 | `	PH7_MemObjInit(pVm, &sResult);` |
|       29 | 11213 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       29 | 11214 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 11215 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 11216 | `		return PH7_ABORT;` |
|        - | 11217 | `	}` |
|       29 | 11218 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 11219 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 11220 | `		return PH7_EXCEPTION;` |
|        - | 11221 | `	}` |
|       29 | 11222 | `	ph7_result_value(pCtx, &sResult);` |
|       29 | 11223 | `	PH7_MemObjRelease(&sResult);` |
|       29 | 11224 | `	return PH7_OK;` |
|       18 | 11225 |  |
|        - | 11226 | `/*` |
|        - | 11227 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 11228 | ` */` |
|       36 | 11229 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11230 |  |
|       41 | 11231 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11232 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 11233 | `	ph7_value sResult;` |
|        - | 11234 | `	ph7_value *pResumeVal;` |
|        - | 11235 | `	sxi32 rc;` |
|       41 | 11236 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11237 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 11238 | `		return PH7_OK;` |
|        - | 11239 | `	}` |
|       41 | 11240 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       41 | 11241 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 11242 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 11243 | `		return PH7_OK;` |
|        - | 11244 | `	}` |
|       41 | 11245 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 11246 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11247 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 11248 | `	}` |
|       39 | 11249 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       39 | 11250 | `	PH7_MemObjInit(pVm, &sResult);` |
|       39 | 11251 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       39 | 11252 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 11253 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 11254 | `		return PH7_ABORT;` |
|        - | 11255 | `	}` |
|       39 | 11256 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 11257 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 11258 | `		return PH7_EXCEPTION;` |
|        - | 11259 | `	}` |
|       39 | 11260 | `	ph7_result_value(pCtx, &sResult);` |
|       39 | 11261 | `	PH7_MemObjRelease(&sResult);` |
|       39 | 11262 | `	return PH7_OK;` |
|       23 | 11263 |  |
|        - | 11264 | `/*` |
|        - | 11265 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 11266 | ` */` |
|        6 | 11267 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        3 | 11268 |  |
|        9 | 11269 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11270 | `	ph7_exec_ctx *pExecCtx;` |
|        9 | 11271 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11272 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11273 | `		return PH7_OK;` |
|        - | 11274 | `	}` |
|        9 | 11275 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        9 | 11276 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 11277 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11278 | `		return PH7_OK;` |
|        - | 11279 | `	}` |
|        9 | 11280 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11281 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11282 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11283 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 11284 | `		}` |
|      ! 0 | 11285 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11286 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 11287 | `	}` |
|        9 | 11288 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        9 | 11289 | `	return PH7_OK;` |
|        6 | 11290 |  |
|        - | 11291 | `/*` |
|        - | 11292 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 11293 | ` */` |
|        6 | 11294 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11295 |  |
|        - | 11296 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 11297 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 11298 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 11299 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 11300 | `	return PH7_OK;` |
|        4 | 11301 |  |
|      ! 0 | 11302 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11303 |  |
|        - | 11304 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 11305 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 11306 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11307 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 11308 | `	return PH7_OK;` |
|      ! 0 | 11309 |  |
|        6 | 11310 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11311 |  |
|        - | 11312 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 11313 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 11314 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 11315 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 11316 | `	return PH7_OK;` |
|        4 | 11317 |  |
|        6 | 11318 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11319 |  |
|        - | 11320 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 11321 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 11322 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 11323 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 11324 | `	return PH7_OK;` |
|        4 | 11325 |  |
|        - | 11326 | `/*` |
|        - | 11327 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 11328 | ` */` |
|        4 | 11329 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11330 |  |
|        5 | 11331 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11332 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 11333 | `	if( nArg < 1 ){` |
|      ! 0 | 11334 | `		return PH7_OK;` |
|        - | 11335 | `	}` |
|        5 | 11336 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 11337 | `	if( pExecCtx ){` |
|        5 | 11338 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 11339 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 11340 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 11341 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11342 | `			SyString sAttrName;` |
|        - | 11343 | `			ph7_value *pAttr;` |
|        5 | 11344 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 11345 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 11346 | `			if( pAttr ){` |
|        5 | 11347 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 11348 | `			}` |
|        2 | 11349 | `		}` |
|        2 | 11350 | `	}` |
|        5 | 11351 | `	return PH7_OK;` |
|        3 | 11352 |  |
|        - | 11353 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 11354 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 11355 |  |
|        - | 11356 | `	ph7_class_instance *pThis;` |
|      ! 0 | 11357 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 11358 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 11359 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 11360 |  |
|      ! 0 | 11361 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 11362 |  |
|        - | 11363 | `	ph7_class_instance *pThis;` |
|      ! 0 | 11364 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 11365 | `	ph7_exec_ctx *pCtx;` |
|        - | 11366 | `	ph7_vm_func *pFunc;` |
|        - | 11367 | `	ph7_value *pCallable;` |
|        - | 11368 | `	ph7_value *pCtxAttr;` |
|        - | 11369 | `	SyString sAttrName;` |
|        - | 11370 | `	/* Must not already be started */` |
|      ! 0 | 11371 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11372 | `	if( pCtx != 0 ){` |
|      ! 0 | 11373 | `		return SXERR_INVALID;` |
|        - | 11374 | `	}` |
|      ! 0 | 11375 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11376 | `		return SXERR_INVALID;` |
|        - | 11377 | `	}` |
|      ! 0 | 11378 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 11379 | `	/* Get the callable */` |
|      ! 0 | 11380 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 11381 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11382 | `	if( pCallable == 0 ){` |
|      ! 0 | 11383 | `		return SXERR_INVALID;` |
|        - | 11384 | `	}` |
|        - | 11385 | `	/* Resolve callable */` |
|      ! 0 | 11386 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 11387 | `		SyString sName;` |
|        - | 11388 | `		SyHashEntry *pEntry;` |
|      ! 0 | 11389 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 11390 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 11391 | `		if( pEntry == 0 ){` |
|      ! 0 | 11392 | `			return SXERR_NOTFOUND;` |
|        - | 11393 | `		}` |
|      ! 0 | 11394 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 11395 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11396 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 11397 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 11398 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 11399 | `		if( pMethod == 0 ){` |
|      ! 0 | 11400 | `			return SXERR_INVALID;` |
|        - | 11401 | `		}` |
|      ! 0 | 11402 | `		pClosureThis = pClosure;` |
|      ! 0 | 11403 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 11404 | `	}else{` |
|      ! 0 | 11405 | `		return SXERR_INVALID;` |
|        - | 11406 | `	}` |
|        - | 11407 | `	/* Create context */` |
|      ! 0 | 11408 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 11409 | `	if( pCtx == 0 ){` |
|      ! 0 | 11410 | `		return SXERR_MEM;` |
|        - | 11411 | `	}` |
|        - | 11412 | `	/* Store in __ctx */` |
|      ! 0 | 11413 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11414 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11415 | `	if( pCtxAttr ){` |
|      ! 0 | 11416 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 11417 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 11418 | `	}` |
|        - | 11419 | `	/* Set up frame with args */` |
|      ! 0 | 11420 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 11421 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 11422 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 11423 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 11424 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 11425 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 11426 |  |
|      ! 0 | 11427 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 11428 |  |
|      ! 0 | 11429 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11430 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 11431 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 11432 |  |
|      ! 0 | 11433 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11434 |  |
|      ! 0 | 11435 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11436 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 11437 |  |
|      ! 0 | 11438 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11439 |  |
|      ! 0 | 11440 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11441 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 11442 |  |
|      ! 0 | 11443 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11444 |  |
|      ! 0 | 11445 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11446 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 11447 | `	return &pCtx->sRetValue;` |
|      ! 0 | 11448 |  |
|        - | 11449 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 11450 | `/*` |
|        - | 11451 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 11452 | ` */` |
|       48 | 11453 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 | 11454 |  |
|        - | 11455 | `	ph7_generator *pGen;` |
|       53 | 11456 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       53 | 11457 | `	if( pGen == 0 ){` |
|      ! 0 | 11458 | `		return 0;` |
|        - | 11459 | `	}` |
|       53 | 11460 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       53 | 11461 | `	pGen->pCtx = pCtx;` |
|       53 | 11462 | `	pGen->iImplicitKey = 0;` |
|       53 | 11463 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       53 | 11464 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 11465 | `	/* Link the generator back to the exec context */` |
|       53 | 11466 | `	pCtx->pPrivate = pGen;` |
|       53 | 11467 | `	return pGen;` |
|       29 | 11468 |  |
|        - | 11469 | `/*` |
|        - | 11470 | ` * Release a generator and its execution context.` |
|        - | 11471 | ` */` |
|      ! 0 | 11472 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 11473 |  |
|      ! 0 | 11474 | `	if( pGen == 0 ){` |
|      ! 0 | 11475 | `		return;` |
|        - | 11476 | `	}` |
|      ! 0 | 11477 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 11478 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 11479 | `	if( pGen->pCtx ){` |
|      ! 0 | 11480 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 11481 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 11482 | `		pGen->pCtx = 0;` |
|      ! 0 | 11483 | `	}` |
|      ! 0 | 11484 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 11485 |  |
|        - | 11486 | `/*` |
|        - | 11487 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 11488 | ` */` |
|      496 | 11489 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        5 | 11490 |  |
|        - | 11491 | `	ph7_class_instance *pThis;` |
|        - | 11492 | `	SyString sAttr;` |
|        - | 11493 | `	ph7_value *pAttr;` |
|      501 | 11494 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11495 | `		return 0;` |
|        - | 11496 | `	}` |
|      501 | 11497 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      501 | 11498 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 11499 | `		return 0;` |
|        - | 11500 | `	}` |
|      501 | 11501 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      501 | 11502 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      501 | 11503 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 11504 | `		return 0;` |
|        - | 11505 | `	}` |
|      501 | 11506 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      253 | 11507 |  |
|        - | 11508 | `/*` |
|        - | 11509 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 11510 | ` */` |
|       44 | 11511 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11512 |  |
|        - | 11513 | `	ph7_generator *pGen;` |
|        - | 11514 | `	sxi32 rc;` |
|       49 | 11515 | `	if( nArg < 1 ) return PH7_OK;` |
|       49 | 11516 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       49 | 11517 | `	if( pGen == 0 ) return PH7_OK;` |
|       49 | 11518 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       49 | 11519 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       49 | 11520 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       49 | 11521 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       22 | 11522 | `	}` |
|       49 | 11523 | `	return PH7_OK;` |
|       27 | 11524 |  |
|        - | 11525 | `/*` |
|        - | 11526 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 11527 | ` */` |
|      142 | 11528 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        4 | 11529 |  |
|        - | 11530 | `	ph7_generator *pGen;` |
|      146 | 11531 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      146 | 11532 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      146 | 11533 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|      146 | 11534 | `	return PH7_OK;` |
|       75 | 11535 |  |
|        - | 11536 | `/*` |
|        - | 11537 | ` * Generator::current() — return the last yielded value.` |
|        - | 11538 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11539 | ` */` |
|      124 | 11540 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11541 |  |
|        - | 11542 | `	ph7_generator *pGen;` |
|        - | 11543 | `	sxi32 rc;` |
|      129 | 11544 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      129 | 11545 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      129 | 11546 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      129 | 11547 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11548 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11549 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11550 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11551 | `	}` |
|      129 | 11552 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      129 | 11553 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       67 | 11554 | `	}else{` |
|      ! 0 | 11555 | `		ph7_result_null(pCtx);` |
|        - | 11556 | `	}` |
|      129 | 11557 | `	return PH7_OK;` |
|       67 | 11558 |  |
|        - | 11559 | `/*` |
|        - | 11560 | ` * Generator::key() — return the last yielded key.` |
|        - | 11561 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11562 | ` */` |
|       68 | 11563 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        3 | 11564 |  |
|        - | 11565 | `	ph7_generator *pGen;` |
|        - | 11566 | `	sxi32 rc;` |
|       71 | 11567 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       71 | 11568 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       71 | 11569 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       71 | 11570 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11571 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11572 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11573 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11574 | `	}` |
|       71 | 11575 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       71 | 11576 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|       37 | 11577 | `	}else{` |
|      ! 0 | 11578 | `		ph7_result_null(pCtx);` |
|        - | 11579 | `	}` |
|       71 | 11580 | `	return PH7_OK;` |
|       37 | 11581 |  |
|        - | 11582 | `/*` |
|        - | 11583 | ` * Generator::next() — advance to the next yield point.` |
|        - | 11584 | ` */` |
|      112 | 11585 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11586 |  |
|        - | 11587 | `	ph7_generator *pGen;` |
|        - | 11588 | `	sxi32 rc;` |
|      117 | 11589 | `	if( nArg < 1 ) return PH7_OK;` |
|      117 | 11590 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      117 | 11591 | `	if( pGen == 0 ) return PH7_OK;` |
|      117 | 11592 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11593 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      117 | 11594 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      117 | 11595 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       61 | 11596 | `	}else{` |
|      ! 0 | 11597 | `		return PH7_OK;` |
|        - | 11598 | `	}` |
|      117 | 11599 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      117 | 11600 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      117 | 11601 | `	return PH7_OK;` |
|       61 | 11602 |  |
|        - | 11603 | `/*` |
|        - | 11604 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 11605 | ` */` |
|        4 | 11606 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11607 |  |
|        - | 11608 | `	ph7_generator *pGen;` |
|        - | 11609 | `	ph7_value *pSendVal;` |
|        - | 11610 | `	sxi32 rc;` |
|        5 | 11611 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 11612 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 11613 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 11614 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 11615 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 11616 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 11617 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 11618 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 11619 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 11620 | `	}else{` |
|      ! 0 | 11621 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11622 | `		return PH7_OK;` |
|        - | 11623 | `	}` |
|        5 | 11624 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 11625 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 11626 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 11627 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 11628 | `	}else{` |
|        3 | 11629 | `		ph7_result_null(pCtx);` |
|        - | 11630 | `	}` |
|        5 | 11631 | `	return PH7_OK;` |
|        3 | 11632 |  |
|        - | 11633 | `/*` |
|        - | 11634 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 11635 | ` *` |
|        - | 11636 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 11637 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 11638 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 11639 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 11640 | ` * the exception to the caller.` |
|        - | 11641 | ` */` |
|      ! 0 | 11642 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11643 |  |
|        - | 11644 | `	ph7_generator *pGen;` |
|        - | 11645 | `	const char *zMsg;` |
|        - | 11646 | `	int nLen;` |
|      ! 0 | 11647 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 11648 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11649 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 11650 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 11651 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 11652 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11653 | `			"Cannot throw into a closed generator");` |
|        - | 11654 | `	}` |
|        - | 11655 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 11656 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 11657 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 11658 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 11659 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 11660 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 11661 | `	nLen = 0;` |
|      ! 0 | 11662 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 11663 | `		/* Try to get the exception's message */` |
|        - | 11664 | `		SyString sAttr;` |
|        - | 11665 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 11666 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 11667 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 11668 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 11669 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 11670 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 11671 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 11672 | `		}` |
|      ! 0 | 11673 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 11674 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 11675 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 11676 | `	}` |
|      ! 0 | 11677 | `	(void)nLen;` |
|      ! 0 | 11678 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 11679 |  |
|        - | 11680 | `/*` |
|        - | 11681 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 11682 | ` */` |
|        2 | 11683 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11684 |  |
|        - | 11685 | `	ph7_generator *pGen;` |
|        3 | 11686 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11687 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 11688 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11689 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11690 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11691 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 11692 | `	}` |
|        3 | 11693 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 11694 | `	return PH7_OK;` |
|        2 | 11695 |  |
|        - | 11696 | `/*` |
|        - | 11697 | ` * Generator::__destruct() — clean up.` |
|        - | 11698 | ` */` |
|      ! 0 | 11699 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11700 |  |
|        - | 11701 | `	ph7_generator *pGen;` |
|      ! 0 | 11702 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 11703 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11704 | `	if( pGen ){` |
|      ! 0 | 11705 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 11706 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11707 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11708 | `			SyString sAttrName;` |
|        - | 11709 | `			ph7_value *pAttr;` |
|      ! 0 | 11710 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11711 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11712 | `			if( pAttr ){` |
|      ! 0 | 11713 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 11714 | `			}` |
|      ! 0 | 11715 | `		}` |
|      ! 0 | 11716 | `	}` |
|      ! 0 | 11717 | `	return PH7_OK;` |
|      ! 0 | 11718 |  |
|        - | 11719 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 11720 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 11721 | `/*` |
|        - | 11722 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 11723 | ` * the desired message.` |
|        - | 11724 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 11725 | ` * in 'api.c' for additional information.` |
|        - | 11726 | ` */` |
|      370 | 11727 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 11728 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 11729 | `	SyString *pString /* Message to output */` |
|        - | 11730 | `	)` |
|        4 | 11731 |  |
|      374 | 11732 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      374 | 11733 | `	sxi32 rc = SXRET_OK;` |
|        - | 11734 | `	/* Call the output consumer */` |
|      374 | 11735 | `	if( pString->nByte > 0 ){` |
|      374 | 11736 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      374 | 11737 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 11738 | `	}` |
|      374 | 11739 | `	return rc;` |
|        4 | 11740 |  |
|        - | 11741 | `/*` |
|        - | 11742 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 11743 | ` * callback to consume the formatted message.` |
|        - | 11744 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 11745 | ` * in 'api.c' for additional information.` |
|        - | 11746 | ` */` |
|        2 | 11747 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 11748 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 11749 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 11750 | `	va_list ap           /* Variable list of arguments */` |
|        - | 11751 | `	)` |
|        1 | 11752 |  |
|        3 | 11753 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 11754 | `	sxi32 rc = SXRET_OK;` |
|        - | 11755 | `	SyBlob sWorker;` |
|        - | 11756 | `	/* Format the message and call the output consumer */` |
|        3 | 11757 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 11758 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 11759 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 11760 | `		/* Consume the formatted message */` |
|        3 | 11761 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 11762 | `	}` |
|        3 | 11763 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 11764 | `	/* Release the working buffer */` |
|        3 | 11765 | `	SyBlobRelease(&sWorker);` |
|        3 | 11766 | `	return rc;` |
|        1 | 11767 |  |
|        - | 11768 | `/*` |
|        - | 11769 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 11770 | ` * This function never fail and always return a pointer` |
|        - | 11771 | ` * to a null terminated string.` |
|        - | 11772 | ` */` |
|       12 | 11773 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 11774 |  |
|       13 | 11775 | `	const char *zOp = "Unknown     ";` |
|       13 | 11776 | `	switch(nOp){` |
|        3 | 11777 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 11778 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 11779 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 11780 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 11781 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 11782 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 11783 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 11784 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 11785 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 11786 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 11787 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 11788 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 11789 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 11790 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 11791 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 11792 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 11793 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 11794 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 11795 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 11796 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 11797 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 11798 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 11799 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 11800 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 11801 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 11802 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 11803 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 11804 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 11805 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 11806 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 11807 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 11808 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 11809 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 11810 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 11811 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 11812 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 11813 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 11814 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 11815 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 11816 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 11817 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 11818 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 11819 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 11820 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 11821 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 11822 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 11823 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 11824 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 11825 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 11826 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 11827 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 11828 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 11829 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 11830 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 11831 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 11832 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 11833 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 11834 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 11835 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 11836 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 11837 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 11838 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 11839 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 11840 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 11841 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 11842 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 11843 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 11844 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 11845 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 11846 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 11847 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 11848 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 11849 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 11850 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 11851 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 11852 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 11853 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 11854 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 11855 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 11856 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 11857 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 11858 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 11859 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 11860 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 11861 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 11862 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 11863 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 11864 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 11865 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 11866 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 11867 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 11868 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 11869 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 11870 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 11871 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 11872 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 11873 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 11874 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 11875 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 11876 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 11877 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 11878 | `	default:` |
|      ! 0 | 11879 | `		break;` |
|        - | 11880 | `	}` |
|       13 | 11881 | `	return zOp;` |
|        1 | 11882 |  |
|        - | 11883 | `/*` |
|        - | 11884 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 11885 | ` * The xConsumer() callback which is an used defined function` |
|        - | 11886 | ` * is responsible of consuming the generated dump.` |
|        - | 11887 | ` */` |
|        2 | 11888 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 11889 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 11890 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 11891 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 11892 | `	)` |
|        1 | 11893 |  |
|        - | 11894 | `	sxi32 rc;` |
|        3 | 11895 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 11896 | `	return rc;` |
|        1 | 11897 |  |
|        - | 11898 | `/*` |
|        - | 11899 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 11900 | ` * outside a class body [i.e: global or function scope].` |
|        - | 11901 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 11902 | ` * in 'compile.c' for additional information.` |
|        - | 11903 | ` */` |
|       14 | 11904 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 11905 |  |
|       15 | 11906 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 11907 | `	/* Evaluate and expand constant value */` |
|       15 | 11908 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|       15 | 11909 |  |
|        - | 11910 | `/*` |
|        - | 11911 | ` * Section:` |
|        - | 11912 | ` *  Function handling functions.` |
|        - | 11913 | ` * Status:` |
|        - | 11914 | ` *    Stable.` |
|        - | 11915 | ` */` |
|        - | 11916 | `/*` |
|        - | 11917 | ` * int func_num_args(void)` |
|        - | 11918 | ` *   Returns the number of arguments passed to the function.` |
|        - | 11919 | ` * Parameters` |
|        - | 11920 | ` *   None.` |
|        - | 11921 | ` * Return` |
|        - | 11922 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 11923 | ` *  or -1 if called from the globe scope.` |
|        - | 11924 | ` */` |
|     1010 | 11925 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 11926 |  |
|        - | 11927 | `	VmFrame *pFrame;` |
|        - | 11928 | `	ph7_vm *pVm;` |
|        - | 11929 | `	/* Point to the target VM */` |
|     1015 | 11930 | `	pVm = pCtx->pVm;` |
|        - | 11931 | `	/* Current frame */` |
|     1015 | 11932 | `	pFrame = pVm->pFrame;` |
|     1015 | 11933 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1015 | 11934 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 11935 | `		SXUNUSED(nArg);` |
|      ! 0 | 11936 | `		SXUNUSED(apArg);` |
|        - | 11937 | `		/* Global frame,return -1 */` |
|      ! 0 | 11938 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 11939 | `		return SXRET_OK;` |
|        - | 11940 | `	}` |
|        - | 11941 | `	/* Total number of arguments passed to the enclosing function */` |
|     1015 | 11942 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|     1015 | 11943 | `	ph7_result_int(pCtx,nArg);` |
|     1015 | 11944 | `	return SXRET_OK;` |
|      510 | 11945 |  |
|        - | 11946 | `/*` |
|        - | 11947 | ` * value func_get_arg(int $arg_num)` |
|        - | 11948 | ` *   Return an item from the argument list.` |
|        - | 11949 | ` * Parameters` |
|        - | 11950 | ` *  Argument number(index start from zero).` |
|        - | 11951 | ` * Return` |
|        - | 11952 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 11953 | ` */` |
|       22 | 11954 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11955 |  |
|       24 | 11956 | `	ph7_value *pObj = 0;` |
|       24 | 11957 | `	VmSlot *pSlot = 0;` |
|        - | 11958 | `	VmFrame *pFrame;` |
|        - | 11959 | `	ph7_vm *pVm;` |
|        - | 11960 | `	/* Point to the target VM */` |
|       24 | 11961 | `	pVm = pCtx->pVm;` |
|        - | 11962 | `	/* Current frame */` |
|       24 | 11963 | `	pFrame = pVm->pFrame;` |
|       24 | 11964 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 11965 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 11966 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 11967 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 11968 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11969 | `		return SXRET_OK;` |
|        - | 11970 | `	}` |
|        - | 11971 | `	/* Extract the desired index */` |
|       21 | 11972 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 11973 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 11974 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 11975 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11976 | `		return SXRET_OK;` |
|        - | 11977 | `	}` |
|        - | 11978 | `	/* Extract the desired argument */` |
|       21 | 11979 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 11980 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 11981 | `			/* Return the desired argument */` |
|       21 | 11982 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 11983 | `		}else{` |
|        - | 11984 | `			/* No such argument,return false */` |
|      ! 0 | 11985 | `			ph7_result_bool(pCtx,0);` |
|        - | 11986 | `		}` |
|       11 | 11987 | `	}else{` |
|        - | 11988 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11989 | `		ph7_result_bool(pCtx,0);` |
|        - | 11990 | `	}` |
|       21 | 11991 | `	return SXRET_OK;` |
|       13 | 11992 |  |
|        - | 11993 | `/*` |
|        - | 11994 | ` * array func_get_args_byref(void)` |
|        - | 11995 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11996 | ` * Parameters` |
|        - | 11997 | ` *  None.` |
|        - | 11998 | ` * Return` |
|        - | 11999 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 12000 | ` *  member of the current user-defined function's argument list.` |
|        - | 12001 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 12002 | ` * NOTE:` |
|        - | 12003 | ` *  Arguments are returned to the array by reference.` |
|        - | 12004 | ` */` |
|        2 | 12005 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12006 |  |
|        - | 12007 | `	ph7_value *pArray;` |
|        - | 12008 | `	VmFrame *pFrame;` |
|        - | 12009 | `	VmSlot *aSlot;` |
|        - | 12010 | `	sxu32 n;` |
|        - | 12011 | `	/* Point to the current frame */` |
|        3 | 12012 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 12013 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 12014 | `	if( pFrame->pParent == 0 ){` |
|        - | 12015 | `		/* Global frame,return FALSE */` |
|      ! 0 | 12016 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 12017 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12018 | `		return SXRET_OK;` |
|        - | 12019 | `	}` |
|        - | 12020 | `	/* Create a new array */` |
|        3 | 12021 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12022 | `	if( pArray == 0 ){` |
|      ! 0 | 12023 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12024 | `		SXUNUSED(apArg);` |
|      ! 0 | 12025 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12026 | `		return SXRET_OK;` |
|        - | 12027 | `	}` |
|        - | 12028 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 12029 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 12030 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 12031 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 12032 | `	}` |
|        - | 12033 | `	/* Return the freshly created array */` |
|        3 | 12034 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12035 | `	return SXRET_OK;` |
|        2 | 12036 |  |
|        - | 12037 | `/*` |
|        - | 12038 | ` * array func_get_args(void)` |
|        - | 12039 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 12040 | ` * Parameters` |
|        - | 12041 | ` *  None.` |
|        - | 12042 | ` * Return` |
|        - | 12043 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 12044 | ` *  member of the current user-defined function's argument list.` |
|        - | 12045 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 12046 | ` */` |
|       88 | 12047 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 12048 |  |
|       93 | 12049 | `	ph7_value *pObj = 0;` |
|        - | 12050 | `	ph7_value *pArray;` |
|        - | 12051 | `	VmFrame *pFrame;` |
|        - | 12052 | `	VmSlot *aSlot;` |
|        - | 12053 | `	sxu32 n;` |
|        - | 12054 | `	/* Point to the current frame */` |
|       93 | 12055 | `	pFrame = pCtx->pVm->pFrame;` |
|       93 | 12056 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       93 | 12057 | `	if( pFrame->pParent == 0 ){` |
|        - | 12058 | `		/* Global frame,return FALSE */` |
|      ! 0 | 12059 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 12060 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12061 | `		return SXRET_OK;` |
|        - | 12062 | `	}` |
|        - | 12063 | `	/* Create a new array */` |
|       93 | 12064 | `	pArray = ph7_context_new_array(pCtx);` |
|       93 | 12065 | `	if( pArray == 0 ){` |
|      ! 0 | 12066 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12067 | `		SXUNUSED(apArg);` |
|      ! 0 | 12068 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12069 | `		return SXRET_OK;` |
|        - | 12070 | `	}` |
|        - | 12071 | `	/* Start filling the array with the given arguments */` |
|       93 | 12072 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      225 | 12073 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      135 | 12074 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      135 | 12075 | `		if( pObj ){` |
|      135 | 12076 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 12077 | `		}` |
|       69 | 12078 | `	}` |
|        - | 12079 | `	/* Return the freshly created array */` |
|       93 | 12080 | `	ph7_result_value(pCtx,pArray);` |
|       93 | 12081 | `	return SXRET_OK;` |
|       49 | 12082 |  |
|        - | 12083 | `/*` |
|        - | 12084 | ` * bool function_exists(string $name)` |
|        - | 12085 | ` *  Return TRUE if the given function has been defined.` |
|        - | 12086 | ` * Parameters` |
|        - | 12087 | ` *  The name of the desired function.` |
|        - | 12088 | ` * Return` |
|        - | 12089 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 12090 | ` */` |
|     1748 | 12091 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 12092 |  |
|        - | 12093 | `	const char *zName;` |
|        - | 12094 | `	ph7_vm *pVm;` |
|        - | 12095 | `	int nLen;` |
|        - | 12096 | `	int res;` |
|     1753 | 12097 | `	if( nArg < 1 ){` |
|        - | 12098 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 12099 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12100 | `		return SXRET_OK;` |
|        - | 12101 | `	}` |
|        - | 12102 | `	/* Point to the target VM */` |
|     1753 | 12103 | `	pVm = pCtx->pVm;` |
|        - | 12104 | `	/* Extract the function name */` |
|     1753 | 12105 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12106 | `	/* Assume the function is not defined */` |
|     1753 | 12107 | `	res = 0;` |
|        - | 12108 | `	/* Perform the lookup */` |
|     2625 | 12109 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1744 | 12110 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12111 | `			/* Function is defined */` |
|      271 | 12112 | `			res = 1;` |
|      133 | 12113 | `	}` |
|     1753 | 12114 | `	ph7_result_bool(pCtx,res);` |
|     1753 | 12115 | `	return SXRET_OK;` |
|      879 | 12116 |  |
|        - | 12117 | `/*` |
|        - | 12118 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 12119 | ` * [i.e: Whether it is callable or not].` |
|        - | 12120 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 12121 | ` */` |
|    24716 | 12122 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        5 | 12123 |  |
|    24721 | 12124 | `	int res = 0;` |
|    24721 | 12125 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 12126 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 12127 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 12128 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 12129 | `		 * standard PHP behavior. */` |
|       21 | 12130 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       21 | 12131 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       19 | 12132 | `			res = 1;` |
|       11 | 12133 | `		}` |
|        9 | 12134 | `		(void)CallInvoke;` |
|    24712 | 12135 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       30 | 12136 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       30 | 12137 | `		if( pMap->nEntry == 2 ){` |
|        - | 12138 | `			ph7_class *pClass;` |
|        - | 12139 | `			ph7_value *pV;` |
|        - | 12140 | `			/* Extract the target class */` |
|       13 | 12141 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       13 | 12142 | `			if( pV ){` |
|       13 | 12143 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       13 | 12144 | `				if( pClass ){` |
|        - | 12145 | `					ph7_class_method *pMethod;` |
|        - | 12146 | `					/* Extract the target method */` |
|       10 | 12147 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 12148 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 12149 | `						/* Perform the lookup */` |
|       10 | 12150 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 12151 | `						if( pMethod ){` |
|        - | 12152 | `							/* Method is callable */` |
|        5 | 12153 | `							res = 1;` |
|        2 | 12154 | `						}` |
|        4 | 12155 | `					}` |
|        4 | 12156 | `				}` |
|        5 | 12157 | `			}` |
|        9 | 12158 | `		}` |
|    24690 | 12159 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 12160 | `		const char *zName;` |
|        - | 12161 | `		int nLen;` |
|        - | 12162 | `		/* Extract the name */` |
|     5995 | 12163 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 12164 | `		/* Perform the lookup */` |
|     6010 | 12165 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 12166 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12167 | `				/* Function is callable */` |
|     5977 | 12168 | `				res = 1;` |
|     2986 | 12169 | `		}` |
|     2995 | 12170 | `	}` |
|    24721 | 12171 | `	return res;` |
|        5 | 12172 |  |
|        - | 12173 | `/*` |
|        - | 12174 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 12175 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 12176 | ` * Parameters` |
|        - | 12177 | ` * $name` |
|        - | 12178 | ` *    The callback function to check` |
|        - | 12179 | ` * $syntax_only` |
|        - | 12180 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 12181 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 12182 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 12183 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 12184 | ` *    a string.` |
|        - | 12185 | ` * Return` |
|        - | 12186 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 12187 | ` */` |
|       20 | 12188 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12189 |  |
|        - | 12190 | `	ph7_vm *pVm;` |
|        - | 12191 | `	int res;` |
|       21 | 12192 | `	if( nArg < 1 ){` |
|        - | 12193 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 12194 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12195 | `		return SXRET_OK;` |
|        - | 12196 | `	}` |
|        - | 12197 | `	/* Point to the target VM */` |
|       21 | 12198 | `	pVm = pCtx->pVm;` |
|        - | 12199 | `	/* Perform the requested operation */` |
|       21 | 12200 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 12201 | `	ph7_result_bool(pCtx,res);` |
|       21 | 12202 | `	return SXRET_OK;` |
|       11 | 12203 |  |
|        - | 12204 | `/*` |
|        - | 12205 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 12206 | ` * defined below.` |
|        - | 12207 | ` */` |
|     1328 | 12208 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12209 |  |
|     1329 | 12210 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12211 | `	ph7_value sName;` |
|        - | 12212 | `	sxi32 rc;` |
|        - | 12213 | `	/* Prepare the function name for insertion */` |
|     1329 | 12214 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1329 | 12215 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 12216 | `	/* Perform the insertion */` |
|     1329 | 12217 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1329 | 12218 | `	PH7_MemObjRelease(&sName);` |
|     1329 | 12219 | `	return rc;` |
|        1 | 12220 |  |
|        - | 12221 | `/*` |
|        - | 12222 | ` * array get_defined_functions(void)` |
|        - | 12223 | ` *  Returns an array of all defined functions.` |
|        - | 12224 | ` * Parameter` |
|        - | 12225 | ` *  None.` |
|        - | 12226 | ` * Return` |
|        - | 12227 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 12228 | ` *  both built-in (internal) and user-defined.` |
|        - | 12229 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 12230 | ` *  defined ones using $arr["user"].` |
|        - | 12231 | ` * Note:` |
|        - | 12232 | ` *  NULL is returned on failure.` |
|        - | 12233 | ` */` |
|        2 | 12234 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12235 |  |
|        - | 12236 | `	ph7_value *pArray,*pEntry;` |
|        - | 12237 | `	/* NOTE:` |
|        - | 12238 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 12239 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 12240 | `	 */` |
|        3 | 12241 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12242 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12243 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12244 | `		SXUNUSED(apArg);` |
|        - | 12245 | `		/* Return NULL */` |
|      ! 0 | 12246 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12247 | `		return SXRET_OK;` |
|        - | 12248 | `	}` |
|        3 | 12249 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 12250 | `	if( pEntry == 0 ){` |
|        - | 12251 | `		/* Return NULL */` |
|      ! 0 | 12252 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12253 | `		return SXRET_OK;` |
|        - | 12254 | `	}` |
|        - | 12255 | `	/* Fill with the appropriate information */` |
|        3 | 12256 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 12257 | `	/* Create the 'internal' index */` |
|        3 | 12258 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 12259 | `	/* Create the user-func array */` |
|        3 | 12260 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 12261 | `	if( pEntry == 0 ){` |
|        - | 12262 | `		/* Return NULL */` |
|      ! 0 | 12263 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12264 | `		return SXRET_OK;` |
|        - | 12265 | `	}` |
|        - | 12266 | `	/* Fill with the appropriate information */` |
|        3 | 12267 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 12268 | `	/* Create the 'user' index */` |
|        3 | 12269 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 12270 | `	/* Return the multi-dimensional array */` |
|        3 | 12271 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12272 | `	return SXRET_OK;` |
|        2 | 12273 |  |
|        - | 12274 | `/*` |
|        - | 12275 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 12276 | ` *  Register a function for execution on shutdown.` |
|        - | 12277 | ` * Note` |
|        - | 12278 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 12279 | ` *  be called in the same order as they were registered.` |
|        - | 12280 | ` * Parameters` |
|        - | 12281 | ` *  $callback` |
|        - | 12282 | ` *   The shutdown callback to register.` |
|        - | 12283 | ` * $param` |
|        - | 12284 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 12285 | ` * Return` |
|        - | 12286 | ` *  Nothing.` |
|        - | 12287 | ` */` |
|       12 | 12288 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 12289 |  |
|        - | 12290 | `	VmShutdownCB sEntry;` |
|        - | 12291 | `	int i,j;` |
|       17 | 12292 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 12293 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 12294 | `		return PH7_OK;` |
|        - | 12295 | `	}` |
|        - | 12296 | `	/* Zero the Entry */` |
|       17 | 12297 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 12298 | `	/* Initialize fields */` |
|       17 | 12299 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 12300 | `	/* Save the callback name for later invocation name */` |
|       17 | 12301 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      137 | 12302 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      125 | 12303 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       65 | 12304 | `	}` |
|        - | 12305 | `	/* Copy arguments */` |
|       17 | 12306 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 12307 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 12308 | `			/* Limit reached */` |
|      ! 0 | 12309 | `			break;` |
|        - | 12310 | `		}` |
|      ! 0 | 12311 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 12312 | `	}` |
|       17 | 12313 | `	sEntry.nArg = j;` |
|        - | 12314 | `	/* Install the callback */` |
|       17 | 12315 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       17 | 12316 | `	return PH7_OK;` |
|       11 | 12317 |  |
|        - | 12318 | `/*` |
|        - | 12319 | ` * Section:` |
|        - | 12320 | ` *  Class handling functions.` |
|        - | 12321 | ` * Status:` |
|        - | 12322 | ` *    Stable.` |
|        - | 12323 | ` */` |
|        - | 12324 | `/*` |
|        - | 12325 | ` * Extract the top active class. NULL is returned` |
|        - | 12326 | ` * if the class stack is empty.` |
|        - | 12327 | ` */` |
|     1090 | 12328 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        5 | 12329 |  |
|     1095 | 12330 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 12331 | `	ph7_class **apClass;` |
|     1095 | 12332 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 12333 | `		/* Empty stack,return NULL */` |
|       15 | 12334 | `		return 0;` |
|        - | 12335 | `	}` |
|        - | 12336 | `	/* Peek the last entry */` |
|     1081 | 12337 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|     1081 | 12338 | `	return apClass[pSet->nUsed - 1];` |
|      550 | 12339 |  |
|        - | 12340 | `/*` |
|        - | 12341 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 12342 | ` *   Get the class that declared the currently executing method.` |
|        - | 12343 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 12344 | ` *` |
|        - | 12345 | ` * Parameters` |
|        - | 12346 | ` *   pVm: Target VM` |
|        - | 12347 | ` *` |
|        - | 12348 | ` * Return` |
|        - | 12349 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 12350 | ` *   - Not executing within a class method` |
|        - | 12351 | ` *` |
|        - | 12352 | ` * Note` |
|        - | 12353 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 12354 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 12355 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 12356 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 12357 | ` *   declaring class.` |
|        - | 12358 | ` */` |
|       98 | 12359 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        5 | 12360 |  |
|      103 | 12361 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12362 | `	ph7_vm_func *pVmFunc;` |
|        - | 12363 |  |
|        - | 12364 | `	/* Skip exception frames to find the actual method frame */` |
|      103 | 12365 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 12366 |  |
|        - | 12367 | `	/* Check if we're in a method context */` |
|      103 | 12368 | `	if( pFrame->pParent ){` |
|       99 | 12369 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       99 | 12370 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 12371 | `			/* Return the declaring class */` |
|       99 | 12372 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 12373 | `		}` |
|      ! 0 | 12374 | `	}` |
|        - | 12375 |  |
|        5 | 12376 | `	return 0;` |
|       54 | 12377 |  |
|        - | 12378 |  |
|        - | 12379 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 12380 | `/*` |
|        - | 12381 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 12382 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 12383 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 12384 | ` * return value indicates failure.` |
|        - | 12385 | ` */` |
|        - | 12386 | `/*` |
|        - | 12387 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 12388 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 12389 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 12390 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 12391 | ` */` |
|     2972 | 12392 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 12393 | `	ph7_vm *pVm,` |
|        - | 12394 | `	ph7_class_instance *pThis,` |
|        - | 12395 | `	ph7_class_method *pMethod,` |
|        - | 12396 | `	ph7_value *pResult,` |
|        - | 12397 | `	int nArg,` |
|        - | 12398 | `	ph7_value **apArg,` |
|        - | 12399 | `	VmCallArgMap *pMap` |
|        - | 12400 | `	)` |
|        5 | 12401 |  |
|        - | 12402 | `	ph7_value *aStack;` |
|        - | 12403 | `	VmInstr aInstr[2];` |
|        - | 12404 | `	int iCursor;` |
|        - | 12405 | `	int i;` |
|        - | 12406 | `	sxi32 rc;` |
|     2977 | 12407 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2977 | 12408 | `	if( aStack == 0 ){` |
|      ! 0 | 12409 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12410 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 12411 | `		return SXERR_MEM;` |
|        - | 12412 | `	}` |
|     4647 | 12413 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1675 | 12414 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1675 | 12415 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      840 | 12416 | `	}` |
|     2977 | 12417 | `	iCursor = nArg + 1;` |
|     2977 | 12418 | `	if( pThis ){` |
|     2971 | 12419 | `		pThis->iRef++;` |
|     2971 | 12420 | `		aStack[i].x.pOther = pThis;` |
|     2971 | 12421 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1483 | 12422 | `	}` |
|     2977 | 12423 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2977 | 12424 | `	i++;` |
|     2977 | 12425 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2977 | 12426 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2977 | 12427 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2977 | 12428 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2977 | 12429 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2977 | 12430 | `	aInstr[0].iP1 = nArg;` |
|     2977 | 12431 | `	aInstr[0].iP2 = 0;` |
|     2977 | 12432 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2977 | 12433 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2977 | 12434 | `	aInstr[1].iP1 = 1;` |
|     2977 | 12435 | `	aInstr[1].iP2 = 0;` |
|     2977 | 12436 | `	aInstr[1].p3  = 0;` |
|     2977 | 12437 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE);` |
|     2977 | 12438 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12439 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 12440 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2977 | 12441 | `	return rc;` |
|     1491 | 12442 |  |
|     2330 | 12443 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 12444 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 12445 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 12446 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 12447 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 12448 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 12449 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 12450 | `	)` |
|        5 | 12451 |  |
|     2335 | 12452 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        5 | 12453 |  |
|        - | 12454 | `/*` |
|        - | 12455 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|        - | 12456 | ` * returning its result. Returns the exec status so a method that throws` |
|        - | 12457 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|        - | 12458 | ` * opcode, which discards it.` |
|        - | 12459 | ` */` |
|      324 | 12460 | `static sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|        1 | 12461 |  |
|      325 | 12462 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|      325 | 12463 | `	if( pMethod == 0 ){` |
|      ! 0 | 12464 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|        - | 12465 | `	}` |
|      325 | 12466 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|      163 | 12467 |  |
|        - | 12468 | `/*` |
|        - | 12469 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|        - | 12470 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|        - | 12471 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|        - | 12472 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|        - | 12473 | ` *` |
|        - | 12474 | ` * Returns:` |
|        - | 12475 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|        - | 12476 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|        - | 12477 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|        - | 12478 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|        - | 12479 | ` *` |
|        - | 12480 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|        - | 12481 | ` * returns); xStep must copy what it needs.` |
|        - | 12482 | ` */` |
|       28 | 12483 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|        1 | 12484 |  |
|        - | 12485 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|       29 | 12486 | `	ph7_class_instance *pAggregate = 0;` |
|        - | 12487 | `	ph7_class *pIteratorClass;` |
|       29 | 12488 | `	sxi32 rc = SXRET_OK;` |
|       29 | 12489 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|      ! 0 | 12490 | `		return SXERR_NOTIMPLEMENTED;` |
|        - | 12491 | `	}` |
|       29 | 12492 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|       29 | 12493 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       29 | 12494 | `	if( pIteratorClass == 0 ){` |
|      ! 0 | 12495 | `		return SXERR_NOTIMPLEMENTED;` |
|        - | 12496 | `	}` |
|       29 | 12497 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|       27 | 12498 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|       14 | 12499 | `	}else{` |
|        - | 12500 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|        3 | 12501 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|        - | 12502 | `		ph7_value sInner;` |
|        3 | 12503 | `		int bOk = 0;` |
|        3 | 12504 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|      ! 0 | 12505 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|        - | 12506 | `		}` |
|        3 | 12507 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|        3 | 12508 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|        3 | 12509 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      ! 0 | 12510 | `			PH7_MemObjRelease(&sInner);` |
|      ! 0 | 12511 | `			return rc;` |
|        - | 12512 | `		}` |
|        3 | 12513 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|        3 | 12514 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|        3 | 12515 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|        3 | 12516 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|        3 | 12517 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|        3 | 12518 | `				bOk = 1;` |
|        1 | 12519 | `			}` |
|        1 | 12520 | `		}` |
|        3 | 12521 | `		PH7_MemObjRelease(&sInner);` |
|        3 | 12522 | `		if( !bOk ){` |
|        - | 12523 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|      ! 0 | 12524 | `			return SXERR_NOTIMPLEMENTED;` |
|        - | 12525 | `		}` |
|        - | 12526 | `	}` |
|        - | 12527 | `	/* Drive rewind / valid / current / key / step / next */` |
|       29 | 12528 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|       29 | 12529 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|       78 | 12530 | `	for(;;){` |
|        - | 12531 | `		ph7_value sValid,sValue,sKey;` |
|        - | 12532 | `		int isValid;` |
|       93 | 12533 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|       93 | 12534 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|       96 | 12535 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|       93 | 12536 | `		PH7_MemObjToBool(&sValid);` |
|       93 | 12537 | `		isValid = (sValid.x.iVal != 0);` |
|       93 | 12538 | `		PH7_MemObjRelease(&sValid);` |
|       93 | 12539 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|       71 | 12540 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|       71 | 12541 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|       71 | 12542 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|       69 | 12543 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|       69 | 12544 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       69 | 12545 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|       69 | 12546 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|       69 | 12547 | `		PH7_MemObjRelease(&sValue);` |
|       69 | 12548 | `		PH7_MemObjRelease(&sKey);` |
|       69 | 12549 | `		if( rc != SXRET_OK ){` |
|        5 | 12550 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|        5 | 12551 | `			goto done;` |
|        - | 12552 | `		}` |
|       65 | 12553 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|       65 | 12554 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|       12 | 12555 | `	}` |
|       14 | 12556 | `done:` |
|       29 | 12557 | `	PH7_ClassInstanceUnref(pThis);` |
|       29 | 12558 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|       29 | 12559 | `	return rc;` |
|       15 | 12560 |  |
|        - | 12561 | `/*` |
|        - | 12562 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 12563 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 12564 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 12565 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 12566 | ` *` |
|        - | 12567 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 12568 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 12569 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 12570 | ` *` |
|        - | 12571 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 12572 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 12573 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 12574 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 12575 | ` *` |
|        - | 12576 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 12577 | ` */` |
|      174 | 12578 | `static sxi32 VmCallObjectInvoke(` |
|        - | 12579 | `	ph7_vm *pVm,` |
|        - | 12580 | `	ph7_class_instance *pThis,` |
|        - | 12581 | `	int nArg,` |
|        - | 12582 | `	ph7_value **apArg,` |
|        - | 12583 | `	ph7_value *pResult,` |
|        - | 12584 | `	VmCallArgMap *pMap` |
|        - | 12585 | `	)` |
|        4 | 12586 |  |
|        - | 12587 | `	ph7_class_method *pMethod;` |
|      178 | 12588 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      178 | 12589 | `	if( pMethod == 0 ){` |
|       13 | 12590 | `		if( pResult ){` |
|       13 | 12591 | `			PH7_MemObjRelease(pResult);` |
|        6 | 12592 | `		}` |
|       13 | 12593 | `		return SXERR_INVALID;` |
|        - | 12594 | `	}` |
|      166 | 12595 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       91 | 12596 |  |
|        - | 12597 | `/*` |
|        - | 12598 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 12599 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 12600 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 12601 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 12602 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 12603 | ` * lookup or 'goto Exception').` |
|        - | 12604 | ` *` |
|        - | 12605 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 12606 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 12607 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 12608 | ` * reported.` |
|        - | 12609 | ` */` |
|       12 | 12610 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 12611 |  |
|        - | 12612 | `	ph7_class *pErrorClass;` |
|       13 | 12613 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 12614 | `	ph7_class_method *pCons;` |
|        - | 12615 | `	VmFrame *pThrowFrame;` |
|        - | 12616 | `	char zMsg[256];` |
|        - | 12617 | `	int nMsg;` |
|        - | 12618 | `	sxi32 rc;` |
|       25 | 12619 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 12620 | `		"Object of type %.*s is not callable",` |
|       12 | 12621 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 12622 | `		pThis->pClass->sName.zString);` |
|       13 | 12623 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 12624 | `	if( pErrorClass ){` |
|       13 | 12625 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 12626 | `	}` |
|       13 | 12627 | `	if( pErrInst == 0 ){` |
|        - | 12628 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 12629 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 12630 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 12631 | `		 * visible to the user. */` |
|      ! 0 | 12632 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 12633 | `		return SXERR_ABORT;` |
|        - | 12634 | `	}` |
|       13 | 12635 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 12636 | `	if( pCons ){` |
|        - | 12637 | `		ph7_value sArg;` |
|        - | 12638 | `		ph7_value *apMsg[1];` |
|        - | 12639 | `		SyString sMsgStr;` |
|       13 | 12640 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 12641 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 12642 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 12643 | `		apMsg[0] = &sArg;` |
|       13 | 12644 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 12645 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 12646 | `	}` |
|        - | 12647 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 12648 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 12649 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 12650 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 12651 | `	if( pThrowFrame ){` |
|       13 | 12652 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 12653 | `	}` |
|       13 | 12654 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 12655 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 12656 | `	return rc;` |
|        7 | 12657 |  |
|        - | 12658 | `/*` |
|        - | 12659 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 12660 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 12661 | ` * in the apArg[] array.` |
|        - | 12662 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12663 | ` * return value indicates failure.` |
|        - | 12664 | ` */` |
|     1264 | 12665 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 12666 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12667 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12668 | `	int nArg,          /* Total number of given arguments */` |
|        - | 12669 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 12670 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 12671 | `	)` |
|        5 | 12672 |  |
|        - | 12673 | `	ph7_value *aStack;` |
|        - | 12674 | `	VmInstr aInstr[2];` |
|        - | 12675 | `	int i;` |
|     1269 | 12676 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 12677 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 12678 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 12679 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      141 | 12680 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 12681 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 12682 | `			nArg,apArg,pResult,0);` |
|        - | 12683 | `	}` |
|     1177 | 12684 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 12685 | `		/* Don't bother processing,it's invalid anyway */` |
|      540 | 12686 | `		if( pResult ){` |
|        - | 12687 | `			/* Assume a null return value */` |
|      ! 0 | 12688 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12689 | `		}` |
|      540 | 12690 | `		return SXERR_INVALID;` |
|        - | 12691 | `	}` |
|      641 | 12692 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 12693 | `		/* Class method */` |
|       15 | 12694 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 12695 | `		ph7_class_method *pMethod = 0;` |
|       15 | 12696 | `		ph7_class_instance *pThis = 0;` |
|       15 | 12697 | `		ph7_class *pClass = 0;` |
|        - | 12698 | `		ph7_value *pValue;` |
|        - | 12699 | `		sxi32 rc;` |
|       15 | 12700 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 12701 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 12702 | `			if( pResult ){` |
|        - | 12703 | `				/* Assume a null return value */` |
|      ! 0 | 12704 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12705 | `			}` |
|      ! 0 | 12706 | `			return SXRET_OK;` |
|        - | 12707 | `		}` |
|        - | 12708 | `		/* Extract the class name or an instance of it */` |
|       15 | 12709 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 12710 | `		if( pValue ){` |
|       15 | 12711 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 12712 | `		}` |
|       15 | 12713 | `		if( pClass == 0 ){` |
|        - | 12714 | `			/* No such class,return NULL */` |
|      ! 0 | 12715 | `			if( pResult ){` |
|      ! 0 | 12716 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12717 | `			}` |
|      ! 0 | 12718 | `			return SXRET_OK;` |
|        - | 12719 | `		}` |
|       15 | 12720 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 12721 | `			/* Point to the class instance */` |
|        9 | 12722 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 12723 | `		}` |
|        - | 12724 | `		/* Try to extract the method */` |
|       15 | 12725 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 12726 | `		if( pValue ){` |
|       15 | 12727 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 12728 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 12729 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 12730 | `			}` |
|        7 | 12731 | `		}` |
|       15 | 12732 | `		if( pMethod == 0 ){` |
|        - | 12733 | `			/* No such method,return NULL */` |
|      ! 0 | 12734 | `			if( pResult ){` |
|      ! 0 | 12735 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12736 | `			}` |
|      ! 0 | 12737 | `			return SXRET_OK;` |
|        - | 12738 | `		}` |
|        - | 12739 | `		/* Call the class method */` |
|       15 | 12740 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 12741 | `		return rc;` |
|        - | 12742 | `	}` |
|        - | 12743 | `	/* Create a new operand stack */` |
|      627 | 12744 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      627 | 12745 | `	if( aStack == 0 ){` |
|      ! 0 | 12746 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12747 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 12748 | `		if( pResult ){` |
|        - | 12749 | `			/* Assume a null return value */` |
|      ! 0 | 12750 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12751 | `		}` |
|      ! 0 | 12752 | `		return SXERR_MEM;` |
|        - | 12753 | `	}` |
|        - | 12754 | `	/* Fill the operand stack with the given arguments */` |
|     1937 | 12755 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1315 | 12756 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 12757 | `		/*` |
|        - | 12758 | `		 * Symisc eXtension:` |
|        - | 12759 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 12760 | `		 */` |
|     1315 | 12761 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      660 | 12762 | `	}` |
|        - | 12763 | `	/* Push the function name */` |
|      627 | 12764 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      627 | 12765 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 12766 | `	/* Emit the CALL istruction */` |
|      627 | 12767 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      627 | 12768 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      627 | 12769 | `	aInstr[0].iP2 = 0;` |
|      627 | 12770 | `	aInstr[0].p3  = 0;` |
|        - | 12771 | `	/* Emit the DONE instruction */` |
|      627 | 12772 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      627 | 12773 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      627 | 12774 | `	aInstr[1].iP2 = 0;` |
|      627 | 12775 | `	aInstr[1].p3  = 0;` |
|        - | 12776 | `	/* Execute the function body (if available) */` |
|        - | 12777 | `	{` |
|        - | 12778 | `		sxi32 rcExec;` |
|      627 | 12779 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE);` |
|        - | 12780 | `		/* Clean up the mess left behind */` |
|      627 | 12781 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12782 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      627 | 12783 | `		return rcExec;` |
|        - | 12784 | `	}` |
|      637 | 12785 |  |
|        - | 12786 | `/*` |
|        - | 12787 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 12788 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 12789 | ` * parameter.` |
|        - | 12790 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12791 | ` * return value indicates failure.` |
|        - | 12792 | ` */` |
|      240 | 12793 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 12794 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12795 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12796 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 12797 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 12798 | `	)` |
|        1 | 12799 |  |
|        - | 12800 | `	ph7_value *pArg;` |
|        - | 12801 | `	SySet aArg;` |
|        - | 12802 | `	va_list ap;` |
|        - | 12803 | `	sxi32 rc;` |
|      241 | 12804 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 12805 | `	/* Copy arguments one after one */` |
|      241 | 12806 | `	va_start(ap,pResult);` |
|      399 | 12807 | `	for(;;){` |
|      799 | 12808 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 12809 | `		if( pArg == 0 ){` |
|      241 | 12810 | `			break;` |
|        - | 12811 | `		}` |
|      559 | 12812 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 12813 | `	}` |
|        - | 12814 | `	/* Call the core routine */` |
|      241 | 12815 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 12816 | `	/* Cleanup */` |
|      241 | 12817 | `	SySetRelease(&aArg);` |
|      241 | 12818 | `	return rc;` |
|        1 | 12819 |  |
|        - | 12820 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 12821 | `/*` |
|        - | 12822 | ` * bool defined(string $name)` |
|        - | 12823 | ` *  Checks whether a given named constant exists.` |
|        - | 12824 | ` * Parameter:` |
|        - | 12825 | ` *  Name of the desired constant.` |
|        - | 12826 | ` * Return` |
|        - | 12827 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 12828 | ` */` |
|       28 | 12829 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12830 |  |
|        - | 12831 | `	const char *zName;` |
|       30 | 12832 | `	int nLen = 0;` |
|       30 | 12833 | `	int res = 0;` |
|       30 | 12834 | `	if( nArg < 1 ){` |
|        - | 12835 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 12836 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 12837 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12838 | `		return SXRET_OK;` |
|        - | 12839 | `	}` |
|        - | 12840 | `	/* Extract constant name */` |
|       30 | 12841 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12842 | `	/* Perform the lookup */` |
|       30 | 12843 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12844 | `		/* Already defined */` |
|       28 | 12845 | `		res = 1;` |
|       13 | 12846 | `	}` |
|       30 | 12847 | `	ph7_result_bool(pCtx,res);` |
|       30 | 12848 | `	return SXRET_OK;` |
|       16 | 12849 |  |
|        - | 12850 | `/*` |
|        - | 12851 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 12852 | ` * below.` |
|        - | 12853 | ` */` |
|       16 | 12854 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        3 | 12855 |  |
|       19 | 12856 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 12857 | `	/* Expand constant value */` |
|       19 | 12858 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       19 | 12859 |  |
|        - | 12860 | `/*` |
|        - | 12861 | ` * bool define(string $constant_name,expression value)` |
|        - | 12862 | ` *  Defines a named constant at runtime.` |
|        - | 12863 | ` * Parameter:` |
|        - | 12864 | ` *  $constant_name` |
|        - | 12865 | ` *   The name of the constant` |
|        - | 12866 | ` *  $value` |
|        - | 12867 | ` *   Constant value` |
|        - | 12868 | ` * Return:` |
|        - | 12869 | ` *   TRUE on success,FALSE on failure.` |
|        - | 12870 | ` */` |
|       14 | 12871 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 12872 |  |
|        - | 12873 | `	const char *zName;  /* Constant name */` |
|        - | 12874 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       17 | 12875 | `	int nLen = 0;       /* Name length */` |
|        - | 12876 | `	sxi32 rc;` |
|       17 | 12877 | `	if( nArg < 2 ){` |
|        - | 12878 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 12879 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 12880 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12881 | `		return SXRET_OK;` |
|        - | 12882 | `	}` |
|       17 | 12883 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 12884 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 12885 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12886 | `		return SXRET_OK;` |
|        - | 12887 | `	}` |
|        - | 12888 | `	/* Extract constant name */` |
|       17 | 12889 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       17 | 12890 | `	if( nLen < 1 ){` |
|      ! 0 | 12891 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 12892 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12893 | `		return SXRET_OK;` |
|        - | 12894 | `	}` |
|        - | 12895 | `	/* Duplicate constant value */` |
|       17 | 12896 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       17 | 12897 | `	if( pValue == 0 ){` |
|      ! 0 | 12898 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12899 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12900 | `		return SXRET_OK;` |
|        - | 12901 | `	}` |
|        - | 12902 | `	/* Initialize the memory object */` |
|       17 | 12903 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 12904 | `	/* Register the constant */` |
|       17 | 12905 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       17 | 12906 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12907 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 12908 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12909 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12910 | `		return SXRET_OK;` |
|        - | 12911 | `	}` |
|        - | 12912 | `	/* Duplicate constant value */` |
|       17 | 12913 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       17 | 12914 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 12915 | `		/* Lower case the constant name */` |
|      ! 0 | 12916 | `		char *zCur = (char *)zName;` |
|      ! 0 | 12917 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 12918 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 12919 | `				/* UTF-8 stream */` |
|      ! 0 | 12920 | `				zCur++;` |
|      ! 0 | 12921 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 12922 | `					zCur++;` |
|      ! 0 | 12923 | `				}` |
|      ! 0 | 12924 | `				continue;` |
|        - | 12925 | `			}` |
|      ! 0 | 12926 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 12927 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 12928 | `				zCur[0] = (char)c;` |
|      ! 0 | 12929 | `			}` |
|      ! 0 | 12930 | `			zCur++;` |
|      ! 0 | 12931 | `		}` |
|        - | 12932 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|        - | 12933 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|        - | 12934 | `		 * one on a later overwrite would dangle the other. */` |
|        - | 12935 | `		{` |
|      ! 0 | 12936 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      ! 0 | 12937 | `			if( pAlias ){` |
|      ! 0 | 12938 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|      ! 0 | 12939 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|      ! 0 | 12940 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|      ! 0 | 12941 | `			}` |
|        - | 12942 | `		}` |
|      ! 0 | 12943 | `	}` |
|        - | 12944 | `	/* All done,return TRUE */` |
|       17 | 12945 | `	ph7_result_bool(pCtx,1);` |
|       17 | 12946 | `	return SXRET_OK;` |
|       10 | 12947 |  |
|        - | 12948 | `/*` |
|        - | 12949 | ` * value constant(string $name)` |
|        - | 12950 | ` *  Returns the value of a constant` |
|        - | 12951 | ` * Parameter` |
|        - | 12952 | ` *  $name` |
|        - | 12953 | ` *    Name of the constant.` |
|        - | 12954 | ` * Return` |
|        - | 12955 | ` *  Constant value or NULL if not defined.` |
|        - | 12956 | ` */` |
|        8 | 12957 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 12958 |  |
|        - | 12959 | `	SyHashEntry *pEntry;` |
|        - | 12960 | `	ph7_constant *pCons;` |
|        - | 12961 | `	const char *zName; /* Constant name */` |
|        - | 12962 | `	ph7_value sVal;    /* Constant value */` |
|        - | 12963 | `	int nLen;` |
|       11 | 12964 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12965 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 12966 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 12967 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12968 | `		return SXRET_OK;` |
|        - | 12969 | `	}` |
|        - | 12970 | `	/* Extract the constant name */` |
|       11 | 12971 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12972 | `	/* Perform the query */` |
|       11 | 12973 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       11 | 12974 | `	if( pEntry == 0 ){` |
|        3 | 12975 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 12976 | `		ph7_result_null(pCtx);` |
|        3 | 12977 | `		return SXRET_OK;` |
|        - | 12978 | `	}` |
|        9 | 12979 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 12980 | `	/* Point to the structure that describe the constant */` |
|        9 | 12981 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 12982 | `	/* Extract constant value by calling it's associated callback */` |
|        9 | 12983 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 12984 | `	/* Return that value */` |
|        9 | 12985 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 12986 | `	/* Cleanup */` |
|        9 | 12987 | `	PH7_MemObjRelease(&sVal);` |
|        9 | 12988 | `	return SXRET_OK;` |
|        7 | 12989 |  |
|        - | 12990 | `/*` |
|        - | 12991 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 12992 | ` * defined below.` |
|        - | 12993 | ` */` |
|      472 | 12994 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12995 |  |
|      473 | 12996 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12997 | `	ph7_value sName;` |
|        - | 12998 | `	sxi32 rc;` |
|        - | 12999 | `	/* Prepare the constant name for insertion */` |
|      473 | 13000 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      473 | 13001 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 13002 | `	/* Perform the insertion */` |
|      473 | 13003 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      473 | 13004 | `	PH7_MemObjRelease(&sName);` |
|      473 | 13005 | `	return rc;` |
|        1 | 13006 |  |
|        - | 13007 | `/*` |
|        - | 13008 | ` * array get_defined_constants(void)` |
|        - | 13009 | ` *  Returns an associative array with the names of all defined` |
|        - | 13010 | ` *  constants.` |
|        - | 13011 | ` * Parameters` |
|        - | 13012 | ` *  NONE.` |
|        - | 13013 | ` * Returns` |
|        - | 13014 | ` *  Returns the names of all the constants currently defined.` |
|        - | 13015 | ` */` |
|        2 | 13016 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13017 |  |
|        - | 13018 | `	ph7_value *pArray;` |
|        - | 13019 | `	/* Create the array first*/` |
|        3 | 13020 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13021 | `	if( pArray == 0 ){` |
|      ! 0 | 13022 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13023 | `		SXUNUSED(apArg);` |
|        - | 13024 | `		/* Return NULL */` |
|      ! 0 | 13025 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13026 | `		return SXRET_OK;` |
|        - | 13027 | `	}` |
|        - | 13028 | `	/* Fill the array with the defined constants */` |
|        3 | 13029 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 13030 | `	/* Return the created array */` |
|        3 | 13031 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13032 | `	return SXRET_OK;` |
|        2 | 13033 |  |
|        - | 13034 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 13035 | `/*` |
|        - | 13036 | ` * Section:` |
|        - | 13037 | ` *  Random numbers/string generators.` |
|        - | 13038 | ` * Status:` |
|        - | 13039 | ` *    Stable.` |
|        - | 13040 | ` */` |
|        - | 13041 | `/*` |
|        - | 13042 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 13043 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 13044 | ` * implemented in src/sx/sxrand.c).` |
|        - | 13045 | ` */` |
|     3040 | 13046 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        5 | 13047 |  |
|        - | 13048 | `	sxu32 iNum;` |
|     3045 | 13049 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     3045 | 13050 | `	return iNum;` |
|        5 | 13051 |  |
|        - | 13052 | `/*` |
|        - | 13053 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 13054 | ` * Note that the generated string is NOT null terminated.` |
|        - | 13055 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 13056 | ` * implemented in src/sx/sxrand.c).` |
|        - | 13057 | ` */` |
|   248856 | 13058 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        5 | 13059 |  |
|        - | 13060 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 13061 | `	int i;` |
|        - | 13062 | `	/* Generate a binary string first */` |
|   248861 | 13063 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 13064 | `	/* Turn the binary string into english based alphabet */` |
|  2737589 | 13065 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2488733 | 13066 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1244369 | 13067 | `	 }` |
|   248861 | 13068 |  |
|        - | 13069 | `/*` |
|        - | 13070 | ` * int rand()` |
|        - | 13071 | ` * int mt_rand()` |
|        - | 13072 | ` * int rand(int $min,int $max)` |
|        - | 13073 | ` * int mt_rand(int $min,int $max)` |
|        - | 13074 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 13075 | ` * Parameter` |
|        - | 13076 | ` *  $min` |
|        - | 13077 | ` *    The lowest value to return (default: 0)` |
|        - | 13078 | ` *  $max` |
|        - | 13079 | ` *   The highest value to return (default: getrandmax())` |
|        - | 13080 | ` * Return` |
|        - | 13081 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 13082 | ` * Note:` |
|        - | 13083 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 13084 | ` *  by te SQLite3 library.` |
|        - | 13085 | ` */` |
|       20 | 13086 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13087 |  |
|        - | 13088 | `	sxu32 iNum;` |
|        - | 13089 | `	/* Generate the random number */` |
|       21 | 13090 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 13091 | `	if( nArg > 1 ){` |
|        - | 13092 | `		sxu32 iMin,iMax;` |
|        3 | 13093 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 13094 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 13095 | `		if( iMin < iMax ){` |
|        3 | 13096 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 13097 | `			if( iDiv > 0 ){` |
|        3 | 13098 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 13099 | `			}` |
|        1 | 13100 | `		}else if(iMax > 0 ){` |
|      ! 0 | 13101 | `			iNum %= iMax;` |
|      ! 0 | 13102 | `		}` |
|        1 | 13103 | `	}` |
|        - | 13104 | `	/* Return the number */` |
|       21 | 13105 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 13106 | `	return SXRET_OK;` |
|        1 | 13107 |  |
|        - | 13108 | `/*` |
|        - | 13109 | ` * int getrandmax(void)` |
|        - | 13110 | ` * int mt_getrandmax(void)` |
|        - | 13111 | ` * int rc4_getrandmax(void)` |
|        - | 13112 | ` *   Show largest possible random value` |
|        - | 13113 | ` * Return` |
|        - | 13114 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 13115 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 13116 | ` * Note:` |
|        - | 13117 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 13118 | ` *  by te SQLite3 library.` |
|        - | 13119 | ` */` |
|        4 | 13120 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13121 |  |
|        2 | 13122 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 13123 | `	SXUNUSED(apArg);` |
|        5 | 13124 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 13125 | `	return SXRET_OK;` |
|        1 | 13126 |  |
|        - | 13127 | `/*` |
|        - | 13128 | ` * string rand_str()` |
|        - | 13129 | ` * string rand_str(int $len)` |
|        - | 13130 | ` *  Generate a random string (English alphabet).` |
|        - | 13131 | ` * Parameter` |
|        - | 13132 | ` *  $len` |
|        - | 13133 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 13134 | ` * Return` |
|        - | 13135 | ` *   A pseudo random string.` |
|        - | 13136 | ` * Note:` |
|        - | 13137 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 13138 | ` *  by te SQLite3 library.` |
|        - | 13139 | ` *  This function is a symisc extension.` |
|        - | 13140 | ` */` |
|      120 | 13141 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13142 |  |
|        - | 13143 | `	char zString[1024];` |
|      122 | 13144 | `	int iLen = 0x10;` |
|      122 | 13145 | `	if( nArg > 0 ){` |
|        - | 13146 | `		/* Get the desired length */` |
|      122 | 13147 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 13148 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 13149 | `			/* Default length */` |
|        3 | 13150 | `			iLen = 0x10;` |
|        1 | 13151 | `		}` |
|       60 | 13152 | `	}` |
|        - | 13153 | `	/* Generate the random string */` |
|      122 | 13154 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 13155 | `	/* Return the generated string */` |
|      122 | 13156 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 13157 | `	return SXRET_OK;` |
|        2 | 13158 |  |
|        - | 13159 | `/*` |
|        - | 13160 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 13161 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 13162 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 13163 | ` */` |
|      488 | 13164 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 13165 |  |
|      488 | 13166 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 13167 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 13168 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13169 | `			"TypeError",` |
|        - | 13170 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 13171 | `			zFunc,iArgPos,zParamName,` |
|        3 | 13172 | `			ph7_type_name(pArg)` |
|        - | 13173 | `			);` |
|        - | 13174 | `	}` |
|      483 | 13175 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 13176 | `		int len;` |
|        9 | 13177 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 13178 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 13179 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13180 | `				"TypeError",` |
|        - | 13181 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 13182 | `				zFunc,iArgPos,zParamName` |
|        - | 13183 | `				);` |
|        - | 13184 | `		}` |
|        2 | 13185 | `	}` |
|      479 | 13186 | `	return SXRET_OK;` |
|      245 | 13187 |  |
|        - | 13188 | `/*` |
|        - | 13189 | ` * int random_int(int $min, int $max)` |
|        - | 13190 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 13191 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 13192 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 13193 | ` *  power-of-two mask covering the range.` |
|        - | 13194 | ` */` |
|      242 | 13195 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13196 |  |
|        - | 13197 | `	sxi64 iMin,iMax;` |
|        - | 13198 | `	sxu64 uRange,uMask,uResult;` |
|        - | 13199 | `	unsigned int nAttempt;` |
|        - | 13200 | `	int rc;` |
|      243 | 13201 | `	if( nArg != 2 ){` |
|       10 | 13202 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13203 | `			"ArgumentCountError",` |
|        - | 13204 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 13205 | `			nArg` |
|        - | 13206 | `			);` |
|        - | 13207 | `	}` |
|      237 | 13208 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 13209 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 13210 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 13211 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 13212 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 13213 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 13214 | `	if( iMin > iMax ){` |
|        3 | 13215 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13216 | `			"ValueError",` |
|        - | 13217 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 13218 | `			);` |
|        - | 13219 | `	}` |
|      229 | 13220 | `	if( iMin == iMax ){` |
|        5 | 13221 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 13222 | `		return SXRET_OK;` |
|        - | 13223 | `	}` |
|      225 | 13224 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 13225 | `	uMask = uRange;` |
|      225 | 13226 | `	uMask \|= uMask >> 1;` |
|      225 | 13227 | `	uMask \|= uMask >> 2;` |
|      225 | 13228 | `	uMask \|= uMask >> 4;` |
|      225 | 13229 | `	uMask \|= uMask >> 8;` |
|      225 | 13230 | `	uMask \|= uMask >> 16;` |
|      225 | 13231 | `	uMask \|= uMask >> 32;` |
|      225 | 13232 | `	uResult = 0;` |
|      349 | 13233 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 13234 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 13235 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 13236 | `		 * and the low-half mask would always read 0). */` |
|        - | 13237 | `		sxu64 uDraw;` |
|      349 | 13238 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 13239 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 13240 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 13241 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13242 | `				"Exception",` |
|        - | 13243 | `				"Cannot gather sufficient random data"` |
|        - | 13244 | `				);` |
|        - | 13245 | `		}` |
|      349 | 13246 | `		uDraw &= uMask;` |
|      349 | 13247 | `		if( uDraw <= uRange ){` |
|      225 | 13248 | `			uResult = uDraw;` |
|      225 | 13249 | `			break;` |
|        - | 13250 | `		}` |
|       58 | 13251 | `	}` |
|      225 | 13252 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 13253 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13254 | `			"Exception",` |
|        - | 13255 | `			"Cannot gather sufficient random data"` |
|        - | 13256 | `			);` |
|        - | 13257 | `	}` |
|      225 | 13258 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 13259 | `	return SXRET_OK;` |
|      122 | 13260 |  |
|        - | 13261 | `/*` |
|        - | 13262 | ` * string random_bytes(int $length)` |
|        - | 13263 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 13264 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 13265 | ` */` |
|       24 | 13266 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13267 |  |
|        - | 13268 | `	sxi64 iLen;` |
|        - | 13269 | `	unsigned char zStack[256];` |
|        - | 13270 | `	void *pBuf;` |
|        - | 13271 | `	int rc;` |
|       25 | 13272 | `	int bHeap = 0;` |
|       25 | 13273 | `	if( nArg != 1 ){` |
|        7 | 13274 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13275 | `			"ArgumentCountError",` |
|        - | 13276 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 13277 | `			nArg` |
|        - | 13278 | `			);` |
|        - | 13279 | `	}` |
|       21 | 13280 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 13281 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 13282 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 13283 | `	if( iLen < 1 ){` |
|        5 | 13284 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13285 | `			"ValueError",` |
|        - | 13286 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 13287 | `			);` |
|        - | 13288 | `	}` |
|        - | 13289 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 13290 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 13291 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 13292 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 13293 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13294 | `			"ValueError",` |
|        - | 13295 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 13296 | `			);` |
|        - | 13297 | `	}` |
|       13 | 13298 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 13299 | `		pBuf = zStack;` |
|        7 | 13300 | `	}else{` |
|      ! 0 | 13301 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 13302 | `		if( pBuf == 0 ){` |
|      ! 0 | 13303 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13304 | `				"Exception",` |
|        - | 13305 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 13306 | `				iLen` |
|        - | 13307 | `				);` |
|        - | 13308 | `		}` |
|      ! 0 | 13309 | `		bHeap = 1;` |
|        - | 13310 | `	}` |
|       13 | 13311 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 13312 | `		if( bHeap ){` |
|      ! 0 | 13313 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 13314 | `		}` |
|      ! 0 | 13315 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13316 | `			"Exception",` |
|        - | 13317 | `			"Cannot gather sufficient random data"` |
|        - | 13318 | `			);` |
|        - | 13319 | `	}` |
|       13 | 13320 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 13321 | `	if( bHeap ){` |
|      ! 0 | 13322 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 13323 | `	}` |
|       13 | 13324 | `	return SXRET_OK;` |
|       13 | 13325 |  |
|        - | 13326 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13327 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13328 | `/* Unique ID private data */` |
|        - | 13329 | `struct unique_id_data` |
|        - | 13330 |  |
|        - | 13331 | `	ph7_context *pCtx; /* Call context */` |
|        - | 13332 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 13333 | `};` |
|        - | 13334 | `/*` |
|        - | 13335 | ` * Binary to hex consumer callback.` |
|        - | 13336 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 13337 | ` * defined below.` |
|        - | 13338 | ` */` |
|      192 | 13339 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 13340 |  |
|      193 | 13341 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 13342 | `	sxu32 nBuflen;` |
|        - | 13343 | `	/* Extract result buffer length */` |
|      193 | 13344 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 13345 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 13346 | `			/*` |
|        - | 13347 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 13348 | `			 * string will be 13 characters long` |
|        - | 13349 | `			 */` |
|       25 | 13350 | `		return SXERR_ABORT;` |
|        - | 13351 | `	}` |
|      169 | 13352 | `	if( nBuflen > 22 ){` |
|      ! 0 | 13353 | `		return SXERR_ABORT;` |
|        - | 13354 | `	}` |
|        - | 13355 | `	/* Safely Consume the hex stream */` |
|      169 | 13356 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 13357 | `	return SXRET_OK;` |
|       97 | 13358 |  |
|        - | 13359 | `/*` |
|        - | 13360 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 13361 | ` *  Generate a unique ID` |
|        - | 13362 | ` * Parameter` |
|        - | 13363 | ` * $prefix` |
|        - | 13364 | ` *  Append this prefix to the generated unique ID.` |
|        - | 13365 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 13366 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 13367 | ` * $more_entropy` |
|        - | 13368 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 13369 | ` *  that the result will be unique.` |
|        - | 13370 | ` * Return` |
|        - | 13371 | ` *  Returns the unique identifier, as a string.` |
|        - | 13372 | ` */` |
|       24 | 13373 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13374 |  |
|        - | 13375 | `	struct unique_id_data sUniq;` |
|        - | 13376 | `	unsigned char zDigest[20];` |
|       25 | 13377 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13378 | `	const char *zPrefix;` |
|        - | 13379 | `	SHA1Context sCtx;` |
|        - | 13380 | `	char zRandom[7];` |
|        - | 13381 | `	int nPrefix;` |
|        - | 13382 | `	int entropy;` |
|        - | 13383 | `	/* Generate a random string first */` |
|       25 | 13384 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 13385 | `	/* Initialize fields */` |
|       25 | 13386 | `	zPrefix = 0;` |
|       25 | 13387 | `	nPrefix = 0;` |
|       25 | 13388 | `	entropy = 0;` |
|       25 | 13389 | `	if( nArg > 0 ){` |
|        - | 13390 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 13391 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 13392 | `		if( nArg > 1 ){` |
|      ! 0 | 13393 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 13394 | `		}` |
|      ! 0 | 13395 | `	}` |
|       25 | 13396 | `	SHA1Init(&sCtx);` |
|        - | 13397 | `	/* Generate the random ID */` |
|       25 | 13398 | `	if( nPrefix > 0 ){` |
|      ! 0 | 13399 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 13400 | `	}` |
|        - | 13401 | `	/* Append the random ID */` |
|       25 | 13402 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 13403 | `	/* Append the random string */` |
|       25 | 13404 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 13405 | `	/* Increment the number */` |
|       25 | 13406 | `	pVm->unique_id++;` |
|       25 | 13407 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 13408 | `	/* Hexify the digest */` |
|       25 | 13409 | `	sUniq.pCtx = pCtx;` |
|       25 | 13410 | `	sUniq.entropy = entropy;` |
|       25 | 13411 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 13412 | `	/* All done */` |
|       25 | 13413 | `	return PH7_OK;` |
|        1 | 13414 |  |
|        - | 13415 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13416 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13417 | `/*` |
|        - | 13418 | ` * Section:` |
|        - | 13419 | ` *  Language construct implementation as foreign functions.` |
|        - | 13420 | ` * Status:` |
|        - | 13421 | ` *    Stable.` |
|        - | 13422 | ` */` |
|        - | 13423 | `/*` |
|        - | 13424 | ` * void echo($string...)` |
|        - | 13425 | ` *  Output one or more messages.` |
|        - | 13426 | ` * Parameters` |
|        - | 13427 | ` *  $string` |
|        - | 13428 | ` *   Message to output.` |
|        - | 13429 | ` * Return` |
|        - | 13430 | ` *  NULL.` |
|        - | 13431 | ` */` |
|      ! 0 | 13432 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 13433 |  |
|        - | 13434 | `	const char *zData;` |
|      ! 0 | 13435 | `	int nDataLen = 0;` |
|        - | 13436 | `	ph7_vm *pVm;` |
|        - | 13437 | `	int i,rc;` |
|        - | 13438 | `	/* Point to the target VM */` |
|      ! 0 | 13439 | `	pVm = pCtx->pVm;` |
|        - | 13440 | `	/* Output */` |
|      ! 0 | 13441 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 13442 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 13443 | `		if( nDataLen > 0 ){` |
|      ! 0 | 13444 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 13445 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 13446 | `			if( rc == SXERR_ABORT ){` |
|        - | 13447 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 13448 | `				return PH7_ABORT;` |
|        - | 13449 | `			}` |
|      ! 0 | 13450 | `		}` |
|      ! 0 | 13451 | `	}` |
|      ! 0 | 13452 | `	return SXRET_OK;` |
|      ! 0 | 13453 |  |
|        - | 13454 | `/*` |
|        - | 13455 | ` * int print($string...)` |
|        - | 13456 | ` *  Output one or more messages.` |
|        - | 13457 | ` * Parameters` |
|        - | 13458 | ` *  $string` |
|        - | 13459 | ` *   Message to output.` |
|        - | 13460 | ` * Return` |
|        - | 13461 | ` *  1 always.` |
|        - | 13462 | ` */` |
|        2 | 13463 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13464 |  |
|        - | 13465 | `	const char *zData;` |
|        3 | 13466 | `	int nDataLen = 0;` |
|        - | 13467 | `	ph7_vm *pVm;` |
|        - | 13468 | `	int i,rc;` |
|        - | 13469 | `	/* Point to the target VM */` |
|        3 | 13470 | `	pVm = pCtx->pVm;` |
|        - | 13471 | `	/* Output */` |
|        5 | 13472 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 13473 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 13474 | `		if( nDataLen > 0 ){` |
|        3 | 13475 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 13476 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 13477 | `			if( rc == SXERR_ABORT ){` |
|        - | 13478 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 13479 | `				return PH7_ABORT;` |
|        - | 13480 | `			}` |
|        1 | 13481 | `		}` |
|        2 | 13482 | `	}` |
|        - | 13483 | `	/* Return 1 */` |
|        3 | 13484 | `	ph7_result_int(pCtx,1);` |
|        3 | 13485 | `	return SXRET_OK;` |
|        2 | 13486 |  |
|        - | 13487 | `/*` |
|        - | 13488 | ` * void exit(string $msg)` |
|        - | 13489 | ` * void exit(int $status)` |
|        - | 13490 | ` * void die(string $ms)` |
|        - | 13491 | ` * void die(int $status)` |
|        - | 13492 | ` *   Output a message and terminate program execution.` |
|        - | 13493 | ` * Parameter` |
|        - | 13494 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 13495 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 13496 | ` *  and not printed` |
|        - | 13497 | ` * Return` |
|        - | 13498 | ` *  NULL` |
|        - | 13499 | ` */` |
|      ! 0 | 13500 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 13501 |  |
|      ! 0 | 13502 | `	if( nArg > 0 ){` |
|      ! 0 | 13503 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 13504 | `			const char *zData;` |
|      ! 0 | 13505 | `			int iLen = 0;` |
|        - | 13506 | `			/* Print exit message */` |
|      ! 0 | 13507 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 13508 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 13509 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 13510 | `			sxi32 iExitStatus;` |
|        - | 13511 | `			/* Record exit status code */` |
|      ! 0 | 13512 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 13513 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 13514 | `		}` |
|      ! 0 | 13515 | `	}` |
|        - | 13516 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 13517 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 13518 | `	 */` |
|      ! 0 | 13519 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 13520 | `	return PH7_ABORT;` |
|      ! 0 | 13521 |  |
|        - | 13522 | `/*` |
|        - | 13523 | ` * bool isset($var,...)` |
|        - | 13524 | ` *  Finds out whether a variable is set.` |
|        - | 13525 | ` * Parameters` |
|        - | 13526 | ` *  One or more variable to check.` |
|        - | 13527 | ` * Return` |
|        - | 13528 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 13529 | ` */` |
|    95870 | 13530 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13531 |  |
|        - | 13532 | `	ph7_value *pObj;` |
|    95875 | 13533 | `	int res = 0;` |
|        - | 13534 | `	int i;` |
|    95875 | 13535 | `	if( nArg < 1 ){` |
|        - | 13536 | `		/* Missing arguments,return false */` |
|      ! 0 | 13537 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 13538 | `		return SXRET_OK;` |
|        - | 13539 | `	}` |
|        - | 13540 | `	/* Iterate over available arguments */` |
|   125211 | 13541 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    95885 | 13542 | `		pObj = apArg[i];` |
|    95885 | 13543 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 13544 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 13545 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 13546 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    65463 | 13547 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 13548 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 13549 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 13550 | `			}` |
|    32729 | 13551 | `		}` |
|    95885 | 13552 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    95885 | 13553 | `		if( !res ){` |
|        - | 13554 | `			/* Variable not set,return FALSE */` |
|    66549 | 13555 | `			ph7_result_bool(pCtx,0);` |
|    66549 | 13556 | `			return SXRET_OK;` |
|        - | 13557 | `		}` |
|    14673 | 13558 | `	}` |
|        - | 13559 | `	/* All given variable are set,return TRUE */` |
|    29331 | 13560 | `	ph7_result_bool(pCtx,1);` |
|    29331 | 13561 | `	return SXRET_OK;` |
|    47940 | 13562 |  |
|        - | 13563 | `/*` |
|        - | 13564 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 13565 | ` * frame,the reference table and discard it's contents.` |
|        - | 13566 | ` * This function never fail and always return SXRET_OK.` |
|        - | 13567 | ` */` |
|  3202466 | 13568 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        5 | 13569 |  |
|        - | 13570 | `	ph7_value *pObj;` |
|        - | 13571 | `	VmRefObj *pRef;` |
|  3202471 | 13572 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3202471 | 13573 | `	if( pObj ){` |
|        - | 13574 | `		/* Release the object */` |
|  3202471 | 13575 | `		PH7_MemObjRelease(pObj);` |
|  1601233 | 13576 | `	}` |
|        - | 13577 | `	/* Remove old reference links */` |
|  3202471 | 13578 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3202471 | 13579 | `	if( pRef ){` |
|  3202465 | 13580 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 13581 | `		/* Unlink from the reference table */` |
|  3202465 | 13582 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3202465 | 13583 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 13584 | `			VmSlot sFree;` |
|        - | 13585 | `			/* Restore to the free list */` |
|  3202457 | 13586 | `			sFree.nIdx = nObjIdx;` |
|  3202457 | 13587 | `			sFree.pUserData = 0;` |
|  3202457 | 13588 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1601226 | 13589 | `		}` |
|  1601230 | 13590 | `	}` |
|  3202471 | 13591 | `	return SXRET_OK;` |
|        5 | 13592 |  |
|        - | 13593 | `/*` |
|        - | 13594 | ` * void unset($var,...)` |
|        - | 13595 | ` *   Unset one or more given variable.` |
|        - | 13596 | ` * Parameters` |
|        - | 13597 | ` *  One or more variable to unset.` |
|        - | 13598 | ` * Return` |
|        - | 13599 | ` *  Nothing.` |
|        - | 13600 | ` */` |
|     7608 | 13601 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13602 |  |
|        - | 13603 | `	ph7_value *pObj;` |
|        - | 13604 | `	ph7_vm *pVm;` |
|        - | 13605 | `	int i;` |
|        - | 13606 | `	/* Point to the target VM */` |
|     7613 | 13607 | `	pVm = pCtx->pVm;` |
|        - | 13608 | `	/* Iterate and unset */` |
|    15221 | 13609 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7613 | 13610 | `		pObj = apArg[i];` |
|     7613 | 13611 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      861 | 13612 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 13613 | `				/* Throw an error */` |
|      ! 0 | 13614 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 13615 | `			}` |
|      433 | 13616 | `		}else{` |
|     6757 | 13617 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 13618 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6757 | 13619 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6751 | 13620 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3373 | 13621 | `			}` |
|        - | 13622 | `		}` |
|     3809 | 13623 | `	}` |
|     7613 | 13624 | `	return SXRET_OK;` |
|        5 | 13625 |  |
|        - | 13626 | `/*` |
|        - | 13627 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 13628 | ` */` |
|      122 | 13629 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 13630 |  |
|      123 | 13631 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      123 | 13632 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 13633 | `	ph7_value *pObj;` |
|        - | 13634 | `	sxu32 nIdx;` |
|        - | 13635 | `	/* Extract the memory object */` |
|      123 | 13636 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      123 | 13637 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      123 | 13638 | `	if( pObj ){` |
|      123 | 13639 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      121 | 13640 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 13641 | `				SyString sName;` |
|        - | 13642 | `				ph7_value sKey;` |
|        - | 13643 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - | 13644 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - | 13645 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|      121 | 13646 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      121 | 13647 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      121 | 13648 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      121 | 13649 | `				PH7_MemObjRelease(&sKey);` |
|       60 | 13650 | `			}` |
|       60 | 13651 | `		}` |
|       61 | 13652 | `	}` |
|      123 | 13653 | `	return SXRET_OK;` |
|        1 | 13654 |  |
|        - | 13655 | `/*` |
|        - | 13656 | ` * array get_defined_vars(void)` |
|        - | 13657 | ` *  Returns an array of all defined variables.` |
|        - | 13658 | ` * Parameter` |
|        - | 13659 | ` *  None` |
|        - | 13660 | ` * Return` |
|        - | 13661 | ` *  An array with all the variables defined in the current scope.` |
|        - | 13662 | ` */` |
|        2 | 13663 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13664 |  |
|        3 | 13665 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13666 | `	ph7_value *pArray;` |
|        - | 13667 | `	/* Create a new array */` |
|        3 | 13668 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13669 | ` 	if( pArray == 0 ){` |
|      ! 0 | 13670 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13671 | `		SXUNUSED(apArg);` |
|        - | 13672 | `		/* Return NULL */` |
|      ! 0 | 13673 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13674 | `		return SXRET_OK;` |
|        - | 13675 | `	}` |
|        - | 13676 | `	/* Superglobals first */` |
|        3 | 13677 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 13678 | `	/* Then variable defined in the current frame */` |
|        3 | 13679 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 13680 | `	/* Finally,return the created array */` |
|        3 | 13681 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13682 | `	return SXRET_OK;` |
|        2 | 13683 |  |
|        - | 13684 | `/*` |
|        - | 13685 | ` * bool gettype($var)` |
|        - | 13686 | ` *  Get the type of a variable` |
|        - | 13687 | ` * Parameters` |
|        - | 13688 | ` *   $var` |
|        - | 13689 | ` *    The variable being type checked.` |
|        - | 13690 | ` * Return` |
|        - | 13691 | ` *   String representation of the given variable type.` |
|        - | 13692 | ` */` |
|       34 | 13693 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 13694 |  |
|       37 | 13695 | `	const char *zType = "Empty";` |
|       37 | 13696 | `	if( nArg > 0 ){` |
|       37 | 13697 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       17 | 13698 | `	}` |
|        - | 13699 | `	/* Return the variable type */` |
|       37 | 13700 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       37 | 13701 | `	return SXRET_OK;` |
|        3 | 13702 |  |
|        - | 13703 | `/*` |
|        - | 13704 | ` * string get_resource_type(resource $handle)` |
|        - | 13705 | ` *  This function gets the type of the given resource.` |
|        - | 13706 | ` * Parameters` |
|        - | 13707 | ` *  $handle` |
|        - | 13708 | ` *  The evaluated resource handle.` |
|        - | 13709 | ` * Return` |
|        - | 13710 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 13711 | ` *  representing its type. If the type is not identified by this function` |
|        - | 13712 | ` *  the return value will be the string Unknown.` |
|        - | 13713 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 13714 | ` *  is not a resource.` |
|        - | 13715 | ` */` |
|        2 | 13716 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13717 |  |
|        3 | 13718 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13719 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 13720 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13721 | `		return PH7_OK;` |
|        - | 13722 | `	}` |
|        3 | 13723 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 13724 | `	return SXRET_OK;` |
|        2 | 13725 |  |
|        - | 13726 | `/*` |
|        - | 13727 | ` * void var_dump(expression,....)` |
|        - | 13728 | ` *   var_dump � Dumps information about a variable` |
|        - | 13729 | ` * Parameters` |
|        - | 13730 | ` *   One or more expression to dump.` |
|        - | 13731 | ` * Returns` |
|        - | 13732 | ` *  Nothing.` |
|        - | 13733 | ` */` |
|      218 | 13734 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 13735 |  |
|        - | 13736 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 13737 | `	int i;` |
|      222 | 13738 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 13739 | `	/* Dump one or more expressions */` |
|      446 | 13740 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      228 | 13741 | `		ph7_value *pObj = apArg[i];` |
|        - | 13742 | `		/* Reset the working buffer */` |
|      228 | 13743 | `		SyBlobReset(&sDump);` |
|        - | 13744 | `		/* Dump the given expression */` |
|      228 | 13745 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 13746 | `		/* Output */` |
|      228 | 13747 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      228 | 13748 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 13749 | `		}` |
|      116 | 13750 | `	}` |
|        - | 13751 | `	/* Release the working buffer */` |
|      222 | 13752 | `	SyBlobRelease(&sDump);` |
|      222 | 13753 | `	return SXRET_OK;` |
|        4 | 13754 |  |
|        - | 13755 | `/*` |
|        - | 13756 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 13757 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 13758 | ` * Parameters` |
|        - | 13759 | ` *   expression: Expression to dump` |
|        - | 13760 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 13761 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 13762 | ` *            print_r() will return the information rather than print it.` |
|        - | 13763 | ` * Return` |
|        - | 13764 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 13765 | ` *  Otherwise, the return value is TRUE.` |
|        - | 13766 | ` */` |
|       16 | 13767 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13768 |  |
|       17 | 13769 | `	int ret_string = 0;` |
|        - | 13770 | `	SyBlob sDump;` |
|       17 | 13771 | `	if( nArg < 1 ){` |
|        - | 13772 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13773 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13774 | `		return SXRET_OK;` |
|        - | 13775 | `	}` |
|       17 | 13776 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 13777 | `	if ( nArg > 1 ){` |
|        - | 13778 | `		/* Where to redirect output */` |
|       11 | 13779 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 13780 | `	}` |
|        - | 13781 | `	/* Generate dump */` |
|       17 | 13782 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 13783 | `	if( !ret_string ){` |
|        - | 13784 | `		/* Output dump */` |
|        7 | 13785 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13786 | `		/* Return true */` |
|        7 | 13787 | `		ph7_result_bool(pCtx,1);` |
|        4 | 13788 | `	}else{` |
|        - | 13789 | `		/* Generated dump as return value */` |
|       11 | 13790 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13791 | `	}` |
|        - | 13792 | `	/* Release the working buffer */` |
|       17 | 13793 | `	SyBlobRelease(&sDump);` |
|       17 | 13794 | `	return SXRET_OK;` |
|        9 | 13795 |  |
|        - | 13796 | `/*` |
|        - | 13797 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 13798 | ` * Same job as print_r. (see coment above)` |
|        - | 13799 | ` */` |
|        2 | 13800 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13801 |  |
|        3 | 13802 | `	int ret_string = 0;` |
|        - | 13803 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 13804 | `	if( nArg < 1 ){` |
|        - | 13805 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13806 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13807 | `		return SXRET_OK;` |
|        - | 13808 | `	}` |
|        3 | 13809 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 13810 | `	if ( nArg > 1 ){` |
|        - | 13811 | `		/* Where to redirect output */` |
|        3 | 13812 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 13813 | `	}` |
|        - | 13814 | `	/* Generate dump */` |
|        3 | 13815 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 13816 | `	if( !ret_string ){` |
|        - | 13817 | `		/* Output dump */` |
|      ! 0 | 13818 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13819 | `		/* Return NULL */` |
|      ! 0 | 13820 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13821 | `	}else{` |
|        - | 13822 | `		/* Generated dump as return value */` |
|        3 | 13823 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13824 | `	}` |
|        - | 13825 | `	/* Release the working buffer */` |
|        3 | 13826 | `	SyBlobRelease(&sDump);` |
|        3 | 13827 | `	return SXRET_OK;` |
|        2 | 13828 |  |
|        - | 13829 | `/*` |
|        - | 13830 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 13831 | ` *  Set/get the various assert flags.` |
|        - | 13832 | ` * Parameter` |
|        - | 13833 | ` * $what` |
|        - | 13834 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 13835 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 13836 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 13837 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 13838 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 13839 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 13840 | ` * $value` |
|        - | 13841 | ` *   An optional new value for the option.` |
|        - | 13842 | ` * Return` |
|        - | 13843 | ` *  Old setting on success or FALSE on failure.` |
|        - | 13844 | ` */` |
|       28 | 13845 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13846 |  |
|       33 | 13847 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13848 | `	int iOption;` |
|        - | 13849 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       33 | 13850 | `	if( nArg < 1 ){` |
|        3 | 13851 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13852 | `			"ArgumentCountError",` |
|        - | 13853 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 13854 | `			);` |
|        - | 13855 | `	}` |
|        - | 13856 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 13857 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       31 | 13858 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 13859 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13860 | `			"TypeError",` |
|        - | 13861 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 13862 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 13863 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 13864 | `			);` |
|        - | 13865 | `	}` |
|       31 | 13866 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 13867 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 13868 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 13869 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       31 | 13870 | `	switch( iOption ){` |
|        5 | 13871 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 13872 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 13873 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 13874 | `		if( nArg > 1 ){` |
|        5 | 13875 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13876 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 13877 | `			}else{` |
|        3 | 13878 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 13879 | `			}` |
|        2 | 13880 | `		}` |
|       12 | 13881 | `		break;` |
|        1 | 13882 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 13883 | `		/* Return old callback or null */` |
|        3 | 13884 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 13885 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 13886 | `		}else{` |
|        3 | 13887 | `			ph7_result_null(pCtx);` |
|        - | 13888 | `		}` |
|        3 | 13889 | `		if( nArg > 1 ){` |
|      ! 0 | 13890 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 13891 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 13892 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 13893 | `			}else{` |
|      ! 0 | 13894 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 13895 | `			}` |
|      ! 0 | 13896 | `		}` |
|        3 | 13897 | `		break;` |
|        5 | 13898 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 13899 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 13900 | `		if( nArg > 1 ){` |
|        5 | 13901 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13902 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 13903 | `			}else{` |
|        3 | 13904 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 13905 | `			}` |
|        2 | 13906 | `		}` |
|       11 | 13907 | `		break;` |
|      ! 0 | 13908 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 13909 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13910 | `		break;` |
|        1 | 13911 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 13912 | `		ph7_result_int(pCtx, 1);` |
|        3 | 13913 | `		break;` |
|      ! 0 | 13914 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 13915 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13916 | `		break;` |
|        1 | 13917 | `	default:` |
|        - | 13918 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 13919 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13920 | `			"ValueError",` |
|        - | 13921 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 13922 | `			);` |
|        - | 13923 | `	}` |
|       29 | 13924 | `	return PH7_OK;` |
|       19 | 13925 |  |
|        - | 13926 | `/*` |
|        - | 13927 | ` * bool assert(mixed $assertion)` |
|        - | 13928 | ` *  Checks if assertion is FALSE.` |
|        - | 13929 | ` * Parameter` |
|        - | 13930 | ` *  $assertion` |
|        - | 13931 | ` *    The assertion to test.` |
|        - | 13932 | ` * Return` |
|        - | 13933 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 13934 | ` */` |
|       24 | 13935 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13936 |  |
|       29 | 13937 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13938 | `	int iFlags,iResult;` |
|        - | 13939 | `	const char *zDesc;` |
|        - | 13940 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       29 | 13941 | `	if( nArg < 1 ){` |
|        3 | 13942 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13943 | `			"ArgumentCountError",` |
|        - | 13944 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 13945 | `			);` |
|        - | 13946 | `	}` |
|       27 | 13947 | `	iFlags = pVm->iAssertFlags;` |
|       27 | 13948 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 13949 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 13950 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 13951 | `		return PH7_OK;` |
|        - | 13952 | `	}` |
|        - | 13953 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       27 | 13954 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       27 | 13955 | `	if( !iResult ){` |
|        - | 13956 | `		/* Assertion failed */` |
|        - | 13957 | `		/* Extract optional description */` |
|       16 | 13958 | `		zDesc = 0;` |
|       16 | 13959 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13960 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 13961 | `		}` |
|       16 | 13962 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 13963 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 13964 | `			ph7_value sFile,sLine;` |
|        - | 13965 | `			ph7_value *apCbArg[3];` |
|        - | 13966 | `			SyString *pFile;` |
|        - | 13967 | `			/* Extract the processed script */` |
|      ! 0 | 13968 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 13969 | `			if( pFile == 0 ){` |
|      ! 0 | 13970 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 13971 | `			}` |
|        - | 13972 | `			/* Invoke the callback */` |
|      ! 0 | 13973 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 13974 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 13975 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 13976 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 13977 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 13978 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 13979 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 13980 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 13981 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 13982 | `		}` |
|       16 | 13983 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 13984 | `			/* Abort VM execution immediately */` |
|      ! 0 | 13985 | `			return PH7_ABORT;` |
|        - | 13986 | `		}` |
|        - | 13987 | `		/* PHP 8: throw AssertionError by default */` |
|       16 | 13988 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 13989 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13990 | `				"AssertionError",` |
|        - | 13991 | `				"%s",` |
|        1 | 13992 | `				zDesc` |
|        - | 13993 | `				);` |
|      ! 0 | 13994 | `		}else{` |
|       13 | 13995 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13996 | `				"AssertionError",` |
|        - | 13997 | `				"assert(false)"` |
|        - | 13998 | `				);` |
|        - | 13999 | `		}` |
|        - | 14000 | `	}` |
|        - | 14001 | `	/* Assertion passed */` |
|       11 | 14002 | `	ph7_result_bool(pCtx,1);` |
|       11 | 14003 | `	return PH7_OK;` |
|       17 | 14004 |  |
|        - | 14005 | `/*` |
|        - | 14006 | ` * Section:` |
|        - | 14007 | ` *  Error reporting functions.` |
|        - | 14008 | ` * Status:` |
|        - | 14009 | ` *    Stable.` |
|        - | 14010 | ` */` |
|        - | 14011 | `/*` |
|        - | 14012 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 14013 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 14014 | ` * Parameters` |
|        - | 14015 | ` *  $error_msg` |
|        - | 14016 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 14017 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 14018 | ` * $error_type` |
|        - | 14019 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 14020 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 14021 | ` * Return` |
|        - | 14022 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 14023 | ` */` |
|       12 | 14024 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 14025 |  |
|       17 | 14026 | `	int nErr = PH7_CTX_NOTICE;` |
|       17 | 14027 | `	int rc = PH7_OK;` |
|       17 | 14028 | `	if( nArg > 0 ){` |
|        - | 14029 | `		const char *zErr;` |
|        - | 14030 | `		int nLen;` |
|        - | 14031 | `		/* Extract the error message */` |
|       14 | 14032 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 14033 | `		if( nArg > 1 ){` |
|        - | 14034 | `			/* Extract the error type */` |
|       14 | 14035 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       14 | 14036 | `			switch( nErr ){` |
|        1 | 14037 | `			case 1:   /* E_ERROR */` |
|        - | 14038 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 14039 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 14040 | `			case 256: /* E_USER_ERROR */` |
|        3 | 14041 | `				nErr = PH7_CTX_ERR;` |
|        3 | 14042 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 14043 | `				break;` |
|        1 | 14044 | `			case 2:   /* E_WARNING */` |
|        - | 14045 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 14046 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 14047 | `			case 512: /* E_USER_WARNING */` |
|        3 | 14048 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 14049 | `				break;` |
|        3 | 14050 | `			default:` |
|        9 | 14051 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 14052 | `				break;` |
|        - | 14053 | `			}` |
|        5 | 14054 | `		}` |
|        - | 14055 | `		/* Report error */` |
|       14 | 14056 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       14 | 14057 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 14058 | `			return rc;` |
|        - | 14059 | `		}` |
|        - | 14060 | `		/* Return true */` |
|       14 | 14061 | `		ph7_result_bool(pCtx,1);` |
|        9 | 14062 | `	}else{` |
|        - | 14063 | `		/* Missing arguments,return FALSE */` |
|        3 | 14064 | `		ph7_result_bool(pCtx,0);` |
|        - | 14065 | `	}` |
|       17 | 14066 | `	return rc;` |
|       11 | 14067 |  |
|        - | 14068 | `/*` |
|        - | 14069 | ` * int error_reporting([int $level])` |
|        - | 14070 | ` *  Sets which PHP errors are reported.` |
|        - | 14071 | ` * Parameters` |
|        - | 14072 | ` *  $level` |
|        - | 14073 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 14074 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 14075 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 14076 | ` *   levels will not always behave as expected.` |
|        - | 14077 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 14078 | ` *   in the predefined constants.` |
|        - | 14079 | ` * Return` |
|        - | 14080 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 14081 | ` *   parameter is given.` |
|        - | 14082 | ` */` |
|       32 | 14083 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 14084 |  |
|       37 | 14085 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14086 | `	int nOld;` |
|        - | 14087 | `	/* Extract the old reporting level */` |
|       37 | 14088 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       37 | 14089 | `	if( nArg > 0 ){` |
|        - | 14090 | `		int nNew;` |
|        - | 14091 | `		/* Extract the desired error reporting level */` |
|       31 | 14092 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       31 | 14093 | `		if( !nNew ){` |
|        - | 14094 | `			/* Do not report errors at all */` |
|        5 | 14095 | `			pVm->bErrReport = 0;` |
|        3 | 14096 | `		}else{` |
|        - | 14097 | `			/* Report all errors */` |
|       27 | 14098 | `			pVm->bErrReport = 1;` |
|        - | 14099 | `		}` |
|       13 | 14100 | `	}` |
|        - | 14101 | `	/* Return the old level */` |
|       37 | 14102 | `	ph7_result_int(pCtx,nOld);` |
|       37 | 14103 | `	return PH7_OK;` |
|        5 | 14104 |  |
|        - | 14105 | `/*` |
|        - | 14106 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 14107 | ` *  Send an error message somewhere.` |
|        - | 14108 | ` * Parameter` |
|        - | 14109 | ` *  $message` |
|        - | 14110 | ` *   The error message that should be logged.` |
|        - | 14111 | ` *  $message_type` |
|        - | 14112 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 14113 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 14114 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 14115 | ` *       This is the default option.` |
|        - | 14116 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 14117 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 14118 | ` *    2  No longer an option.` |
|        - | 14119 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 14120 | ` *       to the end of the message string.` |
|        - | 14121 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 14122 | ` *  $destination` |
|        - | 14123 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 14124 | ` *  $extra_headers` |
|        - | 14125 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 14126 | ` * Return` |
|        - | 14127 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14128 | ` * NOTE:` |
|        - | 14129 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 14130 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 14131 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 14132 | ` *  Otherwise this function is no-op.` |
|        - | 14133 | ` */` |
|        4 | 14134 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14135 |  |
|        - | 14136 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 14137 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 14138 | `	int iType = 0;` |
|        5 | 14139 | `	if( nArg < 1 ){` |
|        - | 14140 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 14141 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14142 | `		return PH7_OK;` |
|        - | 14143 | `	}` |
|        5 | 14144 | `	if( pVm->xErrLog  ){` |
|        - | 14145 | `		/* Invoke the user callback */` |
|      ! 0 | 14146 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 14147 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 14148 | `		if( nArg > 1 ){` |
|      ! 0 | 14149 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 14150 | `			if( nArg > 2 ){` |
|      ! 0 | 14151 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 14152 | `				if( nArg > 3 ){` |
|      ! 0 | 14153 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 14154 | `				}` |
|      ! 0 | 14155 | `			}` |
|      ! 0 | 14156 | `		}` |
|      ! 0 | 14157 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 14158 | `	}` |
|        - | 14159 | `	/* Retun TRUE */` |
|        5 | 14160 | `	ph7_result_bool(pCtx,1);` |
|        5 | 14161 | `	return PH7_OK;` |
|        3 | 14162 |  |
|        - | 14163 | `/*` |
|        - | 14164 | ` * bool restore_exception_handler(void)` |
|        - | 14165 | ` *  Restores the previously defined exception handler function.` |
|        - | 14166 | ` * Parameter` |
|        - | 14167 | ` *  None` |
|        - | 14168 | ` * Return` |
|        - | 14169 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 14170 | ` */` |
|        4 | 14171 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14172 |  |
|        5 | 14173 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14174 | `	ph7_value *pOld,*pNew;` |
|        - | 14175 | `	/* Point to the old and the new handler */` |
|        5 | 14176 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 14177 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 14178 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 14179 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 14180 | `		SXUNUSED(apArg);` |
|        - | 14181 | `		/* No installed handler,return FALSE */` |
|        5 | 14182 | `		ph7_result_bool(pCtx,0);` |
|        5 | 14183 | `		return PH7_OK;` |
|        - | 14184 | `	}` |
|        - | 14185 | `	/* Copy the old handler */` |
|      ! 0 | 14186 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 14187 | `	PH7_MemObjRelease(pOld);` |
|        - | 14188 | `	/* Return TRUE */` |
|      ! 0 | 14189 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 14190 | `	return PH7_OK;` |
|        3 | 14191 |  |
|        - | 14192 | `/*` |
|        - | 14193 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 14194 | ` *  Sets a user-defined exception handler function.` |
|        - | 14195 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 14196 | ` * NOTE` |
|        - | 14197 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 14198 | ` *  the satndard PHP engine.` |
|        - | 14199 | ` * Parameters` |
|        - | 14200 | ` *  $exception_handler` |
|        - | 14201 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 14202 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 14203 | ` *   that was thrown.` |
|        - | 14204 | ` *  Note:` |
|        - | 14205 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 14206 | ` * Return` |
|        - | 14207 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 14208 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 14209 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 14210 | ` */` |
|        4 | 14211 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14212 |  |
|        6 | 14213 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14214 | `	ph7_value *pOld,*pNew;` |
|        - | 14215 | `	/* Point to the old and the new handler */` |
|        6 | 14216 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 14217 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 14218 | `	/* Return the old handler */` |
|        6 | 14219 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 14220 | `	if( nArg > 0 ){` |
|        6 | 14221 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 14222 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 14223 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 14224 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 14225 | `		}else{` |
|        6 | 14226 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 14227 | `			/* Install the new handler */` |
|        6 | 14228 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 14229 | `		}` |
|        2 | 14230 | `	}` |
|        6 | 14231 | `	return PH7_OK;` |
|        2 | 14232 |  |
|        - | 14233 | `/*` |
|        - | 14234 | ` * bool restore_error_handler(void)` |
|        - | 14235 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 14236 | ` * Parameters:` |
|        - | 14237 | ` *  None.` |
|        - | 14238 | ` * Return` |
|        - | 14239 | ` *  Always TRUE.` |
|        - | 14240 | ` */` |
|        6 | 14241 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14242 |  |
|        8 | 14243 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14244 | `	ph7_value *pOld,*pNew;` |
|        - | 14245 | `	/* Point to the old and the new handler */` |
|        8 | 14246 | `	pOld = &pVm->aErrCB[0];` |
|        8 | 14247 | `	pNew = &pVm->aErrCB[1];` |
|        8 | 14248 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 14249 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 14250 | `		SXUNUSED(apArg);` |
|        - | 14251 | `		/* No installed callback,return FALSE */` |
|        8 | 14252 | `		ph7_result_bool(pCtx,0);` |
|        8 | 14253 | `		return PH7_OK;` |
|        - | 14254 | `	}` |
|        - | 14255 | `	/* Copy the old callback */` |
|      ! 0 | 14256 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 14257 | `	PH7_MemObjRelease(pOld);` |
|        - | 14258 | `	/* Return TRUE */` |
|      ! 0 | 14259 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 14260 | `	return PH7_OK;` |
|        5 | 14261 |  |
|        - | 14262 | `/*` |
|        - | 14263 | ` * value set_error_handler(callable $error_handler)` |
|        - | 14264 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 14265 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 14266 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 14267 | ` *  Sets a user-defined error handler function.` |
|        - | 14268 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 14269 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 14270 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 14271 | ` *  conditions (using trigger_error()).` |
|        - | 14272 | ` * Parameters` |
|        - | 14273 | ` *  $error_handler` |
|        - | 14274 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 14275 | ` *   describing the error.` |
|        - | 14276 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 14277 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 14278 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 14279 | ` *   The function can be shown as:` |
|        - | 14280 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 14281 | ` *     errno` |
|        - | 14282 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 14283 | ` *   errstr` |
|        - | 14284 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 14285 | ` *   errfile` |
|        - | 14286 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 14287 | ` *     was raised in, as a string.` |
|        - | 14288 | ` *  Note:` |
|        - | 14289 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 14290 | ` * Return` |
|        - | 14291 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 14292 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 14293 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 14294 | ` */` |
|    11084 | 14295 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 14296 |  |
|    11087 | 14297 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14298 | `	ph7_value *pOld,*pNew;` |
|        - | 14299 | `	/* Point to the old and the new handler */` |
|    11087 | 14300 | `	pOld = &pVm->aErrCB[0];` |
|    11087 | 14301 | `	pNew = &pVm->aErrCB[1];` |
|        - | 14302 | `	/* Return the old handler */` |
|    11087 | 14303 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    11087 | 14304 | `	if( nArg > 0 ){` |
|    11087 | 14305 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 14306 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5537 | 14307 | `			PH7_MemObjRelease(pNew);` |
|     5537 | 14308 | `			ph7_result_bool(pCtx,1);` |
|     2769 | 14309 | `		}else{` |
|     5551 | 14310 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 14311 | `			/* Install the new handler */` |
|     5551 | 14312 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 14313 | `		}` |
|     5542 | 14314 | `	}` |
|    11087 | 14315 | `	return PH7_OK;` |
|        3 | 14316 |  |
|        - | 14317 | `/*` |
|        - | 14318 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 14319 | ` *  Generates a backtrace.` |
|        - | 14320 | ` * Paramaeter` |
|        - | 14321 | ` *  $options` |
|        - | 14322 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 14323 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 14324 | ` *   all the function/method arguments, to save memory.` |
|        - | 14325 | ` * $limit` |
|        - | 14326 | ` *   (Not Used)` |
|        - | 14327 | ` * Return` |
|        - | 14328 | ` *  An array.The possible returned elements are as follows:` |
|        - | 14329 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 14330 | ` *          Name        Type      Description` |
|        - | 14331 | ` *          ------      ------     -----------` |
|        - | 14332 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 14333 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 14334 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 14335 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 14336 | ` *          object      object    The current object.` |
|        - | 14337 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 14338 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 14339 | ` */` |
|     1032 | 14340 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 14341 |  |
|     1037 | 14342 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14343 | `	ph7_value *pArray;` |
|        - | 14344 | `	ph7_class *pClass;` |
|        - | 14345 | `	ph7_value *pValue;` |
|        - | 14346 | `	SyString *pFile;` |
|        - | 14347 | `	/* Create a new array */` |
|     1037 | 14348 | `	pArray = ph7_context_new_array(pCtx);` |
|     1037 | 14349 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     1037 | 14350 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14351 | `		/* Out of memory,return NULL */` |
|      ! 0 | 14352 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 14353 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14354 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14355 | `		SXUNUSED(apArg);` |
|      ! 0 | 14356 | `		return PH7_OK;` |
|        - | 14357 | `	}` |
|        - | 14358 | `	/* Dump running function name and it's arguments  */` |
|     1037 | 14359 | `	if( pVm->pFrame->pParent ){` |
|     1037 | 14360 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 14361 | `		ph7_vm_func *pFunc;` |
|        - | 14362 | `		ph7_value *pArg;` |
|     1037 | 14363 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     1037 | 14364 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     1037 | 14365 | `		if( pFrame->pParent && pFunc ){` |
|     1037 | 14366 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|     1037 | 14367 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|     1037 | 14368 | `			ph7_value_reset_string_cursor(pValue);` |
|      516 | 14369 | `		}` |
|        - | 14370 | `		/* Function arguments */` |
|     1037 | 14371 | `		pArg = ph7_context_new_array(pCtx);` |
|     1037 | 14372 | `		if( pArg  ){` |
|        - | 14373 | `			ph7_value *pObj;` |
|        - | 14374 | `			VmSlot *aSlot;` |
|        - | 14375 | `			sxu32 n;` |
|        - | 14376 | `			/* Start filling the array with the given arguments */` |
|     1037 | 14377 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     4137 | 14378 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     3105 | 14379 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     3105 | 14380 | `				if( pObj ){` |
|     3105 | 14381 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1550 | 14382 | `				}` |
|     1555 | 14383 | `			}` |
|        - | 14384 | `			/* Save the array */` |
|     1037 | 14385 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      516 | 14386 | `		}` |
|      516 | 14387 | `	}` |
|     1037 | 14388 | `	ph7_value_int(pValue,1);` |
|        - | 14389 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 14390 | `	 * line numbers at run-time. )` |
|        - | 14391 | `	 */` |
|     1037 | 14392 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 14393 | `	/* Current processed script */` |
|     1037 | 14394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     1037 | 14395 | `	if( pFile ){` |
|     1037 | 14396 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|     1037 | 14397 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|     1037 | 14398 | `		ph7_value_reset_string_cursor(pValue);` |
|      516 | 14399 | `	}` |
|        - | 14400 | `	/* Top class */` |
|     1037 | 14401 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     1037 | 14402 | `	if( pClass ){` |
|     1033 | 14403 | `		ph7_value_reset_string_cursor(pValue);` |
|     1033 | 14404 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|     1033 | 14405 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      514 | 14406 | `	}` |
|        - | 14407 | `	/* Return the freshly created array */` |
|     1037 | 14408 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14409 | `	/*` |
|        - | 14410 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 14411 | `	 * as soon we return from this function.` |
|        - | 14412 | `	 */` |
|     1037 | 14413 | `	return PH7_OK;` |
|      521 | 14414 |  |
|        - | 14415 | `/*` |
|        - | 14416 | ` * Generate a small backtrace.` |
|        - | 14417 | ` * Store the generated dump in the given BLOB` |
|        - | 14418 | ` */` |
|        4 | 14419 | `static int VmMiniBacktrace(` |
|        - | 14420 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 14421 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 14422 | `	)` |
|        1 | 14423 |  |
|        5 | 14424 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14425 | `	ph7_vm_func *pFunc;` |
|        - | 14426 | `	ph7_class *pClass;` |
|        - | 14427 | `	SyString *pFile;` |
|        - | 14428 | `	/* Called function */` |
|        5 | 14429 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 14430 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 14431 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 14432 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 14433 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 14434 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 14435 | `	}else{` |
|      ! 0 | 14436 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 14437 | `	}` |
|        5 | 14438 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 14439 | `	/* Current processed script */` |
|        5 | 14440 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 14441 | `	if( pFile ){` |
|        5 | 14442 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 14443 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 14444 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 14445 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 14446 | `	}` |
|        - | 14447 | `	/* Top class */` |
|        5 | 14448 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 14449 | `	if( pClass ){` |
|      ! 0 | 14450 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 14451 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 14452 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 14453 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 14454 | `	}` |
|        5 | 14455 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 14456 | `	/* All done */` |
|        5 | 14457 | `	return SXRET_OK;` |
|        1 | 14458 |  |
|        - | 14459 | `/*` |
|        - | 14460 | ` * void debug_print_backtrace()` |
|        - | 14461 | ` *  Prints a backtrace` |
|        - | 14462 | ` * Parameters` |
|        - | 14463 | ` * None` |
|        - | 14464 | ` * Return` |
|        - | 14465 | ` * NULL` |
|        - | 14466 | ` */` |
|        2 | 14467 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14468 |  |
|        3 | 14469 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14470 | `	SyBlob sDump;` |
|        3 | 14471 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 14472 | `	/* Generate the backtrace */` |
|        3 | 14473 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 14474 | `	/* Output backtrace */` |
|        3 | 14475 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 14476 | `	/* All done,cleanup */` |
|        3 | 14477 | `	SyBlobRelease(&sDump);` |
|        1 | 14478 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14479 | `	SXUNUSED(apArg);` |
|        3 | 14480 | `	return PH7_OK;` |
|        1 | 14481 |  |
|        - | 14482 | `/*` |
|        - | 14483 | ` * string debug_string_backtrace()` |
|        - | 14484 | ` *  Generate a backtrace` |
|        - | 14485 | ` * Parameters` |
|        - | 14486 | ` * None` |
|        - | 14487 | ` * Return` |
|        - | 14488 | ` *  A mini backtrace().` |
|        - | 14489 | ` * Note that this is a symisc extension.` |
|        - | 14490 | ` */` |
|        2 | 14491 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14492 |  |
|        3 | 14493 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14494 | `	SyBlob sDump;` |
|        3 | 14495 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 14496 | `	/* Generate the backtrace */` |
|        3 | 14497 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 14498 | `	/* Return the backtrace */` |
|        3 | 14499 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 14500 | `	/* All done,cleanup */` |
|        3 | 14501 | `	SyBlobRelease(&sDump);` |
|        1 | 14502 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14503 | `	SXUNUSED(apArg);` |
|        3 | 14504 | `	return PH7_OK;` |
|        1 | 14505 |  |
|        - | 14506 | `/*` |
|        - | 14507 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 14508 | ` * exception is triggered.` |
|        - | 14509 | ` */` |
|      538 | 14510 | `static sxi32 VmUncaughtException(` |
|        - | 14511 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 14512 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 14513 | `	)` |
|        4 | 14514 |  |
|        - | 14515 | `	ph7_value *apArg[2],sArg;` |
|      542 | 14516 | `	int nArg = 1;` |
|        - | 14517 | `	sxi32 rc;` |
|      542 | 14518 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 14519 | `		/* Nesting limit reached */` |
|      ! 0 | 14520 | `		return SXRET_OK;` |
|        - | 14521 | `	}` |
|        - | 14522 | `	/* Call any exception handler if available */` |
|      542 | 14523 | `	PH7_MemObjInit(pVm,&sArg);` |
|      542 | 14524 | `	if( pThis ){` |
|        - | 14525 | `		/* Load the exception instance */` |
|      542 | 14526 | `		sArg.x.pOther = pThis;` |
|      542 | 14527 | `		pThis->iRef++;` |
|      542 | 14528 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      273 | 14529 | `	}else{` |
|      ! 0 | 14530 | `		nArg = 0;` |
|        - | 14531 | `	}` |
|      542 | 14532 | `	apArg[0] = &sArg;` |
|        - | 14533 | `	/* Call the exception handler if available */` |
|      542 | 14534 | `	pVm->nExceptDepth++;` |
|      542 | 14535 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      542 | 14536 | `	pVm->nExceptDepth--;` |
|      542 | 14537 | `	if( rc != SXRET_OK ){` |
|        - | 14538 | `		SyBlob sMsgBuf;` |
|      540 | 14539 | `		const char *zClass = "Exception";` |
|      540 | 14540 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 14541 | `		const char *zMsg;` |
|        - | 14542 | `		sxu32 nMsg;` |
|        - | 14543 | `		const char *zFuncName;` |
|        - | 14544 | `		int nFuncLen;` |
|      540 | 14545 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      540 | 14546 | `		if( pThis ){` |
|        - | 14547 | `			ph7_class_method *pGetMessage;` |
|        - | 14548 | `			ph7_value sMsg;` |
|        - | 14549 | `			const char *zTmp;` |
|        - | 14550 | `			int nTmp;` |
|      540 | 14551 | `			zClass = pThis->pClass->sName.zString;` |
|      540 | 14552 | `			nClass = pThis->pClass->sName.nByte;` |
|      540 | 14553 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      540 | 14554 | `			if( pGetMessage ){` |
|      540 | 14555 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      540 | 14556 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      540 | 14557 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      540 | 14558 | `					if( zTmp && nTmp > 0 ){` |
|      540 | 14559 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      268 | 14560 | `					}` |
|      268 | 14561 | `				}` |
|      540 | 14562 | `				PH7_MemObjRelease(&sMsg);` |
|      268 | 14563 | `			}` |
|      268 | 14564 | `		}` |
|      540 | 14565 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      540 | 14566 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      540 | 14567 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      540 | 14568 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      540 | 14569 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 14570 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      540 | 14571 | `		rc = SXERR_ABORT;` |
|      268 | 14572 | `	}` |
|      542 | 14573 | `	PH7_MemObjRelease(&sArg);` |
|      542 | 14574 | `	return rc;` |
|      273 | 14575 |  |
|        - | 14576 | `/*` |
|        - | 14577 | ` * Throw a user exception.` |
|        - | 14578 | ` *` |
|        - | 14579 | ` * Exception dispatch follows this sequence:` |
|        - | 14580 | ` *` |
|        - | 14581 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 14582 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 14583 | ` *` |
|        - | 14584 | ` * 2. If NO catch matches:` |
|        - | 14585 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 14586 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 14587 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 14588 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 14589 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 14590 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 14591 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 14592 | ` *` |
|        - | 14593 | ` * 3. If a catch DOES match:` |
|        - | 14594 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 14595 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 14596 | ` *       inside the catch body from immediately propagating past our` |
|        - | 14597 | ` *       finally block.` |
|        - | 14598 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 14599 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 14600 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 14601 | ` *       in pPendingException (step 2c).` |
|        - | 14602 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 14603 | ` *    d. Run finally (if present).` |
|        - | 14604 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 14605 | ` *       that handlers are restored and finally has run.` |
|        - | 14606 | ` */` |
|      962 | 14607 | `static sxi32 VmThrowException(` |
|        - | 14608 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 14609 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 14610 | `	)` |
|        5 | 14611 |  |
|        - | 14612 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 14613 | `	ph7_exception **apException;` |
|        - | 14614 | `	ph7_exception *pException;` |
|        - | 14615 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 14616 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 14617 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      967 | 14618 | `	VmCoalesceDisarm(pVm);` |
|        - | 14619 | `	/* A fresh throw supersedes any pending catch/finally return (PHP: an` |
|        - | 14620 | ``	 * exception thrown in a catch/finally discards an earlier `return`). */`` |
|      967 | 14621 | `	if( pVm->bReturnRequested ){` |
|      ! 0 | 14622 | `		pVm->bReturnRequested = 0;` |
|      ! 0 | 14623 | `		PH7_MemObjRelease(&pVm->sCatchReturn);` |
|      ! 0 | 14624 | `	}` |
|        - | 14625 | `	/* Point to the stack of loaded exceptions */` |
|      967 | 14626 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      967 | 14627 | `	pException = 0;` |
|      967 | 14628 | `	pCatch = 0;` |
|      967 | 14629 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14630 | `		ph7_exception_block *aCatch;` |
|        - | 14631 | `		ph7_class *pClass;` |
|        - | 14632 | `		SyString *aNames;` |
|        - | 14633 | `		sxu32 nNames;` |
|        - | 14634 | `		int matched;` |
|        - | 14635 | `		sxu32 j,k;` |
|        - | 14636 | `		/* Locate the appropriate block to execute */` |
|      419 | 14637 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      419 | 14638 | `		(void)SySetPop(&pVm->aException);` |
|      419 | 14639 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      427 | 14640 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 14641 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      425 | 14642 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      425 | 14643 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      425 | 14644 | `			matched = 0;` |
|      451 | 14645 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 14646 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 14647 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 14648 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      443 | 14649 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      443 | 14650 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 14651 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 14652 | `					continue;` |
|        - | 14653 | `				}` |
|      443 | 14654 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      417 | 14655 | `					matched = 1;` |
|      417 | 14656 | `					break;` |
|        - | 14657 | `				}` |
|       14 | 14658 | `			}` |
|      425 | 14659 | `			if( matched ){` |
|        - | 14660 | `				/* Catch block found,break immediately */` |
|      417 | 14661 | `				pCatch = &aCatch[j];` |
|      417 | 14662 | `				break;` |
|        - | 14663 | `			}` |
|        5 | 14664 | `		}` |
|      207 | 14665 | `	}` |
|        - | 14666 | `	/* Execute the cached block if available */` |
|      967 | 14667 | `	if( pCatch == 0 ){` |
|        - | 14668 | `		sxi32 rc;` |
|        - | 14669 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      555 | 14670 | `		if( pException && pException->iHasFinally ){` |
|        3 | 14671 | `			pException->iFinallyDone = 1;` |
|        3 | 14672 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0,TRUE);` |
|        3 | 14673 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 14674 | `				return SXERR_ABORT;` |
|        - | 14675 | `			}` |
|        1 | 14676 | `		}` |
|        - | 14677 | `		/* Check if there is an outer exception handler on the stack */` |
|      555 | 14678 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14679 | `			/* Re-throw to the outer handler */` |
|        3 | 14680 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 14681 | `		}` |
|        - | 14682 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 14683 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 14684 | `		 * exception instead of reporting it uncaught.` |
|        - | 14685 | `		 */` |
|      553 | 14686 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 14687 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 14688 | `			 * by looking for a catch frame on the stack.` |
|        - | 14689 | `			 */` |
|      553 | 14690 | `			VmFrame *pF = pVm->pFrame;` |
|      553 | 14691 | `			int inCatch = 0;` |
|     1113 | 14692 | `			while( pF ){` |
|      575 | 14693 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|       11 | 14694 | `					inCatch = 1;` |
|       11 | 14695 | `					break;` |
|        - | 14696 | `				}` |
|      564 | 14697 | `				pF = pF->pParent;` |
|        4 | 14698 | `			}` |
|      553 | 14699 | `			if( inCatch ){` |
|        - | 14700 | `				/* Defer — will be re-thrown after finally runs */` |
|       11 | 14701 | `				pThis->iRef++;` |
|       11 | 14702 | `				pVm->pPendingException = pThis;` |
|       11 | 14703 | `				return SXRET_OK;` |
|        - | 14704 | `			}` |
|      269 | 14705 | `		}` |
|        - | 14706 | `		/* Truly uncaught */` |
|      542 | 14707 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      542 | 14708 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 14709 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 14710 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 14711 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 14712 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 14713 | `			}` |
|      ! 0 | 14714 | `		}` |
|      542 | 14715 | `		return rc;` |
|      ! 0 | 14716 | `	}else{` |
|      417 | 14717 | `		VmFrame *pFrame = pVm->pFrame;` |
|      417 | 14718 | `		ph7_exception **apSaved = 0;` |
|        - | 14719 | `		sxu32 nSavedCount;` |
|        - | 14720 | `		sxi32 rc;` |
|      417 | 14721 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      417 | 14722 | `		if( pException->pFrame == pFrame ){` |
|      301 | 14723 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      148 | 14724 | `		}` |
|        - | 14725 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 14726 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 14727 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 14728 | `		 */` |
|      417 | 14729 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      417 | 14730 | `		if( nSavedCount > 0 ){` |
|       22 | 14731 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        7 | 14732 | `				nSavedCount * sizeof(ph7_exception *));` |
|       15 | 14733 | `			if( apSaved ){` |
|       22 | 14734 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        7 | 14735 | `					nSavedCount * sizeof(ph7_exception *));` |
|       15 | 14736 | `				SySetReset(&pVm->aException);` |
|        7 | 14737 | `			}` |
|        7 | 14738 | `		}` |
|        - | 14739 | `		/* Create the catch frame (made transparent below) */` |
|      417 | 14740 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      417 | 14741 | `		if( rc == SXRET_OK ){` |
|        - | 14742 | `			ph7_value *pObj;` |
|        - | 14743 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|        - | 14744 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|        - | 14745 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|        - | 14746 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|        - | 14747 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|        - | 14748 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|        - | 14749 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|        - | 14750 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
|      417 | 14751 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|      417 | 14752 | `			pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      417 | 14753 | `			if( pObj ){` |
|        - | 14754 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|        - | 14755 | `				 * so it may already hold a value from a prior catch or assignment.` |
|        - | 14756 | `				 * Pin the new instance, then release the slot's prior contents` |
|        - | 14757 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|        - | 14758 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|        - | 14759 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
|      417 | 14760 | `				pThis->iRef++;` |
|      417 | 14761 | `				PH7_MemObjRelease(pObj);` |
|      417 | 14762 | `				pObj->x.pOther = pThis;` |
|      417 | 14763 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      206 | 14764 | `			}` |
|        - | 14765 | `			/* Execute the catch block */` |
|      417 | 14766 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0,TRUE);` |
|        - | 14767 | `			/* Leave the frame */` |
|      417 | 14768 | `			VmLeaveFrame(&(*pVm));` |
|      206 | 14769 | `		}` |
|        - | 14770 | `		/* Restore the outer exception handlers */` |
|      417 | 14771 | `		if( apSaved ){` |
|        - | 14772 | `			sxu32 k;` |
|        - | 14773 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 14774 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 14775 | `			 * Restore the original outer entries.` |
|        - | 14776 | `			 */` |
|       15 | 14777 | `			SySetReset(&pVm->aException);` |
|       29 | 14778 | `			for(k = 0; k < nSavedCount; k++){` |
|       15 | 14779 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        8 | 14780 | `			}` |
|       15 | 14781 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        7 | 14782 | `		}` |
|        - | 14783 | `		/* Execute the finally block after catch */` |
|      417 | 14784 | `		if( pException->iHasFinally ){` |
|       25 | 14785 | `			pException->iFinallyDone = 1;` |
|        - | 14786 | `			{` |
|       25 | 14787 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0,TRUE);` |
|       25 | 14788 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 14789 | `					return SXERR_ABORT;` |
|        - | 14790 | `				}` |
|        - | 14791 | `			}` |
|       11 | 14792 | `		}` |
|      417 | 14793 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 14794 | `			return SXERR_ABORT;` |
|        - | 14795 | `		}` |
|        - | 14796 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 14797 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 14798 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|        - | 14799 | ``		 * unless the finally itself issued a `return`, which swallows the`` |
|        - | 14800 | `		 * in-flight exception (PHP semantics).` |
|        - | 14801 | `		 */` |
|      417 | 14802 | `		if( pVm->pPendingException ){` |
|       11 | 14803 | `			if( !pVm->bReturnRequested ){` |
|        9 | 14804 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 14805 | `				pVm->pPendingException = 0;` |
|        9 | 14806 | `				return VmThrowException(&(*pVm),pReThrow);` |
|        - | 14807 | `			}` |
|        - | 14808 | `			/* Swallowed by finally's return: drop the deferred exception. */` |
|        3 | 14809 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|        3 | 14810 | `			pVm->pPendingException = 0;` |
|        1 | 14811 | `		}` |
|        - | 14812 | `	}` |
|        - | 14813 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 14814 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 14815 | `	 */` |
|      409 | 14816 | `	return SXRET_OK;` |
|      486 | 14817 |  |
|        - | 14818 | `/*` |
|        - | 14819 | ` * Section:` |
|        - | 14820 | ` *  Version,Credits and Copyright related functions.` |
|        - | 14821 | ` * Status:` |
|        - | 14822 | ` *    Stable.` |
|        - | 14823 | ` */` |
|        - | 14824 | `/*` |
|        - | 14825 | ` * string ph7version(void)` |
|        - | 14826 | ` *  Returns the running version of the PH7 version.` |
|        - | 14827 | ` * Parameters` |
|        - | 14828 | ` *  None` |
|        - | 14829 | ` * Return` |
|        - | 14830 | ` * Current PH7 version.` |
|        - | 14831 | ` */` |
|        2 | 14832 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14833 |  |
|        1 | 14834 | `	SXUNUSED(nArg);` |
|        1 | 14835 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 14836 | `	/* Current engine version */` |
|        3 | 14837 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 14838 | `	return PH7_OK;` |
|        1 | 14839 |  |
|        - | 14840 | `/*` |
|        - | 14841 | ` * string phpversion([ string $extension ])` |
|        - | 14842 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 14843 | ` * Parameters` |
|        - | 14844 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 14845 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 14846 | ` * Return` |
|        - | 14847 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 14848 | ` */` |
|        4 | 14849 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14850 |  |
|        2 | 14851 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 14852 | `	if( nArg > 0 ){` |
|      ! 0 | 14853 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14854 | `		return PH7_OK;` |
|        - | 14855 | `	}` |
|        5 | 14856 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 14857 | `	return PH7_OK;` |
|        3 | 14858 |  |
|        - | 14859 | `/*` |
|        - | 14860 | ` * string php_sapi_name(void)` |
|        - | 14861 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 14862 | ` * Parameters` |
|        - | 14863 | ` *  None` |
|        - | 14864 | ` * Return` |
|        - | 14865 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 14866 | ` */` |
|        2 | 14867 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14868 |  |
|        3 | 14869 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 14870 | `	SXUNUSED(nArg);` |
|        1 | 14871 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 14872 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 14873 | `	return PH7_OK;` |
|        1 | 14874 |  |
|        - | 14875 | `/*` |
|        - | 14876 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 14877 | ` */` |
|        - | 14878 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 14879 | ` "<html><head>"\` |
|        - | 14880 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 14881 | ` "<style type=\"text/css\">"\` |
|        - | 14882 | ` "div {"\` |
|        - | 14883 | `     "border: 1px solid #cccccc;"\` |
|        - | 14884 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 14885 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 14886 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 14887 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 14888 | `     "-webkit-border-radius: 10px;"\` |
|        - | 14889 | `     "-o-border-radius: 10px;"\` |
|        - | 14890 | `     "border-radius: 10px;"\` |
|        - | 14891 | `     "padding-left: 2em;"\` |
|        - | 14892 | `     "background-color: white;"\` |
|        - | 14893 | `     "margin-left: auto;"\` |
|        - | 14894 | `     "font-family: verdana;"\` |
|        - | 14895 | `     "padding-right: 2em;"\` |
|        - | 14896 | `     "margin-right: auto;"\` |
|        - | 14897 | `     "}"\` |
|        - | 14898 | `     "body {"\` |
|        - | 14899 | `     "padding: 0.2em;"\` |
|        - | 14900 | `     "font-style: normal;"\` |
|        - | 14901 | `     "font-size: medium;"\` |
|        - | 14902 | `     "background-color: #f2f2f2;"\` |
|        - | 14903 | `     "}"\` |
|        - | 14904 | `     "hr {"\` |
|        - | 14905 | `     "border-style: solid none none;"\` |
|        - | 14906 | `     "border-width: 1px medium medium;"\` |
|        - | 14907 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 14908 | `     "height: 1px;"\` |
|        - | 14909 | `     "}"\` |
|        - | 14910 | `     "a {"\` |
|        - | 14911 | `     "color: #3366cc;"\` |
|        - | 14912 | `     "text-decoration: none;"\` |
|        - | 14913 | `     "}"\` |
|        - | 14914 | `     "a:hover {"\` |
|        - | 14915 | `     "color: #999999;"\` |
|        - | 14916 | `     "}"\` |
|        - | 14917 | `     "a:active {"\` |
|        - | 14918 | `     "color: #663399;"\` |
|        - | 14919 | `     "}"\` |
|        - | 14920 | `     "h1 {"\` |
|        - | 14921 | `     "margin: 0;"\` |
|        - | 14922 | `     "padding: 0;"\` |
|        - | 14923 | `     "font-family: Verdana;"\` |
|        - | 14924 | `     "font-weight: bold;"\` |
|        - | 14925 | `     "font-style: normal;"\` |
|        - | 14926 | `     "font-size: medium;"\` |
|        - | 14927 | `     "text-transform: capitalize;"\` |
|        - | 14928 | `     "color: #0a328c;"\` |
|        - | 14929 | `     "}"\` |
|        - | 14930 | `     "p {"\` |
|        - | 14931 | `     "margin: 0 auto;"\` |
|        - | 14932 | `     "font-size: medium;"\` |
|        - | 14933 | `     "font-style: normal;"\` |
|        - | 14934 | `     "font-family: verdana;"\` |
|        - | 14935 | `     "}"\` |
|        - | 14936 | `"</style></head><body>"\` |
|        - | 14937 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 14938 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 14939 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 14940 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 14941 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 14942 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 14943 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 14944 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 14945 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 14946 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 14947 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 14948 |  |
|        - | 14949 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14950 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 14951 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 14952 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 14953 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14954 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 14955 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14956 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 14957 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14958 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 14959 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14960 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 14961 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 14962 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 14963 |  |
|        - | 14964 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 14965 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 14966 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 14967 | `"&nbsp;*<br>"\` |
|        - | 14968 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 14969 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 14970 | `"&nbsp;* are met:<br>"\` |
|        - | 14971 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 14972 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 14973 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 14974 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 14975 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 14976 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 14977 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 14978 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 14979 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 14980 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 14981 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 14982 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 14983 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 14984 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 14985 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 14986 | `"&nbsp;*<br>"\` |
|        - | 14987 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 14988 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 14989 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 14990 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 14991 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 14992 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 14993 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 14994 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 14995 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 14996 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 14997 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 14998 | `"&nbsp;*/<br>"\` |
|        - | 14999 | `"</span></small></small></p>"\` |
|        - | 15000 | `"</div></body></html>"` |
|        - | 15001 | `/*` |
|        - | 15002 | ` * bool ph7credits(void)` |
|        - | 15003 | ` * bool ph7info(void)` |
|        - | 15004 | ` * bool ph7copyright(void)` |
|        - | 15005 | ` *  Prints out the credits for PH7 engine` |
|        - | 15006 | ` * Parameters` |
|        - | 15007 | ` *  None` |
|        - | 15008 | ` * Return` |
|        - | 15009 | ` *  Always TRUE` |
|        - | 15010 | ` */` |
|        2 | 15011 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15012 |  |
|        3 | 15013 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 15014 | `	/* Expand the HTML page above*/` |
|        3 | 15015 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 15016 | `	ph7_context_output_format(` |
|        1 | 15017 | `		pCtx,` |
|        - | 15018 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 15019 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 15020 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 15021 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 15022 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 15023 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 15024 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 15025 | `#ifdef __WINNT__` |
|        - | 15026 | `		"Windows NT"` |
|        - | 15027 | `#elif defined(__UNIXES__)` |
|        - | 15028 | `		"UNIX-Like"` |
|        - | 15029 | `#else` |
|        - | 15030 | `		"Other OS"` |
|        - | 15031 | `#endif` |
|        - | 15032 | `		);` |
|        3 | 15033 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 15034 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 15035 | `	SXUNUSED(apArg);` |
|        - | 15036 | `	/* Return TRUE */` |
|        - | 15037 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 15038 | `	return PH7_OK;` |
|        1 | 15039 |  |
|        - | 15040 | `/*` |
|        - | 15041 | ` * Section:` |
|        - | 15042 | ` *    URL related routines.` |
|        - | 15043 | ` * Status:` |
|        - | 15044 | ` *    Stable.` |
|        - | 15045 | ` */` |
|        - | 15046 | `/*` |
|        - | 15047 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 15048 | ` *  Parse a URL and return its fields.` |
|        - | 15049 | ` * Parameters` |
|        - | 15050 | ` *  $url` |
|        - | 15051 | ` *   The URL to parse.` |
|        - | 15052 | ` * $component` |
|        - | 15053 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 15054 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 15055 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 15056 | ` *  in which case the return value will be an integer).` |
|        - | 15057 | ` * Return` |
|        - | 15058 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 15059 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 15060 | ` *  this array are:` |
|        - | 15061 | ` *   scheme - e.g. http` |
|        - | 15062 | ` *   host` |
|        - | 15063 | ` *   port` |
|        - | 15064 | ` *   user` |
|        - | 15065 | ` *   pass` |
|        - | 15066 | ` *   path` |
|        - | 15067 | ` *   query - after the question mark ?` |
|        - | 15068 | ` *   fragment - after the hashmark #` |
|        - | 15069 | ` * Note:` |
|        - | 15070 | ` *  FALSE is returned on failure.` |
|        - | 15071 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 15072 | ` *  with the standard PHP engine.` |
|        - | 15073 | ` */` |
|       28 | 15074 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15075 |  |
|        - | 15076 | `	const char *zStr; /* Input string */` |
|        - | 15077 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 15078 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 15079 | `	int nLen;` |
|        - | 15080 | `	sxi32 rc;` |
|       29 | 15081 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 15082 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 15083 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15084 | `		return PH7_OK;` |
|        - | 15085 | `	}` |
|        - | 15086 | `	/* Extract the given URI */` |
|       29 | 15087 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 15088 | `	if( nLen < 1 ){` |
|        - | 15089 | `		/* Nothing to process,return FALSE */` |
|        3 | 15090 | `		ph7_result_bool(pCtx,0);` |
|        3 | 15091 | `		return PH7_OK;` |
|        - | 15092 | `	}` |
|        - | 15093 | `	/* Get a parse */` |
|       27 | 15094 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 15095 | `	if( rc != SXRET_OK ){` |
|        - | 15096 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 15097 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15098 | `		return PH7_OK;` |
|        - | 15099 | `	}` |
|       27 | 15100 | `	if( nArg > 1 ){` |
|      ! 0 | 15101 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 15102 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 15103 | `		switch(nComponent){` |
|      ! 0 | 15104 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 15105 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 15106 | `			if( pComp->nByte < 1 ){` |
|        - | 15107 | `				/* No available value,return NULL */` |
|      ! 0 | 15108 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15109 | `			}else{` |
|      ! 0 | 15110 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15111 | `			}` |
|      ! 0 | 15112 | `			break;` |
|      ! 0 | 15113 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 15114 | `			pComp = &sURI.sHost;` |
|      ! 0 | 15115 | `			if( pComp->nByte < 1 ){` |
|        - | 15116 | `				/* No available value,return NULL */` |
|      ! 0 | 15117 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15118 | `			}else{` |
|      ! 0 | 15119 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15120 | `			}` |
|      ! 0 | 15121 | `			break;` |
|      ! 0 | 15122 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 15123 | `			pComp = &sURI.sPort;` |
|      ! 0 | 15124 | `			if( pComp->nByte < 1 ){` |
|        - | 15125 | `				/* No available value,return NULL */` |
|      ! 0 | 15126 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15127 | `			}else{` |
|      ! 0 | 15128 | `				int iPort = 0;` |
|        - | 15129 | `				/* Cast the value to integer */` |
|      ! 0 | 15130 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 15131 | `				ph7_result_int(pCtx,iPort);` |
|        - | 15132 | `			}` |
|      ! 0 | 15133 | `			break;` |
|      ! 0 | 15134 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 15135 | `			pComp = &sURI.sUser;` |
|      ! 0 | 15136 | `			if( pComp->nByte < 1 ){` |
|        - | 15137 | `				/* No available value,return NULL */` |
|      ! 0 | 15138 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15139 | `			}else{` |
|      ! 0 | 15140 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15141 | `			}` |
|      ! 0 | 15142 | `			break;` |
|      ! 0 | 15143 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 15144 | `			pComp = &sURI.sPass;` |
|      ! 0 | 15145 | `			if( pComp->nByte < 1 ){` |
|        - | 15146 | `				/* No available value,return NULL */` |
|      ! 0 | 15147 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15148 | `			}else{` |
|      ! 0 | 15149 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15150 | `			}` |
|      ! 0 | 15151 | `			break;` |
|      ! 0 | 15152 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 15153 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 15154 | `			if( pComp->nByte < 1 ){` |
|        - | 15155 | `				/* No available value,return NULL */` |
|      ! 0 | 15156 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15157 | `			}else{` |
|      ! 0 | 15158 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15159 | `			}` |
|      ! 0 | 15160 | `			break;` |
|      ! 0 | 15161 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 15162 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 15163 | `			if( pComp->nByte < 1 ){` |
|        - | 15164 | `				/* No available value,return NULL */` |
|      ! 0 | 15165 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15166 | `			}else{` |
|      ! 0 | 15167 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15168 | `			}` |
|      ! 0 | 15169 | `			break;` |
|      ! 0 | 15170 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 15171 | `			pComp = &sURI.sPath;` |
|      ! 0 | 15172 | `			if( pComp->nByte < 1 ){` |
|        - | 15173 | `				/* No available value,return NULL */` |
|      ! 0 | 15174 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15175 | `			}else{` |
|      ! 0 | 15176 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15177 | `			}` |
|      ! 0 | 15178 | `			break;` |
|      ! 0 | 15179 | `		default:` |
|        - | 15180 | `			/* No such entry,return NULL */` |
|      ! 0 | 15181 | `			ph7_result_null(pCtx);` |
|      ! 0 | 15182 | `			break;` |
|        - | 15183 | `		}` |
|      ! 0 | 15184 | `	}else{` |
|        - | 15185 | `		ph7_value *pArray,*pValue;` |
|        - | 15186 | `		/* Return an associative array */` |
|       27 | 15187 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 15188 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 15189 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 15190 | `			/* Out of memory */` |
|      ! 0 | 15191 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 15192 | `			/* Return false */` |
|      ! 0 | 15193 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 15194 | `			return PH7_OK;` |
|        - | 15195 | `		}` |
|        - | 15196 | `		/* Fill the array */` |
|       27 | 15197 | `		pComp = &sURI.sScheme;` |
|       27 | 15198 | `		if( pComp->nByte > 0 ){` |
|       19 | 15199 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 15200 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 15201 | `		}` |
|        - | 15202 | `		/* Reset the string cursor */` |
|       27 | 15203 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15204 | `		pComp = &sURI.sHost;` |
|       27 | 15205 | `		if( pComp->nByte > 0 ){` |
|       25 | 15206 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 15207 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 15208 | `		}` |
|        - | 15209 | `		/* Reset the string cursor */` |
|       27 | 15210 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15211 | `		pComp = &sURI.sPort;` |
|       27 | 15212 | `		if( pComp->nByte > 0 ){` |
|       11 | 15213 | `			int iPort = 0;/* cc warning */` |
|        - | 15214 | `			/* Convert to integer */` |
|       11 | 15215 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 15216 | `			ph7_value_int(pValue,iPort);` |
|       11 | 15217 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 15218 | `		}` |
|        - | 15219 | `		/* Reset the string cursor */` |
|       27 | 15220 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15221 | `		pComp = &sURI.sUser;` |
|       27 | 15222 | `		if( pComp->nByte > 0 ){` |
|        7 | 15223 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 15224 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 15225 | `		}` |
|        - | 15226 | `		/* Reset the string cursor */` |
|       27 | 15227 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15228 | `		pComp = &sURI.sPass;` |
|       27 | 15229 | `		if( pComp->nByte > 0 ){` |
|        7 | 15230 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 15231 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 15232 | `		}` |
|        - | 15233 | `		/* Reset the string cursor */` |
|       27 | 15234 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15235 | `		pComp = &sURI.sPath;` |
|       27 | 15236 | `		if( pComp->nByte > 0 ){` |
|       17 | 15237 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 15238 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 15239 | `		}` |
|        - | 15240 | `		/* Reset the string cursor */` |
|       27 | 15241 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15242 | `		pComp = &sURI.sQuery;` |
|       27 | 15243 | `		if( pComp->nByte > 0 ){` |
|        5 | 15244 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 15245 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 15246 | `		}` |
|        - | 15247 | `		/* Reset the string cursor */` |
|       27 | 15248 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15249 | `		pComp = &sURI.sFragment;` |
|       27 | 15250 | `		if( pComp->nByte > 0 ){` |
|        5 | 15251 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 15252 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 15253 | `		}` |
|        - | 15254 | `		/* Return the created array */` |
|       27 | 15255 | `		ph7_result_value(pCtx,pArray);` |
|        - | 15256 | `		/* NOTE:` |
|        - | 15257 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 15258 | `		 * automatically as soon we return from this function.` |
|        - | 15259 | `		 */` |
|        - | 15260 | `	}` |
|        - | 15261 | `	/* All done */` |
|       27 | 15262 | `	return PH7_OK;` |
|       15 | 15263 |  |
|        - | 15264 | `/*` |
|        - | 15265 | ` * Section:` |
|        - | 15266 | ` *   Array related routines.` |
|        - | 15267 | ` * Status:` |
|        - | 15268 | ` *    Stable.` |
|        - | 15269 | ` * Note 2012-5-21 01:04:15:` |
|        - | 15270 | ` *  Array related functions that need access to the underlying` |
|        - | 15271 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 15272 | ` */` |
|        - | 15273 | `/*` |
|        - | 15274 | ` * The [compact()] function store it's state information in an instance` |
|        - | 15275 | ` * of the following structure.` |
|        - | 15276 | ` */` |
|        - | 15277 | `struct compact_data` |
|        - | 15278 |  |
|        - | 15279 | `	ph7_value *pArray;  /* Target array */` |
|        - | 15280 | `	int nRecCount;      /* Recursion count */` |
|        - | 15281 | `};` |
|        - | 15282 | `/*` |
|        - | 15283 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 15284 | ` */` |
|      ! 0 | 15285 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 15286 |  |
|      ! 0 | 15287 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 15288 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 15289 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 15290 | `	/* Act according to the hashmap value */` |
|      ! 0 | 15291 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 15292 | `		SyString sVar;` |
|      ! 0 | 15293 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 15294 | `		if( sVar.nByte > 0 ){` |
|        - | 15295 | `			/* Query the current frame */` |
|      ! 0 | 15296 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 15297 | `			/* ^` |
|        - | 15298 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 15299 | `			 */` |
|      ! 0 | 15300 | `			if( pKey ){` |
|        - | 15301 | `				/* Perform the insertion */` |
|      ! 0 | 15302 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 15303 | `			}` |
|      ! 0 | 15304 | `		}` |
|      ! 0 | 15305 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 15306 | `		int rc;` |
|        - | 15307 | `		/* Recursively traverse this array */` |
|      ! 0 | 15308 | `		pData->nRecCount++;` |
|      ! 0 | 15309 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 15310 | `		pData->nRecCount--;` |
|      ! 0 | 15311 | `		return rc;` |
|        - | 15312 | `	}` |
|      ! 0 | 15313 | `	return SXRET_OK;` |
|      ! 0 | 15314 |  |
|        - | 15315 | `/*` |
|        - | 15316 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 15317 | ` *  Create array containing variables and their values.` |
|        - | 15318 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 15319 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 15320 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 15321 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 15322 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 15323 | ` * Parameters` |
|        - | 15324 | ` *  $varname` |
|        - | 15325 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 15326 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 15327 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 15328 | ` *   it recursively.` |
|        - | 15329 | ` * Return` |
|        - | 15330 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 15331 | ` */` |
|        2 | 15332 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15333 |  |
|        - | 15334 | `	ph7_value *pArray,*pObj;` |
|        3 | 15335 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15336 | `	const char *zName;` |
|        - | 15337 | `	SyString sVar;` |
|        - | 15338 | `	int i,nLen;` |
|        3 | 15339 | `	if( nArg < 1 ){` |
|        - | 15340 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 15341 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15342 | `		return PH7_OK;` |
|        - | 15343 | `	}` |
|        - | 15344 | `	/* Create the array */` |
|        3 | 15345 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 15346 | `	if( pArray == 0 ){` |
|        - | 15347 | `		/* Out of memory */` |
|      ! 0 | 15348 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 15349 | `		/* Return NULL */` |
|      ! 0 | 15350 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15351 | `		return PH7_OK;` |
|        - | 15352 | `	}` |
|        - | 15353 | `	/* Perform the requested operation */` |
|        7 | 15354 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 15355 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 15356 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 15357 | `				struct compact_data sData;` |
|      ! 0 | 15358 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 15359 | `				/* Recursively walk the array */` |
|      ! 0 | 15360 | `				sData.nRecCount = 0;` |
|      ! 0 | 15361 | `				sData.pArray = pArray;` |
|      ! 0 | 15362 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 15363 | `			}` |
|      ! 0 | 15364 | `		}else{` |
|        - | 15365 | `			/* Extract variable name */` |
|        5 | 15366 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 15367 | `			if( nLen > 0 ){` |
|        5 | 15368 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 15369 | `				/* Check if the variable is available in the current frame */` |
|        5 | 15370 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 15371 | `				if( pObj ){` |
|        5 | 15372 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 15373 | `				}` |
|        2 | 15374 | `			}` |
|        - | 15375 | `		}` |
|        3 | 15376 | `	}` |
|        - | 15377 | `	/* Return the array */` |
|        3 | 15378 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 15379 | `	return PH7_OK;` |
|        2 | 15380 |  |
|        - | 15381 | `/*` |
|        - | 15382 | ` * The [extract()] function store it's state information in an instance` |
|        - | 15383 | ` * of the following structure.` |
|        - | 15384 | ` */` |
|        - | 15385 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 15386 | `struct extract_aux_data` |
|        - | 15387 |  |
|        - | 15388 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 15389 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 15390 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 15391 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 15392 | `	int iFlags;           /* Control flags */` |
|        - | 15393 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 15394 | `};` |
|        - | 15395 | `/* Forward declaration */` |
|        - | 15396 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 15397 | `/*` |
|        - | 15398 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 15399 | ` *   Import variables into the current symbol table from an array.` |
|        - | 15400 | ` * Parameters` |
|        - | 15401 | ` * $var_array` |
|        - | 15402 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 15403 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 15404 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 15405 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 15406 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 15407 | ` * $extract_type` |
|        - | 15408 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 15409 | ` *  It can be one of the following values:` |
|        - | 15410 | ` *   EXTR_OVERWRITE` |
|        - | 15411 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 15412 | ` *   EXTR_SKIP` |
|        - | 15413 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 15414 | ` *   EXTR_PREFIX_SAME` |
|        - | 15415 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 15416 | ` *   EXTR_PREFIX_ALL` |
|        - | 15417 | ` *       Prefix all variable names with prefix.` |
|        - | 15418 | ` *   EXTR_PREFIX_INVALID` |
|        - | 15419 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 15420 | ` *   EXTR_IF_EXISTS` |
|        - | 15421 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 15422 | ` *       otherwise do nothing.` |
|        - | 15423 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 15424 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 15425 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 15426 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 15427 | ` *      the current symbol table.` |
|        - | 15428 | ` * $prefix` |
|        - | 15429 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 15430 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 15431 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 15432 | ` *  underscore character.` |
|        - | 15433 | ` * Return` |
|        - | 15434 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 15435 | ` */` |
|        4 | 15436 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15437 |  |
|        - | 15438 | `	extract_aux_data sAux;` |
|        - | 15439 | `	ph7_hashmap *pMap;` |
|        5 | 15440 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 15441 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 15442 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 15443 | `		return PH7_OK;` |
|        - | 15444 | `	}` |
|        - | 15445 | `	/* Point to the target hashmap */` |
|        5 | 15446 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 15447 | `	if( pMap->nEntry < 1 ){` |
|        - | 15448 | `		/* Empty map,return  0 */` |
|      ! 0 | 15449 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 15450 | `		return PH7_OK;` |
|        - | 15451 | `	}` |
|        - | 15452 | `	/* Prepare the aux data */` |
|        5 | 15453 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 15454 | `	if( nArg > 1 ){` |
|        3 | 15455 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 15456 | `		if( nArg > 2 ){` |
|      ! 0 | 15457 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 15458 | `		}` |
|        1 | 15459 | `	}` |
|        5 | 15460 | `	sAux.pVm = pCtx->pVm;` |
|        - | 15461 | `	/* Invoke the worker callback */` |
|        5 | 15462 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 15463 | `	/* Number of variables successfully imported */` |
|        5 | 15464 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 15465 | `	return PH7_OK;` |
|        3 | 15466 |  |
|        - | 15467 | `/*` |
|        - | 15468 | ` * Worker callback for the [extract()] function defined` |
|        - | 15469 | ` * below.` |
|        - | 15470 | ` */` |
|        8 | 15471 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 15472 |  |
|        9 | 15473 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 15474 | `	int iFlags = pAux->iFlags;` |
|        9 | 15475 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 15476 | `	ph7_value *pObj;` |
|        - | 15477 | `	SyString sVar;` |
|        9 | 15478 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 15479 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 15480 | `	}` |
|        - | 15481 | `	/* Perform a string cast */` |
|        9 | 15482 | `	PH7_MemObjToString(pKey);` |
|        9 | 15483 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 15484 | `		/* Unavailable variable name */` |
|      ! 0 | 15485 | `		return SXRET_OK;` |
|        - | 15486 | `	}` |
|        9 | 15487 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 15488 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 15489 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 15490 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 15491 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15492 | `			);` |
|      ! 0 | 15493 | `	}else{` |
|       13 | 15494 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 15495 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 15496 | `	}` |
|        9 | 15497 | `	sVar.zString = pAux->zWorker;` |
|        - | 15498 | `	/* Try to extract the variable */` |
|        9 | 15499 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 15500 | `	if( pObj ){` |
|        - | 15501 | `		/* Collision */` |
|        5 | 15502 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 15503 | `			return SXRET_OK;` |
|        - | 15504 | `		}` |
|        5 | 15505 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 15506 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 15507 | `				/* Already prefixed */` |
|      ! 0 | 15508 | `				return SXRET_OK;` |
|        - | 15509 | `			}` |
|      ! 0 | 15510 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 15511 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 15512 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15513 | `				);` |
|      ! 0 | 15514 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 15515 | `		}` |
|        3 | 15516 | `	}else{` |
|        - | 15517 | `		/* Create the variable */` |
|        5 | 15518 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 15519 | `	}` |
|        9 | 15520 | `	if( pObj ){` |
|        - | 15521 | `		/* Overwrite the old value */` |
|        9 | 15522 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 15523 | `		/* Increment counter */` |
|        9 | 15524 | `		pAux->iCount++;` |
|        4 | 15525 | `	}` |
|        9 | 15526 | `	return SXRET_OK;` |
|        5 | 15527 |  |
|        - | 15528 | `/*` |
|        - | 15529 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 15530 | ` * defined below.` |
|        - | 15531 | ` */` |
|        2 | 15532 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 15533 |  |
|        3 | 15534 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 15535 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 15536 | `	ph7_value *pObj;` |
|        - | 15537 | `	SyString sVar;` |
|        - | 15538 | `	/* Perform a string cast */` |
|        3 | 15539 | `	PH7_MemObjToString(pKey);` |
|        3 | 15540 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 15541 | `		/* Unavailable variable name */` |
|      ! 0 | 15542 | `		return SXRET_OK;` |
|        - | 15543 | `	}` |
|        3 | 15544 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 15545 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 15546 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 15547 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 15548 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15549 | `			);` |
|        2 | 15550 | `	}else{` |
|      ! 0 | 15551 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 15552 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 15553 | `	}` |
|        3 | 15554 | `	sVar.zString = pAux->zWorker;` |
|        - | 15555 | `	/* Extract the variable */` |
|        3 | 15556 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 15557 | `	if( pObj ){` |
|        3 | 15558 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 15559 | `	}` |
|        3 | 15560 | `	return SXRET_OK;` |
|        2 | 15561 |  |
|        - | 15562 | `/*` |
|        - | 15563 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 15564 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 15565 | ` * Parameters` |
|        - | 15566 | ` * $types` |
|        - | 15567 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 15568 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 15569 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 15570 | ` *  POST includes the POST uploaded file information.` |
|        - | 15571 | ` *  Note:` |
|        - | 15572 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 15573 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 15574 | ` * $prefix` |
|        - | 15575 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 15576 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 15577 | ` *  variable named $pref_userid.` |
|        - | 15578 | ` * Return` |
|        - | 15579 | ` *  TRUE on success or FALSE on failure.` |
|        - | 15580 | ` */` |
|        2 | 15581 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15582 |  |
|        - | 15583 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 15584 | `	extract_aux_data sAux;` |
|        - | 15585 | `	int nLen,nPrefixLen;` |
|        - | 15586 | `	ph7_value *pSuper;` |
|        - | 15587 | `	ph7_vm *pVm;` |
|        - | 15588 | `	/* By default import only $_GET variables  */` |
|        3 | 15589 | `	zImport = "G";` |
|        3 | 15590 | `	nLen = (int)sizeof(char);` |
|        3 | 15591 | `	zPrefix = 0;` |
|        3 | 15592 | `	nPrefixLen = 0;` |
|        3 | 15593 | `	if( nArg > 0 ){` |
|        3 | 15594 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 15595 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 15596 | `		}` |
|        3 | 15597 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 15598 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 15599 | `		}` |
|        1 | 15600 | `	}` |
|        - | 15601 | `	/* Point to the underlying VM */` |
|        3 | 15602 | `	pVm = pCtx->pVm;` |
|        - | 15603 | `	/* Initialize the aux data */` |
|        3 | 15604 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 15605 | `	sAux.zPrefix = zPrefix;` |
|        3 | 15606 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 15607 | `	sAux.pVm = pVm;` |
|        - | 15608 | `	/* Extract */` |
|        3 | 15609 | `	zEnd = &zImport[nLen];` |
|        5 | 15610 | `	while( zImport < zEnd ){` |
|        3 | 15611 | `		int c = zImport[0];` |
|        3 | 15612 | `		pSuper = 0;` |
|        3 | 15613 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 15614 | `			/* Import $_GET variables */` |
|        3 | 15615 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 15616 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 15617 | `			/* Import $_POST variables */` |
|      ! 0 | 15618 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 15619 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 15620 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 15621 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 15622 | `		}` |
|        3 | 15623 | `		if( pSuper ){` |
|        - | 15624 | `			/* Iterate throw array entries */` |
|        3 | 15625 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 15626 | `		}` |
|        - | 15627 | `		/* Advance the cursor */` |
|        3 | 15628 | `		zImport++;` |
|        1 | 15629 | `	}` |
|        - | 15630 | `	/* All done,return TRUE*/` |
|        3 | 15631 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15632 | `	return PH7_OK;` |
|        1 | 15633 |  |
|        - | 15634 | `/*` |
|        - | 15635 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 15636 | ` * Refer to the eval() language construct implementation for more` |
|        - | 15637 | ` * information.` |
|        - | 15638 | ` */` |
|    13152 | 15639 | `static sxi32 VmEvalChunk(` |
|        - | 15640 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 15641 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 15642 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 15643 | `	int iFlags,         /* Compile flag */` |
|        - | 15644 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 15645 | `	)` |
|        5 | 15646 |  |
|        - | 15647 | `	SySet *pByteCode,aByteCode;` |
|        - | 15648 | `	SyBlob sSavedNs;` |
|    13157 | 15649 | `	ProcConsumer xErr = 0;` |
|    13157 | 15650 | `	void *pErrData = 0;` |
|        - | 15651 | `	/* Initialize bytecode container */` |
|    13157 | 15652 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    13157 | 15653 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 15654 | `	/* Reset the code generator */` |
|    13157 | 15655 | `	if( bTrueReturn ){` |
|        - | 15656 | `		/* Included file,log compile-time errors */` |
|     9793 | 15657 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9793 | 15658 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4895 | 15659 | `	}` |
|    13157 | 15660 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 15661 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 15662 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 15663 | `	 * the caller's namespace is restored. */` |
|    13157 | 15664 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    13157 | 15665 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    13157 | 15666 | `	if( bTrueReturn ){` |
|        - | 15667 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9793 | 15668 | `		SyBlobReset(&pVm->sNamespace);` |
|     4895 | 15669 | `	}` |
|        - | 15670 | `	/* Swap bytecode container */` |
|    13157 | 15671 | `	pByteCode = pVm->pByteContainer;` |
|    13157 | 15672 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 15673 | `	/* Compile the chunk */` |
|    13157 | 15674 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19731 | 15675 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 15676 | `		/* Compilation error,return false */` |
|        3 | 15677 | `		if( pCtx ){` |
|        3 | 15678 | `			ph7_result_bool(pCtx,0);` |
|        1 | 15679 | `		}` |
|        2 | 15680 | `	}else{` |
|        - | 15681 | `		/* Mount any newly defined classes */` |
|        - | 15682 | `		SyHashEntry *pEntry;` |
|        - | 15683 | `		ph7_class *pClass;` |
|        - | 15684 | `		ph7_value sResult; /* Return value */` |
|        - | 15685 | `		sxi32 rc;` |
|    13155 | 15686 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|  1040154 | 15687 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|  1020431 | 15688 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 15689 | `			/* Only mount classes that haven't been mounted yet */` |
|  1020431 | 15690 | `			if( !pClass->bMounted ){` |
|   257965 | 15691 | `				rc = VmMountUserClass(pVm,pClass);` |
|   257965 | 15692 | `				if( rc != SXRET_OK ){` |
|        - | 15693 | `					/* Mount failure (likely memory error) */` |
|        3 | 15694 | `					if( pCtx ){` |
|        3 | 15695 | `						ph7_result_bool(pCtx,0);` |
|        1 | 15696 | `					}` |
|        4 | 15697 | `					goto Cleanup;` |
|        - | 15698 | `				}` |
|   128979 | 15699 | `			}` |
|        5 | 15700 | `		}` |
|    13153 | 15701 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 15702 | `			/* Out of memory */` |
|      ! 0 | 15703 | `			if( pCtx ){` |
|      ! 0 | 15704 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 15705 | `			}` |
|      ! 0 | 15706 | `			goto Cleanup;` |
|        - | 15707 | `		}` |
|    13153 | 15708 | `		if( bTrueReturn ){` |
|        - | 15709 | `			/* Assume a boolean true return value */` |
|     9793 | 15710 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4898 | 15711 | `		}else{` |
|        - | 15712 | `			/* Assume a null return value */` |
|     3363 | 15713 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 15714 | `		}` |
|        - | 15715 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here,` |
|        - | 15716 | `		 * a path the OP_CALL cap check can't see; bound it under the same limit` |
|        - | 15717 | `		 * so a recursive include/eval can't overflow the native stack. */` |
|    13153 | 15718 | `		if( VmRecursionExceeded(pVm) ){` |
|        3 | 15719 | `			PH7_MemObjRelease(&sResult);` |
|        3 | 15720 | `			VmRecursionFatal(pVm);` |
|        3 | 15721 | `			goto Cleanup;` |
|        - | 15722 | `		}` |
|    13151 | 15723 | `		pVm->nRecursionDepth++;` |
|    13151 | 15724 | `		VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|    13151 | 15725 | `		pVm->nRecursionDepth--;` |
|    13151 | 15726 | `		if( pCtx ){` |
|        - | 15727 | `			/* Set the execution result */` |
|     9847 | 15728 | `			ph7_result_value(pCtx,&sResult);` |
|     4921 | 15729 | `		}` |
|    13151 | 15730 | `		PH7_MemObjRelease(&sResult);` |
|        - | 15731 | `	}` |
|     6576 | 15732 | `Cleanup:` |
|        - | 15733 | `	/* Cleanup the mess left behind */` |
|    13157 | 15734 | `	pVm->pByteContainer = pByteCode;` |
|    13157 | 15735 | `	SySetRelease(&aByteCode);` |
|        - | 15736 | `	/* Restore caller's namespace state */` |
|    13157 | 15737 | `	SyBlobReset(&pVm->sNamespace);` |
|    13157 | 15738 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    13157 | 15739 | `	SyBlobRelease(&sSavedNs);` |
|    13157 | 15740 | `	return SXRET_OK;` |
|        5 | 15741 |  |
|        - | 15742 | `/*` |
|        - | 15743 | ` * value eval(string $code)` |
|        - | 15744 | ` *   Evaluate a string as PHP code.` |
|        - | 15745 | ` * Parameter` |
|        - | 15746 | ` *  code: PHP code to evaluate.` |
|        - | 15747 | ` * Return` |
|        - | 15748 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 15749 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 15750 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 15751 | ` */` |
|       60 | 15752 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 15753 |  |
|        - | 15754 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       65 | 15755 | `	if( nArg < 1 ){` |
|        - | 15756 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15757 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15758 | `		return SXRET_OK;` |
|        - | 15759 | `	}` |
|        - | 15760 | `	/* Chunk to evaluate */` |
|       65 | 15761 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       65 | 15762 | `	if( sChunk.nByte < 1 ){` |
|        - | 15763 | `		/* Empty string,return NULL */` |
|        3 | 15764 | `		ph7_result_null(pCtx);` |
|        3 | 15765 | `		return SXRET_OK;` |
|        - | 15766 | `	}` |
|        - | 15767 | `	/* Eval the chunk */` |
|       63 | 15768 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       63 | 15769 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15770 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       41 | 15771 | `		return PH7_ABORT;` |
|        - | 15772 | `	}` |
|       24 | 15773 | `	return SXRET_OK;` |
|       35 | 15774 |  |
|        - | 15775 | `/*` |
|        - | 15776 | ` * Check if a file path is already included.` |
|        - | 15777 | ` */` |
|    19578 | 15778 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        3 | 15779 |  |
|        - | 15780 | `	SyString *aEntries;` |
|        - | 15781 | `	sxu32 n;` |
|    19581 | 15782 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 15783 | `	/* Perform a linear search */` |
| 95629397 | 15784 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 95609829 | 15785 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 15786 | `			/* Already included */` |
|       11 | 15787 | `			return TRUE;` |
|        - | 15788 | `		}` |
| 47804911 | 15789 | `	}` |
|    19571 | 15790 | `	return FALSE;` |
|     9792 | 15791 |  |
|        - | 15792 | `/*` |
|        - | 15793 | ` * Push a file path in the appropriate VM container.` |
|        - | 15794 | ` */` |
|    22874 | 15795 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 | 15796 |  |
|        - | 15797 | `	SyString sPath;` |
|        - | 15798 | `	char *zDup;` |
|        - | 15799 | `#ifdef __WINNT__` |
|        - | 15800 | `	char *zCur;` |
|        - | 15801 | `#endif` |
|        - | 15802 | `	sxi32 rc;` |
|    22879 | 15803 | `	if( nLen < 0 ){` |
|     3301 | 15804 | `		nLen = SyStrlen(zPath);` |
|     1648 | 15805 | `	}` |
|        - | 15806 | `	/* Duplicate the file path first */` |
|    22879 | 15807 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22879 | 15808 | `	if( zDup == 0 ){` |
|      ! 0 | 15809 | `		return SXERR_MEM;` |
|        - | 15810 | `	}` |
|        - | 15811 | `#ifdef __WINNT__` |
|        - | 15812 | `	/* Normalize path on windows` |
|        - | 15813 | `	 * Example:` |
|        - | 15814 | `	 *    Path/To/File.php` |
|        - | 15815 | `	 * becomes` |
|        - | 15816 | `	 *   path\to\file.php` |
|        - | 15817 | `	 */` |
|        5 | 15818 | `	zCur = zDup;` |
|        5 | 15819 | `	while( zCur[0] != 0 ){` |
|        5 | 15820 | `		if( zCur[0] == '/' ){` |
|        5 | 15821 | `			zCur[0] = '\\';` |
|        5 | 15822 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 15823 | `			int c = SyToLower(zCur[0]);` |
|        1 | 15824 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 15825 | `		}` |
|        5 | 15826 | `		zCur++;` |
|        5 | 15827 | `	}` |
|        - | 15828 | `#endif` |
|        - | 15829 | `	/* Install the file path */` |
|    22879 | 15830 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22879 | 15831 | `	if( !bMain ){` |
|    19581 | 15832 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 15833 | `			/* Already included */` |
|       11 | 15834 | `			*pNew = 0;` |
|        6 | 15835 | `		}else{` |
|        - | 15836 | `			/* Insert in the corresponding container */` |
|    19571 | 15837 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19571 | 15838 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 15839 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 15840 | `				return rc;` |
|        - | 15841 | `			}` |
|    19571 | 15842 | `			*pNew = 1;` |
|        - | 15843 | `		}` |
|     9789 | 15844 | `	}` |
|    22879 | 15845 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22879 | 15846 | `	return SXRET_OK;` |
|    11442 | 15847 |  |
|        - | 15848 | `/*` |
|        - | 15849 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 15850 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 15851 | ` * indicates failure.` |
|        - | 15852 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 15853 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 15854 | ` * operations.` |
|        - | 15855 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 15856 | ` * this function is a no-op.` |
|        - | 15857 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 15858 | ` * constructs for more information.` |
|        - | 15859 | ` */` |
|     9804 | 15860 | `static sxi32 VmExecIncludedFile(` |
|        - | 15861 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 15862 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 15863 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 15864 | `	 )` |
|        3 | 15865 |  |
|        - | 15866 | `	sxi32 rc;` |
|        - | 15867 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15868 | `	const ph7_io_stream *pStream;` |
|        - | 15869 | `	SyBlob sContents;` |
|        - | 15870 | `	void *pHandle;` |
|        - | 15871 | `	ph7_vm *pVm;` |
|        - | 15872 | `	int isNew;` |
|        - | 15873 | `	/* Initialize fields */` |
|     9807 | 15874 | `	pVm = pCtx->pVm;` |
|     9807 | 15875 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9807 | 15876 | `	isNew = 0;` |
|        - | 15877 | `	/* Extract the associated stream */` |
|     9807 | 15878 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 15879 | `	/*` |
|        - | 15880 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 15881 | `	 * in a read-only mode.` |
|        - | 15882 | `	 */` |
|     9807 | 15883 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9807 | 15884 | `	if( pHandle == 0 ){` |
|        8 | 15885 | `		return SXERR_IO;` |
|        - | 15886 | `	}` |
|     9801 | 15887 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9801 | 15888 | `	if( IncludeOnce && !isNew ){` |
|        - | 15889 | `		/* Already included */` |
|        9 | 15890 | `		rc = SXERR_EXISTS;` |
|        5 | 15891 | `	}else{` |
|        - | 15892 | `		/* Read the whole file contents */` |
|     9793 | 15893 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9793 | 15894 | `		if( rc == SXRET_OK ){` |
|        - | 15895 | `			SyString sScript;` |
|        - | 15896 | `			/* Compile and execute the script */` |
|     9793 | 15897 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9793 | 15898 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4895 | 15899 | `		}` |
|        - | 15900 | `	}` |
|        - | 15901 | `	/* Pop from the set of included file */` |
|     9801 | 15902 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 15903 | `	/* Close the handle */` |
|     9801 | 15904 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 15905 | `	/* Release the working buffer */` |
|     9801 | 15906 | `	SyBlobRelease(&sContents);` |
|        - | 15907 | `#else` |
|        - | 15908 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 15909 | `	SXUNUSED(pPath);` |
|        - | 15910 | `	SXUNUSED(IncludeOnce);` |
|        - | 15911 | `	rc = SXERR_IO;` |
|        - | 15912 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9801 | 15913 | `	return rc;` |
|     4905 | 15914 |  |
|        - | 15915 | `/*` |
|        - | 15916 | ` * string get_include_path(void)` |
|        - | 15917 | ` *  Gets the current include_path configuration option.` |
|        - | 15918 | ` * Parameter` |
|        - | 15919 | ` *  None` |
|        - | 15920 | ` * Return` |
|        - | 15921 | ` *  Included paths as a string` |
|        - | 15922 | ` */` |
|        2 | 15923 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15924 |  |
|        3 | 15925 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15926 | `	SyString *aEntry;` |
|        - | 15927 | `	int dir_sep;` |
|        - | 15928 | `	sxu32 n;` |
|        - | 15929 | `#ifdef __WINNT__` |
|        1 | 15930 | `	dir_sep = ';';` |
|        - | 15931 | `#else` |
|        - | 15932 | `	/* Assume UNIX path separator */` |
|        2 | 15933 | `	dir_sep = ':';` |
|        - | 15934 | `#endif` |
|        1 | 15935 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 15936 | `	SXUNUSED(apArg);` |
|        - | 15937 | `	/* Point to the list of import paths */` |
|        3 | 15938 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 15939 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 15940 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 15941 | `		if( n > 0 ){` |
|        - | 15942 | `			/* Append dir seprator */` |
|      ! 0 | 15943 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 15944 | `		}` |
|        - | 15945 | `		/* Append path */` |
|        3 | 15946 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 15947 | `	}` |
|        3 | 15948 | `	return PH7_OK;` |
|        1 | 15949 |  |
|        - | 15950 | `/*` |
|        - | 15951 | ` * string get_get_included_files(void)` |
|        - | 15952 | ` *  Gets the current include_path configuration option.` |
|        - | 15953 | ` * Parameter` |
|        - | 15954 | ` *  None` |
|        - | 15955 | ` * Return` |
|        - | 15956 | ` *  Included paths as a string` |
|        - | 15957 | ` */` |
|        2 | 15958 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15959 |  |
|        3 | 15960 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 15961 | `	ph7_value *pArray,*pWorker;` |
|        - | 15962 | `	SyString *pEntry;` |
|        - | 15963 | `	int c,d;` |
|        - | 15964 | `	/* Create an array and a working value */` |
|        3 | 15965 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 15966 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 15967 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 15968 | `		/* Out of memory,return null */` |
|      ! 0 | 15969 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15970 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 15971 | `		SXUNUSED(apArg);` |
|      ! 0 | 15972 | `		return PH7_OK;` |
|        - | 15973 | `	}` |
|        3 | 15974 | `	c = d = '/';` |
|        - | 15975 | `#ifdef __WINNT__` |
|        1 | 15976 | `	d = '\\';` |
|        - | 15977 | `#endif` |
|        - | 15978 | `	/* Iterate throw entries */` |
|        3 | 15979 | `	SySetResetCursor(pFiles);` |
|     3917 | 15980 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 15981 | `		const char *zBase,*zEnd;` |
|        - | 15982 | `		int iLen;` |
|        - | 15983 | `		/* reset the string cursor */` |
|     3915 | 15984 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 15985 | `		/* Extract base name */` |
|     3915 | 15986 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 15987 | `		/* Ignore trailing '/' */` |
|     5872 | 15988 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 15989 | `			zEnd--;` |
|      ! 0 | 15990 | `		}` |
|     3915 | 15991 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 15992 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 15993 | `			zEnd--;` |
|        1 | 15994 | `		}` |
|     3915 | 15995 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 15996 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 15997 | `		/* Copy entry name */` |
|     3915 | 15998 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 15999 | `		/* Perform the insertion */` |
|     3915 | 16000 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 16001 | `	}` |
|        - | 16002 | `	/* All done,return the created array */` |
|        3 | 16003 | `	ph7_result_value(pCtx,pArray);` |
|        - | 16004 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 16005 | `	 * by the engine as soon we return from this foreign` |
|        - | 16006 | `	 * function.` |
|        - | 16007 | `	 */` |
|        3 | 16008 | `	return PH7_OK;` |
|        2 | 16009 |  |
|        - | 16010 | `/*` |
|        - | 16011 | ` * include:` |
|        - | 16012 | ` * According to the PHP reference manual.` |
|        - | 16013 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 16014 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 16015 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 16016 | ` *  include() will finally check in the calling script's own directory` |
|        - | 16017 | ` *  and the current working directory before failing. The include()` |
|        - | 16018 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 16019 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 16020 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 16021 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 16022 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 16023 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 16024 | ` *  directory to find the requested file.` |
|        - | 16025 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 16026 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 16027 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 16028 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 16029 | ` */` |
|     9780 | 16030 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 16031 |  |
|        - | 16032 | `	SyString sFile;` |
|        - | 16033 | `	sxi32 rc;` |
|     9783 | 16034 | `	if( nArg < 1 ){` |
|        - | 16035 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 16036 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16037 | `		return SXRET_OK;` |
|        - | 16038 | `	}` |
|        - | 16039 | `	/* File to include */` |
|     9783 | 16040 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9783 | 16041 | `	if( sFile.nByte < 1 ){` |
|        - | 16042 | `		/* Empty string,return NULL */` |
|      ! 0 | 16043 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16044 | `		return SXRET_OK;` |
|        - | 16045 | `	}` |
|        - | 16046 | `	/* Open,compile and execute the desired script */` |
|     9783 | 16047 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9783 | 16048 | `	if( rc != SXRET_OK ){` |
|        - | 16049 | `		/* Emit a warning and return false */` |
|        3 | 16050 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 16051 | `		ph7_result_bool(pCtx,0);` |
|        1 | 16052 | `	}` |
|     9783 | 16053 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 16054 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 | 16055 | `		return PH7_ABORT;` |
|        - | 16056 | `	}` |
|     9778 | 16057 | `	return SXRET_OK;` |
|     4893 | 16058 |  |
|        - | 16059 | `/*` |
|        - | 16060 | ` * include_once:` |
|        - | 16061 | ` *  According to the PHP reference manual.` |
|        - | 16062 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 16063 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 16064 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 16065 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 16066 | ` *   just once.` |
|        - | 16067 | ` */` |
|       10 | 16068 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 16069 |  |
|        - | 16070 | `	SyString sFile;` |
|        - | 16071 | `	sxi32 rc;` |
|       11 | 16072 | `	if( nArg < 1 ){` |
|        - | 16073 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 16074 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16075 | `		return SXRET_OK;` |
|        - | 16076 | `	}` |
|        - | 16077 | `	/* File to include */` |
|       11 | 16078 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       11 | 16079 | `	if( sFile.nByte < 1 ){` |
|        - | 16080 | `		/* Empty string,return NULL */` |
|      ! 0 | 16081 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16082 | `		return SXRET_OK;` |
|        - | 16083 | `	}` |
|        - | 16084 | `	/* Open,compile and execute the desired script */` |
|       11 | 16085 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       11 | 16086 | `	if( rc == SXERR_EXISTS ){` |
|        - | 16087 | `		/* File already included,return TRUE */` |
|        7 | 16088 | `		ph7_result_bool(pCtx,1);` |
|        7 | 16089 | `		return SXRET_OK;` |
|        - | 16090 | `	}` |
|        5 | 16091 | `	if( rc != SXRET_OK ){` |
|        - | 16092 | `		/* Emit a warning and return false */` |
|      ! 0 | 16093 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 16094 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16095 | ` 	}` |
|        5 | 16096 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 16097 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 16098 | `		return PH7_ABORT;` |
|        - | 16099 | `	}` |
|        5 | 16100 | `	return SXRET_OK;` |
|        6 | 16101 |  |
|        - | 16102 | `/*` |
|        - | 16103 | ` * require.` |
|        - | 16104 | ` *  According to the PHP reference manual.` |
|        - | 16105 | ` *   require() is identical to include() except upon failure it will` |
|        - | 16106 | ` *   also produce a fatal level error.` |
|        - | 16107 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 16108 | ` *   emits a warning  which allows the script to continue.` |
|        - | 16109 | ` */` |
|        6 | 16110 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 16111 |  |
|        - | 16112 | `	SyString sFile;` |
|        - | 16113 | `	sxi32 rc;` |
|        8 | 16114 | `	if( nArg < 1 ){` |
|        - | 16115 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 16116 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16117 | `		return SXRET_OK;` |
|        - | 16118 | `	}` |
|        - | 16119 | `	/* File to include */` |
|        8 | 16120 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 16121 | `	if( sFile.nByte < 1 ){` |
|        - | 16122 | `		/* Empty string,return NULL */` |
|      ! 0 | 16123 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16124 | `		return SXRET_OK;` |
|        - | 16125 | `	}` |
|        - | 16126 | `	/* Open,compile and execute the desired script */` |
|        8 | 16127 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 16128 | `	if( rc != SXRET_OK ){` |
|        - | 16129 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 16130 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 16131 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16132 | `		return PH7_ABORT;` |
|        - | 16133 | `	}` |
|        8 | 16134 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 16135 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 16136 | `		return PH7_ABORT;` |
|        - | 16137 | `	}` |
|        8 | 16138 | `	return SXRET_OK;` |
|        5 | 16139 |  |
|        - | 16140 | `/*` |
|        - | 16141 | ` * require_once:` |
|        - | 16142 | ` *  According to the PHP reference manual.` |
|        - | 16143 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 16144 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 16145 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 16146 | ` *   and how it differs from its non _once siblings.` |
|        - | 16147 | ` */` |
|        4 | 16148 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 16149 |  |
|        - | 16150 | `	SyString sFile;` |
|        - | 16151 | `	sxi32 rc;` |
|        5 | 16152 | `	if( nArg < 1 ){` |
|        - | 16153 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 16154 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16155 | `		return SXRET_OK;` |
|        - | 16156 | `	}` |
|        - | 16157 | `	/* File to include */` |
|        5 | 16158 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 16159 | `	if( sFile.nByte < 1 ){` |
|        - | 16160 | `		/* Empty string,return NULL */` |
|      ! 0 | 16161 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16162 | `		return SXRET_OK;` |
|        - | 16163 | `	}` |
|        - | 16164 | `	/* Open,compile and execute the desired script */` |
|        5 | 16165 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 16166 | `	if( rc == SXERR_EXISTS ){` |
|        - | 16167 | `		/* File already included,return TRUE */` |
|        3 | 16168 | `		ph7_result_bool(pCtx,1);` |
|        3 | 16169 | `		return SXRET_OK;` |
|        - | 16170 | `	}` |
|        3 | 16171 | `	if( rc != SXRET_OK ){` |
|        - | 16172 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 16173 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 16174 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16175 | `		return PH7_ABORT;` |
|        - | 16176 | `	}` |
|        3 | 16177 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 16178 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 16179 | `		return PH7_ABORT;` |
|        - | 16180 | `	}` |
|        3 | 16181 | `	return SXRET_OK;` |
|        3 | 16182 |  |
|        - | 16183 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 16184 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 16185 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 16186 | `/*` |
|        - | 16187 | ` * Section:` |
|        - | 16188 | ` *  SPL Autoloading functions.` |
|        - | 16189 | ` * Status:` |
|        - | 16190 | ` *  Stable.` |
|        - | 16191 | ` */` |
|        - | 16192 | `/*` |
|        - | 16193 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 16194 | ` *  Register given function as __autoload() implementation.` |
|        - | 16195 | ` * Parameters` |
|        - | 16196 | ` *  callback` |
|        - | 16197 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 16198 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 16199 | ` *  throw` |
|        - | 16200 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 16201 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 16202 | ` *  prepend` |
|        - | 16203 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 16204 | ` *   autoload stack instead of appending it.` |
|        - | 16205 | ` * Return` |
|        - | 16206 | ` *  TRUE on success, FALSE on failure.` |
|        - | 16207 | ` */` |
|       34 | 16208 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 16209 |  |
|        - | 16210 | `	VmAutoloadCB sEntry;` |
|       39 | 16211 | `	ph7_vm *pVm = pCtx->pVm;` |
|       39 | 16212 | `	int iPrepend = 0;` |
|        - | 16213 | `	sxu32 n;` |
|       39 | 16214 | `	if( nArg < 1 ){` |
|        - | 16215 | `		/* No callback provided — register default spl_autoload.` |
|        - | 16216 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 16217 | `		/* Check for duplicates first */` |
|        9 | 16218 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 16219 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 16220 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 16221 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 16222 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 16223 | `				ph7_result_bool(pCtx,1);` |
|        5 | 16224 | `				return SXRET_OK;` |
|        - | 16225 | `			}` |
|      ! 0 | 16226 | `		}` |
|        5 | 16227 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 16228 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 16229 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 16230 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 16231 | `		ph7_result_bool(pCtx,1);` |
|        5 | 16232 | `		return SXRET_OK;` |
|        - | 16233 | `	}` |
|        - | 16234 | `	/* Validate that the callback is callable */` |
|       31 | 16235 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 16236 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 16237 | `		if( nArg >= 2 ){` |
|      ! 0 | 16238 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 16239 | `		}` |
|      ! 0 | 16240 | `		if( iThrow ){` |
|      ! 0 | 16241 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 16242 | `				"Argument is not callable");` |
|      ! 0 | 16243 | `		}` |
|      ! 0 | 16244 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16245 | `		return SXRET_OK;` |
|        - | 16246 | `	}` |
|        - | 16247 | `	/* Check for duplicates */` |
|       49 | 16248 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 16249 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 16250 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 16251 | `			/* Already registered */` |
|      ! 0 | 16252 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 16253 | `			return SXRET_OK;` |
|        - | 16254 | `		}` |
|       11 | 16255 | `	}` |
|        - | 16256 | `	/* Check prepend flag */` |
|       31 | 16257 | `	if( nArg >= 3 ){` |
|        3 | 16258 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 16259 | `	}` |
|        - | 16260 | `	/* Store the callback */` |
|       31 | 16261 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       31 | 16262 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       31 | 16263 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       32 | 16264 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 16265 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 16266 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 16267 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 16268 | `		VmAutoloadCB *aBase;` |
|        3 | 16269 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 16270 | `		/* Rotate: move last entry to front */` |
|        3 | 16271 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 16272 | `		if( aBase ){` |
|        - | 16273 | `			VmAutoloadCB sTemp;` |
|        - | 16274 | `			sxu32 i;` |
|        3 | 16275 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 16276 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 16277 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 16278 | `			}` |
|        3 | 16279 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 16280 | `		}` |
|        2 | 16281 | `	}else{` |
|       29 | 16282 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 16283 | `	}` |
|       31 | 16284 | `	ph7_result_bool(pCtx,1);` |
|       31 | 16285 | `	return SXRET_OK;` |
|       22 | 16286 |  |
|        - | 16287 | `/*` |
|        - | 16288 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 16289 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 16290 | ` * Parameters` |
|        - | 16291 | ` *  callback` |
|        - | 16292 | ` *   The autoload function being unregistered.` |
|        - | 16293 | ` * Return` |
|        - | 16294 | ` *  TRUE on success, FALSE on failure.` |
|        - | 16295 | ` */` |
|       32 | 16296 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 16297 |  |
|       37 | 16298 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 16299 | `	sxu32 n,nEntry;` |
|       37 | 16300 | `	if( nArg < 1 ){` |
|      ! 0 | 16301 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16302 | `		return SXRET_OK;` |
|        - | 16303 | `	}` |
|       37 | 16304 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       41 | 16305 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       39 | 16306 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       39 | 16307 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 16308 | `			/* Found — remove by shifting remaining entries down */` |
|       35 | 16309 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 16310 | `			sxu32 i;` |
|       35 | 16311 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       49 | 16312 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 16313 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 16314 | `			}` |
|        - | 16315 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       35 | 16316 | `			SySetPop(&pVm->aAutoload);` |
|       35 | 16317 | `			ph7_result_bool(pCtx,1);` |
|       35 | 16318 | `			return SXRET_OK;` |
|        - | 16319 | `		}` |
|        3 | 16320 | `	}` |
|        3 | 16321 | `	ph7_result_bool(pCtx,0);` |
|        3 | 16322 | `	return SXRET_OK;` |
|       21 | 16323 |  |
|        - | 16324 | `/*` |
|        - | 16325 | ` * array spl_autoload_functions(void)` |
|        - | 16326 | ` *  Return all registered __autoload() functions.` |
|        - | 16327 | ` * Return` |
|        - | 16328 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 16329 | ` *  an empty array is returned.` |
|        - | 16330 | ` */` |
|       20 | 16331 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 16332 |  |
|       21 | 16333 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 16334 | `	ph7_value *pArray;` |
|        - | 16335 | `	sxu32 n,nEntry;` |
|       10 | 16336 | `	SXUNUSED(nArg);` |
|       10 | 16337 | `	SXUNUSED(apArg);` |
|       21 | 16338 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 16339 | `	if( pArray == 0 ){` |
|      ! 0 | 16340 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16341 | `		return SXRET_OK;` |
|        - | 16342 | `	}` |
|       21 | 16343 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 16344 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 16345 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 16346 | `		if( pEntry ){` |
|       15 | 16347 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 16348 | `		}` |
|        8 | 16349 | `	}` |
|       21 | 16350 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 16351 | `	return SXRET_OK;` |
|       11 | 16352 |  |
|        - | 16353 | `/*` |
|        - | 16354 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 16355 | ` *  Default implementation of __autoload().` |
|        - | 16356 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 16357 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 16358 | ` * Parameters` |
|        - | 16359 | ` *  class` |
|        - | 16360 | ` *   The class name being searched.` |
|        - | 16361 | ` *  file_extensions` |
|        - | 16362 | ` *   Comma-separated list of file extensions to try.` |
|        - | 16363 | ` */` |
|        2 | 16364 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 16365 |  |
|        - | 16366 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 16367 | `	SyBlob sPath;` |
|        - | 16368 | `	int nClass;` |
|        - | 16369 | `	sxi32 rc;` |
|        3 | 16370 | `	if( nArg < 1 ){` |
|      ! 0 | 16371 | `		return SXRET_OK;` |
|        - | 16372 | `	}` |
|        3 | 16373 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 16374 | `	if( nClass < 1 ){` |
|      ! 0 | 16375 | `		return SXRET_OK;` |
|        - | 16376 | `	}` |
|        - | 16377 | `	/* Default extensions */` |
|        3 | 16378 | `	zExt = ".php,.inc";` |
|        3 | 16379 | `	if( nArg >= 2 ){` |
|        - | 16380 | `		int nExt;` |
|      ! 0 | 16381 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 16382 | `		if( nExt < 1 ){` |
|      ! 0 | 16383 | `			zExt = ".php,.inc";` |
|      ! 0 | 16384 | `		}` |
|      ! 0 | 16385 | `	}` |
|        3 | 16386 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 16387 | `	/* Iterate over comma-separated extensions */` |
|        3 | 16388 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 16389 | `	zCur = zExt;` |
|        7 | 16390 | `	while( zCur < zEnd ){` |
|        - | 16391 | `		const char *zComma;` |
|        - | 16392 | `		SyString sFile;` |
|        - | 16393 | `		int i;` |
|        - | 16394 | `		/* Find next comma or end */` |
|        5 | 16395 | `		zComma = zCur;` |
|       21 | 16396 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 16397 | `			zComma++;` |
|        1 | 16398 | `		}` |
|        - | 16399 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 16400 | `		SyBlobReset(&sPath);` |
|       69 | 16401 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 16402 | `			char c = zClass[i];` |
|       65 | 16403 | `			if( c == '\\' ){` |
|      ! 0 | 16404 | `				c = '/';` |
|       65 | 16405 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 16406 | `				c = c + ('a' - 'A');` |
|        6 | 16407 | `			}` |
|       65 | 16408 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 16409 | `		}` |
|        - | 16410 | `		/* Append extension */` |
|        5 | 16411 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 16412 | `		/* Try to include the file */` |
|        5 | 16413 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 16414 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 16415 | `		if( rc == SXRET_OK ){` |
|        - | 16416 | `			/* File included successfully */` |
|      ! 0 | 16417 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 16418 | `			return SXRET_OK;` |
|        - | 16419 | `		}` |
|        - | 16420 | `		/* Move past the comma */` |
|        5 | 16421 | `		zCur = zComma;` |
|        5 | 16422 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 16423 | `			zCur++;` |
|        1 | 16424 | `		}` |
|        1 | 16425 | `	}` |
|        3 | 16426 | `	SyBlobRelease(&sPath);` |
|        3 | 16427 | `	return SXRET_OK;` |
|        2 | 16428 |  |
|        - | 16429 | `/* Table of built-in VM functions. */` |
|        - | 16430 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 16431 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 16432 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 16433 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 16434 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 16435 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 16436 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 16437 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 16438 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 16439 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 16440 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 16441 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 16442 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 16443 | `	    /* Constants management */` |
|        - | 16444 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 16445 | `	{ "define",   vm_builtin_define               },` |
|        - | 16446 | `	{ "constant", vm_builtin_constant             },` |
|        - | 16447 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 16448 | `	   /* Class/Object functions */` |
|        - | 16449 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 16450 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 16451 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 16452 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 16453 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 16454 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 16455 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 16456 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 16457 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 16458 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 16459 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 16460 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 16461 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 16462 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 16463 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 16464 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 16465 | `	   /* SPL Autoloading */` |
|        - | 16466 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 16467 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 16468 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 16469 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 16470 | `	   /* Random numbers/strings generators */` |
|        - | 16471 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 16472 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 16473 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 16474 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 16475 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 16476 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 16477 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 16478 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 16479 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 16480 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 16481 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 16482 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 16483 | `	   /* Language constructs functions */` |
|        - | 16484 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 16485 | `	{ "print", vm_builtin_print                   },` |
|        - | 16486 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 16487 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 16488 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 16489 | `	  /* Variable handling functions */` |
|        - | 16490 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 16491 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 16492 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 16493 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 16494 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 16495 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 16496 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 16497 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 16498 | `	  /* Ouput control functions */` |
|        - | 16499 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 16500 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 16501 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 16502 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 16503 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 16504 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 16505 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 16506 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 16507 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 16508 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 16509 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 16510 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 16511 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 16512 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 16513 | `	  /* Assertion functions */` |
|        - | 16514 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 16515 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 16516 | `	  /* Error reporting functions */` |
|        - | 16517 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 16518 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 16519 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 16520 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 16521 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 16522 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 16523 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 16524 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 16525 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 16526 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 16527 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 16528 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 16529 | `	  /* Release info */` |
|        - | 16530 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 16531 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 16532 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 16533 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 16534 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 16535 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 16536 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 16537 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 16538 | `	  /* hashmap */` |
|        - | 16539 | `	{"compact",          vm_builtin_compact       },` |
|        - | 16540 | `	{"extract",          vm_builtin_extract       },` |
|        - | 16541 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 16542 | `	  /* URL related function */` |
|        - | 16543 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 16544 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 16545 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 16546 | `	   /* XML processing functions */` |
|        - | 16547 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 16548 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 16549 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 16550 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 16551 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 16552 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 16553 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 16554 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 16555 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 16556 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 16557 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 16558 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 16559 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 16560 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 16561 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 16562 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 16563 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 16564 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 16565 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 16566 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 16567 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 16568 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 16569 | `	   /* UTF-8 encoding/decoding */` |
|        - | 16570 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 16571 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 16572 | `	   /* Command line processing */` |
|        - | 16573 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 16574 | `	   /* JSON encoding/decoding */` |
|        - | 16575 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 16576 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 16577 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 16578 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 16579 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 16580 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 16581 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 16582 | `	   /* Files/URI inclusion facility */` |
|        - | 16583 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 16584 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 16585 | `	{ "include",      vm_builtin_include          },` |
|        - | 16586 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 16587 | `	{ "require",      vm_builtin_require          },` |
|        - | 16588 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 16589 | `};` |
|        - | 16590 | `/*` |
|        - | 16591 | ` * Register the built-in VM functions defined above.` |
|        - | 16592 | ` */` |
|     2964 | 16593 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        5 | 16594 |  |
|        - | 16595 | `	sxi32 rc;` |
|        - | 16596 | `	sxu32 n;` |
|   400145 | 16597 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 16598 | `		/* Note that these special functions have access` |
|        - | 16599 | `		 * to the underlying virtual machine as their` |
|        - | 16600 | `		 * private data.` |
|        - | 16601 | `		 */` |
|   397181 | 16602 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   397181 | 16603 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 16604 | `			return rc;` |
|        - | 16605 | `		}` |
|   198593 | 16606 | `	}` |
|     2969 | 16607 | `	return SXRET_OK;` |
|     1487 | 16608 |  |
|        - | 16609 | `/*` |
|        - | 16610 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 16611 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 16612 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 16613 | ` */` |
|   195418 | 16614 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        5 | 16615 |  |
|   195423 | 16616 | `	if( !iLoadable ){` |
|   193173 | 16617 | `		return pClass;` |
|        - | 16618 | `	}` |
|     2261 | 16619 | `	while(pClass){` |
|     2255 | 16620 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2249 | 16621 | `			return pClass;` |
|        - | 16622 | `		}` |
|        7 | 16623 | `		pClass = pClass->pNextName;` |
|        1 | 16624 | `	}` |
|        7 | 16625 | `	return 0;` |
|    97714 | 16626 |  |
|        - | 16627 | `/*` |
|        - | 16628 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 16629 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 16630 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 16631 | ` * registered in the VM's class table.` |
|        - | 16632 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 16633 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 16634 | ` */` |
|       38 | 16635 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 16636 |  |
|        - | 16637 | `	VmAutoloadCB *pEntry;` |
|        - | 16638 | `	ph7_value sArg,sResult;` |
|        - | 16639 | `	SyHashEntry *pHashEntry;` |
|        - | 16640 | `	ph7_class *pClass;` |
|        - | 16641 | `	sxu32 n,nEntry;` |
|       43 | 16642 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       43 | 16643 | `	if( nEntry < 1 ){` |
|       27 | 16644 | `		return 0;` |
|        - | 16645 | `	}` |
|        - | 16646 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       19 | 16647 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 16648 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 16649 | `	}` |
|        - | 16650 | `	/* Mark this class as being autoloaded */` |
|       17 | 16651 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 16652 | `	/* Prepare the class name argument */` |
|       17 | 16653 | `	PH7_MemObjInit(pVm,&sArg);` |
|       17 | 16654 | `	PH7_MemObjInit(pVm,&sResult);` |
|       17 | 16655 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       17 | 16656 | `	pClass = 0;` |
|       31 | 16657 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 16658 | `		ph7_value *apArg[1];` |
|       27 | 16659 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       27 | 16660 | `		if( pEntry == 0 ){` |
|      ! 0 | 16661 | `			continue;` |
|        - | 16662 | `		}` |
|       27 | 16663 | `		apArg[0] = &sArg;` |
|       27 | 16664 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 16665 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 16666 | `			continue;` |
|        - | 16667 | `		}` |
|        - | 16668 | `		/* Check if the class is now available */` |
|       27 | 16669 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       27 | 16670 | `		if( pHashEntry ){` |
|       12 | 16671 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       12 | 16672 | `			if( pClass ){` |
|       12 | 16673 | `				break;` |
|        - | 16674 | `			}` |
|      ! 0 | 16675 | `		}` |
|       10 | 16676 | `	}` |
|       17 | 16677 | `	PH7_MemObjRelease(&sArg);` |
|       17 | 16678 | `	PH7_MemObjRelease(&sResult);` |
|        - | 16679 | `	/* Remove reentrancy guard */` |
|       17 | 16680 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       17 | 16681 | `	return pClass;` |
|       24 | 16682 |  |
|        - | 16683 | `/*` |
|        - | 16684 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 16685 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 16686 | ` */` |
|       18 | 16687 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 16688 |  |
|       23 | 16689 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        5 | 16690 |  |
|        - | 16691 | `/*` |
|        - | 16692 | ` * Check if the given name refer to an installed class.` |
|        - | 16693 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 16694 | ` */` |
|   195430 | 16695 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 16696 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 16697 | `	const char *zName,  /* Name of the target class */` |
|        - | 16698 | `	sxu32 nByte,        /* zName length */` |
|        - | 16699 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 16700 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 16701 | `						 */` |
|        - | 16702 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 16703 | `	)` |
|        5 | 16704 |  |
|        - | 16705 | `	SyHashEntry *pEntry;` |
|        - | 16706 | `	ph7_class *pClass;` |
|    97715 | 16707 | `	SXUNUSED(iNest);` |
|        - | 16708 | `	/* Exact class lookup.` |
|        - | 16709 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 16710 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   195435 | 16711 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   195435 | 16712 | `	if( pEntry == 0 ){` |
|        - | 16713 | `		/* Class not found in hash table — try autoload before giving up */` |
|       23 | 16714 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 16715 | `	}` |
|   195415 | 16716 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   195415 | 16717 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    97720 | 16718 |  |
|        - | 16719 | `/*` |
|        - | 16720 | ` * Reference Table Implementation` |
|        - | 16721 | ` * Status: stable <chm@symisc.net>` |
|        - | 16722 | ` * Intro` |
|        - | 16723 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 16724 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 16725 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 16726 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 16727 | ` *  Refer to the official for more information on this powerful` |
|        - | 16728 | ` *  extension.` |
|        - | 16729 | ` */` |
|        - | 16730 | `/*` |
|        - | 16731 | ` * Allocate a new reference entry.` |
|        - | 16732 | ` */` |
|  3269364 | 16733 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 16734 |  |
|        - | 16735 | `	VmRefObj *pRef;` |
|        - | 16736 | `	/* Allocate a new instance */` |
|  3269369 | 16737 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3269369 | 16738 | `	if( pRef == 0 ){` |
|      ! 0 | 16739 | `		return 0;` |
|        - | 16740 | `	}` |
|        - | 16741 | `	/* Zero the structure */` |
|  3269369 | 16742 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 16743 | `	/* Initialize fields */` |
|  3269369 | 16744 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3269369 | 16745 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3269369 | 16746 | `	pRef->nIdx = nIdx;` |
|  3269369 | 16747 | `	return pRef;` |
|  1634687 | 16748 |  |
|        - | 16749 | `/*` |
|        - | 16750 | ` * Default hash function used by the reference table` |
|        - | 16751 | ` * for lookup/insertion operations.` |
|        - | 16752 | ` */` |
| 17803560 | 16753 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        5 | 16754 |  |
|        - | 16755 | `	/* Calculate the hash based on the memory object index */` |
| 17803565 | 16756 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        5 | 16757 |  |
|        - | 16758 | `/*` |
|        - | 16759 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 16760 | ` * in the reference table.` |
|        - | 16761 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 16762 | ` * otherwise.` |
|        - | 16763 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16764 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16765 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16766 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16767 | ` * Refer to the official for more information on this powerful` |
|        - | 16768 | ` * extension.` |
|        - | 16769 | ` */` |
|  9701334 | 16770 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        5 | 16771 |  |
|        - | 16772 | `	VmRefObj *pRef;` |
|        - | 16773 | `	sxu32 nBucket;` |
|        - | 16774 | `	/* Point to the appropriate bucket */` |
|  9701339 | 16775 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 16776 | `	/* Perform the lookup */` |
|  9701339 | 16777 | `	pRef = pVm->apRefObj[nBucket];` |
| 21346564 | 16778 | `	for(;;){` |
| 42686271 | 16779 | `		if( pRef == 0 ){` |
|  3380763 | 16780 | `			break;` |
|        - | 16781 | `		}` |
| 39305513 | 16782 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 16783 | `			/* Entry found */` |
|  6320581 | 16784 | `			return pRef;` |
|        - | 16785 | `		}` |
|        - | 16786 | `		/* Point to the next entry */` |
| 32984937 | 16787 | `		pRef = pRef->pNextCollide;` |
|        5 | 16788 | `	}` |
|        - | 16789 | `	/* No such entry,return NULL */` |
|  3380763 | 16790 | `	return 0;` |
|  4850672 | 16791 |  |
|        - | 16792 | `/*` |
|        - | 16793 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16794 | ` *` |
|        - | 16795 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16796 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16797 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16798 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16799 | ` * Refer to the official for more information on this powerful` |
|        - | 16800 | ` * extension.` |
|        - | 16801 | ` */` |
|  3269364 | 16802 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 16803 |  |
|        - | 16804 | `	sxu32 nBucket;` |
|  3269369 | 16805 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 16806 | `		VmRefObj **apNew;` |
|        - | 16807 | `		sxu32 nNew;` |
|        - | 16808 | `		/* Allocate a larger table */` |
|     6769 | 16809 | `		nNew = pVm->nRefSize << 1;` |
|     6769 | 16810 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     6769 | 16811 | `		if( apNew ){` |
|     6769 | 16812 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 16813 | `			sxu32 n;` |
|        - | 16814 | `			/* Zero the structure */` |
|     6769 | 16815 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 16816 | `			/* Rehash all referenced entries */` |
|  2876731 | 16817 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 16818 | `				/* Remove old collision links */` |
|  2869967 | 16819 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 16820 | `				/* Point to the appropriate bucket */` |
|  2869967 | 16821 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 16822 | `				/* Insert the entry  */` |
|  2869967 | 16823 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2869967 | 16824 | `				if( apNew[nBucket] ){` |
|  2301119 | 16825 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 16826 | `				}` |
|  2869967 | 16827 | `				apNew[nBucket] = pEntry;` |
|        - | 16828 | `				/* Point to the next entry */` |
|  2869967 | 16829 | `				pEntry = pEntry->pNext;` |
|  1434986 | 16830 | `			}` |
|        - | 16831 | `			/* Release the old table */` |
|     6769 | 16832 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 16833 | `			/* Install the new one */` |
|     6769 | 16834 | `			pVm->apRefObj = apNew;` |
|     6769 | 16835 | `			pVm->nRefSize = nNew;` |
|     3382 | 16836 | `		}` |
|     3382 | 16837 | `	}` |
|        - | 16838 | `	/* Point to the appropriate bucket */` |
|  3269369 | 16839 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 16840 | `	/* Insert the entry */` |
|  3269369 | 16841 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3269369 | 16842 | `	if( pVm->apRefObj[nBucket] ){` |
|  2648609 | 16843 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1324304 | 16844 | `	}` |
|  3269369 | 16845 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3269369 | 16846 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3269369 | 16847 | `	pVm->nRefUsed++;` |
|  3269369 | 16848 | `	return SXRET_OK;` |
|        5 | 16849 |  |
|        - | 16850 | `/*` |
|        - | 16851 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 16852 | ` * the reference table.` |
|        - | 16853 | ` * This function is invoked when the user perform an unset` |
|        - | 16854 | ` * call [i.e: unset($var); ].` |
|        - | 16855 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16856 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16857 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16858 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16859 | ` * Refer to the official for more information on this powerful` |
|        - | 16860 | ` * extension.` |
|        - | 16861 | ` */` |
|  3202658 | 16862 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 16863 |  |
|        - | 16864 | `	ph7_hashmap_node **apNode;` |
|        - | 16865 | `	SyHashEntry **apEntry;` |
|        - | 16866 | `	sxu32 n;` |
|        - | 16867 | `	/* Point to the reference table */` |
|  3202663 | 16868 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3202663 | 16869 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 16870 | `	/* Unlink the entry from the reference table */` |
|  3320061 | 16871 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   117403 | 16872 | `		if( apEntry[n] ){` |
|   117353 | 16873 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    58674 | 16874 | `		}` |
|    58704 | 16875 | `	}` |
|  6287667 | 16876 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3085009 | 16877 | `		if( apNode[n] ){` |
|     7080 | 16878 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3538 | 16879 | `		}` |
|  1542507 | 16880 | `	}` |
|  3202663 | 16881 | `	if( pRef->pPrevCollide ){` |
|  1239763 | 16882 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   620218 | 16883 | `	}else{` |
|  1962905 | 16884 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 16885 | `	}` |
|  3202663 | 16886 | `	if( pRef->pNextCollide ){` |
|  1837142 | 16887 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   918569 | 16888 | `	}` |
|  3202663 | 16889 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 16890 | `	/* Release the node */` |
|  3202663 | 16891 | `	SySetRelease(&pRef->aReference);` |
|  3202663 | 16892 | `	SySetRelease(&pRef->aArrEntries);` |
|  3202663 | 16893 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3202663 | 16894 | `	pVm->nRefUsed--;` |
|  3202663 | 16895 | `	return SXRET_OK;` |
|        5 | 16896 |  |
|        - | 16897 | `/*` |
|        - | 16898 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16899 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16900 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16901 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16902 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16903 | ` * Refer to the official for more information on this powerful` |
|        - | 16904 | ` * extension.` |
|        - | 16905 | ` */` |
|  3309474 | 16906 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 16907 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16908 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16909 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16910 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 16911 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 16912 | `	)` |
|        5 | 16913 |  |
|  3309479 | 16914 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 16915 | `	VmRefObj *pRef;` |
|        - | 16916 | `	/* Check if the referenced object already exists */` |
|  3309479 | 16917 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3309479 | 16918 | `	if( pRef == 0 ){` |
|        - | 16919 | `		/* Create a new entry */` |
|  3269369 | 16920 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3269369 | 16921 | `		if( pRef == 0 ){` |
|      ! 0 | 16922 | `			return SXERR_MEM;` |
|        - | 16923 | `		}` |
|  3269369 | 16924 | `		pRef->iFlags = iFlags;` |
|        - | 16925 | `		/* Install the entry */` |
|  3269369 | 16926 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1634682 | 16927 | `	}` |
|  3309479 | 16928 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3309479 | 16929 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 16930 | `		VmSlot sRef;` |
|        - | 16931 | `		/* Local frame,record referenced entry so that it can` |
|        - | 16932 | `		 * be deleted when we leave this frame.` |
|        - | 16933 | `		 */` |
|   111509 | 16934 | `		sRef.nIdx = nIdx;` |
|   111509 | 16935 | `		sRef.pUserData = pEntry;` |
|   111509 | 16936 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 16937 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 16938 | `		}` |
|    55752 | 16939 | `	}` |
|  3309479 | 16940 | `	if( pEntry ){` |
|        - | 16941 | `		/* Address of the hash-entry */` |
|   151391 | 16942 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    75693 | 16943 | `	}` |
|  3309479 | 16944 | `	if( pMapEntry ){` |
|        - | 16945 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3146767 | 16946 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1573381 | 16947 | `	}` |
|  3309479 | 16948 | `	return SXRET_OK;` |
|  1654742 | 16949 |  |
|        - | 16950 | `/*` |
|        - | 16951 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 16952 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16953 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16954 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16955 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16956 | ` * Refer to the official for more information on this powerful` |
|        - | 16957 | ` * extension.` |
|        - | 16958 | ` */` |
|  3189394 | 16959 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 16960 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16961 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16962 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16963 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 16964 | `	)` |
|        5 | 16965 |  |
|        - | 16966 | `	VmRefObj *pRef;` |
|        - | 16967 | `	sxu32 n;` |
|        - | 16968 | `	/* Check if the referenced object already exists */` |
|  3189399 | 16969 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3189399 | 16970 | `	if( pRef == 0 ){` |
|        - | 16971 | `		/* Not such entry */` |
|   111393 | 16972 | `		return SXERR_NOTFOUND;` |
|        - | 16973 | `	}` |
|        - | 16974 | `	/* Remove the desired entry */` |
|  3078011 | 16975 | `	if( pEntry ){` |
|        - | 16976 | `		SyHashEntry **apEntry;` |
|       77 | 16977 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      267 | 16978 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      195 | 16979 | `			if( apEntry[n] == pEntry ){` |
|        - | 16980 | `				/* Nullify the entry */` |
|       77 | 16981 | `				apEntry[n] = 0;` |
|        - | 16982 | `				/*` |
|        - | 16983 | `				 * NOTE:` |
|        - | 16984 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 16985 | `				 * we avoid wasting spaces.` |
|        - | 16986 | `				 */` |
|       36 | 16987 | `			}` |
|      100 | 16988 | `		}` |
|       36 | 16989 | `	}` |
|  3078011 | 16990 | `	if( pMapEntry ){` |
|        - | 16991 | `		ph7_hashmap_node **apNode;` |
|  3077939 | 16992 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6155967 | 16993 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3078033 | 16994 | `			if( apNode[n] == pMapEntry ){` |
|        - | 16995 | `				/* nullify the entry */` |
|  3077939 | 16996 | `				apNode[n] = 0;` |
|  1538967 | 16997 | `			}` |
|  1539019 | 16998 | `		}` |
|  1538967 | 16999 | `	}` |
|  3078011 | 17000 | `	return SXRET_OK;` |
|  1594702 | 17001 |  |
|        - | 17002 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 17003 | `/*` |
|        - | 17004 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 17005 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 17006 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 17007 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 17008 | ` * For more information on how to register IO stream devices,please` |
|        - | 17009 | ` * refer to the official documentation.` |
|        - | 17010 | ` */` |
|    29936 | 17011 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 17012 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 17013 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 17014 | `	int nByte              /* *pzDevice length*/` |
|        - | 17015 | `	)` |
|        5 | 17016 |  |
|        - | 17017 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 17018 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 17019 | `	SyString sDev,sCur;` |
|        - | 17020 | `	sxu32 n,nEntry;` |
|        - | 17021 | `	int rc;` |
|        - | 17022 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29941 | 17023 | `	zNext = zCur = zIn = *pzDevice;` |
|    29941 | 17024 | `	zEnd = &zIn[nByte];` |
|  1909699 | 17025 | `	while( zIn < zEnd ){` |
|  1879765 | 17026 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 17027 | `			/* Got one */` |
|        3 | 17028 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 17029 | `			break;` |
|        - | 17030 | `		}` |
|        - | 17031 | `		/* Advance the cursor */` |
|  1879763 | 17032 | `		zIn++;` |
|        5 | 17033 | `	}` |
|    29941 | 17034 | `	if( zIn >= zEnd ){` |
|        - | 17035 | `		/* No such scheme,return the default stream */` |
|    29939 | 17036 | `		return pVm->pDefStream;` |
|        - | 17037 | `	}` |
|        3 | 17038 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 17039 | `	/* Remove leading and trailing white spaces */` |
|        3 | 17040 | `	SyStringFullTrim(&sDev);` |
|        - | 17041 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 17042 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 17043 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 17044 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 17045 | `		pStream = apStream[n];` |
|        3 | 17046 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 17047 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 17048 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 17049 | `		if( rc == 0 ){` |
|        - | 17050 | `			/* Stream device found */` |
|        3 | 17051 | `			*pzDevice = zNext;` |
|        3 | 17052 | `			return pStream;` |
|        - | 17053 | `		}` |
|      ! 0 | 17054 | `	}` |
|        - | 17055 | `	/* No such stream,return NULL */` |
|      ! 0 | 17056 | `	return 0;` |
|    14973 | 17057 |  |
|        - | 17058 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 17059 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 17060 |  |
