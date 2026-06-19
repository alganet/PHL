# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6738/8617 lines (78.19%)

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
|   918686 |   140 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   141 |  |
|   918688 |   142 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   143 | `		return TRUE;` |
|        - |   144 | `	}` |
|   918654 |   145 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   146 | `		return TRUE;` |
|        - |   147 | `	}` |
|   918644 |   148 | `	return FALSE;` |
|   459367 |   149 |  |
|        - |   150 | `/*` |
|        - |   151 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   152 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   153 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   154 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   155 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   156 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   157 | ` * still go through the existing numeric coercion.` |
|        - |   158 | ` */` |
|   335908 |   159 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   160 |  |
|        - |   161 | `	SyString sStr;` |
|   335910 |   162 | `	sxu8 bReal = FALSE;` |
|   335910 |   163 | `	const char *zTail = 0;` |
|        - |   164 | `	const char *zEnd;` |
|   335910 |   165 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   335840 |   166 | `		return FALSE;` |
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
|   167978 |   183 |  |
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
|   634828 |   202 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   203 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   204 | `	const SyString *pName,  /* Constant name */` |
|        - |   205 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   206 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   207 | `	)` |
|        2 |   208 |  |
|        - |   209 | `	ph7_constant *pCons;` |
|        - |   210 | `	SyHashEntry *pEntry;` |
|        - |   211 | `	char *zDupName;` |
|        - |   212 | `	sxi32 rc;` |
|   634830 |   213 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   634830 |   214 | `	if( pEntry ){` |
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
|   634826 |   230 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   634826 |   231 | `	if( pCons == 0 ){` |
|      ! 0 |   232 | `		return 0;` |
|        - |   233 | `	}` |
|        - |   234 | `	/* Duplicate constant name */` |
|   634826 |   235 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   634826 |   236 | `	if( zDupName == 0 ){` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   238 | `		return 0;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* Install the constant */` |
|   634826 |   241 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   634826 |   242 | `	pCons->xExpand = xExpand;` |
|   634826 |   243 | `	pCons->pUserData = pUserData;` |
|   634826 |   244 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   634826 |   245 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   246 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   247 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   248 | `		return rc;` |
|        - |   249 | `	}` |
|        - |   250 | `	/* All done,constant can be invoked from PHP code */` |
|   634826 |   251 | `	return SXRET_OK;` |
|   317416 |   252 |  |
|        - |   253 | `/*` |
|        - |   254 | ` * Allocate a new foreign function instance.` |
|        - |   255 | ` * This function return SXRET_OK on success. Any other` |
|        - |   256 | ` * return value indicates failure.` |
|        - |   257 | ` * Please refer to the official documentation for an introduction to` |
|        - |   258 | ` * the foreign function mechanism.` |
|        - |   259 | ` */` |
|  1394626 |   260 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   261 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   262 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   263 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   264 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   265 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   266 | `	)` |
|        2 |   267 |  |
|        - |   268 | `	ph7_user_func *pFunc;` |
|        - |   269 | `	char *zDup;` |
|        - |   270 | `	/* Allocate a new user function */` |
|  1394628 |   271 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1394628 |   272 | `	if( pFunc == 0 ){` |
|      ! 0 |   273 | `		return SXERR_MEM;` |
|        - |   274 | `	}` |
|        - |   275 | `	/* Duplicate function name */` |
|  1394628 |   276 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1394628 |   277 | `	if( zDup == 0 ){` |
|      ! 0 |   278 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   279 | `		return SXERR_MEM;` |
|        - |   280 | `	}` |
|        - |   281 | `	/* Zero the structure */` |
|  1394628 |   282 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   283 | `	/* Initialize structure fields */` |
|  1394628 |   284 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1394628 |   285 | `	pFunc->pVm   = pVm;` |
|  1394628 |   286 | `	pFunc->xFunc = xFunc;` |
|  1394628 |   287 | `	pFunc->pUserData = pUserData;` |
|  1394628 |   288 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   289 | `	/* Write a pointer to the new function */` |
|  1394628 |   290 | `	*ppOut = pFunc;` |
|  1394628 |   291 | `	return SXRET_OK;` |
|   697315 |   292 |  |
|        - |   293 | `/*` |
|        - |   294 | ` * Install a foreign function and it's associated callback so that` |
|        - |   295 | ` * it can be invoked from the target PHP code.` |
|        - |   296 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   297 | ` * return value indicates failure.` |
|        - |   298 | ` * Please refer to the official documentation for an introduction to` |
|        - |   299 | ` * the foreign function mechanism.` |
|        - |   300 | ` */` |
|  1397460 |   301 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   302 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   303 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   304 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   305 | `	void *pUserData           /* Foreign function private data */` |
|        - |   306 | `	)` |
|        2 |   307 |  |
|        - |   308 | `	ph7_user_func *pFunc;` |
|        - |   309 | `	SyHashEntry *pEntry;` |
|        - |   310 | `	sxi32 rc;` |
|        - |   311 | `	/* Overwrite any previously registered function with the same name */` |
|  1397462 |   312 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1397462 |   313 | `	if( pEntry ){` |
|     2836 |   314 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2836 |   315 | `		pFunc->pUserData = pUserData;` |
|     2836 |   316 | `		pFunc->xFunc = xFunc;` |
|     2836 |   317 | `		SySetReset(&pFunc->aAux);` |
|     2836 |   318 | `		return SXRET_OK;` |
|        - |   319 | `	}` |
|        - |   320 | `	/* Create a new user function */` |
|  1394628 |   321 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1394628 |   322 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   323 | `		return rc;` |
|        - |   324 | `	}` |
|        - |   325 | `	/* Install the function in the corresponding hashtable */` |
|  1394628 |   326 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1394628 |   327 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   328 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   329 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   330 | `		return rc;` |
|        - |   331 | `	}` |
|        - |   332 | `	/* User function successfully installed */` |
|  1394628 |   333 | `	return SXRET_OK;` |
|   698732 |   334 |  |
|        - |   335 | `/*` |
|        - |   336 | ` * Initialize a VM function.` |
|        - |   337 | ` */` |
|   278956 |   338 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   339 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   340 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   341 | `	const char *zName,  /* Function name */` |
|        - |   342 | `	sxu32 nByte,        /* zName length */` |
|        - |   343 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   344 | `	void *pUserData     /* Function private data */` |
|        - |   345 | `	)` |
|        2 |   346 |  |
|        - |   347 | `	/* Zero the structure */` |
|   278958 |   348 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   349 | `	/* Initialize structure fields */` |
|        - |   350 | `	/* Arguments container */` |
|   278958 |   351 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   352 | `	/* Static variable container */` |
|   278958 |   353 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   354 | `	/* Bytecode container */` |
|   278958 |   355 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   356 | `    /* Preallocate some instruction slots */` |
|   278958 |   357 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   358 | `	/* Closure environment */` |
|   278958 |   359 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   360 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   278958 |   361 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   278958 |   362 | `	pFunc->iFlags = iFlags;` |
|   278958 |   363 | `	pFunc->pUserData = pUserData;` |
|        - |   364 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   365 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   278958 |   366 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   278958 |   367 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   278958 |   368 | `	return SXRET_OK;` |
|        2 |   369 |  |
|        - |   370 | `/*` |
|        - |   371 | ` * Namespace-aware function lookup.` |
|        - |   372 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   373 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   374 | ` */` |
|        - |   375 | `/*` |
|        - |   376 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   377 | ` */` |
|  1461052 |   378 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   379 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   380 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   381 | `	SyString *pName     /* Function name */` |
|        - |   382 | `	)` |
|        2 |   383 |  |
|        - |   384 | `	SyHashEntry *pEntry;` |
|        - |   385 | `	sxi32 rc;` |
|  1461054 |   386 | `	if( pName == 0 ){` |
|        - |   387 | `		/* Use the built-in name */` |
|    42010 |   388 | `		pName = &pFunc->sName;` |
|    21004 |   389 | `	}` |
|        - |   390 | `	/* Check for duplicates (functions with the same name) first */` |
|  1461054 |   391 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  1461054 |   392 | `	if( pEntry ){` |
|  1264232 |   393 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  1264232 |   394 | `		if( pLink != pFunc ){` |
|        - |   395 | `			/* Link */` |
|      188 |   396 | `			pFunc->pNextName = pLink;` |
|      188 |   397 | `			pEntry->pUserData = pFunc;` |
|       93 |   398 | `		}` |
|  1264232 |   399 | `		return SXRET_OK;` |
|        - |   400 | `	}` |
|        - |   401 | `	/* First time seen */` |
|   196824 |   402 | `	pFunc->pNextName = 0;` |
|   196824 |   403 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   196824 |   404 | `	return rc;` |
|   730528 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   408 | ` */` |
|   120640 |   409 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   410 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   411 | `	ph7_class *pClass /* Target Class */` |
|        - |   412 | `	)` |
|        2 |   413 |  |
|   120642 |   414 | `	SyString *pName = &pClass->sName;` |
|        - |   415 | `	SyHashEntry *pEntry;` |
|        - |   416 | `	sxi32 rc;` |
|        - |   417 | `	/* Check for duplicates */` |
|   120642 |   418 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   120642 |   419 | `	if( pEntry ){` |
|       31 |   420 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   421 | `		/* Link entry with the same name */` |
|       31 |   422 | `		pClass->pNextName = pLink;` |
|       31 |   423 | `		pEntry->pUserData = pClass;` |
|       31 |   424 | `		return SXRET_OK;` |
|        - |   425 | `	}` |
|   120612 |   426 | `	pClass->pNextName = 0;` |
|        - |   427 | `	/* Perform a simple hashtable insertion */` |
|   120612 |   428 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   120612 |   429 | `	return rc;` |
|    60322 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Instruction builder interface.` |
|        - |   433 | ` */` |
|  4269478 |   434 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   435 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   436 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   437 | `	sxi32 iP1,    /* First operand */` |
|        - |   438 | `	sxu32 iP2,    /* Second operand */` |
|        - |   439 | `	void *p3,     /* Third operand */` |
|        - |   440 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   441 | `	)` |
|        2 |   442 |  |
|        - |   443 | `	VmInstr sInstr;` |
|        - |   444 | `	sxi32 rc;` |
|        - |   445 | `	/* Fill the VM instruction */` |
|  4269480 |   446 | `	sInstr.iOp = (sxu8)iOp;` |
|  4269480 |   447 | `	sInstr.iP1 = iP1;` |
|  4269480 |   448 | `	sInstr.iP2 = iP2;` |
|  4269480 |   449 | `	sInstr.p3  = p3;` |
|  4269480 |   450 | `	if( pIndex ){` |
|        - |   451 | `		/* Instruction index in the bytecode array */` |
|   231850 |   452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   115924 |   453 | `	}` |
|        - |   454 | `	/* Finally,record the instruction */` |
|  4269480 |   455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4269480 |   456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   458 | `		/* Fall throw */` |
|      ! 0 |   459 | `	}` |
|  4269480 |   460 | `	return rc;` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Swap the current bytecode container with the given one.` |
|        - |   464 | ` */` |
|   554016 |   465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   466 |  |
|   554018 |   467 | `	if( pContainer == 0 ){` |
|        - |   468 | `		/* Point to the default container */` |
|      ! 0 |   469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   470 | `	}else{` |
|        - |   471 | `		/* Change container */` |
|   554018 |   472 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   473 | `	}` |
|   554018 |   474 | `	return SXRET_OK;` |
|        2 |   475 |  |
|        - |   476 | `/*` |
|        - |   477 | ` * Return the current bytecode container.` |
|        - |   478 | ` */` |
|   277008 |   479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   480 |  |
|   277010 |   481 | `	return pVm->pByteContainer;` |
|        2 |   482 |  |
|        - |   483 | `/*` |
|        - |   484 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   485 | ` */` |
|   228624 |   486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   487 |  |
|        - |   488 | `	VmInstr *pInstr;` |
|   228626 |   489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   228626 |   490 | `	return pInstr;` |
|        2 |   491 |  |
|        - |   492 | `/*` |
|        - |   493 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   494 | ` */` |
|  1282326 |   495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   496 |  |
|  1282328 |   497 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   498 |  |
|        - |   499 | `/*` |
|        - |   500 | ` * Pop the last VM instruction.` |
|        - |   501 | ` */` |
|   211518 |   502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   503 |  |
|   211520 |   504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   505 |  |
|        - |   506 | `/*` |
|        - |   507 | ` * Peek the last VM instruction.` |
|        - |   508 | ` */` |
|   840792 |   509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   510 |  |
|   840794 |   511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   512 |  |
|    33556 |   513 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   514 |  |
|        - |   515 | `	VmInstr *aInstr;` |
|        - |   516 | `	sxu32 n;` |
|    33558 |   517 | `	n = SySetUsed(pVm->pByteContainer);` |
|    33558 |   518 | `	if( n < 2 ){` |
|      ! 0 |   519 | `		return 0;` |
|        - |   520 | `	}` |
|    33558 |   521 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    33558 |   522 | `	return &aInstr[n - 2];` |
|    16780 |   523 |  |
|        - |   524 | `/*` |
|        - |   525 | ` * Allocate a new virtual machine frame.` |
|        - |   526 | ` */` |
|    22546 |   527 | `static VmFrame * VmNewFrame(` |
|        - |   528 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   529 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   531 | `	)` |
|        2 |   532 |  |
|        - |   533 | `	VmFrame *pFrame;` |
|        - |   534 | `	/* Allocate a new vm frame */` |
|    22548 |   535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    22548 |   536 | `	if( pFrame == 0 ){` |
|      ! 0 |   537 | `		return 0;` |
|        - |   538 | `	}` |
|        - |   539 | `	/* Zero the structure */` |
|    22548 |   540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   541 | `	/* Initialize frame fields */` |
|    22548 |   542 | `	pFrame->pUserData = pUserData;` |
|    22548 |   543 | `	pFrame->pThis = pThis;` |
|    22548 |   544 | `	pFrame->pVm = pVm;` |
|    22548 |   545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    22548 |   546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    22548 |   547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    22548 |   548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    22548 |   549 | `	return pFrame;` |
|    11275 |   550 |  |
|        - |   551 | `/* Forward declaration */` |
|        - |   552 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   553 | `/*` |
|        - |   554 | ` * Enter a VM frame.` |
|        - |   555 | ` */` |
|    22500 |   556 | `static sxi32 VmEnterFrame(` |
|        - |   557 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   558 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   559 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   560 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   561 | `	)` |
|        2 |   562 |  |
|        - |   563 | `	VmFrame *pFrame;` |
|        - |   564 | `	/* Allocate a new frame */` |
|    22502 |   565 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    22502 |   566 | `	if( pFrame == 0 ){` |
|      ! 0 |   567 | `		return SXERR_MEM;` |
|        - |   568 | `	}` |
|        - |   569 | `	/* Link to the list of active VM frame */` |
|    22502 |   570 | `	pFrame->pParent = pVm->pFrame;` |
|    22502 |   571 | `	pVm->pFrame = pFrame;` |
|    22502 |   572 | `	if( ppFrame ){` |
|        - |   573 | `		/* Write a pointer to the new VM frame */` |
|    19348 |   574 | `		*ppFrame = pFrame;` |
|     9673 |   575 | `	}` |
|    22502 |   576 | `	return SXRET_OK;` |
|    11252 |   577 |  |
|        - |   578 | `/*` |
|        - |   579 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   580 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   581 | ` * information.` |
|        - |   582 | ` */` |
|       70 |   583 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   584 |  |
|        - |   585 | `	VmFrame *pTarget,*pFrame;` |
|       72 |   586 | `	SyHashEntry *pEntry = 0;` |
|        - |   587 | `	sxi32 rc;` |
|        - |   588 | `	/* Point to the upper frame */` |
|       72 |   589 | `	pFrame = pVm->pFrame;` |
|       72 |   590 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       72 |   591 | `	pTarget = pFrame;` |
|       72 |   592 | `	pFrame = pTarget->pParent;` |
|       72 |   593 | `	while( pFrame ){` |
|       72 |   594 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   595 | `			/* Query the current frame */` |
|       72 |   596 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       72 |   597 | `			if( pEntry ){` |
|        - |   598 | `				/* Variable found */` |
|       72 |   599 | `				break;` |
|        - |   600 | `			}` |
|      ! 0 |   601 | `		}` |
|        - |   602 | `		/* Point to the upper frame */` |
|      ! 0 |   603 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   604 | `	}` |
|       72 |   605 | `	if( pEntry == 0 ){` |
|        - |   606 | `		/* Inexistant variable */` |
|      ! 0 |   607 | `		return SXERR_NOTFOUND;` |
|        - |   608 | `	}` |
|        - |   609 | `	/* Link to the current frame */` |
|       72 |   610 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       72 |   611 | `	if( rc == SXRET_OK ){` |
|        - |   612 | `		sxu32 nIdx;` |
|       72 |   613 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       72 |   614 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       35 |   615 | `	}` |
|       72 |   616 | `	return rc;` |
|       37 |   617 |  |
|        - |   618 | `/*` |
|        - |   619 | ` * Leave the top-most active frame.` |
|        - |   620 | ` */` |
|    19342 |   621 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   622 |  |
|    19344 |   623 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    19344 |   624 | `	if( pCurFrame ){` |
|        - |   625 | `		/* Unlink from the list of active VM frame */` |
|    19344 |   626 | `		pVm->pFrame = pCurFrame->pParent;` |
|    19344 |   627 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   628 | `			VmSlot  *aSlot;` |
|        - |   629 | `			sxu32 n;` |
|        - |   630 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    18970 |   631 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   124480 |   632 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   633 | `				/* Unset the local variable */` |
|   105512 |   634 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    52757 |   635 | `			}` |
|        - |   636 | `			/* Remove local reference */` |
|    18970 |   637 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   124554 |   638 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   105586 |   639 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    52794 |   640 | `			}` |
|     9484 |   641 | `		}` |
|        - |   642 | `		/* Release internal containers */` |
|    19344 |   643 | `		SyHashRelease(&pCurFrame->hVar);` |
|    19344 |   644 | `		SySetRelease(&pCurFrame->sArg);` |
|    19344 |   645 | `		SySetRelease(&pCurFrame->sLocal);` |
|    19344 |   646 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   647 | `		/* Release the whole structure */` |
|    19344 |   648 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     9671 |   649 | `	}` |
|    19344 |   650 |  |
|        - |   651 | `/*` |
|        - |   652 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   653 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   654 | ` * should be skipped when looking for the real execution context.` |
|        - |   655 | ` */` |
|  7124814 |   656 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   657 |  |
|  7127036 |   658 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     2222 |   659 | `		pFrame = pFrame->pParent;` |
|        2 |   660 | `	}` |
|  7124816 |   661 | `	return pFrame;` |
|        2 |   662 |  |
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
|        - |   683 | ` * Compare two functions signature and return the comparison result.` |
|        - |   684 | ` */` |
|      836 |   685 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   686 |  |
|      837 |   687 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   688 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   689 | `	const char *zSin = pSecond->zString;` |
|      837 |   690 | `	const char *zFin = pFirst->zString;` |
|      837 |   691 | `	const char *zPtr = zFin;` |
|      421 |   692 | `	for(;;){` |
|      843 |   693 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   694 | `			break;` |
|        - |   695 | `		}` |
|       19 |   696 | `		if( zFin[0] != zSin[0] ){` |
|        - |   697 | `			/* mismatch */` |
|       13 |   698 | `			break;` |
|        - |   699 | `		}` |
|        7 |   700 | `		zFin++;` |
|        7 |   701 | `		zSin++;` |
|        1 |   702 | `	}` |
|      837 |   703 | `	return (int)(zFin-zPtr);` |
|        1 |   704 |  |
|        - |   705 | `/*` |
|        - |   706 | ` * Select the appropriate VM function for the current call context.` |
|        - |   707 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   708 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   709 | ` * Refer to the official documentation for more information.` |
|        - |   710 | ` */` |
|      138 |   711 | `static ph7_vm_func * VmOverload(` |
|        - |   712 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   713 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   714 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   715 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   716 | `	)` |
|        2 |   717 |  |
|        - |   718 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   719 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   720 | `	ph7_vm_func *pLink;` |
|        - |   721 | `	SyString sArgSig;` |
|        - |   722 | `	SyBlob sSig;` |
|        - |   723 |  |
|      140 |   724 | `	pLink = pList;` |
|      140 |   725 | `	i = 0;` |
|        - |   726 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   727 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   728 | `		if( pLink == 0 ){` |
|       78 |   729 | `			break;` |
|        - |   730 | `		}` |
|      948 |   731 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   732 | `			/* Candidate for overloading */` |
|      902 |   733 | `			apSet[i++] = pLink;` |
|      450 |   734 | `		}` |
|        - |   735 | `		/* Point to the next entry */` |
|      948 |   736 | `		pLink = pLink->pNextName;` |
|        2 |   737 | `	}` |
|      140 |   738 | `	if( i < 1 ){` |
|        - |   739 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   740 | `		return pList;` |
|        - |   741 | `	}` |
|      140 |   742 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   743 | `		/* Return the only candidate */` |
|       32 |   744 | `		return apSet[0];` |
|        - |   745 | `	}` |
|        - |   746 | `	/* Calculate function signature */` |
|      109 |   747 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   748 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   749 | `		int c = 'n'; /* null */` |
|      259 |   750 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   751 | `			/* Hashmap */` |
|       45 |   752 | `			c = 'h';` |
|      237 |   753 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   754 | `			/* bool */` |
|      ! 0 |   755 | `			c = 'b';` |
|      215 |   756 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   757 | `			/* int */` |
|        7 |   758 | `			c = 'i';` |
|      212 |   759 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   760 | `			/* String */` |
|      107 |   761 | `			c = 's';` |
|      156 |   762 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   763 | `			/* Float */` |
|      ! 0 |   764 | `			c = 'f';` |
|      103 |   765 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   766 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   767 | `			int marker = 'o';` |
|        3 |   768 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   769 | `			SyString *pName = &pClass->sName;` |
|        3 |   770 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   771 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   772 | `			c = -1;` |
|        1 |   773 | `		}` |
|      259 |   774 | `		if( c > 0 ){` |
|      257 |   775 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   776 | `		}` |
|      130 |   777 | `	}` |
|      109 |   778 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   779 | `	iTarget = 0;` |
|      109 |   780 | `	iMax = -1;` |
|        - |   781 | `	/* Select the appropriate function */` |
|      945 |   782 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   783 | `		/* Compare the two signatures */` |
|      837 |   784 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   785 | `		if( iCur > iMax ){` |
|      113 |   786 | `			iMax = iCur;` |
|      113 |   787 | `			iTarget = j;` |
|       56 |   788 | `		}` |
|      419 |   789 | `	}` |
|      109 |   790 | `	SyBlobRelease(&sSig);` |
|        - |   791 | `	/* Appropriate function for the current call context */` |
|      109 |   792 | `	return apSet[iTarget];` |
|       71 |   793 |  |
|        - |   794 | `/* Forward declaration */` |
|        - |   795 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   796 | `/*` |
|        - |   797 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   798 | ` * it can be instanciated from the executed PHP script.` |
|        - |   799 | ` */` |
|        - |   800 | `/*` |
|        - |   801 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|        - |   802 | ` * This is the per-execution part of mounting a class: every static/const` |
|        - |   803 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|        - |   804 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|        - |   805 | ` * properties register their enforcement slot. It is factored out of` |
|        - |   806 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|        - |   807 | ` * reuse without re-installing the (compile-time) methods.` |
|        - |   808 | ` */` |
|   355052 |   809 | `static sxi32 VmMountUserClassAttrs(` |
|        - |   810 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   811 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - |   812 | `	)` |
|        2 |   813 |  |
|        - |   814 | `	ph7_class_attr *pAttr;` |
|        - |   815 | `	SyHashEntry *pEntry;` |
|        - |   816 | `	/* Reset the loop cursor */` |
|   355054 |   817 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   818 | `	/* Process only static and constant attribute */` |
|  1404512 |   819 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   820 | `		/* Extract the current attribute */` |
|   871934 |   821 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   871934 |   822 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   823 | `			ph7_value *pMemObj;` |
|        - |   824 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1828 |   825 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1828 |   826 | `			if( pMemObj == 0 ){` |
|      ! 0 |   827 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   828 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   829 | `					&pClass->sName,&pAttr->sName` |
|        - |   830 | `					);` |
|      ! 0 |   831 | `				return SXERR_MEM;` |
|        - |   832 | `			}` |
|     1828 |   833 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   834 | `				/* Initialize attribute default value (any complex expression) */` |
|     1824 |   835 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      911 |   836 | `			}` |
|        - |   837 | `			/* Record attribute index */` |
|     1828 |   838 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   839 | `			/* Install static attribute in the reference table */` |
|     1828 |   840 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   841 | `			/* If this is a typed static property, register the slot so the` |
|        - |   842 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   843 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   844 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1828 |   845 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       16 |   846 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       16 |   847 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   848 | `					return SXERR_MEM;` |
|        - |   849 | `				}` |
|       16 |   850 | `				pVmAttrS->pAttr = pAttr;` |
|       16 |   851 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       16 |   852 | `				pVmAttrS->iState = 0;` |
|       16 |   853 | `				pVmAttrS->pOwner = pClass;` |
|        - |   854 | `				/* Static typed property with no default starts uninitialized */` |
|       14 |   855 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|       11 |   856 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        6 |   857 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   858 | `				}` |
|       16 |   859 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   860 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   861 | `					return SXERR_MEM;` |
|        - |   862 | `				}` |
|        7 |   863 | `			}` |
|      913 |   864 | `		}` |
|        2 |   865 | `	}` |
|   355054 |   866 | `	return SXRET_OK;` |
|   177528 |   867 |  |
|   354820 |   868 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   869 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   870 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   871 | `	)` |
|        2 |   872 |  |
|        - |   873 | `	ph7_class_method *pMeth;` |
|        - |   874 | `	SyHashEntry *pEntry;` |
|        - |   875 | `	sxi32 rc;` |
|        - |   876 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   354822 |   877 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   354822 |   878 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   879 | `		return rc;` |
|        - |   880 | `	}` |
|        - |   881 | `	/* Install class methods */` |
|   354822 |   882 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   883 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   884 | `		 */` |
|   192452 |   885 | `		return SXRET_OK;` |
|        - |   886 | `	}` |
|        - |   887 | `	/* Create constructor alias if not yet done */` |
|   162372 |   888 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   889 | `		/* User constructor with the same base class name */` |
|     6692 |   890 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6692 |   891 | `		if( pEntry ){` |
|      ! 0 |   892 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   893 | `			/* Create the alias */` |
|      ! 0 |   894 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   895 | `		}` |
|     3345 |   896 | `	}` |
|        - |   897 | `	/* Install the methods now */` |
|   162372 |   898 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  1662609 |   899 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  1419054 |   900 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  1419054 |   901 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  1419046 |   902 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  1419046 |   903 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   904 | `				return rc;` |
|        - |   905 | `			}` |
|   709522 |   906 | `		}` |
|        2 |   907 | `	}` |
|        - |   908 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   162372 |   909 | `	pClass->bMounted = TRUE;` |
|   162372 |   910 | `	return SXRET_OK;` |
|   177412 |   911 |  |
|        - |   912 | `/*` |
|        - |   913 | ` * Allocate a private frame for attributes of the given` |
|        - |   914 | ` * class instance (Object in the PHP jargon).` |
|        - |   915 | ` */` |
|     2104 |   916 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   917 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   918 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   919 | `	)` |
|        2 |   920 |  |
|     2106 |   921 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   922 | `	ph7_class_attr *pAttr;` |
|        - |   923 | `	SyHashEntry *pEntry;` |
|        - |   924 | `	sxi32 rc;` |
|        - |   925 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2106 |   926 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8724 |   927 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   928 | `		VmClassAttr *pVmAttr;` |
|        - |   929 | `		/* Extract the current attribute */` |
|     6620 |   930 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     6620 |   931 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     6620 |   932 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   933 | `			return SXERR_MEM;` |
|        - |   934 | `		}` |
|     6620 |   935 | `		pVmAttr->pAttr = pAttr;` |
|     6620 |   936 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   937 | `			ph7_value *pMemObj;` |
|        - |   938 | `			/* Reserve a memory object for this attribute */` |
|     6594 |   939 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     6594 |   940 | `			if( pMemObj == 0 ){` |
|      ! 0 |   941 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   942 | `				return SXERR_MEM;` |
|        - |   943 | `			}` |
|     6594 |   944 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     6594 |   945 | `			pVmAttr->iState = 0;` |
|     6594 |   946 | `			pVmAttr->pOwner = pClass;` |
|     6594 |   947 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   948 | `				/* Initialize attribute default value (any complex expression) */` |
|     2272 |   949 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     5459 |   950 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   951 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   952 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       74 |   953 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |   954 | `			}` |
|     6594 |   955 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     6594 |   956 | `			if( rc != SXRET_OK ){` |
|        - |   957 | `				VmSlot sSlot;` |
|        - |   958 | `				/* Restore memory object */` |
|      ! 0 |   959 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   960 | `				sSlot.pUserData = 0;` |
|      ! 0 |   961 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   962 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   963 | `				return SXERR_MEM;` |
|        - |   964 | `			}` |
|        - |   965 | `			/* Install attribute in the reference table */` |
|     6594 |   966 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   967 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   968 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   969 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     6594 |   970 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      178 |   971 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      178 |   972 | `				if( rc != SXRET_OK ){` |
|        - |   973 | `					VmSlot sSlot;` |
|      ! 0 |   974 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   975 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   976 | `					sSlot.pUserData = 0;` |
|      ! 0 |   977 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   978 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   979 | `					return SXERR_MEM;` |
|        - |   980 | `				}` |
|       88 |   981 | `			}` |
|     3298 |   982 | `		}else{` |
|        - |   983 | `			/* Install static/constant attribute */` |
|       28 |   984 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       28 |   985 | `			pVmAttr->iState = 0;` |
|       28 |   986 | `			pVmAttr->pOwner = pClass;` |
|       28 |   987 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       28 |   988 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   989 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   990 | `				return SXERR_MEM;` |
|        - |   991 | `			}` |
|        - |   992 | `		}` |
|        2 |   993 | `	}` |
|     2106 |   994 | `	return SXRET_OK;` |
|     1054 |   995 |  |
|        - |   996 | `/* Forward declaration */` |
|        - |   997 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   998 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   999 | `/*` |
|        - |  1000 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |  1001 | ` */` |
|        - |  1002 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |  1003 | `/*` |
|        - |  1004 | ` * Reserve a constant memory object.` |
|        - |  1005 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1006 | ` */` |
|   457126 |  1007 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |  1008 |  |
|        - |  1009 | `	ph7_value *pObj;` |
|        - |  1010 | `	sxi32 rc;` |
|   457128 |  1011 | `	if( pIndex ){` |
|        - |  1012 | `		/* Object index in the object table */` |
|   447684 |  1013 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   223841 |  1014 | `	}` |
|        - |  1015 | `	/* Reserve a slot for the new object */` |
|   457128 |  1016 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   457128 |  1017 | `	if( rc != SXRET_OK ){` |
|        - |  1018 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1019 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1020 | `		 */` |
|      ! 0 |  1021 | `		return 0;` |
|        - |  1022 | `	}` |
|   457128 |  1023 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   457128 |  1024 | `	return pObj;` |
|   228565 |  1025 |  |
|        - |  1026 | `/*` |
|        - |  1027 | ` * Reserve a memory object.` |
|        - |  1028 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1029 | ` */` |
|  2152074 |  1030 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |  1031 |  |
|        - |  1032 | `	ph7_value *pObj;` |
|        - |  1033 | `	sxi32 rc;` |
|  2152076 |  1034 | `	if( pIndex ){` |
|        - |  1035 | `		/* Object index in the object table */` |
|  2152076 |  1036 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1076037 |  1037 | `	}` |
|        - |  1038 | `	/* Reserve a slot for the new object */` |
|  2152076 |  1039 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2152076 |  1040 | `	if( rc != SXRET_OK ){` |
|        - |  1041 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1042 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1043 | `		 */` |
|      ! 0 |  1044 | `		return 0;` |
|        - |  1045 | `	}` |
|  2152076 |  1046 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2152076 |  1047 | `	return pObj;` |
|  1076039 |  1048 |  |
|        - |  1049 | `/* Forward declaration */` |
|        - |  1050 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |  1051 | `/* Forward declarations for Fiber C functions */` |
|        - |  1052 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1053 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1054 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1055 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1056 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1057 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1058 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1059 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1060 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1061 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1062 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |  1063 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |  1064 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1065 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  1066 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |  1067 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1068 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |  1069 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |  1070 | `static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1071 | `	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);` |
|        - |  1072 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);` |
|        - |  1073 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |  1074 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1075 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |  1076 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1077 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1078 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1079 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1080 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1081 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1082 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1083 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1084 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1085 | `/*` |
|        - |  1086 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  1087 | ` * directly as foreign functions.` |
|        - |  1088 | ` */` |
|        - |  1089 | `#define PH7_BUILTIN_LIB \` |
|        - |  1090 | `	"interface Throwable {"\` |
|        - |  1091 | `	"public function getMessage();"\` |
|        - |  1092 | `	"public function getCode();"\` |
|        - |  1093 | `	"public function getFile();"\` |
|        - |  1094 | `	"public function getLine();"\` |
|        - |  1095 | `	"public function getTrace();"\` |
|        - |  1096 | `	"public function getTraceAsString();"\` |
|        - |  1097 | `	"public function getPrevious();"\` |
|        - |  1098 | `	"public function __toString();"\` |
|        - |  1099 | `	"}"\` |
|        - |  1100 | `	"interface Traversable {}"\` |
|        - |  1101 | `	"interface ArrayAccess {"\` |
|        - |  1102 | `	"public function offsetExists($offset);"\` |
|        - |  1103 | `	"public function offsetGet($offset);"\` |
|        - |  1104 | `	"public function offsetSet($offset, $value);"\` |
|        - |  1105 | `	"public function offsetUnset($offset);"\` |
|        - |  1106 | `	"}"\` |
|        - |  1107 | `	"interface Countable {"\` |
|        - |  1108 | `	"public function count();"\` |
|        - |  1109 | `	"}"\` |
|        - |  1110 | `	"interface Stringable {"\` |
|        - |  1111 | `	"public function __toString();"\` |
|        - |  1112 | `	"}"\` |
|        - |  1113 | `	"interface JsonSerializable {"\` |
|        - |  1114 | `	"public function jsonSerialize();"\` |
|        - |  1115 | `	"}"\` |
|        - |  1116 | `	"interface UnitEnum {"\` |
|        - |  1117 | `	"public static function cases();"\` |
|        - |  1118 | `	"}"\` |
|        - |  1119 | `	"interface BackedEnum extends UnitEnum {"\` |
|        - |  1120 | `	"public static function from($value);"\` |
|        - |  1121 | `	"public static function tryFrom($value);"\` |
|        - |  1122 | `	"}"\` |
|        - |  1123 | `	"class Exception implements Throwable { "\` |
|        - |  1124 | `    "protected $message = '';"\` |
|        - |  1125 | `    "protected $code = 0;"\` |
|        - |  1126 | `    "protected $file;"\` |
|        - |  1127 | `    "protected $line;"\` |
|        - |  1128 | `    "protected $trace;"\` |
|        - |  1129 | `    "protected $previous;"\` |
|        - |  1130 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1131 | `	"   if( isset($message) ){"\` |
|        - |  1132 | `	"	  $this->message = $message;"\` |
|        - |  1133 | `	"   }"\` |
|        - |  1134 | `	"   $this->code = $code;"\` |
|        - |  1135 | `	"   $this->file = __FILE__;"\` |
|        - |  1136 | `	"   $this->line = __LINE__;"\` |
|        - |  1137 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1138 | `	"   if( isset($previous) ){"\` |
|        - |  1139 | `	"     $this->previous = $previous;"\` |
|        - |  1140 | `	"   }"\` |
|        - |  1141 | `	"}"\` |
|        - |  1142 | `	"public function getMessage(){"\` |
|        - |  1143 | `	"   return $this->message;"\` |
|        - |  1144 | `	"}"\` |
|        - |  1145 | `	" public function getCode(){"\` |
|        - |  1146 | `	"  return $this->code;"\` |
|        - |  1147 | `	"}"\` |
|        - |  1148 | `	"public function getFile(){"\` |
|        - |  1149 | `	"  return $this->file;"\` |
|        - |  1150 | `	"}"\` |
|        - |  1151 | `	"public function getLine(){"\` |
|        - |  1152 | `	"  return $this->line;"\` |
|        - |  1153 | `	"}"\` |
|        - |  1154 | `	"public function getTrace(){"\` |
|        - |  1155 | `	"   return $this->trace;"\` |
|        - |  1156 | `	"}"\` |
|        - |  1157 | `	"public function getTraceAsString(){"\` |
|        - |  1158 | `	"  return debug_string_backtrace();"\` |
|        - |  1159 | `	"}"\` |
|        - |  1160 | `	"public function getPrevious(){"\` |
|        - |  1161 | `	"    return $this->previous;"\` |
|        - |  1162 | `	"}"\` |
|        - |  1163 | `	"public function __toString(){"\` |
|        - |  1164 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1165 | `    "}"\` |
|        - |  1166 | `	"}"\` |
|        - |  1167 | `	"class Error implements Throwable { "\` |
|        - |  1168 | `    "protected $message = '';"\` |
|        - |  1169 | `    "protected $code = 0;"\` |
|        - |  1170 | `    "protected $file;"\` |
|        - |  1171 | `    "protected $line;"\` |
|        - |  1172 | `    "protected $trace;"\` |
|        - |  1173 | `    "protected $previous;"\` |
|        - |  1174 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1175 | `	"   if( isset($message) ){"\` |
|        - |  1176 | `	"	  $this->message = $message;"\` |
|        - |  1177 | `	"   }"\` |
|        - |  1178 | `	"   $this->code = $code;"\` |
|        - |  1179 | `	"   $this->file = __FILE__;"\` |
|        - |  1180 | `	"   $this->line = __LINE__;"\` |
|        - |  1181 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1182 | `	"   if( isset($previous) ){"\` |
|        - |  1183 | `	"     $this->previous = $previous;"\` |
|        - |  1184 | `	"   }"\` |
|        - |  1185 | `	"}"\` |
|        - |  1186 | `	"public function getMessage(){"\` |
|        - |  1187 | `	"   return $this->message;"\` |
|        - |  1188 | `	"}"\` |
|        - |  1189 | `	"public function getCode(){"\` |
|        - |  1190 | `	"  return $this->code;"\` |
|        - |  1191 | `	"}"\` |
|        - |  1192 | `	"public function getFile(){"\` |
|        - |  1193 | `	"  return $this->file;"\` |
|        - |  1194 | `	"}"\` |
|        - |  1195 | `	"public function getLine(){"\` |
|        - |  1196 | `	"  return $this->line;"\` |
|        - |  1197 | `	"}"\` |
|        - |  1198 | `	"public function getTrace(){"\` |
|        - |  1199 | `	"   return $this->trace;"\` |
|        - |  1200 | `	"}"\` |
|        - |  1201 | `	"public function getTraceAsString(){"\` |
|        - |  1202 | `	"  return debug_string_backtrace();"\` |
|        - |  1203 | `	"}"\` |
|        - |  1204 | `	"public function getPrevious(){"\` |
|        - |  1205 | `	"    return $this->previous;"\` |
|        - |  1206 | `	"}"\` |
|        - |  1207 | `	"public function __toString(){"\` |
|        - |  1208 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1209 | `	"}"\` |
|        - |  1210 | `	"}"\` |
|        - |  1211 | `	"class TypeError extends Error { }"\` |
|        - |  1212 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1213 | `	"class ValueError extends Error { }"\` |
|        - |  1214 | `	"class FiberError extends Error { }"\` |
|        - |  1215 | `	"class AssertionError extends Error { }"\` |
|        - |  1216 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1217 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1218 | `	"class ErrorException extends Exception { "\` |
|        - |  1219 | `	"protected $severity;"\` |
|        - |  1220 | `	"public function __construct(string $message = null,"\` |
|        - |  1221 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1222 | `	"   if( isset($message) ){"\` |
|        - |  1223 | `	"	  $this->message = $message;"\` |
|        - |  1224 | `	"   }"\` |
|        - |  1225 | `	"   $this->severity = $severity;"\` |
|        - |  1226 | `	"   $this->code = $code;"\` |
|        - |  1227 | `	"   $this->file = $filename;"\` |
|        - |  1228 | `	"   $this->line = $lineno;"\` |
|        - |  1229 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1230 | `	"   if( isset($previous) ){"\` |
|        - |  1231 | `	"     $this->previous = $previous;"\` |
|        - |  1232 | `	"   }"\` |
|        - |  1233 | `	"}"\` |
|        - |  1234 | `	"public function getSeverity(){"\` |
|        - |  1235 | `	"   return $this->severity;"\` |
|        - |  1236 | `    "}"\` |
|        - |  1237 | `	"}"\` |
|        - |  1238 | `	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\` |
|        - |  1239 | `	"class LogicException extends Exception { }"\` |
|        - |  1240 | `	"class RuntimeException extends Exception { }"\` |
|        - |  1241 | `	"class BadFunctionCallException extends LogicException { }"\` |
|        - |  1242 | `	"class BadMethodCallException extends BadFunctionCallException { }"\` |
|        - |  1243 | `	"class DomainException extends LogicException { }"\` |
|        - |  1244 | `	"class InvalidArgumentException extends LogicException { }"\` |
|        - |  1245 | `	"class LengthException extends LogicException { }"\` |
|        - |  1246 | `	"class OutOfRangeException extends LogicException { }"\` |
|        - |  1247 | `	"class OutOfBoundsException extends RuntimeException { }"\` |
|        - |  1248 | `	"class OverflowException extends RuntimeException { }"\` |
|        - |  1249 | `	"class RangeException extends RuntimeException { }"\` |
|        - |  1250 | `	"class UnderflowException extends RuntimeException { }"\` |
|        - |  1251 | `	"class UnexpectedValueException extends RuntimeException { }"\` |
|        - |  1252 | `	"interface Iterator extends Traversable {"\` |
|        - |  1253 | `	"public function current();"\` |
|        - |  1254 | `	"public function key();"\` |
|        - |  1255 | `	"public function next();"\` |
|        - |  1256 | `	"public function rewind();"\` |
|        - |  1257 | `	"public function valid();"\` |
|        - |  1258 | `	"}"\` |
|        - |  1259 | `	"interface IteratorAggregate extends Traversable {"\` |
|        - |  1260 | `	"public function getIterator();"\` |
|        - |  1261 | `	"}"\` |
|        - |  1262 | `	"interface Serializable {"\` |
|        - |  1263 | `	"public function serialize();"\` |
|        - |  1264 | `	"public function unserialize(string $serialized);"\` |
|        - |  1265 | `	"}"\` |
|        - |  1266 | `	"/* Directory releated IO */"\` |
|        - |  1267 | `	"class Directory {"\` |
|        - |  1268 | `	"public $handle = null;"\` |
|        - |  1269 | `	"public $path  = null;"\` |
|        - |  1270 | `	"public function __construct(string $path)"\` |
|        - |  1271 | `	"{"\` |
|        - |  1272 | `	"   $this->handle = opendir($path);"\` |
|        - |  1273 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1274 | `	"      $this->path = $path;"\` |
|        - |  1275 | `	"   }"\` |
|        - |  1276 | `	"}"\` |
|        - |  1277 | `	"public function __destruct()"\` |
|        - |  1278 | `	"{"\` |
|        - |  1279 | `	"  if( $this->handle != null ){"\` |
|        - |  1280 | `	"       closedir($this->handle);"\` |
|        - |  1281 | `	"  }"\` |
|        - |  1282 | `	"}"\` |
|        - |  1283 | `	"public function read()"\` |
|        - |  1284 | `	"{"\` |
|        - |  1285 | `	"    return readdir($this->handle);"\` |
|        - |  1286 | `	"}"\` |
|        - |  1287 | `	"public function rewind()"\` |
|        - |  1288 | `	"{"\` |
|        - |  1289 | `	"    rewinddir($this->handle);"\` |
|        - |  1290 | `	"}"\` |
|        - |  1291 | `	"public function close()"\` |
|        - |  1292 | `	"{"\` |
|        - |  1293 | `	"    closedir($this->handle);"\` |
|        - |  1294 | `	"    $this->handle = null;"\` |
|        - |  1295 | `	"}"\` |
|        - |  1296 | `	"}"\` |
|        - |  1297 | `	"class Fiber {"\` |
|        - |  1298 | `	"  private $__ctx;"\` |
|        - |  1299 | `	"  private $__callable;"\` |
|        - |  1300 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1301 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1302 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1303 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1304 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1305 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1306 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1307 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1308 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1309 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1310 | `	"}"\` |
|        - |  1311 | `	"class Generator implements Iterator {"\` |
|        - |  1312 | `	"  private $__ctx;"\` |
|        - |  1313 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1314 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1315 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1316 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1317 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1318 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1319 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1320 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1321 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1322 | `	"}"\` |
|        - |  1323 | `	"class stdClass{"\` |
|        - |  1324 | `	"  public $value;"\` |
|        - |  1325 | `	" /* Magic methods */"\` |
|        - |  1326 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1327 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1328 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1329 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1330 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1331 | `	"}"\` |
|        - |  1332 | `	"function dir(string $path){"\` |
|        - |  1333 | `	"   return new Directory($path);"\` |
|        - |  1334 | `	"}"\` |
|        - |  1335 | `	"function Dir(string $path){"\` |
|        - |  1336 | `	"   return new Directory($path);"\` |
|        - |  1337 | `	"}"\` |
|        - |  1338 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1339 | `    "{"\` |
|        - |  1340 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1341 | `	"  $aDir = array();"\` |
|        - |  1342 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1343 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1344 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1345 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1346 | `	"   }"\` |
|        - |  1347 | `	"  closedir($pHandle);"\` |
|        - |  1348 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1349 | `	"      rsort($aDir);"\` |
|        - |  1350 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1351 | `	"      sort($aDir);"\` |
|        - |  1352 | `	"  }"\` |
|        - |  1353 | `	"  return $aDir;"\` |
|        - |  1354 | `	"}"\` |
|        - |  1355 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1356 | `	"/* Open the target directory */"\` |
|        - |  1357 | `	"$zDir = dirname($pattern);"\` |
|        - |  1358 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1359 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1360 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1361 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1362 | `	"	return FALSE;"\` |
|        - |  1363 | `	"}"\` |
|        - |  1364 | `	"$pattern = basename($pattern);"\` |
|        - |  1365 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1366 | `	"/* Loop throw available entries */"\` |
|        - |  1367 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1368 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1369 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1370 | `	"	if( $rc ){"\` |
|        - |  1371 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1372 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1373 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1374 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1375 | `	"		  }"\` |
|        - |  1376 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1377 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1378 | `	"		 continue;"\` |
|        - |  1379 | `	"	   }"\` |
|        - |  1380 | `	"	   /* Add the entry */"\` |
|        - |  1381 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1382 | `	"	}"\` |
|        - |  1383 | `	" }"\` |
|        - |  1384 | `	"/* Close the handle */"\` |
|        - |  1385 | `	"closedir($pHandle);"\` |
|        - |  1386 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1387 | `	"  /* Sort the array */"\` |
|        - |  1388 | `	"  sort($pArray);"\` |
|        - |  1389 | `	"}"\` |
|        - |  1390 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1391 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1392 | `	"  $pArray[] = $pattern;"\` |
|        - |  1393 | `	"}"\` |
|        - |  1394 | `	"/* Return the created array */"\` |
|        - |  1395 | `	"return $pArray;"\` |
|        - |  1396 | `   "}"\` |
|        - |  1397 | `   "/* Creates a temporary file */"\` |
|        - |  1398 | `   "function tmpfile(){"\` |
|        - |  1399 | `   "  /* Extract the temp directory */"\` |
|        - |  1400 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1401 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1402 | `   "    /* Use the current dir */"\` |
|        - |  1403 | `   "    $zTempDir = '.';"\` |
|        - |  1404 | `   "  }"\` |
|        - |  1405 | `   "  /* Create the file */"\` |
|        - |  1406 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1407 | `   "  return $pHandle;"\` |
|        - |  1408 | `   "}"\` |
|        - |  1409 | `   "/* Creates a temporary filename */"\` |
|        - |  1410 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1411 | `   "{"\` |
|        - |  1412 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1413 | `   "}"\` |
|        - |  1414 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1415 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1416 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1417 | `   "/* Copy arguments */"\` |
|        - |  1418 | `   "$nArgs = func_num_args();"\` |
|        - |  1419 | `   "$pNew = array();"\` |
|        - |  1420 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1421 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1422 | `    "}"\` |
|        - |  1423 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1424 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1425 | `	"/* Erase */"\` |
|        - |  1426 | `	"array_erase($pArray);"\` |
|        - |  1427 | `	"/* Unshift */"\` |
|        - |  1428 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1429 | `	"return sizeof($pArray);"\` |
|        - |  1430 | `    "}"\` |
|        - |  1431 | `	"function array_merge_recursive(){"\` |
|        - |  1432 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1433 | `    "$arrays = func_get_args();"\` |
|        - |  1434 | `    "$narrays = count($arrays);"\` |
|        - |  1435 | `    "$ret = array();"\` |
|        - |  1436 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1437 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1438 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1439 | `	 " }"\` |
|        - |  1440 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1441 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1442 | `     "  if( $keyIsInt ) {"\` |
|        - |  1443 | `     "   $ret[] = $value;"\` |
|        - |  1444 | `     "  } else {"\` |
|        - |  1445 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1446 | `     "    $cur = $ret[$key];"\` |
|        - |  1447 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1448 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1449 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1450 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1451 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1452 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1453 | `     "    } else {"\` |
|        - |  1454 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1455 | `     "    }"\` |
|        - |  1456 | `     "   } else {"\` |
|        - |  1457 | `     "    $ret[$key] = $value;"\` |
|        - |  1458 | `     "   }"\` |
|        - |  1459 | `     "  }"\` |
|        - |  1460 | `     " }"\` |
|        - |  1461 | `	 " }"\` |
|        - |  1462 | `	 " return $ret;"\` |
|        - |  1463 | `    "}"\` |
|        - |  1464 | `	"function max(){"\` |
|        - |  1465 | `    "  $pArgs = func_get_args();"\` |
|        - |  1466 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1467 | `	"  return null;"\` |
|        - |  1468 | `    " }"\` |
|        - |  1469 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1470 | `    " $pArg = $pArgs[0];"\` |
|        - |  1471 | `	" if( !is_array($pArg) ){"\` |
|        - |  1472 | `	"   return $pArg; "\` |
|        - |  1473 | `	" }"\` |
|        - |  1474 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1475 | `	"   return null;"\` |
|        - |  1476 | `	" }"\` |
|        - |  1477 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1478 | `	" reset($pArg);"\` |
|        - |  1479 | `	" $max = current($pArg);"\` |
|        - |  1480 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1481 | `	"   if( $val > $max ){"\` |
|        - |  1482 | `	"     $max = $val;"\` |
|        - |  1483 | `    " }"\` |
|        - |  1484 | `	" }"\` |
|        - |  1485 | `	" return $max;"\` |
|        - |  1486 | `    " }"\` |
|        - |  1487 | `    " $max = $pArgs[0];"\` |
|        - |  1488 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1489 | `    " $val = $pArgs[$i];"\` |
|        - |  1490 | `	"if( $val > $max ){"\` |
|        - |  1491 | `	" $max = $val;"\` |
|        - |  1492 | `	"}"\` |
|        - |  1493 | `    " }"\` |
|        - |  1494 | `	" return $max;"\` |
|        - |  1495 | `    "}"\` |
|        - |  1496 | `	"function min(){"\` |
|        - |  1497 | `    "  $pArgs = func_get_args();"\` |
|        - |  1498 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1499 | `	"  return null;"\` |
|        - |  1500 | `    " }"\` |
|        - |  1501 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1502 | `    " $pArg = $pArgs[0];"\` |
|        - |  1503 | `	" if( !is_array($pArg) ){"\` |
|        - |  1504 | `	"   return $pArg; "\` |
|        - |  1505 | `	" }"\` |
|        - |  1506 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1507 | `	"   return null;"\` |
|        - |  1508 | `	" }"\` |
|        - |  1509 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1510 | `	" reset($pArg);"\` |
|        - |  1511 | `	" $min = current($pArg);"\` |
|        - |  1512 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1513 | `	"   if( $val < $min ){"\` |
|        - |  1514 | `	"     $min = $val;"\` |
|        - |  1515 | `    " }"\` |
|        - |  1516 | `	" }"\` |
|        - |  1517 | `	" return $min;"\` |
|        - |  1518 | `    " }"\` |
|        - |  1519 | `    " $min = $pArgs[0];"\` |
|        - |  1520 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1521 | `    " $val = $pArgs[$i];"\` |
|        - |  1522 | `	"if( $val < $min ){"\` |
|        - |  1523 | `	" $min = $val;"\` |
|        - |  1524 | `	" }"\` |
|        - |  1525 | `    " }"\` |
|        - |  1526 | `	" return $min;"\` |
|        - |  1527 | `	"}"\` |
|        - |  1528 | `	"function fileowner(string $file){"\` |
|        - |  1529 | `    " $a = stat($file);"\` |
|        - |  1530 | `	" if( !is_array($a) ){"\` |
|        - |  1531 | `	"	return false;"\` |
|        - |  1532 | `	" }"\` |
|        - |  1533 | `	" return $a['uid'];"\` |
|        - |  1534 | `    "}"\` |
|        - |  1535 | `    "function filegroup(string $file){"\` |
|        - |  1536 | `	" $a = stat($file);"\` |
|        - |  1537 | `	" if( !is_array($a) ){"\` |
|        - |  1538 | `	"	return false;"\` |
|        - |  1539 | `	" }"\` |
|        - |  1540 | `	" return $a['gid'];"\` |
|        - |  1541 | `    "}"\` |
|        - |  1542 | `	 "function fileinode(string $file){"\` |
|        - |  1543 | `	" $a = stat($file);"\` |
|        - |  1544 | `	" if( !is_array($a) ){"\` |
|        - |  1545 | `	"	return false;"\` |
|        - |  1546 | `	" }"\` |
|        - |  1547 | `	" return $a['ino'];"\` |
|        - |  1548 | `    "}"` |
|        - |  1549 |  |
|        - |  1550 | `/*` |
|        - |  1551 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1552 | ` * start compiling the target PHP program.` |
|        - |  1553 | ` */` |
|     3148 |  1554 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1555 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1556 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1557 | `	 )` |
|        2 |  1558 |  |
|        - |  1559 | `	SyString sBuiltin;` |
|        - |  1560 | `	ph7_value *pObj;` |
|        - |  1561 | `	sxi32 rc;` |
|        - |  1562 | `	/* Zero the structure */` |
|     3150 |  1563 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1564 | `	/* Initialize VM fields */` |
|     3150 |  1565 | `	pVm->pEngine = &(*pEngine);` |
|     3150 |  1566 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1567 | `	/* Instructions containers */` |
|     3150 |  1568 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3150 |  1569 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3150 |  1570 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1571 | `	/* Object containers */` |
|     3150 |  1572 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3150 |  1573 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1574 | `	/* Virtual machine internal containers */` |
|     3150 |  1575 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3150 |  1576 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3150 |  1577 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3150 |  1578 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3150 |  1579 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3150 |  1580 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3150 |  1581 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3150 |  1582 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3150 |  1583 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3150 |  1584 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3150 |  1585 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3150 |  1586 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3150 |  1587 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3150 |  1588 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3150 |  1589 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3150 |  1590 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3150 |  1591 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3150 |  1592 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3150 |  1593 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3150 |  1594 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3150 |  1595 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3150 |  1596 | `	pVm->pPendingException = 0;` |
|        - |  1597 | `	/* Configuration containers */` |
|     3150 |  1598 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3150 |  1599 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3150 |  1600 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3150 |  1601 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3150 |  1602 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3150 |  1603 | `	pVm->iResponseStatus = 200;` |
|     3150 |  1604 | `	pVm->bHeadersSent = 0;` |
|     3150 |  1605 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1606 | `	/* Error callbacks containers */` |
|     3150 |  1607 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3150 |  1608 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3150 |  1609 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3150 |  1610 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3150 |  1611 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1612 | `	/* Set a default recursion limit */` |
|        - |  1613 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3150 |  1614 | `	pVm->nMaxDepth = 32;` |
|        - |  1615 | `#else` |
|        - |  1616 | `	pVm->nMaxDepth = 16;` |
|        - |  1617 | `#endif` |
|        - |  1618 | `	/* Default assertion flags */` |
|     3150 |  1619 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1620 | `	/* JSON return status */` |
|     3150 |  1621 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1622 | `	/* PRNG context */` |
|     3150 |  1623 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1624 | `	/* Install the null constant */` |
|     3150 |  1625 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3150 |  1626 | `	if( pObj == 0 ){` |
|      ! 0 |  1627 | `		rc = SXERR_MEM;` |
|      ! 0 |  1628 | `		goto Err;` |
|        - |  1629 | `	}` |
|     3150 |  1630 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1631 | `	/* Install the boolean TRUE constant */` |
|     3150 |  1632 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3150 |  1633 | `	if( pObj == 0 ){` |
|      ! 0 |  1634 | `		rc = SXERR_MEM;` |
|      ! 0 |  1635 | `		goto Err;` |
|        - |  1636 | `	}` |
|     3150 |  1637 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1638 | `	/* Install the boolean FALSE constant */` |
|     3150 |  1639 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3150 |  1640 | `	if( pObj == 0 ){` |
|      ! 0 |  1641 | `		rc = SXERR_MEM;` |
|      ! 0 |  1642 | `		goto Err;` |
|        - |  1643 | `	}` |
|     3150 |  1644 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1645 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1646 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1647 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3150 |  1648 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3150 |  1649 | `	if( pObj == 0 ){` |
|      ! 0 |  1650 | `		rc = SXERR_MEM;` |
|      ! 0 |  1651 | `		goto Err;` |
|        - |  1652 | `	}` |
|     3150 |  1653 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1654 | `	/* Create the global frame */` |
|     3150 |  1655 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3150 |  1656 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1657 | `		goto Err;` |
|        - |  1658 | `	}` |
|        - |  1659 | `	/* Initialize the code generator */` |
|     3150 |  1660 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3150 |  1661 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1662 | `		goto Err;` |
|        - |  1663 | `	}` |
|        - |  1664 | `	/* VM correctly initialized,set the magic number */` |
|     3150 |  1665 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3150 |  1666 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1667 | `	/* Compile the built-in library */` |
|     3150 |  1668 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1669 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3150 |  1670 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1671 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3150 |  1672 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3150 |  1673 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3150 |  1674 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3150 |  1675 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|        - |  1676 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3150 |  1677 | `	pVm->pCoalesceObj = 0;` |
|     3150 |  1678 | `	pVm->bCoalesceArmed = 0;` |
|     3150 |  1679 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  1680 | `	/* Register Fiber internal C functions */` |
|     3150 |  1681 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3150 |  1682 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3150 |  1683 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3150 |  1684 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3150 |  1685 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3150 |  1686 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3150 |  1687 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3150 |  1688 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3150 |  1689 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3150 |  1690 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1691 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3150 |  1692 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3150 |  1693 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3150 |  1694 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3150 |  1695 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3150 |  1696 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3150 |  1697 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3150 |  1698 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3150 |  1699 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3150 |  1700 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3150 |  1701 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1702 | `	/* Reset the code generator */` |
|     3150 |  1703 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3150 |  1704 | `	return SXRET_OK;` |
|      ! 0 |  1705 | `Err:` |
|      ! 0 |  1706 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1707 | `	return rc;` |
|     1576 |  1708 |  |
|        - |  1709 | `/*` |
|        - |  1710 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1711 | ` * routine which store the output in an internal blob.` |
|        - |  1712 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1713 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1714 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1715 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1716 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1717 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1718 | ` * to finish executing and extracting the output.` |
|        - |  1719 | ` */` |
|       56 |  1720 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1721 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1722 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1723 | `	void *pUserData     /* User private data */` |
|        - |  1724 | `	)` |
|      ! 0 |  1725 |  |
|        - |  1726 | `	 sxi32 rc;` |
|        - |  1727 | `	 /* Store the output in an internal BLOB */` |
|       56 |  1728 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       56 |  1729 | `	 return rc;` |
|      ! 0 |  1730 |  |
|        - |  1731 | `/*` |
|        - |  1732 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1733 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1734 | ` */` |
|    20630 |  1735 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1736 |  |
|    20632 |  1737 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    20632 |  1738 | `	if( xCons != VmObConsumer ){` |
|     8236 |  1739 | `		pVm->nOutputLen += nLen;` |
|     8236 |  1740 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1028 |  1741 | `			pVm->bHeadersSent = 1;` |
|      513 |  1742 | `		}` |
|     4117 |  1743 | `	}` |
|    20632 |  1744 |  |
|        - |  1745 | `#define VM_STACK_GUARD 16` |
|        - |  1746 | `/*` |
|        - |  1747 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1748 | ` * our compiled PHP program.` |
|        - |  1749 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1750 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1751 | ` */` |
|    45216 |  1752 | `static ph7_value * VmNewOperandStack(` |
|        - |  1753 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1754 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1755 | `	)` |
|        2 |  1756 |  |
|        - |  1757 | `	ph7_value *pStack;` |
|        - |  1758 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1759 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1760 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1761 | `  ** on the maximum stack depth required.` |
|        - |  1762 | `  **` |
|        - |  1763 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1764 | `  */` |
|    45218 |  1765 | `	nInstr += VM_STACK_GUARD;` |
|    45218 |  1766 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    45218 |  1767 | `	if( pStack == 0 ){` |
|      ! 0 |  1768 | `		return 0;` |
|        - |  1769 | `	}` |
|        - |  1770 | `	/* Initialize the operand stack */` |
|  3042340 |  1771 | `	while( nInstr > 0 ){` |
|  2997124 |  1772 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2997124 |  1773 | `		--nInstr;` |
|        2 |  1774 | `	}` |
|        - |  1775 | `	/* Ready for bytecode execution */` |
|    45218 |  1776 | `	return pStack;` |
|    22610 |  1777 |  |
|        - |  1778 | `/* Forward declaration */` |
|        - |  1779 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1780 | `/*` |
|        - |  1781 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1782 | ` * This routine gets called by the PH7 engine after` |
|        - |  1783 | ` * successful compilation of the target PHP program.` |
|        - |  1784 | ` */` |
|     2834 |  1785 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1786 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1787 | `	)` |
|        2 |  1788 |  |
|        - |  1789 | `	SyHashEntry *pEntry;` |
|        - |  1790 | `	sxi32 rc;` |
|     2836 |  1791 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1792 | `		/* Initialize your VM first */` |
|      ! 0 |  1793 | `		return SXERR_CORRUPT;` |
|        - |  1794 | `	}` |
|        - |  1795 | `	/* Mark the VM ready for byte-code execution */` |
|     2836 |  1796 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1797 | `	/* Release the code generator now we have compiled our program */` |
|     2836 |  1798 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1799 | `	/* Emit the DONE instruction */` |
|     2836 |  1800 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2836 |  1801 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1802 | `		return SXERR_MEM;` |
|        - |  1803 | `	}` |
|        - |  1804 | `	/* Script return value */` |
|     2836 |  1805 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1806 | `	/* Allocate a new operand stack */` |
|     2836 |  1807 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2836 |  1808 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1809 | `		return SXERR_MEM;` |
|        - |  1810 | `	}` |
|        - |  1811 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1812 | `	 * private data. */` |
|     2836 |  1813 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2836 |  1814 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1815 | `	/* Allocate the reference table */` |
|     2836 |  1816 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2836 |  1817 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2836 |  1818 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1819 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1820 | `		return SXERR_MEM;` |
|        - |  1821 | `	}` |
|        - |  1822 | `	/* Zero the reference table */` |
|     2836 |  1823 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1824 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2836 |  1825 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2836 |  1826 | `	if( rc != SXRET_OK ){` |
|        - |  1827 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1828 | `		return rc;` |
|        - |  1829 | `	}` |
|        - |  1830 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|        - |  1831 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|        - |  1832 | `	 * every object/variable created during execution) is per-exec state that` |
|        - |  1833 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|        - |  1834 | `	 * below it is compile-time/init state that survives a reset. */` |
|     2836 |  1835 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|        - |  1836 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2836 |  1837 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2836 |  1838 | `	if( rc != SXRET_OK ){` |
|        - |  1839 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1840 | `		return rc;` |
|        - |  1841 | `	}` |
|        - |  1842 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2836 |  1843 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1844 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2836 |  1845 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1846 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2836 |  1847 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1848 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1849 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2836 |  1850 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2836 |  1851 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1852 | `#endif` |
|        - |  1853 | `	/* Initialize and install static and constants class attributes.` |
|        - |  1854 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|        - |  1855 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|        - |  1856 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|        - |  1857 | `	 * that function in sync when changing what is reserved here. */` |
|     2836 |  1858 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   110874 |  1859 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   108040 |  1860 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   108040 |  1861 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1862 | `			return rc;` |
|        - |  1863 | `		}` |
|        2 |  1864 | `	}` |
|        - |  1865 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2836 |  1866 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1867 | `	/* VM is ready for bytecode execution */` |
|     2836 |  1868 | `	return SXRET_OK;` |
|     1419 |  1869 |  |
|        - |  1870 | `/*` |
|        - |  1871 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|        - |  1872 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|        - |  1873 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|        - |  1874 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|        - |  1875 | ` * a dangling node pointer in some other object's reference record.` |
|        - |  1876 | ` */` |
|        6 |  1877 | `static void VmResetRefTable(ph7_vm *pVm)` |
|      ! 0 |  1878 |  |
|        - |  1879 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|        - |  1880 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|        - |  1881 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|      204 |  1882 | `	while( pVm->pRefList ){` |
|      198 |  1883 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|      ! 0 |  1884 | `	}` |
|        6 |  1885 |  |
|        - |  1886 | `/*` |
|        - |  1887 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|        - |  1888 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|        - |  1889 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|        - |  1890 | ` */` |
|       42 |  1891 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|      ! 0 |  1892 |  |
|       42 |  1893 | `	PH7_MemObjRelease(pObj);` |
|       42 |  1894 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|       42 |  1895 |  |
|        - |  1896 | `/*` |
|        - |  1897 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|        - |  1898 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|        - |  1899 | ` * of statics).` |
|        - |  1900 | ` */` |
|      380 |  1901 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|      ! 0 |  1902 |  |
|      380 |  1903 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|        - |  1904 | `	sxu32 k;` |
|      384 |  1905 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|        4 |  1906 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|        2 |  1907 | `	}` |
|      380 |  1908 |  |
|        - |  1909 | `/*` |
|        - |  1910 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|        - |  1911 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|        - |  1912 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|        - |  1913 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|        - |  1914 | ` *    captured environment values, its name buffer and its structure (the` |
|        - |  1915 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|        - |  1916 | ` *    freed). Its template-shared static sentinels are reset too.` |
|        - |  1917 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|        - |  1918 | ` *    has its static sentinels reset.` |
|        - |  1919 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|        - |  1920 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|        - |  1921 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|        - |  1922 | ` */` |
|        6 |  1923 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|      ! 0 |  1924 |  |
|        - |  1925 | `	SyHashEntry *pEntry;` |
|        6 |  1926 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|      386 |  1927 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|      380 |  1928 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      380 |  1929 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - |  1930 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|        - |  1931 | `			 * release its captured-by-value environment, then free the entry,` |
|        - |  1932 | `			 * name buffer and structure. */` |
|        4 |  1933 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        4 |  1934 | `			const char *zName = SyStringData(&pFunc->sName);` |
|        - |  1935 | `			sxu32 k;` |
|        4 |  1936 | `			VmResetFuncStatics(pFunc);` |
|        8 |  1937 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|        4 |  1938 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|        2 |  1939 | `			}` |
|        4 |  1940 | `			SySetRelease(&pFunc->aClosureEnv);` |
|        - |  1941 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|        4 |  1942 | `			SyHashDeleteEntry2(pEntry);` |
|        4 |  1943 | `			if( zName ){` |
|        4 |  1944 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|        2 |  1945 | `			}` |
|        4 |  1946 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|        4 |  1947 | `			continue;` |
|        - |  1948 | `		}` |
|        - |  1949 | `		/* Named function: reset statics for every overload sharing this name. */` |
|      752 |  1950 | `		while( pFunc ){` |
|      376 |  1951 | `			VmResetFuncStatics(pFunc);` |
|      376 |  1952 | `			pFunc = pFunc->pNextName;` |
|      ! 0 |  1953 | `		}` |
|      ! 0 |  1954 | `	}` |
|        6 |  1955 | `	pVm->closure_cnt = 0;` |
|        6 |  1956 |  |
|        - |  1957 | `/*` |
|        - |  1958 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|        - |  1959 | ` * are already gone (each object's destructor removed its own during the object` |
|        - |  1960 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|        - |  1961 | ` * the class re-mount registers fresh ones.` |
|        - |  1962 | ` */` |
|        6 |  1963 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|      ! 0 |  1964 |  |
|        - |  1965 | `	SyHashEntry *pEntry;` |
|        - |  1966 | `	/* Common case: no class static typed properties — table already empty. */` |
|        6 |  1967 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        2 |  1968 | `		return;` |
|        - |  1969 | `	}` |
|        - |  1970 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|        - |  1971 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|        4 |  1972 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|       10 |  1973 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|        4 |  1974 | `		if( pEntry->pUserData ){` |
|        4 |  1975 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|        2 |  1976 | `		}` |
|      ! 0 |  1977 | `	}` |
|        4 |  1978 | `	SyHashRelease(&pVm->hTypedSlot);` |
|        4 |  1979 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|        3 |  1980 |  |
|        - |  1981 | `/*` |
|        - |  1982 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|        - |  1983 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|        - |  1984 | ` *` |
|        - |  1985 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|        - |  1986 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|        - |  1987 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|        - |  1988 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|        - |  1989 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|        - |  1990 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|        - |  1991 | ` *` |
|        - |  1992 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|        - |  1993 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|        - |  1994 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|        - |  1995 | ` * exception/error-handler state, the reference table and every object/array` |
|        - |  1996 | ` * reserved during the run.` |
|        - |  1997 | ` *` |
|        - |  1998 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|        - |  1999 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|        - |  2000 | ` * global-scope destructors never fired.` |
|        - |  2001 | ` */` |
|        6 |  2002 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  2003 |  |
|        - |  2004 | `	sxu32 nWater,n;` |
|        6 |  2005 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  2006 | `		return SXERR_CORRUPT;` |
|        - |  2007 | `	}` |
|        6 |  2008 | `	nWater = pVm->nSuperBaseline;` |
|        - |  2009 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|        - |  2010 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        6 |  2011 | `	pVm->pGlobal = 0;` |
|        - |  2012 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|        - |  2013 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|        - |  2014 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|        - |  2015 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        6 |  2016 | `	pVm->bInReset = 1;` |
|        - |  2017 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        6 |  2018 | `	VmResetRefTable(&(*pVm));` |
|        - |  2019 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|        - |  2020 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|        - |  2021 | `	 * function/class registrations and intentionally persist across reuse (a` |
|        - |  2022 | `	 * re-run define() overwrites the value in place). */` |
|        6 |  2023 | `	VmResetFunctionState(&(*pVm));` |
|        - |  2024 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|        - |  2025 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|      218 |  2026 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|      212 |  2027 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      212 |  2028 | `		if( pObj ){` |
|      212 |  2029 | `			PH7_MemObjRelease(pObj);` |
|      106 |  2030 | `		}` |
|      106 |  2031 | `	}` |
|        - |  2032 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|        - |  2033 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        6 |  2034 | `	VmResetTypedSlots(&(*pVm));` |
|        - |  2035 | `	/* (5) Unwind any active frames back to none. */` |
|       12 |  2036 | `	while( pVm->pFrame ){` |
|        6 |  2037 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 |  2038 | `	}` |
|        - |  2039 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        6 |  2040 | `	pVm->bInReset = 0;` |
|        - |  2041 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|        - |  2042 | `	 * slots (their indices no longer exist). */` |
|        6 |  2043 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        6 |  2044 | `	SySetReset(&pVm->aFreeObj);` |
|        - |  2045 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        6 |  2046 | `	SyHashRelease(&pVm->hSuper);` |
|        6 |  2047 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        - |  2048 | `	/* (8) Drain remaining per-exec containers. */` |
|        6 |  2049 | `	SySetReset(&pVm->aSelf);` |
|        - |  2050 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|        - |  2051 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|        - |  2052 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        6 |  2053 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|      ! 0 |  2054 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|      ! 0 |  2055 | `		if( pCB ){` |
|        - |  2056 | `			int iArg;` |
|      ! 0 |  2057 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2058 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|      ! 0 |  2059 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|      ! 0 |  2060 | `			}` |
|      ! 0 |  2061 | `		}` |
|      ! 0 |  2062 | `	}` |
|        6 |  2063 | `	SySetReset(&pVm->aShutdown);` |
|        6 |  2064 | `	SySetReset(&pVm->aException);` |
|        6 |  2065 | `	pVm->pPendingException = 0;` |
|        6 |  2066 | `	pVm->nExceptDepth = 0;` |
|        - |  2067 | `	/* spl_autoload_register() callbacks are per request */` |
|        6 |  2068 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|      ! 0 |  2069 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      ! 0 |  2070 | `		if( pCB ){` |
|      ! 0 |  2071 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2072 | `		}` |
|      ! 0 |  2073 | `	}` |
|        6 |  2074 | `	SySetReset(&pVm->aAutoload);` |
|        - |  2075 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|        - |  2076 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        6 |  2077 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|      ! 0 |  2078 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|      ! 0 |  2079 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      ! 0 |  2080 | `	}` |
|        - |  2081 | `	/* Output buffers */` |
|        6 |  2082 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|      ! 0 |  2083 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|      ! 0 |  2084 | `		if( pOb ){` |
|      ! 0 |  2085 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|      ! 0 |  2086 | `			SyBlobRelease(&pOb->sOB);` |
|      ! 0 |  2087 | `		}` |
|      ! 0 |  2088 | `	}` |
|        6 |  2089 | `	SySetReset(&pVm->aOB);` |
|        6 |  2090 | `	pVm->nObDepth = 0;` |
|        - |  2091 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|        - |  2092 | `	{` |
|        6 |  2093 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        6 |  2094 | `		if( rc == SXRET_OK ){` |
|        6 |  2095 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|        3 |  2096 | `		}` |
|        6 |  2097 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  2098 | `			return rc;` |
|        - |  2099 | `		}` |
|        - |  2100 | `	}` |
|        - |  2101 | `	/* (10) Re-mount the static/const attribute slots of every class. */` |
|        - |  2102 | `	{` |
|        - |  2103 | `		SyHashEntry *pEntry;` |
|        6 |  2104 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      238 |  2105 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      232 |  2106 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      232 |  2107 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2108 | `				return rc;` |
|        - |  2109 | `			}` |
|      ! 0 |  2110 | `		}` |
|        - |  2111 | `	}` |
|        - |  2112 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        6 |  2113 | `	SyBlobReset(&pVm->sConsumer);` |
|        6 |  2114 | `	pVm->nOutputLen = 0;` |
|        6 |  2115 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        6 |  2116 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        6 |  2117 | `	pVm->iResponseStatus = 200;` |
|        6 |  2118 | `	pVm->bHeadersSent = 0;` |
|        6 |  2119 | `	pVm->bHttpContext = 0;` |
|        6 |  2120 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|        6 |  2121 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|        6 |  2122 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|        6 |  2123 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|        6 |  2124 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|        6 |  2125 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  2126 | `#ifdef PH7_ENABLE_PCRE` |
|        6 |  2127 | `	pVm->iPcreLastError = 0;` |
|        - |  2128 | `#endif` |
|        6 |  2129 | `	pVm->iCmpCallbackExc = 0;` |
|        6 |  2130 | `	pVm->bHaltRequested = 0;` |
|        6 |  2131 | `	pVm->iExitStatus = 0;` |
|        6 |  2132 | `	pVm->iSpreadExtra = 0;` |
|        6 |  2133 | `	pVm->nRecursionDepth = 0;` |
|        6 |  2134 | `	pVm->pActiveCtx = 0;` |
|        6 |  2135 | `	pVm->pCoalesceObj = 0;` |
|        6 |  2136 | `	pVm->bCoalesceArmed = 0;` |
|        6 |  2137 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|        - |  2138 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        6 |  2139 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  2140 | `	/* Set the ready flag */` |
|        6 |  2141 | `	pVm->nMagic = PH7_VM_RUN;` |
|        6 |  2142 | `	return SXRET_OK;` |
|        3 |  2143 |  |
|        - |  2144 | `/*` |
|        - |  2145 | ` * Release a Virtual Machine.` |
|        - |  2146 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  2147 | ` */` |
|     2834 |  2148 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  2149 |  |
|        - |  2150 | `	/* Set the stale magic number */` |
|     2836 |  2151 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  2152 | `	/* Release the private memory subsystem */` |
|     2836 |  2153 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2836 |  2154 | `	return SXRET_OK;` |
|        2 |  2155 |  |
|        - |  2156 | `/*` |
|        - |  2157 | ` * Initialize a foreign function call context.` |
|        - |  2158 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  2159 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  2160 | ` * functions.` |
|        - |  2161 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  2162 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  2163 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  2164 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  2165 | ` */` |
|   698232 |  2166 | `static sxi32 VmInitCallContext(` |
|        - |  2167 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  2168 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  2169 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  2170 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  2171 | `	sxi32 iFlags          /* Control flags */` |
|        - |  2172 | `	)` |
|        2 |  2173 |  |
|   698234 |  2174 | `	pOut->pFunc = pFunc;` |
|   698234 |  2175 | `	pOut->pVm   = pVm;` |
|   698234 |  2176 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   698234 |  2177 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  2178 | `	/* Assume a null return value */` |
|   698234 |  2179 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   698234 |  2180 | `	pOut->pRet = pRet;` |
|   698234 |  2181 | `	pOut->iFlags = iFlags;` |
|   698234 |  2182 | `	return SXRET_OK;` |
|        2 |  2183 |  |
|        - |  2184 | `/*` |
|        - |  2185 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  2186 | ` * left behind.` |
|        - |  2187 | ` */` |
|   698232 |  2188 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  2189 |  |
|        - |  2190 | `	sxu32 n;` |
|   698234 |  2191 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8644 |  2192 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    25252 |  2193 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    16610 |  2194 | `			if( apObj[n] == 0 ){` |
|        - |  2195 | `				/* Already released */` |
|      384 |  2196 | `				continue;` |
|        - |  2197 | `			}` |
|    16228 |  2198 | `			PH7_MemObjRelease(apObj[n]);` |
|    16228 |  2199 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     8115 |  2200 | `		}` |
|     8644 |  2201 | `		SySetRelease(&pCtx->sVar);` |
|     4321 |  2202 | `	}` |
|   698234 |  2203 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  2204 | `		ph7_aux_data *aAux;` |
|        - |  2205 | `		void *pChunk;` |
|        - |  2206 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  2207 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  2208 | `		 */` |
|        9 |  2209 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  2210 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  2211 | `			pChunk = aAux[n].pAuxData;` |
|        - |  2212 | `			/* Release the chunk */` |
|       25 |  2213 | `			if( pChunk ){` |
|       25 |  2214 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  2215 | `			}` |
|       13 |  2216 | `		}` |
|        9 |  2217 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  2218 | `	}` |
|   698234 |  2219 |  |
|        - |  2220 | `/*` |
|        - |  2221 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  2222 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  2223 | ` */` |
|      382 |  2224 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  2225 | `	ph7_context *pCtx, /* Call context */` |
|        - |  2226 | `	ph7_value *pValue  /* Release this value */` |
|        - |  2227 | `	)` |
|        2 |  2228 |  |
|      384 |  2229 | `	if( pValue == 0 ){` |
|        - |  2230 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  2231 | `		return;` |
|        - |  2232 | `	}` |
|      384 |  2233 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      384 |  2234 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  2235 | `		sxu32 n;` |
|     1282 |  2236 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1282 |  2237 | `			if( apObj[n] == pValue ){` |
|      384 |  2238 | `				PH7_MemObjRelease(pValue);` |
|      384 |  2239 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  2240 | `				/* Mark as released */` |
|      384 |  2241 | `				apObj[n] = 0;` |
|      384 |  2242 | `				break;` |
|        - |  2243 | `			}` |
|      451 |  2244 | `		}` |
|      191 |  2245 | `	}` |
|      193 |  2246 |  |
|        - |  2247 | `/*` |
|        - |  2248 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  2249 | ` */` |
|  3955850 |  2250 | `static void VmPopOperand(` |
|        - |  2251 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  2252 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  2253 | `	)` |
|        2 |  2254 |  |
|  3955852 |  2255 | `	ph7_value *pTos = *ppTos;` |
|  8428432 |  2256 | `	while( nPop > 0 ){` |
|  4472582 |  2257 | `		PH7_MemObjRelease(pTos);` |
|  4472582 |  2258 | `		pTos--;` |
|  4472582 |  2259 | `		nPop--;` |
|        2 |  2260 | `	}` |
|        - |  2261 | `	/* Top of the stack */` |
|  3955852 |  2262 | `	*ppTos = pTos;` |
|  3955852 |  2263 |  |
|        - |  2264 | `/*` |
|        - |  2265 | ` * Reserve a memory object.` |
|        - |  2266 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  2267 | ` */` |
|  3207330 |  2268 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  2269 |  |
|  3207332 |  2270 | `	ph7_value *pObj = 0;` |
|        - |  2271 | `	VmSlot *pSlot;` |
|        - |  2272 | `	sxu32 nIdx;` |
|        - |  2273 | `	/* Check for a free slot */` |
|  3207332 |  2274 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3207332 |  2275 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3207332 |  2276 | `	if( pSlot ){` |
|  1055264 |  2277 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1055264 |  2278 | `		nIdx = pSlot->nIdx;` |
|   527631 |  2279 | `	}` |
|  3207332 |  2280 | `	if( pObj == 0 ){` |
|        - |  2281 | `		/* Reserve a new memory object */` |
|  2152070 |  2282 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2152070 |  2283 | `		if( pObj == 0 ){` |
|      ! 0 |  2284 | `			return 0;` |
|        - |  2285 | `		}` |
|  1076034 |  2286 | `	}` |
|        - |  2287 | `	/* Set a null default value */` |
|  3207332 |  2288 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3207332 |  2289 | `	pObj->nIdx = nIdx;` |
|  3207332 |  2290 | `	return pObj;` |
|  1603667 |  2291 |  |
|        - |  2292 | `/*` |
|        - |  2293 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  2294 | ` */` |
|    35480 |  2295 | `static sxi32 VmHashmapRefInsert(` |
|        - |  2296 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  2297 | `	const char *zKey,  /* Entry key */` |
|        - |  2298 | `	sxu32 nByte,       /* Key length */` |
|        - |  2299 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  2300 | `	)` |
|        2 |  2301 |  |
|        - |  2302 | `	ph7_value sKey;` |
|        - |  2303 | `	sxi32 rc;` |
|    35482 |  2304 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    35482 |  2305 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  2306 | `	/* Perform the insertion */` |
|    35482 |  2307 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    35482 |  2308 | `	PH7_MemObjRelease(&sKey);` |
|    35482 |  2309 | `	return rc;` |
|        2 |  2310 |  |
|        - |  2311 | `/*` |
|        - |  2312 | ` * Extract a variable value from the top active VM frame.` |
|        - |  2313 | ` * Return a pointer to the variable value on success.` |
|        - |  2314 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  2315 | ` */` |
|  3675776 |  2316 | `static ph7_value * VmExtractMemObj(` |
|        - |  2317 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  2318 | `	const SyString *pName, /* Variable name */` |
|        - |  2319 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  2320 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  2321 | `	)` |
|        2 |  2322 |  |
|  3675778 |  2323 | `	int bNullify = FALSE;` |
|        - |  2324 | `	SyHashEntry *pEntry;` |
|        - |  2325 | `	VmFrame *pFrame;` |
|        - |  2326 | `	ph7_value *pObj;` |
|        - |  2327 | `	sxu32 nIdx;` |
|        - |  2328 | `	sxi32 rc;` |
|        - |  2329 | `	/* Point to the top active frame */` |
|  3675778 |  2330 | `	pFrame = pVm->pFrame;` |
|  3675778 |  2331 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  2332 | `	/* Perform the lookup */` |
|  3675778 |  2333 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2334 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2335 | `		pName = &sAnnon;` |
|        - |  2336 | `		/* Always nullify the object */` |
|      ! 0 |  2337 | `		bNullify = TRUE;` |
|      ! 0 |  2338 | `		bDup = FALSE;` |
|      ! 0 |  2339 | `	}` |
|        - |  2340 | `	/* Check the superglobals table first */` |
|  3675778 |  2341 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3675778 |  2342 | `	if( pEntry == 0 ){` |
|        - |  2343 | `		/* Query the top active frame */` |
|  3675732 |  2344 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3675732 |  2345 | `		if( pEntry == 0 ){` |
|   113592 |  2346 | `			char *zName = (char *)pName->zString;` |
|        - |  2347 | `			VmSlot sLocal;` |
|   113592 |  2348 | `			if( !bCreate ){` |
|        - |  2349 | `				/* Do not create the variable,return NULL instead */` |
|      986 |  2350 | `				return 0;` |
|        - |  2351 | `			}` |
|        - |  2352 | `			/* No such variable,automatically create a new one and install` |
|        - |  2353 | `			 * it in the current frame.` |
|        - |  2354 | `			 */` |
|   112608 |  2355 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   112608 |  2356 | `			if( pObj == 0 ){` |
|      ! 0 |  2357 | `				return 0;` |
|        - |  2358 | `			}` |
|   112608 |  2359 | `			nIdx = pObj->nIdx;` |
|   112608 |  2360 | `			if( bDup ){` |
|        - |  2361 | `				/* Duplicate name */` |
|      230 |  2362 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      230 |  2363 | `				if( zName == 0 ){` |
|      ! 0 |  2364 | `					return 0;` |
|        - |  2365 | `				}` |
|      114 |  2366 | `			}` |
|        - |  2367 | `			/* Link to the top active VM frame */` |
|   112608 |  2368 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   112608 |  2369 | `			if( rc != SXRET_OK ){` |
|        - |  2370 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2371 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2372 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2373 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2374 | `				return 0;` |
|        - |  2375 | `			}` |
|   112608 |  2376 | `			if( pFrame->pParent != 0 ){` |
|        - |  2377 | `				/* Local variable */` |
|   105560 |  2378 | `				sLocal.nIdx = nIdx;` |
|   105560 |  2379 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    52781 |  2380 | `			}else{` |
|        - |  2381 | `				/* Register in the $GLOBALS array */` |
|     7050 |  2382 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2383 | `			}` |
|        - |  2384 | `			/* Install in the reference table */` |
|   112608 |  2385 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2386 | `			/* Save object index */` |
|   112608 |  2387 | `			pObj->nIdx = nIdx;` |
|    56305 |  2388 | `		}else{` |
|        - |  2389 | `			/* Extract variable contents */` |
|  3562142 |  2390 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3562142 |  2391 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3562142 |  2392 | `			if( bNullify && pObj ){` |
|      ! 0 |  2393 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2394 | `			}` |
|        - |  2395 | `		}` |
|  1837485 |  2396 | `	}else{` |
|        - |  2397 | `		/* Superglobal */` |
|       48 |  2398 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       48 |  2399 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2400 | `	}` |
|  3674794 |  2401 | `	return pObj;` |
|  1838000 |  2402 |  |
|        - |  2403 | `/*` |
|        - |  2404 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2405 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2406 | ` */` |
|     3264 |  2407 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2408 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2409 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2410 | `	sxu32 nByte        /* zName length */` |
|        - |  2411 | `	)` |
|        2 |  2412 |  |
|        - |  2413 | `	SyHashEntry *pEntry;` |
|        - |  2414 | `	ph7_value *pValue;` |
|        - |  2415 | `	sxu32 nIdx;` |
|        - |  2416 | `	/* Query the superglobal table */` |
|     3266 |  2417 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     3266 |  2418 | `	if( pEntry == 0 ){` |
|        - |  2419 | `		/* No such entry */` |
|      ! 0 |  2420 | `		return 0;` |
|        - |  2421 | `	}` |
|        - |  2422 | `	/* Extract the superglobal index in the global object pool */` |
|     3266 |  2423 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2424 | `	/* Extract the variable value  */` |
|     3266 |  2425 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3266 |  2426 | `	return pValue;` |
|     1634 |  2427 |  |
|        - |  2428 | `/*` |
|        - |  2429 | ` * Perform a raw hashmap insertion.` |
|        - |  2430 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2431 | ` */` |
|     3306 |  2432 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2433 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2434 | `	const char *zKey,   /* Entry key */` |
|        - |  2435 | `	int nKeylen,        /* zKey length*/` |
|        - |  2436 | `	const char *zData,  /* Entry data */` |
|        - |  2437 | `	int nLen            /* zData length */` |
|        - |  2438 | `	)` |
|        2 |  2439 |  |
|        - |  2440 | `	ph7_value sKey,sValue;` |
|        - |  2441 | `	sxi32 rc;` |
|     3308 |  2442 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3308 |  2443 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3308 |  2444 | `	if( zKey ){` |
|     3286 |  2445 | `		if( nKeylen < 0 ){` |
|     3204 |  2446 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1601 |  2447 | `		}` |
|     3286 |  2448 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1642 |  2449 | `	}` |
|     3308 |  2450 | `	if( zData ){` |
|     3308 |  2451 | `		if( nLen < 0 ){` |
|        - |  2452 | `			/* Compute length automatically */` |
|      198 |  2453 | `			nLen = (int)SyStrlen(zData);` |
|       99 |  2454 | `		}` |
|     3308 |  2455 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1653 |  2456 | `	}` |
|        - |  2457 | `	/* Perform the insertion */` |
|     3308 |  2458 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3308 |  2459 | `	PH7_MemObjRelease(&sKey);` |
|     3308 |  2460 | `	PH7_MemObjRelease(&sValue);` |
|     3308 |  2461 | `	return rc;` |
|        2 |  2462 |  |
|        - |  2463 | `/*` |
|        - |  2464 | ` * Configure a working virtual machine instance.` |
|        - |  2465 | ` *` |
|        - |  2466 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2467 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2468 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2469 | ` * The second argument to this function is an integer configuration option` |
|        - |  2470 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2471 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2472 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2473 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2474 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2475 | ` */` |
|    45872 |  2476 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2477 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2478 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2479 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2480 | `	)` |
|        2 |  2481 |  |
|    45874 |  2482 | `	sxi32 rc = SXRET_OK;` |
|    45874 |  2483 | `	switch(nOp){` |
|     1409 |  2484 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2820 |  2485 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2820 |  2486 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2487 | `		/* VM output consumer callback */` |
|        - |  2488 | `#ifdef UNTRUST` |
|        - |  2489 | `		if( xConsumer == 0 ){` |
|        - |  2490 | `			rc = SXERR_CORRUPT;` |
|        - |  2491 | `			break;` |
|        - |  2492 | `		}` |
|        - |  2493 | `#endif` |
|        - |  2494 | `		/* Install the output consumer */` |
|     2820 |  2495 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2820 |  2496 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2820 |  2497 | `		break;` |
|        - |  2498 | `							   }` |
|     1417 |  2499 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2500 | `		/* Import path */` |
|        - |  2501 | `		  const char *zPath;` |
|        - |  2502 | `		  SyString sPath;` |
|     2836 |  2503 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2504 | `#if defined(UNTRUST)` |
|        - |  2505 | `		  if( zPath == 0 ){` |
|        - |  2506 | `			  rc = SXERR_EMPTY;` |
|        - |  2507 | `			  break;` |
|        - |  2508 | `		  }` |
|        - |  2509 | `#endif` |
|     2836 |  2510 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2511 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2512 | `#ifdef __WINNT__` |
|        2 |  2513 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2514 | `#endif` |
|     5670 |  2515 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2516 | `		  /* Remove leading and trailing white spaces */` |
|     2836 |  2517 | `		  SyStringFullTrim(&sPath);` |
|     2836 |  2518 | `		  if( sPath.nByte > 0 ){` |
|        - |  2519 | `			  /* Store the path in the corresponding conatiner */` |
|     2836 |  2520 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1417 |  2521 | `		  }` |
|     2836 |  2522 | `		  break;` |
|        - |  2523 | `									 }` |
|     1420 |  2524 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2525 | `		/* Run-Time Error report */` |
|     2842 |  2526 | `		pVm->bErrReport = 1;` |
|     2842 |  2527 | `		break;` |
|      ! 0 |  2528 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2529 | `		/* Recursion depth */` |
|      ! 0 |  2530 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2531 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2532 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2533 | `		}` |
|      ! 0 |  2534 | `		break;` |
|        - |  2535 | `									   }` |
|      ! 0 |  2536 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2537 | `		/* VM output length in bytes */` |
|      ! 0 |  2538 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2539 | `#ifdef UNTRUST` |
|        - |  2540 | `		if( pOut == 0 ){` |
|        - |  2541 | `			rc = SXERR_CORRUPT;` |
|        - |  2542 | `			break;` |
|        - |  2543 | `		}` |
|        - |  2544 | `#endif` |
|      ! 0 |  2545 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2546 | `		break;` |
|        - |  2547 | `							   }` |
|        - |  2548 |  |
|    14200 |  2549 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2550 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2551 | `		/* Create a new superglobal/global variable */` |
|    28402 |  2552 | `		const char *zName = va_arg(ap,const char *);` |
|    28402 |  2553 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2554 | `		SyHashEntry *pEntry;` |
|        - |  2555 | `		ph7_value *pObj;` |
|        - |  2556 | `		sxu32 nByte;` |
|        - |  2557 | `		sxu32 nIdx;` |
|        - |  2558 | `#ifdef UNTRUST` |
|        - |  2559 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2560 | `			rc = SXERR_CORRUPT;` |
|        - |  2561 | `			break;` |
|        - |  2562 | `		}` |
|        - |  2563 | `#endif` |
|    28402 |  2564 | `		nByte = SyStrlen(zName);` |
|    28402 |  2565 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2566 | `			/* Check if the superglobal is already installed */` |
|    28402 |  2567 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    14202 |  2568 | `		}else{` |
|        - |  2569 | `			/* Query the top active VM frame */` |
|      ! 0 |  2570 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2571 | `		}` |
|    28402 |  2572 | `		if( pEntry ){` |
|        - |  2573 | `			/* Variable already installed */` |
|      ! 0 |  2574 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2575 | `			/* Extract contents */` |
|      ! 0 |  2576 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2577 | `			if( pObj ){` |
|        - |  2578 | `				/* Overwrite old contents */` |
|      ! 0 |  2579 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2580 | `			}` |
|      ! 0 |  2581 | `		}else{` |
|        - |  2582 | `			/* Install a new variable */` |
|    28402 |  2583 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    28402 |  2584 | `			if( pObj == 0 ){` |
|      ! 0 |  2585 | `				rc = SXERR_MEM;` |
|      ! 0 |  2586 | `				break;` |
|        - |  2587 | `			}` |
|    28402 |  2588 | `			nIdx = pObj->nIdx;` |
|        - |  2589 | `			/* Copy value */` |
|    28402 |  2590 | `			PH7_MemObjStore(pValue,pObj);` |
|    28402 |  2591 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2592 | `				/* Install the superglobal */` |
|    28402 |  2593 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    14202 |  2594 | `			}else{` |
|        - |  2595 | `				/* Install in the current frame */` |
|      ! 0 |  2596 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2597 | `			}` |
|    28402 |  2598 | `			if( rc == SXRET_OK ){` |
|        - |  2599 | `				SyHashEntry *pRef;` |
|    28402 |  2600 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    28402 |  2601 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    14202 |  2602 | `				}else{` |
|      ! 0 |  2603 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2604 | `				}` |
|        - |  2605 | `				/* Install in the reference table */` |
|    28402 |  2606 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    28402 |  2607 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2608 | `					/* Register in the $GLOBALS array */` |
|    28402 |  2609 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    14200 |  2610 | `				}` |
|    14200 |  2611 | `			}` |
|        - |  2612 | `		}` |
|    28402 |  2613 | `		break;` |
|        - |  2614 | `									}` |
|     1601 |  2615 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2616 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2617 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2618 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2619 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2620 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2621 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     3204 |  2622 | `		const char *zKey   = va_arg(ap,const char *);` |
|     3204 |  2623 | `		const char *zValue = va_arg(ap,const char *);` |
|     3204 |  2624 | `		int nLen = va_arg(ap,int);` |
|        - |  2625 | `		ph7_hashmap *pMap;` |
|        - |  2626 | `		ph7_value *pValue;` |
|     3204 |  2627 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2628 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2629 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     3203 |  2630 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2631 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2632 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     3202 |  2633 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2634 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2635 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     3202 |  2636 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2637 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2638 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     3202 |  2639 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2640 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2641 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     3202 |  2642 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2643 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2644 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2645 | `		}else{` |
|        - |  2646 | `			/* Extract the $_SERVER superglobal */` |
|     3202 |  2647 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2648 | `		}` |
|     3204 |  2649 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2650 | `			/* No such entry */` |
|      ! 0 |  2651 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2652 | `			break;` |
|        - |  2653 | `		}` |
|        - |  2654 | `		/* Point to the hashmap */` |
|     3204 |  2655 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2656 | `		/* Perform the insertion */` |
|     3204 |  2657 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     3204 |  2658 | `		break;` |
|        - |  2659 | `								   }` |
|       11 |  2660 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2661 | `		/* Script arguments */` |
|       24 |  2662 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2663 | `		ph7_hashmap *pMap;` |
|        - |  2664 | `		ph7_value *pValue;` |
|        - |  2665 | `		sxu32 n;` |
|       24 |  2666 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2667 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2668 | `			break;` |
|        - |  2669 | `		}` |
|        - |  2670 | `		/* Extract the $argv array */` |
|       24 |  2671 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2672 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2673 | `			/* No such entry */` |
|      ! 0 |  2674 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2675 | `			break;` |
|        - |  2676 | `		}` |
|        - |  2677 | `		/* Point to the hashmap */` |
|       24 |  2678 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2679 | `		/* Perform the insertion */` |
|       24 |  2680 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2681 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2682 | `		if( rc == SXRET_OK ){` |
|       24 |  2683 | `			if( pMap->nEntry > 1 ){` |
|        - |  2684 | `				/* Append space separator first */` |
|       18 |  2685 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2686 | `			}` |
|       24 |  2687 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2688 | `		}` |
|       24 |  2689 | `		break;` |
|        - |  2690 | `								  }` |
|      ! 0 |  2691 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2692 | `		/* error_log() consumer */` |
|      ! 0 |  2693 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2694 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2695 | `		break;` |
|        - |  2696 | `										}` |
|      ! 0 |  2697 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2698 | `		/* Script return value */` |
|      ! 0 |  2699 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2700 | `#ifdef UNTRUST` |
|        - |  2701 | `		if( ppValue == 0 ){` |
|        - |  2702 | `			rc = SXERR_CORRUPT;` |
|        - |  2703 | `			break;` |
|        - |  2704 | `		}` |
|        - |  2705 | `#endif` |
|      ! 0 |  2706 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2707 | `		break;` |
|        - |  2708 | `								   }` |
|     2834 |  2709 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2710 | `		/* Register an IO stream device */` |
|     5670 |  2711 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2712 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8502 |  2713 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5670 |  2714 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2715 | `				/* Invalid stream */` |
|      ! 0 |  2716 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2717 | `				break;` |
|        - |  2718 | `		}` |
|     5670 |  2719 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2720 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2836 |  2721 | `			pVm->pDefStream = pStream;` |
|     1417 |  2722 | `		}` |
|        - |  2723 | `		/* Insert in the appropriate container */` |
|     5670 |  2724 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5670 |  2725 | `		break;` |
|        - |  2726 | `								  }` |
|       11 |  2727 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2728 | `		/* Point to the VM internal output consumer buffer */` |
|       22 |  2729 | `		const void **ppOut = va_arg(ap,const void **);` |
|       22 |  2730 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2731 | `#ifdef UNTRUST` |
|        - |  2732 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2733 | `			rc = SXERR_CORRUPT;` |
|        - |  2734 | `			break;` |
|        - |  2735 | `		}` |
|        - |  2736 | `#endif` |
|       22 |  2737 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       22 |  2738 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       22 |  2739 | `		break;` |
|        - |  2740 | `									   }` |
|       11 |  2741 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2742 | `		/* Raw HTTP request*/` |
|       22 |  2743 | `		const char *zRequest = va_arg(ap,const char *);` |
|       22 |  2744 | `		int nByte = va_arg(ap,int);` |
|       22 |  2745 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2746 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2747 | `			break;` |
|        - |  2748 | `		}` |
|       22 |  2749 | `		if( nByte < 0 ){` |
|        - |  2750 | `			/* Compute length automatically */` |
|      ! 0 |  2751 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2752 | `		}` |
|        - |  2753 | `		/* Process the request */` |
|       22 |  2754 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2755 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       22 |  2756 | `		if( rc == SXRET_OK ){` |
|       22 |  2757 | `			pVm->bHttpContext = 1;` |
|       11 |  2758 | `		}` |
|       22 |  2759 | `		break;` |
|        - |  2760 | `									}` |
|       11 |  2761 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2762 | `		/* Extract HTTP response status code */` |
|       22 |  2763 | `		int *pStatus = va_arg(ap, int *);` |
|       22 |  2764 | `		if( pStatus ){` |
|       22 |  2765 | `			*pStatus = pVm->iResponseStatus;` |
|       11 |  2766 | `		}` |
|       22 |  2767 | `		break;` |
|        - |  2768 | `										}` |
|       11 |  2769 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2770 | `		/* Iterate response headers via callback */` |
|        - |  2771 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       22 |  2772 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       22 |  2773 | `		void *pUserData = va_arg(ap, void *);` |
|       22 |  2774 | `		if( xCallback ){` |
|       22 |  2775 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       22 |  2776 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       34 |  2777 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2778 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2779 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2780 | `							   pUserData);` |
|       12 |  2781 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2782 | `					break;` |
|        - |  2783 | `				}` |
|        6 |  2784 | `			}` |
|       11 |  2785 | `		}` |
|       22 |  2786 | `		break;` |
|        - |  2787 | `										 }` |
|      ! 0 |  2788 | `	default:` |
|        - |  2789 | `		/* Unknown configuration option */` |
|      ! 0 |  2790 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2791 | `		break;` |
|        - |  2792 | `	}` |
|    45874 |  2793 | `	return rc;` |
|        2 |  2794 |  |
|        - |  2795 | `/* Forward declaration */` |
|        - |  2796 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2797 | `/*` |
|        - |  2798 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2799 | ` * format.` |
|        - |  2800 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2801 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2802 | ` * (STDOUT).` |
|        - |  2803 | ` */` |
|        2 |  2804 | `static sxi32 VmByteCodeDump(` |
|        - |  2805 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2806 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2807 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2808 | `	)` |
|        1 |  2809 |  |
|        - |  2810 | `	static const char zDump[] = {` |
|        - |  2811 | `		"====================================================\n"` |
|        - |  2812 | `		"PH7 VM Dump\n"` |
|        - |  2813 | `		"====================================================\n"` |
|        - |  2814 | `	};` |
|        - |  2815 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2816 | `	sxi32 rc = SXRET_OK;` |
|        - |  2817 | `	sxu32 n;` |
|        - |  2818 | `	/* Point to the PH7 instructions */` |
|        3 |  2819 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2820 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2821 | `	n = 0;` |
|        3 |  2822 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2823 | `	/* Dump instructions */` |
|        7 |  2824 | `	for(;;){` |
|       15 |  2825 | `		if( pInstr >= pEnd ){` |
|        - |  2826 | `			/* No more instructions */` |
|        3 |  2827 | `			break;` |
|        - |  2828 | `		}` |
|        - |  2829 | `		/* Format and call the consumer callback */` |
|       19 |  2830 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2831 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2832 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2833 | `		if( rc != SXRET_OK ){` |
|        - |  2834 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2835 | `			return rc;` |
|        - |  2836 | `		}` |
|       13 |  2837 | `		++n;` |
|       13 |  2838 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2839 | `	}` |
|        3 |  2840 | `	return rc;` |
|        2 |  2841 |  |
|        - |  2842 | `/* Forward declaration */` |
|        - |  2843 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2844 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2845 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2846 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2847 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2848 | `/*` |
|        - |  2849 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2850 | ` * consumer callback.` |
|        - |  2851 | ` */` |
|      604 |  2852 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2853 |  |
|      605 |  2854 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      605 |  2855 | `	sxi32 rc = SXRET_OK;` |
|        - |  2856 | `	/* Append a new line */` |
|        - |  2857 | `#ifdef __WINNT__` |
|        1 |  2858 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2859 | `#else` |
|      604 |  2860 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2861 | `#endif` |
|        - |  2862 | `	/* Invoke the output consumer callback */` |
|      605 |  2863 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      605 |  2864 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      605 |  2865 | `	return rc;` |
|        1 |  2866 |  |
|        - |  2867 | `/*` |
|        - |  2868 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2869 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2870 | ` * information.` |
|        - |  2871 | ` */` |
|      152 |  2872 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2873 |  |
|      154 |  2874 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2875 | `		ph7_value apArg[4];` |
|        - |  2876 | `		ph7_value *apArgPtr[4];` |
|        - |  2877 | `		ph7_value sResult;` |
|        - |  2878 | `		SyString sErr;` |
|        - |  2879 | `		/* Prepare arguments */` |
|       76 |  2880 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2881 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2882 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2883 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2884 | `		if( pFile ){` |
|       76 |  2885 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2886 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2887 | `		}else{` |
|      ! 0 |  2888 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2889 | `		}` |
|       76 |  2890 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2891 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2892 | `		/* Set up pointer array */` |
|       76 |  2893 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2894 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2895 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2896 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2897 | `		/* Call the handler */` |
|       76 |  2898 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2899 | `		/* Check return value */` |
|       76 |  2900 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2901 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2902 | `		}` |
|        - |  2903 | `		/* Release */` |
|       76 |  2904 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2905 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2906 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2907 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2908 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2909 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2910 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2911 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2912 | `	}` |
|        - |  2913 | `	/* No handler, always call error handler */` |
|       79 |  2914 | `	return TRUE;` |
|       78 |  2915 |  |
|      110 |  2916 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2917 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2918 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2919 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2920 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2921 | `	)` |
|        2 |  2922 |  |
|      112 |  2923 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2924 | `	SyString *pFile;` |
|        - |  2925 | `	char *zErr;` |
|      112 |  2926 | `	sxi32 rc = SXRET_OK;` |
|      112 |  2927 | `	if( !pVm->bErrReport ){` |
|        - |  2928 | `		/* Don't bother reporting errors */` |
|        3 |  2929 | `		return SXRET_OK;` |
|        - |  2930 | `	}` |
|        - |  2931 | `	/* Reset the working buffer */` |
|      110 |  2932 | `	SyBlobReset(pWorker);` |
|        - |  2933 | `	/* Peek the processed file if available */` |
|      110 |  2934 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      110 |  2935 | `	if( pFile ){` |
|        - |  2936 | `		/* Append file name */` |
|      110 |  2937 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      110 |  2938 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2939 | `	}` |
|        - |  2940 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2941 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2942 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2943 | `	 * E_DEPRECATED). */` |
|      110 |  2944 | `	zErr = "Error:  ";` |
|      110 |  2945 | `	switch(iErr){` |
|       19 |  2946 | `	case PH7_CTX_WARNING:` |
|       40 |  2947 | `		zErr = "Warning:  ";` |
|       40 |  2948 | `		break;` |
|        6 |  2949 | `	case PH7_CTX_NOTICE:` |
|       14 |  2950 | `		zErr = "Notice:  ";` |
|       12 |  2951 | `		break;` |
|       29 |  2952 | `	default:` |
|        - |  2953 | `		/* keep iErr unchanged */` |
|       58 |  2954 | `		break;` |
|        - |  2955 | `	}` |
|      110 |  2956 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      110 |  2957 | `	if( pFuncName ){` |
|        - |  2958 | `		/* Append function name first */` |
|       23 |  2959 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2960 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2961 | `	}` |
|      110 |  2962 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2963 | `	/* Check for user error handler.  compute length of C string */` |
|      110 |  2964 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2965 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2966 | `	}` |
|      110 |  2967 | `	return rc;` |
|       57 |  2968 |  |
|        - |  2969 | `/*` |
|        - |  2970 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  2971 | ` *` |
|        - |  2972 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  2973 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  2974 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  2975 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  2976 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  2977 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  2978 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  2979 | ` * calling it from a VM op.` |
|        - |  2980 | ` */` |
|      ! 0 |  2981 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  2982 |  |
|      ! 0 |  2983 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  2984 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  2985 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  2986 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  2987 | `	return PH7_ABORT;` |
|      ! 0 |  2988 |  |
|        - |  2989 | `/*` |
|        - |  2990 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  2991 | ` */` |
|      ! 0 |  2992 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  2993 |  |
|      ! 0 |  2994 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  2995 |  |
|        - |  2996 | `/*` |
|        - |  2997 | ` * Single source of truth for the call-recursion cap policy. Each recursion` |
|        - |  2998 | ` * entry point (OP_CALL, eval/include, fibers/generators) tests this before` |
|        - |  2999 | ` * descending another native C frame; the control flow on a hit differs per` |
|        - |  3000 | ` * site, but the rule itself lives here.` |
|        - |  3001 | ` */` |
|    31592 |  3002 | `static int VmRecursionExceeded(ph7_vm *pVm)` |
|        2 |  3003 |  |
|    31594 |  3004 | `	return pVm->nRecursionDepth > pVm->nMaxDepth;` |
|        2 |  3005 |  |
|        - |  3006 | `/*` |
|        - |  3007 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|        - |  3008 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|        - |  3009 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|        - |  3010 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|        - |  3011 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|        - |  3012 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|        - |  3013 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|        - |  3014 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|        - |  3015 | ` * register_shutdown_function() callbacks. Used by every recursion path —` |
|        - |  3016 | ` * OP_CALL, eval()/include/require (VmEvalChunk) and fibers/generators` |
|        - |  3017 | ` * (VmStartCtx/VmResumeCtx).` |
|        - |  3018 | ` *` |
|        - |  3019 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|        - |  3020 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|        - |  3021 | ` * re-enter and loop.` |
|        - |  3022 | ` */` |
|        6 |  3023 | `static sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|        1 |  3024 |  |
|        7 |  3025 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  3026 | `		return PH7_ABORT;` |
|        - |  3027 | `	}` |
|        7 |  3028 | `	pVm->iExitStatus = 255;` |
|        7 |  3029 | `	pVm->bHaltRequested = 1;` |
|        7 |  3030 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|        7 |  3031 | `	return PH7_ABORT;` |
|        4 |  3032 |  |
|        - |  3033 | `/*` |
|        - |  3034 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3035 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3036 | ` * information.` |
|        - |  3037 | ` */` |
|       44 |  3038 | `static sxi32 VmThrowErrorAp(` |
|        - |  3039 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3040 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  3041 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  3042 | `	const char *zFormat, /* Format message */` |
|        - |  3043 | `	va_list ap           /* Variable list of arguments */` |
|        - |  3044 | `	)` |
|        2 |  3045 |  |
|       46 |  3046 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  3047 | `	SyBlob sMsg;` |
|        - |  3048 | `	SyString *pFile;` |
|        - |  3049 | `	char *zErr;` |
|       46 |  3050 | `	sxi32 rc = SXRET_OK;` |
|       46 |  3051 | `	if( !pVm->bErrReport ){` |
|        - |  3052 | `		/* Don't bother reporting errors */` |
|      ! 0 |  3053 | `		return SXRET_OK;` |
|        - |  3054 | `	}` |
|        - |  3055 | `	/* Reset the working buffer */` |
|       46 |  3056 | `	SyBlobReset(pWorker);` |
|        - |  3057 | `	/* Peek the processed file if available */` |
|       46 |  3058 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       46 |  3059 | `	if( pFile ){` |
|        - |  3060 | `		/* Append file name */` |
|       46 |  3061 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       46 |  3062 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       22 |  3063 | `	}` |
|        - |  3064 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  3065 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  3066 | `	 * the correct errno value. */` |
|       46 |  3067 | `	zErr = "Error:  ";` |
|       46 |  3068 | `	switch(iErr){` |
|        4 |  3069 | `	case PH7_CTX_WARNING:` |
|        9 |  3070 | `		zErr = "Warning:  ";` |
|        9 |  3071 | `		break;` |
|        3 |  3072 | `	case PH7_CTX_NOTICE:` |
|        7 |  3073 | `		zErr = "Notice:  ";` |
|        6 |  3074 | `		break;` |
|       15 |  3075 | `	default:` |
|        - |  3076 | `		/* do not change iErr */` |
|       30 |  3077 | `		break;` |
|        - |  3078 | `	}` |
|       46 |  3079 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       46 |  3080 | `	if( pFuncName ){` |
|        - |  3081 | `		/* Append function name first */` |
|       26 |  3082 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  3083 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  3084 | `	}` |
|        - |  3085 | `	/* Format the raw message */` |
|       46 |  3086 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       46 |  3087 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  3088 | `	/* Check if a user error handler is installed */` |
|       46 |  3089 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  3090 | `		/* No handler or handler returned TRUE, normal processing */` |
|       31 |  3091 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       31 |  3092 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       15 |  3093 | `	}` |
|       46 |  3094 | `	SyBlobRelease(&sMsg);` |
|       46 |  3095 | `	return rc;` |
|       24 |  3096 |  |
|        - |  3097 | `/*` |
|        - |  3098 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  3099 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  3100 | ` * possible.` |
|        - |  3101 | ` */` |
|       40 |  3102 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        2 |  3103 |  |
|        - |  3104 | `	ph7_class *pClass;` |
|       42 |  3105 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  3106 | `	ph7_class_instance *pThis;` |
|        - |  3107 | `	ph7_class_method *pCons;` |
|        - |  3108 | `	ph7_value sArg;` |
|        - |  3109 | `	ph7_value *apArg[1];` |
|        - |  3110 | `	SyBlob sMsg;` |
|        - |  3111 | `	SyString sMsgStr;` |
|        - |  3112 | `	VmFrame *pFrame;` |
|        - |  3113 | `	sxi32 rc;` |
|       42 |  3114 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       42 |  3115 | `	if( pClass == 0 ){` |
|      ! 0 |  3116 | `		return PH7_ABORT;` |
|        - |  3117 | `	}` |
|       42 |  3118 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       42 |  3119 | `	if( pThis == 0 ){` |
|      ! 0 |  3120 | `		return PH7_ABORT;` |
|        - |  3121 | `	}` |
|       42 |  3122 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3123 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  3124 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  3125 | `	{` |
|       42 |  3126 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       42 |  3127 | `		if( pOwner ){` |
|       42 |  3128 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       20 |  3129 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       22 |  3130 | `		}else{` |
|      ! 0 |  3131 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  3132 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  3133 | `		}` |
|        - |  3134 | `	}` |
|       42 |  3135 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       42 |  3136 | `	if( pCons ){` |
|       42 |  3137 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       42 |  3138 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       42 |  3139 | `		apArg[0] = &sArg;` |
|       42 |  3140 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       42 |  3141 | `		PH7_MemObjRelease(&sArg);` |
|       20 |  3142 | `	}` |
|       42 |  3143 | `	SyBlobRelease(&sMsg);` |
|       42 |  3144 | `	pFrame = pVm->pFrame;` |
|       42 |  3145 | `	if( pFrame ){` |
|       42 |  3146 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       42 |  3147 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       20 |  3148 | `	}` |
|       42 |  3149 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       42 |  3150 | `	PH7_ClassInstanceUnref(pThis);` |
|       42 |  3151 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3152 | `		return PH7_ABORT;` |
|        - |  3153 | `	}` |
|       42 |  3154 | `	return PH7_EXCEPTION;` |
|       22 |  3155 |  |
|        - |  3156 |  |
|        - |  3157 | `/*` |
|        - |  3158 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  3159 | ` */` |
|        4 |  3160 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  3161 |  |
|        - |  3162 | `	ph7_class *pErrClass;` |
|        - |  3163 | `	ph7_class_instance *pThis;` |
|        - |  3164 | `	ph7_class_method *pCons;` |
|        - |  3165 | `	ph7_value sArg;` |
|        - |  3166 | `	ph7_value *apArg[1];` |
|        - |  3167 | `	SyBlob sMsg;` |
|        - |  3168 | `	SyString sMsgStr;` |
|        - |  3169 | `	VmFrame *pFrame;` |
|        - |  3170 | `	sxi32 rc;` |
|        5 |  3171 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  3172 | `	if( pErrClass == 0 ){` |
|      ! 0 |  3173 | `		return PH7_ABORT;` |
|        - |  3174 | `	}` |
|        5 |  3175 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  3176 | `	if( pThis == 0 ){` |
|      ! 0 |  3177 | `		return PH7_ABORT;` |
|        - |  3178 | `	}` |
|        5 |  3179 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3180 | `	{` |
|        5 |  3181 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  3182 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  3183 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  3184 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  3185 | `	}` |
|        5 |  3186 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  3187 | `	if( pCons ){` |
|        5 |  3188 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  3189 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  3190 | `		apArg[0] = &sArg;` |
|        5 |  3191 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  3192 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  3193 | `	}` |
|        5 |  3194 | `	SyBlobRelease(&sMsg);` |
|        5 |  3195 | `	pFrame = pVm->pFrame;` |
|        5 |  3196 | `	if( pFrame ){` |
|        5 |  3197 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  3198 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  3199 | `	}` |
|        5 |  3200 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  3201 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  3202 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3203 | `		return PH7_ABORT;` |
|        - |  3204 | `	}` |
|        5 |  3205 | `	return PH7_EXCEPTION;` |
|        3 |  3206 |  |
|        - |  3207 |  |
|        - |  3208 | `/*` |
|        - |  3209 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  3210 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  3211 | ` * For class types, instanceof is verified.` |
|        - |  3212 | ` *` |
|        - |  3213 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  3214 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  3215 | ` */` |
|        - |  3216 | `/*` |
|        - |  3217 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  3218 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  3219 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  3220 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  3221 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  3222 | ` */` |
|       22 |  3223 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  3224 |  |
|        - |  3225 | `	const char *z, *zEnd, *zTail;` |
|        - |  3226 | `	sxu32 n;` |
|        - |  3227 | `	sxu8 bReal;` |
|        - |  3228 | `	sxi32 rc;` |
|       24 |  3229 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3230 | `		return 0;` |
|        - |  3231 | `	}` |
|       24 |  3232 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       24 |  3233 | `	n = SyBlobLength(&pValue->sBlob);` |
|       24 |  3234 | `	zEnd = z + n;` |
|       24 |  3235 | `	if( n == 0 ){` |
|      ! 0 |  3236 | `		return 0;` |
|        - |  3237 | `	}` |
|       24 |  3238 | `	zTail = 0;` |
|       24 |  3239 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       24 |  3240 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  3241 | `		return 0;` |
|        - |  3242 | `	}` |
|        - |  3243 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       18 |  3244 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  3245 | `		zTail++;` |
|      ! 0 |  3246 | `	}` |
|       18 |  3247 | `	return zTail == zEnd ? 1 : 0;` |
|       13 |  3248 |  |
|        - |  3249 |  |
|        - |  3250 | `/*` |
|        - |  3251 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  3252 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  3253 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  3254 | ` *   0 if it's not strictly numeric.` |
|        - |  3255 | ` */` |
|       16 |  3256 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  3257 |  |
|        - |  3258 | `	const char *z, *zEnd, *zTail;` |
|        - |  3259 | `	sxu32 n;` |
|       18 |  3260 | `	sxu8 bReal = 0;` |
|        - |  3261 | `	sxi32 rc;` |
|       18 |  3262 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3263 | `		return 0;` |
|        - |  3264 | `	}` |
|       18 |  3265 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  3266 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  3267 | `	zEnd = z + n;` |
|       18 |  3268 | `	if( n == 0 ) return 0;` |
|       18 |  3269 | `	zTail = 0;` |
|       18 |  3270 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  3271 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  3272 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  3273 | `	if( zTail != zEnd ) return 0;` |
|       15 |  3274 | `	return bReal ? 2 : 1;` |
|       10 |  3275 |  |
|        - |  3276 |  |
|        - |  3277 | `/*` |
|        - |  3278 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  3279 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  3280 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  3281 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  3282 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  3283 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  3284 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  3285 | ` * throw.` |
|        - |  3286 | ` *` |
|        - |  3287 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  3288 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  3289 | ` */` |
|       98 |  3290 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  3291 |  |
|        - |  3292 | `	sxu32 i;` |
|        - |  3293 | `	ph7_type_alt *aAlts;` |
|        - |  3294 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  3295 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  3296 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3297 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  3298 | `	}` |
|       88 |  3299 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  3300 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  3301 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  3302 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  3303 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  3304 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  3305 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  3306 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  3307 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  3308 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  3309 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  3310 | `	}` |
|        - |  3311 | `	/* Object handling */` |
|       88 |  3312 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  3313 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  3314 | `		if( bHasClassAlt ){` |
|       14 |  3315 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  3316 | `			ph7_class *pSelfNow = 0;` |
|       14 |  3317 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  3318 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  3319 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  3320 | `			}` |
|       26 |  3321 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  3322 | `				ph7_class *pExpected;` |
|        - |  3323 | `				SyString *pCN;` |
|       22 |  3324 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  3325 | `				pCN = &aAlts[i].sClass;` |
|       22 |  3326 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  3327 | `					pExpected = pSelfNow;` |
|       22 |  3328 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  3329 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3330 | `				}else{` |
|       22 |  3331 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  3332 | `				}` |
|       22 |  3333 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  3334 | `					return SXRET_OK;` |
|        - |  3335 | `				}` |
|        8 |  3336 | `			}` |
|        2 |  3337 | `		}` |
|        9 |  3338 | `		return SXERR_INVALID;` |
|        - |  3339 | `	}` |
|        - |  3340 | `	/* Array handling */` |
|       72 |  3341 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  3342 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  3343 | `	}` |
|        - |  3344 | `	/* Scalar handling — exact match first */` |
|       66 |  3345 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  3346 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  3347 | `	}` |
|       42 |  3348 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  3349 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  3350 | `	}` |
|       38 |  3351 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  3352 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  3353 | `	}` |
|       18 |  3354 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3355 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  3356 | `	}` |
|       18 |  3357 | `	if( bStrict ){` |
|        - |  3358 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  3359 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  3360 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  3361 | `			return SXRET_OK;` |
|        - |  3362 | `		}` |
|      ! 0 |  3363 | `		return SXERR_INVALID;` |
|        - |  3364 | `	}` |
|        - |  3365 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  3366 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  3367 | `	 * to match PHP's union RFC. */` |
|        - |  3368 | `	{` |
|       18 |  3369 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  3370 | `		if( bHasInt ){` |
|        - |  3371 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  3372 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  3373 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3374 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3375 | `				return SXRET_OK;` |
|        - |  3376 | `			}` |
|       18 |  3377 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3378 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  3379 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  3380 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3381 | `					return SXRET_OK;` |
|        - |  3382 | `				}` |
|      ! 0 |  3383 | `			}` |
|       18 |  3384 | `			if( kind == 1 ){` |
|        9 |  3385 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  3386 | `				return SXRET_OK;` |
|        - |  3387 | `			}` |
|        4 |  3388 | `		}` |
|       10 |  3389 | `		if( bHasFloat ){` |
|       10 |  3390 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  3391 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  3392 | `				return SXRET_OK;` |
|        - |  3393 | `			}` |
|       10 |  3394 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  3395 | `				PH7_MemObjToReal(pValue);` |
|        7 |  3396 | `				return SXRET_OK;` |
|        - |  3397 | `			}` |
|        1 |  3398 | `		}` |
|        3 |  3399 | `		if( bHasString ){` |
|      ! 0 |  3400 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3401 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3402 | `				return SXRET_OK;` |
|        - |  3403 | `			}` |
|      ! 0 |  3404 | `		}` |
|        3 |  3405 | `		if( bHasBool ){` |
|      ! 0 |  3406 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3407 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3408 | `				return SXRET_OK;` |
|        - |  3409 | `			}` |
|      ! 0 |  3410 | `		}` |
|        - |  3411 | `	}` |
|        3 |  3412 | `	return SXERR_INVALID;` |
|       51 |  3413 |  |
|        - |  3414 |  |
|        - |  3415 | `/*` |
|        - |  3416 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3417 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3418 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3419 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3420 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3421 | ` */` |
|       36 |  3422 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  3423 |  |
|       38 |  3424 | `	if( bStrict ){` |
|        - |  3425 | `		/* Only int -> float widening is allowed implicitly. */` |
|       12 |  3426 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3427 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3428 | `			return SXRET_OK;` |
|        - |  3429 | `		}` |
|       10 |  3430 | `		return SXERR_INVALID;` |
|        - |  3431 | `	}` |
|        - |  3432 | `	{` |
|       28 |  3433 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3434 | `		if( xCast ) xCast(pVal);` |
|        - |  3435 | `	}` |
|       28 |  3436 | `	return SXRET_OK;` |
|       20 |  3437 |  |
|        - |  3438 |  |
|        - |  3439 | `/*` |
|        - |  3440 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3441 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3442 | ` *` |
|        - |  3443 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3444 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3445 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3446 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3447 | ` */` |
|       10 |  3448 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        2 |  3449 |  |
|       12 |  3450 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|       12 |  3451 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|       12 |  3452 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       12 |  3453 | `		if( pDeclared->zString && nCopy > 0 ){` |
|       12 |  3454 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        5 |  3455 | `		}` |
|       12 |  3456 | `		zBuf[nCopy] = 0;` |
|       12 |  3457 | `		return zBuf;` |
|        - |  3458 | `	}` |
|      ! 0 |  3459 | `	switch( nType ){` |
|      ! 0 |  3460 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3461 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3462 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3463 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3464 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3465 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3466 | `		default:             return "scalar";` |
|        - |  3467 | `	}` |
|        7 |  3468 |  |
|        - |  3469 |  |
|        - |  3470 | `/*` |
|        - |  3471 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3472 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3473 | ` */` |
|       18 |  3474 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3475 |  |
|       19 |  3476 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3477 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3478 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3479 | `	return zBuf;` |
|        1 |  3480 |  |
|        - |  3481 |  |
|     6426 |  3482 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3483 |  |
|        - |  3484 | `	SyHashEntry *pSlot;` |
|        - |  3485 | `	VmClassAttr *pVmAttr;` |
|        - |  3486 | `	ph7_class_attr *pAttr;` |
|     6428 |  3487 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|     6428 |  3488 | `	if( pSlot == 0 ){` |
|     6216 |  3489 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3490 | `	}` |
|      214 |  3491 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      214 |  3492 | `	pAttr = pVmAttr->pAttr;` |
|      214 |  3493 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3494 | `		return SXRET_OK;` |
|        - |  3495 | `	}` |
|        - |  3496 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3497 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3498 | `	 * matching PHP's documented behavior. */` |
|      214 |  3499 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3500 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3501 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3502 |  |
|       16 |  3503 | `		if( rc == SXRET_OK ){` |
|        9 |  3504 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3505 | `			return SXRET_OK;` |
|        - |  3506 | `		}` |
|        7 |  3507 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3508 | `			char zBuf[128];` |
|        4 |  3509 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3510 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3511 | `		}` |
|        5 |  3512 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3513 | `	}` |
|        - |  3514 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      200 |  3515 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3516 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3517 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3518 | `			return SXRET_OK;` |
|        - |  3519 | `		}` |
|        3 |  3520 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3521 | `	}` |
|        - |  3522 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3523 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3524 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      188 |  3525 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3526 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3527 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3528 | `			return SXRET_OK;` |
|        - |  3529 | `		}` |
|        7 |  3530 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3531 | `	}` |
|      178 |  3532 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3533 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3534 | `		 * currently active on the self-stack. */` |
|       26 |  3535 | `		ph7_class *pExpected = 0;` |
|       26 |  3536 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3537 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3538 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3539 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3540 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3541 | `		}` |
|       26 |  3542 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3543 | `			pExpected = pSelfNow;` |
|       24 |  3544 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3545 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3546 | `		}else{` |
|       22 |  3547 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3548 | `		}` |
|       26 |  3549 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3550 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3551 | `		}` |
|       26 |  3552 | `		if( pExpected ){` |
|       22 |  3553 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3554 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3555 | `				char zBuf[128];` |
|        7 |  3556 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3557 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3558 | `			}` |
|        8 |  3559 | `		}` |
|       22 |  3560 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3561 | `		return SXRET_OK;` |
|        - |  3562 | `	}` |
|        - |  3563 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3564 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      154 |  3565 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3566 | `		char zBuf[128];` |
|       10 |  3567 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3568 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3569 | `	}` |
|      148 |  3570 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       28 |  3571 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       28 |  3572 | `		if( xCast ){` |
|        - |  3573 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       28 |  3574 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3575 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3576 | `			}` |
|       26 |  3577 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3578 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3579 | `			}` |
|        - |  3580 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3581 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3582 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       29 |  3583 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       19 |  3584 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       21 |  3585 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|       12 |  3586 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3587 | `			}` |
|       12 |  3588 | `			xCast(pValue);` |
|        5 |  3589 | `		}` |
|        5 |  3590 | `	}` |
|      132 |  3591 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      132 |  3592 | `	return SXRET_OK;` |
|     3215 |  3593 |  |
|        - |  3594 |  |
|        - |  3595 | `/*` |
|        - |  3596 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3597 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3598 | ` * information.` |
|        - |  3599 | ` * ------------------------------------` |
|        - |  3600 | ` * Simple boring wrapper function.` |
|        - |  3601 | ` * ------------------------------------` |
|        - |  3602 | ` */` |
|       20 |  3603 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3604 |  |
|        - |  3605 | `	va_list ap;` |
|        - |  3606 | `	sxi32 rc;` |
|       21 |  3607 | `	va_start(ap,zFormat);` |
|       21 |  3608 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       21 |  3609 | `	va_end(ap);` |
|       21 |  3610 | `	return rc;` |
|        1 |  3611 |  |
|        - |  3612 | `/*` |
|        - |  3613 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3614 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3615 | ` */` |
|       36 |  3616 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        2 |  3617 |  |
|        - |  3618 | `	ph7_class *pClass;` |
|        - |  3619 | `	ph7_class_instance *pThis;` |
|        - |  3620 | `	ph7_class_method *pCons;` |
|        - |  3621 | `	ph7_value sArg;` |
|        - |  3622 | `	ph7_value *apArg[1];` |
|        - |  3623 | `	SyBlob sMsg;` |
|        - |  3624 | `	SyString sMsgStr;` |
|        - |  3625 | `	VmFrame *pFrame;` |
|        - |  3626 | `	sxi32 rc;` |
|       38 |  3627 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       38 |  3628 | `	if( pClass == 0 ){` |
|      ! 0 |  3629 | `		return PH7_ABORT;` |
|        - |  3630 | `	}` |
|       38 |  3631 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       38 |  3632 | `	if( pThis == 0 ){` |
|      ! 0 |  3633 | `		return PH7_ABORT;` |
|        - |  3634 | `	}` |
|       38 |  3635 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       38 |  3636 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       18 |  3637 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       38 |  3638 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       38 |  3639 | `	if( pCons ){` |
|       38 |  3640 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       38 |  3641 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       38 |  3642 | `		apArg[0] = &sArg;` |
|       38 |  3643 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       38 |  3644 | `		PH7_MemObjRelease(&sArg);` |
|       18 |  3645 | `	}` |
|       38 |  3646 | `	SyBlobRelease(&sMsg);` |
|       38 |  3647 | `	pFrame = pVm->pFrame;` |
|       38 |  3648 | `	if( pFrame ){` |
|       38 |  3649 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 |  3650 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       18 |  3651 | `	}` |
|       38 |  3652 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  3653 | `	PH7_ClassInstanceUnref(pThis);` |
|       38 |  3654 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3655 | `		return PH7_ABORT;` |
|        - |  3656 | `	}` |
|       34 |  3657 | `	return PH7_EXCEPTION;` |
|       20 |  3658 |  |
|        - |  3659 | `/*` |
|        - |  3660 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3661 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3662 | ` */` |
|        6 |  3663 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3664 |  |
|        - |  3665 | `	ph7_class *pClass;` |
|        - |  3666 | `	ph7_class_instance *pThis;` |
|        - |  3667 | `	ph7_class_method *pCons;` |
|        - |  3668 | `	ph7_value sArg;` |
|        - |  3669 | `	ph7_value *apArg[1];` |
|        - |  3670 | `	SyBlob sMsg;` |
|        - |  3671 | `	SyString sMsgStr;` |
|        - |  3672 | `	VmFrame *pFrame;` |
|        - |  3673 | `	sxi32 rc;` |
|        7 |  3674 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3675 | `	if( pClass == 0 ){` |
|      ! 0 |  3676 | `		return PH7_ABORT;` |
|        - |  3677 | `	}` |
|        7 |  3678 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3679 | `	if( pThis == 0 ){` |
|      ! 0 |  3680 | `		return PH7_ABORT;` |
|        - |  3681 | `	}` |
|        7 |  3682 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3683 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3684 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3685 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3686 | `	if( pCons ){` |
|        7 |  3687 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3688 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3689 | `		apArg[0] = &sArg;` |
|        7 |  3690 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3691 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3692 | `	}` |
|        7 |  3693 | `	SyBlobRelease(&sMsg);` |
|        7 |  3694 | `	pFrame = pVm->pFrame;` |
|        7 |  3695 | `	if( pFrame ){` |
|        7 |  3696 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3697 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3698 | `	}` |
|        7 |  3699 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3700 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3701 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3702 | `		return PH7_ABORT;` |
|        - |  3703 | `	}` |
|      ! 0 |  3704 | `	return PH7_EXCEPTION;` |
|        4 |  3705 |  |
|        - |  3706 | `/*` |
|        - |  3707 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3708 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3709 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3710 | ` */` |
|       16 |  3711 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        1 |  3712 |  |
|       17 |  3713 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  3714 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3715 | `	}` |
|       13 |  3716 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3717 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        5 |  3718 | `		if( pThis && pThis->pClass ){` |
|        5 |  3719 | `			SyString *pName = &pThis->pClass->sName;` |
|        5 |  3720 | `			sxu32 n = pName->nByte;` |
|        5 |  3721 | `			if( n >= nBuf ){` |
|      ! 0 |  3722 | `				n = nBuf - 1;` |
|      ! 0 |  3723 | `			}` |
|        5 |  3724 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        5 |  3725 | `			zBuf[n] = 0;` |
|        5 |  3726 | `			return zBuf;` |
|        - |  3727 | `		}` |
|      ! 0 |  3728 | `		return "object";` |
|        - |  3729 | `	}` |
|        9 |  3730 | `	return ph7_type_name(pVal);` |
|        9 |  3731 |  |
|        - |  3732 | `/*` |
|        - |  3733 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3734 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3735 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3736 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3737 | ` */` |
|       16 |  3738 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        1 |  3739 |  |
|        - |  3740 | `	ph7_class *pClass;` |
|        - |  3741 | `	ph7_class_instance *pThis;` |
|        - |  3742 | `	ph7_class_method *pCons;` |
|        - |  3743 | `	ph7_value sArg;` |
|        - |  3744 | `	ph7_value *apArg[1];` |
|        - |  3745 | `	SyBlob sMsg;` |
|        - |  3746 | `	SyString sMsgStr;` |
|        - |  3747 | `	VmFrame *pFrame;` |
|        - |  3748 | `	sxi32 rc;` |
|       17 |  3749 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3750 | `	char zNameBuf[64];` |
|       17 |  3751 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       17 |  3752 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       17 |  3753 | `	if( pClass == 0 ){` |
|      ! 0 |  3754 | `		return PH7_ABORT;` |
|        - |  3755 | `	}` |
|       17 |  3756 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       17 |  3757 | `	if( pThis == 0 ){` |
|      ! 0 |  3758 | `		return PH7_ABORT;` |
|        - |  3759 | `	}` |
|       17 |  3760 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 |  3761 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       17 |  3762 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       17 |  3763 | `	if( pCons ){` |
|       17 |  3764 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       17 |  3765 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       17 |  3766 | `		apArg[0] = &sArg;` |
|       17 |  3767 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       17 |  3768 | `		PH7_MemObjRelease(&sArg);` |
|        8 |  3769 | `	}` |
|       17 |  3770 | `	SyBlobRelease(&sMsg);` |
|       17 |  3771 | `	pFrame = pVm->pFrame;` |
|       17 |  3772 | `	if( pFrame ){` |
|       17 |  3773 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       17 |  3774 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        8 |  3775 | `	}` |
|       17 |  3776 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       17 |  3777 | `	PH7_ClassInstanceUnref(pThis);` |
|       17 |  3778 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3779 | `		return PH7_ABORT;` |
|        - |  3780 | `	}` |
|       17 |  3781 | `	return PH7_EXCEPTION;` |
|        9 |  3782 |  |
|        - |  3783 | `/*` |
|        - |  3784 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3785 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3786 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3787 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3788 | ` */` |
|        - |  3789 | `/*` |
|        - |  3790 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3791 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3792 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3793 | ` */` |
|       24 |  3794 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3795 |  |
|        - |  3796 | `	sxu32 nCopy;` |
|       26 |  3797 | `	if( nBuf == 0 ) return "";` |
|       26 |  3798 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3799 | `		zBuf[0] = 0;` |
|      ! 0 |  3800 | `		return zBuf;` |
|        - |  3801 | `	}` |
|       26 |  3802 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3803 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3804 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3805 | `	zBuf[nCopy] = 0;` |
|       26 |  3806 | `	return zBuf;` |
|       14 |  3807 |  |
|        - |  3808 |  |
|      396 |  3809 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3810 |  |
|      398 |  3811 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3812 | `	const char *zGiven;` |
|        - |  3813 | `	char zBuf[128];` |
|        - |  3814 | `	char zTypeBuf[128];` |
|        - |  3815 | `	/* Untyped function: no enforcement. */` |
|      398 |  3816 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3817 | `		return SXRET_OK;` |
|        - |  3818 | `	}` |
|        - |  3819 | `	/* void return type: the function must not produce a value. */` |
|      398 |  3820 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      136 |  3821 | `		if( pValue == 0 ){` |
|      134 |  3822 | `			return SXRET_OK;` |
|        - |  3823 | `		}` |
|        - |  3824 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3825 | `		 * still counts as "returned a value" here. */` |
|        3 |  3826 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3827 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3828 | `	}` |
|        - |  3829 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3830 | ``	 * returns null. For a typed non-nullable return (including `mixed`, which`` |
|        - |  3831 | `	 * requires an explicit returned value), that's a TypeError. */` |
|      264 |  3832 | `	if( pValue == 0 ){` |
|      ! 0 |  3833 | `		const char *zExpected = "value";` |
|      ! 0 |  3834 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3835 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3836 | `		}` |
|      ! 0 |  3837 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3838 | `	}` |
|        - |  3839 | ``	/* `mixed` accepts any explicitly returned value, including null. It is`` |
|        - |  3840 | `	 * parsed as a class-name atom (SXU32_HIGH, sReturnClass = "mixed") since` |
|        - |  3841 | `	 * it is not a scalar keyword, so short-circuit it here before the null /` |
|        - |  3842 | `	 * class-type checks below — which would otherwise demand an object. */` |
|      272 |  3843 | `	if( pFunc->nReturnType == SXU32_HIGH` |
|      143 |  3844 | `	 && pFunc->sReturnClass.nByte == 5` |
|       24 |  3845 | `	 && SyStrnicmp(pFunc->sReturnClass.zString,"mixed",5) == 0 ){` |
|       21 |  3846 | `		return SXRET_OK;` |
|        - |  3847 | `	}` |
|        - |  3848 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3849 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3850 | `	 * bNullable=0 here. */` |
|      244 |  3851 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3852 | `		sxi32 rcU;` |
|      ! 0 |  3853 | `		int bNullable = 0;` |
|      ! 0 |  3854 | `		const char *zExpected = "union";` |
|        - |  3855 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3856 | `		{` |
|        - |  3857 | `			sxu32 i;` |
|      ! 0 |  3858 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3859 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3860 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3861 | `			}` |
|        - |  3862 | `		}` |
|      ! 0 |  3863 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3864 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3865 | `			return SXRET_OK;` |
|        - |  3866 | `		}` |
|      ! 0 |  3867 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3868 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3869 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3870 | `			zGiven = "null";` |
|      ! 0 |  3871 | `		}else{` |
|      ! 0 |  3872 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3873 | `		}` |
|      ! 0 |  3874 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3875 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3876 | `		}` |
|      ! 0 |  3877 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3878 | `	}` |
|        - |  3879 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3880 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3881 | `	 * it into the TypeError message. */` |
|      244 |  3882 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3883 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3884 | `		const char *zExpected;` |
|        - |  3885 | `		ph7_class *pExpected;` |
|        6 |  3886 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3887 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3888 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3889 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3890 | `		}` |
|        6 |  3891 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3892 | `			pExpected = pSelfNow;` |
|        4 |  3893 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3894 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3895 | `		}else{` |
|        3 |  3896 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3897 | `		}` |
|        6 |  3898 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3899 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3900 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3901 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3902 | `		}` |
|        6 |  3903 | `		if( pExpected ){` |
|        6 |  3904 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3905 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3906 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3907 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3908 | `			}` |
|        2 |  3909 | `		}` |
|        6 |  3910 | `		return SXRET_OK;` |
|        - |  3911 | `	}` |
|        - |  3912 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3913 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3914 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3915 | `	 * via the type-text leading '?'. */` |
|      240 |  3916 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3917 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3918 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3919 | `			return SXRET_OK;` |
|        - |  3920 | `		}` |
|      ! 0 |  3921 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3922 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3923 | `			"null");` |
|        - |  3924 | `	}` |
|        - |  3925 | `	/* Exact match? Done. */` |
|      234 |  3926 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      228 |  3927 | `		return SXRET_OK;` |
|        - |  3928 | `	}` |
|        - |  3929 | `	/* Object->scalar is never compatible. */` |
|        8 |  3930 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3931 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3932 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3933 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3934 | `			zGiven);` |
|        - |  3935 | `	}` |
|        - |  3936 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3937 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3938 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3939 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3940 | `			ph7_type_name(pValue));` |
|        - |  3941 | `	}` |
|        - |  3942 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3943 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3944 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3945 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3946 | `	if( !bStrict` |
|        5 |  3947 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3948 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3949 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3950 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3951 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3952 | `			"string");` |
|        - |  3953 | `	}` |
|        6 |  3954 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3955 | `		return SXRET_OK;` |
|        - |  3956 | `	}` |
|        4 |  3957 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3958 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3959 | `		ph7_type_name(pValue));` |
|      200 |  3960 |  |
|        - |  3961 | `/*` |
|        - |  3962 | ` * Report a fatal named-argument error.` |
|        - |  3963 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3964 | ` */` |
|        6 |  3965 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3966 |  |
|        7 |  3967 | `	const char *zFunc = 0;` |
|        7 |  3968 | `	int nFunc = 0;` |
|        7 |  3969 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3970 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3971 |  |
|        - |  3972 | `/*` |
|        - |  3973 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3974 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3975 | ` * information.` |
|        - |  3976 | ` * ------------------------------------` |
|        - |  3977 | ` * Simple boring wrapper function.` |
|        - |  3978 | ` * ------------------------------------` |
|        - |  3979 | ` */` |
|       24 |  3980 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3981 |  |
|        - |  3982 | `	sxi32 rc;` |
|       26 |  3983 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3984 | `	return rc;` |
|        2 |  3985 |  |
|        - |  3986 | `/*` |
|        - |  3987 | ` * Resolve function context from the current frame.` |
|        - |  3988 | ` */` |
|     1018 |  3989 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3990 |  |
|        - |  3991 | `	VmFrame *pFrame;` |
|        - |  3992 | `	ph7_vm_func *pFunc;` |
|     1019 |  3993 | `	*pzFuncName = 0;` |
|     1019 |  3994 | `	*pnFuncLen = 0;` |
|     1019 |  3995 | `	pFrame = pVm->pFrame;` |
|     1019 |  3996 | `	if( pFrame == 0 ){` |
|      ! 0 |  3997 | `		return;` |
|        - |  3998 | `	}` |
|     1019 |  3999 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1019 |  4000 | `	if( pFrame->pParent == 0 ){` |
|      995 |  4001 | `		return;` |
|        - |  4002 | `	}` |
|       25 |  4003 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  4004 | `	if( pFunc == 0 ){` |
|      ! 0 |  4005 | `		return;` |
|        - |  4006 | `	}` |
|       25 |  4007 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  4008 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      510 |  4009 |  |
|        - |  4010 | `/*` |
|        - |  4011 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  4012 | ` */` |
|      524 |  4013 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  4014 |  |
|        - |  4015 | `	SyBlob sOut;` |
|        - |  4016 | `	SyString *pFile;` |
|      525 |  4017 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  4018 | `		return PH7_OK;` |
|        - |  4019 | `	}` |
|      525 |  4020 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  4021 | `		zClass = "Exception";` |
|      ! 0 |  4022 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  4023 | `	}` |
|      525 |  4024 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      503 |  4025 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      251 |  4026 | `	}` |
|      525 |  4027 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      525 |  4028 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      525 |  4029 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      525 |  4030 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      525 |  4031 | `	if( zMsg && nMsg > 0 ){` |
|      525 |  4032 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      525 |  4033 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      262 |  4034 | `	}` |
|      525 |  4035 | `	if( pFile ){` |
|      525 |  4036 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      525 |  4037 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  4038 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      262 |  4039 | `	}` |
|      525 |  4040 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      525 |  4041 | `	if( pFile ){` |
|      525 |  4042 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      525 |  4043 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  4044 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  4045 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  4046 | `		}else{` |
|      501 |  4047 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  4048 | `		}` |
|      262 |  4049 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  4050 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  4051 | `	}else{` |
|      ! 0 |  4052 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  4053 | `	}` |
|      525 |  4054 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      525 |  4055 | `	if( pFile ){` |
|      525 |  4056 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      525 |  4057 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      525 |  4058 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  4059 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      262 |  4060 | `	}` |
|      525 |  4061 | `	VmCallErrorHandler(pVm,&sOut);` |
|      525 |  4062 | `	SyBlobRelease(&sOut);` |
|      525 |  4063 | `	return PH7_ABORT;` |
|      263 |  4064 |  |
|        - |  4065 | `/*` |
|        - |  4066 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  4067 | ` *` |
|        - |  4068 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  4069 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  4070 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  4071 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  4072 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  4073 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  4074 | ` */` |
|      870 |  4075 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        2 |  4076 |  |
|      872 |  4077 | `	if( pVm->bCoalesceArmed ){` |
|        7 |  4078 | `		if( pVm->pCoalesceObj ){` |
|        7 |  4079 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  4080 | `		}` |
|        7 |  4081 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        7 |  4082 | `		pVm->pCoalesceObj = 0;` |
|        7 |  4083 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  4084 | `	}` |
|      872 |  4085 |  |
|        - |  4086 | `/*` |
|        - |  4087 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  4088 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  4089 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  4090 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  4091 | ` *` |
|        - |  4092 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  4093 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  4094 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  4095 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  4096 | ` */` |
|        4 |  4097 | `static sxi32 VmThrowFromVm(` |
|        - |  4098 | `	ph7_vm *pVm,` |
|        - |  4099 | `	const char *zClass,` |
|        - |  4100 | `	const char *zMsg,` |
|        - |  4101 | `	sxu32 nMsg` |
|        1 |  4102 | `){` |
|        - |  4103 | `	ph7_class *pClass;` |
|        - |  4104 | `	ph7_class_instance *pThis;` |
|        - |  4105 | `	ph7_class_method *pCons;` |
|        - |  4106 | `	VmFrame *pFrame;` |
|        - |  4107 | `	sxi32 rc;` |
|        5 |  4108 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        5 |  4109 | `	if( pClass == 0 ){` |
|      ! 0 |  4110 | `		return SXERR_ABORT;` |
|        - |  4111 | `	}` |
|        5 |  4112 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        5 |  4113 | `	if( pThis == 0 ){` |
|      ! 0 |  4114 | `		return SXERR_ABORT;` |
|        - |  4115 | `	}` |
|        5 |  4116 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        5 |  4117 | `	if( pCons ){` |
|        - |  4118 | `		ph7_value sArg;` |
|        - |  4119 | `		ph7_value *apArg[1];` |
|        - |  4120 | `		SyString sMsgStr;` |
|        5 |  4121 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        5 |  4122 | `		PH7_MemObjInit(pVm,&sArg);` |
|        5 |  4123 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        5 |  4124 | `		apArg[0] = &sArg;` |
|        5 |  4125 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        5 |  4126 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  4127 | `	}` |
|        5 |  4128 | `	pFrame = pVm->pFrame;` |
|        5 |  4129 | `	if( pFrame ){` |
|        5 |  4130 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  4131 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  4132 | `	}` |
|        5 |  4133 | `	rc = VmThrowException(pVm,pThis);` |
|        5 |  4134 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  4135 | `	return rc;` |
|        3 |  4136 |  |
|        - |  4137 | `/*` |
|        - |  4138 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  4139 | ` */` |
|      574 |  4140 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  4141 |  |
|        - |  4142 | `	ph7_vm *pVm;` |
|        - |  4143 | `	ph7_class *pClass;` |
|        - |  4144 | `	ph7_class_instance *pThis;` |
|        - |  4145 | `	ph7_class_method *pCons;` |
|        - |  4146 | `	ph7_value sArg;` |
|        - |  4147 | `	ph7_value *apArg[1];` |
|        - |  4148 | `	SyBlob sMsg;` |
|        - |  4149 | `	SyString sMsgStr;` |
|        - |  4150 | `	VmFrame *pFrame;` |
|        - |  4151 | `	va_list ap;` |
|        - |  4152 | `	sxi32 rc;` |
|        - |  4153 |  |
|      576 |  4154 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4155 | `		return PH7_ABORT;` |
|        - |  4156 | `	}` |
|      576 |  4157 | `	pVm = pCtx->pVm;` |
|      576 |  4158 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4159 | `		zClass = "Error";` |
|      ! 0 |  4160 | `	}` |
|      576 |  4161 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      576 |  4162 | `	if( pClass == 0 ){` |
|      ! 0 |  4163 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4164 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  4165 | `			zClass` |
|        - |  4166 | `			);` |
|        - |  4167 | `	}` |
|      576 |  4168 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      576 |  4169 | `	if( pThis == 0 ){` |
|      ! 0 |  4170 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4171 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  4172 | `			);` |
|        - |  4173 | `	}` |
|        - |  4174 |  |
|      576 |  4175 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      576 |  4176 | `	va_start(ap,zFormat);` |
|      576 |  4177 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      576 |  4178 | `	va_end(ap);` |
|        - |  4179 |  |
|      576 |  4180 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      576 |  4181 | `	if( pCons ){` |
|      576 |  4182 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      576 |  4183 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      576 |  4184 | `		apArg[0] = &sArg;` |
|      576 |  4185 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      576 |  4186 | `		PH7_MemObjRelease(&sArg);` |
|      287 |  4187 | `	}` |
|      576 |  4188 | `	SyBlobRelease(&sMsg);` |
|        - |  4189 |  |
|      576 |  4190 | `	pFrame = pVm->pFrame;` |
|      576 |  4191 | `	if( pFrame ){` |
|      576 |  4192 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      576 |  4193 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      287 |  4194 | `	}` |
|      576 |  4195 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      576 |  4196 | `	PH7_ClassInstanceUnref(pThis);` |
|      576 |  4197 | `	if( rc == SXERR_ABORT ){` |
|      491 |  4198 | `		return PH7_ABORT;` |
|        - |  4199 | `	}` |
|       86 |  4200 | `	return PH7_EXCEPTION;` |
|      289 |  4201 |  |
|        - |  4202 | `/*` |
|        - |  4203 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  4204 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  4205 | ` */` |
|      ! 0 |  4206 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  4207 |  |
|        - |  4208 | `	ph7_vm *pVm;` |
|        - |  4209 | `	SyBlob sMsg;` |
|      ! 0 |  4210 | `	const char *zFuncName = 0;` |
|      ! 0 |  4211 | `	int nFuncLen = 0;` |
|        - |  4212 | `	va_list ap;` |
|        - |  4213 | `	sxi32 rc;` |
|        - |  4214 |  |
|      ! 0 |  4215 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4216 | `		return PH7_OK;` |
|        - |  4217 | `	}` |
|      ! 0 |  4218 | `	pVm = pCtx->pVm;` |
|      ! 0 |  4219 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4220 | `		zClass = "Error";` |
|      ! 0 |  4221 | `	}` |
|        - |  4222 |  |
|      ! 0 |  4223 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  4224 |  |
|      ! 0 |  4225 | `	va_start(ap,zFormat);` |
|      ! 0 |  4226 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  4227 | `	va_end(ap);` |
|        - |  4228 |  |
|      ! 0 |  4229 | `	if( pCtx->pFunc ){` |
|      ! 0 |  4230 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  4231 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  4232 | `	}` |
|      ! 0 |  4233 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  4234 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  4235 | `	}` |
|      ! 0 |  4236 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  4237 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  4238 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  4239 | `	return rc;` |
|      ! 0 |  4240 |  |
|        - |  4241 | `/*` |
|        - |  4242 | ` * Save the execution state of a fiber/generator context.` |
|        - |  4243 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  4244 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  4245 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  4246 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  4247 | ` * when VmByteCodeExec returns.` |
|        - |  4248 | ` */` |
|      144 |  4249 | `static sxi32 VmSuspendCtx(` |
|        - |  4250 | `	ph7_vm *pVm,` |
|        - |  4251 | `	ph7_exec_ctx *pCtx,` |
|        - |  4252 | `	sxi32 pc,` |
|        - |  4253 | `	sxi32 nTos` |
|        - |  4254 | `	)` |
|        2 |  4255 |  |
|       72 |  4256 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  4257 | `	pCtx->pc = pc;` |
|      146 |  4258 | `	pCtx->nTos = nTos;` |
|      146 |  4259 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  4260 | `	return PH7_SUSPEND;` |
|        2 |  4261 |  |
|        - |  4262 | `/*` |
|        - |  4263 | ` * Resolve named-argument mapping.` |
|        - |  4264 | ` *` |
|        - |  4265 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  4266 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  4267 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  4268 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  4269 | ` * every formal parameter that received a value.` |
|        - |  4270 | ` *` |
|        - |  4271 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  4272 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  4273 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  4274 | ` */` |
|       98 |  4275 | `static sxi32 VmResolveNamedArgs(` |
|        - |  4276 | `	ph7_vm *pVm,` |
|        - |  4277 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  4278 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  4279 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  4280 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  4281 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  4282 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  4283 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  4284 |  |
|        2 |  4285 |  |
|      100 |  4286 | `	sxi32 posIdx = 0;` |
|        - |  4287 | `	sxu32 i;` |
|        - |  4288 | `	char zErrMsg[256];` |
|      100 |  4289 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  4290 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  4291 | `		aSlot[i] = -2;` |
|      100 |  4292 | `	}` |
|      290 |  4293 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  4294 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  4295 | `			/* Named argument — find formal by name */` |
|      184 |  4296 | `			int found = 0;` |
|        - |  4297 | `			sxu32 k;` |
|      304 |  4298 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  4299 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  4300 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  4301 | `						pMap->aNames[i].zString,` |
|      402 |  4302 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  4303 | `					if( aUsed[k] ){` |
|        7 |  4304 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4305 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  4306 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  4307 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  4308 | `						return PH7_ABORT;` |
|        - |  4309 | `					}` |
|      168 |  4310 | `					aSlot[i] = (sxi32)k;` |
|      168 |  4311 | `					aUsed[k] = 1;` |
|      168 |  4312 | `					found = 1;` |
|      168 |  4313 | `					break;` |
|        - |  4314 | `				}` |
|       62 |  4315 | `			}` |
|      180 |  4316 | `			if( !found ){` |
|       14 |  4317 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  4318 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  4319 | `				}else{` |
|        4 |  4320 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4321 | `						"Unknown named parameter $%.*s",` |
|        2 |  4322 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  4323 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  4324 | `					return PH7_ABORT;` |
|        - |  4325 | `				}` |
|        5 |  4326 | `			}` |
|       90 |  4327 | `		}else{` |
|        - |  4328 | `			/* Positional argument */` |
|       16 |  4329 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  4330 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  4331 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4332 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  4333 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  4334 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  4335 | `					return PH7_ABORT;` |
|        - |  4336 | `				}` |
|       16 |  4337 | `				aSlot[i] = posIdx;` |
|       16 |  4338 | `				aUsed[posIdx] = 1;` |
|        7 |  4339 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  4340 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  4341 | `			}` |
|       16 |  4342 | `			posIdx++;` |
|        - |  4343 | `		}` |
|       97 |  4344 | `	}` |
|       93 |  4345 | `	return SXRET_OK;` |
|       51 |  4346 |  |
|        - |  4347 | `/*` |
|        - |  4348 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  4349 | ` *` |
|        - |  4350 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  4351 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  4352 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  4353 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  4354 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  4355 | ` * then the program execution is halted.` |
|        - |  4356 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  4357 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  4358 | ` * or to reset the VM to it's initial state.` |
|        - |  4359 | ` */` |
|    45320 |  4360 | `static sxi32 VmByteCodeExec(` |
|        - |  4361 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  4362 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  4363 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  4364 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  4365 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  4366 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  4367 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  4368 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  4369 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  4370 | `	)` |
|        2 |  4371 |  |
|        - |  4372 | `	VmInstr *pInstr;` |
|        - |  4373 | `	ph7_value *pTos;` |
|        - |  4374 | `	SySet aArg;` |
|        - |  4375 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  4376 | `	sxi32 pc;` |
|        - |  4377 | `	sxi32 rc;` |
|        - |  4378 | `	/* Argument container */` |
|    45322 |  4379 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    45322 |  4380 | `	if( nTos < 0 ){` |
|    42144 |  4381 | `		pTos = &pStack[-1];` |
|    21073 |  4382 | `	}else{` |
|     3180 |  4383 | `		pTos = &pStack[nTos];` |
|        - |  4384 | `	}` |
|    45322 |  4385 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    45322 |  4386 | `	pc = nPc;` |
|        - |  4387 | `/*` |
|        - |  4388 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  4389 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  4390 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  4391 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  4392 | ` */` |
|        - |  4393 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  4394 | `	{ \` |
|        - |  4395 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  4396 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  4397 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  4398 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  4399 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  4400 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  4401 | `				break; \` |
|        - |  4402 | `			} \` |
|        - |  4403 | `			goto Exception; \` |
|        - |  4404 | `		} \` |
|        - |  4405 | `	}` |
|        - |  4406 | `	/* Execute as much as we can */` |
|  5916238 |  4407 | `	for(;;){` |
|        - |  4408 | `		/* Fetch the instruction to execute */` |
| 11831774 |  4409 | `		pInstr = &aInstr[pc];` |
| 11831774 |  4410 | `		rc = SXRET_OK;` |
|        - |  4411 | `/*` |
|        - |  4412 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4413 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4414 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4415 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4416 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4417 | ` */` |
| 11831774 |  4418 | `		switch(pInstr->iOp){` |
|        - |  4419 | `/*` |
|        - |  4420 | ` * DONE: P1 * *` |
|        - |  4421 | ` *` |
|        - |  4422 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4423 | ` * and return immediately.` |
|        - |  4424 | ` */` |
|    22213 |  4425 | `case PH7_OP_DONE:` |
|        - |  4426 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4427 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4428 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4429 | `	 * callback trampolines, and the main script. */` |
|    44426 |  4430 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0` |
|      402 |  4431 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  4432 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  4433 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  4434 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  4435 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  4436 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  4437 | `		 * exception. */` |
|      398 |  4438 | `		ph7_value *pRetVal = 0;` |
|      398 |  4439 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      266 |  4440 | `			pRetVal = pTos;` |
|      132 |  4441 | `		}` |
|      398 |  4442 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      398 |  4443 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      392 |  4444 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  4445 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  4446 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4447 | `				pTos--;` |
|      ! 0 |  4448 | `			}` |
|      ! 0 |  4449 | `			goto Exception;` |
|        - |  4450 | `		}` |
|        - |  4451 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4452 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4453 | `		 * defensively we clear the pointer after a successful check). */` |
|      392 |  4454 | `		pEnforceRetFunc = 0;` |
|      195 |  4455 | `	}` |
|    44422 |  4456 | `	if( pInstr->iP1 ){` |
|        - |  4457 | `#ifdef UNTRUST` |
|        - |  4458 | `		if( pTos < pStack ){` |
|        - |  4459 | `			goto Abort;` |
|        - |  4460 | `		}` |
|        - |  4461 | `#endif` |
|    27004 |  4462 | `		if( pLastRef ){` |
|    16438 |  4463 | `			*pLastRef = pTos->nIdx;` |
|     8218 |  4464 | `		}` |
|    27004 |  4465 | `		if( pResult ){` |
|        - |  4466 | `			/* Execution result */` |
|    25500 |  4467 | `			PH7_MemObjStore(pTos,pResult);` |
|    12749 |  4468 | `		}` |
|    27004 |  4469 | `		VmPopOperand(&pTos,1);` |
|    30921 |  4470 | `	}else if( pLastRef ){` |
|        - |  4471 | `		/* Nothing referenced */` |
|     1992 |  4472 | `		*pLastRef = SXU32_HIGH;` |
|      995 |  4473 | `	}` |
|        - |  4474 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4475 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4476 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4477 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4478 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4479 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4480 | `	 * block can override it.` |
|        - |  4481 | `	 */` |
|    44424 |  4482 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  4483 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  4484 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  4485 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  4486 | `		pExc->pFrame = 0;` |
|        3 |  4487 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  4488 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  4489 | `			pExc->iFinallyDone = 1;` |
|        - |  4490 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  4491 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  4492 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4493 | `				goto Abort;` |
|        - |  4494 | `			}` |
|        1 |  4495 | `		}` |
|        1 |  4496 | `	}` |
|    44422 |  4497 | `	goto Done;` |
|        - |  4498 | `/*` |
|        - |  4499 | ` * HALT: P1 * *` |
|        - |  4500 | ` *` |
|        - |  4501 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4502 | ` * and abort immediately.` |
|        - |  4503 | ` */` |
|        7 |  4504 | `case PH7_OP_HALT:` |
|       15 |  4505 | `	if( pInstr->iP1 ){` |
|        - |  4506 | `#ifdef UNTRUST` |
|        - |  4507 | `		if( pTos < pStack ){` |
|        - |  4508 | `			goto Abort;` |
|        - |  4509 | `		}` |
|        - |  4510 | `#endif` |
|       15 |  4511 | `		if( pLastRef ){` |
|        3 |  4512 | `			*pLastRef = pTos->nIdx;` |
|        1 |  4513 | `		}` |
|       15 |  4514 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       11 |  4515 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4516 | `				/* Output the exit message */` |
|       16 |  4517 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        5 |  4518 | `					pVm->sVmConsumer.pUserData);` |
|       11 |  4519 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        6 |  4520 | `			}` |
|       10 |  4521 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4522 | `			/* Record exit status */` |
|        5 |  4523 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4524 | `		}` |
|       15 |  4525 | `		VmPopOperand(&pTos,1);` |
|        7 |  4526 | `	}else if( pLastRef ){` |
|        - |  4527 | `		/* Nothing referenced */` |
|      ! 0 |  4528 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4529 | `	}` |
|        - |  4530 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  4531 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  4532 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  4533 | `	 */` |
|       15 |  4534 | `	pVm->bHaltRequested = 1;` |
|       15 |  4535 | `	goto Abort;` |
|        - |  4536 | `/*` |
|        - |  4537 | ` * JMP: * P2 *` |
|        - |  4538 | ` *` |
|        - |  4539 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4540 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4541 | ` */` |
|   252058 |  4542 | `case PH7_OP_JMP:` |
|   504162 |  4543 | `	pc = pInstr->iP2 - 1;` |
|   504162 |  4544 | `	break;` |
|        - |  4545 | `/*` |
|        - |  4546 | ` * JZ: P1 P2 *` |
|        - |  4547 | ` *` |
|        - |  4548 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4549 | ` * entry in the stack if P1 is zero.` |
|        - |  4550 | ` */` |
|   598496 |  4551 | `case PH7_OP_JZ:` |
|        - |  4552 | `#ifdef UNTRUST` |
|        - |  4553 | `	if( pTos < pStack ){` |
|        - |  4554 | `		goto Abort;` |
|        - |  4555 | `	}` |
|        - |  4556 | `#endif` |
|        - |  4557 | `	/* Get a boolean value */` |
|  1197082 |  4558 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      174 |  4559 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4560 | `	}` |
|  1197082 |  4561 | `	if( !pTos->x.iVal ){` |
|        - |  4562 | `		/* Take the jump */` |
|   615846 |  4563 | `		pc = pInstr->iP2 - 1;` |
|   307922 |  4564 | `	}` |
|  1197082 |  4565 | `	if( !pInstr->iP1 ){` |
|   948312 |  4566 | `		VmPopOperand(&pTos,1);` |
|   474177 |  4567 | `	}` |
|  1197082 |  4568 | `	break;` |
|        - |  4569 | `/*` |
|        - |  4570 | ` * JNZ: P1 P2 *` |
|        - |  4571 | ` *` |
|        - |  4572 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4573 | ` * entry in the stack if P1 is zero.` |
|        - |  4574 | ` */` |
|    61415 |  4575 | `case PH7_OP_JNZ:` |
|        - |  4576 | `#ifdef UNTRUST` |
|        - |  4577 | `	if( pTos < pStack ){` |
|        - |  4578 | `		goto Abort;` |
|        - |  4579 | `	}` |
|        - |  4580 | `#endif` |
|        - |  4581 | `	/* Get a boolean value */` |
|   122832 |  4582 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4583 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4584 | `	}` |
|   122832 |  4585 | `	if( pTos->x.iVal ){` |
|        - |  4586 | `		/* Take the jump */` |
|     5606 |  4587 | `		pc = pInstr->iP2 - 1;` |
|     2802 |  4588 | `	}` |
|   122832 |  4589 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4590 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4591 | `	}` |
|   122832 |  4592 | `	break;` |
|        - |  4593 | `/*` |
|        - |  4594 | ` * NOOP: * * *` |
|        - |  4595 | ` *` |
|        - |  4596 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4597 | ` * destination.` |
|        - |  4598 | ` */` |
|      ! 0 |  4599 | `case PH7_OP_NOOP:` |
|      ! 0 |  4600 | `	break;` |
|        - |  4601 | `/*` |
|        - |  4602 | ` * POP: P1 * *` |
|        - |  4603 | ` *` |
|        - |  4604 | ` * Pop P1 elements from the operand stack.` |
|        - |  4605 | ` */` |
|   463815 |  4606 | `case PH7_OP_POP: {` |
|   927676 |  4607 | `	sxi32 n = pInstr->iP1;` |
|   927676 |  4608 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4609 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       50 |  4610 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4611 | `	}` |
|   927676 |  4612 | `	VmPopOperand(&pTos,n);` |
|   927676 |  4613 | `	break;` |
|        - |  4614 | `				 }` |
|        - |  4615 | `/*` |
|        - |  4616 | ` * DUP: * * *` |
|        - |  4617 | ` *` |
|        - |  4618 | ` * Duplicate the top of the stack.` |
|        - |  4619 | ` */` |
|       41 |  4620 | `case PH7_OP_DUP:` |
|        - |  4621 | `#ifdef UNTRUST` |
|        - |  4622 | `	if( pTos < pStack ){` |
|        - |  4623 | `		goto Abort;` |
|        - |  4624 | `	}` |
|        - |  4625 | `#endif` |
|       84 |  4626 | `	pTos++;` |
|       84 |  4627 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4628 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4629 | `	break;` |
|        - |  4630 | `/*` |
|        - |  4631 | ` * NSSWITCH: * * P3` |
|        - |  4632 | ` *` |
|        - |  4633 | ` * Switch the active namespace at runtime.` |
|        - |  4634 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4635 | ` */` |
|     7859 |  4636 | `case PH7_OP_NSSWITCH:` |
|    15720 |  4637 | `	SyBlobReset(&pVm->sNamespace);` |
|    15720 |  4638 | `	if( pInstr->p3 ){` |
|      100 |  4639 | `		const char *zNs = (const char *)pInstr->p3;` |
|      100 |  4640 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       49 |  4641 | `	}` |
|        - |  4642 | `	/* Clear namespace-scoped use-const imports */` |
|    15720 |  4643 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15720 |  4644 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15720 |  4645 | `	break;` |
|        - |  4646 | `/* OP_USECONST P1 * P3` |
|        - |  4647 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4648 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4649 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4650 | ` */` |
|        7 |  4651 | `case PH7_OP_USECONST: {` |
|       16 |  4652 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4653 | `	if( azPair ){` |
|       16 |  4654 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4655 | `	}` |
|       16 |  4656 | `	break;` |
|        - |  4657 | `				}` |
|        - |  4658 | `/*` |
|        - |  4659 | ` * CVT_INT: * * *` |
|        - |  4660 | ` *` |
|        - |  4661 | ` * Force the top of the stack to be an integer.` |
|        - |  4662 | ` */` |
|       80 |  4663 | `case PH7_OP_CVT_INT:` |
|        - |  4664 | `#ifdef UNTRUST` |
|        - |  4665 | `	if( pTos < pStack ){` |
|        - |  4666 | `		goto Abort;` |
|        - |  4667 | `	}` |
|        - |  4668 | `#endif` |
|      162 |  4669 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4670 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4671 | `	}` |
|        - |  4672 | `	/* Invalidate any prior representation */` |
|      162 |  4673 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      162 |  4674 | `	break;` |
|        - |  4675 | `/*` |
|        - |  4676 | ` * CVT_REAL: * * *` |
|        - |  4677 | ` *` |
|        - |  4678 | ` * Force the top of the stack to be a real.` |
|        - |  4679 | ` */` |
|        7 |  4680 | `case PH7_OP_CVT_REAL:` |
|        - |  4681 | `#ifdef UNTRUST` |
|        - |  4682 | `	if( pTos < pStack ){` |
|        - |  4683 | `		goto Abort;` |
|        - |  4684 | `	}` |
|        - |  4685 | `#endif` |
|       15 |  4686 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 |  4687 | `		PH7_MemObjToReal(pTos);` |
|        5 |  4688 | `	}` |
|        - |  4689 | `	/* Invalidate any prior representation */` |
|       15 |  4690 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       15 |  4691 | `	break;` |
|        - |  4692 | `/*` |
|        - |  4693 | ` * CVT_STR: * * *` |
|        - |  4694 | ` *` |
|        - |  4695 | ` * Force the top of the stack to be a string.` |
|        - |  4696 | ` */` |
|      163 |  4697 | `case PH7_OP_CVT_STR:` |
|        - |  4698 | `#ifdef UNTRUST` |
|        - |  4699 | `	if( pTos < pStack ){` |
|        - |  4700 | `		goto Abort;` |
|        - |  4701 | `	}` |
|        - |  4702 | `#endif` |
|      328 |  4703 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      308 |  4704 | `		PH7_MemObjToString(pTos);` |
|      153 |  4705 | `	}` |
|      328 |  4706 | `	break;` |
|        - |  4707 | `/*` |
|        - |  4708 | ` * CVT_BOOL: * * *` |
|        - |  4709 | ` *` |
|        - |  4710 | ` * Force the top of the stack to be a boolean.` |
|        - |  4711 | ` */` |
|        5 |  4712 | `case PH7_OP_CVT_BOOL:` |
|        - |  4713 | `#ifdef UNTRUST` |
|        - |  4714 | `	if( pTos < pStack ){` |
|        - |  4715 | `		goto Abort;` |
|        - |  4716 | `	}` |
|        - |  4717 | `#endif` |
|       11 |  4718 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4719 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4720 | `	}` |
|       11 |  4721 | `	break;` |
|        - |  4722 | `/*` |
|        - |  4723 | ` * CVT_NULL: * * *` |
|        - |  4724 | ` *` |
|        - |  4725 | ` * Nullify the top of the stack.` |
|        - |  4726 | ` */` |
|        3 |  4727 | `case PH7_OP_CVT_NULL:` |
|        - |  4728 | `#ifdef UNTRUST` |
|        - |  4729 | `	if( pTos < pStack ){` |
|        - |  4730 | `		goto Abort;` |
|        - |  4731 | `	}` |
|        - |  4732 | `#endif` |
|        7 |  4733 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4734 | `	break;` |
|        - |  4735 | `/*` |
|        - |  4736 | ` * CVT_NUMC: * * *` |
|        - |  4737 | ` *` |
|        - |  4738 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4739 | ` */` |
|      ! 0 |  4740 | `case PH7_OP_CVT_NUMC:` |
|        - |  4741 | `#ifdef UNTRUST` |
|        - |  4742 | `	if( pTos < pStack ){` |
|        - |  4743 | `		goto Abort;` |
|        - |  4744 | `	}` |
|        - |  4745 | `#endif` |
|        - |  4746 | `	/* Force a numeric cast */` |
|      ! 0 |  4747 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4748 | `	break;` |
|        - |  4749 | `/*` |
|        - |  4750 | ` * CVT_ARRAY: * * *` |
|        - |  4751 | ` *` |
|        - |  4752 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4753 | ` */` |
|       10 |  4754 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4755 | `#ifdef UNTRUST` |
|        - |  4756 | `	if( pTos < pStack ){` |
|        - |  4757 | `		goto Abort;` |
|        - |  4758 | `	}` |
|        - |  4759 | `#endif` |
|        - |  4760 | `	/* Force a hashmap cast */` |
|       21 |  4761 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4762 | `	if( rc != SXRET_OK ){` |
|        - |  4763 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4764 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4765 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4766 | `	}` |
|       21 |  4767 | `	break;` |
|        - |  4768 | `/*` |
|        - |  4769 | ` * CVT_OBJ: * * *` |
|        - |  4770 | ` *` |
|        - |  4771 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4772 | ` */` |
|        8 |  4773 | `case PH7_OP_CVT_OBJ:` |
|        - |  4774 | `#ifdef UNTRUST` |
|        - |  4775 | `	if( pTos < pStack ){` |
|        - |  4776 | `		goto Abort;` |
|        - |  4777 | `	}` |
|        - |  4778 | `#endif` |
|       17 |  4779 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4780 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4781 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4782 | `	}` |
|       17 |  4783 | `	break;` |
|        - |  4784 | `/*` |
|        - |  4785 | ` * ERR_CTRL * * *` |
|        - |  4786 | ` *` |
|        - |  4787 | ` * Error control operator.` |
|        - |  4788 | ` */` |
|    16089 |  4789 | `case PH7_OP_ERR_CTRL:` |
|        - |  4790 | `	/*` |
|        - |  4791 | `	 * TICKET 1433-038:` |
|        - |  4792 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4793 | `	 * use the public API,to control error output.` |
|        - |  4794 | `	 */` |
|    32178 |  4795 | `	break;` |
|        - |  4796 | `/*` |
|        - |  4797 | ` * IS_A * * *` |
|        - |  4798 | ` *` |
|        - |  4799 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4800 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4801 | ` * holding a class name or an object).` |
|        - |  4802 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4803 | ` */` |
|       75 |  4804 | `case PH7_OP_IS_A:{` |
|      152 |  4805 | `	ph7_value *pNos = &pTos[-1];` |
|      152 |  4806 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4807 | `#ifdef UNTRUST` |
|        - |  4808 | `	if( pNos < pStack ){` |
|        - |  4809 | `		goto Abort;` |
|        - |  4810 | `	}` |
|        - |  4811 | `#endif` |
|      152 |  4812 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      150 |  4813 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      150 |  4814 | `		ph7_class *pClass = 0;` |
|        - |  4815 | `		/* Extract the target class */` |
|      150 |  4816 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4817 | `			/* Instance already loaded */` |
|      ! 0 |  4818 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      150 |  4819 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      150 |  4820 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      150 |  4821 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4822 | `			/* Handle self/static/parent keywords */` |
|      150 |  4823 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4824 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      148 |  4825 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4826 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      147 |  4827 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4828 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4829 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4830 | `					pClass = pSelf->pBase;` |
|        2 |  4831 | `				}` |
|        3 |  4832 | `			}else{` |
|      140 |  4833 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4834 | `			}` |
|       74 |  4835 | `		}` |
|      150 |  4836 | `		if( pClass ){` |
|        - |  4837 | `			/* Perform the query */` |
|      150 |  4838 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       74 |  4839 | `		}` |
|       74 |  4840 | `	}` |
|        - |  4841 | `	/* Push result */` |
|      152 |  4842 | `	VmPopOperand(&pTos,1);` |
|      152 |  4843 | `	PH7_MemObjRelease(pTos);` |
|      152 |  4844 | `	pTos->x.iVal = iRes;` |
|      152 |  4845 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      152 |  4846 | `	break;` |
|        - |  4847 | `				 }` |
|        - |  4848 |  |
|        - |  4849 | `/*` |
|        - |  4850 | ` * LOADC P1 P2 *` |
|        - |  4851 | ` *` |
|        - |  4852 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4853 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4854 | ` */` |
|  1018184 |  4855 | `case PH7_OP_LOADC: {` |
|        - |  4856 | `	ph7_value *pObj;` |
|        - |  4857 | `	/* Reserve a room */` |
|  2036414 |  4858 | `	pTos++;` |
|  3044747 |  4859 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2036414 |  4860 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4861 | `			SyHashEntry *pEntry;` |
|        - |  4862 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4863 | `			{` |
|        - |  4864 | `				SyHashEntry *pConstImport;` |
|    29693 |  4865 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19794 |  4866 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19796 |  4867 | `				if( pConstImport ){` |
|       11 |  4868 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4869 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4870 | `					if( pEntry ){` |
|       11 |  4871 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4872 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4873 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4874 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4875 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4876 | `						break;` |
|        - |  4877 | `					}` |
|        - |  4878 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4879 | `				}` |
|        - |  4880 | `			}` |
|        - |  4881 | `			/* Candidate for expansion via user defined callbacks */` |
|    19786 |  4882 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19786 |  4883 | `			if( pEntry ){` |
|    19780 |  4884 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4885 | `				/* Set a NULL default value */` |
|    19780 |  4886 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19780 |  4887 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4888 | `				/* Invoke the callback and deal with the expanded value */` |
|    19780 |  4889 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4890 | `				/* Mark as constant */` |
|    19780 |  4891 | `				pTos->nIdx = SXU32_HIGH;` |
|    19780 |  4892 | `				break;` |
|        - |  4893 | `			}` |
|        - |  4894 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4895 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4896 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4897 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4898 | `			{` |
|        8 |  4899 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        8 |  4900 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4901 | `				sxu32 j;` |
|        8 |  4902 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       24 |  4903 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  4904 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  4905 | `				}` |
|        8 |  4906 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4907 | `					/* Try current_namespace\name */` |
|      ! 0 |  4908 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4909 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4910 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4911 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4912 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4913 | `					if( pEntry ){` |
|      ! 0 |  4914 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4915 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4916 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4917 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4918 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4919 | `						break;` |
|        - |  4920 | `					}` |
|        - |  4921 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4922 | `				}` |
|        8 |  4923 | `				if( isQualified ){` |
|        - |  4924 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4925 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4926 | `					SyBlob sErr;` |
|        3 |  4927 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4928 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4929 | `					if( pErrFile ){` |
|        3 |  4930 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4931 | `					}` |
|        3 |  4932 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4933 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4934 | `					SyBlobRelease(&sErr);` |
|        3 |  4935 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4936 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4937 | `					goto LoadC_Done;` |
|        - |  4938 | `				}` |
|        - |  4939 | `			}` |
|        2 |  4940 | `		}` |
|  2016624 |  4941 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1008335 |  4942 | `	}else{` |
|        - |  4943 | `		/* Set a NULL value */` |
|      ! 0 |  4944 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4945 | `	}` |
|  1008290 |  4946 | `LoadC_Done:` |
|        - |  4947 | `	/* Mark as constant */` |
|  2016626 |  4948 | `	pTos->nIdx = SXU32_HIGH;` |
|  2016626 |  4949 | `	break;` |
|        - |  4950 | `				  }` |
|        - |  4951 | `/*` |
|        - |  4952 | ` * LOAD: P1 * P3` |
|        - |  4953 | ` *` |
|        - |  4954 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4955 | ` * from the P3 operand.` |
|        - |  4956 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4957 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4958 | ` */` |
|  1579287 |  4959 | `case PH7_OP_LOAD:{` |
|        - |  4960 | `	ph7_value *pObj;` |
|        - |  4961 | `	SyString sName;` |
|  3158796 |  4962 | `	if( pInstr->p3 == 0 ){` |
|        - |  4963 | `		/* Take the variable name from the top of the stack */` |
|        - |  4964 | `#ifdef UNTRUST` |
|        - |  4965 | `		if( pTos < pStack ){` |
|        - |  4966 | `			goto Abort;` |
|        - |  4967 | `		}` |
|        - |  4968 | `#endif` |
|        - |  4969 | `		/* Force a string cast */` |
|       19 |  4970 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4971 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4972 | `		}` |
|       19 |  4973 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4974 | `	}else{` |
|  3158778 |  4975 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4976 | `		/* Reserve a room for the target object */` |
|  3158778 |  4977 | `		pTos++;` |
|        - |  4978 | `	}` |
|        - |  4979 | `	/* Extract the requested memory object */` |
|  3158796 |  4980 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3158796 |  4981 | `	if( pObj == 0 ){` |
|      858 |  4982 | `		if( pInstr->iP1 ){` |
|        - |  4983 | `			/* Variable not found,load NULL */` |
|      858 |  4984 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4985 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4986 | `			}else{` |
|      858 |  4987 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4988 | `			}` |
|      858 |  4989 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1579717 |  4990 | `			break;` |
|      ! 0 |  4991 | `		}else{` |
|        - |  4992 | `			/* Fatal error */` |
|      ! 0 |  4993 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4994 | `			goto Abort;` |
|        - |  4995 | `		}` |
|        - |  4996 | `	}` |
|        - |  4997 | `	/* Load variable contents */` |
|  3157940 |  4998 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3157940 |  4999 | `	pTos->nIdx = pObj->nIdx;` |
|  3157940 |  5000 | `	break;` |
|        - |  5001 | `				   }` |
|        - |  5002 | `/*` |
|        - |  5003 | ` * LOAD_MAP P1 * *` |
|        - |  5004 | ` *` |
|        - |  5005 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  5006 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  5007 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  5008 | ` */` |
|    22849 |  5009 | `case PH7_OP_LOAD_MAP: {` |
|        - |  5010 | `	ph7_hashmap *pMap;` |
|        - |  5011 | `	/* Allocate a new hashmap instance */` |
|    45700 |  5012 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    45700 |  5013 | `	if( pMap == 0 ){` |
|      ! 0 |  5014 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5015 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  5016 | `		goto Abort;` |
|        - |  5017 | `	}` |
|    45700 |  5018 | `	if( pInstr->iP1 > 0 ){` |
|     2788 |  5019 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2788 |  5020 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  5021 | `		/* Perform the insertion */` |
|     8514 |  5022 | `		while( pEntry < pTos ){` |
|     5744 |  5023 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  5024 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  5025 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  5026 | `				 * renumbered. Same routine that backs array_merge. */` |
|       70 |  5027 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       53 |  5028 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       53 |  5029 | `					if( rcMerge != SXRET_OK ){` |
|        - |  5030 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  5031 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  5032 | `						 * map dangling. */` |
|      ! 0 |  5033 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5034 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  5035 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  5036 | `						break;` |
|        - |  5037 | `					}` |
|       27 |  5038 | `				}else{` |
|        - |  5039 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       17 |  5040 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       17 |  5041 | `					break;` |
|        1 |  5042 | `				}` |
|     5702 |  5043 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  5044 | `				/* Insertion by reference */` |
|      151 |  5045 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  5046 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  5047 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  5048 | `					);` |
|       51 |  5049 | `			}else{` |
|        - |  5050 | `				/* Standard insertion */` |
|     8363 |  5051 | `				PH7_HashmapInsert(pMap,` |
|     5574 |  5052 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2787 |  5053 | `					&pEntry[1]` |
|        - |  5054 | `				);` |
|        - |  5055 | `			}` |
|        - |  5056 | `			/* Next pair on the stack */` |
|     5728 |  5057 | `			pEntry += 2;` |
|        2 |  5058 | `		}` |
|        - |  5059 | `		/* Pop P1 elements */` |
|     2788 |  5060 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2788 |  5061 | `		if( rcSpread != SXRET_OK ){` |
|        - |  5062 | `			/* Discard the partially-built map and propagate the exception. */` |
|       17 |  5063 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       17 |  5064 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  5065 | `				goto Abort;` |
|        - |  5066 | `			}` |
|        - |  5067 | `			{` |
|       17 |  5068 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       17 |  5069 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  5070 | `					pc = pFrm2->iExceptionJump - 1;` |
|        3 |  5071 | `					break;` |
|        - |  5072 | `				}` |
|        - |  5073 | `			}` |
|       15 |  5074 | `			goto Exception;` |
|        - |  5075 | `		}` |
|     1385 |  5076 | `	}` |
|        - |  5077 | `	/* Push the hashmap */` |
|    45684 |  5078 | `	pTos++;` |
|    45684 |  5079 | `	pTos->nIdx = SXU32_HIGH;` |
|    45684 |  5080 | `	pTos->x.pOther = pMap;` |
|    45684 |  5081 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    45684 |  5082 | `	break;` |
|        - |  5083 | `					  }` |
|        - |  5084 | `/*` |
|        - |  5085 | ` * LOAD_LIST: P1 * *` |
|        - |  5086 | ` *` |
|        - |  5087 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  5088 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  5089 | ` * Caveats:` |
|        - |  5090 | ` *  This implementation support only a single nesting level.` |
|        - |  5091 | ` */` |
|       48 |  5092 | `case PH7_OP_LOAD_LIST: {` |
|        - |  5093 | `	ph7_value *pEntry;` |
|       98 |  5094 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  5095 | `		/* Empty list,break immediately */` |
|      ! 0 |  5096 | `		break;` |
|        - |  5097 | `	}` |
|       98 |  5098 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  5099 | `#ifdef UNTRUST` |
|        - |  5100 | `	if( &pEntry[-1] < pStack ){` |
|        - |  5101 | `		goto Abort;` |
|        - |  5102 | `	}` |
|        - |  5103 | `#endif` |
|       98 |  5104 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  5105 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  5106 | `		ph7_hashmap_node *pNode;` |
|        - |  5107 | `		ph7_value sKey,*pObj;` |
|        - |  5108 | `		/* Start Copying */` |
|       91 |  5109 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  5110 | `		while( pEntry <= pTos ){` |
|      193 |  5111 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  5112 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  5113 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  5114 | `					if( rc == SXRET_OK ){` |
|        - |  5115 | `						/* Store node value */` |
|      165 |  5116 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  5117 | `					}else{` |
|        - |  5118 | `						/* Undefined array key */` |
|        - |  5119 | `						char zMsg[128];` |
|      ! 0 |  5120 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  5121 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5122 | `						PH7_MemObjRelease(pObj);` |
|        - |  5123 | `					}` |
|       82 |  5124 | `				}` |
|       82 |  5125 | `			}` |
|      193 |  5126 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  5127 | `			pEntry++;` |
|        1 |  5128 | `		}` |
|       46 |  5129 | `	}else{` |
|        - |  5130 | `		/* Source is not an array */` |
|        - |  5131 | `		ph7_value *pObj;` |
|       18 |  5132 | `		while( pEntry <= pTos ){` |
|       12 |  5133 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  5134 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  5135 | `					PH7_MemObjRelease(pObj);` |
|        5 |  5136 | `				}` |
|        5 |  5137 | `			}` |
|       12 |  5138 | `			pEntry++;` |
|        2 |  5139 | `		}` |
|        8 |  5140 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  5141 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  5142 | `			const char *zType = "unknown";` |
|        3 |  5143 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  5144 | `			char zMsg[256];` |
|        3 |  5145 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  5146 | `				zType = "string";` |
|        1 |  5147 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  5148 | `				zType = "int";` |
|      ! 0 |  5149 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5150 | `				zType = "float";` |
|      ! 0 |  5151 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  5152 | `				zType = "object";` |
|      ! 0 |  5153 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  5154 | `				zType = "resource";` |
|      ! 0 |  5155 | `			}` |
|        3 |  5156 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  5157 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  5158 | `		}` |
|        - |  5159 | `	}` |
|       98 |  5160 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  5161 | `	break;` |
|        - |  5162 | `					   }` |
|        - |  5163 | `/*` |
|        - |  5164 | ` * LOAD_IDX: P1 P2 *` |
|        - |  5165 | ` *` |
|        - |  5166 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  5167 | ` * from the stack.` |
|        - |  5168 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  5169 | ` * instead.` |
|        - |  5170 | ` */` |
|   251081 |  5171 | `case PH7_OP_LOAD_IDX: {` |
|   502208 |  5172 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   502208 |  5173 | `	ph7_hashmap *pMap = 0;` |
|        - |  5174 | `	ph7_value *pIdx;` |
|   502208 |  5175 | `	pIdx = 0;` |
|   502208 |  5176 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  5177 | `		if( !pInstr->iP2){` |
|        - |  5178 | `			/* No available index,load NULL */` |
|      ! 0 |  5179 | `			if( pTos >= pStack ){` |
|      ! 0 |  5180 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5181 | `			}else{` |
|        - |  5182 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  5183 | `				pTos++;` |
|      ! 0 |  5184 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5185 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5186 | `			}` |
|        - |  5187 | `			/* Emit a notice */` |
|      ! 0 |  5188 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  5189 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  5190 | `			break;` |
|        - |  5191 | `		}` |
|      ! 0 |  5192 | `	}else{` |
|   502208 |  5193 | `		pIdx = pTos;` |
|   502208 |  5194 | `		pTos--;` |
|        - |  5195 | `	}` |
|   502208 |  5196 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  5197 | `		/* String access */` |
|   388236 |  5198 | `		if( pIdx ){` |
|        - |  5199 | `			sxu32 nOfft;` |
|   388236 |  5200 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  5201 | `				/* Force an int cast */` |
|      ! 0 |  5202 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5203 | `			}` |
|   388236 |  5204 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   388236 |  5205 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  5206 | `				/* Invalid offset,load null */` |
|      ! 0 |  5207 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5208 | `			}else{` |
|   388236 |  5209 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   388236 |  5210 | `				int c = zData[nOfft];` |
|   388236 |  5211 | `				PH7_MemObjRelease(pTos);` |
|   388236 |  5212 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   388236 |  5213 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  5214 | `			}` |
|   194141 |  5215 | `		}else{` |
|        - |  5216 | `			/* No available index,load NULL */` |
|      ! 0 |  5217 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5218 | `		}` |
|   388236 |  5219 | `		break;` |
|        - |  5220 | `	}` |
|   113974 |  5221 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5222 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  5223 | `		 * iP2 codes:` |
|        - |  5224 | `		 *   0 = read       → offsetGet` |
|        - |  5225 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  5226 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  5227 | `		 *   4 = isset()    → offsetExists` |
|        - |  5228 | `		 *   5 = unset()    → offsetUnset` |
|        - |  5229 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      126 |  5230 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      126 |  5231 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      126 |  5232 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5233 | `			ph7_class_method *pMeth;` |
|        - |  5234 | `			ph7_value sResult;` |
|        - |  5235 | `			ph7_value *apArg[1];` |
|      124 |  5236 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  5237 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  5238 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  5239 | `					"Cannot use [] for reading");` |
|      ! 0 |  5240 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5241 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5242 | `				break;` |
|        - |  5243 | `			}` |
|      124 |  5244 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      124 |  5245 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  5246 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       51 |  5247 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5248 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       51 |  5249 | `				apArg[0] = pIdx;` |
|       51 |  5250 | `				if( pMeth ){` |
|       51 |  5251 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       26 |  5252 | `				}` |
|       99 |  5253 | `			}else if( pInstr->iP2 == 5 ){` |
|        9 |  5254 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5255 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|        9 |  5256 | `				apArg[0] = pIdx;` |
|        9 |  5257 | `				if( pMeth ){` |
|        9 |  5258 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  5259 | `				}` |
|        5 |  5260 | `			}else{` |
|       66 |  5261 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5262 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       66 |  5263 | `				apArg[0] = pIdx;` |
|       66 |  5264 | `				if( pMeth ){` |
|       66 |  5265 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  5266 | `				}` |
|        - |  5267 | `			}` |
|      124 |  5268 | `			if( pInstr->iP2 == 4 ){` |
|        - |  5269 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  5270 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  5271 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       33 |  5272 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       33 |  5273 | `				PH7_MemObjRelease(pTos);` |
|       33 |  5274 | `				pTos->nIdx = SXU32_HIGH;` |
|       33 |  5275 | `				if( bExists ){` |
|       17 |  5276 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       17 |  5277 | `					pTos->x.iVal = 1;` |
|        9 |  5278 | `				}else{` |
|       17 |  5279 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        1 |  5280 | `				}` |
|      108 |  5281 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  5282 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  5283 | `				 * vm_builtin_unset is a harmless no-op. */` |
|        9 |  5284 | `				PH7_MemObjRelease(pTos);` |
|        9 |  5285 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  5286 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       88 |  5287 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  5288 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  5289 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  5290 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  5291 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  5292 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  5293 | `				PH7_MemObjRelease(pTos);` |
|       11 |  5294 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  5295 | `				if( !bExists ){` |
|        3 |  5296 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  5297 | `				}else{` |
|        9 |  5298 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5299 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  5300 | `					ph7_value sValue;` |
|        9 |  5301 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  5302 | `					apArg[0] = pIdx;` |
|        9 |  5303 | `					if( pGet ){` |
|        9 |  5304 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  5305 | `					}` |
|        9 |  5306 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  5307 | `					PH7_MemObjRelease(&sValue);` |
|        - |  5308 | `				}` |
|       11 |  5309 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  5310 | `				break; /* skip the duplicate sResult release below */` |
|       74 |  5311 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  5312 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  5313 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  5314 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  5315 | `				 *     and push NULL.` |
|        - |  5316 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|        9 |  5317 | `				int bExists = ph7_value_to_bool(&sResult);` |
|        9 |  5318 | `				int bShouldArm = !bExists;` |
|        - |  5319 | `				ph7_value sValue;` |
|        9 |  5320 | `				PH7_MemObjRelease(&sResult);` |
|        - |  5321 | `				/* Reset any prior arming defensively */` |
|        9 |  5322 | `				VmCoalesceDisarm(pVm);` |
|        9 |  5323 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  5324 | `				if( bExists ){` |
|        5 |  5325 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5326 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  5327 | `					apArg[0] = pIdx;` |
|        5 |  5328 | `					if( pGet ){` |
|        5 |  5329 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  5330 | `					}` |
|        5 |  5331 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  5332 | `						bShouldArm = 1;` |
|        1 |  5333 | `					}` |
|        2 |  5334 | `				}` |
|        9 |  5335 | `				PH7_MemObjRelease(pTos);` |
|        9 |  5336 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  5337 | `				if( bShouldArm ){` |
|        - |  5338 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  5339 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  5340 | `					 * intervening expression evaluation. */` |
|        7 |  5341 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 |  5342 | `					if( pIdx ){` |
|        7 |  5343 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  5344 | `					}` |
|        7 |  5345 | `					pVm->pCoalesceObj = pInst;` |
|        7 |  5346 | `					pInst->iRef++;` |
|        7 |  5347 | `					pVm->bCoalesceArmed = 1;` |
|        4 |  5348 | `				}else{` |
|        3 |  5349 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  5350 | `				}` |
|        9 |  5351 | `				PH7_MemObjRelease(&sValue);` |
|        9 |  5352 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        9 |  5353 | `				break;` |
|      ! 0 |  5354 | `			}else{` |
|        - |  5355 | `				/* offsetGet: replace pTos with the returned value. */` |
|       66 |  5356 | `				PH7_MemObjRelease(pTos);` |
|       66 |  5357 | `				PH7_MemObjStore(&sResult,pTos);` |
|       66 |  5358 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5359 | `			}` |
|      106 |  5360 | `			PH7_MemObjRelease(&sResult);` |
|      106 |  5361 | `			if( pIdx ){` |
|      106 |  5362 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  5363 | `			}` |
|      106 |  5364 | `			break;` |
|        - |  5365 | `		}` |
|        - |  5366 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  5367 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  5368 | `		if( pInst ){` |
|        - |  5369 | `			char zMsg[256];` |
|        3 |  5370 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  5371 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5372 | `				"Cannot use object of type %.*s as array",` |
|        2 |  5373 | `				(int)pName->nByte,pName->zString);` |
|        3 |  5374 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5375 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  5376 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5377 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5378 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5379 | `			break;` |
|        - |  5380 | `		}` |
|      ! 0 |  5381 | `	}` |
|   113850 |  5382 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  5383 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5384 | `			ph7_value *pObj;` |
|        3 |  5385 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5386 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  5387 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  5388 | `			}` |
|        1 |  5389 | `		}` |
|        1 |  5390 | `	}` |
|   113850 |  5391 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   113850 |  5392 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   113850 |  5393 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  5394 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  5395 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  5396 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5397 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5398 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5399 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      896 |  5400 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      447 |  5401 | `		}` |
|        - |  5402 | `		/* Point to the hashmap */` |
|   113850 |  5403 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   113850 |  5404 | `		if( pIdx ){` |
|        - |  5405 | `			/* Load the desired entry */` |
|   113850 |  5406 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    56924 |  5407 | `		}` |
|   113850 |  5408 | `		if( pInstr->iP2 == 3 ){` |
|        - |  5409 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  5410 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  5411 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  5412 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  5413 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5414 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5415 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5416 | `			 * correct for the outermost write. */` |
|       19 |  5417 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5418 | `			if( !needWrite && pNode ){` |
|       13 |  5419 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5420 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5421 | `					needWrite = 1;` |
|        3 |  5422 | `				}` |
|        6 |  5423 | `			}` |
|       19 |  5424 | `			if( needWrite ){` |
|       13 |  5425 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5426 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5427 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5428 | `					 * into the new map's storage. */` |
|        7 |  5429 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5430 | `					if( pIdx ){` |
|        7 |  5431 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5432 | `					}` |
|        3 |  5433 | `				}` |
|        6 |  5434 | `			}` |
|        9 |  5435 | `		}` |
|   113850 |  5436 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5437 | `			/* Create a new empty entry */` |
|      273 |  5438 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5439 | `			if( rc == SXRET_OK ){` |
|        - |  5440 | `				/* Point to the last inserted entry */` |
|      273 |  5441 | `				pNode = pMap->pLast;` |
|      136 |  5442 | `			}` |
|      136 |  5443 | `		}` |
|    56924 |  5444 | `	}` |
|   113850 |  5445 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5446 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5447 | `		char zMsg[128];` |
|      ! 0 |  5448 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5449 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5450 | `		}` |
|      ! 0 |  5451 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5452 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5453 | `	}` |
|   113850 |  5454 | `	if( pIdx ){` |
|   113850 |  5455 | `		PH7_MemObjRelease(pIdx);` |
|    56924 |  5456 | `	}` |
|   113850 |  5457 | `	if( rc == SXRET_OK ){` |
|        - |  5458 | `		/* Load entry contents */` |
|    50480 |  5459 | `		if( pMap->iRef < 2 ){` |
|        - |  5460 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5461 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5462 | `			 */` |
|       28 |  5463 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5464 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5465 | `		}else{` |
|    50454 |  5466 | `			pTos->nIdx = pNode->nValIdx;` |
|    50454 |  5467 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    50454 |  5468 | `			PH7_HashmapUnref(pMap);` |
|        - |  5469 | `		}` |
|    25241 |  5470 | `	}else{` |
|        - |  5471 | `		/* No such entry,load NULL */` |
|    63372 |  5472 | `		PH7_MemObjRelease(pTos);` |
|    63372 |  5473 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5474 | `	}` |
|   113850 |  5475 | `	break;` |
|        - |  5476 | `					  }` |
|        - |  5477 | `/*` |
|        - |  5478 | ` * LOAD_CLOSURE * * P3` |
|        - |  5479 | ` *` |
|        - |  5480 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5481 | ` * name in the stack.` |
|        - |  5482 | ` */` |
|       64 |  5483 | `case PH7_OP_LOAD_CLOSURE:{` |
|      130 |  5484 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      130 |  5485 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5486 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5487 | `		ph7_vm_func *pClosure;` |
|        - |  5488 | `		char *zName;` |
|        - |  5489 | `		sxu32 mLen;` |
|        - |  5490 | `		sxu32 n;` |
|        - |  5491 | `		/* Create a new VM function */` |
|      130 |  5492 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5493 | `		/* Generate an unique closure name */` |
|      130 |  5494 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|      130 |  5495 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5496 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5497 | `			goto Abort;` |
|        - |  5498 | `		}` |
|      130 |  5499 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      130 |  5500 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5501 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5502 | `		}` |
|        - |  5503 | `		/* Zero the stucture */` |
|      130 |  5504 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5505 | `		/* Perform a structure assignment on read-only items */` |
|      130 |  5506 | `		pClosure->aArgs = pFunc->aArgs;` |
|      130 |  5507 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|      130 |  5508 | `		pClosure->aStatic = pFunc->aStatic;` |
|      130 |  5509 | `		pClosure->iFlags = pFunc->iFlags;` |
|      130 |  5510 | `		pClosure->pUserData = pFunc->pUserData;` |
|      130 |  5511 | `		pClosure->sSignature = pFunc->sSignature;` |
|      130 |  5512 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|      130 |  5513 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|      130 |  5514 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|      130 |  5515 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|      130 |  5516 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5517 | `		/* Register the closure */` |
|      130 |  5518 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5519 | `		/* Set up closure environment */` |
|      130 |  5520 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|      130 |  5521 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      324 |  5522 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5523 | `			ph7_value *pValue;` |
|      196 |  5524 | `			pEnv = &aEnv[n];` |
|      196 |  5525 | `			sEnv.sName  = pEnv->sName;` |
|      196 |  5526 | `			sEnv.iFlags = pEnv->iFlags;` |
|      196 |  5527 | `			sEnv.nIdx = SXU32_HIGH;` |
|      196 |  5528 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      196 |  5529 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5530 | `				/* Pass by reference */` |
|      ! 0 |  5531 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5532 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5533 | `					);` |
|      ! 0 |  5534 | `			}` |
|        - |  5535 | `			/* Standard pass by value */` |
|      196 |  5536 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      196 |  5537 | `			if( pValue ){` |
|        - |  5538 | `				/* Copy imported value */` |
|       72 |  5539 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5540 | `			}` |
|        - |  5541 | `			/* Insert the imported variable */` |
|      196 |  5542 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       99 |  5543 | `		}` |
|        - |  5544 | `		/* Finally,load the closure name on the stack */` |
|      130 |  5545 | `		pTos++;` |
|      130 |  5546 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       64 |  5547 | `	}` |
|      130 |  5548 | `	break;` |
|        - |  5549 | `						 }` |
|        - |  5550 | `/*` |
|        - |  5551 | ` * STORE * P2 P3` |
|        - |  5552 | ` *` |
|        - |  5553 | ` * Perform a store (Assignment) operation.` |
|        - |  5554 | ` */` |
|   146469 |  5555 | `case PH7_OP_STORE: {` |
|        - |  5556 | `	ph7_value *pObj;` |
|        - |  5557 | `	SyString sName;` |
|        - |  5558 | `#ifdef UNTRUST` |
|        - |  5559 | `	if( pTos < pStack ){` |
|        - |  5560 | `		goto Abort;` |
|        - |  5561 | `	}` |
|        - |  5562 | `#endif` |
|   292940 |  5563 | `	if( pInstr->iP2 ){` |
|        - |  5564 | `		sxu32 nIdx;` |
|        - |  5565 | `		sxi32 rcT;` |
|        - |  5566 | `		/* Member store operation */` |
|     5280 |  5567 | `		nIdx = pTos->nIdx;` |
|     5280 |  5568 | `		VmPopOperand(&pTos,1);` |
|     5280 |  5569 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5570 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5571 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5572 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5573 | `		}else{` |
|        - |  5574 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5575 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5276 |  5576 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5276 |  5577 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5578 | `				goto Abort;` |
|        - |  5579 | `			}` |
|     5276 |  5580 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5581 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5582 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5583 | `				 * propagate out of the VM loop. */` |
|       37 |  5584 | `				VmPopOperand(&pTos,1);` |
|        - |  5585 | `				{` |
|       37 |  5586 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  5587 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  5588 | `						pc = pFrm2->iExceptionJump - 1;` |
|   146488 |  5589 | `						break;` |
|        - |  5590 | `					}` |
|        - |  5591 | `				}` |
|      ! 0 |  5592 | `				goto Exception;` |
|        - |  5593 | `			}` |
|        - |  5594 | `			/* Point to the desired memory object */` |
|     5240 |  5595 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5240 |  5596 | `			if( pObj ){` |
|        - |  5597 | `				/* Perform the store operation */` |
|     5240 |  5598 | `				PH7_MemObjStore(pTos,pObj);` |
|     2619 |  5599 | `			}` |
|        - |  5600 | `		}` |
|     5244 |  5601 | `		break;` |
|   287662 |  5602 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5603 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5604 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5605 | `			/* Force a string cast */` |
|      ! 0 |  5606 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5607 | `		}` |
|        7 |  5608 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5609 | `		pTos--;` |
|        - |  5610 | `#ifdef UNTRUST` |
|        - |  5611 | `		if( pTos < pStack  ){` |
|        - |  5612 | `			goto Abort;` |
|        - |  5613 | `		}` |
|        - |  5614 | `#endif` |
|        4 |  5615 | `	}else{` |
|   287656 |  5616 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5617 | `	}` |
|        - |  5618 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   287662 |  5619 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   287662 |  5620 | `	if( pObj == 0 ){` |
|      ! 0 |  5621 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5622 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5623 | `		goto Abort;` |
|        - |  5624 | `	}` |
|   287662 |  5625 | `	if( !pInstr->p3 ){` |
|        7 |  5626 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5627 | `	}` |
|        - |  5628 | `	/* Perform the store operation */` |
|   287662 |  5629 | `	PH7_MemObjStore(pTos,pObj);` |
|   287662 |  5630 | `	break;` |
|        - |  5631 | `				   }` |
|        - |  5632 | `/*` |
|        - |  5633 | ` * STORE_IDX:   P1 * P3` |
|        - |  5634 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5635 | ` *` |
|        - |  5636 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5637 | ` */` |
|    97275 |  5638 | `case PH7_OP_STORE_IDX:` |
|        - |  5639 | `case PH7_OP_STORE_IDX_REF: {` |
|   194552 |  5640 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5641 | `	ph7_value *pKey;` |
|        - |  5642 | `	sxu32 nIdx;` |
|   194552 |  5643 | `	if( pInstr->iP1 ){` |
|        - |  5644 | `		/* Key is next on stack */` |
|    63394 |  5645 | `		pKey = pTos;` |
|    63394 |  5646 | `		pTos--;` |
|    31698 |  5647 | `	}else{` |
|   131160 |  5648 | `		pKey = 0;` |
|        - |  5649 | `	}` |
|   194552 |  5650 | `	nIdx = pTos->nIdx;` |
|        - |  5651 | `	{` |
|        - |  5652 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5653 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5654 | `		 * the backing variable slot at nIdx. */` |
|   194552 |  5655 | `		ph7_class_instance *pInst = 0;` |
|   194552 |  5656 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  5657 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   194536 |  5658 | `		}else if( nIdx != SXU32_HIGH ){` |
|   194520 |  5659 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   194520 |  5660 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5661 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5662 | `			}` |
|    97259 |  5663 | `		}` |
|   194552 |  5664 | `		if( pInst ){` |
|       34 |  5665 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       34 |  5666 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5667 | `				ph7_class_method *pMeth;` |
|        - |  5668 | `				ph7_value sNullKey;` |
|        - |  5669 | `				ph7_value *apArg[2];` |
|       32 |  5670 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  5671 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5672 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  5673 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  5674 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  5675 | `					break;` |
|        - |  5676 | `				}` |
|       32 |  5677 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5678 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  5679 | `				/* Pop container; pTos now points to the value */` |
|       32 |  5680 | `				VmPopOperand(&pTos,1);` |
|       32 |  5681 | `				if( pKey == 0 ){` |
|        7 |  5682 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  5683 | `					apArg[0] = &sNullKey;` |
|        4 |  5684 | `				}else{` |
|       26 |  5685 | `					apArg[0] = pKey;` |
|        - |  5686 | `				}` |
|       32 |  5687 | `				apArg[1] = pTos;` |
|       32 |  5688 | `				if( pMeth ){` |
|       32 |  5689 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  5690 | `				}` |
|       32 |  5691 | `				if( pKey ){` |
|       26 |  5692 | `					PH7_MemObjRelease(pKey);` |
|       14 |  5693 | `				}else{` |
|        7 |  5694 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  5695 | `				}` |
|        - |  5696 | `				/* Pop the value */` |
|       32 |  5697 | `				VmPopOperand(&pTos,1);` |
|       32 |  5698 | `				break;` |
|        - |  5699 | `			}` |
|        - |  5700 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  5701 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  5702 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  5703 | `			 * a few lines below). Match PHP. */` |
|        - |  5704 | `			{` |
|        - |  5705 | `				char zMsg[256];` |
|        3 |  5706 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  5707 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5708 | `					"Cannot use object of type %.*s as array",` |
|        2 |  5709 | `					(int)pName->nByte,pName->zString);` |
|        3 |  5710 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5711 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  5712 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  5713 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5714 | `				break;` |
|        - |  5715 | `			}` |
|        - |  5716 | `		}` |
|        - |  5717 | `	}` |
|   194520 |  5718 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5719 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5720 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5721 | `		 * checking true sharing count, then re-add after separation. */` |
|   194468 |  5722 | `		if( nIdx != SXU32_HIGH ){` |
|   194468 |  5723 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   291701 |  5724 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   194468 |  5725 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5726 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5727 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5728 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5729 | `				 * refcounts if the backing array was already separated. */` |
|   194468 |  5730 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   194468 |  5731 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   194468 |  5732 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   194468 |  5733 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   194468 |  5734 | `					pTos->x.pOther = pMap;` |
|    97235 |  5735 | `				}else{` |
|        - |  5736 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5737 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5738 | `					pMap = pCur;` |
|        - |  5739 | `				}` |
|    97235 |  5740 | `			}else{` |
|      ! 0 |  5741 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5742 | `			}` |
|    97235 |  5743 | `		}else{` |
|      ! 0 |  5744 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5745 | `		}` |
|   194468 |  5746 | `		if( pMap->iRef < 2 ){` |
|        - |  5747 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5748 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5749 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5750 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5751 | `			pMap->iRef = 2;` |
|      ! 0 |  5752 | `		}` |
|    97235 |  5753 | `	}else{` |
|        - |  5754 | `		ph7_value *pObj;` |
|       53 |  5755 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5756 | `		if( pObj == 0 ){` |
|      ! 0 |  5757 | `			if( pKey ){` |
|      ! 0 |  5758 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5759 | `			}` |
|      ! 0 |  5760 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5761 | `			break;` |
|        - |  5762 | `		}` |
|        - |  5763 | `		/* Phase#1: Load the array */` |
|       53 |  5764 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5765 | `			VmPopOperand(&pTos,1);` |
|       53 |  5766 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5767 | `				/* Force a string cast */` |
|      ! 0 |  5768 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5769 | `			}` |
|       53 |  5770 | `			if( pKey == 0 ){` |
|        - |  5771 | `				/* Append string */` |
|        3 |  5772 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5773 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5774 | `				}` |
|        2 |  5775 | `			}else{` |
|        - |  5776 | `				sxu32 nOfft;` |
|       51 |  5777 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5778 | `					/* Force an int cast */` |
|       51 |  5779 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5780 | `				}` |
|       51 |  5781 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5782 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5783 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5784 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5785 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5786 | `				}else{` |
|      ! 0 |  5787 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5788 | `						/* Perform an append operation */` |
|      ! 0 |  5789 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5790 | `					}` |
|        - |  5791 | `				}` |
|        - |  5792 | `			}` |
|       53 |  5793 | `			if( pKey ){` |
|       51 |  5794 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  5795 | `			}` |
|       53 |  5796 | `			break;` |
|      ! 0 |  5797 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  5798 | `			/* Force a hashmap cast  */` |
|      ! 0 |  5799 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  5800 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  5801 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  5802 | `				goto Abort;` |
|        - |  5803 | `			}` |
|      ! 0 |  5804 | `		}` |
|        - |  5805 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  5806 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  5807 | `	}` |
|   194468 |  5808 | `	VmPopOperand(&pTos,1);` |
|        - |  5809 | `	/* Phase#2: Perform the insertion */` |
|   194468 |  5810 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5811 | `		/* Insertion by reference */` |
|       15 |  5812 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5813 | `	}else{` |
|   194454 |  5814 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5815 | `	}` |
|   194468 |  5816 | `	if( pKey ){` |
|    63318 |  5817 | `		PH7_MemObjRelease(pKey);` |
|    31658 |  5818 | `	}` |
|   194468 |  5819 | `	break;` |
|        - |  5820 | `					   }` |
|        - |  5821 | `/*` |
|        - |  5822 | ` * INCR: P1 * *` |
|        - |  5823 | ` *` |
|        - |  5824 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5825 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5826 | ` * the stack and increment after that.` |
|        - |  5827 | ` */` |
|   167919 |  5828 | `case PH7_OP_INCR:` |
|        - |  5829 | `#ifdef UNTRUST` |
|        - |  5830 | `	if( pTos < pStack ){` |
|        - |  5831 | `		goto Abort;` |
|        - |  5832 | `	}` |
|        - |  5833 | `#endif` |
|   335884 |  5834 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   335884 |  5835 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5836 | `			ph7_value *pObj;` |
|   335884 |  5837 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   335884 |  5838 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5839 | `					/* Perl-style string increment.` |
|        - |  5840 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  5841 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  5842 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  5843 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  5844 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  5845 | `					}` |
|       49 |  5846 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  5847 | `					if( pInstr->iP1 ){` |
|        - |  5848 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  5849 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  5850 | `					}` |
|       25 |  5851 | `				}else{` |
|        - |  5852 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  5853 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  5854 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  5855 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  5856 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5857 | `					 * so its old-value view survives the coercion. */` |
|   335836 |  5858 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  5859 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  5860 | `					}` |
|        - |  5861 | `					/* Force a numeric cast on the variable */` |
|   335836 |  5862 | `					PH7_MemObjToNumeric(pObj);` |
|   335836 |  5863 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5864 | `						pObj->rVal++;` |
|        - |  5865 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5866 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5867 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5868 | `						 * integer-valued real. */` |
|        9 |  5869 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5870 | `					}else{` |
|   335828 |  5871 | `						pObj->x.iVal++;` |
|        - |  5872 | `					}` |
|   335836 |  5873 | `					if( pInstr->iP1 ){` |
|        - |  5874 | `						/* Pre-increment: result is the new value. */` |
|       83 |  5875 | `						PH7_MemObjStore(pObj,pTos);` |
|       41 |  5876 | `					}` |
|        - |  5877 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5878 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5879 | `				}` |
|   167963 |  5880 | `			}` |
|   167965 |  5881 | `		}else{` |
|      ! 0 |  5882 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5883 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  5884 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  5885 | `				}else{` |
|        - |  5886 | `					/* Force a numeric cast */` |
|      ! 0 |  5887 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5888 | `					/* Pre-increment */` |
|      ! 0 |  5889 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5890 | `						pTos->rVal++;` |
|        - |  5891 | `						/* Try to get an integer representation */` |
|      ! 0 |  5892 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5893 | `					}else{` |
|      ! 0 |  5894 | `						pTos->x.iVal++;` |
|      ! 0 |  5895 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5896 | `					}` |
|        - |  5897 | `				}` |
|      ! 0 |  5898 | `			}` |
|        - |  5899 | `		}` |
|   167963 |  5900 | `	}` |
|   335884 |  5901 | `	break;` |
|        - |  5902 | `/*` |
|        - |  5903 | ` * DECR: P1 * *` |
|        - |  5904 | ` *` |
|        - |  5905 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  5906 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  5907 | ` * and decrement after that.` |
|        - |  5908 | ` */` |
|       14 |  5909 | `case PH7_OP_DECR:` |
|        - |  5910 | `#ifdef UNTRUST` |
|        - |  5911 | `	if( pTos < pStack ){` |
|        - |  5912 | `		goto Abort;` |
|        - |  5913 | `	}` |
|        - |  5914 | `#endif` |
|        - |  5915 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  5916 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  5917 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5918 | `			ph7_value *pObj;` |
|       27 |  5919 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  5920 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5921 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  5922 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  5923 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  5924 | `					if( pInstr->iP1 ){` |
|        - |  5925 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  5926 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5927 | `					}` |
|        - |  5928 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  5929 | `				}else{` |
|        - |  5930 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  5931 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  5932 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  5933 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  5934 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  5935 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  5936 | `					}` |
|       21 |  5937 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  5938 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5939 | `						pObj->rVal--;` |
|        - |  5940 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5941 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5942 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5943 | `						 * integer-valued real. */` |
|        9 |  5944 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5945 | `					}else{` |
|       13 |  5946 | `						pObj->x.iVal--;` |
|        - |  5947 | `					}` |
|       21 |  5948 | `					if( pInstr->iP1 ){` |
|        - |  5949 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  5950 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  5951 | `					}` |
|        - |  5952 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  5953 | `				}` |
|       13 |  5954 | `			}` |
|       14 |  5955 | `		}else{` |
|      ! 0 |  5956 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5957 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  5958 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  5959 | `				}else{` |
|        - |  5960 | `					/* Force a numeric cast */` |
|      ! 0 |  5961 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5962 | `					/* Pre-decrement */` |
|      ! 0 |  5963 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5964 | `						pTos->rVal--;` |
|        - |  5965 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  5966 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5967 | `					}else{` |
|      ! 0 |  5968 | `						pTos->x.iVal--;` |
|      ! 0 |  5969 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5970 | `					}` |
|        - |  5971 | `				}` |
|      ! 0 |  5972 | `			}` |
|        - |  5973 | `		}` |
|       13 |  5974 | `	}` |
|       29 |  5975 | `	break;` |
|        - |  5976 | `/*` |
|        - |  5977 | ` * UMINUS: * * *` |
|        - |  5978 | ` *` |
|        - |  5979 | ` * Perform a unary minus operation.` |
|        - |  5980 | ` */` |
|    29789 |  5981 | `case PH7_OP_UMINUS:` |
|        - |  5982 | `#ifdef UNTRUST` |
|        - |  5983 | `	if( pTos < pStack ){` |
|        - |  5984 | `		goto Abort;` |
|        - |  5985 | `	}` |
|        - |  5986 | `#endif` |
|        - |  5987 | `	/* Force a numeric (integer,real or both) cast */` |
|    59580 |  5988 | `	PH7_MemObjToNumeric(pTos);` |
|    59580 |  5989 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5990 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5991 | `	}` |
|    59580 |  5992 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    59550 |  5993 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    29774 |  5994 | `	}` |
|    59580 |  5995 | `	break;` |
|        - |  5996 | `/*` |
|        - |  5997 | ` * UPLUS: * * *` |
|        - |  5998 | ` *` |
|        - |  5999 | ` * Perform a unary plus operation.` |
|        - |  6000 | ` */` |
|       18 |  6001 | `case PH7_OP_UPLUS:` |
|        - |  6002 | `#ifdef UNTRUST` |
|        - |  6003 | `	if( pTos < pStack ){` |
|        - |  6004 | `		goto Abort;` |
|        - |  6005 | `	}` |
|        - |  6006 | `#endif` |
|        - |  6007 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  6008 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  6009 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6010 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  6011 | `	}` |
|       37 |  6012 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  6013 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  6014 | `	}` |
|       37 |  6015 | `	break;` |
|        - |  6016 | `/*` |
|        - |  6017 | ` * OP_LNOT: * * *` |
|        - |  6018 | ` *` |
|        - |  6019 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  6020 | ` * with its complement.` |
|        - |  6021 | ` */` |
|    45014 |  6022 | `case PH7_OP_LNOT:` |
|        - |  6023 | `#ifdef UNTRUST` |
|        - |  6024 | `	if( pTos < pStack ){` |
|        - |  6025 | `		goto Abort;` |
|        - |  6026 | `	}` |
|        - |  6027 | `#endif` |
|        - |  6028 | `	/* Force a boolean cast */` |
|    90074 |  6029 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  6030 | `		PH7_MemObjToBool(pTos);` |
|       11 |  6031 | `	}` |
|    90074 |  6032 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    90074 |  6033 | `	break;` |
|        - |  6034 | `/*` |
|        - |  6035 | ` * OP_BITNOT: * * *` |
|        - |  6036 | ` *` |
|        - |  6037 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  6038 | ` * with its ones-complement.` |
|        - |  6039 | ` */` |
|       14 |  6040 | `case PH7_OP_BITNOT:` |
|        - |  6041 | `#ifdef UNTRUST` |
|        - |  6042 | `	if( pTos < pStack ){` |
|        - |  6043 | `		goto Abort;` |
|        - |  6044 | `	}` |
|        - |  6045 | `#endif` |
|        - |  6046 | `	/* Force an integer cast */` |
|       30 |  6047 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6048 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6049 | `	}` |
|       30 |  6050 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  6051 | `	break;` |
|        - |  6052 | `/* OP_MUL * * *` |
|        - |  6053 | ` * OP_MUL_STORE * * *` |
|        - |  6054 | ` *` |
|        - |  6055 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  6056 | ` * and push the result back onto the stack.` |
|        - |  6057 | ` */` |
|     1290 |  6058 | `case PH7_OP_MUL:` |
|        - |  6059 | `case PH7_OP_MUL_STORE: {` |
|     2582 |  6060 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6061 | `	/* Force the operand to be numeric */` |
|        - |  6062 | `#ifdef UNTRUST` |
|        - |  6063 | `	if( pNos < pStack ){` |
|        - |  6064 | `		goto Abort;` |
|        - |  6065 | `	}` |
|        - |  6066 | `#endif` |
|     2582 |  6067 | `	PH7_MemObjToNumeric(pTos);` |
|     2582 |  6068 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6069 | `	/* Perform the requested operation */` |
|     2582 |  6070 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6071 | `		/* Floating point arithemic */` |
|        - |  6072 | `		ph7_real a,b,r;` |
|       21 |  6073 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  6074 | `			PH7_MemObjToReal(pTos);` |
|        4 |  6075 | `		}` |
|       21 |  6076 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  6077 | `			PH7_MemObjToReal(pNos);` |
|        3 |  6078 | `		}` |
|       21 |  6079 | `		a = pNos->rVal;` |
|       21 |  6080 | `		b = pTos->rVal;` |
|       21 |  6081 | `		r = a * b;` |
|        - |  6082 | `		/* Push the result */` |
|       21 |  6083 | `		pNos->rVal = r;` |
|       21 |  6084 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6085 | `		/* Try to get an integer representation */` |
|       21 |  6086 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  6087 | `	}else{` |
|        - |  6088 | `		/* Integer arithmetic */` |
|        - |  6089 | `		sxi64 a,b,r;` |
|     2562 |  6090 | `		a = pNos->x.iVal;` |
|     2562 |  6091 | `		b = pTos->x.iVal;` |
|     2562 |  6092 | `		r = a * b;` |
|        - |  6093 | `		/* Push the result */` |
|     2562 |  6094 | `		pNos->x.iVal = r;` |
|     2562 |  6095 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6096 | `	}` |
|     2582 |  6097 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  6098 | `		ph7_value *pObj;` |
|       32 |  6099 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6100 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  6101 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  6102 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  6103 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  6104 | `		}` |
|       15 |  6105 | `	}` |
|     2582 |  6106 | `	VmPopOperand(&pTos,1);` |
|     2582 |  6107 | `	break;` |
|        - |  6108 | `				 }` |
|        - |  6109 | `/* OP_POW * * *` |
|        - |  6110 | ` * OP_POW_STORE * * *` |
|        - |  6111 | ` *` |
|        - |  6112 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  6113 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  6114 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  6115 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  6116 | ` */` |
|       67 |  6117 | `case PH7_OP_POW:` |
|        - |  6118 | `case PH7_OP_POW_STORE: {` |
|      135 |  6119 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  6120 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  6121 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  6122 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  6123 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  6124 | `	 */` |
|      135 |  6125 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  6126 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  6127 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  6128 | `	int bBothInt;` |
|      135 |  6129 | `	int usedInt = 0;` |
|        - |  6130 | `	ph7_real a, b, r;` |
|        - |  6131 | `#endif` |
|      135 |  6132 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  6133 | `#ifdef UNTRUST` |
|        - |  6134 | `	if( pNos < pStack ){` |
|        - |  6135 | `		goto Abort;` |
|        - |  6136 | `	}` |
|        - |  6137 | `#endif` |
|      135 |  6138 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  6139 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6140 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  6141 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  6142 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  6143 | `	if( bBothInt ){` |
|      123 |  6144 | `		base_i = pBase->x.iVal;` |
|      123 |  6145 | `		exp_i  = pExp->x.iVal;` |
|       61 |  6146 | `	}` |
|      135 |  6147 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  6148 | `		PH7_MemObjToReal(pBase);` |
|       62 |  6149 | `	}` |
|      135 |  6150 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  6151 | `		PH7_MemObjToReal(pExp);` |
|       66 |  6152 | `	}` |
|      135 |  6153 | `	a = pBase->rVal;` |
|      135 |  6154 | `	b = pExp->rVal;` |
|      135 |  6155 | `	r = pow(a, b);` |
|        - |  6156 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  6157 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  6158 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  6159 | `	 * representable as double but not as signed int64. */` |
|      135 |  6160 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  6161 | `		sxi64 result_i = 1;` |
|      117 |  6162 | `		sxi64 cur_base = base_i;` |
|      117 |  6163 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  6164 | `		int overflow = 0;` |
|      401 |  6165 | `		while( cur_exp > 0 ){` |
|      289 |  6166 | `			if( cur_exp & 1 ){` |
|      189 |  6167 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  6168 | `					overflow = 1;` |
|        3 |  6169 | `					break;` |
|        - |  6170 | `				}` |
|       93 |  6171 | `			}` |
|      287 |  6172 | `			cur_exp >>= 1;` |
|      287 |  6173 | `			if( cur_exp > 0 ){` |
|      181 |  6174 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  6175 | `					overflow = 1;` |
|        3 |  6176 | `					break;` |
|        - |  6177 | `				}` |
|       89 |  6178 | `			}` |
|        1 |  6179 | `		}` |
|      117 |  6180 | `		if( !overflow ){` |
|      113 |  6181 | `			pNos->x.iVal = result_i;` |
|      113 |  6182 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  6183 | `			usedInt = 1;` |
|       56 |  6184 | `		}` |
|       58 |  6185 | `	}` |
|      135 |  6186 | `	if( !usedInt ){` |
|       23 |  6187 | `		pNos->rVal = r;` |
|       23 |  6188 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  6189 | `	}` |
|        - |  6190 | `#else` |
|        - |  6191 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  6192 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  6193 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  6194 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  6195 | `	 * represented. */` |
|        - |  6196 | `	base_i = pBase->x.iVal;` |
|        - |  6197 | `	exp_i  = pExp->x.iVal;` |
|        - |  6198 | `	{` |
|        - |  6199 | `		sxi64 result_i = 1;` |
|        - |  6200 | `		sxi64 cur_base = base_i;` |
|        - |  6201 | `		sxi64 cur_exp  = exp_i;` |
|        - |  6202 | `		if( cur_exp < 0 ){` |
|        - |  6203 | `			result_i = 0;` |
|        - |  6204 | `		}else{` |
|        - |  6205 | `			while( cur_exp > 0 ){` |
|        - |  6206 | `				if( cur_exp & 1 ){` |
|        - |  6207 | `					result_i *= cur_base;` |
|        - |  6208 | `				}` |
|        - |  6209 | `				cur_exp >>= 1;` |
|        - |  6210 | `				if( cur_exp > 0 ){` |
|        - |  6211 | `					cur_base *= cur_base;` |
|        - |  6212 | `				}` |
|        - |  6213 | `			}` |
|        - |  6214 | `		}` |
|        - |  6215 | `		pNos->x.iVal = result_i;` |
|        - |  6216 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  6217 | `	}` |
|        - |  6218 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  6219 | `	if( bStore ){` |
|        - |  6220 | `		ph7_value *pObj;` |
|       23 |  6221 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6222 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  6223 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  6224 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  6225 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  6226 | `		}` |
|       11 |  6227 | `	}` |
|      135 |  6228 | `	VmPopOperand(&pTos,1);` |
|      135 |  6229 | `	break;` |
|        - |  6230 | `				 }` |
|        - |  6231 | `/* OP_ADD * * *` |
|        - |  6232 | ` *` |
|        - |  6233 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6234 | ` * and push the result back onto the stack.` |
|        - |  6235 | ` */` |
|      536 |  6236 | `case PH7_OP_ADD:{` |
|     1074 |  6237 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6238 | `#ifdef UNTRUST` |
|        - |  6239 | `	if( pNos < pStack ){` |
|        - |  6240 | `		goto Abort;` |
|        - |  6241 | `	}` |
|        - |  6242 | `#endif` |
|        - |  6243 | `	/* Perform the addition */` |
|     1074 |  6244 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1074 |  6245 | `	VmPopOperand(&pTos,1);` |
|     1074 |  6246 | `	break;` |
|        - |  6247 | `				}` |
|        - |  6248 | `/*` |
|        - |  6249 | ` * OP_ADD_STORE * * *` |
|        - |  6250 | ` *` |
|        - |  6251 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6252 | ` * and push the result back onto the stack.` |
|        - |  6253 | ` */` |
|      502 |  6254 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  6255 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6256 | `	ph7_value *pObj;` |
|        - |  6257 | `	sxu32 nIdx;` |
|        - |  6258 | `#ifdef UNTRUST` |
|        - |  6259 | `	if( pNos < pStack ){` |
|        - |  6260 | `		goto Abort;` |
|        - |  6261 | `	}` |
|        - |  6262 | `#endif` |
|        - |  6263 | `	/* Perform the addition */` |
|     1006 |  6264 | `	nIdx = pTos->nIdx;` |
|     1006 |  6265 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  6266 | `	/* Peform the store operation */` |
|     1006 |  6267 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6268 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  6269 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  6270 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  6271 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  6272 | `	}` |
|        - |  6273 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  6274 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  6275 | `	VmPopOperand(&pTos,1);` |
|     1006 |  6276 | `	break;` |
|        - |  6277 | `				}` |
|        - |  6278 | `/* OP_SUB * * *` |
|        - |  6279 | ` *` |
|        - |  6280 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6281 | ` * first (what was next on the stack) from the second (the` |
|        - |  6282 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6283 | ` */` |
|      352 |  6284 | `case PH7_OP_SUB: {` |
|      706 |  6285 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6286 | `#ifdef UNTRUST` |
|        - |  6287 | `	if( pNos < pStack ){` |
|        - |  6288 | `		goto Abort;` |
|        - |  6289 | `	}` |
|        - |  6290 | `#endif` |
|      706 |  6291 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6292 | `		/* Floating point arithemic */` |
|        - |  6293 | `		ph7_real a,b,r;` |
|      103 |  6294 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6295 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6296 | `		}` |
|      103 |  6297 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6298 | `			PH7_MemObjToReal(pNos);` |
|        2 |  6299 | `		}` |
|      103 |  6300 | `		a = pNos->rVal;` |
|      103 |  6301 | `		b = pTos->rVal;` |
|      103 |  6302 | `		r = a - b;` |
|        - |  6303 | `		/* Push the result */` |
|      103 |  6304 | `		pNos->rVal = r;` |
|      103 |  6305 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6306 | `		/* Try to get an integer representation */` |
|      103 |  6307 | `		PH7_MemObjTryInteger(pNos);` |
|       52 |  6308 | `	}else{` |
|        - |  6309 | `		/* Integer arithmetic */` |
|        - |  6310 | `		sxi64 a,b,r;` |
|      604 |  6311 | `		a = pNos->x.iVal;` |
|      604 |  6312 | `		b = pTos->x.iVal;` |
|      604 |  6313 | `		r = a - b;` |
|        - |  6314 | `		/* Push the result */` |
|      604 |  6315 | `		pNos->x.iVal = r;` |
|      604 |  6316 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6317 | `	}` |
|      706 |  6318 | `	VmPopOperand(&pTos,1);` |
|      706 |  6319 | `	break;` |
|        - |  6320 | `				 }` |
|        - |  6321 | `/* OP_SUB_STORE * * *` |
|        - |  6322 | ` *` |
|        - |  6323 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6324 | ` * first (what was next on the stack) from the second (the` |
|        - |  6325 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6326 | ` */` |
|        4 |  6327 | `case PH7_OP_SUB_STORE: {` |
|       10 |  6328 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6329 | `	ph7_value *pObj;` |
|        - |  6330 | `#ifdef UNTRUST` |
|        - |  6331 | `	if( pNos < pStack ){` |
|        - |  6332 | `		goto Abort;` |
|        - |  6333 | `	}` |
|        - |  6334 | `#endif` |
|       10 |  6335 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6336 | `		/* Floating point arithemic */` |
|        - |  6337 | `		ph7_real a,b,r;` |
|      ! 0 |  6338 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6339 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6340 | `		}` |
|      ! 0 |  6341 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6342 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  6343 | `		}` |
|      ! 0 |  6344 | `		a = pTos->rVal;` |
|      ! 0 |  6345 | `		b = pNos->rVal;` |
|      ! 0 |  6346 | `		r = a - b;` |
|        - |  6347 | `		/* Push the result */` |
|      ! 0 |  6348 | `		pNos->rVal = r;` |
|      ! 0 |  6349 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6350 | `		/* Try to get an integer representation */` |
|      ! 0 |  6351 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  6352 | `	}else{` |
|        - |  6353 | `		/* Integer arithmetic */` |
|        - |  6354 | `		sxi64 a,b,r;` |
|       10 |  6355 | `		a = pTos->x.iVal;` |
|       10 |  6356 | `		b = pNos->x.iVal;` |
|       10 |  6357 | `		r = a - b;` |
|        - |  6358 | `		/* Push the result */` |
|       10 |  6359 | `		pNos->x.iVal = r;` |
|       10 |  6360 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6361 | `	}` |
|       10 |  6362 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6363 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  6364 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  6365 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  6366 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  6367 | `	}` |
|       10 |  6368 | `	VmPopOperand(&pTos,1);` |
|       10 |  6369 | `	break;` |
|        - |  6370 | `				 }` |
|        - |  6371 |  |
|        - |  6372 | `/*` |
|        - |  6373 | ` * OP_MOD * * *` |
|        - |  6374 | ` *` |
|        - |  6375 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6376 | ` * first (what was next on the stack) from the second (the` |
|        - |  6377 | ` * top of the stack) and push the remainder after division` |
|        - |  6378 | ` * onto the stack.` |
|        - |  6379 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6380 | ` */` |
|      309 |  6381 | `case PH7_OP_MOD:{` |
|      620 |  6382 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6383 | `	sxi64 a,b,r;` |
|        - |  6384 | `#ifdef UNTRUST` |
|        - |  6385 | `	if( pNos < pStack ){` |
|        - |  6386 | `		goto Abort;` |
|        - |  6387 | `	}` |
|        - |  6388 | `#endif` |
|        - |  6389 | `	/* Force the operands to be integer */` |
|      620 |  6390 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6391 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6392 | `	}` |
|      620 |  6393 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  6394 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  6395 | `	}` |
|        - |  6396 | `	/* Perform the requested operation */` |
|      620 |  6397 | `	a = pNos->x.iVal;` |
|      620 |  6398 | `	b = pTos->x.iVal;` |
|      620 |  6399 | `	if( b == 0 ){` |
|        3 |  6400 | `		r = 0;` |
|        3 |  6401 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6402 | `		/* goto Abort; */` |
|        2 |  6403 | `	}else{` |
|      617 |  6404 | `		r = a%b;` |
|        - |  6405 | `	}` |
|        - |  6406 | `	/* Push the result */` |
|      620 |  6407 | `	pNos->x.iVal = r;` |
|      620 |  6408 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      620 |  6409 | `	VmPopOperand(&pTos,1);` |
|      620 |  6410 | `	break;` |
|        - |  6411 | `				}` |
|        - |  6412 | `/*` |
|        - |  6413 | ` * OP_MOD_STORE * * *` |
|        - |  6414 | ` *` |
|        - |  6415 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6416 | ` * first (what was next on the stack) from the second (the` |
|        - |  6417 | ` * top of the stack) and push the remainder after division` |
|        - |  6418 | ` * onto the stack.` |
|        - |  6419 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6420 | ` */` |
|        1 |  6421 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6422 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6423 | `	ph7_value *pObj;` |
|        - |  6424 | `	sxi64 a,b,r;` |
|        - |  6425 | `#ifdef UNTRUST` |
|        - |  6426 | `	if( pNos < pStack ){` |
|        - |  6427 | `		goto Abort;` |
|        - |  6428 | `	}` |
|        - |  6429 | `#endif` |
|        - |  6430 | `	/* Force the operands to be integer */` |
|        3 |  6431 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6432 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6433 | `	}` |
|        3 |  6434 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6435 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6436 | `	}` |
|        - |  6437 | `	/* Perform the requested operation */` |
|        3 |  6438 | `	a = pTos->x.iVal;` |
|        3 |  6439 | `	b = pNos->x.iVal;` |
|        3 |  6440 | `	if( b == 0 ){` |
|      ! 0 |  6441 | `		r = 0;` |
|      ! 0 |  6442 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6443 | `		/* goto Abort; */` |
|      ! 0 |  6444 | `	}else{` |
|        3 |  6445 | `		r = a%b;` |
|        - |  6446 | `	}` |
|        - |  6447 | `	/* Push the result */` |
|        3 |  6448 | `	pNos->x.iVal = r;` |
|        3 |  6449 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6450 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6451 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6452 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6453 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6454 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6455 | `	}` |
|        3 |  6456 | `	VmPopOperand(&pTos,1);` |
|        3 |  6457 | `	break;` |
|        - |  6458 | `				}` |
|        - |  6459 | `/*` |
|        - |  6460 | ` * OP_DIV * * *` |
|        - |  6461 | ` *` |
|        - |  6462 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6463 | ` * first (what was next on the stack) from the second (the` |
|        - |  6464 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6465 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6466 | ` */` |
|       33 |  6467 | `case PH7_OP_DIV:{` |
|       68 |  6468 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6469 | `	ph7_real a,b,r;` |
|        - |  6470 | `#ifdef UNTRUST` |
|        - |  6471 | `	if( pNos < pStack ){` |
|        - |  6472 | `		goto Abort;` |
|        - |  6473 | `	}` |
|        - |  6474 | `#endif` |
|        - |  6475 | `	/* Force the operands to be real */` |
|       68 |  6476 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6477 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6478 | `	}` |
|       68 |  6479 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6480 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6481 | `	}` |
|        - |  6482 | `	/* Perform the requested operation */` |
|       68 |  6483 | `	a = pNos->rVal;` |
|       68 |  6484 | `	b = pTos->rVal;` |
|       68 |  6485 | `	if( b == 0 ){` |
|        - |  6486 | `		/* Division by zero */` |
|        3 |  6487 | `		pNos->rVal = 0;` |
|        3 |  6488 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6489 | `		/* goto Abort; */` |
|        2 |  6490 | `	}else{` |
|       65 |  6491 | `		r = a/b;` |
|        - |  6492 | `		/* Push the result */` |
|       65 |  6493 | `		pNos->rVal = r;` |
|       65 |  6494 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6495 | `		/* Try to get an integer representation */` |
|       65 |  6496 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6497 | `	}` |
|       68 |  6498 | `	VmPopOperand(&pTos,1);` |
|       68 |  6499 | `	break;` |
|        - |  6500 | `				}` |
|        - |  6501 | `/*` |
|        - |  6502 | ` * OP_DIV_STORE * * *` |
|        - |  6503 | ` *` |
|        - |  6504 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6505 | ` * first (what was next on the stack) from the second (the` |
|        - |  6506 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6507 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6508 | ` */` |
|        2 |  6509 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6510 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6511 | `	ph7_value *pObj;` |
|        - |  6512 | `	ph7_real a,b,r;` |
|        - |  6513 | `#ifdef UNTRUST` |
|        - |  6514 | `	if( pNos < pStack ){` |
|        - |  6515 | `		goto Abort;` |
|        - |  6516 | `	}` |
|        - |  6517 | `#endif` |
|        - |  6518 | `	/* Force the operands to be real */` |
|        5 |  6519 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6520 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6521 | `	}` |
|        5 |  6522 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6523 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6524 | `	}` |
|        - |  6525 | `	/* Perform the requested operation */` |
|        5 |  6526 | `	a = pTos->rVal;` |
|        5 |  6527 | `	b = pNos->rVal;` |
|        5 |  6528 | `	if( b == 0 ){` |
|        - |  6529 | `		/* Division by zero */` |
|      ! 0 |  6530 | `		r = 0;` |
|      ! 0 |  6531 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6532 | `		/* goto Abort; */` |
|      ! 0 |  6533 | `	}else{` |
|        5 |  6534 | `		r = a/b;` |
|        - |  6535 | `		/* Push the result */` |
|        5 |  6536 | `		pNos->rVal = r;` |
|        5 |  6537 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6538 | `		/* Try to get an integer representation */` |
|        5 |  6539 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6540 | `	}` |
|        5 |  6541 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6542 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6543 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6544 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6545 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6546 | `	}` |
|        5 |  6547 | `	VmPopOperand(&pTos,1);` |
|        5 |  6548 | `	break;` |
|        - |  6549 | `				}` |
|        - |  6550 | `/* OP_BAND * * *` |
|        - |  6551 | ` *` |
|        - |  6552 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6553 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6554 | ` * two elements.` |
|        - |  6555 | `*/` |
|        - |  6556 | `/* OP_BOR * * *` |
|        - |  6557 | ` *` |
|        - |  6558 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6559 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6560 | ` * two elements.` |
|        - |  6561 | ` */` |
|        - |  6562 | `/* OP_BXOR * * *` |
|        - |  6563 | ` *` |
|        - |  6564 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6565 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6566 | ` * two elements.` |
|        - |  6567 | ` */` |
|       43 |  6568 | `case PH7_OP_BAND:` |
|        - |  6569 | `case PH7_OP_BOR:` |
|        - |  6570 | `case PH7_OP_BXOR:{` |
|       88 |  6571 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6572 | `	sxi64 a,b,r;` |
|        - |  6573 | `#ifdef UNTRUST` |
|        - |  6574 | `	if( pNos < pStack ){` |
|        - |  6575 | `		goto Abort;` |
|        - |  6576 | `	}` |
|        - |  6577 | `#endif` |
|        - |  6578 | `	/* Force the operands to be integer */` |
|       88 |  6579 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6580 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6581 | `	}` |
|       88 |  6582 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6583 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6584 | `	}` |
|        - |  6585 | `	/* Perform the requested operation */` |
|       88 |  6586 | `	a = pNos->x.iVal;` |
|       88 |  6587 | `	b = pTos->x.iVal;` |
|       88 |  6588 | `	switch(pInstr->iOp){` |
|        7 |  6589 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6590 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6591 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6592 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       29 |  6593 | `	case PH7_OP_BAND_STORE:` |
|       29 |  6594 | `	case PH7_OP_BAND:` |
|       60 |  6595 | `	default:          r = a&b; break;` |
|        - |  6596 | `	}` |
|        - |  6597 | `	/* Push the result */` |
|       88 |  6598 | `	pNos->x.iVal = r;` |
|       88 |  6599 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       88 |  6600 | `	VmPopOperand(&pTos,1);` |
|       88 |  6601 | `	break;` |
|        - |  6602 | `				 }` |
|        - |  6603 | `/* OP_BAND_STORE * * *` |
|        - |  6604 | ` *` |
|        - |  6605 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6606 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6607 | ` * two elements.` |
|        - |  6608 | `*/` |
|        - |  6609 | `/* OP_BOR_STORE * * *` |
|        - |  6610 | ` *` |
|        - |  6611 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6612 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6613 | ` * two elements.` |
|        - |  6614 | ` */` |
|        - |  6615 | `/* OP_BXOR_STORE * * *` |
|        - |  6616 | ` *` |
|        - |  6617 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6618 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6619 | ` * two elements.` |
|        - |  6620 | ` */` |
|       10 |  6621 | `case PH7_OP_BAND_STORE:` |
|        - |  6622 | `case PH7_OP_BOR_STORE:` |
|        - |  6623 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6624 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6625 | `	ph7_value *pObj;` |
|        - |  6626 | `	sxi64 a,b,r;` |
|        - |  6627 | `#ifdef UNTRUST` |
|        - |  6628 | `	if( pNos < pStack ){` |
|        - |  6629 | `		goto Abort;` |
|        - |  6630 | `	}` |
|        - |  6631 | `#endif` |
|        - |  6632 | `	/* Force the operands to be integer */` |
|       21 |  6633 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6634 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6635 | `	}` |
|       21 |  6636 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6637 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6638 | `	}` |
|        - |  6639 | `	/* Perform the requested operation */` |
|       21 |  6640 | `	a = pTos->x.iVal;` |
|       21 |  6641 | `	b = pNos->x.iVal;` |
|       21 |  6642 | `	switch(pInstr->iOp){` |
|        3 |  6643 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6644 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  6645 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  6646 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  6647 | `	case PH7_OP_BAND_STORE:` |
|        3 |  6648 | `	case PH7_OP_BAND:` |
|        7 |  6649 | `	default:          r = a&b; break;` |
|        - |  6650 | `	}` |
|        - |  6651 | `	/* Push the result */` |
|       21 |  6652 | `	pNos->x.iVal = r;` |
|       21 |  6653 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  6654 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6655 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  6656 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  6657 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  6658 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  6659 | `	}` |
|       21 |  6660 | `	VmPopOperand(&pTos,1);` |
|       21 |  6661 | `	break;` |
|        - |  6662 | `				 }` |
|        - |  6663 | `/* OP_SHL * * *` |
|        - |  6664 | ` *` |
|        - |  6665 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6666 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6667 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6668 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6669 | ` */` |
|        - |  6670 | `/* OP_SHR * * *` |
|        - |  6671 | ` *` |
|        - |  6672 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6673 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6674 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6675 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6676 | ` */` |
|       12 |  6677 | `case PH7_OP_SHL:` |
|        - |  6678 | `case PH7_OP_SHR: {` |
|       25 |  6679 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6680 | `	sxi64 a,r;` |
|        - |  6681 | `	sxi32 b;` |
|        - |  6682 | `#ifdef UNTRUST` |
|        - |  6683 | `	if( pNos < pStack ){` |
|        - |  6684 | `		goto Abort;` |
|        - |  6685 | `	}` |
|        - |  6686 | `#endif` |
|        - |  6687 | `	/* Force the operands to be integer */` |
|       25 |  6688 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6689 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6690 | `	}` |
|       25 |  6691 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6692 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6693 | `	}` |
|        - |  6694 | `	/* Perform the requested operation */` |
|       25 |  6695 | `	a = pNos->x.iVal;` |
|       25 |  6696 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  6697 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  6698 | `		r = a << b;` |
|        8 |  6699 | `	}else{` |
|       11 |  6700 | `		r = a >> b;` |
|        - |  6701 | `	}` |
|        - |  6702 | `	/* Push the result */` |
|       25 |  6703 | `	pNos->x.iVal = r;` |
|       25 |  6704 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  6705 | `	VmPopOperand(&pTos,1);` |
|       25 |  6706 | `	break;` |
|        - |  6707 | `				 }` |
|        - |  6708 | `/*  OP_SHL_STORE * * *` |
|        - |  6709 | ` *` |
|        - |  6710 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6711 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6712 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6713 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6714 | ` */` |
|        - |  6715 | `/* OP_SHR_STORE * * *` |
|        - |  6716 | ` *` |
|        - |  6717 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6718 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6719 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6720 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6721 | ` */` |
|        9 |  6722 | `case PH7_OP_SHL_STORE:` |
|        - |  6723 | `case PH7_OP_SHR_STORE: {` |
|       19 |  6724 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6725 | `	ph7_value *pObj;` |
|        - |  6726 | `	sxi64 a,r;` |
|        - |  6727 | `	sxi32 b;` |
|        - |  6728 | `#ifdef UNTRUST` |
|        - |  6729 | `	if( pNos < pStack ){` |
|        - |  6730 | `		goto Abort;` |
|        - |  6731 | `	}` |
|        - |  6732 | `#endif` |
|        - |  6733 | `	/* Force the operands to be integer */` |
|       19 |  6734 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6735 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6736 | `	}` |
|       19 |  6737 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6738 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6739 | `	}` |
|        - |  6740 | `	/* Perform the requested operation */` |
|       19 |  6741 | `	a = pTos->x.iVal;` |
|       19 |  6742 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  6743 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  6744 | `		r = a << b;` |
|        5 |  6745 | `	}else{` |
|       11 |  6746 | `		r = a >> b;` |
|        - |  6747 | `	}` |
|        - |  6748 | `	/* Push the result */` |
|       19 |  6749 | `	pNos->x.iVal = r;` |
|       19 |  6750 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  6751 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6752 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  6753 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  6754 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  6755 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  6756 | `	}` |
|       19 |  6757 | `	VmPopOperand(&pTos,1);` |
|       19 |  6758 | `	break;` |
|        - |  6759 | `				 }` |
|        - |  6760 | `/* CAT:  P1 * *` |
|        - |  6761 | ` *` |
|        - |  6762 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  6763 | ` * back.` |
|        - |  6764 | ` */` |
|    71981 |  6765 | `case PH7_OP_CAT:{` |
|        - |  6766 | `	ph7_value *pNos,*pCur;` |
|   143964 |  6767 | `	if( pInstr->iP1 < 1 ){` |
|   116478 |  6768 | `		pNos = &pTos[-1];` |
|    58240 |  6769 | `	}else{` |
|    27488 |  6770 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6771 | `	}` |
|        - |  6772 | `#ifdef UNTRUST` |
|        - |  6773 | `	if( pNos < pStack ){` |
|        - |  6774 | `		goto Abort;` |
|        - |  6775 | `	}` |
|        - |  6776 | `#endif` |
|        - |  6777 | `	/* Force a string cast */` |
|   143964 |  6778 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1672 |  6779 | `		PH7_MemObjToString(pNos);` |
|      835 |  6780 | `	}` |
|   143964 |  6781 | `	pCur = &pNos[1];` |
|   290656 |  6782 | `	while( pCur <= pTos ){` |
|   146694 |  6783 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50962 |  6784 | `			PH7_MemObjToString(pCur);` |
|    25480 |  6785 | `		}` |
|        - |  6786 | `		/* Perform the concatenation */` |
|   146694 |  6787 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   146650 |  6788 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - |  6789 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 |  6790 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6791 | `				goto Abort;` |
|        - |  6792 | `			}` |
|    73324 |  6793 | `		}` |
|   146694 |  6794 | `		SyBlobRelease(&pCur->sBlob);` |
|   146694 |  6795 | `		pCur++;` |
|        2 |  6796 | `	}` |
|   143964 |  6797 | `	pTos = pNos;` |
|   143964 |  6798 | `	break;` |
|        - |  6799 | `				}` |
|        - |  6800 | `/*  CAT_STORE: * * *` |
|        - |  6801 | ` *` |
|        - |  6802 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6803 | ` * back.` |
|        - |  6804 | ` */` |
|     4149 |  6805 | `case PH7_OP_CAT_STORE:{` |
|     8300 |  6806 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6807 | `	ph7_value *pObj;` |
|        - |  6808 | `	sxu32 nIdx;` |
|        - |  6809 | `#ifdef UNTRUST` |
|        - |  6810 | `	if( pNos < pStack ){` |
|        - |  6811 | `		goto Abort;` |
|        - |  6812 | `	}` |
|        - |  6813 | `#endif` |
|        - |  6814 | `	/* The right operand must be a string to append it */` |
|     8300 |  6815 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6816 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6817 | `	}` |
|     8300 |  6818 | `	nIdx = pTos->nIdx;` |
|        - |  6819 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - |  6820 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - |  6821 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - |  6822 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - |  6823 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - |  6824 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - |  6825 | `	 * the source we copy from — references share the slot index, so one check` |
|        - |  6826 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - |  6827 | `	 * must run before any mutation (left to the slow path).` |
|        - |  6828 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - |  6829 | `	 * and remains O(n^2) by design. */` |
|     8301 |  6830 | `	if( nIdx != SXU32_HIGH` |
|     8298 |  6831 | `	 && nIdx != pNos->nIdx` |
|     8294 |  6832 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|     8292 |  6833 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|     4148 |  6834 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|     8286 |  6835 | `		if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6836 | `			/* e.g. $x = 5; $x .= "a";  ->  "5a" */` |
|        3 |  6837 | `			PH7_MemObjToString(pObj);` |
|        1 |  6838 | `		}` |
|     8286 |  6839 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8284 |  6840 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  6841 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - |  6842 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 |  6843 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6844 | `				goto Abort;` |
|        - |  6845 | `			}` |
|     4141 |  6846 | `		}` |
|        - |  6847 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - |  6848 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - |  6849 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - |  6850 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - |  6851 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - |  6852 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - |  6853 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - |  6854 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - |  6855 | `		 * the same slot is appended to again later in the statement` |
|        - |  6856 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - |  6857 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - |  6858 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|     8286 |  6859 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 |  6860 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 |  6861 | `		}` |
|     8286 |  6862 | `		pNos->nIdx = SXU32_HIGH;` |
|     8286 |  6863 | `		VmPopOperand(&pTos,1);` |
|     8293 |  6864 | `		break;` |
|        - |  6865 | `	}` |
|        - |  6866 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|       16 |  6867 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6868 | `		/* Force a string cast */` |
|        6 |  6869 | `		PH7_MemObjToString(pTos);` |
|        2 |  6870 | `	}` |
|        - |  6871 | `	/* Perform the concatenation (Reverse order) */` |
|       16 |  6872 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 |  6873 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  6874 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - |  6875 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 |  6876 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6877 | `			goto Abort;` |
|        - |  6878 | `		}` |
|        7 |  6879 | `	}` |
|        - |  6880 | `	/* Perform the store operation */` |
|       16 |  6881 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6882 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 |  6883 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       16 |  6884 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 |  6885 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 |  6886 | `	}` |
|       11 |  6887 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 |  6888 | `	VmPopOperand(&pTos,1);` |
|       11 |  6889 | `	break;` |
|        - |  6890 | `				}` |
|        - |  6891 | `/* OP_AND: * * *` |
|        - |  6892 | ` *` |
|        - |  6893 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  6894 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6895 | ` * stack.` |
|        - |  6896 | ` */` |
|        - |  6897 | `/* OP_OR: * * *` |
|        - |  6898 | ` *` |
|        - |  6899 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  6900 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6901 | ` * stack.` |
|        - |  6902 | ` */` |
|   108470 |  6903 | `case PH7_OP_LAND:` |
|        - |  6904 | `case PH7_OP_LOR: {` |
|   216986 |  6905 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6906 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  6907 | `#ifdef UNTRUST` |
|        - |  6908 | `	if( pNos < pStack ){` |
|        - |  6909 | `		goto Abort;` |
|        - |  6910 | `	}` |
|        - |  6911 | `#endif` |
|        - |  6912 | `	/* Force a boolean cast */` |
|   216986 |  6913 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  6914 | `		PH7_MemObjToBool(pTos);` |
|        1 |  6915 | `	}` |
|   216986 |  6916 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6917 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6918 | `	}` |
|   216986 |  6919 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   216986 |  6920 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   216986 |  6921 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  6922 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    99762 |  6923 | `		v1 = and_logic[v1*3+v2];` |
|    49904 |  6924 | `	}else{` |
|        - |  6925 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   117226 |  6926 | `		v1 = or_logic[v1*3+v2];` |
|        - |  6927 | `	}` |
|   216986 |  6928 | `	if( v1 == 2 ){` |
|      ! 0 |  6929 | `		v1 = 1;` |
|      ! 0 |  6930 | `	}` |
|   216986 |  6931 | `	VmPopOperand(&pTos,1);` |
|   216986 |  6932 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   216986 |  6933 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   216986 |  6934 | `	break;` |
|        - |  6935 | `				 }` |
|        - |  6936 | `/*` |
|        - |  6937 | ` * OP_NULLC: * * *` |
|        - |  6938 | ` * Null coalescing operator '??'.` |
|        - |  6939 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  6940 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  6941 | ` */` |
|        - |  6942 | `/*` |
|        - |  6943 | ` * OP_NULLC: * P2 *` |
|        - |  6944 | ` * Short-circuit null coalescing '??'.` |
|        - |  6945 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  6946 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  6947 | ` */` |
|       99 |  6948 | `case PH7_OP_NULLC: {` |
|        - |  6949 | `#ifdef UNTRUST` |
|        - |  6950 | `	if( pTos < pStack ){` |
|        - |  6951 | `		goto Abort;` |
|        - |  6952 | `	}` |
|        - |  6953 | `#endif` |
|      200 |  6954 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  6955 | `		/* Left is not null — keep it and skip the RHS */` |
|      120 |  6956 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       61 |  6957 | `	}else{` |
|        - |  6958 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       82 |  6959 | `		VmPopOperand(&pTos, 1);` |
|        - |  6960 | `	}` |
|      200 |  6961 | `	break;` |
|        - |  6962 |  |
|        - |  6963 | `/*` |
|        - |  6964 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  6965 | ` * Null coalescing assignment short-circuit.` |
|        - |  6966 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  6967 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  6968 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  6969 | ` */` |
|       28 |  6970 | `case PH7_OP_NULLC_JMP: {` |
|        - |  6971 | `#ifdef UNTRUST` |
|        - |  6972 | `	if( pTos < pStack ){` |
|        - |  6973 | `		goto Abort;` |
|        - |  6974 | `	}` |
|        - |  6975 | `#endif` |
|       58 |  6976 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  6977 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  6978 | `	}` |
|       58 |  6979 | `	break;` |
|        - |  6980 |  |
|        - |  6981 | `/*` |
|        - |  6982 | ` * OP_NULLC_STORE: * * *` |
|        - |  6983 | ` * Null coalescing assignment store.` |
|        - |  6984 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  6985 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  6986 | ` * expression result.` |
|        - |  6987 | ` */` |
|        - |  6988 | `/*` |
|        - |  6989 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  6990 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  6991 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  6992 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  6993 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  6994 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  6995 | ` */` |
|       51 |  6996 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  6997 | `#ifdef UNTRUST` |
|        - |  6998 | `	if( pTos < pStack ){` |
|        - |  6999 | `		goto Abort;` |
|        - |  7000 | `	}` |
|        - |  7001 | `#endif` |
|      104 |  7002 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  7003 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  7004 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  7005 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  7006 | `	}` |
|      104 |  7007 | `	break;` |
|        - |  7008 |  |
|       17 |  7009 | `case PH7_OP_NULLC_STORE: {` |
|       36 |  7010 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7011 | `	ph7_value *pObj;` |
|        - |  7012 | `	sxu32 nIdx;` |
|        - |  7013 | `#ifdef UNTRUST` |
|        - |  7014 | `	if( pNos < pStack ){` |
|        - |  7015 | `		goto Abort;` |
|        - |  7016 | `	}` |
|        - |  7017 | `#endif` |
|        - |  7018 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  7019 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  7020 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       36 |  7021 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  7022 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  7023 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  7024 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  7025 | `		ph7_value *apArg[2];` |
|        5 |  7026 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  7027 | `		apArg[1] = pTos;` |
|        5 |  7028 | `		if( pSet ){` |
|        5 |  7029 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  7030 | `		}` |
|        - |  7031 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  7032 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  7033 | `		VmPopOperand(&pTos,1);` |
|        - |  7034 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  7035 | `		VmCoalesceDisarm(pVm);` |
|        5 |  7036 | `		break;` |
|        - |  7037 | `	}` |
|       32 |  7038 | `	nIdx = pNos->nIdx;` |
|       32 |  7039 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  7040 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7041 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  7042 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  7043 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  7044 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  7045 | `	}` |
|       32 |  7046 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  7047 | `	VmPopOperand(&pTos,1);` |
|       32 |  7048 | `	break;` |
|        - |  7049 |  |
|        - |  7050 | `/*` |
|        - |  7051 | ` * OP_SPREAD: * * *` |
|        - |  7052 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  7053 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  7054 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  7055 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  7056 | ` */` |
|        9 |  7057 | `case PH7_OP_SPREAD: {` |
|        - |  7058 | `#ifdef UNTRUST` |
|        - |  7059 | `	if( pTos < pStack ){` |
|        - |  7060 | `		goto Abort;` |
|        - |  7061 | `	}` |
|        - |  7062 | `#endif` |
|       20 |  7063 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  7064 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  7065 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  7066 | `		if( nEntry == 0 ){` |
|        - |  7067 | `			/* Empty array — remove from stack */` |
|        3 |  7068 | `			VmPopOperand(&pTos, 1);` |
|        3 |  7069 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  7070 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  7071 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  7072 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  7073 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  7074 | `				VM_STACK_GUARD);` |
|      ! 0 |  7075 | `		}else{` |
|        - |  7076 | `			ph7_hashmap_node *pNode2;` |
|        - |  7077 | `			ph7_value *pElem;` |
|        - |  7078 | `			sxu32 i;` |
|        - |  7079 | `			/* Overwrite TOS with first element */` |
|       18 |  7080 | `			pNode2 = pMap->pFirst;` |
|       18 |  7081 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  7082 | `			PH7_MemObjRelease(pTos);` |
|       18 |  7083 | `			if( pElem ){` |
|       18 |  7084 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  7085 | `			}` |
|       18 |  7086 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7087 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  7088 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  7089 | `			pNode2 = pNode2->pPrev;` |
|        - |  7090 | `			/* Push remaining elements */` |
|       44 |  7091 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  7092 | `				pTos++;` |
|       28 |  7093 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  7094 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  7095 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  7096 | `				if( pElem ){` |
|       28 |  7097 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  7098 | `				}` |
|       28 |  7099 | `				pNode2 = pNode2->pPrev;` |
|       15 |  7100 | `			}` |
|       18 |  7101 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  7102 | `		}` |
|        9 |  7103 | `	}` |
|        - |  7104 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  7105 | `	break;` |
|        - |  7106 |  |
|        - |  7107 | `/*` |
|        - |  7108 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  7109 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  7110 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  7111 | ` */` |
|       34 |  7112 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  7113 | `#ifdef UNTRUST` |
|        - |  7114 | `	if( pTos < pStack ){` |
|        - |  7115 | `		goto Abort;` |
|        - |  7116 | `	}` |
|        - |  7117 | `#endif` |
|       70 |  7118 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       70 |  7119 | `	break;` |
|        - |  7120 |  |
|        - |  7121 | `/* OP_LXOR: * * *` |
|        - |  7122 | ` *` |
|        - |  7123 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  7124 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7125 | ` * stack.` |
|        - |  7126 | ` * According to the PHP language reference manual:` |
|        - |  7127 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  7128 | ` *  TRUE,but not both.` |
|        - |  7129 | ` */` |
|        5 |  7130 | `case PH7_OP_LXOR:{` |
|       11 |  7131 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  7132 | `	sxi32 v = 0;` |
|        - |  7133 | `#ifdef UNTRUST` |
|        - |  7134 | `	if( pNos < pStack ){` |
|        - |  7135 | `		goto Abort;` |
|        - |  7136 | `	}` |
|        - |  7137 | `#endif` |
|        - |  7138 | `	/* Force a boolean cast */` |
|       11 |  7139 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7140 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  7141 | `	}` |
|       11 |  7142 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7143 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7144 | `	}` |
|       11 |  7145 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  7146 | `		v = 1;` |
|        3 |  7147 | `	}` |
|       11 |  7148 | `	VmPopOperand(&pTos,1);` |
|       11 |  7149 | `	pTos->x.iVal = v;` |
|       11 |  7150 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  7151 | `	break;` |
|        - |  7152 | `				 }` |
|        - |  7153 | `/* OP_EQ P1 P2 P3` |
|        - |  7154 | ` *` |
|        - |  7155 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  7156 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7157 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7158 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7159 | ` */` |
|        - |  7160 | `/* OP_NEQ P1 P2 P3` |
|        - |  7161 | ` *` |
|        - |  7162 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  7163 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7164 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7165 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7166 | ` */` |
|     4588 |  7167 | `case PH7_OP_EQ:` |
|        - |  7168 | `case PH7_OP_NEQ: {` |
|     9178 |  7169 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7170 | `	/* Perform the comparison and act accordingly */` |
|        - |  7171 | `#ifdef UNTRUST` |
|        - |  7172 | `	if( pNos < pStack ){` |
|        - |  7173 | `		goto Abort;` |
|        - |  7174 | `	}` |
|        - |  7175 | `#endif` |
|     9178 |  7176 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9178 |  7177 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  7178 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9169 |  7179 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9134 |  7180 | `		rc = rc == 0;` |
|     4568 |  7181 | `	}else{` |
|       28 |  7182 | `		rc = rc != 0;` |
|        - |  7183 | `	}` |
|     9178 |  7184 | `	VmPopOperand(&pTos,1);` |
|     9178 |  7185 | `	if( !pInstr->iP2 ){` |
|        - |  7186 | `		/* Push comparison result without taking the jump */` |
|     9178 |  7187 | `		PH7_MemObjRelease(pTos);` |
|     9178 |  7188 | `		pTos->x.iVal = rc;` |
|        - |  7189 | `		/* Invalidate any prior representation */` |
|     9178 |  7190 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4590 |  7191 | `	}else{` |
|      ! 0 |  7192 | `		if( rc ){` |
|        - |  7193 | `			/* Jump to the desired location */` |
|      ! 0 |  7194 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7195 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7196 | `		}` |
|        - |  7197 | `	}` |
|     9178 |  7198 | `	break;` |
|        - |  7199 | `				 }` |
|        - |  7200 | `/* OP_TEQ P1 P2 *` |
|        - |  7201 | ` *` |
|        - |  7202 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  7203 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7204 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7205 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7206 | ` */` |
|   161875 |  7207 | `case PH7_OP_TEQ: {` |
|   323752 |  7208 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7209 | `	/* Perform the comparison and act accordingly */` |
|        - |  7210 | `#ifdef UNTRUST` |
|        - |  7211 | `	if( pNos < pStack ){` |
|        - |  7212 | `		goto Abort;` |
|        - |  7213 | `	}` |
|        - |  7214 | `#endif` |
|   323752 |  7215 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   323752 |  7216 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7217 | `		rc = 0;` |
|        2 |  7218 | `	}else{` |
|   323750 |  7219 | `		rc = rc == 0;` |
|        - |  7220 | `	}` |
|   323752 |  7221 | `	VmPopOperand(&pTos,1);` |
|   323752 |  7222 | `	if( !pInstr->iP2 ){` |
|        - |  7223 | `		/* Push comparison result without taking the jump */` |
|   323752 |  7224 | `		PH7_MemObjRelease(pTos);` |
|   323752 |  7225 | `		pTos->x.iVal = rc;` |
|        - |  7226 | `		/* Invalidate any prior representation */` |
|   323752 |  7227 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   161877 |  7228 | `	}else{` |
|      ! 0 |  7229 | `		if( rc ){` |
|        - |  7230 | `			/* Jump to the desired location */` |
|      ! 0 |  7231 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7232 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7233 | `		}` |
|        - |  7234 | `	}` |
|   323752 |  7235 | `	break;` |
|        - |  7236 | `				 }` |
|        - |  7237 | `/* OP_TNE P1 P2 *` |
|        - |  7238 | ` *` |
|        - |  7239 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  7240 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  7241 | ` * instruction.` |
|        - |  7242 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7243 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7244 | ` *` |
|        - |  7245 | ` */` |
|   124524 |  7246 | `case PH7_OP_TNE: {` |
|   249050 |  7247 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7248 | `	/* Perform the comparison and act accordingly */` |
|        - |  7249 | `#ifdef UNTRUST` |
|        - |  7250 | `	if( pNos < pStack ){` |
|        - |  7251 | `		goto Abort;` |
|        - |  7252 | `	}` |
|        - |  7253 | `#endif` |
|   249050 |  7254 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   249050 |  7255 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7256 | `		rc = 1;` |
|        2 |  7257 | `	}else{` |
|   249048 |  7258 | `		rc = rc != 0;` |
|        - |  7259 | `	}` |
|   249050 |  7260 | `	VmPopOperand(&pTos,1);` |
|   249050 |  7261 | `	if( !pInstr->iP2 ){` |
|        - |  7262 | `		/* Push comparison result without taking the jump */` |
|   249050 |  7263 | `		PH7_MemObjRelease(pTos);` |
|   249050 |  7264 | `		pTos->x.iVal = rc;` |
|        - |  7265 | `		/* Invalidate any prior representation */` |
|   249050 |  7266 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   124526 |  7267 | `	}else{` |
|      ! 0 |  7268 | `		if( rc ){` |
|        - |  7269 | `			/* Jump to the desired location */` |
|      ! 0 |  7270 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7271 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7272 | `		}` |
|        - |  7273 | `	}` |
|   249050 |  7274 | `	break;` |
|        - |  7275 | `				 }` |
|        - |  7276 | `/* OP_LT P1 P2 P3` |
|        - |  7277 | ` *` |
|        - |  7278 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7279 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7280 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7281 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7282 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7283 | ` *` |
|        - |  7284 | ` */` |
|        - |  7285 | `/* OP_LE P1 P2 P3` |
|        - |  7286 | ` *` |
|        - |  7287 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7288 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7289 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7290 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7291 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7292 | ` *` |
|        - |  7293 | ` */` |
|   112608 |  7294 | `case PH7_OP_LT:` |
|        - |  7295 | `case PH7_OP_LE: {` |
|   225262 |  7296 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7297 | `	/* Perform the comparison and act accordingly */` |
|        - |  7298 | `#ifdef UNTRUST` |
|        - |  7299 | `	if( pNos < pStack ){` |
|        - |  7300 | `		goto Abort;` |
|        - |  7301 | `	}` |
|        - |  7302 | `#endif` |
|   225262 |  7303 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   225262 |  7304 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7305 | `		rc = 0;` |
|   225258 |  7306 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1608 |  7307 | `		rc = rc < 1;` |
|      805 |  7308 | `	}else{` |
|   223648 |  7309 | `		rc = rc < 0;` |
|        - |  7310 | `	}` |
|   225262 |  7311 | `	VmPopOperand(&pTos,1);` |
|   225262 |  7312 | `	if( !pInstr->iP2 ){` |
|        - |  7313 | `		/* Push comparison result without taking the jump */` |
|   225262 |  7314 | `		PH7_MemObjRelease(pTos);` |
|   225262 |  7315 | `		pTos->x.iVal = rc;` |
|        - |  7316 | `		/* Invalidate any prior representation */` |
|   225262 |  7317 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112654 |  7318 | `	}else{` |
|      ! 0 |  7319 | `		if( rc ){` |
|        - |  7320 | `			/* Jump to the desired location */` |
|      ! 0 |  7321 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7322 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7323 | `		}` |
|        - |  7324 | `	}` |
|   225262 |  7325 | `	break;` |
|        - |  7326 | `				}` |
|        - |  7327 | `/* OP_GT P1 P2 P3` |
|        - |  7328 | ` *` |
|        - |  7329 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7330 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7331 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7332 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7333 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7334 | ` *` |
|        - |  7335 | ` */` |
|        - |  7336 | `/* OP_GE P1 P2 P3` |
|        - |  7337 | ` *` |
|        - |  7338 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7339 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7340 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7341 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7342 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7343 | ` *` |
|        - |  7344 | ` */` |
|    55701 |  7345 | `case PH7_OP_GT:` |
|        - |  7346 | `case PH7_OP_GE: {` |
|   111404 |  7347 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7348 | `	/* Perform the comparison and act accordingly */` |
|        - |  7349 | `#ifdef UNTRUST` |
|        - |  7350 | `	if( pNos < pStack ){` |
|        - |  7351 | `		goto Abort;` |
|        - |  7352 | `	}` |
|        - |  7353 | `#endif` |
|   111404 |  7354 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111404 |  7355 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7356 | `		rc = 0;` |
|   111400 |  7357 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110968 |  7358 | `		rc = rc >= 0;` |
|    55485 |  7359 | `	}else{` |
|      430 |  7360 | `		rc = rc > 0;` |
|        - |  7361 | `	}` |
|   111404 |  7362 | `	VmPopOperand(&pTos,1);` |
|   111404 |  7363 | `	if( !pInstr->iP2 ){` |
|        - |  7364 | `		/* Push comparison result without taking the jump */` |
|   111404 |  7365 | `		PH7_MemObjRelease(pTos);` |
|   111404 |  7366 | `		pTos->x.iVal = rc;` |
|        - |  7367 | `		/* Invalidate any prior representation */` |
|   111404 |  7368 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55703 |  7369 | `	}else{` |
|      ! 0 |  7370 | `		if( rc ){` |
|        - |  7371 | `			/* Jump to the desired location */` |
|      ! 0 |  7372 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7373 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7374 | `		}` |
|        - |  7375 | `	}` |
|   111404 |  7376 | `	break;` |
|        - |  7377 | `				}` |
|        - |  7378 | `/* OP_SPACESHIP * * *` |
|        - |  7379 | ` *` |
|        - |  7380 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  7381 | ` *   -1 if left < right` |
|        - |  7382 | ` *    0 if left == right` |
|        - |  7383 | ` *    1 if left > right` |
|        - |  7384 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  7385 | ` */` |
|       25 |  7386 | `case PH7_OP_SPACESHIP: {` |
|       51 |  7387 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7388 | `#ifdef UNTRUST` |
|        - |  7389 | `	if( pNos < pStack ){` |
|        - |  7390 | `		goto Abort;` |
|        - |  7391 | `	}` |
|        - |  7392 | `#endif` |
|       51 |  7393 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  7394 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  7395 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  7396 | `		rc = 1;` |
|        4 |  7397 | `	}else{` |
|        - |  7398 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  7399 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  7400 | `	}` |
|       51 |  7401 | `	VmPopOperand(&pTos,1);` |
|       51 |  7402 | `	PH7_MemObjRelease(pTos);` |
|       51 |  7403 | `	pTos->x.iVal = rc;` |
|       51 |  7404 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  7405 | `	break;` |
|        - |  7406 | `				}` |
|        - |  7407 | `/* OP_SEQ P1 P2 *` |
|        - |  7408 | ` * Strict string comparison.` |
|        - |  7409 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  7410 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7411 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7412 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7413 | ` * use PH7_OP_EQ.` |
|        - |  7414 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7415 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7416 | ` */` |
|        - |  7417 | `/* OP_SNE P1 P2 *` |
|        - |  7418 | ` * Strict string comparison.` |
|        - |  7419 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  7420 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7421 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7422 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7423 | ` * use PH7_OP_EQ.` |
|        - |  7424 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7425 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7426 | ` */` |
|       18 |  7427 | `case PH7_OP_SEQ:` |
|        - |  7428 | `case PH7_OP_SNE: {` |
|       38 |  7429 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7430 | `	SyString s1,s2;` |
|        - |  7431 | `	/* Perform the comparison and act accordingly */` |
|        - |  7432 | `#ifdef UNTRUST` |
|        - |  7433 | `	if( pNos < pStack ){` |
|        - |  7434 | `		goto Abort;` |
|        - |  7435 | `	}` |
|        - |  7436 | `#endif` |
|        - |  7437 | `	/* Force a string cast */` |
|       38 |  7438 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  7439 | `		PH7_MemObjToString(pTos);` |
|        2 |  7440 | `	}` |
|       38 |  7441 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7442 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7443 | `	}` |
|       38 |  7444 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  7445 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  7446 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  7447 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  7448 | `		rc = rc != 0;` |
|      ! 0 |  7449 | `	}else{` |
|       38 |  7450 | `		rc = rc == 0;` |
|        - |  7451 | `	}` |
|       38 |  7452 | `	VmPopOperand(&pTos,1);` |
|       38 |  7453 | `	if( !pInstr->iP2 ){` |
|        - |  7454 | `		/* Push comparison result without taking the jump */` |
|       38 |  7455 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7456 | `		pTos->x.iVal = rc;` |
|        - |  7457 | `		/* Invalidate any prior representation */` |
|       38 |  7458 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7459 | `	}else{` |
|      ! 0 |  7460 | `		if( rc ){` |
|        - |  7461 | `			/* Jump to the desired location */` |
|      ! 0 |  7462 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7463 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7464 | `		}` |
|        - |  7465 | `	}` |
|       38 |  7466 | `	break;` |
|        - |  7467 | `				 }` |
|        - |  7468 | `/*` |
|        - |  7469 | ` * OP_LOAD_REF * * *` |
|        - |  7470 | ` * Push the index of a referenced object on the stack.` |
|        - |  7471 | ` */` |
|       60 |  7472 | `case PH7_OP_LOAD_REF: {` |
|        - |  7473 | `	sxu32 nIdx;` |
|        - |  7474 | `#ifdef UNTRUST` |
|        - |  7475 | `	if( pTos < pStack ){` |
|        - |  7476 | `		goto Abort;` |
|        - |  7477 | `	}` |
|        - |  7478 | `#endif` |
|        - |  7479 | `	/* Extract memory object index */` |
|      121 |  7480 | `	nIdx = pTos->nIdx;` |
|      121 |  7481 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7482 | `		/* Nullify the object */` |
|      101 |  7483 | `		PH7_MemObjRelease(pTos);` |
|        - |  7484 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7485 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7486 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7487 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7488 | `	}` |
|      121 |  7489 | `	break;` |
|        - |  7490 | `					  }` |
|        - |  7491 | `/*` |
|        - |  7492 | ` * OP_STORE_REF * * P3` |
|        - |  7493 | ` * Perform an assignment operation by reference.` |
|        - |  7494 | ` */` |
|       18 |  7495 | ` case PH7_OP_STORE_REF: {` |
|       38 |  7496 | `	 SyString sName = { 0 , 0 };` |
|        - |  7497 | `	 VmFrame *pFrameLocal;` |
|        - |  7498 | `	SyHashEntry *pEntry;` |
|        - |  7499 | `	sxu32 nIdx;` |
|        - |  7500 | `#ifdef UNTRUST` |
|        - |  7501 | `	if( pTos < pStack ){` |
|        - |  7502 | `		goto Abort;` |
|        - |  7503 | `	}` |
|        - |  7504 | `#endif` |
|       38 |  7505 | `	if( pInstr->p3 == 0 ){` |
|        - |  7506 | `		char *zName;` |
|        - |  7507 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7508 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7509 | `			/* Force a string cast */` |
|      ! 0 |  7510 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7511 | `		}` |
|      ! 0 |  7512 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7513 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7514 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7515 | `			if( zName ){` |
|      ! 0 |  7516 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7517 | `			}` |
|      ! 0 |  7518 | `		}` |
|      ! 0 |  7519 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7520 | `		pTos--;` |
|      ! 0 |  7521 | `	}else{` |
|       38 |  7522 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7523 | `	}` |
|       38 |  7524 | `	nIdx = pTos->nIdx;` |
|       38 |  7525 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7526 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7527 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7528 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7529 | `		}else{` |
|        - |  7530 | `			ph7_value *pObj;` |
|        - |  7531 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7532 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7533 | `			if( pObj == 0 ){` |
|      ! 0 |  7534 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7535 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7536 | `				goto Abort;` |
|        - |  7537 | `			}` |
|        - |  7538 | `			/* Perform the store operation */` |
|      ! 0 |  7539 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7540 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7541 | `		}` |
|       38 |  7542 | `	}else if( sName.nByte > 0){` |
|       38 |  7543 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7544 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7545 | `		}else{` |
|       38 |  7546 | `			pFrameLocal = pVm->pFrame;` |
|       38 |  7547 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7548 | `			/* Query the local frame */` |
|       38 |  7549 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       38 |  7550 | `			if( pEntry ){` |
|      ! 0 |  7551 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7552 | `			}else{` |
|       38 |  7553 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       38 |  7554 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7555 | `					/* Insert in the $GLOBALS array */` |
|       34 |  7556 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       16 |  7557 | `				}` |
|       38 |  7558 | `				if( rc == SXRET_OK ){` |
|       38 |  7559 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       18 |  7560 | `				}` |
|        - |  7561 | `			}` |
|        - |  7562 | `		}` |
|       18 |  7563 | `	}` |
|       38 |  7564 | `	break;` |
|        - |  7565 | `				 }` |
|        - |  7566 | `/*` |
|        - |  7567 | ` * OP_UPLINK P1 * *` |
|        - |  7568 | ` * Link a variable to the top active VM frame.` |
|        - |  7569 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7570 | ` */` |
|       30 |  7571 | `case PH7_OP_UPLINK: {` |
|       62 |  7572 | `	if( pVm->pFrame->pParent ){` |
|       62 |  7573 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7574 | `		SyString sName;` |
|        - |  7575 | `		/* Perform the link */` |
|      132 |  7576 | `		while( pLink <= pTos ){` |
|       72 |  7577 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7578 | `				/* Force a string cast */` |
|      ! 0 |  7579 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7580 | `			}` |
|       72 |  7581 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       72 |  7582 | `			if( sName.nByte > 0 ){` |
|       72 |  7583 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  7584 | `			}` |
|       72 |  7585 | `			pLink++;` |
|        2 |  7586 | `		}` |
|       30 |  7587 | `	}` |
|       62 |  7588 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       62 |  7589 | `	break;` |
|        - |  7590 | `					}` |
|        - |  7591 | `/*` |
|        - |  7592 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7593 | ` * Push an exception in the corresponding container so that` |
|        - |  7594 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7595 | ` */` |
|      184 |  7596 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      370 |  7597 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7598 | `	VmFrame *pFrameLocal;` |
|        - |  7599 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      370 |  7600 | `	pException->iFinallyDone = 0;` |
|      370 |  7601 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7602 | `	/* Create the exception frame */` |
|      370 |  7603 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      370 |  7604 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7605 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7606 | `		goto Abort;` |
|        - |  7607 | `	}` |
|        - |  7608 | `	/* Mark the special frame */` |
|      370 |  7609 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      370 |  7610 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7611 | `	/* Point to the frame that trigger the exception */` |
|      370 |  7612 | `	pFrameLocal = pFrameLocal->pParent;` |
|      370 |  7613 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      370 |  7614 | `	pException->pFrame = pFrameLocal;` |
|      370 |  7615 | `	break;` |
|        - |  7616 | `							}` |
|        - |  7617 | `/*` |
|        - |  7618 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7619 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7620 | ` */` |
|      183 |  7621 | `case PH7_OP_POP_EXCEPTION: {` |
|      368 |  7622 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      368 |  7623 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7624 | `		ph7_exception **apException;` |
|        - |  7625 | `		/* Pop the loaded exception */` |
|       32 |  7626 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  7627 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  7628 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  7629 | `		}` |
|       15 |  7630 | `	}` |
|      368 |  7631 | `	pException->pFrame = 0;` |
|        - |  7632 | `	/* Leave the exception frame */` |
|      368 |  7633 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7634 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      368 |  7635 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7636 | `		sxi32 rcFinally;` |
|       20 |  7637 | `		pException->iFinallyDone = 1;` |
|       20 |  7638 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  7639 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7640 | `			goto Abort;` |
|        - |  7641 | `		}` |
|        9 |  7642 | `	}` |
|      368 |  7643 | `	break;` |
|        - |  7644 | `							}` |
|        - |  7645 |  |
|        - |  7646 | `/*` |
|        - |  7647 | ` * OP_THROW * P2 *` |
|        - |  7648 | ` * Throw an user exception.` |
|        - |  7649 | ` */` |
|       78 |  7650 | `case PH7_OP_THROW: {` |
|      158 |  7651 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      158 |  7652 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7653 | `#ifdef UNTRUST` |
|        - |  7654 | `	if( pTos < pStack ){` |
|        - |  7655 | `		goto Abort;` |
|        - |  7656 | `	}` |
|        - |  7657 | `#endif` |
|      158 |  7658 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7659 | `	/* Tell the upper layer that an exception was thrown */` |
|      158 |  7660 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      158 |  7661 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      158 |  7662 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7663 | `		ph7_class *pThrowable;` |
|        - |  7664 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      158 |  7665 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      159 |  7666 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7667 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7668 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7669 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7670 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7671 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7672 | `			if( pErrorClass ){` |
|        3 |  7673 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7674 | `			}` |
|        3 |  7675 | `			if( pErrInst ){` |
|        - |  7676 | `				ph7_class_method *pCons;` |
|        3 |  7677 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7678 | `				if( pCons ){` |
|        - |  7679 | `					ph7_value sArg;` |
|        - |  7680 | `					ph7_value *apArg[1];` |
|        - |  7681 | `					SyString sMsgStr;` |
|        - |  7682 | `					static const char zErrMsg[] =` |
|        - |  7683 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7684 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7685 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7686 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7687 | `					apArg[0] = &sArg;` |
|        3 |  7688 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7689 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7690 | `				}` |
|        3 |  7691 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7692 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7693 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7694 | `					goto Abort;` |
|        - |  7695 | `				}` |
|        2 |  7696 | `			}else{` |
|        - |  7697 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7698 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7699 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7700 | `					goto Abort;` |
|        - |  7701 | `				}` |
|        - |  7702 | `			}` |
|        2 |  7703 | `		}else{` |
|        - |  7704 | `			/* Throw the exception */` |
|      156 |  7705 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      156 |  7706 | `			if( rc == SXERR_ABORT ){` |
|        - |  7707 | `				/* Abort processing immediately */` |
|       11 |  7708 | `				goto Abort;` |
|        - |  7709 | `			}` |
|        - |  7710 | `		}` |
|       75 |  7711 | `	}else{` |
|        - |  7712 | `		/* Expecting a class instance */` |
|      ! 0 |  7713 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7714 | `		if( rc == SXERR_ABORT ){` |
|        - |  7715 | `			/* Abort processing immediately */` |
|      ! 0 |  7716 | `			goto Abort;` |
|        - |  7717 | `		}` |
|        - |  7718 | `	}` |
|        - |  7719 | `	/* Pop the top entry */` |
|      148 |  7720 | `	VmPopOperand(&pTos,1);` |
|        - |  7721 | `	/* Perform an unconditional jump */` |
|      148 |  7722 | `	pc = nJump - 1;` |
|      148 |  7723 | `	break;` |
|        - |  7724 | `				   }` |
|        - |  7725 | `/*` |
|        - |  7726 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7727 | ` * Prepare a foreach step.` |
|        - |  7728 | ` */` |
|     6189 |  7729 | `case PH7_OP_FOREACH_INIT: {` |
|    12380 |  7730 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7731 | `	void *pName;` |
|        - |  7732 | `#ifdef UNTRUST` |
|        - |  7733 | `	if( pTos < pStack ){` |
|        - |  7734 | `		goto Abort;` |
|        - |  7735 | `	}` |
|        - |  7736 | `#endif` |
|    12380 |  7737 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7738 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  7739 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7740 | `			/* Force a string cast */` |
|      ! 0 |  7741 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7742 | `		}` |
|        - |  7743 | `		/* Duplicate name */` |
|      ! 0 |  7744 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7745 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7746 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7747 | `		}` |
|      ! 0 |  7748 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7749 | `	}` |
|    12380 |  7750 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  7751 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7752 | `			/* Force a string cast */` |
|      ! 0 |  7753 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7754 | `		}` |
|        - |  7755 | `		/* Duplicate name */` |
|      ! 0 |  7756 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7757 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7758 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7759 | `		}` |
|      ! 0 |  7760 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7761 | `	}` |
|        - |  7762 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12380 |  7763 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7764 | `		/* Jump out of the loop */` |
|      ! 0 |  7765 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7766 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  7767 | `		}` |
|      ! 0 |  7768 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  7769 | `	}else{` |
|        - |  7770 | `		ph7_foreach_step *pStep;` |
|    12380 |  7771 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12380 |  7772 | `		if( pStep == 0 ){` |
|      ! 0 |  7773 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  7774 | `			/* Jump out of the loop */` |
|      ! 0 |  7775 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7776 | `		}else{` |
|        - |  7777 | `			/* Zero the structure */` |
|    12380 |  7778 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  7779 | `			/* Prepare the step */` |
|    12380 |  7780 | `			pStep->iFlags = pInfo->iFlags;` |
|    12380 |  7781 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7782 | `				ph7_hashmap *pMap;` |
|        - |  7783 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  7784 | `				 * source array so mutations don't affect other sharers. */` |
|    12346 |  7785 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  7786 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  7787 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  7788 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7789 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  7790 | `						 * variable still points at the same hashmap as` |
|        - |  7791 | `						 * the stack value. */` |
|        9 |  7792 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  7793 | `							pCur->iRef--;` |
|        - |  7794 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|        - |  7795 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|        - |  7796 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|        9 |  7797 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7798 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7799 | `						}` |
|        4 |  7800 | `					}` |
|        4 |  7801 | `				}` |
|    12346 |  7802 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7803 | `				/* Reset the internal loop cursor */` |
|    12346 |  7804 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7805 | `				/* Mark the step */` |
|    12346 |  7806 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12346 |  7807 | `				pStep->xIter.pMap = pMap;` |
|    12346 |  7808 | `				pMap->iRef++;` |
|     6174 |  7809 | `			}else{` |
|       36 |  7810 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7811 | `				ph7_class *pIteratorClass;` |
|        - |  7812 | `				/* Check if the object implements Iterator */` |
|       36 |  7813 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7814 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7815 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  7816 | `					ph7_class_method *pRewind;` |
|       24 |  7817 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  7818 | `					pStep->xIter.pThis = pThis;` |
|       24 |  7819 | `					pThis->iRef++;` |
|       24 |  7820 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  7821 | `					if( pRewind ){` |
|       24 |  7822 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  7823 | `					}` |
|       13 |  7824 | `				}else{` |
|        - |  7825 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  7826 | `					ph7_class *pIterAggClass;` |
|       14 |  7827 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  7828 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  7829 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  7830 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  7831 | `						ph7_class_method *pGetIter;` |
|        3 |  7832 | `						int iterAggOk = 0;` |
|        3 |  7833 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  7834 | `						if( pGetIter ){` |
|        - |  7835 | `							ph7_value sResult;` |
|        3 |  7836 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  7837 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  7838 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  7839 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  7840 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  7841 | `									ph7_class_method *pRewind;` |
|        3 |  7842 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  7843 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  7844 | `									pIterObj->iRef++;` |
|        - |  7845 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  7846 | `									pStep->pOwner = pThis;` |
|        3 |  7847 | `									pThis->iRef++;` |
|        3 |  7848 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  7849 | `									if( pRewind ){` |
|        3 |  7850 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  7851 | `									}` |
|        3 |  7852 | `									iterAggOk = 1;` |
|        1 |  7853 | `								}` |
|        1 |  7854 | `							}` |
|        3 |  7855 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7856 | `						}` |
|        3 |  7857 | `						if( !iterAggOk ){` |
|        - |  7858 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7859 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7860 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7861 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7862 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7863 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7864 | `						}` |
|        2 |  7865 | `					}else{` |
|        - |  7866 | `						/* Plain object iteration via hAttr */` |
|       12 |  7867 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  7868 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  7869 | `						pStep->xIter.pThis = pThis;` |
|       12 |  7870 | `						pThis->iRef++;` |
|        - |  7871 | `					}` |
|        - |  7872 | `				}` |
|        - |  7873 | `			}` |
|        - |  7874 | `		}` |
|    12380 |  7875 | `		if( pStep ){` |
|    12380 |  7876 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7877 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7878 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7879 | `				/* Jump out of the loop */` |
|      ! 0 |  7880 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7881 | `			}` |
|     6189 |  7882 | `		}` |
|        - |  7883 | `	}` |
|    12380 |  7884 | `	VmPopOperand(&pTos,1);` |
|    12380 |  7885 | `	break;` |
|        - |  7886 | `						  }` |
|        - |  7887 | `/*` |
|        - |  7888 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7889 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7890 | ` */` |
|   101721 |  7891 | `case PH7_OP_FOREACH_STEP: {` |
|   203444 |  7892 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7893 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7894 | `	ph7_value *pValue;` |
|        - |  7895 | `	VmFrame *pFrameLocal;` |
|        - |  7896 | `	/* Peek the last step */` |
|   203444 |  7897 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   203444 |  7898 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   203444 |  7899 | `	pFrameLocal = pVm->pFrame;` |
|   203444 |  7900 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   203444 |  7901 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   203310 |  7902 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  7903 | `		ph7_hashmap_node *pNode;` |
|        - |  7904 | `		/* Extract the current node value */` |
|   203310 |  7905 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   203310 |  7906 | `		if( pNode == 0 ){` |
|        - |  7907 | `			/* No more entry to process */` |
|    12344 |  7908 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12344 |  7909 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7910 | `				/* Break the reference with the last element */` |
|        7 |  7911 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  7912 | `			}` |
|        - |  7913 | `			/* Automatically reset the loop cursor */` |
|    12344 |  7914 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7915 | `			/* Cleanup the mess left behind */` |
|    12344 |  7916 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12344 |  7917 | `			SySetPop(&pInfo->aStep);` |
|    12344 |  7918 | `			PH7_HashmapUnref(pMap);` |
|     6173 |  7919 | `		}else{` |
|   190968 |  7920 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      528 |  7921 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      528 |  7922 | `				if( pKey ){` |
|      528 |  7923 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  7924 | `				}` |
|      263 |  7925 | `			}` |
|   190968 |  7926 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7927 | `				SyHashEntry *pEntry;` |
|        - |  7928 | `				/* Pass by reference */` |
|       23 |  7929 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  7930 | `				if( pEntry ){` |
|       21 |  7931 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  7932 | `				}else{` |
|        4 |  7933 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  7934 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  7935 | `				}` |
|       12 |  7936 | `			}else{` |
|        - |  7937 | `				/* Make a copy of the entry value */` |
|   190946 |  7938 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   190946 |  7939 | `				if( pValue ){` |
|   190946 |  7940 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    95472 |  7941 | `				}` |
|        - |  7942 | `			}` |
|        2 |  7943 | `		}` |
|   101790 |  7944 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  7945 | `		/* Iterator-based iteration.` |
|        - |  7946 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  7947 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  7948 | `		 */` |
|      106 |  7949 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  7950 | `		ph7_class_method *pMethod;` |
|        - |  7951 | `		ph7_value sResult;` |
|      106 |  7952 | `		int isValid = 0;` |
|        - |  7953 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  7954 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  7955 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  7956 | `		}else{` |
|       82 |  7957 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  7958 | `			if( pMethod ){` |
|       82 |  7959 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  7960 | `			}` |
|        - |  7961 | `		}` |
|        - |  7962 | `		/* Call valid() */` |
|      106 |  7963 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7964 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  7965 | `		if( pMethod ){` |
|      106 |  7966 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  7967 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  7968 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  7969 | `		}` |
|      106 |  7970 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  7971 | `		if( !isValid ){` |
|        - |  7972 | `			/* Iterator exhausted */` |
|       24 |  7973 | `			pc = pInstr->iP2 - 1;` |
|        - |  7974 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  7975 | `			if( pStep->pOwner ){` |
|        3 |  7976 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  7977 | `			}` |
|       24 |  7978 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  7979 | `			SySetPop(&pInfo->aStep);` |
|       24 |  7980 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  7981 | `		}else{` |
|        - |  7982 | `			/* Call current() to get value */` |
|       84 |  7983 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7984 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7985 | `			if( pMethod ){` |
|       84 |  7986 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7987 | `			}` |
|       84 |  7988 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7989 | `			if( pValue ){` |
|       84 |  7990 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7991 | `			}` |
|       84 |  7992 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7993 | `			/* Call key() if needed */` |
|       84 |  7994 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7995 | `				ph7_value sKey;` |
|       35 |  7996 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7997 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7998 | `				if( pMethod ){` |
|       35 |  7999 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  8000 | `				}` |
|       35 |  8001 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  8002 | `				if( pValue ){` |
|       35 |  8003 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  8004 | `				}` |
|       35 |  8005 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  8006 | `			}` |
|        - |  8007 | `		}` |
|       54 |  8008 | `	}else{` |
|       32 |  8009 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  8010 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  8011 | `		SyHashEntry *pEntry;` |
|        - |  8012 | `		/* Point to the next attribute */` |
|       36 |  8013 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  8014 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  8015 | `			/* Check access permission */` |
|       38 |  8016 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  8017 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  8018 | `					break; /* Access is granted */` |
|        - |  8019 | `			}` |
|        1 |  8020 | `		}` |
|       32 |  8021 | `		if( pEntry == 0 ){` |
|        - |  8022 | `			/* Clean up the mess left behind */` |
|       12 |  8023 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  8024 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8025 | `				/* Break the reference with the last element */` |
|        3 |  8026 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  8027 | `			}` |
|       12 |  8028 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  8029 | `			SySetPop(&pInfo->aStep);` |
|       12 |  8030 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  8031 | `		}else{` |
|       22 |  8032 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  8033 | `			ph7_value *pAttrValue;` |
|       22 |  8034 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  8035 | `				/* Fill with the current attribute name */` |
|       22 |  8036 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  8037 | `				if( pKey ){` |
|       22 |  8038 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  8039 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  8040 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  8041 | `				}` |
|       10 |  8042 | `			}` |
|        - |  8043 | `			/* Extract attribute value */` |
|       22 |  8044 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  8045 | `			if( pAttrValue ){` |
|       22 |  8046 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8047 | `					/* Pass by reference */` |
|        3 |  8048 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  8049 | `					if( pEntry ){` |
|        3 |  8050 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  8051 | `					}else{` |
|      ! 0 |  8052 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  8053 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  8054 | `					}` |
|        2 |  8055 | `				}else{` |
|        - |  8056 | `					/* Make a copy of the attribute value */` |
|       20 |  8057 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  8058 | `					if( pValue ){` |
|       20 |  8059 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  8060 | `					}` |
|        - |  8061 | `				}` |
|       10 |  8062 | `			}` |
|        - |  8063 | `		}` |
|        - |  8064 | `	}` |
|   203444 |  8065 | `	break;` |
|        - |  8066 | `						  }` |
|        - |  8067 | `/*` |
|        - |  8068 | ` * OP_MEMBER P1 P2` |
|        - |  8069 | ` * Load class attribute/method on the stack.` |
|        - |  8070 | ` */` |
|     4051 |  8071 | `case PH7_OP_MEMBER: {` |
|        - |  8072 | `	ph7_class_instance *pThis;` |
|        - |  8073 | `	ph7_value *pNos;` |
|        - |  8074 | `	SyString sName;` |
|     8104 |  8075 | `	if( !pInstr->iP1 ){` |
|     7864 |  8076 | `		pNos = &pTos[-1];` |
|        - |  8077 | `#ifdef UNTRUST` |
|        - |  8078 | `		if( pNos < pStack ){` |
|        - |  8079 | `			goto Abort;` |
|        - |  8080 | `		}` |
|        - |  8081 | `#endif` |
|     7864 |  8082 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8083 | `			ph7_class *pClass;` |
|        - |  8084 | `			/* Class already instantiated */` |
|     7862 |  8085 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  8086 | `			/* Point to the instantiated class */` |
|     7862 |  8087 | `			pClass = pThis->pClass;` |
|        - |  8088 | `			/* Extract attribute name first */` |
|     7862 |  8089 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7862 |  8090 | `			if( pInstr->iP2 ){` |
|        - |  8091 | `				/* Method call */` |
|      786 |  8092 | `				ph7_class_method *pMeth = 0;` |
|      786 |  8093 | `				if( sName.nByte > 0 ){` |
|        - |  8094 | `					/* Extract the target method */` |
|      786 |  8095 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      392 |  8096 | `				}` |
|      786 |  8097 | `				if( pMeth == 0 ){` |
|      ! 0 |  8098 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  8099 | `						&pClass->sName,&sName` |
|        - |  8100 | `						);` |
|        - |  8101 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  8102 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  8103 | `					/* Pop the method name from the stack */` |
|      ! 0 |  8104 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8105 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  8106 | `				}else{` |
|        - |  8107 | `					/* Push method name on the stack */` |
|      786 |  8108 | `					PH7_MemObjRelease(pTos);` |
|      786 |  8109 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      786 |  8110 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8111 | `				}` |
|      786 |  8112 | `				pTos->nIdx = SXU32_HIGH;` |
|      394 |  8113 | `			}else{` |
|        - |  8114 | `				/* Attribute access */` |
|     7078 |  8115 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  8116 | `				SyHashEntry *pEntry;` |
|        - |  8117 | `				/* Extract the target attribute */` |
|     7078 |  8118 | `				if( sName.nByte > 0 ){` |
|     7078 |  8119 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     7078 |  8120 | `					if( pEntry ){` |
|        - |  8121 | `						/* Point to the attribute value */` |
|     7076 |  8122 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3537 |  8123 | `					}` |
|     3538 |  8124 | `				}` |
|     7078 |  8125 | `				if( pObjAttr == 0 ){` |
|        - |  8126 | `					/* No such attribute,load null */` |
|        4 |  8127 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  8128 | `						&pClass->sName,&sName);` |
|        - |  8129 | `					/* Call the __get magic method if available */` |
|        3 |  8130 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  8131 | `				}` |
|     7078 |  8132 | `				VmPopOperand(&pTos,1);` |
|        - |  8133 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  8134 | `				 * This is due to the following case:` |
|        - |  8135 | `				 *     (new TestClass())->foo;` |
|        - |  8136 | `				 */` |
|     7078 |  8137 | `				pThis->iRef++;` |
|     7078 |  8138 | `				PH7_MemObjRelease(pTos);` |
|     7078 |  8139 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     7078 |  8140 | `				if( pObjAttr ){` |
|     7076 |  8141 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  8142 | `					/* Check attribute access */` |
|     7076 |  8143 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  8144 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  8145 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  8146 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  8147 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  8148 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     7074 |  8149 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3579 |  8150 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  8151 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  8152 | `							int bIsLhs = 0;` |
|       82 |  8153 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  8154 | `								bIsLhs = 1;` |
|       39 |  8155 | `							}` |
|       82 |  8156 | `							if( !bIsLhs ){` |
|        3 |  8157 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  8158 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  8159 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  8160 | `									goto Abort;` |
|        - |  8161 | `								}` |
|        - |  8162 | `								{` |
|        3 |  8163 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8164 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8165 | `										pc = pFrm2->iExceptionJump - 1;` |
|     4051 |  8166 | `										break;` |
|        - |  8167 | `									}` |
|        - |  8168 | `								}` |
|      ! 0 |  8169 | `								goto Exception;` |
|        - |  8170 | `							}` |
|       39 |  8171 | `						}` |
|        - |  8172 | `						/* Load attribute */` |
|     7074 |  8173 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     7074 |  8174 | `						if( pValue ){` |
|     7074 |  8175 | `							if( pThis->iRef < 2 ){` |
|        - |  8176 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  8177 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  8178 | `								 */` |
|        7 |  8179 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  8180 | `							}else{` |
|        - |  8181 | `								/* Simple load */` |
|     7068 |  8182 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  8183 | `							}` |
|     7074 |  8184 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     7072 |  8185 | `								if( pThis->iRef > 1 ){` |
|        - |  8186 | `									/* Load attribute index */` |
|     7066 |  8187 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3532 |  8188 | `								}` |
|     3535 |  8189 | `							}` |
|     3536 |  8190 | `						}` |
|     3538 |  8191 | `					}else{` |
|        - |  8192 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  8193 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  8194 | `						char zMsg[256];` |
|      ! 0 |  8195 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8196 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8197 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8198 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  8199 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8200 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8201 | `						goto Abort;` |
|        - |  8202 | `					}` |
|     3536 |  8203 | `				}` |
|        - |  8204 | `				/* Safely unreference the object */` |
|     7076 |  8205 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  8206 | `			}` |
|     3931 |  8207 | `		}else{` |
|        3 |  8208 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  8209 | `			VmPopOperand(&pTos,1);` |
|        3 |  8210 | `			PH7_MemObjRelease(pTos);` |
|        3 |  8211 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  8212 | `		}` |
|     3932 |  8213 | `	}else{` |
|        - |  8214 | `		/* Static member access using class name */` |
|      242 |  8215 | `		pNos = pTos;` |
|      242 |  8216 | `		pThis = 0;` |
|      242 |  8217 | `		if( !pInstr->p3 ){` |
|      192 |  8218 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      192 |  8219 | `			pNos--;` |
|        - |  8220 | `#ifdef UNTRUST` |
|        - |  8221 | `			if( pNos < pStack ){` |
|        - |  8222 | `				goto Abort;` |
|        - |  8223 | `			}` |
|        - |  8224 | `#endif` |
|       97 |  8225 | `		}else{` |
|        - |  8226 | `			/* Attribute name already computed */` |
|       52 |  8227 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  8228 | `		}` |
|      242 |  8229 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      242 |  8230 | `			ph7_class *pClass = 0;` |
|      242 |  8231 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8232 | `				/* Class already instantiated */` |
|        5 |  8233 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  8234 | `				pClass = pThis->pClass;` |
|        5 |  8235 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  8236 | `			}else{` |
|        - |  8237 | `				/* Try to extract the target class */` |
|      238 |  8238 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      238 |  8239 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      238 |  8240 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  8241 | `					/* Handle self/static/parent keywords */` |
|      238 |  8242 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  8243 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  8244 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  8245 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  8246 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  8247 | `						}` |
|      208 |  8248 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  8249 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      178 |  8250 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  8251 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  8252 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  8253 | `							pClass = pSelf->pBase;` |
|       13 |  8254 | `						}` |
|       15 |  8255 | `					}else{` |
|      126 |  8256 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  8257 | `					}` |
|      118 |  8258 | `				}` |
|        - |  8259 | `			}` |
|      242 |  8260 | `			if( pClass == 0 ){` |
|        - |  8261 | `				/* Undefined class */` |
|      ! 0 |  8262 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  8263 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  8264 | `					);` |
|      ! 0 |  8265 | `				if( !pInstr->p3 ){` |
|      ! 0 |  8266 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8267 | `				}` |
|      ! 0 |  8268 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  8269 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  8270 | `			}else{` |
|      242 |  8271 | `				if( pInstr->iP2 ){` |
|        - |  8272 | `					/* Method call */` |
|       86 |  8273 | `					ph7_class_method *pMeth = 0;` |
|       86 |  8274 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  8275 | `						/* Extract the target method */` |
|       86 |  8276 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  8277 | `					}` |
|       86 |  8278 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  8279 | `						if( pMeth ){` |
|      ! 0 |  8280 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  8281 | `								&pClass->sName,&sName` |
|        - |  8282 | `								);` |
|      ! 0 |  8283 | `						}else{` |
|      ! 0 |  8284 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8285 | `								&pClass->sName,&sName` |
|        - |  8286 | `								);` |
|        - |  8287 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  8288 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  8289 | `						}` |
|        - |  8290 | `						/* Pop the method name from the stack */` |
|      ! 0 |  8291 | `						if( !pInstr->p3 ){` |
|      ! 0 |  8292 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  8293 | `						}` |
|      ! 0 |  8294 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  8295 | `					}else{` |
|        - |  8296 | `						/* Push method name on the stack */` |
|       86 |  8297 | `						PH7_MemObjRelease(pTos);` |
|       86 |  8298 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  8299 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8300 | `					}` |
|       86 |  8301 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  8302 | `				}else{` |
|        - |  8303 | `					/* Attribute access */` |
|      158 |  8304 | `					ph7_class_attr *pAttr = 0;` |
|        - |  8305 | `					/* Check for special ::class pseudo-constant */` |
|      204 |  8306 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  8307 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  8308 | `						/* ::class returns the fully qualified class name */` |
|        - |  8309 | `						/* Pop the attribute name from the stack */` |
|       60 |  8310 | `						if( !pInstr->p3 ){` |
|       60 |  8311 | `							VmPopOperand(&pTos,1);` |
|       29 |  8312 | `						}` |
|       60 |  8313 | `						PH7_MemObjRelease(pTos);` |
|        - |  8314 | `						/* Load the class name */` |
|       60 |  8315 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  8316 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  8317 | `					}else{` |
|        - |  8318 | `						/* Extract the target attribute */` |
|      100 |  8319 | `						if( sName.nByte > 0 ){` |
|      100 |  8320 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       49 |  8321 | `						}` |
|      100 |  8322 | `						if( pAttr == 0 ){` |
|        - |  8323 | `							/* No such attribute,load null */` |
|      ! 0 |  8324 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8325 | `								&pClass->sName,&sName);` |
|        - |  8326 | `							/* Call the __get magic method if available */` |
|      ! 0 |  8327 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  8328 | `						}` |
|        - |  8329 | `						/* Pop the attribute name from the stack */` |
|      100 |  8330 | `						if( !pInstr->p3 ){` |
|       50 |  8331 | `							VmPopOperand(&pTos,1);` |
|       24 |  8332 | `						}` |
|      100 |  8333 | `						PH7_MemObjRelease(pTos);` |
|      100 |  8334 | `						pTos->nIdx = SXU32_HIGH;` |
|      100 |  8335 | `						if( pAttr ){` |
|      100 |  8336 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  8337 | `								/* Access to a non static attribute */` |
|      ! 0 |  8338 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8339 | `									&pClass->sName,&pAttr->sName` |
|        - |  8340 | `									);` |
|      ! 0 |  8341 | `							}else{` |
|        - |  8342 | `								ph7_value *pValue;` |
|        - |  8343 | `								/* Check if the access to the attribute is allowed */` |
|      100 |  8344 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  8345 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  8346 | `									 * Same LHS-of-store peek as the instance path. */` |
|       94 |  8347 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       68 |  8348 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       59 |  8349 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       38 |  8350 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       40 |  8351 | `										if( pS ){` |
|       40 |  8352 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       40 |  8353 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  8354 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  8355 | `												int bIsLhs = 0;` |
|        8 |  8356 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  8357 | `													bIsLhs = 1;` |
|        2 |  8358 | `												}` |
|        8 |  8359 | `												if( !bIsLhs ){` |
|        3 |  8360 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  8361 | `													if( pThis ){` |
|      ! 0 |  8362 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8363 | `													}` |
|        3 |  8364 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  8365 | `														goto Abort;` |
|        - |  8366 | `													}` |
|        - |  8367 | `													{` |
|        3 |  8368 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8369 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8370 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  8371 | `															break;` |
|        - |  8372 | `														}` |
|        - |  8373 | `													}` |
|      ! 0 |  8374 | `													goto Exception;` |
|        - |  8375 | `												}` |
|        2 |  8376 | `											}` |
|       18 |  8377 | `										}` |
|       18 |  8378 | `									}` |
|        - |  8379 | `									/* Load the desired attribute */` |
|       94 |  8380 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       94 |  8381 | `									if( pValue ){` |
|       94 |  8382 | `										PH7_MemObjLoad(pValue,pTos);` |
|       94 |  8383 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  8384 | `											/* Load index number */` |
|       50 |  8385 | `											pTos->nIdx = pAttr->nIdx;` |
|       24 |  8386 | `										}` |
|       46 |  8387 | `									}` |
|       48 |  8388 | `								}else{` |
|        - |  8389 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  8390 | `									char zMsg[256];` |
|        5 |  8391 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  8392 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  8393 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  8394 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  8395 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  8396 | `									}else{` |
|      ! 0 |  8397 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8398 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8399 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  8400 | `									}` |
|        5 |  8401 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  8402 | `									goto Abort;` |
|        - |  8403 | `								}` |
|        - |  8404 | `							}` |
|       46 |  8405 | `						}` |
|        - |  8406 | `					}` |
|        - |  8407 | `				}` |
|      236 |  8408 | `				if( pThis ){` |
|        - |  8409 | `					/* Safely unreference the object */` |
|        5 |  8410 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  8411 | `				}` |
|        - |  8412 | `			}` |
|      119 |  8413 | `		}else{` |
|        - |  8414 | `			/* Pop operands */` |
|      ! 0 |  8415 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  8416 | `			if( !pInstr->p3 ){` |
|      ! 0 |  8417 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  8418 | `			}` |
|      ! 0 |  8419 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8420 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  8421 | `		}` |
|        - |  8422 | `	}` |
|     8096 |  8423 | `	break;` |
|        - |  8424 | `					}` |
|        - |  8425 | `/*` |
|        - |  8426 | ` * OP_NEW P1 * * *` |
|        - |  8427 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  8428 | ` */` |
|      664 |  8429 | `case PH7_OP_NEW: {` |
|     1330 |  8430 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1330 |  8431 | `	ph7_class *pClass = 0;` |
|        - |  8432 | `	ph7_class_instance *pNew;` |
|     1330 |  8433 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  8434 | `		/* Try to extract the desired class */` |
|     1994 |  8435 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1328 |  8436 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      664 |  8437 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8438 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8439 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8440 | `	}` |
|     1330 |  8441 | `	if( pClass == 0 ){` |
|        - |  8442 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8443 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8444 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8445 | `			);` |
|        - |  8446 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8447 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8448 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8449 | `			/* Pop given arguments */` |
|      ! 0 |  8450 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8451 | `		}` |
|      ! 0 |  8452 | `		goto Abort;` |
|      ! 0 |  8453 | `	}else{` |
|        - |  8454 | `		ph7_class_method *pCons;` |
|        - |  8455 | `		/* Create a new class instance */` |
|     1330 |  8456 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1330 |  8457 | `		if( pNew == 0 ){` |
|      ! 0 |  8458 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8459 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8460 | `				&pClass->sName` |
|        - |  8461 | `			);` |
|      ! 0 |  8462 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8463 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8464 | `				/* Pop given arguments */` |
|      ! 0 |  8465 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8466 | `			}` |
|      ! 0 |  8467 | `			break;` |
|        - |  8468 | `		}` |
|        - |  8469 | `		/* Check if a constructor is available */` |
|     1330 |  8470 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1330 |  8471 | `		if( pCons == 0 ){` |
|      934 |  8472 | `			SyString *pName = &pClass->sName;` |
|        - |  8473 | `			/* Check for a constructor with the same base class name */` |
|      934 |  8474 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      466 |  8475 | `		}` |
|     1330 |  8476 | `		if( pCons ){` |
|        - |  8477 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8478 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8479 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8480 | `			 * (including variadic string-key packing). */` |
|      398 |  8481 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8482 | `			sxi32 rcCons;` |
|      398 |  8483 | `			SySetReset(&aArg);` |
|      778 |  8484 | `			while( pArg < pTos ){` |
|      382 |  8485 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      382 |  8486 | `				pArg++;` |
|        2 |  8487 | `			}` |
|      398 |  8488 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8489 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8490 | `				sxu32 n;` |
|      114 |  8491 | `				n = SySetUsed(&aArg);` |
|        - |  8492 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8493 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8494 | `				 * after resolution). */` |
|      222 |  8495 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      110 |  8496 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      110 |  8497 | `					if( pFuncArg ){` |
|      110 |  8498 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  8499 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8500 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8501 | `						}` |
|       54 |  8502 | `					}` |
|      110 |  8503 | `					n++;` |
|        2 |  8504 | `				}` |
|       56 |  8505 | `			}` |
|      398 |  8506 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8507 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      398 |  8508 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8509 | `				pNew->iRef = 1;` |
|      ! 0 |  8510 | `			}` |
|      398 |  8511 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8512 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8513 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8514 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8515 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8516 | `				sxi32 iResumePc;` |
|        5 |  8517 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8518 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8519 | `					goto Abort;` |
|        - |  8520 | `				}` |
|        5 |  8521 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8522 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8523 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8524 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8525 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8526 | `					}` |
|        5 |  8527 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8528 | `					pc = iResumePc;` |
|        5 |  8529 | `					break;` |
|        - |  8530 | `				}` |
|      ! 0 |  8531 | `				goto Exception;` |
|        - |  8532 | `			}` |
|      196 |  8533 | `		}` |
|     1326 |  8534 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8535 | `			/* Pop given arguments */` |
|      312 |  8536 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      155 |  8537 | `		}` |
|     1326 |  8538 | `		PH7_MemObjRelease(pTos);` |
|     1326 |  8539 | `		pTos->x.pOther = pNew;` |
|     1326 |  8540 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8541 | `	}` |
|     1326 |  8542 | `	break;` |
|        - |  8543 | `				 }` |
|        - |  8544 | `/*` |
|        - |  8545 | ` * OP_CLONE * * *` |
|        - |  8546 | ` * Perfome a clone operation.` |
|        - |  8547 | ` */` |
|       24 |  8548 | `case PH7_OP_CLONE: {` |
|        - |  8549 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8550 | `#ifdef UNTRUST` |
|        - |  8551 | `	if( pTos < pStack ){` |
|        - |  8552 | `		goto Abort;` |
|        - |  8553 | `	}` |
|        - |  8554 | `#endif` |
|        - |  8555 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8556 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8557 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8558 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8559 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8560 | `		break;` |
|        - |  8561 | `	}` |
|        - |  8562 | `	/* Point to the source */` |
|       46 |  8563 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8564 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8565 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8566 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8567 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8568 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8569 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8570 | `		break;` |
|        - |  8571 | `	}` |
|        - |  8572 | `	/* Perform the clone operation */` |
|       46 |  8573 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8574 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8575 | `	if( pClone == 0 ){` |
|      ! 0 |  8576 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8577 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8578 | `	}else{` |
|        - |  8579 | `		/* Load the cloned object */` |
|       46 |  8580 | `		pTos->x.pOther = pClone;` |
|       46 |  8581 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8582 | `	}` |
|       46 |  8583 | `	break;` |
|        - |  8584 | `				   }` |
|        - |  8585 | `/*` |
|        - |  8586 | ` * OP_SWITCH * * P3` |
|        - |  8587 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8588 | ` */` |
|       26 |  8589 | `case PH7_OP_SWITCH: {` |
|       54 |  8590 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8591 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8592 | `	ph7_value sValue,sCaseValue;` |
|        - |  8593 | `	sxu32 n,nEntry;` |
|        - |  8594 | `#ifdef UNTRUST` |
|        - |  8595 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8596 | `		goto Abort;` |
|        - |  8597 | `	}` |
|        - |  8598 | `#endif` |
|        - |  8599 | `	/* Point to the case table  */` |
|       54 |  8600 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8601 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8602 | `	/* Select the appropriate case block to execute */` |
|       54 |  8603 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8604 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8605 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8606 | `		pCase = &aCase[n];` |
|      130 |  8607 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8608 | `		/* Execute the case expression first */` |
|      130 |  8609 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8610 | `		/* Compare the two expression */` |
|      130 |  8611 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8612 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8613 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8614 | `		if( rc == 0 ){` |
|        - |  8615 | `			/* Value match,jump to this block */` |
|       52 |  8616 | `			pc = pCase->nStart - 1;` |
|       52 |  8617 | `			break;` |
|        - |  8618 | `		}` |
|       41 |  8619 | `	}` |
|       54 |  8620 | `	VmPopOperand(&pTos,1);` |
|       54 |  8621 | `	if( n >= nEntry ){` |
|        - |  8622 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8623 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8624 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8625 | `		}else{` |
|        - |  8626 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8627 | `			pc = pSwitch->nOut - 1;` |
|        - |  8628 | `		}` |
|        1 |  8629 | `	}` |
|       54 |  8630 | `	break;` |
|        - |  8631 | `					}` |
|        - |  8632 | `/*` |
|        - |  8633 | ` * OP_MATCH * * P3` |
|        - |  8634 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8635 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8636 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8637 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8638 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8639 | ` */` |
|       54 |  8640 | `case PH7_OP_MATCH: {` |
|      110 |  8641 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8642 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8643 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8644 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8645 | `	int matched = 0;` |
|        - |  8646 | `#ifdef UNTRUST` |
|        - |  8647 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8648 | `		goto Abort;` |
|        - |  8649 | `	}` |
|        - |  8650 | `#endif` |
|      110 |  8651 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8652 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8653 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8654 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8655 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8656 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8657 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8658 | `		pArm = &aArm[i];` |
|      240 |  8659 | `		if( pArm->bDefault ){` |
|       13 |  8660 | `			pDefault = pArm;` |
|       13 |  8661 | `			continue;` |
|        - |  8662 | `		}` |
|      228 |  8663 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8664 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8665 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8666 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8667 | `				continue;` |
|        - |  8668 | `			}` |
|      260 |  8669 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8670 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8671 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8672 | `			if( rc == 0 ){` |
|       93 |  8673 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8674 | `				matched = 1;` |
|       93 |  8675 | `				break;` |
|        - |  8676 | `			}` |
|       85 |  8677 | `		}` |
|      115 |  8678 | `	}` |
|      110 |  8679 | `	if( !matched && pDefault ){` |
|       13 |  8680 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8681 | `		matched = 1;` |
|        6 |  8682 | `	}` |
|      110 |  8683 | `	if( !matched ){` |
|        5 |  8684 | `		const char *zType = "unknown";` |
|        - |  8685 | `		char zMsg[128];` |
|        - |  8686 | `		sxu32 nMsg;` |
|        5 |  8687 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8688 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8689 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8690 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8691 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8692 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8693 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8694 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8695 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8696 | `		default: break;` |
|        - |  8697 | `		}` |
|        7 |  8698 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8699 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8700 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8701 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8702 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8703 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8704 | `		goto Abort;` |
|        - |  8705 | `	}` |
|      105 |  8706 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8707 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8708 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8709 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8710 | `	break;` |
|        - |  8711 | `					}` |
|        - |  8712 | `/*` |
|        - |  8713 | ` * OP_YIELD P1 P2 *` |
|        - |  8714 | ` *  Yield a value from a generator function.` |
|        - |  8715 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8716 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8717 | ` */` |
|       34 |  8718 | `case PH7_OP_YIELD: {` |
|        - |  8719 | `	ph7_generator *pGen;` |
|       70 |  8720 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8721 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8722 | `		goto Abort;` |
|        - |  8723 | `	}` |
|       70 |  8724 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  8725 | `	if( pInstr->iP2 ){` |
|        - |  8726 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8727 | `#ifdef UNTRUST` |
|        - |  8728 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8729 | `#endif` |
|        7 |  8730 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  8731 | `		VmPopOperand(&pTos, 1);` |
|        7 |  8732 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  8733 | `		VmPopOperand(&pTos, 1);` |
|        - |  8734 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  8735 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8736 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8737 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8738 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8739 | `			}` |
|        1 |  8740 | `		}` |
|       67 |  8741 | `	}else if( pInstr->iP1 ){` |
|        - |  8742 | `		/* yield $value */` |
|        - |  8743 | `#ifdef UNTRUST` |
|        - |  8744 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8745 | `#endif` |
|       64 |  8746 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  8747 | `		VmPopOperand(&pTos, 1);` |
|        - |  8748 | `		/* Auto-increment key */` |
|       64 |  8749 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  8750 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  8751 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  8752 | `	}else{` |
|        - |  8753 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8754 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8755 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8756 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8757 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8758 | `	}` |
|        - |  8759 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  8760 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  8761 | `	goto Suspend;` |
|        - |  8762 |  |
|        - |  8763 | `/*` |
|        - |  8764 | ` * OP_CALL P1 * *` |
|        - |  8765 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8766 | ` *  function on the stack.` |
|        - |  8767 | ` */` |
|   358472 |  8768 | `case PH7_OP_CALL: {` |
|   716990 |  8769 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8770 | `	ph7_value *pArg;` |
|   716990 |  8771 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   716990 |  8772 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8773 | `	SyHashEntry *pEntry;` |
|        - |  8774 | `	SyString sName;` |
|        - |  8775 | `	/* Extract function name */` |
|   716990 |  8776 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  8777 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8778 | `			ph7_value sResult;` |
|        - |  8779 | `			sxi32 rcArr;` |
|        3 |  8780 | `			SySetReset(&aArg);` |
|        3 |  8781 | `			while( pArg < pTos ){` |
|      ! 0 |  8782 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8783 | `				pArg++;` |
|      ! 0 |  8784 | `			}` |
|        3 |  8785 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8786 | `			/* May be a class instance and it's static method */` |
|        3 |  8787 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  8788 | `			SySetReset(&aArg);` |
|        - |  8789 | `			/* Pop given arguments */` |
|        3 |  8790 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8791 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8792 | `			}` |
|        3 |  8793 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  8794 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8795 | `				goto Abort;` |
|        - |  8796 | `			}` |
|        3 |  8797 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  8798 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  8799 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  8800 | `				sxi32 iResumePc;` |
|        3 |  8801 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  8802 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  8803 | `					PH7_MemObjRelease(pTos);` |
|        3 |  8804 | `					pc = iResumePc;` |
|        3 |  8805 | `					break;` |
|        - |  8806 | `				}` |
|      ! 0 |  8807 | `				goto Exception;` |
|        - |  8808 | `			}` |
|        - |  8809 | `			/* Copy result */` |
|      ! 0 |  8810 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8811 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  8812 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  8813 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8814 | `			ph7_value sResult;` |
|        - |  8815 | `			sxi32 rcInv;` |
|       84 |  8816 | `			SySetReset(&aArg);` |
|      200 |  8817 | `			while( pArg < pTos ){` |
|      118 |  8818 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  8819 | `				pArg++;` |
|        2 |  8820 | `			}` |
|       84 |  8821 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  8822 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  8823 | `				(int)SySetUsed(&aArg),` |
|       82 |  8824 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  8825 | `				&sResult,` |
|       82 |  8826 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  8827 | `			SySetReset(&aArg);` |
|       84 |  8828 | `			if( nCallArgs > 0 ){` |
|       76 |  8829 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  8830 | `			}` |
|       84 |  8831 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  8832 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  8833 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  8834 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  8835 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  8836 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  8837 | `				pThis->iRef++;` |
|       13 |  8838 | `				PH7_MemObjRelease(pTos);` |
|       13 |  8839 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  8840 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  8841 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8842 | `					goto Abort;` |
|        - |  8843 | `				}` |
|        - |  8844 | `				{` |
|       13 |  8845 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  8846 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  8847 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  8848 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  8849 | `						break;` |
|        - |  8850 | `					}` |
|        - |  8851 | `				}` |
|      ! 0 |  8852 | `				goto Exception;` |
|        - |  8853 | `			}` |
|       72 |  8854 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  8855 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8856 | `				goto Abort;` |
|        - |  8857 | `			}` |
|       72 |  8858 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  8859 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  8860 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  8861 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  8862 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  8863 | `				sxi32 iResumePc;` |
|        7 |  8864 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  8865 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  8866 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8867 | `					pc = iResumePc;` |
|        5 |  8868 | `					break;` |
|        - |  8869 | `				}` |
|        3 |  8870 | `				goto Exception;` |
|        - |  8871 | `			}` |
|       66 |  8872 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  8873 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  8874 | `		}else{` |
|        - |  8875 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  8876 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  8877 | `			/* Pop given arguments */` |
|      ! 0 |  8878 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8879 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8880 | `			}` |
|        - |  8881 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8882 | `			PH7_MemObjRelease(pTos);` |
|        - |  8883 | `		}` |
|       66 |  8884 | `		break;` |
|        - |  8885 | `	}` |
|   716906 |  8886 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  8887 | `	/* Check for a compiled function first.` |
|        - |  8888 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  8889 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   716906 |  8890 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8891 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  8892 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  8893 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  8894 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  8895 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  8896 | `	{` |
|   716906 |  8897 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   716906 |  8898 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  8899 | `		const char *zFunc;` |
|        - |  8900 | `		const char *zEnd;` |
|        - |  8901 | `		const char *z;` |
|        - |  8902 | `		SyString sGlobal;` |
|       22 |  8903 | `		zFunc = sName.zString;` |
|       22 |  8904 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  8905 | `		z = zEnd;` |
|        - |  8906 | `		/* Find last namespace separator */` |
|      194 |  8907 | `		while( z > zFunc ){` |
|      194 |  8908 | `			if( z[-1] == '\\' ){` |
|       22 |  8909 | `				break;` |
|        - |  8910 | `			}` |
|      174 |  8911 | `			z--;` |
|        2 |  8912 | `		}` |
|       22 |  8913 | `		if( z > zFunc && z < zEnd ){` |
|        - |  8914 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  8915 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  8916 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  8917 | `		}` |
|       10 |  8918 | `	}` |
|        - |  8919 | `	} /* end VmCallArgMap namespace scope */` |
|   716906 |  8920 | `	if( pEntry ){` |
|        - |  8921 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  8922 | `		ph7_class_instance *pThis;` |
|        - |  8923 | `		ph7_value *pFrameStack;` |
|        - |  8924 | `		ph7_vm_func *pVmFunc;` |
|        - |  8925 | `		ph7_class *pSelf;` |
|        - |  8926 | `		VmFrame *pFrame;` |
|        - |  8927 | `		ph7_value *pObj;` |
|        - |  8928 | `		VmSlot sArg;` |
|        - |  8929 | `		sxu32 n;` |
|        - |  8930 | `		/* initialize fields */` |
|    18670 |  8931 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    18670 |  8932 | `		pThis = 0;` |
|    18670 |  8933 | `		pSelf = 0;` |
|    18670 |  8934 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  8935 | `			ph7_class_method *pMeth;` |
|        - |  8936 | `			/* Class method call */` |
|     3352 |  8937 | `			ph7_value *pTarget = &pTos[-1];` |
|     3352 |  8938 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  8939 | `				/* Extract the 'this' pointer */` |
|     3352 |  8940 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  8941 | `					/* Instance already loaded */` |
|     3262 |  8942 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3262 |  8943 | `					pThis->iRef++;` |
|     3262 |  8944 | `					pSelf = pThis->pClass;` |
|     1630 |  8945 | `				}` |
|     3352 |  8946 | `				if( pSelf == 0 ){` |
|       92 |  8947 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  8948 | `						/* "Late Static Binding" class name */` |
|      128 |  8949 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  8950 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  8951 | `					}` |
|       92 |  8952 | `					if( pSelf == 0 ){` |
|       21 |  8953 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  8954 | `					}` |
|       45 |  8955 | `				}` |
|     3352 |  8956 | `				if( pThis == 0  ){` |
|       92 |  8957 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  8958 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  8959 | `					if( pFrameLocal->pParent ){` |
|        - |  8960 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  8961 | `						pThis = pFrameLocal->pThis;` |
|       66 |  8962 | `						if( pThis ){` |
|       21 |  8963 | `							pThis->iRef++;` |
|       10 |  8964 | `						}` |
|       32 |  8965 | `					}` |
|       45 |  8966 | `				}` |
|     3352 |  8967 | `				VmPopOperand(&pTos,1);` |
|     3352 |  8968 | `				PH7_MemObjRelease(pTos);` |
|        - |  8969 | `				/* Synchronize pointers */` |
|     3352 |  8970 | `				pArg = &pTos[-nCallArgs];` |
|        - |  8971 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  8972 | `				 * user have already computed the random generated unique class method name` |
|        - |  8973 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  8974 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  8975 | `				 */` |
|     3352 |  8976 | `				while( pArg < pStack ){` |
|      ! 0 |  8977 | `					pArg++;` |
|      ! 0 |  8978 | `				}` |
|     3352 |  8979 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  8980 | `					/* Check if the call is allowed */` |
|     3352 |  8981 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3352 |  8982 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  8983 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  8984 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  8985 | `							char zMsg[256];` |
|      ! 0 |  8986 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8987 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  8988 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  8989 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  8990 | `							/* Pop given arguments */` |
|      ! 0 |  8991 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  8992 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8993 | `							}` |
|      ! 0 |  8994 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8995 | `							goto Abort;` |
|        - |  8996 | `						}` |
|        6 |  8997 | `					}` |
|     1675 |  8998 | `				}` |
|     1675 |  8999 | `			}` |
|     1675 |  9000 | `		}` |
|        - |  9001 | `		/* Check The recursion limit. Hitting it raises a clean, non-catchable` |
|        - |  9002 | `		 * fatal (was: silently set NULL and continue) and halts. The check is` |
|        - |  9003 | `		 * before VmEnterFrame/the recursive VmByteCodeExec below, so a` |
|        - |  9004 | `		 * correctly-set cap also keeps deep recursion off the native stack. */` |
|    18670 |  9005 | `		if( VmRecursionExceeded(pVm) ){` |
|        - |  9006 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - |  9007 | `			 * which walks the whole operand stack — don't release them here. */` |
|        5 |  9008 | `			VmRecursionFatal(&(*pVm));` |
|        5 |  9009 | `			goto Abort;` |
|        - |  9010 | `		}` |
|    18666 |  9011 | `		if( pVmFunc->pNextName ){` |
|        - |  9012 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  9013 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  9014 | `		}` |
|    18666 |  9015 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  9016 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  9017 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  9018 | `			ph7_generator *pGenerator;` |
|        - |  9019 | `			ph7_class_instance *pGenObj;` |
|        - |  9020 | `			ph7_value *pCtxAttr;` |
|        - |  9021 | `			SyString sAttrName;` |
|        - |  9022 | `			ph7_value **apCallArgs;` |
|        - |  9023 | `			int nGenArgs, iArg;` |
|        - |  9024 | `			/* Collect arguments from the operand stack */` |
|       24 |  9025 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  9026 | `			apCallArgs = 0;` |
|       24 |  9027 | `			if( nGenArgs > 0 ){` |
|       14 |  9028 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9029 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  9030 | `				if( apCallArgs == 0 ){` |
|        - |  9031 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  9032 | `					nGenArgs = 0;` |
|      ! 0 |  9033 | `				}else{` |
|       10 |  9034 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  9035 | `					int didReorder = 0;` |
|       10 |  9036 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  9037 | `						/* Named-argument reordering for generator */` |
|        5 |  9038 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  9039 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  9040 | `						sxu32 nNV = nF;` |
|        5 |  9041 | `						sxi32 iVIdx = -1;` |
|        - |  9042 | `						sxi32 *aGSlot;` |
|        - |  9043 | `						sxu8 *aGUsed;` |
|        - |  9044 | `						sxu32 gi;` |
|       13 |  9045 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  9046 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  9047 | `						}` |
|        7 |  9048 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9049 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  9050 | `						if( aGSlot ){` |
|        5 |  9051 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  9052 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  9053 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  9054 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9055 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  9056 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9057 | `								goto Abort;` |
|        - |  9058 | `							}` |
|        - |  9059 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  9060 | `							 * append overflow (variadic / positional beyond` |
|        - |  9061 | `							 * formals) so downstream sees every argument. */` |
|        - |  9062 | `							{` |
|        5 |  9063 | `								int nOut = 0;` |
|       13 |  9064 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  9065 | `									sxu32 gj;` |
|       13 |  9066 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  9067 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  9068 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  9069 | `											break;` |
|        - |  9070 | `										}` |
|        3 |  9071 | `									}` |
|        5 |  9072 | `								}` |
|       13 |  9073 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  9074 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  9075 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  9076 | `									}` |
|        5 |  9077 | `								}` |
|        5 |  9078 | `								nGenArgs = nOut;` |
|        - |  9079 | `							}` |
|        5 |  9080 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  9081 | `							didReorder = 1;` |
|        2 |  9082 | `						}` |
|        - |  9083 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  9084 | `						 * positional fill below — preserves arg order rather` |
|        - |  9085 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  9086 | `					}` |
|       10 |  9087 | `					if( !didReorder ){` |
|       12 |  9088 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  9089 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  9090 | `						}` |
|        2 |  9091 | `					}` |
|        - |  9092 | `				}` |
|        4 |  9093 | `			}` |
|        - |  9094 | `			/* Create execution context and generator wrapper */` |
|       24 |  9095 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  9096 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  9097 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9098 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9099 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9100 | `				break;` |
|        - |  9101 | `			}` |
|       24 |  9102 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  9103 | `			if( pGenerator == 0 ){` |
|      ! 0 |  9104 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  9105 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9106 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9107 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9108 | `				break;` |
|        - |  9109 | `			}` |
|        - |  9110 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  9111 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  9112 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  9113 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  9114 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  9115 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  9116 | `			if( apCallArgs ){` |
|       10 |  9117 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  9118 | `			}` |
|       24 |  9119 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9120 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9121 | `				if( pThis ){` |
|      ! 0 |  9122 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9123 | `				}` |
|      ! 0 |  9124 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9125 | `					goto Abort;` |
|        - |  9126 | `				}` |
|      ! 0 |  9127 | `				break;` |
|        - |  9128 | `			}` |
|        - |  9129 | `			/* Create Generator class instance */` |
|       24 |  9130 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  9131 | `			if( pGenObj == 0 ){` |
|      ! 0 |  9132 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9133 | `				break;` |
|        - |  9134 | `			}` |
|        - |  9135 | `			/* Store generator in __ctx attribute */` |
|       24 |  9136 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  9137 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  9138 | `			if( pCtxAttr ){` |
|       24 |  9139 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  9140 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  9141 | `			}` |
|        - |  9142 | `			/* Pop args and function name, push Generator object */` |
|       24 |  9143 | `			PH7_MemObjRelease(pTos);` |
|       24 |  9144 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  9145 | `			pTos->x.pOther = pGenObj;` |
|       24 |  9146 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  9147 | `			pGenObj->iRef++;` |
|       24 |  9148 | `			if( pThis ){` |
|      ! 0 |  9149 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9150 | `			}` |
|       24 |  9151 | `			break;` |
|        - |  9152 | `		}` |
|        - |  9153 | `		/* Extract the formal argument set */` |
|    18644 |  9154 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  9155 | `		/* Create a new VM frame  */` |
|    18644 |  9156 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    18644 |  9157 | `		if( rc != SXRET_OK ){` |
|        - |  9158 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9159 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9160 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9161 | `				&pVmFunc->sName);` |
|        - |  9162 | `			/* Pop given arguments */` |
|      ! 0 |  9163 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9164 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9165 | `			}` |
|        - |  9166 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9167 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  9168 | `			break;` |
|        - |  9169 | `		}` |
|    18644 |  9170 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  9171 | `			/* Install the '$this' variable */` |
|        - |  9172 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3280 |  9173 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3280 |  9174 | `			if( pObj ){` |
|        - |  9175 | `				/* Reflect the change */` |
|     3280 |  9176 | `				pObj->x.pOther = pThis;` |
|     3280 |  9177 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1639 |  9178 | `			}` |
|     1639 |  9179 | `		}` |
|    18644 |  9180 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  9181 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  9182 | `			/* Install static variables */` |
|        6 |  9183 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       12 |  9184 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|        6 |  9185 | `				pStatic = &aStatic[n];` |
|        6 |  9186 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  9187 | `					/* Initialize the static variables */` |
|        6 |  9188 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|        6 |  9189 | `					if( pObj ){` |
|        - |  9190 | `						/* Assume a NULL initialization value */` |
|        6 |  9191 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|        6 |  9192 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  9193 | `							/* Evaluate initialization expression (Any complex expression) */` |
|        6 |  9194 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|        3 |  9195 | `						}` |
|        6 |  9196 | `						pObj->nIdx = pStatic->nIdx;` |
|        3 |  9197 | `					}else{` |
|      ! 0 |  9198 | `						continue;` |
|        - |  9199 | `					}` |
|        3 |  9200 | `				}` |
|        - |  9201 | `				/* Install in the current frame */` |
|        9 |  9202 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|        6 |  9203 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|        3 |  9204 | `			}` |
|        3 |  9205 | `		}` |
|        - |  9206 | `		/* Push arguments in the local frame */` |
|        - |  9207 | `		{` |
|    18644 |  9208 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  9209 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  9210 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    18644 |  9211 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    18644 |  9212 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  9213 | `			/* ============================================================` |
|        - |  9214 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  9215 | `			 *` |
|        - |  9216 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  9217 | `			 * or position, then install them in the frame.` |
|        - |  9218 | `			 * ============================================================ */` |
|       96 |  9219 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  9220 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  9221 | `			sxi32 iVariadicIdx = -1;` |
|        - |  9222 | `			sxu32 nNonVariadic;` |
|        - |  9223 | `			sxi32 *aSlot;` |
|        - |  9224 | `			sxu8  *aUsed;` |
|        - |  9225 | `			sxu32 i;` |
|        - |  9226 | `			/* Find variadic parameter index */` |
|      292 |  9227 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  9228 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  9229 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  9230 | `					break;` |
|        - |  9231 | `				}` |
|      100 |  9232 | `			}` |
|       96 |  9233 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  9234 | `			/* Allocate mapping arrays */` |
|      143 |  9235 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  9236 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  9237 | `			if( aSlot == 0 ){` |
|      ! 0 |  9238 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  9239 | `				goto Abort;` |
|        - |  9240 | `			}` |
|       96 |  9241 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  9242 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  9243 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  9244 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  9245 | `			if( rc == PH7_ABORT ){` |
|        7 |  9246 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  9247 | `				goto Abort;` |
|        - |  9248 | `			}` |
|        - |  9249 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  9250 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  9251 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  9252 | `				sxi32 iSrc = -1;` |
|      309 |  9253 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  9254 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  9255 | `						iSrc = (sxi32)i;` |
|      169 |  9256 | `						break;` |
|        - |  9257 | `					}` |
|       62 |  9258 | `				}` |
|      187 |  9259 | `				if( iSrc >= 0 ){` |
|        - |  9260 | `					/* Argument was provided — install with type checking */` |
|      169 |  9261 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  9262 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  9263 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  9264 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  9265 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  9266 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9267 | `					}` |
|        - |  9268 | `					/* Type checking: union types */` |
|      169 |  9269 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  9270 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  9271 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  9272 | `							bCallIsStrict);` |
|       13 |  9273 | `						if( rcU != SXRET_OK ){` |
|        - |  9274 | `							const char *zGiven;` |
|      ! 0 |  9275 | `							const char *zExpected = "union";` |
|        - |  9276 | `							char zBuf[128];` |
|        - |  9277 | `							char zTypeBuf[128];` |
|      ! 0 |  9278 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9279 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  9280 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9281 | `								zGiven = "null";` |
|      ! 0 |  9282 | `							}else{` |
|      ! 0 |  9283 | `								zGiven = ph7_type_name(pVal);` |
|        - |  9284 | `							}` |
|      ! 0 |  9285 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  9286 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  9287 | `							}` |
|      ! 0 |  9288 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9289 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  9290 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9291 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9292 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  9293 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9294 | `							pFrameStack = 0;` |
|      ! 0 |  9295 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  9296 | `							goto SkipFuncBody;` |
|        - |  9297 | `						}` |
|      171 |  9298 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  9299 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  9300 | `						/* Scalar/class type checking */` |
|       17 |  9301 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  9302 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  9303 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  9304 | `							if( pClass ){` |
|      ! 0 |  9305 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9306 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9307 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9308 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9309 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9310 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9311 | `									}` |
|      ! 0 |  9312 | `								}else{` |
|      ! 0 |  9313 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9314 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  9315 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9316 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9317 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9318 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9319 | `									}` |
|        - |  9320 | `								}` |
|      ! 0 |  9321 | `							}` |
|       17 |  9322 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  9323 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  9324 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9325 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  9326 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9327 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9328 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9329 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9330 | `								pFrameStack = 0;` |
|      ! 0 |  9331 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9332 | `								goto SkipFuncBody;` |
|        7 |  9333 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9334 | `								char zTypeBuf[128];` |
|      ! 0 |  9335 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9336 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9337 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9338 | `									ph7_type_name(pVal));` |
|      ! 0 |  9339 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9340 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9341 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9342 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9343 | `								pFrameStack = 0;` |
|      ! 0 |  9344 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9345 | `								goto SkipFuncBody;` |
|        - |  9346 | `							}` |
|        3 |  9347 | `						}` |
|        8 |  9348 | `					}` |
|        - |  9349 | `					/* Install: by reference or by value */` |
|      169 |  9350 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  9351 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9352 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  9353 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9354 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9355 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9356 | `							}` |
|      ! 0 |  9357 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9358 | `						}else{` |
|        7 |  9359 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  9360 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  9361 | `							if( pRefEntry == 0 ){` |
|        7 |  9362 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  9363 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  9364 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  9365 | `								sArg.pUserData = 0;` |
|        5 |  9366 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  9367 | `							}` |
|        5 |  9368 | `							pObj = 0;` |
|        - |  9369 | `						}` |
|        3 |  9370 | `					}else{` |
|      165 |  9371 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9372 | `					}` |
|      169 |  9373 | `					if( pObj ){` |
|      165 |  9374 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  9375 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  9376 | `						sArg.pUserData = 0;` |
|      165 |  9377 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  9378 | `					}` |
|       85 |  9379 | `				}else{` |
|        - |  9380 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  9381 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9382 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  9383 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  9384 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  9385 | `						if( pObj ){` |
|       19 |  9386 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  9387 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  9388 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  9389 | `							sArg.pUserData = 0;` |
|       19 |  9390 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9391 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  9392 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9393 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9394 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  9395 | `							}` |
|        9 |  9396 | `						}` |
|        9 |  9397 | `					}` |
|        - |  9398 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  9399 | `				}` |
|       94 |  9400 | `			}` |
|        - |  9401 | `			/* Handle variadic parameter */` |
|       89 |  9402 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  9403 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  9404 | `				if( pObj ){` |
|        9 |  9405 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9406 | `					{` |
|        9 |  9407 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  9408 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  9409 | `							if( aSlot[i] == -1 ){` |
|       16 |  9410 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  9411 | `									/* Named variadic entry: insert with string key */` |
|        - |  9412 | `									ph7_value sKey;` |
|       11 |  9413 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  9414 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  9415 | `										pCallMap3->aNames[i].zString,` |
|       10 |  9416 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  9417 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  9418 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  9419 | `								}else{` |
|        - |  9420 | `									/* Positional variadic entry */` |
|      ! 0 |  9421 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  9422 | `								}` |
|        5 |  9423 | `							}` |
|       12 |  9424 | `						}` |
|        - |  9425 | `					}` |
|        9 |  9426 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  9427 | `					sArg.pUserData = 0;` |
|        9 |  9428 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  9429 | `				}` |
|        5 |  9430 | `			}else{` |
|        - |  9431 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  9432 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  9433 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9434 | `				 * the positional-only path's behavior. */` |
|       81 |  9435 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9436 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9437 | `					if( aSlot[i] == -2 ){` |
|        - |  9438 | `						char zAnonBuf[32];` |
|        - |  9439 | `						SyString sAnonName;` |
|      ! 0 |  9440 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9441 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9442 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9443 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9444 | `						if( pObj ){` |
|      ! 0 |  9445 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9446 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9447 | `							sArg.pUserData = 0;` |
|      ! 0 |  9448 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9449 | `						}` |
|      ! 0 |  9450 | `						nAnon++;` |
|      ! 0 |  9451 | `					}` |
|       79 |  9452 | `				}` |
|        - |  9453 | `			}` |
|        - |  9454 | `			/* Release all stack arguments */` |
|      267 |  9455 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9456 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9457 | `			}` |
|       89 |  9458 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9459 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9460 | `			n = nFormal;` |
|       45 |  9461 | `		}else{` |
|        - |  9462 | `		/* ============================================================` |
|        - |  9463 | `		 * Positional-only matching path (original)` |
|        - |  9464 | `		 * ============================================================ */` |
|    18550 |  9465 | `		n = 0;` |
|    49222 |  9466 | `		while( pArg < pTos ){` |
|    30746 |  9467 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9468 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  9469 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  9470 | `				if( pObj ){` |
|        - |  9471 | `					/* Initialize as empty array */` |
|       40 |  9472 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9473 | `					{` |
|       40 |  9474 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  9475 | `						while( pArg < pTos ){` |
|        - |  9476 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9477 | `							 *` |
|        - |  9478 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9479 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9480 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9481 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9482 | `							 * fixing both wants a separate counter for elements` |
|        - |  9483 | `							 * already packed into the variadic array. */` |
|      114 |  9484 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9485 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9486 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9487 | `									bCallIsStrict);` |
|       16 |  9488 | `								if( rcU != SXRET_OK ){` |
|        - |  9489 | `									const char *zGiven;` |
|        3 |  9490 | `									const char *zExpected = "union";` |
|        - |  9491 | `									char zBuf[128];` |
|        - |  9492 | `									char zTypeBuf[128];` |
|        3 |  9493 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9494 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9495 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9496 | `										zGiven = "null";` |
|      ! 0 |  9497 | `									}else{` |
|        3 |  9498 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9499 | `									}` |
|        3 |  9500 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9501 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9502 | `									}` |
|        4 |  9503 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9504 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9505 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9506 | `										goto Abort;` |
|        - |  9507 | `									}` |
|        3 |  9508 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9509 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9510 | `									pFrameStack = 0;` |
|        3 |  9511 | `									rc = PH7_EXCEPTION;` |
|        3 |  9512 | `									goto SkipFuncBody;` |
|        - |  9513 | `								}` |
|       14 |  9514 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9515 | `								pArg++;` |
|       14 |  9516 | `								continue;` |
|        - |  9517 | `							}` |
|        - |  9518 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9519 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  9520 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9521 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  9522 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9523 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9524 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9525 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9526 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9527 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9528 | `										goto Abort;` |
|        - |  9529 | `									}` |
|        - |  9530 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9531 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9532 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9533 | `									pFrameStack = 0;` |
|      ! 0 |  9534 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9535 | `									goto SkipFuncBody;` |
|       13 |  9536 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9537 | `									char zTypeBuf[128];` |
|      ! 0 |  9538 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9539 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9540 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9541 | `										ph7_type_name(pArg));` |
|      ! 0 |  9542 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9543 | `										goto Abort;` |
|        - |  9544 | `									}` |
|      ! 0 |  9545 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9546 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9547 | `									pFrameStack = 0;` |
|      ! 0 |  9548 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9549 | `									goto SkipFuncBody;` |
|        - |  9550 | `								}` |
|        6 |  9551 | `							}` |
|      100 |  9552 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  9553 | `							pArg++;` |
|        2 |  9554 | `						}` |
|        - |  9555 | `					}` |
|       38 |  9556 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  9557 | `					sArg.pUserData = 0;` |
|       38 |  9558 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9559 | `				}` |
|       38 |  9560 | `				break; /* All remaining args consumed */` |
|        - |  9561 | `			}` |
|    30708 |  9562 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    30490 |  9563 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       39 |  9564 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9565 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9566 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9567 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9568 | `						goto Abort;` |
|        - |  9569 | `					}` |
|      ! 0 |  9570 | `				}` |
|        - |  9571 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    30492 |  9572 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  9573 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  9574 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  9575 | `						bCallIsStrict);` |
|       60 |  9576 | `					if( rcU != SXRET_OK ){` |
|        - |  9577 | `						const char *zGiven;` |
|       19 |  9578 | `						const char *zExpected = "union";` |
|        - |  9579 | `						char zBuf[128];` |
|        - |  9580 | `						char zTypeBuf[128];` |
|       19 |  9581 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9582 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9583 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9584 | `							zGiven = "null";` |
|        5 |  9585 | `						}else{` |
|        5 |  9586 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9587 | `						}` |
|       19 |  9588 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9589 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9590 | `						}` |
|       28 |  9591 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9592 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9593 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9594 | `							goto Abort;` |
|        - |  9595 | `						}` |
|       19 |  9596 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9597 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9598 | `						pFrameStack = 0;` |
|       19 |  9599 | `						rc = PH7_EXCEPTION;` |
|       19 |  9600 | `						goto SkipFuncBody;` |
|        - |  9601 | `					}` |
|       21 |  9602 | `				}else` |
|        - |  9603 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9604 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    30458 |  9605 | `				if( aFormalArg[n].nType > 0` |
|    15933 |  9606 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1406 |  9607 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9608 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  9609 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9610 | `						ph7_class *pClass;` |
|        - |  9611 | `						/* Try to extract the desired class */` |
|       26 |  9612 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  9613 | `						if( pClass ){` |
|       22 |  9614 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9615 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9616 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9617 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9618 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9619 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9620 | `								}` |
|      ! 0 |  9621 | `							}else{` |
|        - |  9622 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9623 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9624 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9625 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9626 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9627 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9628 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9629 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9630 | `								}` |
|        - |  9631 | `							}` |
|       12 |  9632 | `						}` |
|     1394 |  9633 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       26 |  9634 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9635 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9636 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9637 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9638 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9639 | `								goto Abort;` |
|        - |  9640 | `							}` |
|        - |  9641 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9642 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9643 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9644 | `							pFrameStack = 0;` |
|       11 |  9645 | `							rc = PH7_EXCEPTION;` |
|       11 |  9646 | `							goto SkipFuncBody;` |
|       16 |  9647 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9648 | `							char zTypeBuf[128];` |
|       11 |  9649 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        6 |  9650 | `								&aFormalArg[n].sName,` |
|        6 |  9651 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        3 |  9652 | `								ph7_type_name(pArg));` |
|        8 |  9653 | `							if( rc == PH7_ABORT ){` |
|        5 |  9654 | `								goto Abort;` |
|        - |  9655 | `							}` |
|        3 |  9656 | `							PH7_MemObjRelease(pTos);` |
|        3 |  9657 | `							pTos = &pTos[-nCallArgs];` |
|        3 |  9658 | `							pFrameStack = 0;` |
|        3 |  9659 | `							rc = PH7_EXCEPTION;` |
|        3 |  9660 | `							goto SkipFuncBody;` |
|        - |  9661 | `						}` |
|        4 |  9662 | `					}` |
|      694 |  9663 | `				}` |
|    30458 |  9664 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9665 | `					/* Pass by reference */` |
|       58 |  9666 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9667 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9668 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9669 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9670 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9671 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9672 | `						}` |
|        - |  9673 | `						/* Switch to pass by value */` |
|      ! 0 |  9674 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9675 | `					}else{` |
|        - |  9676 | `						SyHashEntry *pRefEntry;` |
|        - |  9677 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9678 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9679 | `						if( pRefEntry == 0 ){` |
|       86 |  9680 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9681 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9682 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9683 | `							sArg.pUserData = 0;` |
|       58 |  9684 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9685 | `						}` |
|       58 |  9686 | `						pObj = 0;` |
|        - |  9687 | `					}` |
|       30 |  9688 | `				}else{` |
|        - |  9689 | `					/* Pass by value,make a copy of the given argument */` |
|    30402 |  9690 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9691 | `				}` |
|    15230 |  9692 | `			}else{` |
|        - |  9693 | `				char zName[32];` |
|        - |  9694 | `				SyString sArgName;` |
|        - |  9695 | `				/* Set a dummy name */` |
|      218 |  9696 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      218 |  9697 | `				sArgName.zString = zName;` |
|        - |  9698 | `				/* Annonymous argument */` |
|      218 |  9699 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9700 | `			}` |
|    30674 |  9701 | `			if( pObj ){` |
|    30618 |  9702 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9703 | `				/* Insert argument index  */` |
|    30618 |  9704 | `				sArg.nIdx = pObj->nIdx;` |
|    30618 |  9705 | `				sArg.pUserData = 0;` |
|    30618 |  9706 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15308 |  9707 | `			}` |
|    30674 |  9708 | `			PH7_MemObjRelease(pArg);` |
|    30674 |  9709 | `			pArg++;` |
|    30674 |  9710 | `			++n;` |
|        2 |  9711 | `		}` |
|        - |  9712 | `		} /* end named vs positional branch */` |
|        - |  9713 | `		/* Set up closure environment */` |
|    18602 |  9714 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9715 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9716 | `			ph7_value *pValue;` |
|        - |  9717 | `			sxu32 iEnv;` |
|      184 |  9718 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      434 |  9719 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      252 |  9720 | `				pEnv = &aEnv[iEnv];` |
|      252 |  9721 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9722 | `					/* Do not install null value */` |
|      178 |  9723 | `					continue;` |
|        - |  9724 | `				}` |
|       76 |  9725 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9726 | `				if( pValue == 0 ){` |
|      ! 0 |  9727 | `					continue;` |
|        - |  9728 | `				}` |
|        - |  9729 | `				/* Invalidate any prior representation */` |
|       76 |  9730 | `				PH7_MemObjRelease(pValue);` |
|        - |  9731 | `				/* Duplicate bound variable value */` |
|       76 |  9732 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9733 | `			}` |
|       91 |  9734 | `		}` |
|        - |  9735 | `		/* Process default values for remaining formal parameters */` |
|    21494 |  9736 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2940 |  9737 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9738 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  9739 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  9740 | `				if( pObj ){` |
|       48 |  9741 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  9742 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  9743 | `					sArg.pUserData = 0;` |
|       48 |  9744 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  9745 | `				}` |
|       48 |  9746 | `				n++;` |
|       48 |  9747 | `				break; /* Variadic is always last */` |
|        - |  9748 | `			}` |
|     2894 |  9749 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2888 |  9750 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2888 |  9751 | `				if( pObj ){` |
|        - |  9752 | `					/* Evaluate the default value and extract it's result */` |
|     2888 |  9753 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2888 |  9754 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9755 | `						goto Abort;` |
|        - |  9756 | `					}` |
|        - |  9757 | `					/* Insert argument index */` |
|     2888 |  9758 | `					sArg.nIdx = pObj->nIdx;` |
|     2888 |  9759 | `					sArg.pUserData = 0;` |
|     2888 |  9760 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9761 | `					/* Make sure the default argument is of the correct type */` |
|     2886 |  9762 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1866 |  9763 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9764 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9765 | `						/* Cast to the desired type */` |
|        3 |  9766 | `						xCast(pObj);` |
|        1 |  9767 | `					}` |
|     1443 |  9768 | `				}` |
|     1443 |  9769 | `			}` |
|     2894 |  9770 | `			++n;` |
|        2 |  9771 | `		}` |
|        - |  9772 | `		} /* end VmCallArgMap scope */` |
|        - |  9773 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9774 | `		 * does not return anything.` |
|        - |  9775 | `		 */` |
|    18602 |  9776 | `		PH7_MemObjRelease(pTos);` |
|    18602 |  9777 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9778 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    18602 |  9779 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    18602 |  9780 | `		if( pFrameStack == 0 ){` |
|        - |  9781 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9782 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9783 | `				&pVmFunc->sName);` |
|      ! 0 |  9784 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9785 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9786 | `			}` |
|      ! 0 |  9787 | `			break;` |
|        - |  9788 | `		}` |
|     9300 |  9789 | `SkipFuncBody:` |
|    18634 |  9790 | `		if( pSelf ){` |
|        - |  9791 | `			/* Push class name */` |
|     3350 |  9792 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1674 |  9793 | `		}` |
|        - |  9794 | `		/* Increment nesting level */` |
|    18634 |  9795 | `		pVm->nRecursionDepth++;` |
|    18634 |  9796 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  9797 | `			/* Execute function body */` |
|    27902 |  9798 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    18600 |  9799 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     9300 |  9800 | `		}` |
|        - |  9801 | `		/* Decrement nesting level */` |
|    18634 |  9802 | `		pVm->nRecursionDepth--;` |
|    18634 |  9803 | `		if( pSelf ){` |
|        - |  9804 | `			/* Pop class name */` |
|     3350 |  9805 | `			(void)SySetPop(&pVm->aSelf);` |
|     1674 |  9806 | `		}` |
|        - |  9807 | `		/* Cleanup the mess left behind */` |
|    18634 |  9808 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  9809 | `			/* Return by reference,reflect that */` |
|        9 |  9810 | `			if( n != SXU32_HIGH ){` |
|        9 |  9811 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  9812 | `				sxu32 i;` |
|        - |  9813 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  9814 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  9815 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  9816 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  9817 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9818 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9819 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  9820 | `								&pVmFunc->sName);` |
|      ! 0 |  9821 | `						}` |
|      ! 0 |  9822 | `						n = SXU32_HIGH;` |
|      ! 0 |  9823 | `						break;` |
|        - |  9824 | `					}` |
|        3 |  9825 | `				}` |
|        5 |  9826 | `			}else{` |
|      ! 0 |  9827 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9828 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9829 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  9830 | `						&pVmFunc->sName);` |
|      ! 0 |  9831 | `				}` |
|        - |  9832 | `			}` |
|        9 |  9833 | `			pTos->nIdx = n;` |
|        4 |  9834 | `		}` |
|        - |  9835 | `		/* Cleanup the mess left behind */` |
|    18634 |  9836 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  9837 | `			/* An exception was throw in this frame */` |
|      100 |  9838 | `			pFrame = pFrame->pParent;` |
|      100 |  9839 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  9840 | `				/* Pop the resutlt */` |
|       62 |  9841 | `				VmPopOperand(&pTos,1);` |
|        - |  9842 | `				/* Jump to this destination */` |
|       62 |  9843 | `				pc = pFrame->iExceptionJump - 1;` |
|       62 |  9844 | `				rc = PH7_OK;` |
|       32 |  9845 | `			}else{` |
|       39 |  9846 | `				if( pFrame->pParent ){` |
|       39 |  9847 | `					rc = PH7_EXCEPTION;` |
|       20 |  9848 | `				}else{` |
|        - |  9849 | `					/* Continue normal execution */` |
|      ! 0 |  9850 | `					rc = PH7_OK;` |
|        - |  9851 | `				}` |
|        - |  9852 | `			}` |
|       49 |  9853 | `		}` |
|        - |  9854 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    18634 |  9855 | `		if( pFrameStack ){` |
|    18602 |  9856 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9300 |  9857 | `		}` |
|        - |  9858 | `		/* Leave the frame */` |
|    18634 |  9859 | `		VmLeaveFrame(&(*pVm));` |
|    18634 |  9860 | `		if( rc == PH7_ABORT ){` |
|        - |  9861 | `			/* Abort processing immeditaley */` |
|      117 |  9862 | `			goto Abort;` |
|    18518 |  9863 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9864 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  9865 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  9866 | `			 * overwriting the state saved by the inner level.` |
|        - |  9867 | `			 * pTos points to the result slot (not yet written).` |
|        - |  9868 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  9869 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  9870 | `			goto Suspend;` |
|    18480 |  9871 | `		}else if( rc == PH7_EXCEPTION ){` |
|       39 |  9872 | `			goto Exception;` |
|        - |  9873 | `		}` |
|     9222 |  9874 | `	}else{` |
|        - |  9875 | `		ph7_user_func *pFunc;` |
|        - |  9876 | `		ph7_context sCtx;` |
|        - |  9877 | `		ph7_value sRet;` |
|        - |  9878 | `		/* Look for an installed foreign function.` |
|        - |  9879 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  9880 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  9881 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  9882 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   698238 |  9883 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9884 | `		{` |
|   698238 |  9885 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   698238 |  9886 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  9887 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  9888 | `			const char *zShort = sName.zString;` |
|        - |  9889 | `			sxu32 i;` |
|      334 |  9890 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  9891 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  9892 | `					zShort = &sName.zString[i + 1];` |
|       13 |  9893 | `				}` |
|      158 |  9894 | `			}` |
|       22 |  9895 | `			if( zShort != sName.zString ){` |
|       22 |  9896 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  9897 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  9898 | `			}` |
|       10 |  9899 | `		}` |
|        - |  9900 | `		} /* end VmCallArgMap namespace scope */` |
|   698238 |  9901 | `		if( pEntry == 0 ){` |
|        - |  9902 | `			/* Call to undefined function */` |
|        5 |  9903 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  9904 | `			/* Pop given arguments */` |
|        5 |  9905 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  9906 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  9907 | `			}` |
|        - |  9908 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  9909 | `			PH7_MemObjRelease(pTos);` |
|       58 |  9910 | `			break;` |
|        - |  9911 | `		}` |
|   698234 |  9912 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  9913 | `		/* Start collecting function arguments */` |
|   698234 |  9914 | `		SySetReset(&aArg);` |
|  1882544 |  9915 | `		while( pArg < pTos ){` |
|  1184312 |  9916 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1184312 |  9917 | `			pArg++;` |
|        2 |  9918 | `		}` |
|        - |  9919 | `		/* Assume a null return value */` |
|   698234 |  9920 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  9921 | `		/* Init the call context */` |
|   698234 |  9922 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  9923 | `		/* Call the foreign function */` |
|   698234 |  9924 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  9925 | `		/* Release the call context */` |
|   698234 |  9926 | `		VmReleaseCallContext(&sCtx);` |
|   698234 |  9927 | `		if( rc == PH7_ABORT ){` |
|        - |  9928 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - |  9929 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - |  9930 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      531 |  9931 | `			PH7_MemObjRelease(&sRet);` |
|      531 |  9932 | `			goto Abort;` |
|   697704 |  9933 | `		}else if( rc == PH7_EXCEPTION ){` |
|      112 |  9934 | `			VmFrame *pFrm = pVm->pFrame;` |
|      112 |  9935 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      112 |  9936 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  9937 | `				/* Exception was NOT caught, propagate */` |
|        5 |  9938 | `				goto Exception;` |
|        - |  9939 | `			}` |
|        - |  9940 | `			/* Exception was caught: pop args and the result slot */` |
|      108 |  9941 | `			PH7_MemObjRelease(&sRet);` |
|      108 |  9942 | `			if( pInstr->iP1 > 0 ){` |
|       92 |  9943 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       45 |  9944 | `			}` |
|        - |  9945 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      108 |  9946 | `			VmPopOperand(&pTos,1);` |
|        - |  9947 | `			/* Jump past the try/catch block via the exception frame */` |
|      108 |  9948 | `			pFrm = pVm->pFrame;` |
|      108 |  9949 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      108 |  9950 | `				pc = pFrm->iExceptionJump - 1;` |
|       53 |  9951 | `			}` |
|      108 |  9952 | `			break;` |
|        - |  9953 | `		}` |
|   697594 |  9954 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9955 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  9956 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  9957 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  9958 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  9959 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  9960 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  9961 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  9962 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  9963 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  9964 | `			}` |
|        - |  9965 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  9966 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  9967 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  9968 | `			goto Suspend;` |
|        - |  9969 | `		}` |
|   697556 |  9970 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9971 | `			/* Pop function name and arguments */` |
|   675536 |  9972 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   337789 |  9973 | `		}` |
|        - |  9974 | `		/* Save foreign function return value */` |
|   697556 |  9975 | `		PH7_MemObjStore(&sRet,pTos);` |
|   697556 |  9976 | `		PH7_MemObjRelease(&sRet);` |
|        - |  9977 | `	}` |
|   715996 |  9978 | `	break;` |
|        - |  9979 | `				  }` |
|        - |  9980 | `/*` |
|        - |  9981 | ` * OP_CONSUME: P1 * *` |
|        - |  9982 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  9983 | ` */` |
|    16018 |  9984 | `case PH7_OP_CONSUME: {` |
|    32038 |  9985 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    32038 |  9986 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  9987 |  |
|    32038 |  9988 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    32038 |  9989 | `	pCur = pOut;` |
|        - |  9990 | `	/* Start the consume process  */` |
|    64116 |  9991 | `	while( pOut <= pTos ){` |
|        - |  9992 | `		/* Force a string cast */` |
|    32080 |  9993 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1052 |  9994 | `			PH7_MemObjToString(pOut);` |
|      525 |  9995 | `		}` |
|    32080 |  9996 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  9997 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  9998 | `			/* Invoke the output consumer callback */` |
|    19644 |  9999 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    19644 | 10000 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    19644 | 10001 | `			SyBlobRelease(&pOut->sBlob);` |
|    19644 | 10002 | `			if( rc == SXERR_ABORT ){` |
|        - | 10003 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 | 10004 | `				goto Abort;` |
|        - | 10005 | `			}` |
|     9821 | 10006 | `		}` |
|    32080 | 10007 | `		pOut++;` |
|        2 | 10008 | `	}` |
|    32038 | 10009 | `	pTos = &pCur[-1];` |
|    32036 | 10010 | `	break;` |
|        - | 10011 | `					 }` |
|        - | 10012 |  |
|        - | 10013 | `		} /* Switch() */` |
| 11786454 | 10014 | `		pc++; /* Next instruction in the stream */` |
|        2 | 10015 | `	} /* For(;;) */` |
|    22210 | 10016 | `Done:` |
|    44422 | 10017 | `	SySetRelease(&aArg);` |
|    44422 | 10018 | `	return SXRET_OK;` |
|       72 | 10019 | `Suspend:` |
|      146 | 10020 | `	SySetRelease(&aArg);` |
|      146 | 10021 | `	return PH7_SUSPEND;` |
|      349 | 10022 | `Abort:` |
|      699 | 10023 | `	SySetRelease(&aArg);` |
|     2185 | 10024 | `	while( pTos >= pStack ){` |
|     1487 | 10025 | `		PH7_MemObjRelease(pTos);` |
|     1487 | 10026 | `		pTos--;` |
|        1 | 10027 | `	}` |
|      699 | 10028 | `	return PH7_ABORT;` |
|       29 | 10029 | `Exception:` |
|       60 | 10030 | `	SySetRelease(&aArg);` |
|      112 | 10031 | `	while( pTos >= pStack ){` |
|       54 | 10032 | `		PH7_MemObjRelease(pTos);` |
|       54 | 10033 | `		pTos--;` |
|        2 | 10034 | `	}` |
|       60 | 10035 | `	return PH7_EXCEPTION;` |
|    22662 | 10036 |  |
|        - | 10037 | `/*` |
|        - | 10038 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 10039 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10040 | ` * See block-comment on that function for additional information.` |
|        - | 10041 | ` */` |
|    20656 | 10042 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 | 10043 |  |
|        - | 10044 | `	ph7_value *pStack;` |
|        - | 10045 | `	sxi32 rc;` |
|        - | 10046 | `	/* Allocate a new operand stack */` |
|    20658 | 10047 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    20658 | 10048 | `	if( pStack == 0 ){` |
|      ! 0 | 10049 | `		return SXERR_MEM;` |
|        - | 10050 | `	}` |
|        - | 10051 | `	/* Execute the program */` |
|    20658 | 10052 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - | 10053 | `	/* Free the operand stack */` |
|    20658 | 10054 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 10055 | `	/* Execution result */` |
|    20658 | 10056 | `	return rc;` |
|    10330 | 10057 |  |
|        - | 10058 | `/*` |
|        - | 10059 | ` * Invoke any installed shutdown callbacks.` |
|        - | 10060 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 10061 | ` * or more calls to [register_shutdown_function()].` |
|        - | 10062 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 10063 | ` * execution ends.` |
|        - | 10064 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 10065 | ` * additional information.` |
|        - | 10066 | ` */` |
|     2840 | 10067 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 | 10068 |  |
|        - | 10069 | `	VmShutdownCB *pEntry;` |
|        - | 10070 | `	ph7_value *apArg[10];` |
|        - | 10071 | `	sxu32 n,nEntry;` |
|        - | 10072 | `	int i;` |
|        - | 10073 | `	/* Point to the stack of registered callbacks */` |
|     2842 | 10074 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31242 | 10075 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28402 | 10076 | `		apArg[i] = 0;` |
|    14202 | 10077 | `	}` |
|        - | 10078 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 10079 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 10080 | `	 * callbacks, mirroring PHP.` |
|        - | 10081 | `	 */` |
|     2842 | 10082 | `	pVm->bHaltRequested = 0;` |
|     2854 | 10083 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       14 | 10084 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       14 | 10085 | `		if( pEntry ){` |
|        - | 10086 | `			/* Prepare callback arguments if any */` |
|       14 | 10087 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 10088 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 10089 | `					break;` |
|        - | 10090 | `				}` |
|      ! 0 | 10091 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 10092 | `			}` |
|        - | 10093 | `			/* Invoke the callback */` |
|       14 | 10094 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 10095 | `			/*` |
|        - | 10096 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 10097 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 10098 | `			 */` |
|       14 | 10099 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       14 | 10100 | `			if( pEntry ){` |
|       14 | 10101 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       14 | 10102 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 10103 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 10104 | `				}` |
|        6 | 10105 | `			}` |
|       14 | 10106 | `			if( pVm->bHaltRequested ){` |
|        - | 10107 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 10108 | `				break;` |
|        - | 10109 | `			}` |
|        6 | 10110 | `		}` |
|        8 | 10111 | `	}` |
|     2842 | 10112 | `	SySetReset(&pVm->aShutdown);` |
|     2842 | 10113 |  |
|        - | 10114 | `/*` |
|        - | 10115 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 10116 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10117 | ` * See block-comment on that function for additional information.` |
|        - | 10118 | ` */` |
|     2840 | 10119 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 | 10120 |  |
|        - | 10121 | `	/* Make sure we are ready to execute this program */` |
|     2842 | 10122 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 10123 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 10124 | `	}` |
|        - | 10125 | `	/* Set the execution magic number  */` |
|     2842 | 10126 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 10127 | `	/* Execute the program */` |
|     2842 | 10128 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - | 10129 | `	/* Invoke any shutdown callbacks */` |
|     2842 | 10130 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 10131 | `	/*` |
|        - | 10132 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 10133 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 10134 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 10135 | `	 */` |
|     2842 | 10136 | `	return SXRET_OK;` |
|     1422 | 10137 |  |
|        - | 10138 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 10139 | `/*` |
|        - | 10140 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - | 10141 | ` * The context is in CREATED state and ready to be started.` |
|        - | 10142 | ` */` |
|       46 | 10143 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 | 10144 |  |
|        - | 10145 | `	ph7_exec_ctx *pCtx;` |
|        - | 10146 | `	ph7_value *pStack;` |
|        - | 10147 | `	VmFrame *pFrame;` |
|       48 | 10148 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 | 10149 | `	if( pCtx == 0 ){` |
|      ! 0 | 10150 | `		return 0;` |
|        - | 10151 | `	}` |
|       48 | 10152 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 | 10153 | `	pCtx->pVm = pVm;` |
|       48 | 10154 | `	pCtx->pFunc = pFunc;` |
|       48 | 10155 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 | 10156 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 | 10157 | `	pCtx->pc = 0;` |
|       48 | 10158 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 | 10159 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - | 10160 | `	/* Allocate a private operand stack */` |
|       48 | 10161 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 | 10162 | `	if( pStack == 0 ){` |
|      ! 0 | 10163 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10164 | `		return 0;` |
|        - | 10165 | `	}` |
|       48 | 10166 | `	pCtx->pStack = pStack;` |
|        - | 10167 | `	/* Create a detached frame for the fiber */` |
|       48 | 10168 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 | 10169 | `	if( pFrame == 0 ){` |
|      ! 0 | 10170 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 | 10171 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10172 | `		return 0;` |
|        - | 10173 | `	}` |
|       48 | 10174 | `	pCtx->pFrame = pFrame;` |
|       48 | 10175 | `	return pCtx;` |
|       25 | 10176 |  |
|        - | 10177 | `/*` |
|        - | 10178 | ` * Start executing a fiber context for the first time.` |
|        - | 10179 | ` */` |
|       46 | 10180 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 | 10181 |  |
|        - | 10182 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10183 | `	sxi32 rc;` |
|       48 | 10184 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10185 | `		return SXERR_INVALID;` |
|        - | 10186 | `	}` |
|        - | 10187 | `	/* Bound fiber/generator nesting under the same cap (each start adds a C` |
|        - | 10188 | `	 * frame); reject before mutating VM state so the abort is clean. */` |
|       48 | 10189 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10190 | `		return VmRecursionFatal(pVm);` |
|        - | 10191 | `	}` |
|        - | 10192 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 | 10193 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 | 10194 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10195 | `	/* Save and set the active context */` |
|       48 | 10196 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 | 10197 | `	pVm->pActiveCtx = pCtx;` |
|       48 | 10198 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 | 10199 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 | 10200 | `	pVm->nRecursionDepth++;` |
|        - | 10201 | `	/* Execute from the beginning */` |
|       48 | 10202 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 | 10203 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 | 10204 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 | 10205 | `	pVm->nRecursionDepth--;` |
|        - | 10206 | `	/* Restore the previous context */` |
|       48 | 10207 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 | 10208 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10209 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 | 10210 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 | 10211 | `		pCtx->pFrame->pParent = 0;` |
|       46 | 10212 | `		if( pResult ){` |
|       24 | 10213 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 | 10214 | `		}` |
|       46 | 10215 | `		return SXRET_OK;` |
|        - | 10216 | `	}` |
|        - | 10217 | `	/* Detach frame */` |
|        3 | 10218 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 | 10219 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 | 10220 | `		pCtx->pFrame->pParent = 0;` |
|        1 | 10221 | `	}` |
|        3 | 10222 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10223 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10224 | `		return PH7_ABORT;` |
|        - | 10225 | `	}` |
|        3 | 10226 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10227 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10228 | `		return PH7_EXCEPTION;` |
|        - | 10229 | `	}` |
|        - | 10230 | `	/* Normal completion */` |
|        3 | 10231 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 | 10232 | `	if( pResult ){` |
|        3 | 10233 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 | 10234 | `	}` |
|        3 | 10235 | `	return SXRET_OK;` |
|       25 | 10236 |  |
|        - | 10237 | `/*` |
|        - | 10238 | ` * Resume a suspended fiber context.` |
|        - | 10239 | ` */` |
|       98 | 10240 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 | 10241 |  |
|        - | 10242 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10243 | `	sxi32 rc;` |
|      100 | 10244 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 | 10245 | `		return SXERR_INVALID;` |
|        - | 10246 | `	}` |
|        - | 10247 | `	/* Bound fiber/generator nesting under the same cap; reject before mutating` |
|        - | 10248 | `	 * VM state so the abort is clean. */` |
|      100 | 10249 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10250 | `		return VmRecursionFatal(pVm);` |
|        - | 10251 | `	}` |
|        - | 10252 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - | 10253 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - | 10254 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 | 10255 | `	if( pResumeValue ){` |
|       40 | 10256 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 | 10257 | `	}else{` |
|       62 | 10258 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - | 10259 | `	}` |
|      100 | 10260 | `	pCtx->nTos++;` |
|        - | 10261 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 | 10262 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 | 10263 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10264 | `	/* Save and set the active context */` |
|      100 | 10265 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 | 10266 | `	pVm->pActiveCtx = pCtx;` |
|      100 | 10267 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 | 10268 | `	pVm->nRecursionDepth++;` |
|        - | 10269 | `	/* Resume execution from saved PC */` |
|      100 | 10270 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 | 10271 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 | 10272 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 | 10273 | `	pVm->nRecursionDepth--;` |
|        - | 10274 | `	/* Restore the previous context */` |
|      100 | 10275 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 | 10276 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10277 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 | 10278 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 | 10279 | `		pCtx->pFrame->pParent = 0;` |
|       64 | 10280 | `		if( pResult ){` |
|       18 | 10281 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 | 10282 | `		}` |
|       64 | 10283 | `		return SXRET_OK;` |
|        - | 10284 | `	}` |
|        - | 10285 | `	/* Detach frame */` |
|       38 | 10286 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 | 10287 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 | 10288 | `		pCtx->pFrame->pParent = 0;` |
|       18 | 10289 | `	}` |
|       38 | 10290 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10291 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10292 | `		return PH7_ABORT;` |
|        - | 10293 | `	}` |
|       38 | 10294 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10295 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10296 | `		return PH7_EXCEPTION;` |
|        - | 10297 | `	}` |
|        - | 10298 | `	/* Normal completion */` |
|       38 | 10299 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 | 10300 | `	if( pResult ){` |
|       20 | 10301 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 | 10302 | `	}` |
|       38 | 10303 | `	return SXRET_OK;` |
|       51 | 10304 |  |
|        - | 10305 | `/*` |
|        - | 10306 | ` * Release an execution context and all its resources.` |
|        - | 10307 | ` */` |
|        4 | 10308 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 | 10309 |  |
|        5 | 10310 | `	if( pCtx == 0 ){` |
|      ! 0 | 10311 | `		return;` |
|        - | 10312 | `	}` |
|        5 | 10313 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - | 10314 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 | 10315 | `		return;` |
|        - | 10316 | `	}` |
|        5 | 10317 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - | 10318 | `	/* Release values */` |
|        5 | 10319 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 | 10320 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - | 10321 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 | 10322 | `	if( pCtx->pFrame ){` |
|        - | 10323 | `		VmSlot *aSlot;` |
|        - | 10324 | `		sxu32 n;` |
|        - | 10325 | `		/* Free local variables */` |
|        5 | 10326 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 | 10327 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 | 10328 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 | 10329 | `		}` |
|        - | 10330 | `		/* Remove local references */` |
|        5 | 10331 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 | 10332 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 | 10333 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 | 10334 | `		}` |
|        5 | 10335 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 | 10336 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 | 10337 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 | 10338 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 | 10339 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 | 10340 | `		pCtx->pFrame = 0;` |
|        2 | 10341 | `	}` |
|        - | 10342 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - | 10343 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - | 10344 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 | 10345 | `	if( pCtx->pStack ){` |
|        5 | 10346 | `		if( pCtx->nTos >= 0 ){` |
|        5 | 10347 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 | 10348 | `			while( pTos >= pCtx->pStack ){` |
|        5 | 10349 | `				PH7_MemObjRelease(pTos);` |
|        5 | 10350 | `				pTos--;` |
|        1 | 10351 | `			}` |
|        2 | 10352 | `		}` |
|        5 | 10353 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 | 10354 | `		pCtx->pStack = 0;` |
|        2 | 10355 | `	}` |
|        - | 10356 | `	/* Free the context itself */` |
|        5 | 10357 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 | 10358 |  |
|        - | 10359 | `/*` |
|        - | 10360 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - | 10361 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - | 10362 | ` */` |
|       90 | 10363 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 | 10364 |  |
|        - | 10365 | `	ph7_class_instance *pThis;` |
|        - | 10366 | `	SyString sAttr;` |
|        - | 10367 | `	ph7_value *pAttr;` |
|       92 | 10368 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10369 | `		return 0;` |
|        - | 10370 | `	}` |
|       92 | 10371 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 | 10372 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 | 10373 | `		return 0;` |
|        - | 10374 | `	}` |
|       92 | 10375 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 | 10376 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 | 10377 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 | 10378 | `		return 0;` |
|        - | 10379 | `	}` |
|       62 | 10380 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 | 10381 |  |
|        - | 10382 | `/*` |
|        - | 10383 | ` * Fiber::suspend($value = null) — static method.` |
|        - | 10384 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - | 10385 | ` */` |
|       38 | 10386 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10387 |  |
|       40 | 10388 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 | 10389 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 | 10390 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10391 | `			"Cannot suspend outside of a fiber");` |
|        - | 10392 | `	}` |
|       40 | 10393 | `	if( nArg > 0 ){` |
|       40 | 10394 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 | 10395 | `	}else{` |
|      ! 0 | 10396 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - | 10397 | `	}` |
|       40 | 10398 | `	return PH7_SUSPEND;` |
|       21 | 10399 |  |
|        - | 10400 | `/*` |
|        - | 10401 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - | 10402 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - | 10403 | ` * and closure-environment binding happen with the correct argument context.` |
|        - | 10404 | ` */` |
|       24 | 10405 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10406 |  |
|        - | 10407 | `	ph7_class_instance *pThis;` |
|        - | 10408 | `	ph7_value *pAttr;` |
|        - | 10409 | `	SyString sAttrName;` |
|       26 | 10410 | `	if( nArg < 2 ){` |
|      ! 0 | 10411 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10412 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 10413 | `	}` |
|       26 | 10414 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10415 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10416 | `			"Fiber::__construct(): invalid $this");` |
|        - | 10417 | `	}` |
|       26 | 10418 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 | 10419 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 10420 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10421 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 10422 | `	}` |
|        - | 10423 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 | 10424 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10425 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10426 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - | 10427 | `	}` |
|        - | 10428 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 | 10429 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10430 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10431 | `	if( pAttr ){` |
|       26 | 10432 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 | 10433 | `	}` |
|       26 | 10434 | `	return PH7_OK;` |
|       14 | 10435 |  |
|        - | 10436 | `/*` |
|        - | 10437 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 10438 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 10439 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 10440 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 10441 | ` */` |
|       24 | 10442 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 10443 | `	ph7_class_instance **ppThis)` |
|        2 | 10444 |  |
|       26 | 10445 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10446 | `	ph7_value *pCallable;` |
|        - | 10447 | `	SyString sAttrName;` |
|       26 | 10448 | `	*ppThis = 0;` |
|       26 | 10449 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10450 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 | 10451 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10452 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10453 | `		return 0;` |
|        - | 10454 | `	}` |
|       26 | 10455 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10456 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10457 | `		SyString sName;` |
|        - | 10458 | `		SyHashEntry *pEntry;` |
|        - | 10459 | `		ph7_vm_func *pFunc;` |
|       26 | 10460 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 | 10461 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 | 10462 | `		if( pEntry == 0 ){` |
|      ! 0 | 10463 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10464 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10465 | `			return 0;` |
|        - | 10466 | `		}` |
|       26 | 10467 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 | 10468 | `		return pFunc;` |
|      ! 0 | 10469 | `	}else{` |
|        - | 10470 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10471 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10472 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10473 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10474 | `		if( pMethod == 0 ){` |
|      ! 0 | 10475 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10476 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10477 | `			return 0;` |
|        - | 10478 | `		}` |
|      ! 0 | 10479 | `		*ppThis = pClosure;` |
|      ! 0 | 10480 | `		return &pMethod->sFunc;` |
|        - | 10481 | `	}` |
|       14 | 10482 |  |
|        - | 10483 | `/*` |
|        - | 10484 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10485 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10486 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10487 | ` */` |
|       46 | 10488 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10489 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 | 10490 |  |
|       48 | 10491 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10492 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10493 | `	sxu32 nFormal, n;` |
|        - | 10494 | `	VmSlot sSlot;` |
|        - | 10495 | `	sxi32 rc;` |
|        - | 10496 | `	/* Install $this for closure/method callables */` |
|       48 | 10497 | `	if( pClosureThis ){` |
|        - | 10498 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10499 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10500 | `		if( pObj ){` |
|      ! 0 | 10501 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10502 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10503 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10504 | `		}` |
|      ! 0 | 10505 | `	}` |
|        - | 10506 | `	/* Install static variables */` |
|       48 | 10507 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10508 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10509 | `		ph7_value *pVal;` |
|      ! 0 | 10510 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10511 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10512 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10513 | `			if( pVal ){` |
|      ! 0 | 10514 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10515 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10516 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10517 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10518 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10519 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 | 10520 | `				}` |
|      ! 0 | 10521 | `			}` |
|      ! 0 | 10522 | `		}` |
|      ! 0 | 10523 | `	}` |
|        - | 10524 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 | 10525 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 | 10526 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 | 10527 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10528 | `		ph7_value *pObj;` |
|       20 | 10529 | `		if( n < (sxu32)nArg ){` |
|        - | 10530 | `			/* Argument provided — install with type casting */` |
|       20 | 10531 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10532 | `			if( pObj ){` |
|       20 | 10533 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10534 | `				/* Type casting */` |
|       20 | 10535 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10536 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10537 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10538 | `						if( xCast ){` |
|      ! 0 | 10539 | `							xCast(pObj);` |
|      ! 0 | 10540 | `						}` |
|      ! 0 | 10541 | `					}` |
|      ! 0 | 10542 | `				}` |
|       20 | 10543 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10544 | `				sSlot.pUserData = 0;` |
|       20 | 10545 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10546 | `			}` |
|        9 | 10547 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10548 | `			/* Default value */` |
|      ! 0 | 10549 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10550 | `			if( pObj ){` |
|      ! 0 | 10551 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 | 10552 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10553 | `					return rc;` |
|        - | 10554 | `				}` |
|      ! 0 | 10555 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10556 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10557 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10558 | `						if( xCast ){` |
|      ! 0 | 10559 | `							xCast(pObj);` |
|      ! 0 | 10560 | `						}` |
|      ! 0 | 10561 | `					}` |
|      ! 0 | 10562 | `				}` |
|      ! 0 | 10563 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10564 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10565 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10566 | `			}` |
|      ! 0 | 10567 | `		}` |
|       11 | 10568 | `	}` |
|        - | 10569 | `	/* Install closure environment (captured variables) */` |
|       48 | 10570 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10571 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10572 | `		ph7_value *pValue;` |
|        - | 10573 | `		sxu32 iEnv;` |
|        3 | 10574 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10575 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10576 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10577 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10578 | `				continue;` |
|        - | 10579 | `			}` |
|        5 | 10580 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10581 | `			if( pValue == 0 ){` |
|      ! 0 | 10582 | `				continue;` |
|        - | 10583 | `			}` |
|        5 | 10584 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10585 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10586 | `		}` |
|        1 | 10587 | `	}` |
|       48 | 10588 | `	return SXRET_OK;` |
|       25 | 10589 |  |
|        - | 10590 | `/*` |
|        - | 10591 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10592 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10593 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10594 | ` */` |
|       26 | 10595 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10596 |  |
|       28 | 10597 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10598 | `	ph7_class_instance *pThis;` |
|        - | 10599 | `	ph7_class_instance *pClosureThis;` |
|        - | 10600 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10601 | `	ph7_vm_func *pFunc;` |
|        - | 10602 | `	ph7_value sResult;` |
|        - | 10603 | `	ph7_value *pCtxAttr;` |
|        - | 10604 | `	SyString sAttrName;` |
|        - | 10605 | `	sxi32 rc;` |
|       28 | 10606 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10607 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10608 | `	}` |
|       28 | 10609 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10610 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10611 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10612 | `	if( pExecCtx != 0 ){` |
|        3 | 10613 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10614 | `			"Cannot start a fiber that has already been started");` |
|        - | 10615 | `	}` |
|        - | 10616 | `	/* Resolve callable */` |
|       26 | 10617 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10618 | `	if( pFunc == 0 ){` |
|      ! 0 | 10619 | `		return PH7_EXCEPTION;` |
|        - | 10620 | `	}` |
|        - | 10621 | `	/* Create execution context now that we know the function */` |
|       26 | 10622 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10623 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10624 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10625 | `			"Fiber::start(): out of memory");` |
|        - | 10626 | `	}` |
|        - | 10627 | `	/* Store context in $this->__ctx */` |
|       26 | 10628 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10629 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10630 | `	if( pCtxAttr ){` |
|       26 | 10631 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10632 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10633 | `	}` |
|        - | 10634 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10635 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10636 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10637 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10638 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10639 | `	/* Unpack the args array and install into the frame */` |
|        - | 10640 | `	{` |
|       26 | 10641 | `		ph7_value **apValues = 0;` |
|       26 | 10642 | `		ph7_value *aStore = 0;` |
|       26 | 10643 | `		int nActual = 0;` |
|       26 | 10644 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10645 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10646 | `			ph7_hashmap_node *pNode;` |
|       26 | 10647 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10648 | `			if( nCount > 0 ){` |
|        3 | 10649 | `				sxu32 idx = 0;` |
|        4 | 10650 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10651 | `					nCount * sizeof(ph7_value *));` |
|        4 | 10652 | `				aStore = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10653 | `					nCount * sizeof(ph7_value));` |
|        3 | 10654 | `				if( apValues && aStore ){` |
|        3 | 10655 | `					pNode = pMap->pFirst;` |
|        7 | 10656 | `					while( pNode && idx < nCount ){` |
|        - | 10657 | `						/* Snapshot each source into stable storage: VmFiberSetupFrame reserves` |
|        - | 10658 | `						 * memory objects (VmExtractMemObj) before reading the args, which can` |
|        - | 10659 | `						 * reallocate (move) pVm->aMemObj and dangle a raw pool pointer. A` |
|        - | 10660 | `						 * shallow copy is a safe source — the referent and the heap-resident` |
|        - | 10661 | `						 * blob data survive the move (same sSafeVal idiom the hashmap inserters` |
|        - | 10662 | `						 * use); it owns nothing independently, so it needs no release. */` |
|        5 | 10663 | `						ph7_value *pSrc = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10664 | `						if( pSrc ){` |
|        5 | 10665 | `							aStore[idx] = *pSrc;` |
|        3 | 10666 | `						}else{` |
|      ! 0 | 10667 | `							PH7_MemObjInit(pVm, &aStore[idx]);` |
|        - | 10668 | `						}` |
|        5 | 10669 | `						apValues[idx] = &aStore[idx];` |
|        5 | 10670 | `						idx++;` |
|        5 | 10671 | `						pNode = pNode->pPrev;` |
|        1 | 10672 | `					}` |
|        3 | 10673 | `					nActual = (int)idx;` |
|        1 | 10674 | `				}` |
|        1 | 10675 | `			}` |
|       12 | 10676 | `		}` |
|       26 | 10677 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10678 | `		if( aStore ){` |
|        3 | 10679 | `			SyMemBackendFree(&pVm->sAllocator, aStore);` |
|        1 | 10680 | `		}` |
|       26 | 10681 | `		if( apValues ){` |
|        3 | 10682 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10683 | `		}` |
|        - | 10684 | `	}` |
|        - | 10685 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10686 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10687 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10688 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10689 | `		return PH7_ABORT;` |
|        - | 10690 | `	}` |
|       26 | 10691 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10692 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10693 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10694 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10695 | `		return PH7_ABORT;` |
|        - | 10696 | `	}` |
|       26 | 10697 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10698 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10699 | `		return PH7_EXCEPTION;` |
|        - | 10700 | `	}` |
|       26 | 10701 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10702 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10703 | `	return PH7_OK;` |
|       15 | 10704 |  |
|        - | 10705 | `/*` |
|        - | 10706 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10707 | ` */` |
|       36 | 10708 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10709 |  |
|       38 | 10710 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10711 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10712 | `	ph7_value sResult;` |
|        - | 10713 | `	ph7_value *pResumeVal;` |
|        - | 10714 | `	sxi32 rc;` |
|       38 | 10715 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10716 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10717 | `		return PH7_OK;` |
|        - | 10718 | `	}` |
|       38 | 10719 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10720 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10721 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10722 | `		return PH7_OK;` |
|        - | 10723 | `	}` |
|       38 | 10724 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10725 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10726 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10727 | `	}` |
|       36 | 10728 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10729 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10730 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10731 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10732 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10733 | `		return PH7_ABORT;` |
|        - | 10734 | `	}` |
|       36 | 10735 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10736 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10737 | `		return PH7_EXCEPTION;` |
|        - | 10738 | `	}` |
|       36 | 10739 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10740 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10741 | `	return PH7_OK;` |
|       20 | 10742 |  |
|        - | 10743 | `/*` |
|        - | 10744 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10745 | ` */` |
|        6 | 10746 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10747 |  |
|        8 | 10748 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10749 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10750 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10751 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10752 | `		return PH7_OK;` |
|        - | 10753 | `	}` |
|        8 | 10754 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10755 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10756 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10757 | `		return PH7_OK;` |
|        - | 10758 | `	}` |
|        8 | 10759 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10760 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10761 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10762 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10763 | `		}` |
|      ! 0 | 10764 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10765 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10766 | `	}` |
|        8 | 10767 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10768 | `	return PH7_OK;` |
|        5 | 10769 |  |
|        - | 10770 | `/*` |
|        - | 10771 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10772 | ` */` |
|        6 | 10773 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10774 |  |
|        - | 10775 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10776 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10777 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10778 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10779 | `	return PH7_OK;` |
|        4 | 10780 |  |
|      ! 0 | 10781 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10782 |  |
|        - | 10783 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 10784 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 10785 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10786 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 10787 | `	return PH7_OK;` |
|      ! 0 | 10788 |  |
|        6 | 10789 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10790 |  |
|        - | 10791 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10792 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10793 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10794 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 10795 | `	return PH7_OK;` |
|        4 | 10796 |  |
|        6 | 10797 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10798 |  |
|        - | 10799 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10800 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10801 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10802 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 10803 | `	return PH7_OK;` |
|        4 | 10804 |  |
|        - | 10805 | `/*` |
|        - | 10806 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 10807 | ` */` |
|        4 | 10808 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10809 |  |
|        5 | 10810 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10811 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 10812 | `	if( nArg < 1 ){` |
|      ! 0 | 10813 | `		return PH7_OK;` |
|        - | 10814 | `	}` |
|        5 | 10815 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 10816 | `	if( pExecCtx ){` |
|        5 | 10817 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 10818 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 10819 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 10820 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10821 | `			SyString sAttrName;` |
|        - | 10822 | `			ph7_value *pAttr;` |
|        5 | 10823 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 10824 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 10825 | `			if( pAttr ){` |
|        5 | 10826 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 10827 | `			}` |
|        2 | 10828 | `		}` |
|        2 | 10829 | `	}` |
|        5 | 10830 | `	return PH7_OK;` |
|        3 | 10831 |  |
|        - | 10832 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 10833 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 10834 |  |
|        - | 10835 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10836 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 10837 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 10838 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 10839 |  |
|      ! 0 | 10840 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 10841 |  |
|        - | 10842 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10843 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 10844 | `	ph7_exec_ctx *pCtx;` |
|        - | 10845 | `	ph7_vm_func *pFunc;` |
|        - | 10846 | `	ph7_value *pCallable;` |
|        - | 10847 | `	ph7_value *pCtxAttr;` |
|        - | 10848 | `	SyString sAttrName;` |
|        - | 10849 | `	/* Must not already be started */` |
|      ! 0 | 10850 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10851 | `	if( pCtx != 0 ){` |
|      ! 0 | 10852 | `		return SXERR_INVALID;` |
|        - | 10853 | `	}` |
|      ! 0 | 10854 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10855 | `		return SXERR_INVALID;` |
|        - | 10856 | `	}` |
|      ! 0 | 10857 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 10858 | `	/* Get the callable */` |
|      ! 0 | 10859 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 10860 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10861 | `	if( pCallable == 0 ){` |
|      ! 0 | 10862 | `		return SXERR_INVALID;` |
|        - | 10863 | `	}` |
|        - | 10864 | `	/* Resolve callable */` |
|      ! 0 | 10865 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10866 | `		SyString sName;` |
|        - | 10867 | `		SyHashEntry *pEntry;` |
|      ! 0 | 10868 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 10869 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 10870 | `		if( pEntry == 0 ){` |
|      ! 0 | 10871 | `			return SXERR_NOTFOUND;` |
|        - | 10872 | `		}` |
|      ! 0 | 10873 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 10874 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10875 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10876 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10877 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10878 | `		if( pMethod == 0 ){` |
|      ! 0 | 10879 | `			return SXERR_INVALID;` |
|        - | 10880 | `		}` |
|      ! 0 | 10881 | `		pClosureThis = pClosure;` |
|      ! 0 | 10882 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 10883 | `	}else{` |
|      ! 0 | 10884 | `		return SXERR_INVALID;` |
|        - | 10885 | `	}` |
|        - | 10886 | `	/* Create context */` |
|      ! 0 | 10887 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 10888 | `	if( pCtx == 0 ){` |
|      ! 0 | 10889 | `		return SXERR_MEM;` |
|        - | 10890 | `	}` |
|        - | 10891 | `	/* Store in __ctx */` |
|      ! 0 | 10892 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10893 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10894 | `	if( pCtxAttr ){` |
|      ! 0 | 10895 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 10896 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 10897 | `	}` |
|        - | 10898 | `	/* Set up frame with args */` |
|      ! 0 | 10899 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 10900 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 10901 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 10902 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 10903 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 10904 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 10905 |  |
|      ! 0 | 10906 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 10907 |  |
|      ! 0 | 10908 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10909 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 10910 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 10911 |  |
|      ! 0 | 10912 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10913 |  |
|      ! 0 | 10914 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10915 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 10916 |  |
|      ! 0 | 10917 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10918 |  |
|      ! 0 | 10919 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10920 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 10921 |  |
|      ! 0 | 10922 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10923 |  |
|      ! 0 | 10924 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10925 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 10926 | `	return &pCtx->sRetValue;` |
|      ! 0 | 10927 |  |
|        - | 10928 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 10929 | `/*` |
|        - | 10930 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 10931 | ` */` |
|       22 | 10932 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 10933 |  |
|        - | 10934 | `	ph7_generator *pGen;` |
|       24 | 10935 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 | 10936 | `	if( pGen == 0 ){` |
|      ! 0 | 10937 | `		return 0;` |
|        - | 10938 | `	}` |
|       24 | 10939 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 | 10940 | `	pGen->pCtx = pCtx;` |
|       24 | 10941 | `	pGen->iImplicitKey = 0;` |
|       24 | 10942 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 | 10943 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 10944 | `	/* Link the generator back to the exec context */` |
|       24 | 10945 | `	pCtx->pPrivate = pGen;` |
|       24 | 10946 | `	return pGen;` |
|       13 | 10947 |  |
|        - | 10948 | `/*` |
|        - | 10949 | ` * Release a generator and its execution context.` |
|        - | 10950 | ` */` |
|      ! 0 | 10951 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 10952 |  |
|      ! 0 | 10953 | `	if( pGen == 0 ){` |
|      ! 0 | 10954 | `		return;` |
|        - | 10955 | `	}` |
|      ! 0 | 10956 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 10957 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 10958 | `	if( pGen->pCtx ){` |
|      ! 0 | 10959 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 10960 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 10961 | `		pGen->pCtx = 0;` |
|      ! 0 | 10962 | `	}` |
|      ! 0 | 10963 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 10964 |  |
|        - | 10965 | `/*` |
|        - | 10966 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 10967 | ` */` |
|      236 | 10968 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 10969 |  |
|        - | 10970 | `	ph7_class_instance *pThis;` |
|        - | 10971 | `	SyString sAttr;` |
|        - | 10972 | `	ph7_value *pAttr;` |
|      238 | 10973 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10974 | `		return 0;` |
|        - | 10975 | `	}` |
|      238 | 10976 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 10977 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 10978 | `		return 0;` |
|        - | 10979 | `	}` |
|      238 | 10980 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 10981 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 10982 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 10983 | `		return 0;` |
|        - | 10984 | `	}` |
|      238 | 10985 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 10986 |  |
|        - | 10987 | `/*` |
|        - | 10988 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 10989 | ` */` |
|       22 | 10990 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10991 |  |
|        - | 10992 | `	ph7_generator *pGen;` |
|        - | 10993 | `	sxi32 rc;` |
|       24 | 10994 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 10995 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 10996 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 10997 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 10998 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 10999 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 11000 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 11001 | `	}` |
|       24 | 11002 | `	return PH7_OK;` |
|       13 | 11003 |  |
|        - | 11004 | `/*` |
|        - | 11005 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 11006 | ` */` |
|       68 | 11007 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11008 |  |
|        - | 11009 | `	ph7_generator *pGen;` |
|       70 | 11010 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 11011 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 11012 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 11013 | `	return PH7_OK;` |
|       36 | 11014 |  |
|        - | 11015 | `/*` |
|        - | 11016 | ` * Generator::current() — return the last yielded value.` |
|        - | 11017 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11018 | ` */` |
|       68 | 11019 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11020 |  |
|        - | 11021 | `	ph7_generator *pGen;` |
|        - | 11022 | `	sxi32 rc;` |
|       70 | 11023 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 11024 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 11025 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 11026 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11027 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11028 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11029 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11030 | `	}` |
|       70 | 11031 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 11032 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 11033 | `	}else{` |
|      ! 0 | 11034 | `		ph7_result_null(pCtx);` |
|        - | 11035 | `	}` |
|       70 | 11036 | `	return PH7_OK;` |
|       36 | 11037 |  |
|        - | 11038 | `/*` |
|        - | 11039 | ` * Generator::key() — return the last yielded key.` |
|        - | 11040 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11041 | ` */` |
|       12 | 11042 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11043 |  |
|        - | 11044 | `	ph7_generator *pGen;` |
|        - | 11045 | `	sxi32 rc;` |
|       13 | 11046 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 11047 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 11048 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 11049 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11050 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11051 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11052 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11053 | `	}` |
|       13 | 11054 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 11055 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 11056 | `	}else{` |
|      ! 0 | 11057 | `		ph7_result_null(pCtx);` |
|        - | 11058 | `	}` |
|       13 | 11059 | `	return PH7_OK;` |
|        7 | 11060 |  |
|        - | 11061 | `/*` |
|        - | 11062 | ` * Generator::next() — advance to the next yield point.` |
|        - | 11063 | ` */` |
|       60 | 11064 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11065 |  |
|        - | 11066 | `	ph7_generator *pGen;` |
|        - | 11067 | `	sxi32 rc;` |
|       62 | 11068 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 11069 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 11070 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 11071 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11072 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 11073 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 11074 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 11075 | `	}else{` |
|      ! 0 | 11076 | `		return PH7_OK;` |
|        - | 11077 | `	}` |
|       62 | 11078 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 11079 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 11080 | `	return PH7_OK;` |
|       32 | 11081 |  |
|        - | 11082 | `/*` |
|        - | 11083 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 11084 | ` */` |
|        4 | 11085 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11086 |  |
|        - | 11087 | `	ph7_generator *pGen;` |
|        - | 11088 | `	ph7_value *pSendVal;` |
|        - | 11089 | `	sxi32 rc;` |
|        5 | 11090 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 11091 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 11092 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 11093 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 11094 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 11095 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 11096 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 11097 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 11098 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 11099 | `	}else{` |
|      ! 0 | 11100 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11101 | `		return PH7_OK;` |
|        - | 11102 | `	}` |
|        5 | 11103 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 11104 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 11105 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 11106 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 11107 | `	}else{` |
|        3 | 11108 | `		ph7_result_null(pCtx);` |
|        - | 11109 | `	}` |
|        5 | 11110 | `	return PH7_OK;` |
|        3 | 11111 |  |
|        - | 11112 | `/*` |
|        - | 11113 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 11114 | ` *` |
|        - | 11115 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 11116 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 11117 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 11118 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 11119 | ` * the exception to the caller.` |
|        - | 11120 | ` */` |
|      ! 0 | 11121 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11122 |  |
|        - | 11123 | `	ph7_generator *pGen;` |
|        - | 11124 | `	const char *zMsg;` |
|        - | 11125 | `	int nLen;` |
|      ! 0 | 11126 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 11127 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11128 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 11129 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 11130 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 11131 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11132 | `			"Cannot throw into a closed generator");` |
|        - | 11133 | `	}` |
|        - | 11134 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 11135 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 11136 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 11137 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 11138 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 11139 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 11140 | `	nLen = 0;` |
|      ! 0 | 11141 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 11142 | `		/* Try to get the exception's message */` |
|        - | 11143 | `		SyString sAttr;` |
|        - | 11144 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 11145 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 11146 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 11147 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 11148 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 11149 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 11150 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 11151 | `		}` |
|      ! 0 | 11152 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 11153 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 11154 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 11155 | `	}` |
|      ! 0 | 11156 | `	(void)nLen;` |
|      ! 0 | 11157 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 11158 |  |
|        - | 11159 | `/*` |
|        - | 11160 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 11161 | ` */` |
|        2 | 11162 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11163 |  |
|        - | 11164 | `	ph7_generator *pGen;` |
|        3 | 11165 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11166 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 11167 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11168 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11169 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11170 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 11171 | `	}` |
|        3 | 11172 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 11173 | `	return PH7_OK;` |
|        2 | 11174 |  |
|        - | 11175 | `/*` |
|        - | 11176 | ` * Generator::__destruct() — clean up.` |
|        - | 11177 | ` */` |
|      ! 0 | 11178 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11179 |  |
|        - | 11180 | `	ph7_generator *pGen;` |
|      ! 0 | 11181 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 11182 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11183 | `	if( pGen ){` |
|      ! 0 | 11184 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 11185 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11186 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11187 | `			SyString sAttrName;` |
|        - | 11188 | `			ph7_value *pAttr;` |
|      ! 0 | 11189 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11190 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11191 | `			if( pAttr ){` |
|      ! 0 | 11192 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 11193 | `			}` |
|      ! 0 | 11194 | `		}` |
|      ! 0 | 11195 | `	}` |
|      ! 0 | 11196 | `	return PH7_OK;` |
|      ! 0 | 11197 |  |
|        - | 11198 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 11199 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 11200 | `/*` |
|        - | 11201 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 11202 | ` * the desired message.` |
|        - | 11203 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 11204 | ` * in 'api.c' for additional information.` |
|        - | 11205 | ` */` |
|      370 | 11206 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 11207 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 11208 | `	SyString *pString /* Message to output */` |
|        - | 11209 | `	)` |
|        2 | 11210 |  |
|      372 | 11211 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 11212 | `	sxi32 rc = SXRET_OK;` |
|        - | 11213 | `	/* Call the output consumer */` |
|      372 | 11214 | `	if( pString->nByte > 0 ){` |
|      372 | 11215 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 11216 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 11217 | `	}` |
|      372 | 11218 | `	return rc;` |
|        2 | 11219 |  |
|        - | 11220 | `/*` |
|        - | 11221 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 11222 | ` * callback to consume the formatted message.` |
|        - | 11223 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 11224 | ` * in 'api.c' for additional information.` |
|        - | 11225 | ` */` |
|        2 | 11226 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 11227 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 11228 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 11229 | `	va_list ap           /* Variable list of arguments */` |
|        - | 11230 | `	)` |
|        1 | 11231 |  |
|        3 | 11232 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 11233 | `	sxi32 rc = SXRET_OK;` |
|        - | 11234 | `	SyBlob sWorker;` |
|        - | 11235 | `	/* Format the message and call the output consumer */` |
|        3 | 11236 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 11237 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 11238 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 11239 | `		/* Consume the formatted message */` |
|        3 | 11240 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 11241 | `	}` |
|        3 | 11242 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 11243 | `	/* Release the working buffer */` |
|        3 | 11244 | `	SyBlobRelease(&sWorker);` |
|        3 | 11245 | `	return rc;` |
|        1 | 11246 |  |
|        - | 11247 | `/*` |
|        - | 11248 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 11249 | ` * This function never fail and always return a pointer` |
|        - | 11250 | ` * to a null terminated string.` |
|        - | 11251 | ` */` |
|       12 | 11252 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 11253 |  |
|       13 | 11254 | `	const char *zOp = "Unknown     ";` |
|       13 | 11255 | `	switch(nOp){` |
|        3 | 11256 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 11257 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 11258 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 11259 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 11260 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 11261 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 11262 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 11263 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 11264 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 11265 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 11266 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 11267 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 11268 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 11269 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 11270 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 11271 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 11272 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 11273 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 11274 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 11275 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 11276 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 11277 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 11278 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 11279 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 11280 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 11281 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 11282 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 11283 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 11284 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 11285 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 11286 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 11287 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 11288 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 11289 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 11290 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 11291 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 11292 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 11293 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 11294 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 11295 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 11296 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 11297 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 11298 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 11299 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 11300 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 11301 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 11302 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 11303 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 11304 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 11305 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 11306 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 11307 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 11308 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 11309 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 11310 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 11311 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 11312 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 11313 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 11314 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 11315 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 11316 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 11317 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 11318 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 11319 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 11320 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 11321 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 11322 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 11323 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 11324 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 11325 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 11326 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 11327 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 11328 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 11329 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 11330 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 11331 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 11332 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 11333 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 11334 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 11335 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 11336 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 11337 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 11338 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 11339 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 11340 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 11341 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 11342 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 11343 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 11344 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 11345 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 11346 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 11347 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 11348 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 11349 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 11350 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 11351 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 11352 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 11353 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 11354 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 11355 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 11356 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 11357 | `	default:` |
|      ! 0 | 11358 | `		break;` |
|        - | 11359 | `	}` |
|       13 | 11360 | `	return zOp;` |
|        1 | 11361 |  |
|        - | 11362 | `/*` |
|        - | 11363 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 11364 | ` * The xConsumer() callback which is an used defined function` |
|        - | 11365 | ` * is responsible of consuming the generated dump.` |
|        - | 11366 | ` */` |
|        2 | 11367 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 11368 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 11369 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 11370 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 11371 | `	)` |
|        1 | 11372 |  |
|        - | 11373 | `	sxi32 rc;` |
|        3 | 11374 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 11375 | `	return rc;` |
|        1 | 11376 |  |
|        - | 11377 | `/*` |
|        - | 11378 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 11379 | ` * outside a class body [i.e: global or function scope].` |
|        - | 11380 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 11381 | ` * in 'compile.c' for additional information.` |
|        - | 11382 | ` */` |
|       14 | 11383 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 11384 |  |
|       15 | 11385 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 11386 | `	/* Evaluate and expand constant value */` |
|       15 | 11387 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 11388 |  |
|        - | 11389 | `/*` |
|        - | 11390 | ` * Section:` |
|        - | 11391 | ` *  Function handling functions.` |
|        - | 11392 | ` * Status:` |
|        - | 11393 | ` *    Stable.` |
|        - | 11394 | ` */` |
|        - | 11395 | `/*` |
|        - | 11396 | ` * int func_num_args(void)` |
|        - | 11397 | ` *   Returns the number of arguments passed to the function.` |
|        - | 11398 | ` * Parameters` |
|        - | 11399 | ` *   None.` |
|        - | 11400 | ` * Return` |
|        - | 11401 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 11402 | ` *  or -1 if called from the globe scope.` |
|        - | 11403 | ` */` |
|      980 | 11404 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11405 |  |
|        - | 11406 | `	VmFrame *pFrame;` |
|        - | 11407 | `	ph7_vm *pVm;` |
|        - | 11408 | `	/* Point to the target VM */` |
|      982 | 11409 | `	pVm = pCtx->pVm;` |
|        - | 11410 | `	/* Current frame */` |
|      982 | 11411 | `	pFrame = pVm->pFrame;` |
|      982 | 11412 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      982 | 11413 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 11414 | `		SXUNUSED(nArg);` |
|      ! 0 | 11415 | `		SXUNUSED(apArg);` |
|        - | 11416 | `		/* Global frame,return -1 */` |
|      ! 0 | 11417 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 11418 | `		return SXRET_OK;` |
|        - | 11419 | `	}` |
|        - | 11420 | `	/* Total number of arguments passed to the enclosing function */` |
|      982 | 11421 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      982 | 11422 | `	ph7_result_int(pCtx,nArg);` |
|      982 | 11423 | `	return SXRET_OK;` |
|      492 | 11424 |  |
|        - | 11425 | `/*` |
|        - | 11426 | ` * value func_get_arg(int $arg_num)` |
|        - | 11427 | ` *   Return an item from the argument list.` |
|        - | 11428 | ` * Parameters` |
|        - | 11429 | ` *  Argument number(index start from zero).` |
|        - | 11430 | ` * Return` |
|        - | 11431 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 11432 | ` */` |
|       22 | 11433 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11434 |  |
|       24 | 11435 | `	ph7_value *pObj = 0;` |
|       24 | 11436 | `	VmSlot *pSlot = 0;` |
|        - | 11437 | `	VmFrame *pFrame;` |
|        - | 11438 | `	ph7_vm *pVm;` |
|        - | 11439 | `	/* Point to the target VM */` |
|       24 | 11440 | `	pVm = pCtx->pVm;` |
|        - | 11441 | `	/* Current frame */` |
|       24 | 11442 | `	pFrame = pVm->pFrame;` |
|       24 | 11443 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 11444 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 11445 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 11446 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 11447 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11448 | `		return SXRET_OK;` |
|        - | 11449 | `	}` |
|        - | 11450 | `	/* Extract the desired index */` |
|       21 | 11451 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 11452 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 11453 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 11454 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11455 | `		return SXRET_OK;` |
|        - | 11456 | `	}` |
|        - | 11457 | `	/* Extract the desired argument */` |
|       21 | 11458 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 11459 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 11460 | `			/* Return the desired argument */` |
|       21 | 11461 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 11462 | `		}else{` |
|        - | 11463 | `			/* No such argument,return false */` |
|      ! 0 | 11464 | `			ph7_result_bool(pCtx,0);` |
|        - | 11465 | `		}` |
|       11 | 11466 | `	}else{` |
|        - | 11467 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11468 | `		ph7_result_bool(pCtx,0);` |
|        - | 11469 | `	}` |
|       21 | 11470 | `	return SXRET_OK;` |
|       13 | 11471 |  |
|        - | 11472 | `/*` |
|        - | 11473 | ` * array func_get_args_byref(void)` |
|        - | 11474 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11475 | ` * Parameters` |
|        - | 11476 | ` *  None.` |
|        - | 11477 | ` * Return` |
|        - | 11478 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 11479 | ` *  member of the current user-defined function's argument list.` |
|        - | 11480 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11481 | ` * NOTE:` |
|        - | 11482 | ` *  Arguments are returned to the array by reference.` |
|        - | 11483 | ` */` |
|        2 | 11484 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11485 |  |
|        - | 11486 | `	ph7_value *pArray;` |
|        - | 11487 | `	VmFrame *pFrame;` |
|        - | 11488 | `	VmSlot *aSlot;` |
|        - | 11489 | `	sxu32 n;` |
|        - | 11490 | `	/* Point to the current frame */` |
|        3 | 11491 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11492 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11493 | `	if( pFrame->pParent == 0 ){` |
|        - | 11494 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11495 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11496 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11497 | `		return SXRET_OK;` |
|        - | 11498 | `	}` |
|        - | 11499 | `	/* Create a new array */` |
|        3 | 11500 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11501 | `	if( pArray == 0 ){` |
|      ! 0 | 11502 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11503 | `		SXUNUSED(apArg);` |
|      ! 0 | 11504 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11505 | `		return SXRET_OK;` |
|        - | 11506 | `	}` |
|        - | 11507 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11508 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11509 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11510 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11511 | `	}` |
|        - | 11512 | `	/* Return the freshly created array */` |
|        3 | 11513 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11514 | `	return SXRET_OK;` |
|        2 | 11515 |  |
|        - | 11516 | `/*` |
|        - | 11517 | ` * array func_get_args(void)` |
|        - | 11518 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11519 | ` * Parameters` |
|        - | 11520 | ` *  None.` |
|        - | 11521 | ` * Return` |
|        - | 11522 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11523 | ` *  member of the current user-defined function's argument list.` |
|        - | 11524 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11525 | ` */` |
|       88 | 11526 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11527 |  |
|       90 | 11528 | `	ph7_value *pObj = 0;` |
|        - | 11529 | `	ph7_value *pArray;` |
|        - | 11530 | `	VmFrame *pFrame;` |
|        - | 11531 | `	VmSlot *aSlot;` |
|        - | 11532 | `	sxu32 n;` |
|        - | 11533 | `	/* Point to the current frame */` |
|       90 | 11534 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 11535 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 11536 | `	if( pFrame->pParent == 0 ){` |
|        - | 11537 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11538 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11539 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11540 | `		return SXRET_OK;` |
|        - | 11541 | `	}` |
|        - | 11542 | `	/* Create a new array */` |
|       90 | 11543 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 11544 | `	if( pArray == 0 ){` |
|      ! 0 | 11545 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11546 | `		SXUNUSED(apArg);` |
|      ! 0 | 11547 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11548 | `		return SXRET_OK;` |
|        - | 11549 | `	}` |
|        - | 11550 | `	/* Start filling the array with the given arguments */` |
|       90 | 11551 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 11552 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 11553 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 11554 | `		if( pObj ){` |
|      134 | 11555 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 11556 | `		}` |
|       68 | 11557 | `	}` |
|        - | 11558 | `	/* Return the freshly created array */` |
|       90 | 11559 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 11560 | `	return SXRET_OK;` |
|       46 | 11561 |  |
|        - | 11562 | `/*` |
|        - | 11563 | ` * bool function_exists(string $name)` |
|        - | 11564 | ` *  Return TRUE if the given function has been defined.` |
|        - | 11565 | ` * Parameters` |
|        - | 11566 | ` *  The name of the desired function.` |
|        - | 11567 | ` * Return` |
|        - | 11568 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 11569 | ` */` |
|     1748 | 11570 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11571 |  |
|        - | 11572 | `	const char *zName;` |
|        - | 11573 | `	ph7_vm *pVm;` |
|        - | 11574 | `	int nLen;` |
|        - | 11575 | `	int res;` |
|     1750 | 11576 | `	if( nArg < 1 ){` |
|        - | 11577 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11578 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11579 | `		return SXRET_OK;` |
|        - | 11580 | `	}` |
|        - | 11581 | `	/* Point to the target VM */` |
|     1750 | 11582 | `	pVm = pCtx->pVm;` |
|        - | 11583 | `	/* Extract the function name */` |
|     1750 | 11584 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11585 | `	/* Assume the function is not defined */` |
|     1750 | 11586 | `	res = 0;` |
|        - | 11587 | `	/* Perform the lookup */` |
|     2622 | 11588 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1744 | 11589 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11590 | `			/* Function is defined */` |
|      268 | 11591 | `			res = 1;` |
|      133 | 11592 | `	}` |
|     1750 | 11593 | `	ph7_result_bool(pCtx,res);` |
|     1750 | 11594 | `	return SXRET_OK;` |
|      876 | 11595 |  |
|        - | 11596 | `/*` |
|        - | 11597 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11598 | ` * [i.e: Whether it is callable or not].` |
|        - | 11599 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11600 | ` */` |
|    23890 | 11601 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11602 |  |
|    23892 | 11603 | `	int res = 0;` |
|    23892 | 11604 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11605 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11606 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11607 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11608 | `		 * standard PHP behavior. */` |
|       20 | 11609 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11610 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11611 | `			res = 1;` |
|       10 | 11612 | `		}` |
|        9 | 11613 | `		(void)CallInvoke;` |
|    23883 | 11614 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11615 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11616 | `		if( pMap->nEntry == 2 ){` |
|        - | 11617 | `			ph7_class *pClass;` |
|        - | 11618 | `			ph7_value *pV;` |
|        - | 11619 | `			/* Extract the target class */` |
|       12 | 11620 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11621 | `			if( pV ){` |
|       12 | 11622 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11623 | `				if( pClass ){` |
|        - | 11624 | `					ph7_class_method *pMethod;` |
|        - | 11625 | `					/* Extract the target method */` |
|       10 | 11626 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11627 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11628 | `						/* Perform the lookup */` |
|       10 | 11629 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11630 | `						if( pMethod ){` |
|        - | 11631 | `							/* Method is callable */` |
|        5 | 11632 | `							res = 1;` |
|        2 | 11633 | `						}` |
|        4 | 11634 | `					}` |
|        4 | 11635 | `				}` |
|        5 | 11636 | `			}` |
|        7 | 11637 | `		}` |
|    23861 | 11638 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11639 | `		const char *zName;` |
|        - | 11640 | `		int nLen;` |
|        - | 11641 | `		/* Extract the name */` |
|     5878 | 11642 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11643 | `		/* Perform the lookup */` |
|     5893 | 11644 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11645 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11646 | `				/* Function is callable */` |
|     5860 | 11647 | `				res = 1;` |
|     2929 | 11648 | `		}` |
|     2938 | 11649 | `	}` |
|    23892 | 11650 | `	return res;` |
|        2 | 11651 |  |
|        - | 11652 | `/*` |
|        - | 11653 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11654 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11655 | ` * Parameters` |
|        - | 11656 | ` * $name` |
|        - | 11657 | ` *    The callback function to check` |
|        - | 11658 | ` * $syntax_only` |
|        - | 11659 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11660 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11661 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11662 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11663 | ` *    a string.` |
|        - | 11664 | ` * Return` |
|        - | 11665 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11666 | ` */` |
|       20 | 11667 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11668 |  |
|        - | 11669 | `	ph7_vm *pVm;` |
|        - | 11670 | `	int res;` |
|       21 | 11671 | `	if( nArg < 1 ){` |
|        - | 11672 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11673 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11674 | `		return SXRET_OK;` |
|        - | 11675 | `	}` |
|        - | 11676 | `	/* Point to the target VM */` |
|       21 | 11677 | `	pVm = pCtx->pVm;` |
|        - | 11678 | `	/* Perform the requested operation */` |
|       21 | 11679 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11680 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11681 | `	return SXRET_OK;` |
|       11 | 11682 |  |
|        - | 11683 | `/*` |
|        - | 11684 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11685 | ` * defined below.` |
|        - | 11686 | ` */` |
|     1306 | 11687 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11688 |  |
|     1307 | 11689 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11690 | `	ph7_value sName;` |
|        - | 11691 | `	sxi32 rc;` |
|        - | 11692 | `	/* Prepare the function name for insertion */` |
|     1307 | 11693 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1307 | 11694 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11695 | `	/* Perform the insertion */` |
|     1307 | 11696 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1307 | 11697 | `	PH7_MemObjRelease(&sName);` |
|     1307 | 11698 | `	return rc;` |
|        1 | 11699 |  |
|        - | 11700 | `/*` |
|        - | 11701 | ` * array get_defined_functions(void)` |
|        - | 11702 | ` *  Returns an array of all defined functions.` |
|        - | 11703 | ` * Parameter` |
|        - | 11704 | ` *  None.` |
|        - | 11705 | ` * Return` |
|        - | 11706 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11707 | ` *  both built-in (internal) and user-defined.` |
|        - | 11708 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11709 | ` *  defined ones using $arr["user"].` |
|        - | 11710 | ` * Note:` |
|        - | 11711 | ` *  NULL is returned on failure.` |
|        - | 11712 | ` */` |
|        2 | 11713 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11714 |  |
|        - | 11715 | `	ph7_value *pArray,*pEntry;` |
|        - | 11716 | `	/* NOTE:` |
|        - | 11717 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11718 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11719 | `	 */` |
|        3 | 11720 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11721 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11722 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11723 | `		SXUNUSED(apArg);` |
|        - | 11724 | `		/* Return NULL */` |
|      ! 0 | 11725 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11726 | `		return SXRET_OK;` |
|        - | 11727 | `	}` |
|        3 | 11728 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11729 | `	if( pEntry == 0 ){` |
|        - | 11730 | `		/* Return NULL */` |
|      ! 0 | 11731 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11732 | `		return SXRET_OK;` |
|        - | 11733 | `	}` |
|        - | 11734 | `	/* Fill with the appropriate information */` |
|        3 | 11735 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11736 | `	/* Create the 'internal' index */` |
|        3 | 11737 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11738 | `	/* Create the user-func array */` |
|        3 | 11739 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11740 | `	if( pEntry == 0 ){` |
|        - | 11741 | `		/* Return NULL */` |
|      ! 0 | 11742 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11743 | `		return SXRET_OK;` |
|        - | 11744 | `	}` |
|        - | 11745 | `	/* Fill with the appropriate information */` |
|        3 | 11746 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11747 | `	/* Create the 'user' index */` |
|        3 | 11748 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11749 | `	/* Return the multi-dimensional array */` |
|        3 | 11750 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11751 | `	return SXRET_OK;` |
|        2 | 11752 |  |
|        - | 11753 | `/*` |
|        - | 11754 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11755 | ` *  Register a function for execution on shutdown.` |
|        - | 11756 | ` * Note` |
|        - | 11757 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11758 | ` *  be called in the same order as they were registered.` |
|        - | 11759 | ` * Parameters` |
|        - | 11760 | ` *  $callback` |
|        - | 11761 | ` *   The shutdown callback to register.` |
|        - | 11762 | ` * $param` |
|        - | 11763 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11764 | ` * Return` |
|        - | 11765 | ` *  Nothing.` |
|        - | 11766 | ` */` |
|       12 | 11767 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11768 |  |
|        - | 11769 | `	VmShutdownCB sEntry;` |
|        - | 11770 | `	int i,j;` |
|       14 | 11771 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11772 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11773 | `		return PH7_OK;` |
|        - | 11774 | `	}` |
|        - | 11775 | `	/* Zero the Entry */` |
|       14 | 11776 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11777 | `	/* Initialize fields */` |
|       14 | 11778 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11779 | `	/* Save the callback name for later invocation name */` |
|       14 | 11780 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      134 | 11781 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      122 | 11782 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       62 | 11783 | `	}` |
|        - | 11784 | `	/* Copy arguments */` |
|       14 | 11785 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 11786 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 11787 | `			/* Limit reached */` |
|      ! 0 | 11788 | `			break;` |
|        - | 11789 | `		}` |
|      ! 0 | 11790 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 11791 | `	}` |
|       14 | 11792 | `	sEntry.nArg = j;` |
|        - | 11793 | `	/* Install the callback */` |
|       14 | 11794 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       14 | 11795 | `	return PH7_OK;` |
|        8 | 11796 |  |
|        - | 11797 | `/*` |
|        - | 11798 | ` * Section:` |
|        - | 11799 | ` *  Class handling functions.` |
|        - | 11800 | ` * Status:` |
|        - | 11801 | ` *    Stable.` |
|        - | 11802 | ` */` |
|        - | 11803 | `/*` |
|        - | 11804 | ` * Extract the top active class. NULL is returned` |
|        - | 11805 | ` * if the class stack is empty.` |
|        - | 11806 | ` */` |
|      986 | 11807 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 11808 |  |
|      988 | 11809 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 11810 | `	ph7_class **apClass;` |
|      988 | 11811 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 11812 | `		/* Empty stack,return NULL */` |
|       15 | 11813 | `		return 0;` |
|        - | 11814 | `	}` |
|        - | 11815 | `	/* Peek the last entry */` |
|      974 | 11816 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      974 | 11817 | `	return apClass[pSet->nUsed - 1];` |
|      495 | 11818 |  |
|        - | 11819 | `/*` |
|        - | 11820 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 11821 | ` *   Get the class that declared the currently executing method.` |
|        - | 11822 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 11823 | ` *` |
|        - | 11824 | ` * Parameters` |
|        - | 11825 | ` *   pVm: Target VM` |
|        - | 11826 | ` *` |
|        - | 11827 | ` * Return` |
|        - | 11828 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 11829 | ` *   - Not executing within a class method` |
|        - | 11830 | ` *` |
|        - | 11831 | ` * Note` |
|        - | 11832 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 11833 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 11834 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 11835 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 11836 | ` *   declaring class.` |
|        - | 11837 | ` */` |
|       98 | 11838 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 11839 |  |
|      100 | 11840 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11841 | `	ph7_vm_func *pVmFunc;` |
|        - | 11842 |  |
|        - | 11843 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 11844 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 11845 |  |
|        - | 11846 | `	/* Check if we're in a method context */` |
|      100 | 11847 | `	if( pFrame->pParent ){` |
|       96 | 11848 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 11849 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 11850 | `			/* Return the declaring class */` |
|       96 | 11851 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 11852 | `		}` |
|      ! 0 | 11853 | `	}` |
|        - | 11854 |  |
|        5 | 11855 | `	return 0;` |
|       51 | 11856 |  |
|        - | 11857 |  |
|        - | 11858 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 11859 | `/*` |
|        - | 11860 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 11861 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 11862 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 11863 | ` * return value indicates failure.` |
|        - | 11864 | ` */` |
|        - | 11865 | `/*` |
|        - | 11866 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 11867 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 11868 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 11869 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 11870 | ` */` |
|     2482 | 11871 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 11872 | `	ph7_vm *pVm,` |
|        - | 11873 | `	ph7_class_instance *pThis,` |
|        - | 11874 | `	ph7_class_method *pMethod,` |
|        - | 11875 | `	ph7_value *pResult,` |
|        - | 11876 | `	int nArg,` |
|        - | 11877 | `	ph7_value **apArg,` |
|        - | 11878 | `	VmCallArgMap *pMap` |
|        - | 11879 | `	)` |
|        2 | 11880 |  |
|        - | 11881 | `	ph7_value *aStack;` |
|        - | 11882 | `	VmInstr aInstr[2];` |
|        - | 11883 | `	int iCursor;` |
|        - | 11884 | `	int i;` |
|        - | 11885 | `	sxi32 rc;` |
|     2484 | 11886 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2484 | 11887 | `	if( aStack == 0 ){` |
|      ! 0 | 11888 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11889 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 11890 | `		return SXERR_MEM;` |
|        - | 11891 | `	}` |
|     4028 | 11892 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1546 | 11893 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1546 | 11894 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      774 | 11895 | `	}` |
|     2484 | 11896 | `	iCursor = nArg + 1;` |
|     2484 | 11897 | `	if( pThis ){` |
|     2478 | 11898 | `		pThis->iRef++;` |
|     2478 | 11899 | `		aStack[i].x.pOther = pThis;` |
|     2478 | 11900 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1238 | 11901 | `	}` |
|     2484 | 11902 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2484 | 11903 | `	i++;` |
|     2484 | 11904 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2484 | 11905 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2484 | 11906 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2484 | 11907 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2484 | 11908 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2484 | 11909 | `	aInstr[0].iP1 = nArg;` |
|     2484 | 11910 | `	aInstr[0].iP2 = 0;` |
|     2484 | 11911 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2484 | 11912 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2484 | 11913 | `	aInstr[1].iP1 = 1;` |
|     2484 | 11914 | `	aInstr[1].iP2 = 0;` |
|     2484 | 11915 | `	aInstr[1].p3  = 0;` |
|     2484 | 11916 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2484 | 11917 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11918 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 11919 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2484 | 11920 | `	return rc;` |
|     1243 | 11921 |  |
|     1924 | 11922 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 11923 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 11924 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 11925 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 11926 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 11927 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 11928 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 11929 | `	)` |
|        2 | 11930 |  |
|     1926 | 11931 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 11932 |  |
|        - | 11933 | `/*` |
|        - | 11934 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 11935 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 11936 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 11937 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 11938 | ` *` |
|        - | 11939 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 11940 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 11941 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 11942 | ` *` |
|        - | 11943 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 11944 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 11945 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 11946 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 11947 | ` *` |
|        - | 11948 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 11949 | ` */` |
|      174 | 11950 | `static sxi32 VmCallObjectInvoke(` |
|        - | 11951 | `	ph7_vm *pVm,` |
|        - | 11952 | `	ph7_class_instance *pThis,` |
|        - | 11953 | `	int nArg,` |
|        - | 11954 | `	ph7_value **apArg,` |
|        - | 11955 | `	ph7_value *pResult,` |
|        - | 11956 | `	VmCallArgMap *pMap` |
|        - | 11957 | `	)` |
|        2 | 11958 |  |
|        - | 11959 | `	ph7_class_method *pMethod;` |
|      176 | 11960 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      176 | 11961 | `	if( pMethod == 0 ){` |
|       13 | 11962 | `		if( pResult ){` |
|       13 | 11963 | `			PH7_MemObjRelease(pResult);` |
|        6 | 11964 | `		}` |
|       13 | 11965 | `		return SXERR_INVALID;` |
|        - | 11966 | `	}` |
|      164 | 11967 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       89 | 11968 |  |
|        - | 11969 | `/*` |
|        - | 11970 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 11971 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 11972 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 11973 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 11974 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 11975 | ` * lookup or 'goto Exception').` |
|        - | 11976 | ` *` |
|        - | 11977 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 11978 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 11979 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 11980 | ` * reported.` |
|        - | 11981 | ` */` |
|       12 | 11982 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 11983 |  |
|        - | 11984 | `	ph7_class *pErrorClass;` |
|       13 | 11985 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 11986 | `	ph7_class_method *pCons;` |
|        - | 11987 | `	VmFrame *pThrowFrame;` |
|        - | 11988 | `	char zMsg[256];` |
|        - | 11989 | `	int nMsg;` |
|        - | 11990 | `	sxi32 rc;` |
|       25 | 11991 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 11992 | `		"Object of type %.*s is not callable",` |
|       12 | 11993 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 11994 | `		pThis->pClass->sName.zString);` |
|       13 | 11995 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 11996 | `	if( pErrorClass ){` |
|       13 | 11997 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 11998 | `	}` |
|       13 | 11999 | `	if( pErrInst == 0 ){` |
|        - | 12000 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 12001 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 12002 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 12003 | `		 * visible to the user. */` |
|      ! 0 | 12004 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 12005 | `		return SXERR_ABORT;` |
|        - | 12006 | `	}` |
|       13 | 12007 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 12008 | `	if( pCons ){` |
|        - | 12009 | `		ph7_value sArg;` |
|        - | 12010 | `		ph7_value *apMsg[1];` |
|        - | 12011 | `		SyString sMsgStr;` |
|       13 | 12012 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 12013 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 12014 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 12015 | `		apMsg[0] = &sArg;` |
|       13 | 12016 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 12017 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 12018 | `	}` |
|        - | 12019 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 12020 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 12021 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 12022 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 12023 | `	if( pThrowFrame ){` |
|       13 | 12024 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 12025 | `	}` |
|       13 | 12026 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 12027 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 12028 | `	return rc;` |
|        7 | 12029 |  |
|        - | 12030 | `/*` |
|        - | 12031 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 12032 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 12033 | ` * in the apArg[] array.` |
|        - | 12034 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12035 | ` * return value indicates failure.` |
|        - | 12036 | ` */` |
|     1214 | 12037 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 12038 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12039 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12040 | `	int nArg,          /* Total number of given arguments */` |
|        - | 12041 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 12042 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 12043 | `	)` |
|        2 | 12044 |  |
|        - | 12045 | `	ph7_value *aStack;` |
|        - | 12046 | `	VmInstr aInstr[2];` |
|        - | 12047 | `	int i;` |
|     1216 | 12048 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 12049 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 12050 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 12051 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      140 | 12052 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 12053 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 12054 | `			nArg,apArg,pResult,0);` |
|        - | 12055 | `	}` |
|     1124 | 12056 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 12057 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 12058 | `		if( pResult ){` |
|        - | 12059 | `			/* Assume a null return value */` |
|      ! 0 | 12060 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12061 | `		}` |
|      511 | 12062 | `		return SXERR_INVALID;` |
|        - | 12063 | `	}` |
|      614 | 12064 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 12065 | `		/* Class method */` |
|       15 | 12066 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 12067 | `		ph7_class_method *pMethod = 0;` |
|       15 | 12068 | `		ph7_class_instance *pThis = 0;` |
|       15 | 12069 | `		ph7_class *pClass = 0;` |
|        - | 12070 | `		ph7_value *pValue;` |
|        - | 12071 | `		sxi32 rc;` |
|       15 | 12072 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 12073 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 12074 | `			if( pResult ){` |
|        - | 12075 | `				/* Assume a null return value */` |
|      ! 0 | 12076 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12077 | `			}` |
|      ! 0 | 12078 | `			return SXRET_OK;` |
|        - | 12079 | `		}` |
|        - | 12080 | `		/* Extract the class name or an instance of it */` |
|       15 | 12081 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 12082 | `		if( pValue ){` |
|       15 | 12083 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 12084 | `		}` |
|       15 | 12085 | `		if( pClass == 0 ){` |
|        - | 12086 | `			/* No such class,return NULL */` |
|      ! 0 | 12087 | `			if( pResult ){` |
|      ! 0 | 12088 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12089 | `			}` |
|      ! 0 | 12090 | `			return SXRET_OK;` |
|        - | 12091 | `		}` |
|       15 | 12092 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 12093 | `			/* Point to the class instance */` |
|        9 | 12094 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 12095 | `		}` |
|        - | 12096 | `		/* Try to extract the method */` |
|       15 | 12097 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 12098 | `		if( pValue ){` |
|       15 | 12099 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 12100 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 12101 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 12102 | `			}` |
|        7 | 12103 | `		}` |
|       15 | 12104 | `		if( pMethod == 0 ){` |
|        - | 12105 | `			/* No such method,return NULL */` |
|      ! 0 | 12106 | `			if( pResult ){` |
|      ! 0 | 12107 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12108 | `			}` |
|      ! 0 | 12109 | `			return SXRET_OK;` |
|        - | 12110 | `		}` |
|        - | 12111 | `		/* Call the class method */` |
|       15 | 12112 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 12113 | `		return rc;` |
|        - | 12114 | `	}` |
|        - | 12115 | `	/* Create a new operand stack */` |
|      600 | 12116 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      600 | 12117 | `	if( aStack == 0 ){` |
|      ! 0 | 12118 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12119 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 12120 | `		if( pResult ){` |
|        - | 12121 | `			/* Assume a null return value */` |
|      ! 0 | 12122 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12123 | `		}` |
|      ! 0 | 12124 | `		return SXERR_MEM;` |
|        - | 12125 | `	}` |
|        - | 12126 | `	/* Fill the operand stack with the given arguments */` |
|     1902 | 12127 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1304 | 12128 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 12129 | `		/*` |
|        - | 12130 | `		 * Symisc eXtension:` |
|        - | 12131 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 12132 | `		 */` |
|     1304 | 12133 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      653 | 12134 | `	}` |
|        - | 12135 | `	/* Push the function name */` |
|      600 | 12136 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      600 | 12137 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 12138 | `	/* Emit the CALL istruction */` |
|      600 | 12139 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      600 | 12140 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      600 | 12141 | `	aInstr[0].iP2 = 0;` |
|      600 | 12142 | `	aInstr[0].p3  = 0;` |
|        - | 12143 | `	/* Emit the DONE instruction */` |
|      600 | 12144 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      600 | 12145 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      600 | 12146 | `	aInstr[1].iP2 = 0;` |
|      600 | 12147 | `	aInstr[1].p3  = 0;` |
|        - | 12148 | `	/* Execute the function body (if available) */` |
|        - | 12149 | `	{` |
|        - | 12150 | `		sxi32 rcExec;` |
|      600 | 12151 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 12152 | `		/* Clean up the mess left behind */` |
|      600 | 12153 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12154 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      600 | 12155 | `		return rcExec;` |
|        - | 12156 | `	}` |
|      609 | 12157 |  |
|        - | 12158 | `/*` |
|        - | 12159 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 12160 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 12161 | ` * parameter.` |
|        - | 12162 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12163 | ` * return value indicates failure.` |
|        - | 12164 | ` */` |
|      240 | 12165 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 12166 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12167 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12168 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 12169 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 12170 | `	)` |
|        1 | 12171 |  |
|        - | 12172 | `	ph7_value *pArg;` |
|        - | 12173 | `	SySet aArg;` |
|        - | 12174 | `	va_list ap;` |
|        - | 12175 | `	sxi32 rc;` |
|      241 | 12176 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 12177 | `	/* Copy arguments one after one */` |
|      241 | 12178 | `	va_start(ap,pResult);` |
|      399 | 12179 | `	for(;;){` |
|      799 | 12180 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 12181 | `		if( pArg == 0 ){` |
|      241 | 12182 | `			break;` |
|        - | 12183 | `		}` |
|      559 | 12184 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 12185 | `	}` |
|        - | 12186 | `	/* Call the core routine */` |
|      241 | 12187 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 12188 | `	/* Cleanup */` |
|      241 | 12189 | `	SySetRelease(&aArg);` |
|      241 | 12190 | `	return rc;` |
|        1 | 12191 |  |
|        - | 12192 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 12193 | `/*` |
|        - | 12194 | ` * bool defined(string $name)` |
|        - | 12195 | ` *  Checks whether a given named constant exists.` |
|        - | 12196 | ` * Parameter:` |
|        - | 12197 | ` *  Name of the desired constant.` |
|        - | 12198 | ` * Return` |
|        - | 12199 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 12200 | ` */` |
|       26 | 12201 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12202 |  |
|        - | 12203 | `	const char *zName;` |
|       28 | 12204 | `	int nLen = 0;` |
|       28 | 12205 | `	int res = 0;` |
|       28 | 12206 | `	if( nArg < 1 ){` |
|        - | 12207 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 12208 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 12209 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12210 | `		return SXRET_OK;` |
|        - | 12211 | `	}` |
|        - | 12212 | `	/* Extract constant name */` |
|       28 | 12213 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12214 | `	/* Perform the lookup */` |
|       28 | 12215 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12216 | `		/* Already defined */` |
|       26 | 12217 | `		res = 1;` |
|       12 | 12218 | `	}` |
|       28 | 12219 | `	ph7_result_bool(pCtx,res);` |
|       28 | 12220 | `	return SXRET_OK;` |
|       15 | 12221 |  |
|        - | 12222 | `/*` |
|        - | 12223 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 12224 | ` * below.` |
|        - | 12225 | ` */` |
|       16 | 12226 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 12227 |  |
|       18 | 12228 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 12229 | `	/* Expand constant value */` |
|       18 | 12230 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       18 | 12231 |  |
|        - | 12232 | `/*` |
|        - | 12233 | ` * bool define(string $constant_name,expression value)` |
|        - | 12234 | ` *  Defines a named constant at runtime.` |
|        - | 12235 | ` * Parameter:` |
|        - | 12236 | ` *  $constant_name` |
|        - | 12237 | ` *   The name of the constant` |
|        - | 12238 | ` *  $value` |
|        - | 12239 | ` *   Constant value` |
|        - | 12240 | ` * Return:` |
|        - | 12241 | ` *   TRUE on success,FALSE on failure.` |
|        - | 12242 | ` */` |
|       14 | 12243 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12244 |  |
|        - | 12245 | `	const char *zName;  /* Constant name */` |
|        - | 12246 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       16 | 12247 | `	int nLen = 0;       /* Name length */` |
|        - | 12248 | `	sxi32 rc;` |
|       16 | 12249 | `	if( nArg < 2 ){` |
|        - | 12250 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 12251 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 12252 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12253 | `		return SXRET_OK;` |
|        - | 12254 | `	}` |
|       16 | 12255 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 12256 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 12257 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12258 | `		return SXRET_OK;` |
|        - | 12259 | `	}` |
|        - | 12260 | `	/* Extract constant name */` |
|       16 | 12261 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       16 | 12262 | `	if( nLen < 1 ){` |
|      ! 0 | 12263 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 12264 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12265 | `		return SXRET_OK;` |
|        - | 12266 | `	}` |
|        - | 12267 | `	/* Duplicate constant value */` |
|       16 | 12268 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       16 | 12269 | `	if( pValue == 0 ){` |
|      ! 0 | 12270 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12271 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12272 | `		return SXRET_OK;` |
|        - | 12273 | `	}` |
|        - | 12274 | `	/* Initialize the memory object */` |
|       16 | 12275 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 12276 | `	/* Register the constant */` |
|       16 | 12277 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       16 | 12278 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12279 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 12280 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12281 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12282 | `		return SXRET_OK;` |
|        - | 12283 | `	}` |
|        - | 12284 | `	/* Duplicate constant value */` |
|       16 | 12285 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       16 | 12286 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 12287 | `		/* Lower case the constant name */` |
|      ! 0 | 12288 | `		char *zCur = (char *)zName;` |
|      ! 0 | 12289 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 12290 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 12291 | `				/* UTF-8 stream */` |
|      ! 0 | 12292 | `				zCur++;` |
|      ! 0 | 12293 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 12294 | `					zCur++;` |
|      ! 0 | 12295 | `				}` |
|      ! 0 | 12296 | `				continue;` |
|        - | 12297 | `			}` |
|      ! 0 | 12298 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 12299 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 12300 | `				zCur[0] = (char)c;` |
|      ! 0 | 12301 | `			}` |
|      ! 0 | 12302 | `			zCur++;` |
|      ! 0 | 12303 | `		}` |
|        - | 12304 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|        - | 12305 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|        - | 12306 | `		 * one on a later overwrite would dangle the other. */` |
|        - | 12307 | `		{` |
|      ! 0 | 12308 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      ! 0 | 12309 | `			if( pAlias ){` |
|      ! 0 | 12310 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|      ! 0 | 12311 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|      ! 0 | 12312 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|      ! 0 | 12313 | `			}` |
|        - | 12314 | `		}` |
|      ! 0 | 12315 | `	}` |
|        - | 12316 | `	/* All done,return TRUE */` |
|       16 | 12317 | `	ph7_result_bool(pCtx,1);` |
|       16 | 12318 | `	return SXRET_OK;` |
|        9 | 12319 |  |
|        - | 12320 | `/*` |
|        - | 12321 | ` * value constant(string $name)` |
|        - | 12322 | ` *  Returns the value of a constant` |
|        - | 12323 | ` * Parameter` |
|        - | 12324 | ` *  $name` |
|        - | 12325 | ` *    Name of the constant.` |
|        - | 12326 | ` * Return` |
|        - | 12327 | ` *  Constant value or NULL if not defined.` |
|        - | 12328 | ` */` |
|        8 | 12329 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12330 |  |
|        - | 12331 | `	SyHashEntry *pEntry;` |
|        - | 12332 | `	ph7_constant *pCons;` |
|        - | 12333 | `	const char *zName; /* Constant name */` |
|        - | 12334 | `	ph7_value sVal;    /* Constant value */` |
|        - | 12335 | `	int nLen;` |
|       10 | 12336 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12337 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 12338 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 12339 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12340 | `		return SXRET_OK;` |
|        - | 12341 | `	}` |
|        - | 12342 | `	/* Extract the constant name */` |
|       10 | 12343 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12344 | `	/* Perform the query */` |
|       10 | 12345 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 12346 | `	if( pEntry == 0 ){` |
|        3 | 12347 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 12348 | `		ph7_result_null(pCtx);` |
|        3 | 12349 | `		return SXRET_OK;` |
|        - | 12350 | `	}` |
|        8 | 12351 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 12352 | `	/* Point to the structure that describe the constant */` |
|        8 | 12353 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 12354 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 12355 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 12356 | `	/* Return that value */` |
|        8 | 12357 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 12358 | `	/* Cleanup */` |
|        8 | 12359 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 12360 | `	return SXRET_OK;` |
|        6 | 12361 |  |
|        - | 12362 | `/*` |
|        - | 12363 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 12364 | ` * defined below.` |
|        - | 12365 | ` */` |
|      466 | 12366 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12367 |  |
|      467 | 12368 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12369 | `	ph7_value sName;` |
|        - | 12370 | `	sxi32 rc;` |
|        - | 12371 | `	/* Prepare the constant name for insertion */` |
|      467 | 12372 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 12373 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 12374 | `	/* Perform the insertion */` |
|      467 | 12375 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 12376 | `	PH7_MemObjRelease(&sName);` |
|      467 | 12377 | `	return rc;` |
|        1 | 12378 |  |
|        - | 12379 | `/*` |
|        - | 12380 | ` * array get_defined_constants(void)` |
|        - | 12381 | ` *  Returns an associative array with the names of all defined` |
|        - | 12382 | ` *  constants.` |
|        - | 12383 | ` * Parameters` |
|        - | 12384 | ` *  NONE.` |
|        - | 12385 | ` * Returns` |
|        - | 12386 | ` *  Returns the names of all the constants currently defined.` |
|        - | 12387 | ` */` |
|        2 | 12388 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12389 |  |
|        - | 12390 | `	ph7_value *pArray;` |
|        - | 12391 | `	/* Create the array first*/` |
|        3 | 12392 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12393 | `	if( pArray == 0 ){` |
|      ! 0 | 12394 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12395 | `		SXUNUSED(apArg);` |
|        - | 12396 | `		/* Return NULL */` |
|      ! 0 | 12397 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12398 | `		return SXRET_OK;` |
|        - | 12399 | `	}` |
|        - | 12400 | `	/* Fill the array with the defined constants */` |
|        3 | 12401 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 12402 | `	/* Return the created array */` |
|        3 | 12403 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12404 | `	return SXRET_OK;` |
|        2 | 12405 |  |
|        - | 12406 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 12407 | `/*` |
|        - | 12408 | ` * Section:` |
|        - | 12409 | ` *  Random numbers/string generators.` |
|        - | 12410 | ` * Status:` |
|        - | 12411 | ` *    Stable.` |
|        - | 12412 | ` */` |
|        - | 12413 | `/*` |
|        - | 12414 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 12415 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12416 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12417 | ` */` |
|     2913 | 12418 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 12419 |  |
|        - | 12420 | `	sxu32 iNum;` |
|     2915 | 12421 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2915 | 12422 | `	return iNum;` |
|        2 | 12423 |  |
|        - | 12424 | `/*` |
|        - | 12425 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 12426 | ` * Note that the generated string is NOT null terminated.` |
|        - | 12427 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12428 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12429 | ` */` |
|   237084 | 12430 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 12431 |  |
|        - | 12432 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 12433 | `	int i;` |
|        - | 12434 | `	/* Generate a binary string first */` |
|   237086 | 12435 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 12436 | `	/* Turn the binary string into english based alphabet */` |
|  2608094 | 12437 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2371010 | 12438 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1185506 | 12439 | `	 }` |
|   237086 | 12440 |  |
|        - | 12441 | `/*` |
|        - | 12442 | ` * int rand()` |
|        - | 12443 | ` * int mt_rand()` |
|        - | 12444 | ` * int rand(int $min,int $max)` |
|        - | 12445 | ` * int mt_rand(int $min,int $max)` |
|        - | 12446 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 12447 | ` * Parameter` |
|        - | 12448 | ` *  $min` |
|        - | 12449 | ` *    The lowest value to return (default: 0)` |
|        - | 12450 | ` *  $max` |
|        - | 12451 | ` *   The highest value to return (default: getrandmax())` |
|        - | 12452 | ` * Return` |
|        - | 12453 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 12454 | ` * Note:` |
|        - | 12455 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12456 | ` *  by te SQLite3 library.` |
|        - | 12457 | ` */` |
|       20 | 12458 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12459 |  |
|        - | 12460 | `	sxu32 iNum;` |
|        - | 12461 | `	/* Generate the random number */` |
|       21 | 12462 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 12463 | `	if( nArg > 1 ){` |
|        - | 12464 | `		sxu32 iMin,iMax;` |
|        3 | 12465 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 12466 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 12467 | `		if( iMin < iMax ){` |
|        3 | 12468 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 12469 | `			if( iDiv > 0 ){` |
|        3 | 12470 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 12471 | `			}` |
|        1 | 12472 | `		}else if(iMax > 0 ){` |
|      ! 0 | 12473 | `			iNum %= iMax;` |
|      ! 0 | 12474 | `		}` |
|        1 | 12475 | `	}` |
|        - | 12476 | `	/* Return the number */` |
|       21 | 12477 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 12478 | `	return SXRET_OK;` |
|        1 | 12479 |  |
|        - | 12480 | `/*` |
|        - | 12481 | ` * int getrandmax(void)` |
|        - | 12482 | ` * int mt_getrandmax(void)` |
|        - | 12483 | ` * int rc4_getrandmax(void)` |
|        - | 12484 | ` *   Show largest possible random value` |
|        - | 12485 | ` * Return` |
|        - | 12486 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 12487 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 12488 | ` * Note:` |
|        - | 12489 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12490 | ` *  by te SQLite3 library.` |
|        - | 12491 | ` */` |
|        4 | 12492 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12493 |  |
|        2 | 12494 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 12495 | `	SXUNUSED(apArg);` |
|        5 | 12496 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 12497 | `	return SXRET_OK;` |
|        1 | 12498 |  |
|        - | 12499 | `/*` |
|        - | 12500 | ` * string rand_str()` |
|        - | 12501 | ` * string rand_str(int $len)` |
|        - | 12502 | ` *  Generate a random string (English alphabet).` |
|        - | 12503 | ` * Parameter` |
|        - | 12504 | ` *  $len` |
|        - | 12505 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 12506 | ` * Return` |
|        - | 12507 | ` *   A pseudo random string.` |
|        - | 12508 | ` * Note:` |
|        - | 12509 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12510 | ` *  by te SQLite3 library.` |
|        - | 12511 | ` *  This function is a symisc extension.` |
|        - | 12512 | ` */` |
|      120 | 12513 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12514 |  |
|        - | 12515 | `	char zString[1024];` |
|      122 | 12516 | `	int iLen = 0x10;` |
|      122 | 12517 | `	if( nArg > 0 ){` |
|        - | 12518 | `		/* Get the desired length */` |
|      122 | 12519 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 12520 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 12521 | `			/* Default length */` |
|        3 | 12522 | `			iLen = 0x10;` |
|        1 | 12523 | `		}` |
|       60 | 12524 | `	}` |
|        - | 12525 | `	/* Generate the random string */` |
|      122 | 12526 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 12527 | `	/* Return the generated string */` |
|      122 | 12528 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 12529 | `	return SXRET_OK;` |
|        2 | 12530 |  |
|        - | 12531 | `/*` |
|        - | 12532 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 12533 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 12534 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 12535 | ` */` |
|      488 | 12536 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 12537 |  |
|      488 | 12538 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 12539 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 12540 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12541 | `			"TypeError",` |
|        - | 12542 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 12543 | `			zFunc,iArgPos,zParamName,` |
|        3 | 12544 | `			ph7_type_name(pArg)` |
|        - | 12545 | `			);` |
|        - | 12546 | `	}` |
|      483 | 12547 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 12548 | `		int len;` |
|        9 | 12549 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 12550 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 12551 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12552 | `				"TypeError",` |
|        - | 12553 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 12554 | `				zFunc,iArgPos,zParamName` |
|        - | 12555 | `				);` |
|        - | 12556 | `		}` |
|        2 | 12557 | `	}` |
|      479 | 12558 | `	return SXRET_OK;` |
|      245 | 12559 |  |
|        - | 12560 | `/*` |
|        - | 12561 | ` * int random_int(int $min, int $max)` |
|        - | 12562 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 12563 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 12564 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 12565 | ` *  power-of-two mask covering the range.` |
|        - | 12566 | ` */` |
|      242 | 12567 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12568 |  |
|        - | 12569 | `	sxi64 iMin,iMax;` |
|        - | 12570 | `	sxu64 uRange,uMask,uResult;` |
|        - | 12571 | `	unsigned int nAttempt;` |
|        - | 12572 | `	int rc;` |
|      243 | 12573 | `	if( nArg != 2 ){` |
|       10 | 12574 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12575 | `			"ArgumentCountError",` |
|        - | 12576 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 12577 | `			nArg` |
|        - | 12578 | `			);` |
|        - | 12579 | `	}` |
|      237 | 12580 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 12581 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 12582 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 12583 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 12584 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 12585 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 12586 | `	if( iMin > iMax ){` |
|        3 | 12587 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12588 | `			"ValueError",` |
|        - | 12589 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12590 | `			);` |
|        - | 12591 | `	}` |
|      229 | 12592 | `	if( iMin == iMax ){` |
|        5 | 12593 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 12594 | `		return SXRET_OK;` |
|        - | 12595 | `	}` |
|      225 | 12596 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 12597 | `	uMask = uRange;` |
|      225 | 12598 | `	uMask \|= uMask >> 1;` |
|      225 | 12599 | `	uMask \|= uMask >> 2;` |
|      225 | 12600 | `	uMask \|= uMask >> 4;` |
|      225 | 12601 | `	uMask \|= uMask >> 8;` |
|      225 | 12602 | `	uMask \|= uMask >> 16;` |
|      225 | 12603 | `	uMask \|= uMask >> 32;` |
|      225 | 12604 | `	uResult = 0;` |
|      343 | 12605 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 12606 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 12607 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 12608 | `		 * and the low-half mask would always read 0). */` |
|        - | 12609 | `		sxu64 uDraw;` |
|      343 | 12610 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 12611 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 12612 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 12613 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12614 | `				"Exception",` |
|        - | 12615 | `				"Cannot gather sufficient random data"` |
|        - | 12616 | `				);` |
|        - | 12617 | `		}` |
|      343 | 12618 | `		uDraw &= uMask;` |
|      343 | 12619 | `		if( uDraw <= uRange ){` |
|      225 | 12620 | `			uResult = uDraw;` |
|      225 | 12621 | `			break;` |
|        - | 12622 | `		}` |
|       59 | 12623 | `	}` |
|      225 | 12624 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12625 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12626 | `			"Exception",` |
|        - | 12627 | `			"Cannot gather sufficient random data"` |
|        - | 12628 | `			);` |
|        - | 12629 | `	}` |
|      225 | 12630 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12631 | `	return SXRET_OK;` |
|      122 | 12632 |  |
|        - | 12633 | `/*` |
|        - | 12634 | ` * string random_bytes(int $length)` |
|        - | 12635 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12636 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12637 | ` */` |
|       24 | 12638 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12639 |  |
|        - | 12640 | `	sxi64 iLen;` |
|        - | 12641 | `	unsigned char zStack[256];` |
|        - | 12642 | `	void *pBuf;` |
|        - | 12643 | `	int rc;` |
|       25 | 12644 | `	int bHeap = 0;` |
|       25 | 12645 | `	if( nArg != 1 ){` |
|        7 | 12646 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12647 | `			"ArgumentCountError",` |
|        - | 12648 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12649 | `			nArg` |
|        - | 12650 | `			);` |
|        - | 12651 | `	}` |
|       21 | 12652 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12653 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12654 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12655 | `	if( iLen < 1 ){` |
|        5 | 12656 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12657 | `			"ValueError",` |
|        - | 12658 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12659 | `			);` |
|        - | 12660 | `	}` |
|        - | 12661 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12662 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12663 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12664 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12665 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12666 | `			"ValueError",` |
|        - | 12667 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12668 | `			);` |
|        - | 12669 | `	}` |
|       13 | 12670 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12671 | `		pBuf = zStack;` |
|        7 | 12672 | `	}else{` |
|      ! 0 | 12673 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12674 | `		if( pBuf == 0 ){` |
|      ! 0 | 12675 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12676 | `				"Exception",` |
|        - | 12677 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 12678 | `				iLen` |
|        - | 12679 | `				);` |
|        - | 12680 | `		}` |
|      ! 0 | 12681 | `		bHeap = 1;` |
|        - | 12682 | `	}` |
|       13 | 12683 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 12684 | `		if( bHeap ){` |
|      ! 0 | 12685 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12686 | `		}` |
|      ! 0 | 12687 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12688 | `			"Exception",` |
|        - | 12689 | `			"Cannot gather sufficient random data"` |
|        - | 12690 | `			);` |
|        - | 12691 | `	}` |
|       13 | 12692 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 12693 | `	if( bHeap ){` |
|      ! 0 | 12694 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12695 | `	}` |
|       13 | 12696 | `	return SXRET_OK;` |
|       13 | 12697 |  |
|        - | 12698 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12699 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12700 | `/* Unique ID private data */` |
|        - | 12701 | `struct unique_id_data` |
|        - | 12702 |  |
|        - | 12703 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12704 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 12705 | `};` |
|        - | 12706 | `/*` |
|        - | 12707 | ` * Binary to hex consumer callback.` |
|        - | 12708 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 12709 | ` * defined below.` |
|        - | 12710 | ` */` |
|      192 | 12711 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 12712 |  |
|      193 | 12713 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 12714 | `	sxu32 nBuflen;` |
|        - | 12715 | `	/* Extract result buffer length */` |
|      193 | 12716 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 12717 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 12718 | `			/*` |
|        - | 12719 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 12720 | `			 * string will be 13 characters long` |
|        - | 12721 | `			 */` |
|       25 | 12722 | `		return SXERR_ABORT;` |
|        - | 12723 | `	}` |
|      169 | 12724 | `	if( nBuflen > 22 ){` |
|      ! 0 | 12725 | `		return SXERR_ABORT;` |
|        - | 12726 | `	}` |
|        - | 12727 | `	/* Safely Consume the hex stream */` |
|      169 | 12728 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 12729 | `	return SXRET_OK;` |
|       97 | 12730 |  |
|        - | 12731 | `/*` |
|        - | 12732 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 12733 | ` *  Generate a unique ID` |
|        - | 12734 | ` * Parameter` |
|        - | 12735 | ` * $prefix` |
|        - | 12736 | ` *  Append this prefix to the generated unique ID.` |
|        - | 12737 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 12738 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 12739 | ` * $more_entropy` |
|        - | 12740 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 12741 | ` *  that the result will be unique.` |
|        - | 12742 | ` * Return` |
|        - | 12743 | ` *  Returns the unique identifier, as a string.` |
|        - | 12744 | ` */` |
|       24 | 12745 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12746 |  |
|        - | 12747 | `	struct unique_id_data sUniq;` |
|        - | 12748 | `	unsigned char zDigest[20];` |
|       25 | 12749 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12750 | `	const char *zPrefix;` |
|        - | 12751 | `	SHA1Context sCtx;` |
|        - | 12752 | `	char zRandom[7];` |
|        - | 12753 | `	int nPrefix;` |
|        - | 12754 | `	int entropy;` |
|        - | 12755 | `	/* Generate a random string first */` |
|       25 | 12756 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 12757 | `	/* Initialize fields */` |
|       25 | 12758 | `	zPrefix = 0;` |
|       25 | 12759 | `	nPrefix = 0;` |
|       25 | 12760 | `	entropy = 0;` |
|       25 | 12761 | `	if( nArg > 0 ){` |
|        - | 12762 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 12763 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 12764 | `		if( nArg > 1 ){` |
|      ! 0 | 12765 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12766 | `		}` |
|      ! 0 | 12767 | `	}` |
|       25 | 12768 | `	SHA1Init(&sCtx);` |
|        - | 12769 | `	/* Generate the random ID */` |
|       25 | 12770 | `	if( nPrefix > 0 ){` |
|      ! 0 | 12771 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 12772 | `	}` |
|        - | 12773 | `	/* Append the random ID */` |
|       25 | 12774 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 12775 | `	/* Append the random string */` |
|       25 | 12776 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 12777 | `	/* Increment the number */` |
|       25 | 12778 | `	pVm->unique_id++;` |
|       25 | 12779 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 12780 | `	/* Hexify the digest */` |
|       25 | 12781 | `	sUniq.pCtx = pCtx;` |
|       25 | 12782 | `	sUniq.entropy = entropy;` |
|       25 | 12783 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 12784 | `	/* All done */` |
|       25 | 12785 | `	return PH7_OK;` |
|        1 | 12786 |  |
|        - | 12787 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12788 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12789 | `/*` |
|        - | 12790 | ` * Section:` |
|        - | 12791 | ` *  Language construct implementation as foreign functions.` |
|        - | 12792 | ` * Status:` |
|        - | 12793 | ` *    Stable.` |
|        - | 12794 | ` */` |
|        - | 12795 | `/*` |
|        - | 12796 | ` * void echo($string...)` |
|        - | 12797 | ` *  Output one or more messages.` |
|        - | 12798 | ` * Parameters` |
|        - | 12799 | ` *  $string` |
|        - | 12800 | ` *   Message to output.` |
|        - | 12801 | ` * Return` |
|        - | 12802 | ` *  NULL.` |
|        - | 12803 | ` */` |
|      ! 0 | 12804 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12805 |  |
|        - | 12806 | `	const char *zData;` |
|      ! 0 | 12807 | `	int nDataLen = 0;` |
|        - | 12808 | `	ph7_vm *pVm;` |
|        - | 12809 | `	int i,rc;` |
|        - | 12810 | `	/* Point to the target VM */` |
|      ! 0 | 12811 | `	pVm = pCtx->pVm;` |
|        - | 12812 | `	/* Output */` |
|      ! 0 | 12813 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 12814 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 12815 | `		if( nDataLen > 0 ){` |
|      ! 0 | 12816 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 12817 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 12818 | `			if( rc == SXERR_ABORT ){` |
|        - | 12819 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12820 | `				return PH7_ABORT;` |
|        - | 12821 | `			}` |
|      ! 0 | 12822 | `		}` |
|      ! 0 | 12823 | `	}` |
|      ! 0 | 12824 | `	return SXRET_OK;` |
|      ! 0 | 12825 |  |
|        - | 12826 | `/*` |
|        - | 12827 | ` * int print($string...)` |
|        - | 12828 | ` *  Output one or more messages.` |
|        - | 12829 | ` * Parameters` |
|        - | 12830 | ` *  $string` |
|        - | 12831 | ` *   Message to output.` |
|        - | 12832 | ` * Return` |
|        - | 12833 | ` *  1 always.` |
|        - | 12834 | ` */` |
|        2 | 12835 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12836 |  |
|        - | 12837 | `	const char *zData;` |
|        3 | 12838 | `	int nDataLen = 0;` |
|        - | 12839 | `	ph7_vm *pVm;` |
|        - | 12840 | `	int i,rc;` |
|        - | 12841 | `	/* Point to the target VM */` |
|        3 | 12842 | `	pVm = pCtx->pVm;` |
|        - | 12843 | `	/* Output */` |
|        5 | 12844 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 12845 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 12846 | `		if( nDataLen > 0 ){` |
|        3 | 12847 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 12848 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 12849 | `			if( rc == SXERR_ABORT ){` |
|        - | 12850 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12851 | `				return PH7_ABORT;` |
|        - | 12852 | `			}` |
|        1 | 12853 | `		}` |
|        2 | 12854 | `	}` |
|        - | 12855 | `	/* Return 1 */` |
|        3 | 12856 | `	ph7_result_int(pCtx,1);` |
|        3 | 12857 | `	return SXRET_OK;` |
|        2 | 12858 |  |
|        - | 12859 | `/*` |
|        - | 12860 | ` * void exit(string $msg)` |
|        - | 12861 | ` * void exit(int $status)` |
|        - | 12862 | ` * void die(string $ms)` |
|        - | 12863 | ` * void die(int $status)` |
|        - | 12864 | ` *   Output a message and terminate program execution.` |
|        - | 12865 | ` * Parameter` |
|        - | 12866 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 12867 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 12868 | ` *  and not printed` |
|        - | 12869 | ` * Return` |
|        - | 12870 | ` *  NULL` |
|        - | 12871 | ` */` |
|      ! 0 | 12872 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12873 |  |
|      ! 0 | 12874 | `	if( nArg > 0 ){` |
|      ! 0 | 12875 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 12876 | `			const char *zData;` |
|      ! 0 | 12877 | `			int iLen = 0;` |
|        - | 12878 | `			/* Print exit message */` |
|      ! 0 | 12879 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 12880 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 12881 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 12882 | `			sxi32 iExitStatus;` |
|        - | 12883 | `			/* Record exit status code */` |
|      ! 0 | 12884 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 12885 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 12886 | `		}` |
|      ! 0 | 12887 | `	}` |
|        - | 12888 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 12889 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 12890 | `	 */` |
|      ! 0 | 12891 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 12892 | `	return PH7_ABORT;` |
|      ! 0 | 12893 |  |
|        - | 12894 | `/*` |
|        - | 12895 | ` * bool isset($var,...)` |
|        - | 12896 | ` *  Finds out whether a variable is set.` |
|        - | 12897 | ` * Parameters` |
|        - | 12898 | ` *  One or more variable to check.` |
|        - | 12899 | ` * Return` |
|        - | 12900 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 12901 | ` */` |
|    92872 | 12902 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12903 |  |
|        - | 12904 | `	ph7_value *pObj;` |
|    92874 | 12905 | `	int res = 0;` |
|        - | 12906 | `	int i;` |
|    92874 | 12907 | `	if( nArg < 1 ){` |
|        - | 12908 | `		/* Missing arguments,return false */` |
|      ! 0 | 12909 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 12910 | `		return SXRET_OK;` |
|        - | 12911 | `	}` |
|        - | 12912 | `	/* Iterate over available arguments */` |
|   121390 | 12913 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    92884 | 12914 | `		pObj = apArg[i];` |
|    92884 | 12915 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 12916 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 12917 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 12918 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    63392 | 12919 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 12920 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 12921 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 12922 | `			}` |
|    31695 | 12923 | `		}` |
|    92884 | 12924 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    92884 | 12925 | `		if( !res ){` |
|        - | 12926 | `			/* Variable not set,return FALSE */` |
|    64368 | 12927 | `			ph7_result_bool(pCtx,0);` |
|    64368 | 12928 | `			return SXRET_OK;` |
|        - | 12929 | `		}` |
|    14260 | 12930 | `	}` |
|        - | 12931 | `	/* All given variable are set,return TRUE */` |
|    28508 | 12932 | `	ph7_result_bool(pCtx,1);` |
|    28508 | 12933 | `	return SXRET_OK;` |
|    46438 | 12934 |  |
|        - | 12935 | `/*` |
|        - | 12936 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 12937 | ` * frame,the reference table and discard it's contents.` |
|        - | 12938 | ` * This function never fail and always return SXRET_OK.` |
|        - | 12939 | ` */` |
|  3162946 | 12940 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 12941 |  |
|        - | 12942 | `	ph7_value *pObj;` |
|        - | 12943 | `	VmRefObj *pRef;` |
|  3162948 | 12944 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3162948 | 12945 | `	if( pObj ){` |
|        - | 12946 | `		/* Release the object */` |
|  3162948 | 12947 | `		PH7_MemObjRelease(pObj);` |
|  1581473 | 12948 | `	}` |
|        - | 12949 | `	/* Remove old reference links */` |
|  3162948 | 12950 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3162948 | 12951 | `	if( pRef ){` |
|  3162942 | 12952 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 12953 | `		/* Unlink from the reference table */` |
|  3162942 | 12954 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3162942 | 12955 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 12956 | `			VmSlot sFree;` |
|        - | 12957 | `			/* Restore to the free list */` |
|  3162934 | 12958 | `			sFree.nIdx = nObjIdx;` |
|  3162934 | 12959 | `			sFree.pUserData = 0;` |
|  3162934 | 12960 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1581466 | 12961 | `		}` |
|  1581470 | 12962 | `	}` |
|  3162948 | 12963 | `	return SXRET_OK;` |
|        2 | 12964 |  |
|        - | 12965 | `/*` |
|        - | 12966 | ` * void unset($var,...)` |
|        - | 12967 | ` *   Unset one or more given variable.` |
|        - | 12968 | ` * Parameters` |
|        - | 12969 | ` *  One or more variable to unset.` |
|        - | 12970 | ` * Return` |
|        - | 12971 | ` *  Nothing.` |
|        - | 12972 | ` */` |
|     7576 | 12973 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12974 |  |
|        - | 12975 | `	ph7_value *pObj;` |
|        - | 12976 | `	ph7_vm *pVm;` |
|        - | 12977 | `	int i;` |
|        - | 12978 | `	/* Point to the target VM */` |
|     7578 | 12979 | `	pVm = pCtx->pVm;` |
|        - | 12980 | `	/* Iterate and unset */` |
|    15154 | 12981 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7578 | 12982 | `		pObj = apArg[i];` |
|     7578 | 12983 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      840 | 12984 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 12985 | `				/* Throw an error */` |
|      ! 0 | 12986 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 12987 | `			}` |
|      421 | 12988 | `		}else{` |
|     6740 | 12989 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 12990 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6740 | 12991 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6734 | 12992 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3366 | 12993 | `			}` |
|        - | 12994 | `		}` |
|     3790 | 12995 | `	}` |
|     7578 | 12996 | `	return SXRET_OK;` |
|        2 | 12997 |  |
|        - | 12998 | `/*` |
|        - | 12999 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 13000 | ` */` |
|      116 | 13001 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 13002 |  |
|      117 | 13003 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      117 | 13004 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 13005 | `	ph7_value *pObj;` |
|        - | 13006 | `	sxu32 nIdx;` |
|        - | 13007 | `	/* Extract the memory object */` |
|      117 | 13008 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      117 | 13009 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      117 | 13010 | `	if( pObj ){` |
|      117 | 13011 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      115 | 13012 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 13013 | `				SyString sName;` |
|        - | 13014 | `				ph7_value sKey;` |
|        - | 13015 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - | 13016 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - | 13017 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|      115 | 13018 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      115 | 13019 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      115 | 13020 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      115 | 13021 | `				PH7_MemObjRelease(&sKey);` |
|       57 | 13022 | `			}` |
|       57 | 13023 | `		}` |
|       58 | 13024 | `	}` |
|      117 | 13025 | `	return SXRET_OK;` |
|        1 | 13026 |  |
|        - | 13027 | `/*` |
|        - | 13028 | ` * array get_defined_vars(void)` |
|        - | 13029 | ` *  Returns an array of all defined variables.` |
|        - | 13030 | ` * Parameter` |
|        - | 13031 | ` *  None` |
|        - | 13032 | ` * Return` |
|        - | 13033 | ` *  An array with all the variables defined in the current scope.` |
|        - | 13034 | ` */` |
|        2 | 13035 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13036 |  |
|        3 | 13037 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13038 | `	ph7_value *pArray;` |
|        - | 13039 | `	/* Create a new array */` |
|        3 | 13040 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13041 | ` 	if( pArray == 0 ){` |
|      ! 0 | 13042 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13043 | `		SXUNUSED(apArg);` |
|        - | 13044 | `		/* Return NULL */` |
|      ! 0 | 13045 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13046 | `		return SXRET_OK;` |
|        - | 13047 | `	}` |
|        - | 13048 | `	/* Superglobals first */` |
|        3 | 13049 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 13050 | `	/* Then variable defined in the current frame */` |
|        3 | 13051 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 13052 | `	/* Finally,return the created array */` |
|        3 | 13053 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13054 | `	return SXRET_OK;` |
|        2 | 13055 |  |
|        - | 13056 | `/*` |
|        - | 13057 | ` * bool gettype($var)` |
|        - | 13058 | ` *  Get the type of a variable` |
|        - | 13059 | ` * Parameters` |
|        - | 13060 | ` *   $var` |
|        - | 13061 | ` *    The variable being type checked.` |
|        - | 13062 | ` * Return` |
|        - | 13063 | ` *   String representation of the given variable type.` |
|        - | 13064 | ` */` |
|       32 | 13065 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13066 |  |
|       34 | 13067 | `	const char *zType = "Empty";` |
|       34 | 13068 | `	if( nArg > 0 ){` |
|       34 | 13069 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 13070 | `	}` |
|        - | 13071 | `	/* Return the variable type */` |
|       34 | 13072 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 13073 | `	return SXRET_OK;` |
|        2 | 13074 |  |
|        - | 13075 | `/*` |
|        - | 13076 | ` * string get_resource_type(resource $handle)` |
|        - | 13077 | ` *  This function gets the type of the given resource.` |
|        - | 13078 | ` * Parameters` |
|        - | 13079 | ` *  $handle` |
|        - | 13080 | ` *  The evaluated resource handle.` |
|        - | 13081 | ` * Return` |
|        - | 13082 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 13083 | ` *  representing its type. If the type is not identified by this function` |
|        - | 13084 | ` *  the return value will be the string Unknown.` |
|        - | 13085 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 13086 | ` *  is not a resource.` |
|        - | 13087 | ` */` |
|        2 | 13088 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13089 |  |
|        3 | 13090 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13091 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 13092 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13093 | `		return PH7_OK;` |
|        - | 13094 | `	}` |
|        3 | 13095 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 13096 | `	return SXRET_OK;` |
|        2 | 13097 |  |
|        - | 13098 | `/*` |
|        - | 13099 | ` * void var_dump(expression,....)` |
|        - | 13100 | ` *   var_dump � Dumps information about a variable` |
|        - | 13101 | ` * Parameters` |
|        - | 13102 | ` *   One or more expression to dump.` |
|        - | 13103 | ` * Returns` |
|        - | 13104 | ` *  Nothing.` |
|        - | 13105 | ` */` |
|      218 | 13106 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13107 |  |
|        - | 13108 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 13109 | `	int i;` |
|      220 | 13110 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 13111 | `	/* Dump one or more expressions */` |
|      444 | 13112 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 13113 | `		ph7_value *pObj = apArg[i];` |
|        - | 13114 | `		/* Reset the working buffer */` |
|      226 | 13115 | `		SyBlobReset(&sDump);` |
|        - | 13116 | `		/* Dump the given expression */` |
|      226 | 13117 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 13118 | `		/* Output */` |
|      226 | 13119 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 13120 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 13121 | `		}` |
|      114 | 13122 | `	}` |
|        - | 13123 | `	/* Release the working buffer */` |
|      220 | 13124 | `	SyBlobRelease(&sDump);` |
|      220 | 13125 | `	return SXRET_OK;` |
|        2 | 13126 |  |
|        - | 13127 | `/*` |
|        - | 13128 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 13129 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 13130 | ` * Parameters` |
|        - | 13131 | ` *   expression: Expression to dump` |
|        - | 13132 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 13133 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 13134 | ` *            print_r() will return the information rather than print it.` |
|        - | 13135 | ` * Return` |
|        - | 13136 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 13137 | ` *  Otherwise, the return value is TRUE.` |
|        - | 13138 | ` */` |
|       16 | 13139 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13140 |  |
|       17 | 13141 | `	int ret_string = 0;` |
|        - | 13142 | `	SyBlob sDump;` |
|       17 | 13143 | `	if( nArg < 1 ){` |
|        - | 13144 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13145 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13146 | `		return SXRET_OK;` |
|        - | 13147 | `	}` |
|       17 | 13148 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 13149 | `	if ( nArg > 1 ){` |
|        - | 13150 | `		/* Where to redirect output */` |
|       11 | 13151 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 13152 | `	}` |
|        - | 13153 | `	/* Generate dump */` |
|       17 | 13154 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 13155 | `	if( !ret_string ){` |
|        - | 13156 | `		/* Output dump */` |
|        7 | 13157 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13158 | `		/* Return true */` |
|        7 | 13159 | `		ph7_result_bool(pCtx,1);` |
|        4 | 13160 | `	}else{` |
|        - | 13161 | `		/* Generated dump as return value */` |
|       11 | 13162 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13163 | `	}` |
|        - | 13164 | `	/* Release the working buffer */` |
|       17 | 13165 | `	SyBlobRelease(&sDump);` |
|       17 | 13166 | `	return SXRET_OK;` |
|        9 | 13167 |  |
|        - | 13168 | `/*` |
|        - | 13169 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 13170 | ` * Same job as print_r. (see coment above)` |
|        - | 13171 | ` */` |
|        2 | 13172 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13173 |  |
|        3 | 13174 | `	int ret_string = 0;` |
|        - | 13175 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 13176 | `	if( nArg < 1 ){` |
|        - | 13177 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13178 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13179 | `		return SXRET_OK;` |
|        - | 13180 | `	}` |
|        3 | 13181 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 13182 | `	if ( nArg > 1 ){` |
|        - | 13183 | `		/* Where to redirect output */` |
|        3 | 13184 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 13185 | `	}` |
|        - | 13186 | `	/* Generate dump */` |
|        3 | 13187 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 13188 | `	if( !ret_string ){` |
|        - | 13189 | `		/* Output dump */` |
|      ! 0 | 13190 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13191 | `		/* Return NULL */` |
|      ! 0 | 13192 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13193 | `	}else{` |
|        - | 13194 | `		/* Generated dump as return value */` |
|        3 | 13195 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13196 | `	}` |
|        - | 13197 | `	/* Release the working buffer */` |
|        3 | 13198 | `	SyBlobRelease(&sDump);` |
|        3 | 13199 | `	return SXRET_OK;` |
|        2 | 13200 |  |
|        - | 13201 | `/*` |
|        - | 13202 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 13203 | ` *  Set/get the various assert flags.` |
|        - | 13204 | ` * Parameter` |
|        - | 13205 | ` * $what` |
|        - | 13206 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 13207 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 13208 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 13209 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 13210 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 13211 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 13212 | ` * $value` |
|        - | 13213 | ` *   An optional new value for the option.` |
|        - | 13214 | ` * Return` |
|        - | 13215 | ` *  Old setting on success or FALSE on failure.` |
|        - | 13216 | ` */` |
|       28 | 13217 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13218 |  |
|       30 | 13219 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13220 | `	int iOption;` |
|        - | 13221 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 13222 | `	if( nArg < 1 ){` |
|        3 | 13223 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13224 | `			"ArgumentCountError",` |
|        - | 13225 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 13226 | `			);` |
|        - | 13227 | `	}` |
|        - | 13228 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 13229 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 13230 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 13231 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13232 | `			"TypeError",` |
|        - | 13233 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 13234 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 13235 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 13236 | `			);` |
|        - | 13237 | `	}` |
|       28 | 13238 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 13239 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 13240 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 13241 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 13242 | `	switch( iOption ){` |
|        5 | 13243 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 13244 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 13245 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 13246 | `		if( nArg > 1 ){` |
|        5 | 13247 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13248 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 13249 | `			}else{` |
|        3 | 13250 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 13251 | `			}` |
|        2 | 13252 | `		}` |
|       12 | 13253 | `		break;` |
|        1 | 13254 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 13255 | `		/* Return old callback or null */` |
|        3 | 13256 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 13257 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 13258 | `		}else{` |
|        3 | 13259 | `			ph7_result_null(pCtx);` |
|        - | 13260 | `		}` |
|        3 | 13261 | `		if( nArg > 1 ){` |
|      ! 0 | 13262 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 13263 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 13264 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 13265 | `			}else{` |
|      ! 0 | 13266 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 13267 | `			}` |
|      ! 0 | 13268 | `		}` |
|        3 | 13269 | `		break;` |
|        5 | 13270 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 13271 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 13272 | `		if( nArg > 1 ){` |
|        5 | 13273 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13274 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 13275 | `			}else{` |
|        3 | 13276 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 13277 | `			}` |
|        2 | 13278 | `		}` |
|       11 | 13279 | `		break;` |
|      ! 0 | 13280 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 13281 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13282 | `		break;` |
|        1 | 13283 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 13284 | `		ph7_result_int(pCtx, 1);` |
|        3 | 13285 | `		break;` |
|      ! 0 | 13286 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 13287 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13288 | `		break;` |
|        1 | 13289 | `	default:` |
|        - | 13290 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 13291 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13292 | `			"ValueError",` |
|        - | 13293 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 13294 | `			);` |
|        - | 13295 | `	}` |
|       26 | 13296 | `	return PH7_OK;` |
|       16 | 13297 |  |
|        - | 13298 | `/*` |
|        - | 13299 | ` * bool assert(mixed $assertion)` |
|        - | 13300 | ` *  Checks if assertion is FALSE.` |
|        - | 13301 | ` * Parameter` |
|        - | 13302 | ` *  $assertion` |
|        - | 13303 | ` *    The assertion to test.` |
|        - | 13304 | ` * Return` |
|        - | 13305 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 13306 | ` */` |
|       24 | 13307 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13308 |  |
|       26 | 13309 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13310 | `	int iFlags,iResult;` |
|        - | 13311 | `	const char *zDesc;` |
|        - | 13312 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 13313 | `	if( nArg < 1 ){` |
|        3 | 13314 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13315 | `			"ArgumentCountError",` |
|        - | 13316 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 13317 | `			);` |
|        - | 13318 | `	}` |
|       24 | 13319 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 13320 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 13321 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 13322 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 13323 | `		return PH7_OK;` |
|        - | 13324 | `	}` |
|        - | 13325 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 13326 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 13327 | `	if( !iResult ){` |
|        - | 13328 | `		/* Assertion failed */` |
|        - | 13329 | `		/* Extract optional description */` |
|       13 | 13330 | `		zDesc = 0;` |
|       13 | 13331 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13332 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 13333 | `		}` |
|       13 | 13334 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 13335 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 13336 | `			ph7_value sFile,sLine;` |
|        - | 13337 | `			ph7_value *apCbArg[3];` |
|        - | 13338 | `			SyString *pFile;` |
|        - | 13339 | `			/* Extract the processed script */` |
|      ! 0 | 13340 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 13341 | `			if( pFile == 0 ){` |
|      ! 0 | 13342 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 13343 | `			}` |
|        - | 13344 | `			/* Invoke the callback */` |
|      ! 0 | 13345 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 13346 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 13347 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 13348 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 13349 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 13350 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 13351 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 13352 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 13353 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 13354 | `		}` |
|       13 | 13355 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 13356 | `			/* Abort VM execution immediately */` |
|      ! 0 | 13357 | `			return PH7_ABORT;` |
|        - | 13358 | `		}` |
|        - | 13359 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 13360 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 13361 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13362 | `				"AssertionError",` |
|        - | 13363 | `				"%s",` |
|        1 | 13364 | `				zDesc` |
|        - | 13365 | `				);` |
|      ! 0 | 13366 | `		}else{` |
|       11 | 13367 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13368 | `				"AssertionError",` |
|        - | 13369 | `				"assert(false)"` |
|        - | 13370 | `				);` |
|        - | 13371 | `		}` |
|        - | 13372 | `	}` |
|        - | 13373 | `	/* Assertion passed */` |
|       11 | 13374 | `	ph7_result_bool(pCtx,1);` |
|       11 | 13375 | `	return PH7_OK;` |
|       14 | 13376 |  |
|        - | 13377 | `/*` |
|        - | 13378 | ` * Section:` |
|        - | 13379 | ` *  Error reporting functions.` |
|        - | 13380 | ` * Status:` |
|        - | 13381 | ` *    Stable.` |
|        - | 13382 | ` */` |
|        - | 13383 | `/*` |
|        - | 13384 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 13385 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 13386 | ` * Parameters` |
|        - | 13387 | ` *  $error_msg` |
|        - | 13388 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 13389 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 13390 | ` * $error_type` |
|        - | 13391 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 13392 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 13393 | ` * Return` |
|        - | 13394 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 13395 | ` */` |
|       12 | 13396 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13397 |  |
|       14 | 13398 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 13399 | `	int rc = PH7_OK;` |
|       14 | 13400 | `	if( nArg > 0 ){` |
|        - | 13401 | `		const char *zErr;` |
|        - | 13402 | `		int nLen;` |
|        - | 13403 | `		/* Extract the error message */` |
|       12 | 13404 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 13405 | `		if( nArg > 1 ){` |
|        - | 13406 | `			/* Extract the error type */` |
|       12 | 13407 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 13408 | `			switch( nErr ){` |
|        1 | 13409 | `			case 1:   /* E_ERROR */` |
|        - | 13410 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 13411 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 13412 | `			case 256: /* E_USER_ERROR */` |
|        3 | 13413 | `				nErr = PH7_CTX_ERR;` |
|        3 | 13414 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 13415 | `				break;` |
|        1 | 13416 | `			case 2:   /* E_WARNING */` |
|        - | 13417 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 13418 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 13419 | `			case 512: /* E_USER_WARNING */` |
|        3 | 13420 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 13421 | `				break;` |
|        3 | 13422 | `			default:` |
|        8 | 13423 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 13424 | `				break;` |
|        - | 13425 | `			}` |
|        5 | 13426 | `		}` |
|        - | 13427 | `		/* Report error */` |
|       12 | 13428 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 13429 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 13430 | `			return rc;` |
|        - | 13431 | `		}` |
|        - | 13432 | `		/* Return true */` |
|       12 | 13433 | `		ph7_result_bool(pCtx,1);` |
|        7 | 13434 | `	}else{` |
|        - | 13435 | `		/* Missing arguments,return FALSE */` |
|        3 | 13436 | `		ph7_result_bool(pCtx,0);` |
|        - | 13437 | `	}` |
|       14 | 13438 | `	return rc;` |
|        8 | 13439 |  |
|        - | 13440 | `/*` |
|        - | 13441 | ` * int error_reporting([int $level])` |
|        - | 13442 | ` *  Sets which PHP errors are reported.` |
|        - | 13443 | ` * Parameters` |
|        - | 13444 | ` *  $level` |
|        - | 13445 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 13446 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 13447 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 13448 | ` *   levels will not always behave as expected.` |
|        - | 13449 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 13450 | ` *   in the predefined constants.` |
|        - | 13451 | ` * Return` |
|        - | 13452 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 13453 | ` *   parameter is given.` |
|        - | 13454 | ` */` |
|       32 | 13455 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13456 |  |
|       34 | 13457 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13458 | `	int nOld;` |
|        - | 13459 | `	/* Extract the old reporting level */` |
|       34 | 13460 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       34 | 13461 | `	if( nArg > 0 ){` |
|        - | 13462 | `		int nNew;` |
|        - | 13463 | `		/* Extract the desired error reporting level */` |
|       28 | 13464 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       28 | 13465 | `		if( !nNew ){` |
|        - | 13466 | `			/* Do not report errors at all */` |
|        5 | 13467 | `			pVm->bErrReport = 0;` |
|        3 | 13468 | `		}else{` |
|        - | 13469 | `			/* Report all errors */` |
|       24 | 13470 | `			pVm->bErrReport = 1;` |
|        - | 13471 | `		}` |
|       13 | 13472 | `	}` |
|        - | 13473 | `	/* Return the old level */` |
|       34 | 13474 | `	ph7_result_int(pCtx,nOld);` |
|       34 | 13475 | `	return PH7_OK;` |
|        2 | 13476 |  |
|        - | 13477 | `/*` |
|        - | 13478 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 13479 | ` *  Send an error message somewhere.` |
|        - | 13480 | ` * Parameter` |
|        - | 13481 | ` *  $message` |
|        - | 13482 | ` *   The error message that should be logged.` |
|        - | 13483 | ` *  $message_type` |
|        - | 13484 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 13485 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 13486 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 13487 | ` *       This is the default option.` |
|        - | 13488 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 13489 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 13490 | ` *    2  No longer an option.` |
|        - | 13491 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 13492 | ` *       to the end of the message string.` |
|        - | 13493 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 13494 | ` *  $destination` |
|        - | 13495 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 13496 | ` *  $extra_headers` |
|        - | 13497 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 13498 | ` * Return` |
|        - | 13499 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13500 | ` * NOTE:` |
|        - | 13501 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 13502 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 13503 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 13504 | ` *  Otherwise this function is no-op.` |
|        - | 13505 | ` */` |
|        4 | 13506 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13507 |  |
|        - | 13508 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 13509 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 13510 | `	int iType = 0;` |
|        5 | 13511 | `	if( nArg < 1 ){` |
|        - | 13512 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 13513 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13514 | `		return PH7_OK;` |
|        - | 13515 | `	}` |
|        5 | 13516 | `	if( pVm->xErrLog  ){` |
|        - | 13517 | `		/* Invoke the user callback */` |
|      ! 0 | 13518 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 13519 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 13520 | `		if( nArg > 1 ){` |
|      ! 0 | 13521 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 13522 | `			if( nArg > 2 ){` |
|      ! 0 | 13523 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 13524 | `				if( nArg > 3 ){` |
|      ! 0 | 13525 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 13526 | `				}` |
|      ! 0 | 13527 | `			}` |
|      ! 0 | 13528 | `		}` |
|      ! 0 | 13529 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 13530 | `	}` |
|        - | 13531 | `	/* Retun TRUE */` |
|        5 | 13532 | `	ph7_result_bool(pCtx,1);` |
|        5 | 13533 | `	return PH7_OK;` |
|        3 | 13534 |  |
|        - | 13535 | `/*` |
|        - | 13536 | ` * bool restore_exception_handler(void)` |
|        - | 13537 | ` *  Restores the previously defined exception handler function.` |
|        - | 13538 | ` * Parameter` |
|        - | 13539 | ` *  None` |
|        - | 13540 | ` * Return` |
|        - | 13541 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 13542 | ` */` |
|        4 | 13543 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13544 |  |
|        5 | 13545 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13546 | `	ph7_value *pOld,*pNew;` |
|        - | 13547 | `	/* Point to the old and the new handler */` |
|        5 | 13548 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 13549 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 13550 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 13551 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 13552 | `		SXUNUSED(apArg);` |
|        - | 13553 | `		/* No installed handler,return FALSE */` |
|        5 | 13554 | `		ph7_result_bool(pCtx,0);` |
|        5 | 13555 | `		return PH7_OK;` |
|        - | 13556 | `	}` |
|        - | 13557 | `	/* Copy the old handler */` |
|      ! 0 | 13558 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13559 | `	PH7_MemObjRelease(pOld);` |
|        - | 13560 | `	/* Return TRUE */` |
|      ! 0 | 13561 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13562 | `	return PH7_OK;` |
|        3 | 13563 |  |
|        - | 13564 | `/*` |
|        - | 13565 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 13566 | ` *  Sets a user-defined exception handler function.` |
|        - | 13567 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 13568 | ` * NOTE` |
|        - | 13569 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 13570 | ` *  the satndard PHP engine.` |
|        - | 13571 | ` * Parameters` |
|        - | 13572 | ` *  $exception_handler` |
|        - | 13573 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 13574 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 13575 | ` *   that was thrown.` |
|        - | 13576 | ` *  Note:` |
|        - | 13577 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13578 | ` * Return` |
|        - | 13579 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 13580 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13581 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13582 | ` */` |
|        4 | 13583 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13584 |  |
|        6 | 13585 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13586 | `	ph7_value *pOld,*pNew;` |
|        - | 13587 | `	/* Point to the old and the new handler */` |
|        6 | 13588 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13589 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13590 | `	/* Return the old handler */` |
|        6 | 13591 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 13592 | `	if( nArg > 0 ){` |
|        6 | 13593 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13594 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 13595 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 13596 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13597 | `		}else{` |
|        6 | 13598 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13599 | `			/* Install the new handler */` |
|        6 | 13600 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13601 | `		}` |
|        2 | 13602 | `	}` |
|        6 | 13603 | `	return PH7_OK;` |
|        2 | 13604 |  |
|        - | 13605 | `/*` |
|        - | 13606 | ` * bool restore_error_handler(void)` |
|        - | 13607 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13608 | ` * Parameters:` |
|        - | 13609 | ` *  None.` |
|        - | 13610 | ` * Return` |
|        - | 13611 | ` *  Always TRUE.` |
|        - | 13612 | ` */` |
|        6 | 13613 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13614 |  |
|        7 | 13615 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13616 | `	ph7_value *pOld,*pNew;` |
|        - | 13617 | `	/* Point to the old and the new handler */` |
|        7 | 13618 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13619 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13620 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13621 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13622 | `		SXUNUSED(apArg);` |
|        - | 13623 | `		/* No installed callback,return FALSE */` |
|        7 | 13624 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13625 | `		return PH7_OK;` |
|        - | 13626 | `	}` |
|        - | 13627 | `	/* Copy the old callback */` |
|      ! 0 | 13628 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13629 | `	PH7_MemObjRelease(pOld);` |
|        - | 13630 | `	/* Return TRUE */` |
|      ! 0 | 13631 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13632 | `	return PH7_OK;` |
|        4 | 13633 |  |
|        - | 13634 | `/*` |
|        - | 13635 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13636 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13637 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13638 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13639 | ` *  Sets a user-defined error handler function.` |
|        - | 13640 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13641 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13642 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13643 | ` *  conditions (using trigger_error()).` |
|        - | 13644 | ` * Parameters` |
|        - | 13645 | ` *  $error_handler` |
|        - | 13646 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13647 | ` *   describing the error.` |
|        - | 13648 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13649 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13650 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13651 | ` *   The function can be shown as:` |
|        - | 13652 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13653 | ` *     errno` |
|        - | 13654 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13655 | ` *   errstr` |
|        - | 13656 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13657 | ` *   errfile` |
|        - | 13658 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13659 | ` *     was raised in, as a string.` |
|        - | 13660 | ` *  Note:` |
|        - | 13661 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13662 | ` * Return` |
|        - | 13663 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13664 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13665 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13666 | ` */` |
|    10872 | 13667 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13668 |  |
|    10874 | 13669 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13670 | `	ph7_value *pOld,*pNew;` |
|        - | 13671 | `	/* Point to the old and the new handler */` |
|    10874 | 13672 | `	pOld = &pVm->aErrCB[0];` |
|    10874 | 13673 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13674 | `	/* Return the old handler */` |
|    10874 | 13675 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10874 | 13676 | `	if( nArg > 0 ){` |
|    10874 | 13677 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13678 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5431 | 13679 | `			PH7_MemObjRelease(pNew);` |
|     5431 | 13680 | `			ph7_result_bool(pCtx,1);` |
|     2716 | 13681 | `		}else{` |
|     5444 | 13682 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13683 | `			/* Install the new handler */` |
|     5444 | 13684 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13685 | `		}` |
|     5436 | 13686 | `	}` |
|    10874 | 13687 | `	return PH7_OK;` |
|        2 | 13688 |  |
|        - | 13689 | `/*` |
|        - | 13690 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 13691 | ` *  Generates a backtrace.` |
|        - | 13692 | ` * Paramaeter` |
|        - | 13693 | ` *  $options` |
|        - | 13694 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 13695 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 13696 | ` *   all the function/method arguments, to save memory.` |
|        - | 13697 | ` * $limit` |
|        - | 13698 | ` *   (Not Used)` |
|        - | 13699 | ` * Return` |
|        - | 13700 | ` *  An array.The possible returned elements are as follows:` |
|        - | 13701 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 13702 | ` *          Name        Type      Description` |
|        - | 13703 | ` *          ------      ------     -----------` |
|        - | 13704 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 13705 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 13706 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 13707 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 13708 | ` *          object      object    The current object.` |
|        - | 13709 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 13710 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 13711 | ` */` |
|      928 | 13712 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13713 |  |
|      930 | 13714 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13715 | `	ph7_value *pArray;` |
|        - | 13716 | `	ph7_class *pClass;` |
|        - | 13717 | `	ph7_value *pValue;` |
|        - | 13718 | `	SyString *pFile;` |
|        - | 13719 | `	/* Create a new array */` |
|      930 | 13720 | `	pArray = ph7_context_new_array(pCtx);` |
|      930 | 13721 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      930 | 13722 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13723 | `		/* Out of memory,return NULL */` |
|      ! 0 | 13724 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13725 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13726 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13727 | `		SXUNUSED(apArg);` |
|      ! 0 | 13728 | `		return PH7_OK;` |
|        - | 13729 | `	}` |
|        - | 13730 | `	/* Dump running function name and it's arguments  */` |
|      930 | 13731 | `	if( pVm->pFrame->pParent ){` |
|      930 | 13732 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 13733 | `		ph7_vm_func *pFunc;` |
|        - | 13734 | `		ph7_value *pArg;` |
|      930 | 13735 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      930 | 13736 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      930 | 13737 | `		if( pFrame->pParent && pFunc ){` |
|      930 | 13738 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      930 | 13739 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      930 | 13740 | `			ph7_value_reset_string_cursor(pValue);` |
|      464 | 13741 | `		}` |
|        - | 13742 | `		/* Function arguments */` |
|      930 | 13743 | `		pArg = ph7_context_new_array(pCtx);` |
|      930 | 13744 | `		if( pArg  ){` |
|        - | 13745 | `			ph7_value *pObj;` |
|        - | 13746 | `			VmSlot *aSlot;` |
|        - | 13747 | `			sxu32 n;` |
|        - | 13748 | `			/* Start filling the array with the given arguments */` |
|      930 | 13749 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3718 | 13750 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2790 | 13751 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2790 | 13752 | `				if( pObj ){` |
|     2790 | 13753 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1394 | 13754 | `				}` |
|     1396 | 13755 | `			}` |
|        - | 13756 | `			/* Save the array */` |
|      930 | 13757 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      464 | 13758 | `		}` |
|      464 | 13759 | `	}` |
|      930 | 13760 | `	ph7_value_int(pValue,1);` |
|        - | 13761 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 13762 | `	 * line numbers at run-time. )` |
|        - | 13763 | `	 */` |
|      930 | 13764 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 13765 | `	/* Current processed script */` |
|      930 | 13766 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      930 | 13767 | `	if( pFile ){` |
|      930 | 13768 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      930 | 13769 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      930 | 13770 | `		ph7_value_reset_string_cursor(pValue);` |
|      464 | 13771 | `	}` |
|        - | 13772 | `	/* Top class */` |
|      930 | 13773 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      930 | 13774 | `	if( pClass ){` |
|      926 | 13775 | `		ph7_value_reset_string_cursor(pValue);` |
|      926 | 13776 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      926 | 13777 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      462 | 13778 | `	}` |
|        - | 13779 | `	/* Return the freshly created array */` |
|      930 | 13780 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13781 | `	/*` |
|        - | 13782 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 13783 | `	 * as soon we return from this function.` |
|        - | 13784 | `	 */` |
|      930 | 13785 | `	return PH7_OK;` |
|      466 | 13786 |  |
|        - | 13787 | `/*` |
|        - | 13788 | ` * Generate a small backtrace.` |
|        - | 13789 | ` * Store the generated dump in the given BLOB` |
|        - | 13790 | ` */` |
|        4 | 13791 | `static int VmMiniBacktrace(` |
|        - | 13792 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13793 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 13794 | `	)` |
|        1 | 13795 |  |
|        5 | 13796 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13797 | `	ph7_vm_func *pFunc;` |
|        - | 13798 | `	ph7_class *pClass;` |
|        - | 13799 | `	SyString *pFile;` |
|        - | 13800 | `	/* Called function */` |
|        5 | 13801 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 13802 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 13803 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13804 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 13805 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 13806 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 13807 | `	}else{` |
|      ! 0 | 13808 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 13809 | `	}` |
|        5 | 13810 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 13811 | `	/* Current processed script */` |
|        5 | 13812 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 13813 | `	if( pFile ){` |
|        5 | 13814 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13815 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 13816 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 13817 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 13818 | `	}` |
|        - | 13819 | `	/* Top class */` |
|        5 | 13820 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 13821 | `	if( pClass ){` |
|      ! 0 | 13822 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 13823 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 13824 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 13825 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 13826 | `	}` |
|        5 | 13827 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 13828 | `	/* All done */` |
|        5 | 13829 | `	return SXRET_OK;` |
|        1 | 13830 |  |
|        - | 13831 | `/*` |
|        - | 13832 | ` * void debug_print_backtrace()` |
|        - | 13833 | ` *  Prints a backtrace` |
|        - | 13834 | ` * Parameters` |
|        - | 13835 | ` * None` |
|        - | 13836 | ` * Return` |
|        - | 13837 | ` * NULL` |
|        - | 13838 | ` */` |
|        2 | 13839 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13840 |  |
|        3 | 13841 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13842 | `	SyBlob sDump;` |
|        3 | 13843 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13844 | `	/* Generate the backtrace */` |
|        3 | 13845 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13846 | `	/* Output backtrace */` |
|        3 | 13847 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13848 | `	/* All done,cleanup */` |
|        3 | 13849 | `	SyBlobRelease(&sDump);` |
|        1 | 13850 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13851 | `	SXUNUSED(apArg);` |
|        3 | 13852 | `	return PH7_OK;` |
|        1 | 13853 |  |
|        - | 13854 | `/*` |
|        - | 13855 | ` * string debug_string_backtrace()` |
|        - | 13856 | ` *  Generate a backtrace` |
|        - | 13857 | ` * Parameters` |
|        - | 13858 | ` * None` |
|        - | 13859 | ` * Return` |
|        - | 13860 | ` *  A mini backtrace().` |
|        - | 13861 | ` * Note that this is a symisc extension.` |
|        - | 13862 | ` */` |
|        2 | 13863 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13864 |  |
|        3 | 13865 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13866 | `	SyBlob sDump;` |
|        3 | 13867 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13868 | `	/* Generate the backtrace */` |
|        3 | 13869 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13870 | `	/* Return the backtrace */` |
|        3 | 13871 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 13872 | `	/* All done,cleanup */` |
|        3 | 13873 | `	SyBlobRelease(&sDump);` |
|        1 | 13874 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13875 | `	SXUNUSED(apArg);` |
|        3 | 13876 | `	return PH7_OK;` |
|        1 | 13877 |  |
|        - | 13878 | `/*` |
|        - | 13879 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 13880 | ` * exception is triggered.` |
|        - | 13881 | ` */` |
|      512 | 13882 | `static sxi32 VmUncaughtException(` |
|        - | 13883 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13884 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13885 | `	)` |
|        1 | 13886 |  |
|        - | 13887 | `	ph7_value *apArg[2],sArg;` |
|      513 | 13888 | `	int nArg = 1;` |
|        - | 13889 | `	sxi32 rc;` |
|      513 | 13890 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 13891 | `		/* Nesting limit reached */` |
|      ! 0 | 13892 | `		return SXRET_OK;` |
|        - | 13893 | `	}` |
|        - | 13894 | `	/* Call any exception handler if available */` |
|      513 | 13895 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 13896 | `	if( pThis ){` |
|        - | 13897 | `		/* Load the exception instance */` |
|      513 | 13898 | `		sArg.x.pOther = pThis;` |
|      513 | 13899 | `		pThis->iRef++;` |
|      513 | 13900 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 13901 | `	}else{` |
|      ! 0 | 13902 | `		nArg = 0;` |
|        - | 13903 | `	}` |
|      513 | 13904 | `	apArg[0] = &sArg;` |
|        - | 13905 | `	/* Call the exception handler if available */` |
|      513 | 13906 | `	pVm->nExceptDepth++;` |
|      513 | 13907 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 13908 | `	pVm->nExceptDepth--;` |
|      513 | 13909 | `	if( rc != SXRET_OK ){` |
|        - | 13910 | `		SyBlob sMsgBuf;` |
|      511 | 13911 | `		const char *zClass = "Exception";` |
|      511 | 13912 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 13913 | `		const char *zMsg;` |
|        - | 13914 | `		sxu32 nMsg;` |
|        - | 13915 | `		const char *zFuncName;` |
|        - | 13916 | `		int nFuncLen;` |
|      511 | 13917 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 13918 | `		if( pThis ){` |
|        - | 13919 | `			ph7_class_method *pGetMessage;` |
|        - | 13920 | `			ph7_value sMsg;` |
|        - | 13921 | `			const char *zTmp;` |
|        - | 13922 | `			int nTmp;` |
|      511 | 13923 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 13924 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 13925 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 13926 | `			if( pGetMessage ){` |
|      511 | 13927 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 13928 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 13929 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 13930 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 13931 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 13932 | `					}` |
|      255 | 13933 | `				}` |
|      511 | 13934 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 13935 | `			}` |
|      255 | 13936 | `		}` |
|      511 | 13937 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 13938 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 13939 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 13940 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 13941 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 13942 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 13943 | `		rc = SXERR_ABORT;` |
|      255 | 13944 | `	}` |
|      513 | 13945 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 13946 | `	return rc;` |
|      257 | 13947 |  |
|        - | 13948 | `/*` |
|        - | 13949 | ` * Throw a user exception.` |
|        - | 13950 | ` *` |
|        - | 13951 | ` * Exception dispatch follows this sequence:` |
|        - | 13952 | ` *` |
|        - | 13953 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 13954 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 13955 | ` *` |
|        - | 13956 | ` * 2. If NO catch matches:` |
|        - | 13957 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 13958 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 13959 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 13960 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 13961 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 13962 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 13963 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 13964 | ` *` |
|        - | 13965 | ` * 3. If a catch DOES match:` |
|        - | 13966 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 13967 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 13968 | ` *       inside the catch body from immediately propagating past our` |
|        - | 13969 | ` *       finally block.` |
|        - | 13970 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 13971 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 13972 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 13973 | ` *       in pPendingException (step 2c).` |
|        - | 13974 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 13975 | ` *    d. Run finally (if present).` |
|        - | 13976 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 13977 | ` *       that handlers are restored and finally has run.` |
|        - | 13978 | ` */` |
|      858 | 13979 | `static sxi32 VmThrowException(` |
|        - | 13980 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 13981 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13982 | `	)` |
|        2 | 13983 |  |
|        - | 13984 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 13985 | `	ph7_exception **apException;` |
|        - | 13986 | `	ph7_exception *pException;` |
|        - | 13987 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 13988 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 13989 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      860 | 13990 | `	VmCoalesceDisarm(pVm);` |
|        - | 13991 | `	/* Point to the stack of loaded exceptions */` |
|      860 | 13992 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      860 | 13993 | `	pException = 0;` |
|      860 | 13994 | `	pCatch = 0;` |
|      860 | 13995 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13996 | `		ph7_exception_block *aCatch;` |
|        - | 13997 | `		ph7_class *pClass;` |
|        - | 13998 | `		SyString *aNames;` |
|        - | 13999 | `		sxu32 nNames;` |
|        - | 14000 | `		int matched;` |
|        - | 14001 | `		sxu32 j,k;` |
|        - | 14002 | `		/* Locate the appropriate block to execute */` |
|      340 | 14003 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      340 | 14004 | `		(void)SySetPop(&pVm->aException);` |
|      340 | 14005 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      348 | 14006 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 14007 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      346 | 14008 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      346 | 14009 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      346 | 14010 | `			matched = 0;` |
|      372 | 14011 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 14012 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 14013 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 14014 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      364 | 14015 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      364 | 14016 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 14017 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 14018 | `					continue;` |
|        - | 14019 | `				}` |
|      364 | 14020 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      338 | 14021 | `					matched = 1;` |
|      338 | 14022 | `					break;` |
|        - | 14023 | `				}` |
|       14 | 14024 | `			}` |
|      346 | 14025 | `			if( matched ){` |
|        - | 14026 | `				/* Catch block found,break immediately */` |
|      338 | 14027 | `				pCatch = &aCatch[j];` |
|      338 | 14028 | `				break;` |
|        - | 14029 | `			}` |
|        5 | 14030 | `		}` |
|      169 | 14031 | `	}` |
|        - | 14032 | `	/* Execute the cached block if available */` |
|      860 | 14033 | `	if( pCatch == 0 ){` |
|        - | 14034 | `		sxi32 rc;` |
|        - | 14035 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 14036 | `		if( pException && pException->iHasFinally ){` |
|        3 | 14037 | `			pException->iFinallyDone = 1;` |
|        3 | 14038 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 14039 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 14040 | `				return SXERR_ABORT;` |
|        - | 14041 | `			}` |
|        1 | 14042 | `		}` |
|        - | 14043 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 14044 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14045 | `			/* Re-throw to the outer handler */` |
|        3 | 14046 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 14047 | `		}` |
|        - | 14048 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 14049 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 14050 | `		 * exception instead of reporting it uncaught.` |
|        - | 14051 | `		 */` |
|      522 | 14052 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 14053 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 14054 | `			 * by looking for a catch frame on the stack.` |
|        - | 14055 | `			 */` |
|      522 | 14056 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 14057 | `			int inCatch = 0;` |
|     1050 | 14058 | `			while( pF ){` |
|      538 | 14059 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 14060 | `					inCatch = 1;` |
|        9 | 14061 | `					break;` |
|        - | 14062 | `				}` |
|      529 | 14063 | `				pF = pF->pParent;` |
|        1 | 14064 | `			}` |
|      522 | 14065 | `			if( inCatch ){` |
|        - | 14066 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 14067 | `				pThis->iRef++;` |
|        9 | 14068 | `				pVm->pPendingException = pThis;` |
|        9 | 14069 | `				return SXRET_OK;` |
|        - | 14070 | `			}` |
|      256 | 14071 | `		}` |
|        - | 14072 | `		/* Truly uncaught */` |
|      513 | 14073 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 14074 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 14075 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 14076 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 14077 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 14078 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 14079 | `			}` |
|      ! 0 | 14080 | `		}` |
|      513 | 14081 | `		return rc;` |
|      ! 0 | 14082 | `	}else{` |
|      338 | 14083 | `		VmFrame *pFrame = pVm->pFrame;` |
|      338 | 14084 | `		ph7_exception **apSaved = 0;` |
|        - | 14085 | `		sxu32 nSavedCount;` |
|        - | 14086 | `		sxi32 rc;` |
|      338 | 14087 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      338 | 14088 | `		if( pException->pFrame == pFrame ){` |
|      238 | 14089 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      118 | 14090 | `		}` |
|        - | 14091 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 14092 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 14093 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 14094 | `		 */` |
|      338 | 14095 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      338 | 14096 | `		if( nSavedCount > 0 ){` |
|       16 | 14097 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 14098 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 14099 | `			if( apSaved ){` |
|       16 | 14100 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 14101 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 14102 | `				SySetReset(&pVm->aException);` |
|        5 | 14103 | `			}` |
|        5 | 14104 | `		}` |
|        - | 14105 | `		/* Create a private frame first */` |
|      338 | 14106 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      338 | 14107 | `		if( rc == SXRET_OK ){` |
|      338 | 14108 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      338 | 14109 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      338 | 14110 | `			if( pObj ){` |
|      338 | 14111 | `				pThis->iRef++;` |
|      338 | 14112 | `				pObj->x.pOther = pThis;` |
|      338 | 14113 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      168 | 14114 | `			}` |
|        - | 14115 | `			/* Execute the catch block */` |
|      338 | 14116 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 14117 | `			/* Leave the frame */` |
|      338 | 14118 | `			VmLeaveFrame(&(*pVm));` |
|      168 | 14119 | `		}` |
|        - | 14120 | `		/* Restore the outer exception handlers */` |
|      338 | 14121 | `		if( apSaved ){` |
|        - | 14122 | `			sxu32 k;` |
|        - | 14123 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 14124 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 14125 | `			 * Restore the original outer entries.` |
|        - | 14126 | `			 */` |
|       11 | 14127 | `			SySetReset(&pVm->aException);` |
|       21 | 14128 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 14129 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 14130 | `			}` |
|       11 | 14131 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 14132 | `		}` |
|        - | 14133 | `		/* Execute the finally block after catch */` |
|      338 | 14134 | `		if( pException->iHasFinally ){` |
|       16 | 14135 | `			pException->iFinallyDone = 1;` |
|        - | 14136 | `			{` |
|       16 | 14137 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 14138 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 14139 | `					return SXERR_ABORT;` |
|        - | 14140 | `				}` |
|        - | 14141 | `			}` |
|        7 | 14142 | `		}` |
|      338 | 14143 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 14144 | `			return SXERR_ABORT;` |
|        - | 14145 | `		}` |
|        - | 14146 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 14147 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 14148 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 14149 | `		 */` |
|      338 | 14150 | `		if( pVm->pPendingException ){` |
|        9 | 14151 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 14152 | `			pVm->pPendingException = 0;` |
|        9 | 14153 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 14154 | `		}` |
|        - | 14155 | `	}` |
|        - | 14156 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 14157 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 14158 | `	 */` |
|      330 | 14159 | `	return SXRET_OK;` |
|      431 | 14160 |  |
|        - | 14161 | `/*` |
|        - | 14162 | ` * Section:` |
|        - | 14163 | ` *  Version,Credits and Copyright related functions.` |
|        - | 14164 | ` * Status:` |
|        - | 14165 | ` *    Stable.` |
|        - | 14166 | ` */` |
|        - | 14167 | `/*` |
|        - | 14168 | ` * string ph7version(void)` |
|        - | 14169 | ` *  Returns the running version of the PH7 version.` |
|        - | 14170 | ` * Parameters` |
|        - | 14171 | ` *  None` |
|        - | 14172 | ` * Return` |
|        - | 14173 | ` * Current PH7 version.` |
|        - | 14174 | ` */` |
|        2 | 14175 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14176 |  |
|        1 | 14177 | `	SXUNUSED(nArg);` |
|        1 | 14178 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 14179 | `	/* Current engine version */` |
|        3 | 14180 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 14181 | `	return PH7_OK;` |
|        1 | 14182 |  |
|        - | 14183 | `/*` |
|        - | 14184 | ` * string phpversion([ string $extension ])` |
|        - | 14185 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 14186 | ` * Parameters` |
|        - | 14187 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 14188 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 14189 | ` * Return` |
|        - | 14190 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 14191 | ` */` |
|        4 | 14192 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14193 |  |
|        2 | 14194 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 14195 | `	if( nArg > 0 ){` |
|      ! 0 | 14196 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14197 | `		return PH7_OK;` |
|        - | 14198 | `	}` |
|        5 | 14199 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 14200 | `	return PH7_OK;` |
|        3 | 14201 |  |
|        - | 14202 | `/*` |
|        - | 14203 | ` * string php_sapi_name(void)` |
|        - | 14204 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 14205 | ` * Parameters` |
|        - | 14206 | ` *  None` |
|        - | 14207 | ` * Return` |
|        - | 14208 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 14209 | ` */` |
|        2 | 14210 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14211 |  |
|        3 | 14212 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 14213 | `	SXUNUSED(nArg);` |
|        1 | 14214 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 14215 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 14216 | `	return PH7_OK;` |
|        1 | 14217 |  |
|        - | 14218 | `/*` |
|        - | 14219 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 14220 | ` */` |
|        - | 14221 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 14222 | ` "<html><head>"\` |
|        - | 14223 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 14224 | ` "<style type=\"text/css\">"\` |
|        - | 14225 | ` "div {"\` |
|        - | 14226 | `     "border: 1px solid #cccccc;"\` |
|        - | 14227 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 14228 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 14229 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 14230 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 14231 | `     "-webkit-border-radius: 10px;"\` |
|        - | 14232 | `     "-o-border-radius: 10px;"\` |
|        - | 14233 | `     "border-radius: 10px;"\` |
|        - | 14234 | `     "padding-left: 2em;"\` |
|        - | 14235 | `     "background-color: white;"\` |
|        - | 14236 | `     "margin-left: auto;"\` |
|        - | 14237 | `     "font-family: verdana;"\` |
|        - | 14238 | `     "padding-right: 2em;"\` |
|        - | 14239 | `     "margin-right: auto;"\` |
|        - | 14240 | `     "}"\` |
|        - | 14241 | `     "body {"\` |
|        - | 14242 | `     "padding: 0.2em;"\` |
|        - | 14243 | `     "font-style: normal;"\` |
|        - | 14244 | `     "font-size: medium;"\` |
|        - | 14245 | `     "background-color: #f2f2f2;"\` |
|        - | 14246 | `     "}"\` |
|        - | 14247 | `     "hr {"\` |
|        - | 14248 | `     "border-style: solid none none;"\` |
|        - | 14249 | `     "border-width: 1px medium medium;"\` |
|        - | 14250 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 14251 | `     "height: 1px;"\` |
|        - | 14252 | `     "}"\` |
|        - | 14253 | `     "a {"\` |
|        - | 14254 | `     "color: #3366cc;"\` |
|        - | 14255 | `     "text-decoration: none;"\` |
|        - | 14256 | `     "}"\` |
|        - | 14257 | `     "a:hover {"\` |
|        - | 14258 | `     "color: #999999;"\` |
|        - | 14259 | `     "}"\` |
|        - | 14260 | `     "a:active {"\` |
|        - | 14261 | `     "color: #663399;"\` |
|        - | 14262 | `     "}"\` |
|        - | 14263 | `     "h1 {"\` |
|        - | 14264 | `     "margin: 0;"\` |
|        - | 14265 | `     "padding: 0;"\` |
|        - | 14266 | `     "font-family: Verdana;"\` |
|        - | 14267 | `     "font-weight: bold;"\` |
|        - | 14268 | `     "font-style: normal;"\` |
|        - | 14269 | `     "font-size: medium;"\` |
|        - | 14270 | `     "text-transform: capitalize;"\` |
|        - | 14271 | `     "color: #0a328c;"\` |
|        - | 14272 | `     "}"\` |
|        - | 14273 | `     "p {"\` |
|        - | 14274 | `     "margin: 0 auto;"\` |
|        - | 14275 | `     "font-size: medium;"\` |
|        - | 14276 | `     "font-style: normal;"\` |
|        - | 14277 | `     "font-family: verdana;"\` |
|        - | 14278 | `     "}"\` |
|        - | 14279 | `"</style></head><body>"\` |
|        - | 14280 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 14281 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 14282 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 14283 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 14284 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 14285 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 14286 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 14287 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 14288 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 14289 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 14290 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 14291 |  |
|        - | 14292 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14293 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 14294 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 14295 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 14296 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14297 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 14298 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14299 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 14300 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14301 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 14302 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14303 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 14304 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 14305 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 14306 |  |
|        - | 14307 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 14308 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 14309 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 14310 | `"&nbsp;*<br>"\` |
|        - | 14311 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 14312 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 14313 | `"&nbsp;* are met:<br>"\` |
|        - | 14314 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 14315 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 14316 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 14317 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 14318 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 14319 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 14320 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 14321 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 14322 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 14323 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 14324 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 14325 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 14326 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 14327 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 14328 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 14329 | `"&nbsp;*<br>"\` |
|        - | 14330 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 14331 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 14332 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 14333 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 14334 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 14335 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 14336 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 14337 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 14338 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 14339 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 14340 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 14341 | `"&nbsp;*/<br>"\` |
|        - | 14342 | `"</span></small></small></p>"\` |
|        - | 14343 | `"</div></body></html>"` |
|        - | 14344 | `/*` |
|        - | 14345 | ` * bool ph7credits(void)` |
|        - | 14346 | ` * bool ph7info(void)` |
|        - | 14347 | ` * bool ph7copyright(void)` |
|        - | 14348 | ` *  Prints out the credits for PH7 engine` |
|        - | 14349 | ` * Parameters` |
|        - | 14350 | ` *  None` |
|        - | 14351 | ` * Return` |
|        - | 14352 | ` *  Always TRUE` |
|        - | 14353 | ` */` |
|        2 | 14354 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14355 |  |
|        3 | 14356 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 14357 | `	/* Expand the HTML page above*/` |
|        3 | 14358 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 14359 | `	ph7_context_output_format(` |
|        1 | 14360 | `		pCtx,` |
|        - | 14361 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 14362 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 14363 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 14364 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 14365 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 14366 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 14367 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 14368 | `#ifdef __WINNT__` |
|        - | 14369 | `		"Windows NT"` |
|        - | 14370 | `#elif defined(__UNIXES__)` |
|        - | 14371 | `		"UNIX-Like"` |
|        - | 14372 | `#else` |
|        - | 14373 | `		"Other OS"` |
|        - | 14374 | `#endif` |
|        - | 14375 | `		);` |
|        3 | 14376 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 14377 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14378 | `	SXUNUSED(apArg);` |
|        - | 14379 | `	/* Return TRUE */` |
|        - | 14380 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 14381 | `	return PH7_OK;` |
|        1 | 14382 |  |
|        - | 14383 | `/*` |
|        - | 14384 | ` * Section:` |
|        - | 14385 | ` *    URL related routines.` |
|        - | 14386 | ` * Status:` |
|        - | 14387 | ` *    Stable.` |
|        - | 14388 | ` */` |
|        - | 14389 | `/*` |
|        - | 14390 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 14391 | ` *  Parse a URL and return its fields.` |
|        - | 14392 | ` * Parameters` |
|        - | 14393 | ` *  $url` |
|        - | 14394 | ` *   The URL to parse.` |
|        - | 14395 | ` * $component` |
|        - | 14396 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 14397 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 14398 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 14399 | ` *  in which case the return value will be an integer).` |
|        - | 14400 | ` * Return` |
|        - | 14401 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 14402 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 14403 | ` *  this array are:` |
|        - | 14404 | ` *   scheme - e.g. http` |
|        - | 14405 | ` *   host` |
|        - | 14406 | ` *   port` |
|        - | 14407 | ` *   user` |
|        - | 14408 | ` *   pass` |
|        - | 14409 | ` *   path` |
|        - | 14410 | ` *   query - after the question mark ?` |
|        - | 14411 | ` *   fragment - after the hashmark #` |
|        - | 14412 | ` * Note:` |
|        - | 14413 | ` *  FALSE is returned on failure.` |
|        - | 14414 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 14415 | ` *  with the standard PHP engine.` |
|        - | 14416 | ` */` |
|       28 | 14417 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14418 |  |
|        - | 14419 | `	const char *zStr; /* Input string */` |
|        - | 14420 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 14421 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 14422 | `	int nLen;` |
|        - | 14423 | `	sxi32 rc;` |
|       29 | 14424 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 14425 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 14426 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14427 | `		return PH7_OK;` |
|        - | 14428 | `	}` |
|        - | 14429 | `	/* Extract the given URI */` |
|       29 | 14430 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 14431 | `	if( nLen < 1 ){` |
|        - | 14432 | `		/* Nothing to process,return FALSE */` |
|        3 | 14433 | `		ph7_result_bool(pCtx,0);` |
|        3 | 14434 | `		return PH7_OK;` |
|        - | 14435 | `	}` |
|        - | 14436 | `	/* Get a parse */` |
|       27 | 14437 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 14438 | `	if( rc != SXRET_OK ){` |
|        - | 14439 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 14440 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14441 | `		return PH7_OK;` |
|        - | 14442 | `	}` |
|       27 | 14443 | `	if( nArg > 1 ){` |
|      ! 0 | 14444 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 14445 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 14446 | `		switch(nComponent){` |
|      ! 0 | 14447 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 14448 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 14449 | `			if( pComp->nByte < 1 ){` |
|        - | 14450 | `				/* No available value,return NULL */` |
|      ! 0 | 14451 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14452 | `			}else{` |
|      ! 0 | 14453 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14454 | `			}` |
|      ! 0 | 14455 | `			break;` |
|      ! 0 | 14456 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 14457 | `			pComp = &sURI.sHost;` |
|      ! 0 | 14458 | `			if( pComp->nByte < 1 ){` |
|        - | 14459 | `				/* No available value,return NULL */` |
|      ! 0 | 14460 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14461 | `			}else{` |
|      ! 0 | 14462 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14463 | `			}` |
|      ! 0 | 14464 | `			break;` |
|      ! 0 | 14465 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 14466 | `			pComp = &sURI.sPort;` |
|      ! 0 | 14467 | `			if( pComp->nByte < 1 ){` |
|        - | 14468 | `				/* No available value,return NULL */` |
|      ! 0 | 14469 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14470 | `			}else{` |
|      ! 0 | 14471 | `				int iPort = 0;` |
|        - | 14472 | `				/* Cast the value to integer */` |
|      ! 0 | 14473 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 14474 | `				ph7_result_int(pCtx,iPort);` |
|        - | 14475 | `			}` |
|      ! 0 | 14476 | `			break;` |
|      ! 0 | 14477 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 14478 | `			pComp = &sURI.sUser;` |
|      ! 0 | 14479 | `			if( pComp->nByte < 1 ){` |
|        - | 14480 | `				/* No available value,return NULL */` |
|      ! 0 | 14481 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14482 | `			}else{` |
|      ! 0 | 14483 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14484 | `			}` |
|      ! 0 | 14485 | `			break;` |
|      ! 0 | 14486 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 14487 | `			pComp = &sURI.sPass;` |
|      ! 0 | 14488 | `			if( pComp->nByte < 1 ){` |
|        - | 14489 | `				/* No available value,return NULL */` |
|      ! 0 | 14490 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14491 | `			}else{` |
|      ! 0 | 14492 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14493 | `			}` |
|      ! 0 | 14494 | `			break;` |
|      ! 0 | 14495 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 14496 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 14497 | `			if( pComp->nByte < 1 ){` |
|        - | 14498 | `				/* No available value,return NULL */` |
|      ! 0 | 14499 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14500 | `			}else{` |
|      ! 0 | 14501 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14502 | `			}` |
|      ! 0 | 14503 | `			break;` |
|      ! 0 | 14504 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 14505 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 14506 | `			if( pComp->nByte < 1 ){` |
|        - | 14507 | `				/* No available value,return NULL */` |
|      ! 0 | 14508 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14509 | `			}else{` |
|      ! 0 | 14510 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14511 | `			}` |
|      ! 0 | 14512 | `			break;` |
|      ! 0 | 14513 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 14514 | `			pComp = &sURI.sPath;` |
|      ! 0 | 14515 | `			if( pComp->nByte < 1 ){` |
|        - | 14516 | `				/* No available value,return NULL */` |
|      ! 0 | 14517 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14518 | `			}else{` |
|      ! 0 | 14519 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14520 | `			}` |
|      ! 0 | 14521 | `			break;` |
|      ! 0 | 14522 | `		default:` |
|        - | 14523 | `			/* No such entry,return NULL */` |
|      ! 0 | 14524 | `			ph7_result_null(pCtx);` |
|      ! 0 | 14525 | `			break;` |
|        - | 14526 | `		}` |
|      ! 0 | 14527 | `	}else{` |
|        - | 14528 | `		ph7_value *pArray,*pValue;` |
|        - | 14529 | `		/* Return an associative array */` |
|       27 | 14530 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 14531 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 14532 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14533 | `			/* Out of memory */` |
|      ! 0 | 14534 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14535 | `			/* Return false */` |
|      ! 0 | 14536 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 14537 | `			return PH7_OK;` |
|        - | 14538 | `		}` |
|        - | 14539 | `		/* Fill the array */` |
|       27 | 14540 | `		pComp = &sURI.sScheme;` |
|       27 | 14541 | `		if( pComp->nByte > 0 ){` |
|       19 | 14542 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 14543 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 14544 | `		}` |
|        - | 14545 | `		/* Reset the string cursor */` |
|       27 | 14546 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14547 | `		pComp = &sURI.sHost;` |
|       27 | 14548 | `		if( pComp->nByte > 0 ){` |
|       25 | 14549 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 14550 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 14551 | `		}` |
|        - | 14552 | `		/* Reset the string cursor */` |
|       27 | 14553 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14554 | `		pComp = &sURI.sPort;` |
|       27 | 14555 | `		if( pComp->nByte > 0 ){` |
|       11 | 14556 | `			int iPort = 0;/* cc warning */` |
|        - | 14557 | `			/* Convert to integer */` |
|       11 | 14558 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 14559 | `			ph7_value_int(pValue,iPort);` |
|       11 | 14560 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 14561 | `		}` |
|        - | 14562 | `		/* Reset the string cursor */` |
|       27 | 14563 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14564 | `		pComp = &sURI.sUser;` |
|       27 | 14565 | `		if( pComp->nByte > 0 ){` |
|        7 | 14566 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14567 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 14568 | `		}` |
|        - | 14569 | `		/* Reset the string cursor */` |
|       27 | 14570 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14571 | `		pComp = &sURI.sPass;` |
|       27 | 14572 | `		if( pComp->nByte > 0 ){` |
|        7 | 14573 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14574 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 14575 | `		}` |
|        - | 14576 | `		/* Reset the string cursor */` |
|       27 | 14577 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14578 | `		pComp = &sURI.sPath;` |
|       27 | 14579 | `		if( pComp->nByte > 0 ){` |
|       17 | 14580 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 14581 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 14582 | `		}` |
|        - | 14583 | `		/* Reset the string cursor */` |
|       27 | 14584 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14585 | `		pComp = &sURI.sQuery;` |
|       27 | 14586 | `		if( pComp->nByte > 0 ){` |
|        5 | 14587 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14588 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 14589 | `		}` |
|        - | 14590 | `		/* Reset the string cursor */` |
|       27 | 14591 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14592 | `		pComp = &sURI.sFragment;` |
|       27 | 14593 | `		if( pComp->nByte > 0 ){` |
|        5 | 14594 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14595 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 14596 | `		}` |
|        - | 14597 | `		/* Return the created array */` |
|       27 | 14598 | `		ph7_result_value(pCtx,pArray);` |
|        - | 14599 | `		/* NOTE:` |
|        - | 14600 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 14601 | `		 * automatically as soon we return from this function.` |
|        - | 14602 | `		 */` |
|        - | 14603 | `	}` |
|        - | 14604 | `	/* All done */` |
|       27 | 14605 | `	return PH7_OK;` |
|       15 | 14606 |  |
|        - | 14607 | `/*` |
|        - | 14608 | ` * Section:` |
|        - | 14609 | ` *   Array related routines.` |
|        - | 14610 | ` * Status:` |
|        - | 14611 | ` *    Stable.` |
|        - | 14612 | ` * Note 2012-5-21 01:04:15:` |
|        - | 14613 | ` *  Array related functions that need access to the underlying` |
|        - | 14614 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 14615 | ` */` |
|        - | 14616 | `/*` |
|        - | 14617 | ` * The [compact()] function store it's state information in an instance` |
|        - | 14618 | ` * of the following structure.` |
|        - | 14619 | ` */` |
|        - | 14620 | `struct compact_data` |
|        - | 14621 |  |
|        - | 14622 | `	ph7_value *pArray;  /* Target array */` |
|        - | 14623 | `	int nRecCount;      /* Recursion count */` |
|        - | 14624 | `};` |
|        - | 14625 | `/*` |
|        - | 14626 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 14627 | ` */` |
|      ! 0 | 14628 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 14629 |  |
|      ! 0 | 14630 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 14631 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 14632 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 14633 | `	/* Act according to the hashmap value */` |
|      ! 0 | 14634 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 14635 | `		SyString sVar;` |
|      ! 0 | 14636 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 14637 | `		if( sVar.nByte > 0 ){` |
|        - | 14638 | `			/* Query the current frame */` |
|      ! 0 | 14639 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 14640 | `			/* ^` |
|        - | 14641 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 14642 | `			 */` |
|      ! 0 | 14643 | `			if( pKey ){` |
|        - | 14644 | `				/* Perform the insertion */` |
|      ! 0 | 14645 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 14646 | `			}` |
|      ! 0 | 14647 | `		}` |
|      ! 0 | 14648 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 14649 | `		int rc;` |
|        - | 14650 | `		/* Recursively traverse this array */` |
|      ! 0 | 14651 | `		pData->nRecCount++;` |
|      ! 0 | 14652 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14653 | `		pData->nRecCount--;` |
|      ! 0 | 14654 | `		return rc;` |
|        - | 14655 | `	}` |
|      ! 0 | 14656 | `	return SXRET_OK;` |
|      ! 0 | 14657 |  |
|        - | 14658 | `/*` |
|        - | 14659 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14660 | ` *  Create array containing variables and their values.` |
|        - | 14661 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14662 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14663 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14664 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14665 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14666 | ` * Parameters` |
|        - | 14667 | ` *  $varname` |
|        - | 14668 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14669 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14670 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14671 | ` *   it recursively.` |
|        - | 14672 | ` * Return` |
|        - | 14673 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14674 | ` */` |
|        2 | 14675 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14676 |  |
|        - | 14677 | `	ph7_value *pArray,*pObj;` |
|        3 | 14678 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14679 | `	const char *zName;` |
|        - | 14680 | `	SyString sVar;` |
|        - | 14681 | `	int i,nLen;` |
|        3 | 14682 | `	if( nArg < 1 ){` |
|        - | 14683 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 14684 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14685 | `		return PH7_OK;` |
|        - | 14686 | `	}` |
|        - | 14687 | `	/* Create the array */` |
|        3 | 14688 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 14689 | `	if( pArray == 0 ){` |
|        - | 14690 | `		/* Out of memory */` |
|      ! 0 | 14691 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14692 | `		/* Return NULL */` |
|      ! 0 | 14693 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14694 | `		return PH7_OK;` |
|        - | 14695 | `	}` |
|        - | 14696 | `	/* Perform the requested operation */` |
|        7 | 14697 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 14698 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 14699 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 14700 | `				struct compact_data sData;` |
|      ! 0 | 14701 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 14702 | `				/* Recursively walk the array */` |
|      ! 0 | 14703 | `				sData.nRecCount = 0;` |
|      ! 0 | 14704 | `				sData.pArray = pArray;` |
|      ! 0 | 14705 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 14706 | `			}` |
|      ! 0 | 14707 | `		}else{` |
|        - | 14708 | `			/* Extract variable name */` |
|        5 | 14709 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 14710 | `			if( nLen > 0 ){` |
|        5 | 14711 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 14712 | `				/* Check if the variable is available in the current frame */` |
|        5 | 14713 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 14714 | `				if( pObj ){` |
|        5 | 14715 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 14716 | `				}` |
|        2 | 14717 | `			}` |
|        - | 14718 | `		}` |
|        3 | 14719 | `	}` |
|        - | 14720 | `	/* Return the array */` |
|        3 | 14721 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 14722 | `	return PH7_OK;` |
|        2 | 14723 |  |
|        - | 14724 | `/*` |
|        - | 14725 | ` * The [extract()] function store it's state information in an instance` |
|        - | 14726 | ` * of the following structure.` |
|        - | 14727 | ` */` |
|        - | 14728 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 14729 | `struct extract_aux_data` |
|        - | 14730 |  |
|        - | 14731 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 14732 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 14733 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 14734 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 14735 | `	int iFlags;           /* Control flags */` |
|        - | 14736 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 14737 | `};` |
|        - | 14738 | `/* Forward declaration */` |
|        - | 14739 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 14740 | `/*` |
|        - | 14741 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 14742 | ` *   Import variables into the current symbol table from an array.` |
|        - | 14743 | ` * Parameters` |
|        - | 14744 | ` * $var_array` |
|        - | 14745 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 14746 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 14747 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 14748 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 14749 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 14750 | ` * $extract_type` |
|        - | 14751 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 14752 | ` *  It can be one of the following values:` |
|        - | 14753 | ` *   EXTR_OVERWRITE` |
|        - | 14754 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 14755 | ` *   EXTR_SKIP` |
|        - | 14756 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 14757 | ` *   EXTR_PREFIX_SAME` |
|        - | 14758 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 14759 | ` *   EXTR_PREFIX_ALL` |
|        - | 14760 | ` *       Prefix all variable names with prefix.` |
|        - | 14761 | ` *   EXTR_PREFIX_INVALID` |
|        - | 14762 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 14763 | ` *   EXTR_IF_EXISTS` |
|        - | 14764 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 14765 | ` *       otherwise do nothing.` |
|        - | 14766 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 14767 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 14768 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 14769 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 14770 | ` *      the current symbol table.` |
|        - | 14771 | ` * $prefix` |
|        - | 14772 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 14773 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 14774 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 14775 | ` *  underscore character.` |
|        - | 14776 | ` * Return` |
|        - | 14777 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 14778 | ` */` |
|        4 | 14779 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14780 |  |
|        - | 14781 | `	extract_aux_data sAux;` |
|        - | 14782 | `	ph7_hashmap *pMap;` |
|        5 | 14783 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 14784 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 14785 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14786 | `		return PH7_OK;` |
|        - | 14787 | `	}` |
|        - | 14788 | `	/* Point to the target hashmap */` |
|        5 | 14789 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 14790 | `	if( pMap->nEntry < 1 ){` |
|        - | 14791 | `		/* Empty map,return  0 */` |
|      ! 0 | 14792 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14793 | `		return PH7_OK;` |
|        - | 14794 | `	}` |
|        - | 14795 | `	/* Prepare the aux data */` |
|        5 | 14796 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 14797 | `	if( nArg > 1 ){` |
|        3 | 14798 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 14799 | `		if( nArg > 2 ){` |
|      ! 0 | 14800 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 14801 | `		}` |
|        1 | 14802 | `	}` |
|        5 | 14803 | `	sAux.pVm = pCtx->pVm;` |
|        - | 14804 | `	/* Invoke the worker callback */` |
|        5 | 14805 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 14806 | `	/* Number of variables successfully imported */` |
|        5 | 14807 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 14808 | `	return PH7_OK;` |
|        3 | 14809 |  |
|        - | 14810 | `/*` |
|        - | 14811 | ` * Worker callback for the [extract()] function defined` |
|        - | 14812 | ` * below.` |
|        - | 14813 | ` */` |
|        8 | 14814 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14815 |  |
|        9 | 14816 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 14817 | `	int iFlags = pAux->iFlags;` |
|        9 | 14818 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14819 | `	ph7_value *pObj;` |
|        - | 14820 | `	SyString sVar;` |
|        9 | 14821 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 14822 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 14823 | `	}` |
|        - | 14824 | `	/* Perform a string cast */` |
|        9 | 14825 | `	PH7_MemObjToString(pKey);` |
|        9 | 14826 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14827 | `		/* Unavailable variable name */` |
|      ! 0 | 14828 | `		return SXRET_OK;` |
|        - | 14829 | `	}` |
|        9 | 14830 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 14831 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 14832 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14833 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14834 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14835 | `			);` |
|      ! 0 | 14836 | `	}else{` |
|       13 | 14837 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 14838 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14839 | `	}` |
|        9 | 14840 | `	sVar.zString = pAux->zWorker;` |
|        - | 14841 | `	/* Try to extract the variable */` |
|        9 | 14842 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 14843 | `	if( pObj ){` |
|        - | 14844 | `		/* Collision */` |
|        5 | 14845 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 14846 | `			return SXRET_OK;` |
|        - | 14847 | `		}` |
|        5 | 14848 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 14849 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 14850 | `				/* Already prefixed */` |
|      ! 0 | 14851 | `				return SXRET_OK;` |
|        - | 14852 | `			}` |
|      ! 0 | 14853 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14854 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14855 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14856 | `				);` |
|      ! 0 | 14857 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 14858 | `		}` |
|        3 | 14859 | `	}else{` |
|        - | 14860 | `		/* Create the variable */` |
|        5 | 14861 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 14862 | `	}` |
|        9 | 14863 | `	if( pObj ){` |
|        - | 14864 | `		/* Overwrite the old value */` |
|        9 | 14865 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 14866 | `		/* Increment counter */` |
|        9 | 14867 | `		pAux->iCount++;` |
|        4 | 14868 | `	}` |
|        9 | 14869 | `	return SXRET_OK;` |
|        5 | 14870 |  |
|        - | 14871 | `/*` |
|        - | 14872 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 14873 | ` * defined below.` |
|        - | 14874 | ` */` |
|        2 | 14875 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14876 |  |
|        3 | 14877 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 14878 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14879 | `	ph7_value *pObj;` |
|        - | 14880 | `	SyString sVar;` |
|        - | 14881 | `	/* Perform a string cast */` |
|        3 | 14882 | `	PH7_MemObjToString(pKey);` |
|        3 | 14883 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14884 | `		/* Unavailable variable name */` |
|      ! 0 | 14885 | `		return SXRET_OK;` |
|        - | 14886 | `	}` |
|        3 | 14887 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 14888 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 14889 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 14890 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 14891 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14892 | `			);` |
|        2 | 14893 | `	}else{` |
|      ! 0 | 14894 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 14895 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14896 | `	}` |
|        3 | 14897 | `	sVar.zString = pAux->zWorker;` |
|        - | 14898 | `	/* Extract the variable */` |
|        3 | 14899 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 14900 | `	if( pObj ){` |
|        3 | 14901 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 14902 | `	}` |
|        3 | 14903 | `	return SXRET_OK;` |
|        2 | 14904 |  |
|        - | 14905 | `/*` |
|        - | 14906 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 14907 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 14908 | ` * Parameters` |
|        - | 14909 | ` * $types` |
|        - | 14910 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 14911 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 14912 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 14913 | ` *  POST includes the POST uploaded file information.` |
|        - | 14914 | ` *  Note:` |
|        - | 14915 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 14916 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 14917 | ` * $prefix` |
|        - | 14918 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 14919 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 14920 | ` *  variable named $pref_userid.` |
|        - | 14921 | ` * Return` |
|        - | 14922 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14923 | ` */` |
|        2 | 14924 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14925 |  |
|        - | 14926 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 14927 | `	extract_aux_data sAux;` |
|        - | 14928 | `	int nLen,nPrefixLen;` |
|        - | 14929 | `	ph7_value *pSuper;` |
|        - | 14930 | `	ph7_vm *pVm;` |
|        - | 14931 | `	/* By default import only $_GET variables  */` |
|        3 | 14932 | `	zImport = "G";` |
|        3 | 14933 | `	nLen = (int)sizeof(char);` |
|        3 | 14934 | `	zPrefix = 0;` |
|        3 | 14935 | `	nPrefixLen = 0;` |
|        3 | 14936 | `	if( nArg > 0 ){` |
|        3 | 14937 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 14938 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 14939 | `		}` |
|        3 | 14940 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 14941 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 14942 | `		}` |
|        1 | 14943 | `	}` |
|        - | 14944 | `	/* Point to the underlying VM */` |
|        3 | 14945 | `	pVm = pCtx->pVm;` |
|        - | 14946 | `	/* Initialize the aux data */` |
|        3 | 14947 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 14948 | `	sAux.zPrefix = zPrefix;` |
|        3 | 14949 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 14950 | `	sAux.pVm = pVm;` |
|        - | 14951 | `	/* Extract */` |
|        3 | 14952 | `	zEnd = &zImport[nLen];` |
|        5 | 14953 | `	while( zImport < zEnd ){` |
|        3 | 14954 | `		int c = zImport[0];` |
|        3 | 14955 | `		pSuper = 0;` |
|        3 | 14956 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 14957 | `			/* Import $_GET variables */` |
|        3 | 14958 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 14959 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 14960 | `			/* Import $_POST variables */` |
|      ! 0 | 14961 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14962 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 14963 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 14964 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14965 | `		}` |
|        3 | 14966 | `		if( pSuper ){` |
|        - | 14967 | `			/* Iterate throw array entries */` |
|        3 | 14968 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 14969 | `		}` |
|        - | 14970 | `		/* Advance the cursor */` |
|        3 | 14971 | `		zImport++;` |
|        1 | 14972 | `	}` |
|        - | 14973 | `	/* All done,return TRUE*/` |
|        3 | 14974 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14975 | `	return PH7_OK;` |
|        1 | 14976 |  |
|        - | 14977 | `/*` |
|        - | 14978 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 14979 | ` * Refer to the eval() language construct implementation for more` |
|        - | 14980 | ` * information.` |
|        - | 14981 | ` */` |
|    12782 | 14982 | `static sxi32 VmEvalChunk(` |
|        - | 14983 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 14984 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 14985 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 14986 | `	int iFlags,         /* Compile flag */` |
|        - | 14987 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 14988 | `	)` |
|        2 | 14989 |  |
|        - | 14990 | `	SySet *pByteCode,aByteCode;` |
|        - | 14991 | `	SyBlob sSavedNs;` |
|    12784 | 14992 | `	ProcConsumer xErr = 0;` |
|    12784 | 14993 | `	void *pErrData = 0;` |
|        - | 14994 | `	/* Initialize bytecode container */` |
|    12784 | 14995 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12784 | 14996 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 14997 | `	/* Reset the code generator */` |
|    12784 | 14998 | `	if( bTrueReturn ){` |
|        - | 14999 | `		/* Included file,log compile-time errors */` |
|     9580 | 15000 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9580 | 15001 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4789 | 15002 | `	}` |
|    12784 | 15003 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 15004 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 15005 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 15006 | `	 * the caller's namespace is restored. */` |
|    12784 | 15007 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12784 | 15008 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12784 | 15009 | `	if( bTrueReturn ){` |
|        - | 15010 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9580 | 15011 | `		SyBlobReset(&pVm->sNamespace);` |
|     4789 | 15012 | `	}` |
|        - | 15013 | `	/* Swap bytecode container */` |
|    12784 | 15014 | `	pByteCode = pVm->pByteContainer;` |
|    12784 | 15015 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 15016 | `	/* Compile the chunk */` |
|    12784 | 15017 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19174 | 15018 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 15019 | `		/* Compilation error,return false */` |
|        3 | 15020 | `		if( pCtx ){` |
|        3 | 15021 | `			ph7_result_bool(pCtx,0);` |
|        1 | 15022 | `		}` |
|        2 | 15023 | `	}else{` |
|        - | 15024 | `		/* Mount any newly defined classes */` |
|        - | 15025 | `		SyHashEntry *pEntry;` |
|        - | 15026 | `		ph7_class *pClass;` |
|        - | 15027 | `		ph7_value sResult; /* Return value */` |
|        - | 15028 | `		sxi32 rc;` |
|    12782 | 15029 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   970094 | 15030 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   950924 | 15031 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 15032 | `			/* Only mount classes that haven't been mounted yet */` |
|   950924 | 15033 | `			if( !pClass->bMounted ){` |
|   246784 | 15034 | `				rc = VmMountUserClass(pVm,pClass);` |
|   246784 | 15035 | `				if( rc != SXRET_OK ){` |
|        - | 15036 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 15037 | `					if( pCtx ){` |
|      ! 0 | 15038 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 15039 | `					}` |
|      ! 0 | 15040 | `					goto Cleanup;` |
|        - | 15041 | `				}` |
|   123391 | 15042 | `			}` |
|        2 | 15043 | `		}` |
|    12782 | 15044 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 15045 | `			/* Out of memory */` |
|      ! 0 | 15046 | `			if( pCtx ){` |
|      ! 0 | 15047 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 15048 | `			}` |
|      ! 0 | 15049 | `			goto Cleanup;` |
|        - | 15050 | `		}` |
|    12782 | 15051 | `		if( bTrueReturn ){` |
|        - | 15052 | `			/* Assume a boolean true return value */` |
|     9580 | 15053 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4791 | 15054 | `		}else{` |
|        - | 15055 | `			/* Assume a null return value */` |
|     3204 | 15056 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 15057 | `		}` |
|        - | 15058 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here,` |
|        - | 15059 | `		 * a path the OP_CALL cap check can't see; bound it under the same limit` |
|        - | 15060 | `		 * so a recursive include/eval can't overflow the native stack. */` |
|    12782 | 15061 | `		if( VmRecursionExceeded(pVm) ){` |
|        3 | 15062 | `			PH7_MemObjRelease(&sResult);` |
|        3 | 15063 | `			VmRecursionFatal(pVm);` |
|        3 | 15064 | `			goto Cleanup;` |
|        - | 15065 | `		}` |
|    12780 | 15066 | `		pVm->nRecursionDepth++;` |
|    12780 | 15067 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12780 | 15068 | `		pVm->nRecursionDepth--;` |
|    12780 | 15069 | `		if( pCtx ){` |
|        - | 15070 | `			/* Set the execution result */` |
|     9632 | 15071 | `			ph7_result_value(pCtx,&sResult);` |
|     4815 | 15072 | `		}` |
|    12780 | 15073 | `		PH7_MemObjRelease(&sResult);` |
|        - | 15074 | `	}` |
|     6391 | 15075 | `Cleanup:` |
|        - | 15076 | `	/* Cleanup the mess left behind */` |
|    12784 | 15077 | `	pVm->pByteContainer = pByteCode;` |
|    12784 | 15078 | `	SySetRelease(&aByteCode);` |
|        - | 15079 | `	/* Restore caller's namespace state */` |
|    12784 | 15080 | `	SyBlobReset(&pVm->sNamespace);` |
|    12784 | 15081 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12784 | 15082 | `	SyBlobRelease(&sSavedNs);` |
|    12784 | 15083 | `	return SXRET_OK;` |
|        2 | 15084 |  |
|        - | 15085 | `/*` |
|        - | 15086 | ` * value eval(string $code)` |
|        - | 15087 | ` *   Evaluate a string as PHP code.` |
|        - | 15088 | ` * Parameter` |
|        - | 15089 | ` *  code: PHP code to evaluate.` |
|        - | 15090 | ` * Return` |
|        - | 15091 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 15092 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 15093 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 15094 | ` */` |
|       58 | 15095 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15096 |  |
|        - | 15097 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       60 | 15098 | `	if( nArg < 1 ){` |
|        - | 15099 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15100 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15101 | `		return SXRET_OK;` |
|        - | 15102 | `	}` |
|        - | 15103 | `	/* Chunk to evaluate */` |
|       60 | 15104 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       60 | 15105 | `	if( sChunk.nByte < 1 ){` |
|        - | 15106 | `		/* Empty string,return NULL */` |
|        3 | 15107 | `		ph7_result_null(pCtx);` |
|        3 | 15108 | `		return SXRET_OK;` |
|        - | 15109 | `	}` |
|        - | 15110 | `	/* Eval the chunk */` |
|       58 | 15111 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       58 | 15112 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15113 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       37 | 15114 | `		return PH7_ABORT;` |
|        - | 15115 | `	}` |
|       22 | 15116 | `	return SXRET_OK;` |
|       31 | 15117 |  |
|        - | 15118 | `/*` |
|        - | 15119 | ` * Check if a file path is already included.` |
|        - | 15120 | ` */` |
|    19154 | 15121 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 15122 |  |
|        - | 15123 | `	SyString *aEntries;` |
|        - | 15124 | `	sxu32 n;` |
|    19156 | 15125 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 15126 | `	/* Perform a linear search */` |
| 91528416 | 15127 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 91509272 | 15128 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 15129 | `			/* Already included */` |
|       11 | 15130 | `			return TRUE;` |
|        - | 15131 | `		}` |
| 45754632 | 15132 | `	}` |
|    19146 | 15133 | `	return FALSE;` |
|     9579 | 15134 |  |
|        - | 15135 | `/*` |
|        - | 15136 | ` * Push a file path in the appropriate VM container.` |
|        - | 15137 | ` */` |
|    22294 | 15138 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 15139 |  |
|        - | 15140 | `	SyString sPath;` |
|        - | 15141 | `	char *zDup;` |
|        - | 15142 | `#ifdef __WINNT__` |
|        - | 15143 | `	char *zCur;` |
|        - | 15144 | `#endif` |
|        - | 15145 | `	sxi32 rc;` |
|    22296 | 15146 | `	if( nLen < 0 ){` |
|     3142 | 15147 | `		nLen = SyStrlen(zPath);` |
|     1570 | 15148 | `	}` |
|        - | 15149 | `	/* Duplicate the file path first */` |
|    22296 | 15150 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22296 | 15151 | `	if( zDup == 0 ){` |
|      ! 0 | 15152 | `		return SXERR_MEM;` |
|        - | 15153 | `	}` |
|        - | 15154 | `#ifdef __WINNT__` |
|        - | 15155 | `	/* Normalize path on windows` |
|        - | 15156 | `	 * Example:` |
|        - | 15157 | `	 *    Path/To/File.php` |
|        - | 15158 | `	 * becomes` |
|        - | 15159 | `	 *   path\to\file.php` |
|        - | 15160 | `	 */` |
|        2 | 15161 | `	zCur = zDup;` |
|        2 | 15162 | `	while( zCur[0] != 0 ){` |
|        2 | 15163 | `		if( zCur[0] == '/' ){` |
|        2 | 15164 | `			zCur[0] = '\\';` |
|        2 | 15165 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 15166 | `			int c = SyToLower(zCur[0]);` |
|        1 | 15167 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 15168 | `		}` |
|        2 | 15169 | `		zCur++;` |
|        2 | 15170 | `	}` |
|        - | 15171 | `#endif` |
|        - | 15172 | `	/* Install the file path */` |
|    22296 | 15173 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22296 | 15174 | `	if( !bMain ){` |
|    19156 | 15175 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 15176 | `			/* Already included */` |
|       11 | 15177 | `			*pNew = 0;` |
|        6 | 15178 | `		}else{` |
|        - | 15179 | `			/* Insert in the corresponding container */` |
|    19146 | 15180 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19146 | 15181 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 15182 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 15183 | `				return rc;` |
|        - | 15184 | `			}` |
|    19146 | 15185 | `			*pNew = 1;` |
|        - | 15186 | `		}` |
|     9577 | 15187 | `	}` |
|    22296 | 15188 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22296 | 15189 | `	return SXRET_OK;` |
|    11149 | 15190 |  |
|        - | 15191 | `/*` |
|        - | 15192 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 15193 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 15194 | ` * indicates failure.` |
|        - | 15195 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 15196 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 15197 | ` * operations.` |
|        - | 15198 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 15199 | ` * this function is a no-op.` |
|        - | 15200 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 15201 | ` * constructs for more information.` |
|        - | 15202 | ` */` |
|     9592 | 15203 | `static sxi32 VmExecIncludedFile(` |
|        - | 15204 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 15205 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 15206 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 15207 | `	 )` |
|        2 | 15208 |  |
|        - | 15209 | `	sxi32 rc;` |
|        - | 15210 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15211 | `	const ph7_io_stream *pStream;` |
|        - | 15212 | `	SyBlob sContents;` |
|        - | 15213 | `	void *pHandle;` |
|        - | 15214 | `	ph7_vm *pVm;` |
|        - | 15215 | `	int isNew;` |
|        - | 15216 | `	/* Initialize fields */` |
|     9594 | 15217 | `	pVm = pCtx->pVm;` |
|     9594 | 15218 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9594 | 15219 | `	isNew = 0;` |
|        - | 15220 | `	/* Extract the associated stream */` |
|     9594 | 15221 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 15222 | `	/*` |
|        - | 15223 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 15224 | `	 * in a read-only mode.` |
|        - | 15225 | `	 */` |
|     9594 | 15226 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9594 | 15227 | `	if( pHandle == 0 ){` |
|        8 | 15228 | `		return SXERR_IO;` |
|        - | 15229 | `	}` |
|     9588 | 15230 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9588 | 15231 | `	if( IncludeOnce && !isNew ){` |
|        - | 15232 | `		/* Already included */` |
|        9 | 15233 | `		rc = SXERR_EXISTS;` |
|        5 | 15234 | `	}else{` |
|        - | 15235 | `		/* Read the whole file contents */` |
|     9580 | 15236 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9580 | 15237 | `		if( rc == SXRET_OK ){` |
|        - | 15238 | `			SyString sScript;` |
|        - | 15239 | `			/* Compile and execute the script */` |
|     9580 | 15240 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9580 | 15241 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4789 | 15242 | `		}` |
|        - | 15243 | `	}` |
|        - | 15244 | `	/* Pop from the set of included file */` |
|     9588 | 15245 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 15246 | `	/* Close the handle */` |
|     9588 | 15247 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 15248 | `	/* Release the working buffer */` |
|     9588 | 15249 | `	SyBlobRelease(&sContents);` |
|        - | 15250 | `#else` |
|        - | 15251 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 15252 | `	SXUNUSED(pPath);` |
|        - | 15253 | `	SXUNUSED(IncludeOnce);` |
|        - | 15254 | `	rc = SXERR_IO;` |
|        - | 15255 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9588 | 15256 | `	return rc;` |
|     4798 | 15257 |  |
|        - | 15258 | `/*` |
|        - | 15259 | ` * string get_include_path(void)` |
|        - | 15260 | ` *  Gets the current include_path configuration option.` |
|        - | 15261 | ` * Parameter` |
|        - | 15262 | ` *  None` |
|        - | 15263 | ` * Return` |
|        - | 15264 | ` *  Included paths as a string` |
|        - | 15265 | ` */` |
|        2 | 15266 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15267 |  |
|        3 | 15268 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15269 | `	SyString *aEntry;` |
|        - | 15270 | `	int dir_sep;` |
|        - | 15271 | `	sxu32 n;` |
|        - | 15272 | `#ifdef __WINNT__` |
|        1 | 15273 | `	dir_sep = ';';` |
|        - | 15274 | `#else` |
|        - | 15275 | `	/* Assume UNIX path separator */` |
|        2 | 15276 | `	dir_sep = ':';` |
|        - | 15277 | `#endif` |
|        1 | 15278 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 15279 | `	SXUNUSED(apArg);` |
|        - | 15280 | `	/* Point to the list of import paths */` |
|        3 | 15281 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 15282 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 15283 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 15284 | `		if( n > 0 ){` |
|        - | 15285 | `			/* Append dir seprator */` |
|      ! 0 | 15286 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 15287 | `		}` |
|        - | 15288 | `		/* Append path */` |
|        3 | 15289 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 15290 | `	}` |
|        3 | 15291 | `	return PH7_OK;` |
|        1 | 15292 |  |
|        - | 15293 | `/*` |
|        - | 15294 | ` * string get_get_included_files(void)` |
|        - | 15295 | ` *  Gets the current include_path configuration option.` |
|        - | 15296 | ` * Parameter` |
|        - | 15297 | ` *  None` |
|        - | 15298 | ` * Return` |
|        - | 15299 | ` *  Included paths as a string` |
|        - | 15300 | ` */` |
|        2 | 15301 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15302 |  |
|        3 | 15303 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 15304 | `	ph7_value *pArray,*pWorker;` |
|        - | 15305 | `	SyString *pEntry;` |
|        - | 15306 | `	int c,d;` |
|        - | 15307 | `	/* Create an array and a working value */` |
|        3 | 15308 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 15309 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 15310 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 15311 | `		/* Out of memory,return null */` |
|      ! 0 | 15312 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15313 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 15314 | `		SXUNUSED(apArg);` |
|      ! 0 | 15315 | `		return PH7_OK;` |
|        - | 15316 | `	}` |
|        3 | 15317 | `	c = d = '/';` |
|        - | 15318 | `#ifdef __WINNT__` |
|        1 | 15319 | `	d = '\\';` |
|        - | 15320 | `#endif` |
|        - | 15321 | `	/* Iterate throw entries */` |
|        3 | 15322 | `	SySetResetCursor(pFiles);` |
|     3917 | 15323 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 15324 | `		const char *zBase,*zEnd;` |
|        - | 15325 | `		int iLen;` |
|        - | 15326 | `		/* reset the string cursor */` |
|     3915 | 15327 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 15328 | `		/* Extract base name */` |
|     3915 | 15329 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 15330 | `		/* Ignore trailing '/' */` |
|     5872 | 15331 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 15332 | `			zEnd--;` |
|      ! 0 | 15333 | `		}` |
|     3915 | 15334 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 15335 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 15336 | `			zEnd--;` |
|        1 | 15337 | `		}` |
|     3915 | 15338 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 15339 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 15340 | `		/* Copy entry name */` |
|     3915 | 15341 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 15342 | `		/* Perform the insertion */` |
|     3915 | 15343 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 15344 | `	}` |
|        - | 15345 | `	/* All done,return the created array */` |
|        3 | 15346 | `	ph7_result_value(pCtx,pArray);` |
|        - | 15347 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 15348 | `	 * by the engine as soon we return from this foreign` |
|        - | 15349 | `	 * function.` |
|        - | 15350 | `	 */` |
|        3 | 15351 | `	return PH7_OK;` |
|        2 | 15352 |  |
|        - | 15353 | `/*` |
|        - | 15354 | ` * include:` |
|        - | 15355 | ` * According to the PHP reference manual.` |
|        - | 15356 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 15357 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 15358 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 15359 | ` *  include() will finally check in the calling script's own directory` |
|        - | 15360 | ` *  and the current working directory before failing. The include()` |
|        - | 15361 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 15362 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 15363 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 15364 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 15365 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 15366 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 15367 | ` *  directory to find the requested file.` |
|        - | 15368 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 15369 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 15370 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 15371 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 15372 | ` */` |
|     9568 | 15373 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15374 |  |
|        - | 15375 | `	SyString sFile;` |
|        - | 15376 | `	sxi32 rc;` |
|     9570 | 15377 | `	if( nArg < 1 ){` |
|        - | 15378 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15379 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15380 | `		return SXRET_OK;` |
|        - | 15381 | `	}` |
|        - | 15382 | `	/* File to include */` |
|     9570 | 15383 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9570 | 15384 | `	if( sFile.nByte < 1 ){` |
|        - | 15385 | `		/* Empty string,return NULL */` |
|      ! 0 | 15386 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15387 | `		return SXRET_OK;` |
|        - | 15388 | `	}` |
|        - | 15389 | `	/* Open,compile and execute the desired script */` |
|     9570 | 15390 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9570 | 15391 | `	if( rc != SXRET_OK ){` |
|        - | 15392 | `		/* Emit a warning and return false */` |
|        3 | 15393 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 15394 | `		ph7_result_bool(pCtx,0);` |
|        1 | 15395 | `	}` |
|     9570 | 15396 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15397 | `		/* exit/die inside the included file: cascade the halt */` |
|        5 | 15398 | `		return PH7_ABORT;` |
|        - | 15399 | `	}` |
|     9566 | 15400 | `	return SXRET_OK;` |
|     4786 | 15401 |  |
|        - | 15402 | `/*` |
|        - | 15403 | ` * include_once:` |
|        - | 15404 | ` *  According to the PHP reference manual.` |
|        - | 15405 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 15406 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 15407 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 15408 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 15409 | ` *   just once.` |
|        - | 15410 | ` */` |
|       10 | 15411 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15412 |  |
|        - | 15413 | `	SyString sFile;` |
|        - | 15414 | `	sxi32 rc;` |
|       11 | 15415 | `	if( nArg < 1 ){` |
|        - | 15416 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15417 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15418 | `		return SXRET_OK;` |
|        - | 15419 | `	}` |
|        - | 15420 | `	/* File to include */` |
|       11 | 15421 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       11 | 15422 | `	if( sFile.nByte < 1 ){` |
|        - | 15423 | `		/* Empty string,return NULL */` |
|      ! 0 | 15424 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15425 | `		return SXRET_OK;` |
|        - | 15426 | `	}` |
|        - | 15427 | `	/* Open,compile and execute the desired script */` |
|       11 | 15428 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       11 | 15429 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15430 | `		/* File already included,return TRUE */` |
|        7 | 15431 | `		ph7_result_bool(pCtx,1);` |
|        7 | 15432 | `		return SXRET_OK;` |
|        - | 15433 | `	}` |
|        5 | 15434 | `	if( rc != SXRET_OK ){` |
|        - | 15435 | `		/* Emit a warning and return false */` |
|      ! 0 | 15436 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15437 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15438 | ` 	}` |
|        5 | 15439 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15440 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15441 | `		return PH7_ABORT;` |
|        - | 15442 | `	}` |
|        5 | 15443 | `	return SXRET_OK;` |
|        6 | 15444 |  |
|        - | 15445 | `/*` |
|        - | 15446 | ` * require.` |
|        - | 15447 | ` *  According to the PHP reference manual.` |
|        - | 15448 | ` *   require() is identical to include() except upon failure it will` |
|        - | 15449 | ` *   also produce a fatal level error.` |
|        - | 15450 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 15451 | ` *   emits a warning  which allows the script to continue.` |
|        - | 15452 | ` */` |
|        6 | 15453 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15454 |  |
|        - | 15455 | `	SyString sFile;` |
|        - | 15456 | `	sxi32 rc;` |
|        8 | 15457 | `	if( nArg < 1 ){` |
|        - | 15458 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15459 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15460 | `		return SXRET_OK;` |
|        - | 15461 | `	}` |
|        - | 15462 | `	/* File to include */` |
|        8 | 15463 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 15464 | `	if( sFile.nByte < 1 ){` |
|        - | 15465 | `		/* Empty string,return NULL */` |
|      ! 0 | 15466 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15467 | `		return SXRET_OK;` |
|        - | 15468 | `	}` |
|        - | 15469 | `	/* Open,compile and execute the desired script */` |
|        8 | 15470 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 15471 | `	if( rc != SXRET_OK ){` |
|        - | 15472 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15473 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15474 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15475 | `		return PH7_ABORT;` |
|        - | 15476 | `	}` |
|        8 | 15477 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15478 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15479 | `		return PH7_ABORT;` |
|        - | 15480 | `	}` |
|        8 | 15481 | `	return SXRET_OK;` |
|        5 | 15482 |  |
|        - | 15483 | `/*` |
|        - | 15484 | ` * require_once:` |
|        - | 15485 | ` *  According to the PHP reference manual.` |
|        - | 15486 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 15487 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 15488 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 15489 | ` *   and how it differs from its non _once siblings.` |
|        - | 15490 | ` */` |
|        4 | 15491 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15492 |  |
|        - | 15493 | `	SyString sFile;` |
|        - | 15494 | `	sxi32 rc;` |
|        5 | 15495 | `	if( nArg < 1 ){` |
|        - | 15496 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15497 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15498 | `		return SXRET_OK;` |
|        - | 15499 | `	}` |
|        - | 15500 | `	/* File to include */` |
|        5 | 15501 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 15502 | `	if( sFile.nByte < 1 ){` |
|        - | 15503 | `		/* Empty string,return NULL */` |
|      ! 0 | 15504 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15505 | `		return SXRET_OK;` |
|        - | 15506 | `	}` |
|        - | 15507 | `	/* Open,compile and execute the desired script */` |
|        5 | 15508 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 15509 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15510 | `		/* File already included,return TRUE */` |
|        3 | 15511 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15512 | `		return SXRET_OK;` |
|        - | 15513 | `	}` |
|        3 | 15514 | `	if( rc != SXRET_OK ){` |
|        - | 15515 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15516 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15517 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15518 | `		return PH7_ABORT;` |
|        - | 15519 | `	}` |
|        3 | 15520 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15521 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15522 | `		return PH7_ABORT;` |
|        - | 15523 | `	}` |
|        3 | 15524 | `	return SXRET_OK;` |
|        3 | 15525 |  |
|        - | 15526 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 15527 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 15528 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 15529 | `/*` |
|        - | 15530 | ` * Section:` |
|        - | 15531 | ` *  SPL Autoloading functions.` |
|        - | 15532 | ` * Status:` |
|        - | 15533 | ` *  Stable.` |
|        - | 15534 | ` */` |
|        - | 15535 | `/*` |
|        - | 15536 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 15537 | ` *  Register given function as __autoload() implementation.` |
|        - | 15538 | ` * Parameters` |
|        - | 15539 | ` *  callback` |
|        - | 15540 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 15541 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 15542 | ` *  throw` |
|        - | 15543 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 15544 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 15545 | ` *  prepend` |
|        - | 15546 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 15547 | ` *   autoload stack instead of appending it.` |
|        - | 15548 | ` * Return` |
|        - | 15549 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15550 | ` */` |
|       34 | 15551 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15552 |  |
|        - | 15553 | `	VmAutoloadCB sEntry;` |
|       36 | 15554 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 15555 | `	int iPrepend = 0;` |
|        - | 15556 | `	sxu32 n;` |
|       36 | 15557 | `	if( nArg < 1 ){` |
|        - | 15558 | `		/* No callback provided — register default spl_autoload.` |
|        - | 15559 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 15560 | `		/* Check for duplicates first */` |
|        9 | 15561 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 15562 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 15563 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 15564 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 15565 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 15566 | `				ph7_result_bool(pCtx,1);` |
|        5 | 15567 | `				return SXRET_OK;` |
|        - | 15568 | `			}` |
|      ! 0 | 15569 | `		}` |
|        5 | 15570 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 15571 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 15572 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 15573 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 15574 | `		ph7_result_bool(pCtx,1);` |
|        5 | 15575 | `		return SXRET_OK;` |
|        - | 15576 | `	}` |
|        - | 15577 | `	/* Validate that the callback is callable */` |
|       28 | 15578 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 15579 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 15580 | `		if( nArg >= 2 ){` |
|      ! 0 | 15581 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 15582 | `		}` |
|      ! 0 | 15583 | `		if( iThrow ){` |
|      ! 0 | 15584 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 15585 | `				"Argument is not callable");` |
|      ! 0 | 15586 | `		}` |
|      ! 0 | 15587 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15588 | `		return SXRET_OK;` |
|        - | 15589 | `	}` |
|        - | 15590 | `	/* Check for duplicates */` |
|       46 | 15591 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 15592 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 15593 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15594 | `			/* Already registered */` |
|      ! 0 | 15595 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 15596 | `			return SXRET_OK;` |
|        - | 15597 | `		}` |
|       11 | 15598 | `	}` |
|        - | 15599 | `	/* Check prepend flag */` |
|       28 | 15600 | `	if( nArg >= 3 ){` |
|        3 | 15601 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 15602 | `	}` |
|        - | 15603 | `	/* Store the callback */` |
|       28 | 15604 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 15605 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 15606 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 15607 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 15608 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 15609 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 15610 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 15611 | `		VmAutoloadCB *aBase;` |
|        3 | 15612 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15613 | `		/* Rotate: move last entry to front */` |
|        3 | 15614 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 15615 | `		if( aBase ){` |
|        - | 15616 | `			VmAutoloadCB sTemp;` |
|        - | 15617 | `			sxu32 i;` |
|        3 | 15618 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 15619 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 15620 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 15621 | `			}` |
|        3 | 15622 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 15623 | `		}` |
|        2 | 15624 | `	}else{` |
|       26 | 15625 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15626 | `	}` |
|       28 | 15627 | `	ph7_result_bool(pCtx,1);` |
|       28 | 15628 | `	return SXRET_OK;` |
|       19 | 15629 |  |
|        - | 15630 | `/*` |
|        - | 15631 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 15632 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 15633 | ` * Parameters` |
|        - | 15634 | ` *  callback` |
|        - | 15635 | ` *   The autoload function being unregistered.` |
|        - | 15636 | ` * Return` |
|        - | 15637 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15638 | ` */` |
|       32 | 15639 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15640 |  |
|       34 | 15641 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15642 | `	sxu32 n,nEntry;` |
|       34 | 15643 | `	if( nArg < 1 ){` |
|      ! 0 | 15644 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15645 | `		return SXRET_OK;` |
|        - | 15646 | `	}` |
|       34 | 15647 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 15648 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 15649 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 15650 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15651 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 15652 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 15653 | `			sxu32 i;` |
|       32 | 15654 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 15655 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 15656 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 15657 | `			}` |
|        - | 15658 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 15659 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 15660 | `			ph7_result_bool(pCtx,1);` |
|       32 | 15661 | `			return SXRET_OK;` |
|        - | 15662 | `		}` |
|        3 | 15663 | `	}` |
|        3 | 15664 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15665 | `	return SXRET_OK;` |
|       18 | 15666 |  |
|        - | 15667 | `/*` |
|        - | 15668 | ` * array spl_autoload_functions(void)` |
|        - | 15669 | ` *  Return all registered __autoload() functions.` |
|        - | 15670 | ` * Return` |
|        - | 15671 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 15672 | ` *  an empty array is returned.` |
|        - | 15673 | ` */` |
|       20 | 15674 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15675 |  |
|       21 | 15676 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15677 | `	ph7_value *pArray;` |
|        - | 15678 | `	sxu32 n,nEntry;` |
|       10 | 15679 | `	SXUNUSED(nArg);` |
|       10 | 15680 | `	SXUNUSED(apArg);` |
|       21 | 15681 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 15682 | `	if( pArray == 0 ){` |
|      ! 0 | 15683 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15684 | `		return SXRET_OK;` |
|        - | 15685 | `	}` |
|       21 | 15686 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 15687 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 15688 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 15689 | `		if( pEntry ){` |
|       15 | 15690 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 15691 | `		}` |
|        8 | 15692 | `	}` |
|       21 | 15693 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 15694 | `	return SXRET_OK;` |
|       11 | 15695 |  |
|        - | 15696 | `/*` |
|        - | 15697 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 15698 | ` *  Default implementation of __autoload().` |
|        - | 15699 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 15700 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 15701 | ` * Parameters` |
|        - | 15702 | ` *  class` |
|        - | 15703 | ` *   The class name being searched.` |
|        - | 15704 | ` *  file_extensions` |
|        - | 15705 | ` *   Comma-separated list of file extensions to try.` |
|        - | 15706 | ` */` |
|        2 | 15707 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15708 |  |
|        - | 15709 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 15710 | `	SyBlob sPath;` |
|        - | 15711 | `	int nClass;` |
|        - | 15712 | `	sxi32 rc;` |
|        3 | 15713 | `	if( nArg < 1 ){` |
|      ! 0 | 15714 | `		return SXRET_OK;` |
|        - | 15715 | `	}` |
|        3 | 15716 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 15717 | `	if( nClass < 1 ){` |
|      ! 0 | 15718 | `		return SXRET_OK;` |
|        - | 15719 | `	}` |
|        - | 15720 | `	/* Default extensions */` |
|        3 | 15721 | `	zExt = ".php,.inc";` |
|        3 | 15722 | `	if( nArg >= 2 ){` |
|        - | 15723 | `		int nExt;` |
|      ! 0 | 15724 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 15725 | `		if( nExt < 1 ){` |
|      ! 0 | 15726 | `			zExt = ".php,.inc";` |
|      ! 0 | 15727 | `		}` |
|      ! 0 | 15728 | `	}` |
|        3 | 15729 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 15730 | `	/* Iterate over comma-separated extensions */` |
|        3 | 15731 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 15732 | `	zCur = zExt;` |
|        7 | 15733 | `	while( zCur < zEnd ){` |
|        - | 15734 | `		const char *zComma;` |
|        - | 15735 | `		SyString sFile;` |
|        - | 15736 | `		int i;` |
|        - | 15737 | `		/* Find next comma or end */` |
|        5 | 15738 | `		zComma = zCur;` |
|       21 | 15739 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 15740 | `			zComma++;` |
|        1 | 15741 | `		}` |
|        - | 15742 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 15743 | `		SyBlobReset(&sPath);` |
|       69 | 15744 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 15745 | `			char c = zClass[i];` |
|       65 | 15746 | `			if( c == '\\' ){` |
|      ! 0 | 15747 | `				c = '/';` |
|       65 | 15748 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 15749 | `				c = c + ('a' - 'A');` |
|        6 | 15750 | `			}` |
|       65 | 15751 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 15752 | `		}` |
|        - | 15753 | `		/* Append extension */` |
|        5 | 15754 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 15755 | `		/* Try to include the file */` |
|        5 | 15756 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 15757 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 15758 | `		if( rc == SXRET_OK ){` |
|        - | 15759 | `			/* File included successfully */` |
|      ! 0 | 15760 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 15761 | `			return SXRET_OK;` |
|        - | 15762 | `		}` |
|        - | 15763 | `		/* Move past the comma */` |
|        5 | 15764 | `		zCur = zComma;` |
|        5 | 15765 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 15766 | `			zCur++;` |
|        1 | 15767 | `		}` |
|        1 | 15768 | `	}` |
|        3 | 15769 | `	SyBlobRelease(&sPath);` |
|        3 | 15770 | `	return SXRET_OK;` |
|        2 | 15771 |  |
|        - | 15772 | `/* Table of built-in VM functions. */` |
|        - | 15773 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 15774 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 15775 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 15776 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 15777 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 15778 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 15779 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 15780 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 15781 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 15782 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 15783 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 15784 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 15785 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 15786 | `	    /* Constants management */` |
|        - | 15787 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 15788 | `	{ "define",   vm_builtin_define               },` |
|        - | 15789 | `	{ "constant", vm_builtin_constant             },` |
|        - | 15790 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 15791 | `	   /* Class/Object functions */` |
|        - | 15792 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 15793 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 15794 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 15795 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 15796 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 15797 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 15798 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 15799 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 15800 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 15801 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 15802 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 15803 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 15804 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 15805 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 15806 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 15807 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 15808 | `	   /* SPL Autoloading */` |
|        - | 15809 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 15810 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 15811 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 15812 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 15813 | `	   /* Random numbers/strings generators */` |
|        - | 15814 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 15815 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 15816 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 15817 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 15818 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 15819 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 15820 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 15821 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15822 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 15823 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 15824 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 15825 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15826 | `	   /* Language constructs functions */` |
|        - | 15827 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 15828 | `	{ "print", vm_builtin_print                   },` |
|        - | 15829 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 15830 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 15831 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 15832 | `	  /* Variable handling functions */` |
|        - | 15833 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 15834 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 15835 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 15836 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 15837 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 15838 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 15839 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 15840 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 15841 | `	  /* Ouput control functions */` |
|        - | 15842 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 15843 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 15844 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 15845 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 15846 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 15847 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 15848 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 15849 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 15850 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 15851 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 15852 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 15853 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 15854 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 15855 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 15856 | `	  /* Assertion functions */` |
|        - | 15857 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 15858 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 15859 | `	  /* Error reporting functions */` |
|        - | 15860 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 15861 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 15862 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 15863 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 15864 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 15865 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 15866 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 15867 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 15868 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 15869 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 15870 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 15871 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 15872 | `	  /* Release info */` |
|        - | 15873 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 15874 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 15875 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 15876 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 15877 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 15878 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 15879 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 15880 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 15881 | `	  /* hashmap */` |
|        - | 15882 | `	{"compact",          vm_builtin_compact       },` |
|        - | 15883 | `	{"extract",          vm_builtin_extract       },` |
|        - | 15884 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 15885 | `	  /* URL related function */` |
|        - | 15886 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 15887 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 15888 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15889 | `	   /* XML processing functions */` |
|        - | 15890 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 15891 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 15892 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 15893 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 15894 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 15895 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 15896 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 15897 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 15898 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 15899 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 15900 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 15901 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 15902 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 15903 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 15904 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 15905 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 15906 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 15907 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 15908 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 15909 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 15910 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 15911 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15912 | `	   /* UTF-8 encoding/decoding */` |
|        - | 15913 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 15914 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 15915 | `	   /* Command line processing */` |
|        - | 15916 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 15917 | `	   /* JSON encoding/decoding */` |
|        - | 15918 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 15919 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 15920 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 15921 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 15922 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 15923 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 15924 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 15925 | `	   /* Files/URI inclusion facility */` |
|        - | 15926 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 15927 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 15928 | `	{ "include",      vm_builtin_include          },` |
|        - | 15929 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 15930 | `	{ "require",      vm_builtin_require          },` |
|        - | 15931 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 15932 | `};` |
|        - | 15933 | `/*` |
|        - | 15934 | ` * Register the built-in VM functions defined above.` |
|        - | 15935 | ` */` |
|     2834 | 15936 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 15937 |  |
|        - | 15938 | `	sxi32 rc;` |
|        - | 15939 | `	sxu32 n;` |
|   382592 | 15940 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 15941 | `		/* Note that these special functions have access` |
|        - | 15942 | `		 * to the underlying virtual machine as their` |
|        - | 15943 | `		 * private data.` |
|        - | 15944 | `		 */` |
|   379758 | 15945 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   379758 | 15946 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 15947 | `			return rc;` |
|        - | 15948 | `		}` |
|   189880 | 15949 | `	}` |
|     2836 | 15950 | `	return SXRET_OK;` |
|     1419 | 15951 |  |
|        - | 15952 | `/*` |
|        - | 15953 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 15954 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 15955 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 15956 | ` */` |
|   182838 | 15957 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 15958 |  |
|   182840 | 15959 | `	if( !iLoadable ){` |
|   180752 | 15960 | `		return pClass;` |
|        - | 15961 | `	}` |
|     2094 | 15962 | `	while(pClass){` |
|     2090 | 15963 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2086 | 15964 | `			return pClass;` |
|        - | 15965 | `		}` |
|        5 | 15966 | `		pClass = pClass->pNextName;` |
|        1 | 15967 | `	}` |
|        5 | 15968 | `	return 0;` |
|    91421 | 15969 |  |
|        - | 15970 | `/*` |
|        - | 15971 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 15972 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 15973 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 15974 | ` * registered in the VM's class table.` |
|        - | 15975 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 15976 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 15977 | ` */` |
|       38 | 15978 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15979 |  |
|        - | 15980 | `	VmAutoloadCB *pEntry;` |
|        - | 15981 | `	ph7_value sArg,sResult;` |
|        - | 15982 | `	SyHashEntry *pHashEntry;` |
|        - | 15983 | `	ph7_class *pClass;` |
|        - | 15984 | `	sxu32 n,nEntry;` |
|       40 | 15985 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 15986 | `	if( nEntry < 1 ){` |
|       26 | 15987 | `		return 0;` |
|        - | 15988 | `	}` |
|        - | 15989 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 15990 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 15991 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 15992 | `	}` |
|        - | 15993 | `	/* Mark this class as being autoloaded */` |
|       14 | 15994 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 15995 | `	/* Prepare the class name argument */` |
|       14 | 15996 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 15997 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 15998 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 15999 | `	pClass = 0;` |
|       28 | 16000 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 16001 | `		ph7_value *apArg[1];` |
|       24 | 16002 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 16003 | `		if( pEntry == 0 ){` |
|      ! 0 | 16004 | `			continue;` |
|        - | 16005 | `		}` |
|       24 | 16006 | `		apArg[0] = &sArg;` |
|       24 | 16007 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 16008 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 16009 | `			continue;` |
|        - | 16010 | `		}` |
|        - | 16011 | `		/* Check if the class is now available */` |
|       24 | 16012 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 16013 | `		if( pHashEntry ){` |
|       10 | 16014 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 16015 | `			if( pClass ){` |
|       10 | 16016 | `				break;` |
|        - | 16017 | `			}` |
|      ! 0 | 16018 | `		}` |
|        9 | 16019 | `	}` |
|       14 | 16020 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 16021 | `	PH7_MemObjRelease(&sResult);` |
|        - | 16022 | `	/* Remove reentrancy guard */` |
|       14 | 16023 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 16024 | `	return pClass;` |
|       21 | 16025 |  |
|        - | 16026 | `/*` |
|        - | 16027 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 16028 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 16029 | ` */` |
|       18 | 16030 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 16031 |  |
|       20 | 16032 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 16033 |  |
|        - | 16034 | `/*` |
|        - | 16035 | ` * Check if the given name refer to an installed class.` |
|        - | 16036 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 16037 | ` */` |
|   182850 | 16038 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 16039 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 16040 | `	const char *zName,  /* Name of the target class */` |
|        - | 16041 | `	sxu32 nByte,        /* zName length */` |
|        - | 16042 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 16043 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 16044 | `						 */` |
|        - | 16045 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 16046 | `	)` |
|        2 | 16047 |  |
|        - | 16048 | `	SyHashEntry *pEntry;` |
|        - | 16049 | `	ph7_class *pClass;` |
|    91425 | 16050 | `	SXUNUSED(iNest);` |
|        - | 16051 | `	/* Exact class lookup.` |
|        - | 16052 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 16053 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   182852 | 16054 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   182852 | 16055 | `	if( pEntry == 0 ){` |
|        - | 16056 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 16057 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 16058 | `	}` |
|   182832 | 16059 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   182832 | 16060 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    91427 | 16061 |  |
|        - | 16062 | `/*` |
|        - | 16063 | ` * Reference Table Implementation` |
|        - | 16064 | ` * Status: stable <chm@symisc.net>` |
|        - | 16065 | ` * Intro` |
|        - | 16066 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 16067 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 16068 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 16069 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 16070 | ` *  Refer to the official for more information on this powerful` |
|        - | 16071 | ` *  extension.` |
|        - | 16072 | ` */` |
|        - | 16073 | `/*` |
|        - | 16074 | ` * Allocate a new reference entry.` |
|        - | 16075 | ` */` |
|  3204490 | 16076 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 16077 |  |
|        - | 16078 | `	VmRefObj *pRef;` |
|        - | 16079 | `	/* Allocate a new instance */` |
|  3204492 | 16080 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3204492 | 16081 | `	if( pRef == 0 ){` |
|      ! 0 | 16082 | `		return 0;` |
|        - | 16083 | `	}` |
|        - | 16084 | `	/* Zero the structure */` |
|  3204492 | 16085 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 16086 | `	/* Initialize fields */` |
|  3204492 | 16087 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3204492 | 16088 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3204492 | 16089 | `	pRef->nIdx = nIdx;` |
|  3204492 | 16090 | `	return pRef;` |
|  1602247 | 16091 |  |
|        - | 16092 | `/*` |
|        - | 16093 | ` * Default hash function used by the reference table` |
|        - | 16094 | ` * for lookup/insertion operations.` |
|        - | 16095 | ` */` |
| 17549261 | 16096 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 16097 |  |
|        - | 16098 | `	/* Calculate the hash based on the memory object index */` |
| 17549263 | 16099 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 16100 |  |
|        - | 16101 | `/*` |
|        - | 16102 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 16103 | ` * in the reference table.` |
|        - | 16104 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 16105 | ` * otherwise.` |
|        - | 16106 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16107 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16108 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16109 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16110 | ` * Refer to the official for more information on this powerful` |
|        - | 16111 | ` * extension.` |
|        - | 16112 | ` */` |
|  9553398 | 16113 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 16114 |  |
|        - | 16115 | `	VmRefObj *pRef;` |
|        - | 16116 | `	sxu32 nBucket;` |
|        - | 16117 | `	/* Point to the appropriate bucket */` |
|  9553400 | 16118 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 16119 | `	/* Perform the lookup */` |
|  9553400 | 16120 | `	pRef = pVm->apRefObj[nBucket];` |
| 21002506 | 16121 | `	for(;;){` |
| 41997668 | 16122 | `		if( pRef == 0 ){` |
|  3310016 | 16123 | `			break;` |
|        - | 16124 | `		}` |
| 38687654 | 16125 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 16126 | `			/* Entry found */` |
|  6243386 | 16127 | `			return pRef;` |
|        - | 16128 | `		}` |
|        - | 16129 | `		/* Point to the next entry */` |
| 32444270 | 16130 | `		pRef = pRef->pNextCollide;` |
|        2 | 16131 | `	}` |
|        - | 16132 | `	/* No such entry,return NULL */` |
|  3310016 | 16133 | `	return 0;` |
|  4776701 | 16134 |  |
|        - | 16135 | `/*` |
|        - | 16136 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16137 | ` *` |
|        - | 16138 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16139 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16140 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16141 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16142 | ` * Refer to the official for more information on this powerful` |
|        - | 16143 | ` * extension.` |
|        - | 16144 | ` */` |
|  3204490 | 16145 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16146 |  |
|        - | 16147 | `	sxu32 nBucket;` |
|  3204492 | 16148 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 16149 | `		VmRefObj **apNew;` |
|        - | 16150 | `		sxu32 nNew;` |
|        - | 16151 | `		/* Allocate a larger table */` |
|     4492 | 16152 | `		nNew = pVm->nRefSize << 1;` |
|     4492 | 16153 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4492 | 16154 | `		if( apNew ){` |
|     4492 | 16155 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 16156 | `			sxu32 n;` |
|        - | 16157 | `			/* Zero the structure */` |
|     4492 | 16158 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 16159 | `			/* Rehash all referenced entries */` |
|  2848166 | 16160 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 16161 | `				/* Remove old collision links */` |
|  2843676 | 16162 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 16163 | `				/* Point to the appropriate bucket */` |
|  2843676 | 16164 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 16165 | `				/* Insert the entry  */` |
|  2843676 | 16166 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843676 | 16167 | `				if( apNew[nBucket] ){` |
|  2301116 | 16168 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 16169 | `				}` |
|  2843676 | 16170 | `				apNew[nBucket] = pEntry;` |
|        - | 16171 | `				/* Point to the next entry */` |
|  2843676 | 16172 | `				pEntry = pEntry->pNext;` |
|  1421839 | 16173 | `			}` |
|        - | 16174 | `			/* Release the old table */` |
|     4492 | 16175 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 16176 | `			/* Install the new one */` |
|     4492 | 16177 | `			pVm->apRefObj = apNew;` |
|     4492 | 16178 | `			pVm->nRefSize = nNew;` |
|     2245 | 16179 | `		}` |
|     2245 | 16180 | `	}` |
|        - | 16181 | `	/* Point to the appropriate bucket */` |
|  3204492 | 16182 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 16183 | `	/* Insert the entry */` |
|  3204492 | 16184 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3204492 | 16185 | `	if( pVm->apRefObj[nBucket] ){` |
|  2615633 | 16186 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1307824 | 16187 | `	}` |
|  3204492 | 16188 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3204492 | 16189 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3204492 | 16190 | `	pVm->nRefUsed++;` |
|  3204492 | 16191 | `	return SXRET_OK;` |
|        2 | 16192 |  |
|        - | 16193 | `/*` |
|        - | 16194 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 16195 | ` * the reference table.` |
|        - | 16196 | ` * This function is invoked when the user perform an unset` |
|        - | 16197 | ` * call [i.e: unset($var); ].` |
|        - | 16198 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16199 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16200 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16201 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16202 | ` * Refer to the official for more information on this powerful` |
|        - | 16203 | ` * extension.` |
|        - | 16204 | ` */` |
|  3163138 | 16205 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16206 |  |
|        - | 16207 | `	ph7_hashmap_node **apNode;` |
|        - | 16208 | `	SyHashEntry **apEntry;` |
|        - | 16209 | `	sxu32 n;` |
|        - | 16210 | `	/* Point to the reference table */` |
|  3163140 | 16211 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3163140 | 16212 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 16213 | `	/* Unlink the entry from the reference table */` |
|  3274654 | 16214 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   111516 | 16215 | `		if( apEntry[n] ){` |
|   111466 | 16216 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    55732 | 16217 | `		}` |
|    55759 | 16218 | `	}` |
|  6214860 | 16219 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3051722 | 16220 | `		if( apNode[n] ){` |
|     7064 | 16221 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3531 | 16222 | `		}` |
|  1525862 | 16223 | `	}` |
|  3163140 | 16224 | `	if( pRef->pPrevCollide ){` |
|  1215441 | 16225 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   607410 | 16226 | `	}else{` |
|  1947701 | 16227 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 16228 | `	}` |
|  3163140 | 16229 | `	if( pRef->pNextCollide ){` |
|  1802656 | 16230 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   901323 | 16231 | `	}` |
|  3163140 | 16232 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 16233 | `	/* Release the node */` |
|  3163140 | 16234 | `	SySetRelease(&pRef->aReference);` |
|  3163140 | 16235 | `	SySetRelease(&pRef->aArrEntries);` |
|  3163140 | 16236 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3163140 | 16237 | `	pVm->nRefUsed--;` |
|  3163140 | 16238 | `	return SXRET_OK;` |
|        2 | 16239 |  |
|        - | 16240 | `/*` |
|        - | 16241 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16242 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16243 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16244 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16245 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16246 | ` * Refer to the official for more information on this powerful` |
|        - | 16247 | ` * extension.` |
|        - | 16248 | ` */` |
|  3240198 | 16249 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 16250 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16251 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16252 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16253 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 16254 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 16255 | `	)` |
|        2 | 16256 |  |
|  3240200 | 16257 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 16258 | `	VmRefObj *pRef;` |
|        - | 16259 | `	/* Check if the referenced object already exists */` |
|  3240200 | 16260 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3240200 | 16261 | `	if( pRef == 0 ){` |
|        - | 16262 | `		/* Create a new entry */` |
|  3204492 | 16263 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3204492 | 16264 | `		if( pRef == 0 ){` |
|      ! 0 | 16265 | `			return SXERR_MEM;` |
|        - | 16266 | `		}` |
|  3204492 | 16267 | `		pRef->iFlags = iFlags;` |
|        - | 16268 | `		/* Install the entry */` |
|  3204492 | 16269 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1602245 | 16270 | `	}` |
|  3240200 | 16271 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3240200 | 16272 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 16273 | `		VmSlot sRef;` |
|        - | 16274 | `		/* Local frame,record referenced entry so that it can` |
|        - | 16275 | `		 * be deleted when we leave this frame.` |
|        - | 16276 | `		 */` |
|   105634 | 16277 | `		sRef.nIdx = nIdx;` |
|   105634 | 16278 | `		sRef.pUserData = pEntry;` |
|   105634 | 16279 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 16280 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 16281 | `		}` |
|    52816 | 16282 | `	}` |
|  3240200 | 16283 | `	if( pEntry ){` |
|        - | 16284 | `		/* Address of the hash-entry */` |
|   141114 | 16285 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    70556 | 16286 | `	}` |
|  3240200 | 16287 | `	if( pMapEntry ){` |
|        - | 16288 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3090670 | 16289 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1545334 | 16290 | `	}` |
|  3240200 | 16291 | `	return SXRET_OK;` |
|  1620101 | 16292 |  |
|        - | 16293 | `/*` |
|        - | 16294 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 16295 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16296 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16297 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16298 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16299 | ` * Refer to the official for more information on this powerful` |
|        - | 16300 | ` * extension.` |
|        - | 16301 | ` */` |
|  3150254 | 16302 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 16303 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16304 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16305 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16306 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 16307 | `	)` |
|        2 | 16308 |  |
|        - | 16309 | `	VmRefObj *pRef;` |
|        - | 16310 | `	sxu32 n;` |
|        - | 16311 | `	/* Check if the referenced object already exists */` |
|  3150256 | 16312 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3150256 | 16313 | `	if( pRef == 0 ){` |
|        - | 16314 | `		/* Not such entry */` |
|   105520 | 16315 | `		return SXERR_NOTFOUND;` |
|        - | 16316 | `	}` |
|        - | 16317 | `	/* Remove the desired entry */` |
|  3044738 | 16318 | `	if( pEntry ){` |
|        - | 16319 | `		SyHashEntry **apEntry;` |
|       74 | 16320 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      264 | 16321 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      192 | 16322 | `			if( apEntry[n] == pEntry ){` |
|        - | 16323 | `				/* Nullify the entry */` |
|       74 | 16324 | `				apEntry[n] = 0;` |
|        - | 16325 | `				/*` |
|        - | 16326 | `				 * NOTE:` |
|        - | 16327 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 16328 | `				 * we avoid wasting spaces.` |
|        - | 16329 | `				 */` |
|       36 | 16330 | `			}` |
|       97 | 16331 | `		}` |
|       36 | 16332 | `	}` |
|  3044738 | 16333 | `	if( pMapEntry ){` |
|        - | 16334 | `		ph7_hashmap_node **apNode;` |
|  3044666 | 16335 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6089424 | 16336 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3044760 | 16337 | `			if( apNode[n] == pMapEntry ){` |
|        - | 16338 | `				/* nullify the entry */` |
|  3044666 | 16339 | `				apNode[n] = 0;` |
|  1522332 | 16340 | `			}` |
|  1522381 | 16341 | `		}` |
|  1522332 | 16342 | `	}` |
|  3044738 | 16343 | `	return SXRET_OK;` |
|  1575129 | 16344 |  |
|        - | 16345 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 16346 | `/*` |
|        - | 16347 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 16348 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 16349 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 16350 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 16351 | ` * For more information on how to register IO stream devices,please` |
|        - | 16352 | ` * refer to the official documentation.` |
|        - | 16353 | ` */` |
|    29152 | 16354 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 16355 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 16356 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 16357 | `	int nByte              /* *pzDevice length*/` |
|        - | 16358 | `	)` |
|        2 | 16359 |  |
|        - | 16360 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 16361 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 16362 | `	SyString sDev,sCur;` |
|        - | 16363 | `	sxu32 n,nEntry;` |
|        - | 16364 | `	int rc;` |
|        - | 16365 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29154 | 16366 | `	zNext = zCur = zIn = *pzDevice;` |
|    29154 | 16367 | `	zEnd = &zIn[nByte];` |
|  1862297 | 16368 | `	while( zIn < zEnd ){` |
|  1833147 | 16369 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 16370 | `			/* Got one */` |
|        3 | 16371 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 16372 | `			break;` |
|        - | 16373 | `		}` |
|        - | 16374 | `		/* Advance the cursor */` |
|  1833145 | 16375 | `		zIn++;` |
|        2 | 16376 | `	}` |
|    29154 | 16377 | `	if( zIn >= zEnd ){` |
|        - | 16378 | `		/* No such scheme,return the default stream */` |
|    29152 | 16379 | `		return pVm->pDefStream;` |
|        - | 16380 | `	}` |
|        3 | 16381 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 16382 | `	/* Remove leading and trailing white spaces */` |
|        3 | 16383 | `	SyStringFullTrim(&sDev);` |
|        - | 16384 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 16385 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 16386 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 16387 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 16388 | `		pStream = apStream[n];` |
|        3 | 16389 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 16390 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 16391 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 16392 | `		if( rc == 0 ){` |
|        - | 16393 | `			/* Stream device found */` |
|        3 | 16394 | `			*pzDevice = zNext;` |
|        3 | 16395 | `			return pStream;` |
|        - | 16396 | `		}` |
|      ! 0 | 16397 | `	}` |
|        - | 16398 | `	/* No such stream,return NULL */` |
|      ! 0 | 16399 | `	return 0;` |
|    14578 | 16400 |  |
|        - | 16401 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 16402 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 16403 |  |
